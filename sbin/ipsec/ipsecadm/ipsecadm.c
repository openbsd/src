/* $OpenBSD: ipsecadm.c,v 1.16 1998/06/08 17:42:33 provos Exp $ */
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
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
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
#include "net/encap.h"
#include "netinet/ip_ipsp.h"
#include "netinet/ip_esp.h"

#define ESP_OLD		0x01
#define ESP_NEW		0x02
#define AH_OLD		0x04
#define AH_NEW		0x08

#define XF_ENC		0x10
#define XF_AUTH		0x20
#define DEL_SPI		0x30
#define GRP_SPI		0x40
#define FLOW		0x50
#define ENC_IP		0x80

#define CMD_MASK	0xf0

#define isencauth(x) ((x)&~CMD_MASK)
#define iscmd(x,y)   (((x) & CMD_MASK) == (y))

typedef struct {
	char *name;
	int   id, flags;
}       transform;

int xf_esp_new __P((struct in_addr, struct in_addr, u_int32_t, int, int, 
    u_char *, u_char *, u_char *, struct in_addr, struct in_addr, int));
int xf_esp_old __P((struct in_addr, struct in_addr, u_int32_t, int, u_char *,
    u_char *, struct in_addr, struct in_addr)); 
int xf_ah_new __P((struct in_addr, struct in_addr, u_int32_t, int, u_char *,
    struct in_addr, struct in_addr));
int xf_ah_old __P((struct in_addr, struct in_addr, u_int32_t, int, u_char *,
    struct in_addr, struct in_addr));

int xf_delspi __P((struct in_addr, u_int32_t, int, int));
int xf_grp __P((struct in_addr, u_int32_t, int, struct in_addr, u_int32_t, int));
int xf_flow __P((struct in_addr, u_int32_t, int, struct in_addr, 
    struct in_addr, struct in_addr, struct in_addr, int, int, int, int, int));
int xf_ip4 __P((struct in_addr, struct in_addr, u_int32_t, 
    struct in_addr, struct in_addr));

transform xf[] = {
	{"des", ALG_ENC_DES,   XF_ENC |ESP_OLD|ESP_NEW},
	{"3des", ALG_ENC_3DES, XF_ENC |ESP_OLD|ESP_NEW},
	{"blf", ALG_ENC_BLF,   XF_ENC |        ESP_NEW},
	{"cast", ALG_ENC_CAST, XF_ENC |        ESP_NEW},
	{"md5", ALG_AUTH_MD5,  XF_AUTH|AH_OLD|AH_NEW|ESP_NEW},
	{"sha1", ALG_AUTH_SHA1,XF_AUTH|AH_OLD|AH_NEW|ESP_NEW},
	{"rmd160", ALG_AUTH_RMD160, XF_AUTH|AH_NEW|ESP_NEW},
};

char    buf[1024];

int
x2i(char *s)
{
	char    ss[3];
	ss[0] = s[0];
	ss[1] = s[1];
	ss[2] = 0;

	return strtoul(ss, NULL, 16);
}

int
isvalid(char *option, int type, int mode)
{
     int i;

     for (i = sizeof(xf) / sizeof(transform) - 1; i >= 0; i--)
	  if (!strcmp(option, xf[i].name)) {
	       if ((xf[i].flags & CMD_MASK) == type && 
		   (xf[i].flags & mode))
		    return xf[i].id;
	       else
		    return 0;
	  }
     return 0;
}

void
usage()
{
     fprintf( stderr, "usage: ipsecadm [command] <modifier...>\n"
	      "\tCommands: new esp, old esp, new ah, old ah, group, delspi, ip4, flow\n"
	      "\tPossible modifiers:\n"
	      "\t\t-enc <alg>\t encryption algorithm\n"
	      "\t\t-auth <alg>\t authentication algorithm\n"
	      "\t\t-src <ip>\t source address to be used\n"
              "\t\t-tunnel <ip> <ip> tunneling addresses\n"
	      "\t\t-dst <ip>\t destination address to be used\n"
	      "\t\t-spi <val>\t SPI to be used\n"
	      "\t\t-key <val>\t key material to be used\n"
	      "\t\t-authkey <val>\t key material for auth in new esp\n"
	      "\t\t-iv <val>\t iv to be used\n"
	      "\t\t-proto <val>\t security protocol\n"
	      "\t\t-chain\t\t SPI chain delete\n"
	      "\t\t-newpadding\t new style padding for new ESP\n"
	      "\t\t-transport <val>\t protocol number for flow\n"
	      "\t\t-addr <ip> <net> <ip> <net>\t subnets for flow\n"
	      "\t\t-delete\t\t delete specified flow\n"
	      "\t\t-local\t\t also create a local flow\n"
	      "\talso: dst2, spi2, proto2\n"
	  );
}

