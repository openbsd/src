/*	$OpenBSD: cert.c,v 1.2 2018/12/11 07:25:57 eric Exp $	*/

/*
 * Copyright (c) 2018 Eric Faurot <eric@openbsd.org>
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
#include <sys/tree.h>
#include <sys/queue.h>
#include <netinet/in.h>

#include <imsg.h>
#include <limits.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "smtpd.h"
#include "ssl.h"

#define p_cert p_lka

struct request {
	SPLAY_ENTRY(request)	 entry;
	uint32_t		 id;
	void			(*cb_get_certificate)(void *, int, const char *,
				    const void *, size_t);
	void			(*cb_verify)(void *, int);
	void			*arg;
};

#define MAX_CERTS	16
#define MAX_CERT_LEN	(MAX_IMSGSIZE - (IMSG_HEADER_SIZE + sizeof(size_t)))

struct session {
	SPLAY_ENTRY(session)	 entry;
	uint32_t		 id;
	struct mproc		*proc;
	char			*cert[MAX_CERTS];
	size_t			 cert_len[MAX_CERTS];
	int			 cert_count;
};

SPLAY_HEAD(cert_reqtree, request);
SPLAY_HEAD(cert_sestree, session);

static int request_cmp(struct request *, struct request *);
static int session_cmp(struct session *, struct session *);
SPLAY_PROTOTYPE(cert_reqtree, request, entry, request_cmp);
SPLAY_PROTOTYPE(cert_sestree, session, entry, session_cmp);

static void cert_do_verify(struct session *, const char *, int);
static int cert_X509_verify(struct session *, const char *, const char *);

static struct cert_reqtree reqs = SPLAY_INITIALIZER(&reqs);
static struct cert_sestree sess = SPLAY_INITIALIZER(&sess);

int
cert_init(const char *name, int fallback, void (*cb)(void *, int,
    const char *, const void *, size_t), void *arg)
{
	struct request *req;

	req = calloc(1, sizeof(*req));
	if (req ==  NULL) {
		cb(arg, CA_FAIL, NULL, NULL, 0);
		return 0;
	}
	while (req->id == 0 || SPLAY_FIND(cert_reqtree, &reqs, req))
		req->id = arc4random();
	req->cb_get_certificate = cb;
	req->arg = arg;
	SPLAY_INSERT(cert_reqtree, &reqs, req);

	m_create(p_cert, IMSG_CERT_INIT, req->id, 0, -1);
	m_add_string(p_cert, name);
	m_add_int(p_cert, fallback);
	m_close(p_cert);

	return 1;
}

int
cert_verify(const void *ssl, const char *name, int fallback,
    void (*cb)(void *, int), void *arg)
{
	struct request	       *req;
	X509		       *x;
	STACK_OF(X509)	       *xchain;
	unsigned char	       *cert_der[MAX_CERTS];
	int			cert_len[MAX_CERTS];
	int			i, cert_count, ret;

	x = SSL_get_peer_certificate(ssl);
	if (x == NULL) {
		cb(arg, CERT_NOCERT);
		return 0;
	}

	ret = 0;
	memset(cert_der, 0, sizeof(cert_der));

	req = calloc(1, sizeof(*req));
	if (req ==  NULL)
		goto end;
	while (req->id == 0 || SPLAY_FIND(cert_reqtree, &reqs, req))
		req->id = arc4random();
	req->cb_verify = cb;
	req->arg = arg;
	SPLAY_INSERT(cert_reqtree, &reqs, req);

	cert_count = 1;
	if ((xchain = SSL_get_peer_cert_chain(ssl))) {
		cert_count += sk_X509_num(xchain);
		if (cert_count > MAX_CERTS) {
			log_warnx("warn: certificate chain too long");
			goto end;
		}
	}

	for (i = 0; i < cert_count; ++i) {
		if (i != 0) {
			if ((x = sk_X509_value(xchain, i - 1)) == NULL) {
				log_warnx("warn: failed to retrieve certificate");
				goto end;
			}
		}

		cert_len[i] = i2d_X509(x, &cert_der[i]);
		if (i == 0)
			X509_free(x);

		if (cert_len[i] < 0) {
			log_warnx("warn: failed to encode certificate");
			goto end;
		}

		log_debug("debug: certificate %i: len=%d", i, cert_len[i]);
		if (cert_len[i] > (int)MAX_CERT_LEN) {
			log_warnx("warn: certificate too long");
			goto end;
		}
	}

	/* Send the cert chain, one cert at a time */
	for (i = 0; i < cert_count; ++i) {
		m_create(p_cert, IMSG_CERT_CERTIFICATE, req->id, 0, -1);
		m_add_data(p_cert, cert_der[i], cert_len[i]);
		m_close(p_cert);
	}

	/* Tell lookup process that it can start verifying, we're done */
	m_create(p_cert, IMSG_CERT_VERIFY, req->id, 0, -1);
	m_add_string(p_cert, name);
	m_add_int(p_cert, fallback);
	m_close(p_cert);

	ret = 1;

    end:
	for (i = 0; i < MAX_CERTS; ++i)
		free(cert_der[i]);

	if (ret == 0) {
		if (req)
			SPLAY_REMOVE(cert_reqtree, &reqs, req);
		free(req);
		cb(arg, CERT_ERROR);
	}

	return ret;
}


