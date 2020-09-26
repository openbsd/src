/* $OpenBSD: x509_verify.c,v 1.13 2020/09/26 15:44:06 jsing Exp $ */
/*
 * Copyright (c) 2020 Bob Beck <beck@openbsd.org>
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

/* x509_verify - inspired by golang's crypto/x509/Verify */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <openssl/safestack.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "x509_internal.h"
#include "x509_issuer_cache.h"

static int x509_verify_cert_valid(struct x509_verify_ctx *ctx, X509 *cert,
    struct x509_verify_chain *current_chain);
static void x509_verify_build_chains(struct x509_verify_ctx *ctx, X509 *cert,
    struct x509_verify_chain *current_chain);
static int x509_verify_cert_error(struct x509_verify_ctx *ctx, X509 *cert,
    size_t depth, int error, int ok);
static void x509_verify_chain_free(struct x509_verify_chain *chain);

#define X509_VERIFY_CERT_HASH (EVP_sha512())

struct x509_verify_chain *
x509_verify_chain_new(void)
{
	struct x509_verify_chain *chain;

	if ((chain = calloc(1, sizeof(*chain))) == NULL)
		goto err;
	if ((chain->certs = sk_X509_new_null()) == NULL)
		goto err;
	if ((chain->names = x509_constraints_names_new()) == NULL)
		goto err;

	return chain;
 err:
	x509_verify_chain_free(chain);
	return NULL;
}

static void
x509_verify_chain_clear(struct x509_verify_chain *chain)
{
	sk_X509_pop_free(chain->certs, X509_free);
	chain->certs = NULL;
	x509_constraints_names_free(chain->names);
	chain->names = NULL;
}

static void
x509_verify_chain_free(struct x509_verify_chain *chain)
{
	if (chain == NULL)
		return;
	x509_verify_chain_clear(chain);
	free(chain);
}

static struct x509_verify_chain *
x509_verify_chain_dup(struct x509_verify_chain *chain)
{
	struct x509_verify_chain *new_chain;

	if ((new_chain = x509_verify_chain_new()) == NULL)
		goto err;
	if ((new_chain->certs = X509_chain_up_ref(chain->certs)) == NULL)
		goto err;
	if ((new_chain->names =
	    x509_constraints_names_dup(chain->names)) == NULL)
		goto err;
	return(new_chain);
 err:
	x509_verify_chain_free(new_chain);
	return NULL;
}

static int
x509_verify_chain_append(struct x509_verify_chain *chain, X509 *cert,
    int *error)
{
	int verify_err = X509_V_ERR_UNSPECIFIED;

	if (!x509_constraints_extract_names(chain->names, cert,
	    sk_X509_num(chain->certs) == 0, &verify_err)) {
		*error = verify_err;
		return 0;
	}
	X509_up_ref(cert);
	if (!sk_X509_push(chain->certs, cert)) {
		X509_free(cert);
		*error = X509_V_ERR_OUT_OF_MEM;
		return 0;
	}
	return 1;
}

static X509 *
x509_verify_chain_last(struct x509_verify_chain *chain)
{
	int last;

	if (chain->certs == NULL)
		return NULL;
	if ((last = sk_X509_num(chain->certs) - 1) < 0)
		return NULL;
	return sk_X509_value(chain->certs, last);
}

X509 *
x509_verify_chain_leaf(struct x509_verify_chain *chain)
{
	if (chain->certs == NULL)
		return NULL;
	return sk_X509_value(chain->certs, 0);
}

static void
x509_verify_ctx_reset(struct x509_verify_ctx *ctx)
{
	size_t i;

	for (i = 0; i < ctx->chains_count; i++)
		x509_verify_chain_free(ctx->chains[i]);
	ctx->error = 0;
	ctx->error_depth = 0;
	ctx->chains_count = 0;
	ctx->sig_checks = 0;
	ctx->check_time = NULL;
}

static void
x509_verify_ctx_clear(struct x509_verify_ctx *ctx)
{
	x509_verify_ctx_reset(ctx);
	sk_X509_pop_free(ctx->intermediates, X509_free);
	free(ctx->chains);
	memset(ctx, 0, sizeof(*ctx));
}

