/*
    handler_test.c: main function used for IFDH debug
    Copyright (C) 2001-2003   Ludovic Rousseau

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
#include <winscard.h>
#include <string.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <getopt.h>

#include "ifdhandler.h"
#include "debug.h"

#define LUN 0
#define ENV_LIBNAME "LIB"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

int handler_test(int lun, int channel);
void pcsc_error(int rv);
int exchange(char *text, DWORD lun, SCARD_IO_HEADER SendPci,
	PSCARD_IO_HEADER RecvPci,
	UCHAR s[], DWORD s_length,
	UCHAR r[], PDWORD r_length,
	UCHAR e[], int e_length);

#define DLSYM(func)  f.func = dlsym(lib_handle, "" # func); \
	if (f.func == NULL) { \
	DEBUG("dlsym: " # func); \
	return 1; }

#define COMPARE(r, e, l) { int i; \
	for (i=0; i<l; i++) \
		if (r[i] != e[i]) \
		{ \
			printf("ERROR byte %d: expected 0x%02X, got 0x%02X\n", i, e[i], r[i]);\
			return 1; \
		} \
	printf("--------> OK\n"); \
	}

struct f_t {
	RESPONSECODE (*IFDHCreateChannel)(DWORD, DWORD);
	RESPONSECODE (*IFDHCloseChannel)(DWORD);
	//RESPONSECODE IFDHGetCapabilities ( DWORD, DWORD, PDWORD, PUCHAR );
	//RESPONSECODE IFDHSetCapabilities ( DWORD, DWORD, DWORD, PUCHAR );
	//RESPONSECODE IFDHSetProtocolParameters ( DWORD, DWORD, UCHAR, UCHAR, UCHAR, UCHAR );
	RESPONSECODE (*IFDHPowerICC)(DWORD, DWORD, PUCHAR, PDWORD);
	RESPONSECODE (*IFDHTransmitToICC)(DWORD, SCARD_IO_HEADER, PUCHAR, 
	  			   DWORD, PUCHAR, PDWORD, 
	  			   PSCARD_IO_HEADER);
	//RESPONSECODE IFDHControl ( DWORD, PUCHAR, DWORD, PUCHAR, PDWORD );
	RESPONSECODE (*IFDHICCPresence)(DWORD);
};

struct f_t f = { NULL, NULL, NULL, NULL, NULL };

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

/* getopt(3) */
extern char *optarg;
extern int optind, opterr, optopt;


void help(char *argv0)
{
	printf("\nUsage: %s [-f] [-t val] [-1] [-2] [-3] [-4] [-A] [-T] libname [channel]\n", argv0);
	printf("  -f : test APDU with every possible lengths\n");
	printf("  -t val : use val as timerequest value. Set to 0 to avoid test\n");
	printf("  -1 : test CASE 1 APDU\n");
	printf("  -2 : test CASE 2 APDU\n");
	printf("  -3 : test CASE 3 APDU\n");
	printf("  -4 : test CASE 4 APDU\n");
	printf("  -A : use APDU\n");
	printf("  -T : use TPDU\n");
	printf("  -Z : use T=1 instead of default T=0\n");
	printf("  libname : driver to load\n");
	printf("  channel : channel to use (for a serial driver)\n\n");
	printf("example: %s /usr/lib/pcsc/drivers/serial/libGemPC410.so 2\n",
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

	while ((opt = getopt(argc, argv, "ft:1234ATZ")) != EOF)
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

			default:
				printf ("caractère: %c (0x%02X)\n", opt, opt);
				help(argv[0]);
		}
	}

	driver = getenv(ENV_LIBNAME);

	if (driver == NULL)
	{
		if (argc - optind < 1)
			help(argv[0]);

		// driver
		driver = argv[optind];

		// channel
		if (argc - optind >= 2)
			channel = atoi(argv[optind+1]);
	}
	else
	{
		// channel
		if (argc - optind >= 1)
			channel = atoi(argv[optind]);
	}

	printf("Using driver: %s\n", driver);
	printf("Using channel: %d\n", channel);

	lib_handle = dlopen(driver, RTLD_LAZY);
	if (lib_handle == NULL)
	{
		DEBUG2("dlopen: %s", dlerror());
		return 1;
	}

	DLSYM(IFDHCreateChannel)
	DLSYM(IFDHCloseChannel)
	DLSYM(IFDHPowerICC)
	DLSYM(IFDHTransmitToICC)
	DLSYM(IFDHICCPresence)

	ret = handler_test(LUN, channel);
	dlclose(lib_handle);

	return ret;
} /* main */

