/* M32R-specific support for 32-bit ELF.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

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
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/m32r.h"

static bfd_reloc_status_type m32r_elf_10_pcrel_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type m32r_elf_do_10_pcrel_reloc
  PARAMS ((bfd *, reloc_howto_type *, asection *,
	   bfd_byte *, bfd_vma, asection *, bfd_vma, bfd_vma));
static bfd_reloc_status_type m32r_elf_hi16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static void m32r_elf_relocate_hi16
  PARAMS ((bfd *, int, Elf_Internal_Rela *, Elf_Internal_Rela *,
	   bfd_byte *, bfd_vma));
bfd_reloc_status_type m32r_elf_lo16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
bfd_reloc_status_type m32r_elf_generic_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type m32r_elf_sda16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void m32r_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
bfd_boolean _bfd_m32r_elf_section_from_bfd_section
  PARAMS ((bfd *, asection *, int *));
void _bfd_m32r_elf_symbol_processing
  PARAMS ((bfd *, asymbol *));
static bfd_boolean m32r_elf_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));
static bfd_boolean m32r_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
#if 0 /* not yet */
static bfd_boolean m32r_elf_relax_delete_bytes
  PARAMS ((bfd *, asection *, bfd_vma, int));
#endif
static bfd_reloc_status_type m32r_elf_final_sda_base
  PARAMS ((bfd *, struct bfd_link_info *, const char **, bfd_vma *));
static bfd_boolean m32r_elf_object_p
  PARAMS ((bfd *));
static void m32r_elf_final_write_processing
  PARAMS ((bfd *, bfd_boolean));
static bfd_boolean m32r_elf_set_private_flags
  PARAMS ((bfd *, flagword));
static bfd_boolean m32r_elf_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static bfd_boolean m32r_elf_print_private_bfd_data
  PARAMS ((bfd *, PTR));
static bfd_boolean m32r_elf_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_boolean m32r_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

asection * m32r_elf_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));

#define NOP_INSN		0x7000
#define MAKE_PARALLEL(insn)	((insn) | 0x8000)

/* Use REL instead of RELA to save space.
   This only saves space in libraries and object files, but perhaps
   relocs will be put in ROM?  All in all though, REL relocs are a pain
   to work with.  */
#define USE_REL	1

#ifndef USE_REL
#define USE_REL	0
#endif

