/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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

#include <arla_local.h>
RCSID("$arla: arladeb.c,v 1.29 2002/12/06 04:57:31 lha Exp $");

Log_method* arla_log_method = NULL;
Log_unit* arla_log_unit = NULL;

void
arla_log(unsigned level, const char *fmt, ...)
{
    va_list args;
    
    assert (arla_log_method);
    
    va_start(args, fmt);
    log_vlog(arla_log_unit, level, fmt, args);
    va_end(args);
}


void
arla_loginit(char *logname, log_flags flags)
{
    assert (logname);
    
    arla_log_method = log_open("arla", logname);
    if (arla_log_method == NULL)
	errx (1, "arla_loginit: log_opened failed with log `%s'", logname);
    arla_log_unit = log_unit_init (arla_log_method, "arla", arla_deb_units,
				   ARLA_DEFAULT_LOG);
    if (arla_log_unit == NULL)
	errx (1, "arla_loginit: log_unit_init failed");
    log_setflags (arla_log_method, flags);
}

int
arla_log_set_level (const char *s)
{
    log_set_mask_str (arla_log_method, NULL, s);
    return 0;
}

void
arla_log_set_level_num (unsigned level)
{
    log_set_mask (arla_log_unit, level);
}

void
arla_log_get_level (char *s, size_t len)
{
    log_mask2str (arla_log_method, NULL, s, len);
}

unsigned
arla_log_get_level_num (void)
{
    return log_get_mask (arla_log_unit);
}

/*
 *
 */

void
arla_err (int eval, unsigned level, int error, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    arla_verr (eval, level, error, fmt, args);
    va_end (args);
}

void
arla_verr (int eval, unsigned level, int error, const char *fmt, va_list args)
{
    char *s;

    vasprintf (&s, fmt, args);
    if (s == NULL) {
	log_log (arla_log_unit, level,
		 "Sorry, no memory to print `%s'...", fmt);
	exit (eval);
    }
    log_log (arla_log_unit, level, "%s: %s", s, koerr_gettext (error));
    free (s);
    exit (eval);
}

void
arla_errx (int eval, unsigned level, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    arla_verrx (eval, level, fmt, args);
    va_end (args);
}

void
arla_verrx (int eval, unsigned level, const char *fmt, va_list args)
{
    log_vlog (arla_log_unit, level, fmt, args);
    exit (eval);
}

void
arla_warn (unsigned level, int error, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    arla_vwarn (level, error, fmt, args);
    va_end (args);
}

void
arla_vwarn (unsigned level, int error, const char *fmt, va_list args)
{
    char *s;

    vasprintf (&s, fmt, args);
    if (s == NULL) {
	log_log (arla_log_unit, level,
		 "Sorry, no memory to print `%s'...", fmt);
	return;
    }
    log_log (arla_log_unit, level, "%s: %s", s, koerr_gettext (error));
    free (s);
}

void
arla_warnx (unsigned level, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    arla_vwarnx (level, fmt, args);
    va_end (args);
}

void
arla_vwarnx (unsigned level, const char *fmt, va_list args)
{
    log_vlog (arla_log_unit, level, fmt, args);
}

void
arla_warnx_with_fid (unsigned level, const VenusFid *fid, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    arla_vwarnx_with_fid (level, fid, fmt, args);
    va_end(args);
}

void
arla_vwarnx_with_fid (unsigned level, const VenusFid *fid, const char *fmt,
		      va_list args)
{
    char *s;
    const char *cellname;
    char volname[VLDB_MAXNAMELEN];

    cellname = cell_num2name (fid->Cell);
    if (cellname == NULL)
	cellname = "<unknown>";
    if (volcache_getname (fid->fid.Volume, fid->Cell,
			  volname, sizeof(volname)) != 0)
	strlcpy(volname, "<unknown>", sizeof(volname));

    vasprintf (&s, fmt, args);
    if (s == NULL) {
	log_log (arla_log_unit, level,
		 "Sorry, no memory to print `%s'...", fmt);
	return;
    }
    log_log (arla_log_unit, level,
	     "volume %s (%ld) in cell %s (%ld): %s",
	     volname, (unsigned long)fid->fid.Volume, cellname,
	     (long)fid->Cell, s);
    free (s);
}
