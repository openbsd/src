/*	$OpenBSD: filemode.c,v 1.1 2022/04/21 09:53:07 claudio Exp $ */
/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/types.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <imsg.h>

#include <openssl/asn1.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "extern.h"

static X509_STORE_CTX	*ctx;
static struct auth_tree	 auths = RB_INITIALIZER(&auths);
static struct crl_tree	 crlt = RB_INITIALIZER(&crlt);

struct tal		*talobj[TALSZ_MAX];

/*
 * Validate a certificate, if invalid free the resouces and return NULL.
 */
static struct cert *
proc_parser_cert_validate(char *file, struct cert *cert)
{
	struct auth	*a;
	struct crl	*crl;

	a = valid_ski_aki(file, &auths, cert->ski, cert->aki);
	crl = crl_get(&crlt, a);

	if (!valid_x509(file, ctx, cert->x509, a, crl, 0)) {
		cert_free(cert);
		return NULL;
	}

	cert->talid = a->cert->talid;

	/* Validate the cert */
	if (!valid_cert(file, a, cert)) {
		cert_free(cert);
		return NULL;
	}

	/*
	 * Add validated CA certs to the RPKI auth tree.
	 */
	if (cert->purpose == CERT_PURPOSE_CA)
		auth_insert(&auths, cert, a);

	return cert;
}

/*
 * Root certificates come from TALs (has a pkey and is self-signed).
 * Parse the certificate, ensure that its public key matches the
 * known public key from the TAL, and then validate the RPKI
 * content.
 *
 * This returns a certificate (which must not be freed) or NULL on
 * parse failure.
 */
static struct cert *
proc_parser_root_cert(char *file, const unsigned char *der, size_t len,
    unsigned char *pkey, size_t pkeysz, int talid)
{
	struct cert		*cert;

	/* Extract certificate data. */

	cert = cert_parse_pre(file, der, len);
	if (cert == NULL)
		return NULL;
	cert = ta_parse(file, cert, pkey, pkeysz);
	if (cert == NULL)
		return NULL;

	if (!valid_ta(file, &auths, cert)) {
		warnx("%s: certificate not a valid ta", file);
		cert_free(cert);
		return NULL;
	}

	cert->talid = talid;

	/*
	 * Add valid roots to the RPKI auth tree.
	 */
	auth_insert(&auths, cert, NULL);

	return cert;
}

/*
 * Use the X509 CRL Distribution Points to locate the CRL needed for
 * verification.
 */
static void
parse_load_crl(char *uri)
{
	struct crl *crl;
	char *f;
	size_t flen;

	if (uri == NULL)
		return;
	if (strncmp(uri, "rsync://", strlen("rsync://")) != 0) {
		warnx("bad CRL distribution point URI %s", uri);
		return;
	}
	uri += strlen("rsync://");

	f = load_file(uri, &flen);
	if (f == NULL) {
		warn("parse file %s", uri);
		return;
	}

	crl = crl_parse(uri, f, flen);
	if (crl != NULL && !crl_insert(&crlt, crl))
		crl_free(crl);

	free(f);
}

/*
 * Parse the cert pointed at by the AIA URI while doing that also load
 * the CRL of this cert. While the CRL is validated the returned cert
 * is not. The caller needs to make sure it is validated once all
 * necessary certs were loaded. Returns NULL on failure.
 */
static struct cert *
parse_load_cert(char *uri)
{
	struct cert *cert = NULL;
	char *f;
	size_t flen;

	if (uri == NULL)
		return NULL;

	if (strncmp(uri, "rsync://", strlen("rsync://")) != 0) {
		warnx("bad authority information access URI %s", uri);
		return NULL;
	}
	uri += strlen("rsync://");

	f = load_file(uri, &flen);
	if (f == NULL) {
		warn("parse file %s", uri);
		goto done;
	}

	cert = cert_parse_pre(uri, f, flen);
	free(f);

	if (cert == NULL)
		goto done;
	if (cert->purpose != CERT_PURPOSE_CA) {
		warnx("AIA reference to bgpsec cert %s", uri);
		goto done;
	}
	/* try to load the CRL of this cert */
	parse_load_crl(cert->crl);

	return cert;

 done:
	cert_free(cert);
	return NULL;
}

/*
 * Build the certificate chain by using the Authority Information Access.
 * This requires that the TA are already validated and added to the auths
 * tree. Once the TA is located in the chain the chain is validated in
 * reverse order.
 */
