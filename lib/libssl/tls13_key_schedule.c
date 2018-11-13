/* $OpenBSD: tls13_key_schedule.c,v 1.7 2018/11/13 01:25:13 beck Exp $ */
/* Copyright (c) 2018, Bob Beck <beck@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>
#include <stdlib.h>

#include <openssl/hkdf.h>

#include "bytestring.h"
#include "tls13_internal.h"

void
tls13_secrets_destroy(struct tls13_secrets *secrets)
{
	if (secrets == NULL)
		return;

	/* you can never be too sure :) */
	freezero(secrets->zeros.data, secrets->zeros.len);
	freezero(secrets->empty_hash.data, secrets->empty_hash.len);

	freezero(secrets->extracted_early.data,
	    secrets->extracted_early.len);
	freezero(secrets->binder_key.data,
	    secrets->binder_key.len);
	freezero(secrets->client_early_traffic.data,
	    secrets->client_early_traffic.len);
	freezero(secrets->early_exporter_master.data,
	    secrets->early_exporter_master.len);
	freezero(secrets->derived_early.data,
	    secrets->derived_early.len);
	freezero(secrets->extracted_handshake.data,
	    secrets->extracted_handshake.len);
	freezero(secrets->client_handshake_traffic.data,
	    secrets->client_handshake_traffic.len);
	freezero(secrets->server_handshake_traffic.data,
	    secrets->server_handshake_traffic.len);
	freezero(secrets->derived_handshake.data,
	    secrets->derived_handshake.len);
	freezero(secrets->extracted_master.data,
	    secrets->extracted_master.len);
	freezero(secrets->client_application_traffic.data,
	    secrets->client_application_traffic.len);
	freezero(secrets->server_application_traffic.data,
	    secrets->server_application_traffic.len);
	freezero(secrets->exporter_master.data,
	    secrets->exporter_master.len);
	freezero(secrets->resumption_master.data,
	    secrets->resumption_master.len);

	freezero(secrets, sizeof(struct tls13_secrets));
}

/*
 * Allocate a set of secrets for a key schedule using
 * a size of hash_length from RFC 8446 section 7.1.
 */
struct tls13_secrets *
tls13_secrets_create(const EVP_MD *digest, int resumption)
{
	struct tls13_secrets *secrets = NULL;
	EVP_MD_CTX *mdctx = NULL;
	unsigned int mdlen;
	size_t hash_length;

	hash_length = EVP_MD_size(digest);

	if ((secrets = calloc(1, sizeof(struct tls13_secrets))) == NULL)
		goto err;

	if ((secrets->zeros.data = calloc(hash_length, sizeof(uint8_t))) ==
	    NULL)
		goto err;
	secrets->zeros.len = hash_length;

	if ((secrets->empty_hash.data = malloc(hash_length)) == NULL)
		goto err;
	secrets->empty_hash.len = hash_length;

	if ((secrets->extracted_early.data = malloc(hash_length)) == NULL)
		goto err;
	secrets->extracted_early.len = hash_length;
	if ((secrets->binder_key.data = malloc(hash_length)) == NULL)
		goto err;
	secrets->binder_key.len = hash_length;
	if ((secrets->client_early_traffic.data = malloc(hash_length)) == NULL)
		goto err;
	secrets->client_early_traffic.len = hash_length;
	if ((secrets->early_exporter_master.data = malloc(hash_length)) ==
	    NULL)
		goto err;
	secrets->early_exporter_master.len = hash_length;
	if ((secrets->derived_early.data = malloc(hash_length)) == NULL)
		goto err;
	secrets->derived_early.len = hash_length;
	if ((secrets->extracted_handshake.data = malloc(hash_length)) == NULL)
		goto err;
	secrets->extracted_handshake.len = hash_length;
	if ((secrets->client_handshake_traffic.data = malloc(hash_length))
	    == NULL)
		goto err;
	secrets->client_handshake_traffic.len = hash_length;
	if ((secrets->server_handshake_traffic.data = malloc(hash_length))
	    == NULL)
		goto err;
	secrets->server_handshake_traffic.len = hash_length;
	if ((secrets->derived_handshake.data = malloc(hash_length)) == NULL)
		goto err;
	secrets->derived_handshake.len = hash_length;
	if ((secrets->extracted_master.data = malloc(hash_length)) == NULL)
		goto err;
	secrets->extracted_master.len = hash_length;
	if ((secrets->client_application_traffic.data = malloc(hash_length)) ==
	    NULL)
		goto err;
	secrets->client_application_traffic.len = hash_length;
	if ((secrets->server_application_traffic.data = malloc(hash_length)) ==
	    NULL)
		goto err;
	secrets->server_application_traffic.len = hash_length;
	if ((secrets->exporter_master.data = malloc(hash_length)) == NULL)
		goto err;
	secrets->exporter_master.len = hash_length;
	if ((secrets->resumption_master.data = malloc(hash_length)) == NULL)
		goto err;
	secrets->resumption_master.len = hash_length;

	/*
	 * Calculate the hash of a zero-length string - this is needed during
	 * the "derived" step for key extraction.
	 */
	if ((mdctx = EVP_MD_CTX_new()) == NULL)
		goto err;
	if (!EVP_DigestInit_ex(mdctx, digest, NULL))
		goto err;
	if (!EVP_DigestUpdate(mdctx, secrets->zeros.data, 0))
		goto err;
	if (!EVP_DigestFinal_ex(mdctx, secrets->empty_hash.data, &mdlen))
		goto err;
	EVP_MD_CTX_free(mdctx);
	mdctx = NULL;

	if (secrets->empty_hash.len != mdlen)
		goto err;

	secrets->digest = digest;
	secrets->resumption = resumption;
	secrets->init_done = 1;

	return secrets;

 err:
	tls13_secrets_destroy(secrets);
	EVP_MD_CTX_free(mdctx);

	return NULL;
}

