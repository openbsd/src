/*	$OpenBSD: ext2_bitops.c,v 1.1 1996/07/13 21:21:15 downsj Exp $	*/

/*
 * bitops.c --- Bitmap frobbing code.  See bitops.h for the inlined
 * 	routines.
 * 
 * Copyright (C) 1993, 1994 Theodore Ts'o.  This file may be
 * redistributed under the terms of the GNU Public License.
 * 
 * Taken from <asm/bitops.h>, Copyright 1992, Linus Torvalds.
 */

#include <sys/types.h>

#include <gnu/ext2fs/ext2_fs.h>

#ifndef INLINE
#define INLINE
#endif

#ifdef alpha

/*
 * Copyright 1994, Linus Torvalds.
 */

/*
 * These have to be done with inline assembly: that way the bit-setting
 * is guaranteed to be atomic. All bit operations return 0 if the bit
 * was cleared before the operation and != 0 if it was not.
 *
 * bit 0 is the LSB of addr; bit 64 is the LSB of (addr+1).
 */

#define EXT2FS_SET_BIT
static INLINE unsigned long ext2fs_set_bit(nr, addr)
	unsigned long nr;
	void *addr;
{
	unsigned long oldbit;
	unsigned long temp;
	unsigned int * m = ((unsigned int *) addr) + (nr >> 5);

	__asm__ __volatile__(
		"\n1:\t"
		"ldl_l %0,%1\n\t"
		"and %0,%3,%2\n\t"
		"bne %2,2f\n\t"
		"xor %0,%3,%0\n\t"
		"stl_c %0,%1\n\t"
		"beq %0,1b\n"
		"2:"
		:"=&r" (temp),
		 "=m" (*m),
		 "=&r" (oldbit)
		:"Ir" (1UL << (nr & 31)),
		 "m" (*m));
	return oldbit != 0;
}

#define EXT2FS_CLEAR_BIT
static INLINE unsigned long ext2fs_clear_bit(nr, addr)
	unsigned long nr;
	void *addr;
{
	unsigned long oldbit;
	unsigned long temp;
	unsigned int * m = ((unsigned int *) addr) + (nr >> 5);

	__asm__ __volatile__(
		"\n1:\t"
		"ldl_l %0,%1\n\t"
		"and %0,%3,%2\n\t"
		"beq %2,2f\n\t"
		"xor %0,%3,%0\n\t"
		"stl_c %0,%1\n\t"
		"beq %0,1b\n"
		"2:"
		:"=&r" (temp),
		 "=m" (*m),
		 "=&r" (oldbit)
		:"Ir" (1UL << (nr & 31)),
		 "m" (*m));
	return oldbit != 0;
}

#define EXT2FS_CHANGE_BIT
static INLINE unsigned long ext2fs_change_bit(nr, addr)
	unsigned long nr;
	void *addr;
{
	unsigned long oldbit;
	unsigned long temp;
	unsigned int * m = ((unsigned int *) addr) + (nr >> 5);

	__asm__ __volatile__(
		"\n1:\t"
		"ldl_l %0,%1\n\t"
		"and %0,%3,%2\n\t"
		"xor %0,%3,%0\n\t"
		"stl_c %0,%1\n\t"
		"beq %0,1b\n"
		:"=&r" (temp),
		 "=m" (*m),
		 "=&r" (oldbit)
		:"Ir" (1UL << (nr & 31)),
		 "m" (*m));
	return oldbit != 0;
}

#define EXT2FS_TEST_BIT
static INLINE unsigned long ext2fs_test_bit(nr, addr)
	int nr;
	const void *addr;
{
	return 1UL & (((const int *) addr)[nr >> 5] >> (nr & 31));
}

/*
 * ffz = Find First Zero in word. Undefined if no zero exists,
 * so code should check against ~0UL first..
 *
 * Do a binary search on the bits.  Due to the nature of large
 * constants on the alpha, it is worthwhile to split the search.
 */
static INLINE unsigned long ext2fs_ffz_b(x)
	unsigned long x;
{
	unsigned long sum = 0;

	x = ~x & -~x;		/* set first 0 bit, clear others */
	if (x & 0xF0) sum += 4;
	if (x & 0xCC) sum += 2;
	if (x & 0xAA) sum += 1;

	return sum;
}

static INLINE unsigned long ext2fs_ffz(word)
	unsigned long word;
{
	unsigned long bits, qofs, bofs;

	__asm__("cmpbge %1,%2,%0" : "=r"(bits) : "r"(word), "r"(~0UL));
	qofs = ffz_b(bits);
	__asm__("extbl %1,%2,%0" : "=r"(bits) : "r"(word), "r"(qofs));
	bofs = ext2fs_ffz_b(bits);

	return qofs*8 + bofs;
}

