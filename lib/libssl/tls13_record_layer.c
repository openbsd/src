/* $OpenBSD: tls13_record_layer.c,v 1.9 2019/03/17 15:13:23 jsing Exp $ */
/*
 * Copyright (c) 2018, 2019 Joel Sing <jsing@openbsd.org>
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

#include <openssl/curve25519.h>

#include "tls13_internal.h"
#include "tls13_record.h"

struct tls13_record_layer {
	int change_cipher_spec_seen;
	int handshake_completed;

	/*
	 * Read and/or write channels are closed due to an alert being
	 * sent or received. In the case of an error alert both channels
	 * are closed, whereas in the case of a close notify only one
	 * channel is closed.
	 */
	int read_closed;
	int write_closed;

	struct tls13_record *rrec;
	struct tls13_record *wrec;

	/* Buffer containing plaintext from opened records. */
	uint8_t rbuf_content_type;
	uint8_t *rbuf;
	size_t rbuf_len;
	CBS rbuf_cbs;

	/* Record protection. */
	const EVP_MD *hash;
	const EVP_AEAD *aead;
	EVP_AEAD_CTX read_aead_ctx;
	EVP_AEAD_CTX write_aead_ctx;
	struct tls13_secret read_iv;
	struct tls13_secret write_iv;
	struct tls13_secret read_nonce;
	struct tls13_secret write_nonce;
	uint8_t read_seq_num[TLS13_RECORD_SEQ_NUM_LEN];
	uint8_t write_seq_num[TLS13_RECORD_SEQ_NUM_LEN];

	/* Record callbacks. */
	tls13_alert_cb alert_cb;
	tls13_post_handshake_cb post_handshake_cb;

	/* Wire read/write callbacks. */
	tls13_read_cb wire_read;
	tls13_write_cb wire_write;
	void *cb_arg;
};

static void
tls13_record_layer_rbuf_free(struct tls13_record_layer *rl)
{
	CBS_init(&rl->rbuf_cbs, NULL, 0);
	freezero(rl->rbuf, rl->rbuf_len);
	rl->rbuf = NULL;
	rl->rbuf_len = 0;
	rl->rbuf_content_type = 0;
}

static void
tls13_record_layer_rrec_free(struct tls13_record_layer *rl)
{
	tls13_record_free(rl->rrec);
	rl->rrec = NULL;
}

static void
tls13_record_layer_wrec_free(struct tls13_record_layer *rl)
{
	tls13_record_free(rl->wrec);
	rl->wrec = NULL;
}

struct tls13_record_layer *
tls13_record_layer_new(tls13_read_cb wire_read, tls13_write_cb wire_write,
    tls13_alert_cb alert_cb, tls13_post_handshake_cb post_handshake_cb,
    void *cb_arg)
{
	struct tls13_record_layer *rl;

	if ((rl = calloc(1, sizeof(struct tls13_record_layer))) == NULL)
		return NULL;

	rl->wire_read = wire_read;
	rl->wire_write = wire_write;
	rl->alert_cb = alert_cb;
	rl->post_handshake_cb = post_handshake_cb;
	rl->cb_arg = cb_arg;

	return rl;
}

void
tls13_record_layer_free(struct tls13_record_layer *rl)
{
	if (rl == NULL)
		return;

	tls13_record_layer_rbuf_free(rl);

	tls13_record_layer_rrec_free(rl);
	tls13_record_layer_wrec_free(rl);

	EVP_AEAD_CTX_cleanup(&rl->read_aead_ctx);
	EVP_AEAD_CTX_cleanup(&rl->write_aead_ctx);

	freezero(rl->read_iv.data, rl->read_iv.len);
	freezero(rl->write_iv.data, rl->write_iv.len);
	freezero(rl->read_nonce.data, rl->read_nonce.len);
	freezero(rl->write_nonce.data, rl->write_nonce.len);

	freezero(rl, sizeof(struct tls13_record_layer));
}

static int
tls13_record_layer_inc_seq_num(uint8_t *seq_num)
{
	size_t i;

	for (i = TLS13_RECORD_SEQ_NUM_LEN - 1; i > 0; i--) {
		if (++seq_num[i] != 0)
			break;
	}

	/* RFC 8446 section 5.3 - sequence numbers must not wrap. */
	return (i != 0 || seq_num[0] != 0);
}

