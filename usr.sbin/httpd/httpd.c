/*	$OpenBSD: httpd.c,v 1.2 2014/07/13 14:17:37 reyk Exp $	*/

/*
 * Copyright (c) 2014 Reyk Floeter <reyk@openbsd.org>
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
#include <fnmatch.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <sha1.h>
#include <md5.h>

#include <openssl/ssl.h>

#include "httpd.h"

__dead void	 usage(void);

int		 parent_configure(struct httpd *);
void		 parent_configure_done(struct httpd *);
void		 parent_reload(struct httpd *, u_int, const char *);
void		 parent_sig_handler(int, short, void *);
void		 parent_shutdown(struct httpd *);
int		 parent_dispatch_server(int, struct privsep_proc *,
		    struct imsg *);

struct httpd			*httpd_env;

static struct privsep_proc procs[] = {
	{ "server",	PROC_SERVER, parent_dispatch_server, server }
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
					if (fail)
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
	struct httpd		*env;
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
			opts |= HTTPD_OPT_NOACTION;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'v':
			verbose++;
			opts |= HTTPD_OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	log_init(debug ? debug : 1);	/* log to stderr until daemonized */

	argc -= optind;
	if (argc > 0)
		usage();

	if ((env = calloc(1, sizeof(*env))) == NULL ||
	    (ps = calloc(1, sizeof(*ps))) == NULL)
		exit(1);

	httpd_env = env;
	env->sc_ps = ps;
	ps->ps_env = env;
	TAILQ_INIT(&ps->ps_rcsocks);
	env->sc_conffile = conffile;
	env->sc_opts = opts;

	if (parse_config(env->sc_conffile, env) == -1)
		exit(1);

	if (debug)
		env->sc_opts |= HTTPD_OPT_LOGUPDATE;

	if (geteuid())
		errx(1, "need root privileges");

	if ((ps->ps_pw =  getpwnam(HTTPD_USER)) == NULL)
		errx(1, "unknown user %s", HTTPD_USER);

	/* Configure the control socket */
	ps->ps_csock.cs_name = HTTPD_SOCKET;

	log_init(debug);
	log_verbose(verbose);

	if (!debug && daemon(1, 0) == -1)
		err(1, "failed to daemonize");

	if (env->sc_opts & HTTPD_OPT_NOACTION)
		ps->ps_noaction = 1;
	else
		log_info("startup");

	ps->ps_instances[PROC_SERVER] = env->sc_prefork_server;
	ps->ps_ninstances = env->sc_prefork_server;

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

	proc_listen(ps, procs, nitems(procs));

	if (load_config(env->sc_conffile, env) == -1) {
		proc_kill(env->sc_ps);
		exit(1);
	}

	if (env->sc_opts & HTTPD_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		proc_kill(env->sc_ps);
		exit(0);
	}

	if (parent_configure(env) == -1)
		fatalx("configuration failed");

	event_dispatch();

	parent_shutdown(env);
	/* NOTREACHED */

	return (0);
}

int
parent_configure(struct httpd *env)
{
	int			 id;
	struct ctl_flags	 cf;
	int			 ret = -1;
	struct server		*srv;
	struct media_type	*media;

	RB_FOREACH(media, mediatypes, env->sc_mediatypes) {
		if (config_setmedia(env, media) == -1)
			fatal("send media");
	}

	TAILQ_FOREACH(srv, env->sc_servers, srv_entry) {
		if (config_setserver(env, srv) == -1)
			fatal("send server");
	}

	/* The servers need to reload their config. */
	env->sc_reload = env->sc_prefork_server;

	for (id = 0; id < PROC_MAX; id++) {
		if (id == privsep_process)
			continue;
		cf.cf_opts = env->sc_opts;
		cf.cf_flags = env->sc_flags;

		proc_compose_imsg(env->sc_ps, id, -1, IMSG_CFG_DONE, -1,
		    &cf, sizeof(cf));
	}

	ret = 0;

	config_purge(env, CONFIG_ALL);
	return (ret);
}

void
parent_reload(struct httpd *env, u_int reset, const char *filename)
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
parent_configure_done(struct httpd *env)
{
	int	 id;

	if (env->sc_reload == 0) {
		log_warnx("%s: configuration already finished", __func__);
		return;
	}

	env->sc_reload--;
	if (env->sc_reload == 0) {
		for (id = 0; id < PROC_MAX; id++) {
			if (id == privsep_process)
				continue;

			proc_compose_imsg(env->sc_ps, id, -1, IMSG_CTL_START,
			    -1, NULL, 0);
		}
	}
}

