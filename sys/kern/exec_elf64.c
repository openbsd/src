/*	$OpenBSD: exec_elf64.c,v 1.6 1999/09/25 11:43:29 kstailey Exp $	*/

/*
 * Copyright (c) 1996 Per Fogelstrom
 * All rights reserved.
 *
 * Copyright (c) 1994 Christos Zoulas
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
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/exec.h>

#if defined(_KERN_DO_ELF64)

#include <sys/exec_elf.h>
#include <sys/exec_olf.h>
#include <sys/file.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/stat.h>

#include <sys/mman.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/exec.h>

#ifdef COMPAT_LINUX
#include <compat/linux/linux_exec.h>
#endif

#ifdef COMPAT_SVR4
#include <compat/svr4/svr4_exec.h>
#endif

#ifdef COMPAT_FREEBSD
#include <compat/freebsd/freebsd_exec.h>
#endif

#ifdef COMPAT_NETBSD
#include <compat/netbsd/netbsd_exec.h>
#endif

struct elf64_probe_entry {
	int (*func) __P((struct proc *, struct exec_package *, char *,
	    u_long *, u_int8_t *));
	int os_mask;
} elf64_probes[] = {
#ifdef COMPAT_FREEBSD
	{ freebsd_elf64_probe, 1 << OOS_FREEBSD },
#endif
#ifdef COMPAT_SVR4
	{ svr4_elf64_probe,
	    1 << OOS_SVR4 | 1 << OOS_ESIX | 1 << OOS_SOLARIS | 1 << OOS_SCO |
	    1 << OOS_DELL | 1 << OOS_NCR },
#endif
#ifdef COMPAT_LINUX
	{ linux_elf64_probe, 1 << OOS_LINUX },
#endif
#ifdef COMPAT_NETBSD
	{ netbsd_elf64_probe, 1 << OOS_NETBSD },
#endif
	{ 0, 1 << OOS_OPENBSD }
};

int elf64_load_file __P((struct proc *, char *, struct exec_package *,
    struct elf_args *, u_long *));

int elf64_check_header __P((Elf64_Ehdr *, int));
int olf64_check_header __P((Elf64_Ehdr *, int, u_int8_t *));
int elf64_read_from __P((struct proc *, struct vnode *, u_long, caddr_t, int));
void elf64_load_psection __P((struct exec_vmcmd_set *, struct vnode *,
    Elf64_Phdr *, u_long *, u_long *, int *));

int exec_elf64_fixup __P((struct proc *, struct exec_package *));

/* round up and down to page boundaries. */
#define ELF_ROUND(a, b)         (((a) + (b) - 1) & ~((b) - 1))
#define ELF_TRUNC(a, b)         ((a) & ~((b) - 1))

/*
 * This is the basic elf emul. elf64_probe_funcs may change to other emuls.
 */

extern char sigcode[], esigcode[];
#ifdef SYSCALL_DEBUG
extern char *syscallnames[];
#endif

struct emul emul_elf64 = {
	"native",
	NULL,
	sendsig,
	SYS_syscall,
	SYS_MAXSYSCALL,
	sysent,
#ifdef SYSCALL_DEBUG
	syscallnames,
#else
	NULL,
#endif
	sizeof (AuxInfo) * ELF_AUX_ENTRIES,
	elf64_copyargs,
	setregs,
	exec_elf64_fixup,
	sigcode,
	esigcode,
};


/*
 * Copy arguments onto the stack in the normal way, but add some
 * space for extra information in case of dynamic binding.
 */
void *
elf64_copyargs(pack, arginfo, stack, argp)
	struct exec_package *pack;
	struct ps_strings *arginfo;
	void *stack;
	void *argp;
{
	stack = copyargs(pack, arginfo, stack, argp);
	if (!stack)
		return (NULL);

	/*
	 * Push space for extra arguments on the stack needed by
	 * dynamically linked binaries
	 */
	if (pack->ep_interp != NULL) {
		pack->ep_emul_argp = stack;
		stack += ELF_AUX_ENTRIES * sizeof (AuxInfo);
	}
	return (stack);
}

