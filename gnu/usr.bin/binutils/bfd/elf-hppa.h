/* Common code for PA ELF implementations.
   Copyright 1999, 2000, 2001 Free Software Foundation, Inc.

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

#define ELF_HOWTO_TABLE_SIZE       R_PARISC_UNIMPLEMENTED + 1

/* This file is included by multiple PA ELF BFD backends with different
   sizes.

   Most of the routines are written to be size independent, but sometimes
   external constraints require 32 or 64 bit specific code.  We remap
   the definitions/functions as necessary here.  */
#if ARCH_SIZE == 64
#define ELF_R_TYPE(X)   ELF64_R_TYPE(X)
#define ELF_R_SYM(X)   ELF64_R_SYM(X)
#define elf_hppa_internal_shdr Elf64_Internal_Shdr
#define _bfd_elf_hppa_gen_reloc_type _bfd_elf64_hppa_gen_reloc_type
#define elf_hppa_relocate_section elf64_hppa_relocate_section
#define bfd_elf_bfd_final_link bfd_elf64_bfd_final_link
#define elf_hppa_final_link elf64_hppa_final_link
#endif
#if ARCH_SIZE == 32
#define ELF_R_TYPE(X)   ELF32_R_TYPE(X)
#define ELF_R_SYM(X)   ELF32_R_SYM(X)
#define elf_hppa_internal_shdr Elf32_Internal_Shdr
#define _bfd_elf_hppa_gen_reloc_type _bfd_elf32_hppa_gen_reloc_type
#define elf_hppa_relocate_section elf32_hppa_relocate_section
#define bfd_elf_bfd_final_link bfd_elf32_bfd_final_link
#define elf_hppa_final_link elf32_hppa_final_link
#endif

elf_hppa_reloc_type ** _bfd_elf_hppa_gen_reloc_type
  PARAMS ((bfd *, elf_hppa_reloc_type, int, unsigned int, int, asymbol *));

static void elf_hppa_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));

static void elf_hppa_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf_Internal_Rel *));

static reloc_howto_type * elf_hppa_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));

static boolean elf_hppa_is_local_label_name
  PARAMS ((bfd *, const char *));

static boolean elf_hppa_fake_sections
  PARAMS ((bfd *abfd, elf_hppa_internal_shdr *, asection *));

static void elf_hppa_final_write_processing
  PARAMS ((bfd *, boolean));

#if ARCH_SIZE == 64
static boolean elf_hppa_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));

static boolean elf_hppa_unmark_useless_dynamic_symbols
  PARAMS ((struct elf_link_hash_entry *, PTR));

static boolean elf_hppa_remark_useless_dynamic_symbols
  PARAMS ((struct elf_link_hash_entry *, PTR));

static void elf_hppa_record_segment_addrs
  PARAMS ((bfd *, asection *, PTR));

static boolean elf_hppa_final_link
  PARAMS ((bfd *, struct bfd_link_info *));

static boolean elf_hppa_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *,
           bfd_byte *, Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));

static bfd_reloc_status_type elf_hppa_final_link_relocate
  PARAMS ((Elf_Internal_Rela *, bfd *, bfd *, asection *,
           bfd_byte *, bfd_vma, struct bfd_link_info *,
           asection *, struct elf_link_hash_entry *,
	   struct elf64_hppa_dyn_hash_entry *));

static unsigned int elf_hppa_relocate_insn
  PARAMS ((unsigned int, unsigned int, unsigned int));
#endif

/* ELF/PA relocation howto entries.  */

