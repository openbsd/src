/*	$OpenBSD: parser.c,v 1.28 2021/11/04 18:26:48 claudio Exp $ */
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

#include "extern.h"

static void		 build_chain(const struct auth *, STACK_OF(X509) **);
static struct crl	*get_crl(const struct auth *);
static void		 build_crls(const struct crl *, STACK_OF(X509_CRL) **);

static X509_STORE_CTX	*ctx;
static struct auth_tree  auths = RB_INITIALIZER(&auths);
static struct crl_tree	 crlt = RB_INITIALIZER(&crlt);

/*
 * Parse and validate a ROA.
 * This is standard stuff.
 * Returns the roa on success, NULL on failure.
 */
static struct roa *
proc_parser_roa(struct entity *entp, const unsigned char *der, size_t len)
{
	struct roa		*roa;
	X509			*x509;
	int			 c;
	struct auth		*a;
	STACK_OF(X509)		*chain;
	STACK_OF(X509_CRL)	*crls;
	struct crl		*crl;

	if ((roa = roa_parse(&x509, entp->file, der, len)) == NULL)
		return NULL;

	a = valid_ski_aki(entp->file, &auths, roa->ski, roa->aki);
	build_chain(a, &chain);
	crl = get_crl(a);
	build_crls(crl, &crls);

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, NULL, x509, NULL))
		cryptoerrx("X509_STORE_CTX_init");
	X509_STORE_CTX_set_flags(ctx,
	    X509_V_FLAG_IGNORE_CRITICAL | X509_V_FLAG_CRL_CHECK);
	X509_STORE_CTX_set_depth(ctx, MAX_CERT_DEPTH);
	X509_STORE_CTX_set0_trusted_stack(ctx, chain);
	X509_STORE_CTX_set0_crls(ctx, crls);

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		X509_STORE_CTX_cleanup(ctx);
		if (verbose > 0 || c != X509_V_ERR_UNABLE_TO_GET_CRL)
			warnx("%s: %s", entp->file,
			    X509_verify_cert_error_string(c));
		X509_free(x509);
		roa_free(roa);
		sk_X509_free(chain);
		sk_X509_CRL_free(crls);
		return NULL;
	}
	X509_STORE_CTX_cleanup(ctx);

	/*
	 * Check CRL to figure out the soonest transitive expiry moment
	 */
	if (crl != NULL && roa->expires > crl->expires)
		roa->expires = crl->expires;

	/*
	 * Scan the cert tree to figure out the soonest transitive
	 * expiry moment
	 */
	for (; a != NULL; a = a->parent) {
		if (roa->expires > a->cert->expires)
			roa->expires = a->cert->expires;
	}

	/*
	 * If the ROA isn't valid, we accept it anyway and depend upon
	 * the code around roa_read() to check the "valid" field itself.
	 */

	if (valid_roa(entp->file, &auths, roa))
		roa->valid = 1;

	sk_X509_free(chain);
	sk_X509_CRL_free(crls);
	X509_free(x509);

	return roa;
}

/*
 * Parse and validate a manifest file.
 * Here we *don't* validate against the list of CRLs, because the
 * certificate used to sign the manifest may specify a CRL that the root
 * certificate didn't, and we haven't scanned for it yet.
 * This chicken-and-egg isn't important, however, because we'll catch
 * the revocation list by the time we scan for any contained resources
 * (ROA, CER) and will see it then.
 * Return the mft on success or NULL on failure.
 */
static struct mft *
proc_parser_mft(struct entity *entp, const unsigned char *der, size_t len)
{
	struct mft		*mft;
	X509			*x509;
	int			 c;
	struct auth		*a;
	STACK_OF(X509)		*chain;

	if ((mft = mft_parse(&x509, entp->file, der, len)) == NULL)
		return NULL;

	a = valid_ski_aki(entp->file, &auths, mft->ski, mft->aki);
	build_chain(a, &chain);

	if (!X509_STORE_CTX_init(ctx, NULL, x509, NULL))
		cryptoerrx("X509_STORE_CTX_init");

	/* CRL checked disabled here because CRL is referenced from mft */
	X509_STORE_CTX_set_flags(ctx, X509_V_FLAG_IGNORE_CRITICAL);
	X509_STORE_CTX_set_depth(ctx, MAX_CERT_DEPTH);
	X509_STORE_CTX_set0_trusted_stack(ctx, chain);

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		X509_STORE_CTX_cleanup(ctx);
		warnx("%s: %s", entp->file, X509_verify_cert_error_string(c));
		mft_free(mft);
		X509_free(x509);
		sk_X509_free(chain);
		return NULL;
	}

	X509_STORE_CTX_cleanup(ctx);
	sk_X509_free(chain);
	X509_free(x509);

	if (!mft_check(entp->file, mft)) {
		mft_free(mft);
		return NULL;
	}

	return mft;
}

