/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include "gprof.h"
#include "cg_arcs.h"
#include "corefile.h"
#include "hist.h"
#include "symtab.h"


int
DEFUN (i386_iscall, (ip), unsigned char *ip)
{
  if (*ip == 0xe8)
    return 1;
  return 0;
}


void
i386_find_call (parent, p_lowpc, p_highpc)
     Sym *parent;
     bfd_vma p_lowpc;
     bfd_vma p_highpc;
{
  unsigned char *instructp;
  Sym *child;
  bfd_vma destpc, delta;

  if (core_text_space == 0)
    {
      return;
    }
  if (p_lowpc < s_lowpc)
    {
      p_lowpc = s_lowpc;
    }
  if (p_highpc > s_highpc)
    {
      p_highpc = s_highpc;
    }
  DBG (CALLDEBUG, printf ("[findcall] %s: 0x%lx to 0x%lx\n",
			  parent->name, (unsigned long) p_lowpc,
			  (unsigned long) p_highpc));

  delta = (bfd_vma) core_text_space - core_text_sect->vma;

  for (instructp = (unsigned char *) (p_lowpc + delta);
       instructp < (unsigned char *) (p_highpc + delta);
       instructp ++)
    {
      if (i386_iscall (instructp))
	{
	  DBG (CALLDEBUG,
	       printf ("[findcall]\t0x%lx:call",
		       (unsigned long) (instructp - (unsigned char *) delta)));
	  /*
	   *  regular pc relative addressing
	   *    check that this is the address of 
	   *    a function.
	   */

	  destpc = ((bfd_vma) bfd_get_32 (core_bfd, instructp + 1)
		    + (bfd_vma) instructp - (bfd_vma) delta + 5);
	  if (destpc >= s_lowpc && destpc <= s_highpc)
	    {
	      child = sym_lookup (&symtab, destpc);
	      if (child && child->addr == destpc)
		{
		  /*
		   *      a hit
		   */
		  DBG (CALLDEBUG,
		       printf ("\tdestpc 0x%lx (%s)\n",
			       (unsigned long) destpc, child->name));
		  arc_add (parent, child, (unsigned long) 0);
		  instructp += 4;	/* call is a 5 byte instruction */
		  continue;
		}
	    }
	  /*
	   *  else:
	   *    it looked like a callf, but it:
	   *      a) wasn't actually a callf, or
	   *      b) didn't point to a known function in the symtab, or
	   *      c) something funny is going on.
	   */
	  DBG (CALLDEBUG, printf ("\tbut it's a botch\n"));
	}
    }
}
