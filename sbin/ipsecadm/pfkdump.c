/*	$OpenBSD: pfkdump.c,v 1.14 2004/10/08 05:59:55 ho Exp $	*/

/*
 * Copyright (c) 2003 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <net/pfkeyv2.h>
#include <netinet/ip_ipsp.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

#define PFKEY2_CHUNK sizeof(u_int64_t)

void	print_sa(struct sadb_ext *, struct sadb_msg *);
void	print_addr(struct sadb_ext *, struct sadb_msg *);
void	print_key(struct sadb_ext *, struct sadb_msg *);
void	print_life(struct sadb_ext *, struct sadb_msg *);
void	print_proto(struct sadb_ext *, struct sadb_msg *);
void	print_flow(struct sadb_ext *, struct sadb_msg *);
void	print_supp(struct sadb_ext *, struct sadb_msg *);
void	print_prop(struct sadb_ext *, struct sadb_msg *);
void	print_sens(struct sadb_ext *, struct sadb_msg *);
void	print_spir(struct sadb_ext *, struct sadb_msg *);
void	print_ident(struct sadb_ext *, struct sadb_msg *);
void	print_policy(struct sadb_ext *, struct sadb_msg *);
void	print_cred(struct sadb_ext *, struct sadb_msg *);
void	print_auth(struct sadb_ext *, struct sadb_msg *);
void	print_udpenc(struct sadb_ext *, struct sadb_msg *);

struct idname *lookup(struct idname [], u_int8_t);
char	*lookup_name(struct idname [], u_int8_t);
void	print_ext(struct sadb_ext *, struct sadb_msg *);
void	print_msg(struct sadb_msg *, int);
char	*alg_by_ext(u_int8_t, u_int8_t);
void	print_alg(struct sadb_alg *, u_int8_t);
void	print_comb(struct sadb_comb *, struct sadb_msg *);
void	msg_send(int, u_int8_t, u_int8_t);
void	msg_read(int);
void	do_pfkey(int, u_int8_t);
void	ipsecadm_monitor(void);
void	ipsecadm_show(u_int8_t);

struct idname {
	u_int8_t id;
	char *name;
	void (*func)(struct sadb_ext *, struct sadb_msg *);
};

struct idname ext_types[] = {
	{ SADB_EXT_RESERVED,		"reserved",		NULL },
	{ SADB_EXT_SA,			"sa",			print_sa},
	{ SADB_EXT_LIFETIME_CURRENT,	"lifetime_cur",		print_life },
	{ SADB_EXT_LIFETIME_HARD,	"lifetime_hard",	print_life },
	{ SADB_EXT_LIFETIME_SOFT,	"lifetime_soft",	print_life },
	{ SADB_EXT_ADDRESS_SRC,		"address_src",		print_addr},
	{ SADB_EXT_ADDRESS_DST,		"address_dst",		print_addr},
	{ SADB_EXT_ADDRESS_PROXY,	"address_proxy",	print_addr},
	{ SADB_EXT_KEY_AUTH,		"key_auth",		print_key},
	{ SADB_EXT_KEY_ENCRYPT,		"key_encrypt",		print_key},
	{ SADB_EXT_IDENTITY_SRC,	"identity_src",		print_ident },
	{ SADB_EXT_IDENTITY_DST,	"identity_dst",		print_ident },
	{ SADB_EXT_SENSITIVITY,		"sensitivity",		print_sens },
	{ SADB_EXT_PROPOSAL,		"proposal",		print_prop },
	{ SADB_EXT_SUPPORTED_AUTH,	"supported_auth",	print_supp },
	{ SADB_EXT_SUPPORTED_ENCRYPT,	"supported_encrypt",	print_supp },
	{ SADB_EXT_SPIRANGE,		"spirange",		print_spir },
	{ SADB_X_EXT_SRC_MASK,		"x_src_mask",		print_addr },
	{ SADB_X_EXT_DST_MASK,		"x_dst_mask",		print_addr },
	{ SADB_X_EXT_PROTOCOL,		"x_protocol",		print_proto },
	{ SADB_X_EXT_FLOW_TYPE,		"x_flow_type",		print_flow },
	{ SADB_X_EXT_SRC_FLOW,		"x_src_flow",		print_addr },
	{ SADB_X_EXT_DST_FLOW,		"x_dst_flow",		print_addr },
	{ SADB_X_EXT_SA2,		"x_sa2",		print_sa },
	{ SADB_X_EXT_DST2,		"x_dst2",		print_addr },
	{ SADB_X_EXT_POLICY,		"x_policy",		print_policy },
	{ SADB_X_EXT_LOCAL_CREDENTIALS,	"x_local_cred",		print_cred },
	{ SADB_X_EXT_REMOTE_CREDENTIALS,"x_remote_cred",	print_cred },
	{ SADB_X_EXT_LOCAL_AUTH,	"x_local_auth",		print_auth },
	{ SADB_X_EXT_REMOTE_AUTH,	"x_remote_auth",	print_auth },
	{ SADB_X_EXT_SUPPORTED_COMP,	"x_supported_comp",	print_supp },
	{ SADB_X_EXT_UDPENCAP,		"x_udpencap",		print_udpenc },
#ifdef SADB_X_EXT_LIFETIME_LASTUSE
	{ SADB_X_EXT_LIFETIME_LASTUSE,	"x_lifetime_lastuse",	print_life },
#endif
	{ 0,				NULL,			NULL }
};

struct idname msg_types[] = {
	{ SADB_ACQUIRE,			"sadb_aqcuire",		NULL },
	{ SADB_ADD,			"sadb_add",		NULL },
	{ SADB_DELETE,			"sadb_delete",		NULL },
	{ SADB_DUMP,			"sadb_dump",		NULL },
	{ SADB_EXPIRE,			"sadb_expire",		NULL },
	{ SADB_FLUSH,			"sadb_flush",		NULL },
	{ SADB_GET,			"sadb_get",		NULL },
	{ SADB_GETSPI,			"sadb_getspi",		NULL },
	{ SADB_REGISTER,		"sadb_register",	NULL },
	{ SADB_UPDATE,			"sadb_update",		NULL },
	{ SADB_X_ADDFLOW,		"sadb_x_addflow",	NULL },
	{ SADB_X_ASKPOLICY,		"sadb_x_askpolicy",	NULL },
	{ SADB_X_DELFLOW,		"sadb_x_delflow",	NULL },
	{ SADB_X_GRPSPIS,		"sadb_x_grpspis",	NULL },
	{ SADB_X_PROMISC,		"sadb_x_promisc",	NULL },
	{ 0,				NULL,			NULL },
};

struct idname sa_types[] = {
	{ SADB_SATYPE_UNSPEC,		"unspec",		NULL },
	{ SADB_SATYPE_AH,		"ah",			NULL },
	{ SADB_SATYPE_ESP,		"esp",			NULL },
	{ SADB_SATYPE_RSVP,		"rsvp",			NULL },
	{ SADB_SATYPE_OSPFV2,		"ospfv2",		NULL },
	{ SADB_SATYPE_RIPV2,		"ripv2",		NULL },
	{ SADB_SATYPE_MIP,		"mip",			NULL },
	{ SADB_X_SATYPE_IPIP,		"ipip",			NULL },
	{ SADB_X_SATYPE_TCPSIGNATURE,	"tcpmd5",		NULL },
	{ SADB_X_SATYPE_IPCOMP,		"ipcomp",		NULL },
	{ 0,				NULL,			NULL }
};

struct idname auth_types[] = {
	{ SADB_AALG_NONE,		"none",			NULL },
	{ SADB_X_AALG_DES,		"des",			NULL },
	{ SADB_AALG_MD5HMAC,		"hmac-md5",		NULL },
	{ SADB_X_AALG_RIPEMD160HMAC,	"hmac-ripemd160",	NULL },
	{ SADB_AALG_SHA1HMAC,		"hmac-sha1",		NULL },
	{ SADB_X_AALG_SHA2_256,		"hmac-sha2-256",	NULL },
	{ SADB_X_AALG_SHA2_384,		"hmac-sha2-384",	NULL },
	{ SADB_X_AALG_SHA2_512,		"hmac-sha2-512",	NULL },
	{ SADB_X_AALG_MD5,		"md5",			NULL },
	{ SADB_X_AALG_SHA1,		"sha1",			NULL },
	{ 0,				NULL,			NULL }
};

struct idname enc_types[] = {
	{ SADB_EALG_NONE,		"none",			NULL },
	{ SADB_EALG_3DESCBC,		"3des-cbc",		NULL },
	{ SADB_EALG_DESCBC,		"des-cbc",		NULL },
	{ SADB_X_EALG_3IDEA,		"idea3",		NULL },
	{ SADB_X_EALG_AES,		"aes",			NULL },
	{ SADB_X_EALG_BLF,		"blowfish",		NULL },
	{ SADB_X_EALG_CAST,		"cast128",		NULL },
	{ SADB_X_EALG_DES_IV32,		"des-iv32",		NULL },
	{ SADB_X_EALG_DES_IV64,		"des-iv64",		NULL },
	{ SADB_X_EALG_IDEA,		"idea",			NULL },
	{ SADB_EALG_NULL,		"null",			NULL },
	{ SADB_X_EALG_RC4,		"rc4",			NULL },
	{ SADB_X_EALG_RC5,		"rc5",			NULL },
	{ SADB_X_EALG_SKIPJACK,		"skipjack",		NULL },
	{ 0,				NULL,			NULL }
};

struct idname comp_types[] = {
	{ SADB_X_CALG_NONE,		"none",			NULL },
	{ SADB_X_CALG_OUI,		"oui",			NULL },
	{ SADB_X_CALG_DEFLATE,		"deflate",		NULL },
	{ SADB_X_CALG_LZS,		"lzs",			NULL },
	{ 0,				NULL,			NULL }
};

struct idname cred_types[] = {
	{ SADB_X_CREDTYPE_X509,		"x509-asn1",		NULL },
	{ SADB_X_CREDTYPE_KEYNOTE,	"keynote",		NULL },
	{ 0,				NULL,			NULL }
};

struct idname xauth_types[] = {
	{ SADB_X_AUTHTYPE_NONE,		"none",			NULL },
	{ SADB_X_AUTHTYPE_PASSPHRASE,	"passphrase",		NULL },
	{ SADB_X_AUTHTYPE_RSA,		"rsa",			NULL },
	{ 0,				NULL,			NULL }
};

struct idname identity_types[] = {
	{ SADB_IDENTTYPE_RESERVED,	"reserved",		NULL },
	{ SADB_IDENTTYPE_PREFIX,	"prefix",		NULL },
	{ SADB_IDENTTYPE_FQDN,		"fqdn",			NULL },
	{ SADB_IDENTTYPE_USERFQDN,	"ufqdn",		NULL },
	{ SADB_X_IDENTTYPE_CONNECTION,	"x_connection",		NULL },
	{ 0,				NULL,			NULL }
};

struct idname flow_types[] = {
	{ SADB_X_FLOW_TYPE_USE,		"use",			NULL },
	{ SADB_X_FLOW_TYPE_ACQUIRE,	"acquire",		NULL },
	{ SADB_X_FLOW_TYPE_REQUIRE,	"require",		NULL },
	{ SADB_X_FLOW_TYPE_BYPASS,	"bypass",		NULL },
	{ SADB_X_FLOW_TYPE_DENY,	"deny",			NULL },
	{ SADB_X_FLOW_TYPE_DONTACQ,	"dontacq",		NULL },
	{ 0,				NULL,			NULL }
};

struct idname states[] = {
	{ SADB_SASTATE_LARVAL,		"larval",		NULL },
	{ SADB_SASTATE_MATURE,		"mature",		NULL },
	{ SADB_SASTATE_DYING,		"dying",		NULL },
	{ SADB_SASTATE_DEAD,		"dead",			NULL },
	{ 0,				NULL,			NULL }
};

struct idname *
lookup(struct idname tab[], u_int8_t id)
{
	struct idname *entry;

	for (entry = tab; entry->name; entry++)
		if (entry->id == id)
			return (entry);
	return (NULL);
}

char *
lookup_name(struct idname tab[], u_int8_t id)
{
	struct idname *entry;

	entry = lookup(tab, id);
	return (entry ? entry->name : "unknown");
}

void
print_ext(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct idname *entry;

	if ((entry = lookup(ext_types, ext->sadb_ext_type)) == NULL) {
		printf("unknown ext: type %u len %u\n",
		    ext->sadb_ext_type, ext->sadb_ext_len);
		return;
	}
	printf("\t%s: ", entry->name);
	if (entry->func != NULL)
		(*entry->func)(ext, msg);
	else
		printf("type %u len %u\n",
		    ext->sadb_ext_type, ext->sadb_ext_len);
}

void
print_msg(struct sadb_msg *msg, int promisc)
{
	struct sadb_ext *ext;

	printf("%s: satype %s vers %u len %u seq %u %spid %u\n",
	    lookup_name(msg_types, msg->sadb_msg_type),
	    lookup_name(sa_types, msg->sadb_msg_satype),
	    msg->sadb_msg_version, msg->sadb_msg_len,
	    msg->sadb_msg_seq,
#if 0
	    promisc ? "from " : "to ",
#else
	    "",
#endif
	    msg->sadb_msg_pid);
	if (msg->sadb_msg_errno)
		printf("\terrno %u: %s\n", msg->sadb_msg_errno,
		    strerror(msg->sadb_msg_errno));
	for (ext = (struct sadb_ext *)(msg + 1);
	    (u_int8_t *)ext - (u_int8_t *)msg <
	    msg->sadb_msg_len * PFKEY2_CHUNK &&
	    ext->sadb_ext_len > 0;
	    ext = (struct sadb_ext *)((u_int8_t *)ext +
	    ext->sadb_ext_len * PFKEY2_CHUNK))
		print_ext(ext, msg);
	fflush(stdout);
}

void
print_sa(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_sa *sa = (struct sadb_sa *) ext;

	if (msg->sadb_msg_satype == SADB_X_SATYPE_IPCOMP)
		printf("cpi 0x%8.8x comp %s\n",
		    ntohl(sa->sadb_sa_spi),
		    lookup_name(comp_types, sa->sadb_sa_encrypt));
	else
		printf("spi 0x%8.8x auth %s enc %s\n",
		    ntohl(sa->sadb_sa_spi),
		    lookup_name(auth_types, sa->sadb_sa_auth),
		    lookup_name(enc_types, sa->sadb_sa_encrypt));
	printf("\t\tstate %s replay %u flags %u\n",
	    lookup_name(states, sa->sadb_sa_state),
	    sa->sadb_sa_replay, sa->sadb_sa_flags);
}

void
print_addr(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_address *addr = (struct sadb_address *) ext;
	struct sockaddr *sa;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	char hbuf[NI_MAXHOST];

	sa = (struct sockaddr *)(addr + 1);
	if (sa->sa_family == 0)
		printf("<any>");
	else if (getnameinfo(sa, sa->sa_len, hbuf, sizeof(hbuf), NULL, 0,
	    NI_NUMERICHOST))
		printf("<could not get numeric hostname>");
	else
		printf("%s", hbuf);
	switch (sa->sa_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		if (sin->sin_port)
			printf(" port %u", ntohs(sin->sin_port));
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
		if (sin6->sin6_port)
			printf(" port %u", ntohs(sin6->sin6_port));
		break;
	}
	printf("\n");
}

void
print_key(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_key *key = (struct sadb_key *) ext;
	u_int8_t *data;
	int i;

	printf("bits %u: ", key->sadb_key_bits);
	data = (u_int8_t *)(key + 1);
	for (i = 0; i < key->sadb_key_bits / 8; i++) {
		printf("%2.2x", data[i]);
		data[i] = 0x00;		/* clear sensitive data */
	}
	printf("\n");
}

