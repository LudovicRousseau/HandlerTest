/*
    handler_test.c: main function used for IFDH debug
    Copyright (C) 2001-2004   Ludovic Rousseau

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * $Id$
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <getopt.h>
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#include <PCSC/ifdhandler.h>

#include "debug.h"

#undef CONTACTLESS
#undef COMBI

#define LUN 0
#define ENV_LIBNAME "LIB"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

int handler_test(int lun, int channel, char device_name[]);
char *ifd_error(int rv);
int exchange(char *text, DWORD lun, SCARD_IO_HEADER SendPci,
	PSCARD_IO_HEADER RecvPci,
	UCHAR s[], DWORD s_length,
	UCHAR r[], PDWORD r_length,
	UCHAR e[], int e_length);
int extended_apdu(int lun);
int short_apdu(int lun);

#define DLSYM(func)  f.func = dlsym(lib_handle, "" # func); \
	if (f.func == NULL) { \
	DEBUG2("dlsym: %s", dlerror()); \
	return 1; }

#define PCSC_ERROR(x) printf("%s:%d " x ": %s\n", __FILE__, __LINE__, ifd_error(rv))

struct f_t {
	RESPONSECODE (*IFDHCreateChannel)(DWORD, DWORD);
	RESPONSECODE (*IFDHCreateChannelByName)(DWORD, LPSTR);
	RESPONSECODE (*IFDHCloseChannel)(DWORD);
	//RESPONSECODE IFDHGetCapabilities ( DWORD, DWORD, PDWORD, PUCHAR );
	//RESPONSECODE IFDHSetCapabilities ( DWORD, DWORD, DWORD, PUCHAR );
	RESPONSECODE (*IFDHSetProtocolParameters)(DWORD, DWORD, UCHAR, UCHAR,
		UCHAR, UCHAR);
	RESPONSECODE (*IFDHPowerICC)(DWORD, DWORD, PUCHAR, PDWORD);
	RESPONSECODE (*IFDHTransmitToICC)(DWORD, SCARD_IO_HEADER, PUCHAR, 
	  			   DWORD, PUCHAR, PDWORD, 
	  			   PSCARD_IO_HEADER);
	RESPONSECODE (*IFDHControl)(DWORD, DWORD, PUCHAR, DWORD, PUCHAR, DWORD, PDWORD);
	RESPONSECODE (*IFDHICCPresence)(DWORD);
	int version;
};

struct f_t f = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0 };

#define CASE1 (1)
#define CASE2 (1<<1)
#define CASE3 (1<<2)
#define CASE4 (1<<3)

/* flags */
char full = FALSE;
int timerequest = -1;
char cases = 0;
char tpdu = FALSE;
char apdu = FALSE;
char t1 = FALSE;
char stop_on_error = TRUE;
char extended = FALSE;

/* getopt(3) */
extern char *optarg;
extern int optind, opterr, optopt;


void help(char *argv0)
{
	printf("\nUsage: %s [-f] [-t val] [-1] [-2] [-3] [-4] [-A] [-T] [-l libname] [-c channel] [-d device_name]\n", argv0);
	printf("  -f : test APDU with every possible lengths\n");
	printf("  -t val : use val as timerequest value. Set to 0 to avoid test\n");
	printf("  -1 : test CASE 1 APDU\n");
	printf("  -2 : test CASE 2 APDU\n");
	printf("  -3 : test CASE 3 APDU\n");
	printf("  -4 : test CASE 4 APDU\n");
	printf("  -e : test extended APDU\n");
	printf("  -A : use APDU\n");
	printf("  -T : use TPDU\n");
	printf("  -Z : use T=1 instead of default T=0\n");
	printf("  -n : non stop, do not stop on the first error\n");
	printf("  -l libname : driver to load\n");
	printf("  -c channel : channel to use (for a serial driver)\n");
	printf("  -d device_name : name to use in IFDHCreateChannelByName\n");
	printf("   like usb:08e6/3437:libusb:001/038 or /dev/pcsc/1\n\n");
	printf("example: %s -d /usr/lib/pcsc/drivers/serial/libGemPC410.so -c 2\n",
		argv0);
	printf(" to load the libGemPC410 and use /dev/pcsc/2\n");
	printf("or define environment variable LIB\n");
	printf(" LIB=/usr/lib/pcsc/drivers/serial/libGemPC410.so %s\n", argv0);

	exit(1);
} /* help */

