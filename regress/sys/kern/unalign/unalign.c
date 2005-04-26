/*	$OpenBSD: unalign.c,v 1.1 2005/04/26 21:37:05 miod Exp $	*/
/* Written by Miod Vallat, 2004 AD -- this file is in the public domain */

/*
 * This test checks for the ability, for 32 bit systems, to correctly
 * access a long long (64 bit) value aligned on a 32 bit boundary, but not
 * on a 64 bit boundary.
 *
 * All architectures should pass this test; on m88k this requires assistance
 * from the kernel to recover from the misaligned operand exception: see
 * double_reg_fixup() in arch/m88k/m88k/trap.c for details.
 */

#include <stdio.h>
#include <sys/types.h>

int
main(int argc, char *argv[])
{
#if !defined(__LP64__)
	long array[4] = { 0x12345678, 0x13579ace, 0xffffabcd, 0x2468fedc };
	long long t;
	unsigned int i;

	t = *(long long *)(array + 1);
#if BYTE_ORDER == BIG_ENDIAN
	if (t != 0x13579aceffffabcdULL)
#else
	if (t != 0xffffabcd13579aceULL)
#endif
		return (1);

	t = 0xdeadbeaffadebabeULL;
	*(long long *)(array + 1) = t;

#if BYTE_ORDER == BIG_ENDIAN
	if (array[1] != 0xdeadbeaf || array[2] != 0xfadebabe)
#else
	if (array[1] != 0xfadebabe || array[2] != 0xdeadbeaf)
#endif
		return (1);

#endif	/* __LP64__ */
	return (0);
}
