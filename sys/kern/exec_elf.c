/*	$OpenBSD: exec_elf.c,v 1.61 2007/05/28 23:10:10 beck Exp $	*/

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
#include <sys/pool.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/exec_olf.h>
#include <sys/file.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/stat.h>

#include <sys/mman.h>
#include <uvm/uvm_extern.h>

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

struct ELFNAME(probe_entry) {
	int (*func)(struct proc *, struct exec_package *, char *,
	    u_long *, u_int8_t *);
	int os_mask;
} ELFNAME(probes)[] = {
	/* XXX - bogus, shouldn't be size independent.. */
#ifdef COMPAT_FREEBSD
	{ freebsd_elf_probe, 1 << OOS_FREEBSD },
#endif
#ifdef COMPAT_SVR4
	{ svr4_elf_probe,
	    1 << OOS_SVR4 | 1 << OOS_ESIX | 1 << OOS_SOLARIS | 1 << OOS_SCO |
	    1 << OOS_DELL | 1 << OOS_NCR },
#endif
#ifdef COMPAT_LINUX
	{ linux_elf_probe, 1 << OOS_LINUX },
#endif
	{ 0, 1 << OOS_OPENBSD }
};

int ELFNAME(load_file)(struct proc *, char *, struct exec_package *,
	struct elf_args *, Elf_Addr *);
int ELFNAME(check_header)(Elf_Ehdr *, int);
int ELFNAME(olf_check_header)(Elf_Ehdr *, int, u_int8_t *);
int ELFNAME(read_from)(struct proc *, struct vnode *, u_long, caddr_t, int);
void ELFNAME(load_psection)(struct exec_vmcmd_set *, struct vnode *,
	Elf_Phdr *, Elf_Addr *, Elf_Addr *, int *, int);

extern char sigcode[], esigcode[];
#ifdef SYSCALL_DEBUG
extern char *syscallnames[];
#endif

/* round up and down to page boundaries. */
#define ELF_ROUND(a, b)		(((a) + (b) - 1) & ~((b) - 1))
#define ELF_TRUNC(a, b)		((a) & ~((b) - 1))

/*
 * We limit the number of program headers to 32, this should
 * be a reasonable limit for ELF, the most we have seen so far is 12
 */
#define ELF_MAX_VALID_PHDR 32

/*
 * This is the basic elf emul. elf_probe_funcs may change to other emuls.
 */
struct emul ELFNAMEEND(emul) = {
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
	ELFNAME(copyargs),
	setregs,
	ELFNAME2(exec,fixup),
	sigcode,
	esigcode,
	EMUL_ENABLED | EMUL_NATIVE,
};

/*
 * Copy arguments onto the stack in the normal way, but add some
 * space for extra information in case of dynamic binding.
 */
void *
ELFNAME(copyargs)(struct exec_package *pack, struct ps_strings *arginfo,
		void *stack, void *argp)
{
	stack = copyargs(pack, arginfo, stack, argp);
	if (!stack)
		return (NULL);

	/*
	 * Push space for extra arguments on the stack needed by
	 * dynamically linked binaries.
	 */
	if (pack->ep_interp != NULL) {
		pack->ep_emul_argp = stack;
		stack = (char *)stack + ELF_AUX_ENTRIES * sizeof (AuxInfo);
	}
	return (stack);
}

/*
 * Check header for validity; return 0 for ok, ENOEXEC if error
 */
int
ELFNAME(check_header)(Elf_Ehdr *ehdr, int type)
{
	/*
	 * We need to check magic, class size, endianess, and version before
	 * we look at the rest of the Elf_Ehdr structure. These few elements
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

	/* Don't allow an insane amount of sections. */
	if (ehdr->e_phnum > ELF_MAX_VALID_PHDR)
		return (ENOEXEC);

	return (0);
}

#ifndef	SMALL_KERNEL
/*
 * Check header for validity; return 0 for ok, ENOEXEC if error.
 * Remember OS tag for callers sake.
 */
