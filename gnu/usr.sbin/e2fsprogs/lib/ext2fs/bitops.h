/*
 * bitops.h --- Bitmap frobbing code.  The byte swapping routines are
 * 	also included here.
 * 
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 * 
 * i386 bitops operations taken from <asm/bitops.h>, Copyright 1992,
 * Linus Torvalds.
 */


extern int ext2fs_set_bit(int nr,void * addr);
extern int ext2fs_clear_bit(int nr, void * addr);
extern int ext2fs_test_bit(int nr, const void * addr);
extern __u16 ext2fs_swab16(__u16 val);
extern __u32 ext2fs_swab32(__u32 val);

/*
 * EXT2FS bitmap manipulation routines.
 */

/* Support for sending warning messages from the inline subroutines */
extern const char *ext2fs_block_string;
extern const char *ext2fs_inode_string;
extern const char *ext2fs_mark_string;
extern const char *ext2fs_unmark_string;
extern const char *ext2fs_test_string;
extern void ext2fs_warn_bitmap(errcode_t errcode, unsigned long arg,
			       const char *description);
extern void ext2fs_warn_bitmap2(ext2fs_generic_bitmap bitmap,
				int code, unsigned long arg);

extern void ext2fs_mark_block_bitmap(ext2fs_block_bitmap bitmap, blk_t block);
extern void ext2fs_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
				       blk_t block);
extern int ext2fs_test_block_bitmap(ext2fs_block_bitmap bitmap, blk_t block);

extern void ext2fs_mark_inode_bitmap(ext2fs_inode_bitmap bitmap, ino_t inode);
extern void ext2fs_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
				       ino_t inode);
extern int ext2fs_test_inode_bitmap(ext2fs_inode_bitmap bitmap, ino_t inode);

extern void ext2fs_fast_mark_block_bitmap(ext2fs_block_bitmap bitmap,
					  blk_t block);
extern void ext2fs_fast_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
					    blk_t block);
extern int ext2fs_fast_test_block_bitmap(ext2fs_block_bitmap bitmap,
					 blk_t block);

extern void ext2fs_fast_mark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					  ino_t inode);
extern void ext2fs_fast_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					    ino_t inode);
extern int ext2fs_fast_test_inode_bitmap(ext2fs_inode_bitmap bitmap,
					 ino_t inode);
extern blk_t ext2fs_get_block_bitmap_start(ext2fs_block_bitmap bitmap);
extern ino_t ext2fs_get_inode_bitmap_start(ext2fs_inode_bitmap bitmap);
extern blk_t ext2fs_get_block_bitmap_end(ext2fs_block_bitmap bitmap);
extern ino_t ext2fs_get_inode_bitmap_end(ext2fs_inode_bitmap bitmap);

extern void ext2fs_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					   blk_t block, int num);
extern void ext2fs_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					     blk_t block, int num);
extern int ext2fs_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
					  blk_t block, int num);
extern void ext2fs_fast_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						blk_t block, int num);
extern void ext2fs_fast_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						  blk_t block, int num);
extern int ext2fs_fast_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
					       blk_t block, int num);

/*
 * The inline routines themselves...
 * 
 * If NO_INLINE_FUNCS is defined, then we won't try to do inline
 * functions at all; they will be included as normal functions in
 * inline.c
 */
#ifdef NO_INLINE_FUNCS
#if (defined(__i386__) || defined(__i486__) || defined(__i586__) || \
     defined(__mc68000__) || defined(__sparc__))
	/* This prevents bitops.c from trying to include the C */
	/* function version of these functions */
#define _EXT2_HAVE_ASM_BITOPS_
#endif
#endif /* NO_INLINE_FUNCS */

#if (defined(INCLUDE_INLINE_FUNCS) || !defined(NO_INLINE_FUNCS))
#ifdef INCLUDE_INLINE_FUNCS
#define _INLINE_ extern
#else
#define _INLINE_ extern __inline__
#endif

#if (defined(__i386__) || defined(__i486__) || defined(__i586__))

