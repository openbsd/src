/*	$OpenBSD: parser.c,v 1.6 2021/03/02 09:00:46 claudio Exp $ */
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

static void	build_chain(const struct auth *, STACK_OF(X509) **);
static void	build_crls(const struct auth *, struct crl_tree *,
		    STACK_OF(X509_CRL) **);
/*
 * Parse and validate a ROA.
 * This is standard stuff.
 * Returns the roa on success, NULL on failure.
 */
static struct roa *
proc_parser_roa(struct entity *entp,
    X509_STORE *store, X509_STORE_CTX *ctx,
    struct auth_tree *auths, struct crl_tree *crlt)
{
	struct roa		*roa;
	X509			*x509;
	int			 c;
	struct auth		*a;
	STACK_OF(X509)		*chain;
	STACK_OF(X509_CRL)	*crls;

	if ((roa = roa_parse(&x509, entp->file)) == NULL)
		return NULL;

	a = valid_ski_aki(entp->file, auths, roa->ski, roa->aki);

	build_chain(a, &chain);
	build_crls(a, crlt, &crls);

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, store, x509, chain))
		cryptoerrx("X509_STORE_CTX_init");
	X509_STORE_CTX_set_flags(ctx,
	    X509_V_FLAG_IGNORE_CRITICAL | X509_V_FLAG_CRL_CHECK);
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
	sk_X509_free(chain);
	sk_X509_CRL_free(crls);
	X509_free(x509);

	/*
	 * If the ROA isn't valid, we accept it anyway and depend upon
	 * the code around roa_read() to check the "valid" field itself.
	 */

	if (valid_roa(entp->file, auths, roa))
		roa->valid = 1;

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
proc_parser_mft(struct entity *entp, X509_STORE *store, X509_STORE_CTX *ctx,
	struct auth_tree *auths, struct crl_tree *crlt)
{
	struct mft		*mft;
	X509			*x509;
	int			 c;
	struct auth		*a;
	STACK_OF(X509)		*chain;

	if ((mft = mft_parse(&x509, entp->file)) == NULL)
		return NULL;

	a = valid_ski_aki(entp->file, auths, mft->ski, mft->aki);
	build_chain(a, &chain);

	if (!X509_STORE_CTX_init(ctx, store, x509, chain))
		cryptoerrx("X509_STORE_CTX_init");

	/* CRL checked disabled here because CRL is referenced from mft */
	X509_STORE_CTX_set_flags(ctx, X509_V_FLAG_IGNORE_CRITICAL);

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
proc_parser_cert(const struct entity *entp,
    X509_STORE *store, X509_STORE_CTX *ctx,
    struct auth_tree *auths, struct crl_tree *crlt)
{
	struct cert		*cert;
	X509			*x509;
	int			 c;
	struct auth		*a = NULL, *na;
	char			*tal;
	STACK_OF(X509)		*chain;
	STACK_OF(X509_CRL)	*crls;

	assert(!entp->has_pkey);

	/* Extract certificate data and X509. */

	cert = cert_parse(&x509, entp->file);
	if (cert == NULL)
		return NULL;

	a = valid_ski_aki(entp->file, auths, cert->ski, cert->aki);
	build_chain(a, &chain);
	build_crls(a, crlt, &crls);

	/*
	 * Validate certificate chain w/CRLs.
	 * Only check the CRLs if specifically asked.
	 */

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, store, x509, chain))
		cryptoerrx("X509_STORE_CTX_init");

	X509_STORE_CTX_set_flags(ctx,
	    X509_V_FLAG_IGNORE_CRITICAL | X509_V_FLAG_CRL_CHECK);
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

	/* Validate the cert to get the parent */
	if (!valid_cert(entp->file, auths, cert)) {
		X509_free(x509); // needed? XXX
		return cert;
	}

	/*
	 * Add validated certs to the RPKI auth tree.
	 */

	cert->valid = 1;

	na = malloc(sizeof(*na));
	if (na == NULL)
		err(1, NULL);

	tal = a->tal;

	na->parent = a;
	na->cert = cert;
	na->tal = tal;
	na->fn = strdup(entp->file);
	if (na->fn == NULL)
		err(1, NULL);

	if (RB_INSERT(auth_tree, auths, na) != NULL)
		err(1, "auth tree corrupted");

	return cert;
}


