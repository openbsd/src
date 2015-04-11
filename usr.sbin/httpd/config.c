/*	$OpenBSD: config.c,v 1.37 2015/04/11 14:52:49 jsing Exp $	*/

/*
 * Copyright (c) 2011 - 2015 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/tree.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <imsg.h>

#include "httpd.h"

int	 config_getserver_config(struct httpd *, struct server *,
	    struct imsg *);
int	 config_getserver_auth(struct httpd *, struct server_config *);

int
config_init(struct httpd *env)
{
	struct privsep	*ps = env->sc_ps;
	u_int		 what;

	/* Global configuration */
	if (privsep_process == PROC_PARENT) {
		env->sc_prefork_server = SERVER_NUMPROC;

		ps->ps_what[PROC_PARENT] = CONFIG_ALL;
		ps->ps_what[PROC_SERVER] =
		    CONFIG_SERVERS|CONFIG_MEDIA|CONFIG_AUTH;
		ps->ps_what[PROC_LOGGER] = CONFIG_SERVERS;
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

	if (what & CONFIG_AUTH) {
		if ((env->sc_auth =
		    calloc(1, sizeof(*env->sc_auth))) == NULL)
			return (-1);
		TAILQ_INIT(env->sc_auth);
	}

	return (0);
}

void
config_purge(struct httpd *env, u_int reset)
{
	struct privsep		*ps = env->sc_ps;
	struct server		*srv;
	struct auth		*auth;
	u_int			 what;

	what = ps->ps_what[privsep_process] & reset;

	if (what & CONFIG_SERVERS && env->sc_servers != NULL) {
		while ((srv = TAILQ_FIRST(env->sc_servers)) != NULL)
			server_purge(srv);
	}

	if (what & CONFIG_MEDIA && env->sc_mediatypes != NULL)
		media_purge(env->sc_mediatypes);

	if (what & CONFIG_AUTH && env->sc_auth != NULL) {
		while ((auth = TAILQ_FIRST(env->sc_auth)) != NULL) {
			auth_free(env->sc_auth, auth);
			free(auth);
		}
	}
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

		DPRINTF("%s: sending %s \"%s[%u]\" to %s fd %d", __func__,
		    (srv->srv_conf.flags & SRVFLAG_LOCATION) ?
		    "location" : "server",
		    srv->srv_conf.name, srv->srv_conf.id,
		    ps->ps_title[id], srv->srv_s);

		memcpy(&s, &srv->srv_conf, sizeof(s));

		c = 0;
		iov[c].iov_base = &s;
		iov[c++].iov_len = sizeof(s);
		if (srv->srv_conf.return_uri_len != 0) {
			iov[c].iov_base = srv->srv_conf.return_uri;
			iov[c++].iov_len = srv->srv_conf.return_uri_len;
		}
		if (srv->srv_conf.tls_cert_len != 0) {
			iov[c].iov_base = srv->srv_conf.tls_cert;
			iov[c++].iov_len = srv->srv_conf.tls_cert_len;
		}
		if (srv->srv_conf.tls_key_len != 0) {
			iov[c].iov_base = srv->srv_conf.tls_key;
			iov[c++].iov_len = srv->srv_conf.tls_key_len;
		}

		if (id == PROC_SERVER &&
		    (srv->srv_conf.flags & SRVFLAG_LOCATION) == 0) {
			/* XXX imsg code will close the fd after 1st call */
			n = -1;
			proc_range(ps, id, &n, &m);
			for (n = 0; n < m; n++) {
				if (srv->srv_s == -1)
					fd = -1;
				else if ((fd = dup(srv->srv_s)) == -1)
					return (-1);
				if (proc_composev_imsg(ps, id, n,
				    IMSG_CFG_SERVER, fd, iov, c) != 0) {
					log_warn("%s: failed to compose "
					    "IMSG_CFG_SERVER imsg for `%s'",
					    __func__, srv->srv_conf.name);
					return (-1);
				}
			}
		} else {
			if (proc_composev_imsg(ps, id, -1, IMSG_CFG_SERVER, -1,
			    iov, c) != 0) {
				log_warn("%s: failed to compose "
				    "IMSG_CFG_SERVER imsg for `%s'",
				    __func__, srv->srv_conf.name);
				return (-1);
			}
		}
	}

	return (0);
}

