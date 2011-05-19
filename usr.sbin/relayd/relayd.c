/*	$OpenBSD: relayd.c,v 1.103 2011/05/19 08:56:49 reyk Exp $	*/

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
#include <sys/resource.h>
#include <sys/hash.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <sha1.h>
#include <md5.h>

#include <openssl/ssl.h>

#include "relayd.h"

__dead void	 usage(void);

int		 parent_configure(struct relayd *);
void		 parent_reload(struct relayd *, u_int, const char *);
void		 parent_sig_handler(int, short, void *);
void		 parent_shutdown(struct relayd *);
int		 parent_dispatch_pfe(int, struct privsep_proc *, struct imsg *);
int		 parent_dispatch_hce(int, struct privsep_proc *, struct imsg *);
int		 parent_dispatch_relay(int, struct privsep_proc *, struct imsg *);
int		 bindany(struct ctl_bindany *);

struct relayd			*relayd_env;

static struct privsep_proc procs[] = {
	{ "pfe",	PROC_PFE, parent_dispatch_pfe, pfe },
	{ "hce",	PROC_HCE, parent_dispatch_hce, hce },
	{ "relay",	PROC_RELAY, parent_dispatch_relay, relay }
};

void
parent_sig_handler(int sig, short event, void *arg)
{
	struct privsep	*ps = arg;
	int		 die = 0, status, fail, id;
	pid_t		 pid;
	char		*cause;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		do {
			pid = waitpid(WAIT_ANY, &status, WNOHANG);
			if (pid <= 0)
				continue;

			fail = 0;
			if (WIFSIGNALED(status)) {
				fail = 1;
				asprintf(&cause, "terminated; signal %d",
				    WTERMSIG(status));
			} else if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					fail = 1;
					asprintf(&cause, "exited abnormally");
				} else
					asprintf(&cause, "exited okay");
			} else
				fatalx("unexpected cause of SIGCHLD");

			die = 1;

			for (id = 0; id < PROC_MAX; id++)
				if (pid == ps->ps_pid[id]) {
					log_warnx("lost child: %s %s",
					    ps->ps_title[id], cause);
					break;
				}

			free(cause);
		} while (pid > 0 || (pid == -1 && errno == EINTR));

		if (die)
			parent_shutdown(ps->ps_env);
		break;
	case SIGHUP:
		log_info("%s: reload requested with SIGHUP", __func__);

		/*
		 * This is safe because libevent uses async signal handlers
		 * that run in the event loop and not in signal context.
		 */
		parent_reload(ps->ps_env, CONFIG_RELOAD, NULL);
		break;
	case SIGPIPE:
		/* ignore */
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
	int			 debug = 0, verbose = 0;
	u_int32_t		 opts = 0;
	struct relayd		*env;
	struct privsep		*ps;
	const char		*conffile = CONF_FILE;

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
			verbose++;
			opts |= RELAYD_OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	log_init(debug ? debug : 1);	/* log to stderr until daemonized */

	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

	if ((env = calloc(1, sizeof(*env))) == NULL ||
	    (ps = calloc(1, sizeof(*ps))) == NULL)
		exit(1);

	relayd_env = env;
	env->sc_ps = ps;
	ps->ps_env = env;
	env->sc_conffile = conffile;
	env->sc_opts = opts;

	if (parse_config(env->sc_conffile, env) == -1)
		exit(1);

	if (debug)
		env->sc_opts |= RELAYD_OPT_LOGUPDATE;

	if (geteuid())
		errx(1, "need root privileges");

	if ((ps->ps_pw =  getpwnam(RELAYD_USER)) == NULL)
		errx(1, "unknown user %s", RELAYD_USER);

	/* Configure the control socket */
	ps->ps_csock.cs_name = RELAYD_SOCKET;

	log_init(debug);
	log_verbose(verbose);

	if (!debug && daemon(1, 0) == -1)
		err(1, "failed to daemonize");

	if (env->sc_opts & RELAYD_OPT_NOACTION)
		ps->ps_noaction = 1;
	else
		log_info("startup");

	ps->ps_instances[PROC_RELAY] = env->sc_prefork_relay;
	proc_init(ps, procs, nitems(procs));

	setproctitle("parent");

	event_init();

	signal_set(&ps->ps_evsigint, SIGINT, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigterm, SIGTERM, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigchld, SIGCHLD, parent_sig_handler, ps);
	signal_set(&ps->ps_evsighup, SIGHUP, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigpipe, SIGPIPE, parent_sig_handler, ps);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsigchld, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);

	proc_config(ps, procs, nitems(procs));

	if (load_config(env->sc_conffile, env) == -1) {
		proc_kill(env->sc_ps);
		exit(1);
	}

	if (env->sc_opts & RELAYD_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		proc_kill(env->sc_ps);
		exit(0);
	}

	if (env->sc_flags & (F_SSL|F_SSLCLIENT))
		ssl_init(env);

	if (parent_configure(env) == -1)
		fatalx("configuration failed");

	init_routes(env);

	event_dispatch();

	parent_shutdown(env);
	/* NOTREACHED */

	return (0);
}

