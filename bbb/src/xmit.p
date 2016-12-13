// PRUSS program to perform PWM on three PRU output pins.
//
// At any given time the PRU will output a PWM signal with its duty cycle
// set by duty/iperiod on one of the configured output pins. The PRU will
// cycle which pin is outputting every speriod.
//
// Outputs: REG_LEDS.0 (P9_29), REG_LEDS.1 (P0_30), REG_LEDS.2 (P0_31)
//

// The beginning of PRU memory is laid out as follows:
.struct pru_xmit_info
  .u32 speriod  // symbol period: time between changing symbols
  .u32 iperiod  // dimming [intra-symbol] period: time used for PWM dimming
  .u32 duty     // duty period: on time, less than or equal to iperiod
  .u16 left     // number of bytes left of the data packet
  .u8  bhalt    // halt bit - set by the host to request termination
  .u8  pad      // unused
.ends
.assign pru_xmit_info, r4, r7, info // map tx info structure starting at r4
#define DATA_START     16 // data starts immediately after info struct

.struct pru_data_info
  .u32 buf     // data word buffer, copied from register file
  .u32 off     // PRU memory offset of next data block transfer
  .u8  shift   // data shift amount, used to read 2 bits at a time to choose LED
  .u8  block   // size of next block transfer, usually DATA_BLK_BYTES
  .u8  bits    // which bits to set in the LED
  .u8  cycles  // number of cycles we've wasted with initializing code
.ends
.assign pru_data_info, r8, r10, data // map data info structure starting at r8

// These are used as pointers into the register file to read the tx data,
// which is burst copied from main PRU memory now and then.
.struct reg_ptrs
  .u8  current
  .u8  end
.ends
.assign reg_ptrs, r1.b2, r1.b3, reg

// data is copied into the register file starting at this address, which
// must be after all the mapped structures
#define DATA_BLK_BYTES   64 // == 16 registers; number of bytes per block read
#define REGFILE_DATA     11*4 // r11 - start of data copied into regfile
#define REGFILE_DATA_END 27*4 // r27 - REGFILE_DATA plus DATA_BLK/4 registers

.origin 0               // offset of start of program in PRU memory
.entrypoint START       // program entry point used by the debugger

#define PRU0_R31_VEC_VALID 32
#define EVENTOUT0 3

#define REG_LEDS    r30
#define LEDR_BIT 0 // 001
#define LEDG_BIT 1 // 010
#define LEDB_BIT 2 // 100
#define LED_BITS 7 // 111 - mask for all LED bits

#define DATA_RED   0 // 00
#define DATA_GREEN 1 // 01
#define DATA_BLUE  2 // 10
#define DATA_WHITE 3 // 11 - mask for data colors

// 200MHz -> 5ns per instruction
#define NS_PER_INSTR 5

// Each PWM loop we subtract the PWM period from the running symbol period
// count. To keep our symbol frequency as accurate as possible, we need to
// adjust for any extra wasted instructions.
#define EXTRA_INSTR_PER_CYCLE 10

START:
        // Disable LEDs to start, since the beginning code is in the
        // PWM OFF cycle
        LDI     REG_LEDS, 0

        // Load the info structure into the register file
        // abuse the fact that we loaded 0 into REG_LEDS
        LBBO    &info, REG_LEDS, 0, SIZE(info)

        // initialize data info
        LDI     data.off, DATA_START
        LDI     data.block, DATA_BLK_BYTES

        // Unconditional instructions from SET_OUTBITS to PWM_DELAY_LOW
        LDI     data.cycles, 11 * NS_PER_INSTR

        // NB. we omit the following block because we know we will be given
        // SPERIOD divisible by IPERIOD, due to checking in the host program.

        // We need to check whether the IPERIOD is divisible by SPERIOD.
        // If not, the duty cycle will be wrong and the xmit frequency will
        // drift.  However skip this step if the duty-cycle or intra-period are
        // zero.
//        QBEQ    CHECK_DIVIS, info.duty, 0
//        QBEQ    CHECK_DIVIS, info.iperiod, 0

//        MAX     r0, info.iperiod, info.speriod
//        MIN     r1, info.iperiod, info.speriod

//CHECK_DIVIS: // while (x > 0) x -= y; if (x != 0) FAIL
//        SUB     r0, r0, r1
//        QBBS    END, r0, 31 // If r0 < 0, we are not divisible so we quit!
//        QBLT    CHECK_DIVIS, r0, 0 // If r0 > 0, keep subtracting

NEXT_DATA:
        // read a block at a time; or whatever is left, if less
        MIN     r0, data.block, info.left // r0 is used for the block size
        LBBO    REGFILE_DATA, data.off, 0, b0 // r0.b0 from above
        ADD     data.off, data.off, r0 // increment data offset

        // start reading registers from the start of data, go to end of what
        // we just read
        LDI     reg.current, REGFILE_DATA
        ADD     reg.end, r0, REGFILE_DATA // set end reg

        ADD     data.cycles, data.cycles, 6 * NS_PER_INSTR

