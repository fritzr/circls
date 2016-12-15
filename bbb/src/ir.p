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
  .u8  flags    // [Read]  host flags, see PFLAGS_*
  .u8  count    // [Write] incremented on each received IR frame
  .u16 frame    // [Write] received frame data
.ends
.assign pru_ir_info, r4, r4, info
#define FRAME_BITS 16  // number of bits per frame
#define PWM_WIDTH   4  // number of pulses in one bit

// PFLAGS_* - private flags for PRU1

// SFLAGS_* - shared flags for PRU0/1
#define SFLAGS_HALT  0 // bit 0, set to terminate program
#define SFLAGS_START 1 // bit 1, set to allow program to start

.struct pru_data
   .u32 fs_period
   .u8 bits       // from 0 to FRAME_BITS-1
   .u8 npulse     // from 0 to PWM_WIDTH-1
   .u8 parity     // number of HIGH pulses in a PWM_WIDTH window
   .u8 pad
.ends
.assign pru_data, r5, r6, data

#define IR_IN r31.t16
#define IR_INREG r31
#define IR_INBIT 16

#define NS_PER_INSTR 5
// inverse of the carrier frequency (38.4kHz in our case)
#define CYCLE_PERIOD     26042 // nanoseconds

// number of cycles in one pulse
#define CYCLES_PER_PULSE 8

// number of pulses comprising one symbol
#define PWM_PULSES       4

// sample period at which we read the IR pin, once per pulse in this case
// yes, Nyquist says we need twice this to find all the cycles,
// but this vastly simplifies the decoding for the common case
#define FS_PERIOD 208330 // round(CYCLES_PER_PULSE * CYCLE_PERIOD, nearest 10)

#define PRU_R31_VEC_VALID 32
#define EVENTOUT1 4

.origin 0               // offset of start of program in PRU memory
.entrypoint START       // program entry point used by the debugger

START:
        MOV    data.fs_period, FS_PERIOD

        // keep reading the IR bit until we are active
INACTIVE_WAIT:
//        MOV r0, data.fs_period
//WAIT_FS1:
//        SUB r0, r0, 2*NS_PER_INSTR // 2 instructions per loop cycle
//        QBNE WAIT_FS1, r0, 0
//        QBBS INACTIVE_WAIT, IR_INREG, IR_INBIT
        WBC    IR_IN

        LDI    info.frame, 0
        LDI    data.bits, FRAME_BITS

LOOP_READBIT:
        LDI    data.npulse, PWM_PULSES
        LDI    data.parity, 0

LOOP_READPULSE:
        // Count parity of each pulse
        QBBC   ZERO_PULSE, IR_IN // if we see a '1' pulse, add 1 to parity
        ADD    data.parity, data.parity, 1
ZERO_PULSE: // ADD data.parity, data.parity, 0
        MOV r0, data.fs_period
WAIT_FS2:
        SUB r0, r0, 2*NS_PER_INSTR // 2 instructions per loop cycle
        QBNE WAIT_FS2, r0, 0
        // Keep going until we've read enough pulses
        SUB    data.npulse, data.npulse, 1
        QBNE   LOOP_READPULSE, data.npulse, 0

        // We are active low; symbols are defined as follows:
        //
        // Symbol:        0                  1
        //          _   _________      _         ___
        //           |_|                |_______|
        // Pulses:    0  1  1  1         0  0  0  1
        //
        // Therefore, if parity is greater than 1 we say the symbol is 0.
        QBLT   PARITY_ZERO, data.parity, 1 // parity > 1, -> symbol 0
        OR     info.frame, info.frame, 1   //   otherwise -> symbol 1
PARITY_ZERO:
        LSL    info.frame, info.frame, 1   // shift bits into place

        // Read more bits if we need them
        SUB    data.bits, data.bits, 1
        QBNE   LOOP_READBIT, data.bits, 0

        // Write back to memory
        ADD    info.count, info.count, 1 // increment frame-written count
        SBCO   info, c24, 0, SIZE(info) // c24 == 0 info.count, info.frame

        // Notify host of new IR frame
        LDI    r30, PRU_R31_VEC_VALID + EVENTOUT1

        // Wait for activity on IR
        QBA   INACTIVE_WAIT

END:    // end of IR program, nobody cares
        HALT
