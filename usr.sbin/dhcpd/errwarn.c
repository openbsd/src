/*	$OpenBSD: errwarn.c,v 1.10 2017/02/11 16:12:36 krw Exp $	*/

/* Errors and warnings... */

/*
 * Copyright (c) 1996 The Internet Software Consortium.
 * All Rights Reserved.
 * Copyright (c) 1995 RadioMail Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of RadioMail Corporation, the Internet Software
 *    Consortium nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RADIOMAIL CORPORATION, THE INTERNET
 * SOFTWARE CONSORTIUM AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL RADIOMAIL CORPORATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software was written for RadioMail Corporation by Ted Lemon
 * under a contract with Vixie Enterprises.   Further modifications have
 * been made for the Internet Software Consortium under a contract
 * with Vixie Laboratories.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "dhcp.h"
#include "tree.h"
#include "dhcpd.h"

char mbuf[1024];
char fbuf[1024];

int warnings_occurred;

/*
 * Log an error message, then exit.
 */
void
error(char *fmt, ...)
{
	va_list list;

	do_percentm(fbuf, sizeof(fbuf), fmt);

	va_start(list, fmt);
	vsnprintf(mbuf, sizeof(mbuf), fbuf, list);
	va_end(list);

	/* Also log it to stderr? */
	if (log_perror) {
		write(STDERR_FILENO, mbuf, strlen(mbuf));
		write(STDERR_FILENO, "\n", 1);
	} else
		syslog_r(log_priority | LOG_ERR, &sdata, "%s", mbuf);

	if (log_perror) {
		fprintf(stderr, "exiting.\n");
		fflush(stderr);
	} else
		syslog_r(LOG_CRIT, &sdata, "exiting.");

	exit(1);
}

/*
 * Log a warning message...
 */
int
warning(char *fmt, ...)
{
	va_list list;

	do_percentm(fbuf, sizeof(fbuf), fmt);

	va_start(list, fmt);
	vsnprintf(mbuf, sizeof(mbuf), fbuf, list);
	va_end(list);

	if (log_perror) {
		write(STDERR_FILENO, mbuf, strlen(mbuf));
		write(STDERR_FILENO, "\n", 1);
	} else
		syslog_r(log_priority | LOG_ERR, &sdata, "%s", mbuf);

	return (0);
}

/*
 * Log a note...
 */
int
note(char *fmt, ...)
{
	va_list list;

	do_percentm(fbuf, sizeof(fbuf), fmt);

	va_start(list, fmt);
	vsnprintf(mbuf, sizeof(mbuf), fbuf, list);
	va_end(list);

	if (log_perror) {
		write(STDERR_FILENO, mbuf, strlen(mbuf));
		write(STDERR_FILENO, "\n", 1);
	} else
		syslog_r(log_priority | LOG_INFO, &sdata, "%s", mbuf);

	return (0);
}

/*
 * Log a debug message...
 */
int
debug(char *fmt, ...)
{
	va_list list;

	do_percentm(fbuf, sizeof(fbuf), fmt);

	va_start(list, fmt);
	vsnprintf(mbuf, sizeof(mbuf), fbuf, list);
	va_end(list);

	if (log_perror) {
		write(STDERR_FILENO, mbuf, strlen(mbuf));
		write(STDERR_FILENO, "\n", 1);
	} else
		syslog_r(log_priority | LOG_DEBUG, &sdata, "%s", mbuf);

	return (0);
}

/*
 * Find %m in the input string and substitute an error message string.
 */
void
do_percentm(char *obuf, size_t size, char *ibuf)
{
	char ch;
	char *s = ibuf;
	char *t = obuf;
	size_t prlen;
	size_t fmt_left;
	int saved_errno = errno;

	/*
	 * We wouldn't need this mess if printf handled %m, or if
	 * strerror() had been invented before syslog_r().
	 */
	for (fmt_left = size; (ch = *s); ++s) {
		if (ch == '%' && s[1] == 'm') {
			++s;
			prlen = snprintf(t, fmt_left, "%s",
			    strerror(saved_errno));
			if (prlen == -1)
				prlen = 0;
			if (prlen >= fmt_left)
				prlen = fmt_left - 1;
			t += prlen;
			fmt_left -= prlen;
		} else {
			if (fmt_left > 1) {
				*t++ = ch;
				fmt_left--;
			}
		}
	}
	*t = '\0';
}