static int
x509_verify_ctx_cert_is_root(struct x509_verify_ctx *ctx, X509 *cert)
{
	int i;

	for (i = 0; i < sk_X509_num(ctx->roots); i++) {
		if (X509_cmp(sk_X509_value(ctx->roots, i), cert) == 0)
			return 1;
	}
	return 0;
}

static int
x509_verify_ctx_set_xsc_chain(struct x509_verify_ctx *ctx,
    struct x509_verify_chain *chain)
{
	size_t depth;
	X509 *last = x509_verify_chain_last(chain);

	if (ctx->xsc == NULL)
		return 1;

	depth = sk_X509_num(chain->certs);
	if (depth > 0)
		depth--;

	ctx->xsc->last_untrusted = depth ? depth - 1 : 0;
	sk_X509_pop_free(ctx->xsc->chain, X509_free);
	ctx->xsc->chain = X509_chain_up_ref(chain->certs);
	if (ctx->xsc->chain == NULL)
		return x509_verify_cert_error(ctx, last, depth,
		    X509_V_ERR_OUT_OF_MEM, 0);
	return 1;
}

/* Add a validated chain to our list of valid chains */
static int
x509_verify_ctx_add_chain(struct x509_verify_ctx *ctx,
    struct x509_verify_chain *chain)
{
	size_t depth;
	X509 *last = x509_verify_chain_last(chain);

	depth = sk_X509_num(chain->certs);
	if (depth > 0)
		depth--;

	if (ctx->chains_count >= ctx->max_chains)
		return x509_verify_cert_error(ctx, last, depth,
		    X509_V_ERR_CERT_CHAIN_TOO_LONG, 0);

	/*
	 * If we have a legacy xsc, choose a validated chain,
	 * and apply the extensions, revocation, and policy checks
	 * just like the legacy code did. We do this here instead
	 * of as building the chains to more easily support the
	 * callback and the bewildering array of VERIFY_PARAM
	 * knobs that are there for the fiddling.
	 */
	if (ctx->xsc != NULL) {
		if (!x509_verify_ctx_set_xsc_chain(ctx, chain))
			return 0;

		/*
		 * XXX currently this duplicates some work done
		 * in chain build, but we keep it here until
		 * we have feature parity
		 */
		if (!x509_vfy_check_chain_extensions(ctx->xsc))
			return 0;

		if (!x509_constraints_chain(ctx->xsc->chain,
		    &ctx->xsc->error, &ctx->xsc->error_depth)) {
			X509 *cert = sk_X509_value(ctx->xsc->chain, depth);
			if (!x509_verify_cert_error(ctx, cert,
			    ctx->xsc->error_depth, ctx->xsc->error, 0))
				return 0;
		}

		if (!x509_vfy_check_revocation(ctx->xsc))
			return 0;

		if (!x509_vfy_check_policy(ctx->xsc))
			return 0;
	}
	/*
	 * no xsc means we are being called from the non-legacy API,
	 * extensions and purpose are dealt with as the chain is built.
	 *
	 * The non-legacy api returns multiple chains but does not do
	 * any revocation checking (it must be done by the caller on
	 * any chain they wish to use)
	 */

	if ((ctx->chains[ctx->chains_count] = x509_verify_chain_dup(chain)) ==
	    NULL) {
		return x509_verify_cert_error(ctx, last, depth,
		    X509_V_ERR_OUT_OF_MEM, 0);
	}
	ctx->chains_count++;
	ctx->error = X509_V_OK;
	ctx->error_depth = depth;
	return 1;
}

static int
x509_verify_potential_parent(struct x509_verify_ctx *ctx, X509 *parent,
    X509 *child)
{
	if (ctx->xsc != NULL)
		return (ctx->xsc->check_issued(ctx->xsc, child, parent));

	/* XXX key usage */
	return X509_check_issued(child, parent) != X509_V_OK;
}