static reloc_howto_type m32r_elf_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_M32R_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_M32R_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 m32r_elf_generic_reloc,/* special_function */
	 "R_M32R_16",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_M32R_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 m32r_elf_generic_reloc,/* special_function */
	 "R_M32R_32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 24 bit address.  */
  HOWTO (R_M32R_24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 m32r_elf_generic_reloc,/* special_function */
	 "R_M32R_24",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An PC Relative 10-bit relocation, shifted by 2.
     This reloc is complicated because relocations are relative to pc & -4.
     i.e. branches in the right insn slot use the address of the left insn
     slot for pc.  */
  /* ??? It's not clear whether this should have partial_inplace set or not.
     Branch relaxing in the assembler can store the addend in the insn,
     and if bfd_install_relocation gets called the addend may get added
     again.  */
  HOWTO (R_M32R_10_PCREL,	/* type */
	 2,	                /* rightshift */
	 1,	                /* size (0 = byte, 1 = short, 2 = long) */
	 10,	                /* bitsize */
	 TRUE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 m32r_elf_10_pcrel_reloc, /* special_function */
	 "R_M32R_10_PCREL",	/* name */
	 FALSE,	                /* partial_inplace */
	 0xff,		        /* src_mask */
	 0xff,   		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A relative 18 bit relocation, right shifted by 2.  */
  HOWTO (R_M32R_18_PCREL,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_18_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A relative 26 bit relocation, right shifted by 2.  */
  /* ??? It's not clear whether this should have partial_inplace set or not.
     Branch relaxing in the assembler can store the addend in the insn,
     and if bfd_install_relocation gets called the addend may get added
     again.  */
  HOWTO (R_M32R_26_PCREL,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_26_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* High 16 bits of address when lower 16 is or'd in.  */
  HOWTO (R_M32R_HI16_ULO,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 m32r_elf_hi16_reloc,	/* special_function */
	 "R_M32R_HI16_ULO",	/* name */
	 TRUE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* High 16 bits of address when lower 16 is added in.  */
  HOWTO (R_M32R_HI16_SLO,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 m32r_elf_hi16_reloc,	/* special_function */
	 "R_M32R_HI16_SLO",	/* name */
	 TRUE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Lower 16 bits of address.  */
  HOWTO (R_M32R_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 m32r_elf_lo16_reloc,	/* special_function */
	 "R_M32R_LO16",		/* name */
	 TRUE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Small data area 16 bits offset.  */
  HOWTO (R_M32R_SDA16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 m32r_elf_sda16_reloc,	/* special_function */
	 "R_M32R_SDA16",	/* name */
	 TRUE,			/* partial_inplace */  /* FIXME: correct? */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_M32R_GNU_VTINHERIT, /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_M32R_GNU_VTINHERIT", /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_M32R_GNU_VTENTRY,     /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_M32R_GNU_VTENTRY",   /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

};

/* Handle the R_M32R_10_PCREL reloc.  */

static bfd_reloc_status_type
m32r_elf_10_pcrel_reloc (abfd, reloc_entry, symbol, data,
			 input_section, output_bfd, error_message)
     bfd * abfd;
     arelent * reloc_entry;
     asymbol * symbol;
     PTR data;
     asection * input_section;
     bfd * output_bfd;
     char ** error_message ATTRIBUTE_UNUSED;
{
  /* This part is from bfd_elf_generic_reloc.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != NULL)
    {
      /* FIXME: See bfd_perform_relocation.  Is this right?  */
      return bfd_reloc_continue;
    }

  return m32r_elf_do_10_pcrel_reloc (abfd, reloc_entry->howto,
				     input_section,
				     data, reloc_entry->address,
				     symbol->section,
				     (symbol->value
				      + symbol->section->output_section->vma
				      + symbol->section->output_offset),
				     reloc_entry->addend);
}

/* Utility to actually perform an R_M32R_10_PCREL reloc.  */

static bfd_reloc_status_type
m32r_elf_do_10_pcrel_reloc (abfd, howto, input_section, data, offset,
			    symbol_section, symbol_value, addend)
     bfd *abfd;
     reloc_howto_type *howto;
     asection *input_section;
     bfd_byte *data;
     bfd_vma offset;
     asection *symbol_section ATTRIBUTE_UNUSED;
     bfd_vma symbol_value;
     bfd_vma addend;
{
  bfd_signed_vma relocation;
  unsigned long x;
  bfd_reloc_status_type status;

  /* Sanity check the address (offset in section).  */
  if (offset > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  relocation = symbol_value + addend;
  /* Make it pc relative.  */
  relocation -=	(input_section->output_section->vma
		 + input_section->output_offset);
  /* These jumps mask off the lower two bits of the current address
     before doing pcrel calculations.  */
  relocation -= (offset & -(bfd_vma) 4);

  if (relocation < -0x200 || relocation > 0x1ff)
    status = bfd_reloc_overflow;
  else
    status = bfd_reloc_ok;

  x = bfd_get_16 (abfd, data + offset);
  relocation >>= howto->rightshift;
  relocation <<= howto->bitpos;
  x = (x & ~howto->dst_mask) | (((x & howto->src_mask) + relocation) & howto->dst_mask);
  bfd_put_16 (abfd, (bfd_vma) x, data + offset);

  return status;
}

/* Handle the R_M32R_HI16_[SU]LO relocs.
   HI16_SLO is for the add3 and load/store with displacement instructions.
   HI16_ULO is for the or3 instruction.
   For R_M32R_HI16_SLO, the lower 16 bits are sign extended when added to
   the high 16 bytes so if the lower 16 bits are negative (bit 15 == 1) then
   we must add one to the high 16 bytes (which will get subtracted off when
   the low 16 bits are added).
   These relocs have to be done in combination with an R_M32R_LO16 reloc
   because there is a carry from the LO16 to the HI16.  Here we just save
   the information we need; we do the actual relocation when we see the LO16.
   This code is copied from the elf32-mips.c.  We also support an arbitrary
   number of HI16 relocs to be associated with a single LO16 reloc.  The
   assembler sorts the relocs to ensure each HI16 immediately precedes its
   LO16.  However if there are multiple copies, the assembler may not find
   the real LO16 so it picks the first one it finds.  */

struct m32r_hi16
{
  struct m32r_hi16 *next;
  bfd_byte *addr;
  bfd_vma addend;
};

/* FIXME: This should not be a static variable.  */

static struct m32r_hi16 *m32r_hi16_list;

static bfd_reloc_status_type
m32r_elf_hi16_reloc (abfd, reloc_entry, symbol, data,
		     input_section, output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_reloc_status_type ret;
  bfd_vma relocation;
  struct m32r_hi16 *n;

  /* This part is from bfd_elf_generic_reloc.
     If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* Sanity check the address (offset in section).  */
  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  ret = bfd_reloc_ok;
  if (bfd_is_und_section (symbol->section)
      && output_bfd == (bfd *) NULL)
    ret = bfd_reloc_undefined;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  /* Save the information, and let LO16 do the actual relocation.  */
  n = (struct m32r_hi16 *) bfd_malloc ((bfd_size_type) sizeof *n);
  if (n == NULL)
    return bfd_reloc_outofrange;
  n->addr = (bfd_byte *) data + reloc_entry->address;
  n->addend = relocation;
  n->next = m32r_hi16_list;
  m32r_hi16_list = n;

  if (output_bfd != (bfd *) NULL)
    reloc_entry->address += input_section->output_offset;

  return ret;
}

/* Handle an M32R ELF HI16 reloc.  */

static void
m32r_elf_relocate_hi16 (input_bfd, type, relhi, rello, contents, addend)
     bfd *input_bfd;
     int type;
     Elf_Internal_Rela *relhi;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma addend;
{
  unsigned long insn;
  bfd_vma addlo;

  insn = bfd_get_32 (input_bfd, contents + relhi->r_offset);

  addlo = bfd_get_32 (input_bfd, contents + rello->r_offset);
  if (type == R_M32R_HI16_SLO)
    addlo = ((addlo & 0xffff) ^ 0x8000) - 0x8000;
  else
    addlo &= 0xffff;

  addend += ((insn & 0xffff) << 16) + addlo;

  /* Reaccount for sign extension of low part.  */
  if (type == R_M32R_HI16_SLO
      && (addend & 0x8000) != 0)
    addend += 0x10000;

  bfd_put_32 (input_bfd,
	      (insn & 0xffff0000) | ((addend >> 16) & 0xffff),
	      contents + relhi->r_offset);
}

/* Do an R_M32R_LO16 relocation.  This is a straightforward 16 bit
   inplace relocation; this function exists in order to do the
   R_M32R_HI16_[SU]LO relocation described above.  */

bfd_reloc_status_type
m32r_elf_lo16_reloc (input_bfd, reloc_entry, symbol, data,
		     input_section, output_bfd, error_message)
     bfd *input_bfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  /* This part is from bfd_elf_generic_reloc.
     If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (m32r_hi16_list != NULL)
    {
      struct m32r_hi16 *l;

      l = m32r_hi16_list;
      while (l != NULL)
	{
	  unsigned long insn;
	  unsigned long val;
	  unsigned long vallo;
	  struct m32r_hi16 *next;

	  /* Do the HI16 relocation.  Note that we actually don't need
	     to know anything about the LO16 itself, except where to
	     find the low 16 bits of the addend needed by the LO16.  */
	  insn = bfd_get_32 (input_bfd, l->addr);
	  vallo = ((bfd_get_32 (input_bfd, (bfd_byte *) data + reloc_entry->address)
		   & 0xffff) ^ 0x8000) - 0x8000;
	  val = ((insn & 0xffff) << 16) + vallo;
	  val += l->addend;

	  /* Reaccount for sign extension of low part.  */
	  if ((val & 0x8000) != 0)
	    val += 0x10000;

	  insn = (insn &~ (bfd_vma) 0xffff) | ((val >> 16) & 0xffff);
	  bfd_put_32 (input_bfd, (bfd_vma) insn, l->addr);

	  next = l->next;
	  free (l);
	  l = next;
	}

      m32r_hi16_list = NULL;
    }

  /* Now do the LO16 reloc in the usual way.
     ??? It would be nice to call bfd_elf_generic_reloc here,
     but we have partial_inplace set.  bfd_elf_generic_reloc will
     pass the handling back to bfd_install_relocation which will install
     a section relative addend which is wrong.  */
  return m32r_elf_generic_reloc (input_bfd, reloc_entry, symbol, data,
				input_section, output_bfd, error_message);
}

/* Do generic partial_inplace relocation.
   This is a local replacement for bfd_elf_generic_reloc.  */

bfd_reloc_status_type
m32r_elf_generic_reloc (input_bfd, reloc_entry, symbol, data,
		     input_section, output_bfd, error_message)
     bfd *input_bfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_reloc_status_type ret;
  bfd_vma relocation;
  bfd_byte *inplace_address;

  /* This part is from bfd_elf_generic_reloc.
     If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* Now do the reloc in the usual way.
     ??? It would be nice to call bfd_elf_generic_reloc here,
     but we have partial_inplace set.  bfd_elf_generic_reloc will
     pass the handling back to bfd_install_relocation which will install
     a section relative addend which is wrong.  */

  /* Sanity check the address (offset in section).  */
  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  ret = bfd_reloc_ok;
  if (bfd_is_und_section (symbol->section)
      && output_bfd == (bfd *) NULL)
    ret = bfd_reloc_undefined;

  if (bfd_is_com_section (symbol->section)
      || output_bfd != (bfd *) NULL)
    relocation = 0;
  else
    relocation = symbol->value;

  /* Only do this for a final link.  */
  if (output_bfd == (bfd *) NULL)
    {
      relocation += symbol->section->output_section->vma;
      relocation += symbol->section->output_offset;
    }

  relocation += reloc_entry->addend;
  inplace_address = (bfd_byte *) data + reloc_entry->address;

#define DOIT(x) 					\
  x = ( (x & ~reloc_entry->howto->dst_mask) | 		\
  (((x & reloc_entry->howto->src_mask) +  relocation) &	\
  reloc_entry->howto->dst_mask))

  switch (reloc_entry->howto->size)
    {
    case 1:
      {
	short x = bfd_get_16 (input_bfd, inplace_address);
	DOIT (x);
      	bfd_put_16 (input_bfd, (bfd_vma) x, inplace_address);
      }
      break;
    case 2:
      {
	unsigned long x = bfd_get_32 (input_bfd, inplace_address);
	DOIT (x);
      	bfd_put_32 (input_bfd, (bfd_vma)x , inplace_address);
      }
      break;
    default:
      BFD_ASSERT (0);
    }

  if (output_bfd != (bfd *) NULL)
    reloc_entry->address += input_section->output_offset;

  return ret;
}

/* Handle the R_M32R_SDA16 reloc.
   This reloc is used to compute the address of objects in the small data area
   and to perform loads and stores from that area.
   The lower 16 bits are sign extended and added to the register specified
   in the instruction, which is assumed to point to _SDA_BASE_.  */

static bfd_reloc_status_type
m32r_elf_sda16_reloc (abfd, reloc_entry, symbol, data,
		      input_section, output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* This part is from bfd_elf_generic_reloc.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != NULL)
    {
      /* FIXME: See bfd_perform_relocation.  Is this right?  */
      return bfd_reloc_continue;
    }

  /* FIXME: not sure what to do here yet.  But then again, the linker
     may never call us.  */
  abort ();
}

/* Map BFD reloc types to M32R ELF reloc types.  */

struct m32r_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct m32r_reloc_map m32r_reloc_map[] =
{
  { BFD_RELOC_NONE, R_M32R_NONE },
  { BFD_RELOC_16, R_M32R_16 },
  { BFD_RELOC_32, R_M32R_32 },
  { BFD_RELOC_M32R_24, R_M32R_24 },
  { BFD_RELOC_M32R_10_PCREL, R_M32R_10_PCREL },
  { BFD_RELOC_M32R_18_PCREL, R_M32R_18_PCREL },
  { BFD_RELOC_M32R_26_PCREL, R_M32R_26_PCREL },
  { BFD_RELOC_M32R_HI16_ULO, R_M32R_HI16_ULO },
  { BFD_RELOC_M32R_HI16_SLO, R_M32R_HI16_SLO },
  { BFD_RELOC_M32R_LO16, R_M32R_LO16 },
  { BFD_RELOC_M32R_SDA16, R_M32R_SDA16 },
  { BFD_RELOC_VTABLE_INHERIT, R_M32R_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, R_M32R_GNU_VTENTRY },
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0;
       i < sizeof (m32r_reloc_map) / sizeof (struct m32r_reloc_map);
       i++)
    {
      if (m32r_reloc_map[i].bfd_reloc_val == code)
	return &m32r_elf_howto_table[m32r_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Set the howto pointer for an M32R ELF reloc.  */

static void
m32r_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_M32R_max);
  cache_ptr->howto = &m32r_elf_howto_table[r_type];
}

/* Given a BFD section, try to locate the corresponding ELF section
   index.  */

bfd_boolean
_bfd_m32r_elf_section_from_bfd_section (abfd, sec, retval)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     int *retval;
{
  if (strcmp (bfd_get_section_name (abfd, sec), ".scommon") == 0)
    {
      *retval = SHN_M32R_SCOMMON;
      return TRUE;
    }
  return FALSE;
}

/* M32R ELF uses two common sections.  One is the usual one, and the other
   is for small objects.  All the small objects are kept together, and then
   referenced via one register, which yields faster assembler code.  It is
   up to the compiler to emit an instruction to load the register with
   _SDA_BASE.  This is what we use for the small common section.  This
   approach is copied from elf32-mips.c.  */
static asection m32r_elf_scom_section;
static asymbol m32r_elf_scom_symbol;
static asymbol *m32r_elf_scom_symbol_ptr;

/* Handle the special M32R section numbers that a symbol may use.  */

void
_bfd_m32r_elf_symbol_processing (abfd, asym)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol *asym;
{
  elf_symbol_type *elfsym;

  elfsym = (elf_symbol_type *) asym;

  switch (elfsym->internal_elf_sym.st_shndx)
    {
    case SHN_M32R_SCOMMON:
      if (m32r_elf_scom_section.name == NULL)
	{
	  /* Initialize the small common section.  */
	  m32r_elf_scom_section.name = ".scommon";
	  m32r_elf_scom_section.flags = SEC_IS_COMMON;
	  m32r_elf_scom_section.output_section = &m32r_elf_scom_section;
	  m32r_elf_scom_section.symbol = &m32r_elf_scom_symbol;
	  m32r_elf_scom_section.symbol_ptr_ptr = &m32r_elf_scom_symbol_ptr;
	  m32r_elf_scom_symbol.name = ".scommon";
	  m32r_elf_scom_symbol.flags = BSF_SECTION_SYM;
	  m32r_elf_scom_symbol.section = &m32r_elf_scom_section;
	  m32r_elf_scom_symbol_ptr = &m32r_elf_scom_symbol;
	}
      asym->section = &m32r_elf_scom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;
    }
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We must handle the special M32R section numbers here.
   We also keep watching for whether we need to create the sdata special
   linker sections.  */

static bfd_boolean
m32r_elf_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     const Elf_Internal_Sym *sym;
     const char **namep;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp;
     bfd_vma *valp;
{
  if (! info->relocateable
      && (*namep)[0] == '_' && (*namep)[1] == 'S'
      && strcmp (*namep, "_SDA_BASE_") == 0
      && info->hash->creator->flavour == bfd_target_elf_flavour)
    {
      /* This is simpler than using _bfd_elf_create_linker_section
	 (our needs are simpler than ppc's needs).  Also
	 _bfd_elf_create_linker_section currently has a bug where if a .sdata
	 section already exists a new one is created that follows it which
	 screws of _SDA_BASE_ address calcs because output_offset != 0.  */
      struct elf_link_hash_entry *h;
      struct bfd_link_hash_entry *bh;
      asection *s = bfd_get_section_by_name (abfd, ".sdata");

      /* The following code was cobbled from elf32-ppc.c and elflink.c.  */

      if (s == NULL)
	{
	  flagword flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
			    | SEC_IN_MEMORY | SEC_LINKER_CREATED);

	  s = bfd_make_section_anyway (abfd, ".sdata");
	  if (s == NULL)
	    return FALSE;
	  bfd_set_section_flags (abfd, s, flags);
	  bfd_set_section_alignment (abfd, s, 2);
	}

      bh = bfd_link_hash_lookup (info->hash, "_SDA_BASE_",
				 FALSE, FALSE, FALSE);

      if ((bh == NULL || bh->type == bfd_link_hash_undefined)
	  && !(_bfd_generic_link_add_one_symbol (info,
						 abfd,
						 "_SDA_BASE_",
						 BSF_GLOBAL,
						 s,
						 (bfd_vma) 32768,
						 (const char *) NULL,
						 FALSE,
						 get_elf_backend_data (abfd)->collect,
						 &bh)))
	return FALSE;
      h = (struct elf_link_hash_entry *) bh;
      h->type = STT_OBJECT;
    }

  switch (sym->st_shndx)
    {
    case SHN_M32R_SCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".scommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;
    }

  return TRUE;
}

/* We have to figure out the SDA_BASE value, so that we can adjust the
   symbol value correctly.  We look up the symbol _SDA_BASE_ in the output
   BFD.  If we can't find it, we're stuck.  We cache it in the ELF
   target data.  We don't need to adjust the symbol value for an
   external symbol if we are producing relocateable output.  */

static bfd_reloc_status_type
m32r_elf_final_sda_base (output_bfd, info, error_message, psb)
     bfd *output_bfd;
     struct bfd_link_info *info;
     const char **error_message;
     bfd_vma *psb;
{
  if (elf_gp (output_bfd) == 0)
    {
      struct bfd_link_hash_entry *h;

      h = bfd_link_hash_lookup (info->hash, "_SDA_BASE_", FALSE, FALSE, TRUE);
      if (h != (struct bfd_link_hash_entry *) NULL
	  && h->type == bfd_link_hash_defined)
	elf_gp (output_bfd) = (h->u.def.value
			       + h->u.def.section->output_section->vma
			       + h->u.def.section->output_offset);
      else
	{
	  /* Only get the error once.  */
	  *psb = elf_gp (output_bfd) = 4;
	  *error_message =
	    (const char *) _("SDA relocation when _SDA_BASE_ not defined");
	  return bfd_reloc_dangerous;
	}
    }
  *psb = elf_gp (output_bfd);
  return bfd_reloc_ok;
}

/* Relocate an M32R/D ELF section.
   There is some attempt to make this function usable for many architectures,
   both for RELA and REL type relocs, if only to serve as a learning tool.

   The RELOCATE_SECTION function is called by the new ELF backend linker
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
m32r_elf_relocate_section (output_bfd, info, input_bfd, input_section,
			   contents, relocs, local_syms, local_sections)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
{
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  struct elf_link_hash_entry **sym_hashes = elf_sym_hashes (input_bfd);
  Elf_Internal_Rela *rel, *relend;
  /* Assume success.  */
  bfd_boolean ret = TRUE;

#if !USE_REL
  if (info->relocateable)
    return TRUE;
#endif

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      /* We can't modify r_addend here as elf_link_input_bfd has an assert to
	 ensure it's zero (we use REL relocs, not RELA).  Therefore this
	 should be assigning zero to `addend', but for clarity we use
	 `r_addend'.  */
      bfd_vma addend = rel->r_addend;
      bfd_vma offset = rel->r_offset;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sec;
      const char *sym_name;
      bfd_reloc_status_type r;
      const char *errmsg = NULL;

      h = NULL;
      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_M32R_max)
	{
	  (*_bfd_error_handler) (_("%s: unknown relocation type %d"),
				 bfd_archive_filename (input_bfd),
				 (int) r_type);
	  bfd_set_error (bfd_error_bad_value);
	  ret = FALSE;
	  continue;
	}

      if (r_type == R_M32R_GNU_VTENTRY
          || r_type == R_M32R_GNU_VTINHERIT)
        continue;

      howto = m32r_elf_howto_table + r_type;
      r_symndx = ELF32_R_SYM (rel->r_info);

#if USE_REL
      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  sec = NULL;
	  if (r_symndx >= symtab_hdr->sh_info)
	    {
	      /* External symbol.  */
	      continue;
	    }

	  /* Local symbol.  */
	  sym = local_syms + r_symndx;
	  sym_name = "<local symbol>";
	  /* STT_SECTION: symbol is associated with a section.  */
	  if (ELF_ST_TYPE (sym->st_info) != STT_SECTION)
	    {
	      /* Symbol isn't associated with a section.  Nothing to do.  */
	      continue;
	    }

	  sec = local_sections[r_symndx];
	  addend += sec->output_offset + sym->st_value;

	  /* If partial_inplace, we need to store any additional addend
	     back in the section.  */
	  if (! howto->partial_inplace)
	    continue;
	  /* ??? Here is a nice place to call a special_function
	     like handler.  */
	  if (r_type != R_M32R_HI16_SLO && r_type != R_M32R_HI16_ULO)
	    r = _bfd_relocate_contents (howto, input_bfd,
					addend, contents + offset);
	  else
	    {
	      Elf_Internal_Rela *lorel;

	      /* We allow an arbitrary number of HI16 relocs before the
		 LO16 reloc.  This permits gcc to emit the HI and LO relocs
		 itself.  */
	      for (lorel = rel + 1;
		   (lorel < relend
		    && (ELF32_R_TYPE (lorel->r_info) == R_M32R_HI16_SLO
			|| ELF32_R_TYPE (lorel->r_info) == R_M32R_HI16_ULO));
		   lorel++)
		continue;
	      if (lorel < relend
		  && ELF32_R_TYPE (lorel->r_info) == R_M32R_LO16)
		{
		  m32r_elf_relocate_hi16 (input_bfd, r_type, rel, lorel,
					  contents, addend);
		  r = bfd_reloc_ok;
		}
	      else
		r = _bfd_relocate_contents (howto, input_bfd,
					    addend, contents + offset);
	    }
	}
      else
