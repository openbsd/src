/* Print instructions for the Motorola 88000, for GDB and GNU Binutils.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1993
   Free Software Foundation, Inc.
   Contributed by Data General Corporation, November 1989.
   Partially derived from an earlier printcmd.c.

This file is part of GDB and the GNU Binutils.

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

#include "dis-asm.h"
#include "opcode/m88k.h"

INSTAB  *hashtable[HASHVAL] = {0};

static int
m88kdis PARAMS ((bfd_vma, unsigned long, struct disassemble_info *));

static void
printop PARAMS ((struct disassemble_info *, OPSPEC *,
		 unsigned long, bfd_vma, int));

static void
init_disasm PARAMS ((void));

static void
install PARAMS ((INSTAB *instptr));

/*
*		Disassemble an M88000 Instruction
*
*
*       This module decodes the instruction at memaddr.
*
*			Revision History
*
*       Revision 1.0    11/08/85        Creation date by Motorola
*			05/11/89	R. Trawick adapted to GDB interface.
*			07/12/93	Ian Lance Taylor updated to
*					binutils interface.
*/

int
print_insn_m88k (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  bfd_byte buffer[4];
  int status;

  /* Instruction addresses may have low two bits set. Clear them.	*/
  memaddr &=~ (bfd_vma) 3;

  status = (*info->read_memory_func) (memaddr, buffer, 4, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }

  return m88kdis (memaddr, bfd_getb32 (buffer), info);
}

/*
 * disassemble the instruction in 'instruction'.
 * 'pc' should be the address of this instruction, it will
 *   be used to print the target address if this is a relative jump or call
 * the disassembled instruction is written to 'info'.
 * The function returns the length of this instruction in bytes.
 */

static int
m88kdis (pc, instruction, info)
     bfd_vma pc;
     unsigned long instruction;
     struct disassemble_info *info;
{
  static int ihashtab_initialized = 0;
  unsigned int opcode;
  INSTAB *entry_ptr;
  int opmask;
  unsigned int class;

  if (! ihashtab_initialized)
    init_disasm ();

  /* create the appropriate mask to isolate the opcode */
  opmask = DEFMASK;
  class = instruction & DEFMASK;
  if ((class >= SFU0) && (class <= SFU7))
    {
      if (instruction < SFU1)
	opmask = CTRLMASK;
      else
	opmask = SFUMASK;
    }
  else if (class == RRR)
    opmask = RRRMASK;
  else if (class == RRI10)
    opmask = RRI10MASK;

  /* isolate the opcode */
  opcode = instruction & opmask;

  /* search the hash table with the isolated opcode */
  for (entry_ptr = hashtable[opcode % HASHVAL];
       (entry_ptr != NULL) && (entry_ptr->opcode != opcode);
       entry_ptr = entry_ptr->next)
    ;

  if (entry_ptr == NULL)
    (*info->fprintf_func) (info->stream, "word\t%08x", instruction);
  else
    {
      (*info->fprintf_func) (info->stream, "%s", entry_ptr->mnemonic);
      printop (info, &(entry_ptr->op1), instruction, pc, 1);
      printop (info, &(entry_ptr->op2), instruction, pc, 0);
      printop (info, &(entry_ptr->op3), instruction, pc, 0);
    }

  return 4;
}

/*
*                      Decode an Operand of an Instruction
*
*			Functional Description
*
*       This module formats and writes an operand of an instruction to info
*       based on the operand specification.  When the first flag is set this
*       is the first operand of an instruction.  Undefined operand types
*       cause a <dis error> message.
*
*			Parameters
*	disassemble_info	where the operand may be printed
*       OPSPEC  *opptr          Pointer to an operand specification
*       UINT    inst            Instruction from which operand is extracted
*	UINT    pc		PC of instruction; used for pc-relative disp.
*       int     first           Flag which if nonzero indicates the first
*                               operand of an instruction
*
*			Output
*
*       The operand specified is extracted from the instruction and is
*       written to buf in the format specified. The operand is preceded
*       by a comma if it is not the first operand of an instruction and it
*       is not a register indirect form.  Registers are preceded by 'r' and
*       hex values by '0x'.
*
*			Revision History
*
*       Revision 1.0    11/08/85        Creation date
*/

