/* Instruction printing code for the Alpha
   Copyright (C) 1993, 1995 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

Written by Steve Chamberlain (sac@cygnus.com)

This file is part of libopcodes.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

You should have received a copy of the GNU General Public License along with
This program; if not, write to the Free Software Foundation, Inc., 675
 Mass Ave, Boston, MA 02111-1307, USA.
*/

#include "dis-asm.h"
#define DEFINE_TABLE
#include "alpha-opc.h"


/* Print one instruction from PC on INFO->STREAM.
   Return the size of the instruction (always 4 on alpha). */

int
print_insn_alpha(pc, info)
     bfd_vma pc;
     struct disassemble_info *info;
{
  alpha_insn	*insn;
  unsigned char	b[4];
  void		*stream = info->stream;
  fprintf_ftype	func = info->fprintf_func;
  unsigned long	given;
  int		status ;
  int found = 0;

  status = (*info->read_memory_func) (pc, (bfd_byte *) &b[0], 4, info);
  if (status != 0) {
    (*info->memory_error_func) (status, pc, info);
    return -1;
  }
  given = (b[0]) | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);

  func (stream, "\t%08x\t", given);

  for (insn = alpha_insn_set;
       insn->name && !found;
       insn++)
    {
      switch (insn->type)
	{
	case MEMORY_FORMAT_CODE:
	  if ((insn->i & MEMORY_FORMAT_MASK)
	      ==(given & MEMORY_FORMAT_MASK))
	    {
	      func (stream, "%s\t%s, %d(%s)",
		    insn->name,
		    alpha_regs[RA(given)],
		    OPCODE (given) == 9 ? DISP(given) * 65536 : DISP(given),
		    alpha_regs[RB(given)]);
	      found = 1;
	    }
	  break;


	case MEMORY_FUNCTION_FORMAT_CODE:
	  if ((insn->i & MEMORY_FUNCTION_FORMAT_MASK)
	      ==(given & MEMORY_FUNCTION_FORMAT_MASK))
	    {
	      switch (given & 0xffff)
		{
		case 0x8000: /* fetch */
		case 0xa000: /* fetch_m */
		  func (stream, "%s\t(%s)", insn->name, alpha_regs[RB(given)]);
		  break;

		case 0xc000: /* rpcc */
		  func (stream, "%s\t%s", insn->name, alpha_regs[RA(given)]);
		  break;

		default:
		  func (stream, "%s", insn->name);
		  break;
		}
	      found = 1;
	    }
	  break;

	case BRANCH_FORMAT_CODE:
	  if ((insn->i & BRANCH_FORMAT_MASK)
	      == (given & BRANCH_FORMAT_MASK))
	    {
	      if (RA(given) == 31)
		func (stream, "%s\t ", insn->name);
	      else
		func (stream, "%s\t%s, ", insn->name,
		      alpha_regs[RA(given)]);
	      (*info->print_address_func) (BDISP(given) * 4 + pc + 4, info);
	      found = 1;
	    }
	  break;

	case MEMORY_BRANCH_FORMAT_CODE:
	  if ((insn->i & MEMORY_BRANCH_FORMAT_MASK)
	      == (given & MEMORY_BRANCH_FORMAT_MASK))
	    {
	      if (given & (1<<15))
		{
		  func (stream, "%s\t%s, (%s), %d", insn->name,
			alpha_regs[RA(given)],
			alpha_regs[RB(given)],
			JUMP_HINT(given));
		}
	      else
		{
		  /* The displacement is a hint only, do not put out
		     a symbolic address.  */
		  func (stream, "%s\t%s, (%s), 0x%lx", insn->name,
			alpha_regs[RA(given)],
			alpha_regs[RB(given)],
			JDISP(given) * 4 + pc + 4);
		}
	      found = 1;
	    }
	  break;

	case OPERATE_FORMAT_CODE:
	  if ((insn->i & OPERATE_FORMAT_MASK)
	      == (given & OPERATE_FORMAT_MASK))
	    {
	      int optype = OP_OPTYPE(given);
	      if (OP_OPTYPE(insn->i) == optype)
		{
		  int ra;
		  ra = RA(given);

		  if (OP_IS_CONSTANT(given))
		    {
		      if ((optype == 0x20)	/* bis R31, lit, Ry */
			  && (ra == 31))
			{
			  func (stream, "mov\t0x%x, %s",
				LITERAL(given), alpha_regs[RC(given)] );
			}
		      else
			{
#if GNU_ASMCODE
			  func (stream, "%s\t%s, 0x%x, %s", insn->name,
				alpha_regs[RA(given)],
				LITERAL(given),
				alpha_regs[RC(given)]);
#else
			  func (stream, "%s\t%s, #%d, %s", insn->name,
				alpha_regs[RA(given)],
				LITERAL(given),
				alpha_regs[RC(given)]);
			}
#endif
		  } else {		/* not constant */
		    int rb, rc;
		    rb = RB(given); rc = RC(given);
		    switch(optype)
		      {
		      case 0x09:			/* subl */
			if (ra == 31)
			  {
			    func (stream, "negl\t%s, %s",
				  alpha_regs[rb], alpha_regs[rc]);
			    found = 1;
			  }
			break;
		      case 0x29:			/* subq */
			if (ra == 31)
			  {
			    func (stream, "negq\t%s, %s",
				  alpha_regs[rb], alpha_regs[rc]);
			    found = 1;
			  }
			break;
		      case 0x20:			/* bis */
			if (ra == 31)
			  {
			    if (ra == rb)		/* ra=R31, rb=R31 */
			      {
				if (rc == 31)
				  func (stream, "nop");
				else
				  func (stream, "clr\t%s", alpha_regs[rc]);
			      }
			    else
			      func (stream, "mov\t%s, %s",
				    alpha_regs[rb], alpha_regs[rc]);
			  }
			else
			  func (stream, "or\t%s, %s, %s",
				alpha_regs[ra], alpha_regs[rb], alpha_regs[rc]);
			found = 1;
			break;

		      default:
			break;

		      }

		    if (!found)
		      func (stream, "%s\t%s, %s, %s", insn->name,
			    alpha_regs[ra], alpha_regs[rb], alpha_regs[rc]);
		  }
		  found = 1;
		}
	    }

	  break;

	case FLOAT_FORMAT_CODE:
	  if ((insn->i & OPERATE_FORMAT_MASK)
	      == (given & OPERATE_FORMAT_MASK))
	    {
	      int ra, rb, rc;
	      ra = RA(given); rb = RB(given); rc = RC(given);
	      switch (OP_OPTYPE(given))
		{
		case 0x20:		/* cpys */
		  if (ra == 31)
		    {
		      if (ra == rb)
			{
			  if (rc == 31)
			    func (stream, "fnop");
			  else
			    func (stream, "fclr\tf%d", rc);
			}
		      else
			func (stream, "fmov\tf%d, f%d", rb, rc);
		      found = 1;
		    }
		  else
		    {
		      if (ra == 31) {
			func (stream, "fabs\tf%d, f%d", rb, rc);
			found = 1;
		      }
		    }
		  break;
		case 0x21:		/* cpysn */
		  if (ra == rb)
		    {
		      func (stream, "fneg\tf%d, f%d", rb, rc);
		      found = 1;
		    }
		default:
		  ;
	        }

	      if (!found)
	        func (stream, "%s\tf%d, f%d, f%d", insn->name, ra, rb, rc);

	      found = 1;
	    }

	  break;
	case PAL_FORMAT_CODE:
	  if (insn->i == given)
	    {
	      func (stream, "call_pal %s", insn->name);
	      found = 1;
	    }

	  break;
	case FLOAT_MEMORY_FORMAT_CODE:
	  if ((insn->i & MEMORY_FORMAT_MASK)
	      ==(given & MEMORY_FORMAT_MASK))
	    {
	      func (stream, "%s\tf%d, %d(%s)",
		      insn->name,
		      RA(given),
		      OPCODE (given) == 9 ? DISP(given) * 65536 : DISP(given),
		      alpha_regs[RB(given)]);
	      found = 1;
	    }
	  break;
	case FLOAT_BRANCH_FORMAT_CODE:
	  if ((insn->i & BRANCH_FORMAT_MASK)
	      == (given & BRANCH_FORMAT_MASK))
	    {
	      func (stream, "%s\tf%d, ",
		    insn->name,
		    RA(given));
	      (*info->print_address_func) (BDISP(given) * 4 + pc + 4, info);
	      found = 1;
	    }
	  break;
	}
    }

  if (!found)
    switch (OPCODE (given))
      {
      case 0x00:
	func (stream, "call_pal 0x%x", given);
	break;
      case 0x19:
      case 0x1b:
      case 0x1d:
      case 0x1e:
      case 0x1f:
	func (stream, "PAL%X 0x%x", OPCODE (given), given & 0x3ffffff);
	break;
      case 0x01:
      case 0x02:
      case 0x03:
      case 0x04:
      case 0x05:
      case 0x06:
      case 0x07:
      case 0x0a:
      case 0x0c:
      case 0x0d:
      case 0x0e:
      case 0x14:
      case 0x1c:
	func (stream, "OPC%02X 0x%x", OPCODE (given), given & 0x3ffffff);
	break;
      }

  return 4;
}
