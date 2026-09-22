/* $OpenBSD: tls12_record.h,v 1.2 2026/09/22 00:38:51 jsing Exp $ */
/*
 * Copyright (c) 2019, 2026 Joel Sing <jsing@openbsd.org>
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

#ifndef HEADER_TLS12_RECORD_H
#define HEADER_TLS12_RECORD_H

#include <sys/types.h>

#include <stdint.h>

#include "bytestring.h"

__BEGIN_HIDDEN_DECLS

/*
 * TLSv1.2 Record Layer - RFC 5246 section 6.2.
 *
 * The maximum plaintext length is 2^14 and the maximum ciphertext length is
 * 2^14 + 2048, along with a 5 byte header, giving a maximum size of
 * 5 + 2^14 + 2048 = 18,437 bytes.
 */
#define TLS12_RECORD_HEADER_LEN			5
#define TLS12_RECORD_MAX_PLAINTEXT_LEN		16384
#define TLS12_RECORD_MAX_CIPHER_OVERHEAD	2048
#define TLS12_RECORD_MAX_CIPHERTEXT_LEN \
	(TLS12_RECORD_MAX_PLAINTEXT_LEN + TLS12_RECORD_MAX_CIPHER_OVERHEAD)
#define TLS12_RECORD_MAX_LEN \
	(TLS12_RECORD_HEADER_LEN + TLS12_RECORD_MAX_CIPHERTEXT_LEN)

/*
 * TLSv1.2 Per-Record Sequence Numbers - RFC 5246 section 6.1.
 */
#define TLS12_RECORD_SEQ_NUM_LEN 8

struct tls12_record;

struct tls12_record *tls12_record_new(void);
void tls12_record_free(struct tls12_record *_rec);
uint16_t tls12_record_version(struct tls12_record *_rec);
void tls12_record_data(struct tls12_record *_rec, CBS *_cbs);
int tls12_record_set_data(struct tls12_record *_rec, uint8_t *_data,
    size_t _data_len);
ssize_t tls12_record_recv(struct tls12_record *_rec, tls_read_cb _wire_read,
    void *_wire_arg);
ssize_t tls12_record_send(struct tls12_record *_rec, tls_write_cb _wire_write,
    void *_wire_arg);

__END_HIDDEN_DECLS

#endif
