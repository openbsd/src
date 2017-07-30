/*	$OpenBSD: dhclient.c,v 1.489 2017/07/30 15:26:46 krw Exp $	*/

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

char *path_dhclient_conf = _PATH_DHCLIENT_CONF;
char *path_dhclient_db = NULL;

char path_option_db[PATH_MAX];

int log_perror = 1;
int nullfd = -1;
int daemonize = 1;
int unknown_ok = 1;

volatile sig_atomic_t quit;

const struct in_addr inaddr_any = { INADDR_ANY };
const struct in_addr inaddr_broadcast = { INADDR_BROADCAST };

struct client_config *config;
struct imsgbuf *unpriv_ibuf;

struct proposal {
	uint8_t		rtstatic[RTSTATIC_LEN];
	uint8_t		rtsearch[RTSEARCH_LEN];
	uint8_t		rtdns[RTDNS_LEN];
	struct in_addr	ifa;
	struct in_addr	netmask;
	unsigned int	rtstatic_len;
	unsigned int	rtsearch_len;
	unsigned int	rtdns_len;
	int		mtu;
	int		addrs;
	int		inits;
};

void		 sighdlr(int);
void		 usage(void);
int		 res_hnok(const char *dn);
int		 res_hnok_list(const char *dn);
int		 addressinuse(char *, struct in_addr, char *);

void		 fork_privchld(struct interface_info *, int, int);
void		 get_ifname(struct interface_info *, int, char *);
int		 get_ifa_family(char *, int);
void		 interface_link_forceup(char *, int);
int		 interface_status(char *);
void		 get_hw_address(struct interface_info *);

struct client_lease *apply_defaults(struct client_lease *);
struct client_lease *clone_lease(struct client_lease *);
void		 apply_ignore_list(char *);

void set_lease_times(struct client_lease *);

void state_preboot(struct interface_info *);
void state_reboot(struct interface_info *);
void state_init(struct interface_info *);
void state_selecting(struct interface_info *);
void state_bound(struct interface_info *);
void state_panic(struct interface_info *);

void send_discover(struct interface_info *);
void send_request(struct interface_info *);
void send_decline(struct interface_info *);

void bind_lease(struct interface_info *);

void make_discover(struct interface_info *, struct client_lease *);
void make_request(struct interface_info *, struct client_lease *);
void make_decline(struct interface_info *, struct client_lease *);

void rewrite_client_leases(struct interface_info *);
void rewrite_option_db(char *, struct client_lease *, struct client_lease *);
char *lease_as_string(char *, char *, struct client_lease *);
struct proposal *lease_as_proposal(struct client_lease *);
void append_statement(char *, size_t, char *, char *);

struct client_lease *packet_to_lease(struct interface_info *,
    struct option_data *);
void go_daemon(void);
int rdaemon(int);
void	take_charge(struct interface_info *, int);
void	set_default_client_identifier(struct interface_info *);
struct client_lease *get_recorded_lease(struct interface_info *);

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define	ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

static FILE *leaseFile;
static FILE *optionDB;

void
sighdlr(int sig)
{
	quit = sig;
}

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

void
interface_link_forceup(char *name, int ioctlfd)
{
	struct ifreq	 ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(ioctlfd, SIOCGIFFLAGS, (caddr_t)&ifr) == -1) {
		log_warn("SIOCGIFFLAGS");
		return;
	}

	/* Force it up if it isn't already. */
	if ((ifr.ifr_flags & IFF_UP) == 0) {
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(ioctlfd, SIOCSIFFLAGS, (caddr_t)&ifr) == -1) {
			log_warn("SIOCSIFFLAGS");
			return;
		}
	}
}

int
interface_status(char *name)
{
	struct ifaddrs	*ifap, *ifa;
	struct if_data	*ifdata;

	if (getifaddrs(&ifap) != 0)
		fatalx("getifaddrs failed");

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) ||
		    (ifa->ifa_flags & IFF_POINTOPOINT))
			continue;

		if (strcmp(name, ifa->ifa_name) != 0)
			continue;

		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		if ((ifa->ifa_flags & (IFF_UP|IFF_RUNNING)) !=
		    (IFF_UP|IFF_RUNNING))
			return 0;

		ifdata = ifa->ifa_data;

		return LINK_STATE_IS_UP(ifdata->ifi_link_state);
	}

	return 0;
}

void
get_hw_address(struct interface_info *ifi)
{
	struct ifaddrs		*ifap, *ifa;
	struct sockaddr_dl	*sdl;
	struct if_data		*ifdata;
	int			 found;

	if (getifaddrs(&ifap) != 0)
		fatalx("getifaddrs failed");

	found = 0;
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) ||
		    (ifa->ifa_flags & IFF_POINTOPOINT))
			continue;

		if (strcmp(ifi->name, ifa->ifa_name) != 0)
			continue;
		found = 1;

		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		if (sdl->sdl_type != IFT_ETHER ||
		    sdl->sdl_alen != ETHER_ADDR_LEN)
			continue;

		ifdata = ifa->ifa_data;
		ifi->rdomain = ifdata->ifi_rdomain;

		memcpy(ifi->hw_address.ether_addr_octet, LLADDR(sdl),
		    ETHER_ADDR_LEN);
		ifi->flags |= IFI_VALID_LLADDR;
	}

	if (found == 0)
		fatalx("%s: no such interface", ifi->name);

	freeifaddrs(ifap);
}

void
routehandler(struct interface_info *ifi, int routefd)
{
	struct ether_addr		 hw;
	struct rt_msghdr		*rtm;
	struct if_msghdr		*ifm;
	struct if_announcemsghdr	*ifan;
	struct ifa_msghdr		*ifam;
	char				*errmsg, *rtmmsg;
	ssize_t				 n;
	int				 linkstat, rslt;

	rtmmsg = calloc(1, 2048);
	if (rtmmsg == NULL)
		fatalx("No memory for rtmmsg");

	do {
		n = read(routefd, rtmmsg, 2048);
	} while (n == -1 && errno == EINTR);
	if (n == -1)
		goto done;

	rtm = (struct rt_msghdr *)rtmmsg;
	if ((size_t)n < sizeof(rtm->rtm_msglen) || n < rtm->rtm_msglen ||
	    rtm->rtm_version != RTM_VERSION)
		goto done;

	switch (rtm->rtm_type) {
	case RTM_PROPOSAL:
		if (rtm->rtm_index != ifi->index ||
		    rtm->rtm_priority != RTP_PROPOSAL_DHCLIENT)
			goto done;
		if ((rtm->rtm_flags & RTF_PROTO3) != 0) {
			if (rtm->rtm_seq == (int32_t)ifi->xid) {
				ifi->flags |= IFI_IN_CHARGE;
			} else if ((ifi->flags & IFI_IN_CHARGE) != 0) {
				rslt = asprintf(&errmsg, "yielding "
				    "responsibility for %s",
				    ifi->name);
				goto die;
			}
			goto done;
		}
		break;
	case RTM_DESYNC:
		log_warnx("route socket buffer overflow");
		break;
	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		if (ifm->ifm_index != ifi->index)
			break;
		if ((rtm->rtm_flags & RTF_UP) == 0) {
			rslt = asprintf(&errmsg, "%s down", ifi->name);
			goto die;
		}

		if ((ifi->flags & IFI_VALID_LLADDR) != 0) {
			memcpy(&hw, &ifi->hw_address, sizeof(hw));
			get_hw_address(ifi);
			if (memcmp(&hw, &ifi->hw_address, sizeof(hw))) {
				log_warnx("LLADDR changed; restarting");
				sendhup();
				goto done;
			}
		}

		linkstat = interface_status(ifi->name);
		if (linkstat != ifi->linkstat) {
#ifdef DEBUG
			log_debug("link state %s -> %s",
			    (ifi->linkstat != 0) ? "up" : "down",
			    (linkstat != 0) ? "up" : "down");
#endif	/* DEBUG */
			ifi->linkstat = linkstat;
			if (ifi->linkstat != 0) {
				if (ifi->state == S_PREBOOT) {
					state_preboot(ifi);
					get_hw_address(ifi);
				} else {
					ifi->state = S_REBOOTING;
					state_reboot(ifi);
				}
			} else {
				/* No need to wait for anything but link. */
				cancel_timeout(ifi);
			}
		}
		break;
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *)rtm;
		if (ifan->ifan_what == IFAN_DEPARTURE &&
		    ifan->ifan_index == ifi->index) {
			rslt = asprintf(&errmsg, "%s departured", ifi->name);
			goto die;
		}
		break;
	case RTM_NEWADDR:
	case RTM_DELADDR:
		/* Need to check if it is time to write resolv.conf. */
		ifam = (struct ifa_msghdr *)rtm;
		if (ifam->ifam_index != ifi->index)
			goto done;
		if (get_ifa_family((char *)ifam + ifam->ifam_hdrlen,
		    ifam->ifam_addrs) != AF_INET)
			goto done;
		break;
	default:
		break;
	}

	/* Something has happened. Try to write out the resolv.conf. */
	if (ifi->active != NULL && ifi->active->resolv_conf != NULL &&
	    (ifi->flags & IFI_IN_CHARGE) != 0)
		write_resolv_conf(ifi->active->resolv_conf,
		    strlen(ifi->active->resolv_conf));

