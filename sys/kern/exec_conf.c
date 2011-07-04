/*	$OpenBSD: exec_conf.c,v 1.28 2011/07/04 22:53:53 tedu Exp $	*/
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

#ifdef COMPAT_LINUX
#include <compat/linux/linux_exec.h>
#endif

extern struct emul emul_native, emul_elf32, emul_elf64, emul_aout,
	emul_freebsd_aout, emul_freebsd_elf,
	emul_linux_elf, emul_linux_aout, emul_netbsd_elf64;

struct execsw execsw[] = {
	{ EXEC_SCRIPT_HDRSZ, exec_script_makecmds, &emul_native, },	/* shell scripts */
#ifdef _KERN_DO_AOUT
#ifdef COMPAT_AOUT
	{ sizeof(struct exec), exec_aout_makecmds, &emul_aout },
#else
	{ sizeof(struct exec), exec_aout_makecmds, &emul_native },	/* a.out binaries */
#endif
#endif
#ifdef _KERN_DO_ECOFF
	{ ECOFF_HDR_SIZE, exec_ecoff_makecmds, &emul_native },	/* ecoff binaries */
#endif
#ifdef _KERN_DO_ELF
	{ sizeof(Elf32_Ehdr), exec_elf32_makecmds, &emul_native },	/* elf binaries */
#endif
#ifdef _KERN_DO_ELF64
	{ sizeof(Elf64_Ehdr), exec_elf64_makecmds, &emul_native },	/* elf binaries */
#endif /* ELF64 */
#ifdef COMPAT_LINUX
	{ LINUX_AOUT_HDR_SIZE, exec_linux_aout_makecmds, &emul_linux_aout }, /* linux a.out */
	{ sizeof(Elf32_Ehdr), exec_linux_elf32_makecmds, &emul_linux_elf },
#endif
#ifdef LKM
	{ 0, NULL, NULL },				/* entries for LKMs */
	{ 0, NULL, NULL },
	{ 0, NULL, NULL },
	{ 0, NULL, NULL },
	{ 0, NULL, NULL },
#endif
};
int nexecs = (sizeof execsw / sizeof(*execsw));
int exec_maxhdrsz;

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
