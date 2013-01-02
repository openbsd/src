/*	$OpenBSD: dhclient.c,v 1.200 2013/01/02 16:27:42 krw Exp $	*/

/*
 * Copyright 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.    All rights reserved.
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
 *
 * This client was substantially modified and enhanced by Elliot Poger
 * for use on Linux while he was working on the MosquitoNet project at
 * Stanford.
 *
 * The current version owes much to Elliot's Linux enhancements, but
 * was substantially reorganized and partially rewritten by Ted Lemon
 * so as to use the same networking framework that the Internet Software
 * Consortium DHCP server uses.   Much system-specific configuration code
 * was moved into a shell script so that as support for more operating
 * systems is added, it will not be necessary to port and maintain
 * system-specific configuration code to these operating systems - instead,
 * the shell script can invoke the native tools to accomplish the same
 * purpose.
 */

#include "dhcpd.h"
#include "privsep.h"

#include <sys/ioctl.h>

#include <poll.h>
#include <pwd.h>

#define	CLIENT_PATH 		"PATH=/usr/bin:/usr/sbin:/bin:/sbin"
#define DEFAULT_LEASE_TIME	43200	/* 12 hours... */
#define TIME_MAX		2147483647

char *path_dhclient_conf = _PATH_DHCLIENT_CONF;
char *path_dhclient_db = NULL;

int log_perror = 1;
int nullfd = -1;
int no_daemon;
int unknown_ok = 1;
int routefd = -1;

volatile sig_atomic_t quit;

struct in_addr deleting;
struct in_addr adding;

struct in_addr inaddr_any;
struct sockaddr_in sockaddr_broadcast;

struct interface_info *ifi;
struct client_state *client;
struct client_config *config;
struct imsgbuf *unpriv_ibuf;

void		 sighdlr(int);
int		 findproto(char *, int);
struct sockaddr	*get_ifa(char *, int);
void		 usage(void);
int		 check_option(struct client_lease *l, int option);
int		 ipv4addrs(char * buf);
int		 res_hnok(const char *dn);
char		*option_as_string(unsigned int code, unsigned char *data, int len);
void		 fork_privchld(int, int);
void		 get_ifname(char *);
void		 new_resolv_conf(char *, char *, char *);
struct client_lease *apply_defaults(struct client_lease *);
struct client_lease *clone_lease(struct client_lease *);
void		 socket_nonblockmode(int);
void		 apply_ignore_list(char *);

#define	ROUNDUP(a) \
	    ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define	ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

static FILE *leaseFile;

void
sighdlr(int sig)
{
	quit = sig;
}

int
findproto(char *cp, int n)
{
	struct sockaddr *sa;
	int i;

	if (n == 0)
		return -1;
	for (i = 1; i; i <<= 1) {
		if (i & n) {
			sa = (struct sockaddr *)cp;
			switch (i) {
			case RTA_IFA:
			case RTA_DST:
			case RTA_GATEWAY:
			case RTA_NETMASK:
				if (sa->sa_family == AF_INET)
					return AF_INET;
				if (sa->sa_family == AF_INET6)
					return AF_INET6;
				break;
			case RTA_IFP:
				break;
			}
			ADVANCE(cp, sa);
		}
	}
	return (-1);
}

struct sockaddr *
get_ifa(char *cp, int n)
{
	struct sockaddr *sa;
	int i;

	if (n == 0)
		return (NULL);
	for (i = 1; i; i <<= 1)
		if (i & n) {
			sa = (struct sockaddr *)cp;
			if (i == RTA_IFA)
				return (sa);
			ADVANCE(cp, sa);
		}

	return (NULL);
}

void
routehandler(void)
{
	char msg[2048];
	struct in_addr a, b;
	ssize_t n;
	int linkstat;
	struct rt_msghdr *rtm;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct if_announcemsghdr *ifan;
	struct sockaddr *sa;
	char *errmsg;

	do {
		n = read(routefd, &msg, sizeof(msg));
	} while (n == -1 && errno == EINTR);

	rtm = (struct rt_msghdr *)msg;
	if (n < sizeof(rtm->rtm_msglen) || n < rtm->rtm_msglen ||
	    rtm->rtm_version != RTM_VERSION)
		return;

	switch (rtm->rtm_type) {
	case RTM_NEWADDR:
	case RTM_DELADDR:
		ifam = (struct ifa_msghdr *)rtm;
		if (ifam->ifam_index != ifi->index)
			break;
		if (findproto((char *)ifam + ifam->ifam_hdrlen,
		    ifam->ifam_addrs) != AF_INET)
			break;
		sa = get_ifa((char *)ifam + ifam->ifam_hdrlen,
		    ifam->ifam_addrs);
		if (sa == NULL) {
			errmsg = "sa == NULL";
			goto die;
		}

		memcpy(&a, &((struct sockaddr_in *)sa)->sin_addr, sizeof(a));
		if (a.s_addr == INADDR_ANY)
			break;

		/*
		 * If we are in the process of binding a new lease, ignore
		 * messages generated by that process.
		 */
		if (rtm->rtm_type == RTM_NEWADDR) {
			if (a.s_addr == adding.s_addr) {
				adding.s_addr = INADDR_ANY;
				note("bound to %s -- renewal in %lld seconds.",
				    inet_ntoa(client->active->address),
				    (long long)(client->active->renewal -
				    time(NULL)));
				go_daemon();
				break;
			}
			errmsg = "interface address added";
		} else {
			if (a.s_addr == deleting.s_addr) {
				deleting.s_addr = INADDR_ANY;
				break;
			}
			if (client->active &&
			    a.s_addr == client->active->address.s_addr) {
				/* Tell the priv process active_addr is gone. */
				memset(&b, 0, sizeof(b));
				add_address(ifi->name, 0, b, b);
				break;
			}
			errmsg = "interface address deleted";
		} 
		goto die;
	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		if (ifm->ifm_index != ifi->index)
			break;
		if ((rtm->rtm_flags & RTF_UP) == 0) {
			errmsg = "interface down";
			goto die;
		}

		linkstat =
		    LINK_STATE_IS_UP(ifm->ifm_data.ifi_link_state) ? 1 : 0;
		if (linkstat != ifi->linkstat) {
#ifdef DEBUG
			debug("link state %s -> %s",
			    ifi->linkstat ? "up" : "down",
			    linkstat ? "up" : "down");
#endif
			ifi->linkstat = interface_status(ifi->name);
			if (ifi->linkstat) {
				client->state = S_REBOOTING;
				state_reboot();
			}
		}
		break;
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *)rtm;
		if (ifan->ifan_what == IFAN_DEPARTURE &&
		    ifan->ifan_index == ifi->index) {
			errmsg = "interface departure";
			goto die;
		}
		break;
	default:
		break;
	}
	return;

die:
	error("routehandler: %s", errmsg);
}

char **saved_argv;

