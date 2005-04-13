/*	$OpenBSD: privsep.c,v 1.5 2005/04/13 20:09:49 moritz Exp $	*/

/*
 * Copyright (c) 2004, 2005 Reyk Floeter <reyk@vantronix.net>
 * Copyright (c) 1995, 1999 Theo de Raadt
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

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <dev/ic/if_wi_ieee.h>
#include <dev/ic/if_wireg.h>
#include <dev/ic/if_wi_hostap.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hostapd.h"

enum hostapd_cmd_types {
	PRIV_APME_BSSID,	/* Get the Host AP's BSSID */
	PRIV_APME_GETNODE,	/* Get a node from the Host AP */
	PRIV_APME_DELNODE,	/* Delete a node from the Host AP */
	PRIV_LLC_SEND_XID	/* Send IEEE 802.3 LLC XID frame */
};

void	 hostapd_priv(int, short, void *);
void	 hostapd_sig_relay(int);
void	 hostapd_sig_chld(int);
int	 hostapd_may_read(int, void *, size_t);
void	 hostapd_must_read(int, void *, size_t);
void	 hostapd_must_write(int, void *, size_t);

static int priv_fd = -1;
static volatile pid_t child_pid = -1;

/* defined in event(3) to terminate the event loop */
extern volatile sig_atomic_t event_gotterm;

/*
 * Main privsep functions
 */

void
hostapd_priv_init(struct hostapd_config *cfg)
{
	int i, socks[2];
	struct passwd *pw;
	struct servent *se;

	for (i = 1; i < _NSIG; i++)
		signal(i, SIG_DFL);

	if ((se = getservbyname("iapp", "udp")) == NULL) {
		cfg->c_iapp_udp_port = IAPP_PORT;
	} else
		cfg->c_iapp_udp_port = se->s_port;

	if ((pw = getpwnam(HOSTAPD_USER)) == NULL)
		hostapd_fatal("failed to get user \"%s\"\n", HOSTAPD_USER);

	endpwent();
	endservent();

	/* Create sockets */
	if (socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, socks) == -1)
		hostapd_fatal("failed to get socket pair");

	if ((child_pid = fork()) < 0)
		hostapd_fatal("failed to fork child process");

	/*
	 * Unprivileged child process
	 */
	if (child_pid == 0) {
		cfg->c_flags &= ~HOSTAPD_CFG_F_PRIV;

		/*
		 * Change the child's root directory to the unprivileged
		 * user's home directory
		 */
		if (chroot(pw->pw_dir) == -1)
			hostapd_fatal("failed to change root directory\n");
		if (chdir("/") == -1)
			hostapd_fatal("failed to change directory");

		/*
		 * Drop privileges and clear the group access list
		 */
		if (setgroups(1, &pw->pw_gid) == -1 ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1 ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1)
			hostapd_fatal("can't drop privileges");

		close(socks[0]);
		priv_fd = socks[1];
		return;
	}

	/*
	 * Privileged mother process
	 */
	cfg->c_flags |= HOSTAPD_CFG_F_PRIV;

	event_init();

	/* Pass ALR/TERM/HUP through to child, and accept CHLD */
	signal(SIGALRM, hostapd_sig_relay);
	signal(SIGTERM, hostapd_sig_relay);
	signal(SIGINT, hostapd_sig_relay);
	signal(SIGHUP, hostapd_sig_relay);
	signal(SIGCHLD, hostapd_sig_chld);

	close(socks[1]);

	if (cfg->c_flags & HOSTAPD_CFG_F_APME) {
		if ((cfg->c_apme = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
			hostapd_fatal("unable to open ioctl socket\n");
		}
	}

	setproctitle("[priv]");

	/* Start a new event listener */
	event_set(&cfg->c_priv_ev, socks[0], EV_READ, hostapd_priv, cfg);
	event_add(&cfg->c_priv_ev, NULL);

	/* Run privileged event loop */
	event_dispatch();

	/* Executed after the event loop has been terminated */
	hostapd_cleanup(cfg);
	_exit(EXIT_SUCCESS);
}

