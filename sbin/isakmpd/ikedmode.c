/*	$OpenBSD: ikedmode.c,v 1.1 2026/11/09 12:31:36 hshoexer Exp $	*/

/*
 * Copyright (c) 2026 Hans-Joerg Hoexer <hshoexer@yerbouti.franken.de>
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

/*
 * isakmpd as the ikev1 process of iked(8).
 *
 * The parent also forwards IMSG_CTL_ACTIVE/IMSG_CTL_PASSIVE (iked -S,
 * ikectl active/passive); in passive mode isakmpd does not initiate.
 *
 * iked starts isakmpd as "isakmpd --iked-mode -P ikev1 -I 0" followed by
 * iked's own command line, with the imsg channel to the iked parent on
 * file descriptor PROC_PARENT_SOCK_FILENO.  This file is the child side
 * of iked's proc.c: it drops privileges, accepts the pipes to the other
 * iked processes, acknowledges the ready message and runs a libevent
 * loop that drives isakmpd's timers.  The monitor, the FIFO user
 * interface and all file and socket access of isakmpd are bypassed:
 *
 * - the parent sends the configuration as the command lines ipsecctl(8)
 *   would write to the FIFO (IMSG_IKEV1_COMMAND), the PF_KEY socket
 *   (IMSG_PFKEY_SOCKET) and reset requests (IMSG_CTL_RESET);
 * - the ikev2 process forwards IKEv1 datagrams (IMSG_IKE_MESSAGE) and
 *   sends our replies, see transport_imsg.c.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app.h"
#include "conf.h"
#include "connection.h"
#include "ikedmode.h"
#include "init.h"
#include "log.h"
#include "policy.h"
#include "sa.h"
#include "ui.h"

struct message;
struct payload;
struct sa;
#include "nat_traversal.h"

#include "iked/types.h"

/*
 * timer.h cannot be included here, its struct event clashes with
 * libevent's.  These two functions are all the event loop needs.
 */
void	 timer_next_event(struct timespec **);
void	 timer_handle_expirations(void);

/* An iked process we talk to.  */
struct ikedmode_peer {
	const char		*name;
	enum privsep_procid	 id;
	int			(*cb)(struct ikedmode_peer *, struct imsg *);
	int			 fd;
	struct imsgbuf		 ibuf;
	struct event		 ev;
};

int		 iked_mode = 0;

static int	 ikedmode_instance;

static struct event ikedmode_evtimer;
static struct event ikedmode_evapp;

static void	 ikedmode_usage(void);
static void	 ikedmode_setverbose(int);
static void	 ikedmode_dropprivs(void);
static void	 ikedmode_sig_handler(int, short, void *);
static void	 ikedmode_timer(int, short, void *);
static void	 ikedmode_timer_schedule(void);
static void	 ikedmode_work(void);
static void	 ikedmode_app(int, short, void *);
static void	 ikedmode_reset(int);
static void	 ikedmode_shutdown(void);

static struct ikedmode_peer *ikedmode_peer_lookup(enum privsep_procid);
static void	 ikedmode_peer_accept(enum privsep_procid, unsigned int, int);
static void	 ikedmode_peer_event_add(struct ikedmode_peer *);
static void	 ikedmode_peer_dispatch(int, short, void *);
static void	 ikedmode_peer_closed(struct ikedmode_peer *);
static void	 ikedmode_peer_flush(enum privsep_procid);
static void	 ikedmode_dispatch_generic(struct ikedmode_peer *,
		    struct imsg *);
static int	 ikedmode_dispatch_parent(struct ikedmode_peer *,
		    struct imsg *);
static int	 ikedmode_dispatch_control(struct ikedmode_peer *,
		    struct imsg *);
static int	 ikedmode_dispatch_ikev2(struct ikedmode_peer *,
		    struct imsg *);

/*
 * The processes we exchange messages with.  This table must be symmetric
 * with the tables in iked: every process listed here lists ikev1, and
 * vice versa.  Descriptors for processes not listed here are closed.
 */
static struct ikedmode_peer ikedmode_peers[] = {
	{ "parent",	PROC_PARENT,	ikedmode_dispatch_parent },
	{ "control",	PROC_CONTROL,	ikedmode_dispatch_control },
	{ "ikev2",	PROC_IKEV2,	ikedmode_dispatch_ikev2 },
};

static void
ikedmode_usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s --iked-mode -P ikev1 -I 0 "
	    "[iked arguments]\n", __progname);
	exit(1);
}

