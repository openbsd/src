/*
 * The author of this code is John Ioannidis, ji@tla.org,
 * 	(except when noted otherwise).
 *
 * This code was written for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis, kermit@forthnet.gr.
 *
 * Copyright (C) 1995, 1996, 1997 by John Ioannidis and Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NEITHER AUTHOR MAKES ANY
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
 

#define IFT_ENC 0x37


char buf[1024];

int x2i(char *s)
{
	char ss[3];
	ss[0] = s[0];
	ss[1] = s[1];
	ss[2] = 0;

	return strtol(ss, NULL, 16);
}
	

main(argc, argv)
int argc;
char **argv;
{
	int sd, len, i;

	struct encap_msghdr *em;
	struct espdesmd5_xencap *xd;
	struct sockaddr_encap *dst, *msk, *gw;
	struct sockaddr_dl *dl;
	u_char *opts;

	if (argc != 5)
	  fprintf(stderr, "usage: %s dst spi iv key\n", argv[0]), exit(1);
	sd = socket(AF_ENCAP, SOCK_RAW, AF_UNSPEC);
	if (sd < 0)
	  perror("socket"), exit(1);
	
	em = (struct encap_msghdr *)&buf[0];
	
	em->em_msglen = EMT_SETSPI_FLEN + ESPDESMD5_ULENGTH;
	em->em_version = 0;
	em->em_type = EMT_SETSPI;
	em->em_spi = htonl(strtol(argv[2], NULL, 16));
	em->em_if = 1;
	em->em_dst.s_addr = inet_addr(argv[1]);
	em->em_alg = XF_ESPDESMD5;
	xd = (struct espdesmd5_xencap *)(em->em_dat);

	xd->edx_ivlen = 0;
	xd->edx_initiator = 1;
	xd->edx_wnd = 32;
	xd->edx_keylen = 8;

#if 0
#define max(_a,_b) (((_a)>(_b))?(_a):(_b))

	memcpy(&(xd->edx_iv[0]), argv[3], max(strlen(argv[3]), 8));
	memcpy(&(xd->edx_iv[8]), argv[4], max(strlen(argv[4]), 8));
#endif

	for (i = 0; i < 8; i++)
	  xd->edx_key[i] = x2i(&(argv[4][2*i]));

	if (write(sd, buf, em->em_msglen) != em->em_msglen)
	  perror("write");
}


