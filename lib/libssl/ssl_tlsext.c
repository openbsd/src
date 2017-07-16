/* $OpenBSD: ssl_tlsext.c,v 1.1 2017/07/16 18:14:37 jsing Exp $ */
/*
 * Copyright (c) 2016, 2017 Joel Sing <jsing@openbsd.org>
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

#include "ssl_locl.h"

#include "bytestring.h"
#include "ssl_tlsext.h"

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

	return 1;
}

struct tls_extension {
	uint16_t type;
	int (*clienthello_needs)(SSL *s);
	int (*clienthello_build)(SSL *s, CBB *cbb);
	int (*clienthello_parse)(SSL *s, CBS *cbs, int *alert);
	int (*serverhello_needs)(SSL *s);
	int (*serverhello_build)(SSL *s, CBB *cbb);
	int (*serverhello_parse)(SSL *s, CBS *cbs, int *alert);
};

static struct tls_extension tls_extensions[] = {
	{
		.type = TLSEXT_TYPE_server_name,
		.clienthello_needs = tlsext_sni_clienthello_needs,
		.clienthello_build = tlsext_sni_clienthello_build,
		.clienthello_parse = tlsext_sni_clienthello_parse,
		.serverhello_needs = tlsext_sni_serverhello_needs,
		.serverhello_build = tlsext_sni_serverhello_build,
		.serverhello_parse = tlsext_sni_serverhello_parse,
	},
};

#define N_TLS_EXTENSIONS (sizeof(tls_extensions) / sizeof(*tls_extensions))

int
tlsext_clienthello_build(SSL *s, CBB *cbb)
{
	struct tls_extension *tlsext;
	CBB extension_data;
	size_t i;

	memset(&extension_data, 0, sizeof(extension_data));

	for (i = 0; i < N_TLS_EXTENSIONS; i++) {
		tlsext = &tls_extensions[i];

		if (!tlsext->clienthello_needs(s))
			continue;
		
		if (!CBB_add_u16(cbb, tlsext->type))
			return 0;
		if (!CBB_add_u16_length_prefixed(cbb, &extension_data))
			return 0;
		if (!tls_extensions[i].clienthello_build(s, &extension_data))
			return 0;
		if (!CBB_flush(cbb))
			return 0;
	}

	return 1;
}

int
tlsext_clienthello_parse_one(SSL *s, CBS *cbs, uint16_t type, int *alert)
{
	struct tls_extension *tlsext;
	size_t i;

	for (i = 0; i < N_TLS_EXTENSIONS; i++) {
		tlsext = &tls_extensions[i];

		if (tlsext->type != type)
			continue;
		if (!tlsext->clienthello_parse(s, cbs, alert))
			return 0;
		if (CBS_len(cbs) != 0) {
			*alert = SSL_AD_DECODE_ERROR;
			return 0;
		}

		return 1;
	}

	/* Not found. */
	return 2;
}

int
tlsext_serverhello_build(SSL *s, CBB *cbb)
{
	struct tls_extension *tlsext;
	CBB extension_data;
	size_t i;

	memset(&extension_data, 0, sizeof(extension_data));

	for (i = 0; i < N_TLS_EXTENSIONS; i++) {
		tlsext = &tls_extensions[i];

		if (!tlsext->serverhello_needs(s))
			continue;
		
		if (!CBB_add_u16(cbb, tlsext->type))
			return 0;
		if (!CBB_add_u16_length_prefixed(cbb, &extension_data))
			return 0;
		if (!tlsext->serverhello_build(s, &extension_data))
			return 0;
		if (!CBB_flush(cbb))
			return 0;
	}

	return 1;
}

int
tlsext_serverhello_parse_one(SSL *s, CBS *cbs, uint16_t type, int *alert)
{
	struct tls_extension *tlsext;
	size_t i;

	for (i = 0; i < N_TLS_EXTENSIONS; i++) {
		tlsext = &tls_extensions[i];

		if (tlsext->type != type)
			continue;
		if (!tlsext->serverhello_parse(s, cbs, alert))
			return 0;
		if (CBS_len(cbs) != 0) {
			*alert = SSL_AD_DECODE_ERROR;
			return 0;
		}

		return 1;
	}

	/* Not found. */
	return 2;
}
