/*	$OpenBSD: log.h,v 1.13 2005/12/19 18:21:31 xsa Exp $	*/
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

#include <sys/types.h>

#include <stdarg.h>

/* log destinations */
#define LD_STD		0x01
#define LD_SYSLOG	0x02
#define LD_CONS		0x04

#define LD_ALL		(LD_STD|LD_SYSLOG|LD_CONS)

/* log flags */
#define LF_PID		0x01	/* include PID in messages */


/* log priority levels */
#define LP_DEBUG	0
#define LP_INFO		1
#define LP_NOTICE	2
#define LP_WARN		3
#define LP_ERR		4
#define LP_ALERT	5
#define LP_ERRNO	6
#define LP_ABORT	7
#define LP_TRACE	8

#define LP_MAX		8
#define LP_ALL		255

/* filtering methods */
#define LP_FILTER_SET	0	/* set a filter */
#define LP_FILTER_UNSET	1	/* remove a filter */

int	cvs_log_init(u_int, u_int);
void	cvs_log_cleanup(void);
int	cvs_log_filter(u_int, u_int);
int	cvs_log(u_int, const char *, ...);
int	cvs_vlog(u_int, const char *, va_list);
int	cvs_printf(const char *, ...);
void	cvs_putchar(int);
void	fatal(const char *, ...);

#endif	/* LOG_H */
