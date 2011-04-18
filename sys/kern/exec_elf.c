/*	$OpenBSD: exec_elf.c,v 1.81 2011/04/18 21:44:56 guenther Exp $	*/

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

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
#include <sys/core.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/exec_olf.h>
#include <sys/file.h>
#include <sys/ptrace.h>
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

struct ELFNAME(probe_entry) {
	int (*func)(struct proc *, struct exec_package *, char *,
	    u_long *, u_int8_t *);
} ELFNAME(probes)[] = {
	/* XXX - bogus, shouldn't be size independent.. */
#ifdef COMPAT_LINUX
	{ linux_elf_probe },
#endif
#ifdef COMPAT_SVR4
	{ svr4_elf_probe },
#endif
	{ NULL }
};

int ELFNAME(load_file)(struct proc *, char *, struct exec_package *,
	struct elf_args *, Elf_Addr *);
int ELFNAME(check_header)(Elf_Ehdr *);
int ELFNAME(read_from)(struct proc *, struct vnode *, u_long, caddr_t, int);
void ELFNAME(load_psection)(struct exec_vmcmd_set *, struct vnode *,
	Elf_Phdr *, Elf_Addr *, Elf_Addr *, int *, int);
int ELFNAMEEND(coredump)(struct proc *, void *);

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
	(sizeof(AuxInfo) * ELF_AUX_ENTRIES / sizeof(char *)),
	ELFNAME(copyargs),
	setregs,
	ELFNAME2(exec,fixup),
	ELFNAMEEND(coredump),
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
ELFNAME(check_header)(Elf_Ehdr *ehdr)
{
	/*
	 * We need to check magic, class size, endianess, and version before
	 * we look at the rest of the Elf_Ehdr structure. These few elements
	 * are represented in a machine independent fashion.
	 */
	if (!IS_ELF(*ehdr) ||
	    ehdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||
	    ehdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
	    ehdr->e_ident[EI_VERSION] != ELF_TARG_VER)
		return (ENOEXEC);

	/* Now check the machine dependent header */
	if (ehdr->e_machine != ELF_TARG_MACH ||
	    ehdr->e_version != ELF_TARG_VER)
		return (ENOEXEC);

	/* Don't allow an insane amount of sections. */
	if (ehdr->e_phnum > ELF_MAX_VALID_PHDR)
		return (ENOEXEC);

	return (0);
}

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

	if (ELFNAME(check_header)(&eh) || eh.e_type != ET_DYN) {
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

		/* CRAP - map_findspace does not avoid daddr+BRKSIZ */
		if ((addr + size > (vaddr_t)p->p_vmspace->vm_daddr) &&
		    (addr < (vaddr_t)p->p_vmspace->vm_daddr + BRKSIZ))
			addr = round_page((vaddr_t)p->p_vmspace->vm_daddr +
			    BRKSIZ);

		vm_map_lock(&p->p_vmspace->vm_map);
		if (uvm_map_findspace(&p->p_vmspace->vm_map, addr, size,
		    &addr, uobj, uoff, 0, UVM_FLAG_FIXED) == NULL) {
			if (uvm_map_findspace(&p->p_vmspace->vm_map, addr, size,
			    &addr, uobj, uoff, 0, 0) == NULL) {
				error = ENOMEM; /* XXX */
				vm_map_unlock(&p->p_vmspace->vm_map);
				goto bad1;
			}
		} 
		vm_map_unlock(&p->p_vmspace->vm_map);
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
	Elf_Phdr *ph, *pp, *base_ph = NULL;
	Elf_Addr phdr = 0, exe_base = 0;
	int error, i;
	char *interp = NULL;
	u_long pos = 0, phsize;
	u_int8_t os = OOS_NULL;

	if (epp->ep_hdrvalid < sizeof(Elf_Ehdr))
		return (ENOEXEC);

	if (ELFNAME(check_header)(eh) ||
	   (eh->e_type != ET_EXEC && eh->e_type != ET_DYN))
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

	for (i = 0, pp = ph; i < eh->e_phnum; i++, pp++) {
		if (pp->p_type == PT_INTERP && !interp) {
			if (pp->p_filesz >= MAXPATHLEN)
				goto bad;
			interp = pool_get(&namei_pool, PR_WAITOK);
			if ((error = ELFNAME(read_from)(p, epp->ep_vp,
			    pp->p_offset, interp, pp->p_filesz)) != 0) {
				goto bad;
			}
		} else if (pp->p_type == PT_LOAD) {
			if (base_ph == NULL)
				base_ph = pp;
		}
	}

	if (eh->e_type == ET_DYN) {
		/* need an interpreter and load sections for PIE */
		if (interp == NULL || base_ph == NULL)
			goto bad;
		/* randomize exe_base for PIE */
		exe_base = uvm_map_pie(base_ph->p_align);
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
	for (i = 0; ELFNAME(probes)[i].func != NULL && error; i++) {
		error = (*ELFNAME(probes)[i].func)(p, epp, interp, &pos, &os);
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
	for (i = 0, pp = ph; i < eh->e_phnum; i++, pp++) {
		Elf_Addr addr, size = 0;
		int prot = 0;
		int flags = 0;

		switch (pp->p_type) {
		case PT_LOAD:
			if (exe_base != 0) {
				if (pp == base_ph) {
					flags = VMCMD_BASE;
					addr = exe_base;
				} else {
					flags = VMCMD_RELATIVE;
					addr = pp->p_vaddr - base_ph->p_vaddr;
				}
			} else
				addr = ELFDEFNNAME(NO_ADDR);

			/*
			 * Calculates size of text and data segments
			 * by starting at first and going to end of last.
			 * 'rwx' sections are treated as data.
			 * this is correct for BSS_PLT, but may not be
			 * for DATA_PLT, is fine for TEXT_PLT.
			 */
			ELFNAME(load_psection)(&epp->ep_vmcmds, epp->ep_vp,
			    pp, &addr, &size, &prot, flags);

			/*
			 * Update exe_base in case alignment was off.
			 * For PIE, addr is relative to exe_base so
			 * adjust it (non PIE exe_base is 0 so no change).
			 */
			if (flags == VMCMD_BASE)
				exe_base = addr;
			else
				addr += exe_base;

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

	phdr += exe_base;

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
	epp->ep_entry = eh->e_entry + exe_base;

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
		ap->arg_entry = eh->e_entry + exe_base;
		ap->arg_os = os;

		epp->ep_emul_arg = ap;
		epp->ep_interp_pos = pos;
	}

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

struct countsegs_state {
	int	npsections;
};

int	ELFNAMEEND(coredump_countsegs)(struct proc *, void *,
	    struct uvm_coredump_state *);

struct writesegs_state {
	Elf_Phdr *psections;
	off_t	secoff;
};

int	ELFNAMEEND(coredump_writeseghdrs)(struct proc *, void *,
	    struct uvm_coredump_state *);

int	ELFNAMEEND(coredump_notes)(struct proc *, void *, size_t *);
int	ELFNAMEEND(coredump_note)(struct proc *, void *, size_t *);
int	ELFNAMEEND(coredump_writenote)(struct proc *, void *, Elf_Note *,
	    const char *, void *);

#define	ELFROUNDSIZE	4	/* XXX Should it be sizeof(Elf_Word)? */
#define	elfround(x)	roundup((x), ELFROUNDSIZE)

int
ELFNAMEEND(coredump)(struct proc *p, void *cookie)
{
#ifdef SMALL_KERNEL
	return EPERM;
#else
	Elf_Ehdr ehdr;
	Elf_Phdr phdr, *psections;
	struct countsegs_state cs;
	struct writesegs_state ws;
	off_t notestart, secstart, offset;
	size_t notesize;
	int error, i;

	psections = NULL;
	/*
	 * We have to make a total of 3 passes across the map:
	 *
	 *	1. Count the number of map entries (the number of
	 *	   PT_LOAD sections).
	 *
	 *	2. Write the P-section headers.
	 *
	 *	3. Write the P-sections.
	 */

	/* Pass 1: count the entries. */
	cs.npsections = 0;
	error = uvm_coredump_walkmap(p, NULL,
	    ELFNAMEEND(coredump_countsegs), &cs);
	if (error)
		goto out;

	/* Count the PT_NOTE section. */
	cs.npsections++;

	/* Get the size of the notes. */
	error = ELFNAMEEND(coredump_notes)(p, NULL, &notesize);
	if (error)
		goto out;

	memset(&ehdr, 0, sizeof(ehdr));
	memcpy(ehdr.e_ident, ELFMAG, SELFMAG);
	ehdr.e_ident[EI_CLASS] = ELF_TARG_CLASS;
	ehdr.e_ident[EI_DATA] = ELF_TARG_DATA;
	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	/* XXX Should be the OSABI/ABI version of the executable. */
	ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
	ehdr.e_ident[EI_ABIVERSION] = 0;
	ehdr.e_type = ET_CORE;
	/* XXX This should be the e_machine of the executable. */
	ehdr.e_machine = ELF_TARG_MACH;
	ehdr.e_version = EV_CURRENT;
	ehdr.e_entry = 0;
	ehdr.e_phoff = sizeof(ehdr);
	ehdr.e_shoff = 0;
	ehdr.e_flags = 0;
	ehdr.e_ehsize = sizeof(ehdr);
	ehdr.e_phentsize = sizeof(Elf_Phdr);
	ehdr.e_phnum = cs.npsections;
	ehdr.e_shentsize = 0;
	ehdr.e_shnum = 0;
	ehdr.e_shstrndx = 0;

	/* Write out the ELF header. */
	error = coredump_write(cookie, UIO_SYSSPACE, &ehdr, sizeof(ehdr));
	if (error)
		goto out;

	offset = sizeof(ehdr);

	notestart = offset + sizeof(phdr) * cs.npsections;
	secstart = notestart + notesize;

	psections = malloc(cs.npsections * sizeof(Elf_Phdr),
	    M_TEMP, M_WAITOK|M_ZERO);

	/* Pass 2: now write the P-section headers. */
	ws.secoff = secstart;
	ws.psections = psections;
	error = uvm_coredump_walkmap(p, cookie,
	    ELFNAMEEND(coredump_writeseghdrs), &ws);
	if (error)
		goto out;

	/* Write out the PT_NOTE header. */
	ws.psections->p_type = PT_NOTE;
	ws.psections->p_offset = notestart;
	ws.psections->p_vaddr = 0;
	ws.psections->p_paddr = 0;
	ws.psections->p_filesz = notesize;
	ws.psections->p_memsz = 0;
	ws.psections->p_flags = PF_R;
	ws.psections->p_align = ELFROUNDSIZE;

	error = coredump_write(cookie, UIO_SYSSPACE, psections,
	    cs.npsections * sizeof(Elf_Phdr));
	if (error)
		goto out;

#ifdef DIAGNOSTIC
	offset += cs.npsections * sizeof(Elf_Phdr);
	if (offset != notestart)
		panic("coredump: offset %lld != notestart %lld",
		    (long long) offset, (long long) notestart);
#endif

	/* Write out the notes. */
	error = ELFNAMEEND(coredump_notes)(p, cookie, &notesize);
	if (error)
		goto out;

#ifdef DIAGNOSTIC
	offset += notesize;
	if (offset != secstart)
		panic("coredump: offset %lld != secstart %lld",
		    (long long) offset, (long long) secstart);
#endif

	/* Pass 3: finally, write the sections themselves. */
	for (i = 0; i < cs.npsections - 1; i++) {
		if (psections[i].p_filesz == 0)
			continue;

#ifdef DIAGNOSTIC
		if (offset != psections[i].p_offset)
			panic("coredump: offset %lld != p_offset[%d] %lld",
			    (long long) offset, i,
			    (long long) psections[i].p_filesz);
#endif

		error = coredump_write(cookie, UIO_USERSPACE,
		    (void *)(vaddr_t)psections[i].p_vaddr,
		    psections[i].p_filesz);
		if (error)
			goto out;

#ifdef DIAGNOSTIC
		offset += psections[i].p_filesz;
#endif
	}

out:
	if (psections)
		free(psections, M_TEMP);
	return (error);
#endif
}

int
ELFNAMEEND(coredump_countsegs)(struct proc *p, void *iocookie,
    struct uvm_coredump_state *us)
{
#ifndef SMALL_KERNEL
	struct countsegs_state *cs = us->cookie;

	cs->npsections++;
#endif
	return (0);
}

int
ELFNAMEEND(coredump_writeseghdrs)(struct proc *p, void *iocookie,
    struct uvm_coredump_state *us)
{
#ifndef SMALL_KERNEL
	struct writesegs_state *ws = us->cookie;
	Elf_Phdr phdr;
	vsize_t size, realsize;

	size = us->end - us->start;
	realsize = us->realend - us->start;

	phdr.p_type = PT_LOAD;
	phdr.p_offset = ws->secoff;
	phdr.p_vaddr = us->start;
	phdr.p_paddr = 0;
	phdr.p_filesz = realsize;
	phdr.p_memsz = size;
	phdr.p_flags = 0;
	if (us->prot & VM_PROT_READ)
		phdr.p_flags |= PF_R;
	if (us->prot & VM_PROT_WRITE)
		phdr.p_flags |= PF_W;
	if (us->prot & VM_PROT_EXECUTE)
		phdr.p_flags |= PF_X;
	phdr.p_align = PAGE_SIZE;

	ws->secoff += phdr.p_filesz;
	*ws->psections++ = phdr;
#endif

	return (0);
}

int
ELFNAMEEND(coredump_notes)(struct proc *p, void *iocookie, size_t *sizep)
{
#ifndef SMALL_KERNEL
	struct ps_strings pss;
	struct iovec iov;
	struct uio uio;
	struct elfcore_procinfo cpi;
	Elf_Note nhdr;
	struct process *pr = p->p_p;
	struct proc *q;
	size_t size, notesize;
	int error;

	size = 0;

	/* First, write an elfcore_procinfo. */
	notesize = sizeof(nhdr) + elfround(sizeof("OpenBSD")) +
	    elfround(sizeof(cpi));
	if (iocookie) {
		bzero(&cpi, sizeof(cpi));

		cpi.cpi_version = ELFCORE_PROCINFO_VERSION;
		cpi.cpi_cpisize = sizeof(cpi);
		cpi.cpi_signo = p->p_sigacts->ps_sig;
		cpi.cpi_sigcode = p->p_sigacts->ps_code;

		cpi.cpi_sigpend = p->p_siglist;
		cpi.cpi_sigmask = p->p_sigmask;
		cpi.cpi_sigignore = p->p_sigignore;
		cpi.cpi_sigcatch = p->p_sigcatch;

		cpi.cpi_pid = pr->ps_pid;
		cpi.cpi_ppid = pr->ps_pptr->ps_pid;
		cpi.cpi_pgrp = pr->ps_pgid;
		if (pr->ps_session->s_leader)
			cpi.cpi_sid = pr->ps_session->s_leader->ps_pid;
		else
			cpi.cpi_sid = 0;

		cpi.cpi_ruid = p->p_cred->p_ruid;
		cpi.cpi_euid = p->p_ucred->cr_uid;
		cpi.cpi_svuid = p->p_cred->p_svuid;

		cpi.cpi_rgid = p->p_cred->p_rgid;
		cpi.cpi_egid = p->p_ucred->cr_gid;
		cpi.cpi_svgid = p->p_cred->p_svgid;

		(void)strlcpy(cpi.cpi_name, p->p_comm, sizeof(cpi.cpi_name));

		nhdr.namesz = sizeof("OpenBSD");
		nhdr.descsz = sizeof(cpi);
		nhdr.type = NT_OPENBSD_PROCINFO;

		error = ELFNAMEEND(coredump_writenote)(p, iocookie, &nhdr,
		    "OpenBSD", &cpi);
		if (error)
			return (error);
	}
	size += notesize;

	/* Second, write an NT_OPENBSD_AUXV note. */
	notesize = sizeof(nhdr) + elfround(sizeof("OpenBSD")) +
	    elfround(p->p_emul->e_arglen * sizeof(char *));
	if (iocookie) {
		iov.iov_base = &pss;
		iov.iov_len = sizeof(pss);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)(vaddr_t)PS_STRINGS;
		uio.uio_resid = sizeof(pss);
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_rw = UIO_READ;
		uio.uio_procp = NULL;

		error = uvm_io(&p->p_vmspace->vm_map, &uio, 0);
		if (error)
			return (error);

		if (pss.ps_envstr == NULL)
			return (EIO);

		nhdr.namesz = sizeof("OpenBSD");
		nhdr.descsz = p->p_emul->e_arglen * sizeof(char *);
		nhdr.type = NT_OPENBSD_AUXV;

		error = coredump_write(iocookie, UIO_SYSSPACE,
		    &nhdr, sizeof(nhdr));
		if (error)
			return (error);

		error = coredump_write(iocookie, UIO_SYSSPACE,
		    "OpenBSD", elfround(nhdr.namesz));
		if (error)
			return (error);

		error = coredump_write(iocookie, UIO_USERSPACE,
		    pss.ps_envstr + pss.ps_nenvstr + 1, nhdr.descsz);
		if (error)
			return (error);
	}
	size += notesize;

#ifdef PT_WCOOKIE
	notesize = sizeof(nhdr) + elfround(sizeof("OpenBSD")) +
	    elfround(sizeof(register_t));
	if (iocookie) {
		register_t wcookie;

		nhdr.namesz = sizeof("OpenBSD");
		nhdr.descsz = sizeof(register_t);
		nhdr.type = NT_OPENBSD_WCOOKIE;

		wcookie = process_get_wcookie(p);
		error = ELFNAMEEND(coredump_writenote)(p, iocookie, &nhdr,
		    "OpenBSD", &wcookie);
		if (error)
			return (error);
	}
	size += notesize;
#endif

	/*
	 * Now write the register info for the thread that caused the
	 * coredump.
	 */
	error = ELFNAMEEND(coredump_note)(p, iocookie, &notesize);
	if (error)
		return (error);
	size += notesize;

	/*
	 * Now, for each thread, write the register info and any other
	 * per-thread notes.  Since we're dumping core, we don't bother
	 * locking.
	 */
	TAILQ_FOREACH(q, &pr->ps_threads, p_thr_link) {
		if (q == p)		/* we've taken care of this thread */
			continue;
		error = ELFNAMEEND(coredump_note)(q, iocookie, &notesize);
		if (error)
			return (error);
		size += notesize;
	}

	*sizep = size;
#endif
	return (0);
}

int
ELFNAMEEND(coredump_note)(struct proc *p, void *iocookie, size_t *sizep)
{
#ifndef SMALL_KERNEL
	Elf_Note nhdr;
	int size, notesize, error;
	int namesize;
	char name[64+ELFROUNDSIZE];
	struct reg intreg;
#ifdef PT_GETFPREGS
	struct fpreg freg;
#endif

	size = 0;

	snprintf(name, sizeof(name)-ELFROUNDSIZE, "%s@%d",
	    "OpenBSD", p->p_pid);
	namesize = strlen(name) + 1;
	memset(name + namesize, 0, elfround(namesize) - namesize);

	notesize = sizeof(nhdr) + elfround(namesize) + elfround(sizeof(intreg));
	if (iocookie) {
		error = process_read_regs(p, &intreg);
		if (error)
			return (error);

		nhdr.namesz = namesize;
		nhdr.descsz = sizeof(intreg);
		nhdr.type = NT_OPENBSD_REGS;

		error = ELFNAMEEND(coredump_writenote)(p, iocookie, &nhdr,
		    name, &intreg);
		if (error)
			return (error);

	}
	size += notesize;

#ifdef PT_GETFPREGS
	notesize = sizeof(nhdr) + elfround(namesize) + elfround(sizeof(freg));
	if (iocookie) {
		error = process_read_fpregs(p, &freg);
		if (error)
			return (error);

		nhdr.namesz = namesize;
		nhdr.descsz = sizeof(freg);
		nhdr.type = NT_OPENBSD_FPREGS;

		error = ELFNAMEEND(coredump_writenote)(p, iocookie, &nhdr,
		    name, &freg);
		if (error)
			return (error);
	}
	size += notesize;
#endif

	*sizep = size;
	/* XXX Add hook for machdep per-LWP notes. */
#endif
	return (0);
}

int
ELFNAMEEND(coredump_writenote)(struct proc *p, void *cookie, Elf_Note *nhdr,
    const char *name, void *data)
{
#ifdef SMALL_KERNEL
	return EPERM;
#else
	int error;

	error = coredump_write(cookie, UIO_SYSSPACE, nhdr, sizeof(*nhdr));
	if (error)
		return error;

	error = coredump_write(cookie, UIO_SYSSPACE, name,
	    elfround(nhdr->namesz));
	if (error)
		return error;

	return coredump_write(cookie, UIO_SYSSPACE, data, nhdr->descsz);
#endif
}
