/* dhcpd.c

   DHCP Server Daemon. */

/*
 * Copyright (c) 1995, 1996, 1997 The Internet Software Consortium.
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
static char ocopyright[] =
"$Id: dhcpd.c,v 1.1 1998/08/18 03:43:34 deraadt Exp $ Copyright 1995, 1996 The Internet Software Consortium.";
#endif

static char copyright[] =
"Copyright 1995, 1996 The Internet Software Consortium.";
static char arr [] = "All rights reserved.";
static char message [] = "Internet Software Consortium DHCPD $Name:  $";

#include "dhcpd.h"

static void usage PROTO ((void));

TIME cur_time;
struct group root_group;

struct iaddr server_identifier;
int server_identifier_matched;

#ifdef USE_FALLBACK
struct interface_info fallback_interface;
#endif

u_int16_t local_port;
u_int16_t remote_port;

int log_priority;
#ifdef DEBUG
int log_perror = -1;
#else
int log_perror = 1;
#endif

char *path_dhcpd_conf = _PATH_DHCPD_CONF;
char *path_dhcpd_db = _PATH_DHCPD_DB;
char *path_dhcpd_pid = _PATH_DHCPD_PID;

int main (argc, argv, envp)
	int argc;
	char **argv, **envp;
{
	int i, status;
	struct servent *ent;
	char *s;
	int cftest = 0;
#ifndef DEBUG
	int pidfilewritten = 0;
	int pid;
	char pbuf [20];
	int daemon = 1;
#endif
	int quiet = 0;

	/* Initially, log errors to stderr as well as to syslogd. */
#ifdef SYSLOG_4_2
	openlog ("dhcpd", LOG_NDELAY);
	log_priority = DHCPD_LOG_FACILITY;
#else
	openlog ("dhcpd", LOG_NDELAY, DHCPD_LOG_FACILITY);
#endif

#ifndef DEBUG
#ifndef SYSLOG_4_2
#ifndef __CYGWIN32__ /* XXX */
	setlogmask (LOG_UPTO (LOG_INFO));
#endif
#endif
#endif	

	for (i = 1; i < argc; i++) {
		if (!strcmp (argv [i], "-p")) {
			if (++i == argc)
				usage ();
			for (s = argv [i]; *s; s++)
				if (!isdigit (*s))
					error ("%s: not a valid UDP port",
					       argv [i]);
			status = atoi (argv [i]);
			if (status < 1 || status > 65535)
				error ("%s: not a valid UDP port",
				       argv [i]);
			local_port = htons (status);
			debug ("binding to user-specified port %d",
			       ntohs (local_port));
		} else if (!strcmp (argv [i], "-f")) {
#ifndef DEBUG
			daemon = 0;
#endif
		} else if (!strcmp (argv [i], "-d")) {
#ifndef DEBUG
			daemon = 0;
#endif
			log_perror = -1;
		} else if (!strcmp (argv [i], "-cf")) {
			if (++i == argc)
				usage ();
			path_dhcpd_conf = argv [i];
		} else if (!strcmp (argv [i], "-lf")) {
			if (++i == argc)
				usage ();
			path_dhcpd_db = argv [i];
                } else if (!strcmp (argv [i], "-t")) {
			/* test configurations only */
#ifndef DEBUG
			daemon = 0;
#endif
			cftest = 1;
			log_perror = -1;
		} else if (!strcmp (argv [i], "-q")) {
			quiet = 1;
			quiet_interface_discovery = 1;
		} else if (argv [i][0] == '-') {
			usage ();
		} else {
			struct interface_info *tmp =
				((struct interface_info *)
				 dmalloc (sizeof *tmp, "get_interface_list"));
			if (!tmp)
				error ("Insufficient memory to %s %s",
				       "record interface", argv [i]);
			memset (tmp, 0, sizeof *tmp);
			strcpy (tmp -> name, argv [i]);
			tmp -> next = interfaces;
			tmp -> flags = INTERFACE_REQUESTED;
			interfaces = tmp;
		}
	}

	if (!quiet) {
		note (message);
		note (copyright);
		note (arr);
	}

	/* Default to the DHCP/BOOTP port. */
	if (!local_port)
	{
		ent = getservbyname ("dhcp", "udp");
		if (!ent)
			local_port = htons (67);
		else
			local_port = ent -> s_port;
#ifndef __CYGWIN32__ /* XXX */
		endservent ();
#endif
	}
  
	remote_port = htons (ntohs (local_port) + 1);

	/* Get the current time... */
	GET_TIME (&cur_time);

	/* Read the dhcpd.conf file... */
	if (!readconf ())
		error ("Configuration file errors encountered -- exiting");

        /* test option should cause an early exit */
 	if (cftest) 
 		exit(0);

	/* Start up the database... */
	db_startup ();

	/* Discover all the network interfaces and initialize them. */
	discover_interfaces (1);

	/* Initialize icmp support... */
	icmp_startup (1, lease_pinged);

