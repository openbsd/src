/*	$OpenBSD: usb_port.h,v 1.36 2002/05/07 18:08:04 nate Exp $ */
/*	$NetBSD: usb_port.h,v 1.54 2002/03/28 21:49:19 ichiro Exp $	*/
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

#if defined(__NetBSD__)
/*
 * NetBSD
 */

#include "opt_usbverbose.h"

#define USB_USE_SOFTINTR

#ifdef USB_DEBUG
#define UKBD_DEBUG 1
#define UHIDEV_DEBUG 1
#define UHID_DEBUG 1
#define OHCI_DEBUG 1
#define UGEN_DEBUG 1
#define UHCI_DEBUG 1
#define UHUB_DEBUG 1
#define ULPT_DEBUG 1
#define UCOM_DEBUG 1
#define UPLCOM_DEBUG 1
#define UMCT_DEBUG 1
#define UMODEM_DEBUG 1
#define UAUDIO_DEBUG 1
#define AUE_DEBUG 1
#define CUE_DEBUG 1
#define KUE_DEBUG 1
#define URL_DEBUG 1
#define UMASS_DEBUG 1
#define UVISOR_DEBUG 1
#define UPL_DEBUG 1
#define UZCOM_DEBUG 1
#define URIO_DEBUG 1
#define UFTDI_DEBUG 1
#define USCANNER_DEBUG 1
#define USSCANNER_DEBUG 1
#define EHCI_DEBUG 1
#define UIRDA_DEBUG 1
#define USTIR_DEBUG 1
#define UISDATA_DEBUG 1
#define UDSBR_DEBUG 1
#define Static
#else
#define Static static
#endif

#define SCSI_MODE_SENSE		MODE_SENSE

#define UMASS_ATAPISTR		"atapibus"

typedef struct proc *usb_proc_ptr;

typedef struct device *device_ptr_t;
#define USBBASEDEVICE struct device
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

typedef struct callout usb_callout_t;
#define usb_callout_init(h)	callout_init(&(h))
#define	usb_callout(h, t, f, d)	callout_reset(&(h), (t), (f), (d))
#define usb_callout_pending(h)	callout_pending(&(h))
#define	usb_uncallout(h, f, d)	callout_stop(&(h))

#define usb_lockmgr(l, f, sl, p) lockmgr((l), (f), (sl))

#define usb_kthread_create1	kthread_create1
#define usb_kthread_create	kthread_create

typedef int usb_malloc_type;

#define Ether_ifattach ether_ifattach
#define IF_INPUT(ifp, m) (*(ifp)->if_input)((ifp), (m))

#define logprintf printf

#define USB_DECLARE_DRIVER(dname)  \
int __CONCAT(dname,_match)(struct device *, struct cfdata *, void *); \
void __CONCAT(dname,_attach)(struct device *, struct device *, void *); \
int __CONCAT(dname,_detach)(struct device *, int); \
int __CONCAT(dname,_activate)(struct device *, enum devact); \
\
extern struct cfdriver __CONCAT(dname,_cd); \
\
struct cfattach __CONCAT(dname,_ca) = { \
	sizeof(struct __CONCAT(dname,_softc)), \
	__CONCAT(dname,_match), \
	__CONCAT(dname,_attach), \
	__CONCAT(dname,_detach), \
	__CONCAT(dname,_activate), \
}

#define USB_MATCH(dname) \
int __CONCAT(dname,_match)(struct device *parent, struct cfdata *match, void *aux)

#define USB_MATCH_START(dname, uaa) \
	struct usb_attach_arg *uaa = aux

#define USB_ATTACH(dname) \
void __CONCAT(dname,_attach)(struct device *parent, struct device *self, void *aux)

#define USB_ATTACH_START(dname, sc, uaa) \
	struct __CONCAT(dname,_softc) *sc = \
		(struct __CONCAT(dname,_softc) *)self; \
	struct usb_attach_arg *uaa = aux

/* Returns from attach */
#define USB_ATTACH_ERROR_RETURN	return
#define USB_ATTACH_SUCCESS_RETURN	return

#define USB_ATTACH_SETUP printf("\n")

#define USB_DETACH(dname) \
int __CONCAT(dname,_detach)(struct device *self, int flags)

#define USB_DETACH_START(dname, sc) \
	struct __CONCAT(dname,_softc) *sc = \
		(struct __CONCAT(dname,_softc) *)self

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

#elif defined(__OpenBSD__)
/*
 * OpenBSD
 */
