/* This file defines standard ELF types, structures, and macros.
Copyright (C) 1995 Free Software Foundation, Inc.
Contributed by Ian Lance Taylor (ian@cygnus.com).

This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#ifndef _ELF_H
#define	_ELF_H 1


/* Standard ELF types.

   Using __attribute__ mode ensures that gcc will choose the right for
   these types.  */

typedef unsigned int Elf32_Addr    __attribute__ ((mode (SI)));
typedef unsigned int Elf32_Half    __attribute__ ((mode (HI)));
typedef unsigned int Elf32_Off     __attribute__ ((mode (SI)));
typedef		 int Elf32_Sword   __attribute__ ((mode (SI)));
typedef unsigned int Elf32_Word    __attribute__ ((mode (SI)));

/* The ELF file header.  This appears at the start of every ELF file.  */

#define EI_NIDENT (16)

typedef struct
{
  unsigned char	e_ident[EI_NIDENT];	/* Magic number and other info */
  Elf32_Half	e_type;			/* Object file type */
  Elf32_Half	e_machine;		/* Architecture */
  Elf32_Word	e_version;		/* Object file version */
  Elf32_Addr	e_entry;		/* Entry point virtual address */
  Elf32_Off	e_phoff;		/* Program header table file offset */
  Elf32_Off	e_shoff;		/* Section header table file offset */
  Elf32_Word	e_flags;		/* Processor-specific flags */
  Elf32_Half	e_ehsize;		/* ELF header size in bytes */
  Elf32_Half	e_phentsize;		/* Program header table entry size */
  Elf32_Half	e_phnum;		/* Program header table entry count */
  Elf32_Half	e_shentsize;		/* Section header table entry size */
  Elf32_Half	e_shnum;		/* Section header table entry count */
  Elf32_Half	e_shstrndx;		/* Section header string table index */
} Elf32_Ehdr;

/* Fields in the e_ident array.  The EI_* macros are indices into the
   array.  The macros under each EI_* macro are the values the byte
   may have.  */

#define EI_MAG0		0		/* File identification byte 0 index */
#define ELFMAG0		0x7f		/* Magic number byte 0 */

#define EI_MAG1		1		/* File identification byte 1 index */
#define ELFMAG1		'E'		/* Magic number byte 1 */

#define EI_MAG2		2		/* File identification byte 2 index */
#define ELFMAG2		'L'		/* Magic number byte 2 */

#define EI_MAG3		3		/* File identification byte 3 index */
#define ELFMAG3		'F'		/* Magic number byte 3 */

/* Conglomeration of the identification bytes, for easy testing as a word.  */
#define	ELFMAG		"\177ELF"
#define	SELFMAG		4

#define EI_CLASS	4		/* File class byte index */
#define ELFCLASSNONE	0		/* Invalid class */
#define ELFCLASS32	1		/* 32-bit objects */
#define ELFCLASS64	2		/* 64-bit objects */

#define EI_DATA		5		/* Data encoding byte index */
#define ELFDATANONE	0		/* Invalid data encoding */
#define ELFDATA2LSB	1		/* 2's complement, little endian */
#define ELFDATA2MSB	2		/* 2's complement, big endian */

#define EI_VERSION	6		/* File version byte index */
					/* Value must be EV_CURRENT */

#define EI_PAD		7		/* Byte index of padding bytes */

/* Legal values for e_type (object file type).  */

#define ET_NONE		0		/* No file type */
#define ET_REL		1		/* Relocatable file */
#define ET_EXEC		2		/* Executable file */
#define ET_DYN		3		/* Shared object file */
#define ET_CORE		4		/* Core file */
#define	ET_NUM		5		/* Number of defined types.  */
#define ET_LOPROC	0xff00		/* Processor-specific */
#define ET_HIPROC	0xffff		/* Processor-specific */

/* Legal values for e_machine (architecture).  */

