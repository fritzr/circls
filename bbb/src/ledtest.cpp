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
doCommand (LED &led, const string &cmd)
{
#if defined(LED_GPIO) && !defined(LED_PWM)
  led.toggleOutput ();
#else // LED_PWM
  // Update duty cycle if given
  int dutyPercent = -1;
  if (!cmd.empty ())
  {
    istringstream s (cmd);
    s >> dutyPercent;
    if (dutyPercent >= 0 && dutyPercent <= 100)
      led.setDutyCycle (((float)dutyPercent)/100.0f);
    else
      cerr << "ignoring bad duty cycle '" << cmd << "'" << endl;
  }

  // Toggle output
  if (led.isRunning ())
    led.stop ();
  else
    led.run ();

#endif
}

int
main (int argc, char *argv[])
{
  if (getuid () != 0)
    usage (argv[0], "must be root");

  LED leds[] = {
#if defined(LED_GPIO) && !defined(LED_PWM)
    { LEDR_GPIO, GPIO::OUTPUT }, /* LEDR */
    { LEDG_GPIO, GPIO::OUTPUT }, /* LEDG */
    { LEDB_GPIO, GPIO::OUTPUT }, /* LEDB */
#else /* LED_PWM */
    { LEDR_PWM_MOD, LEDR_PWM_SUB }, /* LEDR */
    { LEDG_PWM_MOD, LEDG_PWM_SUB }, /* LEDG */
    { LEDB_PWM_MOD, LEDB_PWM_SUB }, /* LEDB */
#endif
  };

  char c;
  int idx = -1;
  /* Command-loop.  */
  while (!cin.bad () && !cin.eof ())
  {
    string line;
    getline (cin, line);
    c = line[0];

    string rest;
    size_t rest_idx = line.find_first_of (' ');
    if (rest_idx != string::npos)
      rest = line.substr (rest_idx);

    switch (c)
    {
      case 'r': idx = LEDR; break;
      case 'g': idx = LEDG; break;
      case 'b': idx = LEDB; break;
      case 'w': // 'white', toggle all
        for (idx = 0; idx < NLEDS; idx++)
          doCommand (leds[idx], rest);
        idx = -1;
        break;
      default: break;
    }

    if (idx >= 0 && idx < NLEDS)
    {
      cout << "> toggle " << c << endl;
      doCommand (leds[idx], rest);
      idx = -1;
    }
  }

  return 0;
}
