#include <machine/asm.h>
/*
 * Will work only if the argument passed is in IEEE format!
 */

ENTRY(fabs)
	subu	r31,r31,40
	st.d	r2,r31,32
	ld.bu	r4,r31,32
	mask	r4,r4,0x7f	/* set sign bit to 0 */
	st.b	r4,r31,32
	ld.d	r2,r31,32
	jmp.n	r1
	addu	r31,r31,40