static void
parse_load_certchain(char *uri)
{
	struct cert *stack[MAX_CERT_DEPTH];
	char *filestack[MAX_CERT_DEPTH];
	struct cert *cert;
	int i, failed;

	for (i = 0; i < MAX_CERT_DEPTH; i++) {
		cert = parse_load_cert(uri);
		if (cert == NULL) {
			warnx("failed to build authority chain");
			return;
		}
		if (auth_find(&auths, cert->ski) != NULL) {
			assert(i == 0);
			cert_free(cert);
			return;	/* cert already added */
		}
		stack[i] = cert;
		filestack[i] = uri;
		if (auth_find(&auths, cert->aki) != NULL)
			break;	/* found chain to TA */
		uri = cert->aia;
	}

	if (i >= MAX_CERT_DEPTH) {
		warnx("authority chain exceeds max depth of %d",
		    MAX_CERT_DEPTH);
		for (i = 0; i < MAX_CERT_DEPTH; i++)
			cert_free(stack[i]);
		return;
	}

	/* TA found play back the stack and add all certs */
	for (failed = 0; i >= 0; i--) {
		cert = stack[i];
		uri = filestack[i];

		if (failed)
			cert_free(cert);
		else if (proc_parser_cert_validate(uri, cert) == NULL)
			failed = 1;
	}
}

static void
parse_load_ta(struct tal *tal)
{
	const char *file;
	char *nfile, *f;
	size_t flen;

	/* does not matter which URI, all end with same filename */
	file = strrchr(tal->uri[0], '/');
	assert(file);

	if (asprintf(&nfile, "ta/%s%s", tal->descr, file) == -1)
		err(1, NULL);

	f = load_file(nfile, &flen);
	if (f == NULL) {
		warn("parse file %s", nfile);
		free(nfile);
		return;
	}

	/* if TA is valid it was added as a root which is all we need */
	proc_parser_root_cert(nfile, f, flen, tal->pkey, tal->pkeysz, tal->id);
	free(nfile);
	free(f);
}

static struct tal *
find_tal(struct cert *cert)
{
	EVP_PKEY	*pk, *opk;
	struct tal	*tal;
	int		 i;

	if ((opk = X509_get0_pubkey(cert->x509)) == NULL)
		return NULL;

	for (i = 0; i < TALSZ_MAX; i++) {
		const unsigned char *pkey;

		if (talobj[i] == NULL)
			break;
		tal = talobj[i];
		pkey = tal->pkey;
		pk = d2i_PUBKEY(NULL, &pkey, tal->pkeysz);
		if (pk == NULL)
			continue;
		if (EVP_PKEY_cmp(pk, opk) == 1) {
			EVP_PKEY_free(pk);
			return tal;
		}
		EVP_PKEY_free(pk);
	}
	return NULL;
}

/*
 * Parse file passed with -f option.
 */
static void
proc_parser_file(char *file, unsigned char *buf, size_t len)
{
	static int num;
	X509 *x509 = NULL;
	struct cert *cert = NULL;
	struct crl *crl = NULL;
	struct mft *mft = NULL;
	struct roa *roa = NULL;
	struct gbr *gbr = NULL;
	struct tal *tal = NULL;
	char *aia = NULL, *aki = NULL;
	enum rtype type;
	int is_ta = 0;

	if (num++ > 0) {
		if (outformats & FORMAT_JSON)
			printf("\n");
		else
			printf("--\n");
	}

	if (strncmp(file, "rsync://", strlen("rsync://")) == 0) {
		file += strlen("rsync://");
		buf = load_file(file, &len);
		if (buf == NULL) {
			warn("parse file %s", file);
			return;
		}
	}

	if (outformats & FORMAT_JSON)
		printf("{\n\t\"file\": \"%s\",\n", file);
	else
		printf("File: %s\n", file);

	type = rtype_from_file_extension(file);

	switch (type) {
	case RTYPE_CER:
		cert = cert_parse_pre(file, buf, len);
		if (cert == NULL)
			break;
		is_ta = X509_get_extension_flags(cert->x509) & EXFLAG_SS;
		if (!is_ta)
			cert = cert_parse(file, cert);
		if (cert == NULL)
			break;
		cert_print(cert);
		aia = cert->aia;
		aki = cert->aki;
		x509 = cert->x509;
		if (X509_up_ref(x509) == 0)
			errx(1, "%s: X509_up_ref failed", __func__);
		break;
	case RTYPE_CRL:
		crl = crl_parse(file, buf, len);
		if (crl == NULL)
			break;
		crl_print(crl);
		break;
	case RTYPE_MFT:
		mft = mft_parse(&x509, file, buf, len);
		if (mft == NULL)
			break;
		mft_print(x509, mft);
		aia = mft->aia;
		aki = mft->aki;
		break;
	case RTYPE_ROA:
		roa = roa_parse(&x509, file, buf, len);
		if (roa == NULL)
			break;
		roa_print(x509, roa);
		aia = roa->aia;
		aki = roa->aki;
		break;
	case RTYPE_GBR:
		gbr = gbr_parse(&x509, file, buf, len);
		if (gbr == NULL)
			break;
		gbr_print(x509, gbr);
		aia = gbr->aia;
		aki = gbr->aki;
		break;
	case RTYPE_TAL:
		tal = tal_parse(file, buf, len);
		if (tal == NULL)
			break;
		tal_print(tal);
		break;
	default:
		printf("%s: unsupported file type\n", file);
		break;
	}

	if (outformats & FORMAT_JSON)
		printf("\t\"validation\": \"");
	else
		printf("Validation: ");

	if (aia != NULL) {
		struct auth *a;
		struct crl *c;
		char *crl_uri;

		x509_get_crl(x509, file, &crl_uri);
		parse_load_crl(crl_uri);
		free(crl_uri);
		if (auth_find(&auths, aki) == NULL)
			parse_load_certchain(aia);
		a = auth_find(&auths, aki);
		c = crl_get(&crlt, a);

		if (valid_x509(file, ctx, x509, a, c, 0))
			printf("OK");
		else
			printf("Failed");
	} else if (is_ta) {
		if ((tal = find_tal(cert)) != NULL) {
			cert = ta_parse(file, cert, tal->pkey, tal->pkeysz);
			if (cert != NULL)
				printf("OK");
			else
				printf("Failed");
			if (outformats & FORMAT_JSON)
				printf("\",\n\t\"tal\": \"%s", tal->descr);
			else
				printf("\nTAL: %s", tal->descr);
			tal = NULL;
		} else {
			cert_free(cert);
			cert = NULL;
			printf("Failed");
		}
	}

	if (outformats & FORMAT_JSON)
		printf("\"\n}");
	else
		printf("\n");

	X509_free(x509);
	cert_free(cert);
	crl_free(crl);
	mft_free(mft);
	roa_free(roa);
	gbr_free(gbr);
	tal_free(tal);
}

