/* $OpenBSD: ipsecadm.c,v 1.79 2004/05/23 16:04:18 markus Exp $ */
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece,
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 * Copyright (c) 2001, Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/stat.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netns/ns.h>
#include <netiso/iso.h>
#include <netccitt/x25.h>
#include <net/pfkeyv2.h>
#include <netinet/ip_ipsp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <err.h>

#define KEYSIZE_LIMIT	1024

#define ESP_OLD		0x0001
#define ESP_NEW		0x0002
#define AH_OLD		0x0004
#define AH_NEW		0x0008

#define XF_ENC		0x0100
#define XF_AUTH		0x0200
#define DEL_SPI		0x0300
#define GRP_SPI		0x0400
#define FLOW		0x0500
#define FLUSH		0x0700
#define XF_COMP		0x0900
#define SHOW		0x0a00
#define MONITOR		0x0b00

/* pseudo commands */
#define IPCOMP		0x1000
#define TCPMD5		0x2000
#define ENC_IP		0x4000

#define CMD_MASK	0xff00
#define XFORM_MASK	0x0f00

#define isencauth(x)	((x)&~CMD_MASK)
#define iscmd(x,y)	(((x) & CMD_MASK) == (y))

typedef struct {
	char	*name;
	int	id, flags;
} transform;

transform xf[] = {
	{ "des", SADB_EALG_DESCBC, XF_ENC | ESP_OLD | ESP_NEW },
	{ "3des", SADB_EALG_3DESCBC, XF_ENC | ESP_OLD | ESP_NEW },
	{ "aes", SADB_X_EALG_AES, XF_ENC | ESP_NEW },
	{ "blf", SADB_X_EALG_BLF, XF_ENC | ESP_NEW },
	{ "cast", SADB_X_EALG_CAST, XF_ENC | ESP_NEW },
	{ "skipjack", SADB_X_EALG_SKIPJACK, XF_ENC | ESP_NEW },
	{ "md5", SADB_AALG_MD5HMAC, XF_AUTH | AH_NEW | ESP_NEW },
	{ "sha1", SADB_AALG_SHA1HMAC, XF_AUTH | AH_NEW | ESP_NEW },
	{ "sha2-256", SADB_X_AALG_SHA2_256, XF_AUTH | AH_NEW | ESP_NEW },
	{ "sha2-384", SADB_X_AALG_SHA2_384, XF_AUTH | AH_NEW | ESP_NEW },
	{ "sha2-512", SADB_X_AALG_SHA2_512, XF_AUTH | AH_NEW | ESP_NEW },
	{ "md5", SADB_X_AALG_MD5, XF_AUTH | AH_OLD },
	{ "sha1", SADB_X_AALG_SHA1, XF_AUTH | AH_OLD },
	{ "rmd160", SADB_X_AALG_RIPEMD160HMAC, XF_AUTH | AH_NEW | ESP_NEW },
	{ "deflate", SADB_X_CALG_DEFLATE, XF_COMP | IPCOMP },
	{ "lzs", SADB_X_CALG_LZS, XF_COMP | IPCOMP },
};

#define ROUNDUP(x) (((x) + sizeof(u_int64_t) - 1) & ~(sizeof(u_int64_t) - 1))

extern void ipsecadm_monitor(void);
extern void ipsecadm_show(u_int8_t);
int	addrparse(const char *, struct sockaddr *, struct sockaddr *);
void	xf_set(struct iovec *, int, int);
int	x2i(u_char *);
int	isvalid(char *, int, int);
void	usage(void);

/*
 * returns 0 if "str" represents an address, returns 1 if address/mask,
 * returns -1 on failure.
 */
int
addrparse(const char *str, struct sockaddr *addr, struct sockaddr *mask)
{
	struct addrinfo hints, *res = NULL;
	char	*p = NULL, *sp, *ep;
	u_long	prefixlen = 0;
	u_char	*ap;
	int	bitlen;

	/* slash */
	if (mask && (p = strchr(str, '/')) != NULL) {
		if (!p[1])
			return -1;
		ep = NULL;
		prefixlen = strtoul(p + 1, &ep, 10);
		if (*ep)
			return -1;

		sp = strdup(str);
		if (!sp)
			return -1;
		sp[p - str] = '\0';
		str = sp;
	} else
		sp = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;	/* dummy */
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_family = PF_UNSPEC;
	if (getaddrinfo(str, "0", &hints, &res) != 0)
		return -1;
	if (res->ai_next)
		goto fail;

	memcpy(addr, res->ai_addr, res->ai_addrlen);

	if (!p) {
		freeaddrinfo(res);
		if (sp)
			free(sp);
		return 0;
	}
	switch (res->ai_family) {
	case AF_INET:
		ap = (u_char *) & ((struct sockaddr_in *) mask)->sin_addr;
		bitlen = 32;
		break;
	case AF_INET6:
		ap = (u_char *) & ((struct sockaddr_in6 *) mask)->sin6_addr;
		bitlen = 128;
		break;
	default:
		goto fail;
	}

	if (prefixlen > bitlen)
		goto fail;

	memset(mask, 0, addr->sa_len);
	mask->sa_len = addr->sa_len;
	mask->sa_family = addr->sa_family;
	memset(ap, 0xff, prefixlen / 8);
	if (prefixlen % 8)
		ap[prefixlen / 8] = (0xff00 >> (prefixlen % 8)) & 0xff;

	if (res)
		freeaddrinfo(res);
	if (sp)
		free(sp);
	return 1;

fail:
	if (res)
		freeaddrinfo(res);
	if (sp)
		free(sp);
	return -1;
}

void
xf_set(struct iovec *iov, int cnt, int len)
{
	struct sadb_msg sm;
	int sd;

	sd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2);
	if (sd < 0) {
		perror("socket");
		if (errno == EPROTONOSUPPORT)
			fprintf(stderr,
				"Make sure your kernel is compiled with option KEY\n");
		exit(1);
	}
	if (writev(sd, iov, cnt) != len) {
		perror("write");
		exit(1);
	}
	if (read(sd, &sm, sizeof(sm)) != sizeof(sm)) {
		perror("read");
		exit(1);
	}
	if (sm.sadb_msg_errno != 0) {
		/* XXX We need better error reporting than this */
		errno = sm.sadb_msg_errno;
		perror("pfkey");
		exit(1);
	}
	close(sd);
}

int
x2i(u_char *s)
{
	char ss[3];

	ss[0] = s[0];
	ss[1] = s[1];
	ss[2] = 0;

	if (!isxdigit(s[0]) || !isxdigit(s[1])) {
		fprintf(stderr, "Keys should be specified in hex digits.\n");
		exit(1);
	}
	return strtoul(ss, NULL, 16);
}

int
isvalid(char *option, int type, int mode)
{
	int i;

	for (i = sizeof(xf) / sizeof(transform) - 1; i >= 0; i--)
		if (!strcmp(option, xf[i].name) &&
		    (xf[i].flags & XFORM_MASK) == type &&
		    (xf[i].flags & mode)) {
			goto gotit;
		}
	return 0;
gotit:
	if (!strcmp(option, "des") || !strcmp(option, "skipjack"))
		fprintf(stderr,
		    "Warning: use of %s is strongly discouraged due to"
		    " cryptographic weaknesses\n", option);
	return xf[i].id;
}

