/* $OpenBSD: usbpcap.h,v 1.1 2018/02/03 13:37:37 mpi Exp $ */

/*
 * Copyright (c) 2018 Martin Pieuchot
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

#ifndef _USBCAP_H_
#define _USBCAP_H_

/*
 * Common DLT_USBPCAP header.
 */
struct usbpcap_pkt_hdr {
	uint16_t		uph_hlen;	/* header length */
	uint64_t		uph_id;		/* request ID */
	uint32_t		uph_status;	/* USB status code */
	uint16_t		uph_function;	/* stack function ID */
	uint8_t			uph_info;	/* info flags */
#define USBPCAP_INFO_DIRECTION_IN	(1 << 0)/* from Device to Host */

	uint16_t		uph_bus;	/* bus number */
	uint16_t		uph_devaddr;	/* device address */
	uint8_t			uph_epaddr;	/* copy of bEndpointAddress */
	uint8_t			uph_xfertype;	/* transfer type */
#define USBPCAP_TRANSFER_ISOCHRONOUS	0
#define USBPCAP_TRANSFER_INTERRUPT	1
#define USBPCAP_TRANSFER_CONTROL	2
#define USBPCAP_TRANSFER_BULK		3

	uint32_t		uph_dlen;	/* data length */
} __attribute__((packed));

/*
 * Header used when dumping control transfers.
 */
struct usbpcap_ctl_hdr {
	struct usbpcap_pkt_hdr	uch_hdr;
	uint8_t			uch_stage;
#define USBPCAP_CONTROL_STAGE_SETUP	0
#define USBPCAP_CONTROL_STAGE_DATA	1
#define USBPCAP_CONTROL_STAGE_STATUS	2
} __attribute__((packed));

#endif /* _USBCAP_H_ */
