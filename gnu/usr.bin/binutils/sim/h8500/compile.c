/* Simulator for the Hitachi H8/500 architecture.

   Written by Steve Chamberlain of Cygnus Support.
   sac@cygnus.com

   This file is part of H8/500 sim


		THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

*/

#include "config.h"

#include <signal.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <sys/param.h>
#include <setjmp.h>
#include "ansidecl.h"
#include "callback.h"
#include "remote-sim.h"

#define O_RECOMPILE 85
#define DEFINE_TABLE
#define DISASSEMBLER_TABLE


int debug;

/* This code can be compiled with any old C compiler, in which case
   four or five switch statements will be executed for each
   instruction simulated.  It can be compiled with GCC, then the
   simulated instructions thread through the code fragments, and
   everything goes much faster.

   These definitions make the code work either way
*/
#ifdef __GNUC__
#define DISPATCH(X) goto *(X); do
#define LABEL(X) X##_L
#define LABELN(X,N) X##_L##N
#define LABEL_REF(X) &&X##_L
#define LABEL_REFN(X,N) &&X##_L##N
#define ENDDISPATCH while (0);
#define fastref void *

#define DEFAULT ;
#define INLINE __inline__
#else
#define DEFAULT default :
#define DISPATCH(X) switch (X)
#define LABEL(X) case X
#define LABELN(X,N) case X
#define LABEL_REF(X) X
#define LABEL_REFN(X,N) X
#define ENDDISPATCH
#define fastref int



#define INLINE
#define STORE_REG_B 	1
#define STORE_REG_W 	2
#define STORE_INC_B 	3
#define STORE_INC_W 	4
#define STORE_DEC_B 	5
#define STORE_DEC_W 	6
#define STORE_DISP_B 	7
#define STORE_DISP_W 	8
#define STORE_CRB 	9
#define STORE_CRW 	10
#define STORE_REG_L 	11
#define STORE_NOP 	12

#define FETCH_NOP 	9
#define FETCH_REG_B 	10
#define FETCH_REG_W 	11
#define FETCH_INC_B 	12
#define FETCH_INC_W 	13
#define FETCH_DEC_B 	14
#define FETCH_DEC_W 	15
#define FETCH_DISP_B 	16
#define FETCH_DISP_W 	17
#define FETCH_IMM 	18
#define FETCH_CRB 	19
#define FETCH_CRW 	20
#define FETCH_LVAL 	21
#define FETCH_LVAL24 	22
#define FETCH_REG_L 	23

#define FLAG_m 		20
#define FLAG_M 		21
#define FLAG_A 		22
#define FLAG_NONE 	23
#define FLAG_NOSTORE 	24
#define FLAG_CLEAR 	25
#define FLAG_a 		26
#define FLAG_BRANCH 	27
#define FLAG_special 	28

#define FLAG_shiftword 	29
#define FLAG_shiftbyte 	30

#define FLAG_multword 	31
#define FLAG_multbyte 	32
#endif


#define h8500_table h8500_compile_table
#include "../opcodes/h8500-opc.h"

#include "inst.h"

#define LOW_BYTE(x) ((x) & 0xff)
#define HIGH_BYTE(x) (((x)>>8) & 0xff)
#define NORMAL_CP ((cpu.regs[R_CP].c - cpu.memory)>>16)
#define NORMAL_DP ((cpu.regs[R_DP].c - cpu.memory)>>16)
#define NORMAL_EP ((cpu.regs[R_EP].c - cpu.memory)>>16)
#define NORMAL_TP ((cpu.regs[R_TP].c - cpu.memory)>>16)
#define SET_NORMREG(x,y) ((cpu.regs[x].l = (y)))
#define GET_NORMREG(x) (cpu.regs[x].l)
#define SET_SEGREG(x,y) { cpu.regs[x].c = ((y) & 0xff0000) + cpu.memory;}
#define GET_SEGREG(x)  ( (cpu.regs[x].c  - cpu.memory ) >> 16)
#define SET_NORMAL_CPPC(x) { pc = (x) & 0xffff; SET_SEGREG(R_CP, (x));}
#define NORMAL_SR ((N<<3)|(Z<<2)|(V<<1)|(C))
#define P(X,Y) ((X<<8) | Y)

#define BUILDSR()   cpu.regs[R_SR].s[LOW] = (N << 3) | (Z << 2) | (V<<1) | C;

#define GETSR()		    \
  C = (cpu.regs[R_SR].s[LOW] >> 0) & 1;\
  V = (cpu.regs[R_SR].s[LOW] >> 1) & 1;\
  Z = (cpu.regs[R_SR].s[LOW] >> 2) & 1;\
  N = (cpu.regs[R_SR].s[LOW] >> 3) & 1;

#ifdef __CHAR_IS_SIGNED__
#define SEXTCHAR(x) ((char)(x))
#endif

#ifndef SEXTCHAR
#define SEXTCHAR(x) ((x & 0x80) ? (x | ~0xff):x)
#endif

#define SEXTSHORT(x) ((short)(x))

/* Which segment registers go with which pointer registers */
static unsigned char **segmap[R_LAST];
static unsigned char *(regptr[R_LAST][3]);
static unsigned char *(segregptr[R_LAST][3]);
static cpu_state_type cpu;

static int segforreg[] = {R_DP, R_DP, R_DP, R_DP,
			    R_EP, R_EP, R_TP, R_TP,
			    R_DP, R_DP, R_DP, R_DP,
			    R_EP, R_EP, R_TP, R_TP};
int LOW;
int HIGH;

/* routines for getting and storing args */
#define elval(struct, lit) \
 (((*(struct.reg.wptr) + lit) & 0xffff) + (*(struct.r2.segreg)))

#define displval(s) elval((s),(s).literal)

#define ireglval(struct) elval(struct, 0)
#define wordat(x) (((x)[0] << 8) | (x)[1])
#define longat(x) ((wordat((x))<<16)|(wordat((x)+2)))
#define byteat(x) ((x)[0])

#define setwordat(x,y) {x[0] =( y)>>8; x[1] = y;}
#define setbyteat(x,y) {x[0] = y;}

/*#define setalignedwordat(x,y) {((short *)x)[0] =y;}*/
/*
statics
*/

ea_type rd;
ea_type rs;
ea_type imm;
ea_type cr;
ea_type ea;
ea_type nop;
ea_type lval;
ea_type lval24;

ea_type eavector[2];

int disp;

#define JBYTE 0
#define JWORD 1
#define JLONG 2

typedef union
{
  struct
    {
      fastref srcabyte;
      fastref srcaword;
      fastref srcalong;

      fastref srcbbyte;
      fastref srcbword;
      fastref srcblong;

      fastref dstbyte;
      fastref dstword;
      fastref dstlong;
    } s;
  struct
    {
      fastref byte;
      fastref word;
      fastref lon;
    } a[3];

  fastref j[9];
} size_ptr;

union
{
  struct ea_struct
    {
      size_ptr ea_nop;
      size_ptr ea_reg;
      size_ptr ea_inc;
      size_ptr ea_dec;
      size_ptr ea_disp;

      size_ptr ea_imm;
      size_ptr ea_cr;
      size_ptr ea_lval;
      size_ptr ea_lval24;
    } s;
#define N_EATYPES (sizeof(struct ea_struct) / sizeof(size_ptr))
  size_ptr a[N_EATYPES];
} eas;

/* This function takes an ea structure filled in for the 1st source
 operand and modifies it to be for either the 1st, 2nd or dst operand */

static void
howto_workout (encoded, semiencoded, n)
     ea_type *encoded;
     ea_type *semiencoded;
     int n;
{
  int i;
  *encoded = *semiencoded;

  for (i = 0; i < N_EATYPES; i++)
    {
      if (encoded->type == eas.a[i].s.srcabyte)
	{
	  encoded->type = eas.a[i].a[n].byte;
	  return;
	}
      else if (encoded->type == eas.a[i].s.srcaword)
	{
	  encoded->type = eas.a[i].a[n].word;
	  return;
	}
      else if (encoded->type == eas.a[i].s.srcalong)
	{
	  encoded->type = eas.a[i].a[n].lon;
	  return;
	}
    }

  abort ();
}

fastref flag_shiftword;
fastref flag_shiftbyte;
fastref flag_multword;
fastref flag_multbyte;
fastref flag_mp;
fastref flag_special;
fastref flag_Mp;
fastref flag_ap;
fastref flag_Ap;
fastref flag_nonep;
fastref flag_nostorep;
fastref flag_clearp;
fastref flag_branch;
fastref exec_dispatch[100];

static int
get_now ()
{
  return time ((char *) 0);
}

static int
now_persec ()
{
  return 1;
}

static void
gotcr (ptr, n)
     ea_type *ptr;
     int n;
{
  int size;
  n &= 0x7;
  if (n == 0)
    {
      abort ();
    }
  else
    {
      ptr->type = eas.s.ea_cr.j[JBYTE];
      ptr->reg.bptr = segregptr[n][JLONG];
    }
}
static void
gotreg (ptr, n, size)
     ea_type *ptr;
     int n;
     int size;
{
  n &= 0x7;
  ptr->type = eas.s.ea_reg.j[size];
  ptr->reg.bptr = regptr[n][size];
}

