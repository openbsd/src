/* tc-i960.h - Basic 80960 instruction formats.
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with GAS; see the file COPYING.  If not, write
   to the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#ifndef TC_I960
#define TC_I960 1

/*
 * The 'COJ' instructions are actually COBR instructions with the 'b' in
 * the mnemonic replaced by a 'j';  they are ALWAYS "de-optimized" if necessary:
 * if the displacement will not fit in 13 bits, the assembler will replace them
 * with the corresponding compare and branch instructions.
 *
 * All of the 'MEMn' instructions are the same format; the 'n' in the name
 * indicates the default index scale factor (the size of the datum operated on).
 *
 * The FBRA formats are not actually an instruction format.  They are the
 * "convenience directives" for branching on floating-point comparisons,
 * each of which generates 2 instructions (a 'bno' and one other branch).
 *
 * The CALLJ format is not actually an instruction format.  It indicates that
 * the instruction generated (a CTRL-format 'call') should have its relocation
 * specially flagged for link-time replacement with a 'bal' or 'calls' if
 * appropriate.
 */

/* tailor gas */
#define SYMBOLS_NEED_BACKPOINTERS
#define LOCAL_LABELS_FB 1
#define BITFIELD_CONS_EXPRESSIONS
#define MRI_MODE_NEEDS_PSEUDO_DOT 1

/* tailor the coff format */
#define BFD_ARCH				bfd_arch_i960
#define COFF_FLAGS				F_AR32WR
#define COFF_MAGIC				I960ROMAGIC
#define OBJ_COFF_SECTION_HEADER_HAS_ALIGNMENT
#define OBJ_COFF_MAX_AUXENTRIES			(2)
#define TC_COUNT_RELOC(FIXP)			(!(FIXP)->fx_done)
#define TC_COFF_FIX2RTYPE(FIXP)			tc_coff_fix2rtype(FIXP)
#define TC_COFF_SIZEMACHDEP(FRAGP)		tc_coff_sizemachdep(FRAGP)
#define TC_COFF_SET_MACHINE(HDRS)		tc_headers_hook (HDRS)
extern void tc_headers_hook ();
extern short tc_coff_fix2rtype ();
extern int tc_coff_sizemachdep ();

/* MEANING OF 'n_other' in the symbol record.
 *
 * If non-zero, the 'n_other' fields indicates either a leaf procedure or
 * a system procedure, as follows:
 *
 *	1 <= n_other <= 32 :
 *		The symbol is the entry point to a system procedure.
 *		'n_value' is the address of the entry, as for any other
 *		procedure.  The system procedure number (which can be used in
 *		a 'calls' instruction) is (n_other-1).  These entries come from
 *		'.sysproc' directives.
 *
 *	n_other == N_CALLNAME
 *		the symbol is the 'call' entry point to a leaf procedure.
 *		The *next* symbol in the symbol table must be the corresponding
 *		'bal' entry point to the procedure (see following).  These
 *		entries come from '.leafproc' directives in which two different
 *		symbols are specified (the first one is represented here).
 *
 *
 *	n_other == N_BALNAME
 *		the symbol is the 'bal' entry point to a leaf procedure.
 *		These entries result from '.leafproc' directives in which only
 *		one symbol is specified, or in which the same symbol is
 *		specified twice.
 *
 * Note that an N_CALLNAME entry *must* have a corresponding N_BALNAME entry,
 * but not every N_BALNAME entry must have an N_CALLNAME entry.
 */
#define	N_CALLNAME	((char)-1)
#define	N_BALNAME	((char)-2)

/* i960 uses a custom relocation record. */

/* let obj-aout.h know */
#define CUSTOM_RELOC_FORMAT 1
/* let aout_gnu.h know */
#define N_RELOCATION_INFO_DECLARED 1
struct relocation_info
  {
    int r_address;		/* File address of item to be relocated	*/
    unsigned
      r_index:24,		/* Index of symbol on which relocation is based*/
      r_pcrel:1,		/* 1 => relocate PC-relative; else absolute
				 *	On i960, pc-relative implies 24-bit
				 *	address, absolute implies 32-bit.
				 */
      r_length:2,		/* Number of bytes to relocate:
				 *	0 => 1 byte
				 *	1 => 2 bytes
				 *	2 => 4 bytes -- only value used for i960
				 */
      r_extern:1, r_bsr:1,	/* Something for the GNU NS32K assembler */
      r_disp:1,			/* Something for the GNU NS32K assembler */
      r_callj:1,		/* 1 if relocation target is an i960 'callj' */
      nuthin:1;			/* Unused				*/
  };

#ifdef OBJ_COFF
#define TC_ADJUST_RELOC_COUNT(FIXP,COUNT) \
  { fixS *tcfixp = (FIXP); \
    for (;tcfixp;tcfixp=tcfixp->fx_next) \
      if (tcfixp->fx_tcbit && tcfixp->fx_addsy != 0) \
        ++(COUNT); \
  }
#endif

extern int i960_validate_fix PARAMS ((struct fix *, segT, struct symbol **));
#define TC_VALIDATE_FIX(FIXP,SEGTYPE,LABEL) \
	if (i960_validate_fix (FIXP, SEGTYPE, &add_symbolP) != 0) goto LABEL

#define tc_fix_adjustable(FIXP)		((FIXP)->fx_bsr == 0)

void brtab_emit PARAMS ((void));
#define md_end()	brtab_emit ()

void reloc_callj ();		/* this is really reloc_callj(fixS *fixP) but I don't want to change header inclusion order. */
void tc_set_bal_of_call ();	/* this is really tc_set_bal_of_call(symbolS *callP, symbolS *balP) */

char *_tc_get_bal_of_call ();	/* this is really symbolS *tc_get_bal_of_call(symbolS *callP). */
#define tc_get_bal_of_call(c)	((symbolS *) _tc_get_bal_of_call(c))

void i960_handle_align ();
#define HANDLE_ALIGN(FRAG)	i960_handle_align (FRAG)
#define NEED_FX_R_TYPE
#define NO_RELOC -1

#define md_operand(x)

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

#define LINKER_RELAXING_SHRINKS_ONLY

#define TC_FIX_TYPE struct { unsigned bsr : 1; }
#define fx_bsr tc_fix_data.bsr
#define TC_INIT_FIX_DATA(F)	((F)->tc_fix_data.bsr = 0)

#endif

/* end of tc-i960.h */
