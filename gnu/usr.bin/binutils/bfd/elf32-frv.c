/* FRV-specific support for 32-bit ELF.
   Copyright 2002 Free Software Foundation, Inc.

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
#include "elf/frv.h"

/* Forward declarations.  */
static bfd_reloc_status_type elf32_frv_relocate_lo16
  PARAMS ((bfd *,  Elf_Internal_Rela *, bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_hi16
  PARAMS ((bfd *,  Elf_Internal_Rela *, bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_label24
  PARAMS ((bfd *, asection *, Elf_Internal_Rela *, bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_gprel12
  PARAMS ((struct bfd_link_info *, bfd *, asection *, Elf_Internal_Rela *,
	   bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_gprelu12
  PARAMS ((struct bfd_link_info *, bfd *, asection *, Elf_Internal_Rela *,
	   bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_gprello
  PARAMS ((struct bfd_link_info *, bfd *, asection *, Elf_Internal_Rela *,
	   bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_gprelhi
  PARAMS ((struct bfd_link_info *, bfd *, asection *, Elf_Internal_Rela *,
	   bfd_byte *, bfd_vma));
static reloc_howto_type *frv_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void frv_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static bfd_boolean elf32_frv_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static bfd_boolean elf32_frv_add_symbol_hook
  PARAMS (( bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	    const char **, flagword *, asection **, bfd_vma *));
static bfd_reloc_status_type frv_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, bfd_vma));
static bfd_boolean elf32_frv_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *, const
	   Elf_Internal_Rela *));
static asection * elf32_frv_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static bfd_boolean elf32_frv_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static int elf32_frv_machine
  PARAMS ((bfd *));
static bfd_boolean elf32_frv_object_p
  PARAMS ((bfd *));
static bfd_boolean frv_elf_set_private_flags
  PARAMS ((bfd *, flagword));
static bfd_boolean frv_elf_copy_private_bfd_data
  PARAMS ((bfd *, bfd *));
static bfd_boolean frv_elf_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static bfd_boolean frv_elf_print_private_bfd_data
  PARAMS ((bfd *, PTR));

static reloc_howto_type elf32_frv_howto_table [] =
{
  /* This reloc does nothing.  */
  HOWTO (R_FRV_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_FRV_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit pc-relative relocation.  */
  HOWTO (R_FRV_LABEL16,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_LABEL16",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  /* A 24-bit pc-relative relocation.  */
  HOWTO (R_FRV_LABEL24,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_LABEL24",	/* name */
	 FALSE,			/* partial_inplace */
	 0x7e03ffff,		/* src_mask */
	 0x7e03ffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_FRV_LO16,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_LO16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_HI16,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_HI16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_GPREL12,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GPREL12",       /* name */
	 FALSE,			/* partial_inplace */
	 0xfff,		        /* src_mask */
	 0xfff,		        /* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_GPRELU12,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GPRELU12",      /* name */
	 FALSE,			/* partial_inplace */
	 0xfff,		        /* src_mask */
	 0x3f03f,	        /* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_GPREL32,         /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GPREL32",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_GPRELHI,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GPRELHI",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		        /* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_GPRELLO,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GPRELLO",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		        /* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
};

/* GNU extension to record C++ vtable hierarchy.  */
static reloc_howto_type elf32_frv_vtinherit_howto =
  HOWTO (R_FRV_GNU_VTINHERIT,   /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_FRV_GNU_VTINHERIT", /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE);                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage.  */
static reloc_howto_type elf32_frv_vtentry_howto =
  HOWTO (R_FRV_GNU_VTENTRY,     /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_FRV_GNU_VTENTRY",   /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE);                /* pcrel_offset */

/* Map BFD reloc types to FRV ELF reloc types.  */
#if 0
struct frv_reloc_map
{
  unsigned int bfd_reloc_val;
  unsigned int frv_reloc_val;
};

static const struct frv_reloc_map frv_reloc_map [] =
{
  { BFD_RELOC_NONE,           R_FRV_NONE },
  { BFD_RELOC_32,             R_FRV_32 },
  { BFD_RELOC_FRV_LABEL16,    R_FRV_LABEL16 },
  { BFD_RELOC_FRV_LABEL24,    R_FRV_LABEL24 },
  { BFD_RELOC_FRV_LO16,       R_FRV_LO16 },
  { BFD_RELOC_FRV_HI16,       R_FRV_HI16 },
  { BFD_RELOC_FRV_GPREL12,    R_FRV_GPREL12 },
  { BFD_RELOC_FRV_GPRELU12,   R_FRV_GPRELU12 },
  { BFD_RELOC_FRV_GPREL32,    R_FRV_GPREL32 },
  { BFD_RELOC_FRV_GPRELHI,    R_FRV_GPRELHI },
  { BFD_RELOC_FRV_GPRELLO,    R_FRV_GPRELLO },
  { BFD_RELOC_VTABLE_INHERIT, R_FRV_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY,   R_FRV_GNU_VTENTRY },
};
#endif

/* Handle an FRV small data reloc.  */

static bfd_reloc_status_type
elf32_frv_relocate_gprel12 (info, input_bfd, input_section, relocation,
			    contents, value)
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *relocation;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  bfd_vma gp;
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (info->hash, "_gp", FALSE, FALSE, TRUE);

  gp = (h->u.def.value
	+ h->u.def.section->output_section->vma
	+ h->u.def.section->output_offset);

  value -= input_section->output_section->vma;
  value -= (gp - input_section->output_section->vma);

  insn = bfd_get_32 (input_bfd, contents + relocation->r_offset);

  value += relocation->r_addend;

  if ((long) value > 0x7ff || (long) value < -0x800)
    return bfd_reloc_overflow;

  bfd_put_32 (input_bfd,
	      (insn & 0xfffff000) | (value & 0xfff),
	      contents + relocation->r_offset);

  return bfd_reloc_ok;
}

/* Handle an FRV small data reloc. for the u12 field.  */

static bfd_reloc_status_type
elf32_frv_relocate_gprelu12 (info, input_bfd, input_section, relocation,
			     contents, value)
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *relocation;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  bfd_vma gp;
  struct bfd_link_hash_entry *h;
  bfd_vma mask;

  h = bfd_link_hash_lookup (info->hash, "_gp", FALSE, FALSE, TRUE);

  gp = (h->u.def.value
	+ h->u.def.section->output_section->vma
	+ h->u.def.section->output_offset);

  value -= input_section->output_section->vma;
  value -= (gp - input_section->output_section->vma);

  insn = bfd_get_32 (input_bfd, contents + relocation->r_offset);

  value += relocation->r_addend;

  if ((long) value > 0x7ff || (long) value < -0x800)
    return bfd_reloc_overflow;

  /* The high 6 bits go into bits 17-12. The low 6 bits go into bits 5-0.  */
  mask = 0x3f03f;
  insn = (insn & ~mask) | ((value & 0xfc0) << 12) | (value & 0x3f);

  bfd_put_32 (input_bfd, insn, contents + relocation->r_offset);

  return bfd_reloc_ok;
}

/* Handle an FRV ELF HI16 reloc.  */

static bfd_reloc_status_type
elf32_frv_relocate_hi16 (input_bfd, relhi, contents, value)
     bfd *input_bfd;
     Elf_Internal_Rela *relhi;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;

  insn = bfd_get_32 (input_bfd, contents + relhi->r_offset);

  value += relhi->r_addend;
  value = ((value >> 16) & 0xffff);

  insn = (insn & 0xffff0000) | value;

  if ((long) value > 0xffff || (long) value < -0x10000)
    return bfd_reloc_overflow;

  bfd_put_32 (input_bfd, insn, contents + relhi->r_offset);
  return bfd_reloc_ok;

}
static bfd_reloc_status_type
elf32_frv_relocate_lo16 (input_bfd, rello, contents, value)
     bfd *input_bfd;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;

  insn = bfd_get_32 (input_bfd, contents + rello->r_offset);

  value += rello->r_addend;
  value = value & 0xffff;

  insn = (insn & 0xffff0000) | value;

  if ((long) value > 0xffff || (long) value < -0x10000)
    return bfd_reloc_overflow;

  bfd_put_32 (input_bfd, insn, contents + rello->r_offset);
  return bfd_reloc_ok;
}

/* Perform the relocation for the CALL label24 instruction.  */

static bfd_reloc_status_type
elf32_frv_relocate_label24 (input_bfd, input_section, rello, contents, value)
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  bfd_vma label6;
  bfd_vma label18;

  /* The format for the call instruction is:

    0 000000 0001111 000000000000000000
      label6 opcode  label18

    The branch calculation is: pc + (4*label24)
    where label24 is the concatenation of label6 and label18.  */

  /* Grab the instruction.  */
  insn = bfd_get_32 (input_bfd, contents + rello->r_offset);

  value -= input_section->output_section->vma + input_section->output_offset;
  value -= rello->r_offset;
  value += rello->r_addend;

  value = value >> 2;

  label6  = value & 0xfc0000;
  label6  = label6 << 7;

  label18 = value & 0x3ffff;

  insn = insn & 0x803c0000;
  insn = insn | label6;
  insn = insn | label18;

  bfd_put_32 (input_bfd, insn, contents + rello->r_offset);

  return bfd_reloc_ok;
}

static bfd_reloc_status_type
elf32_frv_relocate_gprelhi (info, input_bfd, input_section, relocation,
			    contents, value)
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *relocation;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  bfd_vma gp;
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (info->hash, "_gp", FALSE, FALSE, TRUE);

  gp = (h->u.def.value
        + h->u.def.section->output_section->vma
        + h->u.def.section->output_offset);

  value -= input_section->output_section->vma;
  value -= (gp - input_section->output_section->vma);
  value += relocation->r_addend;
  value = ((value >> 16) & 0xffff);

  if ((long) value > 0xffff || (long) value < -0x10000)
    return bfd_reloc_overflow;

  insn = bfd_get_32 (input_bfd, contents + relocation->r_offset);
  insn = (insn & 0xffff0000) | value;

  bfd_put_32 (input_bfd, insn, contents + relocation->r_offset);
  return bfd_reloc_ok;
}

static bfd_reloc_status_type
elf32_frv_relocate_gprello (info, input_bfd, input_section, relocation,
			    contents, value)
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *relocation;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  bfd_vma gp;
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (info->hash, "_gp", FALSE, FALSE, TRUE);

  gp = (h->u.def.value
        + h->u.def.section->output_section->vma
        + h->u.def.section->output_offset);

  value -= input_section->output_section->vma;
  value -= (gp - input_section->output_section->vma);
  value += relocation->r_addend;
  value = value & 0xffff;

  if ((long) value > 0xffff || (long) value < -0x10000)
    return bfd_reloc_overflow;

  insn = bfd_get_32 (input_bfd, contents + relocation->r_offset);
  insn = (insn & 0xffff0000) | value;

  bfd_put_32 (input_bfd, insn, contents + relocation->r_offset);

 return bfd_reloc_ok;
}

static reloc_howto_type *
frv_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    default:
      break;

    case BFD_RELOC_NONE:
      return &elf32_frv_howto_table[ (int) R_FRV_NONE];

    case BFD_RELOC_32:
    case BFD_RELOC_CTOR:
      return &elf32_frv_howto_table[ (int) R_FRV_32];

    case BFD_RELOC_FRV_LABEL16:
      return &elf32_frv_howto_table[ (int) R_FRV_LABEL16];

    case BFD_RELOC_FRV_LABEL24:
      return &elf32_frv_howto_table[ (int) R_FRV_LABEL24];

    case BFD_RELOC_FRV_LO16:
      return &elf32_frv_howto_table[ (int) R_FRV_LO16];

    case BFD_RELOC_FRV_HI16:
      return &elf32_frv_howto_table[ (int) R_FRV_HI16];

    case BFD_RELOC_FRV_GPREL12:
      return &elf32_frv_howto_table[ (int) R_FRV_GPREL12];

    case BFD_RELOC_FRV_GPRELU12:
      return &elf32_frv_howto_table[ (int) R_FRV_GPRELU12];

    case BFD_RELOC_FRV_GPREL32:
      return &elf32_frv_howto_table[ (int) R_FRV_GPREL32];

    case BFD_RELOC_FRV_GPRELHI:
      return &elf32_frv_howto_table[ (int) R_FRV_GPRELHI];

    case BFD_RELOC_FRV_GPRELLO:
      return &elf32_frv_howto_table[ (int) R_FRV_GPRELLO];

    case BFD_RELOC_VTABLE_INHERIT:
      return &elf32_frv_vtinherit_howto;

    case BFD_RELOC_VTABLE_ENTRY:
      return &elf32_frv_vtentry_howto;
    }

  return NULL;
}

/* Set the howto pointer for an FRV ELF reloc.  */

static void
frv_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  switch (r_type)
    {
    case R_FRV_GNU_VTINHERIT:
      cache_ptr->howto = &elf32_frv_vtinherit_howto;
      break;

    case R_FRV_GNU_VTENTRY:
      cache_ptr->howto = &elf32_frv_vtentry_howto;
      break;

    default:
      cache_ptr->howto = & elf32_frv_howto_table [r_type];
      break;
    }
}

/* Perform a single relocation.  By default we use the standard BFD
   routines, but a few relocs, we have to do them ourselves.  */

static bfd_reloc_status_type
frv_final_link_relocate (howto, input_bfd, input_section, contents, rel,
			 relocation)
     reloc_howto_type *howto;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *rel;
     bfd_vma relocation;
{
  return _bfd_final_link_relocate (howto, input_bfd, input_section,
				   contents, rel->r_offset, relocation,
				   rel->r_addend);
}


/* Relocate an FRV ELF section.

   The RELOCATE_SECTION function is called by the new ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjusting the section contents as
   necessary, and (if using Rela relocs and generating a relocateable
   output file) adjusting the reloc addend as necessary.

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
elf32_frv_relocate_section (output_bfd, info, input_bfd, input_section,
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
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  if (info->relocateable)
    return TRUE;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;
      const char * name = NULL;
      int r_type;

      r_type = ELF32_R_TYPE (rel->r_info);

      if (   r_type == R_FRV_GNU_VTINHERIT
	  || r_type == R_FRV_GNU_VTENTRY)
	continue;

      /* This is a final link.  */
      r_symndx = ELF32_R_SYM (rel->r_info);
      howto  = elf32_frv_howto_table + ELF32_R_TYPE (rel->r_info);
      h      = NULL;
      sym    = NULL;
      sec    = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);

	  name = bfd_elf_string_from_elf_section
	    (input_bfd, symtab_hdr->sh_link, sym->st_name);
	  name = (name == NULL) ? bfd_section_name (input_bfd, sec) : name;
	}
      else
	{
	  h = sym_hashes [r_symndx - symtab_hdr->sh_info];

	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  name = h->root.root.string;

	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      relocation = (h->root.u.def.value
			    + sec->output_section->vma
			    + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    {
	      relocation = 0;
	    }
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset, TRUE)))
		return FALSE;
	      relocation = 0;
	    }
	}

     if (r_type == R_FRV_HI16)
       r = elf32_frv_relocate_hi16 (input_bfd, rel, contents, relocation);

     else if (r_type == R_FRV_LO16)
       r = elf32_frv_relocate_lo16 (input_bfd, rel, contents, relocation);

     else if (r_type == R_FRV_LABEL24)
       r = elf32_frv_relocate_label24 (input_bfd, input_section, rel,
				       contents, relocation);

     else if (r_type == R_FRV_GPREL12)
       r = elf32_frv_relocate_gprel12 (info, input_bfd, input_section, rel,
				       contents, relocation);

     else if (r_type == R_FRV_GPRELU12)
       r = elf32_frv_relocate_gprelu12 (info, input_bfd, input_section, rel,
					contents, relocation);

     else if (r_type == R_FRV_GPRELLO)
       r = elf32_frv_relocate_gprello (info, input_bfd, input_section, rel,
				       contents, relocation);

     else if (r_type == R_FRV_GPRELHI)
       r = elf32_frv_relocate_gprelhi (info, input_bfd, input_section, rel,
				       contents, relocation);

     else
       r = frv_final_link_relocate (howto, input_bfd, input_section, contents,
				    rel, relocation);

      if (r != bfd_reloc_ok)
	{
	  const char * msg = (const char *) NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		(info, name, howto->name, (bfd_vma) 0,
		 input_bfd, input_section, rel->r_offset);
	      break;

	    case bfd_reloc_undefined:
	      r = info->callbacks->undefined_symbol
		(info, name, input_bfd, input_section, rel->r_offset, TRUE);
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      break;

	    case bfd_reloc_notsupported:
	      msg = _("internal error: unsupported relocation error");
	      break;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous relocation");
	      break;

	    default:
	      msg = _("internal error: unknown error");
	      break;
	    }

	  if (msg)
	    r = info->callbacks->warning
	      (info, msg, name, input_bfd, input_section, rel->r_offset);

	  if (! r)
	    return FALSE;
	}
    }

  return TRUE;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
