/* Disassembly routines for TMS320C54X architecture
   Copyright 1999, 2000 Free Software Foundation, Inc.
   Contributed by Timothy Wall (twall@cygnus.com)

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic54x.h"
#include "coff/tic54x.h"

typedef struct _instruction {
  int parallel;
  template *tm;
  partemplate *ptm;
} instruction;

static int get_insn_size PARAMS ((unsigned short, instruction *));
static int get_instruction PARAMS ((disassemble_info *, bfd_vma, 
                                    unsigned short, instruction *));
static int print_instruction PARAMS ((disassemble_info *, bfd_vma, 
                                      unsigned short, char *, 
                                      enum optype [], int, int));
static int print_parallel_instruction PARAMS ((disassemble_info *, bfd_vma,
                                               unsigned short, partemplate *,
                                               int)); 
static int sprint_dual_address (disassemble_info *,char [], 
                                unsigned short);
static int sprint_indirect_address (disassemble_info *,char [], 
                                    unsigned short);
static int sprint_direct_address (disassemble_info *,char [], 
                                  unsigned short);
static int sprint_mmr (disassemble_info *,char [],int);
static int sprint_condition (disassemble_info *,char *,unsigned short);
static int sprint_cc2 (disassemble_info *,char *,unsigned short);

int
print_insn_tic54x(memaddr, info)
  bfd_vma memaddr;
  disassemble_info *info;
{
  bfd_byte opbuf[2]; 
  unsigned short opcode;
  int status, size;
  instruction insn;

  status = (*info->read_memory_func) (memaddr, opbuf, 2, info);
  if (status != 0)
  {
    (*info->memory_error_func)(status, memaddr, info);
    return -1;
  }

  opcode = bfd_getl16(opbuf);
  if (!get_instruction (info, memaddr, opcode, &insn))
      return -1;

  size = get_insn_size (opcode, &insn);
  info->bytes_per_line = 2;
  info->bytes_per_chunk = 2;
  info->octets_per_byte = 2;
  info->display_endian = BFD_ENDIAN_LITTLE;

  if (insn.parallel)
  {
    if (!print_parallel_instruction (info, memaddr, opcode, insn.ptm, size))
      return -1;
  }
  else
  {
    if (!print_instruction (info, memaddr, opcode, 
                            (char *)insn.tm->name, 
                            insn.tm->operand_types,
                            size, (insn.tm->flags & FL_EXT)))
      return -1;
  }

  return size*2;
}

static int
has_lkaddr(opcode, tm)
  unsigned short opcode;
  template *tm;
{
  return IS_LKADDR(opcode) && 
    (OPTYPE(tm->operand_types[0]) == OP_Smem ||
     OPTYPE(tm->operand_types[1]) == OP_Smem ||
     OPTYPE(tm->operand_types[2]) == OP_Smem ||
     OPTYPE(tm->operand_types[1]) == OP_Sind);
}

/* always returns 1 (whether an insn template was found) since we provide an
   "unknown instruction" template */
static int 
get_instruction (info, addr, opcode, insn)
  disassemble_info *info;
  bfd_vma addr;
  unsigned short opcode;
  instruction *insn;
{
  template * tm;
  partemplate * ptm;

  insn->parallel = 0;
  for (tm = (template *)tic54x_optab; tm->name; tm++)
  {
    if (tm->opcode == (opcode & tm->mask))
    {
      /* a few opcodes span two words */
      if (tm->flags & FL_EXT)
        {
          /* if lk addressing is used, the second half of the opcode gets
             pushed one word later */
          bfd_byte opbuf[2];
          bfd_vma addr2 = addr + 1 + has_lkaddr(opcode, tm);
          int status = (*info->read_memory_func)(addr2, opbuf, 2, info);
          if (status == 0)
            {
              unsigned short opcode2 = bfd_getl16(opbuf);
              if (tm->opcode2 == (opcode2 & tm->mask2))
                {
                  insn->tm = tm;
                  return 1;
                }
            }
        }
      else
        {
          insn->tm = tm;
          return 1;
        }
    }
  }
  for (ptm = (partemplate *)tic54x_paroptab; ptm->name; ptm++)
  {
    if (ptm->opcode == (opcode & ptm->mask))
    {
      insn->parallel = 1;
      insn->ptm = ptm;
      return 1;
    }
  }

  insn->tm = (template *)&tic54x_unknown_opcode;
  return 1;
}

