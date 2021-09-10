/* $OpenBSD: ssl_tlsext.c,v 1.99 2021/09/10 09:25:29 tb Exp $ */
/*
 * Copyright (c) 2016, 2017, 2019 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2017 Doug Hogan <doug@openbsd.org>
 * Copyright (c) 2018-2019 Bob Beck <beck@openbsd.org>
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

#include <ctype.h>

#include <openssl/ocsp.h>
#include <openssl/opensslconf.h>

#include "bytestring.h"
#include "ssl_locl.h"
#include "ssl_sigalgs.h"
#include "ssl_tlsext.h"

/*
 * Supported Application-Layer Protocol Negotiation - RFC 7301
 */

int
tlsext_alpn_client_needs(SSL *s, uint16_t msg_type)
{
	/* ALPN protos have been specified and this is the initial handshake */
	return s->internal->alpn_client_proto_list != NULL &&
	    S3I(s)->hs.finished_len == 0;
}

int
tlsext_alpn_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
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
tlsext_alpn_server_parse(SSL *s, uint16_t msg_types, CBS *cbs, int *alert)
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

	/*
	 * XXX - A few things should be considered here:
	 * 1. Ensure that the same protocol is selected on session resumption.
	 * 2. Should the callback be called even if no ALPN extension was sent?
	 * 3. TLSv1.2 and earlier: ensure that SNI has already been processed.
	 */
	r = s->ctx->internal->alpn_select_cb(s, &selected, &selected_len,
	    CBS_data(&alpn), CBS_len(&alpn),
	    s->ctx->internal->alpn_select_cb_arg);

	if (r == SSL_TLSEXT_ERR_OK) {
		free(S3I(s)->alpn_selected);
		if ((S3I(s)->alpn_selected = malloc(selected_len)) == NULL) {
			S3I(s)->alpn_selected_len = 0;
			*alert = SSL_AD_INTERNAL_ERROR;
			return 0;
		}
		memcpy(S3I(s)->alpn_selected, selected, selected_len);
		S3I(s)->alpn_selected_len = selected_len;

		return 1;
	}

	/* On SSL_TLSEXT_ERR_NOACK behave as if no callback was present. */
	if (r == SSL_TLSEXT_ERR_NOACK)
		return 1;

	*alert = SSL_AD_NO_APPLICATION_PROTOCOL;
	SSLerror(s, SSL_R_NO_APPLICATION_PROTOCOL);

	return 0;

 err:
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

int
tlsext_alpn_server_needs(SSL *s, uint16_t msg_type)
{
	return S3I(s)->alpn_selected != NULL;
}

int
tlsext_alpn_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
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
tlsext_alpn_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	CBS list, proto;

	if (s->internal->alpn_client_proto_list == NULL) {
		*alert = SSL_AD_UNSUPPORTED_EXTENSION;
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
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

/*
 * Supported Groups - RFC 7919 section 2
 */
int
tlsext_supportedgroups_client_needs(SSL *s, uint16_t msg_type)
{
	return ssl_has_ecc_ciphers(s) ||
	    (S3I(s)->hs.our_max_tls_version >= TLS1_3_VERSION);
}

int
tlsext_supportedgroups_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
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
tlsext_supportedgroups_server_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert)
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

		if (S3I(s)->hs.tls13.hrr) {
			if (SSI(s)->tlsext_supportedgroups == NULL) {
				*alert = SSL_AD_HANDSHAKE_FAILURE;
				return 0;
			}
			/*
			 * In the case of TLSv1.3 the client cannot change
			 * the supported groups.
			 */
			if (groups_len != SSI(s)->tlsext_supportedgroups_length) {
				*alert = SSL_AD_ILLEGAL_PARAMETER;
				return 0;
			}
			for (i = 0; i < groups_len; i++) {
				uint16_t group;

				if (!CBS_get_u16(&grouplist, &group))
					goto err;
				if (SSI(s)->tlsext_supportedgroups[i] != group) {
					*alert = SSL_AD_ILLEGAL_PARAMETER;
					return 0;
				}
			}

			return 1;
		}

		if (SSI(s)->tlsext_supportedgroups != NULL)
			goto err;

		if ((groups = reallocarray(NULL, groups_len,
		    sizeof(uint16_t))) == NULL) {
			*alert = SSL_AD_INTERNAL_ERROR;
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
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

/* This extension is never used by the server. */
int
tlsext_supportedgroups_server_needs(SSL *s, uint16_t msg_type)
{
	return 0;
}

int
tlsext_supportedgroups_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	return 0;
}

int
tlsext_supportedgroups_client_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert)
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
		*alert = SSL_AD_INTERNAL_ERROR;
		return 0;
	}

	return 1;
}

/*
 * Supported Point Formats Extension - RFC 4492 section 5.1.2
 */