elf32_frv_gc_mark_hook (sec, info, rel, h, sym)
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
	case R_FRV_GNU_VTINHERIT:
	case R_FRV_GNU_VTENTRY:
	  break;

	default:
	  switch (h->root.type)
	    {
	    default:
	      break;

	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      return h->root.u.def.section;

	    case bfd_link_hash_common:
	      return h->root.u.c.p->section;
	    }
	}
    }
  else
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}

/* Update the got entry reference counts for the section being removed.  */

static bfd_boolean
elf32_frv_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  return TRUE;
}


/* Hook called by the linker routine which adds symbols from an object
   file.  We use it to put .comm items in .scomm, and not .comm.  */

static bfd_boolean
elf32_frv_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
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
      && (int)sym->st_size <= (int)bfd_get_gp_size (abfd))
    {
      /* Common symbols less than or equal to -G nn bytes are
	 automatically put into .sbss.  */

      asection *scomm = bfd_get_section_by_name (abfd, ".scommon");

      if (scomm == NULL)
	{
	  scomm = bfd_make_section (abfd, ".scommon");
	  if (scomm == NULL
	      || !bfd_set_section_flags (abfd, scomm, (SEC_ALLOC
						       | SEC_IS_COMMON
						       | SEC_LINKER_CREATED)))
	    return FALSE;
	}

      *secp = scomm;
      *valp = sym->st_size;
    }

  return TRUE;
}
/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
elf32_frv_check_relocs (abfd, info, sec, relocs)
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
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof(Elf32_External_Sym);
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
        case R_FRV_GNU_VTINHERIT:
          if (!_bfd_elf32_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

        /* This relocation describes which C++ vtable entries are actually
           used.  Record for later use during GC.  */
        case R_FRV_GNU_VTENTRY:
          if (!_bfd_elf32_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return FALSE;
          break;
        }
    }

  return TRUE;
}