int
ikedmode_main(int argc, char *argv[])
{
	struct event	 evsigint, evsigterm, evsigchld, evsighup, evsigpipe,
			 evsigusr1;
	const char	*errstr, *title = NULL;
	unsigned int	 i;
	int		 ch, debug = 0, verbose = 0;

	iked_mode = 1;
	log_to(stderr);

	/*
	 * argv[1] is --iked-mode, the rest is the command line of iked
	 * with the process name and instance prepended.  The option string
	 * is iked's; only the options relevant here are looked at, the
	 * others have been validated by iked already.
	 */
	optind = 2;
	while ((ch = getopt(argc, argv, "6D:df:I:nP:p:Ss:TtvV")) != -1) {
		switch (ch) {
		case 'd':
			debug++;
			break;
		case 'I':
			ikedmode_instance = strtonum(optarg, 0, 0, &errstr);
			if (errstr)
				log_fatalx("ikedmode_main: invalid process "
				    "instance");
			break;
		case 'P':
			title = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case '6':
		case 'D':
		case 'f':
		case 'n':
		case 'p':
		case 'S':
		case 's':
		case 'T':
		case 't':
		case 'V':
			break;
		default:
			ikedmode_usage();
		}
	}
	argc -= optind;
	if (argc > 0 || title == NULL || strcmp(title, "ikev1") != 0)
		ikedmode_usage();

	log_init(debug);
	ikedmode_setverbose(verbose);
	log_print("isakmpd: starting as ikev1 process of iked");

	/* Open protocols and services databases before chroot.  */
	setprotoent(1);
	setservent(1);

	/* Set timezone before chroot.  */
	tzset();

	/*
	 * iked does the work of the monitor: no KeyNote policy file, the
	 * PF_KEY socket comes from the parent, no UDP sockets and thus no
	 * NAT-T of our own.
	 */
	ignore_policy = 1;
	disable_nat_t = 1;

	for (i = 0; i < sizeof ikedmode_peers / sizeof ikedmode_peers[0]; i++)
		ikedmode_peers[i].fd = -1;

	ikedmode_dropprivs();
	setproctitle("%s", title);

	/*
	 * stdio - for malloc and basic I/O including events.
	 * recvfd - for receiving privsep pipe descriptors.
	 */
	if (pledge("stdio recvfd", NULL) == -1)
		log_fatal("ikedmode_main: pledge");

	init();

	event_init();

	signal_set(&evsigint, SIGINT, ikedmode_sig_handler, NULL);
	signal_set(&evsigterm, SIGTERM, ikedmode_sig_handler, NULL);
	signal_set(&evsigchld, SIGCHLD, ikedmode_sig_handler, NULL);
	signal_set(&evsighup, SIGHUP, ikedmode_sig_handler, NULL);
	signal_set(&evsigpipe, SIGPIPE, ikedmode_sig_handler, NULL);
	signal_set(&evsigusr1, SIGUSR1, ikedmode_sig_handler, NULL);

	signal_add(&evsigint, NULL);
	signal_add(&evsigterm, NULL);
	signal_add(&evsigchld, NULL);
	signal_add(&evsighup, NULL);
	signal_add(&evsigpipe, NULL);
	signal_add(&evsigusr1, NULL);

	evtimer_set(&ikedmode_evtimer, ikedmode_timer, NULL);

	ikedmode_peer_accept(PROC_PARENT, 0, PROC_PARENT_SOCK_FILENO);

	event_dispatch();

	ikedmode_shutdown();
	log_print("isakmpd: exit");
	exit(0);
}

static int
ikedmode_phase2_sa_check(struct sa *sa, void *arg)
{
	return sa->phase == 2;
}

static int
ikedmode_phase1_sa_check(struct sa *sa, void *arg)
{
	return sa->phase == 1;
}

/*
 * Delete all SAs, IPsec SAs first, as daemon_shutdown() in isakmpd.c
 * does.  Each DELETE notification is a message to the peer; hand them to
 * ikev2 before we go.  The event loop is gone at this point, so the pipe
 * is written synchronously; ikev2 waits for us to finish, see
 * ikev2_shutdown() in iked.
 */
static void
ikedmode_shutdown(void)
{
	struct sa	*sa;

	while ((sa = sa_find(ikedmode_phase2_sa_check, NULL)))
		sa_delete(sa, 1);
	while ((sa = sa_find(ikedmode_phase1_sa_check, NULL)))
		sa_delete(sa, 1);
	transport_imsg_flush();
	ikedmode_peer_flush(PROC_IKEV2);
}

