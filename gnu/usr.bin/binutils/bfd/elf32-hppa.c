/* BFD back-end for HP PA-RISC ELF files.
   Copyright (C) 1990, 91, 92, 93, 94, 95, 96, 97, 98, 1999
   Free Software Foundation, Inc.

   Written by

	Center for Software Science
	Department of Computer Science
	University of Utah

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
#include "elf/hppa.h"
#include "libhppa.h"
#include "elf32-hppa.h"
#define ARCH_SIZE		32
#include "elf-hppa.h"


/* We use three different hash tables to hold information for
   linking PA ELF objects.

   The first is the elf32_hppa_link_hash_table which is derived
   from the standard ELF linker hash table.  We use this as a place to
   attach other hash tables and static information.

   The second is the stub hash table which is derived from the
   base BFD hash table.  The stub hash table holds the information
   necessary to build the linker stubs during a link.  */

/* Hash table for linker stubs.  */

struct elf32_hppa_stub_hash_entry
{
  /* Base hash table entry structure, we can get the name of the stub
     (and thus know exactly what actions it performs) from the base
     hash table entry.  */
  struct bfd_hash_entry root;

  /* Offset of the beginning of this stub.  */
  bfd_vma offset;

  /* Given the symbol's value and its section we can determine its final
     value when building the stubs (so the stub knows where to jump.  */
  symvalue target_value;
  asection *target_section;
};

struct elf32_hppa_stub_hash_table
{
  /* The hash table itself.  */
  struct bfd_hash_table root;

  /* The stub BFD.  */
  bfd *stub_bfd;

  /* Where to place the next stub.  */
  bfd_byte *location;

  /* Current offset in the stub section.  */
  unsigned int offset;

};

struct elf32_hppa_link_hash_entry
{
  struct elf_link_hash_entry root;
};

struct elf32_hppa_link_hash_table
{
  /* The main hash table.  */
  struct elf_link_hash_table root;

  /* The stub hash table.  */
  struct elf32_hppa_stub_hash_table *stub_hash_table;

  /* A count of the number of output symbols.  */
  unsigned int output_symbol_count;

  /* Stuff so we can handle DP relative relocations.  */
  long global_value;
  int global_sym_defined;
};

/* ELF32/HPPA relocation support

	This file contains ELF32/HPPA relocation support as specified
	in the Stratus FTX/Golf Object File Format (SED-1762) dated
	February 1994.  */

#include "elf32-hppa.h"
#include "hppa_stubs.h"

static unsigned long hppa_elf_relocate_insn
  PARAMS ((bfd *, asection *, unsigned long, unsigned long, long,
	   long, unsigned long, unsigned long, unsigned long));

static boolean elf32_hppa_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));

static bfd_reloc_status_type elf32_hppa_bfd_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, bfd *, asection *,
	   bfd_byte *, bfd_vma, bfd_vma, bfd_vma, struct bfd_link_info *,
	   asection *, const char *, int));

static struct bfd_link_hash_table *elf32_hppa_link_hash_table_create
  PARAMS ((bfd *));

static struct bfd_hash_entry *
elf32_hppa_stub_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));

static boolean
elf32_hppa_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *,
	   bfd_byte *, Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));

static boolean
elf32_hppa_stub_hash_table_init
  PARAMS ((struct elf32_hppa_stub_hash_table *, bfd *,
	   struct bfd_hash_entry *(*) PARAMS ((struct bfd_hash_entry *,
					       struct bfd_hash_table *,
					       const char *))));

static boolean
elf32_hppa_build_one_stub PARAMS ((struct bfd_hash_entry *, PTR));

static unsigned int elf32_hppa_size_of_stub
  PARAMS ((bfd_vma, bfd_vma, const char *));

static void elf32_hppa_name_of_stub
  PARAMS ((bfd_vma, bfd_vma, char *));

/* For linker stub hash tables.  */
#define elf32_hppa_stub_hash_lookup(table, string, create, copy) \
  ((struct elf32_hppa_stub_hash_entry *) \
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

#define elf32_hppa_stub_hash_traverse(table, func, info) \
  (bfd_hash_traverse \
   (&(table)->root, \
    (boolean (*) PARAMS ((struct bfd_hash_entry *, PTR))) (func), \
    (info)))

/* For HPPA linker hash table.  */

#define elf32_hppa_link_hash_lookup(table, string, create, copy, follow)\
  ((struct elf32_hppa_link_hash_entry *)				\
   elf_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

#define elf32_hppa_link_hash_traverse(table, func, info)		\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the PA ELF linker hash table from a link_info structure.  */

#define elf32_hppa_hash_table(p) \
  ((struct elf32_hppa_link_hash_table *) ((p)->hash))


/* Assorted hash table functions.  */

/* Initialize an entry in the stub hash table.  */

static struct bfd_hash_entry *
elf32_hppa_stub_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf32_hppa_stub_hash_entry *ret;

  ret = (struct elf32_hppa_stub_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == NULL)
    ret = ((struct elf32_hppa_stub_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct elf32_hppa_stub_hash_entry)));
  if (ret == NULL)
    return NULL;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf32_hppa_stub_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));

  if (ret)
    {
      /* Initialize the local fields.  */
      ret->offset = 0;
      ret->target_value = 0;
      ret->target_section = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Initialize a stub hash table.  */

static boolean
elf32_hppa_stub_hash_table_init (table, stub_bfd, newfunc)
     struct elf32_hppa_stub_hash_table *table;
     bfd *stub_bfd;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
						struct bfd_hash_table *,
						const char *));
{
  table->offset = 0;
  table->location = 0;
  table->stub_bfd = stub_bfd;
  return (bfd_hash_table_init (&table->root, newfunc));
}