int
parent_configure(struct relayd *env)
{
	struct table		*tb;
	struct rdr		*rdr;
	struct router		*rt;
	struct protocol		*proto;
	struct relay		*rlay;
	int			 id;
	struct ctl_flags	 cf;
	int			 s, ret = -1;

	TAILQ_FOREACH(tb, env->sc_tables, entry)
		config_settable(env, tb);
	TAILQ_FOREACH(rdr, env->sc_rdrs, entry)
		config_setrdr(env, rdr);
	TAILQ_FOREACH(rt, env->sc_rts, rt_entry)
		config_setrt(env, rt);
	TAILQ_FOREACH(proto, env->sc_protos, entry)
		config_setproto(env, proto);
	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry)
		config_setrelay(env, rlay);

	for (id = 0; id < PROC_MAX; id++) {
		if (id == privsep_process)
			continue;
		cf.cf_opts = env->sc_opts;
		cf.cf_flags = env->sc_flags;

		if ((env->sc_flags & F_NEEDPF) && id == PROC_PFE) {
			/* Send pf socket to the pf engine */
			if ((s = open(PF_SOCKET, O_RDWR)) == -1) {
				log_debug("%s: cannot open pf socket",
				    __func__);
				goto done;
			}
		} else
			s = -1;

		env->sc_reload++;
		proc_compose_imsg(env->sc_ps, id, -1, IMSG_CFG_DONE, s,
		    &cf, sizeof(cf));
	}

	ret = 0;

 done:
	config_purge(env, CONFIG_ALL);
	return (ret);
}

void
parent_reload(struct relayd *env, u_int reset, const char *filename)
{
	if (env->sc_reload) {
		log_debug("%s: already in progress: %d pending",
		    __func__, env->sc_reload);
		return;
	}

	/* Switch back to the default config file */
	if (filename == NULL || *filename == '\0')
		filename = env->sc_conffile;

	log_debug("%s: level %d config file %s", __func__, reset, filename);

	config_purge(env, CONFIG_ALL);

	if (reset == CONFIG_RELOAD) {
		if (load_config(filename, env) == -1) {
			log_debug("%s: failed to load config file %s",
			    __func__, filename);
		}

		config_setreset(env, CONFIG_ALL);

		if (parent_configure(env) == -1) {
			log_debug("%s: failed to commit config from %s",
			    __func__, filename);
		}
	} else
		config_setreset(env, reset);
}

void
parent_shutdown(struct relayd *env)
{
	config_purge(env, CONFIG_ALL);

	proc_kill(env->sc_ps);
	control_cleanup(&env->sc_ps->ps_csock);
	carp_demote_shutdown();
	if (env->sc_flags & F_DEMOTE)
		carp_demote_reset(env->sc_demote_group, 128);

	free(env->sc_ps);
	free(env);

	log_info("parent terminating, pid %d", getpid());

	exit(0);
}

int
parent_dispatch_pfe(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct relayd		*env = p->p_env;
	struct ctl_demote	 demote;
	struct ctl_netroute	 crt;
	u_int		 	 v;
	char			*str = NULL;

	switch (imsg->hdr.type) {
	case IMSG_DEMOTE:
		IMSG_SIZE_CHECK(imsg, &demote);
		memcpy(&demote, imsg->data, sizeof(demote));
		carp_demote_set(demote.group, demote.level);
		break;
	case IMSG_RTMSG:
		IMSG_SIZE_CHECK(imsg, &crt);
		memcpy(&crt, imsg->data, sizeof(crt));
		pfe_route(env, &crt);
		break;
	case IMSG_CTL_RESET:
		IMSG_SIZE_CHECK(imsg, &v);
		memcpy(&v, imsg->data, sizeof(v));
		parent_reload(env, v, NULL);
		break;
	case IMSG_CTL_RELOAD:
		if (IMSG_DATA_SIZE(imsg) > 0)
			str = get_string(imsg->data, IMSG_DATA_SIZE(imsg));
		parent_reload(env, CONFIG_RELOAD, str);
		if (str != NULL)
			free(str);
		break;
	case IMSG_CTL_SHUTDOWN:
		parent_shutdown(env);
		break;
	case IMSG_CFG_DONE:
		if (env->sc_reload)
			env->sc_reload--;
		break;
	default:
		return (-1);
	}

	return (0);
}

int
parent_dispatch_hce(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct relayd		*env = p->p_env;
	struct privsep		*ps = env->sc_ps;
	struct ctl_script	 scr;

	switch (imsg->hdr.type) {
	case IMSG_SCRIPT:
		IMSG_SIZE_CHECK(imsg, &scr);
		bcopy(imsg->data, &scr, sizeof(scr));
		scr.retval = script_exec(env, &scr);
		proc_compose_imsg(ps, PROC_HCE, -1, IMSG_SCRIPT,
		    -1, &scr, sizeof(scr));
		break;
	case IMSG_SNMPSOCK:
		(void)snmp_setsock(env, p->p_id);
		break;
	case IMSG_CFG_DONE:
		if (env->sc_reload)
			env->sc_reload--;
		break;
	default:
		return (-1);
	}

	return (0);
}

