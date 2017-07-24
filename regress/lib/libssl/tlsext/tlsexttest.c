/* $OpenBSD: tlsexttest.c,v 1.3 2017/07/24 17:42:14 jsing Exp $ */
/*
 * Copyright (c) 2017 Joel Sing <jsing@openbsd.org>
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

#include <err.h>

#include "ssl_locl.h"

#include "bytestring.h"
#include "ssl_tlsext.h"

static void
hexdump(const unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 1; i <= len; i++)
		fprintf(stderr, " 0x%02hhx,%s", buf[i - 1], i % 8 ? "" : "\n");

	fprintf(stderr, "\n");
}

/*
 * Renegotiation Indication - RFC 5746.
 */

static unsigned char tlsext_ri_prev_client[] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};

static unsigned char tlsext_ri_prev_server[] = {
	0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88,
	0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
};

static unsigned char tlsext_ri_clienthello[] = {
	0x10,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};

static unsigned char tlsext_ri_serverhello[] = {
	0x20,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
	0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88,
	0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
};

static int
test_tlsext_ri_clienthello(void)
{
	unsigned char *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int failure = 0;
	size_t dlen;
	int alert;
	CBB cbb;
	CBS cbs;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLSv1_2_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if (tlsext_ri_clienthello_needs(ssl)) {
		fprintf(stderr, "FAIL: clienthello should not need RI\n");
		failure = 1;
		goto done;
	}

	if (!SSL_renegotiate(ssl)) {
		fprintf(stderr, "FAIL: client failed to set renegotiate\n");
		failure = 1;
		goto done;
	}

	if (!tlsext_ri_clienthello_needs(ssl)) {
		fprintf(stderr, "FAIL: clienthello should need RI\n");
		failure = 1;
		goto done;
	}

	memcpy(S3I(ssl)->previous_client_finished, tlsext_ri_prev_client,
	    sizeof(tlsext_ri_prev_client));
	S3I(ssl)->previous_client_finished_len = sizeof(tlsext_ri_prev_client);

	S3I(ssl)->renegotiate_seen = 0;

	if (!tlsext_ri_clienthello_build(ssl, &cbb)) {
		fprintf(stderr, "FAIL: clienthello failed to build RI\n");
		failure = 1;
		goto done;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_ri_clienthello)) {
		fprintf(stderr, "FAIL: got clienthello RI with length %zu, "
		    "want length %zu\n", dlen, sizeof(tlsext_ri_clienthello));
		failure = 1;
		goto done;
	}

	if (memcmp(data, tlsext_ri_clienthello, dlen) != 0) {
		fprintf(stderr, "FAIL: clienthello RI differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_ri_clienthello, sizeof(tlsext_ri_clienthello));
		failure = 1;
		goto done;
	}

	CBS_init(&cbs, tlsext_ri_clienthello, sizeof(tlsext_ri_clienthello));
	if (!tlsext_ri_clienthello_parse(ssl, &cbs, &alert)) {
		fprintf(stderr, "FAIL: failed to parse clienthello RI\n");
		failure = 1;
		goto done;
	}

	if (S3I(ssl)->renegotiate_seen != 1) {
		fprintf(stderr, "FAIL: renegotiate seen not set\n");
		failure = 1;
		goto done;
	}
        if (S3I(ssl)->send_connection_binding != 1) {
		fprintf(stderr, "FAIL: send connection binding not set\n");
		failure = 1;
		goto done;
	}

	memset(S3I(ssl)->previous_client_finished, 0,
	    sizeof(S3I(ssl)->previous_client_finished));

	S3I(ssl)->renegotiate_seen = 0;

	CBS_init(&cbs, tlsext_ri_clienthello, sizeof(tlsext_ri_clienthello));
	if (tlsext_ri_clienthello_parse(ssl, &cbs, &alert)) {
		fprintf(stderr, "FAIL: parsed invalid clienthello RI\n");
		failure = 1;
		goto done;
	}

	if (S3I(ssl)->renegotiate_seen == 1) {
		fprintf(stderr, "FAIL: renegotiate seen set\n");
		failure = 1;
		goto done;
	}

 done:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

static int
test_tlsext_ri_serverhello(void)
{
	unsigned char *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int failure = 0;
	size_t dlen;
	int alert;
	CBB cbb;
	CBS cbs;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLS_server_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if (tlsext_ri_serverhello_needs(ssl)) {
		fprintf(stderr, "FAIL: serverhello should not need RI\n");
		failure = 1;
		goto done;
	}

        S3I(ssl)->send_connection_binding = 1;
	
	if (!tlsext_ri_serverhello_needs(ssl)) {
		fprintf(stderr, "FAIL: serverhello should need RI\n");
		failure = 1;
		goto done;
	}

	memcpy(S3I(ssl)->previous_client_finished, tlsext_ri_prev_client,
	    sizeof(tlsext_ri_prev_client));
	S3I(ssl)->previous_client_finished_len = sizeof(tlsext_ri_prev_client);

	memcpy(S3I(ssl)->previous_server_finished, tlsext_ri_prev_server,
	    sizeof(tlsext_ri_prev_server));
	S3I(ssl)->previous_server_finished_len = sizeof(tlsext_ri_prev_server);

	S3I(ssl)->renegotiate_seen = 0;

	if (!tlsext_ri_serverhello_build(ssl, &cbb)) {
		fprintf(stderr, "FAIL: serverhello failed to build RI\n");
		failure = 1;
		goto done;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_ri_serverhello)) {
		fprintf(stderr, "FAIL: got serverhello RI with length %zu, "
		    "want length %zu\n", dlen, sizeof(tlsext_ri_serverhello));
		failure = 1;
		goto done;
	}

	if (memcmp(data, tlsext_ri_serverhello, dlen) != 0) {
		fprintf(stderr, "FAIL: serverhello RI differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_ri_serverhello, sizeof(tlsext_ri_serverhello));
		failure = 1;
		goto done;
	}

	CBS_init(&cbs, tlsext_ri_serverhello, sizeof(tlsext_ri_serverhello));
	if (!tlsext_ri_serverhello_parse(ssl, &cbs, &alert)) {
		fprintf(stderr, "FAIL: failed to parse serverhello RI\n");
		failure = 1;
		goto done;
	}

	if (S3I(ssl)->renegotiate_seen != 1) {
		fprintf(stderr, "FAIL: renegotiate seen not set\n");
		failure = 1;
		goto done;
	}
        if (S3I(ssl)->send_connection_binding != 1) {
		fprintf(stderr, "FAIL: send connection binding not set\n");
		failure = 1;
		goto done;
	}

	memset(S3I(ssl)->previous_client_finished, 0,
	    sizeof(S3I(ssl)->previous_client_finished));
	memset(S3I(ssl)->previous_server_finished, 0,
	    sizeof(S3I(ssl)->previous_server_finished));

	S3I(ssl)->renegotiate_seen = 0;

	CBS_init(&cbs, tlsext_ri_serverhello, sizeof(tlsext_ri_serverhello));
	if (tlsext_ri_serverhello_parse(ssl, &cbs, &alert)) {
		fprintf(stderr, "FAIL: parsed invalid serverhello RI\n");
		failure = 1;
		goto done;
	}

	if (S3I(ssl)->renegotiate_seen == 1) {
		fprintf(stderr, "FAIL: renegotiate seen set\n");
		failure = 1;
		goto done;
	}

 done:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

/*
 * Server Name Indication - RFC 6066, section 3.
 */

#define TEST_SNI_SERVERNAME "www.libressl.org"

static unsigned char tlsext_sni_clienthello[] = {
	0x00, 0x13, 0x00, 0x00, 0x10, 0x77, 0x77, 0x77,
	0x2e, 0x6c, 0x69, 0x62, 0x72, 0x65, 0x73, 0x73,
	0x6c, 0x2e, 0x6f, 0x72, 0x67,
};

static unsigned char tlsext_sni_serverhello[] = {
};

static int
test_tlsext_sni_clienthello(void)
{
	unsigned char *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int failure = 0;
	size_t dlen;
	int alert;
	CBB cbb;
	CBS cbs;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if (tlsext_sni_clienthello_needs(ssl)) {
		fprintf(stderr, "FAIL: clienthello should not need SNI\n");
		failure = 1;
		goto done;
	}

	if (!SSL_set_tlsext_host_name(ssl, TEST_SNI_SERVERNAME)) {
		fprintf(stderr, "FAIL: client failed to set server name\n");
		failure = 1;
		goto done;
	}

	if (!tlsext_sni_clienthello_needs(ssl)) {
		fprintf(stderr, "FAIL: clienthello should need SNI\n");
		failure = 1;
		goto done;
	}

	if (!tlsext_sni_clienthello_build(ssl, &cbb)) {
		fprintf(stderr, "FAIL: clienthello failed to build SNI\n");
		failure = 1;
		goto done;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_sni_clienthello)) {
		fprintf(stderr, "FAIL: got clienthello SNI with length %zu, "
		    "want length %zu\n", dlen, sizeof(tlsext_sni_clienthello));
		failure = 1;
		goto done;
	}

	if (memcmp(data, tlsext_sni_clienthello, dlen) != 0) {
		fprintf(stderr, "FAIL: clienthello SNI differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_sni_clienthello, sizeof(tlsext_sni_clienthello));
		failure = 1;
		goto done;
	}

	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	ssl->internal->hit = 0;

	CBS_init(&cbs, tlsext_sni_clienthello, sizeof(tlsext_sni_clienthello));
	if (!tlsext_sni_clienthello_parse(ssl, &cbs, &alert)) {
		fprintf(stderr, "FAIL: failed to parse clienthello SNI\n");
		failure = 1;
		goto done;
	}

	if (ssl->session->tlsext_hostname == NULL) {
		fprintf(stderr, "FAIL: no tlsext_hostname from clienthello SNI\n");
		failure = 1;
		goto done;
	}

	if (strlen(ssl->session->tlsext_hostname) != strlen(TEST_SNI_SERVERNAME) ||
	    strncmp(ssl->session->tlsext_hostname, TEST_SNI_SERVERNAME,
		strlen(TEST_SNI_SERVERNAME)) != 0) {
		fprintf(stderr, "FAIL: got tlsext_hostname `%s', want `%s'\n",
		    ssl->session->tlsext_hostname, TEST_SNI_SERVERNAME);
		failure = 1;
		goto done;
	}

	ssl->internal->hit = 1;

	if ((ssl->session->tlsext_hostname = strdup("notthesame.libressl.org")) ==
	    NULL)
		errx(1, "failed to strdup tlsext_hostname");

	CBS_init(&cbs, tlsext_sni_clienthello, sizeof(tlsext_sni_clienthello));
	if (tlsext_sni_clienthello_parse(ssl, &cbs, &alert)) {
		fprintf(stderr, "FAIL: parsed clienthello with mismatched SNI\n");
		failure = 1;
		goto done;
	}

 done:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

static int
test_tlsext_sni_serverhello(void)
{
	unsigned char *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int failure = 0;
	size_t dlen;
	int alert;
	CBB cbb;
	CBS cbs;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLS_server_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	if (tlsext_sni_serverhello_needs(ssl)) {
		fprintf(stderr, "FAIL: serverhello should not need SNI\n");
		failure = 1;
		goto done;
	}

	if (!SSL_set_tlsext_host_name(ssl, TEST_SNI_SERVERNAME)) {
		fprintf(stderr, "FAIL: client failed to set server name\n");
		failure = 1;
		goto done;
	}

	if ((ssl->session->tlsext_hostname = strdup(TEST_SNI_SERVERNAME)) ==
	    NULL)
		errx(1, "failed to strdup tlsext_hostname");
	
	if (!tlsext_sni_serverhello_needs(ssl)) {
		fprintf(stderr, "FAIL: serverhello should need SNI\n");
		failure = 1;
		goto done;
	}

	if (!tlsext_sni_serverhello_build(ssl, &cbb)) {
		fprintf(stderr, "FAIL: serverhello failed to build SNI\n");
		failure = 1;
		goto done;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_sni_serverhello)) {
		fprintf(stderr, "FAIL: got serverhello SNI with length %zu, "
		    "want length %zu\n", dlen, sizeof(tlsext_sni_serverhello));
		failure = 1;
		goto done;
	}

	if (memcmp(data, tlsext_sni_serverhello, dlen) != 0) {
		fprintf(stderr, "FAIL: serverhello SNI differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_sni_serverhello, sizeof(tlsext_sni_serverhello));
		failure = 1;
		goto done;
	}

	free(ssl->session->tlsext_hostname);
	ssl->session->tlsext_hostname = NULL;

	CBS_init(&cbs, tlsext_sni_serverhello, sizeof(tlsext_sni_serverhello));
	if (!tlsext_sni_serverhello_parse(ssl, &cbs, &alert)) {
		fprintf(stderr, "FAIL: failed to parse serverhello SNI\n");
		failure = 1;
		goto done;
	}

	if (ssl->session->tlsext_hostname == NULL) {
		fprintf(stderr, "FAIL: no tlsext_hostname after serverhello SNI\n");
		failure = 1;
		goto done;
	}

	if (strlen(ssl->session->tlsext_hostname) != strlen(TEST_SNI_SERVERNAME) ||
	    strncmp(ssl->session->tlsext_hostname, TEST_SNI_SERVERNAME,
		strlen(TEST_SNI_SERVERNAME)) != 0) {
		fprintf(stderr, "FAIL: got tlsext_hostname `%s', want `%s'\n",
		    ssl->session->tlsext_hostname, TEST_SNI_SERVERNAME);
		failure = 1;
		goto done;
	}

 done:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

int
main(int argc, char **argv)
{
	int failed = 0;

	SSL_library_init();

	failed |= test_tlsext_ri_clienthello();
	failed |= test_tlsext_ri_serverhello();

	failed |= test_tlsext_sni_clienthello();
	failed |= test_tlsext_sni_serverhello();

	return (failed);
}