#ifndef DEBUG
	if (daemon) {
		/* First part of becoming a daemon... */
		if ((pid = fork ()) < 0)
			error ("Can't fork daemon: %m");
		else if (pid)
			exit (0);
	}

	/* Read previous pid file. */
	if ((i = open (path_dhcpd_pid, O_RDONLY)) >= 0) {
		status = read (i, pbuf, (sizeof pbuf) - 1);
		close (i);
		pbuf [status] = 0;
		pid = atoi (pbuf);

		/* If the previous server process is not still running,
		   write a new pid file immediately. */
		if (pid && kill (pid, 0) < 0) {
			unlink (path_dhcpd_pid);
			if ((i = open (path_dhcpd_pid,
				       O_WRONLY | O_CREAT, 0640)) >= 0) {
				sprintf (pbuf, "%d\n", (int)getpid ());
				write (i, pbuf, strlen (pbuf));
				close (i);
				pidfilewritten = 1;
			}
		}
	}

	/* If we were requested to log to stdout on the command line,
	   keep doing so; otherwise, stop. */
	if (log_perror == -1)
		log_perror = 1;
	else
		log_perror = 0;

	if (daemon) {
		/* Become session leader and get pid... */
		close (0);
		close (1);
		close (2);
		pid = setsid ();
	}

	/* If we didn't write the pid file earlier because we found a
	   process running the logged pid, but we made it to here,
	   meaning nothing is listening on the bootp port, then write
	   the pid file out - what's in it now is bogus anyway. */
	if (!pidfilewritten) {
		unlink (path_dhcpd_pid);
		if ((i = open (path_dhcpd_pid,
			       O_WRONLY | O_CREAT, 0640)) >= 0) {
			sprintf (pbuf, "%d\n", (int)getpid ());
			write (i, pbuf, strlen (pbuf));
			close (i);
			pidfilewritten = 1;
		}
	}
#endif /* !DEBUG */

	/* Set up the bootp packet handler... */
	bootp_packet_handler = do_packet;

	/* Receive packets and dispatch them... */
	dispatch ();

	/* Not reached */
	return 0;
}

/* Print usage message. */

static void usage ()
{
	note (message);
	note (copyright);
	note (arr);

	error ("Usage: dhcpd [-p <UDP port #>] [-d] [-f] [-cf config-file]%s",
	       "\n            [-lf lease-file] [if0 [...ifN]]");
}

void cleanup ()
{
}

void lease_pinged (from, packet, length)
	struct iaddr from;
	u_int8_t *packet;
	int length;
{
	struct lease *lp;

	/* Don't try to look up a pinged lease if we aren't trying to
	   ping one - otherwise somebody could easily make us churn by
	   just forging repeated ICMP EchoReply packets for us to look
	   up. */
	if (!outstanding_pings)
		return;

	lp = find_lease_by_ip_addr (from);

	if (!lp) {
		note ("unexpected ICMP Echo Reply from %s", piaddr (from));
		return;
	}

	if (!lp -> state) {
		warn ("ICMP Echo Reply for %s arrived late or is spurious.\n",
		      piaddr (from));
		return;
	}

	/* At this point it looks like we pinged a lease and got a
	   response, which shouldn't have happened. */
	free_lease_state (lp -> state, "lease_pinged");
	lp -> state = (struct lease_state *)0;

	abandon_lease (lp, "pinged before offer");
	cancel_timeout (lease_ping_timeout, lp);
	--outstanding_pings;
}

void lease_ping_timeout (vlp)
	void *vlp;
{
	struct lease *lp = vlp;

	--outstanding_pings;
	dhcp_reply (lp);
}