static int
tls13_record_layer_update_nonce(struct tls13_secret *nonce,
    struct tls13_secret *iv, uint8_t *seq_num)
{
	ssize_t i, j;

	if (nonce->len != iv->len)
		return 0;

	/*
	 * RFC 8446 section 5.3 - sequence number is zero padded and XOR'd
	 * with the IV to produce a per-record nonce. The IV will also be
	 * at least 8-bytes in length.
	 */
	for (i = nonce->len - 1, j = TLS13_RECORD_SEQ_NUM_LEN - 1; i >= 0; i--, j--)
		nonce->data[i] = iv->data[i] ^ (j >= 0 ? seq_num[j] : 0);

	return 1;
}

void
tls13_record_layer_set_aead(struct tls13_record_layer *rl,
    const EVP_AEAD *aead)
{
	rl->aead = aead;
}

void
tls13_record_layer_set_hash(struct tls13_record_layer *rl,
    const EVP_MD *hash)
{
	rl->hash = hash;
}

void
tls13_record_layer_handshake_completed(struct tls13_record_layer *rl)
{
	rl->handshake_completed = 1;
}

static ssize_t
tls13_record_layer_process_alert(struct tls13_record_layer *rl)
{
	uint8_t alert_level, alert_desc;
	ssize_t ret = TLS13_IO_FAILURE;

	/*
	 * RFC 8446 - sections 5.1 and 6.
	 *
	 * A TLSv1.3 alert record can only contain a single alert - this means
	 * that processing the alert must consume all of the record. The alert
	 * will result in one of three things - continuation (user_cancelled),
	 * read channel closure (close_notify) or termination (all others).
	 */
	if (rl->rbuf == NULL)
		goto err;
	if (rl->rbuf_content_type != SSL3_RT_ALERT)
		goto err;

	if (!CBS_get_u8(&rl->rbuf_cbs, &alert_level))
		goto err; /* XXX - decode error alert. */
	if (!CBS_get_u8(&rl->rbuf_cbs, &alert_desc))
		goto err; /* XXX - decode error alert. */

	if (CBS_len(&rl->rbuf_cbs) != 0)
		goto err; /* XXX - decode error alert. */

	tls13_record_layer_rbuf_free(rl);

	/*
	 * Alert level is ignored for closure alerts (RFC 8446 section 6.1),
	 * however for error alerts (RFC 8446 section 6.2), the alert level
	 * must be specified as fatal.
	 */
	if (alert_desc == SSL_AD_CLOSE_NOTIFY) {
		rl->read_closed = 1;
		ret = TLS13_IO_EOF;
	} else if (alert_desc == SSL_AD_USER_CANCELLED) {
		/* Ignored at the record layer. */
		ret = TLS13_IO_WANT_POLLIN;
	} else if (alert_level == SSL3_AL_FATAL) {
		rl->read_closed = 1;
		rl->write_closed = 1;
		ret = TLS13_IO_EOF;
	} else {
		/* XXX - decode error alert. */
		return TLS13_IO_FAILURE;
	}

	rl->alert_cb(alert_desc, rl->cb_arg);

 err:
	return ret;
}

int
tls13_record_layer_send_alert(struct tls13_record_layer *rl,
    uint8_t alert_level, uint8_t alert_desc)
{
	/* XXX - implement. */
	return -1;
}

static int
tls13_record_layer_set_traffic_key(const EVP_AEAD *aead, EVP_AEAD_CTX *aead_ctx,
    const EVP_MD *hash, struct tls13_secret *iv, struct tls13_secret *nonce,
    struct tls13_secret *traffic_key)
{
	struct tls13_secret context = { .data = "", .len = 0 };
	struct tls13_secret key = { .data = NULL, .len = 0 };
	int ret = 0;

	freezero(iv->data, iv->len);
	iv->data = NULL;
	iv->len = 0;

	freezero(nonce->data, nonce->len);
	nonce->data = NULL;
	nonce->len = 0;

