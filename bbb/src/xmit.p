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
  .u16 data_len // number of dwords (4 bytes) in the data packet to xmit
  .u8  halt     // whether to halt, set by the host
  .u8  pad      // unused
.ends
.assign pru_xmit_info, r4, r7, info // map tx info structure starting at r4
#define DATA_START     16 // data starts immediately after info struct

.struct pru_data_info
  .u16 left    // bytes of data left to xmit
  .u16 off     // PRU memory offset of next data block transfer
  .u8  shift   // data shift amount, used to read 2 bits at a time to choose LED
  .u8  block   // size of next block transfer, usually DATA_BLK_BYTES
  .u32 buf     // data word buffer, copied from register file
  .u8  reg     // register pointer to next data word in the register file
  .u8  reg_end // register pointer after the end of data in the register file
  .u8  bits    // which bits to set in the LED
  .u8  cycles  // number of cycles we've wasted with initializing code
.ends
.assign pru_data_info, r8, r10, data // map data info structure starting at r8

// data is copied into the register file starting at this address, which
// must be after all the mapped structures
#define REGFILE_DATA     r12
#define DATA_BLK_BYTES   64  // == 16 registers; number of bytes per block read
#define REGFILE_DATA_END r28 // REGFILE_DATA plus DATA_BLK registers

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
        MOV     data.left, info.data_len
        MVID    data.reg_end, &REGFILE_DATA_END

        // We need to check whether the IPERIOD is divisible by SPERIOD.
        // If not, the duty cycle will be wrong and the xmit frequency will
        // drift.  However skip this step if the duty-cycle or intra-period are
        // zero.
        QBEQ    CHECK_DIVIS, info.duty, 0
        QBEQ    CHECK_DIVIS, info.iperiod, 0

        // check that speriod is divisible by iperiod for ease of xmit
        // this block could be omitted if we are certain the period given
        // is divisible
        MAX     r0, info.iperiod, info.speriod
        MIN     r1, info.iperiod, info.speriod

CHECK_DIVIS: // while (x > 0) x -= y; if (x != 0) FAIL
        SUB     r0, r0, r1
        QBBS    END, r0, 31 // If r0 < 0, we are not divisible so we quit!
        QBLT    CHECK_DIVIS, r0, 0 // If r0 > 0, keep subtracting

MAINLOOP:
        // Quit if the halt register is asserted
        // abuse the fact that we've loaded 0 into REG_LEDS
        LBBO    info.halt, REG_LEDS, OFFSET(info.halt), SIZE(info.halt)
        QBNE    END, info.halt, 0

        // Keep track of the number of instruction cycles we've wasted up
        // to the point of the actual PWM loop. This is so we can perform
        // as accurate a symbol frequency as possible. We waste:
        //    3 instructions here,
        //   10 more from SET_OUTBITS to PWM_DELAY_LOW
        // SET_OUTBITS and 2 more at SYMBOL_NEXT.
        LDI     data.cycles, 14 * NS_PER_INSTR

NEXT_DATA:
        // Get the next block of data from PRU memory into the regsiter file
        QBEQ    END, data.left, 0 // done if we have 0 bytes left

        // Unless we've more registers to read, fall-through to read the
        // next data block from PRU main memory into the register file
        QBLT    NEXT_REG, data.reg_end, data.reg // data.reg < data.reg_end

        // read a block at a time; or whatever is left, if less
        MIN     data.block, data.block, data.left 
        LBBO    &REGFILE_DATA, data.off, 0, data.block
        ADD     data.off, data.off, data.block // increment data offset
        SUB     data.left, data.left, data.block // decrement bytes left
        // start reading registers from the start of data
        MVID    data.reg, &REGFILE_DATA

        ADD     data.cycles, data.cycles, 8 * NS_PER_INSTR

NEXT_REG:
        // Unless we've read all the bits in this register, fall-through to
        // read the next register and reset the shift count
        QBNE    SET_OUTBITS, data.shift, 0 // not done shifting

        // Load next data word from the register file
        MVID    data.buf, *data.reg++

        // we assume 4-CSK, so we read and shift 2 bits at a time
        // until we hit a shift of 32, so we shift 16 times
        LDI     data.shift, 16

        ADD     data.cycles, data.cycles, 4 * NS_PER_INSTR

SET_OUTBITS:
        // Set the output bits based on the next data word according to
        // our current shift

        // Read 2 bits from the data:
        //   00  RED     (LEDR_BIT==0)  set 001
        //   01  GREEN   (LEDG_BIT==1)  set 010
        //   10  BLUE    (LEDB_BIT==2)  set 100
        //   11  WHITE                  set 111
        LDI     data.bits, 0 // clear LED bits
        AND     r0, data.buf, DATA_WHITE // read bottom 2 bits
        SET     data.bits, r0 // set LED bits from data (see table above)
        QBNE    SKIP_WHITE, r0, DATA_WHITE // r0 != DATA_WHITE, we're done

        // if we actually chose white, we did erroneously set the unused
        // "bit 3" above; however, the unconditional SET saves a branch,
        // and using MOV here fixes it anyway
        MOV     data.bits, LED_BITS
        ADD     data.cycles, data.cycles, 2 * NS_PER_INSTR

SKIP_WHITE:
        // Shift the data over for next time, decrement shift count
        LSR     data.buf, data.buf, 2
        SUB     data.shift, data.shift, 1

        MOV     r2, info.speriod // set up counter for symbol period

SYMBOL_PWM:
        // hold symbol low for (iperiod - duty)
        MOV     r1, info.iperiod // start inner counter
        // We waste a few instructions per PWM cycle with all the
        // initialization stuff above, which we've kept track of to this point.
        // Make sure we account for this extra time to keep our frequency as
        // accurate as possible.
        SUB     r1, r1, data.cycles
        LDI     REG_LEDS, 0 // disable LED output pins

PWM_DELAY_LOW:
        SUB     r1, r1, 2*NS_PER_INSTR // 2 instructions per loop
        QBLT    PWM_DELAY_LOW, r1, info.duty // loop while r1 > info.duty

        QBGT    PWM_CYCLE_END, r1, 10 // don't set output high if r1 < 10
        MOV     REG_LEDS, data.bits // set symbol high for remainder (duty)
PWM_DELAY_HIGH:
        SUB     r1, r1, 2*NS_PER_INSTR // 2 instructions per loop
        QBNE    PWM_DELAY_HIGH, r1, 0 // loop while 0 < r1 < info.duty

PWM_CYCLE_END:
        // Finished one PWM cycle. Do it again until time for the next symbol
        SUB     r2, r2, info.iperiod
        QBNE    SYMBOL_PWM, r2, 0 // repeat PWM cycle while r2 > 0

SYMBOL_NEXT:
        // Transmit the next symbol, but start the OFF PWM cycle first.
        LDI     REG_LEDS, 0 // disable LED output pins
        QBA     MAINLOOP

END:                               // end of program, send back interrupt
        // clear output pins first
        LDI     REG_LEDS, 0
        MOV     R31.b0, PRU0_R31_VEC_VALID | EVENTOUT0
        HALT