/* Return the machine subcode from the ELF e_flags header.  */

static int
elf32_frv_machine (abfd)
     bfd *abfd;
{
  switch (elf_elfheader (abfd)->e_flags & EF_FRV_CPU_MASK)
    {
    default:		    break;
    case EF_FRV_CPU_FR500:  return bfd_mach_fr500;
    case EF_FRV_CPU_FR400:  return bfd_mach_fr400;
    case EF_FRV_CPU_FR300:  return bfd_mach_fr300;
    case EF_FRV_CPU_SIMPLE: return bfd_mach_frvsimple;
    case EF_FRV_CPU_TOMCAT: return bfd_mach_frvtomcat;
    }

  return bfd_mach_frv;
}

/* Set the right machine number for a FRV ELF file.  */

static bfd_boolean
elf32_frv_object_p (abfd)
     bfd *abfd;
{
  bfd_default_set_arch_mach (abfd, bfd_arch_frv, elf32_frv_machine (abfd));
  return TRUE;
}

/* Function to set the ELF flag bits.  */

static bfd_boolean
frv_elf_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

/* Copy backend specific data from one object module to another.  */

static bfd_boolean
frv_elf_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  BFD_ASSERT (!elf_flags_init (obfd)
	      || elf_elfheader (obfd)->e_flags == elf_elfheader (ibfd)->e_flags);

  elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;
  elf_flags_init (obfd) = TRUE;
  return TRUE;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static bfd_boolean
