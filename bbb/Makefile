PROG_COMMON=sysfs GPIO PWM
PROG_BIN=ledtest

PROG_TARGETS=$(PROG_BIN:%=$(BIN)/%)
PROG_DEPS=$(PROG_COMMON:%=$(BUILD)/%.o)
PROG_SRC=$(PROG_BIN:%=$(SRC)/%.cpp)

SRC?=src
BIN?=bin
BUILD?=build

CXX?=g++
CPPFLAGS+=
CXXFLAGS+=-g -Wall -std=c++0x
LDLIBS=-lpthread

all: $(PROG_BIN)

# special targets to build apps with different circuits (gpio/pwm)
gpio: CPPFLAGS+=-DLED_GPIO
pwm: CPPFLAGS+=-DLED_PWM
gpio pwm: $(PROG_BIN)

.PHONY: all dto gpio pwm $(PROG_BIN)

# CD down into 'dto' to build device tree overlays
dto:
	@$(MAKE) --no-print-directory -C $@ \
	  $(shell echo $(MAKECMDGOALS) | cut -d' ' -f2-)

# Build programs
$(PROG_BIN): %: $(BUILD) $(BUILD)/%.o $(BIN) $(BIN)/%

$(PROG_TARGETS): $(BIN)/%: $(BUILD)/%.o $(PROG_DEPS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Build intermediate objects

$(BUILD)/%.o: $(SRC)/%.cpp $(SRC)/%.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -c -o $@

$(BUILD)/%.o: $(SRC)/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -c -o $@

$(BUILD) $(BIN):
	mkdir -p $@

clean:
	@rm -rf $(BUILD)

distclean: clean clean-dto
	@rm -rf $(BIN)

clean-dto:
	@$(MAKE) --no-print-directory -C $(@:clean-%=%) clean

# This target exists to silently ignore X from 'make dto X'
%:
	@exit 0
