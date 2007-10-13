/*	$OpenBSD: ipsecctl.h,v 1.55 2007/10/13 16:35:18 deraadt Exp $	*/
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
#define IPSECCTL_OPT_MONITOR		0x0400
#define IPSECCTL_OPT_SHOWKEY		0x0800

enum {
	ACTION_ADD, ACTION_DELETE
};

#define RULE_FLOW	0x01
#define RULE_SA		0x02
#define RULE_IKE	0x04
#define RULE_GROUP	0x08

enum {
	DIRECTION_UNKNOWN, IPSEC_IN, IPSEC_OUT, IPSEC_INOUT
};
enum {
	PROTO_UNKNOWN, IPSEC_ESP, IPSEC_AH, IPSEC_IPCOMP, IPSEC_TCPMD5,
	IPSEC_IPIP
};
enum {
	MODE_UNKNOWN, IPSEC_TRANSPORT, IPSEC_TUNNEL
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
	AUTHXF_HMAC_SHA2_512
};
enum {
	ENCXF_UNKNOWN, ENCXF_NONE, ENCXF_3DES_CBC, ENCXF_DES_CBC, ENCXF_AES,
	ENCXF_AESCTR, ENCXF_BLOWFISH, ENCXF_CAST128, ENCXF_NULL, ENCXF_SKIPJACK
};
enum {
	COMPXF_UNKNOWN, COMPXF_DEFLATE, COMPXF_LZS
};
enum {
	GROUPXF_UNKNOWN, GROUPXF_NONE, GROUPXF_768, GROUPXF_1024, GROUPXF_1536,
	GROUPXF_2048, GROUPXF_3072, GROUPXF_4096, GROUPXF_6144, GROUPXF_8192,
};
enum {
	IKE_ACTIVE, IKE_PASSIVE, IKE_DYNAMIC
};
enum {
	IKE_AUTH_RSA, IKE_AUTH_PSK
};
enum {
	IKE_MM=0, IKE_AM, IKE_QM
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
	struct ipsec_addr_wrap	*next;
	struct ipsec_addr_wrap	*tail;
};

struct ipsec_hosts {
	struct ipsec_addr_wrap	*src;
	struct ipsec_addr_wrap	*dst;
	u_int16_t		 sport;
	u_int16_t		 dport;
};

struct ipsec_auth {
	char		*srcid;
	char		*dstid;
	u_int8_t	 srcid_type;
	u_int8_t	 dstid_type;
	u_int16_t	 type;
};

struct ipsec_key {
	size_t		 len;
	u_int8_t	*data;
};

struct ike_auth {
	u_int8_t	 type;
	char		*string;
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
	const struct ipsec_xf *groupxf;
};

struct ipsec_life {
	int		 lifetime;
	int		 lifevolume;
};

struct ike_mode {
	struct ipsec_transforms	*xfs;
	struct ipsec_life	*life;
	u_int8_t		 ike_exch;
};

extern const struct ipsec_xf authxfs[];
extern const struct ipsec_xf encxfs[];
extern const struct ipsec_xf compxfs[];

TAILQ_HEAD(dst_group_queue, ipsec_rule);

/* Complete state of one rule. */
struct ipsec_rule {
	u_int8_t	 type;

	struct ipsec_addr_wrap *src;
	struct ipsec_addr_wrap *dst;
	struct ipsec_addr_wrap *dst2;
	struct ipsec_addr_wrap *local;
	struct ipsec_addr_wrap *peer;
	struct ipsec_auth *auth;
	struct ike_auth *ikeauth;
	struct ipsec_transforms *xfs;
	struct ipsec_transforms *p1xfs;
	struct ipsec_life *p1life;
	struct ipsec_transforms *p2xfs;
	struct ipsec_life *p2life;
	struct ipsec_key  *authkey;
	struct ipsec_key  *enckey;

	char		*tag;		/* pf tag for SAs */
	char		*p2name;	/* Phase 2 Name (IPsec-XX) */
	char		*p2lid;		/* Phase 2 source ID */
	char		*p2rid;		/* Phase 2 destination ID */
	u_int8_t	 satype;	/* encapsulating prococol */
	u_int8_t	 proto;		/* encapsulated protocol */
	u_int8_t	 proto2;
	u_int8_t	 tmode;
	u_int8_t	 direction;
	u_int8_t	 flowtype;
	u_int8_t	 ikemode;
	u_int8_t	 p1ie;
	u_int8_t	 p2ie;
	u_int16_t	 sport;
	u_int16_t	 dport;
	u_int32_t	 spi;
	u_int32_t	 spi2;
	u_int32_t	 nr;

	TAILQ_ENTRY(ipsec_rule) rule_entry;
	TAILQ_ENTRY(ipsec_rule) group_entry;
	TAILQ_ENTRY(ipsec_rule) dst_group_entry;

	struct dst_group_queue	dst_group_queue;
};

TAILQ_HEAD(ipsec_rule_queue, ipsec_rule);
TAILQ_HEAD(ipsec_group_queue, ipsec_rule);

struct ipsecctl {
	u_int32_t	rule_nr;
	int		opts;
	struct ipsec_rule_queue rule_queue;
	struct ipsec_group_queue group_queue;
};

int	parse_rules(const char *, struct ipsecctl *);
int	cmdline_symset(char *);
int	ipsecctl_add_rule(struct ipsecctl *, struct ipsec_rule *);
void	ipsecctl_free_rule(struct ipsec_rule *);
void	ipsecctl_get_rules(struct ipsecctl *);
void	ipsecctl_print_rule(struct ipsec_rule *, int);
int	ike_print_config(struct ipsec_rule *, int);
int	ike_ipsec_establish(int, struct ipsec_rule *);
void	set_ipmask(struct ipsec_addr_wrap *, u_int8_t);

#endif /* _IPSECCTL_H_ */
