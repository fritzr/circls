// PRUSS program to perform PWM on three PRU output pins.
// The start of the PRU unit's data memory should contain the following data
// structures:
//
//  struct pru_xmit_info {
//    uint32_t symbol_period_ns; // duration of each symbol
//    uint32_t period_ns;        // PWM period within a symbol, for dimming
//    uint32_t duty_ns;          //     duty cycle within period_ns
//    uint8_t  check_halt;       // when set, will halt the PRU
//  }
//
// At any given time the PRU will output a PWM signal with its duty cycle
// set by duty_ns/period_ns on one of the configured output pins. The PRU will
// cycle which pin is outputting every symbol_period_ns.
//
// Outputs: r30.0 (P9_29), r30.1 (P0_30), r30.2 (P0_31)
//

.origin 0               // offset of start of program in PRU memory
.entrypoint START       // program entry point used by the debugger

#define PRU0_R31_VEC_VALID 32
#define EVENTOUT0 3

#define a_SYMBOL_PERIOD_NS 0x00000000
#define a_PERIOD_NS        0x00000004
#define a_DUTY_NS          0x00000008
#define a_CHECK_HALT       0x0000000c

#define SWITCH_SYMBOLS

#define LEDR_BIT 0
#define LEDG_BIT 1
#define LEDB_BIT 2

// Symbol period
#define REG_SPERIOD r4

// Intra-symbol period
#define REG_IPERIOD r5

// Intra-symbol duty cycle
#define REG_DUTY r6

// Output bit
#define REG_OUTBIT r7

// Halt bit
#define REG_A_HALT r8
#define REG_HALT r9

// 200MHz -> 5ns per instruction
#define NS_PER_INSTR 5

// Each PWM loop we subtract the PWM period from the running symbol period
// count. To keep our symbol frequency as accurate as possible, we need to
// adjust for any extra wasted instructions.
#define EXTRA_INSTR_PER_CYCLE 10

START:
        // Reading the memory that was set by the C program into registers
        // Intra-symbol period for dimming
        LDI     r0, a_PERIOD_NS
        LBBO    REG_IPERIOD, r0, 0, 4

        // Duty-cycle, portion of r5 (PERIOD_NS)
        LDI     r0, a_DUTY_NS
        LBBO    REG_DUTY,    r0, 0, 4

        // Symbol mask - which bit to set on r30
        LDI     REG_OUTBIT, LEDR_BIT

READ_PERIOD:
        // Inter-symbol period <-> transmit frequency
        LDI     r0, a_SYMBOL_PERIOD_NS
        LBBO    REG_SPERIOD, r0, 0, 4

        // We need to check whether the IPERIOD is divisible by SPERIOD.
        // If not, the duty cycle will be wrong and the xmit frequency will
        // drift.  However skip this step if the duty-cycle or intra-period are
        // zero.
        QBEQ    CHECK_DIVIS, REG_DUTY, 0
        QBEQ    CHECK_DIVIS, REG_IPERIOD, 0

        MAX     r0, REG_IPERIOD, REG_SPERIOD
        MIN     r1, REG_IPERIOD, REG_SPERIOD

CHECK_DIVIS: // while (x > 0) x -= y; if (x != 0) FAIL
        SUB     r0, r0, r1
        QBGT    END, r0, 0 // If r0 < 0, we are not divisible so we quit!
        QBLT    CHECK_DIVIS, r0, 0 // If r0 > 0, keep subtracting

        LDI     REG_A_HALT, a_CHECK_HALT
MAINLOOP:
        // Quit if the halt register is asserted
        LBBO    REG_HALT, REG_A_HALT, 0, 4
        QBNE    END, REG_HALT, 0

        CLR     r30, LEDR_BIT
        CLR     r30, LEDG_BIT
        CLR     r30, LEDB_BIT

        MOV     r2, REG_SPERIOD // counter for symbol period

SYMBOL_PWM:
        CLR     r30, REG_OUTBIT // hold symbol low for (period_ns - duty_ns)
        MOV     r1, REG_IPERIOD // start inner counter
        // We actually waste a few instructions per PWM cycle. Make sure we
        // account for this extra time to keep our symbol frequency as accurate
        // as possible.
        SUB     r1, r1, EXTRA_INSTR_PER_CYCLE*NS_PER_INSTR

PWM_DELAY_LOW:
        SUB     r1, r1, 2*NS_PER_INSTR // 2 instructions per loop
        QBLT    PWM_DELAY_LOW, r1, REG_DUTY // loop while r1 > REG_DUTY

        QBEQ    PWM_CYCLE_END, r1, 0 // don't set output high if we have 0 left
        SET     r30, REG_OUTBIT // set symbol high for remainder (duty_ns)
PWM_DELAY_HIGH:
        SUB     r1, r1, 2*NS_PER_INSTR // 2 instructions per loop
        QBNE    PWM_DELAY_HIGH, r1, 0 // loop while 0 < r1 < REG_DUTY

PWM_CYCLE_END:
        // Finished one PWM cycle. Do it again until time for the next symbol
        SUB     r2, r2, REG_IPERIOD
        QBNE    SYMBOL_PWM, r2, 0 // repeat PWM cycle while r2 > 0

        // Here we cycle through the three symbol bits. If we just did bit 2
        // we need to add an extra to skip 3 and go to 4 (which becomes 0 when
        // masked below).
        QBBC    SYMBOL_NEXT, REG_OUTBIT, 1
        ADD     REG_OUTBIT, REG_OUTBIT, 1

SYMBOL_NEXT:
        // Increment the symbol bit to transmit.
        ADD     REG_OUTBIT, REG_OUTBIT, 1
        AND     REG_OUTBIT, REG_OUTBIT, 3

        QBA     MAINLOOP
END:                               // end of program, send back interrupt
        // clear output pins first
        CLR     r30, LEDR_BIT
        CLR     r30, LEDG_BIT
        CLR     r30, LEDB_BIT
        MOV     R31.b0, PRU0_R31_VEC_VALID | EVENTOUT0
        HALT
