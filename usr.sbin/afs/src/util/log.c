/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
RCSID("$Id: log.c,v 1.4 2000/09/11 14:41:39 art Exp $");
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <roken.h>
#include "log.h"

/*
 * The structure for each logging method.
 */

struct log_method;

struct log_unit {
    struct log_method *method;
    char *name;
    const struct units *unit;
    unsigned mask;
};

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
    int num_units;
    struct log_unit **units;
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
     syslog (LOG_INFO, "%s", str);
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
    FILE *f = (FILE *)lm->data.v;

    gettimeofday(&tv, NULL);
    t = tv.tv_sec;
    time = strdup(ctime(&t));
    if (time) {
	time[strlen(time)-1] = '\0';
	fprintf (f, "%s: ", time);
	free(time);
    } else
	fprintf (f, "unknown time:");

    fprintf (f, "%s: ", __progname);
    vfprintf (f, fmt, args);
    putc ('\n', f);
    fflush (f);
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
     if (log == NULL)
	 return NULL;
     log->num_units = 0;
     log->units = NULL;
     (*log->open)(log, progname, fname);
     return log;
}

void
log_set_mask (Log_unit *log, unsigned m)
{
     log->mask = m;
}

unsigned
log_get_mask (Log_unit *unit)
{
     return unit->mask;
}

static void
_internal_vlog (Log_method *method, const char *fmt, va_list args)
{
    if (method->vprint)
	    (*method->vprint)(method, (char *) fmt, args);
    else {
	char *buf;
	
	vasprintf (&buf, fmt, args);
	if (buf != NULL)
	    (*method->print)(method, buf);
	else
	    (*method->print)(method, "not enough memory to print");
	free(buf);
    }
}

static void
_internal_log (Log_method *method, const char *fmt, ...)
{
    va_list args;
    
    va_start (args, fmt);
    _internal_vlog(method, fmt, args);
    va_end (args);
}

void
log_vlog(Log_unit *unit, unsigned level, const char *fmt, va_list args)
{
    if (level & unit->mask)
	_internal_vlog (unit->method, fmt, args);
}


void
log_log (Log_unit *log, unsigned level, const char *fmt, ...)
{
    va_list args;
    
    va_start (args, fmt);
    log_vlog(log, level, fmt, args);
    va_end (args);
}

void
log_close (Log_method *method)
{
    int i;
    if (method->close)
	(*method->close)(method);
    for (i = 0 ; i < method->num_units; i++)
	log_unit_free (method, method->units[i]);
    free (method->units);
    method->units = NULL;
    free (method);
}

Log_unit *
log_unit_init (Log_method *method, const char *name, struct units *unit,
	       unsigned long default_mask)
{
    Log_unit *u, **list;
    
    u = malloc (sizeof(Log_unit));
    if (u == NULL)
	return NULL;
    list = realloc (method->units,
		    (method->num_units + 1) * sizeof(Log_unit *));
    if (list == NULL) {
	free (u);
	return NULL;
    }
    method->units = list;
    method->units[method->num_units] = u;
    method->num_units += 1;

    u->method = method;
    u->name   = estrdup (name);
    u->unit   = unit;
    u->mask   = default_mask;
    return u;
}

void
log_unit_free (Log_method *method, Log_unit *log)
{
    Log_unit **list;
    int i;

    for (i = 0; i < method->num_units; i++)
	if (log == method->units[method->num_units])
	    break;
    if (i < method->num_units - 1)
	memmove (&method->units[i], &method->units[i+1],
		 method->num_units - i);

    method->num_units -= 1;
    list = realloc (method->units, method->num_units * sizeof(Log_unit *));
    if (list == NULL)
	abort();
    method->units = list;

    free (log->name);
    assert (log->method == method);
    log->name = NULL;
    log->unit = NULL;
    log->mask = 0;
    free (log);
}

static int
parse_word (Log_method *m, char **str, Log_unit **u, char **log_str)
{
    int j;
    char *first;

    if (**str == '\0') return 1;
    while (**str != '\0' && isspace(**str) && **str == ';')
	(*str)++;
    if (**str == '\0') return 1;

    first = *str;
    while (**str != '\0' && !isspace(**str) && **str != ':')
	(*str)++;
    if (**str == ':') {
	int best_fit = m->num_units;
	**str = '\0';
	(*str)++;
	for (j = 0; j < m->num_units; j++)
	    if (strcasecmp(m->units[j]->name, first) == 0)
		break;
	if (j == m->num_units) {
	    if (best_fit != m->num_units)
		*u = m->units[best_fit];
	    else
		return 1;
	} else
	    *u = m->units[j];
	*log_str = *str;
    } else {
	*u = NULL;
	*log_str = first;
    }
    while (**str != '\0' && **str != ';')
	(*str)++;
    if (**str == '\0')
	return 0;
    **str = '\0';
    (*str)++;
    return 0;
}

static void
unit_parse_flags (const char *log_str, struct log_unit *unit)
{
    int ret;
    ret = parse_flags (log_str, unit->unit, log_get_mask(unit));
    if (ret < 0)
	_internal_log (unit->method,
		       "log internal error parsing: %s\n", 
		       log_str);
    else
	log_set_mask (unit, ret);
}

void
log_set_mask_str (Log_method *method, Log_unit *default_unit, const char *str)
{
    char *log_str, *ptr, *str2;
    Log_unit *unit = NULL;

    str2 = ptr = estrdup (str);
    while (parse_word (method, &ptr, &unit, &log_str) == 0) {
	if (unit || default_unit) {
	    if ((unit && default_unit) && unit != default_unit)
		_internal_log (method,
			       "log_set_mask_str: default with unit string"
			       "%s:%s", unit->name, log_str);
	    if (unit == NULL)
		unit = default_unit;
	    unit_parse_flags (log_str, unit);
	    unit = NULL;
	} else {
	    int i;
	    for (i = 0; i < method->num_units; i++) 
		unit_parse_flags (log_str, method->units[i]);
	}
    }
    free (str2);
}

#define UPDATESZ(str,len,update) \
	do { (str) += (update); (len) -= (update); } while (0)

static size_t
_print_unit (Log_unit *unit, char *buf, size_t sz)
{
    size_t ret, orig_sz = sz;
    ret = snprintf (buf, sz, "%s:", unit->name);
    UPDATESZ(buf,sz,ret);
    ret = unparse_flags (log_get_mask (unit), unit->unit, buf, sz);
    UPDATESZ(buf,sz,ret);
    return orig_sz - sz;
}

size_t
log_mask2str (Log_method *method, Log_unit *unit, char *buf, size_t sz)
{
    size_t ret, orig_sz = sz;
    int i, printed = 0;

    if (unit)
	return _print_unit (unit, buf, sz);
    
    for (i = 0; i < method->num_units; i++) {
	if (log_get_mask (method->units[i])) {
	    if (printed) {
		ret = snprintf (buf, sz, ";");
		UPDATESZ(buf,sz,ret);
	    }
	    ret = _print_unit (method->units[i], buf, sz);
	    UPDATESZ(buf,sz,ret);
	    printed = 1;
	}
    }
    return orig_sz - sz;
}

#undef UPDATESZ
