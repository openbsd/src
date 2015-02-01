/*	$OpenBSD: ubavar.h,v 1.9 2015/02/01 15:27:11 miod Exp $	*/
/*	$NetBSD: ubavar.h,v 1.31 2001/04/26 19:16:07 ragge Exp $	*/

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ubavar.h	7.7 (Berkeley) 6/28/90
 */
#ifndef _QBUS_UBAVAR_H
#define	_QBUS_UBAVAR_H

/*
 * This file contains definitions related to the kernel structures
 * for dealing with the unibus adapters.
 *
 * Each uba has a uba_softc structure.
 * Each unibus controller which is not a device has a uba_ctlr structure.
 * Each unibus device has a uba_device structure.
 */

/*
 * Per-uba structure.
 *
 * This structure holds the interrupt vector for the uba,
 * and its address in physical and virtual space.  At boot time
 * we determine the devices attached to the uba's and their
 * interrupt vectors, filling in uh_vec.  We free the map
 * register and bdp resources of the uba into the structures
 * defined here.
 *
 * During normal operation, resources are allocated and returned
 * to the structures here.  We watch the number of passive releases
 * on each uba, and if the number is excessive may reset the uba.
 * 
 * When uba resources are needed and not available, or if a device
 * which can tolerate no other uba activity (rk07) gets on the bus,
 * then device drivers may have to wait to get to the bus and are
 * queued here.  It is also possible for processes to block in
 * the unibus driver in resource wait (mrwant, bdpwant); these
 * wait states are also recorded here.
 */
struct	uba_softc {
	struct	device uh_dev;		/* Device struct, autoconfig */
	struct	evcount uh_intrcnt;		/* interrupt counting */
	SIMPLEQ_HEAD(, uba_unit) uh_resq;	/* resource wait chain */
	SIMPLEQ_HEAD(, uba_reset) uh_resetq;	/* ubareset queue */
	int	uh_lastiv;		/* last free interrupt vector */
	int	(*uh_errchk)(struct uba_softc *);
	void	(*uh_beforescan)(struct uba_softc *);
	void	(*uh_afterscan)(struct uba_softc *);
	void	(*uh_ubainit)(struct uba_softc *);
	void	(*uh_ubapurge)(struct uba_softc *, int);
	short	uh_nr;			/* Unibus sequential number */
	bus_space_tag_t	uh_iot;		/* Tag for this Unibus */
	bus_space_handle_t uh_ioh;	/* Handle for I/O space */
	bus_dma_tag_t	uh_dmat;
};

/*
 * Per-controller structure.
 * The unit struct is common to both the adapter and the controller
 * to which it belongs. It is only used on controllers that handles
 * BDP's, and calls the adapter queueing subroutines.
 */
struct	uba_unit {
	SIMPLEQ_ENTRY(uba_unit) uu_resq;/* Queue while waiting for resources */
	void	*uu_softc;	/* Pointer to units softc */
	int	uu_bdp;		/* for controllers that hang on to bdp's */
	int    (*uu_ready)(struct uba_unit *);
	void	*uu_ref;	/* Buffer this is related to */
	short   uu_xclu;        /* want exclusive use of bdp's */
};

/*
 * Reset structure. All devices that needs to be reinitialized
 * after an ubareset registers with this struct.
 */
struct	uba_reset {
	SIMPLEQ_ENTRY(uba_reset) ur_resetq;
	void (*ur_reset)(struct device *);
	struct device *ur_dev;
};

/*
 * uba_attach_args is used during autoconfiguration. It is sent
 * from ubascan() to each (possible) device.
 */
struct uba_attach_args {
	bus_space_tag_t	ua_iot;		/* Tag for this bus I/O-space */
	bus_addr_t	ua_ioh;		/* I/O regs addr */
	bus_dma_tag_t	ua_dmat;
	void		*ua_icookie;	/* Cookie for interrupt establish */
	int		ua_iaddr;	/* Full CSR address of device */
	int		ua_br;		/* IPL this dev interrupted on */
	int		ua_cvec;	/* Vector for this device */
};

/*
 * Flags to UBA map/bdp allocation routines
 */
#define	UBA_NEEDBDP	0x01		/* transfer needs a bdp */
#define	UBA_CANTWAIT	0x02		/* don't block me */
#define	UBA_NEED16	0x04		/* need 16 bit addresses only */
#define	UBA_HAVEBDP	0x08		/* use bdp specified in high bits */
#define	UBA_DONTQUE	0x10		/* Do not enqueue xfer */

/*
 * Struct for unibus allocation.
 */
struct ubinfo {
	bus_dmamap_t ui_dmam;
	bus_dma_segment_t ui_seg;
	int ui_rseg;
	caddr_t ui_vaddr;
	bus_addr_t ui_baddr;
	bus_size_t ui_size;
};

/*
 * Some common defines for all subtypes of U/Q-buses/adapters.
 */
#define ubdevreg(addr) ((addr) & 017777)

#ifdef _KERNEL
void uba_intr_establish(void *, int, void (*)(void *), void *, struct evcount *);
void uba_reset_establish(void (*)(struct device *), struct device *);
void uba_attach(struct uba_softc *, unsigned long);
void uba_enqueue(struct uba_unit *);
void uba_done(struct uba_softc *);
void ubareset(struct uba_softc *);
int uballoc(struct uba_softc *, struct ubinfo *, int);
int ubmemalloc(struct uba_softc *, struct ubinfo *, int);
void ubfree(struct uba_softc *, struct ubinfo *);
void ubmemfree(struct uba_softc *, struct ubinfo *);
#endif /* _KERNEL */

#endif /* _QBUS_UBAVAR_H */