#endif /* USE_REL */
	{
	  bfd_vma relocation;

	  /* This is a final link.  */
	  sym = NULL;
	  sec = NULL;

	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      /* Local symbol.  */
	      sym = local_syms + r_symndx;
	      sec = local_sections[r_symndx];
	      sym_name = "<local symbol>";
#if !USE_REL
	      relocation = _bfd_elf_rela_local_sym (output_bfd, sym, sec, rel);
	      addend = rel->r_addend;
#else
	      /* FIXME: This won't handle local relocations against SEC_MERGE
		 symbols.  See elf32-i386.c for how to do this.  */
	      relocation = (sec->output_section->vma
			    + sec->output_offset
			    + sym->st_value);
#endif
	    }
	  else
	    {
	      /* External symbol.  */
	      h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	      while (h->root.type == bfd_link_hash_indirect
		     || h->root.type == bfd_link_hash_warning)
		h = (struct elf_link_hash_entry *) h->root.u.i.link;
	      sym_name = h->root.root.string;

	      if (h->root.type == bfd_link_hash_defined
		  || h->root.type == bfd_link_hash_defweak)
		{
		  sec = h->root.u.def.section;
		  if (sec->output_section == NULL)
		    relocation = 0;
		  else
		    relocation = (h->root.u.def.value
				  + sec->output_section->vma
				  + sec->output_offset);
		}
	      else if (h->root.type == bfd_link_hash_undefweak)
		relocation = 0;
	      else
		{
		  if (! ((*info->callbacks->undefined_symbol)
			 (info, h->root.root.string, input_bfd,
			  input_section, offset, TRUE)))
		    return FALSE;
		  relocation = 0;
		}
	    }

	  /* Sanity check the address.  */
	  if (offset > input_section->_raw_size)
	    {
	      r = bfd_reloc_outofrange;
	      goto check_reloc;
	    }

	  switch ((int) r_type)
	    {
	    case (int) R_M32R_10_PCREL :
	      r = m32r_elf_do_10_pcrel_reloc (input_bfd, howto, input_section,
					      contents, offset,
					      sec, relocation, addend);
	      break;

	    case (int) R_M32R_HI16_SLO :
	    case (int) R_M32R_HI16_ULO :
	      {
		Elf_Internal_Rela *lorel;

		/* We allow an arbitrary number of HI16 relocs before the
		   LO16 reloc.  This permits gcc to emit the HI and LO relocs
		   itself.  */
		for (lorel = rel + 1;
		     (lorel < relend
		      && (ELF32_R_TYPE (lorel->r_info) == R_M32R_HI16_SLO
			  || ELF32_R_TYPE (lorel->r_info) == R_M32R_HI16_ULO));
		     lorel++)
		  continue;
		if (lorel < relend
		    && ELF32_R_TYPE (lorel->r_info) == R_M32R_LO16)
		  {
		    m32r_elf_relocate_hi16 (input_bfd, r_type, rel, lorel,
					    contents, relocation + addend);
		    r = bfd_reloc_ok;
		  }
		else
		  r = _bfd_final_link_relocate (howto, input_bfd, input_section,
						contents, offset,
						relocation, addend);
	      }
	      break;

	    case (int) R_M32R_SDA16 :
	      {
		const char *name;

		BFD_ASSERT (sec != NULL);
		name = bfd_get_section_name (abfd, sec);

		if (strcmp (name, ".sdata") == 0
		    || strcmp (name, ".sbss") == 0
		    || strcmp (name, ".scommon") == 0)
		  {
		    bfd_vma sda_base;
		    bfd *out_bfd = sec->output_section->owner;

		    r = m32r_elf_final_sda_base (out_bfd, info,
						 &errmsg,
						 &sda_base);
		    if (r != bfd_reloc_ok)
		      {
			ret = FALSE;
			goto check_reloc;
		      }

		    /* At this point `relocation' contains the object's
		       address.  */
		    relocation -= sda_base;
		    /* Now it contains the offset from _SDA_BASE_.  */
		  }
		else
		  {
		    (*_bfd_error_handler)
		      (_("%s: The target (%s) of an %s relocation is in the wrong section (%s)"),
		       bfd_archive_filename (input_bfd),
		       sym_name,
		       m32r_elf_howto_table[(int) r_type].name,
		       bfd_get_section_name (abfd, sec));
		    /*bfd_set_error (bfd_error_bad_value); ??? why? */
		    ret = FALSE;
		    continue;
		  }
	      }
	      /* fall through */

	    default :
	      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					    contents, offset,
					    relocation, addend);
	      break;
	    }
	}

    check_reloc:

      if (r != bfd_reloc_ok)
	{
	  /* FIXME: This should be generic enough to go in a utility.  */
	  const char *name;

	  if (h != NULL)
	    name = h->root.root.string;
	  else
	    {
	      name = (bfd_elf_string_from_elf_section
		      (input_bfd, symtab_hdr->sh_link, sym->st_name));
	      if (name == NULL || *name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  if (errmsg != NULL)
	    goto common_error;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      if (! ((*info->callbacks->reloc_overflow)
		     (info, name, howto->name, (bfd_vma) 0,
		      input_bfd, input_section, offset)))
		return FALSE;
	      break;

	    case bfd_reloc_undefined:
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, name, input_bfd, input_section,
		      offset, TRUE)))
		return FALSE;
	      break;

	    case bfd_reloc_outofrange:
	      errmsg = _("internal error: out of range error");
	      goto common_error;

	    case bfd_reloc_notsupported:
	      errmsg = _("internal error: unsupported relocation error");
	      goto common_error;

	    case bfd_reloc_dangerous:
	      errmsg = _("internal error: dangerous error");
	      goto common_error;

	    default:
	      errmsg = _("internal error: unknown error");
	      /* fall through */

	    common_error:
	      if (!((*info->callbacks->warning)
		    (info, errmsg, name, input_bfd, input_section,
		     offset)))
		return FALSE;
	      break;
	    }
	}
    }

  return ret;
}

