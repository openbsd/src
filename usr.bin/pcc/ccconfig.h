/*	$OpenBSD: ccconfig.h,v 1.2 2007/10/15 09:46:30 otto Exp $	*/

/*
 * Copyright (c) 2004 Anders Magnusson (ragge@ludd.luth.se).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * Various settings that controls how the C compiler works.
 */

/* common cpp predefines */
#define	CPPADD	{ "-D__OpenBSD__", "-D__unix__", NULL }
#define	DYNLINKER { "-dynamic-linker", "/usr/libexec/ld.so", NULL }
#define CRT0FILE "/usr/lib/crt0.o"
#define STARTFILES { "/usr/lib/crtbegin.o", NULL }
#define	ENDFILES { "/usr/lib/crtend.o", NULL }

#define CPPMDADDS { \
	{ "x86", { "-D__i386__", "-D__i386", "-D__ELF__", \
		NULL } }, \
	{ "alpha", { "-D__alpha", "-D__alpha_ev4__", "-D__alpha__", \
		"-D__ELF__", NULL } }, \
	{ "amd64", { "-D__x86_64", "-D__athlon", "-D__amd64__", \
		"-D__athlon__", "-D__ELF__", NULL } }, \
	{ "arm", { "-D__arm__", "-D__ELF__", NULL } }, \
	{ "hppa", { "-D__hppa", "-D__hppa__", "-D_PA_RISC_1", "-D__ELF__", \
		NULL } }, \
	{ "m68k", { "-D__m68k__", "-D__mc68000__", "-D__mc68020__", \
		NULL } }, \
	{ "powerpc", { "-D__powerpc__", "-D_ARCH_PPC", "-D__PPC", \
		"-D__powerpc", "-D__PPC__", "-D__ELF__", NULL } }, \
	{ "mips64", { "-D__mips64", "-D__mips64__", "-D_R4000", "-D_mips", \
		"-D_mips_r64", "-D__mips__", "-D__ELF__", NULL } }, \
	{ "sh", { "-D__SH4__", "-D__sh__", NULL } }, \
	{ "sparc", { "-D__sparc__", "-D__ELF__", NULL } }, \
	{ "sparc64", { "-D__sparc", "-D__sparc__", "-D__sparc64__", \
		"-D__sparcv9__", "-D__sparc_v9__", "-D__ELF__", NULL } }, \
	{ "vax", { "-D__vax__", NULL } }, \
	{ NULL }, \
}
#define MAXCPPMDARGS 8

#define	STABS
