/*	$OpenBSD: dhclient.c,v 1.727 2022/07/02 17:21:32 deraadt Exp $	*/

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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <imsg.h>
#include <limits.h>
#include <paths.h>
#include <poll.h>
#include <pwd.h>
#include <resolv.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "log.h"
#include "privsep.h"

char *path_dhclient_conf;
char *path_lease_db;
char *log_procname;

int nullfd = -1;
int cmd_opts;
int quit;

const struct in_addr inaddr_any = { INADDR_ANY };
const struct in_addr inaddr_broadcast = { INADDR_BROADCAST };

struct client_config *config;
struct imsgbuf *unpriv_ibuf;

void		 usage(void);
int		 res_hnok_list(const char *);
int		 addressinuse(char *, struct in_addr, char *);

void		 fork_privchld(struct interface_info *, int, int);
void		 get_name(struct interface_info *, int, char *);
void		 get_ssid(struct interface_info *, int);
void		 get_sockets(struct interface_info *);
int		 get_routefd(int);
void		 set_iff_up(struct interface_info *, int);
void		 set_user(char *);
int		 get_ifa_family(char *, int);
struct ifaddrs	*get_link_ifa(const char *, struct ifaddrs *);
void		 interface_state(struct interface_info *);
struct interface_info *initialize_interface(char *, int);
void		 tick_msg(const char *, int);
void		 rtm_dispatch(struct interface_info *, struct rt_msghdr *);

struct client_lease *apply_defaults(struct client_lease *);
struct client_lease *clone_lease(struct client_lease *);

void state_reboot(struct interface_info *);
void state_init(struct interface_info *);
void state_selecting(struct interface_info *);
void state_bound(struct interface_info *);
void state_panic(struct interface_info *);

void set_interval(struct interface_info *, struct timespec *);
void set_resend_timeout(struct interface_info *, struct timespec *,
    void (*where)(struct interface_info *));
void set_secs(struct interface_info *, struct timespec *);

void send_discover(struct interface_info *);
void send_request(struct interface_info *);
void send_decline(struct interface_info *);
void send_release(struct interface_info *);

void process_offer(struct interface_info *, struct option_data *,
    const char *);
void bind_lease(struct interface_info *);

void make_discover(struct interface_info *, struct client_lease *);
void make_request(struct interface_info *, struct client_lease *);
void make_decline(struct interface_info *, struct client_lease *);
void make_release(struct interface_info *, struct client_lease *);

void release_lease(struct interface_info *);
void propose_release(struct interface_info *);

void write_lease_db(struct interface_info *);
char *lease_as_string(char *, struct client_lease *);
struct proposal *lease_as_proposal(struct client_lease *);
struct unwind_info *lease_as_unwind_info(struct client_lease *);
void append_statement(char *, size_t, char *, char *);
time_t lease_expiry(struct client_lease *);
time_t lease_renewal(struct client_lease *);
time_t lease_rebind(struct client_lease *);
void   get_lease_timeouts(struct interface_info *, struct client_lease *);

struct client_lease *packet_to_lease(struct interface_info *,
    struct option_data *);
void go_daemon(void);
int rdaemon(int);
int take_charge(struct interface_info *, int, char *);
int autoconf(struct interface_info *);
struct client_lease *get_recorded_lease(struct interface_info *);

#define ROUNDUP(a)	\
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define	ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

#define	TICK_WAIT	0
#define	TICK_SUCCESS	1
#define	TICK_DAEMON	2

static FILE *leaseFile;

int
get_ifa_family(char *cp, int n)
{
	struct sockaddr		*sa;
	unsigned int		 i;

	for (i = 1; i; i <<= 1) {
		if ((i & n) != 0) {
			sa = (struct sockaddr *)cp;
			if (i == RTA_IFA)
				return sa->sa_family;
			ADVANCE(cp, sa);
		}
	}

	return AF_UNSPEC;
}

struct ifaddrs *
get_link_ifa(const char *name, struct ifaddrs *ifap)
{
	struct ifaddrs		*ifa;
	struct sockaddr_dl	*sdl;

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(name, ifa->ifa_name) == 0 &&
		    (ifa->ifa_flags & IFF_LOOPBACK) == 0 &&
		    (ifa->ifa_flags & IFF_POINTOPOINT) == 0 &&
		    ifa->ifa_data != NULL && /* NULL shouldn't be possible. */
		    ifa->ifa_addr != NULL &&
		    ifa->ifa_addr->sa_family == AF_LINK) {
			sdl = (struct sockaddr_dl *)ifa->ifa_addr;
			if (sdl->sdl_alen == ETHER_ADDR_LEN &&
			    (sdl->sdl_type == IFT_ETHER ||
			    sdl->sdl_type == IFT_CARP))
				break;
		}
	}

	if (ifa == NULL)
		fatal("get_link_ifa()");

	return ifa;
}

void
interface_state(struct interface_info *ifi)
{
	struct ifaddrs			*ifap, *ifa;
	struct if_data			*ifd;
	struct sockaddr_dl		*sdl;
	char				*oldlladdr;
	int				 newlinkup, oldlinkup;

	oldlinkup = LINK_STATE_IS_UP(ifi->link_state);

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	ifa = get_link_ifa(ifi->name, ifap);
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	ifd = (struct if_data *)ifa->ifa_data;

	if ((ifa->ifa_flags & IFF_UP) == 0 ||
	    (ifa->ifa_flags & IFF_RUNNING) == 0) {
		ifi->link_state = LINK_STATE_DOWN;
	} else {
		ifi->link_state = ifd->ifi_link_state;
		ifi->mtu = ifd->ifi_mtu;
	}

	newlinkup = LINK_STATE_IS_UP(ifi->link_state);
	if (newlinkup != oldlinkup) {
		log_debug("%s: link %s -> %s", log_procname,
		    (oldlinkup != 0) ? "up" : "down",
		    (newlinkup != 0) ? "up" : "down");
		tick_msg("link", newlinkup ? TICK_SUCCESS : TICK_WAIT);
	}

	if (memcmp(&ifi->hw_address, LLADDR(sdl), ETHER_ADDR_LEN) != 0) {
		if (log_getverbose()) {
			oldlladdr = strdup(ether_ntoa(&ifi->hw_address));
			if (oldlladdr == NULL)
				fatal("oldlladdr");
			log_debug("%s: LLADDR %s -> %s", log_procname,
			    oldlladdr,
			    ether_ntoa((struct ether_addr *)LLADDR(sdl)));
			free(oldlladdr);
		}
		memcpy(&ifi->hw_address, LLADDR(sdl), ETHER_ADDR_LEN);
		quit = RESTART;	/* Even if MTU has changed. */
	}

	freeifaddrs(ifap);
}

struct interface_info *
initialize_interface(char *name, int noaction)
{
	struct interface_info		*ifi;
	struct ifaddrs			*ifap, *ifa;
	struct if_data			*ifd;
	struct sockaddr_dl		*sdl;
	int				 ioctlfd;

	ifi = calloc(1, sizeof(*ifi));
	if (ifi == NULL)
		fatal("ifi");

	ifi->rbuf_max = RT_BUF_SIZE;
	ifi->rbuf = malloc(ifi->rbuf_max);
	if (ifi->rbuf == NULL)
		fatal("rbuf");

	if ((ioctlfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fatal("socket(AF_INET, SOCK_DGRAM)");

	get_name(ifi, ioctlfd, name);
	ifi->index = if_nametoindex(ifi->name);
	if (ifi->index == 0)
		fatalx("if_nametoindex(%s) == 0", ifi->name);

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs()");

	ifa = get_link_ifa(ifi->name, ifap);
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	ifd = (struct if_data *)ifa->ifa_data;

	if ((ifa->ifa_flags & IFF_UP) == 0 ||
	    (ifa->ifa_flags & IFF_RUNNING) == 0)
		ifi->link_state = LINK_STATE_DOWN;
	else
		ifi->link_state = ifd->ifi_link_state;

	memcpy(ifi->hw_address.ether_addr_octet, LLADDR(sdl), ETHER_ADDR_LEN);
	ifi->rdomain = ifd->ifi_rdomain;

	get_sockets(ifi);
	get_ssid(ifi, ioctlfd);

	if (noaction == 0 && !LINK_STATE_IS_UP(ifi->link_state))
		set_iff_up(ifi, ioctlfd);

	close(ioctlfd);
	freeifaddrs(ifap);

	return ifi;
}

void
get_name(struct interface_info *ifi, int ioctlfd, char *arg)
{
	struct ifgroupreq	 ifgr;
	size_t			 len;

	if (strcmp(arg, "egress") == 0) {
		memset(&ifgr, 0, sizeof(ifgr));
		strlcpy(ifgr.ifgr_name, "egress", sizeof(ifgr.ifgr_name));
		if (ioctl(ioctlfd, SIOCGIFGMEMB, (caddr_t)&ifgr) == -1)
			fatal("SIOCGIFGMEMB");
		if (ifgr.ifgr_len > sizeof(struct ifg_req))
			fatalx("too many interfaces in group egress");
		if ((ifgr.ifgr_groups = calloc(1, ifgr.ifgr_len)) == NULL)
			fatalx("ifgr_groups");
		if (ioctl(ioctlfd, SIOCGIFGMEMB, (caddr_t)&ifgr) == -1)
			fatal("SIOCGIFGMEMB");
		len = strlcpy(ifi->name, ifgr.ifgr_groups->ifgrq_member,
		    IFNAMSIZ);
		free(ifgr.ifgr_groups);
	} else
		len = strlcpy(ifi->name, arg, IFNAMSIZ);

	if (len >= IFNAMSIZ)
		fatalx("interface name too long");
}

void
get_ssid(struct interface_info *ifi, int ioctlfd)
{
	struct ieee80211_nwid		nwid;
	struct ifreq			ifr;

	memset(&ifr, 0, sizeof(ifr));
	memset(&nwid, 0, sizeof(nwid));

	ifr.ifr_data = (caddr_t)&nwid;
	strlcpy(ifr.ifr_name, ifi->name, sizeof(ifr.ifr_name));

	if (ioctl(ioctlfd, SIOCG80211NWID, (caddr_t)&ifr) == 0) {
		memset(ifi->ssid, 0, sizeof(ifi->ssid));
		memcpy(ifi->ssid, nwid.i_nwid, nwid.i_len);
		ifi->ssid_len = nwid.i_len;
	}
}

void
set_iff_up(struct interface_info *ifi, int ioctlfd)
{
	struct ifreq	 ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifi->name, sizeof(ifr.ifr_name));

	if (ioctl(ioctlfd, SIOCGIFFLAGS, (caddr_t)&ifr) == -1)
		fatal("%s: SIOCGIFFLAGS", ifi->name);

	if ((ifr.ifr_flags & IFF_UP) == 0) {
		ifi->link_state = LINK_STATE_DOWN;
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(ioctlfd, SIOCSIFFLAGS, (caddr_t)&ifr) == -1)
			fatal("%s: SIOCSIFFLAGS", ifi->name);
	}
}

void
set_user(char *user)
{
	struct passwd		*pw;

	pw = getpwnam(user);
	if (pw == NULL)
		fatalx("no such user: %s", user);

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot(%s)", pw->pw_dir);
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");
	if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1)
		fatal("setresgid");
	if (setgroups(1, &pw->pw_gid) == -1)
		fatal("setgroups");
	if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
		fatal("setresuid");

	endpwent();
}