#define EM_NONE		0		/* No machine */
#define EM_M32		1		/* AT&T WE 32100 */
#define EM_SPARC	2		/* SUN SPARC */
#define EM_386		3		/* Intel 80386 */
#define EM_68K		4		/* Motorola m68k family */
#define EM_88K		5		/* Motorola m88k family */
#define EM_486		6		/* Intel 80486 */
#define EM_860		7		/* Intel 80860 */
#define EM_MIPS		8		/* MIPS R3000 big-endian */
#define EM_S370		9		/* Amdahl */
#define EM_MIPS_RS4_BE 10		/* MIPS R4000 big-endian */

#define EM_SPARC64     11		/* SPARC v9 (not official) 64-bit */

#define EM_PARISC      15		/* HPPA */

/* If it is necessary to assign new unofficial EM_* values, please
   pick large random numbers (0x8523, 0xa7f2, etc.) to minimize the
   chances of collision with official or non-GNU unofficial values.  */

/* Legal values for e_version (version).  */

#define EV_NONE		0		/* Invalid ELF version */
#define EV_CURRENT	1		/* Current version */

/* Section header.  */

typedef struct
{
  Elf32_Word	sh_name;		/* Section name (string tbl index) */
  Elf32_Word	sh_type;		/* Section type */
  Elf32_Word	sh_flags;		/* Section flags */
  Elf32_Addr	sh_addr;		/* Section virtual addr at execution */
  Elf32_Off	sh_offset;		/* Section file offset */
  Elf32_Word	sh_size;		/* Section size in bytes */
  Elf32_Word	sh_link;		/* Link to another section */
  Elf32_Word	sh_info;		/* Additional section information */
  Elf32_Word	sh_addralign;		/* Section alignment */
  Elf32_Word	sh_entsize;		/* Entry size if section holds table */
} Elf32_Shdr;

/* Special section indices.  */

#define SHN_UNDEF	0		/* Undefined section */
#define SHN_LORESERVE	0xff00		/* Start of reserved indices */
#define SHN_LOPROC	0xff00		/* Start of processor-specific */
#define SHN_HIPROC	0xff1f		/* End of processor-specific */
#define SHN_ABS		0xfff1		/* Associated symbol is absolute */
#define SHN_COMMON	0xfff2		/* Associated symbol is common */
#define SHN_HIRESERVE	0xffff		/* End of reserved indices */

/* Legal values for sh_type (section type).  */

#define SHT_NULL	0		/* Section header table entry unused */
#define SHT_PROGBITS	1		/* Program data */
#define SHT_SYMTAB	2		/* Symbol table */
#define SHT_STRTAB	3		/* String table */
#define SHT_RELA	4		/* Relocation entries with addends */
#define SHT_HASH	5		/* Symbol hash table */
#define SHT_DYNAMIC	6		/* Dynamic linking information */
#define SHT_NOTE	7		/* Notes */
#define SHT_NOBITS	8		/* Program space with no data (bss) */
#define SHT_REL		9		/* Relocation entries, no addends */
#define SHT_SHLIB	10		/* Reserved */
#define SHT_DYNSYM	11		/* Dynamic linker symbol table */
#define	SHT_NUM		12		/* Number of defined types.  */
#define SHT_LOPROC	0x70000000	/* Start of processor-specific */
#define SHT_HIPROC	0x7fffffff	/* End of processor-specific */
#define SHT_LOUSER	0x80000000	/* Start of application-specific */
#define SHT_HIUSER	0x8fffffff	/* End of application-specific */

/* Legal values for sh_flags (section flags).  */

#define SHF_WRITE	(1 << 0)	/* Writable */
#define SHF_ALLOC	(1 << 1)	/* Occupies memory during execution */
#define SHF_EXECINSTR	(1 << 2)	/* Executable */
#define SHF_MASKPROC	0xf0000000	/* Processor-specific */

/* Symbol table entry.  */

typedef struct
{
  Elf32_Word	st_name;		/* Symbol name (string tbl index) */
  Elf32_Addr	st_value;		/* Symbol value */
  Elf32_Word	st_size;		/* Symbol size */
  unsigned char	st_info;		/* Symbol type and binding */
  unsigned char	st_other;		/* No defined meaning, 0 */
  Elf32_Half	st_shndx;		/* Section index */
} Elf32_Sym;

