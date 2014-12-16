#!/usr/bin/env python

import sys
import struct
import argparse
import usb # pyusb

parser = argparse.ArgumentParser()
parser.add_argument("reqtype", help="read or write")
parser.add_argument("reqname", help="name of request to make")
parser.add_argument("argument", help="For writes, an output argument", nargs='+', default=-1, type=int)
args = parser.parse_args()

note_c = 261
note_d = 294
note_e = 329
note_f = 349
note_g = 392
note_a = 440
note_b = 493
note_uc = 523
tune = [
        [ note_c, 500 ],
        [ note_d, 500 ],
        [ note_e, 500 ],
        [ note_f, 500 ],
        [ note_g, 500 ],
        [ note_a, 500 ],
        [ note_b, 500 ],
        [ note_uc, 500 ],
        [ note_b, 500 ],
        [ note_a, 500 ],
        [ note_g, 500 ],
        [ note_f, 500 ],
        [ note_e, 500 ],
        [ note_d, 500 ],
        [ note_c, 500 ],
        ]

write_ids = {
        'output0' : 0,
        'output1' : 1,
        'output2' : 2,
        'output3' : 3,
        'output4' : 4,
        'output5' : 5,
        'runled' : 6,
        'errorled' : 7,
        'piezo' : 8,
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
    if len(sys.argv) < 4:
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

import power

p = power.Power()

if is_read:
    if req_id < 7:
        print >>sys.stderr, "EUNIMPLEMENTED"
        sys.exit(1)
    elif req_id == 7:
        res = p.read_batt_status()
    elif req_id == 8:
        res = p.read_button()
    print repr(res)
else:
    b = not (args.argument == 0)
    if (req_id < 6):
        p.set_output_rail(req_id, b)
    elif (req_id == 6):
        p.set_run_led(b)
    elif (req_id == 7):
        p.set_error_led(b)
    elif (req_id == 8):
        #p.send_piezo(int(sys.argv[3]), int(sys.argv[4]))
        for note in tune:
            print note
            p.send_piezo(note[0], note[1])
    else:
            raise ""
