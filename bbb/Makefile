PROG_DEPS=GPIO.o
PROG_BIN=ledtest

CXX?=g++
CXXFLAGS+=-g -Wall -std=c++0x
LDLIBS=-lpthread

all: $(PROG_BIN)

.PHONY: all dto

# CD down into 'dto' to build device tree overlays
dto:
	@$(MAKE) --no-print-directory -C $@ \
	  $(shell echo $(MAKECMDGOALS) | cut -d' ' -f2-)

# Build programs
$(PROG_BIN): %: %.o $(PROG_DEPS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Build intermediate objects
%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) $< -c -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -c -o $@

%:
	@exit 0
