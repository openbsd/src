/* PowerPC-specific support for 32-bit ELF
   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* This file is based on a preliminary PowerPC ELF ABI.  The
   information may not match the final PowerPC ELF ABI.  It includes
   suggestions from the in-progress Embedded PowerPC ABI, and that
   information may also not match.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/ppc.h"
#include "elf32-ppc.h"

/* RELA relocations are used here.  */

static struct bfd_hash_entry *ppc_elf_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *entry, struct bfd_hash_table *table,
	   const char *string));
static struct bfd_link_hash_table *ppc_elf_link_hash_table_create
  PARAMS ((bfd *abfd));
static void ppc_elf_copy_indirect_symbol
  PARAMS ((struct elf_backend_data *bed, struct elf_link_hash_entry *dir,
	   struct elf_link_hash_entry *ind));
static reloc_howto_type *ppc_elf_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void ppc_elf_info_to_howto
  PARAMS ((bfd *abfd, arelent *cache_ptr, Elf_Internal_Rela *dst));
static void ppc_elf_howto_init
  PARAMS ((void));
static int ppc_elf_sort_rela
  PARAMS ((const PTR, const PTR));
static bfd_boolean ppc_elf_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, bfd_boolean *));
static bfd_reloc_status_type ppc_elf_addr16_ha_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type ppc_elf_unhandled_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_boolean ppc_elf_object_p
  PARAMS ((bfd *));
static bfd_boolean ppc_elf_set_private_flags
  PARAMS ((bfd *, flagword));
static bfd_boolean ppc_elf_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static int ppc_elf_additional_program_headers
  PARAMS ((bfd *));
static bfd_boolean ppc_elf_modify_segment_map
  PARAMS ((bfd *));
static bfd_boolean ppc_elf_create_got
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean ppc_elf_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean ppc_elf_section_from_shdr
  PARAMS ((bfd *, Elf_Internal_Shdr *, const char *));
static bfd_boolean ppc_elf_fake_sections
  PARAMS ((bfd *, Elf_Internal_Shdr *, asection *));
static elf_linker_section_t *ppc_elf_create_linker_section
  PARAMS ((bfd *abfd, struct bfd_link_info *info,
	   enum elf_linker_section_enum));
static bfd_boolean update_local_sym_info
  PARAMS ((bfd *, Elf_Internal_Shdr *, unsigned long, int));
static void bad_shared_reloc
  PARAMS ((bfd *, enum elf_ppc_reloc_type));
static bfd_boolean ppc_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static asection *ppc_elf_gc_mark_hook
  PARAMS ((asection *sec, struct bfd_link_info *info, Elf_Internal_Rela *rel,
	   struct elf_link_hash_entry *h, Elf_Internal_Sym *sym));
static bfd_boolean ppc_elf_gc_sweep_hook
  PARAMS ((bfd *abfd, struct bfd_link_info *info, asection *sec,
	   const Elf_Internal_Rela *relocs));
static bfd_boolean ppc_elf_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static bfd_boolean allocate_dynrelocs
  PARAMS ((struct elf_link_hash_entry *, PTR));
static bfd_boolean readonly_dynrelocs
  PARAMS ((struct elf_link_hash_entry *, PTR));
static bfd_boolean ppc_elf_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean ppc_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *info, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *relocs, Elf_Internal_Sym *local_syms,
	   asection **));
static bfd_boolean ppc_elf_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));
static bfd_boolean ppc_elf_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static bfd_boolean ppc_elf_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static enum elf_reloc_type_class ppc_elf_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));
static bfd_boolean ppc_elf_grok_prstatus
  PARAMS ((bfd *abfd, Elf_Internal_Note *note));
static bfd_boolean ppc_elf_grok_psinfo
  PARAMS ((bfd *abfd, Elf_Internal_Note *note));

/* Branch prediction bit for branch taken relocs.  */
#define BRANCH_PREDICT_BIT 0x200000
/* Mask to set RA in memory instructions.  */
#define RA_REGISTER_MASK 0x001f0000
/* Value to shift register by to insert RA.  */
#define RA_REGISTER_SHIFT 16

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */
#define ELF_DYNAMIC_INTERPRETER "/usr/lib/ld.so.1"

/* The size in bytes of an entry in the procedure linkage table.  */
#define PLT_ENTRY_SIZE 12
/* The initial size of the plt reserved for the dynamic linker.  */
#define PLT_INITIAL_ENTRY_SIZE 72
/* The size of the gap between entries in the PLT.  */
#define PLT_SLOT_SIZE 8
/* The number of single-slot PLT entries (the rest use two slots).  */
#define PLT_NUM_SINGLE_ENTRIES 8192

/* Some nop instructions.  */
#define NOP		0x60000000
#define CROR_151515	0x4def7b82
#define CROR_313131	0x4ffffb82

/* Offset of tp and dtp pointers from start of TLS block.  */
#define TP_OFFSET	0x7000
#define DTP_OFFSET	0x8000

/* Will references to this symbol always reference the symbol
   in this object?  STV_PROTECTED is excluded from the visibility test
   here so that function pointer comparisons work properly.  Since
   function symbols not defined in an app are set to their .plt entry,
   it's necessary for shared libs to also reference the .plt even
   though the symbol is really local to the shared lib.  */
#define SYMBOL_REFERENCES_LOCAL(INFO, H)				\
  ((! INFO->shared							\
    || INFO->symbolic							\
    || H->dynindx == -1							\
    || ELF_ST_VISIBILITY (H->other) == STV_INTERNAL			\
    || ELF_ST_VISIBILITY (H->other) == STV_HIDDEN)			\
   && (H->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0)

/* Will _calls_ to this symbol always call the version in this object?  */
#define SYMBOL_CALLS_LOCAL(INFO, H)					\
  ((! INFO->shared							\
    || INFO->symbolic							\
    || H->dynindx == -1							\
    || ELF_ST_VISIBILITY (H->other) != STV_DEFAULT)			\
   && (H->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0)

/* The PPC linker needs to keep track of the number of relocs that it
   decides to copy as dynamic relocs in check_relocs for each symbol.
   This is so that it can later discard them if they are found to be
   unnecessary.  We store the information in a field extending the
   regular ELF linker hash table.  */

struct ppc_elf_dyn_relocs
{
  struct ppc_elf_dyn_relocs *next;

  /* The input section of the reloc.  */
  asection *sec;

  /* Total number of relocs copied for the input section.  */
  bfd_size_type count;

  /* Number of pc-relative relocs copied for the input section.  */
  bfd_size_type pc_count;
};

/* PPC ELF linker hash entry.  */

struct ppc_elf_link_hash_entry
{
  struct elf_link_hash_entry elf;

  /* Track dynamic relocs copied for this symbol.  */
  struct ppc_elf_dyn_relocs *dyn_relocs;

  /* Contexts in which symbol is used in the GOT (or TOC).
     TLS_GD .. TLS_TLS bits are or'd into the mask as the
     corresponding relocs are encountered during check_relocs.
     tls_optimize clears TLS_GD .. TLS_TPREL when optimizing to
     indicate the corresponding GOT entry type is not needed.  */
#define TLS_GD		 1	/* GD reloc. */
#define TLS_LD		 2	/* LD reloc. */
#define TLS_TPREL	 4	/* TPREL reloc, => IE. */
#define TLS_DTPREL	 8	/* DTPREL reloc, => LD. */
#define TLS_TLS		16	/* Any TLS reloc.  */
#define TLS_TPRELGD	32	/* TPREL reloc resulting from GD->IE. */
  char tls_mask;
};

#define ppc_elf_hash_entry(ent) ((struct ppc_elf_link_hash_entry *) (ent))

/* PPC ELF linker hash table.  */

struct ppc_elf_link_hash_table
{
  struct elf_link_hash_table elf;

  /* Short-cuts to get to dynamic linker sections.  */
  asection *got;
  asection *relgot;
  asection *plt;
  asection *relplt;
  asection *dynbss;
  asection *relbss;
  asection *dynsbss;
  asection *relsbss;
  elf_linker_section_t *sdata;
  elf_linker_section_t *sdata2;

  /* Short-cut to first output tls section.  */
  asection *tls_sec;

  /* Shortcut to .__tls_get_addr.  */
  struct elf_link_hash_entry *tls_get_addr;

  /* TLS local dynamic got entry handling.  */
  union {
    bfd_signed_vma refcount;
    bfd_vma offset;
  } tlsld_got;

  /* Small local sym to section mapping cache.  */
  struct sym_sec_cache sym_sec;
};

/* Get the PPC ELF linker hash table from a link_info structure.  */

#define ppc_elf_hash_table(p) \
  ((struct ppc_elf_link_hash_table *) (p)->hash)

/* Create an entry in a PPC ELF linker hash table.  */

static struct bfd_hash_entry *
ppc_elf_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (entry == NULL)
    {
      entry = bfd_hash_allocate (table,
				 sizeof (struct ppc_elf_link_hash_entry));
      if (entry == NULL)
	return entry;
    }

  /* Call the allocation method of the superclass.  */
  entry = _bfd_elf_link_hash_newfunc (entry, table, string);
  if (entry != NULL)
    {
      ppc_elf_hash_entry (entry)->dyn_relocs = NULL;
      ppc_elf_hash_entry (entry)->tls_mask = 0;
    }

  return entry;
}

/* Create a PPC ELF linker hash table.  */

static struct bfd_link_hash_table *
ppc_elf_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct ppc_elf_link_hash_table *ret;

  ret = ((struct ppc_elf_link_hash_table *)
	 bfd_malloc (sizeof (struct ppc_elf_link_hash_table)));
  if (ret == NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->elf, abfd,
				       ppc_elf_link_hash_newfunc))
    {
      free (ret);
      return NULL;
    }

  ret->got = NULL;
  ret->relgot = NULL;
  ret->plt = NULL;
  ret->relplt = NULL;
  ret->dynbss = NULL;
  ret->relbss = NULL;
  ret->dynsbss = NULL;
  ret->relsbss = NULL;
  ret->sdata = NULL;
  ret->sdata2 = NULL;
  ret->tls_sec = NULL;
  ret->tls_get_addr = NULL;
  ret->tlsld_got.refcount = 0;
  ret->sym_sec.abfd = NULL;

  return &ret->elf.root;
}

/* If ELIMINATE_COPY_RELOCS is non-zero, the linker will try to avoid
   copying dynamic variables from a shared lib into an app's dynbss
   section, and instead use a dynamic relocation to point into the
   shared lib.  */
#define ELIMINATE_COPY_RELOCS 1

/* Copy the extra info we tack onto an elf_link_hash_entry.  */

static void
ppc_elf_copy_indirect_symbol (bed, dir, ind)
     struct elf_backend_data *bed;
     struct elf_link_hash_entry *dir, *ind;
{
  struct ppc_elf_link_hash_entry *edir, *eind;

  edir = (struct ppc_elf_link_hash_entry *) dir;
  eind = (struct ppc_elf_link_hash_entry *) ind;

  if (eind->dyn_relocs != NULL)
    {
      if (edir->dyn_relocs != NULL)
	{
	  struct ppc_elf_dyn_relocs **pp;
	  struct ppc_elf_dyn_relocs *p;

	  if (ind->root.type == bfd_link_hash_indirect)
	    abort ();

	  /* Add reloc counts against the weak sym to the strong sym
	     list.  Merge any entries against the same section.  */
	  for (pp = &eind->dyn_relocs; (p = *pp) != NULL; )
	    {
	      struct ppc_elf_dyn_relocs *q;

	      for (q = edir->dyn_relocs; q != NULL; q = q->next)
		if (q->sec == p->sec)
		  {
		    q->pc_count += p->pc_count;
		    q->count += p->count;
		    *pp = p->next;
		    break;
		  }
	      if (q == NULL)
		pp = &p->next;
	    }
	  *pp = edir->dyn_relocs;
	}

      edir->dyn_relocs = eind->dyn_relocs;
      eind->dyn_relocs = NULL;
    }

  edir->tls_mask |= eind->tls_mask;

  if (ELIMINATE_COPY_RELOCS
      && ind->root.type != bfd_link_hash_indirect
      && (dir->elf_link_hash_flags & ELF_LINK_HASH_DYNAMIC_ADJUSTED) != 0)
    /* If called to transfer flags for a weakdef during processing
       of elf_adjust_dynamic_symbol, don't copy ELF_LINK_NON_GOT_REF.
       We clear it ourselves for ELIMINATE_COPY_RELOCS.  */
    dir->elf_link_hash_flags |=
      (ind->elf_link_hash_flags & (ELF_LINK_HASH_REF_DYNAMIC
				   | ELF_LINK_HASH_REF_REGULAR
				   | ELF_LINK_HASH_REF_REGULAR_NONWEAK));
  else
    _bfd_elf_link_hash_copy_indirect (bed, dir, ind);
}

static reloc_howto_type *ppc_elf_howto_table[(int) R_PPC_max];