/* Special section index.  */

#define SHN_UNDEF	0		/* No section, undefined symbol.  */

/* How to extract and insert information held in the st_info field.  */

#define ELF32_ST_BIND(val)		(((unsigned char) (val)) >> 4)
#define ELF32_ST_TYPE(val)		((val) & 0xf)
#define ELF32_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))

/* Legal values for ST_BIND subfield of st_info (symbol binding).  */

#define STB_LOCAL	0		/* Local symbol */
#define STB_GLOBAL	1		/* Global symbol */
#define STB_WEAK	2		/* Weak symbol */
#define	STB_NUM		3		/* Number of defined types.  */
#define STB_LOPROC	13		/* Start of processor-specific */
#define STB_HIPROC	15		/* End of processor-specific */

/* Legal values for ST_TYPE subfield of st_info (symbol type).  */

#define STT_NOTYPE	0		/* Symbol type is unspecified */
#define STT_OBJECT	1		/* Symbol is a data object */
#define STT_FUNC	2		/* Symbol is a code object */
#define STT_SECTION	3		/* Symbol associated with a section */
#define STT_FILE	4		/* Symbol's name is file name */
#define	STT_NUM		5		/* Number of defined types.  */
#define STT_LOPROC	13		/* Start of processor-specific */
#define STT_HIPROC	15		/* End of processor-specific */


/* Symbol table indices are found in the hash buckets and chain table
   of a symbol hash table section.  This special index value indicates
   the end of a chain, meaning no further symbols are found in that bucket.  */

#define STN_UNDEF	0		/* End of a chain.  */


/* Relocation table entry without addend (in section of type SHT_REL).  */

typedef struct
{
  Elf32_Addr	r_offset;		/* Address */
  Elf32_Word	r_info;			/* Relocation type and symbol index */
} Elf32_Rel;

/* Relocation table entry with addend (in section of type SHT_RELA).  */

typedef struct
{
  Elf32_Addr	r_offset;		/* Address */
  Elf32_Word	r_info;			/* Relocation type and symbol index */
  Elf32_Sword	r_addend;		/* Addend */
} Elf32_Rela;

/* How to extract and insert information held in the r_info field.  */

#define ELF32_R_SYM(val)		((val) >> 8)
#define ELF32_R_TYPE(val)		((val) & 0xff)
#define ELF32_R_INFO(sym, type)		(((sym) << 8) + ((type) & 0xff))

/* Program segment header.  */

typedef struct {
  Elf32_Word	p_type;			/* Segment type */
  Elf32_Off	p_offset;		/* Segment file offset */
  Elf32_Addr	p_vaddr;		/* Segment virtual address */
  Elf32_Addr	p_paddr;		/* Segment physical address */
  Elf32_Word	p_filesz;		/* Segment size in file */
  Elf32_Word	p_memsz;		/* Segment size in memory */
  Elf32_Word	p_flags;		/* Segment flags */
  Elf32_Word	p_align;		/* Segment alignment */
} Elf32_Phdr;

/* Legal values for p_type (segment type).  */

#define	PT_NULL		0		/* Program header table entry unused */
#define PT_LOAD		1		/* Loadable program segment */
#define PT_DYNAMIC	2		/* Dynamic linking information */
#define PT_INTERP	3		/* Program interpreter */
#define PT_NOTE		4		/* Auxiliary information */
#define PT_SHLIB	5		/* Reserved */
#define PT_PHDR		6		/* Entry for header table itself */
#define	PT_NUM		7		/* Number of defined types.  */
#define PT_LOPROC	0x70000000	/* Start of processor-specific */
#define PT_HIPROC	0x7fffffff	/* End of processor-specific */

/* Legal values for p_flags (segment flags).  */

#define PF_X		(1 << 0)	/* Segment is executable */
#define PF_W		(1 << 1)	/* Segment is writable */
#define PF_R		(1 << 2)	/* Segment is readable */
#define PF_MASKPROC	0xf0000000	/* Processor-specific */

