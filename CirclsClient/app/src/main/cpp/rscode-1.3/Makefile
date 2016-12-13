# Makefile for Cross Interleaved Reed Solomon encoder/decoder
#
# (c) Henry Minsky, Universal Access 1991-1996
#

RANLIB = ranlib
AR = ar


VERSION = 1.0
DIRNAME= rscode-$(VERSION)


CC = gcc
# OPTIMIZE_FLAGS = -O69
DEBUG_FLAGS = -g
CFLAGS = -Wall -Wstrict-prototypes  $(OPTIMIZE_FLAGS) $(DEBUG_FLAGS) -I.. -fPIC
LDFLAGS = $(OPTIMIZE_FLAGS) $(DEBUG_FLAGS)

LIB_CSRC = rs.c galois.c berlekamp.c crcgen.c 
LIB_HSRC = ecc.h
LIB_OBJS = rs.o galois.o berlekamp.o crcgen.o 

STATIC_LIB = libecc.a
DYNAMIC_LIB = libecc.so
TEST_PROGS = example

TARGETS = $(TARGET_LIB) $(TEST_PROGS)

all: $(TARGETS)

$(STATIC_LIB): $(LIB_OBJS)
	$(RM) $@
	$(AR) cq $@ $(LIB_OBJS)
	if [ "$(RANLIB)" ]; then $(RANLIB) $@; fi

$(DYNAMIC_LIB): $(LIB_OBJS)
	$(RM) $@
	$(LD) -shared -o $@ $^

example: example.o galois.o berlekamp.o crcgen.o rs.o $(DYNAMIC_LIB)
	gcc -o example example.o -L. -lecc

clean:
	rm -f *.o example libecc.a
	rm -f *~

dist:
	(cd ..; tar -cvf rscode-$(VERSION).tar $(DIRNAME))

depend:
	makedepend $(SRCS)

# DO NOT DELETE THIS LINE -- make depend depends on it.