int
ELFNAME(olf_check_header)(Elf_Ehdr *ehdr, int type, u_int8_t *os)
{
	int i;

	/*
	 * We need to check magic, class size, endianess, version, and OS
	 * before we look at the rest of the Elf_Ehdr structure. These few
	 * elements are represented in a machine independant fashion.
	 */
	if (!IS_OLF(*ehdr) ||
	    ehdr->e_ident[OI_CLASS] != ELF_TARG_CLASS ||
	    ehdr->e_ident[OI_DATA] != ELF_TARG_DATA ||
	    ehdr->e_ident[OI_VERSION] != ELF_TARG_VER)
		return (ENOEXEC);

	for (i = 0;
	    i < sizeof(ELFNAME(probes)) / sizeof(ELFNAME(probes)[0]);
	    i++) {
		if ((1 << ehdr->e_ident[OI_OS]) & ELFNAME(probes)[i].os_mask)
			goto os_ok;
	}
	return (ENOEXEC);

os_ok:
	/* Now check the machine dependant header */
	if (ehdr->e_machine != ELF_TARG_MACH ||
	    ehdr->e_version != ELF_TARG_VER)
		return (ENOEXEC);

	/* Check the type */
	if (ehdr->e_type != type)
		return (ENOEXEC);

	/* Don't allow an insane amount of sections. */
	if (ehdr->e_phnum > ELF_MAX_VALID_PHDR)
		return (ENOEXEC);

	*os = ehdr->e_ident[OI_OS];
	return (0);
}
#endif	/* !SMALL_KERNEL */

/*
 * Load a psection at the appropriate address
 */
void
ELFNAME(load_psection)(struct exec_vmcmd_set *vcset, struct vnode *vp,
	Elf_Phdr *ph, Elf_Addr *addr, Elf_Addr *size, int *prot, int flags)
{
	u_long uaddr, msize, lsize, psize, rm, rf;
	long diff, offset, bdiff;
	Elf_Addr base;

	/*
	 * If the user specified an address, then we load there.
	 */
	if (*addr != ELFDEFNNAME(NO_ADDR)) {
		if (ph->p_align > 1) {
			*addr = ELF_TRUNC(*addr, ph->p_align);
			diff = ph->p_vaddr - ELF_TRUNC(ph->p_vaddr, ph->p_align);
			/* page align vaddr */
			base = *addr + trunc_page(ph->p_vaddr) 
			    - ELF_TRUNC(ph->p_vaddr, ph->p_align);

			bdiff = ph->p_vaddr - trunc_page(ph->p_vaddr);

		} else
			diff = 0;
	} else {
		*addr = uaddr = ph->p_vaddr;
		if (ph->p_align > 1)
			*addr = ELF_TRUNC(uaddr, ph->p_align);
		base = trunc_page(uaddr);
		bdiff = uaddr - base;
		diff = uaddr - *addr;
	}

	*prot |= (ph->p_flags & PF_R) ? VM_PROT_READ : 0;
	*prot |= (ph->p_flags & PF_W) ? VM_PROT_WRITE : 0;
	*prot |= (ph->p_flags & PF_X) ? VM_PROT_EXECUTE : 0;

	msize = ph->p_memsz + diff;
	offset = ph->p_offset - bdiff;
	lsize = ph->p_filesz + bdiff;
	psize = round_page(lsize);

	/*
	 * Because the pagedvn pager can't handle zero fill of the last
	 * data page if it's not page aligned we map the last page readvn.
	 */
	if (ph->p_flags & PF_W) {
		psize = trunc_page(lsize);
		if (psize > 0)
			NEW_VMCMD2(vcset, vmcmd_map_pagedvn, psize, base, vp,
			    offset, *prot, flags);
		if (psize != lsize) {
			NEW_VMCMD2(vcset, vmcmd_map_readvn, lsize - psize,
			    base + psize, vp, offset + psize, *prot, flags);
		}
	} else {
		NEW_VMCMD2(vcset, vmcmd_map_pagedvn, psize, base, vp, offset,
		    *prot, flags);
	}

	/*
	 * Check if we need to extend the size of the segment
	 */
	rm = round_page(*addr + ph->p_memsz + diff);
	rf = round_page(*addr + ph->p_filesz + diff);

	if (rm != rf) {
		NEW_VMCMD2(vcset, vmcmd_map_zero, rm - rf, rf, NULLVP, 0,
		    *prot, flags);
	}
	*size = msize;
}

