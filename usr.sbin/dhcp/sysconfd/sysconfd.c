/* main.c

   System configuration status daemon...

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
"$Id: sysconfd.c,v 1.1 1998/08/18 03:43:36 deraadt Exp $ Copyright (c) 1995, 1996 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

int sysconf_fd;

struct sysconf_client {
	struct sysconf_client *next;
	int fd;
} *clients;

static void new_connection PROTO ((struct protocol *));
static void client_input PROTO ((struct protocol *));

int log_priority;
int log_perror;

struct interface_info fallback_interface;
TIME cur_time;
u_int16_t local_port;


int main (argc, argv, envp)
	int argc;
	char **argv;
	char **envp;
{
	struct sockaddr_un name;
	int sysconf_fd;
	int pid;

#ifdef SYSLOG_4_2
	openlog ("sysconfd", LOG_NDELAY);
	log_priority = LOG_DAEMON;
#else
	openlog ("sysconfd", LOG_NDELAY, LOG_DAEMON);
#endif

#if !(defined (DEBUG) || defined (SYSLOG_4_2) || defined (__CYGWIN32__))
	setlogmask (LOG_UPTO (LOG_INFO));
#endif	

	/* Make a socket... */
	sysconf_fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (sysconf_fd < 0)
		error ("unable to create sysconf socket: %m");

	/* XXX for now... */
	name.sun_family = PF_UNIX;
	strcpy (name.sun_path, "/var/run/sysconf");
	name.sun_len = ((sizeof name) - (sizeof name.sun_path) +
			strlen (name.sun_path));
	unlink (name.sun_path);

	/* Bind to it... */
	if (bind (sysconf_fd, (struct sockaddr *)&name, name.sun_len) < 0)
		error ("can't bind to sysconf socket: %m");

	/* Listen for connections... */
	if (listen (sysconf_fd, 1) < 0)
		error ("can't listen on sysconf socket: %m");

	/* Stop logging to stderr... */
	log_perror = 0;

	/* Become a daemon... */
	if ((pid = fork ()) < 0)
		error ("Can't fork daemon: %m");
	else if (pid)
		exit (0);

	/* Become session leader... */
	(void)setsid ();

	/* Set up a protocol structure for it... */
	add_protocol ("listener", sysconf_fd, new_connection, 0);

	/* Kernel status stuff goes here... */

	/* Wait for something to happen... */
	dispatch ();

	exit (0);
}

void new_connection (proto)
	struct protocol *proto;
{
	struct sockaddr_un name;
	int namelen;
	struct sysconf_client *tmp;
	int new_fd;

	tmp = (struct sysconf_client *)malloc (sizeof *tmp);
	if (tmp < 0)
		error ("Can't find memory for new client!");
	memset (tmp, 0, sizeof *tmp);

	namelen = sizeof name;
	new_fd = accept (proto -> fd, (struct sockaddr *)&name, &namelen);
	if (new_fd < 0)
		warn ("accept: %m");

	tmp -> next = clients;
	tmp -> fd = new_fd;
	clients = tmp;

	add_protocol ("aclient", new_fd, client_input, 0);
}

void client_input (proto)
	struct protocol *proto;
{
	struct sysconf_header hdr;
	int status;
	char *buf;
	void (*handler) PROTO ((struct sysconf_header *, void *));
	struct sysconf_client *client;

	status = read (proto -> fd, &hdr, sizeof hdr);
	if (status < 0) {
	      blow:
		warn ("client_input: %m");
		close (proto -> fd);
		remove_protocol (proto);
		return;
	}
	if (status < sizeof (hdr)) {
		warn ("client_input: short message");
		goto blow;
	}

	if (hdr.length) {
		buf = malloc (hdr.length);
		if (!buf) {
			warn ("client_input: can't buffer payload");
			goto blow;
		}
		status = read (proto -> fd, buf, hdr.length);
		if (status < 0) {
			warn ("client_input payload read: %m");
			goto blow;
		}
		if (status != hdr.length) {
			warn ("client_input payload: short read");
			goto blow;
		}
	} else
		buf = (char *)0;

	for (client = clients; client; client = client -> next) {
		if (client -> fd == proto -> fd)
			continue;

		status = write (client -> fd, &hdr, sizeof hdr);
		if (status < 0) {
			warn ("client_input: %m");
			continue;
		}
		if (status < sizeof (hdr)) {
			warn ("client_input: short write");
			continue;
		}

		if (hdr.length) {
			status = write (client -> fd, buf, hdr.length);
			if (status < 0) {
				warn ("client_input payload write: %m");
				continue;
			}
			if (status != hdr.length) {
				warn ("client_input payload: short write");
				continue;
			}
		}
	}

	if (buf)
		free (buf);
}

void cleanup ()
{
}

int commit_leases ()
{
	return 0;
}

int write_lease (lease)
	struct lease *lease;
{
	return 0;
}

void db_startup ()
{
}

void bootp (packet)
	struct packet *packet;
{
}

void dhcp (packet)
	struct packet *packet;
{
}


