/*	$OpenBSD: radiusd.c,v 1.1 2015/07/21 04:06:04 yasuoka Exp $	*/

/*
 * Copyright (c) 2013 Internet Initiative Japan Inc.
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
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <imsg.h>
#include <md5.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>
#include <util.h>

#include <radius.h>

#include "radiusd.h"
#include "radiusd_local.h"
#include "log.h"
#include "util.h"
#include "imsg_subr.h"

static int		 radiusd_start(struct radiusd *);
static void		 radiusd_stop(struct radiusd *);
static void		 radiusd_free(struct radiusd *);
static void		 radiusd_listen_on_event(int, short, void *);
static void		 radiusd_on_sigterm(int, short, void *);
static void		 radiusd_on_sigint(int, short, void *);
static void		 radiusd_on_sighup(int, short, void *);
static void		 radiusd_on_sigchld(int, short, void *);
static int		 radius_query_request_decoration(struct radius_query *);
static int		 radius_query_response_decoration(
			    struct radius_query *);
static const char	*radius_code_string(int);
static int		 radiusd_access_response_fixup (struct radius_query *);



static void		 radiusd_module_reset_ev_handler(
			    struct radiusd_module *);
static int		 radiusd_module_imsg_read(struct radiusd_module *,
			    bool);
static void		 radiusd_module_imsg(struct radiusd_module *,
			    struct imsg *);

static struct radiusd_module_radpkt_arg *
			 radiusd_module_recv_radpkt(struct radiusd_module *,
			    struct imsg *, uint32_t, const char *);
static void		 radiusd_module_on_imsg_io(int, short, void *);
void			 radiusd_module_start(struct radiusd_module *);
void			 radiusd_module_stop(struct radiusd_module *);
static void		 radiusd_module_close(struct radiusd_module *);
static void		 radiusd_module_userpass(struct radiusd_module *,
			    struct radius_query *);
static void		 radiusd_module_access_request(struct radiusd_module *,
			    struct radius_query *);

static u_int		 radius_query_id_seq = 0;
int			 debug = 0;

static __dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dh] [-f file]\n", __progname);
	exit(EX_USAGE);
}

int
main(int argc, char *argv[])
{
	extern char		*__progname;
	const char		*conffile = CONFFILE;
	int			 ch;
	struct radiusd		*radiusd;
	bool			 noaction = false;
	struct passwd		*pw;

	while ((ch = getopt(argc, argv, "df:nh")) != -1)
		switch (ch) {
		case 'd':
			debug++;
			break;

		case 'f':
			conffile = optarg;
			break;

		case 'n':
			noaction = true;
			break;

		case 'h':
			usage();
			/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if ((radiusd = calloc(1, sizeof(*radiusd))) == NULL)
		err(1, "calloc");
	TAILQ_INIT(&radiusd->listen);
	TAILQ_INIT(&radiusd->query);

	log_init(debug);
	event_init();
	if (parse_config(conffile, radiusd) != 0)
		errx(EX_DATAERR, "config error");
	if (noaction) {
		fprintf(stderr, "configuration OK\n");
		exit(EXIT_SUCCESS);
	}

	if (debug == 0)
		daemon(0, 0);

	if ((pw = getpwnam(RADIUSD_USER)) == NULL)
	    errx(EXIT_FAILURE, "user `%s' is not found in password "
		    "database", RADIUSD_USER);

	if (chroot(pw->pw_dir) == -1)
		err(EXIT_FAILURE, "chroot");
	if (chdir("/") == -1)
		err(EXIT_FAILURE, "chdir(\"/\")");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		err(EXIT_FAILURE, "cannot drop privileges");

	signal(SIGPIPE, SIG_IGN);
	openlog(NULL, LOG_PID, LOG_DAEMON);

	signal_set(&radiusd->ev_sigterm, SIGTERM, radiusd_on_sigterm, radiusd);
	signal_set(&radiusd->ev_sigint,  SIGINT,  radiusd_on_sigint,  radiusd);
	signal_set(&radiusd->ev_sighup,  SIGHUP,  radiusd_on_sighup,  radiusd);
	signal_set(&radiusd->ev_sigchld, SIGCHLD, radiusd_on_sigchld, radiusd);

	if (radiusd_start(radiusd) != 0)
		errx(EX_DATAERR, "start failed");

	if (event_loop(0) < 0)
		radiusd_stop(radiusd);

	radiusd_free(radiusd);
	event_base_free(NULL);

	exit(EXIT_SUCCESS);
}

static int
radiusd_start(struct radiusd *radiusd)
{
	struct radiusd_listen	*l;
	struct radiusd_module	*module;
	int			 s;
	char			 hbuf[NI_MAXHOST];

	TAILQ_FOREACH(l, &radiusd->listen, next) {
		if (getnameinfo(
		    (struct sockaddr *)&l->addr, l->addr.ipv4.sin_len,
		    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0) {
			log_warn("%s: getnameinfo()", __func__);
			goto on_error;
		}
		if ((s = socket(l->addr.ipv4.sin_family, l->stype, l->sproto))
		    < 0) {
			log_warn("Listen %s port %d is failed: socket()",
			    hbuf, (int)htons(l->addr.ipv4.sin_port));
			goto on_error;
		}
		if (bind(s, (struct sockaddr *)&l->addr, l->addr.ipv4.sin_len)
		    != 0) {
			log_warn("Listen %s port %d is failed: bind()",
			    hbuf, (int)htons(l->addr.ipv4.sin_port));
			close(s);
			goto on_error;
		}
		if (l->addr.ipv4.sin_family == AF_INET)
			log_info("Start listening on %s:%d/udp", hbuf,
			    (int)ntohs(l->addr.ipv4.sin_port));
		else
			log_info("Start listening on [%s]:%d/udp", hbuf,
			    (int)ntohs(l->addr.ipv4.sin_port));
		event_set(&l->ev, s, EV_READ | EV_PERSIST,
		    radiusd_listen_on_event, l);
		if (event_add(&l->ev, NULL) != 0) {
			log_warn("event_add() failed at %s()", __func__);
			close(s);
			goto on_error;
		}
		l->sock = s;
		l->radiusd = radiusd;
	}

	signal_add(&radiusd->ev_sigterm, NULL);
	signal_add(&radiusd->ev_sigint, NULL);
	signal_add(&radiusd->ev_sighup, NULL);
	signal_add(&radiusd->ev_sigchld, NULL);

	TAILQ_FOREACH(module, &radiusd->module, next) {
		radiusd_module_start(module);
	}

	return (0);
on_error:
	radiusd_stop(radiusd);

	return (-1);
}

static void
radiusd_stop(struct radiusd *radiusd)
{
	char			 hbuf[NI_MAXHOST];
	struct radiusd_listen	*l;
	struct radiusd_module	*module;

	TAILQ_FOREACH_REVERSE(l, &radiusd->listen, radiusd_listen_head, next) {
		if (l->sock >= 0) {
			if (getnameinfo(
			    (struct sockaddr *)&l->addr, l->addr.ipv4.sin_len,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
				strlcpy(hbuf, "error", sizeof(hbuf));
			if (l->addr.ipv4.sin_family == AF_INET)
				log_info("Stop listening on %s:%d/udp", hbuf,
				    (int)ntohs(l->addr.ipv4.sin_port));
			else
				log_info("Stop listening on [%s]:%d/udp", hbuf,
				    (int)ntohs(l->addr.ipv4.sin_port));
			event_del(&l->ev);
			close(l->sock);
		}
		l->sock = -1;
	}
	TAILQ_FOREACH(module, &radiusd->module, next) {
		radiusd_module_stop(module);
		radiusd_module_close(module);
	}
	if (signal_pending(&radiusd->ev_sigterm, NULL))
		signal_del(&radiusd->ev_sigterm);
	if (signal_pending(&radiusd->ev_sigint, NULL))
		signal_del(&radiusd->ev_sigint);
	if (signal_pending(&radiusd->ev_sighup, NULL))
		signal_del(&radiusd->ev_sighup);
	if (signal_pending(&radiusd->ev_sigchld, NULL))
		signal_del(&radiusd->ev_sigchld);
}

static void
radiusd_free(struct radiusd *radiusd)
{
	int				 i;
	struct radiusd_listen		*listn, *listnt;
	struct radiusd_client		*client, *clientt;
	struct radiusd_module		*module, *modulet;
	struct radiusd_module_ref	*modref, *modreft;
	struct radiusd_authentication	*authen, *authent;

	TAILQ_FOREACH_SAFE(authen, &radiusd->authen, next, authent) {
		TAILQ_REMOVE(&radiusd->authen, authen, next);
		if (authen->auth != NULL)
			free(authen->auth);
		TAILQ_FOREACH_SAFE(modref, &authen->deco, next, modreft) {
			TAILQ_REMOVE(&authen->deco, modref, next);
			free(modref);
		}
		for (i = 0; authen->username[i] != NULL; i++)
			free(authen->username[i]);
		free(authen->username);
		free(authen);
	}
	TAILQ_FOREACH_SAFE(module, &radiusd->module, next, modulet) {
		TAILQ_REMOVE(&radiusd->module, module, next);
		radiusd_module_unload(module);
	}
	TAILQ_FOREACH_SAFE(client, &radiusd->client, next, clientt) {
		TAILQ_REMOVE(&radiusd->client, client, next);
		explicit_bzero(client->secret, sizeof(client->secret));
		free(client);
	}
	TAILQ_FOREACH_SAFE(listn, &radiusd->listen, next, listnt) {
		TAILQ_REMOVE(&radiusd->listen, listn, next);
		free(listn);
	}
	free(radiusd);
}

/***********************************************************************
 * Network event handlers
 ***********************************************************************/
