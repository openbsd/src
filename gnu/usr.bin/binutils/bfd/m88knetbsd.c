/* BFD backend for MIPS BSD (a.out) binaries.
   Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.
   Written by Ralph Campbell.

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

/* NetBSD fits its header into the start of its text segment */
#define BYTES_IN_WORD	4
#define TARGET_IS_BIG_ENDIAN_P

#define	TARGET_PAGE_SIZE	8192
#define SEGMENT_SIZE	0x20000

#define DEFAULT_ARCH	bfd_arch_m88k
#define MACHTYPE_OK(mtype) \
  ((mtype) == M_88K_NETBSD || (mtype) == M_UNKNOWN || (mtype) == 151)

#define	N_HEADER_IN_TEXT(x)	1
#define TEXT_START_ADDR		4128

#define N_MACHTYPE(exec) \
	((enum machine_type)(((exec).a_info >> 16) & 0x03ff))
#define N_FLAGS(exec) \
	(((exec).a_info >> 26) & 0x3f)

#define N_SET_INFO(exec, magic, type, flags) \
	((exec).a_info = ((magic) & 0xffff) \
	 | (((int)(type) & 0x3ff) << 16) \
	 | (((flags) & 0x3f) << 24))
#define N_SET_MACHTYPE(exec, machtype) \
	((exec).a_info = \
         ((exec).a_info & 0xfb00ffff) | ((((int)(machtype))&0x3ff) << 16))
#define N_SET_FLAGS(exec, flags) \
	((exec).a_info = \
	 ((exec).a_info & 0x03ffffff) | ((flags & 0x03f) << 26))

#define N_SHARED_LIB(x) 0

#define MY(OP) CAT(m88knetbsd_,OP)

#define TARGETNAME "a.out-m88k-netbsd"

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libaout.h"

#define SET_ARCH_MACH(ABFD, EXEC) \
  MY(set_arch_mach)(ABFD, N_MACHTYPE (EXEC)); \
  MY(choose_reloc_size)(ABFD); 

#if 0
  MY(fix_howto_table);
#endif
  
void MY(set_arch_mach) PARAMS ((bfd *abfd, int machtype));
static void MY(choose_reloc_size) PARAMS ((bfd *abfd));
static void MY(fix_howto_table) PARAMS ((void));

/* On NetBSD, the magic number is always in ntohl's "network" (big-endian)
   format.  */
#define SWAP_MAGIC(ext) bfd_getb32 (ext)

#define MY_write_object_contents MY(write_object_contents)
static boolean MY(write_object_contents) PARAMS ((bfd *abfd));
#define MY_text_includes_header 1

/* We can't use MY(x) here because it leads to a recursive call to CAT
   when expanded inside JUMP_TABLE.  */
#define MY_bfd_reloc_type_lookup m88knetbsd_bfd_reloc_type_lookup
#define MY_canonicalize_reloc m88knetbsd_canonicalize_reloc

#if 1
#define MY_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define MY_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define MY_final_link_callback unused
#define MY_bfd_final_link _bfd_generic_final_link
#endif

/*#define MY_bfd_final_link m88knetbsd_bfd_final_link*/
#define howto_table_ext m88k_howto_table_ext
#define MY_BFD_TARGET

#include "aout-target.h"

void
MY(set_arch_mach) (abfd, machtype)
     bfd *abfd;
     int machtype;
{
  enum bfd_architecture arch;
  long machine;

  /* Determine the architecture and machine type of the object file. */
  switch (machtype) {

  case M_88K_NETBSD:
    arch = bfd_arch_m88k;
    machine = 88100;
    break;

  default:
    arch = bfd_arch_obscure;
    machine = 0;
    break;
  }
  bfd_set_arch_mach(abfd, arch, machine);  
}

/* Determine the size of a relocation entry, based on the architecture */
static void
MY(choose_reloc_size) (abfd)
     bfd *abfd;
{
  switch (bfd_get_arch(abfd)) {
  case bfd_arch_sparc:
  case bfd_arch_m88k:
  case bfd_arch_a29k:
  case bfd_arch_mips:
    obj_reloc_entry_size (abfd) = RELOC_EXT_SIZE;
    break;
  default:
    obj_reloc_entry_size (abfd) = RELOC_STD_SIZE;
    break;
  }
}