void
get_sockets(struct interface_info *ifi)
{
	unsigned char		*newp;
	size_t			 newsize;

	ifi->udpfd = get_udp_sock(ifi->rdomain);
	ifi->bpffd = get_bpf_sock(ifi->name);

	newsize = configure_bpf_sock(ifi->bpffd);
	if (newsize > ifi->rbuf_max) {
		if ((newp = realloc(ifi->rbuf, newsize)) == NULL)
			fatal("rbuf");
		ifi->rbuf = newp;
		ifi->rbuf_max = newsize;
	}
}

int
get_routefd(int rdomain)
{
	int		routefd, rtfilter;

	if ((routefd = socket(AF_ROUTE, SOCK_RAW, AF_INET)) == -1)
		fatal("socket(AF_ROUTE, SOCK_RAW)");

	rtfilter = ROUTE_FILTER(RTM_PROPOSAL) | ROUTE_FILTER(RTM_IFINFO) |
	    ROUTE_FILTER(RTM_NEWADDR) | ROUTE_FILTER(RTM_DELADDR) |
	    ROUTE_FILTER(RTM_IFANNOUNCE) | ROUTE_FILTER(RTM_80211INFO);

	if (setsockopt(routefd, AF_ROUTE, ROUTE_MSGFILTER,
	    &rtfilter, sizeof(rtfilter)) == -1)
		fatal("setsockopt(ROUTE_MSGFILTER)");
	if (setsockopt(routefd, AF_ROUTE, ROUTE_TABLEFILTER, &rdomain,
	    sizeof(rdomain)) == -1)
		fatal("setsockopt(ROUTE_TABLEFILTER)");

	return routefd;
}

void
routefd_handler(struct interface_info *ifi, int routefd)
{
	struct rt_msghdr		*rtm;
	unsigned char			*buf = ifi->rbuf;
	unsigned char			*lim, *next;
	ssize_t				 n;

	do {
		n = read(routefd, buf, RT_BUF_SIZE);
	} while (n == -1 && errno == EINTR);
	if (n == -1) {
		log_warn("%s: routing socket", log_procname);
		return;
	}
	if (n == 0)
		fatalx("%s: routing socket closed", log_procname);

	lim = buf + n;
	for (next = buf; next < lim && quit == 0; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (lim < next + sizeof(rtm->rtm_msglen) ||
		    lim < next + rtm->rtm_msglen)
			fatalx("%s: partial rtm in buffer", log_procname);

		if (rtm->rtm_version != RTM_VERSION)
			continue;

		rtm_dispatch(ifi, rtm);
	}
}

void
rtm_dispatch(struct interface_info *ifi, struct rt_msghdr *rtm)
{
	struct if_msghdr		*ifm;
	struct if_announcemsghdr	*ifan;
	struct ifa_msghdr		*ifam;
	struct if_ieee80211_data	*ifie;
	char				*oldssid;
	uint32_t			 oldmtu;

	switch (rtm->rtm_type) {
	case RTM_PROPOSAL:
		if (rtm->rtm_priority == RTP_PROPOSAL_SOLICIT) {
			if (quit == 0 && ifi->active != NULL)
				tell_unwind(ifi->unwind_info, ifi->flags);
			return;
		}
		if (rtm->rtm_index != ifi->index ||
		    rtm->rtm_priority != RTP_PROPOSAL_DHCLIENT)
			return;
		if ((rtm->rtm_flags & RTF_PROTO3) != 0) {
			if (rtm->rtm_seq == (int32_t)ifi->xid) {
				ifi->flags |= IFI_IN_CHARGE;
				return;
			} else if ((ifi->flags & IFI_IN_CHARGE) != 0) {
				log_debug("%s: yielding responsibility",
				    log_procname);
				quit = TERMINATE;
			}
		} else if ((rtm->rtm_flags & RTF_PROTO2) != 0) {
			release_lease(ifi); /* OK even if we sent it. */
			quit = TERMINATE;
		} else
			return; /* Ignore tell_unwind() proposals. */
		break;

	case RTM_DESYNC:
		log_warnx("%s: RTM_DESYNC", log_procname);
		break;

	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		if (ifm->ifm_index != ifi->index)
			break;
		if ((rtm->rtm_flags & RTF_UP) == 0)
			fatalx("down");

		oldmtu = ifi->mtu;
		interface_state(ifi);
		if (oldmtu == ifi->mtu)
			quit = RESTART;
		else
			log_debug("%s: MTU %u -> %u",
			    log_procname, oldmtu, ifi->mtu);
		break;

	case RTM_80211INFO:
		if (rtm->rtm_index != ifi->index)
			break;
		ifie = &((struct if_ieee80211_msghdr *)rtm)->ifim_ifie;
		if (ifi->ssid_len != ifie->ifie_nwid_len || memcmp(ifi->ssid,
		    ifie->ifie_nwid, ifie->ifie_nwid_len) != 0) {
			if (log_getverbose()) {
				oldssid = strdup(pretty_print_string(ifi->ssid,
				    ifi->ssid_len, 1));
				if (oldssid == NULL)
					fatal("oldssid");
				log_debug("%s: SSID %s -> %s", log_procname,
				    oldssid, pretty_print_string(ifie->ifie_nwid,
				    ifie->ifie_nwid_len, 1));
				free(oldssid);
			}
			quit = RESTART;
		}
		break;

	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *)rtm;
		if (ifan->ifan_what == IFAN_DEPARTURE && ifan->ifan_index ==
		    ifi->index)
			fatalx("departed");
		break;

	case RTM_NEWADDR:
	case RTM_DELADDR:
		/* Need to check if it is time to write resolv.conf. */
		ifam = (struct ifa_msghdr *)rtm;
		if (get_ifa_family((char *)ifam + ifam->ifam_hdrlen,
		    ifam->ifam_addrs) != AF_INET)
			return;
		break;

	default:
		break;
	}

	/*
	 * Responsibility for resolv.conf may have changed hands.
	 */
	if (quit == 0 && ifi->active != NULL &&
	    (ifi->flags & IFI_IN_CHARGE) != 0 &&
	    ifi->state == S_BOUND)
		write_resolv_conf();
}

int
main(int argc, char *argv[])
{
	uint8_t			 actions[DHO_END];
	struct stat		 sb;
	struct interface_info	*ifi;
	char			*ignore_list, *p;
	int			 fd, socket_fd[2];
	int			 routefd;
	int			 ch, i;

	if (isatty(STDERR_FILENO) != 0)
		log_init(1, LOG_DEBUG); /* log to stderr until daemonized */
	else
		log_init(0, LOG_DEBUG); /* can't log to stderr */

	log_setverbose(0);	/* Don't show log_debug() messages. */

	if (lstat(_PATH_DHCLIENT_CONF, &sb) == 0)
		path_dhclient_conf = _PATH_DHCLIENT_CONF;
	memset(actions, ACTION_USELEASE, sizeof(actions));

	while ((ch = getopt(argc, argv, "c:di:nrv")) != -1) {
		syslog(LOG_ALERT | LOG_CONS,
		    "dhclient will go away, so -%c option will not exist", ch);
		switch (ch) {
		case 'c':
			if (strlen(optarg) == 0)
				path_dhclient_conf = NULL;
			else if (lstat(optarg, &sb) == 0)
				path_dhclient_conf = optarg;
			else
				fatal("lstat(%s)", optarg);
			break;
		case 'd':
			cmd_opts |= OPT_FOREGROUND;
			break;
		case 'i':
			syslog(LOG_ALERT | LOG_CONS,
			    "dhclient will go away, for -i learn dhcpleased.conf");
			if (strlen(optarg) == 0)
				break;
			ignore_list = strdup(optarg);
			if (ignore_list == NULL)
				fatal("ignore_list");
			for (p = strsep(&ignore_list, ", "); p != NULL;
			     p = strsep(&ignore_list, ", ")) {
				if (*p == '\0')
					continue;
				i = name_to_code(p);
				if (i == DHO_END)
					fatalx("invalid option name: '%s'", p);
				actions[i] = ACTION_IGNORE;
			}
			free(ignore_list);
			break;
		case 'n':
			cmd_opts |= OPT_NOACTION;
			break;
		case 'r':
			cmd_opts |= OPT_RELEASE;
			break;
		case 'v':
			cmd_opts |= OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	syslog(LOG_ALERT | LOG_CONS,
	    "dhclient will go away, stop using it");

	execl("/sbin/ifconfig", "ifconfig", argv[0], "inet", "autoconf", NULL);

	if ((cmd_opts & (OPT_FOREGROUND | OPT_NOACTION)) != 0)
		cmd_opts |= OPT_VERBOSE;

	if ((cmd_opts & OPT_VERBOSE) != 0)
		log_setverbose(1);	/* Show log_debug() messages. */

	ifi = initialize_interface(argv[0], cmd_opts & OPT_NOACTION);

	log_procname = strdup(ifi->name);
	if (log_procname == NULL)
		fatal("log_procname");
	setproctitle("%s", log_procname);
	log_procinit(log_procname);

	tzset();

	if (setrtable(ifi->rdomain) == -1)
		fatal("setrtable(%u)", ifi->rdomain);

	if ((cmd_opts & OPT_RELEASE) != 0) {
		if ((cmd_opts & OPT_NOACTION) == 0)
			propose_release(ifi);
		exit(0);
	}

	signal(SIGPIPE, SIG_IGN);	/* Don't wait for go_daemon()! */

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0,
	    socket_fd) == -1)
		fatal("socketpair");

	if ((nullfd = open(_PATH_DEVNULL, O_RDWR)) == -1)
		fatal("open(%s)", _PATH_DEVNULL);

	fork_privchld(ifi, socket_fd[0], socket_fd[1]);

	close(socket_fd[0]);
	if ((unpriv_ibuf = malloc(sizeof(*unpriv_ibuf))) == NULL)
		fatal("unpriv_ibuf");
	imsg_init(unpriv_ibuf, socket_fd[1]);

	read_conf(ifi->name, actions, &ifi->hw_address);
	if ((cmd_opts & OPT_NOACTION) != 0)
		return 0;

	if (asprintf(&path_lease_db, "%s.%s", _PATH_LEASE_DB, ifi->name) == -1)
		fatal("path_lease_db");

	routefd = get_routefd(ifi->rdomain);
	fd = take_charge(ifi, routefd, path_lease_db);	/* Kill other dhclients. */
	if (autoconf(ifi)) {
		/* dhcpleased has been notified to request a new lease. */
		return 0;
	}
	if (fd != -1)
		read_lease_db(&ifi->lease_db);

	if ((leaseFile = fopen(path_lease_db, "w")) == NULL)
		log_warn("%s: fopen(%s)", log_procname, path_lease_db);
	write_lease_db(ifi);

	set_user("_dhcp");

	if ((cmd_opts & OPT_FOREGROUND) == 0) {
		if (pledge("stdio inet dns route proc", NULL) == -1)
			fatal("pledge");
	} else {
		if (pledge("stdio inet dns route", NULL) == -1)
			fatal("pledge");
	}

	tick_msg("link", LINK_STATE_IS_UP(ifi->link_state) ? TICK_SUCCESS :
	    TICK_WAIT);
	quit = RESTART;
	dispatch(ifi, routefd);

	return 0;
}

void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr,
	    "usage: %s [-dnrv] [-c file] [-i options] "
	    "interface\n", __progname);
	exit(1);
}

