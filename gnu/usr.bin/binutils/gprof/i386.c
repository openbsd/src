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
#include "core.h"
#include "hist.h"
#include "symtab.h"


int
DEFUN (iscall, (ip), unsigned char *ip)
{
  if (*ip == 0xeb || *ip == 0x9a)
    return 1;
  return 0;
}


void
find_call (parent, p_lowpc, p_highpc)
     Sym *parent;
     bfd_vma p_lowpc;
     bfd_vma p_highpc;
{
  unsigned char *instructp;
  long length;
  Sym *child;
  bfd_vma destpc;

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
			  parent->name, p_lowpc, p_highpc));
  for (instructp = (unsigned char *) core_text_space + p_lowpc;
       instructp < (unsigned char *) core_text_space + p_highpc;
       instructp += length)
    {
      length = 1;
      if (iscall (instructp))
	{
	  DBG (CALLDEBUG,
	       printf ("[findcall]\t0x%x:callf",
		       instructp - (unsigned char *) core_text_space));
	  length = 4;
	  /*
	   *  regular pc relative addressing
	   *    check that this is the address of 
	   *    a function.
	   */
	  destpc = ((bfd_vma) instructp + 5 - (bfd_vma) core_text_space);
	  if (destpc >= s_lowpc && destpc <= s_highpc)
	    {
	      child = sym_lookup (&symtab, destpc);
	      DBG (CALLDEBUG,
		   printf ("[findcall]\tdestpc 0x%lx", destpc);
		   printf (" child->name %s", child->name);
		   printf (" child->addr 0x%lx\n", child->addr);
		);
	      if (child->addr == destpc)
		{
		  /*
		   *      a hit
		   */
		  arc_add (parent, child, (long) 0);
		  length += 4;	/* constant lengths */
		  continue;
		}
	      goto botched;
	    }
	  /*
	   *  else:
	   *    it looked like a callf,
	   *    but it wasn't to anywhere.
	   */
	botched:
	  /*
	   *  something funny going on.
	   */
	  DBG (CALLDEBUG, printf ("[findcall]\tbut it's a botch\n"));
	  length = 1;
	  continue;
	}
    }
}
