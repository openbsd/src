/*	$OpenBSD: archdep.h,v 1.17 2004/05/24 20:16:12 drahn Exp $ */

/*
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _SPARC64_ARCHDEP_H_
#define _SPARC64_ARCHDEP_H_

#define	DL_MALLOC_ALIGN	8	/* Arch constraint or otherwise */

#define	MACHID	EM_SPARCV9	/* ELF e_machine ID value checked */

#define	RELTYPE	Elf64_Rela
#define	RELSIZE	sizeof(Elf64_Rela)

#include <elf_abi.h>
#include <machine/exec.h>
#include <machine/reloc.h>
#include <sys/syscall.h>
#include "syscall.h"
#include "util.h"

#define RTLD_PROTECT_PLT

static inline void *
_dl_mmap(void *addr, unsigned int len, unsigned int prot,
	unsigned int flags, int fd, off_t offset)
{
	return((void *)_dl__syscall((quad_t)SYS_mmap, addr, len, prot,
		flags, fd, 0, offset));
}

static inline void
RELOC_REL(Elf_Rel *r, const Elf_Sym *s, Elf_Addr *p, unsigned long v)
{
	/* SPARC64 does not use REL type relocations */
	_dl_exit(20);
}

static inline void
RELOC_RELA(Elf_RelA *r, const Elf_Sym *s, Elf_Addr *p, unsigned long v)
{
	if (ELF_R_TYPE(r->r_info) == RELOC_RELATIVE) {
		*p = v + r->r_addend;
	} else {
		/* XXX - printf might not work here, but we give it a shot. */
		_dl_printf("Unknown bootstrap relocation.\n");
		_dl_exit(6);
	}
}

#define RELOC_GOT(obj, offs)

#define GOT_PERMS PROT_READ

#endif /* _SPARC64_ARCHDEP_H_ */