/* Create the derived linker hash table.  The PA ELF port uses the derived
   hash table to keep information specific to the PA ELF linker (without
   using static variables).  */

static struct bfd_link_hash_table *
elf32_hppa_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf32_hppa_link_hash_table *ret;

  ret = ((struct elf32_hppa_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct elf32_hppa_link_hash_table)));
  if (ret == NULL)
    return NULL;
  if (!_bfd_elf_link_hash_table_init (&ret->root, abfd,
				      _bfd_elf_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }
  ret->stub_hash_table = NULL;
  ret->output_symbol_count = 0;
  ret->global_value = 0;
  ret->global_sym_defined = 0;

  return &ret->root.root;
}

/* Relocate the given INSN given the various input parameters.

   FIXME: endianness and sizeof (long) issues abound here.  */

static unsigned long
hppa_elf_relocate_insn (abfd, input_sect, insn, address, sym_value,
			r_addend, r_format, r_field, pcrel)
     bfd *abfd;
     asection *input_sect;
     unsigned long insn;
     unsigned long address;
     long sym_value;
     long r_addend;
     unsigned long r_format;
     unsigned long r_field;
     unsigned long pcrel;
{
  unsigned char opcode = get_opcode (insn);
  long constant_value;

  switch (opcode)
    {
    case LDO:
    case LDB:
    case LDH:
    case LDW:
    case LDWM:
    case STB:
    case STH:
    case STW:
    case STWM:
    case COMICLR:
    case SUBI:
    case ADDIT:
    case ADDI:
    case LDIL:
    case ADDIL:
      constant_value = HPPA_R_CONSTANT (r_addend);

      if (pcrel)
	sym_value -= address;

      sym_value = hppa_field_adjust (sym_value, constant_value, r_field);
      return hppa_rebuild_insn (abfd, insn, sym_value, r_format);

    case BL:
    case BE:
    case BLE:
      /* XXX computing constant_value is not needed??? */
      constant_value = assemble_17 ((insn & 0x001f0000) >> 16,
				    (insn & 0x00001ffc) >> 2,
				    insn & 1);

      constant_value = (constant_value << 15) >> 15;
      if (pcrel)
	{
	  sym_value -=
	    address + input_sect->output_offset
	    + input_sect->output_section->vma;
	  sym_value = hppa_field_adjust (sym_value, -8, r_field);
	}
      else
	sym_value = hppa_field_adjust (sym_value, constant_value, r_field);

      return hppa_rebuild_insn (abfd, insn, sym_value >> 2, r_format);

    default:
      if (opcode == 0)
	{
	  constant_value = HPPA_R_CONSTANT (r_addend);

	  if (pcrel)
	    sym_value -= address;

	  return hppa_field_adjust (sym_value, constant_value, r_field);
	}
      else
	abort ();
    }
}

/* Relocate an HPPA ELF section.  */

