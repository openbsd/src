/* evax-etir.c -- BFD back-end for ALPHA EVAX (openVMS/Alpha) files.
   Copyright 1996, 1997 Free Software Foundation, Inc.
   ETIR record handling functions

   go and read the openVMS linker manual (esp. appendix B)
   if you don't know what's going on here :-)

   Written by Klaus K"ampf (kkaempf@progis.de)
   of proGIS Softwareentwicklung, Aachen, Germany

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


/* The following type abbreviations are used:

	cs	counted string (ascii string with length byte)
	by	byte (1 byte)
	sh	short (2 byte, 16 bit)
	lw	longword (4 byte, 32 bit)
	qw	quadword (8 byte, 64 bit)
	da	data stream  */

#include <stdio.h>
#include <ctype.h>

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"

#include "evax.h"

#if 0
static void location_save
  PARAMS ((bfd *abfd, unsigned long index, unsigned long loc, int section));
static unsigned long location_restore
  PARAMS ((bfd *abfd, unsigned long index, int *section));
#endif /* 0 */

static void image_set_ptr PARAMS ((bfd *abfd, int psect, uquad offset));
static void image_inc_ptr PARAMS ((bfd *abfd, uquad offset));
static void image_dump PARAMS ((bfd *abfd, unsigned char *ptr, int size, int offset));
static void image_write_b PARAMS ((bfd *abfd, unsigned int value));
static void image_write_w PARAMS ((bfd *abfd, unsigned int value));
static void image_write_l PARAMS ((bfd *abfd, unsigned long value));
static void image_write_q PARAMS ((bfd *abfd, uquad value));

/*-----------------------------------------------------------------------------*/

#if 0

/* Save location counter at index  */

static void
location_save (abfd, index, loc, section)
     bfd *abfd;
     unsigned long index;
     unsigned long loc;
     int section;
{
  PRIV(location_stack)[index].value = loc;
  PRIV(location_stack)[index].psect = section;

  return;
}

/* Restore location counter from index  */

static unsigned long
location_restore (abfd, index, section)
     bfd *abfd;
     unsigned long index;
     int *section;
{
  if (section != NULL)
    *section = PRIV(location_stack)[index].psect;
  return PRIV(location_stack)[index].value;
}

#endif /* 0 */

/* routines to fill sections contents during etir read */

/* Initialize image buffer pointer to be filled  */

static void
image_set_ptr (abfd, psect, offset)
     bfd *abfd;
     int psect;
     uquad offset;
{
#if EVAX_DEBUG
  evax_debug (4, "image_set_ptr(%d=%s, %d)\n",
		psect, PRIV(sections)[psect]->name, offset);
#endif

  PRIV(image_ptr) = PRIV(sections)[psect]->contents + offset;
  return;
}


/* Increment image buffer pointer by offset  */

static void
image_inc_ptr (abfd, offset)
     bfd *abfd;
     uquad offset;
{
#if EVAX_DEBUG
  evax_debug (4, "image_inc_ptr(%d)\n", offset);
#endif

  PRIV(image_ptr) += offset;

  return;
}


/* Dump multiple bytes to section image  */

static void
image_dump (abfd, ptr, size, offset)
    bfd *abfd;
    unsigned char *ptr;
    int size;
    int offset;
{
#if EVAX_DEBUG
  evax_debug (6, "image_dump from (%p, %d) to (%p)\n", ptr, size, PRIV(image_ptr));
  _bfd_hexdump (7, ptr, size, offset);
#endif

  while (size-- > 0)
    *PRIV(image_ptr)++ = *ptr++;
  return;
}


/* Write byte to section image  */

static void
image_write_b (abfd, value)
     bfd *abfd;
     unsigned int value;
{
#if EVAX_DEBUG
  evax_debug (6, "image_write_b(%02x)\n", (int)value);
#endif

  *PRIV(image_ptr)++ = (value & 0xff);
  return;
}


/* Write 2-byte word to image  */

static void
image_write_w (abfd, value)
     bfd *abfd;
     unsigned int value;
{
#if EVAX_DEBUG
  evax_debug (6, "image_write_w(%04x)\n", (int)value);
#endif

  bfd_putl16 (value, PRIV(image_ptr));
  PRIV(image_ptr) += 2;

  return;
}


/* Write 4-byte long to image  */

static void
image_write_l (abfd, value)
     bfd *abfd;
     unsigned long value;
{
#if EVAX_DEBUG
  evax_debug (6, "image_write_l(%08lx)\n", value);
#endif

  bfd_putl32 (value, PRIV(image_ptr));
  PRIV(image_ptr) += 4;

  return;
}


/* Write 4-byte long to image  */

static void
image_write_q (abfd, value)
     bfd *abfd;
     uquad value;
{
#if EVAX_DEBUG
  evax_debug (6, "image_write_q(%016lx)\n", value);
#endif

  bfd_putl64 (value, PRIV(image_ptr));
  PRIV(image_ptr) += 8;

  return;
}


#define HIGHBIT(op) ((op & 0x80000000L) == 0x80000000L)

/* etir_sta
  
   evax stack commands
  
   handle sta_xxx commands in etir section
   ptr points to data area in record
  
   see table B-8 of the openVMS linker manual  */

