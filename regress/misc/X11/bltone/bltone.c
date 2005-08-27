/*	$OpenBSD: bltone.c,v 1.4 2005/08/27 14:00:57 kettenis Exp $	*/
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

extern void fbBltOne (FbStip *, FbStride, int, FbBits *, FbStride, int, int,
		      int, int, FbBits, FbBits, FbBits, FbBits);

FbStip mask[] = { 0x77ff7700, 0x11335577 };

int
main (void)
{
	int pagesize;
	FbStip *src;
	FbBits *dst;
	int srcX, dstX;
	int dstBpp;

	pagesize = getpagesize();

	src = mmap(NULL, 2 * pagesize, PROT_READ|PROT_WRITE, MAP_ANON, -1, 0);
	assert(src);

	dst = mmap(NULL, 2 * pagesize, PROT_READ|PROT_WRITE, MAP_ANON, -1, 0);
	assert(dst);

	mprotect((char *)src + pagesize, pagesize, PROT_NONE);
	src = (FbStip *)((char *)src + (pagesize - sizeof mask));
	memcpy (src, mask, sizeof mask);

	for (dstBpp = 8; dstBpp <= 32; dstBpp += 8)
		for (dstX = 0; dstX < 64; dstX += 8)
			for (srcX = 0; srcX < 32; srcX++)
				fbBltOne(src, 1, srcX, dst, 128, dstX, dstBpp,
				    (32 - srcX) * dstBpp, 2,
				    0, 0, 0xffffffff, 0);

	for (dstBpp = 8; dstBpp <= 32; dstBpp += 8)
		for (dstX = 0; dstX < 64; dstX += 8)
			for (srcX = 0; srcX < 32; srcX++)
				fbBltOne(src, 1, srcX, dst, 128, dstX, dstBpp,
				    (64 - srcX) * dstBpp, 1,
				    0, 0, 0xffffffff, 0);

  return 0;
}
