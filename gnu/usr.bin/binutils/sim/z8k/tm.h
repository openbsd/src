/* tm.h
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.

This file is part of Z8KSIM

Z8KSIM is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Z8KSIM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Z8KZIM; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _TM_H
#define _TM_H

#ifdef __FOOBEYGNUC__
typedef SFtype __attribute__ ((mode (SF)));
typedef DFtype __attribute__ ((mode (DF)));

typedef int HItype __attribute__ ((mode (HI)));
typedef int SItype __attribute__ ((mode (SI)));

typedef unsigned int UHItype __attribute__ ((mode (HI)));
typedef unsigned int USItype __attribute__ ((mode (SI)));
#else
typedef float SFtype;
typedef double DFtype;
typedef short int HItype;
typedef long int SItype;
typedef unsigned short UHItype ;
typedef unsigned int USItype ;
#endif

typedef struct UDIstruct
{
  USItype high;
  USItype low;
} UDItype;

#define BIG_ENDIAN_HOST
typedef unsigned int sim_phys_addr_type;
typedef unsigned int sim_logical_addr_type;

#define PAGE_POWER 23		/*  only one pages */

#define MAP_PHYSICAL_TO_LOGICAL(x)  (((x >> 8) & 0x7f0000) | (x & 0xffff))
#define MAP_LOGICAL_TO_PHYSICAL(x)  (((x <<8) & 0x7f000000) | (x & 0xffff))
#define REG_PC     17
#define REG_CYCLES 18
#define REG_INSTS  19
#define REG_TIME   20
#define REG_FP     21
#define REG_SP     22
#define REG_CCR    16

#define SET_REG(x,y)  set_reg(x,y)
#define SINGLE_STEP 1

#define PSW_CARRY context->carry
#define PSW_OP    context->op
#define PSW_OVERFLOW context->overflow
#define PSW_SIGN context->sign
#define PSW_ZERO context->zero
#define GET_PC()  context->pc
#define SET_PC(x) context->pc = x

struct op_info
{
  short int exec;
};

extern struct op_info op_info_table[];

typedef union
{
  unsigned short int word;

}

borw_type;

typedef struct state_struct
{
  unsigned short *memory;
  int carry;
  int sign;
  int zero;
  int overflow;
  int op;
  int cycles;

  borw_type regs[16];

  sim_phys_addr_type sometimes_pc;
#ifdef __GNUC__
  volatile
#endif
  int exception;

#define iwords_0  iwords0
#define iwords_1  iwords1
#define iwords_2  iwords2
#define iwords_3  iwords3

#define ibytes_0    (iwords_0>>8)
#define ibytes_1    (iwords_0&0xff)
#define ibytes_2    (iwords_1>>8)
#define ibytes_3    (iwords_1& 0xff)
#define ibytes_4    (iwords_2>>8)

  int insts;
  int ticks;

  int next_inst;
  int broken_flags;

  int srca;
  int srcb;
  int dst;
  int size;
}

sim_state_type;

#define CMP_FLAGS 100
#define TST_FLAGS 101
#endif

extern int get_word_mem_da PARAMS((sim_state_type *context, int addr)); 
extern int get_word_reg PARAMS((sim_state_type *context, int reg)); 
extern int sim_trace PARAMS((void)); 
extern void support_call PARAMS((sim_state_type *context, int sc)); 
extern void tm_exception PARAMS((int x)); 
extern int tm_read_byte PARAMS((int x)); 
extern int tm_signal PARAMS((void)); 
extern void tm_state PARAMS((sim_state_type *x)); 
extern void tm_write_byte PARAMS((int x, int y)); 
extern void bfop_bad1 PARAMS(()); 
extern int fail PARAMS((sim_state_type *context, int v)); 
extern void fop_bad PARAMS((sim_state_type *context)); 
extern void sfop_bad1 PARAMS(()); 
extern void swap_long PARAMS((char *buf, int val)); 
extern void swap_word PARAMS((char *buf, int val)); 
extern void tm_fetch_register PARAMS((int regno, char *buf)); 
extern void tm_info_print PARAMS((sim_state_type *x)); 
extern void tm_resume PARAMS((int step)); 
extern void tm_store_register PARAMS((int regno, int value)); 


#ifndef __GNUC__
/* If were using gnuc then these will be inlined, so the prototypes 
 won't be right */
long int sitoptr PARAMS((long int si)); 
long int ptrtosi PARAMS((long int ptr)); 
void put_long_reg PARAMS((sim_state_type *context, int reg, int val)); 
void put_quad_reg PARAMS((sim_state_type *context, int reg, int val1, int val2)); 
void put_word_reg PARAMS((sim_state_type *context, int reg, int val)); 
SItype get_long_reg PARAMS((sim_state_type *context, int reg)); 
void put_byte_reg PARAMS((sim_state_type *context, int reg, int val)); 
int get_byte_reg PARAMS((sim_state_type *context, int reg)); 
void put_word_mem_da PARAMS((sim_state_type *context, int addr, int value)); 
unsigned char get_byte_mem_da PARAMS((sim_state_type *context, int addr)); 
void put_byte_mem_da PARAMS((sim_state_type *context, int addr, int value)); 
SItype get_long_mem_da PARAMS((sim_state_type *context, int addr)); 
void put_long_mem_da PARAMS((sim_state_type *context, int addr, int value)); 
int get_word_mem_ir PARAMS((sim_state_type *context, int reg)); 
void put_word_mem_ir PARAMS((sim_state_type *context, int reg, int value)); 
int get_byte_mem_ir PARAMS((sim_state_type *context, int reg)); 
void put_byte_mem_ir PARAMS((sim_state_type *context, int reg, int value)); 
int get_long_mem_ir PARAMS((sim_state_type *context, int reg)); 
void put_long_mem_ir PARAMS((sim_state_type *context, int reg, int value)); 
void put_long_mem_x PARAMS((sim_state_type *context, int base, int reg, int value)); 
void put_word_mem_x PARAMS((sim_state_type *context, int base, int reg, int value)); 
void put_byte_mem_x PARAMS((sim_state_type *context, int base, int reg, int value)); 
int get_word_mem_x PARAMS((sim_state_type *context, int base, int reg)); 
int get_byte_mem_x PARAMS((sim_state_type *context, int base, int reg)); 
int get_long_mem_x PARAMS((sim_state_type *context, int base, int reg)); 
int COND PARAMS((sim_state_type *context, int c)); 
void NORMAL_FLAGS PARAMS((sim_state_type *context, int size, int dst, int srca, int srcb)); 
void TEST_NORMAL_FLAGS PARAMS((sim_state_type *context, int size, int dst)); 
void put_ptr_long_reg PARAMS((sim_state_type *context, int reg, int val)); 
long int get_ptr_long_reg PARAMS((sim_state_type *context, int reg)); 
long int get_ptr_long_mem_ir PARAMS((sim_state_type *context, int reg)); 
long int get_ptr_long_mem_da PARAMS((sim_state_type *context, long int addr)); 
void put_ptr_long_mem_da PARAMS((sim_state_type *context, long int addr, long int ptr)); 
#endif
