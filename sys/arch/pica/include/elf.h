/*	$NetBSD: elf.h,v 1.2 1995/03/28 18:19:14 jtc Exp $	*/

/*
 * Copyright (c) 1994 Ted Lemon
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __MACHINE_ELF_H__
#define __MACHINE_ELF_H__

/* ELF executable header... */
struct ehdr {
  char elf_magic [4];		/* Elf magic number... */
  unsigned long magic [3];	/* Magic number... */
  unsigned short type;		/* Object file type... */
  unsigned short machine;	/* Machine ID... */
  unsigned long version;	/* File format version... */
  unsigned long entry;		/* Entry point... */
  unsigned long phoff;		/* Program header table offset... */
  unsigned long shoff;		/* Section header table offset... */
  unsigned long flags;		/* Processor-specific flags... */
  unsigned short ehsize;	/* Elf header size in bytes... */
  unsigned short phsize;	/* Program header size... */
  unsigned short phcount;	/* Program header count... */
  unsigned short shsize;	/* Section header size... */
  unsigned short shcount;	/* Section header count... */
  unsigned short shstrndx;	/* Section header string table index... */
};

/* Program header... */
struct phdr {
  unsigned long type;		/* Segment type... */
  unsigned long offset;		/* File offset... */
  unsigned long vaddr;		/* Virtual address... */
  unsigned long paddr;		/* Physical address... */
  unsigned long filesz;		/* Size of segment in file... */
  unsigned long memsz;		/* Size of segment in memory... */
  unsigned long flags;		/* Segment flags... */
  unsigned long align;		/* Alighment, file and memory... */
};

/* Section header... */
struct shdr {
  unsigned long name;		/* Offset into string table of section name */
  unsigned long type;		/* Type of section... */
  unsigned long flags;		/* Section flags... */
  unsigned long addr;		/* Section virtual address at execution... */
  unsigned long offset;		/* Section file offset... */
  unsigned long size;		/* Section size... */
  unsigned long link;		/* Link to another section... */
  unsigned long info;		/* Additional section info... */
  unsigned long align;		/* Section alignment... */
  unsigned long esize;		/* Entry size if section holds table... */
};

/* Symbol table entry... */
struct sym {
  unsigned long name;		/* Index into strtab of symbol name. */
  unsigned long value;		/* Section offset, virt addr or common align. */
  unsigned long size;		/* Size of object referenced. */
  unsigned type    : 4;		/* Symbol type (e.g., function, data)... */
  unsigned binding : 4;		/* Symbol binding (e.g., global, local)... */
  unsigned char other;		/* Unused. */
  unsigned short shndx;		/* Section containing symbol. */
};

/* Values for program header type field */

#define PT_NULL         0               /* Program header table entry unused */
#define PT_LOAD         1               /* Loadable program segment */
#define PT_DYNAMIC      2               /* Dynamic linking information */
#define PT_INTERP       3               /* Program interpreter */
#define PT_NOTE         4               /* Auxiliary information */
#define PT_SHLIB        5               /* Reserved, unspecified semantics */
#define PT_PHDR         6               /* Entry for header table itself */
#define PT_LOPROC       0x70000000      /* Processor-specific */
#define PT_HIPROC       0x7FFFFFFF      /* Processor-specific */
#define PT_MIPS_REGINFO	PT_LOPROC	/* Mips reginfo section... */

/* Program segment permissions, in program header flags field */

#define PF_X            (1 << 0)        /* Segment is executable */
#define PF_W            (1 << 1)        /* Segment is writable */
#define PF_R            (1 << 2)        /* Segment is readable */
#define PF_MASKPROC     0xF0000000      /* Processor-specific reserved bits */

/* Reserved section indices... */
#define SHN_UNDEF	0
#define SHN_ABS		0xfff1
#define SHN_COMMON	0xfff2
#define SHN_MIPS_ACOMMON 0xfff0

/* Symbol bindings... */
#define STB_LOCAL	0
#define STB_GLOBAL	1
#define STB_WEAK	2

/* Symbol types... */
#define STT_NOTYPE	0
#define	STT_OBJECT	1
#define STT_FUNC	2
#define	STT_SECTION	3
#define STT_FILE	4

#define ELF_HDR_SIZE	(sizeof (struct ehdr))
#ifdef _KERNEL
int     pmax_elf_makecmds __P((struct proc *, struct exec_package *));
#endif /* _KERNEL */
#endif /* __MACHINE_ELF_H__ */