void
usage(void)
{
	fprintf(stderr, "usage: ipsecadm [command] <modifier...>\n"
	    "\tCommands: new esp, old esp, new ah, old ah, group, delspi, ip4, ipcomp,\n"
	    "\t\t  tcpmd5, flow, flush, show, monitor\n"
	    "\tPossible modifiers:\n"
	    "\t  -enc <alg>\t\t\tencryption algorithm\n"
	    "\t  -auth <alg>\t\t\tauthentication algorithm\n"
	    "\t  -comp <alg>\t\t\tcompression algorithm\n"
	    "\t  -src <ip>\t\t\tsource address to be used\n"
	    "\t  -halfiv\t\t\tuse 4-byte IV in old ESP\n"
	    "\t  -forcetunnel\t\t\tforce IP-in-IP encapsulation\n"
	    "\t  -udpencap <port>\t\tenable ESP-in-UDP encapsulation\n"
	    "\t  -dst <ip>\t\t\tdestination address to be used\n"
	    "\t  -proto <val>\t\t\tsecurity protocol\n"
	    "\t  -proxy <ip>\t\t\tproxy address to be used\n"
	    "\t  -spi <val>\t\t\tSPI to be used\n"
	    "\t  -cpi <val>\t\t\tCPI to be used\n"
	    "\t  -key <val>\t\t\tkey material to be used\n"
	    "\t  -keyfile <file>\t\tfile to read key material from\n"
	    "\t  -authkey <val>\t\tkey material for auth in new esp\n"
	    "\t  -authkeyfile <file>\t\tfile to read authkey material from\n"
	    "\t  -sport\t\t\tsource port for flow\n"
	    "\t  -dport\t\t\tdestination port for flow\n"
	    "\t  -transport <val>\t\tprotocol number for flow\n"
	    "\t  -addr [<ip> <net> <ip> <net>|<ip/len> <ip/len>] subnets for flow\n"
	    "\t  -delete\t\t\tdelete specified flow\n"
	    "\t  -bypass\t\t\tpermit a flow through without IPsec\n"
	    "\t  -permit\t\t\tsame as bypass\n"
	    "\t  -deny\t\t\t\tcreate a deny-packets flow\n"
	    "\t  -use\t\t\t\tuse an SA for a flow if it exists\n"
	    "\t  -acquire\t\t\tsend unprotected while acquiring SA\n"
	    "\t  -require\t\t\trequire an SA for a flow, use key mgmt.\n"
	    "\t  -dontacq\t\t\trequire, without using key mgmt.\n"
	    "\t  -in\t\t\t\tspecify incoming-packet policy\n"
	    "\t  -out\t\t\t\tspecify outgoing-packet policy\n"
	    "\t  -[ah|esp|ip4|ipcomp|tcpmd5]\t\tflush a particular protocol\n"
	    "\t  -srcid\t\t\tsource identity for flows\n"
	    "\t  -dstid\t\t\tdestination identity for flows\n"
	    "\t  -srcid_type\t\t\tsource identity type\n"
	    "\t  -dstid_type\t\t\tdestination identity type\n"
	    "\talso: dst2, spi2, proto2\n"
	);
}

