/* BFD back-end for TMS320C4X coff binaries.
   Copyright 1996, 1997, 1998, 1999, 2000, 2002, 2003
   Free Software Foundation, Inc.

   Contributed by Michael Hayes (m.hayes@elec.canterbury.ac.nz)

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/tic4x.h"
#include "coff/internal.h"
#include "libcoff.h"

#undef  F_LSYMS
#define	F_LSYMS		F_LSYMS_TICOFF

static bfd_boolean ticoff0_bad_format_hook
    PARAMS ((bfd *, PTR ));
static bfd_boolean ticoff1_bad_format_hook
    PARAMS ((bfd *, PTR ));
static bfd_boolean ticoff_bfd_is_local_label_name
    PARAMS ((bfd *, const char *));
static bfd_reloc_status_type tic4x_relocation
    PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char ** ));
static reloc_howto_type *tic4x_coff_reloc_type_lookup
    PARAMS ((bfd *, bfd_reloc_code_real_type ));
static void tic4x_lookup_howto
    PARAMS ((arelent *, struct internal_reloc * ));
static reloc_howto_type *coff_tic4x_rtype_to_howto
    PARAMS ((bfd *, asection *, struct internal_reloc *, struct coff_link_hash_entry *, struct internal_syment *, bfd_vma * ));
static void tic4x_reloc_processing
    PARAMS ((arelent *, struct internal_reloc *, asymbol **, bfd *, asection * ));


static bfd_boolean
ticoff0_bad_format_hook (abfd, filehdr)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR filehdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;

  if (COFF0_BADMAG (*internal_f))
    return FALSE;

  return TRUE;
}

static bfd_boolean
ticoff1_bad_format_hook (abfd, filehdr)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR filehdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;

  if (COFF1_BADMAG (*internal_f))
    return FALSE;

  return TRUE;
}

/* Replace the stock _bfd_coff_is_local_label_name to recognize TI COFF local
   labels.  */
static bfd_boolean
ticoff_bfd_is_local_label_name (abfd, name)
  bfd *abfd ATTRIBUTE_UNUSED;
  const char *name;
{
  if (TICOFF_LOCAL_LABEL_P(name))
    return TRUE;
  return FALSE;
}

#define coff_bfd_is_local_label_name ticoff_bfd_is_local_label_name

#define RELOC_PROCESSING(RELENT,RELOC,SYMS,ABFD,SECT)\
 tic4x_reloc_processing (RELENT,RELOC,SYMS,ABFD,SECT)

/* Customize coffcode.h; the default coff_ functions are set up to use
   COFF2; coff_bad_format_hook uses BADMAG, so set that for COFF2.
   The COFF1 and COFF0 vectors use custom _bad_format_hook procs
   instead of setting BADMAG.  */
#define BADMAG(x) COFF2_BADMAG(x)
#include "coffcode.h"

static bfd_reloc_status_type
tic4x_relocation (abfd, reloc_entry, symbol, data, input_section,
		  output_bfd, error_message)
  bfd *abfd ATTRIBUTE_UNUSED;
  arelent *reloc_entry;
  asymbol *symbol ATTRIBUTE_UNUSED;
  PTR data ATTRIBUTE_UNUSED;
  asection *input_section;
  bfd *output_bfd;
  char **error_message ATTRIBUTE_UNUSED;
{
  if (output_bfd != (bfd *) NULL)
    {
      /* This is a partial relocation, and we want to apply the
 	 relocation to the reloc entry rather than the raw data.
 	 Modify the reloc inplace to reflect what we now know.  */
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }
  return bfd_reloc_continue;
}

