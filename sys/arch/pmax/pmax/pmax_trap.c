/*	$NetBSD: pmax_trap.c,v 1.44 1997/05/31 20:33:35 mhitch Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: trap.c 1.32 91/04/06
 *
 *	@(#)trap.c	8.5 (Berkeley) 1/11/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/buf.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <net/netisr.h>

#include <machine/trap.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/mips_opcode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

/* XXX */
#include <pmax/pmax/clockreg.h>
#include <pmax/pmax/kn02.h>
#include <pmax/pmax/kmin.h>
#include <pmax/pmax/maxine.h>
#include <pmax/pmax/kn03.h>
#include <pmax/pmax/asic.h>
#include <pmax/pmax/turbochannel.h>

#include <pmax/stand/dec_prom.h>

#include <sys/socket.h>
#include <sys/mbuf.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/if_ether.h>

struct ifnet; struct ethercom;
#include <dev/ic/am7990var.h>		/* Lance interrupt for kn01 */

#include "asc.h"
#include "sii.h"
#include "le_pmax.h"
#include "dc_ds.h"
#include "led.h"

#include <sys/cdefs.h>
#include <sys/syslog.h>

#include <pmax/pmax/trap.h>

#include <machine/autoconf.h>

#ifdef DS3100
#include <pmax/pmax/kn01.h>
#include <pmax/pmax/kn01var.h>
#endif /* DS3100 */

struct	proc *machFPCurProcPtr;		/* pointer to last proc to use FP */





#ifdef DS3100
static void pmax_errintr __P((void));
#endif

static void kn02_errintr __P((void)), kn02ba_errintr __P((void));

#ifdef DS5000_240
static void kn03_errintr __P ((void));
extern u_long kn03_tc3_imask;

/*

 * IOASIC 40ns bus-cycle counter, used as hi-resolution clock:
 * may also be present on (some) XINE, 3min hardware, but not tested there.
 */
extern u_long ioasic_base;	/* Base address of I/O asic */
u_long latched_cycle_cnt;	/*
				 * IOASIC cycle counter, latched on every
				 * interrupt from RTC chip (64Hz).
				 */
#endif /*DS5000_240*/

static unsigned kn02ba_recover_erradr __P((u_int phys, u_int mer));
extern tc_option_t tc_slot_info[TC_MAX_LOGICAL_SLOTS];
extern u_long kmin_tc3_imask, xine_tc3_imask;
extern const struct callback *callv;

extern volatile struct chiptime *Mach_clock_addr;
extern u_long intrcnt[];

/*
 * Index into intrcnt[], which is defined in locore
 */
typedef enum {
	SOFTCLOCK_INTR =0,
	SOFTNET_INTR	=1,
	SERIAL0_INTR=2,
	SERIAL1_INTR = 3,
	SERIAL2_INTR = 4,
	LANCE_INTR =5,
	SCSI_INTR = 6,
	ERROR_INTR=7,
	HARDCLOCK = 8,
  	FPU_INTR   =9,
	SLOT0_INTR =10,
	SLOT1_INTR =11,
	SLOT2_INTR =12,
	DTOP_INTR = 13, /* XXX */
	ISDN_INTR = 14, /* XXX */
	FLOPPY_INTR = 15,
	STRAY_INTR = 16
} decstation_intr_t;


#ifdef DS3100

/*
 *  The pmax (3100) has no option bus. Each device is wired to
 * a separate interrupt.  For historical reasons, we call interrupt
 * routines directly, if they're enabled.
 */

#if NLE > 0
int leintr __P((void *));
#endif
#if NSII > 0
int siiintr __P((void *));
#endif
#if NDC_DS > 0
int dcintr __P((void *));
#endif

/*
 * Handle pmax (DECstation 2100/3100) interrupts.
 */
