/*
 * PWM.h  Created on: 29 Apr 2014
 * Copyright (c) 2014 Derek Molloy (www.derekmolloy.ie)
 * Made available for the book "Exploring BeagleBone"
 * See: www.exploringbeaglebone.com
 * Licensed under the EUPL V.1.1
 *
 * This Software is provided to You under the terms of the European
 * Union Public License (the "EUPL") version 1.1 as published by the
 * European Union. Any use of this Software, other than as authorized
 * under this License is strictly prohibited (to the extent such use
 * is covered by a right of the copyright holder of this Software).
 *
 * This Software is provided under the License on an "AS IS" basis and
 * without warranties of any kind concerning the Software, including
 * without limitation merchantability, fitness for a particular purpose,
 * absence of defects or errors, accuracy, and non-infringement of
 * intellectual property rights other than copyright. This disclaimer
 * of warranty is an essential part of the License and a condition for
 * the grant of any rights to this Software.
 *
 * For more details, see http://www.derekmolloy.ie/
 */

#ifndef PWM_H_
#define PWM_H_
#include<string>
#include <vector>
using std::string;
using std::vector;

#include "sysfs.h"

namespace bbb
{

/**
 * @class PWM
 * @brief A class to control a basic PWM output -- you must know the exact sysfs filename
 * for the PWM output.
 */
class PWM : public SysFS
{
public:
  enum POLARITY { ACTIVE_HIGH=0, ACTIVE_LOW=1 };

private:
  float analogFrequency;  //defaults to 100 Hz
  float analogMax;        //defaults to 3.3V

protected:
  /* Dummy PWM object which doesn't actually do anything on initialization. */
  PWM();

  /* Cache of which submodules are running, so we don't need to keep querying
   * the sysfs.  */
  bool _runcache;

  /* Cache of 'last' duty cycles, so they are not forgotten from the user's
   * perspective between run()s on the same submodule.  */
  unsigned int _dutycache;

public:
  /* The first number is which ehrpwm module to use, the second number is which
   * output of the module to use.  For example, ehrpwm0_0 would be PWM(0,0). */
  PWM(int ehrpwmX, int subX);

  virtual int setPeriod(unsigned int period_ns);
  virtual unsigned int getPeriod();
  virtual int setFrequency(float frequency_hz);
  virtual float getFrequency();
  virtual int setDutyCycle(unsigned int duration_ns);
  virtual unsigned int setDutyCycle(float percentage); // returns duty_ns
  virtual unsigned int getDutyCycle();
  virtual float getDutyCyclePercent();

  virtual int setPolarity(PWM::POLARITY);
  virtual void invertPolarity();
  virtual PWM::POLARITY getPolarity();

  virtual void setAnalogFrequency(float frequency_hz) { this->analogFrequency = frequency_hz; }
  virtual int calibrateAnalogMax(float analogMax); //must be between 3.2 and 3.4
  virtual int analogWrite(float voltage);

  virtual int run();
  virtual bool isRunning();
  virtual int stop();

  virtual ~PWM();
private:
  float period_nsToFrequency(unsigned int);
  unsigned int frequencyToPeriod_ns(float);
};

class PWMModule;

/* The PWMSubmodule is a module-aware PWM pin. It knows that certain properties
 * are universal to the module, and must be set through a PWMModule instance.
 */
class PWMSubmodule : public PWM
{
  friend class PWMModule;
private:
  /* We keep a reference to the parent module, so that any properties changed
   * that must be kept global across the module are reserved if they are set
   * through this submodule.  */
  PWMModule *module;

protected:
  /* For use by the PWMModule.  */
  int reallyRun(void) {
    _runcache = true;
    return PWM::run ();
  }
  int reallySetPeriod(unsigned int period_ns) {
    return PWM::setPeriod(period_ns);
  }
  int reallySetFrequency(float frequency_hz) {
    return PWM::setFrequency(frequency_hz);
  }
  int reallySetPolarity(PWM::POLARITY polarity) {
    return PWM::setPolarity(polarity);
  }
  int reallyStop(void) {
    _runcache = false;
    return PWM::stop ();
  }
  unsigned int reallyGetDutyCycle (void) {
    unsigned int duty = PWM::getDutyCycle ();
    if (duty != 0)
      _dutycache = duty;
    return duty;
  }

public:
  PWMSubmodule(PWMModule *mod, unsigned int modnum, unsigned int submod);
  ~PWMSubmodule();

  /*** These properties are common to all sub-modules.  ***/
  int setPeriod(unsigned int period_ns);
  int setFrequency(float frequency_hz);
  int setPolarity(PWM::POLARITY polarity);

  /* Run only this module (independently of others).
   * If exclusive is given, then also disable all other submodules.  */
  int run(void);

  /* Whether a sub-module is running (independently of others).  */
  bool isRunning(void);

  /* Stop this submodule (independently of others).  */
  int stop(void);

  /*** These properties can be unique to each sub-module.  ***/
  unsigned int getDutyCycle();

  int setDutyCycle(unsigned int duration_ns);
};

/* Unfortunately multiple outputs from the same ehrpwm module cannot have
 * different run-states.
 *
 * This class wraps up an ehrpwm module (two output pins) to effectively
 * control them independently.  Though they must have the same period
 * (frequency) and run state, they can have different duty cycles.
 * Therefore we can simulate independence by setting the duty-cycle of one to
 * '0' while the other is enabled.
 */
class PWMModule : public PWM
{
private:
  unsigned int modnum;
  unsigned int nsubs;
  vector<PWMSubmodule *>submods;

  /* Cache of whether the entire module is running.  */
  unsigned int _modulesRunning;

public:
  PWMModule(unsigned int ehrpwmX, unsigned int nsubmods=2);

  ~PWMModule();

  PWM &getSubmodule (unsigned int submod);

  /*** These properties can be set for each submodule separately.  ***/
  int setDutyCycle (unsigned int submod, unsigned int duration_ns);
  unsigned int setDutyCycle (unsigned int submod, float percentage_out_of_100);

  /*** These properties are common to all sub-modules.  ***/
  int setPeriod(unsigned int period_ns);
  int setFrequency(float frequency_hz);
  int setPolarity(PWM::POLARITY polarity);

  /* Run only the given submodule PWM (independently of others).
   * If exclusive is given, then also disable all other submodules.  */
  int run(unsigned int submod, bool exclusive=false);

  /* Whether a sub-module is running (independently of others).  */
  bool isRunning(unsigned int submod);

  /* Stop a single submodule (independently of others).  */
  int stop(unsigned int submod);

  /* Run all PWMs in the module.  */
  int run();

  /* Whether the entire module is in the 'run' state.
   * Doesn't imply anything about each independent sub-module, since they can
   * have a duty cycle of 0 while the module is 'running'.  */
  bool isRunning();

  /* Stop all PWMs.  */
  int stop();
};

} /* namespace bbb */

#endif /* PWM_H_ */