static boolean
elf32_hppa_relocate_section (output_bfd, info, input_bfd, input_section,
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

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sym_sec;
      bfd_vma relocation;
      bfd_reloc_status_type r;
      const char *sym_name;

      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_PARISC_UNIMPLEMENTED)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      howto = elf_hppa_howto_table + r_type;

      r_symndx = ELF32_R_SYM (rel->r_info);

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
	  sym = local_syms + r_symndx;
	  sym_sec = local_sections[r_symndx];
	  relocation = ((ELF_ST_TYPE (sym->st_info) == STT_SECTION
			   ? 0 : sym->st_value)
			 + sym_sec->output_offset
			 + sym_sec->output_section->vma);
	}
      else
	{
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
	      relocation = (h->root.u.def.value
			    + sym_sec->output_offset
			    + sym_sec->output_section->vma);
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

      r = elf32_hppa_bfd_final_link_relocate (howto, input_bfd, output_bfd,
					      input_section, contents,
					      rel->r_offset, relocation,
					      rel->r_addend, info, sym_sec,
					      sym_name, h == NULL);

      if (r != bfd_reloc_ok)
	{
	  switch (r)
	    {
	    /* This can happen for DP relative relocs if $global$ is
	       undefined.  This is a panic situation so we don't try
	       to continue.  */
	    case bfd_reloc_undefined:
	    case bfd_reloc_notsupported:
	      if (!((*info->callbacks->undefined_symbol)
		    (info, "$global$", input_bfd,
		     input_section, rel->r_offset, true)))
		return false;
	      return false;
	    case bfd_reloc_dangerous:
	      {
		/* We use this return value to indicate that we performed
		   a "dangerous" relocation.  This doesn't mean we did
		   the wrong thing, it just means there may be some cleanup
		   that needs to be done here.

		   In particular we had to swap the last call insn and its
		   delay slot.  If the delay slot insn needed a relocation,
		   then we'll need to adjust the next relocation entry's
		   offset to account for the fact that the insn moved.

		   This hair wouldn't be necessary if we inserted stubs
		   between procedures and used a "bl" to get to the stub.  */
		if (rel != relend)
		  {
		    Elf_Internal_Rela *next_rel = rel + 1;

		    if (rel->r_offset + 4 == next_rel->r_offset)
		      next_rel->r_offset -= 4;
		  }
		break;
	      }
	    default:
	    case bfd_reloc_outofrange:
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

/* Actually perform a relocation as part of a final link.  This can get
   rather hairy when linker stubs are needed.  */

static bfd_reloc_status_type
elf32_hppa_bfd_final_link_relocate (howto, input_bfd, output_bfd,
				    input_section, contents, offset, value,
				    addend, info, sym_sec, sym_name, is_local)
     reloc_howto_type *howto;
     bfd *input_bfd;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd_byte *contents;
     bfd_vma offset;
     bfd_vma value;
     bfd_vma addend;
     struct bfd_link_info *info;
     asection *sym_sec;
     const char *sym_name;
     int is_local;
{
  unsigned long insn;
  unsigned long r_type = howto->type;
  unsigned long r_format = howto->bitsize;
  unsigned long r_field = e_fsel;
  bfd_byte *hit_data = contents + offset;
  boolean r_pcrel = howto->pc_relative;

  insn = bfd_get_32 (input_bfd, hit_data);

  /* Make sure we have a value for $global$.  FIXME isn't this effectively
     just like the gp pointer on MIPS?  Can we use those routines for this
     purpose?  */
  if (!elf32_hppa_hash_table (info)->global_sym_defined)
    {
      struct elf_link_hash_entry *h;
      asection *sec;

      h = elf_link_hash_lookup (elf_hash_table (info), "$global$", false,
				 false, false);

      /* If there isn't a $global$, then we're in deep trouble.  */
      if (h == NULL)
	return bfd_reloc_notsupported;

      /* If $global$ isn't a defined symbol, then we're still in deep
	 trouble.  */
      if (h->root.type != bfd_link_hash_defined)
	return bfd_reloc_undefined;

      sec = h->root.u.def.section;
      elf32_hppa_hash_table (info)->global_value = (h->root.u.def.value
						    + sec->output_section->vma
						    + sec->output_offset);
      elf32_hppa_hash_table (info)->global_sym_defined = 1;
    }

  switch (r_type)
    {
    case R_PARISC_NONE:
      break;

    case R_PARISC_DIR32:
    case R_PARISC_DIR17F:
    case R_PARISC_PCREL17C:
      r_field = e_fsel;
      goto do_basic_type_1;
    case R_PARISC_DIR21L:
    case R_PARISC_PCREL21L:
      r_field = e_lrsel;
      goto do_basic_type_1;
    case R_PARISC_DIR17R:
    case R_PARISC_PCREL17R:
    case R_PARISC_DIR14R:
    case R_PARISC_PCREL14R:
      r_field = e_rrsel;
      goto do_basic_type_1;

    /* For all the DP relative relocations, we need to examine the symbol's
       section.  If it's a code section, then "data pointer relative" makes
       no sense.  In that case we don't adjust the "value", and for 21 bit
       addil instructions, we change the source addend register from %dp to
       %r0.  */
    case R_PARISC_DPREL21L:
      r_field = e_lrsel;
      if (sym_sec->flags & SEC_CODE)
	{
	  if ((insn & 0xfc000000) >> 26 == 0xa
	       && (insn & 0x03e00000) >> 21 == 0x1b)
	    insn &= ~0x03e00000;
	}
      else
	value -= elf32_hppa_hash_table (info)->global_value;
      goto do_basic_type_1;
    case R_PARISC_DPREL14R:
      r_field = e_rrsel;
      if ((sym_sec->flags & SEC_CODE) == 0)
	value -= elf32_hppa_hash_table (info)->global_value;
      goto do_basic_type_1;
    case R_PARISC_DPREL14F:
      r_field = e_fsel;
      if ((sym_sec->flags & SEC_CODE) == 0)
	value -= elf32_hppa_hash_table (info)->global_value;
      goto do_basic_type_1;

    /* These cases are separate as they may involve a lot more work
       to deal with linker stubs.  */
    case R_PARISC_PLABEL32:
    case R_PARISC_PLABEL21L:
    case R_PARISC_PLABEL14R:
    case R_PARISC_PCREL17F:
      {
	bfd_vma location;
	unsigned int len;
	char *new_name, *stub_name;

	/* Get the field selector right.  We'll need it in a minute.  */
	if (r_type == R_PARISC_PCREL17F
	    || r_type == R_PARISC_PLABEL32)
	  r_field = e_fsel;
	else if (r_type == R_PARISC_PLABEL21L)
	  r_field = e_lrsel;
	else if (r_type == R_PARISC_PLABEL14R)
	  r_field = e_rrsel;

	/* Find out where we are and where we're going.  */
	location = (offset +
		    input_section->output_offset +
		    input_section->output_section->vma);

	len = strlen (sym_name) + 1;
	if (is_local)
	  len += 9;
	new_name = bfd_malloc (len);
	if (!new_name)
	  return bfd_reloc_notsupported;
	strcpy (new_name, sym_name);

	/* Local symbols have unique IDs.  */
	if (is_local)
	  sprintf (new_name + len - 10, "_%08x", (int)sym_sec);

	/* Any kind of linker stub needed?  */
	if (((int)(value - location) > 0x3ffff)
	    || ((int)(value - location) < (int)0xfffc0000))
	  {
	    struct elf32_hppa_stub_hash_table *stub_hash_table;
	    struct elf32_hppa_stub_hash_entry *stub_hash;
	    asection *stub_section;

	    /* Build a name for the stub.  */

	    len = strlen (new_name);
	    len += 23;
	    stub_name = bfd_malloc (len);
	    if (!stub_name)
	      return bfd_reloc_notsupported;
	    elf32_hppa_name_of_stub (location, value, stub_name);
	    strcat (stub_name, new_name);
	    free (new_name);

	    stub_hash_table = elf32_hppa_hash_table (info)->stub_hash_table;

	    stub_hash
	      = elf32_hppa_stub_hash_lookup (stub_hash_table, stub_name,
					     false, false);

	    /* We're done with that name.  */
	    free (stub_name);

	    /* The stub BFD only has one section.  */
	    stub_section = stub_hash_table->stub_bfd->sections;

	    if (stub_hash != NULL)
	      {
		if (r_type == R_PARISC_PCREL17F)
		  {
		    unsigned long delay_insn;
		    unsigned int opcode, rtn_reg, ldo_target_reg, ldo_src_reg;

		    /* We'll need to peek at the next insn.  */
		    delay_insn = bfd_get_32 (input_bfd, hit_data + 4);
		    opcode = get_opcode (delay_insn);

		    /* We also need to know the return register for this
		       call.  */
		    rtn_reg = (insn & 0x03e00000) >> 21;

		    ldo_src_reg = (delay_insn & 0x03e00000) >> 21;
		    ldo_target_reg = (delay_insn & 0x001f0000) >> 16;

		    /* Munge up the value and other parameters for
		       hppa_elf_relocate_insn.  */

		    value = (stub_hash->offset
			     + stub_section->output_offset
			     + stub_section->output_section->vma);

		    r_format = 17;
		    r_field = e_fsel;
		    r_pcrel = 0;
		    addend = 0;

		    /* We need to peek at the delay insn and determine if
		       we'll need to swap the branch and its delay insn.  */
		    if ((insn & 2)
			|| (opcode == LDO
			    && ldo_target_reg == rtn_reg)
			|| (delay_insn == 0x08000240))
		      {
			/* No need to swap the branch and its delay slot, but
			   we do need to make sure to jump past the return
			   pointer update in the stub.  */
			value += 4;

			/* If the delay insn does a return pointer adjustment,
			   then we have to make sure it stays valid.  */
			if (opcode == LDO
			    && ldo_target_reg == rtn_reg)
			  {
			    delay_insn &= 0xfc00ffff;
			    delay_insn |= ((31 << 21) | (31 << 16));
			    bfd_put_32 (input_bfd, delay_insn, hit_data + 4);
			  }
			/* Use a BLE to reach the stub.  */
			insn = BLE_SR4_R0;
		      }
		    else
		      {
			/* Wonderful, we have to swap the call insn and its
			   delay slot.  */
			bfd_put_32 (input_bfd, delay_insn, hit_data);
			/* Use a BLE,n to reach the stub.  */
			insn = (BLE_SR4_R0 | 0x2);
			bfd_put_32 (input_bfd, insn, hit_data + 4);
			insn = hppa_elf_relocate_insn (input_bfd,
						       input_section,
						       insn, offset + 4,
						       value, addend,
						       r_format, r_field,
						       r_pcrel);
			/* Update the instruction word.  */
			bfd_put_32 (input_bfd, insn, hit_data + 4);
			return bfd_reloc_dangerous;
		      }
		  }
	        else
		  return bfd_reloc_notsupported;
	      }
	  }
	goto do_basic_type_1;
      }

do_basic_type_1:
      insn = hppa_elf_relocate_insn (input_bfd, input_section, insn,
				     offset, value, addend, r_format,
				     r_field, r_pcrel);
      break;

    /* Something we don't know how to handle.  */
    default:
      return bfd_reloc_notsupported;
    }

  /* Update the instruction word.  */
  bfd_put_32 (input_bfd, insn, hit_data);
  return (bfd_reloc_ok);
}

/* Undo the generic ELF code's subtraction of section->vma from the
   value of each external symbol.  */

static boolean
elf32_hppa_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const Elf_Internal_Sym *sym ATTRIBUTE_UNUSED;
     const char **namep ATTRIBUTE_UNUSED;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp;
     bfd_vma *valp;
{
  *valp += (*secp)->vma;
  return true;
}

/* Determine the name of the stub needed to perform a call assuming the
   argument relocation bits for caller and callee are in CALLER and CALLEE
   for a call from LOCATION to DESTINATION.  Copy the name into STUB_NAME.  */

static void
elf32_hppa_name_of_stub (location, destination, stub_name)
     bfd_vma location ATTRIBUTE_UNUSED;
     bfd_vma destination ATTRIBUTE_UNUSED;
     char *stub_name;
{
  strcpy (stub_name, "_____long_branch_stub_");
}

/* Compute the size of the stub needed to call from LOCATION to DESTINATION
   (a function named SYM_NAME), with argument relocation bits CALLER and
   CALLEE.  Return zero if no stub is needed to perform such a call.  */

static unsigned int
elf32_hppa_size_of_stub (location, destination, sym_name)
     bfd_vma location, destination;
     const char *sym_name;
{
  /* Determine if a long branch stub is needed.  */
  if (!(((int)(location - destination) > 0x3ffff)
	|| ((int)(location - destination) < (int)0xfffc0000)))
    return 0;

  if (!strncmp ("$$", sym_name, 2)
      && strcmp ("$$dyncall", sym_name))
    return 12;
  else
    return 16;
}

/* Build one linker stub as defined by the stub hash table entry GEN_ENTRY.
   IN_ARGS contains the stub BFD and link info pointers.  */

static boolean
elf32_hppa_build_one_stub (gen_entry, in_args)
     struct bfd_hash_entry *gen_entry;
     PTR in_args;
{
  void **args = (void **)in_args;
  bfd *stub_bfd = (bfd *)args[0];
  struct bfd_link_info *info = (struct bfd_link_info *)args[1];
  struct elf32_hppa_stub_hash_entry *entry;
  struct elf32_hppa_stub_hash_table *stub_hash_table;
  bfd_byte *loc;
  symvalue sym_value;
  const char *sym_name;

  /* Initialize pointers to the stub hash table, the particular entry we
     are building a stub for, and where (in memory) we should place the stub
     instructions.  */
  entry = (struct elf32_hppa_stub_hash_entry *)gen_entry;
  stub_hash_table = elf32_hppa_hash_table(info)->stub_hash_table;
  loc = stub_hash_table->location;

  /* Make a note of the offset within the stubs for this entry.  */
  entry->offset = stub_hash_table->offset;

  /* The symbol's name starts at offset 22.  */
  sym_name = entry->root.string + 22;

  sym_value = (entry->target_value
	       + entry->target_section->output_offset
	       + entry->target_section->output_section->vma);

  if (1)
    {
      /* Create one of two variant long branch stubs.  One for $$dyncall and
	 normal calls, the other for calls to millicode.  */
      unsigned long insn;
      int millicode_call = 0;

      if (!strncmp ("$$", sym_name, 2) && strcmp ("$$dyncall", sym_name))
	millicode_call = 1;

      /* First the return pointer adjustment.  Depending on exact calling
	 sequence this instruction may be skipped.  */
      bfd_put_32 (stub_bfd, LDO_M4_R31_R31, loc);

      /* The next two instructions are the long branch itself.  A long branch
	 is formed with "ldil" loading the upper bits of the target address
	 into a register, then branching with "be" which adds in the lower bits.
	 Long branches to millicode nullify the delay slot of the "be".  */
      insn = hppa_rebuild_insn (stub_bfd, LDIL_R1,
				hppa_field_adjust (sym_value, 0, e_lrsel), 21);
      bfd_put_32 (stub_bfd, insn, loc + 4);
      insn = hppa_rebuild_insn (stub_bfd, BE_SR4_R1 | (millicode_call ? 2 : 0),
				hppa_field_adjust (sym_value, 0, e_rrsel) >> 2,
				17);
      bfd_put_32 (stub_bfd, insn, loc + 8);

      if (!millicode_call)
	{
	  /* The sequence to call this stub places the return pointer into %r31,
	     the final target expects the return pointer in %r2, so copy the
	      return pointer into the proper register.  */
	  bfd_put_32 (stub_bfd, COPY_R31_R2, loc + 12);

	  /* Update the location and offsets.  */
	  stub_hash_table->location += 16;
	  stub_hash_table->offset += 16;
	}
      else
	{
	  /* Update the location and offsets.  */
	  stub_hash_table->location += 12;
	  stub_hash_table->offset += 12;
	}

    }
  return true;
}

/* External entry points for sizing and building linker stubs.  */

/* Build all the stubs associated with the current output file.  The
   stubs are kept in a hash table attached to the main linker hash
   table.  This is called via hppaelf_finish in the linker.  */

boolean
elf32_hppa_build_stubs (stub_bfd, info)
     bfd *stub_bfd;
     struct bfd_link_info *info;
{
  /* The stub BFD only has one section.  */
  asection *stub_sec = stub_bfd->sections;
  struct elf32_hppa_stub_hash_table *table;
  unsigned int size;
  void *args[2];

  /* So we can pass both the BFD for the stubs and the link info
     structure to the routine which actually builds stubs.  */
  args[0] = stub_bfd;
  args[1] = info;

  /* Allocate memory to hold the linker stubs.  */
  size = bfd_section_size (stub_bfd, stub_sec);
  stub_sec->contents = (unsigned char *) bfd_zalloc (stub_bfd, size);
  if (stub_sec->contents == NULL)
    return false;
  table = elf32_hppa_hash_table(info)->stub_hash_table;
  table->location = stub_sec->contents;

  /* Build the stubs as directed by the stub hash table.  */
  elf32_hppa_stub_hash_traverse (table, elf32_hppa_build_one_stub, args);

  return true;
}

/* Determine and set the size of the stub section for a final link.

   The basic idea here is to examine all the relocations looking for
   PC-relative calls to a target that is unreachable with a "bl"
   instruction or calls where the caller and callee disagree on the
   location of their arguments or return value.  */

boolean
elf32_hppa_size_stubs (stub_bfd, output_bfd, link_info)
     bfd *stub_bfd;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info;
{
  bfd *input_bfd;
  asection *section, *stub_sec = 0;
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Sym *local_syms, **all_local_syms;
  unsigned int i, index, bfd_count = 0;
  struct elf32_hppa_stub_hash_table *stub_hash_table = 0;

  /* Create and initialize the stub hash table.  */
  stub_hash_table = ((struct elf32_hppa_stub_hash_table *)
		     bfd_malloc (sizeof (struct elf32_hppa_stub_hash_table)));
  if (!stub_hash_table)
    goto error_return;

  if (!elf32_hppa_stub_hash_table_init (stub_hash_table, stub_bfd,
					elf32_hppa_stub_hash_newfunc))
    goto error_return;

  /* Attach the hash tables to the main hash table.  */
  elf32_hppa_hash_table(link_info)->stub_hash_table = stub_hash_table;

  /* Count the number of input BFDs.  */
  for (input_bfd = link_info->input_bfds;
       input_bfd != NULL;
       input_bfd = input_bfd->link_next)
     bfd_count++;

  /* Magic as we know the stub bfd only has one section.  */
  stub_sec = stub_bfd->sections;

  /* If generating a relocateable output file, then we don't
     have to examine the relocs.  */
  if (link_info->relocateable)
    {
      for (i = 0; i < bfd_count; i++)
	if (all_local_syms[i])
	  free (all_local_syms[i]);
      free (all_local_syms);
      return true;
    }

  /* Now that we have argument location information for all the global
     functions we can start looking for stubs.  */
  for (input_bfd = link_info->input_bfds, index = 0;
       input_bfd != NULL;
       input_bfd = input_bfd->link_next, index++)
    {
      /* We'll need the symbol table in a second.  */
      symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
      if (symtab_hdr->sh_info == 0)
	continue;

      local_syms = all_local_syms[index];

      /* Walk over each section attached to the input bfd.  */
      for (section = input_bfd->sections;
	   section != NULL;
	   section = section->next)
	{
	  Elf_Internal_Shdr *input_rel_hdr;
	  Elf32_External_Rela *external_relocs, *erelaend, *erela;
	  Elf_Internal_Rela *internal_relocs, *irelaend, *irela;

	  /* If there aren't any relocs, then there's nothing to do.  */
	  if ((section->flags & SEC_RELOC) == 0
	      || section->reloc_count == 0)
	    continue;

	  /* Allocate space for the external relocations.  */
	  external_relocs
	    = ((Elf32_External_Rela *)
	       bfd_malloc (section->reloc_count
			   * sizeof (Elf32_External_Rela)));
	  if (external_relocs == NULL)
	    {
	      for (i = 0; i < bfd_count; i++)
		if (all_local_syms[i])
		  free (all_local_syms[i]);
	      free (all_local_syms);
	      goto error_return;
	    }

	  /* Likewise for the internal relocations.  */
	  internal_relocs
	    = ((Elf_Internal_Rela *)
	       bfd_malloc (section->reloc_count * sizeof (Elf_Internal_Rela)));
	  if (internal_relocs == NULL)
	    {
	      free (external_relocs);
	      for (i = 0; i < bfd_count; i++)
		if (all_local_syms[i])
		  free (all_local_syms[i]);
	      free (all_local_syms);
	      goto error_return;
	    }

	  /* Read in the external relocs.  */
	  input_rel_hdr = &elf_section_data (section)->rel_hdr;
	  if (bfd_seek (input_bfd, input_rel_hdr->sh_offset, SEEK_SET) != 0
	      || bfd_read (external_relocs, 1, input_rel_hdr->sh_size,
			   input_bfd) != input_rel_hdr->sh_size)
	    {
	      free (external_relocs);
	      free (internal_relocs);
	      for (i = 0; i < bfd_count; i++)
		if (all_local_syms[i])
		  free (all_local_syms[i]);
	      free (all_local_syms);
	      goto error_return;
	    }

	  /* Swap in the relocs.  */
	  erela = external_relocs;
	  erelaend = erela + section->reloc_count;
	  irela = internal_relocs;
	  for (; erela < erelaend; erela++, irela++)
	    bfd_elf32_swap_reloca_in (input_bfd, erela, irela);

	  /* We're done with the external relocs, free them.  */
	  free (external_relocs);

	  /* Now examine each relocation.  */
	  irela = internal_relocs;
	  irelaend = irela + section->reloc_count;
	  for (; irela < irelaend; irela++)
	    {
	      long r_type, size_of_stub;
	      unsigned long r_index;
	      struct elf_link_hash_entry *hash;
	      struct elf32_hppa_stub_hash_entry *stub_hash;
	      Elf_Internal_Sym *sym;
	      asection *sym_sec;
	      const char *sym_name;
	      symvalue sym_value;
	      bfd_vma location, destination;
	      char *new_name = NULL;

	      r_type = ELF32_R_TYPE (irela->r_info);
	      r_index = ELF32_R_SYM (irela->r_info);

	      if (r_type < 0 || r_type >= (int) R_PARISC_UNIMPLEMENTED)
		{
		  bfd_set_error (bfd_error_bad_value);
		  free (internal_relocs);
		  for (i = 0; i < bfd_count; i++)
		    if (all_local_syms[i])
		      free (all_local_syms[i]);
		  free (all_local_syms);
		  goto error_return;
		}

	      /* Only look for stubs on call instructions or plabel
		 references.  */
	      if (r_type != R_PARISC_PCREL17F
		  && r_type != R_PARISC_PLABEL32
		  && r_type != R_PARISC_PLABEL21L
		  && r_type != R_PARISC_PLABEL14R)
		continue;

	      /* Now determine the call target, its name, value, section
		 and argument relocation bits.  */
	      hash = NULL;
	      sym = NULL;
	      sym_sec = NULL;
	      if (r_index < symtab_hdr->sh_info)
		{
		  /* It's a local symbol.  */
		  Elf_Internal_Shdr *hdr;

		  sym = local_syms + r_index;
		  hdr = elf_elfsections (input_bfd)[sym->st_shndx];
		  sym_sec = hdr->bfd_section;
		  sym_name = bfd_elf_string_from_elf_section (input_bfd,
							      symtab_hdr->sh_link,
							      sym->st_name);
		  sym_value = (ELF_ST_TYPE (sym->st_info) == STT_SECTION
			       ? 0 : sym->st_value);
		  destination = (sym_value
				 + sym_sec->output_offset
				 + sym_sec->output_section->vma);

		  /* Tack on an ID so we can uniquely identify this local
		     symbol in the stub or arg info hash tables.  */
		  new_name = bfd_malloc (strlen (sym_name) + 10);
		  if (new_name == 0)
		    {
		      free (internal_relocs);
		      for (i = 0; i < bfd_count; i++)
			if (all_local_syms[i])
			  free (all_local_syms[i]);
		      free (all_local_syms);
		      goto error_return;
		    }
		  sprintf (new_name, "%s_%08x", sym_name, (int)sym_sec);
		  sym_name = new_name;
		}
	      else
		{
		  /* It's an external symbol.  */
		  long index;

		  index = r_index - symtab_hdr->sh_info;
		  hash = elf_sym_hashes (input_bfd)[index];
		  if (hash->root.type == bfd_link_hash_defined
		      || hash->root.type == bfd_link_hash_defweak)
		    {
		      sym_sec = hash->root.u.def.section;
		      sym_name = hash->root.root.string;
		      sym_value = hash->root.u.def.value;
		      destination = (sym_value
				     + sym_sec->output_offset
				     + sym_sec->output_section->vma);
		    }
		  else
		    {
		      bfd_set_error (bfd_error_bad_value);
		      free (internal_relocs);
		      for (i = 0; i < bfd_count; i++)
			if (all_local_syms[i])
			  free (all_local_syms[i]);
		      free (all_local_syms);
		      goto error_return;
		    }
		}

	      /* Now determine where the call point is.  */
	      location = (section->output_offset
			  + section->output_section->vma
			  + irela->r_offset);

	      /* We only care about the destination for PCREL function
		 calls (eg. we don't care for PLABELS).  */
	      if (r_type != R_PARISC_PCREL17F)
		location = destination;

	      /* Determine what (if any) linker stub is needed and its
		 size (in bytes).  */
	      size_of_stub = elf32_hppa_size_of_stub (location,
						      destination,
						      sym_name);
	      if (size_of_stub != 0)
		{
		  char *stub_name;
		  unsigned int len;

		  /* Get the name of this stub.  */
		  len = strlen (sym_name);
		  len += 23;

		  stub_name = bfd_malloc (len);
		  if (!stub_name)
		    {
		      /* Because sym_name was mallocd above for local
			 symbols.  */
		      if (r_index < symtab_hdr->sh_info)
			free (new_name);

		      free (internal_relocs);
		      for (i = 0; i < bfd_count; i++)
			if (all_local_syms[i])
			  free (all_local_syms[i]);
		      free (all_local_syms);
		      goto error_return;
		    }
		  elf32_hppa_name_of_stub (location, destination, stub_name);
		  strcat (stub_name + 22, sym_name);

		  /* Because sym_name was malloced above for local symbols.  */
		  if (r_index < symtab_hdr->sh_info)
		    free (new_name);

		  stub_hash
		    = elf32_hppa_stub_hash_lookup (stub_hash_table, stub_name,
						   false, false);
		  if (stub_hash != NULL)
		    {
		      /* The proper stub has already been created, nothing
			 else to do.  */
		      free (stub_name);
		    }
		  else
		    {
		      bfd_set_section_size (stub_bfd, stub_sec,
					    (bfd_section_size (stub_bfd,
							       stub_sec)
					     + size_of_stub));

		      /* Enter this entry into the linker stub hash table.  */
		      stub_hash
			= elf32_hppa_stub_hash_lookup (stub_hash_table,
						       stub_name, true, true);
		      if (stub_hash == NULL)
			{
			  free (stub_name);
			  free (internal_relocs);
			  for (i = 0; i < bfd_count; i++)
			    if (all_local_syms[i])
			      free (all_local_syms[i]);
			  free (all_local_syms);
			  goto error_return;
			}

		      /* We'll need these to determine the address that the
			 stub will branch to.  */
		      stub_hash->target_value = sym_value;
		      stub_hash->target_section = sym_sec;
		    }
		  free (stub_name);
		}
	    }
	  /* We're done with the internal relocs, free them.  */
	  free (internal_relocs);
	}
    }
  /* We're done with the local symbols, free them.  */
  for (i = 0; i < bfd_count; i++)
    if (all_local_syms[i])
      free (all_local_syms[i]);
  free (all_local_syms);
  return true;

error_return:
  /* Return gracefully, avoiding dangling references to the hash tables.  */
  if (stub_hash_table)
    {
      elf32_hppa_hash_table(link_info)->stub_hash_table = NULL;
      free (stub_hash_table);
    }
  /* Set the size of the stub section to zero since we're never going
     to create them.   Avoids losing when we try to get its contents
     too.  */
  bfd_set_section_size (stub_bfd, stub_sec, 0);
  return false;
}

/* Misc BFD support code.  */
#define bfd_elf32_bfd_reloc_type_lookup		elf_hppa_reloc_type_lookup
#define bfd_elf32_bfd_is_local_label_name	elf_hppa_is_local_label_name
#define elf_info_to_howto               	elf_hppa_info_to_howto
#define elf_info_to_howto_rel           	elf_hppa_info_to_howto_rel

/* Stuff for the BFD linker.  */
#define elf_backend_relocate_section		elf32_hppa_relocate_section
#define elf_backend_add_symbol_hook		elf32_hppa_add_symbol_hook
#define bfd_elf32_bfd_link_hash_table_create \
  elf32_hppa_link_hash_table_create
#define elf_backend_fake_sections		elf_hppa_fake_sections


#define TARGET_BIG_SYM		bfd_elf32_hppa_vec
#define TARGET_BIG_NAME		"elf32-hppa"
#define ELF_ARCH		bfd_arch_hppa
#define ELF_MACHINE_CODE	EM_PARISC
#define ELF_MAXPAGESIZE		0x1000

#include "elf32-target.h"
