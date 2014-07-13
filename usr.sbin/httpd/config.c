/*	$OpenBSD: config.c,v 1.2 2014/07/13 14:17:37 reyk Exp $	*/

/*
 * Copyright (c) 2011 - 2014 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/pfvar.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <net/route.h>

#include <ctype.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <ifaddrs.h>

#include <openssl/ssl.h>

#include "httpd.h"

int
config_init(struct httpd *env)
{
	struct privsep	*ps = env->sc_ps;
	u_int		 what;

	/* Global configuration */
	if (privsep_process == PROC_PARENT) {
		env->sc_prefork_server = SERVER_NUMPROC;

		ps->ps_what[PROC_PARENT] = CONFIG_ALL;
		ps->ps_what[PROC_SERVER] = CONFIG_SERVERS|CONFIG_MEDIA;
	}

	/* Other configuration */
	what = ps->ps_what[privsep_process];

	if (what & CONFIG_SERVERS) {
		if ((env->sc_servers =
		    calloc(1, sizeof(*env->sc_servers))) == NULL)
			return (-1);
		TAILQ_INIT(env->sc_servers);
	}

	if (what & CONFIG_MEDIA) {
		if ((env->sc_mediatypes =
		    calloc(1, sizeof(*env->sc_mediatypes))) == NULL)
			return (-1);
		RB_INIT(env->sc_mediatypes);
	}

	return (0);
}

void
config_purge(struct httpd *env, u_int reset)
{
	struct privsep		*ps = env->sc_ps;
	struct server		*srv;
	u_int			 what;

	what = ps->ps_what[privsep_process] & reset;

	if (what & CONFIG_SERVERS && env->sc_servers != NULL) {
		while ((srv = TAILQ_FIRST(env->sc_servers)) != NULL) {
			TAILQ_REMOVE(env->sc_servers, srv, srv_entry);
			free(srv);
		}
	}

	if (what & CONFIG_MEDIA && env->sc_mediatypes != NULL)
		media_purge(env->sc_mediatypes);
}

int
config_setreset(struct httpd *env, u_int reset)
{
	struct privsep	*ps = env->sc_ps;
	int		 id;

	for (id = 0; id < PROC_MAX; id++) {
		if ((reset & ps->ps_what[id]) == 0 ||
		    id == privsep_process)
			continue;
		proc_compose_imsg(ps, id, -1, IMSG_CTL_RESET, -1,
		    &reset, sizeof(reset));
	}

	return (0);
}

int
config_getreset(struct httpd *env, struct imsg *imsg)
{
	u_int		 mode;

	IMSG_SIZE_CHECK(imsg, &mode);
	memcpy(&mode, imsg->data, sizeof(mode));

	config_purge(env, mode);

	return (0);
}

int
config_getcfg(struct httpd *env, struct imsg *imsg)
{
	struct privsep		*ps = env->sc_ps;
	struct ctl_flags	 cf;
	u_int			 what;

	if (IMSG_DATA_SIZE(imsg) != sizeof(cf))
		return (0); /* ignore */

	/* Update runtime flags */
	memcpy(&cf, imsg->data, sizeof(cf));
	env->sc_opts = cf.cf_opts;
	env->sc_flags = cf.cf_flags;

	what = ps->ps_what[privsep_process];

	if (privsep_process != PROC_PARENT)
		proc_compose_imsg(env->sc_ps, PROC_PARENT, -1,
		    IMSG_CFG_DONE, -1, NULL, 0);

	return (0);
}

int
config_setserver(struct httpd *env, struct server *srv)
{
	struct privsep		*ps = env->sc_ps;
	struct server_config	 s;
	int			 id;
	int			 fd, n, m;
	struct iovec		 iov[6];
	size_t			 c;
	u_int			 what;

	/* opens listening sockets etc. */
	if (server_privinit(srv) == -1)
		return (-1);

	for (id = 0; id < PROC_MAX; id++) {
		what = ps->ps_what[id];

		if ((what & CONFIG_SERVERS) == 0 || id == privsep_process)
			continue;

		DPRINTF("%s: sending server %s to %s fd %d", __func__,
		    srv->srv_conf.name, ps->ps_title[id], srv->srv_s);

		memcpy(&s, &srv->srv_conf, sizeof(s));

		c = 0;
		iov[c].iov_base = &s;
		iov[c++].iov_len = sizeof(s);

		if (id == PROC_SERVER) {
			/* XXX imsg code will close the fd after 1st call */
			n = -1;
			proc_range(ps, id, &n, &m);
			for (n = 0; n < m; n++) {
				if ((fd = dup(srv->srv_s)) == -1)
					return (-1);
				proc_composev_imsg(ps, id, n,
				    IMSG_CFG_SERVER, fd, iov, c);
			}
		} else {
			proc_composev_imsg(ps, id, -1, IMSG_CFG_SERVER, -1,
			    iov, c);
		}
	}

	close(srv->srv_s);
	srv->srv_s = -1;

	return (0);
}

int
config_getserver(struct httpd *env, struct imsg *imsg)
{
#ifdef DEBUG
	struct privsep		*ps = env->sc_ps;
#endif
	struct server		*srv;
	u_int8_t		*p = imsg->data;
	size_t			 s;

	if ((srv = calloc(1, sizeof(*srv))) == NULL) {
		close(imsg->fd);
		return (-1);
	}

	IMSG_SIZE_CHECK(imsg, &srv->srv_conf);
	memcpy(&srv->srv_conf, p, sizeof(srv->srv_conf));
	s = sizeof(srv->srv_conf);

	srv->srv_s = imsg->fd;

	SPLAY_INIT(&srv->srv_clients);
	TAILQ_INSERT_TAIL(env->sc_servers, srv, srv_entry);

	DPRINTF("%s: %s %d received configuration \"%s\"", __func__,
	    ps->ps_title[privsep_process], ps->ps_instance,
	    srv->srv_conf.name);

	return (0);
}

int
config_setmedia(struct httpd *env, struct media_type *media)
{
	struct privsep		*ps = env->sc_ps;
	int			 id;
	u_int			 what;

	for (id = 0; id < PROC_MAX; id++) {
		what = ps->ps_what[id];

		if ((what & CONFIG_MEDIA) == 0 || id == privsep_process)
			continue;

		DPRINTF("%s: sending media \"%s\" to %s", __func__,
		    media->media_name, ps->ps_title[id]);

		proc_compose_imsg(ps, id, -1, IMSG_CFG_MEDIA, -1,
		    media, sizeof(*media));
	}

	return (0);
}

int
config_getmedia(struct httpd *env, struct imsg *imsg)
{
#ifdef DEBUG
	struct privsep		*ps = env->sc_ps;
#endif
	struct media_type	 media;
	u_int8_t		*p = imsg->data;

	IMSG_SIZE_CHECK(imsg, &media);
	memcpy(&media, p, sizeof(media));

	if (media_add(env->sc_mediatypes, &media) == NULL) {
		log_debug("%s: failed to add media \"%s\"",
		    __func__, media.media_name);
		return (-1);
	}

	DPRINTF("%s: %s %d received media \"%s\"", __func__,
	    ps->ps_title[privsep_process], ps->ps_instance,
	    media.media_name);

	return (0);
}
