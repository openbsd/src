/* This is part of the shared library ld test.  This file becomes part
   of a shared library.  */

/* This variable is defined here, and referenced by another file in
   the shared library.  */
int shlibvar2 = 4;

/* This function is called by another file in the shared library.  */

int
shlib_shlibcalled ()
{
  return 5;
}

#ifdef DSO_DEFINE_TEST
int
visibility ()
{
  return 2;
}

int visibility_var = 2;
#endif
