/* Instruction printing code for the ARM
   Copyright (C) 1994 Free Software Foundation, Inc. 
   Contributed by Richard Earnshaw (rwe@pegasus.esprit.ec.org)

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
#include "arm-opc.h"


static char *arm_conditional[] =
{"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
 "hi", "ls", "ge", "lt", "gt", "le", "", "nv"};

static char *arm_regnames[] =
{"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
 "r8", "r9", "sl", "fp", "ip", "sp", "lr", "pc"};

static char *arm_fp_const[] =
{"0.0", "1.0", "2.0", "3.0", "4.0", "5.0", "0.5", "10.0"};

static char *arm_shift[] = 
{"lsl", "lsr", "asr", "ror"};

static void
arm_decode_shift (given, func, stream)
     long given;
     fprintf_ftype func;
     void *stream;
{
  func (stream, "%s", arm_regnames[given & 0xf]);
  if ((given & 0xff0) != 0)
    {
      if ((given & 0x10) == 0)
	{
	  int amount = (given & 0xf80) >> 7;
	  int shift = (given & 0x60) >> 5;
	  if (amount == 0)
	    {
	      if (shift == 3)
		{
		  func (stream, ", rrx");
		  return;
		}
	      amount = 32;
	    }
	  func (stream, ", %s #%x", arm_shift[shift], amount);
	}
      else
	func (stream, ", %s %s", arm_shift[(given & 0x60) >> 5],
	      arm_regnames[(given & 0xf00) >> 8]);
    }
}

/* Print one instruction from PC on INFO->STREAM.
   Return the size of the instruction (always 4 on ARM). */