#include <sys/timeout.h>

#define USB_USE_SOFTINTR

#define USB_DEBUG
#ifdef USB_DEBUG
#define UKBD_DEBUG 1
#define UHIDEV_DEBUG 1
#define UHID_DEBUG 1
#define OHCI_DEBUG 1
#define UGEN_DEBUG 1
#define UHCI_DEBUG 1
#define UHUB_DEBUG 1
#define ULPT_DEBUG 1
#define UCOM_DEBUG 1
#define UPLCOM_DEBUG 1
#define UMCT_DEBUG 1
#define UMODEM_DEBUG 1
#define UAUDIO_DEBUG 1
#define AUE_DEBUG 1
#define CUE_DEBUG 1
#define KUE_DEBUG 1
#define UMASS_DEBUG 1
#define UVISOR_DEBUG 1
#define UPL_DEBUG 1
#define UZCOM_DEBUG 1
#define URIO_DEBUG 1
#define UFTDI_DEBUG 1
#define USCANNER_DEBUG 1
#define USSCANNER_DEBUG 1
#define EHCI_DEBUG 1
#define UISDATA_DEBUG 1
#define UDSBR_DEBUG 1
#endif

#define Static

#define UMASS_ATAPISTR		"atapiscsi"

/* periph_quirks */
#define	PQUIRK_AUTOSAVE		0x00000001	/* do implicit SAVE POINTERS */
#define	PQUIRK_NOSYNC		0x00000002	/* does not grok SDTR */
#define	PQUIRK_NOWIDE		0x00000004	/* does not grok WDTR */
#define	PQUIRK_NOTAG		0x00000008	/* does not grok tagged cmds */
#define	PQUIRK_NOLUNS		0x00000010	/* DTWT with LUNs */
#define	PQUIRK_FORCELUNS	0x00000020	/* prehistoric device groks
						   LUNs */
#define	PQUIRK_NOMODESENSE	0x00000040	/* device doesn't do MODE SENSE
						   properly */
#define	PQUIRK_NOSTARTUNIT	0x00000080	/* do not issue START UNIT */
#define	PQUIRK_NOSYNCCACHE	0x00000100	/* do not issue SYNC CACHE */
#define	PQUIRK_CDROM		0x00000200	/* device is a CD-ROM, no
						   matter what else it claims */
#define	PQUIRK_LITTLETOC	0x00000400	/* audio TOC is little-endian */
#define	PQUIRK_NOCAPACITY	0x00000800	/* no READ CD CAPACITY */
#define	PQUIRK_NOTUR		0x00001000	/* no TEST UNIT READY */
#define	PQUIRK_NODOORLOCK	0x00002000	/* can't lock door */
#define	PQUIRK_NOSENSE		0x00004000	/* can't REQUEST SENSE */
#define PQUIRK_ONLYBIG		0x00008000	/* only use SCSI_{R,W}_BIG */
#define PQUIRK_BYTE5_ZERO	0x00010000	/* byte5 in capacity is wrong */
#define PQUIRK_NO_FLEX_PAGE	0x00020000	/* does not support flex geom
						   page */
#define PQUIRK_NOBIGMODESENSE	0x00040000	/* has no big mode-sense op */
#define PQUIRK_CAP_SYNC		0x00080000	/* SCSI1 device with sync op */

typedef struct proc *usb_proc_ptr;

#define UCOMBUSCF_PORTNO		-1
#define UCOMBUSCF_PORTNO_DEFAULT	-1
#define UHIDBUSCF_REPORTID		-1
#define UHIDBUSCF_REPORTID_DEFAULT	-1

#define bswap32(x)		swap32(x)
#define bswap16(x)		swap16(x)

/*
 * The UHCI/OHCI controllers are little endian, so on big endian machines
 * the data strored in memory needs to be swapped.
 */

#if defined(letoh32)
#define le32toh(x) letoh32(x)
#define le16toh(x) letoh16(x)
#endif

#define usb_kthread_create1	kthread_create
#define usb_kthread_create	kthread_create_deferred

#define	config_pending_incr()
#define	config_pending_decr()

typedef int usb_malloc_type;

#define Ether_ifattach(ifp, eaddr) ether_ifattach(ifp)
#define if_deactivate(x)
#define IF_INPUT(ifp, m) ether_input_mbuf((ifp), (m))

#define	usbpoll			usbselect
#define	uhidpoll		uhidselect
#define	ugenpoll		ugenselect
#define	uriopoll		urioselect
#define uscannerpoll		uscannerselect

