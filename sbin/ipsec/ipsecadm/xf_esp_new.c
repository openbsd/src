/* $OpenBSD: xf_esp_new.c,v 1.7 1998/05/24 13:29:07 provos Exp $ */
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
 
extern char buf[];

int xf_set __P(( struct encap_msghdr *));
int x2i __P((char *));

int
xf_esp_new(src, dst, spi, enc, auth, ivp, keyp, authp, 
	   osrc, odst, newpadding)
struct in_addr src, dst;
u_int32_t spi;
int enc, auth;
u_char *ivp, *keyp, *authp;
struct in_addr osrc, odst;
int newpadding;
{
	int i, klen, alen, ivlen;

	struct encap_msghdr *em;
	struct esp_new_xencap *xd;

	klen = strlen(keyp)/2;
	alen = authp == NULL ? 0 : strlen(authp)/2;
	ivlen = ivp == NULL ? 0 : strlen(ivp)/2;

	em = (struct encap_msghdr *)&buf[0];
	
	em->em_msglen = EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN + 
	     ivlen + klen + alen;

	em->em_version = PFENCAP_VERSION_1;
	em->em_type = EMT_SETSPI;
	em->em_spi = spi;
	em->em_src = src;
	em->em_dst = dst;
	em->em_osrc = osrc;
	em->em_odst = odst;
	em->em_alg = XF_NEW_ESP;
	em->em_sproto = IPPROTO_ESP;

	xd = (struct esp_new_xencap *)(em->em_dat);

	xd->edx_enc_algorithm = enc;
	xd->edx_hash_algorithm = auth;
	xd->edx_ivlen = ivlen;
	xd->edx_confkeylen = klen;
	xd->edx_authkeylen = alen;
	xd->edx_wnd = -1;	/* Manual keying -- no seq */
	xd->edx_flags = auth ? ESP_NEW_FLAG_AUTH : 0;
	
	if (newpadding)
	  xd->edx_flags |= ESP_NEW_FLAG_NPADDING;

	for (i = 0; i < ivlen; i++)
	     xd->edx_data[i] = x2i(ivp+2*i);

	for (i = 0; i < klen; i++)
	     xd->edx_data[i+ivlen] = x2i(keyp+2*i);

	for (i = 0; i < alen; i++)
	     xd->edx_data[i+ivlen+klen] = x2i(authp+2*i);

	return xf_set(em);
}


