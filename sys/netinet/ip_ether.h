/*	$OpenBSD: ip_ether.h,v 1.13 2002/06/09 16:26:10 itojun Exp $ */
/*
 * The author of this code is Angelos D. Keromytis (angelos@adk.gr)
 *
 * This code was written by Angelos D. Keromytis in October 1999.
 *
 * Copyright (C) 1999-2001 Angelos D. Keromytis.
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

#ifndef _NETINET_IP_ETHER_H_
#define _NETINET_IP_ETHER_H_

/*
 * Ethernet-inside-IP processing.
 */

struct etheripstat {
	u_int32_t	etherip_hdrops;		/* packet shorter than header shows */
	u_int32_t	etherip_qfull;		/* bridge queue full, packet dropped */
	u_int32_t	etherip_noifdrops;	/* no interface/bridge information */
	u_int32_t	etherip_pdrops;		/* packet dropped due to policy */
	u_int32_t	etherip_adrops;         /* all other drops */
	u_int32_t	etherip_ipackets;	/* total input packets */
	u_int32_t	etherip_opackets;	/* total output packets */
	u_int64_t	etherip_ibytes;		/* input bytes */
	u_int64_t	etherip_obytes;		/* output bytes */
};

struct etherip_header {
	u_int8_t	eip_ver;		/* version/reserved */
	u_int8_t	eip_pad;		/* required padding byte */
};
#define ETHERIP_VER_VERS_MASK	0x0f
#define ETHERIP_VER_RSVD_MASK	0xf0

#define ETHERIP_VERSION		0x03

/*
 * Names for Ether-IP sysctl objects
 */
#define	ETHERIPCTL_ALLOW	1	/* accept incoming EtherIP packets */
#define ETHERIPCTL_MAXID	2

#define ETHERIPCTL_NAMES { \
	{ 0, 0 }, \
	{ "allow", CTLTYPE_INT }, \
}

#ifdef _KERNEL
int	etherip_sysctl(int *, u_int, void *, size_t *, void *, size_t);

extern int etherip_allow;
extern struct etheripstat etheripstat;
#endif /* _KERNEL */
#endif /* _NETINET_IP_ETHER_H_ */
