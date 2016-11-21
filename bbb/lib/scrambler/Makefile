CXX ?= g++
LD ?= ld

SHARED_OUT=libscrambler.so
OBJS=scrambler.o

CPPFLAGS+=
CXXFLAGS+=-Wall -fPIC -O3
LDFLAGS +=

all: $(SHARED_OUT)

.PHONY: all

$(SHARED_OUT): $(OBJS)
	$(LD) -o $@ -shared $(LDFLAGS) $<

$(OBJS): %.o: %.cc %.h
	$(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) -c $<