#define IPv4_cmp(_in, _addr, _mask) (				\
	((_in)->s_addr & (_mask)->addr.ipv4.s_addr) ==		\
	    (_addr)->addr.ipv4.s_addr)
#define	s6_addr32(_in6)	((uint32_t *)(_in6)->s6_addr)
#define IPv6_cmp(_in6, _addr, _mask) (				\
	((s6_addr32(_in6)[3] & (_mask)->addr.addr32[3])		\
	    == (_addr)->addr.addr32[3]) &&			\
	((s6_addr32(_in6)[2] & (_mask)->addr.addr32[2])		\
	    == (_addr)->addr.addr32[2]) &&			\
	((s6_addr32(_in6)[1] & (_mask)->addr.addr32[1])		\
	    == (_addr)->addr.addr32[1]) &&			\
	((s6_addr32(_in6)[0] & (_mask)->addr.addr32[0])		\
	    == (_addr)->addr.addr32[0]))

static void
radiusd_listen_on_event(int fd, short evmask, void *ctx)
{
	int				 i, sz, req_id, req_code;
	struct radiusd_listen		*listn = ctx;
	static u_char			 buf[65535];
	static char			 username[256];
	struct sockaddr_storage		 peer;
	socklen_t			 peersz;
	RADIUS_PACKET			*packet = NULL;
	char				 peerstr[NI_MAXHOST + NI_MAXSERV + 30];
	struct radiusd_authentication	*authen;
	struct radiusd_client		*client;
	struct radius_query		*q;
#define in(_x)	(((struct sockaddr_in  *)_x)->sin_addr)
#define in6(_x)	(((struct sockaddr_in6 *)_x)->sin6_addr)

	if (evmask & EV_READ) {
		peersz = sizeof(peer);
		if ((sz = recvfrom(listn->sock, buf, sizeof(buf), 0,
		    (struct sockaddr *)&peer, &peersz)) < 0) {
			log_warn("%s: recvfrom() failed", __func__);
			goto on_error;
		}
		RADIUSD_ASSERT(peer.ss_family == AF_INET ||
		    peer.ss_family == AF_INET6);

		/* prepare some information about this messages */
		if (addrport_tostring((struct sockaddr *)&peer, peersz,
		    peerstr, sizeof(peerstr)) == NULL) {
			log_warn("%s: getnameinfo() failed", __func__);
			goto on_error;
		}
		if ((packet = radius_convert_packet(buf, sz)) == NULL) {
			log_warn("%s: radius_convert_packet() failed",
			    __func__);
			goto on_error;
		}
		req_id = radius_get_id(packet);
		req_code = radius_get_code(packet);

		/*
		 * Find a matching `client' entry
		 */
		TAILQ_FOREACH(client, &listn->radiusd->client, next) {
			if (client->af != peer.ss_family)
				continue;
			if (peer.ss_family == AF_INET &&
			    IPv4_cmp(&((struct sockaddr_in *)&peer)->sin_addr,
			    &client->addr, &client->mask))
				break;
			else if (peer.ss_family == AF_INET6 &&
			    IPv6_cmp(&((struct sockaddr_in6 *)&peer)->sin6_addr,
			    &client->addr, &client->mask))
				break;
		}
		if (client == NULL) {
			log_warnx("Received %s(code=%d) from %s id=%d: "
			    "no `client' matches", radius_code_string(req_code),
			    req_code, peerstr, req_id);
			goto on_error;
		}

		/* Check the client's Message-Authenticator */
		if (client->msgauth_required &&
		    !radius_has_attr(packet,
		    RADIUS_TYPE_MESSAGE_AUTHENTICATOR)) {
			log_warnx("Received %s(code=%d) from %s id=%d: "
			    "no message authenticator",
			    radius_code_string(req_code), req_code, peerstr,
			    req_id);
			goto on_error;
		}

		if (radius_has_attr(packet,
		    RADIUS_TYPE_MESSAGE_AUTHENTICATOR) &&
		    radius_check_message_authenticator(packet, client->secret)
		    != 0) {
			log_warnx("Received %s(code=%d) from %s id=%d: "
			    "bad message authenticator",
			    radius_code_string(req_code), req_code, peerstr,
			    req_id);
			goto on_error;
		}

		/*
		 * Find a duplicate request.  In RFC 2865, it has the same
		 * source IP address and source UDP port and Identifier.
		 */
		TAILQ_FOREACH(q, &listn->radiusd->query, next) {
			if (peer.ss_family == q->clientaddr.ss_family &&
			    ((peer.ss_family == AF_INET &&
			    in(&q->clientaddr).s_addr ==
			    in(&peer).s_addr) ||
			    (peer.ss_family == AF_INET6 &&
			    IN6_ARE_ADDR_EQUAL(
			    &in6(&q->clientaddr), &in6(&peer)))) &&
			    ((struct sockaddr_in *)&q->clientaddr)->sin_port ==
			    ((struct sockaddr_in *)&peer)->sin_port &&
			    req_id == q->req_id)
				break;	/* found it */
		}
		if (q != NULL) {
			/* RFC 5080 suggests to answer the cached result */
			log_info("Received %s(code=%d) from %s id=%d: "
			    "duplicate request by q=%u",
			    radius_code_string(req_code), req_code, peerstr,
			    req_id, q->id);
			// XXX
			return;
		}

		/* FIXME: we can support other request codes */
		if (req_code != RADIUS_CODE_ACCESS_REQUEST) {
			log_info("Received %s(code=%d) from %s id=%d: %s "
			    "is not supported in this implementation",
			    radius_code_string(req_code), req_code, peerstr,
			    req_id, radius_code_string(req_code));
			goto on_error;
		}

		/*
		 * Find a matching `authenticate' entry
		 */
		if (radius_get_string_attr(packet, RADIUS_TYPE_USER_NAME,
		    username, sizeof(username)) != 0) {
			log_info("Received %s(code=%d) from %s id=%d: "
			    "no User-Name attribute",
			    radius_code_string(req_code), req_code, peerstr,
			    req_id);
			goto on_error;
		}
		TAILQ_FOREACH(authen, &listn->radiusd->authen, next) {
			for (i = 0; authen->username[i] != NULL; i++) {
				if (fnmatch(authen->username[i], username, 0)
				    == 0)
					goto found;
			}
		}
		if (authen == NULL) {
			log_warnx("Received %s(code=%d) from %s id=%d "
			    "username=%s: no `authenticate' matches.",
			    radius_code_string(req_code), req_code, peerstr,
			    req_id, username);
			goto on_error;
		}
found:
		if (!MODULE_DO_USERPASS(authen->auth->module) &&
		    !MODULE_DO_ACCSREQ(authen->auth->module)) {
			log_warnx("Received %s(code=%d) from %s id=%d "
			    "username=%s: module `%s' is not running.",
			    radius_code_string(req_code), req_code, peerstr,
			    req_id, username, authen->auth->module->name);
			goto on_error;
		}
		if ((q = calloc(1, sizeof(struct radius_query))) == NULL) {
			log_warn("%s: Out of memory", __func__);
			goto on_error;
		}
		memcpy(&q->clientaddr, &peer, peersz);
		strlcpy(q->username, username, sizeof(q->username));
		q->id = ++radius_query_id_seq;
		q->clientaddrlen = peersz;
		q->authen = authen;
		q->listen = listn;
		q->req = packet;
		q->client = client;
		q->req_id = req_id;
		radius_get_authenticator(packet, q->req_auth);

		if (radius_query_request_decoration(q) != 0) {
			log_warnx(
			    "Received %s(code=%d) from %s id=%d username=%s "
			    "q=%u: failed to decorate the request",
			    radius_code_string(req_code), req_code, peerstr,
			    q->req_id, q->username, q->id);
			radiusd_access_request_aborted(q);
			return;
		}
		log_info("Received %s(code=%d) from %s id=%d username=%s "
		    "q=%u: `%s' authentication is starting",
		    radius_code_string(req_code), req_code, peerstr, q->req_id,
		    q->username, q->id, q->authen->auth->module->name);
		TAILQ_INSERT_TAIL(&listn->radiusd->query, q, next);

		if (MODULE_DO_ACCSREQ(authen->auth->module)) {
			radiusd_module_access_request(authen->auth->module, q);
		} else if (MODULE_DO_USERPASS(authen->auth->module))
			radiusd_module_userpass(authen->auth->module, q);

		return;
	}
on_error:
	if (packet != NULL)
		radius_delete_packet(packet);
#undef in
#undef in6

	return;
}

