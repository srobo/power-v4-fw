#!/usr/bin/env python

import sys
import struct
import argparse
from usb1 import * # In lieu of packages for pyusb, I `pip install libusb1`'d.

parser = argparse.ArgumentParser()
parser.add_argument("reqtype", help="read or write")
parser.add_argument("reqname", help="name of request to make")
parser.add_argument("argument", help="For writes, an output argument", nargs='?', default=-1, type=int)
args = parser.parse_args()

write_ids = {
        'output0' : 0,
        'output1' : 1,
        'output2' : 2,
        'output3' : 3,
        'output4' : 4,
        'output5' : 5,
        'runled' : 6,
        'errorled' : 7,
        }

read_ids = {
        'output0' : 0,
        'output1' : 1,
        'output2' : 2,
        'output3' : 3,
        'output4' : 4,
        'output5' : 5,
        '5vrail' : 6,
        'batt' : 7,
        'button' : 8,
        'fwver' : 9,
        }

if args.reqtype == "read":
    req_map = read_ids
    is_read = True
elif args.reqtype == "write":
    req_map = write_ids
    is_read = False
    if len(sys.argv) != 4:
        print >>sys.stderr, "You need to pass an argument to write"
        sys.exit(1)
else:
    print >>sys.stderr, "Unrecognized request type"
    sys.exit(1)

if args.reqname not in req_map:
    print >>sys.stderr, "\"{0}\" is not a valid request for {1}".format(args.reqname, args.reqtype)
    sys.exit(1)

req_id = req_map[args.reqname]

###############################################################################

ctx = USBContext()
dev = ctx.getByVendorIDAndProductID(0x1bda, 1)

if dev == None:
    print >>sys.stderr, "Could not find power board attached"
    sys.exit(1)

handle = dev.open()

if handle == None:
    print >>sys.stderr, "Could not open power board"
    sys.exit(1)

if is_read:
    ret = handle.controlRead(0x80, 64, 0, req_id, 8)
    a, = struct.unpack("i", ret)
    a = a & 0xFFFF
    print "{0}".format(a)
else:
    handle.controlWrite(0, 64, args.argument, req_id, "")
