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

/*
 * Opcodes of the call instructions:
 */
#define OP_Jxx	0x1a
#define	OP_BSR	0x34

#define Jxx_FUNC_JMP		0
#define Jxx_FUNC_JSR		1
#define Jxx_FUNC_RET		2
#define Jxx_FUNC_JSR_COROUTINE	3

typedef union
  {
    struct
      {
	unsigned other:26;
	unsigned op_code:6;
      }
    a;				/* any format */
    struct
      {
	signed disp:21;
	unsigned ra:5;
	unsigned op_code:6;
      }
    b;				/* branch format */
    struct
      {
	signed hint:14;
	unsigned func:2;
	unsigned rb:5;
	unsigned ra:5;
	unsigned op_code:6;
      }
    j;				/* jump format */
  }
Instruction;

static Sym indirect_child;


/*
 * On the Alpha we can only detect PC relative calls, which are
 * usually generated for calls to functions within the same
 * object file only.  This is still better than nothing, however.
 * (In particular it should be possible to find functions that
 *  potentially call integer division routines, for example.)
 */
void
find_call (parent, p_lowpc, p_highpc)
     Sym *parent;
     bfd_vma p_lowpc;
     bfd_vma p_highpc;
{
  static bfd_vma delta = 0;
  bfd_vma dest_pc;
  Instruction *pc;
  Sym *child;

  if (!delta)
    {
      delta = (bfd_vma) core_text_space - core_text_sect->vma;

      sym_init (&indirect_child);
      indirect_child.name = "<indirect child>";
      indirect_child.cg.prop.fract = 1.0;
      indirect_child.cg.cyc.head = &indirect_child;
    }

  if (!core_text_space)
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
  DBG (CALLDEBUG, printf ("[find_call] %s: 0x%lx to 0x%lx\n",
			  parent->name, p_lowpc, p_highpc));
  for (pc = (Instruction *) (p_lowpc + delta);
       pc < (Instruction *) (p_highpc + delta);
       ++pc)
    {
      switch (pc->a.op_code)
	{
	case OP_Jxx:
	  /*
	   * There is no simple and reliable way to determine the
	   * target of a jsr (the hint bits help, but there aren't
	   * enough bits to get a satisfactory hit rate).  Instead,
	   * for any indirect jump we simply add an arc from PARENT
	   * to INDIRECT_CHILD---that way the user it at least able
	   * to see that there are other calls as well.
	   */
	  if (pc->j.func == Jxx_FUNC_JSR
	      || pc->j.func == Jxx_FUNC_JSR_COROUTINE)
	    {
	      DBG (CALLDEBUG,
		   printf ("[find_call] 0x%lx: jsr%s <indirect_child>\n",
			   (bfd_vma) pc - delta,
			   pc->j.func == Jxx_FUNC_JSR ? "" : "_coroutine"));
	      arc_add (parent, &indirect_child, 0);
	    }
	  break;

	case OP_BSR:
	  DBG (CALLDEBUG,
	       printf ("[find_call] 0x%lx: bsr", (bfd_vma) pc - delta));
	  /*
	   * Regular PC relative addressing.  Check that this is the
	   * address of a function.  The linker sometimes redirects
	   * the entry point by 8 bytes to skip loading the global
	   * pointer, so we all for either address:
	   */
	  dest_pc = ((bfd_vma) (pc + 1 + pc->b.disp)) - delta;
	  if (dest_pc >= s_lowpc && dest_pc <= s_highpc)
	    {
	      child = sym_lookup (&symtab, dest_pc);
	      DBG (CALLDEBUG,
		   printf (" 0x%lx\t; name=%s, addr=0x%lx",
			   dest_pc, child->name, child->addr));
	      if (child->addr == dest_pc || child->addr == dest_pc - 8)
		{
		  DBG (CALLDEBUG, printf ("\n"));
		  /* a hit:  */
		  arc_add (parent, child, 0);
		  continue;
		}
	    }
	  /*
	   * Something funny going on.
	   */
	  DBG (CALLDEBUG, printf ("\tbut it's a botch\n"));
	  break;

	default:
	  break;
	}
    }
}