static void
gotinc (ptr, n, inc, size)
     ea_type *ptr;
     int n;
     int size;
{
  n &= 0x7;
  if (inc > 0)
    {
      ptr->type = eas.s.ea_inc.j[size];
    }
  else
    {
      ptr->type = eas.s.ea_dec.j[size];
    }
  ptr->reg.bptr = regptr[n][JWORD];
  ptr->r2.segreg = segmap[n];
}


static void
gotabs (ptr, disp, reg, size)
     ea_type *ptr;
     int disp;
     int reg;
     int size;
{
  ptr->type = eas.s.ea_disp.j[size];
  ptr->reg.bptr = regptr[reg][JWORD];
  ptr->r2.segreg = segmap[reg];
  ptr->literal = disp;
}

static void
gotind (ptr, disp, reg, size)
     ea_type *ptr;
     int disp;
     int reg;
     int size;
{
  gotabs (ptr, disp, reg & 0x7, size);
}

static void
gotimm (ptr, val)
     ea_type *ptr;
     int val;
{
  ptr->type = eas.s.ea_imm.j[0];
  ptr->literal = val;
}

static void
indoff (ptr)
     ea_type *ptr;
{
  int i;
  for (i = 0; i < 6; i++)
    {
      if (ptr->type == eas.s.ea_disp.j[i])
	{
	  ptr->type = eas.s.ea_lval.j[i];
	  return;
	}
    }
}

thinkabout_shifts (d, bytesized)
     decoded_inst *d;
     int bytesized;
{
  if (bytesized)
    {
      /* Got a byte shift, fake up second arg */
      d->srcb.type = eas.s.ea_imm.s.srcbword;
      d->srcb.literal = 8;
    }
  else
    {
      /* got a word shift, fake up second arg */
      d->srcb.type = eas.s.ea_imm.s.srcbword;
      d->srcb.literal = 16;
    }
}

/* Calculate the number of cycles required to run this
   instruction
*/
static void
compcycles (dst, opcode)
     decoded_inst *dst;
     h8500_opcode_info *opcode;
{
  int cycles = 0;
  /* Guess for the time being - 1 cycle for the first two bytes in the
     opcode - to fecth the operand, and 3 cycles for all the rest of
     the bytes, since they mean that there is probably an operand to
     fetch */

  switch (opcode->length)
    {
    case 1:
    case 2:
      cycles += opcode->length;
      break;
    default:
      cycles += opcode->length * 3;
      break;
    }

  dst->cycles = cycles;
}

static void
translate (ptr, from, to)
     ea_type *ptr;
     fastref from;
     fastref to;
{
  if (ptr->reg.wptr == &cpu.regs[7].s[LOW]
      && ptr->type == from)
    {
      ptr->type = to;
    }
}

static
void
fix_incdecs (dst)
     decoded_inst *dst;
{
  if (dst->dst.type == eas.s.ea_inc.s.dstbyte
      && (dst->srca.type == eas.s.ea_inc.s.srcabyte
	  || dst->srcb.type == eas.s.ea_inc.s.srcbbyte))
    {
      dst->dst.type = eas.s.ea_disp.s.dstbyte;
    }

  if (dst->dst.type == eas.s.ea_inc.s.dstword
      && (dst->srca.type == eas.s.ea_inc.s.srcaword
	  || dst->srcb.type == eas.s.ea_inc.s.srcbword))
    {
      dst->dst.type = eas.s.ea_disp.s.dstword;
    }

  if (dst->dst.type == eas.s.ea_dec.s.dstbyte
      || dst->dst.type == eas.s.ea_dec.s.dstword)
    {
      if (dst->srca.type == eas.s.ea_dec.s.srcabyte)
	{
	  dst->srca.type = eas.s.ea_disp.s.srcabyte;
	}
      else if (dst->srca.type == eas.s.ea_dec.s.srcaword)
	{
	  dst->srca.type = eas.s.ea_disp.s.srcaword;
	}
      else if (dst->srcb.type == eas.s.ea_dec.s.srcbbyte)
	{
	  dst->srcb.type = eas.s.ea_disp.s.srcbbyte;
	}
      else if (dst->srcb.type == eas.s.ea_dec.s.srcbword)
	{
	  dst->srcb.type = eas.s.ea_disp.s.srcbword;
	}
    }


  /* Turn a byte ops from the sp into word ops */
  translate (&dst->dst, eas.s.ea_dec.s.dstbyte, eas.s.ea_dec.s.dstword);
  translate (&dst->dst, eas.s.ea_inc.s.dstbyte, eas.s.ea_inc.s.dstword);

  translate (&dst->srca, eas.s.ea_dec.s.srcabyte, eas.s.ea_dec.s.srcaword);
  translate (&dst->srca, eas.s.ea_inc.s.srcabyte, eas.s.ea_inc.s.srcaword);

  translate (&dst->srcb, eas.s.ea_dec.s.srcbbyte, eas.s.ea_dec.s.srcbword);
  translate (&dst->srcb, eas.s.ea_inc.s.srcbbyte, eas.s.ea_inc.s.srcbword);


}