static int
radius_query_request_decoration(struct radius_query *q)
{
	struct radiusd_module_ref	*deco;

	TAILQ_FOREACH(deco, &q->authen->deco, next) {
		/* XXX module is running? */
		/* XXX */
		if (deco->module->request_decoration != NULL &&
		    deco->module->request_decoration(NULL, q) != 0) {
			log_warnx("q=%u request decoration `%s' failed", q->id,
			    deco->module->name);
			return (-1);
		}
	}

	return (0);
}

static int
radius_query_response_decoration(struct radius_query *q)
{
	struct radiusd_module_ref	*deco;

	TAILQ_FOREACH(deco, &q->authen->deco, next) {
		/* XXX module is running? */
		/* XXX */
		if (deco->module->response_decoration != NULL &&
		    deco->module->response_decoration(NULL, q) != 0) {
			log_warnx("q=%u response decoration `%s' failed", q->id,
			    deco->module->name);
			return (-1);
		}
	}

	return (0);
}

/***********************************************************************
 * Callback functions from the modules
 ***********************************************************************/
void
radiusd_access_request_answer(struct radius_query *q)
{
	int		 sz, res_id, res_code;
	char		 buf[NI_MAXHOST + NI_MAXSERV + 30];
	const char	*authen_secret = q->authen->auth->module->secret;

	radius_set_request_packet(q->res, q->req);

	if (authen_secret == NULL) {
		/*
		 * The module couldn't check the autheticators
		 */
		if (radius_check_response_authenticator(q->res,
		    q->client->secret) != 0) {
			log_info("Response from module has bad response "
			    "authenticator: id=%d", q->id);
			goto on_error;
		}
		if (radius_has_attr(q->res,
		    RADIUS_TYPE_MESSAGE_AUTHENTICATOR) &&
		    radius_check_message_authenticator(q->res,
		    q->client->secret) != 0) {
			log_info("Response from module has bad message "
			    "authenticator: id=%d", q->id);
			goto on_error;
		}
	}

	/* Decorate the response */
	if (radius_query_response_decoration(q) != 0)
		goto on_error;

	if (radiusd_access_response_fixup(q) != 0)
		goto on_error;

	res_id = radius_get_id(q->res);
	res_code = radius_get_code(q->res);

	/*
	 * Reset response authenticator in the following cases:
	 * - response is modified by decorator
	 * - server's secret is differ from client's secret.
	 */
	if (q->res_modified > 0 ||
	    (authen_secret != NULL &&
		    strcmp(q->client->secret, authen_secret) != 0))
		radius_set_response_authenticator(q->res, q->client->secret);

	log_info("Sending %s(code=%d) to %s id=%u q=%u",
	    radius_code_string(res_code), res_code,
	    addrport_tostring((struct sockaddr *)&q->clientaddr,
		    q->clientaddrlen, buf, sizeof(buf)), res_id, q->id);

	if ((sz = sendto(q->listen->sock, radius_get_data(q->res),
	    radius_get_length(q->res), 0,
	    (struct sockaddr *)&q->clientaddr, q->clientaddrlen)) <= 0)
		log_warn("Sending a RADIUS response failed");
on_error:
	radiusd_access_request_aborted(q);
}