void
print_life(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_lifetime *life = (struct sadb_lifetime *) ext;

	printf("alloc %u bytes %llu add %llu first %llu\n",
	    life->sadb_lifetime_allocations,
	    life->sadb_lifetime_bytes,
	    life->sadb_lifetime_addtime,
	    life->sadb_lifetime_usetime);
}

void
print_proto(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_protocol *proto = (struct sadb_protocol *) ext;

	/* overloaded */
	if (msg->sadb_msg_type == SADB_X_GRPSPIS)
		printf("satype %s flags %u\n",
		    lookup_name(sa_types, proto->sadb_protocol_proto),
		    proto->sadb_protocol_flags);
	else
		printf("proto %u flags %u\n",
		    proto->sadb_protocol_proto, proto->sadb_protocol_flags);
}

void
print_flow(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_protocol *proto = (struct sadb_protocol *) ext;
	char *dir = "unknown";

	switch (proto->sadb_protocol_direction) {
	case IPSP_DIRECTION_IN:
		dir = "in";
		break;
	case IPSP_DIRECTION_OUT:
		dir = "out";
		break;
	}
	printf("type %s direction %s\n",
	    lookup_name(flow_types, proto->sadb_protocol_proto), dir);
}

char *
alg_by_ext(u_int8_t ext_type, u_int8_t id)
{
	switch (ext_type) {
	case SADB_EXT_SUPPORTED_ENCRYPT:
		return lookup_name(enc_types, id);
	case SADB_EXT_SUPPORTED_AUTH:
		return lookup_name(auth_types, id);
	case SADB_X_EXT_SUPPORTED_COMP:
		return lookup_name(comp_types, id);
	default:
		return "unknown";
	}
}