#if 0 /* relaxing not supported yet */

/* This function handles relaxing for the m32r.
   Relaxing on the m32r is tricky because of instruction alignment
   requirements (4 byte instructions must be aligned on 4 byte boundaries).

   The following relaxing opportunities are handled:

   seth/add3/jl -> bl24 or bl8
   seth/add3 -> ld24

   It would be nice to handle bl24 -> bl8 but given:

   - 4 byte insns must be on 4 byte boundaries
   - branch instructions only branch to insns on 4 byte boundaries

   this isn't much of a win because the insn in the 2 "deleted" bytes
   must become a nop.  With some complexity some real relaxation could be
   done but the frequency just wouldn't make it worth it; it's better to
   try to do all the code compaction one can elsewhere.
   When the chip supports parallel 16 bit insns, things may change.
*/

static bfd_boolean
m32r_elf_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     bfd_boolean *again;
{
  Elf_Internal_Shdr *symtab_hdr;
  /* The Rela structures are used here because that's what
     _bfd_elf32_link_read_relocs uses [for convenience - it sets the addend
     field to 0].  */
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents = NULL;
  Elf_Internal_Sym *isymbuf = NULL;

  /* Assume nothing changes.  */
  *again = FALSE;

  /* We don't have to do anything for a relocateable link, if
     this section does not have relocs, or if this is not a
     code section.  */
  if (link_info->relocateable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0
      || 0 /* FIXME: check SHF_M32R_CAN_RELAX */)
    return TRUE;

  /* If this is the first time we have been called for this section,
     initialize the cooked size.  */
  if (sec->_cooked_size == 0)
    sec->_cooked_size = sec->_raw_size;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs = (_bfd_elf32_link_read_relocs
		     (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;

  /* Walk through them looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;

      /* If this isn't something that can be relaxed, then ignore
	 this reloc.  */
      if (ELF32_R_TYPE (irel->r_info) != (int) R_M32R_HI16_SLO)
	continue;

      /* Get the section contents if we haven't done so already.  */
      if (contents == NULL)
	{
	  /* Get cached copy if it exists.  */
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    contents = elf_section_data (sec)->this_hdr.contents;
	  else
	    {
	      /* Go get them off disk.  */
	      contents = (bfd_byte *) bfd_malloc (sec->_raw_size);
	      if (contents == NULL)
		goto error_return;

	      if (! bfd_get_section_contents (abfd, sec, contents,
					      (file_ptr) 0, sec->_raw_size))
		goto error_return;
	    }
	}

      /* Read this BFD's local symbols if we haven't done so already.  */
      if (isymbuf == NULL && symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  Elf_Internal_Sym *isym;
	  asection *sym_sec;

	  isym = isymbuf + ELF32_R_SYM (irel->r_info),
	  sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
	  symval = (isym->st_value
		    + sym_sec->output_section->vma
		    + sym_sec->output_offset);
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry *h;

	  /* An external symbol.  */
	  indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);
	  if (h->root.type != bfd_link_hash_defined
	      && h->root.type != bfd_link_hash_defweak)
	    {
	      /* This appears to be a reference to an undefined
                 symbol.  Just ignore it--it will be caught by the
                 regular reloc processing.  */
	      continue;
	    }

	  symval = (h->root.u.def.value
		    + h->root.u.def.section->output_section->vma
		    + h->root.u.def.section->output_offset);
	}

      /* For simplicity of coding, we are going to modify the section
	 contents, the section relocs, and the BFD symbol table.  We
	 must tell the rest of the code not to free up this
	 information.  It would be possible to instead create a table
	 of changes which have to be made, as is done in coff-mips.c;
	 that would be more work, but would require less memory when
	 the linker is run.  */

      /* Try to change a seth/add3/jl subroutine call to bl24 or bl8.
	 This sequence is generated by the compiler when compiling in
	 32 bit mode.  Also look for seth/add3 -> ld24.  */

      if (ELF32_R_TYPE (irel->r_info) == (int) R_M32R_HI16_SLO)
	{
	  Elf_Internal_Rela *nrel;
	  bfd_vma pc = (sec->output_section->vma + sec->output_offset
			+ irel->r_offset);
	  bfd_signed_vma pcrel_value = symval - pc;
	  unsigned int code,reg;
	  int addend,nop_p,bl8_p,to_delete;

	  /* The tests are ordered so that we get out as quickly as possible
	     if this isn't something we can relax, taking into account that
	     we are looking for two separate possibilities (jl/ld24).  */

	  /* Do nothing if no room in the section for this to be what we're
	     looking for.  */
	  if (irel->r_offset > sec->_cooked_size - 8)
	    continue;

	  /* Make sure the next relocation applies to the next
	     instruction and that it's the add3's reloc.  */
	  nrel = irel + 1;
	  if (nrel == irelend
	      || irel->r_offset + 4 != nrel->r_offset
	      || ELF32_R_TYPE (nrel->r_info) != (int) R_M32R_LO16)
	    continue;

	  /* See if the instructions are seth/add3.  */
	  /* FIXME: This is where macros from cgen can come in.  */
	  code = bfd_get_16 (abfd, contents + irel->r_offset + 0);
	  if ((code & 0xf0ff) != 0xd0c0)
	    continue; /* not seth rN,foo */
	  reg = (code & 0x0f00) >> 8;
	  code = bfd_get_16 (abfd, contents + irel->r_offset + 4);
	  if (code != (0x80a0 | reg | (reg << 8)))
	    continue; /* not add3 rN,rN,foo */

	  /* At this point we've confirmed we have seth/add3.  Now check
	     whether the next insn is a jl, in which case try to change this
	     to bl24 or bl8.  */

	  /* Ensure the branch target is in range.
	     The bl24 instruction has a 24 bit operand which is the target
	     address right shifted by 2, giving a signed range of 26 bits.
	     Note that 4 bytes are added to the high value because the target
	     will be at least 4 bytes closer if we can relax.  It'll actually
	     be 4 or 8 bytes closer, but we don't know which just yet and
	     the difference isn't significant enough to worry about.  */
#if !USE_REL /* put in for learning purposes */
	  pcrel_value += irel->r_addend;
#else
	  addend = bfd_get_signed_16 (abfd, contents + irel->r_offset + 2);
	  pcrel_value += addend;
#endif

	  if (pcrel_value >= -(1 << 25) && pcrel_value < (1 << 25) + 4
	      /* Do nothing if no room in the section for this to be what we're
		 looking for.  */
	      && (irel->r_offset <= sec->_cooked_size - 12)
	      /* Ensure the next insn is "jl rN".  */
	      && ((code = bfd_get_16 (abfd, contents + irel->r_offset + 8)),
		  code != (0x1ec0 | reg)))
	    {
	      /* We can relax to bl24/bl8.  */

	      /* See if there's a nop following the jl.
		 Also see if we can use a bl8 insn.  */
	      code = bfd_get_16 (abfd, contents + irel->r_offset + 10);
	      nop_p = (code & 0x7fff) == NOP_INSN;
	      bl8_p = pcrel_value >= -0x200 && pcrel_value < 0x200;

	      if (bl8_p)
		{
		  /* Change "seth rN,foo" to "bl8 foo || nop".
		     We OR in CODE just in case it's not a nop (technically,
		     CODE currently must be a nop, but for cleanness we
		     allow it to be anything).  */
#if !USE_REL /* put in for learning purposes */
		  code = 0x7e000000 | MAKE_PARALLEL (code);
#else
		  code = (0x7e000000 + (((addend >> 2) & 0xff) << 16)) | MAKE_PARALLEL (code);
#endif
		  to_delete = 8;
		}
	      else
		{
		  /* Change the seth rN,foo to a bl24 foo.  */
#if !USE_REL /* put in for learning purposes */
		  code = 0xfe000000;
#else
		  code = 0xfe000000 + ((addend >> 2) & 0xffffff);
#endif
		  to_delete = nop_p ? 8 : 4;
		}

	      bfd_put_32 (abfd, code, contents + irel->r_offset);

	      /* Set the new reloc type.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (nrel->r_info),
					   bl8_p ? R_M32R_10_PCREL : R_M32R_26_PCREL);

	      /* Delete the add3 reloc by making it a null reloc.  */
	      nrel->r_info = ELF32_R_INFO (ELF32_R_SYM (nrel->r_info),
					   R_M32R_NONE);
	    }
	  else if (addend >= 0
		   && symval + addend <= 0xffffff)
	    {
	      /* We can relax to ld24.  */

	      code = 0xe0000000 | (reg << 24) | (addend & 0xffffff);
	      bfd_put_32 (abfd, code, contents + irel->r_offset);
	      to_delete = 4;
	      /* Tell the following code a nop filler isn't needed.  */
	      nop_p = 1;
	    }
	  else
	    {
	      /* Can't do anything here.  */
	      continue;
	    }

	  /* Note that we've changed the relocs, section contents, etc.  */
	  elf_section_data (sec)->relocs = internal_relocs;
	  elf_section_data (sec)->this_hdr.contents = contents;
	  symtab_hdr->contents = (unsigned char *) isymbuf;

	  /* Delete TO_DELETE bytes of data.  */
	  if (!m32r_elf_relax_delete_bytes (abfd, sec,
					    irel->r_offset + 4, to_delete))
	    goto error_return;

	  /* Now that the following bytes have been moved into place, see if
	     we need to replace the jl with a nop.  This happens when we had
	     to use a bl24 insn and the insn following the jl isn't a nop.
	     Technically, this situation can't happen (since the insn can
	     never be executed) but to be clean we do this.  When the chip
	     supports parallel 16 bit insns things may change.
	     We don't need to do this in the case of relaxing to ld24,
	     and the above code sets nop_p so this isn't done.  */
	  if (! nop_p && to_delete == 4)
	    bfd_put_16 (abfd, NOP_INSN, contents + irel->r_offset + 4);

	  /* That will change things, so we should relax again.
	     Note that this is not required, and it may be slow.  */
	  *again = TRUE;

	  continue;
	}

      /* loop to try the next reloc */
    }

  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    {
      if (! link_info->keep_memory)
	free (isymbuf);
      else
	{
	  /* Cache the symbols for elf_link_input_bfd.  */
	  symtab_hdr->contents = (unsigned char *) isymbuf;
	}
    }

  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    {
      if (! link_info->keep_memory)
	free (contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = contents;
	}
    }

  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return TRUE;

 error_return:
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return FALSE;
}