void
radiusd_access_request_aborted(struct radius_query *q)
{
	if (q->req != NULL)
		radius_delete_packet(q->req);
	if (q->res != NULL)
		radius_delete_packet(q->res);
	TAILQ_REMOVE(&q->listen->radiusd->query, q, next);
	free(q);
}

/***********************************************************************
 * Signal handlers
 ***********************************************************************/
static void
radiusd_on_sigterm(int fd, short evmask, void *ctx)
{
	struct radiusd	*radiusd = ctx;

	log_info("Received SIGTERM");
	radiusd_stop(radiusd);
}

static void
radiusd_on_sigint(int fd, short evmask, void *ctx)
{
	struct radiusd	*radiusd = ctx;

	log_info("Received SIGINT");
	radiusd_stop(radiusd);
}

static void
radiusd_on_sighup(int fd, short evmask, void *ctx)
{
	log_info("Received SIGHUP");
}

static void
radiusd_on_sigchld(int fd, short evmask, void *ctx)
{
	struct radiusd		*radiusd = ctx;
	struct radiusd_module	*module;
	pid_t			 pid;
	int			 status;

	log_debug("Received SIGCHLD");
	while ((pid = wait3(&status, WNOHANG, NULL)) != 0) {
		if (pid == -1)
			break;
		TAILQ_FOREACH(module, &radiusd->module, next) {
			if (module->pid == pid) {
				if (WIFEXITED(status))
					log_warnx("module `%s'(pid=%d) exited "
					    "with status %d", module->name,
					    (int)pid, WEXITSTATUS(status));
				else
					log_warnx("module `%s'(pid=%d) exited "
					    "by signal %d", module->name,
					    (int)pid, WTERMSIG(status));
				break;
			}
		}
		if (!module) {
			if (WIFEXITED(status))
				log_warnx("unkown child process pid=%d exited "
				    "with status %d", (int)pid,
				     WEXITSTATUS(status));
			else
				log_warnx("unkown child process pid=%d exited "
				    "by signal %d", (int)pid,
				    WTERMSIG(status));
		}
	}
}

static const char *
radius_code_string(int code)
{
	int			i;
	struct _codestrings {
		int		 code;
		const char	*string;
	} codestrings[] = {
	    { RADIUS_CODE_ACCESS_REQUEST,	"Access-Request" },
	    { RADIUS_CODE_ACCESS_ACCEPT,	"Access-Accept" },
	    { RADIUS_CODE_ACCESS_REJECT,	"Access-Reject" },
	    { RADIUS_CODE_ACCOUNTING_REQUEST,	"Accounting-Request" },
	    { RADIUS_CODE_ACCOUNTING_RESPONSE,	"Accounting-Response" },
	    { RADIUS_CODE_ACCESS_CHALLENGE,	"Access-Challenge" },
	    { RADIUS_CODE_STATUS_SERVER,	"Status-Server" },
	    { RADIUS_CODE_STATUS_CLIENT,	"Status-Clinet" },
	    { -1,				NULL }
	};

	for (i = 0; codestrings[i].code != -1; i++)
		if (codestrings[i].code == code)
			return (codestrings[i].string);

	return "Unknown";
}

void
radiusd_conf_init(struct radiusd *conf)
{

	TAILQ_INIT(&conf->listen);
	TAILQ_INIT(&conf->module);
	TAILQ_INIT(&conf->authen);
	TAILQ_INIT(&conf->client);

	/*
	 * TODO: load the standard modules
	 */
#if 0
static struct radiusd_module *radiusd_standard_modules[] = {
	NULL
};

	u_int			 i;
	struct radiusd_module	*module;
	for (i = 0; radiusd_standard_modules[i] != NULL; i++) {
		module = radiusd_create_module_class(
		    radiusd_standard_modules[i]);
		TAILQ_INSERT_TAIL(&conf->module, module, next);
	}
#endif

	return;
}

