#include "SYS.h"

	.data
	.extern curbrk
	.extern _end

	.text
PSEUDO_PREFIX(brk,break)
	
	/* check >= _end, if not make the call for _end */
	addis 5,0,_end@h
	ori  5,5,_end@l		/* # 5 = &_end */
	cmpw 3,5
	bge+ brk_call
	mr 3, 5

brk_call:
	mr 7, 3
	/* call break(size) */
	addis 6, 0, curbrk@H
	ori  6, 6, curbrk@L	/* # 6 = &curbrk */

	sc

	/* check for error */
	cmpwi 0, 0
	beq+ brk_ok		/* OK so this is stupid but I haven't read b */
	b cerror

	/* update, curbrk and return */
brk_ok:
	stw	7, 0(6)		/* # remember, 6=&curbrk, 3= new value */
	mr	3, 0		/* # return 0 */
	blr