/* Write everything queued for a peer; the other side may be gone.  */
static void
ikedmode_peer_flush(enum privsep_procid id)
{
	struct ikedmode_peer	*peer;

	if ((peer = ikedmode_peer_lookup(id)) == NULL || peer->fd == -1 ||
	    imsgbuf_queuelen(&peer->ibuf) == 0)
		return;

	signal(SIGPIPE, SIG_IGN);
	if (imsgbuf_flush(&peer->ibuf) == -1)
		LOG_DBG((LOG_MISC, 10, "ikedmode_peer_flush: cannot flush "
		    "messages to %s", peer->name));
}

/*
 * Map iked's verbosity to isakmpd's: -v enables verbose logging, each
 * further -v raises the debug level of all classes by ten, the same as
 * "-D A=level" on the isakmpd command line.
 */
static void
ikedmode_setverbose(int verbose)
{
	int	cls, level;

	verbose_logging = verbose > 0;
	level = verbose > 1 ? (verbose - 1) * 10 : 0;
	for (cls = 0; cls < LOG_ENDCLASS; cls++)
		log_debug_cmd(cls, level);
}

/* Change root and drop privileges to the iked user, as iked's proc.c.  */
static void
ikedmode_dropprivs(void)
{
	struct passwd	*pw;

	if ((pw = getpwnam(IKED_USER)) == NULL)
		log_fatalx("ikedmode_dropprivs: unknown user %s", IKED_USER);

	if (chroot(pw->pw_dir) == -1)
		log_fatal("ikedmode_dropprivs: chroot");
	if (chdir("/") == -1)
		log_fatal("ikedmode_dropprivs: chdir(\"/\")");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		log_fatal("ikedmode_dropprivs: cannot drop privileges");
}

static void
ikedmode_sig_handler(int sig, short event, void *arg)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		event_loopexit(NULL);
		break;
	case SIGCHLD:
	case SIGHUP:
	case SIGPIPE:
	case SIGUSR1:
		/* ignore */
		break;
	default:
		log_fatalx("ikedmode_sig_handler: unexpected signal %d", sig);
		/* NOTREACHED */
	}
}

/*
 * isakmpd keeps its own list of timed events (timer.c).  A single
 * libevent timer fires when the first of them expires; it is rescheduled
 * after every piece of work that may have changed the list.
 */
static void
ikedmode_timer(int fd, short event, void *arg)
{
	timer_handle_expirations();
	ikedmode_work();
}

/*
 * After any work (a message, a command, an expired timer) send what has
 * been queued and reschedule the timer.
 */
static void
ikedmode_work(void)
{
	transport_imsg_flush();
	ikedmode_timer_schedule();
}

/* The PF_KEY socket is readable.  */
static void
ikedmode_app(int fd, short event, void *arg)
{
	app_handler();
	ikedmode_work();
}

/* IMSG_CTL_RESET from the parent, see enum flushmode in iked.  */
static void
ikedmode_reset(int mode)
{
	switch (mode) {
	case RESET_RELOAD:
	case RESET_ALL:
	case RESET_POLICY:
		/*
		 * Drop the configuration; the parent sends the policies
		 * again.  conf_reinit() loads the defaults in iked mode.
		 */
		log_print("ikedmode_reset: resetting configuration");
		conf_reinit();
		connection_reinit();
		break;
	case RESET_SA:
		log_print("ikedmode_reset: tearing down all SAs");
		sa_teardown_all();
		break;
	default:
		/* Nothing of ours.  */
		break;
	}
}

static void
ikedmode_timer_schedule(void)
{
	struct timespec	 ts, *timeout = &ts;
	struct timeval	 tv;

	timer_next_event(&timeout);
	if (timeout == NULL) {
		evtimer_del(&ikedmode_evtimer);
		return;
	}
	TIMESPEC_TO_TIMEVAL(&tv, timeout);
	evtimer_add(&ikedmode_evtimer, &tv);
}

static struct ikedmode_peer *
ikedmode_peer_lookup(enum privsep_procid id)
{
	unsigned int	 i;

	for (i = 0; i < sizeof ikedmode_peers / sizeof ikedmode_peers[0]; i++)
		if (ikedmode_peers[i].id == id)
			return &ikedmode_peers[i];
	return NULL;
}

