/*	$OpenBSD: psl.h,v 1.7 1996/05/31 10:46:29 niklas Exp $	*/
/*	$NetBSD: psl.h,v 1.8 1996/04/21 21:13:22 veego Exp $	*/

#ifndef _MACHINE_PSL_H_
#define _MACHINE_PSL_H_

#include <m68k/psl.h>

#if defined(_KERNEL) && !defined(_LOCORE)
static	__inline int splraise __P((int));
static	__inline int splexact __P((int));
static	__inline void splx __P((int));
static	__inline int spllower __P((int));

static __inline int
splraise(npsl)
	register int npsl;
{
        register int opsl;

        __asm __volatile ("clrl %0; movew sr,%0" : "&=d" (opsl) : : "cc");
	if (npsl > (opsl & (PSL_S|PSL_IPL)))
        	__asm __volatile ("movew %0,sr" : : "di" (npsl) : "cc");
        return opsl;
}

static __inline int
splexact(npsl)
	register int npsl;
{
        register int opsl;

        __asm __volatile ("clrl %0; movew sr,%0; movew %1,sr" : "&=d" (opsl) :
	    "di" (npsl) : "cc");
        return opsl;
}

#if !defined(IPL_REMAP_1) && !defined(IPL_REMAP_2)
static __inline void
splx(npsl)
	register int npsl;
{
        __asm __volatile ("movew %0,sr" : : "di" (npsl) : "cc");
}
#endif

#ifdef IPL_REMAP_1
extern int isr_exter_ipl;
extern void walk_ipls __P((int, int));

static __inline void
splx(npsl)
	register int npsl;
{
/*
 * XXX This is scary as hell.  Actually removing this increases performance
 * XXX while functionality remains.  However fairness of service is altered.
 * XXX Potential lower priority services gets serviced before higher ones.
 */
	if ((isr_exter_ipl << 8) > (npsl & PSL_IPL))
		walk_ipls(isr_exter_ipl, npsl);
        __asm __volatile("movew %0,sr" : : "di" (npsl) : "cc");
}
#endif

#ifdef IPL_REMAP_2
extern int walk_ipls __P((int));

static __inline void
splx(npsl)
	register int npsl;
{
	/* We should maybe have a flag telling if this is needed.  */
	walk_ipls(npsl);
        __asm __volatile("movew %0,sr" : : "di" (npsl) : "cc");
}
#endif

static __inline int
spllower(npsl)
	register int npsl;
{
        register int opsl;

        __asm __volatile ("clrl %0; movew sr,%0" : "&=d" (opsl) : : "cc");
        splx(npsl);
        return opsl;
}

/*
 * Shortcuts.  For enhanced security use splraise instead of splexact.
 */
#define spl1()		splexact(PSL_S|PSL_IPL1)
#define spl2()		splexact(PSL_S|PSL_IPL2)
#define spl3()		splexact(PSL_S|PSL_IPL3)
#define spl4()		splexact(PSL_S|PSL_IPL4)
#define spl5()		splexact(PSL_S|PSL_IPL5)
#define spl6()		splexact(PSL_S|PSL_IPL6)
#define spl7()  	splexact(PSL_S|PSL_IPL7)

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
#define splsoftclock()	spllower(PSL_S|PSL_IPL1)
#define splsoftnet()	spl1()
#define splsofttty()	spl1()

/*
 * Miscellaneous
 */

/*
 * When remapping high interrupts down we also pull down splhigh, so that
 * the fast internal serial interrupt can get called allover.  This is safe
 * as this interrupt never goes outside of its own structures.
 */
#if defined(LEV6_DEFER) || defined(IPL_REMAP_1) || defined(IPL_REMAP_2)
#define splhigh()	spl4()
#else
#define splhigh()	spl7()
#endif
#define spl0()		spllower(PSL_S|PSL_IPL0)

#endif	/* KERNEL && !_LOCORE */
#endif	/* _MACHINE_PSL_H_ */
