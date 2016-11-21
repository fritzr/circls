/* PRUSS program to drive a HC-SR04 sensor and display the sensor output
*  in Linux userspace by sending an interrupt.
*  written by Derek Molloy for the book Exploring BeagleBone
*/

#include <prussdrv.h>
#include <pruss_intc_mapping.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <signal.h>

#include <cstring>
#include <cstdlib>
#include <cctype>

#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

#define PRU_PROG "./bin/xmit.bin"
#define PRU_NUM 0
#define LEDR_PIN 0
#define LEDG_PIN 1
#define LEDB_PIN 2

typedef struct pru_xmit_info {
  uint32_t symbol_period_ns; // duration of each symbol
  uint32_t period_ns;        // PWM period within a symbol, for dimming
  uint32_t duty_ns;          //     duty cycle within period_ns
  uint8_t  halt;             // set to halt the PRU loop
} pru_xmit_info_t;

static pru_xmit_info_t *pru_data;

static void
usage (const char *prog, const char *errmsg="") __attribute__((noreturn));

static void
usage (const char *prog, const char *errmsg)
{
  cerr << "usage: " << prog << " [-v] [-d <duty>] <frequency>[KM]" << endl;
  cerr << "Cycle through transmittable symbols with the given symbol frequency."
       << endl << "The frequency may end in 'K' or 'M' for kHz or MHz." << endl
       << "Options:" << endl
       << "    -v\t\tverbose output" << endl
       << "    -d <duty>\tduty cycle (0 - 100)" << endl
       << errmsg << endl;
  exit(1);
}

static void
getOptions (pru_xmit_info_t &info, int argc, char *argv[])
{
  bool verbose = false;
  unsigned long duty = 50; // percent
  char *duty_str = NULL;

  int opt;
  while ((opt = getopt (argc, argv, "vd:")) != -1)
  {
    switch (opt)
    {
      case 'd': // debug
        duty_str = optarg;
        break;
      case 'v':
        verbose = true;
        break;
      default:
        usage(argv[0], "bad options");
    }
  }

  if (optind >= argc)
    usage (argv[0], "expected <frequency_hz> argument");

  // Get duty cycle
  if (duty_str)
  {
    istringstream duty_is (duty_str);
    duty_is >> duty;
    if (duty_is.bad () || duty > 100)
      usage (argv[0], "bad duty cycle percentage");
  }

  // Parse frequency string, which may end in [kKmM]
  double fdiv = 0;
  char *freq_str = argv[optind];
  char *end = argv[optind];

  while (*end != '\0' && !isspace(*end))
  {
    switch (*end)
    {
      case 'k':
      case 'K': // kHz
        fdiv = 1000.0*1000.0;
        break;
      case 'm':
      case 'M': // MHz
        fdiv = 1000.0;
        break;
      default:
        break;
    }
    if (fdiv != 0)
      break;
    end++;
  }
  // No suffix, interpret frequency in Hz
  if (*end == '\0' || isspace(*end))
    fdiv = 1000.0*1000.0*1000.0;
  *end = '\0';

  double frequency_hz = 0;
  istringstream freq_in (freq_str);
  freq_in >> frequency_hz;
  if (freq_in.bad ())
    usage ("bad frequency string");

  // Convert frequency to GHz, so period_ns = 1/f;
  double frequency_ghz = static_cast<double>(frequency_hz) / fdiv;
  double period_ns = 1.0 / frequency_ghz;

  // Round to nearest 5
  period_ns = 5.0 * floor (ceil (period_ns + 2.5) / 5.0);

  // Make sure we don't go over the max value of a signed 32-bit integer
  if (period_ns >= static_cast<double>(static_cast<uint32_t>(-1)>>1))
    usage (argv[0], "period too large");

  info.symbol_period_ns = static_cast<uint32_t> (period_ns);

  // hard-coded intra-symbol period/duty-cycle
  info.period_ns = 2000; // 500kHz
  info.duty_ns = (info.period_ns * duty) / 100;

  if (verbose)
  {
    cout << "    frequency: " << frequency_ghz << "GHz" << endl
         << "symbol period: " << info.symbol_period_ns << " ns" << endl
         << "   pwm period: " << info.period_ns << " ns" << endl
         << "     pwm duty: " << info.duty_ns << " ns" << endl;
  }

  if ((info.symbol_period_ns % info.period_ns) != 0)
  {
    cerr << "symbol period not divisible by PWM period!" << endl;
    exit (2);
  }
}

static volatile bool run;

static void
handle_interrupt(int signum)
{
  if (run)
  {
    run = false;
    pru_data->halt = 1;
    cout << "sent halt" << endl;
  }
  // After two interrupts, force quit
  else
  {
    cerr << "force quit" << endl;
    /* Disable PRU and close memory mappings.  */
    prussdrv_pru_disable (PRU_NUM);
    prussdrv_exit ();

    exit(2);
  }
}

int
main (int argc, char *argv[])
{
  int ret = 2;
  int evt;
  tpruss_intc_initdata interrupts = PRUSS_INTC_INITDATA;

  if (getuid () != 0) {
    cerr << "must be root" << endl;
    exit(1);
  }

  ifstream bin(PRU_PROG);
  if (!bin.good ())
  {
    cerr << "failed to find PRU binary (" << PRU_PROG << ")" << endl;
    exit(1);
  }

  pru_xmit_info_t pwm_options;
  memset (&pwm_options, 0, sizeof(pwm_options));
  getOptions (pwm_options, argc, argv);
  if (pwm_options.period_ns > pwm_options.symbol_period_ns
      || ((pwm_options.symbol_period_ns % pwm_options.period_ns) != 0))
    usage (argv[0], "bad PWM options");

  // Allocate and initialize memory
  if (prussdrv_init () < 0)
  {
    cerr << "failed to init PRUSS!" << endl;
    goto done;
  }

  // events
  prussdrv_open (PRU_EVTOUT_0);

  // map interrupts
  prussdrv_pruintc_init(&interrupts);

  // Copy data to PRU memory
  if (prussdrv_map_prumem (PRUSS0_PRU0_DATARAM,
      reinterpret_cast<void **>(&pru_data)) < 0)
  {
    cerr << "failed to map PRU memory!" << endl;
    goto done;
  }

  memcpy (pru_data, &pwm_options, sizeof(pwm_options));

  // Load and execute binary on PRU
  if (prussdrv_exec_program (PRU_NUM, PRU_PROG) < 0)
  {
    cerr << "failed to exec PRU program!" << endl;
    goto done;
  }

  // Wait for interrupt
  run = true;
  signal (SIGINT,  handle_interrupt);
  signal (SIGQUIT, handle_interrupt);

  evt = prussdrv_pru_wait_event (PRU_EVTOUT_0);
  cout << "PRU done, event " << evt << endl;

  ret = 0;
done:
  /* Disable PRU and close memory mappings.  */
  prussdrv_pru_disable (PRU_NUM);
  prussdrv_exit ();

  exit(ret);
}