int
kn01_intr(mask, pc, statusReg, causeReg)
	unsigned mask;
	unsigned pc;
	unsigned statusReg;
	unsigned causeReg;
{
	register volatile struct chiptime *c = Mach_clock_addr;
	struct clockframe cf;
	int temp;
	extern struct cfdriver sii_cd;
	extern struct cfdriver le_cd;
	extern struct cfdriver dc_cd;

	/* handle clock interrupts ASAP */
	if (mask & MIPS_INT_MASK_3) {
		temp = c->regc;	/* XXX clear interrupt bits */
		cf.pc = pc;
		cf.sr = statusReg;
		hardclock(&cf);
		intrcnt[HARDCLOCK]++;

		/* keep clock interrupts enabled when we return */
		causeReg &= ~MIPS_INT_MASK_3;
	}

	/* If clock interrupts were enabled, re-enable them ASAP. */
	splx(MIPS_SR_INT_ENA_CUR | (statusReg & MIPS_INT_MASK_3));

#if NSII > 0
	if (mask & MIPS_INT_MASK_0) {
		intrcnt[SCSI_INTR]++;
		siiintr(sii_cd.cd_devs[0]);
	}
#endif /* NSII */

#if NLE_PMAX > 0
	if (mask & MIPS_INT_MASK_1) {
		/* 
		 * tty interrupts were disabled by the splx() call
		 * that re-enables clock interrupts.  A slip or ppp driver
		 * manipulating if queues should have called splimp(),
		 * which would mask out MIPS_INT_MASK_1.
		 */
		am7990_intr(tc_slot_info[1].sc);
		intrcnt[LANCE_INTR]++;
	}
#endif /* NLE_PMAX */

#if NDC_DS > 0
	if (mask & MIPS_INT_MASK_2) {
		dcintr(dc_cd.cd_devs[0]);
		intrcnt[SERIAL0_INTR]++;
	}
#endif /* NDC_DS */

	if (mask & MIPS_INT_MASK_4) {
		pmax_errintr();
		intrcnt[ERROR_INTR]++;
	}
	return ((statusReg & ~causeReg & MIPS_HARD_INT_MASK) |
		MIPS_SR_INT_ENA_CUR);
}
#endif	/* DS3100 */


/*
 * Handle hardware interrupts for the KN02. (DECstation 5000/200)
 * Returns spl value.
 */
int
kn02_intr(mask, pc, statusReg, causeReg)
	unsigned mask;
	unsigned pc;
	unsigned statusReg;
	unsigned causeReg;
{
	register unsigned i, m;
	register volatile struct chiptime *c = Mach_clock_addr;
	register unsigned csr;
	int temp;
	struct clockframe cf;
	static int warned = 0;

	/* handle clock interrupts ASAP */
	if (mask & MIPS_INT_MASK_1) {
		csr = *(unsigned *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CSR);
		if ((csr & KN02_CSR_PSWARN) && !warned) {
			warned = 1;
			printf("WARNING: power supply is overheating!\n");
		} else if (warned && !(csr & KN02_CSR_PSWARN)) {
			warned = 0;
			printf("WARNING: power supply is OK again\n");
		}

		temp = c->regc;	/* XXX clear interrupt bits */
		cf.pc = pc;
		cf.sr = statusReg;
		hardclock(&cf);
		intrcnt[HARDCLOCK]++;

		/* keep clock interrupts enabled when we return */
		causeReg &= ~MIPS_INT_MASK_1;
	}

	/* If clock interrups were enabled, re-enable them ASAP. */
	splx(MIPS_SR_INT_ENA_CUR | (statusReg & MIPS_INT_MASK_1));

	if (mask & MIPS_INT_MASK_0) {
		static int intr_map[8] = { SLOT0_INTR, SLOT1_INTR, SLOT2_INTR,
					   /* these two bits reserved */
					   STRAY_INTR,  STRAY_INTR,
					   SCSI_INTR, LANCE_INTR,
					   SERIAL0_INTR };

		csr = *(unsigned *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CSR);
		m = csr & (csr >> KN02_CSR_IOINTEN_SHIFT) & KN02_CSR_IOINT;
#if 0
		*(unsigned *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CSR) =
			(csr & ~(KN02_CSR_WRESERVED | 0xFF)) |
			(m << KN02_CSR_IOINTEN_SHIFT);
#endif
		for (i = 0; m; i++, m >>= 1) {
			if (!(m & 1))
				continue;
			intrcnt[intr_map[i]]++;
			if (tc_slot_info[i].intr)
				(*tc_slot_info[i].intr)(tc_slot_info[i].sc);
			else
				printf("spurious interrupt %d\n", i);
		}
#if 0
		*(unsigned *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CSR) =
			csr & ~(KN02_CSR_WRESERVED | 0xFF);
#endif
	}
	if (mask & MIPS_INT_MASK_3) {
		intrcnt[ERROR_INTR]++;
		kn02_errintr();
	}

	return ((statusReg & ~causeReg & MIPS_HARD_INT_MASK) |
		MIPS_SR_INT_ENA_CUR);
}