static int 
get_insn_size (opcode, insn)
  unsigned short opcode;
  instruction *insn;
{
  int size;

  if (insn->parallel)
    {
      /* only non-parallel instructions support lk addressing */
      size = insn->ptm->words;
    }
  else
    {
      size = insn->tm->words + has_lkaddr(opcode, insn->tm);
    }

  return size;
}

int
print_instruction (info, memaddr, opcode, tm_name, tm_operands, size, ext)
  disassemble_info *info;
  bfd_vma memaddr;
  unsigned short opcode;
  char *tm_name;
  enum optype tm_operands[];
  int size;
  int ext;
{
  static int n;
  /* string storage for multiple operands */
  char operand[4][64] = { {0},{0},{0},{0}, };
  bfd_byte buf[2];
  unsigned long opcode2, lkaddr;
  enum optype src = OP_None;
  enum optype dst = OP_None;
  int i, shift;
  char *comma = "";

  info->fprintf_func (info->stream, "%-7s", tm_name);

  if (size > 1)
    {
      int status = (*info->read_memory_func) (memaddr+1, buf, 2, info);
      if (status != 0)
        return 0;
      lkaddr = opcode2 = bfd_getl16(buf);
      if (size > 2)
        {
          status = (*info->read_memory_func) (memaddr+2, buf, 2, info);
          if (status != 0)
            return 0;
          opcode2 = bfd_getl16(buf);
        }
    }

  for (i=0;i < MAX_OPERANDS && OPTYPE(tm_operands[i]) != OP_None;i++)
    {
      char *next_comma = ",";
      int optional = (tm_operands[i] & OPT) != 0;

      switch (OPTYPE(tm_operands[i]))
        {
        case OP_Xmem:
          sprint_dual_address (info, operand[i], XMEM(opcode));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_Ymem:
          sprint_dual_address (info, operand[i], YMEM(opcode));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_Smem:
        case OP_Sind:
        case OP_Lmem:
          info->fprintf_func (info->stream, "%s", comma);
          if (INDIRECT(opcode))
            {
              if (MOD(opcode) >= 12)
                {
                  bfd_vma addr = lkaddr;
                  int arf = ARF(opcode);
                  int mod = MOD(opcode);
                  if (mod == 15)
                      info->fprintf_func (info->stream, "*(");
                  else
                      info->fprintf_func (info->stream, "*%sar%d(", 
                                          (mod == 13 || mod == 14 ? "+" : ""),
                                          arf);
                  (*(info->print_address_func))((bfd_vma)addr, info);
                  info->fprintf_func (info->stream, ")%s", 
                                      mod == 14 ? "%" : "");
                }
              else
                {
                  sprint_indirect_address (info, operand[i], opcode);
                  info->fprintf_func (info->stream, "%s", operand[i]);
                }
            }
          else
          {
            /* FIXME -- use labels (print_address_func) */
            /* in order to do this, we need to guess what DP is */
            sprint_direct_address (info, operand[i], opcode);
            info->fprintf_func (info->stream, "%s", operand[i]);
          }
          break;
        case OP_dmad:
          info->fprintf_func (info->stream, "%s", comma);
          (*(info->print_address_func))((bfd_vma)opcode2, info);
          break;
        case OP_xpmad:
          /* upper 7 bits of address are in the opcode */
          opcode2 += ((unsigned long)opcode & 0x7F) << 16;
          /* fall through */
        case OP_pmad:
          info->fprintf_func (info->stream, "%s", comma);
          (*(info->print_address_func))((bfd_vma)opcode2, info);
          break;
        case OP_MMRX:
          sprint_mmr (info, operand[i], MMRX(opcode));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_MMRY:
          sprint_mmr (info, operand[i], MMRY(opcode));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_MMR:
          sprint_mmr (info, operand[i], MMR(opcode));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_PA:
          sprintf (operand[i], "pa%d", (unsigned)opcode2);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_SRC:
          src = SRC(ext ? opcode2 : opcode) ? OP_B : OP_A;
          sprintf (operand[i], (src == OP_B) ? "b" : "a");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_SRC1:
          src = SRC1(ext ? opcode2 : opcode) ? OP_B : OP_A;
          sprintf (operand[i], (src == OP_B) ? "b" : "a");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_RND:
          dst = DST(opcode) ? OP_B : OP_A;
          sprintf (operand[i], (dst == OP_B) ? "a" : "b");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_DST:
          dst = DST(ext ? opcode2 : opcode) ? OP_B : OP_A;
          if (!optional || dst != src)
            {
              sprintf (operand[i], (dst == OP_B) ? "b" : "a");
              info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
            }
          else
            next_comma = comma;
          break;
        case OP_B:
          sprintf (operand[i], "b");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_A:
          sprintf (operand[i], "a");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_ARX:
          sprintf (operand[i],"ar%d", (int)ARX(opcode));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_SHIFT:
          shift = SHIFT(ext ? opcode2 : opcode);
          if (!optional || shift != 0)
            {
              sprintf (operand[i],"%d", shift);
              info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
            }
          else
            next_comma = comma;
          break;
        case OP_SHFT:
          shift = SHFT(opcode);
          if (!optional || shift != 0)
            {
              sprintf (operand[i],"%d", (unsigned)shift);
              info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
            }
          else
            next_comma = comma;
          break;
        case OP_lk:
          sprintf (operand[i],"#%d", (int)(short)opcode2);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_T:
          sprintf (operand[i], "t");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_TS:
          sprintf (operand[i], "ts");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_k8:
          sprintf (operand[i], "%d", (int)((signed char)(opcode & 0xFF)));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_16:
          sprintf (operand[i], "16");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_ASM:
          sprintf (operand[i], "asm");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_BITC:
          sprintf (operand[i], "%d", (int)(opcode & 0xF));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_CC:
          /* put all CC operands in the same operand */
          sprint_condition (info, operand[i], opcode);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          i = MAX_OPERANDS;
          break;
        case OP_CC2:
          sprint_cc2 (info, operand[i], opcode);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_CC3:
        {
          const char *code[] = { "eq", "lt", "gt", "neq" };
          sprintf (operand[i], code[CC3(opcode)]);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        }
        case OP_123:
          {
            int code = (opcode>>8) & 0x3;
            sprintf (operand[i], "%d", (code == 0) ? 1 : (code == 2) ? 2 : 3);
            info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
            break;
          }
        case OP_k5:
          sprintf (operand[i], "#%d", 
                   (int)(((signed char)opcode & 0x1F) << 3)>>3);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_k8u:
          sprintf (operand[i], "#%d", (unsigned)(opcode & 0xFF));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_k3:
          sprintf (operand[i], "#%d", (int)(opcode & 0x7));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_lku:
          sprintf (operand[i], "#%d", (unsigned)opcode2);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_N:
          n = (opcode >> 9) & 0x1;
          sprintf (operand[i], "st%d", n);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_SBIT:
        {
          const char *status0[] = {
            "0", "1", "2", "3", "4", "5", "6", "7", "8", 
            "ovb", "ova", "c", "tc", "13", "14", "15"
          };
          const char *status1[] = {
            "0", "1", "2", "3", "4", 
            "cmpt", "frct", "c16", "sxm", "ovm", "10",
            "intm", "hm", "xf", "cpl", "braf"
          };
          sprintf (operand[i], "%s", 
                   n ? status1[SBIT(opcode)] : status0[SBIT(opcode)]);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        }
        case OP_12:
          sprintf (operand[i], "%d", (int)((opcode >> 9)&1) + 1);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_TRN:
          sprintf (operand[i], "trn");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_DP:
          sprintf (operand[i], "dp");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_k9:
          /* FIXME-- this is DP, print the original address? */
          sprintf (operand[i], "#%d", (int)(opcode & 0x1FF));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_ARP:
          sprintf (operand[i], "arp");
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        case OP_031:
          sprintf (operand[i], "%d", (int)(opcode & 0x1F));
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        default:
          sprintf (operand[i], "??? (0x%x)", tm_operands[i]);
          info->fprintf_func (info->stream, "%s%s", comma, operand[i]);
          break;
        }
      comma = next_comma;
    }
  return 1;
}

