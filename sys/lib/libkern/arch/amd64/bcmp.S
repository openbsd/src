#include <machine/asm.h>

ENTRY(bcmp)
	RETGUARD_SETUP(bcmp, r11)
	xorl	%eax,%eax		/* clear return value */

	movq	%rdx,%rcx		/* compare by words */
	shrq	$3,%rcx
	repe
	cmpsq
	jne	L1

	movq	%rdx,%rcx		/* compare remainder by bytes */
	andq	$7,%rcx
	repe
	cmpsb
	je	L2

L1:	incl	%eax
L2:	RETGUARD_CHECK(bcmp, r11)
	ret
