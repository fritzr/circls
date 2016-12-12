#!/usr/bin/env python
import sys
import struct

if __name__ == '__main__':
  if len(sys.argv) < 3:
    print 'usage: %s <filename> <size> [<value>]' % (sys.argv[0],)
    sys.exit(1)

  if len(sys.argv) > 3:
    val = int(sys.argv[3], 0)
  else:
    val = 0xe4 # 11 10 01 00 ... W G B R

  val = struct.pack('B', (val&0xff))

  length = int(sys.argv[2], 0)
  with open(sys.argv[1], 'w') as f:
    while length > 0:
      f.write(val)
      length = length - 1

