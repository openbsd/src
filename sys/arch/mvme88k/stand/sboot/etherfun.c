/*	$OpenBSD: etherfun.c,v 1.3 2001/09/20 17:02:31 mpech Exp $	*/

/*
 *
 * Copyright (c) 1995 Charles D. Cranor and Seth Widoff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor
 *	and Seth Widoff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
/* etherfun.c */

#include <sys/cdefs.h>
#include "sboot.h"
#include "etherfun.h"

/* Construct and send a rev arp packet */
void
do_rev_arp()
{
	int     i;

	for (i = 0; i < 6; i++)
		eh->ether_dhost[i] = 0xff;

	bcopy(myea, eh->ether_shost, 6);
	eh->ether_type = ETYPE_RARP;

	rarp->ar_hrd = 1;	/* hardware type is 1 */
	rarp->ar_pro = PTYPE_IP;
	rarp->ar_hln = 6;	/* length of hardware address is 6 bytes */
	rarp->ar_pln = 4;	/* length of ip address is 4 byte */
	rarp->ar_op = OPCODE_RARP;
	bcopy(myea, rarp->arp_sha, sizeof(myea));
	bcopy(myea, rarp->arp_tha, sizeof(myea));
	for (i = 0; i < 4; i++)
		rarp->arp_spa[i] = rarp->arp_tpa[i] = 0x00;

	le_put(buf, 76);
}

/* Receive and disassemble the rev_arp reply */
int
get_rev_arp()
{
	le_get(buf, sizeof(buf), 6);
	if (eh->ether_type == ETYPE_RARP && rarp->ar_op == OPCODE_REPLY) {
		bcopy(rarp->arp_tpa, myip, sizeof(rarp->arp_tpa));
		bcopy(rarp->arp_spa, servip, sizeof(rarp->arp_spa));
		bcopy(rarp->arp_sha, servea, sizeof(rarp->arp_sha));
		return (1);
	}
	return (0);
}

/* Try to get a reply to a rev arp request */
int
rev_arp()
{
	int     tries = 0;
	while (tries < 5) {
		do_rev_arp();
		if (get_rev_arp())
			return (1);
		tries++;
	}
	return (0);
}

/*
 * Send a tftp read request or acknowledgement
 * mesgtype 0 is a read request, 1 is an
 * acknowledgement
 */
void
do_send_tftp(mesgtype)
	int mesgtype;
{
	u_long  res, iptmp, lcv;
	char   *tot;

	if (mesgtype == 0) {
		tot = tftp_r + (sizeof(MSG) - 1);
		myport = (u_short) time();
		if (myport < 1000)
			myport += 1000;
		servport = FTP_PORT;	/* to start */
	} else {
		tot = (char *) tftp_a + 4;
	}

	bcopy(servea, eh->ether_dhost, sizeof(servea));
	bcopy(myea, eh->ether_shost, sizeof(myea));
	eh->ether_type = ETYPE_IP;

	iph->ip_v = IP_VERSION;
	iph->ip_hl = IP_HLEN;
	iph->ip_tos = 0;	/* type of service is 0 */
	iph->ip_id = 0;		/* id field is 0 */
	iph->ip_off = IP_DF;
	iph->ip_ttl = 3;	/* time to live is 3 seconds/hops */
	iph->ip_p = IPP_UDP;
	bcopy(myip, iph->ip_src, sizeof(myip));
	bcopy(servip, iph->ip_dst, sizeof(servip));
	iph->ip_sum = 0;
	iph->ip_len = tot - (char *) iph;
	res = oc_cksum(iph, sizeof(struct ip), 0);
	iph->ip_sum = 0xffff & ~res;
	udph->uh_sport = myport;
	udph->uh_dport = servport;
	udph->uh_sum = 0;

	if (mesgtype) {
		tftp_a->op_code = FTPOP_ACKN;
		tftp_a->block = (u_short) (mesgtype);
	} else {
		bcopy(myip, &iptmp, sizeof(iptmp));
		bcopy(MSG, tftp_r, (sizeof(MSG) - 1));
		for (lcv = 9; lcv >= 2; lcv--) {
			tftp_r[lcv] = "0123456789ABCDEF"[iptmp & 0xF];

			iptmp = iptmp >> 4;
		}
	}

	udph->uh_ulen = tot - (char *) udph;

	le_put(buf, tot - buf);
}

/* Attempt to tftp a file and read it into memory */
int
do_get_file()
{
	int     fail = 0, oldlen;
	char   *loadat = (char *) LOAD_ADDR;
	last_ack = 0;

	do_send_tftp(READ);
	while (1) {
		if (le_get(buf, sizeof(buf), 5) == 0) {
			/* timeout occurred */
			if (last_ack)
				do_send_tftp(last_ack);
			else
				do_send_tftp(READ);

			fail++;
			if (fail > 5) {
				printf("\n");
				return (1);
			}
		} else {
			printf("%x \r", tftp->info.block * 512);
			if ((eh->ether_type != ETYPE_IP) || (iph->ip_p != IPP_UDP)) {
				fail++;
				continue;
			}
			if (servport == FTP_PORT)
				servport = udph->uh_sport;
			if (tftp->info.op_code == FTPOP_ERR) {
				printf("TFTP: Download error %d: %s\n",
				    tftp->info.block, tftp->data);
				return (1);
			}
			if (tftp->info.block != last_ack + 1) {
				/* we received the wrong block */
				if (tftp->info.block < last_ack + 1) {
					/* nack whatever we received */
					do_send_tftp(tftp->info.block);
				} else {
					/* nack the last confirmed block */
					do_send_tftp(last_ack);
				}
				fail++;
			} else {/* we got the right block */
				fail = 0;
				last_ack++;
				oldlen = udph->uh_ulen;
				do_send_tftp(last_ack);
				/* printf("bcopy %x %x %d\n", &tftp->data,
				 * loadat, oldlen - 12); */
				bcopy(&tftp->data, loadat, oldlen - 12);
				loadat += oldlen - 12;
				if (oldlen < (8 + 4 + 512)) {
					printf("\n");
					return (0);
				}
			}
		}
	}
	printf("\n");
	return (0);
}
