/*	$OpenBSD: log.h,v 1.23 2008/06/10 01:00:34 joris Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

/* log priority levels */
#define LP_NOTICE	0
#define LP_ERR		1
#define LP_ERRNO	2
#define LP_ABORT	3
#define LP_TRACE	4
#define LP_RCS		5

#define LOG_REVSEP \
"----------------------------"

#define LOG_REVEND \
 "============================================================================="

void	cvs_log(u_int, const char *, ...) __attribute__((format(printf, 2, 3)));
void	cvs_vlog(u_int, const char *, va_list);
int	cvs_printf(const char *, ...) __attribute__((format(printf, 1, 2)));
int	cvs_vprintf(const char *, va_list);
void	fatal(const char *, ...) __dead __attribute__((format(printf, 1,2)));

#endif	/* LOG_H */
