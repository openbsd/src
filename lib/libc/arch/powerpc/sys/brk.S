#include "SYS.h"

	.extern curbrk
	.extern _C_LABEL(end)

PREFIX2(brk,break)
	
	/* check >= _end, if not make the call for _end */
#ifndef PIC
	addis 5,0,_C_LABEL(end)@h
	ori  5,5,_C_LABEL(end)@l		/* # 5 = &_end */
#else
	mflr	10
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	9
	mtlr	10
	lwz	5,_C_LABEL(end)@got(9)
#endif
	cmpw	3,5
	bge+	.L_brk_call
	mr	3, 5

.L_brk_call:
	mr	7, 3
	/* call break(size) */
#ifndef PIC
	addis	6, 0, curbrk@H
	ori	6, 6, curbrk@L	/* # 6 = &curbrk */
#else
	lwz	6,_ASM_LABEL(curbrk)@got(9)
#endif

	sc

	/* check for error */
	cmpwi	0, 0
	beq+	.L_brk_ok	/* OK so this is stupid but I haven't read b */
	b	PIC_PLT(_ASM_LABEL(cerror))

	/* update, curbrk and return */
.L_brk_ok:
	stw	7, 0(6)		/* # remember, 6=&curbrk, 3= new value */
	mr	3, 0		/* # return 0 */
	blr