void
state_preboot(struct interface_info *ifi)
{
	interface_state(ifi);
	if (quit != 0)
		return;

	if (LINK_STATE_IS_UP(ifi->link_state)) {
		tick_msg("link", TICK_SUCCESS);
		ifi->state = S_REBOOTING;
		state_reboot(ifi);
	} else {
		tick_msg("link", TICK_WAIT);
		set_timeout(ifi, 1, state_preboot);
	}
}

/*
 * Called when the interface link becomes active.
 */
void
state_reboot(struct interface_info *ifi)
{
	const struct timespec	 reboot_intvl = {config->reboot_interval, 0};
	struct client_lease	*lease;

	cancel_timeout(ifi);

	/*
	 * If there is no recorded lease or the lease is BOOTP then
	 * go straight to INIT and try to DISCOVER a new lease.
	 */
	ifi->active = get_recorded_lease(ifi);
	if (ifi->active == NULL || BOOTP_LEASE(ifi->active)) {
		ifi->state = S_INIT;
		state_init(ifi);
		return;
	}
	lease = apply_defaults(ifi->active);
	get_lease_timeouts(ifi, lease);
	free_client_lease(lease);

	ifi->xid = arc4random();
	make_request(ifi, ifi->active);

	ifi->destination.s_addr = INADDR_BROADCAST;
	clock_gettime(CLOCK_MONOTONIC, &ifi->first_sending);
	timespecadd(&ifi->first_sending, &reboot_intvl, &ifi->reboot_timeout);
	ifi->interval = 0;

	send_request(ifi);
}

/*
 * Called when a lease has completely expired and we've been unable to
 * renew it.
 */
void
state_init(struct interface_info *ifi)
{
	const struct timespec	offer_intvl = {config->offer_interval, 0};

	ifi->xid = arc4random();
	make_discover(ifi, ifi->active);

	ifi->destination.s_addr = INADDR_BROADCAST;
	ifi->state = S_SELECTING;
	clock_gettime(CLOCK_MONOTONIC, &ifi->first_sending);
	timespecadd(&ifi->first_sending, &offer_intvl, &ifi->offer_timeout);
	ifi->select_timeout = ifi->offer_timeout;
	ifi->interval = 0;

	send_discover(ifi);
}

/*
 * Called when one or more DHCPOFFER packets have been received and a
 * configurable period of time has passed.
 */
void
state_selecting(struct interface_info *ifi)
{
	cancel_timeout(ifi);

	if (ifi->offer == NULL) {
		state_panic(ifi);
		return;
	}

	ifi->state = S_REQUESTING;

	/* If it was a BOOTREPLY, we can just take the lease right now. */
	if (BOOTP_LEASE(ifi->offer)) {
		bind_lease(ifi);
		return;
	}

	ifi->destination.s_addr = INADDR_BROADCAST;
	clock_gettime(CLOCK_MONOTONIC, &ifi->first_sending);
	ifi->interval = 0;

	/*
	 * Make a DHCPREQUEST packet from the lease we picked. Keep
	 * the current xid, as all offers should have had the same
	 * one.
	 */
	make_request(ifi, ifi->offer);

	/* Toss the lease we picked - we'll get it back in a DHCPACK. */
	free_client_lease(ifi->offer);
	ifi->offer = NULL;
	free(ifi->offer_src);
	ifi->offer_src = NULL;

	send_request(ifi);
}

void
dhcpoffer(struct interface_info *ifi, struct option_data *options,
    const char *src)
{
	if (ifi->state != S_SELECTING) {
		log_debug("%s: unexpected DHCPOFFER from %s - state #%d",
		    log_procname, src, ifi->state);
		return;
	}

	log_debug("%s: DHCPOFFER from %s", log_procname, src);
	process_offer(ifi, options, src);
}

void
bootreply(struct interface_info *ifi, struct option_data *options,
    const char *src)
{
	if (ifi->state != S_SELECTING) {
		log_debug("%s: unexpected BOOTREPLY from %s - state #%d",
		    log_procname, src, ifi->state);
		return;
	}

	log_debug("%s: BOOTREPLY from %s", log_procname, src);
	process_offer(ifi, options, src);
}

void
process_offer(struct interface_info *ifi, struct option_data *options,
    const char *src)
{
	const struct timespec	 select_intvl = {config->select_interval, 0};
	struct timespec		 now;
	struct client_lease	*lease;

	clock_gettime(CLOCK_MONOTONIC, &now);

	lease = packet_to_lease(ifi, options);
	if (lease != NULL) {
		if (ifi->offer == NULL) {
			ifi->offer = lease;
			free(ifi->offer_src);
			ifi->offer_src = strdup(src);	/* NULL is OK */
			timespecadd(&now, &select_intvl, &ifi->select_timeout);
			if (timespeccmp(&ifi->select_timeout,
			    &ifi->offer_timeout, >))
				ifi->select_timeout = ifi->offer_timeout;
		} else if (lease->address.s_addr ==
		    ifi->offer->address.s_addr) {
			/* Decline duplicate offers. */
		} else if (lease->address.s_addr ==
		    ifi->requested_address.s_addr) {
			free_client_lease(ifi->offer);
			ifi->offer = lease;
			free(ifi->offer_src);
			ifi->offer_src = strdup(src);	/* NULL is OK */
		}

		if (ifi->offer != lease) {
			make_decline(ifi, lease);
			send_decline(ifi);
			free_client_lease(lease);
		} else if (ifi->offer->address.s_addr ==
		    ifi->requested_address.s_addr) {
			ifi->select_timeout = now;
		}
	}

	if (timespeccmp(&now, &ifi->select_timeout, >=))
		state_selecting(ifi);
	else {
		ifi->timeout = ifi->select_timeout;
		ifi->timeout_func = state_selecting;
	}
}

void
dhcpack(struct interface_info *ifi, struct option_data *options,
    const char *src)
{
	struct client_lease	*lease;

	if (ifi->state != S_REBOOTING &&
	    ifi->state != S_REQUESTING &&
	    ifi->state != S_RENEWING) {
		log_debug("%s: unexpected DHCPACK from %s - state #%d",
		    log_procname, src, ifi->state);
		return;
	}

	log_debug("%s: DHCPACK", log_procname);

	lease = packet_to_lease(ifi, options);
	if (lease == NULL) {
		ifi->state = S_INIT;
		state_init(ifi);
		return;
	}

	ifi->offer = lease;
	ifi->offer_src = strdup(src);	/* NULL is OK */
	memcpy(ifi->offer->ssid, ifi->ssid, sizeof(ifi->offer->ssid));
	ifi->offer->ssid_len = ifi->ssid_len;

	/* Stop resending DHCPREQUEST. */
	cancel_timeout(ifi);

	bind_lease(ifi);
}

void
dhcpnak(struct interface_info *ifi, const char *src)
{
	struct client_lease		*ll, *pl;

	if (ifi->state != S_REBOOTING &&
	    ifi->state != S_REQUESTING &&
	    ifi->state != S_RENEWING) {
		log_debug("%s: unexpected DHCPNAK from %s - state #%d",
		    log_procname, src, ifi->state);
		return;
	}

	log_debug("%s: DHCPNAK", log_procname);

	/* Remove the NAK'd address from the database. */
	TAILQ_FOREACH_SAFE(ll, &ifi->lease_db, next, pl) {
		if (ifi->ssid_len == ll->ssid_len &&
		    memcmp(ifi->ssid, ll->ssid, ll->ssid_len) == 0 &&
		    ll->address.s_addr == ifi->requested_address.s_addr) {
			if (ll == ifi->active) {
				tell_unwind(NULL, ifi->flags);
				free(ifi->unwind_info);
				ifi->unwind_info = NULL;
				revoke_proposal(ifi->configured);
				free(ifi->configured);
				ifi->configured = NULL;
				ifi->active = NULL;
			}
			TAILQ_REMOVE(&ifi->lease_db, ll, next);
			free_client_lease(ll);
			write_lease_db(ifi);
		}
	}

	/* Stop sending DHCPREQUEST packets. */
	cancel_timeout(ifi);

	ifi->state = S_INIT;
	state_init(ifi);
}

void
bind_lease(struct interface_info *ifi)
{
	struct timespec		 now;
	struct client_lease	*lease, *pl, *ll;
	struct proposal		*effective_proposal = NULL;
	struct unwind_info	*unwind_info;
	char			*msg = NULL;
	int			 rslt, seen;

	tick_msg("lease", TICK_SUCCESS);
	clock_gettime(CLOCK_MONOTONIC, &now);

	lease = apply_defaults(ifi->offer);
	get_lease_timeouts(ifi, lease);

	/* Replace the old active lease with the accepted offer. */
	ifi->active = ifi->offer;
	ifi->offer = NULL;

	/*
	 * Supply unwind with updated info.
	 */
	unwind_info = lease_as_unwind_info(ifi->active);
	if (ifi->unwind_info == NULL && unwind_info != NULL) {
		ifi->unwind_info = unwind_info;
		tell_unwind(ifi->unwind_info, ifi->flags);
	} else if (ifi->unwind_info != NULL && unwind_info == NULL) {
		tell_unwind(NULL, ifi->flags);
		free(ifi->unwind_info);
		ifi->unwind_info = NULL;
	} else if (ifi->unwind_info != NULL && unwind_info != NULL) {
		if (memcmp(ifi->unwind_info, unwind_info,
		    sizeof(*ifi->unwind_info)) != 0) {
			tell_unwind(NULL, ifi->flags);
			free(ifi->unwind_info);
			ifi->unwind_info = unwind_info;
			tell_unwind(ifi->unwind_info, ifi->flags);
		}
	}

	effective_proposal = lease_as_proposal(lease);
	if (ifi->configured != NULL) {
		if (memcmp(ifi->configured, effective_proposal,
		    sizeof(*ifi->configured)) == 0)
			goto newlease;
	}
	free(ifi->configured);
	ifi->configured = effective_proposal;
	effective_proposal = NULL;

	propose(ifi->configured);
	rslt = asprintf(&msg, "%s lease accepted from %s",
	    inet_ntoa(ifi->active->address),
	    (ifi->offer_src == NULL) ? "<unknown>" : ifi->offer_src);
	if (rslt == -1)
		fatal("bind msg");

newlease:
	seen = 0;
	TAILQ_FOREACH_SAFE(ll, &ifi->lease_db, next, pl) {
		if (ifi->ssid_len != ll->ssid_len ||
		    memcmp(ifi->ssid, ll->ssid, ll->ssid_len) != 0)
			continue;
		if (ifi->active == ll)
			seen = 1;
		else if (ll->address.s_addr == ifi->active->address.s_addr) {
			TAILQ_REMOVE(&ifi->lease_db, ll, next);
			free_client_lease(ll);
		}
	}
	if (seen == 0) {
		if (ifi->active->epoch == 0)
			time(&ifi->active->epoch);
		TAILQ_INSERT_HEAD(&ifi->lease_db, ifi->active,  next);
	}

	/*
	 * Write out updated information before going daemon.
	 *
	 * Some scripts (e.g. the installer in autoinstall mode) assume that
	 * the bind process is complete and all related information is in
	 * place when dhclient(8) goes daemon.
	 */
	write_lease_db(ifi);

	free_client_lease(lease);
	free(effective_proposal);
	free(ifi->offer_src);
	ifi->offer_src = NULL;

	if (msg != NULL) {
		if ((cmd_opts & OPT_FOREGROUND) != 0) {
			/* log msg on console only. */
			;
		} else if (isatty(STDERR_FILENO) != 0) {
			/*
			 * log msg to console and then go_daemon() so it is
			 * logged again, this time to /var/log/daemon.
			 */
			log_info("%s: %s", log_procname, msg);
			go_daemon();
		}
		log_info("%s: %s", log_procname, msg);
		free(msg);
	}

	ifi->state = S_BOUND;
	go_daemon();

	/*
	 * Set timeout to start the renewal process.
	 *
	 * If the renewal time is in the past, the lease is from the
	 * leaseDB. Rather than immediately trying to contact a server,
	 * pause the configured time between attempts.
	 */
	if (timespeccmp(&now, &ifi->renew, >=))
		set_timeout(ifi, config->retry_interval, state_bound);
	else {
		ifi->timeout = ifi->renew;
		ifi->timeout_func = state_bound;
	}
}

