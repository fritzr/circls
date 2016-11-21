C ?= g++
LD ?= ld

SHARED_OUT=libmd5.so
OBJS=md5.o

CPPFLAGS+=
CFLAGS_DEBUG:=$(CFLAGS) -Wall -fPIC -g
CFLAGS+=-Wall -fPIC -O3
LDFLAGS +=

all: $(SHARED_OUT)

debug: CFLAGS=$(CFLAGS_DEBUG)
debug: clean $(SHARED_OUT)

test: CFLAGS_DEBUG+=-L. -Wl,-rpath,.
test: LDLIBS+=-lmd5
test: md5main.c $(SHARED_OUT)
	$(C) -o $@ $< $(CPPFLAGS) $(CFLAGS_DEBUG) $(LDFLAGS) $(LDLIBS)

.PHONY: all debug

$(SHARED_OUT): $(OBJS)
	$(LD) -shared $(LDFLAGS) $< -o $@

$(OBJS): %.o: %.c %.h
	$(CC) -o $@ $(CPPFLAGS) $(CFLAGS) -c $<


clean:
	rm -f $(SHARED_OUT) $(OBJS)
