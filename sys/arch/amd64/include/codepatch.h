/*      $OpenBSD: codepatch.h,v 1.4 2017/08/25 19:28:48 guenther Exp $    */
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

void *codepatch_maprw(vaddr_t *nva, vaddr_t dest);
void codepatch_unmaprw(vaddr_t nva);
void codepatch_fill_nop(void *caddr, uint16_t len);
void codepatch_nop(uint16_t tag);
void codepatch_replace(uint16_t tag, void *code, size_t len);
void codepatch_call(uint16_t tag, void *func);

#endif /* !_LOCORE */

/*
 * Mark the start of some code snippet to be patched.
 */
#define	CODEPATCH_START	998:
/*
 * Mark the end of some code to be patched, and assign the given tag.
 */
#define	CODEPATCH_END(tag)			 \
	999:					 \
	.section .codepatch, "a"		;\
	.quad 998b				;\
	.short (999b - 998b)			;\
	.short tag				;\
	.int 0					;\
	.previous

#define CPTAG_STAC		1
#define CPTAG_CLAC		2
#define CPTAG_EOI		3

/*
 * As stac/clac SMAP instructions are 3 bytes, we want the fastest
 * 3 byte nop sequence possible here.  This will be replaced by
 * stac/clac instructions if SMAP is detected after booting.
 *
 * This would be 'nop (%rax)' if binutils could cope.
 * Intel documents multi-byte NOP sequences as being available
 * on all family 0x6 and 0xf processors (ie 686+)
 */
#define SMAP_NOP	.byte 0x0f, 0x1f, 0x00
#define SMAP_STAC	CODEPATCH_START			;\
			SMAP_NOP			;\
			CODEPATCH_END(CPTAG_STAC)
#define SMAP_CLAC	CODEPATCH_START			;\
			SMAP_NOP			;\
			CODEPATCH_END(CPTAG_CLAC)

#endif /* _MACHINE_CODEPATCH_H_ */
