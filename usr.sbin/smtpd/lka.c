/*	$OpenBSD: lka.c,v 1.148 2013/01/28 11:09:53 gilles Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2012 Eric Faurot <eric@faurot.net>
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
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pwd.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"

static void lka_imsg(struct mproc *, struct imsg *);
static void lka_shutdown(void);
static void lka_sig_handler(int, short, void *);
static int lka_authenticate(const char *, const char *, const char *);
static int lka_credentials(const char *, const char *, char *, size_t);
static int lka_userinfo(const char *, const char *, struct userinfo *);
static int lka_addrname(const char *, const struct sockaddr *,
    struct addrname *);
static int lka_X509_verify(struct ca_vrfy_req_msg *, const char *, const char *);

static void
lka_imsg(struct mproc *p, struct imsg *imsg)
{
	struct rule		*rule;
	struct table		*table;
	void			*tmp;
	int			 ret;
	const char		*key, *val;
	char			*src;
	struct ssl		*ssl;
	struct iovec		iov[3];
	static struct dict	*ssl_dict;
	static struct dict	*tables_dict;
	static struct tree	*tables_tree;
	static struct table	*table_last;
	static struct ca_vrfy_req_msg	*req_ca_vrfy_smtp = NULL;
	static struct ca_vrfy_req_msg	*req_ca_vrfy_mta = NULL;
	struct ca_vrfy_req_msg		*req_ca_vrfy_chain;
	struct ca_vrfy_resp_msg		resp_ca_vrfy;
	struct ca_cert_req_msg		*req_ca_cert;
	struct ca_cert_resp_msg		 resp_ca_cert;
	struct sockaddr_storage	 ss;
	struct addrinfo		 hints, *ai;
	struct userinfo		 userinfo;
	struct addrname		 addrname;
	struct envelope		 evp;
	struct msg		 m;
	char			 buf[MAX_LINE_SIZE];
	const char		*tablename, *username, *password, *label;
	uint64_t		 reqid;
	size_t			 i, len;
	int			 v;

	if (imsg->hdr.type == IMSG_DNS_HOST ||
	    imsg->hdr.type == IMSG_DNS_PTR ||
	    imsg->hdr.type == IMSG_DNS_MX ||
	    imsg->hdr.type == IMSG_DNS_MX_PREFERENCE) {
		dns_imsg(p, imsg);
		return;
	}

	if (p->proc == PROC_SMTP) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_EXPAND_RCPT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_envelope(&m, &evp);
			m_end(&m);
			lka_session(reqid, &evp);
			return;

		case IMSG_LKA_SSL_INIT:
			req_ca_cert = imsg->data;
			resp_ca_cert.reqid = req_ca_cert->reqid;

			ssl = dict_get(env->sc_ssl_dict, req_ca_cert->name);
			if (ssl == NULL) {
				resp_ca_cert.status = CA_FAIL;
				m_compose(p, IMSG_LKA_SSL_INIT, 0, 0, -1, &resp_ca_cert,
				    sizeof(resp_ca_cert));
				return;
			}
			resp_ca_cert.status = CA_OK;
			resp_ca_cert.cert_len = ssl->ssl_cert_len;
			resp_ca_cert.key_len = ssl->ssl_key_len;
			iov[0].iov_base = &resp_ca_cert;
			iov[0].iov_len = sizeof(resp_ca_cert);
			iov[1].iov_base = ssl->ssl_cert;
			iov[1].iov_len = ssl->ssl_cert_len;
			iov[2].iov_base = ssl->ssl_key;
			iov[2].iov_len = ssl->ssl_key_len;
			m_composev(p, IMSG_LKA_SSL_INIT, 0, 0, -1, iov, nitems(iov));
			return;

		case IMSG_LKA_SSL_VERIFY_CERT:
			req_ca_vrfy_smtp = xmemdup(imsg->data, sizeof *req_ca_vrfy_smtp, "lka:ca_vrfy");
			if (req_ca_vrfy_smtp == NULL)
				fatal(NULL);
			req_ca_vrfy_smtp->cert = xmemdup((char *)imsg->data +
			    sizeof *req_ca_vrfy_smtp, req_ca_vrfy_smtp->cert_len, "lka:ca_vrfy");
			req_ca_vrfy_smtp->chain_cert = xcalloc(req_ca_vrfy_smtp->n_chain,
			    sizeof (unsigned char *), "lka:ca_vrfy");
			req_ca_vrfy_smtp->chain_cert_len = xcalloc(req_ca_vrfy_smtp->n_chain,
			    sizeof (off_t), "lka:ca_vrfy");
			return;

		case IMSG_LKA_SSL_VERIFY_CHAIN:
			if (req_ca_vrfy_smtp == NULL)
				fatalx("lka:ca_vrfy: chain without a certificate");
			req_ca_vrfy_chain = imsg->data;
			req_ca_vrfy_smtp->chain_cert[req_ca_vrfy_smtp->chain_offset] = xmemdup((char *)imsg->data +
			    sizeof *req_ca_vrfy_chain, req_ca_vrfy_chain->cert_len, "lka:ca_vrfy");
			req_ca_vrfy_smtp->chain_cert_len[req_ca_vrfy_smtp->chain_offset] = req_ca_vrfy_chain->cert_len;
			req_ca_vrfy_smtp->chain_offset++;
			return;

		case IMSG_LKA_SSL_VERIFY:
			if (req_ca_vrfy_smtp == NULL)
				fatalx("lka:ca_vrfy: verify without a certificate");

			resp_ca_vrfy.reqid = req_ca_vrfy_smtp->reqid;

			if (! lka_X509_verify(req_ca_vrfy_smtp, "/etc/ssl/cert.pem", NULL))
				resp_ca_vrfy.status = CA_FAIL;
			else
				resp_ca_vrfy.status = CA_OK;

			m_compose(p, IMSG_LKA_SSL_VERIFY, 0, 0, -1, &resp_ca_vrfy,
			    sizeof resp_ca_vrfy);

			for (i = 0; i < req_ca_vrfy_smtp->n_chain; ++i)
				free(req_ca_vrfy_smtp->chain_cert[i]);
			free(req_ca_vrfy_smtp->chain_cert);
			free(req_ca_vrfy_smtp->chain_cert_len);
			free(req_ca_vrfy_smtp->cert);
			free(req_ca_vrfy_smtp);
			return;

		case IMSG_LKA_AUTHENTICATE:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &tablename);
			m_get_string(&m, &username);
			m_get_string(&m, &password);
			m_end(&m);

			if (!tablename[0]) {
				m_create(p_parent, IMSG_LKA_AUTHENTICATE,
				    0, 0, -1, 128);
				m_add_id(p_parent, reqid);
				m_add_string(p_parent, username);
				m_add_string(p_parent, password);
				m_close(p_parent);
				return;
			}

			ret = lka_authenticate(tablename, username, password);

			m_create(p, IMSG_LKA_AUTHENTICATE, 0, 0, -1, 128);
			m_add_id(p, reqid);
			m_add_int(p, ret);
			m_close(p);
			return;
		}
	}

	if (p->proc == PROC_MDA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_USERINFO:
			m_msg(&m, imsg);
			m_get_string(&m, &tablename);
			m_get_string(&m, &username);
			m_end(&m);

			ret = lka_userinfo(tablename, username, &userinfo);

			len = 32 + strlen(tablename) + strlen(username);
			if (ret == LKA_OK)
				len += sizeof(userinfo);
			m_create(p, IMSG_LKA_USERINFO, 0, 0, -1, len);
			m_add_string(p, tablename);
			m_add_string(p, username);
			m_add_int(p, ret);
			if (ret == LKA_OK)
				m_add_data(p, &userinfo, sizeof(userinfo));
			m_close(p);
			return;
		}
	}

	if (p->proc == PROC_MTA) {
		switch (imsg->hdr.type) {

		case IMSG_LKA_SSL_INIT:
			req_ca_cert = imsg->data;
			resp_ca_cert.reqid = req_ca_cert->reqid;

			ssl = dict_get(env->sc_ssl_dict, req_ca_cert->name);
			if (ssl == NULL) {
				resp_ca_cert.status = CA_FAIL;
				m_compose(p, IMSG_LKA_SSL_INIT, 0, 0, -1, &resp_ca_cert,
				    sizeof(resp_ca_cert));
				return;
			}
			resp_ca_cert.status = CA_OK;
			resp_ca_cert.cert_len = ssl->ssl_cert_len;
			resp_ca_cert.key_len = ssl->ssl_key_len;
			iov[0].iov_base = &resp_ca_cert;
			iov[0].iov_len = sizeof(resp_ca_cert);
			iov[1].iov_base = ssl->ssl_cert;
			iov[1].iov_len = ssl->ssl_cert_len;
			iov[2].iov_base = ssl->ssl_key;
			iov[2].iov_len = ssl->ssl_key_len;
			m_composev(p, IMSG_LKA_SSL_INIT, 0, 0, -1, iov, nitems(iov));
			return;

		case IMSG_LKA_SSL_VERIFY_CERT:
			req_ca_vrfy_mta = xmemdup(imsg->data, sizeof *req_ca_vrfy_mta, "lka:ca_vrfy");
			if (req_ca_vrfy_mta == NULL)
				fatal(NULL);
			req_ca_vrfy_mta->cert = xmemdup((char *)imsg->data +
			    sizeof *req_ca_vrfy_mta, req_ca_vrfy_mta->cert_len, "lka:ca_vrfy");
			req_ca_vrfy_mta->chain_cert = xcalloc(req_ca_vrfy_mta->n_chain,
			    sizeof (unsigned char *), "lka:ca_vrfy");
			req_ca_vrfy_mta->chain_cert_len = xcalloc(req_ca_vrfy_mta->n_chain,
			    sizeof (off_t), "lka:ca_vrfy");
			return;

		case IMSG_LKA_SSL_VERIFY_CHAIN:
			if (req_ca_vrfy_mta == NULL)
				fatalx("lka:ca_vrfy: verify without a certificate");

			req_ca_vrfy_chain = imsg->data;
			req_ca_vrfy_mta->chain_cert[req_ca_vrfy_mta->chain_offset] = xmemdup((char *)imsg->data +
			    sizeof *req_ca_vrfy_chain, req_ca_vrfy_chain->cert_len, "lka:ca_vrfy");
			req_ca_vrfy_mta->chain_cert_len[req_ca_vrfy_mta->chain_offset] = req_ca_vrfy_chain->cert_len;
			req_ca_vrfy_mta->chain_offset++;
			return;

		case IMSG_LKA_SSL_VERIFY:
			if (req_ca_vrfy_mta == NULL)
				fatalx("lka:ca_vrfy: verify without a certificate");

			resp_ca_vrfy.reqid = req_ca_vrfy_mta->reqid;

			if (! lka_X509_verify(req_ca_vrfy_mta, "/etc/ssl/cert.pem", NULL))
				resp_ca_vrfy.status = CA_FAIL;
			else
				resp_ca_vrfy.status = CA_OK;

			m_compose(p, IMSG_LKA_SSL_VERIFY, 0, 0, -1, &resp_ca_vrfy,
			    sizeof resp_ca_vrfy);

			for (i = 0; i < req_ca_vrfy_mta->n_chain; ++i)
				free(req_ca_vrfy_mta->chain_cert[i]);
			free(req_ca_vrfy_mta->chain_cert);
			free(req_ca_vrfy_mta->chain_cert_len);
			free(req_ca_vrfy_mta->cert);
			free(req_ca_vrfy_mta);
			return;

		case IMSG_LKA_SECRET:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &tablename);
			m_get_string(&m, &label);
			m_end(&m);

			lka_credentials(tablename, label, buf, sizeof(buf));

			m_create(p, IMSG_LKA_SECRET, 0, 0, -1, 128);
			m_add_id(p, reqid);
			m_add_string(p, buf);
			m_close(p);
			return;

		case IMSG_LKA_SOURCE:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &tablename);

			table = table_findbyname(tablename);

			m_create(p, IMSG_LKA_SOURCE, 0, 0, -1, 64);
			m_add_id(p, reqid);

			if (table == NULL) {
				log_warn("warn: source address table %s missing",
				    tablename);
				m_add_int(p, LKA_TEMPFAIL);
			} 
			else {
				ret = table_fetch(table, K_SOURCE, &src);
				if (ret == -1)
					m_add_int(p, LKA_TEMPFAIL);
				else if (ret == 0)
					m_add_int(p, LKA_PERMFAIL);
				else {
					m_add_int(p, LKA_OK);

					/* XXX find a nicer way? */
					bzero(&hints, sizeof hints);
					hints.ai_flags = AI_NUMERICHOST;
					getaddrinfo(src, NULL, &hints, &ai);
					free(src);

					m_add_sockaddr(p, ai->ai_addr);
					freeaddrinfo(ai);
				}
			}
			m_close(p);
			return;

		case IMSG_LKA_HELO:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &tablename);
			m_get_sockaddr(&m, (struct sockaddr *)&ss);
			m_end(&m);

			ret = lka_addrname(tablename, (struct sockaddr*)&ss,
			    &addrname);

			m_create(p, IMSG_LKA_HELO, 0, 0, -1, 1024);
			m_add_id(p, reqid);
			m_add_int(p, ret);
			if (ret == LKA_OK)
				m_add_string(p, addrname.name);
			m_close(p);
			return;

		}
	}

	if (p->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CONF_START:
			env->sc_rules_reload = xcalloc(1,
			    sizeof *env->sc_rules, "lka:sc_rules_reload");
			tables_dict = xcalloc(1,
			    sizeof *tables_dict, "lka:tables_dict");
			tables_tree = xcalloc(1,
			    sizeof *tables_tree, "lka:tables_tree");

			ssl_dict = calloc(1, sizeof *ssl_dict);
			if (ssl_dict == NULL)
				fatal(NULL);
			dict_init(ssl_dict);
			dict_init(tables_dict);
			tree_init(tables_tree);
			TAILQ_INIT(env->sc_rules_reload);

			return;

		case IMSG_CONF_SSL:
			ssl = calloc(1, sizeof *ssl);
			if (ssl == NULL)
				fatal(NULL);
			*ssl = *(struct ssl *)imsg->data;
			ssl->ssl_cert = xstrdup((char *)imsg->data +
			    sizeof *ssl, "smtp:ssl_cert");
			ssl->ssl_key = xstrdup((char *)imsg->data +
			    sizeof *ssl + ssl->ssl_cert_len, "smtp:ssl_key");
			if (ssl->ssl_dhparams_len) {
				ssl->ssl_dhparams = xstrdup((char *)imsg->data
				    + sizeof *ssl + ssl->ssl_cert_len +
				    ssl->ssl_key_len, "smtp:ssl_dhparams");
			}
			if (ssl->ssl_ca_len) {
				ssl->ssl_ca = xstrdup((char *)imsg->data
				    + sizeof *ssl + ssl->ssl_cert_len +
				    ssl->ssl_key_len + ssl->ssl_dhparams_len,
				    "smtp:ssl_ca");
			}
			dict_set(ssl_dict, ssl->ssl_name, ssl);
			return;

		case IMSG_CONF_RULE:
			rule = xmemdup(imsg->data, sizeof *rule, "lka:rule");
			TAILQ_INSERT_TAIL(env->sc_rules_reload, rule, r_entry);
			return;

		case IMSG_CONF_TABLE:
			table_last = table = xmemdup(imsg->data, sizeof *table,
			    "lka:table");
			dict_init(&table->t_dict);
			dict_set(tables_dict, table->t_name, table);
			tree_set(tables_tree, table->t_id, table);
			return;

		case IMSG_CONF_RULE_SOURCE:
			rule = TAILQ_LAST(env->sc_rules_reload, rulelist);
			tmp = env->sc_tables_dict;
			env->sc_tables_dict = tables_dict;
			rule->r_sources = table_findbyname(imsg->data);
			if (rule->r_sources == NULL)
				fatalx("lka: tables inconsistency");
			env->sc_tables_dict = tmp;
			return;

		case IMSG_CONF_RULE_SENDER:
			rule = TAILQ_LAST(env->sc_rules_reload, rulelist);
			tmp = env->sc_tables_dict;
			env->sc_tables_dict = tables_dict;
			rule->r_senders = table_findbyname(imsg->data);
			if (rule->r_senders == NULL)
				fatalx("lka: tables inconsistency");
			env->sc_tables_dict = tmp;
			return;

		case IMSG_CONF_RULE_DESTINATION:
			rule = TAILQ_LAST(env->sc_rules_reload, rulelist);
			tmp = env->sc_tables_dict;
			env->sc_tables_dict = tables_dict;
			rule->r_destination = table_findbyname(imsg->data);
			if (rule->r_destination == NULL)
				fatalx("lka: tables inconsistency");
			env->sc_tables_dict = tmp;
			return;

		case IMSG_CONF_RULE_MAPPING:
			rule = TAILQ_LAST(env->sc_rules_reload, rulelist);
			tmp = env->sc_tables_dict;
			env->sc_tables_dict = tables_dict;
			rule->r_mapping = table_findbyname(imsg->data);
			if (rule->r_mapping == NULL)
				fatalx("lka: tables inconsistency");
			env->sc_tables_dict = tmp;
			return;

		case IMSG_CONF_RULE_USERS:
			rule = TAILQ_LAST(env->sc_rules_reload, rulelist);
			tmp = env->sc_tables_dict;
			env->sc_tables_dict = tables_dict;
			rule->r_users = table_findbyname(imsg->data);
			if (rule->r_users == NULL)
				fatalx("lka: tables inconsistency");
			env->sc_tables_dict = tmp;
			return;

		case IMSG_CONF_TABLE_CONTENT:
			table = table_last;
			if (table == NULL)
				fatalx("lka: tables inconsistency");

			key = imsg->data;
			if (table->t_type == T_HASH)
				val = key + strlen(key) + 1;
			else
				val = NULL;

			dict_set(&table->t_dict, key,
			    val ? xstrdup(val, "lka:dict_set") : NULL);
			return;

		case IMSG_CONF_END:

			if (env->sc_rules)
				purge_config(PURGE_RULES);
			if (env->sc_tables_tree) {
				table_close_all();
				purge_config(PURGE_TABLES);
			}
			env->sc_rules = env->sc_rules_reload;
			env->sc_ssl_dict = ssl_dict;
			env->sc_tables_dict = tables_dict;
			env->sc_tables_tree = tables_tree;
			table_open_all();

			ssl_dict = NULL;
			table_last = NULL;
			tables_dict = NULL;
			tables_tree = NULL;

			/* Start fulfilling requests */
			mproc_enable(p_mda);
			mproc_enable(p_mta);
			mproc_enable(p_smtp);
			return;

		case IMSG_CTL_VERBOSE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			log_verbose(v);
			return;

		case IMSG_CTL_PROFILE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			profiling = v;
			return;

		case IMSG_PARENT_FORWARD_OPEN:
			lka_session_forward_reply(imsg->data, imsg->fd);
			return;

		case IMSG_LKA_AUTHENTICATE:
			m_forward(p_smtp, imsg);
			return;
		}
	}

	if (p->proc == PROC_CONTROL) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_UPDATE_TABLE:
			table = table_findbyname(imsg->data);
			if (table == NULL) {
				log_warnx("warn: Lookup table not found: "
				    "\"%s\"", (char *)imsg->data);
				return;
			}
			table_update(table);
			return;
		}
	}

	errx(1, "lka_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
lka_sig_handler(int sig, short event, void *p)
{
	int status;
	pid_t pid;

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		lka_shutdown();
		break;
	case SIGCHLD:
		do {
			pid = waitpid(-1, &status, WNOHANG);
		} while (pid > 0 || (pid == -1 && errno == EINTR));
		break;
	default:
		fatalx("lka_sig_handler: unexpected signal");
	}
}

