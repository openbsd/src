/* sigprocmask(int how, const sigset_t *set, sigset_t *oset); */

#include "SYS.h"

	.text
PREFIX(sigprocmask)
	stwu	1, -16(1)
	stw	5, 12(1)
	
	/* check set (new mask value) for null, in which case
	   fiddle arguments */
	cmpwi 4, 0
	bne+ load_set
	addi 3, 0, 1	/* how = SIG_BLOCK, new mask already 0 */
	b do_call
load_set:
	lwz 4, 0(4)	/* get new mask */
do_call:
	
	sc
	
	/* didnt work? */
	cmpwi 0, 0
	beq+ sigprocmask_ok
	b cerror

sigprocmask_ok:
	lwz	5, 12(1)
	cmpwi 5, 0
	beq+ sigprocmask_done
	stw 3, 0(5)
sigprocmask_done:
	addi	1, 1, 16
	blr