static int
print_parallel_instruction (info, memaddr, opcode, ptm, size)
  disassemble_info *info;
  bfd_vma memaddr;
  unsigned short opcode;
  partemplate *ptm;
  int size;
{
  print_instruction (info, memaddr, opcode, 
                     ptm->name, ptm->operand_types, size, 0);
  info->fprintf_func (info->stream, " || ");
  return print_instruction (info, memaddr, opcode, 
                            ptm->parname, ptm->paroperand_types, size, 0);
}

static int
sprint_dual_address (info, buf, code)
  disassemble_info *info;
  char buf[];
  unsigned short code;
{
  const char *formats[] = {
    "*ar%d",
    "*ar%d-",
    "*ar%d+",
    "*ar%d+0%%",
  };
  return sprintf (buf, formats[XMOD(code)], XARX(code));
}

static int
sprint_indirect_address (info, buf, opcode)
  disassemble_info *info;
  char buf[];
  unsigned short opcode;
{
  const char *formats[] = {
    "*ar%d",
    "*ar%d-",
    "*ar%d+",
    "*+ar%d",
    "*ar%d-0B",
    "*ar%d-0",
    "*ar%d+0",
    "*ar%d+0B",
    "*ar%d-%%",
    "*ar%d-0%%",
    "*ar%d+%%",
    "*ar%d+0%%",
  };
  return sprintf (buf, formats[MOD(opcode)], ARF(opcode));
}

