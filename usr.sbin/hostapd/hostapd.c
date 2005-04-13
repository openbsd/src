/*	$OpenBSD: hostapd.c,v 1.3 2005/04/13 18:55:00 deraadt Exp $	*/

/*
 * Copyright (c) 2004, 2005 Reyk Floeter <reyk@vantronix.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "hostapd.h"

void	 hostapd_usage(void);
void	 hostapd_udp_init(struct hostapd_config *);
void	 hostapd_sig_handler(int);

struct hostapd_config hostapd_cfg;

extern char *__progname;

/* defined in event(3) to terminate the event loop */
extern volatile sig_atomic_t event_gotterm;

void
hostapd_usage(void)
{
	fprintf(stderr, "usage: %s [-dvb] [-a interface] [-D macro=value] "
	    "[-f file] [-i interface]\n",
	    __progname);
	exit(EXIT_FAILURE);
}

void
hostapd_log(u_int level, const char *fmt, ...)
{
	va_list ap;

	if (level > hostapd_cfg.c_verbose)
		return;

	va_start(ap, fmt);
	if (hostapd_cfg.c_debug)
		vfprintf(stderr, fmt, ap);
	else
		vsyslog(LOG_INFO, fmt, ap);
	fflush(stderr);
	va_end(ap);
}

void
hostapd_fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (hostapd_cfg.c_debug)
		vfprintf(stderr, fmt, ap);
	else
		vsyslog(LOG_ERR, fmt, ap);
	fflush(stderr);
	va_end(ap);

	hostapd_cleanup(&hostapd_cfg);

	exit(EXIT_FAILURE);
}

int
hostapd_check_file_secrecy(int fd, const char *fname)
{
	struct stat st;

	if (fstat(fd, &st)) {
		hostapd_log(HOSTAPD_LOG,
		    "cannot stat %s\n", fname);
		return (-1);
	}

	if (st.st_uid != 0 && st.st_uid != getuid()) {
		hostapd_log(HOSTAPD_LOG,
		    "%s: owner not root or current user\n", fname);
		return (-1);
	}

	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		hostapd_log(HOSTAPD_LOG,
		    "%s: group/world readable/writeable\n", fname);
		return (-1);
	}

	return (0);
}

int
hostapd_bpf_open(u_int flags)
{
	u_int i;
	int fd = -1;
	char *dev;
	struct bpf_version bpv;

	/*
	 * Try to open the next available BPF device
	 */
	for (i = 0; i < 255; i++) {
		if (asprintf(&dev, "/dev/bpf%u", i) == -1)
			hostapd_fatal("failed to allocate buffer\n");

		if ((fd = open(dev, flags)) != -1)
			break;

		free(dev);
	}

	if (fd == -1) {
		free(dev);
		hostapd_fatal("unable to open BPF device\n");
	}

	free(dev);

	/*
	 * Get and validate the BPF version
	 */

	if (ioctl(fd, BIOCVERSION, &bpv) == -1)
		hostapd_fatal("failed to get BPF version: %s\n",
		    strerror(errno));

	if (bpv.bv_major != BPF_MAJOR_VERSION ||
	    bpv.bv_minor < BPF_MINOR_VERSION)
		hostapd_fatal("invalid BPF version\n");

	return (fd);
}

