/*	$OpenBSD: db_disasm.c,v 1.4 1999/02/09 06:36:24 smurph Exp $	*/
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * m88k disassembler for use in ddb
 */

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>		/* DB_STGY_PROC, db_printsym() */
#include <ddb/db_access.h>	/* db_get_value() */
#include <ddb/db_output.h>	/* db_printf() */

static char *instwidth[4] = {
	".d", "  ", ".h", ".b"
};

static char *condname[6] = {
	"gt0 ", "eq0 ", "ge0 ", "lt0 ", "ne0 ", "le0 "
};  

static char *ctrlreg[64] = {
	"cr0(PID)   ",
	"cr1(PSR)   ",
	"cr2(EPSR)  ",
	"cr3(SSBR)  ",
	"cr4(SXIP)  ",
	"cr5(SNIP)  ",
	"cr6(SFIP)  ",
	"cr7(VBR)   ",
	"cr8(DMT0)  ",
	"cr9(DMD0)  ",
	"cr10(DMA0) ",
	"cr11(DMT1) ",
	"cr12(DMD1) ",
	"cr13(DMA1) ",
	"cr14(DMT2) ",
	"cr15(DMD2) ",
	"cr16(DMA2) ",
	"cr17(SR0)  ",
	"cr18(SR1)  ",
	"cr19(SR2)  ",
	"cr20(SR3)  ",
	"fcr0(FPECR)",
	"fcr1(FPHS1)",
	"fcr2(FPLS1)",
	"fcr3(FPHS2)",
	"fcr4(FPLS2)",
	"fcr5(FPPT) ",
	"fcr6(FPRH) ",
	"fcr7(FPRL) ",
	"fcr8(FPIT) ",
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	"fcr62(FPSR)",
	"fcr63(FPCR)"
};

#define printval(x)  if (x<0) db_printf ("-0x%X", -x); else db_printf("0x%X",x)

/* Handlers immediate integer arithmetic instructions */      
static void
oimmed(long inst, char  *opcode, long iadr)
{
  register int Linst = inst & 0177777;
  register int Hinst = inst >> 16;
  register int H6inst = Hinst >> 10;
  register int rs1 = Hinst & 037;
  register int rd = ( Hinst >> 5 ) & 037;

  if (( H6inst > 017 ) && ( H6inst < 030 ) && ( H6inst & 01) == 1 ) 
    db_printf("\t%s.u",opcode);
  else {
    db_printf("\t%s",opcode);
    db_printf("  ");
  }
  db_printf("\t\tr%-3d,r%-3d,", rd, rs1);
  printval(Linst);
}


/* Handles instructions dealing with control registers */
static void
ctrlregs(long inst, char *opcode, long iadr)
{
  register int L6inst = (inst >> 11) & 037;
  register int creg = (inst >> 5) & 077;
  register int rd = (inst >> 21) & 037;
  register int rs1 = (inst >> 16) & 037;
  
  db_printf("\t%s",opcode);
  
  if ( L6inst == 010 || L6inst == 011 )
    db_printf("\t\tr%-3d,%s", rd, ctrlreg[creg]);
  else if ( L6inst == 020 || L6inst == 021 )
    db_printf("\t\tr%-3d,%s", rs1, ctrlreg[creg]);
  else
    db_printf("\t\tr%-3d,r%-3d,%s", rd, rs1, ctrlreg[creg]);
}


static void
printsod(int t)
{
  if ( t == 0 ) 
    db_printf("s");
  else
    db_printf("d");
}

/* Handles floating point instructions */
static void
sindou(int inst, char *opcode, long iadr)
{
  register int rs2 = inst & 037;
  register int td = ( inst >> 5 ) & 03;
  register int t2 = ( inst >> 7 ) & 03;
  register int t1 = ( inst >> 9 ) & 03;
  register int rs1 = ( inst >> 16 ) & 037;
  register int rd = ( inst >> 21 ) & 037;
  register int checkbits  = ( inst >> 11 ) & 037;
  
  db_printf("\t%s.",opcode);
  printsod(td);
  if (( checkbits > 010 && checkbits < 014 ) || ( checkbits == 04 )) {
    printsod(t2);
    db_printf(" ");
    if ( checkbits == 012 || checkbits == 013 )
      db_printf("\t\tr%-3d,r%-3d", rd, rs2);
    else
      db_printf("\t\tr%-3d,r%-3d", rd, rs2);
  }
  else{
    printsod(t1);printsod(t2);
    db_printf("\t\tr%-3d,r%-3d,r%-3d", rd, rs1, rs2);
  }
}