done:
	free(rtmmsg);
	return;

die:
	if (rslt == -1)
		fatalx("no memory for errmsg");
	fatalx("%s", errmsg);
}

char **saved_argv;

int
main(int argc, char *argv[])
{
	struct ieee80211_nwid	 nwid;
	struct ifreq		 ifr;
	struct stat		 sb;
	const char		*tail_path = "/etc/resolv.conf.tail";
	struct interface_info	*ifi;
	struct passwd		*pw;
	struct client_lease	*lp, *nlp;
	char			*ignore_list = NULL;
	ssize_t			 tailn;
	int			 fd, socket_fd[2];
	int			 rtfilter, ioctlfd, routefd, tailfd;
	int			 ch, q_flag, d_flag;

	saved_argv = argv;

	if (isatty(STDERR_FILENO) != 0)
		log_perror = 1; /* log to stderr until daemonized */
	else
		log_perror = 0; /* can't log to stderr */

	log_init(log_perror, LOG_DAEMON);
	log_setverbose(1);

	q_flag = d_flag = 0;
	while ((ch = getopt(argc, argv, "c:di:l:L:qu")) != -1)
		switch (ch) {
		case 'c':
			path_dhclient_conf = optarg;
			break;
		case 'd':
			d_flag = 1;
			break;
		case 'i':
			ignore_list = optarg;
			break;
		case 'l':
			path_dhclient_db = optarg;
			if (lstat(path_dhclient_db, &sb) != -1) {
				if (S_ISREG(sb.st_mode) == 0)
					fatalx("'%s' is not a regular file",
					    path_dhclient_db);
			}
			break;
		case 'L':
			strlcat(path_option_db, optarg, PATH_MAX);
			if (lstat(path_option_db, &sb) != -1) {
				if (S_ISREG(sb.st_mode) == 0)
					fatalx("'%s' is not a regular file",
					    path_option_db);
			}
			break;
		case 'q':
			q_flag = 1;
			break;
		case 'u':
			unknown_ok = 0;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 1 || (q_flag != 0 && d_flag != 0))
		usage();

	if (d_flag != 0)
		daemonize = 0;

	if (q_flag != 0)
		log_perror = 0;

	log_init(log_perror, LOG_DAEMON);

	ifi = calloc(1, sizeof(struct interface_info));
	if (ifi == NULL)
		fatalx("ifi calloc");
	if ((ioctlfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fatal("Can't create socket to do ioctl");
	get_ifname(ifi, ioctlfd, argv[0]);
	ifi->index = if_nametoindex(ifi->name);
	if (ifi->index == 0)
		fatalx("%s: no such interface", ifi->name);
	get_hw_address(ifi);

	tzset();

	/* Get the ssid if present. */
	memset(&ifr, 0, sizeof(ifr));
	memset(&nwid, 0, sizeof(nwid));
	ifr.ifr_data = (caddr_t)&nwid;
	strlcpy(ifr.ifr_name, ifi->name, sizeof(ifr.ifr_name));
	if (ioctl(ioctlfd, SIOCG80211NWID, (caddr_t)&ifr) == 0) {
		memset(ifi->ssid, 0, sizeof(ifi->ssid));
		memcpy(ifi->ssid, nwid.i_nwid, nwid.i_len);
		ifi->ssid_len = nwid.i_len;
	}

	/* Put us into the correct rdomain */
	if (setrtable(ifi->rdomain) == -1)
		fatal("setting routing table to %u", ifi->rdomain);

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
	    PF_UNSPEC, socket_fd) == -1)
		fatal("socketpair");

	if ((nullfd = open(_PATH_DEVNULL, O_RDWR, 0)) == -1)
		fatal("cannot open %s", _PATH_DEVNULL);

	fork_privchld(ifi, socket_fd[0], socket_fd[1]);

	close(socket_fd[0]);
	if ((unpriv_ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		fatalx("no memory for unpriv_ibuf");
	imsg_init(unpriv_ibuf, socket_fd[1]);

	config = calloc(1, sizeof(struct client_config));
	if (config == NULL)
		fatalx("config calloc");

	read_client_conf(ifi->name);

	/*
	 * Set default client identifier, if needed, *before* reading
	 * the leases file! Changes to the lladdr will trigger a restart
	 * and go through here again.
	 */
	set_default_client_identifier(ifi);

	if ((pw = getpwnam("_dhcp")) == NULL)
		fatalx("no such user: _dhcp");

	if (path_dhclient_db == NULL && asprintf(&path_dhclient_db, "%s.%s",
	    _PATH_DHCLIENT_DB, ifi->name) == -1)
		fatalx("asprintf");

	/* 2nd stage (post fork) config setup. */
	if (ignore_list != NULL)
		apply_ignore_list(ignore_list);

	tailfd = open(tail_path, O_RDONLY);
	if (tailfd == -1) {
		if (errno != ENOENT)
			fatal("Cannot open %s", tail_path);
	} else if (fstat(tailfd, &sb) == -1) {
		fatal("Cannot stat %s", tail_path);
	} else {
		if (sb.st_size > 0 && sb.st_size < LLONG_MAX) {
			config->resolv_tail = calloc(1, sb.st_size + 1);
			if (config->resolv_tail == NULL) {
				fatalx("no memory for %s contents", tail_path);
			}
			tailn = read(tailfd, config->resolv_tail, sb.st_size);
			if (tailn == -1)
				fatal("Couldn't read %s", tail_path);
			else if (tailn == 0)
				fatalx("Got no data from %s", tail_path);
			else if (tailn != sb.st_size)
				fatalx("Short read of %s", tail_path);
		}
		close(tailfd);
	}

	/*
	 * Do the initial status check and possible force up before creating
	 * the routing socket. If we bounce the interface down and up while
	 * the routing socket is listening, the RTM_IFINFO message with the
	 * RTF_UP flag reset will cause premature exit.
	 */
	ifi->linkstat = interface_status(ifi->name);
	if (ifi->linkstat == 0)
		interface_link_forceup(ifi->name, ioctlfd);
	close(ioctlfd);
	ioctlfd = -1;

	if ((routefd = socket(PF_ROUTE, SOCK_RAW, AF_INET)) == -1)
		fatal("socket(PF_ROUTE, SOCK_RAW)");

	rtfilter = ROUTE_FILTER(RTM_PROPOSAL) | ROUTE_FILTER(RTM_IFINFO) |
	    ROUTE_FILTER(RTM_NEWADDR) | ROUTE_FILTER(RTM_DELADDR) |
	    ROUTE_FILTER(RTM_IFANNOUNCE);

	if (setsockopt(routefd, PF_ROUTE, ROUTE_MSGFILTER,
	    &rtfilter, sizeof(rtfilter)) == -1)
		fatal("setsockopt(ROUTE_MSGFILTER)");
	if (setsockopt(routefd, AF_ROUTE, ROUTE_TABLEFILTER, &ifi->rdomain,
	    sizeof(ifi->rdomain)) == -1)
		fatal("setsockopt(ROUTE_TABLEFILTER)");

	take_charge(ifi, routefd);

	if ((fd = open(path_dhclient_db,
	    O_RDONLY|O_EXLOCK|O_CREAT|O_NOFOLLOW, 0640)) == -1)
		fatal("can't open and lock %s", path_dhclient_db);
	read_client_leases(ifi->name, &ifi->leases);
	if ((leaseFile = fopen(path_dhclient_db, "w")) == NULL)
		fatal("can't open %s", path_dhclient_db);
	rewrite_client_leases(ifi);
	close(fd);

	/* Add the static leases to the end of the list of available leases. */
	TAILQ_FOREACH_SAFE(lp, &config->static_leases, next, nlp) {
		TAILQ_REMOVE(&config->static_leases, lp, next);
		lp->is_static = 1;
		TAILQ_INSERT_TAIL(&ifi->leases, lp, next);
	}

	if (strlen(path_option_db) != 0) {
		if ((optionDB = fopen(path_option_db, "a")) == NULL)
			fatal("can't open %s", path_option_db);
	}

	/* Register the interface. */
	ifi->ufdesc = get_udp_sock(ifi->rdomain);
	ifi->bfdesc = get_bpf_sock(ifi->name);
	ifi->rbuf_max = configure_bpf_sock(ifi->bfdesc);
	ifi->rbuf = malloc(ifi->rbuf_max);
	if (ifi->rbuf == NULL)
		fatalx("Can't allocate %lu bytes for bpf input buffer.",
		    (unsigned long)ifi->rbuf_max);
	ifi->rbuf_offset = 0;
	ifi->rbuf_len = 0;

	if (chroot(_PATH_VAREMPTY) == -1)
		fatalx("chroot");
	if (chdir("/") == -1)
		fatalx("chdir(\"/\")");

	if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1)
		fatalx("setresgid");
	if (setgroups(1, &pw->pw_gid) == -1)
		fatalx("setgroups");
	if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
		fatalx("setresuid");

	endpwent();

	if (daemonize != 0) {
		if (pledge("stdio inet dns route proc", NULL) == -1)
			fatalx("pledge");
	} else {
		if (pledge("stdio inet dns route", NULL) == -1)
			fatalx("pledge");
	}

	setproctitle("%s", ifi->name);
	time(&ifi->startup_time);

	if (ifi->linkstat != 0) {
		ifi->state = S_REBOOTING;
		state_reboot(ifi);
	} else {
		ifi->state = S_PREBOOT;
		state_preboot(ifi);
	}

	dispatch(ifi, routefd);

	/* not reached */
	return 0;
}

void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr,
	    "usage: %s [-d | -q] [-u] [-c file] [-i options] [-L file] "
	    "[-l file] interface\n", __progname);
	exit(1);
}

