/* sigprocmask(int how, const sigset_t *set, sigset_t *oset); */

#include "SYS.h"

	.text
PREFIX(sigprocmask)
	stwu	1, -16(1)
	stw	5, 12(1)
	
	/* check set (new mask value) for null, in which case
	   fiddle arguments */
	cmpwi	4, 0
	bne+	.L_load_set
	addi	3, 0, 1	/* how = SIG_BLOCK, new mask already 0 */
	b .L_do_call
.L_load_set:
	lwz	4, 0(4)	/* get new mask */
.L_do_call:
	
	sc
	
	/* didnt work? */
	cmpwi	0, 0
	beq+	.L_sigprocmask_ok
	b	PIC_PLT(_ASM_LABEL(cerror))

.L_sigprocmask_ok:
	lwz	5, 12(1)
	cmpwi	5, 0
	beq+	.L_sigprocmask_done
	stw	3, 0(5)
.L_sigprocmask_done:
	addi	1, 1, 16
	blr
