/* BFD back-end for Hitachi H8/300 COFF binaries.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 98, 1999
   Free Software Foundation, Inc.
   Written by Steve Chamberlain, <sac@cygnus.com>.

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
#include "bfdlink.h"
#include "genlink.h"
#include "coff/h8300.h"
#include "coff/internal.h"
#include "libcoff.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (1)

/* We derive a hash table from the basic BFD hash table to
   hold entries in the function vector.  Aside from the 
   info stored by the basic hash table, we need the offset
   of a particular entry within the hash table as well as
   the offset where we'll add the next entry.  */

struct funcvec_hash_entry
{
  /* The basic hash table entry.  */
  struct bfd_hash_entry root;

  /* The offset within the vectors section where
     this entry lives.  */
  bfd_vma offset;
};

struct funcvec_hash_table
{
  /* The basic hash table.  */
  struct bfd_hash_table root;

  bfd *abfd;

  /* Offset at which we'll add the next entry.  */
  unsigned int offset;
};

static struct bfd_hash_entry *
funcvec_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));

static boolean
funcvec_hash_table_init
  PARAMS ((struct funcvec_hash_table *, bfd *,
           struct bfd_hash_entry *(*) PARAMS ((struct bfd_hash_entry *,
                                               struct bfd_hash_table *,
                                               const char *))));

/* To lookup a value in the function vector hash table.  */
#define funcvec_hash_lookup(table, string, create, copy) \
  ((struct funcvec_hash_entry *) \
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

/* The derived h8300 COFF linker table.  Note it's derived from
   the generic linker hash table, not the COFF backend linker hash
   table!  We use this to attach additional data structures we
   need while linking on the h8300.  */
struct h8300_coff_link_hash_table
{
  /* The main hash table.  */
  struct generic_link_hash_table root;

  /* Section for the vectors table.  This gets attached to a
     random input bfd, we keep it here for easy access.  */
  asection *vectors_sec;

  /* Hash table of the functions we need to enter into the function
     vector.  */
  struct funcvec_hash_table *funcvec_hash_table;
};

static struct bfd_link_hash_table *h8300_coff_link_hash_table_create
  PARAMS ((bfd *));

/* Get the H8/300 COFF linker hash table from a link_info structure.  */

#define h8300_coff_hash_table(p) \
  ((struct h8300_coff_link_hash_table *) ((coff_hash_table (p))))

/* Initialize fields within a funcvec hash table entry.  Called whenever
   a new entry is added to the funcvec hash table.  */

static struct bfd_hash_entry *
funcvec_hash_newfunc (entry, gen_table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *gen_table;
     const char *string;
{
  struct funcvec_hash_entry *ret;
  struct funcvec_hash_table *table;

  ret = (struct funcvec_hash_entry *) entry;
  table = (struct funcvec_hash_table *) gen_table;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == NULL)
    ret = ((struct funcvec_hash_entry *)
           bfd_hash_allocate (gen_table,
                              sizeof (struct funcvec_hash_entry)));
  if (ret == NULL)
    return NULL;

  /* Call the allocation method of the superclass.  */
  ret = ((struct funcvec_hash_entry *)
         bfd_hash_newfunc ((struct bfd_hash_entry *) ret, gen_table, string));

  if (ret == NULL)
    return NULL;

  /* Note where this entry will reside in the function vector table.  */
  ret->offset = table->offset;

  /* Bump the offset at which we store entries in the function
     vector.  We'd like to bump up the size of the vectors section,
     but it's not easily available here.  */
  if (bfd_get_mach (table->abfd) == bfd_mach_h8300)
    table->offset += 2;
  else if (bfd_get_mach (table->abfd) == bfd_mach_h8300h
	   || bfd_get_mach (table->abfd) == bfd_mach_h8300s)
    table->offset += 4;
  else
    return NULL;

  /* Everything went OK.  */
  return (struct bfd_hash_entry *) ret;
}

/* Initialize the function vector hash table.  */

static boolean
funcvec_hash_table_init (table, abfd, newfunc)
     struct funcvec_hash_table *table;
     bfd *abfd;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
                                                struct bfd_hash_table *,
                                                const char *));
{
  /* Initialize our local fields, then call the generic initialization
     routine.  */
  table->offset = 0;
  table->abfd = abfd;
  return (bfd_hash_table_init (&table->root, newfunc));
}

/* Create the derived linker hash table.  We use a derived hash table
   basically to hold "static" information during an h8/300 coff link
   without using static variables.  */