	if ((iv->data = calloc(1, EVP_AEAD_nonce_length(aead))) == NULL)
		goto err;
	iv->len = EVP_AEAD_nonce_length(aead);

	if ((nonce->data = calloc(1, EVP_AEAD_nonce_length(aead))) == NULL)
		goto err;
	nonce->len = EVP_AEAD_nonce_length(aead);

	if ((key.data = calloc(1, EVP_AEAD_key_length(aead))) == NULL)
		goto err;
	key.len = EVP_AEAD_key_length(aead);

	if (!tls13_hkdf_expand_label(iv, hash, traffic_key, "iv", &context))
		goto err;
	if (!tls13_hkdf_expand_label(&key, hash, traffic_key, "key", &context))
		goto err;

	if (!EVP_AEAD_CTX_init(aead_ctx, aead, key.data, key.len,
	    EVP_AEAD_DEFAULT_TAG_LENGTH, NULL))
		goto err;

	ret = 1;

 err:
	freezero(key.data, key.len);

	return ret;
}

int
tls13_record_layer_set_read_traffic_key(struct tls13_record_layer *rl,
    struct tls13_secret *read_key)
{
	memset(rl->read_seq_num, 0, TLS13_RECORD_SEQ_NUM_LEN);

	return tls13_record_layer_set_traffic_key(rl->aead, &rl->read_aead_ctx,
	    rl->hash, &rl->read_iv, &rl->read_nonce, read_key);
}

int
tls13_record_layer_set_write_traffic_key(struct tls13_record_layer *rl,
    struct tls13_secret *write_key)
{
	memset(rl->write_seq_num, 0, TLS13_RECORD_SEQ_NUM_LEN);

	return tls13_record_layer_set_traffic_key(rl->aead, &rl->write_aead_ctx,
	    rl->hash, &rl->write_iv, &rl->write_nonce, write_key);
}

static int
tls13_record_layer_open_record_plaintext(struct tls13_record_layer *rl)
{
	CBS cbs;

	if (rl->aead != NULL)
		return 0;

	/*
	 * We're still operating in plaintext mode, so just copy the
	 * content from the record to the plaintext buffer.
	 */
	if (!tls13_record_content(rl->rrec, &cbs))
		return 0;

	tls13_record_layer_rbuf_free(rl);

	if (!CBS_stow(&cbs, &rl->rbuf, &rl->rbuf_len))
		return 0;

	rl->rbuf_content_type = tls13_record_content_type(rl->rrec);

	CBS_init(&rl->rbuf_cbs, rl->rbuf, rl->rbuf_len);

	return 1;
}

static int
tls13_record_layer_open_record_protected(struct tls13_record_layer *rl)
{
	CBS header, enc_record;
	uint8_t *content = NULL;
	ssize_t content_len = 0;
	uint8_t content_type;
	size_t out_len;

	if (rl->aead == NULL)
		goto err;

	if (!tls13_record_header(rl->rrec, &header))
		goto err;
	if (!tls13_record_content(rl->rrec, &enc_record))
		goto err;

	if ((content = calloc(1, CBS_len(&enc_record))) == NULL)
		goto err;
	content_len = CBS_len(&enc_record);

	if (!tls13_record_layer_update_nonce(&rl->read_nonce, &rl->read_iv,
	    rl->read_seq_num))
		goto err;

	if (!EVP_AEAD_CTX_open(&rl->read_aead_ctx,
	    content, &out_len, content_len,
	    rl->read_nonce.data, rl->read_nonce.len,
	    CBS_data(&enc_record), CBS_len(&enc_record),
	    CBS_data(&header), CBS_len(&header)))
		goto err;

	if (!tls13_record_layer_inc_seq_num(rl->read_seq_num))
		goto err;

	/*
	 * The real content type is hidden at the end of the record content and
	 * it may be followed by padding that consists of one or more zeroes.
	 * Time to hunt for that elusive content type!
	 */
	/* XXX - CBS from end? CBS_get_end_u8()? */
	content_len = out_len - 1;
	while (content_len >= 0 && content[content_len] == 0)
		content_len--;
	if (content_len < 0)
		goto err;
	content_type = content[content_len];

	tls13_record_layer_rbuf_free(rl);

	rl->rbuf_content_type = content_type;
	rl->rbuf = content;
	rl->rbuf_len = content_len;

	CBS_init(&rl->rbuf_cbs, rl->rbuf, rl->rbuf_len);

	return 1;

 err:
	freezero(content, content_len);

	return 0;
}

