/*  Test passing of arguments to functions.  Use various sorts of arguments,
    including basic types, pointers to those types, structures, lots of
    args, etc, in various combinations. */

/* AIX requires this to be the first thing in the file.  */
#ifdef __GNUC__
#  define alloca __builtin_alloca
#  define HAVE_STACK_ALLOCA 1
#else /* not __GNUC__ */
#  ifdef _AIX
     #pragma alloca
#    define HAVE_STACK_ALLOCA 1
#  else /* Not AIX */
#    ifdef sparc
#      include <alloca.h>
#      define HAVE_STACK_ALLOCA 1
#      ifdef __STDC__
         void *alloca ();
#      else
         char *alloca ();
#      endif /* __STDC__ */
#    endif /* sparc */
#  endif /* Not AIX */
#endif /* not __GNUC__ */

char c = 'a';
char *cp = &c;

unsigned char uc = 'b';
unsigned char *ucp = &uc;

short s = 1;
short *sp = &s;

unsigned short us = 6;
unsigned short *usp = &us;

int i = 2;
int *ip = &i;

unsigned int ui = 7;
unsigned int *uip = &ui;

long l = 3;
long *lp = &l;

unsigned long ul = 8;
unsigned long *ulp = &ul;

float f = 4.0;
float *fp = &f;

double d = 5.0;
double *dp = &d;

struct stag {
    int s1;
    int s2;
} st = { 101, 102 };
struct stag *stp = &st;

union utag {
    int u1;
    long u2;
} un;
union utag *unp = &un;

char carray[] = {'a', 'n', ' ', 'a', 'r', 'r', 'a', 'y', '\0'};


/* Test various permutations and interleaving of integral arguments */


call0a (c, s, i, l)
char c; short s; int i; long l;
{
  c = 'a';
  s = 5;
  i = 6;
  l = 7;
}

call0b (s, i, l, c)
short s; int i; long l; char c;
{
  s = 6; i = 7; l = 8; c = 'j';
}

call0c (i, l, c, s)
int i; long l; char c; short s;
{
  i = 3; l = 4; c = 'k'; s = 5;
}

call0d (l, c, s, i)
long l; char c; short s; int i;
{
  l = 7; c = 'z'; s = 8; i = 9;
}

call0e (c1, l, c2, i, c3, s, c4, c5)
char c1; long l; char c2; int i; char c3; short s; char c4; char c5;
{
  c1 = 'a'; l = 5; c2 = 'b'; i = 7; c3 = 'c'; s = 7; c4 = 'f'; c5 = 'g';
}


/* Test various permutations and interleaving of unsigned integral arguments */


call1a (uc, us, ui, ul)
unsigned char uc; unsigned short us; unsigned int ui; unsigned long ul;
{
  uc = 5; us = 6; ui = 7; ul = 8;
}

call1b (us, ui, ul, uc)
unsigned short us; unsigned int ui; unsigned long ul; unsigned char uc;
{
  uc = 5; us = 6; ui = 7; ul = 8;
}

call1c (ui, ul, uc, us)
unsigned int ui; unsigned long ul; unsigned char uc; unsigned short us;
{
  uc = 5; us = 6; ui = 7; ul = 8;
}

call1d (ul, uc, us, ui)
unsigned long ul; unsigned char uc; unsigned short us; unsigned int ui;
{
  uc = 5; us = 6; ui = 7; ul = 8;
}

call1e (uc1, ul, uc2, ui, uc3, us, uc4, uc5)
unsigned char uc1; unsigned long ul; unsigned char uc2; unsigned int ui;
unsigned char uc3; unsigned short us; unsigned char uc4; unsigned char uc5;
{
  uc1 = 5; ul = 7; uc2 = 8; ui = 9; uc3 = 10; us = 11; uc4 = 12; uc5 = 55;
}

/* Test various permutations and interleaving of integral arguments with
   floating point arguments. */


call2a (c, f1, s, d1, i, f2, l, d2)
char c; float f1; short s; double d1; int i; float f2; long l; double d2;
{
  c = 'a'; f1 = 0.0; s = 5; d1 = 0.0; i = 6; f2 = 0.1; l = 7; d2 = 0.2;
}

call2b (f1, s, d1, i, f2, l, d2, c)
float f1; short s; double d1; int i; float f2; long l; double d2; char c;
{
  c = 'a'; f1 = 0.0; s = 5; d1 = 0.0; i = 6; f2 = 0.1; l = 7; d2 = 0.2;
}