/*
 * Find next zero bit in a bitmap reasonably efficiently..
 */
#define EXT2FS_FNZB
static INLINE unsigned long ext2fs_fnzb(addr, size, offset)
	void *addr;
	unsigned long size;
	unsgined long offset;
{
	unsigned long * p = ((unsigned long *) addr) + (offset >> 6);
	unsigned long result = offset & ~63UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 63UL;
	if (offset) {
		tmp = *(p++);
		tmp |= ~0UL >> (64-offset);
		if (size < 64)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= 64;
		result += 64;
	}
	while (size & ~63UL) {
		if (~(tmp = *(p++)))
			goto found_middle;
		result += 64;
		size -= 64;
	}
	if (!size)
		return result;
	tmp = *p;
found_first:
	tmp |= ~0UL << size;
found_middle:
	return result + ext2fs_ffz(tmp);
}

/*
 * The optimizer actually does good code for this case..
 */
#define EXT2FS_FFZB
#define ext2fs_ffzb(addr, size) \
	ext2fs_fnzb((addr), (size), 0)

#endif /* alpha */

#ifdef i386

/*
 * Copyright 1992, Linus Torvalds.
 */

/*
 * These have to be done with inline assembly: that way the bit-setting
 * is guaranteed to be atomic. All bit operations return 0 if the bit
 * was cleared before the operation and != 0 if it was not.
 *
 * bit 0 is the LSB of addr; bit 32 is the LSB of (addr+1).
 */

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy { unsigned long a[100]; };
#define ADDR (*(struct __dummy *) addr)
#define CONST_ADDR (*(const struct __dummy *) addr)

