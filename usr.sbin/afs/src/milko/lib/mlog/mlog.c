/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <parse_units.h>
#include <roken.h>
#include <err.h>
#include "ko.h"

#include "mlog.h"

RCSID("$arla: mlog.c,v 1.8 2000/10/03 00:17:58 lha Exp $");

static Log_method* mlog_log_method = NULL;
static Log_unit* mlog_log_unit = NULL;
static struct units *mlog_deb_units = NULL;

void
mlog_log(unsigned level, char *fmt, ...)
{
    va_list args;
    
    assert (mlog_log_unit);
    
    va_start(args, fmt);
    log_vlog(mlog_log_unit, level, fmt, args);
    va_end(args);
}

void
mlog_loginit(Log_method *method,
	     struct units *deb_units,
	     unsigned default_level)
{
    assert (deb_units && method && mlog_deb_units == NULL);

    mlog_deb_units = deb_units;
    mlog_log_method = method;
    mlog_log_unit = log_unit_init (mlog_log_method, "milko",
				   mlog_deb_units, default_level);
    if (mlog_log_unit == NULL)
	errx (1, "mlog_loginit: log_unit_init failed");
}

void
mlog_log_set_level (const char *s)
{
    log_set_mask_str (mlog_log_method, NULL, s);
}

void
mlog_log_set_level_num (unsigned level)
{
    log_set_mask (mlog_log_unit, level);
}

void
mlog_log_get_level (char *s, size_t len)
{
    log_mask2str (mlog_log_method, NULL, s, len);
}

unsigned
mlog_log_get_level_num (void)
{
    return log_get_mask (mlog_log_unit);
}

void
mlog_log_print_levels (FILE *f)
{
    print_flags_table (mlog_deb_units, f);
}

/*
 *
 */

void
mlog_err (int eval, unsigned level, int error, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    mlog_verr (eval, level, error, fmt, args);
    va_end (args);
}

void
mlog_verr (int eval, unsigned level, int error, const char *fmt, va_list args)
{
    char *s;

    vasprintf (&s, fmt, args);
    if (s == NULL) {
	log_log (mlog_log_unit, level,
		 "Sorry, no memory to print `%s'...", fmt);
	exit (eval);
    }
    log_log (mlog_log_unit, level, "%s: %s", s, koerr_gettext (error));
    free (s);
    exit (eval);
}

void
mlog_errx (int eval, unsigned level, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    mlog_verrx (eval, level, fmt, args);
    va_end (args);
}

void
mlog_verrx (int eval, unsigned level, const char *fmt, va_list args)
{
    log_vlog (mlog_log_unit, level, fmt, args);
    exit (eval);
}

void
mlog_warn (unsigned level, int error, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    mlog_vwarn (level, error, fmt, args);
    va_end (args);
}

void
mlog_vwarn (unsigned level, int error, const char *fmt, va_list args)
{
    char *s;

    vasprintf (&s, fmt, args);
    if (s == NULL) {
	log_log (mlog_log_unit, level,
		 "Sorry, no memory to print `%s'...", fmt);
	return;
    }
    log_log (mlog_log_unit, level, "%s: %s", s, koerr_gettext (error));
    free (s);
}

void
mlog_warnx (unsigned level, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    mlog_vwarnx (level, fmt, args);
    va_end (args);
}

void
mlog_vwarnx (unsigned level, const char *fmt, va_list args)
{
    log_vlog (mlog_log_unit, level, fmt, args);
}
