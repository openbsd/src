/*	$OpenBSD: intr.h,v 1.21 2002/09/15 09:01:59 deraadt Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom, Opsycon AB and RTMX Inc, USA.
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
 *	Per Fogelstrom, Opsycon AB, Sweden for RTMX Inc, North Carolina USA.
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

#ifndef _POWERPC_INTR_H_
#define _POWERPC_INTR_H_

#define	IPL_BIO		0
#define	IPL_AUDIO	IPL_BIO /* XXX - was defined this val in audio_if.h */
#define	IPL_NET		1
#define	IPL_TTY		2
#define	IPL_IMP		3
#define	IPL_CLOCK	4
#define	IPL_NONE	5
#define	IPL_HIGH	6

#define	IST_NONE	0
#define	IST_PULSE	1
#define	IST_EDGE	2
#define	IST_LEVEL	3

#ifndef _LOCORE

#define PPC_NIRQ	65
#define PPC_CLK_IRQ	64
extern int intrcnt[PPC_NIRQ];

void setsoftclock(void);
void clearsoftclock(void);
int  splsoftclock(void);
void setsoftnet(void);
void clearsoftnet(void);
int  splsoftnet(void);

void do_pending_int(void);


volatile extern int cpl, ipending, astpending, tickspending;
extern int imask[7];

/* SPL asserts */
#define	splassert(wantipl)	/* nothing */

/*
 * Reorder protection in the following inline functions is
 * achived with an empty asm volatile statement. the compiler
 * will not move instructions past asm volatiles.
 */
volatile static __inline int
splraise(int newcpl)
{
	int oldcpl;

	__asm__ volatile("":::"memory");	/* don't reorder.... */
	oldcpl = cpl;
	cpl = oldcpl | newcpl;
	__asm__ volatile("":::"memory");	/* don't reorder.... */
	return(oldcpl);
}

volatile static __inline void
splx(int newcpl)
{
	__asm__ volatile("":::"memory");	/* reorder protect */
	cpl = newcpl;
	if(ipending & ~newcpl)
		do_pending_int();
	__asm__ volatile("":::"memory");	/* reorder protect */
}

volatile static __inline int
spllower(int newcpl)
{
	int oldcpl;

	__asm__ volatile("":::"memory");	/* reorder protect */
	oldcpl = cpl;
	cpl = newcpl;
	if(ipending & ~newcpl)
		do_pending_int();
	__asm__ volatile("":::"memory");	/* reorder protect */
	return(oldcpl);
}

/* Following code should be implemented with lwarx/stwcx to avoid
 * the disable/enable. i need to read the manual once more.... */
static __inline void
set_sint(int pending)
{
	int	msrsave;

	__asm__ ("mfmsr %0" : "=r"(msrsave));
	__asm__ volatile ("mtmsr %0" :: "r"(msrsave & ~PSL_EE));
	ipending |= pending;
	__asm__ volatile ("mtmsr %0" :: "r"(msrsave));
}

#define	SINT_CLOCK	0x10000000
#define	SINT_NET	0x20000000
#define	SINT_TTY	0x40000000
#define	SPL_CLOCK	0x80000000
#define	SINT_MASK	(SINT_CLOCK|SINT_NET|SINT_TTY)

#define splbio()	splraise(imask[IPL_BIO])
#define splnet()	splraise(imask[IPL_NET])
#define spltty()	splraise(imask[IPL_TTY])
#define splimp()	splraise(imask[IPL_IMP])
#define splaudio()	splraise(imask[IPL_AUDIO])
#define splclock()	splraise(imask[IPL_CLOCK])
#define splvm()		splraise(imask[IPL_IMP])
#define splstatclock()	splhigh()
#define	spllowersoftclock()	spllower(SINT_CLOCK)
#define	splsoftclock()	splraise(SINT_CLOCK)
#define	splsoftnet()	splraise(SINT_NET)
#define	splsofttty()	splraise(SINT_TTY)

#define	setsoftclock()	set_sint(SINT_CLOCK);
#define	setsoftnet()	set_sint(SINT_NET);
#define	setsofttty()	set_sint(SINT_TTY);

#define	splhigh()	splraise(0xffffffff)
#define	spl0()		spllower(0)

/*
 *	Interrupt control struct used to control the ICU setup.
 */

struct intrhand {
	struct	intrhand *ih_next;
	int	(*ih_fun)(void *);
	void    *ih_arg;
	u_long  ih_count;
	int     ih_level;
	int     ih_irq;
	char    *ih_what;
};
extern int ppc_configed_intr_cnt;
#define MAX_PRECONF_INTR 16
extern struct intrhand ppc_configed_intr[MAX_PRECONF_INTR];
void softnet(int isr);

#endif /* _LOCORE */


#endif /* _POWERPC_INTR_H_ */
