/* main.c

   System status updater...

   !!!Boy, howdy, is this ever not guaranteed not to change!!! */

/*
 * Copyright (c) 1997 The Internet Software Consortium.
 * All rights reserved.
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
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: main.c,v 1.1 1998/08/18 03:43:36 deraadt Exp $ Copyright (c) 1995, 1996 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

int log_priority;
int log_perror = 1;

int main (argc, argv, envp)
	int argc;
	char **argv;
	char **envp;
{
	struct sockaddr_un name;
	int sysconf_fd;
	struct sysconf_header hdr;
	int status;
	char *buf;

#ifdef SYSLOG_4_2
	openlog ("statmsg", LOG_NDELAY);
	log_priority = LOG_DAEMON;
#else
	openlog ("statmsg", LOG_NDELAY, LOG_DAEMON);
#endif

#if !(defined (DEBUG) || defined (SYSLOG_4_2) || defined (__CYGWIN32__))
	setlogmask (LOG_UPTO (LOG_INFO));
#endif	

	if (argc < 2)
		error ("usage: statmsg type [data]");

	hdr.length = 0;
	if (!strcmp (argv [1], "network-location-changed"))
		hdr.type = NETWORK_LOCATION_CHANGED;
	else
		error ("unknown status message type %s", argv [1]);

	sysconf_fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (sysconf_fd < 0)
		error ("unable to create sysconf socket: %m");

	/* XXX for now... */
	name.sun_family = PF_UNIX;
	strcpy (name.sun_path, "/var/run/sysconf");
	name.sun_len = ((sizeof name) - (sizeof name.sun_path) +
			strlen (name.sun_path));

	if (connect (sysconf_fd, (struct sockaddr *)&name, name.sun_len) < 0)
		error ("can't connect to sysconf socket: %m");

	status = write (sysconf_fd, &hdr, sizeof hdr);
	if (status < 0)
		error ("sysconf: %m");
	if (status < sizeof (hdr))
		error ("sysconf: short write");

	if (hdr.length) {
		status = write (sysconf_fd, buf, hdr.length);
		if (status < 0)
			error ("sysconf payload write: %m");
		if (status != hdr.length)
			error ("sysconf payload: short write");
	}

	exit (0);
}

void cleanup ()
{
}