/*
 * elf64_check_header():
 *
 * Check header for validity; return 0 for ok, ENOEXEC if error
 */
int
elf64_check_header(ehdr, type)
	Elf64_Ehdr *ehdr;
	int type;
{
        /*
	 * We need to check magic, class size, endianess, and version before
	 * we look at the rest of the Elf64_Ehdr structure. These few elements
	 * are represented in a machine independant fashion.
	 */
	if (!IS_ELF(*ehdr) ||
	    ehdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||
	    ehdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
	    ehdr->e_ident[EI_VERSION] != ELF_TARG_VER)
                return (ENOEXEC);
        
        /* Now check the machine dependant header */
	if (ehdr->e_machine != ELF_TARG_MACH ||
	    ehdr->e_version != ELF_TARG_VER)
                return (ENOEXEC);

        /* Check the type */
	if (ehdr->e_type != type)
		return (ENOEXEC);

	return (0);
}

/*
 * olf64_check_header():
 *
 * Check header for validity; return 0 for ok, ENOEXEC if error.
 * Remeber OS tag for callers sake.
 */
int
olf64_check_header(ehdr, type, os)
	Elf64_Ehdr *ehdr;
	int type;
	u_int8_t *os;
{
	int i;

        /*
	 * We need to check magic, class size, endianess, version, and OS
	 * before we look at the rest of the Elf64_Ehdr structure. These few
	 * elements are represented in a machine independant fashion.
	 */
	if (!IS_OLF(*ehdr) ||
	    ehdr->e_ident[OI_CLASS] != ELF_TARG_CLASS ||
	    ehdr->e_ident[OI_DATA] != ELF_TARG_DATA ||
	    ehdr->e_ident[OI_VERSION] != ELF_TARG_VER)
                return (ENOEXEC);

	for (i = 0; i < sizeof elf64_probes / sizeof elf64_probes[0]; i++)
		if ((1 << ehdr->e_ident[OI_OS]) & elf64_probes[i].os_mask)
                	goto os_ok;
	return (ENOEXEC);

os_ok:        
        /* Now check the machine dependant header */
	if (ehdr->e_machine != ELF_TARG_MACH ||
	    ehdr->e_version != ELF_TARG_VER)
                return (ENOEXEC);

        /* Check the type */
	if (ehdr->e_type != type)
		return (ENOEXEC);

	*os = ehdr->e_ident[OI_OS];
	return (0);
}

/*
 * elf64_load_psection():
 * 
 * Load a psection at the appropriate address
 */
void
elf64_load_psection(vcset, vp, ph, addr, size, prot)
	struct exec_vmcmd_set *vcset;
	struct vnode *vp;
	Elf64_Phdr *ph;
	u_long *addr;
	u_long *size;
	int *prot;
{
	u_long uaddr, msize, psize, rm, rf;
	long diff, offset;

	/*
	 * If the user specified an address, then we load there.
	 */
	if (*addr != ELF64_NO_ADDR) {
		if (ph->p_align > 1) {
			*addr = ELF_ROUND(*addr + ph->p_align, ph->p_align);
			uaddr = ELF_TRUNC(ph->p_vaddr, ph->p_align);
		} else
			uaddr = ph->p_vaddr;
		diff = ph->p_vaddr - uaddr;
	} else {
		*addr = uaddr = ph->p_vaddr;
		if (ph->p_align > 1)
			*addr = ELF_TRUNC(uaddr, ph->p_align);
		diff = uaddr - *addr;
	}

	*prot |= (ph->p_flags & PF_R) ? VM_PROT_READ : 0;
	*prot |= (ph->p_flags & PF_W) ? VM_PROT_WRITE : 0;
	*prot |= (ph->p_flags & PF_X) ? VM_PROT_EXECUTE : 0;

	offset = ph->p_offset - diff;
	*size = ph->p_filesz + diff;
	msize = ph->p_memsz + diff;
	psize = round_page(*size);

	/*
	 * Because the pagedvn pager can't handle zero fill of the last
	 * data page if it's not page aligned we map the last page readvn.
	 */
	if(ph->p_flags & PF_W) {
		psize = trunc_page(*size);
		NEW_VMCMD(vcset, vmcmd_map_pagedvn, psize, *addr, vp,
		    offset, *prot);
		if(psize != *size) {
			NEW_VMCMD(vcset, vmcmd_map_readvn, *size - psize,
			    *addr + psize, vp, offset + psize, *prot);
		}
	}
	else {
		 NEW_VMCMD(vcset, vmcmd_map_pagedvn, psize, *addr, vp, offset,
		     *prot);
	}

	/*
	 * Check if we need to extend the size of the segment
	 */
	rm = round_page(*addr + msize);
	rf = round_page(*addr + *size);

	if (rm != rf) {
		NEW_VMCMD(vcset, vmcmd_map_zero, rm - rf, rf, NULLVP, 0,
		    *prot);
		*size = msize;
	}
}

