/* Disassembler interface for targets using CGEN. -*- C -*-
   CGEN: Cpu tools GENerator

This file is used to generate m32r-dis.c.

Copyright (C) 1996, 1997 Free Software Foundation, Inc.

This file is part of the GNU Binutils and GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sysdep.h"
#include <stdio.h>
#include "ansidecl.h"
#include "dis-asm.h"
#include "m32r-opc.h"
#include "bfd.h"

/* ??? The layout of this stuff is still work in progress.
   For speed in assembly/disassembly, we use inline functions.  That of course
   will only work for GCC.  When this stuff is finished, we can decide whether
   to keep the inline functions (and only get the performance increase when
   compiled with GCC), or switch to macros, or use something else.
*/

/* Default text to print if an instruction isn't recognized.  */
#define UNKNOWN_INSN_MSG "*unknown*"

/* FIXME: Machine generate.  */
#ifndef CGEN_PCREL_OFFSET
#define CGEN_PCREL_OFFSET 0
#endif

static int print_insn PARAMS ((bfd_vma, disassemble_info *, char *, int));

static int extract_insn_normal
     PARAMS ((const struct cgen_insn *, void *, cgen_insn_t, struct cgen_fields *));
static void print_insn_normal
     PARAMS ((void *, const struct cgen_insn *, struct cgen_fields *, bfd_vma, int));

/* Default extraction routine.

   ATTRS is a mask of the boolean attributes.  We only need `unsigned',
   but for generality we take a bitmask of all of them.  */

static int
extract_normal (buf_ctrl, insn_value, attrs, start, length, shift, total_length, valuep)
     void *buf_ctrl;
     cgen_insn_t insn_value;
     unsigned int attrs;
     int start, length, shift, total_length;
     long *valuep;
{
  long value;

#ifdef CGEN_INT_INSN
#if 0
  value = ((insn_value >> (CGEN_BASE_INSN_BITSIZE - (start + length)))
	   & ((1 << length) - 1));
#else
  value = ((insn_value >> (total_length - (start + length)))
	   & ((1 << length) - 1));
#endif
  if (! (attrs & CGEN_ATTR_MASK (CGEN_OPERAND_UNSIGNED))
      && (value & (1 << (length - 1))))
    value -= 1 << length;
#else
  /* FIXME: unfinished */
#endif

  /* This is backwards as we undo the effects of insert_normal.  */
  if (shift < 0)
    value >>= -shift;
  else
    value <<= shift;

  *valuep = value;
  return 1;
}

/* Default print handler.  */

static void
print_normal (dis_info, value, attrs, pc, length)
     void *dis_info;
     long value;
     unsigned int attrs;
     unsigned long pc; /* FIXME: should be bfd_vma */
     int length;
{
  disassemble_info *info = dis_info;

  /* Print the operand as directed by the attributes.  */
  if (attrs & CGEN_ATTR_MASK (CGEN_OPERAND_FAKE))
    ; /* nothing to do (??? at least not yet) */
  else if (attrs & CGEN_ATTR_MASK (CGEN_OPERAND_PCREL_ADDR))
    (*info->print_address_func) (pc + CGEN_PCREL_OFFSET + value, info);
  /* ??? Not all cases of this are currently caught.  */
  else if (attrs & CGEN_ATTR_MASK (CGEN_OPERAND_ABS_ADDR))
    /* FIXME: Why & 0xffffffff?  */
    (*info->print_address_func) ((bfd_vma) value & 0xffffffff, info);
  else if (attrs & CGEN_ATTR_MASK (CGEN_OPERAND_UNSIGNED))
    (*info->fprintf_func) (info->stream, "0x%lx", value);
  else
    (*info->fprintf_func) (info->stream, "%ld", value);
}

/* Keyword print handler.  */

static void
print_keyword (dis_info, keyword_table, value, attrs)
     void *dis_info;
     struct cgen_keyword *keyword_table;
     long value;
     CGEN_ATTR *attrs;
{
  disassemble_info *info = dis_info;
  const struct cgen_keyword_entry *ke;

  ke = cgen_keyword_lookup_value (keyword_table, value);
  if (ke != NULL)
    (*info->fprintf_func) (info->stream, "%s", ke->name);
  else
    (*info->fprintf_func) (info->stream, "???");
}

/* -- disassembler routines inserted here */
/* -- dis.c */

#undef CGEN_PRINT_INSN
#define CGEN_PRINT_INSN my_print_insn

