/* $OpenBSD: ipsecadm.c,v 1.16 1999/03/29 04:52:53 provos Exp $ */
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
 *	
 * Permission to use, copy, and modify this software without fee
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

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netns/ns.h>
#include <netiso/iso.h>
#include <netccitt/x25.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <sys/uio.h>
#include <net/pfkeyv2.h>
#include <netinet/ip_ipsp.h>

#define ESP_OLD		0x01
#define ESP_NEW		0x02
#define AH_OLD		0x04
#define AH_NEW		0x08

#define XF_ENC		0x10
#define XF_AUTH		0x20
#define DEL_SPI		0x30
#define GRP_SPI		0x40
#define FLOW		0x50
#define BINDSA		0x60
#define ENC_IP		0x80

#define CMD_MASK	0xf0

#define isencauth(x) ((x)&~CMD_MASK)
#define iscmd(x,y)   (((x) & CMD_MASK) == (y))

typedef struct {
    char *name;
    int   id, flags;
} transform;

transform xf[] = {
    {"des", SADB_EALG_DESCBC,   XF_ENC |ESP_OLD|ESP_NEW},
    {"3des", SADB_EALG_3DESCBC, XF_ENC |ESP_OLD|ESP_NEW},
    {"blf", SADB_EALG_X_BLF,   XF_ENC |        ESP_NEW},
    {"cast", SADB_EALG_X_CAST, XF_ENC |        ESP_NEW},
    {"skipjack", SADB_EALG_X_SKIPJACK, XF_ENC |        ESP_NEW},
    {"md5", SADB_AALG_MD5HMAC96,  XF_AUTH|AH_NEW|ESP_NEW},
    {"sha1", SADB_AALG_SHA1HMAC96,XF_AUTH|AH_NEW|ESP_NEW},
    {"md5", SADB_AALG_X_MD5,  XF_AUTH|AH_OLD},
    {"sha1", SADB_AALG_X_SHA1,XF_AUTH|AH_OLD},
    {"rmd160", SADB_AALG_X_RIPEMD160HMAC96, XF_AUTH|AH_NEW|ESP_NEW},
};

void
xf_set(struct iovec *iov, int cnt, int len)
{
    struct sadb_msg sm;
    int sd;
    
    sd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2);
    if (sd < 0) 
    {
	perror("socket");
	if (errno == EPROTONOSUPPORT)
	    fprintf(stderr,
		"Make sure your kernel is compiled with option KEY\n");
	exit(1);
    }

    if (writev(sd, iov, cnt) != len)
    {
	perror("write");
	exit(1);
    }

    if (read(sd, &sm, sizeof(sm)) != sizeof(sm))
    {
	perror("read");
	exit(1);
    }

    if (sm.sadb_msg_errno != 0)
    {
	/* XXX We need better error reporting than this */
	errno = sm.sadb_msg_errno;
	perror("pfkey");
	exit(1);
    }

    close(sd);
}

int
x2i(char *s)
{
    char    ss[3];
    ss[0] = s[0];
    ss[1] = s[1];
    ss[2] = 0;

    if (!isxdigit(s[0]) || !isxdigit(s[1]))
    {
	fprintf(stderr, "Keys should be specified in hex digits.\n");
	exit(-1);
    }

    return strtoul(ss, NULL, 16);
}

int
isvalid(char *option, int type, int mode)
{
    int i;

    for (i = sizeof(xf) / sizeof(transform) - 1; i >= 0; i--)
      if (!strcmp(option, xf[i].name) &&
	  (xf[i].flags & CMD_MASK) == type && 
	  (xf[i].flags & mode))
      {
	  if (!strcmp(option, "des") || !strcmp(option, "skipjack"))
	    fprintf(stderr, "Warning: use of %s is strongly discouraged due to cryptographic weaknesses\n", option);

          return xf[i].id;
      }

    return 0;
}