int
main(int argc, char *argv[])
{
	struct stat sb;
	int	 ch, fd, quiet = 0, i = 0, socket_fd[2];
	extern char *__progname;
	struct passwd *pw;
	char *ignore_list = NULL;
	int rtfilter;

	saved_argv = argv;

	/* Initially, log errors to stderr as well as to syslogd. */
	openlog(__progname, LOG_PID | LOG_NDELAY, DHCPD_LOG_FACILITY);
	setlogmask(LOG_UPTO(LOG_INFO));

	while ((ch = getopt(argc, argv, "c:di:l:qu")) != -1)
		switch (ch) {
		case 'c':
			path_dhclient_conf = optarg;
			break;
		case 'd':
			no_daemon = 1;
			break;
		case 'i':
			ignore_list = optarg;
			break;
		case 'l':
			path_dhclient_db = optarg;
			if (lstat(path_dhclient_db, &sb) != -1) {
				if (!S_ISREG(sb.st_mode))
					error("'%s' is not a regular file",
					    path_dhclient_db);
			}
			break;
		case 'q':
			quiet = 1;
			break;
		case 'u':
			unknown_ok = 0;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	ifi = calloc(1, sizeof(*ifi));
	if (ifi == NULL)
		error("ifi calloc");
	client = calloc(1, sizeof(*client));
	if (client == NULL)
		error("client calloc");
	config = calloc(1, sizeof(*config));
	if (config == NULL)
		error("config calloc");

	get_ifname(argv[0]);
	if (path_dhclient_db == NULL && asprintf(&path_dhclient_db, "%s.%s",
	    _PATH_DHCLIENT_DB, ifi->name) == -1)
		error("asprintf");

	if (quiet)
		log_perror = 0;

	tzset();

	memset(&sockaddr_broadcast, 0, sizeof(sockaddr_broadcast));
	sockaddr_broadcast.sin_family = AF_INET;
	sockaddr_broadcast.sin_port = htons(REMOTE_PORT);
	sockaddr_broadcast.sin_addr.s_addr = INADDR_BROADCAST;
	sockaddr_broadcast.sin_len = sizeof(sockaddr_broadcast);
	inaddr_any.s_addr = INADDR_ANY;

	/* Put us into the correct rdomain */
	ifi->rdomain = get_rdomain(ifi->name);

	read_client_conf();
	if (ignore_list)
		apply_ignore_list(ignore_list);

	if (interface_status(ifi->name) == 0) {
		interface_link_forceup(ifi->name);
		/* Give it up to 4 seconds of silent grace to find link */
		i = -4;
	} else
		i = 0;

	while (!(ifi->linkstat = interface_status(ifi->name))) {
		if (i == 0)
			fprintf(stderr, "%s: no link ...", ifi->name);
		else if (i > 0)
			fprintf(stderr, ".");
		fflush(stderr);
		if (++i > config->link_timeout) {
			fprintf(stderr, " sleeping\n");
			goto dispatch;
		}
		sleep(1);
	}
	if (i > 0)
		fprintf(stderr, " got link\n");

 dispatch:
	if ((nullfd = open(_PATH_DEVNULL, O_RDWR, 0)) == -1)
		error("cannot open %s: %s", _PATH_DEVNULL, strerror(errno));

	if ((pw = getpwnam("_dhcp")) == NULL)
		error("no such user: _dhcp");

	/* set up the interface */
	discover_interface();

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, socket_fd) == -1)
		error("socketpair: %s", strerror(errno));
	socket_nonblockmode(socket_fd[0]);
	fcntl(socket_fd[0], F_SETFD, FD_CLOEXEC);
	socket_nonblockmode(socket_fd[1]);
	fcntl(socket_fd[1], F_SETFD, FD_CLOEXEC);

	fork_privchld(socket_fd[0], socket_fd[1]);

	close(socket_fd[0]);
	if ((unpriv_ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		error("no memory for unpriv_ibuf");
	imsg_init(unpriv_ibuf, socket_fd[1]);

	if ((fd = open(path_dhclient_db, O_RDONLY|O_EXLOCK|O_CREAT, 0)) == -1)
		error("can't open and lock %s: %s", path_dhclient_db,
		    strerror(errno));
	read_client_leases();
	if ((leaseFile = fopen(path_dhclient_db, "w")) == NULL)
		error("can't open %s: %s", path_dhclient_db, strerror(errno));
	rewrite_client_leases();
	close(fd);

	if ((routefd = socket(PF_ROUTE, SOCK_RAW, 0)) == -1)
		error("socket(PF_ROUTE, SOCK_RAW): %s", strerror(errno));

	rtfilter = ROUTE_FILTER(RTM_NEWADDR) | ROUTE_FILTER(RTM_DELADDR) |
	    ROUTE_FILTER(RTM_IFINFO) | ROUTE_FILTER(RTM_IFANNOUNCE);

	if (setsockopt(routefd, PF_ROUTE, ROUTE_MSGFILTER,
	    &rtfilter, sizeof(rtfilter)) == -1)
		error("setsockopt(ROUTE_MSGFILTER): %s", strerror(errno));
	if (setsockopt(routefd, AF_ROUTE, ROUTE_TABLEFILTER, &ifi->rdomain,
	    sizeof(ifi->rdomain)) == -1)
		error("setsockopt(ROUTE_TABLEFILTER): %s", strerror(errno));

	if (chroot(_PATH_VAREMPTY) == -1)
		error("chroot");
	if (chdir("/") == -1)
		error("chdir(\"/\")");

	if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1)
		error("setresgid");
	if (setgroups(1, &pw->pw_gid) == -1)
		error("setgroups");
	if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
		error("setresuid");

	endpwent();

	setproctitle("%s", ifi->name);

	if (ifi->linkstat) {
		client->state = S_REBOOTING;
		state_reboot();
	} else
		go_daemon();

	dispatch();

	/* not reached */
	return (0);
}

void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr,
	    "usage: %s [-dqu] [-c file] [-i options] [-l file] interface\n",
	    __progname);
	exit(1);
}

/*
 * Individual States:
 *
 * Each routine is called from the dhclient_state_machine() in one of
 * these conditions:
 * -> entering INIT state
 * -> recvpacket_flag == 0: timeout in this state
 * -> otherwise: received a packet in this state
 *
 * Return conditions as handled by dhclient_state_machine():
 * Returns 1, sendpacket_flag = 1: send packet, reset timer.
 * Returns 1, sendpacket_flag = 0: just reset the timer (wait for a milestone).
 * Returns 0: finish the nap which was interrupted for no good reason.
 *
 * Several per-interface variables are used to keep track of the process:
 *   active_lease: the lease that is being used on the interface
 *                 (null pointer if not configured yet).
 *   offered_leases: leases corresponding to DHCPOFFER messages that have
 *                   been sent to us by DHCP servers.
 *   acked_leases: leases corresponding to DHCPACK messages that have been
 *                 sent to us by DHCP servers.
 *   sendpacket: DHCP packet we're trying to send.
 *   destination: IP address to send sendpacket to
 * In addition, there are several relevant per-lease variables.
 *   T1_expiry, T2_expiry, lease_expiry: lease milestones
 * In the active lease, these control the process of renewing the lease;
 * In leases on the acked_leases list, this simply determines when we
 * can no longer legitimately use the lease.
 */
void
state_reboot(void)
{
	/* Cancel all timeouts, since a link state change gets us here
	   and can happen anytime. */
	cancel_timeout();
	deleting.s_addr = INADDR_ANY;
	adding.s_addr = INADDR_ANY;

	/* If we don't remember an active lease, go straight to INIT. */
	if (!client->active || client->active->is_bootp) {
		client->state = S_INIT;
		state_init();
		return;
	}

	/* make_request doesn't initialize xid because it normally comes
	   from the DHCPDISCOVER, but we haven't sent a DHCPDISCOVER,
	   so pick an xid now. */
	client->xid = arc4random();

	/* Make a DHCPREQUEST packet, and set appropriate per-interface
	   flags. */
	make_request(client->active);
	client->destination.s_addr = INADDR_BROADCAST;
	client->first_sending = time(NULL);
	client->interval = 0;

	send_request();
}

/*
 * Called when a lease has completely expired and we've
 * been unable to renew it.
 */
void
state_init(void)
{
	/* Make a DHCPDISCOVER packet, and set appropriate per-interface
	   flags. */
	make_discover(client->active);
	client->xid = client->packet.xid;
	client->destination.s_addr = INADDR_BROADCAST;
	client->state = S_SELECTING;
	client->first_sending = time(NULL);
	client->interval = 0;

	send_discover();
}

/*
 * state_selecting is called when one or more DHCPOFFER packets
 * have been received and a configurable period of time has passed.
 */
void
state_selecting(void)
{
	struct client_lease *lp, *next, *picked;
	time_t cur_time;

	/* Cancel state_selecting and send_discover timeouts, since either
	   one could have got us here. */
	cancel_timeout();

	/* We have received one or more DHCPOFFER packets.   Currently,
	   the only criterion by which we judge leases is whether or
	   not we get a response when we arp for them. */
	picked = NULL;
	for (lp = client->offered_leases; lp; lp = next) {
		next = lp->next;
		if (!picked) {
			picked = lp;
		} else {
			make_decline(lp);
			send_decline();
			free_client_lease(lp);
		}
	}
	client->offered_leases = NULL;

	/* If we just tossed all the leases we were offered, go back
	   to square one. */
	if (!picked) {
		client->state = S_INIT;
		state_init();
		return;
	}

	picked->next = NULL;

	time(&cur_time);

	/* If it was a BOOTREPLY, we can just take the address right now. */
	if (!picked->options[DHO_DHCP_MESSAGE_TYPE].len) {
		client->new = picked;

		/* Make up some lease expiry times
		   XXX these should be configurable. */
		client->new->expiry = cur_time + 12000;
		client->new->renewal += cur_time + 8000;
		client->new->rebind += cur_time + 10000;

		client->state = S_REQUESTING;

		/* Bind to the address we received. */
		bind_lease();
		return;
	}

	/* Go to the REQUESTING state. */
	client->destination.s_addr = INADDR_BROADCAST;
	client->state = S_REQUESTING;
	client->first_sending = cur_time;
	client->interval = 0;

	/* Make a DHCPREQUEST packet from the lease we picked. */
	make_request(picked);
	client->xid = client->packet.xid;

	/* Toss the lease we picked - we'll get it back in a DHCPACK. */
	free_client_lease(picked);

	send_request();
}