#define LXFLAGS(X) ( (((X) & PF_R) ? PROT_READ : 0) | \
                    (((X) & PF_W) ? PROT_WRITE : 0) | \
                    (((X) & PF_X) ? PROT_EXEC : 0))

/* Dynamic section entry.  */

typedef struct
{
  Elf32_Sword	d_tag;			/* Dynamic entry type */
  union
    {
      Elf32_Word d_val;			/* Integer value */
      Elf32_Addr d_ptr;			/* Address value */
    } d_un;
} Elf32_Dyn;

/* Legal values for d_tag (dynamic entry type).  */

#define DT_NULL		0		/* Marks end of dynamic section */
#define DT_NEEDED	1		/* Name of needed library */
#define DT_PLTRELSZ	2		/* Size in bytes of PLT relocs */
#define DT_PLTGOT	3		/* Processor defined value */
#define DT_HASH		4		/* Address of symbol hash table */
#define DT_STRTAB	5		/* Address of string table */
#define DT_SYMTAB	6		/* Address of symbol table */
#define DT_RELA		7		/* Address of Rela relocs */
#define DT_RELASZ	8		/* Total size of Rela relocs */
#define DT_RELAENT	9		/* Size of one Rela reloc */
#define DT_STRSZ	10		/* Size of string table */
#define DT_SYMENT	11		/* Size of one symbol table entry */
#define DT_INIT		12		/* Address of init function */
#define DT_FINI		13		/* Address of termination function */
#define DT_SONAME	14		/* Name of shared object */
#define DT_RPATH	15		/* Library search path */
#define DT_SYMBOLIC	16		/* Start symbol search here */
#define DT_REL		17		/* Address of Rel relocs */
#define DT_RELSZ	18		/* Total size of Rel relocs */
#define DT_RELENT	19		/* Size of one Rel reloc */
#define DT_PLTREL	20		/* Type of reloc in PLT */
#define DT_DEBUG	21		/* For debugging; unspecified */
#define DT_TEXTREL	22		/* Reloc might modify .text */
#define DT_JMPREL	23		/* Address of PLT relocs */
#define	DT_NUM		24		/* Number used.  */
#define DT_LOPROC	0x70000000	/* Start of processor-specific */
#define DT_HIPROC	0x7fffffff	/* End of processor-specific */

/* Standard 64 bit ELF types.  */

typedef unsigned int Elf64_Addr    __attribute__ ((mode (DI)));
typedef unsigned int Elf64_Half    __attribute__ ((mode (HI)));
typedef unsigned int Elf64_Off     __attribute__ ((mode (DI)));
typedef		 int Elf64_Sword   __attribute__ ((mode (SI)));
typedef		 int Elf64_Sxword  __attribute__ ((mode (DI)));
typedef unsigned int Elf64_Word    __attribute__ ((mode (SI)));
typedef unsigned int Elf64_Xword   __attribute__ ((mode (DI)));
typedef unsigned int Elf64_Byte    __attribute__ ((mode (QI)));
typedef unsigned int Elf64_Section __attribute__ ((mode (HI)));

/* 64 bit ELF file header.  */

typedef struct
{
  unsigned char	e_ident[EI_NIDENT];	/* Magic number and other info */
  Elf64_Half	e_type;			/* Object file type */
  Elf64_Half	e_machine;		/* Architecture */
  Elf64_Word	e_version;		/* Object file version */
  Elf64_Addr	e_entry;		/* Entry point virtual address */
  Elf64_Off	e_phoff;		/* Program header table file offset */
  Elf64_Off	e_shoff;		/* Section header table file offset */
  Elf64_Word	e_flags;		/* Processor-specific flags */
  Elf64_Half	e_ehsize;		/* ELF header size in bytes */
  Elf64_Half	e_phentsize;		/* Program header table entry size */
  Elf64_Half	e_phnum;		/* Program header table entry count */
  Elf64_Half	e_shentsize;		/* Section header table entry size */
  Elf64_Half	e_shnum;		/* Section header table entry count */
  Elf64_Half	e_shstrndx;		/* Section header string table index */
} Elf64_Ehdr;

