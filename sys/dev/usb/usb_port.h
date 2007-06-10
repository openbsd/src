/*	$OpenBSD: usb_port.h,v 1.74 2007/06/10 08:02:16 mbalmer Exp $ */
/*	$NetBSD: usb_port.h,v 1.62 2003/02/15 18:33:30 augustss Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/usb_port.h,v 1.21 1999/11/17 22:33:47 n_hibma Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef _USB_PORT_H
#define _USB_PORT_H

/*
 * Macro's to cope with the differences between operating systems.
 */

#include <sys/timeout.h>

#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
#define USB_USE_SOFTINTR
#else
#undef USB_USE_SOFTINTR
#endif

#define UMASS_ATAPISTR		"atapiscsi"

/* periph_quirks */
#define	PQUIRK_NOSENSE		ADEV_NOSENSE	/* can't REQUEST SENSE */
#define PQUIRK_ONLYBIG		SDEV_ONLYBIG

#define sel_klist si_note

typedef struct proc *usb_proc_ptr;

#define UCOMBUSCF_PORTNO		0
#define UCOMBUSCF_PORTNO_DEFAULT	-1
#define UHIDBUSCF_REPORTID		0
#define UHIDBUSCF_REPORTID_DEFAULT	-1

#define mstohz(ms) ((ms) * hz / 1000)

#define sel_klist si_note

#define IF_INPUT(ifp, m) ether_input_mbuf((ifp), (m))

#define swap_bytes_change_sign16_le swap_bytes_change_sign16
#define change_sign16_swap_bytes_le change_sign16_swap_bytes
#define change_sign16_le change_sign16

#define ulinear8_to_slinear16_le ulinear8_to_linear16_le
#define ulinear8_to_slinear16_be ulinear8_to_linear16_be
#define slinear16_to_ulinear8_le linear16_to_ulinear8_le
#define slinear16_to_ulinear8_be linear16_to_ulinear8_be

typedef struct device *device_ptr_t;
#define USBDEV(bdev) (&(bdev))
#define USBDEVNAME(bdev) ((bdev).dv_xname)
#define USBDEVUNIT(bdev) ((bdev).dv_unit)
#define USBDEVPTRNAME(bdevptr) ((bdevptr)->dv_xname)
#define USBGETSOFTC(d) ((void *)(d))

#define DECLARE_USB_DMA_T \
	struct usb_dma_block; \
	typedef struct { \
		struct usb_dma_block *block; \
		u_int offs; \
	} usb_dma_t

typedef struct timeout usb_callout_t;
#define usb_callout_init(h)	timeout_set(&(h), NULL, NULL)
#define usb_callout(h, t, f, d) \
	do { \
		timeout_del(&(h)); \
		timeout_set(&(h), (f), (d)); \
		timeout_add(&(h), (t)); \
	} while (0)
#define usb_callout_pending(h)	timeout_pending(&(h))
#define usb_uncallout(h, f, d) timeout_del(&(h))

#define USB_DECLARE_DRIVER_CLASS(dname, devclass)  \
int __CONCAT(dname,_match)(struct device *, void *, void *); \
void __CONCAT(dname,_attach)(struct device *, struct device *, void *); \
int __CONCAT(dname,_detach)(struct device *, int); \
int __CONCAT(dname,_activate)(struct device *, enum devact); \
\
struct cfdriver __CONCAT(dname,_cd) = { \
	NULL, #dname, devclass \
}; \
\
const struct cfattach __CONCAT(dname,_ca) = { \
	sizeof(struct __CONCAT(dname,_softc)), \
	__CONCAT(dname,_match), \
	__CONCAT(dname,_attach), \
	__CONCAT(dname,_detach), \
	__CONCAT(dname,_activate), \
}

#define USB_DECLARE_DRIVER(dname) USB_DECLARE_DRIVER_CLASS(dname, DV_DULL)

#define USB_GET_SC_OPEN(dname, unit, sc) \
	if (unit >= __CONCAT(dname,_cd).cd_ndevs) \
		return (ENXIO); \
	sc = __CONCAT(dname,_cd).cd_devs[unit]; \
	if (sc == NULL) \
		return (ENXIO)

#define USB_GET_SC(dname, unit, sc) \
	sc = __CONCAT(dname,_cd).cd_devs[unit]

#define USB_DO_ATTACH(dev, bdev, parent, args, print, sub) \
	(config_found_sm(parent, args, print, sub))

#endif /* _USB_PORT_H */
