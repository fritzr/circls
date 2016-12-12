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
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdio>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

extern "C"
{
#include "ecc.h" // rs-lib
};

using namespace std;

#define PRU_PROG "./bin/xmit.bin"
#define PRU_NUM 0
#define LEDR_PIN 0
#define LEDG_PIN 1
#define LEDB_PIN 2

// Size of PRU data memory
#define MAX_PRU_DATA 8192

// Our transmit header - we haven't determined this yet, but it should
// contain things like the packet length, MCS, etc...
struct circls_tx_hdr_t {
  uint8_t _reserved[32];
};

struct pru_xmit_info_t {
  uint32_t symbol_period_ns; // duration of each symbol
  uint32_t period_ns;        // PWM period within a symbol, for dimming
  uint32_t duty_ns;          //     duty cycle within period_ns
  uint16_t data_len;         // length of data packet in bytes
  uint8_t  halt;             // set to halt the PRU loop
  uint8_t  _pad;             // pad structure to 4-byte boundary
} __attribute__((packed));
static pru_xmit_info_t *pru_data; // start of PRU memory - begins with tx info

// Maximum packet size we can copy down to the PRU at once
#define MAX_PACKET \
  (MAX_PRU_DATA \
  - sizeof(pru_xmit_info_t) \
  - sizeof(circls_tx_hdr_t) \
  - NPAR) /* NPAR - parity bits added from RS encoding */

struct mapped_file {
  size_t len;
  uint8_t *mem;
  int fd;
};

static void
dump_buf (const uint8_t *buf, size_t buflen, const char *msg=NULL)
{
  if (msg)
    cout << msg << ":" << endl;
  for (size_t i = 0; i < buflen; i++)
  {
    if (i % 16 == 0)
      cout << hex << setw(4) << setfill('0') << i << ": ";
    cout << hex << setw(2) << setfill('0') << *buf++;
  }
}

static void
usage (const char *prog, const char *errmsg="", const char *err2=NULL)
  __attribute__((noreturn));

static void
usage (const char *prog, const char *errmsg, const char *err2)
{
  cerr
    << "usage: " << prog << " [-v] [-d <duty>] [-f <frequency>[KM]] <datafile>"
    << endl
    << "Transmit data from the given file. You may specify a" << endl
    << "symbol frequency for transmit, and a duty cycle for dimming." << endl
    << endl
    << "Options:" << endl
    << "    -v\t\tverbose output" << endl
    << "    -d <duty>\tduty cycle (0 - 100), default 50" << endl
    << "    -f <frequency>[kM]\tfrequency (Hz [kHz/MHz]), default 100Hz" << endl
    << errmsg;
  if (err2)
    cerr << ": " << err2;
  cerr << endl;
  exit(1);
}

static void
parse_frequency (pru_xmit_info_t &info, char *freq_str, bool verbose)
{
  info.symbol_period_ns = 1000 * 1000 * 10; // default 100 Hz
  if (freq_str == NULL)
    return;

  double fdiv = 0;
  char *end = freq_str;

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
    usage ("xmit", "bad frequency string");

  // Convert frequency to GHz, so period_ns = 1/f;
  double frequency_ghz = static_cast<double>(frequency_hz) / fdiv;
  double period_ns = 1.0 / frequency_ghz;

  // Round to nearest 5
  period_ns = 5.0 * floor (ceil (period_ns + 2.5) / 5.0);

  // Make sure we don't go over the max value of a signed 32-bit integer
  if (period_ns >= static_cast<double>(static_cast<uint32_t>(-1)>>1))
    usage ("xmit", "period too large");

  info.symbol_period_ns = static_cast<uint32_t> (period_ns);
}