static int
x509_verify_parent_signature(X509 *parent, X509 *child,
    unsigned char *child_md, int *error)
{
	unsigned char parent_md[EVP_MAX_MD_SIZE] = { 0 };
	EVP_PKEY *pkey;
	int cached;
	int ret = 0;

	/* Use cached value if we have it */
	if (child_md != NULL) {
		if (!X509_digest(parent, X509_VERIFY_CERT_HASH, parent_md,
		    NULL))
			return 0;
		if ((cached = x509_issuer_cache_find(parent_md, child_md)) >= 0)
			return cached;
	}

	/* Check signature. Did parent sign child? */
	if ((pkey = X509_get_pubkey(parent)) == NULL) {
		*error = X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY;
		return 0;
	}
	if (X509_verify(child, pkey) <= 0)
		*error = X509_V_ERR_CERT_SIGNATURE_FAILURE;
	else
		ret = 1;

	/* Add result to cache */
	if (child_md != NULL)
		x509_issuer_cache_add(parent_md, child_md, ret);

	EVP_PKEY_free(pkey);

	return ret;
}

static int
x509_verify_consider_candidate(struct x509_verify_ctx *ctx, X509 *cert,
    unsigned char *cert_md, int is_root_cert, X509 *candidate,
    struct x509_verify_chain *current_chain)
{
	int depth = sk_X509_num(current_chain->certs);
	struct x509_verify_chain *new_chain;
	int i;

	/* Fail if the certificate is already in the chain */
	for (i = 0; i < sk_X509_num(current_chain->certs); i++) {
		if (X509_cmp(sk_X509_value(current_chain->certs, i),
		    candidate) == 0)
			return 0;
	}

	if (ctx->sig_checks++ > X509_VERIFY_MAX_SIGCHECKS) {
		/* don't allow callback to override safety check */
		(void) x509_verify_cert_error(ctx, candidate, depth,
		    X509_V_ERR_CERT_CHAIN_TOO_LONG, 0);
		return 0;
	}


	if (!x509_verify_parent_signature(candidate, cert, cert_md,
	    &ctx->error)) {
		    if (!x509_verify_cert_error(ctx, candidate, depth,
			ctx->error, 0))
			    return 0;
	}

	if (!x509_verify_cert_valid(ctx, candidate, current_chain))
		return 0;

	/* candidate is good, add it to a copy of the current chain */
	if ((new_chain = x509_verify_chain_dup(current_chain)) == NULL) {
		x509_verify_cert_error(ctx, candidate, depth,
		    X509_V_ERR_OUT_OF_MEM, 0);
		return 0;
	}
	if (!x509_verify_chain_append(new_chain, candidate, &ctx->error)) {
		x509_verify_cert_error(ctx, candidate, depth,
		    ctx->error, 0);
		x509_verify_chain_free(new_chain);
		return 0;
	}

	/*
	 * If candidate is a trusted root, we have a validated chain,
	 * so we save it.  Otherwise, recurse until we find a root or
	 * give up.
	 */
	if (is_root_cert) {
		if (!x509_verify_ctx_set_xsc_chain(ctx, new_chain)) {
			x509_verify_chain_free(new_chain);
			return 0;
		}
		if (x509_verify_cert_error(ctx, candidate, depth, X509_V_OK, 1)) {
			(void) x509_verify_ctx_add_chain(ctx, new_chain);
			goto done;
		}
	}

	x509_verify_build_chains(ctx, candidate, new_chain);

 done:
	x509_verify_chain_free(new_chain);
	return 1;
}

static int
x509_verify_cert_error(struct x509_verify_ctx *ctx, X509 *cert, size_t depth,
    int error, int ok)
{
	ctx->error = error;
	ctx->error_depth = depth;
	if (ctx->xsc != NULL) {
		ctx->xsc->error = error;
		ctx->xsc->error_depth = depth;
		ctx->xsc->current_cert = cert;
		return ctx->xsc->verify_cb(ok, ctx->xsc);
	}
	return ok;
}

static void
x509_verify_build_chains(struct x509_verify_ctx *ctx, X509 *cert,
    struct x509_verify_chain *current_chain)
{
	unsigned char cert_md[EVP_MAX_MD_SIZE] = { 0 };
	X509 *candidate;
	int i, depth, count;

	depth = sk_X509_num(current_chain->certs);
	if (depth > 0)
		depth--;