/*
 * Called when we've successfully bound to a particular lease, but the renewal
 * time on that lease has expired.  We are expected to unicast a DHCPREQUEST to
 * the server that gave us our original lease.
 */
void
state_bound(struct interface_info *ifi)
{
	struct option_data	*opt;
	struct in_addr		*dest;

	ifi->xid = arc4random();
	make_request(ifi, ifi->active);

	dest = &ifi->destination;
	opt = &ifi->active->options[DHO_DHCP_SERVER_IDENTIFIER];

	if (opt->len == sizeof(*dest))
		dest->s_addr = ((struct in_addr *)opt->data)->s_addr;
	else
		dest->s_addr = INADDR_BROADCAST;

	clock_gettime(CLOCK_MONOTONIC, &ifi->first_sending);
	ifi->interval = 0;
	ifi->state = S_RENEWING;

	send_request(ifi);
}

int
addressinuse(char *name, struct in_addr address, char *ifname)
{
	struct ifaddrs		*ifap, *ifa;
	struct sockaddr_in	*sin;
	int			 used = 0;

	if (getifaddrs(&ifap) != 0) {
		log_warn("%s: getifaddrs", log_procname);
		return 0;
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != AF_INET)
			continue;

		sin = (struct sockaddr_in *)ifa->ifa_addr;
		if (memcmp(&address, &sin->sin_addr, sizeof(address)) == 0) {
			strlcpy(ifname, ifa->ifa_name, IF_NAMESIZE);
			used = 1;
			if (strncmp(ifname, name, IF_NAMESIZE) != 0)
				break;
		}
	}

	freeifaddrs(ifap);
	return used;
}

/*
 * Allocate a client_lease structure and initialize it from the
 * parameters in the received packet.
 *
 * Return NULL and decline the lease if a valid lease cannot be
 * constructed.
 */
struct client_lease *
packet_to_lease(struct interface_info *ifi, struct option_data *options)
{
	char			 ifname[IF_NAMESIZE];
	struct dhcp_packet	*packet = &ifi->recv_packet;
	struct client_lease	*lease;
	char			*pretty, *name;
	int			 i;

	lease = calloc(1, sizeof(*lease));
	if (lease == NULL) {
		log_warn("%s: lease", log_procname);
		return NULL;	/* Can't even DECLINE. */
	}

	/*  Copy the lease addresses. */
	lease->address.s_addr = packet->yiaddr.s_addr;
	lease->next_server.s_addr = packet->siaddr.s_addr;

	/* Copy the lease options. */
	for (i = 0; i < DHO_COUNT; i++) {
		if (options[i].len == 0)
			continue;
		name = code_to_name(i);
		if (i == DHO_DOMAIN_SEARCH) {
			/* Replace RFC 1035 data with a string. */
			pretty = rfc1035_as_string(options[i].data,
			    options[i].len);
			free(options[i].data);
			options[i].data = strdup(pretty);
			if (options[i].data == NULL)
				fatal("RFC1035 string");
			options[i].len = strlen(options[i].data);
		} else
			pretty = pretty_print_option(i, &options[i], 0);
		if (strlen(pretty) == 0)
			continue;
		switch (i) {
		case DHO_DOMAIN_SEARCH:
		case DHO_DOMAIN_NAME:
			/*
			 * Allow deviant but historically blessed
			 * practice of supplying multiple domain names
			 * with DHO_DOMAIN_NAME. Thus allowing multiple
			 * entries in the resolv.conf 'search' statement.
			 */
			if (res_hnok_list(pretty) == 0) {
				log_debug("%s: invalid host name in %s",
				    log_procname, name);
				continue;
			}
			break;
		case DHO_HOST_NAME:
		case DHO_NIS_DOMAIN:
			if (res_hnok(pretty) == 0) {
				log_debug("%s: invalid host name in %s",
				    log_procname, name);
				continue;
			}
			break;
		default:
			break;
		}
		lease->options[i] = options[i];
		options[i].data = NULL;
		options[i].len = 0;
	}

	/*
	 * If this lease doesn't supply a required parameter, decline it.
	 */
	for (i = 0; i < config->required_option_count; i++) {
		if (lease->options[config->required_options[i]].len == 0) {
			name = code_to_name(config->required_options[i]);
			log_warnx("%s: %s required but missing", log_procname,
			    name);
			goto decline;
		}
	}

	/*
	 * If this lease is trying to sell us an address we are already
	 * using, decline it.
	 */
	memset(ifname, 0, sizeof(ifname));
	if (addressinuse(ifi->name, lease->address, ifname) != 0 &&
	    strncmp(ifname, ifi->name, IF_NAMESIZE) != 0) {
		log_warnx("%s: %s already configured on %s", log_procname,
		    inet_ntoa(lease->address), ifname);
		goto decline;
	}

	/* If the server name was filled out, copy it. */
	if ((lease->options[DHO_DHCP_OPTION_OVERLOAD].len == 0 ||
	    (lease->options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 2) == 0) &&
	    packet->sname[0]) {
		lease->server_name = calloc(1, DHCP_SNAME_LEN + 1);
		if (lease->server_name == NULL) {
			log_warn("%s: SNAME", log_procname);
			goto decline;
		}
		memcpy(lease->server_name, packet->sname, DHCP_SNAME_LEN);
		if (res_hnok(lease->server_name) == 0) {
			log_debug("%s: invalid host name in SNAME ignored",
			    log_procname);
			free(lease->server_name);
			lease->server_name = NULL;
		}
	}

	/* If the file name was filled out, copy it. */
	if ((lease->options[DHO_DHCP_OPTION_OVERLOAD].len == 0 ||
	    (lease->options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 1) == 0) &&
	    packet->file[0]) {
		/* Don't count on the NUL terminator. */
		lease->filename = malloc(DHCP_FILE_LEN + 1);
		if (lease->filename == NULL) {
			log_warn("%s: filename", log_procname);
			goto decline;
		}
		memcpy(lease->filename, packet->file, DHCP_FILE_LEN);
		lease->filename[DHCP_FILE_LEN] = '\0';
	}

	/*
	 * Record the client identifier used to obtain the lease.  We already
	 * checked that the packet client identifier is absent (RFC 2131) or
	 * matches what we sent (RFC 6842),
	 */
	i = DHO_DHCP_CLIENT_IDENTIFIER;
	if (lease->options[i].len == 0 && config->send_options[i].len != 0) {
		lease->options[i].len = config->send_options[i].len;
		lease->options[i].data = malloc(lease->options[i].len);
		if (lease->options[i].data == NULL)
			fatal("lease client-identifier");
		memcpy(lease->options[i].data, config->send_options[i].data,
		    lease->options[i].len);
	}

	time(&lease->epoch);
	return lease;

decline:
	make_decline(ifi, lease);
	send_decline(ifi);
	free_client_lease(lease);
	return NULL;
}

void
set_interval(struct interface_info *ifi, struct timespec *now)
{
	struct timespec		interval;

	if (timespeccmp(now, &ifi->timeout, >))
		ifi->interval = 1;
	else {
		timespecsub(&ifi->timeout, now, &interval);
		if (interval.tv_sec == 0 || interval.tv_nsec > 500000000LL)
			interval.tv_sec++;
		ifi->interval = interval.tv_sec;
	}
}

void
set_resend_timeout(struct interface_info *ifi, struct timespec *now,
    void (*where)(struct interface_info *))
{
	const struct timespec	reboot_intvl = {config->reboot_interval, 0};
	const struct timespec	initial_intvl = {config->initial_interval, 0};
	const struct timespec	cutoff_intvl = {config->backoff_cutoff, 0};
	const struct timespec	onesecond = {1, 0};
	struct timespec		interval, when;

	if (timespeccmp(now, &ifi->link_timeout, <))
		interval = onesecond;
	else if (ifi->interval == 0) {
		if (ifi->state == S_REBOOTING)
			interval = reboot_intvl;
		else
			interval = initial_intvl;
	} else {
		timespecclear(&interval);
		interval.tv_sec = ifi->interval + arc4random_uniform(2 *
		    ifi->interval);
	}
	if (timespeccmp(&interval, &onesecond, <))
		interval = onesecond;
	else if (timespeccmp(&interval, &cutoff_intvl, >))
		interval = cutoff_intvl;

	timespecadd(now, &interval, &when);
	switch (ifi->state) {
	case S_REBOOTING:
	case S_RENEWING:
		if (timespeccmp(&when, &ifi->expiry, >))
			when = ifi->expiry;
		break;
	case S_SELECTING:
		if (timespeccmp(&when, &ifi->select_timeout, >))
			when = ifi->select_timeout;
		break;
	case S_REQUESTING:
		if (timespeccmp(&when, &ifi->offer_timeout, >))
			when = ifi->offer_timeout;
		break;
	default:
		break;
	}

	ifi->timeout = when;
	ifi->timeout_func = where;
}

void
set_secs(struct interface_info *ifi, struct timespec *now)
{
	struct timespec interval;

	if (ifi->state != S_REQUESTING) {
		/* Update the number of seconds since we started sending. */
		timespecsub(now, &ifi->first_sending, &interval);
		if (interval.tv_nsec > 500000000LL)
			interval.tv_sec++;
		if (interval.tv_sec > UINT16_MAX)
			ifi->secs = UINT16_MAX;
		else
			ifi->secs = interval.tv_sec;
	}

	ifi->sent_packet.secs = htons(ifi->secs);
}

/*
 * Send out a DHCPDISCOVER packet, and set a timeout to send out another
 * one after the right interval has expired.  If we don't get an offer by
 * the time we reach the panic interval, call the panic function.
 */
void
send_discover(struct interface_info *ifi)
{
	struct timespec		 now;
	ssize_t			 rslt;

	clock_gettime(CLOCK_MONOTONIC, &now);
	if (timespeccmp(&now, &ifi->offer_timeout, >=)) {
		state_panic(ifi);
		return;
	}

	set_resend_timeout(ifi, &now, send_discover);
	set_interval(ifi, &now);
	set_secs(ifi, &now);

	rslt = send_packet(ifi, inaddr_any, inaddr_broadcast, "DHCPDISCOVER");
	if (rslt != -1)
		log_debug("%s: DHCPDISCOVER %s", log_procname,
		    (ifi->requested_address.s_addr == INADDR_ANY) ? "" :
		    inet_ntoa(ifi->requested_address));

	tick_msg("lease", TICK_WAIT);
}

/*
 * Called if we haven't received any offers in a preset amount of time. When
 * this happens, we try to use existing leases that haven't yet expired.
 *
 * If LINK_STATE_UNKNOWN, do NOT use recorded leases.
 */
