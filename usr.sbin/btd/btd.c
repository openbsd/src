/*	$OpenBSD: btd.c,v 1.4 2008/11/27 00:51:17 uwe Exp $	*/

/*
 * Copyright (c) 2008 Uwe Stuehler <uwe@openbsd.org>
 * Copyright (c) 2003 Can Erkin Acar
 * Copyright (c) 2003 Anil Madhavapeddy <anil@recoil.org>
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

#include <sys/ioctl.h>
#include <sys/wait.h>

#include <dev/bluetooth/btdev.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btd.h"

static void sighdlr(int);
static __dead void usage(void);
static int check_child(pid_t, const char *);
static int read_all(void *, size_t);
static int write_all(const void *, size_t);
static int dispatch_imsg(struct btd *);
static void btd_open_hci(struct btd *);
static void btd_set_link_policy(struct btd *);
static void btd_set_interface_flags(struct btd *);
static void btd_devctl(struct btd *, enum imsg_type);

static const char *progname;
static volatile sig_atomic_t quit = 0;
static volatile sig_atomic_t reconfig = 0;
static volatile sig_atomic_t sigchld = 0;
static int pipe_fd;

static void
sighdlr(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		quit = 1;
		break;
	case SIGCHLD:
		sigchld = 1;
		break;
	case SIGHUP:
		reconfig = 1;
		break;
	}
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-d]\n", progname);
	exit(2);
}

int
main(int argc, char *argv[])
{
	struct btd env;
	pid_t chld_pid, pid;
	int pipe_chld[2];
	struct passwd *pw;
	struct pollfd pfd;
	int ch;

	progname = basename(argv[0]);
	bzero(&env, sizeof(env));
	TAILQ_INIT(&env.interfaces);
	TAILQ_INIT(&env.devices);

	while ((ch = getopt(argc, argv, "d")) != -1) {
		switch (ch) {
		case 'd':
			env.debug = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 0) {
		usage();
		/* NOTREACHED */
	}

	if (geteuid() != 0)
		errx(1, "need root privileges");

	if ((pw = getpwnam(BTD_USER)) == NULL)
		errx(1, "unknown user %s", BTD_USER);

	endpwent();

	log_init(env.debug);

	if (!env.debug && daemon(1, 0))
		fatal("daemon");

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_chld) == -1)
		fatal("socketpair");

	/* fork child process */
	sigchld = 1;
	chld_pid = bt_main(pipe_chld, &env, pw);

	setproctitle("[priv]");

	signal(SIGCHLD, sighdlr);
	signal(SIGTERM, sighdlr);
	signal(SIGINT, sighdlr);
	signal(SIGHUP, sighdlr);

	close(pipe_chld[1]);
	pipe_fd = pipe_chld[0];

	while (quit == 0) {
		int nfds;

		pfd.fd = pipe_fd;
		pfd.events = POLLIN;

		if ((nfds = poll(&pfd, 1, 0)) == -1)
			if (errno != EINTR) {
				log_warn("poll error");
				quit = 1;
			}


		if (pfd.revents & POLLIN)
			if (dispatch_imsg(&env) == -1)
				quit = 1;

		if (sigchld) {
			if (check_child(chld_pid, "child")) {
				quit = 1;
				chld_pid = 0;
			}
			sigchld = 0;
		}

	}

	signal(SIGCHLD, SIG_DFL);

	if (chld_pid)
		kill(chld_pid, SIGTERM);

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	log_info("Terminating");
	control_cleanup();
	return 0;
}

int
check_child(pid_t pid, const char *pname)
{
	int	 status, sig;
	char	*signame;

	if (waitpid(pid, &status, WNOHANG) > 0) {
		if (WIFEXITED(status)) {
			log_warnx("Lost child: %s exited", pname);
			return (1);
		}
		if (WIFSIGNALED(status)) {
			sig = WTERMSIG(status);
			signame = strsignal(sig) ? strsignal(sig) : "unknown";
			log_warnx("Lost child: %s terminated; signal %d (%s)",
			    pname, sig, signame);
			return (1);
		}
	}

	return (0);
}

static int
read_all(void *buf, size_t n)
{
	if (atomic_read(pipe_fd, buf, n) < 0) {
		close(pipe_fd);
		pipe_fd = -1;
		quit = 1;
		return -1;
	}

	return 0;
}

static int
write_all(const void *buf, size_t n)
{
	if (atomic_write(pipe_fd, buf, n) < 0) {
		close(pipe_fd);
		pipe_fd = -1;
		quit = 1;
		return -1;
	}

	return 0;
}

