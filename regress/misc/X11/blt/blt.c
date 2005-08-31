/*	$OpenBSD: blt.c,v 1.1 2005/08/31 20:53:49 kettenis Exp $	*/
/*
 *	Written by Mark Kettenis <kettenis@openbsd.org> 2004 Public Domain
 */

#include <sys/types.h>
#include <sys/mman.h>

#include <assert.h>
#include <stddef.h>

typedef unsigned FbStip;
typedef unsigned FbBits;
typedef int FbStride;
typedef int Bool;

extern void fbBlt (FbBits *, FbStride, int, FbBits *, FbStride, int,
		   int,	int, int, FbBits, int, Bool, Bool);

FbBits map[] = { 0x77ff7700, 0x11335577 };

int
main (void)
{
	int pagesize;
	FbBits *src;
	FbBits *dst;
	int srcX, dstX;
	int bpp;
	int alu = 1;
	FbBits pm = 0xffffffff;

	pagesize = getpagesize();

	src = mmap(NULL, 2 * pagesize, PROT_READ|PROT_WRITE, MAP_ANON, -1, 0);
	assert(src);

	dst = mmap(NULL, 2 * pagesize, PROT_READ|PROT_WRITE, MAP_ANON, -1, 0);
	assert(dst);

	mprotect((char *)src + pagesize, pagesize, PROT_NONE);
	src = (FbBits *)((char *)src + (pagesize - sizeof map));
	memcpy (src, map, sizeof map);

	for (bpp = 8; bpp <= 32; bpp += 8)
		for (dstX = 0; dstX < 64; dstX += bpp)
			for (srcX = 0; srcX < 32; srcX += bpp)
				fbBlt(src, 1, srcX, dst, 256, dstX,
				    (32 - srcX), 2, alu, pm, bpp, 0, 0);

	for (bpp = 8; bpp <= 32; bpp += 8)
		for (dstX = 0; dstX < 64; dstX += bpp)
			for (srcX = 0; srcX < 32; srcX += bpp)
				fbBlt(src, 1, srcX, dst, 256, dstX,
				    (64 - srcX), 1, alu, pm, bpp, 0, 0);

	return 0;
}
