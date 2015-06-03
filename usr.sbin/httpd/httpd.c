/*	$OpenBSD: httpd.c,v 1.37 2015/06/03 02:24:36 millert Exp $	*/

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

#include <sys/param.h>	/* nitems */
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <fnmatch.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>

#include "httpd.h"

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

__dead void	 usage(void);

int		 parent_configure(struct httpd *);
void		 parent_configure_done(struct httpd *);
void		 parent_reload(struct httpd *, u_int, const char *);
void		 parent_reopen(struct httpd *);
void		 parent_sig_handler(int, short, void *);
void		 parent_shutdown(struct httpd *);
int		 parent_dispatch_server(int, struct privsep_proc *,
		    struct imsg *);
int		 parent_dispatch_logger(int, struct privsep_proc *,
		    struct imsg *);

struct httpd			*httpd_env;

static struct privsep_proc procs[] = {
	{ "server",	PROC_SERVER, parent_dispatch_server, server },
	{ "logger",	PROC_LOGGER, parent_dispatch_logger, logger }
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
			int len;

			pid = waitpid(WAIT_ANY, &status, WNOHANG);
			if (pid <= 0)
				continue;

			fail = 0;
			if (WIFSIGNALED(status)) {
				fail = 1;
				len = asprintf(&cause, "terminated; signal %d",
				    WTERMSIG(status));
			} else if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					fail = 1;
					len = asprintf(&cause,
					    "exited abnormally");
				} else
					len = asprintf(&cause, "exited okay");
			} else
				fatalx("unexpected cause of SIGCHLD");

			if (len == -1)
				fatal("asprintf");

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
	case SIGUSR1:
		log_info("%s: reopen requested with SIGUSR1", __func__);

		parent_reopen(ps->ps_env);
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
	unsigned int		 proc;
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

	if (geteuid())
		errx(1, "need root privileges");

	if ((ps->ps_pw =  getpwnam(HTTPD_USER)) == NULL)
		errx(1, "unknown user %s", HTTPD_USER);

	/* Configure the control socket */
	ps->ps_csock.cs_name = NULL;

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

	if (env->sc_chroot == NULL)
		env->sc_chroot = ps->ps_pw->pw_dir;
	for (proc = 0; proc < nitems(procs); proc++)
		procs[proc].p_chroot = env->sc_chroot;

	if (env->sc_logdir == NULL) {
		if (asprintf(&env->sc_logdir, "%s%s", env->sc_chroot,
			HTTPD_LOGROOT) == -1)
			errx(1, "malloc failed");
	}

	proc_init(ps, procs, nitems(procs));

	setproctitle("parent");

	event_init();

	signal_set(&ps->ps_evsigint, SIGINT, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigterm, SIGTERM, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigchld, SIGCHLD, parent_sig_handler, ps);
	signal_set(&ps->ps_evsighup, SIGHUP, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigpipe, SIGPIPE, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigusr1, SIGUSR1, parent_sig_handler, ps);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsigchld, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);
	signal_add(&ps->ps_evsigusr1, NULL);

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
	struct auth		*auth;

	RB_FOREACH(media, mediatypes, env->sc_mediatypes) {
		if (config_setmedia(env, media) == -1)
			fatal("send media");
	}

	TAILQ_FOREACH(auth, env->sc_auth, auth_entry) {
		if (config_setauth(env, auth) == -1)
			fatal("send auth");
	}

	/* First send the servers... */
	TAILQ_FOREACH(srv, env->sc_servers, srv_entry) {
		if (srv->srv_conf.flags & SRVFLAG_LOCATION)
			continue;
		if (config_setserver(env, srv) == -1)
			fatal("send server");
	}
	/* ...and now send the locations */
	TAILQ_FOREACH(srv, env->sc_servers, srv_entry) {
		if ((srv->srv_conf.flags & SRVFLAG_LOCATION) == 0)
			continue;
		if (config_setserver(env, srv) == -1)
			fatal("send location");
	}

	/* The servers need to reload their config. */
	env->sc_reload = env->sc_prefork_server + 1;

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
parent_reopen(struct httpd *env)
{
	proc_compose_imsg(env->sc_ps, PROC_LOGGER, -1, IMSG_CTL_REOPEN,
	    -1, NULL, 0);
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
	if (env->sc_ps->ps_csock.cs_name != NULL)
		(void)unlink(env->sc_ps->ps_csock.cs_name);

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

int
parent_dispatch_logger(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct httpd		*env = p->p_env;
	u_int			 v;
	char			*str = NULL;

	switch (imsg->hdr.type) {
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
	case IMSG_CTL_REOPEN:
		parent_reopen(env);
		break;
	case IMSG_CFG_DONE:
		parent_configure_done(env);
		break;
	case IMSG_LOG_OPEN:
		if (logger_open_priv(imsg) == -1)
			fatalx("failed to open log file");
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
			free(tmp);
			return (-1);
		}
		q += strlen(srch);
		p = q;
	}
	if (strlcat(tmp, p, len) >= len) {
		log_debug("%s: string too long", __func__);
		free(tmp);
		return (-1);
	}
	(void)strlcpy(label, tmp, len);	/* always fits */
	free(tmp);

	return (0);
}

