/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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

/* $arla: log.h,v 1.12 2003/01/10 12:38:44 lha Exp $ */

#ifndef _ARLAUTIL_LOG_H
#define _ARLAUTIL_LOG_H 1

#include <stdarg.h>
#include <parse_units.h>

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

typedef enum {
    LOG_CPU_USAGE = 1
} log_flags;

typedef struct log_method Log_method;
typedef struct log_unit Log_unit;

/*
 * Functions for handling logging
 */

Log_method *log_open (const char *progname, char *fname);
/* Starting logging to `fname'.  Label all messages as coming from
 * `progname'. */

void log_close (Log_method *logm);

Log_unit *log_unit_init (Log_method *method, const char *name,
			 struct units *lognames,
			 unsigned long default_mask);

void log_unit_free (Log_method *method, Log_unit *logu);

log_flags log_setflags(Log_method *logm, log_flags flags);
log_flags log_getflags(Log_method *logm);

void log_log (Log_unit *logu, unsigned level, const char *fmt, ...)
__attribute__((format (printf, 3, 4)))
;

void log_vlog(Log_unit *logu, unsigned level, const char *fmt, va_list args)
__attribute__((format (printf, 3, 0)))
;

unsigned log_get_mask (Log_unit *logu);

void log_set_mask (Log_unit *logu, unsigned mask);

void log_set_mask_str (Log_method *method, Log_unit *default_unit,
		       const char *str);

size_t log_mask2str (Log_method *method, Log_unit *unit, char *buf, size_t sz);

#endif /* _ARLAUTIL_LOG_H */