void
hostapd_priv(int fd, short sig, void *arg)
{
	struct hostapd_config *cfg = (struct hostapd_config *)arg;
	struct hostapd_node node;
	struct ieee80211_bssid bssid;
	struct hostap_sta sta;
	struct ifreq ifr;
	int ret, cmd;

	/* Terminate the event if we got an invalid signal */
	if (sig != EV_READ)
		return;

	bzero(&node, sizeof(struct hostapd_node));
	bzero(&sta, sizeof(struct hostap_sta));

	/* Get privsep command */
	if (hostapd_may_read(fd, &cmd, sizeof(int)))
		return;

	switch (cmd) {
	case PRIV_APME_BSSID:
		hostapd_log(HOSTAPD_LOG_DEBUG,
		    "[priv]: msg PRIV_APME_BSSID received\n");

		strlcpy(bssid.i_name, cfg->c_apme_iface, sizeof(bssid.i_name));

		/* Try to get the APME's BSSID */
		if (cfg->c_flags & HOSTAPD_CFG_F_APME) {
			if ((ret = ioctl(cfg->c_apme,
			    SIOCG80211BSSID, &bssid)) != 0)
				ret = errno;
		} else
			ret = ENXIO;

		hostapd_must_write(fd, &ret, sizeof(int));
		if (ret == 0) {
			hostapd_must_write(fd, &bssid.i_bssid,
			    IEEE80211_ADDR_LEN);
		}
		break;

	case PRIV_APME_GETNODE:
		hostapd_log(HOSTAPD_LOG_DEBUG,
		    "[priv]: msg PRIV_APME_GETNODE received\n");

		hostapd_must_read(fd, &node, sizeof(struct hostapd_node));
		bcopy(node.ni_macaddr, sta.addr, IEEE80211_ADDR_LEN);

		strlcpy(ifr.ifr_name, cfg->c_apme_iface, sizeof(ifr.ifr_name));
		ifr.ifr_data = (caddr_t)&sta;

		/* Try to get a station from the APME */
		if (cfg->c_flags & HOSTAPD_CFG_F_APME) {
			if ((ret = ioctl(cfg->c_apme,
			    SIOCHOSTAP_GET, &ifr)) != 0)
				ret = errno;
		} else
			ret = ENXIO;

		hostapd_must_write(fd, &ret, sizeof(int));
		if (ret == 0) {
			node.ni_associd = sta.asid;
			node.ni_flags = sta.flags;
			node.ni_rssi = sta.sig_info;
			node.ni_capinfo = sta.capinfo;

			hostapd_must_write(fd, &node,
			    sizeof(struct hostapd_node));
		}
		break;

	case PRIV_APME_DELNODE:
		hostapd_log(HOSTAPD_LOG_DEBUG,
		    "[priv]: msg PRIV_APME_DELNODE received\n");

		hostapd_must_read(fd, &node, sizeof(struct hostapd_node));
		bcopy(node.ni_macaddr, sta.addr, IEEE80211_ADDR_LEN);

		/* Try to delete a station from the APME */
		if (cfg->c_flags & HOSTAPD_CFG_F_APME) {
			if ((ret = ioctl(cfg->c_apme,
			    SIOCHOSTAP_DEL, &sta)) != 0)
				ret = errno;
		} else
			ret = ENXIO;

		hostapd_must_write(fd, &ret, sizeof(int));
		break;

	case PRIV_LLC_SEND_XID:
		hostapd_log(HOSTAPD_LOG_DEBUG,
		    "[priv]: msg PRIV_LLC_SEND_XID received\n");

		hostapd_must_read(fd, &node, sizeof(struct hostapd_node));

		/* Send a LLC XID frame to reset possible switch ports */
		ret = hostapd_llc_send_xid(cfg, &node);

		hostapd_must_write(fd, &ret, sizeof(int));
		break;

	default:
		hostapd_fatal("[priv]: unknown command %d\n", cmd);
	}

	event_add(&cfg->c_priv_ev, NULL);
}

/*
 * Unprivileged callers
 */

