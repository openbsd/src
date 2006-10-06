/*	$OpenBSD: setjmp.h,v 1.1.1.1 2006/10/06 21:02:55 miod Exp $	*/
/*	$NetBSD: setjmp.h,v 1.3 2006/01/05 00:50:23 uwe Exp $	*/

/*
 * machine/setjmp.h: machine dependent setjmp-related information.
 */

#define	_JBLEN 14		/* size, in longs, of a jmp_buf */

#define _JB_REG_PR	0
#define _JB_REG_R8	1
#define _JB_REG_R9	2
#define _JB_REG_R10	3
#define _JB_REG_R11	4
#define _JB_REG_R12	5
#define _JB_REG_R13	6
#define _JB_REG_R14	7
#define _JB_REG_R15	8

#define _JB_HAS_MASK	9
#define _JB_SIGMASK	10	/* occupies sizeof(sigset_t) = 4 slots */

#define _JB_REG_SP	_JB_REG_R15
