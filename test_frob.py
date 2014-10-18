#!/usr/bin/env python

import sys
import struct
import argparse
import usb # pyusb

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

all_busses = list(usb.busses())
all_devices = []
for b in all_busses:
	all_devices = all_devices + (list(b.devices))
list_of_power_boards = [d for d in all_devices if (d.idVendor == 0x1bda and d.idProduct == 0x10)]

if len(list_of_power_boards) == 0:
	print >>sys.stderr, "No power board found"
	sys.exit(1)
elif len(list_of_power_boards) != 1:
	print >>sys.stderr, "More than one power board found"
	sys.exit(1)

dev = list_of_power_boards[0]
handle = dev.open()

if handle == None:
    print >>sys.stderr, "Could not open power board"
    sys.exit(1)

if is_read:
    ret = handle.controlMsg(0x80, 64, 8, 0, req_id)
    ret = list(ret)
    ret = "".join(map(chr, ret))
    if len(ret) == 4:
        a, = struct.unpack("i", ret)
        print "{0}".format(a)
    elif len(ret) == 8:
        a, b = struct.unpack("ii", ret)
        print "{0} {1}".format(a, b)
    else:
        print >>sys.stderr, "Short read (or otherwise), board returned {0} bytes".format(len(ret))
        sys.exit(1)
else:
    handle.controlMsg(0, 64, 0, args.argument, req_id)