static int
tlsext_ecpf_build(SSL *s, uint16_t msg_type, CBB *cbb)
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
tlsext_ecpf_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	CBS ecpf;

	if (!CBS_get_u8_length_prefixed(cbs, &ecpf))
		return 0;
	if (CBS_len(&ecpf) == 0)
		return 0;
	if (CBS_len(cbs) != 0)
		return 0;

	/* Must contain uncompressed (0) - RFC 8422, section 5.1.2. */
	if (!CBS_contains_zero_byte(&ecpf)) {
		SSLerror(s, SSL_R_TLS_INVALID_ECPOINTFORMAT_LIST);
		*alert = SSL_AD_ILLEGAL_PARAMETER;
		return 0;
	}

	if (!s->internal->hit) {
		if (!CBS_stow(&ecpf, &(SSI(s)->tlsext_ecpointformatlist),
		    &(SSI(s)->tlsext_ecpointformatlist_length))) {
			*alert = SSL_AD_INTERNAL_ERROR;
			return 0;
		}
	}

	return 1;
}

int
tlsext_ecpf_client_needs(SSL *s, uint16_t msg_type)
{
	return ssl_has_ecc_ciphers(s);
}

int
tlsext_ecpf_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	return tlsext_ecpf_build(s, msg_type, cbb);
}

int
tlsext_ecpf_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	return tlsext_ecpf_parse(s, msg_type, cbs, alert);
}

int
tlsext_ecpf_server_needs(SSL *s, uint16_t msg_type)
{
	return ssl_using_ecc_cipher(s);
}

int
tlsext_ecpf_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	return tlsext_ecpf_build(s, msg_type, cbb);
}

int
tlsext_ecpf_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	return tlsext_ecpf_parse(s, msg_type, cbs, alert);
}

/*
 * Renegotiation Indication - RFC 5746.
 */
int
tlsext_ri_client_needs(SSL *s, uint16_t msg_type)
{
	return (s->internal->renegotiate);
}

int
tlsext_ri_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
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
tlsext_ri_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
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
tlsext_ri_server_needs(SSL *s, uint16_t msg_type)
{
	return (S3I(s)->hs.negotiated_tls_version < TLS1_3_VERSION &&
	    S3I(s)->send_connection_binding);
}

int
tlsext_ri_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
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
tlsext_ri_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
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
		*alert = SSL_AD_INTERNAL_ERROR;
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
tlsext_sigalgs_client_needs(SSL *s, uint16_t msg_type)
{
	return (S3I(s)->hs.our_max_tls_version >= TLS1_2_VERSION);
}

int
tlsext_sigalgs_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	uint16_t tls_version = S3I(s)->hs.negotiated_tls_version;
	CBB sigalgs;

	if (msg_type == SSL_TLSEXT_MSG_CH)
		tls_version = S3I(s)->hs.our_min_tls_version;

	if (!CBB_add_u16_length_prefixed(cbb, &sigalgs))
		return 0;
	if (!ssl_sigalgs_build(tls_version, &sigalgs))
		return 0;
	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_sigalgs_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	CBS sigalgs;

	if (!CBS_get_u16_length_prefixed(cbs, &sigalgs))
		return 0;
	if (CBS_len(&sigalgs) % 2 != 0 || CBS_len(&sigalgs) > 64)
		return 0;
	if (!CBS_stow(&sigalgs, &S3I(s)->hs.sigalgs, &S3I(s)->hs.sigalgs_len))
		return 0;

	return 1;
}

int
tlsext_sigalgs_server_needs(SSL *s, uint16_t msg_type)
{
	return (S3I(s)->hs.negotiated_tls_version >= TLS1_3_VERSION);
}

int
tlsext_sigalgs_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	CBB sigalgs;

	if (!CBB_add_u16_length_prefixed(cbb, &sigalgs))
		return 0;
	if (!ssl_sigalgs_build(S3I(s)->hs.negotiated_tls_version, &sigalgs))
		return 0;
	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_sigalgs_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	CBS sigalgs;

	if (ssl_effective_tls_version(s) < TLS1_3_VERSION)
		return 0;

	if (!CBS_get_u16_length_prefixed(cbs, &sigalgs))
		return 0;
	if (CBS_len(&sigalgs) % 2 != 0 || CBS_len(&sigalgs) > 64)
		return 0;
	if (!CBS_stow(&sigalgs, &S3I(s)->hs.sigalgs, &S3I(s)->hs.sigalgs_len))
		return 0;

	return 1;
}

/*
 * Server Name Indication - RFC 6066, section 3.
 */
int
tlsext_sni_client_needs(SSL *s, uint16_t msg_type)
{
	return (s->tlsext_hostname != NULL);
}

int
tlsext_sni_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
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

/*
 * Validate that the CBS contains only a hostname consisting of RFC 5890
 * compliant A-labels (see RFC 6066 section 3). Not a complete check
 * since we don't parse punycode to verify its validity but limits to
 * correct structure and character set.
 */
int
tlsext_sni_is_valid_hostname(CBS *cbs)
{
	uint8_t prev, c = 0;
	int component = 0;
	CBS hostname;

	CBS_dup(cbs, &hostname);

	if (CBS_len(&hostname) > TLSEXT_MAXLEN_host_name)
		return 0;

	while(CBS_len(&hostname) > 0) {
		prev = c;
		if (!CBS_get_u8(&hostname, &c))
			return 0;
		/* Everything has to be ASCII, with no NUL byte. */
		if (!isascii(c) || c == '\0')
			return 0;
		/* It must be alphanumeric, a '-', or a '.' */
		if (!isalnum(c) && c != '-' && c != '.')
			return 0;
		/* '-' and '.' must not start a component or be at the end. */
		if (component == 0 || CBS_len(&hostname) == 0) {
			if (c == '-' || c == '.')
				return 0;
		}
		if (c == '.') {
			/* Components can not end with a dash. */
			if (prev == '-')
				return 0;
			/* Start new component */
			component = 0;
			continue;
		}
		/* Components must be 63 chars or less. */
		if (++component > 63)
			return 0;
	}

	return 1;
}

