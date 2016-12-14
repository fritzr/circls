#ifndef __XMIT_H_
#define __XMIT_H_

#define LEDR_PIN 0
#define LEDG_PIN 1
#define LEDB_PIN 2

// Size of PRU data memory
#define MAX_PRU_DATA 8192

typedef uint16_t fcs_t; // just use crc16 as fcs

// Our transmit header - we haven't determined this yet, but it should
// contain things like the packet length, MCS, etc...
struct circls_tx_hdr_t {
  uint16_t length; // full [unencoded] size of packet; header+data+FCS
  uint8_t  seq;    // sequence number
} __attribute__((packed));

struct xmit_flags_t {
  unsigned int _reserved : 8; // bits [7:0]
}; // 1 byte

struct pru_xmit_info_t {
  uint32_t symbol_period_ns; // duration of each symbol
  uint32_t period_ns;        // PWM period within a symbol, for dimming
  uint32_t duty_ns;          //     duty cycle within period_ns
  uint16_t data_len;         // length of data packet in bytes
  xmit_flags_t  flags;       // host to PRU communication flags
  uint8_t  _pad;             // pad structure to 4-byte boundary
} __attribute__((packed));

struct ir_flags_t {
  unsigned int _reserved : 8; // bits [7:0]
}; // 1 byte

struct pru_ir_info_t {
  ir_flags_t flags;
} __attribute__((packed));
static pru_xmit_info_t *pru0_data; // start of PRU memory - begins with tx info

struct shared_flags_t {
  unsigned int _reserved : 7; // bits [7:2]
  unsigned int start     : 1; // bit  [1]
  unsigned int halt      : 1; // bit  [0]
};

struct pru_shared_info_t {
  shared_flags_t flags; // host to PRU flags, common to PRU0 and PRU1
} __attribute((packed));

// Maximum packet size we can copy down to the PRU at once
#define MAX_PACKET \
  (MAX_PRU_DATA \
  - sizeof(pru_xmit_info_t) \
  - sizeof(circls_tx_hdr_t))

struct mapped_file {
  size_t len;
  uint8_t *mem;
  int fd;
};

#endif // __XMIT_H_