/*
 * 3min hardware interrupts. (DECstation 5000/1xx)
 */
int
kmin_intr(mask, pc, statusReg, causeReg)
	unsigned mask;
	unsigned pc;
	unsigned statusReg;
	unsigned causeReg;
{
	register u_int intr;
	register volatile struct chiptime *c = Mach_clock_addr;
	volatile u_int *imaskp =
		(volatile u_int *)MIPS_PHYS_TO_KSEG1(KMIN_REG_IMSK);
	volatile u_int *intrp =
		(volatile u_int *)MIPS_PHYS_TO_KSEG1(KMIN_REG_INTR);
	unsigned int old_mask;
	struct clockframe cf;
	int temp;
	static int user_warned = 0;

	old_mask = *imaskp & kmin_tc3_imask;
	*imaskp = kmin_tc3_imask |
		 (KMIN_IM0 & ~(KN03_INTR_TC_0|KN03_INTR_TC_1|KN03_INTR_TC_2));

	if (mask & MIPS_INT_MASK_4)
		(*callv->_halt)((int *)0, 0);
	if (mask & MIPS_INT_MASK_3) {
		intr = *intrp;

		/* masked interrupts are still observable */
		intr &= old_mask;
	
		if (intr & KMIN_INTR_SCSI_PTR_LOAD) {
			*intrp &= ~KMIN_INTR_SCSI_PTR_LOAD;
#ifdef notdef
			asc_dma_intr();
#endif
		}
	
		if (intr & (KMIN_INTR_SCSI_OVRUN | KMIN_INTR_SCSI_READ_E))
			*intrp &= ~(KMIN_INTR_SCSI_OVRUN | KMIN_INTR_SCSI_READ_E);

		if (intr & KMIN_INTR_LANCE_READ_E)
			*intrp &= ~KMIN_INTR_LANCE_READ_E;

		if (intr & KMIN_INTR_TIMEOUT)
			kn02ba_errintr();
	
		if (intr & KMIN_INTR_CLOCK) {
			temp = c->regc;	/* XXX clear interrupt bits */
			cf.pc = pc;
			cf.sr = statusReg;
			hardclock(&cf);
			intrcnt[HARDCLOCK]++;
		}
	
		if ((intr & KMIN_INTR_SCC_0) &&
		    tc_slot_info[KMIN_SCC0_SLOT].intr) {
			(*(tc_slot_info[KMIN_SCC0_SLOT].intr))
			  (tc_slot_info[KMIN_SCC0_SLOT].sc);
			intrcnt[SERIAL0_INTR]++;
		}

		if ((intr & KMIN_INTR_SCC_1) &&
		    tc_slot_info[KMIN_SCC1_SLOT].intr) {
			(*(tc_slot_info[KMIN_SCC1_SLOT].intr))
			  (tc_slot_info[KMIN_SCC1_SLOT].sc);
			intrcnt[SERIAL1_INTR]++;
		}
	
		if ((intr & KMIN_INTR_SCSI) &&
		    tc_slot_info[KMIN_SCSI_SLOT].intr) {
			(*(tc_slot_info[KMIN_SCSI_SLOT].intr))
			  (tc_slot_info[KMIN_SCSI_SLOT].sc);
			intrcnt[SCSI_INTR]++;
		}

		if ((intr & KMIN_INTR_LANCE) &&
		    tc_slot_info[KMIN_LANCE_SLOT].intr) {
			(*(tc_slot_info[KMIN_LANCE_SLOT].intr))
			  (tc_slot_info[KMIN_LANCE_SLOT].sc);
			intrcnt[LANCE_INTR]++;
		}
		
		if (user_warned && ((intr & KMIN_INTR_PSWARN) == 0)) {
			printf("%s\n", "Power supply ok now.");
			user_warned = 0;
		}
		if ((intr & KMIN_INTR_PSWARN) && (user_warned < 3)) {
			user_warned++;
			printf("%s\n", "Power supply overheating");
		}
	}
	if ((mask & MIPS_INT_MASK_0) && tc_slot_info[0].intr) {
		(*tc_slot_info[0].intr)(tc_slot_info[0].sc);
		intrcnt[SLOT0_INTR]++;
 	}
	
	if ((mask & MIPS_INT_MASK_1) && tc_slot_info[1].intr) {
		(*tc_slot_info[1].intr)(tc_slot_info[1].sc);
		intrcnt[SLOT1_INTR]++;
	}
	if ((mask & MIPS_INT_MASK_2) && tc_slot_info[2].intr) {
		(*tc_slot_info[2].intr)(tc_slot_info[2].sc);
		intrcnt[SLOT2_INTR]++;
	}

#if 0 /*XXX*/
	if (mask & (MIPS_INT_MASK_2|MIPS_INT_MASK_1|MIPS_INT_MASK_0))
		printf("kmin: slot intr, mask 0x%x\n",
			mask &
			(MIPS_INT_MASK_2|MIPS_INT_MASK_1|MIPS_INT_MASK_0));
#endif
	
	return ((statusReg & ~causeReg & MIPS_HARD_INT_MASK) |
		MIPS_SR_INT_ENA_CUR);
}