/*
 * elf64_read_from():
 *
 *	Read from vnode into buffer at offset.
 */
int
elf64_read_from(p, vp, off, buf, size)
	struct proc *p;
	struct vnode *vp;
	u_long off;
	caddr_t buf;
	int size;
{
	int error;
	size_t resid;

	if ((error = vn_rdwr(UIO_READ, vp, buf, size, off, UIO_SYSSPACE,
	    IO_NODELOCKED, p->p_ucred, &resid, p)) != 0)
		return error;
	/*
	 * See if we got all of it
	 */
	if (resid != 0)
		return (ENOEXEC);
	return (0);
}

/*
 * elf64_load_file():
 *
 * Load a file (interpreter/library) pointed to by path [stolen from
 * coff_load_shlib()]. Made slightly generic so it might be used externally.
 */
int
elf64_load_file(p, path, epp, ap, last)
	struct proc *p;
	char *path;
	struct exec_package *epp;
	struct elf_args	*ap;
	u_long *last;
{
	int error, i;
	struct nameidata nd;
	Elf64_Ehdr eh;
	Elf64_Phdr *ph = NULL;
	u_long phsize;
	char *bp = NULL;
	u_long addr = *last;
	struct vnode *vp;
	u_int8_t os;			/* Just a dummy in this routine */

	bp = path;
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, path, p);
	if ((error = namei(&nd)) != 0) {
		return (error);
	}
	vp = nd.ni_vp;
	if (vp->v_type != VREG) {
		error = EACCES;
		goto bad;
	}
	if ((error = VOP_GETATTR(vp, epp->ep_vap, p->p_ucred, p)) != 0)
		goto bad;
	if ((error = VOP_ACCESS(vp, VREAD, p->p_ucred, p)) != 0)
		goto bad1;
	if ((error = elf64_read_from(p, nd.ni_vp, 0,
				    (caddr_t)&eh, sizeof(eh))) != 0)
		goto bad1;

	if (elf64_check_header(&eh, ET_DYN) &&
	    olf64_check_header(&eh, ET_DYN, &os)) {
		error = ENOEXEC;
		goto bad1;
	}

	phsize = eh.e_phnum * sizeof(Elf64_Phdr);
	ph = (Elf64_Phdr *)malloc(phsize, M_TEMP, M_WAITOK);

	if ((error = elf64_read_from(p, nd.ni_vp, eh.e_phoff, (caddr_t)ph,
	    phsize)) != 0)
		goto bad1;

	/*
	 * Load all the necessary sections
	 */
	for (i = 0; i < eh.e_phnum; i++) {
		u_long size = 0;
		int prot = 0;
#if defined(__mips__)
		if (*last == ELF64_NO_ADDR)
			addr = ELF64_NO_ADDR;	/* GRRRRR!!!!! */
#endif

		switch (ph[i].p_type) {
		case PT_LOAD:
			elf64_load_psection(&epp->ep_vmcmds, nd.ni_vp, &ph[i],
						&addr, &size, &prot);
			/* If entry is within this section it must be text */
			if (eh.e_entry >= ph[i].p_vaddr &&
			    eh.e_entry < (ph[i].p_vaddr + size)) {
 				epp->ep_entry = addr + eh.e_entry -
                                        ELF_TRUNC(ph[i].p_vaddr,ph[i].p_align);
				ap->arg_interp = addr;
			}
			addr += size;
			break;

		case PT_DYNAMIC:
		case PT_PHDR:
		case PT_NOTE:
			break;

		default:
			break;
		}
	}

