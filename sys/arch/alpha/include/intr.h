/* $OpenBSD: intr.h,v 1.10 2001/06/24 17:05:26 miod Exp $ */
/* $NetBSD: intr.h,v 1.25 2000/05/23 05:12:56 thorpej Exp $ */

/*
 * Copyright (c) 1997 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _ALPHA_INTR_H_
#define _ALPHA_INTR_H_

#include <sys/queue.h>
#include <machine/atomic.h>

#define	IPL_NONE	0	/* disable only this interrupt */
#define	IPL_BIO		1	/* disable block I/O interrupts */
#define	IPL_NET		2	/* disable network interrupts */
#define	IPL_TTY		3	/* disable terminal interrupts */
#define	IPL_CLOCK	4	/* disable clock interrupts */
#define	IPL_HIGH	5	/* disable all interrupts */
#define	IPL_SERIAL	6	/* disable serial interrupts */

#define	IST_UNUSABLE	-1	/* interrupt cannot be used */
#define	IST_NONE	0	/* none (dummy) */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#ifdef	_KERNEL

/* IPL-lowering/restoring macros */
#define splx(s)								\
    ((s) == ALPHA_PSL_IPL_0 ? spl0() : alpha_pal_swpipl(s))
#define	spllowersoftclock()	alpha_pal_swpipl(ALPHA_PSL_IPL_SOFT)

/* IPL-raising functions/macros */
static __inline int _splraise __P((int));
static __inline int
_splraise(s)
	int s;
{
	int cur = alpha_pal_rdps() & ALPHA_PSL_IPL_MASK;
	return (s > cur ? alpha_pal_swpipl(s) : cur);
}
#define splsoft()		_splraise(ALPHA_PSL_IPL_SOFT)
#define splsoftserial()		splsoft()
#define splsoftclock()		splsoft()
#define splsoftnet()		splsoft()
#define splnet()                _splraise(ALPHA_PSL_IPL_IO)
#define splbio()                _splraise(ALPHA_PSL_IPL_IO)
#define splimp()                _splraise(ALPHA_PSL_IPL_IO)
#define spltty()                _splraise(ALPHA_PSL_IPL_IO)
#define splserial()             _splraise(ALPHA_PSL_IPL_IO)
#define	splvm()			_splraise(ALPHA_PSL_IPL_IO)
#define splclock()              _splraise(ALPHA_PSL_IPL_CLOCK)
#define splstatclock()          _splraise(ALPHA_PSL_IPL_CLOCK)
#define splhigh()               _splraise(ALPHA_PSL_IPL_HIGH)

#define spllpt()		spltty()

/*
 * simulated software interrupt register
 */
extern u_int64_t ssir;

#define	SIR_NET		0x1
#define	SIR_CLOCK	0x2
#define	SIR_SERIAL	0x4

#define	setsoft(x)	atomic_setbits_ulong(&ssir, (x))

#define	setsoftnet()	setsoft(SIR_NET)
#define	setsoftclock()	setsoft(SIR_CLOCK)
#define	setsoftserial()	setsoft(SIR_SERIAL)

/*
 * Interprocessor interrupts.  In order how we want them processed.
 */
#define	ALPHA_IPI_HALT		0x0000000000000001UL
#define	ALPHA_IPI_TBIA		0x0000000000000002UL
#define	ALPHA_IPI_TBIAP		0x0000000000000004UL
#define	ALPHA_IPI_SHOOTDOWN	0x0000000000000008UL
#define	ALPHA_IPI_IMB		0x0000000000000010UL
#define	ALPHA_IPI_AST		0x0000000000000020UL

#define	ALPHA_NIPIS		6	/* must not exceed 64 */

typedef void (*ipifunc_t) __P((void));
extern	ipifunc_t ipifuncs[ALPHA_NIPIS];

void	alpha_send_ipi __P((unsigned long, unsigned long));
void	alpha_broadcast_ipi __P((unsigned long));

/*
 * Alpha shared-interrupt-line common code.
 */

struct alpha_shared_intrhand {
	TAILQ_ENTRY(alpha_shared_intrhand)
		ih_q;
	struct alpha_shared_intr *ih_intrhead;
	int	(*ih_fn) __P((void *));
	void	*ih_arg;
	int	ih_level;
	unsigned int ih_num;
};

struct alpha_shared_intr {
	TAILQ_HEAD(,alpha_shared_intrhand)
		intr_q;
	void	*intr_private;
	int	intr_sharetype;
	int	intr_dfltsharetype;
	int	intr_nstrays;
	int	intr_maxstrays;
};

#define	ALPHA_SHARED_INTR_DISABLE(asi, num)				\
	((asi)[num].intr_maxstrays != 0 &&				\
	 (asi)[num].intr_nstrays == (asi)[num].intr_maxstrays)

struct alpha_shared_intr *alpha_shared_intr_alloc __P((unsigned int));
int	alpha_shared_intr_dispatch __P((struct alpha_shared_intr *,
	    unsigned int));
void	*alpha_shared_intr_establish __P((struct alpha_shared_intr *,
	    unsigned int, int, int, int (*)(void *), void *, const char *));
void	alpha_shared_intr_disestablish __P((struct alpha_shared_intr *,
	    void *, const char *));
int	alpha_shared_intr_get_sharetype __P((struct alpha_shared_intr *,
	    unsigned int));
int	alpha_shared_intr_isactive __P((struct alpha_shared_intr *,
	    unsigned int));
void	alpha_shared_intr_set_dfltsharetype __P((struct alpha_shared_intr *,
	    unsigned int, int));
void	alpha_shared_intr_set_maxstrays __P((struct alpha_shared_intr *,
	    unsigned int, int));
void	alpha_shared_intr_stray __P((struct alpha_shared_intr *, unsigned int,
	    const char *));
void	alpha_shared_intr_set_private __P((struct alpha_shared_intr *,
	    unsigned int, void *));
void	*alpha_shared_intr_get_private __P((struct alpha_shared_intr *,
	    unsigned int));

void	set_iointr(void (*)(void *, unsigned long));

#endif /* _KERNEL */
#endif /* ! _ALPHA_INTR_H_ */
