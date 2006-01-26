/*	$OpenBSD: monitor.c,v 1.7 2006/01/26 09:53:46 moritz Exp $	*/

/*
 * Copyright (c) 2005 Håkan Olsson.  All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <net/pfkeyv2.h>

#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "sasyncd.h"

struct m_state {
	pid_t	pid;
	int	s;
} m_state;

volatile sig_atomic_t		sigchld = 0;

static void	got_sigchld(int);
static void	sig_to_child(int);
static void	m_priv_pfkey_snap(int);
static ssize_t	m_write(int, void *, size_t);
static ssize_t	m_read(int, void *, size_t);

pid_t
monitor_init(void)
{
	struct passwd	*pw = getpwnam(SASYNCD_USER);
	extern char	*__progname;
	char		root[MAXPATHLEN];
	int		p[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, p) != 0) {
		log_err("%s: socketpair failed - %s", __progname, 
		    strerror(errno));
		exit(1);
	}
	
	if (!pw) {
		log_err("%s: getpwnam(\"%s\") failed", __progname,
		    SASYNCD_USER);
		exit(1);
	}
	strlcpy(root, pw->pw_dir, sizeof root);
	endpwent();

	signal(SIGCHLD, got_sigchld);
	signal(SIGTERM, sig_to_child);
	signal(SIGHUP, sig_to_child);
	signal(SIGINT, sig_to_child);

	if (chroot(pw->pw_dir) != 0 || chdir("/") != 0) {
		log_err("%s: chroot failed", __progname);
		exit(1);
	}

	m_state.pid = fork();

	if (m_state.pid == -1) {
		log_err("%s: fork failed - %s", __progname, strerror(errno));
		exit(1);
	} else if (m_state.pid == 0) {
		/* Child */
		m_state.s = p[0];
		close(p[1]);

		if (setgroups(1, &pw->pw_gid) || 
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid)) {
			log_err("%s: failed to drop privileges", __progname);
			exit(1);
		}
	} else {
		/* Parent */
		setproctitle("[priv]");
		m_state.s = p[1];
		close(p[0]);
	}
	return m_state.pid;
}

static void
got_sigchld(int s)
{
	sigchld = 1;
}

static void
sig_to_child(int s)
{
	if (m_state.pid != -1)
		kill(m_state.pid, s);
}

static void
monitor_drain_input(void)
{
	int		one = 1;
	u_int8_t	tmp;

	ioctl(m_state.s, FIONBIO, &one);
	while (m_read(m_state.s, &tmp, 1) > 0)
		;
	ioctl(m_state.s, FIONBIO, 0);
}

/* We only use privsep to get in-kernel SADB and SPD snapshots via sysctl */
void
monitor_loop(void)
{
	u_int32_t	v;
	ssize_t		r;

	for (;;) {
		if (sigchld) {
			pid_t	pid;
			int	status;
			do {
				pid = waitpid(m_state.pid, &status, WNOHANG);
			} while (pid == -1 && errno == EINTR);

			if (pid == m_state.pid &&
			    (WIFEXITED(status) || WIFSIGNALED(status)))
				break;
		}

		/* Wait for next snapshot task. Disregard read data. */
		if ((r = m_read(m_state.s, &v, sizeof v)) < 1) {
			if (r == -1)
				log_err(0, "monitor_loop: read() ");
			break;
		}

		/* Get the data. */
		m_priv_pfkey_snap(m_state.s);
	}

	if (!sigchld)
		log_msg(0, "monitor_loop: priv process exiting abnormally");
	exit(0);
}

int
monitor_get_pfkey_snap(u_int8_t **sadb, u_int32_t *sadbsize, u_int8_t **spd,
    u_int32_t *spdsize)
{
	u_int32_t	v;
	ssize_t		rbytes;

	/* We write a (any) value to the monitor socket to start a snapshot. */
	v = 0;
	if (m_write(m_state.s, &v, sizeof v) < 1)
		return -1;

	/* Read SADB data. */
	*sadb = *spd = NULL;
	*spdsize = 0;
	if (m_read(m_state.s, sadbsize, sizeof *sadbsize) < 1)
		return -1;
	if (*sadbsize) {
		*sadb = (u_int8_t *)malloc(*sadbsize);
		if (!*sadb) {
			log_err("monitor_get_pfkey_snap: malloc()");
			monitor_drain_input();
			return -1;
		}
		rbytes = m_read(m_state.s, *sadb, *sadbsize);
		if (rbytes < 1) {
			memset(*sadb, 0, *sadbsize);
			free(*sadb);
			return -1;
		}
	}

	/* Read SPD data */
	if (m_read(m_state.s, spdsize, sizeof *spdsize) < 1) {
		if (*sadbsize) {
			memset(*sadb, 0, *sadbsize);
			free(*sadb);
		}
		return -1;
	}
	if (*spdsize) {
		*spd = (u_int8_t *)malloc(*spdsize);
		if (!*spd) {
			log_err("monitor_get_pfkey_snap: malloc()");
			monitor_drain_input();
			if (*sadbsize) {
				memset(*sadb, 0, *sadbsize);
				free(*sadb);
			}
			return -1;
		}
		rbytes = m_read(m_state.s, *spd, *spdsize);
		if (rbytes < 1) {
			memset(*spd, 0, *spdsize);
			free(*spd);
			if (*sadbsize) {
				memset(*sadb, 0, *sadbsize);
				free(*sadb);
			}
			return -1;
		}
	}

	log_msg(3, "monitor_get_pfkey_snap: got %u bytes SADB, %u bytes SPD",
	    *sadbsize, *spdsize);
	return 0;
}