int handler_test(int lun, int channel)
{
	int rv, i, len_i, len_o;
	UCHAR atr[MAX_ATR_SIZE];
	DWORD atrlength;
	UCHAR s[MAX_BUFFER_SIZE], r[MAX_BUFFER_SIZE];
	DWORD dwSendLength, dwRecvLength;
	SCARD_IO_HEADER SendPci, RecvPci;
	UCHAR e[MAX_BUFFER_SIZE];	// expected result
	int e_length;	// expected result length
	char *text = NULL;
	int time;
	int start, end;

	rv = f.IFDHCreateChannel(lun, channel);

	if (rv != IFD_SUCCESS)
	{
		printf("IFDHCreateChannel: %d\n", rv);
		printf("\nAre you sure a CCID reader is connected?\n");
		printf("and that you have read/write permission on the device?\n");
		return 1;
	}

	rv = f.IFDHICCPresence(LUN);
	pcsc_error(rv);

	rv = f.IFDHPowerICC(LUN, IFD_RESET, atr, &atrlength);
	if (rv != IFD_SUCCESS)
	{
		printf("IFDHPowerICC: %d\n", rv);

		goto end;
	}

	debug_xxd("ATR: ", atr, atrlength);

	rv = f.IFDHICCPresence(LUN);
	pcsc_error(rv);

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
	s[10] = 0xFF;

	dwSendLength = 11;
	dwRecvLength = sizeof(r);

	e[0] = 0x90;
	e[1] = 0x00;
	e_length = 2;

	if (exchange(text, lun, SendPci, &RecvPci,
		s, dwSendLength, r, &dwRecvLength, e, e_length))
		goto end;

	/* Time Request */
	if (timerequest >= 0)
	{
		text = "Time Request";
		time = timerequest;

		s[0] = 0x80;
		s[1] = 0x38;
		s[2] = 0x00;
		s[3] = time;

		dwSendLength = 4;
		dwRecvLength = sizeof(r);

		e[0] = 0x90;
		e[1] = 0x00;
		e_length = 2;

		if (exchange(text, lun, SendPci, &RecvPci,
			s, dwSendLength, r, &dwRecvLength, e, e_length))
			goto end;
	}

	if (cases & CASE1)
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
			goto end;

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
			goto end;
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
				goto end;
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
		start = end = 255;
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
				goto end;
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
			goto end;
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
			goto end;
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
					goto end;

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
					goto end;
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
					goto end;
			}
		}
	}

end:
	/* Close */
	rv = f.IFDHCloseChannel(LUN);
	if (rv != IFD_SUCCESS)
	{
		printf("IFDHCloseChannel: %d\n", rv);
		return 1;
	}

	return 0;
} /* handler_test */

void pcsc_error(int rv)
{
	switch (rv)
	{
		case IFD_ICC_PRESENT:
			DEBUG("IFD: card present");
			break;

		case IFD_ICC_NOT_PRESENT:
			DEBUG("IFD: card _NOT_ present");
			break;
			
		case IFD_COMMUNICATION_ERROR:
			DEBUG("IFD: communication error");
			break;

		case IFD_PROTOCOL_NOT_SUPPORTED:
			DEBUG("IFD: protocol not supported");
			break;

		case IFD_RESPONSE_TIMEOUT:
			DEBUG("IFD: response timeout");
			break;

		default:
			DEBUG2("IFD: undocumented error: %X", rv);
	}
} /* pcsc_error */

int exchange(char *text, DWORD lun, SCARD_IO_HEADER SendPci,
	PSCARD_IO_HEADER RecvPci,
	UCHAR s[], DWORD s_length,
	UCHAR r[], PDWORD r_length,
	UCHAR e[], int e_length)
{
	int rv, i;

	printf("\n%s\n", text);
	debug_xxd("Sent: ", s, s_length);

	rv = f.IFDHTransmitToICC(lun, SendPci, s, s_length, r, r_length, RecvPci);

	debug_xxd("Received: ", r, *r_length);
	if (rv)
	{
		pcsc_error(rv);
		return 1;
	}

	/* check the received data */
	for (i=0; i<*r_length; i++)
		if (r[i] != e[i])
		{
			printf("ERROR byte %d: expected 0x%02X, got 0x%02X\n", i, e[i], r[i]);
			return 1;
		}

	/* check the received length */
	if (*r_length != e_length)
	{
		printf("ERROR: Expected %d bytes and received %ld\n", e_length, *r_length);
		return 1;
	}

	printf("--------> OK\n");

	return 0;
} /* exchange */