	if (depth >= ctx->max_depth &&
	    !x509_verify_cert_error(ctx, cert, depth,
		X509_V_ERR_CERT_CHAIN_TOO_LONG, 0))
		return;

	if (!X509_digest(cert, X509_VERIFY_CERT_HASH, cert_md, NULL) &&
	    !x509_verify_cert_error(ctx, cert, depth,
		X509_V_ERR_UNSPECIFIED, 0))
		return;

	count = ctx->chains_count;
	ctx->error = X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY;
	ctx->error_depth = depth;

	for (i = 0; i < sk_X509_num(ctx->roots); i++) {
		candidate = sk_X509_value(ctx->roots, i);
		if (x509_verify_potential_parent(ctx, candidate, cert)) {
			x509_verify_consider_candidate(ctx, cert,
			    cert_md, 1, candidate, current_chain);
		}
	}

	if (ctx->intermediates != NULL) {
		for (i = 0; i < sk_X509_num(ctx->intermediates); i++) {
			candidate = sk_X509_value(ctx->intermediates, i);
			if (x509_verify_potential_parent(ctx, candidate, cert)) {
				x509_verify_consider_candidate(ctx, cert,
				    cert_md, 0, candidate, current_chain);
			}
		}
	}
	if (ctx->chains_count > count) {
		if (ctx->xsc != NULL) {
			ctx->xsc->error = X509_V_OK;
			ctx->xsc->error_depth = depth;
			ctx->xsc->current_cert = cert;
			(void) ctx->xsc->verify_cb(1, ctx->xsc);
		}
	} else if (ctx->error_depth == depth) {
			(void) x509_verify_cert_error(ctx, cert, depth,
			    ctx->error, 0);
	}
}

static int
x509_verify_cert_hostname(struct x509_verify_ctx *ctx, X509 *cert, char *name)
{
	char *candidate;
	size_t len;

	if (name == NULL) {
		if (ctx->xsc != NULL)
			return x509_vfy_check_id(ctx->xsc);
		return 1;
	}
	if ((candidate = strdup(name)) == NULL) {
		ctx->error = X509_V_ERR_OUT_OF_MEM;
		goto err;
	}
	if ((len = strlen(candidate)) < 1) {
		ctx->error = X509_V_ERR_UNSPECIFIED; /* XXX */
		goto err;
	}

	/* IP addresses may be written in [ ]. */
	if (candidate[0] == '[' && candidate[len - 1] == ']') {
		candidate[len - 1] = '\0';
		if (X509_check_ip_asc(cert, candidate + 1, 0) <= 0) {
			ctx->error = X509_V_ERR_IP_ADDRESS_MISMATCH;
			goto err;
		}
	} else {
		int flags = 0;

		if (ctx->xsc == NULL)
			flags = X509_CHECK_FLAG_NEVER_CHECK_SUBJECT;

		if (X509_check_host(cert, candidate, len, flags, NULL) <= 0) {
			ctx->error = X509_V_ERR_HOSTNAME_MISMATCH;
			goto err;
		}
	}
	free(candidate);
	return 1;
 err:
	free(candidate);
	return x509_verify_cert_error(ctx, cert, 0, ctx->error, 0);
}

static int
x509_verify_set_check_time(struct x509_verify_ctx *ctx) {
	if (ctx->xsc != NULL)  {
		if (ctx->xsc->param->flags & X509_V_FLAG_USE_CHECK_TIME) {
			ctx->check_time = &ctx->xsc->param->check_time;
			return 1;
		}
		if (ctx->xsc->param->flags & X509_V_FLAG_NO_CHECK_TIME)
			return 0;
	}

	ctx->check_time = NULL;
	return 1;
}

