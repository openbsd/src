/* $OpenBSD: tls12_record_layer.c,v 1.4 2020/09/16 17:15:01 jsing Exp $ */
/*
 * Copyright (c) 2020 Joel Sing <jsing@openbsd.org>
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

#include <stdlib.h>

#include <openssl/evp.h>

#include "ssl_locl.h"

struct tls12_record_layer {
	uint16_t version;
	int dtls;

	uint16_t read_epoch;
	uint16_t write_epoch;

	int read_stream_mac;
	int write_stream_mac;

	/*
	 * XXX - for now these are just pointers to externally managed
	 * structs/memory. These should eventually be owned by the record layer.
	 */
	SSL_AEAD_CTX *read_aead_ctx;
	SSL_AEAD_CTX *write_aead_ctx;

	EVP_CIPHER_CTX *read_cipher_ctx;
	EVP_MD_CTX *read_hash_ctx;
	EVP_CIPHER_CTX *write_cipher_ctx;
	EVP_MD_CTX *write_hash_ctx;

	uint8_t *read_seq_num;
	uint8_t *write_seq_num;
};

struct tls12_record_layer *
tls12_record_layer_new(void)
{
	struct tls12_record_layer *rl;

	if ((rl = calloc(1, sizeof(struct tls12_record_layer))) == NULL)
		return NULL;

	return rl;
}

void
tls12_record_layer_free(struct tls12_record_layer *rl)
{
	freezero(rl, sizeof(struct tls12_record_layer));
}

void
tls12_record_layer_set_version(struct tls12_record_layer *rl, uint16_t version)
{
	rl->version = version;
	rl->dtls = (version == DTLS1_VERSION);
}

void
tls12_record_layer_set_read_epoch(struct tls12_record_layer *rl, uint16_t epoch)
{
	rl->read_epoch = epoch;
}

void
tls12_record_layer_set_write_epoch(struct tls12_record_layer *rl, uint16_t epoch)
{
	rl->write_epoch = epoch;
}

static void
tls12_record_layer_set_read_state(struct tls12_record_layer *rl,
    SSL_AEAD_CTX *aead_ctx, EVP_CIPHER_CTX *cipher_ctx, EVP_MD_CTX *hash_ctx,
    int stream_mac)
{
	rl->read_aead_ctx = aead_ctx;

	rl->read_cipher_ctx = cipher_ctx;
	rl->read_hash_ctx = hash_ctx;
	rl->read_stream_mac = stream_mac;
}

static void
tls12_record_layer_set_write_state(struct tls12_record_layer *rl,
    SSL_AEAD_CTX *aead_ctx, EVP_CIPHER_CTX *cipher_ctx, EVP_MD_CTX *hash_ctx,
    int stream_mac)
{
	rl->write_aead_ctx = aead_ctx;

	rl->write_cipher_ctx = cipher_ctx;
	rl->write_hash_ctx = hash_ctx;
	rl->write_stream_mac = stream_mac;
}

void
tls12_record_layer_clear_read_state(struct tls12_record_layer *rl)
{
	tls12_record_layer_set_read_state(rl, NULL, NULL, NULL, 0);
	rl->read_seq_num = NULL;
}

void
tls12_record_layer_clear_write_state(struct tls12_record_layer *rl)
{
	tls12_record_layer_set_write_state(rl, NULL, NULL, NULL, 0);
	rl->write_seq_num = NULL;
}

void
tls12_record_layer_set_read_seq_num(struct tls12_record_layer *rl,
    uint8_t *seq_num)
{
	rl->read_seq_num = seq_num;
}

void
tls12_record_layer_set_write_seq_num(struct tls12_record_layer *rl,
    uint8_t *seq_num)
{
	rl->write_seq_num = seq_num;
}

int
tls12_record_layer_set_read_aead(struct tls12_record_layer *rl,
    SSL_AEAD_CTX *aead_ctx)
{
	tls12_record_layer_set_read_state(rl, aead_ctx, NULL, NULL, 0);

	return 1;
}

int
tls12_record_layer_set_write_aead(struct tls12_record_layer *rl,
    SSL_AEAD_CTX *aead_ctx)
{
	tls12_record_layer_set_write_state(rl, aead_ctx, NULL, NULL, 0);

	return 1;
}