/*
 * Maxine hardware interrupts. (Personal DECstation 5000/xx)
 */
int
xine_intr(mask, pc, statusReg, causeReg)
	unsigned mask;
	unsigned pc;
	unsigned statusReg;
	unsigned causeReg;
{
	register u_int intr;
	register volatile struct chiptime *c = Mach_clock_addr;
	volatile u_int *imaskp = (volatile u_int *)
		MIPS_PHYS_TO_KSEG1(XINE_REG_IMSK);
	volatile u_int *intrp = (volatile u_int *)
		MIPS_PHYS_TO_KSEG1(XINE_REG_INTR);
	u_int old_mask;
	struct clockframe cf;
	int temp;

	old_mask = *imaskp & xine_tc3_imask;
	*imaskp = xine_tc3_imask;

	if (mask & MIPS_INT_MASK_4)
		(*callv->_halt)((int *)0, 0);

	/* handle clock interrupts ASAP */
	if (mask & MIPS_INT_MASK_1) {
		temp = c->regc;	/* XXX clear interrupt bits */
		cf.pc = pc;
		cf.sr = statusReg;
		hardclock(&cf);
		intrcnt[HARDCLOCK]++;
		/* keep clock interrupts enabled when we return */
		causeReg &= ~MIPS_INT_MASK_1;
	}

	/* If clock interrups were enabled, re-enable them ASAP. */
	splx(MIPS_SR_INT_ENA_CUR | (statusReg & MIPS_INT_MASK_1));

	if (mask & MIPS_INT_MASK_3) {
		intr = *intrp;
		/* masked interrupts are still observable */
		intr &= old_mask;

		if ((intr & XINE_INTR_SCC_0)) {
			if (tc_slot_info[XINE_SCC0_SLOT].intr)
				(*(tc_slot_info[XINE_SCC0_SLOT].intr))
				(tc_slot_info[XINE_SCC0_SLOT].sc);
			else
				printf ("can't handle scc interrupt\n");
			intrcnt[SERIAL0_INTR]++;
		}
	
		if (intr & XINE_INTR_SCSI_PTR_LOAD) {
			*intrp &= ~XINE_INTR_SCSI_PTR_LOAD;
#ifdef notdef
			asc_dma_intr();
#endif
		}
	
		if (intr & (XINE_INTR_SCSI_OVRUN | XINE_INTR_SCSI_READ_E))
			*intrp &= ~(XINE_INTR_SCSI_OVRUN | XINE_INTR_SCSI_READ_E);

		if (intr & XINE_INTR_LANCE_READ_E)
			*intrp &= ~XINE_INTR_LANCE_READ_E;

		if (intr & XINE_INTR_DTOP_RX) {
			if (tc_slot_info[XINE_DTOP_SLOT].intr)
				(*(tc_slot_info[XINE_DTOP_SLOT].intr))
				(tc_slot_info[XINE_DTOP_SLOT].sc);
			else
				printf ("can't handle dtop interrupt\n");
			intrcnt[DTOP_INTR]++;
		}
	
		if (intr & XINE_INTR_FLOPPY) {
			if (tc_slot_info[XINE_FLOPPY_SLOT].intr)
				(*(tc_slot_info[XINE_FLOPPY_SLOT].intr))
				(tc_slot_info[XINE_FLOPPY_SLOT].sc);
		else
			printf ("can't handle floppy interrupt\n");
			intrcnt[FLOPPY_INTR]++;
		}
	
		if (intr & XINE_INTR_TC_0) {
			if (tc_slot_info[0].intr)
				(*(tc_slot_info[0].intr))
				(tc_slot_info[0].sc);
			else
				printf ("can't handle tc0 interrupt\n");
			intrcnt[SLOT0_INTR]++;
		}
	
		if (intr & XINE_INTR_TC_1) {
			if (tc_slot_info[1].intr)
				(*(tc_slot_info[1].intr))
				(tc_slot_info[1].sc);
			else
				printf ("can't handle tc1 interrupt\n");
			intrcnt[SLOT1_INTR]++;
		}
	
		if (intr & XINE_INTR_ISDN) {
			if (tc_slot_info[XINE_ISDN_SLOT].intr)
				(*(tc_slot_info[XINE_ISDN_SLOT].intr))
				(tc_slot_info[XINE_ISDN_SLOT].sc);
			else
				printf ("can't handle isdn interrupt\n");
				intrcnt[ISDN_INTR]++;
		}
	
		if (intr & XINE_INTR_SCSI) {
			if (tc_slot_info[XINE_SCSI_SLOT].intr)
				(*(tc_slot_info[XINE_SCSI_SLOT].intr))
				(tc_slot_info[XINE_SCSI_SLOT].sc);
			else
				printf ("can't handle scsi interrupt\n");
			intrcnt[SCSI_INTR]++;
		}
	
		if (intr & XINE_INTR_LANCE) {
			if (tc_slot_info[XINE_LANCE_SLOT].intr)
				(*(tc_slot_info[XINE_LANCE_SLOT].intr))
				(tc_slot_info[XINE_LANCE_SLOT].sc);
			else
				printf ("can't handle lance interrupt\n");
	
			intrcnt[LANCE_INTR]++;
		}
	}
	if (mask & MIPS_INT_MASK_2)
		kn02ba_errintr();
	return ((statusReg & ~causeReg & MIPS_HARD_INT_MASK) |
		MIPS_SR_INT_ENA_CUR);
}

