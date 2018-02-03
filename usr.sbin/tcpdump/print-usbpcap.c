/*
 * Copyright (c) 2018 Martin Pieuchot <mpi@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <dev/usb/usb.h>
#include <dev/usb/usbpcap.h>

#include <pcap.h>

#include "interface.h"

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

const char *usbpcap_xfer_type[] = {"isoc", "intr", "ctrl", "bulk"};

void
usbpcap_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int length = h->len;
	u_int caplen = h->caplen;
	const struct usbpcap_pkt_hdr *uph;
	u_int16_t hdrlen;

	ts_print(&h->ts);

	/* check length */
	if (caplen < sizeof(uint16_t)) {
		printf("[|usb]");
		goto out;
	}
	uph = (struct usbpcap_pkt_hdr *)p;
	hdrlen = letoh16(uph->uph_hlen);
	if (hdrlen < sizeof(*uph)) {
		printf("[usb: invalid header length %u!]", hdrlen);
		goto out;
	}

	if (caplen < hdrlen) {
		printf("[|usb]");
		goto out;
	}

	printf("bus %u %c addr %u: ep%u",
	    letoh16(uph->uph_bus),
	     ((uph->uph_info & USBPCAP_INFO_DIRECTION_IN) ? '<' : '>'),
	    letoh16(uph->uph_devaddr), UE_GET_ADDR(uph->uph_epaddr));

	if (uph->uph_xfertype < nitems(usbpcap_xfer_type))
		printf(" %s", usbpcap_xfer_type[uph->uph_xfertype]);
	else
		printf(" ??");

	printf(" %u", letoh32(uph->uph_dlen));

	if (xflag)
		default_print(p + sizeof(*uph), length - sizeof(*uph));
out:
	putchar('\n');
}
