/* dhcrelay.c

   DHCP/BOOTP Relay Agent. */

/*
 * Copyright (c) 1997, 1998, 1999 The Internet Software Consortium.
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

#include "dhcpd.h"

void	 usage(void);
void	 relay(struct interface_info *, struct dhcp_packet *, int,
	    unsigned int, struct iaddr, struct hardware *);
char	*print_hw_addr(int, int, unsigned char *);

time_t cur_time;
time_t default_lease_time = 43200; /* 12 hours... */
time_t max_lease_time = 86400; /* 24 hours... */
struct tree_cache *global_options [256];

int log_perror = 1;

u_int16_t local_port;
u_int16_t remote_port;
int log_priority;
struct interface_info *interfaces = NULL;

struct server_list {
	struct server_list *next;
	struct sockaddr_in to;
} *servers;

int main (argc, argv)
	int argc;
	char **argv;
{
	int			 ch, quiet = 0, no_daemon = 0;
	extern char		*__progname;
	struct servent		*ent;
	struct server_list	*sp = NULL;

	/* Initially, log errors to stderr as well as to syslogd. */
	openlog(__progname, LOG_NDELAY, DHCPD_LOG_FACILITY);
	setlogmask (LOG_UPTO (LOG_INFO));

	while ((ch = getopt(argc, argv, "di:p:q")) != -1) {
		switch (ch) {
		case 'd':
			no_daemon = 1;
			break;
		case 'i':
			if (interfaces != NULL)
				usage();
			if ((interfaces = calloc(1,
			    sizeof(struct interface_info))) == NULL)
				error("calloc");
			strlcpy(interfaces->name, optarg,
			    sizeof(interfaces->name));
			break;
		case 'p':
			local_port = htons(atoi(optarg));
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			usage();
			/* not reached */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	while (argc > 0) {
		struct hostent		*he;
		struct in_addr		 ia, *iap = NULL;

		if (inet_aton(argv[0], &ia))
			iap = &ia;
		else {
			he = gethostbyname(argv[0]);
			if (!he)
				warn ("%s: host unknown", argv[0]);
			else
				iap =
				   ((struct in_addr *)he->h_addr_list[0]);
		}
		if (iap) {
			if ((sp = calloc(1, sizeof *sp)) == NULL)
				error("calloc");
			sp -> next = servers;
			servers = sp;
			memcpy (&sp->to.sin_addr, iap, sizeof *iap);
		}
		argc--;
		argv++;
	}

	if (!quiet)
		log_perror = 0;

	if (interfaces == NULL)
		error("no interface given");

	/* Default to the DHCP/BOOTP port. */
	if (!local_port) {
		ent = getservbyname ("dhcps", "udp");
		if (!ent)
			local_port = htons (67);
		else
			local_port = ent -> s_port;
		endservent ();
	}
	remote_port = htons (ntohs (local_port) + 1);

	/* We need at least one server. */
	if (!sp) {
		usage();
	}

	/* Set up the server sockaddrs. */
	for (sp = servers; sp; sp = sp -> next) {
		sp -> to.sin_port = local_port;
		sp -> to.sin_family = AF_INET;
		sp -> to.sin_len = sizeof sp -> to;
	}

	/* Get the current time... */
	time(&cur_time);

	/* Discover all the network interfaces. */
	discover_interfaces(interfaces);

	/* Set up the bootp packet handler... */
	bootp_packet_handler = relay;

	/* Become a daemon... */
	if (!no_daemon) {
		int pid;

		log_perror = 0;

		if ((pid = fork()) < 0)
			error ("can't fork daemon: %m");
		else if (pid)
			exit (0);

		close (0);
		close (1);
		close (2);
		pid = setsid ();
	}

	/* Start dispatching packets and timeouts... */
	dispatch ();

	/*NOTREACHED*/
	return 0;
}

void relay (ip, packet, length, from_port, from, hfrom)
	struct interface_info *ip;
	struct dhcp_packet *packet;
	int length;
	unsigned int from_port;
	struct iaddr from;
	struct hardware *hfrom;
{
	struct server_list *sp;
	struct sockaddr_in to;
	struct interface_info *out;
	struct hardware hto;

	if (packet -> hlen > sizeof packet -> chaddr) {
		note ("Discarding packet with invalid hlen.");
		return;
	}

	/* If it's a bootreply, forward it to the client. */
	if (packet -> op == BOOTREPLY) {
		memset(&to, 0, sizeof(to));
		if (!(packet -> flags & htons (BOOTP_BROADCAST))) {
			to.sin_addr = packet -> yiaddr;
			to.sin_port = remote_port;
		} else {
			to.sin_addr.s_addr = htonl (INADDR_BROADCAST);
			to.sin_port = remote_port;
		}
		to.sin_family = AF_INET;
		to.sin_len = sizeof to;

		/* Set up the hardware destination address. */
		hto.hlen = packet -> hlen;
		if (hto.hlen > sizeof hto.haddr)
			hto.hlen = sizeof hto.haddr;
		memcpy (hto.haddr, packet -> chaddr, hto.hlen);
		hto.htype = packet -> htype;

		/* Find the interface that corresponds to the giaddr
		   in the packet. */
		for (out = interfaces; out; out = out -> next) {
			if (!memcmp (&out -> primary_address,
				     &packet -> giaddr,
				     sizeof packet -> giaddr))
				break;
		}
		if (!out) {
			warn ("packet to bogus giaddr %s.",
			      inet_ntoa (packet -> giaddr));
			return;
		}

		if (send_packet (out,
				  (struct packet *)0,
				  packet, length, out -> primary_address,
				  &to, &hto) != -1)
			debug ("forwarded BOOTREPLY for %s to %s",
			       print_hw_addr (packet -> htype, packet -> hlen,
					      packet -> chaddr),
			       inet_ntoa (to.sin_addr));

		return;
	}

	/* If giaddr is set on a BOOTREQUEST, ignore it - it's already
	   been gatewayed. */
	if (packet -> giaddr.s_addr) {
		note ("ignoring BOOTREQUEST with giaddr of %s\n",
		      inet_ntoa (packet -> giaddr));
		return;
	}

	/* Set the giaddr so the server can figure out what net it's
	   from and so that we can later forward the response to the
	   correct net. */
	packet -> giaddr = ip -> primary_address;

	/* Otherwise, it's a BOOTREQUEST, so forward it to all the
	   servers. */
	for (sp = servers; sp; sp = sp -> next) {
		if (send_packet (interfaces,
				  (struct packet *)0,
				  packet, length, ip -> primary_address,
				  &sp -> to, (struct hardware *)0) != -1) {
			debug ("forwarded BOOTREQUEST for %s to %s",
			       print_hw_addr (packet -> htype, packet -> hlen,
					      packet -> chaddr),
			       inet_ntoa (sp -> to.sin_addr));
		}
	}
}

void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "Usage: %s [-q] [-d] [-p <port>] ", __progname);
	fprintf(stderr, "-i interface server1 [... serverN]\n");
	exit (1);
}

void cleanup ()
{
}

int write_lease (lease)
	struct lease *lease;
{
	return 1;
}

int commit_leases ()
{
	return 1;
}

void bootp (packet)
	struct packet *packet;
{
}

void dhcp (packet)
	struct packet *packet;
{
}

char *print_hw_addr (htype, hlen, data)
	int htype;
	int hlen;
	unsigned char *data;
{
	static char habuf [49];
	char *s;
	int i;

	if (htype == 0 || hlen == 0) {
		goto bad;
	} else {
		int slen = sizeof(habuf);
		s = habuf;
		for (i = 0; i < hlen; i++) {
			int j;
			j = snprintf (s, slen, "%02x", data [i]);
			if (j <= 0)
				goto bad;

			s += strlen (s);
			slen -= (strlen(s) + 1);
			*s++ = ':';
		}
		*--s = 0;
	}
	return habuf;
 bad:
	strlcpy (habuf, "<null>", sizeof habuf);
	return habuf;

}
