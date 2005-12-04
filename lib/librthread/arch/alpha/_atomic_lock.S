/*	$OpenBSD: _atomic_lock.S,v 1.1 2005/12/04 05:47:38 brad Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

#include <machine/asm.h>

LEAF(_atomic_lock,1)
        LDGP(pv)

				/* NOTE: using ldl_l/stl_c instead of
				   ldq_l and ldq_c as machine/spinlock.h
				   defines _spinlock_lock_t as int */
0:      ldl_l   v0, 0(a0)       /* read existing lock value */
        mov     1, t0           /* locked value to store */
        stl_c   t0, 0(a0)       /* attempt to store, status in t0 */
        beq     t0, 1f          /* branch forward to optimise prediction */
        mb                      /* sync with other processors */
        RET                     /* return with v0==0 if lock obtained */
1:      br      0b              /* loop to try again */
END(_atomic_lock)