static struct bfd_link_hash_table *
h8300_coff_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct h8300_coff_link_hash_table *ret;
  ret = ((struct h8300_coff_link_hash_table *)
         bfd_alloc (abfd, sizeof (struct h8300_coff_link_hash_table)));
  if (ret == NULL)
    return NULL;
  if (!_bfd_link_hash_table_init (&ret->root.root, abfd, _bfd_generic_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  /* Initialize our data.  */
  ret->vectors_sec = NULL;
  ret->funcvec_hash_table = NULL;

  /* OK.  Everything's intialized, return the base pointer.  */
  return &ret->root.root;
}

/* special handling for H8/300 relocs.
   We only come here for pcrel stuff and return normally if not an -r link.
   When doing -r, we can't do any arithmetic for the pcrel stuff, because
   the code in reloc.c assumes that we can manipulate the targets of
   the pcrel branches.  This isn't so, since the H8/300 can do relaxing, 
   which means that the gap after the instruction may not be enough to
   contain the offset required for the branch, so we have to use the only
   the addend until the final link */

static bfd_reloc_status_type
special (abfd, reloc_entry, symbol, data, input_section, output_bfd,
		 error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry ATTRIBUTE_UNUSED;
     asymbol *symbol ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;

  return bfd_reloc_ok;
}

static reloc_howto_type howto_table[] =
{
  HOWTO (R_RELBYTE, 0, 0, 8, false, 0, complain_overflow_bitfield, special, "8", false, 0x000000ff, 0x000000ff, false),
  HOWTO (R_RELWORD, 0, 1, 16, false, 0, complain_overflow_bitfield, special, "16", false, 0x0000ffff, 0x0000ffff, false),
  HOWTO (R_RELLONG, 0, 2, 32, false, 0, complain_overflow_bitfield, special, "32", false, 0xffffffff, 0xffffffff, false),
  HOWTO (R_PCRBYTE, 0, 0, 8, true, 0, complain_overflow_signed, special, "DISP8", false, 0x000000ff, 0x000000ff, true),
  HOWTO (R_PCRWORD, 0, 1, 16, true, 0, complain_overflow_signed, special, "DISP16", false, 0x0000ffff, 0x0000ffff, true),
  HOWTO (R_PCRLONG, 0, 2, 32, true, 0, complain_overflow_signed, special, "DISP32", false, 0xffffffff, 0xffffffff, true),
  HOWTO (R_MOV16B1, 0, 1, 16, false, 0, complain_overflow_bitfield, special, "relaxable mov.b:16", false, 0x0000ffff, 0x0000ffff, false),
  HOWTO (R_MOV16B2, 0, 1, 8, false, 0, complain_overflow_bitfield, special, "relaxed mov.b:16", false, 0x000000ff, 0x000000ff, false),
  HOWTO (R_JMP1, 0, 1, 16, false, 0, complain_overflow_bitfield, special, "16/pcrel", false, 0x0000ffff, 0x0000ffff, false),
  HOWTO (R_JMP2, 0, 0, 8, false, 0, complain_overflow_bitfield, special, "pcrecl/16", false, 0x000000ff, 0x000000ff, false),
  HOWTO (R_JMPL1, 0, 2, 32, false, 0, complain_overflow_bitfield, special, "24/pcrell", false, 0x00ffffff, 0x00ffffff, false),
  HOWTO (R_JMPL2, 0, 0, 8, false, 0, complain_overflow_bitfield, special, "pc8/24", false, 0x000000ff, 0x000000ff, false),
  HOWTO (R_MOV24B1, 0, 1, 32, false, 0, complain_overflow_bitfield, special, "relaxable mov.b:24", false, 0xffffffff, 0xffffffff, false),
  HOWTO (R_MOV24B2, 0, 1, 8, false, 0, complain_overflow_bitfield, special, "relaxed mov.b:24", false, 0x0000ffff, 0x0000ffff, false),

  /* An indirect reference to a function.  This causes the function's address
     to be added to the function vector in lo-mem and puts the address of
     the function vector's entry in the jsr instruction.  */
  HOWTO (R_MEM_INDIRECT, 0, 0, 8, false, 0, complain_overflow_bitfield, special, "8/indirect", false, 0x000000ff, 0x000000ff, false),

  /* Internal reloc for relaxing.  This is created when a 16bit pc-relative
     branch is turned into an 8bit pc-relative branch.  */
  HOWTO (R_PCRWORD_B, 0, 0, 8, true, 0, complain_overflow_bitfield, special, "relaxed bCC:16", false, 0x000000ff, 0x000000ff, false),

  HOWTO (R_MOVL1, 0, 2, 32, false, 0, complain_overflow_bitfield,special, "32/24 relaxable move", false, 0xffffffff, 0xffffffff, false),

  HOWTO (R_MOVL2, 0, 1, 16, false, 0, complain_overflow_bitfield, special, "32/24 relaxed move", false, 0x0000ffff, 0x0000ffff, false),

  HOWTO (R_BCC_INV, 0, 0, 8, true, 0, complain_overflow_signed, special, "DISP8 inverted", false, 0x000000ff, 0x000000ff, true),

  HOWTO (R_JMP_DEL, 0, 0, 8, true, 0, complain_overflow_signed, special, "Deleted jump", false, 0x000000ff, 0x000000ff, true),
};


/* Turn a howto into a reloc number */

#define SELECT_RELOC(x,howto) \
  { x.r_type = select_reloc(howto); }

#define BADMAG(x) (H8300BADMAG(x) && H8300HBADMAG(x) && H8300SBADMAG(x))
#define H8300 1			/* Customize coffcode.h */
#define __A_MAGIC_SET__



/* Code to swap in the reloc */
#define SWAP_IN_RELOC_OFFSET   bfd_h_get_32
#define SWAP_OUT_RELOC_OFFSET bfd_h_put_32
#define SWAP_OUT_RELOC_EXTRA(abfd, src, dst) \
  dst->r_stuff[0] = 'S'; \
  dst->r_stuff[1] = 'C';


static int
select_reloc (howto)
     reloc_howto_type *howto;
{
  return howto->type;
}

/* Code to turn a r_type into a howto ptr, uses the above howto table
   */

static void
rtype2howto (internal, dst)
     arelent *internal;
     struct internal_reloc *dst;
{
  switch (dst->r_type)
    {
    case R_RELBYTE:
      internal->howto = howto_table + 0;
      break;
    case R_RELWORD:
      internal->howto = howto_table + 1;
      break;
    case R_RELLONG:
      internal->howto = howto_table + 2;
      break;
    case R_PCRBYTE:
      internal->howto = howto_table + 3;
      break;
    case R_PCRWORD:
      internal->howto = howto_table + 4;
      break;
    case R_PCRLONG:
      internal->howto = howto_table + 5;
      break;
    case R_MOV16B1:
      internal->howto = howto_table + 6;
      break;
    case R_MOV16B2:
      internal->howto = howto_table + 7;
      break;
    case R_JMP1:
      internal->howto = howto_table + 8;
      break;
    case R_JMP2:
      internal->howto = howto_table + 9;
      break;
    case R_JMPL1:
      internal->howto = howto_table + 10;
      break;
    case R_JMPL2:
      internal->howto = howto_table + 11;
      break;
    case R_MOV24B1:
      internal->howto = howto_table + 12;
      break;
    case R_MOV24B2:
      internal->howto = howto_table + 13;
      break;
    case R_MEM_INDIRECT:
      internal->howto = howto_table + 14;
      break;
    case R_PCRWORD_B:
      internal->howto = howto_table + 15;
      break;
    case R_MOVL1:
      internal->howto = howto_table + 16;
      break;
    case R_MOVL2:
      internal->howto = howto_table + 17;
      break;
    case R_BCC_INV:
      internal->howto = howto_table + 18;
      break;
    case R_JMP_DEL:
      internal->howto = howto_table + 19;
      break;
    default:
      abort ();
      break;
    }
}

#define RTYPE2HOWTO(internal, relocentry) rtype2howto(internal,relocentry)


/* Perform any necessary magic to the addend in a reloc entry */


#define CALC_ADDEND(abfd, symbol, ext_reloc, cache_ptr) \
 cache_ptr->addend =  ext_reloc.r_offset;


#define RELOC_PROCESSING(relent,reloc,symbols,abfd,section) \
 reloc_processing(relent, reloc, symbols, abfd, section)

static void
reloc_processing (relent, reloc, symbols, abfd, section)
     arelent * relent;
     struct internal_reloc *reloc;
     asymbol ** symbols;
     bfd * abfd;
     asection * section;
{
  relent->address = reloc->r_vaddr;
  rtype2howto (relent, reloc);

  if (((int) reloc->r_symndx) > 0)
    {
      relent->sym_ptr_ptr = symbols + obj_convert (abfd)[reloc->r_symndx];
    }
  else
    {
      relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
    }



  relent->addend = reloc->r_offset;

  relent->address -= section->vma;
  /*  relent->section = 0;*/
}

static boolean
h8300_symbol_address_p (abfd, input_section, address)
     bfd *abfd;
     asection *input_section;
     bfd_vma address;
{
  asymbol **s;

  s = _bfd_generic_link_get_symbols (abfd);
  BFD_ASSERT (s != (asymbol **) NULL);

  /* Search all the symbols for one in INPUT_SECTION with
     address ADDRESS.  */
  while (*s) 
    {
      asymbol *p = *s;
      if (p->section == input_section
	  && (input_section->output_section->vma
	      + input_section->output_offset
	      + p->value) == address)
	return true;
      s++;
    }    
  return false;
}


/* If RELOC represents a relaxable instruction/reloc, change it into
   the relaxed reloc, notify the linker that symbol addresses
   have changed (bfd_perform_slip) and return how much the current
   section has shrunk by.

   FIXME: Much of this code has knowledge of the ordering of entries
   in the howto table.  This needs to be fixed.  */

static int
h8300_reloc16_estimate(abfd, input_section, reloc, shrink, link_info)
     bfd *abfd;
     asection *input_section;
     arelent *reloc;
     unsigned int shrink;
     struct bfd_link_info *link_info;
{
  bfd_vma value;  
  bfd_vma dot;
  bfd_vma gap;
  static asection *last_input_section = NULL;
  static arelent *last_reloc = NULL;

  /* The address of the thing to be relocated will have moved back by 
     the size of the shrink - but we don't change reloc->address here,
     since we need it to know where the relocation lives in the source
     uncooked section.  */
  bfd_vma address = reloc->address - shrink;

  if (input_section != last_input_section)
    last_reloc = NULL;

  /* Only examine the relocs which might be relaxable.  */
  switch (reloc->howto->type)
    {     

    /* This is the 16/24 bit absolute branch which could become an 8 bit
       pc-relative branch.  */
    case R_JMP1:
    case R_JMPL1:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);

      /* Get the address of the next instruction (not the reloc).  */
      dot = (input_section->output_section->vma
	     + input_section->output_offset + address);

      /* Adjust for R_JMP1 vs R_JMPL1.  */
      dot += (reloc->howto->type == R_JMP1 ? 1 : 2);

      /* Compute the distance from this insn to the branch target.  */
      gap = value - dot;
  
      /* If the distance is within -128..+128 inclusive, then we can relax
	 this jump.  +128 is valid since the target will move two bytes
	 closer if we do relax this branch.  */
      if ((int)gap >= -128 && (int)gap <= 128 )
	{ 

	  /* It's possible we may be able to eliminate this branch entirely;
	     if the previous instruction is a branch around this instruction,
	     and there's no label at this instruction, then we can reverse
	     the condition on the previous branch and eliminate this jump.

	       original:			new:
		 bCC lab1			bCC' lab2
		 jmp lab2
		lab1:				lab1:
	
	     This saves 4 bytes instead of two, and should be relatively
	     common.  */

	  if (gap <= 126
	      && last_reloc
	      && last_reloc->howto->type == R_PCRBYTE)
	    {
	      bfd_vma last_value;
	      last_value = bfd_coff_reloc16_get_value (last_reloc, link_info,
						       input_section) + 1;

	      if (last_value == dot + 2
		  && last_reloc->address + 1 == reloc->address
		  && ! h8300_symbol_address_p (abfd, input_section, dot - 2))
		{
		  reloc->howto = howto_table + 19;
		  last_reloc->howto = howto_table + 18;
		  last_reloc->sym_ptr_ptr = reloc->sym_ptr_ptr;
		  last_reloc->addend = reloc->addend;
		  shrink += 4;
		  bfd_perform_slip (abfd, 4, input_section, address);
		  break;
		}
	    }

	  /* Change the reloc type.  */
	  reloc->howto = reloc->howto + 1;	  

	  /* This shrinks this section by two bytes.  */
	  shrink += 2;
	  bfd_perform_slip(abfd, 2, input_section, address);
	}
      break;

    /* This is the 16 bit pc-relative branch which could become an 8 bit
       pc-relative branch.  */
    case R_PCRWORD:
      /* Get the address of the target of this branch, add one to the value
         because the addend field in PCrel jumps is off by -1.  */
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section) + 1;
	
      /* Get the address of the next instruction if we were to relax.  */
      dot = input_section->output_section->vma +
	input_section->output_offset + address;
  
      /* Compute the distance from this insn to the branch target.  */
      gap = value - dot;

      /* If the distance is within -128..+128 inclusive, then we can relax
	 this jump.  +128 is valid since the target will move two bytes
	 closer if we do relax this branch.  */
      if ((int)gap >= -128 && (int)gap <= 128 )
	{ 
	  /* Change the reloc type.  */
	  reloc->howto = howto_table + 15;

	  /* This shrinks this section by two bytes.  */
	  shrink += 2;
	  bfd_perform_slip(abfd, 2, input_section, address);
	}
      break;

    /* This is a 16 bit absolute address in a mov.b insn, which can
       become an 8 bit absolute address if it's in the right range.  */
    case R_MOV16B1:
      /* Get the address of the data referenced by this mov.b insn.  */
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);

      /* The address is in 0xff00..0xffff inclusive on the h8300 or
	 0xffff00..0xffffff inclusive on the h8300h, then we can
	 relax this mov.b  */
      if ((bfd_get_mach (abfd) == bfd_mach_h8300
	   && value >= 0xff00
	   && value <= 0xffff)
	  || ((bfd_get_mach (abfd) == bfd_mach_h8300h
	       || bfd_get_mach (abfd) == bfd_mach_h8300s)
	      && value >= 0xffff00
	      && value <= 0xffffff))
	{
	  /* Change the reloc type.  */
	  reloc->howto = reloc->howto + 1;

	  /* This shrinks this section by two bytes.  */
	  shrink += 2;
	  bfd_perform_slip(abfd, 2, input_section, address);
	}
      break;

    /* Similarly for a 24 bit absolute address in a mov.b.  Note that
       if we can't relax this into an 8 bit absolute, we'll fall through
       and try to relax it into a 16bit absolute.  */
    case R_MOV24B1:
      /* Get the address of the data referenced by this mov.b insn.  */
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);

      /* The address is in 0xffff00..0xffffff inclusive on the h8300h,
	 then we can relax this mov.b  */
      if ((bfd_get_mach (abfd) == bfd_mach_h8300h
	   || bfd_get_mach (abfd) == bfd_mach_h8300s)
	  && value >= 0xffff00
	  && value <= 0xffffff)
	{
	  /* Change the reloc type.  */
	  reloc->howto = reloc->howto + 1;

	  /* This shrinks this section by four bytes.  */
	  shrink += 4;
	  bfd_perform_slip(abfd, 4, input_section, address);

	  /* Done with this reloc.  */
	  break;
	}

      /* FALLTHROUGH and try to turn the 32/24 bit reloc into a 16 bit
	 reloc.  */

    /* This is a 24/32 bit absolute address in a mov insn, which can
       become an 16 bit absolute address if it's in the right range.  */
    case R_MOVL1:
      /* Get the address of the data referenced by this mov insn.  */
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);

      /* If this address is in 0x0000..0x7fff inclusive or
         0xff8000..0xffffff inclusive, then it can be relaxed.  */
      if (value <= 0x7fff || value >= 0xff8000)
	{
	  /* Change the reloc type.  */
	  reloc->howto = howto_table + 17;

	  /* This shrinks this section by two bytes.  */
	  shrink += 2;
	  bfd_perform_slip(abfd, 2, input_section, address);
	}
      break;

      /* No other reloc types represent relaxing opportunities.  */
      default:
	break;
    }

  last_reloc = reloc;
  last_input_section = input_section;
  return shrink;
}