call2c (s, d1, i, f2, l, d2, c, f1)
short s; double d1; int i; float f2; long l; double d2; char c; float f1;
{
  c = 'a'; f1 = 0.0; s = 5; d1 = 0.0; i = 6; f2 = 0.1; l = 7; d2 = 0.2;
}

call2d (d1, i, f2, l, d2, c, f1, s)
double d1; int i; float f2; long l; double d2; char c; float f1; short s;
{
  c = 'a'; f1 = 0.0; s = 5; d1 = 0.0; i = 6; f2 = 0.1; l = 7; d2 = 0.2;
}

call2e (i, f2, l, d2, c, f1, s, d1)
int i; float f2; long l; double d2; char c; float f1; short s; double d1;
{
  c = 'a'; f1 = 0.0; s = 5; d1 = 0.0; i = 6; f2 = 0.1; l = 7; d2 = 0.2;
}

call2f (f2, l, d2, c, f1, s, d1, i)
float f2; long l; double d2; char c; float f1; short s; double d1; int i;
{
  c = 'a'; f1 = 0.0; s = 5; d1 = 0.0; i = 6; f2 = 0.1; l = 7; d2 = 0.2;
}

call2g (l, d2, c, f1, s, d1, i, f2)
long l; double d2; char c; float f1; short s; double d1; int i; float f2;
{
  c = 'a'; f1 = 0.0; s = 5; d1 = 0.0; i = 6; f2 = 0.1; l = 7; d2 = 0.2;
}

call2h (d2, c, f1, s, d1, i, f2, l)
double d2; char c; float f1; short s; double d1; int i; float f2; long l;
{
  c = 'a'; f1 = 0.0; s = 5; d1 = 0.0; i = 6; f2 = 0.1; l = 7; d2 = 0.2;
}

call2i (c1, f1, c2, c3, d1, c4, c5, c6, f2, s, c7, d2)
char c1; float f1; char c2; char c3; double d1; char c4; char c5; char c6;
float f2; short s; char c7; double d2;
{
  c1 = 'a'; f1 = 0.0; c2 = 5; d1 = 0.0; c3 = 6; f2 = 0.1; c4 = 7; d2 = 0.2;
  c5 = 's'; c6 = 'f'; c7 = 'z'; s = 77;
}


/* Test pointers to various integral and floating types. */


call3a (cp, sp, ip, lp)
char *cp; short *sp; int *ip; long *lp;
{
  cp = 0; sp = 0; ip = 0; lp = 0;
}

call3b (ucp, usp, uip, ulp)
unsigned char *ucp; unsigned short *usp; unsigned int *uip;
unsigned long *ulp;
{
  ucp = 0; usp = 0; uip = 0; ulp = 0;
}

call3c (fp, dp)
float *fp; double *dp;
{
  fp = 0; dp = 0;
}


/* Test passing structures and unions by reference. */


call4a (stp)
struct stag *stp; {
stp = 0;}

call4b (unp)
union utag *unp;
{
  unp = 0;
}


/* Test passing structures and unions by value. */


call5a (st)
struct stag st; {st.s1 = 5;}

call5b (un)
union utag un; {un.u1 = 7;}


/* Test shuffling of args */


call6a (c, s, i, l, f, d, uc, us, ui, ul)
char c; short s; int i; long l; float f; double d;
unsigned char uc; unsigned short us; unsigned int ui; unsigned long ul;
{
  c = c;
    call6b (s, i, l, f, d, uc, us, ui, ul);
}

call6b (s, i, l, f, d, uc, us, ui, ul)
short s; int i; long l; float f; double d;
unsigned char uc; unsigned short us; unsigned int ui; unsigned long ul;
{
  s = s;
    call6c (i, l, f, d, uc, us, ui, ul);
}

call6c (i, l, f, d, uc, us, ui, ul)
int i; long l; float f; double d;
unsigned char uc; unsigned short us; unsigned int ui; unsigned long ul;
{
  i = i;
    call6d (l, f, d, uc, us, ui, ul);
}

call6d (l, f, d, uc, us, ui, ul)
long l; float f; double d;
unsigned char uc; unsigned short us; unsigned int ui; unsigned long ul;
{
  l = l;
    call6e (f, d, uc, us, ui, ul);
}

call6e (f, d, uc, us, ui, ul)
float f; double d;
unsigned char uc; unsigned short us; unsigned int ui; unsigned long ul;
{
  f = f;
    call6f (d, uc, us, ui, ul);
}