/*
 * Read from vnode into buffer at offset.
 */
int
ELFNAME(read_from)(struct proc *p, struct vnode *vp, u_long off, caddr_t buf,
	int size)
{
	int error;
	size_t resid;

	if ((error = vn_rdwr(UIO_READ, vp, buf, size, off, UIO_SYSSPACE,
	    0, p->p_ucred, &resid, p)) != 0)
		return error;
	/*
	 * See if we got all of it
	 */
	if (resid != 0)
		return (ENOEXEC);
	return (0);
}

/*
 * Load a file (interpreter/library) pointed to by path [stolen from
 * coff_load_shlib()]. Made slightly generic so it might be used externally.
 */
int
ELFNAME(load_file)(struct proc *p, char *path, struct exec_package *epp,
	struct elf_args *ap, Elf_Addr *last)
{
	int error, i;
	struct nameidata nd;
	Elf_Ehdr eh;
	Elf_Phdr *ph = NULL;
	u_long phsize;
	Elf_Addr addr;
	struct vnode *vp;
#ifndef SMALL_KERNEL
	u_int8_t os;			/* Just a dummy in this routine */
#endif
	Elf_Phdr *base_ph = NULL;
	struct interp_ld_sec {
		Elf_Addr vaddr;
		u_long memsz;
	} loadmap[ELF_MAX_VALID_PHDR];
	int nload, idx = 0;
	Elf_Addr pos = *last;
	int file_align;

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
	if (vp->v_mount->mnt_flag & MNT_NOEXEC) {
		error = EACCES;
		goto bad;
	}
	if ((error = VOP_ACCESS(vp, VREAD, p->p_ucred, p)) != 0)
		goto bad1;
	if ((error = ELFNAME(read_from)(p, nd.ni_vp, 0,
				    (caddr_t)&eh, sizeof(eh))) != 0)
		goto bad1;

	if (ELFNAME(check_header)(&eh, ET_DYN)
#ifndef SMALL_KERNEL
	    && ELFNAME(olf_check_header)(&eh, ET_DYN, &os)
#endif
	    ) {
		error = ENOEXEC;
		goto bad1;
	}

	phsize = eh.e_phnum * sizeof(Elf_Phdr);
	ph = malloc(phsize, M_TEMP, M_WAITOK);

	if ((error = ELFNAME(read_from)(p, nd.ni_vp, eh.e_phoff, (caddr_t)ph,
	    phsize)) != 0)
		goto bad1;

	for (i = 0; i < eh.e_phnum; i++) {
		if (ph[i].p_type == PT_LOAD) {
			loadmap[idx].vaddr = trunc_page(ph[i].p_vaddr);
			loadmap[idx].memsz = round_page (ph[i].p_vaddr +
			    ph[i].p_memsz - loadmap[idx].vaddr);
			file_align = ph[i].p_align;
			idx++;
		}
	}
	nload = idx;

	/*
	 * If no position to load the interpreter was set by a probe
	 * function, pick the same address that a non-fixed mmap(0, ..)
	 * would (i.e. something safely out of the way).
	 */
	if (pos == ELFDEFNNAME(NO_ADDR)) {
		pos = uvm_map_hint(p, VM_PROT_EXECUTE);
	}