/* Handle relocations for the H8/300, including relocs for relaxed
   instructions.

   FIXME: Not all relocations check for overflow!  */

static void
h8300_reloc16_extra_cases (abfd, link_info, link_order, reloc, data, src_ptr,
			   dst_ptr)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     arelent *reloc;
     bfd_byte *data;
     unsigned int *src_ptr;
     unsigned int *dst_ptr;
{
  unsigned int src_address = *src_ptr;
  unsigned int dst_address = *dst_ptr;
  asection *input_section = link_order->u.indirect.section;
  bfd_vma value;
  bfd_vma dot;
  int gap,tmp;

  switch (reloc->howto->type)
    {

    /* Generic 8bit pc-relative relocation.  */
    case R_PCRBYTE:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);

      dot = (link_order->offset 
	     + dst_address 
	     + link_order->u.indirect.section->output_section->vma);

      gap = value - dot;

      /* Sanity check.  */
      if (gap < -128 || gap > 126)
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* Everything looks OK.  Apply the relocation and update the
	 src/dst address appropriately.  */

      bfd_put_8 (abfd, gap, data + dst_address);
      dst_address++;
      src_address++;

      /* All done.  */
      break;

    /* Generic 16bit pc-relative relocation.  */
    case R_PCRWORD:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);

      /* Get the address of the instruction (not the reloc).  */
      dot = (link_order->offset 
	     + dst_address 
	     + link_order->u.indirect.section->output_section->vma + 1);

      gap = value - dot;

      /* Sanity check.  */
      if (gap > 32766 || gap < -32768)
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* Everything looks OK.  Apply the relocation and update the
	 src/dst address appropriately.  */

      bfd_put_16 (abfd, gap, data + dst_address);
      dst_address += 2;
      src_address += 2;

      /* All done.  */
      break;

    /* Generic 8bit absolute relocation.  */
    case R_RELBYTE:
      /* Get the address of the object referenced by this insn.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Sanity check.  */
      if (value <= 0xff
	  || (value >= 0x0000ff00 && value <= 0x0000ffff)
  	  || (value >= 0x00ffff00 && value <= 0x00ffffff)
	  || (value >= 0xffffff00 && value <= 0xffffffff))
	{
	  /* Everything looks OK.  Apply the relocation and update the
	     src/dst address appropriately.  */

	  bfd_put_8 (abfd, value & 0xff, data + dst_address);
	  dst_address += 1;
	  src_address += 1;
	}
      else
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* All done.  */
      break;

    /* Various simple 16bit absolute relocations.  */
    case R_MOV16B1:
    case R_JMP1:
    case R_RELWORD:
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);
      bfd_put_16 (abfd, value, data + dst_address);
      dst_address += 2;
      src_address += 2;
      break;

    /* Various simple 24/32bit absolute relocations.  */
    case R_MOV24B1:
    case R_MOVL1:
    case R_RELLONG:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section),
      bfd_put_32 (abfd, value, data + dst_address);
      dst_address += 4;
      src_address += 4;
      break;

    /* Another 24/32bit absolute relocation.  */
    case R_JMPL1:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      value = ((value & 0x00ffffff)
	       | (bfd_get_32 (abfd, data + src_address) & 0xff000000));
      bfd_put_32 (abfd, value, data + dst_address);
      dst_address += 4;
      src_address += 4;
      break;

    /* A 16bit abolute relocation that was formerlly a 24/32bit
       absolute relocation.  */
    case R_MOVL2:
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Sanity check.  */
      if (value <= 0x7fff || value >= 0xff8000)
	{
	  /* Insert the 16bit value into the proper location.  */
	  bfd_put_16 (abfd, value, data + dst_address);

	  /* Fix the opcode.  For all the move insns, we simply
	     need to turn off bit 0x20 in the previous byte.  */
          data[dst_address - 1] &= ~0x20;
	  dst_address += 2;
	  src_address += 4;
	}
      else
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
        }
      break;

    /* A 16bit absolute branch that is now an 8-bit pc-relative branch.  */
    case R_JMP2:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Get the address of the next instruction.  */
      dot = (link_order->offset
	     + dst_address
	     + link_order->u.indirect.section->output_section->vma + 1);

      gap = value - dot;

      /* Sanity check.  */
      if (gap < -128 || gap > 126)
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* Now fix the instruction itself.  */
      switch (data[dst_address - 1])
	{
	case 0x5e:
	  /* jsr -> bsr */
	  bfd_put_8 (abfd, 0x55, data + dst_address - 1);
	  break;
	case 0x5a:
	  /* jmp ->bra */
	  bfd_put_8 (abfd, 0x40, data + dst_address - 1);
	  break;

	default:
	  abort ();
	}

      /* Write out the 8bit value.  */
      bfd_put_8 (abfd, gap, data + dst_address);

      dst_address += 1;
      src_address += 3;

      break;

    /* A 16bit pc-relative branch that is now an 8-bit pc-relative branch.  */
    case R_PCRWORD_B:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Get the address of the instruction (not the reloc).  */
      dot = (link_order->offset
	     + dst_address
	     + link_order->u.indirect.section->output_section->vma - 1);

      gap = value - dot;

      /* Sanity check.  */
      if (gap < -128 || gap > 126)
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* Now fix the instruction.  */
      switch (data[dst_address - 2])
	{
	case 0x58:
	  /* bCC:16 -> bCC:8 */
	  /* Get the condition code from the original insn.  */
	  tmp = data[dst_address - 1];
	  tmp &= 0xf0;
	  tmp >>= 4;

	  /* Now or in the high nibble of the opcode.  */
	  tmp |= 0x40;

	  /* Write it.  */
	  bfd_put_8 (abfd, tmp, data + dst_address - 2);
	  break;

	default:
	  abort ();
	}

	/* Output the target.  */
	bfd_put_8 (abfd, gap, data + dst_address - 1);

	/* We don't advance dst_address -- the 8bit reloc is applied at
	   dst_address - 1, so the next insn should begin at dst_address.  */
	src_address += 2;

	break;
      
    /* Similarly for a 24bit absolute that is now 8 bits.  */
    case R_JMPL2:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Get the address of the instruction (not the reloc).  */
      dot = (link_order->offset
	     + dst_address
	     + link_order->u.indirect.section->output_section->vma + 2);

      gap = value - dot;

      /* Fix the instruction.  */
      switch (data[src_address])
	{
	case 0x5e:
	  /* jsr -> bsr */
	  bfd_put_8 (abfd, 0x55, data + dst_address);
	  break;
	case 0x5a:
	  /* jmp ->bra */
	  bfd_put_8 (abfd, 0x40, data + dst_address);
	  break;
	default:
	  abort ();
	}

      bfd_put_8 (abfd, gap, data + dst_address + 1);
      dst_address += 2;
      src_address += 4;

      break;

    /* A 16bit absolute mov.b that is now an 8bit absolute mov.b.  */
    case R_MOV16B2:
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Sanity check.  */
      if (data[dst_address - 2] != 0x6a)
	abort ();

      /* Fix up the opcode.  */
      switch (data[src_address-1] & 0xf0)
	{
	case 0x00:
	  data[dst_address - 2] = (data[src_address-1] & 0xf) | 0x20;
	  break;
	case 0x80:
	  data[dst_address - 2] = (data[src_address-1] & 0xf) | 0x30;
	  break;
	default:
	  abort ();
	}

      bfd_put_8 (abfd, value & 0xff, data + dst_address - 1);
      src_address += 2;
      break;

    /* Similarly for a 24bit mov.b  */
    case R_MOV24B2:
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Sanity check.  */
      if (data[dst_address - 2] != 0x6a)
	abort ();

      /* Fix up the opcode.  */
      switch (data[src_address-1] & 0xf0)
	{
	case 0x20:
	  data[dst_address - 2] = (data[src_address-1] & 0xf) | 0x20;
	  break;
	case 0xa0:
	  data[dst_address - 2] = (data[src_address-1] & 0xf) | 0x30;
	  break;
	default:
	  abort ();
	}

      bfd_put_8 (abfd, value & 0xff, data + dst_address - 1);
      src_address += 4;
      break;

    case R_BCC_INV:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);

      dot = (link_order->offset 
	     + dst_address 
	     + link_order->u.indirect.section->output_section->vma) + 1;

      gap = value - dot;

      /* Sanity check.  */
      if (gap < -128 || gap > 126)
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* Everything looks OK.  Fix the condition in the instruction, apply
	 the relocation, and update the src/dst address appropriately.  */

      bfd_put_8 (abfd, bfd_get_8 (abfd, data + dst_address - 1) ^ 1,
		 data + dst_address - 1);
      bfd_put_8 (abfd, gap, data + dst_address);
      dst_address++;
      src_address++;

      /* All done.  */
      break;

    case R_JMP_DEL:
      src_address += 4;
      break;

    /* An 8bit memory indirect instruction (jmp/jsr).

       There's several things that need to be done to handle
       this relocation.

       If this is a reloc against the absolute symbol, then
       we should handle it just R_RELBYTE.  Likewise if it's
       for a symbol with a value ge 0 and le 0xff.

       Otherwise it's a jump/call through the function vector,
       and the linker is expected to set up the function vector
       and put the right value into the jump/call instruction.  */
    case R_MEM_INDIRECT:
      {
	/* We need to find the symbol so we can determine it's
	   address in the function vector table.  */
	asymbol *symbol;
	bfd_vma value;
	const char *name;
	struct funcvec_hash_entry *h;
	asection *vectors_sec = h8300_coff_hash_table (link_info)->vectors_sec;

	/* First see if this is a reloc against the absolute symbol
	   or against a symbol with a nonnegative value <= 0xff.  */
	symbol = *(reloc->sym_ptr_ptr);
	value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);
	if (symbol == bfd_abs_section_ptr->symbol
	    || value <= 0xff)
	  {
	    /* This should be handled in a manner very similar to
	       R_RELBYTES.   If the value is in range, then just slam
	       the value into the right location.  Else trigger a
	       reloc overflow callback.  */
	    if (value <= 0xff)
	      {
		bfd_put_8 (abfd, value, data + dst_address);
		dst_address += 1;
		src_address += 1;
	      }
	    else
	      {
		if (! ((*link_info->callbacks->reloc_overflow)
		       (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
			reloc->howto->name, reloc->addend, input_section->owner,
			input_section, reloc->address)))
		  abort ();
	      }
	    break;
	  }

	/* This is a jump/call through a function vector, and we're
	   expected to create the function vector ourselves. 

	   First look up this symbol in the linker hash table -- we need
	   the derived linker symbol which holds this symbol's index
	   in the function vector.  */
	name = symbol->name;
	if (symbol->flags & BSF_LOCAL)
	  {
	    char *new_name = bfd_malloc (strlen (name) + 9);
	    if (new_name == NULL)
	      abort ();

	    strcpy (new_name, name);
	    sprintf (new_name + strlen (name), "_%08x",
		     (int)symbol->section);
	    name = new_name;
	  }

	h = funcvec_hash_lookup (h8300_coff_hash_table (link_info)->funcvec_hash_table,
				 name, false, false);

	/* This shouldn't ever happen.  If it does that means we've got
	   data corruption of some kind.  Aborting seems like a reasonable
	   think to do here.  */
	if (h == NULL || vectors_sec == NULL)
	  abort ();

	/* Place the address of the function vector entry into the
	   reloc's address.  */
	bfd_put_8 (abfd,
		   vectors_sec->output_offset + h->offset,
		   data + dst_address);

	dst_address++;
	src_address++;

	/* Now create an entry in the function vector itself.  */
	if (bfd_get_mach (input_section->owner) == bfd_mach_h8300)
	  bfd_put_16 (abfd,
		      bfd_coff_reloc16_get_value (reloc,
						  link_info,
						  input_section),
		      vectors_sec->contents + h->offset);
	else if (bfd_get_mach (input_section->owner) == bfd_mach_h8300h
		 || bfd_get_mach (input_section->owner) == bfd_mach_h8300s)
	  bfd_put_32 (abfd,
		      bfd_coff_reloc16_get_value (reloc,
						  link_info,
						  input_section),
		      vectors_sec->contents + h->offset);
	else
	  abort ();

	/* Gross.  We've already written the contents of the vector section
	   before we get here...  So we write it again with the new data.  */
	bfd_set_section_contents (vectors_sec->output_section->owner,
				  vectors_sec->output_section,
				  vectors_sec->contents,
				  vectors_sec->output_offset,
				  vectors_sec->_raw_size);
	break;
      }

    default:
      abort ();
      break;

    }

  *src_ptr = src_address;
  *dst_ptr = dst_address;
}


