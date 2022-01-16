/*
 * GCdebug.c: log (or not) messages
 * Copyright (C) 2001-2022 Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * License: this code is under a double licence COPYING.BSD and COPYING.GPL
 *
 */


#include "debuglog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

static void log_time(void)
{
	static struct timeval last_time = { 0, 0 };
	struct timeval new_time = { 0, 0 };
	struct timeval tmp;
	int delta;

	gettimeofday(&new_time, NULL);
	if (0 == last_time.tv_sec)
		last_time = new_time;

	tmp.tv_sec = new_time.tv_sec - last_time.tv_sec;
	tmp.tv_usec = new_time.tv_usec - last_time.tv_usec;
	if (tmp.tv_usec < 0)
	{
		tmp.tv_sec--;
		tmp.tv_usec += 1000000;
	}
	if (tmp.tv_sec < 100)
		delta = tmp.tv_sec * 1000000 + tmp.tv_usec;
	else
		delta = 99999999;

	last_time = new_time;

	printf("%.8d ", delta);
}

void log_msg(const int priority, const char *fmt, ...)
{
	va_list argptr;

	(void)priority;

	log_time();

	va_start(argptr, fmt);
	printf("\33[35m"); /* Magenta */
	vprintf(fmt, argptr);
	printf("\33[0m");
	va_end(argptr);
	printf("\n");

	fflush(stdout);
} /* log_msg */

void log_xxd(const int priority, const char *msg, const unsigned char *buffer,
	const int len)
{
	int i;

	(void)priority;

	log_time();

	printf("%s", msg);

	for (i = 0; i < len; ++i)
		printf("%02X ", buffer[i]);

	printf("\n");

	fflush(stdout);
} /* log_xxd */