static void
find (pc, buffer, dst)
     int pc;
     unsigned char *buffer;
     decoded_inst *dst;
{
  h8500_opcode_info *opcode;
  int i;
  int idx;
  int hadimm = 0;
  dst->srca.reg.rptr = 0;

  /* Run down the table to find the one which matches */
  for (opcode = h8500_table; opcode->name; opcode++)
    {
      int byte;
      int rn;
      int rd;
      int rs;
      int disp;
      int abs;
      int imm;
      int pcrel;
      int qim;
      int i;
      int cr;


      dst->opcode = exec_dispatch[opcode->flavor & 0x7f];

      for (byte = 0; byte < opcode->length; byte++)
	{
	  if ((buffer[byte] & opcode->bytes[byte].mask)
	      != (opcode->bytes[byte].contents))
	    {
	      goto next;
	    }
	  else
	    {
	      /* extract any info parts */
	      switch (opcode->bytes[byte].insert)
		{
		case 0:
		case FP:
		  break;
		default:
		  abort ();
		  break;
		case RN:
		  rn = buffer[byte] & 0x7;
		  break;
		case RS:
		  rs = buffer[byte] & 0x7;
		  break;
		case CRB:
		  cr = buffer[byte] & 0x7;
		  if (cr == 0)
		    goto next;
		  break;
		case CRW:
		  cr = buffer[byte] & 0x7;
		  if (cr != 0)
		    goto next;
		  break;
		case DISP16:
		  disp = (buffer[byte] << 8) | (buffer[byte + 1]);
		  break;
		case FPIND_D8:
		case DISP8:
		  disp = ((char) (buffer[byte]));
		  break;
		case RD:
		case RDIND:
		  rd = buffer[byte] & 0x7;
		  break;
		case ABS24:
		  abs =
		    (buffer[byte] << 16)
		      | (buffer[byte + 1] << 8)
			| (buffer[byte + 2]);
		  break;
		case ABS16:
		  abs = (buffer[byte] << 8) | (buffer[byte + 1]);
		  break;
		case ABS8:
		  abs = (buffer[byte]);
		  break;
		case IMM16:
		  imm = (buffer[byte] << 8) | (buffer[byte + 1]);
		  break;
		case IMM4:
		  imm = (buffer[byte]) & 0xf;
		  break;
		case IMM8:
		case RLIST:
		  imm = SEXTCHAR (buffer[byte]);
		  break;
		case PCREL16:
		  pcrel = SEXTSHORT ((buffer[byte] << 8) | (buffer[byte + 1]));
		  break;
		case PCREL8:
		  pcrel = SEXTCHAR ((buffer[byte]));
		  break;
		case QIM:
		  switch (buffer[byte] & 0x7)
		    {
		    case 0:
		      imm = 1;
		      break;
		    case 1:
		      imm = 2;
		      break;
		    case 4:
		      imm = -1;
		      break;
		    case 5:
		      imm = -2;
		      break;
		    }
		  break;

		}
	    }
	}
      if (opcode->flavor & O_BYTE)
	{
	  idx = 0;
	  switch (opcode->flags)
	    {
	    case 'h':
	      dst->flags = flag_shiftbyte;
	      break;
	    case 'p':
	      dst->flags = flag_multbyte;
	      break;
	    case 'B':
	      dst->flags = flag_branch;
	      break;
	    case 'm':
	      dst->flags = flag_mp;
	      break;
	    case 'a':
	      dst->flags = flag_ap;
	      break;
	    case '-':
	      dst->flags = flag_nonep;
	      break;
	    case 0:
	      dst->flags = flag_nostorep;
	      break;
	    case 'c':
	      dst->flags = flag_clearp;
	      break;
	    case 's':
	      /* special */
	      dst->flags = flag_special;
	    }
	}
      else
	{
	  idx = 1;
	  switch (opcode->flags)
	    {
	    case 'h':
	      dst->flags = flag_shiftword;
	      break;
	    case 'p':
	      dst->flags = flag_multword;
	      break;
	    case 'B':
	      dst->flags = flag_branch;
	      break;
	    case 'm':
	      dst->flags = flag_Mp;
	      break;
	    case 'a':
	      dst->flags = flag_Ap;
	      break;
	    case '-':
	      dst->flags = flag_nonep;
	      break;
	    case 0:
	      dst->flags = flag_nostorep;
	      break;
	    case 'c':
	      dst->flags = flag_clearp;
	      break;
	    case 's':
	      /* special */
	      dst->flags = flag_special;
	      break;
	    }
	}

      for (i = 0; i < opcode->nargs; i++)
	{
	  ea_type *p = eavector + i;

	  switch (opcode->arg_type[i])
	    {
	    default:
	      abort ();

	    case FP:
	      gotreg (p, 6, idx);
	      break;
	    case RNIND:
	      disp = 0;
	    case RNIND_D16:
	    case RNIND_D8:
	      gotind (p, disp, rn, idx);
	      break;
	      break;
	    case RDIND:
	      disp = 0;
	    case RDIND_D16:
	    case RDIND_D8:
	      gotind (p, disp, rd, idx);
	      break;
	    case FPIND_D8:
	      gotind (p, disp, 6, idx);
	      break;
	    case CRB:
	    case CRW:
	      gotcr (p, cr);
	      break;
	    case RN:
	      gotreg (p, rn, idx);
	      break;
	    case RD:
	      gotreg (p, rd, idx);
	      break;
	    case RS:
	      gotreg (p, rs, idx);
	      break;
	    case RNDEC:
	      gotinc (p, rn, -1, idx);
	      break;
	    case RNINC:
	      gotinc (p, rn, 1, idx);
	      break;
	    case SPINC:
	      gotinc (p, 7, 1, idx);
	      break;
	    case SPDEC:
	      gotinc (p, 7, -1, idx);
	      break;
	    case ABS24:
	    case ABS16:
	      gotabs (p, abs, R_HARD_0, idx);
	      break;
	    case ABS8:
	      gotabs (p, abs, R_HARD8_0, idx);
	      break;
	    case IMM16:
	    case RLIST:
	    case QIM:
	    case IMM4:
	    case IMM8:
	      gotimm (p, imm);
	      break;
	    case PCREL16:
	    case PCREL8:
	      gotimm (p,
		      ((pcrel + pc + opcode->length) & 0xffff) | (pc & 0xff0000),
		      R_HARD_0, JLONG);

	    }
	}

      /* Finished and done - turn from two operand stuff into three */

      dst->srca.type = eas.s.ea_nop.s.srcabyte;
      dst->srcb.type = eas.s.ea_nop.s.srcbbyte;
      dst->dst.type = eas.s.ea_nop.s.dstbyte;

      if (opcode->nargs)
	{
	  switch (opcode->nargs)
	    {
	    case 1:
	      howto_workout (&dst->srca, &eavector[0], 0);
	      if (opcode->dst != '!')
		howto_workout (&dst->dst, &eavector[0], 2);
	      break;
	    case 2:
	      if (opcode->src2 == '!')
		{
		  howto_workout (&dst->srca, &eavector[0], 0);
		  howto_workout (&dst->dst, &eavector[1], 2);
		}
	      else
		{
		  howto_workout (&dst->srca, &eavector[0], 0);
		  howto_workout (&dst->srcb, &eavector[1], 1);
		  if (opcode->dst != '!')
		    {
		      howto_workout (&dst->dst, &eavector[1], 2);
		    }
		}
	      break;
	    }



	  /* Some extra stuff with pre inc and post dec,
	     make sure that if the same ea is there twice, only one of the
	     ops is auto inc/dec */

	  fix_incdecs (dst);


	  /* Some special cases */
	  if (dst->opcode == exec_dispatch[O_PJSR]
	      || dst->opcode == exec_dispatch[O_PJMP])
	    {
	      /* Both the @abs:24 and @rn turn into a disp word,
		 chose the right a mode since  @abs:24 is 4 bytes
		 long */

	      if (opcode->length == 4)
		{
		  dst->srca.type = eas.s.ea_lval24.s.srcabyte;
		}
	      else
		{
		  dst->srca.type = eas.s.ea_reg.s.srcalong;
		}

	      dst->srca.r2.rptr = &cpu.regs[R_HARD_0];

	      /* For [P]JSR, keep return address precomputed */
	      dst->srcb.literal = pc + opcode->length;
	      dst->srcb.type = eas.s.ea_imm.s.srcbword;
	    }
	  else if (dst->opcode == exec_dispatch[O_MULXU])
	    {
	      /* This is a multiply -fix the destination op */
	      if (dst->dst.type == eas.s.ea_reg.s.dstword)
		{
		  dst->dst.type = eas.s.ea_reg.s.dstlong;
		}
	      else
		{
		  dst->dst.type = eas.s.ea_reg.s.dstword;
		}
	      dst->dst.reg.bptr = regptr[rd][JWORD];
	    }
	  else if (dst->opcode == exec_dispatch[O_DIVXU])
	    {
	      /* This is a wider than normal, fix the source operand */
	      dst->srcb.type
		= (dst->srcb.type == eas.s.ea_reg.s.srcbword)
		  ? eas.s.ea_reg.s.srcblong
		    : eas.s.ea_reg.s.srcbword;

	      dst->dst.type
		= (dst->dst.type == eas.s.ea_reg.s.dstword)
		  ? eas.s.ea_reg.s.dstlong
		    : eas.s.ea_reg.s.dstword;

	    }

	  else if (dst->opcode == exec_dispatch[O_LDM])
	    {
	      /* Turn of the stack ref */
	      dst->srca.type = eas.s.ea_nop.s.srcabyte;
	    }
	  else if (dst->opcode == exec_dispatch[O_STM])
	    {
	      /* Turn of the stack ref */
	      dst->srcb.type = eas.s.ea_nop.s.srcbbyte;
	    }


	  /* extends read one size and write another */
	  else if (dst->opcode == exec_dispatch[O_EXTS]
		   || dst->opcode == exec_dispatch[O_EXTU])
	    {
	      dst->dst.type = eas.s.ea_reg.s.dstword;
	      dst->dst.reg.bptr = regptr[rd][JWORD];
	      dst->flags = flag_Ap;
	    }


	  if (opcode->flags == 'h')
	    thinkabout_shifts (dst, opcode->flavor & O_BYTE);


	  /* For a branch, turn off one level of indirection */
	  if (opcode->src1 == 'B')
	    {
	      indoff (&dst->srca, 0);
	    }

	}
      dst->next_pc = pc + opcode->length;

      compcycles (dst, opcode);

      return;


    next:;
    }

  /* Couldn't understand anything */
  dst->opcode = exec_dispatch[O_TRAPA];
  dst->next_pc = pc + 1;

}

compile (pc)
{
  int idx;

  /* find the next cache entry to use */

  idx = cpu.cache_top + 1;
  cpu.compiles++;
  if (idx >= cpu.csize)
    {
      idx = 1;
    }
  cpu.cache_top = idx;

  /* Throw away its old meaning */
  cpu.cache_idx[cpu.cache[idx].oldpc] = 0;

  /* set to new address */
  cpu.cache[idx].oldpc = pc;

  /* fill in instruction info */
  find (pc, cpu.memory + pc, cpu.cache + idx);

  /* point to new cache entry */
  cpu.cache_idx[pc] = idx;
}

baddefault (x)
{
  printf ("bad default %d\n", x);
}

static int fetch_l (arg)
     ea_type *arg;
{
  int l, r;

  int h = *(arg->reg.wptr);
  r = (union rtype *) (arg->reg.wptr) - &cpu.regs[0];
  r++;

  l = cpu.regs[r].s[LOW];
  return (h << 16) | l;

}

#define FETCH(dst, arg, n)  \
{ \
 int r; unsigned char*lval; \
 DISPATCH((arg).type) \
 { LABELN(FETCH_NOP,n): \
 dst= 0; \
 break; \
 DEFAULT baddefault((arg).type); break; \
 LABELN(FETCH_LVAL,n):  \
 dst = (*(((arg).reg.wptr)) + (arg.literal)) ; \
 break; \
 LABELN(FETCH_LVAL24,n):  \
 dst = (*(((arg).reg.wptr)) + *(((arg).r2.wptr)) + (arg.literal)) &0xffffff; \
 break; \
 LABELN(FETCH_CRB,n):  \
 dst = (*((arg).reg.segptr) - cpu.memory)>>16; \
 break; \
 LABELN(FETCH_CRW,n):  \
  dst = BUILDSR();\
 break; \
 LABELN(FETCH_REG_B,n): \
 dst = *((arg).reg.bptr); \
 break; \
 LABELN(FETCH_REG_W,n): \
 dst = *((arg).reg.wptr); \
 break; \
 LABELN(FETCH_REG_L,n): \
 dst = fetch_l(&(arg));\
 break; \
 LABELN(FETCH_INC_B,n): \
 lval = elval ((arg), 0); \
 dst = byteat (lval); \
 (*((arg).reg.wptr))++; \
 break; \
 LABELN(FETCH_INC_W,n): \
 lval = elval ((arg), 0); \
 dst = wordat (lval); \
 (*(((arg).reg.wptr))) += 2; \
 break; \
 LABELN(FETCH_DEC_B, n): \
 (*(arg).reg.wptr)--; \
 lval = elval ((arg), 0); \
 r = byteat (lval); \
 dst = r; \
 break; \
 LABELN(FETCH_DEC_W, n): \
 (*((arg).reg.wptr)) -= 2; \
 lval = elval ((arg), 0); \
 r = wordat (lval); \
 dst = r; \
 break; \
 LABELN(FETCH_DISP_B,n): \
 lval = displval ((arg)); \
 dst = byteat (lval); \
 break; \
 LABELN(FETCH_DISP_W,n): \
 lval = displval ((arg)); \
 dst = wordat (lval); \
 break; \
 LABELN(FETCH_IMM, n): \
 dst = (arg).literal; \
 break; \
 } \
 ENDDISPATCH; \
}

