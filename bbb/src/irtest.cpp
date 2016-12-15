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

  pru_ir_info_t buf;
  memset (&buf, 0, sizeof(buf));
  while (true)
  {
    while (pru_data->flags == 0)
      sched_yield ();
    memcpy (&buf, (void *)pru_data, sizeof(buf));
    pru_data->flags = 0;
    cout << "[" << setw(3) << setfill(' ') << evt++ << ","
      << setw(3) << setfill(' ') << (int)buf.count
      << "] 0x" << hex << setw(4) << setfill('0')
      << (uint16_t)*((uint16_t*)&buf.frame) << endl;
  }

done:
  /* Disable PRU.  */
  prussdrv_pru_disable (PRU_NUM);
  prussdrv_exit ();

  exit(0);
}