void
state_preboot(struct interface_info *ifi)
{
	static int	 preamble;
	time_t		 cur_time;
	int		 interval;

	time(&cur_time);

	interval = cur_time - ifi->startup_time;

	ifi->linkstat = interface_status(ifi->name);

	if (log_perror != 0 && interval > 3) {
		if (preamble == 0 && ifi->linkstat == 0) {
			fprintf(stderr, "%s: no link ....", ifi->name);
			preamble = 1;
		}
		if (preamble != 0) {
			if (ifi->linkstat != 0)
				fprintf(stderr, " got link\n");
			else if (interval > config->link_timeout)
				fprintf(stderr, " sleeping\n");
			else
				fprintf(stderr, ".");
			fflush(stderr);
		}
	}

	if (ifi->linkstat != 0) {
		ifi->state = S_REBOOTING;
		set_timeout(ifi, 1, state_reboot);
	} else {
		if (interval > config->link_timeout)
			go_daemon();
		ifi->state = S_PREBOOT;
		set_timeout(ifi, 1, state_preboot);
	}
}

/*
 * Called when the interface link becomes active.
 */
void
state_reboot(struct interface_info *ifi)
{
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

	ifi->xid = arc4random();
	make_request(ifi, ifi->active);

	ifi->destination.s_addr = INADDR_BROADCAST;
	time(&ifi->first_sending);
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
	ifi->xid = arc4random();
	make_discover(ifi, ifi->active);

	ifi->destination.s_addr = INADDR_BROADCAST;
	ifi->state = S_SELECTING;
	time(&ifi->first_sending);
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
	struct option_data	*option;

	cancel_timeout(ifi);

	if (ifi->offer == NULL) {
		state_panic(ifi);
		return;
	}

	/* If it was a BOOTREPLY, we can just take the lease right now. */
	if (BOOTP_LEASE(ifi->offer)) {
		/*
		 * Set (unsigned 32 bit) options
		 *
		 * DHO_DHCP_LEASE_TIME (12000 seconds),
		 * DHO_RENEWAL_TIME (8000 seconds)
		 * DHO_REBINDING_TIME (10000 seconds)
		 *
		 * so bind_lease() can set the lease times. Note that the
		 * values must be big-endian.
		 */
		option = &ifi->offer->options[DHO_DHCP_LEASE_TIME];
		option->data = malloc(4);
		if (option->data) {
			option->len = 4;
			memcpy(option->data, "\x00\x00\x2e\xe0", 4);
		}
		option = &ifi->offer->options[DHO_DHCP_RENEWAL_TIME];
		option->data = malloc(4);
		if (option->data) {
			option->len = 4;
			memcpy(option->data, "\x00\x00\x1f\x40", 4);
		}
		option = &ifi->offer->options[DHO_DHCP_REBINDING_TIME];
		option->data = malloc(4);
		if (option->data) {
			option->len = 4;
			memcpy(option->data, "\x00\x00\x27\x10", 4);
		}

		ifi->state = S_REQUESTING;
		bind_lease(ifi);

		return;
	}

	ifi->destination.s_addr = INADDR_BROADCAST;
	ifi->state = S_REQUESTING;
	time(&ifi->first_sending);

	ifi->interval = 0;

	/*
	 * Make a DHCPREQUEST packet from the lease we picked. Keep
	 * the current xid, as all offers should have had the same
	 * one.
	 */
	make_request(ifi, ifi->offer);

	/* Toss the lease we picked - we'll get it back in a DHCPACK. */
	free_client_lease(ifi->offer);

	send_request(ifi);
}

void
dhcpoffer(struct interface_info *ifi, struct option_data *options, char *info)
{
	struct client_lease	*lease;
	time_t			 stop_selecting;

	if (ifi->state != S_SELECTING) {
#ifdef DEBUG
		log_debug("Unexpected %s. State #%d.", info, ifi->state);
#endif	/* DEBUG */
		return;
	}

	log_info("%s", info);

	lease = packet_to_lease(ifi, options);
	if (lease != NULL) {
		if (ifi->offer == NULL) {
			ifi->offer = lease;
		} else if (lease->address.s_addr ==
		    ifi->requested_address.s_addr) {
			free_client_lease(ifi->offer);
			ifi->offer = lease;
		}
		if (ifi->offer != lease) {
			make_decline(ifi, lease);
			send_decline(ifi);
			free_client_lease(lease);
		}
	}

	/* Figure out when we're supposed to stop selecting. */
	stop_selecting = ifi->first_sending + config->select_interval;
	if (stop_selecting <= time(NULL))
		state_selecting(ifi);
	else
		set_timeout(ifi, stop_selecting, state_selecting);
}

