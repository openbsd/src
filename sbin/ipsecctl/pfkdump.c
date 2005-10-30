/*	$OpenBSD: pfkdump.c,v 1.6 2005/10/30 19:50:24 hshoexer Exp $	*/

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
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <net/pfkeyv2.h>
#include <netinet/ip_ipsp.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

#include "ipsecctl.h"
#include "pfkey.h"

static void	print_sa(struct sadb_ext *, struct sadb_msg *);
static void	print_addr(struct sadb_ext *, struct sadb_msg *);
static void	print_key(struct sadb_ext *, struct sadb_msg *);
static void	print_life(struct sadb_ext *, struct sadb_msg *);
static void	print_ident(struct sadb_ext *, struct sadb_msg *);
static void	print_auth(struct sadb_ext *, struct sadb_msg *);
static void	print_cred(struct sadb_ext *, struct sadb_msg *);
static void	print_udpenc(struct sadb_ext *, struct sadb_msg *);

static struct idname *lookup(struct idname [], u_int8_t);
static char    *lookup_name(struct idname [], u_int8_t);
static void	print_ext(struct sadb_ext *, struct sadb_msg *, int);

void		pfkey_print_sa(struct sadb_msg *, int);

struct sadb_ext *extensions[SADB_EXT_MAX];

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
	{ SADB_EXT_KEY_AUTH,		"key_auth",		print_key},
	{ SADB_EXT_KEY_ENCRYPT,		"key_encrypt",		print_key},
	{ SADB_EXT_IDENTITY_SRC,	"identity_src",		print_ident },
	{ SADB_EXT_IDENTITY_DST,	"identity_dst",		print_ident },
	{ SADB_X_EXT_REMOTE_AUTH,	"remote_auth",		print_auth },
	{ SADB_X_EXT_LOCAL_CREDENTIALS, "local_cred",		print_cred },
	{ SADB_X_EXT_REMOTE_CREDENTIALS,"remote_cred",		print_cred },
	{ SADB_X_EXT_UDPENCAP,		"udpencap",		print_udpenc },
	{ SADB_X_EXT_LIFETIME_LASTUSE,	"lifetime_lastuse",	print_life },
	{ 0,				NULL,			NULL }
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

struct idname cred_types[] = {
	{ SADB_X_CREDTYPE_X509,		"x509-asn1",		NULL },
	{ SADB_X_CREDTYPE_KEYNOTE,	"keynote",		NULL },
	{ 0,				NULL,			NULL }
};

