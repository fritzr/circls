// Region mapped to each PRU's control registers
#define PRU0_CTL 0x22000
#define PRU1_CTL 0x24000

// Ofsset of the CTPPR0 control register from PRUx_CTL
#define CTPPR0_OFFSET 0x28

// Full offset of CTPPR0 for PRUx
#define PRU0_CTPPR0 PRU0_CTL + CTPPR0_OFFSET
#define PRU1_CTPPR0 PRU1_CTL + CTPPR0_OFFSET

#define SHARED_MEMORY 0x10000
// Configure the c28 control register to point to the start of shared RAM,
// which is mapped to 0x10000.

// [15:0] in c28_pointer (nnnn) becomes offset 0x00 nnnn 00
// So CTPPR0_SHARED into c28_pointer becomes   0x00 0100 00
#define CTPPR0_SHARED 0x0100

// Constant register which refers to shared memory, set up in CTPPR0 as above
#define SHMEM_COFF c28 // uses c28_pointer as discussed above


// Macros to setup shared memory in the SHMEM_COFF constant table register
// for each PRU.

.macro PRU0_SETUP_SHMEM
  MOV     r0, PRU0_CTPPR0
  LDI     r1.w0, CTPPR0_SHARED
  // c4 points to the config register map, offset 0x28 controls CTPPR0
  SBBO    r1.w0, r0, 0, 2
.endm

.macro PRU1_SETUP_SHMEM
  MOV     r0, PRU1_CTPPR0
  LDI     r1, CTPPR0_SHARED
  // c4 points to the config register map, offset 0x28 controls CTPPR0
  SBBO    r1, r0, 0, 4
.endm
