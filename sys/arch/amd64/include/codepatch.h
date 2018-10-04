/*      $OpenBSD: codepatch.h,v 1.8 2018/10/04 05:00:40 guenther Exp $    */
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
__cptext void codepatch_replace(uint16_t tag, void *code, size_t len);
__cptext void codepatch_call(uint16_t tag, void *func);
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

#define CPTAG_STAC		1
#define CPTAG_CLAC		2
#define CPTAG_EOI		3
#define CPTAG_XRSTOR		4
#define CPTAG_XSAVE		5
#define CPTAG_MELTDOWN_NOP	6
#define CPTAG_PCID_SET_REUSE	7

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

#define	PCID_SET_REUSE_SIZE	12
#define	PCID_SET_REUSE_NOP					\
	997:							;\
	.byte	0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00	;\
	.byte	0x0f, 0x1f, 0x40, 0x00				;\
	CODEPATCH_END2(997, CPTAG_PCID_SET_REUSE)

#endif /* _MACHINE_CODEPATCH_H_ */
