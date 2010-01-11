/*	$OpenBSD: relayd.c,v 1.94 2010/01/11 06:40:14 jsg Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <sha1.h>
#include <md5.h>

#include <openssl/ssl.h>

#include "relayd.h"

__dead void	 usage(void);

void		 main_sig_handler(int, short, void *);
void		 main_shutdown(struct relayd *);
void		 main_dispatch_pfe(int, short, void *);
void		 main_dispatch_hce(int, short, void *);
void		 main_dispatch_relay(int, short, void *);
int		 check_child(pid_t, const char *);
int		 send_all(struct relayd *, enum imsg_type,
		    void *, u_int16_t);
void		 reconfigure(void);
void		 purge_tree(struct proto_tree *);
int		 bindany(struct ctl_bindany *);

int		 pipe_parent2pfe[2];
int		 pipe_parent2hce[2];
int		 pipe_pfe2hce[2];
int		 pipe_parent2relay[RELAY_MAXPROC][2];
int		 pipe_pfe2relay[RELAY_MAXPROC][2];

struct relayd	*relayd_env;

struct imsgev	*iev_pfe;
struct imsgev	*iev_hce;
struct imsgev	*iev_relay;

pid_t		 pfe_pid = 0;
pid_t		 hce_pid = 0;
pid_t		 relay_pid = 0;

void
main_sig_handler(int sig, short event, void *arg)
{
	struct relayd		*env = arg;
	int			 die = 0;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		if (check_child(pfe_pid, "pf update engine")) {
			pfe_pid = 0;
			die  = 1;
		}
		if (check_child(hce_pid, "host check engine")) {
			hce_pid = 0;
			die  = 1;
		}
		if (check_child(relay_pid, "socket relay engine")) {
			relay_pid = 0;
			die  = 1;
		}
		if (die)
			main_shutdown(env);
		break;
	case SIGHUP:
		reconfigure();
		break;
	default:
		fatalx("unexpected signal");
	}
}

/* __dead is for lint */
__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] [-f file]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int			 c;
	int			 debug;
	u_int32_t		 opts;
	struct relayd		*env;
	const char		*conffile;
	struct event		 ev_sigint;
	struct event		 ev_sigterm;
	struct event		 ev_sigchld;
	struct event		 ev_sighup;
	struct imsgev		*iev;

	opts = 0;
	debug = 0;
	conffile = CONF_FILE;

	log_init(1);	/* log to stderr until daemonized */

	while ((c = getopt(argc, argv, "dD:nf:v")) != -1) {
		switch (c) {
		case 'd':
			debug = 2;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'n':
			debug = 2;
			opts |= RELAYD_OPT_NOACTION;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'v':
			opts |= RELAYD_OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

	if ((env = parse_config(conffile, opts)) == NULL)
		exit(1);
	relayd_env = env;

	if (env->sc_opts & RELAYD_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}
	if (debug)
		env->sc_opts |= RELAYD_OPT_LOGUPDATE;

	if (geteuid())
		errx(1, "need root privileges");

	if (getpwnam(RELAYD_USER) == NULL)
		errx(1, "unknown user %s", RELAYD_USER);

	log_init(debug);

	if (!debug) {
		if (daemon(1, 0) == -1)
			err(1, "failed to daemonize");
	}

	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
	    pipe_parent2pfe) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
	    pipe_parent2hce) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
	    pipe_pfe2hce) == -1)
		fatal("socketpair");
	for (c = 0; c < env->sc_prefork_relay; c++) {
		if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
		    pipe_parent2relay[c]) == -1)
			fatal("socketpair");
		if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
		    pipe_pfe2relay[c]) == -1)
			fatal("socketpair");
		session_socket_blockmode(pipe_pfe2relay[c][0], BM_NONBLOCK);
		session_socket_blockmode(pipe_pfe2relay[c][1], BM_NONBLOCK);
		session_socket_blockmode(pipe_parent2relay[c][0], BM_NONBLOCK);
		session_socket_blockmode(pipe_parent2relay[c][1], BM_NONBLOCK);
	}

	session_socket_blockmode(pipe_parent2pfe[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2pfe[1], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2hce[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2hce[1], BM_NONBLOCK);
	session_socket_blockmode(pipe_pfe2hce[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_pfe2hce[1], BM_NONBLOCK);

	pfe_pid = pfe(env, pipe_parent2pfe, pipe_parent2hce,
	    pipe_parent2relay, pipe_pfe2hce, pipe_pfe2relay);
	hce_pid = hce(env, pipe_parent2pfe, pipe_parent2hce,
	    pipe_parent2relay, pipe_pfe2hce, pipe_pfe2relay);
	if (env->sc_prefork_relay > 0)
		relay_pid = relay(env, pipe_parent2pfe, pipe_parent2hce,
		    pipe_parent2relay, pipe_pfe2hce, pipe_pfe2relay);

	setproctitle("parent");

	event_init();

	signal_set(&ev_sigint, SIGINT, main_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, main_sig_handler, env);
	signal_set(&ev_sigchld, SIGCHLD, main_sig_handler, env);
	signal_set(&ev_sighup, SIGHUP, main_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	close(pipe_parent2pfe[1]);
	close(pipe_parent2hce[1]);
	close(pipe_pfe2hce[0]);
	close(pipe_pfe2hce[1]);
	for (c = 0; c < env->sc_prefork_relay; c++) {
		close(pipe_pfe2relay[c][0]);
		close(pipe_pfe2relay[c][1]);
		close(pipe_parent2relay[c][0]);
	}

	if ((iev_pfe = calloc(1, sizeof(struct imsgev))) == NULL ||
	    (iev_hce = calloc(1, sizeof(struct imsgev))) == NULL)
		fatal(NULL);

	if (env->sc_prefork_relay > 0) {
		if ((iev_relay = calloc(env->sc_prefork_relay,
		    sizeof(struct imsgev))) == NULL)
			fatal(NULL);
	}

	imsg_init(&iev_pfe->ibuf, pipe_parent2pfe[0]);
	imsg_init(&iev_hce->ibuf, pipe_parent2hce[0]);
	iev_pfe->handler = main_dispatch_pfe;
	iev_pfe->data = env;
	iev_hce->handler = main_dispatch_hce;
	iev_hce->data = env;

	for (c = 0; c < env->sc_prefork_relay; c++) {
		iev = &iev_relay[c];
		imsg_init(&iev->ibuf, pipe_parent2relay[c][1]);
		iev->handler = main_dispatch_relay;
		iev->events = EV_READ;
		event_set(&iev->ev, iev->ibuf.fd, iev->events,
		    iev->handler, iev);
		event_add(&iev->ev, NULL);
	}

	iev_pfe->events = EV_READ;
	event_set(&iev_pfe->ev, iev_pfe->ibuf.fd, iev_pfe->events,
	    iev_pfe->handler, iev_pfe);
	event_add(&iev_pfe->ev, NULL);

	iev_hce->events = EV_READ;
	event_set(&iev_hce->ev, iev_hce->ibuf.fd, iev_hce->events,
	    iev_hce->handler, iev_hce);
	event_add(&iev_hce->ev, NULL);

	if (env->sc_flags & F_DEMOTE)
		carp_demote_reset(env->sc_demote_group, 0);

	init_routes(env);

	event_dispatch();

	return (0);
}

void
main_shutdown(struct relayd *env)
{
	pid_t	pid;

	if (pfe_pid)
		kill(pfe_pid, SIGTERM);
	if (hce_pid)
		kill(hce_pid, SIGTERM);
	if (relay_pid)
		kill(relay_pid, SIGTERM);

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	control_cleanup();
	carp_demote_shutdown();
	if (env->sc_flags & F_DEMOTE)
		carp_demote_reset(env->sc_demote_group, 128);
	log_info("terminating");
	exit(0);
}

int
check_child(pid_t pid, const char *pname)
{
	int	status;

	if (waitpid(pid, &status, WNOHANG) > 0) {
		if (WIFEXITED(status)) {
			log_warnx("check_child: lost child: %s exited", pname);
			return (1);
		}
		if (WIFSIGNALED(status)) {
			log_warnx("check_child: lost child: %s terminated; "
			    "signal %d", pname, WTERMSIG(status));
			return (1);
		}
	}

	return (0);
}

int
send_all(struct relayd *env, enum imsg_type type, void *buf, u_int16_t len)
{
	int		 i;

	if (imsg_compose_event(iev_pfe, type, 0, 0, -1, buf, len) == -1)
		return (-1);
	if (imsg_compose_event(iev_hce, type, 0, 0, -1, buf, len) == -1)
		return (-1);
	for (i = 0; i < env->sc_prefork_relay; i++) {
		if (imsg_compose_event(&iev_relay[i], type, 0, 0, -1, buf, len)
		    == -1)
			return (-1);
	}
	return (0);
}

void
merge_config(struct relayd *env, struct relayd *new_env)
{
	env->sc_opts = new_env->sc_opts;
	env->sc_flags = new_env->sc_flags;
	env->sc_confpath = new_env->sc_confpath;
	env->sc_tablecount = new_env->sc_tablecount;
	env->sc_rdrcount = new_env->sc_rdrcount;
	env->sc_protocount = new_env->sc_protocount;
	env->sc_relaycount = new_env->sc_relaycount;

	memcpy(&env->sc_interval, &new_env->sc_interval,
	    sizeof(env->sc_interval));
	memcpy(&env->sc_timeout, &new_env->sc_timeout,
	    sizeof(env->sc_timeout));
	memcpy(&env->sc_empty_table, &new_env->sc_empty_table,
	    sizeof(env->sc_empty_table));
	memcpy(&env->sc_proto_default, &new_env->sc_proto_default,
	    sizeof(env->sc_proto_default));
	env->sc_prefork_relay = new_env->sc_prefork_relay;
	(void)strlcpy(env->sc_demote_group, new_env->sc_demote_group,
	    sizeof(env->sc_demote_group));

	env->sc_tables = new_env->sc_tables;
	env->sc_rdrs = new_env->sc_rdrs;
	env->sc_relays = new_env->sc_relays;
	env->sc_protos = new_env->sc_protos;
}


void
reconfigure(void)
{
	struct relayd		*env = relayd_env;
	struct relayd		*new_env = NULL;
	struct rdr		*rdr;
	struct address		*virt;
	struct table            *table;
	struct host             *host;

	log_info("reloading configuration");
	if ((new_env = parse_config(env->sc_confpath, env->sc_opts)) == NULL) {
		log_warnx("configuration reloading FAILED");
		return;
	}

	if (!(env->sc_flags & F_NEEDPF) && (new_env->sc_flags & F_NEEDPF)) {
		log_warnx("new configuration requires pf while it "
		    "was previously disabled."
		    "configuration will not be reloaded");
		purge_config(new_env, PURGE_EVERYTHING);
		free(new_env);
		return;
	}

	purge_config(env, PURGE_EVERYTHING);
	merge_config(env, new_env);
	free(new_env);
	log_info("configuration merge done");

	/*
	 * first reconfigure pfe
	 */
	imsg_compose_event(iev_pfe, IMSG_RECONF, 0, 0, -1, env, sizeof(*env));
	TAILQ_FOREACH(table, env->sc_tables, entry) {
		imsg_compose_event(iev_pfe, IMSG_RECONF_TABLE, 0, 0, -1,
		    &table->conf, sizeof(table->conf));
		TAILQ_FOREACH(host, &table->hosts, entry) {
			imsg_compose_event(iev_pfe, IMSG_RECONF_HOST, 0, 0, -1,
			    &host->conf, sizeof(host->conf));
		}
	}
	TAILQ_FOREACH(rdr, env->sc_rdrs, entry) {
		imsg_compose_event(iev_pfe, IMSG_RECONF_RDR, 0, 0, -1,
		    &rdr->conf, sizeof(rdr->conf));
		TAILQ_FOREACH(virt, &rdr->virts, entry)
			imsg_compose_event(iev_pfe, IMSG_RECONF_VIRT, 0, 0, -1,
				virt, sizeof(*virt));
	}
	imsg_compose_event(iev_pfe, IMSG_RECONF_END, 0, 0, -1, NULL, 0);

	/*
	 * then reconfigure hce
	 */
	imsg_compose_event(iev_hce, IMSG_RECONF, 0, 0, -1, env, sizeof(*env));
	TAILQ_FOREACH(table, env->sc_tables, entry) {
		imsg_compose_event(iev_hce, IMSG_RECONF_TABLE, 0, 0, -1,
		    &table->conf, sizeof(table->conf));
		if (table->sendbuf != NULL)
			imsg_compose_event(iev_hce, IMSG_RECONF_SENDBUF,
			    0, 0, -1, table->sendbuf,
			    strlen(table->sendbuf) + 1);
		TAILQ_FOREACH(host, &table->hosts, entry) {
			imsg_compose_event(iev_hce, IMSG_RECONF_HOST, 0, 0, -1,
			    &host->conf, sizeof(host->conf));
		}
	}
	imsg_compose_event(iev_hce, IMSG_RECONF_END, 0, 0, -1, NULL, 0);
}

void
purge_config(struct relayd *env, u_int8_t what)
{
	struct table		*table;
	struct rdr		*rdr;
	struct address		*virt;
	struct protocol		*proto;
	struct relay		*rlay;
	struct rsession		*sess;

	if (what & PURGE_TABLES && env->sc_tables != NULL) {
		while ((table = TAILQ_FIRST(env->sc_tables)) != NULL)
			purge_table(env->sc_tables, table);
		free(env->sc_tables);
		env->sc_tables = NULL;
	}

	if (what & PURGE_RDRS && env->sc_rdrs != NULL) {
		while ((rdr = TAILQ_FIRST(env->sc_rdrs)) != NULL) {
			TAILQ_REMOVE(env->sc_rdrs, rdr, entry);
			while ((virt = TAILQ_FIRST(&rdr->virts)) != NULL) {
				TAILQ_REMOVE(&rdr->virts, virt, entry);
				free(virt);
			}
			free(rdr);
		}
		free(env->sc_rdrs);
		env->sc_rdrs = NULL;
	}

	if (what & PURGE_RELAYS && env->sc_relays != NULL) {
		while ((rlay = TAILQ_FIRST(env->sc_relays)) != NULL) {
			TAILQ_REMOVE(env->sc_relays, rlay, rl_entry);
			while ((sess =
			    SPLAY_ROOT(&rlay->rl_sessions)) != NULL) {
				SPLAY_REMOVE(session_tree,
				    &rlay->rl_sessions, sess);
				free(sess);
			}
			if (rlay->rl_bev != NULL)
				bufferevent_free(rlay->rl_bev);
			if (rlay->rl_dstbev != NULL)
				bufferevent_free(rlay->rl_dstbev);
			if (rlay->rl_ssl_ctx != NULL)
				SSL_CTX_free(rlay->rl_ssl_ctx);
			free(rlay);
		}
		free(env->sc_relays);
		env->sc_relays = NULL;
	}

	if (what & PURGE_PROTOS && env->sc_protos != NULL) {
		while ((proto = TAILQ_FIRST(env->sc_protos)) != NULL) {
			TAILQ_REMOVE(env->sc_protos, proto, entry);
			purge_tree(&proto->request_tree);
			purge_tree(&proto->response_tree);
			if (proto->style != NULL)
				free(proto->style);
			free(proto);
		}
		free(env->sc_protos);
		env->sc_protos = NULL;
	}
}

void
purge_tree(struct proto_tree *tree)
{
	struct protonode	*proot, *pn;

	while ((proot = RB_ROOT(tree)) != NULL) {
		RB_REMOVE(proto_tree, tree, proot);
		if (proot->key != NULL)
			free(proot->key);
		if (proot->value != NULL)
			free(proot->value);
		while ((pn = SIMPLEQ_FIRST(&proot->head)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(&proot->head, entry);
			if (pn->key != NULL)
				free(pn->key);
			if (pn->value != NULL)
				free(pn->value);
			if (pn->label != 0)
				pn_unref(pn->label);
			free(pn);
		}
		free(proot);
	}
}

void
purge_table(struct tablelist *head, struct table *table)
{
	struct host		*host;

	while ((host = TAILQ_FIRST(&table->hosts)) != NULL) {
		TAILQ_REMOVE(&table->hosts, host, entry);
		if (host->cte.ssl != NULL)
			SSL_free(host->cte.ssl);
		free(host);
	}
	if (table->sendbuf != NULL)
		free(table->sendbuf);
	if (table->conf.flags & F_SSL)
		SSL_CTX_free(table->ssl_ctx);

	if (head != NULL)
		TAILQ_REMOVE(head, table, entry);
	free(table);
}

void
imsg_event_add(struct imsgev *iev)
{
	if (iev->handler == NULL) {
		imsg_flush(&iev->ibuf);
		return;
	}

	iev->events = EV_READ;
	if (iev->ibuf.w.queued)
		iev->events |= EV_WRITE;

	event_del(&iev->ev);
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev);
	event_add(&iev->ev, NULL);
}

int
imsg_compose_event(struct imsgev *iev, u_int16_t type,
    u_int32_t peerid, pid_t pid, int fd, void *data, u_int16_t datalen)
{
	int	ret;

	if ((ret = imsg_compose(&iev->ibuf, type, peerid,
	    pid, fd, data, datalen)) != -1)
		imsg_event_add(iev);
	return (ret);
}

void
main_dispatch_pfe(int fd, short event, void *ptr)
{
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	struct ctl_demote	 demote;
	struct ctl_netroute	 crt;
	struct relayd		*env;
	int			 verbose;

	iev = ptr;
	ibuf = &iev->ibuf;
	env = (struct relayd *)iev->data;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("main_dispatch_pfe: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_DEMOTE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(demote))
				fatalx("main_dispatch_pfe: "
				    "invalid size of demote request");
			memcpy(&demote, imsg.data, sizeof(demote));
			carp_demote_set(demote.group, demote.level);
			break;
		case IMSG_RTMSG:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(crt))
				fatalx("main_dispatch_pfe: "
				    "invalid size of rtmsg request");
			memcpy(&crt, imsg.data, sizeof(crt));
			pfe_route(env, &crt);
			break;
		case IMSG_CTL_RELOAD:
			/*
			 * so far we only get here if no L7 (relay) is done.
			 */
			reconfigure();
			break;
		case IMSG_CTL_LOG_VERBOSE:
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		default:
			log_debug("main_dispatch_pfe: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
main_dispatch_hce(int fd, short event, void * ptr)
{
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	struct ctl_script	 scr;
	struct relayd		*env;

	env = relayd_env;
	iev = ptr;
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("main_dispatch_hce: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_SCRIPT:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(scr))
				fatalx("main_dispatch_hce: "
				    "invalid size of script request");
			bcopy(imsg.data, &scr, sizeof(scr));
			scr.retval = script_exec(env, &scr);
			imsg_compose_event(iev_hce, IMSG_SCRIPT,
			    0, 0, -1, &scr, sizeof(scr));
			break;
		case IMSG_SNMPSOCK:
			(void)snmp_sendsock(iev);
			break;
		default:
			log_debug("main_dispatch_hce: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
main_dispatch_relay(int fd, short event, void * ptr)
{
	struct relayd		*env = relayd_env;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	struct ctl_bindany	 bnd;
	int			 s;

	iev = ptr;
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("main_dispatch_relay: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_BINDANY:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(bnd))
				fatalx("invalid imsg header len");
			bcopy(imsg.data, &bnd, sizeof(bnd));
			if (bnd.bnd_proc > env->sc_prefork_relay)
				fatalx("pfe_dispatch_relay: "
				    "invalid relay proc");
			switch (bnd.bnd_proto) {
			case IPPROTO_TCP:
			case IPPROTO_UDP:
				break;
			default:
				fatalx("pfe_dispatch_relay: requested socket "
				    "for invalid protocol");
				/* NOTREACHED */
			}
			s = bindany(&bnd);
			imsg_compose_event(&iev_relay[bnd.bnd_proc],
			    IMSG_BINDANY,
			    0, 0, s, &bnd.bnd_id, sizeof(bnd.bnd_id));
			break;
		default:
			log_debug("main_dispatch_relay: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

struct host *
host_find(struct relayd *env, objid_t id)
{
	struct table	*table;
	struct host	*host;

	TAILQ_FOREACH(table, env->sc_tables, entry)
		TAILQ_FOREACH(host, &table->hosts, entry)
			if (host->conf.id == id)
				return (host);
	return (NULL);
}

struct table *
table_find(struct relayd *env, objid_t id)
{
	struct table	*table;

	TAILQ_FOREACH(table, env->sc_tables, entry)
		if (table->conf.id == id)
			return (table);
	return (NULL);
}

struct rdr *
rdr_find(struct relayd *env, objid_t id)
{
	struct rdr	*rdr;

	TAILQ_FOREACH(rdr, env->sc_rdrs, entry)
		if (rdr->conf.id == id)
			return (rdr);
	return (NULL);
}

struct relay *
relay_find(struct relayd *env, objid_t id)
{
	struct relay	*rlay;

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry)
		if (rlay->rl_conf.id == id)
			return (rlay);
	return (NULL);
}

struct rsession *
session_find(struct relayd *env, objid_t id)
{
	struct relay		*rlay;
	struct rsession		*con;

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry)
		SPLAY_FOREACH(con, session_tree, &rlay->rl_sessions)
			if (con->se_id == id)
				return (con);
	return (NULL);
}

struct netroute *
route_find(struct relayd *env, objid_t id)
{
	struct netroute	*nr;

	TAILQ_FOREACH(nr, env->sc_routes, nr_route)
		if (nr->nr_conf.id == id)
			return (nr);
	return (NULL);
}

struct host *
host_findbyname(struct relayd *env, const char *name)
{
	struct table	*table;
	struct host	*host;

	TAILQ_FOREACH(table, env->sc_tables, entry)
		TAILQ_FOREACH(host, &table->hosts, entry)
			if (strcmp(host->conf.name, name) == 0)
				return (host);
	return (NULL);
}

struct table *
table_findbyname(struct relayd *env, const char *name)
{
	struct table	*table;

	TAILQ_FOREACH(table, env->sc_tables, entry)
		if (strcmp(table->conf.name, name) == 0)
			return (table);
	return (NULL);
}

struct table *
table_findbyconf(struct relayd *env, struct table *tb)
{
	struct table		*table;
	struct table_config	 a, b;

	bcopy(&tb->conf, &a, sizeof(a));
	a.id = a.rdrid = 0;
	a.flags &= ~(F_USED|F_BACKUP);

	TAILQ_FOREACH(table, env->sc_tables, entry) {
		bcopy(&table->conf, &b, sizeof(b));
		b.id = b.rdrid = 0;
		b.flags &= ~(F_USED|F_BACKUP);

		/*
		 * Compare two tables and return the existing table if
		 * the configuration seems to be the same.
		 */
		if (bcmp(&a, &b, sizeof(b)) == 0 &&
		    ((tb->sendbuf == NULL && table->sendbuf == NULL) ||
		    (tb->sendbuf != NULL && table->sendbuf != NULL &&
		    strcmp(tb->sendbuf, table->sendbuf) == 0)))
			return (table);
	}
	return (NULL);
}

struct rdr *
rdr_findbyname(struct relayd *env, const char *name)
{
	struct rdr	*rdr;

	TAILQ_FOREACH(rdr, env->sc_rdrs, entry)
		if (strcmp(rdr->conf.name, name) == 0)
			return (rdr);
	return (NULL);
}

struct relay *
relay_findbyname(struct relayd *env, const char *name)
{
	struct relay	*rlay;

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry)
		if (strcmp(rlay->rl_conf.name, name) == 0)
			return (rlay);
	return (NULL);
}

struct relay *
relay_findbyaddr(struct relayd *env, struct relay_config *rc)
{
	struct relay	*rlay;

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry)
		if (bcmp(&rlay->rl_conf.ss, &rc->ss, sizeof(rc->ss)) == 0 &&
		    rlay->rl_conf.port == rc->port)
			return (rlay);
	return (NULL);
}

void
event_again(struct event *ev, int fd, short event,
    void (*fn)(int, short, void *),
    struct timeval *start, struct timeval *end, void *arg)
{
	struct timeval tv_next, tv_now, tv;

	if (gettimeofday(&tv_now, NULL) == -1)
		fatal("event_again: gettimeofday");

	bcopy(end, &tv_next, sizeof(tv_next));
	timersub(&tv_now, start, &tv_now);
	timersub(&tv_next, &tv_now, &tv_next);

	bzero(&tv, sizeof(tv));
	if (timercmp(&tv_next, &tv, >))
		bcopy(&tv_next, &tv, sizeof(tv));

	event_set(ev, fd, event, fn, arg);
	event_add(ev, &tv);
}

int
expand_string(char *label, size_t len, const char *srch, const char *repl)
{
	char *tmp;
	char *p, *q;

	if ((tmp = calloc(1, len)) == NULL) {
		log_debug("expand_string: calloc");
		return (-1);
	}
	p = q = label;
	while ((q = strstr(p, srch)) != NULL) {
		*q = '\0';
		if ((strlcat(tmp, p, len) >= len) ||
		    (strlcat(tmp, repl, len) >= len)) {
			log_debug("expand_string: string too long");
			return (-1);
		}
		q += strlen(srch);
		p = q;
	}
	if (strlcat(tmp, p, len) >= len) {
		log_debug("expand_string: string too long");
		return (-1);
	}
	(void)strlcpy(label, tmp, len);	/* always fits */
	free(tmp);

	return (0);
}

void
translate_string(char *str)
{
	char	*reader;
	char	*writer;

	reader = writer = str;

	while (*reader) {
		if (*reader == '\\') {
			reader++;
			switch (*reader) {
			case 'n':
				*writer++ = '\n';
				break;
			case 'r':
				*writer++ = '\r';
				break;
			default:
				*writer++ = *reader;
			}
		} else
			*writer++ = *reader;
		reader++;
	}
	*writer = '\0';
}

char *
digeststr(enum digest_type type, const u_int8_t *data, size_t len, char *buf)
{
	switch (type) {
	case DIGEST_SHA1:
		return (SHA1Data(data, len, buf));
		break;
	case DIGEST_MD5:
		return (MD5Data(data, len, buf));
		break;
	default:
		break;
	}
	return (NULL);
}

const char *
canonicalize_host(const char *host, char *name, size_t len)
{
	struct sockaddr_in	 sin4;
	struct sockaddr_in6	 sin6;
	u_int			 i, j;
	size_t			 plen;
	char			 c;

	if (len < 2)
		goto fail;

	/*
	 * Canonicalize an IPv4/6 address
	 */
	if (inet_pton(AF_INET, host, &sin4) == 1)
		return (inet_ntop(AF_INET, &sin4, name, len));
	if (inet_pton(AF_INET6, host, &sin6) == 1)
		return (inet_ntop(AF_INET6, &sin6, name, len));

	/*
	 * Canonicalize a hostname
	 */

	/* 1. remove repeated dots and convert upper case to lower case */	
	plen = strlen(host);
	bzero(name, len);
	for (i = j = 0; i < plen; i++) {
		if (j >= (len - 1))
			goto fail;
		c = tolower(host[i]);
		if ((c == '.') && (j == 0 || name[j - 1] == '.'))
			continue;
		name[j++] = c;
	}

	/* 2. remove trailing dots */
	for (i = j; i > 0; i--) {
		if (name[i - 1] != '.')
			break;
		name[i - 1] = '\0';
		j--;
	}
	if (j <= 0)
		goto fail;

	return (name);

 fail:
	errno = EINVAL;
	return (NULL);
}

struct protonode *
protonode_header(enum direction dir, struct protocol *proto,
    struct protonode *pk)
{
	struct protonode	*pn;
	struct proto_tree	*tree;

	if (dir == RELAY_DIR_RESPONSE)
		tree = &proto->response_tree;
	else
		tree = &proto->request_tree;

	pn = RB_FIND(proto_tree, tree, pk);
	if (pn != NULL)
		return (pn);
	if ((pn = (struct protonode *)calloc(1, sizeof(*pn))) == NULL) {
		log_warn("out of memory");
		return (NULL);
	}
	pn->key = strdup(pk->key);
	if (pn->key == NULL) {
		free(pn);
		log_warn("out of memory");
		return (NULL);
	}
	pn->value = NULL;
	pn->action = NODE_ACTION_NONE;
	pn->type = pk->type;
	SIMPLEQ_INIT(&pn->head);
	if (dir == RELAY_DIR_RESPONSE)
		pn->id =
		    proto->response_nodes++;
	else
		pn->id = proto->request_nodes++;
	if (pn->id == INT_MAX) {
		log_warnx("too many protocol "
		    "nodes defined");
		return (NULL);
	}
	RB_INSERT(proto_tree, tree, pn);
	return (pn);
}

int
protonode_add(enum direction dir, struct protocol *proto,
    struct protonode *node)
{
	struct protonode	*pn, *proot, pk;
	struct proto_tree	*tree;

	if (dir == RELAY_DIR_RESPONSE)
		tree = &proto->response_tree;
	else
		tree = &proto->request_tree;

	if ((pn = calloc(1, sizeof (*pn))) == NULL) {
		log_warn("out of memory");
		return (-1);
	}
	bcopy(node, pn, sizeof(*pn));
	pn->key = node->key;
	pn->value = node->value;
	SIMPLEQ_INIT(&pn->head);
	if (dir == RELAY_DIR_RESPONSE)
		pn->id = proto->response_nodes++;
	else
		pn->id = proto->request_nodes++;
	if (pn->id == INT_MAX) {
		log_warnx("too many protocol nodes defined");
		free(pn);
		return (-1);
	}
	if ((proot =
	    RB_INSERT(proto_tree, tree, pn)) != NULL) {
		/*
		 * A protocol node with the same key already
		 * exists, append it to a queue behind the
		 * existing node->
		 */
		if (SIMPLEQ_EMPTY(&proot->head))
			SIMPLEQ_NEXT(proot, entry) = pn;
		SIMPLEQ_INSERT_TAIL(&proot->head, pn, entry);
	}

	if (node->type == NODE_TYPE_COOKIE)
		pk.key = "Cookie";
	else if (node->type == NODE_TYPE_URL)
		pk.key = "Host";
	else
		pk.key = "GET";
	if (node->type != NODE_TYPE_HEADER) {
		pk.type = NODE_TYPE_HEADER;
		pn = protonode_header(dir, proto, &pk);
		if (pn == NULL)
			return (-1);
		switch (node->type) {
		case NODE_TYPE_QUERY:
			pn->flags |= PNFLAG_LOOKUP_QUERY;
			break;
		case NODE_TYPE_COOKIE:
			pn->flags |= PNFLAG_LOOKUP_COOKIE;
			break;
		case NODE_TYPE_URL:
			if (node->flags &
			    PNFLAG_LOOKUP_URL_DIGEST)
				pn->flags |= node->flags &
				    PNFLAG_LOOKUP_URL_DIGEST;
			else
				pn->flags |=
				    PNFLAG_LOOKUP_DIGEST(0);
			break;
		default:
			break;
		}
	}

	return (0);
}

int
protonode_load(enum direction dir, struct protocol *proto,
    struct protonode *node, const char *name)
{
	FILE			*fp;
	char			 buf[BUFSIZ];
	int			 ret = -1;
	struct protonode	 pn;

	bcopy(node, &pn, sizeof(pn));
	pn.key = pn.value = NULL;

	if ((fp = fopen(name, "r")) == NULL)
		return (-1);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* strip whitespace and newline characters */
		buf[strcspn(buf, "\r\n\t ")] = '\0';
		if (!strlen(buf) || buf[0] == '#')
			continue;
		pn.key = strdup(buf);
		if (node->value != NULL)
			pn.value = strdup(node->value);
		if (pn.key == NULL ||
		    (node->value != NULL && pn.value == NULL))
			goto fail;
		if (protonode_add(dir, proto, &pn) == -1)
			goto fail;
		pn.key = pn.value = NULL;
	}

	ret = 0;
 fail:
	if (pn.key != NULL)
		free(pn.key);
	if (pn.value != NULL)
		free(pn.value);
	fclose(fp);
	return (ret);
}

int
bindany(struct ctl_bindany *bnd)
{
	int	s, v;

	s = -1;
	v = 1;

	if (relay_socket_af(&bnd->bnd_ss, bnd->bnd_port) == -1)
		goto fail;
	if ((s = socket(bnd->bnd_ss.ss_family,
	    bnd->bnd_proto == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM,
	    bnd->bnd_proto)) == -1)
		goto fail;
	if (setsockopt(s, SOL_SOCKET, SO_BINDANY,
	    &v, sizeof(v)) == -1)
		goto fail;
	if (bind(s, (struct sockaddr *)&bnd->bnd_ss,
	    bnd->bnd_ss.ss_len) == -1)
		goto fail;

	return (s);

 fail:
	if (s != -1)
		close(s);
	return (-1);
}

int
map6to4(struct sockaddr_storage *in6)
{
	struct sockaddr_storage	 out4;
	struct sockaddr_in	*sin4 = (struct sockaddr_in *)&out4;
	struct sockaddr_in6	*sin6 = (struct sockaddr_in6 *)in6;

	bzero(sin4, sizeof(*sin4));
	sin4->sin_len = sizeof(*sin4);
	sin4->sin_family = AF_INET;
	sin4->sin_port = sin6->sin6_port;

	bcopy(&sin6->sin6_addr.s6_addr[12], &sin4->sin_addr.s_addr,
	    sizeof(sin4->sin_addr));

	if (sin4->sin_addr.s_addr == INADDR_ANY ||
	    sin4->sin_addr.s_addr == INADDR_BROADCAST ||
	    IN_MULTICAST(ntohl(sin4->sin_addr.s_addr)))
		return (-1);

	bcopy(&out4, in6, sizeof(*in6));

	return (0);
}

int
map4to6(struct sockaddr_storage *in4, struct sockaddr_storage *map)
{
	struct sockaddr_storage	 out6;
	struct sockaddr_in	*sin4 = (struct sockaddr_in *)in4;
	struct sockaddr_in6	*sin6 = (struct sockaddr_in6 *)&out6;
	struct sockaddr_in6	*map6 = (struct sockaddr_in6 *)map;

	if (sin4->sin_addr.s_addr == INADDR_ANY ||
	    sin4->sin_addr.s_addr == INADDR_BROADCAST ||
	    IN_MULTICAST(ntohl(sin4->sin_addr.s_addr)))
		return (-1);

	bcopy(map6, sin6, sizeof(*sin6));
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = sin4->sin_port;

	bcopy(&sin4->sin_addr.s_addr, &sin6->sin6_addr.s6_addr[12],
	    sizeof(sin4->sin_addr));

	bcopy(&out6, in4, sizeof(*in4));

	return (0);
}
