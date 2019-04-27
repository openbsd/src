/*	$OpenBSD: if_wg.h,v 1.1 2019/04/27 05:14:33 dlg Exp $ */

/*
 * Copyright (c) 2018 David Gwynne <dlg@openbsd.org>
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

#ifndef _NET_IF_WG_H_
#define _NET_IF_WG_H_

/* the timing side of messages */

#define WG_MSGS_WINDOW		2000

#define WG_REKEY_MSGS		(~0ULL - 0xffffULL)
#define WG_REKEY_TIME		120000	/* msec */
#define WG_KEEPALIVE		10000	/* msec */
#define WG_REKEY_TIMEOUT	5000	/* msec */
#define WG_REKEY_JITTER		333	/* msec */
#define WG_REKEY_MAX		90000	/* msec */
#define WG_REJECT_MSGS		(~0ULL - WG_MSGS_WINDOW)
#define WG_REJECT_TIME		180000	/* msec */


/* the bytes side of messages */

#define WG_POLY1305_TAG_LEN	16
#define WG_CURVE25519_KEY_LEN	32
#define WG_AEAD_CHACHA20_POLY1305_KEY_LEN \
				32
#define WG_BLAKE2S_OUTPUT_LEN	16
#define WG_CHACHA20_POLY1305_NONCE_LEN \
				24

#define WG_AEAD_LEN(_l)		((_l) + WG_POLY1305_TAG_LEN)

#define WG_ENCRYPTED_STATIC_LEN	WG_AEAD_LEN(WG_CURVE25519_KEY_LEN)

#define WG_MSG_HANDSHAKE_INIT	1
#define WG_MSG_HANDSHAKE_RESP	2
#define WG_MSG_COOKIE_RESP	3
#define WG_MSG_DATA		4

struct wg_header {
	uint8_t			msg_type;
	uint8_t			zero[3];
} __packed __aligned(4);

struct wg_tai64n {
	uint64_t		tai_seconds;		/* BE */
	uint32_t		tai_nanoseconds;	/* BE */
} __packed __aligned(4);

#define WG_ENCRYPTED_TAI64N_LEN	WG_AEAD_LEN(sizeof(struct wg_tai64n))

struct wg_handshake_init {
	struct wg_header	hdr;
	uint32_t		sndr_idx;		/* LE */
	uint8_t			unencrypted_ephemeral[WG_CURVE25519_KEY_LEN];
	uint8_t			encrypted_static[WG_ENCRYPTED_STATIC_LEN];
	uint8_t			encrypted_timestamp[WG_ENCRYPTED_TAI64N_LEN];
	uint8_t			mac1[WG_BLAKE2S_OUTPUT_LEN];
	uint8_t			mac2[WG_BLAKE2S_OUTPUT_LEN];
} __packed __aligned(4);

struct wg_handshake_resp {
	struct wg_header	hdr;
	uint32_t		sndr_idx;		/* LE */
	uint32_t		rcvr_idx;		/* LE */
	uint8_t			unencrypted_ephemeral[WG_CURVE25519_KEY_LEN];
	uint8_t			encrypted_nothing[WG_AEAD_LEN(0)];
	uint8_t			mac1[WG_BLAKE2S_OUTPUT_LEN];
	uint8_t			mac2[WG_BLAKE2S_OUTPUT_LEN];
} __packed __aligned(4);

struct wg_cookie_resp {
	struct wg_header	hdr;
	uint32_t		rcvr_idx;		/* LE */
	uint8_t			nonce[24];
	uint8_t			encrypted_cookie[WG_AEAD_LEN(0)];
} __packed __aligned(4);

struct wg_data {
	struct wg_header	hdr;
	uint32_t		rcvr_idx;
	uint32_t		counter_lo;
	uint32_t		counter_hi;

	/* followed by encrypted data */
} __packed __aligned(4);

/*
 * ioctl interface
 */

#include <sys/ioccom.h>

#define WGIFCREATE	_IOW('w', 10, unsigned int)
#define WGIFATTACH	_IOW('w', 11, unsigned int)
#define WGIFDETACH	_IOW('w', 12, unsigned int)
#define WGIFDESTROY	_IOW('w', 13, unsigned int)

struct wg_if_sock {
	unsigned int		wg_unit;
	int			wg_sock;
};

#define WGIFSSOCK	_IOW('w', 14, struct wg_if_sock)
#define WGIFDSOCK	_IOW('w', 15, unsigned int)

struct wg_aead_key {
	uint8_t			key[WG_AEAD_CHACHA20_POLY1305_KEY_LEN];
};

struct wg_if_data_keys {
	unsigned int		wg_unit;
	uint32_t		wg_tx_index;		/* LE */
	uint32_t		wg_rx_index;		/* LE */
	struct wg_aead_key	wg_tx_key;
	struct wg_aead_key	wg_rx_key;
};

#define WGIFADDKEYS	_IOW('w', 16, struct wg_if_data_keys)
#define WGIFDELKEYS	_IOW('w', 17, struct wg_if_data_keys)

struct wg_if_role {
	unsigned int		wg_unit;
	unsigned int		wg_role;
#define WG_DATA_INITIATOR		0x696e6974
#define WG_DATA_RESPONDER		0x72657370
};

#define WGIFSROLE	 _IOW('w', 18, struct wg_if_role)
#define WGIFGROLE	 _IOWR('w', 20, struct wg_if_role)

/*
 * messages via reads
 */

#define WG_MSG_REKEY	0x746b6579

struct wg_msg {
	unsigned int	wg_unit;
	unsigned int	wg_type;
};

#endif /* _NET_IF_WG_H_ */
