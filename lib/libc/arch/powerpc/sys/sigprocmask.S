/* sigprocmask(int how, const sigset_t *set, sigset_t *oset); */

#include "SYS.h"

	.text
PREFIX(sigprocmask)
	
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
	cmpwi 5, 0
	beq+ sigprocmask_done
	stw 4, 0(5)
sigprocmask_done:
	blr