#define _EXT2_HAVE_ASM_BITOPS_
	
/*
 * These are done by inline assembly for speed reasons.....
 *
 * All bitoperations return 0 if the bit was cleared before the
 * operation and != 0 if it was not.  Bit 0 is the LSB of addr; bit 32
 * is the LSB of (addr+1).
 */

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy_h { unsigned long a[100]; };
#define EXT2FS_ADDR (*(struct __dummy_h *) addr)
#define EXT2FS_CONST_ADDR (*(const struct __dummy_h *) addr)	

_INLINE_ int ext2fs_set_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (EXT2FS_ADDR)
		:"r" (nr));
	return oldbit;
}

_INLINE_ int ext2fs_clear_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (EXT2FS_ADDR)
		:"r" (nr));
	return oldbit;
}

_INLINE_ int ext2fs_test_bit(int nr, const void * addr)
{
	int oldbit;

	__asm__ __volatile__("btl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit)
		:"m" (EXT2FS_CONST_ADDR),"r" (nr));
	return oldbit;
}

#undef EXT2FS_ADDR

#endif	/* i386 */

#ifdef __mc68000__

#define _EXT2_HAVE_ASM_BITOPS_

_INLINE_ int ext2fs_set_bit(int nr,void * addr)
{
	char retval;

	__asm__ __volatile__ ("bfset %2@{%1:#1}; sne %0"
	     : "=d" (retval) : "d" (nr^7), "a" (addr));

	return retval;
}

_INLINE_ int ext2fs_clear_bit(int nr, void * addr)
{
	char retval;

	__asm__ __volatile__ ("bfclr %2@{%1:#1}; sne %0"
	     : "=d" (retval) : "d" (nr^7), "a" (addr));

	return retval;
}

_INLINE_ int ext2fs_test_bit(int nr, const void * addr)
{
	char retval;

	__asm__ __volatile__ ("bftst %2@{%1:#1}; sne %0"
	     : "=d" (retval) : "d" (nr^7), "a" (addr));

	return retval;
}

#endif /* __mc68000__ */

#ifdef __sparc__

#define _EXT2_HAVE_ASM_BITOPS_

#ifndef EXT2_OLD_BITOPS

/*
 * Do the bitops so that we are compatible with the standard i386
 * convention.
 */

_INLINE_ int ext2fs_set_bit(int nr,void * addr)
{
#if 1
	int		mask;
	unsigned char	*ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	__asm__ __volatile__("ldub	[%0], %%g6\n\t"
			     "or	%%g6, %2, %%g5\n\t"
			     "stb	%%g5, [%0]\n\t"
			     "and	%%g6, %2, %0\n"
	: "=&r" (ADDR)
	: "0" (ADDR), "r" (mask)
	: "g5", "g6");
	return (int) ADDR;
#else
	int		mask, retval;
	unsigned char	*ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	retval = (mask & *ADDR) != 0;
	*ADDR |= mask;
	return retval;
#endif
}

_INLINE_ int ext2fs_clear_bit(int nr, void * addr)
{
#if 1
	int		mask;
	unsigned char	*ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	__asm__ __volatile__("ldub	[%0], %%g6\n\t"
			     "andn	%%g6, %2, %%g5\n\t"
			     "stb	%%g5, [%0]\n\t"
			     "and	%%g6, %2, %0\n"
	: "=&r" (ADDR)
	: "0" (ADDR), "r" (mask)
	: "g5", "g6");
	return (int) ADDR;
	
#else
	int		mask, retval;
	unsigned char	*ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	retval = (mask & *ADDR) != 0;
	*ADDR &= ~mask;
	return retval;
#endif
}

