/*	$OpenBSD: libaltq2.c,v 1.1.1.1 2001/06/27 18:23:17 kjc Exp $	*/
/*
 * this file contains functions and variables needed to use libaltq.
 * since these are defined in rsvpd, they should be separated in order
 * to link libaltq to rsvpd.
 */
#include <sys/param.h>

#include <altq/altq.h>

#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

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

#ifdef __STDC__
	va_start(ap, format);
#else
	va_start(ap);
#endif

	if (severity <= l_debug) {
		if (!daemonize)
			vfprintf(stderr, format, ap);
		else
			vsyslog(severity, format, ap);
	}

	va_end(ap);

	if (syserr == 0) {
		/* Do nothing for now */
	} else if (syserr < sys_nerr) {
		if (severity <= l_debug) {
			if (!daemonize)
				fprintf(stderr, ": %s\n", sys_errlist[syserr]);
			else
				syslog(severity, ": %s", sys_errlist[syserr]);
		}
	} else {
		if (severity <= l_debug) {
			if (!daemonize)
				fprintf(stderr, ": errno %d\n", syserr);
			else
				syslog(severity, ": errno %d", syserr);
		}
	}
}