static boolean
etir_sta (abfd, cmd, ptr)
     bfd *abfd;
     int cmd;
     unsigned char *ptr;
{

  switch (cmd)
    {
      /* stack */

      /* stack global
	   arg: cs	symbol name

	   stack 32 bit value of symbol (high bits set to 0)  */

      case ETIR_S_C_STA_GBL:
	{
	  char *name;
	  evax_symbol_entry *entry;

	  name = _bfd_evax_save_counted_string ((char *)ptr);
	  entry = (evax_symbol_entry *)
		  bfd_hash_lookup (PRIV(evax_symbol_table), name, false, false);
	  if (entry == (evax_symbol_entry *)NULL)
	    {
#if EVAX_DEBUG
	      evax_debug (3, "ETIR_S_C_STA_GBL: no symbol \"%s\"\n", name);
#endif
	      return false;
	    }
	  else
	    {
	      _bfd_evax_push (abfd, (uquad)(entry->symbol->value), -1);
	    }
	}
      break;

	/* stack longword
	   arg: lw	value

	   stack 32 bit value, sign extend to 64 bit  */

      case ETIR_S_C_STA_LW:
	_bfd_evax_push (abfd, (uquad)bfd_getl32 (ptr), -1);
      break;

	/* stack global
	   arg: qw	value

	   stack 64 bit value of symbol	 */

      case ETIR_S_C_STA_QW:
	_bfd_evax_push (abfd, (uquad)bfd_getl64(ptr), -1);
      break;

	/* stack psect base plus quadword offset
	   arg: lw	section index
	  	qw	signed quadword offset (low 32 bits)

	   stack qw argument and section index
	   (see ETIR_S_C_STO_OFF, ETIR_S_C_CTL_SETRB)  */

      case ETIR_S_C_STA_PQ:
  	{
	  uquad dummy;
	  int psect;

	  psect = bfd_getl32 (ptr);
	  if (psect >= PRIV(egsd_sec_count))
	    {
	      (*_bfd_error_handler) ("Bad section index in ETIR_S_C_STA_PQ");
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	  dummy = bfd_getl64 (ptr+4);
	  _bfd_evax_push (abfd, dummy, psect);
        }
      break;

	/* all not supported  */

      case ETIR_S_C_STA_LI:
      case ETIR_S_C_STA_MOD:
      case ETIR_S_C_STA_CKARG:

	(*_bfd_error_handler) ("Unsupported STA cmd %d", cmd);
	return false;
      break;

      default:
	(*_bfd_error_handler) ("Reserved STA cmd %d", cmd);
	return false;
      break;
  }
  return true;
}


/*
   etir_sto
  
   evax store commands
  
   handle sto_xxx commands in etir section
   ptr points to data area in record
  
   see table B-9 of the openVMS linker manual  */

static boolean
etir_sto (abfd, cmd, ptr)
     bfd *abfd;
     int cmd;
     unsigned char *ptr;
{
  uquad dummy;
  int psect;

  switch (cmd)
    {

      /* store byte: pop stack, write byte
	 arg: -  */

    case ETIR_S_C_STO_B:
      dummy = _bfd_evax_pop (abfd, &psect);
#if 0
      if (is_share)		/* FIXME */
	(*_bfd_error_handler) ("ETIR_S_C_STO_B: byte fixups not supported");
#endif
      image_write_b (abfd, dummy & 0xff);	/* FIXME: check top bits */
      break;

      /* store word: pop stack, write word
	 arg: -  */

    case ETIR_S_C_STO_W:
      dummy = _bfd_evax_pop (abfd, &psect);
#if 0
      if (is_share)		/* FIXME */
	(*_bfd_error_handler) ("ETIR_S_C_STO_B: word fixups not supported");
#endif
      image_write_w (abfd, dummy & 0xffff);	/* FIXME: check top bits */
      break;

      /* store longword: pop stack, write longword
	 arg: -  */

    case ETIR_S_C_STO_LW:
      dummy = _bfd_evax_pop (abfd, &psect);
      dummy += (PRIV(sections)[psect])->vma;
      image_write_l (abfd, dummy & 0xffffffff);/* FIXME: check top bits */
#if 0				/* FIXME */
      if (is_rel)
	evax_debug (3, "ETIR_S_C_STO_LW: Relocation !\n");
      if (is_share)
	evax_debug (3, "ETIR_S_C_STO_LW: Fix-up share !\n");
#endif
      break;

      /* store quadword: pop stack, write quadword
	 arg: -  */

    case ETIR_S_C_STO_QW:
      dummy = _bfd_evax_pop (abfd, &psect);
      dummy += (PRIV(sections)[psect])->vma;
      image_write_q(abfd, dummy);		/* FIXME: check top bits */
#if 0				/* FIXME */
      if (is_rel)
	evax_debug (3, "ETIR_S_C_STO_LW: Relocation !\n");
      if (is_share)
	evax_debug (3, "ETIR_S_C_STO_LW: Fix-up share !\n");
#endif
      break;

      /* store immediate repeated: pop stack for repeat count
	 arg: lw	byte count
	 da	data  */

    case ETIR_S_C_STO_IMMR:
      {
	unsigned long size;

	size = bfd_getl32 (ptr);
	dummy = (unsigned long)_bfd_evax_pop (abfd, NULL);
	while (dummy-- > 0L)
	  image_dump (abfd, ptr+4, size, 0);
      }
      break;

      /* store global: write symbol value
	 arg: cs	global symbol name  */

    case ETIR_S_C_STO_GBL:
      {
	evax_symbol_entry *entry;
	char *name;

	name = _bfd_evax_save_counted_string ((char *)ptr);
	entry = (evax_symbol_entry *)bfd_hash_lookup (PRIV(evax_symbol_table), name, false, false);
	if (entry == (evax_symbol_entry *)NULL)
	  {
	    (*_bfd_error_handler) ("ETIR_S_C_STO_GBL: no symbol \"%s\"",
				   name);
	    return false;
	  }
	else
	  image_write_q (abfd, (uquad)(entry->symbol->value));	/* FIXME, reloc */
      }
      break;

      /* store code address: write address of entry point
	 arg: cs	global symbol name (procedure)  */

    case ETIR_S_C_STO_CA:
      {
	evax_symbol_entry *entry;
	char *name;

	name = _bfd_evax_save_counted_string ((char *)ptr);
	entry = (evax_symbol_entry *) bfd_hash_lookup (PRIV(evax_symbol_table), name, false, false);
	if (entry == (evax_symbol_entry *)NULL)
	  {
	    (*_bfd_error_handler) ("ETIR_S_C_STO_CA: no symbol \"%s\"",
				   name);
	    return false;
	  }
	else
	  image_write_q (abfd, (uquad)(entry->symbol->value));	/* FIXME, reloc */
      }
      break;

      /* not supported  */

    case ETIR_S_C_STO_RB:
    case ETIR_S_C_STO_AB:
      (*_bfd_error_handler) ("ETIR_S_C_STO_RB/AB: Not supported");
      break;

    /* store offset to psect: pop stack, add low 32 bits to base of psect
       arg: -  */

    case ETIR_S_C_STO_OFF:
      {
	uquad q;
	int psect;

	q = _bfd_evax_pop (abfd, &psect);
	q += (PRIV(sections)[psect])->vma;
	image_write_q (abfd, q);
      }
      break;

      /* store immediate
	 arg: lw	count of bytes
	 da	data  */

    case ETIR_S_C_STO_IMM:
      {
	int size;

	size = bfd_getl32 (ptr);
	image_dump (abfd, ptr+4, size, 0);
      }
      break;

      /* this code is 'reserved to digital' according to the openVMS linker manual,
	 however it is generated by the DEC C compiler and defined in the include file.
	 FIXME, since the following is just a guess
	 store global longword: store 32bit value of symbol
	 arg: cs	symbol name  */

    case ETIR_S_C_STO_GBL_LW:
      {
	evax_symbol_entry *entry;
	char *name;

	name = _bfd_evax_save_counted_string ((char *)ptr);
	entry = (evax_symbol_entry *)bfd_hash_lookup (PRIV(evax_symbol_table), name, false, false);
	if (entry == (evax_symbol_entry *)NULL)
	  {
#if EVAX_DEBUG
	    evax_debug (3, "ETIR_S_C_STO_GBL_LW: no symbol \"%s\"\n", name);
#endif
	    return false;
	  }
	else
	  image_write_l (abfd, (unsigned long)(entry->symbol->value));	/* FIXME, reloc */
      }
      break;

      /* not supported  */

    case ETIR_S_C_STO_LP_PSB:
      (*_bfd_error_handler) ("ETIR_S_C_STO_LP_PSB: Not supported");
      break;

    /* */

    case ETIR_S_C_STO_HINT_GBL:
      (*_bfd_error_handler) ("ETIR_S_C_STO_HINT_GBL: not implemented");
      break;

    /* */

    case ETIR_S_C_STO_HINT_PS:
      (*_bfd_error_handler) ("ETIR_S_C_STO_HINT_PS: not implemented");
      break;

    default:
      (*_bfd_error_handler) ("Reserved STO cmd %d", cmd);
      break;
    }

  return true;
}

/* stack operator commands
   all 32 bit signed arithmetic
   all word just like a stack calculator
   arguments are popped from stack, results are pushed on stack
  
   see table B-10 of the openVMS linker manual  */

static boolean
etir_opr (abfd, cmd, ptr)
     bfd *abfd;
     int cmd;
     unsigned char *ptr;
{
  long op1, op2;

  switch (cmd)
    {
      /* operation */

      /* no-op  */

    case ETIR_S_C_OPR_NOP:
      break;

      /* add  */

    case ETIR_S_C_OPR_ADD:
      op1 = (long)_bfd_evax_pop (abfd, NULL);
      op2 = (long)_bfd_evax_pop (abfd, NULL);
      _bfd_evax_push (abfd, (uquad)(op1 + op2), -1);
      break;

      /* subtract  */

    case ETIR_S_C_OPR_SUB:
      op1 = (long)_bfd_evax_pop (abfd, NULL);
      op2 = (long)_bfd_evax_pop (abfd, NULL);
      _bfd_evax_push (abfd, (uquad)(op2 - op1), -1);
      break;

      /* multiply  */

    case ETIR_S_C_OPR_MUL:
      op1 = (long)_bfd_evax_pop (abfd, NULL);
      op2 = (long)_bfd_evax_pop (abfd, NULL);
      _bfd_evax_push (abfd, (uquad)(op1 * op2), -1);
      break;

      /* divide  */

    case ETIR_S_C_OPR_DIV:
      op1 = (long)_bfd_evax_pop (abfd, NULL);
      op2 = (long)_bfd_evax_pop (abfd, NULL);
      if (op2 == 0)
	_bfd_evax_push (abfd, (uquad)0L, -1);
      else
	_bfd_evax_push (abfd, (uquad)(op2 / op1), -1);
      break;

      /* logical and  */

    case ETIR_S_C_OPR_AND:
      op1 = (long)_bfd_evax_pop (abfd, NULL);
      op2 = (long)_bfd_evax_pop (abfd, NULL);
      _bfd_evax_push (abfd, (uquad)(op1 & op2), -1);
      break;

      /* logical inclusive or	 */

    case ETIR_S_C_OPR_IOR:
      op1 = (long)_bfd_evax_pop (abfd, NULL);
      op2 = (long)_bfd_evax_pop (abfd, NULL);
      _bfd_evax_push (abfd, (uquad)(op1 | op2), -1);
      break;

      /* logical exclusive or  */

    case ETIR_S_C_OPR_EOR:
      op1 = (long)_bfd_evax_pop (abfd, NULL);
      op2 = (long)_bfd_evax_pop (abfd, NULL);
      _bfd_evax_push (abfd, (uquad)(op1 ^ op2), -1);
      break;

      /* negate  */

    case ETIR_S_C_OPR_NEG:
      op1 = (long)_bfd_evax_pop (abfd, NULL);
      _bfd_evax_push (abfd, (uquad)(-op1), -1);
      break;

      /* complement  */

    case ETIR_S_C_OPR_COM:
      op1 = (long)_bfd_evax_pop (abfd, NULL);
      _bfd_evax_push (abfd, (uquad)(op1 ^ -1L), -1);
      break;

      /* insert field  */

    case ETIR_S_C_OPR_INSV:
      (void)_bfd_evax_pop (abfd, NULL);
      (*_bfd_error_handler) ("ETIR_S_C_OPR_INSV: Not supported");
      break;

    /* arithmetic shift  */

    case ETIR_S_C_OPR_ASH:
      op1 = (long)_bfd_evax_pop (abfd, NULL);
      op2 = (long)_bfd_evax_pop (abfd, NULL);
      if (op2 < 0)		/* shift right */
	op1 >>= -op2;
      else			/* shift left */
	op1 <<= op2;
      _bfd_evax_push (abfd, (uquad)op1, -1);
      break;

      /* unsigned shift  */

    case ETIR_S_C_OPR_USH:
      (*_bfd_error_handler) ("ETIR_S_C_OPR_USH: Not supported");
      break;

      /* rotate  */

    case ETIR_S_C_OPR_ROT:
      (*_bfd_error_handler) ("ETIR_S_C_OPR_ROT: Not supported");
      break;

      /* select  */

    case ETIR_S_C_OPR_SEL:
      if ((long)_bfd_evax_pop (abfd, NULL) & 0x01L)
	(void)_bfd_evax_pop (abfd, NULL);
      else
	{
	  op1 = (long)_bfd_evax_pop (abfd, NULL);
	  (void)_bfd_evax_pop (abfd, NULL);
	  _bfd_evax_push (abfd, (uquad)op1, -1);
	}
      break;

      /* redefine symbol to current location  */

    case ETIR_S_C_OPR_REDEF:
      (*_bfd_error_handler) ("ETIR_S_C_OPR_REDEF: Not supported");
      break;

      /* define a literal  */

    case ETIR_S_C_OPR_DFLIT:
      (*_bfd_error_handler) ("ETIR_S_C_OPR_DFLIT: Not supported");
      break;

    default:
      (*_bfd_error_handler) ("Reserved OPR cmd %d", cmd);
      break;
    }

  return true;
}


/* control commands
  
   see table B-11 of the openVMS linker manual  */

static boolean
etir_ctl (abfd, cmd, ptr)
     bfd *abfd;
     int cmd;
     unsigned char *ptr;
{
  uquad	 dummy;
  int psect;

  switch (cmd)
    {
      /* set relocation base: pop stack, set image location counter
	 arg: -  */

    case ETIR_S_C_CTL_SETRB:
      dummy = _bfd_evax_pop (abfd, &psect);
      image_set_ptr (abfd, psect, dummy);
      break;

      /* augment relocation base: increment image location counter by offset
	 arg: lw	offset value  */

    case ETIR_S_C_CTL_AUGRB:
      dummy = bfd_getl32 (ptr);
      image_inc_ptr (abfd, dummy);
      break;

      /* define location: pop index, save location counter under index
	 arg: -  */

    case ETIR_S_C_CTL_DFLOC:
      dummy = _bfd_evax_pop (abfd, NULL);
      /* FIXME */
      break;

      /* set location: pop index, restore location counter from index
	 arg: -  */

    case ETIR_S_C_CTL_STLOC:
      dummy = _bfd_evax_pop (abfd, &psect);
      /* FIXME */
      break;

      /* stack defined location: pop index, push location counter from index
	 arg: -  */

    case ETIR_S_C_CTL_STKDL:
      dummy = _bfd_evax_pop (abfd, &psect);
      /* FIXME */
      break;

    default:
      (*_bfd_error_handler) ("Reserved CTL cmd %d", cmd);
      break;
    }
  return true;
}


/* store conditional commands
  
   see table B-12 and B-13 of the openVMS linker manual  */

static boolean
etir_stc (abfd, cmd, ptr)
     bfd *abfd;
     int cmd;
     unsigned char *ptr;
{

  switch (cmd)
    {
      /* 200 Store-conditional Linkage Pair
	 arg:  */

    case ETIR_S_C_STC_LP:
      (*_bfd_error_handler) ("ETIR_S_C_STC_LP: not supported");
      break;

      /* 201 Store-conditional Linkage Pair with Procedure Signature
	 arg:	lw	linkage index
	 cs	procedure name
	 by	signature length
	 da	signature  */

    case ETIR_S_C_STC_LP_PSB:
      image_inc_ptr (abfd, 16);	/* skip entry,procval */
      break;

      /* 202 Store-conditional Address at global address
	 arg:	lw	linkage index
	 cs	global name  */

    case ETIR_S_C_STC_GBL:
      (*_bfd_error_handler) ("ETIR_S_C_STC_GBL: not supported");
      break;

      /* 203 Store-conditional Code Address at global address
	 arg:	lw	linkage index
	 cs	procedure name  */

    case ETIR_S_C_STC_GCA:
      (*_bfd_error_handler) ("ETIR_S_C_STC_GCA: not supported");
      break;

      /* 204 Store-conditional Address at psect + offset
	 arg:	lw	linkage index
	 lw	psect index
	 qw	offset  */

    case ETIR_S_C_STC_PS:
      (*_bfd_error_handler) ("ETIR_S_C_STC_PS: not supported");
      break;

      /* 205 Store-conditional NOP at address of global
	 arg:  */

    case ETIR_S_C_STC_NOP_GBL:

      /* 206 Store-conditional NOP at pect + offset
	 arg:  */

    case ETIR_S_C_STC_NOP_PS:

      /* 207 Store-conditional BSR at global address
	 arg:  */

    case ETIR_S_C_STC_BSR_GBL:

      /* 208 Store-conditional BSR at pect + offset
	 arg:  */

    case ETIR_S_C_STC_BSR_PS:

      /* 209 Store-conditional LDA at global address
	 arg:  */

    case ETIR_S_C_STC_LDA_GBL:

      /* 210 Store-conditional LDA at psect + offset
	 arg:  */

    case ETIR_S_C_STC_LDA_PS:

      /* 211 Store-conditional BSR or Hint at global address
	 arg:  */

    case ETIR_S_C_STC_BOH_GBL:

      /* 212 Store-conditional BSR or Hint at pect + offset
	 arg:  */

    case ETIR_S_C_STC_BOH_PS:

      /* 213 Store-conditional NOP,BSR or HINT at global address
	 arg:  */

    case ETIR_S_C_STC_NBH_GBL:

      /* 214 Store-conditional NOP,BSR or HINT at psect + offset
	 arg:  */

    case ETIR_S_C_STC_NBH_PS:
/* FIXME     (*_bfd_error_handler) ("ETIR_S_C_STC_xx: (%d) not supported", cmd); */
      break;

    default:
#if EVAX_DEBUG
      evax_debug (3,  "Reserved STC cmd %d", cmd);
#endif
      break;
    }
  return true;
}


/* handle command from ETIR section  */

static boolean
tir_cmd (abfd, cmd, ptr)
     bfd *abfd;
     int cmd;
     unsigned char *ptr;
{
  static struct {
    int mincod;
    int maxcod;
    boolean (*explain) PARAMS((bfd *, int, unsigned char *));
  } tir_table[] = {
    { ETIR_S_C_MINSTACOD, ETIR_S_C_MAXSTACOD, etir_sta },
    { ETIR_S_C_MINSTOCOD, ETIR_S_C_MAXSTOCOD, etir_sto },
    { ETIR_S_C_MINOPRCOD, ETIR_S_C_MAXOPRCOD, etir_opr },
    { ETIR_S_C_MINCTLCOD, ETIR_S_C_MAXCTLCOD, etir_ctl },
    { ETIR_S_C_MINSTCCOD, ETIR_S_C_MAXSTCCOD, etir_stc },
    { -1, -1, NULL }
  };

  int i = 0;
  boolean res = true;

  while (tir_table[i].mincod >= 0)
    {
      if ( (tir_table[i].mincod <= cmd) 
	&& (cmd <= tir_table[i].maxcod))
	{
	  res = tir_table[i].explain (abfd, cmd, ptr);
	  break;
	}
      i++;
    }

  return res;
}


/* Text Information and Relocation Records (OBJ$C_TIR)
   handle etir record  */

static boolean
analyze_etir (abfd, ptr, length)
     bfd *abfd;
     unsigned char *ptr;
     unsigned int length;
{
  int cmd;
  unsigned char *maxptr;
  boolean res = true;

  maxptr = ptr + length;

  while (ptr < maxptr)
    {
      cmd = bfd_getl16 (ptr);
      length = bfd_getl16 (ptr + 2);
      res = tir_cmd (abfd, cmd, ptr+4);
      if (!res)
	break;
      ptr += length;
    }
  return res;
}


/* process ETIR record
  
   return 0 on success, -1 on error  */

int
_bfd_evax_slurp_etir (abfd)
     bfd *abfd;
{

#if EVAX_DEBUG
  evax_debug (2, "ETIR\n");
#endif

  PRIV(evax_rec) += 4;	/* skip type, size */
  PRIV(rec_size) -= 4;
  if (analyze_etir (abfd, PRIV(evax_rec), PRIV(rec_size)))
    return 0;

  return -1;
}


/* process EDBG record
   return 0 on success, -1 on error
  
   not implemented yet  */

int
_bfd_evax_slurp_edbg (abfd)
     bfd *abfd;
{
#if EVAX_DEBUG
  evax_debug (2, "EDBG\n");
#endif

  abfd->flags |= (HAS_DEBUG | HAS_LINENO);
  return 0;
}


/* process ETBT record
   return 0 on success, -1 on error
  
   not implemented yet  */

int
_bfd_evax_slurp_etbt (abfd)
     bfd *abfd;
{
#if EVAX_DEBUG
  evax_debug (2, "ETBT\n");
#endif

  return 0;
}

/*----------------------------------------------------------------------*/
/*									*/
/*	WRITE ETIR SECTION						*/
/*									*/
/*	this is still under construction and therefore not documented	*/
/*									*/
/*----------------------------------------------------------------------*/

static void start_etir_record PARAMS ((bfd *abfd, int index, uquad offset, boolean justoffset));
static void sto_imm PARAMS ((bfd *abfd, evax_section *sptr, bfd_vma vaddr, int index));
static void end_etir_record PARAMS ((bfd *abfd));

static void
sto_imm (abfd, sptr, vaddr, index)
     bfd *abfd;
     evax_section *sptr;
     bfd_vma vaddr;
     int index;
{
  int size;
  int ssize;
  unsigned char *cptr;

#if EVAX_DEBUG
  evax_debug (8, "sto_imm %d bytes\n", sptr->size);
  _bfd_hexdump (9, sptr->contents, (int)sptr->size, (int)vaddr);
#endif

  ssize = sptr->size;
  cptr = sptr->contents;

  while (ssize > 0)
    {

      size = ssize;				/* try all the rest */

      if (_bfd_evax_output_check (abfd, size) < 0)
	{					/* doesn't fit, split ! */
	  end_etir_record (abfd);
	  start_etir_record (abfd, index, vaddr, false);
	  size = _bfd_evax_output_check (abfd, 0);	/* get max size */
	  if (size > ssize)			/* more than what's left ? */
	    size = ssize;
	}

      _bfd_evax_output_begin (abfd, ETIR_S_C_STO_IMM, -1);
      _bfd_evax_output_long (abfd, (unsigned long)(size));
      _bfd_evax_output_dump (abfd, cptr, size);
      _bfd_evax_output_flush (abfd);

#if EVAX_DEBUG
      evax_debug (10, "dumped %d bytes\n", size);
      _bfd_hexdump (10, cptr, (int)size, (int)vaddr);
#endif

      vaddr += size;
      ssize -= size;
      cptr += size;
    }

  return;
}

/*-------------------------------------------------------------------*/

/* start ETIR record for section #index at virtual addr offset.  */

static void
start_etir_record (abfd, index, offset, justoffset)
    bfd *abfd;
    int index;
    uquad offset;
    boolean justoffset;
{
  if (!justoffset)
    {
      _bfd_evax_output_begin (abfd, EOBJ_S_C_ETIR, -1);	/* one ETIR per section */
      _bfd_evax_output_push (abfd);
    }

  _bfd_evax_output_begin (abfd, ETIR_S_C_STA_PQ, -1);	/* push start offset */
  _bfd_evax_output_long (abfd, (unsigned long)index);
  _bfd_evax_output_quad (abfd, (uquad)offset);
  _bfd_evax_output_flush (abfd);

  _bfd_evax_output_begin (abfd, ETIR_S_C_CTL_SETRB, -1);	/* start = pop() */
  _bfd_evax_output_flush (abfd);

  return;
}


/* end etir record  */
static void
end_etir_record (abfd)
    bfd *abfd;
{
  _bfd_evax_output_pop (abfd);
  _bfd_evax_output_end (abfd); 
}

/* write section contents for bfd abfd  */

int
_bfd_evax_write_etir (abfd)
     bfd *abfd;
{
  asection *section;
  evax_section *sptr;
  int nextoffset;

#if EVAX_DEBUG
  evax_debug (2, "evax_write_etir(%p)\n", abfd);
#endif

  _bfd_evax_output_alignment (abfd, 4);

  nextoffset = 0;
  PRIV(evax_linkage_index) = 1;

  /* dump all other sections  */

  section = abfd->sections;

  while (section != NULL)
    {

#if EVAX_DEBUG
      evax_debug (4, "writing %d. section '%s' (%d bytes)\n", section->index, section->name, (int)(section->_raw_size));
#endif

      if (section->flags & SEC_RELOC)
	{
	  int i;

	  if ((i = section->reloc_count) <= 0)
	    {
	      (*_bfd_error_handler) ("SEC_RELOC with no relocs in section %s",
				     section->name);
	    }
#if EVAX_DEBUG
	  else
	    {
	      arelent **rptr;
	      evax_debug (4, "%d relocations:\n", i);
	      rptr = section->orelocation;
	      while (i-- > 0)
		{
		  evax_debug (4, "sym %s in sec %s, value %08lx, addr %08lx, off %08lx, len %d: %s\n",
			      (*(*rptr)->sym_ptr_ptr)->name,
			      (*(*rptr)->sym_ptr_ptr)->section->name,
			      (long)(*(*rptr)->sym_ptr_ptr)->value,
			      (*rptr)->address, (*rptr)->addend,
			      bfd_get_reloc_size((*rptr)->howto),
			      (*rptr)->howto->name);
		  rptr++;
		}
	    }
#endif
	}

      if (section->flags & SEC_HAS_CONTENTS)
	{
	  bfd_vma vaddr;		/* virtual addr in section */

	  sptr = _bfd_get_evax_section (abfd, section->index);
	  if (sptr == NULL)
	    {
	      bfd_set_error (bfd_error_no_contents);
	      return -1;
	    }

	  vaddr = (bfd_vma)(sptr->offset);

	  start_etir_record (abfd, section->index, (uquad) sptr->offset,
			     false);

	  while (sptr != NULL)				/* one STA_PQ, CTL_SETRB per evax_section */
	    {

	      if (section->flags & SEC_RELOC)			/* check for relocs */
		{
		  arelent **rptr = section->orelocation;
		  int i = section->reloc_count;
		  for (;;)
		    {
		      bfd_size_type addr = (*rptr)->address;
		      int len = bfd_get_reloc_size ((*rptr)->howto);
		      if (sptr->offset < addr)		/* sptr starts before reloc */
			{
			  int before = addr - sptr->offset;
			  if (sptr->size <= before)		/* complete before */
			    {
			      sto_imm (abfd, sptr, vaddr, section->index);
			      vaddr += sptr->size;
			      break;
			    }
			  else				/* partly before */
			    {
			      int after = sptr->size - before;
			      sptr->size = before;
			      sto_imm (abfd, sptr, vaddr, section->index);
			      vaddr += sptr->size;
			      sptr->contents += before;
			      sptr->offset += before;
			      sptr->size = after;
			    }
			}
		      else if (sptr->offset == addr)	/* sptr starts at reloc */
			{
			  asymbol *sym = *(*rptr)->sym_ptr_ptr;
			  asection *sec = sym->section;

			  switch ((*rptr)->howto->type)
			    {
			    case ALPHA_R_IGNORE:
			      break;

			    case ALPHA_R_REFLONG:
			      {
				if (bfd_is_und_section (sym->section))
				  {
				    if (_bfd_evax_output_check (abfd,
								strlen((char *)sym->name))
					< 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, false);
				      }
				    _bfd_evax_output_begin (abfd,
							    ETIR_S_C_STO_GBL_LW,
							    -1);
				    _bfd_evax_output_counted (abfd,
							      _bfd_evax_length_hash_symbol (abfd, sym->name));
				    _bfd_evax_output_flush (abfd);
				  }
				else if (bfd_is_abs_section (sym->section))
				  {
				    if (_bfd_evax_output_check (abfd, 16) < 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, false);
				      }
				    _bfd_evax_output_begin (abfd,
							    ETIR_S_C_STA_LW,
							    -1);
				    _bfd_evax_output_quad (abfd,
							   (uquad)sym->value);
				    _bfd_evax_output_flush (abfd);
				    _bfd_evax_output_begin (abfd,
							    ETIR_S_C_STO_LW,
							    -1);
				    _bfd_evax_output_flush (abfd);
				  }
				else
				  {
				    if (_bfd_evax_output_check (abfd, 32) < 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, false);
				      }
				    _bfd_evax_output_begin (abfd,
							    ETIR_S_C_STA_PQ,
							    -1);
				    _bfd_evax_output_long (abfd,
							   (unsigned long)(sec->index));
				    _bfd_evax_output_quad (abfd,
							   ((uquad)(*rptr)->addend
							    + (uquad)sym->value));
				    _bfd_evax_output_flush (abfd);
				    _bfd_evax_output_begin (abfd,
							    ETIR_S_C_STO_LW,
							    -1);
				    _bfd_evax_output_flush (abfd);
				  }
			      }
			      break;

			    case ALPHA_R_REFQUAD:
			      {
				if (bfd_is_und_section (sym->section))
				  {
				    if (_bfd_evax_output_check (abfd,
								strlen((char *)sym->name))
					< 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, false);
				      }
				    _bfd_evax_output_begin (abfd,
							    ETIR_S_C_STO_GBL,
							    -1);
				    _bfd_evax_output_counted (abfd,
							      _bfd_evax_length_hash_symbol (abfd, sym->name));
				    _bfd_evax_output_flush (abfd);
				  }
				else if (bfd_is_abs_section (sym->section))
				  {
				    if (_bfd_evax_output_check (abfd, 16) < 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, false);
				      }
				    _bfd_evax_output_begin (abfd,
							    ETIR_S_C_STA_QW,
							    -1);
				    _bfd_evax_output_quad (abfd,
							   (uquad)sym->value);
				    _bfd_evax_output_flush (abfd);
				    _bfd_evax_output_begin (abfd,
							    ETIR_S_C_STO_QW,
							    -1);
				    _bfd_evax_output_flush (abfd);
				  }
				else
				  {
				    if (_bfd_evax_output_check (abfd, 32) < 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, false);
				      }
				    _bfd_evax_output_begin (abfd,
							    ETIR_S_C_STA_PQ,
							    -1);
				    _bfd_evax_output_long (abfd,
							   (unsigned long)(sec->index));
				    _bfd_evax_output_quad (abfd,
							   ((uquad)(*rptr)->addend
							    + (uquad)sym->value));
				    _bfd_evax_output_flush (abfd);
				    _bfd_evax_output_begin (abfd,
							    ETIR_S_C_STO_OFF,
							    -1);
				    _bfd_evax_output_flush (abfd);
				  }
			      }
			      break;

			    case ALPHA_R_HINT:
			      {
				int hint_size;

				hint_size = sptr->size;
				sptr->size = len;
				sto_imm (abfd, sptr, vaddr, section->index);
				sptr->size = hint_size;
#if 0
				evax_output_begin(abfd, ETIR_S_C_STO_HINT_GBL, -1);
				evax_output_long(abfd, (unsigned long)(sec->index));
				evax_output_quad(abfd, (uquad)addr);

				evax_output_counted(abfd, _bfd_evax_length_hash_symbol (abfd, sym->name));
				evax_output_flush(abfd);
#endif
			      }
			      break;
			    case ALPHA_R_LINKAGE:
			      {
				if (_bfd_evax_output_check (abfd, 64) < 0)
				  {
				    end_etir_record (abfd);
				    start_etir_record (abfd, section->index,
						       vaddr, false);
				  }
				_bfd_evax_output_begin (abfd,
							ETIR_S_C_STC_LP_PSB,
							-1);
				_bfd_evax_output_long (abfd,
						       (unsigned long)PRIV(evax_linkage_index));
				PRIV(evax_linkage_index) += 2;
				_bfd_evax_output_counted (abfd,
							  _bfd_evax_length_hash_symbol (abfd, sym->name));
				_bfd_evax_output_byte (abfd, 0);
				_bfd_evax_output_flush (abfd);
			      }
			      break;

			    case ALPHA_R_CODEADDR:
			      {
				if (_bfd_evax_output_check (abfd,
							    strlen((char *)sym->name))
				    < 0)
				  {
				    end_etir_record (abfd);
				    start_etir_record (abfd,
						       section->index,
						       vaddr, false);
				  }
				_bfd_evax_output_begin (abfd,
							ETIR_S_C_STO_CA,
							-1);
				_bfd_evax_output_counted (abfd,
							  _bfd_evax_length_hash_symbol (abfd, sym->name));
				_bfd_evax_output_flush (abfd);
			      }
			      break;

			    default:
			      (*_bfd_error_handler) ("Unhandled relocation %s",
						     (*rptr)->howto->name);
			      break;
			    }

			  vaddr += len;

			  if (len == sptr->size)
			    {
			      break;
			    }
			  else
			    {
			      sptr->contents += len;
			      sptr->offset += len;
			      sptr->size -= len;
			      i--;
			      rptr++;
			    }
			}
		      else					/* sptr starts after reloc */
			{
			  i--;				/* check next reloc */
			  rptr++;
			}

		      if (i==0)				/* all reloc checked */
			{
			  if (sptr->size > 0)
			    {
			      sto_imm (abfd, sptr, vaddr, section->index);	/* dump rest */
			      vaddr += sptr->size;
			    }
			  break;
			}
		    } /* for (;;) */
		} /* if SEC_RELOC */
	      else						/* no relocs, just dump */
		{
		  sto_imm (abfd, sptr, vaddr, section->index);
		  vaddr += sptr->size;
		}

	      sptr = sptr->next;

	    } /* while (sptr != 0) */

	  end_etir_record (abfd);

	} /* has_contents */

      section = section->next;
    }

  _bfd_evax_output_alignment(abfd, 2);
  return 0;
}


/* write traceback data for bfd abfd  */

int
_bfd_evax_write_etbt (abfd)
     bfd *abfd;
{
#if EVAX_DEBUG
  evax_debug (2, "evax_write_etbt(%p)\n", abfd);
#endif

  return 0;
}


/* write debug info for bfd abfd  */

int
_bfd_evax_write_edbg (abfd)
     bfd *abfd;
{
#if EVAX_DEBUG
  evax_debug (2, "evax_write_edbg(%p)\n", abfd);
#endif

  return 0;
}
