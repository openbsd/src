/*	$NetBSD: ubavar.h,v 1.18 1996/08/20 13:38:04 ragge Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * This file contains definitions related to the kernel structures
 * for dealing with the unibus adapters.
 *
 * Each uba has a uba_softc structure.
 * Each unibus controller which is not a device has a uba_ctlr structure.
 * Each unibus device has a uba_device structure.
 */

#include <sys/buf.h>
#include <sys/device.h>

#include <machine/trap.h> /* For struct ivec_dsp */
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
	SIMPLEQ_HEAD(, uba_unit) uh_resq;	/* resource wait chain */
	int	uh_type;		/* type of adaptor */
	struct	uba_regs *uh_uba;	/* virt addr of uba adaptor regs */
	struct	pte *uh_mr;		/* start of page map */
	int	uh_memsize;		/* size of uba memory, pages */
	caddr_t	uh_iopage;		/* start of uba io page */
	void	(**uh_reset) __P((int));/* UBA reset function array */
	int	*uh_resarg;		/* array of ubareset args */
	int	uh_resno;		/* Number of devices to reset */
	struct	ivec_dsp *uh_idsp;	/* Interrupt dispatch area */
	u_int	*uh_iarea;		/* Interrupt vector array */
	short	uh_mrwant;		/* someone is waiting for map reg */
	short	uh_bdpwant;		/* someone awaits bdp's */
	int	uh_bdpfree;		/* free bdp's */
	int	uh_zvcnt;		/* number of recent 0 vectors */
	long	uh_zvtime;		/* time over which zvcnt accumulated */
	int	uh_zvtotal;		/* total number of 0 vectors */
	int	uh_lastiv;		/* last free interrupt vector */
	short	uh_users;		/* transient bdp use count */
	short	uh_xclu;		/* an rk07 is using this uba! */
	int	uh_lastmem;		/* limit of any unibus memory */
	struct	map *uh_map;		/* register free map */
	int	(*uh_errchk) __P((struct uba_softc *));
	void	(*uh_beforescan) __P((struct uba_softc *));
	void	(*uh_afterscan) __P((struct uba_softc *));
	void	(*uh_ubainit) __P((struct uba_softc *));
	void	(*uh_ubapurge) __P((struct uba_softc *, int));
#ifdef DW780
	struct	ivec_dsp uh_dw780;	/* Interrupt handles for DW780 */
#endif
	short	uh_nr;			/* Unibus sequential number */
	short	uh_nbdp;		/* # of BDP's */
};

#define	UAMSIZ	100

/* given a pointer to uba_regs, find DWBUA registers */
/* this should be replaced with a union in uba_softc */
#define	BUA(uba)	((struct dwbua_regs *)(uba))

/*
 * Per-controller structure.
 * The unit struct is common to both the adapter and the controller
 * to which it belongs. It is only used on controllers that handles
 * BDP's, and calls the adapter queueing subroutines.
 */
struct	uba_unit {
	SIMPLEQ_ENTRY(uba_unit) uu_resq;/* Queue while waiting for resources */
	void	*uu_softc;	/* Pointer to units softc */
	int	uu_ubinfo;	/* save unibus registers, etc */
	int	uu_bdp;		/* for controllers that hang on to bdp's */
	int    (*uu_ready) __P((struct uba_unit *));
	short   uu_xclu;        /* want exclusive use of bdp's */
	short   uu_keepbdp;     /* hang on to bdp's once allocated */
};

/*
 * uba_attach_args is used during autoconfiguration. It is sent
 * from ubascan() to each (possible) device.
 */
struct uba_attach_args {
	caddr_t	ua_addr;
	    /* Pointer to int routine, filled in by probe*/
	void	(*ua_ivec) __P((int));
	    /* UBA reset routine, filled in by probe */
	void	(*ua_reset) __P((int));
	int	ua_iaddr;
	int	ua_br;
	int	ua_cvec;
};

/*
 * Flags to UBA map/bdp allocation routines
 */
#define	UBA_NEEDBDP	0x01		/* transfer needs a bdp */
#define	UBA_CANTWAIT	0x02		/* don't block me */
#define	UBA_NEED16	0x04		/* need 16 bit addresses only */
#define	UBA_HAVEBDP	0x08		/* use bdp specified in high bits */

/*
 * Macros to bust return word from map allocation routines.
 * SHOULD USE STRUCTURE TO STORE UBA RESOURCE ALLOCATION:
 */
#ifdef notyet
struct ubinfo {
	long	ub_addr;	/* unibus address: mr + boff */
	int	ub_nmr;		/* number of registers, 0 if empty */
	int	ub_bdp;		/* bdp number, 0 if none */
};
#define	UBAI_MR(i)	(((i) >> 9) & 0x7ff)	/* starting map register */
#define	UBAI_BOFF(i)	((i)&0x1ff)		/* page offset */
#else
#define	UBAI_BDP(i)	((int)(((unsigned)(i)) >> 28))
#define	BDPMASK		0xf0000000
#define	UBAI_NMR(i)	((int)((i) >> 20) & 0xff)	/* max 255 (=127.5K) */
#define	UBA_MAXNMR	255
#define	UBAI_MR(i)	((int)((i) >> 9) & 0x7ff)	/* max 2047 */
#define	UBA_MAXMR	2047
#define	UBAI_BOFF(i)	((int)((i) & 0x1ff))
#define	UBAI_ADDR(i)	((int)((i) & 0xfffff))	/* uba addr (boff+mr) */
#define	UBAI_INFO(off, mr, nmr, bdp) \
	(((bdp) << 28) | ((nmr) << 20) | ((mr) << 9) | (off))
#endif

#ifndef _LOCORE
#ifdef _KERNEL
#define	ubago(ui)	ubaqueue(ui)
#define b_forw  b_hash.le_next	/* Nice to have when handling uba queues */

extern	struct cfdriver	uba_cd;

void    ubasetvec __P((struct device *, int, void (*) __P((int))));
int	uballoc __P((struct uba_softc *, caddr_t, int, int));
void	ubarelse __P((struct uba_softc *, int *));
int	ubaqueue __P((struct uba_unit *, struct buf *));
void	ubadone __P((struct uba_unit *));
void	ubareset __P((int));

#endif /* _KERNEL */
#endif !_LOCORE
