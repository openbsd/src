/* BFD back-end for ALPHA Extended-Coff files.
   Copyright 1993, 1994 Free Software Foundation, Inc.
   Modified from coff-mips.c by Steve Chamberlain <sac@cygnus.com> and
   Ian Lance Taylor <ian@cygnus.com>.

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
#include "bfdlink.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/ecoff.h"
#include "coff/alpha.h"
#include "libcoff.h"
#include "libecoff.h"

/* Prototypes for static functions.  */

static const bfd_target *alpha_ecoff_object_p PARAMS ((bfd *));
static boolean alpha_ecoff_bad_format_hook PARAMS ((bfd *abfd, PTR filehdr));
static PTR alpha_ecoff_mkobject_hook PARAMS ((bfd *, PTR filehdr, PTR aouthdr));
static void alpha_ecoff_swap_reloc_in PARAMS ((bfd *, PTR,
					      struct internal_reloc *));
static void alpha_ecoff_swap_reloc_out PARAMS ((bfd *,
					       const struct internal_reloc *,
					       PTR));
static void alpha_adjust_reloc_in PARAMS ((bfd *,
					   const struct internal_reloc *,
					   arelent *));
static void alpha_adjust_reloc_out PARAMS ((bfd *, const arelent *,
					    struct internal_reloc *));
static bfd_byte *alpha_ecoff_get_relocated_section_contents
  PARAMS ((bfd *abfd, struct bfd_link_info *, struct bfd_link_order *,
	   bfd_byte *data, boolean relocateable, asymbol **symbols));
static bfd_vma alpha_convert_external_reloc
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, struct external_reloc *,
	   struct ecoff_link_hash_entry *));
static boolean alpha_relocate_section PARAMS ((bfd *, struct bfd_link_info *,
					       bfd *, asection *,
					       bfd_byte *, PTR));
static boolean alpha_adjust_headers
  PARAMS ((bfd *, struct internal_filehdr *, struct internal_aouthdr *));

/* ECOFF has COFF sections, but the debugging information is stored in
   a completely different format.  ECOFF targets use some of the
   swapping routines from coffswap.h, and some of the generic COFF
   routines in coffgen.c, but, unlike the real COFF targets, do not
   use coffcode.h itself.

   Get the generic COFF swapping routines, except for the reloc,
   symbol, and lineno ones.  Give them ecoff names.  Define some
   accessor macros for the large sizes used for Alpha ECOFF.  */

#define GET_FILEHDR_SYMPTR bfd_h_get_64
#define PUT_FILEHDR_SYMPTR bfd_h_put_64
#define GET_AOUTHDR_TSIZE bfd_h_get_64
#define PUT_AOUTHDR_TSIZE bfd_h_put_64
#define GET_AOUTHDR_DSIZE bfd_h_get_64
#define PUT_AOUTHDR_DSIZE bfd_h_put_64
#define GET_AOUTHDR_BSIZE bfd_h_get_64
#define PUT_AOUTHDR_BSIZE bfd_h_put_64
#define GET_AOUTHDR_ENTRY bfd_h_get_64
#define PUT_AOUTHDR_ENTRY bfd_h_put_64
#define GET_AOUTHDR_TEXT_START bfd_h_get_64
#define PUT_AOUTHDR_TEXT_START bfd_h_put_64
#define GET_AOUTHDR_DATA_START bfd_h_get_64
#define PUT_AOUTHDR_DATA_START bfd_h_put_64
#define GET_SCNHDR_PADDR bfd_h_get_64
#define PUT_SCNHDR_PADDR bfd_h_put_64
#define GET_SCNHDR_VADDR bfd_h_get_64
#define PUT_SCNHDR_VADDR bfd_h_put_64
#define GET_SCNHDR_SIZE bfd_h_get_64
#define PUT_SCNHDR_SIZE bfd_h_put_64
#define GET_SCNHDR_SCNPTR bfd_h_get_64
#define PUT_SCNHDR_SCNPTR bfd_h_put_64
#define GET_SCNHDR_RELPTR bfd_h_get_64
#define PUT_SCNHDR_RELPTR bfd_h_put_64
#define GET_SCNHDR_LNNOPTR bfd_h_get_64
#define PUT_SCNHDR_LNNOPTR bfd_h_put_64

#define ALPHAECOFF

#define NO_COFF_RELOCS
#define NO_COFF_SYMBOLS
#define NO_COFF_LINENOS
#define coff_swap_filehdr_in alpha_ecoff_swap_filehdr_in
#define coff_swap_filehdr_out alpha_ecoff_swap_filehdr_out
#define coff_swap_aouthdr_in alpha_ecoff_swap_aouthdr_in
#define coff_swap_aouthdr_out alpha_ecoff_swap_aouthdr_out
#define coff_swap_scnhdr_in alpha_ecoff_swap_scnhdr_in
#define coff_swap_scnhdr_out alpha_ecoff_swap_scnhdr_out
#include "coffswap.h"

/* Get the ECOFF swapping routines.  */
#define ECOFF_64
#include "ecoffswap.h"

/* How to process the various reloc types.  */

static bfd_reloc_status_type
reloc_nil PARAMS ((bfd *, arelent *, asymbol *, PTR,
		   asection *, bfd *, char **));

static bfd_reloc_status_type
reloc_nil (abfd, reloc, sym, data, sec, output_bfd, error_message)
     bfd *abfd;
     arelent *reloc;
     asymbol *sym;
     PTR data;
     asection *sec;
     bfd *output_bfd;
     char **error_message;
{
  return bfd_reloc_ok;
}

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