#ifdef DS5000_240
/*
 * 3Max+ hardware interrupts. (DECstation 5000/240) UNTESTED!!
 */
int
kn03_intr(mask, pc, statusReg, causeReg)
	unsigned mask;
	unsigned pc;
	unsigned statusReg;
	unsigned causeReg;
{
	register u_int intr;
	register volatile struct chiptime *c = Mach_clock_addr;
	volatile u_int *imaskp = (volatile u_int *)
		MIPS_PHYS_TO_KSEG1(KN03_REG_IMSK);
	volatile u_int *intrp = (volatile u_int *)
		MIPS_PHYS_TO_KSEG1(KN03_REG_INTR);
	u_int old_mask;
	struct clockframe cf;
	int temp;
	static int user_warned = 0;
	register u_long old_buscycle = latched_cycle_cnt;

	old_mask = *imaskp & kn03_tc3_imask;
	*imaskp = kn03_tc3_imask;

	if (mask & MIPS_INT_MASK_4)
		(*callv->_halt)((int *)0, 0);

	/* handle clock interrupts ASAP */
	if (mask & MIPS_INT_MASK_1) {
		temp = c->regc;	/* XXX clear interrupt bits */
		cf.pc = pc;
		cf.sr = statusReg;
		latched_cycle_cnt = *(u_long*)(IOASIC_REG_CTR(ioasic_base));
		hardclock(&cf);
		intrcnt[HARDCLOCK]++;
		old_buscycle = latched_cycle_cnt - old_buscycle;
		/* keep clock interrupts enabled when we return */
		causeReg &= ~MIPS_INT_MASK_1;
	}

	/* If clock interrups were enabled, re-enable them ASAP. */
	splx(MIPS_SR_INT_ENA_CUR | (statusReg & MIPS_INT_MASK_1));

	/*
	 * Check for late clock interrupts (allow 10% slop). Be careful
	 * to do so only after calling hardclock(), due to logging cost.
	 * Even then, logging dropped ticks just causes more clock
	 * ticks to be missed.
	 */
#ifdef notdef
	if ((mask & MIPS_INT_MASK_1) && old_buscycle > (tick+49) * 25) {
		extern int msgbufmapped;
  		if(msgbufmapped && 0)
			 addlog("kn03: clock intr %d usec late\n",
				 old_buscycle/25);
	}
#endif
	/*
	 * IOCTL asic DMA-related interrupts should be checked here,
	 * and DMA pointers serviced as soon as possible.
	 */

	if (mask & MIPS_INT_MASK_0) {
		intr = *intrp;
		/* masked interrupts are still observable */
		intr &= old_mask;

		if (intr & KN03_INTR_SCSI_PTR_LOAD) {
			*intrp &= ~KN03_INTR_SCSI_PTR_LOAD;
#ifdef notdef
			asc_dma_intr();
#endif
		}
	
	/*
	 * XXX
	 * DMA and non-DMA  interrupts from the IOCTl asic all use the
	 * single interrupt request line from the IOCTL asic.
	 * Disabling IOASIC interrupts while servicing network or
	 * disk-driver interrupts causes DMA overruns. NON-dma IOASIC
	 * interrupts should be disabled in the ioasic, and
	 * interrupts from the IOASIC itself should be re-enabled.
	 * DMA interrupts can then be serviced whilst still servicing
	 * non-DMA interrupts from ioctl devices or TC options.
	 */

		if (intr & (KN03_INTR_SCSI_OVRUN | KN03_INTR_SCSI_READ_E))
			*intrp &= ~(KN03_INTR_SCSI_OVRUN | KN03_INTR_SCSI_READ_E);

		if (intr & KN03_INTR_LANCE_READ_E)
			*intrp &= ~KN03_INTR_LANCE_READ_E;

		if ((intr & KN03_INTR_SCC_0) &&
			tc_slot_info[KN03_SCC0_SLOT].intr) {
			(*(tc_slot_info[KN03_SCC0_SLOT].intr))
			(tc_slot_info[KN03_SCC0_SLOT].sc);
			intrcnt[SERIAL0_INTR]++;
		}
	
		if ((intr & KN03_INTR_SCC_1) &&
			tc_slot_info[KN03_SCC1_SLOT].intr) {
			(*(tc_slot_info[KN03_SCC1_SLOT].intr))
			(tc_slot_info[KN03_SCC1_SLOT].sc);
			intrcnt[SERIAL1_INTR]++;
		}
	
		if ((intr & KN03_INTR_TC_0) &&
			tc_slot_info[0].intr) {
			(*(tc_slot_info[0].intr))
			(tc_slot_info[0].sc);
			intrcnt[SLOT0_INTR]++;
		}
#ifdef DIAGNOSTIC
		else if (intr & KN03_INTR_TC_0)
			printf ("can't handle tc0 interrupt\n");
#endif /*DIAGNOSTIC*/

		if ((intr & KN03_INTR_TC_1) &&
			tc_slot_info[1].intr) {
			(*(tc_slot_info[1].intr))
			(tc_slot_info[1].sc);
			intrcnt[SLOT1_INTR]++;
		}
#ifdef DIAGNOSTIC
		else if (intr & KN03_INTR_TC_1)
			printf ("can't handle tc1 interrupt\n");
#endif /*DIAGNOSTIC*/

		if ((intr & KN03_INTR_TC_2) &&
			tc_slot_info[2].intr) {
			(*(tc_slot_info[2].intr))
			(tc_slot_info[2].sc);
			intrcnt[SLOT2_INTR]++;
		}
#ifdef DIAGNOSTIC
		else if (intr & KN03_INTR_TC_2)
			printf ("can't handle tc2 interrupt\n");
#endif /*DIAGNOSTIC*/
	
		if ((intr & KN03_INTR_SCSI) &&
			tc_slot_info[KN03_SCSI_SLOT].intr) {
			(*(tc_slot_info[KN03_SCSI_SLOT].intr))
			(tc_slot_info[KN03_SCSI_SLOT].sc);
			intrcnt[SCSI_INTR]++;
		}
	
		if ((intr & KN03_INTR_LANCE) &&
			tc_slot_info[KN03_LANCE_SLOT].intr) {
			(*(tc_slot_info[KN03_LANCE_SLOT].intr))
			(tc_slot_info[KN03_LANCE_SLOT].sc);
			intrcnt[LANCE_INTR]++;
		}
	
		if (user_warned && ((intr & KN03_INTR_PSWARN) == 0)) {
			printf("%s\n", "Power supply ok now.");
			user_warned = 0;
		}
		if ((intr & KN03_INTR_PSWARN) && (user_warned < 3)) {
			user_warned++;
			printf("%s\n", "Power supply overheating");
		}
	}
	if (mask & MIPS_INT_MASK_3)
		kn03_errintr();
	return ((statusReg & ~causeReg & MIPS_HARD_INT_MASK) |
		MIPS_SR_INT_ENA_CUR);
}
#endif /* DS5000_240 */