static void
printop (info, opptr, inst, pc, first)
     struct disassemble_info *info;
     OPSPEC *opptr;
     unsigned long inst;
     bfd_vma pc;
     int first;
{
  int extracted_field;
  char *cond_mask_sym;

  if (opptr->width == 0)
    return;

  if (! first)
    {
      switch (opptr->type)
	{
	case REGSC:
	case CONT:
	  break;
	default:
	  (*info->fprintf_func) (info->stream, ",");
	  break;
	}
    }

  switch (opptr->type)
    {
    case CRREG:
      (*info->fprintf_func) (info->stream, "cr%d",
			     UEXT (inst, opptr->offset, opptr->width));
      break;

    case FCRREG:
      (*info->fprintf_func) (info->stream, "fcr%d",
			     UEXT (inst, opptr->offset, opptr->width));
      break;

    case REGSC:
      (*info->fprintf_func) (info->stream, "[r%d]",
			     UEXT (inst, opptr->offset, opptr->width));
      break;

    case REG:
      (*info->fprintf_func) (info->stream, "r%d",
			     UEXT (inst, opptr->offset, opptr->width));
      break;

    case XREG:
      (*info->fprintf_func) (info->stream, "x%d",
			     UEXT (inst, opptr->offset, opptr->width));
      break;

    case HEX:
      extracted_field = UEXT (inst, opptr->offset, opptr->width);
      if (extracted_field == 0)
	(*info->fprintf_func) (info->stream, "0");
      else
	(*info->fprintf_func) (info->stream, "0x%02x", extracted_field);
      break;

    case DEC:
      extracted_field = UEXT (inst, opptr->offset, opptr->width);
      (*info->fprintf_func) (info->stream, "%d", extracted_field);
      break;

    case CONDMASK:
      extracted_field = UEXT (inst, opptr->offset, opptr->width);
      switch (extracted_field & 0x0f)
	{
	case 0x1: cond_mask_sym = "gt0"; break;
	case 0x2: cond_mask_sym = "eq0"; break;
	case 0x3: cond_mask_sym = "ge0"; break;
	case 0xc: cond_mask_sym = "lt0"; break;
	case 0xd: cond_mask_sym = "ne0"; break;
	case 0xe: cond_mask_sym = "le0"; break;
	default: cond_mask_sym = NULL; break;
	}
      if (cond_mask_sym != NULL)
	(*info->fprintf_func) (info->stream, "%s", cond_mask_sym);
      else
	(*info->fprintf_func) (info->stream, "%x", extracted_field);
      break;
			
    case PCREL:
      (*info->print_address_func)
	(pc + (4 * (SEXT (inst, opptr->offset, opptr->width))),
	 info);
      break;

    case CONT:
      (*info->fprintf_func) (info->stream, "%d,r%d",
			     UEXT (inst, opptr->offset, 5),
			     UEXT (inst, (opptr->offset) + 5, 5));
      break;

    case BF:
      (*info->fprintf_func) (info->stream, "%d<%d>",
			      UEXT (inst, (opptr->offset) + 5, 5),
			      UEXT (inst, opptr->offset, 5));
      break;

    default:
      (*info->fprintf_func) (info->stream, "# <dis error: %08x>", inst);
    }
}

/*
*                 Initialize the Disassembler Instruction Table
*
*       Initialize the hash table and instruction table for the disassembler.
*       This should be called once before the first call to disasm().
*
*			Parameters
*
*			Output
*
*       If the debug option is selected, certain statistics about the hashing
*       distribution are written to stdout.
*
*			Revision History
*
*       Revision 1.0    11/08/85        Creation date
*/

static void
init_disasm ()
{
   int i, size;

   for (i = 0; i < HASHVAL; i++)
     hashtable[i] = NULL;

   size = sizeof (instructions) / sizeof (INSTAB);
   for (i = 0; i < size; i++)
     install (&instructions[i]);
}

/*
*       Insert an instruction into the disassembler table by hashing the
*       opcode and inserting it into the linked list for that hash value.
*
*			Parameters
*
*       INSTAB *instptr         Pointer to the entry in the instruction table
*                               to be installed
*
*       Revision 1.0    11/08/85        Creation date
*			05/11/89	R. TRAWICK ADAPTED FROM MOTOROLA
*/

static void
install (instptr)
     INSTAB *instptr;
{
  unsigned int i;

  i = (instptr->opcode) % HASHVAL;
  instptr->next = hashtable[i];
  hashtable[i] = instptr;
}
