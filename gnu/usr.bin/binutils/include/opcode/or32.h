/* Table of opcodes for the OpenRISC 1000 ISA.
   Copyright 2002 Free Software Foundation, Inc.
   Contributed by Damjan Lampret (lampret@opencores.org).
   
   This file is part of or1k_gen_isa, or1ksim, GDB and GAS.

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

/* We treat all letters the same in encode/decode routines so
   we need to assign some characteristics to them like signess etc.  */

#ifndef OR32_H_ISA
#define OR32_H_ISA

#define NUM_UNSIGNED (0)
#define NUM_SIGNED (1)

#ifndef PARAMS
#define PARAMS(x) x
#endif

#define MAX_GPRS 32
#define PAGE_SIZE 4096
#undef __HALF_WORD_INSN__

#define OPERAND_DELIM (',')

#define OR32_IF_DELAY (1)
#define OR32_W_FLAG   (2)
#define OR32_R_FLAG   (4)

struct or32_letter
{
  char letter;
  int  sign;
  /* int  reloc; relocation per letter ??  */
};

/* Main instruction specification array.  */
struct or32_opcode
{
  /* Name of the instruction.  */
  char *name;

  /* A string of characters which describe the operands.
     Valid characters are:
     ,() Itself.  Characters appears in the assembly code.
     rA	 Register operand.
     rB  Register operand.
     rD  Register operand.
     I	 An immediate operand, range -32768 to 32767.
     J	 An immediate operand, range . (unused)
     K	 An immediate operand, range 0 to 65535.
     L	 An immediate operand, range 0 to 63.
     M	 An immediate operand, range . (unused)
     N	 An immediate operand, range -33554432 to 33554431.
     O	 An immediate operand, range . (unused).  */
  char *args;
  
  /* Opcode and operand encoding.  */
  char *encoding;
  void (*exec) PARAMS ((void));
  unsigned int flags;
};

#define OPTYPE_LAST (0x80000000)
#define OPTYPE_OP   (0x40000000)
#define OPTYPE_REG  (0x20000000)
#define OPTYPE_SIG  (0x10000000)
#define OPTYPE_DIS  (0x08000000)
#define OPTYPE_DST  (0x04000000)
#define OPTYPE_SBIT (0x00001F00)
#define OPTYPE_SHR  (0x0000001F)
#define OPTYPE_SBIT_SHR (8)

/* MM: Data how to decode operands.  */
extern struct insn_op_struct
{
  unsigned long type;
  unsigned long data;
} **op_start;

#ifdef HAS_EXECUTION
extern void l_invalid PARAMS ((void));
extern void l_sfne    PARAMS ((void));
extern void l_bf      PARAMS ((void));
extern void l_add     PARAMS ((void));
extern void l_sw      PARAMS ((void));
extern void l_sb      PARAMS ((void));
extern void l_sh      PARAMS ((void));
extern void l_lwz     PARAMS ((void));
extern void l_lbs     PARAMS ((void));
extern void l_lbz     PARAMS ((void));
extern void l_lhs     PARAMS ((void));
extern void l_lhz     PARAMS ((void));
extern void l_movhi   PARAMS ((void));
extern void l_and     PARAMS ((void));
extern void l_or      PARAMS ((void));
extern void l_xor     PARAMS ((void));
extern void l_sub     PARAMS ((void));
extern void l_mul     PARAMS ((void));
extern void l_div     PARAMS ((void));
extern void l_divu    PARAMS ((void));
extern void l_sll     PARAMS ((void));
extern void l_sra     PARAMS ((void));
extern void l_srl     PARAMS ((void));
extern void l_j       PARAMS ((void));
extern void l_jal     PARAMS ((void));
extern void l_jalr    PARAMS ((void));
extern void l_jr      PARAMS ((void));
extern void l_rfe     PARAMS ((void));
extern void l_nop     PARAMS ((void));
extern void l_bnf     PARAMS ((void));
extern void l_sfeq    PARAMS ((void));
extern void l_sfgts   PARAMS ((void));
extern void l_sfges   PARAMS ((void));
extern void l_sflts   PARAMS ((void));
extern void l_sfles   PARAMS ((void));
extern void l_sfgtu   PARAMS ((void));
extern void l_sfgeu   PARAMS ((void));
extern void l_sfltu   PARAMS ((void));
extern void l_sfleu   PARAMS ((void));
extern void l_mtspr   PARAMS ((void));
extern void l_mfspr   PARAMS ((void));
extern void l_sys     PARAMS ((void));
extern void l_trap    PARAMS ((void)); /* CZ 21/06/01.  */
extern void l_macrc   PARAMS ((void));
extern void l_mac     PARAMS ((void));
extern void l_msb     PARAMS ((void));
extern void l_invalid PARAMS ((void));
extern void l_cust1   PARAMS ((void));
extern void l_cust2   PARAMS ((void));
extern void l_cust3   PARAMS ((void));
extern void l_cust4   PARAMS ((void));
#endif
extern void l_none    PARAMS ((void));

extern const struct or32_letter or32_letters[];

extern const struct  or32_opcode or32_opcodes[];

extern const unsigned int or32_num_opcodes;

/* Calculates instruction length in bytes.  Always 4 for OR32.  */
extern int insn_len PARAMS ((int));

/* Is individual insn's operand signed or unsigned?  */
extern int letter_signed PARAMS ((char));

/* Number of letters in the individual lettered operand.  */
extern int letter_range PARAMS ((char));

/* MM: Returns index of given instruction name.  */
extern int insn_index PARAMS ((char *));

/* MM: Returns instruction name from index.  */
extern const char *insn_name PARAMS ((int));

/* MM: Constructs new FSM, based on or32_opcodes.  */ 
extern void build_automata PARAMS ((void));

/* MM: Destructs FSM.  */ 
extern void destruct_automata PARAMS ((void));

/* MM: Decodes instruction using FSM.  Call build_automata first.  */
extern int insn_decode PARAMS ((unsigned int));

/* Disassemble one instruction from insn to disassemble.
   Return the size of the instruction.  */
int disassemble_insn PARAMS ((unsigned long));

#endif