reloc_howto_type tic4x_howto_table[] =
{
    HOWTO(R_RELWORD,	 0,  2, 16, FALSE, 0, complain_overflow_signed,   tic4x_relocation, "RELWORD",   TRUE, 0x0000ffff, 0x0000ffff, FALSE),
    HOWTO(R_REL24,	 0,  2, 24, FALSE, 0, complain_overflow_bitfield, tic4x_relocation, "REL24",     TRUE, 0x00ffffff, 0x00ffffff, FALSE),
    HOWTO(R_RELLONG,	 0,  2, 32, FALSE, 0, complain_overflow_dont,     tic4x_relocation, "RELLONG",   TRUE, 0xffffffff, 0xffffffff, FALSE),
    HOWTO(R_PCRWORD,	 0,  2, 16, TRUE,  0, complain_overflow_signed,   tic4x_relocation, "PCRWORD",   TRUE, 0x0000ffff, 0x0000ffff, FALSE),
    HOWTO(R_PCR24,	 0,  2, 24, TRUE,  0, complain_overflow_signed,   tic4x_relocation, "PCR24",     TRUE, 0x00ffffff, 0x00ffffff, FALSE),
    HOWTO(R_PARTLS16,	 0,  2, 16, FALSE, 0, complain_overflow_dont,     tic4x_relocation, "PARTLS16",  TRUE, 0x0000ffff, 0x0000ffff, FALSE),
    HOWTO(R_PARTMS8,	16,  2, 16, FALSE, 0, complain_overflow_dont,     tic4x_relocation, "PARTMS8",   TRUE, 0x0000ffff, 0x0000ffff, FALSE),
    HOWTO(R_RELWORD,	 0,  2, 16, FALSE, 0, complain_overflow_signed,   tic4x_relocation, "ARELWORD",  TRUE, 0x0000ffff, 0x0000ffff, FALSE),
    HOWTO(R_REL24,	 0,  2, 24, FALSE, 0, complain_overflow_signed,   tic4x_relocation, "AREL24",    TRUE, 0x00ffffff, 0x00ffffff, FALSE),
    HOWTO(R_RELLONG,	 0,  2, 32, FALSE, 0, complain_overflow_signed,   tic4x_relocation, "ARELLONG",  TRUE, 0xffffffff, 0xffffffff, FALSE),
    HOWTO(R_PCRWORD,	 0,  2, 16, TRUE,  0, complain_overflow_signed,   tic4x_relocation, "APCRWORD",  TRUE, 0x0000ffff, 0x0000ffff, FALSE),
    HOWTO(R_PCR24,	 0,  2, 24, TRUE,  0, complain_overflow_signed,   tic4x_relocation, "APCR24",    TRUE, 0x00ffffff, 0x00ffffff, FALSE),
    HOWTO(R_PARTLS16,	 0,  2, 16, FALSE, 0, complain_overflow_dont,     tic4x_relocation, "APARTLS16", TRUE, 0x0000ffff, 0x0000ffff, FALSE),
    HOWTO(R_PARTMS8,	16,  2, 16, FALSE, 0, complain_overflow_dont,     tic4x_relocation, "APARTMS8",  TRUE, 0x0000ffff, 0x0000ffff, FALSE),
};
#define HOWTO_SIZE (sizeof(tic4x_howto_table) / sizeof(tic4x_howto_table[0]))

#undef coff_bfd_reloc_type_lookup
#define coff_bfd_reloc_type_lookup tic4x_coff_reloc_type_lookup

/* For the case statement use the code values used tc_gen_reloc (defined in
   bfd/reloc.c) to map to the howto table entries.  */

static reloc_howto_type *
tic4x_coff_reloc_type_lookup (abfd, code)
    bfd *abfd ATTRIBUTE_UNUSED;
    bfd_reloc_code_real_type code;
{
  unsigned int type;
  unsigned int i;

  switch (code)
    {
    case BFD_RELOC_32:		type = R_RELLONG; break;
    case BFD_RELOC_24:		type = R_REL24; break;
    case BFD_RELOC_16:		type = R_RELWORD; break;
    case BFD_RELOC_24_PCREL:	type = R_PCR24; break;
    case BFD_RELOC_16_PCREL:	type = R_PCRWORD; break;
    case BFD_RELOC_HI16:	type = R_PARTMS8; break;
    case BFD_RELOC_LO16:	type = R_PARTLS16; break;
    default:
      return NULL;
    }

  for (i = 0; i < HOWTO_SIZE; i++)
    {
      if (tic4x_howto_table[i].type == type)
	return tic4x_howto_table + i;
    }
  return NULL;
}


/* Code to turn a r_type into a howto ptr, uses the above howto table.
   Called after some initial checking by the tic4x_rtype_to_howto fn
   below.  */
static void
tic4x_lookup_howto (internal, dst)
     arelent *internal;
     struct internal_reloc *dst;
{
  unsigned int i;
  int bank = (dst->r_symndx == -1) ? HOWTO_BANK : 0;

  for (i = 0; i < HOWTO_SIZE; i++)
    {
      if (tic4x_howto_table[i].type == dst->r_type)
	{
	  internal->howto = tic4x_howto_table + i + bank;
	  return;
	}
    }

