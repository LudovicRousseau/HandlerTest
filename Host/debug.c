/*
 * $Id$
 * GCdebug.c: log (or not) messages
 * Copyright (C) 2001-2010 Ludovic Rousseau <ludovic.rousseau@free.fr>
 * 
 * License: this code is under a double licence COPYING.BSD and COPYING.GPL
 * 
 */


#include "debuglog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void log_msg(const int priority, const char *fmt, ...)
{
	va_list argptr;

	(void)priority;

	va_start(argptr, fmt);
	printf("\33[35m"); /* Magenta */
	vprintf(fmt, argptr);
	printf("\33[0m");
	va_end(argptr);
	printf("\n");
} /* log_msg */

void log_xxd(const int priority, const char *msg, const unsigned char *buffer,
	const int len)
{
	int i;

	(void)priority;

	printf("%s", msg);

	for (i = 0; i < len; ++i)
		printf("%02X ", buffer[i]);

	printf("\n");
} /* log_xxd */