#define logprintf printf

#define swap_bytes_change_sign16_le swap_bytes_change_sign16
#define change_sign16_swap_bytes_le change_sign16_swap_bytes
#define change_sign16_le change_sign16

#define ulinear8_to_slinear16_le ulinear8_to_linear16_le
#define ulinear8_to_slinear16_be ulinear8_to_linear16_be
#define slinear16_to_ulinear8_le linear16_to_ulinear8_le
#define slinear16_to_ulinear8_be linear16_to_ulinear8_be

#define realloc usb_realloc
void *usb_realloc(void *, u_int, int, int);

typedef struct proc *usb_proc_ptr;
typedef struct device *device_ptr_t;
#define USBBASEDEVICE struct device
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
#define usb_callout_init(h)
#define usb_callout(h, t, f, d) \
	{ timeout_set(&(h), (f), (d)); timeout_add(&(h), (t)); }
#define usb_callout_pending(h)	timeout_pending(&(h))
#define usb_uncallout(h, f, d) timeout_del(&(h))

#define usb_lockmgr(l, f, sl, p) lockmgr((l), (f), (sl), (p))

#define USB_DECLARE_DRIVER(dname)  \
int __CONCAT(dname,_match)(struct device *, void *, void *); \
void __CONCAT(dname,_attach)(struct device *, struct device *, void *); \
int __CONCAT(dname,_detach)(struct device *, int); \
int __CONCAT(dname,_activate)(struct device *, enum devact); \
\
struct cfdriver __CONCAT(dname,_cd) = { \
	NULL, #dname, DV_DULL \
}; \
\
struct cfattach __CONCAT(dname,_ca) = { \
	sizeof(struct __CONCAT(dname,_softc)), \
	__CONCAT(dname,_match), \
	__CONCAT(dname,_attach), \
	__CONCAT(dname,_detach), \
	__CONCAT(dname,_activate), \
}

#define USB_MATCH(dname) \
int \
__CONCAT(dname,_match)(parent, match, aux) \
	struct device *parent; \
	void *match; \
	void *aux;

#define USB_MATCH_START(dname, uaa) \
	struct usb_attach_arg *uaa = aux

#define USB_ATTACH(dname) \
void \
__CONCAT(dname,_attach)(parent, self, aux) \
	struct device *parent; \
	struct device *self; \
	void *aux;

#define USB_ATTACH_START(dname, sc, uaa) \
	struct __CONCAT(dname,_softc) *sc = \
		(struct __CONCAT(dname,_softc) *)self; \
	struct usb_attach_arg *uaa = aux

/* Returns from attach */
#define USB_ATTACH_ERROR_RETURN	return
#define USB_ATTACH_SUCCESS_RETURN	return

#define USB_ATTACH_SETUP printf("\n")

#define USB_DETACH(dname) \
int \
__CONCAT(dname,_detach)(self, flags) \
	struct device *self; \
	int flags;

#define USB_DETACH_START(dname, sc) \
	struct __CONCAT(dname,_softc) *sc = \
		(struct __CONCAT(dname,_softc) *)self

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

#elif defined(__FreeBSD__)
/*
 * FreeBSD
 */

#include "opt_usb.h"

#if defined(_KERNEL)
#include <sys/malloc.h>

MALLOC_DECLARE(M_USB);
MALLOC_DECLARE(M_USBDEV);
MALLOC_DECLARE(M_USBHC);

#endif

#define Static

#define USBVERBOSE

#define device_ptr_t device_t
#define USBBASEDEVICE device_t
#define USBDEV(bdev) (bdev)
#define USBDEVNAME(bdev) device_get_nameunit(bdev)
#define USBDEVUNIT(bdev) device_get_unit(bdev)
#define USBDEVPTRNAME(bdev) device_get_nameunit(bdev)
#define USBGETSOFTC(bdev) (device_get_softc(bdev))

#define DECLARE_USB_DMA_T typedef void * usb_dma_t

typedef struct proc *usb_proc_ptr;

/* XXX Change this when FreeBSD has memset
 */
#define	memcpy(d, s, l)		bcopy((s),(d),(l))
#define	memset(d, v, l)		bzero((d),(l))
#define bswap32(x)		swap32(x)
#define kthread_create1(f, s, p, a0, a1) \
		kthread_create((f), (s), (p), RFHIGHPID, (a0), (a1))