int
tls12_record_layer_set_read_cipher_hash(struct tls12_record_layer *rl,
    EVP_CIPHER_CTX *cipher_ctx, EVP_MD_CTX *hash_ctx, int stream_mac)
{
	tls12_record_layer_set_read_state(rl, NULL, cipher_ctx, hash_ctx,
	    stream_mac);

	return 1;
}

int
tls12_record_layer_set_write_cipher_hash(struct tls12_record_layer *rl,
    EVP_CIPHER_CTX *cipher_ctx, EVP_MD_CTX *hash_ctx, int stream_mac)
{
	tls12_record_layer_set_write_state(rl, NULL, cipher_ctx, hash_ctx,
	    stream_mac);

	return 1;
}

static int
tls12_record_layer_build_seq_num(struct tls12_record_layer *rl, CBB *cbb,
    uint16_t epoch, uint8_t *seq_num, size_t seq_num_len)
{
	CBS seq;

	CBS_init(&seq, seq_num, seq_num_len);

	if (rl->dtls) {
		if (!CBB_add_u16(cbb, epoch))
			return 0;
		if (!CBS_skip(&seq, 2))
			return 0;
	}

	return CBB_add_bytes(cbb, CBS_data(&seq), CBS_len(&seq));
}

static int
tls12_record_layer_pseudo_header(struct tls12_record_layer *rl,
    uint8_t content_type, uint16_t record_len, uint16_t epoch, uint8_t *seq_num,
    size_t seq_num_len, uint8_t **out, size_t *out_len)
{
	CBB cbb;

	*out = NULL;
	*out_len = 0;

	/* Build the pseudo-header used for MAC/AEAD. */
	if (!CBB_init(&cbb, 13))
		goto err;

	if (!tls12_record_layer_build_seq_num(rl, &cbb, epoch,
	    seq_num, seq_num_len))
		goto err;
	if (!CBB_add_u8(&cbb, content_type))
		goto err;
	if (!CBB_add_u16(&cbb, rl->version))
		goto err;
	if (!CBB_add_u16(&cbb, record_len))
		goto err;

	if (!CBB_finish(&cbb, out, out_len))
		goto err;

	return 1;

 err:
	CBB_cleanup(&cbb);

	return 0;
}

static int
tls12_record_layer_mac(struct tls12_record_layer *rl, CBB *cbb,
    EVP_MD_CTX *hash_ctx, int stream_mac, uint16_t epoch, uint8_t *seq_num,
    size_t seq_num_len, uint8_t content_type, const uint8_t *content,
    size_t content_len, size_t *out_len)
{
	EVP_MD_CTX *mac_ctx = NULL;
	uint8_t *header = NULL;
	size_t header_len;
	size_t mac_len;
	uint8_t *mac;
	int ret = 0;

	if ((mac_ctx = EVP_MD_CTX_new()) == NULL)
		goto err;
	if (!EVP_MD_CTX_copy(mac_ctx, hash_ctx))
		goto err;

	if (!tls12_record_layer_pseudo_header(rl, content_type, content_len,
	    epoch, seq_num, seq_num_len, &header, &header_len))
		goto err;

	if (EVP_DigestSignUpdate(mac_ctx, header, header_len) <= 0)
		goto err;
	if (EVP_DigestSignUpdate(mac_ctx, content, content_len) <= 0)
		goto err;
	if (EVP_DigestSignFinal(mac_ctx, NULL, &mac_len) <= 0)
		goto err;
	if (!CBB_add_space(cbb, &mac, mac_len))
		goto err;
	if (EVP_DigestSignFinal(mac_ctx, mac, &mac_len) <= 0)
		goto err;

	if (stream_mac) {
		if (!EVP_MD_CTX_copy(hash_ctx, mac_ctx))
			goto err;
	}

	*out_len = mac_len;
	ret = 1;

 err:
	EVP_MD_CTX_free(mac_ctx);
	free(header);

	return ret;
}

static int
tls12_record_layer_write_mac(struct tls12_record_layer *rl, CBB *cbb,
    uint8_t content_type, const uint8_t *content, size_t content_len,
    size_t *out_len)
{
	return tls12_record_layer_mac(rl, cbb, rl->write_hash_ctx,
	    rl->write_stream_mac, rl->write_epoch, rl->write_seq_num,
	    SSL3_SEQUENCE_SIZE, content_type, content, content_len, out_len);
}

