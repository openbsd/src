/*	$NetBSD: exec_elf.h,v 1.2 1996/01/16 23:19:43 fvdl Exp $	*/

/*
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
 */

#ifndef _SYS_EXEC_ELF_H_
#define _SYS_EXEC_ELF_H_

#include <machine/types.h>

typedef u_int32_t Elf32_Addr;
typedef u_int32_t Elf32_Off;
typedef int32_t   Elf32_Sword;
typedef u_int32_t Elf32_Word;
typedef u_int16_t Elf32_Half;


#define ELF_IDSIZE	16

enum Elf32_e_type {
	Elf32_et_none = 0,
	Elf32_et_rel,
	Elf32_et_exec,
	Elf32_et_dyn,
	Elf32_et_core,
	Elf32_et_num
};

enum Elf32_e_machine {
	Elf32_em_none = 0,
	Elf32_em_m32,
	Elf32_em_sparc,
	Elf32_em_386,
	Elf32_em_68k,
	Elf32_em_88k,
	Elf32_em_486,
	Elf32_em_860,
	Elf32_em_mips,
	Elf32_em_num
};

typedef struct {
	unsigned char	e_ident[ELF_IDSIZE];	/* Id bytes */
	Elf32_Half	e_type;			/* file type */
	Elf32_Half	e_machine;		/* machine type */
	Elf32_Word	e_version;		/* version number */
	Elf32_Addr	e_entry;		/* entry point */
	Elf32_Off	e_phoff;		/* Program hdr offset */
	Elf32_Off	e_shoff;		/* Section hdr offset */
	Elf32_Word	e_flags;		/* Processor flags */
	Elf32_Half      e_ehsize;		/* sizeof ehdr */
	Elf32_Half      e_phentsize;		/* Program header entry size */
	Elf32_Half      e_phnum;		/* Number of program headers */
	Elf32_Half      e_shentsize;		/* Section header entry size */
	Elf32_Half      e_shnum;		/* Number of section headers */
	Elf32_Half      e_shstrndx;		/* String table index */
} Elf32_Ehdr;


enum Elf32_p_pf {
	Elf32_pf_r = 4,
	Elf32_pf_w = 2,
	Elf32_pf_x = 1
};

enum Elf32_p_pt {
	Elf32_pt_nulli = 0,		/* Program header table entry unused */
	Elf32_pt_load = 1,		/* Loadable program segment */
	Elf32_pt_dynamic = 2,		/* Dynamic linking information */
	Elf32_pt_interp = 3,		/* Program interpreter */
	Elf32_pt_note = 4,		/* Auxiliary information */
	Elf32_pt_shlib = 5,		/* Reserved, unspecified semantics */
	Elf32_pt_phdr = 6,		/* Entry for header table itself */
	Elf32_pt_loproc = 0x70000000,	/* Processor-specific */
	Elf32_pt_hiproc = 0x7FFFFFFF	/* Processor-specific */

};

typedef struct {
	Elf32_Word	p_type;		/* entry type */
	Elf32_Off	p_offset;	/* offset */
	Elf32_Addr	p_vaddr;	/* virtual address */
	Elf32_Addr	p_paddr;	/* physical address */
	Elf32_Word	p_filesz;	/* file size */
	Elf32_Word	p_memsz;	/* memory size */
	Elf32_Word	p_flags;	/* flags */
	Elf32_Word	p_align;	/* memory & file alignment */
} Elf32_Phdr;

#define Elf32_e_ident "\177ELF"
#define Elf32_e_siz (sizeof(Elf32_e_ident) - 1)

typedef struct {
	int		au_id;
	unsigned long	au_v;
} AuxInfo;

enum AuxID {
	AUX_null = 0,
	AUX_ignore = 1,
	AUX_execfd = 2,
	AUX_phdr = 3,		/* &phdr[0] */
	AUX_phent = 4,		/* sizeof(phdr[0]) */
	AUX_phnum = 5,		/* # phdr entries */
	AUX_pagesz = 6,		/* PAGESIZE */
	AUX_base = 7,		/* ld.so base addr */
	AUX_flags = 8,		/* processor flags */
	AUX_entry = 9,		/* a.out entry */
	AUX_sun_uid = 2000,	/* euid */
	AUX_sun_ruid = 2001,	/* ruid */
	AUX_sun_gid = 2002,	/* egid */
	AUX_sun_rgid = 2003	/* rgid */
};

#define	ELF_HDR_SIZE	(sizeof(Elf32_Ehdr))


#ifdef _KERNEL

#define ELF32_NO_ADDR	((u_long) ~0)	/* Indicates addr. not yet filled in */
#define ELF_AUX_ENTRIES	8		/* Size of aux array passed to loader */

struct elf_args {
        u_long  arg_entry;      /* progran entry point */
        u_long  arg_interp;     /* Interpreter load address */
        u_long  arg_phaddr;     /* program header address */
        u_long  arg_phentsize;  /* Size of program header */
        u_long  arg_phnum;      /* Number of program headers */
};

int exec_elf_makecmds __P((struct proc *, struct exec_package *));
void *elf_copyargs __P((struct exec_package *, struct ps_strings *, void *,
	void *));
void *elf_copyargs __P((struct exec_package *, struct ps_strings *,
        void *, void *));

#endif /* _KERNEL */

#endif /* !_SYS_EXEC_ELF_H_ */
