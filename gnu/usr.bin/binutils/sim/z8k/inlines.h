/* inline functions for Z8KSIM
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.

This file is part of Z8KSIM

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Z8KZIM; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef INLINE
#define INLINE
#endif
#define UGT 0x0b
#define ULE 0x03
#define ULT 0x07
#define UGE 0x0f
#define SLOW 0
#define T 0x8
#define F 0x0
#define LT 0x1
#define GT 0xa
#define LE 0x2
#define EQ 0x6
#define NE 0xe
#define GE 0x9

 static int is_cond_true PARAMS((sim_state_type *context, int c)); 
 static void makeflags PARAMS((sim_state_type *context, int mask)); 

static INLINE
long
sitoptr (si)
long si;
{
  return ((si & 0xff000000) >> 8) | (si & 0xffff);
}
static INLINE long
ptrtosi (ptr)
long ptr;
{
  return ((ptr & 0xff0000) << 8) | (ptr & 0xffff);
}

static INLINE
void 
put_long_reg (context, reg, val)
     sim_state_type *context;
     int reg;
     int val;
{
  context->regs[reg].word = val >> 16;
  context->regs[reg + 1].word = val;
}

static INLINE
void 
put_quad_reg (context, reg, val1, val2)
     sim_state_type *context;
     int reg;
     int val1;
     int val2;
{
  context->regs[reg].word = val2 >> 16;
  context->regs[reg + 1].word = val2;
  context->regs[reg + 2].word = val1 >> 16;
  context->regs[reg + 3].word = val1;
}

static INLINE
void 
put_word_reg (context, reg, val)
     sim_state_type *context;
     int reg;
     int val;
{
  context->regs[reg].word = val;
}

static INLINE
SItype get_long_reg (context, reg)
     sim_state_type *context;
     int reg;
{
  USItype lsw = context->regs[reg + 1].word;
  USItype msw = context->regs[reg].word;

  return (msw << 16) | lsw;
}

#ifdef __GNUC__
static INLINE
struct UDIstruct
get_quad_reg (context, reg)
     sim_state_type *context;
     int reg;
{
  UDItype res;
  USItype lsw = get_long_reg (context, reg + 2);
  USItype msw = get_long_reg (context, reg);

  res.low = lsw;
  res.high = msw;
  return res;
}

#endif

static INLINE void
put_byte_mem_da (context, addr, value)
     sim_state_type *context;
     int addr;
     int value;
{
  ((unsigned char *) (context->memory))[addr] = value;
}

static INLINE
void 
put_byte_reg (context, reg, val)
     sim_state_type *context;
     int reg;
     int val;
{
  int old = context->regs[reg & 0x7].word;
  if (reg & 0x8)
    {
      old = old & 0xff00 | (val & 0xff);
    }
  else
    {
      old = old & 0x00ff | (val << 8);
    }
  context->regs[reg & 0x7].word = old;      
}

static INLINE
int 
get_byte_reg (context, reg)
     sim_state_type *context;
     int reg;
{
  if (reg & 0x8)
    return  context->regs[reg & 0x7].word & 0xff;
  else
    return  (context->regs[reg & 0x7].word >> 8) & 0xff;
}

static INLINE
void 
put_word_mem_da (context, addr, value)
     sim_state_type *context;
     int addr;
     int value;
{
  if (addr & 1)
    {
      context->exception = SIM_BAD_ALIGN;
      addr &= ~1;
    }
  put_byte_mem_da(context, addr, value>>8);
  put_byte_mem_da(context, addr+1, value);
}

static INLINE unsigned char
get_byte_mem_da (context, addr)
     sim_state_type *context;
     int addr;
{
  return ((unsigned char *) (context->memory))[addr];
}


#if 0
#define get_word_mem_da(context,addr)\
 *((unsigned short*)((char*)((context)->memory)+(addr)))

#else
#define get_word_mem_da(context,addr) (get_byte_mem_da(context, addr) << 8) | (get_byte_mem_da(context,addr+1))
#endif

#define get_word_reg(context,reg) (context)->regs[reg].word

static INLINE
SItype
get_long_mem_da (context, addr)
     sim_state_type *context;
     int addr;
{
  USItype lsw = get_word_mem_da(context,addr+2);
  USItype msw =  get_word_mem_da(context, addr);

  return (msw << 16) + lsw;
}

static INLINE
void 
put_long_mem_da (context, addr, value)
     sim_state_type *context;
     int addr;
     int value;
{
  put_word_mem_da(context,addr, value>>16);
  put_word_mem_da(context,addr+2, value);
}

static INLINE
int 
get_word_mem_ir (context, reg)
     sim_state_type *context;
     int reg;
{
  return get_word_mem_da (context, get_word_reg (context, reg));
}

static INLINE
void 
put_word_mem_ir (context, reg, value)
     sim_state_type *context;
     int reg;
     int value;
{

  put_word_mem_da (context, get_word_reg (context, reg), value);
}

static INLINE
int 
get_byte_mem_ir (context, reg)
     sim_state_type *context;
     int reg;
{
  return get_byte_mem_da (context, get_word_reg (context, reg));
}

static INLINE
void 
put_byte_mem_ir (context, reg, value)
     sim_state_type *context;
     int reg;
     int value;
{
  put_byte_mem_da (context, get_word_reg (context, reg), value);
}

static INLINE
int 
get_long_mem_ir (context, reg)
     sim_state_type *context;
     int reg;
{
  return get_long_mem_da (context, get_word_reg (context, reg));
}

static INLINE
void 
put_long_mem_ir (context, reg, value)
     sim_state_type *context;
     int reg;
     int value;
{

  put_long_mem_da (context, get_word_reg (context, reg), value);
}

static INLINE
void 
put_long_mem_x (context, base, reg, value)
     sim_state_type *context;
     int base;
     int reg;
     int value;
{
  put_long_mem_da (context, get_word_reg (context, reg) + base, value);
}

static INLINE
void 
put_word_mem_x (context, base, reg, value)
     sim_state_type *context;
     int base;
     int reg;
     int value;
{
  put_word_mem_da (context, get_word_reg (context, reg) + base, value);
}

static INLINE
void 
put_byte_mem_x (context, base, reg, value)
     sim_state_type *context;
     int base;
     int reg;
     int value;
{
  put_byte_mem_da (context, get_word_reg (context, reg) + base, value);
}

static INLINE
int 
get_word_mem_x (context, base, reg)
     sim_state_type *context;
     int base;
     int reg;
{
  return get_word_mem_da (context, base + get_word_reg (context, reg));
}

static INLINE
int 
get_byte_mem_x (context, base, reg)
     sim_state_type *context;
     int base;
     int reg;
{
  return get_byte_mem_da (context, base + get_word_reg (context, reg));
}

static INLINE
int 
get_long_mem_x (context, base, reg)
     sim_state_type *context;
     int base;
     int reg;
{
  return get_long_mem_da (context, base + get_word_reg (context, reg));
}


static
void
makeflags (context, mask)
     sim_state_type *context;
     int mask;
{

  PSW_ZERO = (context->dst & mask) == 0;
  PSW_SIGN = (context->dst >> (context->size - 1));

  if (context->broken_flags == TST_FLAGS)
    {
      extern char the_parity[];

      if (context->size == 8)
	{
	  PSW_OVERFLOW = the_parity[context->dst & 0xff];
	}
    }
  else
    {
      /* Overflow is set if both operands have the same sign and the
         result is of different sign.

         V =  A==B && R!=B  jumping logic
         (~(A^B))&(R^B)
         V =  (A^B)^(R^B)   boolean
         */

      PSW_OVERFLOW =
	((
	   (~(context->srca ^ context->srcb)
	    & (context->srca ^ context->dst))
	 ) >> (context->size - 1)
	);

      if (context->size < 32)
	{
	  PSW_CARRY = ((context->dst >> context->size)) & 1;
	}
      else
	{
	  /* carry is set when the result is smaller than a source */


	  PSW_CARRY =  (unsigned) context->dst > (unsigned) context->srca ;

	}
    }
  context->broken_flags = 0;
}