/* Routine for the h8300 linker.

   This routine is necessary to handle the special R_MEM_INDIRECT
   relocs on the h8300.  It's responsible for generating a vectors
   section and attaching it to an input bfd as well as sizing
   the vectors section.  It also creates our vectors hash table.

   It uses the generic linker routines to actually add the symbols.
   from this BFD to the bfd linker hash table.  It may add a few
   selected static symbols to the bfd linker hash table.  */

static boolean
h8300_bfd_link_add_symbols(abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection *sec;
  struct funcvec_hash_table *funcvec_hash_table;

  /* If we haven't created a vectors section, do so now.  */
  if (!h8300_coff_hash_table (info)->vectors_sec)
    {
      flagword flags;

      /* Make sure the appropriate flags are set, including SEC_IN_MEMORY.  */
      flags = (SEC_ALLOC | SEC_LOAD
	       | SEC_HAS_CONTENTS | SEC_IN_MEMORY | SEC_READONLY);
      h8300_coff_hash_table (info)->vectors_sec = bfd_make_section (abfd,
								    ".vectors");

      /* If the section wasn't created, or we couldn't set the flags,
	 quit quickly now, rather than dieing a painful death later.  */
      if (! h8300_coff_hash_table (info)->vectors_sec
	  || ! bfd_set_section_flags (abfd,
				      h8300_coff_hash_table(info)->vectors_sec,
				      flags))
	return false;

      /* Also create the vector hash table.  */
      funcvec_hash_table = ((struct funcvec_hash_table *)
	bfd_alloc (abfd, sizeof (struct funcvec_hash_table)));

      if (!funcvec_hash_table)
	return false;

      /* And initialize the funcvec hash table.  */
      if (!funcvec_hash_table_init (funcvec_hash_table, abfd,
				    funcvec_hash_newfunc))
	{
	  bfd_release (abfd, funcvec_hash_table);
	  return false;
	}

      /* Store away a pointer to the funcvec hash table.  */
      h8300_coff_hash_table (info)->funcvec_hash_table = funcvec_hash_table;
    }

  /* Load up the function vector hash table.  */
  funcvec_hash_table = h8300_coff_hash_table (info)->funcvec_hash_table;

  /* Add the symbols using the generic code.  */
  _bfd_generic_link_add_symbols (abfd, info);

  /* Now scan the relocs for all the sections in this bfd; create
     additional space in the .vectors section as needed.  */
  for (sec = abfd->sections; sec; sec = sec->next)
    {
      long reloc_size, reloc_count, i;
      asymbol **symbols;
      arelent **relocs;

      /* Suck in the relocs, symbols & canonicalize them.  */
      reloc_size = bfd_get_reloc_upper_bound (abfd, sec);
      if (reloc_size <= 0)
	continue;

      relocs = (arelent **)bfd_malloc ((size_t)reloc_size);
      if (!relocs)
	return false;

      /* The symbols should have been read in by _bfd_generic link_add_symbols
	 call abovec, so we can cheat and use the pointer to them that was
	 saved in the above call.  */
      symbols = _bfd_generic_link_get_symbols(abfd);
      reloc_count = bfd_canonicalize_reloc (abfd, sec, relocs, symbols);
      if (reloc_count <= 0)
	{
	  free (relocs);
	  continue;
	}

      /* Now walk through all the relocations in this section.  */
      for (i = 0; i < reloc_count; i++)
	{
	  arelent *reloc = relocs[i];
	  asymbol *symbol = *(reloc->sym_ptr_ptr);
	  const char *name;

	  /* We've got an indirect reloc.  See if we need to add it
	     to the function vector table.   At this point, we have
	     to add a new entry for each unique symbol referenced
	     by an R_MEM_INDIRECT relocation except for a reloc
	     against the absolute section symbol.  */
	  if (reloc->howto->type == R_MEM_INDIRECT
	      && symbol != bfd_abs_section_ptr->symbol)

	    {
	      struct funcvec_hash_entry *h;

	      name = symbol->name;
	      if (symbol->flags & BSF_LOCAL)
		{
		  char *new_name = bfd_malloc (strlen (name) + 9);

		  if (new_name == NULL)
		    abort ();

		  strcpy (new_name, name);
		  sprintf (new_name + strlen (name), "_%08x",
			   (int)symbol->section);
		  name = new_name;
		}

	      /* Look this symbol up in the function vector hash table.  */
	      h = funcvec_hash_lookup (h8300_coff_hash_table (info)->funcvec_hash_table,
				       name, false, false);


	      /* If this symbol isn't already in the hash table, add
		 it and bump up the size of the hash table.  */
	      if (h == NULL)
		{
		  h = funcvec_hash_lookup (h8300_coff_hash_table (info)->funcvec_hash_table,
					   name, true, true);
		  if (h == NULL)
		    {
		      free (relocs);
		      return false;
		    }

		  /* Bump the size of the vectors section.  Each vector
		     takes 2 bytes on the h8300 and 4 bytes on the h8300h.  */
		  if (bfd_get_mach (abfd) == bfd_mach_h8300)
		    h8300_coff_hash_table (info)->vectors_sec->_raw_size += 2;
		  else if (bfd_get_mach (abfd) == bfd_mach_h8300h
			   || bfd_get_mach (abfd) == bfd_mach_h8300s)
		    h8300_coff_hash_table (info)->vectors_sec->_raw_size += 4;
		}
	    }
	}

      /* We're done with the relocations, release them.  */
      free (relocs);
    }

  /* Now actually allocate some space for the function vector.  It's
     wasteful to do this more than once, but this is easier.  */
  if (h8300_coff_hash_table (info)->vectors_sec->_raw_size != 0)
    {
      /* Free the old contents.  */
      if (h8300_coff_hash_table (info)->vectors_sec->contents)
	free (h8300_coff_hash_table (info)->vectors_sec->contents);

      /* Allocate new contents.  */
      h8300_coff_hash_table (info)->vectors_sec->contents
	= bfd_malloc (h8300_coff_hash_table (info)->vectors_sec->_raw_size);
    }

  return true;
}

#define coff_reloc16_extra_cases h8300_reloc16_extra_cases
#define coff_reloc16_estimate h8300_reloc16_estimate
#define coff_bfd_link_add_symbols h8300_bfd_link_add_symbols
#define coff_bfd_link_hash_table_create h8300_coff_link_hash_table_create

#define COFF_LONG_FILENAMES
#include "coffcode.h"


#undef coff_bfd_get_relocated_section_contents
#undef coff_bfd_relax_section
#define coff_bfd_get_relocated_section_contents \
  bfd_coff_reloc16_get_relocated_section_contents
#define coff_bfd_relax_section bfd_coff_reloc16_relax_section


CREATE_BIG_COFF_TARGET_VEC (h8300coff_vec, "coff-h8300", BFD_IS_RELAXABLE, 0, '_', NULL)