/* 64 bit section header.  */

typedef struct
{
  Elf64_Word	sh_name;		/* Section name (string tbl index) */
  Elf64_Word	sh_type;		/* Section type */
  Elf64_Xword	sh_flags;		/* Section flags */
  Elf64_Addr	sh_addr;		/* Section virtual addr at execution */
  Elf64_Off	sh_offset;		/* Section file offset */
  Elf64_Xword	sh_size;		/* Section size in bytes */
  Elf64_Word	sh_link;		/* Link to another section */
  Elf64_Word	sh_info;		/* Additional section information */
  Elf64_Xword	sh_addralign;		/* Section alignment */
  Elf64_Xword	sh_entsize;		/* Entry size if section holds table */
} Elf64_Shdr;

/* 64 bit symbol table entry.  */

typedef struct
{
  Elf64_Word	st_name;		/* Symbol name (string tbl index) */
  Elf64_Byte	st_info;		/* Symbol type and binding */
  Elf64_Byte	st_other;		/* No defined meaning, 0 */
  Elf64_Section	st_shndx;		/* Section index */
  Elf64_Addr	st_value;		/* Symbol value */
  Elf64_Xword	st_size;		/* Symbol size */
} Elf64_Sym;

/* The 64 bit st_info field is the same as the 32 bit one.  */

#define ELF64_ST_BIND(val)		(((unsigned char) (val)) >> 4)
#define ELF64_ST_TYPE(val)		((val) & 0xf)
#define ELF64_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))

/* I have seen two different definitions of the Elf64_Rel and
   Elf64_Rela structures, so we'll leave them out until Novell (or
   whoever) gets their act together.  */

/* Auxiliary vector.  */

/* This vector is normally only used by the program interpreter.  The
   usual definition in an ABI supplement uses the name auxv_t.  The
   vector is not usually defined in a standard <elf.h> file, but it
   can't hurt.  We rename it to avoid conflicts.  The sizes of these
   types are an arrangement between the exec server and the program
   interpreter, so we don't fully specify them here.  */

typedef struct
{
  int a_type;				/* Entry type */
  union
    {
      long a_val;			/* Integer value */
      void *a_ptr;			/* Pointer value */
      void (*a_fcn) ();			/* Function pointer value */
    } a_un;
} Elf32_auxv_t;

/* Legal values for a_type (entry type).  */

#define AT_NULL		0		/* End of vector */
#define AT_IGNORE	1		/* Entry should be ignored */
#define AT_EXECFD	2		/* File descriptor of program */
#define AT_PHDR		3		/* Program headers for program */
#define AT_PHENT	4		/* Size of program header entry */
#define AT_PHNUM	5		/* Number of program headers */
#define AT_PAGESZ	6		/* System page size */
#define AT_BASE		7		/* Base address of interpreter */
#define AT_FLAGS	8		/* Flags */
#define AT_ENTRY	9		/* Entry point of program */
#define AT_NOTELF	10		/* Program is not ELF */
#define AT_UID		11		/* Real uid */
#define AT_EUID		12		/* Effective uid */
#define AT_GID		13		/* Real gid */
#define AT_EGID		14		/* Effective gid */

/* Intel 80386 specific definitions.  */

/* i386 relocs.  */

#define R_386_NONE	0		/* No reloc */
#define R_386_32	1		/* Direct 32 bit  */
#define R_386_PC32	2		/* PC relative 32 bit */
#define R_386_GOT32	3		/* 32 bit GOT entry */
#define R_386_PLT32	4		/* 32 bit PLT address */
#define R_386_COPY	5		/* Copy symbol at runtime */
#define R_386_GLOB_DAT	6		/* Create GOT entry */
#define R_386_JMP_SLOT	7		/* Create PLT entry */
#define R_386_RELATIVE	8		/* Adjust by program base */
#define R_386_GOTOFF	9		/* 32 bit offset to GOT */
#define R_386_GOTPC	10		/* 32 bit PC relative offset to GOT */

