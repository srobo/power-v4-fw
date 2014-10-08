#!/usr/bin/env python

import sys
import struct
from usb1 import *

ctx = USBContext()
dev = ctx.getByVendorIDAndProductID(0x1bda, 1)

if dev == None:
    print >>sys.stderr, "Could not find power board attached"
    sys.exit(1)

handle = dev.open()

if handle == None:
    print >>sys.stderr, "Could not open power board"
    sys.exit(1)

handle.controlWrite(0, 64, 0x1234, 0x5678, "")
ret = handle.controlRead(0x80, 64, 0x1234, 0x5678, 4)
a, b = struct.unpack("hh", ret)
print "{:02X}, {:02X}".format(a, b)