/* Write an object file.
   Section contents have already been written.  We write the
   file header, symbols, and relocation.  */

static boolean
MY(write_object_contents) (abfd)
     bfd *abfd;
{
  struct external_exec exec_bytes;
  struct internal_exec *execp = exec_hdr (abfd);

  /* We must make certain that the magic number has been set.  This
     will normally have been done by set_section_contents, but only if
     there actually are some section contents.  */
  if (! abfd->output_has_begun)
    {
      bfd_size_type text_size;
      file_ptr text_end;

      NAME(aout,adjust_sizes_and_vmas) (abfd, &text_size, &text_end);
    }

  MY(choose_reloc_size) (abfd);

  /* Magic number, maestro, please!  */
  switch (bfd_get_arch(abfd)) {
  case bfd_arch_m88k:
    N_SET_MACHTYPE(*execp, M_88K_NETBSD);
    break;
  default:
    N_SET_MACHTYPE(*execp, M_UNKNOWN);
    break;
  }

  /* The NetBSD magic number is always big-endian */
#ifndef TARGET_IS_BIG_ENDIAN_P
  /* XXX aren't there any macro to change byteorder of a word independent of
     the host's or target's endianesses?  */
  execp->a_info
    = (execp->a_info & 0xff) << 24 | (execp->a_info & 0xff00) << 8
      | (execp->a_info & 0xff0000) >> 8 | (execp->a_info & 0xff000000) >> 24;
#endif

  WRITE_HEADERS(abfd, execp);

  return true;
}

/*
 * m88k relocation types.
 */

enum m88k_reloc_type {
        R_88K_LO16,  /* lo16(sym) */
        R_88K_HI16,  /* hi16(sym) */
        R_88K_PC16,  /* bb0, bb1, bcnd */
        R_88K_PC26,  /* br, bsr */
        R_88K_32,    /* jump tables, etc */
        R_88K_IW16,  /* global access through linker regs 28 */
        R_88K_NONE,
	R_88K_GLOB_DAT,
	R_88K_JMP_SLOT,
	R_88K_RELATIVE,
	R_88K__max
};

static bfd_reloc_status_type m88k_special_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

static bfd_reloc_status_type 
m88k_special_reloc (abfd, reloc_entry, symbol, data,
		    input_section, output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  reloc_howto_type *howto = reloc_entry->howto;
  switch (howto->type)
    {
    case R_88K_LO16: /* lo16(sym) */
    case R_88K_HI16: /* hi16(sym) */
      if (output_bfd != (bfd *) NULL)
	{
	  /* This is a partial relocation, and we want to apply the
	     relocation to the reloc entry rather than the raw data.
	     Modify the reloc inplace to reflect what we now know.  */

	  reloc_entry->address += input_section->output_offset;
	}
      else
	{
	  bfd_vma output_base = 0;
	  bfd_vma addr = reloc_entry->address;
	  bfd_vma x = bfd_get_16 (abfd, (bfd_byte *) data + addr);
	  asection *reloc_target_output_section;
	  long relocation = 0;

	  /* Work out which section the relocation is targetted at and the
	     initial relocation command value.  */

	  /* Get symbol value.  (Common symbols are special.)  */
	  if (bfd_is_com_section (symbol->section))
	    relocation = 0;
	  else
	    relocation = symbol->value;

	  reloc_target_output_section = symbol->section->output_section;

	  /* Convert input-section-relative symbol value to absolute.  */
	  if (output_bfd)
	    output_base = 0;
	  else
	    output_base = reloc_target_output_section->vma;

	  relocation += output_base + symbol->section->output_offset;

	  /* Add in supplied addend.  */
	  relocation += ((reloc_entry->addend << howto->bitsize) + x);

	  reloc_entry->addend = 0;

	  relocation >>= (bfd_vma) howto->rightshift;

	  /* Shift everything up to where it's going to be used */

	  relocation <<= (bfd_vma) howto->bitpos;

	  if (relocation)
	      bfd_put_16 (abfd, relocation, (unsigned char *) data + addr);
	}

      return bfd_reloc_ok;
      break;

    default:
      if (output_bfd != (bfd *) NULL)
	{
	  /* This is a partial relocation, and we want to apply the
	     relocation to the reloc entry rather than the raw data.
	     Modify the reloc inplace to reflect what we now know.  */

	  reloc_entry->address += input_section->output_offset;
	  return bfd_reloc_ok;
	}
      break;
    }

  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;

  return bfd_reloc_ok;
}