void
dhcpack(struct in_addr client_addr, struct option_data *options)
{
	struct client_lease *lease;
	time_t cur_time;

	if (client->state != S_REBOOTING &&
	    client->state != S_REQUESTING &&
	    client->state != S_RENEWING &&
	    client->state != S_REBINDING)
		return;

	lease = packet_to_lease(client_addr, options);
	if (!lease) {
		note("packet_to_lease failed.");
		return;
	}

	client->new = lease;

	/* Stop resending DHCPREQUEST. */
	cancel_timeout();

	/* Figure out the lease time. */
	if (client->new->options[DHO_DHCP_LEASE_TIME].data)
		client->new->expiry =
		    getULong(client->new->options[DHO_DHCP_LEASE_TIME].data);
	else
		client->new->expiry = DEFAULT_LEASE_TIME;
	/* A number that looks negative here is really just very large,
	   because the lease expiry offset is unsigned. */
	if (client->new->expiry < 0)
		client->new->expiry = TIME_MAX;
	/* XXX should be fixed by resetting the client state */
	if (client->new->expiry < 60)
		client->new->expiry = 60;

	/* Take the server-provided renewal time if there is one;
	   otherwise figure it out according to the spec. */
	if (client->new->options[DHO_DHCP_RENEWAL_TIME].len)
		client->new->renewal =
		    getULong(client->new->options[DHO_DHCP_RENEWAL_TIME].data);
	else
		client->new->renewal = client->new->expiry / 2;

	/* Same deal with the rebind time. */
	if (client->new->options[DHO_DHCP_REBINDING_TIME].len)
		client->new->rebind =
		    getULong(client->new->options[DHO_DHCP_REBINDING_TIME].data);
	else
		client->new->rebind = client->new->renewal +
		    client->new->renewal / 2 + client->new->renewal / 4;

	time(&cur_time);

	client->new->expiry += cur_time;
	/* Lease lengths can never be negative. */
	if (client->new->expiry < cur_time)
		client->new->expiry = TIME_MAX;
	client->new->renewal += cur_time;
	if (client->new->renewal < cur_time)
		client->new->renewal = TIME_MAX;
	client->new->rebind += cur_time;
	if (client->new->rebind < cur_time)
		client->new->rebind = TIME_MAX;

	bind_lease();
}

void
bind_lease(void)
{
	struct in_addr gateway, mask;
	struct option_data *options;
	struct client_lease *lease;
	char *domainname, *nameservers;

	delete_addresses(ifi->name, ifi->rdomain);
	flush_routes_and_arp_cache(ifi->rdomain);

	lease = apply_defaults(client->new);
	options = lease->options;

	memset(&mask, 0, sizeof(mask));
	memcpy(&mask.s_addr, options[DHO_SUBNET_MASK].data,
	    options[DHO_SUBNET_MASK].len);

	if (options[DHO_DOMAIN_NAME].len)
		domainname = strdup(pretty_print_option(
		    DHO_DOMAIN_NAME, &options[DHO_DOMAIN_NAME], 0));
	else
		domainname = strdup("");
	if (options[DHO_DOMAIN_NAME_SERVERS].len) {
		nameservers = strdup(pretty_print_option(
		    DHO_DOMAIN_NAME_SERVERS,
		    &options[DHO_DOMAIN_NAME_SERVERS], 0));
	} else
		nameservers = strdup("");

	new_resolv_conf(ifi->name, domainname, nameservers);

        /*
	 * Add address and default route last, so we know when the binding
	 * is done by the RTM_NEWADDR message being received.
	 */
	add_address(ifi->name, ifi->rdomain, client->new->address, mask);
	if (options[DHO_ROUTERS].len) {
		memset(&gateway, 0, sizeof(gateway));
		/* XXX Only use FIRST router address for now. */
		memcpy(&gateway.s_addr, options[DHO_ROUTERS].data,
		    options[DHO_ROUTERS].len);
		add_default_route(ifi->rdomain, client->new->address, gateway);
	}

	free(domainname);
	free(nameservers);
	free_client_lease(lease);

	/* Replace the old active lease with the new one. */
	if (client->active)
		free_client_lease(client->active);
	client->active = client->new;
	client->new = NULL;

	client->state = S_BOUND;

	/* Write out new leases file. */
	rewrite_client_leases();

	/* Set timeout to start the renewal process. */
	set_timeout(client->active->renewal, state_bound);
}

/*
 * state_bound is called when we've successfully bound to a particular
 * lease, but the renewal time on that lease has expired.   We are
 * expected to unicast a DHCPREQUEST to the server that gave us our
 * original lease.
 */
void
state_bound(void)
{
	/* T1 has expired. */
	make_request(client->active);
	client->xid = client->packet.xid;

	if (client->active->options[DHO_DHCP_SERVER_IDENTIFIER].len == 4) {
		memcpy(&client->destination.s_addr,
		    client->active->options[DHO_DHCP_SERVER_IDENTIFIER].data,
		    client->active->options[DHO_DHCP_SERVER_IDENTIFIER].len);
	} else
		client->destination.s_addr = INADDR_BROADCAST;

	client->first_sending = time(NULL);
	client->interval = 0;
	client->state = S_RENEWING;

	send_request();
}

void
dhcpoffer(struct in_addr client_addr, struct option_data *options)
{
	struct client_lease *lease, *lp;
	int i;
	time_t stop_selecting;
	char *name = options[DHO_DHCP_MESSAGE_TYPE].len ? "DHCPOFFER" :
	    "BOOTREPLY";

	if (client->state != S_SELECTING)
		return;

	/* If this lease doesn't supply the minimum required parameters,
	   blow it off. */
	for (i = 0; i < config->required_option_count; i++) {
		if (!options[config->required_options[i]].len) {
			note("%s isn't satisfactory.", name);
			return;
		}
	}

	/* If we've already seen this lease, don't record it again. */
	for (lease = client->offered_leases;
	    lease; lease = lease->next) {
		if (!memcmp(&lease->address.s_addr, &client->packet.yiaddr,
		    sizeof(in_addr_t))) {
#ifdef DEBUG
			debug("%s already seen.", name);
#endif
			return;
		}
	}

	lease = packet_to_lease(client_addr, options);
	if (!lease) {
		note("packet_to_lease failed.");
		return;
	}

	/*
	 * Reject offers whose subnet is already configured on another
	 * interface.
	 */
	if (subnet_exists(lease))
		return;

	/* If this lease was acquired through a BOOTREPLY, record that
	   fact. */
	if (!options[DHO_DHCP_MESSAGE_TYPE].len)
		lease->is_bootp = 1;

	/* Figure out when we're supposed to stop selecting. */
	stop_selecting = client->first_sending + config->select_interval;

	/* If this is the lease we asked for, put it at the head of the
	   list, and don't mess with the arp request timeout. */
	if (lease->address.s_addr == client->requested_address.s_addr) {
		lease->next = client->offered_leases;
		client->offered_leases = lease;
	} else {
		/* Put the lease at the end of the list. */
		lease->next = NULL;
		if (!client->offered_leases)
			client->offered_leases = lease;
		else {
			for (lp = client->offered_leases; lp->next;
			    lp = lp->next)
				;	/* nothing */
			lp->next = lease;
		}
	}

	/* If the selecting interval has expired, go immediately to
	   state_selecting().  Otherwise, time out into
	   state_selecting at the select interval. */
	if (stop_selecting <= time(NULL))
		state_selecting();
	else {
		set_timeout(stop_selecting, state_selecting);
	}
}

