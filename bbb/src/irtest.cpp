/* Run the IR PRU and just dump all received frames */

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

#include "xmit.h"

using namespace std;

#define PRU_NUM 0
#define PRU_RAM PRUSS0_PRU0_DATARAM
//#define PRU_NUM 1
//#define PRU_RAM PRUSS0_PRU1_DATARAM

#define PRU_PROG "./bin/ir.bin"

static volatile pru_ir_info_t     *pru_data;  // start of PRU memory

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

static bool
check_exists (const char *message, const char *filename)
{
  bool ret = true;
  ifstream fil(filename);
  if (!fil.good ())
  {
    cerr << "failed to find " << message << " file ("
      << filename << ")" << endl;
    ret = false;
  }
  fil.close ();
  return ret;
}

static volatile bool run;

static void
handle_interrupt(int signum)
{
  cout << "done" << endl;

  prussdrv_pru_disable (PRU_NUM);
  prussdrv_exit ();
  exit(0);
}

static inline uint8_t
count_ones(uint32_t dword)
{
  uint8_t i;
  uint8_t sum = 0;
  for (i = 0; i < 32; i++)
  {
    sum += dword & 1;
    dword >>= 1;
  }
  return sum;
}

static ir_frame_t
decode_ir (uint32_t *cycles_bits, size_t nwords)
{
  ir_frame_t frame = 0;

  // We use 4 pulses per IR bit, and each pulse is 8 bits,
  // so a single IR word comes out as a 4-byte value. Using our PWM symbol
  // protocol we should see 00 ff ff ff for a 1, and 00 00 00 ff for a 0.
  // We count the number of 1 bits by adding the bytes together.
  for (size_t i = 0; i < nwords; i++)
  {
    frame <<= 1;
    // More ones than zeros, it's a 1
    if (count_ones(*cycles_bits++) > 16)
      frame |= 1;
  }
  return frame;
}

int
main (int argc, char *argv[])
{
  int evt = 1;
  tpruss_intc_initdata interrupts = PRUSS_INTC_INITDATA;

  if (getuid () != 0) {
    cerr << "must be root" << endl;
    exit(1);
  }

  // Make sure PRU programs exist
  if (!check_exists ("PRU", PRU_PROG))
    exit (1);

  // Allocate and initialize memory
  if (prussdrv_init () < 0)
  {
    cerr << "failed to init PRUSS!" << endl;
    goto done;
  }

  // events
  prussdrv_open (PRU_EVTOUT_1);

  // map interrupts
  prussdrv_pruintc_init(&interrupts);

  // Map PRU memory
  pru_data = 0;
  if (prussdrv_map_prumem (PRU_RAM, (void **)&pru_data) < 0
      || pru_data == 0 || pru_data == MAP_FAILED)
  {
    cerr << "failed to map PRU memory!" << endl;
    goto done;
  }

  /* Clear PRU memory  */
  memset ((void *)pru_data, 0, sizeof (pru_ir_info_t));

  // Load and execute binary on PRU
  if (prussdrv_exec_program (PRU_NUM, PRU_PROG) < 0)
  {
    cerr << "failed to exec PRU program!" << endl;
    goto done;
  }

  // Wait for interrupt from user
  run = true;
  signal (SIGINT,  handle_interrupt);
  signal (SIGQUIT, handle_interrupt);

  ir_frame_t ir_frame;
  pru_ir_info_t buf;
  memset (&buf, 0, sizeof(buf));
  while (true)
  {
    while (pru_data->flags == 0)
      sched_yield ();
    // Save a copy of the buffer and do what we want to it, while we reset
    // the PRU copy so it is free to queue up another IR event
    memcpy (&buf, (void *)pru_data, sizeof(buf));
    memset ((void *)pru_data, 0, sizeof (*pru_data));

    cout << "[" << setw(3) << setfill(' ') << evt++ << ","
      << setw(3) << setfill(' ') << (int)buf.count
      << "]:" << endl;
    dump_buf (&buf.cycles[0], sizeof(buf.cycles));

    // Decode the IR frame. We pass it as a 4-byte array, because each bit
    // comes out in 4 bytes = 4 pulses * 8 bits / pulse.
    ir_frame = decode_ir (
        (uint32_t *)&buf.cycles[0], sizeof(buf.cycles)/sizeof(uint32_t));
    cout << "      => 0x" << hex << setw(4) << ir_frame << endl;
  }

done:
  /* Disable PRU.  */
  prussdrv_pru_disable (PRU_NUM);
  prussdrv_exit ();

  exit(0);
}
