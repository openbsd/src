/*	$OpenBSD: debug.c,v 1.2 2003/06/04 04:46:13 jason Exp $	*/

/*
 * Copyright (c) 2000 Network Security Technologies, Inc. http://www.netsec.net
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/ppp_defs.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <string.h>
#include <ctype.h>

#include "pppoe.h"

void
debug_packet(u_int8_t *pkt, int len)
{
	struct ether_header eh;
	struct pppoe_header ph;

	if (option_verbose == 0)
		return;

	if (len < sizeof(eh)) {
		printf("short packet (%d)\n", len);
		return;
	}
	bcopy(pkt, &eh, sizeof(eh));
	pkt += sizeof(eh);
	len -= sizeof(eh);
	eh.ether_type = ntohs(eh.ether_type);

	if (eh.ether_type == ETHERTYPE_PPPOE && option_verbose < 2)
		return;

	printf("%s -> %s %04x[",
	    ether_ntoa((struct ether_addr *)eh.ether_shost),
	    ether_ntoa((struct ether_addr *)eh.ether_dhost),
	    eh.ether_type);
	switch (eh.ether_type) {
	case ETHERTYPE_PPPOEDISC:
		printf("discovery]");
		break;
	case ETHERTYPE_PPPOE:
		printf("session]");
		break;
	default:
		printf("unknown]\n");
		return;
	}
	
	if (len < sizeof(ph)) {
		printf("short packet (%d)\n", len);
		return;
	}
	bcopy(pkt, &ph, sizeof(ph));
	pkt += sizeof(ph);
	len -= sizeof(ph);
	ph.sessionid = ntohs(ph.sessionid);
	ph.len = ntohs(ph.len);

	if (ph.len <= len)
		len = ph.len;
	else {
		printf("missing (%d)\n", ph.len - len);
		return;
	}

	printf(": version %d, type %d, ",
	    PPPOE_VER(ph.vertype), PPPOE_TYPE(ph.vertype));
	switch (ph.code) {
	case PPPOE_CODE_SESSION:
		printf("session");
	case PPPOE_CODE_PADO:
		printf("PADO");
		break;
	case PPPOE_CODE_PADI:
		printf("PADI");
		break;
	case PPPOE_CODE_PADR:
		printf("PADR");
		break;
	case PPPOE_CODE_PADS:
		printf("PADS");
		break;
	case PPPOE_CODE_PADT:
		printf("PADT");
		break;
	default:
		printf("unknown");
		break;
	}
	printf(", len %d\n", len);

	while (len > 0 && eh.ether_type == ETHERTYPE_PPPOEDISC) {
		int i;
		u_int16_t ttype, tlen;

		if (len < sizeof(ttype)) {
			printf("|\n");
			return;
		}
		bcopy(pkt, &ttype, sizeof(ttype));
		len -= sizeof(ttype);
		pkt += sizeof(ttype);
		ttype = ntohs(ttype);
		printf("\ttag ");
		switch (ttype) {
		case PPPOE_TAG_END_OF_LIST:
			printf("end-of-list");
			break;
		case PPPOE_TAG_SERVICE_NAME:
			printf("service-name");
			break;
		case PPPOE_TAG_AC_NAME:
			printf("ac-name");
			break;
		case PPPOE_TAG_HOST_UNIQ:
			printf("host-uniq");
			break;
		case PPPOE_TAG_AC_COOKIE:
			printf("ac-cookie");
			break;
		case PPPOE_TAG_VENDOR_SPEC:
			printf("vendor-spec");
			break;
		case PPPOE_TAG_RELAY_SESSION:
			printf("relay-session");
			break;
		case PPPOE_TAG_SERVICE_NAME_ERROR:
			printf("service-name-error");
			break;
		case PPPOE_TAG_AC_SYSTEM_ERROR:
			printf("ac-system-error");
			break;
		case PPPOE_TAG_GENERIC_ERROR:
			printf("generic-error");
			break;
		default:
			printf("unknown %04x", ttype);
			break;
		}

		if (len < sizeof(tlen)) {
			printf("|\n");
			return;
		}
		bcopy(pkt, &tlen, sizeof(tlen));
		len -= sizeof(tlen);
		pkt += sizeof(tlen);
		tlen = ntohs(tlen);
		printf(", len %d", tlen);

		if (len < tlen) {
			printf("|\n");
			return;
		}
		for (i = 0; i < tlen; i++) {
			if (isprint(pkt[i]))
				printf("%c", pkt[i]);
			else
				printf("\\%03o", pkt[i]);
		}
		len -= tlen;
	}
}