int
main(argc, argv)
	int     argc;
	char  **argv;
{
	int i;
	int mode = ESP_NEW, new = 1, flag = 0, newpadding = 0;
	int auth = 0, enc = 0, ivlen = 0, klen = 0, alen = 0;
	int proto = IPPROTO_ESP, proto2 = IPPROTO_AH;
	int dport = -1, sport = -1, tproto = -1;
	int delete = 0, local = 0, result;
	int chain = 0; 
	u_int32_t spi = 0, spi2 = 0;
	struct in_addr src, dst, dst2, osrc, odst, osmask, odmask;
	u_char *ivp = NULL, *keyp = NULL, *authp = NULL;

	osrc.s_addr = odst.s_addr = src.s_addr = dst.s_addr = dst2.s_addr = 0;
	osmask.s_addr = odmask.s_addr = 0;

	if (argc < 2) {
		usage();
		exit(1);
	}

	for (i=1; i < argc; i++) {
	     if (!strcmp(argv[i], "new") && !flag) {
		  flag = 1;
		  new = 1;
	     } else if (!strcmp(argv[i], "old") && !flag) {
		  flag = 1;
		  new = 0;
	     } else if (!strcmp(argv[i], "esp") && flag < 2) {
		  flag = 2;
		  mode = new ? ESP_NEW : ESP_OLD;
	     } else if (!strcmp(argv[i], "ah") && flag < 2) {
		  flag = 2;
		  mode = new ? AH_NEW : AH_OLD;
	     } else if (!strcmp(argv[i], "delspi") && flag < 2) {
		  flag = 2;
		  mode = DEL_SPI;
	     } else if (!strcmp(argv[i], "group") && flag < 2) {
		  flag = 2;
		  mode = GRP_SPI;
	     } else if (!strcmp(argv[i], "flow") && flag < 2) {
		  flag = 2;
		  mode = FLOW;
	     } else if (!strcmp(argv[i], "ip4") && flag < 2) {
		  flag = 2;
		  mode = ENC_IP;
	     } else if (argv[i][0] == '-') {
		  break;
	     } else {
		  fprintf(stderr, "%s: Unknown command: %s", argv[0], argv[i]);
		  exit(1);
	     }
	}

	for (; i < argc; i++) {
	     if (argv[i][0] != '-') {
		  fprintf(stderr, "%s: Expected option, got %s\n", 
			  argv[0], argv[i]);
		  exit(1);
	     } else if (!strcmp(argv[i]+1, "enc") && enc == 0 && i+1 < argc) {
		  if ((enc = isvalid(argv[i+1], XF_ENC, mode)) == 0) {
		       fprintf(stderr, "%s: Invalid encryption algorithm %s\n",
			       argv[0], argv[i+1]);
		       exit(1);
		  }
		  i++;
	     } else if (!strcmp(argv[i]+1, "auth") && auth == 0 && i+1 < argc) {
		  if ((auth = isvalid(argv[i+1], XF_AUTH, mode)) == 0) {
		       fprintf(stderr, "%s: Invalid auth algorithm %s\n",
			       argv[0], argv[i+1]);
		       exit(1);
		  }
		  i++;
	     } else if (!strcmp(argv[i]+1, "key") && keyp == NULL && i+1 < argc) {
		  keyp = argv[++i];
		  klen = strlen(keyp);
	     } else if (!strcmp(argv[i]+1, "authkey") && authp == NULL && i+1 < argc) {
		  if (!(mode & ESP_NEW)) {
		       fprintf(stderr, "%s: Invalid option %s for selected mode\n",
			       argv[0], argv[i]);
		       exit(1);
		  }
		  authp = argv[++i];
		  alen = strlen(authp);
	     } else if (!strcmp(argv[i]+1, "iv") && ivp == NULL && i+1 < argc) {
		  if (mode & (AH_OLD|AH_NEW)) {
		       fprintf(stderr, "%s: Invalid option %s with auth\n",
			       argv[0], argv[i]);
		       exit(1);
		  }
		  ivp = argv[++i];
		  ivlen = strlen(ivp);
	     } else if (!strcmp(argv[i]+1, "spi") && spi == 0 && i+1 < argc) {
		  if ((spi = htonl(strtoul(argv[i+1], NULL, 16))) == 0) {
		       fprintf(stderr, "%s: Invalid spi %s\n", 
			       argv[0], argv[i+1]);
		       exit(1);
		  }
		  i++;
	     } else if (!strcmp(argv[i]+1, "spi2") && spi2 == 0 && 
			iscmd(mode, GRP_SPI) && i+1 < argc) {
		  if ((spi2 = htonl(strtoul(argv[i+1], NULL, 16))) == 0) {
		       fprintf(stderr, "%s: Invalid spi2 %s\n", 
			       argv[0], argv[i+1]);
		       exit(1);
		  }
		  i++;
	     } else if (!strcmp(argv[i]+1, "src") && i+1 < argc) {
		  src.s_addr = inet_addr(argv[i+1]);
		  i++;
	     } else if (!strcmp(argv[i]+1, "newpadding") && (mode & ESP_NEW)) {
		  newpadding = 1;
	     } else if (!strcmp(argv[i]+1, "delete") && iscmd(mode, FLOW)) {
		  delete = 1;
	     } else if (!strcmp(argv[i]+1, "local") && iscmd(mode, FLOW)) {
		  local = 1;
	     } else if (!strcmp(argv[i]+1, "tunnel") &&
			isencauth(mode) && i+2 < argc) {
		  osrc.s_addr = inet_addr(argv[i+1]);
		  i++;
		  odst.s_addr = inet_addr(argv[i+1]);
		  i++;
	     } else if (!strcmp(argv[i]+1, "addr") &&
			iscmd(mode, FLOW) && i+4 < argc) {
		  osrc.s_addr = inet_addr(argv[i+1]); i++;
		  osmask.s_addr = inet_addr(argv[i+1]); i++;
		  odst.s_addr = inet_addr(argv[i+1]); i++;
		  odmask.s_addr = inet_addr(argv[i+1]); i++;
	     } else if (!strcmp(argv[i]+1, "transport") && 
			iscmd(mode, FLOW) && i+1 < argc) {
		  tproto = atoi(argv[i+1]);
		  i++;
	     } else if (!strcmp(argv[i]+1, "sport") && 
			iscmd(mode, FLOW) && i+1 < argc) {
		  sport = atoi(argv[i+1]);
		  i++;
	     } else if (!strcmp(argv[i]+1, "dport") && 
			iscmd(mode, FLOW) && i+1 < argc) {
		  dport = atoi(argv[i+1]);
		  i++;
	     } else if (!strcmp(argv[i]+1, "dst") && i+1 < argc) {
		  dst.s_addr = inet_addr(argv[i+1]);
		  i++;
	     } else if (!strcmp(argv[i]+1, "dst2") && 
			iscmd(mode, GRP_SPI) && i+1 < argc) {
		  dst2.s_addr = inet_addr(argv[i+1]);
		  i++;
	     } else if (!strcmp(argv[i]+1, "proto") && i+1 < argc) {
		  proto = atoi(argv[i+1]);
		  i++;
	     } else if (!strcmp(argv[i]+1, "proto2") && 
			iscmd(mode, GRP_SPI) && i+1 < argc) {
		  proto2 = atoi(argv[i+1]);
		  i++;
	     } else if (!strcmp(argv[i]+1, "chain") && chain == 0 &&
			iscmd(mode, DEL_SPI)) {
		  chain = 1;
	     } else {
		  fprintf(stderr, "%s: Unkown option: %s\n", argv[0], argv[i]);
		  exit(1);
	     }
	}


	/* Sanity checks */
	if ((mode & (ESP_NEW|ESP_OLD)) && enc == 0) {
	     fprintf(stderr, "%s: No encryption algorithm specified\n", 
		     argv[0]);
	     exit(1);
	} else if ((mode & (AH_NEW|AH_OLD)) && auth == 0) {
	     fprintf(stderr, "%s: No authenication algorithm specified\n", 
		     argv[0]);
	     exit(1);
	} else if (isencauth(mode) && keyp == NULL) {
	     fprintf(stderr, "%s: No key material specified\n", argv[0]);
	     exit(1);
	} else if ((mode & ESP_NEW) && auth && authp == NULL) {
	     fprintf(stderr, "%s: No auth key material specified\n", argv[0]);
	     exit(1);
	} else if (spi == 0) {
	     fprintf(stderr, "%s: No SPI specified\n", argv[0]);
	     exit(1);
	} else if (iscmd(mode, GRP_SPI) && spi2 == 0) {
	     fprintf(stderr, "%s: No SPI2 specified\n", argv[0]);
	     exit(1);
	} else if ((isencauth(mode) || iscmd(mode, ENC_IP)) && 
		    src.s_addr == 0) {
	     fprintf(stderr, "%s: No source address specified\n", argv[0]);
	     exit(1);
	} else if ((iscmd(mode, DEL_SPI) || iscmd(mode, GRP_SPI) || 
		   iscmd(mode, FLOW)) && 
		   proto != IPPROTO_ESP && proto != IPPROTO_AH) {
	     fprintf(stderr, "%s: Security protocol is neither AH or ESP\n", argv[0]);
	     exit(1);
	} else if (iscmd(mode, GRP_SPI) && 
		   proto2 != IPPROTO_ESP && proto2 != IPPROTO_AH) {
	     fprintf(stderr, "%s: Security protocol2 is neither AH or ESP\n", argv[0]);
	     exit(1);
	} else if (dst.s_addr == 0) {
	     fprintf(stderr, "%s: No destination address specified\n", 
		     argv[0]);
	     exit(1);
	} else if (iscmd(mode, ENC_IP) && 
		   (odst.s_addr == 0 || osrc.s_addr == 0)) {
	     fprintf(stderr, "%s: No tunnel addresses specified\n", 
		     argv[0]);
	     exit(1);
	} else if (iscmd(mode, FLOW) && 
		   (odst.s_addr == 0 && odmask.s_addr == 0 && 
		    osrc.s_addr == 0 && osmask.s_addr == 0)) {
	     fprintf(stderr, "%s: No subnets for flow specified\n", 
		     argv[0]);
	     exit(1);
	} else if (iscmd(mode, GRP_SPI) && dst2.s_addr == 0) {
	     fprintf(stderr, "%s: No destination address2 specified\n", 
		     argv[0]);
	     exit(1);
	}

	if (isencauth(mode)) {
	     switch(mode) {
	     case ESP_NEW:
		  result = xf_esp_new(src, dst, spi, enc, auth, ivp, keyp,
				     authp, osrc, odst, newpadding);
		  break;
	     case ESP_OLD:
		  result = xf_esp_old(src, dst, spi, enc, ivp, keyp, osrc, odst);
		  break;
	     case AH_NEW:
		  result = xf_ah_new(src, dst, spi, auth, keyp, osrc, odst);
		  break;
	     case AH_OLD:
		  result = xf_ah_old(src, dst, spi, auth, keyp, osrc, odst);
		  break;
	     }
	} else {
	     switch(mode & CMD_MASK) {
	     case GRP_SPI:
		  result = xf_grp(dst, spi, proto, dst2, spi2, proto2);
		  break;
	     case DEL_SPI:
		  result = xf_delspi(dst, spi, proto, chain);
		  break;
	     case ENC_IP:
		  result = xf_ip4(src, dst, spi, osrc, odst);
		  break;
	     case FLOW:
		  result = xf_flow(dst, spi, proto, osrc, osmask, odst, odmask,
				  tproto, sport, dport, delete, local);
		  break;
	     }
	}

	exit (result ? 0 : 1);
}