static int
my_print_insn (pc, info, buf, buflen)
     bfd_vma pc;
     disassemble_info *info;
     char *buf;
     int buflen;
{
  unsigned long insn_value;

  /* 32 bit insn?  */
  if ((pc & 3) == 0 && (buf[0] & 0x80) != 0)
    return print_insn (pc, info, buf, buflen);

  /* Print the first insn.  */
  if ((pc & 3) == 0)
    {
      if (print_insn (pc, info, buf, 16) == 0)
	(*info->fprintf_func) (info->stream, UNKNOWN_INSN_MSG);
      buf += 2;
    }

  if (buf[0] & 0x80)
    {
      /* Parallel.  */
      (*info->fprintf_func) (info->stream, " || ");
      buf[0] &= 0x7f;
    }
  else
    (*info->fprintf_func) (info->stream, " -> ");

  /* The "& 3" is to ensure the branch address is computed correctly
     [if it is a branch].  */
  if (print_insn (pc & ~ (bfd_vma) 3, info, buf, 16) == 0)
    (*info->fprintf_func) (info->stream, UNKNOWN_INSN_MSG);

  return (pc & 3) ? 2 : 4;
}

/* -- */

/* Main entry point for operand extraction.

   This function is basically just a big switch statement.  Earlier versions
   used tables to look up the function to use, but
   - if the table contains both assembler and disassembler functions then
     the disassembler contains much of the assembler and vice-versa,
   - there's a lot of inlining possibilities as things grow,
   - using a switch statement avoids the function call overhead.

   This function could be moved into `print_insn_normal', but keeping it
   separate makes clear the interface between `print_insn_normal' and each of
   the handlers.
*/

CGEN_INLINE int
m32r_cgen_extract_operand (opindex, buf_ctrl, insn_value, fields)
     int opindex;
     void *buf_ctrl;
     cgen_insn_t insn_value;
     struct cgen_fields *fields;
{
  int length;

  switch (opindex)
    {
    case 0 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_UNSIGNED), 12, 4, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_r2);
      break;
    case 1 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_UNSIGNED), 4, 4, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_r1);
      break;
    case 2 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_UNSIGNED), 4, 4, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_r1);
      break;
    case 3 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_UNSIGNED), 12, 4, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_r2);
      break;
    case 4 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_UNSIGNED), 12, 4, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_r2);
      break;
    case 5 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_UNSIGNED), 4, 4, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_r1);
      break;
    case 6 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0, 8, 8, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_simm8);
      break;
    case 7 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0, 16, 16, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_simm16);
      break;
    case 8 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_UNSIGNED), 12, 4, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_uimm4);
      break;
    case 9 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_UNSIGNED), 11, 5, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_uimm5);
      break;
    case 10 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_UNSIGNED), 16, 16, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_uimm16);
      break;
    case 11 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_SIGN_OPT)|(1<<CGEN_OPERAND_UNSIGNED), 16, 16, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_hi16);
      break;
    case 12 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0, 16, 16, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_simm16);
      break;
    case 13 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_UNSIGNED), 16, 16, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_uimm16);
      break;
    case 14 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_ABS_ADDR)|(1<<CGEN_OPERAND_UNSIGNED), 8, 24, 0, CGEN_FIELDS_BITSIZE (fields), &fields->f_uimm24);
      break;
    case 15 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_PCREL_ADDR), 8, 8, 2, CGEN_FIELDS_BITSIZE (fields), &fields->f_disp8);
      break;
    case 16 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_PCREL_ADDR), 16, 16, 2, CGEN_FIELDS_BITSIZE (fields), &fields->f_disp16);
      break;
    case 17 :
      length = extract_normal (NULL /*FIXME*/, insn_value, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_PCREL_ADDR), 8, 24, 2, CGEN_FIELDS_BITSIZE (fields), &fields->f_disp24);
      break;

    default :
      fprintf (stderr, "Unrecognized field %d while decoding insn.\n",
	       opindex);
      abort ();
    }

  return length;
}

/* Main entry point for printing operands.

   This function is basically just a big switch statement.  Earlier versions
   used tables to look up the function to use, but
   - if the table contains both assembler and disassembler functions then
     the disassembler contains much of the assembler and vice-versa,
   - there's a lot of inlining possibilities as things grow,
   - using a switch statement avoids the function call overhead.

   This function could be moved into `print_insn_normal', but keeping it
   separate makes clear the interface between `print_insn_normal' and each of
   the handlers.
*/

