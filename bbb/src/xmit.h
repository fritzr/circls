#ifndef __XMIT_H_
#define __XMIT_H_

#define LEDR_PIN 0
#define LEDG_PIN 1
#define LEDB_PIN 2

// Size of PRU data memory
#define MAX_PRU_DATA 8192

typedef uint16_t fcs_t; // just use crc16 as fcs

// Our transmit header - should contain things like the packet length, etc...
struct circls_tx_hdr_t {
  uint16_t length; // full [unencoded] size of packet; header+data+FCS
  uint8_t  seq;    // sequence number
} __attribute__((packed));

// Info for transmit PRU - this is copied to the start of PRU0 data memory.
struct pru_xmit_info_t {
  uint32_t symbol_period_ns; // duration of each symbol
  uint32_t period_ns;        // PWM period within a symbol, for dimming
  uint32_t duty_ns;          //     duty cycle within period_ns
  uint16_t data_len;         // length of data packet in bytes
  uint8_t  halt;       // host to PRU communication - halt
  uint8_t  _pad;             // pad structure to 4-byte boundary
} __attribute__((packed));

// 0  IR_NACK - send to "not acknowledge" a frame.
// Then data is the sequence number which the receiver missed.
#define IR_FC_NACK 0
// 1  IR_SYN  - send to "sync" with the transmitter on startup.
// Used to allow the receiver camera time to focus.
#define IR_FC_SYN  1

#define IR_FC_SHIFT 8
#define IR_FC_MASK  0x30
#define IR_FC(fc) (((fc) << IR_FC_SHIFT) & IR_FC_MASK)

#define IR_NACK IR_FC(IR_FC_NACK)
#define IR_SYN  IR_FC(IR_FC_SYN)

// Info for IR receive PRU - this is copied to the start of PRU1 data memory.
struct pru_ir_info_t {
  uint8_t flags;
  uint8_t count;
  uint8_t _pad[2];
  uint8_t cycles[64]; // 64B buffer describing the 512 cycles captured
} __attribute__((packed));


// Maximum packet size we can copy down to the PRU at once
#define MAX_PACKET \
  (MAX_PRU_DATA \
  - sizeof(pru_xmit_info_t) \
  - sizeof(circls_tx_hdr_t))

#endif // __XMIT_H_