static void
jump(long inst, char *opcode, long iadr)
{
  register int rs2 = inst & 037;
  register int Nbit = ( inst >> 10 ) & 01;
  
  db_printf("\t%s",opcode);
  if ( Nbit == 1 )
    db_printf(".n");
  else
    db_printf("  ");
  db_printf("\t\tr%-3d",rs2);
}


/* Handles ff1, ff0, tbnd and rte instructions */ 
static void
instset(long inst, char *opcode, long iadr)
{
  register int rs2 = inst & 037;
  register int rs1 = ( inst >> 16 ) & 037;
  register int rd = ( inst >> 21 ) & 037;
  register int checkbits = ( inst >> 10 ) & 077;
  register int H6inst = ( inst >> 26 ) & 077;
  
  db_printf("\t%s",opcode);
  if ( H6inst == 076 ) {
    db_printf("\t\tr%-3d,",rs1);
    printval(inst & 0177777);
  }
  else if (( checkbits == 072 ) || ( checkbits == 073 ))
    db_printf("\t\tr%-3d,r%-3d", rd, rs2);
  else if ( checkbits == 076 )
    db_printf("\t\tr%-3d,r%-3d",rs1,rs2);
}

static void
symofset(int  disp, int  bit, int iadr)
{
  long addr;

  if ( disp & (1 << (bit-1)) ) {
    /* negative value */
    addr = iadr + ((disp << 2) | (~0 << bit));
  }
  else {
    addr = iadr + (disp << 2);
  }
  db_printsym(addr,DB_STGY_PROC);
  return;
}

static void
obranch(int inst, char *opcode, long iadr)
{
  int cond = ( inst >> 26 ) & 01;
  int disp = inst &0377777777;
  
  if ( cond == 0 ) {
    db_printf("\t%s\t\t",opcode);
    symofset(disp, 26, iadr);
  }
  else {
    db_printf("\t%s.n\t\t",opcode);
    symofset(disp, 26, iadr);
  }
}


/* Handles branch on conditions instructions */
static void
brcond(int inst, char *opcode, long iadr)
{
  int cond = ( inst >> 26 ) & 1;
  int match = ( inst >> 21 ) & 037;
  int rs = ( inst >> 16 ) & 037;
  int disp = ( inst & 0177777 );
  
  if ( cond == 0 )
    db_printf("\t%s\t\t", opcode); 
  else
    db_printf("\t%s.n\t\t", opcode);
  if ( ( ( inst >> 27 ) & 03 ) == 1 )
    switch (match) {
    case 1 : db_printf("%s,", condname[0]); break;
    case 2 : db_printf("%s,", condname[1]); break;
    case 3 : db_printf("%s,", condname[2]); break;
    case 12: db_printf("%s,", condname[3]); break;
    case 13: db_printf("%s,", condname[4]); break;
    case 14: db_printf("%s,", condname[5]); break;
    default: printval(match); 
      db_printf(",");
    }
  else {
    printval(match);
    db_printf(",");
  }

  db_printf("r%-3d,", rs);
  symofset(disp,16, iadr);
}


static void
otrap(int inst, char *opcode, long iadr)
{
  int vecno = inst & 0777;
  int match = ( inst >> 21 ) & 037;
  int rs = ( inst >> 16 ) & 037;
  
  db_printf("\t%s\t",opcode);
  if ( ( ( inst >> 12 ) & 017 ) == 0xe )
    switch (match) {
    case 1 : db_printf("%s,", condname[0]);break;
    case 2 : db_printf("%s,", condname[1]);break;
    case 3 : db_printf("%s,", condname[2]);break;
    case 12: db_printf("%s,", condname[3]);break;
    case 13: db_printf("%s,", condname[4]);break;
    case 14: db_printf("%s,", condname[5]);break;
    default: printval(match);
      db_printf(",");
    }
  else {
    printval(match);
    db_printf(",");
  }
  db_printf("\tr%-3d,", rs);
  printval(vecno);
}


