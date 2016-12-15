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


// Various flags used to communicate internally with the PRUs.
struct xmit_flags_t {
  unsigned int _reserved : 7; // bits [7:1]
  unsigned int halt;          // bit [0]
}; // 1 byte

struct ir_flags_t {
  unsigned int _reserved : 8; // bits [7:0]
}; // 1 byte


// Info for transmit PRU - this is copied to the start of PRU0 data memory.
struct pru_xmit_info_t {
  uint32_t symbol_period_ns; // duration of each symbol
  uint32_t period_ns;        // PWM period within a symbol, for dimming
  uint32_t duty_ns;          //     duty cycle within period_ns
  uint16_t data_len;         // length of data packet in bytes
  xmit_flags_t  flags;       // host to PRU communication flags
  uint8_t  _pad;             // pad structure to 4-byte boundary
} __attribute__((packed));

struct ir_frame_t {
  unsigned int magic : 4; // should always be 1010 (0xa)
  unsigned int fc    : 4; // frame control, see below
  uint8_t      data;      // varies based on FC
} __attribute__((packed)); // 2 bytes

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
  ir_flags_t flags;
  uint8_t count;
  ir_frame_t frame;
} __attribute__((packed));


// Maximum packet size we can copy down to the PRU at once
#define MAX_PACKET \
  (MAX_PRU_DATA \
  - sizeof(pru_xmit_info_t) \
  - sizeof(circls_tx_hdr_t))

#endif // __XMIT_H_
