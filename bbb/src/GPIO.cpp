/*
 * GPIO.cpp  Created on: 29 Apr 2014
 * Copyright (c) 2014 Derek Molloy (www.derekmolloy.ie)
 * Made available for the book "Exploring BeagleBone"
 * If you use this code in your work please cite:
 *   Derek Molloy, "Exploring BeagleBone: Tools and Techniques for Building
 *   with Embedded Linux", Wiley, 2014, ISBN:9781118935125.
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

#include "GPIO.h"

#include<iostream>
#include<fstream>
#include<string>
#include<sstream>
#include<cstdlib>
#include<cstdio>
#include<fcntl.h>
#include<unistd.h>
#include<sys/epoll.h>
#include<pthread.h>
using namespace std;

namespace bbb
{

/**
 *
 * @param number The GPIO number for the BBB
 */
GPIO::GPIO(int number, GPIO::DIRECTION dir)
  : SysFS("gpio", number), debounceTime(0)
{
  this->debounceTime = 0;
  this->togglePeriod=100;
  this->toggleNumber=-1; //infinite number
  this->callbackFunction = NULL;
  this->threadRunning = false;

  ostringstream s;
  s << "gpio" << number;
  // need to give Linux time to set up the sysfs structure
  usleep(250000); // 250ms delay
  setDirection(dir);
}

GPIO::~GPIO() {
}

int GPIO::setDirection(GPIO::DIRECTION dir){
   switch(dir){
   case INPUT: return setProperty ("direction", "in");
      break;
   case OUTPUT:return setProperty ("direction", "out");
      break;
   }
   return -1;
}

int GPIO::setValue(GPIO::VALUE value){
   switch(value){
   case GPIO::HIGH: setProperty ("value", "1");
      break;
   case GPIO::LOW: setProperty ("value", "0");
      break;
   }
   return -1;
}

int GPIO::setEdgeType(GPIO::EDGE value){
   switch(value){
   case GPIO::NONE: setProperty ("edge", "none");
      break;
   case GPIO::RISING: setProperty ("edge", "rising");
      break;
   case GPIO::FALLING: setProperty ("edge", "falling");
      break;
   case GPIO::BOTH: setProperty ("edge", "both");
      break;
   }
   return -1;
}

int GPIO::setActiveLow(bool isLow){
   if(isLow) return setProperty ("active_low", "1");
   /*else */ return setProperty ("active_low", "0");
}

int GPIO::setActiveHigh(){
   return this->setActiveLow(false);
}

GPIO::VALUE GPIO::getValue(){
	string input = getProperty ("value");
	if (input == "0") return GPIO::LOW;
	else return GPIO::HIGH;
}

GPIO::DIRECTION GPIO::getDirection(){
	string input = getProperty ("direction");
	if (input == "in") return INPUT;
	else return OUTPUT;
}

GPIO::EDGE GPIO::getEdgeType(){
	string input = getProperty ("edge");
	if (input == "rising") return GPIO::RISING;
	else if (input == "falling") return GPIO::FALLING;
	else if (input == "both") return GPIO::BOTH;
	else return GPIO::NONE;
}

/* TODO
int GPIO::streamOpen(){
	stream.open((dir + modname + "value").c_str());
	return 0;
}
int GPIO::streamWrite(GPIO::VALUE value){
	stream << value << std::flush;
	return 0;
}
int GPIO::streamClose(){
	stream.close();
	return 0;
}
*/

int GPIO::toggleOutput(){
	if ((bool) this->getValue()) this->setValue(GPIO::LOW);
	else this->setValue(GPIO::HIGH);
    return 0;
}

int GPIO::toggleOutput(int time){ return this->toggleOutput(-1, time); }
int GPIO::toggleOutput(int numberOfTimes, int time){
	this->toggleNumber = numberOfTimes;
	this->togglePeriod = time;
	this->threadRunning = true;
    if(pthread_create(&this->thread, NULL, &threadedToggle, static_cast<void*>(this))){
    	perror("GPIO: Failed to create the toggle thread");
    	this->threadRunning = false;
    	return -1;
    }
    return 0;
}

// This thread function is a friend function of the class
void* threadedToggle(void *value){
	GPIO *gpio = static_cast<GPIO*>(value);
	bool isHigh = (bool) gpio->getValue(); //find current value
	while(gpio->threadRunning){
		if (isHigh)	gpio->setValue(GPIO::HIGH);
		else gpio->setValue(GPIO::LOW);
		usleep(gpio->togglePeriod * 500);
		isHigh=!isHigh;
		if(gpio->toggleNumber>0) gpio->toggleNumber--;
		if(gpio->toggleNumber==0) gpio->threadRunning=false;
	}
	return 0;
}

// Blocking Poll - based on the epoll socket code in the epoll man page
int GPIO::waitForEdge(){
	this->setDirection(INPUT); // must be an input pin to poll its value
	int fd, i, epollfd, count=0;
	struct epoll_event ev;
	epollfd = epoll_create(1);
    if (epollfd == -1) {
	   perror("GPIO: Failed to create epollfd");
	   return -1;
    }

    fd = open((dir + modname + "value").c_str(), O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
       perror("GPIO: Failed to open file");
       return -1;
    }

    //ev.events = read operation | edge triggered | urgent data
    ev.events = EPOLLIN | EPOLLET | EPOLLPRI;
    ev.data.fd = fd;  // attach the file file descriptor

    //Register the file descriptor on the epoll instance, see: man epoll_ctl
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
       perror("GPIO: Failed to add control interface");
       return -1;
    }
	while(count<=1){  // ignore the first trigger
		i = epoll_wait(epollfd, &ev, 1, -1);
		if (i==-1){
			perror("GPIO: Poll Wait fail");
			count=5; // terminate loop
		}
		else {
			count++; // count the triggers up
		}
	}
    close(fd);
    if (count==5) return -1;
	return 0;
}

// This thread function is a friend function of the class
void* threadedPoll(void *value){
	GPIO *gpio = static_cast<GPIO*>(value);
	while(gpio->threadRunning){
		gpio->callbackFunction(gpio->waitForEdge());
		usleep(gpio->debounceTime * 1000);
	}
	return 0;
}

int GPIO::waitForEdge(CallbackType callback){
	this->threadRunning = true;
	this->callbackFunction = callback;
    // create the thread, pass the reference, address of the function and data
    if(pthread_create(&this->thread, NULL, &threadedPoll, static_cast<void*>(this))){
    	perror("GPIO: Failed to create the poll thread");
    	this->threadRunning = false;
    	return -1;
    }
    return 0;
}

} /* namespace bbb */