bad1:
	VOP_CLOSE(nd.ni_vp, FREAD, p->p_ucred, p);
bad:
	if (ph != NULL)
		free((char *)ph, M_TEMP);

	*last = addr;
	vput(nd.ni_vp);
	return (error);
}

/*
 * exec_elf64_makecmds(): Prepare an Elf binary's exec package
 *
 * First, set of the various offsets/lengths in the exec package.
 *
 * Then, mark the text image busy (so it can be demand paged) or error out if
 * this is not possible.  Finally, set up vmcmds for the text, data, bss, and
 * stack segments.
 */
int
exec_elf64_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	Elf64_Ehdr *eh = epp->ep_hdr;
	Elf64_Phdr *ph, *pp;
	Elf64_Addr phdr = 0;
	int error, i, nload;
	char interp[MAXPATHLEN];
	u_long pos = 0, phsize;
	u_int8_t os = OOS_NULL;

	if (epp->ep_hdrvalid < sizeof(Elf64_Ehdr))
		return (ENOEXEC);

	if (elf64_check_header(eh, ET_EXEC) &&
	    olf64_check_header(eh, ET_EXEC, &os))
		return (ENOEXEC);

	/*
	 * check if vnode is in open for writing, because we want to demand-
	 * page out of it.  if it is, don't do it, for various reasons.
	 */
	if (epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0");
#endif
		return (ETXTBSY);
	}
	/*
	 * Allocate space to hold all the program headers, and read them
	 * from the file
	 */
	phsize = eh->e_phnum * sizeof(Elf64_Phdr);
	ph = (Elf64_Phdr *)malloc(phsize, M_TEMP, M_WAITOK);

	if ((error = elf64_read_from(p, epp->ep_vp, eh->e_phoff, (caddr_t)ph,
	    phsize)) != 0)
		goto bad;

	epp->ep_tsize = ELF64_NO_ADDR;
	epp->ep_dsize = ELF64_NO_ADDR;

	interp[0] = '\0';

	for (i = 0; i < eh->e_phnum; i++) {
		pp = &ph[i];
		if (pp->p_type == PT_INTERP) {
			if (pp->p_filesz >= sizeof(interp))
				goto bad;
			if ((error = elf64_read_from(p, epp->ep_vp,
			     pp->p_offset, (caddr_t)interp,
			     pp->p_filesz)) != 0)
				goto bad;
			break;
		}
	}

	/*
	 * OK, we want a slightly different twist of the
	 * standard emulation package for "real" elf.
	 */
	epp->ep_emul = &emul_elf64;
	pos = ELF64_NO_ADDR;

	/*
	 * On the same architecture, we may be emulating different systems.
	 * See which one will accept this executable. This currently only
	 * applies to Linux and SVR4 on the i386 and the Alpha.
	 *
	 * Probe functions would normally see if the interpreter (if any)
	 * exists. Emulation packages may possibly replace the interpreter in
	 * interp[] with a changed path (/emul/xxx/<path>), and also
	 * set the ep_emul field in the exec package structure.
	 */
	error = ENOEXEC;
	p->p_os = OOS_OPENBSD;
	for (i = 0; i < sizeof elf64_probes / sizeof elf64_probes[0] && error;
	     i++)
		if (os == OOS_NULL || ((1 << os) & elf64_probes[i].os_mask))
			error = elf64_probes[i].func ?
			    (*elf64_probes[i].func)(p, epp, interp, &pos, &os) :
			    0;
	if (!error)
		p->p_os = os;
