/*	$OpenBSD: dhcpd.c,v 1.13 2004/04/18 00:30:33 henning Exp $ */

/*
 * Copyright (c) 2004 Henning Brauer <henning@cvs.openbsd.org>
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.  All rights reserved.
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
#include "pwd.h"

void usage(void);

time_t cur_time;
struct group root_group;

struct iaddr server_identifier;
int server_identifier_matched;

u_int16_t local_port;
u_int16_t remote_port;

int log_priority;
int log_perror = 1;
char *path_dhcpd_conf = _PATH_DHCPD_CONF;
char *path_dhcpd_db = _PATH_DHCPD_DB;

int
main(int argc, char *argv[])
{
	int		 ch, status;
	int		 cftest = 0, quiet = 0, daemonize = 1;
	struct servent	*ent;
	struct passwd	*pw;
	extern char *__progname;

	/* Initially, log errors to stderr as well as to syslogd. */
	openlog(__progname, LOG_NDELAY, DHCPD_LOG_FACILITY);
	setlogmask(LOG_UPTO (LOG_INFO));

	while ((ch = getopt(argc, argv, "c:dfl:p:tq")) != -1)
		switch (ch) {
		case 'c':
			path_dhcpd_conf = optarg;
			break;
		case 'd':
			daemonize = 0;
			log_perror = -1;
			break;
		case 'f':
			daemonize = 0;
			break;
		case 'l':
			path_dhcpd_db = optarg;
			break;
		case 'p':
			status = atoi(optarg);
			if (status < 1 || status > 65535)
				error("%s: not a valid UDP port", optarg);
			local_port = htons(status);
			break;
		case 'q':
			quiet = 1;
			quiet_interface_discovery = 1;
			break;
		case 't':
			daemonize = 0;
			cftest = 1;
			log_perror = -1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	while (argc > 0) {
		struct interface_info *tmp = calloc(1, sizeof(*tmp));
		if (!tmp)
			error("calloc");
		strlcpy(tmp->name, argv[0], sizeof(tmp->name));
		tmp->next = interfaces;
		tmp->flags = INTERFACE_REQUESTED;
		interfaces = tmp;
		argc--;
		argv++;
	}

	if (quiet)
		log_perror = 0;

	/* Default to the DHCP/BOOTP port. */
	if (!local_port) {
		ent = getservbyname ("dhcp", "udp");
		if (!ent)
			local_port = htons (67);
		else
			local_port = ent->s_port;
		endservent();
	}

	remote_port = htons(ntohs(local_port) + 1);
	time(&cur_time);
	if (!readconf ())
		error("Configuration file errors encountered -- exiting");

	if (cftest)
		exit(0);

	db_startup();
	discover_interfaces(DISCOVER_SERVER);
	icmp_startup(1, lease_pinged);

	if ((pw = getpwnam("_dhcp")) == NULL)
		error("%m");

	log_perror = 0;
	if (daemonize)
		daemon(0, 0);

	if (chroot(_PATH_VAREMPTY) == -1)
		error("chroot %s: %m", _PATH_VAREMPTY);
	if (chdir("/") == -1)
		error("chdir(\"/\"): %m");
	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		error("can't drop privileges: %m");
	endpwent();

	bootp_packet_handler = do_packet;
	dispatch();

	/* not reached */
	exit(0);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-df] [-c config-file] [-l lease-file]",
	    __progname);
	fprintf(stderr, " [-p port] [if0 [...ifN]]\n");
	exit(1);
}

void
lease_pinged(struct iaddr from, u_int8_t *packet, int length)
{
	struct lease	*lp;

	/*
	 * Don't try to look up a pinged lease if we aren't trying to
	 * ping one - otherwise somebody could easily make us churn by
	 * just forging repeated ICMP EchoReply packets for us to look
	 * up.
	 */
	if (!outstanding_pings)
		return;

	lp = find_lease_by_ip_addr(from);

	if (!lp) {
		note("unexpected ICMP Echo Reply from %s", piaddr(from));
		return;
	}

	if (!lp->state && !lp->releasing) {
		warn("ICMP Echo Reply for %s arrived late or is spurious.",
		    piaddr(from));
		return;
	}

	/* At this point it looks like we pinged a lease and got a
	 * response, which shouldn't have happened.
	 * if it did it's either one of two two cases:
	 * 1 - we pinged this lease before offering it and
	 *     something answered, so we abandon it.
	 * 2 - we pinged this lease before releasing it
	 *     and something answered, so we don't release it.
	 */
	if (lp->releasing) {
		warn("IP address %s answers a ping after sending a release",
		    piaddr(lp->ip_addr));
		warn("Possible release spoof - Not releasing address %s",
		    piaddr(lp->ip_addr));
		lp->releasing = 0;
	} else {
		free_lease_state(lp->state, "lease_pinged");
		lp->state = NULL;
		abandon_lease(lp, "pinged before offer");
	}
	cancel_timeout(lease_ping_timeout, lp);
	--outstanding_pings;
}

void
lease_ping_timeout(void *vlp)
{
	struct lease	*lp = vlp;

	--outstanding_pings;
	if (lp->releasing) {
		lp->releasing = 0;
		release_lease(lp);
	} else
		dhcp_reply(lp);
}