/*
 *----------------------------------------------------------------------
 *
 * MemErrorInterrupts --
 *   pmax_errintr - for the DS2100/DS3100
 *   kn02_errintr - for the DS5000/200
 *   kn02ba_errintr - for the DS5000/1xx and DS5000/xx
 *
 *	Handler an interrupt for the control register.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
#ifdef DS3100

#if (NLED > 0)
extern unsigned short led_current;
#endif

static void
pmax_errintr()
{
	volatile u_short *sysCSRPtr =
		(u_short *)MIPS_PHYS_TO_KSEG1(KN01_SYS_CSR);
	u_short csr;

	csr = *sysCSRPtr;

	if (csr & KN01_CSR_MERR) {
		printf("Memory error at 0x%x\n",
			*(unsigned *)MIPS_PHYS_TO_KSEG1(KN01_SYS_ERRADR));
		panic("Mem error interrupt");
	}
#if (NLED > 0)
	*sysCSRPtr = ((csr & ~KN01_CSR_MBZ) & ~(KN01_CSR_LEDS_MASK))
			| led_current;
#else
	*sysCSRPtr = (csr & ~KN01_CSR_MBZ) | 0xff;
#endif
}
#endif /* DS3100 */

static void
kn02_errintr()
{
	u_int erradr, chksyn, physadr;

	erradr = *(u_int *)MIPS_PHYS_TO_KSEG1(KN02_SYS_ERRADR);
	chksyn = *(u_int *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CHKSYN);
	*(u_int *)MIPS_PHYS_TO_KSEG1(KN02_SYS_ERRADR) = 0;
	wbflush();

	if (!(erradr & KN02_ERR_VALID))
		return;
	/* extract the physical word address and compensate for pipelining */
	physadr = erradr & KN02_ERR_ADDRESS;
	if (!(erradr & KN02_ERR_WRITE))
		physadr = (physadr & ~0xfff) | ((physadr & 0xfff) - 5);
	physadr <<= 2;
	printf("%s memory %s %s error at 0x%08x\n",
		(erradr & KN02_ERR_CPU) ? "CPU" : "DMA",
		(erradr & KN02_ERR_WRITE) ? "write" : "read",
		(erradr & KN02_ERR_ECCERR) ? "ECC" : "timeout",
		physadr);
	if (erradr & KN02_ERR_ECCERR) {
		*(u_int *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CHKSYN) = 0;
		wbflush();
		printf("ECC 0x%08x\n", chksyn);

		/* check for a corrected, single bit, read error */
		if (!(erradr & KN02_ERR_WRITE)) {
			if (physadr & 0x4) {
				/* check high word */
				if (chksyn & KN02_ECC_SNGHI)
					return;
			} else {
				/* check low word */
				if (chksyn & KN02_ECC_SNGLO)
					return;
			}
		}
	}
	panic("Mem error interrupt");
}