	pos = ELF_ROUND(pos, file_align);
	*last = epp->ep_interp_pos = pos;
	for (i = 0; i < nload;/**/) {
		vaddr_t	addr;
		struct	uvm_object *uobj;
		off_t	uoff;
		size_t	size;

#ifdef this_needs_fixing
		if (i == 0) {
			uobj = &vp->v_uvm.u_obj;
			/* need to fix uoff */
		} else {
#endif
			uobj = NULL;
			uoff = 0;
#ifdef this_needs_fixing
		}
#endif

		addr = trunc_page(pos + loadmap[i].vaddr);
		size =  round_page(addr + loadmap[i].memsz) - addr;

		/* CRAP - map_findspace does not avoid daddr+MAXDSIZ */
		if ((addr + size > (vaddr_t)p->p_vmspace->vm_daddr) &&
		    (addr < (vaddr_t)p->p_vmspace->vm_daddr + MAXDSIZ))
			addr = round_page((vaddr_t)p->p_vmspace->vm_daddr +
			    MAXDSIZ);

		if (uvm_map_findspace(&p->p_vmspace->vm_map, addr, size,
		    &addr, uobj, uoff, 0, UVM_FLAG_FIXED) == NULL) {
			if (uvm_map_findspace(&p->p_vmspace->vm_map, addr, size,
			    &addr, uobj, uoff, 0, 0) == NULL) {
				error = ENOMEM; /* XXX */
				goto bad1;
			}
		} 
		if (addr != pos + loadmap[i].vaddr) {
			/* base changed. */
			pos = addr - trunc_page(loadmap[i].vaddr);
			pos = ELF_ROUND(pos,file_align);
			epp->ep_interp_pos = *last = pos;
			i = 0;
			continue;
		}

		i++;
	}