void
hostapd_udp_init(struct hostapd_config *cfg)
{
	struct ifreq ifr;
	struct sockaddr_in *addr, baddr;
	struct ip_mreq mreq;
	int brd = 1;

	bzero(&ifr, sizeof(ifr));

	/*
	 * Open a listening UDP socket
	 */

	if ((cfg->c_iapp_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		hostapd_fatal("unable to open udp socket\n");

	cfg->c_flags |= HOSTAPD_CFG_F_UDP;

	strlcpy(ifr.ifr_name, cfg->c_iapp_iface, sizeof(ifr.ifr_name));

	if (ioctl(cfg->c_iapp_udp, SIOCGIFADDR, &ifr) == -1)
		hostapd_fatal("UDP ioctl %s on \"%s\" failed: %s\n",
		    "SIOCGIFADDR", ifr.ifr_name, strerror(errno));

	addr = (struct sockaddr_in *)&ifr.ifr_addr;
	cfg->c_iapp_addr.sin_family = AF_INET;
	cfg->c_iapp_addr.sin_addr.s_addr = addr->sin_addr.s_addr;
	cfg->c_iapp_addr.sin_port = htons(IAPP_PORT);

	if (ioctl(cfg->c_iapp_udp, SIOCGIFBRDADDR, &ifr) == -1)
		hostapd_fatal("UDP ioctl %s on \"%s\" failed: %s\n",
		    "SIOCGIFBRDADDR", ifr.ifr_name, strerror(errno));

	addr = (struct sockaddr_in *)&ifr.ifr_addr;
	cfg->c_iapp_broadcast.sin_family = AF_INET;
	cfg->c_iapp_broadcast.sin_addr.s_addr = addr->sin_addr.s_addr;
	cfg->c_iapp_broadcast.sin_port = htons(IAPP_PORT);

	baddr.sin_family = AF_INET;
	baddr.sin_addr.s_addr = htonl(INADDR_ANY);
	baddr.sin_port = htons(IAPP_PORT);

	if (bind(cfg->c_iapp_udp, (struct sockaddr *)&baddr,
	    sizeof(baddr)) == -1)
		hostapd_fatal("failed to bind UDP socket: %s\n",
		    strerror(errno));

	/*
	 * The revised 802.11F standard requires IAPP messages to be
	 * send via multicast to the group 224.0.1.178. Nevertheless,
	 * some implementations still use broadcasts for IAPP
	 * messages.
	 */
	if (cfg->c_flags & HOSTAPD_CFG_F_BRDCAST) {
		/*
		 * Enable broadcast
		 */

		hostapd_log(HOSTAPD_LOG_DEBUG, "using broadcast mode\n");

		if (setsockopt(cfg->c_iapp_udp, SOL_SOCKET, SO_BROADCAST,
		    &brd, sizeof(brd)) == -1)
			hostapd_fatal("failed to enable broadcast on socket\n");
	} else {
		/*
		 * Enable multicast
		 */

		hostapd_log(HOSTAPD_LOG_DEBUG, "using multicast mode\n");

		bzero(&mreq, sizeof(mreq));

		cfg->c_iapp_multicast.sin_family = AF_INET;
		cfg->c_iapp_multicast.sin_addr.s_addr =
		    inet_addr(IAPP_MCASTADDR);
		cfg->c_iapp_multicast.sin_port = htons(IAPP_PORT);

		mreq.imr_multiaddr.s_addr =
		    cfg->c_iapp_multicast.sin_addr.s_addr;
		mreq.imr_interface.s_addr =
		    cfg->c_iapp_addr.sin_addr.s_addr;

		if (setsockopt(cfg->c_iapp_udp, IPPROTO_IP,
		    IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
			hostapd_fatal("failed to add multicast membership to "
			    "%s: %s\n", IAPP_MCASTADDR, strerror(errno));
	}
}

void
hostapd_sig_handler(int sig)
{
	switch (sig) {
	case SIGALRM:
	case SIGTERM:
	case SIGQUIT:
	case SIGINT:
		/* This will terminate libevent's main loop */
		event_gotterm = 1;
	}
}

void
hostapd_cleanup(struct hostapd_config *cfg)
{
	struct ip_mreq mreq;

	if (cfg->c_flags & HOSTAPD_CFG_F_PRIV &&
	    (cfg->c_flags & HOSTAPD_CFG_F_BRDCAST) == 0 &&
	    cfg->c_apme_n == 0) {
		/*
		 * Disable multicast and let the kernel unsubscribe
		 * from the multicast group.
		 */

		bzero(&mreq, sizeof(mreq));

		mreq.imr_multiaddr.s_addr =
		    inet_addr(IAPP_MCASTADDR);
		mreq.imr_interface.s_addr =
		    cfg->c_iapp_addr.sin_addr.s_addr;

		if (setsockopt(cfg->c_iapp_udp, IPPROTO_IP,
		    IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
			hostapd_log(HOSTAPD_LOG, "failed to remove multicast"
			    " membership to %s: %s\n",
			    IAPP_MCASTADDR, strerror(errno));
	}

	if ((cfg->c_flags & HOSTAPD_CFG_F_PRIV) == 0 &&
	    cfg->c_flags & HOSTAPD_CFG_F_APME) {
		/* Shutdown the Host AP protocol handler */
		hostapd_iapp_term(&hostapd_cfg);
	}

	hostapd_log(HOSTAPD_LOG_VERBOSE, "bye!\n");

	if (!cfg->c_debug)
		closelog();

	/* Close all open file descriptors and sockets */
	closefrom(0);
}

int
main(int argc, char *argv[])
{
	struct hostapd_config *cfg = &hostapd_cfg;
	char *iapp_iface = NULL, *hostap_iface = NULL, *config = NULL;
	int ch;

	/*
	 * Get and parse command line options
	 */
	while ((ch = getopt(argc, argv, "a:bf:D:di:v")) != -1) {
		switch (ch) {
		case 'a':
			hostap_iface = optarg;
			cfg->c_flags |= HOSTAPD_CFG_F_APME;
			break;
		case 'b':
			cfg->c_flags |= HOSTAPD_CFG_F_BRDCAST;
			break;
		case 'f':
			config = optarg;
			break;
		case 'D':
			if (hostapd_parse_symset(optarg) < 0)
				hostapd_fatal("could not parse macro "
				    "definition %s", optarg);
			break;
		case 'd':
			cfg->c_debug++;
			break;
		case 'i':
			iapp_iface = optarg;
			cfg->c_flags |= HOSTAPD_CFG_F_IAPP;
			break;
		case 'v':
			cfg->c_verbose++;
			break;
		default:
			hostapd_usage();
		}
	}

	if (config == NULL)
		strlcpy(cfg->c_config, HOSTAPD_CONFIG, sizeof(cfg->c_config));
	else
		strlcpy(cfg->c_config, config, sizeof(cfg->c_config));

	if (iapp_iface != NULL)
		strlcpy(cfg->c_iapp_iface, iapp_iface,
		    sizeof(cfg->c_iapp_iface));

	if (hostap_iface != NULL)
		strlcpy(cfg->c_apme_iface, hostap_iface,
		    sizeof(cfg->c_apme_iface));

	if (!cfg->c_debug) {
		openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);
		daemon(0, 0);
	}

	/* Parse the configuration file */
	hostapd_parse_file(cfg);

	if ((cfg->c_flags & HOSTAPD_CFG_F_IAPP) == 0)
		hostapd_usage();

	if ((cfg->c_flags & HOSTAPD_CFG_F_APME) == 0)
		strlcpy(cfg->c_apme_iface, "<none>", sizeof(cfg->c_apme_iface));

	/*
	 * Setup the hostapd handlers
	 */
	hostapd_udp_init(cfg);
	hostapd_llc_init(cfg);

	if (cfg->c_flags & HOSTAPD_CFG_F_APME)
		hostapd_apme_init(cfg);
	else
		hostapd_log(HOSTAPD_LOG, "%s/%s: running without a Host AP\n",
		    cfg->c_apme_iface, cfg->c_iapp_iface);

	/* Drop all privileges in an unprivileged child process */
	hostapd_priv_init(cfg);

	setproctitle("Host AP: %s, IAPP: %s",
	    cfg->c_apme_iface, cfg->c_iapp_iface);

	/*
	 * Unprivileged child process
	 */	

	event_init();

	/*
	 * Set signal handlers
	 */
	signal(SIGALRM, hostapd_sig_handler);
	signal(SIGTERM, hostapd_sig_handler);
	signal(SIGQUIT, hostapd_sig_handler);
	signal(SIGINT, hostapd_sig_handler);
	signal(SIGHUP, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	/* Initialize the IAPP protcol handler */
	hostapd_iapp_init(cfg);

	/*
	 * Schedule the Host AP listener
	 */
	if (cfg->c_flags & HOSTAPD_CFG_F_APME) {
		event_set(&cfg->c_apme_ev, cfg->c_apme_raw,
		    EV_READ | EV_PERSIST, hostapd_apme_input, cfg);
		event_add(&cfg->c_apme_ev, NULL);
	}

	/*
	 * Schedule the IAPP listener
	 */
	event_set(&cfg->c_iapp_udp_ev, cfg->c_iapp_udp, EV_READ | EV_PERSIST,
	    hostapd_iapp_input, cfg);
	event_add(&cfg->c_iapp_udp_ev, NULL);

	hostapd_log(HOSTAPD_LOG, "%s/%s: starting hostapd with pid %u\n",
	    cfg->c_apme_iface, cfg->c_iapp_iface, getpid());

	/* Run event loop */
	event_dispatch();

	/* Executed after the event loop has been terminated */
	hostapd_cleanup(cfg);

	return (EXIT_SUCCESS);
}