struct idname enc_types[] = {
	{ SADB_EALG_NONE,		"none",			NULL },
	{ SADB_EALG_3DESCBC,		"3des-cbc",		NULL },
	{ SADB_EALG_DESCBC,		"des-cbc",		NULL },
	{ SADB_X_EALG_3IDEA,		"idea3",		NULL },
	{ SADB_X_EALG_AES,		"aes",			NULL },
	{ SADB_X_EALG_AESCTR,		"aesctr",		NULL },
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

static struct idname *
lookup(struct idname tab[], u_int8_t id)
{
	struct idname *entry;

	for (entry = tab; entry->name; entry++)
		if (entry->id == id)
			return (entry);
	return (NULL);
}

static char *
lookup_name(struct idname tab[], u_int8_t id)
{
	struct idname *entry;

	entry = lookup(tab, id);
	return (entry ? entry->name : "unknown");
}

static void
print_ext(struct sadb_ext *ext, struct sadb_msg *msg, int opts)
{
	struct idname *entry;

	if (ext->sadb_ext_type == SADB_EXT_ADDRESS_SRC ||
	    ext->sadb_ext_type == SADB_EXT_ADDRESS_DST)
		return;

	if ((entry = lookup(ext_types, ext->sadb_ext_type)) == NULL) {
		printf("unknown ext: type %u len %u\n",
		    ext->sadb_ext_type, ext->sadb_ext_len);
		return;
	}

	if (!(opts & IPSECCTL_OPT_VERBOSE) && (entry->id != SADB_EXT_SA))
		return;
	if (entry->id != SADB_EXT_SA)
		printf("\t%s: ", entry->name);
	if (entry->func != NULL)
		(*entry->func)(ext, msg);
	else
		printf("type %u len %u\n",
		    ext->sadb_ext_type, ext->sadb_ext_len);
}

static void
print_sa(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_sa *sa = (struct sadb_sa *)ext;

	if (extensions[SADB_EXT_ADDRESS_SRC]) {
		printf("from ");
		print_addr(extensions[SADB_EXT_ADDRESS_SRC], msg);
	}
	if (extensions[SADB_EXT_ADDRESS_DST]) {
		printf(" to ");
		print_addr(extensions[SADB_EXT_ADDRESS_DST], msg);
	}
	printf(" spi 0x%08x", ntohl(sa->sadb_sa_spi));
	if (msg->sadb_msg_satype == SADB_X_SATYPE_IPCOMP)
		printf(" comp %s", lookup_name(comp_types,
		    sa->sadb_sa_encrypt));
	else {
		if (sa->sadb_sa_encrypt)
			printf(" enc %s", lookup_name(enc_types,
			    sa->sadb_sa_encrypt));
		if (sa->sadb_sa_auth)
			printf(" auth %s", lookup_name(auth_types,
			    sa->sadb_sa_auth));
	}
	if (sa->sadb_sa_flags & SADB_X_SAFLAGS_TUNNEL)
		printf(" tunnel");
	printf("\n");
}

static void
print_addr(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_address *addr = (struct sadb_address *)ext;
	struct sockaddr *sa;
	struct sockaddr_in *sin4;
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
		sin4 = (struct sockaddr_in *)sa;
		if (sin4->sin_port)
			printf(" port %u", ntohs(sin4->sin_port));
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
		if (sin6->sin6_port)
			printf(" port %u", ntohs(sin6->sin6_port));
		break;
	}
}

static void
print_key(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_key *key = (struct sadb_key *)ext;
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

static void
print_life(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_lifetime *life = (struct sadb_lifetime *)ext;

	printf("alloc %u bytes %llu add %llu first %llu\n",
	    life->sadb_lifetime_allocations,
	    life->sadb_lifetime_bytes,
	    life->sadb_lifetime_addtime,
	    life->sadb_lifetime_usetime);
}

static void
print_ident(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_ident *ident = (struct sadb_ident *)ext;

	printf("type %s id %llu: %s\n",
	    lookup_name(identity_types, ident->sadb_ident_type),
	    ident->sadb_ident_id, (char *)(ident + 1));
}

static void
print_auth(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_x_cred *x_cred = (struct sadb_x_cred *)ext;

	printf("type %s\n",
	    lookup_name(xauth_types, x_cred->sadb_x_cred_type));
}

static void
print_cred(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_x_cred *x_cred = (struct sadb_x_cred *)ext;
	printf("type %s\n",
	    lookup_name(cred_types, x_cred->sadb_x_cred_type));
}

static void
print_udpenc(struct sadb_ext *ext, struct sadb_msg *msg)
{
	struct sadb_x_udpencap *x_udpencap = (struct sadb_x_udpencap *)ext;

	printf("udpencap port %u\n", ntohs(x_udpencap->sadb_x_udpencap_port));
}

void
pfkey_print_sa(struct sadb_msg *msg, int opts)
{
	struct sadb_ext *ext;
	int		 i;

	bzero(extensions, sizeof(extensions));

	printf("%s ", lookup_name(sa_types, msg->sadb_msg_satype));
	for (ext = (struct sadb_ext *)(msg + 1);
	    (size_t)((u_int8_t *)ext - (u_int8_t *)msg) <
	    msg->sadb_msg_len * PFKEYV2_CHUNK && ext->sadb_ext_len > 0;
	    ext = (struct sadb_ext *)((u_int8_t *)ext +
	    ext->sadb_ext_len * PFKEYV2_CHUNK))
		extensions[ext->sadb_ext_type] = ext;

	for (i = 0; i < SADB_EXT_MAX; i++)
		if (extensions[i])
			print_ext(extensions[i], msg, opts);
	fflush(stdout);
}