const char *
canonicalize_host(const char *host, char *name, size_t len)
{
	struct sockaddr_in	 sin4;
	struct sockaddr_in6	 sin6;
	size_t			 i, j;
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

const char *
url_decode(char *url)
{
	char	*p, *q;
	char	 hex[3];
	u_long	 x;

	hex[2] = '\0';
	p = q = url;

	while (*p != '\0') {
		switch (*p) {
		case '%':
			/* Encoding character is followed by two hex chars */
			if (!(isxdigit(p[1]) && isxdigit(p[2])))
				return (NULL);

			hex[0] = p[1];
			hex[1] = p[2];

			/*
			 * We don't have to validate "hex" because it is
			 * guaranteed to include two hex chars followed by nul.
			 */
			x = strtoul(hex, NULL, 16);
			*q = (char)x;
			p += 2;
			break;
		default:
			*q = *p;
			break;
		}
		p++;
		q++;
	}
	*q = '\0';

	return (url);
}

const char *
canonicalize_path(const char *input, char *path, size_t len)
{
	const char	*i;
	char		*p, *start, *end;

	/* assuming input starts with '/' and is nul-terminated */
	i = input;
	p = path;

	if (*input != '/' || len < 3)
		return (NULL);

	start = p;
	end = p + (len - 1);

	while (*i != '\0') {
		/* Detect truncation */
		if (p >= end)
			return (NULL);

		/* 1. check for special path elements */
		if (i[0] == '/') {
			if (i[1] == '/') {
				/* a) skip repeating '//' slashes */
				while (i[1] == '/')
					i++;
				continue;
			} else if (i[1] == '.' && i[2] == '.' &&
			    (i[3] == '/' || i[3] == '\0')) {
				/* b) revert '..' to previous directory */
				i += 3;
				while (p > start && *p != '/')
					p--;
				*p = '\0';
				continue;
			} else if (i[1] == '.' &&
			    (i[2] == '/' || i[2] == '\0')) {
				/* c) skip unnecessary '.' current dir */
				i += 2;
				continue;
			}
		}

		/* 2. copy any other characters */
		*p++ = *i;
		i++;
	}
	if (p == start)
		*p++ = '/';
	*p++ = '\0';

	return (path);
}

size_t
path_info(char *path)
{
	char		*p, *start, *end, ch;
	struct stat	 st;
	int		 ret;

	start = path;
	end = start + strlen(path);

	for (p = end; p > start; p--) {
		/* Scan every path component from the end and at each '/' */
		if (p < end && *p != '/')
			continue;

		/* Temporarily cut the path component out */
		ch = *p;
		*p = '\0';
		ret = stat(path, &st);
		*p = ch;

		/* Break if the initial path component was found */
		if (ret == 0)
			break;
	}

	return (p - start);
}

char *
url_encode(const char *src)
{
	static char	 hex[] = "0123456789ABCDEF";
	char		*dp, *dst;
	unsigned char	 c;

	/* We need 3 times the memory if every letter is encoded. */
	if ((dst = calloc(3, strlen(src) + 1)) == NULL)
		return (NULL);

	for (dp = dst; *src != 0; src++) {
		c = (unsigned char) *src;
		if (c == ' ' || c == '#' || c == '%' || c == '?' || c == '"' ||
		    c == '&' || c == '<' || c <= 0x1f || c >= 0x7f) {
			*dp++ = '%';
			*dp++ = hex[c >> 4];
			*dp++ = hex[c & 0x0f];
		} else
			*dp++ = *src;
	}
	return (dst);
}

char*
escape_html(const char* src)
{
	char		*dp, *dst;

	/* We need 5 times the memory if every letter is "<" or ">". */
	if ((dst = calloc(5, strlen(src) + 1)) == NULL)
		return NULL;

	for (dp = dst; *src != 0; src++) {
		if (*src == '<') {
			*dp++ = '&';
			*dp++ = 'l';
			*dp++ = 't';
			*dp++ = ';';
		} else if (*src == '>') {
			*dp++ = '&';
			*dp++ = 'g';
			*dp++ = 't';
			*dp++ = ';';
		} else if (*src == '&') {
			*dp++ = '&';
			*dp++ = 'a';
			*dp++ = 'm';
			*dp++ = 'p';
			*dp++ = ';';
		} else
			*dp++ = *src;
	}
	return (dst);
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
		rl.rlim_cur = MAXIMUM(rl.rlim_max, (rlim_t)maxfd);
	if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
		fatal("socket_rlimit: failed to set resource limit");
}

char *
evbuffer_getline(struct evbuffer *evb)
{
	u_int8_t	*ptr = EVBUFFER_DATA(evb);
	size_t		 len = EVBUFFER_LENGTH(evb);
	char		*str;
	size_t		 i;

	/* Safe version of evbuffer_readline() */
	if ((str = get_string(ptr, len)) == NULL)
		return (NULL);

	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] == '\r' || str[i] == '\n')
			break;
	}

	if (i == len) {
		free(str);
		return (NULL);
	}

	str[i] = '\0';

	if ((i + 1) < len) {
		if (ptr[i] == '\r' && ptr[i + 1] == '\n')
			i++;
	}

	evbuffer_drain(evb, ++i);

	return (str);
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
sockaddr_cmp(struct sockaddr *a, struct sockaddr *b, int prefixlen)
{
	struct sockaddr_in	*a4, *b4;
	struct sockaddr_in6	*a6, *b6;
	u_int32_t		 av[4], bv[4], mv[4];

	if (a->sa_family == AF_UNSPEC || b->sa_family == AF_UNSPEC)
		return (0);
	else if (a->sa_family > b->sa_family)
		return (1);
	else if (a->sa_family < b->sa_family)
		return (-1);

	if (prefixlen == -1)
		memset(&mv, 0xff, sizeof(mv));

	switch (a->sa_family) {
	case AF_INET:
		a4 = (struct sockaddr_in *)a;
		b4 = (struct sockaddr_in *)b;

		av[0] = a4->sin_addr.s_addr;
		bv[0] = b4->sin_addr.s_addr;
		if (prefixlen != -1)
			mv[0] = prefixlen2mask(prefixlen);

		if ((av[0] & mv[0]) > (bv[0] & mv[0]))
			return (1);
		if ((av[0] & mv[0]) < (bv[0] & mv[0]))
			return (-1);
		break;
	case AF_INET6:
		a6 = (struct sockaddr_in6 *)a;
		b6 = (struct sockaddr_in6 *)b;

		memcpy(&av, &a6->sin6_addr.s6_addr, 16);
		memcpy(&bv, &b6->sin6_addr.s6_addr, 16);
		if (prefixlen != -1)
			prefixlen2mask6(prefixlen, mv);

		if ((av[3] & mv[3]) > (bv[3] & mv[3]))
			return (1);
		if ((av[3] & mv[3]) < (bv[3] & mv[3]))
			return (-1);
		if ((av[2] & mv[2]) > (bv[2] & mv[2]))
			return (1);
		if ((av[2] & mv[2]) < (bv[2] & mv[2]))
			return (-1);
		if ((av[1] & mv[1]) > (bv[1] & mv[1]))
			return (1);
		if ((av[1] & mv[1]) < (bv[1] & mv[1]))
			return (-1);
		if ((av[0] & mv[0]) > (bv[0] & mv[0]))
			return (1);
		if ((av[0] & mv[0]) < (bv[0] & mv[0]))
			return (-1);
		break;
	}

	return (0);
}

