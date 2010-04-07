/*	$OpenBSD: usb_quirks.h,v 1.15 2010/04/07 20:52:56 sthen Exp $ */
/*	$NetBSD: usb_quirks.h,v 1.20 2001/04/15 09:38:01 augustss Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/usb_quirks.h,v 1.9 1999/11/12 23:31:03 n_hibma Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

struct usbd_quirks {
	u_int32_t uq_flags;	/* Device problems: */
#define UQ_NO_SET_PROTO		0x00000001 /* cannot handle SET PROTOCOL */
#define UQ_SWAP_UNICODE		0x00000002 /* some Unicode strings swapped */
#define UQ_MS_REVZ		0x00000004 /* mouse has Z-axis reversed */
#define UQ_NO_STRINGS		0x00000008 /* string descriptors are broken */
#define UQ_BAD_ADC		0x00000010 /* bad audio spec version number */
#define UQ_BUS_POWERED		0x00000020 /* is bus-powered, despite claim */
#define UQ_BAD_AUDIO		0x00000040 /* claims audio class, but isn't */
#define UQ_SPUR_BUT_UP		0x00000080 /* spurious mouse button up events */
#define UQ_AU_NO_XU		0x00000100 /* audio device has broken
						extension unit */
#define UQ_POWER_CLAIM		0x00000200 /* hub lies about power status */
#define UQ_AU_NO_FRAC		0x00000400 /* don't adjust for fractional
						samples */
#define UQ_ASSUME_CM_OVER_DATA	0x00001000 /* modem device breaks on cm
						over data */
#define UQ_BROKEN_BIDIR		0x00002000 /* printer has broken bidir mode */
#define UQ_BAD_HID		0x00004000 /* device claims uhid, but isn't */
#define UQ_MS_BAD_CLASS		0x00008000 /* mouse doesn't identify properly */
#define UQ_MS_LEADING_BYTE	0x00010000 /* mouse sends unknown leading byte */
#define UQ_EHCI_NEEDTO_DISOWN	0x00020000 /* must hand device over to USB 1.1
						if attached to EHCI */
};

extern const struct usbd_quirks usbd_no_quirk;

const struct usbd_quirks *usbd_find_quirk(usb_device_descriptor_t *);
