/*
 * GPIO.h  Created on: 29 Apr 2014
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
 *
 * Adapted by Fritz Reese for flexibility.
 */

#ifndef GPIO_H_
#define GPIO_H_

#include <string>
#include <fstream>
using std::string;
using std::ofstream;

#include "sysfs.h"

namespace bbb
{

typedef int (*CallbackType)(int);

class GPIO : public SysFS
{
private:
  int debounceTime;

public:
  enum DIRECTION{ INPUT, OUTPUT };
  enum VALUE{ LOW=0, HIGH=1 };
  enum EDGE{ NONE, RISING, FALLING, BOTH };

  //constructor will export the pin - number is the $PINS number
  GPIO(int number, GPIO::DIRECTION dir=INPUT);

  //destructor will unexport the pin
  virtual ~GPIO();

  // General Input and Output Settings
  virtual int setDirection(GPIO::DIRECTION);
  virtual GPIO::DIRECTION getDirection();
  virtual int setValue(GPIO::VALUE);
  int operator= (GPIO::VALUE val) { return setValue (val); }
  virtual int toggleOutput();
  virtual GPIO::VALUE getValue();
  virtual int setActiveLow(bool isLow=true);  //low=1, high=0
  virtual int setActiveHigh(); //default
  //software debounce input (ms) - default 0
  virtual void setDebounceTime(int time) { this->debounceTime = time; }

  virtual int toggleOutput(int time); //threaded invert output every X ms.
  virtual int toggleOutput(int numberOfTimes, int time);
  virtual void changeToggleTime(int time) { this->togglePeriod = time; }
  virtual void toggleCancel() { this->threadRunning = false; }

  // Advanced INPUT: Detect input edges; threaded and non-threaded
  virtual int setEdgeType(GPIO::EDGE);
  virtual GPIO::EDGE getEdgeType();
  virtual int waitForEdge(); // waits until button is pressed
  virtual int waitForEdge(CallbackType callback); // threaded with callback
  virtual void waitForEdgeCancel() { this->threadRunning = false; }

private:
  pthread_t thread;
  CallbackType callbackFunction;
  bool threadRunning;
  int togglePeriod;  //default 100ms
  int toggleNumber;  //default -1 (infinite)
  friend void* threadedPoll(void *value);
  friend void* threadedToggle(void *value);
}; /* class GPIO */

void* threadedPoll(void *value);
void* threadedToggle(void *value);

} /* namespace bbb */

#endif /* GPIO_H_ */
