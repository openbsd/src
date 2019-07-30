/*	$OpenBSD: lka.c,v 1.202 2018/01/03 11:12:21 sunil Exp $	*/

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
#include <limits.h>
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
static int lka_mailaddrmap(const char *, const char *, const struct mailaddr *);
static int lka_X509_verify(struct ca_vrfy_req_msg *, const char *, const char *);
static void lka_certificate_verify(enum imsg_type, struct ca_vrfy_req_msg *);
static void lka_certificate_verify_resume(enum imsg_type, struct ca_vrfy_req_msg *);

static void
lka_imsg(struct mproc *p, struct imsg *imsg)
{
	struct table		*table;
	int			 ret;
	struct pki		*pki;
	struct iovec		iov[2];
	static struct ca_vrfy_req_msg	*req_ca_vrfy = NULL;
	struct ca_vrfy_req_msg		*req_ca_vrfy_chain;
	struct ca_cert_req_msg		*req_ca_cert;
	struct ca_cert_resp_msg		 resp_ca_cert;
	struct sockaddr_storage	 ss;
	struct userinfo		 userinfo;
	struct addrname		 addrname;
	struct envelope		 evp;
	struct mailaddr		 maddr;
	struct msg		 m;
	union lookup		 lk;
	char			 buf[LINE_MAX];
	const char		*tablename, *username, *password, *label;
	uint64_t		 reqid;
	int			 v;

	if (imsg == NULL)
		lka_shutdown();

	switch (imsg->hdr.type) {

	case IMSG_MTA_DNS_HOST:
	case IMSG_MTA_DNS_PTR:
	case IMSG_SMTP_DNS_PTR:
	case IMSG_MTA_DNS_MX:
	case IMSG_MTA_DNS_MX_PREFERENCE:
		dns_imsg(p, imsg);
		return;

	case IMSG_SMTP_CHECK_SENDER:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_string(&m, &tablename);
		m_get_string(&m, &username);
		m_get_mailaddr(&m, &maddr);
		m_end(&m);

		ret = lka_mailaddrmap(tablename, username, &maddr);

		m_create(p, IMSG_SMTP_CHECK_SENDER, 0, 0, -1);
		m_add_id(p, reqid);
		m_add_int(p, ret);
		m_close(p);
		return;

	case IMSG_SMTP_EXPAND_RCPT:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_envelope(&m, &evp);
		m_end(&m);
		lka_session(reqid, &evp);
		return;

	case IMSG_SMTP_LOOKUP_HELO:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_string(&m, &tablename);
		m_get_sockaddr(&m, (struct sockaddr *)&ss);
		m_end(&m);

		ret = lka_addrname(tablename, (struct sockaddr*)&ss,
		    &addrname);

		m_create(p, IMSG_SMTP_LOOKUP_HELO, 0, 0, -1);
		m_add_id(p, reqid);
		m_add_int(p, ret);
		if (ret == LKA_OK)
			m_add_string(p, addrname.name);
		m_close(p);
		return;

	case IMSG_SMTP_TLS_INIT:
	case IMSG_MTA_TLS_INIT:
		req_ca_cert = imsg->data;
		resp_ca_cert.reqid = req_ca_cert->reqid;

		xlowercase(buf, req_ca_cert->name, sizeof(buf));
		log_debug("debug: lka: looking up pki \"%s\"", buf);
		pki = dict_get(env->sc_pki_dict, buf);
		if (pki == NULL)
			if (req_ca_cert->fallback)
				pki = dict_get(env->sc_pki_dict, "*");
		if (pki == NULL) {
			resp_ca_cert.status = CA_FAIL;
			m_compose(p, imsg->hdr.type, 0, 0, -1, &resp_ca_cert,
			    sizeof(resp_ca_cert));
			return;
		}
		resp_ca_cert.status = CA_OK;
		resp_ca_cert.cert_len = pki->pki_cert_len;
		(void)strlcpy(resp_ca_cert.name, pki->pki_name, sizeof resp_ca_cert.name);
		iov[0].iov_base = &resp_ca_cert;
		iov[0].iov_len = sizeof(resp_ca_cert);
		iov[1].iov_base = pki->pki_cert;
		iov[1].iov_len = pki->pki_cert_len;
		m_composev(p, imsg->hdr.type, 0, 0, -1, iov, nitems(iov));
		return;

	case IMSG_SMTP_TLS_VERIFY_CERT:
	case IMSG_MTA_TLS_VERIFY_CERT:
		req_ca_vrfy = xmemdup(imsg->data, sizeof *req_ca_vrfy, "lka:ca_vrfy");
		req_ca_vrfy->cert = xmemdup((char *)imsg->data +
		    sizeof *req_ca_vrfy, req_ca_vrfy->cert_len, "lka:ca_vrfy");
		req_ca_vrfy->chain_cert = xcalloc(req_ca_vrfy->n_chain,
		    sizeof (unsigned char *), "lka:ca_vrfy");
		req_ca_vrfy->chain_cert_len = xcalloc(req_ca_vrfy->n_chain,
		    sizeof (off_t), "lka:ca_vrfy");
		return;

	case IMSG_SMTP_TLS_VERIFY_CHAIN:
	case IMSG_MTA_TLS_VERIFY_CHAIN:
		if (req_ca_vrfy == NULL)
			fatalx("lka:ca_vrfy: chain without a certificate");
		req_ca_vrfy_chain = imsg->data;
		req_ca_vrfy->chain_cert[req_ca_vrfy->chain_offset] = xmemdup((char *)imsg->data +
		    sizeof *req_ca_vrfy_chain, req_ca_vrfy_chain->cert_len, "lka:ca_vrfy");
		req_ca_vrfy->chain_cert_len[req_ca_vrfy->chain_offset] = req_ca_vrfy_chain->cert_len;
		req_ca_vrfy->chain_offset++;
		return;

	case IMSG_SMTP_TLS_VERIFY:
	case IMSG_MTA_TLS_VERIFY:
		if (req_ca_vrfy == NULL)
			fatalx("lka:ca_vrfy: verify without a certificate");
		lka_certificate_verify(imsg->hdr.type, req_ca_vrfy);
		req_ca_vrfy = NULL;
		return;

	case IMSG_SMTP_AUTHENTICATE:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_string(&m, &tablename);
		m_get_string(&m, &username);
		m_get_string(&m, &password);
		m_end(&m);

		if (!tablename[0]) {
			m_create(p_parent, IMSG_LKA_AUTHENTICATE,
			    0, 0, -1);
			m_add_id(p_parent, reqid);
			m_add_string(p_parent, username);
			m_add_string(p_parent, password);
			m_close(p_parent);
			return;
		}

		ret = lka_authenticate(tablename, username, password);

		m_create(p, IMSG_SMTP_AUTHENTICATE, 0, 0, -1);
		m_add_id(p, reqid);
		m_add_int(p, ret);
		m_close(p);
		return;

	case IMSG_MDA_LOOKUP_USERINFO:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_string(&m, &tablename);
		m_get_string(&m, &username);
		m_end(&m);

		ret = lka_userinfo(tablename, username, &userinfo);

		m_create(p, IMSG_MDA_LOOKUP_USERINFO, 0, 0, -1);
		m_add_id(p, reqid);
		m_add_int(p, ret);
		if (ret == LKA_OK)
			m_add_data(p, &userinfo, sizeof(userinfo));
		m_close(p);
		return;

	case IMSG_MTA_LOOKUP_CREDENTIALS:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_string(&m, &tablename);
		m_get_string(&m, &label);
		m_end(&m);

		lka_credentials(tablename, label, buf, sizeof(buf));

		m_create(p, IMSG_MTA_LOOKUP_CREDENTIALS, 0, 0, -1);
		m_add_id(p, reqid);
		m_add_string(p, buf);
		m_close(p);
		return;

	case IMSG_MTA_LOOKUP_SOURCE:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_string(&m, &tablename);
		m_end(&m);

		table = table_find(tablename, NULL);

		m_create(p, IMSG_MTA_LOOKUP_SOURCE, 0, 0, -1);
		m_add_id(p, reqid);

		if (table == NULL) {
			log_warn("warn: source address table %s missing",
			    tablename);
			m_add_int(p, LKA_TEMPFAIL);
		}
		else {
			ret = table_fetch(table, NULL, K_SOURCE, &lk);
			if (ret == -1)
				m_add_int(p, LKA_TEMPFAIL);
			else if (ret == 0)
				m_add_int(p, LKA_PERMFAIL);
			else {
				m_add_int(p, LKA_OK);
				m_add_sockaddr(p,
				    (struct sockaddr *)&lk.source.addr);
			}
		}
		m_close(p);
		return;

	case IMSG_MTA_LOOKUP_HELO:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_string(&m, &tablename);
		m_get_sockaddr(&m, (struct sockaddr *)&ss);
		m_end(&m);

		ret = lka_addrname(tablename, (struct sockaddr*)&ss,
		    &addrname);

		m_create(p, IMSG_MTA_LOOKUP_HELO, 0, 0, -1);
		m_add_id(p, reqid);
		m_add_int(p, ret);
		if (ret == LKA_OK)
			m_add_string(p, addrname.name);
		m_close(p);
		return;

	case IMSG_CONF_START:
		return;

	case IMSG_CONF_END:
		if (tracing & TRACE_TABLES)
			table_dump_all();

		/* fork & exec tables that need it */
		table_open_all();

		/* revoke proc & exec */
		if (pledge("stdio rpath inet dns getpw recvfd",
			NULL) == -1)
			err(1, "pledge");

		/* Start fulfilling requests */
		mproc_enable(p_pony);
		return;

	case IMSG_LKA_OPEN_FORWARD:
		lka_session_forward_reply(imsg->data, imsg->fd);
		return;

	case IMSG_LKA_AUTHENTICATE:
		imsg->hdr.type = IMSG_SMTP_AUTHENTICATE;
		m_forward(p_pony, imsg);
		return;

	case IMSG_CTL_VERBOSE:
		m_msg(&m, imsg);
		m_get_int(&m, &v);
		m_end(&m);
		log_trace_verbose(v);
		return;

	case IMSG_CTL_PROFILE:
		m_msg(&m, imsg);
		m_get_int(&m, &v);
		m_end(&m);
		profiling = v;
		return;

	case IMSG_CTL_UPDATE_TABLE:
		ret = 0;
		table = table_find(imsg->data, NULL);
		if (table == NULL) {
			log_warnx("warn: Lookup table not found: "
			    "\"%s\"", (char *)imsg->data);
		} else 
			ret = table_update(table);

		m_compose(p_control,
		    (ret == 1) ? IMSG_CTL_OK : IMSG_CTL_FAIL,
		    imsg->hdr.peerid, 0, -1, NULL, 0);
		return;
	}

	errx(1, "lka_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
lka_sig_handler(int sig, short event, void *p)
{
	int status;
	pid_t pid;

	switch (sig) {
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
	log_debug("debug: lookup agent exiting");
	_exit(0);
}

int
lka(void)
{
	struct passwd	*pw;
	struct event	 ev_sigchld;

	purge_config(PURGE_LISTENERS);

	if ((pw = getpwnam(SMTPD_USER)) == NULL)
		fatalx("unknown user " SMTPD_USER);

	config_process(PROC_LKA);

	if (initgroups(pw->pw_name, pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("lka: cannot drop privileges");

	imsg_callback = lka_imsg;
	event_init();

	signal_set(&ev_sigchld, SIGCHLD, lka_sig_handler, NULL);
	signal_add(&ev_sigchld, NULL);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peer(PROC_PARENT);
	config_peer(PROC_QUEUE);
	config_peer(PROC_CONTROL);
	config_peer(PROC_PONY);

	/* Ignore them until we get our config */
	mproc_disable(p_pony);

	/* proc & exec will be revoked before serving requests */
	if (pledge("stdio rpath inet dns getpw recvfd proc exec", NULL) == -1)
		err(1, "pledge");

	event_dispatch();
	fatalx("exited event loop");

	return (0);
}

static int
lka_authenticate(const char *tablename, const char *user, const char *password)
{
	struct table		*table;
	union lookup		 lk;

	log_debug("debug: lka: authenticating for %s:%s", tablename, user);
	table = table_find(tablename, NULL);
	if (table == NULL) {
		log_warnx("warn: could not find table %s needed for authentication",
		    tablename);
		return (LKA_TEMPFAIL);
	}

	switch (table_lookup(table, NULL, user, K_CREDENTIALS, &lk)) {
	case -1:
		log_warnx("warn: user credentials lookup fail for %s:%s",
		    tablename, user);
		return (LKA_TEMPFAIL);
	case 0:
		return (LKA_PERMFAIL);
	default:
		if (crypt_checkpass(password, lk.creds.password) == 0)
			return (LKA_OK);
		return (LKA_PERMFAIL);
	}
}

static int
lka_credentials(const char *tablename, const char *label, char *dst, size_t sz)
{
	struct table		*table;
	union lookup		 lk;
	char			*buf;
	int			 buflen, r;

	table = table_find(tablename, NULL);
	if (table == NULL) {
		log_warnx("warn: credentials table %s missing", tablename);
		return (LKA_TEMPFAIL);
	}

	dst[0] = '\0';

	switch(table_lookup(table, NULL, label, K_CREDENTIALS, &lk)) {
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
		    lk.creds.username, '\0', lk.creds.password)) == -1) {
			log_warn("warn");
			return (LKA_TEMPFAIL);
		}

		r = base64_encode((unsigned char *)buf, buflen, dst, sz);
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
	struct table	*table;
	union lookup	 lk;

	log_debug("debug: lka: userinfo %s:%s", tablename, username);
	table = table_find(tablename, NULL);
	if (table == NULL) {
		log_warnx("warn: cannot find user table %s", tablename);
		return (LKA_TEMPFAIL);
	}

	switch (table_lookup(table, NULL, username, K_USERINFO, &lk)) {
	case -1:
		log_warnx("warn: failure during userinfo lookup %s:%s",
		    tablename, username);
		return (LKA_TEMPFAIL);
	case 0:
		return (LKA_PERMFAIL);
	default:
		*res = lk.userinfo;
		return (LKA_OK);
	}
}

static int
lka_addrname(const char *tablename, const struct sockaddr *sa,
    struct addrname *res)
{
	struct table	*table;
	union lookup	 lk;
	const char	*source;

	source = sa_to_text(sa);

	log_debug("debug: lka: helo %s:%s", tablename, source);
	table = table_find(tablename, NULL);
	if (table == NULL) {
		log_warnx("warn: cannot find helo table %s", tablename);
		return (LKA_TEMPFAIL);
	}

	switch (table_lookup(table, NULL, source, K_ADDRNAME, &lk)) {
	case -1:
		log_warnx("warn: failure during helo lookup %s:%s",
		    tablename, source);
		return (LKA_TEMPFAIL);
	case 0:
		return (LKA_PERMFAIL);
	default:
		*res = lk.addrname;
		return (LKA_OK);
	}
}

static int
lka_mailaddrmap(const char *tablename, const char *username, const struct mailaddr *maddr)
{
	struct table	       *table;
	struct maddrnode       *mn;
	union lookup		lk;
	int			found;

	log_debug("debug: lka: mailaddrmap %s:%s", tablename, username);
	table = table_find(tablename, NULL);
	if (table == NULL) {
		log_warnx("warn: cannot find mailaddrmap table %s", tablename);
		return (LKA_TEMPFAIL);
	}

	switch (table_lookup(table, NULL, username, K_MAILADDRMAP, &lk)) {
	case -1:
		log_warnx("warn: failure during mailaddrmap lookup %s:%s",
		    tablename, username);
		return (LKA_TEMPFAIL);
	case 0:
		return (LKA_PERMFAIL);
	default:
		found = 0;
		TAILQ_FOREACH(mn, &lk.maddrmap->queue, entries) {
			if (!mailaddr_match(maddr, &mn->mailaddr))
				continue;
			found = 1;
			break;
		}
		maddrmap_free(lk.maddrmap);
		if (found)
			return (LKA_OK);
		return (LKA_PERMFAIL);
	}
	return (LKA_OK);
}

static int
lka_X509_verify(struct ca_vrfy_req_msg *vrfy,
    const char *CAfile, const char *CRLfile)
{
	X509			*x509;
	X509			*x509_tmp;
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
			if (d2i_X509(&x509_tmp, &d2i, vrfy->chain_cert_len[i]) == NULL)
				goto end;
			sk_X509_insert(x509_chain, x509_tmp, i);
			x509_tmp = NULL;
		}
	}
	if (!ca_X509_verify(x509, x509_chain, CAfile, NULL, &errstr))
		log_debug("debug: lka: X509 verify: %s", errstr);
	else
		ret = 1;

end:
	X509_free(x509);
	X509_free(x509_tmp);
	if (x509_chain)
		sk_X509_pop_free(x509_chain, X509_free);

	return ret;
}

static void
lka_certificate_verify(enum imsg_type type, struct ca_vrfy_req_msg *req)
{
	lka_certificate_verify_resume(type, req);
}

static void
lka_certificate_verify_resume(enum imsg_type type, struct ca_vrfy_req_msg *req)
{
	struct ca_vrfy_resp_msg		resp;
	struct ca		       *sca;
	const char		       *cafile;
	size_t				i;

	resp.reqid = req->reqid;
	sca = dict_get(env->sc_ca_dict, req->name);
	if (sca == NULL)
		if (req->fallback)
			sca = dict_get(env->sc_ca_dict, "*");
	cafile = sca ? sca->ca_cert_file : CA_FILE;

	if (sca == NULL && !req->fallback)
		resp.status = CA_FAIL;
	else if (!lka_X509_verify(req, cafile, NULL))
		resp.status = CA_FAIL;
	else
		resp.status = CA_OK;

	m_compose(p_pony, type, 0, 0, -1, &resp,
	    sizeof resp);

	for (i = 0; i < req->n_chain; ++i)
		free(req->chain_cert[i]);
	free(req->chain_cert);
	free(req->chain_cert_len);
	free(req->cert);
	free(req);
}
