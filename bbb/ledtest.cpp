#include <unistd.h>
#include <getopt.h>

#include <iostream>
#include <cstdlib>

#define LEDR_GPIO 2  /* P9_22 */
#define LEDG_GPIO 49 /* P9_23 */
#define LEDB_GPIO 3  /* P9_21 */

#define LEDR 0
#define LEDG 1
#define LEDB 2

#define NLEDS 3

#include "GPIO.h"

using namespace std;
using namespace exploringBB;

struct cfg_t
{
  GPIO_VALUE red;
  GPIO_VALUE green;
  GPIO_VALUE blue;

};

static void
usage (const char *prog, const char *msg="") __attribute__((noreturn));

static void
usage (const char *prog, const char *msg)
{
  cerr << "usage: " << prog << " [OPTIONS]" << endl;
  cerr << "Command-line interface from which you can control LED";
  cerr << " output values. Type 'r', 'g', or 'b' at the command-line";
  cerr << " to toggle the red, blue, or green LEDs independently." << endl;
  cerr << "Type 'w' to toggle all LEDs." << endl;
  cerr << endl << msg << endl;
  exit(2);
}

bool
getOptions (cfg_t &opts, int argc, char *argv[])
{
  int c;
  opts.red   = LOW;
  opts.green = LOW;
  opts.blue  = LOW;
  while ((c = getopt (argc, argv, "rgb")) != -1)
  {
    switch (c)
    {
      case 'r': opts.red   = HIGH; break;
      case 'g': opts.green = HIGH; break;
      case 'b': opts.blue  = HIGH; break;
      default:
        cerr << "unrecognized option: " << (char)c << endl;
        return false;
    }
  }

  return true;
}

int
main (int argc, char *argv[])
{
  if (getuid () != 0)
    usage (argv[0], "must be root");

  cfg_t opts;
  if (!getOptions (opts, argc, argv))
    usage (argv[0], "bad options");

  GPIO leds[] = {
    { LEDR_GPIO, OUTPUT }, /* LEDR */
    { LEDG_GPIO, OUTPUT }, /* LEDG */
    { LEDB_GPIO, OUTPUT }, /* LEDB */
  };

  leds[LEDR] = LOW;
  leds[LEDG] = LOW;
  leds[LEDB] = LOW;

  char c;
  int idx = -1;
  while (!cin.bad () && !cin.eof ())
  {
    cin >> c;
    switch (c)
    {
      case 'r': idx = LEDR; break;
      case 'g': idx = LEDG; break;
      case 'b': idx = LEDB; break;
      case 'w': // 'white', toggle all
        for (idx = 0; idx < NLEDS; idx++)
          leds[idx].toggleOutput ();
        idx = -1;
        break;
      default: break;
    }

    if (idx >= 0 && idx < NLEDS)
    {
      cout << "> toggle " << c << endl;
      leds[idx].toggleOutput ();
      idx = -1;
    }
  }

  return 0;
}
