/*	$OpenBSD: freebsd_exec.c,v 1.5 1999/02/10 08:07:19 deraadt Exp $	*/
/*	$NetBSD: freebsd_exec.c,v 1.2 1996/05/18 16:02:08 christos Exp $	*/

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
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/resourcevar.h>
#include <vm/vm.h>
#include <sys/exec_elf.h>
#include <sys/exec_olf.h>

#include <machine/freebsd_machdep.h>

#include <compat/freebsd/freebsd_syscall.h>
#include <compat/freebsd/freebsd_exec.h>
#include <compat/freebsd/freebsd_util.h>

extern struct sysent freebsd_sysent[];
extern char *freebsd_syscallnames[];

extern const char freebsd_emul_path[];

struct emul emul_aout_freebsd = {
	"freebsd",
	NULL,
	freebsd_sendsig,
	FREEBSD_SYS_syscall,
	FREEBSD_SYS_MAXSYSCALL,
	freebsd_sysent,
	freebsd_syscallnames,
	0,
	copyargs,
	setregs,
	NULL,
	freebsd_sigcode,
	freebsd_esigcode,
};

struct emul emul_elf_freebsd = {
	"freebsd",
	NULL,
	freebsd_sendsig,
	FREEBSD_SYS_syscall,
	FREEBSD_SYS_MAXSYSCALL,
	freebsd_sysent,
	freebsd_syscallnames,
	0,
	copyargs,
	setregs,
	NULL,
	freebsd_sigcode,
	freebsd_esigcode,
};

/*
 * exec_aout_makecmds(): Check if it's an a.out-format executable.
 *
 * Given a proc pointer and an exec package pointer, see if the referent
 * of the epp is in a.out format.  First check 'standard' magic numbers for
 * this architecture.  If that fails, try a cpu-dependent hook.
 *
 * This function, in the former case, or the hook, in the latter, is
 * responsible for creating a set of vmcmds which can be used to build
 * the process's vm space and inserting them into the exec package.
 */

int
exec_freebsd_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	u_long midmag;
	int error = ENOEXEC;
	struct exec *execp = epp->ep_hdr;

	if (epp->ep_hdrvalid < sizeof(struct exec))
		return ENOEXEC;

	midmag = FREEBSD_N_GETMID(*execp) << 16 | FREEBSD_N_GETMAGIC(*execp);

	/* assume FreeBSD's MID_MACHINE and [ZQNO]MAGIC is same as NetBSD's */
	switch (midmag) {
	case (MID_MACHINE << 16) | ZMAGIC:
		error = exec_aout_prep_oldzmagic(p, epp);
		break;
	case (MID_MACHINE << 16) | QMAGIC:
		error = exec_aout_prep_zmagic(p, epp);
		break;
	case (MID_MACHINE << 16) | NMAGIC:
		error = exec_aout_prep_nmagic(p, epp);
		break;
	case (MID_MACHINE << 16) | OMAGIC:
		error = exec_aout_prep_omagic(p, epp);
		break;
	}
	if (error == 0)
		epp->ep_emul = &emul_aout_freebsd;
	else
		kill_vmcmds(&epp->ep_vmcmds);

	return error;
}

int
freebsd_elf_probe(p, epp, itp, pos, os)
	struct proc *p;
	struct exec_package *epp;
	char *itp;
	u_long *pos;
	u_int8_t *os;
{
	Elf32_Ehdr *eh = epp->ep_hdr;
	char *bp, *brand;
	int error;
	size_t len;

	brand = elf_check_brand(eh);
	if (brand == NULL || strcmp(brand, "FreeBSD"))
		return (EINVAL);
	if (itp[0]) {
		if ((error = emul_find(p, NULL, freebsd_emul_path, itp, &bp, 0)))
			return (error);
		if ((error = copystr(bp, itp, MAXPATHLEN, &len)))
			return (error);
		free(bp, M_TEMP);
	}
	epp->ep_emul = &emul_elf_freebsd;
	*pos = ELF32_NO_ADDR;
	if (*os == OOS_NULL)
		*os = OOS_FREEBSD;
	return (0);
}