/*
 * Certificates are from manifests (has a digest and is signed with
 * another certificate) Parse the certificate, make sure its
 * signatures are valid (with CRLs), then validate the RPKI content.
 * This returns a certificate (which must not be freed) or NULL on
 * parse failure.
 */
static struct cert *
proc_parser_cert(const struct entity *entp, const unsigned char *der,
    size_t len)
{
	struct cert		*cert;
	X509			*x509;
	int			 c;
	struct auth		*a = NULL;
	STACK_OF(X509)		*chain;
	STACK_OF(X509_CRL)	*crls;

	assert(!entp->has_data);

	/* Extract certificate data and X509. */

	cert = cert_parse(&x509, entp->file, der, len);
	if (cert == NULL)
		return NULL;

	a = valid_ski_aki(entp->file, &auths, cert->ski, cert->aki);
	build_chain(a, &chain);
	build_crls(get_crl(a), &crls);

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, NULL, x509, NULL))
		cryptoerrx("X509_STORE_CTX_init");

	X509_STORE_CTX_set_flags(ctx,
	    X509_V_FLAG_IGNORE_CRITICAL | X509_V_FLAG_CRL_CHECK);
	X509_STORE_CTX_set_depth(ctx, MAX_CERT_DEPTH);
	X509_STORE_CTX_set0_trusted_stack(ctx, chain);
	X509_STORE_CTX_set0_crls(ctx, crls);

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		warnx("%s: %s", entp->file,
		    X509_verify_cert_error_string(c));
		X509_STORE_CTX_cleanup(ctx);
		cert_free(cert);
		sk_X509_free(chain);
		sk_X509_CRL_free(crls);
		X509_free(x509);
		return NULL;
	}

	X509_STORE_CTX_cleanup(ctx);
	sk_X509_free(chain);
	sk_X509_CRL_free(crls);
	X509_free(x509);

	cert->talid = a->cert->talid;

	/* Validate the cert to get the parent */
	if (!valid_cert(entp->file, &auths, cert)) {
		cert_free(cert);
		return NULL;
	}

	/*
	 * Add validated CA certs to the RPKI auth tree.
	 */
	if (cert->purpose == CERT_PURPOSE_CA) {
		if (!auth_insert(&auths, cert, a)) {
			cert_free(cert);
			return NULL;
		}
	}

	return cert;
}

/*
 * Root certificates come from TALs (has a pkey and is self-signed).
 * Parse the certificate, ensure that it's public key matches the
 * known public key from the TAL, and then validate the RPKI
 * content.
 *
 * This returns a certificate (which must not be freed) or NULL on
 * parse failure.
 */