void
lka_shutdown(void)
{
	log_info("info: lookup agent exiting");
	_exit(0);
}

pid_t
lka(void)
{
	pid_t		 pid;
	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
	struct event	 ev_sigchld;

	switch (pid = fork()) {
	case -1:
		fatal("lka: cannot fork");
	case 0:
		env->sc_pid = getpid();
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	pw = env->sc_pw;

	config_process(PROC_LKA);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("lka: cannot drop privileges");

	imsg_callback = lka_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, lka_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, lka_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGCHLD, lka_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/*
	 * lka opens all kinds of files and sockets, so bump the limit to max.
	 * XXX: need to analyse the exact hard limit.
	 */
	fdlimit(1.0);

	config_peer(PROC_PARENT);
	config_peer(PROC_QUEUE);
	config_peer(PROC_SMTP);
	config_peer(PROC_MDA);
	config_peer(PROC_MTA);
	config_peer(PROC_CONTROL);
	config_done();

	/* Ignore them until we get our config */
	mproc_disable(p_mda);
	mproc_disable(p_mta);
	mproc_disable(p_smtp);

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	lka_shutdown();

	return (0);
}

static int
lka_authenticate(const char *tablename, const char *user, const char *password)
{
	struct table		*table;
	struct credentials	*creds;
	int			 r;

	log_trace(TRACE_LOOKUP, "lookup: authenticating for %s:%s", tablename, user);
	table = table_findbyname(tablename);
	if (table == NULL) {
		log_warnx("warn: could not find table %s needed for authentication",
		    tablename);
		return (LKA_TEMPFAIL);
	}

	switch (table_lookup(table, user, K_CREDENTIALS, (void **)&creds)) {
	case -1:
		log_warnx("warn: user credentials lookup fail for %s:%s",
		    tablename, user);
		return (LKA_TEMPFAIL);
	case 0:
		return (LKA_PERMFAIL);
	default:
		r = !strcmp(creds->password, crypt(password, creds->password));
		free(creds);
		if (r)
			return (LKA_OK);
		return (LKA_PERMFAIL);
	}
}

static int
lka_credentials(const char *tablename, const char *label, char *dst, size_t sz)
{
	struct table		*table;
	struct credentials	*creds;
	char			*buf;
	int			 buflen, r;

	table = table_findbyname(tablename);
	if (table == NULL) {
		log_warnx("warn: credentials table %s missing", tablename);
		return (LKA_TEMPFAIL);
	}

	dst[0] = '\0';

	switch(table_lookup(table, label, K_CREDENTIALS, (void **)&creds)) {
	case -1:
		log_warnx("warn: credentials lookup fail for %s:%s",
		    tablename, label);
		return (LKA_TEMPFAIL);
	case 0:
		log_warnx("warn: credentials not found for %s:%s",
		    tablename, label);
		return (LKA_PERMFAIL);
	default:
		if ((buflen = asprintf(&buf, "%c%s%c%s", '\0',
		    creds->username, '\0', creds->password)) == -1) {
			free(creds);
			log_warn("warn");
			return (LKA_TEMPFAIL);
		}
		free(creds);

		r = __b64_ntop((unsigned char *)buf, buflen, dst, sz);
		free(buf);
		
		if (r == -1) {
			log_warnx("warn: credentials parse error for %s:%s",
			    tablename, label);
			return (LKA_TEMPFAIL);
		}
		return (LKA_OK);
	}
}

static int
lka_userinfo(const char *tablename, const char *username, struct userinfo *res)
{
	struct userinfo *info;
	struct table	*table;

	log_trace(TRACE_LOOKUP, "lookup: userinfo %s:%s", tablename, username);
	table = table_findbyname(tablename);
	if (table == NULL) {
		log_warnx("warn: cannot find user table %s", tablename);
		return (LKA_TEMPFAIL);
	}

	switch (table_lookup(table, username, K_USERINFO, (void **)&info)) {
	case -1:
		log_warnx("warn: failure during userinfo lookup %s:%s",
		    tablename, username);
		return (LKA_TEMPFAIL);
	case 0:
		return (LKA_PERMFAIL);
	default:
		*res = *info;
		free(info);
		return (LKA_OK);
	}
}

static int
lka_addrname(const char *tablename, const struct sockaddr *sa,
    struct addrname *res)
{
	struct addrname *addrname;
	struct table	*table;
	const char	*source;

	source = sa_to_text(sa);

	log_trace(TRACE_LOOKUP, "lookup: helo %s:%s", tablename, source);
	table = table_findbyname(tablename);
	if (table == NULL) {
		log_warnx("warn: cannot find helo table %s", tablename);
		return (LKA_TEMPFAIL);
	}

	switch (table_lookup(table, source, K_ADDRNAME, (void **)&addrname)) {
	case -1:
		log_warnx("warn: failure during helo lookup %s:%s",
		    tablename, source);
		return (LKA_TEMPFAIL);
	case 0:
		return (LKA_PERMFAIL);
	default:
		*res = *addrname;
		free(addrname);
		return (LKA_OK);
	}
}      

static int
lka_X509_verify(struct ca_vrfy_req_msg *vrfy,
    const char *CAfile, const char *CRLfile)
{
	X509			*x509;
	X509			*x509_tmp;
	X509			*x509_tmp2;
	STACK_OF(X509)		*x509_chain;
	const unsigned char    	*d2i;
	size_t			i;
	int			ret = 0;
	const char		*errstr;

	x509 = NULL;
	x509_tmp = NULL;
	x509_chain = NULL;

	d2i = vrfy->cert;
	if (d2i_X509(&x509, &d2i, vrfy->cert_len) == NULL) {
		x509 = NULL;
		goto end;
	}

	if (vrfy->n_chain) {
		x509_chain = sk_X509_new_null();
		for (i = 0; i < vrfy->n_chain; ++i) {
			d2i = vrfy->chain_cert[i];
			if (d2i_X509(&x509_tmp, &d2i, vrfy->chain_cert_len[i]) == NULL) {
				x509_tmp = NULL;
				goto end;
			}

			if ((x509_tmp2 = X509_dup(x509_tmp)) == NULL)
				goto end;
			sk_X509_insert(x509_chain, x509_tmp2, i);
			x509_tmp = x509_tmp2 = NULL;
		}
	}
	if (! ca_X509_verify(x509, x509_chain, CAfile, NULL, &errstr))
		log_trace(TRACE_LOOKUP, "lookup: X509 verify: %s", errstr);
	else
		ret = 1;

end:	
	if (x509)
		X509_free(x509);
	if (x509_tmp)
		X509_free(x509_tmp);
	if (x509_chain)
		sk_X509_pop_free(x509_chain, X509_free);

	return ret;
}