/*
 * Allocate a client_lease structure and initialize it from the
 * parameters in the specified packet.
 */
struct client_lease *
packet_to_lease(struct in_addr client_addr, struct option_data *options)
{
	struct client_lease *lease;
	int i;

	lease = malloc(sizeof(struct client_lease));

	if (!lease) {
		warning("dhcpoffer: no memory to record lease.");
		return (NULL);
	}

	memset(lease, 0, sizeof(*lease));

	/* Copy the lease options. */
	for (i = 0; i < 256; i++) {
		if (options[i].len) {
			lease->options[i] = options[i];
			options[i].data = NULL;
			options[i].len = 0;
			if (!check_option(lease, i)) {
				warning("Invalid lease option - ignoring offer");
				free_client_lease(lease);
				return (NULL);
			}
		}
	}

	memcpy(&lease->address.s_addr, &client->packet.yiaddr,
	    sizeof(in_addr_t));

	/* If the server name was filled out, copy it. */
	if ((!lease->options[DHO_DHCP_OPTION_OVERLOAD].len ||
	    !(lease->options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 2)) &&
	    client->packet.sname[0]) {
		lease->server_name = malloc(DHCP_SNAME_LEN + 1);
		if (!lease->server_name) {
			warning("dhcpoffer: no memory for server name.");
			free_client_lease(lease);
			return (NULL);
		}
		memcpy(lease->server_name, client->packet.sname,
		    DHCP_SNAME_LEN);
		lease->server_name[DHCP_SNAME_LEN] = '\0';
		if (!res_hnok(lease->server_name)) {
			warning("Bogus server name %s", lease->server_name);
			free(lease->server_name);
			lease->server_name = NULL;
		}
	}

	/* Ditto for the filename. */
	if ((!lease->options[DHO_DHCP_OPTION_OVERLOAD].len ||
	    !(lease->options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 1)) &&
	    client->packet.file[0]) {
		/* Don't count on the NUL terminator. */
		lease->filename = malloc(DHCP_FILE_LEN + 1);
		if (!lease->filename) {
			warning("dhcpoffer: no memory for filename.");
			free_client_lease(lease);
			return (NULL);
		}
		memcpy(lease->filename, client->packet.file, DHCP_FILE_LEN);
		lease->filename[DHCP_FILE_LEN] = '\0';
	}
	return lease;
}

void
dhcpnak(struct in_addr client_addr, struct option_data *options)
{
	if (client->state != S_REBOOTING &&
	    client->state != S_REQUESTING &&
	    client->state != S_RENEWING &&
	    client->state != S_REBINDING)
		return;

	if (!client->active) {
		note("DHCPNAK with no active lease.");
		return;
	}

	free_client_lease(client->active);
	client->active = NULL;

	/* Stop sending DHCPREQUEST packets... */
	cancel_timeout();

	client->state = S_INIT;
	state_init();
}

/*
 * Send out a DHCPDISCOVER packet, and set a timeout to send out another
 * one after the right interval has expired.  If we don't get an offer by
 * the time we reach the panic interval, call the panic function.
 */
void
send_discover(void)
{
	time_t cur_time;
	int interval, increase = 1;

	time(&cur_time);

	/* Figure out how long it's been since we started transmitting. */
	interval = cur_time - client->first_sending;

	if (interval > config->timeout) {
		state_panic();
		return;
	}

	/*
	 * If we're supposed to increase the interval, do so.  If it's
	 * currently zero (i.e., we haven't sent any packets yet), set
	 * it to initial_interval; otherwise, add to it a random
	 * number between zero and two times itself.  On average, this
	 * means that it will double with every transmission.
	 */
	if (increase) {
		if (!client->interval)
			client->interval = config->initial_interval;
		else {
			client->interval += (arc4random() >> 2) %
			    (2 * client->interval);
		}

		/* Don't backoff past cutoff. */
		if (client->interval > config->backoff_cutoff)
			client->interval = ((config->backoff_cutoff / 2)
				 + ((arc4random() >> 2) %
				    config->backoff_cutoff));
	} else if (!client->interval)
		client->interval = config->initial_interval;

	/* If the backoff would take us to the panic timeout, just use that
	   as the interval. */
	if (cur_time + client->interval >
	    client->first_sending + config->timeout)
		client->interval = (client->first_sending +
			 config->timeout) - cur_time + 1;

	/* Record the number of seconds since we started sending. */
	if (interval < 65536)
		client->packet.secs = htons(interval);
	else
		client->packet.secs = htons(65535);
	client->secs = client->packet.secs;

	note("DHCPDISCOVER on %s to %s port %d interval %d",
	    ifi->name, inet_ntoa(sockaddr_broadcast.sin_addr),
	    ntohs(sockaddr_broadcast.sin_port), client->interval);

	send_packet(inaddr_any, &sockaddr_broadcast, NULL);

	set_timeout_interval(client->interval, send_discover);
}

/*
 * state_panic gets called if we haven't received any offers in a preset
 * amount of time.   When this happens, we try to use existing leases
 * that haven't yet expired.
 */
void
state_panic(void)
{
	struct client_lease *loop = client->active;
	struct client_lease *lp;
	time_t cur_time;

	note("No DHCPOFFERS received.");

	/* We may not have an active lease, but we may have some
	   predefined leases that we can try. */
	if (!client->active && client->leases)
		goto activate_next;

	/* Run through the list of leases and see if one can be used. */
	time(&cur_time);
	while (client->active) {
		if (client->active->expiry > cur_time) {
			note("Trying recorded lease %s",
			    inet_ntoa(client->active->address));

			/*
			 * If the old lease is still good and doesn't
			 * yet need renewal, go into BOUND state and
			 * timeout at the renewal time.
			 */
			if (cur_time < client->active->renewal) {
				client->state = S_BOUND;
				note("bound: renewal in %lld seconds.",
				    (long long)(client->active->renewal
				    - cur_time));
				set_timeout(client->active->renewal,
				    state_bound);
			} else {
				client->state = S_BOUND;
				note("bound: immediate renewal.");
				state_bound();
			}
			go_daemon();
			return;
		}

		/* If there are no other leases, give up. */
		if (!client->leases) {
			client->leases = client->active;
			client->active = NULL;
			break;
		}

activate_next:
		/* Otherwise, put the active lease at the end of the
		   lease list, and try another lease.. */
		for (lp = client->leases; lp->next; lp = lp->next)
			;
		lp->next = client->active;
		if (lp->next)
			lp->next->next = NULL;
		client->active = client->leases;
		client->leases = client->leases->next;

		/* If we already tried this lease, we've exhausted the
		   set of leases, so we might as well give up for
		   now. */
		if (client->active == loop)
			break;
		else if (!loop)
			loop = client->active;
	}

	/*
	 * No leases were available, or what was available didn't work
	 */
	note("No working leases in persistent database - sleeping.");
	client->state = S_INIT;
	set_timeout_interval(config->retry_interval, state_init);
	go_daemon();
}

