/* Test taken from GCC.  Verify that we can print a structure containing
   a complex number.  */

typedef __complex__ float cf;
struct x { char c; cf f; } __attribute__ ((__packed__));
struct unpacked_x { char c; cf f; };
extern void f4 (struct unpacked_x*);
extern void f3 (void);
extern void f2 (struct x*);
extern void f1 (void);
int
main (void)
{
  f1 ();
  f3 ();
  exit (0);
}

void
f1 (void)
{
  struct x s;
  s.f = 1;
  s.c = 42;
  f2 (&s);
}

void
f2 (struct x *y)
{
  if (y->f != 1 || y->c != 42)
    abort ();
}

void
f3 (void)
{
  struct unpacked_x s;
  s.f = 1;
  s.c = 42;
  f4 (&s);
}

void
f4 (struct unpacked_x *y)
{
  if (y->f != 1 || y->c != 42)
    abort ();
}
