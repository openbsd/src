#include "SYS.h"

	.data
	.globl _end
	.globl curbrk
	.globl minbrk
curbrk:	.long _end
minbrk:	.long _end


	.text
PSEUDO_PREFIX(sbrk,break)
	
	/* call break(curbrk + size) */
	addis	6,0,curbrk@h
	ori	6,6,curbrk@l	/* # 6 = &curbrk */
	lwz	5, 0(6)		/* # 5 = *6 (old_curbrk) */
	add	3, 5, 3		/* # 3 = new_curbrk */
	mr	7, 3

	sc

	/* check for error */
	cmpwi 0, 0
	beq+ sbrk_ok		/* OK so this is stupid but I haven't read b */
	b cerror

	/* update, curbrk and return */
sbrk_ok:
	stw	7, 0(6)		/* # remember, 6=&curbrk, 7=new_curbrk */
	mr	3, 5		/* # remember, 5=old_curbrk */
	blr
