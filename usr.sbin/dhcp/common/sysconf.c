/* sysconf.c

   System status watcher...

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
"$Id: sysconf.c,v 1.1 1998/08/18 03:43:27 deraadt Exp $ Copyright (c) 1995, 1996 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

int sysconf_initialized;
int sysconf_fd;

void sysconf_startup (handler)
	void (*handler) PROTO ((struct sysconf_header *, void *));
{
	struct sockaddr_un name;
	static int once;

	/* Only initialize sysconf once. */
	if (sysconf_initialized)
		error ("attempted to reinitialize sysconf protocol");
	sysconf_initialized = 1;

	sysconf_fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (sysconf_fd < 0)
		error ("unable to create sysconf socket: %m");

	/* XXX for now... */
	name.sun_family = PF_UNIX;
	strcpy (name.sun_path, "/var/run/sysconf");
	name.sun_len = ((sizeof name) - (sizeof name.sun_path) +
			strlen (name.sun_path));

	if (connect (sysconf_fd, (struct sockaddr *)&name, name.sun_len) < 0) {
		if (!once)
			warn ("can't connect to sysconf socket: %m");
		once = 1;
		close (sysconf_fd);
		sysconf_initialized = 0;
		add_timeout (cur_time + 60, sysconf_restart, handler);
	} else
		add_protocol ("sysconf", sysconf_fd, sysconf_message, handler);
}

void sysconf_restart (v)
	void *v;
{
	void (*handler) PROTO ((struct sysconf_header *, void *)) = v;

	sysconf_startup (handler);
}

void sysconf_message (proto)
	struct protocol *proto;
{
	struct sysconf_header hdr;
	int status;
	char *buf;
	void (*handler) PROTO ((struct sysconf_header *, void *));

	status = read (sysconf_fd, &hdr, sizeof hdr);
	if (status < 0) {
		warn ("sysconf_message: %m");
	      lose:
		add_timeout (cur_time + 60, sysconf_restart, proto -> local);
		remove_protocol (proto);
		return;
	}
	if (status < sizeof (hdr)) {
		warn ("sysconf_message: short message");
		goto lose;
	}

	if (hdr.length) {
		buf = malloc (hdr.length);
		if (!buf)
			error ("sysconf_message: can't buffer payload");
		status = read (sysconf_fd, buf, hdr.length);
		if (status < 0)
			error ("sysconf_message payload read: %m");
		if (status != hdr.length)
			error ("sysconf_message payload: short read");
	} else
		buf = (char *)0;

	/* Call the handler... */
	if ((handler = proto -> local))
		(*handler) (&hdr, buf);

	if (buf)
		free (buf);
}
