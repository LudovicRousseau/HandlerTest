#! /usr/bin/env python

"""
#    validation.py: perform exchanges with all APDU sizes possible
#    Copyright (C) 2014-2016  Ludovic Rousseau
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
"""

from smartcard.System import readers
from smartcard.util import toBytes, toHexString
from smartcard.CardConnectionObserver import ConsoleCardConnectionObserver
from smartcard.CardConnection import CardConnection


def print_error(text):
    RED = '\033[01;31m'
    NORMAL = '\033[0m'
    print RED + text + NORMAL


class Validation(object):
    def __init__(self, reader, extended=False, debug=False,
            protocol=None, combi=False):
        self.reader = reader

        # Begining Of Line
        import curses
        curses.setupterm()
        self.BOL = curses.tigetstr("cr") + curses.tigetstr("cuu1")

        # connect to the reader
        self.connection = reader.createConnection()

        # create an observer to get debug
        if debug:
            observer = ConsoleCardConnectionObserver()
            self.connection.addObserver(observer)
            self.BOL = ""

        # connect using the selected protocol (if any)
        self.connection.connect(protocol=protocol)

        # get the ATR
        self.ATR = self.connection.getATR()

        # display used protocol
        protocols = {
                CardConnection.T0_protocol: "T=0",
                CardConnection.T1_protocol: "T=1"
                }
        print "Using protocol:", protocols[self.connection.getProtocol()]

        # extended APDU
        self.extended = extended

        # select APDU
        if not extended:
            if combi:
                SELECT = toBytes("00 A4 04 00 06 A0 00 00 00 18 50")
            else:
                SELECT = toBytes("00 A4 04 00 06 A0 00 00 00 18 FF")
            expected = [[], 0x90, 0x00]
            self.transmitAndCompare(SELECT, expected)

    def transmitAndCompare(self, apdu, expected):
        r_data, r_sw1, r_sw2 = self.connection.transmit(apdu)
        e_data, e_sw1, e_sw2 = expected
        if r_sw1 != e_sw1 or r_sw2 != e_sw2:
            print_error("Wrong SW: %02X %02X instead of %02X %02X" %
                    (r_sw1, r_sw2, e_sw1, e_sw2))
            raise Exception("Fail!")
        if r_data != e_data:
            print_error("Wrong data")
            print "Received:", r_data
            print "Expected:", e_data
            raise Exception("Fail!")

    def case_1(self, full):
        # no data exchanged
        # >  80 30 00 00 00
        # <  []  90 0
        CASE_1 = toBytes("80 30 00 00 00")

        print "Case 1"

        expected = ([], 0x90, 0x00)
        self.transmitAndCompare(CASE_1, expected)

    def case_2(self, full):
        if self.extended:
            self.case_2e(full)
        else:
            self.case_2s(full)

    def case_2s(self, full):
        # gets (1 to 256) data from the card
        # >  80 34 00 07 07
        # <  00 01 02 03 04 05 06 90 0
        CASE_2 = toBytes("80 34 00 00 00")

        print "Case 2 short"
        print

        end = 256
        if full:
            start = 1
        else:
            start = end

        for length in range(start, end + 1):
            print self.BOL, "length:", length
            APDU = list(CASE_2)
            APDU[2] = (length & 0xFF00) >> 8
            APDU[3] = length & 0x00FF
            APDU[4] = length & 0x00FF

            expected = ([i for i in range(0, length)], 0x90, 0x00)
            self.transmitAndCompare(APDU, expected)

    def case_2e(self, full):
        # gets (1 to 256) data from the card
        # >  80 00 04 2A 00 00 07
        # <  2A 2A 2A 2A 2A 2A 2A 90 0
        magic_value = 42
        CASE_2 = toBytes("80 00 04 00 00 00 00")
        CASE_2[3] = magic_value

        print "Case 2 extended"
        print

        # ATRs of the Athena test cards
        Athena_ATRs = ["3B D6 18 00 80 B1 80 6D 1F 03 80 51 00 61 10 30 9E",
                       "3F 96 18 80 01 80 51 00 61 10 30 9F"]

        if toHexString(self.ATR) not in Athena_ATRs:
            print_error("Wrong card inserted!")
            print "Got ATR:", toHexString(self.ATR)
            return

        step = 100
        end = 65535
        if full:
            start = 1
        else:
            start = end

        for length in range(start, end + 1, step):
            print self.BOL, "length:", length
            APDU = list(CASE_2)
            APDU[5] = (length & 0xFF00) >> 8
            APDU[6] = length & 0x00FF

            expected = ([magic_value] * length, 0x90, 0x00)
            self.transmitAndCompare(APDU, expected)

    def case_3(self, full):
        if self.extended:
            self.case_3e(full)
        else:
            self.case_3s(full)

    def case_3s(self, full):
        # send data to the card
        # >  80 32 00 00 07 00 01 02 03 04 05 06
        # <  []  90 0
        CASE_3 = toBytes("80 32 00 00 00")

        print "Case 3 short"
        print

        end = 255
        if full:
            start = 1
        else:
            start = end

        expected = ([], 0x90, 0x00)
        for length in range(start, end + 1):
            print self.BOL, "length:", length
            APDU = list(CASE_3)
            APDU[4] = length
            APDU += [i for i in range(0, length)]

            self.transmitAndCompare(APDU, expected)

    def case_3e(self, full):
        # send data to the card
        # >  80 12 01 80 00 00 07 00 01 02 03 04 05 06
        # <  []  90 0
        CASE_3 = toBytes("80 12 01 80 00 00 00")

        print "Case 3 extended"
        print

        if toHexString(self.ATR) != "3B D6 18 00 81 B1 80 7D 1F 03 80 51 00 61 10 30 8F":
            print_error("Wrong card inserted!")
            print "Got ATR:", toHexString(self.ATR)
            return

        end = 65535
        step = 100
        if full:
            start = 1
        else:
            start = end

        expected = ([], 0x90, 0x00)
        for length in range(start, end + 1, step):
            print self.BOL, "length:", length
            APDU = list(CASE_3)
            APDU[5] = (length & 0xFF00) >> 8
            APDU[6] = length & 0x00FF
            APDU += [i for i in range(0, length)]

            self.transmitAndCompare(APDU, expected)

    def case_4(self, full, apdu):
        # send data to the card and get response
        # mode APDU (T=1)
        # >  80 36 00 09 08 00 01 02 03 04 05 06 07
        # <  00 01 02 03 04 05 06 07 08 90 0
        # mode TPDU (T=0)
        # >  80 36 00 09 08 00 01 02 03 04 05 06 07
        # <  []  61 9
        # >  80 C0 00 00 09
        # <  00 01 02 03 04 05 06 07 08 90 0
        CASE_2 = toBytes("80 36 00 00 00")

        print "Case 4"
        print

        end = 255
        if full:
            start = 1
        else:
            start = 255

        for length_in in range(start, end + 1):
            print self.BOL, "length:", length_in
            length_out = length_in + 1
            APDU = list(CASE_2)
            APDU[2] = (length_out & 0xFF00) >> 8
            APDU[3] = (length_out & 0x00FF)
            APDU[4] = length_in
            APDU += [i for i in range(0, length_in)]

            if apdu:
                expected = ([i for i in range(0, length_out)], 0x90, 0x00)
                self.transmitAndCompare(APDU, expected)
            else:
                expected = ([], 0x61, length_out & 0xFF)
                self.transmitAndCompare(APDU, expected)

                GET_RESPONSE = toBytes("80 C0 00 00 00")
                GET_RESPONSE[4] = length_out & 0xFF

                expected = ([i for i in range(0, length_out)], 0x90, 0x00)
                self.transmitAndCompare(GET_RESPONSE, expected)

    def time_extension(self, extension):
        # time extension
        TIME = toBytes("80 38 00 00")
        TIME[3] = extension

        expected = ([], 0x90, 0x00)
        self.transmitAndCompare(TIME, expected)