#ifdef DS5000_240
static void
kn03_errintr()
{
	u_int erradr, errsyn, physadr;

	erradr = *(u_int *)MIPS_PHYS_TO_KSEG1(KN03_SYS_ERRADR);
	errsyn = *(u_int *)MIPS_PHYS_TO_KSEG1(KN03_SYS_ERRSYN);
	*(u_int *)MIPS_PHYS_TO_KSEG1(KN03_SYS_ERRADR) = 0;
	wbflush();

	if (!(erradr & KN03_ERR_VALID))
		return;
	/* extract the physical word address and compensate for pipelining */
	physadr = erradr & KN03_ERR_ADDRESS;
	if (!(erradr & KN03_ERR_WRITE))
		physadr = (physadr & ~0xfff) | ((physadr & 0xfff) - 5);
	physadr <<= 2;
	printf("%s memory %s %s error at 0x%08x",
		(erradr & KN03_ERR_CPU) ? "CPU" : "DMA",
		(erradr & KN03_ERR_WRITE) ? "write" : "read",
		(erradr & KN03_ERR_ECCERR) ? "ECC" : "timeout",
		physadr);
	if (erradr & KN03_ERR_ECCERR) {
		*(u_int *)MIPS_PHYS_TO_KSEG1(KN03_SYS_ERRSYN) = 0;
		wbflush();
		printf("   ECC 0x%08x\n", errsyn);

		/* check for a corrected, single bit, read error */
		if (!(erradr & KN03_ERR_WRITE)) {
			if (physadr & 0x4) {
				/* check high word */
				if (errsyn & KN03_ECC_SNGHI)
					return;
			} else {
				/* check low word */
				if (errsyn & KN03_ECC_SNGLO)
					return;
			}
		}
		printf("\n");
	}
	else
		printf("\n");
	printf("panic(\"Mem error interrupt\");\n");
}
#endif /* DS5000_240 */

