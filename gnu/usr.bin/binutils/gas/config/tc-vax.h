/* tc-vax.h -- Header file for tc-vax.c.
   Copyright 1987, 1991, 1992, 1993, 1995, 1996, 1997, 2000
   Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define TC_VAX 1

#define TARGET_BYTES_BIG_ENDIAN 0

#define NO_RELOC 0
#define NOP_OPCODE 0x01

#define tc_aout_pre_write_hook(x)	{;}	/* not used */
#define tc_crawl_symbol_chain(a)	{;}	/* not used */
#define tc_headers_hook(a)		{;}	/* not used */
#define md_operand(x)

long md_chars_to_number PARAMS ((unsigned char *, int));

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */
