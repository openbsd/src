static int filelocal = 2;	/* In Data section */
static int filelocal_bss;	/* In BSS section */
#ifndef __STDC__
#define	const	/**/
#endif
static const int filelocal_ro = 202;	/* In Read-Only Data section */

foo ()
{
  static int funclocal = 3;	/* In Data section */
  static int funclocal_bss;	/* In BSS section */
  static const int funclocal_ro = 203;	/* RO Data */
  static const int funclocal_ro_bss;	/* RO Data */

  funclocal_bss = 103;
  bar ();
}

bar ()
{
  static int funclocal = 4;	/* In data section */
  static int funclocal_bss;	/* In BSS section */
  funclocal_bss = 104;
}

init1 ()
{
  filelocal_bss = 102;
}

/* On some systems, such as AIX, unreferenced variables are deleted
   from the executable.  */
usestatics1 ()
{
  useit1 (filelocal);
  useit1 (filelocal_bss);
  useit1 (filelocal_ro);
}

useit1 (val)
{
    static int usedval;

    usedval = val;
}