frv_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword old_flags, old_partial;
  flagword new_flags, new_partial;
  bfd_boolean error = FALSE;
  char new_opt[80];
  char old_opt[80];

  new_opt[0] = old_opt[0] = '\0';
  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;

#ifdef DEBUG
  (*_bfd_error_handler) ("old_flags = 0x%.8lx, new_flags = 0x%.8lx, init = %s, filename = %s",
			 old_flags, new_flags, elf_flags_init (obfd) ? "yes" : "no",
			 bfd_get_filename (ibfd));
#endif

  if (!elf_flags_init (obfd))			/* First call, no flags set.  */
    {
      elf_flags_init (obfd) = TRUE;
      old_flags = new_flags;
    }

  else if (new_flags == old_flags)		/* Compatible flags are ok.  */
    ;

  else						/* Possibly incompatible flags.  */
    {
      /* Warn if different # of gprs are used.  Note, 0 means nothing is
         said about the size of gprs.  */
      new_partial = (new_flags & EF_FRV_GPR_MASK);
      old_partial = (old_flags & EF_FRV_GPR_MASK);
      if (new_partial == old_partial)
	;

      else if (new_partial == 0)
	;

      else if (old_partial == 0)
	old_flags |= new_partial;

      else
	{
	  switch (new_partial)
	    {
	    default:		strcat (new_opt, " -mgpr-??"); break;
	    case EF_FRV_GPR_32: strcat (new_opt, " -mgpr-32"); break;
	    case EF_FRV_GPR_64: strcat (new_opt, " -mgpr-64"); break;
	    }

	  switch (old_partial)
	    {
	    default:		strcat (old_opt, " -mgpr-??"); break;
	    case EF_FRV_GPR_32: strcat (old_opt, " -mgpr-32"); break;
	    case EF_FRV_GPR_64: strcat (old_opt, " -mgpr-64"); break;
	    }
	}

      /* Warn if different # of fprs are used.  Note, 0 means nothing is
         said about the size of fprs.  */
      new_partial = (new_flags & EF_FRV_FPR_MASK);
      old_partial = (old_flags & EF_FRV_FPR_MASK);
      if (new_partial == old_partial)
	;

      else if (new_partial == 0)
	;

      else if (old_partial == 0)
	old_flags |= new_partial;

      else
	{
	  switch (new_partial)
	    {
	    default:		  strcat (new_opt, " -mfpr-?");      break;
	    case EF_FRV_FPR_32:   strcat (new_opt, " -mfpr-32");     break;
	    case EF_FRV_FPR_64:   strcat (new_opt, " -mfpr-64");     break;
	    case EF_FRV_FPR_NONE: strcat (new_opt, " -msoft-float"); break;
	    }

	  switch (old_partial)
	    {
	    default:		  strcat (old_opt, " -mfpr-?");      break;
	    case EF_FRV_FPR_32:   strcat (old_opt, " -mfpr-32");     break;
	    case EF_FRV_FPR_64:   strcat (old_opt, " -mfpr-64");     break;
	    case EF_FRV_FPR_NONE: strcat (old_opt, " -msoft-float"); break;
	    }
	}

      /* Warn if different dword support was used.  Note, 0 means nothing is
         said about the dword support.  */
      new_partial = (new_flags & EF_FRV_DWORD_MASK);
      old_partial = (old_flags & EF_FRV_DWORD_MASK);
      if (new_partial == old_partial)
	;

      else if (new_partial == 0)
	;

      else if (old_partial == 0)
	old_flags |= new_partial;

      else
	{
	  switch (new_partial)
	    {
	    default:		   strcat (new_opt, " -mdword-?");  break;
	    case EF_FRV_DWORD_YES: strcat (new_opt, " -mdword");    break;
	    case EF_FRV_DWORD_NO:  strcat (new_opt, " -mno-dword"); break;
	    }

	  switch (old_partial)
	    {
	    default:		   strcat (old_opt, " -mdword-?");  break;
	    case EF_FRV_DWORD_YES: strcat (old_opt, " -mdword");    break;
	    case EF_FRV_DWORD_NO:  strcat (old_opt, " -mno-dword"); break;
	    }
	}

      /* Or in flags that accumulate (ie, if one module uses it, mark that the
	 feature is used.  */
      old_flags |= new_flags & (EF_FRV_DOUBLE
				| EF_FRV_MEDIA
				| EF_FRV_MULADD
				| EF_FRV_NON_PIC_RELOCS);

      /* If any module was compiled without -G0, clear the G0 bit.  */
      old_flags = ((old_flags & ~ EF_FRV_G0)
		   | (old_flags & new_flags & EF_FRV_G0));

      /* If any module was compiled without -mnopack, clear the mnopack bit.  */
      old_flags = ((old_flags & ~ EF_FRV_NOPACK)
		   | (old_flags & new_flags & EF_FRV_NOPACK));

      /* We don't have to do anything if the pic flags are the same, or the new
         module(s) were compiled with -mlibrary-pic.  */
      new_partial = (new_flags & EF_FRV_PIC_FLAGS);
      old_partial = (old_flags & EF_FRV_PIC_FLAGS);
      if ((new_partial == old_partial) || ((new_partial & EF_FRV_LIBPIC) != 0))
	;

      /* If the old module(s) were compiled with -mlibrary-pic, copy in the pic
         flags if any from the new module.  */
      else if ((old_partial & EF_FRV_LIBPIC) != 0)
	old_flags = (old_flags & ~ EF_FRV_PIC_FLAGS) | new_partial;

      /* If we have mixtures of -fpic and -fPIC, or in both bits.  */
      else if (new_partial != 0 && old_partial != 0)
	old_flags |= new_partial;

      /* One module was compiled for pic and the other was not, see if we have
         had any relocations that are not pic-safe.  */
      else
	{
	  if ((old_flags & EF_FRV_NON_PIC_RELOCS) == 0)
	    old_flags |= new_partial;
	  else
	    {
	      old_flags &= ~ EF_FRV_PIC_FLAGS;
#ifndef FRV_NO_PIC_ERROR
	      error = TRUE;
	      (*_bfd_error_handler)
		(_("%s: compiled with %s and linked with modules that use non-pic relocations"),
		 bfd_get_filename (ibfd),
		 (new_flags & EF_FRV_BIGPIC) ? "-fPIC" : "-fpic");
#endif
	    }
	}

      /* Warn if different cpu is used (allow a specific cpu to override
	 the generic cpu).  */
      new_partial = (new_flags & EF_FRV_CPU_MASK);
      old_partial = (old_flags & EF_FRV_CPU_MASK);
      if (new_partial == old_partial)
	;

      else if (new_partial == EF_FRV_CPU_GENERIC)
	;

      else if (old_partial == EF_FRV_CPU_GENERIC)
	old_flags = (old_flags & ~EF_FRV_CPU_MASK) | new_partial;

      else
	{
	  switch (new_partial)
	    {
	    default:		     strcat (new_opt, " -mcpu=?");      break;
	    case EF_FRV_CPU_GENERIC: strcat (new_opt, " -mcpu=frv");    break;
	    case EF_FRV_CPU_SIMPLE:  strcat (new_opt, " -mcpu=simple"); break;
	    case EF_FRV_CPU_FR500:   strcat (new_opt, " -mcpu=fr500");  break;
	    case EF_FRV_CPU_FR400:   strcat (new_opt, " -mcpu=fr400");  break;
	    case EF_FRV_CPU_FR300:   strcat (new_opt, " -mcpu=fr300");  break;
	    case EF_FRV_CPU_TOMCAT:  strcat (new_opt, " -mcpu=tomcat"); break;
	    }

	  switch (old_partial)
	    {
	    default:		     strcat (old_opt, " -mcpu=?");      break;
	    case EF_FRV_CPU_GENERIC: strcat (old_opt, " -mcpu=frv");    break;
	    case EF_FRV_CPU_SIMPLE:  strcat (old_opt, " -mcpu=simple"); break;
	    case EF_FRV_CPU_FR500:   strcat (old_opt, " -mcpu=fr500");  break;
	    case EF_FRV_CPU_FR400:   strcat (old_opt, " -mcpu=fr400");  break;
	    case EF_FRV_CPU_FR300:   strcat (old_opt, " -mcpu=fr300");  break;
	    case EF_FRV_CPU_TOMCAT:  strcat (old_opt, " -mcpu=tomcat"); break;
	    }
	}

      /* Print out any mismatches from above.  */
      if (new_opt[0])
	{
	  error = TRUE;
	  (*_bfd_error_handler)
	    (_("%s: compiled with %s and linked with modules compiled with %s"),
	     bfd_get_filename (ibfd), new_opt, old_opt);
	}

      /* Warn about any other mismatches */
      new_partial = (new_flags & ~ EF_FRV_ALL_FLAGS);
      old_partial = (old_flags & ~ EF_FRV_ALL_FLAGS);
      if (new_partial != old_partial)
	{
	  old_flags |= new_partial;
	  error = TRUE;
	  (*_bfd_error_handler)
	    (_("%s: uses different unknown e_flags (0x%lx) fields than previous modules (0x%lx)"),
	     bfd_get_filename (ibfd), (long)new_partial, (long)old_partial);
	}
    }

  /* If the cpu is -mcpu=simple, then set the -mnopack bit.  */
  if ((old_flags & EF_FRV_CPU_MASK) == EF_FRV_CPU_SIMPLE)
    old_flags |= EF_FRV_NOPACK;

  /* Update the old flags now with changes made above.  */
  old_partial = elf_elfheader (obfd)->e_flags & EF_FRV_CPU_MASK;
  elf_elfheader (obfd)->e_flags = old_flags;
  if (old_partial != (old_flags & EF_FRV_CPU_MASK))
    bfd_default_set_arch_mach (obfd, bfd_arch_frv, elf32_frv_machine (obfd));

  if (error)
    bfd_set_error (bfd_error_bad_value);

  return !error;
}