void
cert_dispatch_request(struct mproc *proc, struct imsg *imsg)
{
	struct pki *pki;
	struct session key, *s;
	const char *name;
	const void *data;
	size_t datalen;
	struct msg m;
	uint32_t reqid;
	char buf[LINE_MAX];
	int fallback;

	reqid = imsg->hdr.peerid;
	m_msg(&m, imsg);

	switch (imsg->hdr.type) {

	case IMSG_CERT_INIT:
		m_get_string(&m, &name);
		m_get_int(&m, &fallback);
		m_end(&m);

		xlowercase(buf, name, sizeof(buf));
		log_debug("debug: looking up pki \"%s\"", buf);
		pki = dict_get(env->sc_pki_dict, buf);
		if (pki == NULL && fallback)
			pki = dict_get(env->sc_pki_dict, "*");

		m_create(proc, IMSG_CERT_INIT, reqid, 0, -1);
		if (pki) {
			m_add_int(proc, CA_OK);
			m_add_string(proc, pki->pki_name);
			m_add_data(proc, pki->pki_cert, pki->pki_cert_len);
		} else {
			m_add_int(proc, CA_FAIL);
			m_add_string(proc, NULL);
			m_add_data(proc, NULL, 0);
		}
		m_close(proc);
		return;

	case IMSG_CERT_CERTIFICATE:
		m_get_data(&m, &data, &datalen);
		m_end(&m);

		key.id = reqid;
		key.proc = proc;
		s = SPLAY_FIND(cert_sestree, &sess, &key);
		if (s == NULL) {
			s = calloc(1, sizeof(*s));
			s->proc = proc;
			s->id = reqid;
			SPLAY_INSERT(cert_sestree, &sess, s);
		}

		if (s->cert_count == MAX_CERTS)
			fatalx("%s: certificate chain too long", __func__);

		s->cert[s->cert_count] = xmemdup(data, datalen);
		s->cert_len[s->cert_count] = datalen;
		s->cert_count++;
		return;

	case IMSG_CERT_VERIFY:
		m_get_string(&m, &name);
		m_get_int(&m, &fallback);
		m_end(&m);

		key.id = reqid;
		key.proc = proc;
		s = SPLAY_FIND(cert_sestree, &sess, &key);
		if (s == NULL)
			fatalx("%s: no certificate", __func__);

		SPLAY_REMOVE(cert_sestree, &sess, s);
		cert_do_verify(s, name, fallback);
		return;

	default:
		fatalx("%s: %s", __func__, imsg_to_str(imsg->hdr.type));
	}
}

