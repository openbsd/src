/*	$OpenBSD: algor.h,v 1.3 1997/04/19 17:19:36 pefo Exp $ */

/*
 * Copyright (c) 1996 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_ALGOR_H_
#define	_ALGOR_H_ 1

/*
 * P-4032's Physical address space
 */

#define P4032_PHYS_MIN		0x00000000	/* 256 Meg */
#define P4032_PHYS_MAX		0x0fffffff

/*
 * Memory map
 */

#define P4032_PHYS_MEMORY_START	0x00000000
#define P4032_PHYS_MEMORY_END	0x0fffffff	/* 256 Meg in 2 slots */

/*
 * I/O map
 */

#define	P4032_V96x		0xbef00000	/* PCI Bus bridge ctrlregs */

#define	P4032_CLOCK		0xbff00000	/* RTC clock ptr reg */
#define	P4032_KEYB		0xbff10000	/* PC Keyboard controller */
#define	P4032_LED		0xbff20010	/* 4 Char LED display */
#define	P4032_LCD		0xbff30000	/* LCD option display */
#define	P4032_GPIO		0xbff40000	/* General purpose I/O */
#define	P4032_GPIO_IACK		0xbff50000	/* General purpose I/O Iack */
#define	P4032_FPY		0xbff807c0	/* Floppy controller */
#define	P4032_COM1		0xbff80fe0	/* Serial port com1 */
#define	P4032_COM2		0xbff80be0	/* Serial port com2 */
#define	P4032_CENTR		0xbff80de0	/* Centronics paralell port */
#define	P4032_IMR		0xbff90000	/* Int mask reg (wr) */
#define	P4032_IRR		0xbff90000	/* Int request reg (rd) */
#define	P4032_EIRR		0xbff90004	/* Error int request reg (rd) */
#define	P4032_ICR		0xbff90004	/* Int clear register (wr) */
#define	P4032_PCIIMR		0xbff90008	/* PCI Int mask reg (wr) */
#define	P4032_PCIIRR		0xbff90008	/* PCI Int req reg (rd) */
#define	P4032_IXR0		0xbff9000c	/* Int crossbar register 0 */
#define	P4032_IXR1		0xbff90010	/* Int crossbar register 0 */
#define	P4032_IXR2		0xbff90014	/* Int crossbar register 0 */

/*
 * Interrupt controller interrupt masks
 */

#define	P4032_IM_RTC	0x80		/* RT Clock */
#define	P4032_IM_GPIO	0x40		/* General purpose I/O */
#define	P4032_IM_CENTR	0x20		/* Centronics paralell port */
#define	P4032_IM_COM2	0x10		/* Serial port 2 */
#define	P4032_IM_COM1	0x08		/* Serial port 1 */
#define	P4032_IM_KEYB	0x04		/* PC Keyboard IFC */
#define	P4032_IM_FPY	0x02		/* Floppy disk */
#define	P4032_IM_PCI	0x01		/* PCI controller */

#define	P4032_IRR_BER	0x04		/* Bus error */
#define	P4032_IRR_PFAIL	0x02		/* Power fail */
#define	P4032_IRR_DBG	0x01		/* Debug switch */

#define	P4032_PCI_IRQ3	0x80		/* PCI interrupt request 3 */
#define	P4032_PCI_IRQ2	0x40		/* PCI interrupt request 2 */
#define	P4032_PCI_IRQ1	0x20		/* PCI interrupt request 1 */
#define	P4032_PCI_IRQ0	0x10		/* PCI interrupt request 0 */
#define	P4032_FPY_DMA	0x08		/* FPY "DMA" interrupt request */
/*
 *  Interrupt vector descriptor for device on pica bus.
 */
struct algor_int_desc {
        int             int_mask;       /* Mask used in PICA_SYS_LB_IE */
        intr_handler_t  int_hand;       /* Interrupt handler */
        void            *param;         /* Parameter to send to handler */
        int             spl_mask;       /* Spl mask for interrupt */
};

int algor_intrnull __P((void *));
void *algor_pci_intr_establish __P((int, int, intr_handler_t, void *, void *));
void algor_pci_intr_disestablish __P((void *));


#endif	/* _ALGOR_H_ */