void
print_alg(struct sadb_alg *alg, u_int8_t ext_type)
{
	printf("\t\t%s iv %u min %u max %u\n",
	    alg_by_ext(ext_type, alg->sadb_alg_id), alg->sadb_alg_ivlen,
	    alg->sadb_alg_minbits, alg->sadb_alg_maxbits);
}

void
print_supp(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_supported *supported = (struct sadb_supported *) ext;
	struct sadb_alg *alg;

	printf("\n");
	for (alg = (struct sadb_alg *)(supported + 1);
	    (u_int8_t *)alg - (u_int8_t *)ext <
	    ext->sadb_ext_len * PFKEY2_CHUNK;
	    alg++)
		print_alg(alg, ext->sadb_ext_type);
}

void
print_comb(struct sadb_comb *comb, struct sadb_msg *msg)
{
	printf("\t\tauth %s min %u max %u\n"
	    "\t\tenc %s min %u max %u\n"
	    "\t\taddtime hard %llu soft %llu\n"
	    "\t\tusetime hard %llu soft %llu\n",
	    lookup_name(auth_types, comb->sadb_comb_auth),
	    comb->sadb_comb_auth_minbits,
	    comb->sadb_comb_auth_maxbits,
	    lookup_name(enc_types, comb->sadb_comb_encrypt),
	    comb->sadb_comb_encrypt_minbits,
	    comb->sadb_comb_encrypt_maxbits,
	    comb->sadb_comb_soft_addtime,
	    comb->sadb_comb_hard_addtime,
	    comb->sadb_comb_soft_usetime,
	    comb->sadb_comb_hard_usetime);
#if 0
	    comb->sadb_comb_flags,
	    comb->sadb_comb_reserved,
	    comb->sadb_comb_soft_allocations,
	    comb->sadb_comb_hard_allocations,
	    comb->sadb_comb_soft_bytes,
	    comb->sadb_comb_hard_bytes,
#endif
}