NEXT_REG:
        // r0 = min { 4, left } ; number of bytes remaining, usually 4
        LDI     r0, 4
        MIN     r0, r0, info.left
        // shift = 4 * r0 - 1   ; number of symbols left to send
        LSL     data.shift, r0, 2
        SUB     data.shift, data.shift, 1
        // left -= r0           ; decrement number of bytes used
        SUB     info.left, info.left, r0

        // Load next data word from the register file
        MVID    data.buf, *reg.current
        ADD     reg.current, reg.current, 4 // read 4 bytes at a time

        ADD     data.cycles, data.cycles, 8 * NS_PER_INSTR

SET_OUTBITS:
        // Set the output bits based on the next data word according to
        // our current shift

        // Read 2 bits from the data:
        //   00  RED     (LEDR_BIT==0)  set 001
        //   01  GREEN   (LEDG_BIT==1)  set 010
        //   10  BLUE    (LEDB_BIT==2)  set 100
        //   11  WHITE                  set 111
        LDI     data.bits, 0 // clear LED bits
        AND     r0.b0, data.buf, DATA_WHITE // read bottom 2 bits
        SET     data.bits, r0.b0 // set LED bits from data (see table above)

        QBNE    SKIP_WHITE, r0.b0, DATA_WHITE // r0 != DATA_WHITE, we're done

        // if we actually chose white, we did erroneously set the unused
        // "bit 3" above; however, the unconditional SET saves a branch,
        // and using MOV here fixes it anyway
        MOV     data.bits, LED_BITS
        ADD     data.cycles, data.cycles, 2 * NS_PER_INSTR

SKIP_WHITE:
        // Shift the data over for next time, decrement shift count
        LSR     data.buf, data.buf, 2
        SUB     data.shift, data.shift, 1

        MOV     r3, info.speriod // set up counter for symbol period

SYMBOL_PWM:
        // hold symbol low for (iperiod - duty)
        MOV     r2, info.iperiod // start inner counter
        // We waste a few instructions per PWM cycle with all the
        // initialization stuff above, which we've kept track of to this point.
        // Make sure we account for this extra time to keep our frequency as
        // accurate as possible.
        SUB     r2, r2, data.cycles
        // Once we've entered the loop, we only waste 8 instructions per iperiod
        LDI     data.cycles, 8*NS_PER_INSTR
        LDI     REG_LEDS, 0 // disable LED output pins

PWM_DELAY_LOW:
        SUB     r2, r2, 2*NS_PER_INSTR // 2 instructions per loop
        QBLT    PWM_DELAY_LOW, r2, info.duty // loop while r2 > info.duty

        QBGT    PWM_CYCLE_END, r2, 10 // don't set output high if r2 < 10
        MOV     REG_LEDS, data.bits // set symbol high for remainder (duty)
PWM_DELAY_HIGH:
        SUB     r2, r2, 2*NS_PER_INSTR // 2 instructions per loop
        QBLT    PWM_DELAY_HIGH, r2, 10 // loop while 10 < r2 < info.duty

PWM_CYCLE_END:
        // Finished one PWM cycle. Do it again until time for the next symbol
        SUB     r3, r3, info.iperiod
        QBLT    SYMBOL_PWM, r3, 10 // repeat PWM cycle while r3 > 10

SYMBOL_NEXT:
        // Transmit the next symbol, but start the OFF PWM cycle first.
        LDI     REG_LEDS, 0 // disable LED output pins

        // Unconditional instructions from SET_OUTBITS to PWM_DELAY_LOW
        LDI     data.cycles, 11 * NS_PER_INSTR

        // set bits from shift again until we run out of bits in this reg
        ADD     data.cycles, data.cycles, 2 * NS_PER_INSTR // branch
        QBNE    SET_OUTBITS, data.shift, 0

        // Take this time to check if the halt register is asserted
        // abuse the fact that we've loaded 0 into REG_LEDS
        LBBO    info.bhalt, REG_LEDS, OFFSET(info.bhalt), SIZE(info.bhalt)
        QBNE    END, info.bhalt, 0

        // go to next reg while reg.current < reg.end
        ADD     data.cycles, data.cycles, 4 * NS_PER_INSTR // branch + halt
        QBLT    NEXT_REG, reg.end, reg.current

        ADD     data.cycles, data.cycles, 2 * NS_PER_INSTR // branch
        // keep reading data while bytes left != 0
        QBNE    NEXT_DATA, info.left, 0

END:                               // end of program, send back interrupt
        MOV     R31.b0, PRU0_R31_VEC_VALID | EVENTOUT0
        HALT
