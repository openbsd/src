/*	$OpenBSD: usb_port.h,v 1.60 2005/11/19 02:18:01 pedro Exp $ */
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

#if defined(__NetBSD__)
/*
 * NetBSD
 */

#include "opt_usbverbose.h"

#if defined(_KERNEL)
#include <sys/mallocvar.h>

MALLOC_DECLARE(M_USB);
MALLOC_DECLARE(M_USBDEV);
MALLOC_DECLARE(M_USBHC);

#endif

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
#define AXE_DEBUG 1
#define CUE_DEBUG 1
#define KUE_DEBUG 1
#define UDAV_DEBUG 1
#define URL_DEBUG 1
#define UMASS_DEBUG 1
#define UVISOR_DEBUG 1
#define UPL_DEBUG 1
#define UZCOM_DEBUG 1
#define URIO_DEBUG 1
#define UFTDI_DEBUG 1
#define USCANNER_DEBUG 1
#define USSCANNER_DEBUG 1
#define UIRDA_DEBUG 1
#define USTIR_DEBUG 1
#define UISDATA_DEBUG 1
#define UDSBR_DEBUG 1
#define UBT_DEBUG 1
#define UAX_DEBUG 1
#define UIPAQ_DEBUG 1
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

typedef struct malloc_type *usb_malloc_type;

#define Ether_ifattach ether_ifattach
#define IF_INPUT(ifp, m) (*(ifp)->if_input)((ifp), (m))

#define logprintf printf

#define	USB_DNAME(dname)	dname
#define USB_DECLARE_DRIVER(dname)  \
int __CONCAT(dname,_match)(struct device *, struct cfdata *, void *); \
void __CONCAT(dname,_attach)(struct device *, struct device *, void *); \
int __CONCAT(dname,_detach)(struct device *, int); \
int __CONCAT(dname,_activate)(struct device *, enum devact); \
\
extern struct cfdriver __CONCAT(dname,_cd); \
\
CFATTACH_DECL(USB_DNAME(dname), \
    sizeof(struct ___CONCAT(dname,_softc)), \
    ___CONCAT(dname,_match), \
    ___CONCAT(dname,_attach), \
    ___CONCAT(dname,_detach), \
    ___CONCAT(dname,_activate))

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

#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
#define USB_USE_SOFTINTR
#else
#undef USB_USE_SOFTINTR
#endif

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
#define UDAV_DEBUG 1
#define UMASS_DEBUG 1
#define UVISOR_DEBUG 1
#define UPL_DEBUG 1
#define UZCOM_DEBUG 1
#define URIO_DEBUG 1
#define UFTDI_DEBUG 1
#define USCANNER_DEBUG 1
#define USSCANNER_DEBUG 1
#define UISDATA_DEBUG 1
#define UDSBR_DEBUG 1
#define UIPAQ_DEBUG 1
#endif

#define Static

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

#define bswap32(x)		swap32(x)
#define bswap16(x)		swap16(x)

#define mstohz(ms) ((ms) * hz / 1000)

/*
 * The UHCI/OHCI controllers are little endian, so on big endian machines
 * the data stored in memory needs to be swapped.
 */

#if defined(letoh32)
#define le32toh(x) letoh32(x)
#define le16toh(x) letoh16(x)
#endif

#define sel_klist si_note

#define usb_kthread_create1	kthread_create
#define usb_kthread_create	kthread_create_deferred

typedef int usb_malloc_type;

#define Ether_ifattach(ifp, eaddr) ether_ifattach(ifp)
#define if_deactivate(x)
#define IF_INPUT(ifp, m) ether_input_mbuf((ifp), (m))

#define logprintf printf

#define swap_bytes_change_sign16_le swap_bytes_change_sign16
#define change_sign16_swap_bytes_le change_sign16_swap_bytes
#define change_sign16_le change_sign16

#define ulinear8_to_slinear16_le ulinear8_to_linear16_le
#define ulinear8_to_slinear16_be ulinear8_to_linear16_be
#define slinear16_to_ulinear8_le linear16_to_ulinear8_le
#define slinear16_to_ulinear8_be linear16_to_ulinear8_be

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
#define usb_callout_init(h)	timeout_set(&(h), NULL, NULL)
#define usb_callout(h, t, f, d) \
	do { \
		timeout_del(&(h)); \
		timeout_set(&(h), (f), (d)); \
		timeout_add(&(h), (t)); \
	} while (0)
#define usb_callout_pending(h)	timeout_pending(&(h))
#define usb_uncallout(h, f, d) timeout_del(&(h))

#define usb_lockmgr(l, f, sl, p) lockmgr((l), (f), (sl))

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
#define SIMPLEQ_REMOVE_HEAD	STAILQ_REMOVE_HEAD
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

