#include "gprof.h"
#include "symtab.h"


/*
 * dummy.c -- This file should be used for an unsupported processor type.
 * It does nothing, but prevents findcall() from being unresolved.
 */

void
DEFUN (find_call, (parent, p_lowpc, p_highpc),
       Sym * parent AND bfd_vma p_lowpc AND bfd_vma p_highpc)
{
  fprintf (stderr, "%s: -c not supported on this machine architecture\n",
	   whoami);
}