#define EXT2FS_SET_BIT
static INLINE int ext2fs_set_bit(nr, addr)
	int nr;
	void *addr;
{
	int oldbit;

	__asm__ __volatile__("btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"ir" (nr));
	return oldbit;
}

#define EXT2FS_CLEAR_BIT
static INLINE int ext2fs_clear_bit(nr, addr)
	int nr;
	void *addr;
{
	int oldbit;

	__asm__ __volatile__("btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"ir" (nr));
	return oldbit;
}

#define EXT2FS_CHANGE_BIT
static INLINE int ext2fs_change_bit(nr, addr)
	int nr;
	void *addr;
{
	int oldbit;

	__asm__ __volatile__("btcl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"ir" (nr));
	return oldbit;
}

/*
 * This routine doesn't need to be atomic.
 */
#define EXT2FS_TEST_BIT
static INLINE int ext2fs_test_bit(nr, addr)
	int nr;
	const void *addr;
{
	return ((1UL << (nr & 31)) & (((const unsigned int *) addr)[nr >> 5])) != 0;
}

/*
 * Find-bit routines..
 */
#define EXT2FS_FFZB
static INLINE int ext2fs_ffzb(addr, size)
	void *addr;
	unsigned size;
{
	int res;

	if (!size)
		return 0;
	__asm__("cld\n\t"
		"movl $-1,%%eax\n\t"
		"xorl %%edx,%%edx\n\t"
		"repe; scasl\n\t"
		"je 1f\n\t"
		"xorl -4(%%edi),%%eax\n\t"
		"subl $4,%%edi\n\t"
		"bsfl %%eax,%%edx\n"
		"1:\tsubl %%ebx,%%edi\n\t"
		"shll $3,%%edi\n\t"
		"addl %%edi,%%edx"
		:"=d" (res)
		:"c" ((size + 31) >> 5), "D" (addr), "b" (addr)
		:"ax", "cx", "di");
	return res;
}

#define EXT2FS_FNZB
static INLINE int ext2fs_fnzb(addr, size, offset)
	void *addr;
	int size;
	int offset;
{
	unsigned long * p = ((unsigned long *) addr) + (offset >> 5);
	int set = 0, bit = offset & 31, res;
	
	if (bit) {
		/*
		 * Look for zero in first byte
		 */
		__asm__("bsfl %1,%0\n\t"
			"jne 1f\n\t"
			"movl $32, %0\n"
			"1:"
			: "=r" (set)
			: "r" (~(*p >> bit)));
		if (set < (32 - bit))
			return set + offset;
		set = 32 - bit;
		p++;
	}
	/*
	 * No zero yet, search remaining full bytes for a zero
	 */
	res = ext2fs_ffzb (p, size - 32 * (p - (unsigned long *) addr));
	return (offset + set + res);
}

#endif /* i386 */

#ifdef m68k

/*
 * Copyright 1992, Linus Torvalds.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

/*
 * Require 68020 or better.
 *
 * They use the standard big-endian m680x0 bit ordering.
 */

#define EXT2FS_SET_BIT
static INLINE int ext2fs_set_bit (nr, vaddr)
	int nr;
	void *vaddr;
{
	char retval;

	__asm__ __volatile__ ("bfset %2{%1,#1}; sne %0"
	     : "=d" (retval) : "d" (nr^7), "m" (*(char *) vaddr));

	return retval;
}

#define EXT2FS_CLEAR_BIT
static INLINE int ext2fs_clear_bit (nr, vaddr)
	int nr;
	void *vaddr;
{
	char retval;

	__asm__ __volatile__ ("bfclr %2{%1,#1}; sne %0"
	     : "=d" (retval) : "d" (nr^7), "m" (*(char *) vaddr));

	return retval;
}

#define EXT2FS_TEST_BIT
static INLINE int ext2fs_test_bit (nr, vaddr)
	int nr;
	const void *vaddr;
{
	return ((1U << (nr & 7)) & (((const unsigned char *) vaddr)[nr >> 3])) != 0;
}

#define EXT2FS_FFZB
static INLINE int ext2fs_ffzb (vaddr, size)
	const void *vaddr;
	unsigned size;
{
	const unsigned long *p = vaddr, *addr = vaddr;
	int res;

	if (!size)
		return 0;

	while (*p++ == ~0UL)
	{
		if (size <= 32)
			return (p - addr) << 5;
		size -= 32;
	}

	--p;
	for (res = 0; res < 32; res++)
		if (!ext2fs_test_bit (res, p))
			break;
	return (p - addr) * 32 + res;
}

#define EXT2FS_FNZB
static INLINE int ext2fs_fnzb (vaddr, size, offset)
	const void *vaddr;
	unsigned size;
	unsigned offset;
{
	const unsigned long *addr = vaddr;
	const unsigned long *p = addr + (offset >> 5);
	int bit = offset & 31UL, res;

	if (offset >= size)
		return size;

	if (bit) {
		/* Look for zero in first longword */
		for (res = bit; res < 32; res++)
			if (!ext2fs_test_bit (res, p))
				return (p - addr) * 32 + res;
		p++;
	}
	/* No zero yet, search remaining full bytes for a zero */
	res = ext2fs_ffzb (p, size - 32 * (p - addr));
	return (p - addr) * 32 + res;
}
#endif	/* m68k */

/*
 * For the benefit of those who are trying to port Linux to another
 * architecture, here are some C-language equivalents.  You should
 * recode these in the native assmebly language, if at all possible.
 *
 * C language equivalents written by Theodore Ts'o, 9/26/92.
 * Modified by Pete A. Zaitcev 7/14/95 to be portable to big endian
 * systems, as well as non-32 bit systems.
 */

#ifndef EXT2FS_SET_BIT
static INLINE int ext2fs_set_bit(nr, addr)
	int nr;
	void *addr;
{
	int		mask, retval, s;
	unsigned char	*ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	s = splhigh();
	retval = (mask & *ADDR) != 0;
	*ADDR |= mask;
	splx(s);
	return retval;
}
#endif

#ifndef EXT2FS_CLEAR_BIT
static INLINE int ext2fs_clear_bit(nr, addr)
	int nr;
	void *addr;
{
	int		mask, retval, s;
	unsigned char	*ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	s = splhigh();
	retval = (mask & *ADDR) != 0;
	*ADDR &= ~mask;
	splx(s);
	return retval;
}
#endif

#ifndef EXT2FS_TEST_BIT
static INLINE int ext2fs_test_bit(nr, addr)
	int nr;
	const void *addr;
{
	int			mask;
	const unsigned char	*ADDR = (const unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	return ((mask & *ADDR) != 0);
}
#endif

#if !defined(EXT2FS_FNZB) || !defined(EXT2FS_FFZB)
#error "Sorry, you need versions of ext2fs_fnzb() and ext2fs_ffzb()."
#endif

/*
 * find the first occurrence of byte 'c', or 1 past the area if none
 */
static INLINE void *ext2fs_memscan(addr, c, size)
	void *addr;
	int c;
	size_t size;
{
	unsigned char * p = (unsigned char *) addr;

	while (size) {
		if (*p == c)
			return (void *) p;
		p++;
		size--;
	}
  	return (void *) p;
}
