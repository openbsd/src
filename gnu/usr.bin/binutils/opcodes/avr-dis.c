/* Disassemble AVR instructions.
   Copyright (C) 1999, 2000 Free Software Foundation, Inc.

   Contributed by Denis Chertykov <denisc@overta.ru>

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


#include "sysdep.h"
#include "dis-asm.h"
#include "opintl.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

#define IFMASK(a,b)     ((opcode & (a)) == (b))

static char* SREG_flags = "CZNVSHTI";
static char* sect94[] = {"COM","NEG","SWAP","INC","NULL","ASR","LSR","ROR",
			 0,0,"DEC",0,0,0,0,0};
static char* sect98[] = {"CBI","SBIC","SBI","SBIS"};
static char* branchs[] = {
  "BRCS","BREQ","BRMI","BRVS",
  "BRLT","BRHS","BRTS","BRIE",
  "BRCC","BRNE","BRPL","BRVC",
  "BRGE","BRHC","BRTC","BRID"
};

static char* last4[] = {"BLD","BST","SBRC","SBRS"};


static void dispLDD PARAMS ((u16, char *));

static void
dispLDD (opcode, dest)
     u16 opcode;
     char *dest;
{
  opcode = (((opcode & 0x2000) >> 8) | ((opcode & 0x0c00) >> 7)
	    | (opcode & 7));
  sprintf(dest, "%d", opcode);
}


static void regPP PARAMS ((u16, char *));

static void
regPP (opcode, dest)
     u16 opcode;
     char *dest;
{
  opcode = ((opcode & 0x0600) >> 5) | (opcode & 0xf);
  sprintf(dest, "0x%02X", opcode);
}


static void reg50 PARAMS ((u16, char *));

static void
reg50 (opcode, dest)
     u16 opcode;
     char *dest;
{
  opcode = (opcode & 0x01f0) >> 4;
  sprintf(dest, "R%d", opcode);
}


static void reg104 PARAMS ((u16, char *));

static void
reg104 (opcode, dest)
     u16 opcode;
     char *dest;
{
  opcode = (opcode & 0xf) | ((opcode & 0x0200) >> 5);
  sprintf(dest, "R%d", opcode);
}


static void reg40 PARAMS ((u16, char *));

static void
reg40 (opcode, dest)
     u16 opcode;
     char *dest;
{
  opcode = (opcode & 0xf0) >> 4;
  sprintf(dest, "R%d", opcode + 16);
}


static void reg20w PARAMS ((u16, char *));

static void
reg20w (opcode, dest)
     u16 opcode;
     char *dest;
{
  opcode = (opcode & 0x30) >> 4;
  sprintf(dest, "R%d", 24 + opcode * 2);
}


static void lit404 PARAMS ((u16, char *));

static void
lit404 (opcode, dest)
     u16 opcode;
     char *dest;
{
  opcode = ((opcode & 0xf00) >> 4) | (opcode & 0xf);
  sprintf(dest, "0x%02X", opcode);
}


static void lit204 PARAMS ((u16, char *));

static void
lit204 (opcode, dest)
     u16 opcode;
     char *dest;
{
  opcode = ((opcode & 0xc0) >> 2) | (opcode & 0xf);
  sprintf(dest, "0x%02X", opcode);
}


static void add0fff PARAMS ((u16, char *, int));

static void
add0fff (op, dest, pc)
     u16 op;
     char *dest;
     int pc;
{
  int rel_addr = (((op & 0xfff) ^ 0x800) - 0x800) * 2;
  sprintf(dest, ".%+-8d ; 0x%06X", rel_addr, pc + 2 + rel_addr);
}


static void add03f8 PARAMS ((u16, char *, int));

static void
add03f8 (op, dest, pc)
     u16 op;
     char *dest;
     int pc;
{
  int rel_addr = ((((op >> 3) & 0x7f) ^ 0x40) - 0x40) * 2;
  sprintf(dest, ".%+-8d ; 0x%06X", rel_addr, pc + 2 + rel_addr);
}


static u16 avrdis_opcode PARAMS ((bfd_vma, disassemble_info *));

static u16
avrdis_opcode (addr, info)
     bfd_vma addr;
     disassemble_info *info;
{
  bfd_byte buffer[2];
  int status;
  status = info->read_memory_func(addr, buffer, 2, info);
  if (status != 0)
    {
      info->memory_error_func(status, addr, info);
      return -1;
    }
  return bfd_getl16 (buffer);
}


int
print_insn_avr(addr, info)
     bfd_vma addr;
     disassemble_info *info;
{
  char rr[200];
  char rd[200];
  u16 opcode;
  void *stream = info->stream;
  fprintf_ftype prin = info->fprintf_func;
  int cmd_len = 2;

  opcode = avrdis_opcode (addr, info);

  if (IFMASK(0xd000, 0x8000))
    {
      char letter;
      reg50(opcode, rd);
      dispLDD(opcode, rr);
      if (opcode & 8)
	letter = 'Y';
      else
	letter = 'Z';
      if (opcode & 0x0200)
	(*prin) (stream, "    STD     %c+%s,%s", letter, rr, rd);
      else
	(*prin) (stream, "    LDD     %s,%c+%s", rd, letter, rr);
    }
  else
    {
      switch (opcode & 0xf000)
        {
        case 0x0000:
	  {
	    reg50(opcode, rd);
	    reg104(opcode, rr);
	    switch (opcode & 0x0c00)
	      {
	      case 0x0000:
		(*prin) (stream, "    NOP");
		break;
	      case 0x0400:
		(*prin) (stream, "    CPC     %s,%s", rd, rr);
		break;
	      case 0x0800:
		(*prin) (stream, "    SBC     %s,%s", rd, rr);
		break;
	      case 0x0c00:
		(*prin) (stream, "    ADD     %s,%s", rd, rr);
		break;
	      }
	  }
	  break;
        case 0x1000:
	  {
	    reg50(opcode, rd);
	    reg104(opcode, rr);
	    switch (opcode & 0x0c00)
	      {
	      case 0x0000:
		(*prin) (stream, "    CPSE    %s,%s", rd, rr);
		break;
	      case 0x0400:
		(*prin) (stream, "    CP      %s,%s", rd, rr);
		break;
	      case 0x0800:
		(*prin) (stream, "    SUB     %s,%s", rd, rr);
		break;
	      case 0x0c00:
		(*prin) (stream, "    ADC     %s,%s", rd, rr);
		break;
	      }
	  }
	  break;
        case 0x2000:
	  {
	    reg50(opcode, rd);
	    reg104(opcode, rr);
	    switch (opcode & 0x0c00)
	      {
	      case 0x0000:
		(*prin) (stream, "    AND     %s,%s", rd, rr);
		break;
	      case 0x0400:
		(*prin) (stream, "    EOR     %s,%s", rd, rr);
		break;
	      case 0x0800:
		(*prin) (stream, "    OR      %s,%s", rd, rr);
		break;
	      case 0x0c00:
		(*prin) (stream, "    MOV     %s,%s", rd, rr);
		break;
	      }
	  }
	  break;
        case 0x3000:
	  {
	    reg40(opcode, rd);
	    lit404(opcode, rr);
	    (*prin) (stream, "    CPI     %s,%s", rd, rr);
	  }
	  break;
        case 0x4000:
	  {
	    reg40(opcode, rd);
	    lit404(opcode, rr);
	    (*prin) (stream, "    SBCI    %s,%s", rd, rr);
	  }
	  break;
        case 0x5000:
	  {
	    reg40(opcode, rd);
	    lit404(opcode, rr);
	    (*prin) (stream, "    SUBI    %s,%s", rd, rr);
	  }
	  break;
        case 0x6000:
	  {
	    reg40(opcode, rd);
	    lit404(opcode, rr);
	    (*prin) (stream, "    ORI     %s,%s", rd, rr);
	  }
	  break;
        case 0x7000:
	  {
	    reg40(opcode, rd);
	    lit404(opcode, rr);
	    (*prin) (stream, "    ANDI    %s,%s", rd, rr);
	  }
	  break;
        case 0x9000:
	  {
	    switch (opcode & 0x0e00)
	      {
	      case 0x0000:
		{
		  reg50(opcode, rd);
		  switch (opcode & 0xf)
		    {
		    case 0x0:
		      {
			(*prin) (stream, "    LDS     %s,0x%04X", rd,
				 avrdis_opcode(addr + 2, info));
			cmd_len = 4;
		      }
		      break;
		    case 0x1:
		      (*prin) (stream, "    LD      %s,Z+", rd);
		      break;
		    case 0x2:
		      (*prin) (stream, "    LD      %s,-Z", rd);
		      break;
		    case 0x9:
		      (*prin) (stream, "    LD      %s,Y+", rd);
		      break;
		    case 0xa:
		      (*prin) (stream, "    LD      %s,-Y", rd);
		      break;
		    case 0xc:
		      (*prin) (stream, "    LD      %s,X", rd);
		      break;
		    case 0xd:
		      (*prin) (stream, "    LD      %s,X+", rd);
		      break;
		    case 0xe:
		      (*prin) (stream, "    LD      %s,-X", rd);
		      break;
		    case 0xf:
		      (*prin) (stream, "    POP     %s", rd);
		      break;
		    default:
		      (*prin) (stream, "    ????");
		      break;
		    }
		}
		break;
	      case 0x0200:
		{
		  reg50(opcode, rd);
		  switch (opcode & 0xf)
		    {
		    case 0x0:
		      {
			(*prin) (stream, "    STS     0x%04X,%s",
				 avrdis_opcode(addr + 2, info), rd);
			cmd_len = 4;
		      }
		      break;
		    case 0x1:
		      (*prin) (stream, "    ST      Z+,%s", rd);
		      break;
		    case 0x2:
		      (*prin) (stream, "    ST      -Z,%s", rd);
		      break;
		    case 0x9:
		      (*prin) (stream, "    ST      Y+,%s", rd);
		      break;
		    case 0xa:
		      (*prin) (stream, "    ST      -Y,%s", rd);
		      break;
		    case 0xc:
		      (*prin) (stream, "    ST      X,%s", rd);
		      break;
		    case 0xd:
		      (*prin) (stream, "    ST      X+,%s", rd);
		      break;
		    case 0xe:
		      (*prin) (stream, "    ST      -X,%s", rd);
		      break;
		    case 0xf:
		      (*prin) (stream, "    PUSH    %s", rd);
		      break;
		    default:
		      (*prin) (stream, "    ????");
		      break;
		    }
		}
		break;
	      case 0x0400:
		{
		  if (IFMASK(0x020c, 0x000c))
		    {
		      u32 k = ((opcode & 0x01f0) >> 3) | (opcode & 1);
		      k = (k << 16) | avrdis_opcode(addr + 2, info);
		      if (opcode & 0x0002)
			(*prin) (stream, "    CALL    0x%06X", k*2);
		      else
			(*prin) (stream, "    JMP     0x%06X", k*2);
		      cmd_len = 4;
		    }
		  else if (IFMASK(0x010f, 0x0008))
		    {
		      int sf = (opcode & 0x70) >> 4;
		      if (opcode & 0x0080)
			(*prin) (stream, "    CL%c", SREG_flags[sf]);
		      else
			(*prin) (stream, "    SE%c", SREG_flags[sf]);
		    }
		  else if (IFMASK(0x000f, 0x0009))
		    {
		      if (opcode & 0x0100)
			(*prin) (stream, "    ICALL");
		      else
			(*prin) (stream, "    IJMP");
		    }
		  else if (IFMASK(0x010f, 0x0108))
		    {
		      if (IFMASK(0x0090, 0x0000))
			(*prin) (stream, "    RET");
		      else if (IFMASK(0x0090, 0x0010))
			(*prin) (stream, "    RETI");
		      else if (IFMASK(0x00e0, 0x0080))
			(*prin) (stream, "    SLEEP");
		      else if (IFMASK(0x00e0, 0x00a0))
			(*prin) (stream, "    WDR");
		      else if (IFMASK(0x00f0, 0x00c0))
			(*prin) (stream, "    LPM");
		      else if (IFMASK(0x00f0, 0x00d0))
			(*prin) (stream, "    ELPM");
		      else
			(*prin) (stream, "    ????");
		    }
		  else
		    {
		      const char* p;
		      reg50(opcode, rd);
		      p = sect94[opcode & 0xf];
		      if (!p)
			p = "????";
		      (*prin) (stream, "    %-8s%s", p, rd);
		    }
		}
		break;
	      case 0x0600:
		{
		  if (opcode & 0x0200)
		    {
		      lit204(opcode, rd);
		      reg20w(opcode, rr);
		      if (opcode & 0x0100)
			(*prin) (stream, "    SBIW    %s,%s", rr, rd);
		      else
			(*prin) (stream, "    ADIW    %s,%s", rr, rd);
		    }
		}
		break;
	      case 0x0800:
	      case 0x0a00:
		{
		  (*prin) (stream, "    %-8s0x%02X,%d",
			   sect98[(opcode & 0x0300) >> 8],
			   (opcode & 0xf8) >> 3,
			   opcode & 7);
		}
		break;
	      default:
		{
		  reg50(opcode, rd);
		  reg104(opcode, rr);
		  (*prin) (stream, "    MUL     %s,%s", rd, rr);
		}
	      }
	  }
	  break;
        case 0xb000:
	  {
	    reg50(opcode, rd);
	    regPP(opcode, rr);
	    if (opcode & 0x0800)
	      (*prin) (stream, "    OUT     %s,%s", rr, rd);
	    else
	      (*prin) (stream, "    IN      %s,%s", rd, rr);
	  }
	  break;
        case 0xc000:
	  {
	    add0fff(opcode, rd, addr);
	    (*prin) (stream, "    RJMP    %s", rd);
	  }
	  break;
        case 0xd000:
	  {
	    add0fff(opcode, rd, addr);
	    (*prin) (stream, "    RCALL   %s", rd);
	  }
	  break;
        case 0xe000:
	  {
	    reg40(opcode, rd);
	    lit404(opcode, rr);
	    (*prin) (stream, "    LDI     %s,%s", rd, rr);
	  }
	  break;
        case 0xf000:
	  {
	    if (opcode & 0x0800)
	      {
		reg50(opcode, rd);
		(*prin) (stream, "    %-8s%s,%d",
			 last4[(opcode & 0x0600) >> 9],
			 rd, opcode & 7);
	      }
	    else
	      {
		char* p;
		add03f8(opcode, rd, addr);
		p = branchs[((opcode & 0x0400) >> 7) | (opcode & 7)];
		(*prin) (stream, "    %-8s%s", p, rd);
	      }
	  }
	  break;
        }
    }
  return cmd_len;
}
