#ifdef PROTECTED_CHECK
#include <features.h>
#include <stdio.h>

int
main (void)
{
#if defined (__GLIBC__) && (__GLIBC__ > 2 \
			    || (__GLIBC__ == 2 \
				&&  __GLIBC_MINOR__ >= 2))
  puts ("yes");
#else
  puts ("no");
#endif
  return 0;
}
#else
/* This is the main program for the shared library test.  */

#include <stdio.h>

int mainvar = 1;
int overriddenvar = 2;
extern int shlibvar1;

extern int shlib_mainvar ();
extern int shlib_overriddenvar ();
extern int shlib_shlibvar1 ();
extern int shlib_shlibvar2 ();
extern int shlib_shlibcall ();
extern int shlib_maincall ();
extern int shlib_checkfunptr1 ();
extern int shlib_checkfunptr2 ();
extern int (*shlib_getfunptr1 ()) ();
extern int (*shlib_getfunptr2 ()) ();
extern int shlib_check ();
extern int shlib_shlibcall2 ();
extern int visibility_check ();
extern int visibility_checkfunptr ();
extern void *visibility_funptr ();
extern int visibility_checkvar ();
extern int visibility_checkvarptr ();
extern int visibility_varval ();
extern void *visibility_varptr ();

#ifdef HIDDEN_WEAK_TEST
#define WEAK_TEST
#endif

#ifdef PROTECTED_WEAK_TEST
#define WEAK_TEST
#endif

#ifdef PROTECTED_UNDEF_TEST
#define PROTECTED_TEST
#endif

#ifndef WEAK_TEST
extern int visibility ();
extern int visibility_var;
#endif

#if !defined (HIDDEN_TEST) && defined (PROTECTED_TEST)
int
visibility ()
{
  return 1;
}

static int
main_visibility_check ()
{
  return visibility_funptr () != visibility;
}

int visibility_var = 1;

static int
main_visibility_checkvar ()
{
  return visibility_varval () != visibility_var
	 && visibility_varptr () != &visibility_var;
}
#else
static int
main_visibility_check ()
{
#ifdef WEAK_TEST
  return visibility_funptr () == NULL;
#else
  return visibility_funptr () == visibility;
#endif
}

static int
main_visibility_checkvar ()
{
#ifdef WEAK_TEST
  return visibility_varval () == 0
	 && visibility_varptr () == NULL;
#else
  return visibility_varval () == visibility_var
	 && visibility_varptr () == &visibility_var;
#endif
}
#endif

/* This function is called by the shared library.  */

int
main_called ()
{
  return 6;
}

/* This function overrides a function in the shared library.  */

int
shlib_overriddencall2 ()
{
  return 8;
}

int
main ()
{
  int (*p) ();

  printf ("mainvar == %d\n", mainvar);
  printf ("overriddenvar == %d\n", overriddenvar);
  printf ("shlibvar1 == %d\n", shlibvar1);
#ifndef XCOFF_TEST
  printf ("shlib_mainvar () == %d\n", shlib_mainvar ());
  printf ("shlib_overriddenvar () == %d\n", shlib_overriddenvar ());
#endif
  printf ("shlib_shlibvar1 () == %d\n", shlib_shlibvar1 ());
  printf ("shlib_shlibvar2 () == %d\n", shlib_shlibvar2 ());
  printf ("shlib_shlibcall () == %d\n", shlib_shlibcall ());
#ifndef XCOFF_TEST
  printf ("shlib_shlibcall2 () == %d\n", shlib_shlibcall2 ());
  printf ("shlib_maincall () == %d\n", shlib_maincall ());
#endif
  printf ("main_called () == %d\n", main_called ());
  printf ("shlib_checkfunptr1 (shlib_shlibvar1) == %d\n",
	  shlib_checkfunptr1 (shlib_shlibvar1));
#ifndef XCOFF_TEST
  printf ("shlib_checkfunptr2 (main_called) == %d\n",
	  shlib_checkfunptr2 (main_called));
#endif
  p = shlib_getfunptr1 ();
  printf ("shlib_getfunptr1 () ");
  if (p == shlib_shlibvar1)
    printf ("==");
  else
    printf ("!=");
  printf (" shlib_shlibvar1\n");
#ifndef XCOFF_TEST
  p = shlib_getfunptr2 ();
  printf ("shlib_getfunptr2 () ");
  if (p == main_called)
    printf ("==");
  else
    printf ("!=");
  printf (" main_called\n");
#endif
  printf ("shlib_check () == %d\n", shlib_check ());
  printf ("visibility_check () == %d\n", visibility_check ());
  printf ("visibility_checkfunptr () == %d\n",
	  visibility_checkfunptr ());
  printf ("main_visibility_check () == %d\n", main_visibility_check ());
  printf ("visibility_checkvar () == %d\n", visibility_checkvar ());
  printf ("visibility_checkvarptr () == %d\n",
	  visibility_checkvarptr ());
  printf ("main_visibility_checkvar () == %d\n",
	  main_visibility_checkvar ());
  return 0;
}
#endif