CGEN_INLINE void
m32r_cgen_print_operand (opindex, info, fields, attrs, pc, length)
     int opindex;
     disassemble_info *info;
     struct cgen_fields *fields;
     int attrs;
     bfd_vma pc;
     int length;
{
  switch (opindex)
    {
    case 0 :
      print_keyword (info, & m32r_cgen_opval_h_gr, fields->f_r2, 0|(1<<CGEN_OPERAND_UNSIGNED));
      break;
    case 1 :
      print_keyword (info, & m32r_cgen_opval_h_gr, fields->f_r1, 0|(1<<CGEN_OPERAND_UNSIGNED));
      break;
    case 2 :
      print_keyword (info, & m32r_cgen_opval_h_gr, fields->f_r1, 0|(1<<CGEN_OPERAND_UNSIGNED));
      break;
    case 3 :
      print_keyword (info, & m32r_cgen_opval_h_gr, fields->f_r2, 0|(1<<CGEN_OPERAND_UNSIGNED));
      break;
    case 4 :
      print_keyword (info, & m32r_cgen_opval_h_cr, fields->f_r2, 0|(1<<CGEN_OPERAND_UNSIGNED));
      break;
    case 5 :
      print_keyword (info, & m32r_cgen_opval_h_cr, fields->f_r1, 0|(1<<CGEN_OPERAND_UNSIGNED));
      break;
    case 6 :
      print_normal (info, fields->f_simm8, 0, pc, length);
      break;
    case 7 :
      print_normal (info, fields->f_simm16, 0, pc, length);
      break;
    case 8 :
      print_normal (info, fields->f_uimm4, 0|(1<<CGEN_OPERAND_UNSIGNED), pc, length);
      break;
    case 9 :
      print_normal (info, fields->f_uimm5, 0|(1<<CGEN_OPERAND_UNSIGNED), pc, length);
      break;
    case 10 :
      print_normal (info, fields->f_uimm16, 0|(1<<CGEN_OPERAND_UNSIGNED), pc, length);
      break;
    case 11 :
      print_normal (info, fields->f_hi16, 0|(1<<CGEN_OPERAND_SIGN_OPT)|(1<<CGEN_OPERAND_UNSIGNED), pc, length);
      break;
    case 12 :
      print_normal (info, fields->f_simm16, 0, pc, length);
      break;
    case 13 :
      print_normal (info, fields->f_uimm16, 0|(1<<CGEN_OPERAND_UNSIGNED), pc, length);
      break;
    case 14 :
      print_normal (info, fields->f_uimm24, 0|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_ABS_ADDR)|(1<<CGEN_OPERAND_UNSIGNED), pc, length);
      break;
    case 15 :
      print_normal (info, fields->f_disp8, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_PCREL_ADDR), pc, length);
      break;
    case 16 :
      print_normal (info, fields->f_disp16, 0|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_PCREL_ADDR), pc, length);
      break;
    case 17 :
      print_normal (info, fields->f_disp24, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_PCREL_ADDR), pc, length);
      break;

    default :
      fprintf (stderr, "Unrecognized field %d while printing insn.\n",
	       opindex);
    abort ();
  }
}

cgen_extract_fn *m32r_cgen_extract_handlers[] = {
  0, /* default */
  extract_insn_normal,
};

cgen_print_fn *m32r_cgen_print_handlers[] = {
  0, /* default */
  print_insn_normal,
};


void
m32r_cgen_init_dis (mach, endian)
     int mach;
     enum cgen_endian endian;
{
  m32r_cgen_init_tables (mach);
  cgen_set_cpu (& m32r_cgen_opcode_data, mach, endian);
  cgen_dis_init ();
}


/* Default insn extractor.

   The extracted fields are stored in DIS_FLDS.
   BUF_CTRL is used to handle reading variable length insns (FIXME: not done).
   Return the length of the insn in bits, or 0 if no match.  */

static int
extract_insn_normal (insn, buf_ctrl, insn_value, fields)
     const struct cgen_insn *insn;
     void *buf_ctrl;
     cgen_insn_t insn_value;
     struct cgen_fields *fields;
{
  const struct cgen_syntax *syntax = CGEN_INSN_SYNTAX (insn);
  const unsigned char *syn;

  /* ??? Some of the operand extract routines need to know the insn length,
     which might be computed as we go.  Set a default value and it'll be
     modified as necessary.  */
  CGEN_FIELDS_BITSIZE (fields) = CGEN_INSN_BITSIZE (insn);

  CGEN_INIT_EXTRACT ();

  for (syn = syntax->syntax; *syn; ++syn)
    {
      int length;

      if (CGEN_SYNTAX_CHAR_P (*syn))
	continue;

      length = m32r_cgen_extract_operand (CGEN_SYNTAX_FIELD (*syn),
					   buf_ctrl, insn_value, fields);
      if (length == 0)
	return 0;
    }

  /* We recognized and successfully extracted this insn.
     If a length is recorded with this insn, it has a fixed length.
     Otherwise we require the syntax string to have a fake operand which
     sets the `length' field in `flds'.  */
  /* FIXME: wip */
  if (syntax->length > 0)
    return syntax->length;
  return fields->length;
}

/* Default insn printer.

   DIS_INFO is defined as `void *' so the disassembler needn't know anything
   about disassemble_info.
*/

