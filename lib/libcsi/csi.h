/* $OpenBSD: csi.h,v 1.1 2018/06/02 17:40:33 jsing Exp $ */
/*
 * Copyright (c) 2018 Joel Sing <jsing@openbsd.org>
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

#ifndef HEADER_CSI_H
#define HEADER_CSI_H

#include <sys/types.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSI_ERR_MEM	1	/* Out of memory. */
#define CSI_ERR_INVAL	2	/* Invalid argument. */
#define CSI_ERR_CRYPTO	3	/* Crypto failure. */

/*
 * Primitives.
 */
struct csi_integer {
	const uint8_t *data;
	size_t len;
};

/*
 * Diffie-Hellman Key Exchange.
 */

struct csi_dh;

struct csi_dh_params {
	struct csi_integer g;
	struct csi_integer p;
};

struct csi_dh_public {
	struct csi_integer key;
};

struct csi_dh_shared {
	struct csi_integer key;
};

struct csi_dh *csi_dh_new(void);
void csi_dh_free(struct csi_dh *_cdh);
u_int csi_dh_size_bits(struct csi_dh *_cdh);

const char *csi_dh_error(struct csi_dh *_cdh);
int csi_dh_error_code(struct csi_dh *_cdh);

int csi_dh_set_params(struct csi_dh *_cdh, struct csi_dh_params *_params);
int csi_dh_set_peer_public(struct csi_dh *_cdh, struct csi_dh_public *_peer);
int csi_dh_generate_keys(struct csi_dh *_cdh, size_t _length,
    struct csi_dh_public **_public);
int csi_dh_derive_shared_key(struct csi_dh *_cdh,
    struct csi_dh_shared **_secret);

struct csi_dh_params *csi_dh_params(struct csi_dh *_cdh);
struct csi_dh_public *csi_dh_public_key(struct csi_dh *_cdh);
struct csi_dh_public *csi_dh_peer_public_key(struct csi_dh *_cdh);

void csi_dh_params_free(struct csi_dh_params *_cdhp);
void csi_dh_public_free(struct csi_dh_public *_cdhp);
void csi_dh_shared_free(struct csi_dh_shared *_cdhs);

struct csi_dh_params *csi_dh_params_modp_group1(void);
struct csi_dh_params *csi_dh_params_modp_group2(void);
struct csi_dh_params *csi_dh_params_modp_group5(void);
struct csi_dh_params *csi_dh_params_modp_group14(void);
struct csi_dh_params *csi_dh_params_modp_group15(void);
struct csi_dh_params *csi_dh_params_modp_group16(void);
struct csi_dh_params *csi_dh_params_modp_group17(void);
struct csi_dh_params *csi_dh_params_modp_group18(void);

#ifdef __cplusplus
}
#endif

#endif /* HEADER_CSI_H */