/* Take over a pipe to another iked process, see proc_accept() in iked.  */
static void
ikedmode_peer_accept(enum privsep_procid id, unsigned int n, int fd)
{
	struct ikedmode_peer	*peer;

	if (fd == -1 || id < 0 || id >= PROC_MAX || n != 0)
		log_fatalx("ikedmode_peer_accept: invalid descriptor "
		    "target %d instance %u", id, n);

	if ((peer = ikedmode_peer_lookup(id)) == NULL) {
		/* Not a process we talk to.  */
		close(fd);
		return;
	}

	if (peer->fd != -1)
		log_fatalx("ikedmode_peer_accept: duplicated descriptor "
		    "for %s", peer->name);
	peer->fd = fd;

	if (imsgbuf_init(&peer->ibuf, fd) == -1)
		log_fatal("ikedmode_peer_accept: imsgbuf_init");
	imsgbuf_allow_fdpass(&peer->ibuf);
	event_set(&peer->ev, fd, EV_READ, ikedmode_peer_dispatch, peer);
	event_add(&peer->ev, NULL);

	LOG_DBG((LOG_MISC, 10, "ikedmode_peer_accept: %s on fd %d",
	    peer->name, fd));
}

static void
ikedmode_peer_event_add(struct ikedmode_peer *peer)
{
	short	 events = EV_READ;

	if (imsgbuf_queuelen(&peer->ibuf) > 0)
		events |= EV_WRITE;

	event_del(&peer->ev);
	event_set(&peer->ev, peer->fd, events, ikedmode_peer_dispatch, peer);
	event_add(&peer->ev, NULL);
}

/* The other side closed the pipe; iked is shutting down, so are we.  */
static void
ikedmode_peer_closed(struct ikedmode_peer *peer)
{
	LOG_DBG((LOG_MISC, 10, "ikedmode_peer_closed: %s closed the pipe",
	    peer->name));
	event_del(&peer->ev);
	event_loopexit(NULL);
}