void
print_prop(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_prop *prop = (struct sadb_prop *) ext;
	struct sadb_comb *comb;

	printf("replay %u\n", prop->sadb_prop_replay);
	for (comb = (struct sadb_comb *)(prop + 1);
	    (u_int8_t *)comb - (u_int8_t *)ext <
	    ext->sadb_ext_len * PFKEY2_CHUNK;
	    comb++)
		print_comb(comb, msg);
}

void
print_sens(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_sens *sens = (struct sadb_sens *) ext;

	printf("dpd %u sens_level %u integ_level %u\n",
	    sens->sadb_sens_dpd,
	    sens->sadb_sens_sens_level,
	    sens->sadb_sens_integ_level);
}

void
print_spir(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_spirange *spirange = (struct sadb_spirange *) ext;

	printf("min 0x%8.8x max 0x%8.8x\n",
	    spirange->sadb_spirange_min, spirange->sadb_spirange_max);
}

void
print_ident(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_ident *ident = (struct sadb_ident *) ext;

	printf("type %s id %llu: %s\n",
	    lookup_name(identity_types, ident->sadb_ident_type),
	    ident->sadb_ident_id, (char *)(ident + 1));
}

void
print_policy(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_x_policy *x_policy = (struct sadb_x_policy *) ext;

	printf("seq %u\n", x_policy->sadb_x_policy_seq);
}

