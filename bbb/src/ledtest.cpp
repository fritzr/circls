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
doLEDCommand (LED *leds[], vector<string> &args)
{
#if defined(LED_GPIO) && !defined(LED_PWM)
  int idx = -1;
  int last = -1;
  switch (line[0])
  {
    case 'r': idx = LEDR; last = idx+1; break;
    case 'g': idx = LEDG; last = idx+1; break;
    case 'b': idx = LEDB; last = idx+1; break;
    case 'w': idx = 0; last = NLEDS; break;
    default: break;
  }

  for (; idx >= 0 && idx < last && idx < NLEDS; idx++)
    leds[idx]->toggleOutput ();

#else // LED_PWM
  // Take duty cycle string: R G B
  float duty[3] = { -1, -1, -1 };

  if (args.size () == 3)
  {
    /* Get the 3 duty cycles */
    for (unsigned int i = 0; i < NLEDS && i < args.size (); i++)
    {
      istringstream s(args[i]);
      s >> duty[i];
      if (duty[i] < 0.0f || duty[i] > 100.0f)
        cerr << "# bad colorspec [" << i << "] '" << args[i] << endl;
    }
  }
  /* With one arg, take it as a hex color */
  else if (args.size () == 1)
  {
    istringstream s(args[0]);
    /* Skip a leading '#' */
    if (s.peek() == '#')
      s.seekg(1);

    unsigned int code;
    s.setf (ios::hex, ios::basefield);
    s >> code;
    if (s.bad ())
    {
      cerr << "# bad hex colorspec" << endl;
      return;
    }
    duty[0] = 100.0f * ((float)((code >> 16) & 0xff)) / 255.0f;
    duty[1] = 100.0f * ((float)((code >>  8) & 0xff)) / 255.0f;
    duty[2] = 100.0f * ((float)( code        & 0xff)) / 255.0f;
#ifdef DEBUG
    cerr << "# hex " << args[0] << " -> [" << std::hex << code << "] "
      << duty[0] << " | " << duty[1] << " | " << duty[2] << endl;
#endif
  }
  else
  {
    cerr << "# bad colorspec" << endl;
    return;
  }

  /* Then pass them to the PWMs  */
  for (unsigned int i = 0; i < NLEDS; i++)
  {
    PWM *led = leds[i];
    if (duty[i] == 0.0f)
      led->stop ();
    else
    {
      led->setDutyCycle (duty[i]);
      led->run ();
    }
  }

#endif // LED_PWM

}

int
main (int argc, char *argv[])
{
  if (getuid () != 0)
    usage (argv[0], "must be root");


#if defined(LED_GPIO) && !defined(LED_PWM)
  cerr << "Usage: r|g|b|w" << endl;

  GPIO led_r(LEDR_GPIO, GPIO::OUTPUT);
  GPIO led_g(LEDG_GPIO, GPIO::OUTPUT);
  GPIO led_b(LEDB_GPIO, GPIO::OUTPUT);

#else /* LED_PWM */
  cerr << "Usage: R<0-100> G<0-100> B<0-100>" << endl;

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
  while (!cin.bad () && !cin.eof ())
  {
    string line;
    getline (cin, line);

    if (cin.bad () || cin.eof ())
      break;

    /* Split args into words.  */
    vector<string> args;
    istringstream iss (line);
    copy (istream_iterator<string> (iss),
          istream_iterator<string> (),
          back_inserter (args));

    /* Delegate.  */
    doLEDCommand (leds, args);
  }

  return 0;
}