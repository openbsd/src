/*	$OpenBSD: print-stp.c,v 1.1 2000/10/19 16:31:42 jason Exp $	*/

/*
 * Copyright (c) 2000 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Pretty print 802.1D Bridge Protocol Data Units
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#ifdef __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <signal.h>
#include <stdio.h>

#include <netinet/if_ether.h>
#include "ethertype.h"

#include <net/ppp_defs.h>
#include "interface.h"
#include "addrtoname.h"
#include "extract.h"
#include "llc.h"

#define	STP_MSGTYPE_CBPDU	0x00
#define	STP_MSGTYPE_TBPDU	0x80

#define	STP_FLAGS_TC		0x01            /* Topology change */
#define	STP_FLAGS_TCA		0x80            /* Topology change ack */

static void stp_print_cbpdu(const u_char *, u_int);
static void stp_print_tbpdu(const u_char *, u_int);

void
stp_print(p, len)
	const u_char *p;
	u_int len;
{
	u_int16_t id;

	if (len < 3)
		goto truncated;
	if (p[0] != LLCSAP_8021D || p[1] != LLCSAP_8021D || p[2] != LLC_UI) {
		printf("invalid protocol");
		return;
	}
	printf("802.1d");
	p += 3;
	len -= 3;

	if (len < 3)
		goto truncated;
	id = EXTRACT_16BITS(p);
	if (id != 0) {
		printf(" unknown protocol id(0x%x)", id);
		return;
	}
	if (p[2] != 0) {
		printf(" unknown protocol ver(0x%x)", p[2]);
		return;
	}
	p += 3;
	len -= 3;

	if (len < 1)
		goto truncated;
	switch (*p) {
	case STP_MSGTYPE_CBPDU:
		stp_print_cbpdu(p, len);
		break;
	case STP_MSGTYPE_TBPDU:
		stp_print_tbpdu(p, len);
		break;
	default:
		printf(" unknown message (0x%02x)", *p);
		break;
	}

	return;

truncated:
	printf("[|802.1d]");
}

static void
stp_print_cbpdu(p, len)
	const u_char *p;
	u_int len;
{
	u_int32_t cost;
	u_int16_t t;
	int x;

	p += 1;
	len -= 1;

	printf(" config");

	if (len < 1)
		goto truncated;
	if (*p) {
		x = 0;

		printf(" flags=0x%x<", *p);
		if ((*p) & STP_FLAGS_TC)
			printf("%stc", (x++ != 0) ? "," : "");
		if ((*p) & STP_FLAGS_TCA)
			printf("%stcack", (x++ != 0) ? "," : "");
		putchar('>');
	}
	p += 1;
	len -= 1;

	if (len < 8)
		goto truncated;
	printf(" root=");
	printf("%x.", EXTRACT_16BITS(p));
	p += 2;
	len -= 2;
	for (x = 0; x < 6; x++) {
		printf("%s%x", (x != 0) ? ":" : "", *p);
		p++;
		len--;
	}

	if (len < 4)
		goto truncated;
	cost = EXTRACT_32BITS(p);
	printf(" rootcost=0x%x", cost);
	p += 4;
	len -= 4;

	if (len < 8)
		goto truncated;
	printf(" bridge=");
	printf("%x.", EXTRACT_16BITS(p));
	p += 2;
	len -= 2;
	for (x = 0; x < 6; x++) {
		printf("%s%x", (x != 0) ? ":" : "", *p);
		p++;
		len--;
	}

	if (len < 2)
		goto truncated;
	t = EXTRACT_16BITS(p);
	printf(" port=0x%x", t);
	p += 2;
	len -= 2;

	if (len < 2)
		goto truncated;
	printf(" age=%u/%u", p[0], p[1]);
	p += 2;
	len -= 2;

	if (len < 2)
		goto truncated;
	printf(" max=%u/%u", p[0], p[1]);
	p += 2;
	len -= 2;

	if (len < 2)
		goto truncated;
	printf(" hello=%u/%u", p[0], p[1]);
	p += 2;
	len -= 2;

	if (len < 2)
		goto truncated;
	printf(" fwdelay=%u/%u", p[0], p[1]);
	p += 2;
	len -= 2;

	return;

truncated:
	printf("[|802.1d]");
}

static void
stp_print_tbpdu(p, len)
	const u_char *p;
	u_int len;
{
	printf(" tcn");
}