int
tls13_hkdf_expand_label(struct tls13_secret *out, const EVP_MD *digest,
    const struct tls13_secret *secret, const char *label,
    const struct tls13_secret *context)
{
	const char tls13_plabel[] = "tls13 ";
	uint8_t *hkdf_label;
	size_t hkdf_label_len;
	CBB cbb, child;
	int ret;

	if (!CBB_init(&cbb, 256))
		return 0;
	if (!CBB_add_u16(&cbb, out->len))
		goto err;
	if (!CBB_add_u8_length_prefixed(&cbb, &child))
		goto err;
	if (!CBB_add_bytes(&child, tls13_plabel, strlen(tls13_plabel)))
		goto err;
	if (!CBB_add_bytes(&child, label, strlen(label)))
		goto err;
	if (!CBB_add_u8_length_prefixed(&cbb, &child))
		goto err;
	if (!CBB_add_bytes(&child, context->data, context->len))
		goto err;
	if (!CBB_finish(&cbb, &hkdf_label, &hkdf_label_len))
		goto err;

	ret = HKDF_expand(out->data, out->len, digest, secret->data,
	    secret->len, hkdf_label, hkdf_label_len);

	free(hkdf_label);
	return(ret);
 err:
	CBB_cleanup(&cbb);
	return(0);
}

static int
tls13_derive_secret(struct tls13_secret *out, const EVP_MD *digest,
    const struct tls13_secret *secret, const char *label,
    const struct tls13_secret *context)
{
	return tls13_hkdf_expand_label(out, digest, secret, label, context);
}

int
tls13_derive_early_secrets(struct tls13_secrets *secrets,
    uint8_t *psk, size_t psk_len, const struct tls13_secret *context)
{
	if (!secrets->init_done || secrets->early_done)
		return 0;

	if (!HKDF_extract(secrets->extracted_early.data,
	    &secrets->extracted_early.len, secrets->digest, psk, psk_len,
	    secrets->zeros.data, secrets->zeros.len))
		return 0;

	if (secrets->extracted_early.len != secrets->zeros.len)
		return 0;

	if (!tls13_derive_secret(&secrets->binder_key, secrets->digest,
	    &secrets->extracted_early,
	    secrets->resumption ? "res binder" : "ext binder",
	    &secrets->empty_hash))
		return 0;
	if (!tls13_derive_secret(&secrets->client_early_traffic,
	    secrets->digest, &secrets->extracted_early, "c e traffic",
	    context))
		return 0;
	if (!tls13_derive_secret(&secrets->early_exporter_master,
	    secrets->digest, &secrets->extracted_early, "e exp master",
	    context))
		return 0;
	if (!tls13_derive_secret(&secrets->derived_early,
	    secrets->digest, &secrets->extracted_early, "derived",
	    &secrets->empty_hash))
		return 0;

	/* RFC 8446 recommends */
	if (!secrets->insecure)
		explicit_bzero(secrets->extracted_early.data,
		    secrets->extracted_early.len);
	secrets->early_done = 1;
	return 1;
}