static reloc_howto_type elf_hppa_howto_table[ELF_HOWTO_TABLE_SIZE] =
{
  { R_PARISC_NONE, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_NONE", false, 0, 0, false },

  /* The values in DIR32 are to placate the check in
     _bfd_stab_section_find_nearest_line.  */
  { R_PARISC_DIR32, 0, 2, 32, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR32", false, 0, 0xffffffff, false },
  { R_PARISC_DIR21L, 0, 0, 21, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR21L", false, 0, 0, false },
  { R_PARISC_DIR17R, 0, 0, 17, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR17R", false, 0, 0, false },
  { R_PARISC_DIR17F, 0, 0, 17, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR17F", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_DIR14R, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR14R", false, 0, 0, false },
  { R_PARISC_DIR14F, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR14F", false, 0, 0, false },
  /* 8 */
  { R_PARISC_PCREL12F, 0, 0, 12, true, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL12F", false, 0, 0, false },
  { R_PARISC_PCREL32, 0, 0, 32, true, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL32", false, 0, 0, false },
  { R_PARISC_PCREL21L, 0, 0, 21, true, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL21L", false, 0, 0, false },
  { R_PARISC_PCREL17R, 0, 0, 17, true, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL17R", false, 0, 0, false },
  { R_PARISC_PCREL17F, 0, 0, 17, true, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL17F", false, 0, 0, false },
  { R_PARISC_PCREL17C, 0, 0, 17, true, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL17C", false, 0, 0, false },
  { R_PARISC_PCREL14R, 0, 0, 14, true, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL14R", false, 0, 0, false },
  { R_PARISC_PCREL14F, 0, 0, 14, true, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL14F", false, 0, 0, false },
  /* 16 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_DPREL21L, 0, 0, 21, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DPREL21L", false, 0, 0, false },
  { R_PARISC_DPREL14WR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DPREL14WR", false, 0, 0, false },
  { R_PARISC_DPREL14DR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DPREL14DR", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_DPREL14R, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DPREL14R", false, 0, 0, false },
  { R_PARISC_DPREL14F, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DPREL14F", false, 0, 0, false },
  /* 24 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_DLTREL21L, 0, 0, 21, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DLTREL21L", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_DLTREL14R, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DLTREL14R", false, 0, 0, false },
  { R_PARISC_DLTREL14F, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DLTREL14F", false, 0, 0, false },
  /* 32 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_DLTIND21L, 0, 0, 21, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DLTIND21L", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_DLTIND14R, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DLTIND14R", false, 0, 0, false },
  { R_PARISC_DLTIND14F, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DLTIND14F", false, 0, 0, false },
  /* 40 */
  { R_PARISC_SETBASE, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_SETBASE", false, 0, 0, false },
  { R_PARISC_SECREL32, 0, 0, 32, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_SECREL32", false, 0, 0, false },
  { R_PARISC_BASEREL21L, 0, 0, 21, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_BASEREL21L", false, 0, 0, false },
  { R_PARISC_BASEREL17R, 0, 0, 17, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_BASEREL17R", false, 0, 0, false },
  { R_PARISC_BASEREL17F, 0, 0, 17, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_BASEREL17F", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_BASEREL14R, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_BASEREL14R", false, 0, 0, false },
  { R_PARISC_BASEREL14F, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_BASEREL14F", false, 0, 0, false },
  /* 48 */
  { R_PARISC_SEGBASE, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_SEGBASE", false, 0, 0, false },
  { R_PARISC_SEGREL32, 0, 0, 32, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_SEGREL32", false, 0, 0, false },
  { R_PARISC_PLTOFF21L, 0, 0, 21, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLTOFF21L", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_PLTOFF14R, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLTOFF14R", false, 0, 0, false },
  { R_PARISC_PLTOFF14F, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLTOFF14F", false, 0, 0, false },
  /* 56 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_LTOFF_FPTR32, 0, 0, 32, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_FPTR32", false, 0, 0, false },
  { R_PARISC_LTOFF_FPTR21L, 0, 0, 21, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_FPTR21L", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_LTOFF_FPTR14R, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_FPTR14R", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 64 */
  { R_PARISC_FPTR64, 0, 0, 64, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_FPTR64", false, 0, 0, false },
  { R_PARISC_PLABEL32, 0, 0, 32, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLABEL32", false, 0, 0, false },
  { R_PARISC_PLABEL21L, 0, 0, 21, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLABEL21L", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_PLABEL14R, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLABEL14R", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 72 */
  { R_PARISC_PCREL64, 0, 0, 64, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL64", false, 0, 0, false },
  { R_PARISC_PCREL22C, 0, 0, 22, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL22C", false, 0, 0, false },
  { R_PARISC_PCREL22F, 0, 0, 22, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL22F", false, 0, 0, false },
  { R_PARISC_PCREL14WR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL14WR", false, 0, 0, false },
  { R_PARISC_PCREL14DR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL14DR", false, 0, 0, false },
  { R_PARISC_PCREL16F, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL16F", false, 0, 0, false },
  { R_PARISC_PCREL16WF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL16WF", false, 0, 0, false },
  { R_PARISC_PCREL16DF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PCREL16DF", false, 0, 0, false },
  /* 80 */
  { R_PARISC_DIR64, 0, 0, 64, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR64", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_DIR14WR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR14WR", false, 0, 0, false },
  { R_PARISC_DIR14DR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR14DR", false, 0, 0, false },
  { R_PARISC_DIR16F, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR16F", false, 0, 0, false },
  { R_PARISC_DIR16WF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR16WF", false, 0, 0, false },
  { R_PARISC_DIR16DF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DIR16DF", false, 0, 0, false },
  /* 88 */
  { R_PARISC_GPREL64, 0, 0, 64, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_GPREL64", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_DLTREL14WR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DLTREL14WR", false, 0, 0, false },
  { R_PARISC_DLTREL14DR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DLTREL14DR", false, 0, 0, false },
  { R_PARISC_GPREL16F, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_GPREL16F", false, 0, 0, false },
  { R_PARISC_GPREL16WF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_GPREL16WF", false, 0, 0, false },
  { R_PARISC_GPREL16DF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_GPREL16DF", false, 0, 0, false },
  /* 96 */
  { R_PARISC_LTOFF64, 0, 0, 64, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF64", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_DLTIND14WR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DLTIND14WR", false, 0, 0, false },
  { R_PARISC_DLTIND14DR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_DLTIND14DR", false, 0, 0, false },
  { R_PARISC_LTOFF16F, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF16F", false, 0, 0, false },
  { R_PARISC_LTOFF16WF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF16DF", false, 0, 0, false },
  { R_PARISC_LTOFF16DF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF16DF", false, 0, 0, false },
  /* 104 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_BASEREL14WR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_BASEREL14WR", false, 0, 0, false },
  { R_PARISC_BASEREL14DR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_BASEREL14DR", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 112 */
  { R_PARISC_SEGREL64, 0, 0, 64, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_SEGREL64", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_PLTOFF14WR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLTOFF14WR", false, 0, 0, false },
  { R_PARISC_PLTOFF14DR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLTOFF14DR", false, 0, 0, false },
  { R_PARISC_PLTOFF16F, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLTOFF16F", false, 0, 0, false },
  { R_PARISC_PLTOFF16WF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLTOFF16WF", false, 0, 0, false },
  { R_PARISC_PLTOFF16DF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_PLTOFF16DF", false, 0, 0, false },
  /* 120 */
  { R_PARISC_LTOFF_FPTR64, 0, 0, 64, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_LTOFF_FPTR14WR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_FPTR14WR", false, 0, 0, false },
  { R_PARISC_LTOFF_FPTR14DR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_FPTR14DR", false, 0, 0, false },
  { R_PARISC_LTOFF_FPTR16F, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_FPTR16F", false, 0, 0, false },
  { R_PARISC_LTOFF_FPTR16WF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_FPTR16WF", false, 0, 0, false },
  { R_PARISC_LTOFF_FPTR16DF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 128 */
  { R_PARISC_COPY, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_COPY", false, 0, 0, false },
  { R_PARISC_IPLT, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_IPLT", false, 0, 0, false },
  { R_PARISC_EPLT, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_EPLT", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 136 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 144 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 152 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_TPREL32, 0, 0, 32, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_TPREL32", false, 0, 0, false },
  { R_PARISC_TPREL21L, 0, 0, 21, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_TPREL21L", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_TPREL14R, 0, 0, 14, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_TPREL14R", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 160 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_LTOFF_TP21L, 0, 0, 21, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_TP21L", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_LTOFF_TP14R, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_LTOFF_TP14F, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_TP14F", false, 0, 0, false },
  /* 168 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 176 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 184 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 192 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 200 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 208 */
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  /* 216 */
  { R_PARISC_TPREL64, 0, 0, 64, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_TPREL64", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_TPREL14WR, 0, 0, 14, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_TPREL14WR", false, 0, 0, false },
  { R_PARISC_TPREL14DR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_TPREL14DR", false, 0, 0, false },
  { R_PARISC_TPREL16F, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_TPREL16F", false, 0, 0, false },
  { R_PARISC_TPREL16WF, 0, 0, 16, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_TPREL16WF", false, 0, 0, false },
  { R_PARISC_TPREL16DF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_TPREL16DF", false, 0, 0, false },
  /* 224 */
  { R_PARISC_LTOFF_TP64, 0, 0, 64, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_TP64", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_UNIMPLEMENTED", false, 0, 0, false },
  { R_PARISC_LTOFF_TP14WR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_TP14WR", false, 0, 0, false },
  { R_PARISC_LTOFF_TP14DR, 0, 0, 14, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_TP14DR", false, 0, 0, false },
  { R_PARISC_LTOFF_TP16F, 0, 0, 16, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_TP16F", false, 0, 0, false },
  { R_PARISC_LTOFF_TP16WF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_TP16WF", false, 0, 0, false },
  { R_PARISC_LTOFF_TP16DF, 0, 0, 16, false, 0, complain_overflow_bitfield,
    bfd_elf_generic_reloc, "R_PARISC_LTOFF_TP16DF", false, 0, 0, false },
  /* 232 */
  { R_PARISC_GNU_VTENTRY, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_GNU_VTENTRY", false, 0, 0, false },
  { R_PARISC_GNU_VTINHERIT, 0, 0, 0, false, 0, complain_overflow_dont,
    bfd_elf_generic_reloc, "R_PARISC_GNU_VTINHERIT", false, 0, 0, false },
};

#define OFFSET_14R_FROM_21L 4
#define OFFSET_14F_FROM_21L 5

/* Return one (or more) BFD relocations which implement the base
   relocation with modifications based on format and field.  */

elf_hppa_reloc_type **
_bfd_elf_hppa_gen_reloc_type (abfd, base_type, format, field, ignore, sym)
     bfd *abfd;
     elf_hppa_reloc_type base_type;
     int format;
     unsigned int field;
     int ignore ATTRIBUTE_UNUSED;
     asymbol *sym ATTRIBUTE_UNUSED;
{
  elf_hppa_reloc_type *finaltype;
  elf_hppa_reloc_type **final_types;

  /* Allocate slots for the BFD relocation.  */
  final_types = ((elf_hppa_reloc_type **)
		 bfd_alloc (abfd, sizeof (elf_hppa_reloc_type *) * 2));
  if (final_types == NULL)
    return NULL;

  /* Allocate space for the relocation itself.  */
  finaltype = ((elf_hppa_reloc_type *)
	       bfd_alloc (abfd, sizeof (elf_hppa_reloc_type)));
  if (finaltype == NULL)
    return NULL;

  /* Some reasonable defaults.  */
  final_types[0] = finaltype;
  final_types[1] = NULL;

#define final_type finaltype[0]

  final_type = base_type;

  /* Just a tangle of nested switch statements to deal with the braindamage
     that a different field selector means a completely different relocation
     for PA ELF.  */
  switch (base_type)
    {
    /* We have been using generic relocation types.  However, that may not
       really make sense.  Anyway, we need to support both R_PARISC_DIR64
       and R_PARISC_DIR32 here.  */
    case R_PARISC_DIR32:
    case R_PARISC_DIR64:
    case R_HPPA_ABS_CALL:
      switch (format)
	{
	case 14:
	  switch (field)
	    {
	    case e_fsel:
	      final_type = R_PARISC_DIR14F;
	      break;
	    case e_rsel:
	    case e_rrsel:
	    case e_rdsel:
	      final_type = R_PARISC_DIR14R;
	      break;
	    case e_rtsel:
	      final_type = R_PARISC_DLTIND14R;
	      break;
	    case e_rtpsel:
	      final_type = R_PARISC_LTOFF_FPTR14DR;
	      break;
	    case e_tsel:
	      final_type = R_PARISC_DLTIND14F;
	      break;
	    case e_rpsel:
	      final_type = R_PARISC_PLABEL14R;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 17:
	  switch (field)
	    {
	    case e_fsel:
	      final_type = R_PARISC_DIR17F;
	      break;
	    case e_rsel:
	    case e_rrsel:
	    case e_rdsel:
	      final_type = R_PARISC_DIR17R;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 21:
	  switch (field)
	    {
	    case e_lsel:
	    case e_lrsel:
	    case e_ldsel:
	    case e_nlsel:
	    case e_nlrsel:
	      final_type = R_PARISC_DIR21L;
	      break;
	    case e_ltsel:
	      final_type = R_PARISC_DLTIND21L;
	      break;
	    case e_ltpsel:
	      final_type = R_PARISC_LTOFF_FPTR21L;
	      break;
	    case e_lpsel:
	      final_type = R_PARISC_PLABEL21L;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 32:
	  switch (field)
	    {
	    case e_fsel:
	      final_type = R_PARISC_DIR32;
	      /* When in 64bit mode, a 32bit relocation is supposed to
		 be a section relative relocation.  Dwarf2 (for example)
		 uses 32bit section relative relocations.  */
	      if (bfd_get_arch_info (abfd)->bits_per_address != 32)
	        final_type = R_PARISC_SECREL32;
	      break;
	    case e_psel:
	      final_type = R_PARISC_PLABEL32;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 64:
	  switch (field)
	    {
	    case e_fsel:
	      final_type = R_PARISC_DIR64;
	      break;
	    case e_psel:
	      final_type = R_PARISC_FPTR64;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	default:
	  return NULL;
	}
      break;

    case R_HPPA_GOTOFF:
      switch (format)
	{
	case 14:
	  switch (field)
	    {
	    case e_rsel:
	    case e_rrsel:
	    case e_rdsel:
	      /* R_PARISC_DLTREL14R for elf64, R_PARISC_DPREL14R for elf32  */
	      final_type = base_type + OFFSET_14R_FROM_21L;
	      break;
	    case e_fsel:
	      /* R_PARISC_DLTREL14F for elf64, R_PARISC_DPREL14F for elf32  */
	      final_type = base_type + OFFSET_14F_FROM_21L;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 21:
	  switch (field)
	    {
	    case e_lsel:
	    case e_lrsel:
	    case e_ldsel:
	    case e_nlsel:
	    case e_nlrsel:
	      /* R_PARISC_DLTREL21L for elf64, R_PARISC_DPREL21L for elf32  */
	      final_type = base_type;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	default:
	  return NULL;
	}
      break;

    case R_HPPA_PCREL_CALL:
      switch (format)
	{
	case 12:
	  switch (field)
	    {
	    case e_fsel:
	      final_type = R_PARISC_PCREL12F;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 14:
	  /* Contrary to appearances, these are not calls of any sort.
	     Rather, they are loads/stores with a pcrel reloc.  */
	  switch (field)
	    {
	    case e_rsel:
	    case e_rrsel:
	    case e_rdsel:
	      final_type = R_PARISC_PCREL14R;
	      break;
	    case e_fsel:
	      final_type = R_PARISC_PCREL14F;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 17:
	  switch (field)
	    {
	    case e_rsel:
	    case e_rrsel:
	    case e_rdsel:
	      final_type = R_PARISC_PCREL17R;
	      break;
	    case e_fsel:
	      final_type = R_PARISC_PCREL17F;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 21:
	  switch (field)
	    {
	    case e_lsel:
	    case e_lrsel:
	    case e_ldsel:
	    case e_nlsel:
	    case e_nlrsel:
	      final_type = R_PARISC_PCREL21L;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 22:
	  switch (field)
	    {
	    case e_fsel:
	      final_type = R_PARISC_PCREL22F;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	default:
	  return NULL;
	}
      break;

    case R_PARISC_GNU_VTENTRY:
    case R_PARISC_GNU_VTINHERIT:
    case R_PARISC_SEGREL32:
    case R_PARISC_SEGBASE:
      /* The defaults are fine for these cases.  */
      break;

    default:
      return NULL;
    }

  return final_types;
}

/* Translate from an elf into field into a howto relocation pointer.  */

static void
elf_hppa_info_to_howto (abfd, bfd_reloc, elf_reloc)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *bfd_reloc;
     Elf_Internal_Rela *elf_reloc;
{
  BFD_ASSERT (ELF_R_TYPE(elf_reloc->r_info)
	      < (unsigned int) R_PARISC_UNIMPLEMENTED);
  bfd_reloc->howto = &elf_hppa_howto_table[ELF_R_TYPE (elf_reloc->r_info)];
}

/* Translate from an elf into field into a howto relocation pointer.  */

static void
elf_hppa_info_to_howto_rel (abfd, bfd_reloc, elf_reloc)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *bfd_reloc;
     Elf_Internal_Rel *elf_reloc;
{
  BFD_ASSERT (ELF_R_TYPE(elf_reloc->r_info)
	      < (unsigned int) R_PARISC_UNIMPLEMENTED);
  bfd_reloc->howto = &elf_hppa_howto_table[ELF_R_TYPE (elf_reloc->r_info)];
}

/* Return the address of the howto table entry to perform the CODE
   relocation for an ARCH machine.  */

static reloc_howto_type *
elf_hppa_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  if ((int) code < (int) R_PARISC_UNIMPLEMENTED)
    {
      BFD_ASSERT ((int) elf_hppa_howto_table[(int) code].type == (int) code);
      return &elf_hppa_howto_table[(int) code];
    }
  return NULL;
}

/* Return true if SYM represents a local label symbol.  */

static boolean
elf_hppa_is_local_label_name (abfd, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name;
{
  if (name[0] == 'L' && name[1] == '$')
    return 1;
  return _bfd_elf_is_local_label_name (abfd, name);
}

/* Set the correct type for an ELF section.  We do this by the
   section name, which is a hack, but ought to work.  */

static boolean
elf_hppa_fake_sections (abfd, hdr, sec)
     bfd *abfd;
     elf_hppa_internal_shdr *hdr;
     asection *sec;
{
  register const char *name;

  name = bfd_get_section_name (abfd, sec);

  if (strcmp (name, ".PARISC.unwind") == 0)
    {
      int indx;
      asection *asec;
#if ARCH_SIZE == 64
      hdr->sh_type = SHT_LOPROC + 1;
#else
      hdr->sh_type = 1;
#endif
      /* ?!? How are unwinds supposed to work for symbols in arbitrary
	 sections?  Or what if we have multiple .text sections in a single
	 .o file?  HP really messed up on this one.

	 Ugh.  We can not use elf_section_data (sec)->this_idx at this
	 point because it is not initialized yet.

	 So we (gasp) recompute it here.  Hopefully nobody ever changes the
	 way sections are numbered in elf.c!  */
      for (asec = abfd->sections, indx = 1; asec; asec = asec->next, indx++)
	{
	  if (asec->name && strcmp (asec->name, ".text") == 0)
	    {
	      hdr->sh_info = indx;
	      break;
	    }
	}

      /* I have no idea if this is really necessary or what it means.  */
      hdr->sh_entsize = 4;
    }
  return true;
}

static void
elf_hppa_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker ATTRIBUTE_UNUSED;
{
  int mach = bfd_get_mach (abfd);

  elf_elfheader (abfd)->e_flags &= ~(EF_PARISC_ARCH | EF_PARISC_TRAPNIL
				     | EF_PARISC_EXT | EF_PARISC_LSB
				     | EF_PARISC_WIDE | EF_PARISC_NO_KABP
				     | EF_PARISC_LAZYSWAP);

  if (mach == 10)
    elf_elfheader (abfd)->e_flags |= EFA_PARISC_1_0;
  else if (mach == 11)
    elf_elfheader (abfd)->e_flags |= EFA_PARISC_1_1;
  else if (mach == 20)
    elf_elfheader (abfd)->e_flags |= EFA_PARISC_2_0;
  else if (mach == 25)
    elf_elfheader (abfd)->e_flags |= (EF_PARISC_WIDE
				      | EFA_PARISC_2_0
				      /* The GNU tools have trapped without
					 option since 1993, so need to take
					 a step backwards with the ELF
					 based toolchains.  */
				      | EF_PARISC_TRAPNIL);
}

#if ARCH_SIZE == 64
/* Hook called by the linker routine which adds symbols from an object
   file.  HP's libraries define symbols with HP specific section
   indices, which we have to handle.  */

static boolean
elf_hppa_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const Elf_Internal_Sym *sym;
     const char **namep ATTRIBUTE_UNUSED;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp;
     bfd_vma *valp;
{
  int index = sym->st_shndx;

  switch (index)
    {
    case SHN_PARISC_ANSI_COMMON:
      *secp = bfd_make_section_old_way (abfd, ".PARISC.ansi.common");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;

    case SHN_PARISC_HUGE_COMMON:
      *secp = bfd_make_section_old_way (abfd, ".PARISC.huge.common");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;
    }

  return true;
}

static boolean
elf_hppa_unmark_useless_dynamic_symbols (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct bfd_link_info *info = (struct bfd_link_info *)data;

  /* If we are not creating a shared library, and this symbol is
     referenced by a shared library but is not defined anywhere, then
     the generic code will warn that it is undefined.

     This behavior is undesirable on HPs since the standard shared
     libraries contain references to undefined symbols.

     So we twiddle the flags associated with such symbols so that they
     will not trigger the warning.  ?!? FIXME.  This is horribly fragile.

     Ultimately we should have better controls over the generic ELF BFD
     linker code.  */
  if (! info->relocateable
      && ! (info->shared
	    && !info->no_undefined)
      && h->root.type == bfd_link_hash_undefined
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    {
      h->elf_link_hash_flags &= ~ELF_LINK_HASH_REF_DYNAMIC;
      h->elf_link_hash_flags |= 0x8000;
    }

  return true;
}

static boolean
elf_hppa_remark_useless_dynamic_symbols (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct bfd_link_info *info = (struct bfd_link_info *)data;

  /* If we are not creating a shared library, and this symbol is
     referenced by a shared library but is not defined anywhere, then
     the generic code will warn that it is undefined.

     This behavior is undesirable on HPs since the standard shared
     libraries contain reerences to undefined symbols.

     So we twiddle the flags associated with such symbols so that they
     will not trigger the warning.  ?!? FIXME.  This is horribly fragile.

     Ultimately we should have better controls over the generic ELF BFD
     linker code.  */
  if (! info->relocateable
      && ! (info->shared
	    && !info->no_undefined)
      && h->root.type == bfd_link_hash_undefined
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) == 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0
      && (h->elf_link_hash_flags & 0x8000) != 0)
    {
      h->elf_link_hash_flags |= ELF_LINK_HASH_REF_DYNAMIC;
      h->elf_link_hash_flags &= ~0x8000;
    }

  return true;
}

/* Record the lowest address for the data and text segments.  */
static void
elf_hppa_record_segment_addrs (abfd, section, data)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section;
     PTR data;
{
  struct elf64_hppa_link_hash_table *hppa_info;
  bfd_vma value;

  hppa_info = (struct elf64_hppa_link_hash_table *)data;

  value = section->vma - section->filepos;

  if (((section->flags & (SEC_ALLOC | SEC_LOAD | SEC_READONLY))
       == (SEC_ALLOC | SEC_LOAD | SEC_READONLY))
      && value < hppa_info->text_segment_base)
    hppa_info->text_segment_base = value;
  else if (((section->flags & (SEC_ALLOC | SEC_LOAD | SEC_READONLY))
	    == (SEC_ALLOC | SEC_LOAD))
	   && value < hppa_info->data_segment_base)
    hppa_info->data_segment_base = value;
}

/* Called after we have seen all the input files/sections, but before
   final symbol resolution and section placement has been determined.

   We use this hook to (possibly) provide a value for __gp, then we
   fall back to the generic ELF final link routine.  */

static boolean
elf_hppa_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  boolean retval;
  struct elf64_hppa_link_hash_table *hppa_info = elf64_hppa_hash_table (info);

  if (! info->relocateable)
    {
      struct elf_link_hash_entry *gp;
      bfd_vma gp_val;

      /* The linker script defines a value for __gp iff it was referenced
	 by one of the objects being linked.  First try to find the symbol
	 in the hash table.  If that fails, just compute the value __gp
	 should have had.  */
      gp = elf_link_hash_lookup (elf_hash_table (info), "__gp", false,
				 false, false);

      if (gp)
	{

	  /* Adjust the value of __gp as we may want to slide it into the
	     .plt section so that the stubs can access PLT entries without
	     using an addil sequence.  */
	  gp->root.u.def.value += hppa_info->gp_offset;

	  gp_val = (gp->root.u.def.section->output_section->vma
		    + gp->root.u.def.section->output_offset
		    + gp->root.u.def.value);
	}
      else
	{
	  asection *sec;

	  /* First look for a .plt section.  If found, then __gp is the
	     address of the .plt + gp_offset.

	     If no .plt is found, then look for .dlt, .opd and .data (in
	     that order) and set __gp to the base address of whichever section
	    is found first.  */

	  sec = hppa_info->plt_sec;
	  if (sec)
	    gp_val = (sec->output_offset
		      + sec->output_section->vma
		      + hppa_info->gp_offset);
	  else
	    {
	      sec = hppa_info->dlt_sec;
	      if (!sec)
		sec = hppa_info->opd_sec;
	      if (!sec)
		sec = bfd_get_section_by_name (abfd, ".data");
	      if (!sec)
		return false;

	      gp_val = sec->output_offset + sec->output_section->vma;
	    }
	}

      /* Install whatever value we found/computed for __gp.  */
      _bfd_set_gp_value (abfd, gp_val);
    }

  /* We need to know the base of the text and data segments so that we
     can perform SEGREL relocations.  We will record the base addresses
     when we encounter the first SEGREL relocation.  */
  hppa_info->text_segment_base = (bfd_vma)-1;
  hppa_info->data_segment_base = (bfd_vma)-1;

  /* HP's shared libraries have references to symbols that are not
     defined anywhere.  The generic ELF BFD linker code will complaim
     about such symbols.

     So we detect the losing case and arrange for the flags on the symbol
     to indicate that it was never referenced.  This keeps the generic
     ELF BFD link code happy and appears to not create any secondary
     problems.  Ultimately we need a way to control the behavior of the
     generic ELF BFD link code better.  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_hppa_unmark_useless_dynamic_symbols,
			  info);

  /* Invoke the regular ELF backend linker to do all the work.  */
  retval = bfd_elf_bfd_final_link (abfd, info);

  elf_link_hash_traverse (elf_hash_table (info),
			  elf_hppa_remark_useless_dynamic_symbols,
			  info);

  return retval;
}

/* Relocate an HPPA ELF section.  */

static boolean
elf_hppa_relocate_section (output_bfd, info, input_bfd, input_section,
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
  struct elf64_hppa_link_hash_table *hppa_info = elf64_hppa_hash_table (info);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto = elf_hppa_howto_table + ELF_R_TYPE (rel->r_info);
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sym_sec;
      bfd_vma relocation;
      bfd_reloc_status_type r;
      const char *sym_name;
      const char *dyn_name;
      char *dynh_buf = NULL;
      size_t dynh_buflen = 0;
      struct elf64_hppa_dyn_hash_entry *dyn_h = NULL;

      r_type = ELF_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_PARISC_UNIMPLEMENTED)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      r_symndx = ELF_R_SYM (rel->r_info);

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sym_sec = local_sections[r_symndx];
		  rel->r_addend += sym_sec->output_offset;
		}
	    }

	  continue;
	}

      /* This is a final link.  */
      h = NULL;
      sym = NULL;
      sym_sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  /* This is a local symbol.  */
	  sym = local_syms + r_symndx;
	  sym_sec = local_sections[r_symndx];
	  relocation = ((ELF_ST_TYPE (sym->st_info) == STT_SECTION
			   ? 0 : sym->st_value)
			 + sym_sec->output_offset
			 + sym_sec->output_section->vma);

	  /* If this symbol has an entry in the PA64 dynamic hash
	     table, then get it.  */
	  dyn_name = get_dyn_name (input_section, h, rel,
				   &dynh_buf, &dynh_buflen);
	  dyn_h = elf64_hppa_dyn_hash_lookup (&hppa_info->dyn_hash_table,
					      dyn_name, false, false);

	}
      else
	{
	  /* This is not a local symbol.  */
	  long indx;

	  indx = r_symndx - symtab_hdr->sh_info;
	  h = elf_sym_hashes (input_bfd)[indx];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sym_sec = h->root.u.def.section;

	      /* If this symbol has an entry in the PA64 dynamic hash
		 table, then get it.  */
	      dyn_name = get_dyn_name (input_section, h, rel,
				       &dynh_buf, &dynh_buflen);
	      dyn_h = elf64_hppa_dyn_hash_lookup (&hppa_info->dyn_hash_table,
						  dyn_name, false, false);

	      /* If we have a relocation against a symbol defined in a
		 shared library and we have not created an entry in the
		 PA64 dynamic symbol hash table for it, then we lose.  */
	      if (sym_sec->output_section == NULL && dyn_h == NULL)
		{
		  (*_bfd_error_handler)
		    (_("%s: warning: unresolvable relocation against symbol `%s' from %s section"),
		     bfd_get_filename (input_bfd), h->root.root.string,
		     bfd_get_section_name (input_bfd, input_section));
		  relocation = 0;
		}
	      else if (sym_sec->output_section)
		relocation = (h->root.u.def.value
			      + sym_sec->output_offset
			      + sym_sec->output_section->vma);
	      /* Value will be provided via one of the offsets in the
		 dyn_h hash table entry.  */
	      else
		relocation = 0;
	    }
	  /* Allow undefined symbols in shared libraries.  */
          else if (info->shared && !info->no_undefined
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    {
	      if (info->symbolic)
		(*info->callbacks->undefined_symbol)
		  (info, h->root.root.string, input_bfd,
		   input_section, rel->r_offset, false);

	      /* If this symbol has an entry in the PA64 dynamic hash
		 table, then get it.  */
	      dyn_name = get_dyn_name (input_section, h, rel,
				       &dynh_buf, &dynh_buflen);
	      dyn_h = elf64_hppa_dyn_hash_lookup (&hppa_info->dyn_hash_table,
						  dyn_name, false, false);

	      if (dyn_h == NULL)
		{
		  (*_bfd_error_handler)
		    (_("%s: warning: unresolvable relocation against symbol `%s' from %s section"),
		     bfd_get_filename (input_bfd), h->root.root.string,
		     bfd_get_section_name (input_bfd, input_section));
		  relocation = 0;
		}
	      relocation = 0;
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else
	    {
	      if (!((*info->callbacks->undefined_symbol)
		    (info, h->root.root.string, input_bfd,
		     input_section, rel->r_offset, true)))
		return false;
	      break;
	    }
	}

      if (h != NULL)
	sym_name = h->root.root.string;
      else
	{
	  sym_name = bfd_elf_string_from_elf_section (input_bfd,
						      symtab_hdr->sh_link,
						      sym->st_name);
	  if (sym_name == NULL)
	    return false;
	  if (*sym_name == '\0')
	    sym_name = bfd_section_name (input_bfd, sym_sec);
	}

      r = elf_hppa_final_link_relocate (rel, input_bfd, output_bfd,
					input_section, contents,
					relocation, info, sym_sec,
					h, dyn_h);

      if (r != bfd_reloc_ok)
	{
	  switch (r)
	    {
	    default:
	      abort ();
	    case bfd_reloc_overflow:
	      {
		if (!((*info->callbacks->reloc_overflow)
		      (info, sym_name, howto->name, (bfd_vma) 0,
			input_bfd, input_section, rel->r_offset)))
		  return false;
	      }
	      break;
	    }
	}
    }
  return true;
}

/* Compute the value for a relocation (REL) during a final link stage,
   then insert the value into the proper location in CONTENTS.

   VALUE is a tentative value for the relocation and may be overridden
   and modified here based on the specific relocation to be performed.

   For example we do conversions for PC-relative branches in this routine
   or redirection of calls to external routines to stubs.

   The work of actually applying the relocation is left to a helper
   routine in an attempt to reduce the complexity and size of this
   function.  */

static bfd_reloc_status_type
elf_hppa_final_link_relocate (rel, input_bfd, output_bfd,
			      input_section, contents, value,
			      info, sym_sec, h, dyn_h)
     Elf_Internal_Rela *rel;
     bfd *input_bfd;
     bfd *output_bfd;
     asection *input_section;
     bfd_byte *contents;
     bfd_vma value;
     struct bfd_link_info *info;
     asection *sym_sec;
     struct elf_link_hash_entry *h ATTRIBUTE_UNUSED;
     struct elf64_hppa_dyn_hash_entry *dyn_h;
{
  unsigned int insn;
  bfd_vma offset = rel->r_offset;
  bfd_vma addend = rel->r_addend;
  reloc_howto_type *howto = elf_hppa_howto_table + ELF_R_TYPE (rel->r_info);
  unsigned int r_type = howto->type;
  bfd_byte *hit_data = contents + offset;
  struct elf64_hppa_link_hash_table *hppa_info = elf64_hppa_hash_table (info);

  insn = bfd_get_32 (input_bfd, hit_data);

  switch (r_type)
    {
    case R_PARISC_NONE:
      break;

    /* Basic function call support.  I'm not entirely sure if PCREL14F is
       actually needed or even handled correctly.

       Note for a call to a function defined in another dynamic library
       we want to redirect the call to a stub.  */

    /* Random PC relative relocs.  */
    case R_PARISC_PCREL21L:
    case R_PARISC_PCREL14R:
    case R_PARISC_PCREL14F:
    case R_PARISC_PCREL14WR:
    case R_PARISC_PCREL14DR:
    case R_PARISC_PCREL16F:
    case R_PARISC_PCREL16WF:
    case R_PARISC_PCREL16DF:
      {
	/* If this is a call to a function defined in another dynamic
	   library, then redirect the call to the local stub for this
	   function.  */
	if (sym_sec == NULL || sym_sec->output_section == NULL)
	  value = (dyn_h->stub_offset + hppa_info->stub_sec->output_offset
		   + hppa_info->stub_sec->output_section->vma);

	/* Turn VALUE into a proper PC relative address.  */
	value -= (offset + input_section->output_offset
		  + input_section->output_section->vma);

	/* Adjust for any field selectors.  */
	if (r_type == R_PARISC_PCREL21L)
	  value = hppa_field_adjust (value, -8 + addend, e_lsel);
	else if (r_type == R_PARISC_PCREL14F
		 || r_type == R_PARISC_PCREL16F
		 || r_type == R_PARISC_PCREL16WF
		 || r_type == R_PARISC_PCREL16DF)
	  value = hppa_field_adjust (value, -8 + addend, e_fsel);
	else
	  value = hppa_field_adjust (value, -8 + addend, e_rsel);

	/* Apply the relocation to the given instruction.  */
	insn = elf_hppa_relocate_insn (insn, value, r_type);
	break;
      }

    case R_PARISC_PCREL12F:
    case R_PARISC_PCREL22F:
    case R_PARISC_PCREL17F:
    case R_PARISC_PCREL22C:
    case R_PARISC_PCREL17C:
    case R_PARISC_PCREL17R:
      {
	/* If this is a call to a function defined in another dynamic
	   library, then redirect the call to the local stub for this
	   function.  */
	if (sym_sec == NULL || sym_sec->output_section == NULL)
	  value = (dyn_h->stub_offset + hppa_info->stub_sec->output_offset
		   + hppa_info->stub_sec->output_section->vma);

	/* Turn VALUE into a proper PC relative address.  */
	value -= (offset + input_section->output_offset
		  + input_section->output_section->vma);

	/* Adjust for any field selectors.  */
	if (r_type == R_PARISC_PCREL17R)
	  value = hppa_field_adjust (value, -8 + addend, e_rsel);
	else
	  value = hppa_field_adjust (value, -8 + addend, e_fsel);

	/* All branches are implicitly shifted by 2 places.  */
	value >>= 2;

	/* Apply the relocation to the given instruction.  */
	insn = elf_hppa_relocate_insn (insn, value, r_type);
	break;
      }

    /* Indirect references to data through the DLT.  */
    case R_PARISC_DLTIND14R:
    case R_PARISC_DLTIND14F:
    case R_PARISC_DLTIND14DR:
    case R_PARISC_DLTIND14WR:
    case R_PARISC_DLTIND21L:
    case R_PARISC_LTOFF_FPTR14R:
    case R_PARISC_LTOFF_FPTR14DR:
    case R_PARISC_LTOFF_FPTR14WR:
    case R_PARISC_LTOFF_FPTR21L:
    case R_PARISC_LTOFF_FPTR16F:
    case R_PARISC_LTOFF_FPTR16WF:
    case R_PARISC_LTOFF_FPTR16DF:
    case R_PARISC_LTOFF_TP21L:
    case R_PARISC_LTOFF_TP14R:
    case R_PARISC_LTOFF_TP14F:
    case R_PARISC_LTOFF_TP14WR:
    case R_PARISC_LTOFF_TP14DR:
    case R_PARISC_LTOFF_TP16F:
    case R_PARISC_LTOFF_TP16WF:
    case R_PARISC_LTOFF_TP16DF:
    case R_PARISC_LTOFF16F:
    case R_PARISC_LTOFF16WF:
    case R_PARISC_LTOFF16DF:
      {
	/* If this relocation was against a local symbol, then we still
	   have not set up the DLT entry (it's not convenient to do so
	   in the "finalize_dlt" routine because it is difficult to get
	   to the local symbol's value).

	   So, if this is a local symbol (h == NULL), then we need to
	   fill in its DLT entry.

	   Similarly we may still need to set up an entry in .opd for
	   a local function which had its address taken.  */
	if (dyn_h->h == NULL)
	  {
	    bfd_put_64 (hppa_info->dlt_sec->owner,
			value,
			hppa_info->dlt_sec->contents + dyn_h->dlt_offset);

	    /* Now handle .opd creation if needed.  */
	    if (r_type == R_PARISC_LTOFF_FPTR14R
		|| r_type == R_PARISC_LTOFF_FPTR14DR
		|| r_type == R_PARISC_LTOFF_FPTR14WR
		|| r_type == R_PARISC_LTOFF_FPTR21L
		|| r_type == R_PARISC_LTOFF_FPTR16F
		|| r_type == R_PARISC_LTOFF_FPTR16WF
		|| r_type == R_PARISC_LTOFF_FPTR16DF)
	      {
		/* The first two words of an .opd entry are zero.  */
		memset (hppa_info->opd_sec->contents + dyn_h->opd_offset,
			0, 16);

		/* The next word is the address of the function.  */
		bfd_put_64 (hppa_info->opd_sec->owner, value,
			    (hppa_info->opd_sec->contents
			     + dyn_h->opd_offset + 16));

		/* The last word is our local __gp value.  */
		value = _bfd_get_gp_value
			  (hppa_info->opd_sec->output_section->owner);
		bfd_put_64 (hppa_info->opd_sec->owner, value,
			    (hppa_info->opd_sec->contents
			     + dyn_h->opd_offset + 24));
	      }
	  }

	/* We want the value of the DLT offset for this symbol, not
	   the symbol's actual address.  Note that __gp may not point
	   to the start of the DLT, so we have to compute the absolute
	   address, then subtract out the value of __gp.  */
	value = (dyn_h->dlt_offset
		 + hppa_info->dlt_sec->output_offset
		 + hppa_info->dlt_sec->output_section->vma);
	value -= _bfd_get_gp_value (output_bfd);

	/* All DLTIND relocations are basically the same at this point,
	   except that we need different field selectors for the 21bit
	   version vs the 14bit versions.  */
	if (r_type == R_PARISC_DLTIND21L
	    || r_type == R_PARISC_LTOFF_FPTR21L
	    || r_type == R_PARISC_LTOFF_TP21L)
	  value = hppa_field_adjust (value, addend, e_lrsel);
	else if (r_type == R_PARISC_DLTIND14F
		 || r_type == R_PARISC_LTOFF_FPTR16F
		 || r_type == R_PARISC_LTOFF_FPTR16WF
		 || r_type == R_PARISC_LTOFF_FPTR16DF
		 || r_type == R_PARISC_LTOFF16F
		 || r_type == R_PARISC_LTOFF16DF
		 || r_type == R_PARISC_LTOFF16WF
		 || r_type == R_PARISC_LTOFF_TP16F
		 || r_type == R_PARISC_LTOFF_TP16WF
		 || r_type == R_PARISC_LTOFF_TP16DF)
	  value = hppa_field_adjust (value, addend, e_fsel);
	else
	  value = hppa_field_adjust (value, addend, e_rrsel);

	insn = elf_hppa_relocate_insn (insn, value, r_type);
	break;
      }

    case R_PARISC_DLTREL14R:
    case R_PARISC_DLTREL14F:
    case R_PARISC_DLTREL14DR:
    case R_PARISC_DLTREL14WR:
    case R_PARISC_DLTREL21L:
    case R_PARISC_DPREL21L:
    case R_PARISC_DPREL14WR:
    case R_PARISC_DPREL14DR:
    case R_PARISC_DPREL14R:
    case R_PARISC_DPREL14F:
    case R_PARISC_GPREL16F:
    case R_PARISC_GPREL16WF:
    case R_PARISC_GPREL16DF:
      {
	/* Subtract out the global pointer value to make value a DLT
	   relative address.  */
	value -= _bfd_get_gp_value (output_bfd);

	/* All DLTREL relocations are basically the same at this point,
	   except that we need different field selectors for the 21bit
	   version vs the 14bit versions.  */
	if (r_type == R_PARISC_DLTREL21L
	    || r_type == R_PARISC_DPREL21L)
	  value = hppa_field_adjust (value, addend, e_lrsel);
	else if (r_type == R_PARISC_DLTREL14F
		 || r_type == R_PARISC_DPREL14F
		 || r_type == R_PARISC_GPREL16F
		 || r_type == R_PARISC_GPREL16WF
		 || r_type == R_PARISC_GPREL16DF)
	  value = hppa_field_adjust (value, addend, e_fsel);
	else
	  value = hppa_field_adjust (value, addend, e_rrsel);

	insn = elf_hppa_relocate_insn (insn, value, r_type);
	break;
      }

    case R_PARISC_DIR21L:
    case R_PARISC_DIR17R:
    case R_PARISC_DIR17F:
    case R_PARISC_DIR14R:
    case R_PARISC_DIR14F:
    case R_PARISC_DIR14WR:
    case R_PARISC_DIR14DR:
    case R_PARISC_DIR16F:
    case R_PARISC_DIR16WF:
    case R_PARISC_DIR16DF:
      {
	/* All DIR relocations are basically the same at this point,
	   except that branch offsets need to be divided by four, and
	   we need different field selectors.  Note that we don't
	   redirect absolute calls to local stubs.  */

	if (r_type == R_PARISC_DIR21L)
	  value = hppa_field_adjust (value, addend, e_lrsel);
	else if (r_type == R_PARISC_DIR17F
		 || r_type == R_PARISC_DIR16F
		 || r_type == R_PARISC_DIR16WF
		 || r_type == R_PARISC_DIR16DF
		 || r_type == R_PARISC_DIR14F)
	  value = hppa_field_adjust (value, addend, e_fsel);
	else
	  value = hppa_field_adjust (value, addend, e_rrsel);

	if (r_type == R_PARISC_DIR17R || r_type == R_PARISC_DIR17F)
	  {
	    /* All branches are implicitly shifted by 2 places.  */
	    value >>= 2;
	  }

	insn = elf_hppa_relocate_insn (insn, value, r_type);
	break;
      }

    case R_PARISC_PLTOFF21L:
    case R_PARISC_PLTOFF14R:
    case R_PARISC_PLTOFF14F:
    case R_PARISC_PLTOFF14WR:
    case R_PARISC_PLTOFF14DR:
    case R_PARISC_PLTOFF16F:
    case R_PARISC_PLTOFF16WF:
    case R_PARISC_PLTOFF16DF:
      {
	/* We want the value of the PLT offset for this symbol, not
	   the symbol's actual address.  Note that __gp may not point
	   to the start of the DLT, so we have to compute the absolute
	   address, then subtract out the value of __gp.  */
	value = (dyn_h->plt_offset
		 + hppa_info->plt_sec->output_offset
		 + hppa_info->plt_sec->output_section->vma);
	value -= _bfd_get_gp_value (output_bfd);

	/* All PLTOFF relocations are basically the same at this point,
	   except that we need different field selectors for the 21bit
	   version vs the 14bit versions.  */
	if (r_type == R_PARISC_PLTOFF21L)
	  value = hppa_field_adjust (value, addend, e_lrsel);
	else if (r_type == R_PARISC_PLTOFF14F
		 || r_type == R_PARISC_PLTOFF16F
		 || r_type == R_PARISC_PLTOFF16WF
		 || r_type == R_PARISC_PLTOFF16DF)
	  value = hppa_field_adjust (value, addend, e_fsel);
	else
	  value = hppa_field_adjust (value, addend, e_rrsel);

	insn = elf_hppa_relocate_insn (insn, value, r_type);
	break;
      }

    case R_PARISC_LTOFF_FPTR32:
      {
	/* We may still need to create the FPTR itself if it was for
	   a local symbol.  */
	if (dyn_h->h == NULL)
	  {
	    /* The first two words of an .opd entry are zero.  */
	    memset (hppa_info->opd_sec->contents + dyn_h->opd_offset, 0, 16);

	    /* The next word is the address of the function.  */
	    bfd_put_64 (hppa_info->opd_sec->owner, value,
			(hppa_info->opd_sec->contents
			 + dyn_h->opd_offset + 16));

	    /* The last word is our local __gp value.  */
	    value = _bfd_get_gp_value
		      (hppa_info->opd_sec->output_section->owner);
	    bfd_put_64 (hppa_info->opd_sec->owner, value,
			hppa_info->opd_sec->contents + dyn_h->opd_offset + 24);
	  }

	/* We want the value of the DLT offset for this symbol, not
	   the symbol's actual address.  Note that __gp may not point
	   to the start of the DLT, so we have to compute the absolute
	   address, then subtract out the value of __gp.  */
	value = (dyn_h->dlt_offset
		 + hppa_info->dlt_sec->output_offset
		 + hppa_info->dlt_sec->output_section->vma);
	value -= _bfd_get_gp_value (output_bfd);
	bfd_put_32 (input_bfd, value, hit_data);
	return bfd_reloc_ok;
      }

    case R_PARISC_LTOFF_FPTR64:
    case R_PARISC_LTOFF_TP64:
      {
	/* We may still need to create the FPTR itself if it was for
	   a local symbol.  */
	if (dyn_h->h == NULL && r_type == R_PARISC_LTOFF_FPTR64)
	  {
	    /* The first two words of an .opd entry are zero.  */
	    memset (hppa_info->opd_sec->contents + dyn_h->opd_offset, 0, 16);

	    /* The next word is the address of the function.  */
	    bfd_put_64 (hppa_info->opd_sec->owner, value,
			(hppa_info->opd_sec->contents
			 + dyn_h->opd_offset + 16));

	    /* The last word is our local __gp value.  */
	    value = _bfd_get_gp_value
		      (hppa_info->opd_sec->output_section->owner);
	    bfd_put_64 (hppa_info->opd_sec->owner, value,
			hppa_info->opd_sec->contents + dyn_h->opd_offset + 24);
	  }

	/* We want the value of the DLT offset for this symbol, not
	   the symbol's actual address.  Note that __gp may not point
	   to the start of the DLT, so we have to compute the absolute
	   address, then subtract out the value of __gp.  */
	value = (dyn_h->dlt_offset
		 + hppa_info->dlt_sec->output_offset
		 + hppa_info->dlt_sec->output_section->vma);
	value -= _bfd_get_gp_value (output_bfd);
	bfd_put_64 (input_bfd, value, hit_data);
	return bfd_reloc_ok;
      }

    case R_PARISC_DIR32:
      bfd_put_32 (input_bfd, value + addend, hit_data);
      return bfd_reloc_ok;

    case R_PARISC_DIR64:
      bfd_put_64 (input_bfd, value + addend, hit_data);
      return bfd_reloc_ok;

    case R_PARISC_GPREL64:
      /* Subtract out the global pointer value to make value a DLT
	 relative address.  */
      value -= _bfd_get_gp_value (output_bfd);

      bfd_put_64 (input_bfd, value + addend, hit_data);
      return bfd_reloc_ok;

    case R_PARISC_LTOFF64:
	/* We want the value of the DLT offset for this symbol, not
	   the symbol's actual address.  Note that __gp may not point
	   to the start of the DLT, so we have to compute the absolute
	   address, then subtract out the value of __gp.  */
      value = (dyn_h->dlt_offset
	       + hppa_info->dlt_sec->output_offset
	       + hppa_info->dlt_sec->output_section->vma);
      value -= _bfd_get_gp_value (output_bfd);

      bfd_put_64 (input_bfd, value + addend, hit_data);
      return bfd_reloc_ok;

    case R_PARISC_PCREL32:
      {
	/* If this is a call to a function defined in another dynamic
	   library, then redirect the call to the local stub for this
	   function.  */
	if (sym_sec == NULL || sym_sec->output_section == NULL)
	  value = (dyn_h->stub_offset + hppa_info->stub_sec->output_offset
		   + hppa_info->stub_sec->output_section->vma);

	/* Turn VALUE into a proper PC relative address.  */
	value -= (offset + input_section->output_offset
		  + input_section->output_section->vma);

	value += addend;
	value -= 8;
	bfd_put_32 (input_bfd, value, hit_data);
	return bfd_reloc_ok;
      }

    case R_PARISC_PCREL64:
      {
	/* If this is a call to a function defined in another dynamic
	   library, then redirect the call to the local stub for this
	   function.  */
	if (sym_sec == NULL || sym_sec->output_section == NULL)
	  value = (dyn_h->stub_offset + hppa_info->stub_sec->output_offset
		   + hppa_info->stub_sec->output_section->vma);

	/* Turn VALUE into a proper PC relative address.  */
	value -= (offset + input_section->output_offset
		  + input_section->output_section->vma);

	value += addend;
	value -= 8;
	bfd_put_64 (input_bfd, value, hit_data);
	return bfd_reloc_ok;
      }

    case R_PARISC_FPTR64:
      {
	/* We may still need to create the FPTR itself if it was for
	   a local symbol.  */
	if (dyn_h->h == NULL)
	  {
	    /* The first two words of an .opd entry are zero.  */
	    memset (hppa_info->opd_sec->contents + dyn_h->opd_offset, 0, 16);

	    /* The next word is the address of the function.  */
	    bfd_put_64 (hppa_info->opd_sec->owner, value,
			(hppa_info->opd_sec->contents
			 + dyn_h->opd_offset + 16));

	    /* The last word is our local __gp value.  */
	    value = _bfd_get_gp_value
		      (hppa_info->opd_sec->output_section->owner);
	    bfd_put_64 (hppa_info->opd_sec->owner, value,
			hppa_info->opd_sec->contents + dyn_h->opd_offset + 24);
	  }

	/* We want the value of the OPD offset for this symbol, not
           the symbol's actual address.  */
	value = (dyn_h->opd_offset
		 + hppa_info->opd_sec->output_offset
		 + hppa_info->opd_sec->output_section->vma);

	bfd_put_64 (input_bfd, value + addend, hit_data);
	return bfd_reloc_ok;
      }

    case R_PARISC_SECREL32:
      bfd_put_32 (input_bfd,
		  value + addend - sym_sec->output_section->vma,
		  hit_data);
      return bfd_reloc_ok;

    case R_PARISC_SEGREL32:
    case R_PARISC_SEGREL64:
      {
	/* If this is the first SEGREL relocation, then initialize
	   the segment base values.  */
	if (hppa_info->text_segment_base == (bfd_vma) -1)
	  bfd_map_over_sections (output_bfd, elf_hppa_record_segment_addrs,
				 hppa_info);

	/* VALUE holds the absolute address.  We want to include the
	   addend, then turn it into a segment relative address.

	   The segment is derived from SYM_SEC.  We assume that there are
	   only two segments of note in the resulting executable/shlib.
	   A readonly segment (.text) and a readwrite segment (.data).  */
	value += addend;

	if (sym_sec->flags & SEC_CODE)
	  value -= hppa_info->text_segment_base;
	else
	  value -= hppa_info->data_segment_base;

	if (r_type == R_PARISC_SEGREL32)
	  bfd_put_32 (input_bfd, value, hit_data);
	else
	  bfd_put_64 (input_bfd, value, hit_data);
        return bfd_reloc_ok;
      }

    /* Something we don't know how to handle.  */
    default:
      return bfd_reloc_notsupported;
    }

  /* Update the instruction word.  */
  bfd_put_32 (input_bfd, insn, hit_data);
  return bfd_reloc_ok;
}

/* Relocate the given INSN.  VALUE should be the actual value we want
   to insert into the instruction, ie by this point we should not be
   concerned with computing an offset relative to the DLT, PC, etc.
   Instead this routine is meant to handle the bit manipulations needed
   to insert the relocation into the given instruction.  */

static unsigned int
elf_hppa_relocate_insn (insn, sym_value, r_type)
     unsigned int insn;
     unsigned int sym_value;
     unsigned int r_type;
{
  switch (r_type)
    {
    /* This is any 22 bit branch.  In PA2.0 syntax it corresponds to
       the "B" instruction.  */
    case R_PARISC_PCREL22F:
    case R_PARISC_PCREL22C:
      return (insn & ~ 0x3ff1ffd) | re_assemble_22 (sym_value);

      /* This is any 12 bit branch.  */
    case R_PARISC_PCREL12F:
      return (insn & ~ 0x1ffd) | re_assemble_12 (sym_value);

    /* This is any 17 bit branch.  In PA2.0 syntax it also corresponds
       to the "B" instruction as well as BE.  */
    case R_PARISC_PCREL17F:
    case R_PARISC_DIR17F:
    case R_PARISC_DIR17R:
    case R_PARISC_PCREL17C:
    case R_PARISC_PCREL17R:
      return (insn & ~ 0x1f1ffd) | re_assemble_17 (sym_value);

    /* ADDIL or LDIL instructions.  */
    case R_PARISC_DLTREL21L:
    case R_PARISC_DLTIND21L:
    case R_PARISC_LTOFF_FPTR21L:
    case R_PARISC_PCREL21L:
    case R_PARISC_LTOFF_TP21L:
    case R_PARISC_DPREL21L:
    case R_PARISC_PLTOFF21L:
    case R_PARISC_DIR21L:
      return (insn & ~ 0x1fffff) | re_assemble_21 (sym_value);

    /* LDO and integer loads/stores with 14 bit displacements.  */
    case R_PARISC_DLTREL14R:
    case R_PARISC_DLTREL14F:
    case R_PARISC_DLTIND14R:
    case R_PARISC_DLTIND14F:
    case R_PARISC_LTOFF_FPTR14R:
    case R_PARISC_LTOFF_FPTR16F:
    case R_PARISC_PCREL14R:
    case R_PARISC_PCREL14F:
    case R_PARISC_PCREL16F:
    case R_PARISC_LTOFF_TP14R:
    case R_PARISC_LTOFF_TP14F:
    case R_PARISC_LTOFF_TP16F:
    case R_PARISC_DPREL14R:
    case R_PARISC_DPREL14F:
    case R_PARISC_GPREL16F:
    case R_PARISC_PLTOFF14R:
    case R_PARISC_PLTOFF14F:
    case R_PARISC_PLTOFF16F:
    case R_PARISC_DIR14R:
    case R_PARISC_DIR14F:
    case R_PARISC_DIR16F:
    case R_PARISC_LTOFF16F:
      return (insn & ~ 0x3fff) | low_sign_unext (sym_value, 14);

    /* Doubleword loads and stores with a 14 bit displacement.  */
    case R_PARISC_DLTREL14DR:
    case R_PARISC_DLTIND14DR:
    case R_PARISC_LTOFF_FPTR14DR:
    case R_PARISC_LTOFF_FPTR16DF:
    case R_PARISC_PCREL14DR:
    case R_PARISC_PCREL16DF:
    case R_PARISC_LTOFF_TP14DR:
    case R_PARISC_LTOFF_TP16DF:
    case R_PARISC_DPREL14DR:
    case R_PARISC_GPREL16DF:
    case R_PARISC_PLTOFF14DR:
    case R_PARISC_PLTOFF16DF:
    case R_PARISC_DIR14DR:
    case R_PARISC_DIR16DF:
    case R_PARISC_LTOFF16DF:
      return (insn & ~ 0x3ff1) | (((sym_value & 0x2000) >> 13)
				  | ((sym_value & 0x1ff8) << 1));

    /* Floating point single word load/store instructions.  */
    case R_PARISC_DLTREL14WR:
    case R_PARISC_DLTIND14WR:
    case R_PARISC_LTOFF_FPTR14WR:
    case R_PARISC_LTOFF_FPTR16WF:
    case R_PARISC_PCREL14WR:
    case R_PARISC_PCREL16WF:
    case R_PARISC_LTOFF_TP14WR:
    case R_PARISC_LTOFF_TP16WF:
    case R_PARISC_DPREL14WR:
    case R_PARISC_GPREL16WF:
    case R_PARISC_PLTOFF14WR:
    case R_PARISC_PLTOFF16WF:
    case R_PARISC_DIR16WF:
    case R_PARISC_DIR14WR:
    case R_PARISC_LTOFF16WF:
      return (insn & ~ 0x3ff9) | (((sym_value & 0x2000) >> 13)
				  | ((sym_value & 0x1ffc) << 1));

    default:
      return insn;
    }
}
#endif