void
print_cred(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_x_cred *x_cred = (struct sadb_x_cred *) ext;

	printf("type %s\n",
	    lookup_name(cred_types, x_cred->sadb_x_cred_type));
}

void
print_auth(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_x_cred *x_cred = (struct sadb_x_cred *) ext;

	printf("type %s\n",
	    lookup_name(xauth_types, x_cred->sadb_x_cred_type));
}

void
print_udpenc(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_x_udpencap *x_udpencap = (struct sadb_x_udpencap *) ext;

	printf("udpencap port %u\n", ntohs(x_udpencap->sadb_x_udpencap_port));
}

void
msg_send(int pfkey, u_int8_t satype, u_int8_t mtype)
{
	struct sadb_msg msg;
	static u_int32_t seq = 0;

	memset(&msg, 0, sizeof(msg));
	msg.sadb_msg_version = PF_KEY_V2;
	msg.sadb_msg_pid = getpid();
	msg.sadb_msg_seq = ++seq;
	msg.sadb_msg_satype = satype;
	msg.sadb_msg_type = mtype;
	msg.sadb_msg_len = sizeof(msg) / PFKEY2_CHUNK;
	if (write(pfkey, &msg, sizeof(msg)) != sizeof(msg))
		err(1, "write");
}

void
msg_read(int pfkey)
{
	struct sadb_msg hdr, *msg;
	u_int8_t *data;
	int promisc = 0;
	ssize_t len;

	if (recv(pfkey, &hdr, sizeof(hdr), MSG_PEEK) != sizeof(hdr))
		err(1, "peek");
	len = hdr.sadb_msg_len * PFKEY2_CHUNK;
	if ((data = malloc(len)) == NULL)
		err(1, "malloc");
	if (read(pfkey, data, len) != len)
		err(1, "read");
	msg = (struct sadb_msg *)data;
	if (hdr.sadb_msg_type == SADB_X_PROMISC) {
		/* remove extra header from promisc messages */
		if ((msg->sadb_msg_len * PFKEY2_CHUNK) > 2 * sizeof(hdr)) {
			/*printf("promisc\n");*/
			msg++;
			promisc++;
		}
	}
	print_msg(msg, promisc);
	memset(data, 0, len);
	free(data);
}

