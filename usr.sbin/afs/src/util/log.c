/*	$OpenBSD: log.c,v 1.1.1.1 1998/09/14 21:53:23 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Logging functions
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: log.c,v 1.12 1998/07/09 19:57:26 art Exp $");
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <roken.h>
#include "log.h"

/*
 * The structure for each logging method.
 */

struct log_method {
     char *name;
     void (*open)(Log_method *, char *progname, char *fname);
     void (*vprint)(Log_method *, char *, va_list);
     void (*print)(Log_method *, char *);
     void (*close)(Log_method *);
     union {
	  void *v;
	  int i;
     } data;
     unsigned mask;
};

#if HAVE_SYSLOG
static void log_open_syslog (Log_method *lm, char *progname, char *fname);

static void log_close_syslog (Log_method *lm);
#if HAVE_VSYSLOG
static void log_vprint_syslog (Log_method *lm, char *, va_list);
#else
static void log_print_syslog (Log_method *lm, char *);
#endif /* HAVE_VSYSLOG */
#endif /* HAVE_SYSLOG */

static void
log_open_stderr (Log_method *lm, char *progname, char *fname);

static void
log_open_file (Log_method *lm, char *progname, char *fname);

static void
log_close_file (Log_method *lm);

static void
log_vprint_file (Log_method *lm, char *, va_list);

/*
 * The names for which we do special handling in the logging routines.
 */

static
Log_method special_names[] = {
#if HAVE_SYSLOG
{"syslog",	log_open_syslog,
#if HAVE_VSYSLOG
                                log_vprint_syslog, NULL
#else
                                NULL, log_print_syslog
#endif /* HAVE_VSYSLOG */
                                                        , log_close_syslog},
#endif /* HAVE_SYSLOG */
{"/dev/stderr",	log_open_stderr, log_vprint_file, NULL, NULL},
{NULL,		log_open_file, log_vprint_file, NULL, log_close_file}
/* Should be last */
};

#if HAVE_SYSLOG
static void
log_open_syslog (Log_method *lm, char *progname, char *fname)
{
     openlog (progname, LOG_PID, LOG_DAEMON);
}

#if HAVE_VSYSLOG

static void
log_vprint_syslog (Log_method *lm, char *fmt, va_list args)
{
     vsyslog (LOG_INFO, fmt, args);
}
#else

static void
log_print_syslog (Log_method *lm, char *str)
{
     syslog (LOG_INFO, str);
}
#endif /* HAVE_VSYSLOG */

static void
log_close_syslog (Log_method *lm)
{
     closelog ();
}

#endif /* HAVE_SYSLOG */

static void
log_open_stderr (Log_method *lm, char *progname, char *fname)
{
#if 0
     lm = &special_names[sizeof(special_names) /
			 sizeof(*special_names) - 1];
#endif
     lm->data.v = stderr;
}

static void
log_open_file (Log_method *lm, char *progname, char *fname)
{
     lm->data.v = (void *)fopen (fname, "a");
     if (lm->data.v == NULL)
	  lm->data.v = stderr;
}

static void
log_vprint_file (Log_method *lm,  char *fmt, va_list args)
{
    struct timeval tv = { 0, 0 };
    char *time;
    time_t t;

    gettimeofday(&tv, NULL);
    t = tv.tv_sec;
    time = strdup(ctime(&t));
    if (time) {
	time[strlen(time)-1] = '\0';
	fprintf ((FILE *)lm->data.v, "%s: ", time);
	free(time);
    } else
	fprintf ((FILE *)lm->data.v, "unknown time:");

    fprintf ((FILE *)lm->data.v, "%s: ", __progname);
    vfprintf ((FILE *)lm->data.v, fmt, args);
    putc ('\n', (FILE *)lm->data.v);
}

static void
log_close_file (Log_method *lm)
{
     fclose ((FILE *)lm->data.v);
}

Log_method *
log_open (char *progname, char *fname)
{
     int i;
     Log_method *log;

     log = (Log_method *)malloc (sizeof(Log_method));
     if (log == NULL)
	  return log;
     for (i = 0; i < sizeof(special_names) / sizeof(*special_names);
	  ++i)
	  if (special_names[i].name == NULL
	      || strcmp (special_names[i].name, fname) == 0) {
	       *log = special_names[i];
	       break;
	  }
     (*log->open)(log, progname, fname);
     return log;
}

void
log_set_mask (Log_method *log, unsigned m)
{
     log->mask = m;
}

unsigned
log_get_mask (Log_method *log)
{
     return log->mask;
}

void
log_vlog(Log_method *log, unsigned level, const char *fmt, va_list args)
{
    if (level & log->mask) {
	if (log->vprint)
	    (*log->vprint)(log, (char *) fmt, args);
	else {
	    char *buf;
	    
	    vasprintf (&buf, fmt, args);
	    (*log->print)(log, buf);
	    
	    free(buf);
	}
    }
}


void
log_log (Log_method *log, unsigned level, const char *fmt, ...)
{
     va_list args;

     va_start (args, fmt);
     log_vlog(log, level, fmt, args);
     va_end (args);
}

void
log_close (Log_method *log)
{
     (*log->close)(log);
     free (log);
}