static struct cert *
proc_parser_root_cert(const struct entity *entp, const unsigned char *der,
    size_t len)
{
	char			subject[256];
	ASN1_TIME		*notBefore, *notAfter;
	X509_NAME		*name;
	struct cert		*cert;
	X509			*x509;

	assert(entp->has_data);

	/* Extract certificate data and X509. */

	cert = ta_parse(&x509, entp->file, der, len, entp->data, entp->datasz);
	if (cert == NULL)
		return NULL;

	if ((name = X509_get_subject_name(x509)) == NULL) {
		warnx("%s Unable to get certificate subject", entp->file);
		goto badcert;
	}
	if (X509_NAME_oneline(name, subject, sizeof(subject)) == NULL) {
		warnx("%s: Unable to parse certificate subject name",
		    entp->file);
		goto badcert;
	}
	if ((notBefore = X509_get_notBefore(x509)) == NULL) {
		warnx("%s: certificate has invalid notBefore, subject='%s'",
		    entp->file, subject);
		goto badcert;
	}
	if ((notAfter = X509_get_notAfter(x509)) == NULL) {
		warnx("%s: certificate has invalid notAfter, subject='%s'",
		    entp->file, subject);
		goto badcert;
	}
	if (X509_cmp_current_time(notBefore) != -1) {
		warnx("%s: certificate not yet valid, subject='%s'", entp->file,
		    subject);
		goto badcert;
	}
	if (X509_cmp_current_time(notAfter) != 1)  {
		warnx("%s: certificate has expired, subject='%s'", entp->file,
		    subject);
		goto badcert;
	}
	if (!valid_ta(entp->file, &auths, cert)) {
		warnx("%s: certificate not a valid ta, subject='%s'",
		    entp->file, subject);
		goto badcert;
	}

	X509_free(x509);

	cert->talid = entp->talid;

	/*
	 * Add valid roots to the RPKI auth tree.
	 */
	if (!auth_insert(&auths, cert, NULL)) {
		cert_free(cert);
		return NULL;
	}

	return cert;

 badcert:
	X509_free(x509);
	cert_free(cert);
	return NULL;
}

/*
 * Parse a certificate revocation list
 * This simply parses the CRL content itself, optionally validating it
 * within the digest if it comes from a manifest, then adds it to the
 * CRL tree.
 */
static void
proc_parser_crl(struct entity *entp, const unsigned char *der, size_t len)
{
	X509_CRL		*x509_crl;
	struct crl		*crl;
	const ASN1_TIME		*at;
	struct tm		 expires_tm;

	if ((x509_crl = crl_parse(entp->file, der, len)) != NULL) {
		if ((crl = malloc(sizeof(*crl))) == NULL)
			err(1, NULL);
		if ((crl->aki = x509_crl_get_aki(x509_crl, entp->file)) ==
		    NULL) {
			warnx("x509_crl_get_aki failed");
			goto err;
		}

		crl->x509_crl = x509_crl;

		/* extract expire time for later use */
		at = X509_CRL_get0_nextUpdate(x509_crl);
		if (at == NULL) {
			warnx("%s: X509_CRL_get0_nextUpdate failed",
			    entp->file);
			goto err;
		}
		memset(&expires_tm, 0, sizeof(expires_tm));
		if (ASN1_time_parse(at->data, at->length, &expires_tm,
		    0) == -1) {
			warnx("%s: ASN1_time_parse failed", entp->file);
			goto err;
		}
		if ((crl->expires = mktime(&expires_tm)) == -1)
			errx(1, "%s: mktime failed", entp->file);

		if (RB_INSERT(crl_tree, &crlt, crl) != NULL) {
			warnx("%s: duplicate AKI %s", entp->file, crl->aki);
			goto err;
		}
	}
	return;
 err:
	free_crl(crl);
}

/*
 * Parse a ghostbuster record
 */
static void
proc_parser_gbr(struct entity *entp, const unsigned char *der, size_t len)
{
	struct gbr		*gbr;
	X509			*x509;
	int			 c;
	struct auth		*a;
	STACK_OF(X509)		*chain;
	STACK_OF(X509_CRL)	*crls;

	if ((gbr = gbr_parse(&x509, entp->file, der, len)) == NULL)
		return;

	a = valid_ski_aki(entp->file, &auths, gbr->ski, gbr->aki);

	build_chain(a, &chain);
	build_crls(get_crl(a), &crls);

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, NULL, x509, NULL))
		cryptoerrx("X509_STORE_CTX_init");
	X509_STORE_CTX_set_flags(ctx,
	    X509_V_FLAG_IGNORE_CRITICAL | X509_V_FLAG_CRL_CHECK);
	X509_STORE_CTX_set_depth(ctx, MAX_CERT_DEPTH);
	X509_STORE_CTX_set0_trusted_stack(ctx, chain);
	X509_STORE_CTX_set0_crls(ctx, crls);

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		if (verbose > 0 || c != X509_V_ERR_UNABLE_TO_GET_CRL)
			warnx("%s: %s", entp->file,
			    X509_verify_cert_error_string(c));
	}

	X509_STORE_CTX_cleanup(ctx);
	sk_X509_free(chain);
	sk_X509_CRL_free(crls);
	X509_free(x509);
	gbr_free(gbr);
}