call6f (d, uc, us, ui, ul)
double d;
unsigned char uc; unsigned short us; unsigned int ui; unsigned long ul;
{
  d = d;
    call6g (uc, us, ui, ul);
}

call6g (uc, us, ui, ul)
unsigned char uc; unsigned short us; unsigned int ui; unsigned long ul;
{
  uc = uc;
    call6h (us, ui, ul);
}

call6h (us, ui, ul)
unsigned short us; unsigned int ui; unsigned long ul;
{
  us = us;
    call6i (ui, ul);
}

call6i (ui, ul)
unsigned int ui; unsigned long ul;
{
  ui = ui;
    call6j (ul);
}

call6j (ul)
unsigned long ul;
{
  ul = ul;
    call6k ();
}

call6k ()
{
}


/*  Test shuffling of args, round robin */


call7a (c, i, s, l, f, uc, d, us, ul, ui)
char c; int i; short s; long l; float f; unsigned char uc; double d; unsigned short us; unsigned long ul; unsigned int ui;
{
    call7b (i, s, l, f, uc, d, us, ul, ui, c);
}

call7b (i, s, l, f, uc, d, us, ul, ui, c)
int i; short s; long l; float f; unsigned char uc; double d; unsigned short us; unsigned long ul; unsigned int ui; char c;
{
    call7c (s, l, f, uc, d, us, ul, ui, c, i);
}

call7c (s, l, f, uc, d, us, ul, ui, c, i)
short s; long l; float f; unsigned char uc; double d; unsigned short us; unsigned long ul; unsigned int ui; char c; int i;
{
    call7d (l, f, uc, d, us, ul, ui, c, i, s);
}

call7d (l, f, uc, d, us, ul, ui, c, i, s)
long l; float f; unsigned char uc; double d; unsigned short us; unsigned long ul; unsigned int ui; char c; int i; short s;
{
    call7e (f, uc, d, us, ul, ui, c, i, s, l);
}

call7e (f, uc, d, us, ul, ui, c, i, s, l)
float f; unsigned char uc; double d; unsigned short us; unsigned long ul; unsigned int ui; char c; int i; short s; long l;
{
    call7f (uc, d, us, ul, ui, c, i, s, l, f);
}

call7f (uc, d, us, ul, ui, c, i, s, l, f)
unsigned char uc; double d; unsigned short us; unsigned long ul; unsigned int ui; char c; int i; short s; long l; float f;
{
    call7g (d, us, ul, ui, c, i, s, l, f, uc);
}

call7g (d, us, ul, ui, c, i, s, l, f, uc)
double d; unsigned short us; unsigned long ul; unsigned int ui; char c; int i; short s; long l; float f; unsigned char uc;
{
    call7h (us, ul, ui, c, i, s, l, f, uc, d);
}

call7h (us, ul, ui, c, i, s, l, f, uc, d)
unsigned short us; unsigned long ul; unsigned int ui; char c; int i; short s; long l; float f; unsigned char uc; double d;
{
    call7i (ul, ui, c, i, s, l, f, uc, d, us);
}

call7i (ul, ui, c, i, s, l, f, uc, d, us)
unsigned long ul; unsigned int ui; char c; int i; short s; long l; float f; unsigned char uc; double d; unsigned short us;
{
    call7j (ui, c, i, s, l, f, uc, d, us, ul);
}

call7j (ui, c, i, s, l, f, uc, d, us, ul)
unsigned int ui; char c; int i; short s; long l; float f; unsigned char uc; double d; unsigned short us; unsigned long ul;
{
    call7k (c, i, s, l, f, uc, d, us, ul, ui);
}

call7k (c, i, s, l, f, uc, d, us, ul, ui)
char c; int i; short s; long l; float f; unsigned char uc; double d; unsigned short us; unsigned long ul; unsigned int ui;
{
  c = 'a'; i = 7; s = 8; l = 7; f = 0.3; uc = 44; d = 0.44; us = 77;
  ul = 43; ui = 33;
}


/*  Test printing of structures passed as arguments to recursive functions. */


typedef struct s
{
  short s;
  int i;
  long l;
} SVAL;	

hitbottom ()
{
}

void recurse (a, depth)
SVAL a;
int depth;
{
  a.s = a.i = a.l = --depth;
  if (depth == 0)
    hitbottom ();
  else
    recurse (a, depth);
}

test_struct_args ()
{
  SVAL s; s.s = 5; s.i = 5; s.l = 5;

  recurse (s, 5);
}