static int
tls13_record_layer_open_record(struct tls13_record_layer *rl)
{
	if (rl->aead == NULL)
		return tls13_record_layer_open_record_plaintext(rl);

	return tls13_record_layer_open_record_protected(rl);
}

static int
tls13_record_layer_seal_record_plaintext(struct tls13_record_layer *rl,
    uint8_t content_type, const uint8_t *content, size_t content_len)
{
	uint8_t *data = NULL;
	size_t data_len = 0;
	uint16_t version;
	CBB cbb, body;

	if (rl->aead != NULL)
		return 0;

	/* XXX - TLS1_VERSION for first client hello... */
	version = TLS1_2_VERSION;

	/*
	 * We're still operating in plaintext mode, so just copy the
	 * content into the record.
	 */
	if (!CBB_init(&cbb, TLS13_RECORD_HEADER_LEN + content_len))
		goto err;

	if (!CBB_add_u8(&cbb, content_type))
		goto err;
	if (!CBB_add_u16(&cbb, version))
		goto err;
	if (!CBB_add_u16_length_prefixed(&cbb, &body))
		goto err;
	if (!CBB_add_bytes(&body, content, content_len))
		goto err;

	if (!CBB_finish(&cbb, &data, &data_len))
		goto err;

	if (!tls13_record_set_data(rl->wrec, data, data_len))
		goto err;

	return 1;

 err:
	CBB_cleanup(&cbb);
	freezero(data, data_len);

	return 0;
}

static int
tls13_record_layer_seal_record_protected(struct tls13_record_layer *rl,
    uint8_t content_type, const uint8_t *content, size_t content_len)
{
	uint8_t *data = NULL, *header = NULL, *inner = NULL;
	size_t data_len = 0, header_len = 0, inner_len = 0;
	uint8_t *enc_record;
	size_t enc_record_len;
	ssize_t ret = 0;
	size_t out_len;
	CBB cbb;

	if (rl->aead == NULL)
		return 0;

	memset(&cbb, 0, sizeof(cbb));

	/* Build inner plaintext. */
	if (!CBB_init(&cbb, content_len + 1))
		goto err;
	if (!CBB_add_bytes(&cbb, content, content_len))
		goto err;
	if (!CBB_add_u8(&cbb, content_type))
		goto err;
	/* XXX - padding? */
	if (!CBB_finish(&cbb, &inner, &inner_len))
		goto err;

	if (inner_len > TLS13_RECORD_MAX_INNER_PLAINTEXT_LEN)
		goto err;

	/* XXX EVP_AEAD_max_tag_len vs EVP_AEAD_CTX_tag_len. */
	enc_record_len = inner_len + EVP_AEAD_max_tag_len(rl->aead);
	if (enc_record_len > TLS13_RECORD_MAX_CIPHERTEXT_LEN)
		goto err;

	/* Build the record header. */
	if (!CBB_init(&cbb, TLS13_RECORD_HEADER_LEN))
		goto err;
	if (!CBB_add_u8(&cbb, SSL3_RT_APPLICATION_DATA))
		goto err;
	if (!CBB_add_u16(&cbb, TLS1_2_VERSION))
		goto err;
	if (!CBB_add_u16(&cbb, enc_record_len))
		goto err;
	if (!CBB_finish(&cbb, &header, &header_len))
		goto err;

	/* Build the actual record. */
	if (!CBB_init(&cbb, TLS13_RECORD_HEADER_LEN + enc_record_len))
		goto err;
	if (!CBB_add_bytes(&cbb, header, header_len))
		goto err;
	if (!CBB_add_space(&cbb, &enc_record, enc_record_len))
		goto err;
	if (!CBB_finish(&cbb, &data, &data_len))
		goto err;

	if (!tls13_record_layer_update_nonce(&rl->write_nonce,
	    &rl->write_iv, rl->write_seq_num))
		goto err;

	/*
	 * XXX - consider a EVP_AEAD_CTX_seal_iov() that takes an iovec...
	 * this would avoid a copy since the inner would be passed as two
	 * separate pieces.
	 */
	if (!EVP_AEAD_CTX_seal(&rl->write_aead_ctx,
	    enc_record, &out_len, enc_record_len,
	    rl->write_nonce.data, rl->write_nonce.len,
	    inner, inner_len, header, header_len))
		goto err;

	if (out_len != enc_record_len)
		goto err;

	if (!tls13_record_layer_inc_seq_num(rl->write_seq_num))
		goto err;

	if (!tls13_record_set_data(rl->wrec, data, data_len))
		goto err;

	data = NULL;
	data_len = 0;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	freezero(data, data_len);
	freezero(header, header_len);
	freezero(inner, inner_len);

	return ret;
}