int
config_getserver_auth(struct httpd *env, struct server_config *srv_conf)
{
	struct privsep		*ps = env->sc_ps;

	if ((ps->ps_what[privsep_process] & CONFIG_AUTH) == 0 ||
	    (srv_conf->flags & SRVFLAG_AUTH) == 0)
		return (0);

	if ((srv_conf->auth = auth_byid(env->sc_auth,
	    srv_conf->auth_id)) == NULL)
		return (-1);

	return (0);
}

int
config_getserver_config(struct httpd *env, struct server *srv,
    struct imsg *imsg)
{
#ifdef DEBUG
	struct privsep		*ps = env->sc_ps;
#endif
	struct server_config	*srv_conf, *parent;
	u_int8_t		*p = imsg->data;
	u_int			 f;
	size_t			 s;

	if ((srv_conf = calloc(1, sizeof(*srv_conf))) == NULL)
		return (-1);

	IMSG_SIZE_CHECK(imsg, srv_conf);
	memcpy(srv_conf, p, sizeof(*srv_conf));
	s = sizeof(*srv_conf);

	/* Reset these variables to avoid free'ing invalid pointers */
	serverconfig_reset(srv_conf);

	TAILQ_FOREACH(parent, &srv->srv_hosts, entry) {
		if (strcmp(parent->name, srv_conf->name) == 0)
			break;
	}
	if (parent == NULL)
		parent = &srv->srv_conf;

	if (config_getserver_auth(env, srv_conf) != 0)
		goto fail;

	/*
	 * Get variable-length values for the virtual host.  The tls_* ones
	 * aren't needed in the virtual hosts unless we implement SNI.
	 */
	if (srv_conf->return_uri_len != 0) {
		if ((srv_conf->return_uri = get_data(p + s,
		    srv_conf->return_uri_len)) == NULL)
			goto fail;
		s += srv_conf->return_uri_len;
	}