void
send_request(void)
{
	struct sockaddr_in destination;
	struct in_addr from;
	time_t cur_time;
	int interval;

	time(&cur_time);

	/* Figure out how long it's been since we started transmitting. */
	interval = (int)(cur_time - client->first_sending);

	/* If we're in the INIT-REBOOT or REQUESTING state and we're
	   past the reboot timeout, go to INIT and see if we can
	   DISCOVER an address... */
	/* XXX In the INIT-REBOOT state, if we don't get an ACK, it
	   means either that we're on a network with no DHCP server,
	   or that our server is down.  In the latter case, assuming
	   that there is a backup DHCP server, DHCPDISCOVER will get
	   us a new address, but we could also have successfully
	   reused our old address.  In the former case, we're hosed
	   anyway.  This is not a win-prone situation. */
	if ((client->state == S_REBOOTING ||
	    client->state == S_REQUESTING) &&
	    interval > config->reboot_timeout) {
		client->state = S_INIT;
		cancel_timeout();
		state_init();
		return;
	}

	/* If the lease has expired, relinquish the address and go back
	   to the INIT state. */
	if (client->state != S_REQUESTING &&
	    cur_time > client->active->expiry) {
		if (client->active) {
			delete_address(ifi->name, ifi->rdomain,
			    client->active->address);
		}
		client->state = S_INIT;
		state_init();
		return;
	}

	/* Do the exponential backoff... */
	if (!client->interval)
		client->interval = config->initial_interval;
	else
		client->interval += ((arc4random() >> 2) %
		    (2 * client->interval));

	/* Don't backoff past cutoff. */
	if (client->interval > config->backoff_cutoff)
		client->interval = ((config->backoff_cutoff / 2) +
		    ((arc4random() >> 2) % client->interval));

	/* If the backoff would take us to the expiry time, just set the
	   timeout to the expiry time. */
	if (client->state != S_REQUESTING && cur_time + client->interval >
	    client->active->expiry)
		client->interval = client->active->expiry - cur_time + 1;

	/* If the lease T2 time has elapsed, or if we're not yet bound,
	   broadcast the DHCPREQUEST rather than unicasting. */
	memset(&destination, 0, sizeof(destination));
	if (client->state == S_REQUESTING ||
	    client->state == S_REBOOTING ||
	    cur_time > client->active->rebind)
		destination.sin_addr.s_addr = INADDR_BROADCAST;
	else
		destination.sin_addr.s_addr = client->destination.s_addr;
	destination.sin_port = htons(REMOTE_PORT);
	destination.sin_family = AF_INET;
	destination.sin_len = sizeof(destination);

	if (client->state != S_REQUESTING)
		from.s_addr = client->active->address.s_addr;
	else
		from.s_addr = INADDR_ANY;

	/* Record the number of seconds since we started sending. */
	if (client->state == S_REQUESTING)
		client->packet.secs = client->secs;
	else {
		if (interval < 65536)
			client->packet.secs = htons(interval);
		else
			client->packet.secs = htons(65535);
	}

	note("DHCPREQUEST on %s to %s port %d", ifi->name,
	    inet_ntoa(destination.sin_addr), ntohs(destination.sin_port));

	send_packet(from, &destination, NULL);

	set_timeout_interval(client->interval, send_request);
}

void
send_decline(void)
{
	note("DHCPDECLINE on %s to %s port %d", ifi->name,
	    inet_ntoa(sockaddr_broadcast.sin_addr),
	    ntohs(sockaddr_broadcast.sin_port));

	send_packet(inaddr_any, &sockaddr_broadcast, NULL);
}