u_int32_t
prefixlen2mask(u_int8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	if (prefixlen > 32)
		prefixlen = 32;

	return (htonl(0xffffffff << (32 - prefixlen)));
}

struct in6_addr *
prefixlen2mask6(u_int8_t prefixlen, u_int32_t *mask)
{
	static struct in6_addr  s6;
	int			i;

	if (prefixlen > 128)
		prefixlen = 128;

	memset(&s6, 0, sizeof(s6));
	for (i = 0; i < prefixlen / 8; i++)
		s6.s6_addr[i] = 0xff;
	i = prefixlen % 8;
	if (i)
		s6.s6_addr[prefixlen / 8] = 0xff00 >> i;

	memcpy(mask, &s6, sizeof(s6));

	return (&s6);
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
	if (media->media_encoding != NULL &&
	    (entry->media_encoding = strdup(media->media_encoding)) == NULL) {
		free(entry);
		return (NULL);
	}
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

	/* Last component of the file name */
	p = strchr(file, '\0');
	while (p > file && p[-1] != '.' && p[-1] != '/')
		p--;
	if (*p == '\0')
		return (NULL);

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

struct auth *
auth_add(struct serverauth *serverauth, struct auth *auth)
{
	struct auth		*entry;

	TAILQ_FOREACH(entry, serverauth, auth_entry) {
		if (strcmp(entry->auth_htpasswd, auth->auth_htpasswd) == 0)
			return (entry);
	}

	if ((entry = calloc(1, sizeof(*entry))) == NULL)
		return (NULL);

	memcpy(entry, auth, sizeof(*entry));

	TAILQ_INSERT_TAIL(serverauth, entry, auth_entry);

	return (entry);
}

struct auth *
auth_byid(struct serverauth *serverauth, u_int32_t id)
{
	struct auth	*auth;

	TAILQ_FOREACH(auth, serverauth, auth_entry) {
		if (auth->auth_id == id)
			return (auth);
	}

	return (NULL);
}

void
auth_free(struct serverauth *serverauth, struct auth *auth)
{
	TAILQ_REMOVE(serverauth, auth, auth_entry);
}