int main(int argc, char *argv[])
{
	void *lib_handle = NULL;
	int ret;
	int channel = 0;
	char *driver;
	int opt;
	char *device_name = NULL;

	driver = getenv(ENV_LIBNAME);

	while ((opt = getopt(argc, argv, "ft:1234ATZnel:c:d:")) != EOF)
	{
		switch (opt)
		{
			case '1':
			case '2':
			case '3':
			case '4':
				cases |= 1 << (opt - '1');
				printf("test case: %c\n", opt);
				break;

			case 'e':
				extended = TRUE;
				printf("text extended APDU\n");
				break;

			case 'f':
				full = TRUE;
				printf("Full test (all APDU sizes)\n");
				break;

			case 't':
				timerequest = atol(optarg);
				printf("time request: %d\n", timerequest);
				break;

			case 'A':
				apdu = TRUE;
				printf("Use APDU\n");
				break;

			case 'T':
				tpdu = TRUE;
				printf("Use TPDU\n");
				break;

			case 'Z':
				t1 = TRUE;
				printf("Use T=1\n");
				break;

			case 'n':
				stop_on_error = FALSE;
				printf("Use non stop mode\n");
				break;

			case 'l':
				driver = optarg;
				printf("Using driver: %s\n", driver);
				break;

			case 'c':
				channel = atol(optarg);
				printf("Usinf channel: %d\n", channel);
				break;

			case 'd':
				device_name = optarg;
				printf("Using device: %s\n", device_name);
				break;

			default:
				printf ("caractère: %c (0x%02X)\n", opt, opt);
				help(argv[0]);
		}
	}

	if ((FALSE == tpdu) && (FALSE == apdu))
	{
		printf("\33[01;31mDefine TPDU (-T) or APDU (-A)\33[0m\n");
		help(argv[0]);
		return 2;
	}

	printf("Using driver: %s\n", driver);
	printf("Using channel: %d\n", channel);
	printf("Using device_name: %s\n", device_name);

	lib_handle = dlopen(driver, RTLD_LAZY);
	if (lib_handle == NULL)
	{
		DEBUG2("dlopen: %s", dlerror());
		return 1;
	}

	DLSYM(IFDHCreateChannel)
	DLSYM(IFDHCloseChannel)
	DLSYM(IFDHSetProtocolParameters)
	DLSYM(IFDHPowerICC)
	DLSYM(IFDHTransmitToICC)
	DLSYM(IFDHICCPresence)
	DLSYM(IFDHControl)

	f.IFDHCreateChannelByName = dlsym(lib_handle, "IFDHCreateChannelByName");
	if (f.IFDHCreateChannelByName == NULL)
	{
		f.version = IFD_HVERSION_2_0;

		/* API v2.0 does not have IFDHCreateChannelByName */
		if (device_name)
		{
			printf("IFDHCreateChannelByName not defined by the driver and device_name set\n");
			return 1;
		}
	}
	else
		f.version = IFD_HVERSION_3_0;

	printf("%s:%d\n", __FILE__, __LINE__);
	ret = handler_test(LUN, channel, device_name);
	dlclose(lib_handle);

	return ret;
} /* main */

