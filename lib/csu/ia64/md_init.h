/* $OpenBSD: md_init.h,v 1.1 2011/07/04 05:42:11 pirofti Exp $ */

/*
 * Copyright (c) 2011 Paul Irofti <pirofti@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

#include <sys/cdefs.h>
#include <machine/asm.h>

#define MD_SECT_CALL_FUNC(section, func)				\
		__asm (".section "#section", \"ax\"		\n"	\
		"	br.call.sptk rp="#func"			\n"	\
		"	.previous")


/*-
 * $FreeBSD: src/lib/csu/ia64/crti.S,v 1.3 2001/11/03 06:31:27 peter Exp $
 */

#define	MD_SECTION_PROLOGUE(sect, entry_pt)			\
		__asm (						\
		".section "#sect",\"ax\",@progbits	\n"	\
		".proc "#entry_pt"			\n"	\
		".global "#entry_pt"			\n"	\
		#entry_pt":				\n"	\
		".regstk	0,2,0,0			\n"	\
		".prologue 12,loc0			\n"	\
		".save	ar.pfs,loc1			\n"	\
		"alloc	loc1=ar.pfs,0,2,0,0		\n"	\
		"mov	loc0=b0	/* Save return addr */	\n"	\
		".endp "#entry_pt"			\n"	\
		".previous")

/*-
 * $FreeBSD: src/lib/csu/ia64/crtn.S,v 1.2 2001/10/29 10:18:58 peter Exp $
 */

#define	MD_SECTION_EPILOGUE(sect)				\
		__asm (						\
		".section "#sect",\"ax\",@progbits	\n"	\
		".regstk 0,2,0,0			\n"	\
		"mov	b0=loc0	/*Recover return addr*/ \n"	\
		"mov	ar.pfs=loc1			\n"	\
		"br.ret.sptk.many b0			\n"	\
		".previous")