static bool
getOptions (int argc, char *argv[], pru_xmit_info_t &info, mapped_file &data)
{
  bool verbose = false;
  unsigned long duty = 50; // percent
  char *duty_str = NULL;
  char *data_file = NULL;
  char *freq_str = NULL;

  int opt;
  while ((opt = getopt (argc, argv, "vd:")) != -1)
  {
    switch (opt)
    {
      case 'd':
        duty_str = optarg;
        break;
      case 'f':
        freq_str = optarg;
        break;
      case 'v':
        verbose = true;
        break;
      default:
        usage(argv[0], "bad options");
    }
  }

  if (optind >= argc)
    usage (argv[0], "expected <datafile> argument");
  data_file = argv[optind];

  // Get duty cycle
  if (duty_str)
  {
    istringstream duty_is (duty_str);
    duty_is >> duty;
    if (duty_is.bad () || duty > 100)
      usage (argv[0], "bad duty cycle percentage");
  }
  // hard-coded intra-symbol period/duty-cycle
  info.period_ns = 2000; // 500kHz
  info.duty_ns = (info.period_ns * duty) / 100;

  // Get frequency
  parse_frequency (info, freq_str, verbose);

  if (verbose)
  {
    cout << "symbol period: " << info.symbol_period_ns << " ns" << endl
         << "   pwm period: " << info.period_ns << " ns" << endl
         << "     pwm duty: " << info.duty_ns << " ns" << endl;
  }

  if (info.symbol_period_ns < info.period_ns)
    usage (argv[0], "period too small");

  if ((info.symbol_period_ns % info.period_ns) != 0)
    usage (argv[0], "symbol period not divisible by duty period");

  // Map data file
  struct stat st;
  memset (&st, 0, sizeof(st));
  int ret = stat (data_file, &st);
  if (ret < 0)
    usage (argv[0], "failed to stat data file", strerror (errno));
  data.len = st.st_size;

  data.fd = open (data_file, O_RDONLY);
  if (data.fd < 0)
    usage (argv[0], "failed to open data file", strerror (errno));

  data.mem = (uint8_t*)mmap (NULL, data.len, PROT_READ, MAP_PRIVATE, data.fd,0);
  if (data.mem == MAP_FAILED)
  {
    close (data.fd);
    usage (argv[0], "failed to map data file", strerror (errno));
  }

  if (verbose)
  {
    cout << "mapped file " << data_file << ": size " << data.len << " bytes"
      << endl;
  }

  return verbose;
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

static void
make_tx_header (circls_tx_hdr_t *outbuf, const mapped_file &data)
{
  // TODO design and populate tx header
  memset (outbuf, 0, sizeof (circls_tx_hdr_t));
}

/* We can only encode in 256-byte chunks.  */
static void
encode_rs (uint8_t *outbuf, const mapped_file &data)
{
  initialize_ecc ();

  //size_t nchunks = data.len / 256;
  size_t in_idx = 0;
  size_t out_idx = 0;
  size_t left = data.len;

  /* Encode in 256-byte chunks, where NPAR of the bytes are for parity.  */
  while (left > 0)
  {
    size_t in_chunk = 256 - NPAR;
    if (left < in_chunk)
      break;

    encode_data (data.mem + in_idx, in_chunk, outbuf + out_idx);

    in_idx += in_chunk;
    out_idx += in_chunk + NPAR;
  }

  /* If we have a leftover chunk smaller than 256-NPAR, pad out with
   * extra data, in a pattern that will result in an even number of each
   * symbol.  */
  if (left > 0)
  {
    uint8_t last_chunk[256];
    memcpy (last_chunk, data.mem + in_idx, left);
    memset (last_chunk + left, 0x2d, 256 - left); // == 00 10 11 01
    encode_data (last_chunk, 256 - NPAR, outbuf + out_idx);
  }
}

static bool
decode_rs_check (const uint8_t *encoded, size_t length)
{
  bool check_pass = true;

  if (length % 256 != 0)
  {
    cerr << "encoded message length (" << length << " bytes)"
      " not divisible by 256" << endl;
    return false;
  }

  // This length will be too long, since we won't copy out the parity bits,
  // but close enough
  uint8_t *outbuf = new uint8_t[length];
  memcpy (&outbuf[0], encoded, length);

  size_t left = length;
  size_t i = 0;
  size_t out_index = 0;
  for (; left > 0; left -= 256)
  {
    decode_data (outbuf + out_index, 256);
    int syndrome = check_syndrome ();
    if (syndrome != 0)
    {
      cerr << "non-zero syndrome in codeword " << i << ": "
        << hex << setw(8) << setfill('0') << syndrome << endl;
      int st = correct_errors_erasures (outbuf + out_index, 256, 0, NULL);
      if (st != 1)
        cerr << "  -> error correction failed" << endl;
    }
    i++;
    out_index += 256 - NPAR; // don't keep parity bits in the output
  }

  dump_buf (encoded, length, "decode check");

  return check_pass;
}

int
main (int argc, char *argv[])
{
  int ret = 2;
  int evt;
  bool verbose;
  uint8_t *txbuf = NULL;
  size_t txsize, parity_len, data_rounded;
  int mask;

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
  mapped_file data;
  memset (&data, 0, sizeof(data));
  verbose = getOptions (argc, argv, pwm_options, data);

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

  /* Now we can figure out how long the tx packet is.  Make space for it.  */

  // Round data length up to next multiple of 256 - we will pad it out
  mask = (1 << 8) - 1; 
  data_rounded = (data.len + mask) & ~mask;

  // Add space for parity bits following each codeword
  parity_len = (data.len / 256) * NPAR;

  // Final tx packet length
  txsize = sizeof(circls_tx_hdr_t) + data_rounded + parity_len;
  txbuf = new uint8_t[txsize];

  // Pass the size of the data to the PRU
  pwm_options.data_len = txsize;

  /* Inject the tx header.  */
  make_tx_header ((circls_tx_hdr_t *)txbuf, data);
  if (verbose)
    dump_buf (txbuf, sizeof(circls_tx_hdr_t), "tx header");

  /*  Encode the entire packet using RS.  */
  encode_rs (txbuf, data);
  if (verbose)
  {
    dump_buf (txbuf, txsize, "encoded packet");
    // Sanity check, don't continue if we fail
    if (!decode_rs_check (txbuf, txsize))
    {
      cerr << "sanity check failed!!!";
      goto done;
    }
  }

  /* Copy tx info into PRU memory */
  memcpy (pru_data, &pwm_options, sizeof(pwm_options));

  /* Copy the encoded packet into PRU memory.  */
  memcpy (pru_data + 1, txbuf, txsize);

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

  munmap (data.mem, data.len);
  close (data.fd);

  if (txbuf)
    delete[] txbuf;

  exit(ret);
}