int
tlsext_sni_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	CBS server_name_list, host_name;
	uint8_t name_type;

	if (!CBS_get_u16_length_prefixed(cbs, &server_name_list))
		goto err;

	if (!CBS_get_u8(&server_name_list, &name_type))
		goto err;
	/*
	 * RFC 6066 section 3, only one type (host_name) is specified.
	 * We do not tolerate unknown types, neither does BoringSSL.
	 * other implementations appear more tolerant.
	 */
	if (name_type != TLSEXT_NAMETYPE_host_name) {
		*alert = SSL_AD_ILLEGAL_PARAMETER;
		goto err;
	}


	if (!CBS_get_u16_length_prefixed(&server_name_list, &host_name))
		goto err;
	/*
	 * RFC 6066 section 3 specifies a host name must be at least 1 byte
	 * so 0 length is a decode error.
	 */
	if (CBS_len(&host_name) < 1)
		goto err;

	if (!tlsext_sni_is_valid_hostname(&host_name)) {
		*alert = SSL_AD_ILLEGAL_PARAMETER;
		goto err;
	}

	if (s->internal->hit || S3I(s)->hs.tls13.hrr) {
		if (s->session->tlsext_hostname == NULL) {
			*alert = SSL_AD_UNRECOGNIZED_NAME;
			goto err;
		}
		if (!CBS_mem_equal(&host_name, s->session->tlsext_hostname,
		    strlen(s->session->tlsext_hostname))) {
			*alert = SSL_AD_UNRECOGNIZED_NAME;
			goto err;
		}
	} else {
		if (s->session->tlsext_hostname != NULL)
			goto err;
		if (!CBS_strdup(&host_name, &s->session->tlsext_hostname)) {
			*alert = SSL_AD_INTERNAL_ERROR;
			goto err;
		}
	}

	/*
	 * RFC 6066 section 3 forbids multiple host names with the same type,
	 * therefore we allow only one entry.
	 */
	if (CBS_len(&server_name_list) != 0) {
		*alert = SSL_AD_ILLEGAL_PARAMETER;
		goto err;
	}
	if (CBS_len(cbs) != 0)
		goto err;

	return 1;

 err:
	return 0;
}

int
tlsext_sni_server_needs(SSL *s, uint16_t msg_type)
{
	if (s->internal->hit)
		return 0;

	return (s->session->tlsext_hostname != NULL);
}

int
tlsext_sni_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	return 1;
}

int
tlsext_sni_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	if (s->tlsext_hostname == NULL || CBS_len(cbs) != 0) {
		*alert = SSL_AD_UNRECOGNIZED_NAME;
		return 0;
	}

	if (s->internal->hit) {
		if (s->session->tlsext_hostname == NULL) {
			*alert = SSL_AD_UNRECOGNIZED_NAME;
			return 0;
		}
		if (strcmp(s->tlsext_hostname,
		    s->session->tlsext_hostname) != 0) {
			*alert = SSL_AD_UNRECOGNIZED_NAME;
			return 0;
		}
	} else {
		if (s->session->tlsext_hostname != NULL) {
			*alert = SSL_AD_DECODE_ERROR;
			return 0;
		}
		if ((s->session->tlsext_hostname =
		    strdup(s->tlsext_hostname)) == NULL) {
			*alert = SSL_AD_INTERNAL_ERROR;
			return 0;
		}
	}

	return 1;
}


/*
 * Certificate Status Request - RFC 6066 section 8.
 */

int
tlsext_ocsp_client_needs(SSL *s, uint16_t msg_type)
{
	if (msg_type != SSL_TLSEXT_MSG_CH)
		return 0;

	return (s->tlsext_status_type == TLSEXT_STATUSTYPE_ocsp);
}

int
tlsext_ocsp_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
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
tlsext_ocsp_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	int alert_desc = SSL_AD_DECODE_ERROR;
	CBS respid_list, respid, exts;
	const unsigned char *p;
	uint8_t status_type;
	int ret = 0;

	if (msg_type != SSL_TLSEXT_MSG_CH)
		goto err;

	if (!CBS_get_u8(cbs, &status_type))
		goto err;
	if (status_type != TLSEXT_STATUSTYPE_ocsp) {
		/* ignore unknown status types */
		s->tlsext_status_type = -1;

		if (!CBS_skip(cbs, CBS_len(cbs))) {
			*alert = SSL_AD_INTERNAL_ERROR;
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
			alert_desc = SSL_AD_INTERNAL_ERROR;
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
			alert_desc = SSL_AD_INTERNAL_ERROR;
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
		*alert = alert_desc;
	return ret;
}

int
tlsext_ocsp_server_needs(SSL *s, uint16_t msg_type)
{
	if (S3I(s)->hs.negotiated_tls_version >= TLS1_3_VERSION &&
	    s->tlsext_status_type == TLSEXT_STATUSTYPE_ocsp &&
	    s->ctx->internal->tlsext_status_cb != NULL) {
		s->internal->tlsext_status_expected = 0;
		if (s->ctx->internal->tlsext_status_cb(s,
		    s->ctx->internal->tlsext_status_arg) == SSL_TLSEXT_ERR_OK &&
		    s->internal->tlsext_ocsp_resp_len > 0)
			s->internal->tlsext_status_expected = 1;
	}
	return s->internal->tlsext_status_expected;
}

int
tlsext_ocsp_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	CBB ocsp_response;

	if (S3I(s)->hs.negotiated_tls_version >= TLS1_3_VERSION) {
		if (!CBB_add_u8(cbb, TLSEXT_STATUSTYPE_ocsp))
			return 0;
		if (!CBB_add_u24_length_prefixed(cbb, &ocsp_response))
			return 0;
		if (!CBB_add_bytes(&ocsp_response,
		    s->internal->tlsext_ocsp_resp,
		    s->internal->tlsext_ocsp_resp_len))
			return 0;
		if (!CBB_flush(cbb))
			return 0;
	}
	return 1;
}

