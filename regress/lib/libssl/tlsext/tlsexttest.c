/* $OpenBSD: tlsexttest.c,v 1.20 2018/11/10 08:10:31 beck Exp $ */
/*
 * Copyright (c) 2017 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2017 Doug Hogan <doug@openbsd.org>
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

static void
hexdump2(const uint16_t *buf, size_t len)
{
	size_t i;

	for (i = 1; i <= len / 2; i++)
		fprintf(stderr, " 0x%04hx,%s", buf[i - 1], i % 8 ? "" : "\n");

	fprintf(stderr, "\n");
}

static void
compare_data(const uint8_t *recv, size_t recv_len, const uint8_t *expect,
    size_t expect_len)
{
	fprintf(stderr, "received:\n");
	hexdump(recv, recv_len);

	fprintf(stderr, "test data:\n");
	hexdump(expect, expect_len);
}

static void
compare_data2(const uint16_t *recv, size_t recv_len, const uint16_t *expect,
    size_t expect_len)
{
	fprintf(stderr, "received:\n");
	hexdump2(recv, recv_len);

	fprintf(stderr, "test data:\n");
	hexdump2(expect, expect_len);
}

#define FAIL(msg, ...)						\
do {								\
	fprintf(stderr, "[%s:%d] FAIL: ", __FILE__, __LINE__);	\
	fprintf(stderr, msg, ##__VA_ARGS__);			\
} while(0)

/*
 * Supported Application-Layer Protocol Negotiation - RFC 7301
 *
 * There are already extensive unit tests for this so this just
 * tests the state info.
 */

const uint8_t tlsext_alpn_multiple_protos_val[] = {
	/* opaque ProtocolName<1..2^8-1> -- 'http/1.1' */
	0x08, /* len */
	0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31,
	/* opaque ProtocolName<1..2^8-1> -- 'stun.nat' */
	0x09, /* len */
	0x73, 0x74, 0x75, 0x6e, 0x2e, 0x74, 0x75, 0x72, 0x6e
};

const uint8_t tlsext_alpn_multiple_protos[] = {
	/* ProtocolName protocol_name_list<2..2^16-1> -- ALPN names */
	0x00, 0x13, /* len of all names */
	/* opaque ProtocolName<1..2^8-1> -- 'http/1.1' */
	0x08, /* len */
	0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31,
	/* opaque ProtocolName<1..2^8-1> -- 'stun.nat' */
	0x09, /* len */
	0x73, 0x74, 0x75, 0x6e, 0x2e, 0x74, 0x75, 0x72, 0x6e
};

const uint8_t tlsext_alpn_single_proto_val[] = {
	/* opaque ProtocolName<1..2^8-1> -- 'http/1.1' */
	0x08, /* len */
	0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31
};

const uint8_t tlsext_alpn_single_proto_name[] = {
	0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31 /* 'http/1.1' */
};

const uint8_t tlsext_alpn_single_proto[] = {
	/* ProtocolName protocol_name_list<2..2^16-1> -- ALPN names */
	0x00, 0x09, /* len of all names */
	/* opaque ProtocolName<1..2^8-1> -- 'http/1.1' */
	0x08, /* len */
	0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31
};

