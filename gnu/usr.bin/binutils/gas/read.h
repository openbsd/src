/* read.h - of read.c

   Copyright (C) 1986, 1990, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

extern char *input_line_pointer;/* -> char we are parsing now. */

#define PERMIT_WHITESPACE	/* Define to make whitespace be allowed in */
/* many syntactically unnecessary places. */
/* Normally undefined. For compatibility */
/* with ancient GNU cc. */
/* #undef PERMIT_WHITESPACE */

#ifdef PERMIT_WHITESPACE
#define SKIP_WHITESPACE() {if (* input_line_pointer == ' ') ++ input_line_pointer;}
#else
#define SKIP_WHITESPACE() know(*input_line_pointer != ' ' )
#endif


#define	LEX_NAME	(1)	/* may continue a name */
#define LEX_BEGIN_NAME	(2)	/* may begin a name */

#define is_name_beginner(c) \
  ( lex_type[(unsigned char) (c)] & LEX_BEGIN_NAME )
#define is_part_of_name(c) \
  ( lex_type[(unsigned char) (c)] & LEX_NAME       )

#ifndef is_a_char
#define CHAR_MASK	(0xff)
#define NOT_A_CHAR	(CHAR_MASK+1)
#define is_a_char(c)	(((unsigned)(c)) <= CHAR_MASK)
#endif /* is_a_char() */

extern char lex_type[];
extern char is_end_of_line[];

extern int target_big_endian;

/* These are initialized by the CPU specific target files (tc-*.c).  */
extern const char comment_chars[];
extern const char line_comment_chars[];
extern const char line_separator_chars[];

/* This flag whether to generate line info for asm file */
extern int generate_asm_lineno;

/* The offset in the absolute section.  */
extern addressT abs_section_offset;

/* The label on a line, used by some of the pseudo-ops.  */
extern symbolS *line_label;

/* This is used to support MRI common sections.  */
extern symbolS *mri_common_symbol;

unsigned int get_stab_string_offset PARAMS ((const char *string,
					     const char *stabstr_secname));

char *demand_copy_C_string PARAMS ((int *len_pointer));
char get_absolute_expression_and_terminator PARAMS ((long *val_pointer));
offsetT get_absolute_expression PARAMS ((void));
unsigned int next_char_of_string PARAMS ((void));
void s_mri_sect PARAMS ((char *));
char *mri_comment_field PARAMS ((char *));
void mri_comment_end PARAMS ((char *, int));
void add_include_dir PARAMS ((char *path));
void cons PARAMS ((int nbytes));
void demand_empty_rest_of_line PARAMS ((void));
void emit_expr PARAMS ((expressionS *exp, unsigned int nbytes));
void equals PARAMS ((char *sym_name));
void float_cons PARAMS ((int float_type));
void ignore_rest_of_line PARAMS ((void));
void pseudo_set PARAMS ((symbolS * symbolP));
void read_a_source_file PARAMS ((char *name));
void read_begin PARAMS ((void));
void s_abort PARAMS ((int));
void s_align_bytes PARAMS ((int arg));
void s_align_ptwo PARAMS ((int));
void s_app_file PARAMS ((int));
void s_app_line PARAMS ((int));
void s_comm PARAMS ((int));
void s_data PARAMS ((int));
void s_desc PARAMS ((int));
void s_else PARAMS ((int arg));
void s_end PARAMS ((int arg));
void s_endif PARAMS ((int arg));
void s_err PARAMS ((int));
void s_fail PARAMS ((int));
void s_fill PARAMS ((int));
void s_float_space PARAMS ((int mult));
void s_globl PARAMS ((int arg));
void s_if PARAMS ((int arg));
void s_ifc PARAMS ((int arg));
void s_ifdef PARAMS ((int arg));
void s_ifeqs PARAMS ((int arg));
void s_ignore PARAMS ((int arg));
void s_include PARAMS ((int arg));
void s_irp PARAMS ((int arg));
void s_lcomm PARAMS ((int needs_align));
void s_lsym PARAMS ((int));
void s_macro PARAMS ((int));
void s_mexit PARAMS ((int));
void s_mri_common PARAMS ((int));
void s_org PARAMS ((int));
void s_print PARAMS ((int));
void s_purgem PARAMS ((int));
void s_rept PARAMS ((int));
void s_set PARAMS ((int));
void s_space PARAMS ((int mult));
void s_stab PARAMS ((int what));
void s_struct PARAMS ((int));
void s_text PARAMS ((int));
void stringer PARAMS ((int append_zero));
void s_xstab PARAMS ((int what));
void s_rva PARAMS ((int));

/* end of read.h */
