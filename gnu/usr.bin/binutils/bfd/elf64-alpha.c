/* ALPHA-specific support for 64-bit ELF
   Copyright 1996 Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@tamu.edu>.

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

/* We need a published ABI spec for this.  Until one comes out, don't
   assume this'll remain unchanged forever.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"

#include "elf/alpha.h"

#define ALPHAECOFF

#define NO_COFF_RELOCS
#define NO_COFF_SYMBOLS
#define NO_COFF_LINENOS

/* Get the ECOFF swapping routines.  Needed for the debug information. */
#include "coff/internal.h"
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/ecoff.h"
#include "coff/alpha.h"
#include "aout/ar.h"
#include "libcoff.h"
#include "libecoff.h"
#define ECOFF_64
#include "ecoffswap.h"

static struct bfd_hash_entry * elf64_alpha_link_hash_newfunc
  PARAMS((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static struct bfd_link_hash_table * elf64_alpha_bfd_link_hash_table_create
  PARAMS((bfd *));

static bfd_reloc_status_type elf64_alpha_reloc_nil
  PARAMS((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type elf64_alpha_do_reloc_gpdisp
  PARAMS((bfd *, bfd_vma, bfd_byte *, bfd_byte *));
static bfd_reloc_status_type elf64_alpha_reloc_gpdisp
  PARAMS((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type elf64_alpha_reloc_op_push
  PARAMS((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type elf64_alpha_reloc_op_store
  PARAMS((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type elf64_alpha_reloc_op_psub
  PARAMS((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type elf64_alpha_reloc_op_prshift
  PARAMS((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

static reloc_howto_type * elf64_alpha_bfd_reloc_type_lookup
  PARAMS((bfd *, bfd_reloc_code_real_type));
static void elf64_alpha_info_to_howto
  PARAMS((bfd *, arelent *, Elf64_Internal_Rela *));

static boolean elf64_alpha_object_p
  PARAMS((bfd *));
static boolean elf64_alpha_section_from_shdr
  PARAMS((bfd *, Elf64_Internal_Shdr *, char *));
static boolean elf64_alpha_fake_sections
  PARAMS((bfd *, Elf64_Internal_Shdr *, asection *));
static int elf64_alpha_additional_program_headers
  PARAMS((bfd *));
static boolean elf64_alpha_create_got_section
  PARAMS((bfd *, struct bfd_link_info *));
static boolean elf64_alpha_create_dynamic_sections
  PARAMS((bfd *, struct bfd_link_info *));

static boolean elf64_alpha_read_ecoff_info
  PARAMS((bfd *, asection *, struct ecoff_debug_info *));
static boolean elf64_alpha_is_local_label
  PARAMS((bfd *, asymbol *));
static boolean elf64_alpha_find_nearest_line
  PARAMS((bfd *, asection *, asymbol **, bfd_vma, const char **,
	  const char **, unsigned int *));

#if defined(__STDC__) || defined(ALMOST_STDC)
struct alpha_elf_link_hash_entry;
#endif

static boolean elf64_alpha_output_extsym
  PARAMS((struct alpha_elf_link_hash_entry *, PTR));

static boolean elf64_alpha_check_relocs
  PARAMS((bfd *, struct bfd_link_info *, asection *sec,
	  const Elf_Internal_Rela *));
static boolean elf64_alpha_adjust_dynamic_symbol
  PARAMS((struct bfd_link_info *, struct elf_link_hash_entry *));
static boolean elf64_alpha_size_dynamic_sections
  PARAMS((bfd *, struct bfd_link_info *));
static boolean elf64_alpha_adjust_dynindx
  PARAMS((struct elf_link_hash_entry *, PTR));
static boolean elf64_alpha_relocate_section
  PARAMS((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	  Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static boolean elf64_alpha_finish_dynamic_symbol
  PARAMS((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	  Elf_Internal_Sym *));
static boolean elf64_alpha_finish_dynamic_sections
  PARAMS((bfd *, struct bfd_link_info *));
static boolean elf64_alpha_final_link
  PARAMS((bfd *, struct bfd_link_info *));


#define alpha_elf_tdata(bfd) \
	((struct alpha_elf_obj_tdata *)elf_tdata(bfd)->tdata)

struct alpha_elf_link_hash_entry
{
  struct elf_link_hash_entry root;

  /* External symbol information.  */
  EXTR esym;

  unsigned char flags;
  /* Contexts (LITUSE) in which a literal was referenced.  */
#define ALPHA_ELF_LINK_HASH_LU_ADDR 01
#define ALPHA_ELF_LINK_HASH_LU_MEM 02
#define ALPHA_ELF_LINK_HASH_LU_FUNC 04
};

/* Alpha ELF linker hash table.  */

struct alpha_elf_link_hash_table
{
  struct elf_link_hash_table root;
};

/* Look up an entry in a Alpha ELF linker hash table.  */

#define alpha_elf_link_hash_lookup(table, string, create, copy, follow)	\
  ((struct alpha_elf_link_hash_entry *)					\
   elf_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

/* Traverse a Alpha ELF linker hash table.  */

#define alpha_elf_link_hash_traverse(table, func, info)			\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the Alpha ELF linker hash table from a link_info structure.  */

#define alpha_elf_hash_table(p) \
  ((struct alpha_elf_link_hash_table *) ((p)->hash))

/* Create an entry in a Alpha ELF linker hash table.  */

static struct bfd_hash_entry *
elf64_alpha_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct alpha_elf_link_hash_entry *ret =
    (struct alpha_elf_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct alpha_elf_link_hash_entry *) NULL)
    ret = ((struct alpha_elf_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct alpha_elf_link_hash_entry)));
  if (ret == (struct alpha_elf_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct alpha_elf_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != (struct alpha_elf_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      memset (&ret->esym, 0, sizeof (EXTR));
      /* We use -2 as a marker to indicate that the information has
	 not been set.  -1 means there is no associated ifd.  */
      ret->esym.ifd = -2;
      ret->flags = 0;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create a Alpha ELF linker hash table.  */

static struct bfd_link_hash_table *
elf64_alpha_bfd_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct alpha_elf_link_hash_table *ret;

  ret = ((struct alpha_elf_link_hash_table *)
	 bfd_zalloc (abfd, sizeof (struct alpha_elf_link_hash_table)));
  if (ret == (struct alpha_elf_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
				       elf64_alpha_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  return &ret->root.root;
}


/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

static reloc_howto_type elf64_alpha_howto_table[] =
{
  HOWTO (R_ALPHA_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_nil,	/* special_function */
	 "NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 32 bit reference to a symbol.  */
  HOWTO (R_ALPHA_REFLONG,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "REFLONG",		/* name */
	 false,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit reference to a symbol.  */
  HOWTO (R_ALPHA_REFQUAD,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "REFQUAD",		/* name */
	 false,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 32 bit GP relative offset.  This is just like REFLONG except
     that when the value is used the value of the gp register will be
     added in.  */
  HOWTO (R_ALPHA_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "GPREL32",		/* name */
	 false,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Used for an instruction that refers to memory off the GP register.  */
  HOWTO (R_ALPHA_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "ELF_LITERAL",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* This reloc only appears immediately following an ELF_LITERAL reloc.
     It identifies a use of the literal.  The symbol index is special:
     1 means the literal address is in the base register of a memory
     format instruction; 2 means the literal address is in the byte
     offset register of a byte-manipulation instruction; 3 means the
     literal address is in the target register of a jsr instruction.
     This does not actually do any relocation.  */
  HOWTO (R_ALPHA_LITUSE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_nil,	/* special_function */
	 "LITUSE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Load the gp register.  This is always used for a ldah instruction
     which loads the upper 16 bits of the gp register.  The symbol
     index of the GPDISP instruction is an offset in bytes to the lda
     instruction that loads the lower 16 bits.  The value to use for
     the relocation is the difference between the GP value and the
     current location; the load will always be done against a register
     holding the current address.

     NOTE: Unlike ECOFF, partial inplace relocation is not done.  If
     any offset is present in the instructions, it is an offset from
     the register to the ldah instruction.  This lets us avoid any
     stupid hackery like inventing a gp value to do partial relocation
     against.  Also unlike ECOFF, we do the whole relocation off of
     the GPDISP rather than a GPDISP_HI16/GPDISP_LO16 pair.  An odd,
     space consuming bit, that, since all the information was present
     in the GPDISP_HI16 reloc.  */
  HOWTO (R_ALPHA_GPDISP,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_gpdisp, /* special_function */
	 "GPDISP",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 21 bit branch.  */
  HOWTO (R_ALPHA_BRADDR,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 21,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "BRADDR",		/* name */
	 false,			/* partial_inplace */
	 0x1fffff,		/* src_mask */
	 0x1fffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A hint for a jump to a register.  */
  HOWTO (R_ALPHA_HINT,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 14,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "HINT",		/* name */
	 false,			/* partial_inplace */
	 0x3fff,		/* src_mask */
	 0x3fff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16 bit PC relative offset.  */
  HOWTO (R_ALPHA_SREL16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL16",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit PC relative offset.  */
  HOWTO (R_ALPHA_SREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL32",		/* name */
	 false,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit PC relative offset.  */
  HOWTO (R_ALPHA_SREL64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL64",		/* name */
	 false,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Push a value on the reloc evaluation stack.  */
  HOWTO (ALPHA_R_OP_PUSH,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_op_push, /* special_function */
	 "OP_PUSH",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Store the value from the stack at the given address.  Store it in
     a bitfield of size r_size starting at bit position r_offset.  */
  HOWTO (ALPHA_R_OP_STORE,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_op_store, /* special_function */
	 "OP_STORE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Subtract the reloc address from the value on the top of the
     relocation stack.  */
  HOWTO (ALPHA_R_OP_PSUB,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_op_psub, /* special_function */
	 "OP_PSUB",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Shift the value on the top of the relocation stack right by the
     given value.  */
  HOWTO (ALPHA_R_OP_PRSHIFT,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_op_prshift, /* special_function */
	 "OP_PRSHIFT",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Misc ELF relocations. */
  HOWTO (R_ALPHA_COPY,
	 0,
	 0,
	 0,
	 false,
	 0,
	 complain_overflow_dont,
	 bfd_elf_generic_reloc,
	 "COPY",
	 false,
	 0,
	 0,
	 true),

  HOWTO (R_ALPHA_GLOB_DAT,
	 0,
	 0,
	 0,
	 false,
	 0,
	 complain_overflow_dont,
	 bfd_elf_generic_reloc,
	 "GLOB_DAT",
	 false,
	 0,
	 0,
	 true),

  HOWTO (R_ALPHA_JMP_SLOT,
	 0,
	 0,
	 0,
	 false,
	 0,
	 complain_overflow_dont,
	 bfd_elf_generic_reloc,
	 "JMP_SLOT",
	 false,
	 0,
	 0,
	 true),

  HOWTO (R_ALPHA_RELATIVE,
	 0,
	 0,
	 0,
	 false,
	 0,
	 complain_overflow_dont,
	 bfd_elf_generic_reloc,
	 "RELATIVE",
	 false,
	 0,
	 0,
	 true)
};

static bfd_reloc_status_type
elf64_alpha_reloc_nil (abfd, reloc, sym, data, sec, output_bfd, error_message)
     bfd *abfd;
     arelent *reloc;
     asymbol *sym;
     PTR data;
     asection *sec;
     bfd *output_bfd;
     char **error_message;
{
  if (output_bfd)
    reloc->address += sec->output_offset;
  return bfd_reloc_ok;
}

static bfd_reloc_status_type
elf64_alpha_do_reloc_gpdisp (abfd, gpdisp, p_ldah, p_lda)
     bfd *abfd;
     bfd_vma gpdisp;
     bfd_byte *p_ldah, *p_lda;
{
  bfd_reloc_status_type ret = bfd_reloc_ok;
  bfd_vma addend;
  unsigned long i_ldah, i_lda;

  i_ldah = bfd_get_32(abfd, p_ldah);
  i_lda = bfd_get_32(abfd, p_lda);

  /* Complain if the instructions are not correct.  */
  if (((i_ldah >> 26) & 0x3f) != 0x09
      || ((i_lda >> 26) & 0x3f) != 0x08)
    ret = bfd_reloc_dangerous;

  /* Extract the user-supplied offset, mirroring the sign extensions
     that the instructions perform.  */
  addend = ((i_ldah & 0xffff) << 16) | (i_lda & 0xffff);
  addend = (addend ^ 0x80008000) - 0x80008000;

  gpdisp += addend;

  if ((bfd_signed_vma)gpdisp < -(bfd_signed_vma)0x80000000
      || gpdisp >= 0x7fff8000)
    ret = bfd_reloc_overflow;

  /* compensate for the sign extension again.  */
  i_ldah = ((i_ldah & 0xffff0000)
	    | (((gpdisp >> 16) + ((gpdisp >> 15) & 1)) & 0xffff));
  i_lda = (i_lda & 0xffff0000) | (gpdisp & 0xffff);

  bfd_put_32 (abfd, i_ldah, p_ldah);
  bfd_put_32 (abfd, i_lda, p_lda);

  return ret;
}

static bfd_reloc_status_type
elf64_alpha_reloc_gpdisp (abfd, reloc_entry, sym, data, input_section,
			  output_bfd, err_msg)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *sym;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **err_msg;
{
  bfd_reloc_status_type ret;
  bfd_vma gp, relocation;
  bfd_byte *p_ldah, *p_lda;

  /* Don't do anything if we're not doing a final link.  */
  if (output_bfd)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (reloc_entry->address > input_section->_cooked_size ||
      reloc_entry->address + reloc_entry->addend > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* The gp used in the portion of the output object to which this
     input object belongs is cached on the input bfd.  */
  gp = _bfd_get_gp_value (abfd);

  relocation = (input_section->output_section->vma
		+ input_section->output_offset
		+ reloc_entry->address);

  p_ldah = (bfd_byte *)data + reloc_entry->address;
  p_lda = p_ldah + reloc_entry->addend;

  ret = elf64_alpha_do_reloc_gpdisp (abfd, gp - relocation, p_ldah, p_lda);

  /* Complain if the instructions are not correct.  */
  if (ret == bfd_reloc_dangerous)
    {
      *err_msg = "GPDISP relocation did not find ldah and lda instructions";
    }

  return ret;
}

/* Due to the nature of the stack operations, I don't think more
   that one entry is useful.  Test this theory by setting the
   stack size to a minimum.  */
/* FIXME: BFD should not use static variables.  */
#define OP_STACK_SIZE 1
static bfd_vma elf64_alpha_op_stack[OP_STACK_SIZE];
static int elf64_alpha_op_tos;

static bfd_reloc_status_type
elf64_alpha_reloc_op_push (abfd, reloc_entry, sym, data, input_section,
			   output_bfd, err_msg)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *sym;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **err_msg;
{
  bfd_reloc_status_type r = bfd_reloc_ok;
  bfd_vma value;

  /* Don't do anything if we're not doing a final link.  */
  if (output_bfd)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (elf64_alpha_op_tos >= OP_STACK_SIZE)
    {
      *err_msg = "operation stack overflow";
      return bfd_reloc_dangerous;
    }

  /* Get the symbol value.  */
  /* FIXME: We should fail if this is a dynamic symbol.  Check on that.  */
  if (bfd_is_und_section (sym->section))
    r = bfd_reloc_undefined;
  if (bfd_is_com_section (sym->section))
    value = 0;
  else
    value = sym->value;
  value += sym->section->output_section->vma;
  value += sym->section->output_offset;
  value += reloc_entry->addend;

  elf64_alpha_op_stack[elf64_alpha_op_tos++] = value;

  return r;
}

static bfd_reloc_status_type
elf64_alpha_reloc_op_store (abfd, reloc_entry, sym, data, input_section,
			    output_bfd, err_msg)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *sym;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **err_msg;
{
  int size, offset;
  bfd_vma value;

  /* Don't do anything before the final link.  */
  if (output_bfd)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (elf64_alpha_op_tos <= 0)
    {
      *err_msg = "operation stack underflow";
      return bfd_reloc_dangerous;
    }

  /* The offset and size for this reloc are encoded into the addend
     field by alpha_adjust_reloc_in.  */
  offset = (reloc_entry->addend >> 8) & 0xff;
  size = reloc_entry->addend & 0xff;

  value = bfd_get_64 (abfd, data + reloc_entry->address);
  value &= ~((((bfd_vma)1 << size) - 1) << offset);
  value |= (elf64_alpha_op_stack[--elf64_alpha_op_tos]
	    & (((bfd_vma)1 << size) - 1)) << offset;
  bfd_put_64 (abfd, value, data + reloc_entry->address);

  return bfd_reloc_ok;
}

static bfd_reloc_status_type
elf64_alpha_reloc_op_psub (abfd, reloc_entry, sym, data, input_section,
			   output_bfd, err_msg)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *sym;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **err_msg;
{
  bfd_reloc_status_type r;
  bfd_vma value;

  /* Don't do anything before the final link.  */
  if (output_bfd)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (elf64_alpha_op_tos <= 0)
    {
      *err_msg = "operation stack underflow";
      return bfd_reloc_dangerous;
    }

  if (bfd_is_und_section (sym->section))
    r = bfd_reloc_undefined;
  if (bfd_is_com_section (sym->section))
    value = 0;
  else
    value = sym->value;
  value += sym->section->output_section->vma;
  value += sym->section->output_offset;
  value += reloc_entry->addend;

  elf64_alpha_op_stack[elf64_alpha_op_tos-1] -= value;

  return r;
}

static bfd_reloc_status_type
elf64_alpha_reloc_op_prshift (abfd, reloc_entry, sym, data, input_section,
			      output_bfd, err_msg)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *sym;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **err_msg;
{
  /* Don't do anything before the final link.  */
  if (output_bfd)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (elf64_alpha_op_tos <= 0)
    {
      *err_msg = "operation stack underflow";
      return bfd_reloc_dangerous;
    }

  elf64_alpha_op_stack[elf64_alpha_op_tos-1] >>= reloc_entry->addend;

  return bfd_reloc_ok;
}

/* A mapping from BFD reloc types to Alpha ELF reloc types.  */

struct elf_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  int elf_reloc_val;
};

static const struct elf_reloc_map elf64_alpha_reloc_map[] =
{
  {BFD_RELOC_NONE,		R_ALPHA_NONE},
  {BFD_RELOC_32,		R_ALPHA_REFLONG},
  {BFD_RELOC_64,		R_ALPHA_REFQUAD},
  {BFD_RELOC_CTOR,		R_ALPHA_REFQUAD},
  {BFD_RELOC_GPREL32,		R_ALPHA_GPREL32},
  {BFD_RELOC_ALPHA_ELF_LITERAL,	R_ALPHA_LITERAL},
  {BFD_RELOC_ALPHA_LITUSE,	R_ALPHA_LITUSE},
  {BFD_RELOC_ALPHA_GPDISP,	R_ALPHA_GPDISP},
  {BFD_RELOC_23_PCREL_S2,	R_ALPHA_BRADDR},
  {BFD_RELOC_ALPHA_HINT,	R_ALPHA_HINT},
  {BFD_RELOC_16_PCREL,		R_ALPHA_SREL16},
  {BFD_RELOC_32_PCREL,		R_ALPHA_SREL32},
  {BFD_RELOC_64_PCREL,		R_ALPHA_SREL64},
#if 0
  {BFD_RELOC_ALPHA_OP_PUSH,	R_ALPHA_OP_PUSH},
  {BFD_RELOC_ALPHA_OP_STORE,	R_ALPHA_OP_STORE},
  {BFD_RELOC_ALPHA_OP_PSUB,	R_ALPHA_OP_PSUB},
  {BFD_RELOC_ALPHA_OP_PRSHIFT,	R_ALPHA_OP_PRSHIFT}
#endif
};

/* Given a BFD reloc type, return a HOWTO structure.  */

static reloc_howto_type *
elf64_alpha_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  const struct elf_reloc_map *i, *e;
  i = e = elf64_alpha_reloc_map;
  e += sizeof (elf64_alpha_reloc_map) / sizeof (struct elf_reloc_map);
  for (; i != e; ++i)
    {
      if (i->bfd_reloc_val == code)
	return &elf64_alpha_howto_table[i->elf_reloc_val];
    }
  return 0;
}

/* Given an Alpha ELF reloc type, fill in an arelent structure.  */

static void
elf64_alpha_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf64_Internal_Rela *dst;
{
  unsigned r_type;

  r_type = ELF64_R_TYPE(dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_ALPHA_max);
  cache_ptr->howto = &elf64_alpha_howto_table[r_type];
}

/* PLT/GOT Stuff */
#define PLT_HEADER_SIZE 32
#define PLT_HEADER_WORD1	0xc3600000	/* br   $27,.+4     */
#define PLT_HEADER_WORD2	0xa77b000c	/* ldq  $27,12($27) */
#define PLT_HEADER_WORD3	0x47ff041f	/* nop              */
#define PLT_HEADER_WORD4	0x6b7b0000	/* jmp  $27,($27)   */

#define PLT_ENTRY_SIZE 12
#define PLT_ENTRY_WORD1		0x279f0000	/* ldah $28, 0($31) */
#define PLT_ENTRY_WORD2		0x239c0000	/* lda  $28, 0($28) */
#define PLT_ENTRY_WORD3		0xc3e00000	/* br   $31, plt0   */

#define RESERVED_GOT_ENTRIES 1

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/ld.so"

/* Set the right machine number for an Alpha ELF file.  */

static boolean
elf64_alpha_object_p (abfd)
     bfd *abfd;
{
  return bfd_default_set_arch_mach (abfd, bfd_arch_alpha, 0);
}

/* Handle a alpha specific section when reading an object file.  This
   is called when elfcode.h finds a section with an unknown type.
   FIXME: We need to handle the SHF_MIPS_GPREL flag, but I'm not sure
   how to.  */

static boolean
elf64_alpha_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf64_Internal_Shdr *hdr;
     char *name;
{
  asection *newsect;

  /* There ought to be a place to keep ELF backend specific flags, but
     at the moment there isn't one.  We just keep track of the
     sections by their name, instead.  Fortunately, the ABI gives
     suggested names for all the MIPS specific sections, so we will
     probably get away with this.  */
  switch (hdr->sh_type)
    {
    case SHT_ALPHA_DEBUG:
      if (strcmp (name, ".mdebug") != 0)
	return false;
      break;
#ifdef ERIC_neverdef
    case SHT_ALPHA_REGINFO:
      if (strcmp (name, ".reginfo") != 0
	  || hdr->sh_size != sizeof (Elf64_External_RegInfo))
	return false;
      break;
#endif
    default:
      return false;
    }

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
    return false;
  newsect = hdr->bfd_section;

  if (hdr->sh_type == SHT_ALPHA_DEBUG)
    {
      if (! bfd_set_section_flags (abfd, newsect,
				   (bfd_get_section_flags (abfd, newsect)
				    | SEC_DEBUGGING)))
	return false;
    }

#ifdef ERIC_neverdef
  /* For a .reginfo section, set the gp value in the tdata information
     from the contents of this section.  We need the gp value while
     processing relocs, so we just get it now.  */
  if (hdr->sh_type == SHT_ALPHA_REGINFO)
    {
      Elf64_External_RegInfo ext;
      Elf64_RegInfo s;

      if (! bfd_get_section_contents (abfd, newsect, (PTR) &ext,
				      (file_ptr) 0, sizeof ext))
	return false;
      bfd_alpha_elf64_swap_reginfo_in (abfd, &ext, &s);
      elf_gp (abfd) = s.ri_gp_value;
    }
#endif

  return true;
}

/* Set the correct type for an Alpha ELF section.  We do this by the
   section name, which is a hack, but ought to work.  */

static boolean
elf64_alpha_fake_sections (abfd, hdr, sec)
     bfd *abfd;
     Elf64_Internal_Shdr *hdr;
     asection *sec;
{
  register const char *name;

  name = bfd_get_section_name (abfd, sec);

  if (strcmp (name, ".mdebug") == 0)
    {
      hdr->sh_type = SHT_ALPHA_DEBUG;
      /* In a shared object on Irix 5.3, the .mdebug section has an
         entsize of 0.  FIXME: Does this matter?  */
      if ((abfd->flags & DYNAMIC) != 0 )
	hdr->sh_entsize = 0;
      else
	hdr->sh_entsize = 1;
    }
#ifdef ERIC_neverdef
  else if (strcmp (name, ".reginfo") == 0)
    {
      hdr->sh_type = SHT_ALPHA_REGINFO;
      /* In a shared object on Irix 5.3, the .reginfo section has an
         entsize of 0x18.  FIXME: Does this matter?  */
      if ((abfd->flags & DYNAMIC) != 0)
	hdr->sh_entsize = sizeof (Elf64_External_RegInfo);
      else
	hdr->sh_entsize = 1;

      /* Force the section size to the correct value, even if the
	 linker thinks it is larger.  The link routine below will only
	 write out this much data for .reginfo.  */
      hdr->sh_size = sec->_raw_size = sizeof (Elf64_External_RegInfo);
    }
  else if (strcmp (name, ".hash") == 0
	   || strcmp (name, ".dynamic") == 0
	   || strcmp (name, ".dynstr") == 0)
    {
      hdr->sh_entsize = 0;
      hdr->sh_info = SIZEOF_ALPHA_DYNSYM_SECNAMES;
    }
  else if (strcmp (name, ".sdata") == 0
	   || strcmp (name, ".sbss") == 0
	   || strcmp (name, ".lit4") == 0
	   || strcmp (name, ".lit8") == 0)
    hdr->sh_flags |= SHF_ALPHA_GPREL;
#endif

  return true;
}

static int
elf64_alpha_additional_program_headers (abfd)
     bfd *abfd;
{
  asection *s;
  int ret;

  ret = 0;

  s = bfd_get_section_by_name (abfd, ".reginfo");
  if (s != NULL && (s->flags & SEC_LOAD) != 0)
    {
      /* We need a PT_ALPHA_REGINFO segment.  */
      ++ret;
    }

  if (bfd_get_section_by_name (abfd, ".dynamic") != NULL
      && bfd_get_section_by_name (abfd, ".mdebug") != NULL)
    {
      /* We need a PT_ALPHA_RTPROC segment.  */
      ++ret;
    }

  return ret;
}

static boolean
elf64_alpha_create_got_section(abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection *s;
  struct elf_link_hash_entry *h;

  if (bfd_get_section_by_name (abfd, ".got"))
    return true;

  s = bfd_make_section(abfd, ".rela.got");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, (SEC_ALLOC | SEC_LOAD
					   | SEC_HAS_CONTENTS | SEC_IN_MEMORY
					   | SEC_READONLY))
      || !bfd_set_section_alignment (abfd, s, 3))
    return false;

  s = bfd_make_section(abfd, ".got");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, (SEC_ALLOC | SEC_LOAD
					   | SEC_HAS_CONTENTS | SEC_IN_MEMORY))
      || !bfd_set_section_alignment (abfd, s, 3))
    return false;

  s->_raw_size = RESERVED_GOT_ENTRIES * 8;

  /* Define the symbol _GLOBAL_OFFSET_TABLE_ at the start of the .got
     (or .got.plt) section.  We don't do this in the linker script
     because we don't want to define the symbol if we are not creating
     a global offset table.  */
  h = NULL;
  if (!(_bfd_generic_link_add_one_symbol
	(info, abfd, "_GLOBAL_OFFSET_TABLE_", BSF_GLOBAL, s, (bfd_vma) 0,
	 (const char *) NULL, false, get_elf_backend_data (abfd)->collect,
	 (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (info->shared
      && ! _bfd_elf_link_record_dynamic_symbol (info, h))
    return false;

  elf_hash_table (info)->hgot = h;

  return true;
}

static boolean
elf64_alpha_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  register asection *s;
  struct elf_link_hash_entry *h;

  /* We need to create .plt, .rela.plt, .got, and .rela.got sections.  */

  s = bfd_make_section (abfd, ".plt");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, (SEC_ALLOC | SEC_LOAD
					    | SEC_HAS_CONTENTS | SEC_IN_MEMORY
					    | SEC_CODE))
      || ! bfd_set_section_alignment (abfd, s, 3))
    return false;

  /* Define the symbol _PROCEDURE_LINKAGE_TABLE_ at the start of the
     .plt section.  */
  h = NULL;
  if (! (_bfd_generic_link_add_one_symbol
	 (info, abfd, "_PROCEDURE_LINKAGE_TABLE_", BSF_GLOBAL, s,
	  (bfd_vma) 0, (const char *) NULL, false,
	  get_elf_backend_data (abfd)->collect,
	  (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (info->shared
      && ! _bfd_elf_link_record_dynamic_symbol (info, h))
    return false;

  s = bfd_make_section (abfd, ".rela.plt");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, (SEC_ALLOC | SEC_LOAD
					  | SEC_HAS_CONTENTS | SEC_IN_MEMORY
					  | SEC_READONLY))
      || ! bfd_set_section_alignment (abfd, s, 3))
    return false;

  if (!elf64_alpha_create_got_section (abfd, info))
    return false;

  return true;
}

/* The structure of the runtile procedure descriptor created by the
   loader for use by the static exception system.  */

/* FIXME */

/* Read ECOFF debugging information from a .mdebug section into a
   ecoff_debug_info structure.  */

static boolean
elf64_alpha_read_ecoff_info (abfd, section, debug)
     bfd *abfd;
     asection *section;
     struct ecoff_debug_info *debug;
{
  HDRR *symhdr;
  const struct ecoff_debug_swap *swap;
  char *ext_hdr = NULL;

  swap = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;

  ext_hdr = (char *) bfd_malloc ((size_t) swap->external_hdr_size);
  if (ext_hdr == NULL && swap->external_hdr_size != 0)
    goto error_return;

  if (bfd_get_section_contents (abfd, section, ext_hdr, (file_ptr) 0,
				swap->external_hdr_size)
      == false)
    goto error_return;

  symhdr = &debug->symbolic_header;
  (*swap->swap_hdr_in) (abfd, ext_hdr, symhdr);

  /* The symbolic header contains absolute file offsets and sizes to
     read.  */
#define READ(ptr, offset, count, size, type)				\
  if (symhdr->count == 0)						\
    debug->ptr = NULL;							\
  else									\
    {									\
      debug->ptr = (type) bfd_malloc ((size_t) (size * symhdr->count));	\
      if (debug->ptr == NULL)						\
	goto error_return;						\
      if (bfd_seek (abfd, (file_ptr) symhdr->offset, SEEK_SET) != 0	\
	  || (bfd_read (debug->ptr, size, symhdr->count,		\
			abfd) != size * symhdr->count))			\
	goto error_return;						\
    }

  READ (line, cbLineOffset, cbLine, sizeof (unsigned char), unsigned char *);
  READ (external_dnr, cbDnOffset, idnMax, swap->external_dnr_size, PTR);
  READ (external_pdr, cbPdOffset, ipdMax, swap->external_pdr_size, PTR);
  READ (external_sym, cbSymOffset, isymMax, swap->external_sym_size, PTR);
  READ (external_opt, cbOptOffset, ioptMax, swap->external_opt_size, PTR);
  READ (external_aux, cbAuxOffset, iauxMax, sizeof (union aux_ext),
	union aux_ext *);
  READ (ss, cbSsOffset, issMax, sizeof (char), char *);
  READ (ssext, cbSsExtOffset, issExtMax, sizeof (char), char *);
  READ (external_fdr, cbFdOffset, ifdMax, swap->external_fdr_size, PTR);
  READ (external_rfd, cbRfdOffset, crfd, swap->external_rfd_size, PTR);
  READ (external_ext, cbExtOffset, iextMax, swap->external_ext_size, PTR);
#undef READ

  debug->fdr = NULL;
  debug->adjust = NULL;

  return true;

 error_return:
  if (ext_hdr != NULL)
    free (ext_hdr);
  if (debug->line != NULL)
    free (debug->line);
  if (debug->external_dnr != NULL)
    free (debug->external_dnr);
  if (debug->external_pdr != NULL)
    free (debug->external_pdr);
  if (debug->external_sym != NULL)
    free (debug->external_sym);
  if (debug->external_opt != NULL)
    free (debug->external_opt);
  if (debug->external_aux != NULL)
    free (debug->external_aux);
  if (debug->ss != NULL)
    free (debug->ss);
  if (debug->ssext != NULL)
    free (debug->ssext);
  if (debug->external_fdr != NULL)
    free (debug->external_fdr);
  if (debug->external_rfd != NULL)
    free (debug->external_rfd);
  if (debug->external_ext != NULL)
    free (debug->external_ext);
  return false;
}

/* Alpha ELF local labels start with '$'.  */

static boolean
elf64_alpha_is_local_label (abfd, symbol)
     bfd *abfd;
     asymbol *symbol;
{
  return symbol->name[0] == '$';
}

/* Alpha ELF follows MIPS ELF in using a special find_nearest_line
   routine in order to handle the ECOFF debugging information.  We
   still call this mips_elf_find_line because of the slot
   find_line_info in elf_obj_tdata is declared that way.  */

struct mips_elf_find_line
{
  struct ecoff_debug_info d;
  struct ecoff_find_line i;
};

static boolean
elf64_alpha_find_nearest_line (abfd, section, symbols, offset, filename_ptr,
			       functionname_ptr, line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     const char **filename_ptr;
     const char **functionname_ptr;
     unsigned int *line_ptr;
{
  asection *msec;

  msec = bfd_get_section_by_name (abfd, ".mdebug");
  if (msec != NULL)
    {
      flagword origflags;
      struct mips_elf_find_line *fi;
      const struct ecoff_debug_swap * const swap =
	get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;

      /* If we are called during a link, alpha_elf_final_link may have
	 cleared the SEC_HAS_CONTENTS field.  We force it back on here
	 if appropriate (which it normally will be).  */
      origflags = msec->flags;
      if (elf_section_data (msec)->this_hdr.sh_type != SHT_NOBITS)
	msec->flags |= SEC_HAS_CONTENTS;

      fi = elf_tdata (abfd)->find_line_info;
      if (fi == NULL)
	{
	  bfd_size_type external_fdr_size;
	  char *fraw_src;
	  char *fraw_end;
	  struct fdr *fdr_ptr;

	  fi = ((struct mips_elf_find_line *)
		bfd_zalloc (abfd, sizeof (struct mips_elf_find_line)));
	  if (fi == NULL)
	    {
	      msec->flags = origflags;
	      return false;
	    }

	  if (!elf64_alpha_read_ecoff_info (abfd, msec, &fi->d))
	    {
	      msec->flags = origflags;
	      return false;
	    }

	  /* Swap in the FDR information.  */
	  fi->d.fdr = ((struct fdr *)
		       bfd_alloc (abfd,
				  (fi->d.symbolic_header.ifdMax *
				   sizeof (struct fdr))));
	  if (fi->d.fdr == NULL)
	    {
	      msec->flags = origflags;
	      return false;
	    }
	  external_fdr_size = swap->external_fdr_size;
	  fdr_ptr = fi->d.fdr;
	  fraw_src = (char *) fi->d.external_fdr;
	  fraw_end = (fraw_src
		      + fi->d.symbolic_header.ifdMax * external_fdr_size);
	  for (; fraw_src < fraw_end; fraw_src += external_fdr_size, fdr_ptr++)
	    (*swap->swap_fdr_in) (abfd, (PTR) fraw_src, fdr_ptr);

	  elf_tdata (abfd)->find_line_info = fi;

	  /* Note that we don't bother to ever free this information.
             find_nearest_line is either called all the time, as in
             objdump -l, so the information should be saved, or it is
             rarely called, as in ld error messages, so the memory
             wasted is unimportant.  Still, it would probably be a
             good idea for free_cached_info to throw it away.  */
	}

      if (_bfd_ecoff_locate_line (abfd, section, offset, &fi->d, swap,
				  &fi->i, filename_ptr, functionname_ptr,
				  line_ptr))
	{
	  msec->flags = origflags;
	  return true;
	}

      msec->flags = origflags;
    }

  /* Fall back on the generic ELF find_nearest_line routine.  */

  return _bfd_elf_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr,
				     line_ptr);
}

/* Structure used to pass information to alpha_elf_output_extsym.  */

struct extsym_info
{
  bfd *abfd;
  struct bfd_link_info *info;
  struct ecoff_debug_info *debug;
  const struct ecoff_debug_swap *swap;
  boolean failed;
};

static boolean
elf64_alpha_output_extsym (h, data)
     struct alpha_elf_link_hash_entry *h;
     PTR data;
{
  struct extsym_info *einfo = (struct extsym_info *) data;
  boolean strip;
  asection *sec, *output_section;

  if (h->root.indx == -2)
    strip = false;
  else if (((h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
           || (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0)
          && (h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
          && (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    strip = true;
  else if (einfo->info->strip == strip_all
          || (einfo->info->strip == strip_some
              && bfd_hash_lookup (einfo->info->keep_hash,
                                  h->root.root.root.string,
                                  false, false) == NULL))
    strip = true;
  else
    strip = false;

  if (strip)
    return true;

  if (h->esym.ifd == -2)
    {
      h->esym.jmptbl = 0;
      h->esym.cobol_main = 0;
      h->esym.weakext = 0;
      h->esym.reserved = 0;
      h->esym.ifd = ifdNil;
      h->esym.asym.value = 0;
      h->esym.asym.st = stGlobal;

      if (h->root.root.type != bfd_link_hash_defined
         && h->root.root.type != bfd_link_hash_defweak)
       h->esym.asym.sc = scAbs;
      else
       {
         const char *name;

         sec = h->root.root.u.def.section;
         output_section = sec->output_section;

         /* When making a shared library and symbol h is the one from
            the another shared library, OUTPUT_SECTION may be null.  */
         if (output_section == NULL)
           h->esym.asym.sc = scUndefined;
         else
           {
             name = bfd_section_name (output_section->owner, output_section);

             if (strcmp (name, ".text") == 0)
               h->esym.asym.sc = scText;
             else if (strcmp (name, ".data") == 0)
               h->esym.asym.sc = scData;
             else if (strcmp (name, ".sdata") == 0)
               h->esym.asym.sc = scSData;
             else if (strcmp (name, ".rodata") == 0
                      || strcmp (name, ".rdata") == 0)
               h->esym.asym.sc = scRData;
             else if (strcmp (name, ".bss") == 0)
               h->esym.asym.sc = scBss;
             else if (strcmp (name, ".sbss") == 0)
               h->esym.asym.sc = scSBss;
             else if (strcmp (name, ".init") == 0)
               h->esym.asym.sc = scInit;
             else if (strcmp (name, ".fini") == 0)
               h->esym.asym.sc = scFini;
             else
               h->esym.asym.sc = scAbs;
           }
       }

      h->esym.asym.reserved = 0;
      h->esym.asym.index = indexNil;
    }

  if (h->root.root.type == bfd_link_hash_common)
    h->esym.asym.value = h->root.root.u.c.size;
  else if (h->root.root.type == bfd_link_hash_defined
	   || h->root.root.type == bfd_link_hash_defweak)
    {
      if (h->esym.asym.sc == scCommon)
       h->esym.asym.sc = scBss;
      else if (h->esym.asym.sc == scSCommon)
       h->esym.asym.sc = scSBss;

      sec = h->root.root.u.def.section;
      output_section = sec->output_section;
      if (output_section != NULL)
       h->esym.asym.value = (h->root.root.u.def.value
                             + sec->output_offset
                             + output_section->vma);
      else
       h->esym.asym.value = 0;
    }
  else if ((h->root.elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      /* Set type and value for a symbol with a function stub.  */
      h->esym.asym.st = stProc;
      sec = bfd_get_section_by_name (einfo->abfd, ".plt");
      if (sec == NULL)
	h->esym.asym.value = 0;
      else
	{
	  output_section = sec->output_section;
	  if (output_section != NULL)
	    h->esym.asym.value = (h->root.plt_offset
				  + sec->output_offset
				  + output_section->vma);
	  else
	    h->esym.asym.value = 0;
	}
#if 0 /* FIXME?  */
      h->esym.ifd = 0;
#endif
    }

  if (! bfd_ecoff_debug_one_external (einfo->abfd, einfo->debug, einfo->swap,
                                     h->root.root.root.string,
                                     &h->esym))
    {
      einfo->failed = true;
      return false;
    }

  return true;
}

/* FIXME:  Create a runtime procedure table from the .mdebug section.

static boolean
mips_elf_create_procedure_table (handle, abfd, info, s, debug)
     PTR handle;
     bfd *abfd;
     struct bfd_link_info *info;
     asection *s;
     struct ecoff_debug_info *debug;
 */


static boolean
elf64_alpha_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  bfd *dynobj;
  asection *sgot;
  asection *srelgot;
  asection *sreloc;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel, *relend;

  if (info->relocateable)
    return true;

  sgot = srelgot = sreloc = NULL;
  symtab_hdr = &elf_tdata(abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes(abfd);
  dynobj = elf_hash_table(info)->dynobj;
  if (dynobj)
    {
      sgot = bfd_get_section_by_name(dynobj, ".got");
      srelgot = bfd_get_section_by_name(dynobj, ".rela.got");
    }

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; ++rel)
    {
      unsigned long r_symndx;
      struct alpha_elf_link_hash_entry *h;

      r_symndx = ELF64_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	h = ((struct alpha_elf_link_hash_entry *)
	     sym_hashes[r_symndx - symtab_hdr->sh_info]);

      switch (ELF64_R_TYPE (rel->r_info))
	{
	case R_ALPHA_LITERAL:
	  /* If this is a load of a function symbol and we are building a
	     shared library or calling a shared library, then we need a
	     .plt entry as well.

	     We can tell if it is a function either by noticing the
	     type of the symbol, or, if the type is undefined, by
	     noticing that we have a LITUSE(3) reloc next.

	     Note that it is not fatal to be wrong guessing that a symbol
	     is an object, but it is fatal to be wrong guessing that a
	     symbol is a function.

	     Furthermore, the .plt trampoline does not give constant
	     function addresses, so if we ever see a function's address
	     taken, we cannot do lazy binding on that function. */

	  if (h)
	    {
	      if (rel+1 < relend
		  && ELF64_R_TYPE (rel[1].r_info) == R_ALPHA_LITUSE)
		{
		  switch (rel[1].r_addend)
		    {
		    case 1: /* Memory reference */
		      h->flags |= ALPHA_ELF_LINK_HASH_LU_MEM;
		      break;
		    case 3: /* Call reference */
		      h->flags |= ALPHA_ELF_LINK_HASH_LU_FUNC;
		      break;
		    }
		}
	      else
		h->flags |= ALPHA_ELF_LINK_HASH_LU_ADDR;

	      if (h->root.root.type != bfd_link_hash_undefweak
		  && (info->shared
		      || !(h->root.elf_link_hash_flags
			   & ELF_LINK_HASH_DEF_REGULAR))
		  && (h->root.type == STT_FUNC
		      || (h->root.type == STT_NOTYPE
			  && (h->flags & ALPHA_ELF_LINK_HASH_LU_FUNC))))
		{
		  h->root.elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
		}
	    }

	  if (dynobj == NULL)
	    {
	      elf_hash_table(info)->dynobj = dynobj = abfd;

	      /* Create the .got section.  */
	      if (!elf64_alpha_create_got_section(dynobj, info))
		return false;

	      sgot = bfd_get_section_by_name(dynobj, ".got");
	      srelgot = bfd_get_section_by_name(dynobj, ".rela.got");
	    }

	  if (h != NULL)
	    {
	      if (h->root.got_offset != MINUS_ONE)
		{
		  /* We have already allocated space in this .got.  */
		  break;
		}

	      /* Make sure this becomes a dynamic symbol.  */
	      if (h->root.dynindx == -1
		  && ! _bfd_elf_link_record_dynamic_symbol (info, &h->root))
		return false;

	      /* Reserve space for a reloc even if we won't use it.  */
	      srelgot->_raw_size += sizeof(Elf64_External_Rela);

	      /* Create the relocation in adjust_dynamic_symbol */

	      h->root.got_offset = sgot->_raw_size;
	      sgot->_raw_size += 8;
	    }
	  else
	    {
	      bfd_vma *lgotoff = elf_local_got_offsets(abfd);
	      if (lgotoff == NULL)
		{
		  size_t size;

		  size = elf_tdata(abfd)->symtab_hdr.sh_info * sizeof(bfd_vma);
		  lgotoff = (bfd_vma *)bfd_alloc(abfd, size);
		  if (lgotoff == NULL)
		    return false;

		  elf_local_got_offsets(abfd) = lgotoff;
		  memset(lgotoff, -1, size);
		}

	      if (lgotoff[ELF64_R_SYM(rel->r_info)] != MINUS_ONE)
		{
		  /* We have already allocated space in the .got.  */
		  break;
		}
	      lgotoff[ELF64_R_SYM(rel->r_info)] = sgot->_raw_size;
	      sgot->_raw_size += 8;

	      if (info->shared)
		{
		  /* If we are generating a shared object, we need to
		     output a R_ALPHA_RELATIVE reloc so that the dynamic
		     linker can adjust this GOT entry.  */
		  srelgot->_raw_size += sizeof(Elf64_External_Rela);
		}
	    }
	  break;

	case R_ALPHA_SREL16:
	case R_ALPHA_SREL32:
	case R_ALPHA_SREL64:
	  if (h == NULL)
	    break;
	  /* FALLTHRU */

	case R_ALPHA_REFLONG:
	case R_ALPHA_REFQUAD:
	  if (info->shared
	      || (h != NULL
		  && !(h->root.elf_link_hash_flags
		       & ELF_LINK_HASH_DEF_REGULAR)))
	    {
	      /* When creating a shared object or referring to a symbol in
		 a shared object, we must copy these relocs into the
		 object file.  We create a reloc section in dynobj and
		 make room for the reloc.  */
	      if (sreloc == NULL)
		{
		  const char *name;
		  name = (bfd_elf_string_from_elf_section
			  (abfd, elf_elfheader(abfd)->e_shstrndx,
			   elf_section_data(sec)->rel_hdr.sh_name));
		  if (name == NULL)
		    return false;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (abfd, sec),
					 name+5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  if (sreloc == NULL)
		    {
		      sreloc = bfd_make_section (dynobj, name);
		      if (sreloc == NULL
			  || !bfd_set_section_flags (dynobj, sreloc,
						     (SEC_ALLOC|SEC_LOAD
						      |SEC_HAS_CONTENTS
						      |SEC_IN_MEMORY
						      |SEC_READONLY))
			  || !bfd_set_section_alignment (dynobj, sreloc, 3))
			return false;
		    }
		}
	      sreloc->_raw_size += sizeof (Elf64_External_Rela);
	    }
	  break;
	}
    }

  return true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static boolean
elf64_alpha_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj;
  asection *s;

  dynobj = elf_hash_table(info)->dynobj;

  /* If this is a function, put it in the procedure linkage table.  We
     will fill in the contents of the procedure linkage table later,
     though we could actually do it here.  */

  if (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT)
    {
      /* We hadn't seen all of the input symbols or all of the relocations
	 when we guessed that we needed a .plt entry.  Revise our decision.  */
      if ((!info->shared
	   && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	  || (((struct alpha_elf_link_hash_entry *) h)->flags
	      & ALPHA_ELF_LINK_HASH_LU_ADDR))
	{
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	  return true;
	}

      s = bfd_get_section_by_name(dynobj, ".plt");
      BFD_ASSERT(s != NULL);

      /* The first bit of the .plt is reserved.  */
      if (s->_raw_size == 0)
	s->_raw_size = PLT_HEADER_SIZE;

      h->plt_offset = s->_raw_size;

      /* If this symbol is not defined in a regular file, and we are not
	 generating a shared library, then set the symbol to the location
	 in the .plt.  This is required to make function pointers compare
	 equal between the normal executable and the shared library.  */
      if (!info->shared)
	{
	  h->root.u.def.section = s;
	  h->root.u.def.value = s->_raw_size;
	}

      s->_raw_size += PLT_ENTRY_SIZE;

      /* We also need an entry in the .rela.plt section.  */
      s = bfd_get_section_by_name(dynobj, ".rela.plt");
      BFD_ASSERT(s != NULL);
      s->_raw_size += sizeof(Elf64_External_Rela);

      return true;
    }

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
		  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
      return true;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  The Alpha, since it uses .got entries for
     symbols even in regular objects, does not need the hackery of a
     .dynbss section and COPY dynamic relocations.  */

  return true;
}

/* Set the sizes of the dynamic sections.  */

static boolean
elf64_alpha_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  boolean reltext;
  boolean relplt;

  dynobj = elf_hash_table(info)->dynobj;
  BFD_ASSERT(dynobj != NULL);

  if (elf_hash_table(info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (!info->shared)
	{
	  s = bfd_get_section_by_name(dynobj, ".interp");
	  BFD_ASSERT(s != NULL);
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *)ELF_DYNAMIC_INTERPRETER;
	}
    }
  else
    {
      /* We may have created entries in the .rela.got section.
         However, if we are not creating the dynamic sections, we will
         not actually use these entries.  Reset the size of .rel.got,
         which will cause it to get stripped from the output file
         below.  */
      s = bfd_get_section_by_name (dynobj, ".rela.got");
      if (s != NULL)
	s->_raw_size = 0;
    }

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  reltext = false;
  relplt = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      boolean strip;

      if (!(s->flags & SEC_IN_MEMORY))
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name(dynobj, s);

      /* If we don't need this section, strip it from the output file.
	 This is to handle .rela.bss and .rela.plt.  We must create it
	 in create_dynamic_sections, because it must be created before
	 the linker maps input sections to output sections.  The
	 linker does that before adjust_dynamic_symbol is called, and
	 it is that function which decides whether anything needs to
	 go into these sections.  */

      strip = false;

      if (strncmp(name, ".rela", 5) == 0)
	{
	  strip = (s->_raw_size == 0);

	  if (!strip)
	    {
	      asection *target;

	      /* If this relocation section applies to a read only
		 section, then we probably need a DT_TEXTREL entry.  */
	      target = bfd_get_section_by_name (output_bfd, name + 5);
	      if (target != NULL
		  && (target->flags & SEC_READONLY) != 0)
		reltext = true;

	      if (strcmp(name, ".rela.plt") == 0)
		relplt = true;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strcmp(name, ".got") == 0)
	{
	  /* If we are generating a shared library, we generate a
	     section symbol for each output section.  These are local
	     symbols, which means that they must come first in the
	     dynamic symbol table.  That means we must increment the
	     dynamic symbol index of every other dynamic symbol.  */
	  if (info->shared)
	    {
	      long c[2], i;
	      asection *p;

	      c[0] = 0;
	      c[1] = bfd_count_sections(output_bfd);

	      elf_link_hash_traverse (elf_hash_table(info),
				      elf64_alpha_adjust_dynindx,
				      (PTR)c);
	      elf_hash_table (info)->dynsymcount += c[1];

	      for (i = 1, p = output_bfd->sections;
		   p != NULL;
		   p = p->next, i++)
		{
		  elf_section_data (p)->dynindx = i;
		  /* These symbols will have no names, so we don't need to
		     fiddle with dynstr_index.  */
		}
	    }

	  /* For now, bitch a lot if we exceed the .got size limit.  We
	     should eventually allocate multiple independent .got
	     subsections as necessary.  */
	  if (s->_raw_size > 64*1024)
	    {
	      (*_bfd_error_handler)
		("%s: .got segment overflow (size %lu, max %lu)",
		 bfd_get_filename (output_bfd), s->_raw_size, 64*1024);
	      bfd_set_error (bfd_error_file_too_big);
	      return false;
	    }
	}
      else if (strcmp (name, ".plt") != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	{
	  asection **spp;

	  for (spp = &s->output_section->owner->sections;
	       *spp != s->output_section;
	       spp = &(*spp)->next)
	    continue;
	  *spp = s->output_section->next;
	  --s->output_section->owner->section_count;

	  continue;
	}

      /* Allocate memory for the section contents.  */
      s->contents = (bfd_byte *) bfd_zalloc(dynobj, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	return false;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf64_alpha_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
      if (!info->shared)
	{
	  if (!bfd_elf64_add_dynamic_entry (info, DT_DEBUG, 0))
	    return false;
	}

      if (! bfd_elf64_add_dynamic_entry (info, DT_PLTGOT, 0))
	return false;

      if (relplt)
	{
	  if (! bfd_elf64_add_dynamic_entry (info, DT_PLTRELSZ, 0)
	      || ! bfd_elf64_add_dynamic_entry (info, DT_PLTREL, DT_RELA)
	      || ! bfd_elf64_add_dynamic_entry (info, DT_JMPREL, 0))
	    return false;
	}

      if (! bfd_elf64_add_dynamic_entry (info, DT_RELA, 0)
	  || ! bfd_elf64_add_dynamic_entry (info, DT_RELASZ, 0)
	  || ! bfd_elf64_add_dynamic_entry (info, DT_RELAENT,
					    sizeof(Elf64_External_Rela)))
	return false;

      if (reltext)
	{
	  if (! bfd_elf64_add_dynamic_entry (info, DT_TEXTREL, 0))
	    return false;
	}
    }

  return true;
}

/* Increment the index of a dynamic symbol by a given amount.  Called
   via elf_link_hash_traverse.  */

static boolean
elf64_alpha_adjust_dynindx (h, cparg)
     struct elf_link_hash_entry *h;
     PTR cparg;
{
  long *cp = (long *)cparg;

  if (h->dynindx >= cp[0])
    h->dynindx += cp[1];

  return true;
}

/* Relocate an Alpha ELF section.  */

static boolean
elf64_alpha_relocate_section (output_bfd, info, input_bfd, input_section,
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
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;
  asection *sec, *sgot, *splt;
  bfd *dynobj;
  bfd_vma gp;

  symtab_hdr = &elf_tdata(input_bfd)->symtab_hdr;

  /* Find the gp value for this input bfd.  */
  sgot = NULL;
  gp = 0;
  dynobj = elf_hash_table(info)->dynobj;
  if (dynobj)
    {
      sgot = bfd_get_section_by_name (dynobj, ".got");
      splt = bfd_get_section_by_name (dynobj, ".plt");

      gp = _bfd_get_gp_value(dynobj);
      if (gp == 0)
	{
	  gp = (sgot->output_section->vma
		+ sgot->output_offset
		+ 0x8000);
	  _bfd_set_gp_value(dynobj, gp);
	}
    }

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      bfd_vma relocation;
      bfd_vma addend;
      bfd_reloc_status_type r;

      r_type = ELF64_R_TYPE(rel->r_info);
      if (r_type < 0 || r_type >= (int) R_ALPHA_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      howto = elf64_alpha_howto_table + r_type;

      r_symndx = ELF64_R_SYM(rel->r_info);

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE(sym->st_info) == STT_SECTION)
		{
		  sec = local_sections[r_symndx];
		  rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }

	  continue;
	}

      /* This is a final link.  */

      h = NULL;
      sym = NULL;
      sec = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);
	}
      else
	{
	  h = elf_sym_hashes(input_bfd)[r_symndx - symtab_hdr->sh_info];

	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *)h->root.u.i.link;

	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;

	      /* If the symbol was defined as a common symbol in a
		 regular object file, and there was no definition in
		 any dynamic object, then the linker will have
		 allocated space for the symbol in a common section
		 but the ELF_LINK_HASH_DEF_REGULAR flag will not have
		 been set.  This is done for dynamic symbols in
		 elf_adjust_dynamic_symbol but this is not done for
		 non-dynamic symbols, somehow.  */
	      if ((h->elf_link_hash_flags
		   & (ELF_LINK_HASH_DEF_REGULAR
		      | ELF_LINK_HASH_REF_REGULAR
		      | ELF_LINK_HASH_DEF_DYNAMIC))
		  == ELF_LINK_HASH_REF_REGULAR
		  && !(sec->owner->flags & DYNAMIC))
		h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;

#if rth_notdef
	      if ((r_type == R_ALPHA_LITERAL
		   && elf_hash_table(info)->dynamic_sections_created
		   && (!info->shared
		       || !info->symbolic
		       || !(h->elf_link_hash_flags
			    & ELF_LINK_HASH_DEF_REGULAR)))
		  || (info->shared
		      && (!info->symbolic
			  || !(h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR))
		      && (input_section->flags & SEC_ALLOC)
		      && (r_type == R_ALPHA_REFLONG
			  || r_type == R_ALPHA_REFQUAD
			  || r_type == R_ALPHA_LITERAL)))
		{
		  /* In these cases, we don't need the relocation value.
		     We check specially because in some obscure cases
		     sec->output_section will be NULL.  */
		  relocation = 0;
		}
#else
	      /* FIXME: Are not these obscure cases simply bugs?  Let's
		 get something working and come back to this.  */
	      if (sec->output_section == NULL)
		relocation = 0;
#endif /* rth_notdef */
	      else
		{
		  relocation = (h->root.u.def.value
				+ sec->output_section->vma
				+ sec->output_offset);
		}
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else if (info->shared && !info->symbolic)
	    relocation = 0;
	  else
	    {
	      if (!((*info->callbacks->undefined_symbol)
		    (info, h->root.root.string, input_bfd,
		     input_section, rel->r_offset)))
		return false;
	      relocation = 0;
	    }
	}
      addend = rel->r_addend;

      switch (r_type)
	{
	case R_ALPHA_GPDISP:
	  {
	    bfd_byte *p_ldah, *p_lda;

	    relocation = (input_section->output_section->vma
			  + input_section->output_offset
			  + rel->r_offset);

	    p_ldah = contents + rel->r_offset - input_section->vma;
	    p_lda = p_ldah + rel->r_addend;

	    r = elf64_alpha_do_reloc_gpdisp (input_bfd, gp - relocation,
					     p_ldah, p_lda);
	  }
	  break;

	case R_ALPHA_OP_PUSH:
	case R_ALPHA_OP_STORE:
	case R_ALPHA_OP_PSUB:
	case R_ALPHA_OP_PRSHIFT:
	  /* FIXME */
	  abort();

	case R_ALPHA_LITERAL:
	  {
	    bfd_vma gotoff;

	    BFD_ASSERT(gp != 0);
	    BFD_ASSERT(sgot != NULL);
	    if (h != NULL)
	      {
		gotoff = h->got_offset;
	      }
	    else
	      {
		gotoff = elf_local_got_offsets (input_bfd)[r_symndx];

		/* Use the lsb as a flag indicating that we've already
		   output the relocation entry.  */
		if (info->shared)
		  if (gotoff & 1)
		    gotoff &= ~(bfd_vma)1;
		  else
		    {
		      asection *srel;
		      Elf_Internal_Rela outrel;

		      srel = bfd_get_section_by_name (dynobj, ".rela.got");
		      BFD_ASSERT(srel != NULL);

		      outrel.r_offset = (sgot->output_section->vma
					 + sgot->output_offset + gotoff);
		      outrel.r_info = ELF64_R_INFO(0, R_ALPHA_RELATIVE);
		      outrel.r_addend = 0;

		      bfd_elf64_swap_reloca_out (output_bfd, &outrel,
						 ((Elf64_External_Rela *)
						  srel->contents)
						 + srel->reloc_count++);

		      elf_local_got_offsets (input_bfd)[r_symndx] |= 1;
		    }
	      }

	    /* Initialize the .got entry.  */
	    bfd_put_64 (output_bfd, relocation, sgot->contents + gotoff);

	    /* Figure the gprel relocation.  */
	    addend = 0;
	    relocation = (sgot->output_section->vma
			  + sgot->output_offset
			  + gotoff);
	    relocation -= gp;
	  }
	  /* overflow handled by _bfd_final_link_relocate */
	  goto default_reloc;

	case R_ALPHA_GPREL32:
	  BFD_ASSERT(gp != 0);
	  relocation -= gp;
	  goto default_reloc;

	case R_ALPHA_BRADDR:
	case R_ALPHA_HINT:
	  /* The regular PC-relative stuff measures from the start of
	     the instruction rather than the end.  */
	  addend -= 4;
	  goto default_reloc;

	case R_ALPHA_REFLONG:
	case R_ALPHA_REFQUAD:
	  if (info->shared
	      || (h && !(h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)
		  /* Undef weak symbols in static executables shouldn't
		     trigger a dynamic reloc.  */
		  && elf_hash_table(info)->dynamic_sections_created))
	    {
	      asection *srel;
	      const char *name;
	      Elf_Internal_Rela outrel;

	      name = (bfd_elf_string_from_elf_section
		      (input_bfd, elf_elfheader(input_bfd)->e_shstrndx,
		       elf_section_data(input_section)->rel_hdr.sh_name));
	      BFD_ASSERT(name != NULL);

	      srel = bfd_get_section_by_name(dynobj, name);
	      BFD_ASSERT(srel != NULL);

	      outrel.r_offset = (input_section->output_section->vma
				 + input_section->output_offset
				 + rel->r_offset);
	      outrel.r_addend = 0;
	      if (h)
		{
		  BFD_ASSERT(h->dynindx != -1);
		  outrel.r_info = ELF64_R_INFO(h->dynindx, r_type);
		  relocation = 0;
		}
	      else
		{
		  outrel.r_info = ELF64_R_INFO(0, R_ALPHA_RELATIVE);
		}

	      bfd_elf64_swap_reloca_out (output_bfd, &outrel,
					 ((Elf64_External_Rela *)
					  srel->contents)
					 + srel->reloc_count++);
	    }
	  goto default_reloc;

	default:
	default_reloc:
	  r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					contents, rel->r_offset, relocation,
					addend);
	  break;
	}

      switch (r)
	{
	case bfd_reloc_ok:
	  break;

	case bfd_reloc_overflow:
	  {
	    const char *name;

	    if (h != NULL)
	      name = h->root.root.string;
	    else
	      {
		name = (bfd_elf_string_from_elf_section
			(input_bfd, symtab_hdr->sh_link, sym->st_name));
		if (name == NULL)
		  return false;
		if (*name == '\0')
		  name = bfd_section_name (input_bfd, sec);
	      }
	    if (! ((*info->callbacks->reloc_overflow)
		   (info, name, howto->name, (bfd_vma) 0,
		    input_bfd, input_section, rel->r_offset)))
	      return false;
	  }
	  break;

	default:
	case bfd_reloc_outofrange:
	  abort ();
	}
    }

  return true;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
elf64_alpha_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj = elf_hash_table(info)->dynobj;

  if (h->plt_offset != MINUS_ONE)
    {
      asection *splt, *sgot, *srel;
      Elf_Internal_Rela outrel;
      bfd_vma got_addr, plt_addr;
      bfd_vma plt_index;

      /* This symbol has an entry in the procedure linkage table.  */

      BFD_ASSERT(h->dynindx != -1);
      BFD_ASSERT(h->got_offset != MINUS_ONE);

      splt = bfd_get_section_by_name(dynobj, ".plt");
      BFD_ASSERT(splt != NULL);
      srel = bfd_get_section_by_name(dynobj, ".rela.plt");
      BFD_ASSERT(srel != NULL);
      sgot = bfd_get_section_by_name(dynobj, ".got");
      BFD_ASSERT(sgot != NULL);

      got_addr = (sgot->output_section->vma
		  + sgot->output_offset
		  + h->got_offset);
      plt_addr = (splt->output_section->vma
		  + splt->output_offset
		  + h->plt_offset);

      plt_index = (h->plt_offset - PLT_HEADER_SIZE) / PLT_ENTRY_SIZE;

      /* Fill in the entry in the procedure linkage table.  */
      {
	unsigned insn1, insn2, insn3;
	long hi, lo;

	/* decompose the reloc offset for the plt for ldah+lda */
	hi = plt_index * sizeof(Elf64_External_Rela);
	lo = ((hi & 0xffff) ^ 0x8000) - 0x8000;
	hi = (hi - lo) >> 16;

	insn1 = PLT_ENTRY_WORD1 | (hi & 0xffff);
	insn2 = PLT_ENTRY_WORD2 | (lo & 0xffff);
	insn3 = PLT_ENTRY_WORD3 | ((-(h->plt_offset + 12) >> 2) & 0x1fffff);

	bfd_put_32 (output_bfd, insn1, splt->contents + h->plt_offset);
	bfd_put_32 (output_bfd, insn2, splt->contents + h->plt_offset + 4);
	bfd_put_32 (output_bfd, insn3, splt->contents + h->plt_offset + 8);
      }

      /* Fill in the entry in the .rela.plt section.  */
      outrel.r_offset = got_addr;
      outrel.r_info = ELF64_R_INFO(h->dynindx, R_ALPHA_JMP_SLOT);
      outrel.r_addend = 0;

      bfd_elf64_swap_reloca_out (output_bfd, &outrel,
				 ((Elf64_External_Rela *)srel->contents
				  + plt_index));

      if (!(h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	{
	  /* Mark the symbol as undefined, rather than as defined in the
	     .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;
	}

      /* Fill in the entry in the global offset table.  */
      bfd_put_64 (output_bfd, plt_addr, sgot->contents + h->got_offset);
    }
  else if (h->got_offset != MINUS_ONE)
    {
      asection *sgot, *srel;
      Elf_Internal_Rela outrel;

      BFD_ASSERT(h->dynindx != -1);

      sgot = bfd_get_section_by_name (dynobj, ".got");
      BFD_ASSERT (sgot != NULL);
      srel = bfd_get_section_by_name (dynobj, ".rela.got");
      BFD_ASSERT (srel != NULL);

      outrel.r_offset = (sgot->output_section->vma
		       + sgot->output_offset
		       + h->got_offset);
      outrel.r_addend = 0;
      if (info->shared
	  && info->symbolic
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	outrel.r_info = ELF64_R_INFO(0, R_ALPHA_RELATIVE);
      else
	{
	  bfd_put_64(output_bfd, (bfd_vma)0, sgot->contents + h->got_offset);
	  outrel.r_info = ELF64_R_INFO(h->dynindx, R_ALPHA_GLOB_DAT);
	}

      bfd_elf64_swap_reloca_out (output_bfd, &outrel,
				 ((Elf64_External_Rela *)srel->contents
				  + srel->reloc_count++));
    }

  /* Mark some specially defined symbols as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0
      || strcmp (h->root.root.string, "_PROCEDURE_LINKAGE_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return true;
}

/* Finish up the dynamic sections.  */

static boolean
elf64_alpha_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn;
  asection *sgot;

  dynobj = elf_hash_table (info)->dynobj;
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      asection *splt;
      Elf64_External_Dyn *dyncon, *dynconend;

      splt = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (splt != NULL && sdyn != NULL);

      dyncon = (Elf64_External_Dyn *) sdyn->contents;
      dynconend = (Elf64_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  asection *s;

	  bfd_elf64_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    case DT_PLTGOT:
	      name = ".plt";
	      goto get_vma;
	    case DT_PLTRELSZ:
	      name = ".rela.plt";
	      goto get_size;
	    case DT_JMPREL:
	      name = ".rela.plt";
	      goto get_vma;

	    case DT_RELASZ:
	      /* My interpretation of the TIS v1.1 ELF document indicates
		 that RELASZ should not include JMPREL.  This is not what
		 the rest of the BFD does.  It is, however, what the
		 glibc ld.so wants.  Do this fixup here until we found
		 out who is right.  */
	      s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	      if (s)
		{
		  dyn.d_un.d_val -=
		    (s->_cooked_size ? s->_cooked_size : s->_raw_size);
		}
	      break;

	    get_vma:
	      s = bfd_get_section_by_name (output_bfd, name);
	      dyn.d_un.d_ptr = (s ? s->vma : 0);
	      break;

	    get_size:
	      s = bfd_get_section_by_name (output_bfd, name);
	      dyn.d_un.d_val =
		(s->_cooked_size ? s->_cooked_size : s->_raw_size);
	      break;
	    }

	  bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	}

      /* Initialize the PLT0 entry */
      if (splt->_raw_size > 0)
	{
	  bfd_put_32 (output_bfd, PLT_HEADER_WORD1, splt->contents);
	  bfd_put_32 (output_bfd, PLT_HEADER_WORD2, splt->contents + 4);
	  bfd_put_32 (output_bfd, PLT_HEADER_WORD3, splt->contents + 8);
	  bfd_put_32 (output_bfd, PLT_HEADER_WORD4, splt->contents + 12);

	  /* The next two words will be filled in by ld.so */
	  bfd_put_64 (output_bfd, 0, splt->contents + 16);
	  bfd_put_64 (output_bfd, 0, splt->contents + 24);

	  elf_section_data (splt->output_section)->this_hdr.sh_entsize =
	    PLT_HEADER_SIZE;
	}
    }

  /* Set the first entry in the global offset table to the address of
     the dynamic section.  */
  sgot = bfd_get_section_by_name (dynobj, ".got");
  if (sgot && sgot->_raw_size > 0)
    {
      if (sdyn == NULL)
        bfd_put_64 (output_bfd, (bfd_vma)0, sgot->contents);
      else
        bfd_put_64 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    sgot->contents);

      elf_section_data (sgot->output_section)->this_hdr.sh_entsize =
	8 * RESERVED_GOT_ENTRIES;
    }

  if (info->shared)
    {
      asection *sdynsym;
      asection *s;
      Elf_Internal_Sym sym;

      /* Set up the section symbols for the output sections.  */

      sdynsym = bfd_get_section_by_name (dynobj, ".dynsym");
      BFD_ASSERT (sdynsym != NULL);

      sym.st_size = 0;
      sym.st_name = 0;
      sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
      sym.st_other = 0;

      for (s = output_bfd->sections; s != NULL; s = s->next)
	{
	  int indx;

	  sym.st_value = s->vma;

	  indx = elf_section_data (s)->this_idx;
	  BFD_ASSERT (indx > 0);
	  sym.st_shndx = indx;

	  bfd_elf64_swap_symbol_out (output_bfd, &sym,
				     (PTR) (((Elf64_External_Sym *)
					     sdynsym->contents)
					    + elf_section_data (s)->dynindx));
	}

      /* Set the sh_info field of the output .dynsym section to the
         index of the first global symbol.  */
      elf_section_data (sdynsym->output_section)->this_hdr.sh_info =
	bfd_count_sections (output_bfd) + 1;
    }

  return true;
}

/* We need to use a special link routine to handle the .reginfo and
   the .mdebug sections.  We need to merge all instances of these
   sections together, not write them all out sequentially.  */

static boolean
elf64_alpha_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection *o;
  struct bfd_link_order *p;
  asection *reginfo_sec, *mdebug_sec, *gptab_data_sec, *gptab_bss_sec;
  struct ecoff_debug_info debug;
  const struct ecoff_debug_swap *swap
    = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;
  HDRR *symhdr = &debug.symbolic_header;
  PTR mdebug_handle = NULL;

  /* Go through the sections and collect the .reginfo and .mdebug
     information.  */
  reginfo_sec = NULL;
  mdebug_sec = NULL;
  gptab_data_sec = NULL;
  gptab_bss_sec = NULL;
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
#ifdef ERIC_neverdef
      if (strcmp (o->name, ".reginfo") == 0)
	{
	  memset (&reginfo, 0, sizeof reginfo);

	  /* We have found the .reginfo section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      Elf64_External_RegInfo ext;
	      Elf64_RegInfo sub;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* The linker emulation code has probably clobbered the
                 size to be zero bytes.  */
	      if (input_section->_raw_size == 0)
		input_section->_raw_size = sizeof (Elf64_External_RegInfo);

	      if (! bfd_get_section_contents (input_bfd, input_section,
					      (PTR) &ext,
					      (file_ptr) 0,
					      sizeof ext))
		return false;

	      bfd_alpha_elf64_swap_reginfo_in (input_bfd, &ext, &sub);

	      reginfo.ri_gprmask |= sub.ri_gprmask;
	      reginfo.ri_cprmask[0] |= sub.ri_cprmask[0];
	      reginfo.ri_cprmask[1] |= sub.ri_cprmask[1];
	      reginfo.ri_cprmask[2] |= sub.ri_cprmask[2];
	      reginfo.ri_cprmask[3] |= sub.ri_cprmask[3];

	      /* ri_gp_value is set by the function
		 alpha_elf_section_processing when the section is
		 finally written out.  */

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* Force the section size to the value we want.  */
	  o->_raw_size = sizeof (Elf64_External_RegInfo);

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  reginfo_sec = o;
	}
#endif

      if (strcmp (o->name, ".mdebug") == 0)
	{
	  struct extsym_info einfo;

	  /* We have found the .mdebug section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  symhdr->magic = swap->sym_magic;
	  /* FIXME: What should the version stamp be?  */
	  symhdr->vstamp = 0;
	  symhdr->ilineMax = 0;
	  symhdr->cbLine = 0;
	  symhdr->idnMax = 0;
	  symhdr->ipdMax = 0;
	  symhdr->isymMax = 0;
	  symhdr->ioptMax = 0;
	  symhdr->iauxMax = 0;
	  symhdr->issMax = 0;
	  symhdr->issExtMax = 0;
	  symhdr->ifdMax = 0;
	  symhdr->crfd = 0;
	  symhdr->iextMax = 0;

	  /* We accumulate the debugging information itself in the
	     debug_info structure.  */
	  debug.line = NULL;
	  debug.external_dnr = NULL;
	  debug.external_pdr = NULL;
	  debug.external_sym = NULL;
	  debug.external_opt = NULL;
	  debug.external_aux = NULL;
	  debug.ss = NULL;
	  debug.ssext = debug.ssext_end = NULL;
	  debug.external_fdr = NULL;
	  debug.external_rfd = NULL;
	  debug.external_ext = debug.external_ext_end = NULL;

	  mdebug_handle = bfd_ecoff_debug_init (abfd, &debug, swap, info);
	  if (mdebug_handle == (PTR) NULL)
	    return false;

	  if (1)
	    {
	      asection *s;
	      EXTR esym;
	      bfd_vma last;
	      unsigned int i;
	      static const char * const name[] =
		{
		  ".text", ".init", ".fini", ".data",
		  ".rodata", ".sdata", ".sbss", ".bss"
		};
	      static const int sc[] = { scText, scInit, scFini, scData,
					  scRData, scSData, scSBss, scBss };

	      esym.jmptbl = 0;
	      esym.cobol_main = 0;
	      esym.weakext = 0;
	      esym.reserved = 0;
	      esym.ifd = ifdNil;
	      esym.asym.iss = issNil;
	      esym.asym.st = stLocal;
	      esym.asym.reserved = 0;
	      esym.asym.index = indexNil;
	      for (i = 0; i < 8; i++)
		{
		  esym.asym.sc = sc[i];
		  s = bfd_get_section_by_name (abfd, name[i]);
		  if (s != NULL)
		    {
		      esym.asym.value = s->vma;
		      last = s->vma + s->_raw_size;
		    }
		  else
		    esym.asym.value = last;

		  if (! bfd_ecoff_debug_one_external (abfd, &debug, swap,
						      name[i], &esym))
		    return false;
		}
	    }

	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      const struct ecoff_debug_swap *input_swap;
	      struct ecoff_debug_info input_debug;
	      char *eraw_src;
	      char *eraw_end;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      if (bfd_get_flavour (input_bfd) != bfd_target_elf_flavour
		  || (get_elf_backend_data (input_bfd)
		      ->elf_backend_ecoff_debug_swap) == NULL)
		{
		  /* I don't know what a non ALPHA ELF bfd would be
		     doing with a .mdebug section, but I don't really
		     want to deal with it.  */
		  continue;
		}

	      input_swap = (get_elf_backend_data (input_bfd)
			    ->elf_backend_ecoff_debug_swap);

	      BFD_ASSERT (p->size == input_section->_raw_size);

	      /* The ECOFF linking code expects that we have already
		 read in the debugging information and set up an
		 ecoff_debug_info structure, so we do that now.  */
	      if (!elf64_alpha_read_ecoff_info (input_bfd, input_section,
						&input_debug))
		return false;

	      if (! (bfd_ecoff_debug_accumulate
		     (mdebug_handle, abfd, &debug, swap, input_bfd,
		      &input_debug, input_swap, info)))
		return false;

	      /* Loop through the external symbols.  For each one with
		 interesting information, try to find the symbol in
		 the linker global hash table and save the information
		 for the output external symbols.  */
	      eraw_src = input_debug.external_ext;
	      eraw_end = (eraw_src
			  + (input_debug.symbolic_header.iextMax
			     * input_swap->external_ext_size));
	      for (;
		   eraw_src < eraw_end;
		   eraw_src += input_swap->external_ext_size)
		{
		  EXTR ext;
		  const char *name;
		  struct alpha_elf_link_hash_entry *h;

		  (*input_swap->swap_ext_in) (input_bfd, (PTR) eraw_src, &ext);
		  if (ext.asym.sc == scNil
		      || ext.asym.sc == scUndefined
		      || ext.asym.sc == scSUndefined)
		    continue;

		  name = input_debug.ssext + ext.asym.iss;
		  h = alpha_elf_link_hash_lookup (alpha_elf_hash_table (info),
						  name, false, false, true);
		  if (h == NULL || h->esym.ifd != -2)
		    continue;

		  if (ext.ifd != -1)
		    {
		      BFD_ASSERT (ext.ifd
				  < input_debug.symbolic_header.ifdMax);
		      ext.ifd = input_debug.ifdmap[ext.ifd];
		    }

		  h->esym = ext;
		}

	      /* Free up the information we just read.  */
	      free (input_debug.line);
	      free (input_debug.external_dnr);
	      free (input_debug.external_pdr);
	      free (input_debug.external_sym);
	      free (input_debug.external_opt);
	      free (input_debug.external_aux);
	      free (input_debug.ss);
	      free (input_debug.ssext);
	      free (input_debug.external_fdr);
	      free (input_debug.external_rfd);
	      free (input_debug.external_ext);

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

#ifdef ERIC_neverdef
	  if (info->shared)
	    {
	      /* Create .rtproc section.  */
	      rtproc_sec = bfd_get_section_by_name (abfd, ".rtproc");
	      if (rtproc_sec == NULL)
		{
		  flagword flags = (SEC_HAS_CONTENTS | SEC_IN_MEMORY
				    | SEC_READONLY);

		  rtproc_sec = bfd_make_section (abfd, ".rtproc");
		  if (rtproc_sec == NULL
		      || ! bfd_set_section_flags (abfd, rtproc_sec, flags)
		      || ! bfd_set_section_alignment (abfd, rtproc_sec, 12))
		    return false;
		}

	      if (! alpha_elf_create_procedure_table (mdebug_handle, abfd,
						     info, rtproc_sec, &debug))
		return false;
	    }
#endif


	  /* Build the external symbol information.  */
	  einfo.abfd = abfd;
	  einfo.info = info;
	  einfo.debug = &debug;
	  einfo.swap = swap;
	  einfo.failed = false;
	  elf_link_hash_traverse (elf_hash_table (info),
				  elf64_alpha_output_extsym,
				  (PTR) &einfo);
	  if (einfo.failed)
	    return false;

	  /* Set the size of the .mdebug section.  */
	  o->_raw_size = bfd_ecoff_debug_size (abfd, &debug, swap);

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  mdebug_sec = o;
	}

#ifdef ERIC_neverdef
      if (strncmp (o->name, ".gptab.", sizeof ".gptab." - 1) == 0)
	{
	  const char *subname;
	  unsigned int c;
	  Elf64_gptab *tab;
	  Elf64_External_gptab *ext_tab;
	  unsigned int i;

	  /* The .gptab.sdata and .gptab.sbss sections hold
	     information describing how the small data area would
	     change depending upon the -G switch.  These sections
	     not used in executables files.  */
	  if (! info->relocateable)
	    {
	      asection **secpp;

	      for (p = o->link_order_head;
		   p != (struct bfd_link_order *) NULL;
		   p = p->next)
		{
		  asection *input_section;

		  if (p->type != bfd_indirect_link_order)
		    {
		      if (p->type == bfd_fill_link_order)
			continue;
		      abort ();
		    }

		  input_section = p->u.indirect.section;

		  /* Hack: reset the SEC_HAS_CONTENTS flag so that
		     elf_link_input_bfd ignores this section.  */
		  input_section->flags &=~ SEC_HAS_CONTENTS;
		}

	      /* Skip this section later on (I don't think this
		 currently matters, but someday it might).  */
	      o->link_order_head = (struct bfd_link_order *) NULL;

	      /* Really remove the section.  */
	      for (secpp = &abfd->sections;
		   *secpp != o;
		   secpp = &(*secpp)->next)
		;
	      *secpp = (*secpp)->next;
	      --abfd->section_count;

	      continue;
	    }

	  /* There is one gptab for initialized data, and one for
	     uninitialized data.  */
	  if (strcmp (o->name, ".gptab.sdata") == 0)
	    gptab_data_sec = o;
	  else if (strcmp (o->name, ".gptab.sbss") == 0)
	    gptab_bss_sec = o;
	  else
	    {
	      (*_bfd_error_handler)
		("%s: illegal section name `%s'",
		 bfd_get_filename (abfd), o->name);
	      bfd_set_error (bfd_error_nonrepresentable_section);
	      return false;
	    }

	  /* The linker script always combines .gptab.data and
	     .gptab.sdata into .gptab.sdata, and likewise for
	     .gptab.bss and .gptab.sbss.  It is possible that there is
	     no .sdata or .sbss section in the output file, in which
	     case we must change the name of the output section.  */
	  subname = o->name + sizeof ".gptab" - 1;
	  if (bfd_get_section_by_name (abfd, subname) == NULL)
	    {
	      if (o == gptab_data_sec)
		o->name = ".gptab.data";
	      else
		o->name = ".gptab.bss";
	      subname = o->name + sizeof ".gptab" - 1;
	      BFD_ASSERT (bfd_get_section_by_name (abfd, subname) != NULL);
	    }

	  /* Set up the first entry.  */
	  c = 1;
	  tab = (Elf64_gptab *) bfd_malloc (c * sizeof (Elf64_gptab));
	  if (tab == NULL)
	    return false;
	  tab[0].gt_header.gt_current_g_value = elf_gp_size (abfd);
	  tab[0].gt_header.gt_unused = 0;

	  /* Combine the input sections.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      bfd_size_type size;
	      unsigned long last;
	      bfd_size_type gpentry;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* Combine the gptab entries for this input section one
		 by one.  We know that the input gptab entries are
		 sorted by ascending -G value.  */
	      size = bfd_section_size (input_bfd, input_section);
	      last = 0;
	      for (gpentry = sizeof (Elf64_External_gptab);
		   gpentry < size;
		   gpentry += sizeof (Elf64_External_gptab))
		{
		  Elf64_External_gptab ext_gptab;
		  Elf64_gptab int_gptab;
		  unsigned long val;
		  unsigned long add;
		  boolean exact;
		  unsigned int look;

		  if (! (bfd_get_section_contents
			 (input_bfd, input_section, (PTR) &ext_gptab,
			  gpentry, sizeof (Elf64_External_gptab))))
		    {
		      free (tab);
		      return false;
		    }

		  bfd_alpha_elf64_swap_gptab_in (input_bfd, &ext_gptab,
						&int_gptab);
		  val = int_gptab.gt_entry.gt_g_value;
		  add = int_gptab.gt_entry.gt_bytes - last;

		  exact = false;
		  for (look = 1; look < c; look++)
		    {
		      if (tab[look].gt_entry.gt_g_value >= val)
			tab[look].gt_entry.gt_bytes += add;

		      if (tab[look].gt_entry.gt_g_value == val)
			exact = true;
		    }

		  if (! exact)
		    {
		      Elf64_gptab *new_tab;
		      unsigned int max;

		      /* We need a new table entry.  */
		      new_tab = ((Elf64_gptab *)
				 bfd_realloc ((PTR) tab,
					      (c + 1) * sizeof (Elf64_gptab)));
		      if (new_tab == NULL)
			{
			  free (tab);
			  return false;
			}
		      tab = new_tab;
		      tab[c].gt_entry.gt_g_value = val;
		      tab[c].gt_entry.gt_bytes = add;

		      /* Merge in the size for the next smallest -G
			 value, since that will be implied by this new
			 value.  */
		      max = 0;
		      for (look = 1; look < c; look++)
			{
			  if (tab[look].gt_entry.gt_g_value < val
			      && (max == 0
				  || (tab[look].gt_entry.gt_g_value
				      > tab[max].gt_entry.gt_g_value)))
			    max = look;
			}
		      if (max != 0)
			tab[c].gt_entry.gt_bytes +=
			  tab[max].gt_entry.gt_bytes;

		      ++c;
		    }

		  last = int_gptab.gt_entry.gt_bytes;
		}

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* The table must be sorted by -G value.  */
	  if (c > 2)
	    qsort (tab + 1, c - 1, sizeof (tab[0]), gptab_compare);

	  /* Swap out the table.  */
	  ext_tab = ((Elf64_External_gptab *)
		     bfd_alloc (abfd, c * sizeof (Elf64_External_gptab)));
	  if (ext_tab == NULL)
	    {
	      free (tab);
	      return false;
	    }

	  for (i = 0; i < c; i++)
	    bfd_alpha_elf64_swap_gptab_out (abfd, tab + i, ext_tab + i);
	  free (tab);

	  o->_raw_size = c * sizeof (Elf64_External_gptab);
	  o->contents = (bfd_byte *) ext_tab;

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;
	}
#endif

    }

  /* Invoke the regular ELF backend linker to do all the work.  */
  if (! bfd_elf64_bfd_final_link (abfd, info))
    return false;

  /* Now write out the computed sections.  */

#ifdef ERIC_neverdef
  if (reginfo_sec != (asection *) NULL)
    {
      Elf64_External_RegInfo ext;

      bfd_alpha_elf64_swap_reginfo_out (abfd, &reginfo, &ext);
      if (! bfd_set_section_contents (abfd, reginfo_sec, (PTR) &ext,
				      (file_ptr) 0, sizeof ext))
	return false;
    }
#endif

  if (mdebug_sec != (asection *) NULL)
    {
      BFD_ASSERT (abfd->output_has_begun);
      if (! bfd_ecoff_write_accumulated_debug (mdebug_handle, abfd, &debug,
					       swap, info,
					       mdebug_sec->filepos))
	return false;

      bfd_ecoff_debug_free (mdebug_handle, abfd, &debug, swap, info);
    }

  if (gptab_data_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_data_sec,
				      gptab_data_sec->contents,
				      (file_ptr) 0,
				      gptab_data_sec->_raw_size))
	return false;
    }

  if (gptab_bss_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_bss_sec,
				      gptab_bss_sec->contents,
				      (file_ptr) 0,
				      gptab_bss_sec->_raw_size))
	return false;
    }

  return true;
}

/* ECOFF swapping routines.  These are used when dealing with the
   .mdebug section, which is in the ECOFF debugging format.  Copied
   from elf32-mips.c. */
static const struct ecoff_debug_swap
elf64_alpha_ecoff_debug_swap =
{
  /* Symbol table magic number.  */
  magicSym2,
  /* Alignment of debugging information.  E.g., 4.  */
  8,
  /* Sizes of external symbolic information.  */
  sizeof (struct hdr_ext),
  sizeof (struct dnr_ext),
  sizeof (struct pdr_ext),
  sizeof (struct sym_ext),
  sizeof (struct opt_ext),
  sizeof (struct fdr_ext),
  sizeof (struct rfd_ext),
  sizeof (struct ext_ext),
  /* Functions to swap in external symbolic data.  */
  ecoff_swap_hdr_in,
  ecoff_swap_dnr_in,
  ecoff_swap_pdr_in,
  ecoff_swap_sym_in,
  ecoff_swap_opt_in,
  ecoff_swap_fdr_in,
  ecoff_swap_rfd_in,
  ecoff_swap_ext_in,
  _bfd_ecoff_swap_tir_in,
  _bfd_ecoff_swap_rndx_in,
  /* Functions to swap out external symbolic data.  */
  ecoff_swap_hdr_out,
  ecoff_swap_dnr_out,
  ecoff_swap_pdr_out,
  ecoff_swap_sym_out,
  ecoff_swap_opt_out,
  ecoff_swap_fdr_out,
  ecoff_swap_rfd_out,
  ecoff_swap_ext_out,
  _bfd_ecoff_swap_tir_out,
  _bfd_ecoff_swap_rndx_out,
  /* Function to read in symbolic data.  */
  elf64_alpha_read_ecoff_info
};

#define TARGET_LITTLE_SYM	bfd_elf64_alpha_vec
#define TARGET_LITTLE_NAME	"elf64-alpha"
#define ELF_ARCH		bfd_arch_alpha
#define ELF_MACHINE_CODE 	EM_ALPHA
#define ELF_MAXPAGESIZE 	0x100000

#define bfd_elf64_bfd_link_hash_table_create \
  elf64_alpha_bfd_link_hash_table_create

#define bfd_elf64_bfd_reloc_type_lookup \
  elf64_alpha_bfd_reloc_type_lookup
#define elf_info_to_howto \
  elf64_alpha_info_to_howto

#define elf_backend_object_p \
  elf64_alpha_object_p
#define elf_backend_section_from_shdr \
  elf64_alpha_section_from_shdr
#define elf_backend_fake_sections \
  elf64_alpha_fake_sections
#define elf_backend_additional_program_headers \
  elf64_alpha_additional_program_headers

#define bfd_elf64_bfd_is_local_label \
  elf64_alpha_is_local_label
#define bfd_elf64_find_nearest_line \
  elf64_alpha_find_nearest_line

#define elf_backend_check_relocs \
  elf64_alpha_check_relocs
#define elf_backend_create_dynamic_sections \
  elf64_alpha_create_dynamic_sections
#define elf_backend_adjust_dynamic_symbol \
  elf64_alpha_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
  elf64_alpha_size_dynamic_sections
#define elf_backend_relocate_section \
  elf64_alpha_relocate_section
#define elf_backend_finish_dynamic_symbol \
  elf64_alpha_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
  elf64_alpha_finish_dynamic_sections
#define bfd_elf64_bfd_final_link \
  elf64_alpha_final_link

#define elf_backend_ecoff_debug_swap \
  &elf64_alpha_ecoff_debug_swap

/*
 * A few constants that determine how the .plt section is set up.
 */
#define elf_backend_want_got_plt 0
#define elf_backend_plt_readonly 0
#define elf_backend_want_plt_sym 1

#include "elf64-target.h"