static reloc_howto_type m88k_howto_table_ext[] = {
  HOWTO (R_88K_LO16,			/* type */
	 00,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "R_88K_LO16",			/* name */
	 false,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 true),				/* pcrel_offset */

  HOWTO (R_88K_HI16,			/* type */
	 16,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "R_88K_HI16",			/* name */
	 false,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 true),				/* pcrel_offset */

  HOWTO (R_88K_PC16,	  		/* type */
	 02,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 true,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_signed,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "R_88K_PC16",			/* name */
	 false,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 true),				/* pcrel_offset */

  HOWTO (R_88K_PC26,			/* type */
	 02,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 26,				/* bitsize */
	 true,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_signed,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "R_88K_PC26",			/* name */
	 false,				/* partial_inplace */
	 0x03ffffff,			/* src_mask */
	 0x03ffffff,			/* dst_mask */
	 true),				/* pcrel_offset */
	 
  HOWTO (R_88K_32,			/* type */
	 00,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "R_88K_32",			/* name */
	 false,				/* partial_inplace */
	 0xffffffff,			/* src_mask */
	 0xffffffff,			/* dst_mask */
	 true),				/* pcrel_offset */

  {-1},		/* R_88K_IW16 */
  
  HOWTO (R_88K_NONE,
         00, 
	 0, 
	 0, 
	 false,
	 0, 
	 complain_overflow_dont,
	 m88k_special_reloc, 
	 "R_88K_NONE",
	 false,
	 0, 
	 0x00000000,
	 false),
	 
  HOWTO (R_88K_GLOB_DAT,		/* type */
	 00,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "R_88K_GLOB_DAT",		/* name */
	 false,				/* partial_inplace */
	 0xffffffff,			/* src_mask */
	 0xffffffff,			/* dst_mask */
	 true),				/* pcrel_offset */
  {-1},		/* R_88K_JMP_SLOT */
  {-1},		/* R_88K_RELATIVE */
	 
};

#define CTOR_TABLE_RELOC_HOWTO(BFD) (MY(howto_table) + 4)

/* Translate internal representation of relocation info to target format.
   Total len is 12 bytes
   On m88k: first 4 bytes are normal unsigned long address,
   next three bytes are index, most sig. byte first.
   Byte 7 is broken up with bit 7 as external,
   	bits 6, 5, & 4 unused, and the lower four bits as relocation
	type.
   Next 4 bytes are long addend. 

   struct reloc_ext_external {
     bfd_byte r_address[BYTES_IN_WORD];	offset of of data to relocate 
     bfd_byte r_index[3];		symbol table index of symbol
     bfd_byte r_type[1];		relocation type
     bfd_byte r_addend[BYTES_IN_WORD];	datum addend
    };
*/

static const struct { unsigned char bfd_val, aout_val; } reloc_map[] = {
  { BFD_RELOC_NONE, R_88K_NONE },
  { BFD_RELOC_32, R_88K_32 },
  { BFD_RELOC_88K_32, R_88K_32 },
  { BFD_RELOC_88K_LO16, R_88K_LO16 },
  { BFD_RELOC_LO16, R_88K_LO16 },
  { BFD_RELOC_88K_HI16, R_88K_HI16 },
  { BFD_RELOC_HI16, R_88K_HI16 },
  { BFD_RELOC_88K_IW16, R_88K_IW16 },
  { BFD_RELOC_88K_16_PCREL, R_88K_PC16 },
  { BFD_RELOC_88K_26_PCREL, R_88K_PC26 },
  { BFD_RELOC_88K_GLOB_DAT, R_88K_GLOB_DAT },
  { BFD_RELOC_88K_JMP_SLOT, R_88K_JMP_SLOT },
  { BFD_RELOC_88K_RELATIVE, R_88K_RELATIVE },
  { BFD_RELOC_CTOR, R_88K_32 },
};