static reloc_howto_type alpha_howto_table[] =
{
  /* Reloc type 0 is ignored by itself.  However, it appears after a
     GPDISP reloc to identify the location where the low order 16 bits
     of the gp register are loaded.  */
  HOWTO (ALPHA_R_IGNORE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "IGNORE",		/* name */
	 true,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 32 bit reference to a symbol.  */
  HOWTO (ALPHA_R_REFLONG,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "REFLONG",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit reference to a symbol.  */
  HOWTO (ALPHA_R_REFQUAD,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "REFQUAD",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 32 bit GP relative offset.  This is just like REFLONG except
     that when the value is used the value of the gp register will be
     added in.  */
  HOWTO (ALPHA_R_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "GPREL32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Used for an instruction that refers to memory off the GP
     register.  The offset is 16 bits of the 32 bit instruction.  This
     reloc always seems to be against the .lita section.  */
  HOWTO (ALPHA_R_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "LITERAL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* This reloc only appears immediately following a LITERAL reloc.
     It identifies a use of the literal.  It seems that the linker can
     use this to eliminate a portion of the .lita section.  The symbol
     index is special: 1 means the literal address is in the base
     register of a memory format instruction; 2 means the literal
     address is in the byte offset register of a byte-manipulation
     instruction; 3 means the literal address is in the target
     register of a jsr instruction.  This does not actually do any
     relocation.  */
  HOWTO (ALPHA_R_LITUSE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "LITUSE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Load the gp register.  This is always used for a ldah instruction
     which loads the upper 16 bits of the gp register.  The next reloc
     will be an IGNORE reloc which identifies the location of the lda
     instruction which loads the lower 16 bits.  The symbol index of
     the GPDISP instruction appears to actually be the number of bytes
     between the ldah and lda instructions.  This gives two different
     ways to determine where the lda instruction is; I don't know why
     both are used.  The value to use for the relocation is the
     difference between the GP value and the current location; the
     load will always be done against a register holding the current
     address.  */
  HOWTO (ALPHA_R_GPDISP,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "GPDISP",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 21 bit branch.  The native assembler generates these for
     branches within the text segment, and also fills in the PC
     relative offset in the instruction.  */
  HOWTO (ALPHA_R_BRADDR,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 21,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "BRADDR",		/* name */
	 true,			/* partial_inplace */
	 0x1fffff,		/* src_mask */
	 0x1fffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A hint for a jump to a register.  */
  HOWTO (ALPHA_R_HINT,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 14,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "HINT",		/* name */
	 true,			/* partial_inplace */
	 0x3fff,		/* src_mask */
	 0x3fff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL64",		/* name */
	 true,			/* partial_inplace */
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
	 0,			/* special_function */
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
	 0,			/* special_function */
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
	 0,			/* special_function */
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
	 0,			/* special_function */
	 "OP_PRSHIFT",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Adjust the GP value for a new range in the object file.  */
  HOWTO (ALPHA_R_GPVALUE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "GPVALUE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false)			/* pcrel_offset */
};

/* Recognize an Alpha ECOFF file.  */

static const bfd_target *
alpha_ecoff_object_p (abfd)
     bfd *abfd;
{
  static const bfd_target *ret;

  ret = coff_object_p (abfd);

  if (ret != NULL)
    {
      asection *sec;

      /* Alpha ECOFF has a .pdata section.  The lnnoptr field of the
	 .pdata section is the number of entries it contains.  Each
	 entry takes up 8 bytes.  The number of entries is required
	 since the section is aligned to a 16 byte boundary.  When we
	 link .pdata sections together, we do not want to include the
	 alignment bytes.  We handle this on input by faking the size
	 of the .pdata section to remove the unwanted alignment bytes.
	 On output we will set the lnnoptr field and force the
	 alignment.  */
      sec = bfd_get_section_by_name (abfd, _PDATA);
      if (sec != (asection *) NULL)
	{
	  bfd_size_type size;

	  size = sec->line_filepos * 8;
	  BFD_ASSERT (size == bfd_section_size (abfd, sec)
		      || size + 8 == bfd_section_size (abfd, sec));
	  if (! bfd_set_section_size (abfd, sec, size))
	    return NULL;
	}
    }

  return ret;
}

/* See whether the magic number matches.  */

static boolean
alpha_ecoff_bad_format_hook (abfd, filehdr)
     bfd *abfd;
     PTR filehdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;

  if (ALPHA_ECOFF_BADMAG (*internal_f))
    return false;

  return true;
}

/* This is a hook called by coff_real_object_p to create any backend
   specific information.  */

static PTR
alpha_ecoff_mkobject_hook (abfd, filehdr, aouthdr)
     bfd *abfd;
     PTR filehdr;
     PTR aouthdr;
{
  PTR ecoff;

  ecoff = _bfd_ecoff_mkobject_hook (abfd, filehdr, aouthdr);

  if (ecoff != NULL)
    {
      struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;

      /* Set additional BFD flags according to the object type from the
	 machine specific file header flags.  */
      switch (internal_f->f_flags & F_ALPHA_OBJECT_TYPE_MASK)
	{
	case F_ALPHA_SHARABLE:
	  abfd->flags |= DYNAMIC;
	  break;
	case F_ALPHA_CALL_SHARED:
	  /* Always executable if using shared libraries as the run time
	     loader might resolve undefined references.  */
	  abfd->flags |= (DYNAMIC | EXEC_P);
	  break;
	}
    }
  return ecoff;
}

/* Reloc handling.  */

/* Swap a reloc in.  */

static void
alpha_ecoff_swap_reloc_in (abfd, ext_ptr, intern)
     bfd *abfd;
     PTR ext_ptr;
     struct internal_reloc *intern;
{
  const RELOC *ext = (RELOC *) ext_ptr;

  intern->r_vaddr = bfd_h_get_64 (abfd, (bfd_byte *) ext->r_vaddr);
  intern->r_symndx = bfd_h_get_32 (abfd, (bfd_byte *) ext->r_symndx);

  BFD_ASSERT (abfd->xvec->header_byteorder_big_p == false);

  intern->r_type = ((ext->r_bits[0] & RELOC_BITS0_TYPE_LITTLE)
		    >> RELOC_BITS0_TYPE_SH_LITTLE);
  intern->r_extern = (ext->r_bits[1] & RELOC_BITS1_EXTERN_LITTLE) != 0;
  intern->r_offset = ((ext->r_bits[1] & RELOC_BITS1_OFFSET_LITTLE)
		      >> RELOC_BITS1_OFFSET_SH_LITTLE);
  /* Ignored the reserved bits.  */
  intern->r_size = ((ext->r_bits[3] & RELOC_BITS3_SIZE_LITTLE)
		    >> RELOC_BITS3_SIZE_SH_LITTLE);

  if (intern->r_type == ALPHA_R_LITUSE
      || intern->r_type == ALPHA_R_GPDISP)
    {
      /* Handle the LITUSE and GPDISP relocs specially.  Its symndx
	 value is not actually a symbol index, but is instead a
	 special code.  We put the code in the r_size field, and
	 clobber the symndx.  */
      if (intern->r_size != 0)
	abort ();
      intern->r_size = intern->r_symndx;
      intern->r_symndx = RELOC_SECTION_NONE;
    }
  else if (intern->r_type == ALPHA_R_IGNORE)
    {
      /* The IGNORE reloc generally follows a GPDISP reloc, and is
	 against the .lita section.  The section is irrelevant.  */
      if (! intern->r_extern &&
	  (intern->r_symndx == RELOC_SECTION_NONE
	   || intern->r_symndx == RELOC_SECTION_ABS))
	abort ();
      if (! intern->r_extern && intern->r_symndx == RELOC_SECTION_LITA)
	intern->r_symndx = RELOC_SECTION_NONE;
    }
}

/* Swap a reloc out.  */

static void
alpha_ecoff_swap_reloc_out (abfd, intern, dst)
     bfd *abfd;
     const struct internal_reloc *intern;
     PTR dst;
{
  RELOC *ext = (RELOC *) dst;
  long symndx;
  unsigned char size;

  /* Undo the hackery done in swap_reloc_in.  */
  if (intern->r_type == ALPHA_R_LITUSE
      || intern->r_type == ALPHA_R_GPDISP)
    {
      symndx = intern->r_size;
      size = 0;
    }
  else if (intern->r_type == ALPHA_R_IGNORE
	   && ! intern->r_extern
	   && intern->r_symndx == RELOC_SECTION_NONE)
    {
      symndx = RELOC_SECTION_LITA;
      size = intern->r_size;
    }
  else
    {
      symndx = intern->r_symndx;
      size = intern->r_size;
    }

  BFD_ASSERT (intern->r_extern
	      || (intern->r_symndx >= 0 && intern->r_symndx <= 14));

  bfd_h_put_64 (abfd, intern->r_vaddr, (bfd_byte *) ext->r_vaddr);
  bfd_h_put_32 (abfd, symndx, (bfd_byte *) ext->r_symndx);

  BFD_ASSERT (abfd->xvec->header_byteorder_big_p == false);

  ext->r_bits[0] = ((intern->r_type << RELOC_BITS0_TYPE_SH_LITTLE)
		    & RELOC_BITS0_TYPE_LITTLE);
  ext->r_bits[1] = ((intern->r_extern ? RELOC_BITS1_EXTERN_LITTLE : 0)
		    | ((intern->r_offset << RELOC_BITS1_OFFSET_SH_LITTLE)
		       & RELOC_BITS1_OFFSET_LITTLE));
  ext->r_bits[2] = 0;
  ext->r_bits[3] = ((size << RELOC_BITS3_SIZE_SH_LITTLE)
		    & RELOC_BITS3_SIZE_LITTLE);
}

/* Finish canonicalizing a reloc.  Part of this is generic to all
   ECOFF targets, and that part is in ecoff.c.  The rest is done in
   this backend routine.  It must fill in the howto field.  */

static void
alpha_adjust_reloc_in (abfd, intern, rptr)
     bfd *abfd;
     const struct internal_reloc *intern;
     arelent *rptr;
{
  if (intern->r_type > ALPHA_R_GPVALUE)
    abort ();

  switch (intern->r_type)
    {
    case ALPHA_R_BRADDR:
    case ALPHA_R_SREL16:
    case ALPHA_R_SREL32:
    case ALPHA_R_SREL64:
      /* The PC relative relocs do not seem to use the section VMA as
	 a negative addend.  */
      rptr->addend = 0;
      break;

    case ALPHA_R_GPREL32:
    case ALPHA_R_LITERAL:
      /* Copy the gp value for this object file into the addend, to
	 ensure that we are not confused by the linker.  */
      if (! intern->r_extern)
	rptr->addend += ecoff_data (abfd)->gp;
      break;

    case ALPHA_R_LITUSE:
    case ALPHA_R_GPDISP:
      /* The LITUSE and GPDISP relocs do not use a symbol, or an
	 addend, but they do use a special code.  Put this code in the
	 addend field.  */
      rptr->addend = intern->r_size;
      break;

    case ALPHA_R_OP_STORE:
      /* The STORE reloc needs the size and offset fields.  We store
	 them in the addend.  */
      BFD_ASSERT (intern->r_offset <= 256 && intern->r_size <= 256);
      rptr->addend = (intern->r_offset << 8) + intern->r_size;
      break;

    case ALPHA_R_OP_PUSH:
    case ALPHA_R_OP_PSUB:
    case ALPHA_R_OP_PRSHIFT:
      /* The PUSH, PSUB and PRSHIFT relocs do not actually use an
	 address.  I believe that the address supplied is really an
	 addend.  */
      rptr->addend = intern->r_vaddr;
      break;

    case ALPHA_R_GPVALUE:
      /* Set the addend field to the new GP value.  */
      rptr->addend = intern->r_symndx + ecoff_data (abfd)->gp;
      break;

    case ALPHA_R_IGNORE:
      /* If the type is ALPHA_R_IGNORE, make sure this is a reference
	 to the absolute section so that the reloc is ignored.  For
	 some reason the address of this reloc type is not adjusted by
	 the section vma.  We record the gp value for this object file
	 here, for convenience when doing the GPDISP relocation.  */
      rptr->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
      rptr->address = intern->r_vaddr;
      rptr->addend = ecoff_data (abfd)->gp;
      break;

    default:
      break;
    }

  rptr->howto = &alpha_howto_table[intern->r_type];
}

/* When writing out a reloc we need to pull some values back out of
   the addend field into the reloc.  This is roughly the reverse of
   alpha_adjust_reloc_in, except that there are several changes we do
   not need to undo.  */

static void
alpha_adjust_reloc_out (abfd, rel, intern)
     bfd *abfd;
     const arelent *rel;
     struct internal_reloc *intern;
{
  switch (intern->r_type)
    {
    case ALPHA_R_LITUSE:
    case ALPHA_R_GPDISP:
      intern->r_size = rel->addend;
      break;

    case ALPHA_R_OP_STORE:
      intern->r_size = rel->addend & 0xff;
      intern->r_offset = (rel->addend >> 8) & 0xff;
      break;

    case ALPHA_R_OP_PUSH:
    case ALPHA_R_OP_PSUB:
    case ALPHA_R_OP_PRSHIFT:
      intern->r_vaddr = rel->addend;
      break;

    case ALPHA_R_IGNORE:
      intern->r_vaddr = rel->address;
      if (intern->r_symndx == RELOC_SECTION_ABS)
	intern->r_symndx = RELOC_SECTION_NONE;
      break;

    default:
      break;
    }
}

/* The size of the stack for the relocation evaluator.  */
#define RELOC_STACKSIZE (10)

/* Alpha ECOFF relocs have a built in expression evaluator as well as
   other interdependencies.  Rather than use a bunch of special
   functions and global variables, we use a single routine to do all
   the relocation for a section.  I haven't yet worked out how the
   assembler is going to handle this.  */

static bfd_byte *
alpha_ecoff_get_relocated_section_contents (abfd, link_info, link_order,
					    data, relocateable, symbols)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  bfd *input_bfd = link_order->u.indirect.section->owner;
  asection *input_section = link_order->u.indirect.section;
  long reloc_size = bfd_get_reloc_upper_bound (input_bfd, input_section);
  arelent **reloc_vector = NULL;
  long reloc_count;
  bfd *output_bfd = relocateable ? abfd : (bfd *) NULL;
  bfd_vma gp;
  boolean gp_undefined;
  bfd_vma stack[RELOC_STACKSIZE];
  int tos = 0;

  if (reloc_size < 0)
    goto error_return;
  reloc_vector = (arelent **) malloc (reloc_size);
  if (reloc_vector == NULL && reloc_size != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  if (! bfd_get_section_contents (input_bfd, input_section, data,
				  (file_ptr) 0, input_section->_raw_size))
    goto error_return;

  /* The section size is not going to change.  */
  input_section->_cooked_size = input_section->_raw_size;
  input_section->reloc_done = true;

  reloc_count = bfd_canonicalize_reloc (input_bfd, input_section,
					reloc_vector, symbols);
  if (reloc_count < 0)
    goto error_return;
  if (reloc_count == 0)
    goto successful_return;

  /* Get the GP value for the output BFD.  */
  gp_undefined = false;
  if (ecoff_data (abfd)->gp == 0)
    {
      if (relocateable != false)
	{
	  asection *sec;
	  bfd_vma lo;

	  /* Make up a value.  */
	  lo = (bfd_vma) -1;
	  for (sec = abfd->sections; sec != NULL; sec = sec->next)
	    {
	      if (sec->vma < lo
		  && (strcmp (sec->name, ".sbss") == 0
		      || strcmp (sec->name, ".sdata") == 0
		      || strcmp (sec->name, ".lit4") == 0
		      || strcmp (sec->name, ".lit8") == 0
		      || strcmp (sec->name, ".lita") == 0))
		lo = sec->vma;
	    }
	  ecoff_data (abfd)->gp = lo + 0x8000;
	}
      else
	{
	  struct bfd_link_hash_entry *h;

	  h = bfd_link_hash_lookup (link_info->hash, "_gp", false, false,
				    true);
	  if (h == (struct bfd_link_hash_entry *) NULL
	      || h->type != bfd_link_hash_defined)
	    gp_undefined = true;
	  else
	    ecoff_data (abfd)->gp = (h->u.def.value
				     + h->u.def.section->output_section->vma
				     + h->u.def.section->output_offset);
	}
    }
  gp = ecoff_data (abfd)->gp;

  for (; *reloc_vector != (arelent *) NULL; reloc_vector++)
    {
      arelent *rel;
      bfd_reloc_status_type r;
      char *err;

      rel = *reloc_vector;
      r = bfd_reloc_ok;
      switch (rel->howto->type)
	{
	case ALPHA_R_IGNORE:
	  rel->address += input_section->output_offset;
	  break;

	case ALPHA_R_REFLONG:
	case ALPHA_R_REFQUAD:
	case ALPHA_R_BRADDR:
	case ALPHA_R_HINT:
	case ALPHA_R_SREL16:
	case ALPHA_R_SREL32:
	case ALPHA_R_SREL64:
	  if (relocateable
	      && ((*rel->sym_ptr_ptr)->flags & BSF_SECTION_SYM) == 0)
	    {
	      rel->address += input_section->output_offset;
	      break;
	    }
	  r = bfd_perform_relocation (input_bfd, rel, data, input_section,
				      output_bfd, &err);
	  break;

	case ALPHA_R_GPREL32:
	  /* This relocation is used in a switch table.  It is a 32
	     bit offset from the current GP value.  We must adjust it
	     by the different between the original GP value and the
	     current GP value.  The original GP value is stored in the
	     addend.  We adjust the addend and let
	     bfd_perform_relocation finish the job.  */
	  rel->addend -= gp;
	  r = bfd_perform_relocation (input_bfd, rel, data, input_section,
				      output_bfd, &err);
	  if (r == bfd_reloc_ok && gp_undefined)
	    {
	      r = bfd_reloc_dangerous;
	      err = (char *) "GP relative relocation used when GP not defined";
	    }
	  break;

	case ALPHA_R_LITERAL:
	  /* This is a reference to a literal value, generally
	     (always?) in the .lita section.  This is a 16 bit GP
	     relative relocation.  Sometimes the subsequent reloc is a
	     LITUSE reloc, which indicates how this reloc is used.
	     This sometimes permits rewriting the two instructions
	     referred to by the LITERAL and the LITUSE into different
	     instructions which do not refer to .lita.  This can save
	     a memory reference, and permits removing a value from
	     .lita thus saving GP relative space.

	     We do not these optimizations.  To do them we would need
	     to arrange to link the .lita section first, so that by
	     the time we got here we would know the final values to
	     use.  This would not be particularly difficult, but it is
	     not currently implemented.  */

	  {
	    unsigned long insn;

	    /* I believe that the LITERAL reloc will only apply to a
	       ldq or ldl instruction, so check my assumption.  */
	    insn = bfd_get_32 (input_bfd, data + rel->address);
	    BFD_ASSERT (((insn >> 26) & 0x3f) == 0x29
			|| ((insn >> 26) & 0x3f) == 0x28);

	    rel->addend -= gp;
	    r = bfd_perform_relocation (input_bfd, rel, data, input_section,
					output_bfd, &err);
	    if (r == bfd_reloc_ok && gp_undefined)
	      {
		r = bfd_reloc_dangerous;
		err =
		  (char *) "GP relative relocation used when GP not defined";
	      }
	  }
	  break;

	case ALPHA_R_LITUSE:
	  /* See ALPHA_R_LITERAL above for the uses of this reloc.  It
	     does not cause anything to happen, itself.  */
	  rel->address += input_section->output_offset;
	  break;
	    
	case ALPHA_R_GPDISP:
	  /* This marks the ldah of an ldah/lda pair which loads the
	     gp register with the difference of the gp value and the
	     current location.  The second of the pair is r_size bytes
	     ahead, and is marked with an ALPHA_R_IGNORE reloc.  */
	  {
	    unsigned long insn1, insn2;
	    bfd_vma addend;

	    BFD_ASSERT (reloc_vector[1] != NULL
			&& reloc_vector[1]->howto->type == ALPHA_R_IGNORE
			&& (rel->address + rel->addend
			    == reloc_vector[1]->address));

	    /* Get the two instructions.  */
	    insn1 = bfd_get_32 (input_bfd, data + rel->address);
	    insn2 = bfd_get_32 (input_bfd, data + rel->address + rel->addend);

	    BFD_ASSERT (((insn1 >> 26) & 0x3f) == 0x09); /* ldah */
	    BFD_ASSERT (((insn2 >> 26) & 0x3f) == 0x08); /* lda */

	    /* Get the existing addend.  We must account for the sign
	       extension done by lda and ldah.  */
	    addend = ((insn1 & 0xffff) << 16) + (insn2 & 0xffff);
	    if (insn1 & 0x8000)
	      {
		addend -= 0x80000000;
		addend -= 0x80000000;
	      }
	    if (insn2 & 0x8000)
	      addend -= 0x10000;

	    /* The existing addend includes the different between the
	       gp of the input BFD and the address in the input BFD.
	       Subtract this out.  */
	    addend -= (reloc_vector[1]->addend
		       - (input_section->vma + rel->address));

	    /* Now add in the final gp value, and subtract out the
	       final address.  */
	    addend += (gp
		       - (input_section->output_section->vma
			  + input_section->output_offset
			  + rel->address));

	    /* Change the instructions, accounting for the sign
	       extension, and write them out.  */
	    if (addend & 0x8000)
	      addend += 0x10000;
	    insn1 = (insn1 & 0xffff0000) | ((addend >> 16) & 0xffff);
	    insn2 = (insn2 & 0xffff0000) | (addend & 0xffff);

	    bfd_put_32 (input_bfd, (bfd_vma) insn1, data + rel->address);
	    bfd_put_32 (input_bfd, (bfd_vma) insn2,
			data + rel->address + rel->addend);

	    rel->address += input_section->output_offset;
	  }
	  break;
	  
	case ALPHA_R_OP_PUSH:
	  /* Push a value on the reloc evaluation stack.  */
	  {
	    asymbol *symbol;
	    bfd_vma relocation;

	    if (relocateable)
	      {
		rel->address += input_section->output_offset;
		break;
	      }

	    /* Figure out the relocation of this symbol.  */
	    symbol = *rel->sym_ptr_ptr;

	    if (bfd_is_und_section (symbol->section))
	      r = bfd_reloc_undefined;

	    if (bfd_is_com_section (symbol->section))
	      relocation = 0;
	    else
	      relocation = symbol->value;
	    relocation += symbol->section->output_section->vma;
	    relocation += symbol->section->output_offset;
	    relocation += rel->addend;

	    if (tos >= RELOC_STACKSIZE)
	      abort ();

	    stack[tos++] = relocation;
	  }
	  break;

	case ALPHA_R_OP_STORE:
	  /* Store a value from the reloc stack into a bitfield.  */
	  {
	    bfd_vma val;
	    int offset, size;

	    if (relocateable)
	      {
		rel->address += input_section->output_offset;
		break;
	      }

	    if (tos == 0)
	      abort ();

	    /* The offset and size for this reloc are encoded into the
	       addend field by alpha_adjust_reloc_in.  */
	    offset = (rel->addend >> 8) & 0xff;
	    size = rel->addend & 0xff;

	    val = bfd_get_64 (abfd, data + rel->address);
	    val &=~ (((1 << size) - 1) << offset);
	    val |= (stack[--tos] & ((1 << size) - 1)) << offset;
	    bfd_put_64 (abfd, val, data + rel->address);
	  }
	  break;

	case ALPHA_R_OP_PSUB:
	  /* Subtract a value from the top of the stack.  */
	  {
	    asymbol *symbol;
	    bfd_vma relocation;

	    if (relocateable)
	      {
		rel->address += input_section->output_offset;
		break;
	      }

	    /* Figure out the relocation of this symbol.  */
	    symbol = *rel->sym_ptr_ptr;

	    if (bfd_is_und_section (symbol->section))
	      r = bfd_reloc_undefined;

	    if (bfd_is_com_section (symbol->section))
	      relocation = 0;
	    else
	      relocation = symbol->value;
	    relocation += symbol->section->output_section->vma;
	    relocation += symbol->section->output_offset;
	    relocation += rel->addend;

	    if (tos == 0)
	      abort ();

	    stack[tos - 1] -= relocation;
	  }
	  break;

	case ALPHA_R_OP_PRSHIFT:
	  /* Shift the value on the top of the stack.  */
	  {
	    asymbol *symbol;
	    bfd_vma relocation;

	    if (relocateable)
	      {
		rel->address += input_section->output_offset;
		break;
	      }

	    /* Figure out the relocation of this symbol.  */
	    symbol = *rel->sym_ptr_ptr;

	    if (bfd_is_und_section (symbol->section))
	      r = bfd_reloc_undefined;

	    if (bfd_is_com_section (symbol->section))
	      relocation = 0;
	    else
	      relocation = symbol->value;
	    relocation += symbol->section->output_section->vma;
	    relocation += symbol->section->output_offset;
	    relocation += rel->addend;

	    if (tos == 0)
	      abort ();

	    stack[tos - 1] >>= relocation;
	  }
	  break;
	    
	case ALPHA_R_GPVALUE:
	  /* I really don't know if this does the right thing.  */
	  gp = rel->addend;
	  gp_undefined = false;
	  break;

	default:
	  abort ();
	}

      if (relocateable)
	{
	  asection *os = input_section->output_section;

	  /* A partial link, so keep the relocs.  */
	  os->orelocation[os->reloc_count] = rel;
	  os->reloc_count++;
	}

      if (r != bfd_reloc_ok) 
	{
	  switch (r)
	    {
	    case bfd_reloc_undefined:
	      if (! ((*link_info->callbacks->undefined_symbol)
		     (link_info, bfd_asymbol_name (*rel->sym_ptr_ptr),
		      input_bfd, input_section, rel->address)))
		goto error_return;
	      break;
	    case bfd_reloc_dangerous: 
	      if (! ((*link_info->callbacks->reloc_dangerous)
		     (link_info, err, input_bfd, input_section,
		      rel->address)))
		goto error_return;
	      break;
	    case bfd_reloc_overflow:
	      if (! ((*link_info->callbacks->reloc_overflow)
		     (link_info, bfd_asymbol_name (*rel->sym_ptr_ptr),
		      rel->howto->name, rel->addend, input_bfd,
		      input_section, rel->address)))
		goto error_return;
	      break;
	    case bfd_reloc_outofrange:
	    default:
	      abort ();
	      break;
	    }
	}
    }

  if (tos != 0)
    abort ();

 successful_return:
  if (reloc_vector != NULL)
    free (reloc_vector);
  return data;

 error_return:
  if (reloc_vector != NULL)
    free (reloc_vector);
  return NULL;
}

/* Get the howto structure for a generic reloc type.  */

static reloc_howto_type *
alpha_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  int alpha_type;

  switch (code)
    {
    case BFD_RELOC_32:
      alpha_type = ALPHA_R_REFLONG;
      break;
    case BFD_RELOC_64:
    case BFD_RELOC_CTOR:
      alpha_type = ALPHA_R_REFQUAD;
      break;
    case BFD_RELOC_GPREL32:
      alpha_type = ALPHA_R_GPREL32;
      break;
    case BFD_RELOC_ALPHA_LITERAL:
      alpha_type = ALPHA_R_LITERAL;
      break;
    case BFD_RELOC_ALPHA_LITUSE:
      alpha_type = ALPHA_R_LITUSE;
      break;
    case BFD_RELOC_ALPHA_GPDISP_HI16:
      alpha_type = ALPHA_R_GPDISP;
      break;
    case BFD_RELOC_ALPHA_GPDISP_LO16:
      alpha_type = ALPHA_R_IGNORE;
      break;
    case BFD_RELOC_23_PCREL_S2:
      alpha_type = ALPHA_R_BRADDR;
      break;
    case BFD_RELOC_ALPHA_HINT:
      alpha_type = ALPHA_R_HINT;
      break;
    case BFD_RELOC_16_PCREL:
      alpha_type = ALPHA_R_SREL16;
      break;
    case BFD_RELOC_32_PCREL:
      alpha_type = ALPHA_R_SREL32;
      break;
    case BFD_RELOC_64_PCREL:
      alpha_type = ALPHA_R_SREL64;
      break;
#if 0
    case ???:
      alpha_type = ALPHA_R_OP_PUSH;
      break;
    case ???:
      alpha_type = ALPHA_R_OP_STORE;
      break;
    case ???:
      alpha_type = ALPHA_R_OP_PSUB;
      break;
    case ???:
      alpha_type = ALPHA_R_OP_PRSHIFT;
      break;
    case ???:
      alpha_type = ALPHA_R_GPVALUE;
      break;
#endif
    default:
      return (reloc_howto_type *) NULL;
    }

  return &alpha_howto_table[alpha_type];
}

/* A helper routine for alpha_relocate_section which converts an
   external reloc when generating relocateable output.  Returns the
   relocation amount.  */

static bfd_vma
alpha_convert_external_reloc (output_bfd, info, input_bfd, ext_rel, h)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     struct external_reloc *ext_rel;
     struct ecoff_link_hash_entry *h;
{
  unsigned long r_symndx;
  bfd_vma relocation;

  BFD_ASSERT (info->relocateable);

  if (h->root.type == bfd_link_hash_defined
      || h->root.type == bfd_link_hash_defweak)
    {
      asection *hsec;
      const char *name;

      /* This symbol is defined in the output.  Convert the reloc from
	 being against the symbol to being against the section.  */

      /* Clear the r_extern bit.  */
      ext_rel->r_bits[1] &=~ RELOC_BITS1_EXTERN_LITTLE;

      /* Compute a new r_symndx value.  */
      hsec = h->root.u.def.section;
      name = bfd_get_section_name (output_bfd, hsec->output_section);

      r_symndx = -1;
      switch (name[1])
	{
	case 'A':
	  if (strcmp (name, "*ABS*") == 0)
	    r_symndx = RELOC_SECTION_ABS;
	  break;
	case 'b':
	  if (strcmp (name, ".bss") == 0)
	    r_symndx = RELOC_SECTION_BSS;
	  break;
	case 'd':
	  if (strcmp (name, ".data") == 0)
	    r_symndx = RELOC_SECTION_DATA;
	  break;
	case 'f':
	  if (strcmp (name, ".fini") == 0)
	    r_symndx = RELOC_SECTION_FINI;
	  break;
	case 'i':
	  if (strcmp (name, ".init") == 0)
	    r_symndx = RELOC_SECTION_INIT;
	  break;
	case 'l':
	  if (strcmp (name, ".lita") == 0)
	    r_symndx = RELOC_SECTION_LITA;
	  else if (strcmp (name, ".lit8") == 0)
	    r_symndx = RELOC_SECTION_LIT8;
	  else if (strcmp (name, ".lit4") == 0)
	    r_symndx = RELOC_SECTION_LIT4;
	  break;
	case 'p':
	  if (strcmp (name, ".pdata") == 0)
	    r_symndx = RELOC_SECTION_PDATA;
	  break;
	case 'r':
	  if (strcmp (name, ".rdata") == 0)
	    r_symndx = RELOC_SECTION_RDATA;
	  break;
	case 's':
	  if (strcmp (name, ".sdata") == 0)
	    r_symndx = RELOC_SECTION_SDATA;
	  else if (strcmp (name, ".sbss") == 0)
	    r_symndx = RELOC_SECTION_SBSS;
	  break;
	case 't':
	  if (strcmp (name, ".text") == 0)
	    r_symndx = RELOC_SECTION_TEXT;
	  break;
	case 'x':
	  if (strcmp (name, ".xdata") == 0)
	    r_symndx = RELOC_SECTION_XDATA;
	  break;
	}
		      
      if (r_symndx == -1)
	abort ();

      /* Add the section VMA and the symbol value.  */
      relocation = (h->root.u.def.value
		    + hsec->output_section->vma
		    + hsec->output_offset);
    }
  else
    {
      /* Change the symndx value to the right one for
	 the output BFD.  */
      r_symndx = h->indx;
      if (r_symndx == -1)
	{
	  /* Caller must give an error.  */
	  r_symndx = 0;
	}
      relocation = 0;
    }

  /* Write out the new r_symndx value.  */
  bfd_h_put_32 (input_bfd, (bfd_vma) r_symndx,
		(bfd_byte *) ext_rel->r_symndx);

  return relocation;
}

/* Relocate a section while linking an Alpha ECOFF file.  This is
   quite similar to get_relocated_section_contents.  Perhaps they
   could be combined somehow.  */

static boolean
alpha_relocate_section (output_bfd, info, input_bfd, input_section,
			contents, external_relocs)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     PTR external_relocs;
{
  asection **symndx_to_section;
  struct ecoff_link_hash_entry **sym_hashes;
  bfd_vma gp;
  boolean gp_undefined;
  bfd_vma stack[RELOC_STACKSIZE];
  int tos = 0;
  struct external_reloc *ext_rel;
  struct external_reloc *ext_rel_end;

  /* We keep a table mapping the symndx found in an internal reloc to
     the appropriate section.  This is faster than looking up the
     section by name each time.  */
  symndx_to_section = ecoff_data (input_bfd)->symndx_to_section;
  if (symndx_to_section == (asection **) NULL)
    {
      symndx_to_section = ((asection **)
			   bfd_alloc (input_bfd,
				      (NUM_RELOC_SECTIONS
				       * sizeof (asection *))));
      if (!symndx_to_section)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return false;
	}

      symndx_to_section[RELOC_SECTION_NONE] = NULL;
      symndx_to_section[RELOC_SECTION_TEXT] =
	bfd_get_section_by_name (input_bfd, ".text");
      symndx_to_section[RELOC_SECTION_RDATA] =
	bfd_get_section_by_name (input_bfd, ".rdata");
      symndx_to_section[RELOC_SECTION_DATA] =
	bfd_get_section_by_name (input_bfd, ".data");
      symndx_to_section[RELOC_SECTION_SDATA] =
	bfd_get_section_by_name (input_bfd, ".sdata");
      symndx_to_section[RELOC_SECTION_SBSS] =
	bfd_get_section_by_name (input_bfd, ".sbss");
      symndx_to_section[RELOC_SECTION_BSS] =
	bfd_get_section_by_name (input_bfd, ".bss");
      symndx_to_section[RELOC_SECTION_INIT] =
	bfd_get_section_by_name (input_bfd, ".init");
      symndx_to_section[RELOC_SECTION_LIT8] =
	bfd_get_section_by_name (input_bfd, ".lit8");
      symndx_to_section[RELOC_SECTION_LIT4] =
	bfd_get_section_by_name (input_bfd, ".lit4");
      symndx_to_section[RELOC_SECTION_XDATA] =
	bfd_get_section_by_name (input_bfd, ".xdata");
      symndx_to_section[RELOC_SECTION_PDATA] =
	bfd_get_section_by_name (input_bfd, ".pdata");
      symndx_to_section[RELOC_SECTION_FINI] =
	bfd_get_section_by_name (input_bfd, ".fini");
      symndx_to_section[RELOC_SECTION_LITA] =
	bfd_get_section_by_name (input_bfd, ".lita");
      symndx_to_section[RELOC_SECTION_ABS] = bfd_abs_section_ptr;

      ecoff_data (input_bfd)->symndx_to_section = symndx_to_section;
    }

  sym_hashes = ecoff_data (input_bfd)->sym_hashes;

  gp = ecoff_data (output_bfd)->gp;
  if (gp == 0)
    gp_undefined = true;
  else
    gp_undefined = false;

  BFD_ASSERT (output_bfd->xvec->header_byteorder_big_p == false);
  BFD_ASSERT (input_bfd->xvec->header_byteorder_big_p == false);

  ext_rel = (struct external_reloc *) external_relocs;
  ext_rel_end = ext_rel + input_section->reloc_count;
  for (; ext_rel < ext_rel_end; ext_rel++)
    {
      bfd_vma r_vaddr;
      unsigned long r_symndx;
      int r_type;
      int r_extern;
      int r_offset;
      int r_size;
      boolean relocatep;
      boolean adjust_addrp;
      boolean gp_usedp;
      bfd_vma addend;

      r_vaddr = bfd_h_get_64 (input_bfd, (bfd_byte *) ext_rel->r_vaddr);
      r_symndx = bfd_h_get_32 (input_bfd, (bfd_byte *) ext_rel->r_symndx);

      r_type = ((ext_rel->r_bits[0] & RELOC_BITS0_TYPE_LITTLE)
		>> RELOC_BITS0_TYPE_SH_LITTLE);
      r_extern = (ext_rel->r_bits[1] & RELOC_BITS1_EXTERN_LITTLE) != 0;
      r_offset = ((ext_rel->r_bits[1] & RELOC_BITS1_OFFSET_LITTLE)
		  >> RELOC_BITS1_OFFSET_SH_LITTLE);
      /* Ignored the reserved bits.  */
      r_size = ((ext_rel->r_bits[3] & RELOC_BITS3_SIZE_LITTLE)
		>> RELOC_BITS3_SIZE_SH_LITTLE);

      relocatep = false;
      adjust_addrp = true;
      gp_usedp = false;
      addend = 0;

      switch (r_type)
	{
	default:
	  abort ();

	case ALPHA_R_IGNORE:
	  /* This reloc appears after a GPDISP reloc.  It marks the
	     position of the second instruction to be altered by the
	     GPDISP reloc, but is not otherwise used for anything.
	     For some reason, the address of the relocation does not
	     appear to include the section VMA, unlike the other
	     relocation types.  */
	  if (info->relocateable)
	    bfd_h_put_64 (input_bfd,
			  input_section->output_offset + r_vaddr,
			  (bfd_byte *) ext_rel->r_vaddr);
	  adjust_addrp = false;
	  break;

	case ALPHA_R_REFLONG:
	case ALPHA_R_REFQUAD:
	case ALPHA_R_BRADDR:
	case ALPHA_R_HINT:
	case ALPHA_R_SREL16:
	case ALPHA_R_SREL32:
	case ALPHA_R_SREL64:
	  relocatep = true;
	  break;

	case ALPHA_R_GPREL32:
	  /* This relocation is used in a switch table.  It is a 32
	     bit offset from the current GP value.  We must adjust it
	     by the different between the original GP value and the
	     current GP value.  */
	  relocatep = true;
	  addend = ecoff_data (input_bfd)->gp - gp;
	  gp_usedp = true;
	  break;

	case ALPHA_R_LITERAL:
	  /* This is a reference to a literal value, generally
	     (always?) in the .lita section.  This is a 16 bit GP
	     relative relocation.  Sometimes the subsequent reloc is a
	     LITUSE reloc, which indicates how this reloc is used.
	     This sometimes permits rewriting the two instructions
	     referred to by the LITERAL and the LITUSE into different
	     instructions which do not refer to .lita.  This can save
	     a memory reference, and permits removing a value from
	     .lita thus saving GP relative space.

	     We do not these optimizations.  To do them we would need
	     to arrange to link the .lita section first, so that by
	     the time we got here we would know the final values to
	     use.  This would not be particularly difficult, but it is
	     not currently implemented.  */

	  /* I believe that the LITERAL reloc will only apply to a ldq
	     or ldl instruction, so check my assumption.  */
	  {
	    unsigned long insn;

	    insn = bfd_get_32 (input_bfd,
			       contents + r_vaddr - input_section->vma);
	    BFD_ASSERT (((insn >> 26) & 0x3f) == 0x29
			|| ((insn >> 26) & 0x3f) == 0x28);
	  }

	  relocatep = true;
	  addend = ecoff_data (input_bfd)->gp - gp;
	  gp_usedp = true;
	  break;

	case ALPHA_R_LITUSE:
	  /* See ALPHA_R_LITERAL above for the uses of this reloc.  It
	     does not cause anything to happen, itself.  */
	  break;
	    
	case ALPHA_R_GPDISP:
	  /* This marks the ldah of an ldah/lda pair which loads the
	     gp register with the difference of the gp value and the
	     current location.  The second of the pair is r_symndx
	     bytes ahead, and is also marked with an ALPHA_R_IGNORE
	     reloc.  */
	  {
	    unsigned long insn1, insn2;

	    BFD_ASSERT (ext_rel + 1 < ext_rel_end
			&& (((ext_rel + 1)->r_bits[0]
			     & RELOC_BITS0_TYPE_LITTLE)
			    >> RELOC_BITS0_TYPE_SH_LITTLE) == ALPHA_R_IGNORE
			&& (bfd_h_get_64 (input_bfd,
					  (bfd_byte *) (ext_rel + 1)->r_vaddr)
			    == r_vaddr - input_section->vma + r_symndx));

	    /* Get the two instructions.  */
	    insn1 = bfd_get_32 (input_bfd,
				contents + r_vaddr - input_section->vma);
	    insn2 = bfd_get_32 (input_bfd,
				(contents
				 + r_vaddr
				 - input_section->vma
				 + r_symndx));

	    BFD_ASSERT (((insn1 >> 26) & 0x3f) == 0x09); /* ldah */
	    BFD_ASSERT (((insn2 >> 26) & 0x3f) == 0x08); /* lda */

	    /* Get the existing addend.  We must account for the sign
	       extension done by lda and ldah.  */
	    addend = ((insn1 & 0xffff) << 16) + (insn2 & 0xffff);
	    if (insn1 & 0x8000)
	      {
		/* This is addend -= 0x100000000 without causing an
		   integer overflow on a 32 bit host.  */
		addend -= 0x80000000;
		addend -= 0x80000000;
	      }
	    if (insn2 & 0x8000)
	      addend -= 0x10000;

	    /* The existing addend includes the difference between the
	       gp of the input BFD and the address in the input BFD.
	       We want to change this to the difference between the
	       final GP and the final address.  */
	    addend += (gp
		       - ecoff_data (input_bfd)->gp
		       + input_section->vma
		       - (input_section->output_section->vma
			  + input_section->output_offset));

	    /* Change the instructions, accounting for the sign
	       extension, and write them out.  */
	    if (addend & 0x8000)
	      addend += 0x10000;
	    insn1 = (insn1 & 0xffff0000) | ((addend >> 16) & 0xffff);
	    insn2 = (insn2 & 0xffff0000) | (addend & 0xffff);

	    bfd_put_32 (input_bfd, (bfd_vma) insn1,
			contents + r_vaddr - input_section->vma);
	    bfd_put_32 (input_bfd, (bfd_vma) insn2,
			contents + r_vaddr - input_section->vma + r_symndx);

	    gp_usedp = true;
	  }
	  break;
	  
	case ALPHA_R_OP_PUSH:
	case ALPHA_R_OP_PSUB:
	case ALPHA_R_OP_PRSHIFT:
	  /* Manipulate values on the reloc evaluation stack.  The
	     r_vaddr field is not an address in input_section, it is
	     the current value (including any addend) of the object
	     being used.  */
	  if (! r_extern)
	    {
	      asection *s;

	      s = symndx_to_section[r_symndx];
	      if (s == (asection *) NULL)
		abort ();
	      addend = s->output_section->vma + s->output_offset - s->vma;
	    }
	  else
	    {
	      struct ecoff_link_hash_entry *h;

	      h = sym_hashes[r_symndx];
	      if (h == (struct ecoff_link_hash_entry *) NULL)
		abort ();

	      if (! info->relocateable)
		{
		  if (h->root.type == bfd_link_hash_defined
		      || h->root.type == bfd_link_hash_defweak)
		    addend = (h->root.u.def.value
			      + h->root.u.def.section->output_section->vma
			      + h->root.u.def.section->output_offset);
		  else
		    {
		      /* Note that we pass the address as 0, since we
			 do not have a meaningful number for the
			 location within the section that is being
			 relocated.  */
		      if (! ((*info->callbacks->undefined_symbol)
			     (info, h->root.root.string, input_bfd,
			      input_section, (bfd_vma) 0)))
			return false;
		      addend = 0;
		    }
		}
	      else
		{
		  if (h->root.type != bfd_link_hash_defined
		      && h->root.type != bfd_link_hash_defweak
		      && h->indx == -1)
		    {
		      /* This symbol is not being written out.  Pass
			 the address as 0, as with undefined_symbol,
			 above.  */
		      if (! ((*info->callbacks->unattached_reloc)
			     (info, h->root.root.string, input_bfd,
			      input_section, (bfd_vma) 0)))
			return false;
		    }

		  addend = alpha_convert_external_reloc (output_bfd, info,
							 input_bfd,
							 ext_rel, h);
		}
	    }

	  addend += r_vaddr;

	  if (info->relocateable)
	    {
	      /* Adjust r_vaddr by the addend.  */
	      bfd_h_put_64 (input_bfd, addend,
			    (bfd_byte *) ext_rel->r_vaddr);
	    }
	  else
	    {
	      switch (r_type)
		{
		case ALPHA_R_OP_PUSH:
		  if (tos >= RELOC_STACKSIZE)
		    abort ();
		  stack[tos++] = addend;
		  break;

		case ALPHA_R_OP_PSUB:
		  if (tos == 0)
		    abort ();
		  stack[tos - 1] -= addend;
		  break;

		case ALPHA_R_OP_PRSHIFT:
		  if (tos == 0)
		    abort ();
		  stack[tos - 1] >>= addend;
		  break;
		}
	    }

	  adjust_addrp = false;
	  break;

	case ALPHA_R_OP_STORE:
	  /* Store a value from the reloc stack into a bitfield.  If
	     we are generating relocateable output, all we do is
	     adjust the address of the reloc.  */
	  if (! info->relocateable)
	    {
	      bfd_vma mask;
	      bfd_vma val;

	      if (tos == 0)
		abort ();

	      /* Get the relocation mask.  The separate steps and the
		 casts to bfd_vma are attempts to avoid a bug in the
		 Alpha OSF 1.3 C compiler.  See reloc.c for more
		 details.  */
	      mask = 1;
	      mask <<= (bfd_vma) r_size;
	      mask -= 1;

	      /* FIXME: I don't know what kind of overflow checking,
		 if any, should be done here.  */
	      val = bfd_get_64 (input_bfd,
				contents + r_vaddr - input_section->vma);
	      val &=~ mask << (bfd_vma) r_offset;
	      val |= (stack[--tos] & mask) << (bfd_vma) r_offset;
	      bfd_put_64 (input_bfd, val,
			  contents + r_vaddr - input_section->vma);
	    }
	  break;

	case ALPHA_R_GPVALUE:
	  /* I really don't know if this does the right thing.  */
	  gp = ecoff_data (input_bfd)->gp + r_symndx;
	  gp_undefined = false;
	  break;
	}

      if (relocatep)
	{
	  reloc_howto_type *howto;
	  struct ecoff_link_hash_entry *h = NULL;
	  asection *s = NULL;
	  bfd_vma relocation;
	  bfd_reloc_status_type r;

	  /* Perform a relocation.  */

	  howto = &alpha_howto_table[r_type];

	  if (r_extern)
	    {
	      h = sym_hashes[r_symndx];
	      /* If h is NULL, that means that there is a reloc
		 against an external symbol which we thought was just
		 a debugging symbol.  This should not happen.  */
	      if (h == (struct ecoff_link_hash_entry *) NULL)
		abort ();
	    }
	  else
	    {
	      if (r_symndx >= NUM_RELOC_SECTIONS)
		s = NULL;
	      else
		s = symndx_to_section[r_symndx];

	      if (s == (asection *) NULL)
		abort ();
	    }

	  if (info->relocateable)
	    {
	      /* We are generating relocateable output, and must
		 convert the existing reloc.  */
	      if (r_extern)
		{
		  if (h->root.type != bfd_link_hash_defined
		      && h->root.type != bfd_link_hash_defweak
		      && h->indx == -1)
		    {
		      /* This symbol is not being written out.  */
		      if (! ((*info->callbacks->unattached_reloc)
			     (info, h->root.root.string, input_bfd,
			      input_section, r_vaddr - input_section->vma)))
			return false;
		    }

		  relocation = alpha_convert_external_reloc (output_bfd,
							     info,
							     input_bfd,
							     ext_rel,
							     h);
		}
	      else
		{
		  /* This is a relocation against a section.  Adjust
		     the value by the amount the section moved.  */
		  relocation = (s->output_section->vma
				+ s->output_offset
				- s->vma);
		}

	      /* If this is PC relative, the existing object file
		 appears to already have the reloc worked out.  We
		 must subtract out the old value and add in the new
		 one.  */
	      if (howto->pc_relative)
		relocation -= (input_section->output_section->vma
			       + input_section->output_offset
			       - input_section->vma);

	      /* Put in any addend.  */
	      relocation += addend;

	      /* Adjust the contents.  */
	      r = _bfd_relocate_contents (howto, input_bfd, relocation,
					  (contents
					   + r_vaddr
					   - input_section->vma));
	    }
	  else
	    {
	      /* We are producing a final executable.  */
	      if (r_extern)
		{
		  /* This is a reloc against a symbol.  */
		  if (h->root.type == bfd_link_hash_defined
		      || h->root.type == bfd_link_hash_defweak)
		    {
		      asection *hsec;

		      hsec = h->root.u.def.section;
		      relocation = (h->root.u.def.value
				    + hsec->output_section->vma
				    + hsec->output_offset);
		    }
		  else
		    {
		      if (! ((*info->callbacks->undefined_symbol)
			     (info, h->root.root.string, input_bfd,
			      input_section,
			      r_vaddr - input_section->vma)))
			return false;
		      relocation = 0;
		    }
		}
	      else
		{
		  /* This is a reloc against a section.  */
		  relocation = (s->output_section->vma
				+ s->output_offset
				- s->vma);

		  /* Adjust a PC relative relocation by removing the
		     reference to the original source section.  */
		  if (howto->pc_relative)
		    relocation += input_section->vma;
		}

	      r = _bfd_final_link_relocate (howto,
					    input_bfd,
					    input_section,
					    contents,
					    r_vaddr - input_section->vma,
					    relocation,
					    addend);
	    }

	  if (r != bfd_reloc_ok)
	    {
	      switch (r)
		{
		default:
		case bfd_reloc_outofrange:
		  abort ();
		case bfd_reloc_overflow:
		  {
		    const char *name;

		    if (r_extern)
		      name = sym_hashes[r_symndx]->root.root.string;
		    else
		      name = bfd_section_name (input_bfd,
					       symndx_to_section[r_symndx]);
		    if (! ((*info->callbacks->reloc_overflow)
			   (info, name, alpha_howto_table[r_type].name,
			    (bfd_vma) 0, input_bfd, input_section,
			    r_vaddr - input_section->vma)))
		      return false;
		  }
		  break;
		}
	    }
	}

      if (info->relocateable && adjust_addrp)
	{
	  /* Change the address of the relocation.  */
	  bfd_h_put_64 (input_bfd,
			(input_section->output_section->vma
			 + input_section->output_offset
			 - input_section->vma
			 + r_vaddr),
			(bfd_byte *) ext_rel->r_vaddr);
	}

      if (gp_usedp && gp_undefined)
	{
	  if (! ((*info->callbacks->reloc_dangerous)
		 (info, "GP relative relocation when GP not defined",
		  input_bfd, input_section, r_vaddr - input_section->vma)))
	    return false;
	  /* Only give the error once per link.  */
	  ecoff_data (output_bfd)->gp = gp = 4;
	  gp_undefined = false;
	}
    }

  if (tos != 0)
    abort ();

  return true;
}

/* Do final adjustments to the filehdr and the aouthdr.  This routine
   sets the dynamic bits in the file header.  */

/*ARGSUSED*/
static boolean
alpha_adjust_headers (abfd, fhdr, ahdr)
     bfd *abfd;
     struct internal_filehdr *fhdr;
     struct internal_aouthdr *ahdr;
{
  if ((abfd->flags & (DYNAMIC | EXEC_P)) == (DYNAMIC | EXEC_P))
    fhdr->f_flags |= F_ALPHA_CALL_SHARED;
  else if ((abfd->flags & DYNAMIC) != 0)
    fhdr->f_flags |= F_ALPHA_SHARABLE;
  return true;
}

/* This is the ECOFF backend structure.  The backend field of the
   target vector points to this.  */

static const struct ecoff_backend_data alpha_ecoff_backend_data =
{
  /* COFF backend structure.  */
  {
    (void (*) PARAMS ((bfd *,PTR,int,int,int,int,PTR))) bfd_void, /* aux_in */
    (void (*) PARAMS ((bfd *,PTR,PTR))) bfd_void, /* sym_in */
    (void (*) PARAMS ((bfd *,PTR,PTR))) bfd_void, /* lineno_in */
    (unsigned (*) PARAMS ((bfd *,PTR,int,int,int,int,PTR)))bfd_void,/*aux_out*/
    (unsigned (*) PARAMS ((bfd *,PTR,PTR))) bfd_void, /* sym_out */
    (unsigned (*) PARAMS ((bfd *,PTR,PTR))) bfd_void, /* lineno_out */
    (unsigned (*) PARAMS ((bfd *,PTR,PTR))) bfd_void, /* reloc_out */
    alpha_ecoff_swap_filehdr_out, alpha_ecoff_swap_aouthdr_out,
    alpha_ecoff_swap_scnhdr_out,
    FILHSZ, AOUTSZ, SCNHSZ, 0, 0, 0, 0, true,
    alpha_ecoff_swap_filehdr_in, alpha_ecoff_swap_aouthdr_in,
    alpha_ecoff_swap_scnhdr_in, NULL,
    alpha_ecoff_bad_format_hook, _bfd_ecoff_set_arch_mach_hook,
    alpha_ecoff_mkobject_hook, _bfd_ecoff_styp_to_sec_flags,
    _bfd_ecoff_set_alignment_hook, _bfd_ecoff_slurp_symbol_table,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
  },
  /* Supported architecture.  */
  bfd_arch_alpha,
  /* Initial portion of armap string.  */
  "________64",
  /* The page boundary used to align sections in a demand-paged
     executable file.  E.g., 0x1000.  */
  0x2000,
  /* True if the .rdata section is part of the text segment, as on the
     Alpha.  False if .rdata is part of the data segment, as on the
     MIPS.  */
  true,
  /* Bitsize of constructor entries.  */
  64,
  /* Reloc to use for constructor entries.  */
  &alpha_howto_table[ALPHA_R_REFQUAD],
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
    _bfd_ecoff_slurp_symbolic_info
  },
  /* External reloc size.  */
  RELSZ,
  /* Reloc swapping functions.  */
  alpha_ecoff_swap_reloc_in,
  alpha_ecoff_swap_reloc_out,
  /* Backend reloc tweaking.  */
  alpha_adjust_reloc_in,
  alpha_adjust_reloc_out,
  /* Relocate section contents while linking.  */
  alpha_relocate_section,
  /* Do final adjustments to filehdr and aouthdr.  */
  alpha_adjust_headers
};

/* Looking up a reloc type is Alpha specific.  */
#define _bfd_ecoff_bfd_reloc_type_lookup alpha_bfd_reloc_type_lookup

/* So is getting relocated section contents.  */
#define _bfd_ecoff_bfd_get_relocated_section_contents \
  alpha_ecoff_get_relocated_section_contents

/* Handling file windows is generic.  */
#define _bfd_ecoff_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

/* Relaxing sections is generic.  */
#define _bfd_ecoff_bfd_relax_section bfd_generic_relax_section

const bfd_target ecoffalpha_little_vec =
{
  "ecoff-littlealpha",		/* name */
  bfd_target_ecoff_flavour,
  false,			/* data byte order is little */
  false,			/* header byte order is little */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | DYNAMIC | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_CODE | SEC_DATA),
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

  {_bfd_dummy_target, alpha_ecoff_object_p, /* bfd_check_format */
     _bfd_ecoff_archive_p, _bfd_dummy_target},
  {bfd_false, _bfd_ecoff_mkobject,  /* bfd_set_format */
     _bfd_generic_mkarchive, bfd_false},
  {bfd_false, _bfd_ecoff_write_object_contents, /* bfd_write_contents */
     _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (_bfd_ecoff),
     BFD_JUMP_TABLE_COPY (_bfd_ecoff),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_ecoff),
     BFD_JUMP_TABLE_SYMBOLS (_bfd_ecoff),
     BFD_JUMP_TABLE_RELOCS (_bfd_ecoff),
     BFD_JUMP_TABLE_WRITE (_bfd_ecoff),
     BFD_JUMP_TABLE_LINK (_bfd_ecoff),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  (PTR) &alpha_ecoff_backend_data
};