/*
 * Root certificates come from TALs (has a pkey and is self-signed).
 * Parse the certificate, ensure that it's public key matches the
 * known public key from the TAL, and then validate the RPKI
 * content. If valid, we add it as a trusted root (trust anchor) to
 * "store".
 *
 * This returns a certificate (which must not be freed) or NULL on
 * parse failure.
 */
static struct cert *
proc_parser_root_cert(const struct entity *entp,
    X509_STORE *store, X509_STORE_CTX *ctx,
    struct auth_tree *auths, struct crl_tree *crlt)
{
	char			subject[256];
	ASN1_TIME		*notBefore, *notAfter;
	X509_NAME		*name;
	struct cert		*cert;
	X509			*x509;
	struct auth		*na;
	char			*tal;

	assert(entp->has_pkey);

	/* Extract certificate data and X509. */

	cert = ta_parse(&x509, entp->file, entp->pkey, entp->pkeysz);
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
	if (!valid_ta(entp->file, auths, cert)) {
		warnx("%s: certificate not a valid ta, subject='%s'",
		    entp->file, subject);
		goto badcert;
	}

	/*
	 * Add valid roots to the RPKI auth tree and as a trusted root
	 * for chain validation to the X509_STORE.
	 */

	cert->valid = 1;

	na = malloc(sizeof(*na));
	if (na == NULL)
		err(1, NULL);

	if ((tal = strdup(entp->descr)) == NULL)
		err(1, NULL);

	na->parent = NULL;
	na->cert = cert;
	na->tal = tal;
	na->fn = strdup(entp->file);
	if (na->fn == NULL)
		err(1, NULL);

	if (RB_INSERT(auth_tree, auths, na) != NULL)
		err(1, "auth tree corrupted");

	X509_STORE_add_cert(store, x509);

	return cert;
 badcert:
	X509_free(x509); // needed? XXX
	return cert;
}

/*
 * Parse a certificate revocation list
 * This simply parses the CRL content itself, optionally validating it
 * within the digest if it comes from a manifest, then adds it to the
 * store of CRLs.
 */
static void
proc_parser_crl(struct entity *entp, X509_STORE *store,
    X509_STORE_CTX *ctx, struct crl_tree *crlt)
{
	X509_CRL		*x509_crl;
	struct crl		*crl;

	if ((x509_crl = crl_parse(entp->file)) != NULL) {
		if ((crl = malloc(sizeof(*crl))) == NULL)
			err(1, NULL);
		if ((crl->aki = x509_crl_get_aki(x509_crl, entp->file)) ==
		    NULL)
			errx(1, "x509_crl_get_aki failed");
		crl->x509_crl = x509_crl;

		if (RB_INSERT(crl_tree, crlt, crl) != NULL) {
			warnx("%s: duplicate AKI %s", entp->file, crl->aki);
			free_crl(crl);
		}
	}
}

/*
 * Parse a ghostbuster record
 */
static void
proc_parser_gbr(struct entity *entp, X509_STORE *store,
    X509_STORE_CTX *ctx, struct auth_tree *auths, struct crl_tree *crlt)
{
	struct gbr		*gbr;
	X509			*x509;
	int			 c;
	struct auth		*a;
	STACK_OF(X509)		*chain;
	STACK_OF(X509_CRL)	*crls;

	if ((gbr = gbr_parse(&x509, entp->file)) == NULL)
		return;

	a = valid_ski_aki(entp->file, auths, gbr->ski, gbr->aki);

	build_chain(a, &chain);
	build_crls(a, crlt, &crls);

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, store, x509, chain))
		cryptoerrx("X509_STORE_CTX_init");
	X509_STORE_CTX_set_flags(ctx,
	    X509_V_FLAG_IGNORE_CRITICAL | X509_V_FLAG_CRL_CHECK);
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
 * Use the parent (id) to walk the tree to the root and
 * build a certificate chain from cert->x509. Do not include
 * the root node since this node should already be in the X509_STORE
 * as a trust anchor.
 */
static void
build_chain(const struct auth *a, STACK_OF(X509) **chain)
{
	*chain = NULL;

	if (a == NULL)
		return;

	if ((*chain = sk_X509_new_null()) == NULL)
		err(1, "sk_X509_new_null");
	for (; a->parent != NULL; a = a->parent) {
		assert(a->cert->x509 != NULL);
		if (!sk_X509_push(*chain, a->cert->x509))
			errx(1, "sk_X509_push");
	}
}

