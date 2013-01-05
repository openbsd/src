/*	$OpenBSD: md_init.h,v 1.1 2013/01/05 11:20:55 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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

#define	MD_SECT_CALL_FUNC(section, func) __asm (			\
	"\t.section\t" #section ",\"ax\",@progbits\n"			\
	"\tbsr\t" #func "\n"					\
	"\t.previous")

#define	MD_SECTION_PROLOGUE(section, entry) __asm (			\
	"\t.section\t" #section ",\"ax\",@progbits\n"			\
	"\t.globl\t" #entry "\n"					\
	"\t.type\t" #entry ",@function\n"				\
	"\t.align\t2\n"							\
	#entry ":\n"							\
	"\tsubu\t%r31, %r31, 16\n"					\
	"\tst\t%r1, %r31, 0\n"						\
	"\t.previous")

#define	MD_SECTION_EPILOGUE(section) __asm(				\
	"\t.section\t" #section ",\"ax\",@progbits\n"			\
	"\tld\t%r1, %r31, 0\n"						\
	"\tjmp.n\t%r1\n"						\
	"\taddu\t%r31, %r31, 16\n"					\
	"\t.previous")