int handler_test(int lun, int channel, char device_name[])
{
	int rv, test_rv = 1;
	UCHAR atr[MAX_ATR_SIZE];
	DWORD atrlength;

	if (device_name)
	{
		rv = f.IFDHCreateChannelByName(lun, device_name);

		if (rv != IFD_SUCCESS)
		{
			printf("IFDHCreateChannelByName: %d\n", rv);
			printf("\nAre you sure a CCID reader is connected?\n");
			printf("and that you have read/write permission on the device?\n");
			return 1;
		}
	}
	else
	{
		rv = f.IFDHCreateChannel(lun, channel);

		if (rv != IFD_SUCCESS)
		{
			printf("IFDHCreateChannel: %d\n", rv);
			printf("\nAre you sure a CCID reader is connected?\n");
			printf("and that you have read/write permission on the device?\n");
			return 1;
		}
	}

#define SCARD_CTL_CODE(code) (0x42000000 + (code))

#define IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE     SCARD_CTL_CODE(1)

#ifndef __APPLE__
	if (f.version >= IFD_HVERSION_3_0)
	{
		unsigned char cmd[] = "\x02";
		unsigned char res[100];
		DWORD length;

		rv = f.IFDHControl(LUN, IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE,
			cmd, sizeof(cmd)-1, res, sizeof(res), &length);
		if (IFD_SUCCESS == rv)
		{
			log_xxd(0, "Firmware: ", res, length);
			res[length] = '\0';
			printf("Firmware: %s\n", res);
		}
		else
			PCSC_ERROR("IFDHControl");
			//printf("IFDHControl: %s\n", ifd_error(rv));
	}
#endif

	rv = f.IFDHICCPresence(LUN);
	PCSC_ERROR("IFDHICCPresence");
	if (IFD_ICC_PRESENT != rv)
		goto end;

	rv = f.IFDHPowerICC(LUN, IFD_RESET, atr, &atrlength);
	if (rv != IFD_SUCCESS)
	{
		PCSC_ERROR("IFDHPowerICC");
		goto end;
	}

#ifdef __APPLE__
	rv = f.IFDHPowerICC(LUN, IFD_POWER_UP, atr, &atrlength);
	if (rv != IFD_SUCCESS)
	{
		PCSC_ERROR("IFDHPowerICC");
		goto end;
	}
#endif

	log_xxd(0, "ATR: ", atr, atrlength);

	rv = f.IFDHICCPresence(LUN);
	PCSC_ERROR("IFDHICCPresence");
	if (IFD_ICC_PRESENT != rv)
		goto end;

	rv = f.IFDHSetProtocolParameters(LUN,
		t1 ? SCARD_PROTOCOL_T1 : SCARD_PROTOCOL_T0,
		0, 0, 0, 0);
	PCSC_ERROR("IFDHSetProtocolParameters");
	if ((IFD_SUCCESS != rv) && (IFD_NOT_SUPPORTED != rv))
		goto end;

	if (extended)
		test_rv = extended_apdu(lun);
	else
		test_rv = short_apdu(lun);

end:
	/* Close */
	rv = f.IFDHCloseChannel(LUN);
	PCSC_ERROR("IFDHCloseChannel");
	if (rv != IFD_SUCCESS)
		return 1;

	printf("\nTest summary:\n");
	printf("Cases: ");
	if (cases & 1)
		printf("1 ");
	if (cases & 2)
		printf("2 ");
	if (cases & 4)
		printf("3 ");
	if (cases & 8)
		printf("4 ");
	printf("\n");
	printf("Extended APDU: %s\n", extended ? "yes" : "no");
	printf("All APDU sizes: %s\n", full ? "yes" : "no");
	printf("time request: %d\n", timerequest);
	printf("Use APDU: %s\n", apdu ? "yes" : "no");
	printf("Use TPDU: %s\n", tpdu ? "yes" : "no");
	printf("Use T=1: %s\n", t1 ? "yes" : "no");
	printf("\n");

	if (test_rv)
		printf("\33[01;31m***********\n*  ERROR  *\n***********\33[0m\n");
	else
		printf("\33[01;34m***********\n*   OK    *\n***********\33[0m\n");

	return 0;
} /* handler_test */