/*
 * Process a file request, in general don't send anything back.
 */
static void
parse_file(struct entityq *q, struct msgbuf *msgq)
{
	struct entity	*entp;
	struct ibuf	*b;
	struct tal	*tal;

	while ((entp = TAILQ_FIRST(q)) != NULL) {
		TAILQ_REMOVE(q, entp, entries);

		switch (entp->type) {
		case RTYPE_FILE:
			proc_parser_file(entp->file, entp->data, entp->datasz);
			break;
		case RTYPE_TAL:
			if ((tal = tal_parse(entp->file, entp->data,
			    entp->datasz)) == NULL)
				errx(1, "%s: could not parse tal file",
				    entp->file);
			tal->id = entp->talid;
			talobj[tal->id] = tal;
			parse_load_ta(tal);
			break;
		default:
			errx(1, "unhandled entity type %d", entp->type);
		}

		b = io_new_buffer();
		io_simple_buffer(b, &entp->type, sizeof(entp->type));
		io_str_buffer(b, entp->file);
		io_close_buffer(msgq, b);
		entity_free(entp);
	}
}

/*
 * Process responsible for parsing and validating content.
 * All this process does is wait to be told about a file to parse, then
 * it parses it and makes sure that the data being returned is fully
 * validated and verified.
 * The process will exit cleanly only when fd is closed.
 */
void
proc_filemode(int fd)
{
	struct entityq	 q;
	struct msgbuf	 msgq;
	struct pollfd	 pfd;
	struct entity	*entp;
	struct ibuf	*b, *inbuf = NULL;

	/* Only allow access to the cache directory. */
	if (unveil(".", "r") == -1)
		err(1, "unveil cachedir");
	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	ERR_load_crypto_strings();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	x509_init_oid();

	if ((ctx = X509_STORE_CTX_new()) == NULL)
		cryptoerrx("X509_STORE_CTX_new");
	TAILQ_INIT(&q);

	msgbuf_init(&msgq);
	msgq.fd = fd;

	pfd.fd = fd;

	for (;;) {
		pfd.events = POLLIN;
		if (msgq.queued)
			pfd.events |= POLLOUT;

		if (poll(&pfd, 1, INFTIM) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "poll");
		}
		if ((pfd.revents & (POLLERR|POLLNVAL)))
			errx(1, "poll: bad descriptor");

		/* If the parent closes, return immediately. */

		if ((pfd.revents & POLLHUP))
			break;

		if ((pfd.revents & POLLIN)) {
			b = io_buf_read(fd, &inbuf);
			if (b != NULL) {
				entp = calloc(1, sizeof(struct entity));
				if (entp == NULL)
					err(1, NULL);
				entity_read_req(b, entp);
				TAILQ_INSERT_TAIL(&q, entp, entries);
				ibuf_free(b);
			}
		}

		if (pfd.revents & POLLOUT) {
			switch (msgbuf_write(&msgq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}

		parse_file(&q, &msgq);
	}

	msgbuf_clear(&msgq);
	while ((entp = TAILQ_FIRST(&q)) != NULL) {
		TAILQ_REMOVE(&q, entp, entries);
		entity_free(entp);
	}

	/* XXX free auths and crl tree */
	X509_STORE_CTX_free(ctx);

	exit(0);
}
