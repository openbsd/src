/* Often the behavior of any particular test depends upon what compiler was
   used to compile the test.  As each test is compiled, this file is
   preprocessed by the same compiler used to compile that specific test
   (different tests might be compiled by different compilers, particularly
   if compiled at different times), and used to generate a *.ci (compiler
   info) file for that test.

   I.E., when callfuncs is compiled, a callfuncs.ci file will be generated,
   which can then be sourced by callfuncs.exp to give callfuncs.exp access
   to information about the compilation environment.

   TODO:  It might be a good idea to add expect code that tests each
   definition made with 'set" to see if one already exists, and if so
   warn about conflicts if it is being set to something else.  */

/* This needs to be kept in sync with whatis.c.  If this proves to end up
   being hairy, we could use a common header file.  */

#if defined (__STDC__) || defined (_AIX)
set signed_keyword_not_used 0
#else
set signed_keyword_not_used 1
#endif

#if defined (__GNUC__)
set gcc_compiled __GNUC__
#else
set gcc_compiled 0
#endif

return 0
