/*	$NetBSD: misc.h,v 1.1.1.1 1995/03/26 07:12:21 leo Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps (allocator stuff)
 * Copyright (c) 1995 Leo Weppelman (Atari modifications)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ATARI_MISC_H
#define _ATARI_MISC_H
/*
 * St-mem allocator stuff.
 */
struct mem_node {
	CIRCLEQ_ENTRY(mem_node) link; 	
	CIRCLEQ_ENTRY(mem_node) free_link;
	u_long size;		/* size of memory following node. */
};

#define ST_BLOCKSIZE	(sizeof(long))
#define ST_BLOCKMASK	(~(ST_BLOCKSIZE - 1))
#define MNODES_MEM(mn)	((u_char *)(&mn[1]))

void init_stmem __P((void));
void *alloc_stmem __P((u_long, void **));
void free_stmem __P((void *));

#endif /* _ATARI_MISC_H */
