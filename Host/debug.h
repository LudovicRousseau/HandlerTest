/*
 * $Id$
 * gcdebug.h: log (or not) messages using syslog
 * Copyright (C) 2001 Ludovic Rousseau <ludovic.rousseau@free.fr>
 * 
 * License: this code is under a double licence COPYING.BSD and COPYING.GPL
 *
 */

/*
 * DEBUG_CRITICAL("text");
 * 	print "text" is DEBUG_LEVEL_CRITICAL and DEBUG_STDERR is defined
 * 	send "text" to syslog if DEBUG_LEVEL_CRITICAL is defined
 *
 * DEBUG_CRITICAL2("text: %d", 1234)
 *  print "text: 1234" is DEBUG_LEVEL_CRITICAL and DEBUG_STDERR is defined
 *  send "text: 1234" to syslog if DEBUG_LEVEL_CRITICAL is defined
 * the format string can be anything printf() can understand
 * 
 * same thing for DEBUG_INFO and DEBUG_COMM
 *
 * DEBUG_XXD(msg, buffer, size) is only defined if DEBUG_LEVEL_COMM if defined
 *
 */
 
#ifndef _GCDEBUG_H_
#define  _GCDEBUG_H_

/* You can't do #ifndef __FUNCTION__ */
#if !defined(__GNUC__) && !defined(__IBMC__)
#define __FUNCTION__ ""
#endif

#define DEBUG(fmt) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__)
#define DEBUG2(fmt, data) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data)
#define DEBUG3(fmt, data1, data2) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2)

void debug_msg(char *fmt, ...);
void debug_xxd(const char *msg, const unsigned char *buffer, const int size);

#endif