int
tlsext_ocsp_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	uint8_t status_type;
	CBS response;

	if (ssl_effective_tls_version(s) >= TLS1_3_VERSION) {
		if (msg_type == SSL_TLSEXT_MSG_CR) {
			/*
			 * RFC 8446, 4.4.2.1 - the server may request an OCSP
			 * response with an empty status_request.
			 */
			if (CBS_len(cbs) == 0)
				return 1;

			SSLerror(s, SSL_R_LENGTH_MISMATCH);
			return 0;
		}
		if (!CBS_get_u8(cbs, &status_type)) {
			SSLerror(s, SSL_R_LENGTH_MISMATCH);
			return 0;
		}
		if (status_type != TLSEXT_STATUSTYPE_ocsp) {
			SSLerror(s, SSL_R_UNSUPPORTED_STATUS_TYPE);
			return 0;
		}
		if (!CBS_get_u24_length_prefixed(cbs, &response)) {
			SSLerror(s, SSL_R_LENGTH_MISMATCH);
			return 0;
		}
		if (CBS_len(&response) > 65536) {
			SSLerror(s, SSL_R_DATA_LENGTH_TOO_LONG);
			return 0;
		}
		if (!CBS_stow(&response, &s->internal->tlsext_ocsp_resp,
		    &s->internal->tlsext_ocsp_resp_len)) {
			*alert = SSL_AD_INTERNAL_ERROR;
			return 0;
		}
	} else {
		if (s->tlsext_status_type == -1) {
			*alert = SSL_AD_UNSUPPORTED_EXTENSION;
			return 0;
		}
		/* Set flag to expect CertificateStatus message */
		s->internal->tlsext_status_expected = 1;
	}
	return 1;
}

/*
 * SessionTicket extension - RFC 5077 section 3.2
 */
int
tlsext_sessionticket_client_needs(SSL *s, uint16_t msg_type)
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
tlsext_sessionticket_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
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
tlsext_sessionticket_server_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert)
{
	if (s->internal->tls_session_ticket_ext_cb) {
		if (!s->internal->tls_session_ticket_ext_cb(s, CBS_data(cbs),
		    (int)CBS_len(cbs),
		    s->internal->tls_session_ticket_ext_cb_arg)) {
			*alert = SSL_AD_INTERNAL_ERROR;
			return 0;
		}
	}

	/* We need to signal that this was processed fully */
	if (!CBS_skip(cbs, CBS_len(cbs))) {
		*alert = SSL_AD_INTERNAL_ERROR;
		return 0;
	}

	return 1;
}

int
tlsext_sessionticket_server_needs(SSL *s, uint16_t msg_type)
{
	return (s->internal->tlsext_ticket_expected &&
	    !(SSL_get_options(s) & SSL_OP_NO_TICKET));
}

int
tlsext_sessionticket_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	/* Empty ticket */
	return 1;
}