static reloc_howto_type ppc_elf_howto_raw[] = {
  /* This reloc does nothing.  */
  HOWTO (R_PPC_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A standard 32 bit relocation.  */
  HOWTO (R_PPC_ADDR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An absolute 26 bit branch; the lower two bits must be zero.
     FIXME: we don't check that, we just clear them.  */
  HOWTO (R_PPC_ADDR24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR24",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A standard 16 bit relocation.  */
  HOWTO (R_PPC_ADDR16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit relocation without overflow.  */
  HOWTO (R_PPC_ADDR16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR16_LO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* The high order 16 bits of an address.  */
  HOWTO (R_PPC_ADDR16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR16_HI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* The high order 16 bits of an address, plus 1 if the contents of
     the low 16 bits, treated as a signed number, is negative.  */
  HOWTO (R_PPC_ADDR16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_ADDR16_HA",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An absolute 16 bit branch; the lower two bits must be zero.
     FIXME: we don't check that, we just clear them.  */
  HOWTO (R_PPC_ADDR14,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR14",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An absolute 16 bit branch, for which bit 10 should be set to
     indicate that the branch is expected to be taken.	The lower two
     bits must be zero.  */
  HOWTO (R_PPC_ADDR14_BRTAKEN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR14_BRTAKEN",/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An absolute 16 bit branch, for which bit 10 should be set to
     indicate that the branch is not expected to be taken.  The lower
     two bits must be zero.  */
  HOWTO (R_PPC_ADDR14_BRNTAKEN, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR14_BRNTAKEN",/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A relative 26 bit branch; the lower two bits must be zero.  */
  HOWTO (R_PPC_REL24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_REL24",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A relative 16 bit branch; the lower two bits must be zero.  */
  HOWTO (R_PPC_REL14,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_REL14",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A relative 16 bit branch.  Bit 10 should be set to indicate that
     the branch is expected to be taken.  The lower two bits must be
     zero.  */
  HOWTO (R_PPC_REL14_BRTAKEN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_REL14_BRTAKEN",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A relative 16 bit branch.  Bit 10 should be set to indicate that
     the branch is not expected to be taken.  The lower two bits must
     be zero.  */
  HOWTO (R_PPC_REL14_BRNTAKEN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_REL14_BRNTAKEN",/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Like R_PPC_ADDR16, but referring to the GOT table entry for the
     symbol.  */
  HOWTO (R_PPC_GOT16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_GOT16",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_PPC_ADDR16_LO, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC_GOT16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_GOT16_LO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_PPC_ADDR16_HI, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC_GOT16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_GOT16_HI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		 /* pcrel_offset */

  /* Like R_PPC_ADDR16_HA, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC_GOT16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_GOT16_HA",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_PPC_REL24, but referring to the procedure linkage table
     entry for the symbol.  */
  HOWTO (R_PPC_PLTREL24,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,  /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_PLTREL24",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* This is used only by the dynamic linker.  The symbol should exist
     both in the object being run and in some shared library.  The
     dynamic linker copies the data addressed by the symbol from the
     shared library into the object, because the object being
     run has to have the data at some particular address.  */
  HOWTO (R_PPC_COPY,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_PPC_COPY",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_PPC_ADDR32, but used when setting global offset table
     entries.  */
  HOWTO (R_PPC_GLOB_DAT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_PPC_GLOB_DAT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Marks a procedure linkage table entry for a symbol.  */
  HOWTO (R_PPC_JMP_SLOT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_PPC_JMP_SLOT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Used only by the dynamic linker.  When the object is run, this
     longword is set to the load address of the object, plus the
     addend.  */
  HOWTO (R_PPC_RELATIVE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_PPC_RELATIVE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_PPC_REL24, but uses the value of the symbol within the
     object rather than the final value.  Normally used for
     _GLOBAL_OFFSET_TABLE_.  */
  HOWTO (R_PPC_LOCAL24PC,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_LOCAL24PC",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Like R_PPC_ADDR32, but may be unaligned.  */
  HOWTO (R_PPC_UADDR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_UADDR32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_PPC_ADDR16, but may be unaligned.  */
  HOWTO (R_PPC_UADDR16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_UADDR16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 32-bit PC relative */
  HOWTO (R_PPC_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_REL32",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 32-bit relocation to the symbol's procedure linkage table.
     FIXME: not supported.  */
  HOWTO (R_PPC_PLT32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_PLT32",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 32-bit PC relative relocation to the symbol's procedure linkage table.
     FIXME: not supported.  */
  HOWTO (R_PPC_PLTREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_PLTREL32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Like R_PPC_ADDR16_LO, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC_PLT16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_PLT16_LO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_PPC_ADDR16_HI, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC_PLT16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_PLT16_HI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		 /* pcrel_offset */

  /* Like R_PPC_ADDR16_HA, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC_PLT16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_PLT16_HA",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A sign-extended 16 bit value relative to _SDA_BASE_, for use with
     small data items.  */
  HOWTO (R_PPC_SDAREL16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_SDAREL16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16-bit section relative relocation.  */
  HOWTO (R_PPC_SECTOFF,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_SECTOFF",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16-bit lower half section relative relocation.  */
  HOWTO (R_PPC_SECTOFF_LO,	  /* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_SECTOFF_LO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16-bit upper half section relative relocation.  */
  HOWTO (R_PPC_SECTOFF_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_SECTOFF_HI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		 /* pcrel_offset */

  /* 16-bit upper half adjusted section relative relocation.  */
  HOWTO (R_PPC_SECTOFF_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_SECTOFF_HA",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Marker reloc for TLS.  */
  HOWTO (R_PPC_TLS,
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_TLS",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Computes the load module index of the load module that contains the
     definition of its TLS sym.  */
  HOWTO (R_PPC_DTPMOD32,
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_DTPMOD32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Computes a dtv-relative displacement, the difference between the value
     of sym+add and the base address of the thread-local storage block that
     contains the definition of sym, minus 0x8000.  */
  HOWTO (R_PPC_DTPREL32,
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_DTPREL32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit dtprel reloc.  */
  HOWTO (R_PPC_DTPREL16,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_DTPREL16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like DTPREL16, but no overflow.  */
  HOWTO (R_PPC_DTPREL16_LO,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_DTPREL16_LO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like DTPREL16_LO, but next higher group of 16 bits.  */
  HOWTO (R_PPC_DTPREL16_HI,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_DTPREL16_HI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like DTPREL16_HI, but adjust for low 16 bits.  */
  HOWTO (R_PPC_DTPREL16_HA,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_DTPREL16_HA",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Computes a tp-relative displacement, the difference between the value of
     sym+add and the value of the thread pointer (r13).  */
  HOWTO (R_PPC_TPREL32,
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_TPREL32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit tprel reloc.  */
  HOWTO (R_PPC_TPREL16,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_TPREL16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like TPREL16, but no overflow.  */
  HOWTO (R_PPC_TPREL16_LO,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_TPREL16_LO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like TPREL16_LO, but next higher group of 16 bits.  */
  HOWTO (R_PPC_TPREL16_HI,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_TPREL16_HI",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like TPREL16_HI, but adjust for low 16 bits.  */
  HOWTO (R_PPC_TPREL16_HA,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_TPREL16_HA",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Allocates two contiguous entries in the GOT to hold a tls_index structure,
     with values (sym+add)@dtpmod and (sym+add)@dtprel, and computes the offset
     to the first entry.  */
  HOWTO (R_PPC_GOT_TLSGD16,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TLSGD16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_TLSGD16, but no overflow.  */
  HOWTO (R_PPC_GOT_TLSGD16_LO,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TLSGD16_LO", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_TLSGD16_LO, but next higher group of 16 bits.  */
  HOWTO (R_PPC_GOT_TLSGD16_HI,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TLSGD16_HI", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_TLSGD16_HI, but adjust for low 16 bits.  */
  HOWTO (R_PPC_GOT_TLSGD16_HA,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TLSGD16_HA", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Allocates two contiguous entries in the GOT to hold a tls_index structure,
     with values (sym+add)@dtpmod and zero, and computes the offset to the
     first entry.  */
  HOWTO (R_PPC_GOT_TLSLD16,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TLSLD16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_TLSLD16, but no overflow.  */
  HOWTO (R_PPC_GOT_TLSLD16_LO,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TLSLD16_LO", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_TLSLD16_LO, but next higher group of 16 bits.  */
  HOWTO (R_PPC_GOT_TLSLD16_HI,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TLSLD16_HI", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_TLSLD16_HI, but adjust for low 16 bits.  */
  HOWTO (R_PPC_GOT_TLSLD16_HA,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TLSLD16_HA", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Allocates an entry in the GOT with value (sym+add)@dtprel, and computes
     the offset to the entry.  */
  HOWTO (R_PPC_GOT_DTPREL16,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_DTPREL16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_DTPREL16, but no overflow.  */
  HOWTO (R_PPC_GOT_DTPREL16_LO,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_DTPREL16_LO", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_DTPREL16_LO, but next higher group of 16 bits.  */
  HOWTO (R_PPC_GOT_DTPREL16_HI,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_DTPREL16_HI", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_DTPREL16_HI, but adjust for low 16 bits.  */
  HOWTO (R_PPC_GOT_DTPREL16_HA,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_DTPREL16_HA", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Allocates an entry in the GOT with value (sym+add)@tprel, and computes the
     offset to the entry.  */
  HOWTO (R_PPC_GOT_TPREL16,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TPREL16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_TPREL16, but no overflow.  */
  HOWTO (R_PPC_GOT_TPREL16_LO,
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TPREL16_LO", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_TPREL16_LO, but next higher group of 16 bits.  */
  HOWTO (R_PPC_GOT_TPREL16_HI,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TPREL16_HI", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like GOT_TPREL16_HI, but adjust for low 16 bits.  */
  HOWTO (R_PPC_GOT_TPREL16_HA,
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unhandled_reloc, /* special_function */
	 "R_PPC_GOT_TPREL16_HA", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* The remaining relocs are from the Embedded ELF ABI, and are not
     in the SVR4 ELF ABI.  */

  /* 32 bit value resulting from the addend minus the symbol.  */
  HOWTO (R_PPC_EMB_NADDR32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_NADDR32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16 bit value resulting from the addend minus the symbol.  */
  HOWTO (R_PPC_EMB_NADDR16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_NADDR16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16 bit value resulting from the addend minus the symbol.  */
  HOWTO (R_PPC_EMB_NADDR16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_ADDR16_LO",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* The high order 16 bits of the addend minus the symbol.  */
  HOWTO (R_PPC_EMB_NADDR16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_NADDR16_HI", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* The high order 16 bits of the result of the addend minus the address,
     plus 1 if the contents of the low 16 bits, treated as a signed number,
     is negative.  */
  HOWTO (R_PPC_EMB_NADDR16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_EMB_NADDR16_HA", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16 bit value resulting from allocating a 4 byte word to hold an
     address in the .sdata section, and returning the offset from
     _SDA_BASE_ for that relocation.  */
  HOWTO (R_PPC_EMB_SDAI16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_SDAI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16 bit value resulting from allocating a 4 byte word to hold an
     address in the .sdata2 section, and returning the offset from
     _SDA2_BASE_ for that relocation.  */
  HOWTO (R_PPC_EMB_SDA2I16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_SDA2I16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A sign-extended 16 bit value relative to _SDA2_BASE_, for use with
     small data items.	 */
  HOWTO (R_PPC_EMB_SDA2REL,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_SDA2REL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Relocate against either _SDA_BASE_ or _SDA2_BASE_, filling in the 16 bit
     signed offset from the appropriate base, and filling in the register
     field with the appropriate register (0, 2, or 13).  */
  HOWTO (R_PPC_EMB_SDA21,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_SDA21",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Relocation not handled: R_PPC_EMB_MRKREF */
  /* Relocation not handled: R_PPC_EMB_RELSEC16 */
  /* Relocation not handled: R_PPC_EMB_RELST_LO */
  /* Relocation not handled: R_PPC_EMB_RELST_HI */
  /* Relocation not handled: R_PPC_EMB_RELST_HA */
  /* Relocation not handled: R_PPC_EMB_BIT_FLD */

  /* PC relative relocation against either _SDA_BASE_ or _SDA2_BASE_, filling
     in the 16 bit signed offset from the appropriate base, and filling in the
     register field with the appropriate register (0, 2, or 13).  */
  HOWTO (R_PPC_EMB_RELSDA,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_RELSDA",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_PPC_GNU_VTINHERIT,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_PPC_GNU_VTINHERIT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_PPC_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_PPC_GNU_VTENTRY",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Phony reloc to handle AIX style TOC entries.  */
  HOWTO (R_PPC_TOC16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_TOC16",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
};

/* Initialize the ppc_elf_howto_table, so that linear accesses can be done.  */

static void
ppc_elf_howto_init ()
{
  unsigned int i, type;

  for (i = 0;
       i < sizeof (ppc_elf_howto_raw) / sizeof (ppc_elf_howto_raw[0]);
       i++)
    {
      type = ppc_elf_howto_raw[i].type;
      if (type >= (sizeof (ppc_elf_howto_table)
		   / sizeof (ppc_elf_howto_table[0])))
	abort ();
      ppc_elf_howto_table[type] = &ppc_elf_howto_raw[i];
    }
}

/* This function handles relaxing for the PPC with option --mpc860c0[=<n>].

   The MPC860, revision C0 or earlier contains a bug in the die.
   If all of the following conditions are true, the next instruction
   to be executed *may* be treated as a no-op.
   1/ A forward branch is executed.
   2/ The branch is predicted as not taken.
   3/ The branch is taken.
   4/ The branch is located in the last 5 words of a page.
      (The EOP limit is 5 by default but may be specified as any value
      from 1-10.)

   Our software solution is to detect these problematic branches in a
   linker pass and modify them as follows:
   1/ Unconditional branches - Since these are always predicted taken,
      there is no problem and no action is required.
   2/ Conditional backward branches - No problem, no action required.
   3/ Conditional forward branches - Ensure that the "inverse prediction
      bit" is set (ensure it is predicted taken).
   4/ Conditional register branches - Ensure that the "y bit" is set
      (ensure it is predicted taken).  */

/* Sort sections by address.  */

static int
ppc_elf_sort_rela (arg1, arg2)
     const PTR arg1;
     const PTR arg2;
{
  const Elf_Internal_Rela **rela1 = (const Elf_Internal_Rela**) arg1;
  const Elf_Internal_Rela **rela2 = (const Elf_Internal_Rela**) arg2;

  /* Sort by offset.  */
  return ((*rela1)->r_offset - (*rela2)->r_offset);
}

static bfd_boolean
ppc_elf_relax_section (abfd, isec, link_info, again)
     bfd *abfd;
     asection *isec;
     struct bfd_link_info *link_info;
     bfd_boolean *again;
{
#define PAGESIZE 0x1000

  bfd_byte *contents = NULL;
  bfd_byte *free_contents = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Rela *free_relocs = NULL;
  Elf_Internal_Rela **rela_comb = NULL;
  int comb_curr, comb_count;

  /* We never have to do this more than once per input section.  */
  *again = FALSE;

  /* If needed, initialize this section's cooked size.  */
  if (isec->_cooked_size == 0)
    isec->_cooked_size = isec->_raw_size;

  /* We're only interested in text sections which overlap the
     troublesome area at the end of a page.  */
  if (link_info->mpc860c0 && (isec->flags & SEC_CODE) && isec->_cooked_size)
    {
      bfd_vma dot, end_page, end_section;
      bfd_boolean section_modified;

      /* Get the section contents.  */
      /* Get cached copy if it exists.  */
      if (elf_section_data (isec)->this_hdr.contents != NULL)
	contents = elf_section_data (isec)->this_hdr.contents;
      else
	{
	  /* Go get them off disk.  */
	  contents = (bfd_byte *) bfd_malloc (isec->_raw_size);
	  if (contents == NULL)
	    goto error_return;
	  free_contents = contents;

	  if (! bfd_get_section_contents (abfd, isec, contents,
					  (file_ptr) 0, isec->_raw_size))
	    goto error_return;
	}

      comb_curr = 0;
      comb_count = 0;
      if (isec->reloc_count)
	{
	  unsigned n;
	  bfd_size_type amt;

	  /* Get a copy of the native relocations.  */
	  internal_relocs
	    = _bfd_elf32_link_read_relocs (abfd, isec, (PTR) NULL,
					   (Elf_Internal_Rela *) NULL,
					   link_info->keep_memory);
	  if (internal_relocs == NULL)
	    goto error_return;
	  if (! link_info->keep_memory)
	    free_relocs = internal_relocs;

	  /* Setup a faster access method for the reloc info we need.  */
	  amt = isec->reloc_count;
	  amt *= sizeof (Elf_Internal_Rela*);
	  rela_comb = (Elf_Internal_Rela**) bfd_malloc (amt);
	  if (rela_comb == NULL)
	    goto error_return;
	  for (n = 0; n < isec->reloc_count; ++n)
	    {
	      long r_type;

	      r_type = ELF32_R_TYPE (internal_relocs[n].r_info);
	      if (r_type < 0 || r_type >= (int) R_PPC_max)
		goto error_return;

	      /* Prologue constants are sometimes present in the ".text"
		 sections and they can be identified by their associated
		 relocation.  We don't want to process those words and
		 some others which can also be identified by their
		 relocations.  However, not all conditional branches will
		 have a relocation so we will only ignore words that
		 1) have a reloc, and 2) the reloc is not applicable to a
		 conditional branch.  The array rela_comb is built here
		 for use in the EOP scan loop.  */
	      switch (r_type)
		{
		case R_PPC_ADDR14_BRNTAKEN:
		case R_PPC_REL14:
		case R_PPC_REL14_BRNTAKEN:
		  /* We should check the instruction.  */
		  break;
		default:
		  /* The word is not a conditional branch - ignore it.  */
		  rela_comb[comb_count++] = &internal_relocs[n];
		  break;
		}
	    }
	  if (comb_count > 1)
	    qsort (rela_comb, (size_t) comb_count, sizeof (int),
		   ppc_elf_sort_rela);
	}

      /* Enumerate each EOP region that overlaps this section.  */
      end_section = isec->vma + isec->_cooked_size;
      dot = end_page = (isec->vma | (PAGESIZE - 1)) + 1;
      dot -= link_info->mpc860c0;
      section_modified = FALSE;
      /* Increment the start position if this section begins in the
	 middle of its first EOP region.  */
      if (dot < isec->vma)
	dot = isec->vma;
      for (;
	   dot < end_section;
	   dot += PAGESIZE, end_page += PAGESIZE)
	{
	  /* Check each word in this EOP region.  */
	  for (; dot < end_page; dot += 4)
	    {
	      bfd_vma isec_offset;
	      unsigned long insn;
	      bfd_boolean skip, modified;

	      /* Don't process this word if there is a relocation for it
		 and the relocation indicates the word is not a
		 conditional branch.  */
	      skip = FALSE;
	      isec_offset = dot - isec->vma;
	      for (; comb_curr<comb_count; ++comb_curr)
		{
		  bfd_vma r_offset;

		  r_offset = rela_comb[comb_curr]->r_offset;
		  if (r_offset >= isec_offset)
		    {
		      if (r_offset == isec_offset) skip = TRUE;
		      break;
		    }
		}
	      if (skip) continue;

	      /* Check the current word for a problematic conditional
		 branch.  */
#define BO0(insn) ((insn) & 0x02000000)
#define BO2(insn) ((insn) & 0x00800000)
#define BO4(insn) ((insn) & 0x00200000)
	      insn = (unsigned long) bfd_get_32 (abfd, contents + isec_offset);
	      modified = FALSE;
	      if ((insn & 0xFc000000) == 0x40000000)
		{
		  /* Instruction is BCx */
		  if ((!BO0(insn) || !BO2(insn)) && !BO4(insn))
		    {
		      bfd_vma target;

		      /* This branch is predicted as "normal".
			 If this is a forward branch, it is problematic.  */
		      target = insn & 0x0000Fffc;
		      target = (target ^ 0x8000) - 0x8000;
		      if ((insn & 0x00000002) == 0)
			/* Convert to abs.  */
			target += dot;
		      if (target > dot)
			{
			  /* Set the prediction bit.  */
			  insn |= 0x00200000;
			  modified = TRUE;
			}
		    }
		}
	      else if ((insn & 0xFc00Fffe) == 0x4c000420)
		{
		  /* Instruction is BCCTRx.  */
		  if ((!BO0(insn) || !BO2(insn)) && !BO4(insn))
		    {
		      /* This branch is predicted as not-taken.
			 If this is a forward branch, it is problematic.
			 Since we can't tell statically if it will branch
			 forward, always set the prediction bit.  */
		      insn |= 0x00200000;
		      modified = TRUE;
		    }
		}
	      else if ((insn & 0xFc00Fffe) == 0x4c000020)
		{
		  /* Instruction is BCLRx */
		  if ((!BO0(insn) || !BO2(insn)) && !BO4(insn))
		    {
		      /* This branch is predicted as not-taken.
			 If this is a forward branch, it is problematic.
			 Since we can't tell statically if it will branch
			 forward, always set the prediction bit.  */
		      insn |= 0x00200000;
		      modified = TRUE;
		    }
		}
#undef BO0
#undef BO2
#undef BO4
	      if (modified)
		{
		  bfd_put_32 (abfd, (bfd_vma) insn, contents + isec_offset);
		  section_modified = TRUE;
		}
	    }
	}
      if (section_modified)
	{
	  elf_section_data (isec)->this_hdr.contents = contents;
	  free_contents = NULL;
	}
    }

  if (rela_comb != NULL)
    {
      free (rela_comb);
      rela_comb = NULL;
    }

  if (free_relocs != NULL)
    {
      free (free_relocs);
      free_relocs = NULL;
    }

  if (free_contents != NULL)
    {
      if (! link_info->keep_memory)
	free (free_contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (isec)->this_hdr.contents = contents;
	}
      free_contents = NULL;
    }

  return TRUE;

 error_return:
  if (rela_comb != NULL)
    free (rela_comb);
  if (free_relocs != NULL)
    free (free_relocs);
  if (free_contents != NULL)
    free (free_contents);
  return FALSE;
}

static reloc_howto_type *
ppc_elf_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  enum elf_ppc_reloc_type r;

  if (!ppc_elf_howto_table[R_PPC_ADDR32])
    /* Initialize howto table if needed.  */
    ppc_elf_howto_init ();

  switch ((int) code)
    {
    default:
      return (reloc_howto_type *) NULL;

    case BFD_RELOC_NONE:		r = R_PPC_NONE;			break;
    case BFD_RELOC_32:			r = R_PPC_ADDR32;		break;
    case BFD_RELOC_PPC_BA26:		r = R_PPC_ADDR24;		break;
    case BFD_RELOC_16:			r = R_PPC_ADDR16;		break;
    case BFD_RELOC_LO16:		r = R_PPC_ADDR16_LO;		break;
    case BFD_RELOC_HI16:		r = R_PPC_ADDR16_HI;		break;
    case BFD_RELOC_HI16_S:		r = R_PPC_ADDR16_HA;		break;
    case BFD_RELOC_PPC_BA16:		r = R_PPC_ADDR14;		break;
    case BFD_RELOC_PPC_BA16_BRTAKEN:	r = R_PPC_ADDR14_BRTAKEN;	break;
    case BFD_RELOC_PPC_BA16_BRNTAKEN:	r = R_PPC_ADDR14_BRNTAKEN;	break;
    case BFD_RELOC_PPC_B26:		r = R_PPC_REL24;		break;
    case BFD_RELOC_PPC_B16:		r = R_PPC_REL14;		break;
    case BFD_RELOC_PPC_B16_BRTAKEN:	r = R_PPC_REL14_BRTAKEN;	break;
    case BFD_RELOC_PPC_B16_BRNTAKEN:	r = R_PPC_REL14_BRNTAKEN;	break;
    case BFD_RELOC_16_GOTOFF:		r = R_PPC_GOT16;		break;
    case BFD_RELOC_LO16_GOTOFF:		r = R_PPC_GOT16_LO;		break;
    case BFD_RELOC_HI16_GOTOFF:		r = R_PPC_GOT16_HI;		break;
    case BFD_RELOC_HI16_S_GOTOFF:	r = R_PPC_GOT16_HA;		break;
    case BFD_RELOC_24_PLT_PCREL:	r = R_PPC_PLTREL24;		break;
    case BFD_RELOC_PPC_COPY:		r = R_PPC_COPY;			break;
    case BFD_RELOC_PPC_GLOB_DAT:	r = R_PPC_GLOB_DAT;		break;
    case BFD_RELOC_PPC_LOCAL24PC:	r = R_PPC_LOCAL24PC;		break;
    case BFD_RELOC_32_PCREL:		r = R_PPC_REL32;		break;
    case BFD_RELOC_32_PLTOFF:		r = R_PPC_PLT32;		break;
    case BFD_RELOC_32_PLT_PCREL:	r = R_PPC_PLTREL32;		break;
    case BFD_RELOC_LO16_PLTOFF:		r = R_PPC_PLT16_LO;		break;
    case BFD_RELOC_HI16_PLTOFF:		r = R_PPC_PLT16_HI;		break;
    case BFD_RELOC_HI16_S_PLTOFF:	r = R_PPC_PLT16_HA;		break;
    case BFD_RELOC_GPREL16:		r = R_PPC_SDAREL16;		break;
    case BFD_RELOC_16_BASEREL:		r = R_PPC_SECTOFF;		break;
    case BFD_RELOC_LO16_BASEREL:	r = R_PPC_SECTOFF_LO;		break;
    case BFD_RELOC_HI16_BASEREL:	r = R_PPC_SECTOFF_HI;		break;
    case BFD_RELOC_HI16_S_BASEREL:	r = R_PPC_SECTOFF_HA;		break;
    case BFD_RELOC_CTOR:		r = R_PPC_ADDR32;		break;
    case BFD_RELOC_PPC_TOC16:		r = R_PPC_TOC16;		break;
    case BFD_RELOC_PPC_TLS:		r = R_PPC_TLS;			break;
    case BFD_RELOC_PPC_DTPMOD:		r = R_PPC_DTPMOD32;		break;
    case BFD_RELOC_PPC_TPREL16:		r = R_PPC_TPREL16;		break;
    case BFD_RELOC_PPC_TPREL16_LO:	r = R_PPC_TPREL16_LO;		break;
    case BFD_RELOC_PPC_TPREL16_HI:	r = R_PPC_TPREL16_HI;		break;
    case BFD_RELOC_PPC_TPREL16_HA:	r = R_PPC_TPREL16_HA;		break;
    case BFD_RELOC_PPC_TPREL:		r = R_PPC_TPREL32;		break;
    case BFD_RELOC_PPC_DTPREL16:	r = R_PPC_DTPREL16;		break;
    case BFD_RELOC_PPC_DTPREL16_LO:	r = R_PPC_DTPREL16_LO;		break;
    case BFD_RELOC_PPC_DTPREL16_HI:	r = R_PPC_DTPREL16_HI;		break;
    case BFD_RELOC_PPC_DTPREL16_HA:	r = R_PPC_DTPREL16_HA;		break;
    case BFD_RELOC_PPC_DTPREL:		r = R_PPC_DTPREL32;		break;
    case BFD_RELOC_PPC_GOT_TLSGD16:	r = R_PPC_GOT_TLSGD16;		break;
    case BFD_RELOC_PPC_GOT_TLSGD16_LO:	r = R_PPC_GOT_TLSGD16_LO;	break;
    case BFD_RELOC_PPC_GOT_TLSGD16_HI:	r = R_PPC_GOT_TLSGD16_HI;	break;
    case BFD_RELOC_PPC_GOT_TLSGD16_HA:	r = R_PPC_GOT_TLSGD16_HA;	break;
    case BFD_RELOC_PPC_GOT_TLSLD16:	r = R_PPC_GOT_TLSLD16;		break;
    case BFD_RELOC_PPC_GOT_TLSLD16_LO:	r = R_PPC_GOT_TLSLD16_LO;	break;
    case BFD_RELOC_PPC_GOT_TLSLD16_HI:	r = R_PPC_GOT_TLSLD16_HI;	break;
    case BFD_RELOC_PPC_GOT_TLSLD16_HA:	r = R_PPC_GOT_TLSLD16_HA;	break;
    case BFD_RELOC_PPC_GOT_TPREL16:	r = R_PPC_GOT_TPREL16;		break;
    case BFD_RELOC_PPC_GOT_TPREL16_LO:	r = R_PPC_GOT_TPREL16_LO;	break;
    case BFD_RELOC_PPC_GOT_TPREL16_HI:	r = R_PPC_GOT_TPREL16_HI;	break;
    case BFD_RELOC_PPC_GOT_TPREL16_HA:	r = R_PPC_GOT_TPREL16_HA;	break;
    case BFD_RELOC_PPC_GOT_DTPREL16:	r = R_PPC_GOT_DTPREL16;		break;
    case BFD_RELOC_PPC_GOT_DTPREL16_LO:	r = R_PPC_GOT_DTPREL16_LO;	break;
    case BFD_RELOC_PPC_GOT_DTPREL16_HI:	r = R_PPC_GOT_DTPREL16_HI;	break;
    case BFD_RELOC_PPC_GOT_DTPREL16_HA:	r = R_PPC_GOT_DTPREL16_HA;	break;
    case BFD_RELOC_PPC_EMB_NADDR32:	r = R_PPC_EMB_NADDR32;		break;
    case BFD_RELOC_PPC_EMB_NADDR16:	r = R_PPC_EMB_NADDR16;		break;
    case BFD_RELOC_PPC_EMB_NADDR16_LO:	r = R_PPC_EMB_NADDR16_LO;	break;
    case BFD_RELOC_PPC_EMB_NADDR16_HI:	r = R_PPC_EMB_NADDR16_HI;	break;
    case BFD_RELOC_PPC_EMB_NADDR16_HA:	r = R_PPC_EMB_NADDR16_HA;	break;
    case BFD_RELOC_PPC_EMB_SDAI16:	r = R_PPC_EMB_SDAI16;		break;
    case BFD_RELOC_PPC_EMB_SDA2I16:	r = R_PPC_EMB_SDA2I16;		break;
    case BFD_RELOC_PPC_EMB_SDA2REL:	r = R_PPC_EMB_SDA2REL;		break;
    case BFD_RELOC_PPC_EMB_SDA21:	r = R_PPC_EMB_SDA21;		break;
    case BFD_RELOC_PPC_EMB_MRKREF:	r = R_PPC_EMB_MRKREF;		break;
    case BFD_RELOC_PPC_EMB_RELSEC16:	r = R_PPC_EMB_RELSEC16;		break;
    case BFD_RELOC_PPC_EMB_RELST_LO:	r = R_PPC_EMB_RELST_LO;		break;
    case BFD_RELOC_PPC_EMB_RELST_HI:	r = R_PPC_EMB_RELST_HI;		break;
    case BFD_RELOC_PPC_EMB_RELST_HA:	r = R_PPC_EMB_RELST_HA;		break;
    case BFD_RELOC_PPC_EMB_BIT_FLD:	r = R_PPC_EMB_BIT_FLD;		break;
    case BFD_RELOC_PPC_EMB_RELSDA:	r = R_PPC_EMB_RELSDA;		break;
    case BFD_RELOC_VTABLE_INHERIT:	r = R_PPC_GNU_VTINHERIT;	break;
    case BFD_RELOC_VTABLE_ENTRY:	r = R_PPC_GNU_VTENTRY;		break;
    }

  return ppc_elf_howto_table[(int) r];
};

/* Set the howto pointer for a PowerPC ELF reloc.  */

static void
ppc_elf_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  if (!ppc_elf_howto_table[R_PPC_ADDR32])
    /* Initialize howto table if needed.  */
    ppc_elf_howto_init ();

  BFD_ASSERT (ELF32_R_TYPE (dst->r_info) < (unsigned int) R_PPC_max);
  cache_ptr->howto = ppc_elf_howto_table[ELF32_R_TYPE (dst->r_info)];
}

/* Handle the R_PPC_ADDR16_HA reloc.  */

static bfd_reloc_status_type
ppc_elf_addr16_ha_reloc (abfd, reloc_entry, symbol, data, input_section,
			 output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;

  if (output_bfd != NULL)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  reloc_entry->addend += (relocation & 0x8000) << 1;

  return bfd_reloc_continue;
}

static bfd_reloc_status_type
ppc_elf_unhandled_reloc (abfd, reloc_entry, symbol, data,
			 input_section, output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  /* If this is a relocatable link (output_bfd test tells us), just
     call the generic function.  Any adjustment will be done at final
     link time.  */
  if (output_bfd != NULL)
    return bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
				  input_section, output_bfd, error_message);

  if (error_message != NULL)
    {
      static char buf[60];
      sprintf (buf, "generic linker can't handle %s",
	       reloc_entry->howto->name);
      *error_message = buf;
    }
  return bfd_reloc_dangerous;
}

/* Fix bad default arch selected for a 32 bit input bfd when the
   default is 64 bit.  */

static bfd_boolean
ppc_elf_object_p (abfd)
     bfd *abfd;
{
  if (abfd->arch_info->the_default && abfd->arch_info->bits_per_word == 64)
    {
      Elf_Internal_Ehdr *i_ehdr = elf_elfheader (abfd);

      if (i_ehdr->e_ident[EI_CLASS] == ELFCLASS32)
	{
	  /* Relies on arch after 64 bit default being 32 bit default.  */
	  abfd->arch_info = abfd->arch_info->next;
	  BFD_ASSERT (abfd->arch_info->bits_per_word == 32);
	}
    }
  return TRUE;
}

/* Function to set whether a module needs the -mrelocatable bit set.  */

static bfd_boolean
ppc_elf_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static bfd_boolean
ppc_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword old_flags;
  flagword new_flags;
  bfd_boolean error;

  /* Check if we have the same endianess.  */
  if (! _bfd_generic_verify_endian_match (ibfd, obfd))
    return FALSE;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;
  if (!elf_flags_init (obfd))
    {
      /* First call, no flags set.  */
      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = new_flags;
    }

  /* Compatible flags are ok.  */
  else if (new_flags == old_flags)
    ;

  /* Incompatible flags.  */
  else
    {
      /* Warn about -mrelocatable mismatch.  Allow -mrelocatable-lib
	 to be linked with either.  */
      error = FALSE;
      if ((new_flags & EF_PPC_RELOCATABLE) != 0
	  && (old_flags & (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB)) == 0)
	{
	  error = TRUE;
	  (*_bfd_error_handler)
	    (_("%s: compiled with -mrelocatable and linked with modules compiled normally"),
	     bfd_archive_filename (ibfd));
	}
      else if ((new_flags & (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB)) == 0
	       && (old_flags & EF_PPC_RELOCATABLE) != 0)
	{
	  error = TRUE;
	  (*_bfd_error_handler)
	    (_("%s: compiled normally and linked with modules compiled with -mrelocatable"),
	     bfd_archive_filename (ibfd));
	}

      /* The output is -mrelocatable-lib iff both the input files are.  */
      if (! (new_flags & EF_PPC_RELOCATABLE_LIB))
	elf_elfheader (obfd)->e_flags &= ~EF_PPC_RELOCATABLE_LIB;

      /* The output is -mrelocatable iff it can't be -mrelocatable-lib,
	 but each input file is either -mrelocatable or -mrelocatable-lib.  */
      if (! (elf_elfheader (obfd)->e_flags & EF_PPC_RELOCATABLE_LIB)
	  && (new_flags & (EF_PPC_RELOCATABLE_LIB | EF_PPC_RELOCATABLE))
	  && (old_flags & (EF_PPC_RELOCATABLE_LIB | EF_PPC_RELOCATABLE)))
	elf_elfheader (obfd)->e_flags |= EF_PPC_RELOCATABLE;

      /* Do not warn about eabi vs. V.4 mismatch, just or in the bit if
	 any module uses it.  */
      elf_elfheader (obfd)->e_flags |= (new_flags & EF_PPC_EMB);

      new_flags &= ~(EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB | EF_PPC_EMB);
      old_flags &= ~(EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB | EF_PPC_EMB);

      /* Warn about any other mismatches.  */
      if (new_flags != old_flags)
	{
	  error = TRUE;
	  (*_bfd_error_handler)
	    (_("%s: uses different e_flags (0x%lx) fields than previous modules (0x%lx)"),
	     bfd_archive_filename (ibfd), (long) new_flags, (long) old_flags);
	}

      if (error)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}
    }

  return TRUE;
}

/* Handle a PowerPC specific section when reading an object file.  This
   is called when elfcode.h finds a section with an unknown type.  */

static bfd_boolean
ppc_elf_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf_Internal_Shdr *hdr;
     const char *name;
{
  asection *newsect;
  flagword flags;

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
    return FALSE;

  newsect = hdr->bfd_section;
  flags = bfd_get_section_flags (abfd, newsect);
  if (hdr->sh_flags & SHF_EXCLUDE)
    flags |= SEC_EXCLUDE;

  if (hdr->sh_type == SHT_ORDERED)
    flags |= SEC_SORT_ENTRIES;

  bfd_set_section_flags (abfd, newsect, flags);
  return TRUE;
}

/* Set up any other section flags and such that may be necessary.  */

static bfd_boolean
ppc_elf_fake_sections (abfd, shdr, asect)
     bfd *abfd ATTRIBUTE_UNUSED;
     Elf_Internal_Shdr *shdr;
     asection *asect;
{
  if ((asect->flags & SEC_EXCLUDE) != 0)
    shdr->sh_flags |= SHF_EXCLUDE;

  if ((asect->flags & SEC_SORT_ENTRIES) != 0)
    shdr->sh_type = SHT_ORDERED;

  return TRUE;
}

/* Create a special linker section */
static elf_linker_section_t *
ppc_elf_create_linker_section (abfd, info, which)
     bfd *abfd;
     struct bfd_link_info *info;
     enum elf_linker_section_enum which;
{
  bfd *dynobj = elf_hash_table (info)->dynobj;
  elf_linker_section_t *lsect;

  /* Record the first bfd section that needs the special section.  */
  if (!dynobj)
    dynobj = elf_hash_table (info)->dynobj = abfd;

  /* If this is the first time, create the section.  */
  lsect = elf_linker_section (dynobj, which);
  if (!lsect)
    {
      elf_linker_section_t defaults;
      static elf_linker_section_t zero_section;

      defaults = zero_section;
      defaults.which = which;
      defaults.hole_written_p = FALSE;
      defaults.alignment = 2;

      /* Both of these sections are (technically) created by the user
	 putting data in them, so they shouldn't be marked
	 SEC_LINKER_CREATED.

	 The linker creates them so it has somewhere to attach their
	 respective symbols. In fact, if they were empty it would
	 be OK to leave the symbol set to 0 (or any random number), because
	 the appropriate register should never be used.  */
      defaults.flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
			| SEC_IN_MEMORY);

      switch (which)
	{
	default:
	  (*_bfd_error_handler) (_("%s: unknown special linker type %d"),
				 bfd_get_filename (abfd),
				 (int) which);

	  bfd_set_error (bfd_error_bad_value);
	  return (elf_linker_section_t *) 0;

	case LINKER_SECTION_SDATA:	/* .sdata/.sbss section */
	  defaults.name		  = ".sdata";
	  defaults.rel_name	  = ".rela.sdata";
	  defaults.bss_name	  = ".sbss";
	  defaults.sym_name	  = "_SDA_BASE_";
	  defaults.sym_offset	  = 32768;
	  break;

	case LINKER_SECTION_SDATA2:	/* .sdata2/.sbss2 section */
	  defaults.name		  = ".sdata2";
	  defaults.rel_name	  = ".rela.sdata2";
	  defaults.bss_name	  = ".sbss2";
	  defaults.sym_name	  = "_SDA2_BASE_";
	  defaults.sym_offset	  = 32768;
	  defaults.flags	 |= SEC_READONLY;
	  break;
	}

      lsect = _bfd_elf_create_linker_section (abfd, info, which, &defaults);
    }

  return lsect;
}

/* If we have a non-zero sized .sbss2 or .PPC.EMB.sbss0 sections, we
   need to bump up the number of section headers.  */

static int
ppc_elf_additional_program_headers (abfd)
     bfd *abfd;
{
  asection *s;
  int ret;

  ret = 0;

  s = bfd_get_section_by_name (abfd, ".interp");
  if (s != NULL)
    ++ret;

  s = bfd_get_section_by_name (abfd, ".sbss2");
  if (s != NULL && (s->flags & SEC_LOAD) != 0 && s->_raw_size > 0)
    ++ret;

  s = bfd_get_section_by_name (abfd, ".PPC.EMB.sbss0");
  if (s != NULL && (s->flags & SEC_LOAD) != 0 && s->_raw_size > 0)
    ++ret;

  return ret;
}

/* Modify the segment map if needed.  */

static bfd_boolean
ppc_elf_modify_segment_map (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
  return TRUE;
}

/* The powerpc .got has a blrl instruction in it.  Mark it executable.  */

static bfd_boolean
ppc_elf_create_got (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  struct ppc_elf_link_hash_table *htab;
  asection *s;
  flagword flags;

  if (!_bfd_elf_create_got_section (abfd, info))
    return FALSE;

  htab = ppc_elf_hash_table (info);
  htab->got = s = bfd_get_section_by_name (abfd, ".got");
  if (s == NULL)
    abort ();

  flags = (SEC_ALLOC | SEC_LOAD | SEC_CODE | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);
  if (!bfd_set_section_flags (abfd, s, flags))
    return FALSE;

  htab->relgot = bfd_make_section (abfd, ".rela.got");
  if (!htab->relgot
      || ! bfd_set_section_flags (abfd, htab->relgot,
				  (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
				   | SEC_IN_MEMORY | SEC_LINKER_CREATED
				   | SEC_READONLY))
      || ! bfd_set_section_alignment (abfd, htab->relgot, 2))
    return FALSE;

  return TRUE;
}

/* We have to create .dynsbss and .rela.sbss here so that they get mapped
   to output sections (just like _bfd_elf_create_dynamic_sections has
   to create .dynbss and .rela.bss).  */

static bfd_boolean
ppc_elf_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  struct ppc_elf_link_hash_table *htab;
  asection *s;
  flagword flags;

  if (!ppc_elf_create_got (abfd, info))
    return FALSE;

  if (!_bfd_elf_create_dynamic_sections (abfd, info))
    return FALSE;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  htab = ppc_elf_hash_table (info);
  htab->dynbss = bfd_get_section_by_name (abfd, ".dynbss");
  htab->dynsbss = s = bfd_make_section (abfd, ".dynsbss");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, SEC_ALLOC))
    return FALSE;

  if (! info->shared)
    {
      htab->relbss = bfd_get_section_by_name (abfd, ".rela.bss");
      htab->relsbss = s = bfd_make_section (abfd, ".rela.sbss");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
	  || ! bfd_set_section_alignment (abfd, s, 2))
	return FALSE;
    }

  htab->relplt = bfd_get_section_by_name (abfd, ".rela.plt");
  htab->plt = s = bfd_get_section_by_name (abfd, ".plt");
  if (s == NULL)
    abort ();

  flags = SEC_ALLOC | SEC_CODE | SEC_IN_MEMORY | SEC_LINKER_CREATED;
  return bfd_set_section_flags (abfd, s, flags);
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static bfd_boolean
ppc_elf_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  struct ppc_elf_link_hash_table *htab;
  asection *s;
  unsigned int power_of_two;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_adjust_dynamic_symbol called for %s\n",
	   h->root.root.string);
#endif

  /* Make sure we know what is going on here.  */
  htab = ppc_elf_hash_table (info);
  BFD_ASSERT (htab->elf.dynobj != NULL
	      && ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT)
		  || h->weakdef != NULL
		  || ((h->elf_link_hash_flags
		       & ELF_LINK_HASH_DEF_DYNAMIC) != 0
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_REF_REGULAR) != 0
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)));

  /* Deal with function syms.  */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      /* Clear procedure linkage table information for any symbol that
	 won't need a .plt entry.  */
      if (! htab->elf.dynamic_sections_created
	  || SYMBOL_CALLS_LOCAL (info, h)
	  || h->plt.refcount <= 0)
	{
	  /* A PLT entry is not required/allowed when:

	  1. We are not using ld.so; because then the PLT entry
	  can't be set up, so we can't use one.

	  2. We know for certain that a call to this symbol
	  will go to this object.

	  3. GC has rendered the entry unused.  */
	  h->plt.offset = (bfd_vma) -1;
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	}
      return TRUE;
    }
  else
    h->plt.offset = (bfd_vma) -1;

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
		  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
      if (ELIMINATE_COPY_RELOCS)
	h->elf_link_hash_flags
	  = ((h->elf_link_hash_flags & ~ELF_LINK_NON_GOT_REF)
	     | (h->weakdef->elf_link_hash_flags & ELF_LINK_NON_GOT_REF));
      return TRUE;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  */

  /* If we are creating a shared library, we must presume that the
     only references to the symbol are via the global offset table.
     For such cases we need not do anything here; the relocations will
     be handled correctly by relocate_section.  */
  if (info->shared)
    return TRUE;

  /* If there are no references to this symbol that do not use the
     GOT, we don't need to generate a copy reloc.  */
  if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0)
    return TRUE;

  if (ELIMINATE_COPY_RELOCS)
    {
      struct ppc_elf_dyn_relocs *p;
      for (p = ppc_elf_hash_entry (h)->dyn_relocs; p != NULL; p = p->next)
	{
	  s = p->sec->output_section;
	  if (s != NULL && (s->flags & SEC_READONLY) != 0)
	    break;
	}

      /* If we didn't find any dynamic relocs in read-only sections, then
	 we'll be keeping the dynamic relocs and avoiding the copy reloc.  */
      if (p == NULL)
	{
	  h->elf_link_hash_flags &= ~ELF_LINK_NON_GOT_REF;
	  return TRUE;
	}
    }

  /* We must allocate the symbol in our .dynbss section, which will
     become part of the .bss section of the executable.  There will be
     an entry for this symbol in the .dynsym section.  The dynamic
     object will contain position independent code, so all references
     from the dynamic object to this symbol will go through the global
     offset table.  The dynamic linker will use the .dynsym entry to
     determine the address it must put in the global offset table, so
     both the dynamic object and the regular object will refer to the
     same memory location for the variable.

     Of course, if the symbol is sufficiently small, we must instead
     allocate it in .sbss.  FIXME: It would be better to do this if and
     only if there were actually SDAREL relocs for that symbol.  */

  if (h->size <= elf_gp_size (htab->elf.dynobj))
    s = htab->dynsbss;
  else
    s = htab->dynbss;
  BFD_ASSERT (s != NULL);

  /* We must generate a R_PPC_COPY reloc to tell the dynamic linker to
     copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rela.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      if (h->size <= elf_gp_size (htab->elf.dynobj))
	srel = htab->relsbss;
      else
	srel = htab->relbss;
      BFD_ASSERT (srel != NULL);
      srel->_raw_size += sizeof (Elf32_External_Rela);
      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_COPY;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 4)
    power_of_two = 4;

  /* Apply the required alignment.  */
  s->_raw_size = BFD_ALIGN (s->_raw_size,
			    (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (htab->elf.dynobj, s))
    {
      if (! bfd_set_section_alignment (htab->elf.dynobj, s, power_of_two))
	return FALSE;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return TRUE;
}

/* This is the condition under which finish_dynamic_symbol will be
   called from elflink.h.  If elflink.h doesn't call our
   finish_dynamic_symbol routine, we'll need to do something about
   initializing any .plt and .got entries in relocate_section.  */
#define WILL_CALL_FINISH_DYNAMIC_SYMBOL(DYN, SHARED, H) \
  ((DYN)								\
   && ((SHARED)								\
       || ((H)->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)	\
   && ((H)->dynindx != -1						\
       || ((H)->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0))

/* Of those relocs that might be copied as dynamic relocs, this macro
   selects those that must be copied when linking a shared library,
   even when the symbol is local.  */

#define MUST_BE_DYN_RELOC(RTYPE)		\
  ((RTYPE) != R_PPC_REL24			\
   && (RTYPE) != R_PPC_REL14			\
   && (RTYPE) != R_PPC_REL14_BRTAKEN		\
   && (RTYPE) != R_PPC_REL14_BRNTAKEN		\
   && (RTYPE) != R_PPC_REL32)

/* Allocate space in associated reloc sections for dynamic relocs.  */

static bfd_boolean
allocate_dynrelocs (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct bfd_link_info *info = (struct bfd_link_info *) inf;
  struct ppc_elf_link_hash_entry *eh;
  struct ppc_elf_link_hash_table *htab;
  struct ppc_elf_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_indirect)
    return TRUE;

  if (h->root.type == bfd_link_hash_warning)
    /* When warning symbols are created, they **replace** the "real"
       entry in the hash table, thus we never get to see the real
       symbol in a hash traversal.  So look at it now.  */
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  htab = ppc_elf_hash_table (info);
  if (htab->elf.dynamic_sections_created
      && h->plt.refcount > 0)
    {
      /* Make sure this symbol is output as a dynamic symbol.  */
      if (h->dynindx == -1
	  && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	{
	  if (! bfd_elf32_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      if (info->shared
	  || WILL_CALL_FINISH_DYNAMIC_SYMBOL (1, 0, h))
	{
	  asection *s = htab->plt;

	  /* If this is the first .plt entry, make room for the special
	     first entry.  */
	  if (s->_raw_size == 0)
	    s->_raw_size += PLT_INITIAL_ENTRY_SIZE;

	  /* The PowerPC PLT is actually composed of two parts, the
	     first part is 2 words (for a load and a jump), and then
	     there is a remaining word available at the end.  */
	  h->plt.offset = (PLT_INITIAL_ENTRY_SIZE
			   + (PLT_SLOT_SIZE
			      * ((s->_raw_size - PLT_INITIAL_ENTRY_SIZE)
				 / PLT_ENTRY_SIZE)));

	  /* If this symbol is not defined in a regular file, and we
	     are not generating a shared library, then set the symbol
	     to this location in the .plt.  This is required to make
	     function pointers compare as equal between the normal
	     executable and the shared library.  */
	  if (! info->shared
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	    {
	      h->root.u.def.section = s;
	      h->root.u.def.value = h->plt.offset;
	    }

	  /* Make room for this entry.  After the 8192nd entry, room
	     for two entries is allocated.  */
	  s->_raw_size += PLT_ENTRY_SIZE;
	  if ((s->_raw_size - PLT_INITIAL_ENTRY_SIZE) / PLT_ENTRY_SIZE
	      > PLT_NUM_SINGLE_ENTRIES)
	    s->_raw_size += PLT_ENTRY_SIZE;

	  /* We also need to make an entry in the .rela.plt section.  */
	  htab->relplt->_raw_size += sizeof (Elf32_External_Rela);
	}
      else
	{
	  h->plt.offset = (bfd_vma) -1;
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	}
    }
  else
    {
      h->plt.offset = (bfd_vma) -1;
      h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
    }

  eh = (struct ppc_elf_link_hash_entry *) h;
  if (eh->elf.got.refcount > 0)
    {
      /* Make sure this symbol is output as a dynamic symbol.  */
      if (eh->elf.dynindx == -1
	  && (eh->elf.elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	{
	  if (!bfd_elf32_link_record_dynamic_symbol (info, &eh->elf))
	    return FALSE;
	}

      if (eh->tls_mask == (TLS_TLS | TLS_LD)
	  && !(eh->elf.elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC))
	/* If just an LD reloc, we'll just use htab->tlsld_got.offset.  */
	eh->elf.got.offset = (bfd_vma) -1;
      else
	{
	  bfd_boolean dyn;
	  eh->elf.got.offset = htab->got->_raw_size;
	  if ((eh->tls_mask & TLS_TLS) != 0)
	    {
	      if ((eh->tls_mask & TLS_LD) != 0)
		htab->got->_raw_size += 8;
	      if ((eh->tls_mask & TLS_GD) != 0)
		htab->got->_raw_size += 8;
	      if ((eh->tls_mask & (TLS_TPREL | TLS_TPRELGD)) != 0)
		htab->got->_raw_size += 4;
	      if ((eh->tls_mask & TLS_DTPREL) != 0)
		htab->got->_raw_size += 4;
	    }
	  else
	    htab->got->_raw_size += 4;
	  dyn = htab->elf.dynamic_sections_created;
	  if (info->shared
	      || WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, 0, &eh->elf))
	    {
	      /* All the entries we allocated need relocs.  */
	      htab->relgot->_raw_size
		+= ((htab->got->_raw_size - eh->elf.got.offset) / 4
		    * sizeof (Elf32_External_Rela));
	      /* Except LD only needs one.  */
	      if ((eh->tls_mask & TLS_LD) != 0)
		htab->relgot->_raw_size -= sizeof (Elf32_External_Rela);
	    }
	}
    }
  else
    eh->elf.got.offset = (bfd_vma) -1;

  if (eh->dyn_relocs == NULL)
    return TRUE;

  /* In the shared -Bsymbolic case, discard space allocated for
     dynamic pc-relative relocs against symbols which turn out to be
     defined in regular objects.  For the normal shared case, discard
     space for relocs that have become local due to symbol visibility
     changes.  */
  if (info->shared)
    {
      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
	  && ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0
	      || info->symbolic))
	{
	  struct ppc_elf_dyn_relocs **pp;

	  for (pp = &eh->dyn_relocs; (p = *pp) != NULL; )
	    {
	      p->count -= p->pc_count;
	      p->pc_count = 0;
	      if (p->count == 0)
		*pp = p->next;
	      else
		pp = &p->next;
	    }
	}
    }
  else if (ELIMINATE_COPY_RELOCS)
    {
      /* For the non-shared case, discard space for relocs against
	 symbols which turn out to need copy relocs or are not
	 dynamic.  */

      if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* Make sure this symbol is output as a dynamic symbol.
	     Undefined weak syms won't yet be marked as dynamic.  */
	  if (h->dynindx == -1
	      && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	    {
	      if (! bfd_elf64_link_record_dynamic_symbol (info, h))
		return FALSE;
	    }

	  /* If that succeeded, we know we'll be keeping all the
	     relocs.  */
	  if (h->dynindx != -1)
	    goto keep;
	}

      eh->dyn_relocs = NULL;

    keep: ;
    }

  /* Finally, allocate space.  */
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      asection *sreloc = elf_section_data (p->sec)->sreloc;
      sreloc->_raw_size += p->count * sizeof (Elf32_External_Rela);
    }

  return TRUE;
}

/* Find any dynamic relocs that apply to read-only sections.  */

static bfd_boolean
readonly_dynrelocs (h, info)
     struct elf_link_hash_entry *h;
     PTR info;
{
  struct ppc_elf_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_indirect)
    return TRUE;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  for (p = ppc_elf_hash_entry (h)->dyn_relocs; p != NULL; p = p->next)
    {
      asection *s = p->sec->output_section;

      if (s != NULL
	  && ((s->flags & (SEC_READONLY | SEC_ALLOC))
	      == (SEC_READONLY | SEC_ALLOC)))
	{
	  ((struct bfd_link_info *) info)->flags |= DF_TEXTREL;

	  /* Not an error, just cut short the traversal.  */
	  return FALSE;
	}
    }
  return TRUE;
}

/* Set the sizes of the dynamic sections.  */

static bfd_boolean
ppc_elf_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  struct ppc_elf_link_hash_table *htab;
  asection *s;
  bfd_boolean relocs;
  bfd *ibfd;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_size_dynamic_sections called\n");
#endif

  htab = ppc_elf_hash_table (info);
  BFD_ASSERT (htab->elf.dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (! info->shared)
	{
	  s = bfd_get_section_by_name (htab->elf.dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}
    }

  if (htab->tlsld_got.refcount > 0)
    {
      htab->tlsld_got.offset = htab->got->_raw_size;
      htab->got->_raw_size += 8;
      if (info->shared)
	htab->relgot->_raw_size += sizeof (Elf32_External_Rela);
    }
  else
    htab->tlsld_got.offset = (bfd_vma) -1;

  /* Set up .got offsets for local syms, and space for local dynamic
     relocs.  */
  for (ibfd = info->input_bfds; ibfd != NULL; ibfd = ibfd->link_next)
    {
      bfd_signed_vma *local_got;
      bfd_signed_vma *end_local_got;
      char *lgot_masks;
      bfd_size_type locsymcount;
      Elf_Internal_Shdr *symtab_hdr;
      asection *srel;

      if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour)
	continue;

      for (s = ibfd->sections; s != NULL; s = s->next)
	{
	  struct ppc_elf_dyn_relocs *p;

	  for (p = ((struct ppc_elf_dyn_relocs *)
		    elf_section_data (s)->local_dynrel);
	       p != NULL;
	       p = p->next)
	    {
	      if (!bfd_is_abs_section (p->sec)
		  && bfd_is_abs_section (p->sec->output_section))
		{
		  /* Input section has been discarded, either because
		     it is a copy of a linkonce section or due to
		     linker script /DISCARD/, so we'll be discarding
		     the relocs too.  */
		}
	      else if (p->count != 0)
		{
		  elf_section_data (p->sec)->sreloc->_raw_size
		    += p->count * sizeof (Elf32_External_Rela);
		  if ((p->sec->output_section->flags
		       & (SEC_READONLY | SEC_ALLOC))
		      == (SEC_READONLY | SEC_ALLOC))
		    info->flags |= DF_TEXTREL;
		}
	    }
	}

      local_got = elf_local_got_refcounts (ibfd);
      if (!local_got)
	continue;

      symtab_hdr = &elf_tdata (ibfd)->symtab_hdr;
      locsymcount = symtab_hdr->sh_info;
      end_local_got = local_got + locsymcount;
      lgot_masks = (char *) end_local_got;
      s = htab->got;
      srel = htab->relgot;
      for (; local_got < end_local_got; ++local_got, ++lgot_masks)
	if (*local_got > 0)
	  {
	    if (*lgot_masks == (TLS_TLS | TLS_LD))
	      {
		/* If just an LD reloc, we'll just use
		   htab->tlsld_got.offset.  */
		if (htab->tlsld_got.offset == (bfd_vma) -1)
		  {
		    htab->tlsld_got.offset = s->_raw_size;
		    s->_raw_size += 8;
		    if (info->shared)
		      srel->_raw_size += sizeof (Elf32_External_Rela);
		  }
		*local_got = (bfd_vma) -1;
	      }
	    else
	      {
		*local_got = s->_raw_size;
		if ((*lgot_masks & TLS_TLS) != 0)
		  {
		    if ((*lgot_masks & TLS_GD) != 0)
		      s->_raw_size += 8;
		    if ((*lgot_masks & (TLS_TPREL | TLS_TPRELGD)) != 0)
		      s->_raw_size += 4;
		    if ((*lgot_masks & TLS_DTPREL) != 0)
		      s->_raw_size += 4;
		  }
		else
		  s->_raw_size += 4;
		if (info->shared)
		  srel->_raw_size += ((s->_raw_size - *local_got) / 4
				      * sizeof (Elf32_External_Rela));
	      }
	  }
	else
	  *local_got = (bfd_vma) -1;
    }

  /* Allocate space for global sym dynamic relocs.  */
  elf_link_hash_traverse (elf_hash_table (info), allocate_dynrelocs, info);

  /* We've now determined the sizes of the various dynamic sections.
     Allocate memory for them.  */
  relocs = FALSE;
  for (s = htab->elf.dynobj->sections; s != NULL; s = s->next)
    {
      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      if (s == htab->plt
	  || s == htab->got
	  || (htab->sdata != NULL && s == htab->sdata->section)
	  || (htab->sdata2 != NULL && s == htab->sdata2->section))
	{
	  /* Strip this section if we don't need it; see the
	     comment below.  */
	}
      else if (strncmp (bfd_get_section_name (dynobj, s), ".rela", 5) == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* If we don't need this section, strip it from the
		 output file.  This is mostly to handle .rela.bss and
		 .rela.plt.  We must create both sections in
		 create_dynamic_sections, because they must be created
		 before the linker maps input sections to output
		 sections.  The linker does that before
		 adjust_dynamic_symbol is called, and it is that
		 function which decides whether anything needs to go
		 into these sections.  */
	    }
	  else
	    {
	      /* Remember whether there are any relocation sections.  */
	      relocs = TRUE;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (s->_raw_size == 0)
	{
	  _bfd_strip_section_from_output (info, s);
	  continue;
	}

      /* Allocate memory for the section contents.  */
      s->contents = (bfd_byte *) bfd_zalloc (htab->elf.dynobj, s->_raw_size);
      if (s->contents == NULL)
	return FALSE;
    }

  if (htab->elf.dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in ppc_elf_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  bfd_elf32_add_dynamic_entry (info, (bfd_vma) (TAG), (bfd_vma) (VAL))

      if (!info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return FALSE;
	}

      if (htab->plt != NULL && htab->plt->_raw_size != 0)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return FALSE;
	}

      if (relocs)
	{
	  if (!add_dynamic_entry (DT_RELA, 0)
	      || !add_dynamic_entry (DT_RELASZ, 0)
	      || !add_dynamic_entry (DT_RELAENT, sizeof (Elf32_External_Rela)))
	    return FALSE;
	}

      /* If any dynamic relocs apply to a read-only section, then we
	 need a DT_TEXTREL entry.  */
      if ((info->flags & DF_TEXTREL) == 0)
	elf_link_hash_traverse (elf_hash_table (info), readonly_dynrelocs,
				(PTR) info);

      if ((info->flags & DF_TEXTREL) != 0)
	{
	  if (!add_dynamic_entry (DT_TEXTREL, 0))
	    return FALSE;
	}
    }
#undef add_dynamic_entry

  return TRUE;
}

static bfd_boolean
update_local_sym_info (abfd, symtab_hdr, r_symndx, tls_type)
     bfd *abfd;
     Elf_Internal_Shdr *symtab_hdr;
     unsigned long r_symndx;
     int tls_type;
{
  bfd_signed_vma *local_got_refcounts = elf_local_got_refcounts (abfd);
  char *local_got_tls_masks;

  if (local_got_refcounts == NULL)
    {
      bfd_size_type size = symtab_hdr->sh_info;

      size *= sizeof (*local_got_refcounts) + sizeof (*local_got_tls_masks);
      local_got_refcounts = (bfd_signed_vma *) bfd_zalloc (abfd, size);
      if (local_got_refcounts == NULL)
	return FALSE;
      elf_local_got_refcounts (abfd) = local_got_refcounts;
    }

  local_got_refcounts[r_symndx] += 1;
  local_got_tls_masks = (char *) (local_got_refcounts + symtab_hdr->sh_info);
  local_got_tls_masks[r_symndx] |= tls_type;
  return TRUE;
}

static void
bad_shared_reloc (abfd, r_type)
     bfd *abfd;
     enum elf_ppc_reloc_type r_type;
{
  (*_bfd_error_handler)
    (_("%s: relocation %s cannot be used when making a shared object"),
     bfd_archive_filename (abfd),
     ppc_elf_howto_table[(int) r_type]->name);
  bfd_set_error (bfd_error_bad_value);
}

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static bfd_boolean
ppc_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  struct ppc_elf_link_hash_table *htab;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sreloc;

  if (info->relocateable)
    return TRUE;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_check_relocs called for section %s in %s\n",
	   bfd_get_section_name (abfd, sec),
	   bfd_archive_filename (abfd));
#endif

  /* Create the linker generated sections all the time so that the
     special symbols are created.  */

  htab = ppc_elf_hash_table (info);
  if (htab->sdata == NULL)
    {
      htab->sdata = elf_linker_section (abfd, LINKER_SECTION_SDATA);
      if (htab->sdata == NULL)
	htab->sdata = ppc_elf_create_linker_section (abfd, info,
						     LINKER_SECTION_SDATA);
      if (htab->sdata == NULL)
	return FALSE;
    }

  if (htab->sdata2 == NULL)
    {
      htab->sdata2 = elf_linker_section (abfd, LINKER_SECTION_SDATA2);
      if (htab->sdata2 == NULL)
	htab->sdata2 = ppc_elf_create_linker_section (abfd, info,
						      LINKER_SECTION_SDATA2);
      if (htab->sdata2 == NULL)
	return FALSE;
    }

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sreloc = NULL;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned long r_symndx;
      enum elf_ppc_reloc_type r_type;
      struct elf_link_hash_entry *h;
      int tls_type = 0;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      /* If a relocation refers to _GLOBAL_OFFSET_TABLE_, create the .got.
	 This shows up in particular in an R_PPC_ADDR32 in the eabi
	 startup code.  */
      if (h && strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	{
	  if (htab->got == NULL)
	    {
	      if (htab->elf.dynobj == NULL)
		htab->elf.dynobj = abfd;
	      if (!ppc_elf_create_got (htab->elf.dynobj, info))
		return FALSE;
	    }
	}

      r_type = (enum elf_ppc_reloc_type) ELF32_R_TYPE (rel->r_info);
      switch (r_type)
	{
	case R_PPC_GOT_TLSLD16:
	case R_PPC_GOT_TLSLD16_LO:
	case R_PPC_GOT_TLSLD16_HI:
	case R_PPC_GOT_TLSLD16_HA:
	  htab->tlsld_got.refcount += 1;
	  tls_type = TLS_TLS | TLS_LD;
	  goto dogottls;

	case R_PPC_GOT_TLSGD16:
	case R_PPC_GOT_TLSGD16_LO:
	case R_PPC_GOT_TLSGD16_HI:
	case R_PPC_GOT_TLSGD16_HA:
	  tls_type = TLS_TLS | TLS_GD;
	  goto dogottls;

	case R_PPC_GOT_TPREL16:
	case R_PPC_GOT_TPREL16_LO:
	case R_PPC_GOT_TPREL16_HI:
	case R_PPC_GOT_TPREL16_HA:
	  if (info->shared)
	    info->flags |= DF_STATIC_TLS;
	  tls_type = TLS_TLS | TLS_TPREL;
	  goto dogottls;

	case R_PPC_GOT_DTPREL16:
	case R_PPC_GOT_DTPREL16_LO:
	case R_PPC_GOT_DTPREL16_HI:
	case R_PPC_GOT_DTPREL16_HA:
	  tls_type = TLS_TLS | TLS_DTPREL;
	dogottls:
	  sec->has_tls_reloc = 1;
	  /* Fall thru */

	  /* GOT16 relocations */
	case R_PPC_GOT16:
	case R_PPC_GOT16_LO:
	case R_PPC_GOT16_HI:
	case R_PPC_GOT16_HA:
	  /* This symbol requires a global offset table entry.  */
	  if (htab->got == NULL)
	    {
	      if (htab->elf.dynobj == NULL)
		htab->elf.dynobj = abfd;
	      if (!ppc_elf_create_got (htab->elf.dynobj, info))
		return FALSE;
	    }
	  if (h != NULL)
	    {
	      h->got.refcount += 1;
	      ppc_elf_hash_entry (h)->tls_mask |= tls_type;
	    }
	  else
	    /* This is a global offset table entry for a local symbol.  */
	    if (!update_local_sym_info (abfd, symtab_hdr, r_symndx, tls_type))
	      return FALSE;
	  break;

	  /* Indirect .sdata relocation.  */
	case R_PPC_EMB_SDAI16:
	  if (info->shared)
	    {
	      bad_shared_reloc (abfd, r_type);
	      return FALSE;
	    }
	  if (!bfd_elf32_create_pointer_linker_section (abfd, info,
							htab->sdata, h, rel))
	    return FALSE;
	  break;

	  /* Indirect .sdata2 relocation.  */
	case R_PPC_EMB_SDA2I16:
	  if (info->shared)
	    {
	      bad_shared_reloc (abfd, r_type);
	      return FALSE;
	    }
	  if (!bfd_elf32_create_pointer_linker_section (abfd, info,
							htab->sdata2, h, rel))
	    return FALSE;
	  break;

	case R_PPC_SDAREL16:
	case R_PPC_EMB_SDA2REL:
	case R_PPC_EMB_SDA21:
	case R_PPC_EMB_RELSDA:
	case R_PPC_EMB_NADDR32:
	case R_PPC_EMB_NADDR16:
	case R_PPC_EMB_NADDR16_LO:
	case R_PPC_EMB_NADDR16_HI:
	case R_PPC_EMB_NADDR16_HA:
	  if (info->shared)
	    {
	      bad_shared_reloc (abfd, r_type);
	      return FALSE;
	    }
	  break;

	case R_PPC_PLT32:
	case R_PPC_PLTREL24:
	case R_PPC_PLTREL32:
	case R_PPC_PLT16_LO:
	case R_PPC_PLT16_HI:
	case R_PPC_PLT16_HA:
#ifdef DEBUG
	  fprintf (stderr, "Reloc requires a PLT entry\n");
#endif
	  /* This symbol requires a procedure linkage table entry.  We
	     actually build the entry in finish_dynamic_symbol,
	     because this might be a case of linking PIC code without
	     linking in any dynamic objects, in which case we don't
	     need to generate a procedure linkage table after all.  */

	  if (h == NULL)
	    {
	      /* It does not make sense to have a procedure linkage
		 table entry for a local symbol.  */
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }

	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	  h->plt.refcount++;
	  break;

	  /* The following relocations don't need to propagate the
	     relocation if linking a shared object since they are
	     section relative.  */
	case R_PPC_SECTOFF:
	case R_PPC_SECTOFF_LO:
	case R_PPC_SECTOFF_HI:
	case R_PPC_SECTOFF_HA:
	case R_PPC_DTPREL16:
	case R_PPC_DTPREL16_LO:
	case R_PPC_DTPREL16_HI:
	case R_PPC_DTPREL16_HA:
	case R_PPC_TOC16:
	  break;

	  /* This are just markers.  */
	case R_PPC_TLS:
	case R_PPC_EMB_MRKREF:
	case R_PPC_NONE:
	case R_PPC_max:
	  break;

	  /* These should only appear in dynamic objects.  */
	case R_PPC_COPY:
	case R_PPC_GLOB_DAT:
	case R_PPC_JMP_SLOT:
	case R_PPC_RELATIVE:
	  break;

	  /* These aren't handled yet.  We'll report an error later.  */
	case R_PPC_ADDR30:
	case R_PPC_EMB_RELSEC16:
	case R_PPC_EMB_RELST_LO:
	case R_PPC_EMB_RELST_HI:
	case R_PPC_EMB_RELST_HA:
	case R_PPC_EMB_BIT_FLD:
	  break;

	  /* This refers only to functions defined in the shared library.  */
	case R_PPC_LOCAL24PC:
	  break;

	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	case R_PPC_GNU_VTINHERIT:
	  if (!_bfd_elf32_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return FALSE;
	  break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_PPC_GNU_VTENTRY:
	  if (!_bfd_elf32_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return FALSE;
	  break;

	  /* We shouldn't really be seeing these.  */
	case R_PPC_TPREL32:
	  if (info->shared)
	    info->flags |= DF_STATIC_TLS;
	  goto dodyn;

	  /* Nor these.  */
	case R_PPC_DTPMOD32:
	case R_PPC_DTPREL32:
	  goto dodyn;

	case R_PPC_TPREL16:
	case R_PPC_TPREL16_LO:
	case R_PPC_TPREL16_HI:
	case R_PPC_TPREL16_HA:
	  if (info->shared)
	    info->flags |= DF_STATIC_TLS;
	  goto dodyn;

	  /* When creating a shared object, we must copy these
	     relocs into the output file.  We create a reloc
	     section in dynobj and make room for the reloc.  */
	case R_PPC_REL24:
	case R_PPC_REL14:
	case R_PPC_REL14_BRTAKEN:
	case R_PPC_REL14_BRNTAKEN:
	case R_PPC_REL32:
	  if (h == NULL
	      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	  /* fall through */

	case R_PPC_ADDR32:
	case R_PPC_ADDR24:
	case R_PPC_ADDR16:
	case R_PPC_ADDR16_LO:
	case R_PPC_ADDR16_HI:
	case R_PPC_ADDR16_HA:
	case R_PPC_ADDR14:
	case R_PPC_ADDR14_BRTAKEN:
	case R_PPC_ADDR14_BRNTAKEN:
	case R_PPC_UADDR32:
	case R_PPC_UADDR16:
	  if (h != NULL && !info->shared)
	    {
	      /* We may need a plt entry if the symbol turns out to be
		 a function defined in a dynamic object.  */
	      h->plt.refcount++;

	      /* We may need a copy reloc too.  */
	      h->elf_link_hash_flags |= ELF_LINK_NON_GOT_REF;
	    }

	dodyn:
	  /* If we are creating a shared library, and this is a reloc
	     against a global symbol, or a non PC relative reloc
	     against a local symbol, then we need to copy the reloc
	     into the shared library.  However, if we are linking with
	     -Bsymbolic, we do not need to copy a reloc against a
	     global symbol which is defined in an object we are
	     including in the link (i.e., DEF_REGULAR is set).  At
	     this point we have not seen all the input files, so it is
	     possible that DEF_REGULAR is not set now but will be set
	     later (it is never cleared).  In case of a weak definition,
	     DEF_REGULAR may be cleared later by a strong definition in
	     a shared library.  We account for that possibility below by
	     storing information in the dyn_relocs field of the hash
	     table entry.  A similar situation occurs when creating
	     shared libraries and symbol visibility changes render the
	     symbol local.

	     If on the other hand, we are creating an executable, we
	     may need to keep relocations for symbols satisfied by a
	     dynamic library if we manage to avoid copy relocs for the
	     symbol.  */
	  if ((info->shared
	       && (MUST_BE_DYN_RELOC (r_type)
		   || (h != NULL
		       && (! info->symbolic
			   || h->root.type == bfd_link_hash_defweak
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))))
	      || (ELIMINATE_COPY_RELOCS
		  && !info->shared
		  && (sec->flags & SEC_ALLOC) != 0
		  && h != NULL
		  && (h->root.type == bfd_link_hash_defweak
		      || (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)))
	    {
	      struct ppc_elf_dyn_relocs *p;
	      struct ppc_elf_dyn_relocs **head;

#ifdef DEBUG
	      fprintf (stderr, "ppc_elf_check_relocs need to create relocation for %s\n",
		       (h && h->root.root.string
			? h->root.root.string : "<unknown>"));
#endif
	      if (sreloc == NULL)
		{
		  const char *name;

		  name = (bfd_elf_string_from_elf_section
			  (abfd,
			   elf_elfheader (abfd)->e_shstrndx,
			   elf_section_data (sec)->rel_hdr.sh_name));
		  if (name == NULL)
		    return FALSE;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (abfd, sec),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (htab->elf.dynobj, name);
		  if (sreloc == NULL)
		    {
		      flagword flags;

		      sreloc = bfd_make_section (htab->elf.dynobj, name);
		      flags = (SEC_HAS_CONTENTS | SEC_READONLY
			       | SEC_IN_MEMORY | SEC_LINKER_CREATED);
		      if ((sec->flags & SEC_ALLOC) != 0)
			flags |= SEC_ALLOC | SEC_LOAD;
		      if (sreloc == NULL
			  || ! bfd_set_section_flags (htab->elf.dynobj,
						      sreloc, flags)
			  || ! bfd_set_section_alignment (htab->elf.dynobj,
							  sreloc, 2))
			return FALSE;
		    }
		  elf_section_data (sec)->sreloc = sreloc;
		}

	      /* If this is a global symbol, we count the number of
		 relocations we need for this symbol.  */
	      if (h != NULL)
		{
		  head = &ppc_elf_hash_entry (h)->dyn_relocs;
		}
	      else
		{
		  /* Track dynamic relocs needed for local syms too.
		     We really need local syms available to do this
		     easily.  Oh well.  */

		  asection *s;
		  s = bfd_section_from_r_symndx (abfd, &htab->sym_sec,
						 sec, r_symndx);
		  if (s == NULL)
		    return FALSE;

		  head = ((struct ppc_elf_dyn_relocs **)
			  &elf_section_data (s)->local_dynrel);
		}

	      p = *head;
	      if (p == NULL || p->sec != sec)
		{
		  p = ((struct ppc_elf_dyn_relocs *)
		       bfd_alloc (htab->elf.dynobj, sizeof *p));
		  if (p == NULL)
		    return FALSE;
		  p->next = *head;
		  *head = p;
		  p->sec = sec;
		  p->count = 0;
		  p->pc_count = 0;
		}

	      p->count += 1;
	      if (!MUST_BE_DYN_RELOC (r_type))
		p->pc_count += 1;
	    }

	  break;
	}
    }

  return TRUE;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
ppc_elf_gc_mark_hook (sec, info, rel, h, sym)
     asection *sec;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_PPC_GNU_VTINHERIT:
	case R_PPC_GNU_VTENTRY:
	  break;

	default:
	  switch (h->root.type)
	    {
	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      return h->root.u.def.section;

	    case bfd_link_hash_common:
	      return h->root.u.c.p->section;

	    default:
	      break;
	    }
	}
    }
  else
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}

/* Update the got, plt and dynamic reloc reference counts for the
   section being removed.  */

static bfd_boolean
ppc_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  struct ppc_elf_link_hash_table *htab;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;

  elf_section_data (sec)->local_dynrel = NULL;

  htab = ppc_elf_hash_table (info);
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    {
      unsigned long r_symndx;
      enum elf_ppc_reloc_type r_type;
      struct elf_link_hash_entry *h = NULL;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx >= symtab_hdr->sh_info)
	{
	  struct ppc_elf_dyn_relocs **pp, *p;
	  struct ppc_elf_link_hash_entry *eh;

	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  eh = (struct ppc_elf_link_hash_entry *) h;

	  for (pp = &eh->dyn_relocs; (p = *pp) != NULL; pp = &p->next)
	    if (p->sec == sec)
	      {
		/* Everything must go for SEC.  */
		*pp = p->next;
		break;
	      }
	}

      r_type = (enum elf_ppc_reloc_type) ELF32_R_TYPE (rel->r_info);
      switch (r_type)
	{
	case R_PPC_GOT_TLSLD16:
	case R_PPC_GOT_TLSLD16_LO:
	case R_PPC_GOT_TLSLD16_HI:
	case R_PPC_GOT_TLSLD16_HA:
	  htab->tlsld_got.refcount -= 1;
	  /* Fall thru */

	case R_PPC_GOT_TLSGD16:
	case R_PPC_GOT_TLSGD16_LO:
	case R_PPC_GOT_TLSGD16_HI:
	case R_PPC_GOT_TLSGD16_HA:
	case R_PPC_GOT_TPREL16:
	case R_PPC_GOT_TPREL16_LO:
	case R_PPC_GOT_TPREL16_HI:
	case R_PPC_GOT_TPREL16_HA:
	case R_PPC_GOT_DTPREL16:
	case R_PPC_GOT_DTPREL16_LO:
	case R_PPC_GOT_DTPREL16_HI:
	case R_PPC_GOT_DTPREL16_HA:
	case R_PPC_GOT16:
	case R_PPC_GOT16_LO:
	case R_PPC_GOT16_HI:
	case R_PPC_GOT16_HA:
	  if (h != NULL)
	    {
	      if (h->got.refcount > 0)
		h->got.refcount--;
	    }
	  else if (local_got_refcounts != NULL)
	    {
	      if (local_got_refcounts[r_symndx] > 0)
		local_got_refcounts[r_symndx]--;
	    }
	  break;

	case R_PPC_REL24:
	case R_PPC_REL14:
	case R_PPC_REL14_BRTAKEN:
	case R_PPC_REL14_BRNTAKEN:
	case R_PPC_REL32:
	  if (h == NULL
	      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	  /* Fall thru */

	case R_PPC_ADDR32:
	case R_PPC_ADDR24:
	case R_PPC_ADDR16:
	case R_PPC_ADDR16_LO:
	case R_PPC_ADDR16_HI:
	case R_PPC_ADDR16_HA:
	case R_PPC_ADDR14:
	case R_PPC_ADDR14_BRTAKEN:
	case R_PPC_ADDR14_BRNTAKEN:
	case R_PPC_UADDR32:
	case R_PPC_UADDR16:
	case R_PPC_PLT32:
	case R_PPC_PLTREL24:
	case R_PPC_PLT16_LO:
	case R_PPC_PLT16_HI:
	case R_PPC_PLT16_HA:
	  if (h != NULL)
	    {
	      if (h->plt.refcount > 0)
		h->plt.refcount--;
	    }
	  break;

	default:
	  break;
	}
    }
  return TRUE;
}

/* Set htab->tls_sec and htab->tls_get_addr.  */

bfd_boolean
ppc_elf_tls_setup (obfd, info)
     bfd *obfd;
     struct bfd_link_info *info;
{
  asection *tls;
  struct ppc_elf_link_hash_table *htab;

  htab = ppc_elf_hash_table (info);
  htab->tls_get_addr = elf_link_hash_lookup (&htab->elf, "__tls_get_addr",
					     FALSE, FALSE, TRUE);

  for (tls = obfd->sections; tls != NULL; tls = tls->next)
    if ((tls->flags & (SEC_THREAD_LOCAL | SEC_LOAD))
	== (SEC_THREAD_LOCAL | SEC_LOAD))
      break;
  htab->tls_sec = tls;

  return tls != NULL;
}

/* Run through all the TLS relocs looking for optimization
   opportunities.  */

bfd_boolean
ppc_elf_tls_optimize (obfd, info)
     bfd *obfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  bfd *ibfd;
  asection *sec;
  struct ppc_elf_link_hash_table *htab;

  if (info->relocateable || info->shared)
    return TRUE;

  htab = ppc_elf_hash_table (info);
  for (ibfd = info->input_bfds; ibfd != NULL; ibfd = ibfd->link_next)
    {
      Elf_Internal_Sym *locsyms = NULL;
      Elf_Internal_Shdr *symtab_hdr = &elf_tdata (ibfd)->symtab_hdr;

      for (sec = ibfd->sections; sec != NULL; sec = sec->next)
	if (sec->has_tls_reloc && !bfd_is_abs_section (sec->output_section))
	  {
	    Elf_Internal_Rela *relstart, *rel, *relend;
	    int expecting_tls_get_addr;

	    /* Read the relocations.  */
	    relstart = _bfd_elf32_link_read_relocs (ibfd, sec, (PTR) NULL,
						    (Elf_Internal_Rela *) NULL,
						    info->keep_memory);
	    if (relstart == NULL)
	      return FALSE;

	    expecting_tls_get_addr = 0;
	    relend = relstart + sec->reloc_count;
	    for (rel = relstart; rel < relend; rel++)
	      {
		enum elf_ppc_reloc_type r_type;
		unsigned long r_symndx;
		struct elf_link_hash_entry *h = NULL;
		char *tls_mask;
		char tls_set, tls_clear;
		bfd_boolean is_local;

		r_symndx = ELF32_R_SYM (rel->r_info);
		if (r_symndx >= symtab_hdr->sh_info)
		  {
		    struct elf_link_hash_entry **sym_hashes;

		    sym_hashes = elf_sym_hashes (ibfd);
		    h = sym_hashes[r_symndx - symtab_hdr->sh_info];
		    while (h->root.type == bfd_link_hash_indirect
			   || h->root.type == bfd_link_hash_warning)
		      h = (struct elf_link_hash_entry *) h->root.u.i.link;
		  }

		is_local = FALSE;
		if (h == NULL
		    || !(h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC))
		  is_local = TRUE;

		r_type = (enum elf_ppc_reloc_type) ELF32_R_TYPE (rel->r_info);
		switch (r_type)
		  {
		  case R_PPC_GOT_TLSLD16:
		  case R_PPC_GOT_TLSLD16_LO:
		  case R_PPC_GOT_TLSLD16_HI:
		  case R_PPC_GOT_TLSLD16_HA:
		    /* These relocs should never be against a symbol
		       defined in a shared lib.  Leave them alone if
		       that turns out to be the case.  */
		    expecting_tls_get_addr = 0;
		    htab->tlsld_got.refcount -= 1;
		    if (!is_local)
		      continue;

		    /* LD -> LE */
		    tls_set = 0;
		    tls_clear = TLS_LD;
		    expecting_tls_get_addr = 1;
		    break;

		  case R_PPC_GOT_TLSGD16:
		  case R_PPC_GOT_TLSGD16_LO:
		  case R_PPC_GOT_TLSGD16_HI:
		  case R_PPC_GOT_TLSGD16_HA:
		    if (is_local)
		      /* GD -> LE */
		      tls_set = 0;
		    else
		      /* GD -> IE */
		      tls_set = TLS_TLS | TLS_TPRELGD;
		    tls_clear = TLS_GD;
		    expecting_tls_get_addr = 1;
		    break;

		  case R_PPC_GOT_TPREL16:
		  case R_PPC_GOT_TPREL16_LO:
		  case R_PPC_GOT_TPREL16_HI:
		  case R_PPC_GOT_TPREL16_HA:
		    expecting_tls_get_addr = 0;
		    if (is_local)
		      {
			/* IE -> LE */
			tls_set = 0;
			tls_clear = TLS_TPREL;
			break;
		      }
		    else
		      continue;

		  case R_PPC_REL14:
		  case R_PPC_REL14_BRTAKEN:
		  case R_PPC_REL14_BRNTAKEN:
		  case R_PPC_REL24:
		    if (expecting_tls_get_addr
			&& h != NULL
			&& h == htab->tls_get_addr)
		      {
			if (h->plt.refcount > 0)
			  h->plt.refcount -= 1;
		      }
		    expecting_tls_get_addr = 0;
		    continue;

		  default:
		    expecting_tls_get_addr = 0;
		    continue;
		  }

		if (h != NULL)
		  {
		    if (tls_set == 0)
		      {
			/* We managed to get rid of a got entry.  */
			if (h->got.refcount > 0)
			  h->got.refcount -= 1;
		      }
		    tls_mask = &ppc_elf_hash_entry (h)->tls_mask;
		  }
		else
		  {
		    Elf_Internal_Sym *sym;
		    bfd_signed_vma *lgot_refs;
		    char *lgot_masks;

		    if (locsyms == NULL)
		      {
			locsyms = (Elf_Internal_Sym *) symtab_hdr->contents;
			if (locsyms == NULL)
			  locsyms = bfd_elf_get_elf_syms (ibfd, symtab_hdr,
							  symtab_hdr->sh_info,
							  0, NULL, NULL, NULL);
			if (locsyms == NULL)
			  {
			    if (elf_section_data (sec)->relocs != relstart)
			      free (relstart);
			    return FALSE;
			  }
		      }
		    sym = locsyms + r_symndx;
		    lgot_refs = elf_local_got_refcounts (ibfd);
		    if (lgot_refs == NULL)
		      abort ();
		    if (tls_set == 0)
		      {
			/* We managed to get rid of a got entry.  */
			if (lgot_refs[r_symndx] > 0)
			  lgot_refs[r_symndx] -= 1;
		      }
		    lgot_masks = (char *) (lgot_refs + symtab_hdr->sh_info);
		    tls_mask = &lgot_masks[r_symndx];
		  }

		*tls_mask |= tls_set;
		*tls_mask &= ~tls_clear;
	      }

	    if (elf_section_data (sec)->relocs != relstart)
	      free (relstart);
	  }

      if (locsyms != NULL
	  && (symtab_hdr->contents != (unsigned char *) locsyms))
	{
	  if (!info->keep_memory)
	    free (locsyms);
	  else
	    symtab_hdr->contents = (unsigned char *) locsyms;
	}
    }
  return TRUE;
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We use it to put .comm items in .sbss, and not .bss.  */

static bfd_boolean
ppc_elf_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     const Elf_Internal_Sym *sym;
     const char **namep ATTRIBUTE_UNUSED;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp;
     bfd_vma *valp;
{
  if (sym->st_shndx == SHN_COMMON
      && !info->relocateable
      && sym->st_size <= elf_gp_size (abfd)
      && info->hash->creator->flavour == bfd_target_elf_flavour)
    {
      /* Common symbols less than or equal to -G nn bytes are automatically
	 put into .sdata.  */
      elf_linker_section_t *sdata
	= ppc_elf_create_linker_section (abfd, info, LINKER_SECTION_SDATA);

      if (!sdata->bss_section)
	{
	  bfd_size_type amt;

	  /* We don't go through bfd_make_section, because we don't
	     want to attach this common section to DYNOBJ.  The linker
	     will move the symbols to the appropriate output section
	     when it defines common symbols.  */
	  amt = sizeof (asection);
	  sdata->bss_section = (asection *) bfd_zalloc (abfd, amt);
	  if (sdata->bss_section == NULL)
	    return FALSE;
	  sdata->bss_section->name = sdata->bss_name;
	  sdata->bss_section->flags = SEC_IS_COMMON;
	  sdata->bss_section->output_section = sdata->bss_section;
	  amt = sizeof (asymbol);
	  sdata->bss_section->symbol = (asymbol *) bfd_zalloc (abfd, amt);
	  amt = sizeof (asymbol *);
	  sdata->bss_section->symbol_ptr_ptr =
	    (asymbol **) bfd_zalloc (abfd, amt);
	  if (sdata->bss_section->symbol == NULL
	      || sdata->bss_section->symbol_ptr_ptr == NULL)
	    return FALSE;
	  sdata->bss_section->symbol->name = sdata->bss_name;
	  sdata->bss_section->symbol->flags = BSF_SECTION_SYM;
	  sdata->bss_section->symbol->section = sdata->bss_section;
	  *sdata->bss_section->symbol_ptr_ptr = sdata->bss_section->symbol;
	}

      *secp = sdata->bss_section;
      *valp = sym->st_size;
    }

  return TRUE;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static bfd_boolean
ppc_elf_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  struct ppc_elf_link_hash_table *htab;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_finish_dynamic_symbol called for %s",
	   h->root.root.string);
#endif

  htab = ppc_elf_hash_table (info);
  BFD_ASSERT (htab->elf.dynobj != NULL);

  if (h->plt.offset != (bfd_vma) -1)
    {
      Elf_Internal_Rela rela;
      bfd_byte *loc;
      bfd_vma reloc_index;

#ifdef DEBUG
      fprintf (stderr, ", plt_offset = %d", h->plt.offset);
#endif

      /* This symbol has an entry in the procedure linkage table.  Set
	 it up.  */

      BFD_ASSERT (h->dynindx != -1);
      BFD_ASSERT (htab->plt != NULL && htab->relplt != NULL);

      /* We don't need to fill in the .plt.  The ppc dynamic linker
	 will fill it in.  */

      /* Fill in the entry in the .rela.plt section.  */
      rela.r_offset = (htab->plt->output_section->vma
		       + htab->plt->output_offset
		       + h->plt.offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_PPC_JMP_SLOT);
      rela.r_addend = 0;

      reloc_index = (h->plt.offset - PLT_INITIAL_ENTRY_SIZE) / PLT_SLOT_SIZE;
      if (reloc_index > PLT_NUM_SINGLE_ENTRIES)
	reloc_index -= (reloc_index - PLT_NUM_SINGLE_ENTRIES) / 2;
      loc = (htab->relplt->contents
	     + reloc_index * sizeof (Elf32_External_Rela));
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;
	  /* If the symbol is weak, we do need to clear the value.
	     Otherwise, the PLT entry would provide a definition for
	     the symbol even if the symbol wasn't defined anywhere,
	     and so the symbol would never be NULL.  */
	  if ((h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR_NONWEAK)
	      == 0)
	    sym->st_value = 0;
	}
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
    {
      asection *s;
      Elf_Internal_Rela rela;
      bfd_byte *loc;

      /* This symbols needs a copy reloc.  Set it up.  */

#ifdef DEBUG
      fprintf (stderr, ", copy");
#endif

      BFD_ASSERT (h->dynindx != -1);

      if (h->size <= elf_gp_size (htab->elf.dynobj))
	s = htab->relsbss;
      else
	s = htab->relbss;
      BFD_ASSERT (s != NULL);

      rela.r_offset = (h->root.u.def.value
		       + h->root.u.def.section->output_section->vma
		       + h->root.u.def.section->output_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_PPC_COPY);
      rela.r_addend = 0;
      loc = s->contents + s->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
    }

#ifdef DEBUG
  fprintf (stderr, "\n");
#endif

  /* Mark some specially defined symbols as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0
      || strcmp (h->root.root.string, "_PROCEDURE_LINKAGE_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return TRUE;
}

/* Finish up the dynamic sections.  */

static bfd_boolean
ppc_elf_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  asection *sdyn;
  struct ppc_elf_link_hash_table *htab;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_finish_dynamic_sections called\n");
#endif

  htab = ppc_elf_hash_table (info);
  sdyn = bfd_get_section_by_name (htab->elf.dynobj, ".dynamic");

  if (htab->elf.dynamic_sections_created)
    {
      Elf32_External_Dyn *dyncon, *dynconend;

      BFD_ASSERT (htab->plt != NULL && sdyn != NULL);

      dyncon = (Elf32_External_Dyn *) sdyn->contents;
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  asection *s;

	  bfd_elf32_swap_dyn_in (htab->elf.dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    case DT_PLTGOT:
	      s = htab->plt;
	      dyn.d_un.d_ptr = s->output_section->vma + s->output_offset;
	      break;

	    case DT_PLTRELSZ:
	      dyn.d_un.d_val = htab->relplt->_raw_size;
	      break;

	    case DT_JMPREL:
	      s = htab->relplt;
	      dyn.d_un.d_ptr = s->output_section->vma + s->output_offset;
	      break;

	    default:
	      continue;
	    }

	  bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	}
    }

  /* Add a blrl instruction at _GLOBAL_OFFSET_TABLE_-4 so that a function can
     easily find the address of the _GLOBAL_OFFSET_TABLE_.  */
  if (htab->got)
    {
      unsigned char *contents = htab->got->contents;
      bfd_put_32 (output_bfd, (bfd_vma) 0x4e800021 /* blrl */, contents);

      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, contents + 4);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    contents + 4);

      elf_section_data (htab->got->output_section)->this_hdr.sh_entsize = 4;
    }

  return TRUE;
}

/* The RELOCATE_SECTION function is called by the ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjust the section contents as
   necessary, and (if using Rela relocs and generating a
   relocateable output file) adjusting the reloc addend as
   necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocateable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.  */

static bfd_boolean
ppc_elf_relocate_section (output_bfd, info, input_bfd, input_section,
			  contents, relocs, local_syms, local_sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  struct ppc_elf_link_hash_table *htab;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;
  Elf_Internal_Rela outrel;
  bfd_byte *loc;
  asection *sreloc = NULL;
  bfd_vma *local_got_offsets;
  bfd_boolean ret = TRUE;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_relocate_section called for %s section %s, %ld relocations%s\n",
	   bfd_archive_filename (input_bfd),
	   bfd_section_name(input_bfd, input_section),
	   (long) input_section->reloc_count,
	   (info->relocateable) ? " (relocatable)" : "");
#endif

  if (info->relocateable)
    return TRUE;

  if (!ppc_elf_howto_table[R_PPC_ADDR32])
    /* Initialize howto table if needed.  */
    ppc_elf_howto_init ();

  htab = ppc_elf_hash_table (info);
  local_got_offsets = elf_local_got_offsets (input_bfd);
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      enum elf_ppc_reloc_type r_type;
      bfd_vma addend;
      bfd_reloc_status_type r;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      const char *sym_name;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      bfd_vma relocation;
      bfd_vma branch_bit, insn, from;
      bfd_boolean unresolved_reloc;
      bfd_boolean warned;
      unsigned int tls_type, tls_mask, tls_gd;

      r_type = (enum elf_ppc_reloc_type)ELF32_R_TYPE (rel->r_info);
      sym = (Elf_Internal_Sym *) 0;
      sec = (asection *) 0;
      h = (struct elf_link_hash_entry *) 0;
      unresolved_reloc = FALSE;
      warned = FALSE;
      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  sym_name = bfd_elf_local_sym_name (input_bfd, sym);

	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, sec, rel);
	}
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	  sym_name = h->root.root.string;

	  relocation = 0;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      /* Set a flag that will be cleared later if we find a
		 relocation value for this symbol.  output_section
		 is typically NULL for symbols satisfied by a shared
		 library.  */
	      if (sec->output_section == NULL)
		unresolved_reloc = TRUE;
	      else
		relocation = (h->root.u.def.value
			      + sec->output_section->vma
			      + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    ;
	  else if (info->shared
		   && !info->no_undefined
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    ;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd, input_section,
		      rel->r_offset, (!info->shared
				      || info->no_undefined
				      || ELF_ST_VISIBILITY (h->other)))))
		return FALSE;
	      warned = TRUE;
	    }
	}

      /* TLS optimizations.  Replace instruction sequences and relocs
	 based on information we collected in tls_optimize.  We edit
	 RELOCS so that --emit-relocs will output something sensible
	 for the final instruction stream.  */
      tls_mask = 0;
      tls_gd = 0;
      if (IS_PPC_TLS_RELOC (r_type))
	{
	  if (h != NULL)
	    tls_mask = ((struct ppc_elf_link_hash_entry *) h)->tls_mask;
	  else if (local_got_offsets != NULL)
	    {
	      char *lgot_masks;
	      lgot_masks = (char *) (local_got_offsets + symtab_hdr->sh_info);
	      tls_mask = lgot_masks[r_symndx];
	    }
	}

      /* Ensure reloc mapping code below stays sane.  */
      if ((R_PPC_GOT_TLSLD16 & 3)    != (R_PPC_GOT_TLSGD16 & 3)
	  || (R_PPC_GOT_TLSLD16_LO & 3) != (R_PPC_GOT_TLSGD16_LO & 3)
	  || (R_PPC_GOT_TLSLD16_HI & 3) != (R_PPC_GOT_TLSGD16_HI & 3)
	  || (R_PPC_GOT_TLSLD16_HA & 3) != (R_PPC_GOT_TLSGD16_HA & 3)
	  || (R_PPC_GOT_TLSLD16 & 3)    != (R_PPC_GOT_TPREL16 & 3)
	  || (R_PPC_GOT_TLSLD16_LO & 3) != (R_PPC_GOT_TPREL16_LO & 3)
	  || (R_PPC_GOT_TLSLD16_HI & 3) != (R_PPC_GOT_TPREL16_HI & 3)
	  || (R_PPC_GOT_TLSLD16_HA & 3) != (R_PPC_GOT_TPREL16_HA & 3))
	abort ();
      switch (r_type)
	{
	default:
	  break;

	case R_PPC_GOT_TPREL16:
	case R_PPC_GOT_TPREL16_LO:
	  if (tls_mask != 0
	      && (tls_mask & TLS_TPREL) == 0)
	    {
	      bfd_vma insn;
	      insn = bfd_get_32 (output_bfd, contents + rel->r_offset - 2);
	      insn &= 31 << 21;
	      insn |= 0x3c020000;	/* addis 0,2,0 */
	      bfd_put_32 (output_bfd, insn, contents + rel->r_offset - 2);
	      r_type = R_PPC_TPREL16_HA;
	      rel->r_info = ELF32_R_INFO (r_symndx, r_type);
	    }
	  break;

	case R_PPC_TLS:
	  if (tls_mask != 0
	      && (tls_mask & TLS_TPREL) == 0)
	    {
	      bfd_vma insn, rtra;
	      insn = bfd_get_32 (output_bfd, contents + rel->r_offset);
	      if ((insn & ((31 << 26) | (31 << 11)))
		  == ((31 << 26) | (2 << 11)))
		rtra = insn & ((1 << 26) - (1 << 16));
	      else if ((insn & ((31 << 26) | (31 << 16)))
		       == ((31 << 26) | (2 << 16)))
		rtra = (insn & (31 << 21)) | ((insn & (31 << 11)) << 5);
	      else
		abort ();
	      if ((insn & ((1 << 11) - (1 << 1))) == 266 << 1)
		/* add -> addi.  */
		insn = 14 << 26;
	      else if ((insn & (31 << 1)) == 23 << 1
		       && ((insn & (31 << 6)) < 14 << 6
			   || ((insn & (31 << 6)) >= 16 << 6
			       && (insn & (31 << 6)) < 24 << 6)))
		/* load and store indexed -> dform.  */
		insn = (32 | ((insn >> 6) & 31)) << 26;
	      else if ((insn & (31 << 1)) == 21 << 1
		       && (insn & (0x1a << 6)) == 0)
		/* ldx, ldux, stdx, stdux -> ld, ldu, std, stdu.  */
		insn = (((58 | ((insn >> 6) & 4)) << 26)
			| ((insn >> 6) & 1));
	      else if ((insn & (31 << 1)) == 21 << 1
		       && (insn & ((1 << 11) - (1 << 1))) == 341 << 1)
		/* lwax -> lwa.  */
		insn = (58 << 26) | 2;
	      else
		abort ();
	      insn |= rtra;
	      bfd_put_32 (output_bfd, insn, contents + rel->r_offset);
	      r_type = R_PPC_TPREL16_LO;
	      rel->r_info = ELF32_R_INFO (r_symndx, r_type);
	      /* Was PPC_TLS which sits on insn boundary, now
		 PPC_TPREL16_LO which is at insn+2.  */
	      rel->r_offset += 2;
	    }
	  break;

	case R_PPC_GOT_TLSGD16_HI:
	case R_PPC_GOT_TLSGD16_HA:
	  tls_gd = TLS_TPRELGD;
	  if (tls_mask != 0 && (tls_mask & TLS_GD) == 0)
	    goto tls_gdld_hi;
	  break;

	case R_PPC_GOT_TLSLD16_HI:
	case R_PPC_GOT_TLSLD16_HA:
	  if (tls_mask != 0 && (tls_mask & TLS_LD) == 0)
	    {
	    tls_gdld_hi:
	      if ((tls_mask & tls_gd) != 0)
		r_type = (((r_type - (R_PPC_GOT_TLSGD16 & 3)) & 3)
			  + R_PPC_GOT_TPREL16);
	      else
		{
		  bfd_put_32 (output_bfd, NOP, contents + rel->r_offset);
		  rel->r_offset -= 2;
		  r_type = R_PPC_NONE;
		}
	      rel->r_info = ELF32_R_INFO (r_symndx, r_type);
	    }
	  break;

	case R_PPC_GOT_TLSGD16:
	case R_PPC_GOT_TLSGD16_LO:
	  tls_gd = TLS_TPRELGD;
	  if (tls_mask != 0 && (tls_mask & TLS_GD) == 0)
	    goto tls_get_addr_check;
	  break;

	case R_PPC_GOT_TLSLD16:
	case R_PPC_GOT_TLSLD16_LO:
	  if (tls_mask != 0 && (tls_mask & TLS_LD) == 0)
	    {
	    tls_get_addr_check:
	      if (rel + 1 < relend)
		{
		  enum elf_ppc_reloc_type r_type2;
		  unsigned long r_symndx2;
		  struct elf_link_hash_entry *h2;
		  bfd_vma insn1, insn2;
		  bfd_vma offset;

		  /* The next instruction should be a call to
		     __tls_get_addr.  Peek at the reloc to be sure.  */
		  r_type2
		    = (enum elf_ppc_reloc_type) ELF32_R_TYPE (rel[1].r_info);
		  r_symndx2 = ELF32_R_SYM (rel[1].r_info);
		  if (r_symndx2 < symtab_hdr->sh_info
		      || (r_type2 != R_PPC_REL14
			  && r_type2 != R_PPC_REL14_BRTAKEN
			  && r_type2 != R_PPC_REL14_BRNTAKEN
			  && r_type2 != R_PPC_REL24
			  && r_type2 != R_PPC_PLTREL24))
		    break;

		  h2 = sym_hashes[r_symndx2 - symtab_hdr->sh_info];
		  while (h2->root.type == bfd_link_hash_indirect
			 || h2->root.type == bfd_link_hash_warning)
		    h2 = (struct elf_link_hash_entry *) h2->root.u.i.link;
		  if (h2 == NULL || h2 != htab->tls_get_addr)
		    break;

		  /* OK, it checks out.  Replace the call.  */
		  offset = rel[1].r_offset;
		  insn1 = bfd_get_32 (output_bfd,
				      contents + rel->r_offset - 2);
		  if ((tls_mask & tls_gd) != 0)
		    {
		      /* IE */
		      insn1 &= (1 << 26) - 1;
		      insn1 |= 32 << 26;	/* lwz */
		      insn2 = 0x7c631214;	/* add 3,3,2 */
		      rel[1].r_info = ELF32_R_INFO (r_symndx2, R_PPC_NONE);
		      r_type = (((r_type - (R_PPC_GOT_TLSGD16 & 3)) & 3)
				+ R_PPC_GOT_TPREL16);
		      rel->r_info = ELF32_R_INFO (r_symndx, r_type);
		    }
		  else
		    {
		      /* LE */
		      insn1 = 0x3c620000;	/* addis 3,2,0 */
		      insn2 = 0x38630000;	/* addi 3,3,0 */
		      if (tls_gd == 0)
			{
			  /* Was an LD reloc.  */
			  r_symndx = 0;
			  rel->r_addend = htab->tls_sec->vma + DTP_OFFSET;
			  rel[1].r_addend = htab->tls_sec->vma + DTP_OFFSET;
			}
		      r_type = R_PPC_TPREL16_HA;
		      rel->r_info = ELF32_R_INFO (r_symndx, r_type);
		      rel[1].r_info = ELF32_R_INFO (r_symndx,
						    R_PPC_TPREL16_LO);
		      rel[1].r_offset += 2;
		    }
		  bfd_put_32 (output_bfd, insn1, contents + rel->r_offset - 2);
		  bfd_put_32 (output_bfd, insn2, contents + offset);
		  if (tls_gd == 0)
		    {
		      /* We changed the symbol on an LD reloc.  Start over
			 in order to get h, sym, sec etc. right.  */
		      rel--;
		      continue;
		    }
		}
	    }
	  break;
	}

      /* Handle other relocations that tweak non-addend part of insn.  */
      branch_bit = 0;
      switch (r_type)
	{
	default:
	  break;

	  /* Branch taken prediction relocations.  */
	case R_PPC_ADDR14_BRTAKEN:
	case R_PPC_REL14_BRTAKEN:
	  branch_bit = BRANCH_PREDICT_BIT;
	  /* Fall thru */

	  /* Branch not taken predicition relocations.  */
	case R_PPC_ADDR14_BRNTAKEN:
	case R_PPC_REL14_BRNTAKEN:
	  insn = bfd_get_32 (output_bfd, contents + rel->r_offset);
	  insn &= ~BRANCH_PREDICT_BIT;
	  insn |= branch_bit;

	  from = (rel->r_offset
		  + input_section->output_offset
		  + input_section->output_section->vma);

	  /* Invert 'y' bit if not the default.  */
	  if ((bfd_signed_vma) (relocation + rel->r_addend - from) < 0)
	    insn ^= BRANCH_PREDICT_BIT;

	  bfd_put_32 (output_bfd, insn, contents + rel->r_offset);
	  break;
	}

      addend = rel->r_addend;
      tls_type = 0;
      howto = NULL;
      if ((unsigned) r_type < (unsigned) R_PPC_max)
	howto = ppc_elf_howto_table[(int) r_type];
      switch (r_type)
	{
	default:
	  (*_bfd_error_handler)
	    (_("%s: unknown relocation type %d for symbol %s"),
	     bfd_archive_filename (input_bfd), (int) r_type, sym_name);

	  bfd_set_error (bfd_error_bad_value);
	  ret = FALSE;
	  continue;

	case R_PPC_NONE:
	case R_PPC_TLS:
	case R_PPC_EMB_MRKREF:
	case R_PPC_GNU_VTINHERIT:
	case R_PPC_GNU_VTENTRY:
	  continue;

	  /* GOT16 relocations.  Like an ADDR16 using the symbol's
	     address in the GOT as relocation value instead of the
	     symbol's value itself.  Also, create a GOT entry for the
	     symbol and put the symbol value there.  */
	case R_PPC_GOT_TLSGD16:
	case R_PPC_GOT_TLSGD16_LO:
	case R_PPC_GOT_TLSGD16_HI:
	case R_PPC_GOT_TLSGD16_HA:
	  tls_type = TLS_TLS | TLS_GD;
	  goto dogot;

	case R_PPC_GOT_TLSLD16:
	case R_PPC_GOT_TLSLD16_LO:
	case R_PPC_GOT_TLSLD16_HI:
	case R_PPC_GOT_TLSLD16_HA:
	  tls_type = TLS_TLS | TLS_LD;
	  goto dogot;

	case R_PPC_GOT_TPREL16:
	case R_PPC_GOT_TPREL16_LO:
	case R_PPC_GOT_TPREL16_HI:
	case R_PPC_GOT_TPREL16_HA:
	  tls_type = TLS_TLS | TLS_TPREL;
	  goto dogot;

	case R_PPC_GOT_DTPREL16:
	case R_PPC_GOT_DTPREL16_LO:
	case R_PPC_GOT_DTPREL16_HI:
	case R_PPC_GOT_DTPREL16_HA:
	  tls_type = TLS_TLS | TLS_DTPREL;
	  goto dogot;

	case R_PPC_GOT16:
	case R_PPC_GOT16_LO:
	case R_PPC_GOT16_HI:
	case R_PPC_GOT16_HA:
	dogot:
	  {
	    /* Relocation is to the entry for this symbol in the global
	       offset table.  */
	    bfd_vma off;
	    bfd_vma *offp;
	    unsigned long indx;

	    if (htab->got == NULL)
	      abort ();

	    indx = 0;
	    if (tls_type == (TLS_TLS | TLS_LD)
		&& (h == NULL
		    || !(h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC)))
	      offp = &htab->tlsld_got.offset;
	    else if (h != NULL)
	      {
		bfd_boolean dyn;
		dyn = htab->elf.dynamic_sections_created;
		if (! WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info->shared, h)
		    || (info->shared
			&& SYMBOL_REFERENCES_LOCAL (info, h)))
		  /* This is actually a static link, or it is a
		     -Bsymbolic link and the symbol is defined
		     locally, or the symbol was forced to be local
		     because of a version file.  */
		  ;
		else
		  {
		    indx = h->dynindx;
		    unresolved_reloc = FALSE;
		  }
		offp = &h->got.offset;
	      }
	    else
	      {
		if (local_got_offsets == NULL)
		  abort ();
		offp = &local_got_offsets[r_symndx];
	      }

	    /* The offset must always be a multiple of 4.  We use the
	       least significant bit to record whether we have already
	       processed this entry.  */
	    off = *offp;
	    if ((off & 1) != 0)
	      off &= ~1;
	    else
	      {
		unsigned int tls_m = (tls_mask
				      & (TLS_LD | TLS_GD | TLS_DTPREL
					 | TLS_TPREL | TLS_TPRELGD));

		if (offp == &htab->tlsld_got.offset)
		  tls_m = TLS_LD;
		else if (h == NULL
			 || !(h->elf_link_hash_flags
			      & ELF_LINK_HASH_DEF_DYNAMIC))
		  tls_m &= ~TLS_LD;

		/* We might have multiple got entries for this sym.
		   Initialize them all.  */
		do
		  {
		    int tls_ty = 0;

		    if ((tls_m & TLS_LD) != 0)
		      {
			tls_ty = TLS_TLS | TLS_LD;
			tls_m &= ~TLS_LD;
		      }
		    else if ((tls_m & TLS_GD) != 0)
		      {
			tls_ty = TLS_TLS | TLS_GD;
			tls_m &= ~TLS_GD;
		      }
		    else if ((tls_m & TLS_DTPREL) != 0)
		      {
			tls_ty = TLS_TLS | TLS_DTPREL;
			tls_m &= ~TLS_DTPREL;
		      }
		    else if ((tls_m & (TLS_TPREL | TLS_TPRELGD)) != 0)
		      {
			tls_ty = TLS_TLS | TLS_TPREL;
			tls_m = 0;
		      }

		    /* Generate relocs for the dynamic linker.  */
		    if (info->shared || indx != 0)
		      {
			outrel.r_offset = (htab->got->output_section->vma
					   + htab->got->output_offset
					   + off);
			outrel.r_addend = 0;
			if (tls_ty & (TLS_LD | TLS_GD))
			  {
			    outrel.r_info = ELF32_R_INFO (indx, R_PPC_DTPMOD32);
			    if (tls_ty == (TLS_TLS | TLS_GD))
			      {
				loc = htab->relgot->contents;
				loc += (htab->relgot->reloc_count++
					* sizeof (Elf32_External_Rela));
				bfd_elf32_swap_reloca_out (output_bfd,
							   &outrel, loc);
				outrel.r_offset += 4;
				outrel.r_info
				  = ELF32_R_INFO (indx, R_PPC_DTPREL32);
			      }
			  }
			else if (tls_ty == (TLS_TLS | TLS_DTPREL))
			  outrel.r_info = ELF32_R_INFO (indx, R_PPC_DTPREL32);
			else if (tls_ty == (TLS_TLS | TLS_TPREL))
			  outrel.r_info = ELF32_R_INFO (indx, R_PPC_TPREL32);
			else if (indx == 0)
			  outrel.r_info = ELF32_R_INFO (indx, R_PPC_RELATIVE);
			else
			  outrel.r_info = ELF32_R_INFO (indx, R_PPC_GLOB_DAT);
			if (indx == 0)
			  {
			    outrel.r_addend += relocation;
			    if (tls_ty & (TLS_GD | TLS_DTPREL | TLS_TPREL))
			      outrel.r_addend -= htab->tls_sec->vma;
			  }
			loc = htab->relgot->contents;
			loc += (htab->relgot->reloc_count++
				* sizeof (Elf32_External_Rela));
			bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);
		      }

		    /* Init the .got section contents if we're not
		       emitting a reloc.  */
		    else
		      {
			bfd_vma value = relocation;

			if (tls_ty == (TLS_TLS | TLS_LD))
			  value = 1;
			else if (tls_ty != 0)
			  {
			    value -= htab->tls_sec->vma + DTP_OFFSET;
			    if (tls_ty == (TLS_TLS | TLS_TPREL))
			      value += DTP_OFFSET - TP_OFFSET;

			    if (tls_ty == (TLS_TLS | TLS_GD))
			      {
				bfd_put_32 (output_bfd, value,
					    htab->got->contents + off + 4);
				value = 1;
			      }
			  }
			bfd_put_32 (output_bfd, value,
				    htab->got->contents + off);
		      }

		    off += 4;
		    if (tls_ty & (TLS_LD | TLS_GD))
		      off += 4;
		  }
		while (tls_m != 0);

		off = *offp;
		*offp = off | 1;
	      }

	    if (off >= (bfd_vma) -2)
	      abort ();

	    if ((tls_type & TLS_TLS) != 0)
	      {
		if (tls_type != (TLS_TLS | TLS_LD))
		  {
		    if ((tls_mask & TLS_LD) != 0
			&& !(h == NULL
			     || !(h->elf_link_hash_flags
				  & ELF_LINK_HASH_DEF_DYNAMIC)))
		      off += 8;
		    if (tls_type != (TLS_TLS | TLS_GD))
		      {
			if ((tls_mask & TLS_GD) != 0)
			  off += 8;
			if (tls_type != (TLS_TLS | TLS_DTPREL))
			  {
			    if ((tls_mask & TLS_DTPREL) != 0)
			      off += 4;
			  }
		      }
		  }
	      }

	    relocation = htab->got->output_offset + off - 4;

	    /* Addends on got relocations don't make much sense.
	       x+off@got is actually x@got+off, and since the got is
	       generated by a hash table traversal, the value in the
	       got at entry m+n bears little relation to the entry m.  */
	    if (addend != 0)
	      (*_bfd_error_handler)
		(_("%s(%s+0x%lx): non-zero addend on %s reloc against `%s'"),
		 bfd_archive_filename (input_bfd),
		 bfd_get_section_name (input_bfd, input_section),
		 (long) rel->r_offset,
		 howto->name,
		 sym_name);
	  }
	break;

	/* Relocations that need no special processing.  */
	case R_PPC_LOCAL24PC:
	  /* It makes no sense to point a local relocation
	     at a symbol not in this object.  */
	  if (unresolved_reloc)
	    {
	      if (! (*info->callbacks->undefined_symbol) (info,
							  h->root.root.string,
							  input_bfd,
							  input_section,
							  rel->r_offset,
							  TRUE))
		return FALSE;
	      continue;
	    }
	  break;

	case R_PPC_DTPREL16:
	case R_PPC_DTPREL16_LO:
	case R_PPC_DTPREL16_HI:
	case R_PPC_DTPREL16_HA:
	  addend -= htab->tls_sec->vma + DTP_OFFSET;
	  break;

	  /* Relocations that may need to be propagated if this is a shared
	     object.  */
	case R_PPC_TPREL16:
	case R_PPC_TPREL16_LO:
	case R_PPC_TPREL16_HI:
	case R_PPC_TPREL16_HA:
	  addend -= htab->tls_sec->vma + TP_OFFSET;
	  /* The TPREL16 relocs shouldn't really be used in shared
	     libs as they will result in DT_TEXTREL being set, but
	     support them anyway.  */
	  goto dodyn;

	case R_PPC_TPREL32:
	  addend -= htab->tls_sec->vma + TP_OFFSET;
	  goto dodyn;

	case R_PPC_DTPREL32:
	  addend -= htab->tls_sec->vma + DTP_OFFSET;
	  goto dodyn;

	case R_PPC_DTPMOD32:
	  relocation = 1;
	  addend = 0;
	  goto dodyn;

	case R_PPC_REL24:
	case R_PPC_REL32:
	case R_PPC_REL14:
	case R_PPC_REL14_BRTAKEN:
	case R_PPC_REL14_BRNTAKEN:
	  /* If these relocations are not to a named symbol, they can be
	     handled right here, no need to bother the dynamic linker.  */
	  if (h == NULL
	      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0
	      || SYMBOL_REFERENCES_LOCAL (info, h))
	    break;
	  /* fall through */

	  /* Relocations that always need to be propagated if this is a shared
	     object.  */
	case R_PPC_ADDR32:
	case R_PPC_ADDR24:
	case R_PPC_ADDR16:
	case R_PPC_ADDR16_LO:
	case R_PPC_ADDR16_HI:
	case R_PPC_ADDR16_HA:
	case R_PPC_ADDR14:
	case R_PPC_ADDR14_BRTAKEN:
	case R_PPC_ADDR14_BRNTAKEN:
	case R_PPC_UADDR32:
	case R_PPC_UADDR16:
	  /* r_symndx will be zero only for relocs against symbols
	     from removed linkonce sections, or sections discarded by
	     a linker script.  */
	dodyn:
	  if (r_symndx == 0)
	    break;
	  /* Fall thru.  */

	  if ((info->shared
	       && (MUST_BE_DYN_RELOC (r_type)
		   || (h != NULL
		       && h->dynindx != -1
		       && (!info->symbolic
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))))
	      || (ELIMINATE_COPY_RELOCS
		  && !info->shared
		  && (input_section->flags & SEC_ALLOC) != 0
		  && h != NULL
		  && h->dynindx != -1
		  && (h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0
		  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
		  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0))
	    {
	      int skip;

#ifdef DEBUG
	      fprintf (stderr, "ppc_elf_relocate_section need to create relocation for %s\n",
		       (h && h->root.root.string
			? h->root.root.string : "<unknown>"));
#endif

	      /* When generating a shared object, these relocations
		 are copied into the output file to be resolved at run
		 time.  */
	      if (sreloc == NULL)
		{
		  const char *name;

		  name = (bfd_elf_string_from_elf_section
			  (input_bfd,
			   elf_elfheader (input_bfd)->e_shstrndx,
			   elf_section_data (input_section)->rel_hdr.sh_name));
		  if (name == NULL)
		    return FALSE;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (input_bfd,
							       input_section),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (htab->elf.dynobj, name);
		  BFD_ASSERT (sreloc != NULL);
		}

	      skip = 0;

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info, input_section,
					 rel->r_offset);
	      if (outrel.r_offset == (bfd_vma) -1
		  || outrel.r_offset == (bfd_vma) -2)
		skip = (int) outrel.r_offset;
	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);

	      if (skip)
		memset (&outrel, 0, sizeof outrel);
	      else if (h != NULL
		       && !SYMBOL_REFERENCES_LOCAL (info, h))
		{
		  unresolved_reloc = FALSE;
		  outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
		  outrel.r_addend = rel->r_addend;
		}
	      else
		{
		  outrel.r_addend = relocation + rel->r_addend;

		  if (r_type == R_PPC_ADDR32)
		    outrel.r_info = ELF32_R_INFO (0, R_PPC_RELATIVE);
		  else
		    {
		      long indx;

		      if (bfd_is_abs_section (sec))
			indx = 0;
		      else if (sec == NULL || sec->owner == NULL)
			{
			  bfd_set_error (bfd_error_bad_value);
			  return FALSE;
			}
		      else
			{
			  asection *osec;

			  /* We are turning this relocation into one
			     against a section symbol.  It would be
			     proper to subtract the symbol's value,
			     osec->vma, from the emitted reloc addend,
			     but ld.so expects buggy relocs.  */
			  osec = sec->output_section;
			  indx = elf_section_data (osec)->dynindx;
			  BFD_ASSERT (indx > 0);
#ifdef DEBUG
			  if (indx <= 0)
			    {
			      printf ("indx=%d section=%s flags=%08x name=%s\n",
				      indx, osec->name, osec->flags,
				      h->root.root.string);
			    }
#endif
			}

		      outrel.r_info = ELF32_R_INFO (indx, r_type);
		    }
		}

	      loc = sreloc->contents;
	      loc += sreloc->reloc_count++ * sizeof (Elf32_External_Rela);
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);

	      if (skip == -1)
		continue;

	      /* This reloc will be computed at runtime.  We clear the memory
		 so that it contains predictable value.  */
	      if (! skip
		  && ((input_section->flags & SEC_ALLOC) != 0
		      || ELF32_R_TYPE (outrel.r_info) != R_PPC_RELATIVE))
		{
		  relocation = howto->pc_relative ? outrel.r_offset : 0;
		  addend = 0;
		  break;
		}
	    }
	  break;

	  /* Indirect .sdata relocation.  */
	case R_PPC_EMB_SDAI16:
	  BFD_ASSERT (htab->sdata != NULL);
	  relocation
	    = bfd_elf32_finish_pointer_linker_section (output_bfd, input_bfd,
						       info, htab->sdata, h,
						       relocation, rel,
						       R_PPC_RELATIVE);
	  break;

	  /* Indirect .sdata2 relocation.  */
	case R_PPC_EMB_SDA2I16:
	  BFD_ASSERT (htab->sdata2 != NULL);
	  relocation
	    = bfd_elf32_finish_pointer_linker_section (output_bfd, input_bfd,
						       info, htab->sdata2, h,
						       relocation, rel,
						       R_PPC_RELATIVE);
	  break;

	  /* Handle the TOC16 reloc.  We want to use the offset within the .got
	     section, not the actual VMA.  This is appropriate when generating
	     an embedded ELF object, for which the .got section acts like the
	     AIX .toc section.  */
	case R_PPC_TOC16:			/* phony GOT16 relocations */
	  BFD_ASSERT (sec != (asection *) 0);
	  BFD_ASSERT (bfd_is_und_section (sec)
		      || strcmp (bfd_get_section_name (abfd, sec), ".got") == 0
		      || strcmp (bfd_get_section_name (abfd, sec), ".cgot") == 0)

	    addend -= sec->output_section->vma + sec->output_offset + 0x8000;
	  break;

	case R_PPC_PLTREL24:
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */
	  BFD_ASSERT (h != NULL);

	  if (h->plt.offset == (bfd_vma) -1
	      || htab->plt == NULL)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
		 happens when statically linking PIC code, or when
		 using -Bsymbolic.  */
	      break;
	    }

	  unresolved_reloc = FALSE;
	  relocation = (htab->plt->output_section->vma
			+ htab->plt->output_offset
			+ h->plt.offset);
	  break;

	  /* Relocate against _SDA_BASE_.  */
	case R_PPC_SDAREL16:
	  {
	    const char *name;
	    const struct elf_link_hash_entry *sh;

	    BFD_ASSERT (sec != (asection *) 0);
	    name = bfd_get_section_name (abfd, sec->output_section);
	    if (! ((strncmp (name, ".sdata", 6) == 0
		    && (name[6] == 0 || name[6] == '.'))
		   || (strncmp (name, ".sbss", 5) == 0
		       && (name[5] == 0 || name[5] == '.'))))
	      {
		(*_bfd_error_handler) (_("%s: the target (%s) of a %s relocation is in the wrong output section (%s)"),
				       bfd_archive_filename (input_bfd),
				       sym_name,
				       howto->name,
				       name);
	      }
	    sh = htab->sdata->sym_hash;
	    addend -= (sh->root.u.def.value
		       + sh->root.u.def.section->output_section->vma
		       + sh->root.u.def.section->output_offset);
	  }
	  break;

	  /* Relocate against _SDA2_BASE_.  */
	case R_PPC_EMB_SDA2REL:
	  {
	    const char *name;
	    const struct elf_link_hash_entry *sh;

	    BFD_ASSERT (sec != (asection *) 0);
	    name = bfd_get_section_name (abfd, sec->output_section);
	    if (! (strncmp (name, ".sdata2", 7) == 0
		   || strncmp (name, ".sbss2", 6) == 0))
	      {
		(*_bfd_error_handler) (_("%s: the target (%s) of a %s relocation is in the wrong output section (%s)"),
				       bfd_archive_filename (input_bfd),
				       sym_name,
				       howto->name,
				       name);

		bfd_set_error (bfd_error_bad_value);
		ret = FALSE;
		continue;
	      }
	    sh = htab->sdata2->sym_hash;
	    addend -= (sh->root.u.def.value
		       + sh->root.u.def.section->output_section->vma
		       + sh->root.u.def.section->output_offset);
	  }
	  break;

	  /* Relocate against either _SDA_BASE_, _SDA2_BASE_, or 0.  */
	case R_PPC_EMB_SDA21:
	case R_PPC_EMB_RELSDA:
	  {
	    const char *name;
	    const struct elf_link_hash_entry *sh;
	    int reg;

	    BFD_ASSERT (sec != (asection *) 0);
	    name = bfd_get_section_name (abfd, sec->output_section);
	    if (((strncmp (name, ".sdata", 6) == 0
		  && (name[6] == 0 || name[6] == '.'))
		 || (strncmp (name, ".sbss", 5) == 0
		     && (name[5] == 0 || name[5] == '.'))))
	      {
		reg = 13;
		sh = htab->sdata->sym_hash;
		addend -= (sh->root.u.def.value
			   + sh->root.u.def.section->output_section->vma
			   + sh->root.u.def.section->output_offset);
	      }

	    else if (strncmp (name, ".sdata2", 7) == 0
		     || strncmp (name, ".sbss2", 6) == 0)
	      {
		reg = 2;
		sh = htab->sdata2->sym_hash;
		addend -= (sh->root.u.def.value
			   + sh->root.u.def.section->output_section->vma
			   + sh->root.u.def.section->output_offset);
	      }

	    else if (strcmp (name, ".PPC.EMB.sdata0") == 0
		     || strcmp (name, ".PPC.EMB.sbss0") == 0)
	      {
		reg = 0;
	      }

	    else
	      {
		(*_bfd_error_handler) (_("%s: the target (%s) of a %s relocation is in the wrong output section (%s)"),
				       bfd_archive_filename (input_bfd),
				       sym_name,
				       howto->name,
				       name);

		bfd_set_error (bfd_error_bad_value);
		ret = FALSE;
		continue;
	      }

	    if (r_type == R_PPC_EMB_SDA21)
	      {			/* fill in register field */
		insn = bfd_get_32 (output_bfd, contents + rel->r_offset);
		insn = (insn & ~RA_REGISTER_MASK) | (reg << RA_REGISTER_SHIFT);
		bfd_put_32 (output_bfd, insn, contents + rel->r_offset);
	      }
	  }
	  break;

	  /* Relocate against the beginning of the section.  */
	case R_PPC_SECTOFF:
	case R_PPC_SECTOFF_LO:
	case R_PPC_SECTOFF_HI:
	case R_PPC_SECTOFF_HA:
	  BFD_ASSERT (sec != (asection *) 0);
	  addend -= sec->output_section->vma;
	  break;

	  /* Negative relocations.  */
	case R_PPC_EMB_NADDR32:
	case R_PPC_EMB_NADDR16:
	case R_PPC_EMB_NADDR16_LO:
	case R_PPC_EMB_NADDR16_HI:
	case R_PPC_EMB_NADDR16_HA:
	  addend -= 2 * relocation;
	  break;

	case R_PPC_COPY:
	case R_PPC_GLOB_DAT:
	case R_PPC_JMP_SLOT:
	case R_PPC_RELATIVE:
	case R_PPC_PLT32:
	case R_PPC_PLTREL32:
	case R_PPC_PLT16_LO:
	case R_PPC_PLT16_HI:
	case R_PPC_PLT16_HA:
	case R_PPC_ADDR30:
	case R_PPC_EMB_RELSEC16:
	case R_PPC_EMB_RELST_LO:
	case R_PPC_EMB_RELST_HI:
	case R_PPC_EMB_RELST_HA:
	case R_PPC_EMB_BIT_FLD:
	  (*_bfd_error_handler)
	    (_("%s: relocation %s is not yet supported for symbol %s."),
	     bfd_archive_filename (input_bfd),
	     howto->name,
	     sym_name);

	  bfd_set_error (bfd_error_invalid_operation);
	  ret = FALSE;
	  continue;
	}

      /* Do any further special processing.  */
      switch (r_type)
	{
	default:
	  break;

	case R_PPC_ADDR16_HA:
	case R_PPC_GOT16_HA:
	case R_PPC_PLT16_HA:
	case R_PPC_SECTOFF_HA:
	case R_PPC_TPREL16_HA:
	case R_PPC_DTPREL16_HA:
	case R_PPC_GOT_TLSGD16_HA:
	case R_PPC_GOT_TLSLD16_HA:
	case R_PPC_GOT_TPREL16_HA:
	case R_PPC_GOT_DTPREL16_HA:
	case R_PPC_EMB_NADDR16_HA:
	case R_PPC_EMB_RELST_HA:
	  /* It's just possible that this symbol is a weak symbol
	     that's not actually defined anywhere.  In that case,
	     'sec' would be NULL, and we should leave the symbol
	     alone (it will be set to zero elsewhere in the link).  */
	  if (sec != NULL)
	    /* Add 0x10000 if sign bit in 0:15 is set.
	       Bits 0:15 are not used.  */
	    addend += 0x8000;
	  break;
	}

#ifdef DEBUG
      fprintf (stderr, "\ttype = %s (%d), name = %s, symbol index = %ld, offset = %ld, addend = %ld\n",
	       howto->name,
	       (int) r_type,
	       sym_name,
	       r_symndx,
	       (long) rel->r_offset,
	       (long) addend);
#endif

      if (unresolved_reloc
	  && !((input_section->flags & SEC_DEBUGGING) != 0
	       && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0))
	{
	  (*_bfd_error_handler)
	    (_("%s(%s+0x%lx): unresolvable %s relocation against symbol `%s'"),
	     bfd_archive_filename (input_bfd),
	     bfd_get_section_name (input_bfd, input_section),
	     (long) rel->r_offset,
	     howto->name,
	     sym_name);
	  ret = FALSE;
	}

      r = _bfd_final_link_relocate (howto,
				    input_bfd,
				    input_section,
				    contents,
				    rel->r_offset,
				    relocation,
				    addend);

      if (r != bfd_reloc_ok)
	{
	  if (sym_name == NULL)
	    sym_name = "(null)";
	  if (r == bfd_reloc_overflow)
	    {
	      if (warned)
		continue;
	      if (h != NULL
		  && h->root.type == bfd_link_hash_undefweak
		  && howto->pc_relative)
		{
		  /* Assume this is a call protected by other code that
		     detect the symbol is undefined.  If this is the case,
		     we can safely ignore the overflow.  If not, the
		     program is hosed anyway, and a little warning isn't
		     going to help.  */

		  continue;
		}

	      if (! (*info->callbacks->reloc_overflow) (info,
							sym_name,
							howto->name,
							rel->r_addend,
							input_bfd,
							input_section,
							rel->r_offset))
		return FALSE;
	    }
	  else
	    {
	      (*_bfd_error_handler)
		(_("%s(%s+0x%lx): %s reloc against `%s': error %d"),
		 bfd_archive_filename (input_bfd),
		 bfd_get_section_name (input_bfd, input_section),
		 (long) rel->r_offset, howto->name, sym_name, (int) r);
	      ret = FALSE;
	    }
	}
    }

#ifdef DEBUG
  fprintf (stderr, "\n");
#endif

  return ret;
}

static enum elf_reloc_type_class
ppc_elf_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  switch ((int) ELF32_R_TYPE (rela->r_info))
    {
    case R_PPC_RELATIVE:
      return reloc_class_relative;
    case R_PPC_REL24:
    case R_PPC_ADDR24:
    case R_PPC_JMP_SLOT:
      return reloc_class_plt;
    case R_PPC_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

/* Support for core dump NOTE sections.  */

static bfd_boolean
ppc_elf_grok_prstatus (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  int offset;
  unsigned int raw_size;

  switch (note->descsz)
    {
    default:
      return FALSE;

    case 268:		/* Linux/PPC.  */
      /* pr_cursig */
      elf_tdata (abfd)->core_signal = bfd_get_16 (abfd, note->descdata + 12);

      /* pr_pid */
      elf_tdata (abfd)->core_pid = bfd_get_32 (abfd, note->descdata + 24);

      /* pr_reg */
      offset = 72;
      raw_size = 192;

      break;
    }

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg",
					  raw_size, note->descpos + offset);
}

static bfd_boolean
ppc_elf_grok_psinfo (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  switch (note->descsz)
    {
    default:
      return FALSE;

    case 128:		/* Linux/PPC elf_prpsinfo.  */
      elf_tdata (abfd)->core_program
	= _bfd_elfcore_strndup (abfd, note->descdata + 32, 16);
      elf_tdata (abfd)->core_command
	= _bfd_elfcore_strndup (abfd, note->descdata + 48, 80);
    }

  /* Note that for some reason, a spurious space is tacked
     onto the end of the args in some (at least one anyway)
     implementations, so strip it off if it exists.  */

  {
    char *command = elf_tdata (abfd)->core_command;
    int n = strlen (command);

    if (0 < n && command[n - 1] == ' ')
      command[n - 1] = '\0';
  }

  return TRUE;
}

/* Very simple linked list structure for recording apuinfo values.  */
typedef struct apuinfo_list
{
  struct apuinfo_list *next;
  unsigned long value;
}
apuinfo_list;

static apuinfo_list * head;

static void apuinfo_list_init PARAMS ((void));
static void apuinfo_list_add PARAMS ((unsigned long));
static unsigned apuinfo_list_length PARAMS ((void));
static unsigned long apuinfo_list_element PARAMS ((unsigned long));
static void apuinfo_list_finish PARAMS ((void));

extern void ppc_elf_begin_write_processing
  PARAMS ((bfd *, struct bfd_link_info *));
extern void ppc_elf_final_write_processing
  PARAMS ((bfd *, bfd_boolean));
extern bfd_boolean ppc_elf_write_section
  PARAMS ((bfd *, asection *, bfd_byte *));


static void
apuinfo_list_init PARAMS ((void))
{
  head = NULL;
}

static void
apuinfo_list_add (value)
     unsigned long value;
{
  apuinfo_list *entry = head;

  while (entry != NULL)
    {
      if (entry->value == value)
	return;
      entry = entry->next;
    }

  entry = bfd_malloc (sizeof (* entry));
  if (entry == NULL)
    return;

  entry->value = value;
  entry->next  = head;
  head = entry;
}

static unsigned
apuinfo_list_length PARAMS ((void))
{
  apuinfo_list *entry;
  unsigned long count;

  for (entry = head, count = 0;
       entry;
       entry = entry->next)
    ++ count;

  return count;
}

static inline unsigned long
apuinfo_list_element (number)
     unsigned long number;
{
  apuinfo_list * entry;

  for (entry = head;
       entry && number --;
       entry = entry->next)
    ;

  return entry ? entry->value : 0;
}

static void
apuinfo_list_finish PARAMS ((void))
{
  apuinfo_list *entry;

  for (entry = head; entry;)
    {
      apuinfo_list *next = entry->next;
      free (entry);
      entry = next;
    }

  head = NULL;
}

#define APUINFO_SECTION_NAME	".PPC.EMB.apuinfo"
#define APUINFO_LABEL		"APUinfo"

/* Scan the input BFDs and create a linked list of
   the APUinfo values that will need to be emitted.  */

void
ppc_elf_begin_write_processing (abfd, link_info)
     bfd *abfd;
     struct bfd_link_info *link_info;
{
  bfd *ibfd;
  asection *asec;
  char *buffer;
  unsigned num_input_sections;
  bfd_size_type	output_section_size;
  unsigned i;
  unsigned num_entries;
  unsigned long	offset;
  unsigned long length;
  const char *error_message = NULL;

  if (link_info == NULL)
    return;

  /* Scan the input bfds, looking for apuinfo sections.  */
  num_input_sections = 0;
  output_section_size = 0;

  for (ibfd = link_info->input_bfds; ibfd; ibfd = ibfd->link_next)
    {
      asec = bfd_get_section_by_name (ibfd, APUINFO_SECTION_NAME);
      if (asec)
	{
	  ++ num_input_sections;
	  output_section_size += asec->_raw_size;
	}
    }

  /* We need at least one input sections
     in order to make merging worthwhile.  */
  if (num_input_sections < 1)
    return;

  /* Just make sure that the output section exists as well.  */
  asec = bfd_get_section_by_name (abfd, APUINFO_SECTION_NAME);
  if (asec == NULL)
    return;

  /* Allocate a buffer for the contents of the input sections.  */
  buffer = bfd_malloc (output_section_size);
  if (buffer == NULL)
    return;

  offset = 0;
  apuinfo_list_init ();

  /* Read in the input sections contents.  */
  for (ibfd = link_info->input_bfds; ibfd; ibfd = ibfd->link_next)
    {
      unsigned long datum;
      char *ptr;

      asec = bfd_get_section_by_name (ibfd, APUINFO_SECTION_NAME);
      if (asec == NULL)
	continue;

      length = asec->_raw_size;
      if (length < 24)
	{
	  error_message = _("corrupt or empty %s section in %s");
	  goto fail;
	}

      if (bfd_seek (ibfd, asec->filepos, SEEK_SET) != 0
	  || (bfd_bread (buffer + offset, length, ibfd) != length))
	{
	  error_message = _("unable to read in %s section from %s");
	  goto fail;
	}

      /* Process the contents of the section.  */
      ptr = buffer + offset;
      error_message = _("corrupt %s section in %s");

      /* Verify the contents of the header.  Note - we have to
	 extract the values this way in order to allow for a
	 host whose endian-ness is different from the target.  */
      datum = bfd_get_32 (ibfd, ptr);
      if (datum != sizeof APUINFO_LABEL)
	goto fail;

      datum = bfd_get_32 (ibfd, ptr + 8);
      if (datum != 0x2)
	goto fail;

      if (strcmp (ptr + 12, APUINFO_LABEL) != 0)
	goto fail;

      /* Get the number of apuinfo entries.  */
      datum = bfd_get_32 (ibfd, ptr + 4);
      if ((datum * 4 + 20) != length)
	goto fail;

      /* Make sure that we do not run off the end of the section.  */
      if (offset + length > output_section_size)
	goto fail;

      /* Scan the apuinfo section, building a list of apuinfo numbers.  */
      for (i = 0; i < datum; i++)
	apuinfo_list_add (bfd_get_32 (ibfd, ptr + 20 + (i * 4)));

      /* Update the offset.  */
      offset += length;
    }

  error_message = NULL;

  /* Compute the size of the output section.  */
  num_entries = apuinfo_list_length ();
  output_section_size = 20 + num_entries * 4;

  asec = bfd_get_section_by_name (abfd, APUINFO_SECTION_NAME);

  if (! bfd_set_section_size (abfd, asec, output_section_size))
    ibfd = abfd,
      error_message = _("warning: unable to set size of %s section in %s");

 fail:
  free (buffer);

  if (error_message)
    (*_bfd_error_handler) (error_message, APUINFO_SECTION_NAME,
			   bfd_archive_filename (ibfd));
}


/* Prevent the output section from accumulating the input sections'
   contents.  We have already stored this in our linked list structure.  */

bfd_boolean
ppc_elf_write_section (abfd, asec, contents)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *asec;
     bfd_byte *contents ATTRIBUTE_UNUSED;
{
  return (apuinfo_list_length ()
	  && strcmp (asec->name, APUINFO_SECTION_NAME) == 0);
}


/* Finally we can generate the output section.  */

void
ppc_elf_final_write_processing (abfd, linker)
     bfd *abfd;
     bfd_boolean linker ATTRIBUTE_UNUSED;
{
  bfd_byte *buffer;
  asection *asec;
  unsigned i;
  unsigned num_entries;
  bfd_size_type length;

  asec = bfd_get_section_by_name (abfd, APUINFO_SECTION_NAME);
  if (asec == NULL)
    return;

  if (apuinfo_list_length () == 0)
    return;

  length = asec->_raw_size;
  if (length < 20)
    return;

  buffer = bfd_malloc (length);
  if (buffer == NULL)
    {
      (*_bfd_error_handler)
	(_("failed to allocate space for new APUinfo section."));
      return;
    }

  /* Create the apuinfo header.  */
  num_entries = apuinfo_list_length ();
  bfd_put_32 (abfd, sizeof APUINFO_LABEL, buffer);
  bfd_put_32 (abfd, num_entries, buffer + 4);
  bfd_put_32 (abfd, 0x2, buffer + 8);
  strcpy (buffer + 12, APUINFO_LABEL);

  length = 20;
  for (i = 0; i < num_entries; i++)
    {
      bfd_put_32 (abfd, apuinfo_list_element (i), buffer + length);
      length += 4;
    }

  if (length != asec->_raw_size)
    (*_bfd_error_handler) (_("failed to compute new APUinfo section."));

  if (! bfd_set_section_contents (abfd, asec, buffer, (file_ptr) 0, length))
    (*_bfd_error_handler) (_("failed to install new APUinfo section."));

  free (buffer);

  apuinfo_list_finish ();
}

#define TARGET_LITTLE_SYM	bfd_elf32_powerpcle_vec
#define TARGET_LITTLE_NAME	"elf32-powerpcle"
#define TARGET_BIG_SYM		bfd_elf32_powerpc_vec
#define TARGET_BIG_NAME		"elf32-powerpc"
#define ELF_ARCH		bfd_arch_powerpc
#define ELF_MACHINE_CODE	EM_PPC
#define ELF_MAXPAGESIZE		0x10000
#define elf_info_to_howto	ppc_elf_info_to_howto

#ifdef  EM_CYGNUS_POWERPC
#define ELF_MACHINE_ALT1	EM_CYGNUS_POWERPC
#endif

#ifdef EM_PPC_OLD
#define ELF_MACHINE_ALT2	EM_PPC_OLD
#endif

#define elf_backend_plt_not_loaded	1
#define elf_backend_got_symbol_offset	4
#define elf_backend_can_gc_sections	1
#define elf_backend_can_refcount	1
#define elf_backend_got_header_size	12
#define elf_backend_plt_header_size	PLT_INITIAL_ENTRY_SIZE
#define elf_backend_rela_normal		1

#define bfd_elf32_bfd_merge_private_bfd_data	ppc_elf_merge_private_bfd_data
#define bfd_elf32_bfd_relax_section		ppc_elf_relax_section
#define bfd_elf32_bfd_reloc_type_lookup		ppc_elf_reloc_type_lookup
#define bfd_elf32_bfd_set_private_flags		ppc_elf_set_private_flags
#define bfd_elf32_bfd_link_hash_table_create	ppc_elf_link_hash_table_create

#define elf_backend_object_p			ppc_elf_object_p
#define elf_backend_gc_mark_hook		ppc_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook		ppc_elf_gc_sweep_hook
#define elf_backend_section_from_shdr		ppc_elf_section_from_shdr
#define elf_backend_relocate_section		ppc_elf_relocate_section
#define elf_backend_create_dynamic_sections	ppc_elf_create_dynamic_sections
#define elf_backend_check_relocs		ppc_elf_check_relocs
#define elf_backend_copy_indirect_symbol	ppc_elf_copy_indirect_symbol
#define elf_backend_adjust_dynamic_symbol	ppc_elf_adjust_dynamic_symbol
#define elf_backend_add_symbol_hook		ppc_elf_add_symbol_hook
#define elf_backend_size_dynamic_sections	ppc_elf_size_dynamic_sections
#define elf_backend_finish_dynamic_symbol	ppc_elf_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections	ppc_elf_finish_dynamic_sections
#define elf_backend_fake_sections		ppc_elf_fake_sections
#define elf_backend_additional_program_headers	ppc_elf_additional_program_headers
#define elf_backend_modify_segment_map		ppc_elf_modify_segment_map
#define elf_backend_grok_prstatus		ppc_elf_grok_prstatus
#define elf_backend_grok_psinfo			ppc_elf_grok_psinfo
#define elf_backend_reloc_type_class		ppc_elf_reloc_type_class
#define elf_backend_begin_write_processing	ppc_elf_begin_write_processing
#define elf_backend_final_write_processing	ppc_elf_final_write_processing
#define elf_backend_write_section		ppc_elf_write_section

#include "elf32-target.h"
