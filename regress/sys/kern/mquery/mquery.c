/*	$OpenBSD: mquery.c,v 1.2 2004/08/02 20:18:50 drahn Exp $ */

/*
 * Copyright (c) 2003 Dale Rahn. All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <machine/vmparam.h>

char chardata = 'a';
extern char etext;
extern char edata;

main()
{
	void *addr;
	void *ret;
	void *bound_a;
	void *bound_b;

	/* check heap */

	errno = 0;
	addr = sbrk(0);
	/* mquery fixed is not allowed in heap, check errno? */
	ret = mquery(addr, 0x1000, PROT_READ|PROT_WRITE, MAP_FIXED, -1, 0);
	if (ret != MAP_FAILED) 
		exit (1);

	errno = 0;
	addr = sbrk(0);
	/* mquery should return next available address after heap. */
	ret = mquery(addr, 0x1000, PROT_READ|PROT_WRITE, 0, -1, 0);
	if (ret == MAP_FAILED) 
		exit (2);


	/* check data */

	errno = 0;
	addr = &chardata;
	/* mquery fixed is not allowed in heap, check errno? */
	ret = mquery(addr, 0x1000, PROT_READ|PROT_WRITE, MAP_FIXED, -1, 0);
	if (ret != MAP_FAILED) 
		exit (3);

	errno = 0;
	addr = &chardata;
	/* mquery should return next available address after heap. */
	ret = mquery(addr, 0x1000, PROT_READ|PROT_WRITE, 0, -1, 0);
	if (ret == MAP_FAILED) 
		exit (4);
	if (ret < (void *)&edata) {
		printf("mquerry returned %p &edata %p\n",
		    ret, (void *)&edata);
		/* should always return above data*/
		exit (5);
	}
#define PAD	(64 * 1024 * 8)
	bound_a = (void *)&chardata;
	bound_b = (void *)(&chardata + MAXDSIZ-PAD);
	/* chardata + MAXDSIZ is valid??? */
	if (ret >= bound_a && ret < bound_b) {
		printf("returned %p should be not be ~%p - ~%p\n",
		    ret, bound_a, bound_b);
		exit (6);
	}

	/* check text */

	errno = 0;
	addr = &main;
	/* mquery on text addresses should fail. */
	ret = mquery(addr, 0x1000, PROT_READ|PROT_WRITE, MAP_FIXED, -1, 0);
	if (ret != MAP_FAILED) 
		exit (7);

	errno = 0;
	addr = &main;
	/* mquery on text addresses should return below data or above heap. */
	ret = mquery(addr, 0x1000, PROT_READ|PROT_WRITE, 0, -1, 0);
	if (ret == MAP_FAILED) 
		exit (8);
	if (ret < (void *)&etext) {
		/* should always return above text */
		exit (9);
	}
	/* chardata + MAXDSIZ is valid??? */
	if (ret >= (void *)&chardata && ret < (void *)(&chardata + MAXDSIZ))
		exit (10);
	
	exit (0);
}