static void
print_insn_normal (dis_info, insn, fields, pc, length)
     void *dis_info;
     const struct cgen_insn *insn;
     struct cgen_fields *fields;
     bfd_vma pc;
     int length;
{
  const struct cgen_syntax *syntax = CGEN_INSN_SYNTAX (insn);
  disassemble_info *info = dis_info;
  const unsigned char *syn;

  CGEN_INIT_PRINT ();

  for (syn = syntax->syntax; *syn; ++syn)
    {
      if (CGEN_SYNTAX_CHAR_P (*syn))
	{
	  (*info->fprintf_func) (info->stream, "%c", CGEN_SYNTAX_CHAR (*syn));
	  continue;
	}

      /* We have an operand.  */
      m32r_cgen_print_operand (CGEN_SYNTAX_FIELD (*syn), info,
				fields, CGEN_INSN_ATTRS (insn), pc, length);
    }
}

/* Default value for CGEN_PRINT_INSN.
   Given BUFLEN bytes (target byte order) read into BUF, look up the
   insn in the instruction table and disassemble it.

   The result is the size of the insn in bytes.  */

#ifndef CGEN_PRINT_INSN
#define CGEN_PRINT_INSN print_insn
#endif

static int
print_insn (pc, info, buf, buflen)
     bfd_vma pc;
     disassemble_info *info;
     char *buf;
     int buflen;
{
  int i;
  unsigned long insn_value;
  const CGEN_INSN_LIST *insn_list;

  switch (buflen)
    {
    case 8:
      insn_value = buf[0];
      break;
    case 16:
      insn_value = info->endian == BFD_ENDIAN_BIG ? bfd_getb16 (buf) : bfd_getl16 (buf);
      break;
    case 32:
      insn_value = info->endian == BFD_ENDIAN_BIG ? bfd_getb32 (buf) : bfd_getl32 (buf);
      break;
    default:
      abort ();
    }

  /* The instructions are stored in hash lists.
     Pick the first one and keep trying until we find the right one.  */

  insn_list = CGEN_DIS_LOOKUP_INSN (buf, insn_value);
  while (insn_list != NULL)
    {
      const CGEN_INSN *insn = insn_list->insn;
      const struct cgen_syntax *syntax = CGEN_INSN_SYNTAX (insn);
      struct cgen_fields fields;
      int length;

#if 0 /* not needed as insn shouldn't be in hash lists if not supported */
      /* Supported by this cpu?  */
      if (! m32r_cgen_insn_supported (insn))
	continue;
#endif

      /* Basic bit mask must be correct.  */
      /* ??? May wish to allow target to defer this check until the extract
	 handler.  */
      if ((insn_value & syntax->mask) == syntax->value)
	{
	  /* Printing is handled in two passes.  The first pass parses the
	     machine insn and extracts the fields.  The second pass prints
	     them.  */

	  length = (*CGEN_EXTRACT_FN (insn)) (insn, NULL, insn_value, &fields);
	  if (length > 0)
	    {
	      (*CGEN_PRINT_FN (insn)) (info, insn, &fields, pc, length);
	      /* length is in bits, result is in bytes */
	      return length / 8;
	    }
	}

      insn_list = CGEN_DIS_NEXT_INSN (insn_list);
    }

  return 0;
}

/* Main entry point.
   Print one instruction from PC on INFO->STREAM.
   Return the size of the instruction (in bytes).  */

int
print_insn_m32r (pc, info)
     bfd_vma pc;
     disassemble_info *info;
{
  char buffer[CGEN_MAX_INSN_SIZE];
  int status, length;
  static int initialized = 0;
  static int current_mach = 0;
  static int current_big_p = 0;
  int mach = info->mach;
  int big_p = info->endian == BFD_ENDIAN_BIG;

  /* If we haven't initialized yet, or if we've switched cpu's, initialize.  */
  if (!initialized || mach != current_mach || big_p != current_big_p)
    {
      initialized = 1;
      current_mach = mach;
      current_big_p = big_p;
      m32r_cgen_init_dis (mach, big_p ? CGEN_ENDIAN_BIG : CGEN_ENDIAN_LITTLE);
    }

  /* Read enough of the insn so we can look it up in the hash lists.  */

  status = (*info->read_memory_func) (pc, buffer, CGEN_BASE_INSN_SIZE, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, pc, info);
      return -1;
    }

  /* We try to have as much common code as possible.
     But at this point some targets need to take over.  */
  /* ??? Some targets may need a hook elsewhere.  Try to avoid this,
     but if not possible, try to move this hook elsewhere rather than
     have two hooks.  */
  length = CGEN_PRINT_INSN (pc, info, buffer, CGEN_BASE_INSN_BITSIZE);
  if (length)
    return length;

  (*info->fprintf_func) (info->stream, UNKNOWN_INSN_MSG);
  return CGEN_DEFAULT_INSN_SIZE;
}