void
state_panic(struct interface_info *ifi)
{
	log_debug("%s: no acceptable DHCPOFFERS received", log_procname);

	if (ifi->link_state >= LINK_STATE_UP) {
		ifi->offer = get_recorded_lease(ifi);
		if (ifi->offer != NULL) {
			ifi->state = S_REQUESTING;
			ifi->offer_src = strdup(path_lease_db); /* NULL is OK. */
			bind_lease(ifi);
			return;
		}
	}

	/*
	 * No leases were available, or what was available didn't work
	 */
	log_debug("%s: no working leases in persistent database - sleeping",
	    log_procname);
	ifi->state = S_INIT;
	set_timeout(ifi, config->retry_interval, state_init);
	tick_msg("lease", TICK_DAEMON);
}

void
send_request(struct interface_info *ifi)
{
	struct sockaddr_in	 destination;
	struct in_addr		 from;
	struct timespec		 now;
	ssize_t			 rslt;
	char			*addr;

	cancel_timeout(ifi);
	clock_gettime(CLOCK_MONOTONIC, &now);

	switch (ifi->state) {
	case S_REBOOTING:
		if (timespeccmp(&now, &ifi->reboot_timeout, >=))
			ifi->state = S_INIT;
		else {
			destination.sin_addr.s_addr = INADDR_BROADCAST;
			if (ifi->active == NULL)
				from.s_addr = INADDR_ANY;
			else
				from.s_addr = ifi->active->address.s_addr;
		}
		break;
	case S_RENEWING:
		if (timespeccmp(&now, &ifi->expiry, >=))
			ifi->state = S_INIT;
		else {
			if (timespeccmp(&now, &ifi->rebind, >=))
				destination.sin_addr.s_addr = INADDR_BROADCAST;
			else
				destination.sin_addr.s_addr = ifi->destination.s_addr;
			if (ifi->active == NULL)
				from.s_addr = INADDR_ANY;
			else
				from.s_addr = ifi->active->address.s_addr;
		}
		break;
	case S_REQUESTING:
		if (timespeccmp(&now, &ifi->offer_timeout, >=))
			ifi->state = S_INIT;
		else {
			destination.sin_addr.s_addr = INADDR_BROADCAST;
			from.s_addr = INADDR_ANY;
		}
		break;
	default:
		ifi->state = S_INIT;
		break;
	}

	if (ifi->state == S_INIT) {
		/* Something has gone wrong. Start over. */
		state_init(ifi);
		return;
	}

	set_resend_timeout(ifi, &now, send_request);
	set_interval(ifi, &now);
	set_secs(ifi, &now);

	rslt = send_packet(ifi, from, destination.sin_addr, "DHCPREQUEST");
	if (rslt != -1 && log_getverbose()) {
		addr = strdup(inet_ntoa(ifi->requested_address));
		if (addr == NULL)
			fatal("strdup(ifi->requested_address)");
		if (destination.sin_addr.s_addr == INADDR_BROADCAST)
			log_debug("%s: DHCPREQUEST %s", log_procname, addr);
		else
			log_debug("%s: DHCPREQUEST %s from %s", log_procname,
			    addr, inet_ntoa(destination.sin_addr));
		free(addr);
	}

	tick_msg("lease", TICK_WAIT);
}

void
send_decline(struct interface_info *ifi)
{
	ssize_t		rslt;

	rslt = send_packet(ifi, inaddr_any, inaddr_broadcast, "DHCPDECLINE");
	if (rslt != -1)
		log_debug("%s: DHCPDECLINE", log_procname);
}

void
send_release(struct interface_info *ifi)
{
	ssize_t		rslt;

	rslt = send_packet(ifi, ifi->configured->address, ifi->destination,
	    "DHCPRELEASE");
	if (rslt != -1)
		log_debug("%s: DHCPRELEASE", log_procname);
}