void
dhcpack(struct interface_info *ifi, struct option_data *options, char *info)
{
	struct client_lease	*lease;

	if (ifi->state != S_REBOOTING &&
	    ifi->state != S_REQUESTING &&
	    ifi->state != S_RENEWING &&
	    ifi->state != S_REBINDING) {
#ifdef DEBUG
		log_debug("Unexpected %s. State #%d", info, ifi->state);
#endif	/* DEBUG */
		return;
	}

	log_info("%s", info);

	lease = packet_to_lease(ifi, options);
	if (lease == NULL) {
		ifi->state = S_INIT;
		state_init(ifi);
		return;
	}

	ifi->offer = lease;
	memcpy(ifi->offer->ssid, ifi->ssid, sizeof(ifi->offer->ssid));
	ifi->offer->ssid_len = ifi->ssid_len;

	/* Stop resending DHCPREQUEST. */
	cancel_timeout(ifi);

	bind_lease(ifi);
}

void
dhcpnak(struct interface_info *ifi, struct option_data *options, char *info)
{
	if (ifi->state != S_REBOOTING &&
	    ifi->state != S_REQUESTING &&
	    ifi->state != S_RENEWING &&
	    ifi->state != S_REBINDING) {
#ifdef DEBUG
		log_debug("Unexpected %s. State #%d", info, ifi->state);
#endif	/* DEBUG */
		return;
	}

	if (ifi->active == NULL) {
#ifdef DEBUG
		log_debug("Unexpected %s. No active lease.", info);
#endif	/* DEBUG */
		return;
	}

	log_info("%s", info);

	/* XXX Do we really want to remove a NAK'd lease from the database? */
	if (ifi->active->is_static == 0) {
		TAILQ_REMOVE(&ifi->leases, ifi->active, next);
		free_client_lease(ifi->active);
	}

	ifi->active = NULL;

	/* Stop sending DHCPREQUEST packets. */
	cancel_timeout(ifi);

	ifi->state = S_INIT;
	state_init(ifi);
}