static int
dispatch_imsg(struct btd *env)
{
	enum imsg_type type;

	if (read_all(&type, sizeof(type)) < 0) {
		log_warnx("read_all");
		return -1;
	}

	switch (type) {
	case IMSG_OPEN_HCI:
		btd_open_hci(env);
		break;
	case IMSG_SET_LINK_POLICY:
		btd_set_link_policy(env);
		break;
	case IMSG_SET_INTERFACE_FLAGS:
		btd_set_interface_flags(env);
		break;
	case IMSG_ATTACH:
	case IMSG_DETACH:
		btd_devctl(env, type);
		break;
	default:
		log_warnx("invalid message, type=%#x", type);
		return -1;
	}

	return 0;
}

static void
btd_open_hci(struct btd *env)
{
	struct sockaddr_bt sa;
	bdaddr_t addr;
	int fd;
	int err = 0;

	if (read_all(&addr, sizeof(addr)) < 0)
		return;

	if ((fd = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) == -1) {
		err = errno;
		write_all(&err, sizeof(err));
		return;
	}

	bzero(&sa, sizeof(sa));
	sa.bt_len = sizeof(sa);
	sa.bt_family = AF_BLUETOOTH;
	bdaddr_copy(&sa.bt_bdaddr, &addr);

	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0 ||
	    connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		err = errno;
		close(fd);
		fd = -1;
	}

	send_fd(pipe_fd, fd);

	(void)write_all(&err, sizeof(err));

	if (fd != -1)
		close(fd);
}

static void
btd_set_link_policy(struct btd *env)
{
	struct btreq btr;
	bdaddr_t addr;
	uint16_t link_policy;
	int err = 0;
	int fd;

	if (read_all(&addr, sizeof(addr)) < 0 ||
	    read_all(&link_policy, sizeof(link_policy)) < 0)
		return;

	if ((fd = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) == -1) {
		err = errno;
		write_all(&err, sizeof(err));
		return;
	}

	bdaddr_copy(&btr.btr_bdaddr, &addr);

	if (ioctl(fd, SIOCGBTINFOA, &btr) < 0) {
		err = errno;
		close(fd);
		write_all(&err, sizeof(err));
		return;
	}

	btr.btr_link_policy = link_policy;

	if (ioctl(fd, SIOCSBTPOLICY, &btr) < 0)
		err = errno;

	write_all(&err, sizeof(err));
	close(fd);
}

static void
btd_set_interface_flags(struct btd *env)
{
	struct btreq btr;
	uint16_t mask;
	uint16_t flags;
	int err = 0;
	int fd;

	memset(&btr, 0, sizeof(btr));

	if (read_all(&btr.btr_name, sizeof(btr.btr_name)) < 0 ||
	    read_all(&flags, sizeof(flags)) < 0)
		return;

	if ((fd = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) == -1) {
		err = errno;
		write_all(&err, sizeof(err));
		return;
	}

	if (ioctl(fd, SIOCGBTINFO, &btr) < 0) {
		err = errno;
		log_warn("SIOCGBTINFO");
		close(fd);
		write_all(&err, sizeof(err));
		return;
	}

	/* limit allowed flags */
	mask = BTF_UP;
	btr.btr_flags &= ~mask;
	btr.btr_flags |= (flags & mask);

	if (ioctl(fd, SIOCSBTFLAGS, &btr) < 0) {
		err = errno;
		log_warn("SIOCSBTFLAGS");
	}

	sleep(1); /* XXX wait until it comes up */

	write_all(&err, sizeof(err));
	close(fd);
}

void
btd_devctl(struct btd *env, enum imsg_type type)
{
	struct btdev_attach_args baa;
	unsigned long cmd;
	void *data;
	size_t datalen;
	int res = 0;
	int fd;

	if (read_all(&datalen, sizeof(datalen)) < 0)
		return;

	if (datalen < sizeof(baa) || datalen > sizeof(baa) + 65536)
		fatalx("invalid data length in IMSG_ATTACH/DETACH");

	if ((data = malloc(datalen)) == NULL)
		fatal("btd_devctl");

	if (read_all(data, datalen) < 0) {
		free(data);
		return;
	}

	if (devinfo_load_attach_args(&baa, data, datalen))
		fatalx("invalid IMSG_ATTACH/DETACH received");

	if ((fd = open(BTHUB_PATH, O_WRONLY, 0)) == -1) {
		res = errno;
		goto ret;
	}

	cmd = type == IMSG_ATTACH ? BTDEV_ATTACH : BTDEV_DETACH;
	if (ioctl(fd, cmd, &baa) != 0)
		res = errno;

	(void)close(fd);
ret:
	if (data != NULL)
		free(data);

	devinfo_unload_attach_args(&baa);

	write_all(&res, sizeof(res));
}
