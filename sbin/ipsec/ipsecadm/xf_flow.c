/*	$OpenBSD: xf_flow.c,v 1.1 1998/05/24 13:29:10 provos Exp $	*/
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

extern char buf[];

int
xf_flow(struct in_addr dst, u_int32_t spi, int proto,
	struct in_addr osrc, struct in_addr osmask,
	struct in_addr odst, struct in_addr odmask,
	int tproto, int sport, int dport, int delete, int local)
{
    struct sockaddr_encap *ddst, *msk, *gw;
    struct rt_msghdr *rtm;
    int sd, off;

    sd = socket(PF_ROUTE, SOCK_RAW, AF_UNSPEC);
    if (sd < 0) {
      perror("socket");
      return 0;
    }

    bzero(buf, sizeof(*rtm) + SENT_IP4_LEN + SENT_IPSP_LEN + SENT_IP4_LEN);
	
    rtm = (struct rt_msghdr *)(&buf[0]);
    ddst = (struct sockaddr_encap *) (&buf[sizeof (*rtm)]);
    off = sizeof(*rtm) + SENT_IP4_LEN;
    if (!delete) {
	gw = (struct sockaddr_encap *) (&buf[off]);
	off += SENT_IPSP_LEN;
    }
    msk = (struct sockaddr_encap *) (&buf[off]);
	
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_type = delete ? RTM_DELETE : RTM_ADD;
    rtm->rtm_index = 0;
    rtm->rtm_pid = getpid();
    rtm->rtm_addrs = RTA_DST | (delete ? 0 : RTA_GATEWAY) | RTA_NETMASK;
    rtm->rtm_errno = 0;
    rtm->rtm_flags = RTF_UP | (delete ? 0 : RTF_GATEWAY) | RTF_STATIC;
    rtm->rtm_inits = 0;
	
    ddst->sen_len = SENT_IP4_LEN;
    ddst->sen_family = AF_ENCAP;
    ddst->sen_type = SENT_IP4;
    ddst->sen_ip_src.s_addr = osrc.s_addr;
    ddst->sen_ip_dst.s_addr = odst.s_addr;
    ddst->sen_proto = ddst->sen_sport = ddst->sen_dport = 0;
    
    if (tproto > 0) {
	ddst->sen_proto = tproto;
	msk->sen_proto = 0xff;

	if (sport > 0) {
	    ddst->sen_sport = sport;
	    msk->sen_sport = 0xffff;
	}

	if (dport > 0) {
	    ddst->sen_dport = dport;
	    msk->sen_dport = 0xffff;
	}
    }

    if (!delete) {
	gw->sen_len = SENT_IPSP_LEN;
	gw->sen_family = AF_ENCAP;
	gw->sen_type = SENT_IPSP;
	gw->sen_ipsp_dst.s_addr = dst.s_addr;
	gw->sen_ipsp_spi = spi;
	gw->sen_ipsp_sproto = proto;
    }

    msk->sen_len = SENT_IP4_LEN;
    msk->sen_family = AF_ENCAP;
    msk->sen_type = SENT_IP4;
    msk->sen_ip_src.s_addr = osmask.s_addr;
    msk->sen_ip_dst.s_addr = odmask.s_addr;

    rtm->rtm_msglen = sizeof(*rtm) + ddst->sen_len + 
	(delete ? 0 : gw->sen_len) + msk->sen_len;
	
    if (write(sd, (caddr_t) buf, rtm->rtm_msglen) < 0) {
	perror("write");
	return 0;
    }

    /* Additionally create/delete a flow for local packets */
    if (local) {
	 ddst->sen_ip_src.s_addr = 0;
	 msk->sen_ip_src.s_addr = INADDR_ANY;
	 if (write(sd, (caddr_t) buf, rtm->rtm_msglen) < 0) {
	      perror("write");
	      return 0;
	 }
    }
    return 1;
}
