/*	$KAME: libaltq2.c,v 1.3 2001/08/16 10:39:16 kjc Exp $	*/
/*
 * Copyright (C) 1997-2000
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * this file contains functions and variables needed to use libaltq.
 * since these are defined in rsvpd, they should be separated in order
 * to link libaltq to rsvpd.
 */
#include <sys/param.h>

#include <altq/altq.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>

#include "altq_qop.h"

/* from rsvp_main.c */
char *altqconfigfile = "/etc/altq.conf";

/* from rsvp_global.h */
int	if_num;		/* number of phyints */
int	m_debug;	/* Debug output control bits */
int	l_debug;	/* Logging severity level */

int daemonize = 1;

/* taken from rsvp_debug.c and modified. */
void
log_write(int severity, int syserr, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	if (severity <= l_debug) {
		if (!daemonize) {
			vfprintf(stderr, format, ap);
			if (syserr != 0)
				fprintf(stderr, ": %s", strerror(syserr));
			fprintf(stderr, "\n");
		} else {
			if (syserr == 0)
				vsyslog(severity, format, ap);
			else {
				char buf[512];

				strlcpy(buf, format, sizeof(buf));
				strlcat(buf, ": %m", sizeof(buf));
				vsyslog(severity, buf, ap);
			}
		}
	}

	va_end(ap);
}
