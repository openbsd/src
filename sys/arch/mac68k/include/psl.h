/*	$OpenBSD: psl.h,v 1.4 1996/05/26 18:36:00 briggs Exp $	*/
/*	$NetBSD: psl.h,v 1.9 1996/05/19 04:30:32 briggs Exp $	*/

#ifndef PSL_C
#include <m68k/psl.h>

#if defined(_KERNEL) && !defined(_LOCORE)
/*
 * spl functions; all but spl0 are done in-line
 */

#define _spl(s) \
({ \
        register int _spl_r; \
\
        __asm __volatile ("clrl %0; movew sr,%0; movew %1,sr" : \
                "&=d" (_spl_r) : "di" (s)); \
        _spl_r; \
})

/* spl0 requires checking for software interrupts */
#define spl1()  _spl(PSL_S|PSL_IPL1)
#define spl2()  _spl(PSL_S|PSL_IPL2)
#define spl3()  _spl(PSL_S|PSL_IPL3)
#define spl4()  _spl(PSL_S|PSL_IPL4)
#define spl5()  _spl(PSL_S|PSL_IPL5)
#define spl6()  _spl(PSL_S|PSL_IPL6)
#define spl7()  _spl(PSL_S|PSL_IPL7)

/*
 * These should be used for:
 * 1) ensuring mutual exclusion (why use processor level?)
 * 2) allowing faster devices to take priority
 *
 * Note that on the Mac, most things are masked at spl1, almost
 * everything at spl2, and everything but the panic switch and
 * power at spl4.
 */
#define	splsoftclock()	spl1()	/* disallow softclock */
#define	splsoftnet()	spl1()	/* disallow network */
#define	spltty()	spl1()	/* disallow tty (softserial & ADB) */
#define	splbio()	spl2()	/* disallow block I/O */
#define	splnet()	spl2()	/* disallow network */
#define	splimp()	spl2()	/* disallow goblins and other nonsense */
#define	splclock()	spl2()	/* disallow clock (and other) interrupts */
#define	splstatclock()	spl2()	/* ditto */
#define	splzs()		spl4()	/* disallow serial hw interrupts */
#define	splhigh()	spl7()	/* disallow everything */
#define	splsched()	spl7()	/* disallow scheduling */

/* watch out for side effects */
#define splx(s)         ((s) & PSL_IPL ? _spl(s) : spl0())

int	spl0 __P((void));

#endif /* _KERNEL && !_LOCORE */

#endif /* ndef PSL_C */