int
main(int argc, char *argv[])
{
	int auth = 0, enc = 0, klen = 0, alen = 0, mode = ESP_NEW, i = 0;
	int proto = IPPROTO_ESP, proto2 = IPPROTO_AH, sproto2 = SADB_SATYPE_AH;
	int dport = -1, sport = -1, tproto = -1;
	int srcset = 0, dstset = 0, dst2set = 0, proxyset = 0;
	int cnt = 0, bypass = 0, deny = 0, ipsec = 0, comp = 0;
	u_int32_t spi = SPI_LOCAL_USE, spi2 = SPI_LOCAL_USE;
	u_int32_t cpi = SPI_LOCAL_USE;
	union sockaddr_union *src, *dst, *dst2, *osrc, *odst, *osmask;
	union sockaddr_union *odmask, *proxy;
	u_char srcbuf[256], dstbuf[256], dst2buf[256], osrcbuf[256];
	u_char odstbuf[256], osmaskbuf[256], odmaskbuf[256], proxybuf[256];
	u_char *keyp = NULL, *authp = NULL;
	u_char *srcid = NULL, *dstid = NULL;
	struct protoent *tp;
	struct servent *svp;
	char *transportproto = NULL;
	struct sadb_msg smsg;
	struct sadb_sa sa, sa2;
	struct sadb_address sad1, sad2, sad3;	/* src, dst, proxy */
	struct sadb_address sad4, sad5;		/* osrc, odst */
	struct sadb_address sad6, sad7, sad8;	/* osmask, odmask, dst2 */
	struct sadb_ident sid1, sid2;
	struct sadb_key skey1, skey2;
	struct sadb_protocol sprotocol, sprotocol2;
	struct sadb_x_udpencap udpencap;	/* Peer UDP Port */
	u_char realkey[8192], realakey[8192];
	struct iovec iov[30];
	struct addrinfo hints, *res;
	const char *errstr;
	char *ep;

	if (argc < 2) {
		usage();
		exit(1);
	}

	/* Zero out */
	memset(&smsg, 0, sizeof(smsg));
	memset(&sa, 0, sizeof(sa));
	memset(&sa2, 0, sizeof(sa2));
	memset(&skey1, 0, sizeof(skey1));
	memset(&skey2, 0, sizeof(skey2));
	memset(&sad1, 0, sizeof(sad1));
	memset(&sad2, 0, sizeof(sad2));
	memset(&sad3, 0, sizeof(sad3));
	memset(&sad4, 0, sizeof(sad4));
	memset(&sad5, 0, sizeof(sad5));
	memset(&sad6, 0, sizeof(sad6));
	memset(&sad7, 0, sizeof(sad7));
	memset(&sad8, 0, sizeof(sad8));
	memset(&sprotocol, 0, sizeof(sprotocol));
	memset(&sprotocol2, 0, sizeof(sprotocol2));
	memset(iov, 0, sizeof(iov));
	memset(realkey, 0, sizeof(realkey));
	memset(realakey, 0, sizeof(realakey));
	memset(&sid1, 0, sizeof(sid1));
	memset(&sid2, 0, sizeof(sid2));
	memset(&udpencap, 0, sizeof(udpencap));

	src = (union sockaddr_union *) srcbuf;
	dst = (union sockaddr_union *) dstbuf;
	dst2 = (union sockaddr_union *) dst2buf;
	osrc = (union sockaddr_union *) osrcbuf;
	odst = (union sockaddr_union *) odstbuf;
	osmask = (union sockaddr_union *) osmaskbuf;
	odmask = (union sockaddr_union *) odmaskbuf;
	proxy = (union sockaddr_union *) proxybuf;

	memset(srcbuf, 0, sizeof(srcbuf));
	memset(dstbuf, 0, sizeof(dstbuf));
	memset(dst2buf, 0, sizeof(dst2buf));
	memset(osrcbuf, 0, sizeof(osrcbuf));
	memset(odstbuf, 0, sizeof(odstbuf));
	memset(osmaskbuf, 0, sizeof(osmaskbuf));
	memset(odmaskbuf, 0, sizeof(odmaskbuf));
	memset(proxybuf, 0, sizeof(proxybuf));

	/* Initialize */
	smsg.sadb_msg_version = PF_KEY_V2;
	smsg.sadb_msg_seq = 1;
	smsg.sadb_msg_pid = getpid();
	smsg.sadb_msg_len = sizeof(smsg) / 8;

	/* Initialize */
	sa.sadb_sa_exttype = SADB_EXT_SA;
	sa.sadb_sa_len = sizeof(sa) / 8;
	sa.sadb_sa_replay = 0;
	sa.sadb_sa_state = SADB_SASTATE_MATURE;

	sa2.sadb_sa_exttype = SADB_X_EXT_SA2;
	sa2.sadb_sa_len = sizeof(sa) / 8;
	sa2.sadb_sa_replay = 0;
	sa2.sadb_sa_state = SADB_SASTATE_MATURE;

	sid1.sadb_ident_len = sizeof(sid1) / 8;
	sid1.sadb_ident_exttype = SADB_EXT_IDENTITY_SRC;

	sid2.sadb_ident_len = sizeof(sid2) / 8;
	sid2.sadb_ident_exttype = SADB_EXT_IDENTITY_DST;

	sprotocol2.sadb_protocol_len = 1;
	sprotocol2.sadb_protocol_exttype = SADB_X_EXT_FLOW_TYPE;
	sprotocol2.sadb_protocol_direction = IPSP_DIRECTION_OUT;
	sprotocol2.sadb_protocol_flags = SADB_X_POLICYFLAGS_POLICY;
	sprotocol.sadb_protocol_exttype = SADB_X_EXT_PROTOCOL;
	sprotocol.sadb_protocol_len = 1;

	if (!strcmp(argv[1], "new") && argc > 3) {
		if (!strcmp(argv[2], "esp")) {
			mode = ESP_NEW;
			smsg.sadb_msg_type = SADB_ADD;
			smsg.sadb_msg_satype = SADB_SATYPE_ESP;
		} else if (!strcmp(argv[2], "ah")) {
			mode = AH_NEW;
			smsg.sadb_msg_type = SADB_ADD;
			smsg.sadb_msg_satype = SADB_SATYPE_AH;
		} else {
			fprintf(stderr, "%s: unexpected identifier %s\n", argv[0],
			    argv[2]);
			exit(1);
		}

		i += 2;
	} else if (!strcmp(argv[1], "old") && argc > 3) {
		if (!strcmp(argv[2], "esp")) {
			mode = ESP_OLD;
			smsg.sadb_msg_type = SADB_ADD;
			smsg.sadb_msg_satype = SADB_SATYPE_ESP;
			sa.sadb_sa_flags |= SADB_X_SAFLAGS_RANDOMPADDING;
			sa.sadb_sa_flags |= SADB_X_SAFLAGS_NOREPLAY;
		} else if (!strcmp(argv[2], "ah")) {
			mode = AH_OLD;
			smsg.sadb_msg_type = SADB_ADD;
			smsg.sadb_msg_satype = SADB_SATYPE_AH;
			sa.sadb_sa_flags |= SADB_X_SAFLAGS_NOREPLAY;
		} else {
			fprintf(stderr, "%s: unexpected identifier %s\n", argv[0],
			    argv[2]);
			exit(1);
		}

		i += 2;
	} else if (!strcmp(argv[1], "delspi")) {
		smsg.sadb_msg_type = SADB_DELETE;
		smsg.sadb_msg_satype = SADB_SATYPE_ESP;
		mode = DEL_SPI;
		i++;
	} else if (!strcmp(argv[1], "group")) {
		smsg.sadb_msg_type = SADB_X_GRPSPIS;
		smsg.sadb_msg_satype = SADB_SATYPE_ESP;
		mode = GRP_SPI;
		i++;
	} else if (!strcmp(argv[1], "flow")) {
		/* It may not be ADDFLOW, but never mind that for now */
		smsg.sadb_msg_type = SADB_X_ADDFLOW;
		smsg.sadb_msg_satype = SADB_SATYPE_ESP;
		mode = FLOW;
		i++;
	} else if (!strcmp(argv[1], "flush")) {
		mode = FLUSH;
		smsg.sadb_msg_type = SADB_FLUSH;
		smsg.sadb_msg_satype = SADB_SATYPE_UNSPEC;
		i++;
	} else if (!strcmp(argv[1], "ip4")) {
		mode = ENC_IP;
		smsg.sadb_msg_type = SADB_ADD;
		smsg.sadb_msg_satype = SADB_X_SATYPE_IPIP;
		i++;
	} else if (!strcmp(argv[1], "tcpmd5")) {
		mode = TCPMD5;
		smsg.sadb_msg_type = SADB_ADD;
		smsg.sadb_msg_satype = SADB_X_SATYPE_TCPSIGNATURE;
		i++;
	} else if (!strcmp(argv[1], "ipcomp")) {
		mode = IPCOMP;
		smsg.sadb_msg_type = SADB_ADD;
		smsg.sadb_msg_satype = SADB_X_SATYPE_IPCOMP;
		i++;
	} else if (!strcmp(argv[1], "monitor")) {
		mode = MONITOR;
		i++;
	} else if (!strcmp(argv[1], "show")) {
		mode = SHOW;
		smsg.sadb_msg_satype = SADB_SATYPE_UNSPEC;
		i++;
	} else {
		fprintf(stderr, "%s: unknown command: %s\n", argv[0],
		    argv[1]);
		usage();
		exit(1);
	}

	for (i++; i < argc; i++) {
		if (argv[i][0] != '-') {
			fprintf(stderr, "%s: expected option, got %s\n",
			    argv[0], argv[i]);
			exit(1);
		}
		if (!strcmp(argv[i] + 1, "enc") && enc == 0 && (i + 1 < argc)) {
			if ((enc = isvalid(argv[i + 1], XF_ENC, mode)) == 0) {
				fprintf(stderr, "%s: invalid encryption algorithm %s\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}
			skey1.sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
			sa.sadb_sa_encrypt = enc;
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "auth") && auth == 0 && (i + 1 < argc)) {
			if ((auth = isvalid(argv[i + 1], XF_AUTH, mode)) == 0) {
				fprintf(stderr, "%s: invalid auth algorithm %s\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}
			skey2.sadb_key_exttype = SADB_EXT_KEY_AUTH;
			sa.sadb_sa_auth = auth;
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "comp") && comp == 0 && (i + 1 < argc)) {
			if ((comp = isvalid(argv[i + 1], XF_COMP, mode)) == 0) {
				fprintf(stderr, "%s: invalid comp algorithm %s\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}
			/*
			 * Use encryption algo slot to store compression algo
			 * since we cannot modify sadb_sa
			 */
			sa.sadb_sa_encrypt = comp;
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "key") && keyp == NULL &&
		    (i + 1 < argc)) {
			if (mode & (AH_NEW | AH_OLD | TCPMD5)) {
				authp = (u_char *)argv[++i];
				alen = strlen((char *)authp) / 2;
			} else {
				keyp = (u_char *)argv[++i];
				klen = strlen((char *)keyp) / 2;
			}
			continue;
		}
		if (!strcmp(argv[i] + 1, "keyfile") && keyp == NULL &&
		    (i + 1 < argc)) {
			struct stat sb;
			u_char *pptr;
			int fd;

			if (stat(argv[++i], &sb) < 0) {
				perror("stat()");
				exit(1);
			}
			if ((sb.st_size > KEYSIZE_LIMIT) || (sb.st_size == 0)) {
				fprintf(stderr,
				    "%s: file %s is too %s "
				    "(must be between 1 and %d bytes).\nb",
				    argv[0], argv[i],
				    sb.st_size ? "large" : "small", KEYSIZE_LIMIT);
				exit(1);
			}
			pptr = malloc((int)sb.st_size);
			if (pptr == NULL) {
				perror("malloc()");
				exit(1);
			}
			fd = open(argv[i], O_RDONLY);
			if (fd < 0) {
				perror("open()");
				exit(1);
			}
			if (read(fd, pptr, (size_t)sb.st_size) < sb.st_size) {
				perror("read()");
				exit(1);
			}
			close(fd);

			if (mode & (AH_NEW | AH_OLD | TCPMD5)) {
				authp = pptr;
				alen = sb.st_size / 2;
			} else {
				keyp = pptr;
				klen = sb.st_size / 2;
			}
			continue;
		}
		if (!strcmp(argv[i] + 1, "authkeyfile") && authp == NULL &&
		    (i + 1 < argc)) {
			struct stat sb;
			int fd;

			if (!(mode & ESP_NEW)) {
				fprintf(stderr,
				    "%s: invalid option %s for selected mode\n",
				    argv[0], argv[i]);
				exit(1);
			}
			if (stat(argv[++i], &sb) < 0) {
				perror("stat()");
				exit(1);
			}
			if ((sb.st_size > KEYSIZE_LIMIT) || (sb.st_size == 0)) {
				fprintf(stderr,
				    "%s: file %s is too %s "
				    "(must be between 1 and %d bytes).\n",
				    argv[0], argv[i],
				    sb.st_size ? "large" : "small", KEYSIZE_LIMIT);
				exit(1);
			}
			authp = malloc((int)sb.st_size);
			if (authp == NULL) {
				perror("malloc()");
				exit(1);
			}
			fd = open(argv[i], O_RDONLY);
			if (fd < 0) {
				perror("open()");
				exit(1);
			}
			if (read(fd, authp, (int)sb.st_size) < sb.st_size) {
				perror("read()");
				exit(1);
			}
			close(fd);

			alen = sb.st_size / 2;
			continue;
		}
		if (!strcmp(argv[i] + 1, "authkey") && authp == NULL &&
		    (i + 1 < argc)) {
			if (!(mode & ESP_NEW)) {
				fprintf(stderr,
				    "%s: invalid option %s for selected mode\n",
				    argv[0], argv[i]);
				exit(1);
			}
			authp = (u_char *)argv[++i];
			alen = strlen((char *)authp) / 2;
			continue;
		}
		if (!strcmp(argv[i] + 1, "iv") && (i + 1 < argc)) {
			if (mode & (AH_OLD | AH_NEW)) {
				fprintf(stderr, "%s: invalid option %s with auth\n",
				    argv[0], argv[i]);
				exit(1);
			}
			fprintf(stderr,
			    "%s: Warning: option iv has been deprecated\n", argv[0]);

			/* Horrible hack */
			if (mode & ESP_OLD)
				if (strlen(argv[i + 2]) == 4)
					sa.sadb_sa_flags |= SADB_X_SAFLAGS_HALFIV;

			i++;
			continue;
		}
		if ((iscmd(mode, FLUSH) || iscmd(mode, SHOW)) &&
		    smsg.sadb_msg_satype == SADB_SATYPE_UNSPEC) {
			if (!strcmp(argv[i] + 1, "esp"))
				smsg.sadb_msg_satype = SADB_SATYPE_ESP;
			else if (!strcmp(argv[i] + 1, "ah"))
				smsg.sadb_msg_satype = SADB_SATYPE_AH;
			else if (!strcmp(argv[i] + 1, "ip4"))
				smsg.sadb_msg_satype = SADB_X_SATYPE_IPIP;
			else if (!strcmp(argv[i] + 1, "tcpmd5"))
				smsg.sadb_msg_satype = SADB_X_SATYPE_TCPSIGNATURE;
			else if (!strcmp(argv[i] + 1, "ipcomp"))
				smsg.sadb_msg_satype = SADB_X_SATYPE_IPCOMP;
			else {
				fprintf(stderr, "%s: invalid SA type %s\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "spi") && iscmd(mode, FLOW)) {
			fprintf(stderr, "%s: use of flag \"-spi\" is deprecated with "
			    "flow creation or deletion\n", argv[0]);
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "spi") && spi == SPI_LOCAL_USE &&
		    (i + 1 < argc) && !bypass && !deny) {
			spi = strtoul(argv[i + 1], &ep, 16);
			if ((argv[i + 1] == '\0' || *ep != '\0') ||
			    (errno == ERANGE && spi == ULONG_MAX) ||
			    (spi >= SPI_RESERVED_MIN && spi <= SPI_RESERVED_MAX))
				errx(1, "invalid spi %s", argv[i + 1]);
			sa.sadb_sa_spi = htonl(spi);
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "spi2") && spi2 == SPI_LOCAL_USE &&
		    iscmd(mode, GRP_SPI) && (i + 1 < argc)) {
			spi2 = strtoul(argv[i + 1], &ep, 16);
			if ((argv[i + 1] == '\0' || *ep != '\0') ||
			    (errno == ERANGE && spi == ULONG_MAX) ||
			    (spi2 >= SPI_RESERVED_MIN && spi2 <= SPI_RESERVED_MAX))
				errx(1, "invalid spi2 %s", argv[i + 1]);
			sa2.sadb_sa_spi = htonl(spi2);
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "cpi") && cpi == SPI_LOCAL_USE &&
		    (i + 1 < argc) && !bypass && !deny) {
			cpi = strtoul(argv[i + 1], &ep, 16);
			if ((argv[i + 1] == '\0' || *ep != '\0') ||
			    (errno == ERANGE && spi == ULONG_MAX) ||
			    (cpi >= CPI_RESERVED_MIN && cpi <= CPI_RESERVED_MAX) ||
			    (cpi > USHRT_MAX))
				errx(1, "invalid cpi %s", argv[i + 1]);
			sa.sadb_sa_spi = ntohl(cpi);
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "dst2") &&
		    iscmd(mode, GRP_SPI) && (i + 1 < argc)) {
			sad8.sadb_address_exttype = SADB_X_EXT_DST2;
			memset(&hints, 0, sizeof(hints));
			hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
			hints.ai_family = PF_UNSPEC;
			if (getaddrinfo(argv[i + 1], "0", &hints, &res) != 0) {
				fprintf(stderr,
				    "%s: destination address2 %s is not valid\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			if (res->ai_next) {
				fprintf(stderr,
				    "%s: destination address2 %s resolves to multiple addresses\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			switch (res->ai_family) {
			case AF_INET6:
				if (res->ai_addrlen != sizeof(dst2->sin6)) {
					fprintf(stderr,
					    "%s: destination address2 %s resolves to unexpected address\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
				memcpy(&dst2->sin6, res->ai_addr,
				    sizeof(dst2->sin6));
				dst2set = 1;
				break;
			case AF_INET:
				if (res->ai_addrlen != sizeof(dst2->sin)) {
					fprintf(stderr,
					    "%s: destination address2 %s resolves to unexpected address\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
				memcpy(&dst2->sin, res->ai_addr,
				    sizeof(dst2->sin));
				dst2set = 1;
				break;
			default:
				fprintf(stderr,
				    "%s: destination address2 %s resolved to unsupported address family\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			freeaddrinfo(res);

			if (dst2set == 0) {
				fprintf(stderr,
				    "%s: destination address2 %s is not valid\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "src") && (i + 1 < argc)) {
			sad1.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
			memset(&hints, 0, sizeof(hints));
			hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
			hints.ai_family = PF_UNSPEC;
			if (getaddrinfo(argv[i + 1], "0", &hints, &res) != 0) {
				fprintf(stderr,
				    "%s: source address %s is not valid\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			if (res->ai_next) {
				fprintf(stderr,
				    "%s: source address %s resolves to multiple addresses\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			switch (res->ai_family) {
			case AF_INET6:
				if (res->ai_addrlen != sizeof(src->sin6)) {
					fprintf(stderr,
					    "%s: source address %s resolves to unexpected address\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
				memcpy(&src->sin6, res->ai_addr,
				    sizeof(src->sin6));
				srcset = 1;
				sad1.sadb_address_len = (sizeof(sad1) +
				    ROUNDUP(sizeof(struct sockaddr_in6))) / 8;
				break;
			case AF_INET:
				if (res->ai_addrlen != sizeof(src->sin)) {
					fprintf(stderr,
					    "%s: source address %s resolves to unexpected address\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
				memcpy(&src->sin, res->ai_addr,
				    sizeof(src->sin));
				srcset = 1;
				sad1.sadb_address_len = (sizeof(sad1) +
				    ROUNDUP(sizeof(struct sockaddr_in))) / 8;
				break;
			default:
				fprintf(stderr,
				    "%s: source address %s resolved to unsupported address family\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			freeaddrinfo(res);

			if (srcset == 0) {
				fprintf(stderr,
				    "%s: source address %s is not valid\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "proxy") && (i + 1 < argc) && !deny &&
		    !bypass && !ipsec) {
			sad3.sadb_address_exttype = SADB_EXT_ADDRESS_PROXY;
			memset(&hints, 0, sizeof(hints));
			hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
			hints.ai_family = PF_UNSPEC;
			if (getaddrinfo(argv[i + 1], "0", &hints, &res) != 0) {
				fprintf(stderr,
				    "%s: proxy address %s is not valid\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			if (res->ai_next) {
				fprintf(stderr,
				    "%s: source address %s resolves to multiple addresses\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			switch (res->ai_family) {
			case AF_INET6:
				if (res->ai_addrlen != sizeof(proxy->sin6)) {
					fprintf(stderr,
					    "%s: source address %s resolves to unexpected address\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
				memcpy(&proxy->sin6, res->ai_addr,
				    sizeof(proxy->sin6));
				proxyset = 1;
				sad3.sadb_address_len = (sizeof(sad3) +
				    ROUNDUP(sizeof(struct sockaddr_in6))) / 8;
				break;
			case AF_INET:
				if (res->ai_addrlen != sizeof(proxy->sin)) {
					fprintf(stderr,
					    "%s: source address %s resolves to unexpected address\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
				memcpy(&proxy->sin, res->ai_addr,
				    sizeof(proxy->sin));
				proxyset = 1;
				sad3.sadb_address_len = (sizeof(sad3) +
				    ROUNDUP(sizeof(struct sockaddr_in))) / 8;
				break;
			default:
				fprintf(stderr,
				    "%s: proxy address %s resolved to unsupported address family\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			freeaddrinfo(res);

			if (proxyset == 0) {
				fprintf(stderr,
				    "%s: proxy address %s is not valid\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "newpadding")) {
			fprintf(stderr,
			    "%s: Warning: option newpadding has been deprecated\n",
			    argv[0]);
			continue;
		}
		if (!strcmp(argv[i] + 1, "in") && iscmd(mode, FLOW)) {
			sprotocol2.sadb_protocol_direction = IPSP_DIRECTION_IN;
			continue;
		}
		if (!strcmp(argv[i] + 1, "out") && iscmd(mode, FLOW)) {
			sprotocol2.sadb_protocol_direction = IPSP_DIRECTION_OUT;
			continue;
		}
		if (!strcmp(argv[i] + 1, "forcetunnel") && isencauth(mode)) {
			sa.sadb_sa_flags |= SADB_X_SAFLAGS_TUNNEL;
			continue;
		}
		if (!strcmp(argv[i] + 1, "udpencap") &&
		    udpencap.sadb_x_udpencap_port == 0 && (i + 1 < argc)) {
			if (!(mode & ESP_NEW)) {
				fprintf(stderr, "%s: option udpencap can "
				    "be used only with new ESP\n", argv[0]);
				exit(1);
			}
			sa.sadb_sa_flags |= SADB_X_SAFLAGS_UDPENCAP;
			udpencap.sadb_x_udpencap_exttype = SADB_X_EXT_UDPENCAP;
			udpencap.sadb_x_udpencap_len = sizeof(udpencap) / 8;
			udpencap.sadb_x_udpencap_port =
			    strtonum(argv[i + 1], 0, USHRT_MAX, &errstr);
			if (errstr)
				errx(1, "invalid port %s", argv[i + 1]);
			udpencap.sadb_x_udpencap_port =
			    htons(udpencap.sadb_x_udpencap_port);
			udpencap.sadb_x_udpencap_reserved = 0;
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "halfiv")) {
			if (!(mode & ESP_OLD)) {
				fprintf(stderr,
				    "%s: option halfiv can be used only with old ESP\n",
				    argv[0]);
				exit(1);
			}
			sa.sadb_sa_flags |= SADB_X_SAFLAGS_HALFIV;
			continue;
		}
		if (!strcmp(argv[i] + 1, "delete") && iscmd(mode, FLOW)) {
			smsg.sadb_msg_type = SADB_X_DELFLOW;
			continue;
		}
		if (!strcmp(argv[i] + 1, "local") && iscmd(mode, FLOW)) {
			fprintf(stderr,
			    "%s: Warning: option local has been deprecated\n",
			    argv[0]);
			continue;
		}
		if (!strcmp(argv[i] + 1, "tunnel") &&
		    (isencauth(mode) || mode == ENC_IP) && (i + 2 < argc)) {
			i += 2;
			sa.sadb_sa_flags |= SADB_X_SAFLAGS_TUNNEL;
			continue;
		}
		if (!strcmp(argv[i] + 1, "srcid") && (iscmd(mode, FLOW) ||
		    isencauth(mode)) && (i + 1 < argc)) {
			int len = ROUNDUP(strlen(argv[i + 1]) + 1);

			if (srcid != NULL) {
				fprintf(stderr, "%s: srcid specified multiple times\n",
				    argv[0]);
				exit(1);
			}
			srcid = calloc(len, sizeof(char));
			if (srcid == NULL) {
				fprintf(stderr, "%s: malloc failed\n", argv[0]);
				exit(1);
			}
			strlcpy((char *)srcid, argv[i + 1], len);
			sid1.sadb_ident_len += ROUNDUP(strlen((char *)srcid) + 1) /
			    sizeof(u_int64_t);
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "dstid") && (iscmd(mode, FLOW) ||
		    isencauth(mode)) && (i + 1 < argc)) {
			int len = ROUNDUP(strlen(argv[i + 1]) + 1);

			if (dstid != NULL) {
				fprintf(stderr, "%s: dstid specified multiple times\n",
				    argv[0]);
				exit(1);
			}
			dstid = calloc(len, sizeof(char));
			if (dstid == NULL) {
				fprintf(stderr, "%s: malloc failed\n", argv[0]);
				exit(1);
			}
			strlcpy((char *)dstid, argv[i + 1], len);
			sid2.sadb_ident_len += ROUNDUP(strlen((char *)dstid) + 1) /
			    sizeof(u_int64_t);
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "srcid_type") && (iscmd(mode, FLOW) ||
		    isencauth(mode)) && (i + 1 < argc)) {
			if (sid1.sadb_ident_type != 0) {
				fprintf(stderr,
				    "%s: srcid_type specified multiple times\n",
				    argv[0]);
				exit(1);
			}
			if (!strcmp(argv[i + 1], "prefix"))
				sid1.sadb_ident_type = SADB_IDENTTYPE_PREFIX;
			else if (!strcmp(argv[i + 1], "fqdn"))
				sid1.sadb_ident_type = SADB_IDENTTYPE_FQDN;
			else if (!strcmp(argv[i + 1], "ufqdn"))
				sid1.sadb_ident_type = SADB_IDENTTYPE_USERFQDN;
			else {
				fprintf(stderr, "%s: unknown identity type \"%s\"\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "dstid_type") && (iscmd(mode, FLOW) ||
		    isencauth(mode)) && (i + 1 < argc)) {
			if (sid2.sadb_ident_type != 0) {
				fprintf(stderr,
				    "%s: dstid_type specified multiple times\n",
				    argv[0]);
				exit(1);
			}
			if (!strcmp(argv[i + 1], "prefix"))
				sid2.sadb_ident_type = SADB_IDENTTYPE_PREFIX;
			else if (!strcmp(argv[i + 1], "fqdn"))
				sid2.sadb_ident_type = SADB_IDENTTYPE_FQDN;
			else if (!strcmp(argv[i + 1], "ufqdn"))
				sid2.sadb_ident_type = SADB_IDENTTYPE_USERFQDN;
			else {
				fprintf(stderr, "%s: unknown identity type \"%s\"\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "addr") && iscmd(mode, FLOW) &&
		    (i + 1 < argc)) {
			int advance;

			sad4.sadb_address_exttype = SADB_X_EXT_SRC_FLOW;
			sad5.sadb_address_exttype = SADB_X_EXT_DST_FLOW;
			sad6.sadb_address_exttype = SADB_X_EXT_SRC_MASK;
			sad7.sadb_address_exttype = SADB_X_EXT_DST_MASK;

			switch (addrparse(argv[i + 1], &osrc->sa, &osmask->sa)) {
			case 0:
				advance = 4;
				if (i + 4 >= argc)
					goto argfail;
				if (addrparse(argv[i + 2], &osmask->sa, NULL) != 0 ||
				    addrparse(argv[i + 3], &odst->sa, NULL) != 0 ||
				    addrparse(argv[i + 4], &odmask->sa, NULL) != 0) {
					fprintf(stderr,
					    "%s: Invalid address on -addr\n",
					    argv[0]);
					exit(1);
				}
				break;
			case 1:
				advance = 2;
				if (i + 2 >= argc)
					goto argfail;
				if (addrparse(argv[i + 2], &odst->sa,
				    &odmask->sa) != 1) {
					fprintf(stderr,
					    "%s: Invalid address on -addr\n", argv[0]);
					exit(1);
				}
				break;
			default:
				fprintf(stderr,
				    "%s: Invalid address %s on -addr\n", argv[0],
				    argv[i + 1]);
				goto argfail;
			}
			if (osrc->sa.sa_family != odst->sa.sa_family) {
				fprintf(stderr,
				    "%s: Mixed address families specified in addr\n",
				    argv[0]);
				exit(1);
			}
			sad4.sadb_address_len = (sizeof(sad4) +
			    ROUNDUP(osrc->sa.sa_len)) / 8;
			sad5.sadb_address_len = (sizeof(sad5) +
			    ROUNDUP(odst->sa.sa_len)) / 8;
			sad6.sadb_address_len = (sizeof(sad6) +
			    ROUNDUP(osmask->sa.sa_len)) / 8;
			sad7.sadb_address_len = (sizeof(sad7) +
			    ROUNDUP(odmask->sa.sa_len)) / 8;

			i += advance;
			continue;
		}
		if ((!strcmp(argv[i] + 1, "bypass") || !strcmp(argv[i] + 1, "permit"))
		    && iscmd(mode, FLOW) && !deny &&
		    !ipsec && !bypass) {
			/* Setup everything for a bypass flow */
			bypass = 1;
			sprotocol2.sadb_protocol_proto = SADB_X_FLOW_TYPE_BYPASS;
			continue;
		}
		if (!strcmp(argv[i] + 1, "deny") && iscmd(mode, FLOW) && !ipsec &&
		    !deny && !bypass) {
			/* Setup everything for a deny flow */
			deny = 1;
			sprotocol2.sadb_protocol_proto = SADB_X_FLOW_TYPE_DENY;
			continue;
		}
		if (!strcmp(argv[i] + 1, "use") && iscmd(mode, FLOW) && !deny &&
		    !bypass && !ipsec) {
			ipsec = 1;
			sprotocol2.sadb_protocol_proto = SADB_X_FLOW_TYPE_USE;
			continue;
		}
		if (!strcmp(argv[i] + 1, "acquire") && iscmd(mode, FLOW) && !deny &&
		    !bypass && !ipsec) {
			ipsec = 1;
			sprotocol2.sadb_protocol_proto = SADB_X_FLOW_TYPE_ACQUIRE;
			continue;
		}
		if (!strcmp(argv[i] + 1, "require") && iscmd(mode, FLOW) && !deny &&
		    !bypass && !ipsec) {
			ipsec = 1;
			sprotocol2.sadb_protocol_proto = SADB_X_FLOW_TYPE_REQUIRE;
			continue;
		}
		if (!strcmp(argv[i] + 1, "dontacq") && iscmd(mode, FLOW) && !deny &&
		    !bypass && !ipsec) {
			ipsec = 1;
			sprotocol2.sadb_protocol_proto = SADB_X_FLOW_TYPE_DONTACQ;
			continue;
		}
		if (!strcmp(argv[i] + 1, "transport") &&
		    iscmd(mode, FLOW) && (i + 1 < argc)) {
			if (isalpha(argv[i + 1][0])) {
				tp = getprotobyname(argv[i + 1]);
				if (tp == NULL) {
					fprintf(stderr,
					    "%s: unknown protocol %s\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
				tproto = tp->p_proto;
				transportproto = argv[i + 1];
			} else {
				tproto = strtonum(argv[i + 1], 0, INT_MAX, 
				    &errstr);
				if (errstr)
					errx(1, "bad protocol %s",
					    argv[i + 1]);
				tp = getprotobynumber(tproto);
				if (tp == NULL)
					transportproto = "UNKNOWN";
				else
					transportproto = tp->p_name;
			}

			sprotocol.sadb_protocol_len = 1;
			sprotocol.sadb_protocol_exttype = SADB_X_EXT_PROTOCOL;
			sprotocol.sadb_protocol_proto = tproto;
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "sport") &&
		    iscmd(mode, FLOW) && (i + 1 < argc)) {
			if (isalpha(argv[i + 1][0])) {
				svp = getservbyname(argv[i + 1], transportproto);
				if (svp == NULL) {
					fprintf(stderr,
					    "%s: unknown service port %s for protocol %s\n",
					    argv[0], argv[i + 1], transportproto);
					exit(1);
				}
				sport = svp->s_port;
			} else {
				sport = strtonum(argv[i + 1], 0, USHRT_MAX,
				    &errstr);
				if (errstr)
					errx(1, "invalid port %s", argv[i + 1]);
				sport = htons(sport);
			}
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "dport") &&
		    iscmd(mode, FLOW) && (i + 1 < argc)) {
			if (isalpha(argv[i + 1][0])) {
				svp = getservbyname(argv[i + 1], transportproto);
				if (svp == NULL) {
					fprintf(stderr,
					    "%s: unknown service port %s for protocol %s\n",
					    argv[0], argv[i + 1], transportproto);
					exit(1);
				}
				dport = svp->s_port;
			} else {
				dport = strtonum(argv[i + 1], 0, USHRT_MAX,
				    &errstr);
				if (errstr)
					errx(1, "invalid port %s", argv[i + 1]);
				dport = htons(dport);
			}
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "dst") && (i + 1 < argc) && !bypass && !deny) {
			sad2.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
			memset(&hints, 0, sizeof(hints));
			hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
			hints.ai_family = PF_UNSPEC;
			if (getaddrinfo(argv[i + 1], "0", &hints, &res) != 0) {
				fprintf(stderr,
				    "%s: destination address %s is not valid\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			if (res->ai_next) {
				fprintf(stderr,
				    "%s: destination address %s resolves to multiple addresses\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			switch (res->ai_family) {
			case AF_INET6:
				if (res->ai_addrlen != sizeof(dst->sin6)) {
					fprintf(stderr,
					    "%s: destination address %s resolves to unexpected address\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
				memcpy(&dst->sin6, res->ai_addr,
				    sizeof(dst->sin6));
				dstset = 1;
				sad2.sadb_address_len = (sizeof(sad2) +
				    ROUNDUP(sizeof(struct sockaddr_in6))) / 8;
				break;
			case AF_INET:
				if (res->ai_addrlen != sizeof(dst->sin)) {
					fprintf(stderr,
					    "%s: destination address %s resolves to unexpected address\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
				memcpy(&dst->sin, res->ai_addr,
				    sizeof(dst->sin));
				dstset = 1;
				sad2.sadb_address_len = (sizeof(sad2) +
				    ROUNDUP(sizeof(struct sockaddr_in))) / 8;
				break;
			default:
				fprintf(stderr,
				    "%s: destination address %s resolved to unsupported address family\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}

			freeaddrinfo(res);

			if (dstset == 0) {
				fprintf(stderr,
				    "%s: destination address %s is not valid\n",
				    argv[0], argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "proto2") &&
		    iscmd(mode, GRP_SPI) && (i + 1 < argc)) {
			if (isalpha(argv[i + 1][0])) {
				if (!strcasecmp(argv[i + 1], "esp")) {
					sprotocol.sadb_protocol_proto = sproto2 =
					    SADB_SATYPE_ESP;
					proto2 = IPPROTO_ESP;
				} else if (!strcasecmp(argv[i + 1], "ah")) {
					sprotocol.sadb_protocol_proto = sproto2 =
					    SADB_SATYPE_AH;
					proto2 = IPPROTO_AH;
				} else if (!strcasecmp(argv[i + 1], "ip4")) {
					sprotocol.sadb_protocol_proto = sproto2 =
					    SADB_X_SATYPE_IPIP;
					proto2 = IPPROTO_IPIP;
				} else if (!strcasecmp(argv[i + 1], "ipcomp")) {
					sprotocol.sadb_protocol_proto = sproto2 =
					    SADB_X_SATYPE_IPCOMP;
				} else {
					fprintf(stderr,
					    "%s: unknown security protocol2 type %s\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
			} else {
				proto2 = strtonum(argv[i + 1], 0, INT_MAX,
				    &errstr);
				if (errstr || (proto2 != IPPROTO_ESP && 
				    proto2 != IPPROTO_AH &&
				    proto2 != IPPROTO_IPIP && 
				    proto2 != IPPROTO_IPCOMP)) {
					errx(1, "unknown security protocol2 %s",
					    argv[i + 1]);
				}
				if (proto2 == IPPROTO_ESP)
					sprotocol.sadb_protocol_proto = sproto2 =
					    SADB_SATYPE_ESP;
				else if (proto2 == IPPROTO_AH)
					sprotocol.sadb_protocol_proto = sproto2 =
					    SADB_SATYPE_AH;
				else if (proto2 == IPPROTO_IPIP)
					sprotocol.sadb_protocol_proto = sproto2 =
					    SADB_X_SATYPE_IPIP;
				else if (proto2 == IPPROTO_IPCOMP)
					sprotocol.sadb_protocol_proto = sproto2 =
					    SADB_X_SATYPE_IPCOMP;
			}
			i++;
			continue;
		}
		if (!strcmp(argv[i] + 1, "proto") && (i + 1 < argc) &&
		    ((iscmd(mode, FLOW) && !bypass && !deny) || iscmd(mode, DEL_SPI) ||
		    iscmd(mode, GRP_SPI))) {
			if (isalpha(argv[i + 1][0])) {
				if (!strcasecmp(argv[i + 1], "esp")) {
					smsg.sadb_msg_satype = SADB_SATYPE_ESP;
					proto = IPPROTO_ESP;
				} else if (!strcasecmp(argv[i + 1], "ah")) {
					smsg.sadb_msg_satype = SADB_SATYPE_AH;
					proto = IPPROTO_AH;
				} else if (!strcasecmp(argv[i + 1], "ip4")) {
					smsg.sadb_msg_satype = SADB_X_SATYPE_IPIP;
					proto = IPPROTO_IPIP;
				} else if (!strcasecmp(argv[i + 1], "ipcomp")) {
					smsg.sadb_msg_satype = SADB_X_SATYPE_IPCOMP;
					proto = IPPROTO_IPCOMP;
				} else if (!strcasecmp(argv[i + 1], "tcpmd5")) {
					smsg.sadb_msg_satype = SADB_X_SATYPE_TCPSIGNATURE;
					proto = IPPROTO_TCP;
				} else {
					fprintf(stderr,
					    "%s: unknown security protocol type %s\n",
					    argv[0], argv[i + 1]);
					exit(1);
				}
			} else {
				proto = strtonum(argv[i + 1], 0, INT_MAX,
				    &errstr);
				if (errstr || (proto != IPPROTO_ESP && 
				    proto != IPPROTO_AH &&
				    proto != IPPROTO_IPIP && 
				    proto != IPPROTO_IPCOMP)) {
					errx(1, "unknown security protocol %s",
					    argv[i + 1]);
				}
				if (proto == IPPROTO_ESP)
					smsg.sadb_msg_satype = SADB_SATYPE_ESP;
				else if (proto == IPPROTO_AH)
					smsg.sadb_msg_satype = SADB_SATYPE_AH;
				else if (proto == IPPROTO_IPIP)
					smsg.sadb_msg_satype = SADB_X_SATYPE_IPIP;
				else if (proto == IPPROTO_IPCOMP)
					smsg.sadb_msg_satype = SADB_X_SATYPE_IPCOMP;
			}
			i++;
			continue;
		}
		/* No match */
argfail:
		fprintf(stderr, "%s: Unknown, invalid, or duplicated option: %s\n",
		    argv[0], argv[i]);
		exit(1);
	}

	if (iscmd(mode, SHOW)) {
		ipsecadm_show(smsg.sadb_msg_satype);
		exit(0);
	} else if (iscmd(mode, MONITOR)) {
		ipsecadm_monitor();
		exit(0);
	}

	/* Sanity checks */
	if ((mode & (ESP_NEW | ESP_OLD)) && enc == 0 && auth == 0) {
		fprintf(stderr, "%s: no encryption or authentication algorithm "
		    "specified\n", argv[0]);
		exit(1);
	}
	if (iscmd(mode, GRP_SPI) && spi2 == SPI_LOCAL_USE) {
		fprintf(stderr, "%s: no SPI2 specified\n", argv[0]);
		exit(1);
	}
	if ((mode & (AH_NEW | AH_OLD)) && auth == 0) {
		fprintf(stderr, "%s: no authentication algorithm specified\n",
		    argv[0]);
		exit(1);
	}
	if (iscmd(mode, IPCOMP) && comp == 0) {
		fprintf(stderr, "%s: no compression algorithm specified\n",
		    argv[0]);
		exit(1);
	}
	if ((srcid != NULL) && (sid1.sadb_ident_type == 0)) {
		fprintf(stderr, "%s: srcid_type not specified\n", argv[0]);
		exit(1);
	}
	if ((dstid != NULL) && (sid2.sadb_ident_type == 0)) {
		fprintf(stderr, "%s: dstid_type not specified\n", argv[0]);
		exit(1);
	}
	if ((srcid == NULL) && (sid1.sadb_ident_type != 0)) {
		fprintf(stderr, "%s: srcid_type specified, but no srcid given\n",
		    argv[0]);
		exit(1);
	}
	if ((dstid == NULL) && (sid2.sadb_ident_type != 0)) {
		fprintf(stderr, "%s: dstid_type specified, but no dstid given\n",
		    argv[0]);
		exit(1);
	}
	if (((mode & (ESP_NEW | ESP_OLD)) && enc && keyp == NULL) ||
	    ((mode & (AH_NEW | AH_OLD | TCPMD5)) && authp == NULL)) {
		fprintf(stderr, "%s: no key material specified\n", argv[0]);
		exit(1);
	}
	if ((mode & ESP_NEW) && auth && authp == NULL) {
		fprintf(stderr, "%s: no auth key material specified\n", argv[0]);
		exit(1);
	}
	if (spi == SPI_LOCAL_USE && !iscmd(mode, FLUSH) && !iscmd(mode, FLOW)
	    && !iscmd(mode, IPCOMP)) {
		fprintf(stderr, "%s: no SPI specified\n", argv[0]);
		exit(1);
	}
	if (iscmd(mode, IPCOMP) && cpi == SPI_LOCAL_USE) {
		fprintf(stderr, "%s: no CPI specified\n", argv[0]);
		exit(1);
	}
	if ((isencauth(mode) || iscmd(mode, ENC_IP)) && !srcset) {
		fprintf(stderr, "%s: no source address specified\n", argv[0]);
		exit(1);
	}
	if (!dstset && !iscmd(mode, FLUSH) && !iscmd(mode, FLOW)) {
		fprintf(stderr, "%s: no destination address for the SA specified\n",
		    argv[0]);
		exit(1);
	}
	if (iscmd(mode, FLOW) && (sprotocol.sadb_protocol_proto == 0) &&
	    (odst->sin.sin_port || osrc->sin.sin_port)) {
		fprintf(stderr,
		    "%s: no transport protocol supplied with"
		    " source/destination ports\n", argv[0]);
		exit(1);
	}
	if (iscmd(mode, GRP_SPI) && !dst2set) {
		fprintf(stderr, "%s: no destination address2 specified\n", argv[0]);
		exit(1);
	}
	if ((klen > 2 * 8100) || (alen > 2 * 8100)) {
		fprintf(stderr, "%s: key too long\n", argv[0]);
		exit(1);
	}
	if (iscmd(mode, FLOW) && proto == IPPROTO_IPCOMP) {
		sprotocol2.sadb_protocol_proto = SADB_X_FLOW_TYPE_USE;
	}
	if (keyp != NULL) {
		for (i = 0; i < klen; i++)
			realkey[i] = x2i(keyp + 2 * i);
	}
	if (authp != NULL) {
		for (i = 0; i < alen; i++)
			realakey[i] = x2i(authp + 2 * i);
	}
	/* message header */
	iov[cnt].iov_base = &smsg;
	iov[cnt++].iov_len = sizeof(smsg);

	if (isencauth(mode) || iscmd(mode, TCPMD5)) {	/* XXX */
		/* SA header */
		iov[cnt].iov_base = &sa;
		iov[cnt++].iov_len = sizeof(sa);
		smsg.sadb_msg_len += sa.sadb_sa_len;

		/* Destination address header */
		iov[cnt].iov_base = &sad2;
		iov[cnt++].iov_len = sizeof(sad2);
		/* Destination address */
		iov[cnt].iov_base = dst;
		iov[cnt++].iov_len = ROUNDUP(dst->sa.sa_len);
		smsg.sadb_msg_len += sad2.sadb_address_len;

		if (srcid) {
			iov[cnt].iov_base = &sid1;
			iov[cnt++].iov_len = sizeof(sid1);
			/* SRC identity */
			iov[cnt].iov_base = srcid;
			iov[cnt++].iov_len = ROUNDUP(strlen((char *)srcid) + 1);
			smsg.sadb_msg_len += sid1.sadb_ident_len;
		}
		if (dstid) {
			iov[cnt].iov_base = &sid2;
			iov[cnt++].iov_len = sizeof(sid2);
			/* DST identity */
			iov[cnt].iov_base = dstid;
			iov[cnt++].iov_len = ROUNDUP(strlen((char *)dstid) + 1);
			smsg.sadb_msg_len += sid2.sadb_ident_len;
		}
		if (sad1.sadb_address_exttype) {
			/* Source address header */
			iov[cnt].iov_base = &sad1;
			iov[cnt++].iov_len = sizeof(sad1);
			/* Source address */
			iov[cnt].iov_base = src;
			iov[cnt++].iov_len = ROUNDUP(src->sa.sa_len);
			smsg.sadb_msg_len += sad1.sadb_address_len;
		}
		if (proxy->sa.sa_len) {
			/* Proxy address header */
			iov[cnt].iov_base = &sad3;
			iov[cnt++].iov_len = sizeof(sad3);
			/* Proxy address */
			iov[cnt].iov_base = proxy;
			iov[cnt++].iov_len = ROUNDUP(proxy->sa.sa_len);
			smsg.sadb_msg_len += sad3.sadb_address_len;
		}
		if (keyp) {
			/* Key header */
			iov[cnt].iov_base = &skey1;
			iov[cnt++].iov_len = sizeof(skey1);
			/* Key */
			iov[cnt].iov_base = realkey;
			iov[cnt++].iov_len = ((klen + 7) / 8) * 8;
			skey1.sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
			skey1.sadb_key_len = (sizeof(skey1) + ((klen + 7) / 8) * 8) / 8;
			skey1.sadb_key_bits = 8 * klen;
			smsg.sadb_msg_len += skey1.sadb_key_len;
		}
		if (authp) {
			/* Auth key header */
			iov[cnt].iov_base = &skey2;
			iov[cnt++].iov_len = sizeof(skey2);
			/* Auth key */
			iov[cnt].iov_base = realakey;
			iov[cnt++].iov_len = ((alen + 7) / 8) * 8;
			skey2.sadb_key_exttype = SADB_EXT_KEY_AUTH;
			skey2.sadb_key_len = (sizeof(skey2) + ((alen + 7) / 8) * 8) / 8;
			skey2.sadb_key_bits = 8 * alen;
			smsg.sadb_msg_len += skey2.sadb_key_len;
		}
		if (sa.sadb_sa_flags & SADB_X_SAFLAGS_UDPENCAP) {
			iov[cnt].iov_base = &udpencap;
			iov[cnt++].iov_len = sizeof(udpencap);
			smsg.sadb_msg_len += udpencap.sadb_x_udpencap_len;
		}
	} else {
		switch (mode & CMD_MASK) {
		case GRP_SPI:
			/* SA header */
			iov[cnt].iov_base = &sa;
			iov[cnt++].iov_len = sizeof(sa);
			smsg.sadb_msg_len += sa.sadb_sa_len;

			/* Destination address header */
			iov[cnt].iov_base = &sad2;
			iov[cnt++].iov_len = sizeof(sad2);
			/* Destination address */
			iov[cnt].iov_base = dst;
			iov[cnt++].iov_len = ROUNDUP(dst->sa.sa_len);
			smsg.sadb_msg_len += sad2.sadb_address_len;

			/* SA header */
			iov[cnt].iov_base = &sa2;
			iov[cnt++].iov_len = sizeof(sa2);
			smsg.sadb_msg_len += sa2.sadb_sa_len;

			/* Destination2 address header */
			iov[cnt].iov_base = &sad8;
			iov[cnt++].iov_len = sizeof(sad8);
			/* Destination2 address */
			iov[cnt].iov_base = dst2;
			iov[cnt++].iov_len = ROUNDUP(dst2->sa.sa_len);
			smsg.sadb_msg_len += sad8.sadb_address_len;

			sprotocol.sadb_protocol_proto = sproto2;

			/* Protocol2 */
			iov[cnt].iov_base = &sprotocol;
			iov[cnt++].iov_len = sizeof(sprotocol);
			smsg.sadb_msg_len += sprotocol.sadb_protocol_len;
			break;

		case DEL_SPI:
			/* SA header */
			iov[cnt].iov_base = &sa;
			iov[cnt++].iov_len = sizeof(sa);
			smsg.sadb_msg_len += sa.sadb_sa_len;

			/* Destination address header */
			iov[cnt].iov_base = &sad2;
			iov[cnt++].iov_len = sizeof(sad2);
			/* Destination address */
			iov[cnt].iov_base = dst;
			iov[cnt++].iov_len = ROUNDUP(dst->sa.sa_len);
			smsg.sadb_msg_len += sad2.sadb_address_len;
			break;

		case ENC_IP:
			/* SA header */
			iov[cnt].iov_base = &sa;
			iov[cnt++].iov_len = sizeof(sa);
			smsg.sadb_msg_len += sa.sadb_sa_len;

			/* Destination address header */
			iov[cnt].iov_base = &sad2;
			iov[cnt++].iov_len = sizeof(sad2);
			/* Destination address */
			iov[cnt].iov_base = dst;
			iov[cnt++].iov_len = ROUNDUP(dst->sa.sa_len);
			smsg.sadb_msg_len += sad2.sadb_address_len;

			if (sad1.sadb_address_exttype) {
				/* Source address header */
				iov[cnt].iov_base = &sad1;
				iov[cnt++].iov_len = sizeof(sad1);
				/* Source address */
				iov[cnt].iov_base = src;
				iov[cnt++].iov_len = ROUNDUP(src->sa.sa_len);
				smsg.sadb_msg_len += sad1.sadb_address_len;
			}
			break;

		case IPCOMP:
			/* SA header */
			iov[cnt].iov_base = &sa;
			iov[cnt++].iov_len = sizeof(sa);
			smsg.sadb_msg_len += sa.sadb_sa_len;

			/* Destination address header */
			iov[cnt].iov_base = &sad2;
			iov[cnt++].iov_len = sizeof(sad2);
			/* Destination address */
			iov[cnt].iov_base = dst;
			iov[cnt++].iov_len = ROUNDUP(dst->sa.sa_len);
			smsg.sadb_msg_len += sad2.sadb_address_len;

			if (sad1.sadb_address_exttype) {
				/* Source address header */
				iov[cnt].iov_base = &sad1;
				iov[cnt++].iov_len = sizeof(sad1);
				/* Source address */
				iov[cnt].iov_base = src;
				iov[cnt++].iov_len = ROUNDUP(src->sa.sa_len);
				smsg.sadb_msg_len += sad1.sadb_address_len;
			}
			break;

		case FLOW:
			if ((smsg.sadb_msg_type != SADB_X_DELFLOW) &&
			    (sad2.sadb_address_exttype)) {
				/* Destination address header */
				iov[cnt].iov_base = &sad2;
				iov[cnt++].iov_len = sizeof(sad2);
				/* Destination address */
				iov[cnt].iov_base = dst;
				iov[cnt++].iov_len = ROUNDUP(dst->sa.sa_len);
				smsg.sadb_msg_len += sad2.sadb_address_len;
			}
			if ((sad1.sadb_address_exttype) &&
			    (smsg.sadb_msg_type != SADB_X_DELFLOW)) {
				/* Source address header */
				iov[cnt].iov_base = &sad1;
				iov[cnt++].iov_len = sizeof(sad1);
				/* Source address */
				iov[cnt].iov_base = src;
				iov[cnt++].iov_len = ROUNDUP(src->sa.sa_len);
				smsg.sadb_msg_len += sad1.sadb_address_len;
			}
			if (sprotocol.sadb_protocol_len) {
				/* Transport protocol */
				iov[cnt].iov_base = &sprotocol;
				iov[cnt++].iov_len = sizeof(sprotocol);
				smsg.sadb_msg_len += sprotocol.sadb_protocol_len;
			}
			/* Flow type */
			iov[cnt].iov_base = &sprotocol2;
			iov[cnt++].iov_len = sizeof(sprotocol2);
			smsg.sadb_msg_len += sprotocol2.sadb_protocol_len;

			/* Flow source address header */
			if ((sport != -1) && (sport != 0)) {
				if (osrc->sa.sa_family == AF_INET) {
					osrc->sin.sin_port = sport;
					osmask->sin.sin_port = 0xffff;
				}
				else if (osrc->sa.sa_family == AF_INET6) {
					osrc->sin6.sin6_port = sport;
					osmask->sin6.sin6_port = 0xffff;
				}
			}
			iov[cnt].iov_base = &sad4;
			iov[cnt++].iov_len = sizeof(sad4);
			/* Flow source address */
			iov[cnt].iov_base = osrc;
			iov[cnt++].iov_len = ROUNDUP(osrc->sa.sa_len);
			smsg.sadb_msg_len += sad4.sadb_address_len;

			/* Flow destination address header */
			iov[cnt].iov_base = &sad5;
			iov[cnt++].iov_len = sizeof(sad5);
			/* Flow destination address */
			if ((dport != -1) && (dport != 0)) {
				if (odst->sa.sa_family == AF_INET) {
					odst->sin.sin_port = dport;
					odmask->sin.sin_port = 0xffff;
				}
				else if (odst->sa.sa_family == AF_INET6) {
					odst->sin6.sin6_port = dport;
					odmask->sin6.sin6_port = 0xffff;
				}
			}
			iov[cnt].iov_base = odst;
			iov[cnt++].iov_len = ROUNDUP(odst->sa.sa_len);
			smsg.sadb_msg_len += sad5.sadb_address_len;

			/* Flow source address mask header */
			iov[cnt].iov_base = &sad6;
			iov[cnt++].iov_len = sizeof(sad6);
			/* Flow source address mask */
			iov[cnt].iov_base = osmask;
			iov[cnt++].iov_len = ROUNDUP(osmask->sa.sa_len);
			smsg.sadb_msg_len += sad6.sadb_address_len;

			/* Flow destination address mask header */
			iov[cnt].iov_base = &sad7;
			iov[cnt++].iov_len = sizeof(sad7);
			/* Flow destination address mask */
			iov[cnt].iov_base = odmask;
			iov[cnt++].iov_len = ROUNDUP(odmask->sa.sa_len);
			smsg.sadb_msg_len += sad7.sadb_address_len;

			if ((srcid) &&
			    (smsg.sadb_msg_type != SADB_X_DELFLOW)) {
				iov[cnt].iov_base = &sid1;
				iov[cnt++].iov_len = sizeof(sid1);
				/* SRC identity */
				iov[cnt].iov_base = srcid;
				iov[cnt++].iov_len = ROUNDUP(strlen((char *)srcid) + 1);
				smsg.sadb_msg_len += sid1.sadb_ident_len;
			}
			if ((dstid) &&
			    (smsg.sadb_msg_type != SADB_X_DELFLOW)) {
				iov[cnt].iov_base = &sid2;
				iov[cnt++].iov_len = sizeof(sid2);
				/* DST identity */
				iov[cnt].iov_base = dstid;
				iov[cnt++].iov_len = ROUNDUP(strlen((char *)dstid) + 1);
				smsg.sadb_msg_len += sid2.sadb_ident_len;
			}
			break;

		case FLUSH:
			/* No more work needed */
			break;

		}
	}

	xf_set(iov, cnt, smsg.sadb_msg_len * 8);
	exit(0);
}
