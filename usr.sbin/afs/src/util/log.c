/*
 * Copyright (c) 1995 - 2003 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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
RCSID("$arla: log.c,v 1.38 2003/03/13 14:50:58 lha Exp $");
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/time.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#if HAVE_SYSLOG
#include <syslog.h>
#endif /* HAVE_SYSLOG */

#include <assert.h>
#include <roken.h>
#include <err.h>
#include "log.h"

extern char *__progname;

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
    void (*open)(Log_method *, const char *progname, char *fname,
		 char *extra_args);
    void (*vprint)(Log_method *, char *, va_list);
    void (*print)(Log_method *, char *);
    void (*close)(Log_method *);
    union {
	void *v;
	int i;
    } data;
    log_flags flags;
    int num_units;
    struct log_unit **units;
};

struct log_method_file_data {
    FILE *f;
    int flags;
#define LOGFILE_NO_TIME 1
    char *progname;
};

#if HAVE_SYSLOG
static void log_open_syslog (Log_method *lm, const char *progname,
			     char *fname, char *extra_args);

static void log_close_syslog (Log_method *lm);
#if HAVE_VSYSLOG
static void log_vprint_syslog (Log_method *lm, char *, va_list);
#endif /* HAVE_VSYSLOG */
static void log_print_syslog (Log_method *lm, char *);
#endif /* HAVE_SYSLOG */

static void
log_open_stderr (Log_method *lm, const char *progname, char *fname,
		 char *extra_args);

static void
log_open_file (Log_method *lm, const char *progname, char *fname,
	       char *extra_args);

static void
log_close_file (Log_method *lm);

static void
log_print_file (Log_method *lm, char *);

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
                                log_vprint_syslog, log_print_syslog
#else
                                NULL, log_print_syslog
#endif /* HAVE_VSYSLOG */
                                                        , log_close_syslog},
#endif /* HAVE_SYSLOG */
{"/dev/stderr",	log_open_stderr, log_vprint_file, log_print_file, NULL},
{NULL,		log_open_file, log_vprint_file, log_print_file, log_close_file}
/* Should be last */
};

#if HAVE_SYSLOG

struct units syslog_opt_units[] = {
#ifdef LOG_PERROR
    { "stderr", LOG_PERROR },
#endif
#ifdef LOG_NDELAY
    { "no-delay", LOG_NDELAY },
#endif
#ifdef LOG_CONS
    { "console", LOG_CONS },
#endif
#ifdef LOG_PID
    { "pid", LOG_PID },
#endif
    { NULL }
};

struct units syslog_facility_units[] = {
#ifdef LOG_AUTH
    { "auth",	LOG_AUTH },
#endif
#ifdef LOG_AUTHPRIV
    { "authpriv",	LOG_AUTHPRIV },
#endif
#ifdef LOG_CRON
    { "cron", 	LOG_CRON },
#endif
#ifdef LOG_DAEMON
    { "daemon",	LOG_DAEMON },
#endif
#ifdef LOG_FTP
    { "ftp",	LOG_FTP },
#endif
#ifdef LOG_KERN
    { "kern",	LOG_KERN },
#endif
#ifdef LOG_LPR
    { "lpr",	LOG_LPR },
#endif
#ifdef LOG_MAIL
    { "mail",	LOG_MAIL },
#endif
#ifdef LOG_NEWS
    { "news",	LOG_NEWS },
#endif
#ifdef LOG_SYSLOG
    { "syslog",	LOG_SYSLOG },
#endif
#ifdef LOG_USER
    { "user",	LOG_USER },
#endif
#ifdef LOG_UUCP
    { "uucp",	LOG_UUCP },
#endif
#ifdef LOG_LOCAL0
    { "local0",	LOG_LOCAL0 },
#endif
#ifdef LOG_LOCAL1
    { "local1",	LOG_LOCAL1 },
#endif
#ifdef LOG_LOCAL2
    { "local2",	LOG_LOCAL2 },
#endif
#ifdef LOG_LOCAL3
    { "local3",	LOG_LOCAL3 },
#endif
#ifdef LOG_LOCAL4
    { "local4",	LOG_LOCAL4 },
#endif
#ifdef LOG_LOCAL5
    { "local5",	LOG_LOCAL5 },
#endif
#ifdef LOG_LOCAL6
    { "local6",	LOG_LOCAL6 },
#endif
#ifdef LOG_LOCAL7
    { "local7",	LOG_LOCAL7 },
#endif
    { NULL }
};

