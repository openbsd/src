/* $OpenBSD: tls13_internal.h,v 1.1 2018/11/07 19:43:12 beck Exp $ */
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

struct tls13_secret {
	uint8_t *data;
	size_t len;
};

/* RFC 8446 Section 7.1  Page 92 */
struct tls13_secrets {
	int resumption;
	int init_done;
	int early_done;
	int handshake_done;
	int schedule_done;
	int insecure; /* Set by tests */
	struct tls13_secret zeros;
	struct tls13_secret extracted_early;
	struct tls13_secret binder_key;
	struct tls13_secret client_early_traffic;
	struct tls13_secret early_exporter_master;
	struct tls13_secret derived_early;
	struct tls13_secret extracted_handshake;
	struct tls13_secret client_handshake_traffic;
	struct tls13_secret server_handshake_traffic;
	struct tls13_secret derived_handshake;
	struct tls13_secret extracted_master;
	struct tls13_secret client_application_traffic;
	struct tls13_secret server_application_traffic;
	struct tls13_secret exporter_master;
	struct tls13_secret resumption_master;
};

struct tls13_secrets *tls13_secrets_new(size_t hash_length);
void tls13_secrets_init(struct tls13_secrets *secrets, int resumption);
void tls13_secrets_destroy(struct tls13_secrets *secrets);

int tls13_derive_early_secrets(struct tls13_secrets *secrets,
    const EVP_MD *digest,uint8_t *psk, size_t psk_len,
    const struct tls13_secret *context);
int tls13_derive_handshake_secrets(struct tls13_secrets *secrets,
    const EVP_MD *digest, const uint8_t *ecdhe, size_t ecdhe_len,
    const struct tls13_secret *context);
int tls13_derive_application_secrets(struct tls13_secrets *secrets,
    const EVP_MD *digest, const struct tls13_secret *context);
