/*
 * $Id$
 * GCdebug.c: log (or not) messages
 * Copyright (C) 2001 Ludovic Rousseau <ludovic.rousseau@free.fr>
 * 
 * License: this code is under a double licence COPYING.BSD and COPYING.GPL
 * 
 */


#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_BUF_SIZE 4096

static char DebugBuffer[DEBUG_BUF_SIZE];

void debug_msg(char *fmt, ...)
{
	va_list argptr;

	va_start(argptr, fmt);
	vsnprintf(DebugBuffer, DEBUG_BUF_SIZE, fmt, argptr);
	va_end(argptr);

	fprintf(stdout, "%s\n", DebugBuffer);
} /* debug_msg */

void debug_xxd(const char *msg, const unsigned char *buffer, const int len)
{
	int i;
	unsigned char *c, *debug_buf_end;

	debug_buf_end = DebugBuffer + DEBUG_BUF_SIZE - 5;

	strncpy(DebugBuffer, msg, sizeof(DebugBuffer)-1);
	c = DebugBuffer + strlen(DebugBuffer);

	for (i = 0; (i < len) && (c < debug_buf_end); ++i)
	{
		sprintf(c, "%02X ", buffer[i]);
		c += strlen(c);
	}

	fprintf(stdout, "%s\n", DebugBuffer);
} /* debug_xxd */