void
make_discover(struct interface_info *ifi, struct client_lease *lease)
{
	struct option_data	 options[DHO_COUNT];
	struct dhcp_packet	*packet = &ifi->sent_packet;
	unsigned char		 discover = DHCPDISCOVER;
	int			 i;

	memset(options, 0, sizeof(options));
	memset(packet, 0, sizeof(*packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPDISCOVER */
	i = DHO_DHCP_MESSAGE_TYPE;
	options[i].data = &discover;
	options[i].len = sizeof(discover);

	/* Request the options we want */
	i  = DHO_DHCP_PARAMETER_REQUEST_LIST;
	options[i].data = config->requested_options;
	options[i].len = config->requested_option_count;

	/* If we had an address, try to get it again. */
	if (lease != NULL) {
		ifi->requested_address = lease->address;
		i = DHO_DHCP_REQUESTED_ADDRESS;
		options[i].data = (char *)&lease->address;
		options[i].len = sizeof(lease->address);
	} else
		ifi->requested_address.s_addr = INADDR_ANY;

	/* Send any options requested in the config file. */
	for (i = 0; i < DHO_COUNT; i++)
		if (options[i].data == NULL &&
		    config->send_options[i].data != NULL) {
			options[i].data = config->send_options[i].data;
			options[i].len = config->send_options[i].len;
		}

	/*
	 * Set up the option buffer to fit in a 576-byte UDP packet, which
	 * RFC 791 says is the largest packet that *MUST* be accepted
	 * by any host.
	 */
	i = pack_options(ifi->sent_packet.options, 576 - DHCP_FIXED_LEN,
	    options);
	if (i == -1 || packet->options[i] != DHO_END)
		fatalx("options do not fit in DHCPDISCOVER packet");
	ifi->sent_packet_length = DHCP_FIXED_NON_UDP+i+1;
	if (ifi->sent_packet_length < BOOTP_MIN_LEN)
		ifi->sent_packet_length = BOOTP_MIN_LEN;

	packet->op = BOOTREQUEST;
	packet->htype = HTYPE_ETHER;
	packet->hlen = ETHER_ADDR_LEN;
	packet->hops = 0;
	packet->xid = ifi->xid;
	packet->secs = 0; /* filled in by send_discover. */
	packet->flags = 0;

	packet->ciaddr.s_addr = INADDR_ANY;
	packet->yiaddr.s_addr = INADDR_ANY;
	packet->siaddr.s_addr = INADDR_ANY;
	packet->giaddr.s_addr = INADDR_ANY;

	memcpy(&packet->chaddr, ifi->hw_address.ether_addr_octet,
	    ETHER_ADDR_LEN);
}

void
make_request(struct interface_info *ifi, struct client_lease *lease)
{
	struct option_data	 options[DHO_COUNT];
	struct dhcp_packet	*packet = &ifi->sent_packet;
	unsigned char		 request = DHCPREQUEST;
	int			 i;

	memset(options, 0, sizeof(options));
	memset(packet, 0, sizeof(*packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPREQUEST */
	i = DHO_DHCP_MESSAGE_TYPE;
	options[i].data = &request;
	options[i].len = sizeof(request);

	/* Request the options we want */
	i = DHO_DHCP_PARAMETER_REQUEST_LIST;
	options[i].data = config->requested_options;
	options[i].len = config->requested_option_count;

	/*
	 * If we are requesting an address that hasn't yet been assigned
	 * to us, use the DHCP Requested Address option.
	 */
	if (ifi->state == S_REQUESTING) {
		/* Send back the server identifier. */
		i = DHO_DHCP_SERVER_IDENTIFIER;
		options[i].data = lease->options[i].data;
		options[i].len = lease->options[i].len;
	}
	if (ifi->state == S_REQUESTING ||
	    ifi->state == S_REBOOTING) {
		i = DHO_DHCP_REQUESTED_ADDRESS;
		options[i].data = (char *)&lease->address.s_addr;
		options[i].len = sizeof(lease->address.s_addr);
	}

	/* Send any options requested in the config file. */
	for (i = 0; i < DHO_COUNT; i++)
		if (options[i].data == NULL &&
		    config->send_options[i].data != NULL) {
			options[i].data = config->send_options[i].data;
			options[i].len = config->send_options[i].len;
		}

	/*
	 * Set up the option buffer to fit in a 576-byte UDP packet, which
	 * RFC 791 says is the largest packet that *MUST* be accepted
	 * by any host.
	 */
	i = pack_options(ifi->sent_packet.options, 576 - DHCP_FIXED_LEN,
	    options);
	if (i == -1 || packet->options[i] != DHO_END)
		fatalx("options do not fit in DHCPREQUEST packet");
	ifi->sent_packet_length = DHCP_FIXED_NON_UDP+i+1;
	if (ifi->sent_packet_length < BOOTP_MIN_LEN)
		ifi->sent_packet_length = BOOTP_MIN_LEN;

	packet->op = BOOTREQUEST;
	packet->htype = HTYPE_ETHER;
	packet->hlen = ETHER_ADDR_LEN;
	packet->hops = 0;
	packet->xid = ifi->xid;
	packet->secs = 0; /* Filled in by send_request. */
	packet->flags = 0;

	/*
	 * If we own the address we're requesting, put it in ciaddr. Otherwise
	 * set ciaddr to zero.
	 */
	ifi->requested_address = lease->address;
	if (ifi->state == S_BOUND ||
	    ifi->state == S_RENEWING)
		packet->ciaddr.s_addr = lease->address.s_addr;
	else
		packet->ciaddr.s_addr = INADDR_ANY;

	packet->yiaddr.s_addr = INADDR_ANY;
	packet->siaddr.s_addr = INADDR_ANY;
	packet->giaddr.s_addr = INADDR_ANY;

	memcpy(&packet->chaddr, ifi->hw_address.ether_addr_octet,
	    ETHER_ADDR_LEN);
}

void
make_decline(struct interface_info *ifi, struct client_lease *lease)
{
	struct option_data	 options[DHO_COUNT];
	struct dhcp_packet	*packet = &ifi->sent_packet;
	unsigned char		 decline = DHCPDECLINE;
	int			 i;

	memset(options, 0, sizeof(options));
	memset(packet, 0, sizeof(*packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPDECLINE */
	i = DHO_DHCP_MESSAGE_TYPE;
	options[i].data = &decline;
	options[i].len = sizeof(decline);

	/* Send back the server identifier. */
	i = DHO_DHCP_SERVER_IDENTIFIER;
	options[i].data = lease->options[i].data;
	options[i].len = lease->options[i].len;

	/* Send back the address we're declining. */
	i = DHO_DHCP_REQUESTED_ADDRESS;
	options[i].data = (char *)&lease->address.s_addr;
	options[i].len = sizeof(lease->address.s_addr);

	/* Send the uid if the user supplied one. */
	i = DHO_DHCP_CLIENT_IDENTIFIER;
	if (config->send_options[i].len != 0) {
		options[i].data = config->send_options[i].data;
		options[i].len = config->send_options[i].len;
	}

	/*
	 * Set up the option buffer to fit in a 576-byte UDP packet, which
	 * RFC 791 says is the largest packet that *MUST* be accepted
	 * by any host.
	 */
	i = pack_options(ifi->sent_packet.options, 576 - DHCP_FIXED_LEN,
	    options);
	if (i == -1 || packet->options[i] != DHO_END)
		fatalx("options do not fit in DHCPDECLINE packet");
	ifi->sent_packet_length = DHCP_FIXED_NON_UDP+i+1;
	if (ifi->sent_packet_length < BOOTP_MIN_LEN)
		ifi->sent_packet_length = BOOTP_MIN_LEN;

	packet->op = BOOTREQUEST;
	packet->htype = HTYPE_ETHER;
	packet->hlen = ETHER_ADDR_LEN;
	packet->hops = 0;
	packet->xid = ifi->xid;
	packet->secs = 0;
	packet->flags = 0;

	/* ciaddr must always be zero. */
	packet->ciaddr.s_addr = INADDR_ANY;
	packet->yiaddr.s_addr = INADDR_ANY;
	packet->siaddr.s_addr = INADDR_ANY;
	packet->giaddr.s_addr = INADDR_ANY;

	memcpy(&packet->chaddr, ifi->hw_address.ether_addr_octet,
	    ETHER_ADDR_LEN);
}

void
make_release(struct interface_info *ifi, struct client_lease *lease)
{
	struct option_data	 options[DHO_COUNT];
	struct dhcp_packet	*packet = &ifi->sent_packet;
	unsigned char		 release = DHCPRELEASE;
	int			 i;

	memset(options, 0, sizeof(options));
	memset(packet, 0, sizeof(*packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPRELEASE */
	i = DHO_DHCP_MESSAGE_TYPE;
	options[i].data = &release;
	options[i].len = sizeof(release);

	/* Send back the server identifier. */
	i = DHO_DHCP_SERVER_IDENTIFIER;
	options[i].data = lease->options[i].data;
	options[i].len = lease->options[i].len;

	i = pack_options(ifi->sent_packet.options, 576 - DHCP_FIXED_LEN,
	    options);
	if (i == -1 || packet->options[i] != DHO_END)
		fatalx("options do not fit in DHCPRELEASE packet");
	ifi->sent_packet_length = DHCP_FIXED_NON_UDP+i+1;
	if (ifi->sent_packet_length < BOOTP_MIN_LEN)
		ifi->sent_packet_length = BOOTP_MIN_LEN;

	packet->op = BOOTREQUEST;
	packet->htype = HTYPE_ETHER;
	packet->hlen = ETHER_ADDR_LEN;
	packet->hops = 0;
	packet->xid = ifi->xid;
	packet->secs = 0;
	packet->flags = 0;

	/*
	 * Note we return the *offered* address. NOT the configured address
	 * which could have been changed via dhclient.conf. But the packet
	 * is sent from the *configured* address.
	 *
	 * This might easily confuse a server, but if you play with fire
	 * by modifying the address you are on your own!
	 */
	packet->ciaddr.s_addr = ifi->active->address.s_addr;
	packet->yiaddr.s_addr = INADDR_ANY;
	packet->siaddr.s_addr = INADDR_ANY;
	packet->giaddr.s_addr = INADDR_ANY;

	memcpy(&packet->chaddr, ifi->hw_address.ether_addr_octet,
	    ETHER_ADDR_LEN);
}

void
free_client_lease(struct client_lease *lease)
{
	int	 i;

	if (lease == NULL)
		return;

	free(lease->server_name);
	free(lease->filename);
	for (i = 0; i < DHO_COUNT; i++)
		free(lease->options[i].data);

	free(lease);
}

void
write_lease_db(struct interface_info *ifi)
{
	struct client_lease_tq *lease_db = &ifi->lease_db;
	struct client_lease	*lp, *pl;
	char			*leasestr;

	TAILQ_FOREACH_SAFE(lp, lease_db, next, pl) {
		if (lp != ifi->active && lease_expiry(lp) == 0) {
			TAILQ_REMOVE(lease_db, lp, next);
			free_client_lease(lp);
		}
	}

	if (leaseFile == NULL)
		return;

	rewind(leaseFile);

	/*
	 * The leases file is kept in chronological order, with the
	 * most recently bound lease last. When the file was read
	 * leases that were not expired were added to the head of the
	 * TAILQ ifi->leases as they were read. Therefore write out
	 * the leases in ifi->leases in reverse order to recreate
	 * the chonological order required.
	 */
	TAILQ_FOREACH_REVERSE(lp, lease_db, client_lease_tq, next) {
		leasestr = lease_as_string("lease", lp);
		if (leasestr != NULL)
			fprintf(leaseFile, "%s", leasestr);
		else
			log_warnx("%s: cannot make lease into string",
			    log_procname);
	}

	fflush(leaseFile);
	ftruncate(fileno(leaseFile), ftello(leaseFile));
	fsync(fileno(leaseFile));
}

void
append_statement(char *string, size_t sz, char *s1, char *s2)
{
	strlcat(string, s1, sz);
	strlcat(string, s2, sz);
	strlcat(string, ";\n", sz);
}

struct unwind_info *
lease_as_unwind_info(struct client_lease *lease)
{
	struct unwind_info	*unwind_info;
	struct option_data	*opt;
	unsigned int		 servers;

	unwind_info = calloc(1, sizeof(*unwind_info));
	if (unwind_info == NULL)
		fatal("unwind_info");

	opt = &lease->options[DHO_DOMAIN_NAME_SERVERS];
	if (opt->len != 0) {
		servers = opt->len / sizeof(in_addr_t);
		if (servers > MAXNS)
			servers = MAXNS;
		if (servers > 0) {
			unwind_info->count = servers;
			memcpy(unwind_info->ns, opt->data, servers *
			    sizeof(in_addr_t));
		}
	}

	if (unwind_info->count == 0) {
		free(unwind_info);
		unwind_info = NULL;
	}

	return unwind_info;
}

struct proposal *
lease_as_proposal(struct client_lease *lease)
{
	uint8_t			 defroute[5];	/* 1 + sizeof(in_addr_t) */
	struct option_data	 fake;
	struct option_data	*opt;
	struct proposal		*proposal;
	uint8_t			*ns, *p, *routes, *domains;
	unsigned int		 routes_len = 0, domains_len = 0, ns_len = 0;
	uint16_t		 mtu;

	/* Determine sizes of variable length data. */
	opt = NULL;
	if (lease->options[DHO_CLASSLESS_STATIC_ROUTES].len != 0) {
		opt = &lease->options[DHO_CLASSLESS_STATIC_ROUTES];
	} else if (lease->options[DHO_CLASSLESS_MS_STATIC_ROUTES].len != 0) {
		opt = &lease->options[DHO_CLASSLESS_MS_STATIC_ROUTES];
	} else if (lease->options[DHO_ROUTERS].len != 0) {
		/* Fake a classless static default route. */
		opt = &lease->options[DHO_ROUTERS];
		fake.len = sizeof(defroute);
		fake.data = defroute;
		fake.data[0] = 0;
		memcpy(&fake.data[1], opt->data, sizeof(defroute) - 1);
		opt = &fake;
	}
	if (opt != NULL) {
		routes_len = opt->len;
		routes = opt->data;
	}

	opt = NULL;
	if (lease->options[DHO_DOMAIN_SEARCH].len != 0)
		opt = &lease->options[DHO_DOMAIN_SEARCH];
	else if (lease->options[DHO_DOMAIN_NAME].len != 0)
		opt = &lease->options[DHO_DOMAIN_NAME];
	if (opt != NULL) {
		domains_len = opt->len;
		domains = opt->data;
	}

	if (lease->options[DHO_DOMAIN_NAME_SERVERS].len != 0) {
		opt = &lease->options[DHO_DOMAIN_NAME_SERVERS];
		ns_len = opt->len;
		ns = opt->data;
	}

	/* Allocate proposal. */
	proposal = calloc(1, sizeof(*proposal) + routes_len + domains_len +
	    ns_len);
	if (proposal == NULL)
		fatal("proposal");

	/* Fill in proposal. */
	proposal->address = lease->address;

	opt = &lease->options[DHO_INTERFACE_MTU];
	if (opt->len == sizeof(mtu)) {
		memcpy(&mtu, opt->data, sizeof(mtu));
		proposal->mtu = ntohs(mtu);
	}

	opt = &lease->options[DHO_SUBNET_MASK];
	if (opt->len == sizeof(proposal->netmask))
		memcpy(&proposal->netmask, opt->data, opt->len);

	/* Append variable length uint8_t data. */
	p = (uint8_t *)proposal + sizeof(struct proposal);
	memcpy(p, routes, routes_len);
	p += routes_len;
	proposal->routes_len = routes_len;
	memcpy(p, domains, domains_len);
	p += domains_len;
	proposal->domains_len = domains_len;
	memcpy(p, ns, ns_len);
	proposal->ns_len = ns_len;

	return proposal;
}

char *
lease_as_string(char *type, struct client_lease *lease)
{
	static char		 string[8192];
	char			 timebuf[27];	/* 6 2017/04/08 05:47:50 UTC; */
	struct option_data	*opt;
	char			*buf, *name;
	time_t			 t;
	size_t			 rslt;
	int			 i;

	memset(string, 0, sizeof(string));

	strlcat(string, type, sizeof(string));
	strlcat(string, " {\n", sizeof(string));
	strlcat(string, BOOTP_LEASE(lease) ? "  bootp;\n" : "", sizeof(string));

	append_statement(string, sizeof(string), "  fixed-address ",
	    inet_ntoa(lease->address));
	append_statement(string, sizeof(string), "  next-server ",
	    inet_ntoa(lease->next_server));

	if (lease->filename != NULL) {
		buf = pretty_print_string(lease->filename,
		    strlen(lease->filename), 1);
		if (buf == NULL)
			return NULL;
		append_statement(string, sizeof(string), "  filename ", buf);
	}
	if (lease->server_name != NULL) {
		buf = pretty_print_string(lease->server_name,
		    strlen(lease->server_name), 1);
		if (buf == NULL)
			return NULL;
		append_statement(string, sizeof(string), "  server-name ",
		    buf);
	}
	if (lease->ssid_len != 0) {
		buf = pretty_print_string(lease->ssid, lease->ssid_len, 1);
		if (buf == NULL)
			return NULL;
		append_statement(string, sizeof(string), "  ssid ", buf);
	}

	for (i = 0; i < DHO_COUNT; i++) {
		opt = &lease->options[i];
		if (opt->len == 0)
			continue;
		name = code_to_name(i);

		buf = pretty_print_option(i, opt, 1);
		if (strlen(buf) == 0)
			continue;
		strlcat(string, "  option ", sizeof(string));
		strlcat(string, name, sizeof(string));
		append_statement(string, sizeof(string), " ", buf);
	}

	i = asprintf(&buf, "%lld", (long long)lease->epoch);
	if (i == -1)
		return NULL;
	append_statement(string, sizeof(string), "  epoch ", buf);
	free(buf);

	t = lease->epoch + lease_renewal(lease);
	rslt = strftime(timebuf, sizeof(timebuf), DB_TIMEFMT, gmtime(&t));
	if (rslt == 0)
		return NULL;
	append_statement(string, sizeof(string), "  renew ", timebuf);

	t = lease->epoch + lease_rebind(lease);
	rslt = strftime(timebuf, sizeof(timebuf), DB_TIMEFMT, gmtime(&t));
	if (rslt == 0)
		return NULL;
	append_statement(string, sizeof(string), "  rebind ", timebuf);

	t = lease->epoch + lease_expiry(lease);
	rslt = strftime(timebuf, sizeof(timebuf), DB_TIMEFMT, gmtime(&t));
	if (rslt == 0)
		return NULL;
	append_statement(string, sizeof(string), "  expire ", timebuf);

	rslt = strlcat(string, "}\n", sizeof(string));
	if (rslt >= sizeof(string))
		return NULL;

	return string;
}

void
go_daemon(void)
{
	static int	 daemonized = 0;

	if ((cmd_opts & OPT_FOREGROUND) != 0 || daemonized != 0)
		return;

	daemonized = 1;

	if (rdaemon(nullfd) == -1)
		fatal("daemonize");

	/* Stop logging to stderr. */
	log_init(0, LOG_DAEMON);
	if ((cmd_opts & OPT_VERBOSE) != 0)
		log_setverbose(1);	/* Show log_debug() messages. */
	log_procinit(log_procname);

	setproctitle("%s", log_procname);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
}

int
rdaemon(int devnull)
{
	if (devnull == -1) {
		errno = EBADF;
		return -1;
	}
	if (fcntl(devnull, F_GETFL) == -1)
		return -1;

	switch (fork()) {
	case -1:
		return -1;
	case 0:
		break;
	default:
		_exit(0);
	}

	if (setsid() == -1)
		return -1;

	(void)dup2(devnull, STDIN_FILENO);
	(void)dup2(devnull, STDOUT_FILENO);
	(void)dup2(devnull, STDERR_FILENO);
	if (devnull > 2)
		(void)close(devnull);

	return 0;
}

/*
 * resolv_conf(5) says a max of DHCP_DOMAIN_SEARCH_CNT domains and total
 * length of DHCP_DOMAIN_SEARCH_LEN bytes are acceptable for the 'search'
 * statement.
 */
int
res_hnok_list(const char *names)
{
	char	*dupnames, *hn, *inputstring;
	int	 count;

	if (strlen(names) >= DHCP_DOMAIN_SEARCH_LEN)
		return 0;

	dupnames = inputstring = strdup(names);
	if (inputstring == NULL)
		fatal("domain name list");

	count = 0;
	while ((hn = strsep(&inputstring, " \t")) != NULL) {
		if (strlen(hn) == 0)
			continue;
		if (res_hnok(hn) == 0)
			break;
		count++;
		if (count > DHCP_DOMAIN_SEARCH_CNT)
			break;
	}

	free(dupnames);

	return count > 0 && count < 7 && hn == NULL;
}

/*
 * Decode a byte string encoding a list of domain names as specified in RFC1035
 * section 4.1.4.
 *
 * The result is a string consisting of a blank separated list of domain names.
 *
 * e.g.
 *
 * 3:65:6e:67:5:61:70:70:6c:65:3:63:6f:6d:0:9:6d:61:72:6b:65:74:69:6e:67:c0:04
 *
 * which represents
 *
 *    3 |'e'|'n'|'g'| 5 |'a'|'p'|'p'|'l'|
 *   'e'| 3 |'c'|'o'|'m'| 0 | 9 |'m'|'a'|
 *   'r'|'k'|'e'|'t'|'i'|'n'|'g'|xC0|x04|
 *
 * will be translated to
 *
 * "eng.apple.com. marketing.apple.com."
 */
char *
rfc1035_as_string(unsigned char *src, size_t srclen)
{
	static char		 search[DHCP_DOMAIN_SEARCH_LEN];
	unsigned char		 name[DHCP_DOMAIN_SEARCH_LEN];
	unsigned char		*endsrc, *cp;
	int			 len, domains;

	memset(search, 0, sizeof(search));

	/* Compute expanded length. */
	domains = 0;
	cp = src;
	endsrc = src + srclen;

	while (cp < endsrc && domains < DHCP_DOMAIN_SEARCH_CNT) {
		len = dn_expand(src, endsrc, cp, name, sizeof(name));
		if (len == -1)
			goto bad;
		cp += len;
		if (domains > 0)
			strlcat(search, " ", sizeof(search));
		strlcat(search, name, sizeof(search));
		if (strlcat(search, ".", sizeof(search)) >= sizeof(search))
			goto bad;
		domains++;
	}

	return search;

bad:
	memset(search, 0, sizeof(search));
	return search;
}

void
fork_privchld(struct interface_info *ifi, int fd, int fd2)
{
	struct pollfd	 pfd[1];
	struct imsgbuf	*priv_ibuf;
	ssize_t		 n;
	int		 ioctlfd, routefd, nfds, rslt;

	switch (fork()) {
	case -1:
		fatal("fork");
		break;
	case 0:
		break;
	default:
		return;
	}

	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	go_daemon();

	free(log_procname);
	rslt = asprintf(&log_procname, "%s [priv]", ifi->name);
	if (rslt == -1)
		fatal("log_procname");
	setproctitle("%s", log_procname);
	log_procinit(log_procname);

	close(fd2);

	if ((priv_ibuf = malloc(sizeof(*priv_ibuf))) == NULL)
		fatal("priv_ibuf");

	imsg_init(priv_ibuf, fd);

	if ((ioctlfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fatal("socket(AF_INET, SOCK_DGRAM)");
	if ((routefd = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		fatal("socket(AF_ROUTE, SOCK_RAW)");

	if (unveil(_PATH_RESCONF, "wc") == -1)
		fatal("unveil %s", _PATH_RESCONF);
	if (unveil("/etc/resolv.conf.tail", "r") == -1)
		fatal("unveil /etc/resolve.conf.tail");
	if (unveil(NULL, NULL) == -1)
		fatal("unveil");

	while (quit == 0) {
		pfd[0].fd = priv_ibuf->fd;
		pfd[0].events = POLLIN;

		nfds = ppoll(pfd, 1, NULL, NULL);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			log_warn("%s: ppoll(priv_ibuf)", log_procname);
			break;
		}
		if ((pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
			break;
		if (nfds == 0 || (pfd[0].revents & POLLIN) == 0)
			continue;

		if ((n = imsg_read(priv_ibuf)) == -1 && errno != EAGAIN) {
			log_warn("%s: imsg_read(priv_ibuf)", log_procname);
			break;
		}
		if (n == 0) {
			/* Connection closed - other end should log message. */
			break;
		}

		dispatch_imsg(ifi->name, ifi->rdomain, ioctlfd, routefd,
		    priv_ibuf);
	}
	close(routefd);
	close(ioctlfd);

	imsg_clear(priv_ibuf);
	close(fd);

	exit(1);
}

struct client_lease *
apply_defaults(struct client_lease *lease)
{
	struct option_data	 emptyopt = {0, NULL};
	struct client_lease	*newlease;
	char			*fmt;
	int			 i;

	newlease = clone_lease(lease);
	if (newlease == NULL)
		fatalx("unable to clone lease");

	if (config->filename != NULL) {
		free(newlease->filename);
		newlease->filename = strdup(config->filename);
		if (newlease->filename == NULL)
			fatal("strdup(config->filename)");
	}
	if (config->server_name != NULL) {
		free(newlease->server_name);
		newlease->server_name = strdup(config->server_name);
		if (newlease->server_name == NULL)
			fatal("strdup(config->server_name)");
	}
	if (config->address.s_addr != INADDR_ANY)
		newlease->address.s_addr = config->address.s_addr;
	if (config->next_server.s_addr != INADDR_ANY)
		newlease->next_server.s_addr = config->next_server.s_addr;

	for (i = 0; i < DHO_COUNT; i++) {
		fmt = code_to_format(i);
		switch (config->default_actions[i]) {
		case ACTION_IGNORE:
			merge_option_data(fmt, &emptyopt, &emptyopt,
			    &newlease->options[i]);
			break;

		case ACTION_SUPERSEDE:
			merge_option_data(fmt, &config->defaults[i], &emptyopt,
			    &newlease->options[i]);
			break;

		case ACTION_PREPEND:
			merge_option_data(fmt, &config->defaults[i],
			    &lease->options[i], &newlease->options[i]);
			break;

		case ACTION_APPEND:
			merge_option_data(fmt, &lease->options[i],
			    &config->defaults[i], &newlease->options[i]);
			break;

		case ACTION_DEFAULT:
			if (newlease->options[i].len == 0)
				merge_option_data(fmt, &config->defaults[i],
				    &emptyopt, &newlease->options[i]);
			break;

		default:
			break;
		}
	}

	if (newlease->options[DHO_STATIC_ROUTES].len != 0) {
		log_debug("%s: DHO_STATIC_ROUTES (option 33) not supported",
		    log_procname);
		free(newlease->options[DHO_STATIC_ROUTES].data);
		newlease->options[DHO_STATIC_ROUTES].data = NULL;
		newlease->options[DHO_STATIC_ROUTES].len = 0;
	}

	/*
	 * RFC 3442 says client *MUST* ignore DHO_ROUTERS
	 * when DHO_CLASSLESS_[MS_]_ROUTES present.
	 */
	if ((newlease->options[DHO_CLASSLESS_MS_STATIC_ROUTES].len != 0) ||
	    (newlease->options[DHO_CLASSLESS_STATIC_ROUTES].len != 0)) {
		free(newlease->options[DHO_ROUTERS].data);
		newlease->options[DHO_ROUTERS].data = NULL;
		newlease->options[DHO_ROUTERS].len = 0;
	}

	return newlease;
}

struct client_lease *
clone_lease(struct client_lease *oldlease)
{
	struct client_lease	*newlease;
	int			 i;

	newlease = calloc(1, sizeof(*newlease));
	if (newlease == NULL)
		goto cleanup;

	newlease->epoch = oldlease->epoch;
	newlease->address = oldlease->address;
	newlease->next_server = oldlease->next_server;
	memcpy(newlease->ssid, oldlease->ssid, sizeof(newlease->ssid));
	newlease->ssid_len = oldlease->ssid_len;

	if (oldlease->server_name != NULL) {
		newlease->server_name = strdup(oldlease->server_name);
		if (newlease->server_name == NULL)
			goto cleanup;
	}
	if (oldlease->filename != NULL) {
		newlease->filename = strdup(oldlease->filename);
		if (newlease->filename == NULL)
			goto cleanup;
	}

	for (i = 0; i < DHO_COUNT; i++) {
		if (oldlease->options[i].len == 0)
			continue;
		newlease->options[i].len = oldlease->options[i].len;
		newlease->options[i].data = calloc(1,
		    newlease->options[i].len);
		if (newlease->options[i].data == NULL)
			goto cleanup;
		memcpy(newlease->options[i].data, oldlease->options[i].data,
		    newlease->options[i].len);
	}

	return newlease;

cleanup:
	free_client_lease(newlease);

	return NULL;
}

int
autoconf(struct interface_info *ifi)
{
	struct ifreq            ifr;
	int			ioctlfd;

	if ((ioctlfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fatal("socket(AF_INET, SOCK_DGRAM)");

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifi->name, sizeof(ifr.ifr_name));

	if (ioctl(ioctlfd, SIOCGIFXFLAGS, (caddr_t)&ifr) < 0)
		fatal("SIOGIFXFLAGS");

	close(ioctlfd);

	return ifr.ifr_flags & IFXF_AUTOCONF4;
}

int
take_charge(struct interface_info *ifi, int routefd, char *leasespath)
{
	const struct timespec	 max_timeout = { 9, 0 };
	const struct timespec	 resend_intvl = { 3, 0 };
	const struct timespec	 leasefile_intvl = { 0, 3000000 };
	struct timespec		 now, resend, stop, timeout;
	struct pollfd		 fds[1];
	struct rt_msghdr	 rtm;
	int			 fd, nfds;

	clock_gettime(CLOCK_MONOTONIC, &now);
	resend = now;
	timespecadd(&now, &max_timeout, &stop);

	/*
	 * Send RTM_PROPOSAL with RTF_PROTO3 set.
	 *
	 * When it comes back, we're in charge and other dhclients are
	 * dead processes walking.
	 */
	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_PROPOSAL;
	rtm.rtm_msglen = sizeof(rtm);
	rtm.rtm_tableid = ifi->rdomain;
	rtm.rtm_index = ifi->index;
	rtm.rtm_priority = RTP_PROPOSAL_DHCLIENT;
	rtm.rtm_addrs = 0;
	rtm.rtm_flags = RTF_UP | RTF_PROTO3;

	for (fd = -1; fd == -1 && quit != TERMINATE;) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (timespeccmp(&now, &stop, >=))
			fatalx("failed to take charge");

		if ((ifi->flags & IFI_IN_CHARGE) == 0) {
			if (timespeccmp(&now, &resend, >=)) {
				timespecadd(&resend, &resend_intvl, &resend);
				rtm.rtm_seq = ifi->xid = arc4random();
				if (write(routefd, &rtm, sizeof(rtm)) == -1)
					fatal("write(routefd)");
			}
			timespecsub(&resend, &now, &timeout);
		} else {
			/*
			 * Keep trying to open leasefile in 3ms intervals
			 * while continuing to process any RTM_* messages
			 * that come in.
			 */
			 timeout = leasefile_intvl;
		}

		fds[0].fd = routefd;
		fds[0].events = POLLIN;
		nfds = ppoll(fds, 1, &timeout, NULL);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			fatal("ppoll(routefd)");
		}

		if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
			fatalx("routefd: ERR|HUP|NVAL");
		if (nfds == 1 && (fds[0].revents & POLLIN) == POLLIN)
			routefd_handler(ifi, routefd);

		if (quit != TERMINATE && (ifi->flags & IFI_IN_CHARGE) == IFI_IN_CHARGE) {
			fd = open(leasespath, O_NONBLOCK |
			    O_RDONLY|O_EXLOCK|O_CREAT|O_NOFOLLOW, 0640);
			if (fd == -1 && errno != EWOULDBLOCK)
				break;
		}
	}

	return fd;
}

struct client_lease *
get_recorded_lease(struct interface_info *ifi)
{
	char			 ifname[IF_NAMESIZE];
	struct client_lease	*lp;
	int			 i;

	/* Update on-disk db, which clears out expired leases. */
	ifi->active = NULL;
	write_lease_db(ifi);

	/* Run through the list of leases and see if one can be used. */
	i = DHO_DHCP_CLIENT_IDENTIFIER;
	TAILQ_FOREACH(lp, &ifi->lease_db, next) {
		if (lp->ssid_len != ifi->ssid_len)
			continue;
		if (memcmp(lp->ssid, ifi->ssid, lp->ssid_len) != 0)
			continue;
		if ((lp->options[i].len != 0) && ((lp->options[i].len !=
		    config->send_options[i].len) ||
		    memcmp(lp->options[i].data, config->send_options[i].data,
		    lp->options[i].len) != 0))
			continue;
		if (addressinuse(ifi->name, lp->address, ifname) != 0 &&
		    strncmp(ifname, ifi->name, IF_NAMESIZE) != 0)
			continue;
		break;
	}

	return lp;
}

time_t
lease_expiry(struct client_lease *lease)
{
	time_t		cur_time;
	uint32_t	expiry;

	time(&cur_time);
	expiry = 0;
	if (lease->options[DHO_DHCP_LEASE_TIME].len == sizeof(expiry)) {
		memcpy(&expiry, lease->options[DHO_DHCP_LEASE_TIME].data,
		    sizeof(expiry));
		expiry = ntohl(expiry);
		if (expiry < 60)
			expiry = 60;
	}
	expiry = lease->epoch + expiry - cur_time;

	return (expiry > 0) ? expiry : 0;
}

time_t
lease_renewal(struct client_lease *lease)
{
	time_t		 cur_time, expiry;
	uint32_t	 renewal;

	time(&cur_time);
	expiry = lease_expiry(lease);

	renewal = expiry / 2;
	if (lease->options[DHO_DHCP_RENEWAL_TIME].len == sizeof(renewal)) {
		memcpy(&renewal, lease->options[DHO_DHCP_RENEWAL_TIME].data,
		    sizeof(renewal));
		renewal = ntohl(renewal);
	}
	renewal = lease->epoch + renewal - cur_time;

	return (renewal > 0) ? renewal : 0;
}

time_t
lease_rebind(struct client_lease *lease)
{
	time_t		cur_time, expiry;
	uint32_t	rebind;

	time(&cur_time);
	expiry = lease_expiry(lease);

	rebind = (expiry / 8) * 7;
	if (lease->options[DHO_DHCP_REBINDING_TIME].len == sizeof(rebind)) {
		memcpy(&rebind, lease->options[DHO_DHCP_REBINDING_TIME].data,
		    sizeof(rebind));
		rebind = ntohl(rebind);
	}
	rebind = lease->epoch + rebind - cur_time;

	return (rebind > 0) ? rebind : 0;
}

void
get_lease_timeouts(struct interface_info *ifi, struct client_lease *lease)
{
	struct timespec	now, interval;

	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecclear(&interval);

	interval.tv_sec = lease_expiry(lease);
	timespecadd(&now, &interval, &ifi->expiry);

	interval.tv_sec = lease_rebind(lease);
	timespecadd(&now, &interval, &ifi->rebind);

	interval.tv_sec = lease_renewal(lease);
	timespecadd(&now, &interval, &ifi->renew);

	if (timespeccmp(&ifi->rebind, &ifi->expiry, >))
		ifi->rebind = ifi->expiry;
	if (timespeccmp(&ifi->renew, &ifi->rebind, >))
		ifi->renew = ifi->rebind;
}

void
tick_msg(const char *preamble, int action)
{
	const struct timespec		grace_intvl = {3, 0};
	const struct timespec		link_intvl = {config->link_interval, 0};
	static struct timespec		grace, stop;
	struct timespec			now;
	static int			linkup, preamble_sent, sleeping;
	int				printmsg;

	clock_gettime(CLOCK_MONOTONIC, &now);

	if (!timespecisset(&stop)) {
		preamble_sent = 0;
		timespecadd(&now, &link_intvl, &stop);
		timespecadd(&now, &grace_intvl, &grace);
		return;
	}

	if (isatty(STDERR_FILENO) == 0 || sleeping == 1)
		printmsg = 0;	/* Already in the background. */
	else if (timespeccmp(&now, &grace, <))
		printmsg = 0;	/* Wait a bit before speaking. */
	else if (linkup && strcmp("link", preamble) == 0)
		printmsg = 0;	/* One 'got link' is enough for anyone. */
	else if (log_getverbose())
		printmsg = 0;	/* Verbose has sufficent verbiage. */
	else
		printmsg = 1;

	if (timespeccmp(&now, &stop, >=)) {
		if (action == TICK_WAIT)
			action = TICK_DAEMON;
		if (linkup == 0) {
			log_debug("%s: link timeout (%lld seconds) expired",
			    log_procname, (long long)link_intvl.tv_sec);
			linkup = 1;
		}
	}

	if (printmsg && preamble_sent == 0) {
		fprintf(stderr, "%s: no %s...", log_procname, preamble);
		preamble_sent = 1;
	}

	switch (action) {
	case TICK_SUCCESS:
		if (printmsg)
			fprintf(stderr, "got %s\n", preamble);
		preamble_sent = 0;
		if (strcmp("link", preamble) == 0) {
			linkup = 1;
			/* New silent period for "no lease ... got lease". */
			timespecadd(&now, &grace_intvl, &grace);
		}
		break;
	case TICK_WAIT:
		if (printmsg)
			fprintf(stderr, ".");
		break;
	case TICK_DAEMON:
		if (printmsg)
			fprintf(stderr, "sleeping\n");
		go_daemon();
		sleeping = 1;	/* OPT_FOREGROUND means isatty() == 1! */
		break;
	default:
		break;
	}

	if (printmsg)
		fflush(stderr);
}

/*
 * Release the lease used to configure the interface.
 *
 * 1) Send DHCPRELEASE.
 * 2) Unconfigure address/routes/etc.
 * 3) Remove lease from database & write updated DB.
 */
void
release_lease(struct interface_info *ifi)
{
	char			 buf[INET_ADDRSTRLEN];
	struct option_data	*opt;

	if (ifi->configured == NULL || ifi->active == NULL)
		return;	/* Nothing to release. */
	strlcpy(buf, inet_ntoa(ifi->configured->address), sizeof(buf));

	opt = &ifi->active->options[DHO_DHCP_SERVER_IDENTIFIER];
	if (opt->len == sizeof(in_addr_t))
		ifi->destination.s_addr = *(in_addr_t *)opt->data;
	else
		ifi->destination.s_addr = INADDR_BROADCAST;

	ifi->xid = arc4random();
	make_release(ifi, ifi->active);
	send_release(ifi);

	tell_unwind(NULL, ifi->flags);

	revoke_proposal(ifi->configured);
	imsg_flush(unpriv_ibuf);

	TAILQ_REMOVE(&ifi->lease_db, ifi->active, next);
	free_client_lease(ifi->active);
	ifi->active = NULL;
	write_lease_db(ifi);

	free(ifi->configured);
	ifi->configured = NULL;
	free(ifi->unwind_info);
	ifi->unwind_info = NULL;

	log_warnx("%s: %s RELEASED to %s", log_procname, buf,
	    inet_ntoa(ifi->destination));
}

void
propose_release(struct interface_info *ifi)
{
	const struct timespec	 max_timeout = { 3, 0 };
	struct timespec		 now, stop, timeout;
	struct pollfd		 fds[1];
	struct rt_msghdr	 rtm;
	int			 nfds, routefd, rtfilter;

	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecadd(&now, &max_timeout, &stop);

	if ((routefd = socket(AF_ROUTE, SOCK_RAW, AF_INET)) == -1)
		fatal("socket(AF_ROUTE, SOCK_RAW)");

	rtfilter = ROUTE_FILTER(RTM_PROPOSAL);

	if (setsockopt(routefd, AF_ROUTE, ROUTE_MSGFILTER,
	    &rtfilter, sizeof(rtfilter)) == -1)
		fatal("setsockopt(ROUTE_MSGFILTER)");
	if (setsockopt(routefd, AF_ROUTE, ROUTE_TABLEFILTER, &ifi->rdomain,
	    sizeof(ifi->rdomain)) == -1)
		fatal("setsockopt(ROUTE_TABLEFILTER)");

	memset(&rtm, 0, sizeof(rtm));
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_PROPOSAL;
	rtm.rtm_msglen = sizeof(rtm);
	rtm.rtm_tableid = ifi->rdomain;
	rtm.rtm_index = ifi->index;
	rtm.rtm_priority = RTP_PROPOSAL_DHCLIENT;
	rtm.rtm_addrs = 0;
	rtm.rtm_flags = RTF_UP;
	rtm.rtm_flags |= RTF_PROTO2;
	rtm.rtm_seq = ifi->xid = arc4random();

	if (write(routefd, &rtm, sizeof(rtm)) == -1)
		fatal("write(routefd)");
	log_debug("%s: sent RTM_PROPOSAL to release lease", log_procname);

	while (quit == 0) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (timespeccmp(&now, &stop, >=))
			break;
		timespecsub(&stop, &now, &timeout);
		fds[0].fd = routefd;
		fds[0].events = POLLIN;
		nfds = ppoll(fds, 1, &timeout, NULL);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			fatal("ppoll(routefd)");
		}
		if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
			fatalx("routefd: ERR|HUP|NVAL");
		if (nfds == 0 || (fds[0].revents & POLLIN) == 0)
			continue;
		routefd_handler(ifi, routefd);
	}
}
