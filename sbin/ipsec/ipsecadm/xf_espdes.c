/* $OpenBSD: xf_espdes.c,v 1.4 1997/07/11 23:50:24 provos Exp $ */
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
 
extern char buf[];

int xf_set __P(( struct encap_msghdr *));
int x2i __P((char *));

int
xf_espdes(argc, argv)
int argc;
char **argv;
{
	int i;

	struct encap_msghdr *em;
	struct esp_old_xencap *xd;

	if (argc != 6) {
	  fprintf(stderr, "usage: %s src dst spi iv key\n", argv[0]);
	  return 0;
	}
	
	em = (struct encap_msghdr *)&buf[0];
	
	em->em_msglen = EMT_SETSPI_FLEN + ESP_OLD_XENCAP_LEN + 4 + 8;
	em->em_version = PFENCAP_VERSION_1;
	em->em_type = EMT_SETSPI;
	em->em_spi = htonl(strtoul(argv[3], NULL, 16));
	em->em_src.s_addr = inet_addr(argv[1]);
	em->em_dst.s_addr = inet_addr(argv[2]);
	em->em_alg = XF_OLD_ESP;
	em->em_sproto = IPPROTO_ESP;

	xd = (struct esp_old_xencap *)(em->em_dat);

	xd->edx_enc_algorithm = ALG_ENC_DES;
	xd->edx_ivlen = 4;
	xd->edx_keylen = 8;

	for (i = 0; i < 4; i++)
	  xd->edx_data[i] = x2i(&(argv[4][2*i]));

	for (i = 0; i < 8; i++)
	  xd->edx_data[i+4] = x2i(&(argv[5][2*i]));

	return xf_set(em);
}


