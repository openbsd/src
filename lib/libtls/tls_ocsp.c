/*
 * Copyright (c) 2015 Marko Kreen <markokr@gmail.com>
 * Copyright (c) 2016 Bob Beck <beck@openbsd.org>
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

#include <arpa/inet.h>
#include <netinet/in.h>

#include <openssl/err.h>
#include <openssl/ocsp.h>
#include <openssl/x509.h>

#include <tls.h>
#include "tls_internal.h"

#define MAXAGE_SEC (14*24*60*60)
#define JITTER_SEC (60)

/*
 * State for request.
 */

static struct tls_ocsp_ctx *
tls_ocsp_ctx_new(void)
{
	return (calloc(1, sizeof(struct tls_ocsp_ctx)));
}

void
tls_ocsp_ctx_free(struct tls_ocsp_ctx *ocsp_ctx)
{
	if (ocsp_ctx == NULL)
		return;

	free(ocsp_ctx->ocsp_result);
	ocsp_ctx->ocsp_result = NULL;
	free(ocsp_ctx->ocsp_url);
	ocsp_ctx->ocsp_url = NULL;
	free(ocsp_ctx->request_data);
	ocsp_ctx->request_data = NULL;
	free(ocsp_ctx);
}

static int
tls_ocsp_asn1_parse_time(struct tls *ctx, ASN1_GENERALIZEDTIME *gt, time_t *gt_time)
{
	struct tm tm;

	if (gt == NULL)
		return -1;
	/* RFC 6960 specifies that all times in OCSP must be GENERALIZEDTIME */
	if (asn1_time_parse(gt->data, gt->length, &tm,
		V_ASN1_GENERALIZEDTIME) == -1)
		return -1;
	if ((*gt_time = timegm(&tm)) == -1)
		return -1;
	return 0;
}

static int
tls_ocsp_fill_info(struct tls *ctx, int response_status, int cert_status,
    int crl_reason, ASN1_GENERALIZEDTIME *revtime,
    ASN1_GENERALIZEDTIME *thisupd, ASN1_GENERALIZEDTIME *nextupd)
{
	struct tls_ocsp_result *info = NULL;

	free(ctx->ocsp_ctx->ocsp_result);
	ctx->ocsp_ctx->ocsp_result = NULL;

	if ((info = calloc(1, sizeof (struct tls_ocsp_result))) == NULL) {
		tls_set_error(ctx, "calloc");
		return -1;
	}
	info->response_status = response_status;
	info->cert_status = cert_status;
	info->crl_reason = crl_reason;
	if (info->response_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		info->result_msg =
		    OCSP_response_status_str(info->response_status);
	} else if (info->cert_status != V_OCSP_CERTSTATUS_REVOKED) {
		info->result_msg = OCSP_cert_status_str(info->cert_status);
	} else {
		info->result_msg = OCSP_crl_reason_str(info->crl_reason);
	}
	info->revocation_time = info->this_update = info->next_update = -1;
	if (revtime != NULL &&
	    tls_ocsp_asn1_parse_time(ctx, revtime, &info->revocation_time) != 0) {
		tls_set_error(ctx,
		    "unable to parse revocation time in OCSP reply");
		goto error;
	}
	if (thisupd != NULL &&
	    tls_ocsp_asn1_parse_time(ctx, thisupd, &info->this_update) != 0) {
		tls_set_error(ctx,
		    "unable to parse this update time in OCSP reply");
		goto error;
	}
	if (nextupd != NULL &&
	    tls_ocsp_asn1_parse_time(ctx, nextupd, &info->next_update) != 0) {
		tls_set_error(ctx,
		    "unable to parse next update time in OCSP reply");
		goto error;
	}
	ctx->ocsp_ctx->ocsp_result = info;
	return 0;
 error:
	free(info);
	return -1;
}

static OCSP_CERTID *
tls_ocsp_get_certid(X509 *main_cert, STACK_OF(X509) *extra_certs,
    SSL_CTX *ssl_ctx)
{
	X509_NAME *issuer_name;
	X509 *issuer;
	X509_STORE_CTX storectx;
	X509_OBJECT tmpobj;
	OCSP_CERTID *cid = NULL;
	X509_STORE *store;

	if ((issuer_name = X509_get_issuer_name(main_cert)) == NULL)
		return NULL;

	if (extra_certs != NULL) {
		issuer = X509_find_by_subject(extra_certs, issuer_name);
		if (issuer != NULL)
			return OCSP_cert_to_id(NULL, main_cert, issuer);
	}

	if ((store = SSL_CTX_get_cert_store(ssl_ctx)) == NULL)
		return NULL;
	if (X509_STORE_CTX_init(&storectx, store, main_cert, extra_certs) != 1)
		return NULL;
	if (X509_STORE_get_by_subject(&storectx, X509_LU_X509, issuer_name,
		&tmpobj) == 1) {
		cid = OCSP_cert_to_id(NULL, main_cert, tmpobj.data.x509);
		X509_OBJECT_free_contents(&tmpobj);
	}
	X509_STORE_CTX_cleanup(&storectx);
	return cid;
}

