#PROG_COMMON=sysfs GPIO PWM
PROG_COMMON=
PROG_BIN=ledtest xmit
PRU_BIN=xmit

LEDTEST_COMMON=sysfs GPIO PWM
LEDTEST_DEPS=$(LEDTEST_COMMON:%=$(BUILD)/%.o)

PROG_TARGETS=$(PROG_BIN:%=$(BIN)/%)
PROG_DEPS=$(PROG_COMMON:%=$(BUILD)/%.o)
PROG_SRC=$(PROG_BIN:%=$(SRC)/%.cpp)

PRU_TARGETS=$(PRU_BIN:%=$(BIN)/%.bin)

SRC?=src
BIN?=bin
LIB?=lib
BUILD?=build

INCLUDES=-I$(LIB)/scrambler -I$(LIB)/rscode-1.3
CXX?=g++
CPPFLAGS+=
CXXFLAGS+=-g -Wall -std=c++0x $(INCLUDES)
LDFLAGS+=-L$(BIN)
LDLIBS=

# external library targets
LIBDIRS=$(shell ls -dF $(LIB)/* | grep '/$$')
LIBS=$(LIBDIRS:$(LIB)/%=%)
LIB_TARGETS=$(addprefix lib-, $(LIBS))

all: $(PROG_BIN) $(PRU_TARGETS) $(LIB_TARGETS)

# depend on each lib file under lib/*/, lib-* named after the subdirectory
lib-rscode-1.3: libecc.so
lib-scrambler: libscrambler.so
lib-md5: libmd5.so

# special targets to build apps with different circuits or flags
gpio: CPPFLAGS+=-DLED_GPIO
gpio: LDLIBS+=-lpthread

pwm: CPPFLAGS+=-DLED_PWM

debug: CPPFLAGS+=-DDEBUG

gpio pwm debug: $(PROG_BIN)

$(BIN)/ledtest: $(LEDTEST_DEPS)
$(BIN)/ledtest: LDLIBS+=-lpthread

xmit: LDLIBS+=-lprussdrvd -lm -lecc -lmd5
xmit: lib-rscode-1.3 lib-md5

# specific targets for each library

.PHONY: all dto gpio pwm xmit $(PROG_BIN) $(LIB_TARGETS)

# CD down into 'dto' to build device tree overlays
dto:
	@$(MAKE) --no-print-directory -C $@ \
	  $(shell echo $(MAKECMDGOALS) | cut -d' ' -f2-)

# Build programs
$(PROG_BIN): %: $(BUILD) $(BIN) $(BIN)/%

$(PROG_TARGETS): $(BIN)/%: $(PROG_DEPS) $(BUILD)/%.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(PRU_TARGETS): $(BIN)/%.bin: $(SRC)/%.p
	pasm -b $< $(BIN)/$*

# Build intermediate objects

$(BUILD)/%.o: $(SRC)/%.cpp $(SRC)/%.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -c -o $@

$(BUILD)/%.o: $(SRC)/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -c -o $@

$(BUILD) $(BIN):
	mkdir -p $@

# Build external dependency libraries
$(LIB_TARGETS): lib-%:
	@$(MAKE) -C $(LIB)/$* && cp $(LIB)/$*/$< $(BIN)/$<

clean:
	@rm -rf $(BUILD)

distclean: clean clean-dto
	@rm -rf $(BIN)

clean-dto:
	@$(MAKE) --no-print-directory -C $(@:clean-%=%) clean

# This target exists to silently ignore X from 'make dto X'
%:
	@exit 0
