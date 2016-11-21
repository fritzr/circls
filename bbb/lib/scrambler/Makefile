CXX ?= g++
LD ?= ld

SHARED_OUT=libscrambler.so
OBJS=scrambler.o

CPPFLAGS+=
CXXFLAGS_DEBUG:=$(CXXFLAGS) -Wall -fPIC -g
CXXFLAGS+=-Wall -fPIC -O3
LDFLAGS +=

all: $(SHARED_OUT)

debug: CXXFLAGS=$(CXXFLAGS_DEBUG)
debug: clean $(SHARED_OUT)

test: CXXFLAGS_DEBUG+=-L. -Wl,-rpath,.
test: LDLIBS+=-lscrambler
test: test.cc $(SHARED_OUT)
	$(CXX) -o $@ $< $(CPPFLAGS) $(CXXFLAGS_DEBUG) $(LDFLAGS) $(LDLIBS)

.PHONY: all debug

$(SHARED_OUT): $(OBJS)
	$(LD) -shared $(LDFLAGS) $< -o $@

$(OBJS): %.o: %.cc %.h
	$(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) -c $<


clean:
	rm -f $(SHARED_OUT) $(OBJS)