int
hostapd_priv_apme_getnode(struct hostapd_config *cfg, struct hostapd_node *node)
{
	int ret, cmd;

	if (priv_fd < 0)
		hostapd_fatal("%s: called from privileged portion\n", __func__);

	if ((cfg->c_flags & HOSTAPD_CFG_F_APME) == 0)
		hostapd_fatal("%s: Host AP is not available\n", __func__);

	cmd = PRIV_APME_GETNODE;
	hostapd_must_write(priv_fd, &cmd, sizeof(int));
	hostapd_must_write(priv_fd, node, sizeof(struct hostapd_node));

	hostapd_must_read(priv_fd, &ret, sizeof(int));
	if (ret != 0)
		return (ret);

	hostapd_must_read(priv_fd, node, sizeof(struct hostapd_node));

	cfg->c_stats.cn_tx_apme++;

	return (ret);
}

int
hostapd_priv_apme_delnode(struct hostapd_config *cfg, struct hostapd_node *node)
{
	int ret, cmd;

	if (priv_fd < 0)
		hostapd_fatal("%s: called from privileged portion\n", __func__);

	if ((cfg->c_flags & HOSTAPD_CFG_F_APME) == 0)
		hostapd_fatal("%s: Host AP is not available\n", __func__);

	cmd = PRIV_APME_DELNODE;
	hostapd_must_write(priv_fd, &cmd, sizeof(int));
	hostapd_must_write(priv_fd, node, sizeof(struct hostapd_node));

	hostapd_must_read(priv_fd, &ret, sizeof(int));

	if (ret == 0)
		cfg->c_stats.cn_tx_apme++;

	return (ret);
}

void
hostapd_priv_apme_bssid(struct hostapd_config *cfg)
{
	int ret, cmd;

	if (priv_fd < 0)
		hostapd_fatal("%s: called from privileged portion\n", __func__);

	if ((cfg->c_flags & HOSTAPD_CFG_F_APME) == 0)
		hostapd_fatal("%s: Host AP is not available\n", __func__);

	cmd = PRIV_APME_BSSID;
	hostapd_must_write(priv_fd, &cmd, sizeof(int));

	hostapd_must_read(priv_fd, &ret, sizeof(int));
	if (ret != 0)
		hostapd_fatal("%s: failed to get Host AP's BSSID on"
		    " \"%s\": %s\n",
		    cfg->c_apme_iface, strerror(errno));

	hostapd_must_read(priv_fd, &cfg->c_apme_bssid, IEEE80211_ADDR_LEN);

	cfg->c_stats.cn_tx_apme++;
}

int
hostapd_priv_llc_xid(struct hostapd_config *cfg, struct hostapd_node *node)
{
	int ret, cmd;

	if (priv_fd < 0)
		hostapd_fatal("%s: called from privileged portion\n", __func__);

	cmd = PRIV_LLC_SEND_XID;
	hostapd_must_write(priv_fd, &cmd, sizeof(int));
	hostapd_must_write(priv_fd, node, sizeof(struct hostapd_node));
	hostapd_must_read(priv_fd, &ret, sizeof(int));

	if (ret == 0)
		cfg->c_stats.cn_tx_llc++;

	return (ret);
}

/*
 * Signal handlers
 */

void
hostapd_sig_relay(int sig)
{
	int oerrno = errno;

	/*
	 * If priv parent gets a TERM or HUP, pass it through to child
	 * instead.
	 */

	if (child_pid != -1)
		kill(child_pid, sig);

	errno = oerrno;
}

void
hostapd_sig_chld(int sig)
{
	/*
	 * If parent gets a SIGCHLD, it will exit.
	 */

	if (sig == SIGCHLD) {
		/* This will terminate libevent's main loop */
		event_gotterm = 1;
	}
}

/*
 * privsep I/O functions
 */

/* Read all data or return 1 for error.  */
int
hostapd_may_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while ((ssize_t)n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			return (1);
		default:
			pos += res;
		}
	}
	return (0);
}

/*
 * Read data with the assertion that it all must come through, or
 * else abort the process.  Based on atomicio() from openssh.
 */
void
hostapd_must_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while ((ssize_t)n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			_exit(0);
		default:
			pos += res;
		}
	}
}

/*
 * Write data with the assertion that it all has to be written, or
 * else abort the process.  Based on atomicio() from openssh.
 */
void
hostapd_must_write(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while ((ssize_t)n > pos) {
		res = write(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			_exit(0);
		default:
			pos += res;
		}
	}
}