static void
ikedmode_peer_dispatch(int fd, short event, void *arg)
{
	struct ikedmode_peer	*peer = arg;
	struct imsgbuf		*ibuf = &peer->ibuf;
	struct imsg		 imsg;
	int			 n;

	if (event & EV_READ) {
		switch (imsgbuf_read(ibuf)) {
		case -1:
			log_fatal("ikedmode_peer_dispatch: imsgbuf_read");
			break;
		case 0:
			ikedmode_peer_closed(peer);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (imsgbuf_write(ibuf) == -1) {
			if (errno == EPIPE) {
				ikedmode_peer_closed(peer);
				return;
			}
			log_fatal("ikedmode_peer_dispatch: imsgbuf_write");
		}
	}

	for (;;) {
		if ((n = imsgbuf_get(ibuf, &imsg)) == -1)
			log_fatal("ikedmode_peer_dispatch: imsgbuf_get");
		if (n == 0)
			break;

		if (peer->cb(peer, &imsg) == -1)
			ikedmode_dispatch_generic(peer, &imsg);
		imsg_free(&imsg);
	}

	ikedmode_peer_event_add(peer);
	ikedmode_work();
}

/* Send a message to another iked process, see proc_compose_imsg().  */
int
ikedmode_compose(int id, uint16_t type, uint32_t peerid, int fd,
    const void *data, uint16_t datalen)
{
	struct ikedmode_peer	*peer;

	if ((peer = ikedmode_peer_lookup(id)) == NULL || peer->fd == -1)
		return -1;
	if (imsg_compose(&peer->ibuf, type, peerid,
	    ikedmode_instance + 1, fd, data, datalen) == -1)
		return -1;
	ikedmode_peer_event_add(peer);
	return 0;
}

/* Send a message built from an iovec, see proc_composev_imsg().  */
int
ikedmode_composev(int id, uint16_t type, const struct iovec *iov, int iovcnt)
{
	struct ikedmode_peer	*peer;

	if ((peer = ikedmode_peer_lookup(id)) == NULL || peer->fd == -1)
		return -1;
	if (imsg_composev(&peer->ibuf, type, (uint32_t)-1,
	    ikedmode_instance + 1, -1, iov, iovcnt) == -1)
		return -1;
	ikedmode_peer_event_add(peer);
	return 0;
}

/* Messages every iked process handles, see proc_dispatch() in iked.  */
static void
ikedmode_dispatch_generic(struct ikedmode_peer *peer, struct imsg *imsg)
{
	struct privsep_fd	 pf;
	int			 verbose;

	switch (imsg_get_type(imsg)) {
	case IMSG_CTL_VERBOSE:
		if (imsg_get_data(imsg, &verbose, sizeof verbose) == -1)
			log_fatalx("ikedmode_dispatch_generic: imsg_get_data");
		ikedmode_setverbose(verbose);
		break;
	case IMSG_CTL_PROCFD:
		if (peer->id != PROC_PARENT)
			log_fatalx("ikedmode_dispatch_generic: received "
			    "unexpected IMSG_CTL_PROCFD from %s", peer->name);
		if (imsg_get_data(imsg, &pf, sizeof pf) == -1)
			log_fatalx("ikedmode_dispatch_generic: imsg_get_data");
		ikedmode_peer_accept(pf.pf_procid, pf.pf_instance,
		    imsg_get_fd(imsg));
		break;
	case IMSG_CTL_PROCREADY:
		/* All pipes have been received, ack that we are ready.  */
		if (peer->id != PROC_PARENT)
			log_fatalx("ikedmode_dispatch_generic: received "
			    "unexpected IMSG_CTL_PROCREADY from %s",
			    peer->name);
		if (ikedmode_compose(PROC_PARENT, IMSG_CTL_PROCREADY,
		    (uint32_t)-1, -1, NULL, 0) == -1)
			log_fatal("ikedmode_dispatch_generic: "
			    "ikedmode_compose");
		LOG_DBG((LOG_MISC, 10, "ikedmode_dispatch_generic: ready"));
		break;
	default:
		log_fatalx("ikedmode_dispatch_generic: got invalid imsg %u "
		    "from %s", imsg_get_type(imsg), peer->name);
	}
}

static int
ikedmode_dispatch_parent(struct ikedmode_peer *peer, struct imsg *imsg)
{
	struct ibuf	 ibuf;
	char		*cmd, active[] = "M active", passive[] = "M passive";
	int		 fd, mode;

	switch (imsg_get_type(imsg)) {
	case IMSG_IKEV1_COMMAND:
		/* A command line for the user interface, NUL terminated.  */
		if (imsg_get_ibuf(imsg, &ibuf) == -1 ||
		    ibuf_size(&ibuf) == 0 ||
		    (cmd = ibuf_data(&ibuf))[ibuf_size(&ibuf) - 1] != '\0')
			log_fatalx("ikedmode_dispatch_parent: invalid "
			    "IMSG_IKEV1_COMMAND");
		ui_handle_command(cmd);
		break;
	case IMSG_PFKEY_SOCKET:
		if ((fd = imsg_get_fd(imsg)) == -1)
			log_fatalx("ikedmode_dispatch_parent: no PF_KEY "
			    "socket");
		app_setsocket(fd);
		event_set(&ikedmode_evapp, fd, EV_READ | EV_PERSIST,
		    ikedmode_app, NULL);
		event_add(&ikedmode_evapp, NULL);
		LOG_DBG((LOG_MISC, 10, "ikedmode_dispatch_parent: PF_KEY "
		    "socket on fd %d", fd));
		break;
	case IMSG_CTL_RESET:
		if (imsg_get_data(imsg, &mode, sizeof mode) == -1)
			log_fatalx("ikedmode_dispatch_parent: imsg_get_data");
		ikedmode_reset(mode);
		break;
	case IMSG_COMPILE:
		/*
		 * All policies have been sent.  Rebuild the connection lists
		 * now instead of waiting for the five second timer ui.c arms
		 * after each change: until then Quick Mode requests from the
		 * peer are rejected with INVALID_ID_INFORMATION and our own
		 * connections are not started.
		 */
		connection_reinit();
		break;
	case IMSG_CTL_ACTIVE:
		ui_handle_command(active);
		break;
	case IMSG_CTL_PASSIVE:
		ui_handle_command(passive);
		break;
	default:
		return -1;
	}

	return 0;
}

static int
ikedmode_dispatch_control(struct ikedmode_peer *peer, struct imsg *imsg)
{
	switch (imsg_get_type(imsg)) {
	default:
		break;
	}

	return -1;
}

static int
ikedmode_dispatch_ikev2(struct ikedmode_peer *peer, struct imsg *imsg)
{
	struct ikev1_datagram	 dg;
	struct ibuf		 ibuf;

	switch (imsg_get_type(imsg)) {
	case IMSG_IKE_MESSAGE:
		if (imsg_get_ibuf(imsg, &ibuf) == -1 ||
		    ibuf_get(&ibuf, &dg, sizeof dg) == -1)
			log_fatalx("ikedmode_dispatch_ikev2: invalid "
			    "IMSG_IKE_MESSAGE");
		if (ibuf_size(&ibuf) == 0) {
			log_print("ikedmode_dispatch_ikev2: empty datagram");
			break;
		}
		transport_imsg_recv(&dg, ibuf_data(&ibuf), ibuf_size(&ibuf));
		break;
	case IMSG_CTL_SHOW_SA:
		/*
		 * ikev2 forwards the control request after listing its own
		 * SAs, see ikev2_info(); the reply goes to control.
		 */
		ikedmode_show_sa(imsg_get_id(imsg));
		break;
	default:
		return -1;
	}

	return 0;
}