struct tls_ocsp_ctx *
tls_ocsp_setup_from_peer(struct tls *ctx)
{
	struct tls_ocsp_ctx *ocsp_ctx = NULL;
	STACK_OF(OPENSSL_STRING) *ocsp_urls = NULL;

	if ((ocsp_ctx = tls_ocsp_ctx_new()) == NULL)
		goto failed;

	/* steal state from ctx struct */
	ocsp_ctx->main_cert = SSL_get_peer_certificate(ctx->ssl_conn);
	ocsp_ctx->extra_certs = SSL_get_peer_cert_chain(ctx->ssl_conn);
	if (ocsp_ctx->main_cert == NULL) {
		tls_set_errorx(ctx, "no peer certificate for OCSP");
		goto failed;
	}

	ocsp_urls = X509_get1_ocsp(ocsp_ctx->main_cert);
	if (ocsp_urls == NULL)
		goto failed;
	ocsp_ctx->ocsp_url = strdup(sk_OPENSSL_STRING_value(ocsp_urls, 0));
	if (ocsp_ctx->ocsp_url == NULL) {
		tls_set_errorx(ctx, "out of memory");
		goto failed;
	}

	X509_email_free(ocsp_urls);
	return ocsp_ctx;

 failed:
	tls_ocsp_ctx_free(ocsp_ctx);
	X509_email_free(ocsp_urls);
	return NULL;
}

static int
tls_ocsp_verify_response(struct tls *ctx, OCSP_RESPONSE *resp)
{
	OCSP_BASICRESP *br = NULL;
	ASN1_GENERALIZEDTIME *revtime = NULL, *thisupd = NULL, *nextupd = NULL;
	OCSP_CERTID *cid = NULL;
	STACK_OF(X509) *combined = NULL;
	int response_status=0, cert_status=0, crl_reason=0;
	int ret = -1;
	unsigned long flags;

	if ((br = OCSP_response_get1_basic(resp)) == NULL) {
		tls_set_errorx(ctx, "cannot load ocsp reply");
		goto error;
	}

	/*
	 * Skip validation of 'extra_certs' as this should be done
	 * already as part of main handshake.
	 */
	flags = OCSP_TRUSTOTHER;

	/* now verify */
	if (OCSP_basic_verify(br, ctx->ocsp_ctx->extra_certs,
		SSL_CTX_get_cert_store(ctx->ssl_ctx), flags) != 1) {
		tls_set_error(ctx, "ocsp verify failed");
		goto error;
	}

	/* signature OK, look inside */
	response_status = OCSP_response_status(resp);
	if (response_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		tls_set_errorx(ctx, "ocsp verify failed: response - %s",
		    OCSP_response_status_str(response_status));
		goto error;
	}

	cid = tls_ocsp_get_certid(ctx->ocsp_ctx->main_cert,
	    ctx->ocsp_ctx->extra_certs, ctx->ssl_ctx);
	if (cid == NULL) {
		tls_set_errorx(ctx, "ocsp verify failed: no issuer cert");
		goto error;
	}

	if (OCSP_resp_find_status(br, cid, &cert_status, &crl_reason,
	    &revtime, &thisupd, &nextupd) != 1) {
		tls_set_errorx(ctx, "ocsp verify failed: no result for cert");
		goto error;
	}

	if (OCSP_check_validity(thisupd, nextupd, JITTER_SEC,
	    MAXAGE_SEC) != 1) {
		tls_set_errorx(ctx,
		    "ocsp verify failed: ocsp response not current");
		goto error;
	}

	if (tls_ocsp_fill_info(ctx, response_status, cert_status,
	    crl_reason, revtime, thisupd, nextupd) != 0)
		goto error;

	/* finally can look at status */
	if (cert_status != V_OCSP_CERTSTATUS_GOOD && cert_status !=
	    V_OCSP_CERTSTATUS_UNKNOWN) {
		tls_set_errorx(ctx, "ocsp verify failed: revoked cert - %s",
			       OCSP_crl_reason_str(crl_reason));
		goto error;
	}

	ret = 0;

 error:
	sk_X509_free(combined);
	OCSP_CERTID_free(cid);
	OCSP_BASICRESP_free(br);
	return ret;
}