static void
kn02ba_errintr()
{
	register int mer, adr, siz;
	static int errintr_cnt = 0;

	siz = *(volatile int *)MIPS_PHYS_TO_KSEG1(KMIN_REG_MSR);
	mer = *(volatile int *)MIPS_PHYS_TO_KSEG1(KMIN_REG_MER);
	adr = *(volatile int *)MIPS_PHYS_TO_KSEG1(KMIN_REG_AER);

	/* clear interrupt bit */
	*(unsigned int *)MIPS_PHYS_TO_KSEG1(KMIN_REG_TIMEOUT) = 0;

	errintr_cnt++;
	printf("(%d)%s%x [%x %x %x]\n", errintr_cnt,
	       "Bad memory chip at phys ",
	       kn02ba_recover_erradr(adr, mer),
	       mer, siz, adr);
}

static unsigned
kn02ba_recover_erradr(phys, mer)
	register unsigned phys, mer;
{
	/* phys holds bits 28:2, mer knows which byte */
	switch (mer & KMIN_MER_LASTBYTE) {
	case KMIN_LASTB31:
		mer = 3; break;
	case KMIN_LASTB23:
		mer = 2; break;
	case KMIN_LASTB15:
		mer = 1; break;
	case KMIN_LASTB07:
		mer = 0; break;
	}
	return ((phys & KMIN_AER_ADDR_MASK) | mer);
}