static int
tls12_record_layer_aead_concat_nonce(struct tls12_record_layer *rl,
    const SSL_AEAD_CTX *aead, uint8_t *seq_num, uint8_t **out, size_t *out_len)
{
	CBB cbb;

	if (aead->variable_nonce_len > SSL3_SEQUENCE_SIZE)
		return 0;

	/* Fixed nonce and variable nonce (sequence number) are concatenated. */
	if (!CBB_init(&cbb, 16))
		goto err;
	if (!CBB_add_bytes(&cbb, aead->fixed_nonce,
	    aead->fixed_nonce_len))
		goto err;
	if (!CBB_add_bytes(&cbb, seq_num, aead->variable_nonce_len))
		goto err;
	if (!CBB_finish(&cbb, out, out_len))
		goto err;

	return 1;

 err:
	CBB_cleanup(&cbb);

	return 0;
}

static int
tls12_record_layer_aead_xored_nonce(struct tls12_record_layer *rl,
    const SSL_AEAD_CTX *aead, uint8_t *seq_num, uint8_t **out, size_t *out_len)
{
	uint8_t *nonce = NULL;
	size_t nonce_len = 0;
	uint8_t *pad;
	CBB cbb;
	int i;

	if (aead->variable_nonce_len > SSL3_SEQUENCE_SIZE)
		return 0;
	if (aead->fixed_nonce_len < aead->variable_nonce_len)
		return 0;

	/*
	 * Variable nonce (sequence number) is right padded, before the fixed
	 * nonce is XOR'd in.
	 */
	if (!CBB_init(&cbb, 16))
		goto err;
	if (!CBB_add_space(&cbb, &pad,
	    aead->fixed_nonce_len - aead->variable_nonce_len))
		goto err;
	if (!CBB_add_bytes(&cbb, seq_num, aead->variable_nonce_len))
		goto err;
	if (!CBB_finish(&cbb, &nonce, &nonce_len))
		goto err;

	for (i = 0; i < aead->fixed_nonce_len; i++)
		nonce[i] ^= aead->fixed_nonce[i];

	*out = nonce;
	*out_len = nonce_len;

	return 1;

 err:
	CBB_cleanup(&cbb);
	freezero(nonce, nonce_len);

	return 0;
}

static int
tls12_record_layer_seal_record_plaintext(struct tls12_record_layer *rl,
    uint8_t content_type, const uint8_t *content, size_t content_len, CBB *out)
{
	if (rl->write_aead_ctx != NULL || rl->write_cipher_ctx != NULL)
		return 0;

	return CBB_add_bytes(out, content, content_len);
}

static int
tls12_record_layer_seal_record_protected_aead(struct tls12_record_layer *rl,
    uint8_t content_type, const uint8_t *content, size_t content_len, CBB *out)
{
	const SSL_AEAD_CTX *aead = rl->write_aead_ctx;
	uint8_t *header = NULL, *nonce = NULL;
	size_t header_len = 0, nonce_len = 0;
	size_t enc_record_len, out_len;
	uint16_t epoch = 0;
	uint8_t *enc_data;
	int ret = 0;

	/* XXX - move to nonce allocated in record layer, matching TLSv1.3 */
	if (aead->xor_fixed_nonce) {
		if (!tls12_record_layer_aead_xored_nonce(rl, aead,
		    rl->write_seq_num, &nonce, &nonce_len))
			goto err;
	} else {
		if (!tls12_record_layer_aead_concat_nonce(rl, aead,
		    rl->write_seq_num, &nonce, &nonce_len))
			goto err;
	}

	if (aead->variable_nonce_in_record) {
		/* XXX - length check? */
		if (!CBB_add_bytes(out, rl->write_seq_num, aead->variable_nonce_len))
			goto err;
	}

	if (!tls12_record_layer_pseudo_header(rl, content_type, content_len,
	    epoch, rl->write_seq_num, SSL3_SEQUENCE_SIZE, &header, &header_len))
		goto err;

	/* XXX EVP_AEAD_max_tag_len vs EVP_AEAD_CTX_tag_len. */
	enc_record_len = content_len + aead->tag_len;
	if (enc_record_len > SSL3_RT_MAX_ENCRYPTED_LENGTH)
		goto err;
	if (!CBB_add_space(out, &enc_data, enc_record_len))
		goto err;

	if (!EVP_AEAD_CTX_seal(&aead->ctx, enc_data, &out_len, enc_record_len,
	    nonce, nonce_len, content, content_len, header, header_len))
		goto err;

	if (out_len != enc_record_len)
		goto err;

	ret = 1;

 err:
	freezero(header, header_len);
	freezero(nonce, nonce_len);

	return ret;
}