int
x509_verify_asn1_time_to_tm(const ASN1_TIME *atime, struct tm *tm, int notafter)
{
	int type;

	memset(tm, 0, sizeof(*tm));

	type = ASN1_time_parse(atime->data, atime->length, tm, atime->type);
	if (type == -1)
		return 0;

	/* RFC 5280 section 4.1.2.5 */
	if (tm->tm_year < 150 && type != V_ASN1_UTCTIME)
		return 0;
	if (tm->tm_year >= 150 && type != V_ASN1_GENERALIZEDTIME)
		return 0;

	if (notafter) {
		/*
		 * If we are a completely broken operating system with a
		 * 32 bit time_t, and we have been told this is a notafter
		 * date, limit the date to a 32 bit representable value.
		 */
		if (!ASN1_time_tm_clamp_notafter(tm))
			return 0;
	}

	/*
	 * Defensively fail if the time string is not representable as
	 * a time_t. A time_t must be sane if you care about times after
	 * Jan 19 2038.
	 */
	if (timegm(tm) == -1)
		return 0;

	return 1;
}

static int
x509_verify_cert_time(int is_notafter, const ASN1_TIME *cert_asn1,
    time_t *cmp_time, int *error)
{
	struct tm cert_tm, when_tm;
	time_t when;

	if (cmp_time == NULL)
		when = time(NULL);
	else
		when = *cmp_time;

	if (!x509_verify_asn1_time_to_tm(cert_asn1, &cert_tm,
	    is_notafter)) {
		*error = is_notafter ?
		    X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD :
		    X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD;
		return 0;
	}

	if (gmtime_r(&when, &when_tm) == NULL) {
		*error = X509_V_ERR_UNSPECIFIED;
		return 0;
	}

	if (is_notafter) {
		if (ASN1_time_tm_cmp(&cert_tm, &when_tm) == -1) {
			*error = X509_V_ERR_CERT_HAS_EXPIRED;
			return 0;
		}
	} else  {
		if (ASN1_time_tm_cmp(&cert_tm, &when_tm) == 1) {
			*error = X509_V_ERR_CERT_NOT_YET_VALID;
			return 0;
		}
	}

	return 1;
}

static int
x509_verify_validate_constraints(X509 *cert,
    struct x509_verify_chain *current_chain, int *error)
{
	struct x509_constraints_names *excluded = NULL;
	struct x509_constraints_names *permitted = NULL;
	int err = X509_V_ERR_UNSPECIFIED;

	if (current_chain == NULL)
		return 1;

	if (cert->nc != NULL) {
		if ((permitted = x509_constraints_names_new()) == NULL) {
			err = X509_V_ERR_OUT_OF_MEM;
			goto err;
		}
		if ((excluded = x509_constraints_names_new()) == NULL) {
			err = X509_V_ERR_OUT_OF_MEM;
			goto err;
		}
		if (!x509_constraints_extract_constraints(cert,
		    permitted, excluded, &err))
			goto err;
		if (!x509_constraints_check(current_chain->names,
		    permitted, excluded, &err))
			goto err;
		x509_constraints_names_free(excluded);
		x509_constraints_names_free(permitted);
	}

	return 1;
 err:
	*error = err;
	x509_constraints_names_free(excluded);
	x509_constraints_names_free(permitted);
	return 0;
}

static int
x509_verify_cert_extensions(struct x509_verify_ctx *ctx, X509 *cert, int need_ca)
{
	if (!(cert->ex_flags & EXFLAG_SET)) {
		CRYPTO_w_lock(CRYPTO_LOCK_X509);
		x509v3_cache_extensions(cert);
		CRYPTO_w_unlock(CRYPTO_LOCK_X509);
	}

	if (ctx->xsc != NULL)
		return 1;	/* legacy is checked after chain is built */

	if (cert->ex_flags & EXFLAG_CRITICAL) {
		ctx->error = X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION;
		return 0;
	}
	/* No we don't care about v1, netscape, and other ancient silliness */
	if (need_ca && (!(cert->ex_flags & EXFLAG_BCONS) &&
	    (cert->ex_flags & EXFLAG_CA))) {
		ctx->error = X509_V_ERR_INVALID_CA;
		return 0;
	}
	if (ctx->purpose > 0 && X509_check_purpose(cert, ctx->purpose, need_ca)) {
		ctx->error = X509_V_ERR_INVALID_PURPOSE;
		return 0;
	}

	/* XXX support proxy certs later in new api */
	if (ctx->xsc == NULL && cert->ex_flags & EXFLAG_PROXY) {
		ctx->error = X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED;
		return 0;
	}

	return 1;
}