#ifndef NATIVE_ELF
	else
		goto bad;
#endif /* NATIVE_ELF */

	/*
	 * Load all the necessary sections
	 */
	for (i = nload = 0; i < eh->e_phnum; i++) {
		u_int64_t addr = ELF64_NO_ADDR, size = 0;
		int prot = 0;

		pp = &ph[i];

		switch (ph[i].p_type) {
		case PT_LOAD:
			/*
			 * XXX
			 * Can handle only 2 sections: text and data
			 */
			if (nload++ == 2)
				goto bad;
			elf64_load_psection(&epp->ep_vmcmds, epp->ep_vp, &ph[i],
			    &addr, &size, &prot);
			/*
			 * Decide whether it's text or data by looking
			 * at the entry point.
			 */
			if (eh->e_entry >= addr &&
			    eh->e_entry < (addr + size)) {
				epp->ep_taddr = addr;
				epp->ep_tsize = size;
				if (epp->ep_daddr == ELF64_NO_ADDR) {
                                        epp->ep_daddr = addr;
                                        epp->ep_dsize = size;
                                }
			} else {
				epp->ep_daddr = addr;
				epp->ep_dsize = size;
			}
			break;

		case PT_SHLIB:
			error = ENOEXEC;
			goto bad;

		case PT_INTERP:
			/* Already did this one */
		case PT_DYNAMIC:
		case PT_NOTE:
			break;

		case PT_PHDR:
			/* Note address of program headers (in text segment) */
			phdr = pp->p_vaddr;
			break;

		default:
			/*
			 * Not fatal, we don't need to understand everything
			 * :-)
			 */
			break;
		}
	}

#if !defined(__mips__)
	/*
	 * If no position to load the interpreter was set by a probe
	 * function, pick the same address that a non-fixed mmap(0, ..)
	 * would (i.e. something safely out of the way).
	 */
	if (pos == ELF64_NO_ADDR)
		pos = round_page(epp->ep_daddr + MAXDSIZ);
#endif

	/*
	 * Check if we found a dynamically linked binary and arrange to load
	 * it's interpreter when the exec file is released.
	 */
	if (interp[0]) {
		char *ip;
		struct elf_args *ap;

		ip = (char *)malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
		ap = (struct elf_args *)
		    malloc(sizeof(struct elf_args), M_TEMP, M_WAITOK);

		bcopy(interp, ip, MAXPATHLEN);
		epp->ep_interp = ip;
		epp->ep_interp_pos = pos;

		ap->arg_phaddr = phdr;
		ap->arg_phentsize = eh->e_phentsize;
		ap->arg_phnum = eh->e_phnum;
		ap->arg_entry = eh->e_entry;
		ap->arg_os = os;

		epp->ep_emul_arg = ap;
		epp->ep_entry = eh->e_entry; /* keep check_exec() happy */
	}
	else {
		epp->ep_interp = NULL;
		epp->ep_entry = eh->e_entry;
	}

#if defined(COMPAT_SVR4) && defined(i386)
#ifndef ELF_MAP_PAGE_ZERO
	/* Dell SVR4 maps page zero, yeuch! */
	if (p->p_os == OOS_DELL)
#endif
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, NBPG, 0,
		    epp->ep_vp, 0, VM_PROT_READ);
#endif

	free((char *)ph, M_TEMP);
	epp->ep_vp->v_flag |= VTEXT;
	return (exec_setup_stack(p, epp));

bad:
	free((char *)ph, M_TEMP);
	kill_vmcmds(&epp->ep_vmcmds);
	return (ENOEXEC);
}

/*
 * Phase II of load. It is now safe to load the interpreter. Info collected
 * when loading the program is available for setup of the interpreter.
 */
