/*	$Id: mcreg.h,v 1.2 1995/11/07 08:49:10 deraadt Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *      This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * VME162 MCchip
 */
struct mcreg {
	volatile u_char		mc_chipid;
	volatile u_char		mc_chiprev;
	volatile u_char		mc_genctl;
	volatile u_char		mc_vecbase;
	volatile u_long		mc_t1cmp;
	volatile u_long		mc_t1count;
	volatile u_long		mc_t2cmp;
	volatile u_long		mc_t2count;
	volatile u_char		mc_lsbprescale;
	volatile u_char		mc_adjprescale;
	volatile u_char		mc_t2ctl;
	volatile u_char		mc_t1ctl;
	volatile u_char		mc_t4irq;
	volatile u_char		mc_t3irq;
	volatile u_char		mc_t2irq;
	volatile u_char		mc_t1irq;
	volatile u_char		mc_parity;
	volatile u_char		mc_zsirq;
	volatile u_char		mc_t4ctl;
	volatile u_char		mc_t3ctl;
	volatile u_short	mc_drambase;
	volatile u_short	mc_srambase;
	volatile u_char		mc_dramsize;
	volatile u_char		mc_memoptions;
#define MC_MEMOPTIONS_SRAMMASK	0x18
#define MC_MEMOPTIONS_SRAM128K	0x00
#define MC_MEMOPTIONS_SRAM512K	0x08
#define MC_MEMOPTIONS_SRAM1M	0x10
#define MC_MEMOPTIONS_SRAM2M	0x18
#define MC_MEMOPTIONS_DRAMMASK	0x07
#define MC_MEMOPTIONS_DRAM1M	0x00
#define MC_MEMOPTIONS_DRAM2M	0x01
#define MC_MEMOPTIONS_DRAM4M	0x03
#define MC_MEMOPTIONS_DRAM4M2	0x04
#define MC_MEMOPTIONS_DRAM8M	0x05
#define MC_MEMOPTIONS_DRAM16M	0x07
	volatile u_char		mc_sramsize;
	volatile u_char		mc_resv1;
	volatile u_char		mc_ieerr;
	volatile u_char		mc_resv2;
	volatile u_char		mc_ieirq;
	volatile u_char		mc_iefailirq;
	volatile u_char		mc_ncrerr;
	volatile u_char		mc_input;
	volatile u_char		mc_ver;
	volatile u_char		mc_ncrirq;
	volatile u_long		mc_t3cmp;
	volatile u_long		mc_t3count;
	volatile u_long		mc_t4cmp;
	volatile u_long		mc_t4count;
	volatile u_char		mc_busclock;
	volatile u_char		mc_promtime;
	volatile u_char		mc_flashctl;
	volatile u_char		mc_abortirq;
	volatile u_char		mc_resetctl;
	volatile u_char		mc_watchdogctl;
	volatile u_char		mc_watchdogtime;
	volatile u_char		mc_resv3;
	volatile u_char		mc_dramctl;
	volatile u_char		mc_resv4;
	volatile u_char		mc_mpustat;
	volatile u_char		mc_resv5;
	volatile u_long		mc_prescale;
};
#define MC_MCCHIP_OFF		0x42000
#define MC_CHIPID		0x84

/*
 * points to system's MCchip registers
 */
extern struct mcreg *sys_mc;

/*
 * for the console we need zs phys addr
 */
#define ZS0_PHYS_162	(0xfff45000)
#define ZS1_PHYS_162	(0xfff45801)

/*
 * We lock off our interrupt vector at 0x50.
 */
#define MC_VECBASE	0x50
#define MC_NVEC		16

#define MCV_ZS		0x00
#define MCV_TIMER4	0x03
#define MCV_TIMER3	0x04
#define MCV_NCR		0x05
#define MCV_IEFAIL	0x06
#define MCV_IE		0x07
#define MCV_TIMER2	0x08
#define MCV_TIMER1	0x09
#define MCV_PARITY	0x0b
#define MCV_ABORT	0x0e

#define MC_TCTL_CEN	0x01
#define MC_TCTL_COC	0x02
#define MC_TCTL_COVF	0x04
#define MC_TCTL_OVF	0xf0

#define	MC_ABORT_ABS	0x40

#define mc_timer_us2lim(us)	(us)		/* timer increments in "us" */

#define MC_IRQ_IPL	0x07
#define MC_IRQ_ICLR	0x08
#define MC_IRQ_IEN	0x10
#define MC_IRQ_INT	0x20

#define MC_GENCTL_IEN	0x02

#define MC_IEERR_SCLR	0x01

#define MC_SC_INHIBIT	(0 << 6)
#define MC_SC_SNOOP	(1 << 6)
#define MC_SC_INVAL	(2 << 6)
#define MC_SC_RESV	(3 << 6)

#define MC_VER_ISLX	0x40
#define MC_VER_REAL040	0x10
#define MC_VER_NOIE	0x08
#define MC_VER_NONCR	0x04
#define MC_VER_NOVME	0x02
#define MC_VER_33MHZ	0x01

void mc_enableflashwrite __P((int on));
#define MC_ENAFLASHWRITE_OFFSET	0xcc000
#define MC_DISFLASHWRITE_OFFSET	0xc8000
