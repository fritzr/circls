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

using namespace std;

namespace bbb
{

PWM::PWM(int ehrpwmX, int subX)
  : SysFS("pwm",
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
      (2*ehrpwmX) + subX + (ehrpwmX > 0 ? 1 : 0)),
  analogFrequency (1000), analogMax (3.3)
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
  return setProperty ("duty_ns", duty_ns);
}

int PWM::setDutyCycle(float percentage){
  if ((percentage>100.0f)||(percentage<0.0f)) return -1;
  unsigned int period_ns = this->getPeriod();
  float duty_ns = period_ns * (percentage/100.0f);
  this->setDutyCycle((unsigned int) duty_ns );
  return 0;
}

unsigned int PWM::getDutyCycle(){
  istringstream s (getProperty ("duty_ns"));
  int duty_ns;
  s >> duty_ns;
  return duty_ns;
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
  return setProperty ("run", 1);
}

bool PWM::isRunning(){
  string running = getProperty ("run");
  return (running=="1");
}

int PWM::stop(){
  return setProperty ("run", 0);
}

} /* namespace bbb */