/* Validate that cert is a possible candidate to append to current_chain */
static int
x509_verify_cert_valid(struct x509_verify_ctx *ctx, X509 *cert,
    struct x509_verify_chain *current_chain)
{
	X509 *issuer_candidate;
	int should_be_ca = current_chain != NULL;
	size_t depth = 0;

	if (current_chain != NULL)
		depth = sk_X509_num(current_chain->certs);

	if (!x509_verify_cert_extensions(ctx, cert, should_be_ca))
		return 0;

	if (should_be_ca) {
		issuer_candidate = x509_verify_chain_last(current_chain);
		if (issuer_candidate != NULL &&
		    !X509_check_issued(issuer_candidate, cert))
			if (!x509_verify_cert_error(ctx, cert, depth,
			    X509_V_ERR_SUBJECT_ISSUER_MISMATCH, 0))
				return 0;
	}

	if (x509_verify_set_check_time(ctx)) {
		if (!x509_verify_cert_time(0, X509_get_notBefore(cert),
		    ctx->check_time, &ctx->error)) {
			if (!x509_verify_cert_error(ctx, cert, depth,
			    ctx->error, 0))
				return 0;
		}

		if (!x509_verify_cert_time(1, X509_get_notAfter(cert),
		    ctx->check_time, &ctx->error)) {
			if (!x509_verify_cert_error(ctx, cert, depth,
			    ctx->error, 0))
				return 0;
		}
	}

	if (!x509_verify_validate_constraints(cert, current_chain,
	    &ctx->error) && !x509_verify_cert_error(ctx, cert, depth,
	    ctx->error, 0))
		return 0;

	return 1;
}

struct x509_verify_ctx *
x509_verify_ctx_new_from_xsc(X509_STORE_CTX *xsc, STACK_OF(X509) *roots)
{
	struct x509_verify_ctx *ctx;
	size_t max_depth;

	if (xsc == NULL)
		return NULL;

	if ((ctx = x509_verify_ctx_new(roots)) == NULL)
		return NULL;

	ctx->xsc = xsc;

	if (xsc->untrusted &&
	    (ctx->intermediates = X509_chain_up_ref(xsc->untrusted)) == NULL)
		goto err;

	max_depth = X509_VERIFY_MAX_CHAIN_CERTS;
	if (xsc->param->depth > 0 && xsc->param->depth < X509_VERIFY_MAX_CHAIN_CERTS)
		max_depth = xsc->param->depth;
	if (!x509_verify_ctx_set_max_depth(ctx, max_depth))
		goto err;

	return ctx;
 err:
	x509_verify_ctx_free(ctx);
	return NULL;
}

/* Public API */

struct x509_verify_ctx *
x509_verify_ctx_new(STACK_OF(X509) *roots)
{
	struct x509_verify_ctx *ctx;

	if (roots == NULL)
		return NULL;

	if ((ctx = calloc(1, sizeof(struct x509_verify_ctx))) == NULL)
		return NULL;

	if ((ctx->roots = X509_chain_up_ref(roots)) == NULL)
		goto err;

	ctx->max_depth = X509_VERIFY_MAX_CHAIN_CERTS;
	ctx->max_chains = X509_VERIFY_MAX_CHAINS;
	ctx->max_sigs = X509_VERIFY_MAX_SIGCHECKS;

	if ((ctx->chains = calloc(X509_VERIFY_MAX_CHAINS,
	    sizeof(*ctx->chains))) == NULL)
		goto err;

	return ctx;
 err:
	x509_verify_ctx_free(ctx);
	return NULL;
}

void
x509_verify_ctx_free(struct x509_verify_ctx *ctx)
{
	if (ctx == NULL)
		return;
	sk_X509_pop_free(ctx->roots, X509_free);
	x509_verify_ctx_clear(ctx);
	free(ctx);
}

int
x509_verify_ctx_set_max_depth(struct x509_verify_ctx *ctx, size_t max)
{
	if (max < 1 || max > X509_VERIFY_MAX_CHAIN_CERTS)
		return 0;
	ctx->max_depth = max;
	return 1;
}