void
usage()
{
    fprintf(stderr, "usage: ipsecadm [command] <modifier...>\n"
	    "\tCommands: new esp, old esp, new ah, old ah, group, delspi, ip4\n"
	    "\t\t  flow, bind\n"
	    "\tPossible modifiers:\n"
	    "\t  -enc <alg>\t\t\t encryption algorithm\n"
	    "\t  -auth <alg>\t\t\t authentication algorithm\n"
	    "\t  -src <ip>\t\t\t source address to be used\n"
	    "\t  -halfiv\t\t\t use 4-byte IV in old ESP\n"
	    "\t  -forcetunnel\t\t\t force IP-in-IP encapsulation\n"
	    "\t  -dst <ip>\t\t\t destination address to be used\n"
	    "\t  -proxy <ip>\t\t\t proxy address to be used\n"
	    "\t  -spi <val>\t\t\t SPI to be used\n"
	    "\t  -key <val>\t\t\t key material to be used\n"
	    "\t  -authkey <val>\t\t key material for auth in new esp\n"
	    "\t  -proto <val>\t\t\t security protocol\n"
	    "\t  -chain\t\t\t SPI chain delete\n"
	    "\t  -transport <val>\t\t protocol number for flow\n"
	    "\t  -addr <ip> <net> <ip> <net>\t subnets for flow\n"
	    "\t  -delete\t\t\t delete specified flow\n"
	    "\t  -local\t\t\t also create a local flow\n"
	    "\talso: dst2, spi2, proto2\n"
	);
}

