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

void log_msg(const int priority, char *fmt, ...)
{
	va_list argptr;

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
	printf("\n");
} /* log_msg */

void log_xxd(const int priority, const char *msg, const unsigned char *buffer,
	const int len)
{
	int i;

	printf(msg);

	for (i = 0; i < len; ++i)
		printf("%02X ", buffer[i]);

	printf("\n");
} /* log_xxd */