static union
{
  short int i;
  struct
    {
      char low;
      char high;
    }
  u;
}

littleendian;

static
void
init_pointers ()
{
  static int init;

  if (!init)
    {
      int i;

      init = 1;
      littleendian.i = 1;

      for (i = 0; i < (int) R_LAST; i++)
	{
	  if (littleendian.u.high)
	    {
	      /* big endian host */


	      LOW = 1;
	      HIGH = 0;

	      regptr[i][0] = ((unsigned char *) (cpu.regs + i)) + 3;
	      regptr[i][1] = ((unsigned char *) (cpu.regs + i)) + 2;
	    }
	  else
	    {
	      LOW = 0;
	      HIGH = 1;

	      regptr[i][0] = (unsigned char *) &(cpu.regs[i]);
	      regptr[i][1] = (unsigned char *) (&(cpu.regs[i]));
	    }

	  regptr[i][2] = (unsigned char *) &(cpu.regs[i]);
	}

      memcpy (segregptr + 0, regptr + R_SR, sizeof (segregptr[0]));
      memcpy (segregptr + 1, regptr + R_TP, sizeof (segregptr[1]));
      memcpy (segregptr + 3, regptr + R_BR, sizeof (segregptr[3]));
      memcpy (segregptr + 4, regptr + R_EP, sizeof (segregptr[4]));
      memcpy (segregptr + 5, regptr + R_DP, sizeof (segregptr[5]));
      memcpy (segregptr + 6, regptr + R_CP, sizeof (segregptr[6]));
      memcpy (segregptr + 7, regptr + R_TP, sizeof (segregptr[7]));

      /* Pointers to into the cpu state for the seg registers */

      segmap[R0] = &cpu.regs[R_DP].c;
      segmap[R1] = &cpu.regs[R_DP].c;
      segmap[R2] = &cpu.regs[R_DP].c;
      segmap[R3] = &cpu.regs[R_DP].c;
      segmap[R4] = &cpu.regs[R_EP].c;
      segmap[R5] = &cpu.regs[R_EP].c;
      segmap[R6] = &cpu.regs[R_TP].c;
      segmap[R7] = &cpu.regs[R_TP].c;
      segmap[R_HARD_0] = &cpu.regs[R_DP].c;
      segmap[R_HARD8_0] = &cpu.regs[R_BP].c;

      cpu.memory = (unsigned char *) calloc (sizeof (char), MSIZE);
      cpu.cache_idx = (unsigned short *) calloc (sizeof (short), MSIZE);

      /* initialize the seg registers */

      cpu.regs[R_DP].c = cpu.memory;
      cpu.regs[R_TP].c = cpu.memory;
      cpu.regs[R_CP].c = cpu.memory;
      cpu.regs[R_BP].c = cpu.memory;
      cpu.regs[R_EP].c = cpu.memory;
      cpu.regs[R7].s[LOW] = 0xfffe;
      cpu.regs[R6].s[LOW] = 0xfffe;
      if (!cpu.cache)
	sim_csize (CSIZE);
    }
}

#define PUSHWORD(x)				\
{						\
  int sp = cpu.regs[R7].s[LOW];			\
  unsigned char *p;				\
						\
  sp -= 2;					\
  p = (sp & 0xffff) + (cpu.regs[R_TP].c);	\
  cpu.regs[R7].s[LOW] = sp;			\
  setwordat (p, x);				\
}						\

#define POPWORD(d)				\
{						\
  int spx= cpu.regs[R7].s[LOW];			\
  unsigned char *p;				\
	        				\
  p = (spx& 0xffff) + (cpu.regs[R_TP].c);	\
  spx+= 2;					\
  cpu.regs[R7].s[LOW] = spx;                    \
  d = wordat (p);				\
}						\

/* simulate a monitor trap */
trap ()
{
  switch (cpu.regs[R3].s[LOW] & 0xff)
    {
    case 33:
      /* exit */
      cpu.exception = SIGQUIT;
      break;
    case 34:
      /* abort */
      cpu.exception = SIGABRT;
      break;
    case 6:
      /* print char in r0 */
      printf ("%c", cpu.regs[R0].s[LOW]);
      break;
    }
}
void
control_c (sig, code, scp, addr)
     int sig;
     int code;
     char *scp;
     char *addr;
{
  cpu.exception = SIGINT;
}

static jmp_buf jbuf;
static void
segv ()
{
  cpu.exception = SIGSEGV;
  longjmp (jbuf, 1);
}