_INLINE_ int ext2fs_test_bit(int nr, const void * addr)
{
	int			mask;
	const unsigned char	*ADDR = (const unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	return ((mask & *ADDR) != 0);
}

#else

/* Do things the old, unplesant way. */

_INLINE_ int ext2fs_set_bit(int nr, void *addr)
{
	int		mask, retval;
	unsigned long	*ADDR = (unsigned long *) addr;

	ADDR += nr >> 5;
	mask = 1 << (nr & 31);
	retval = ((mask & *ADDR) != 0);
	*ADDR |= mask;
	return retval;
}

_INLINE_ int ext2fs_clear_bit(int nr, void *addr)
{
	int		mask, retval;
	unsigned long	*ADDR = (unsigned long *) addr;

	ADDR += nr >> 5;
	mask = 1 << (nr & 31);
	retval = ((mask & *ADDR) != 0);
	*ADDR &= ~mask;
	return retval;
}

_INLINE_ int ext2fs_test_bit(int nr, const void *addr)
{
	int			mask;
	const unsigned long	*ADDR = (const unsigned long *) addr;

	ADDR += nr >> 5;
	mask = 1 << (nr & 31);
	return ((mask & *ADDR) != 0);
}
#endif

#endif /* __sparc__ */

#ifndef _EXT2_HAVE_ASM_SWAB

_INLINE_ __u16 ext2fs_swab16(__u16 val)
{
	return (val >> 8) | (val << 8);
}

_INLINE_ __u32 ext2fs_swab32(__u32 val)
{
	return ((val>>24) | ((val>>8)&0xFF00) |
		((val<<8)&0xFF0000) | (val<<24));
}

#endif /* !_EXT2_HAVE_ASM_SWAB */

_INLINE_ void ext2fs_mark_generic_bitmap(ext2fs_generic_bitmap bitmap,
					 __u32 bitno);
_INLINE_ void ext2fs_unmark_generic_bitmap(ext2fs_generic_bitmap bitmap,
					   blk_t bitno);
_INLINE_ int ext2fs_test_generic_bitmap(ext2fs_generic_bitmap bitmap,
					blk_t bitno);

_INLINE_ void ext2fs_mark_generic_bitmap(ext2fs_generic_bitmap bitmap,
					 __u32 bitno)
{
	if ((bitno < bitmap->start) || (bitno > bitmap->end)) {
		ext2fs_warn_bitmap2(bitmap, EXT2FS_MARK_ERROR, bitno);
		return;
	}
	ext2fs_set_bit(bitno - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_unmark_generic_bitmap(ext2fs_generic_bitmap bitmap,
					   blk_t bitno)
{
	if ((bitno < bitmap->start) || (bitno > bitmap->end)) {
		ext2fs_warn_bitmap2(bitmap, EXT2FS_UNMARK_ERROR, bitno);
		return;
	}
	ext2fs_clear_bit(bitno - bitmap->start, bitmap->bitmap);
}

_INLINE_ int ext2fs_test_generic_bitmap(ext2fs_generic_bitmap bitmap,
					blk_t bitno)
{
	if ((bitno < bitmap->start) || (bitno > bitmap->end)) {
		ext2fs_warn_bitmap2(bitmap, EXT2FS_TEST_ERROR, bitno);
		return 0;
	}
	return ext2fs_test_bit(bitno - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_mark_block_bitmap(ext2fs_block_bitmap bitmap,
				       blk_t block)
{
	ext2fs_mark_generic_bitmap((ext2fs_generic_bitmap) bitmap, block);
}

_INLINE_ void ext2fs_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
					 blk_t block)
{
	ext2fs_unmark_generic_bitmap((ext2fs_generic_bitmap) bitmap, block);
}

_INLINE_ int ext2fs_test_block_bitmap(ext2fs_block_bitmap bitmap,
				       blk_t block)
{
	return ext2fs_test_generic_bitmap((ext2fs_generic_bitmap) bitmap, 
					  block);
}

_INLINE_ void ext2fs_mark_inode_bitmap(ext2fs_inode_bitmap bitmap,
				       ino_t inode)
{
	ext2fs_mark_generic_bitmap((ext2fs_generic_bitmap) bitmap, inode);
}

_INLINE_ void ext2fs_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					 ino_t inode)
{
	ext2fs_unmark_generic_bitmap((ext2fs_generic_bitmap) bitmap, inode);
}

_INLINE_ int ext2fs_test_inode_bitmap(ext2fs_inode_bitmap bitmap,
				       ino_t inode)
{
	return ext2fs_test_generic_bitmap((ext2fs_generic_bitmap) bitmap, 
					  inode);
}

_INLINE_ void ext2fs_fast_mark_block_bitmap(ext2fs_block_bitmap bitmap,
					    blk_t block)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_MARK, block,
				   bitmap->description);
		return;
	}
#endif	
	ext2fs_set_bit(block - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_fast_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
					      blk_t block)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_UNMARK,
				   block, bitmap->description);
		return;
	}