int extended_apdu(int lun)
{
	int i, len_i, len_o;
	SCARD_IO_HEADER SendPci, RecvPci;
	UCHAR s[MAX_BUFFER_SIZE_EXTENDED], r[MAX_BUFFER_SIZE_EXTENDED];
	DWORD dwSendLength, dwRecvLength;
	UCHAR e[MAX_BUFFER_SIZE_EXTENDED];	// expected result
	int e_length;	// expected result length
	char *text = NULL;
	int start, end;

	memset(&SendPci, 0, sizeof(SendPci));
	SendPci.Protocol = t1;

	memset(&RecvPci, 0, sizeof(RecvPci));

	if (cases & CASE2)
	{
		/* Case 2 */
		text = "Case 2: CLA INS P1 P2 Le, L(Cmd) = 5";
		start = end = 65535;
		if (full)
			start = 1;

		for (len_i = start; len_i <= end; len_i++)
		{
#ifdef CONTACTLESS
			s[0] = 0x00;
			s[1] = 0xD6;
			s[2] = 0x00;
			s[3] = 0x00;
#else
			s[0] = 0x80;
			s[1] = 0x12;
			s[2] = 0x01;
			s[3] = 0x80;
#endif
			s[4] = 0x00;	/* extended */
			s[5] = len_i >> 8;
			s[6] = len_i;

			for (i=0; i<len_i; i++)
				s[7+i] = i;

			dwSendLength = len_i + 7;
			dwRecvLength = sizeof(r);

			e[0] = 0x90;
			e[1] = 0x00;
			e_length = 2;

			if (exchange(text, lun, SendPci, &RecvPci,
				s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}
	}

	if (cases & CASE3)
	{
		/* Case 3 */
		/*
		 * 252  (0xFC) is max size for one USB or GBP paquet
		 * 256 (0x100) maximum, 1 minimum
		 */
		text = "Case 3: CLA INS P1 P2 Lc Data, L(Cmd) = 5 + Lc";
		start = end = 65535;
		if (full)
			start = 1;

		for (len_o = start; len_o <= end; len_o++)
		{
			char test_value = 0x42;

#ifdef CONTACTLESS
			s[0] = 0x00;
			s[1] = 0xB0;
			s[2] = 0x00;
			s[3] = 0x00;
#else
			s[0] = 0x80;
			s[1] = 0x00;
			s[2] = 0x04;
			s[3] = test_value;
#endif
			s[4] = 0x00;
			s[5] = len_o >> 8;
			s[6] = len_o;

			dwSendLength = 7;
			dwRecvLength = sizeof(r);

			for (i=0; i<len_o; i++)
				e[i] = test_value;
			e[i++] = 0x90;
			e[i++] = 0x00;
			e_length = len_o+2;

			if (exchange(text, lun, SendPci, &RecvPci,
				s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}
	}

	return 0;
} /* extended_apdu */

int short_apdu(int lun)
{
	int i, len_i, len_o;
	SCARD_IO_HEADER SendPci, RecvPci;
	UCHAR s[MAX_BUFFER_SIZE], r[MAX_BUFFER_SIZE];
	DWORD dwSendLength, dwRecvLength;
	UCHAR e[MAX_BUFFER_SIZE];	// expected result
	int e_length;	// expected result length
	char *text = NULL;
	int time;
	int start, end;

	memset(&SendPci, 0, sizeof(SendPci));
	SendPci.Protocol = t1;

	memset(&RecvPci, 0, sizeof(RecvPci));

	/* Select applet */
	text = "Select applet: ";
	s[0] = 0x00;
	s[1] = 0xA4;
	s[2] = 0x04;
	s[3] = 0x00;
	s[4] = 0x06;
	s[5] = 0xA0;
	s[6] = 0x00;
	s[7] = 0x00;
	s[8] = 0x00;
	s[9] = 0x18;
#ifdef COMBI
	s[10] = 0x50;
#else
	s[10] = 0xFF;
#endif

	dwSendLength = 11;
	dwRecvLength = sizeof(r);

	e[0] = 0x90;
	e[1] = 0x00;
	e_length = 2;

	if (exchange(text, lun, SendPci, &RecvPci,
		s, dwSendLength, r, &dwRecvLength, e, e_length))
		return 1;

	/* Time Request */
	if (timerequest >= 0)
	{
		text = "Time Request";
		time = timerequest;

		s[0] = 0x80;
		s[1] = 0x38;
		s[2] = 0x00;
		s[3] = time;
		s[4] = 0;

		if (apdu)
			dwSendLength = 4;
		else
			dwSendLength = 5;
		dwRecvLength = sizeof(r);

		e[0] = 0x90;
		e[1] = 0x00;
		e_length = 2;

		if (exchange(text, lun, SendPci, &RecvPci,
			s, dwSendLength, r, &dwRecvLength, e, e_length))
			return 1;
	}

	if (cases & CASE1)
	{
		if (apdu)
		{
			/* Case 1, APDU */
			text = "Case 1, APDU: CLA INS P1 P2, L(Cmd) = 4";
			s[0] = 0x80;
			s[1] = 0x30;
			s[2] = 0x00;
			s[3] = 0x00;

			dwSendLength = 4;
			dwRecvLength = sizeof(r);

			e[0] = 0x90;
			e[1] = 0x00;
			e_length = 2;

			if (exchange(text, lun, SendPci, &RecvPci,
						s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}

		if (tpdu)
		{
			/* Case 1, TPDU */
			text = "Case 1, TPDU: CLA INS P1 P2 P3 (=0), L(Cmd) = 5";
			s[0] = 0x80;
			s[1] = 0x30;
			s[2] = 0x00;
			s[3] = 0x00;
			s[4] = 0x00;

			dwSendLength = 5;
			dwRecvLength = sizeof(r);

			e[0] = 0x90;
			e[1] = 0x00;
			e_length = 2;

			if (exchange(text, lun, SendPci, &RecvPci,
						s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}
	}

	if (cases & CASE2)
	{
		/* Case 2 */
		/*
		 * 248 (0xF8) is max size for one USB or GBP paquet
		 * 255 (0xFF) maximum, 1 minimum
		 */
		text = "Case 2: CLA INS P1 P2 Le, L(Cmd) = 5";
		start = end = 255;
		if (full)
			start = 1;

		for (len_i = start; len_i <= end; len_i++)
		{
			s[0] = 0x80;
			s[1] = 0x32;
			s[2] = 0x00;
			s[3] = 0x00;
			s[4] = len_i;

			for (i=0; i<len_i; i++)
				s[5+i] = i;

			dwSendLength = len_i + 5;
			dwRecvLength = sizeof(r);

			e[0] = 0x90;
			e[1] = 0x00;
			e_length = 2;

			if (exchange(text, lun, SendPci, &RecvPci,
				s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}
	}

	if (cases & CASE3)
	{
		/* Case 3 */
		/*
		 * 252  (0xFC) is max size for one USB or GBP paquet
		 * 256 (0x100) maximum, 1 minimum
		 */
		text = "Case 3: CLA INS P1 P2 Lc Data, L(Cmd) = 5 + Lc";
		start = end = 256;
		if (full)
			start = 1;

		for (len_o = start; len_o <= end; len_o++)
		{
			s[0] = 0x80;
			s[1] = 0x34;
			if (len_o > 255)
			{
				s[2] = 0x01;
				s[3] = len_o-256;
			}
			else
			{
				s[2] = 0x00;
				s[3] = len_o;
			}
			s[4] = len_o;

			dwSendLength = 5;
			dwRecvLength = sizeof(r);

			for (i=0; i<len_o; i++)
				e[i] = i;
			e[i++] = 0x90;
			e[i++] = 0x00;
			e_length = len_o+2;

			if (exchange(text, lun, SendPci, &RecvPci,
				s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}

#if 0
		/* Case 3, length too short */
		text = "Case 3, length too short: CLA INS P1 P2 Lc Data, L(Cmd) = 5 + Lc";
		len_o = 20;

		s[0] = 0x80;
		s[1] = 0x3C;
		if (len_o > 255)
		{
			s[2] = 0x01;
			s[3] = len_o-256;
		}
		else
		{
			s[2] = 0x00;
			s[3] = len_o;
		}
		s[4] = len_o-10;

		dwSendLength = 5;
		dwRecvLength = sizeof(r);

		if (tpdu)
		{
			for (i=0; i<len_o; i++)
				e[i] = i;
			e[i++] = 0x90;
			e[i++] = 0x00;
			e_length = len_o+2;
		}
		else
		{
			e[0] = 0x6C;
			e[1] = len_o;
			e_length = 2;
		}

		if (exchange(text, lun, SendPci, &RecvPci,
			s, dwSendLength, r, &dwRecvLength, e, e_length))
			return 1;
#endif

#if 0
		/* Case 3, length too long */
		text = "Case 3, length too long: CLA INS P1 P2 Lc Data, L(Cmd) = 5 + Lc";
		len_o = 20;

		s[0] = 0x80;
		s[1] = 0x3C;
		if (len_o > 255)
		{
			s[2] = 0x01;
			s[3] = len_o-256;
		}
		else
		{
			s[2] = 0x00;
			s[3] = len_o;
		}
		s[4] = len_o+10;

		dwSendLength = 5;
		dwRecvLength = sizeof(r);

		if (tpdu)
		{
			for (i=0; i<len_o; i++)
				e[i] = i;
			e[i++] = 0x90;
			e[i++] = 0x00;
			e_length = len_o+2;
		}
		else
		{
			e[0] = 0x6C;
			e[1] = len_o;
			e_length = 2;
		}

		if (exchange(text, lun, SendPci, &RecvPci,
			s, dwSendLength, r, &dwRecvLength, e, e_length))
			return 1;
#endif
	}

	if (cases & CASE4)
	{
		if (tpdu)
		{
			/* Case 4, TPDU */
			/*
			 * len_i
			 * 248 (0xF8) is max size for one USB or GBP paquet
			 * 255 (0xFF) maximum, 1 minimum
			 *
			 * len_o
			 * 252  (0xFC) is max size for one USB or GBP paquet
			 * 256 (0x100) maximum, 1 minimum
			 */
			start = end = 255;
			if (full)
				start = 1;

			for (len_i = start; len_i <= end; len_i++)
			{
				text = "Case 4, TPDU: CLA INS P1 P2 Lc Data, L(Cmd) = 5 + Lc";
				len_o = 256 - len_i;

				s[0] = 0x80;
				s[1] = 0x36;
				if (len_o > 255)
				{
					s[2] = 0x01;
					s[3] = len_o-256;
				}
				else
				{
					s[2] = 0x00;
					s[3] = len_o;
				}
				s[4] = len_i;

				for (i=0; i<len_i; i++)
					s[5+i] = i;

				dwSendLength = len_i + 5;
				dwRecvLength = sizeof(r);

				e[0] = 0x61;
				e[1] = len_o & 0xFF;
				e_length = 2;

				if (exchange(text, lun, SendPci, &RecvPci,
					s, dwSendLength, r, &dwRecvLength, e, e_length))
					return 1;

				/* Get response */
				text = "Case 4, TPDU, Get response: ";
				s[0] = 0x80;
				s[1] = 0xC0;
				s[2] = 0x00;
				s[3] = 0x00;
				s[4] = r[1]; /* SW2 of previous command */

				dwSendLength = 5;
				dwRecvLength = sizeof(r);

				for (i=0; i<len_o; i++)
					e[i] = i;
				e[i++] = 0x90;
				e[i++] = 0x00;
				e_length = len_o+2;

				if (exchange(text, lun, SendPci, &RecvPci,
					s, dwSendLength, r, &dwRecvLength, e, e_length))
					return 1;
			}
		}

		if (apdu)
		{
			/* Case 4, APDU */
			/*
			 * len_i
			 * 248 (0xF8) is max size for one USB or GBP paquet
			 * 255 (0xFF) maximum, 1 minimum
			 *
			 * len_o
			 * 252  (0xFC) is max size for one USB or GBP paquet
			 * 256 (0x100) maximum, 1 minimum
			 */
			text = "Case 4, APDU: CLA INS P1 P2 Lc Data Le, L(Cmd) = 5 + Lc +1";
			start = end = 255;
			if (full)
				start = 1;

			for (len_i = start; len_i <= end; len_i++)
			{
				len_o = 256 - len_i;

				s[0] = 0x80;
				s[1] = 0x36;
				if (len_o > 255)
				{
					s[2] = 0x01;
					s[3] = len_o-256;
				}
				else
				{
					s[2] = 0x00;
					s[3] = len_o;
				}
				s[4] = len_i;

				for (i=0; i<len_i; i++)
					s[5+i] = i;
				s[5+len_i] = len_o & 0xFF;

				dwSendLength = len_i + 6;
				dwRecvLength = sizeof(r);

				for (i=0; i<len_o; i++)
					e[i] = i;
				e[i++] = 0x90;
				e[i++] = 0x00;
				e_length = len_o+2;

				if (exchange(text, lun, SendPci, &RecvPci,
					s, dwSendLength, r, &dwRecvLength, e, e_length))
					return 1;
			}
		}
	}

	return 0;
} /* short_apdu */

char *ifd_error(int rv)
{
	static char strError[80];

	switch (rv)
	{
		case IFD_SUCCESS:
			strcpy(strError, "IFD: success");
			break;

		case IFD_ERROR_PTS_FAILURE:
			strcpy(strError, "IFD: PTS failure");
			break;

		case IFD_ICC_PRESENT:
			strcpy(strError, "IFD: card present");
			break;

		case IFD_ICC_NOT_PRESENT:
			strcpy(strError, "IFD: card _NOT_ present");
			break;
			
		case IFD_COMMUNICATION_ERROR:
			strcpy(strError, "IFD: communication error");
			break;

		case IFD_PROTOCOL_NOT_SUPPORTED:
			strcpy(strError, "IFD: protocol not supported");
			break;

		case IFD_RESPONSE_TIMEOUT:
			strcpy(strError, "IFD: response timeout");
			break;

		case IFD_NOT_SUPPORTED:
			strcpy(strError, "IFD: not supported");
			break;

		case IFD_ERROR_POWER_ACTION:
			strcpy(strError, "IFD: power up failed");
			break;

		case IFD_NO_SUCH_DEVICE:
			strcpy(strError, "IFD: no such device");
			break;

		default:
			snprintf(strError, sizeof(strError)-1, "IFD: undocumented error: %d", rv);
	}

	return strError;
} /* ifd_error */

int exchange(char *text, DWORD lun, SCARD_IO_HEADER SendPci,
	PSCARD_IO_HEADER RecvPci,
	UCHAR s[], DWORD s_length,
	UCHAR r[], PDWORD r_length,
	UCHAR e[], int e_length)
{
	int rv;
#ifndef CONTACTLESS
	int i;
#endif

	printf("\n%s\n", text);
	log_xxd(0, "Sent: ", s, s_length);

	rv = f.IFDHTransmitToICC(lun, SendPci, s, s_length, r, r_length, RecvPci);

	log_msg(0, "Received %d (0x%04X) bytes", *r_length, *r_length);
	log_xxd(0, "Received: ", r, *r_length);
	if (rv)
	{
		PCSC_ERROR("IFDHTransmitToICC");
		return 1;
	}

	/* check the received length */
	if (*r_length != e_length)
	{
		printf("\33[01;31mERROR: Expected %d bytes and received %ld\33[0m\n",
			e_length, *r_length);
		if (stop_on_error)
			return 1;
	}

#ifndef CONTACTLESS
	/* check the received data */
	for (i=0; i<e_length; i++)
		if (r[i] != e[i])
		{
			printf("\33[01;31mERROR byte %d: expected 0x%02X, got 0x%02X\n\33[0m",
				i, e[i], r[i]);
			if (stop_on_error)
				return 1;
			break;
		}
#endif

	printf("--------> OK\n");

	return 0;
} /* exchange */

