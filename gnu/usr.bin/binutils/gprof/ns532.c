#include "gprof.h"
#include "symtab.h"

/*
 * dummy.c -- This file should be used for an unsupported processor type.
 * It does nothing, but prevents findcall() from being unresolved.
 */

void
find_call (parent, p_lowpc, p_highpc)
     Sym *parent;
     bfd_vma p_lowpc;
     bfd_vma p_highpc;
{
  fprintf (stderr, "%s: -c supported on this machine architecture\n",
	   whoami);
}