int
x509_verify_ctx_set_max_chains(struct x509_verify_ctx *ctx, size_t max)
{
	if (max < 1 || max > X509_VERIFY_MAX_CHAINS)
		return 0;
	ctx->max_chains = max;
	return 1;
}

int
x509_verify_ctx_set_max_signatures(struct x509_verify_ctx *ctx, size_t max)
{
	if (max < 1 || max > 100000)
		return 0;
	ctx->max_sigs = max;
	return 1;
}

int
x509_verify_ctx_set_purpose(struct x509_verify_ctx *ctx, int purpose)
{
	if (purpose < X509_PURPOSE_MIN || purpose > X509_PURPOSE_MAX)
		return 0;
	ctx->purpose = purpose;
	return 1;
}

int
x509_verify_ctx_set_intermediates(struct x509_verify_ctx *ctx,
    STACK_OF(X509) *intermediates)
{
	if ((ctx->intermediates = X509_chain_up_ref(intermediates)) == NULL)
		return 0;
	return 1;
}

const char *
x509_verify_ctx_error_string(struct x509_verify_ctx *ctx)
{
	return X509_verify_cert_error_string(ctx->error);
}

size_t
x509_verify_ctx_error_depth(struct x509_verify_ctx *ctx)
{
	return ctx->error_depth;
}

STACK_OF(X509) *
x509_verify_ctx_chain(struct x509_verify_ctx *ctx, size_t i)
{
	if (i >= ctx->chains_count)
		return NULL;
	return ctx->chains[i]->certs;
}

size_t
x509_verify(struct x509_verify_ctx *ctx, X509 *leaf, char *name)
{
	struct x509_verify_chain *current_chain;

	if (ctx->roots == NULL || ctx->max_depth == 0) {
		ctx->error = X509_V_ERR_INVALID_CALL;
		return 0;
	}

	if (ctx->xsc != NULL) {
		if (leaf != NULL || name != NULL) {
			ctx->error = X509_V_ERR_INVALID_CALL;
			return 0;
		}
		leaf = ctx->xsc->cert;

		/*
		 * XXX
		 * The legacy code expects the top level cert to be
		 * there, even if we didn't find a chain. So put it
		 * there, we will clobber it later if we find a valid
		 * chain.
		 */
		if ((ctx->xsc->chain = sk_X509_new_null()) == NULL) {
			ctx->error = X509_V_ERR_OUT_OF_MEM;
			return 0;
		}
		if (!X509_up_ref(leaf)) {
			ctx->error = X509_V_ERR_OUT_OF_MEM;
			return 0;
		}
		if (!sk_X509_push(ctx->xsc->chain, leaf)) {
			X509_free(leaf);
			ctx->error = X509_V_ERR_OUT_OF_MEM;
			return 0;
		}
		ctx->xsc->error_depth = 0;
		ctx->xsc->current_cert = leaf;
	}

	if (!x509_verify_cert_valid(ctx, leaf, NULL))
		return 0;

	if (!x509_verify_cert_hostname(ctx, leaf, name))
		return 0;

	if ((current_chain = x509_verify_chain_new()) == NULL) {
		ctx->error = X509_V_ERR_OUT_OF_MEM;
		return 0;
	}
	if (!x509_verify_chain_append(current_chain, leaf, &ctx->error)) {
		x509_verify_chain_free(current_chain);
		return 0;
	}
	if (x509_verify_ctx_cert_is_root(ctx, leaf))
		x509_verify_ctx_add_chain(ctx, current_chain);
	else
		x509_verify_build_chains(ctx, leaf, current_chain);

	x509_verify_chain_free(current_chain);

	/*
	 * Safety net:
	 * We could not find a validated chain, and for some reason do not
	 * have an error set.
	 */
	if (ctx->chains_count == 0 && ctx->error == 0)
		ctx->error = X509_V_ERR_UNSPECIFIED;

	/* Clear whatever errors happened if we have any validated chain */
	if (ctx->chains_count > 0)
		ctx->error = X509_V_OK;

	if (ctx->xsc != NULL) {
		ctx->xsc->error = ctx->error;
		return ctx->xsc->verify_cb(ctx->chains_count, ctx->xsc);
	}
	return (ctx->chains_count);
}
