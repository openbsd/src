/* $OpenBSD: ssl_tlsext.c,v 1.26 2018/11/09 05:02:53 beck Exp $ */
/*
 * Copyright (c) 2016, 2017 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2017 Doug Hogan <doug@openbsd.org>
 * Copyright (c) 2017 Bob Beck <beck@openbsd.org>
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
#include <openssl/ocsp.h>

#include "ssl_locl.h"

#include "bytestring.h"
#include "ssl_tlsext.h"
#include "ssl_sigalgs.h"

/*
 * Supported Application-Layer Protocol Negotiation - RFC 7301
 */

int
tlsext_alpn_clienthello_needs(SSL *s)
{
	/* ALPN protos have been specified and this is the initial handshake */
	return s->internal->alpn_client_proto_list != NULL &&
	    S3I(s)->tmp.finish_md_len == 0;
}

int
tlsext_alpn_clienthello_build(SSL *s, CBB *cbb)
{
	CBB protolist;

	if (!CBB_add_u16_length_prefixed(cbb, &protolist))
		return 0;

	if (!CBB_add_bytes(&protolist, s->internal->alpn_client_proto_list,
	    s->internal->alpn_client_proto_list_len))
		return 0;

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_alpn_clienthello_parse(SSL *s, CBS *cbs, int *alert)
{
	CBS proto_name_list, alpn;
	const unsigned char *selected;
	unsigned char selected_len;
	int r;

	if (!CBS_get_u16_length_prefixed(cbs, &alpn))
		goto err;
	if (CBS_len(&alpn) < 2)
		goto err;
	if (CBS_len(cbs) != 0)
		goto err;

	CBS_dup(&alpn, &proto_name_list);
	while (CBS_len(&proto_name_list) > 0) {
		CBS proto_name;

		if (!CBS_get_u8_length_prefixed(&proto_name_list, &proto_name))
			goto err;
		if (CBS_len(&proto_name) == 0)
			goto err;
	}

	if (s->ctx->internal->alpn_select_cb == NULL)
		return 1;

	r = s->ctx->internal->alpn_select_cb(s, &selected, &selected_len,
	    CBS_data(&alpn), CBS_len(&alpn),
	    s->ctx->internal->alpn_select_cb_arg);
	if (r == SSL_TLSEXT_ERR_OK) {
		free(S3I(s)->alpn_selected);
		if ((S3I(s)->alpn_selected = malloc(selected_len)) == NULL) {
			*alert = SSL_AD_INTERNAL_ERROR;
			return 0;
		}
		memcpy(S3I(s)->alpn_selected, selected, selected_len);
		S3I(s)->alpn_selected_len = selected_len;
	}

	return 1;

 err:
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

int
tlsext_alpn_serverhello_needs(SSL *s)
{
	return S3I(s)->alpn_selected != NULL;
}

int
tlsext_alpn_serverhello_build(SSL *s, CBB *cbb)
{
	CBB list, selected;

	if (!CBB_add_u16_length_prefixed(cbb, &list))
		return 0;

	if (!CBB_add_u8_length_prefixed(&list, &selected))
		return 0;

	if (!CBB_add_bytes(&selected, S3I(s)->alpn_selected,
	    S3I(s)->alpn_selected_len))
		return 0;

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_alpn_serverhello_parse(SSL *s, CBS *cbs, int *alert)
{
	CBS list, proto;

	if (s->internal->alpn_client_proto_list == NULL) {
		*alert = TLS1_AD_UNSUPPORTED_EXTENSION;
		return 0;
	}

	if (!CBS_get_u16_length_prefixed(cbs, &list))
		goto err;
	if (CBS_len(cbs) != 0)
		goto err;

	if (!CBS_get_u8_length_prefixed(&list, &proto))
		goto err;

	if (CBS_len(&list) != 0)
		goto err;
	if (CBS_len(&proto) == 0)
		goto err;

	if (!CBS_stow(&proto, &(S3I(s)->alpn_selected),
	    &(S3I(s)->alpn_selected_len)))
		goto err;

	return 1;

 err:
	*alert = TLS1_AD_DECODE_ERROR;
	return 0;
}

/*
 * Supported Groups - RFC 7919 section 2
 */
int
tlsext_supportedgroups_clienthello_needs(SSL *s)
{
	return ssl_has_ecc_ciphers(s);
}

int
tlsext_supportedgroups_clienthello_build(SSL *s, CBB *cbb)
{
	const uint16_t *groups;
	size_t groups_len;
	CBB grouplist;
	int i;

	tls1_get_group_list(s, 0, &groups, &groups_len);
	if (groups_len == 0) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		return 0;
	}

	if (!CBB_add_u16_length_prefixed(cbb, &grouplist))
		return 0;

	for (i = 0; i < groups_len; i++) {
		if (!CBB_add_u16(&grouplist, groups[i]))
			return 0;
	}

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_supportedgroups_clienthello_parse(SSL *s, CBS *cbs, int *alert)
{
	CBS grouplist;
	size_t groups_len;

	if (!CBS_get_u16_length_prefixed(cbs, &grouplist))
		goto err;
	if (CBS_len(cbs) != 0)
		goto err;

	groups_len = CBS_len(&grouplist);
	if (groups_len == 0 || groups_len % 2 != 0)
		goto err;
	groups_len /= 2;

	if (!s->internal->hit) {
		uint16_t *groups;
		int i;

		if (SSI(s)->tlsext_supportedgroups != NULL)
			goto err;

		if ((groups = reallocarray(NULL, groups_len,
		    sizeof(uint16_t))) == NULL) {
			*alert = TLS1_AD_INTERNAL_ERROR;
			return 0;
		}

		for (i = 0; i < groups_len; i++) {
			if (!CBS_get_u16(&grouplist, &groups[i])) {
				free(groups);
				goto err;
			}
		}

		if (CBS_len(&grouplist) != 0) {
			free(groups);
			goto err;
		}

		SSI(s)->tlsext_supportedgroups = groups;
		SSI(s)->tlsext_supportedgroups_length = groups_len;
	}

	return 1;

 err:
	*alert = TLS1_AD_DECODE_ERROR;
	return 0;
}

/* This extension is never used by the server. */
int
tlsext_supportedgroups_serverhello_needs(SSL *s)
{
	return 0;
}

int
tlsext_supportedgroups_serverhello_build(SSL *s, CBB *cbb)
{
	return 0;
}

int
tlsext_supportedgroups_serverhello_parse(SSL *s, CBS *cbs, int *alert)
{
	/*
	 * Servers should not send this extension per the RFC.
	 *
	 * However, certain F5 BIG-IP systems incorrectly send it. This bug is
	 * from at least 2014 but as of 2017, there are still large sites with
	 * this unpatched in production. As a result, we need to currently skip
	 * over the extension and ignore its content:
	 *
	 *  https://support.f5.com/csp/article/K37345003
	 */
	if (!CBS_skip(cbs, CBS_len(cbs))) {
		*alert = TLS1_AD_INTERNAL_ERROR;
		return 0;
	}

	return 1;
}

/*
 * Supported Point Formats Extension - RFC 4492 section 5.1.2
 */
static int
tlsext_ecpf_build(SSL *s, CBB *cbb)
{
	CBB ecpf;
	size_t formats_len;
	const uint8_t *formats;

	tls1_get_formatlist(s, 0, &formats, &formats_len);

	if (formats_len == 0) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		return 0;
	}

	if (!CBB_add_u8_length_prefixed(cbb, &ecpf))
		return 0;
	if (!CBB_add_bytes(&ecpf, formats, formats_len))
		return 0;
	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

static int
tlsext_ecpf_parse(SSL *s, CBS *cbs, int *alert)
{
	CBS ecpf;

	if (!CBS_get_u8_length_prefixed(cbs, &ecpf))
		goto err;
	if (CBS_len(&ecpf) == 0)
		goto err;
	if (CBS_len(cbs) != 0)
		goto err;

	/* Must contain uncompressed (0) */
	if (!CBS_contains_zero_byte(&ecpf)) {
		SSLerror(s, SSL_R_TLS_INVALID_ECPOINTFORMAT_LIST);
		goto err;
	}

	if (!s->internal->hit) {
		if (!CBS_stow(&ecpf, &(SSI(s)->tlsext_ecpointformatlist),
		    &(SSI(s)->tlsext_ecpointformatlist_length))) {
			*alert = TLS1_AD_INTERNAL_ERROR;
			return 0;
		}
	}

	return 1;

 err:
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

int
tlsext_ecpf_clienthello_needs(SSL *s)
{
	return ssl_has_ecc_ciphers(s);
}

int
tlsext_ecpf_clienthello_build(SSL *s, CBB *cbb)
{
	return tlsext_ecpf_build(s, cbb);
}

int
tlsext_ecpf_clienthello_parse(SSL *s, CBS *cbs, int *alert)
{
	return tlsext_ecpf_parse(s, cbs, alert);
}

int
tlsext_ecpf_serverhello_needs(SSL *s)
{
	if (s->version == DTLS1_VERSION)
		return 0;

	return ssl_using_ecc_cipher(s);
}

int
tlsext_ecpf_serverhello_build(SSL *s, CBB *cbb)
{
	return tlsext_ecpf_build(s, cbb);
}

int
tlsext_ecpf_serverhello_parse(SSL *s, CBS *cbs, int *alert)
{
	return tlsext_ecpf_parse(s, cbs, alert);
}

/*
 * Renegotiation Indication - RFC 5746.
 */
int
tlsext_ri_clienthello_needs(SSL *s)
{
	return (s->internal->renegotiate);
}

int
tlsext_ri_clienthello_build(SSL *s, CBB *cbb)
{
	CBB reneg;

	if (!CBB_add_u8_length_prefixed(cbb, &reneg))
		return 0;
	if (!CBB_add_bytes(&reneg, S3I(s)->previous_client_finished,
	    S3I(s)->previous_client_finished_len))
		return 0;
	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_ri_clienthello_parse(SSL *s, CBS *cbs, int *alert)
{
	CBS reneg;

	if (!CBS_get_u8_length_prefixed(cbs, &reneg))
		goto err;
	if (CBS_len(cbs) != 0)
		goto err;

	if (!CBS_mem_equal(&reneg, S3I(s)->previous_client_finished,
	    S3I(s)->previous_client_finished_len)) {
		SSLerror(s, SSL_R_RENEGOTIATION_MISMATCH);
		*alert = SSL_AD_HANDSHAKE_FAILURE;
		return 0;
	}

	S3I(s)->renegotiate_seen = 1;
	S3I(s)->send_connection_binding = 1;

	return 1;

 err:
	SSLerror(s, SSL_R_RENEGOTIATION_ENCODING_ERR);
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

int
tlsext_ri_serverhello_needs(SSL *s)
{
	return (S3I(s)->send_connection_binding);
}

int
tlsext_ri_serverhello_build(SSL *s, CBB *cbb)
{
	CBB reneg;

	if (!CBB_add_u8_length_prefixed(cbb, &reneg))
		return 0;
	if (!CBB_add_bytes(&reneg, S3I(s)->previous_client_finished,
	    S3I(s)->previous_client_finished_len))
		return 0;
	if (!CBB_add_bytes(&reneg, S3I(s)->previous_server_finished,
	    S3I(s)->previous_server_finished_len))
		return 0;
	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_ri_serverhello_parse(SSL *s, CBS *cbs, int *alert)
{
	CBS reneg, prev_client, prev_server;

	/*
	 * Ensure that the previous client and server values are both not
	 * present, or that they are both present.
	 */
	if ((S3I(s)->previous_client_finished_len == 0 &&
	    S3I(s)->previous_server_finished_len != 0) ||
	    (S3I(s)->previous_client_finished_len != 0 &&
	    S3I(s)->previous_server_finished_len == 0)) {
		*alert = TLS1_AD_INTERNAL_ERROR;
		return 0;
	}

	if (!CBS_get_u8_length_prefixed(cbs, &reneg))
		goto err;
	if (!CBS_get_bytes(&reneg, &prev_client,
	    S3I(s)->previous_client_finished_len))
		goto err;
	if (!CBS_get_bytes(&reneg, &prev_server,
	    S3I(s)->previous_server_finished_len))
		goto err;
	if (CBS_len(&reneg) != 0)
		goto err;
	if (CBS_len(cbs) != 0)
		goto err;

	if (!CBS_mem_equal(&prev_client, S3I(s)->previous_client_finished,
	    S3I(s)->previous_client_finished_len)) {
		SSLerror(s, SSL_R_RENEGOTIATION_MISMATCH);
		*alert = SSL_AD_HANDSHAKE_FAILURE;
		return 0;
	}
	if (!CBS_mem_equal(&prev_server, S3I(s)->previous_server_finished,
	    S3I(s)->previous_server_finished_len)) {
		SSLerror(s, SSL_R_RENEGOTIATION_MISMATCH);
		*alert = SSL_AD_HANDSHAKE_FAILURE;
		return 0;
	}

	S3I(s)->renegotiate_seen = 1;
	S3I(s)->send_connection_binding = 1;

	return 1;

 err:
	SSLerror(s, SSL_R_RENEGOTIATION_ENCODING_ERR);
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

/*
 * Signature Algorithms - RFC 5246 section 7.4.1.4.1.
 */
int
tlsext_sigalgs_clienthello_needs(SSL *s)
{
	return (TLS1_get_client_version(s) >= TLS1_2_VERSION);
}

int
tlsext_sigalgs_clienthello_build(SSL *s, CBB *cbb)
{
	CBB sigalgs;

	if (!CBB_add_u16_length_prefixed(cbb, &sigalgs))
		return 0;

	if (!ssl_sigalgs_build(&sigalgs, tls12_sigalgs, tls12_sigalgs_len))
		return 0;

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_sigalgs_clienthello_parse(SSL *s, CBS *cbs, int *alert)
{
	CBS sigalgs;

	if (!CBS_get_u16_length_prefixed(cbs, &sigalgs))
		return 0;

	return tls1_process_sigalgs(s, &sigalgs);
}

int
tlsext_sigalgs_serverhello_needs(SSL *s)
{
	return 0;
}

int
tlsext_sigalgs_serverhello_build(SSL *s, CBB *cbb)
{
	return 0;
}

int
tlsext_sigalgs_serverhello_parse(SSL *s, CBS *cbs, int *alert)
{
	/* As per the RFC, servers must not send this extension. */
	return 0;
}

/*
 * Server Name Indication - RFC 6066, section 3.
 */
int
tlsext_sni_clienthello_needs(SSL *s)
{
	return (s->tlsext_hostname != NULL);
}

int
tlsext_sni_clienthello_build(SSL *s, CBB *cbb)
{
	CBB server_name_list, host_name;

	if (!CBB_add_u16_length_prefixed(cbb, &server_name_list))
		return 0;
	if (!CBB_add_u8(&server_name_list, TLSEXT_NAMETYPE_host_name))
		return 0;
	if (!CBB_add_u16_length_prefixed(&server_name_list, &host_name))
		return 0;
	if (!CBB_add_bytes(&host_name, (const uint8_t *)s->tlsext_hostname,
	    strlen(s->tlsext_hostname)))
		return 0;
	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_sni_clienthello_parse(SSL *s, CBS *cbs, int *alert)
{
	CBS server_name_list, host_name;
	uint8_t name_type;

	if (!CBS_get_u16_length_prefixed(cbs, &server_name_list))
		goto err;

	/*
	 * RFC 6066 section 3 forbids multiple host names with the same type.
	 * Additionally, only one type (host_name) is specified.
	 */
	if (!CBS_get_u8(&server_name_list, &name_type))
		goto err;
	if (name_type != TLSEXT_NAMETYPE_host_name)
		goto err;

	if (!CBS_get_u16_length_prefixed(&server_name_list, &host_name))
		goto err;
	if (CBS_len(&host_name) == 0 ||
	    CBS_len(&host_name) > TLSEXT_MAXLEN_host_name ||
	    CBS_contains_zero_byte(&host_name)) {
		*alert = TLS1_AD_UNRECOGNIZED_NAME;
		return 0;
	}

	if (s->internal->hit) {
		if (s->session->tlsext_hostname == NULL) {
			*alert = TLS1_AD_UNRECOGNIZED_NAME;
			return 0;
		}
		if (!CBS_mem_equal(&host_name, s->session->tlsext_hostname,
		    strlen(s->session->tlsext_hostname))) {
			*alert = TLS1_AD_UNRECOGNIZED_NAME;
			return 0;
		}
	} else {
		if (s->session->tlsext_hostname != NULL)
			goto err;
		if (!CBS_strdup(&host_name, &s->session->tlsext_hostname)) {
			*alert = TLS1_AD_INTERNAL_ERROR;
			return 0;
		}
	}

	if (CBS_len(&server_name_list) != 0)
		goto err;
	if (CBS_len(cbs) != 0)
		goto err;

	return 1;

 err:
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

int
tlsext_sni_serverhello_needs(SSL *s)
{
	return (s->session->tlsext_hostname != NULL);
}

int
tlsext_sni_serverhello_build(SSL *s, CBB *cbb)
{
	return 1;
}

int
tlsext_sni_serverhello_parse(SSL *s, CBS *cbs, int *alert)
{
	if (s->tlsext_hostname == NULL || CBS_len(cbs) != 0) {
		*alert = TLS1_AD_UNRECOGNIZED_NAME;
		return 0;
	}

	if (s->internal->hit) {
		if (s->session->tlsext_hostname == NULL) {
			*alert = TLS1_AD_UNRECOGNIZED_NAME;
			return 0;
		}
		if (strcmp(s->tlsext_hostname,
		    s->session->tlsext_hostname) != 0) {
			*alert = TLS1_AD_UNRECOGNIZED_NAME;
			return 0;
		}
	} else {
		if (s->session->tlsext_hostname != NULL) {
			*alert = SSL_AD_DECODE_ERROR;
			return 0;
		}
		if ((s->session->tlsext_hostname =
		    strdup(s->tlsext_hostname)) == NULL) {
			*alert = TLS1_AD_INTERNAL_ERROR;
			return 0;
		}
	}

	return 1;
}


/*
 *Certificate Status Request - RFC 6066 section 8.
 */

int
tlsext_ocsp_clienthello_needs(SSL *s)
{
	return (s->tlsext_status_type == TLSEXT_STATUSTYPE_ocsp &&
	    s->version != DTLS1_VERSION);
}

int
tlsext_ocsp_clienthello_build(SSL *s, CBB *cbb)
{
	CBB respid_list, respid, exts;
	unsigned char *ext_data;
	size_t ext_len;
	int i;

	if (!CBB_add_u8(cbb, TLSEXT_STATUSTYPE_ocsp))
		return 0;
	if (!CBB_add_u16_length_prefixed(cbb, &respid_list))
		return 0;
	for (i = 0; i < sk_OCSP_RESPID_num(s->internal->tlsext_ocsp_ids); i++) {
		unsigned char *respid_data;
		OCSP_RESPID *id;
		size_t id_len;

		if ((id = sk_OCSP_RESPID_value(s->internal->tlsext_ocsp_ids,
		    i)) ==  NULL)
			return 0;
		if ((id_len = i2d_OCSP_RESPID(id, NULL)) == -1)
			return 0;
		if (!CBB_add_u16_length_prefixed(&respid_list, &respid))
			return 0;
		if (!CBB_add_space(&respid, &respid_data, id_len))
			return 0;
		if ((i2d_OCSP_RESPID(id, &respid_data)) != id_len)
			return 0;
	}
	if (!CBB_add_u16_length_prefixed(cbb, &exts))
		return 0;
	if ((ext_len = i2d_X509_EXTENSIONS(s->internal->tlsext_ocsp_exts,
	    NULL)) == -1)
		return 0;
	if (!CBB_add_space(&exts, &ext_data, ext_len))
		return 0;
	if ((i2d_X509_EXTENSIONS(s->internal->tlsext_ocsp_exts, &ext_data) !=
	    ext_len))
		return 0;
	if (!CBB_flush(cbb))
		return 0;
	return 1;
}

int
tlsext_ocsp_clienthello_parse(SSL *s, CBS *cbs, int *alert)
{
	int failure = SSL_AD_DECODE_ERROR;
	CBS respid_list, respid, exts;
	const unsigned char *p;
	uint8_t status_type;
	int ret = 0;

	if (!CBS_get_u8(cbs, &status_type))
		goto err;
	if (status_type != TLSEXT_STATUSTYPE_ocsp) {
		/* ignore unknown status types */
		s->tlsext_status_type = -1;

		if (!CBS_skip(cbs, CBS_len(cbs))) {
			*alert = TLS1_AD_INTERNAL_ERROR;
			return 0;
		}
		return 1;
	}
	s->tlsext_status_type = status_type;
	if (!CBS_get_u16_length_prefixed(cbs, &respid_list))
		goto err;

	/* XXX */
	sk_OCSP_RESPID_pop_free(s->internal->tlsext_ocsp_ids, OCSP_RESPID_free);
	s->internal->tlsext_ocsp_ids = NULL;
	if (CBS_len(&respid_list) > 0) {
		s->internal->tlsext_ocsp_ids = sk_OCSP_RESPID_new_null();
		if (s->internal->tlsext_ocsp_ids == NULL) {
			failure = SSL_AD_INTERNAL_ERROR;
			goto err;
		}
	}

	while (CBS_len(&respid_list) > 0) {
		OCSP_RESPID *id;

		if (!CBS_get_u16_length_prefixed(&respid_list, &respid))
			goto err;
		p = CBS_data(&respid);
		if ((id = d2i_OCSP_RESPID(NULL, &p, CBS_len(&respid))) == NULL)
			goto err;
		if (!sk_OCSP_RESPID_push(s->internal->tlsext_ocsp_ids, id)) {
			failure = SSL_AD_INTERNAL_ERROR;
			OCSP_RESPID_free(id);
			goto err;
		}
	}

	/* Read in request_extensions */
	if (!CBS_get_u16_length_prefixed(cbs, &exts))
		goto err;
	if (CBS_len(&exts) > 0) {
		sk_X509_EXTENSION_pop_free(s->internal->tlsext_ocsp_exts,
		    X509_EXTENSION_free);
		p = CBS_data(&exts);
		if ((s->internal->tlsext_ocsp_exts = d2i_X509_EXTENSIONS(NULL,
		    &p, CBS_len(&exts))) == NULL)
			goto err;
	}

	/* should be nothing left */
	if (CBS_len(cbs) > 0)
		goto err;

	ret = 1;
 err:
	if (ret == 0)
		*alert = failure;
	return ret;
}

int
tlsext_ocsp_serverhello_needs(SSL *s)
{
	return s->internal->tlsext_status_expected;
}

int
tlsext_ocsp_serverhello_build(SSL *s, CBB *cbb)
{
	return 1;
}

int
tlsext_ocsp_serverhello_parse(SSL *s, CBS *cbs, int *alert)
{
	if (s->tlsext_status_type == -1) {
		*alert = TLS1_AD_UNSUPPORTED_EXTENSION;
		return 0;
	}
	/* Set flag to expect CertificateStatus message */
	s->internal->tlsext_status_expected = 1;
	return 1;
}

/*
 * SessionTicket extension - RFC 5077 section 3.2
 */
int
tlsext_sessionticket_clienthello_needs(SSL *s)
{
	/*
	 * Send session ticket extension when enabled and not overridden.
	 *
	 * When renegotiating, send an empty session ticket to indicate support.
	 */
	if ((SSL_get_options(s) & SSL_OP_NO_TICKET) != 0)
		return 0;

	if (s->internal->new_session)
		return 1;

	if (s->internal->tlsext_session_ticket != NULL &&
	    s->internal->tlsext_session_ticket->data == NULL)
		return 0;

	return 1;
}

int
tlsext_sessionticket_clienthello_build(SSL *s, CBB *cbb)
{
	/*
	 * Signal that we support session tickets by sending an empty
	 * extension when renegotiating or no session found.
	 */
	if (s->internal->new_session || s->session == NULL)
		return 1;

	if (s->session->tlsext_tick != NULL) {
		/* Attempt to resume with an existing session ticket */
		if (!CBB_add_bytes(cbb, s->session->tlsext_tick,
		    s->session->tlsext_ticklen))
			return 0;

	} else if (s->internal->tlsext_session_ticket != NULL) {
		/*
		 * Attempt to resume with a custom provided session ticket set
		 * by SSL_set_session_ticket_ext().
		 */
		if (s->internal->tlsext_session_ticket->length > 0) {
			size_t ticklen = s->internal->tlsext_session_ticket->length;

			if ((s->session->tlsext_tick = malloc(ticklen)) == NULL)
				return 0;
			memcpy(s->session->tlsext_tick,
			    s->internal->tlsext_session_ticket->data,
			    ticklen);
			s->session->tlsext_ticklen = ticklen;

			if (!CBB_add_bytes(cbb, s->session->tlsext_tick,
			    s->session->tlsext_ticklen))
				return 0;
		}
	}

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_sessionticket_clienthello_parse(SSL *s, CBS *cbs, int *alert)
{
	if (s->internal->tls_session_ticket_ext_cb) {
		if (!s->internal->tls_session_ticket_ext_cb(s, CBS_data(cbs),
		    (int)CBS_len(cbs),
		    s->internal->tls_session_ticket_ext_cb_arg)) {
			*alert = TLS1_AD_INTERNAL_ERROR;
			return 0;
		}
	}

	/* We need to signal that this was processed fully */
	if (!CBS_skip(cbs, CBS_len(cbs))) {
		*alert = TLS1_AD_INTERNAL_ERROR;
		return 0;
	}

	return 1;
}

int
tlsext_sessionticket_serverhello_needs(SSL *s)
{
	return (s->internal->tlsext_ticket_expected &&
	    !(SSL_get_options(s) & SSL_OP_NO_TICKET));
}

int
tlsext_sessionticket_serverhello_build(SSL *s, CBB *cbb)
{
	/* Empty ticket */

	return 1;
}

int
tlsext_sessionticket_serverhello_parse(SSL *s, CBS *cbs, int *alert)
{
	if (s->internal->tls_session_ticket_ext_cb) {
		if (!s->internal->tls_session_ticket_ext_cb(s, CBS_data(cbs),
		    (int)CBS_len(cbs),
		    s->internal->tls_session_ticket_ext_cb_arg)) {
			*alert = TLS1_AD_INTERNAL_ERROR;
			return 0;
		}
	}

	if ((SSL_get_options(s) & SSL_OP_NO_TICKET) != 0 || CBS_len(cbs) > 0) {
		*alert = TLS1_AD_UNSUPPORTED_EXTENSION;
		return 0;
	}

	s->internal->tlsext_ticket_expected = 1;

	return 1;
}

/*
 * DTLS extension for SRTP key establishment - RFC 5764
 */

#ifndef OPENSSL_NO_SRTP

int
tlsext_srtp_clienthello_needs(SSL *s)
{
	return SSL_IS_DTLS(s) && SSL_get_srtp_profiles(s) != NULL;
}

int
tlsext_srtp_clienthello_build(SSL *s, CBB *cbb)
{
	CBB profiles, mki;
	int ct, i;
	STACK_OF(SRTP_PROTECTION_PROFILE) *clnt = NULL;
	SRTP_PROTECTION_PROFILE *prof;

	if ((clnt = SSL_get_srtp_profiles(s)) == NULL) {
		SSLerror(s, SSL_R_EMPTY_SRTP_PROTECTION_PROFILE_LIST);
		return 0;
	}

	if ((ct = sk_SRTP_PROTECTION_PROFILE_num(clnt)) < 1) {
		SSLerror(s, SSL_R_EMPTY_SRTP_PROTECTION_PROFILE_LIST);
		return 0;
	}

	if (!CBB_add_u16_length_prefixed(cbb, &profiles))
		return 0;

	for (i = 0; i < ct; i++) {
		if ((prof = sk_SRTP_PROTECTION_PROFILE_value(clnt, i)) == NULL)
			return 0;
		if (!CBB_add_u16(&profiles, prof->id))
			return 0;
	}

	if (!CBB_add_u8_length_prefixed(cbb, &mki))
		return 0;

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_srtp_clienthello_parse(SSL *s, CBS *cbs, int *alert)
{
	SRTP_PROTECTION_PROFILE *cprof, *sprof;
	STACK_OF(SRTP_PROTECTION_PROFILE) *clnt = NULL, *srvr;
	int i, j;
	int ret;
	uint16_t id;
	CBS profiles, mki;

	ret = 0;

	if (!CBS_get_u16_length_prefixed(cbs, &profiles))
		goto err;
	if (CBS_len(&profiles) == 0 || CBS_len(&profiles) % 2 != 0)
		goto err;

	if ((clnt = sk_SRTP_PROTECTION_PROFILE_new_null()) == NULL)
		goto err;

	while (CBS_len(&profiles) > 0) {
		if (!CBS_get_u16(&profiles, &id))
			goto err;

		if (!srtp_find_profile_by_num(id, &cprof)) {
			if (!sk_SRTP_PROTECTION_PROFILE_push(clnt, cprof))
				goto err;
		}
	}

	if (!CBS_get_u8_length_prefixed(cbs, &mki) || CBS_len(&mki) != 0) {
		SSLerror(s, SSL_R_BAD_SRTP_MKI_VALUE);
		*alert = SSL_AD_DECODE_ERROR;
		goto done;
	}
	if (CBS_len(cbs) != 0)
		goto err;

	/*
	 * Per RFC 5764 section 4.1.1
	 *
	 * Find the server preferred profile using the client's list.
	 *
	 * The server MUST send a profile if it sends the use_srtp
	 * extension.  If one is not found, it should fall back to the
	 * negotiated DTLS cipher suite or return a DTLS alert.
	 */
	if ((srvr = SSL_get_srtp_profiles(s)) == NULL)
		goto err;
	for (i = 0; i < sk_SRTP_PROTECTION_PROFILE_num(srvr); i++) {
		if ((sprof = sk_SRTP_PROTECTION_PROFILE_value(srvr, i))
		    == NULL)
			goto err;

		for (j = 0; j < sk_SRTP_PROTECTION_PROFILE_num(clnt); j++) {
			if ((cprof = sk_SRTP_PROTECTION_PROFILE_value(clnt, j))
			    == NULL)
				goto err;

			if (cprof->id == sprof->id) {
				s->internal->srtp_profile = sprof;
				ret = 1;
				goto done;
			}
		}
	}

	/* If we didn't find anything, fall back to the negotiated */
	ret = 1;
	goto done;

 err:
	SSLerror(s, SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
	*alert = SSL_AD_DECODE_ERROR;

 done:
	sk_SRTP_PROTECTION_PROFILE_free(clnt);
	return ret;
}

int
tlsext_srtp_serverhello_needs(SSL *s)
{
	return SSL_IS_DTLS(s) && SSL_get_selected_srtp_profile(s) != NULL;
}

int
tlsext_srtp_serverhello_build(SSL *s, CBB *cbb)
{
	SRTP_PROTECTION_PROFILE *profile;
	CBB srtp, mki;

	if (!CBB_add_u16_length_prefixed(cbb, &srtp))
		return 0;

	if ((profile = SSL_get_selected_srtp_profile(s)) == NULL)
		return 0;

	if (!CBB_add_u16(&srtp, profile->id))
		return 0;

	if (!CBB_add_u8_length_prefixed(cbb, &mki))
		return 0;

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_srtp_serverhello_parse(SSL *s, CBS *cbs, int *alert)
{
	STACK_OF(SRTP_PROTECTION_PROFILE) *clnt;
	SRTP_PROTECTION_PROFILE *prof;
	int i;
	uint16_t id;
	CBS profile_ids, mki;

	if (!CBS_get_u16_length_prefixed(cbs, &profile_ids)) {
		SSLerror(s, SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
		goto err;
	}

	if (!CBS_get_u16(&profile_ids, &id) || CBS_len(&profile_ids) != 0) {
		SSLerror(s, SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
		goto err;
	}

	if (!CBS_get_u8_length_prefixed(cbs, &mki) || CBS_len(&mki) != 0) {
		SSLerror(s, SSL_R_BAD_SRTP_MKI_VALUE);
		*alert = SSL_AD_ILLEGAL_PARAMETER;
		return 0;
	}

	if ((clnt = SSL_get_srtp_profiles(s)) == NULL) {
		SSLerror(s, SSL_R_NO_SRTP_PROFILES);
		goto err;
	}

	for (i = 0; i < sk_SRTP_PROTECTION_PROFILE_num(clnt); i++) {
		if ((prof = sk_SRTP_PROTECTION_PROFILE_value(clnt, i))
		    == NULL) {
			SSLerror(s, SSL_R_NO_SRTP_PROFILES);
			goto err;
		}

		if (prof->id == id) {
			s->internal->srtp_profile = prof;
			return 1;
		}
	}

	SSLerror(s, SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
 err:
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

#endif /* OPENSSL_NO_SRTP */

struct tls_extension_funcs {
	int (*needs)(SSL *s);
	int (*build)(SSL *s, CBB *cbb);
	int (*parse)(SSL *s, CBS *cbs, int *alert);
};

struct tls_extension {
	uint16_t type;
	struct tls_extension_funcs clienthello;
	struct tls_extension_funcs serverhello;
};

static struct tls_extension tls_extensions[] = {
	{
		.type = TLSEXT_TYPE_server_name,
		.clienthello = {
			.needs = tlsext_sni_clienthello_needs,
			.build = tlsext_sni_clienthello_build,
			.parse = tlsext_sni_clienthello_parse,
		},
		.serverhello = {
			.needs = tlsext_sni_serverhello_needs,
			.build = tlsext_sni_serverhello_build,
			.parse = tlsext_sni_serverhello_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_renegotiate,
		.clienthello = {
			.needs = tlsext_ri_clienthello_needs,
			.build = tlsext_ri_clienthello_build,
			.parse = tlsext_ri_clienthello_parse,
		},
		.serverhello = {
			.needs = tlsext_ri_serverhello_needs,
			.build = tlsext_ri_serverhello_build,
			.parse = tlsext_ri_serverhello_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_status_request,
		.clienthello = {
			.needs = tlsext_ocsp_clienthello_needs,
			.build = tlsext_ocsp_clienthello_build,
			.parse = tlsext_ocsp_clienthello_parse,
		},
		.serverhello = {
			.needs = tlsext_ocsp_serverhello_needs,
			.build = tlsext_ocsp_serverhello_build,
			.parse = tlsext_ocsp_serverhello_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_ec_point_formats,
		.clienthello = {
			.needs = tlsext_ecpf_clienthello_needs,
			.build = tlsext_ecpf_clienthello_build,
			.parse = tlsext_ecpf_clienthello_parse,
		},
		.serverhello = {
			.needs = tlsext_ecpf_serverhello_needs,
			.build = tlsext_ecpf_serverhello_build,
			.parse = tlsext_ecpf_serverhello_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_supported_groups,
		.clienthello = {
			.needs = tlsext_supportedgroups_clienthello_needs,
			.build = tlsext_supportedgroups_clienthello_build,
			.parse = tlsext_supportedgroups_clienthello_parse,
		},
		.serverhello = {
			.needs = tlsext_supportedgroups_serverhello_needs,
			.build = tlsext_supportedgroups_serverhello_build,
			.parse = tlsext_supportedgroups_serverhello_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_session_ticket,
		.clienthello = {
			.needs = tlsext_sessionticket_clienthello_needs,
			.build = tlsext_sessionticket_clienthello_build,
			.parse = tlsext_sessionticket_clienthello_parse,
		},
		.serverhello = {
			.needs = tlsext_sessionticket_serverhello_needs,
			.build = tlsext_sessionticket_serverhello_build,
			.parse = tlsext_sessionticket_serverhello_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_signature_algorithms,
		.clienthello = {
			.needs = tlsext_sigalgs_clienthello_needs,
			.build = tlsext_sigalgs_clienthello_build,
			.parse = tlsext_sigalgs_clienthello_parse,
		},
		.serverhello = {
			.needs = tlsext_sigalgs_serverhello_needs,
			.build = tlsext_sigalgs_serverhello_build,
			.parse = tlsext_sigalgs_serverhello_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_application_layer_protocol_negotiation,
		.clienthello = {
			.needs = tlsext_alpn_clienthello_needs,
			.build = tlsext_alpn_clienthello_build,
			.parse = tlsext_alpn_clienthello_parse,
		},
		.serverhello = {
			.needs = tlsext_alpn_serverhello_needs,
			.build = tlsext_alpn_serverhello_build,
			.parse = tlsext_alpn_serverhello_parse,
		},
	},
#ifndef OPENSSL_NO_SRTP
	{
		.type = TLSEXT_TYPE_use_srtp,
		.clienthello = {
			.needs = tlsext_srtp_clienthello_needs,
			.build = tlsext_srtp_clienthello_build,
			.parse = tlsext_srtp_clienthello_parse,
		},
		.serverhello = {
			.needs = tlsext_srtp_serverhello_needs,
			.build = tlsext_srtp_serverhello_build,
			.parse = tlsext_srtp_serverhello_parse,
		},
	}
#endif /* OPENSSL_NO_SRTP */
};

#define N_TLS_EXTENSIONS (sizeof(tls_extensions) / sizeof(*tls_extensions))

/* Ensure that extensions fit in a uint32_t bitmask. */
CTASSERT(N_TLS_EXTENSIONS <= (sizeof(uint32_t) * 8));

static struct tls_extension *
tls_extension_find(uint16_t type, size_t *tls_extensions_idx)
{
	size_t i;

	for (i = 0; i < N_TLS_EXTENSIONS; i++) {
		if (tls_extensions[i].type == type) {
			*tls_extensions_idx = i;
			return &tls_extensions[i];
		}
	}

	return NULL;
}

static struct tls_extension_funcs *
tlsext_funcs(struct tls_extension *tlsext, int is_serverhello)
{
	if (is_serverhello)
		return &tlsext->serverhello;

	return &tlsext->clienthello;	
}

static int
tlsext_build(SSL *s, CBB *cbb, int is_serverhello)
{
	struct tls_extension_funcs *ext;
	struct tls_extension *tlsext;
	CBB extensions, extension_data;
	int extensions_present = 0;
	size_t i;

	if (!CBB_add_u16_length_prefixed(cbb, &extensions))
		return 0;

	for (i = 0; i < N_TLS_EXTENSIONS; i++) {
		tlsext = &tls_extensions[i];
		ext = tlsext_funcs(tlsext, is_serverhello);

		if (!ext->needs(s))
			continue;

		if (!CBB_add_u16(&extensions, tlsext->type))
			return 0;
		if (!CBB_add_u16_length_prefixed(&extensions, &extension_data))
			return 0;

		if (!ext->build(s, &extension_data))
			return 0;

		extensions_present = 1;
	}

	if (!extensions_present)
		CBB_discard_child(cbb);

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

static int
tlsext_parse(SSL *s, CBS *cbs, int *alert, int is_serverhello)
{
	struct tls_extension_funcs *ext;
	struct tls_extension *tlsext;
	CBS extensions, extension_data;
	uint32_t extensions_seen = 0;
	uint16_t type;
	size_t idx;

	/* An empty extensions block is valid. */
	if (CBS_len(cbs) == 0)
		return 1;

	*alert = SSL_AD_DECODE_ERROR;

	if (!CBS_get_u16_length_prefixed(cbs, &extensions))
		return 0;

	while (CBS_len(&extensions) > 0) {
		if (!CBS_get_u16(&extensions, &type))
			return 0;
		if (!CBS_get_u16_length_prefixed(&extensions, &extension_data))
			return 0;

		if (s->internal->tlsext_debug_cb != NULL)
			s->internal->tlsext_debug_cb(s, is_serverhello, type,
			    (unsigned char *)CBS_data(&extension_data),
			    CBS_len(&extension_data),
			    s->internal->tlsext_debug_arg);

		/* Unknown extensions are ignored. */
		if ((tlsext = tls_extension_find(type, &idx)) == NULL)
			continue;

		/* Check for duplicate known extensions. */
		if ((extensions_seen & (1 << idx)) != 0)
			return 0;
		extensions_seen |= (1 << idx);

		ext = tlsext_funcs(tlsext, is_serverhello);
		if (!ext->parse(s, &extension_data, alert))
			return 0;

		if (CBS_len(&extension_data) != 0)
			return 0;
	}

	return 1;
}

static void
tlsext_clienthello_reset_state(SSL *s)
{
	s->internal->servername_done = 0;
	s->tlsext_status_type = -1;
	S3I(s)->renegotiate_seen = 0;
	free(S3I(s)->alpn_selected);
	S3I(s)->alpn_selected = NULL;
	s->internal->srtp_profile = NULL;
}

int
tlsext_clienthello_build(SSL *s, CBB *cbb)
{
	return tlsext_build(s, cbb, 0);
}

int
tlsext_clienthello_parse(SSL *s, CBS *cbs, int *alert)
{
	/* XXX - this possibly should be done by the caller... */
	tlsext_clienthello_reset_state(s);

	return tlsext_parse(s, cbs, alert, 0);
}

static void
tlsext_serverhello_reset_state(SSL *s)
{
	S3I(s)->renegotiate_seen = 0;   
	free(S3I(s)->alpn_selected);
	S3I(s)->alpn_selected = NULL;
}

int
tlsext_serverhello_build(SSL *s, CBB *cbb)
{
	return tlsext_build(s, cbb, 1);
}

int
tlsext_serverhello_parse(SSL *s, CBS *cbs, int *alert)
{
	/* XXX - this possibly should be done by the caller... */
	tlsext_serverhello_reset_state(s);

	return tlsext_parse(s, cbs, alert, 1);
}