static int
tls13_record_layer_seal_record(struct tls13_record_layer *rl,
    uint8_t content_type, const uint8_t *content, size_t content_len)
{
	tls13_record_layer_wrec_free(rl);

	if ((rl->wrec = tls13_record_new()) == NULL)
		return 0;

	if (rl->aead == NULL)
		return tls13_record_layer_seal_record_plaintext(rl,
		    content_type, content, content_len);

	return tls13_record_layer_seal_record_protected(rl, content_type,
	    content, content_len);
}

static ssize_t
tls13_record_layer_read_record(struct tls13_record_layer *rl)
{
	uint8_t content_type, ccs;
	ssize_t ret;
	CBS cbs;

	if (rl->rrec == NULL) {
		if ((rl->rrec = tls13_record_new()) == NULL)
			goto err;
	}

	if ((ret = tls13_record_recv(rl->rrec, rl->wire_read, rl->cb_arg)) <= 0)
		return ret;

	/* XXX - record version checks. */

	content_type = tls13_record_content_type(rl->rrec);

	/*
	 * Bag of hacks ahead... after the first ClientHello message has been
	 * sent or received and before the peer's Finished message has been
	 * received, we may receive an unencrypted ChangeCipherSpec record
	 * (see RFC 8446 section 5 and appendix D.4). This record must be
	 * ignored.
	 */
	if (content_type == SSL3_RT_CHANGE_CIPHER_SPEC) {
		/* XXX - need to check after ClientHello, before Finished. */
		if (rl->handshake_completed || rl->change_cipher_spec_seen) {
			/* XXX - unexpected message alert. */
			goto err;
		}
		if (!tls13_record_content(rl->rrec, &cbs)) {
			/* XXX - decode error alert. */
			goto err;
		}
		if (!CBS_get_u8(&cbs, &ccs)) {
			/* XXX - decode error alert. */
			goto err;
		}
		if (ccs != 1) {
			/* XXX - something alert. */
			goto err;
		}
		rl->change_cipher_spec_seen = 1;
		tls13_record_layer_rrec_free(rl);
		return TLS13_IO_WANT_POLLIN;
	}

	/*
	 * Once record protection is engaged, we should only receive
	 * protected application data messages (aside from the
	 * dummy ChangeCipherSpec messages, handled above).
	 */
	if (rl->aead != NULL && content_type != SSL3_RT_APPLICATION_DATA) {
		/* XXX - unexpected message alert. */
		goto err;
	}

	if (!tls13_record_layer_open_record(rl))
		goto err;

	tls13_record_layer_rrec_free(rl);

	switch (rl->rbuf_content_type) {
	case SSL3_RT_ALERT:
		return tls13_record_layer_process_alert(rl);

	case SSL3_RT_HANDSHAKE:
		break;

	case SSL3_RT_APPLICATION_DATA:
		if (!rl->handshake_completed) {
			/* XXX - unexpected message alert. */
			goto err;
		}
		break;

	default:
		/* XXX - unexpected message alert. */
		goto err;
	}

	return TLS13_IO_SUCCESS;

 err:
	return TLS13_IO_FAILURE;
}