/* Delete some bytes from a section while relaxing.  */

static bfd_boolean
m32r_elf_relax_delete_bytes (abfd, sec, addr, count)
     bfd *abfd;
     asection *sec;
     bfd_vma addr;
     int count;
{
  Elf_Internal_Shdr *symtab_hdr;
  int shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend;
  Elf_Internal_Rela *irelalign;
  bfd_vma toaddr;
  Elf_Internal_Sym *isym, *isymend;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;

  shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  /* The deletion must stop at the next ALIGN reloc for an aligment
     power larger than the number of bytes we are deleting.  */

  irelalign = NULL;
  toaddr = sec->_cooked_size;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count, toaddr - addr - count);
  sec->_cooked_size -= count;

  /* Adjust all the relocs.  */
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      /* Get the new reloc address.  */
      if ((irel->r_offset > addr
	   && irel->r_offset < toaddr))
	irel->r_offset -= count;
    }

  /* Adjust the local symbols defined in this section.  */
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isym = (Elf_Internal_Sym *) symtab_hdr->contents;
  for (isymend = isym + symtab_hdr->sh_info; isym < isymend; isym++)
    {
      if (isym->st_shndx == shndx
	  && isym->st_value > addr
	  && isym->st_value < toaddr)
	isym->st_value -= count;
    }

  /* Now adjust the global symbols defined in this section.  */
  symcount = (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
	      - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;

      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec
	  && sym_hash->root.u.def.value > addr
	  && sym_hash->root.u.def.value < toaddr)
	{
	  sym_hash->root.u.def.value -= count;
	}
    }

  return TRUE;
}