void
sim_resume (step, siggnal)
{
  static int init1;
  int res;
  int tmp;
  int arga;
  int argb;
  int bit;
  int pc;
  int C, Z, V, N;
  int cycles = 0;
  int insts = 0;
  int tick_start = get_now ();
  void (*prev) ();
  void (*prev_seg) ();

  if (!init1)
    {
      int i;

      init1 = 1;
      init_pointers ();

      for (i = 0; i < N_EATYPES; i++)
	{
	  eas.a[i].s.srcabyte = LABEL_REFN (FETCH_NOP, 0);
	  eas.a[i].s.srcaword = LABEL_REFN (FETCH_NOP, 0);
	  eas.a[i].s.srcalong = LABEL_REFN (FETCH_NOP, 0);

	  eas.a[i].s.srcbbyte = LABEL_REFN (FETCH_NOP, 1);
	  eas.a[i].s.srcbword = LABEL_REFN (FETCH_NOP, 1);
	  eas.a[i].s.srcblong = LABEL_REFN (FETCH_NOP, 1);

	  eas.a[i].s.dstbyte = LABEL_REF (STORE_NOP);
	  eas.a[i].s.dstword = LABEL_REF (STORE_NOP);
	  eas.a[i].s.dstlong = LABEL_REF (STORE_NOP);
	}

      eas.s.ea_lval.s.srcabyte = LABEL_REFN (FETCH_LVAL, 0);
      eas.s.ea_lval.s.srcaword = LABEL_REFN (FETCH_LVAL, 0);
      eas.s.ea_lval24.s.srcabyte = LABEL_REFN (FETCH_LVAL24, 0);
      eas.s.ea_lval24.s.srcaword = LABEL_REFN (FETCH_LVAL24, 0);

      eas.s.ea_nop.s.srcabyte = LABEL_REFN (FETCH_NOP, 0);
      eas.s.ea_nop.s.srcaword = LABEL_REFN (FETCH_NOP, 0);
      eas.s.ea_nop.s.srcbbyte = LABEL_REFN (FETCH_NOP, 1);
      eas.s.ea_nop.s.srcbword = LABEL_REFN (FETCH_NOP, 1);
      eas.s.ea_nop.s.dstbyte = LABEL_REF (STORE_NOP);
      eas.s.ea_nop.s.dstword = LABEL_REF (STORE_NOP);

      eas.s.ea_cr.s.srcabyte = LABEL_REFN (FETCH_CRB, 0);
      eas.s.ea_cr.s.srcaword = LABEL_REFN (FETCH_CRW, 0);

      eas.s.ea_cr.s.srcbbyte = LABEL_REFN (FETCH_CRB, 1);
      eas.s.ea_cr.s.srcbword = LABEL_REFN (FETCH_CRW, 1);

      eas.s.ea_cr.s.dstbyte = LABEL_REF (STORE_CRB);
      eas.s.ea_cr.s.dstword = LABEL_REF (STORE_CRW);

      eas.s.ea_reg.s.srcabyte = LABEL_REFN (FETCH_REG_B, 0);
      eas.s.ea_reg.s.srcaword = LABEL_REFN (FETCH_REG_W, 0);
      eas.s.ea_reg.s.srcalong = LABEL_REFN (FETCH_REG_L, 0);

      eas.s.ea_reg.s.srcbbyte = LABEL_REFN (FETCH_REG_B, 1);
      eas.s.ea_reg.s.srcbword = LABEL_REFN (FETCH_REG_W, 1);
      eas.s.ea_reg.s.srcblong = LABEL_REFN (FETCH_REG_L, 1);

      eas.s.ea_reg.s.dstbyte = LABEL_REF (STORE_REG_B);
      eas.s.ea_reg.s.dstword = LABEL_REF (STORE_REG_W);
      eas.s.ea_reg.s.dstlong = LABEL_REF (STORE_REG_L);

      eas.s.ea_inc.s.srcabyte = LABEL_REFN (FETCH_INC_B, 0);
      eas.s.ea_inc.s.srcaword = LABEL_REFN (FETCH_INC_W, 0);
      eas.s.ea_inc.s.srcbbyte = LABEL_REFN (FETCH_INC_B, 1);
      eas.s.ea_inc.s.srcbword = LABEL_REFN (FETCH_INC_W, 1);
      eas.s.ea_inc.s.dstbyte = LABEL_REF (STORE_INC_B);
      eas.s.ea_inc.s.dstword = LABEL_REF (STORE_INC_W);

      eas.s.ea_dec.s.srcabyte = LABEL_REFN (FETCH_DEC_B, 0);
      eas.s.ea_dec.s.srcaword = LABEL_REFN (FETCH_DEC_W, 0);
      eas.s.ea_dec.s.srcbbyte = LABEL_REFN (FETCH_DEC_B, 1);
      eas.s.ea_dec.s.srcbword = LABEL_REFN (FETCH_DEC_W, 1);
      eas.s.ea_dec.s.dstbyte = LABEL_REF (STORE_DEC_B);
      eas.s.ea_dec.s.dstword = LABEL_REF (STORE_DEC_W);

      eas.s.ea_disp.s.srcabyte = LABEL_REFN (FETCH_DISP_B, 0);
      eas.s.ea_disp.s.srcaword = LABEL_REFN (FETCH_DISP_W, 0);
      eas.s.ea_disp.s.srcbbyte = LABEL_REFN (FETCH_DISP_B, 1);
      eas.s.ea_disp.s.srcbword = LABEL_REFN (FETCH_DISP_W, 1);
      eas.s.ea_disp.s.dstbyte = LABEL_REF (STORE_DISP_B);
      eas.s.ea_disp.s.dstword = LABEL_REF (STORE_DISP_W);

      eas.s.ea_imm.s.srcabyte = LABEL_REFN (FETCH_IMM, 0);
      eas.s.ea_imm.s.srcaword = LABEL_REFN (FETCH_IMM, 0);
      eas.s.ea_imm.s.srcbbyte = LABEL_REFN (FETCH_IMM, 1);
      eas.s.ea_imm.s.srcbword = LABEL_REFN (FETCH_IMM, 1);

      flag_special = LABEL_REF (FLAG_special);
      flag_mp = LABEL_REF (FLAG_m);
      flag_Mp = LABEL_REF (FLAG_M);
      flag_ap = LABEL_REF (FLAG_a);
      flag_Ap = LABEL_REF (FLAG_A);
      flag_nonep = LABEL_REF (FLAG_NONE);
      flag_nostorep = LABEL_REF (FLAG_NOSTORE);
      flag_clearp = LABEL_REF (FLAG_CLEAR);
      flag_shiftbyte = LABEL_REF (FLAG_shiftbyte);
      flag_shiftword = LABEL_REF (FLAG_shiftword);
      flag_multbyte = LABEL_REF (FLAG_multbyte);
      flag_multword = LABEL_REF (FLAG_multword);


      exec_dispatch[O_ADDS] = LABEL_REF (O_ADDS);
      exec_dispatch[O_ADDX] = LABEL_REF (O_ADDX);
      exec_dispatch[O_ADD] = LABEL_REF (O_ADD);
      exec_dispatch[O_ANDC] = LABEL_REF (O_ANDC);
      exec_dispatch[O_AND] = LABEL_REF (O_AND);
      exec_dispatch[O_BCC] = LABEL_REF (O_BCC);
      exec_dispatch[O_BCLR] = LABEL_REF (O_BCLR);
      exec_dispatch[O_BCS] = LABEL_REF (O_BCS);
      exec_dispatch[O_BEQ] = LABEL_REF (O_BEQ);
      exec_dispatch[O_BF] = LABEL_REF (O_BF);
      exec_dispatch[O_BGE] = LABEL_REF (O_BGE);
      exec_dispatch[O_BGT] = LABEL_REF (O_BGT);
      exec_dispatch[O_BHI] = LABEL_REF (O_BHI);
      exec_dispatch[O_BHS] = LABEL_REF (O_BHS);
      exec_dispatch[O_BLE] = LABEL_REF (O_BLE);
      exec_dispatch[O_BLO] = LABEL_REF (O_BLO);
      exec_dispatch[O_BLS] = LABEL_REF (O_BLS);
      exec_dispatch[O_BLT] = LABEL_REF (O_BLT);
      exec_dispatch[O_BMI] = LABEL_REF (O_BMI);
      exec_dispatch[O_BNE] = LABEL_REF (O_BNE);
      exec_dispatch[O_BNOT] = LABEL_REF (O_BNOT);
      exec_dispatch[O_BPL] = LABEL_REF (O_BPL);
      exec_dispatch[O_BPT] = LABEL_REF (O_BPT);
      exec_dispatch[O_BRA] = LABEL_REF (O_BRA);
      exec_dispatch[O_BRN] = LABEL_REF (O_BRN);
      exec_dispatch[O_BSET] = LABEL_REF (O_BSET);
      exec_dispatch[O_BSR] = LABEL_REF (O_BSR);
      exec_dispatch[O_BTST] = LABEL_REF (O_BTST);
      exec_dispatch[O_BT] = LABEL_REF (O_BT);
      exec_dispatch[O_BVC] = LABEL_REF (O_BVC);
      exec_dispatch[O_BVS] = LABEL_REF (O_BVS);
      exec_dispatch[O_CLR] = LABEL_REF (O_CLR);
      exec_dispatch[O_CMP] = LABEL_REF (O_CMP);
      exec_dispatch[O_DADD] = LABEL_REF (O_DADD);
      exec_dispatch[O_DIVXU] = LABEL_REF (O_DIVXU);
      exec_dispatch[O_DSUB] = LABEL_REF (O_DSUB);
      exec_dispatch[O_EXTS] = LABEL_REF (O_EXTS);
      exec_dispatch[O_EXTU] = LABEL_REF (O_EXTU);
      exec_dispatch[O_JMP] = LABEL_REF (O_JMP);
      exec_dispatch[O_JSR] = LABEL_REF (O_JSR);
      exec_dispatch[O_LDC] = LABEL_REF (O_LDC);
      exec_dispatch[O_LDM] = LABEL_REF (O_LDM);
      exec_dispatch[O_LINK] = LABEL_REF (O_LINK);
      exec_dispatch[O_MOVFPE] = LABEL_REF (O_MOVFPE);
      exec_dispatch[O_MOVTPE] = LABEL_REF (O_MOVTPE);
      exec_dispatch[O_MOV] = LABEL_REF (O_MOV);
      exec_dispatch[O_MULXU] = LABEL_REF (O_MULXU);
      exec_dispatch[O_NEG] = LABEL_REF (O_NEG);
      exec_dispatch[O_NOP] = LABEL_REF (O_NOP);
      exec_dispatch[O_NOT] = LABEL_REF (O_NOT);
      exec_dispatch[O_ORC] = LABEL_REF (O_ORC);
      exec_dispatch[O_OR] = LABEL_REF (O_OR);
      exec_dispatch[O_PJMP] = LABEL_REF (O_PJMP);
      exec_dispatch[O_PJSR] = LABEL_REF (O_PJSR);
      exec_dispatch[O_PRTD] = LABEL_REF (O_PRTD);
      exec_dispatch[O_PRTS] = LABEL_REF (O_PRTS);
      exec_dispatch[O_RECOMPILE] = LABEL_REF (O_RECOMPILE);

      exec_dispatch[O_ROTL] = LABEL_REF (O_ROTL);
      exec_dispatch[O_ROTR] = LABEL_REF (O_ROTR);
      exec_dispatch[O_ROTXL] = LABEL_REF (O_ROTXL);
      exec_dispatch[O_ROTXR] = LABEL_REF (O_ROTXR);

      exec_dispatch[O_RTD] = LABEL_REF (O_RTD);
      exec_dispatch[O_RTS] = LABEL_REF (O_RTS);
      exec_dispatch[O_SCB_EQ] = LABEL_REF (O_SCB_EQ);
      exec_dispatch[O_SCB_F] = LABEL_REF (O_SCB_F);
      exec_dispatch[O_SCB_NE] = LABEL_REF (O_SCB_NE);
      exec_dispatch[O_SHAL] = LABEL_REF (O_SHAL);
      exec_dispatch[O_SHAR] = LABEL_REF (O_SHAR);
      exec_dispatch[O_SHLL] = LABEL_REF (O_SHLL);
      exec_dispatch[O_SHLR] = LABEL_REF (O_SHLR);

      exec_dispatch[O_SLEEP] = LABEL_REF (O_SLEEP);
      exec_dispatch[O_STC] = LABEL_REF (O_STC);
      exec_dispatch[O_STM] = LABEL_REF (O_STM);
      exec_dispatch[O_SUBS] = LABEL_REF (O_SUBS);
      exec_dispatch[O_SUBX] = LABEL_REF (O_SUBX);
      exec_dispatch[O_SUB] = LABEL_REF (O_SUB);
      exec_dispatch[O_SWAP] = LABEL_REF (O_SWAP);
      exec_dispatch[O_TAS] = LABEL_REF (O_TAS);
      exec_dispatch[O_TRAPA] = LABEL_REF (O_TRAPA);
      exec_dispatch[O_TRAP_VS] = LABEL_REF (O_TRAP_VS);
      exec_dispatch[O_TST] = LABEL_REF (O_TST);
      exec_dispatch[O_UNLK] = LABEL_REF (O_UNLK);
      exec_dispatch[O_XCH] = LABEL_REF (O_XCH);
      exec_dispatch[O_XORC] = LABEL_REF (O_XORC);
      exec_dispatch[O_XOR] = LABEL_REF (O_XOR);
      nop.type = eas.s.ea_nop.s.srcabyte;
      cpu.cache[0].opcode = exec_dispatch[O_RECOMPILE];
      cpu.cache[0].srca.type = eas.s.ea_nop.s.srcabyte;
      cpu.cache[0].srcb.type = eas.s.ea_nop.s.srcbbyte;
    }

  prev = signal (SIGINT, control_c);
  prev_seg = signal (SIGSEGV, segv);

  if (step)
    {
      cpu.exception = SIGTRAP;
    }
  else
    {
      cpu.exception = 0;
    }

  pc = cpu.regs[R_PC].s[LOW] + (NORMAL_CP << 16);

  GETSR ();

  if (setjmp (jbuf) == 0) {
    do
      {
	int cidx;
	decoded_inst *code;

      top:
	cidx = cpu.cache_idx[pc];
	code = cpu.cache + cidx;

	FETCH (arga, code->srca, 0);
	FETCH (argb, code->srcb, 1);


	
#ifdef DEBUG
	if (debug)
	  {
	    printf ("%x %d %s\n", pc, code->opcode,
		    code->op ? code->op->name : "**");
	  }
#endif

	cycles += code->cycles;
	insts++;
	DISPATCH (code->opcode)
	  {
	    LABEL (O_RECOMPILE):
	    /* This opcode is a fake for when we get to an instruction which
	       hasn't been compiled */
	    compile (pc);
	    goto top;
	    break;
	    LABEL (O_NEG):
	    arga = -arga;
	    argb = 0;
	    res = arga + argb;
	    break;
	    LABEL (O_SUBX):
	    arga += C;
	    LABEL (O_SUB):
	    LABEL (O_SUBS):
	    arga = -arga;
	    LABEL (O_ADD):
	    LABEL (O_ADDS):
	    res = arga + argb;
	    break;

	    LABEL (O_ADDX):
	    res = arga + argb + C;
	    break;

	    LABEL (O_AND):
	    LABEL (O_ANDC):
	    res = arga & argb;
	    break;
	    break;

	    LABEL (O_BCLR):
	    arga &= 0xf;
	    bit = (argb & (1 << arga));
	    res = argb & ~(1 << arga);
	    goto bitop;


	    LABEL (O_BRA):
	    LABEL (O_BT):
	    if (1)
	      goto condtrue;

	    LABEL (O_BRN):
	    LABEL (O_BF):
	    if (0)
	      goto condtrue;
	    break;

	    LABEL (O_BHI):
	    if ((C || Z) == 0)
	      goto condtrue;
	    break;

	    LABEL (O_BLS):
	    if ((C || Z))
	      goto condtrue;
	    break;

	    LABEL (O_BCS):
	    LABEL (O_BLO):
	    if ((C == 1))
	      goto condtrue;
	    break;

	    LABEL (O_BCC):
	    LABEL (O_BHS):
	    if ((C == 0))
	      goto condtrue;
	    break;

	    LABEL (O_BEQ):
	    if (Z)
	      goto condtrue;
	    break;
	    LABEL (O_BGT):
	    if (((Z || (N ^ V)) == 0))
	      goto condtrue;
	    break;


	    LABEL (O_BLE):
	    if (((Z || (N ^ V)) == 1))
	      goto condtrue;
	    break;

	    LABEL (O_BGE):
	    if ((N ^ V) == 0)
	      goto condtrue;
	    break;
	    LABEL (O_BLT):
	    if ((N ^ V))
	      goto condtrue;
	    break;
	    LABEL (O_BMI):
	    if ((N))
	      goto condtrue;
	    break;
	    LABEL (O_BNE):
	    if ((Z == 0))
	      goto condtrue;
	    break;
	    LABEL (O_BPL):
	    if (N == 0)
	      goto condtrue;
	    break;
	    break;
	    LABEL (O_BVC):
	    if ((V == 0))
	      goto condtrue;
	    break;
	    LABEL (O_BVS):
	    if ((V == 1))
	      goto condtrue;
	    break;

	    LABEL (O_BNOT):
	    bit = argb & (1<<(arga & 0xf));
	    res = argb ^ (1<<(arga & 0xf));
	    goto bitop;
	    break;

	    LABEL (O_BSET):
	    arga = 1 << (arga & 0xf);
	    bit = argb & arga;
	    res = argb | arga;
	    goto bitop;
	    break;

	    LABEL (O_PJMP):
	    pc = arga;
	    goto next;

	    LABEL (O_UNLK):
	    {
	      int t;
	      SET_NORMREG (R7, GET_NORMREG (R6));
	      POPWORD (t);
	      SET_NORMREG (R6, t);
	      pc = code->next_pc;
	      goto next;
	    }

	    LABEL (O_RTS):
	    {
	      int cp = pc & 0xff0000;
	      POPWORD (pc);
	      pc |= cp;
	      goto next;
	    }
	    break;

	    LABEL (O_PRTS):
	    {
	      int cp;
	      int off;
	      POPWORD (cp);
	      POPWORD (off);
	      cp <<= 16;
	      SET_SEGREG (R_CP, cp);
	      pc = cp + off;
	    }
	    goto next;

	    LABEL (O_PJSR):
	    PUSHWORD (argb & 0xffff);
	    PUSHWORD (argb >> 16);
	    pc = (arga & 0xffffff);
	    goto next;

	    LABEL (O_BSR):
	    LABEL (O_JSR):
	    PUSHWORD (code->next_pc);
	    pc = arga | (pc & 0xff0000);
	    goto next;

	    LABEL (O_BTST):
	    Z = (((argb >> (arga & 0xf)) & 1) == 0);
	    pc = code->next_pc;
	    goto next;

	    LABEL (O_CLR):
	    res = 0;
	    break;

	    LABEL (O_CMP):
	    arga = -arga;
	    res = arga + argb;
	    break;

	    LABEL (O_DADD):
	    res = arga + argb + C;
	    if (res > 99)
	      {
		res -= 100;
		C = 1;
	      }
	    else
	      {
		C = 0;
	      }
	    Z = Z && (res == 0);
	    break;


	    LABEL (O_DSUB):
	    res = argb - arga - C;
	    if (res < 0)
	      {
		res += 100;
		C = 1;
	      }
	    else
	      {
		C = 0;
	      }
	    Z = Z && (res == 0);
	    break;

	    LABEL (O_EXTS):
	    res = SEXTCHAR (arga);
	    break;

	    LABEL (O_EXTU):
	    res = (unsigned char) arga;
	    break;

	    LABEL (O_JMP):
	    pc = arga | (pc & 0xff0000);
	    goto next;
	    break;

	    LABEL (O_LDM):

	    for (tmp = 0; tmp < 7; tmp++)
	      {
		if (argb & (1 << tmp))
		  {
		    POPWORD (cpu.regs[tmp].s[LOW]);
		  }
	      }
	    if (argb & 0x80)
	      POPWORD (tmp);	/* dummy ready for sp */
	    goto nextpc;
	    break;

	    LABEL (O_LINK):
	    PUSHWORD (cpu.regs[R6].s[LOW]);
	    cpu.regs[R6].s[LOW] = cpu.regs[R7].s[LOW];
	    cpu.regs[R7].s[LOW] += argb;
	    goto nextpc;

	    LABEL (O_STC):
	    LABEL (O_LDC):
	    LABEL (O_MOVFPE):
	    LABEL (O_MOVTPE):
	    LABEL (O_MOV):
	    LABEL (O_TST):
	    res = arga;
	    break;

	    LABEL (O_TRAPA):
	    if (arga == 15)
	      {
		trap ();
	      }
	    else
	      {
		PUSHWORD (pc & 0xffff);
		if (cpu.maximum)
		  {
		    PUSHWORD (NORMAL_CP);
		  }
		PUSHWORD (NORMAL_SR);
		if (cpu.maximum)
		  {
		    arga = arga * 4 + 0x40;
		    SET_NORMAL_CPPC (longat (cpu.memory + arga));
		  }
		else
		  {
		    arga = arga * 2 + 0x20;
		    SET_NORMAL_CPPC (wordat (cpu.memory + arga));
		  }
	      }
	    break;

	    LABEL (O_OR):
	    LABEL (O_ORC):
	    res = arga | argb;
	    break;

	    LABEL (O_XOR):
	    LABEL (O_XORC):
	    res = arga ^ argb;
	    break;

	    LABEL (O_SCB_F):
	    {
	    scb_f:
	      res = arga - 1;
	      code->srca.reg.wptr[0] = res;
	      if (res != -1)
		{
		  pc = argb;
		  goto next;
		}
	    }
	    break;

	    LABEL (O_SCB_EQ):
	    if (Z == 1)
	      break;
	    else
	      goto scb_f;

	    LABEL (O_SCB_NE):
	    if (Z == 0)
	      break;
	    else
	      goto scb_f;

	    LABEL (O_NOP):
	    /* If only they were all as simple as this */
	    break;

	    LABEL (O_ROTL):
	    res = arga << 1;
	    C = (res >> argb) & 1;
	    res |= C;
	    break;


	    LABEL (O_ROTR):
	    C = arga & 1;
	    res = arga >> 1;
	    res |= (C << (argb - 1));
	    break;

	    LABEL (O_ROTXL):
	    res = arga << 1;
	    res |= C;
	    C = (res >> argb) & 1;
	    break;

	    LABEL (O_ROTXR):
	    res = arga >> 1;
	    res |= (C << (argb - 1));
	    C = arga & 1;
	    break;

	    LABEL (O_SHAL):
	    res = arga << 1;
	    if (argb == 16)
	      {
		C = (res >> (16)) & 1;
		Z = ((res & 0xffff) == 0);
		N = ((res & 0x8000) != 0);
	      }

	    else
	      {
		C = (res >> (8)) & 1;
		Z = ((res & 0xff) == 0);
		N = ((res & 0x80) != 0);

	      }
	    V = C ^ N;
	    goto none;

	    LABEL (O_SHAR):
	    C = arga & 1;
	    if (argb == 16)
	      {
		res = ((short) arga) >> 1;
	      }
	    else
	      {
		res = (SEXTCHAR (arga)) >> 1;
	      }
	    break;

	    LABEL (O_SHLL):
	    res = arga << 1;
	    C = (res >> argb) & 1;
	    break;

	    LABEL (O_SHLR):
	    C = arga & 1;
	    res = arga >> 1;
	    break;

	    LABEL (O_DIVXU):
	    if (arga == 0)
	      {
		N = V = C = 0;
		Z = 1;
		cpu.exception = SIGILL;
	      }
	    else
	      {
		int d = argb / arga;
		int m = argb % arga;
		if (code->dst.type == eas.s.ea_reg.s.dstlong)
		  {
		    res = (m << 16) | (d & 0xffff);
		  }
		else
		  {
		    res = (m << 8) | (d & 0xff);
		  }

	      }
	    break;

	    LABEL (O_MULXU):
	    res = arga * argb;
	    break;

	    LABEL (O_NOT):
	    res = ~arga;
	    break;

	    LABEL (O_SWAP):
	    res = ((arga >> 8) & 0xff) | ((arga << 8) & 0xff00);
	    break;


	    LABEL (O_STM):
	    for (tmp = 7; tmp >= 0; tmp--)
	      {
		if (arga & (1 << tmp))
		  {
		    PUSHWORD (cpu.regs[tmp].s[LOW]);
		  }
	      }
	    goto nextpc;

	    LABEL (O_TAS):
	    C = 0;
	    V = 0;
	    Z = arga == 0;
	    N = arga < 0;
	    res = arga | 0x80;
	    goto none;

	    LABEL (O_PRTD):
	    LABEL (O_XCH):
	    LABEL (O_RTD):
	    cpu.exception = SIGILL;
	    goto next;

	    LABEL (O_TRAP_VS):
	    LABEL (O_SLEEP):
	    LABEL (O_BPT):
	    cpu.exception = SIGTRAP;
	    goto next;
	    break;
	  }

	ENDDISPATCH;

	DISPATCH (code->flags)
	  {
	  bitop:
	    Z = (res & bit) == 0;
	    pc = code->next_pc;
	    break;
	    LABEL (FLAG_multword):
	    Z = (res & 0xffff) == 0;
	    N = (res & 0x8000) != 0;
	    V = 0;
	    C = 0;
	    pc = code->next_pc;
	    break;

	    LABEL (FLAG_multbyte):
	    /* 8*8 -> 16 */
	    Z = (res & 0xff) == 0;
	    N = (res & 0x80) != 0;
	    V = 0;
	    C = 0;
	    pc = code->next_pc;
	    break;

	    LABEL (FLAG_shiftword):
	    N = (res & 0x8000) != 0;
	    Z = (res & 0xffff) == 0;
	    V = 0;
	    pc = code->next_pc;
	    break;

	    LABEL (FLAG_shiftbyte):
	    N = (res & 0x80) != 0;
	    Z = (res & 0xff) == 0;
	    V = 0;
	    pc = code->next_pc;
	    break;

	    LABEL (FLAG_special):
	    pc = code->next_pc;
	    break;

	    LABEL (FLAG_m):
	    /* Move byte flags */
	    /* after a logical instruction */
	    N = (res & 0x80) != 0;
	    Z = (res & 0xff) == 0;
	    V = (((~arga & ~argb & res) | (arga & argb & ~res)) & 0x80) != 0;
	    pc = code->next_pc;
	    break;

	    LABEL (FLAG_M):
	    /* Move word flags */
	    /* after a logical instruction */
	    N = (res & 0x8000) != 0;
	    Z = (res & 0xffff) == 0;
	    V = (((~arga & ~argb & res) | (arga & argb & ~res)) & 0x8000) != 0;
	    pc = code->next_pc;
	    break;

	    LABEL (FLAG_a):
	    /* after byte sized arith */
	    C = (res & 0x100) != 0;
	    N = (res & 0x80) != 0;
	    Z = (res & 0xff) == 0;
	    V = (((~arga & ~argb & res) | (arga & argb & ~res)) & 0x80) != 0;
	    pc = code->next_pc;
	    break;

	    LABEL (FLAG_A):
	    /* after word sized arith */
	    C = (res & 0x10000) != 0;
	    N = (res & 0x8000) != 0;
	    Z = (res & 0xffff) == 0;
	    V = (((~arga & ~argb & res) | (arga & argb & ~res)) & 0x8000) != 0;
	    pc = code->next_pc;
	    break;

	    LABEL (FLAG_NONE):
	  none:;
	    /* no flags but store */
	    pc = code->next_pc;
	    break;
	    LABEL (FLAG_NOSTORE):
	    /* no flags and no store */
	    pc = code->next_pc;
	    break;
	    LABEL (FLAG_CLEAR):
	    /* clear flags */
	    N = 0;
	    Z = 1;
	    V = 0;
	    C = 0;
	    pc = code->next_pc;
	    break;
	  condtrue:
	    pc = arga;
	    goto next;
	  }
	ENDDISPATCH;

	DISPATCH (code->dst.type)
	  {
	    unsigned char *lval;

	    LABEL (STORE_CRB):
	    (*(code->dst.reg.segptr)) = cpu.memory + (res << 16);
	    break;

	    LABEL (STORE_NOP):
	    break;

	    LABEL (STORE_REG_B):
	    (*(code->dst.reg.bptr)) = res;
	    break;

	    LABEL (STORE_REG_W):
	    (*(code->dst.reg.wptr)) = res;
	    break;

	    LABEL (STORE_REG_L):
	    {
	      int l, r;

	      r = (union rtype *) (code->dst.reg.wptr) - &cpu.regs[0];
	      r++;
	      *(code->dst.reg.wptr) = res >> 16;
	      cpu.regs[r].s[LOW] = res & 0xffff;

	    }

	    break;

	    LABEL (STORE_DISP_W):
	    lval = displval (code->dst);
	    setwordat (lval, res);
	    break;

	    LABEL (STORE_DISP_B):
	    lval = displval (code->dst);
	    setbyteat (lval, res);
	    break;

	    LABEL (STORE_INC_B):
	    lval = elval (code->dst, 0);
	    setbyteat (lval, res);
	    (*(code->dst.reg.wptr))++;
	    break;

	    LABEL (STORE_INC_W):
	    lval = elval (code->dst, 0);
	    setwordat (lval, res);
	    (*(code->dst.reg.wptr)) += 2;
	    break;

	    LABEL (STORE_DEC_B):
	    (*(code->dst.reg.wptr))--;
	    lval = elval (code->dst, 0);
	    setbyteat (lval, res);
	    break;

	    LABEL (STORE_CRW):
	    /* Make an up to date sr from the flag state */
	    cpu.regs[R_SR].s[LOW] = res;
	    GETSR ();
	    break;

	    LABEL (STORE_DEC_W):
	    (*(code->dst.reg.wptr)) -= 2;
	    lval = elval (code->dst, 0);
	    setwordat (lval, res);

	    break;

	  nextpc:
	    pc = code->next_pc;

	  }
	ENDDISPATCH;
      next:;
      }
    while (!cpu.exception);
  }

  cpu.ticks += get_now () - tick_start;
  cpu.cycles += cycles;
  cpu.insts += insts;
  cpu.regs[R_PC].s[LOW] = pc;
  BUILDSR ();

  signal (SIGINT, prev);
  signal (SIGSEGV, prev_seg);
}