/* There are two ways to calculate the flags.  We can
   either always calculate them and so the cc will always
   be correct, or we can only keep the arguments around and
   calc the flags when they're actually going to be used. */

/* Right now we always calc the flags - I think it may be faster*/


#define NORMAL_FLAGS(c,s,d,sa,sb,sub) 	\
    if (s == 8)                \
      normal_flags_8(c,d,sa,sb,sub); \
    else if (s == 16)                \
      normal_flags_16(c,d,sa,sb,sub); \
    else if (s == 32)                \
      normal_flags_32(c,d,sa,sb,sub); 

static INLINE
void 
normal_flags (context, size, dst, srca, srcb)
     sim_state_type *context;
     int size;
     int dst;
     int srca;
     int srcb;
{
  context->srca = srca;
  context->srcb = srcb;
  context->dst = dst;
  context->size = size;
  context->broken_flags = CMP_FLAGS;
}

static INLINE
void 
TEST_NORMAL_FLAGS (context, size, dst)
     sim_state_type *context;
     int size;
     int dst;
{
  context->dst = dst;
  context->size = size;
  context->broken_flags = TST_FLAGS;
}

static INLINE
void 
put_ptr_long_reg (context, reg, val)
     sim_state_type *context;
     int reg;
     int val;
{
  context->regs[reg].word = (val >> 8) & 0x7f00;
  context->regs[reg + 1].word = val;
}

static INLINE
long 
get_ptr_long_reg (context, reg)
     sim_state_type *context;
     int reg;
{
  int val;

  val = (context->regs[reg].word << 8) | context->regs[reg + 1].word;
  return val;
}

static INLINE
long 
get_ptr_long_mem_ir (context, reg)
sim_state_type *context;
int reg;
{
  return sitoptr (get_long_mem_da (context, get_ptr_long_reg (context, reg)));
}

static INLINE
long 
get_ptr_long_mem_da (context, addr)
sim_state_type *context;
long addr; 
{
  return sitoptr (get_long_mem_da (context, addr));
}

static INLINE
void 
put_ptr_long_mem_da (context, addr, ptr)
sim_state_type *context;
long addr; 
long ptr;
{
  put_long_mem_da (context, addr, ptrtosi (ptr));

}