/* Handles 10 bit immediate bit field operations */
static void
obit(int inst, char *opcode, long iadr)
{
  int rs = ( inst >> 16 ) & 037;
  int rd = ( inst >> 21 ) & 037;
  int width = ( inst >> 5 ) & 037;
  int offset = ( inst & 037 );  
  
  db_printf("\t%s\t\tr%-3d,r%-3d,", opcode, rd, rs); 
  if ( ( ( inst >> 10 ) & 077 ) == 052 ) {
    db_printf("<"); 
    printval(offset); 
    db_printf(">"); 
  }
  else
    {
      printval(width);
      db_printf("<");
      printval(offset);
      db_printf(">");
    }
}


/* Handles triadic mode bit field instructions */
static void
bitman(int inst, char *opcode, long iadr)
{
  
  int rs1 = ( inst >> 16 ) & 037;
  int rd  = ( inst >> 21 ) & 037;
  int rs2 = inst & 037;
  
  db_printf("\t%s\t\tr%-3d,r%-3d,r%-3d", opcode, rd, rs1, rs2);
}


/* Handles immediate load/store/exchange instructions */
static void
immem(int inst, char *opcode, long iadr)
{
  register int immed  = inst & 0xFFFF;
  register int rd     = (inst >> 21) & 037;
  register int rs     = (inst >> 16) & 037;
  register int st_lda = (inst >> 28) & 03;
  register int aryno  = (inst >> 26) & 03;
  char c = ' ';
  
  if (!st_lda)	{
    if ((aryno == 0) || (aryno == 01))
      opcode = "xmem";
    else
      opcode = "ld";
    if (aryno == 0)
      aryno = 03;
    if (!(aryno == 01))
      c = 'u';
  }
  else
    if (st_lda == 01)
      opcode = "ld";
  
  db_printf("\t%s%s%c\t\tr%-3d,r%-3d,", opcode, instwidth[aryno], 
	    c, rd, rs);
  printval(immed);
}


/* Handles triadic mode load/store/exchange instructions */
static void
nimmem(int inst, char *opcode, long iadr)
{
  register int scaled  = (inst >> 9) & 01;
  register int rd      = (inst >> 21) & 037;
  register int rs1     = (inst >> 16) & 037;
  register int rs2     = inst & 037;
  register int st_lda  = (inst >> 12) & 03;
  register int aryno   = (inst >> 10) & 03;
  register int user_bit = 0;
  int signed_fg  = 1;
  char *user           = "    ";
  char c = ' ';
  
  if (!st_lda)	{
    if ((aryno == 0) || (aryno == 01))
      opcode = "xmem";
    else
      opcode = "ld";
    if (aryno == 0)
      aryno = 03;
    if (!(aryno == 01))	{
      c = 'u';
      signed_fg = 0;
    }
  }
  else
    if (st_lda == 01)
      opcode = "ld";
  
  if (!(st_lda == 03))	{
    user_bit = (inst >> 8) & 01;
    if (user_bit)
      user = ".usr";
  }
  
  if (user_bit && signed_fg && (aryno == 01)) {
    if (st_lda)
      db_printf("\t%s%s\tr%-3d,r%-3d", opcode,
		user, rd, rs1);
    else
      db_printf("\t%s%s\tr%-3d,r%-3d", opcode,
		user, rd, rs1);
  }	
  else	
    if (user_bit && signed_fg)
      db_printf("\t%s%s%s\tr%-3d,r%-3d", opcode, 
		instwidth[aryno], user, rd, rs1);
    else
      db_printf("\t%s%s%c%s\tr%-3d,r%-3d", opcode, 
		instwidth[aryno], c, user, rd, rs1);
  
  if (scaled)
    db_printf("[r%-3d]", rs2);
  else
    db_printf(",r%-3d", rs2);
}


/* Handles triadic mode logical instructions */
static void
lognim(int inst, char *opcode, long iadr)
{
  register int rd   = (inst >> 21) & 037;
  register int rs1  = (inst >> 16) & 037;
  register int rs2  = inst & 037;
  register int complemt = (inst >> 10) & 01;
  char *c = "  ";
  
  if (complemt)
    c = ".c";
  
  db_printf("\t%s%s\t\tr%-3d,r%-3d,r%-3d", opcode, c, rd, rs1, rs2);
}


