/*	$OpenBSD: vsbus.h,v 1.4 2000/04/27 07:19:57 bjc Exp $ */
/*	$NetBSD: vsbus.h,v 1.9 1999/10/22 21:10:13 ragge Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Generic definitions for the (virtual) vsbus. contains common info
 * used by all VAXstations.
 */

#ifndef _VAX_VSBUS_H_
#define _VAX_VSBUS_H_

#include <machine/bus.h>

struct	vsbus_softc {
	struct	device sc_dev;
#if 0
	volatile struct vs_cpu *sc_cpu;
#endif
	u_char	*sc_intmsk;	/* Mask register */
	u_char	*sc_intclr;	/* Clear interrupt register */
	u_char	*sc_intreq;	/* Interrupt request register */
	u_char	sc_mask;	/* Interrupts to enable after autoconf */
};

struct confargs {
    char    ca_name[16];        /* device name */
    int ca_intslot;     /* device interrupt-slot */
    int ca_intpri;      /* device interrupt "priority" */
    int ca_intvec;      /* interrup-vector offset */
    int ca_intbit;      /* bit in interrupt-register */
    int ca_ioaddr;      /* device hardware I/O address */

    int ca_aux1;        /* additional info (DMA, etc.) */
    int ca_aux2;
    int ca_aux3;
    int ca_aux4;
    int ca_aux5;
    int ca_aux6;
    int ca_aux7;
    int ca_aux8;

#define ca_recvslot ca_intslot  /* DC/DZ: Receiver configuration */
#define ca_recvpri  ca_intpri
#define ca_recvvec  ca_intvec
#define ca_recvbit  ca_intbit
#define ca_xmitslot ca_aux1     /* DC/DZ: transmitter configuration */
#define ca_xmitpri  ca_aux2     /* DC/DZ:  */
#define ca_xmitvec  ca_aux3
#define ca_xmitbit  ca_aux4
#define ca_dcflags  ca_aux5

#define ca_dareg    ca_aux1     /* SCSI: DMA address register */
#define ca_dcreg    ca_aux2     /* SCSI: DMA byte count register */
#define ca_ddreg    ca_aux3     /* SCSI: DMA transfer direction */
#define ca_dbase    ca_aux4     /* SCSI: DMA buffer address */
#define ca_dsize    ca_aux5     /* SCSI: DMA buffer size */
#define ca_dflag    ca_aux6     /* SCSI: DMA flags (eg. shared) */
#define ca_idval    ca_aux7     /* SCSI: host-ID to use/set */
#define ca_idreg    ca_aux8     /* SCSI: host-ID port register */

#define ca_enaddr   ca_aux1     /* LANCE: Ethernet address in ROM */
#define ca_leflags  ca_aux2
};

struct	vsbus_attach_args {
	vaddr_t	va_addr;		/* virtual CSR address */
	paddr_t	va_paddr;		/* physical CSR address */

	void	(*va_ivec) __P((void *));	/* Interrupt routine */
	void	*va_vecarg;		/* Interrupt routine argument */

	short	va_br;			/* Interrupt level */
	short	va_cvec;		/* Interrupt vector address */
	u_char	va_maskno;		/* Interrupt vector in mask */
	bus_dma_tag_t va_dmat;
	struct 	confargs *confargs;
};

/*
 * Some chip addresses and constants, same on all VAXstations.
 */
#define VS_CFGTST	0x20020000      /* config register */
#define VS_REGS         0x20080000      /* Misc cpu internal regs */
#define NI_ADDR         0x20090000      /* Ethernet address */
#define DZ_CSR          0x200a0000      /* DZ11-compatible chip csr */
#define VS_CLOCK        0x200b0000      /* clock chip address */
#define SCA_REGS        0x200c0000      /* disk device addresses */
#define NI_BASE         0x200e0000      /* LANCE CSRs */
#define NI_IOSIZE       (128 * VAX_NBPG)    /* IO address size */

/*
 * Small monochrome graphics framebuffer, present on all machines.
 */
#define	SMADDR		0x30000000
#define	SMSIZE		0x20000		/* Actually 256k, only 128k used */

u_char	vsbus_setmask __P((unsigned char));
void	vsbus_clrintr __P((unsigned char));
void	vsbus_intr	__P((void *));
void	vsbus_copytoproc __P((struct proc *, caddr_t, caddr_t, int));
void	vsbus_copyfromproc __P((struct proc *, caddr_t, caddr_t, int));
#endif /* _VAX_VSBUS_H_ */