int
tlsext_sessionticket_client_parse(SSL *s, uint16_t msg_type, CBS *cbs,
    int *alert)
{
	if (s->internal->tls_session_ticket_ext_cb) {
		if (!s->internal->tls_session_ticket_ext_cb(s, CBS_data(cbs),
		    (int)CBS_len(cbs),
		    s->internal->tls_session_ticket_ext_cb_arg)) {
			*alert = SSL_AD_INTERNAL_ERROR;
			return 0;
		}
	}

	if ((SSL_get_options(s) & SSL_OP_NO_TICKET) != 0 || CBS_len(cbs) > 0) {
		*alert = SSL_AD_UNSUPPORTED_EXTENSION;
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
tlsext_srtp_client_needs(SSL *s, uint16_t msg_type)
{
	return SSL_is_dtls(s) && SSL_get_srtp_profiles(s) != NULL;
}

int
tlsext_srtp_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	CBB profiles, mki;
	int ct, i;
	STACK_OF(SRTP_PROTECTION_PROFILE) *clnt = NULL;
	const SRTP_PROTECTION_PROFILE *prof;

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
tlsext_srtp_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	const SRTP_PROTECTION_PROFILE *cprof, *sprof;
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
tlsext_srtp_server_needs(SSL *s, uint16_t msg_type)
{
	return SSL_is_dtls(s) && SSL_get_selected_srtp_profile(s) != NULL;
}

int
tlsext_srtp_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
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
tlsext_srtp_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	STACK_OF(SRTP_PROTECTION_PROFILE) *clnt;
	const SRTP_PROTECTION_PROFILE *prof;
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

/*
 * TLSv1.3 Key Share - RFC 8446 section 4.2.8.
 */
int
tlsext_keyshare_client_needs(SSL *s, uint16_t msg_type)
{
	return (S3I(s)->hs.our_max_tls_version >= TLS1_3_VERSION);
}

int
tlsext_keyshare_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	CBB client_shares;

	if (!CBB_add_u16_length_prefixed(cbb, &client_shares))
		return 0;

	if (!tls13_key_share_public(S3I(s)->hs.tls13.key_share,
	    &client_shares))
		return 0;

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_keyshare_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	CBS client_shares, key_exchange;
	uint16_t group;

	if (!CBS_get_u16_length_prefixed(cbs, &client_shares))
		goto err;

	while (CBS_len(&client_shares) > 0) {

		/* Unpack client share. */
		if (!CBS_get_u16(&client_shares, &group))
			goto err;
		if (!CBS_get_u16_length_prefixed(&client_shares, &key_exchange))
			return 0;

		/*
		 * XXX - check key exchange against supported groups from client.
		 * XXX - check that groups only appear once.
		 */

		/*
		 * Ignore this client share if we're using earlier than TLSv1.3
		 * or we've already selected a key share.
		 */
		if (S3I(s)->hs.our_max_tls_version < TLS1_3_VERSION)
			continue;
		if (S3I(s)->hs.tls13.key_share != NULL)
			continue;

		/* XXX - consider implementing server preference. */
		if (!tls1_check_curve(s, group))
			continue;

		/* Decode and store the selected key share. */
		S3I(s)->hs.tls13.key_share = tls13_key_share_new(group);
		if (S3I(s)->hs.tls13.key_share == NULL)
			goto err;
		if (!tls13_key_share_peer_public(S3I(s)->hs.tls13.key_share,
		    group, &key_exchange))
			goto err;
	}

	return 1;

 err:
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

int
tlsext_keyshare_server_needs(SSL *s, uint16_t msg_type)
{
	return (S3I(s)->hs.negotiated_tls_version >= TLS1_3_VERSION &&
	    tlsext_extension_seen(s, TLSEXT_TYPE_key_share));
}

int
tlsext_keyshare_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	/* In the case of a HRR, we only send the server selected group. */
	if (S3I(s)->hs.tls13.hrr) {
		if (S3I(s)->hs.tls13.server_group == 0)
			return 0;
		return CBB_add_u16(cbb, S3I(s)->hs.tls13.server_group);
	}

	if (S3I(s)->hs.tls13.key_share == NULL)
		return 0;

	if (!tls13_key_share_public(S3I(s)->hs.tls13.key_share, cbb))
		return 0;

	return 1;
}

int
tlsext_keyshare_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	CBS key_exchange;
	uint16_t group;

	/* Unpack server share. */
	if (!CBS_get_u16(cbs, &group))
		goto err;

	if (CBS_len(cbs) == 0) {
		/* HRR does not include an actual key share. */
		/* XXX - we should know that we are in a HRR... */
		S3I(s)->hs.tls13.server_group = group;
		return 1;
	}

	if (!CBS_get_u16_length_prefixed(cbs, &key_exchange))
		return 0;

	if (S3I(s)->hs.tls13.key_share == NULL)
		return 0;

	if (!tls13_key_share_peer_public(S3I(s)->hs.tls13.key_share,
	    group, &key_exchange))
		goto err;

	return 1;

 err:
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

/*
 * Supported Versions - RFC 8446 section 4.2.1.
 */
int
tlsext_versions_client_needs(SSL *s, uint16_t msg_type)
{
	return (S3I(s)->hs.our_max_tls_version >= TLS1_3_VERSION);
}

int
tlsext_versions_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	uint16_t max, min;
	uint16_t version;
	CBB versions;

	max = S3I(s)->hs.our_max_tls_version;
	min = S3I(s)->hs.our_min_tls_version;

	if (!CBB_add_u8_length_prefixed(cbb, &versions))
		return 0;

	/* XXX - fix, but contiguous for now... */
	for (version = max; version >= min; version--) {
		if (!CBB_add_u16(&versions, version))
			return 0;
	}

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_versions_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	CBS versions;
	uint16_t version;
	uint16_t max, min;
	uint16_t matched_version = 0;

	max = S3I(s)->hs.our_max_tls_version;
	min = S3I(s)->hs.our_min_tls_version;

	if (!CBS_get_u8_length_prefixed(cbs, &versions))
		goto err;

	while (CBS_len(&versions) > 0) {
		if (!CBS_get_u16(&versions, &version))
			goto err;
		/*
		 * XXX What is below implements client preference, and
		 * ignores any server preference entirely.
		 */
		if (matched_version == 0 && version >= min && version <= max)
			matched_version = version;
	}

	if (matched_version > 0)  {
		/* XXX - this should be stored for later processing. */
		s->version = matched_version;
		return 1;
	}

	*alert = SSL_AD_PROTOCOL_VERSION;
	return 0;

 err:
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

int
tlsext_versions_server_needs(SSL *s, uint16_t msg_type)
{
	return (S3I(s)->hs.negotiated_tls_version >= TLS1_3_VERSION);
}

int
tlsext_versions_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	return CBB_add_u16(cbb, TLS1_3_VERSION);
}

