/*	$OpenBSD: psl.h,v 1.4 1996/05/02 06:44:46 niklas Exp $	*/
/*	$NetBSD: psl.h,v 1.8 1996/04/21 21:13:22 veego Exp $	*/

#ifndef _MACHINE_PSL_H_
#define _MACHINE_PSL_H_

#include <m68k/psl.h>

#if defined(_KERNEL) && !defined(_LOCORE)

/*
 * spl functions; all are normally done in-line
 */
#include <machine/psl.h>

#ifdef _KERNEL

static __inline int
splraise(npsl)
	register int npsl;
{
        register int opsl;

        __asm __volatile ("clrl %0; movew sr,%0; movew %1,sr" : "&=d" (opsl) :
	    "di" (npsl));
        return opsl;
}

#ifdef IPL_REMAP_1

extern int isr_exter_ipl;
extern void walk_ipls __P((int, int));

static __inline int
splx(npsl)
	register int npsl;
{
        register int opsl;

        __asm __volatile ("clrl %0; movew sr,%0" : "=d" (opsl));
	if ((isr_exter_ipl << 8) > (npsl & PSL_IPL))
		walk_ipls(isr_exter_ipl, npsl);
        __asm __volatile("movew %0,sr" : : "di" (npsl));
        return opsl;
}
#endif

#ifndef IPL_REMAP_2
#define splx splraise
#else

extern int walk_ipls __P((int));

static __inline int
splx(npsl)
	register int npsl;
{
        register int opsl;

	/* We should maybe have a flag telling if this is needed.  */
	opsl = walk_ipls(npsl);
        __asm __volatile("movew %0,sr" : : "di" (npsl));
        return opsl;
}

#endif

/*
 * Shortcuts
 */
#define spl1()	splraise(PSL_S|PSL_IPL1)
#define spl2()	splraise(PSL_S|PSL_IPL2)
#define spl3()	splraise(PSL_S|PSL_IPL3)
#define spl4()	splraise(PSL_S|PSL_IPL4)
#define spl5()	splraise(PSL_S|PSL_IPL5)
#define spl6()	splraise(PSL_S|PSL_IPL6)
#define spl7()  splraise(PSL_S|PSL_IPL7)

/*
 * Hardware interrupt masks
 */
#define splbio()	spl3()
#define splnet()	spl3()
#define spltty()	spl4()
#define splimp()	spl4()
#if defined(LEV6_DEFER) || defined(IPL_REMAP_1) || defined(IPL_REMAP_2)
#define splclock()	spl4()
#else
#define splclock()	spl6()
#endif
#define splstatclock()	splclock()

/*
 * Software interrupt masks
 *
 * NOTE: splsoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */
#define splsoftclock()	splx(PSL_S|PSL_IPL1)
#define splsoftnet()	spl1()
#define splsofttty()	spl1()

/*
 * Miscellaneous
 */
#if defined(LEV6_DEFER) || defined(IPL_REMAP_1) || defined(IPL_REMAP_2)
#define splhigh()	spl4()
#else
#define splhigh()	spl7()
#endif
#define spl0()	splx(PSL_S|PSL_IPL0)

#endif	/* KERNEL && !_LOCORE */
#endif	/* _MACHINE_PSL_H_ */