void
cert_dispatch_result(struct mproc *proc, struct imsg *imsg)
{
	struct request key, *req;
	struct msg m;
	const void *cert;
	const char *name;
	size_t cert_len;
	int res;

	key.id = imsg->hdr.peerid;
	req = SPLAY_FIND(cert_reqtree, &reqs, &key);
	if (req == NULL)
		fatalx("%s: unknown request %08x", __func__, imsg->hdr.peerid);

	m_msg(&m, imsg);

	switch (imsg->hdr.type) {

	case IMSG_CERT_INIT:
		m_get_int(&m, &res);
		m_get_string(&m, &name);
		m_get_data(&m, &cert, &cert_len);
		m_end(&m);
		SPLAY_REMOVE(cert_reqtree, &reqs, req);
		req->cb_get_certificate(req->arg, res, name, cert, cert_len);
		free(req);
		break;

	case IMSG_CERT_VERIFY:
		m_get_int(&m, &res);
		m_end(&m);
		SPLAY_REMOVE(cert_reqtree, &reqs, req);
		req->cb_verify(req->arg, res);
		free(req);
		break;
	}
}

static void
cert_do_verify(struct session *s, const char *name, int fallback)
{
	struct ca *ca;
	const char *cafile;
	int i, res;

	ca = dict_get(env->sc_ca_dict, name);
	if (ca == NULL)
		if (fallback)
			ca = dict_get(env->sc_ca_dict, "*");
	cafile = ca ? ca->ca_cert_file : CA_FILE;

	if (ca == NULL && !fallback)
		res = CERT_NOCA;
	else if (!cert_X509_verify(s, cafile, NULL))
		res = CERT_INVALID;
	else
		res = CERT_OK;

	for (i = 0; i < s->cert_count; ++i)
		free(s->cert[i]);

	m_create(s->proc, IMSG_CERT_VERIFY, s->id, 0, -1);
	m_add_int(s->proc, res);
	m_close(s->proc);

	free(s);
}

static int
cert_X509_verify(struct session *s, const char *CAfile,
    const char *CRLfile)
{
	X509			*x509;
	X509			*x509_tmp;
	STACK_OF(X509)		*x509_chain;
	const unsigned char    	*d2i;
	int			i, ret = 0;
	const char		*errstr;

	x509 = NULL;
	x509_tmp = NULL;
	x509_chain = NULL;

	d2i = s->cert[0];
	if (d2i_X509(&x509, &d2i, s->cert_len[0]) == NULL) {
		x509 = NULL;
		goto end;
	}

	if (s->cert_count > 1) {
		x509_chain = sk_X509_new_null();
		for (i = 1; i < s->cert_count; ++i) {
			d2i = s->cert[i];
			if (d2i_X509(&x509_tmp, &d2i, s->cert_len[i]) == NULL)
				goto end;
			sk_X509_insert(x509_chain, x509_tmp, i);
			x509_tmp = NULL;
		}
	}
	if (!ca_X509_verify(x509, x509_chain, CAfile, NULL, &errstr))
		log_debug("debug: X509 verify: %s", errstr);
	else
		ret = 1;

end:
	X509_free(x509);
	X509_free(x509_tmp);
	if (x509_chain)
		sk_X509_pop_free(x509_chain, X509_free);

	return ret;
}

static int
request_cmp(struct request *a, struct request *b)
{
	if (a->id < b->id)
		return -1;
	if (a->id > b->id)
		return 1;
	return 0;
}

SPLAY_GENERATE(cert_reqtree, request, entry, request_cmp);

static int
session_cmp(struct session *a, struct session *b)
{
	if (a->id < b->id)
		return -1;
	if (a->id > b->id)
		return 1;
	if (a->proc < b->proc)
		return -1;
	if (a->proc > b->proc)
		return 1;
	return 0;
}

SPLAY_GENERATE(cert_sestree, session, entry, session_cmp);