static int
tls12_record_layer_seal_record_protected_cipher(struct tls12_record_layer *rl,
    uint8_t content_type, const uint8_t *content, size_t content_len, CBB *out)
{
	EVP_CIPHER_CTX *enc = rl->write_cipher_ctx;
	size_t mac_len, pad_len;
	int block_size, eiv_len;
	uint8_t *enc_data, *eiv, *pad, pad_val;
	uint8_t *plain = NULL;
	size_t plain_len = 0;
	int ret = 0;
	CBB cbb;

	if (!CBB_init(&cbb, SSL3_RT_MAX_PLAIN_LENGTH))
		goto err;

	/* Add explicit IV if necessary. */
	eiv_len = 0;
	if (rl->version != TLS1_VERSION &&
	    EVP_CIPHER_CTX_mode(enc) == EVP_CIPH_CBC_MODE)
		eiv_len = EVP_CIPHER_CTX_iv_length(enc);
	if (eiv_len < 0 || eiv_len > EVP_MAX_IV_LENGTH)
		goto err;
	if (eiv_len > 0) {
		if (!CBB_add_space(&cbb, &eiv, eiv_len))
			goto err;
		arc4random_buf(eiv, eiv_len);
	}

	if (!CBB_add_bytes(&cbb, content, content_len))
		goto err;

	mac_len = 0;
	if (rl->write_hash_ctx != NULL) {
		if (!tls12_record_layer_write_mac(rl, &cbb, content_type,
		    content, content_len, &mac_len))
			goto err;
	}

	plain_len = (size_t)eiv_len + content_len + mac_len;

	/* Add padding to block size, if necessary. */
	block_size = EVP_CIPHER_CTX_block_size(enc);
	if (block_size < 0 || block_size > EVP_MAX_BLOCK_LENGTH)
		goto err;
	if (block_size > 1) {
		pad_len = block_size - (plain_len % block_size);
		pad_val = pad_len - 1;

		if (pad_len > 255)
			goto err;
		if (!CBB_add_space(&cbb, &pad, pad_len))
			goto err;
		memset(pad, pad_val, pad_len);
	}

	if (!CBB_finish(&cbb, &plain, &plain_len))
		goto err;

	if (plain_len % block_size != 0)
		goto err;
	if (plain_len > SSL3_RT_MAX_ENCRYPTED_LENGTH)
		goto err;

	if (!CBB_add_space(out, &enc_data, plain_len))
		goto err;
	if (!EVP_Cipher(enc, enc_data, plain, plain_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);
	freezero(plain, plain_len);

	return ret;
}

int
tls12_record_layer_seal_record(struct tls12_record_layer *rl,
    uint8_t content_type, const uint8_t *content, size_t content_len, CBB *cbb)
{
	CBB fragment;

	if (!CBB_add_u8(cbb, content_type))
		return 0;
	if (!CBB_add_u16(cbb, rl->version))
		return 0;
	if (rl->dtls) {
		if (!tls12_record_layer_build_seq_num(rl, cbb,
		    rl->write_epoch, rl->write_seq_num,
		    SSL3_SEQUENCE_SIZE))
			return 0;
	}
	if (!CBB_add_u16_length_prefixed(cbb, &fragment))
		return 0;

	if (rl->write_aead_ctx != NULL) {
		if (!tls12_record_layer_seal_record_protected_aead(rl,
		    content_type, content, content_len, &fragment))
			return 0;
	} else if (rl->write_cipher_ctx != NULL) {
		if (!tls12_record_layer_seal_record_protected_cipher(rl,
		    content_type, content, content_len, &fragment))
			return 0;
	} else {
		if (!tls12_record_layer_seal_record_plaintext(rl,
		    content_type, content, content_len, &fragment))
			return 0;
	}

	if (!CBB_flush(cbb))
		return 0;

	tls1_record_sequence_increment(rl->write_seq_num);

	return 1;
}