/* use the parent (id) to walk the tree to the root and
   build a stack of CRLs */
static void
build_crls(const struct auth *a, struct crl_tree *crlt,
    STACK_OF(X509_CRL) **crls)
{
	struct crl	find, *found;

	if ((*crls = sk_X509_CRL_new_null()) == NULL)
		errx(1, "sk_X509_CRL_new_null");

	if (a == NULL)
		return;

	find.aki = a->cert->ski;
	found = RB_FIND(crl_tree, crlt, &find);
	if (found && !sk_X509_CRL_push(*crls, found->x509_crl))
		err(1, "sk_X509_CRL_push");
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
	struct tal	*tal;
	struct cert	*cert;
	struct mft	*mft;
	struct roa	*roa;
	struct entity	*entp;
	struct entityq	 q;
	int		 c, rc = 1;
	struct msgbuf	 msgq;
	struct pollfd	 pfd;
	struct ibuf	*b;
	X509_STORE	*store;
	X509_STORE_CTX	*ctx;
	struct auth_tree auths = RB_INITIALIZER(&auths);
	struct crl_tree	 crlt = RB_INITIALIZER(&crlt);

	ERR_load_crypto_strings();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();

	if ((store = X509_STORE_new()) == NULL)
		cryptoerrx("X509_STORE_new");
	if ((ctx = X509_STORE_CTX_new()) == NULL)
		cryptoerrx("X509_STORE_CTX_new");

	TAILQ_INIT(&q);

	msgbuf_init(&msgq);
	msgq.fd = fd;

	pfd.fd = fd;

	io_socket_nonblocking(pfd.fd);

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

		/*
		 * Start with read events.
		 * This means that the parent process is sending us
		 * something we need to parse.
		 * We don't actually parse it til we have space in our
		 * outgoing buffer for responding, though.
		 */

		if ((pfd.revents & POLLIN)) {
			io_socket_blocking(fd);
			entp = calloc(1, sizeof(struct entity));
			if (entp == NULL)
				err(1, NULL);
			entity_read_req(fd, entp);
			TAILQ_INSERT_TAIL(&q, entp, entries);
			io_socket_nonblocking(fd);
		}

		if (pfd.revents & POLLOUT) {
			switch (msgbuf_write(&msgq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}

		/*
		 * If there's nothing to parse, then stop waiting for
		 * the write signal.
		 */

		if (TAILQ_EMPTY(&q)) {
			pfd.events &= ~POLLOUT;
			continue;
		}

		entp = TAILQ_FIRST(&q);
		assert(entp != NULL);

		if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
			err(1, NULL);
		io_simple_buffer(b, &entp->type, sizeof(entp->type));

		switch (entp->type) {
		case RTYPE_TAL:
			if ((tal = tal_parse(entp->file, entp->descr)) == NULL)
				goto out;
			tal_buffer(b, tal);
			tal_free(tal);
			break;
		case RTYPE_CER:
			if (entp->has_pkey)
				cert = proc_parser_root_cert(entp, store, ctx,
				    &auths, &crlt);
			else
				cert = proc_parser_cert(entp, store, ctx,
				    &auths, &crlt);
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
		case RTYPE_MFT:
			mft = proc_parser_mft(entp, store, ctx, &auths, &crlt);
			c = (mft != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (mft != NULL)
				mft_buffer(b, mft);
			mft_free(mft);
			break;
		case RTYPE_CRL:
			proc_parser_crl(entp, store, ctx, &crlt);
			break;
		case RTYPE_ROA:
			roa = proc_parser_roa(entp, store, ctx, &auths, &crlt);
			c = (roa != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (roa != NULL)
				roa_buffer(b, roa);
			roa_free(roa);
			break;
		case RTYPE_GBR:
			proc_parser_gbr(entp, store, ctx, &auths, &crlt);
			break;
		default:
			abort();
		}

		ibuf_close(&msgq, b);
		TAILQ_REMOVE(&q, entp, entries);
		entity_free(entp);
	}

	rc = 0;
out:
	while ((entp = TAILQ_FIRST(&q)) != NULL) {
		TAILQ_REMOVE(&q, entp, entries);
		entity_free(entp);
	}

	/* XXX free auths and crl tree */

	X509_STORE_CTX_free(ctx);
	X509_STORE_free(store);

	msgbuf_clear(&msgq);

	exit(rc);
}
