/*	$OpenBSD: exec_conf.c,v 1.16 2003/08/23 20:27:30 tedu Exp $	*/
/*	$NetBSD: exec_conf.c,v 1.16 1995/12/09 05:34:47 cgd Exp $	*/

/*
 * Copyright (c) 1993, 1994 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou.
 * 4. The name of the author may not be used to endorse or promote products
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

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_script.h>

#if defined(_KERN_DO_ECOFF)
#include <sys/exec_ecoff.h>
#endif

#if defined(_KERN_DO_ELF) || defined(_KERN_DO_ELF64)
#include <sys/exec_elf.h>
#endif

#ifdef COMPAT_SVR4
#include <compat/svr4/svr4_exec.h>
#endif

#ifdef COMPAT_IBCS2
#include <compat/ibcs2/ibcs2_exec.h>
#endif

#ifdef COMPAT_LINUX
#include <compat/linux/linux_exec.h>
#endif

#ifdef COMPAT_BSDOS
#include <compat/bsdos/bsdos_exec.h>
#endif

#ifdef COMPAT_FREEBSD
#include <compat/freebsd/freebsd_exec.h>
#endif

#ifdef COMPAT_HPUX
#include <compat/hpux/hpux_exec.h>
#endif

#ifdef COMPAT_M68K4K
#include <compat/m68k4k/m68k4k_exec.h>
#endif

#ifdef COMPAT_VAX1K
#include <compat/vax1k/vax1k_exec.h>
#endif

struct execsw execsw[] = {
#ifdef LKM
	{ 0, NULL, },					/* entries for LKMs */
	{ 0, NULL, },
	{ 0, NULL, },
	{ 0, NULL, },
	{ 0, NULL, },
#endif
	{ MAXINTERP, exec_script_makecmds, },		/* shell scripts */
#ifdef _KERN_DO_AOUT
	{ sizeof(struct exec), exec_aout_makecmds, },	/* a.out binaries */
#endif
#ifdef _KERN_DO_ECOFF
	{ ECOFF_HDR_SIZE, exec_ecoff_makecmds, },	/* ecoff binaries */
#endif
#ifdef _KERN_DO_ELF
	{ sizeof(Elf32_Ehdr), exec_elf32_makecmds, },	/* elf binaries */
#endif
#ifdef _KERN_DO_ELF64
	{ sizeof(Elf64_Ehdr), exec_elf64_makecmds, },	/* elf binaries */
#endif
#ifdef COMPAT_LINUX
	{ LINUX_AOUT_HDR_SIZE, exec_linux_aout_makecmds, }, /* linux a.out */
#endif
#ifdef COMPAT_IBCS2
	{ COFF_HDR_SIZE, exec_ibcs2_coff_makecmds, },	/* coff binaries */
	{ XOUT_HDR_SIZE, exec_ibcs2_xout_makecmds, },	/* x.out binaries */
#endif
#ifdef COMPAT_BSDOS
	{ BSDOS_AOUT_HDR_SIZE, exec_bsdos_aout_makecmds, },	/* bsdos */
#endif
#ifdef COMPAT_FREEBSD
	{ FREEBSD_AOUT_HDR_SIZE, exec_freebsd_aout_makecmds, },	/* freebsd */
#endif
#ifdef COMPAT_HPUX
	{ HPUX_EXEC_HDR_SIZE, exec_hpux_makecmds, },	/* HP-UX a.out */
#endif
#ifdef COMPAT_M68K4K
	{ sizeof(struct exec), exec_m68k4k_makecmds, },	/* m68k4k a.out */
#endif
#ifdef COMPAT_VAX1K
	{ sizeof(struct exec), exec_vax1k_makecmds, },	/* vax1k a.out */
#endif
};
int nexecs = (sizeof execsw / sizeof(*execsw));
int exec_maxhdrsz;

extern struct emul emul_native, emul_elf32, emul_elf64, emul_aout,
	emul_bsdos, emul_aout_freebsd, emul_elf_freebsd, emul_hpux,
	emul_ibcs2, emul_linux_elf, emul_linux_aout, emul_elf64_netbsd,
	emul_osf1, emul_sunos, emul_svr4, emul_ultrix;
struct emul *emulsw[] = {
#if defined (_KERN_DO_AOUT) && defined (COMPAT_AOUT)
	&emul_aout,
#endif
#ifdef COMPAT_BSDOS
	&emul_bsdos,
#endif
#ifdef COMPAT_FREEBSD
	&emul_aout_freebsd,
	&emul_elf_freebsd,
#endif
#ifdef COMPAT_HPUX
	&emul_hpux,
#endif
#ifdef COMPAT_IBCS2
	&emul_ibcs2,
#endif
#ifdef COMPAT_LINUX
	&emul_linux_elf,
	&emul_linux_aout,
#endif
#if defined (COMPAT_NETBSD) && defined (_KERN_DO_ELF64)
	&emul_elf64_netbsd,
#endif
#ifdef COMPAT_OSF1
	&emul_osf1,
#endif
#ifdef COMPAT_SUNOS
	&emul_sunos,
#endif
#ifdef COMPAT_SVR4
	&emul_svr4,
#endif
#ifdef COMPAT_ULTRIX
	&emul_ultrix,
#endif
};
int	nemuls = sizeof(emulsw) / sizeof(*emulsw);

void	init_exec(void);

void
init_exec(void)
{
	int i;

	/*
	 * figure out the maximum size of an exec header.
	 * XXX should be able to keep LKM code from modifying exec switch
	 * when we're still using it, but...
	 */
	for (i = 0; i < nexecs; i++)
		if (execsw[i].es_check != NULL &&
		    execsw[i].es_hdrsz > exec_maxhdrsz)
			exec_maxhdrsz = execsw[i].es_hdrsz;
}