/* Handles triadic mode arithmetic instructions */
static void
onimmed(int inst, char *opcode, long iadr)
{
  register int rd   = (inst >> 21) & 037;
  register int rs1  = (inst >> 16) & 037;
  register int rs2  = inst & 037;
  register int carry = (inst >> 8) & 03;
  register int nochar = (inst >> 10) & 07;
  register int nodecode = (inst >> 11) & 01;
  char *tab, *c ;
  
  if (nochar > 02)
    tab = "\t\t";
  else
    tab = "\t";
  
  if (!nodecode) {
    if (carry == 01)
      c = ".co ";
    else
      if (carry == 02)
	c = ".ci ";
      else
	if (carry == 03)
	  c = ".cio";
	else
	  c = "    ";
  }
  else
    c = "    ";
  
  db_printf("\t%s%s%sr%-3d,r%-3d,r%-3d", opcode, c,
	    tab, rd, rs1, rs2);
}

static struct opdesc {
    unsigned mask, match;
    void (*opfun) ();
    char *farg;
} opdecode[] = {

    /* ORDER IS IMPORTANT BELOW */

    {	0xF0000000U, 0x00000000U, immem, 0,		},
    {	0xF0000000U, 0x10000000U, immem, 0,		},
    {	0xF0000000U, 0x20000000U, immem, "st"		},
    {	0xF0000000U, 0x30000000U, immem, "lda"		},

    {	0xF8000000U, 0x40000000U, oimmed, "and" 	},
    {	0xF8000000U, 0x48000000U, oimmed, "mask" 	},
    {	0xF8000000U, 0x50000000U, oimmed, "xor" 	},
    {	0xF8000000U, 0x58000000U, oimmed, "or" 		},
    {	0xFC000000U, 0x60000000U, oimmed, "addu" 	},
    {	0xFC000000U, 0x64000000U, oimmed, "subu" 	},
    {	0xFC000000U, 0x68000000U, oimmed, "divu" 	},
    {	0xFC000000U, 0x6C000000U, oimmed, "mul" 	},
    {	0xFC000000U, 0x70000000U, oimmed, "add" 	},
    {	0xFC000000U, 0x74000000U, oimmed, "sub" 	},
    {	0xFC000000U, 0x78000000U, oimmed, "div" 	},
    {	0xFC000000U, 0x7C000000U, oimmed, "cmp" 	},

    {	0xFC00F800U, 0x80004000U, ctrlregs, "ldcr" 	},
    {	0xFC00F800U, 0x80004800U, ctrlregs, "fldcr" 	},
    {	0xFC00F800U, 0x80008000U, ctrlregs, "stcr" 	},
    {	0xFC00F800U, 0x80008800U, ctrlregs, "fstcr" 	},
    {	0xFC00F800U, 0x8000C000U, ctrlregs, "xcr" 	},
    {	0xFC00F800U, 0x8000C800U, ctrlregs, "fxcr" 	},

    {	0xFC00F800U, 0x84000000U, sindou, "fmul" 	},
    {	0xFC1FFF80U, 0x84002000U, sindou, "flt" 	},
    {	0xFC00F800U, 0x84002800U, sindou, "fadd" 	},
    {	0xFC00F800U, 0x84003000U, sindou, "fsub" 	},
    {	0xFC00F860U, 0x84003800U, sindou, "fcmp" 	},
    {	0xFC1FFE60U, 0x84004800U, sindou, "int" 	},
    {	0xFC1FFE60U, 0x84005000U, sindou, "nint" 	},
    {	0xFC1FFE60U, 0x84005800U, sindou, "trnc" 	},
    {	0xFC00F800U, 0x84007000U, sindou, "fdiv" 	},

    {	0xF8000000U, 0xC0000000U, obranch, "br" 	},
    {	0xF8000000U, 0xC8000000U, obranch, "bsr" 	},

    {	0xF8000000U, 0xD0000000U, brcond, "bb0" 	},
    {	0xF8000000U, 0xD8000000U, brcond, "bb1" 	},
    {	0xF8000000U, 0xE8000000U, brcond, "bcnd" 	},

    {	0xFC00FC00U, 0xF0008000U, obit, "clr" 		},
    {	0xFC00FC00U, 0xF0008800U, obit, "set" 		},
    {	0xFC00FC00U, 0xF0009000U, obit, "ext" 		},
    {	0xFC00FC00U, 0xF0009800U, obit, "extu" 		},
    {	0xFC00FC00U, 0xF000A000U, obit, "mak" 		},
    {	0xFC00FC00U, 0xF000A800U, obit, "rot" 		},

    {	0xFC00FE00U, 0xF000D000U, otrap, "tb0" 		},
    {	0xFC00FE00U, 0xF000D800U, otrap, "tb1" 		},
    {	0xFC00FE00U, 0xF000E800U, otrap, "tcnd" 	},

    {	0xFC00F2E0U, 0xF4000000U, nimmem, 0,		},
    {	0xFC00F2E0U, 0xF4000200U, nimmem, 0,		},
    {	0xFC00F2E0U, 0xF4001000U, nimmem, 0,		},
    {	0xFC00F2E0U, 0xF4001200U, nimmem, 0,		},
    {	0xFC00F2E0U, 0xF4002000U, nimmem, "st" 		},
    {	0xFC00F2E0U, 0xF4002200U, nimmem, "st" 		},
    {	0xFC00F2E0U, 0xF4003000U, nimmem, "lda" 	},
    {	0xFC00F2E0U, 0xF4003200U, nimmem, "lda" 	},

    {	0xFC00FBE0U, 0xF4004000U, lognim, "and" 	},
    {	0xFC00FBE0U, 0xF4005000U, lognim, "xor" 	},
    {	0xFC00FBE0U, 0xF4005800U, lognim, "or" 		},

    {	0xFC00FCE0U, 0xF4006000U, onimmed, "addu" 	},
    {	0xFC00FCE0U, 0xF4006400U, onimmed, "subu" 	},
    {	0xFC00FCE0U, 0xF4006800U, onimmed, "divu" 	},
    {	0xFC00FCE0U, 0xF4006C00U, onimmed, "mul" 	},
    {	0xFC00FCE0U, 0xF4007000U, onimmed, "add" 	},
    {	0xFC00FCE0U, 0xF4007400U, onimmed, "sub" 	},
    {	0xFC00FCE0U, 0xF4007800U, onimmed, "div" 	},
    {	0xFC00FCE0U, 0xF4007C00U, onimmed, "cmp" 	},
 
    {	0xFC00FFE0U, 0xF4008000U, bitman, "clr" 	},
    {	0xFC00FFE0U, 0xF4008800U, bitman, "set" 	},
    {	0xFC00FFE0U, 0xF4009000U, bitman, "ext" 	},
    {	0xFC00FFE0U, 0xF4009800U, bitman, "extu" 	},
    {	0xFC00FFE0U, 0xF400A000U, bitman, "mak" 	},
    {	0xFC00FFE0U, 0xF400A800U, bitman, "rot" 	},

    {	0xFC00FBE0U, 0xF400C000U, jump, "jmp"		},
    {	0xFC00FBE0U, 0xF400C800U, jump, "jsr"		},

    {	0xFC00FFE0U, 0xF400E800U, instset, "ff1"	},
    {	0xFC00FFE0U, 0xF400EC00U, instset, "ff0"	},
    {	0xFC00FFE0U, 0xF400F800U, instset, "tbnd"	},
    {	0xFC00FFE0U, 0xF400FC00U, instset, "rte"	},
    {	0xFC000000U, 0xF8000000U, instset, "tbnd"	},
    {	0,0,0,0 }
};

static char *badop = "\t???";

int
m88k_print_instruction(unsigned iadr, long inst)
{
  register struct opdesc *p;
  
  /* this messes up "orb" instructions ever so slightly, */
  /* but keeps us in sync between routines... */
  if (inst == 0) {
    db_printf ("\t.word 0");
  }
  else 
    {
      for (p = opdecode; p->mask; p++)
	if ((inst & p->mask) == p->match) {
	  (*p->opfun) (inst, p->farg, iadr);
	  break;
	}
      if (!p->mask)
	db_printf (badop);
    }

  return iadr+4;
}

db_addr_t
db_disasm(db_addr_t loc, boolean_t altfmt)
{
  m88k_print_instruction(loc, db_get_value(loc, 4, FALSE));
  db_printf ("\n");
  return loc+4;
}
