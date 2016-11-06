/* ledtest.cpp
 * Fritz Reese (C) 2016
 *
 * Simple command line program to test RGB LED outputs.
 *
 * Support for constant output on GPIO pins 21-23 when you define LED_GPIO,
 * or PWM outputs with variable intensity on pins 16, 21, 22 with LED_PWM.
 *
 */
#include "sysfs.h"
#include "GPIO.h"
#include "PWM.h"

#include <unistd.h>
#include <getopt.h>

#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <iterator>
#include <algorithm>

#if defined(LED_GPIO) && !defined(LED_PWM)
#define LEDR_GPIO 2  /* P9_22 */
#define LEDG_GPIO 49 /* P9_23 */
#define LEDB_GPIO 3  /* P9_21 */
#define LED GPIO /* typedef GPIO LED */
#else // if defined(LED_PWM)
#define LEDR_PWM_MOD 0 /* P9_21 - EHRPWM0B */
#define LEDR_PWM_SUB 1
#define LEDB_PWM_MOD 1 /* P9_16 - EHRPWM1B */
#define LEDB_PWM_SUB 1
#define LEDG_PWM_MOD 0 /* P9_22 - EHRPWM0A */
#define LEDG_PWM_SUB 0
#define LED PWM /* typedef PWM LED */
#endif

#define LEDR 0
#define LEDG 1
#define LEDB 2

#define NLEDS 3

using namespace std;
using namespace bbb;

static void
usage (const char *prog, const char *msg="") __attribute__((noreturn));

static void
usage (const char *prog, const char *msg)
{
  cerr << "usage: " << prog << " [OPTIONS]" << endl;
  cerr << "Command-line interface from which you can control LED";
  cerr << " outputs. Commands are:" << endl;
  cerr << "r|g|b|w [duty]" << endl;
  cerr << "Where r, g, and b control red, green, and blue LEDs independently,"
          " and w controls all LEDs together." << endl;
  cerr << "The 'duty' is a percentage and should be given as an integer"
          " between 0 and 100." << endl;
  cerr << endl << msg << endl;
  exit(2);
}

/* Do a command on a single LED. For PWM pins this is just an optional
 * duty cycle percentage (the period is fixed at something low like 1kHz).  */
static void
doLEDCommand (LED *led, vector<string> &args)
{
#if defined(LED_GPIO) && !defined(LED_PWM)
  cout << "* toggle " << args[0][0] << endl;
  led->toggleOutput ();
#else // LED_PWM
  // Update duty cycle if given
  int dutyPercent = -1;
  if (args.size () > 1 && !args[1].empty ())
  {
    istringstream s (args[1]);
    s >> dutyPercent;
    if (dutyPercent < 0 || dutyPercent > 100)
      cerr << "ignoring bad duty cycle '" << args[1] << "'" << endl;
    else if (dutyPercent != 0)
    {
      led->setDutyCycle ((float)dutyPercent);
      led->run ();
    }
    else
      led->stop ();
  }

  // No args given, just toggle the run state
  else
  {
    // Toggle output, or don't
    if (led->isRunning ())
      led->stop ();
    else
      led->run ();
  }
#endif // LED_PWM

}

int
main (int argc, char *argv[])
{
  if (getuid () != 0)
    usage (argv[0], "must be root");

  cerr << "Usage: r|g|b|w";

#if defined(LED_GPIO) && !defined(LED_PWM)
  cerr << endl;

  GPIO led_r(LEDR_GPIO, GPIO::OUTPUT);
  GPIO led_g(LEDG_GPIO, GPIO::OUTPUT);
  GPIO led_b(LEDB_GPIO, GPIO::OUTPUT);

#else /* LED_PWM */
  cerr << " [duty]" << endl;

  /* The red and green LEDs are in the same module, so we need to use the
   * PWMModule wrapper to handle them.  */
  PWMModule led_pair = PWMModule(LEDR_PWM_MOD);
  PWM &led_r = led_pair.getSubmodule(LEDR_PWM_SUB);
  PWM &led_g = led_pair.getSubmodule(LEDG_PWM_SUB);
  PWM led_b  = PWM(LEDB_PWM_MOD, LEDB_PWM_SUB);

#endif

  LED *leds[] = {
    &led_r, &led_g, &led_b
  };

  /* Command-loop.  */
  int idx = -1;
  int last;
  while (!cin.bad () && !cin.eof ())
  {
    string line;
    getline (cin, line);

    /* Split args into words.  */
    vector<string> args;
    istringstream iss (line);
    copy (istream_iterator<string> (iss),
          istream_iterator<string> (),
          back_inserter (args));

    idx = -1;
    switch (line[0])
    {
      case 'r': idx = LEDR; last = idx+1; break;
      case 'g': idx = LEDG; last = idx+1; break;
      case 'b': idx = LEDB; last = idx+1; break;
      case 'w': idx = 0; last = NLEDS; break;
      default: break;
    }

    /* Delegate the command, passing the chosen LED(s).  */
    for (; idx > 0 && idx < last; idx++)
      doLEDCommand (leds[idx], args);
  }
  return 0;
}
