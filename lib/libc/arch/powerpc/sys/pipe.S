/* int sigsuspend(int *mask) */

#include "SYS.h"

	.text
PREFIX(pipe)
	mr	5, 3		/* keep buf addr safe */
	sc
	cmpwi	0, 0
 	beq	ok
	b	PIC_PLT(_ASM_LABEL(cerror))

ok:	stw	3, 0(5)
	stw	4, 4(5)
	li	3, 0
	blr
