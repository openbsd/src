/*	$OpenBSD: archdep.h,v 1.2 2010/01/02 12:16:35 kettenis Exp $ */

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

#ifndef _SH_ARCHDEP_H_
#define _SH_ARCHDEP_H_

#define	DL_MALLOC_ALIGN	4	/* Arch constraint or otherwise */

#define	MACHID	EM_SH	/* ELF e_machine ID value checked */

#define	RELTYPE	Elf32_Rela
#define	RELSIZE	sizeof(Elf32_Rela)

#include <elf_abi.h>
#include <machine/reloc.h>
#include "syscall.h"
#include "util.h"

/* HACK */
#define DT_PROCNUM 0
#ifndef DT_BIND_NOW
#define DT_BIND_NOW 0
#endif

#define  RTLD_NO_WXORX

/*
 *	The following functions are declared inline so they can
 *	be used before bootstrap linking has been finished.
 */

static inline void *
_dl_mmap(void *addr, unsigned int len, unsigned int prot,
	unsigned int flags, int fd, off_t offset)
{
	return((void *)(long)_dl__syscall((quad_t)SYS_mmap, addr, len, prot,
		flags, fd, 0, offset));
}

static inline void
RELOC_REL(Elf_Rel *r, const Elf_Sym *s, Elf_Addr *p, unsigned long v)
{
	if (ELF_R_TYPE(r->r_info) == R_SH_RELATIVE) {
		*p += v;
	} else {
		/* XXX - printf might not work here, but we give it a shot. */
		_dl_printf("Unknown bootstrap relocation.\n");
		_dl_exit(6);
	}
}

static inline void
RELOC_RELA(Elf32_Rela *r, const Elf32_Sym *s, Elf32_Addr *p, unsigned long v,
    Elf_Addr *pltgot)
{
	if (ELF_R_TYPE(r->r_info) == R_SH_RELATIVE) {
		*p = v + r->r_addend;
	} else {
		/* XXX - printf might not work here, but we give it a shot. */
		_dl_printf("Unknown bootstrap relocation.\n");
		_dl_exit(6);
	}
}

#define RELOC_GOT(obj, offs)

#define GOT_PERMS (PROT_READ|PROT_EXEC)

#endif /* _SH_ARCHDEP_H_ */