static void
log_open_syslog (Log_method *lm, const char *progname, char *fname,
		 char *extra_args)
{
    int logopt = LOG_PID | LOG_NDELAY;
    int facility = LOG_DAEMON;
    char *opt = NULL;
    char *facility_str = NULL;

    opt = extra_args;
    if (opt) {
	facility_str = opt;
	strsep (&facility_str, ":");

	logopt = parse_flags (opt, syslog_opt_units, logopt);
	if (logopt < 0) {
	    fprintf (stderr, "log_open: error parsing syslog "
		     "optional flags: %s\n", opt);
	    print_flags_table (syslog_opt_units, stderr);
	    exit (1);
	}
    }
    if (facility_str) {
	struct units *best_match = NULL, *u = syslog_facility_units;
	int len = strlen(facility_str);

	while (u->name) {
	    if (strcasecmp(u->name, facility_str) == 0) {
		best_match = u;
		break;
	    }
	    if (strncasecmp(u->name, facility_str, len) == 0) {
		if (best_match)
		    errx (1, "log_open: log facility %s is ambiguous", 
			  facility_str);
		best_match = u;
	    }
	    u++;
	}
	if (best_match == NULL)
	    errx (1, "log_open: unknown facility %s", facility_str);
	facility = u->mult;
    }

    openlog (progname, logopt, facility);
}

#if HAVE_VSYSLOG

static void
log_vprint_syslog (Log_method *lm, char *fmt, va_list args)
{
     vsyslog (LOG_NOTICE, fmt, args);
}
#endif /* HAVE_VSYSLOG */

static void
log_print_syslog (Log_method *lm, char *str)
{
     syslog (LOG_NOTICE, "%s", str);
}

static void
log_close_syslog (Log_method *lm)
{
     closelog ();
}

#endif /* HAVE_SYSLOG */

static int
file_parse_extra(FILE *f, char *extra_args)
{
    int flags = 0;
    char *str;
    if (extra_args == NULL)
	return 0;
    do {
	if (strlen(extra_args) == 0)
	    return flags;

	str = strsep(&extra_args, ":");
	if (extra_args) {
	    *extra_args = '\0';
	    extra_args++;
	}
	if (strncasecmp(str, "notime", 6) == 0)
	    flags |= LOGFILE_NO_TIME;
	else
	    fprintf (f, "unknown flag: `%s'\n", str);
    } while (extra_args != NULL);
    
    return flags;
}

static void
log_open_file_common(struct log_method_file_data *data, 
		     const char *progname, char *extra_args)
{
    if (progname != NULL)
	data->progname = strdup(progname);
    else
	progname = "unknown-program";
    if (data->progname == NULL)
	data->progname = "out of memory";
    data->flags = file_parse_extra(data->f, extra_args);
}

static void
log_open_stderr (Log_method *lm, const char *progname, char *fname,
		 char *extra_args)
{
     struct log_method_file_data *data;
     data = malloc(sizeof(*data));
     if (data == NULL)
	 errx (1, "log_open_stderr: failed to malloc");
     lm->data.v = data;

     data->f = stderr;
     log_open_file_common(data, progname, extra_args);
}

static void
log_open_file (Log_method *lm, const char *progname, char *fname,
	       char *extra_args)
{
     struct log_method_file_data *data;
     data = malloc(sizeof(*data));
     if (data == NULL)
	 errx (1, "log_open_stderr: failed to malloc");
     lm->data.v = data;

     data->f = fopen (fname, "a");
     if (data->f == NULL)
	  data->f = stderr;
     log_open_file_common(data, progname, extra_args);
}

static void
log_printf_file(Log_method *lm, char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    log_vprint_file(lm, fmt, args);
    va_end (args);
}

static void
log_print_file(Log_method *lm, char *str)
{
    log_printf_file(lm, "%s", str);
}

static void
log_vprint_file (Log_method *lm,  char *fmt, va_list args)
{
    struct timeval tv = { 0, 0 };
    char time[128];
    time_t t;
    struct log_method_file_data *data = lm->data.v;
    FILE *f = data->f;

    if ((data->flags & LOGFILE_NO_TIME) == 0) {
	struct tm tm;
	gettimeofday(&tv, NULL);
	t = tv.tv_sec;
	strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S %Z", 
		 localtime_r(&t, &tm));
	time[sizeof(time)-1] = '\0';
	fprintf (f, "%s: ", time);
    }

    fprintf (f, "%s: ", data->progname);
    vfprintf (f, fmt, args);
    putc ('\n', f);
    fflush (f);
}

static void
log_close_file (Log_method *lm)
{
    struct log_method_file_data *data = lm->data.v;
    fclose(data->f);
    free (data);
}

Log_method *
log_open (const char *progname, char *fname)
{
     int i;
     Log_method *logm;
     char *name, *extra;

     name = strdup(fname);
     if (name == NULL)
	 return NULL;

     logm = (Log_method *)malloc (sizeof(Log_method));
     if (logm == NULL) {
	 free (name);
	 return logm;
     }
     for (i = 0; i < sizeof(special_names) / sizeof(*special_names);
	  ++i) {
	 int len = 0;
	 if (special_names[i].name)
	     len = strlen(special_names[i].name);
	 if (special_names[i].name == NULL
	     || (strncmp (special_names[i].name, fname, len) == 0 &&
		 (special_names[i].name[len] == '\0'
		  || special_names[i].name[len] == ':'))) {
	     *logm = special_names[i];
	     break;
	 }
     }
     extra = name;
     strsep(&extra, ":");
     logm->num_units = 0;
     logm->units = NULL;
     (*logm->open)(logm, progname, name, extra);
     free (name);
     return logm;
}

