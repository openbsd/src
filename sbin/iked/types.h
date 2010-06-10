/*	$OpenBSD: types.h,v 1.4 2010/06/10 14:08:37 reyk Exp $	*/
/*	$vantronix: types.h,v 1.24 2010/05/11 12:05:56 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
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

#ifndef _IKED_TYPES_H
#define _IKED_TYPES_H

#define IKED_USER	"_iked"

#ifndef IKED_CONFIG
#define IKED_CONFIG	"/etc/iked.conf"
#endif
#define IKED_SOCKET	"/var/run/iked.sock"

#ifndef IKED_CA
#define IKED_CA		"/etc/iked/"
#endif
#define IKED_CA_DIR	"ca/"
#define IKED_CRL_DIR	"crls/"
#define IKED_CERT_DIR	"certs/"
#define IKED_PRIVKEY	IKED_CA "private/local.key"
#define IKED_PUBKEY	"local.pub"

#define IKED_OPT_VERBOSE	0x00000001
#define IKED_OPT_NOACTION	0x00000002
#define IKED_OPT_NONATT		0x00000004

#define IKED_IKE_PORT		500
#define IKED_NATT_PORT		4500

#define IKED_NONCE_MIN		16	/* XXX 128 bits */
#define IKED_NONCE_SIZE		32	/* XXX 256 bits */

#define IKED_ID_SIZE		1024	/* XXX should be dynanic */
#define IKED_PSK_SIZE		1024	/* XXX should be dynamic */
#define IKED_MSGBUF_MAX		8192
#define IKED_CFG_MAX		16	/* maximum CP attributes */
#define IKED_TAG_SIZE		64
#define IKED_CYCLE_BUFFERS	4	/* # of static buffers for mapping */
#define IKED_PASSWORD_SIZE	256	/* limited by most EAP types */

#define IKED_E			0x1000	/* Decrypted flag */

struct iked_constmap {
	u_int		 cm_type;
	const char	*cm_name;
	const char	*cm_descr;
};

struct iked_transform {
	u_int8_t			 xform_type;
	u_int16_t			 xform_id;
	u_int16_t			 xform_length;
	u_int16_t			 xform_keylength;
	u_int				 xform_score;
	struct iked_constmap		*xform_map;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,
	IMSG_CTL_FAIL,
	IMSG_CTL_VERBOSE,
	IMSG_CTL_NOTIFY,
	IMSG_CTL_RELOAD,
	IMSG_CTL_RESET,
	IMSG_CTL_COUPLE,
	IMSG_CTL_DECOUPLE,
	IMSG_CTL_ACTIVE,
	IMSG_CTL_PASSIVE,
	IMSG_UDP_SOCKET,
	IMSG_PFKEY_SOCKET,
	IMSG_IKE_MESSAGE,
	IMSG_CFG_POLICY,
	IMSG_CFG_USER,
	IMSG_CERTREQ,
	IMSG_CERT,
	IMSG_CERTVALID,
	IMSG_CERTINVALID,
	IMSG_AUTH
};

enum iked_procid {
	PROC_PARENT = 0,
	PROC_IKEV1,
	PROC_IKEV2,
	PROC_CERT,
	PROC_MAX
};

/* Attach the control socket to the following process */
#define PROC_CONTROL	PROC_CERT

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK
};

enum flushmode {
	RESET_RELOAD	= 0,
	RESET_ALL,
	RESET_CA,
	RESET_POLICY,
	RESET_SA,
	RESET_USER
};

#endif /* _IKED_TYPES_H */