static int
test_tlsext_alpn_clienthello(void)
{
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	uint8_t *data = NULL;
	CBB cbb;
	CBS cbs;
	int failure, alert;
	size_t dlen;

	CBB_init(&cbb, 0);

	failure = 1;

	if ((ssl_ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	/* By default, we don't need this */
	if (tlsext_alpn_clienthello_needs(ssl)) {
		FAIL("clienthello should not need ALPN by default");
		goto err;
	}

	/*
	 * Prereqs:
	 * 1) Set s->internal->alpn_client_proto_list
	 *    - Using SSL_set_alpn_protos()
	 * 2) We have not finished or renegotiated.
	 *    - S3I(s)->tmp.finish_md_len == 0
	 */
	if (SSL_set_alpn_protos(ssl, tlsext_alpn_single_proto_val,
	    sizeof(tlsext_alpn_single_proto_val)) != 0) {
		FAIL("should be able to set ALPN to http/1.1");
		goto err;
	}
	if (!tlsext_alpn_clienthello_needs(ssl)) {
		FAIL("clienthello should need ALPN by now");
		goto err;
	}

	/* Make sure we can build the clienthello with a single proto. */

	if (!tlsext_alpn_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build ALPN\n");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_alpn_single_proto)) {
		FAIL("got clienthello ALPN with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_alpn_single_proto));
		compare_data(data, dlen, tlsext_alpn_single_proto,
		    sizeof(tlsext_alpn_single_proto));
		goto err;
	}
	if (memcmp(data, tlsext_alpn_single_proto, dlen) != 0) {
		FAIL("clienthello ALPN differs:\n");
		compare_data(data, dlen, tlsext_alpn_single_proto,
		    sizeof(tlsext_alpn_single_proto));
		goto err;
	}

	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	/* Make sure we can parse the single proto. */

	CBS_init(&cbs, tlsext_alpn_single_proto,
	    sizeof(tlsext_alpn_single_proto));
	if (!tlsext_alpn_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse ALPN");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (ssl->internal->alpn_client_proto_list_len !=
	    sizeof(tlsext_alpn_single_proto_val)) {
		FAIL("got clienthello ALPN with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_alpn_single_proto_val));
		compare_data(ssl->internal->alpn_client_proto_list,
		    ssl->internal->alpn_client_proto_list_len,
		    tlsext_alpn_single_proto_val,
		    sizeof(tlsext_alpn_single_proto_val));
		goto err;
	}
	if (memcmp(ssl->internal->alpn_client_proto_list,
	    tlsext_alpn_single_proto_val,
	    sizeof(tlsext_alpn_single_proto_val)) != 0) {
		FAIL("clienthello ALPN differs:\n");
		compare_data(data, dlen, tlsext_alpn_single_proto_val,
		    sizeof(tlsext_alpn_single_proto_val));
		goto err;
	}

	/* Make sure we can build the clienthello with multiple entries. */

	if (SSL_set_alpn_protos(ssl, tlsext_alpn_multiple_protos_val,
	    sizeof(tlsext_alpn_multiple_protos_val)) != 0) {
		FAIL("should be able to set ALPN to http/1.1");
		goto err;
	}
	if (!tlsext_alpn_clienthello_needs(ssl)) {
		FAIL("clienthello should need ALPN by now");
		goto err;
	}

	if (!tlsext_alpn_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build ALPN\n");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_alpn_multiple_protos)) {
		FAIL("got clienthello ALPN with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_alpn_multiple_protos));
		compare_data(data, dlen, tlsext_alpn_multiple_protos,
		    sizeof(tlsext_alpn_multiple_protos));
		goto err;
	}
	if (memcmp(data, tlsext_alpn_multiple_protos, dlen) != 0) {
		FAIL("clienthello ALPN differs:\n");
		compare_data(data, dlen, tlsext_alpn_multiple_protos,
		    sizeof(tlsext_alpn_multiple_protos));
		goto err;
	}

	/* Make sure we can parse multiple protos */

	CBS_init(&cbs, tlsext_alpn_multiple_protos,
	    sizeof(tlsext_alpn_multiple_protos));
	if (!tlsext_alpn_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse ALPN");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (ssl->internal->alpn_client_proto_list_len !=
	    sizeof(tlsext_alpn_multiple_protos_val)) {
		FAIL("got clienthello ALPN with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_alpn_multiple_protos_val));
		compare_data(ssl->internal->alpn_client_proto_list,
		    ssl->internal->alpn_client_proto_list_len,
		    tlsext_alpn_multiple_protos_val,
		    sizeof(tlsext_alpn_multiple_protos_val));
		goto err;
	}
	if (memcmp(ssl->internal->alpn_client_proto_list,
	    tlsext_alpn_multiple_protos_val,
	    sizeof(tlsext_alpn_multiple_protos_val)) != 0) {
		FAIL("clienthello ALPN differs:\n");
		compare_data(data, dlen, tlsext_alpn_multiple_protos_val,
		    sizeof(tlsext_alpn_multiple_protos_val));
		goto err;
	}

	/* Make sure we can remove the list and avoid ALPN */

	free(ssl->internal->alpn_client_proto_list);
	ssl->internal->alpn_client_proto_list = NULL;
	ssl->internal->alpn_client_proto_list_len = 0;

	if (tlsext_alpn_clienthello_needs(ssl)) {
		FAIL("clienthello should need ALPN by default");
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

static int
test_tlsext_alpn_serverhello(void)
{
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	uint8_t *data = NULL;
	CBB cbb;
	CBS cbs;
	int failure, alert;
	size_t dlen;

	CBB_init(&cbb, 0);

	failure = 1;

	if ((ssl_ctx = SSL_CTX_new(TLS_server_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	/* By default, ALPN isn't needed. */
	if (tlsext_alpn_serverhello_needs(ssl)) {
		FAIL("serverhello should not need ALPN by default\n");
		goto err;
	}

	/*
	 * The server has a single ALPN selection which is set by
	 * SSL_CTX_set_alpn_select_cb() and calls SSL_select_next_proto().
	 *
	 * This will be a plain name and separate length.
	 */
	if ((S3I(ssl)->alpn_selected = malloc(sizeof(tlsext_alpn_single_proto_name))) == NULL) {
		errx(1, "failed to malloc");
	}
	memcpy(S3I(ssl)->alpn_selected, tlsext_alpn_single_proto_name,
	    sizeof(tlsext_alpn_single_proto_name));
	S3I(ssl)->alpn_selected_len = sizeof(tlsext_alpn_single_proto_name);

	if (!tlsext_alpn_serverhello_needs(ssl)) {
		FAIL("serverhello should need ALPN after a protocol is selected\n");
		goto err;
	}

	/* Make sure we can build a serverhello with one protocol */

	if (!tlsext_alpn_serverhello_build(ssl, &cbb)) {
		FAIL("serverhello should be able to build a response");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_alpn_single_proto)) {
		FAIL("got clienthello ALPN with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_alpn_single_proto));
		compare_data(data, dlen, tlsext_alpn_single_proto,
		    sizeof(tlsext_alpn_single_proto));
		goto err;
	}
	if (memcmp(data, tlsext_alpn_single_proto, dlen) != 0) {
		FAIL("clienthello ALPN differs:\n");
		compare_data(data, dlen, tlsext_alpn_single_proto,
		    sizeof(tlsext_alpn_single_proto));
		goto err;
	}

	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	/* Make sure we can parse the single proto. */

	CBS_init(&cbs, tlsext_alpn_single_proto,
	    sizeof(tlsext_alpn_single_proto));

	/* Shouldn't be able to parse without requesting */
	if (tlsext_alpn_serverhello_parse(ssl, &cbs, &alert)) {
		FAIL("Should only parse serverhello if we requested it");
		goto err;
	}

	/* Should be able to parse once requested. */
	if (SSL_set_alpn_protos(ssl, tlsext_alpn_single_proto_val,
	    sizeof(tlsext_alpn_single_proto_val)) != 0) {
		FAIL("should be able to set ALPN to http/1.1");
		goto err;
	}
	if (!tlsext_alpn_serverhello_parse(ssl, &cbs, &alert)) {
		FAIL("Should be able to parse serverhello when we request it");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (S3I(ssl)->alpn_selected_len !=
	    sizeof(tlsext_alpn_single_proto_name)) {
		FAIL("got serverhello ALPN with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_alpn_single_proto_name));
		compare_data(S3I(ssl)->alpn_selected,
		    S3I(ssl)->alpn_selected_len,
		    tlsext_alpn_single_proto_name,
		    sizeof(tlsext_alpn_single_proto_name));
		goto err;
	}
	if (memcmp(S3I(ssl)->alpn_selected,
	    tlsext_alpn_single_proto_name,
	    sizeof(tlsext_alpn_single_proto_name)) != 0) {
		FAIL("serverhello ALPN differs:\n");
		compare_data(S3I(ssl)->alpn_selected,
		    S3I(ssl)->alpn_selected_len,
		    tlsext_alpn_single_proto_name,
		    sizeof(tlsext_alpn_single_proto_name));
		goto err;
	}

	/*
	 * We should NOT be able to build a serverhello with multiple
	 * protocol names.  However, the existing code did not check for this
	 * case because it is passed in as an encoded value.
	 */

	/* Make sure we can remove the list and avoid ALPN */

	free(S3I(ssl)->alpn_selected);
	S3I(ssl)->alpn_selected = NULL;
	S3I(ssl)->alpn_selected_len = 0;

	if (tlsext_alpn_serverhello_needs(ssl)) {
		FAIL("serverhello should need ALPN by default");
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);

}

/*
 * Supported Elliptic Curves - RFC 4492 section 5.1.1.
 *
 * This extension is only used by the client.
 */

static uint8_t tlsext_supportedgroups_clienthello_default[] = {
	0x00, 0x06,
	0x00, 0x1d,  /* X25519 (29) */
	0x00, 0x17,  /* secp256r1 (23) */
	0x00, 0x18   /* secp384r1 (24) */
};

static uint16_t tlsext_supportedgroups_clienthello_secp384r1_val[] = {
	0x0018   /* tls1_ec_nid2curve_id(NID_secp384r1) */
};
static uint8_t tlsext_supportedgroups_clienthello_secp384r1[] = {
	0x00, 0x02,
	0x00, 0x18  /* secp384r1 (24) */
};

/* Example from RFC 4492 section 5.1.1 */
static uint16_t tlsext_supportedgroups_clienthello_nistp192and224_val[] = {
	0x0013,  /* tls1_ec_nid2curve_id(NID_X9_62_prime192v1) */
	0x0015   /* tls1_ec_nid2curve_id(NID_secp224r1) */
};
static uint8_t tlsext_supportedgroups_clienthello_nistp192and224[] = {
	0x00, 0x04,
	0x00, 0x13, /* secp192r1 aka NIST P-192 */
	0x00, 0x15  /* secp224r1 aka NIST P-224 */
};

static int
test_tlsext_supportedgroups_clienthello(void)
{
	unsigned char *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	size_t dlen;
	int failure, alert;
	CBB cbb;
	CBS cbs;

	failure = 1;

	if (!CBB_init(&cbb, 0))
		errx(1, "failed to create CBB");

	if ((ssl_ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	/*
	 * Default ciphers include EC so we need it by default.
	 */
	if (!tlsext_supportedgroups_clienthello_needs(ssl)) {
		FAIL("clienthello should need Ellipticcurves for default "
		    "ciphers\n");
		goto err;
	}

	/*
	 * Exclude cipher suites so we can test not including it.
	 */
	if (!SSL_set_cipher_list(ssl, "TLSv1.2:!ECDHE:!ECDSA")) {
		FAIL("clienthello should be able to set cipher list\n");
		goto err;
	}
	if (tlsext_supportedgroups_clienthello_needs(ssl)) {
		FAIL("clienthello should not need Ellipticcurves\n");
		goto err;
	}

	/*
	 * Use libtls default for the rest of the testing
	 */
	if (!SSL_set_cipher_list(ssl, "TLSv1.2+AEAD+ECDHE")) {
		FAIL("clienthello should be able to set cipher list\n");
		goto err;
	}
	if (!tlsext_supportedgroups_clienthello_needs(ssl)) {
		FAIL("clienthello should need Ellipticcurves\n");
		goto err;
	}

	/*
	 * Test with a session secp384r1.  The default is used instead.
	 */
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	if ((SSI(ssl)->tlsext_supportedgroups = malloc(sizeof(uint16_t)))
	    == NULL) {
		FAIL("client could not malloc\n");
		goto err;
	}
	SSI(ssl)->tlsext_supportedgroups[0] = tls1_ec_nid2curve_id(NID_secp384r1);
	SSI(ssl)->tlsext_supportedgroups_length = 1;

	if (!tlsext_supportedgroups_clienthello_needs(ssl)) {
		FAIL("clienthello should need Ellipticcurves\n");
		goto err;
	}

	if (!tlsext_supportedgroups_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build Ellipticcurves\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_supportedgroups_clienthello_default)) {
		FAIL("got clienthello Ellipticcurves with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_supportedgroups_clienthello_default));
		compare_data(data, dlen, tlsext_supportedgroups_clienthello_default,
		    sizeof(tlsext_supportedgroups_clienthello_default));
		goto err;
	}

	if (memcmp(data, tlsext_supportedgroups_clienthello_default, dlen) != 0) {
		FAIL("clienthello Ellipticcurves differs:\n");
		compare_data(data, dlen, tlsext_supportedgroups_clienthello_default,
		    sizeof(tlsext_supportedgroups_clienthello_default));
		goto err;
	}

	/*
	 * Test parsing secp384r1
	 */
	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	SSL_SESSION_free(ssl->session);
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	CBS_init(&cbs, tlsext_supportedgroups_clienthello_secp384r1,
	    sizeof(tlsext_supportedgroups_clienthello_secp384r1));
	if (!tlsext_supportedgroups_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse clienthello Ellipticcurves\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (SSI(ssl)->tlsext_supportedgroups_length !=
	    sizeof(tlsext_supportedgroups_clienthello_secp384r1_val) / sizeof(uint16_t)) {
		FAIL("no tlsext_ellipticcurves from clienthello "
		    "Ellipticcurves\n");
		goto err;
	}

	if (memcmp(SSI(ssl)->tlsext_supportedgroups,
	    tlsext_supportedgroups_clienthello_secp384r1_val,
	    sizeof(tlsext_supportedgroups_clienthello_secp384r1_val)) != 0) {
		FAIL("clienthello had an incorrect Ellipticcurves "
		    "entry\n");
		compare_data2(SSI(ssl)->tlsext_supportedgroups,
		    SSI(ssl)->tlsext_supportedgroups_length * 2,
		    tlsext_supportedgroups_clienthello_secp384r1_val,
		    sizeof(tlsext_supportedgroups_clienthello_secp384r1_val));
		goto err;
	}

	/*
	 * Use a custom order.
	 */
	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);

	SSL_SESSION_free(ssl->session);
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	if ((ssl->internal->tlsext_supportedgroups = malloc(sizeof(uint16_t) * 2)) == NULL) {
		FAIL("client could not malloc\n");
		goto err;
	}
	ssl->internal->tlsext_supportedgroups[0] = tls1_ec_nid2curve_id(NID_X9_62_prime192v1);
	ssl->internal->tlsext_supportedgroups[1] = tls1_ec_nid2curve_id(NID_secp224r1);
	ssl->internal->tlsext_supportedgroups_length = 2;

	if (!tlsext_supportedgroups_clienthello_needs(ssl)) {
		FAIL("clienthello should need Ellipticcurves\n");
		goto err;
	}

	if (!tlsext_supportedgroups_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build Ellipticcurves\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_supportedgroups_clienthello_nistp192and224)) {
		FAIL("got clienthello Ellipticcurves with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_supportedgroups_clienthello_nistp192and224));
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_supportedgroups_clienthello_nistp192and224,
		    sizeof(tlsext_supportedgroups_clienthello_nistp192and224));
		goto err;
	}

	if (memcmp(data, tlsext_supportedgroups_clienthello_nistp192and224, dlen) != 0) {
		FAIL("clienthello Ellipticcurves differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_supportedgroups_clienthello_nistp192and224,
		    sizeof(tlsext_supportedgroups_clienthello_nistp192and224));
		goto err;
	}

	/*
	 * Parse non-default curves to session.
	 */
	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	SSL_SESSION_free(ssl->session);
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	/* Reset back to the default list. */
	free(ssl->internal->tlsext_supportedgroups);
	ssl->internal->tlsext_supportedgroups = NULL;
	ssl->internal->tlsext_supportedgroups_length = 0;

	CBS_init(&cbs, tlsext_supportedgroups_clienthello_nistp192and224,
	    sizeof(tlsext_supportedgroups_clienthello_nistp192and224));
	if (!tlsext_supportedgroups_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse clienthello Ellipticcurves\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (SSI(ssl)->tlsext_supportedgroups_length !=
	    sizeof(tlsext_supportedgroups_clienthello_nistp192and224_val) / sizeof(uint16_t)) {
		FAIL("no tlsext_ellipticcurves from clienthello "
		    "Ellipticcurves\n");
		goto err;
	}

	if (memcmp(SSI(ssl)->tlsext_supportedgroups,
	    tlsext_supportedgroups_clienthello_nistp192and224_val,
	    sizeof(tlsext_supportedgroups_clienthello_nistp192and224_val)) != 0) {
		FAIL("clienthello had an incorrect Ellipticcurves entry\n");
		compare_data2(SSI(ssl)->tlsext_supportedgroups,
		    SSI(ssl)->tlsext_supportedgroups_length * 2,
		    tlsext_supportedgroups_clienthello_nistp192and224_val,
		    sizeof(tlsext_supportedgroups_clienthello_nistp192and224_val));
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}


/* elliptic_curves is only used by the client so this doesn't test much. */
static int
test_tlsext_supportedgroups_serverhello(void)
{
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int failure;

	failure = 1;

	if ((ssl_ctx = SSL_CTX_new(TLS_server_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if (tlsext_supportedgroups_serverhello_needs(ssl)) {
		FAIL("serverhello should not need elliptic_curves\n");
		goto err;
	}

	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	if (tlsext_supportedgroups_serverhello_needs(ssl)) {
		FAIL("serverhello should not need elliptic_curves\n");
		goto err;
	}

	failure = 0;

 err:
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);

	return (failure);

}

/*
 * Supported Point Formats - RFC 4492 section 5.1.2.
 *
 * Examples are from the RFC.  Both client and server have the same build and
 * parse but the needs differ.
 */

static uint8_t tlsext_ecpf_hello_uncompressed_val[] = {
	TLSEXT_ECPOINTFORMAT_uncompressed
};
static uint8_t tlsext_ecpf_hello_uncompressed[] = {
	0x01,
	0x00 /* TLSEXT_ECPOINTFORMAT_uncompressed */
};

static uint8_t tlsext_ecpf_hello_prime[] = {
	0x01,
	0x01 /* TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime */
};

static uint8_t tlsext_ecpf_hello_prefer_order_val[] = {
	TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime,
	TLSEXT_ECPOINTFORMAT_uncompressed,
	TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2
};
static uint8_t tlsext_ecpf_hello_prefer_order[] = {
	0x03,
	0x01, /* TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime */
	0x00, /* TLSEXT_ECPOINTFORMAT_uncompressed */
	0x02  /* TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2 */
};

static int
test_tlsext_ecpf_clienthello(void)
{
	uint8_t *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	size_t dlen;
	int failure, alert;
	CBB cbb;
	CBS cbs;

	failure = 1;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	/*
	 * Default ciphers include EC so we need it by default.
	 */
	if (!tlsext_ecpf_clienthello_needs(ssl)) {
		FAIL("clienthello should need ECPointFormats for default "
		    "ciphers\n");
		goto err;
	}

	/*
	 * Exclude EC cipher suites so we can test not including it.
	 */
	if (!SSL_set_cipher_list(ssl, "ALL:!ECDHE:!ECDH")) {
		FAIL("clienthello should be able to set cipher list\n");
		goto err;
	}
	if (tlsext_ecpf_clienthello_needs(ssl)) {
		FAIL("clienthello should not need ECPointFormats\n");
		goto err;
	}

	/*
	 * Use libtls default for the rest of the testing
	 */
	if (!SSL_set_cipher_list(ssl, "TLSv1.2+AEAD+ECDHE")) {
		FAIL("clienthello should be able to set cipher list\n");
		goto err;
	}
	if (!tlsext_ecpf_clienthello_needs(ssl)) {
		FAIL("clienthello should need ECPointFormats\n");
		goto err;
	}

	/*
	 * The default ECPointFormats should only have uncompressed
	 */
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	if (!tlsext_ecpf_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build ECPointFormats\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_ecpf_hello_uncompressed)) {
		FAIL("got clienthello ECPointFormats with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_ecpf_hello_uncompressed));
		compare_data(data, dlen, tlsext_ecpf_hello_uncompressed,
		    sizeof(tlsext_ecpf_hello_uncompressed));
		goto err;
	}

	if (memcmp(data, tlsext_ecpf_hello_uncompressed, dlen) != 0) {
		FAIL("clienthello ECPointFormats differs:\n");
		compare_data(data, dlen, tlsext_ecpf_hello_uncompressed,
		    sizeof(tlsext_ecpf_hello_uncompressed));
		goto err;
	}

	/*
	 * Make sure we can parse the default.
	 */
	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	SSL_SESSION_free(ssl->session);
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	CBS_init(&cbs, tlsext_ecpf_hello_uncompressed,
	    sizeof(tlsext_ecpf_hello_uncompressed));
	if (!tlsext_ecpf_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse clienthello ECPointFormats\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (SSI(ssl)->tlsext_ecpointformatlist_length !=
	    sizeof(tlsext_ecpf_hello_uncompressed_val)) {
		FAIL("no tlsext_ecpointformats from clienthello "
		    "ECPointFormats\n");
		goto err;
	}

	if (memcmp(SSI(ssl)->tlsext_ecpointformatlist,
	    tlsext_ecpf_hello_uncompressed_val,
	    sizeof(tlsext_ecpf_hello_uncompressed_val)) != 0) {
		FAIL("clienthello had an incorrect ECPointFormats entry\n");
		goto err;
	}

	/*
	 * Test with a custom order.
	 */
	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	SSL_SESSION_free(ssl->session);
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	if ((ssl->internal->tlsext_ecpointformatlist = malloc(sizeof(uint8_t) * 3)) == NULL) {
		FAIL("client could not malloc\n");
		goto err;
	}
	ssl->internal->tlsext_ecpointformatlist[0] = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime;
	ssl->internal->tlsext_ecpointformatlist[1] = TLSEXT_ECPOINTFORMAT_uncompressed;
	ssl->internal->tlsext_ecpointformatlist[2] = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2;
	ssl->internal->tlsext_ecpointformatlist_length = 3;

	if (!tlsext_ecpf_clienthello_needs(ssl)) {
		FAIL("clienthello should need ECPointFormats with a custom "
		    "format\n");
		goto err;
	}

	if (!tlsext_ecpf_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build ECPointFormats\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_ecpf_hello_prefer_order)) {
		FAIL("got clienthello ECPointFormats with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_ecpf_hello_prefer_order));
		compare_data(data, dlen, tlsext_ecpf_hello_prefer_order,
		    sizeof(tlsext_ecpf_hello_prefer_order));
		goto err;
	}

	if (memcmp(data, tlsext_ecpf_hello_prefer_order, dlen) != 0) {
		FAIL("clienthello ECPointFormats differs:\n");
		compare_data(data, dlen, tlsext_ecpf_hello_prefer_order,
		    sizeof(tlsext_ecpf_hello_prefer_order));
		goto err;
	}

	/*
	 * Make sure that we can parse this custom order.
	 */
	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	SSL_SESSION_free(ssl->session);
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	/* Reset the custom list so we go back to the default uncompressed. */
	free(ssl->internal->tlsext_ecpointformatlist);
	ssl->internal->tlsext_ecpointformatlist = NULL;
	ssl->internal->tlsext_ecpointformatlist_length = 0;

	CBS_init(&cbs, tlsext_ecpf_hello_prefer_order,
	    sizeof(tlsext_ecpf_hello_prefer_order));
	if (!tlsext_ecpf_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse clienthello ECPointFormats\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (SSI(ssl)->tlsext_ecpointformatlist_length !=
	    sizeof(tlsext_ecpf_hello_prefer_order_val)) {
		FAIL("no tlsext_ecpointformats from clienthello "
		    "ECPointFormats\n");
		goto err;
	}

	if (memcmp(SSI(ssl)->tlsext_ecpointformatlist,
	    tlsext_ecpf_hello_prefer_order_val,
	    sizeof(tlsext_ecpf_hello_prefer_order_val)) != 0) {
		FAIL("clienthello had an incorrect ECPointFormats entry\n");
		goto err;
	}


	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

static int
test_tlsext_ecpf_serverhello(void)
{
	uint8_t *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	size_t dlen;
	int failure, alert;
	CBB cbb;
	CBS cbs;

	failure = 1;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLS_server_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	/* Setup the state so we can call needs. */
	if ((S3I(ssl)->hs.new_cipher =
	    ssl3_get_cipher_by_id(TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305))
	    == NULL) {
		FAIL("serverhello cannot find cipher\n");
		goto err;
	}
	if ((SSI(ssl)->tlsext_ecpointformatlist = malloc(sizeof(uint8_t)))
	    == NULL) {
		FAIL("server could not malloc\n");
		goto err;
	}
	SSI(ssl)->tlsext_ecpointformatlist[0] = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime;
	SSI(ssl)->tlsext_ecpointformatlist_length = 1;

	if (!tlsext_ecpf_serverhello_needs(ssl)) {
		FAIL("serverhello should need ECPointFormats now\n");
		goto err;
	}

	/*
	 * The server will ignore the session list and use either a custom
	 * list or the default (uncompressed).
	 */
	if (!tlsext_ecpf_serverhello_build(ssl, &cbb)) {
		FAIL("serverhello failed to build ECPointFormats\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_ecpf_hello_uncompressed)) {
		FAIL("got serverhello ECPointFormats with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_ecpf_hello_uncompressed));
		compare_data(data, dlen, tlsext_ecpf_hello_uncompressed,
		    sizeof(tlsext_ecpf_hello_uncompressed));
		goto err;
	}

	if (memcmp(data, tlsext_ecpf_hello_uncompressed, dlen) != 0) {
		FAIL("serverhello ECPointFormats differs:\n");
		compare_data(data, dlen, tlsext_ecpf_hello_uncompressed,
		    sizeof(tlsext_ecpf_hello_uncompressed));
		goto err;
	}

	/*
	 * Cannot parse a non-default list without at least uncompressed.
	 */
	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	SSL_SESSION_free(ssl->session);
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	CBS_init(&cbs, tlsext_ecpf_hello_prime,
	    sizeof(tlsext_ecpf_hello_prime));
	if (tlsext_ecpf_serverhello_parse(ssl, &cbs, &alert)) {
		FAIL("must include uncompressed in serverhello ECPointFormats\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	/*
	 * Test with a custom order that replaces the default uncompressed.
	 */
	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	SSL_SESSION_free(ssl->session);
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	/* Add a session list even though it will be ignored. */
	if ((SSI(ssl)->tlsext_ecpointformatlist = malloc(sizeof(uint8_t)))
	    == NULL) {
		FAIL("server could not malloc\n");
		goto err;
	}
	SSI(ssl)->tlsext_ecpointformatlist[0] = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2;
	SSI(ssl)->tlsext_ecpointformatlist_length = 1;

	/* Replace the default list with a custom one. */
	if ((ssl->internal->tlsext_ecpointformatlist = malloc(sizeof(uint8_t) * 3)) == NULL) {
		FAIL("server could not malloc\n");
		goto err;
	}
	ssl->internal->tlsext_ecpointformatlist[0] = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime;
	ssl->internal->tlsext_ecpointformatlist[1] = TLSEXT_ECPOINTFORMAT_uncompressed;
	ssl->internal->tlsext_ecpointformatlist[2] = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2;
	ssl->internal->tlsext_ecpointformatlist_length = 3;

	if (!tlsext_ecpf_serverhello_needs(ssl)) {
		FAIL("serverhello should need ECPointFormats\n");
		goto err;
	}

	if (!tlsext_ecpf_serverhello_build(ssl, &cbb)) {
		FAIL("serverhello failed to build ECPointFormats\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_ecpf_hello_prefer_order)) {
		FAIL("got serverhello ECPointFormats with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_ecpf_hello_prefer_order));
		compare_data(data, dlen, tlsext_ecpf_hello_prefer_order,
		    sizeof(tlsext_ecpf_hello_prefer_order));
		goto err;
	}

	if (memcmp(data, tlsext_ecpf_hello_prefer_order, dlen) != 0) {
		FAIL("serverhello ECPointFormats differs:\n");
		compare_data(data, dlen, tlsext_ecpf_hello_prefer_order,
		    sizeof(tlsext_ecpf_hello_prefer_order));
		goto err;
	}

	/*
	 * Should be able to parse the custom list into a session list.
	 */
	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	SSL_SESSION_free(ssl->session);
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	/* Reset back to the default (uncompressed) */
	free(ssl->internal->tlsext_ecpointformatlist);
	ssl->internal->tlsext_ecpointformatlist = NULL;
	ssl->internal->tlsext_ecpointformatlist_length = 0;

	CBS_init(&cbs, tlsext_ecpf_hello_prefer_order,
	    sizeof(tlsext_ecpf_hello_prefer_order));
	if (!tlsext_ecpf_serverhello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse serverhello ECPointFormats\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (SSI(ssl)->tlsext_ecpointformatlist_length !=
	    sizeof(tlsext_ecpf_hello_prefer_order_val)) {
		FAIL("no tlsext_ecpointformats from serverhello "
		    "ECPointFormats\n");
		goto err;
	}

	if (memcmp(SSI(ssl)->tlsext_ecpointformatlist,
	    tlsext_ecpf_hello_prefer_order_val,
	    sizeof(tlsext_ecpf_hello_prefer_order_val)) != 0) {
		FAIL("serverhello had an incorrect ECPointFormats entry\n");
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
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
	int failure;
	size_t dlen;
	int alert;
	CBB cbb;
	CBS cbs;

	failure = 1;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLSv1_2_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if (tlsext_ri_clienthello_needs(ssl)) {
		FAIL("clienthello should not need RI\n");
		goto err;
	}

	if (!SSL_renegotiate(ssl)) {
		FAIL("client failed to set renegotiate\n");
		goto err;
	}

	if (!tlsext_ri_clienthello_needs(ssl)) {
		FAIL("clienthello should need RI\n");
		goto err;
	}

	memcpy(S3I(ssl)->previous_client_finished, tlsext_ri_prev_client,
	    sizeof(tlsext_ri_prev_client));
	S3I(ssl)->previous_client_finished_len = sizeof(tlsext_ri_prev_client);

	S3I(ssl)->renegotiate_seen = 0;

	if (!tlsext_ri_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build RI\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_ri_clienthello)) {
		FAIL("got clienthello RI with length %zu, "
		    "want length %zu\n", dlen, sizeof(tlsext_ri_clienthello));
		goto err;
	}

	if (memcmp(data, tlsext_ri_clienthello, dlen) != 0) {
		FAIL("clienthello RI differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_ri_clienthello, sizeof(tlsext_ri_clienthello));
		goto err;
	}

	CBS_init(&cbs, tlsext_ri_clienthello, sizeof(tlsext_ri_clienthello));
	if (!tlsext_ri_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse clienthello RI\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (S3I(ssl)->renegotiate_seen != 1) {
		FAIL("renegotiate seen not set\n");
		goto err;
	}
	if (S3I(ssl)->send_connection_binding != 1) {
		FAIL("send connection binding not set\n");
		goto err;
	}

	memset(S3I(ssl)->previous_client_finished, 0,
	    sizeof(S3I(ssl)->previous_client_finished));

	S3I(ssl)->renegotiate_seen = 0;

	CBS_init(&cbs, tlsext_ri_clienthello, sizeof(tlsext_ri_clienthello));
	if (tlsext_ri_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("parsed invalid clienthello RI\n");
		failure = 1;
		goto err;
	}

	if (S3I(ssl)->renegotiate_seen == 1) {
		FAIL("renegotiate seen set\n");
		goto err;
	}

	failure = 0;

 err:
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
	int failure;
	size_t dlen;
	int alert;
	CBB cbb;
	CBS cbs;

	failure = 1;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLS_server_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if (tlsext_ri_serverhello_needs(ssl)) {
		FAIL("serverhello should not need RI\n");
		goto err;
	}

	S3I(ssl)->send_connection_binding = 1;

	if (!tlsext_ri_serverhello_needs(ssl)) {
		FAIL("serverhello should need RI\n");
		goto err;
	}

	memcpy(S3I(ssl)->previous_client_finished, tlsext_ri_prev_client,
	    sizeof(tlsext_ri_prev_client));
	S3I(ssl)->previous_client_finished_len = sizeof(tlsext_ri_prev_client);

	memcpy(S3I(ssl)->previous_server_finished, tlsext_ri_prev_server,
	    sizeof(tlsext_ri_prev_server));
	S3I(ssl)->previous_server_finished_len = sizeof(tlsext_ri_prev_server);

	S3I(ssl)->renegotiate_seen = 0;

	if (!tlsext_ri_serverhello_build(ssl, &cbb)) {
		FAIL("serverhello failed to build RI\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_ri_serverhello)) {
		FAIL("got serverhello RI with length %zu, "
		    "want length %zu\n", dlen, sizeof(tlsext_ri_serverhello));
		goto err;
	}

	if (memcmp(data, tlsext_ri_serverhello, dlen) != 0) {
		FAIL("serverhello RI differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_ri_serverhello, sizeof(tlsext_ri_serverhello));
		goto err;
	}

	CBS_init(&cbs, tlsext_ri_serverhello, sizeof(tlsext_ri_serverhello));
	if (!tlsext_ri_serverhello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse serverhello RI\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (S3I(ssl)->renegotiate_seen != 1) {
		FAIL("renegotiate seen not set\n");
		goto err;
	}
	if (S3I(ssl)->send_connection_binding != 1) {
		FAIL("send connection binding not set\n");
		goto err;
	}

	memset(S3I(ssl)->previous_client_finished, 0,
	    sizeof(S3I(ssl)->previous_client_finished));
	memset(S3I(ssl)->previous_server_finished, 0,
	    sizeof(S3I(ssl)->previous_server_finished));

	S3I(ssl)->renegotiate_seen = 0;

	CBS_init(&cbs, tlsext_ri_serverhello, sizeof(tlsext_ri_serverhello));
	if (tlsext_ri_serverhello_parse(ssl, &cbs, &alert)) {
		FAIL("parsed invalid serverhello RI\n");
		goto err;
	}

	if (S3I(ssl)->renegotiate_seen == 1) {
		FAIL("renegotiate seen set\n");
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

/*
 * Signature Algorithms - RFC 5246 section 7.4.1.4.1.
 */

static unsigned char tlsext_sigalgs_clienthello[] = {
	0x00, 0x1a, 0x06, 0x01, 0x06, 0x03, 0xef, 0xef,
	0x05, 0x01, 0x05, 0x03, 0x04, 0x01, 0x04, 0x03,
	0xee, 0xee, 0xed, 0xed, 0x03, 0x01, 0x03, 0x03,
	0x02, 0x01, 0x02, 0x03,
};

static int
test_tlsext_sigalgs_clienthello(void)
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

	ssl->client_version = TLS1_1_VERSION;

	if (tlsext_sigalgs_clienthello_needs(ssl)) {
		fprintf(stderr, "FAIL: clienthello should not need sigalgs\n");
		failure = 1;
		goto done;
	}

	ssl->client_version = TLS1_2_VERSION;

	if (!tlsext_sigalgs_clienthello_needs(ssl)) {
		fprintf(stderr, "FAIL: clienthello should need sigalgs\n");
		failure = 1;
		goto done;
	}

	if (!tlsext_sigalgs_clienthello_build(ssl, &cbb)) {
		fprintf(stderr, "FAIL: clienthello failed to build sigalgs\n");
		failure = 1;
		goto done;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_sigalgs_clienthello)) {
		fprintf(stderr, "FAIL: got clienthello sigalgs with length %zu, "
		    "want length %zu\n", dlen, sizeof(tlsext_sigalgs_clienthello));
		failure = 1;
		goto done;
	}

	if (memcmp(data, tlsext_sigalgs_clienthello, dlen) != 0) {
		fprintf(stderr, "FAIL: clienthello SNI differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_sigalgs_clienthello, sizeof(tlsext_sigalgs_clienthello));
		failure = 1;
		goto done;
	}

	CBS_init(&cbs, tlsext_sigalgs_clienthello, sizeof(tlsext_sigalgs_clienthello));
	if (!tlsext_sigalgs_clienthello_parse(ssl, &cbs, &alert)) {
		fprintf(stderr, "FAIL: failed to parse clienthello SNI\n");
		failure = 1;
		goto done;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto done;
	}

	if (ssl->cert->pkeys[SSL_PKEY_RSA_SIGN].sigalg->md() != EVP_sha512()) {
		fprintf(stderr, "FAIL: RSA sign digest mismatch\n");
		failure = 1;
		goto done;
	}
	if (ssl->cert->pkeys[SSL_PKEY_RSA_ENC].sigalg->md() != EVP_sha512()) {
		fprintf(stderr, "FAIL: RSA enc digest mismatch\n");
		failure = 1;
		goto done;
	}
	if (ssl->cert->pkeys[SSL_PKEY_ECC].sigalg->md() != EVP_sha512()) {
		fprintf(stderr, "FAIL: ECC digest mismatch\n");
		failure = 1;
		goto done;
	}
	if (ssl->cert->pkeys[SSL_PKEY_GOST01].sigalg->md() != EVP_streebog512()) {
		fprintf(stderr, "FAIL: GOST01 digest mismatch\n");
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
test_tlsext_sigalgs_serverhello(void)
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

	if (tlsext_sigalgs_serverhello_needs(ssl)) {
		fprintf(stderr, "FAIL: serverhello should not need sigalgs\n");
		failure = 1;
		goto done;
	}

	if (tlsext_sigalgs_serverhello_build(ssl, &cbb)) {
		fprintf(stderr, "FAIL: serverhello should not build sigalgs\n");
		failure = 1;
		goto done;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	CBS_init(&cbs, tlsext_sigalgs_clienthello, sizeof(tlsext_sigalgs_clienthello));
	if (tlsext_sigalgs_serverhello_parse(ssl, &cbs, &alert)) {
		fprintf(stderr, "FAIL: serverhello should not parse sigalgs\n");
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
 * Server Name Indication - RFC 6066 section 3.
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
	int failure;
	size_t dlen;
	int alert;
	CBB cbb;
	CBS cbs;

	failure = 1;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if (tlsext_sni_clienthello_needs(ssl)) {
		FAIL("clienthello should not need SNI\n");
		goto err;
	}

	if (!SSL_set_tlsext_host_name(ssl, TEST_SNI_SERVERNAME)) {
		FAIL("client failed to set server name\n");
		goto err;
	}

	if (!tlsext_sni_clienthello_needs(ssl)) {
		FAIL("clienthello should need SNI\n");
		goto err;
	}

	if (!tlsext_sni_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build SNI\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_sni_clienthello)) {
		FAIL("got clienthello SNI with length %zu, "
		    "want length %zu\n", dlen, sizeof(tlsext_sni_clienthello));
		goto err;
	}

	if (memcmp(data, tlsext_sni_clienthello, dlen) != 0) {
		FAIL("clienthello SNI differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_sni_clienthello, sizeof(tlsext_sni_clienthello));
		goto err;
	}

	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	ssl->internal->hit = 0;

	CBS_init(&cbs, tlsext_sni_clienthello, sizeof(tlsext_sni_clienthello));
	if (!tlsext_sni_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse clienthello SNI\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (ssl->session->tlsext_hostname == NULL) {
		FAIL("no tlsext_hostname from clienthello SNI\n");
		goto err;
	}

	if (strlen(ssl->session->tlsext_hostname) != strlen(TEST_SNI_SERVERNAME) ||
	    strncmp(ssl->session->tlsext_hostname, TEST_SNI_SERVERNAME,
		strlen(TEST_SNI_SERVERNAME)) != 0) {
		FAIL("got tlsext_hostname `%s', want `%s'\n",
		    ssl->session->tlsext_hostname, TEST_SNI_SERVERNAME);
		goto err;
	}

	ssl->internal->hit = 1;

	if ((ssl->session->tlsext_hostname = strdup("notthesame.libressl.org")) ==
	    NULL)
		errx(1, "failed to strdup tlsext_hostname");

	CBS_init(&cbs, tlsext_sni_clienthello, sizeof(tlsext_sni_clienthello));
	if (tlsext_sni_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("parsed clienthello with mismatched SNI\n");
		goto err;
	}

	failure = 0;

 err:
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
	int failure;
	size_t dlen;
	int alert;
	CBB cbb;
	CBS cbs;

	failure = 1;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLS_server_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	if (tlsext_sni_serverhello_needs(ssl)) {
		FAIL("serverhello should not need SNI\n");
		goto err;
	}

	if (!SSL_set_tlsext_host_name(ssl, TEST_SNI_SERVERNAME)) {
		FAIL("client failed to set server name\n");
		goto err;
	}

	if ((ssl->session->tlsext_hostname = strdup(TEST_SNI_SERVERNAME)) ==
	    NULL)
		errx(1, "failed to strdup tlsext_hostname");

	if (!tlsext_sni_serverhello_needs(ssl)) {
		FAIL("serverhello should need SNI\n");
		goto err;
	}

	if (!tlsext_sni_serverhello_build(ssl, &cbb)) {
		FAIL("serverhello failed to build SNI\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_sni_serverhello)) {
		FAIL("got serverhello SNI with length %zu, "
		    "want length %zu\n", dlen, sizeof(tlsext_sni_serverhello));
		goto err;
	}

	if (memcmp(data, tlsext_sni_serverhello, dlen) != 0) {
		FAIL("serverhello SNI differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tlsext_sni_serverhello, sizeof(tlsext_sni_serverhello));
		goto err;
	}

	free(ssl->session->tlsext_hostname);
	ssl->session->tlsext_hostname = NULL;

	CBS_init(&cbs, tlsext_sni_serverhello, sizeof(tlsext_sni_serverhello));
	if (!tlsext_sni_serverhello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse serverhello SNI\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if (ssl->session->tlsext_hostname == NULL) {
		FAIL("no tlsext_hostname after serverhello SNI\n");
		goto err;
	}

	if (strlen(ssl->session->tlsext_hostname) != strlen(TEST_SNI_SERVERNAME) ||
	    strncmp(ssl->session->tlsext_hostname, TEST_SNI_SERVERNAME,
		strlen(TEST_SNI_SERVERNAME)) != 0) {
		FAIL("got tlsext_hostname `%s', want `%s'\n",
		    ssl->session->tlsext_hostname, TEST_SNI_SERVERNAME);
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

static unsigned char tls_ocsp_clienthello_default[] = {
	0x01, 0x00, 0x00, 0x00, 0x00
};

static int
test_tlsext_ocsp_clienthello(void)
{
	unsigned char *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	size_t dlen;
	int failure;
	int alert;
	CBB cbb;
	CBS cbs;

	failure = 1;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if (tlsext_ocsp_clienthello_needs(ssl)) {
		FAIL("clienthello should not need ocsp\n");
		goto err;
	}
	SSL_set_tlsext_status_type(ssl, TLSEXT_STATUSTYPE_ocsp);

	if (!tlsext_ocsp_clienthello_needs(ssl)) {
		FAIL("clienthello should need ocsp\n");
		goto err;
	}
	if (!tlsext_ocsp_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build SNI\n");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tls_ocsp_clienthello_default)) {
		FAIL("got ocsp clienthello with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tls_ocsp_clienthello_default));
		goto err;
	}
	if (memcmp(data, tls_ocsp_clienthello_default, dlen) != 0) {
		FAIL("ocsp clienthello differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(data, dlen);
		fprintf(stderr, "test data:\n");
		hexdump(tls_ocsp_clienthello_default,
		    sizeof(tls_ocsp_clienthello_default));
		goto err;
	}
	CBS_init(&cbs, tls_ocsp_clienthello_default,
	    sizeof(tls_ocsp_clienthello_default));
	if (!tlsext_ocsp_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse ocsp clienthello\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

static int
test_tlsext_ocsp_serverhello(void)
{
	unsigned char *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	size_t dlen;
	int failure;
	CBB cbb;

	failure = 1;

	CBB_init(&cbb, 0);

	if ((ssl_ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if (tlsext_ocsp_serverhello_needs(ssl)) {
		FAIL("serverhello should not need ocsp\n");
		goto err;
	}

	ssl->internal->tlsext_status_expected = 1;

	if (!tlsext_ocsp_serverhello_needs(ssl)) {
		FAIL("serverhello should need ocsp\n");
		goto err;
	}
	if (!tlsext_ocsp_serverhello_build(ssl, &cbb)) {
		FAIL("serverhello failed to build ocsp\n");
		goto err;
	}

	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

/*
 * Session ticket - RFC 5077 since no known implementations use 4507.
 *
 * Session tickets can be length 0 (special case) to 2^16-1.
 *
 * The state is encrypted by the server so it is opaque to the client.
 */
static uint8_t tlsext_sessionticket_hello_min[1];
static uint8_t tlsext_sessionticket_hello_max[65535];

static int
test_tlsext_sessionticket_clienthello(void)
{
	unsigned char *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int failure;
	CBB cbb;
	size_t dlen;
	uint8_t dummy[1234];

	failure = 1;

	CBB_init(&cbb, 0);

	/* Create fake session tickets with random data. */
	arc4random_buf(tlsext_sessionticket_hello_min,
	    sizeof(tlsext_sessionticket_hello_min));
	arc4random_buf(tlsext_sessionticket_hello_max,
	    sizeof(tlsext_sessionticket_hello_max));

	if ((ssl_ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	/* Should need a ticket by default. */
	if (!tlsext_sessionticket_clienthello_needs(ssl)) {
		FAIL("clienthello should need Sessionticket for default "
		    "ciphers\n");
		goto err;
	}

	/* Test disabling tickets. */
	if ((SSL_set_options(ssl, SSL_OP_NO_TICKET) & SSL_OP_NO_TICKET) == 0) {
		FAIL("Cannot disable tickets in the TLS connection");
		return 0;
	}
	if (tlsext_sessionticket_clienthello_needs(ssl)) {
		FAIL("clienthello should not need SessionTicket if it was disabled");
		goto err;
	}

	/* Test re-enabling tickets. */
	if ((SSL_clear_options(ssl, SSL_OP_NO_TICKET) & SSL_OP_NO_TICKET) != 0) {
		FAIL("Cannot re-enable tickets in the TLS connection");
		return 0;
	}
	if (!tlsext_sessionticket_clienthello_needs(ssl)) {
		FAIL("clienthello should need SessionTicket if it was disabled");
		goto err;
	}

	/* Since we don't have a session, we should build an empty ticket. */
	if (!tlsext_sessionticket_clienthello_build(ssl, &cbb)) {
		FAIL("Cannot build a ticket");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen)) {
		FAIL("Cannot finish CBB");
		goto err;
	}
	if (dlen != 0) {
		FAIL("Expected 0 length but found %zu\n", dlen);
		goto err;
	}

	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	/* With a new session (but no ticket), we should still have 0 length */
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");
	if (!tlsext_sessionticket_clienthello_needs(ssl)) {
		FAIL("Should still want a session ticket with a new session");
		goto err;
	}
	if (!tlsext_sessionticket_clienthello_build(ssl, &cbb)) {
		FAIL("Cannot build a ticket");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen)) {
		FAIL("Cannot finish CBB");
		goto err;
	}
	if (dlen != 0) {
		FAIL("Expected 0 length but found %zu\n", dlen);
		goto err;
	}

	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	/* With a new session (and ticket), we should use that ticket */
	SSL_SESSION_free(ssl->session);
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	arc4random_buf(&dummy, sizeof(dummy));
	if ((ssl->session->tlsext_tick = malloc(sizeof(dummy))) == NULL) {
		errx(1, "failed to malloc");
	}
	memcpy(ssl->session->tlsext_tick, dummy, sizeof(dummy));
	ssl->session->tlsext_ticklen = sizeof(dummy);

	if (!tlsext_sessionticket_clienthello_needs(ssl)) {
		FAIL("Should still want a session ticket with a new session");
		goto err;
	}
	if (!tlsext_sessionticket_clienthello_build(ssl, &cbb)) {
		FAIL("Cannot build a ticket");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen)) {
		FAIL("Cannot finish CBB");
		goto err;
	}
	if (dlen != sizeof(dummy)) {
		FAIL("Expected %zu length but found %zu\n", sizeof(dummy), dlen);
		goto err;
	}
	if (memcmp(data, dummy, dlen) != 0) {
		FAIL("serverhello SNI differs:\n");
		compare_data(data, dlen,
		    dummy, sizeof(dummy));
		goto err;
	}

	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;
	free(ssl->session->tlsext_tick);
	ssl->session->tlsext_tick = NULL;
	ssl->session->tlsext_ticklen = 0;

	/*
	 * Send in NULL to disable session tickets at runtime without going
	 * through SSL_set_options().
	 */
	if (!SSL_set_session_ticket_ext(ssl, NULL, 0)) {
		FAIL("Could not set a NULL custom ticket");
		goto err;
	}
	/* Should not need a ticket in this case */
	if (tlsext_sessionticket_clienthello_needs(ssl)) {
		FAIL("Should not want to use session tickets with a NULL custom");
		goto err;
	}

	/*
	 * If you want to remove the tlsext_session_ticket behavior, you have
	 * to do it manually.
	 */
	free(ssl->internal->tlsext_session_ticket);
	ssl->internal->tlsext_session_ticket = NULL;

	if (!tlsext_sessionticket_clienthello_needs(ssl)) {
		FAIL("Should need a session ticket again when the custom one is removed");
		goto err;
	}

	/* Test a custom session ticket (not recommended in practice) */
	if (!SSL_set_session_ticket_ext(ssl, tlsext_sessionticket_hello_max,
	    sizeof(tlsext_sessionticket_hello_max))) {
		FAIL("Should be able to set a custom ticket");
		goto err;
	}
	if (!tlsext_sessionticket_clienthello_needs(ssl)) {
		FAIL("Should need a session ticket again when the custom one is not empty");
		goto err;
	}
	if (!tlsext_sessionticket_clienthello_build(ssl, &cbb)) {
		FAIL("Cannot build a ticket with a max length random payload");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen)) {
		FAIL("Cannot finish CBB");
		goto err;
	}
	if (dlen != sizeof(tlsext_sessionticket_hello_max)) {
		FAIL("Expected %zu length but found %zu\n",
		    sizeof(tlsext_sessionticket_hello_max), dlen);
		goto err;
	}
	if (memcmp(data, tlsext_sessionticket_hello_max,
	    sizeof(tlsext_sessionticket_hello_max)) != 0) {
		FAIL("Expected to get what we passed in");
		compare_data(data, dlen,
		    tlsext_sessionticket_hello_max,
		    sizeof(tlsext_sessionticket_hello_max));
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}


static int
test_tlsext_sessionticket_serverhello(void)
{
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int failure;
	uint8_t *data;
	size_t dlen;
	CBB cbb;

	CBB_init(&cbb, 0);

	failure = 1;

	if ((ssl_ctx = SSL_CTX_new(TLS_server_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	/*
	 * By default, should not need a session ticket since the ticket
	 * is not yet expected.
	 */
	if (tlsext_sessionticket_serverhello_needs(ssl)) {
		FAIL("serverhello should not need SessionTicket by default\n");
		goto err;
	}

	/* Test disabling tickets. */
	if ((SSL_set_options(ssl, SSL_OP_NO_TICKET) & SSL_OP_NO_TICKET) == 0) {
		FAIL("Cannot disable tickets in the TLS connection");
		return 0;
	}
	if (tlsext_sessionticket_serverhello_needs(ssl)) {
		FAIL("serverhello should not need SessionTicket if it was disabled");
		goto err;
	}

	/* Test re-enabling tickets. */
	if ((SSL_clear_options(ssl, SSL_OP_NO_TICKET) & SSL_OP_NO_TICKET) != 0) {
		FAIL("Cannot re-enable tickets in the TLS connection");
		return 0;
	}
	if (tlsext_sessionticket_serverhello_needs(ssl)) {
		FAIL("serverhello should not need SessionTicket yet");
		goto err;
	}

	/* Set expected to require it. */
	ssl->internal->tlsext_ticket_expected = 1;
	if (!tlsext_sessionticket_serverhello_needs(ssl)) {
		FAIL("serverhello should now be required for SessionTicket");
		goto err;
	}

	/* server hello's session ticket should always be 0 length payload. */
	if (!tlsext_sessionticket_serverhello_build(ssl, &cbb)) {
		FAIL("Cannot build a ticket with a max length random payload");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen)) {
		FAIL("Cannot finish CBB");
		goto err;
	}
	if (dlen != 0) {
		FAIL("Expected 0 length but found %zu\n", dlen);
		goto err;
	}

	failure = 0;

 err:
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);

	return (failure);
}

#ifndef OPENSSL_NO_SRTP
/*
 * Supported Secure Real-time Transport Protocol (RFC 5764 section 4.1.1)
 */

/* Colon separated string values */
const char *tlsext_srtp_single_profile = "SRTP_AES128_CM_SHA1_80";
const char *tlsext_srtp_multiple_profiles = "SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32";

const char *tlsext_srtp_aes128cmsha80 = "SRTP_AES128_CM_SHA1_80";
const char *tlsext_srtp_aes128cmsha32 = "SRTP_AES128_CM_SHA1_32";

const uint8_t tlsext_srtp_single[] = {
	/* SRTPProtectionProfile SRTPProtectionProfiles<2..2^16-1> */
	0x00, 0x02, /* len */
	0x00, 0x01, /* SRTP_AES128_CM_SHA1_80 */
	0x00        /* opaque srtp_mki<0..255> */
};

const uint8_t tlsext_srtp_multiple[] = {
	/* SRTPProtectionProfile SRTPProtectionProfiles<2..2^16-1> */
	0x00, 0x04, /* len */
	0x00, 0x01, /* SRTP_AES128_CM_SHA1_80 */
	0x00, 0x02, /* SRTP_AES128_CM_SHA1_32 */
	0x00	/* opaque srtp_mki<0..255> */
};

const uint8_t tlsext_srtp_multiple_invalid[] = {
	/* SRTPProtectionProfile SRTPProtectionProfiles<2..2^16-1> */
	0x00, 0x04, /* len */
	0x00, 0x08, /* arbitrary value not found in known profiles */
	0x00, 0x09, /* arbitrary value not found in known profiles */
	0x00	/* opaque srtp_mki<0..255> */
};

const uint8_t tlsext_srtp_single_invalid[] = {
	/* SRTPProtectionProfile SRTPProtectionProfiles<2..2^16-1> */
	0x00, 0x02, /* len */
	0x00, 0x08, /* arbitrary value not found in known profiles */
	0x00	/* opaque srtp_mki<0..255> */
};

const uint8_t tlsext_srtp_multiple_one_valid[] = {
	/* SRTPProtectionProfile SRTPProtectionProfiles<2..2^16-1> */
	0x00, 0x04, /* len */
	0x00, 0x08, /* arbitrary value not found in known profiles */
	0x00, 0x02, /* SRTP_AES128_CM_SHA1_32 */
	0x00	    /* opaque srtp_mki<0..255> */
};

static int
test_tlsext_srtp_clienthello(void)
{
	SRTP_PROTECTION_PROFILE *prof;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	uint8_t *data = NULL;
	CBB cbb;
	CBS cbs;
	int failure, alert;
	size_t dlen;

	CBB_init(&cbb, 0);

	failure = 1;

	/* SRTP is for DTLS */
	if ((ssl_ctx = SSL_CTX_new(DTLSv1_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	/* By default, we don't need this */
	if (tlsext_srtp_clienthello_needs(ssl)) {
		FAIL("clienthello should not need SRTP by default\n");
		goto err;
	}

	if (SSL_set_tlsext_use_srtp(ssl, tlsext_srtp_single_profile) != 0) {
		FAIL("should be able to set a single SRTP\n");
		goto err;
	}
	if (!tlsext_srtp_clienthello_needs(ssl)) {
		FAIL("clienthello should need SRTP\n");
		goto err;
	}

	/* Make sure we can build the clienthello with a single profile. */

	if (!tlsext_srtp_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build SRTP\n");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_srtp_single)) {
		FAIL("got clienthello SRTP with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_srtp_single));
		compare_data(data, dlen, tlsext_srtp_single,
		    sizeof(tlsext_srtp_single));
		goto err;
	}
	if (memcmp(data, tlsext_srtp_single, dlen) != 0) {
		FAIL("clienthello SRTP differs:\n");
		compare_data(data, dlen, tlsext_srtp_single,
		    sizeof(tlsext_srtp_single));
		goto err;
	}

	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	/* Make sure we can parse the single profile. */

	if (SSL_get_selected_srtp_profile(ssl) != NULL) {
		FAIL("SRTP profile should not be set yet\n");
		goto err;
	}

	CBS_init(&cbs, tlsext_srtp_single, sizeof(tlsext_srtp_single));
	if (!tlsext_srtp_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse SRTP\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if ((prof = SSL_get_selected_srtp_profile(ssl)) == NULL) {
		FAIL("SRTP profile should be set now\n");
		goto err;
	}
	if (strcmp(prof->name, tlsext_srtp_aes128cmsha80) != 0) {
		FAIL("SRTP profile was not set properly\n");
		goto err;
	}

	if (!tlsext_srtp_serverhello_needs(ssl)) {
		FAIL("should send server extension when profile selected\n");
		goto err;
	}

	/* Make sure we can build the clienthello with multiple entries. */

	if (SSL_set_tlsext_use_srtp(ssl, tlsext_srtp_multiple_profiles) != 0) {
		FAIL("should be able to set SRTP to multiple profiles\n");
		goto err;
	}
	if (!tlsext_srtp_clienthello_needs(ssl)) {
		FAIL("clienthello should need SRTP by now\n");
		goto err;
	}

	if (!tlsext_srtp_clienthello_build(ssl, &cbb)) {
		FAIL("clienthello failed to build SRTP\n");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_srtp_multiple)) {
		FAIL("got clienthello SRTP with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_srtp_multiple));
		compare_data(data, dlen, tlsext_srtp_multiple,
		    sizeof(tlsext_srtp_multiple));
		goto err;
	}
	if (memcmp(data, tlsext_srtp_multiple, dlen) != 0) {
		FAIL("clienthello SRTP differs:\n");
		compare_data(data, dlen, tlsext_srtp_multiple,
		    sizeof(tlsext_srtp_multiple));
		goto err;
	}

	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	/* Make sure we can parse multiple profiles (selects server preferred) */

	ssl->internal->srtp_profile = NULL;

	CBS_init(&cbs, tlsext_srtp_multiple,
	    sizeof(tlsext_srtp_multiple));
	if (!tlsext_srtp_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse SRTP\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if ((prof = SSL_get_selected_srtp_profile(ssl)) == NULL) {
		FAIL("SRTP profile should be set now\n");
		goto err;
	}
	if (strcmp(prof->name, tlsext_srtp_aes128cmsha80) != 0) {
		FAIL("SRTP profile was not set properly\n");
		goto err;
	}

	if (!tlsext_srtp_serverhello_needs(ssl)) {
		FAIL("should send server extension when profile selected\n");
		goto err;
	}

	/*
	 * Make sure we can parse the clienthello with multiple entries
	 * where one is unknown.
	 */
	ssl->internal->srtp_profile = NULL;

	CBS_init(&cbs, tlsext_srtp_multiple_one_valid,
	    sizeof(tlsext_srtp_multiple_one_valid));
	if (!tlsext_srtp_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse SRTP\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if ((prof = SSL_get_selected_srtp_profile(ssl)) == NULL) {
		FAIL("SRTP profile should be set now\n");
		goto err;
	}
	if (strcmp(prof->name, tlsext_srtp_aes128cmsha32) != 0) {
		FAIL("SRTP profile was not set properly\n");
		goto err;
	}

	if (!tlsext_srtp_serverhello_needs(ssl)) {
		FAIL("should send server extension when profile selected\n");
		goto err;
	}

	/* Make sure we fall back to negotiated when none work. */

	ssl->internal->srtp_profile = NULL;

	CBS_init(&cbs, tlsext_srtp_multiple_invalid,
	    sizeof(tlsext_srtp_multiple_invalid));
	if (!tlsext_srtp_clienthello_parse(ssl, &cbs, &alert)) {
		FAIL("should be able to fall back to negotiated\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	/* If we fallback, the server should NOT send the extension. */
	if (SSL_get_selected_srtp_profile(ssl) != NULL) {
		FAIL("should not have selected a profile when none found\n");
		goto err;
	}
	if (tlsext_srtp_serverhello_needs(ssl)) {
		FAIL("should not send server tlsext when no profile found\n");
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

static int
test_tlsext_srtp_serverhello(void)
{
	SRTP_PROTECTION_PROFILE *prof;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	uint8_t *data = NULL;
	CBB cbb;
	CBS cbs;
	int failure, alert;
	size_t dlen;

	CBB_init(&cbb, 0);

	failure = 1;

	/* SRTP is for DTLS */
	if ((ssl_ctx = SSL_CTX_new(DTLSv1_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	/* By default, we don't need this */
	if (tlsext_srtp_serverhello_needs(ssl)) {
		FAIL("serverhello should not need SRTP by default\n");
		goto err;
	}

	if (srtp_find_profile_by_name((char *)tlsext_srtp_aes128cmsha80, &prof,
	    strlen(tlsext_srtp_aes128cmsha80))) {
		FAIL("should be able to find the given profile\n");
		goto err;
	}
	ssl->internal->srtp_profile = prof;
	if (!tlsext_srtp_serverhello_needs(ssl)) {
		FAIL("serverhello should need SRTP by now\n");
		goto err;
	}

	/* Make sure we can build the serverhello with a single profile. */

	if (!tlsext_srtp_serverhello_build(ssl, &cbb)) {
		FAIL("serverhello failed to build SRTP\n");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_srtp_single)) {
		FAIL("got serverhello SRTP with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_srtp_single));
		compare_data(data, dlen, tlsext_srtp_single,
		    sizeof(tlsext_srtp_single));
		goto err;
	}
	if (memcmp(data, tlsext_srtp_single, dlen) != 0) {
		FAIL("serverhello SRTP differs:\n");
		compare_data(data, dlen, tlsext_srtp_single,
		    sizeof(tlsext_srtp_single));
		goto err;
	}

	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);
	free(data);
	data = NULL;

	/* Make sure we can parse the single profile. */
	ssl->internal->srtp_profile = NULL;

	if (SSL_get_selected_srtp_profile(ssl) != NULL) {
		FAIL("SRTP profile should not be set yet\n");
		goto err;
	}

	/* Setup the environment as if a client sent a list of profiles. */
	if (SSL_set_tlsext_use_srtp(ssl, tlsext_srtp_multiple_profiles) != 0) {
		FAIL("should be able to set multiple profiles in SRTP\n");
		goto err;
	}

	CBS_init(&cbs, tlsext_srtp_single, sizeof(tlsext_srtp_single));
	if (!tlsext_srtp_serverhello_parse(ssl, &cbs, &alert)) {
		FAIL("failed to parse SRTP\n");
		goto err;
	}
	if (CBS_len(&cbs) != 0) {
		FAIL("extension data remaining");
		goto err;
	}

	if ((prof = SSL_get_selected_srtp_profile(ssl)) == NULL) {
		FAIL("SRTP profile should be set now\n");
		goto err;
	}
	if (strcmp(prof->name, tlsext_srtp_aes128cmsha80) != 0) {
		FAIL("SRTP profile was not set properly\n");
		goto err;
	}

	/* Make sure we cannot parse multiple profiles */
	ssl->internal->srtp_profile = NULL;

	CBS_init(&cbs, tlsext_srtp_multiple,
	    sizeof(tlsext_srtp_multiple));
	if (tlsext_srtp_serverhello_parse(ssl, &cbs, &alert)) {
		FAIL("should not find multiple entries from the server\n");
		goto err;
	}

	/* Make sure we cannot parse a serverhello with unknown profile */
	ssl->internal->srtp_profile = NULL;

	CBS_init(&cbs, tlsext_srtp_single_invalid,
	    sizeof(tlsext_srtp_single_invalid));
	if (tlsext_srtp_serverhello_parse(ssl, &cbs, &alert)) {
		FAIL("should not be able to parse this\n");
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}
#endif /* OPENSSL_NO_SRTP */

unsigned char tlsext_clienthello_default[] = {
	0x00, 0x36, 0x00, 0x0b, 0x00, 0x02, 0x01, 0x00,
	0x00, 0x0a, 0x00, 0x08, 0x00, 0x06, 0x00, 0x1d,
	0x00, 0x17, 0x00, 0x18, 0x00, 0x23, 0x00, 0x00,
	0x00, 0x0d, 0x00, 0x1c, 0x00, 0x1a, 0x06, 0x01,
	0x06, 0x03, 0xef, 0xef, 0x05, 0x01, 0x05, 0x03,
	0x04, 0x01, 0x04, 0x03, 0xee, 0xee, 0xed, 0xed,
	0x03, 0x01, 0x03, 0x03, 0x02, 0x01, 0x02, 0x03,
};

unsigned char tlsext_clienthello_disabled[] = {};

static int
test_tlsext_clienthello_build(void)
{
	unsigned char *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	size_t dlen;
	int failure;
	CBB cbb;

	failure = 1;

	if (!CBB_init(&cbb, 0))
		errx(1, "failed to create CBB");

	if ((ssl_ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");

	if (!tlsext_clienthello_build(ssl, &cbb)) {
		FAIL("failed to build clienthello extensions\n");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_clienthello_default)) {
		FAIL("got clienthello extensions with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_clienthello_default));
		compare_data(data, dlen, tlsext_clienthello_default,
		    sizeof(tlsext_clienthello_default));
		goto err;
	}
	if (memcmp(data, tlsext_clienthello_default, dlen) != 0) {
		FAIL("clienthello extensions differs:\n");
		compare_data(data, dlen, tlsext_clienthello_default,
		    sizeof(tlsext_clienthello_default));
		goto err;
	}

	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);

	/* Switch to TLSv1.1, disable EC ciphers and session tickets. */
	ssl->client_version = TLS1_1_VERSION;
	if (!SSL_set_cipher_list(ssl, "TLSv1.2:!ECDHE:!ECDSA")) {
		FAIL("failed to set cipher list\n");
		goto err;
	}
	if ((SSL_set_options(ssl, SSL_OP_NO_TICKET) & SSL_OP_NO_TICKET) == 0) {
		FAIL("failed to disable session tickets");
		return 0;
	}

	if (!tlsext_clienthello_build(ssl, &cbb)) {
		FAIL("failed to build clienthello extensions\n");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_clienthello_disabled)) {
		FAIL("got clienthello extensions with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_clienthello_disabled));
		compare_data(data, dlen, tlsext_clienthello_disabled,
		    sizeof(tlsext_clienthello_disabled));
		goto err;
	}
	if (memcmp(data, tlsext_clienthello_disabled, dlen) != 0) {
		FAIL("clienthello extensions differs:\n");
		compare_data(data, dlen, tlsext_clienthello_disabled,
		    sizeof(tlsext_clienthello_disabled));
		goto err;
	}

	failure = 0;

 err:
	CBB_cleanup(&cbb);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);
	free(data);

	return (failure);
}

unsigned char tlsext_serverhello_default[] = {};

unsigned char tlsext_serverhello_enabled[] = {
	0x00, 0x13, 0xff, 0x01, 0x00, 0x01, 0x00, 0x00,
	0x05, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x02, 0x01,
	0x00, 0x00, 0x23, 0x00, 0x00,
};

static int
test_tlsext_serverhello_build(void)
{
	unsigned char *data = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	size_t dlen;
	int failure;
	CBB cbb;

	failure = 1;

	if (!CBB_init(&cbb, 0))
		errx(1, "failed to create CBB");

	if ((ssl_ctx = SSL_CTX_new(TLS_server_method())) == NULL)
		errx(1, "failed to create SSL_CTX");
	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		errx(1, "failed to create SSL");
	if ((ssl->session = SSL_SESSION_new()) == NULL)
		errx(1, "failed to create session");

	S3I(ssl)->hs.new_cipher =
	    ssl3_get_cipher_by_id(TLS1_CK_RSA_WITH_AES_128_SHA256);

	if (!tlsext_serverhello_build(ssl, &cbb)) {
		FAIL("failed to build serverhello extensions\n");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_serverhello_default)) {
		FAIL("got serverhello extensions with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_serverhello_default));
		compare_data(data, dlen, tlsext_serverhello_default,
		    sizeof(tlsext_serverhello_default));
		goto err;
	}
	if (memcmp(data, tlsext_serverhello_default, dlen) != 0) {
		FAIL("serverhello extensions differs:\n");
		compare_data(data, dlen, tlsext_serverhello_default,
		    sizeof(tlsext_serverhello_default));
		goto err;
	}

	CBB_cleanup(&cbb);
	CBB_init(&cbb, 0);

	/* Turn a few things on so we get extensions... */
	S3I(ssl)->send_connection_binding = 1;
	S3I(ssl)->hs.new_cipher =
	    ssl3_get_cipher_by_id(TLS1_CK_ECDHE_RSA_WITH_AES_128_SHA256);
	ssl->internal->tlsext_status_expected = 1;
	ssl->internal->tlsext_ticket_expected = 1;
	if ((SSI(ssl)->tlsext_ecpointformatlist = malloc(1)) == NULL)
		errx(1, "malloc failed");
	SSI(ssl)->tlsext_ecpointformatlist_length = 1;
	SSI(ssl)->tlsext_ecpointformatlist[0] =
	    TLSEXT_ECPOINTFORMAT_uncompressed;

	if (!tlsext_serverhello_build(ssl, &cbb)) {
		FAIL("failed to build serverhello extensions\n");
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &dlen))
		errx(1, "failed to finish CBB");

	if (dlen != sizeof(tlsext_serverhello_enabled)) {
		FAIL("got serverhello extensions with length %zu, "
		    "want length %zu\n", dlen,
		    sizeof(tlsext_serverhello_enabled));
		compare_data(data, dlen, tlsext_serverhello_enabled,
		    sizeof(tlsext_serverhello_enabled));
		goto err;
	}
	if (memcmp(data, tlsext_serverhello_enabled, dlen) != 0) {
		FAIL("serverhello extensions differs:\n");
		compare_data(data, dlen, tlsext_serverhello_enabled,
		    sizeof(tlsext_serverhello_enabled));
		goto err;
	}

	failure = 0;

 err:
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
	SSL_load_error_strings();

	failed |= test_tlsext_alpn_clienthello();
	failed |= test_tlsext_alpn_serverhello();

	failed |= test_tlsext_supportedgroups_clienthello();
	failed |= test_tlsext_supportedgroups_serverhello();

	failed |= test_tlsext_ecpf_clienthello();
	failed |= test_tlsext_ecpf_serverhello();

	failed |= test_tlsext_ri_clienthello();
	failed |= test_tlsext_ri_serverhello();

	failed |= test_tlsext_sigalgs_clienthello();
	failed |= test_tlsext_sigalgs_serverhello();

	failed |= test_tlsext_sni_clienthello();
	failed |= test_tlsext_sni_serverhello();

	failed |= test_tlsext_ocsp_clienthello();
	failed |= test_tlsext_ocsp_serverhello();

	failed |= test_tlsext_sessionticket_clienthello();
	failed |= test_tlsext_sessionticket_serverhello();

#ifndef OPENSSL_NO_SRTP
	failed |= test_tlsext_srtp_clienthello();
	failed |= test_tlsext_srtp_serverhello();
#else
	fprintf(stderr, "Skipping SRTP tests due to OPENSSL_NO_SRTP\n");
#endif

	failed |= test_tlsext_clienthello_build();
	failed |= test_tlsext_serverhello_build();

	return (failed);
}
