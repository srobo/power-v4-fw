#!/usr/bin/env python

import sys
import struct
import argparse
from usb1 import *

parser = argparse.ArgumentParser()
parser.add_argument("reqtype", help="read or write")
parser.add_argument("reqname", help="name of request to make")
parser.add_argument("argument", help="For writes, an output argument", nargs='?', default=-1, type=int)
args = parser.parse_args()

write_ids = {
        'output1' : 0,
        'output2' : 1,
        'output3' : 2,
        'output4' : 3,
        'output5' : 4,
        'output6' : 5,
        'runled' : 6,
        'errorled' : 7,
        }

read_ids = {
        'output1' : 0,
        'output2' : 1,
        'output3' : 2,
        'output4' : 3,
        'output5' : 4,
        'output6' : 5,
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
    ret = handle.controlRead(0x80, 64, req_id, 0, 4)
    a, b = struct.unpack("hh", ret)
    print "{:02X}, {:02X}".format(a, b)
else:
    handle.controlWrite(0, 64, req_id, args.argument, "")