void
parent_shutdown(struct httpd *env)
{
	config_purge(env, CONFIG_ALL);

	proc_kill(env->sc_ps);
	control_cleanup(&env->sc_ps->ps_csock);

	free(env->sc_ps);
	free(env);

	log_info("parent terminating, pid %d", getpid());

	exit(0);
}

int
parent_dispatch_server(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct httpd		*env = p->p_env;

	switch (imsg->hdr.type) {
	case IMSG_CFG_DONE:
		parent_configure_done(env);
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * Utility functions
 */

void
event_again(struct event *ev, int fd, short event,
    void (*fn)(int, short, void *),
    struct timeval *start, struct timeval *end, void *arg)
{
	struct timeval tv_next, tv_now, tv;

	getmonotime(&tv_now);
	memcpy(&tv_next, end, sizeof(tv_next));
	timersub(&tv_now, start, &tv_now);
	timersub(&tv_next, &tv_now, &tv_next);

	memset(&tv, 0, sizeof(tv));
	if (timercmp(&tv_next, &tv, >))
		memcpy(&tv, &tv_next, sizeof(tv));

	event_del(ev);
	event_set(ev, fd, event, fn, arg);
	event_add(ev, &tv);
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
	memset(name, 0, len);
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

void
socket_rlimit(int maxfd)
{
	struct rlimit	 rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
		fatal("socket_rlimit: failed to get resource limit");
	log_debug("%s: max open files %llu", __func__, rl.rlim_max);

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
		if (!(isprint(ptr[i]) || isspace(ptr[i])))
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

int
accept_reserve(int sockfd, struct sockaddr *addr, socklen_t *addrlen,
    int reserve, volatile int *counter)
{
	int ret;
	if (getdtablecount() + reserve +
	    *counter >= getdtablesize()) {
		errno = EMFILE;
		return (-1);
	}

	if ((ret = accept(sockfd, addr, addrlen)) > -1) {
		(*counter)++;
		DPRINTF("%s: inflight incremented, now %d",__func__, *counter);
	}
	return (ret);
}

struct kv *
kv_add(struct kvtree *keys, char *key, char *value)
{
	struct kv	*kv, *oldkv;

	if (key == NULL)
		return (NULL);
	if ((kv = calloc(1, sizeof(*kv))) == NULL)
		return (NULL);
	if ((kv->kv_key = strdup(key)) == NULL) {
		free(kv);
		return (NULL);
	}
	if (value != NULL &&
	    (kv->kv_value = strdup(value)) == NULL) {
		free(kv->kv_key);
		free(kv);
		return (NULL);
	}
	TAILQ_INIT(&kv->kv_children);

	if ((oldkv = RB_INSERT(kvtree, keys, kv)) != NULL) {
		TAILQ_INSERT_TAIL(&oldkv->kv_children, kv, kv_entry);
		kv->kv_parent = oldkv;
	}

	return (kv);
}

int
kv_set(struct kv *kv, char *fmt, ...)
{
	va_list		  ap;
	char		*value = NULL;
	struct kv	*ckv;

	va_start(ap, fmt);
	if (vasprintf(&value, fmt, ap) == -1)
		return (-1);
	va_end(ap);

	/* Remove all children */
	while ((ckv = TAILQ_FIRST(&kv->kv_children)) != NULL) {
		TAILQ_REMOVE(&kv->kv_children, ckv, kv_entry);
		kv_free(ckv);
		free(ckv);
	}

	/* Set the new value */
	if (kv->kv_value != NULL)
		free(kv->kv_value);
	kv->kv_value = value;

	return (0);
}

int
kv_setkey(struct kv *kv, char *fmt, ...)
{
	va_list  ap;
	char	*key = NULL;

	va_start(ap, fmt);
	if (vasprintf(&key, fmt, ap) == -1)
		return (-1);
	va_end(ap);

	if (kv->kv_key != NULL)
		free(kv->kv_key);
	kv->kv_key = key;

	return (0);
}

void
kv_delete(struct kvtree *keys, struct kv *kv)
{
	struct kv	*ckv;

	RB_REMOVE(kvtree, keys, kv);

	/* Remove all children */
	while ((ckv = TAILQ_FIRST(&kv->kv_children)) != NULL) {
		TAILQ_REMOVE(&kv->kv_children, ckv, kv_entry);
		kv_free(ckv);
		free(ckv);
	}

	kv_free(kv);
	free(kv);
}

struct kv *
kv_extend(struct kvtree *keys, struct kv *kv, char *value)
{
	char		*newvalue;

	if (kv == NULL) {
		return (NULL);
	} else if (kv->kv_value != NULL) {
		if (asprintf(&newvalue, "%s%s", kv->kv_value, value) == -1)
			return (NULL);

		free(kv->kv_value);
		kv->kv_value = newvalue;
	} else if ((kv->kv_value = strdup(value)) == NULL)
		return (NULL);

	return (kv);
}

void
kv_purge(struct kvtree *keys)
{
	struct kv	*kv;

	while ((kv = RB_MIN(kvtree, keys)) != NULL)
		kv_delete(keys, kv);
}

void
kv_free(struct kv *kv)
{
	if (kv->kv_type == KEY_TYPE_NONE)
		return;
	if (kv->kv_key != NULL) {
		free(kv->kv_key);
	}
	kv->kv_key = NULL;
	if (kv->kv_value != NULL) {
		free(kv->kv_value);
	}
	kv->kv_value = NULL;
	memset(kv, 0, sizeof(*kv));
}

struct kv *
kv_inherit(struct kv *dst, struct kv *src)
{
	memset(dst, 0, sizeof(*dst));
	memcpy(dst, src, sizeof(*dst));
	TAILQ_INIT(&dst->kv_children);

	if (src->kv_key != NULL) {
		if ((dst->kv_key = strdup(src->kv_key)) == NULL) {
			kv_free(dst);
			return (NULL);
		}
	}
	if (src->kv_value != NULL) {
		if ((dst->kv_value = strdup(src->kv_value)) == NULL) {
			kv_free(dst);
			return (NULL);
		}
	}

	return (dst);
}

int
kv_log(struct evbuffer *log, struct kv *kv)
{
	char	*msg;

	if (log == NULL)
		return (0);
	if (asprintf(&msg, " [%s%s%s]",
	    kv->kv_key == NULL ? "(unknown)" : kv->kv_key,
	    kv->kv_value == NULL ? "" : ": ",
	    kv->kv_value == NULL ? "" : kv->kv_value) == -1)
		return (-1);
	if (evbuffer_add(log, msg, strlen(msg)) == -1) {
		free(msg);
		return (-1);
	}
	free(msg);

	return (0);
}

struct kv *
kv_find(struct kvtree *keys, struct kv *kv)
{
	struct kv	*match;
	const char	*key;

	if (kv->kv_flags & KV_FLAG_GLOBBING) {
		/* Test header key using shell globbing rules */
		key = kv->kv_key == NULL ? "" : kv->kv_key;
		RB_FOREACH(match, kvtree, keys) {
			if (fnmatch(key, match->kv_key, FNM_CASEFOLD) == 0)
				break;
		}
	} else {
		/* Fast tree-based lookup only works without globbing */
		match = RB_FIND(kvtree, keys, kv);
	}

	return (match);
}

int
kv_cmp(struct kv *a, struct kv *b)
{
	return (strcasecmp(a->kv_key, b->kv_key));
}

RB_GENERATE(kvtree, kv, kv_node, kv_cmp);

struct media_type *
media_add(struct mediatypes *types, struct media_type *media)
{
	struct media_type	*entry;

	if ((entry = RB_FIND(mediatypes, types, media)) != NULL) {
		log_debug("%s: duplicated entry for \"%s\"", __func__,
		    media->media_name);
		return (NULL);
	}

	if ((entry = malloc(sizeof(*media))) == NULL)
		return (NULL);

	memcpy(entry, media, sizeof(*entry));
	RB_INSERT(mediatypes, types, entry);

	return (entry);
}

void
media_delete(struct mediatypes *types, struct media_type *media)
{
	RB_REMOVE(mediatypes, types, media);
	if (media->media_encoding != NULL)
		free(media->media_encoding);
	free(media);
}

void
media_purge(struct mediatypes *types)
{
	struct media_type	*media;

	while ((media = RB_MIN(mediatypes, types)) != NULL)
		media_delete(types, media);
}

struct media_type *
media_find(struct mediatypes *types, char *file)
{
	struct media_type	*match, media;
	char			*p;

	if ((p = strrchr(file, '.')) == NULL) {
		p = file;
	} else if (*p++ == '\0') {
		return (NULL);
	}
	if (strlcpy(media.media_name, p,
	    sizeof(media.media_name)) >=
	    sizeof(media.media_name)) {
		return (NULL);
	}

	/* Find media type by extension name */
	match = RB_FIND(mediatypes, types, &media);

	return (match);
}

int
media_cmp(struct media_type *a, struct media_type *b)
{
	return (strcasecmp(a->media_name, b->media_name));
}

RB_GENERATE(mediatypes, media_type, media_entry, media_cmp);
