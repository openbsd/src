/* This is intended to be a vague simulation of cfront output.  */
#line 1 "langs2.cxx"
extern int csub ();
int
foo__Fi (x) int x;
{
  return csub (x / 2);
}

extern int cppsub_ ();
int
cppsub_ (y) int *y;
{
  return foo__Fi (*y);
}