int
exec_elf64_fixup(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	char	*interp;
	int	error, i;
	struct	elf_args *ap;
	AuxInfo ai[ELF_AUX_ENTRIES], *a;
	u_long	pos = epp->ep_interp_pos;

	if(epp->ep_interp == 0) {
		return (0);
	}

	interp = (char *)epp->ep_interp;
	ap = (struct elf_args *)epp->ep_emul_arg;

	if ((error = elf64_load_file(p, interp, epp, ap, &pos)) != 0) {
		free((char *)ap, M_TEMP);
		free((char *)interp, M_TEMP);
		kill_vmcmds(&epp->ep_vmcmds);
		return (error);
	}

	/*
	 * We have to do this ourselves...
	 */
	for (i = 0; i < epp->ep_vmcmds.evs_used && !error; i++) {
		struct exec_vmcmd *vcp;

		vcp = &epp->ep_vmcmds.evs_cmds[i];
		error = (*vcp->ev_proc)(p, vcp);
	}
	kill_vmcmds(&epp->ep_vmcmds);

	/*
	 * Push extra arguments on the stack needed by dynamically
	 * linked binaries
	 */
	if (error == 0) {
		a = ai;

		a->au_id = AUX_phdr;
		a->au_v = ap->arg_phaddr;
		a++;

		a->au_id = AUX_phent;
		a->au_v = ap->arg_phentsize;
		a++;

		a->au_id = AUX_phnum;
		a->au_v = ap->arg_phnum;
		a++;

		a->au_id = AUX_pagesz;
		a->au_v = NBPG;
		a++;

		a->au_id = AUX_base;
		a->au_v = ap->arg_interp;
		a++;

		a->au_id = AUX_flags;
		a->au_v = 0;
		a++;

		a->au_id = AUX_entry;
		a->au_v = ap->arg_entry;
		a++;

		a->au_id = AUX_null;
		a->au_v = 0;
		a++;

		error = copyout(ai, epp->ep_emul_argp, sizeof ai);
	}
	free((char *)ap, M_TEMP);
	free((char *)interp, M_TEMP);
	return (error);
}

char *
elf64_check_brand(eh)
	Elf64_Ehdr *eh;
{
	if (eh->e_ident[EI_BRAND] == '\0')
		return (NULL);
	return (&eh->e_ident[EI_BRAND]);
}

int
elf64_os_pt_note(p, epp, eh, os_name, name_size, desc_size)
	struct proc *p;
	struct exec_package *epp;
	Elf64_Ehdr *eh;
	char *os_name;
	size_t name_size, desc_size;
{
	Elf64_Phdr *hph, *ph;
	Elf64_Note *np = NULL;
	size_t phsize;
	int error;

	phsize = eh->e_phnum * sizeof(Elf64_Phdr);
	hph = (Elf64_Phdr *)malloc(phsize, M_TEMP, M_WAITOK);
	if ((error = elf64_read_from(p, epp->ep_vp, eh->e_phoff,
	    (caddr_t)hph, phsize)) != 0)
		goto out1;

	for (ph = hph;  ph < &hph[eh->e_phnum]; ph++) {
		if (ph->p_type != PT_NOTE ||
		    ph->p_filesz < sizeof(Elf64_Note) + name_size)
			continue;

		np = (Elf64_Note *)malloc(ph->p_filesz, M_TEMP, M_WAITOK);
		if ((error = elf64_read_from(p, epp->ep_vp, ph->p_offset,
		    (caddr_t)np, ph->p_filesz)) != 0)
			goto out2;

#if 0
		if (np->type != ELF_NOTE_TYPE_OSVERSION) {
			free(np, M_TEMP);
			np = NULL;
			continue;
		}
#endif

		/* Check the name and description sizes. */
		if (np->namesz != name_size ||
		    np->descsz != desc_size)
			goto out3;

		if (bcmp((np + 1), os_name, name_size))
			goto out3;

		/* XXX: We could check for the specific emulation here */
		/* All checks succeeded. */
		error = 0;
		goto out2;
	}

out3:
	error = ENOEXEC;
out2:
	if (np)
		free(np, M_TEMP);
out1:
	free(hph, M_TEMP);
	return error;
}

#endif /* _KERN_DO_ELF64 */