/*
 * Fix some attributes which depend the secret value.
 */
static int
radiusd_access_response_fixup(struct radius_query *q)
{
	int		 res_id;
	size_t		 attrlen;
	u_char		 req_auth[16], attrbuf[256];
	const char	*olds, *news;
	const char	*authen_secret = q->authen->auth->module->secret;

	olds = q->client->secret;
	news = authen_secret;
	if (news == NULL)
		olds = news;
	radius_get_authenticator(q->req, req_auth);

	if ((authen_secret != NULL &&
	    strcmp(authen_secret, q->client->secret) != 0) ||
	    timingsafe_bcmp(q->req_auth, req_auth, 16) != 0) {

		/* RFC 2865 Tunnel-Password */
		if (radius_get_raw_attr(q->res, RADIUS_TYPE_TUNNEL_PASSWORD,
		    attrbuf, &attrlen) == 0) {
			radius_attr_unhide(news, req_auth,
			    attrbuf, attrbuf + 3, attrlen - 3);
			radius_attr_hide(olds, q->req_auth,
			    attrbuf, attrbuf + 3, attrlen - 3);

			radius_del_attr_all(q->res,
			    RADIUS_TYPE_TUNNEL_PASSWORD);
			radius_put_raw_attr(q->res,
			    RADIUS_TYPE_TUNNEL_PASSWORD, attrbuf, attrlen);
			q->res_modified++;
		}

		/* RFC 2548 Microsoft MPPE-{Send,Recv}-Key */
		if (radius_get_vs_raw_attr(q->res, RADIUS_VENDOR_MICROSOFT,
		    RADIUS_VTYPE_MPPE_SEND_KEY, attrbuf, &attrlen) == 0) {

			/* Re-crypt the KEY */
			radius_attr_unhide(news, req_auth,
			    attrbuf, attrbuf + 2, attrlen - 2);
			radius_attr_hide(olds, q->req_auth,
			    attrbuf, attrbuf + 2, attrlen - 2);

			radius_del_vs_attr_all(q->res, RADIUS_VENDOR_MICROSOFT,
			    RADIUS_VTYPE_MPPE_SEND_KEY);
			radius_put_vs_raw_attr(q->res, RADIUS_VENDOR_MICROSOFT,
			    RADIUS_VTYPE_MPPE_SEND_KEY, attrbuf, attrlen);
			q->res_modified++;
		}
		if (radius_get_vs_raw_attr(q->res, RADIUS_VENDOR_MICROSOFT,
		    RADIUS_VTYPE_MPPE_RECV_KEY, attrbuf, &attrlen) == 0) {

			/* Re-crypt the KEY */
			radius_attr_unhide(news, req_auth,
			    attrbuf, attrbuf + 2, attrlen - 2);
			radius_attr_hide(olds, q->req_auth,
			    attrbuf, attrbuf + 2, attrlen - 2);

			radius_del_vs_attr_all(q->res, RADIUS_VENDOR_MICROSOFT,
			    RADIUS_VTYPE_MPPE_RECV_KEY);
			radius_put_vs_raw_attr(q->res, RADIUS_VENDOR_MICROSOFT,
			    RADIUS_VTYPE_MPPE_RECV_KEY, attrbuf, attrlen);
			q->res_modified++;
		}
	}

	res_id = radius_get_id(q->res);
	if (res_id != q->req_id) {
		/* authentication server change the id */
		radius_set_id(q->res, q->req_id);
		q->res_modified++;
	}

	return (0);
}

void
radius_attr_hide(const char *secret, const char *authenticator,
    const u_char *salt, u_char *plain, int plainlen)
{
	int	  i, j;
	u_char	  b[16];
	MD5_CTX	  md5ctx;

	i = 0;
	do {
		MD5Init(&md5ctx);
		MD5Update(&md5ctx, secret, strlen(secret));
		if (i == 0) {
			MD5Update(&md5ctx, authenticator, 16);
			if (salt != NULL)
				MD5Update(&md5ctx, salt, 2);
		} else
			MD5Update(&md5ctx, plain + i - 16, 16);
		MD5Final(b, &md5ctx);

		for (j = 0; j < 16 && i < plainlen; i++, j++)
			plain[i] ^= b[j];
	} while (i < plainlen);
}

void
radius_attr_unhide(const char *secret, const char *authenticator,
    const u_char *salt, u_char *crypt0, int crypt0len)
{
	int	  i, j;
	u_char	  b[16];
	MD5_CTX	  md5ctx;

	i = 16 * ((crypt0len - 1) / 16);
	while (i >= 0) {
		MD5Init(&md5ctx);
		MD5Update(&md5ctx, secret, strlen(secret));
		if (i == 0) {
			MD5Update(&md5ctx, authenticator, 16);
			if (salt != NULL)
				MD5Update(&md5ctx, salt, 2);
		} else
			MD5Update(&md5ctx, crypt0 + i - 16, 16);
		MD5Final(b, &md5ctx);

		for (j = 0; j < 16 && i + j < crypt0len; j++)
			crypt0[i + j] ^= b[j];
		i -= 16;
	}
}

static struct radius_query *
radiusd_find_query(struct radiusd *radiusd, u_int q_id)
{
	struct radius_query	*q;

	TAILQ_FOREACH(q, &radiusd->query, next) {
		if (q->id == q_id)
			return (q);
	}
	return (NULL);
}

/***********************************************************************
 * radiusd module handling
 ***********************************************************************/
