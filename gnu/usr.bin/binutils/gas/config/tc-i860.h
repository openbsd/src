/* tc-i860.h -- Header file for the I860
   Copyright (C) 1991, 92, 95, 1998 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with GAS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define TC_I860 1

#define TARGET_BYTES_BIG_ENDIAN 1

#define WORKING_DOT_WORD

#define tc_headers_hook(a)		{;}	/* not used */
#define tc_crawl_symbol_chain(a)	{;}	/* not used */
#define tc_aout_pre_write_hook(x)	{;}	/* not used */

#define md_operand(x)

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of tc-i860.h */
