/* $OpenBSD: tls_peer.c,v 1.12 2026/09/20 17:26:14 beck Exp $ */
/*
 * Copyright (c) 2015 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2015 Bob Beck <beck@openbsd.org>
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

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <tls.h>

#include "tls_internal.h"

const char *
tls_peer_cert_common_name(struct tls *ctx)
{
	if (ctx->conninfo == NULL)
		return (NULL);
	return (ctx->conninfo->common_name);
}

const char *
tls_peer_cert_hash(struct tls *ctx)
{
	if (ctx->conninfo == NULL)
		return (NULL);
	return (ctx->conninfo->hash);
}
const char *
tls_peer_cert_issuer(struct tls *ctx)
{
	if (ctx->conninfo == NULL)
		return (NULL);
	return (ctx->conninfo->issuer);
}

const char *
tls_peer_cert_subject(struct tls *ctx)
{
	if (ctx->conninfo == NULL)
		return (NULL);
	return (ctx->conninfo->subject);
}

int
tls_peer_cert_provided(struct tls *ctx)
{
	return (ctx->ssl_peer_cert != NULL);
}

int
tls_peer_cert_contains_name(struct tls *ctx, const char *name)
{
	int match;

	if (ctx->ssl_peer_cert == NULL)
		return (0);

	if (tls_check_name(ctx, ctx->ssl_peer_cert, name, &match) == -1)
		return (0);

	return (match);
}

time_t
tls_peer_cert_notbefore(struct tls *ctx)
{
	if (ctx->ssl_peer_cert == NULL)
		return (-1);
	if (ctx->conninfo == NULL)
		return (-1);
	return (ctx->conninfo->notbefore);
}

time_t
tls_peer_cert_notafter(struct tls *ctx)
{
	if (ctx->ssl_peer_cert == NULL)
		return (-1);
	if (ctx->conninfo == NULL)
		return (-1);
	return (ctx->conninfo->notafter);
}

const uint8_t *
tls_peer_cert_unverified_bundle_pem(struct tls *ctx, size_t *size)
{
	if (ctx->ssl_peer_cert == NULL)
		return (NULL);
	if (ctx->conninfo == NULL)
		return (NULL);
	*size = ctx->conninfo->peer_unverified_bundle_len;
	return (ctx->conninfo->peer_unverified_bundle);
}

const uint8_t *
tls_peer_cert_chain_pem(struct tls *ctx, size_t *size)
{
	return tls_peer_cert_unverified_bundle_pem(ctx, size);
}

const uint8_t *
tls_peer_cert_verified_chain_pem(struct tls *ctx, size_t *size)
{
	if (ctx->ssl_peer_verified_chain == NULL)
		return (NULL);
	if (ctx->conninfo == NULL)
		return (NULL);
	*size = ctx->conninfo->peer_verified_chain_len;
	return (ctx->conninfo->peer_verified_chain);
}