	if (srv_conf->flags & SRVFLAG_LOCATION) {
		/* Inherit configuration from the parent */
		f = SRVFLAG_INDEX|SRVFLAG_NO_INDEX;
		if ((srv_conf->flags & f) == 0) {
			srv_conf->flags |= parent->flags & f;
			(void)strlcpy(srv_conf->index, parent->index,
			    sizeof(srv_conf->index));
		}

		f = SRVFLAG_AUTO_INDEX|SRVFLAG_NO_AUTO_INDEX;
		if ((srv_conf->flags & f) == 0)
			srv_conf->flags |= parent->flags & f;

		f = SRVFLAG_SOCKET|SRVFLAG_FCGI;
		if ((srv_conf->flags & f) == SRVFLAG_FCGI) {
			srv_conf->flags |= f;
			(void)strlcpy(srv_conf->socket, HTTPD_FCGI_SOCKET,
			    sizeof(srv_conf->socket));
		}

		f = SRVFLAG_ROOT;
		if ((srv_conf->flags & f) == 0) {
			srv_conf->flags |= parent->flags & f;
			(void)strlcpy(srv_conf->root, parent->root,
			    sizeof(srv_conf->root));
		}

		f = SRVFLAG_FCGI|SRVFLAG_NO_FCGI;
		if ((srv_conf->flags & f) == 0)
			srv_conf->flags |= parent->flags & f;

		f = SRVFLAG_LOG|SRVFLAG_NO_LOG;
		if ((srv_conf->flags & f) == 0) {
			srv_conf->flags |= parent->flags & f;
			srv_conf->logformat = parent->logformat;
		}

		f = SRVFLAG_SYSLOG|SRVFLAG_NO_SYSLOG;
		if ((srv_conf->flags & f) == 0)
			srv_conf->flags |= parent->flags & f;

		f = SRVFLAG_AUTH|SRVFLAG_NO_AUTH;
		if ((srv_conf->flags & f) == 0) {
			srv_conf->flags |= parent->flags & f;
			srv_conf->auth = parent->auth;
			srv_conf->auth_id = parent->auth_id;
			(void)strlcpy(srv_conf->auth_realm,
			    parent->auth_realm,
			    sizeof(srv_conf->auth_realm));
		}

		f = SRVFLAG_TLS;
		srv_conf->flags |= parent->flags & f;

		f = SRVFLAG_ACCESS_LOG;
		if ((srv_conf->flags & f) == 0) {
			srv_conf->flags |= parent->flags & f;
			(void)strlcpy(srv_conf->accesslog,
			    parent->accesslog,
			    sizeof(srv_conf->accesslog));
		}

		f = SRVFLAG_ERROR_LOG;
		if ((srv_conf->flags & f) == 0) {
			srv_conf->flags |= parent->flags & f;
			(void)strlcpy(srv_conf->errorlog,
			    parent->errorlog,
			    sizeof(srv_conf->errorlog));
		}

		f = SRVFLAG_BLOCK|SRVFLAG_NO_BLOCK;
		if ((srv_conf->flags & f) == 0) {
			free(srv_conf->return_uri);
			srv_conf->flags |= parent->flags & f;
			srv_conf->return_code = parent->return_code;
			srv_conf->return_uri_len = parent->return_uri_len;
			if (srv_conf->return_uri_len &&
			    (srv_conf->return_uri =
			    strdup(parent->return_uri)) == NULL)
				goto fail;
		}

		memcpy(&srv_conf->timeout, &parent->timeout,
		    sizeof(srv_conf->timeout));
		srv_conf->maxrequests = parent->maxrequests;
		srv_conf->maxrequestbody = parent->maxrequestbody;

		DPRINTF("%s: %s %d location \"%s\", "
		    "parent \"%s[%u]\", flags: %s",
		    __func__, ps->ps_title[privsep_process], ps->ps_instance,
		    srv_conf->location, parent->name, parent->id,
		    printb_flags(srv_conf->flags, SRVFLAG_BITS));
	} else {
		/* Add a new "virtual" server */
		DPRINTF("%s: %s %d server \"%s[%u]\", parent \"%s[%u]\", "
		    "flags: %s", __func__,
		    ps->ps_title[privsep_process], ps->ps_instance,
		    srv_conf->name, srv_conf->id, parent->name, parent->id,
		    printb_flags(srv_conf->flags, SRVFLAG_BITS));
	}

	TAILQ_INSERT_TAIL(&srv->srv_hosts, srv_conf, entry);

	return (0);

 fail:
	serverconfig_free(srv_conf);
	free(srv_conf);
	return (-1);
}

