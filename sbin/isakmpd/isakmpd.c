/* $OpenBSD: isakmpd.c,v 1.64 2004/06/14 09:55:41 ho Exp $	 */
/* $EOM: isakmpd.c,v 1.54 2000/10/05 09:28:22 niklas Exp $	 */

/*
 * Copyright (c) 1998, 1999, 2000, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 1999, 2000 Angelos D. Keromytis.  All rights reserved.
 * Copyright (c) 1999, 2000, 2001 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include "sysdep.h"

#include "app.h"
#include "conf.h"
#include "connection.h"
#include "init.h"
#include "libcrypto.h"
#include "log.h"
#include "monitor.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"
#include "udp.h"
#include "ui.h"
#include "util.h"
#include "cert.h"

#ifdef USE_POLICY
#include "policy.h"
#endif

static void     usage(void);

/*
 * Set if -d is given, currently just for running in the foreground and log
 * to stderr instead of syslog.
 */
int             debug = 0;

/*
 * If we receive a SIGHUP signal, this flag gets set to show we need to
 * reconfigure ASAP.
 */
volatile sig_atomic_t sighupped = 0;

/*
 * If we receive a USR1 signal, this flag gets set to show we need to dump
 * a report over our internal state ASAP.  The file to report to is settable
 * via the -R parameter.
 */
volatile sig_atomic_t sigusr1ed = 0;
static char    *report_file = "/var/run/isakmpd.report";

/*
 * If we receive a USR2 signal, this flag gets set to show we need to
 * rehash our SA soft expiration timers to a uniform distribution.
 * XXX Perhaps this is a really bad idea?
 */
volatile sig_atomic_t sigusr2ed = 0;

/*
 * If we receive a TERM signal, perform a "clean shutdown" of the daemon.
 * This includes to send DELETE notifications for all our active SAs.
 * Also on recv of an INT signal (Ctrl-C out of an '-d' session, typically).
 */
volatile sig_atomic_t sigtermed = 0;
void            daemon_shutdown_now(int);

/* The default path of the PID file.  */
static char    *pid_file = "/var/run/isakmpd.pid";

#ifdef USE_DEBUG
/* The path of the IKE packet capture log file.  */
static char    *pcap_file = 0;
#endif

static void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-4] [-6] [-c config-file] [-d] [-D class=level]\n"
	    "          [-f fifo] [-i pid-file] [-n] [-p listen-port]\n"
	    "          [-P local-port] [-L] [-l packetlog-file] [-r seed]\n"
	    "          [-R report-file] [-v]\n",
	    sysdep_progname());
	exit(1);
}