static int
sprint_direct_address (info, buf, opcode)
  disassemble_info *info;
  char buf[];
  unsigned short opcode;
{
  /* FIXME -- look up relocation if available */
  return sprintf (buf, "0x??%02x", (int)(opcode & 0x7F));
}

static int
sprint_mmr (info, buf, mmr)
  disassemble_info *info;
  char buf[];
  int mmr;
{
  symbol *reg = (symbol *)mmregs;
  while (reg->name != NULL)
    {
      if (mmr == reg->value)
        {
          sprintf (buf, "%s", (reg+1)->name);
          return 1;
        }
      ++reg;
    }
  sprintf (buf, "MMR(%d)", mmr); /* FIXME -- different targets.  */
  return 0;
}

static int
sprint_cc2 (info, buf, opcode)
  disassemble_info *info;
  char *buf;
  unsigned short opcode;
{
  const char *cc2[] = {
    "??", "??", "ageq", "alt", "aneq", "aeq", "agt", "aleq",
    "??", "??", "bgeq", "blt", "bneq", "beq", "bgt", "bleq",
  };
  return sprintf (buf, "%s", cc2[opcode & 0xF]);
}

static int
sprint_condition (info, buf, opcode)
  disassemble_info *info;
  char *buf;
  unsigned short opcode;
{
  char *start = buf;
  const char *cmp[] = {
      "??", "??", "geq", "lt", "neq", "eq", "gt", "leq"
  };
  if (opcode & 0x40)
    {
      char acc = (opcode & 0x8) ? 'b' : 'a';
      if (opcode & 0x7)
          buf += sprintf (buf, "%c%s%s", acc, cmp[(opcode&0x7)],
                          (opcode&0x20) ? ", " : "");
      if (opcode & 0x20)
          buf += sprintf (buf, "%c%s", acc, (opcode&0x10) ? "ov" : "nov");
    }
  else if (opcode & 0x3F)
    {
      if (opcode & 0x30)
        buf += sprintf (buf, "%s%s", 
                        ((opcode & 0x30) == 0x30) ? "tc" : "ntc",
                        (opcode & 0x0F) ? ", " : "");
      if (opcode & 0x0C)
        buf += sprintf (buf, "%s%s", 
                        ((opcode & 0x0C) == 0x0C) ? "c" : "nc",
                        (opcode & 0x03) ? ", " : "");
      if (opcode & 0x03)
        buf += sprintf (buf, "%s", 
                        ((opcode & 0x03) == 0x03) ? "bio" : "nbio");
    }
  else
    buf += sprintf (buf, "unc");

  return buf - start;
}
