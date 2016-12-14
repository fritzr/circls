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

#define PRU0_PROG "./bin/xmit.bin"
#define PRU1_PROG "./bin/ir.bin"
#define PRU_NUM 0

static pru_xmit_info_t   *pru0_data;  // start of PRU0 memory
static pru_ir_info_t     *pru1_data;  // start of PRU1 memory
static pru_shared_info_t *pru_shared; // start of shared PRU memory

static void
dump_buf (const uint8_t *buf, size_t buflen, const char *msg=NULL)
{
  if (msg)
    cout << msg << " (";
  cout << "length " << dec << buflen;
  if (msg)
    cout << ")";
  cout << endl;

  if (!buf || !buflen)
    return;

  cout << "0000: ";
  for (size_t i = 0; i < buflen; i++)
  {
    if (i && ((i % 16) == 0))
      cout << endl << hex << setw(4) << setfill('0') << i << ": ";
    else if (i && ((i % 8) == 0))
      cout << "  ";
    cout << hex << setw(2) << setfill('0') << (int) *buf++ << " ";
  }
  cout << endl;
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
  while ((opt = getopt (argc, argv, "vd:f:")) != -1)
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

  if (data.len > MAX_PACKET)
    usage (argv[0], "data too long");

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
    pru_shared->flags.halt = 1;
    cout << "sent halt" << endl;
  }
  // After two interrupts, force quit
  else
  {
    cerr << "force quit" << endl;
    /* Disable PRUs and close memory mappings.  */
    prussdrv_pru_disable (0);
    prussdrv_pru_disable (1);
    prussdrv_exit ();

    exit(2);
  }
}

static void
make_tx_header (circls_tx_hdr_t *outbuf, const mapped_file &data)
{
  // TODO design and populate tx header
  outbuf->length = data.len;
  outbuf->seq = 0;
}

/* We can only encode in 255-byte chunks (codewords), which includes NPAR
 * parity bytes.  We stash the 251B data + 4B parity together in 255B blocks,
 * so we need slightly more output than input data.  */
static void
encode_rs (uint8_t *__restrict outbuf, uint8_t *inbuf, size_t insize)
{
  initialize_ecc ();

  size_t in_idx = 0;
  size_t out_idx = 0;
  size_t left = insize;

  /* Encode data in 255-byte chunks, where NPAR of the bytes are for parity.  */
  while (left > 0)
  {
    size_t in_chunk = 255 - NPAR;
    if (left < in_chunk)
      in_chunk = left;

    encode_data (inbuf + in_idx, in_chunk, outbuf + out_idx);

    in_idx += in_chunk;
    out_idx += in_chunk + NPAR;
    left -= in_chunk;
  }
}

static bool
decode_rs_check (uint8_t *encoded, size_t length)
{
  bool check_pass = true;
  int st;

  // This length will be too long, since we won't copy out the parity bits,
  // but close enough.
  uint8_t *outbuf = new uint8_t[length];

  // Then decode the remainder of the message, in 255-byte chunks.
  // The input in 255 - NPAR byte data chunks, with NPAR parity bits after each
  // chunk, plus a possibly smaller chunk at the end, followed by a FCS.
  size_t left = length;
  size_t i = 0;
  size_t out_index = 0;
  size_t in_index = 0;
  while (left > 0)
  {
    size_t en_chunk = 255;
    if (left < en_chunk)
      en_chunk = left;

    decode_data (encoded + in_index, en_chunk);
    int syndrome = check_syndrome ();
    if (syndrome != 0)
    {
      cerr << "non-zero syndrome in codeword " << i << ": "
        << hex << setw(8) << setfill('0') << syndrome << endl;
      st = correct_errors_erasures (encoded + in_index, en_chunk, 0, NULL);
      if (st != 1)
      {
        cerr << "  -> error correction failed" << endl;
        check_pass = false;
      }
    }
    memcpy (outbuf + out_index, encoded + in_index, en_chunk - NPAR);
    out_index += en_chunk - NPAR; // don't keep parity bits in the output
    in_index += en_chunk;
    left -= en_chunk;
    i++;
  }

  dump_buf (outbuf, out_index, "decoded packet");
  delete[] outbuf;

  return check_pass;
}