static void
parse_args(int argc, char *argv[])
{
	int             ch;
	char           *ep;
#ifdef USE_DEBUG
	int             cls, level;
	int             do_packetlog = 0;
#endif

	while ((ch = getopt(argc, argv, "46c:dD:f:i:np:P:Ll:r:R:v")) != -1) {
		switch (ch) {
		case '4':
			bind_family |= BIND_FAMILY_INET4;
			break;

		case '6':
			bind_family |= BIND_FAMILY_INET6;
			break;

		case 'c':
			conf_path = optarg;
			break;

		case 'd':
			debug++;
			break;

#ifdef USE_DEBUG
		case 'D':
			if (sscanf(optarg, "%d=%d", &cls, &level) != 2) {
				if (sscanf(optarg, "A=%d", &level) == 1) {
					for (cls = 0; cls < LOG_ENDCLASS;
					     cls++)
						log_debug_cmd(cls, level);
				} else
					log_print("parse_args: -D argument "
					    "unparseable: %s", optarg);
			} else
				log_debug_cmd(cls, level);
			break;
#endif				/* USE_DEBUG */

		case 'f':
			ui_fifo = optarg;
			break;

		case 'i':
			pid_file = optarg;
			break;

		case 'n':
			app_none++;
			break;

		case 'p':
			udp_default_port = optarg;
			break;

		case 'P':
			udp_bind_port = optarg;
			break;

#ifdef USE_DEBUG
		case 'l':
			pcap_file = optarg;
			/* Fallthrough intended.  */

		case 'L':
			do_packetlog++;
			break;
#endif				/* USE_DEBUG */

		case 'r':
			seed = strtoul(optarg, &ep, 0);
			srandom(seed);
			if (*ep != '\0')
				log_fatal("parse_args: invalid numeric arg "
				    "to -r (%s)", optarg);
			regrand = 1;
			break;

		case 'R':
			report_file = optarg;
			break;

		case 'v':
			verbose_logging = 1;
			break;

		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

#ifdef USE_DEBUG
	if (do_packetlog && !pcap_file)
		pcap_file = PCAP_FILE_DEFAULT;
#endif
}

static void
sighup(int sig)
{
	sighupped = 1;
}

/* Report internal state on SIGUSR1.  */
static void
report(void)
{
	FILE	*rfp, *old;
	mode_t	old_umask;

	old_umask = umask(S_IRWXG | S_IRWXO);
	rfp = monitor_fopen(report_file, "w");
	umask(old_umask);

	if (!rfp) {
		log_error("report: fopen (\"%s\", \"w\") failed", report_file);
		return;
	}
	/* Divert the log channel to the report file during the report.  */
	old = log_current();
	log_to(rfp);
	ui_report("r");
	log_to(old);
	fclose(rfp);

	sigusr1ed = 0;
}

static void
sigusr1(int sig)
{
	sigusr1ed = 1;
}

/* Rehash soft expiration timers on SIGUSR2.  */
static void
rehash_timers(void)
{
#if 0
	/* XXX - not yet */
	log_print("SIGUSR2 received, rehashing soft expiration timers.");

	timer_rehash_timers();
#endif

	sigusr2ed = 0;
}

static void
sigusr2(int sig)
{
	sigusr2ed = 1;
}

static int
phase2_sa_check(struct sa *sa, void *arg)
{
	return sa->phase == 2;
}

static void
daemon_shutdown(void)
{
	/* Perform a (protocol-wise) clean shutdown of the daemon.  */
	struct sa	*sa;

	if (sigtermed == 1) {
		log_print("isakmpd: shutting down...");

		/* Delete all active phase 2 SAs.  */
		while ((sa = sa_find(phase2_sa_check, NULL))) {
			/* Each DELETE is another (outgoing) message.  */
			sa_delete(sa, 1);
		}
		sigtermed++;
	}
	if (transport_prio_sendqs_empty()) {
		/*
		 * When the prioritized transport sendq:s are empty, i.e all
		 * the DELETE notifications have been sent, we can shutdown.
	         */

#ifdef USE_DEBUG
		log_packet_stop();
#endif
		/* Remove FIFO and pid files.  */
		unlink(ui_fifo);
		unlink(pid_file);
		log_print("isakmpd: exit");
		exit(0);
	}
}

/* Called on SIGTERM, SIGINT or by ui_shutdown_daemon().  */
void
daemon_shutdown_now(int sig)
{
	sigtermed = 1;
}

/* Write pid file.  */
static void
write_pid_file(void)
{
	FILE	*fp;

	/* Ignore errors. This will fail with USE_PRIVSEP.  */
	unlink(pid_file);

	fp = monitor_fopen(pid_file, "w");
	if (fp != NULL) {
		if (fprintf(fp, "%ld\n", (long) getpid()) < 0)
			log_error("write_pid_file: failed to write PID to "
			    "\"%.100s\"", pid_file);
		fclose(fp);
	} else
		log_fatal("write_pid_file: fopen (\"%.100s\", \"w\") failed",
		    pid_file);
}

int
main(int argc, char *argv[])
{
	fd_set         *rfds, *wfds;
	int             n, m;
	size_t          mask_size;
	struct timeval  tv, *timeout;

#if defined (HAVE_CLOSEFROM) && (!defined (OpenBSD) || (OpenBSD >= 200405))
	closefrom(STDERR_FILENO + 1);
#else
	m = getdtablesize();
	for (n = STDERR_FILENO + 1; n < m; n++)
		(void) close(n);
#endif

	/*
	 * Make sure init() won't alloc fd 0, 1 or 2, as daemon() will close
	 * them.
	 */
	for (n = 0; n <= 2; n++)
		if (fcntl(n, F_GETFL, 0) == -1 && errno == EBADF)
			(void) open("/dev/null", n ? O_WRONLY : O_RDONLY, 0);

	for (n = 1; n < _NSIG; n++)
		signal(n, SIG_DFL);

	/* Log cmd line parsing and initialization errors to stderr.  */
	log_to(stderr);
	parse_args(argc, argv);
	log_init(debug);

	/* Open protocols and services databases.  */
	setprotoent(1);
	setservent(1);

	/*
	 * Do a clean daemon shutdown on TERM/INT. These signals must be
	 * initialized before monitor_init(). INT is only used with '-d'.
         */
	signal(SIGTERM, daemon_shutdown_now);
	if (debug == 1)		/* i.e '-dd' will skip this.  */
		signal(SIGINT, daemon_shutdown_now);

	/* Daemonize before forking unpriv'ed child */
	if (!debug)
		if (daemon(0, 0))
			log_fatal("main: daemon (0, 0) failed");

	/* Set timezone before priv'separation */
	tzset();

#if defined (USE_PRIVSEP)
	if (monitor_init()) {
		/* The parent, with privileges enters infinite monitor loop. */
		monitor_loop(debug);
		exit(0);	/* Never reached.  */
	}
	/* Child process only from this point on, no privileges left.  */
#endif

	init();

	write_pid_file();

	/* Reinitialize on HUP reception.  */
	signal(SIGHUP, sighup);

	/* Report state on USR1 reception.  */
	signal(SIGUSR1, sigusr1);

	/* Rehash soft expiration timers on USR2 reception.  */
	signal(SIGUSR2, sigusr2);

#if defined (USE_DEBUG)
	/* If we wanted IKE packet capture to file, initialize it now.  */
	if (pcap_file != 0)
		log_packet_init(pcap_file);
#endif

	/* Allocate the file descriptor sets just big enough.  */
	n = getdtablesize();
	mask_size = howmany(n, NFDBITS) * sizeof(fd_mask);
	rfds = (fd_set *) malloc(mask_size);
	if (!rfds)
		log_fatal("main: malloc (%lu) failed",
		    (unsigned long)mask_size);
	wfds = (fd_set *) malloc(mask_size);
	if (!wfds)
		log_fatal("main: malloc (%lu) failed",
		    (unsigned long)mask_size);

#if defined (USE_PRIVSEP)
	monitor_init_done();
#endif

	while (1) {
		/* If someone has sent SIGHUP to us, reconfigure.  */
		if (sighupped) {
			log_print("SIGHUP received");
			reinit();
			sighupped = 0;
		}
		/* and if someone sent SIGUSR1, do a state report.  */
		if (sigusr1ed) {
			log_print("SIGUSR1 received");
			report();
		}
		/* and if someone sent SIGUSR2, do a timer rehash.  */
		if (sigusr2ed) {
			log_print("SIGUSR2 received");
			rehash_timers();
		}
		/*
		 * and if someone set 'sigtermed' (SIGTERM, SIGINT or via the
		 * UI), this indicates we should start a controlled shutdown
		 * of the daemon.
	         *
		 * Note: Since _one_ message is sent per iteration of this
		 * enclosing while-loop, and we want to send a number of
		 * DELETE notifications, we must loop atleast this number of
		 * times. The daemon_shutdown() function starts by queueing
		 * the DELETEs, all other calls just increments the
		 * 'sigtermed' variable until it reaches a "safe" value, and
		 * the daemon exits.
	         */
		if (sigtermed)
			daemon_shutdown();

		/* Setup the descriptors to look for incoming messages at.  */
		memset(rfds, 0, mask_size);
		n = transport_fd_set(rfds);
		FD_SET(ui_socket, rfds);
		if (ui_socket + 1 > n)
			n = ui_socket + 1;

		/*
		 * XXX Some day we might want to deal with an abstract
		 * application class instead, with many instantiations
		 * possible.
	         */
		if (!app_none && app_socket >= 0) {
			FD_SET(app_socket, rfds);
			if (app_socket + 1 > n)
				n = app_socket + 1;
		}
		/* Setup the descriptors that have pending messages to send. */
		memset(wfds, 0, mask_size);
		m = transport_pending_wfd_set(wfds);
		if (m > n)
			n = m;

		/* Find out when the next timed event is.  */
		timeout = &tv;
		timer_next_event(&timeout);

		n = select(n, rfds, wfds, 0, timeout);
		if (n == -1) {
			if (errno != EINTR) {
				log_error("main: select");

				/*
				 * In order to give the unexpected error
				 * condition time to resolve without letting
				 * this process eat up all available CPU
				 * we sleep for a short while.
			         */
				sleep(1);
			}
		} else if (n) {
			transport_handle_messages(rfds);
			transport_send_messages(wfds);
			if (FD_ISSET(ui_socket, rfds))
				ui_handler();
			if (!app_none && app_socket >= 0 &&
			    FD_ISSET(app_socket, rfds))
				app_handler();
		}
		timer_handle_expirations();
	}
}