int
tlsext_versions_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	uint16_t selected_version;

	if (!CBS_get_u16(cbs, &selected_version)) {
		*alert = SSL_AD_DECODE_ERROR;
		return 0;
	}

	/* XXX - need to fix for DTLS 1.3 */
	if (selected_version < TLS1_3_VERSION) {
		*alert = SSL_AD_ILLEGAL_PARAMETER;
		return 0;
	}

	/* XXX test between min and max once initialization code goes in */
	S3I(s)->hs.tls13.server_version = selected_version;

	return 1;
}


/*
 * Cookie - RFC 8446 section 4.2.2.
 */

int
tlsext_cookie_client_needs(SSL *s, uint16_t msg_type)
{
	return (S3I(s)->hs.our_max_tls_version >= TLS1_3_VERSION &&
	    S3I(s)->hs.tls13.cookie_len > 0 && S3I(s)->hs.tls13.cookie != NULL);
}

int
tlsext_cookie_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	CBB cookie;

	if (!CBB_add_u16_length_prefixed(cbb, &cookie))
		return 0;

	if (!CBB_add_bytes(&cookie, S3I(s)->hs.tls13.cookie,
	    S3I(s)->hs.tls13.cookie_len))
		return 0;

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_cookie_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	CBS cookie;

	if (!CBS_get_u16_length_prefixed(cbs, &cookie))
		goto err;

	if (CBS_len(&cookie) != S3I(s)->hs.tls13.cookie_len)
		goto err;

	/*
	 * Check provided cookie value against what server previously
	 * sent - client *MUST* send the same cookie with new CR after
	 * a cookie is sent by the server with an HRR.
	 */
	if (!CBS_mem_equal(&cookie, S3I(s)->hs.tls13.cookie,
	    S3I(s)->hs.tls13.cookie_len)) {
		/* XXX special cookie mismatch alert? */
		*alert = SSL_AD_ILLEGAL_PARAMETER;
		return 0;
	}

	return 1;

 err:
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

int
tlsext_cookie_server_needs(SSL *s, uint16_t msg_type)
{
	/*
	 * Server needs to set cookie value in tls13 handshake
	 * in order to send one, should only be sent with HRR.
	 */
	return (S3I(s)->hs.our_max_tls_version >= TLS1_3_VERSION &&
	    S3I(s)->hs.tls13.cookie_len > 0 && S3I(s)->hs.tls13.cookie != NULL);
}

int
tlsext_cookie_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	CBB cookie;

	/* XXX deduplicate with client code */

	if (!CBB_add_u16_length_prefixed(cbb, &cookie))
		return 0;

	if (!CBB_add_bytes(&cookie, S3I(s)->hs.tls13.cookie,
	    S3I(s)->hs.tls13.cookie_len))
		return 0;

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_cookie_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	CBS cookie;

	/*
	 * XXX This currently assumes we will not get a second
	 * HRR from a server with a cookie to process after accepting
	 * one from the server in the same handshake
	 */
	if (S3I(s)->hs.tls13.cookie != NULL ||
	    S3I(s)->hs.tls13.cookie_len != 0) {
		*alert = SSL_AD_ILLEGAL_PARAMETER;
		return 0;
	}

	if (!CBS_get_u16_length_prefixed(cbs, &cookie))
		goto err;

	if (!CBS_stow(&cookie, &S3I(s)->hs.tls13.cookie,
	    &S3I(s)->hs.tls13.cookie_len))
		goto err;

	return 1;

 err:
	*alert = SSL_AD_DECODE_ERROR;
	return 0;
}

struct tls_extension_funcs {
	int (*needs)(SSL *s, uint16_t msg_type);
	int (*build)(SSL *s, uint16_t msg_type, CBB *cbb);
	int (*parse)(SSL *s, uint16_t msg_type, CBS *cbs, int *alert);
};

struct tls_extension {
	uint16_t type;
	uint16_t messages;
	struct tls_extension_funcs client;
	struct tls_extension_funcs server;
};

