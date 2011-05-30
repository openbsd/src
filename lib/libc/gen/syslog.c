/*	$OpenBSD: syslog.c,v 1.31 2011/05/30 18:48:33 martynas Exp $ */
/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

static struct syslog_data sdata = SYSLOG_DATA_INIT;

void	__vsyslog_r(int pri, struct syslog_data *, size_t (*)(char *, size_t),
    const char *, va_list);

static size_t
gettime(char *buf, size_t maxsize)
{
	time_t	now;

	(void)time(&now);
	return (strftime(buf, maxsize, "%h %e %T ", localtime(&now)));
}


/*
 * syslog, vsyslog --
 *	print message on log file; output is intended for syslogd(8).
 */
/* PRINTFLIKE2 */
void
syslog(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(pri, fmt, ap);
	va_end(ap);
}

void
vsyslog(int pri, const char *fmt, va_list ap)
{
	__vsyslog_r(pri, &sdata, &gettime, fmt, ap);
}

void
openlog(const char *ident, int logstat, int logfac)
{
	openlog_r(ident, logstat, logfac, &sdata);
}

void
closelog(void)
{
	closelog_r(&sdata);
}

/* setlogmask -- set the log mask level */
int
setlogmask(int pmask)
{
	return setlogmask_r(pmask, &sdata);
}

/* setlogmask -- set the log mask level */
int
setlogmask_r(int pmask, struct syslog_data *data)
{
	int omask;

	omask = data->log_mask;
	if (pmask != 0)
		data->log_mask = pmask;
	return (omask);
}