void
do_pfkey(int monitor, u_int8_t satype)
{
	struct timeval tv;
	fd_set *rset;
	ssize_t set_size;
	int pfkey, n;

	if ((pfkey = socket(PF_KEY, SOCK_RAW, PF_KEY_V2)) < 0)
		err(1, "socket PFKEY");
	if (monitor) {
		/* satype 1 enables promisc mode */
		msg_send(pfkey, 1, SADB_X_PROMISC);
	} else
		msg_send(pfkey, satype, SADB_DUMP);
	set_size = howmany(pfkey + 1, NFDBITS) * sizeof(fd_mask);
	if ((rset = malloc(set_size)) == NULL)
		err(1, "malloc");
	tv.tv_sec = 0;
	tv.tv_usec = 10000;
	for (;;) {
		memset(rset, 0, set_size);
		FD_SET(pfkey, rset);
		if ((n = select(pfkey+1, rset, NULL, NULL,
		    monitor ? NULL : &tv)) < 0)
			err(2, "select");
		if (n == 0)
			break;
		if (FD_ISSET(pfkey, rset))
			msg_read(pfkey);
	}
	close(pfkey);
}

void
ipsecadm_monitor(void)
{
	do_pfkey(1, 0);
}

void
ipsecadm_show(u_int8_t satype)
{
	do_pfkey(0, satype);
}