typedef struct callout_handle usb_callout_t;
#define usb_callout_init(h) callout_handle_init(&(h))
#define usb_callout(h, t, f, d) ((h) = timeout((f), (d), (t)))
#define usb_uncallout(h, f, d) uncallout((f), (d), (h))

#define clalloc(p, s, x) (clist_alloc_cblocks((p), (s), (s)), 0)
#define clfree(p) clist_free_cblocks((p))

#define powerhook_establish(fn, sc) (fn)
#define powerhook_disestablish(hdl)
#define PWR_RESUME 0

#define config_detach(dev, flag) device_delete_child(device_get_parent(dev), dev)

typedef struct malloc_type *usb_malloc_type;

#define USB_DECLARE_DRIVER_INIT(dname, init) \
Static device_probe_t __CONCAT(dname,_match); \
Static device_attach_t __CONCAT(dname,_attach); \
Static device_detach_t __CONCAT(dname,_detach); \
\
Static devclass_t __CONCAT(dname,_devclass); \
\
Static device_method_t __CONCAT(dname,_methods)[] = { \
        DEVMETHOD(device_probe, __CONCAT(dname,_match)), \
        DEVMETHOD(device_attach, __CONCAT(dname,_attach)), \
        DEVMETHOD(device_detach, __CONCAT(dname,_detach)), \
	init, \
        {0,0} \
}; \
\
Static driver_t __CONCAT(dname,_driver) = { \
        #dname, \
        __CONCAT(dname,_methods), \
        sizeof(struct __CONCAT(dname,_softc)) \
}
#define METHODS_NONE			{0,0}
#define USB_DECLARE_DRIVER(dname)	USB_DECLARE_DRIVER_INIT(dname, METHODS_NONE)


#define USB_MATCH(dname) \
Static int \
__CONCAT(dname,_match)(device_t self)

#define USB_MATCH_START(dname, uaa) \
        struct usb_attach_arg *uaa = device_get_ivars(self)

#define USB_ATTACH(dname) \
Static int \
__CONCAT(dname,_attach)(device_t self)

#define USB_ATTACH_START(dname, sc, uaa) \
        struct __CONCAT(dname,_softc) *sc = device_get_softc(self); \
        struct usb_attach_arg *uaa = device_get_ivars(self)

/* Returns from attach */
#define USB_ATTACH_ERROR_RETURN	return ENXIO
#define USB_ATTACH_SUCCESS_RETURN	return 0

#define USB_ATTACH_SETUP \
	sc->sc_dev = self; \
	device_set_desc_copy(self, devinfo)

#define USB_DETACH(dname) \
Static int \
__CONCAT(dname,_detach)(device_t self)

#define USB_DETACH_START(dname, sc) \
	struct __CONCAT(dname,_softc) *sc = device_get_softc(self)

#define USB_GET_SC_OPEN(dname, unit, sc) \
	sc = devclass_get_softc(__CONCAT(dname,_devclass), unit); \
	if (sc == NULL) \
		return (ENXIO)

#define USB_GET_SC(dname, unit, sc) \
	sc = devclass_get_softc(__CONCAT(dname,_devclass), unit)

#define USB_DO_ATTACH(dev, bdev, parent, args, print, sub) \
	(device_probe_and_attach((bdev)) == 0 ? (bdev) : 0)

/* conversion from one type of queue to the other */
/* XXX In FreeBSD SIMPLEQ_REMOVE_HEAD only removes the head element.
 */
#define SIMPLEQ_REMOVE_HEAD(h, e, f)	do {				\
		if ( (e) != SIMPLEQ_FIRST((h)) )			\
			panic("Removing other than first element");	\
		STAILQ_REMOVE_HEAD(h, f);				\
} while (0)
#define SIMPLEQ_INSERT_HEAD	STAILQ_INSERT_HEAD
#define SIMPLEQ_INSERT_TAIL	STAILQ_INSERT_TAIL
#define SIMPLEQ_NEXT		STAILQ_NEXT
#define SIMPLEQ_FIRST		STAILQ_FIRST
#define SIMPLEQ_HEAD		STAILQ_HEAD
#define SIMPLEQ_INIT		STAILQ_INIT
#define SIMPLEQ_HEAD_INITIALIZER	STAILQ_HEAD_INITIALIZER
#define SIMPLEQ_ENTRY		STAILQ_ENTRY

#include <sys/syslog.h>
/*
#define logprintf(args...)	log(LOG_DEBUG, args)
*/
#define logprintf		printf

#endif /* __FreeBSD__ */

#endif /* _USB_PORT_H */