int
main(int argc, char **argv)
{
    int auth = 0, enc = 0, klen = 0, alen = 0, mode = ESP_NEW, i = 0;
    int proto = IPPROTO_ESP, proto2 = IPPROTO_AH;
    int dport = -1, sport = -1, tproto = -1;
    u_int32_t spi = 0, spi2 = 0;
    union sockaddr_union src, dst, dst2, osrc, odst, osmask, odmask, proxy;
    int srcset = 0, dstset = 0, dst2set = 0;
    u_char *keyp = NULL, *authp = NULL;
    struct protoent *tp;
    struct servent *svp;
    char *transportproto = NULL;
    struct sadb_msg smsg;
    struct sadb_sa sa;
    struct sadb_sa sa2;
    struct sadb_address sad1; /* src */
    struct sadb_address sad2; /* dst */
    struct sadb_address sad3; /* proxy */
    struct sadb_address sad4; /* osrc */
    struct sadb_address sad5; /* odst */
    struct sadb_address sad6; /* osmask */
    struct sadb_address sad7; /* odmask */
    struct sadb_address sad8; /* dst2 */
    struct sadb_key skey1;
    struct sadb_key skey2;
    struct sadb_protocol sprotocol;
    struct iovec iov[20];
    int cnt = 0;
    u_char realkey[8192], realakey[8192];
    
    if (argc < 2)
    {
	usage();
	exit(1);
    }

    /* Zero out */
    bzero(&smsg, sizeof(smsg));
    bzero(&sa, sizeof(sa));
    bzero(&sa2, sizeof(sa2));
    bzero(&skey1, sizeof(skey1));
    bzero(&skey2, sizeof(skey2));
    bzero(&sad1, sizeof(sad1));
    bzero(&sad2, sizeof(sad2));
    bzero(&sad3, sizeof(sad3));
    bzero(&sad4, sizeof(sad4));
    bzero(&sad5, sizeof(sad5));
    bzero(&sad6, sizeof(sad6));
    bzero(&sad7, sizeof(sad7));
    bzero(&sad8, sizeof(sad8));
    bzero(&sprotocol, sizeof(sprotocol));
    bzero(iov, sizeof(iov));
    bzero(realkey, sizeof(realkey));
    bzero(realakey, sizeof(realakey));
    
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

    /* Initialize */
    sa2.sadb_sa_exttype = SADB_EXT_X_SA2;
    sa2.sadb_sa_len = sizeof(sa2) / 8;
    sa2.sadb_sa_replay = 0;
    sa2.sadb_sa_state = SADB_SASTATE_MATURE;

    /* Initialize */
    bzero(&src, sizeof(union sockaddr_union));
    bzero(&dst, sizeof(union sockaddr_union));
    bzero(&dst2, sizeof(union sockaddr_union));
    bzero(&osrc, sizeof(union sockaddr_union));
    bzero(&odst, sizeof(union sockaddr_union));
    bzero(&osmask, sizeof(union sockaddr_union));
    bzero(&odmask, sizeof(union sockaddr_union));
    bzero(&proxy, sizeof(union sockaddr_union));

    if (!strcmp(argv[1], "new") && argc > 3)
    {
	if (!strcmp(argv[2], "esp"))
	{
	    mode = ESP_NEW;
	    smsg.sadb_msg_type = SADB_ADD;
	    smsg.sadb_msg_satype = SADB_SATYPE_ESP;
	}
	else
	  if (!strcmp(argv[2], "ah"))
	  {
	      mode = AH_NEW;
	      smsg.sadb_msg_type = SADB_ADD;
	      smsg.sadb_msg_satype = SADB_SATYPE_AH;
	  }
	  else
	  {
	      fprintf(stderr, "%s: unexpected identifier %s\n", argv[0],
		      argv[2]);
	      exit(1);
	  }
	
	i += 2;
    }
    else
      if (!strcmp(argv[1], "old") && argc > 3)
      {
	  if (!strcmp(argv[2], "esp"))
	  {
	      mode = ESP_OLD;
	      smsg.sadb_msg_type = SADB_ADD;
	      smsg.sadb_msg_satype = SADB_SATYPE_X_ESP_OLD;
	  }
	  else
	    if (!strcmp(argv[2], "ah"))
	    {
		mode = AH_OLD;
		smsg.sadb_msg_type = SADB_ADD;
		smsg.sadb_msg_satype = SADB_SATYPE_X_AH_OLD;
	    }
	    else
	    {
		fprintf(stderr, "%s: unexpected identifier %s\n", argv[0],
			argv[2]);
		exit(1);
	    }
	  
	  i += 2;
      }
      else
	if (!strcmp(argv[1], "delspi"))
	{
	    smsg.sadb_msg_type = SADB_DELETE;
	    smsg.sadb_msg_satype = SADB_SATYPE_ESP;
	    mode = DEL_SPI;
	    i++;
	}
	else
	  if (!strcmp(argv[1], "group"))
	  {
	      smsg.sadb_msg_type = SADB_X_GRPSPIS;
	      mode = GRP_SPI;
	      i++;
	  }
	  else
	    if (!strcmp(argv[1], "bind"))
	    {
		smsg.sadb_msg_type = SADB_X_BINDSA;
		smsg.sadb_msg_satype = SADB_SATYPE_ESP;
		mode = BINDSA;
		i++;
	    }
	    else
	      if (!strcmp(argv[1], "flow"))
	      {
		  /* It may not be ADDFLOW, but never mind that for now */
		  smsg.sadb_msg_type = SADB_X_ADDFLOW;
		  smsg.sadb_msg_satype = SADB_SATYPE_ESP;
		  mode = FLOW;
		  i++;
	      }
	      else
		if (!strcmp(argv[1], "ip4"))
		{
		    mode = ENC_IP;
		    smsg.sadb_msg_type = SADB_ADD;
		    smsg.sadb_msg_satype = SADB_SATYPE_X_IPIP;
		    i++;
		}
		else
		{
		    fprintf(stderr, "%s: unknown command: %s", argv[0], argv[1]);
		    exit(1);
		}
    
    for (i++; i < argc; i++)
    {
	if (argv[i][0] != '-')
	{
	    fprintf(stderr, "%s: expected option, got %s\n", 
		    argv[0], argv[i]);
	    exit(1);
	}

	if (!strcmp(argv[i] + 1, "enc") && enc == 0 && (i + 1 < argc))
	{
	    if ((enc = isvalid(argv[i + 1], XF_ENC, mode)) == 0)
	    {
		fprintf(stderr, "%s: invalid encryption algorithm %s\n",
			argv[0], argv[i + 1]);
		exit(1);
	    }
	
	    skey1.sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
	    sa.sadb_sa_encrypt = enc;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "auth") && auth == 0 && (i + 1 < argc))
	{
	    if ((auth = isvalid(argv[i + 1], XF_AUTH, mode)) == 0)
	    {
		fprintf(stderr, "%s: invalid auth algorithm %s\n",
			argv[0], argv[i + 1]);
		exit(1);
	    }

	    skey2.sadb_key_exttype = SADB_EXT_KEY_AUTH;
	    sa.sadb_sa_auth = auth;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "key") && keyp == NULL &&
	    (i + 1 < argc))
	{
	    if (mode & (AH_NEW | AH_OLD))
	    {
		authp = argv[++i];
		alen = strlen(authp) / 2;
	    }
	    else
	    {
		keyp = argv[++i];
		klen = strlen(keyp) / 2;
	    }
	    continue;
	}

	if (!strcmp(argv[i] + 1, "authkey") && authp == NULL &&
	    (i + 1 < argc))
	{
	    if (!(mode & ESP_NEW))
	    {
		fprintf(stderr,	"%s: invalid option %s for selected mode\n",
			argv[0], argv[i]);
		exit(1);
	    }

	    authp = argv[++i];
	    alen = strlen(authp) / 2;
	    continue;
	}
	
	if (!strcmp(argv[i] + 1, "iv") && (i + 1 < argc))
	{
	    if (mode & (AH_OLD | AH_NEW))
	    {
		fprintf(stderr, "%s: invalid option %s with auth\n",
			argv[0], argv[i]);
		exit(1);
	    }

	    fprintf(stderr,
		    "%s: Warning: option iv has been deprecated\n", argv[0]);

	    if (mode & ESP_OLD)
	      if (strlen(argv[i + 2]) == 4)
		sa.sadb_sa_flags |= SADB_SAFLAGS_X_HALFIV;

	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "spi") && spi == 0 && (i + 1 < argc))
	{
	    if ((spi = htonl(strtoul(argv[i + 1], NULL, 16))) == 0)
	    {
		fprintf(stderr, "%s: invalid spi %s\n", argv[0], argv[i + 1]);
		exit(1);
	    }

	    sa.sadb_sa_spi = spi;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "spi2") && spi2 == 0 && 
	    (iscmd(mode, GRP_SPI) || iscmd(mode, BINDSA)) && (i + 1 < argc))
	{
	    if ((spi2 = htonl(strtoul(argv[i + 1], NULL, 16))) == 0)
	    {
		fprintf(stderr, "%s: invalid spi2 %s\n", argv[0], argv[i + 1]);
		exit(1);
	    }

	    sa2.sadb_sa_spi = spi2;
	    i++;
	    continue;
	} 

	if (!strcmp(argv[i] + 1, "src") && (i + 1 < argc))
	{
	    src.sin.sin_family = AF_INET;
	    src.sin.sin_len = sizeof(struct sockaddr_in);
	    srcset = inet_aton(argv[i + 1], &src.sin.sin_addr) != -1 ? 1 : 0;
	    sad1.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
	    sad1.sadb_address_len = 1 + sizeof(struct sockaddr_in) / 8;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "proxy") && (i + 1 < argc))
	{
	    proxy.sin.sin_family = AF_INET;
	    proxy.sin.sin_len = sizeof(struct sockaddr_in);
	    proxy.sin.sin_addr.s_addr = inet_addr(argv[i + 1]);
	    sad3.sadb_address_exttype = SADB_EXT_ADDRESS_PROXY;
	    sad3.sadb_address_len = 1 + sizeof(struct sockaddr_in) / 8;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "newpadding"))
	{
	    fprintf(stderr,
		    "%s: Warning: option newpadding has been deprecated\n",
		    argv[0]);
	    continue;
	}

	if (!strcmp(argv[i] + 1, "forcetunnel") && isencauth(mode))
	{
	    sa.sadb_sa_flags |= SADB_SAFLAGS_X_TUNNEL;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "halfiv"))
	{
	    if (!(mode & ESP_OLD))
	    {
		fprintf(stderr,
			"%s: option halfiv can be used only with old ESP\n",
			argv[0]);
		exit(1);
	    }

	    sa.sadb_sa_flags |= SADB_SAFLAGS_X_HALFIV;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "delete") && iscmd(mode, FLOW))
	{
	    smsg.sadb_msg_type = SADB_X_DELFLOW;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "local") && iscmd(mode, FLOW))
	{
	    sa.sadb_sa_flags |= SADB_SAFLAGS_X_LOCALFLOW;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "tunnel") &&	
	    (isencauth(mode) || mode == ENC_IP) && ( i + 2 < argc))
	{
	    i += 2;
	    sa.sadb_sa_flags |= SADB_SAFLAGS_X_TUNNEL;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "addr") && iscmd(mode, FLOW) &&
	    (i + 4 < argc))
	{
	    sad4.sadb_address_exttype = SADB_EXT_X_SRC_FLOW;
	    sad5.sadb_address_exttype = SADB_EXT_X_DST_FLOW;
	    sad6.sadb_address_exttype = SADB_EXT_X_SRC_MASK;
	    sad7.sadb_address_exttype = SADB_EXT_X_DST_MASK;

	    sad4.sadb_address_len = (sizeof(sad4) +
				     sizeof(struct sockaddr_in)) / 8;
	    sad5.sadb_address_len = (sizeof(sad5) +
				     sizeof(struct sockaddr_in)) / 8;
	    sad6.sadb_address_len = (sizeof(sad6) +
				     sizeof(struct sockaddr_in)) / 8;
	    sad7.sadb_address_len = (sizeof(sad7) +
				     sizeof(struct sockaddr_in)) / 8;

	    osrc.sin.sin_family = odst.sin.sin_family = AF_INET;
	    osmask.sin.sin_family = odmask.sin.sin_family = AF_INET;
	    osrc.sin.sin_len = odst.sin.sin_len = sizeof(struct sockaddr_in);
	    osmask.sin.sin_len = sizeof(struct sockaddr_in);
	    odmask.sin.sin_len = sizeof(struct sockaddr_in);
	    
	    osrc.sin.sin_addr.s_addr = inet_addr(argv[i + 1]); i++;
	    osmask.sin.sin_addr.s_addr = inet_addr(argv[i + 1]); i++;
	    odst.sin.sin_addr.s_addr = inet_addr(argv[i + 1]); i++;
	    odmask.sin.sin_addr.s_addr = inet_addr(argv[i + 1]); i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "transport") && 
	    iscmd(mode, FLOW) && (i + 1 < argc))
	{
	    if (isalpha(argv[i + 1][0]))
	    {
		tp = getprotobyname(argv[i + 1]);
		if (tp == NULL)
		{
		    fprintf(stderr,
			    "%s: unknown protocol %s\n", argv[0], argv[i + 1]);
		    exit(1);
		}

		tproto = tp->p_proto;
		transportproto = argv[i + 1];
	    }
	    else
	    {
		tproto = atoi(argv[i + 1]);
		tp = getprotobynumber(tproto);
		if (tp == NULL)
		  transportproto = "UNKNOWN";
		else
		  transportproto = tp->p_name;
	    }

	    sprotocol.sadb_protocol_len = 1;
	    sprotocol.sadb_protocol_exttype = SADB_EXT_X_PROTOCOL;
	    sprotocol.sadb_protocol_proto = tproto;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "sport") && 
	    iscmd(mode, FLOW) && (i + 1 < argc))
	{
	    if (isalpha(argv[i + 1][0]))
	    {
		svp = getservbyname(argv[i + 1], transportproto);
		if (svp == NULL)
		{
		    fprintf(stderr,
			    "%s: unknown service port %s for protocol %s\n",
			    argv[0], argv[i + 1], transportproto);
		    exit(1);
		}

		sport = svp->s_port;
	    }
	    else
	      sport = atoi(argv[i+1]);

	    osrc.sin.sin_port = sport;
	    osmask.sin.sin_port = 0xffff;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "dport") && 
	    iscmd(mode, FLOW) && (i + 1 < argc))
	{
	    if (isalpha(argv[i + 1][0]))
	    {
		svp = getservbyname(argv[i + 1], transportproto);
		if (svp == NULL)
		{
		    fprintf(stderr,
			    "%s: unknown service port %s for protocol %s\n",
			    argv[0], argv[i + 1], transportproto);
		    exit(1);
		}
		dport = svp->s_port;
	    }
	    else
	      dport = atoi(argv[i + 1]);

	    odst.sin.sin_port = dport;
	    odmask.sin.sin_port = 0xffff;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "dst") && (i + 1 < argc))
	{
	    sad2.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
	    sad2.sadb_address_len = (sizeof(sad2) +
				     sizeof(struct sockaddr_in)) / 8;
	    dst.sin.sin_family = AF_INET;
	    dst.sin.sin_len = sizeof(struct sockaddr_in);
	    dstset = inet_aton(argv[i + 1], &dst.sin.sin_addr) != -1 ? 1 : 0;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "dst2") && 
	    (iscmd(mode, GRP_SPI) || iscmd(mode, BINDSA)) && (i + 1 < argc))
	{
	    sad8.sadb_address_len = (sizeof(sad8) +
				     sizeof(struct sockaddr_in)) / 8;
	    sad8.sadb_address_exttype = SADB_EXT_X_DST2;
	    dst2.sin.sin_family = AF_INET;
	    dst2.sin.sin_len = sizeof(struct sockaddr_in);
	    dst2set = inet_aton(argv[i + 1], &dst2.sin.sin_addr) != -1 ? 1 : 0;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "proto") && (i + 1 < argc) &&
	    (iscmd(mode, FLOW) || iscmd(mode, GRP_SPI) ||
	     iscmd(mode, DEL_SPI) || iscmd(mode, BINDSA)))
	{
	    if (isalpha(argv[i + 1][0]))
	    {
		if (!strcasecmp(argv[i + 1], "esp"))
		{
		    smsg.sadb_msg_satype = SADB_SATYPE_ESP;
		    proto = IPPROTO_ESP;
		}
		else
		  if (!strcasecmp(argv[i + 1], "ah"))
		  {
		      smsg.sadb_msg_satype = SADB_SATYPE_AH;
		      proto = IPPROTO_AH;
		  }
		  else
		    if (!strcasecmp(argv[i + 1], "ip4"))
		    {
			smsg.sadb_msg_satype = SADB_SATYPE_X_IPIP;
			proto = IPPROTO_IPIP;
		    }
		    else
		    {
			fprintf(stderr,
				"%s: unknown security protocol type %s\n",
				argv[0], argv[i + 1]);
				exit(1);
		    }
	    }
	    else
	    {
		proto = atoi(argv[i + 1]);
		if (proto != IPPROTO_ESP && proto != IPPROTO_AH &&
		    proto != IPPROTO_IPIP)
		{
		    fprintf(stderr,
			    "%s: unknown security protocol %d\n",
			    argv[0], proto);
		    exit(1);
		}

		if (proto == IPPROTO_ESP)
		  smsg.sadb_msg_satype = SADB_SATYPE_ESP;
		else
		  if (proto == IPPROTO_AH)
		    smsg.sadb_msg_satype = SADB_SATYPE_AH;
		  else
		    if (proto == IPPROTO_IPIP)
		      smsg.sadb_msg_satype = SADB_SATYPE_X_IPIP;
	    }
	    
	    i++;
	    continue;
	}
	
	if (!strcmp(argv[i] + 1, "proto2") && 
	    iscmd(mode, GRP_SPI) && (i + 1 < argc))
	{
	    if (isalpha(argv[i + 1][0]))
	    {
		if (!strcasecmp(argv[i + 1], "esp"))
		{
		    sprotocol.sadb_protocol_proto = SADB_SATYPE_ESP;
		    proto2 = IPPROTO_ESP;
		}
		else
		  if (!strcasecmp(argv[i + 1], "ah"))
		  {
		      sprotocol.sadb_protocol_proto = SADB_SATYPE_AH;
		      proto2 = IPPROTO_AH;
		  }
		  else
		    if (!strcasecmp(argv[i + 1], "ip4"))
		    {
			sprotocol.sadb_protocol_proto = SADB_SATYPE_X_IPIP;
			proto2 = IPPROTO_IPIP;
		    }
		    else
		    {
			fprintf(stderr,
				"%s: unknown security protocol2 type %s\n",
				argv[0], argv[i+1]);
			exit(1);
		    }
	    }
	    else
	    {
		proto2 = atoi(argv[i + 1]);

		if (proto2 != IPPROTO_ESP && proto2 != IPPROTO_AH &&
		    proto2 != IPPROTO_IPIP)
		{
		    fprintf(stderr,
			    "%s: unknown security protocol2 %d\n",
			    argv[0], proto2);
		    exit(1);
		}

		if (proto2 == IPPROTO_ESP)
		  sprotocol.sadb_protocol_proto = SADB_SATYPE_ESP;
		else
		  if (proto2 == IPPROTO_AH)
		    sprotocol.sadb_protocol_proto = SADB_SATYPE_AH;
		  else
		    if (proto2 == IPPROTO_IPIP)
		      sprotocol.sadb_protocol_proto = SADB_SATYPE_X_IPIP;
	    }

	    sprotocol.sadb_protocol_exttype = SADB_EXT_X_PROTOCOL;
	    sprotocol.sadb_protocol_len = 1;
	    i++;
	    continue;
	}

	if (!strcmp(argv[i] + 1, "chain") &&
	    !(sa.sadb_sa_flags & SADB_SAFLAGS_X_CHAINDEL) &&
	    iscmd(mode, DEL_SPI))
	{
	    sa.sadb_sa_flags |= SADB_SAFLAGS_X_CHAINDEL;
	    continue;
	}

	/* No match */
	fprintf(stderr, "%s: Unknown or invalid option: %s\n",
		argv[0], argv[i]);
	exit(1);
    }
    
    /* Sanity checks */
    if ((mode & (ESP_NEW | ESP_OLD)) && enc == 0)
    {
	fprintf(stderr, "%s: no encryption algorithm specified\n",  argv[0]);
	exit(1);
    }

    if ((mode & (AH_NEW | AH_OLD)) && auth == 0)
    {
	fprintf(stderr, "%s: no authentication algorithm specified\n", 
		argv[0]);
	exit(1);
    }

    if (((mode & (ESP_NEW | ESP_OLD)) && keyp == NULL) ||
        ((mode & (AH_NEW | AH_OLD)) && authp == NULL))
    {
	fprintf(stderr, "%s: no key material specified\n", argv[0]);
	exit(1);
    }

    if ((mode & ESP_NEW) && auth && authp == NULL)
    {
	fprintf(stderr, "%s: no auth key material specified\n", argv[0]);
	exit(1);
    }

    if (spi == 0)
    {
	fprintf(stderr, "%s: no SPI specified\n", argv[0]);
	exit(1);
    }

    if ((iscmd(mode, GRP_SPI) || iscmd(mode, BINDSA)) && spi2 == 0)
    {
	fprintf(stderr, "%s: no SPI2 specified\n", argv[0]);
	exit(1);
    }

    if ((isencauth(mode) || iscmd(mode, ENC_IP)) && !srcset)
    {
	fprintf(stderr, "%s: no source address specified\n", argv[0]);
	exit(1);
    } 

    if ((iscmd(mode, DEL_SPI) || iscmd(mode, GRP_SPI) || iscmd(mode, FLOW) ||
	 iscmd(mode, BINDSA)) && proto != IPPROTO_ESP &&
	proto != IPPROTO_AH && proto != IPPROTO_IPIP)
    {
	fprintf(stderr, "%s: security protocol is none of AH, ESP or IPIP\n",
		argv[0]);
	exit(1);
    }

    if ((iscmd(mode, GRP_SPI) || iscmd(mode, BINDSA)) &&
	proto2 != IPPROTO_ESP && proto2 != IPPROTO_AH &&
	proto2 != IPPROTO_IPIP)
    {
	fprintf(stderr, "%s: security protocol2 is none of AH, ESP or IPIP\n",
		argv[0]);
	exit(1);
    }

    if (!dstset)
    {
	fprintf(stderr, "%s: no destination address for the SA specified\n", 
		argv[0]);
	exit(1);
    } 

    if (iscmd(mode, FLOW) && (odst.sin.sin_addr.s_addr == 0 &&
			      odmask.sin.sin_addr.s_addr == 0 && 
			      osrc.sin.sin_addr.s_addr == 0 &&
			      osmask.sin.sin_addr.s_addr == 0))
    {
	fprintf(stderr,	"%s: no subnets for flow specified\n", argv[0]);
	exit(1);
    }

    if (iscmd(mode, FLOW) && (sprotocol.sadb_protocol_proto == 0) &&
	(odst.sin.sin_port || osrc.sin.sin_port))
    {
	fprintf(stderr, "%s: no transport protocol supplied with source/destination ports\n", argv[0]);
	exit(1);
    }
    
    if ((iscmd(mode, GRP_SPI) || iscmd(mode, BINDSA)) && !dst2set)
    {
	fprintf(stderr, "%s: no destination address2 specified\n", argv[0]);
	exit(1);
    }

    if ((klen > 2 * 8100) || (alen > 2 * 8100))
    {
	fprintf(stderr, "%s: key too long\n", argv[0]);
	exit(1);
    }

    if (keyp != NULL)
    {
	for (i = 0; i < klen; i++)
	  realkey[i] = x2i(keyp + 2 * i);
    }
    
    if (authp != NULL)
    {
	for (i = 0; i < alen; i++)
	  realakey[i] = x2i(authp + 2 * i);
    }
    
    /* message header */
    iov[cnt].iov_base = &smsg;
    iov[cnt++].iov_len = sizeof(smsg);

    if (isencauth(mode))
    {
	/* SA header */
	iov[cnt].iov_base = &sa;
	iov[cnt++].iov_len = sizeof(sa);
	smsg.sadb_msg_len += sa.sadb_sa_len;

	/* Destination address header */
	iov[cnt].iov_base = &sad2;
	iov[cnt++].iov_len = sizeof(sad2);
	/* Destination address */
	iov[cnt].iov_base = &dst;
	iov[cnt++].iov_len = sizeof(struct sockaddr_in);
	smsg.sadb_msg_len += sad2.sadb_address_len;

	if (src.sin.sin_addr.s_addr)
	{
	    /* Source address header */
	    iov[cnt].iov_base = &sad1;
	    iov[cnt++].iov_len = sizeof(sad1);
	    /* Source address */
	    iov[cnt].iov_base = &src;
	    iov[cnt++].iov_len = sizeof(struct sockaddr_in);
	    smsg.sadb_msg_len += sad1.sadb_address_len;
	}

	if (proxy.sin.sin_addr.s_addr)
	{
	    /* Proxy address header */
	    iov[cnt].iov_base = &sad3;
	    iov[cnt++].iov_len = sizeof(sad3);
	    /* Proxy address */
	    iov[cnt].iov_base = &proxy;
	    iov[cnt++].iov_len = sizeof(struct sockaddr_in);
	    smsg.sadb_msg_len += sad3.sadb_address_len;
	}

	if (keyp)
	{
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

	if (authp)
	{
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
    }
    else
    {
	switch(mode & CMD_MASK)
	{
	    case GRP_SPI:
	    case BINDSA:
		/* SA header */
		iov[cnt].iov_base = &sa;
		iov[cnt++].iov_len = sizeof(sa);
		smsg.sadb_msg_len += sa.sadb_sa_len;

		/* Destination address header */
		iov[cnt].iov_base = &sad2;
		iov[cnt++].iov_len = sizeof(sad2);
		/* Destination address */
		iov[cnt].iov_base = &dst;
		iov[cnt++].iov_len = sizeof(struct sockaddr_in);
		smsg.sadb_msg_len += sad2.sadb_address_len;

		/* SA header */
		iov[cnt].iov_base = &sa2;
		iov[cnt++].iov_len = sizeof(sa2);
		smsg.sadb_msg_len += sa2.sadb_sa_len;

		/* Destination2 address header */
		iov[cnt].iov_base = &sad8;
		iov[cnt++].iov_len = sizeof(sad8);
		/* Destination2 address */
		iov[cnt].iov_base = &dst2;
		iov[cnt++].iov_len = sizeof(struct sockaddr_in);
		smsg.sadb_msg_len += sad8.sadb_address_len;

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
		iov[cnt].iov_base = &dst;
		iov[cnt++].iov_len = sizeof(struct sockaddr_in);
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
		iov[cnt].iov_base = &dst;
		iov[cnt++].iov_len = sizeof(struct sockaddr_in);
		smsg.sadb_msg_len += sad2.sadb_address_len;

		if (src.sin.sin_addr.s_addr)
		{
		    /* Source address header */
		    iov[cnt].iov_base = &sad1;
		    iov[cnt++].iov_len = sizeof(sad1);
		    /* Source address */
		    iov[cnt].iov_base = &src;
		    iov[cnt++].iov_len = sizeof(struct sockaddr_in);
		    smsg.sadb_msg_len += sad1.sadb_address_len;
		}
		break;

	     case FLOW:
		 if (smsg.sadb_msg_type != SADB_X_DELFLOW)
		 {
		     /* Destination address header */
		     iov[cnt].iov_base = &sad2;
		     iov[cnt++].iov_len = sizeof(sad2);
		     /* Destination address */
		     iov[cnt].iov_base = &dst;
		     iov[cnt++].iov_len = sizeof(struct sockaddr_in);
		     smsg.sadb_msg_len += sad2.sadb_address_len;
		 }
		 
		 /* SA header */
		 iov[cnt].iov_base = &sa;
		 iov[cnt++].iov_len = sizeof(sa);
		 smsg.sadb_msg_len += sa.sadb_sa_len;

		 if (sprotocol.sadb_protocol_len)
		 {
		     /* Protocol2 */
		     iov[cnt].iov_base = &sprotocol;
		     iov[cnt++].iov_len = sizeof(sprotocol);
		     smsg.sadb_msg_len += sprotocol.sadb_protocol_len;
		 }
		 
		 /* Flow source address header */
		 iov[cnt].iov_base = &sad4;
		 iov[cnt++].iov_len = sizeof(sad4);
		 /* Flow source addressaddress */
		 iov[cnt].iov_base = &osrc;
		 iov[cnt++].iov_len = sizeof(struct sockaddr_in);
		 smsg.sadb_msg_len += sad4.sadb_address_len;

		 /* Flow destination address header */
		 iov[cnt].iov_base = &sad5;
		 iov[cnt++].iov_len = sizeof(sad5);
		 /* Flow destination address */
		 iov[cnt].iov_base = &odst;
		 iov[cnt++].iov_len = sizeof(struct sockaddr_in);
		 smsg.sadb_msg_len += sad5.sadb_address_len;

		 /* Flow source address mask header */
		 iov[cnt].iov_base = &sad6;
		 iov[cnt++].iov_len = sizeof(sad6);
		 /* Flow source address mask */
		 iov[cnt].iov_base = &osmask;
		 iov[cnt++].iov_len = sizeof(struct sockaddr_in);
		 smsg.sadb_msg_len += sad6.sadb_address_len;

		 /* Flow destination address mask header */
		 iov[cnt].iov_base = &sad7;
		 iov[cnt++].iov_len = sizeof(sad7);
		 /* Flow destination address mask */
		 iov[cnt].iov_base = &odmask;
		 iov[cnt++].iov_len = sizeof(struct sockaddr_in);
		 smsg.sadb_msg_len += sad7.sadb_address_len;
		 break;
	}
    }

    xf_set(iov, cnt, smsg.sadb_msg_len * 8);
    exit (0);
}

