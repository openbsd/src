/*	$Id: log.c,v 1.1.1.1 1998/11/15 00:03:49 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "log.h"

/*
 * We cannot do the log strings dynamically sizeable as out of memory is one
 * of the situations we need to report about.
 */
#define LOG_SIZE	200

static void _log_print (int, int, const char *, va_list);

static FILE *log_output = stderr;
static int log_level[LOG_ENDCLASS];

void
log_to (FILE *f)
{
  if (!log_output && f)
    closelog ();
  log_output = f;
  if (!f)
    openlog ("isakmpd", 0, LOG_DAEMON);
}

static void
_log_print (int error, int level, const char *fmt, va_list ap)
{
  char buffer[LOG_SIZE];
  int len;

  len = vsnprintf (buffer, LOG_SIZE, fmt, ap);
  if (len < LOG_SIZE - 1 && error)
    snprintf (buffer + len, LOG_SIZE - len, ": %s", strerror (errno));
  if (log_output)
    {
      fputs (buffer, log_output);
      fputc ('\n', log_output);
    }
  else
    syslog (level, buffer);
}

void
#ifdef __STDC__
log_debug (int cls, int level, const char *fmt, ...)
#else
log_debug (cls, level, clfmt, va_alist)
     int cls;
     int level;
     const char *fmt;
     va_dcl
#endif
{
  va_list ap;

  /*
   * If we are not debugging this class, or the level is too low, just return.
   */
  if (log_level[cls] == 0 || level > log_level[cls])
    return;
#ifdef __STDC__
  va_start (ap, fmt);
#else
  va_start (ap);
  fmt = va_arg (ap, const char *);
#endif
  _log_print (0, LOG_DEBUG, fmt, ap);
  va_end (ap);
}

void
log_debug_buf (int cls, int level, const char *header, const u_int8_t *buf,
	       size_t sz)
{
  char s[73];
  int i, j;

  /*
   * If we are not debugging this class, or the level is too low, just return.
   */
  if (log_level[cls] == 0 || level > log_level[cls])
    return;

  log_debug (cls, level, "%s:", header);
  for (i = j = 0; i < sz;)
    {
      sprintf (s + j, "%02x", buf[i++]);
      j += 2;
      if (i % 4 == 0)
	{
	  if (i % 32 == 0)
	    {
	      s[j] = '\0';
	      log_debug (cls, level, "%s", s);
	      j = 0;
	    }
	  else
	    s[j++] = ' ';
	}
    }
  if (j)
    {
      s[j] = '\0';
      log_debug (cls, level, "%s", s);
    }
}

void
#ifdef __STDC__
log_print (const char *fmt, ...)
#else
log_print (fmt, va_alist)
     const char *fmt;
     va_dcl
#endif
{
  va_list ap;

#ifdef __STDC__
  va_start (ap, fmt);
#else
  va_start (ap);
  fmt = va_arg (ap, const char *);
#endif
  _log_print (0, LOG_NOTICE, fmt, ap);
  va_end (ap);
}

void
#ifdef __STDC__
log_error (const char *fmt, ...)
#else
log_error (fmt, va_alist)
     const char *fmt;
     va_dcl
#endif
{
  va_list ap;

#ifdef __STDC__
  va_start (ap, fmt);
#else
  va_start (ap);
  fmt = va_arg (ap, const char *);
#endif
  _log_print (1, LOG_ERR, fmt, ap);
  va_end (ap);
}

void
#ifdef __STDC__
log_fatal (const char *fmt, ...)
#else
log_fatal (fmt, va_alist)
     const char *fmt;
     va_dcl
#endif
{
  va_list ap;

#ifdef __STDC__
  va_start (ap, fmt);
#else
  va_start (ap);
  fmt = va_arg (ap, const char *);
#endif
  _log_print (1, LOG_CRIT, fmt, ap);
  va_end (ap);
  exit (1);
}

void
log_debug_cmd (int cls, int level)
{
  if (cls < 0 || cls >= LOG_ENDCLASS)
    {
      log_print ("log_debug_cmd: invalid debugging class %d", cls);
      return;
    }

  if (level < 0)
    {
      log_print ("log_debug_cmd: invalid debugging level %d for class %d",
		 level, cls);
      return;
    }

  if (level == log_level[cls])
    log_print ("log_debug_cmd: log level unchanged for class %d", cls);
  else
    {
      log_print ("log_debug_cmd: log level changed from %d to %d for class %d",
		 log_level[cls], level, cls);
      log_level[cls] = level;
    }
}
