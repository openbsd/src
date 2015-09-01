/*	$OpenBSD: md_init.h,v 1.3 2015/09/01 05:40:06 guenther Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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
	"\tcalls\t$0," #func "\n"					\
	"\t.previous")

#define	MD_SECTION_PROLOGUE(section, entry) __asm (			\
	"\t.section\t" #section ",\"ax\",@progbits\n"			\
	"\t.globl\t" #entry "\n"					\
	"\t.type\t" #entry ",@function\n"				\
	"\t.align\t1\n"							\
	#entry ":\n"							\
	"\t.word 0x0000\n"	/* entry mask */			\
	"\t.previous")

#define	MD_SECTION_EPILOGUE(section) __asm(				\
	"\t.section\t" #section ",\"ax\",@progbits\n"			\
	"\tret\n"							\
	"\t.previous")

/*
 * _start has no registers to save before calling the real start.
 * _start has two nops, just in case (c.f. m88k?).
 */
#define	MD_CRT0_START				\
	__asm(					\
	".text					\n" \
	"	.align 2			\n" \
	"	.globl _start			\n" \
	"	.type _start,@function		\n" \
	"_start:				\n" \
	"	.word 0x0101			\n" \
	"	pushl %sp			\n" \
	"	calls $1,___start		\n" \
	"	halt				\n" \
	"	.previous")

struct kframe {
	int	kargc;
	char	*kargv[1];	/* size depends on kargc */
	char	kargstr[1];	/* size varies */
	char	kenvstr[1];	/* size varies */
};

/* no cleanup() callback passed to ___start, because no ld.so */
#define	MD_NO_CLEANUP
/* kbind disabled in the kernel for vax until we do dynamic linking */
#define	MD_DISABLE_KBIND	do { } while (0)

#define	MD_START_ARGS		struct kframe *kfp
#define	MD_START_SETUP				\
	char	**argv, **envp;			\
	int	argc;				\
						\
	argc = kfp->kargc;			\
	argv = &kfp->kargv[0];			\
	environ = envp = argv + argc + 1;

