/*	$OpenBSD: archdep.h,v 1.7 2018/06/16 16:06:03 guenther Exp $	*/

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

#ifndef _M88K_ARCHDEP_H_
#define _M88K_ARCHDEP_H_

#define	RELOC_TAG	DT_RELA

#define	MACHID			EM_88K	/* ELF e_machine ID value checked */

#include <elf.h>
#include <machine/reloc.h>
#include "syscall.h"
#include "util.h"

/*
 *	The following functions are declared inline so they can
 *	be used before bootstrap linking has been finished.
 */

static inline void
RELOC_DYN(Elf32_Rela *r, const Elf32_Sym *s, Elf32_Addr *p, unsigned long v)
{
	if (ELF32_R_TYPE(r->r_info) == RELOC_BBASED_32) {
		*p = v + r->r_addend;
	} else if (ELF32_R_TYPE(r->r_info) == RELOC_32) {
		*p = v + s->st_value + r->r_addend;
	} else if (ELF32_R_TYPE(r->r_info) != RELOC_NONE) {
		_dl_exit(ELF32_R_TYPE(r->r_info) + 100);
	}
}

#define RELOC_GOT(obj, offs)	do { } while (0)

#endif /* _M88K_ARCHDEP_H_ */