int
config_getserver(struct httpd *env, struct imsg *imsg)
{
#ifdef DEBUG
	struct privsep		*ps = env->sc_ps;
#endif
	struct server		*srv = NULL;
	struct server_config	 srv_conf;
	u_int8_t		*p = imsg->data;
	size_t			 s;

	IMSG_SIZE_CHECK(imsg, &srv_conf);
	memcpy(&srv_conf, p, sizeof(srv_conf));
	s = sizeof(srv_conf);

	/* Reset these variables to avoid free'ing invalid pointers */
	serverconfig_reset(&srv_conf);

	if ((IMSG_DATA_SIZE(imsg) - s) <
	    (srv_conf.tls_cert_len + srv_conf.tls_key_len +
	    srv_conf.return_uri_len)) {
		log_debug("%s: invalid message length", __func__);
		goto fail;
	}

	/* Check if server with matching listening socket already exists */
	if ((srv = server_byaddr((struct sockaddr *)
	    &srv_conf.ss, srv_conf.port)) != NULL) {
		/* Add "host" to existing listening server */
		if (imsg->fd != -1) {
			if (srv->srv_s == -1)
				srv->srv_s = imsg->fd;
			else
				close(imsg->fd);
		}
		return (config_getserver_config(env, srv, imsg));
	}

	if (srv_conf.flags & SRVFLAG_LOCATION)
		fatalx("invalid location");

	/* Otherwise create a new server */
	if ((srv = calloc(1, sizeof(*srv))) == NULL)
		goto fail;

	memcpy(&srv->srv_conf, &srv_conf, sizeof(srv->srv_conf));
	srv->srv_s = imsg->fd;

	if (config_getserver_auth(env, &srv->srv_conf) != 0)
		goto fail;

	SPLAY_INIT(&srv->srv_clients);
	TAILQ_INIT(&srv->srv_hosts);

	TAILQ_INSERT_TAIL(&srv->srv_hosts, &srv->srv_conf, entry);
	TAILQ_INSERT_TAIL(env->sc_servers, srv, srv_entry);

	DPRINTF("%s: %s %d configuration \"%s[%u]\", flags: %s", __func__,
	    ps->ps_title[privsep_process], ps->ps_instance,
	    srv->srv_conf.name, srv->srv_conf.id,
	    printb_flags(srv->srv_conf.flags, SRVFLAG_BITS));

	/*
	 * Get all variable-length values for the parent server.
	 */
	if (srv->srv_conf.return_uri_len != 0) {
		if ((srv->srv_conf.return_uri = get_data(p + s,
		    srv->srv_conf.return_uri_len)) == NULL)
			goto fail;
		s += srv->srv_conf.return_uri_len;
	}
	if (srv->srv_conf.tls_cert_len != 0) {
		if ((srv->srv_conf.tls_cert = get_data(p + s,
		    srv->srv_conf.tls_cert_len)) == NULL)
			goto fail;
		s += srv->srv_conf.tls_cert_len;
	}
	if (srv->srv_conf.tls_key_len != 0) {
		if ((srv->srv_conf.tls_key = get_data(p + s,
		    srv->srv_conf.tls_key_len)) == NULL)
			goto fail;
		s += srv->srv_conf.tls_key_len;
	}

	return (0);

 fail:
	if (imsg->fd != -1)
		close(imsg->fd);
	if (srv != NULL) {
		free(srv->srv_conf.tls_cert);
		free(srv->srv_conf.tls_key);
	}
	free(srv);

	return (-1);
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

int
config_setauth(struct httpd *env, struct auth *auth)
{
	struct privsep		*ps = env->sc_ps;
	int			 id;
	u_int			 what;

	for (id = 0; id < PROC_MAX; id++) {
		what = ps->ps_what[id];

		if ((what & CONFIG_AUTH) == 0 || id == privsep_process)
			continue;

		DPRINTF("%s: sending auth \"%s[%u]\" to %s", __func__,
		    auth->auth_htpasswd, auth->auth_id, ps->ps_title[id]);

		proc_compose_imsg(ps, id, -1, IMSG_CFG_AUTH, -1,
		    auth, sizeof(*auth));
	}

	return (0);
}

int
config_getauth(struct httpd *env, struct imsg *imsg)
{
#ifdef DEBUG
	struct privsep		*ps = env->sc_ps;
#endif
	struct auth		 auth;
	u_int8_t		*p = imsg->data;

	IMSG_SIZE_CHECK(imsg, &auth);
	memcpy(&auth, p, sizeof(auth));

	if (auth_add(env->sc_auth, &auth) == NULL) {
		log_debug("%s: failed to add auth \"%s[%u]\"",
		    __func__, auth.auth_htpasswd, auth.auth_id);
		return (-1);
	}

	DPRINTF("%s: %s %d received auth \"%s[%u]\"", __func__,
	    ps->ps_title[privsep_process], ps->ps_instance,
	    auth.auth_htpasswd, auth.auth_id);

	return (0);
}