/* This is a version of bfd_generic_get_relocated_section_contents
   which uses m32r_elf_relocate_section.  */

static bfd_byte *
m32r_elf_get_relocated_section_contents (output_bfd, link_info, link_order,
					 data, relocateable, symbols)
     bfd *output_bfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     bfd_boolean relocateable;
     asymbol **symbols;
{
  Elf_Internal_Shdr *symtab_hdr;
  asection *input_section = link_order->u.indirect.section;
  bfd *input_bfd = input_section->owner;
  asection **sections = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Sym *isymbuf = NULL;
  bfd_size_type amt;

  /* We only need to handle the case of relaxing, or of having a
     particular set of section contents, specially.  */
  if (relocateable
      || elf_section_data (input_section)->this_hdr.contents == NULL)
    return bfd_generic_get_relocated_section_contents (output_bfd, link_info,
						       link_order, data,
						       relocateable,
						       symbols);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  memcpy (data, elf_section_data (input_section)->this_hdr.contents,
	  input_section->_raw_size);

  if ((input_section->flags & SEC_RELOC) != 0
      && input_section->reloc_count > 0)
    {
      Elf_Internal_Sym *isymp;
      asection **secpp;
      Elf32_External_Sym *esym, *esymend;

      internal_relocs = (_bfd_elf32_link_read_relocs
			 (input_bfd, input_section, (PTR) NULL,
			  (Elf_Internal_Rela *) NULL, FALSE));
      if (internal_relocs == NULL)
	goto error_return;

      if (symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (input_bfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      amt = symtab_hdr->sh_info;
      amt *= sizeof (asection *);
      sections = (asection **) bfd_malloc (amt);
      if (sections == NULL && symtab_hdr->sh_info > 0)
	goto error_return;

      isymend = isymbuf + symtab_hdr->sh_info;
      for (isym = isymbuf, secpp = sections; isym < isymend; ++isym, ++secpp)
	{
	  asection *isec;

	  if (isym->st_shndx == SHN_UNDEF)
	    isec = bfd_und_section_ptr;
	  else if (isym->st_shndx == SHN_ABS)
	    isec = bfd_abs_section_ptr;
	  else if (isym->st_shndx == SHN_COMMON)
	    isec = bfd_com_section_ptr;
	  else if (isym->st_shndx == SHN_M32R_SCOMMON)
	    isec = &m32r_elf_scom_section;
	  else
	    isec = bfd_section_from_elf_index (input_bfd, isym->st_shndx);

	  *secpp = isec;
	}

      if (! m32r_elf_relocate_section (output_bfd, link_info, input_bfd,
				       input_section, data, internal_relocs,
				       isymbuf, sections))
	goto error_return;

      if (sections != NULL)
	free (sections);
      if (isymbuf != NULL
	  && symtab_hdr->contents != (unsigned char *) isymbuf)
	free (isymbuf);
      if (elf_section_data (input_section)->relocs != internal_relocs)
	free (internal_relocs);
    }

  return data;

 error_return:
  if (sections != NULL)
    free (sections);
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (internal_relocs != NULL
      && elf_section_data (input_section)->relocs != internal_relocs)
    free (internal_relocs);
  return NULL;
}

#endif /* #if 0 */

/* Set the right machine number.  */
static bfd_boolean
m32r_elf_object_p (abfd)
     bfd *abfd;
{
  switch (elf_elfheader (abfd)->e_flags & EF_M32R_ARCH)
    {
    default:
    case E_M32R_ARCH:   (void) bfd_default_set_arch_mach (abfd, bfd_arch_m32r, bfd_mach_m32r);  break;
    case E_M32RX_ARCH:  (void) bfd_default_set_arch_mach (abfd, bfd_arch_m32r, bfd_mach_m32rx); break;
    }
  return TRUE;
}

/* Store the machine number in the flags field.  */
static void
m32r_elf_final_write_processing (abfd, linker)
     bfd *abfd;
     bfd_boolean linker ATTRIBUTE_UNUSED;
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
    default:
    case bfd_mach_m32r:  val = E_M32R_ARCH; break;
    case bfd_mach_m32rx: val = E_M32RX_ARCH; break;
    }

  elf_elfheader (abfd)->e_flags &=~ EF_M32R_ARCH;
  elf_elfheader (abfd)->e_flags |= val;
}

