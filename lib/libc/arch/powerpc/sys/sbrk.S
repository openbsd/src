#include "SYS.h"

	.data
	.globl _end
	.globl curbrk
	.globl minbrk
_ASM_LABEL(curbrk):
	.long _C_LABEL(end)
_ASM_LABEL(minbrk):
	.long _C_LABEL(end)


	.text
PREFIX2(sbrk,break)
	
	/* call break(curbrk + size) */
#ifndef PIC
	addis	6, 0, curbrk@H
	ori	6, 6, curbrk@L	/* # 6 = &curbrk */
#else
	mflr	10
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	4
	mtlr	10
	lwz	6,_ASM_LABEL(curbrk)@got(9)
#endif
	lwz	5, 0(6)		/* # 5 = *6 (old_curbrk) */
	add	3, 5, 3		/* # 3 = new_curbrk */
	mr	7, 3

	sc

	/* check for error */
	cmpwi	0, 0
	beq+	.L_sbrk_ok
	b	PIC_PLT(_ASM_LABEL(cerror))

	/* update, curbrk and return */
.L_sbrk_ok:
	stw	7, 0(6)		/* # remember, 6=&curbrk, 7=new_curbrk */
	mr	3, 5		/* # remember, 5=old_curbrk */
	blr
