/* Disassemble h8300 instructions.
   Copyright (C) 1993 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define DEFINE_TABLE

#define h8_opcodes h8ops
#include "opcode/h8300.h"
#include "dis-asm.h"


/* Run through the opcodes and sort them into order to make them easy
   to disassemble
 */
static void
bfd_h8_disassemble_init ()
{
  unsigned int i;


  struct h8_opcode *p;

  for (p = h8_opcodes; p->name; p++)
    {
      int n1 = 0;
      int n2 = 0;

      if ((int) p->data.nib[0] < 16)
	{
	  n1 = (int) p->data.nib[0];
	}
      else
	n1 = 0;
      if ((int) p->data.nib[1] < 16)
	{
	  n2 = (int) p->data.nib[1];
	}
      else
	n2 = 0;

      /* Just make sure there are an even number of nibbles in it, and
	 that the count is the same s the length */
      for (i = 0; p->data.nib[i] != E; i++)
	/*EMPTY*/ ;
      if (i & 1)
	abort ();
      p->length = i / 2;
    }

}


unsigned int
bfd_h8_disassemble (addr, info, hmode)
     bfd_vma addr;
     disassemble_info *info;
     int hmode;
{
  /* Find the first entry in the table for this opcode */
  static CONST char *regnames[] =
    {
      "r0h", "r1h", "r2h", "r3h", "r4h", "r5h", "r6h", "r7h",
      "r0l", "r1l", "r2l", "r3l", "r4l", "r5l", "r6l", "r7l"};
  
  static CONST char *wregnames[] =
    {
      "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
      "e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7"
      };
  
  static CONST char *lregnames[] =
    {
      "er0", "er1", "er2", "er3", "er4", "er5", "er6", "er7",
      "er0", "er1", "er2", "er3", "er4", "er5", "er6", "er7"
      }
  ;

  int rs = 0;
  int rd = 0;
  int rdisp = 0;
  int abs = 0;
  int plen = 0;
  static boolean init = 0;
  struct h8_opcode *q = h8_opcodes;
  char CONST **pregnames = hmode ? lregnames : wregnames;
  int status;
  int l;
  
  unsigned char data[20];  
  void *stream = info->stream;
  fprintf_ftype fprintf = info->fprintf_func;

  if (!init)
    {
      bfd_h8_disassemble_init ();
      init = 1;
    }

  status = info->read_memory_func(addr, data, 2, info);
  if (status != 0) 
    {
      info->memory_error_func(status, addr, info);
      return -1;
    }
  for (l = 2; status == 0 && l < 10; l+=2)
    {
      status = info->read_memory_func(addr+l, data+l, 2, info);
    }
  
  

  /* Find the exact opcode/arg combo */
  while (q->name)
    {
      op_type *nib;
      unsigned int len = 0;

      nib = q->data.nib;
      
      while (1)
	{
	  op_type looking_for = *nib;
	  int thisnib = data[len >> 1];
	  
	  thisnib = (len & 1) ? (thisnib & 0xf) : ((thisnib >> 4) & 0xf);
	  
	  if (looking_for < 16 && looking_for >=0) 
	    {
	      
	      if (looking_for != thisnib) 
		goto fail;
	    }
	  
	  else 
	    {
	      
	      if ((int) looking_for & (int) B31)
		{
		  if (! (((int) thisnib & 0x8) != 0)) 
		    goto fail;
		  looking_for = (op_type) ((int) looking_for & ~(int) B31);
		}
	      if ((int) looking_for & (int) B30)
		{
		  if (!(((int) thisnib & 0x8) == 0)) 
		    goto fail;
		  looking_for = (op_type) ((int) looking_for & ~(int) B30);
		}

	      if (looking_for & DBIT)
		{
		  if ((looking_for & 5) != (thisnib &5)) goto fail;
		  abs = (thisnib & 0x8) ? 2 : 1;
		}		  
	      
	      else  if (looking_for & (REG | IND|INC|DEC))
		{
		  if (looking_for & SRC)
		    {
		      rs = thisnib;
		    }
		  else
		    {
		      rd = thisnib;
		    }
		}
	      else if (looking_for & L_16)
		{
		  abs = (data[len >> 1]) * 256 + data[(len + 2) >> 1];
		  plen = 16;
	      
		}
	      else if(looking_for & ABSJMP)
		{
		  abs =
		    (data[1] << 16)
		      | (data[2] << 8)
			| (data[3]);
		}
	      else if(looking_for & MEMIND)
		{
		  abs = data[1];
		}
	      else if (looking_for & L_32)
		{
		  int i = len >> 1;
		  abs = (data[i] << 24)
		    | (data[i + 1] << 16)
		      | (data[i + 2] << 8)
			| (data[i+ 3]);

		  plen =32;
	      
		}
	      else if (looking_for & L_24)
		{
		  int i = len >> 1;
		  abs = (data[i] << 16) | (data[i + 1] << 8)|  (data[i+
								     2]);
		  plen =24;
		}
	      else if (looking_for & IGNORE)
		{
		  
		}
	      else if (looking_for & DISPREG)
		{
		  rdisp = thisnib;
		}
	      else if (looking_for & KBIT)
		{
		  switch (thisnib) 
		    {
		    case 9:
		      abs = 4;
		      break;
		    case 8:
		      abs = 2;
		      break;
		    case 0:
		      abs = 1;
		      break;
		    }
		}
	      else if (looking_for & L_8)
		{
		  plen = 8;		  
		  abs = data[len >> 1];
		}
	      else if (looking_for & L_3)
		{
		  plen = 3;
		  abs = thisnib;
		}
	      else if (looking_for & L_2)
		{
		  plen = 2;
		  abs = thisnib;
		}
	      else if (looking_for == E)
		{

		  {
		    int i;

		    for (i = 0; i < q->length; i++)
		      {
			fprintf (stream, "%02x ", data[i]);
		      }
		    for (; i < 6; i++)
		      {
			fprintf (stream, "   ");
		      }
		  }
		  fprintf (stream, "%s\t", q->name);
		  /* Fill in the args */
		  {
		    op_type *args = q->args.nib;
		    int hadone = 0;


		    while (*args != E)
		      {
			int x = *args;
			if (hadone)
			  fprintf (stream, ",");


			if (x & (IMM|KBIT|DBIT))
			  {
			
			    fprintf (stream, "#0x%x", (unsigned) abs);
			  }
			else if (x & REG)
			  {
			    int rn = (x & DST) ? rd : rs;
			    switch (x & SIZE)
			      {
			      case L_8:
				fprintf (stream, "%s", regnames[rn]);
				break;
			      case L_16:
				fprintf (stream, "%s", wregnames[rn]);
				break;
			      case L_P:
			      case L_32:
				fprintf (stream, "%s", lregnames[rn]);
				break;
		    
			      }
			  }

			else if (x & INC)
			  {
			    fprintf (stream, "@%s+", pregnames[rs]);
			  }
			else if (x & DEC)
			  {
			    fprintf (stream, "@-%s", pregnames[rd]);
			  }

			else if (x & IND)
			  {
			    int rn = (x & DST) ? rd : rs;
			    fprintf (stream, "@%s", pregnames[rn]);
			  }

			else if (x & (ABS|ABSJMP|ABSMOV))
			  {
			    fprintf (stream, "@0x%x:%d", (unsigned) abs, plen);
			  }

			else if (x & MEMIND)
			  {
			    fprintf (stream, "@@%d (%x)", abs, abs);
			  }

			else if (x & PCREL)
			  {
			    if (x & L_16) 
			      {
				abs  +=2;
				fprintf (stream, ".%s%d (%x)", (short) abs > 0 ? "+" : "", (short) abs,
					 addr + (short) abs + 2);
			      }
			    else {
			      fprintf (stream, ".%s%d (%x)", (char) abs > 0 ? "+" : "", (char) abs,
				       addr + (char) abs + 2);
			    }
			  }
			else if (x & DISP)
			  {
			    fprintf (stream, "@(0x%x:%d,%s)", abs,plen, pregnames[rdisp]);
			  }

			else if (x & CCR)
			  {

			    fprintf (stream, "ccr");
			  }

			else
			  fprintf (stream, "Hmmmm %x", x);
			hadone = 1;
			args++;
		      }
		  }
		  return q->length;
		}

      
	      else
		{
		  fprintf (stream, "Dont understand %x \n", looking_for);
		}
	    }
	  
	  len++;
	  nib++;
	}
      
    fail:
      q++;
    }

  /* Fell of the end */
  fprintf (stream, "%02x %02x        .word\tH'%x,H'%x",
	   data[0], data[1],
	   data[0], data[1]);
  return 2;
}

int 
print_insn_h8300 (addr, info)
bfd_vma addr; 
disassemble_info *info;
{
  return bfd_h8_disassemble (addr, info , 0);
}

 int 
print_insn_h8300h (addr, info)
bfd_vma addr;
disassemble_info *info;
{
  return bfd_h8_disassemble (addr, info , 1);
}

