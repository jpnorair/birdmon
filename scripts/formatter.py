#! python3

# Import an eph columnar data file (CSV) from command line, which has data in
# hex form, and translate it to the natural number units of the GPS-EPH struct.
# Output natural-numbered CSV to stdout.

import csv
import struct
import string

import argparse
import sys
import os






def structify_line(hexline):
    s_tup = struct.unpack("B B B B B B B b H H B b h i h h i h h I I H h i h h i i i h 2B", bytes.fromhex(hexline))
    return s_tup



# ------- Program startup ---------
if __name__ == '__main__':

    # -----------------------------------------------------------------------------------
    # Prepare Inputs.
    # -----------------------------------------------------------------------------------
    parser = argparse.ArgumentParser(description="convert eph hex data into real number list")
    parser.add_argument('ephcsv', nargs=1, help="Ephemeris CSV file.")
    args = parser.parse_args()
    
    if args.ephcsv is not None:
        if len(args.ephcsv) > 0:
            ephcsv = str(args.ephcsv[0])
    
    print(ephcsv)
    
    with open(ephcsv, newline='') as csvfile:
        csvreader = csv.reader(csvfile, delimiter=',', quotechar='"')
        for row in csvreader:
            elements = structify_line(''.join(row))
            print(elements)
            
    
    sys.exit(0)
