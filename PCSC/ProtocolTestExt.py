#! /usr/bin/env python3

"""
#    validationProtocolTestExt.py.py: perform exchanges with all APDU sizes possible
#    Copyright (C) 2022  Ludovic Rousseau
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

import curses
import sys
import getopt
from smartcard.System import readers
from smartcard.util import toBytes, toHexString
from smartcard.CardConnectionObserver import ConsoleCardConnectionObserver
from smartcard.CardConnection import CardConnection


def print_error(text):
    RED = '\033[01;31m'
    NORMAL = '\033[0m'
    print(RED + text + NORMAL)


class Validation(object):
    def __init__(self, reader, extended=False, debug=False,
            protocol=None, full=False, apdu=False):
        self.reader = reader

        # Begining Of Line
        curses.setupterm()
        BOL = curses.tigetstr("cr") + curses.tigetstr("cuu1")
        self.BOL = BOL.decode('utf-8')

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
        self.protocol = self.connection.getProtocol()
        print("Using protocol:", protocols[self.protocol])

        # Store parameters
        self.extended = extended
        self.full = full
        self.apdu = apdu

        applets_ATR = {
                "3B FF 97 00 00 81 31 FE 43 80 31 80 65 B0 84 66 69 39 12 FF FE 82 90 00 32": { "version": 2, "protocol": 1},
                "3B FF 96 00 00 81 31 FE 43 80 31 80 65 B0 84 66 69 FB 12 FF FE 82 90 00 F1": { "version": 1, "protocol": 1},
                "3B 8F 80 01 80 91 00 31 80 65 B0 84 05 00 25 83 01 90 00 CD": { "version": 2, "protocol": 1},
                "3B 7F 97 00 00 80 31 80 65 B0 84 66 69 39 12 FF FE 82 90 00": { "version": 2, "protocol": 0}
                }

        # select APDU
        SELECT_version = {
                1: "00 A4 04 00 09 A0 00 00 00 66 03 01 B3 01",
                2: "00 A4 04 00 09 A0 00 00 00 66 03 01 B4 01"
                }
        atr = toHexString(self.ATR)
        self.applet = applets_ATR[atr]
        SELECT = toBytes(SELECT_version[self.applet["version"]])
        if self.protocol == CardConnection.T0_protocol:
            expected = [[], 0x61, 0x00]
        else:
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
            print("Received:", toHexString(r_data))
            print("Expected:", toHexString(e_data))
            raise Exception("Fail!")

    def case_1(self):
        # no data exchanged
        # > 00 10 00 00
        # < [] 90 00
        CASE_1 = toBytes("00 10 00 00")

        print("Case 1")

        expected = ([], 0x90, 0x00)
        self.transmitAndCompare(CASE_1, expected)

    def case_2(self):
        if self.extended:
            self.case_2e()
        else:
            self.case_2s()

    def case_2s(self):
        # gets (1 to 256) data from the card
        # >  00 20 00 07 07
        # <  00 01 02 03 04 05 06 90 00
        CASE_2 = toBytes("00 20 00 00 00")
        # 2 bytes expected
        GET_LE = toBytes("00 56 00 00 02")

        print("Case 2 short\n")

        end = 256
        if self.full:
            start = 1
        else:
            start = end

        for length in range(start, end + 1):
            print(self.BOL, "length:", length)
            APDU = list(CASE_2)
            length_low = length & 0xFF
            length_high = length >> 8
            APDU[2] = length_high
            APDU[3] = length_low
            APDU[4] = length_low

            expected = ([i for i in range(0, length)], 0x90, 0x00)
            self.transmitAndCompare(APDU, expected)

            # check Le length
            APDU = list(GET_LE)
            if length == 256 and self.protocol == CardConnection.T0_protocol:
                # Applet bug?
                length_high, length_low = 0x7F, 0xFF
            expected_Le = ([length_high, length_low], 0x90, 0x00)
            self.transmitAndCompare(APDU, expected_Le)

    def case_2e(self):
        # gets (1 to 64k) data from the card
        # >  00 B0 01 42 00 01 42
        # < 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 
        # 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 
        # 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F 
        # 30 31 32 33 34 35 36 37 38 39 3A 3B 3C 3D 3E 3F 
        # 40 41 42 43 44 45 46 47 48 49 4A 4B 4C 4D 4E 4F 
        # 50 51 52 53 54 55 56 57 58 59 5A 5B 5C 5D 5E 5F 
        # 60 61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F 
        # 70 71 72 73 74 75 76 77 78 79 7A 7B 7C 7D 7E 7F 
        # 80 81 82 83 84 85 86 87 88 89 8A 8B 8C 8D 8E 8F 
        # 90 91 92 93 94 95 96 97 98 99 9A 9B 9C 9D 9E 9F 
        # A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA AB AC AD AE AF 
        # B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA BB BC BD BE BF 
        # C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF 
        # D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA DB DC DD DE DF 
        # E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA EB EC ED EE EF 
        # F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA FB FC FD FE FF 
        # 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 
        # 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 
        # 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F 
        # 30 31 32 33 34 35 36 37 38 39 3A 3B 3C 3D 3E 3F 
        # 40 41 90 00 : Normal processing.
        CASE_2E = toBytes("00 B0 00 00 00 00 00")
        GET_LE = toBytes("00 56 00 00")

        print("Case 2 extended\n")

        step = 100
        end = 65535
        if self.full:
            start = 1
        else:
            start = end

        for length in range(start, end + 1, step):
            print(self.BOL, "length:", length)
            APDU = list(CASE_2E)
            length_low = length & 0xFF
            length_high = length >> 8
            APDU[2] = length_high
            APDU[3] = length_low
            APDU[4] = 0
            APDU[5] = length_high
            APDU[6] = length_low

            expected = ([i % 256 for i in range(0, length)], 0x90, 0x00)
            self.transmitAndCompare(APDU, expected)

            # check Le length
            APDU = list(GET_LE)
            expected_Le = ([length_high, length_low], 0x90, 0x00)
            self.transmitAndCompare(APDU, expected_Le)

    def case_3(self):
        if self.extended:
            self.case_3e()
        else:
            self.case_3s()

    def case_3s(self):
        # send data to the card
        # >  00 30 00 00 07 00 01 02 03 04 05 06
        # <  []  90 0
        CASE_3 = toBytes("00 30 00 00 00")
        # expect up to 255 byes
        GET_DATA_IN = toBytes("00 51 00 00 00")

        print("Case 3 short\n")

        end = 255
        if self.full:
            start = 1
        else:
            start = end

        expected = ([], 0x90, 0x00)
        for length in range(start, end + 1):
            print(self.BOL, "length:", length)
            APDU = list(CASE_3)
            APDU[4] = length
            APDU += [i for i in range(0, length)]

            self.transmitAndCompare(APDU, expected)

            # check incoming data
            APDU = list(GET_DATA_IN)
            data = [0] * 255
            for i in range(0, length):
                data[i] = i
            expected_data = (data + [length], 0x90, 0x00)
            self.transmitAndCompare(APDU, expected_data)

    def case_3e(self):
        # send data to the card
        # >  00 D0 00 00 00 00 07 00 01 02 03 04 05 06
        # <  []  90 0
        CASE_3E = toBytes("00 D0 00 00 00 00 00")
        GET_RECEIVED_LENGTH = toBytes("00 54 00 00")
        GET_DATA_IN = toBytes("00 53 00 00")

        print("Case 3 extended\n")

        end = 65535
        step = 100
        if self.full:
            start = 10
        else:
            start = end

        expected = ([], 0x90, 0x00)
        for length in range(start, end + 1, step):
            print(self.BOL, "length:", length)
            APDU = list(CASE_3E)
            length_low = length & 0xFF
            length_high = length >> 8
            APDU[5] = length_high
            APDU[6] = length_low
            APDU += [i for i in range(0, length)]

            self.transmitAndCompare(APDU, expected)

            # check received length
            APDU = list(GET_RECEIVED_LENGTH)
            expected_lc = ([length_high, length_low], 0x90, 0x00)
            self.transmitAndCompare(APDU, expected_lc)

            # check incoming data
            APDU = list(GET_DATA_IN)
            data = [0] * 256
            for i in range(0, min(256, length)):
                data[i] = i
            expected_data = (data, 0x90, 0x00)
            self.transmitAndCompare(APDU, expected_data)

    def case_4(self):
        # send data to the card and get response
        # mode APDU (T=1)
        # >  80 36 00 09 08 00 01 02 03 04 05 06 07
        # <  00 01 02 03 04 05 06 07 08 90 0
        # mode TPDU (T=0)
        # >  80 36 00 09 08 00 01 02 03 04 05 06 07
        # <  []  61 9
        # >  80 C0 00 00 09
        # <  00 01 02 03 04 05 06 07 08 90 0
        CASE_4 = toBytes("00 40 00 00 00")
        GET_DATA_IN = toBytes("00 51 00 00")

        print("Case 4\n")

        end = 255
        if self.full:
            start = 1
        else:
            start = 255

        expected = ([], 0x90, 0x00)
        for length_in in range(start, end + 1):
            print(self.BOL, "length:", length_in)
            length_out = length_in + 1
            length_out_low = length_out & 0xFF
            length_out_high = length_out >> 8
            APDU = list(CASE_4)
            APDU[2] = length_out_high
            APDU[3] = length_out_low
            APDU[4] = length_in
            APDU += [i for i in range(0, length_in)]

            if self.apdu and self.protocol == CardConnection.T1_protocol:
                expected = ([i for i in range(0, length_out)], 0x90, 0x00)
                self.transmitAndCompare(APDU, expected)
            else:
                expected = ([], 0x61, length_out & 0xFF)
                self.transmitAndCompare(APDU, expected)

                GET_RESPONSE = toBytes("00 C0 00 00 00")
                GET_RESPONSE[4] = length_out & 0xFF

                expected = ([i for i in range(0, length_out)], 0x90, 0x00)
                self.transmitAndCompare(GET_RESPONSE, expected)

    def time_extension(self, extension):
        # time extension
        SET_WTX = toBytes("00 50 00 00")
        extension_high = extension >> 8
        extension_low = extension & 0xFF
        SET_WTX[2] = extension_high
        SET_WTX[3] = extension_low

        expected = ([], 0x90, 0x00)
        self.transmitAndCompare(SET_WTX, expected)

        print("Time extension\n")

        CASE_1_WTX = toBytes("00 11 00 00")
        self.transmitAndCompare(CASE_1_WTX, expected)


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
  -d: debug mode
  -Z: force use T=1
  -z: force use T=0
  -t val: use val as time request value"""
    print("Usage: %s [arguments]" % command)
    print(HELP)

if __name__ == "__main__":
    optlist, args = getopt.getopt(sys.argv[1:], "1234r:ft:acedhZz")

    case_1 = False
    case_2 = False
    case_3 = False
    case_4 = False
    full = False
    apdu = False
    time_extension = False
    extended = False
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
    print("Using reader:", reader)

    validation = Validation(reader, extended=extended, debug=debug,
            protocol=protocol, full=full, apdu=apdu)
    if case_1:
        validation.case_1()
    if case_2:
        validation.case_2()
    if case_3:
        validation.case_3()
    if case_4:
        validation.case_4()
    if time_extension:
        validation.time_extension(extension)