static int
print_insn_arm (pc, info, given)
     bfd_vma         pc;
     struct disassemble_info *info;
     long given;
{
  struct arm_opcode *insn;
  void *stream = info->stream;
  fprintf_ftype func = info->fprintf_func;

  func (stream, "%08x\t", given);

  for (insn = arm_opcodes; insn->assembler; insn++)
    {
      if ((given & insn->mask) == insn->value)
	{
	  char *c;
	  for (c = insn->assembler; *c; c++)
	    {
	      if (*c == '%')
		{
		  switch (*++c)
		    {
		    case '%':
		      func (stream, "%%");
		      break;

		    case 'a':
		      if (((given & 0x000f0000) == 0x000f0000)
			  && ((given & 0x02000000) == 0))
			{
			  int offset = given & 0xfff;
			  if ((given & 0x00800000) == 0)
			    offset = -offset;
			  (*info->print_address_func)
			    (offset + pc + 8, info);
			}
		      else
			{
			  func (stream, "[%s", 
				arm_regnames[(given >> 16) & 0xf]);
			  if ((given & 0x01000000) != 0)
			    {
			      if ((given & 0x02000000) == 0)
				{
				  int offset = given & 0xfff;
				  if (offset)
				    func (stream, ", %s#%x",
					  (((given & 0x00800000) == 0)
					   ? "-" : ""), offset);
				}
			      else
				{
				  func (stream, ", %s",
					(((given & 0x00800000) == 0)
					 ? "-" : ""));
				  arm_decode_shift (given, func, stream);
				}

			      func (stream, "]%s", 
				    ((given & 0x00200000) != 0) ? "!" : "");
			    }
			  else
			    {
			      if ((given & 0x02000000) == 0)
				{
				  int offset = given & 0xfff;
				  if (offset)
				    func (stream, "], %s#%x",
					  (((given & 0x00800000) == 0)
					   ? "-" : ""), offset);
				  else 
				    func (stream, "]");
				}
			      else
				{
				  func (stream, "], %s",
					(((given & 0x00800000) == 0) 
					 ? "-" : ""));
				  arm_decode_shift (given, func, stream);
				}
			    }
			}
		      break;
			  
		    case 'b':
		      (*info->print_address_func)
			(BDISP (given) * 4 + pc + 8, info);
		      break;

		    case 'c':
		      func (stream, "%s",
			    arm_conditional [(given >> 28) & 0xf]);
		      break;

		    case 'm':
		      {
			int started = 0;
			int reg;

			func (stream, "{");
			for (reg = 0; reg < 16; reg++)
			  if ((given & (1 << reg)) != 0)
			    {
			      if (started)
				func (stream, ", ");
			      started = 1;
			      func (stream, "%s", arm_regnames[reg]);
			    }
			func (stream, "}");
		      }
		      break;

		    case 'o':
		      if ((given & 0x02000000) != 0)
			{
			  int rotate = (given & 0xf00) >> 7;
			  int immed = (given & 0xff);
			  func (stream, "#%x",
				((immed << (32 - rotate))
				 | (immed >> rotate)) & 0xffffffff);
			}
		      else
			arm_decode_shift (given, func, stream);
		      break;

		    case 'p':
		      if ((given & 0x0000f000) == 0x0000f000)
			func (stream, "p");
		      break;

		    case 't':
		      if ((given & 0x01200000) == 0x00200000)
			func (stream, "t");
		      break;

		    case 'A':
		      func (stream, "[%s", arm_regnames [(given >> 16) & 0xf]);
		      if ((given & 0x01000000) != 0)
			{
			  int offset = given & 0xff;
			  if (offset)
			    func (stream, ", %s#%x]%s",
				  ((given & 0x00800000) == 0 ? "-" : ""),
				  offset * 4,
				  ((given & 0x00200000) != 0 ? "!" : ""));
			  else
			    func (stream, "]");
			}
		      else
			{
			  int offset = given & 0xff;
			  if (offset)
			    func (stream, "], %s#%x",
				  ((given & 0x00800000) == 0 ? "-" : ""),
				  offset * 4);
			  else
			    func (stream, "]");
			}
		      break;

		    case 'C':
		      switch (given & 0x00090000)
			{
			case 0:
			  func (stream, "_???");
			  break;
			case 0x10000:
			  func (stream, "_ctl");
			  break;
			case 0x80000:
			  func (stream, "_flg");
			  break;
			}
		      break;

		    case 'F':
		      switch (given & 0x00408000)
			{
			case 0:
			  func (stream, "4");
			  break;
			case 0x8000:
			  func (stream, "1");
			  break;
			case 0x00400000:
			  func (stream, "2");
			  break;
			default:
			  func (stream, "3");
			}
		      break;
			
		    case 'P':
		      switch (given & 0x00080080)
			{
			case 0:
			  func (stream, "s");
			  break;
			case 0x80:
			  func (stream, "d");
			  break;
			case 0x00080000:
			  func (stream, "e");
			  break;
			default:
			  func (stream, "<illegal precision>");
			  break;
			}
		      break;
		    case 'Q':
		      switch (given & 0x00408000)
			{
			case 0:
			  func (stream, "s");
			  break;
			case 0x8000:
			  func (stream, "d");
			  break;
			case 0x00400000:
			  func (stream, "e");
			  break;
			default:
			  func (stream, "p");
			  break;
			}
		      break;
		    case 'R':
		      switch (given & 0x60)
			{
			case 0:
			  break;
			case 0x20:
			  func (stream, "p");
			  break;
			case 0x40:
			  func (stream, "m");
			  break;
			default:
			  func (stream, "z");
			  break;
			}
		      break;

		    case '0': case '1': case '2': case '3': case '4': 
		    case '5': case '6': case '7': case '8': case '9':
		      {
			int bitstart = *c++ - '0';
			int bitend = 0;
			while (*c >= '0' && *c <= '9')
			  bitstart = (bitstart * 10) + *c++ - '0';

			switch (*c)
			  {
			  case '-':
			    c++;
			    while (*c >= '0' && *c <= '9')
			      bitend = (bitend * 10) + *c++ - '0';
			    if (!bitend)
			      abort ();
			    switch (*c)
			      {
			      case 'r':
				{
				  long reg;
				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;
				  func (stream, "%s", arm_regnames[reg]);
				}
				break;
			      case 'd':
				{
				  long reg;
				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;
				  func (stream, "%d", reg);
				}
				break;
			      case 'x':
				{
				  long reg;
				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;
				  func (stream, "0x%08x", reg);
				}
				break;
			      case 'f':
				{
				  long reg;
				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;
				  if (reg > 7)
				    func (stream, "#%s",
					  arm_fp_const[reg & 7]);
				  else
				    func (stream, "f%d", reg);
				}
				break;
			      default:
				abort ();
			      }
			    break;
			  case '`':
			    c++;
			    if ((given & (1 << bitstart)) == 0)
			      func (stream, "%c", *c);
			    break;
			  case '\'':
			    c++;
			    if ((given & (1 << bitstart)) != 0)
			      func (stream, "%c", *c);
			    break;
			  case '?':
			    ++c;
			    if ((given & (1 << bitstart)) != 0)
			      func (stream, "%c", *c++);
			    else
			      func (stream, "%c", *++c);
			    break;
			  default:
			    abort ();
			  }
			break;

		      default:
			abort ();
		      }
		    }
		}
	      else
		func (stream, "%c", *c);
	    }
	  return 4;
	}
    }
  abort ();
}

int
print_insn_big_arm (pc, info)
     bfd_vma pc;
     struct disassemble_info *info;
{
  unsigned char b[4];
  long given;
  int status;

  status = (*info->read_memory_func) (pc, (bfd_byte *) &b[0], 4, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, pc, info);
      return -1;
    }

  given = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | (b[3]);

  return print_insn_arm (pc, info, given);
}

int
print_insn_little_arm (pc, info)
     bfd_vma pc;
     struct disassemble_info *info;
{
  unsigned char b[4];
  long given;
  int status;

  status = (*info->read_memory_func) (pc, (bfd_byte *) &b[0], 4, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, pc, info);
      return -1;
    }

  given = (b[0]) | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);

  return print_insn_arm (pc, info, given);
}