  (*_bfd_error_handler) (_("Unrecognized reloc type 0x%x"),
			 (unsigned int) dst->r_type);
  abort();
}

#undef coff_rtype_to_howto
#define coff_rtype_to_howto coff_tic4x_rtype_to_howto

static reloc_howto_type *
coff_tic4x_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h ATTRIBUTE_UNUSED;
     struct internal_syment *sym ATTRIBUTE_UNUSED;
     bfd_vma *addendp;
{
  arelent genrel;

  if (rel->r_symndx == -1 && addendp != NULL)
    /* This is a TI "internal relocation", which means that the relocation
       amount is the amount by which the current section is being relocated
       in the output section.  */
    *addendp = (sec->output_section->vma + sec->output_offset) - sec->vma;

  tic4x_lookup_howto (&genrel, rel);

  return genrel.howto;
}


static void
tic4x_reloc_processing (relent, reloc, symbols, abfd, section)
     arelent *relent;
     struct internal_reloc *reloc;
     asymbol **symbols;
     bfd *abfd;
     asection *section;
{
  asymbol *ptr;

  relent->address = reloc->r_vaddr;

  if (reloc->r_symndx != -1)
    {
      if (reloc->r_symndx < 0 || reloc->r_symndx >= obj_conv_table_size (abfd))
        {
          (*_bfd_error_handler)
            (_("%s: warning: illegal symbol index %ld in relocs"),
             bfd_get_filename (abfd), reloc->r_symndx);
          relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
          ptr = NULL;
        }
      else
        {
          relent->sym_ptr_ptr = (symbols
                                 + obj_convert (abfd)[reloc->r_symndx]);
          ptr = *(relent->sym_ptr_ptr);
        }
    }
  else
    {
      relent->sym_ptr_ptr = section->symbol_ptr_ptr;
      ptr = *(relent->sym_ptr_ptr);
    }

  /* The symbols definitions that we have read in have been relocated
     as if their sections started at 0.  But the offsets refering to
     the symbols in the raw data have not been modified, so we have to
     have a negative addend to compensate.

     Note that symbols which used to be common must be left alone.  */

  /* Calculate any reloc addend by looking at the symbol.  */
  CALC_ADDEND (abfd, ptr, *reloc, relent);

  relent->address -= section->vma;
  /* !!     relent->section = (asection *) NULL;  */

  /* Fill in the relent->howto field from reloc->r_type.  */
  tic4x_lookup_howto (relent, reloc);
}


static const bfd_coff_backend_data ticoff0_swap_table =
{
  coff_SWAP_aux_in, coff_SWAP_sym_in, coff_SWAP_lineno_in,
  coff_SWAP_aux_out, coff_SWAP_sym_out,
  coff_SWAP_lineno_out, coff_SWAP_reloc_out,
  coff_SWAP_filehdr_out, coff_SWAP_aouthdr_out,
  coff_SWAP_scnhdr_out,
  FILHSZ_V0, AOUTSZ, SCNHSZ_V01, SYMESZ, AUXESZ, RELSZ_V0, LINESZ, FILNMLEN,
#ifdef COFF_LONG_FILENAMES
  TRUE,
#else
  FALSE,
#endif
#ifdef COFF_LONG_SECTION_NAMES
  TRUE,
#else
  FALSE,
#endif
  COFF_DEFAULT_SECTION_ALIGNMENT_POWER,
#ifdef COFF_FORCE_SYMBOLS_IN_STRINGS
  TRUE,
#else
  FALSE,
#endif
#ifdef COFF_DEBUG_STRING_WIDE_PREFIX
  4,
#else
  2,
#endif
  coff_SWAP_filehdr_in, coff_SWAP_aouthdr_in, coff_SWAP_scnhdr_in,
  coff_SWAP_reloc_in, ticoff0_bad_format_hook, coff_set_arch_mach_hook,
  coff_mkobject_hook, styp_to_sec_flags, coff_set_alignment_hook,
  coff_slurp_symbol_table, symname_in_debug_hook, coff_pointerize_aux_hook,
  coff_print_aux, coff_reloc16_extra_cases, coff_reloc16_estimate,
  coff_classify_symbol, coff_compute_section_file_positions,
  coff_start_final_link, coff_relocate_section, coff_rtype_to_howto,
  coff_adjust_symndx, coff_link_add_one_symbol,
  coff_link_output_has_begun, coff_final_link_postscript
};