int
sim_write (addr, buffer, size)
     SIM_ADDR addr;
     unsigned char *buffer;
     int size;
{
  int i;

  init_pointers ();
  if (addr < 0 || addr + size > MSIZE)
    return 0;
  for (i = 0; i < size; i++)
    {
      cpu.memory[addr + i] = buffer[i];
      cpu.cache_idx[addr + i] = 0;
    }
  return size;
}

int
sim_read (addr, buffer, size)
     SIM_ADDR addr;
     unsigned char *buffer;
     int size;
{
  init_pointers ();
  if (addr < 0 || addr + size > MSIZE)
    return 0;
  memcpy (buffer, cpu.memory + addr, size);
  return size;
}

/* Ripped off from tm-h8500.h */

#define R0_REGNUM	0
#define R1_REGNUM	1
#define R2_REGNUM	2
#define R3_REGNUM	3
#define R4_REGNUM	4
#define R5_REGNUM	5
#define R6_REGNUM	6
#define R7_REGNUM	7

/* As above, but with correct seg register glued on */
#define PR0_REGNUM	8
#define PR1_REGNUM	9
#define PR2_REGNUM	10
#define PR3_REGNUM	11
#define PR4_REGNUM	12
#define PR5_REGNUM	13
#define PR6_REGNUM	14
#define PR7_REGNUM	15