bfd_boolean
frv_elf_print_private_bfd_data (abfd, ptr)
     bfd *abfd;
     PTR ptr;
{
  FILE *file = (FILE *) ptr;
  flagword flags;

  BFD_ASSERT (abfd != NULL && ptr != NULL);

  /* Print normal ELF private data.  */
  _bfd_elf_print_private_bfd_data (abfd, ptr);

  flags = elf_elfheader (abfd)->e_flags;
  fprintf (file, _("private flags = 0x%lx:"), (long)flags);

  switch (flags & EF_FRV_CPU_MASK)
    {
    default:							break;
    case EF_FRV_CPU_SIMPLE: fprintf (file, " -mcpu=simple");	break;
    case EF_FRV_CPU_FR500:  fprintf (file, " -mcpu=fr500");	break;
    case EF_FRV_CPU_FR400:  fprintf (file, " -mcpu=fr400");	break;
    case EF_FRV_CPU_FR300:  fprintf (file, " -mcpu=fr300");	break;
    case EF_FRV_CPU_TOMCAT: fprintf (file, " -mcpu=tomcat");	break;
    }

  switch (flags & EF_FRV_GPR_MASK)
    {
    default:							break;
    case EF_FRV_GPR_32: fprintf (file, " -mgpr-32");		break;
    case EF_FRV_GPR_64: fprintf (file, " -mgpr-64");		break;
    }

  switch (flags & EF_FRV_FPR_MASK)
    {
    default:							break;
    case EF_FRV_FPR_32:   fprintf (file, " -mfpr-32");		break;
    case EF_FRV_FPR_64:   fprintf (file, " -mfpr-64");		break;
    case EF_FRV_FPR_NONE: fprintf (file, " -msoft-float");	break;
    }

  switch (flags & EF_FRV_DWORD_MASK)
    {
    default:							break;
    case EF_FRV_DWORD_YES: fprintf (file, " -mdword");		break;
    case EF_FRV_DWORD_NO:  fprintf (file, " -mno-dword");	break;
    }

  if (flags & EF_FRV_DOUBLE)
    fprintf (file, " -mdouble");

  if (flags & EF_FRV_MEDIA)
    fprintf (file, " -mmedia");

  if (flags & EF_FRV_MULADD)
    fprintf (file, " -mmuladd");

  if (flags & EF_FRV_PIC)
    fprintf (file, " -fpic");

  if (flags & EF_FRV_BIGPIC)
    fprintf (file, " -fPIC");

  if (flags & EF_FRV_NON_PIC_RELOCS)
    fprintf (file, " non-pic relocations");

  if (flags & EF_FRV_G0)
    fprintf (file, " -G0");

  fputc ('\n', file);
  return TRUE;
}