log_flags
log_setflags(Log_method *method, log_flags flags)
{
    log_flags oldflags;

    oldflags = method->flags;
    method->flags = flags;
    return oldflags;
}

log_flags
log_getflags(Log_method *method)
{
    return method->flags;
}

void
log_set_mask (Log_unit *logu, unsigned m)
{
     logu->mask = m;
}

unsigned
log_get_mask (Log_unit *unit)
{
     return unit->mask;
}

static void
_internal_vlog (Log_method *method, const char *fmt, va_list args)
{
    if (method->vprint && (method->flags & LOG_CPU_USAGE) == 0)
	    (*method->vprint)(method, (char *) fmt, args);
    else {
	char *buf;
	
	vasprintf (&buf, fmt, args);

	if (buf != NULL) {
#ifdef HAVE_GETRUSAGE
	    if (method->flags & LOG_CPU_USAGE) {
		struct rusage usage;
		int ret;
		char *rbuf = NULL;
		
		ret = getrusage(RUSAGE_SELF, &usage);
		if (ret == 0) {
		    asprintf(&rbuf, "s: %d.%d u: %d.%d",
			     (int)usage.ru_stime.tv_sec, 
			     (int)usage.ru_stime.tv_usec,
			     (int)usage.ru_utime.tv_sec,
			     (int)usage.ru_utime.tv_usec);
		    if (rbuf) {
			char *buf2;
			
			asprintf(&buf2, "%s %s", buf, rbuf);
			if (buf2) {
			    free(buf);
			    buf = buf2;
			}
			free(rbuf);
		    }
		}
	    }
#endif /* HAVE_GETRUSAGE */
	    (*method->print)(method, buf);
	} else
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
log_log (Log_unit *logu, unsigned level, const char *fmt, ...)
{
    va_list args;
    
    va_start (args, fmt);
    log_vlog(logu, level, fmt, args);
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
log_unit_free (Log_method *method, Log_unit *logu)
{
    Log_unit **list;
    int i;

    for (i = 0; i < method->num_units; i++)
	if (logu == method->units[method->num_units])
	    break;
    if (i < method->num_units - 1)
	memmove (&method->units[i], &method->units[i+1],
		 method->num_units - i);

    method->num_units -= 1;
    list = realloc (method->units, method->num_units * sizeof(Log_unit *));
    if (list == NULL)
	abort();
    method->units = list;

    free (logu->name);
    assert (logu->method == method);
    logu->name = NULL;
    logu->unit = NULL;
    logu->mask = 0;
    free (logu);
}

static int
parse_word (Log_method *m, char **str, Log_unit **u, char **log_str)
{
    int j;
    char *first;

    if (**str == '\0') return 1;
    while (**str != '\0' && (isspace((unsigned char)**str) || **str == ';'))
	(*str)++;
    if (**str == '\0') return 1;

    first = *str;
    while (**str != '\0' && !isspace((unsigned char)**str) && **str != ':')
	(*str)++;
    if (**str == ':') {
	int best_fit = -1;
	int str_len;
	**str = '\0';
	(*str)++;
	str_len = strlen(first);
	for (j = 0; j < m->num_units; j++) {
	    if (strcasecmp(m->units[j]->name, first) == 0)
		break;
	    if (strncasecmp(m->units[j]->name, first, str_len) == 0) {
		if (best_fit != -1)
		    return 1;
		best_fit = j;
	    }
	}
	if (j == m->num_units) {
	    if (best_fit != -1)
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

static int
unit_parse_flags (const char *log_str, struct log_unit *unit)
{
    int ret;
    ret = parse_flags (log_str, unit->unit, log_get_mask(unit));
    if (ret < 0)
	return ret;
    log_set_mask (unit, ret);
    return 0;
}

void
log_set_mask_str (Log_method *method, Log_unit *default_unit, const char *str)
{
    char *log_str, *ptr, *str2;
    Log_unit *unit = NULL;
    int ret;

    str2 = ptr = estrdup (str);
    while (parse_word (method, &ptr, &unit, &log_str) == 0) {
	if (unit || default_unit) {
	    if ((unit && default_unit) && unit != default_unit)
		_internal_log (method,
			       "log_set_mask_str: default with unit string"
			       "%s:%s", unit->name, log_str);
	    if (unit == NULL)
		unit = default_unit;
	    ret = unit_parse_flags (log_str, unit);
	    if (ret)
		_internal_log (unit->method,
			       "log error parsing: %s:%s\n", 
			       unit->name, log_str);
	    unit = NULL;
	} else {
	    int i;
	    ret = 1;
	    /* If something matches, be merry */
	    for (i = 0; i < method->num_units; i++) {
		if (unit_parse_flags (log_str, method->units[i]) != -1)
		    ret = 0;
	    }
	    if (ret)
		_internal_log (method,
			       "log error parsing: %s\n", 
			       log_str);
	}
    }
    free (str2);
}

#define UPDATESZ(str,len,update) \
	do { (str) += (update); (len) -= min((len),(update)); } while (0)

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

    if (sz) buf[0] = '\0';

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