int
parent_dispatch_relay(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct relayd		*env = p->p_env;
	struct privsep		*ps = env->sc_ps;
	struct ctl_bindany	 bnd;
	int			 s;

	switch (imsg->hdr.type) {
	case IMSG_BINDANY:
		IMSG_SIZE_CHECK(imsg, &bnd);
		bcopy(imsg->data, &bnd, sizeof(bnd));
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
		proc_compose_imsg(ps, PROC_RELAY, bnd.bnd_proc,
		    IMSG_BINDANY, s, &bnd.bnd_id, sizeof(bnd.bnd_id));
		break;
	case IMSG_CFG_DONE:
		if (env->sc_reload)
			env->sc_reload--;
		break;
	default:
		return (-1);
	}

	return (0);
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
		if (event_initialized(&host->cte.ev)) {
			event_del(&host->cte.ev);
			close(host->cte.s);
		}
		if (host->cte.buf != NULL)
			ibuf_free(host->cte.buf);
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
purge_relay(struct relayd *env, struct relay *rlay)
{
	struct rsession		*con;

	/* shutdown and remove relay */
	if (event_initialized(&rlay->rl_ev))
		event_del(&rlay->rl_ev);
	close(rlay->rl_s);
	TAILQ_REMOVE(env->sc_relays, rlay, rl_entry);

	/* cleanup sessions */
	while ((con =
	    SPLAY_ROOT(&rlay->rl_sessions)) != NULL)
		relay_close(con, NULL);

	/* cleanup relay */
	if (rlay->rl_bev != NULL)
		bufferevent_free(rlay->rl_bev);
	if (rlay->rl_dstbev != NULL)
		bufferevent_free(rlay->rl_dstbev);

	if (rlay->rl_ssl_ctx != NULL)
		SSL_CTX_free(rlay->rl_ssl_ctx);
	if (rlay->rl_ssl_cert != NULL)
		free(rlay->rl_ssl_cert);
	if (rlay->rl_ssl_key != NULL)
		free(rlay->rl_ssl_key);
	if (rlay->rl_ssl_ca != NULL)
		free(rlay->rl_ssl_ca);

	free(rlay);
}

/*
 * Utility functions
 */

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

struct protocol *
proto_find(struct relayd *env, objid_t id)
{
	struct protocol	*p;

	TAILQ_FOREACH(p, env->sc_protos, entry)
		if (p->id == id)
			return (p);
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

struct router *
router_find(struct relayd *env, objid_t id)
{
	struct router	*rt;

	TAILQ_FOREACH(rt, env->sc_rts, rt_entry)
		if (rt->rt_conf.id == id)
			return (rt);
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

	event_del(ev);
	event_set(ev, fd, event, fn, arg);
	event_add(ev, &tv);
}

int
expand_string(char *label, size_t len, const char *srch, const char *repl)
{
	char *tmp;
	char *p, *q;

	if ((tmp = calloc(1, len)) == NULL) {
		log_debug("%s: calloc", __func__);
		return (-1);
	}
	p = q = label;
	while ((q = strstr(p, srch)) != NULL) {
		*q = '\0';
		if ((strlcat(tmp, p, len) >= len) ||
		    (strlcat(tmp, repl, len) >= len)) {
			log_debug("%s: string too long", __func__);
			return (-1);
		}
		q += strlen(srch);
		p = q;
	}
	if (strlcat(tmp, p, len) >= len) {
		log_debug("%s: string too long", __func__);
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
		log_warn("%s: calloc", __func__);
		return (NULL);
	}
	pn->key = strdup(pk->key);
	if (pn->key == NULL) {
		free(pn);
		log_warn("%s: strdup", __func__);
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
		log_warnx("%s: too many protocol "
		    "nodes defined", __func__);
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
		log_warn("%s: calloc", __func__);
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
		log_warnx("%s: too many protocol nodes defined", __func__);
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

void
socket_rlimit(int maxfd)
{
	struct rlimit	 rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
		fatal("socket_rlimit: failed to get resource limit");
	log_debug("%s: max open files %d", __func__, rl.rlim_max);

	/*
	 * Allow the maximum number of open file descriptors for this
	 * login class (which should be the class "daemon" by default).
	 */
	if (maxfd == -1)
		rl.rlim_cur = rl.rlim_max;
	else
		rl.rlim_cur = MAX(rl.rlim_max, (rlim_t)maxfd);
	if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
		fatal("socket_rlimit: failed to set resource limit");
}

char *
get_string(u_int8_t *ptr, size_t len)
{
	size_t	 i;
	char	*str;

	for (i = 0; i < len; i++)
		if (!(isprint((char)ptr[i]) || isspace((char)ptr[i])))
			break;

	if ((str = calloc(1, i + 1)) == NULL)
		return (NULL);
	memcpy(str, ptr, i);

	return (str);
}

void *
get_data(u_int8_t *ptr, size_t len)
{
	u_int8_t	*data;

	if ((data = calloc(1, len)) == NULL)
		return (NULL);
	memcpy(data, ptr, len);

	return (data);
}
