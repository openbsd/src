/*	$OpenBSD: ipsecctl.h,v 1.22 2005/11/12 16:43:11 deraadt Exp $	*/
/*
 * Copyright (c) 2004, 2005 Hans-Joerg Hoexer <hshoexer@openbsd.org>
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

#ifndef _IPSECCTL_H_
#define _IPSECCTL_H_

#define IPSECCTL_OPT_DISABLE		0x0001
#define IPSECCTL_OPT_ENABLE		0x0002
#define IPSECCTL_OPT_NOACTION		0x0004
#define IPSECCTL_OPT_VERBOSE		0x0010
#define IPSECCTL_OPT_VERBOSE2		0x0020
#define IPSECCTL_OPT_SHOW		0x0040
#define IPSECCTL_OPT_SHOWALL		0x0080
#define IPSECCTL_OPT_FLUSH		0x0100
#define IPSECCTL_OPT_DELETE		0x0200

enum {
	ACTION_ADD, ACTION_DELETE
};

#define RULE_FLOW	0x01
#define RULE_SA		0x02
#define RULE_IKE	0x04

enum {
	DIRECTION_UNKNOWN, IPSEC_IN, IPSEC_OUT, IPSEC_INOUT
};
enum {
	PROTO_UNKNOWN, IPSEC_ESP, IPSEC_AH, IPSEC_IPCOMP, IPSEC_TCPMD5
};
enum {
	MODE_UNKNOWN, IPSEC_TRANSPORT, IPSEC_TUNNEL
};
enum {
	AUTH_UNKNOWN, AUTH_PSK, AUTH_RSA
};
enum {
	ID_UNKNOWN, ID_PREFIX, ID_FQDN, ID_UFQDN
};
enum {
	TYPE_UNKNOWN, TYPE_USE, TYPE_ACQUIRE, TYPE_REQUIRE, TYPE_DENY,
	TYPE_BYPASS, TYPE_DONTACQ
};
enum {
	AUTHXF_UNKNOWN, AUTHXF_NONE, AUTHXF_HMAC_MD5, AUTHXF_HMAC_RIPEMD160,
	AUTHXF_HMAC_SHA1, AUTHXF_HMAC_SHA2_256, AUTHXF_HMAC_SHA2_384,
	AUTHXF_HMAC_SHA2_512, AUTHXF_MD5, AUTHXF_SHA1
};
enum {
	ENCXF_UNKNOWN, ENCXF_NONE, ENCXF_3DES_CBC, ENCXF_DES_CBC, ENCXF_AES,
	ENCXF_AESCTR, ENCXF_BLOWFISH, ENCXF_CAST128, ENCXF_NULL, ENCXF_SKIPJACK
};
enum {
	COMPXF_UNKNOWN, COMPXF_DEFLATE, COMPXF_LZS
};
enum {
	IKE_ACTIVE, IKE_PASSIVE
};

struct ipsec_addr {
	union {
		struct in_addr		v4;
		struct in6_addr		v6;
		u_int8_t		addr8[16];
		u_int16_t		addr16[8];
		u_int32_t		addr32[4];
	} ipa;
#define v4	ipa.v4
#define v6	ipa.v6
#define addr8	ipa.addr8
#define addr16	ipa.addr16
#define addr32	ipa.addr32
};

struct ipsec_addr_wrap {
	struct ipsec_addr	 address;
	struct ipsec_addr	 mask;
	int			 netaddress;
	sa_family_t		 af;
	char			*name;
};

struct ipsec_auth {
	char		*srcid;
	char		*dstid;
	u_int8_t	 idtype;
	u_int16_t	 type;
};

struct ipsec_key {
	size_t		 len;
	u_int8_t	*data;
};

struct ipsec_xf {
	char		*name;
	u_int16_t	 id;
	size_t		 keymin;
	size_t		 keymax;
};

struct ipsec_transforms {
	const struct ipsec_xf *authxf;
	const struct ipsec_xf *encxf;
	const struct ipsec_xf *compxf;
};

extern const struct ipsec_xf authxfs[];
extern const struct ipsec_xf encxfs[];

/* Complete state of one rule. */
struct ipsec_rule {
	u_int8_t	 type;

	struct ipsec_addr_wrap *src;
	struct ipsec_addr_wrap *dst;
	struct ipsec_addr_wrap *peer;
	struct ipsec_auth *auth;
	struct ipsec_transforms *xfs;
	struct ipsec_transforms *mmxfs;
	struct ipsec_transforms *qmxfs;
	struct ipsec_key  *authkey;
	struct ipsec_key  *enckey;

	u_int8_t	 proto;
	u_int8_t	 tmode;
	u_int8_t	 direction;
	u_int8_t	 flowtype;
	u_int8_t	 ikemode;
	u_int32_t	 spi;
	u_int32_t	 nr;

	TAILQ_ENTRY(ipsec_rule) entries;
};

TAILQ_HEAD(ipsec_rule_queue, ipsec_rule);

struct ipsecctl {
	u_int32_t	rule_nr;
	int		opts;
	struct ipsec_rule_queue rule_queue;
};

int	parse_rules(FILE *, struct ipsecctl *);
int	ipsecctl_add_rule(struct ipsecctl * ipsec, struct ipsec_rule *);
void	ipsecctl_get_rules(struct ipsecctl *);
int	ike_print_config(struct ipsec_rule *, int);
int	ike_ipsec_establish(int, struct ipsec_rule *);

#endif /* _IPSECCTL_H_ */