int
tls13_derive_handshake_secrets(struct tls13_secrets *secrets,
    const uint8_t *ecdhe, size_t ecdhe_len,
    const struct tls13_secret *context)
{
	if (!secrets->init_done || !secrets->early_done ||
	    secrets->handshake_done)
		return 0;

	if (!HKDF_extract(secrets->extracted_handshake.data,
	    &secrets->extracted_handshake.len, secrets->digest,
	    ecdhe, ecdhe_len, secrets->derived_early.data,
	    secrets->derived_early.len))
		return 0;

	if (secrets->extracted_handshake.len != secrets->zeros.len)
		return 0;

	/* XXX */
	if (!secrets->insecure)
		explicit_bzero(secrets->derived_early.data,
		    secrets->derived_early.len);

	if (!tls13_derive_secret(&secrets->client_handshake_traffic,
	    secrets->digest, &secrets->extracted_handshake, "c hs traffic",
	    context))
		return 0;
	if (!tls13_derive_secret(&secrets->server_handshake_traffic,
	    secrets->digest, &secrets->extracted_handshake, "s hs traffic",
	    context))
		return 0;
	if (!tls13_derive_secret(&secrets->derived_handshake,
	    secrets->digest, &secrets->extracted_handshake, "derived",
	    &secrets->empty_hash))
		return 0;

	/* RFC 8446 recommends */
	if (!secrets->insecure)
		explicit_bzero(secrets->extracted_handshake.data,
		    secrets->extracted_handshake.len);

	secrets->handshake_done = 1;

	return 1;
}

int
tls13_derive_application_secrets(struct tls13_secrets *secrets,
    const struct tls13_secret *context)
{
	if (!secrets->init_done || !secrets->early_done ||
	    !secrets->handshake_done || secrets->schedule_done)
		return 0;

	if (!HKDF_extract(secrets->extracted_master.data,
	    &secrets->extracted_master.len, secrets->digest,
	    secrets->zeros.data, secrets->zeros.len,
	    secrets->derived_handshake.data, secrets->derived_handshake.len))
		return 0;

	if (secrets->extracted_master.len != secrets->zeros.len)
		return 0;

	/* XXX */
	if (!secrets->insecure)
		explicit_bzero(secrets->derived_handshake.data,
		    secrets->derived_handshake.len);

	if (!tls13_derive_secret(&secrets->client_application_traffic,
	    secrets->digest, &secrets->extracted_master, "c ap traffic",
	    context))
		return 0;
	if (!tls13_derive_secret(&secrets->server_application_traffic,
	    secrets->digest, &secrets->extracted_master, "s ap traffic",
	    context))
		return 0;
	if (!tls13_derive_secret(&secrets->exporter_master,
	    secrets->digest, &secrets->extracted_master, "exp master",
	    context))
		return 0;
	if (!tls13_derive_secret(&secrets->resumption_master,
	    secrets->digest, &secrets->extracted_master, "res master",
	    context))
		return 0;

	/* RFC 8446 recommends */
	if (!secrets->insecure)
		explicit_bzero(secrets->extracted_master.data,
		    secrets->extracted_master.len);

	secrets->schedule_done = 1;

	return 1;
}

int
tls13_update_client_traffic_secret(struct tls13_secrets *secrets)
{
	if (!secrets->init_done || !secrets->early_done ||
	    !secrets->handshake_done || !secrets->schedule_done)
		return 0;

	return tls13_hkdf_expand_label(&secrets->client_application_traffic,
	    secrets->digest, &secrets->client_application_traffic,
	    "traffic upd", &secrets->empty_hash);
}

int
tls13_update_server_traffic_secret(struct tls13_secrets *secrets)
{
	if (!secrets->init_done || !secrets->early_done ||
	    !secrets->handshake_done || !secrets->schedule_done)
		return 0;

	return tls13_hkdf_expand_label(&secrets->server_application_traffic,
	    secrets->digest, &secrets->server_application_traffic,
	    "traffic upd", &secrets->empty_hash);
}
