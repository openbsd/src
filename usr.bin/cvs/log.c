/*	$OpenBSD: log.c,v 1.6 2004/12/06 21:03:12 deraadt Exp $	*/
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

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <syslog.h>

#include "log.h"
#include "cvs.h"

extern char *__progname;


#ifdef unused
static char *cvs_log_levels[] = {
	"debug",
	"info",
	"notice",
	"warning",
	"error",
	"alert",
	"error",
	"abort",
};
#endif

static int cvs_slpriomap[] = {
	LOG_DEBUG,
	LOG_INFO,
	LOG_NOTICE,
	LOG_WARNING,
	LOG_ERR,
	LOG_ALERT,
	LOG_ERR,
	LOG_ERR,
};



static u_int cvs_log_dest = LD_STD;
static u_int cvs_log_flags = 0;

static u_int cvs_log_filters[LP_MAX + 1];
#define NB_FILTERS  sizeof(cvs_log_filters)/sizeof(cvs_log_filters[0])


static struct syslog_data cvs_sl;


/*
 * cvs_log_init()
 *
 * Initialize the logging facility of the server.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_log_init(u_int dest, u_int flags)
{
	int slopt;

	cvs_log_dest = dest;
	cvs_log_flags = flags;

	/* by default, filter only LP_DEBUG and LP_INFO levels */
	memset(cvs_log_filters, 0, sizeof(cvs_log_filters));
	cvs_log_filters[LP_DEBUG] = 1;
	cvs_log_filters[LP_INFO] = 1;

	if (dest & LD_SYSLOG) {
		slopt = 0;

		if (dest & LD_CONS)
			slopt |= LOG_CONS;
		if (flags & LF_PID)
			slopt |= LOG_PID;

		openlog_r(__progname, slopt, LOG_DAEMON, &cvs_sl);
	}

	return (0);
}


/*
 * cvs_log_cleanup()
 *
 * Cleanup the logging facility.
 */

void
cvs_log_cleanup(void)
{
	closelog_r(&cvs_sl);

}


/*
 * cvs_log_filter()
 *
 * Apply or remove filters on the logging facility.  The exact operation is
 * specified by the <how> and <level> arguments.  The <how> arguments tells
 * how the filters will be affected, and <level> gives the log levels that
 * will be affected by the change.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_log_filter(u_int how, u_int level)
{
	u_int i;

	if ((level > LP_MAX) && (level != LP_ALL)) {
		cvs_log(LP_ERR, "invalid log level for filter");
		return (-1);
	}

	switch (how) {
	case LP_FILTER_SET:
		if (level == LP_ALL)
			for (i = 0; i < NB_FILTERS; i++)
				cvs_log_filters[i] = 1;
		else
			cvs_log_filters[level] = 1;
		break;
	case LP_FILTER_UNSET:
		if (level == LP_ALL)
			for (i = 0; i < NB_FILTERS; i++)
				cvs_log_filters[i] = 0;
		else
			cvs_log_filters[level] = 0;
		break;
	case LP_FILTER_TOGGLE:
		if (level == LP_ALL)
			for (i = 0; i < NB_FILTERS; i++)
				cvs_log_filters[i] =
				    (cvs_log_filters[i] == 0) ? 1 : 0;
		else
			cvs_log_filters[level] =
			    (cvs_log_filters[level] == 0) ? 1 : 0;
		break;
	default:
		return (-1);
	}

	return (0);
}


/*
 * cvs_log()
 *
 * Log the format-string message
 * The <fmt> argument should not have a terminating newline, as this is taken
 * care of by the logging facility.
 */

int
cvs_log(u_int level, const char *fmt, ...)
{
	int ret;
	va_list vap;

	va_start(vap, fmt);
	ret = cvs_vlog(level, fmt, vap);
	va_end(vap);

	return (ret);
}


/*
 * cvs_vlog()
 *
 * The <fmt> argument should not have a terminating newline, as this is taken
 * care of by the logging facility.
 */

int
cvs_vlog(u_int level, const char *fmt, va_list vap)
{
	int ecp;
	char prefix[64], buf[1024], ebuf[32];
	FILE *out;

	if (level > LP_MAX)
		return (-1);


	/* apply any filters */
	if (cvs_log_filters[level] != 0)
		return (0);

	if (level == LP_ERRNO)
		ecp = errno;
	else
		ecp = 0;

#ifdef CVS
	/* The cvs program appends the command name to the program name */
	if (cvs_command != NULL) {
		if (level == LP_ABORT)
			snprintf(prefix, sizeof(prefix), "%s [%s aborted]",
			    __progname, cvs_command);
		else
			snprintf(prefix, sizeof(prefix), "%s %s", __progname,
			    cvs_command);
	} else /* just use the standard strlcpy */
#endif
		strlcpy(prefix, __progname, sizeof(prefix));

	if (cvs_log_flags & LF_PID) {
		snprintf(buf, sizeof(buf), "[%d]", (int)getpid());
		strlcat(prefix, buf, sizeof(prefix));
	}

	vsnprintf(buf, sizeof(buf), fmt, vap);
	if (level == LP_ERRNO) {
		snprintf(ebuf, sizeof(ebuf), ": %s", strerror(errno));
		strlcat(buf, ebuf, sizeof(buf));
	}

	if (cvs_log_dest & LD_STD) {
		if (level <= LP_NOTICE)
			out = stdout;
		else
			out = stderr;

#ifdef CVS
		if (cvs_cmdop == CVS_OP_SERVER) {
			if (out == stdout)
				putc('M', out);
			else
				putc('E', out);
			putc(' ', out);
		}
#endif

		fprintf(out, "%s: %s\n", prefix, buf);
	}

	if (cvs_log_dest & LD_SYSLOG)
		syslog_r(cvs_slpriomap[level], &cvs_sl, "%s", buf);

	/* preserve it just in case we changed it? */
	if (level == LP_ERRNO)
		errno = ecp;

	return (0);
}


/*
 * cvs_printf()
 *
 * Wrapper function around printf() that prepends a 'M' or 'E' command when
 * the program is acting as server.
 */

int
cvs_printf(const char *fmt, ...)
{
	int ret;
	va_list vap;

	va_start(vap, fmt);
	ret = vprintf(fmt, vap);
	va_end(vap);

	return (ret);
}
