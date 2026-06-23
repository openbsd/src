/*	$OpenBSD: codepatch.h,v 1.1 2026/06/23 11:45:54 kettenis Exp $	*/
/*
 * Copyright (c) 2014-2015 Stefan Fritsch <sf@sfritsch.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MACHINE_CODEPATCH_H_
#define _MACHINE_CODEPATCH_H_

#include <machine/param.h>

#ifndef _LOCORE

/* code in this section will be unmapped after boot */
#define __cptext __attribute__((section(".cptext")))

__cptext void *codepatch_maprw(vaddr_t *nva, vaddr_t dest);
__cptext void codepatch_unmaprw(vaddr_t nva);
__cptext void codepatch_fill_nop(void *caddr, uint16_t len);
__cptext void codepatch_nop(uint16_t tag);
void codepatch_disable(void);

#endif /* !_LOCORE */

/*
 * Mark the start of some code snippet to be patched.
 */
#define	CODEPATCH_START	998:
/*
 * Mark the end of some code to be patched, and assign the given tag.
 */
#define	CODEPATCH_END2(startnum,tag)		 \
	999:					 \
	.section .codepatch, "a"		;\
	.quad startnum##b			;\
	.short (999b - startnum##b)		;\
	.short tag				;\
	.int 0					;\
	.previous
#define	CODEPATCH_END(tag)	CODEPATCH_END2(998,tag)

#define CPTAG_REPEAT_TLBI	1


/* Would be neat if these could be in something like .cptext */
#define CODEPATCH_CODE(symbol, instructions...)		\
	.section .rodata;				\
	.globl	symbol;					\
symbol:	instructions;					\
	.size	symbol, . - symbol

/* provide a (short) variable with the length of the patch */
#define CODEPATCH_CODE_LEN(symbol, instructions...)	\
	CODEPATCH_CODE(symbol, instructions);		\
996:	.globl	symbol##_len;				\
	.align	2;					\
symbol##_len:						\
	.short	996b - symbol;				\
	.size	symbol##_len, 2

#endif /* _MACHINE_CODEPATCH_H_ */