/* COFF1 differs in section header size.  */
static const bfd_coff_backend_data ticoff1_swap_table =
{
  coff_SWAP_aux_in, coff_SWAP_sym_in, coff_SWAP_lineno_in,
  coff_SWAP_aux_out, coff_SWAP_sym_out,
  coff_SWAP_lineno_out, coff_SWAP_reloc_out,
  coff_SWAP_filehdr_out, coff_SWAP_aouthdr_out,
  coff_SWAP_scnhdr_out,
  FILHSZ, AOUTSZ, SCNHSZ_V01, SYMESZ, AUXESZ, RELSZ, LINESZ, FILNMLEN,
#ifdef COFF_LONG_FILENAMES
  TRUE,
#else
  FALSE,
#endif
#ifdef COFF_LONG_SECTION_NAMES
  TRUE,
#else
  FALSE,
#endif
  COFF_DEFAULT_SECTION_ALIGNMENT_POWER,
#ifdef COFF_FORCE_SYMBOLS_IN_STRINGS
  TRUE,
#else
  FALSE,
#endif
#ifdef COFF_DEBUG_STRING_WIDE_PREFIX
  4,
#else
  2,
#endif
  coff_SWAP_filehdr_in, coff_SWAP_aouthdr_in, coff_SWAP_scnhdr_in,
  coff_SWAP_reloc_in, ticoff1_bad_format_hook, coff_set_arch_mach_hook,
  coff_mkobject_hook, styp_to_sec_flags, coff_set_alignment_hook,
  coff_slurp_symbol_table, symname_in_debug_hook, coff_pointerize_aux_hook,
  coff_print_aux, coff_reloc16_extra_cases, coff_reloc16_estimate,
  coff_classify_symbol, coff_compute_section_file_positions,
  coff_start_final_link, coff_relocate_section, coff_rtype_to_howto,
  coff_adjust_symndx, coff_link_add_one_symbol,
  coff_link_output_has_begun, coff_final_link_postscript
};


/* TI COFF v0, DOS tools (little-endian headers).  */
const bfd_target tic4x_coff0_vec =
{
  "coff0-tic4x",		/* Name.  */
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* Data byte order is little.  */
  BFD_ENDIAN_LITTLE,		/* Header byte order is little (DOS tools).  */

  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | HAS_LOAD_PAGE ),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* Section flags.  */
  '_',				/* Leading symbol underscore.  */
  '/',				/* ar_pad_char.  */
  15,				/* ar_max_namelen.  */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* hdrs */

  {_bfd_dummy_target, coff_object_p,	/* bfd_check_format */
   bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, coff_mkobject, _bfd_generic_mkarchive,	/* bfd_set_format */
   bfd_false},
  {bfd_false, coff_write_object_contents,	/* bfd_write_contents */
   _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (coff),
  BFD_JUMP_TABLE_COPY (coff),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
  BFD_JUMP_TABLE_SYMBOLS (coff),
  BFD_JUMP_TABLE_RELOCS (coff),
  BFD_JUMP_TABLE_WRITE (coff),
  BFD_JUMP_TABLE_LINK (coff),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),
  NULL,

  (PTR)&ticoff0_swap_table
};

/* TI COFF v0, SPARC tools (big-endian headers).  */
const bfd_target tic4x_coff0_beh_vec =
{
  "coff0-beh-tic4x",		/* Name.  */
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* Data byte order is little.  */
  BFD_ENDIAN_BIG,		/* Header byte order is big.  */

  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | HAS_LOAD_PAGE ),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* Section flags.  */
  '_',				/* Leading symbol underscore.  */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */

  {_bfd_dummy_target, coff_object_p,	/* bfd_check_format */
   bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, coff_mkobject, _bfd_generic_mkarchive,	/* bfd_set_format */
   bfd_false},
  {bfd_false, coff_write_object_contents,	/* bfd_write_contents */
   _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (coff),
  BFD_JUMP_TABLE_COPY (coff),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
  BFD_JUMP_TABLE_SYMBOLS (coff),
  BFD_JUMP_TABLE_RELOCS (coff),
  BFD_JUMP_TABLE_WRITE (coff),
  BFD_JUMP_TABLE_LINK (coff),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  &tic4x_coff0_vec,

  (PTR)&ticoff0_swap_table
};