/* SUN SPARC specific definitions.  */

/* SPARC relocs.  */

#define R_SPARC_NONE	0		/* No reloc */
#define R_SPARC_8	1		/* Direct 8 bit */
#define R_SPARC_16	2		/* Direct 16 bit */
#define R_SPARC_32	3		/* Direct 32 bit */
#define R_SPARC_DISP8	4		/* PC relative 8 bit */
#define R_SPARC_DISP16	5		/* PC relative 16 bit */
#define R_SPARC_DISP32	6		/* PC relative 32 bit */
#define R_SPARC_WDISP30	7		/* PC relative 30 bit shifted */
#define R_SPARC_WDISP22	8		/* PC relative 22 bit shifted */
#define R_SPARC_HI22	9		/* High 22 bit */
#define R_SPARC_22	10		/* Direct 22 bit */
#define R_SPARC_13	11		/* Direct 13 bit */
#define R_SPARC_LO10	12		/* Truncated 10 bit */
#define R_SPARC_GOT10	13		/* Truncated 10 bit GOT entry */
#define R_SPARC_GOT13	14		/* 13 bit GOT entry */
#define R_SPARC_GOT22	15		/* 22 bit GOT entry shifted */
#define R_SPARC_PC10	16		/* PC relative 10 bit truncated */
#define R_SPARC_PC22	17		/* PC relative 22 bit shifted */
#define R_SPARC_WPLT30	18		/* 30 bit PC relative PLT address */
#define R_SPARC_COPY	19		/* Copy symbol at runtime */
#define R_SPARC_GLOB_DAT 20		/* Create GOT entry */
#define R_SPARC_JMP_SLOT 21		/* Create PLT entry */
#define R_SPARC_RELATIVE 22		/* Adjust by program base */
#define R_SPARC_UA32	23		/* Direct 32 bit unaligned */

/* MIPS R3000 specific definitions.  */

/* Legal values for e_flags field of Elf32_Ehdr.  */

#define EF_MIPS_NOREORDER 1		/* A .noreorder directive was used */
#define EF_MIPS_PIC	  2		/* Contains PIC code */
#define EF_MIPS_CPIC	  4		/* Uses PIC calling sequence */
#define EF_MIPS_ARCH	  0xf0000000	/* MIPS architecture level */

/* Special section indices.  */

#define SHN_MIPS_ACOMMON 0xff00		/* Allocated common symbols */
#define SHN_MIPS_SCOMMON 0xff03		/* Small common symbols */
#define SHN_MIPS_SUNDEFINED 0xff04	/* Small undefined symbols */

/* Legal values for sh_type field of Elf32_Shdr.  */

#define SHT_MIPS_LIBLIST  0x70000000	/* Shared objects used in link */
#define SHT_MIPS_CONFLICT 0x70000002	/* Conflicting symbols */
#define SHT_MIPS_GPTAB	  0x70000003	/* Global data area sizes */
#define SHT_MIPS_UCODE	  0x70000004	/* Reserved for SGI/MIPS compilers */
#define SHT_MIPS_DEBUG	  0x70000005	/* MIPS ECOFF debugging information */
#define SHT_MIPS_REGINFO  0x70000006	/* Register usage information */

/* Legal values for sh_flags field of Elf32_Shdr.  */

#define SHF_MIPS_GPREL	0x10000000	/* Must be part of global data area */

/* Entries found in sections of type SHT_MIPS_GPTAB.  */

typedef union
{
  struct
    {
      Elf32_Word gt_current_g_value;	/* -G value used for compilation */
      Elf32_Word gt_unused;		/* Not used */
    } gt_header;			/* First entry in section */
  struct
    {
      Elf32_Word gt_g_value;		/* If this value were used for -G */
      Elf32_Word gt_bytes;		/* This many bytes would be used */
    } gt_entry;				/* Subsequent entries in section */
} Elf32_gptab;

/* Entry found in sections of type SHT_MIPS_REGINFO.  */