#define SP_REGNUM       PR7_REGNUM	/* Contains address of top of stack */
#define FP_REGNUM       PR6_REGNUM	/* Contains address of executing stack frame */


#define SEG_C_REGNUM	16	/* Segment registers */
#define SEG_D_REGNUM	17
#define SEG_E_REGNUM	18
#define SEG_T_REGNUM	19

#define CCR_REGNUM      20	/* Contains processor status */
#define PC_REGNUM       21	/* Contains program counter */

#define CYCLE_REGNUM    22
#define INST_REGNUM     23
#define TICK_REGNUM     24

void
sim_store_register (rn, value)
     int rn;
     unsigned char *value;
{
  int seg = 0;
  int reg = -1;

  init_pointers ();
  switch (rn)
    {
    case PC_REGNUM:
      SET_SEGREG (R_CP, (value[1]<<16));
      cpu.regs[R_PC].s[LOW] = (value[2] << 8) | value[3];
      break;
    case SEG_C_REGNUM:
    case SEG_D_REGNUM:
    case SEG_E_REGNUM:
    case SEG_T_REGNUM:
      seg = rn - SEG_C_REGNUM + R_CP;
      reg = -1;
      break;
    default:
      abort ();
    case R0_REGNUM:
    case R1_REGNUM:
    case R2_REGNUM:
    case R3_REGNUM:
    case R4_REGNUM:
    case R5_REGNUM:
    case R6_REGNUM:
    case R7_REGNUM:
      seg = 0;
      reg = rn - R0_REGNUM;
      break;
    case CCR_REGNUM:
      seg = 0;
      reg = R_SR;
      break;
    case CYCLE_REGNUM:
      cpu.cycles = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
      return;
    case INST_REGNUM:
      cpu.insts = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
      return;
    case TICK_REGNUM:
      cpu.ticks = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
      return;
    case PR0_REGNUM:
    case PR1_REGNUM:
    case PR2_REGNUM:
    case PR3_REGNUM:
    case PR4_REGNUM:
    case PR5_REGNUM:
    case PR6_REGNUM:
    case PR7_REGNUM:
      SET_SEGREG (segforreg[rn], value[1]);
      reg = rn - PR0_REGNUM;      
      cpu.regs[reg].s[LOW] = (value[2] << 8) | value[3];
      return;
    }

  if (seg)
    SET_SEGREG (seg, value[0] << 16);

  if (reg > 0)
    {
      cpu.regs[reg].s[LOW] = (value[0] << 8) | value[1];
    }
}