/* TI COFF v1, DOS tools (little-endian headers).  */
const bfd_target tic4x_coff1_vec =
{
  "coff1-tic4x",		/* Name.  */
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* Data byte order is little.  */
  BFD_ENDIAN_LITTLE,		/* Header byte order is little (DOS tools).  */

  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | HAS_LOAD_PAGE ),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* Section flags.  */
  '_',				/* Leading symbol underscore.  */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* hdrs */

  {_bfd_dummy_target, coff_object_p,	/* bfd_check_format */
   bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, coff_mkobject, _bfd_generic_mkarchive,	/* bfd_set_format */
   bfd_false},
  {bfd_false, coff_write_object_contents,	/* bfd_write_contents */
   _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (coff),
  BFD_JUMP_TABLE_COPY (coff),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
  BFD_JUMP_TABLE_SYMBOLS (coff),
  BFD_JUMP_TABLE_RELOCS (coff),
  BFD_JUMP_TABLE_WRITE (coff),
  BFD_JUMP_TABLE_LINK (coff),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  &tic4x_coff0_beh_vec,

  (PTR)&ticoff1_swap_table
};

/* TI COFF v1, SPARC tools (big-endian headers).  */
const bfd_target tic4x_coff1_beh_vec =
{
  "coff1-beh-tic4x",		/* Name.  */
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* Data byte order is little.  */
  BFD_ENDIAN_BIG,		/* Header byte order is big.  */

  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | HAS_LOAD_PAGE ),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* Section flags.  */
  '_',				/* Leading symbol underscore.  */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */

  {_bfd_dummy_target, coff_object_p,	/* bfd_check_format */
   bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, coff_mkobject, _bfd_generic_mkarchive,	/* bfd_set_format */
   bfd_false},
  {bfd_false, coff_write_object_contents,	/* bfd_write_contents */
   _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (coff),
  BFD_JUMP_TABLE_COPY (coff),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
  BFD_JUMP_TABLE_SYMBOLS (coff),
  BFD_JUMP_TABLE_RELOCS (coff),
  BFD_JUMP_TABLE_WRITE (coff),
  BFD_JUMP_TABLE_LINK (coff),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  &tic4x_coff1_vec,

  (PTR)&ticoff1_swap_table
};

/* TI COFF v2, TI DOS tools output (little-endian headers).  */
const bfd_target tic4x_coff2_vec =
{
  "coff2-tic4x",		/* Name.  */
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* Data byte order is little.  */
  BFD_ENDIAN_LITTLE,		/* Header byte order is little (DOS tools).  */

  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | HAS_LOAD_PAGE ),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* Section flags.  */
  '_',				/* Leading symbol underscore.  */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* hdrs */

  {_bfd_dummy_target, coff_object_p,	/* bfd_check_format */
   bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, coff_mkobject, _bfd_generic_mkarchive,	/* bfd_set_format */
   bfd_false},
  {bfd_false, coff_write_object_contents,	/* bfd_write_contents */
   _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (coff),
  BFD_JUMP_TABLE_COPY (coff),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
  BFD_JUMP_TABLE_SYMBOLS (coff),
  BFD_JUMP_TABLE_RELOCS (coff),
  BFD_JUMP_TABLE_WRITE (coff),
  BFD_JUMP_TABLE_LINK (coff),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  &tic4x_coff1_beh_vec,

  COFF_SWAP_TABLE
};

/* TI COFF v2, TI SPARC tools output (big-endian headers).  */
const bfd_target tic4x_coff2_beh_vec =
{
  "coff2-beh-tic4x",		/* Name.  */
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* Data byte order is little.  */
  BFD_ENDIAN_BIG,		/* Header byte order is big.  */

  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | HAS_LOAD_PAGE ),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* Section flags.  */
  '_',				/* Leading symbol underscore.  */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */

  {_bfd_dummy_target, coff_object_p,	/* bfd_check_format */
   bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, coff_mkobject, _bfd_generic_mkarchive,	/* bfd_set_format */
   bfd_false},
  {bfd_false, coff_write_object_contents,	/* bfd_write_contents */
   _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (coff),
  BFD_JUMP_TABLE_COPY (coff),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
  BFD_JUMP_TABLE_SYMBOLS (coff),
  BFD_JUMP_TABLE_RELOCS (coff),
  BFD_JUMP_TABLE_WRITE (coff),
  BFD_JUMP_TABLE_LINK (coff),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  &tic4x_coff2_vec,

  COFF_SWAP_TABLE
};