void
bind_lease(struct interface_info *ifi)
{
	struct client_lease	*lease, *pl;
	struct proposal		*active_proposal = NULL;
	struct proposal		*offered_proposal = NULL;
	struct proposal		*effective_proposal = NULL;
	time_t			 cur_time;
	int			 seen;

	/*
	 * Clear out any old resolv_conf in case the lease has been here
	 * before (e.g. static lease).
	 */
	free(ifi->offer->resolv_conf);
	ifi->offer->resolv_conf = NULL;

	lease = apply_defaults(ifi->offer);

	set_lease_times(lease);

	ifi->offer->expiry = lease->expiry;
	ifi->offer->renewal = lease->renewal;
	ifi->offer->rebind = lease->rebind;

	/*
	 * A duplicate proposal once we are responsible & S_RENEWING means we
	 * don't need to change the interface, routing table or resolv.conf.
	 */
	if ((ifi->flags & IFI_IN_CHARGE) && ifi->state == S_RENEWING) {
		active_proposal = lease_as_proposal(ifi->active);
		offered_proposal = lease_as_proposal(ifi->offer);
		if (memcmp(active_proposal, offered_proposal,
		    sizeof(*active_proposal)) == 0) {
			ifi->offer->resolv_conf = ifi->active->resolv_conf;
			ifi->active->resolv_conf = NULL;
			ifi->active = ifi->offer;
			ifi->offer = NULL;
			goto newlease;
		}
	}

	/* Replace the old active lease with the accepted offer. */
	ifi->active = ifi->offer;
	ifi->offer = NULL;
	effective_proposal = lease_as_proposal(lease);

	ifi->active->resolv_conf = resolv_conf_contents(ifi->name,
	    effective_proposal->rtsearch,
	    effective_proposal->rtsearch_len,
	    effective_proposal->rtdns,
	    effective_proposal->rtdns_len);

	set_mtu(effective_proposal->inits, effective_proposal->mtu);

	set_address(ifi->name, effective_proposal->ifa,
	    effective_proposal->netmask);

	set_routes(effective_proposal->ifa, effective_proposal->netmask,
	    effective_proposal->rtstatic, effective_proposal->rtstatic_len);

newlease:
	log_info("bound to %s -- renewal in %lld seconds.",
	    inet_ntoa(ifi->active->address),
	    (long long)(ifi->active->renewal - time(NULL)));
	go_daemon();
	rewrite_option_db(ifi->name, ifi->active, lease);
	free_client_lease(lease);
	free(active_proposal);
	free(offered_proposal);
	free(effective_proposal);

	/*
	 * Remove previous dynamic lease(es) for this address, and any expired
	 * dynamic leases.
	 */
	seen = 0;
	time(&cur_time);
	TAILQ_FOREACH_SAFE(lease, &ifi->leases, next, pl) {
		if (lease->is_static != 0)
			break;
		if (ifi->active == NULL)
			continue;
		if (ifi->active->ssid_len != lease->ssid_len)
			continue;
		if (memcmp(ifi->active->ssid, lease->ssid, lease->ssid_len)
		    != 0)
			continue;
		if (ifi->active == lease)
			seen = 1;
		else if (lease->expiry <= cur_time || lease->address.s_addr ==
		    ifi->active->address.s_addr) {
			TAILQ_REMOVE(&ifi->leases, lease, next);
			free_client_lease(lease);
		}
	}
	if (ifi->active->is_static == 0 && seen == 0)
		TAILQ_INSERT_HEAD(&ifi->leases, ifi->active,  next);

	/* Write out new leases file. */
	rewrite_client_leases(ifi);

	ifi->state = S_BOUND;

	/* Set timeout to start the renewal process. */
	set_timeout(ifi, ifi->active->renewal - cur_time, state_bound);
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

	time(&ifi->first_sending);
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
		log_warn("addressinuse: getifaddrs");
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
	char			*pretty, *buf, *name;
	int			 i;

	lease = calloc(1, sizeof(struct client_lease));
	if (lease == NULL) {
		log_warnx("lease declined: no memory for lease.");
		return NULL;
	}

	/* Copy the lease options. */
	for (i = 0; i < DHO_COUNT; i++) {
		if (options[i].len == 0)
			continue;
		name = code_to_name(i);
		if (unknown_ok == 0 && strncmp("option-", name, 7) != 0) {
			log_warnx("lease declined: unknown option %d", i);
			goto decline;
		}
		pretty = pretty_print_option(i, &options[i], 0);
		if (strlen(pretty) == 0)
			continue;
		switch (i) {
		case DHO_DOMAIN_SEARCH:
			/* Must decode the option into text to check names. */
			buf = pretty_print_domain_search(options[i].data,
			    options[i].len);
			if (buf == NULL || res_hnok_list(buf) == 0) {
				log_warnx("Ignoring %s in offer: invalid host "
				    "name(s)", name);
				continue;
			}
			break;
		case DHO_DOMAIN_NAME:
			/*
			 * Allow deviant but historically blessed
			 * practice of supplying multiple domain names
			 * with DHO_DOMAIN_NAME. Thus allowing multiple
			 * entries in the resolv.conf 'search' statement.
			 */
			if (res_hnok_list(pretty) == 0) {
				log_warnx("Ignoring %s in offer: invalid host "
				    "name(s)", name);
				continue;
			}
			break;
		case DHO_HOST_NAME:
		case DHO_NIS_DOMAIN:
			if (res_hnok(pretty) == 0) {
				log_warnx("Ignoring %s in offer: invalid host "
				    "name", name);
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
			name = code_to_name(i);
			log_warnx("lease declined: %s required but missing",
			    name);
			goto decline;
		}
	}

	/*
	 * If this lease is trying to sell us an address we are already
	 * using, decline it.
	 */
	lease->address.s_addr = packet->yiaddr.s_addr;
	memset(ifname, 0, sizeof(ifname));
	if (addressinuse(ifi->name, lease->address, ifname) != 0 &&
	    strncmp(ifname, ifi->name, IF_NAMESIZE) != 0) {
		log_warnx("lease declined: %s already configured on %s",
		    inet_ntoa(lease->address), ifname);
		goto decline;
	}

	/* Save the siaddr (a.k.a. next-server) info. */
	lease->next_server.s_addr = packet->siaddr.s_addr;

	/* If the server name was filled out, copy it. */
	if ((lease->options[DHO_DHCP_OPTION_OVERLOAD].len == 0 ||
	    (lease->options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 2) == 0) &&
	    packet->sname[0]) {
		lease->server_name = malloc(DHCP_SNAME_LEN + 1);
		if (lease->server_name == NULL) {
			log_warnx("lease declined:: no memory for SNAME.");
			goto decline;
		}
		memcpy(lease->server_name, packet->sname, DHCP_SNAME_LEN);
		lease->server_name[DHCP_SNAME_LEN] = '\0';
		if (res_hnok(lease->server_name) == 0) {
			log_warnx("lease declined: invalid host name in SNAME");
			goto decline;
		}
	}

	/* If the file name was filled out, copy it. */
	if ((lease->options[DHO_DHCP_OPTION_OVERLOAD].len == 0 ||
	    (lease->options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 1) == 0) &&
	    packet->file[0]) {
		/* Don't count on the NUL terminator. */
		lease->filename = malloc(DHCP_FILE_LEN + 1);
		if (lease->filename == NULL) {
			log_warnx("lease declined: no memory for filename.");
			goto decline;
		}
		memcpy(lease->filename, packet->file, DHCP_FILE_LEN);
		lease->filename[DHCP_FILE_LEN] = '\0';
	}
	return lease;

decline:
	make_decline(ifi, lease);
	send_decline(ifi);
	free_client_lease(lease);
	return NULL;
}

/*
 * Send out a DHCPDISCOVER packet, and set a timeout to send out another
 * one after the right interval has expired.  If we don't get an offer by
 * the time we reach the panic interval, call the panic function.
 */
void
send_discover(struct interface_info *ifi)
{
	struct dhcp_packet	*packet = &ifi->sent_packet;
	time_t			 cur_time;
	ssize_t			 rslt;
	int			 interval;

	time(&cur_time);

	/* Figure out how long it's been since we started transmitting. */
	interval = cur_time - ifi->first_sending;

	if (interval > config->timeout) {
		state_panic(ifi);
		return;
	}

	/*
	 * If we're supposed to increase the interval, do so.  If it's
	 * currently zero (i.e., we haven't sent any packets yet), set
	 * it to initial_interval; otherwise, add to it a random
	 * number between zero and two times itself.  On average, this
	 * means that it will double with every transmission.
	 */
	if (ifi->interval == 0)
		ifi->interval = config->initial_interval;
	else {
		ifi->interval += arc4random_uniform(2 * ifi->interval);
	}

	/* Don't backoff past cutoff. */
	if (ifi->interval > config->backoff_cutoff)
		ifi->interval = config->backoff_cutoff;

	/*
	 * If the backoff would take us to the panic timeout, just use that
	 * as the interval.
	 */
	if (cur_time + ifi->interval >
	    ifi->first_sending + config->timeout)
		ifi->interval = (ifi->first_sending +
		    config->timeout) - cur_time + 1;

	/* Record the number of seconds since we started sending. */
	if (interval < UINT16_MAX)
		packet->secs = htons(interval);
	else
		packet->secs = htons(UINT16_MAX);
	ifi->secs = packet->secs;

	log_info("DHCPDISCOVER on %s - interval %lld", ifi->name,
	    (long long)ifi->interval);

	rslt = send_packet(ifi, inaddr_any, inaddr_broadcast);
	if (rslt == -1 && errno == EAFNOSUPPORT) {
		log_warnx("dhclient cannot be used on %s", ifi->name);
		quit = INTERNALSIG;
	} else
		set_timeout(ifi, ifi->interval, send_discover);
}

/*
 * Called if we haven't received any offers in a preset amount of time. When
 * this happens, we try to use existing leases that haven't yet expired.
 */
void
state_panic(struct interface_info *ifi)
{
	log_info("No acceptable DHCPOFFERS received.");

	ifi->offer = get_recorded_lease(ifi);
	if (ifi->offer) {
		ifi->state = S_REQUESTING;
		bind_lease(ifi);
		return;
	}

	/*
	 * No leases were available, or what was available didn't work
	 */
	log_info("No working leases in persistent database - sleeping.");
	ifi->state = S_INIT;
	set_timeout(ifi, config->retry_interval, state_init);
	go_daemon();
}

void
send_request(struct interface_info *ifi)
{
	struct sockaddr_in	 destination;
	struct in_addr		 from;
	struct dhcp_packet	*packet = &ifi->sent_packet;
	time_t			 cur_time;
	int			 interval;

	time(&cur_time);

	/* Figure out how long it's been since we started transmitting. */
	interval = cur_time - ifi->first_sending;

	/*
	 * If we're in the INIT-REBOOT state and we've been trying longer
	 * than reboot_timeout, go to INIT state and DISCOVER an address.
	 *
	 * In the INIT-REBOOT state, if we don't get an ACK, it
	 * means either that we're on a network with no DHCP server,
	 * or that our server is down.  In the latter case, assuming
	 * that there is a backup DHCP server, DHCPDISCOVER will get
	 * us a new address, but we could also have successfully
	 * reused our old address.  In the former case, we're hosed
	 * anyway.  This is not a win-prone situation.
	 */
	if (ifi->state == S_REBOOTING && interval >
	    config->reboot_timeout) {
		ifi->state = S_INIT;
		cancel_timeout(ifi);
		state_init(ifi);
		return;
	}

	/*
	 * If the lease has expired, relinquish the address and go back to the
	 * INIT state.
	 */
	if (ifi->state != S_REQUESTING &&
	    cur_time > ifi->active->expiry) {
		if (ifi->active)
			delete_address(ifi->active->address);
		ifi->state = S_INIT;
		state_init(ifi);
		return;
	}

	/* Do the exponential backoff. */
	if (ifi->interval == 0) {
		if (ifi->state == S_REBOOTING)
			ifi->interval = config->reboot_timeout;
		else
			ifi->interval = config->initial_interval;
	} else
		ifi->interval += arc4random_uniform(2 * ifi->interval);

	/* Don't backoff past cutoff. */
	if (ifi->interval > config->backoff_cutoff)
		ifi->interval = config->backoff_cutoff;

	/*
	 * If the backoff would take us to the expiry time, just set the
	 * timeout to the expiry time.
	 */
	if (ifi->state != S_REQUESTING && cur_time + ifi->interval >
	    ifi->active->expiry)
		ifi->interval = ifi->active->expiry - cur_time + 1;

	/*
	 * If the reboot timeout has expired, or the lease rebind time has
	 * elapsed, or if we're not yet bound, broadcast the DHCPREQUEST rather
	 * than unicasting.
	 */
	memset(&destination, 0, sizeof(destination));
	if (ifi->state == S_REQUESTING ||
	    ifi->state == S_REBOOTING ||
	    cur_time > ifi->active->rebind ||
	    interval > config->reboot_timeout)
		destination.sin_addr.s_addr = INADDR_BROADCAST;
	else
		destination.sin_addr.s_addr = ifi->destination.s_addr;

	if (ifi->state != S_REQUESTING)
		from.s_addr = ifi->active->address.s_addr;
	else
		from.s_addr = INADDR_ANY;

	/* Record the number of seconds since we started sending. */
	if (ifi->state == S_REQUESTING)
		packet->secs = ifi->secs;
	else {
		if (interval < UINT16_MAX)
			packet->secs = htons(interval);
		else
			packet->secs = htons(UINT16_MAX);
	}

	log_info("DHCPREQUEST on %s to %s", ifi->name,
	    inet_ntoa(destination.sin_addr));

	send_packet(ifi, from, destination.sin_addr);

	set_timeout(ifi, ifi->interval, send_request);
}

void
send_decline(struct interface_info *ifi)
{
	log_info("DHCPDECLINE on %s", ifi->name);

	send_packet(ifi, inaddr_any, inaddr_broadcast);
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
		fatalx("options do not fit in DHCPDISCOVER packet.");
	ifi->sent_packet_length = DHCP_FIXED_NON_UDP+i+1;
	if (ifi->sent_packet_length < BOOTP_MIN_LEN)
		ifi->sent_packet_length = BOOTP_MIN_LEN;

	packet->op = BOOTREQUEST;
	packet->htype = HTYPE_ETHER ;
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
make_request(struct interface_info *ifi, struct client_lease * lease)
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
		ifi->requested_address = lease->address;
		i = DHO_DHCP_REQUESTED_ADDRESS;
		options[i].data = (char *)&lease->address.s_addr;
		options[i].len = sizeof(in_addr_t);
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
		fatalx("options do not fit in DHCPREQUEST packet.");
	ifi->sent_packet_length = DHCP_FIXED_NON_UDP+i+1;
	if (ifi->sent_packet_length < BOOTP_MIN_LEN)
		ifi->sent_packet_length = BOOTP_MIN_LEN;

	packet->op = BOOTREQUEST;
	packet->htype = HTYPE_ETHER ;
	packet->hlen = ETHER_ADDR_LEN;
	packet->hops = 0;
	packet->xid = ifi->xid;
	packet->secs = 0; /* Filled in by send_request. */
	packet->flags = 0;

	/*
	 * If we own the address we're requesting, put it in ciaddr. Otherwise
	 * set ciaddr to zero.
	 */
	if (ifi->state == S_BOUND ||
	    ifi->state == S_RENEWING ||
	    ifi->state == S_REBINDING)
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
	options[i].len = sizeof(in_addr_t);

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
		fatalx("options do not fit in DHCPDECLINE packet.");
	ifi->sent_packet_length = DHCP_FIXED_NON_UDP+i+1;
	if (ifi->sent_packet_length < BOOTP_MIN_LEN)
		ifi->sent_packet_length = BOOTP_MIN_LEN;

	packet->op = BOOTREQUEST;
	packet->htype = HTYPE_ETHER ;
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
free_client_lease(struct client_lease *lease)
{
	int	 i;

	/* Static leases are forever. */
	if (lease == NULL || lease->is_static)
		return;

	free(lease->server_name);
	free(lease->filename);
	free(lease->resolv_conf);
	for (i = 0; i < DHO_COUNT; i++)
		free(lease->options[i].data);

	free(lease);
}

void
rewrite_client_leases(struct interface_info *ifi)
{
	struct client_lease	*lp;
	char			*leasestr;
	time_t			 cur_time;

	if (leaseFile == NULL)
		fatalx("lease file not open");

	rewind(leaseFile);

	/*
	 * The leases file is kept in chronological order, with the
	 * most recently bound lease last. When the file was read
	 * leases that were not expired were added to the head of the
	 * TAILQ ifi->leases as they were read. Therefore write out
	 * the leases in ifi->leases in reverse order to recreate
	 * the chonological order required.
	 */
	time(&cur_time);
	TAILQ_FOREACH_REVERSE(lp, &ifi->leases, client_lease_tq, next) {
		/* Don't write out static leases from dhclient.conf. */
		if (lp->is_static != 0)
			continue;
		if (lp->expiry <= cur_time)
			continue;
		leasestr = lease_as_string(ifi->name, "lease", lp);
		if (leasestr != NULL)
			fprintf(leaseFile, "%s", leasestr);
		else
			log_warnx("cannot make lease into string");
	}

	fflush(leaseFile);
	ftruncate(fileno(leaseFile), ftello(leaseFile));
	fsync(fileno(leaseFile));
}

void
rewrite_option_db(char *name, struct client_lease *offered,
    struct client_lease *effective)
{
	char	*leasestr;

	if (optionDB == NULL)
		return;

	rewind(optionDB);

	leasestr = lease_as_string(name, "offered", offered);
	if (leasestr != NULL)
		fprintf(optionDB, "%s", leasestr);
	else
		log_warnx("cannot make offered lease into string");

	leasestr = lease_as_string(name, "effective", effective);
	if (leasestr != NULL)
		fprintf(optionDB, "%s", leasestr);
	else
		log_warnx("cannot make effective lease into string");

	fflush(optionDB);
	ftruncate(fileno(optionDB), ftello(optionDB));
	fsync(fileno(optionDB));
}

void
append_statement(char *string, size_t sz, char *s1, char *s2)
{
	strlcat(string, s1, sz);
	strlcat(string, s2, sz);
	strlcat(string, ";\n", sz);
}

struct proposal *
lease_as_proposal(struct client_lease *lease)
{
	struct proposal		*proposal;
	struct option_data	*opt;
	char			*buf;

	proposal = calloc(1, sizeof(*proposal));
	if (proposal == NULL)
		fatal("No memory for lease_as_proposal");

	proposal->ifa = lease->address;
	proposal->addrs |= RTA_IFA;

	opt = &lease->options[DHO_INTERFACE_MTU];
	if (opt->len == sizeof(uint16_t)) {
		memcpy(&proposal->mtu, opt->data, sizeof(proposal->mtu));
		proposal->mtu = ntohs(proposal->mtu);
		proposal->inits |= RTV_MTU;
	}

	opt = &lease->options[DHO_SUBNET_MASK];
	if (opt->len == sizeof(proposal->netmask)) {
		proposal->addrs |= RTA_NETMASK;
		proposal->netmask.s_addr = ((struct in_addr *)opt->data)->s_addr;
	}

	if (lease->options[DHO_CLASSLESS_STATIC_ROUTES].len != 0) {
		opt = &lease->options[DHO_CLASSLESS_STATIC_ROUTES];
		/* XXX */
		if (opt->len < sizeof(proposal->rtstatic)) {
			proposal->rtstatic_len = opt->len;
			memcpy(&proposal->rtstatic, opt->data, opt->len);
			proposal->addrs |= RTA_STATIC;
		} else
			log_warnx("CLASSLESS_STATIC_ROUTES too long");
	} else if (lease->options[DHO_CLASSLESS_MS_STATIC_ROUTES].len != 0) {
		opt = &lease->options[DHO_CLASSLESS_MS_STATIC_ROUTES];
		/* XXX */
		if (opt->len < sizeof(proposal->rtstatic)) {
			proposal->rtstatic_len = opt->len;
			memcpy(&proposal->rtstatic[1], opt->data, opt->len);
			proposal->addrs |= RTA_STATIC;
		} else
			log_warnx("MS_CLASSLESS_STATIC_ROUTES too long");
	} else {
		opt = &lease->options[DHO_ROUTERS];
		if (opt->len >= sizeof(struct in_addr)) {
			proposal->rtstatic_len = 1 + sizeof(struct in_addr);
			proposal->rtstatic[0] = 0;
			memcpy(&proposal->rtstatic[1], opt->data,
			    sizeof(struct in_addr));
			proposal->addrs |= RTA_STATIC;
		}
	}

	if (lease->options[DHO_DOMAIN_SEARCH].len != 0) {
		opt = &lease->options[DHO_DOMAIN_SEARCH];
		buf = pretty_print_domain_search(opt->data, opt->len);
		if (buf == NULL )
			log_warnx("DOMAIN_SEARCH too long");
		else {
			proposal->rtsearch_len = strlen(buf);
			memcpy(proposal->rtsearch, buf, proposal->rtsearch_len);
			proposal->addrs |= RTA_SEARCH;
		}
	} else if (lease->options[DHO_DOMAIN_NAME].len != 0) {
		opt = &lease->options[DHO_DOMAIN_NAME];
		if (opt->len < sizeof(proposal->rtsearch)) {
			proposal->rtsearch_len = opt->len;
			memcpy(proposal->rtsearch, opt->data, opt->len);
			proposal->addrs |= RTA_SEARCH;
		} else
			log_warnx("DOMAIN_NAME too long");
	}
	if (lease->options[DHO_DOMAIN_NAME_SERVERS].len != 0) {
		int servers;
		opt = &lease->options[DHO_DOMAIN_NAME_SERVERS];
		servers = opt->len / sizeof(struct in_addr);
		if (servers > MAXNS)
			servers = MAXNS;
		if (servers > 0) {
			proposal->addrs |= RTA_DNS;
			proposal->rtdns_len = servers * sizeof(struct in_addr);
			memcpy(proposal->rtdns, opt->data, proposal->rtdns_len);
		}
	}

	return proposal;
}

char *
lease_as_string(char *ifname, char *type, struct client_lease *lease)
{
	static char		 string[8192];
	char			 timebuf[27];	/* 6 2017/04/08 05:47:50 UTC; */
	struct option_data	*opt;
	char			*buf, *name;
	size_t			 rslt;
	int			 i;

	memset(string, 0, sizeof(string));

	strlcat(string, type, sizeof(string));
	strlcat(string, " {\n", sizeof(string));
	strlcat(string, BOOTP_LEASE(lease) ? "  bootp;\n" : "", sizeof(string));

	buf = pretty_print_string(ifname, strlen(ifname), 1);
	if (buf == NULL)
		return NULL;
	append_statement(string, sizeof(string), "  interface ", buf);

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
		if (buf == NULL)
			return NULL;
		strlcat(string, "  option ", sizeof(string));
		strlcat(string, name, sizeof(string));
		append_statement(string, sizeof(string), " ", buf);
	}

	rslt = strftime(timebuf, sizeof(timebuf), DB_TIMEFMT,
	    gmtime(&lease->renewal));
	if (rslt == 0)
		return NULL;
	append_statement(string, sizeof(string), "  renew ", timebuf);

	rslt = strftime(timebuf, sizeof(timebuf), DB_TIMEFMT,
	    gmtime(&lease->rebind));
	if (rslt == 0)
		return NULL;
	append_statement(string, sizeof(string), "  rebind ", timebuf);

	rslt = strftime(timebuf, sizeof(timebuf), DB_TIMEFMT,
	    gmtime(&lease->expiry));
	if (rslt == 0)
		return NULL;
	append_statement(string, sizeof(string), "  expire ", timebuf);

	rslt = strlcat(string, "}\n", sizeof(string));
	if (rslt >= sizeof(string))
		return NULL;

	return  string ;
}

void
go_daemon(void)
{
	static int	 state = 0;

	if (daemonize == 0 || state != 0)
		return;

	state = 1;

	/* Stop logging to stderr. */
	log_perror = 0;
	log_init(0, LOG_DAEMON);
	log_setverbose(0);

	if (rdaemon(nullfd) == -1)
		fatal("Cannot daemonize");

	signal(SIGHUP, sighdlr);
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

int
res_hnok(const char *name)
{
	const char	*dn = name;
	int		 pch = '.', ch = (unsigned char)*dn++;
	int		 warn = 0;

	while (ch != '\0') {
		int nch = (unsigned char)*dn++;

		if (ch == '.') {
			;
		} else if (pch == '.' || nch == '.' || nch == '\0') {
			if (isalnum(ch) == 0)
				return 0;
		} else if (isalnum(ch) == 0 && ch != '-' && ch != '_') {
			return 0;
		} else if (ch == '_' && warn == 0) {
			log_warnx("warning: hostname %s contains an "
			    "underscore which violates RFC 952", name);
			warn++;
		}
		pch = ch, ch = nch;
	}
	return 1;
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
		fatalx("Cannot copy domain name list");

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

void
fork_privchld(struct interface_info *ifi, int fd, int fd2)
{
	struct pollfd	 pfd[1];
	struct imsgbuf	*priv_ibuf;
	ssize_t		 n;
	int		 ioctlfd, routefd, nfds, got_imsg_hup = 0;

	switch (fork()) {
	case -1:
		fatalx("cannot fork");
		break;
	case 0:
		break;
	default:
		return;
	}

	if (chdir("/") == -1)
		fatalx("chdir(\"/\")");

	setproctitle("%s [priv]", ifi->name);

	go_daemon();

	close(fd2);

	if ((priv_ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		fatalx("no memory for priv_ibuf");

	imsg_init(priv_ibuf, fd);

	if ((ioctlfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fatal("socket open failed");
	if ((routefd = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		fatal("opening socket to flush routes");

	while (quit == 0) {
		pfd[0].fd = priv_ibuf->fd;
		pfd[0].events = POLLIN;
		if ((nfds = poll(pfd, 1, INFTIM)) == -1) {
			if (errno != EINTR) {
				log_warn("poll error");
				quit = INTERNALSIG;
			}
			continue;
		}

		if (nfds == 0 || (pfd[0].revents & POLLIN) == 0)
			continue;

		if ((n = imsg_read(priv_ibuf)) == -1 && errno != EAGAIN) {
			log_warn("imsg_read(priv_ibuf)");
			quit = INTERNALSIG;
			continue;
		}

		if (n == 0) {
			/* Connection closed - other end should log message. */
			quit = INTERNALSIG;
			continue;
		}

		got_imsg_hup = dispatch_imsg(ifi->name, ifi->rdomain, ioctlfd,
		    routefd, priv_ibuf);
		if (got_imsg_hup != 0)
			quit = SIGHUP;
	}
	close(routefd);
	close(ioctlfd);

	imsg_clear(priv_ibuf);
	close(fd);

	if (quit == SIGHUP) {
		if (got_imsg_hup == 0)
			log_warnx("%s; restarting.", strsignal(quit));
		signal(SIGHUP, SIG_IGN); /* will be restored after exec */
		execvp(saved_argv[0], saved_argv);
		fatal("RESTART FAILED: '%s'", saved_argv[0]);
	}

	if (quit != INTERNALSIG)
		fatalx("%s", strsignal(quit));

	exit(1);
}

void
get_ifname(struct interface_info *ifi, int ioctlfd, char *arg)
{
	struct ifgroupreq	 ifgr;
	struct ifg_req		*ifg;
	unsigned int		 len;

	if (strcmp(arg, "egress") == 0) {
		memset(&ifgr, 0, sizeof(ifgr));
		strlcpy(ifgr.ifgr_name, "egress", sizeof(ifgr.ifgr_name));
		if (ioctl(ioctlfd, SIOCGIFGMEMB, (caddr_t)&ifgr) == -1) {
			if (errno == ENOENT)
				fatalx("no interface in group egress found");
			fatal("ioctl SIOCGIFGMEMB");
		}
		len = ifgr.ifgr_len;
		if ((ifgr.ifgr_groups = calloc(1, len)) == NULL)
			fatalx("get_ifname");
		if (ioctl(ioctlfd, SIOCGIFGMEMB, (caddr_t)&ifgr) == -1)
			fatal("ioctl SIOCGIFGMEMB");

		arg = NULL;
		for (ifg = ifgr.ifgr_groups;
		    ifg && len >= sizeof(struct ifg_req); ifg++) {
			len -= sizeof(struct ifg_req);
			if (arg != NULL)
				fatalx("too many interfaces in group egress");
			arg = ifg->ifgrq_member;
		}

		if (strlcpy(ifi->name, arg, IFNAMSIZ) >= IFNAMSIZ)
			fatal("Interface name too long");

		free(ifgr.ifgr_groups);
	} else if (strlcpy(ifi->name, arg, IFNAMSIZ) >= IFNAMSIZ)
		fatalx("Interface name too long");
}

struct client_lease *
apply_defaults(struct client_lease *lease)
{
	struct client_lease	*newlease;
	int			 i, j;

	newlease = clone_lease(lease);
	if (newlease == NULL)
		fatalx("Unable to clone lease");

	if (config->filename != NULL) {
		free(newlease->filename);
		newlease->filename = strdup(config->filename);
	}
	if (config->server_name != NULL) {
		free(newlease->server_name);
		newlease->server_name = strdup(config->server_name);
	}
	if (config->address.s_addr != INADDR_ANY)
		newlease->address.s_addr = config->address.s_addr;
	if (config->next_server.s_addr != INADDR_ANY)
		newlease->next_server.s_addr = config->next_server.s_addr;

	for (i = 0; i < DHO_COUNT; i++) {
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

	if (newlease->options[DHO_STATIC_ROUTES].len != 0) {
		log_warnx("DHO_STATIC_ROUTES (option 33) not supported");
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

cleanup:
	if (newlease != NULL) {
		newlease->is_static = 0;
		free_client_lease(newlease);
	}

	fatalx("Unable to apply defaults");
	/* NOTREACHED */

	return NULL;
}

struct client_lease *
clone_lease(struct client_lease *oldlease)
{
	struct client_lease	*newlease;
	int			 i;

	newlease = calloc(1, sizeof(struct client_lease));
	if (newlease == NULL)
		goto cleanup;

	newlease->expiry = oldlease->expiry;
	newlease->renewal = oldlease->renewal;
	newlease->rebind = oldlease->rebind;
	newlease->is_static = oldlease->is_static;
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
	if (oldlease->resolv_conf != NULL) {
		newlease->resolv_conf = strdup(oldlease->resolv_conf);
		if (newlease->resolv_conf == NULL)
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
	if (newlease != NULL) {
		newlease->is_static = 0;
		free_client_lease(newlease);
	}

	return NULL;
}

/*
 * Apply the list of options to be ignored that was provided on the
 * command line. This will override any ignore list obtained from
 * dhclient.conf.
 */
void
apply_ignore_list(char *ignore_list)
{
	uint8_t		 list[DHO_COUNT];
	char		*p;
	int		 ix, i, j;

	memset(list, 0, sizeof(list));
	ix = 0;

	for (p = strsep(&ignore_list, ", "); p != NULL;
	    p = strsep(&ignore_list, ", ")) {
		if (*p == '\0')
			continue;

		i = name_to_code(p);
		if (i == DHO_END) {
			log_info("Invalid option name: '%s'", p);
			return;
		}

		/* Avoid storing duplicate options in the list. */
		for (j = 0; j < ix && list[j] != i; j++)
			;
		if (j == ix)
			list[ix++] = i;
	}

	config->ignored_option_count = ix;
	memcpy(config->ignored_options, list, sizeof(config->ignored_options));
}

void
set_lease_times(struct client_lease *lease)
{
	time_t		 cur_time, time_max;
	uint32_t	 uint32val;

	time(&cur_time);

	time_max = LLONG_MAX - cur_time;
	if (time_max > UINT32_MAX)
		time_max = UINT32_MAX;

	/*
	 * Take the server-provided times if available.  Otherwise
	 * figure them out according to the spec.
	 *
	 * expiry  == time to discard lease.
	 * renewal == time to renew lease from server that provided it.
	 * rebind  == time to renew lease from any server.
	 *
	 * 0 <= renewal <= rebind <= expiry <= time_max
	 * &&
	 * expiry >= MIN(time_max, 60)
	 */

	lease->expiry = 43200;	/* Default to 12 hours */
	if (lease->options[DHO_DHCP_LEASE_TIME].len == sizeof(uint32val)) {
		memcpy(&uint32val, lease->options[DHO_DHCP_LEASE_TIME].data,
		    sizeof(uint32val));
		lease->expiry = ntohl(uint32val);
		if (lease->expiry < 60)
			lease->expiry = 60;
	}
	if (lease->expiry > time_max)
		lease->expiry = time_max;

	lease->renewal = lease->expiry / 2;
	if (lease->options[DHO_DHCP_RENEWAL_TIME].len == sizeof(uint32val)) {
		memcpy(&uint32val, lease->options[DHO_DHCP_RENEWAL_TIME].data,
		    sizeof(uint32val));
		lease->renewal = ntohl(uint32val);
		if (lease->renewal > lease->expiry)
			lease->renewal = lease->expiry;
	}

	lease->rebind = (lease->expiry * 7) / 8;
	if (lease->options[DHO_DHCP_REBINDING_TIME].len == sizeof(uint32val)) {
		memcpy(&uint32val,
		    lease->options[DHO_DHCP_REBINDING_TIME].data,
		    sizeof(uint32val));
		lease->rebind = ntohl(uint32val);
		if (lease->rebind > lease->expiry)
			lease->rebind = lease->expiry;
	}
	if (lease->rebind < lease->renewal)
		lease->rebind = lease->renewal;

	/* Convert lease lengths to times. */
	lease->expiry += cur_time;
	lease->renewal += cur_time;
	lease->rebind += cur_time;
}

void
take_charge(struct interface_info *ifi, int routefd)
{
	struct pollfd		 fds[1];
	struct rt_msghdr	 rtm;
	time_t			 start_time, cur_time;
	int			 retries;

	if (time(&start_time) == -1)
		fatal("time");

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
	rtm.rtm_seq = ifi->xid = arc4random();
	rtm.rtm_priority = RTP_PROPOSAL_DHCLIENT;
	rtm.rtm_addrs = 0;
	rtm.rtm_flags = RTF_UP | RTF_PROTO3;

	retries = 0;
	while ((ifi->flags & IFI_IN_CHARGE) == 0) {
		if (write(routefd, &rtm, sizeof(rtm)) == -1)
			fatal("tried to take charge");
		time(&cur_time);
		if ((cur_time - start_time) > 3) {
			if (++retries <= 3) {
				if (time(&start_time) == -1)
					fatal("time");
			} else {
				fatalx("failed to take charge of %s",
				    ifi->name);
			}
		}
		fds[0].fd = routefd;
		fds[0].events = POLLIN;
		if (poll(fds, 1, 3) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("routefd poll");
		}
		if ((fds[0].revents & (POLLIN | POLLHUP)) != 0)
			routehandler(ifi, routefd);
	}
}

struct client_lease *
get_recorded_lease(struct interface_info *ifi)
{
	char			 ifname[IF_NAMESIZE];
	time_t			 cur_time;
	struct client_lease	*lp;
	int			 i;

	time(&cur_time);

	/* Run through the list of leases and see if one can be used. */
	i = DHO_DHCP_CLIENT_IDENTIFIER;
	TAILQ_FOREACH(lp, &ifi->leases, next) {
		if (lp->ssid_len != ifi->ssid_len)
			continue;
		if (memcmp(lp->ssid, ifi->ssid, lp->ssid_len) != 0)
			continue;
		if ((lp->options[i].len != 0) && ((lp->options[i].len !=
		    config->send_options[i].len) ||
		    memcmp(lp->options[i].data, config->send_options[i].data,
		    lp->options[i].len)))
			continue;
		if (addressinuse(ifi->name, lp->address, ifname) != 0 &&
		    strncmp(ifname, ifi->name, IF_NAMESIZE) != 0)
			continue;
		if (lp->is_static == 0 && lp->expiry <= cur_time)
			continue;

		if (lp->is_static != 0)
			set_lease_times(lp);
		break;
	}

	return lp;
}

void
set_default_client_identifier(struct interface_info *ifi)
{
	struct option_data	*opt;

	/*
	 * Check both len && data so
	 *
	 *     send dhcp-client-identifier "";
	 *
	 * can be used to suppress sending the default client
	 * identifier.
	 */
	opt = &config->send_options[DHO_DHCP_CLIENT_IDENTIFIER];
	if (opt->len == 0 && opt->data == NULL) {
		opt->data = calloc(1, ETHER_ADDR_LEN + 1);
		if (opt->data == NULL)
			fatalx("no memory for default client identifier");
		opt->data[0] = HTYPE_ETHER;
		memcpy(&opt->data[1], ifi->hw_address.ether_addr_octet,
		    ETHER_ADDR_LEN);
		opt->len = ETHER_ADDR_LEN + 1;
	}
}