void
sim_fetch_register (rn, buf)
     int rn;
     unsigned char *buf;
{
  init_pointers ();

  switch (rn)
    {
    default:
      abort ();
    case SEG_C_REGNUM:
    case SEG_D_REGNUM:
    case SEG_E_REGNUM:
    case SEG_T_REGNUM:
      buf[0] = GET_SEGREG(rn - SEG_C_REGNUM + R_CP);
      break;
    case CCR_REGNUM:
      buf[0] = cpu.regs[R_SR].s[HIGH];
      buf[1] = cpu.regs[R_SR].s[LOW];
      break;
    case PC_REGNUM:
      buf[0] = 0;
      buf[1] = GET_SEGREG(R_CP);
      buf[2] = HIGH_BYTE (cpu.regs[R_PC].s[LOW]);
      buf[3] = LOW_BYTE (cpu.regs[R_PC].s[LOW]);
      break;

    case PR0_REGNUM:
    case PR1_REGNUM:
    case PR2_REGNUM:
    case PR3_REGNUM:
    case PR4_REGNUM:
    case PR5_REGNUM:
    case PR6_REGNUM:
    case PR7_REGNUM:
      rn -= PR0_REGNUM;
      buf[0] = 0;
      buf[1] = GET_SEGREG(segforreg[rn]);
      buf[2] = HIGH_BYTE (cpu.regs[rn].s[LOW]);
      buf[3] = LOW_BYTE (cpu.regs[rn].s[LOW]);
      break;
    case R0_REGNUM:
    case R1_REGNUM:
    case R2_REGNUM:
    case R3_REGNUM:
    case R4_REGNUM:
    case R5_REGNUM:
    case R6_REGNUM:
    case R7_REGNUM:
      buf[0] = HIGH_BYTE (cpu.regs[rn].s[LOW]);
      buf[1] = LOW_BYTE (cpu.regs[rn].s[LOW]);
      break;
    case CYCLE_REGNUM:
      buf[0] = cpu.cycles >> 24;
      buf[1] = cpu.cycles >> 16;
      buf[2] = cpu.cycles >> 8;
      buf[3] = cpu.cycles >> 0;
      break;

    case TICK_REGNUM:
      buf[0] = cpu.ticks >> 24;
      buf[1] = cpu.ticks >> 16;
      buf[2] = cpu.ticks >> 8;
      buf[3] = cpu.ticks >> 0;
      break;

    case INST_REGNUM:
      buf[0] = cpu.insts >> 24;
      buf[1] = cpu.insts >> 16;
      buf[2] = cpu.insts >> 8;
      buf[3] = cpu.insts >> 0;
      break;
    }
}

int
sim_trace ()
{

  int i;

  for (i = 0; i < 12; i += 2)
    {
      unsigned char *p = cpu.regs[R_TP].c + ((cpu.regs[R6].s[LOW] + i) & 0xffff);
      unsigned short *j = (unsigned short *) p;

      printf ("%04x ", *j);
    }
  printf ("\n");
  printf ("%02x %02x %02x %02x:%04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
	  NORMAL_DP,
	  NORMAL_EP,
	  NORMAL_TP,
	  NORMAL_CP,
	  cpu.regs[R_PC].s[LOW],
	  cpu.regs[0].s[LOW],
	  cpu.regs[1].s[LOW],
	  cpu.regs[2].s[LOW],
	  cpu.regs[3].s[LOW],
	  cpu.regs[4].s[LOW],
	  cpu.regs[5].s[LOW],
	  cpu.regs[6].s[LOW],
	  cpu.regs[7].s[LOW]);
  sim_resume (1, 0);
  return 0;
}

void
sim_stop_reason (reason, sigrc)
     enum sim_stop *reason;
     int *sigrc;
{
  *reason = sim_stopped;
  *sigrc = cpu.exception;
}


sim_csize (n)
{
  if (cpu.cache)
    free (cpu.cache);
  if (n < 2)
    n = 2;
  cpu.cache = (decoded_inst *) malloc (sizeof (decoded_inst) * n);
  cpu.csize = n;
}


void
sim_info (verbose)
     int verbose;
{
  double timetaken = (double) cpu.ticks / (double) now_persec ();
  double virttime = cpu.cycles / 10.0e6;

  printf_filtered ("\n\ninstructions executed  %10d\n", cpu.insts);
  printf_filtered ("cycles (v approximate) %10d\n", cpu.cycles);
  printf_filtered ("real time taken        %10.4f\n", timetaken);
  printf_filtered ("virtual time taked     %10.4f\n", virttime);
  if (timetaken) 
    {
      printf_filtered ("simulation ratio       %10.4f\n", virttime / timetaken);
    }
  
  printf_filtered ("compiles               %10d\n", cpu.compiles);
  printf_filtered ("cache size             %10d\n", cpu.csize);
}

void
sim_kill()
{
  /* nothing to do */
}

void
sim_open (args)
     char *args;
{
  /* nothing to do */
}

void
sim_close (quitting)
     int quitting;
{
  /* nothing to do */
}

int
sim_load (prog, from_tty)
     char *prog;
     int from_tty;
{
  /* Return nonzero so gdb will handle it.  */
  return 1;
}

void
sim_create_inferior (start_address, argv, env)
     SIM_ADDR start_address;
     char **argv;
     char **env;
{
  /* ??? We assume this is a 4 byte quantity.  */
  int pc;

  pc = start_address;
  sim_store_register (PC_REGNUM, (unsigned char *) &pc);
}

void
sim_do_command (cmd)
     char *cmd;
{
  printf_filtered ("This simulator does not accept any commands.\n");
}


void
sim_set_callbacks (ptr)
struct host_callback_struct *ptr;
{

}