/* On various machines (pa, 29k, and rs/6000, at least), a function which
   calls alloca may do things differently with respect to frames.  So give
   it a try.  */

int
localvars_after_alloca (c, s, i, l)
     char c;
     short s;
     int i;
     long l;
{
#ifdef HAVE_STACK_ALLOCA
  /* No need to use the alloca.c alloca-on-top-of-malloc; it doesn't
     test what we are looking for, so if we don't have an alloca which
     allocates on the stack, just don't bother to call alloca at all.  */

  char *z = alloca (s + 50);
#endif
  c = 'a';
  s = 5;
  i = 6;
  l = 7;
}

void
call_after_alloca_subr (c, s, i, l, uc, us, ui, ul)
char c; int i; short s; long l; unsigned char uc; unsigned short us; unsigned long ul; unsigned int ui;
{
  c = 'a';
  i = 7; s = 8; l = 7; uc = 44; us = 77;
  ul = 43; ui = 33;
}

int
call_after_alloca (c, s, i, l)
     char c;
     short s;
     int i;
     long l;
{
#ifdef HAVE_STACK_ALLOCA
  /* No need to use the alloca.c alloca-on-top-of-malloc; it doesn't
     test what we are looking for, so if we don't have an alloca which
     allocates on the stack, just don't bother to call alloca at all.  */

  char *z = alloca (s + 50);
#endif
  call_after_alloca_subr (c, s, i, l, 'b', 11, 12, (unsigned long)13);
}



/* The point behind this test is the PA will call this indirectly
   through dyncall.  Unlike the indirect calls to call0a, this test
   will require a trampoline between dyncall and this function on the
   call path, then another trampoline on between this function and main
   on the return path.  */
double
call_with_trampolines (d1)
double d1;
{
  return d1;
} /* End of call_with_trampolines, this comment is needed by funcargs.exp */

/* Dummy functions which the testsuite can use to run to, etc.  */

void
marker_indirect_call () {}

void
marker_call_with_trampolines () {}

main ()
{
  int (*pointer_to_call0a) () = call0a;
  double (*pointer_to_call_with_trampolines) () = call_with_trampolines;

#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  /* Test calling with basic integer types */
  call0a (c, s, i, l);
  call0b (s, i, l, c);
  call0c (i, l, c, s);
  call0d (l, c, s, i);
  call0e (c, l, c, i, c, s, c, c);

  /* Test calling with unsigned integer types */
  call1a (uc, us, ui, ul);
  call1b (us, ui, ul, uc);
  call1c (ui, ul, uc, us);
  call1d (ul, uc, us, ui);
  call1e (uc, ul, uc, ui, uc, us, uc, uc);

  /* Test calling with integral types mixed with floating point types */
  call2a (c, f, s, d, i, f, l, d);
  call2b (f, s, d, i, f, l, d, c);
  call2c (s, d, i, f, l, d, c, f);
  call2d (d, i, f, l, d, c, f, s);
  call2e (i, f, l, d, c, f, s, d);
  call2f (f, l, d, c, f, s, d, i);
  call2g (l, d, c, f, s, d, i, f);
  call2h (d, c, f, s, d, i, f, l);
  call2i (c, f, c, c, d, c, c, c, f, s, c, d);;

  /* Test dereferencing pointers to various integral and floating types */

  call3a (cp, sp, ip, lp);
  call3b (ucp, usp, uip, ulp);
  call3c (fp, dp);

  /* Test dereferencing pointers to structs and unions */

  call4a (stp);
  un.u1 = 1;
  call4b (unp);

  /* Test calling with structures and unions. */

  call5a (st);
  un.u1 = 2;
  call5b (un);

  /* Test shuffling of args */

  call6a (c, s, i, l, f, d, uc, us, ui, ul);
  call7a (c, i, s, l, f, uc, d, us, ul, ui);
  
  /* Test passing structures recursively. */

  test_struct_args ();

  localvars_after_alloca (c, s, i, l);

  call_after_alloca (c, s, i, l);

  /* This is for localvars_in_indirect_call.  */
  marker_indirect_call ();
  /* The comment on the following two lines is used by funcargs.exp,
     don't change it.  */
  (*pointer_to_call0a) (c, s, i, l);	/* First step into call0a.  */
  (*pointer_to_call0a) (c, s, i, l);	/* Second step into call0a.  */
  marker_call_with_trampolines ();
  (*pointer_to_call_with_trampolines) (d); /* Test multiple trampolines.  */
}