int
main (int argc, char *argv[])
{
  int ret = 2;
  int evt;
  bool verbose;
  uint8_t *txbuf = NULL, *tx_enc = NULL;
  size_t txsize, tx_encsize;
  fcs_t crc16;
  unsigned int crc_offset;
  std::string blah;

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
  bin.close ();

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
  prussdrv_open (PRU_EVTOUT_1);

  // map interrupts
  prussdrv_pruintc_init(&interrupts);

  // Map PRU memory
  if (prussdrv_map_prumem (PRUSS0_PRU0_DATARAM, (void **)&pru0_data) < 0
      || prussdrv_map_prumem (PRUSS0_PRU1_DATARAM, (void **)&pru1_data) < 0
      || prussdrv_map_prumem (PRUSS0_SHARED_DATARAM, (void **)&pru_shared) < 0)
  {
    cerr << "failed to map PRU memory!" << endl;
    goto done;
  }

  // Final tx packet length
  txsize = sizeof(circls_tx_hdr_t) + data.len + sizeof(fcs_t);
  txbuf = new uint8_t[txsize];

  // Encoded packet length with parity bytes
  tx_encsize = txsize / 255;
  if ((txsize % 255) != 0)
    tx_encsize++;
  tx_encsize = txsize + (tx_encsize * NPAR);
  tx_enc = new uint8_t[tx_encsize];

  /* Inject the tx header.  */
  make_tx_header ((circls_tx_hdr_t *)txbuf, data);

  /* Copy the data.  */
  memcpy (txbuf + sizeof(circls_tx_hdr_t), data.mem, data.len);

  /* Compute FCS (CRC16) for header+data.  */
  crc_offset = sizeof(circls_tx_hdr_t) + data.len;
  crc16 = crc_ccitt (txbuf, crc_offset);
  *(fcs_t *)(txbuf + crc_offset) = crc16;

  if (verbose)
    dump_buf (txbuf, txsize, "raw packet");

  /*  Encode the entire packet using RS.  */
  encode_rs (tx_enc, txbuf, txsize);

  /* Pass encoded packet size to PRU for xmit.  */
  pwm_options.data_len = tx_encsize;

  if (verbose)
  {
    dump_buf (tx_enc, tx_encsize, "encoded packet");
    // Sanity check, don't continue if we fail
    if (!decode_rs_check (tx_enc, tx_encsize))
    {
      cerr << "sanity check failed!!!" << endl;
      goto done;
    }
  }

  if (tx_encsize > MAX_PACKET)
  {
    cerr << "encoded packet size too large to transmit" << endl;
    goto done;
  }

  /* Copy tx info into PRU memory */
  memcpy (pru0_data, &pwm_options, sizeof(pwm_options));

  /* Copy the encoded packet into PRU memory.  */
  memcpy (pru0_data + 1, tx_enc, tx_encsize);

  /* Wait for user input, to give a chance to hook into the debugger (?) */
  cout << "press enter to continue..." << endl;
  getline (cin, blah);

  // Load and execute binary on PRU0
  if (prussdrv_exec_program (0, PRU0_PROG) < 0)
  {
    cerr << "failed to exec PRU0 program!" << endl;
    goto done;
  }

  // Load and execute binary on PRU1
  if (prussdrv_exec_program (1, PRU1_PROG) < 0)
  {
    cerr << "failed to exec PRU1 program!" << endl;
    goto done;
  }

  // Wait for interrupt from user
  run = true;
  signal (SIGINT,  handle_interrupt);
  signal (SIGQUIT, handle_interrupt);

  // Let the PRUs actually start
  pru_shared->flags.start = 1;

  // Wait for finish interrupt from PRU 0
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
  if (tx_enc)
    delete[] tx_enc;

  exit(ret);
}