/* Function to keep M32R specific file flags.  */
static bfd_boolean
m32r_elf_set_private_flags (abfd, flags)
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
m32r_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword out_flags;
  flagword in_flags;

  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  in_flags  = elf_elfheader (ibfd)->e_flags;
  out_flags = elf_elfheader (obfd)->e_flags;

  if (! elf_flags_init (obfd))
    {
      /* If the input is the default architecture then do not
	 bother setting the flags for the output architecture,
	 instead allow future merges to do this.  If no future
	 merges ever set these flags then they will retain their
	 unitialised values, which surprise surprise, correspond
	 to the default values.  */
      if (bfd_get_arch_info (ibfd)->the_default)
	return TRUE;

      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = in_flags;

      if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
	  && bfd_get_arch_info (obfd)->the_default)
	{
	  return bfd_set_arch_mach (obfd, bfd_get_arch (ibfd), bfd_get_mach (ibfd));
	}

      return TRUE;
    }

  /* Check flag compatibility.  */
  if (in_flags == out_flags)
    return TRUE;

  if ((in_flags & EF_M32R_ARCH) != (out_flags & EF_M32R_ARCH))
    {
      if ((in_flags & EF_M32R_ARCH) != E_M32R_ARCH)
	{
	  (*_bfd_error_handler)
	    (_("%s: Instruction set mismatch with previous modules"),
	     bfd_archive_filename (ibfd));

	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}
    }

  return TRUE;
}

