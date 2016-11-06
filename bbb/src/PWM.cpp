/*
 * PWM.cpp  Created on: 29 Apr 2014
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

#include "PWM.h"

#include <sstream>
#include <cstring>

using namespace std;

static inline unsigned int
ehrpwm_export(unsigned int mod, unsigned int submod)
{
  /* Here is the mapping of ehrpwm modules to pwm export numbers:
   * 0  ehrpwm0a
   * 1  ehrpwm0b
   * 2  X
   * 3  ehrpwm1a
   * 4  ehrpwm1b
   * 5  ehrpwm2a
   * 6  ehrpwm2b
   * 7  X
   *
   * Note the export number follows the below calculation.
   * */
  return (2*mod) + submod + (mod > 0 ? 1 : 0);
}

namespace bbb
{

PWM::PWM()
  : SysFS()
{
}

PWM::PWM(int ehrpwmX, int subX)
  : SysFS("pwm", ehrpwm_export(ehrpwmX, subX)),
    analogFrequency (1000), analogMax (3.3),
    _runcache(false)
{
  setFrequency (analogFrequency);
  setDutyCycle (50.0f);
}

PWM::~PWM()
{
  stop ();
}

int PWM::setPeriod(unsigned int period_ns) {
  return setProperty ("period_ns", period_ns);
}

unsigned int PWM::getPeriod(){
  int period;
  istringstream s (getProperty ("period_ns"));
  s >> period;
  return period;
}

float PWM::period_nsToFrequency(unsigned int period_ns){
  float period_s = ((float)period_ns)/1000000000;
  return 1.0f/period_s;
}

unsigned int PWM::frequencyToPeriod_ns(float frequency_hz){
  float period_s = 1.0f/frequency_hz;
  return (unsigned int)(period_s*1000000000);
}

int PWM::setFrequency(float frequency_hz){
  return this->setPeriod(this->frequencyToPeriod_ns(frequency_hz));
}

float PWM::getFrequency(){
  return this->period_nsToFrequency(this->getPeriod());
}

int PWM::setDutyCycle(unsigned int duty_ns){
  int ret = setProperty ("duty_ns", duty_ns);
  if (ret >= 0)
    _dutycache = duty_ns;
  return ret;
}

unsigned int PWM::setDutyCycle(float percentage){
  if ((percentage>100.0f)||(percentage<0.0f)) return -1;
  unsigned int period_ns = this->getPeriod();
  float duty_ns = period_ns * (percentage/100.0f);
  this->setDutyCycle((unsigned int) duty_ns );
  return period_ns;
}

unsigned int PWM::getDutyCycle(){
  return _dutycache;
}

float PWM::getDutyCyclePercent(){
  unsigned int period_ns = this->getPeriod();
  unsigned int duty_ns = this->getDutyCycle();
  return 100.0f * (float)duty_ns/(float)period_ns;
}

int PWM::setPolarity(PWM::POLARITY polarity){
  return setProperty ("polarity", polarity);
}

void PWM::invertPolarity(){
  if (this->getPolarity()==PWM::ACTIVE_LOW) this->setPolarity(PWM::ACTIVE_HIGH);
  else this->setPolarity(PWM::ACTIVE_LOW);
}

PWM::POLARITY PWM::getPolarity(){
  istringstream s (getProperty ("polarity"));
  int polarity;
  s >> polarity;
  if (polarity == 0) return PWM::ACTIVE_LOW;
  else return PWM::ACTIVE_HIGH;
}

int PWM::calibrateAnalogMax(float analogMax){ //must be between 3.2 and 3.4
  if((analogMax<3.2f) || (analogMax>3.4f)) return -1;
  else this->analogMax = analogMax;
  return 0;
}

int PWM::analogWrite(float voltage){
  if ((voltage<0.0f)||(voltage>3.3f)) return -1;
  this->setFrequency(this->analogFrequency);
  this->setPolarity(PWM::ACTIVE_LOW);
  this->setDutyCycle((100.0f*voltage)/this->analogMax);
  return this->run();
}

int PWM::run(){
  int ret = setProperty ("run", 1);
  _runcache = true;
  return ret;
}

bool PWM::isRunning(){
  return _runcache;
}

int PWM::stop() {
  int ret = setProperty ("run", 0);
  _runcache = false;
  return ret;
}

/*********** PWMSubmodule ************/

PWMSubmodule::PWMSubmodule (PWMModule *mod,
    unsigned int modnum, unsigned int submod)
  : PWM(modnum, submod), module(mod)
{
}

PWMSubmodule::~PWMSubmodule ()
{
}

int PWMSubmodule::
setPeriod (unsigned int period_ns)
{
  return module->setPeriod (period_ns);
}

int PWMSubmodule::
setFrequency (float frequency_hz)
{
  return module->setFrequency (frequency_hz);
}

int PWMSubmodule::
setPolarity (PWM::POLARITY polarity)
{
  return module->setPolarity (polarity);
}

int PWMSubmodule::
run (void)
{
  int ret = -1;
  if (!module->isRunning ())
    ret = PWM::run ();
  else
    ret = module->run (number, false);
  _runcache = ret >= 0;
  return ret;
}

bool PWMSubmodule::
isRunning (void)
{
  return _runcache;
}

int PWMSubmodule::
stop (void)
{
  int ret = module->stop (number);
  _runcache = !(ret >= 0);
  return ret;
}

unsigned int PWMSubmodule::
getDutyCycle (void)
{
  return _dutycache;
}

int PWMSubmodule::
setDutyCycle (unsigned int duration_ns)
{
  int ret = PWM::setDutyCycle (duration_ns);
  if (ret >= 0 && duration_ns != 0)
    _dutycache = duration_ns;
  else if (duration_ns == 0)
    _runcache = false;
  return ret;
}

/******* PWMModule ******/
#define FOR_EACH_SUB(var) \
  for (unsigned int var = 0 ; var < nsubs; var ++ )

PWMModule::PWMModule (unsigned int ehrpwmX, unsigned int nsubmods)
  : PWM(), modnum(ehrpwmX), nsubs(nsubmods), _modulesRunning(0)
{
  /* List of submodules.  */
  FOR_EACH_SUB(subi) {
    submods.push_back (new PWMSubmodule(this, modnum, subi));
  }
}

PWMModule::~PWMModule ()
{
  FOR_EACH_SUB(subi) {
    delete submods[subi];
  }
  // Prevent SysFS destructor from extra unexport
  number = -1;
}

PWM &PWMModule::
getSubmodule (unsigned int subi)
{
  return *submods[subi];
}

/*** These properties are common to all sub-modules,
 * so when we set it for one it is best to set it for all to keep the
 * sysfs entries in sync with the actual pin state.  ***/

int PWMModule::
setPeriod(unsigned int period_ns)
{
  int ret = 0;
  FOR_EACH_SUB(subi) {
    number = ehrpwm_export (modnum, subi);
    ret += PWM::setPeriod (period_ns);
  }
  return ret;
}

int PWMModule::
setFrequency(float frequency_hz)
{
  int ret = 0;
  FOR_EACH_SUB(subi) {
    number = ehrpwm_export (modnum, subi);
    ret += submods[subi]->setFrequency (frequency_hz);
  }
  return ret;
}

int PWMModule::
setPolarity(PWM::POLARITY polarity)
{
  int ret = 0;
  FOR_EACH_SUB(subi) {
    number = ehrpwm_export (modnum, subi);
    ret += submods[subi]->setPolarity (polarity);
  }
  return ret;
}

int PWMModule::
run (unsigned int submod, bool exclusive)
{
  int ret = -1;
  if (submod >= nsubs)
    return -1;

  PWMSubmodule *sub = submods[submod];
  /* If the module is already running, just set the duty cycle of this submod.
   * Otherwise we have to actually start the submodule.  */
  if (!sub->isRunning ())
  {
    if (isRunning ())
      ret = sub->setDutyCycle (sub->getDutyCycle ());
    else
    {
      /* Set all to 'run' state to keep sysfs consistent, but keep their duty
       * cycles 0 so they aren't inadvertently run.  */
      FOR_EACH_SUB(subi) {
        if (subi != submod)
          submods[subi]->setDutyCycle (0u);
      }
      ret = run ();
      _modulesRunning++;
    }

    /* If we want exclusive running of this submodule, stop all the other ones.
     * We do this after the above so stop() doesn't turn off the module when the
     * last active submodule is disabled.  */
    if (exclusive) {
      FOR_EACH_SUB(subi) {
        stop (subi);
      }
    }
  }

  return ret;
}

bool PWMModule::
isRunning (unsigned int submod)
{
  return (submod < nsubs) ? submods[submod]->isRunning () : false;
}

int PWMModule::
stop (unsigned int submod)
{
  int ret = -1;
  if (submod >= nsubs)
    return -1;

  PWMSubmodule *sub = submods[submod];
  if (sub->isRunning ())
  {
    /* The current duty cycle is already cached, so just set this to 0,
     * effectively stopping the submodule output.  */
    if (_modulesRunning > 1)
    {
      sub->setDutyCycle (0u);
      _modulesRunning--;
    }

    /* When _modulesRunning == 1, we are stopping the last submodule.
     * At this point we can stop the entire module.  */
    else
      stop (); // sets _modulesRunning = 0
  }
  return ret;
}

int PWMModule::
run ()
{
  int ret = 0;
  unsigned int runningCount = 0;
  if (!isRunning ())
  {
    FOR_EACH_SUB(subi) {
      if (submods[subi]->isRunning ())
        runningCount++;
      number = ehrpwm_export (modnum, subi);
      ret += PWM::run ();
    }
  }
  /* Note that some submodules may still have a duty cycle of 0, which means
   * they will not be running after 'run' is set to true.  Their runcache
   * should report this, however.  */
  _modulesRunning = runningCount;
  return ret;
}

bool PWMModule::
isRunning ()
{
  return _modulesRunning > 0;
}

int PWMModule::
stop ()
{
  int ret = 0;
  FOR_EACH_SUB(subi) {
    number = ehrpwm_export (modnum, subi);
    ret += PWM::stop ();
  }

  _modulesRunning = 0;
  return ret;
}

/*** These properties can be unique to each sub-module.  ***/
int PWMModule::
setDutyCycle(unsigned int submod, unsigned int duration_ns)
{
  return (submod < nsubs) ? submods[submod]->setDutyCycle (duration_ns) : -1;
}

unsigned int PWMModule::
setDutyCycle(unsigned int submod, float percentage)
{
  return (submod < nsubs) ? submods[submod]->setDutyCycle (percentage) : -1;
}


} /* namespace bbb */