#endif
	ext2fs_clear_bit(block - bitmap->start, bitmap->bitmap);
}

_INLINE_ int ext2fs_fast_test_block_bitmap(ext2fs_block_bitmap bitmap,
					    blk_t block)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_TEST,
				   block, bitmap->description);
		return 0;
	}
#endif
	return ext2fs_test_bit(block - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_fast_mark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					    ino_t inode)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((inode < bitmap->start) || (inode > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_INODE_MARK,
				   inode, bitmap->description);
		return;
	}
#endif
	ext2fs_set_bit(inode - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_fast_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					      ino_t inode)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((inode < bitmap->start) || (inode > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_INODE_UNMARK,
				   inode, bitmap->description);
		return;
	}
#endif
	ext2fs_clear_bit(inode - bitmap->start, bitmap->bitmap);
}

_INLINE_ int ext2fs_fast_test_inode_bitmap(ext2fs_inode_bitmap bitmap,
					   ino_t inode)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((inode < bitmap->start) || (inode > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_INODE_TEST,
				   inode, bitmap->description);
		return 0;
	}
#endif
	return ext2fs_test_bit(inode - bitmap->start, bitmap->bitmap);
}

_INLINE_ blk_t ext2fs_get_block_bitmap_start(ext2fs_block_bitmap bitmap)
{
	return bitmap->start;
}

_INLINE_ ino_t ext2fs_get_inode_bitmap_start(ext2fs_inode_bitmap bitmap)
{
	return bitmap->start;
}

_INLINE_ blk_t ext2fs_get_block_bitmap_end(ext2fs_block_bitmap bitmap)
{
	return bitmap->end;
}

_INLINE_ ino_t ext2fs_get_inode_bitmap_end(ext2fs_inode_bitmap bitmap)
{
	return bitmap->end;
}

_INLINE_ int ext2fs_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
					    blk_t block, int num)
{
	int	i;

	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_TEST,
				   block, bitmap->description);
		return 0;
	}
	for (i=0; i < num; i++) {
		if (ext2fs_fast_test_block_bitmap(bitmap, block+i))
			return 0;
	}
	return 1;
}

_INLINE_ int ext2fs_fast_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
						 blk_t block, int num)
{
	int	i;

#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_TEST,
				   block, bitmap->description);
		return 0;
	}
#endif
	for (i=0; i < num; i++) {
		if (ext2fs_fast_test_block_bitmap(bitmap, block+i))
			return 0;
	}
	return 1;
}

_INLINE_ void ext2fs_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					     blk_t block, int num)
{
	int	i;
	
	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_MARK, block,
				   bitmap->description);
		return;
	}
	for (i=0; i < num; i++)
		ext2fs_set_bit(block + i - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_fast_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						  blk_t block, int num)
{
	int	i;
	
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_MARK, block,
				   bitmap->description);
		return;
	}
#endif	
	for (i=0; i < num; i++)
		ext2fs_set_bit(block + i - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					       blk_t block, int num)
{
	int	i;
	
	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_UNMARK, block,
				   bitmap->description);
		return;
	}
	for (i=0; i < num; i++)
		ext2fs_clear_bit(block + i - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_fast_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						    blk_t block, int num)
{
	int	i;
	
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_UNMARK, block,
				   bitmap->description);
		return;
	}
#endif	
	for (i=0; i < num; i++)
		ext2fs_clear_bit(block + i - bitmap->start, bitmap->bitmap);
}

#undef _INLINE_
#endif