/* Privileged */
static void
m_priv_pfkey_snap(int s)
{
	u_int8_t	*sadb_buf = 0, *spd_buf = 0;
	size_t		 sadb_buflen = 0, spd_buflen = 0, sz;
	int		 mib[5];
	u_int32_t	 v;

	mib[0] = CTL_NET;
	mib[1] = PF_KEY;
	mib[2] = PF_KEY_V2;
	mib[3] = NET_KEY_SADB_DUMP;
	mib[4] = 0; /* Unspec SA type */

	/* First, fetch SADB data */
	if (sysctl(mib, sizeof mib / sizeof mib[0], NULL, &sz, NULL, 0) == -1
	    || sz == 0) {
		sadb_buflen = 0;
		goto try_spd;
	}
	
	sadb_buflen = sz;
	if ((sadb_buf = malloc(sadb_buflen)) == NULL) {
		log_err("m_priv_pfkey_snap: malloc");
		sadb_buflen = 0;
		goto out;
	}

	if (sysctl(mib, sizeof mib / sizeof mib[0], sadb_buf, &sz, NULL, 0)
	    == -1) {
		log_err("m_priv_pfkey_snap: sysctl");
		sadb_buflen = 0;
		goto out;
	}

	/* Next, fetch SPD data */
  try_spd:
	mib[3] = NET_KEY_SPD_DUMP;

	if (sysctl(mib, sizeof mib / sizeof mib[0], NULL, &sz, NULL, 0) == -1
	    || sz == 0) {
		spd_buflen = 0;
		goto out;
	}
	
	spd_buflen = sz;
	if ((spd_buf = malloc(spd_buflen)) == NULL) {
		log_err("m_priv_pfkey_snap: malloc");
		spd_buflen = 0;
		goto out;
	}

	if (sysctl(mib, sizeof mib / sizeof mib[0], spd_buf, &sz, NULL, 0)
	    == -1) {
		log_err("m_priv_pfkey_snap: sysctl");
		spd_buflen = 0;
		goto out;
	}

  out:
	/* Return SADB data */
	v = (u_int32_t)sadb_buflen;
	if (m_write(s, &v, sizeof v) == -1) {
		log_err("m_priv_pfkey_snap: write");
		return;
	}
	if (sadb_buflen) {
		if (m_write(s, sadb_buf, sadb_buflen) == -1)
			log_err("m_priv_pfkey_snap: write");
		memset(sadb_buf, 0, sadb_buflen);
		free(sadb_buf);
	}

	/* Return SPD data */
	v = (u_int32_t)spd_buflen;
	if (m_write(s, &v, sizeof v) == -1) {
		log_err("m_priv_pfkey_snap: write");
		return;
	}
	if (spd_buflen) {
		if (m_write(s, spd_buf, spd_buflen) == -1)
			log_err("m_priv_pfkey_snap: write");
		memset(spd_buf, 0, spd_buflen);
		free(spd_buf);
	}
	return;
}

ssize_t
m_write(int sock, void *buf, size_t len)
{
	ssize_t n;
	size_t pos = 0;
	char *ptr = buf;

	while (len > pos) {
		switch (n = write(sock, ptr + pos, len - pos)) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
			/* FALLTHROUGH */
		case 0:
			return n;
			/* NOTREACHED */
		default:
			pos += n;
		}
	}
	return pos;
}

ssize_t
m_read(int sock, void *buf, size_t len)
{
	ssize_t n;
	size_t pos = 0;
	char *ptr = buf;

	while (len > pos) {
		switch (n = read(sock, ptr + pos, len - pos)) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
			/* FALLTHROUGH */
		case 0:
			return n;
			/* NOTREACHED */
		default:
			pos += n;
		}
	}
	return pos;
}
