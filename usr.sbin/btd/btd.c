/*	$OpenBSD: btd.c,v 1.1 2008/11/24 23:34:42 uwe Exp $	*/

#include <sys/ioctl.h>
#include <sys/wait.h>

#include <dev/bluetooth/btdev.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btd.h"

void sighdlr(int, short, void *);
__dead void usage(void);
int check_child(pid_t, const char *);

static const char *progname;
static int quit = 0;
static int reconfig = 0;
static int sigchld = 0;

static struct event ev_sigchld;
static struct event ev_sighup;
static struct event ev_sigint;
static struct event ev_sigterm;
static struct bufferevent *ev_bt;

static void readcb(struct bufferevent *, void *);
static void writecb(struct bufferevent *, void *);
static void errorcb(struct bufferevent *, short, void *);

void
sighdlr(int sig, short what, void *arg)
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

__dead void
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

	/* only after daemon() */
	event_init();

	signal_set(&ev_sigchld, SIGCHLD, sighdlr, &env);
	signal_set(&ev_sighup, SIGHUP, sighdlr, &env);
	signal_set(&ev_sigint, SIGINT, sighdlr, &env);
	signal_set(&ev_sigterm, SIGTERM, sighdlr, &env);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);

	close(pipe_chld[1]);

	if ((ev_bt = bufferevent_new(pipe_chld[0], readcb,
	    writecb, errorcb, &env)) == NULL)
		fatalx("bufferevent_new ev_bt");

	bufferevent_enable(ev_bt, EV_READ);

	while (quit == 0) {
		if (!event_loop(EVLOOP_ONCE))
			quit = 1;

		if (sigchld) {
			if (check_child(chld_pid, "child")) {
				quit = 1;
				chld_pid = 0;
			}
			sigchld = 0;
		}
	}

	signal_del(&ev_sigchld);

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

static void
readcb(struct bufferevent *ev, void *arg)
{
	log_warnx("readcb");
}

static void
writecb(struct bufferevent *ev, void *arg)
{
	/* nothing to do here */
	log_warnx("writecb");
}

static void
errorcb(struct bufferevent *ev, short what, void *arg)
{
	log_warnx("Pipe error");
	quit = 1;
}

#ifdef notyet
void
btd_devctl(struct imsg *imsg)
{
	struct btdev_attach_args baa;
	char buf[sizeof(int) + sizeof(bdaddr_t)];
	unsigned long cmd;
	int res;
	int fd;

	if (devinfo_load_attach_args(&baa, imsg->data,
	    imsg->hdr.len - IMSG_HEADER_SIZE))
		fatalx("invalid IMSG_ATTACH/DETACH received");

	if ((fd = open(BTHUB_PATH, O_WRONLY, 0)) == -1) {
		res = errno;
		log_warn("can't open %s", BTHUB_PATH);
		goto ret;
	}

	cmd = imsg->hdr.type == IMSG_ATTACH ? BTDEV_ATTACH : BTDEV_DETACH;
	if (ioctl(fd, cmd, &baa) == -1)
		res = errno;

	res = 0;
	(void)close(fd);
ret:
	devinfo_unload_attach_args(&baa);

	memcpy(buf, &res, sizeof(res));
	memcpy((int *)buf + 1, &baa.bd_raddr, sizeof(bdaddr_t));
	/* send reply */
}
#endif