struct radiusd_module *
radiusd_module_load(struct radiusd *radiusd, const char *path, const char *name)
{
	struct radiusd_module		*module = NULL;
	pid_t				 pid;
	int				 on, pairsock[] = { -1, -1 };
	const char			*av[3];
	ssize_t				 n;
	struct imsg			 imsg;

	module = calloc(1, sizeof(struct radiusd_module));
	if (module == NULL)
		fatal("Out of memory");
	module->radiusd = radiusd;

        if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pairsock) == -1) {
		log_warn("Could not load module `%s'(%s): pipe()", name, path);
		goto on_error;
	}

	pid = fork();
	if (pid == -1) {
		log_warn("Could not load module `%s'(%s): fork()", name, path);
		goto on_error;
	}
	if (pid == 0) {
		setsid();
		close(pairsock[0]);
		av[0] = path;
		av[1] = name;
		av[2] = NULL;
		dup2(pairsock[1], STDIN_FILENO);
		dup2(pairsock[1], STDOUT_FILENO);
		close(pairsock[1]);
		closefrom(STDERR_FILENO + 1);
		execv(path, (char * const *)av);
		warn("Failed to execute %s", path);
		_exit(EXIT_FAILURE);
	}
	close(pairsock[1]);

	module->fd = pairsock[0];
	on = 1;
	if (fcntl(module->fd, O_NONBLOCK, &on) == -1) {
		log_warn("Could not load module `%s': fcntl(,O_NONBLOCK)",
		    name);
		goto on_error;
	}
	strlcpy(module->name, name, sizeof(module->name));
	module->pid = pid;
	imsg_init(&module->ibuf, module->fd);

	if (imsg_sync_read(&module->ibuf, MODULE_IO_TIMEOUT) <= 0 ||
	    (n = imsg_get(&module->ibuf, &imsg)) <= 0) {
		log_warnx("Could not load module `%s': module didn't "
		    "respond", name);
		goto on_error;
	}
	if (imsg.hdr.type != IMSG_RADIUSD_MODULE_LOAD) {
		imsg_free(&imsg);
		log_warnx("Could not load module `%s': unknown imsg type=%d",
		    name, imsg.hdr.type);
		goto on_error;
	}

	module->capabilities =
	    ((struct radiusd_module_load_arg *)imsg.data)->cap;
	radiusd_module_reset_ev_handler(module);

	log_debug("Loaded module `%s' successfully.  pid=%d", module->name,
	    module->pid);
	imsg_free(&imsg);

	return (module);

on_error:
	if (module != NULL)
		free(module);
	if (pairsock[0] >= 0)
		close(pairsock[0]);
	if (pairsock[1] >= 0)
		close(pairsock[1]);

	return (NULL);
}

void
radiusd_module_start(struct radiusd_module *module)
{
	int		 datalen;
	struct imsg	 imsg;

	RADIUSD_ASSERT(module->fd >= 0);
	imsg_compose(&module->ibuf, IMSG_RADIUSD_MODULE_START, 0, 0, -1,
	    NULL, 0);
	imsg_sync_flush(&module->ibuf, MODULE_IO_TIMEOUT);
	if (imsg_sync_read(&module->ibuf, MODULE_IO_TIMEOUT) <= 0 ||
	    imsg_get(&module->ibuf, &imsg) <= 0) {
		log_warnx("Module `%s' could not start: no response",
		    module->name);
		goto on_fail;
	}

	datalen = imsg.hdr.len - IMSG_HEADER_SIZE;
	if (imsg.hdr.type != IMSG_OK) {
		if (imsg.hdr.type == IMSG_NG) {
			if (datalen > 0)
				log_warnx("Module `%s' could not start: %s",
				    module->name, (char *)imsg.data);
			else
				log_warnx("Module `%s' could not start",
				    module->name);
		} else
			log_warnx("Module `%s' could not started: module "
			    "returned unknow message type %d", module->name,
			    imsg.hdr.type);
		goto on_fail;
	}

	log_debug("Module `%s' started successfully", module->name);
	radiusd_module_reset_ev_handler(module);
	return;
on_fail:
	radiusd_module_close(module);
	return;
}

void
radiusd_module_stop(struct radiusd_module *module)
{
	RADIUSD_ASSERT(module->fd >= 0);

	module->stopped = true;

	if (module->secret != NULL)
		explicit_bzero(module->secret, strlen(module->secret));
	free(module->secret);
	module->secret = NULL;

	imsg_compose(&module->ibuf, IMSG_RADIUSD_MODULE_STOP, 0, 0, -1,
	    NULL, 0);
	radiusd_module_reset_ev_handler(module);
}

static void
radiusd_module_close(struct radiusd_module *module)
{
	if (module->fd >= 0) {
		event_del(&module->ev);
		imsg_clear(&module->ibuf);
		close(module->fd);
		module->fd = -1;
	}
}

void
radiusd_module_unload(struct radiusd_module *module)
{
	free(module->radpkt);
	radiusd_module_close(module);
	free(module);
}

static void
radiusd_module_on_imsg_io(int fd, short evmask, void *ctx)
{
	struct radiusd_module	*module = ctx;
	int			 ret;

	if (evmask & EV_WRITE)
		module->writeready = true;

	if (evmask & EV_READ || module->ibuf.r.wpos > IMSG_HEADER_SIZE) {
		if (radiusd_module_imsg_read(module,
		    (evmask & EV_READ)? true : false) == -1)
			goto on_error;
	}

	while (module->writeready && module->ibuf.w.queued) {
		ret = msgbuf_write(&module->ibuf.w);
		if (ret > 0)
			continue;
		module->writeready = false;
		if (ret == 0 && errno == EAGAIN)
			break;
		log_warn("Failed to write to module `%s': msgbuf_write()",
		    module->name);
		goto on_error;
	}
	radiusd_module_reset_ev_handler(module);

	return;
on_error:
	radiusd_module_close(module);
}

static void
radiusd_module_reset_ev_handler(struct radiusd_module *module)
{
	short		 evmask;
	struct timeval	*tvp = NULL, tv = { 0, 0 };

	RADIUSD_ASSERT(module->fd >= 0);
	if (event_initialized(&module->ev))
		event_del(&module->ev);

	evmask = EV_READ;
	if (module->ibuf.w.queued) {
		if (!module->writeready)
			evmask |= EV_WRITE;
		else
			tvp = &tv;	/* fire immediately */
	} else if (module->ibuf.r.wpos > IMSG_HEADER_SIZE)
		tvp = &tv;		/* fire immediately */

	/* module stopped and no event handler is set */
	if (evmask & EV_WRITE && tvp == NULL && module->stopped) {
		/* stop requested and no more to write */
		radiusd_module_close(module);
		return;
	}

	event_set(&module->ev, module->fd, evmask, radiusd_module_on_imsg_io,
	    module);
	if (event_add(&module->ev, tvp) == -1) {
		log_warn("Could not set event handlers for module `%s': "
		    "event_add()", module->name);
		radiusd_module_close(module);
	}
}