void
make_discover(struct client_lease *lease)
{
	unsigned char discover = DHCPDISCOVER;
	struct option_data options[256];
	int i;

	memset(options, 0, sizeof(options));
	memset(&client->packet, 0, sizeof(client->packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPDISCOVER */
	i = DHO_DHCP_MESSAGE_TYPE;
	options[i].data = &discover;
	options[i].len = sizeof(discover);

	/* Request the options we want */
	i  = DHO_DHCP_PARAMETER_REQUEST_LIST;
	options[i].data = config->requested_options;
	options[i].len = config->requested_option_count;

	/* If we had an address, try to get it again. */
	if (lease) {
		client->requested_address = lease->address;
		i = DHO_DHCP_REQUESTED_ADDRESS;
		options[i].data = (char *)&lease->address;
		options[i].len = sizeof(lease->address);
	} else
		client->requested_address.s_addr = INADDR_ANY;

	/* Send any options requested in the config file. */
	for (i = 0; i < 256; i++)
		if (!options[i].data &&
		    config->send_options[i].data) {
			options[i].data = config->send_options[i].data;
			options[i].len = config->send_options[i].len;
		}

	/* Set up the option buffer to fit in a minimal UDP packet. */
	i = cons_options(options);
	if (i == -1 || client->packet.options[i] != DHO_END)
		error("options do not fit in DHCPDISCOVER packet.");
	client->packet_length = DHCP_FIXED_NON_UDP+i+1;
	if (client->packet_length < BOOTP_MIN_LEN)
		client->packet_length = BOOTP_MIN_LEN;

	client->packet.op = BOOTREQUEST;
	client->packet.htype = ifi->hw_address.htype;
	client->packet.hlen = ifi->hw_address.hlen;
	client->packet.hops = 0;
	client->packet.xid = arc4random();
	client->packet.secs = 0; /* filled in by send_discover. */
	client->packet.flags = 0;

	memset(&client->packet.ciaddr, 0, sizeof(client->packet.ciaddr));
	memset(&client->packet.yiaddr, 0, sizeof(client->packet.yiaddr));
	memset(&client->packet.siaddr, 0, sizeof(client->packet.siaddr));
	memset(&client->packet.giaddr, 0, sizeof(client->packet.giaddr));
	memcpy(client->packet.chaddr, ifi->hw_address.haddr,
	    ifi->hw_address.hlen);
}

void
make_request(struct client_lease * lease)
{
	unsigned char request = DHCPREQUEST;
	struct option_data options[256];
	int i;

	memset(options, 0, sizeof(options));
	memset(&client->packet, 0, sizeof(client->packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPREQUEST */
	i = DHO_DHCP_MESSAGE_TYPE;
	options[i].data = &request;
	options[i].len = sizeof(request);

	/* Request the options we want */
	i = DHO_DHCP_PARAMETER_REQUEST_LIST;
	options[i].data = config->requested_options;
	options[i].len = config->requested_option_count;

	/* If we are requesting an address that hasn't yet been assigned
	   to us, use the DHCP Requested Address option. */
	if (client->state == S_REQUESTING) {
		/* Send back the server identifier... */
		i = DHO_DHCP_SERVER_IDENTIFIER;
		options[i].data = lease->options[i].data;
		options[i].len = lease->options[i].len;
	}
	if (client->state == S_REQUESTING ||
	    client->state == S_REBOOTING) {
		client->requested_address = lease->address;
		i = DHO_DHCP_REQUESTED_ADDRESS;
		options[i].data = (char *)&lease->address.s_addr;
		options[i].len = sizeof(in_addr_t);
	}

	/* Send any options requested in the config file. */
	for (i = 0; i < 256; i++)
		if (!options[i].data && config->send_options[i].data) {
			options[i].data = config->send_options[i].data;
			options[i].len = config->send_options[i].len;
		}

	/* Set up the option buffer to fit in a minimal UDP packet. */
	i = cons_options(options);
	if (i == -1 || client->packet.options[i] != DHO_END)
		error("options do not fit in DHCPREQUEST packet.");
	client->packet_length = DHCP_FIXED_NON_UDP+i+1;
	if (client->packet_length < BOOTP_MIN_LEN)
		client->packet_length = BOOTP_MIN_LEN;

	client->packet.op = BOOTREQUEST;
	client->packet.htype = ifi->hw_address.htype;
	client->packet.hlen = ifi->hw_address.hlen;
	client->packet.hops = 0;
	client->packet.xid = client->xid;
	client->packet.secs = 0; /* Filled in by send_request. */
	client->packet.flags = 0;

	/* If we own the address we're requesting, put it in ciaddr;
	   otherwise set ciaddr to zero. */
	if (client->state == S_BOUND ||
	    client->state == S_RENEWING ||
	    client->state == S_REBINDING) {
		memcpy(&client->packet.ciaddr, &lease->address.s_addr,
		    sizeof(in_addr_t));
	} else {
		memset(&client->packet.ciaddr, 0,
		    sizeof(client->packet.ciaddr));
	}

	memset(&client->packet.yiaddr, 0, sizeof(client->packet.yiaddr));
	memset(&client->packet.siaddr, 0, sizeof(client->packet.siaddr));
	memset(&client->packet.giaddr, 0, sizeof(client->packet.giaddr));
	memcpy(client->packet.chaddr, ifi->hw_address.haddr,
	    ifi->hw_address.hlen);
}

void
make_decline(struct client_lease *lease)
{
	struct option_data options[256];
	unsigned char decline = DHCPDECLINE;
	int i;

	memset(options, 0, sizeof(options));
	memset(&client->packet, 0, sizeof(client->packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPDECLINE */
	i = DHO_DHCP_MESSAGE_TYPE;
	options[i].data = &decline;
	options[i].len = sizeof(decline);

	/* Send back the server identifier... */
	i = DHO_DHCP_SERVER_IDENTIFIER;
	options[i].data = lease->options[i].data;
	options[i].len = lease->options[i].len;

	/* Send back the address we're declining. */
	i = DHO_DHCP_REQUESTED_ADDRESS;
	options[i].data = (char *)&lease->address.s_addr;
	options[i].len = sizeof(in_addr_t);

	/* Send the uid if the user supplied one. */
	i = DHO_DHCP_CLIENT_IDENTIFIER;
	if (config->send_options[i].len) {
		options[i].data = config->send_options[i].data;
		options[i].len = config->send_options[i].len;
	}

	/* Set up the option buffer to fit in a minimal UDP packet. */
	i = cons_options(options);
	if (i == -1 || client->packet.options[i] != DHO_END)
		error("options do not fit in DHCPDECLINE packet.");
	client->packet_length = DHCP_FIXED_NON_UDP+i+1;
	if (client->packet_length < BOOTP_MIN_LEN)
		client->packet_length = BOOTP_MIN_LEN;

	client->packet.op = BOOTREQUEST;
	client->packet.htype = ifi->hw_address.htype;
	client->packet.hlen = ifi->hw_address.hlen;
	client->packet.hops = 0;
	client->packet.xid = client->xid;
	client->packet.secs = 0; /* Filled in by send_request. */
	client->packet.flags = 0;

	/* ciaddr must always be zero. */
	memset(&client->packet.ciaddr, 0, sizeof(client->packet.ciaddr));
	memset(&client->packet.yiaddr, 0, sizeof(client->packet.yiaddr));
	memset(&client->packet.siaddr, 0, sizeof(client->packet.siaddr));
	memset(&client->packet.giaddr, 0, sizeof(client->packet.giaddr));
	memcpy(client->packet.chaddr, ifi->hw_address.haddr,
	    ifi->hw_address.hlen);
}

void
free_client_lease(struct client_lease *lease)
{
	int i;

	if (lease->server_name)
		free(lease->server_name);
	if (lease->filename)
		free(lease->filename);
	for (i = 0; i < 256; i++) {
		if (lease->options[i].len)
			free(lease->options[i].data);
	}
	free(lease);
}

void
rewrite_client_leases(void)
{
	struct client_lease *lp;

	if (!leaseFile)	/* XXX */
		error("lease file not open");

	fflush(leaseFile);
	rewind(leaseFile);

	for (lp = client->leases; lp; lp = lp->next) {
		if (client->active && lp->address.s_addr ==
		    client->active->address.s_addr)
			continue;
		write_client_lease(lp);
	}

	if (client->active)
		write_client_lease(client->active);

	fflush(leaseFile);
	ftruncate(fileno(leaseFile), ftello(leaseFile));
	fsync(fileno(leaseFile));
}

void
write_client_lease(struct client_lease *lease)
{
	struct tm *t;
	int i;

	/* If the lease came from the config file, we don't need to stash
	   a copy in the lease database. */
	if (lease->is_static)
		return;

	if (!leaseFile)	/* XXX */
		error("lease file not open");

	fprintf(leaseFile, "lease {\n");
	if (lease->is_bootp)
		fprintf(leaseFile, "  bootp;\n");
	fprintf(leaseFile, "  interface \"%s\";\n", ifi->name);
	fprintf(leaseFile, "  fixed-address %s;\n", inet_ntoa(lease->address));
	if (lease->filename)
		fprintf(leaseFile, "  filename \"%s\";\n", lease->filename);
	if (lease->server_name)
		fprintf(leaseFile, "  server-name \"%s\";\n",
		    lease->server_name);
	for (i = 0; i < 256; i++)
		if (lease->options[i].len)
			fprintf(leaseFile, "  option %s %s;\n",
			    dhcp_options[i].name,
			    pretty_print_option(i, &lease->options[i], 1));

	t = gmtime(&lease->renewal);
	fprintf(leaseFile, "  renew %d %d/%d/%d %02d:%02d:%02d;\n",
	    t->tm_wday, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
	    t->tm_hour, t->tm_min, t->tm_sec);
	t = gmtime(&lease->rebind);
	fprintf(leaseFile, "  rebind %d %d/%d/%d %02d:%02d:%02d;\n",
	    t->tm_wday, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
	    t->tm_hour, t->tm_min, t->tm_sec);
	t = gmtime(&lease->expiry);
	fprintf(leaseFile, "  expire %d %d/%d/%d %02d:%02d:%02d;\n",
	    t->tm_wday, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
	    t->tm_hour, t->tm_min, t->tm_sec);
	fprintf(leaseFile, "}\n");
	fflush(leaseFile);
}

void
go_daemon(void)
{
	static int state = 0;

	if (no_daemon || state)
		return;

	state = 1;

	/* Stop logging to stderr... */
	log_perror = 0;

	if (daemon(1, 0) == -1)
		error("daemon");

	/* we are chrooted, daemon(3) fails to open /dev/null */
	if (nullfd != -1) {
		dup2(nullfd, STDIN_FILENO);
		dup2(nullfd, STDOUT_FILENO);
		dup2(nullfd, STDERR_FILENO);
		close(nullfd);
		nullfd = -1;
	}

	/*
	 * Catch stuff that might be trying to terminate the program.
	 */
	signal(SIGHUP, sighdlr);
	signal(SIGINT, sighdlr);
	signal(SIGTERM, sighdlr);
	signal(SIGUSR1, sighdlr);
	signal(SIGUSR2, sighdlr);

	signal(SIGPIPE, SIG_IGN);
}

int
check_option(struct client_lease *l, int option)
{
	char *opbuf;
	char *sbuf;

	/* we use this, since this is what gets passed to dhclient-script */

	opbuf = pretty_print_option(option, &l->options[option], 0);

	sbuf = option_as_string(option, l->options[option].data,
	    l->options[option].len);

	switch (option) {
	case DHO_SUBNET_MASK:
	case DHO_SWAP_SERVER:
	case DHO_BROADCAST_ADDRESS:
	case DHO_DHCP_SERVER_IDENTIFIER:
	case DHO_ROUTER_SOLICITATION_ADDRESS:
	case DHO_DHCP_REQUESTED_ADDRESS:
		if (ipv4addrs(opbuf) == 0) {
			warning("Invalid IP address in option %s: %s",
			    dhcp_options[option].name, opbuf);
			return (0);
		}
		if (l->options[option].len != 4) { /* RFC 2132 */
			warning("warning: Only 1 IP address allowed in "
			    "%s option; length %d, must be 4",
			    dhcp_options[option].name,
			    l->options[option].len);
			l->options[option].len = 4;
		}
		return (1);
	case DHO_TIME_SERVERS:
	case DHO_NAME_SERVERS:
	case DHO_ROUTERS:
	case DHO_DOMAIN_NAME_SERVERS:
	case DHO_LOG_SERVERS:
	case DHO_COOKIE_SERVERS:
	case DHO_LPR_SERVERS:
	case DHO_IMPRESS_SERVERS:
	case DHO_RESOURCE_LOCATION_SERVERS:
	case DHO_NIS_SERVERS:
	case DHO_NTP_SERVERS:
	case DHO_NETBIOS_NAME_SERVERS:
	case DHO_NETBIOS_DD_SERVER:
	case DHO_FONT_SERVERS:
		if (ipv4addrs(opbuf) == 0) {
			warning("Invalid IP address in option %s: %s",
			    dhcp_options[option].name, opbuf);
			return (0);
		}
		return (1);
	case DHO_HOST_NAME:
	case DHO_DOMAIN_NAME:
	case DHO_NIS_DOMAIN:
		if (!res_hnok(sbuf)) {
			warning("Bogus Host Name option %d: %s (%s)", option,
			    sbuf, opbuf);
			l->options[option].len = 0;
			free(l->options[option].data);
		}
		return (1);
	case DHO_PAD:
	case DHO_TIME_OFFSET:
	case DHO_BOOT_SIZE:
	case DHO_MERIT_DUMP:
	case DHO_ROOT_PATH:
	case DHO_EXTENSIONS_PATH:
	case DHO_IP_FORWARDING:
	case DHO_NON_LOCAL_SOURCE_ROUTING:
	case DHO_POLICY_FILTER:
	case DHO_MAX_DGRAM_REASSEMBLY:
	case DHO_DEFAULT_IP_TTL:
	case DHO_PATH_MTU_AGING_TIMEOUT:
	case DHO_PATH_MTU_PLATEAU_TABLE:
	case DHO_INTERFACE_MTU:
	case DHO_ALL_SUBNETS_LOCAL:
	case DHO_PERFORM_MASK_DISCOVERY:
	case DHO_MASK_SUPPLIER:
	case DHO_ROUTER_DISCOVERY:
	case DHO_STATIC_ROUTES:
	case DHO_TRAILER_ENCAPSULATION:
	case DHO_ARP_CACHE_TIMEOUT:
	case DHO_IEEE802_3_ENCAPSULATION:
	case DHO_DEFAULT_TCP_TTL:
	case DHO_TCP_KEEPALIVE_INTERVAL:
	case DHO_TCP_KEEPALIVE_GARBAGE:
	case DHO_VENDOR_ENCAPSULATED_OPTIONS:
	case DHO_NETBIOS_NODE_TYPE:
	case DHO_NETBIOS_SCOPE:
	case DHO_X_DISPLAY_MANAGER:
	case DHO_DHCP_LEASE_TIME:
	case DHO_DHCP_OPTION_OVERLOAD:
	case DHO_DHCP_MESSAGE_TYPE:
	case DHO_DHCP_PARAMETER_REQUEST_LIST:
	case DHO_DHCP_MESSAGE:
	case DHO_DHCP_MAX_MESSAGE_SIZE:
	case DHO_DHCP_RENEWAL_TIME:
	case DHO_DHCP_REBINDING_TIME:
	case DHO_DHCP_CLASS_IDENTIFIER:
	case DHO_DHCP_CLIENT_IDENTIFIER:
	case DHO_DHCP_USER_CLASS_ID:
	case DHO_TFTP_SERVER:
	case DHO_END:
		return (1);
	default:
		if (!unknown_ok)
			warning("unknown dhcp option value 0x%x", option);
		return (unknown_ok);
	}
}

int
res_hnok(const char *name)
{
	const char *dn = name;
	int pch = '.', ch = *dn++;
	int warn = 0;

	while (ch != '\0') {
		int nch = *dn++;

		if (ch == '.') {
			;
		} else if (pch == '.' || nch == '.' || nch == '\0') {
			if (!isalnum(ch))
				return (0);
		} else if (!isalnum(ch) && ch != '-' && ch != '_')
				return (0);
		else if (ch == '_' && warn == 0) {
			warning("warning: hostname %s contains an "
			    "underscore which violates RFC 952", name);
			warn++;
		}
		pch = ch, ch = nch;
	}
	return (1);
}

/* Does buf consist only of dotted decimal ipv4 addrs?
 * return how many if so,
 * otherwise, return 0
 */
int
ipv4addrs(char * buf)
{
	struct in_addr jnk;
	int count = 0;

	while (inet_aton(buf, &jnk) == 1){
		count++;
		while (*buf == '.' || isdigit(*buf))
			buf++;
		if (*buf == '\0')
			return (count);
		while (*buf ==  ' ')
			buf++;
	}
	return (0);
}

char *
option_as_string(unsigned int code, unsigned char *data, int len)
{
	static char optbuf[32768]; /* XXX */
	char *op = optbuf;
	int opleft = sizeof(optbuf);
	unsigned char *dp = data;

	if (code > 255)
		error("option_as_string: bad code %d", code);

	for (; dp < data + len; dp++) {
		if (!isascii(*dp) || !isprint(*dp)) {
			if (dp + 1 != data + len || *dp != 0) {
				size_t oplen;
				snprintf(op, opleft, "\\%03o", *dp);
				oplen = strlen(op);
				op += oplen;
				opleft -= oplen;
			}
		} else if (*dp == '"' || *dp == '\'' || *dp == '$' ||
		    *dp == '`' || *dp == '\\') {
			*op++ = '\\';
			*op++ = *dp;
			opleft -= 2;
		} else {
			*op++ = *dp;
			opleft--;
		}
	}
	if (opleft < 1)
		goto toobig;
	*op = 0;
	return optbuf;
toobig:
	warning("dhcp option too large");
	return "<error>";
}

void
fork_privchld(int fd, int fd2)
{
	struct imsg_cleanup imsg;
	struct pollfd pfd[1];
	struct imsgbuf *priv_ibuf;
	ssize_t n;
	int nfds;

	switch (fork()) {
	case -1:
		error("cannot fork");
		break;
	case 0:
		break;
	default:
		return;
	}

	if (chdir("/") == -1)
		error("chdir(\"/\")");

	setproctitle("%s [priv]", ifi->name);

	go_daemon();

	close(fd2);

	if ((priv_ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		error("no memory for priv_ibuf");

	imsg_init(priv_ibuf, fd);

	while (quit == 0) {
		pfd[0].fd = priv_ibuf->fd;
		pfd[0].events = POLLIN;
		if ((nfds = poll(pfd, 1, INFTIM)) == -1) {
			if (errno != EINTR) {
				warning("poll error: %s", strerror(errno));
				break;
			}
			continue;
		} 

		if (nfds == 0 || !(pfd[0].revents & POLLIN))
			continue;

		if ((n = imsg_read(priv_ibuf)) == -1) {
			warning("imsg_read(priv_ibuf): %s", strerror(errno));
			break;
		}

		if (n == 0) {	/* connection closed */
			warning("dispatch_imsg in main: pipe closed");
			break;
		}

		dispatch_imsg(priv_ibuf);
	}

	imsg_clear(priv_ibuf);
	close(fd);

	memset(&imsg, 0, sizeof(imsg));
	strlcpy(imsg.ifname, ifi->name, sizeof(imsg.ifname));
	imsg.rdomain = ifi->rdomain;
	imsg.addr = active_addr;

	priv_cleanup(&imsg);

	if (quit == SIGHUP) {
		warning("Received SIGHUP; restarting.");
		signal(SIGHUP, SIG_IGN); /* will be restored after exec */
		execvp(saved_argv[0], saved_argv);
		error("RESTART FAILED: '%s': %s", saved_argv[0],
		    strerror(errno));
	}

	exit(1);
}

void
get_ifname(char *arg)
{
	struct ifgroupreq ifgr;
	struct ifg_req *ifg;
	int s, len;

	if (!strcmp(arg, "egress")) {
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s == -1)
			error("socket error");
		memset(&ifgr, 0, sizeof(ifgr));
		strlcpy(ifgr.ifgr_name, "egress", sizeof(ifgr.ifgr_name));
		if (ioctl(s, SIOCGIFGMEMB, (caddr_t)&ifgr) == -1) {
			if (errno == ENOENT)
				error("no interface in group egress found");
			error("ioctl SIOCGIFGMEMB: %s", strerror(errno));
		}
		len = ifgr.ifgr_len;
		if ((ifgr.ifgr_groups = calloc(1, len)) == NULL)
			error("get_ifname");
		if (ioctl(s, SIOCGIFGMEMB, (caddr_t)&ifgr) == -1)
			error("ioctl SIOCGIFGMEMB: %s", strerror(errno));

		arg = NULL;
		for (ifg = ifgr.ifgr_groups;
		    ifg && len >= sizeof(struct ifg_req); ifg++) {
			len -= sizeof(struct ifg_req);
			if (arg)
				error("too many interfaces in group egress");
			arg = ifg->ifgrq_member;
		}

		if (strlcpy(ifi->name, arg, IFNAMSIZ) >= IFNAMSIZ)
			error("Interface name too long: %s", strerror(errno));

		free(ifgr.ifgr_groups);
		close(s);
	} else if (strlcpy(ifi->name, arg, IFNAMSIZ) >= IFNAMSIZ)
		error("Interface name too long");
}

/*
 * Update resolv.conf.
 */

void
new_resolv_conf(char *ifname, char *domainname, char *nameservers)
{
	struct imsg_resolv_conf  imsg;
	char			*p;
	int			 rslt;

	memset(&imsg, 0, sizeof(imsg));

	/* Build string of contents of new resolv.conf. */
	if (domainname && strlen(domainname)) {
		strlcat(imsg.contents, "search ", MAXRESOLVCONFSIZE);
		strlcat(imsg.contents, domainname, MAXRESOLVCONFSIZE);
		strlcat(imsg.contents, "\n", MAXRESOLVCONFSIZE);
	}

	for (p = strsep(&nameservers, " "); p != NULL;
	    p = strsep(&nameservers, " ")) {
		if (*p == '\0')
			continue;
		strlcat(imsg.contents, "nameserver ", MAXRESOLVCONFSIZE);
		strlcat(imsg.contents, p, MAXRESOLVCONFSIZE);
		strlcat(imsg.contents, "\n", MAXRESOLVCONFSIZE);
	}

	rslt = imsg_compose(unpriv_ibuf, IMSG_NEW_RESOLV_CONF, 0, 0, -1, &imsg,
	    sizeof(imsg));

	if (rslt == -1)
		warning("new_resolv_conf: imsg_compose: %s", strerror(errno));
}

void
priv_resolv_conf(struct imsg_resolv_conf *imsg)
{
	ssize_t n, tailn;
	int conffd, tailfd;
	char *buf;

	if (strlen(imsg->contents) == 0)
		return;

	conffd = open("/etc/resolv.conf",
	    O_WRONLY | O_CREAT | O_TRUNC | O_SYNC | O_EXLOCK,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (conffd == -1) {
		note("Couldn't open resolv.conf: %s", strerror(errno));
		return;
	}

	n = write(conffd, imsg->contents, strlen(imsg->contents));
	if (n == -1)
		note("Couldn't write contents to resolv.conf: %s",
		    strerror(errno));
	else if (n < strlen(imsg->contents))
		note("Short contents write to resolv.conf (%zd vs %zd)",
		    n, strlen(imsg->contents));

	tailfd = open("/etc/resolv.conf.tail", O_RDONLY);
	if (tailfd != -1) {
		buf = calloc(1, MAXRESOLVCONFSIZE);
		if (buf == NULL) {
			note("Can't allocate buf for resolv.conf.tail: %s",
			    strerror(errno));
			close(tailfd);
			goto done;
		}

		tailn = read(tailfd, buf, MAXRESOLVCONFSIZE - 1);
		close(tailfd);

		if (tailn == -1)
			note("Couldn't read resolv.conf.tail: %s",
			    strerror(errno));
		else if (tailn == 0)
			note("Got no data from resolv.conf.tail");
		else if (tailn > 0) {
			n = write(conffd, buf, strlen(buf));
			if (n == -1)
				note("Couldn't write tail to resolv.conf: %s",
				    strerror(errno));
			else if (n == 0)
				note("Couldn't write tail to resolv.conf");
			else if (n < strlen(buf))
				note("Short tail write to resolv.conf "
				    "(%zd vs %zd)", n, strlen(buf));
		}
		free(buf);
	}

done:
	fchmod(conffd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	fchown(conffd, 0, 0); /* root:wheel */

	close(conffd);
}

struct client_lease *
apply_defaults(struct client_lease *lease)
{
	struct client_lease *newlease;
	int i, j;

	newlease = clone_lease(lease);
	if (newlease == NULL)
		error("Unable to clone lease");

	for (i = 0; i < 256; i++) {
		for (j = 0; j < config->ignored_option_count; j++) {
			if (config->ignored_options[j] == i) {
				free(newlease->options[i].data);
				newlease->options[i].data = NULL;
				newlease->options[i].len = 0;
				break;
			}
		}
		if (j < config->ignored_option_count)
			continue;
		
		switch (config->default_actions[i]) {
		case ACTION_SUPERSEDE:
			if (newlease->options[i].len != 0)
				free(newlease->options[i].data);
			newlease->options[i].len = config->defaults[i].len;
			newlease->options[i].data = calloc(1, 
			    config->defaults[i].len);
			if (newlease->options[i].data == NULL)
				goto cleanup;
			memcpy(newlease->options[i].data,
			    config->defaults[i].data, config->defaults[i].len);
			break;

		case ACTION_PREPEND:
			if (newlease->options[i].len != 0)
				free(newlease->options[i].data);
			newlease->options[i].len = config->defaults[i].len +
			    lease->options[i].len;
			newlease->options[i].data = calloc(1,
			    newlease->options[i].len);
			if (newlease->options[i].data == NULL)
				goto cleanup;
			memcpy(newlease->options[i].data,
			    config->defaults[i].data, config->defaults[i].len);
			memcpy(newlease->options[i].data + 
			    config->defaults[i].len, lease->options[i].data,
			    lease->options[i].len);
			break;

		case ACTION_APPEND:
			if (newlease->options[i].len != 0)
				free(newlease->options[i].data);
			newlease->options[i].len = config->defaults[i].len +
			    lease->options[i].len;
			newlease->options[i].data = calloc(1,
			    newlease->options[i].len);
			if (newlease->options[i].data == NULL)
				goto cleanup;
			memcpy(newlease->options[i].data,
			    lease->options[i].data, lease->options[i].len);
			memcpy(newlease->options[i].data + 
			    lease->options[i].len, config->defaults[i].data,
			    config->defaults[i].len);
			break;

		case ACTION_DEFAULT:
			if ((newlease->options[i].len == 0) &&
			    (config->defaults[i].len != 0)) {
				newlease->options[i].len =
				    config->defaults[i].len;
				newlease->options[i].data = calloc(1, 
				    config->defaults[i].len);
				if (newlease->options[i].data == NULL)
					goto cleanup;
				memcpy(newlease->options[i].data,
				    config->defaults[i].data,
				    config->defaults[i].len);
			}
			break;

		default:
			break;
		}
	}

	return (newlease);

cleanup:
	if (newlease)
		free_client_lease(newlease);

	error("Unable to apply defaults");
	/* NOTREACHED */

	return (NULL);
}

struct client_lease *
clone_lease(struct client_lease *oldlease)
{
	struct client_lease *newlease;
	int i;

	newlease = calloc(1, sizeof(struct client_lease));
	if (newlease == NULL)
		goto cleanup;

	newlease->expiry = oldlease->expiry;
	newlease->renewal = oldlease->renewal;
	newlease->rebind = oldlease->rebind;
	newlease->is_static = oldlease->is_static;
	newlease->is_bootp = oldlease->is_bootp;

	if (oldlease->server_name) {
		newlease->server_name = strdup(oldlease->server_name);
		if (newlease->server_name == NULL)
			goto cleanup;
	}
	if (oldlease->filename) {
		newlease->filename = strdup(oldlease->filename);
		if (newlease->filename == NULL)
			goto cleanup;
	}

	for (i = 0; i < 256; i++) {
		newlease->options[i].len = oldlease->options[i].len;
		newlease->options[i].data = calloc(1, newlease->options[i].len);
		if (newlease->options[i].data == NULL)
			goto cleanup;
		memcpy(newlease->options[i].data, oldlease->options[i].data,
		    newlease->options[i].len);
	}

	return (newlease);

cleanup:
	if (newlease)
		free_client_lease(newlease);

	return (NULL);
}

void
socket_nonblockmode(int fd)
{
	int	flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		error("fcntl F_GETF: %s", strerror(errno));

	flags |= O_NONBLOCK;

	if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		error("fcntl F_SETFL: %s", strerror(errno));
}

/*
 * Apply the list of options to be ignored that was provided on the
 * command line. This will override any ignore list obtained from
 * dhclient.conf.
 */
void
apply_ignore_list(char *ignore_list)
{
	u_int8_t list[256];
	char *p;
	int ix, i, j;

	memset(list, 0, sizeof(list));
	ix = 0;

	for (p = strsep(&ignore_list, ", "); p != NULL;
	    p = strsep(&ignore_list, ", ")) {
		if (*p == '\0')
			continue;
		
		for (i = 1; i < DHO_END; i++)
			if (!strcasecmp(dhcp_options[i].name, p))
				break;

		if (i == DHO_END) {
			note("Invalid option name: '%s'", p);
			return;
		}

		/* Avoid storing duplicate options in the list. */
		for (j = 0; j < ix; j++) {
			if (list[j] == i) {
				note("Duplicate option name: '%s'", p);
				return;
			}
		}
		list[ix++] = i;
	}

	config->ignored_option_count = ix;
	memcpy(config->ignored_options, list, sizeof(config->ignored_options));
}
