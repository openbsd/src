/* tc-mn10300.h -- Header file for tc-mn10300.c.
   Copyright (C) 1996 Free Software Foundation, Inc.

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
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#define TC_MN10300

#ifndef BFD_ASSEMBLER
 #error MN10300 support requires BFD_ASSEMBLER
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_mn10300

#define TARGET_FORMAT "elf32-mn10300"

#define MD_APPLY_FIX3
#define md_operand(x)

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define LOCAL_LABEL(name) ((name[0] == '.' \
			    && (name[1] == 'L' || name[1] == '.')) \
			   || (name[0] == '_' && name[1] == '.' && name[2] == 'L' \
			       && name[3] == '_'))

#define FAKE_LABEL_NAME ".L0\001"
#define DIFF_EXPR_OK		/* .-foo gets turned into PC relative relocs */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define md_number_to_chars number_to_chars_littleendian
