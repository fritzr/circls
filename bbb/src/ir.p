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
// PFLAGS_* - private flags for PRU1

// The beginning of PRU shared memory is laid out as follows:
.struct pru_shared_info
  .u8  flags    // host flags - see SFLAGS_* bits
.ends
.assign pru_xmit_info, r11.b0, r11.b0, shared
// SFLAGS_* - shared flags for PRU0/1
#define SFLAGS_HALT  0 // bit 0, set to terminate program
#define SFLAGS_START 1 // bit 1, set to allow program to start


.origin 0               // offset of start of program in PRU memory
.entrypoint START       // program entry point used by the debugger

#define PRU_R31_VEC_VALID 32
#define EVENTOUT1 4

START:
        // Wait for START flag to synchronize start times, if HALT is set first
        // then we can quit immediately
        LDI     r0, 0
SYNC_START:
        LBCO    shared.flags, C28, OFFSET(shared.flags), SIZE(shared.flags)
        QBBS    END, shared.flags, SFLAGS_HALT
        QBBC    SYNC_START, shared.flags, SFLAGS_START


END:                               // end of program, send back interrupt
        MOV     R31.b0, PRU_R31_VEC_VALID | EVENTOUT1
        HALT