#define ELF_ARCH		bfd_arch_frv
#define ELF_MACHINE_CODE	EM_CYGNUS_FRV
#define ELF_MAXPAGESIZE		0x1000

#define TARGET_BIG_SYM          bfd_elf32_frv_vec
#define TARGET_BIG_NAME		"elf32-frv"

#define elf_info_to_howto_rel			NULL
#define elf_info_to_howto			frv_info_to_howto_rela
#define elf_backend_relocate_section		elf32_frv_relocate_section
#define elf_backend_gc_mark_hook		elf32_frv_gc_mark_hook
#define elf_backend_gc_sweep_hook		elf32_frv_gc_sweep_hook
#define elf_backend_check_relocs                elf32_frv_check_relocs
#define elf_backend_object_p			elf32_frv_object_p
#define elf_backend_add_symbol_hook             elf32_frv_add_symbol_hook

#define elf_backend_can_gc_sections		1
#define elf_backend_rela_normal			1

#define bfd_elf32_bfd_reloc_type_lookup		frv_reloc_type_lookup
#define bfd_elf32_bfd_set_private_flags		frv_elf_set_private_flags
#define bfd_elf32_bfd_copy_private_bfd_data	frv_elf_copy_private_bfd_data
#define bfd_elf32_bfd_merge_private_bfd_data	frv_elf_merge_private_bfd_data
#define bfd_elf32_bfd_print_private_bfd_data	frv_elf_print_private_bfd_data

#include "elf32-target.h"
