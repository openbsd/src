/*	$OpenBSD: log.c,v 1.45 2008/06/12 16:53:12 joris Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
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

#include <errno.h>
#include <string.h>

#include "cvs.h"

extern char *__progname;
static int send_m = 1;

/*
 * cvs_log()
 *
 * Log the format-string message
 * The <fmt> argument should not have a terminating newline, as this is taken
 * care of by the logging facility.
 */
void
cvs_log(u_int level, const char *fmt, ...)
{
	va_list vap;

	va_start(vap, fmt);
	cvs_vlog(level, fmt, vap);
	va_end(vap);
}

/*
 * cvs_vlog()
 *
 * The <fmt> argument should not have a terminating newline, as this is taken
 * care of by the logging facility.
 */
void
cvs_vlog(u_int level, const char *fmt, va_list vap)
{
	int ecp;
	FILE *out;
	char *cmdname;

	if (cvs_trace != 1 && level == LP_TRACE)
		return;

	if (level == LP_ERRNO)
		ecp = errno;
	else
		ecp = 0;

	if (level == LP_NOTICE)
		out = stdout;
	else
		out = stderr;

	if (cvs_server_active) {
		if (out == stdout)
			putc('M', out);
		else {
			out = stdout;
			putc('E', out);
		}

		putc(' ', out);
	}

	cmdname = (cmdp != NULL) ? cmdp->cmd_name : __progname;

	/* The cvs program appends the command name to the program name */
	if (level == LP_TRACE) {
		if (cvs_server_active)
			putc('S', out);
		else
			putc('C', out);
		(void)fputs("-> ", out);
	} else if (level != LP_RCS) {
		(void)fputs(__progname, out);
		putc(' ', out);
		if (level == LP_ABORT)
			(void)fprintf(out,
			    "[%s aborted]", cmdname);
		else
			(void)fputs(cmdname, out);
		(void)fputs(": ", out);
	}

	(void)vfprintf(out, fmt, vap);
	if (level == LP_ERRNO) {
		(void)fprintf(out, ": %s\n", strerror(ecp));

		/* preserve it just in case we changed it? */
		errno = ecp;
	} else
		fputc('\n', out);
}

/*
 * cvs_printf()
 *
 * Wrapper function around printf() that prepends a 'M' command when
 * the program is acting as server.
 */
int
cvs_printf(const char *fmt, ...)
{
	int ret;
	char *nstr, *dp, *sp;
	va_list vap;

	va_start(vap, fmt);

	ret = vasprintf(&nstr, fmt, vap);
	if (ret == -1)
		fatal("cvs_printf: could not allocate memory");

	for (dp = nstr; *dp != '\0';) {
		sp = strchr(dp, '\n');
		if (sp == NULL)
			for (sp = dp; *sp != '\0'; sp++)
				;

		if (cvs_server_active && send_m) {
			send_m = 0;
			putc('M', stdout);
			putc(' ', stdout);
		}

		if (dp != nstr && dp != sp &&
		    !strncmp(dp, LOG_REVSEP, sp - dp))
			putc('>', stdout);

		fwrite(dp, sizeof(char), (size_t)(sp - dp), stdout);

		if (*sp != '\n')
			break;

		putc('\n', stdout);
		send_m = 1;
		dp = sp + 1;
	}

	xfree(nstr);
	va_end(vap);

	return (ret);
}