ssize_t
tls13_record_layer_read(struct tls13_record_layer *rl, uint8_t content_type,
    uint8_t *buf, size_t n)
{
	ssize_t ret;

	if (rl->read_closed)
		return TLS13_IO_EOF;

	/* XXX - loop here with record and byte limits. */
	/* XXX - send alert... */

	/* If necessary, pull up the next record. */
	if (CBS_len(&rl->rbuf_cbs) == 0) {
		if ((ret = tls13_record_layer_read_record(rl)) <= 0)
			return ret;

		/* XXX - need to check record version. */
	}
	if (rl->rbuf_content_type != content_type) {
		/*
		 * Handshake content can appear as post-handshake messages (yup,
		 * the RFC reused the same content type...), which means we can
		 * be trying to read application data and need to handle a
		 * post-handshake handshake message instead...
		 */
		if (rl->rbuf_content_type == SSL3_RT_HANDSHAKE) {
			if (rl->handshake_completed) {
				/* XXX - call callback, drop for now... */
				tls13_record_layer_rbuf_free(rl);
				return TLS13_IO_WANT_POLLIN;
			}
		}

		/* XXX - unexpected message alert. */
		goto err;
	}

	if (n > CBS_len(&rl->rbuf_cbs))
		n = CBS_len(&rl->rbuf_cbs);

	/* XXX - CBS_memcpy? CBS_copy_bytes? */
	memcpy(buf, CBS_data(&rl->rbuf_cbs), n);
	if (!CBS_skip(&rl->rbuf_cbs, n))
		goto err;

	if (CBS_len(&rl->rbuf_cbs) == 0)
		tls13_record_layer_rbuf_free(rl);

	return n;

 err:
	return TLS13_IO_FAILURE;
}

static ssize_t
tls13_record_layer_write_record(struct tls13_record_layer *rl,
    uint8_t content_type, const uint8_t *content, size_t content_len)
{
	ssize_t ret;

	if (rl->write_closed)
		return TLS13_IO_EOF;

	/* See if there is an existing record and attempt to push it out... */
	if (rl->wrec != NULL) {
		if ((ret = tls13_record_send(rl->wrec, rl->wire_write,
		    rl->cb_arg)) <= 0)
			return ret;

		tls13_record_layer_wrec_free(rl);

		/* XXX - could be pushing out different data... */
		return content_len;
	}

	if (content_len > TLS13_RECORD_MAX_PLAINTEXT_LEN)
		goto err;

	if (!tls13_record_layer_seal_record(rl, content_type, content, content_len))
		goto err;

	if ((ret = tls13_record_send(rl->wrec, rl->wire_write, rl->cb_arg)) <= 0)
		return ret;

	tls13_record_layer_wrec_free(rl);

	return content_len;

 err:
	return TLS13_IO_FAILURE;
}

static ssize_t
tls13_record_layer_write(struct tls13_record_layer *rl, uint8_t content_type,
    const uint8_t *buf, size_t n)
{
	if (n > TLS13_RECORD_MAX_PLAINTEXT_LEN)
		n = TLS13_RECORD_MAX_PLAINTEXT_LEN;

	return tls13_record_layer_write_record(rl, content_type, buf, n);
}

ssize_t
tls13_read_handshake_data(struct tls13_record_layer *rl, uint8_t *buf, size_t n)
{
	return tls13_record_layer_read(rl, SSL3_RT_HANDSHAKE, buf, n);
}

ssize_t
tls13_write_handshake_data(struct tls13_record_layer *rl, const uint8_t *buf,
    size_t n)
{
	return tls13_record_layer_write(rl, SSL3_RT_HANDSHAKE, buf, n);
}

ssize_t
tls13_read_application_data(struct tls13_record_layer *rl, uint8_t *buf, size_t n)
{
	if (!rl->handshake_completed)
		return TLS13_IO_FAILURE;

	return tls13_record_layer_read(rl, SSL3_RT_APPLICATION_DATA, buf, n);
}

ssize_t
tls13_write_application_data(struct tls13_record_layer *rl, const uint8_t *buf,
    size_t n)
{
	if (!rl->handshake_completed)
		return TLS13_IO_FAILURE;

	return tls13_record_layer_write(rl, SSL3_RT_APPLICATION_DATA, buf, n);
}
