/*	$OpenBSD: log.c,v 1.6 1999/04/19 19:53:41 niklas Exp $	*/
/*	$EOM: log.c,v 1.23 1999/04/18 15:17:24 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
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

#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "sysdep.h"

#include "log.h"

static void _log_print (int, int, const char *, va_list, int, int);

static FILE *log_output = stderr;
static int log_level[LOG_ENDCLASS];

void
log_to (FILE *f)
{
  if (!log_output && f)
    closelog ();
  log_output = f;
  if (!f)
    openlog ("isakmpd", LOG_CONS, LOG_DAEMON);
}

FILE *
log_current (void)
{
  return log_output;
}

static char *
_log_get_class (int error_class)
{
  /* XXX For test purposes. To be removed later on?  */
  static char *class_text[] = LOG_CLASSES_TEXT;

  if (error_class < 0)
    return "Dflt";
  else if (error_class >= LOG_ENDCLASS)
    return "Unkn";
  else
    return class_text[error_class];
}

static void
_log_print (int error, int syslog_level, const char *fmt, va_list ap, 
	    int class, int level)
{
  char buffer[LOG_SIZE], nbuf[LOG_SIZE + 32];
  static const char fallback_msg[] = 
    "write to log file failed (errno %d), redirecting output to syslog";
  int len;
  struct tm *tm;
  struct timeval now;
  time_t t;

  len = vsnprintf (buffer, LOG_SIZE, fmt, ap);
  if (len < LOG_SIZE - 1 && error)
    snprintf (buffer + len, LOG_SIZE - len, ": %s", strerror (errno));
  if (log_output)
    {
      gettimeofday (&now, 0);
      t = now.tv_sec;
      tm = localtime (&t);
      if (class >= 0)
	sprintf (nbuf, "%02d%02d%02d.%06ld %s %02d ", tm->tm_hour, 
		 tm->tm_min, tm->tm_sec, now.tv_usec, _log_get_class (class), 
		 level);
      else /* LOG_PRINT (-1) or LOG_REPORT (-2) */
	sprintf (nbuf, "%02d%02d%02d.%06ld %s ", tm->tm_hour, 
		 tm->tm_min, tm->tm_sec, now.tv_usec,
		 class == LOG_PRINT ? "Default" : "Report>");	
      strcat (nbuf, buffer);
      strcat (nbuf, "\n");

      if (fwrite (nbuf, strlen (nbuf), 1, log_output) == 0)
	{
	  /* Report fallback.  */
	  syslog (LOG_ALERT, fallback_msg, errno);
	  fprintf (log_output, fallback_msg, errno);

	  /* Fallback to syslog.  */
	  log_to (0);

	  /* (Re)send current message to syslog(). */
	  syslog (class == LOG_REPORT ? LOG_ALERT : syslog_level, buffer);
	}
    }
  else
    syslog (class == LOG_REPORT ? LOG_ALERT : syslog_level, buffer);
}

void
#ifdef __STDC__
log_debug (int cls, int level, const char *fmt, ...)
#else
log_debug (cls, level, fmt, va_alist)
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
  if (cls >= 0 && (log_level[cls] == 0 || level > log_level[cls]))
    return;
#ifdef __STDC__
  va_start (ap, fmt);
#else
  va_start (ap);
  fmt = va_arg (ap, const char *);
#endif
  _log_print (0, LOG_DEBUG, fmt, ap, cls, level);
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
  if (cls >= 0 && (log_level[cls] == 0 || level > log_level[cls]))
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
  _log_print (0, LOG_NOTICE, fmt, ap, LOG_PRINT, 0);
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
  _log_print (1, LOG_ERR, fmt, ap, LOG_PRINT, 0);
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
  _log_print (1, LOG_CRIT, fmt, ap, LOG_PRINT, 0);
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