/*
 * Walk the certificate tree to the root and build a certificate
 * chain from cert->x509. All certs in the tree are validated and
 * can be loaded as trusted stack into the validator.
 */
static void
build_chain(const struct auth *a, STACK_OF(X509) **chain)
{
	*chain = NULL;

	if (a == NULL)
		return;

	if ((*chain = sk_X509_new_null()) == NULL)
		err(1, "sk_X509_new_null");
	for (; a != NULL; a = a->parent) {
		assert(a->cert->x509 != NULL);
		if (!sk_X509_push(*chain, a->cert->x509))
			errx(1, "sk_X509_push");
	}
}

/*
 * Find a CRL based on the auth SKI value.
 */
static struct crl *
get_crl(const struct auth *a)
{
	struct crl	find;

	if (a == NULL)
		return NULL;

	find.aki = a->cert->ski;
	return RB_FIND(crl_tree, &crlt, &find);
}

/*
 * Add the CRL based on the certs SKI value.
 * No need to insert any other CRL since those were already checked.
 */
static void
build_crls(const struct crl *crl, STACK_OF(X509_CRL) **crls)
{
	*crls = NULL;

	if (crl == NULL)
		return;

	if ((*crls = sk_X509_CRL_new_null()) == NULL)
		errx(1, "sk_X509_CRL_new_null");

	if (!sk_X509_CRL_push(*crls, crl->x509_crl))
		err(1, "sk_X509_CRL_push");
}

static void
parse_entity(struct entityq *q, struct msgbuf *msgq)
{
	struct entity	*entp;
	struct tal	*tal;
	struct cert	*cert;
	struct mft	*mft;
	struct roa	*roa;
	struct ibuf	*b;
	unsigned char	*f;
	size_t		 flen;
	int		 c;

	while ((entp = TAILQ_FIRST(q)) != NULL) {
		TAILQ_REMOVE(q, entp, entries);

		b = io_new_buffer();
		io_simple_buffer(b, &entp->type, sizeof(entp->type));

		f = NULL;
		if (entp->type != RTYPE_TAL) {
			f = load_file(entp->file, &flen);
			if (f == NULL)
				warn("%s", entp->file);
		}

		switch (entp->type) {
		case RTYPE_TAL:
			if ((tal = tal_parse(entp->file, entp->data,
			    entp->datasz)) == NULL)
				errx(1, "%s: could not parse tal file",
				    entp->file);
			tal->id = entp->talid;
			tal_buffer(b, tal);
			tal_free(tal);
			break;
		case RTYPE_CER:
			if (entp->has_data)
				cert = proc_parser_root_cert(entp, f, flen);
			else
				cert = proc_parser_cert(entp, f, flen);
			c = (cert != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (cert != NULL)
				cert_buffer(b, cert);
			/*
			 * The parsed certificate data "cert" is now
			 * managed in the "auths" table, so don't free
			 * it here (see the loop after "out").
			 */
			break;
		case RTYPE_CRL:
			proc_parser_crl(entp, f, flen);
			break;
		case RTYPE_MFT:
			mft = proc_parser_mft(entp, f, flen);
			c = (mft != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (mft != NULL)
				mft_buffer(b, mft);
			mft_free(mft);
			break;
		case RTYPE_ROA:
			roa = proc_parser_roa(entp, f, flen);
			c = (roa != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (roa != NULL)
				roa_buffer(b, roa);
			roa_free(roa);
			break;
		case RTYPE_GBR:
			proc_parser_gbr(entp, f, flen);
			break;
		default:
			abort();
		}

		free(f);
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
proc_parser(int fd)
{
	struct entityq	 q;
	struct msgbuf	 msgq;
	struct pollfd	 pfd;
	struct entity	*entp;
	struct ibuf	*b, *inbuf = NULL;

	ERR_load_crypto_strings();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();

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

		if (poll(&pfd, 1, INFTIM) == -1)
			err(1, "poll");
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

		parse_entity(&q, &msgq);
	}

	while ((entp = TAILQ_FIRST(&q)) != NULL) {
		TAILQ_REMOVE(&q, entp, entries);
		entity_free(entp);
	}

	/* XXX free auths and crl tree */

	X509_STORE_CTX_free(ctx);
	msgbuf_clear(&msgq);

	exit(0);
}
