/* This test file is part of GDB, the GNU debugger.

   Copyright 1995, 1997, 1999, 2003, 2004 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Please email any bugs, comments, and/or additions to this file to:
   bug-gdb@prep.ai.mit.edu  */

/* Sometimes the behavior of a test depends upon the compiler used to
   compile the test program.  A test script can call get_compiler_info
   to figure out the compiler version and test_compiler_info to test it.

   get_compiler_info runs the preprocessor on this file and then eval's
   the result.  This sets various symbols for use by test_compiler_info.

   TODO: make compiler_info a local variable for get_compiler_info and
   test_compiler_info.

   TODO: all clients should use test_compiler_info and should not
   use gcc_compiled, hp_cc_compiler, or hp_aCC_compiler.

   TODO: purge signed_keyword_not_used.  */

set compiler_info ""

#if defined (__GNUC__)
set compiler_info [join {gcc __GNUC__ __GNUC_MINOR__ } -]
set gcc_compiled __GNUC__
#else
set gcc_compiled 0
#endif

#if defined (__HP_cc)
set compiler_info [join {hpcc __HP_cc} -]
set hp_cc_compiler __HP_cc
#else
set hp_cc_compiler 0
#endif

#if defined (__HP_aCC)
set compiler_info [join {hpacc __HP_aCC} -]
set hp_aCC_compiler __HP_aCC
#else
set hp_aCC_compiler 0
#endif

/* gdb.base/whatis.exp still uses this */
#if defined (__STDC__) || defined (_AIX)
set signed_keyword_not_used 0
#else
set signed_keyword_not_used 1
#endif