	/*
	 * Load all the necessary sections
	 */
	for (i = 0; i < eh.e_phnum; i++) {
		Elf_Addr size = 0;
		int prot = 0;
		int flags;

		switch (ph[i].p_type) {
		case PT_LOAD:
			if (base_ph == NULL) {
				flags = VMCMD_BASE;
				addr = *last;
				base_ph = &ph[i];
			} else {
				flags = VMCMD_RELATIVE;
				addr = ph[i].p_vaddr - base_ph->p_vaddr;
			}
			ELFNAME(load_psection)(&epp->ep_vmcmds, nd.ni_vp,
			    &ph[i], &addr, &size, &prot, flags);
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

	vn_marktext(nd.ni_vp);

bad1:
	VOP_CLOSE(nd.ni_vp, FREAD, p->p_ucred, p);
bad:
	if (ph != NULL)
		free(ph, M_TEMP);

	*last = addr;
	vput(nd.ni_vp);
	return (error);
}

/*
 * Prepare an Elf binary's exec package
 *
 * First, set of the various offsets/lengths in the exec package.
 *
 * Then, mark the text image busy (so it can be demand paged) or error out if
 * this is not possible.  Finally, set up vmcmds for the text, data, bss, and
 * stack segments.
 */
int
ELFNAME2(exec,makecmds)(struct proc *p, struct exec_package *epp)
{
	Elf_Ehdr *eh = epp->ep_hdr;
	Elf_Phdr *ph, *pp;
	Elf_Addr phdr = 0;
	int error, i;
	char *interp = NULL;
	u_long pos = 0, phsize;
	u_int8_t os = OOS_NULL;

	if (epp->ep_hdrvalid < sizeof(Elf_Ehdr))
		return (ENOEXEC);

	if (ELFNAME(check_header)(eh, ET_EXEC)
#ifndef SMALL_KERNEL
	    && ELFNAME(olf_check_header)(eh, ET_EXEC, &os)
#endif
	    )
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
	phsize = eh->e_phnum * sizeof(Elf_Phdr);
	ph = malloc(phsize, M_TEMP, M_WAITOK);

	if ((error = ELFNAME(read_from)(p, epp->ep_vp, eh->e_phoff, (caddr_t)ph,
	    phsize)) != 0)
		goto bad;

	epp->ep_tsize = ELFDEFNNAME(NO_ADDR);
	epp->ep_dsize = ELFDEFNNAME(NO_ADDR);

	for (i = 0; i < eh->e_phnum; i++) {
		pp = &ph[i];
		if (pp->p_type == PT_INTERP) {
			if (pp->p_filesz >= MAXPATHLEN)
				goto bad;
			interp = pool_get(&namei_pool, PR_WAITOK);
			if ((error = ELFNAME(read_from)(p, epp->ep_vp,
			    pp->p_offset, interp, pp->p_filesz)) != 0) {
				goto bad;
			}
			break;
		}
	}

	/*
	 * OK, we want a slightly different twist of the
	 * standard emulation package for "real" elf.
	 */
	epp->ep_emul = &ELFNAMEEND(emul);
	pos = ELFDEFNNAME(NO_ADDR);

	/*
	 * On the same architecture, we may be emulating different systems.
	 * See which one will accept this executable.
	 *
	 * Probe functions would normally see if the interpreter (if any)
	 * exists. Emulation packages may possibly replace the interpreter in
	 * *interp with a changed path (/emul/xxx/<path>), and also
	 * set the ep_emul field in the exec package structure.
	 */
	error = ENOEXEC;
	p->p_os = OOS_OPENBSD;
#ifdef NATIVE_EXEC_ELF
	if (ELFNAME(os_pt_note)(p, epp, epp->ep_hdr, "OpenBSD", 8, 4) == 0) {
		goto native;
	}
#endif
	for (i = 0;
	    i < sizeof(ELFNAME(probes)) / sizeof(ELFNAME(probes)[0]) && error;
	    i++) {
		if (os == OOS_NULL || ((1 << os) & ELFNAME(probes)[i].os_mask))
			error = ELFNAME(probes)[i].func ?
			    (*ELFNAME(probes)[i].func)(p, epp, interp, &pos, &os) :
			    0;
	}
	if (!error)
		p->p_os = os;
#ifndef NATIVE_EXEC_ELF
	else
		goto bad;
#else
native:
#endif /* NATIVE_EXEC_ELF */
	/*
	 * Load all the necessary sections
	 */
	for (i = 0; i < eh->e_phnum; i++) {
		Elf_Addr addr = ELFDEFNNAME(NO_ADDR), size = 0;
		int prot = 0;

		pp = &ph[i];

		switch (ph[i].p_type) {
		case PT_LOAD:
			/*
			 * Calculates size of text and data segments
			 * by starting at first and going to end of last.
			 * 'rwx' sections are treated as data.
			 * this is correct for BSS_PLT, but may not be
			 * for DATA_PLT, is fine for TEXT_PLT.
			 */
			ELFNAME(load_psection)(&epp->ep_vmcmds, epp->ep_vp,
			    &ph[i], &addr, &size, &prot, 0);
			/*
			 * Decide whether it's text or data by looking
			 * at the protection of the section
			 */
			if (prot & VM_PROT_WRITE) {
				/* data section */
				if (epp->ep_dsize == ELFDEFNNAME(NO_ADDR)) {
					epp->ep_daddr = addr;
					epp->ep_dsize = size;
				} else {
					if (addr < epp->ep_daddr) {
						epp->ep_dsize =
						    epp->ep_dsize +
						    epp->ep_daddr -
						    addr;
						epp->ep_daddr = addr;
					} else
						epp->ep_dsize = addr+size -
						    epp->ep_daddr;
				}
			} else if (prot & VM_PROT_EXECUTE) {
				/* text section */
				if (epp->ep_tsize == ELFDEFNNAME(NO_ADDR)) {
					epp->ep_taddr = addr;
					epp->ep_tsize = size;
				} else {
					if (addr < epp->ep_taddr) {
						epp->ep_tsize =
						    epp->ep_tsize +
						    epp->ep_taddr -
						    addr;
						epp->ep_taddr = addr;
					} else
						epp->ep_tsize = addr+size -
						    epp->ep_taddr;
				}
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

	/*
	 * Strangely some linux programs may have all load sections marked
	 * writeable, in this case, textsize is not -1, but rather 0;
	 */
	if (epp->ep_tsize == ELFDEFNNAME(NO_ADDR))
		epp->ep_tsize = 0;
	/*
	 * Another possibility is that it has all load sections marked
	 * read-only.  Fake a zero-sized data segment right after the
	 * text segment.
	 */
	if (epp->ep_dsize == ELFDEFNNAME(NO_ADDR)) {
		epp->ep_daddr = round_page(epp->ep_taddr + epp->ep_tsize);
		epp->ep_dsize = 0;
	}

	epp->ep_interp = interp;
	epp->ep_entry = eh->e_entry;

	/*
	 * Check if we found a dynamically linked binary and arrange to load
	 * its interpreter when the exec file is released.
	 */
	if (interp) {
		struct elf_args *ap;

		ap = malloc(sizeof(struct elf_args), M_TEMP, M_WAITOK);

		ap->arg_phaddr = phdr;
		ap->arg_phentsize = eh->e_phentsize;
		ap->arg_phnum = eh->e_phnum;
		ap->arg_entry = eh->e_entry;
		ap->arg_os = os;

		epp->ep_emul_arg = ap;
		epp->ep_interp_pos = pos;
	}

#if defined(COMPAT_SVR4) && defined(i386)
#ifndef ELF_MAP_PAGE_ZERO
	/* Dell SVR4 maps page zero, yeuch! */
	if (p->p_os == OOS_DELL)
#endif
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, PAGE_SIZE, 0,
		    epp->ep_vp, 0, VM_PROT_READ);
#endif

	free(ph, M_TEMP);
	vn_marktext(epp->ep_vp);
	return (exec_setup_stack(p, epp));

bad:
	if (interp)
		pool_put(&namei_pool, interp);
	free(ph, M_TEMP);
	kill_vmcmds(&epp->ep_vmcmds);
	return (ENOEXEC);
}

/*
 * Phase II of load. It is now safe to load the interpreter. Info collected
 * when loading the program is available for setup of the interpreter.
 */
int
ELFNAME2(exec,fixup)(struct proc *p, struct exec_package *epp)
{
	char	*interp;
	int	error;
	struct	elf_args *ap;
	AuxInfo ai[ELF_AUX_ENTRIES], *a;
	Elf_Addr	pos = epp->ep_interp_pos;

	if (epp->ep_interp == NULL) {
		return (0);
	}

	interp = epp->ep_interp;
	ap = epp->ep_emul_arg;

	if ((error = ELFNAME(load_file)(p, interp, epp, ap, &pos)) != 0) {
		free(ap, M_TEMP);
		pool_put(&namei_pool, interp);
		kill_vmcmds(&epp->ep_vmcmds);
		return (error);
	}
	/*
	 * We have to do this ourselves...
	 */
	error = exec_process_vmcmds(p, epp);

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
		a->au_v = PAGE_SIZE;
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
	free(ap, M_TEMP);
	pool_put(&namei_pool, interp);
	return (error);
}

/*
 * Older ELF binaries use EI_ABIVERSION (formerly EI_BRAND) to brand
 * executables.  Newer ELF binaries use EI_OSABI instead.
 */
char *
ELFNAME(check_brand)(Elf_Ehdr *eh)
{
	if (eh->e_ident[EI_ABIVERSION] == '\0')
		return (NULL);
	return (&eh->e_ident[EI_ABIVERSION]);
}

int
ELFNAME(os_pt_note)(struct proc *p, struct exec_package *epp, Elf_Ehdr *eh,
	char *os_name, size_t name_size, size_t desc_size)
{
	Elf_Phdr *hph, *ph;
	Elf_Note *np = NULL;
	size_t phsize;
	int error;

	phsize = eh->e_phnum * sizeof(Elf_Phdr);
	hph = malloc(phsize, M_TEMP, M_WAITOK);
	if ((error = ELFNAME(read_from)(p, epp->ep_vp, eh->e_phoff,
	    (caddr_t)hph, phsize)) != 0)
		goto out1;

	for (ph = hph;  ph < &hph[eh->e_phnum]; ph++) {
		if (ph->p_type != PT_NOTE ||
		    ph->p_filesz > 1024 ||
		    ph->p_filesz < sizeof(Elf_Note) + name_size)
			continue;

		np = malloc(ph->p_filesz, M_TEMP, M_WAITOK);
		if ((error = ELFNAME(read_from)(p, epp->ep_vp, ph->p_offset,
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