static int
radiusd_module_imsg_read(struct radiusd_module *module, bool doread)
{
	int		 n;
	struct imsg	 imsg;

	if (doread) {
		if ((n = imsg_read(&module->ibuf)) == -1 || n == 0) {
			if (n == -1 && errno == EAGAIN)
				return (0);
			if (n == -1)
				log_warn("Receiving a message from module `%s' "
				    "failed: imsg_read", module->name);
			/* else closed */
			radiusd_module_close(module);
			return (-1);
		}
	}
	for (;;) {
		if ((n = imsg_get(&module->ibuf, &imsg)) == -1) {
			log_warn("Receiving a message from module `%s' failed: "
			    "imsg_get", module->name);
			return (-1);
		}
		if (n == 0)
			return (0);
		radiusd_module_imsg(module, &imsg);
	}

	return (0);
}

static void
radiusd_module_imsg(struct radiusd_module *module, struct imsg *imsg)
{
	int			 datalen;
	struct radius_query	*q;
	u_int			 q_id;

	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;
	switch (imsg->hdr.type) {
	case IMSG_RADIUSD_MODULE_NOTIFY_SECRET:
		if (datalen > 0) {
			module->secret = strdup(imsg->data);
			if (module->secret == NULL)
				log_warn("Could not handle NOTIFY_SECRET "
				    "from `%s'", module->name);
		}
		break;
	case IMSG_RADIUSD_MODULE_USERPASS_OK:
	case IMSG_RADIUSD_MODULE_USERPASS_FAIL:
	    {
		char			*msg = NULL;
		const char		*msgtypestr;

		msgtypestr = (imsg->hdr.type == IMSG_RADIUSD_MODULE_USERPASS_OK)
		    ? "USERPASS_OK" : "USERPASS_NG";

		q_id = *(u_int *)imsg->data;
		if (datalen > (ssize_t)sizeof(u_int))
			msg = (char *)(((u_int *)imsg->data) + 1);

		q = radiusd_find_query(module->radiusd, q_id);
		if (q == NULL) {
			log_warnx("Received %s from `%s', but query id=%u "
			    "unknown", msgtypestr, module->name, q_id);
			break;
		}

		if ((q->res = radius_new_response_packet(
		    (imsg->hdr.type == IMSG_RADIUSD_MODULE_USERPASS_OK)
		    ? RADIUS_CODE_ACCESS_ACCEPT : RADIUS_CODE_ACCESS_REJECT,
		    q->req)) == NULL) {
			log_warn("radius_new_response_packet() failed");
			radiusd_access_request_aborted(q);
		} else {
			if (msg)
				radius_put_string_attr(q->res,
				    RADIUS_TYPE_REPLY_MESSAGE, msg);
		}
		radius_set_response_authenticator(q->res,
		    q->client->secret);
		radiusd_access_request_answer(q);
		break;
	    }
	case IMSG_RADIUSD_MODULE_ACCSREQ_ANSWER:
	    {
		static struct radiusd_module_radpkt_arg *ans;
		if (datalen <
		    (ssize_t)sizeof(struct radiusd_module_radpkt_arg)) {
			log_warnx("Received ACCSREQ_ANSWER message, but "
			    "length is wrong");
			break;
		}
		q_id = ((struct radiusd_module_radpkt_arg *)imsg->data)->q_id;
		q = radiusd_find_query(module->radiusd, q_id);
		if (q == NULL) {
			log_warnx("Received ACCSREQ_ANSWER from %s, but query "
			    "id=%u unknown", module->name, q_id);
			break;
		}
		if ((ans = radiusd_module_recv_radpkt(module, imsg,
		    IMSG_RADIUSD_MODULE_ACCSREQ_ANSWER,
		    "ACCSREQ_ANSWER")) != NULL) {
			q->res = radius_convert_packet(
			    module->radpkt, module->radpktoff);
			q->res_modified = ans->modified;
			radiusd_access_request_answer(q);
			module->radpktoff = 0;
		}
		break;
	    }
	case IMSG_RADIUSD_MODULE_ACCSREQ_ABORTED:
	    {
		q_id = *((u_int *)imsg->data);
		q = radiusd_find_query(module->radiusd, q_id);
		if (q == NULL) {
			log_warnx("Received ACCSREQ_ABORT from %s, but query "
			    "id=%u unknown", module->name, q_id);
			break;
		}
		radiusd_access_request_aborted(q);
		break;
	    }
	default:
		RADIUSD_DBG(("Unhandled imsg type=%d",
		    imsg->hdr.type));
	}
}

static struct radiusd_module_radpkt_arg *
radiusd_module_recv_radpkt(struct radiusd_module *module, struct imsg *imsg,
    uint32_t imsg_type, const char *type_str)
{
	struct radiusd_module_radpkt_arg	*ans;
	int					 datalen, chunklen;

	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;
	ans = (struct radiusd_module_radpkt_arg *)imsg->data;
	if (module->radpktsiz < ans->datalen) {
		u_char *nradpkt;
		if ((nradpkt = realloc(module->radpkt, ans->datalen)) == NULL) {
			log_warn("Could not handle received %s message from "
			    "`%s'", type_str, module->name);
			goto on_fail;
		}
		module->radpkt = nradpkt;
		module->radpktsiz = ans->datalen;
	}
	chunklen = datalen - sizeof(struct radiusd_module_radpkt_arg);
	if (chunklen > module->radpktsiz - module->radpktoff) {
		log_warnx("Could not handle received %s message from `%s': "
		    "received length is too big", type_str, module->name);
		goto on_fail;
	}
	memcpy(module->radpkt + module->radpktoff,
	    (caddr_t)(ans + 1), chunklen);
	module->radpktoff += chunklen;
	if (!ans->final)
		return (NULL);	/* again */
	if (module->radpktoff != module->radpktsiz) {
		log_warnx("Could not handle received %s message from `%s': "
		    "length is mismatch", type_str, module->name);
		goto on_fail;
	}

	return (ans);
on_fail:
	module->radpktoff = 0;
	return (NULL);
}