typedef struct
{
  Elf32_Word	ri_gprmask;		/* General registers used */
  Elf32_Word	ri_cprmask[4];		/* Coprocessor registers used */
  Elf32_Sword	ri_gp_value;		/* $gp register value */
} Elf32_RegInfo;

/* MIPS relocs.  */

#define R_MIPS_NONE	0		/* No reloc */
#define R_MIPS_16	1		/* Direct 16 bit */
#define R_MIPS_32	2		/* Direct 32 bit */
#define R_MIPS_REL32	3		/* PC relative 32 bit */
#define R_MIPS_26	4		/* Direct 26 bit shifted */
#define R_MIPS_HI16	5		/* High 16 bit */
#define R_MIPS_LO16	6		/* Low 16 bit */
#define R_MIPS_GPREL16	7		/* GP relative 16 bit */
#define R_MIPS_LITERAL	8		/* 16 bit literal entry */
#define R_MIPS_GOT16	9		/* 16 bit GOT entry */
#define R_MIPS_PC16	10		/* PC relative 16 bit */
#define R_MIPS_CALL16	11		/* 16 bit GOT entry for function */
#define R_MIPS_GPREL32	12		/* GP relative 32 bit */

/* Legal values for p_type field of Elf32_Phdr.  */

#define PT_MIPS_REGINFO	0x70000000	/* Register usage information */

/* Legal values for d_tag field of Elf32_Dyn.  */

#define DT_MIPS_RLD_VERSION  0x70000001	/* Runtime linker interface version */
#define DT_MIPS_TIME_STAMP   0x70000002	/* Timestamp */
#define DT_MIPS_ICHECKSUM    0x70000003	/* Checksum */
#define DT_MIPS_IVERSION     0x70000004	/* Version string (string tbl index) */
#define DT_MIPS_FLAGS	     0x70000005	/* Flags */
#define DT_MIPS_BASE_ADDRESS 0x70000006	/* Base address */
#define DT_MIPS_CONFLICT     0x70000008	/* Address of CONFLICT section */
#define DT_MIPS_LIBLIST	     0x70000009	/* Address of LIBLIST section */
#define DT_MIPS_LOCAL_GOTNO  0x7000000a	/* Number of local GOT entries */
#define DT_MIPS_CONFLICTNO   0x7000000b	/* Number of CONFLICT entries */
#define DT_MIPS_LIBLISTNO    0x70000010	/* Number of LIBLIST entries */
#define DT_MIPS_SYMTABNO     0x70000011	/* Number of DYNSYM entries */
#define DT_MIPS_UNREFEXTNO   0x70000012	/* First external DYNSYM */
#define DT_MIPS_GOTSYM	     0x70000013	/* First GOT entry in DYNSYM */
#define DT_MIPS_HIPAGENO     0x70000014	/* Number of GOT page table entries */
#define DT_MIPS_RLD_MAP      0x70000016	/* Address of debug map pointer */

/* Legal values for DT_MIPS_FLAG Elf32_Dyn entry.  */

#define RHF_NONE		   0		/* No flags */
#define RHF_QUICKSTART		   (1 << 0)	/* Use quickstart */
#define RHF_NOTPOT		   (1 << 1)	/* Hash size not power of 2 */
#define RHF_NO_LIBRARY_REPLACEMENT (1 << 2)	/* Ignore LD_LIBRARY_PATH */

/* Entries found in sections of type SHT_MIPS_LIBLIST.  */

typedef struct
{
  Elf32_Word	l_name;			/* Name (string table index) */
  Elf32_Word	l_time_stamp;		/* Timestamp */
  Elf32_Word	l_checksum;		/* Checksum */
  Elf32_Word	l_version;		/* Interface version */
  Elf32_Word	l_flags;		/* Flags */
} Elf32_Lib;

/* Legal values for l_flags.  */

#define LL_EXACT_MATCH	  (1 << 0)	/* Require exact match */
#define LL_IGNORE_INT_VER (1 << 1)	/* Ignore interface version */

/* Entries found in sections of type SHT_MIPS_CONFLICT.  */

typedef Elf32_Addr Elf32_Conflict;


#endif	/* elf.h */