static reloc_howto_type *
MY(bfd_reloc_type_lookup)(abfd,code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  unsigned int i;
/*  printf("In MY_bfd_reloc_type_lookup\n");*/
  for (i = 0; i < sizeof (reloc_map) / sizeof (reloc_map[0]); i++)
    {
      if (reloc_map[i].bfd_val == code){
	return &m88k_howto_table_ext[(int) reloc_map[i].aout_val];
      }	
    }
  return 0;
}

#if 0
static void 
MY(fix_howto_table)(void)
{
  extern reloc_howto_type NAME(aout,ext_howto_table)[];
  memcpy(&NAME(aout,ext_howto_table), m88k_howto_table_ext, 
         sizeof(m88k_howto_table_ext));
}
static int Init = 0;
#endif
#if 0
static boolean
MY(bfd_final_link) (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  unsigned int i;
  extern reloc_howto_type NAME(aout,ext_howto_table)[];

  for (i = 0; i < sizeof (m88k_howto_table_ext) / sizeof (reloc_howto_type); i++)
  {
       memcpy(&NAME(aout,ext_howto_table)[i], &m88k_howto_table_ext[i], 
              sizeof (reloc_howto_type));
  }
/*  memcpy(&aout_32_ext_howto_table, &m88k_howto_table_ext, 
         sizeof(m88k_howto_table_ext));*/
  return NAME(aout,final_link) (abfd, info, MY_final_link_callback);
}
#endif
/*
 * This is just like the standard aoutx.h version but we need to do our
 * own mapping of external reloc type values to howto entries.
 */
long
MY(canonicalize_reloc)(abfd, section, relptr, symbols)
      bfd *abfd;
      sec_ptr section;
      arelent **relptr;
      asymbol **symbols;
{
  arelent *tblptr = section->relocation;
  unsigned int count, c;
  extern reloc_howto_type NAME(aout,ext_howto_table)[];

  /* If we have already read in the relocation table, return the values. */
  if (section->flags & SEC_CONSTRUCTOR) {
    arelent_chain *chain = section->constructor_chain;

    for (count = 0; count < section->reloc_count; count++) {
      *relptr++ = &chain->relent;
      chain = chain->next;
    }
    *relptr = 0;
    return section->reloc_count;
  }
  if (tblptr && section->reloc_count) {
    for (count = 0; count++ < section->reloc_count;) 
      *relptr++ = tblptr++;
    *relptr = 0;
    return section->reloc_count;
  }

  if (!NAME(aout,slurp_reloc_table)(abfd, section, symbols))
    return -1;
  tblptr = section->relocation;

  /* fix up howto entries */
  for (count = 0; count++ < section->reloc_count;) 
    {
      c = tblptr->howto - NAME(aout,ext_howto_table);
      tblptr->howto = &m88k_howto_table_ext[c];

      *relptr++ = tblptr++;
    }
  *relptr = 0;
  return section->reloc_count;
}

const bfd_target MY(vec) =
{
  TARGETNAME,		/* name */
  bfd_target_aout_flavour,
  BFD_ENDIAN_BIG,		/* target byte order (big) */
  BFD_ENDIAN_BIG,		/* target headers byte order (big) */
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  MY_symbol_leading_char,
  ' ',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

    {_bfd_dummy_target, MY_object_p, /* bfd_check_format */
       bfd_generic_archive_p, MY_core_file_p},
    {bfd_false, MY_mkobject,	/* bfd_set_format */
       _bfd_generic_mkarchive, bfd_false},
    {bfd_false, MY_write_object_contents, /* bfd_write_contents */
       _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (MY),
     BFD_JUMP_TABLE_COPY (MY),
     BFD_JUMP_TABLE_CORE (MY),
     BFD_JUMP_TABLE_ARCHIVE (MY),
     BFD_JUMP_TABLE_SYMBOLS (MY),
     BFD_JUMP_TABLE_RELOCS (MY),
     BFD_JUMP_TABLE_WRITE (MY),
     BFD_JUMP_TABLE_LINK (MY),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  (PTR) MY_backend_data,
};