int
radiusd_module_set(struct radiusd_module *module, const char *name,
    int argc, char * const * argv)
{
	struct radiusd_module_set_arg	 arg;
	struct radiusd_module_object	*val;
	int				 i, niov = 0;
	u_char				*buf = NULL, *buf0;
	ssize_t				 n;
	size_t				 bufsiz = 0, bufoff = 0, bufsiz0;
	size_t				 vallen, valsiz;
	struct iovec			 iov[2];
	struct imsg			 imsg;

	memset(&arg, 0, sizeof(arg));
	arg.nparamval = argc;
	strlcpy(arg.paramname, name, sizeof(arg.paramname));

	iov[niov].iov_base = &arg;
	iov[niov].iov_len = sizeof(struct radiusd_module_set_arg);
	niov++;

	for (i = 0; i < argc; i++) {
		vallen = strlen(argv[i]) + 1;
		valsiz = sizeof(struct radiusd_module_object) + vallen;
		if (bufsiz < bufoff + valsiz) {
			bufsiz0 = bufoff + valsiz + 128;
			if ((buf0 = realloc(buf, bufsiz0)) == NULL) {
				log_warn("Failed to set config parameter to "
				    "module `%s': realloc", module->name);
				goto on_error;
			}
			buf = buf0;
			bufsiz = bufsiz0;
			memset(buf + bufoff, 0, bufsiz - bufoff);
		}
		val = (struct radiusd_module_object *)(buf + bufoff);
		val->size = valsiz;
		memcpy(val + 1, argv[i], vallen);

		bufoff += valsiz;
	}
	iov[niov].iov_base = buf;
	iov[niov].iov_len = bufoff;
	niov++;

	if (imsg_composev(&module->ibuf, IMSG_RADIUSD_MODULE_SET_CONFIG, 0, 0,
	    -1, iov, niov) == -1) {
		log_warn("Failed to set config parameter to module `%s': "
		    "imsg_composev", module->name);
		goto on_error;
	}
	if (imsg_sync_flush(&module->ibuf, MODULE_IO_TIMEOUT) == -1) {
		log_warn("Failed to set config parameter to module `%s': "
		    "imsg_flush_timeout", module->name);
		goto on_error;
	}
	for (;;) {
		if (imsg_sync_read(&module->ibuf, MODULE_IO_TIMEOUT) <= 0) {
			log_warn("Failed to get reply from module `%s': "
			    "imsg_sync_read", module->name);
			goto on_error;
		}
		if ((n = imsg_get(&module->ibuf, &imsg)) > 0)
			break;
		if (n < 0) {
			log_warn("Failed to get reply from module `%s': "
			    "imsg_get", module->name);
			goto on_error;
		}
	}
	if (imsg.hdr.type == IMSG_NG) {
		log_warnx("Could not set `%s' for module `%s': %s", name,
		    module->name, (char *)imsg.data);
		goto on_error;
	} else if (imsg.hdr.type != IMSG_OK) {
		imsg_free(&imsg);
		log_warnx("Failed to get reply from module `%s': "
		    "unknown imsg type=%d", module->name, imsg.hdr.type);
		goto on_error;
	}
	imsg_free(&imsg);
	radiusd_module_reset_ev_handler(module);

	free(buf);
	return (0);

on_error:
	radiusd_module_reset_ev_handler(module);
	if (buf != NULL)
		free(buf);
	return (-1);
}

static void
radiusd_module_userpass(struct radiusd_module *module, struct radius_query *q)
{
	struct radiusd_module_userpass_arg userpass;

	memset(&userpass, 0, sizeof(userpass));
	userpass.q_id = q->id;

	if (radius_get_user_password_attr(q->req, userpass.pass,
	    sizeof(userpass.pass), q->client->secret) == 0)
		userpass.has_pass = true;
	else
		userpass.has_pass = false;

	if (strlcpy(userpass.user, q->username, sizeof(userpass.user))
	    >= sizeof(userpass.user)) {
		log_warnx("Could request USERPASS to module `%s': "
		    "User-Name too long", module->name);
		goto on_error;
	}
	imsg_compose(&module->ibuf, IMSG_RADIUSD_MODULE_USERPASS, 0, 0, -1,
	    &userpass, sizeof(userpass));
	radiusd_module_reset_ev_handler(module);
	return;
on_error:
	radiusd_access_request_aborted(q);
}

static void
radiusd_module_access_request(struct radiusd_module *module,
    struct radius_query *q)
{
	struct radiusd_module_radpkt_arg	 accsreq;
	struct iovec				 iov[2];
	int					 off = 0, len, siz;
	const u_char				*pkt;
	RADIUS_PACKET				*radpkt;
	char					 pass[256];

	if ((radpkt = radius_convert_packet(radius_get_data(q->req),
	    radius_get_length(q->req))) == NULL) {
		log_warn("Could not send ACCSREQ for `%s'", module->name);
		return;
	}
	if (q->client->secret[0] != '\0' && module->secret != NULL &&
	    radius_get_user_password_attr(radpkt, pass, sizeof(pass),
		    q->client->secret) == 0) {
		radius_del_attr_all(radpkt, RADIUS_TYPE_USER_PASSWORD);
		(void)radius_put_raw_attr(radpkt, RADIUS_TYPE_USER_PASSWORD,
		    pass, strlen(pass));
	}

	pkt = radius_get_data(radpkt);
	len = radius_get_length(radpkt);
	memset(&accsreq, 0, sizeof(accsreq));
	accsreq.q_id = q->id;
	while (off < len) {
		siz = MAX_IMSGSIZE - sizeof(accsreq);
		if (len - off > siz) {
			accsreq.final = false;
			accsreq.datalen = siz;
		} else {
			accsreq.final = true;
			accsreq.datalen = len - off;
		}
		iov[0].iov_base = &accsreq;
		iov[0].iov_len = sizeof(accsreq);
		iov[1].iov_base = (caddr_t)pkt + off;
		iov[1].iov_len = accsreq.datalen;
		imsg_composev(&module->ibuf, IMSG_RADIUSD_MODULE_ACCSREQ, 0, 0,
		    -1, iov, 2);
		off += accsreq.datalen;
	}
	radiusd_module_reset_ev_handler(module);
	radius_delete_packet(radpkt);

	return;
}