static const struct tls_extension tls_extensions[] = {
	{
		.type = TLSEXT_TYPE_supported_versions,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_SH |
		    SSL_TLSEXT_MSG_HRR,
		.client = {
			.needs = tlsext_versions_client_needs,
			.build = tlsext_versions_client_build,
			.parse = tlsext_versions_client_parse,
		},
		.server = {
			.needs = tlsext_versions_server_needs,
			.build = tlsext_versions_server_build,
			.parse = tlsext_versions_server_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_key_share,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_SH |
		    SSL_TLSEXT_MSG_HRR,
		.client = {
			.needs = tlsext_keyshare_client_needs,
			.build = tlsext_keyshare_client_build,
			.parse = tlsext_keyshare_client_parse,
		},
		.server = {
			.needs = tlsext_keyshare_server_needs,
			.build = tlsext_keyshare_server_build,
			.parse = tlsext_keyshare_server_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_server_name,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_EE,
		.client = {
			.needs = tlsext_sni_client_needs,
			.build = tlsext_sni_client_build,
			.parse = tlsext_sni_client_parse,
		},
		.server = {
			.needs = tlsext_sni_server_needs,
			.build = tlsext_sni_server_build,
			.parse = tlsext_sni_server_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_renegotiate,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_SH,
		.client = {
			.needs = tlsext_ri_client_needs,
			.build = tlsext_ri_client_build,
			.parse = tlsext_ri_client_parse,
		},
		.server = {
			.needs = tlsext_ri_server_needs,
			.build = tlsext_ri_server_build,
			.parse = tlsext_ri_server_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_status_request,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_CR |
		    SSL_TLSEXT_MSG_CT,
		.client = {
			.needs = tlsext_ocsp_client_needs,
			.build = tlsext_ocsp_client_build,
			.parse = tlsext_ocsp_client_parse,
		},
		.server = {
			.needs = tlsext_ocsp_server_needs,
			.build = tlsext_ocsp_server_build,
			.parse = tlsext_ocsp_server_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_ec_point_formats,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_SH,
		.client = {
			.needs = tlsext_ecpf_client_needs,
			.build = tlsext_ecpf_client_build,
			.parse = tlsext_ecpf_client_parse,
		},
		.server = {
			.needs = tlsext_ecpf_server_needs,
			.build = tlsext_ecpf_server_build,
			.parse = tlsext_ecpf_server_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_supported_groups,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_EE,
		.client = {
			.needs = tlsext_supportedgroups_client_needs,
			.build = tlsext_supportedgroups_client_build,
			.parse = tlsext_supportedgroups_client_parse,
		},
		.server = {
			.needs = tlsext_supportedgroups_server_needs,
			.build = tlsext_supportedgroups_server_build,
			.parse = tlsext_supportedgroups_server_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_session_ticket,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_SH,
		.client = {
			.needs = tlsext_sessionticket_client_needs,
			.build = tlsext_sessionticket_client_build,
			.parse = tlsext_sessionticket_client_parse,
		},
		.server = {
			.needs = tlsext_sessionticket_server_needs,
			.build = tlsext_sessionticket_server_build,
			.parse = tlsext_sessionticket_server_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_signature_algorithms,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_CR,
		.client = {
			.needs = tlsext_sigalgs_client_needs,
			.build = tlsext_sigalgs_client_build,
			.parse = tlsext_sigalgs_client_parse,
		},
		.server = {
			.needs = tlsext_sigalgs_server_needs,
			.build = tlsext_sigalgs_server_build,
			.parse = tlsext_sigalgs_server_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_application_layer_protocol_negotiation,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_EE,
		.client = {
			.needs = tlsext_alpn_client_needs,
			.build = tlsext_alpn_client_build,
			.parse = tlsext_alpn_client_parse,
		},
		.server = {
			.needs = tlsext_alpn_server_needs,
			.build = tlsext_alpn_server_build,
			.parse = tlsext_alpn_server_parse,
		},
	},
	{
		.type = TLSEXT_TYPE_cookie,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_HRR,
		.client = {
			.needs = tlsext_cookie_client_needs,
			.build = tlsext_cookie_client_build,
			.parse = tlsext_cookie_client_parse,
		},
		.server = {
			.needs = tlsext_cookie_server_needs,
			.build = tlsext_cookie_server_build,
			.parse = tlsext_cookie_server_parse,
		},
	},
#ifndef OPENSSL_NO_SRTP
	{
		.type = TLSEXT_TYPE_use_srtp,
		.messages = SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_SH /* XXX */ |
		    SSL_TLSEXT_MSG_EE,
		.client = {
			.needs = tlsext_srtp_client_needs,
			.build = tlsext_srtp_client_build,
			.parse = tlsext_srtp_client_parse,
		},
		.server = {
			.needs = tlsext_srtp_server_needs,
			.build = tlsext_srtp_server_build,
			.parse = tlsext_srtp_server_parse,
		},
	}
#endif /* OPENSSL_NO_SRTP */
};

#define N_TLS_EXTENSIONS (sizeof(tls_extensions) / sizeof(*tls_extensions))

/* Ensure that extensions fit in a uint32_t bitmask. */
CTASSERT(N_TLS_EXTENSIONS <= (sizeof(uint32_t) * 8));

const struct tls_extension *
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

int
tlsext_extension_seen(SSL *s, uint16_t type)
{
	size_t idx;

	if (tls_extension_find(type, &idx) == NULL)
		return 0;
	return ((S3I(s)->hs.extensions_seen & (1 << idx)) != 0);
}

static const struct tls_extension_funcs *
tlsext_funcs(const struct tls_extension *tlsext, int is_server)
{
	if (is_server)
		return &tlsext->server;

	return &tlsext->client;
}

static int
tlsext_build(SSL *s, int is_server, uint16_t msg_type, CBB *cbb)
{
	const struct tls_extension_funcs *ext;
	const struct tls_extension *tlsext;
	CBB extensions, extension_data;
	int extensions_present = 0;
	uint16_t tls_version;
	size_t i;

	tls_version = ssl_effective_tls_version(s);

	if (!CBB_add_u16_length_prefixed(cbb, &extensions))
		return 0;

	for (i = 0; i < N_TLS_EXTENSIONS; i++) {
		tlsext = &tls_extensions[i];
		ext = tlsext_funcs(tlsext, is_server);

		/* RFC 8446 Section 4.2 */
		if (tls_version >= TLS1_3_VERSION &&
		    !(tlsext->messages & msg_type))
			continue;

		if (!ext->needs(s, msg_type))
			continue;

		if (!CBB_add_u16(&extensions, tlsext->type))
			return 0;
		if (!CBB_add_u16_length_prefixed(&extensions, &extension_data))
			return 0;

		if (!ext->build(s, msg_type, &extension_data))
			return 0;

		extensions_present = 1;
	}

	if (!extensions_present &&
	    (msg_type & (SSL_TLSEXT_MSG_CH | SSL_TLSEXT_MSG_SH)) != 0)
		CBB_discard_child(cbb);

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

int
tlsext_clienthello_hash_extension(SSL *s, uint16_t type, CBS *cbs)
{
	/*
	 * RFC 8446 4.1.2. For subsequent CH, early data will be removed,
	 * cookie may be added, padding may be removed.
	 */
	struct tls13_ctx *ctx = s->internal->tls13;

	if (type == TLSEXT_TYPE_early_data || type == TLSEXT_TYPE_cookie ||
	    type == TLSEXT_TYPE_padding)
		return 1;
	if (!tls13_clienthello_hash_update_bytes(ctx, (void *)&type,
	    sizeof(type)))
		return 0;
	/*
	 * key_share data may be changed, and pre_shared_key data may
	 * be changed
	 */
	if (type == TLSEXT_TYPE_pre_shared_key || type == TLSEXT_TYPE_key_share)
		return 1;
	if (!tls13_clienthello_hash_update(ctx, cbs))
		return 0;

	return 1;
}

static int
tlsext_parse(SSL *s, int is_server, uint16_t msg_type, CBS *cbs, int *alert)
{
	const struct tls_extension_funcs *ext;
	const struct tls_extension *tlsext;
	CBS extensions, extension_data;
	uint16_t type;
	size_t idx;
	uint16_t tls_version;
	int alert_desc;

	tls_version = ssl_effective_tls_version(s);

	S3I(s)->hs.extensions_seen = 0;

	/* An empty extensions block is valid. */
	if (CBS_len(cbs) == 0)
		return 1;

	alert_desc = SSL_AD_DECODE_ERROR;

	if (!CBS_get_u16_length_prefixed(cbs, &extensions))
		goto err;

	while (CBS_len(&extensions) > 0) {
		if (!CBS_get_u16(&extensions, &type))
			goto err;
		if (!CBS_get_u16_length_prefixed(&extensions, &extension_data))
			goto err;

		if (s->internal->tlsext_debug_cb != NULL)
			s->internal->tlsext_debug_cb(s, !is_server, type,
			    (unsigned char *)CBS_data(&extension_data),
			    CBS_len(&extension_data),
			    s->internal->tlsext_debug_arg);

		/* Unknown extensions are ignored. */
		if ((tlsext = tls_extension_find(type, &idx)) == NULL)
			continue;

		if (tls_version >= TLS1_3_VERSION && is_server &&
		    msg_type == SSL_TLSEXT_MSG_CH) {
			if (!tlsext_clienthello_hash_extension(s, type,
			    &extension_data))
				goto err;
		}

		/* RFC 8446 Section 4.2 */
		if (tls_version >= TLS1_3_VERSION &&
		    !(tlsext->messages & msg_type)) {
			alert_desc = SSL_AD_ILLEGAL_PARAMETER;
			goto err;
		}

		/* Check for duplicate known extensions. */
		if ((S3I(s)->hs.extensions_seen & (1 << idx)) != 0)
			goto err;
		S3I(s)->hs.extensions_seen |= (1 << idx);

		ext = tlsext_funcs(tlsext, is_server);
		if (!ext->parse(s, msg_type, &extension_data, &alert_desc))
			goto err;

		if (CBS_len(&extension_data) != 0)
			goto err;
	}

	return 1;

 err:
	*alert = alert_desc;

	return 0;
}

static void
tlsext_server_reset_state(SSL *s)
{
	s->tlsext_status_type = -1;
	S3I(s)->renegotiate_seen = 0;
	free(S3I(s)->alpn_selected);
	S3I(s)->alpn_selected = NULL;
	S3I(s)->alpn_selected_len = 0;
	s->internal->srtp_profile = NULL;
}

int
tlsext_server_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	return tlsext_build(s, 1, msg_type, cbb);
}

int
tlsext_server_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	/* XXX - this should be done by the caller... */
	if (msg_type == SSL_TLSEXT_MSG_CH)
		tlsext_server_reset_state(s);

	return tlsext_parse(s, 1, msg_type, cbs, alert);
}

static void
tlsext_client_reset_state(SSL *s)
{
	S3I(s)->renegotiate_seen = 0;
	free(S3I(s)->alpn_selected);
	S3I(s)->alpn_selected = NULL;
	S3I(s)->alpn_selected_len = 0;
}

int
tlsext_client_build(SSL *s, uint16_t msg_type, CBB *cbb)
{
	return tlsext_build(s, 0, msg_type, cbb);
}

int
tlsext_client_parse(SSL *s, uint16_t msg_type, CBS *cbs, int *alert)
{
	/* XXX - this should be done by the caller... */
	if (msg_type == SSL_TLSEXT_MSG_SH)
		tlsext_client_reset_state(s);

	return tlsext_parse(s, 0, msg_type, cbs, alert);
}
