/*	$OpenBSD: archdep.h,v 1.3 2003/02/15 22:43:06 drahn Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _I386_ARCHDEP_H_
#define _I386_ARCHDEP_H_

#define	DL_MALLOC_ALIGN	8	/* Arch constraint or otherwise */

#define	MACHID	EM_386		/* ELF e_machine ID value checked */

#define	RELTYPE	Elf32_Rela
#define	RELSIZE	sizeof(Elf32_Rela)

#include <sys/mman.h>
#include <elf_abi.h>
#include <machine/reloc.h>
#include "syscall.h"
#include "util.h"

static inline void *
_dl_mmap(void *addr, unsigned int len, unsigned int prot,
	unsigned int flags, int fd, off_t offset)
{
	return((void *)_dl__syscall((quad_t)SYS_mmap, addr, len, prot,
		flags, fd, 0, offset));
}

static inline void
RELOC_REL(Elf32_Rel *r, const Elf32_Sym *s, Elf32_Addr *p, unsigned long v)
{

	if (ELF32_R_TYPE(r->r_info) == RELOC_RELATIVE) {
		*p += v;
	} else if (ELF32_R_TYPE(r->r_info) == RELOC_GLOB_DAT) {
		*p += v + s->st_value;
	} else {
		_dl_printf("unknown bootstrap relocation\n");
		_dl_exit(6);
	}
}

static inline void
RELOC_RELA(Elf32_Rela *r, const Elf32_Sym *s, Elf32_Addr *p, unsigned long v)
{
	/* does i386 use RELA type relocations? - XXX */

	if (ELF32_R_TYPE(r->r_info) == RELOC_RELATIVE) {
		*p = v + r->r_addend;
	} else if (ELF32_R_TYPE(r->r_info) == RELOC_GLOB_DAT) {
		*p = v + s->st_value + r->r_addend;
	} else {
		_dl_printf("unknown bootstrap relocation\n");
		_dl_exit(6);
	}
}

#define RELOC_GOT(obj, offs)

#define GOT_PERMS PROT_READ

#endif /* _I386_ARCHDEP_H_ */