def usage(command):
    HELP = """ Possible arguments:
  -1: test APDU Case 1
  -2: test APDU Case 2
  -3: test APDU Case 3
  -4: test APDU Case 4
  -e: test extended APDU (using PC/SC test cards)
  -f: test APDU with every possible lengths
  -r: reader index. By default the first reader is used
  -a: use APDU
  -c: use combi card
  -d: debug mode
  -Z: force use T=1
  -z: force use T=0
  -t val: use val as time request value"""
    print "Usage: %s [arguments]" % command
    print HELP

if __name__ == "__main__":
    import sys
    import getopt

    optlist, args = getopt.getopt(sys.argv[1:], "1234r:ft:acedhZz")

    case_1 = False
    case_2 = False
    case_3 = False
    case_4 = False
    full = False
    apdu = False
    time_extension = False
    extended = False
    combi = False
    debug = False
    protocol = None

    for o, a in optlist:
        if o == "-1":
            case_1 = True
        elif o == "-2":
            case_2 = True
        elif o == "-3":
            case_3 = True
        elif o == "-4":
            case_4 = True
        elif o == "-r":
            reader_index = int(a)
        elif o == "-f":
            full = True
        elif o == "-a":
            apdu = True
        elif o == "-t":
            time_extension = True
            extension = int(a)
        elif o == "-c":
            combi = True
        elif o == "-e":
            extended = True
        elif o == "-d":
            debug = True
        elif o == "-Z":
            protocol = CardConnection.T1_protocol
        elif o == "-z":
            protocol = CardConnection.T0_protocol
        elif o == "-h":
            usage(sys.argv[0])
            sys.exit()

    readers = readers()
    try:
        reader = readers[reader_index]
    except:
        reader = readers[0]
    print "Using reader:", reader

    validation = Validation(reader, extended=extended, debug=debug,
            protocol=protocol, combi=combi)
    if case_1:
        validation.case_1(full)
    if case_2:
        validation.case_2(full)
    if case_3:
        validation.case_3(full)
    if case_4:
        validation.case_4(full, apdu)
    if time_extension:
        validation.time_extension(extension)
