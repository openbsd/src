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

/*
 * $arla: mlog.h,v 1.4 2000/10/03 00:18:03 lha Exp $
 */

#ifndef _arladeb_h
#define _arladeb_h

#include <stdio.h>
#include <stdarg.h>
#include <log.h>

#include <roken.h>
#include <parse_units.h>

void mlog_log(unsigned level, char *fmt, ...);
void mlog_loginit(Log_method *method, struct units *deb_units, 
		  unsigned default_level);
void mlog_log_set_level (const char *s);
void mlog_log_set_level_num (unsigned level);
void mlog_log_get_level (char *s, size_t len);
unsigned mlog_log_get_level_num (void);
void mlog_log_print_levels (FILE *f);

void
mlog_err (int eval, unsigned level, int error, const char *fmt, ...)
__attribute__ ((noreturn))
__attribute__ ((format (printf, 4, 5)))
;

void
mlog_verr (int eval, unsigned level, int error, const char *fmt, va_list args)
__attribute__ ((noreturn))
__attribute__ ((format (printf, 4, 0)))
;

void
mlog_errx (int eval, unsigned level, const char *fmt, ...)
__attribute__ ((noreturn))
__attribute__ ((format (printf, 3, 4)))
;

void
mlog_verrx (int eval, unsigned level, const char *fmt, va_list args)
__attribute__ ((noreturn))
__attribute__ ((format (printf, 3, 0)))
;

void
mlog_warn (unsigned level, int error, const char *fmt, ...)
__attribute__ ((format (printf, 3, 4)))
;

void
mlog_vwarn (unsigned level, int error, const char *fmt, va_list args)
__attribute__ ((format (printf, 3, 0)))
;

void
mlog_warnx (unsigned level, const char *fmt, ...)
__attribute__ ((format (printf, 2, 3)))
;

void
mlog_vwarnx (unsigned level, const char *fmt, va_list args)
__attribute__ ((format (printf, 2, 0)))
;

void
mlog_warnx_with_fid (unsigned level, const VenusFid *fid, const char *fmt, ...)
__attribute__ ((format (printf, 3, 4)));

void
mlog_vwarnx_with_fid (unsigned level, const VenusFid *fid, const char *fmt,
		      va_list args)
__attribute__ ((format (printf, 3, 0)));

#endif				       /* _arladeb_h */
