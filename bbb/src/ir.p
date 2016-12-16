// PRUSS program to poll an input pin for incoming IR data.
// This runs on PRU1 in parallel with the transmit code on PRU0 (see xmit.p).
//
// IR data comes across as pulses on a 38.4kHz carrier wave.
// We encode data most-significant-bit first as we blast it across the air.
// We use OOK encoding essentially, since both ends of the IR transceiver
// have fairly consistent timing. A pulse (half on, half off) indicates a '1'
// bit, and a valley (double off) indicates a '0'.
//
// The received IR frame is just two bytes, and looks like the following:
//
// <===== 1B =====> <===== 1B =====>
// ----------------------------------
// | magic |   fc  |      data      |
// ----------------------------------
//    4b       4b          8b
//
// The fields are as follows:
//   magic - always '1010', so we can identify the IR frame start and bit rate
//   fc    - frame control, to identify the type of IR packet
//   data  - depends on the fc
//
// The following frames are supported:
// fc      name     data
// 0       NACK     missed sequence number
// 1-127   <reserved>
//
// This code simply waits until all 16 bits (2B) of the IR frame have been
// received, then copies the data to PRU data memory and generates an
// interrupt. The start of PRU data memory looks like this:

.struct pru_ir_info
  .u8  flags    // host flags, see PFLAGS_*
  .u8  count    // incremented on each received IR frame
  // followed by a 512-cycle buffer totaling 64B
.ends
.assign pru_ir_info, r5.b2, r5.b3, info

.struct pru_vars
  .u8  size    // size of data to transfer - always 64B
  .u8  bitsloop // loop through sampling 32 bits
  .u8  regsloop // loop through sampling 16 regs
  .u8  r0b3
  .u8  reg     // register pointer: write buf to *reg when it's full
  .u8  r1b1
  .u8  r1b2
  .u8  r1b3
  .u32 buf     // bits are read into buf
  .u32 fscount // counter for waiting a sample period
  .u32 off     // offset of data from start of data memory, always 4B
.ends
.assign pru_vars, r0, r4, data
#define REGFILE_START r8 // from here, 16 registers = 512 cycles @ 1 bit/cycle

// PFLAGS_* - private flags for PRU1
#define PFLAGS_EVENT 0 // bit 0

#define IR_IN r31.t16
#define IR_INREG r31
#define IR_INBIT 3

#define NS_PER_INSTR 5
// inverse of the carrier frequency (38.4kHz in our case)
#define CYCLE_PERIOD     26042 // nanoseconds

// number of cycles in one pulse
#define CYCLES_PER_PULSE 8

// number of pulses comprising one symbol
#define PWM_PULSES       4

// Sample period at which we read the IR pin.
// We subtract the time for the 4 wasted instructions in the BITS_LOOP
// for optimal accuracy.
#define FS_PERIOD 26040 - 4*NS_PER_INSTR

#define PRU_R31_VEC_VALID 32
#define EVENTOUT1 4

.origin 0               // offset of start of program in PRU memory
.entrypoint START       // program entry point used by the debugger

START:
        LDI    data.off,  4  // Write data back at 4B past start of data memory
        LDI    data.size, 64 // Always send back 64B = 512 cycles
        LDI    r7, 0

        // Wait until IR bit is active (active low)
INACTIVE_WAIT:
        WBC    r31, IR_INBIT

        LDI    data.reg, &REGFILE_START

        // We usually transmit for:
        //   8 cycles/pulse * 4 pulses/bit * 16 bits => 512 cycles
        // Thus we use 16 registers to buffer the data:
        //   32 bits/register * 1 bit/cycle => 512 cycles
        LDI    data.regsloop, 16
REGS_LOOP:

        // Copy 32 bits into buf, then copy out to register file
        MOV    data.buf, 0
        LDI    data.bitsloop, 32
BITS_LOOP:
        // Read the next bit - HIGH means INACTIVE, so set a 1 on LOW
        LSL    data.buf, data.buf, 1
        QBBS   INACTIVE, IR_INREG, IR_INBIT
        OR     data.buf, data.buf, 1 // ACTIVE, set 1
INACTIVE:

        // Wait one sample period before we check the next bit
        LDI    data.fscount, FS_PERIOD
WAIT_FS:
        SUB    data.fscount, data.fscount, 2*NS_PER_INSTR
        QBNE   WAIT_FS, data.fscount, 0

        // Loop while we have more bits to read
        SUB    data.bitsloop, data.bitsloop, 1
        QBNE   BITS_LOOP, data.bitsloop, 0

        // Done with all 32 bits in r0, copy them back to the register file
        MVID   *data.reg, data.buf   // copy r0 into regfile[reg++]
        ADD    data.reg, data.reg, 4 // next 4 bytes

        // Loop while we have more regs to fill
        SUB    data.regsloop, data.regsloop, 1
        QBNE   REGS_LOOP, data.regsloop, 0

        // Done with all 16 registers, giving us a 512-cycle buffer
        // in the register file starting at REGFILE_START

        // First transmit info up to host
        CLR    info.flags, PFLAGS_EVENT
        ADD    info.count, info.count, 1 // increment frame-sent count
        SBBO   &info, r7, 0, SIZE(info) // c4 === 0 by default

        // Then transmit the cycle buffer
        // Store &REGFILE_START to data address 4 (after info struct), size 64B
        LDI    data.reg, &REGFILE_START
        SBBO   &REGFILE_START, data.off, 0, b0 // b0 === data.size

        // Note it is important here that the PFLAGS_EVENT bit in info.flags
        // gets written LAST, otherwise the host may read the entire data
        // structure before we've finished writing to it.
        SET    info.flags, PFLAGS_EVENT
        SBBO   &info.flags, r3, OFFSET(info.flags), SIZE(info.flags)

        // Wait for activity on IR and repeat
        QBA   INACTIVE_WAIT

END:    // end of IR program, nobody cares
        HALT