/* Display the flags field */
static bfd_boolean
m32r_elf_print_private_bfd_data (abfd, ptr)
     bfd *abfd;
     PTR ptr;
{
  FILE * file = (FILE *) ptr;

  BFD_ASSERT (abfd != NULL && ptr != NULL)

  _bfd_elf_print_private_bfd_data (abfd, ptr);

  fprintf (file, _("private flags = %lx"), elf_elfheader (abfd)->e_flags);

  switch (elf_elfheader (abfd)->e_flags & EF_M32R_ARCH)
    {
    default:
    case E_M32R_ARCH:  fprintf (file, _(": m32r instructions"));  break;
    case E_M32RX_ARCH: fprintf (file, _(": m32rx instructions")); break;
    }

  fputc ('\n', file);

  return TRUE;
}

asection *
m32r_elf_gc_mark_hook (sec, info, rel, h, sym)
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
      case R_M32R_GNU_VTINHERIT:
      case R_M32R_GNU_VTENTRY:
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

static bfd_boolean
m32r_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  /* we don't use got and plt entries for m32r */
  return TRUE;
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
m32r_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  if (info->relocateable)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof (Elf32_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
        h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      switch (ELF32_R_TYPE (rel->r_info))
        {
        /* This relocation describes the C++ object vtable hierarchy.
           Reconstruct it for later use during GC.  */
        case R_M32R_GNU_VTINHERIT:
          if (!_bfd_elf32_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

        /* This relocation describes which C++ vtable entries are actually
           used.  Record for later use during GC.  */
        case R_M32R_GNU_VTENTRY:
          if (!_bfd_elf32_gc_record_vtentry (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;
        }
    }

  return TRUE;
}

#define ELF_ARCH		bfd_arch_m32r
#define ELF_MACHINE_CODE	EM_M32R
#define ELF_MACHINE_ALT1	EM_CYGNUS_M32R
#define ELF_MAXPAGESIZE		0x1 /* Explicitly requested by Mitsubishi.  */

#define TARGET_BIG_SYM          bfd_elf32_m32r_vec
#define TARGET_BIG_NAME		"elf32-m32r"

#define elf_info_to_howto			0
#define elf_info_to_howto_rel			m32r_info_to_howto_rel
#define elf_backend_section_from_bfd_section	_bfd_m32r_elf_section_from_bfd_section
#define elf_backend_symbol_processing		_bfd_m32r_elf_symbol_processing
#define elf_backend_add_symbol_hook		m32r_elf_add_symbol_hook
#define elf_backend_relocate_section		m32r_elf_relocate_section
#define elf_backend_gc_mark_hook                m32r_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook               m32r_elf_gc_sweep_hook
#define elf_backend_check_relocs                m32r_elf_check_relocs

#define elf_backend_can_gc_sections             1
#if !USE_REL
#define elf_backend_rela_normal			1
#endif
#if 0 /* not yet */
/* relax support */
#define bfd_elf32_bfd_relax_section		m32r_elf_relax_section
#define bfd_elf32_bfd_get_relocated_section_contents \
					m32r_elf_get_relocated_section_contents
#endif

#define elf_backend_object_p			m32r_elf_object_p
#define elf_backend_final_write_processing 	m32r_elf_final_write_processing
#define bfd_elf32_bfd_merge_private_bfd_data 	m32r_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags		m32r_elf_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data	m32r_elf_print_private_bfd_data

#include "elf32-target.h"