/* TLS handshake verification callback for stapled requests */
int
tls_ocsp_verify_cb(SSL *ssl, void *arg)
{
	const unsigned char *raw = NULL;
	int size, res = -1;
	struct tls *ctx;

	if ((ctx = SSL_get_app_data(ssl)) == NULL)
		return -1;

	size = SSL_get_tlsext_status_ocsp_resp(ssl, &raw);
	if (size <= 0)
		return 1;

	tls_ocsp_ctx_free(ctx->ocsp_ctx);
	ctx->ocsp_ctx = tls_ocsp_setup_from_peer(ctx);
	if (ctx->ocsp_ctx != NULL)
		res = tls_ocsp_process_response(ctx, raw, size);

	return (res == 0) ? 1 : 0;
}

/*
 * Public API
 */

/* Retrieve OSCP URL from peer certificate, if present */
const char *
tls_peer_ocsp_url(struct tls *ctx)
{
	if (ctx->ocsp_ctx == NULL)
		return NULL;
	return ctx->ocsp_ctx->ocsp_url;
}

const char *
tls_peer_ocsp_result(struct tls *ctx)
{
	if (ctx->ocsp_ctx == NULL)
		return NULL;
	if (ctx->ocsp_ctx->ocsp_result == NULL)
		return NULL;
	return ctx->ocsp_ctx->ocsp_result->result_msg;
}

int
tls_peer_ocsp_response_status(struct tls *ctx)
{
	if (ctx->ocsp_ctx == NULL)
		return -1;
	if (ctx->ocsp_ctx->ocsp_result == NULL)
		return -1;
	return ctx->ocsp_ctx->ocsp_result->response_status;
}

int
tls_peer_ocsp_cert_status(struct tls *ctx)
{
	if (ctx->ocsp_ctx == NULL)
		return -1;
	if (ctx->ocsp_ctx->ocsp_result == NULL)
		return -1;
	return ctx->ocsp_ctx->ocsp_result->cert_status;
}

int
tls_peer_ocsp_crl_reason(struct tls *ctx)
{
	if (ctx->ocsp_ctx == NULL)
		return -1;
	if (ctx->ocsp_ctx->ocsp_result == NULL)
		return -1;
	return ctx->ocsp_ctx->ocsp_result->crl_reason;
}

time_t
tls_peer_ocsp_this_update(struct tls *ctx)
{
	if (ctx->ocsp_ctx == NULL)
		return -1;
	if (ctx->ocsp_ctx->ocsp_result == NULL)
		return -1;
	return ctx->ocsp_ctx->ocsp_result->this_update;
}

time_t
tls_peer_ocsp_next_update(struct tls *ctx)
{
	if (ctx->ocsp_ctx == NULL)
		return -1;
	if (ctx->ocsp_ctx->ocsp_result == NULL)
		return -1;
	return ctx->ocsp_ctx->ocsp_result->next_update;
}

time_t
tls_peer_ocsp_revocation_time(struct tls *ctx)
{
	if (ctx->ocsp_ctx == NULL)
		return -1;
	if (ctx->ocsp_ctx->ocsp_result == NULL)
		return -1;
	return ctx->ocsp_ctx->ocsp_result->revocation_time;
}

/*
 * Process a raw OCSP response from an OCSP server request.
 * OCSP details can then be retrieved with tls_peer_ocsp_* functions.
 * returns 0 if certificate ok, -1 otherwise.
 */
int
tls_ocsp_process_response(struct tls *ctx, const unsigned char *response,
    size_t size)
{
	int ret;
	OCSP_RESPONSE *resp;

	if ((ctx->state & TLS_HANDSHAKE_COMPLETE) == 0)
		return -1;

	resp = d2i_OCSP_RESPONSE(NULL, &response, size);
	if (resp == NULL) {
		tls_ocsp_ctx_free(ctx->ocsp_ctx);
		ctx->ocsp_ctx = NULL;
		tls_set_error(ctx, "unable to parse OCSP response");
		return -1;
	}
	ret = tls_ocsp_verify_response(ctx, resp);
	OCSP_RESPONSE_free(resp);
	return ret;
}
