/* Target-dependent code for the HP PA architecture, for GDB.

   Copyright 1986, 1987, 1989, 1990, 1991, 1992, 1993, 1994, 1995,
   1996, 1998, 1999, 2000, 2001, 2002, 2003, 2004 Free Software
   Foundation, Inc.

   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "frame.h"
#include "bfd.h"
#include "inferior.h"
#include "value.h"
#include "regcache.h"
#include "completer.h"
#include "language.h"
#include "osabi.h"
#include "gdb_assert.h"
#include "infttrace.h"
#include "arch-utils.h"
/* For argument passing to the inferior */
#include "symtab.h"
#include "infcall.h"
#include "dis-asm.h"
#include "trad-frame.h"
#include "frame-unwind.h"
#include "frame-base.h"

#ifdef USG
#include <sys/types.h>
#endif

#include <dl.h>
#include <sys/param.h>
#include <signal.h>

#include <sys/ptrace.h>
#include <machine/save_state.h>

#ifdef COFF_ENCAPSULATE
#include "a.out.encap.h"
#else
#endif

/*#include <sys/user.h>         After a.out.h  */
#include <sys/file.h>
#include "gdb_stat.h"
#include "gdb_wait.h"

#include "gdbcore.h"
#include "gdbcmd.h"
#include "target.h"
#include "symfile.h"
#include "objfiles.h"
#include "hppa-tdep.h"

/* Some local constants.  */
static const int hppa32_num_regs = 128;
static const int hppa64_num_regs = 96;

static const int hppa64_call_dummy_breakpoint_offset = 22 * 4;

/* DEPRECATED_CALL_DUMMY_LENGTH is computed based on the size of a
   word on the target machine, not the size of an instruction.  Since
   a word on this target holds two instructions we have to divide the
   instruction size by two to get the word size of the dummy.  */
static const int hppa32_call_dummy_length = INSTRUCTION_SIZE * 28;
static const int hppa64_call_dummy_length = INSTRUCTION_SIZE * 26 / 2;

/* Get at various relevent fields of an instruction word. */
#define MASK_5 0x1f
#define MASK_11 0x7ff
#define MASK_14 0x3fff
#define MASK_21 0x1fffff

/* Define offsets into the call dummy for the target function address.
   See comments related to CALL_DUMMY for more info.  */
#define FUNC_LDIL_OFFSET (INSTRUCTION_SIZE * 9)
#define FUNC_LDO_OFFSET (INSTRUCTION_SIZE * 10)

/* Define offsets into the call dummy for the _sr4export address.
   See comments related to CALL_DUMMY for more info.  */
#define SR4EXPORT_LDIL_OFFSET (INSTRUCTION_SIZE * 12)
#define SR4EXPORT_LDO_OFFSET (INSTRUCTION_SIZE * 13)

/* To support detection of the pseudo-initial frame
   that threads have. */
#define THREAD_INITIAL_FRAME_SYMBOL  "__pthread_exit"
#define THREAD_INITIAL_FRAME_SYM_LEN  sizeof(THREAD_INITIAL_FRAME_SYMBOL)

/* Sizes (in bytes) of the native unwind entries.  */
#define UNWIND_ENTRY_SIZE 16
#define STUB_UNWIND_ENTRY_SIZE 8

static int get_field (unsigned word, int from, int to);

static int extract_5_load (unsigned int);

static unsigned extract_5R_store (unsigned int);

static unsigned extract_5r_store (unsigned int);

static void hppa_frame_init_saved_regs (struct frame_info *frame);

static void find_dummy_frame_regs (struct frame_info *, CORE_ADDR *);

static int find_proc_framesize (CORE_ADDR);

static int find_return_regnum (CORE_ADDR);

struct unwind_table_entry *find_unwind_entry (CORE_ADDR);

static int extract_17 (unsigned int);

static unsigned deposit_21 (unsigned int, unsigned int);

static int extract_21 (unsigned);

static unsigned deposit_14 (int, unsigned int);

static int extract_14 (unsigned);

static void unwind_command (char *, int);

static int low_sign_extend (unsigned int, unsigned int);

static int sign_extend (unsigned int, unsigned int);

static int restore_pc_queue (CORE_ADDR *);

static int hppa_alignof (struct type *);

static int prologue_inst_adjust_sp (unsigned long);

static int is_branch (unsigned long);

static int inst_saves_gr (unsigned long);

static int inst_saves_fr (unsigned long);

static int pc_in_interrupt_handler (CORE_ADDR);

static int pc_in_linker_stub (CORE_ADDR);

static int compare_unwind_entries (const void *, const void *);

static void read_unwind_info (struct objfile *);

static void internalize_unwinds (struct objfile *,
				 struct unwind_table_entry *,
				 asection *, unsigned int,
				 unsigned int, CORE_ADDR);
static void pa_print_registers (char *, int, int);
static void pa_strcat_registers (char *, int, int, struct ui_file *);
static void pa_register_look_aside (char *, int, long *);
static void pa_print_fp_reg (int);
static void pa_strcat_fp_reg (int, struct ui_file *, enum precision_type);
static void record_text_segment_lowaddr (bfd *, asection *, void *);
/* FIXME: brobecker 2002-11-07: We will likely be able to make the
   following functions static, once we hppa is partially multiarched.  */
int hppa_reg_struct_has_addr (int gcc_p, struct type *type);
CORE_ADDR hppa_skip_prologue (CORE_ADDR pc);
CORE_ADDR hppa_skip_trampoline_code (CORE_ADDR pc);
int hppa_in_solib_call_trampoline (CORE_ADDR pc, char *name);
int hppa_in_solib_return_trampoline (CORE_ADDR pc, char *name);
CORE_ADDR hppa_saved_pc_after_call (struct frame_info *frame);
int hppa_inner_than (CORE_ADDR lhs, CORE_ADDR rhs);
CORE_ADDR hppa64_stack_align (CORE_ADDR sp);
int hppa_pc_requires_run_before_use (CORE_ADDR pc);
int hppa_instruction_nullified (void);
int hppa_register_raw_size (int reg_nr);
int hppa_register_byte (int reg_nr);
struct type * hppa32_register_virtual_type (int reg_nr);
struct type * hppa64_register_virtual_type (int reg_nr);
void hppa_store_struct_return (CORE_ADDR addr, CORE_ADDR sp);
void hppa64_extract_return_value (struct type *type, char *regbuf,
                                  char *valbuf);
int hppa64_use_struct_convention (int gcc_p, struct type *type);
void hppa64_store_return_value (struct type *type, char *valbuf);
int hppa_cannot_store_register (int regnum);
void hppa_init_extra_frame_info (int fromleaf, struct frame_info *frame);
CORE_ADDR hppa_frame_chain (struct frame_info *frame);
int hppa_frame_chain_valid (CORE_ADDR chain, struct frame_info *thisframe);
int hppa_frameless_function_invocation (struct frame_info *frame);
CORE_ADDR hppa_frame_saved_pc (struct frame_info *frame);
CORE_ADDR hppa_frame_args_address (struct frame_info *fi);
int hppa_frame_num_args (struct frame_info *frame);
void hppa_push_dummy_frame (void);
void hppa_pop_frame (void);
CORE_ADDR hppa_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR fun,
                               int nargs, struct value **args,
                               struct type *type, int gcc_p);
CORE_ADDR hppa_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
		               int struct_return, CORE_ADDR struct_addr);
CORE_ADDR hppa_smash_text_address (CORE_ADDR addr);
CORE_ADDR hppa_target_read_pc (ptid_t ptid);
void hppa_target_write_pc (CORE_ADDR v, ptid_t ptid);
CORE_ADDR hppa_target_read_fp (void);

typedef struct
  {
    struct minimal_symbol *msym;
    CORE_ADDR solib_handle;
    CORE_ADDR return_val;
  }
args_for_find_stub;

static int cover_find_stub_with_shl_get (void *);

static int is_pa_2 = 0;		/* False */

/* This is declared in symtab.c; set to 1 in hp-symtab-read.c */
extern int hp_som_som_object_present;

/* In breakpoint.c */
extern int exception_catchpoints_are_fragile;

/* Should call_function allocate stack space for a struct return?  */

int
hppa64_use_struct_convention (int gcc_p, struct type *type)
{
  /* RM: struct upto 128 bits are returned in registers */
  return TYPE_LENGTH (type) > 16;
}

/* Handle 32/64-bit struct return conventions.  */

static enum return_value_convention
hppa32_return_value (struct gdbarch *gdbarch,
		     struct type *type, struct regcache *regcache,
		     void *readbuf, const void *writebuf)
{
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      if (readbuf != NULL)
	regcache_cooked_read_part (regcache, FP4_REGNUM, 0,
				   TYPE_LENGTH (type), readbuf);
      if (writebuf != NULL)
	regcache_cooked_write_part (regcache, FP4_REGNUM, 0,
				    TYPE_LENGTH (type), writebuf);
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  if (TYPE_LENGTH (type) <= 2 * 4)
    {
      /* The value always lives in the right hand end of the register
	 (or register pair)?  */
      int b;
      int reg = 28;
      int part = TYPE_LENGTH (type) % 4;
      /* The left hand register contains only part of the value,
	 transfer that first so that the rest can be xfered as entire
	 4-byte registers.  */
      if (part > 0)
	{
	  if (readbuf != NULL)
	    regcache_cooked_read_part (regcache, reg, 4 - part,
				       part, readbuf);
	  if (writebuf != NULL)
	    regcache_cooked_write_part (regcache, reg, 4 - part,
					part, writebuf);
	  reg++;
	}
      /* Now transfer the remaining register values.  */
      for (b = part; b < TYPE_LENGTH (type); b += 4)
	{
	  if (readbuf != NULL)
	    regcache_cooked_read (regcache, reg, (char *) readbuf + b);
	  if (writebuf != NULL)
	    regcache_cooked_write (regcache, reg, (const char *) writebuf + b);
	  reg++;
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  else
    return RETURN_VALUE_STRUCT_CONVENTION;
}

static enum return_value_convention
hppa64_return_value (struct gdbarch *gdbarch,
		     struct type *type, struct regcache *regcache,
		     void *readbuf, const void *writebuf)
{
  /* RM: Floats are returned in FR4R, doubles in FR4.  Integral values
     are in r28, padded on the left.  Aggregates less that 65 bits are
     in r28, right padded.  Aggregates upto 128 bits are in r28 and
     r29, right padded.  */ 
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      /* Floats are right aligned?  */
      int offset = register_size (gdbarch, FP4_REGNUM) - TYPE_LENGTH (type);
      if (readbuf != NULL)
	regcache_cooked_read_part (regcache, FP4_REGNUM, offset,
				   TYPE_LENGTH (type), readbuf);
      if (writebuf != NULL)
	regcache_cooked_write_part (regcache, FP4_REGNUM, offset,
				    TYPE_LENGTH (type), writebuf);
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  else if (TYPE_LENGTH (type) <= 8 && is_integral_type (type))
    {
      /* Integrals are right aligned.  */
      int offset = register_size (gdbarch, FP4_REGNUM) - TYPE_LENGTH (type);
      if (readbuf != NULL)
	regcache_cooked_read_part (regcache, 28, offset,
				   TYPE_LENGTH (type), readbuf);
      if (writebuf != NULL)
	regcache_cooked_write_part (regcache, 28, offset,
				    TYPE_LENGTH (type), writebuf);
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  else if (TYPE_LENGTH (type) <= 2 * 8)
    {
      /* Composite values are left aligned.  */
      int b;
      for (b = 0; b < TYPE_LENGTH (type); b += 8)
	{
	  int part = (TYPE_LENGTH (type) - b - 1) % 8 + 1;
	  if (readbuf != NULL)
	    regcache_cooked_read_part (regcache, 28, 0, part,
				       (char *) readbuf + b);
	  if (writebuf != NULL)
	    regcache_cooked_write_part (regcache, 28, 0, part,
					(const char *) writebuf + b);
	}
  return RETURN_VALUE_REGISTER_CONVENTION;
    }
  else
    return RETURN_VALUE_STRUCT_CONVENTION;
}

/* Routines to extract various sized constants out of hppa 
   instructions. */

/* This assumes that no garbage lies outside of the lower bits of 
   value. */

static int
sign_extend (unsigned val, unsigned bits)
{
  return (int) (val >> (bits - 1) ? (-1 << bits) | val : val);
}

/* For many immediate values the sign bit is the low bit! */

static int
low_sign_extend (unsigned val, unsigned bits)
{
  return (int) ((val & 0x1 ? (-1 << (bits - 1)) : 0) | val >> 1);
}

/* Extract the bits at positions between FROM and TO, using HP's numbering
   (MSB = 0). */

static int
get_field (unsigned word, int from, int to)
{
  return ((word) >> (31 - (to)) & ((1 << ((to) - (from) + 1)) - 1));
}

/* extract the immediate field from a ld{bhw}s instruction */

static int
extract_5_load (unsigned word)
{
  return low_sign_extend (word >> 16 & MASK_5, 5);
}

/* extract the immediate field from a break instruction */

static unsigned
extract_5r_store (unsigned word)
{
  return (word & MASK_5);
}

/* extract the immediate field from a {sr}sm instruction */

static unsigned
extract_5R_store (unsigned word)
{
  return (word >> 16 & MASK_5);
}

/* extract a 14 bit immediate field */

static int
extract_14 (unsigned word)
{
  return low_sign_extend (word & MASK_14, 14);
}

/* deposit a 14 bit constant in a word */

static unsigned
deposit_14 (int opnd, unsigned word)
{
  unsigned sign = (opnd < 0 ? 1 : 0);

  return word | ((unsigned) opnd << 1 & MASK_14) | sign;
}

/* extract a 21 bit constant */

static int
extract_21 (unsigned word)
{
  int val;

  word &= MASK_21;
  word <<= 11;
  val = get_field (word, 20, 20);
  val <<= 11;
  val |= get_field (word, 9, 19);
  val <<= 2;
  val |= get_field (word, 5, 6);
  val <<= 5;
  val |= get_field (word, 0, 4);
  val <<= 2;
  val |= get_field (word, 7, 8);
  return sign_extend (val, 21) << 11;
}

/* deposit a 21 bit constant in a word. Although 21 bit constants are
   usually the top 21 bits of a 32 bit constant, we assume that only
   the low 21 bits of opnd are relevant */

static unsigned
deposit_21 (unsigned opnd, unsigned word)
{
  unsigned val = 0;

  val |= get_field (opnd, 11 + 14, 11 + 18);
  val <<= 2;
  val |= get_field (opnd, 11 + 12, 11 + 13);
  val <<= 2;
  val |= get_field (opnd, 11 + 19, 11 + 20);
  val <<= 11;
  val |= get_field (opnd, 11 + 1, 11 + 11);
  val <<= 1;
  val |= get_field (opnd, 11 + 0, 11 + 0);
  return word | val;
}

/* extract a 17 bit constant from branch instructions, returning the
   19 bit signed value. */

static int
extract_17 (unsigned word)
{
  return sign_extend (get_field (word, 19, 28) |
		      get_field (word, 29, 29) << 10 |
		      get_field (word, 11, 15) << 11 |
		      (word & 0x1) << 16, 17) << 2;
}


/* Compare the start address for two unwind entries returning 1 if 
   the first address is larger than the second, -1 if the second is
   larger than the first, and zero if they are equal.  */

static int
compare_unwind_entries (const void *arg1, const void *arg2)
{
  const struct unwind_table_entry *a = arg1;
  const struct unwind_table_entry *b = arg2;

  if (a->region_start > b->region_start)
    return 1;
  else if (a->region_start < b->region_start)
    return -1;
  else
    return 0;
}

static CORE_ADDR low_text_segment_address;

static void
record_text_segment_lowaddr (bfd *abfd, asection *section, void *ignored)
{
  if (((section->flags & (SEC_ALLOC | SEC_LOAD | SEC_READONLY))
       == (SEC_ALLOC | SEC_LOAD | SEC_READONLY))
      && section->vma < low_text_segment_address)
    low_text_segment_address = section->vma;
}

static void
internalize_unwinds (struct objfile *objfile, struct unwind_table_entry *table,
		     asection *section, unsigned int entries, unsigned int size,
		     CORE_ADDR text_offset)
{
  /* We will read the unwind entries into temporary memory, then
     fill in the actual unwind table.  */
  if (size > 0)
    {
      unsigned long tmp;
      unsigned i;
      char *buf = alloca (size);

      low_text_segment_address = -1;

      /* If addresses are 64 bits wide, then unwinds are supposed to
	 be segment relative offsets instead of absolute addresses. 

	 Note that when loading a shared library (text_offset != 0) the
	 unwinds are already relative to the text_offset that will be
	 passed in.  */
      if (TARGET_PTR_BIT == 64 && text_offset == 0)
	{
	  bfd_map_over_sections (objfile->obfd,
				 record_text_segment_lowaddr, NULL);

	  /* ?!? Mask off some low bits.  Should this instead subtract
	     out the lowest section's filepos or something like that?
	     This looks very hokey to me.  */
	  low_text_segment_address &= ~0xfff;
	  text_offset += low_text_segment_address;
	}

      bfd_get_section_contents (objfile->obfd, section, buf, 0, size);

      /* Now internalize the information being careful to handle host/target
         endian issues.  */
      for (i = 0; i < entries; i++)
	{
	  table[i].region_start = bfd_get_32 (objfile->obfd,
					      (bfd_byte *) buf);
	  table[i].region_start += text_offset;
	  buf += 4;
	  table[i].region_end = bfd_get_32 (objfile->obfd, (bfd_byte *) buf);
	  table[i].region_end += text_offset;
	  buf += 4;
	  tmp = bfd_get_32 (objfile->obfd, (bfd_byte *) buf);
	  buf += 4;
	  table[i].Cannot_unwind = (tmp >> 31) & 0x1;
	  table[i].Millicode = (tmp >> 30) & 0x1;
	  table[i].Millicode_save_sr0 = (tmp >> 29) & 0x1;
	  table[i].Region_description = (tmp >> 27) & 0x3;
	  table[i].reserved1 = (tmp >> 26) & 0x1;
	  table[i].Entry_SR = (tmp >> 25) & 0x1;
	  table[i].Entry_FR = (tmp >> 21) & 0xf;
	  table[i].Entry_GR = (tmp >> 16) & 0x1f;
	  table[i].Args_stored = (tmp >> 15) & 0x1;
	  table[i].Variable_Frame = (tmp >> 14) & 0x1;
	  table[i].Separate_Package_Body = (tmp >> 13) & 0x1;
	  table[i].Frame_Extension_Millicode = (tmp >> 12) & 0x1;
	  table[i].Stack_Overflow_Check = (tmp >> 11) & 0x1;
	  table[i].Two_Instruction_SP_Increment = (tmp >> 10) & 0x1;
	  table[i].Ada_Region = (tmp >> 9) & 0x1;
	  table[i].cxx_info = (tmp >> 8) & 0x1;
	  table[i].cxx_try_catch = (tmp >> 7) & 0x1;
	  table[i].sched_entry_seq = (tmp >> 6) & 0x1;
	  table[i].reserved2 = (tmp >> 5) & 0x1;
	  table[i].Save_SP = (tmp >> 4) & 0x1;
	  table[i].Save_RP = (tmp >> 3) & 0x1;
	  table[i].Save_MRP_in_frame = (tmp >> 2) & 0x1;
	  table[i].extn_ptr_defined = (tmp >> 1) & 0x1;
	  table[i].Cleanup_defined = tmp & 0x1;
	  tmp = bfd_get_32 (objfile->obfd, (bfd_byte *) buf);
	  buf += 4;
	  table[i].MPE_XL_interrupt_marker = (tmp >> 31) & 0x1;
	  table[i].HP_UX_interrupt_marker = (tmp >> 30) & 0x1;
	  table[i].Large_frame = (tmp >> 29) & 0x1;
	  table[i].Pseudo_SP_Set = (tmp >> 28) & 0x1;
	  table[i].reserved4 = (tmp >> 27) & 0x1;
	  table[i].Total_frame_size = tmp & 0x7ffffff;

	  /* Stub unwinds are handled elsewhere. */
	  table[i].stub_unwind.stub_type = 0;
	  table[i].stub_unwind.padding = 0;
	}
    }
}

/* Read in the backtrace information stored in the `$UNWIND_START$' section of
   the object file.  This info is used mainly by find_unwind_entry() to find
   out the stack frame size and frame pointer used by procedures.  We put
   everything on the psymbol obstack in the objfile so that it automatically
   gets freed when the objfile is destroyed.  */

static void
read_unwind_info (struct objfile *objfile)
{
  asection *unwind_sec, *stub_unwind_sec;
  unsigned unwind_size, stub_unwind_size, total_size;
  unsigned index, unwind_entries;
  unsigned stub_entries, total_entries;
  CORE_ADDR text_offset;
  struct obj_unwind_info *ui;
  obj_private_data_t *obj_private;

  text_offset = ANOFFSET (objfile->section_offsets, 0);
  ui = (struct obj_unwind_info *) obstack_alloc (&objfile->objfile_obstack,
					   sizeof (struct obj_unwind_info));

  ui->table = NULL;
  ui->cache = NULL;
  ui->last = -1;

  /* For reasons unknown the HP PA64 tools generate multiple unwinder
     sections in a single executable.  So we just iterate over every
     section in the BFD looking for unwinder sections intead of trying
     to do a lookup with bfd_get_section_by_name. 

     First determine the total size of the unwind tables so that we
     can allocate memory in a nice big hunk.  */
  total_entries = 0;
  for (unwind_sec = objfile->obfd->sections;
       unwind_sec;
       unwind_sec = unwind_sec->next)
    {
      if (strcmp (unwind_sec->name, "$UNWIND_START$") == 0
	  || strcmp (unwind_sec->name, ".PARISC.unwind") == 0)
	{
	  unwind_size = bfd_section_size (objfile->obfd, unwind_sec);
	  unwind_entries = unwind_size / UNWIND_ENTRY_SIZE;

	  total_entries += unwind_entries;
	}
    }

  /* Now compute the size of the stub unwinds.  Note the ELF tools do not
     use stub unwinds at the curren time.  */
  stub_unwind_sec = bfd_get_section_by_name (objfile->obfd, "$UNWIND_END$");

  if (stub_unwind_sec)
    {
      stub_unwind_size = bfd_section_size (objfile->obfd, stub_unwind_sec);
      stub_entries = stub_unwind_size / STUB_UNWIND_ENTRY_SIZE;
    }
  else
    {
      stub_unwind_size = 0;
      stub_entries = 0;
    }

  /* Compute total number of unwind entries and their total size.  */
  total_entries += stub_entries;
  total_size = total_entries * sizeof (struct unwind_table_entry);

  /* Allocate memory for the unwind table.  */
  ui->table = (struct unwind_table_entry *)
    obstack_alloc (&objfile->objfile_obstack, total_size);
  ui->last = total_entries - 1;

  /* Now read in each unwind section and internalize the standard unwind
     entries.  */
  index = 0;
  for (unwind_sec = objfile->obfd->sections;
       unwind_sec;
       unwind_sec = unwind_sec->next)
    {
      if (strcmp (unwind_sec->name, "$UNWIND_START$") == 0
	  || strcmp (unwind_sec->name, ".PARISC.unwind") == 0)
	{
	  unwind_size = bfd_section_size (objfile->obfd, unwind_sec);
	  unwind_entries = unwind_size / UNWIND_ENTRY_SIZE;

	  internalize_unwinds (objfile, &ui->table[index], unwind_sec,
			       unwind_entries, unwind_size, text_offset);
	  index += unwind_entries;
	}
    }

  /* Now read in and internalize the stub unwind entries.  */
  if (stub_unwind_size > 0)
    {
      unsigned int i;
      char *buf = alloca (stub_unwind_size);

      /* Read in the stub unwind entries.  */
      bfd_get_section_contents (objfile->obfd, stub_unwind_sec, buf,
				0, stub_unwind_size);

      /* Now convert them into regular unwind entries.  */
      for (i = 0; i < stub_entries; i++, index++)
	{
	  /* Clear out the next unwind entry.  */
	  memset (&ui->table[index], 0, sizeof (struct unwind_table_entry));

	  /* Convert offset & size into region_start and region_end.  
	     Stuff away the stub type into "reserved" fields.  */
	  ui->table[index].region_start = bfd_get_32 (objfile->obfd,
						      (bfd_byte *) buf);
	  ui->table[index].region_start += text_offset;
	  buf += 4;
	  ui->table[index].stub_unwind.stub_type = bfd_get_8 (objfile->obfd,
							  (bfd_byte *) buf);
	  buf += 2;
	  ui->table[index].region_end
	    = ui->table[index].region_start + 4 *
	    (bfd_get_16 (objfile->obfd, (bfd_byte *) buf) - 1);
	  buf += 2;
	}

    }

  /* Unwind table needs to be kept sorted.  */
  qsort (ui->table, total_entries, sizeof (struct unwind_table_entry),
	 compare_unwind_entries);

  /* Keep a pointer to the unwind information.  */
  if (objfile->obj_private == NULL)
    {
      obj_private = (obj_private_data_t *)
	obstack_alloc (&objfile->objfile_obstack,
		       sizeof (obj_private_data_t));
      obj_private->unwind_info = NULL;
      obj_private->so_info = NULL;
      obj_private->dp = 0;

      objfile->obj_private = obj_private;
    }
  obj_private = (obj_private_data_t *) objfile->obj_private;
  obj_private->unwind_info = ui;
}

/* Lookup the unwind (stack backtrace) info for the given PC.  We search all
   of the objfiles seeking the unwind table entry for this PC.  Each objfile
   contains a sorted list of struct unwind_table_entry.  Since we do a binary
   search of the unwind tables, we depend upon them to be sorted.  */

struct unwind_table_entry *
find_unwind_entry (CORE_ADDR pc)
{
  int first, middle, last;
  struct objfile *objfile;

  /* A function at address 0?  Not in HP-UX! */
  if (pc == (CORE_ADDR) 0)
    return NULL;

  ALL_OBJFILES (objfile)
  {
    struct obj_unwind_info *ui;
    ui = NULL;
    if (objfile->obj_private)
      ui = ((obj_private_data_t *) (objfile->obj_private))->unwind_info;

    if (!ui)
      {
	read_unwind_info (objfile);
	if (objfile->obj_private == NULL)
	  error ("Internal error reading unwind information.");
	ui = ((obj_private_data_t *) (objfile->obj_private))->unwind_info;
      }

    /* First, check the cache */

    if (ui->cache
	&& pc >= ui->cache->region_start
	&& pc <= ui->cache->region_end)
      return ui->cache;

    /* Not in the cache, do a binary search */

    first = 0;
    last = ui->last;

    while (first <= last)
      {
	middle = (first + last) / 2;
	if (pc >= ui->table[middle].region_start
	    && pc <= ui->table[middle].region_end)
	  {
	    ui->cache = &ui->table[middle];
	    return &ui->table[middle];
	  }

	if (pc < ui->table[middle].region_start)
	  last = middle - 1;
	else
	  first = middle + 1;
      }
  }				/* ALL_OBJFILES() */
  return NULL;
}

const unsigned char *
hppa_breakpoint_from_pc (CORE_ADDR *pc, int *len)
{
  static const unsigned char breakpoint[] = {0x00, 0x01, 0x00, 0x04};
  (*len) = sizeof (breakpoint);
  return breakpoint;
}

/* Return the name of a register.  */

const char *
hppa32_register_name (int i)
{
  static char *names[] = {
    "flags",  "r1",      "rp",     "r3",
    "r4",     "r5",      "r6",     "r7",
    "r8",     "r9",      "r10",    "r11",
    "r12",    "r13",     "r14",    "r15",
    "r16",    "r17",     "r18",    "r19",
    "r20",    "r21",     "r22",    "r23",
    "r24",    "r25",     "r26",    "dp",
    "ret0",   "ret1",    "sp",     "r31",
    "sar",    "pcoqh",   "pcsqh",  "pcoqt",
    "pcsqt",  "eiem",    "iir",    "isr",
    "ior",    "ipsw",    "goto",   "sr4",
    "sr0",    "sr1",     "sr2",    "sr3",
    "sr5",    "sr6",     "sr7",    "cr0",
    "cr8",    "cr9",     "ccr",    "cr12",
    "cr13",   "cr24",    "cr25",   "cr26",
    "mpsfu_high","mpsfu_low","mpsfu_ovflo","pad",
    "fpsr",    "fpe1",   "fpe2",   "fpe3",
    "fpe4",   "fpe5",    "fpe6",   "fpe7",
    "fr4",     "fr4R",   "fr5",    "fr5R",
    "fr6",    "fr6R",    "fr7",    "fr7R",
    "fr8",     "fr8R",   "fr9",    "fr9R",
    "fr10",   "fr10R",   "fr11",   "fr11R",
    "fr12",    "fr12R",  "fr13",   "fr13R",
    "fr14",   "fr14R",   "fr15",   "fr15R",
    "fr16",    "fr16R",  "fr17",   "fr17R",
    "fr18",   "fr18R",   "fr19",   "fr19R",
    "fr20",    "fr20R",  "fr21",   "fr21R",
    "fr22",   "fr22R",   "fr23",   "fr23R",
    "fr24",    "fr24R",  "fr25",   "fr25R",
    "fr26",   "fr26R",   "fr27",   "fr27R",
    "fr28",    "fr28R",  "fr29",   "fr29R",
    "fr30",   "fr30R",   "fr31",   "fr31R"
  };
  if (i < 0 || i >= (sizeof (names) / sizeof (*names)))
    return NULL;
  else
    return names[i];
}

const char *
hppa64_register_name (int i)
{
  static char *names[] = {
    "flags",  "r1",      "rp",     "r3",
    "r4",     "r5",      "r6",     "r7",
    "r8",     "r9",      "r10",    "r11",
    "r12",    "r13",     "r14",    "r15",
    "r16",    "r17",     "r18",    "r19",
    "r20",    "r21",     "r22",    "r23",
    "r24",    "r25",     "r26",    "dp",
    "ret0",   "ret1",    "sp",     "r31",
    "sar",    "pcoqh",   "pcsqh",  "pcoqt",
    "pcsqt",  "eiem",    "iir",    "isr",
    "ior",    "ipsw",    "goto",   "sr4",
    "sr0",    "sr1",     "sr2",    "sr3",
    "sr5",    "sr6",     "sr7",    "cr0",
    "cr8",    "cr9",     "ccr",    "cr12",
    "cr13",   "cr24",    "cr25",   "cr26",
    "mpsfu_high","mpsfu_low","mpsfu_ovflo","pad",
    "fpsr",    "fpe1",   "fpe2",   "fpe3",
    "fr4",    "fr5",     "fr6",    "fr7",
    "fr8",     "fr9",    "fr10",   "fr11",
    "fr12",   "fr13",    "fr14",   "fr15",
    "fr16",    "fr17",   "fr18",   "fr19",
    "fr20",   "fr21",    "fr22",   "fr23",
    "fr24",    "fr25",   "fr26",   "fr27",
    "fr28",  "fr29",    "fr30",   "fr31"
  };
  if (i < 0 || i >= (sizeof (names) / sizeof (*names)))
    return NULL;
  else
    return names[i];
}



/* Return the adjustment necessary to make for addresses on the stack
   as presented by hpread.c.

   This is necessary because of the stack direction on the PA and the
   bizarre way in which someone (?) decided they wanted to handle
   frame pointerless code in GDB.  */
int
hpread_adjust_stack_address (CORE_ADDR func_addr)
{
  struct unwind_table_entry *u;

  u = find_unwind_entry (func_addr);
  if (!u)
    return 0;
  else
    return u->Total_frame_size << 3;
}

/* Called to determine if PC is in an interrupt handler of some
   kind.  */

static int
pc_in_interrupt_handler (CORE_ADDR pc)
{
  struct unwind_table_entry *u;
  struct minimal_symbol *msym_us;

  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* Oh joys.  HPUX sets the interrupt bit for _sigreturn even though
     its frame isn't a pure interrupt frame.  Deal with this.  */
  msym_us = lookup_minimal_symbol_by_pc (pc);

  return (u->HP_UX_interrupt_marker
	  && !PC_IN_SIGTRAMP (pc, DEPRECATED_SYMBOL_NAME (msym_us)));
}

/* Called when no unwind descriptor was found for PC.  Returns 1 if it
   appears that PC is in a linker stub.

   ?!? Need to handle stubs which appear in PA64 code.  */

static int
pc_in_linker_stub (CORE_ADDR pc)
{
  int found_magic_instruction = 0;
  int i;
  char buf[4];

  /* If unable to read memory, assume pc is not in a linker stub.  */
  if (target_read_memory (pc, buf, 4) != 0)
    return 0;

  /* We are looking for something like

     ; $$dyncall jams RP into this special spot in the frame (RP')
     ; before calling the "call stub"
     ldw     -18(sp),rp

     ldsid   (rp),r1         ; Get space associated with RP into r1
     mtsp    r1,sp           ; Move it into space register 0
     be,n    0(sr0),rp)      ; back to your regularly scheduled program */

  /* Maximum known linker stub size is 4 instructions.  Search forward
     from the given PC, then backward.  */
  for (i = 0; i < 4; i++)
    {
      /* If we hit something with an unwind, stop searching this direction.  */

      if (find_unwind_entry (pc + i * 4) != 0)
	break;

      /* Check for ldsid (rp),r1 which is the magic instruction for a 
         return from a cross-space function call.  */
      if (read_memory_integer (pc + i * 4, 4) == 0x004010a1)
	{
	  found_magic_instruction = 1;
	  break;
	}
      /* Add code to handle long call/branch and argument relocation stubs
         here.  */
    }

  if (found_magic_instruction != 0)
    return 1;

  /* Now look backward.  */
  for (i = 0; i < 4; i++)
    {
      /* If we hit something with an unwind, stop searching this direction.  */

      if (find_unwind_entry (pc - i * 4) != 0)
	break;

      /* Check for ldsid (rp),r1 which is the magic instruction for a 
         return from a cross-space function call.  */
      if (read_memory_integer (pc - i * 4, 4) == 0x004010a1)
	{
	  found_magic_instruction = 1;
	  break;
	}
      /* Add code to handle long call/branch and argument relocation stubs
         here.  */
    }
  return found_magic_instruction;
}

static int
find_return_regnum (CORE_ADDR pc)
{
  struct unwind_table_entry *u;

  u = find_unwind_entry (pc);

  if (!u)
    return RP_REGNUM;

  if (u->Millicode)
    return 31;

  return RP_REGNUM;
}

/* Return size of frame, or -1 if we should use a frame pointer.  */
static int
find_proc_framesize (CORE_ADDR pc)
{
  struct unwind_table_entry *u;
  struct minimal_symbol *msym_us;

  /* This may indicate a bug in our callers... */
  if (pc == (CORE_ADDR) 0)
    return -1;

  u = find_unwind_entry (pc);

  if (!u)
    {
      if (pc_in_linker_stub (pc))
	/* Linker stubs have a zero size frame.  */
	return 0;
      else
	return -1;
    }

  msym_us = lookup_minimal_symbol_by_pc (pc);

  /* If Save_SP is set, and we're not in an interrupt or signal caller,
     then we have a frame pointer.  Use it.  */
  if (u->Save_SP
      && !pc_in_interrupt_handler (pc)
      && msym_us
      && !PC_IN_SIGTRAMP (pc, DEPRECATED_SYMBOL_NAME (msym_us)))
    return -1;

  return u->Total_frame_size << 3;
}

/* Return offset from sp at which rp is saved, or 0 if not saved.  */
static int rp_saved (CORE_ADDR);

static int
rp_saved (CORE_ADDR pc)
{
  struct unwind_table_entry *u;

  /* A function at, and thus a return PC from, address 0?  Not in HP-UX! */
  if (pc == (CORE_ADDR) 0)
    return 0;

  u = find_unwind_entry (pc);

  if (!u)
    {
      if (pc_in_linker_stub (pc))
	/* This is the so-called RP'.  */
	return -24;
      else
	return 0;
    }

  if (u->Save_RP)
    return (TARGET_PTR_BIT == 64 ? -16 : -20);
  else if (u->stub_unwind.stub_type != 0)
    {
      switch (u->stub_unwind.stub_type)
	{
	case EXPORT:
	case IMPORT:
	  return -24;
	case PARAMETER_RELOCATION:
	  return -8;
	default:
	  return 0;
	}
    }
  else
    return 0;
}

int
hppa_frameless_function_invocation (struct frame_info *frame)
{
  struct unwind_table_entry *u;

  u = find_unwind_entry (get_frame_pc (frame));

  if (u == 0)
    return 0;

  return (u->Total_frame_size == 0 && u->stub_unwind.stub_type == 0);
}

/* Immediately after a function call, return the saved pc.
   Can't go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

CORE_ADDR
hppa_saved_pc_after_call (struct frame_info *frame)
{
  int ret_regnum;
  CORE_ADDR pc;
  struct unwind_table_entry *u;

  ret_regnum = find_return_regnum (get_frame_pc (frame));
  pc = read_register (ret_regnum) & ~0x3;

  /* If PC is in a linker stub, then we need to dig the address
     the stub will return to out of the stack.  */
  u = find_unwind_entry (pc);
  if (u && u->stub_unwind.stub_type != 0)
    return DEPRECATED_FRAME_SAVED_PC (frame);
  else
    return pc;
}

CORE_ADDR
hppa_frame_saved_pc (struct frame_info *frame)
{
  CORE_ADDR pc = get_frame_pc (frame);
  struct unwind_table_entry *u;
  CORE_ADDR old_pc = 0;
  int spun_around_loop = 0;
  int rp_offset = 0;

  /* BSD, HPUX & OSF1 all lay out the hardware state in the same manner
     at the base of the frame in an interrupt handler.  Registers within
     are saved in the exact same order as GDB numbers registers.  How
     convienent.  */
  if (pc_in_interrupt_handler (pc))
    return read_memory_integer (get_frame_base (frame) + PC_REGNUM * 4,
				TARGET_PTR_BIT / 8) & ~0x3;

  if ((get_frame_pc (frame) >= get_frame_base (frame)
       && (get_frame_pc (frame)
	   <= (get_frame_base (frame)
	       /* A call dummy is sized in words, but it is actually a
		  series of instructions.  Account for that scaling
		  factor.  */
	       + ((DEPRECATED_REGISTER_SIZE / INSTRUCTION_SIZE)
		  * DEPRECATED_CALL_DUMMY_LENGTH)
	       /* Similarly we have to account for 64bit wide register
		  saves.  */
	       + (32 * DEPRECATED_REGISTER_SIZE)
	       /* We always consider FP regs 8 bytes long.  */
	       + (NUM_REGS - FP0_REGNUM) * 8
	       /* Similarly we have to account for 64bit wide register
		  saves.  */
	       + (6 * DEPRECATED_REGISTER_SIZE)))))
    {
      return read_memory_integer ((get_frame_base (frame)
				   + (TARGET_PTR_BIT == 64 ? -16 : -20)),
				  TARGET_PTR_BIT / 8) & ~0x3;
    }

#ifdef FRAME_SAVED_PC_IN_SIGTRAMP
  /* Deal with signal handler caller frames too.  */
  if ((get_frame_type (frame) == SIGTRAMP_FRAME))
    {
      CORE_ADDR rp;
      FRAME_SAVED_PC_IN_SIGTRAMP (frame, &rp);
      return rp & ~0x3;
    }
#endif

  if (hppa_frameless_function_invocation (frame))
    {
      int ret_regnum;

      ret_regnum = find_return_regnum (pc);

      /* If the next frame is an interrupt frame or a signal
         handler caller, then we need to look in the saved
         register area to get the return pointer (the values
         in the registers may not correspond to anything useful).  */
      if (get_next_frame (frame)
	  && ((get_frame_type (get_next_frame (frame)) == SIGTRAMP_FRAME)
	      || pc_in_interrupt_handler (get_frame_pc (get_next_frame (frame)))))
	{
	  CORE_ADDR *saved_regs;
	  hppa_frame_init_saved_regs (get_next_frame (frame));
	  saved_regs = deprecated_get_frame_saved_regs (get_next_frame (frame));
	  if (read_memory_integer (saved_regs[FLAGS_REGNUM],
				   TARGET_PTR_BIT / 8) & 0x2)
	    {
	      pc = read_memory_integer (saved_regs[31],
					TARGET_PTR_BIT / 8) & ~0x3;

	      /* Syscalls are really two frames.  The syscall stub itself
	         with a return pointer in %rp and the kernel call with
	         a return pointer in %r31.  We return the %rp variant
	         if %r31 is the same as frame->pc.  */
	      if (pc == get_frame_pc (frame))
		pc = read_memory_integer (saved_regs[RP_REGNUM],
					  TARGET_PTR_BIT / 8) & ~0x3;
	    }
	  else
	    pc = read_memory_integer (saved_regs[RP_REGNUM],
				      TARGET_PTR_BIT / 8) & ~0x3;
	}
      else
	pc = read_register (ret_regnum) & ~0x3;
    }
  else
    {
      spun_around_loop = 0;
      old_pc = pc;

    restart:
      rp_offset = rp_saved (pc);

      /* Similar to code in frameless function case.  If the next
         frame is a signal or interrupt handler, then dig the right
         information out of the saved register info.  */
      if (rp_offset == 0
	  && get_next_frame (frame)
	  && ((get_frame_type (get_next_frame (frame)) == SIGTRAMP_FRAME)
	      || pc_in_interrupt_handler (get_frame_pc (get_next_frame (frame)))))
	{
	  CORE_ADDR *saved_regs;
	  hppa_frame_init_saved_regs (get_next_frame (frame));
	  saved_regs = deprecated_get_frame_saved_regs (get_next_frame (frame));
	  if (read_memory_integer (saved_regs[FLAGS_REGNUM],
				   TARGET_PTR_BIT / 8) & 0x2)
	    {
	      pc = read_memory_integer (saved_regs[31],
					TARGET_PTR_BIT / 8) & ~0x3;

	      /* Syscalls are really two frames.  The syscall stub itself
	         with a return pointer in %rp and the kernel call with
	         a return pointer in %r31.  We return the %rp variant
	         if %r31 is the same as frame->pc.  */
	      if (pc == get_frame_pc (frame))
		pc = read_memory_integer (saved_regs[RP_REGNUM],
					  TARGET_PTR_BIT / 8) & ~0x3;
	    }
	  else
	    pc = read_memory_integer (saved_regs[RP_REGNUM],
				      TARGET_PTR_BIT / 8) & ~0x3;
	}
      else if (rp_offset == 0)
	{
	  old_pc = pc;
	  pc = read_register (RP_REGNUM) & ~0x3;
	}
      else
	{
	  old_pc = pc;
	  pc = read_memory_integer (get_frame_base (frame) + rp_offset,
				    TARGET_PTR_BIT / 8) & ~0x3;
	}
    }

  /* If PC is inside a linker stub, then dig out the address the stub
     will return to. 

     Don't do this for long branch stubs.  Why?  For some unknown reason
     _start is marked as a long branch stub in hpux10.  */
  u = find_unwind_entry (pc);
  if (u && u->stub_unwind.stub_type != 0
      && u->stub_unwind.stub_type != LONG_BRANCH)
    {
      unsigned int insn;

      /* If this is a dynamic executable, and we're in a signal handler,
         then the call chain will eventually point us into the stub for
         _sigreturn.  Unlike most cases, we'll be pointed to the branch
         to the real sigreturn rather than the code after the real branch!. 

         Else, try to dig the address the stub will return to in the normal
         fashion.  */
      insn = read_memory_integer (pc, 4);
      if ((insn & 0xfc00e000) == 0xe8000000)
	return (pc + extract_17 (insn) + 8) & ~0x3;
      else
	{
	  if (old_pc == pc)
	    spun_around_loop++;

	  if (spun_around_loop > 1)
	    {
	      /* We're just about to go around the loop again with
	         no more hope of success.  Die. */
	      error ("Unable to find return pc for this frame");
	    }
	  else
	    goto restart;
	}
    }

  return pc;
}

/* We need to correct the PC and the FP for the outermost frame when we are
   in a system call.  */

void
hppa_init_extra_frame_info (int fromleaf, struct frame_info *frame)
{
  int flags;
  int framesize;

  if (get_next_frame (frame) && !fromleaf)
    return;

  /* If the next frame represents a frameless function invocation then
     we have to do some adjustments that are normally done by
     DEPRECATED_FRAME_CHAIN.  (DEPRECATED_FRAME_CHAIN is not called in
     this case.)  */
  if (fromleaf)
    {
      /* Find the framesize of *this* frame without peeking at the PC
         in the current frame structure (it isn't set yet).  */
      framesize = find_proc_framesize (DEPRECATED_FRAME_SAVED_PC (get_next_frame (frame)));

      /* Now adjust our base frame accordingly.  If we have a frame pointer
         use it, else subtract the size of this frame from the current
         frame.  (we always want frame->frame to point at the lowest address
         in the frame).  */
      if (framesize == -1)
	deprecated_update_frame_base_hack (frame, deprecated_read_fp ());
      else
	deprecated_update_frame_base_hack (frame, get_frame_base (frame) - framesize);
      return;
    }

  flags = read_register (FLAGS_REGNUM);
  if (flags & 2)		/* In system call? */
    deprecated_update_frame_pc_hack (frame, read_register (31) & ~0x3);

  /* The outermost frame is always derived from PC-framesize

     One might think frameless innermost frames should have
     a frame->frame that is the same as the parent's frame->frame.
     That is wrong; frame->frame in that case should be the *high*
     address of the parent's frame.  It's complicated as hell to
     explain, but the parent *always* creates some stack space for
     the child.  So the child actually does have a frame of some
     sorts, and its base is the high address in its parent's frame.  */
  framesize = find_proc_framesize (get_frame_pc (frame));
  if (framesize == -1)
    deprecated_update_frame_base_hack (frame, deprecated_read_fp ());
  else
    deprecated_update_frame_base_hack (frame, read_register (SP_REGNUM) - framesize);
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct, and
   then DEPRECATED_INIT_EXTRA_FRAME_INFO and DEPRECATED_INIT_FRAME_PC
   will be called for the new frame.

   This may involve searching through prologues for several functions
   at boundaries where GCC calls HP C code, or where code which has
   a frame pointer calls code without a frame pointer.  */

CORE_ADDR
hppa_frame_chain (struct frame_info *frame)
{
  int my_framesize, caller_framesize;
  struct unwind_table_entry *u;
  CORE_ADDR frame_base;
  struct frame_info *tmp_frame;

  /* A frame in the current frame list, or zero.  */
  struct frame_info *saved_regs_frame = 0;
  /* Where the registers were saved in saved_regs_frame.  If
     saved_regs_frame is zero, this is garbage.  */
  CORE_ADDR *saved_regs = NULL;

  CORE_ADDR caller_pc;

  struct minimal_symbol *min_frame_symbol;
  struct symbol *frame_symbol;
  char *frame_symbol_name;

  /* If this is a threaded application, and we see the
     routine "__pthread_exit", treat it as the stack root
     for this thread. */
  min_frame_symbol = lookup_minimal_symbol_by_pc (get_frame_pc (frame));
  frame_symbol = find_pc_function (get_frame_pc (frame));

  if ((min_frame_symbol != 0) /* && (frame_symbol == 0) */ )
    {
      /* The test above for "no user function name" would defend
         against the slim likelihood that a user might define a
         routine named "__pthread_exit" and then try to debug it.

         If it weren't commented out, and you tried to debug the
         pthread library itself, you'd get errors.

         So for today, we don't make that check. */
      frame_symbol_name = DEPRECATED_SYMBOL_NAME (min_frame_symbol);
      if (frame_symbol_name != 0)
	{
	  if (0 == strncmp (frame_symbol_name,
			    THREAD_INITIAL_FRAME_SYMBOL,
			    THREAD_INITIAL_FRAME_SYM_LEN))
	    {
	      /* Pretend we've reached the bottom of the stack. */
	      return (CORE_ADDR) 0;
	    }
	}
    }				/* End of hacky code for threads. */

  /* Handle HPUX, BSD, and OSF1 style interrupt frames first.  These
     are easy; at *sp we have a full save state strucutre which we can
     pull the old stack pointer from.  Also see frame_saved_pc for
     code to dig a saved PC out of the save state structure.  */
  if (pc_in_interrupt_handler (get_frame_pc (frame)))
    frame_base = read_memory_integer (get_frame_base (frame) + SP_REGNUM * 4,
				      TARGET_PTR_BIT / 8);
#ifdef FRAME_BASE_BEFORE_SIGTRAMP
  else if ((get_frame_type (frame) == SIGTRAMP_FRAME))
    {
      FRAME_BASE_BEFORE_SIGTRAMP (frame, &frame_base);
    }
#endif
  else
    frame_base = get_frame_base (frame);

  /* Get frame sizes for the current frame and the frame of the 
     caller.  */
  my_framesize = find_proc_framesize (get_frame_pc (frame));
  caller_pc = DEPRECATED_FRAME_SAVED_PC (frame);

  /* If we can't determine the caller's PC, then it's not likely we can
     really determine anything meaningful about its frame.  We'll consider
     this to be stack bottom. */
  if (caller_pc == (CORE_ADDR) 0)
    return (CORE_ADDR) 0;

  caller_framesize = find_proc_framesize (DEPRECATED_FRAME_SAVED_PC (frame));

  /* If caller does not have a frame pointer, then its frame
     can be found at current_frame - caller_framesize.  */
  if (caller_framesize != -1)
    {
      return frame_base - caller_framesize;
    }
  /* Both caller and callee have frame pointers and are GCC compiled
     (SAVE_SP bit in unwind descriptor is on for both functions.
     The previous frame pointer is found at the top of the current frame.  */
  if (caller_framesize == -1 && my_framesize == -1)
    {
      return read_memory_integer (frame_base, TARGET_PTR_BIT / 8);
    }
  /* Caller has a frame pointer, but callee does not.  This is a little
     more difficult as GCC and HP C lay out locals and callee register save
     areas very differently.

     The previous frame pointer could be in a register, or in one of 
     several areas on the stack.

     Walk from the current frame to the innermost frame examining 
     unwind descriptors to determine if %r3 ever gets saved into the
     stack.  If so return whatever value got saved into the stack.
     If it was never saved in the stack, then the value in %r3 is still
     valid, so use it. 

     We use information from unwind descriptors to determine if %r3
     is saved into the stack (Entry_GR field has this information).  */

  for (tmp_frame = frame; tmp_frame; tmp_frame = get_next_frame (tmp_frame))
    {
      u = find_unwind_entry (get_frame_pc (tmp_frame));

      if (!u)
	{
	  /* We could find this information by examining prologues.  I don't
	     think anyone has actually written any tools (not even "strip")
	     which leave them out of an executable, so maybe this is a moot
	     point.  */
	  /* ??rehrauer: Actually, it's quite possible to stepi your way into
	     code that doesn't have unwind entries.  For example, stepping into
	     the dynamic linker will give you a PC that has none.  Thus, I've
	     disabled this warning. */
#if 0
	  warning ("Unable to find unwind for PC 0x%x -- Help!", get_frame_pc (tmp_frame));
#endif
	  return (CORE_ADDR) 0;
	}

      if (u->Save_SP
	  || (get_frame_type (tmp_frame) == SIGTRAMP_FRAME)
	  || pc_in_interrupt_handler (get_frame_pc (tmp_frame)))
	break;

      /* Entry_GR specifies the number of callee-saved general registers
         saved in the stack.  It starts at %r3, so %r3 would be 1.  */
      if (u->Entry_GR >= 1)
	{
	  /* The unwind entry claims that r3 is saved here.  However,
	     in optimized code, GCC often doesn't actually save r3.
	     We'll discover this if we look at the prologue.  */
	  hppa_frame_init_saved_regs (tmp_frame);
	  saved_regs = deprecated_get_frame_saved_regs (tmp_frame);
	  saved_regs_frame = tmp_frame;

	  /* If we have an address for r3, that's good.  */
	  if (saved_regs[DEPRECATED_FP_REGNUM])
	    break;
	}
    }

  if (tmp_frame)
    {
      /* We may have walked down the chain into a function with a frame
         pointer.  */
      if (u->Save_SP
	  && !(get_frame_type (tmp_frame) == SIGTRAMP_FRAME)
	  && !pc_in_interrupt_handler (get_frame_pc (tmp_frame)))
	{
	  return read_memory_integer (get_frame_base (tmp_frame), TARGET_PTR_BIT / 8);
	}
      /* %r3 was saved somewhere in the stack.  Dig it out.  */
      else
	{
	  /* Sick.

	     For optimization purposes many kernels don't have the
	     callee saved registers into the save_state structure upon
	     entry into the kernel for a syscall; the optimization
	     is usually turned off if the process is being traced so
	     that the debugger can get full register state for the
	     process.

	     This scheme works well except for two cases:

	     * Attaching to a process when the process is in the
	     kernel performing a system call (debugger can't get
	     full register state for the inferior process since
	     the process wasn't being traced when it entered the
	     system call).

	     * Register state is not complete if the system call
	     causes the process to core dump.


	     The following heinous code is an attempt to deal with
	     the lack of register state in a core dump.  It will
	     fail miserably if the function which performs the
	     system call has a variable sized stack frame.  */

	  if (tmp_frame != saved_regs_frame)
	    {
	      hppa_frame_init_saved_regs (tmp_frame);
	      saved_regs = deprecated_get_frame_saved_regs (tmp_frame);
	    }

	  /* Abominable hack.  */
	  if (current_target.to_has_execution == 0
	      && ((saved_regs[FLAGS_REGNUM]
		   && (read_memory_integer (saved_regs[FLAGS_REGNUM],
					    TARGET_PTR_BIT / 8)
		       & 0x2))
		  || (saved_regs[FLAGS_REGNUM] == 0
		      && read_register (FLAGS_REGNUM) & 0x2)))
	    {
	      u = find_unwind_entry (DEPRECATED_FRAME_SAVED_PC (frame));
	      if (!u)
		{
		  return read_memory_integer (saved_regs[DEPRECATED_FP_REGNUM],
					      TARGET_PTR_BIT / 8);
		}
	      else
		{
		  return frame_base - (u->Total_frame_size << 3);
		}
	    }

	  return read_memory_integer (saved_regs[DEPRECATED_FP_REGNUM],
				      TARGET_PTR_BIT / 8);
	}
    }
  else
    {
      /* Get the innermost frame.  */
      tmp_frame = frame;
      while (get_next_frame (tmp_frame) != NULL)
	tmp_frame = get_next_frame (tmp_frame);

      if (tmp_frame != saved_regs_frame)
	{
	  hppa_frame_init_saved_regs (tmp_frame);
	  saved_regs = deprecated_get_frame_saved_regs (tmp_frame);
	}

      /* Abominable hack.  See above.  */
      if (current_target.to_has_execution == 0
	  && ((saved_regs[FLAGS_REGNUM]
	       && (read_memory_integer (saved_regs[FLAGS_REGNUM],
					TARGET_PTR_BIT / 8)
		   & 0x2))
	      || (saved_regs[FLAGS_REGNUM] == 0
		  && read_register (FLAGS_REGNUM) & 0x2)))
	{
	  u = find_unwind_entry (DEPRECATED_FRAME_SAVED_PC (frame));
	  if (!u)
	    {
	      return read_memory_integer (saved_regs[DEPRECATED_FP_REGNUM],
					  TARGET_PTR_BIT / 8);
	    }
	  else
	    {
	      return frame_base - (u->Total_frame_size << 3);
	    }
	}

      /* The value in %r3 was never saved into the stack (thus %r3 still
         holds the value of the previous frame pointer).  */
      return deprecated_read_fp ();
    }
}


/* To see if a frame chain is valid, see if the caller looks like it
   was compiled with gcc. */

int
hppa_frame_chain_valid (CORE_ADDR chain, struct frame_info *thisframe)
{
  struct minimal_symbol *msym_us;
  struct minimal_symbol *msym_start;
  struct unwind_table_entry *u, *next_u = NULL;
  struct frame_info *next;

  u = find_unwind_entry (get_frame_pc (thisframe));

  if (u == NULL)
    return 1;

  /* We can't just check that the same of msym_us is "_start", because
     someone idiotically decided that they were going to make a Ltext_end
     symbol with the same address.  This Ltext_end symbol is totally
     indistinguishable (as nearly as I can tell) from the symbol for a function
     which is (legitimately, since it is in the user's namespace)
     named Ltext_end, so we can't just ignore it.  */
  msym_us = lookup_minimal_symbol_by_pc (DEPRECATED_FRAME_SAVED_PC (thisframe));
  msym_start = lookup_minimal_symbol ("_start", NULL, NULL);
  if (msym_us
      && msym_start
      && SYMBOL_VALUE_ADDRESS (msym_us) == SYMBOL_VALUE_ADDRESS (msym_start))
    return 0;

  /* Grrrr.  Some new idiot decided that they don't want _start for the
     PRO configurations; $START$ calls main directly....  Deal with it.  */
  msym_start = lookup_minimal_symbol ("$START$", NULL, NULL);
  if (msym_us
      && msym_start
      && SYMBOL_VALUE_ADDRESS (msym_us) == SYMBOL_VALUE_ADDRESS (msym_start))
    return 0;

  next = get_next_frame (thisframe);
  if (next)
    next_u = find_unwind_entry (get_frame_pc (next));

  /* If this frame does not save SP, has no stack, isn't a stub,
     and doesn't "call" an interrupt routine or signal handler caller,
     then its not valid.  */
  if (u->Save_SP || u->Total_frame_size || u->stub_unwind.stub_type != 0
      || (get_next_frame (thisframe) && (get_frame_type (get_next_frame (thisframe)) == SIGTRAMP_FRAME))
      || (next_u && next_u->HP_UX_interrupt_marker))
    return 1;

  if (pc_in_linker_stub (get_frame_pc (thisframe)))
    return 1;

  return 0;
}

/* These functions deal with saving and restoring register state
   around a function call in the inferior.  They keep the stack
   double-word aligned;  eventually, on an hp700, the stack will have
   to be aligned to a 64-byte boundary.  */

void
hppa_push_dummy_frame (void)
{
  CORE_ADDR sp, pc, pcspace;
  int regnum;
  CORE_ADDR int_buffer;
  double freg_buffer;

  pc = hppa_target_read_pc (inferior_ptid);
  int_buffer = read_register (FLAGS_REGNUM);
  if (int_buffer & 0x2)
    {
      const unsigned int sid = (pc >> 30) & 0x3;
      if (sid == 0)
	pcspace = read_register (SR4_REGNUM);
      else
	pcspace = read_register (SR4_REGNUM + 4 + sid);
    }
  else
    pcspace = read_register (PCSQ_HEAD_REGNUM);

  /* Space for "arguments"; the RP goes in here. */
  sp = read_register (SP_REGNUM) + 48;
  int_buffer = read_register (RP_REGNUM) | 0x3;

  /* The 32bit and 64bit ABIs save the return pointer into different
     stack slots.  */
  if (DEPRECATED_REGISTER_SIZE == 8)
    write_memory (sp - 16, (char *) &int_buffer, DEPRECATED_REGISTER_SIZE);
  else
    write_memory (sp - 20, (char *) &int_buffer, DEPRECATED_REGISTER_SIZE);

  int_buffer = deprecated_read_fp ();
  write_memory (sp, (char *) &int_buffer, DEPRECATED_REGISTER_SIZE);

  write_register (DEPRECATED_FP_REGNUM, sp);

  sp += 2 * DEPRECATED_REGISTER_SIZE;

  for (regnum = 1; regnum < 32; regnum++)
    if (regnum != RP_REGNUM && regnum != DEPRECATED_FP_REGNUM)
      sp = push_word (sp, read_register (regnum));

  /* This is not necessary for the 64bit ABI.  In fact it is dangerous.  */
  if (DEPRECATED_REGISTER_SIZE != 8)
    sp += 4;

  for (regnum = FP0_REGNUM; regnum < NUM_REGS; regnum++)
    {
      deprecated_read_register_bytes (DEPRECATED_REGISTER_BYTE (regnum),
				      (char *) &freg_buffer, 8);
      sp = push_bytes (sp, (char *) &freg_buffer, 8);
    }
  sp = push_word (sp, read_register (IPSW_REGNUM));
  sp = push_word (sp, read_register (SAR_REGNUM));
  sp = push_word (sp, pc);
  sp = push_word (sp, pcspace);
  sp = push_word (sp, pc + 4);
  sp = push_word (sp, pcspace);
  write_register (SP_REGNUM, sp);
}

static void
find_dummy_frame_regs (struct frame_info *frame,
		       CORE_ADDR frame_saved_regs[])
{
  CORE_ADDR fp = get_frame_base (frame);
  int i;

  /* The 32bit and 64bit ABIs save RP into different locations.  */
  if (DEPRECATED_REGISTER_SIZE == 8)
    frame_saved_regs[RP_REGNUM] = (fp - 16) & ~0x3;
  else
    frame_saved_regs[RP_REGNUM] = (fp - 20) & ~0x3;

  frame_saved_regs[DEPRECATED_FP_REGNUM] = fp;

  frame_saved_regs[1] = fp + (2 * DEPRECATED_REGISTER_SIZE);

  for (fp += 3 * DEPRECATED_REGISTER_SIZE, i = 3; i < 32; i++)
    {
      if (i != DEPRECATED_FP_REGNUM)
	{
	  frame_saved_regs[i] = fp;
	  fp += DEPRECATED_REGISTER_SIZE;
	}
    }

  /* This is not necessary or desirable for the 64bit ABI.  */
  if (DEPRECATED_REGISTER_SIZE != 8)
    fp += 4;

  for (i = FP0_REGNUM; i < NUM_REGS; i++, fp += 8)
    frame_saved_regs[i] = fp;

  frame_saved_regs[IPSW_REGNUM] = fp;
  frame_saved_regs[SAR_REGNUM] = fp + DEPRECATED_REGISTER_SIZE;
  frame_saved_regs[PCOQ_HEAD_REGNUM] = fp + 2 * DEPRECATED_REGISTER_SIZE;
  frame_saved_regs[PCSQ_HEAD_REGNUM] = fp + 3 * DEPRECATED_REGISTER_SIZE;
  frame_saved_regs[PCOQ_TAIL_REGNUM] = fp + 4 * DEPRECATED_REGISTER_SIZE;
  frame_saved_regs[PCSQ_TAIL_REGNUM] = fp + 5 * DEPRECATED_REGISTER_SIZE;
}

void
hppa_pop_frame (void)
{
  struct frame_info *frame = get_current_frame ();
  CORE_ADDR fp, npc, target_pc;
  int regnum;
  CORE_ADDR *fsr;
  double freg_buffer;

  fp = get_frame_base (frame);
  hppa_frame_init_saved_regs (frame);
  fsr = deprecated_get_frame_saved_regs (frame);

#ifndef NO_PC_SPACE_QUEUE_RESTORE
  if (fsr[IPSW_REGNUM])	/* Restoring a call dummy frame */
    restore_pc_queue (fsr);
#endif

  for (regnum = 31; regnum > 0; regnum--)
    if (fsr[regnum])
      write_register (regnum, read_memory_integer (fsr[regnum],
						   DEPRECATED_REGISTER_SIZE));

  for (regnum = NUM_REGS - 1; regnum >= FP0_REGNUM; regnum--)
    if (fsr[regnum])
      {
	read_memory (fsr[regnum], (char *) &freg_buffer, 8);
	deprecated_write_register_bytes (DEPRECATED_REGISTER_BYTE (regnum),
					 (char *) &freg_buffer, 8);
      }

  if (fsr[IPSW_REGNUM])
    write_register (IPSW_REGNUM,
		    read_memory_integer (fsr[IPSW_REGNUM],
					 DEPRECATED_REGISTER_SIZE));

  if (fsr[SAR_REGNUM])
    write_register (SAR_REGNUM,
		    read_memory_integer (fsr[SAR_REGNUM],
					 DEPRECATED_REGISTER_SIZE));

  /* If the PC was explicitly saved, then just restore it.  */
  if (fsr[PCOQ_TAIL_REGNUM])
    {
      npc = read_memory_integer (fsr[PCOQ_TAIL_REGNUM],
				 DEPRECATED_REGISTER_SIZE);
      write_register (PCOQ_TAIL_REGNUM, npc);
    }
  /* Else use the value in %rp to set the new PC.  */
  else
    {
      npc = read_register (RP_REGNUM);
      write_pc (npc);
    }

  write_register (DEPRECATED_FP_REGNUM, read_memory_integer (fp, DEPRECATED_REGISTER_SIZE));

  if (fsr[IPSW_REGNUM])	/* call dummy */
    write_register (SP_REGNUM, fp - 48);
  else
    write_register (SP_REGNUM, fp);

  /* The PC we just restored may be inside a return trampoline.  If so
     we want to restart the inferior and run it through the trampoline.

     Do this by setting a momentary breakpoint at the location the
     trampoline returns to. 

     Don't skip through the trampoline if we're popping a dummy frame.  */
  target_pc = SKIP_TRAMPOLINE_CODE (npc & ~0x3) & ~0x3;
  if (target_pc && !fsr[IPSW_REGNUM])
    {
      struct symtab_and_line sal;
      struct breakpoint *breakpoint;
      struct cleanup *old_chain;

      /* Set up our breakpoint.   Set it to be silent as the MI code
         for "return_command" will print the frame we returned to.  */
      sal = find_pc_line (target_pc, 0);
      sal.pc = target_pc;
      breakpoint = set_momentary_breakpoint (sal, null_frame_id, bp_finish);
      breakpoint->silent = 1;

      /* So we can clean things up.  */
      old_chain = make_cleanup_delete_breakpoint (breakpoint);

      /* Start up the inferior.  */
      clear_proceed_status ();
      proceed_to_finish = 1;
      proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 0);

      /* Perform our cleanups.  */
      do_cleanups (old_chain);
    }
  flush_cached_frames ();
}

/* After returning to a dummy on the stack, restore the instruction
   queue space registers. */

static int
restore_pc_queue (CORE_ADDR *fsr)
{
  CORE_ADDR pc = read_pc ();
  CORE_ADDR new_pc = read_memory_integer (fsr[PCOQ_HEAD_REGNUM],
					  TARGET_PTR_BIT / 8);
  struct target_waitstatus w;
  int insn_count;

  /* Advance past break instruction in the call dummy. */
  write_register (PCOQ_HEAD_REGNUM, pc + 4);
  write_register (PCOQ_TAIL_REGNUM, pc + 8);

  /* HPUX doesn't let us set the space registers or the space
     registers of the PC queue through ptrace. Boo, hiss.
     Conveniently, the call dummy has this sequence of instructions
     after the break:
     mtsp r21, sr0
     ble,n 0(sr0, r22)

     So, load up the registers and single step until we are in the
     right place. */

  write_register (21, read_memory_integer (fsr[PCSQ_HEAD_REGNUM],
					   DEPRECATED_REGISTER_SIZE));
  write_register (22, new_pc);

  for (insn_count = 0; insn_count < 3; insn_count++)
    {
      /* FIXME: What if the inferior gets a signal right now?  Want to
         merge this into wait_for_inferior (as a special kind of
         watchpoint?  By setting a breakpoint at the end?  Is there
         any other choice?  Is there *any* way to do this stuff with
         ptrace() or some equivalent?).  */
      resume (1, 0);
      target_wait (inferior_ptid, &w);

      if (w.kind == TARGET_WAITKIND_SIGNALLED)
	{
	  stop_signal = w.value.sig;
	  terminal_ours_for_output ();
	  printf_unfiltered ("\nProgram terminated with signal %s, %s.\n",
			     target_signal_to_name (stop_signal),
			     target_signal_to_string (stop_signal));
	  gdb_flush (gdb_stdout);
	  return 0;
	}
    }
  target_terminal_ours ();
  target_fetch_registers (-1);
  return 1;
}


#ifdef PA20W_CALLING_CONVENTIONS

/* This function pushes a stack frame with arguments as part of the
   inferior function calling mechanism.

   This is the version for the PA64, in which later arguments appear
   at higher addresses.  (The stack always grows towards higher
   addresses.)

   We simply allocate the appropriate amount of stack space and put
   arguments into their proper slots.  The call dummy code will copy
   arguments into registers as needed by the ABI.

   This ABI also requires that the caller provide an argument pointer
   to the callee, so we do that too.  */
   
CORE_ADDR
hppa_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
		     int struct_return, CORE_ADDR struct_addr)
{
  /* array of arguments' offsets */
  int *offset = (int *) alloca (nargs * sizeof (int));

  /* array of arguments' lengths: real lengths in bytes, not aligned to
     word size */
  int *lengths = (int *) alloca (nargs * sizeof (int));

  /* The value of SP as it was passed into this function after
     aligning.  */
  CORE_ADDR orig_sp = DEPRECATED_STACK_ALIGN (sp);

  /* The number of stack bytes occupied by the current argument.  */
  int bytes_reserved;

  /* The total number of bytes reserved for the arguments.  */
  int cum_bytes_reserved = 0;

  /* Similarly, but aligned.  */
  int cum_bytes_aligned = 0;
  int i;

  /* Iterate over each argument provided by the user.  */
  for (i = 0; i < nargs; i++)
    {
      struct type *arg_type = VALUE_TYPE (args[i]);

      /* Integral scalar values smaller than a register are padded on
         the left.  We do this by promoting them to full-width,
         although the ABI says to pad them with garbage.  */
      if (is_integral_type (arg_type)
	  && TYPE_LENGTH (arg_type) < DEPRECATED_REGISTER_SIZE)
	{
	  args[i] = value_cast ((TYPE_UNSIGNED (arg_type)
				 ? builtin_type_unsigned_long
				 : builtin_type_long),
				args[i]);
	  arg_type = VALUE_TYPE (args[i]);
	}

      lengths[i] = TYPE_LENGTH (arg_type);

      /* Align the size of the argument to the word size for this
	 target.  */
      bytes_reserved = (lengths[i] + DEPRECATED_REGISTER_SIZE - 1) & -DEPRECATED_REGISTER_SIZE;

      offset[i] = cum_bytes_reserved;

      /* Aggregates larger than eight bytes (the only types larger
         than eight bytes we have) are aligned on a 16-byte boundary,
         possibly padded on the right with garbage.  This may leave an
         empty word on the stack, and thus an unused register, as per
         the ABI.  */
      if (bytes_reserved > 8)
	{
	  /* Round up the offset to a multiple of two slots.  */
	  int new_offset = ((offset[i] + 2*DEPRECATED_REGISTER_SIZE-1)
			    & -(2*DEPRECATED_REGISTER_SIZE));

	  /* Note the space we've wasted, if any.  */
	  bytes_reserved += new_offset - offset[i];
	  offset[i] = new_offset;
	}

      cum_bytes_reserved += bytes_reserved;
    }

  /* CUM_BYTES_RESERVED already accounts for all the arguments
     passed by the user.  However, the ABIs mandate minimum stack space
     allocations for outgoing arguments.

     The ABIs also mandate minimum stack alignments which we must
     preserve.  */
  cum_bytes_aligned = DEPRECATED_STACK_ALIGN (cum_bytes_reserved);
  sp += max (cum_bytes_aligned, REG_PARM_STACK_SPACE);

  /* Now write each of the args at the proper offset down the stack.  */
  for (i = 0; i < nargs; i++)
    write_memory (orig_sp + offset[i], VALUE_CONTENTS (args[i]), lengths[i]);

  /* If a structure has to be returned, set up register 28 to hold its
     address */
  if (struct_return)
    write_register (28, struct_addr);

  /* For the PA64 we must pass a pointer to the outgoing argument list.
     The ABI mandates that the pointer should point to the first byte of
     storage beyond the register flushback area.

     However, the call dummy expects the outgoing argument pointer to
     be passed in register %r4.  */
  write_register (4, orig_sp + REG_PARM_STACK_SPACE);

  /* ?!? This needs further work.  We need to set up the global data
     pointer for this procedure.  This assumes the same global pointer
     for every procedure.   The call dummy expects the dp value to
     be passed in register %r6.  */
  write_register (6, read_register (27));
  
  /* The stack will have 64 bytes of additional space for a frame marker.  */
  return sp + 64;
}

#else

/* This function pushes a stack frame with arguments as part of the
   inferior function calling mechanism.

   This is the version of the function for the 32-bit PA machines, in
   which later arguments appear at lower addresses.  (The stack always
   grows towards higher addresses.)

   We simply allocate the appropriate amount of stack space and put
   arguments into their proper slots.  The call dummy code will copy
   arguments into registers as needed by the ABI. */
   
CORE_ADDR
hppa_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
		     int struct_return, CORE_ADDR struct_addr)
{
  /* array of arguments' offsets */
  int *offset = (int *) alloca (nargs * sizeof (int));

  /* array of arguments' lengths: real lengths in bytes, not aligned to
     word size */
  int *lengths = (int *) alloca (nargs * sizeof (int));

  /* The number of stack bytes occupied by the current argument.  */
  int bytes_reserved;

  /* The total number of bytes reserved for the arguments.  */
  int cum_bytes_reserved = 0;

  /* Similarly, but aligned.  */
  int cum_bytes_aligned = 0;
  int i;

  /* Iterate over each argument provided by the user.  */
  for (i = 0; i < nargs; i++)
    {
      lengths[i] = TYPE_LENGTH (VALUE_TYPE (args[i]));

      /* Align the size of the argument to the word size for this
	 target.  */
      bytes_reserved = (lengths[i] + DEPRECATED_REGISTER_SIZE - 1) & -DEPRECATED_REGISTER_SIZE;

      offset[i] = (cum_bytes_reserved
		   + (lengths[i] > 4 ? bytes_reserved : lengths[i]));

      /* If the argument is a double word argument, then it needs to be
	 double word aligned.  */
      if ((bytes_reserved == 2 * DEPRECATED_REGISTER_SIZE)
	  && (offset[i] % 2 * DEPRECATED_REGISTER_SIZE))
	{
	  int new_offset = 0;
	  /* BYTES_RESERVED is already aligned to the word, so we put
	     the argument at one word more down the stack.

	     This will leave one empty word on the stack, and one unused
	     register as mandated by the ABI.  */
	  new_offset = ((offset[i] + 2 * DEPRECATED_REGISTER_SIZE - 1)
			& -(2 * DEPRECATED_REGISTER_SIZE));

	  if ((new_offset - offset[i]) >= 2 * DEPRECATED_REGISTER_SIZE)
	    {
	      bytes_reserved += DEPRECATED_REGISTER_SIZE;
	      offset[i] += DEPRECATED_REGISTER_SIZE;
	    }
	}

      cum_bytes_reserved += bytes_reserved;

    }

  /* CUM_BYTES_RESERVED already accounts for all the arguments passed
     by the user.  However, the ABI mandates minimum stack space
     allocations for outgoing arguments.

     The ABI also mandates minimum stack alignments which we must
     preserve.  */
  cum_bytes_aligned = DEPRECATED_STACK_ALIGN (cum_bytes_reserved);
  sp += max (cum_bytes_aligned, REG_PARM_STACK_SPACE);

  /* Now write each of the args at the proper offset down the stack.
     ?!? We need to promote values to a full register instead of skipping
     words in the stack.  */
  for (i = 0; i < nargs; i++)
    write_memory (sp - offset[i], VALUE_CONTENTS (args[i]), lengths[i]);

  /* If a structure has to be returned, set up register 28 to hold its
     address */
  if (struct_return)
    write_register (28, struct_addr);

  /* The stack will have 32 bytes of additional space for a frame marker.  */
  return sp + 32;
}

#endif

/* This function pushes a stack frame with arguments as part of the
   inferior function calling mechanism.

   This is the version of the function for the 32-bit PA machines, in
   which later arguments appear at lower addresses.  (The stack always
   grows towards higher addresses.)

   We simply allocate the appropriate amount of stack space and put
   arguments into their proper slots.  */
   
CORE_ADDR
hppa32_push_dummy_call (struct gdbarch *gdbarch, CORE_ADDR func_addr,
			struct regcache *regcache, CORE_ADDR bp_addr,
			int nargs, struct value **args, CORE_ADDR sp,
			int struct_return, CORE_ADDR struct_addr)
{
  /* NOTE: cagney/2004-02-27: This is a guess - its implemented by
     reverse engineering testsuite failures.  */

  /* Stack base address at which any pass-by-reference parameters are
     stored.  */
  CORE_ADDR struct_end = 0;
  /* Stack base address at which the first parameter is stored.  */
  CORE_ADDR param_end = 0;

  /* The inner most end of the stack after all the parameters have
     been pushed.  */
  CORE_ADDR new_sp = 0;

  /* Two passes.  First pass computes the location of everything,
     second pass writes the bytes out.  */
  int write_pass;
  for (write_pass = 0; write_pass < 2; write_pass++)
    {
      CORE_ADDR struct_ptr = 0;
      CORE_ADDR param_ptr = 0;
      int reg = 27;	      /* NOTE: Registers go down.  */
      int i;
      for (i = 0; i < nargs; i++)
	{
	  struct value *arg = args[i];
	  struct type *type = check_typedef (VALUE_TYPE (arg));
	  /* The corresponding parameter that is pushed onto the
	     stack, and [possibly] passed in a register.  */
	  char param_val[8];
	  int param_len;
	  memset (param_val, 0, sizeof param_val);
	  if (TYPE_LENGTH (type) > 8)
	    {
	      /* Large parameter, pass by reference.  Store the value
		 in "struct" area and then pass its address.  */
	      param_len = 4;
	      struct_ptr += align_up (TYPE_LENGTH (type), 8);
	      if (write_pass)
		write_memory (struct_end - struct_ptr, VALUE_CONTENTS (arg),
			      TYPE_LENGTH (type));
	      store_unsigned_integer (param_val, 4, struct_end - struct_ptr);
	    }
	  else if (TYPE_CODE (type) == TYPE_CODE_INT
		   || TYPE_CODE (type) == TYPE_CODE_ENUM)
	    {
	      /* Integer value store, right aligned.  "unpack_long"
		 takes care of any sign-extension problems.  */
	      param_len = align_up (TYPE_LENGTH (type), 4);
	      store_unsigned_integer (param_val, param_len,
				      unpack_long (type,
						   VALUE_CONTENTS (arg)));
	    }
	  else
	    {
	      /* Small struct value, store right aligned?  */
	      param_len = align_up (TYPE_LENGTH (type), 4);
	      memcpy (param_val + param_len - TYPE_LENGTH (type),
		      VALUE_CONTENTS (arg), TYPE_LENGTH (type));
	    }
	  param_ptr += param_len;
	  reg -= param_len / 4;
	  if (write_pass)
	    {
	      write_memory (param_end - param_ptr, param_val, param_len);
	      if (reg >= 23)
		{
		  regcache_cooked_write (regcache, reg, param_val);
		  if (param_len > 4)
		    regcache_cooked_write (regcache, reg + 1, param_val + 4);
		}
	    }
	}

      /* Update the various stack pointers.  */
      if (!write_pass)
	{
	  struct_end = sp + struct_ptr;
	  /* PARAM_PTR already accounts for all the arguments passed
	     by the user.  However, the ABI mandates minimum stack
	     space allocations for outgoing arguments.  The ABI also
	     mandates minimum stack alignments which we must
	     preserve.  */
	  param_end = struct_end + max (align_up (param_ptr, 8),
					REG_PARM_STACK_SPACE);
	}
    }

  /* If a structure has to be returned, set up register 28 to hold its
     address */
  if (struct_return)
    write_register (28, struct_addr);

  /* Set the return address.  */
  regcache_cooked_write_unsigned (regcache, RP_REGNUM, bp_addr);

  /* Update the Stack Pointer.  */
  regcache_cooked_write_unsigned (regcache, SP_REGNUM, param_end + 32);

  /* The stack will have 32 bytes of additional space for a frame marker.  */
  return param_end + 32;
}

/* This function pushes a stack frame with arguments as part of the
   inferior function calling mechanism.

   This is the version for the PA64, in which later arguments appear
   at higher addresses.  (The stack always grows towards higher
   addresses.)

   We simply allocate the appropriate amount of stack space and put
   arguments into their proper slots.

   This ABI also requires that the caller provide an argument pointer
   to the callee, so we do that too.  */
   
CORE_ADDR
hppa64_push_dummy_call (struct gdbarch *gdbarch, CORE_ADDR func_addr,
			struct regcache *regcache, CORE_ADDR bp_addr,
			int nargs, struct value **args, CORE_ADDR sp,
			int struct_return, CORE_ADDR struct_addr)
{
  /* Array of arguments' offsets.  */
  int *offset = (int *) alloca (nargs * sizeof (int));

  /* Array of arguments' lengths: real lengths in bytes, not aligned
     to word size.  */
  int *lengths = (int *) alloca (nargs * sizeof (int));

  /* The value of SP as it was passed into this function.  */
  CORE_ADDR orig_sp = sp;

  /* The number of stack bytes occupied by the current argument.  */
  int bytes_reserved;

  /* The total number of bytes reserved for the arguments.  */
  int cum_bytes_reserved = 0;

  /* Similarly, but aligned.  */
  int cum_bytes_aligned = 0;
  int i;

  /* Iterate over each argument provided by the user.  */
  for (i = 0; i < nargs; i++)
    {
      struct type *arg_type = VALUE_TYPE (args[i]);

      /* Integral scalar values smaller than a register are padded on
         the left.  We do this by promoting them to full-width,
         although the ABI says to pad them with garbage.  */
      if (is_integral_type (arg_type)
	  && TYPE_LENGTH (arg_type) < DEPRECATED_REGISTER_SIZE)
	{
	  args[i] = value_cast ((TYPE_UNSIGNED (arg_type)
				 ? builtin_type_unsigned_long
				 : builtin_type_long),
				args[i]);
	  arg_type = VALUE_TYPE (args[i]);
	}

      lengths[i] = TYPE_LENGTH (arg_type);

      /* Align the size of the argument to the word size for this
	 target.  */
      bytes_reserved = (lengths[i] + DEPRECATED_REGISTER_SIZE - 1) & -DEPRECATED_REGISTER_SIZE;

      offset[i] = cum_bytes_reserved;

      /* Aggregates larger than eight bytes (the only types larger
         than eight bytes we have) are aligned on a 16-byte boundary,
         possibly padded on the right with garbage.  This may leave an
         empty word on the stack, and thus an unused register, as per
         the ABI.  */
      if (bytes_reserved > 8)
	{
	  /* Round up the offset to a multiple of two slots.  */
	  int new_offset = ((offset[i] + 2*DEPRECATED_REGISTER_SIZE-1)
			    & -(2*DEPRECATED_REGISTER_SIZE));

	  /* Note the space we've wasted, if any.  */
	  bytes_reserved += new_offset - offset[i];
	  offset[i] = new_offset;
	}

      cum_bytes_reserved += bytes_reserved;
    }

  /* CUM_BYTES_RESERVED already accounts for all the arguments passed
     by the user.  However, the ABIs mandate minimum stack space
     allocations for outgoing arguments.

     The ABIs also mandate minimum stack alignments which we must
     preserve.  */
  cum_bytes_aligned = align_up (cum_bytes_reserved, 16);
  sp += max (cum_bytes_aligned, REG_PARM_STACK_SPACE);

  /* Now write each of the args at the proper offset down the
     stack.  */
  for (i = 0; i < nargs; i++)
    write_memory (orig_sp + offset[i], VALUE_CONTENTS (args[i]), lengths[i]);

  /* If a structure has to be returned, set up register 28 to hold its
     address */
  if (struct_return)
    write_register (28, struct_addr);

  /* For the PA64 we must pass a pointer to the outgoing argument
     list.  The ABI mandates that the pointer should point to the
     first byte of storage beyond the register flushback area.

     However, the call dummy expects the outgoing argument pointer to
     be passed in register %r4.  */
  write_register (4, orig_sp + REG_PARM_STACK_SPACE);

  /* ?!? This needs further work.  We need to set up the global data
     pointer for this procedure.  This assumes the same global pointer
     for every procedure.  The call dummy expects the dp value to be
     passed in register %r6.  */
  write_register (6, read_register (27));
  
  /* Set the return address.  */
  regcache_cooked_write_unsigned (regcache, RP_REGNUM, bp_addr);

  /* Update the Stack Pointer.  */
  regcache_cooked_write_unsigned (regcache, SP_REGNUM, sp + 64);

  /* The stack will have 64 bytes of additional space for a frame
     marker.  */
  return sp + 64;

}

static CORE_ADDR
hppa32_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  /* HP frames are 64-byte (or cache line) aligned (yes that's _byte_
     and not _bit_)!  */
  return align_up (addr, 64);
}

/* Force all frames to 16-byte alignment.  Better safe than sorry.  */

static CORE_ADDR
hppa64_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  /* Just always 16-byte align.  */
  return align_up (addr, 16);
}


/* elz: Used to lookup a symbol in the shared libraries.
   This function calls shl_findsym, indirectly through a
   call to __d_shl_get. __d_shl_get is in end.c, which is always
   linked in by the hp compilers/linkers. 
   The call to shl_findsym cannot be made directly because it needs
   to be active in target address space. 
   inputs: - minimal symbol pointer for the function we want to look up
   - address in target space of the descriptor for the library
   where we want to look the symbol up.
   This address is retrieved using the 
   som_solib_get_solib_by_pc function (somsolib.c). 
   output: - real address in the library of the function.          
   note: the handle can be null, in which case shl_findsym will look for
   the symbol in all the loaded shared libraries.
   files to look at if you need reference on this stuff:
   dld.c, dld_shl_findsym.c
   end.c
   man entry for shl_findsym */

CORE_ADDR
find_stub_with_shl_get (struct minimal_symbol *function, CORE_ADDR handle)
{
  struct symbol *get_sym, *symbol2;
  struct minimal_symbol *buff_minsym, *msymbol;
  struct type *ftype;
  struct value **args;
  struct value *funcval;
  struct value *val;

  int x, namelen, err_value, tmp = -1;
  CORE_ADDR endo_buff_addr, value_return_addr, errno_return_addr;
  CORE_ADDR stub_addr;


  args = alloca (sizeof (struct value *) * 8);		/* 6 for the arguments and one null one??? */
  funcval = find_function_in_inferior ("__d_shl_get");
  get_sym = lookup_symbol ("__d_shl_get", NULL, VAR_DOMAIN, NULL, NULL);
  buff_minsym = lookup_minimal_symbol ("__buffer", NULL, NULL);
  msymbol = lookup_minimal_symbol ("__shldp", NULL, NULL);
  symbol2 = lookup_symbol ("__shldp", NULL, VAR_DOMAIN, NULL, NULL);
  endo_buff_addr = SYMBOL_VALUE_ADDRESS (buff_minsym);
  namelen = strlen (DEPRECATED_SYMBOL_NAME (function));
  value_return_addr = endo_buff_addr + namelen;
  ftype = check_typedef (SYMBOL_TYPE (get_sym));

  /* do alignment */
  if ((x = value_return_addr % 64) != 0)
    value_return_addr = value_return_addr + 64 - x;

  errno_return_addr = value_return_addr + 64;


  /* set up stuff needed by __d_shl_get in buffer in end.o */

  target_write_memory (endo_buff_addr, DEPRECATED_SYMBOL_NAME (function), namelen);

  target_write_memory (value_return_addr, (char *) &tmp, 4);

  target_write_memory (errno_return_addr, (char *) &tmp, 4);

  target_write_memory (SYMBOL_VALUE_ADDRESS (msymbol),
		       (char *) &handle, 4);

  /* now prepare the arguments for the call */

  args[0] = value_from_longest (TYPE_FIELD_TYPE (ftype, 0), 12);
  args[1] = value_from_pointer (TYPE_FIELD_TYPE (ftype, 1), SYMBOL_VALUE_ADDRESS (msymbol));
  args[2] = value_from_pointer (TYPE_FIELD_TYPE (ftype, 2), endo_buff_addr);
  args[3] = value_from_longest (TYPE_FIELD_TYPE (ftype, 3), TYPE_PROCEDURE);
  args[4] = value_from_pointer (TYPE_FIELD_TYPE (ftype, 4), value_return_addr);
  args[5] = value_from_pointer (TYPE_FIELD_TYPE (ftype, 5), errno_return_addr);

  /* now call the function */

  val = call_function_by_hand (funcval, 6, args);

  /* now get the results */

  target_read_memory (errno_return_addr, (char *) &err_value, sizeof (err_value));

  target_read_memory (value_return_addr, (char *) &stub_addr, sizeof (stub_addr));
  if (stub_addr <= 0)
    error ("call to __d_shl_get failed, error code is %d", err_value);

  return (stub_addr);
}

/* Cover routine for find_stub_with_shl_get to pass to catch_errors */
static int
cover_find_stub_with_shl_get (void *args_untyped)
{
  args_for_find_stub *args = args_untyped;
  args->return_val = find_stub_with_shl_get (args->msym, args->solib_handle);
  return 0;
}

/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.

   On the hppa we need to call the stack dummy through $$dyncall.
   Therefore our version of DEPRECATED_FIX_CALL_DUMMY takes an extra
   argument, real_pc, which is the location where gdb should start up
   the inferior to do the function call.

   This has to work across several versions of hpux, bsd, osf1.  It has to
   work regardless of what compiler was used to build the inferior program.
   It should work regardless of whether or not end.o is available.  It has
   to work even if gdb can not call into the dynamic loader in the inferior
   to query it for symbol names and addresses.

   Yes, all those cases should work.  Luckily code exists to handle most
   of them.  The complexity is in selecting exactly what scheme should
   be used to perform the inferior call.

   At the current time this routine is known not to handle cases where
   the program was linked with HP's compiler without including end.o.

   Please contact Jeff Law (law@cygnus.com) before changing this code.  */

CORE_ADDR
hppa_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR fun, int nargs,
		     struct value **args, struct type *type, int gcc_p)
{
  CORE_ADDR dyncall_addr;
  struct minimal_symbol *msymbol;
  struct minimal_symbol *trampoline;
  int flags = read_register (FLAGS_REGNUM);
  struct unwind_table_entry *u = NULL;
  CORE_ADDR new_stub = 0;
  CORE_ADDR solib_handle = 0;

  /* Nonzero if we will use GCC's PLT call routine.  This routine must be
     passed an import stub, not a PLABEL.  It is also necessary to set %r19
     (the PIC register) before performing the call.

     If zero, then we are using __d_plt_call (HP's PLT call routine) or we
     are calling the target directly.  When using __d_plt_call we want to
     use a PLABEL instead of an import stub.  */
  int using_gcc_plt_call = 1;

#ifdef GDB_TARGET_IS_HPPA_20W
  /* We currently use completely different code for the PA2.0W inferior
     function call sequences.  This needs to be cleaned up.  */
  {
    CORE_ADDR pcsqh, pcsqt, pcoqh, pcoqt, sr5;
    struct target_waitstatus w;
    int inst1, inst2;
    char buf[4];
    int status;
    struct objfile *objfile;

    /* We can not modify the PC space queues directly, so we start
       up the inferior and execute a couple instructions to set the
       space queues so that they point to the call dummy in the stack.  */
    pcsqh = read_register (PCSQ_HEAD_REGNUM);
    sr5 = read_register (SR5_REGNUM);
    if (1)
      {
        pcoqh = read_register (PCOQ_HEAD_REGNUM);
        pcoqt = read_register (PCOQ_TAIL_REGNUM);
        if (target_read_memory (pcoqh, buf, 4) != 0)
          error ("Couldn't modify space queue\n");
        inst1 = extract_unsigned_integer (buf, 4);

        if (target_read_memory (pcoqt, buf, 4) != 0)
          error ("Couldn't modify space queue\n");
        inst2 = extract_unsigned_integer (buf, 4);

        /* BVE (r1) */
        *((int *) buf) = 0xe820d000;
        if (target_write_memory (pcoqh, buf, 4) != 0)
          error ("Couldn't modify space queue\n");

        /* NOP */
        *((int *) buf) = 0x08000240;
        if (target_write_memory (pcoqt, buf, 4) != 0)
          {
            *((int *) buf) = inst1;
            target_write_memory (pcoqh, buf, 4);
            error ("Couldn't modify space queue\n");
          }

        write_register (1, pc);

        /* Single step twice, the BVE instruction will set the space queue
	   such that it points to the PC value written immediately above
	   (ie the call dummy).  */
        resume (1, 0);
        target_wait (inferior_ptid, &w);
        resume (1, 0);
        target_wait (inferior_ptid, &w);

	/* Restore the two instructions at the old PC locations.  */
        *((int *) buf) = inst1;
        target_write_memory (pcoqh, buf, 4);
        *((int *) buf) = inst2;
        target_write_memory (pcoqt, buf, 4);
      }

    /* The call dummy wants the ultimate destination address initially
       in register %r5.  */
    write_register (5, fun);

    /* We need to see if this objfile has a different DP value than our
       own (it could be a shared library for example).  */
    ALL_OBJFILES (objfile)
      {
	struct obj_section *s;
	obj_private_data_t *obj_private;

	/* See if FUN is in any section within this shared library.  */
	for (s = objfile->sections; s < objfile->sections_end; s++)
	  if (s->addr <= fun && fun < s->endaddr)
	    break;

        if (s >= objfile->sections_end)
	  continue;

	obj_private = (obj_private_data_t *) objfile->obj_private;
	
	/* The DP value may be different for each objfile.  But within an
	   objfile each function uses the same dp value.  Thus we do not need
	   to grope around the opd section looking for dp values.

	   ?!? This is not strictly correct since we may be in a shared library
	   and want to call back into the main program.  To make that case
	   work correctly we need to set obj_private->dp for the main program's
	   objfile, then remove this conditional.  */
	if (obj_private->dp)
	  write_register (27, obj_private->dp);
	break;
      }
    return pc;
  }
#endif

#ifndef GDB_TARGET_IS_HPPA_20W
  /* Prefer __gcc_plt_call over the HP supplied routine because
     __gcc_plt_call works for any number of arguments.  */
  trampoline = NULL;
  if (lookup_minimal_symbol ("__gcc_plt_call", NULL, NULL) == NULL)
    using_gcc_plt_call = 0;

  msymbol = lookup_minimal_symbol ("$$dyncall", NULL, NULL);
  if (msymbol == NULL)
    error ("Can't find an address for $$dyncall trampoline");

  dyncall_addr = SYMBOL_VALUE_ADDRESS (msymbol);

  /* FUN could be a procedure label, in which case we have to get
     its real address and the value of its GOT/DP if we plan to
     call the routine via gcc_plt_call.  */
  if ((fun & 0x2) && using_gcc_plt_call)
    {
      /* Get the GOT/DP value for the target function.  It's
         at *(fun+4).  Note the call dummy is *NOT* allowed to
         trash %r19 before calling the target function.  */
      write_register (19, read_memory_integer ((fun & ~0x3) + 4,
					       DEPRECATED_REGISTER_SIZE));

      /* Now get the real address for the function we are calling, it's
         at *fun.  */
      fun = (CORE_ADDR) read_memory_integer (fun & ~0x3,
					     TARGET_PTR_BIT / 8);
    }
  else
    {

#ifndef GDB_TARGET_IS_PA_ELF
      /* FUN could be an export stub, the real address of a function, or
         a PLABEL.  When using gcc's PLT call routine we must call an import
         stub rather than the export stub or real function for lazy binding
         to work correctly

         If we are using the gcc PLT call routine, then we need to
         get the import stub for the target function.  */
      if (using_gcc_plt_call && som_solib_get_got_by_pc (fun))
	{
	  struct objfile *objfile;
	  struct minimal_symbol *funsymbol, *stub_symbol;
	  CORE_ADDR newfun = 0;

	  funsymbol = lookup_minimal_symbol_by_pc (fun);
	  if (!funsymbol)
	    error ("Unable to find minimal symbol for target function.\n");

	  /* Search all the object files for an import symbol with the
	     right name. */
	  ALL_OBJFILES (objfile)
	  {
	    stub_symbol
	      = lookup_minimal_symbol_solib_trampoline
	      (DEPRECATED_SYMBOL_NAME (funsymbol), objfile);

	    if (!stub_symbol)
	      stub_symbol = lookup_minimal_symbol (DEPRECATED_SYMBOL_NAME (funsymbol),
						   NULL, objfile);

	    /* Found a symbol with the right name.  */
	    if (stub_symbol)
	      {
		struct unwind_table_entry *u;
		/* It must be a shared library trampoline.  */
		if (MSYMBOL_TYPE (stub_symbol) != mst_solib_trampoline)
		  continue;

		/* It must also be an import stub.  */
		u = find_unwind_entry (SYMBOL_VALUE (stub_symbol));
		if (u == NULL
		    || (u->stub_unwind.stub_type != IMPORT
#ifdef GDB_NATIVE_HPUX_11
			/* Sigh.  The hpux 10.20 dynamic linker will blow
			   chunks if we perform a call to an unbound function
			   via the IMPORT_SHLIB stub.  The hpux 11.00 dynamic
			   linker will blow chunks if we do not call the
			   unbound function via the IMPORT_SHLIB stub.

			   We currently have no way to select bevahior on just
			   the target.  However, we only support HPUX/SOM in
			   native mode.  So we conditinalize on a native
			   #ifdef.  Ugly.  Ugly.  Ugly  */
			&& u->stub_unwind.stub_type != IMPORT_SHLIB
#endif
			))
		  continue;

		/* OK.  Looks like the correct import stub.  */
		newfun = SYMBOL_VALUE (stub_symbol);
		fun = newfun;

		/* If we found an IMPORT stub, then we want to stop
		   searching now.  If we found an IMPORT_SHLIB, we want
		   to continue the search in the hopes that we will find
		   an IMPORT stub.  */
		if (u->stub_unwind.stub_type == IMPORT)
		  break;
	      }
	  }

	  /* Ouch.  We did not find an import stub.  Make an attempt to
	     do the right thing instead of just croaking.  Most of the
	     time this will actually work.  */
	  if (newfun == 0)
	    write_register (19, som_solib_get_got_by_pc (fun));

	  u = find_unwind_entry (fun);
	  if (u
	      && (u->stub_unwind.stub_type == IMPORT
		  || u->stub_unwind.stub_type == IMPORT_SHLIB))
	    trampoline = lookup_minimal_symbol ("__gcc_plt_call", NULL, NULL);

	  /* If we found the import stub in the shared library, then we have
	     to set %r19 before we call the stub.  */
	  if (u && u->stub_unwind.stub_type == IMPORT_SHLIB)
	    write_register (19, som_solib_get_got_by_pc (fun));
	}
#endif
    }

  /* If we are calling into another load module then have sr4export call the
     magic __d_plt_call routine which is linked in from end.o.

     You can't use _sr4export to make the call as the value in sp-24 will get
     fried and you end up returning to the wrong location.  You can't call the
     target as the code to bind the PLT entry to a function can't return to a
     stack address.

     Also, query the dynamic linker in the inferior to provide a suitable
     PLABEL for the target function.  */
  if (!using_gcc_plt_call)
    {
      CORE_ADDR new_fun;

      /* Get a handle for the shared library containing FUN.  Given the
         handle we can query the shared library for a PLABEL.  */
      solib_handle = som_solib_get_solib_by_pc (fun);

      if (solib_handle)
	{
	  struct minimal_symbol *fmsymbol = lookup_minimal_symbol_by_pc (fun);

	  trampoline = lookup_minimal_symbol ("__d_plt_call", NULL, NULL);

	  if (trampoline == NULL)
	    {
	      error ("Can't find an address for __d_plt_call or __gcc_plt_call trampoline\nSuggest linking executable with -g or compiling with gcc.");
	    }

	  /* This is where sr4export will jump to.  */
	  new_fun = SYMBOL_VALUE_ADDRESS (trampoline);

	  /* If the function is in a shared library, then call __d_shl_get to
	     get a PLABEL for the target function.  */
	  new_stub = find_stub_with_shl_get (fmsymbol, solib_handle);

	  if (new_stub == 0)
	    error ("Can't find an import stub for %s", DEPRECATED_SYMBOL_NAME (fmsymbol));

	  /* We have to store the address of the stub in __shlib_funcptr.  */
	  msymbol = lookup_minimal_symbol ("__shlib_funcptr", NULL,
					   (struct objfile *) NULL);

	  if (msymbol == NULL)
	    error ("Can't find an address for __shlib_funcptr");
	  target_write_memory (SYMBOL_VALUE_ADDRESS (msymbol),
			       (char *) &new_stub, 4);

	  /* We want sr4export to call __d_plt_call, so we claim it is
	     the final target.  Clear trampoline.  */
	  fun = new_fun;
	  trampoline = NULL;
	}
    }

  /* Store upper 21 bits of function address into ldil.  fun will either be
     the final target (most cases) or __d_plt_call when calling into a shared
     library and __gcc_plt_call is not available.  */
  store_unsigned_integer
    (&dummy[FUNC_LDIL_OFFSET],
     INSTRUCTION_SIZE,
     deposit_21 (fun >> 11,
		 extract_unsigned_integer (&dummy[FUNC_LDIL_OFFSET],
					   INSTRUCTION_SIZE)));

  /* Store lower 11 bits of function address into ldo */
  store_unsigned_integer
    (&dummy[FUNC_LDO_OFFSET],
     INSTRUCTION_SIZE,
     deposit_14 (fun & MASK_11,
		 extract_unsigned_integer (&dummy[FUNC_LDO_OFFSET],
					   INSTRUCTION_SIZE)));
#ifdef SR4EXPORT_LDIL_OFFSET

  {
    CORE_ADDR trampoline_addr;

    /* We may still need sr4export's address too.  */

    if (trampoline == NULL)
      {
	msymbol = lookup_minimal_symbol ("_sr4export", NULL, NULL);
	if (msymbol == NULL)
	  error ("Can't find an address for _sr4export trampoline");

	trampoline_addr = SYMBOL_VALUE_ADDRESS (msymbol);
      }
    else
      trampoline_addr = SYMBOL_VALUE_ADDRESS (trampoline);


    /* Store upper 21 bits of trampoline's address into ldil */
    store_unsigned_integer
      (&dummy[SR4EXPORT_LDIL_OFFSET],
       INSTRUCTION_SIZE,
       deposit_21 (trampoline_addr >> 11,
		   extract_unsigned_integer (&dummy[SR4EXPORT_LDIL_OFFSET],
					     INSTRUCTION_SIZE)));

    /* Store lower 11 bits of trampoline's address into ldo */
    store_unsigned_integer
      (&dummy[SR4EXPORT_LDO_OFFSET],
       INSTRUCTION_SIZE,
       deposit_14 (trampoline_addr & MASK_11,
		   extract_unsigned_integer (&dummy[SR4EXPORT_LDO_OFFSET],
					     INSTRUCTION_SIZE)));
  }
#endif

  write_register (22, pc);

  /* If we are in a syscall, then we should call the stack dummy
     directly.  $$dyncall is not needed as the kernel sets up the
     space id registers properly based on the value in %r31.  In
     fact calling $$dyncall will not work because the value in %r22
     will be clobbered on the syscall exit path. 

     Similarly if the current PC is in a shared library.  Note however,
     this scheme won't work if the shared library isn't mapped into
     the same space as the stack.  */
  if (flags & 2)
    return pc;
#ifndef GDB_TARGET_IS_PA_ELF
  else if (som_solib_get_got_by_pc (hppa_target_read_pc (inferior_ptid)))
    return pc;
#endif
  else
    return dyncall_addr;
#endif
}

/* If the pid is in a syscall, then the FP register is not readable.
   We'll return zero in that case, rather than attempting to read it
   and cause a warning. */

CORE_ADDR
hppa_read_fp (int pid)
{
  int flags = read_register (FLAGS_REGNUM);

  if (flags & 2)
    {
      return (CORE_ADDR) 0;
    }

  /* This is the only site that may directly read_register () the FP
     register.  All others must use deprecated_read_fp (). */
  return read_register (DEPRECATED_FP_REGNUM);
}

CORE_ADDR
hppa_target_read_fp (void)
{
  return hppa_read_fp (PIDGET (inferior_ptid));
}

/* Get the PC from %r31 if currently in a syscall.  Also mask out privilege
   bits.  */

CORE_ADDR
hppa_target_read_pc (ptid_t ptid)
{
  int flags = read_register_pid (FLAGS_REGNUM, ptid);

  /* The following test does not belong here.  It is OS-specific, and belongs
     in native code.  */
  /* Test SS_INSYSCALL */
  if (flags & 2)
    return read_register_pid (31, ptid) & ~0x3;

  return read_register_pid (PC_REGNUM, ptid) & ~0x3;
}

/* Write out the PC.  If currently in a syscall, then also write the new
   PC value into %r31.  */

void
hppa_target_write_pc (CORE_ADDR v, ptid_t ptid)
{
  int flags = read_register_pid (FLAGS_REGNUM, ptid);

  /* The following test does not belong here.  It is OS-specific, and belongs
     in native code.  */
  /* If in a syscall, then set %r31.  Also make sure to get the 
     privilege bits set correctly.  */
  /* Test SS_INSYSCALL */
  if (flags & 2)
    write_register_pid (31, v | 0x3, ptid);

  write_register_pid (PC_REGNUM, v, ptid);
  write_register_pid (PCOQ_TAIL_REGNUM, v + 4, ptid);
}

/* return the alignment of a type in bytes. Structures have the maximum
   alignment required by their fields. */

static int
hppa_alignof (struct type *type)
{
  int max_align, align, i;
  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_PTR:
    case TYPE_CODE_INT:
    case TYPE_CODE_FLT:
      return TYPE_LENGTH (type);
    case TYPE_CODE_ARRAY:
      return hppa_alignof (TYPE_FIELD_TYPE (type, 0));
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      max_align = 1;
      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  /* Bit fields have no real alignment. */
	  /* if (!TYPE_FIELD_BITPOS (type, i)) */
	  if (!TYPE_FIELD_BITSIZE (type, i))	/* elz: this should be bitsize */
	    {
	      align = hppa_alignof (TYPE_FIELD_TYPE (type, i));
	      max_align = max (max_align, align);
	    }
	}
      return max_align;
    default:
      return 4;
    }
}

/* Print the register regnum, or all registers if regnum is -1 */

void
pa_do_registers_info (int regnum, int fpregs)
{
  char *raw_regs = alloca (DEPRECATED_REGISTER_BYTES);
  int i;

  /* Make a copy of gdb's save area (may cause actual
     reads from the target). */
  for (i = 0; i < NUM_REGS; i++)
    frame_register_read (deprecated_selected_frame, i,
			 raw_regs + DEPRECATED_REGISTER_BYTE (i));

  if (regnum == -1)
    pa_print_registers (raw_regs, regnum, fpregs);
  else if (regnum < FP4_REGNUM)
    {
      long reg_val[2];

      /* Why is the value not passed through "extract_signed_integer"
         as in "pa_print_registers" below? */
      pa_register_look_aside (raw_regs, regnum, &reg_val[0]);

      if (!is_pa_2)
	{
	  printf_unfiltered ("%s %lx\n", REGISTER_NAME (regnum), reg_val[1]);
	}
      else
	{
	  /* Fancy % formats to prevent leading zeros. */
	  if (reg_val[0] == 0)
	    printf_unfiltered ("%s %lx\n", REGISTER_NAME (regnum), reg_val[1]);
	  else
	    printf_unfiltered ("%s %lx%8.8lx\n", REGISTER_NAME (regnum),
			       reg_val[0], reg_val[1]);
	}
    }
  else
    /* Note that real floating point values only start at
       FP4_REGNUM.  FP0 and up are just status and error
       registers, which have integral (bit) values. */
    pa_print_fp_reg (regnum);
}

/********** new function ********************/
void
pa_do_strcat_registers_info (int regnum, int fpregs, struct ui_file *stream,
			     enum precision_type precision)
{
  char *raw_regs = alloca (DEPRECATED_REGISTER_BYTES);
  int i;

  /* Make a copy of gdb's save area (may cause actual
     reads from the target). */
  for (i = 0; i < NUM_REGS; i++)
    frame_register_read (deprecated_selected_frame, i,
			 raw_regs + DEPRECATED_REGISTER_BYTE (i));

  if (regnum == -1)
    pa_strcat_registers (raw_regs, regnum, fpregs, stream);

  else if (regnum < FP4_REGNUM)
    {
      long reg_val[2];

      /* Why is the value not passed through "extract_signed_integer"
         as in "pa_print_registers" below? */
      pa_register_look_aside (raw_regs, regnum, &reg_val[0]);

      if (!is_pa_2)
	{
	  fprintf_unfiltered (stream, "%s %lx", REGISTER_NAME (regnum), reg_val[1]);
	}
      else
	{
	  /* Fancy % formats to prevent leading zeros. */
	  if (reg_val[0] == 0)
	    fprintf_unfiltered (stream, "%s %lx", REGISTER_NAME (regnum),
				reg_val[1]);
	  else
	    fprintf_unfiltered (stream, "%s %lx%8.8lx", REGISTER_NAME (regnum),
				reg_val[0], reg_val[1]);
	}
    }
  else
    /* Note that real floating point values only start at
       FP4_REGNUM.  FP0 and up are just status and error
       registers, which have integral (bit) values. */
    pa_strcat_fp_reg (regnum, stream, precision);
}

/* If this is a PA2.0 machine, fetch the real 64-bit register
   value.  Otherwise use the info from gdb's saved register area.

   Note that reg_val is really expected to be an array of longs,
   with two elements. */
static void
pa_register_look_aside (char *raw_regs, int regnum, long *raw_val)
{
  static int know_which = 0;	/* False */

  int regaddr;
  unsigned int offset;
  int i;
  int start;


  char buf[MAX_REGISTER_SIZE];
  long long reg_val;

  if (!know_which)
    {
      if (CPU_PA_RISC2_0 == sysconf (_SC_CPU_VERSION))
	{
	  is_pa_2 = (1 == 1);
	}

      know_which = 1;		/* True */
    }

  raw_val[0] = 0;
  raw_val[1] = 0;

  if (!is_pa_2)
    {
      raw_val[1] = *(long *) (raw_regs + DEPRECATED_REGISTER_BYTE (regnum));
      return;
    }

  /* Code below copied from hppah-nat.c, with fixes for wide
     registers, using different area of save_state, etc. */
  if (regnum == FLAGS_REGNUM || regnum >= FP0_REGNUM ||
      !HAVE_STRUCT_SAVE_STATE_T || !HAVE_STRUCT_MEMBER_SS_WIDE)
    {
      /* Use narrow regs area of save_state and default macro. */
      offset = U_REGS_OFFSET;
      regaddr = register_addr (regnum, offset);
      start = 1;
    }
  else
    {
      /* Use wide regs area, and calculate registers as 8 bytes wide.

         We'd like to do this, but current version of "C" doesn't
         permit "offsetof":

         offset  = offsetof(save_state_t, ss_wide);

         Note that to avoid "C" doing typed pointer arithmetic, we
         have to cast away the type in our offset calculation:
         otherwise we get an offset of 1! */

      /* NB: save_state_t is not available before HPUX 9.
         The ss_wide field is not available previous to HPUX 10.20,
         so to avoid compile-time warnings, we only compile this for
         PA 2.0 processors.  This control path should only be followed
         if we're debugging a PA 2.0 processor, so this should not cause
         problems. */

      /* #if the following code out so that this file can still be
         compiled on older HPUX boxes (< 10.20) which don't have
         this structure/structure member.  */
#if HAVE_STRUCT_SAVE_STATE_T == 1 && HAVE_STRUCT_MEMBER_SS_WIDE == 1
      save_state_t temp;

      offset = ((int) &temp.ss_wide) - ((int) &temp);
      regaddr = offset + regnum * 8;
      start = 0;
#endif
    }

  for (i = start; i < 2; i++)
    {
      errno = 0;
      raw_val[i] = call_ptrace (PT_RUREGS, PIDGET (inferior_ptid),
				(PTRACE_ARG3_TYPE) regaddr, 0);
      if (errno != 0)
	{
	  /* Warning, not error, in case we are attached; sometimes the
	     kernel doesn't let us at the registers.  */
	  char *err = safe_strerror (errno);
	  char *msg = alloca (strlen (err) + 128);
	  sprintf (msg, "reading register %s: %s", REGISTER_NAME (regnum), err);
	  warning (msg);
	  goto error_exit;
	}

      regaddr += sizeof (long);
    }

  if (regnum == PCOQ_HEAD_REGNUM || regnum == PCOQ_TAIL_REGNUM)
    raw_val[1] &= ~0x3;		/* I think we're masking out space bits */

error_exit:
  ;
}

/* "Info all-reg" command */

static void
pa_print_registers (char *raw_regs, int regnum, int fpregs)
{
  int i, j;
  /* Alas, we are compiled so that "long long" is 32 bits */
  long raw_val[2];
  long long_val;
  int rows = 48, columns = 2;

  for (i = 0; i < rows; i++)
    {
      for (j = 0; j < columns; j++)
	{
	  /* We display registers in column-major order.  */
	  int regnum = i + j * rows;

	  /* Q: Why is the value passed through "extract_signed_integer",
	     while above, in "pa_do_registers_info" it isn't?
	     A: ? */
	  pa_register_look_aside (raw_regs, regnum, &raw_val[0]);

	  /* Even fancier % formats to prevent leading zeros
	     and still maintain the output in columns. */
	  if (!is_pa_2)
	    {
	      /* Being big-endian, on this machine the low bits
	         (the ones we want to look at) are in the second longword. */
	      long_val = extract_signed_integer (&raw_val[1], 4);
	      printf_filtered ("%10.10s: %8lx   ",
			       REGISTER_NAME (regnum), long_val);
	    }
	  else
	    {
	      /* raw_val = extract_signed_integer(&raw_val, 8); */
	      if (raw_val[0] == 0)
		printf_filtered ("%10.10s:         %8lx   ",
				 REGISTER_NAME (regnum), raw_val[1]);
	      else
		printf_filtered ("%10.10s: %8lx%8.8lx   ",
				 REGISTER_NAME (regnum),
				 raw_val[0], raw_val[1]);
	    }
	}
      printf_unfiltered ("\n");
    }

  if (fpregs)
    for (i = FP4_REGNUM; i < NUM_REGS; i++)	/* FP4_REGNUM == 72 */
      pa_print_fp_reg (i);
}

/************* new function ******************/
static void
pa_strcat_registers (char *raw_regs, int regnum, int fpregs,
		     struct ui_file *stream)
{
  int i, j;
  long raw_val[2];		/* Alas, we are compiled so that "long long" is 32 bits */
  long long_val;
  enum precision_type precision;

  precision = unspecified_precision;

  for (i = 0; i < 18; i++)
    {
      for (j = 0; j < 4; j++)
	{
	  /* Q: Why is the value passed through "extract_signed_integer",
	     while above, in "pa_do_registers_info" it isn't?
	     A: ? */
	  pa_register_look_aside (raw_regs, i + (j * 18), &raw_val[0]);

	  /* Even fancier % formats to prevent leading zeros
	     and still maintain the output in columns. */
	  if (!is_pa_2)
	    {
	      /* Being big-endian, on this machine the low bits
	         (the ones we want to look at) are in the second longword. */
	      long_val = extract_signed_integer (&raw_val[1], 4);
	      fprintf_filtered (stream, "%8.8s: %8lx  ",
				REGISTER_NAME (i + (j * 18)), long_val);
	    }
	  else
	    {
	      /* raw_val = extract_signed_integer(&raw_val, 8); */
	      if (raw_val[0] == 0)
		fprintf_filtered (stream, "%8.8s:         %8lx  ",
				  REGISTER_NAME (i + (j * 18)), raw_val[1]);
	      else
		fprintf_filtered (stream, "%8.8s: %8lx%8.8lx  ",
				  REGISTER_NAME (i + (j * 18)), raw_val[0],
				  raw_val[1]);
	    }
	}
      fprintf_unfiltered (stream, "\n");
    }

  if (fpregs)
    for (i = FP4_REGNUM; i < NUM_REGS; i++)	/* FP4_REGNUM == 72 */
      pa_strcat_fp_reg (i, stream, precision);
}

static void
pa_print_fp_reg (int i)
{
  char raw_buffer[MAX_REGISTER_SIZE];
  char virtual_buffer[MAX_REGISTER_SIZE];

  /* Get 32bits of data.  */
  frame_register_read (deprecated_selected_frame, i, raw_buffer);

  /* Put it in the buffer.  No conversions are ever necessary.  */
  memcpy (virtual_buffer, raw_buffer, DEPRECATED_REGISTER_RAW_SIZE (i));

  fputs_filtered (REGISTER_NAME (i), gdb_stdout);
  print_spaces_filtered (8 - strlen (REGISTER_NAME (i)), gdb_stdout);
  fputs_filtered ("(single precision)     ", gdb_stdout);

  val_print (DEPRECATED_REGISTER_VIRTUAL_TYPE (i), virtual_buffer, 0, 0, gdb_stdout, 0,
	     1, 0, Val_pretty_default);
  printf_filtered ("\n");

  /* If "i" is even, then this register can also be a double-precision
     FP register.  Dump it out as such.  */
  if ((i % 2) == 0)
    {
      /* Get the data in raw format for the 2nd half.  */
      frame_register_read (deprecated_selected_frame, i + 1, raw_buffer);

      /* Copy it into the appropriate part of the virtual buffer.  */
      memcpy (virtual_buffer + DEPRECATED_REGISTER_RAW_SIZE (i), raw_buffer,
	      DEPRECATED_REGISTER_RAW_SIZE (i));

      /* Dump it as a double.  */
      fputs_filtered (REGISTER_NAME (i), gdb_stdout);
      print_spaces_filtered (8 - strlen (REGISTER_NAME (i)), gdb_stdout);
      fputs_filtered ("(double precision)     ", gdb_stdout);

      val_print (builtin_type_double, virtual_buffer, 0, 0, gdb_stdout, 0,
		 1, 0, Val_pretty_default);
      printf_filtered ("\n");
    }
}

/*************** new function ***********************/
static void
pa_strcat_fp_reg (int i, struct ui_file *stream, enum precision_type precision)
{
  char raw_buffer[MAX_REGISTER_SIZE];
  char virtual_buffer[MAX_REGISTER_SIZE];

  fputs_filtered (REGISTER_NAME (i), stream);
  print_spaces_filtered (8 - strlen (REGISTER_NAME (i)), stream);

  /* Get 32bits of data.  */
  frame_register_read (deprecated_selected_frame, i, raw_buffer);

  /* Put it in the buffer.  No conversions are ever necessary.  */
  memcpy (virtual_buffer, raw_buffer, DEPRECATED_REGISTER_RAW_SIZE (i));

  if (precision == double_precision && (i % 2) == 0)
    {

      char raw_buf[MAX_REGISTER_SIZE];

      /* Get the data in raw format for the 2nd half.  */
      frame_register_read (deprecated_selected_frame, i + 1, raw_buf);

      /* Copy it into the appropriate part of the virtual buffer.  */
      memcpy (virtual_buffer + DEPRECATED_REGISTER_RAW_SIZE (i), raw_buf,
	      DEPRECATED_REGISTER_RAW_SIZE (i));

      val_print (builtin_type_double, virtual_buffer, 0, 0, stream, 0,
		 1, 0, Val_pretty_default);

    }
  else
    {
      val_print (DEPRECATED_REGISTER_VIRTUAL_TYPE (i), virtual_buffer, 0, 0, stream, 0,
		 1, 0, Val_pretty_default);
    }

}

/* Return one if PC is in the call path of a trampoline, else return zero.

   Note we return one for *any* call trampoline (long-call, arg-reloc), not
   just shared library trampolines (import, export).  */

int
hppa_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  struct minimal_symbol *minsym;
  struct unwind_table_entry *u;
  static CORE_ADDR dyncall = 0;
  static CORE_ADDR sr4export = 0;

#ifdef GDB_TARGET_IS_HPPA_20W
  /* PA64 has a completely different stub/trampoline scheme.  Is it
     better?  Maybe.  It's certainly harder to determine with any
     certainty that we are in a stub because we can not refer to the
     unwinders to help. 

     The heuristic is simple.  Try to lookup the current PC value in th
     minimal symbol table.  If that fails, then assume we are not in a
     stub and return.

     Then see if the PC value falls within the section bounds for the
     section containing the minimal symbol we found in the first
     step.  If it does, then assume we are not in a stub and return.

     Finally peek at the instructions to see if they look like a stub.  */
  {
    struct minimal_symbol *minsym;
    asection *sec;
    CORE_ADDR addr;
    int insn, i;

    minsym = lookup_minimal_symbol_by_pc (pc);
    if (! minsym)
      return 0;

    sec = SYMBOL_BFD_SECTION (minsym);

    if (bfd_get_section_vma (sec->owner, sec) <= pc
	&& pc < (bfd_get_section_vma (sec->owner, sec)
		 + bfd_section_size (sec->owner, sec)))
      return 0;

    /* We might be in a stub.  Peek at the instructions.  Stubs are 3
       instructions long. */
    insn = read_memory_integer (pc, 4);

    /* Find out where we think we are within the stub.  */
    if ((insn & 0xffffc00e) == 0x53610000)
      addr = pc;
    else if ((insn & 0xffffffff) == 0xe820d000)
      addr = pc - 4;
    else if ((insn & 0xffffc00e) == 0x537b0000)
      addr = pc - 8;
    else
      return 0;

    /* Now verify each insn in the range looks like a stub instruction.  */
    insn = read_memory_integer (addr, 4);
    if ((insn & 0xffffc00e) != 0x53610000)
      return 0;
	
    /* Now verify each insn in the range looks like a stub instruction.  */
    insn = read_memory_integer (addr + 4, 4);
    if ((insn & 0xffffffff) != 0xe820d000)
      return 0;
    
    /* Now verify each insn in the range looks like a stub instruction.  */
    insn = read_memory_integer (addr + 8, 4);
    if ((insn & 0xffffc00e) != 0x537b0000)
      return 0;

    /* Looks like a stub.  */
    return 1;
  }
#endif

  /* FIXME XXX - dyncall and sr4export must be initialized whenever we get a
     new exec file */

  /* First see if PC is in one of the two C-library trampolines.  */
  if (!dyncall)
    {
      minsym = lookup_minimal_symbol ("$$dyncall", NULL, NULL);
      if (minsym)
	dyncall = SYMBOL_VALUE_ADDRESS (minsym);
      else
	dyncall = -1;
    }

  if (!sr4export)
    {
      minsym = lookup_minimal_symbol ("_sr4export", NULL, NULL);
      if (minsym)
	sr4export = SYMBOL_VALUE_ADDRESS (minsym);
      else
	sr4export = -1;
    }

  if (pc == dyncall || pc == sr4export)
    return 1;

  minsym = lookup_minimal_symbol_by_pc (pc);
  if (minsym && strcmp (DEPRECATED_SYMBOL_NAME (minsym), ".stub") == 0)
    return 1;

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub, then return now.  */
  if (u->stub_unwind.stub_type == 0)
    return 0;

  /* By definition a long-branch stub is a call stub.  */
  if (u->stub_unwind.stub_type == LONG_BRANCH)
    return 1;

  /* The call and return path execute the same instructions within
     an IMPORT stub!  So an IMPORT stub is both a call and return
     trampoline.  */
  if (u->stub_unwind.stub_type == IMPORT)
    return 1;

  /* Parameter relocation stubs always have a call path and may have a
     return path.  */
  if (u->stub_unwind.stub_type == PARAMETER_RELOCATION
      || u->stub_unwind.stub_type == EXPORT)
    {
      CORE_ADDR addr;

      /* Search forward from the current PC until we hit a branch
         or the end of the stub.  */
      for (addr = pc; addr <= u->region_end; addr += 4)
	{
	  unsigned long insn;

	  insn = read_memory_integer (addr, 4);

	  /* Does it look like a bl?  If so then it's the call path, if
	     we find a bv or be first, then we're on the return path.  */
	  if ((insn & 0xfc00e000) == 0xe8000000)
	    return 1;
	  else if ((insn & 0xfc00e001) == 0xe800c000
		   || (insn & 0xfc000000) == 0xe0000000)
	    return 0;
	}

      /* Should never happen.  */
      warning ("Unable to find branch in parameter relocation stub.\n");
      return 0;
    }

  /* Unknown stub type.  For now, just return zero.  */
  return 0;
}

/* Return one if PC is in the return path of a trampoline, else return zero.

   Note we return one for *any* call trampoline (long-call, arg-reloc), not
   just shared library trampolines (import, export).  */

int
hppa_in_solib_return_trampoline (CORE_ADDR pc, char *name)
{
  struct unwind_table_entry *u;

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub or it's just a long branch stub, then
     return zero.  */
  if (u->stub_unwind.stub_type == 0 || u->stub_unwind.stub_type == LONG_BRANCH)
    return 0;

  /* The call and return path execute the same instructions within
     an IMPORT stub!  So an IMPORT stub is both a call and return
     trampoline.  */
  if (u->stub_unwind.stub_type == IMPORT)
    return 1;

  /* Parameter relocation stubs always have a call path and may have a
     return path.  */
  if (u->stub_unwind.stub_type == PARAMETER_RELOCATION
      || u->stub_unwind.stub_type == EXPORT)
    {
      CORE_ADDR addr;

      /* Search forward from the current PC until we hit a branch
         or the end of the stub.  */
      for (addr = pc; addr <= u->region_end; addr += 4)
	{
	  unsigned long insn;

	  insn = read_memory_integer (addr, 4);

	  /* Does it look like a bl?  If so then it's the call path, if
	     we find a bv or be first, then we're on the return path.  */
	  if ((insn & 0xfc00e000) == 0xe8000000)
	    return 0;
	  else if ((insn & 0xfc00e001) == 0xe800c000
		   || (insn & 0xfc000000) == 0xe0000000)
	    return 1;
	}

      /* Should never happen.  */
      warning ("Unable to find branch in parameter relocation stub.\n");
      return 0;
    }

  /* Unknown stub type.  For now, just return zero.  */
  return 0;

}

/* Figure out if PC is in a trampoline, and if so find out where
   the trampoline will jump to.  If not in a trampoline, return zero.

   Simple code examination probably is not a good idea since the code
   sequences in trampolines can also appear in user code.

   We use unwinds and information from the minimal symbol table to
   determine when we're in a trampoline.  This won't work for ELF
   (yet) since it doesn't create stub unwind entries.  Whether or
   not ELF will create stub unwinds or normal unwinds for linker
   stubs is still being debated.

   This should handle simple calls through dyncall or sr4export,
   long calls, argument relocation stubs, and dyncall/sr4export
   calling an argument relocation stub.  It even handles some stubs
   used in dynamic executables.  */

CORE_ADDR
hppa_skip_trampoline_code (CORE_ADDR pc)
{
  long orig_pc = pc;
  long prev_inst, curr_inst, loc;
  static CORE_ADDR dyncall = 0;
  static CORE_ADDR dyncall_external = 0;
  static CORE_ADDR sr4export = 0;
  struct minimal_symbol *msym;
  struct unwind_table_entry *u;

  /* FIXME XXX - dyncall and sr4export must be initialized whenever we get a
     new exec file */

  if (!dyncall)
    {
      msym = lookup_minimal_symbol ("$$dyncall", NULL, NULL);
      if (msym)
	dyncall = SYMBOL_VALUE_ADDRESS (msym);
      else
	dyncall = -1;
    }

  if (!dyncall_external)
    {
      msym = lookup_minimal_symbol ("$$dyncall_external", NULL, NULL);
      if (msym)
	dyncall_external = SYMBOL_VALUE_ADDRESS (msym);
      else
	dyncall_external = -1;
    }

  if (!sr4export)
    {
      msym = lookup_minimal_symbol ("_sr4export", NULL, NULL);
      if (msym)
	sr4export = SYMBOL_VALUE_ADDRESS (msym);
      else
	sr4export = -1;
    }

  /* Addresses passed to dyncall may *NOT* be the actual address
     of the function.  So we may have to do something special.  */
  if (pc == dyncall)
    {
      pc = (CORE_ADDR) read_register (22);

      /* If bit 30 (counting from the left) is on, then pc is the address of
         the PLT entry for this function, not the address of the function
         itself.  Bit 31 has meaning too, but only for MPE.  */
      if (pc & 0x2)
	pc = (CORE_ADDR) read_memory_integer (pc & ~0x3, TARGET_PTR_BIT / 8);
    }
  if (pc == dyncall_external)
    {
      pc = (CORE_ADDR) read_register (22);
      pc = (CORE_ADDR) read_memory_integer (pc & ~0x3, TARGET_PTR_BIT / 8);
    }
  else if (pc == sr4export)
    pc = (CORE_ADDR) (read_register (22));

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub, then return now.  */
  /* elz: attention here! (FIXME) because of a compiler/linker 
     error, some stubs which should have a non zero stub_unwind.stub_type 
     have unfortunately a value of zero. So this function would return here
     as if we were not in a trampoline. To fix this, we go look at the partial
     symbol information, which reports this guy as a stub.
     (FIXME): Unfortunately, we are not that lucky: it turns out that the 
     partial symbol information is also wrong sometimes. This is because 
     when it is entered (somread.c::som_symtab_read()) it can happen that
     if the type of the symbol (from the som) is Entry, and the symbol is
     in a shared library, then it can also be a trampoline.  This would
     be OK, except that I believe the way they decide if we are ina shared library
     does not work. SOOOO..., even if we have a regular function w/o trampolines
     its minimal symbol can be assigned type mst_solib_trampoline.
     Also, if we find that the symbol is a real stub, then we fix the unwind
     descriptor, and define the stub type to be EXPORT.
     Hopefully this is correct most of the times. */
  if (u->stub_unwind.stub_type == 0)
    {

/* elz: NOTE (FIXME!) once the problem with the unwind information is fixed
   we can delete all the code which appears between the lines */
/*--------------------------------------------------------------------------*/
      msym = lookup_minimal_symbol_by_pc (pc);

      if (msym == NULL || MSYMBOL_TYPE (msym) != mst_solib_trampoline)
	return orig_pc == pc ? 0 : pc & ~0x3;

      else if (msym != NULL && MSYMBOL_TYPE (msym) == mst_solib_trampoline)
	{
	  struct objfile *objfile;
	  struct minimal_symbol *msymbol;
	  int function_found = 0;

	  /* go look if there is another minimal symbol with the same name as 
	     this one, but with type mst_text. This would happen if the msym
	     is an actual trampoline, in which case there would be another
	     symbol with the same name corresponding to the real function */

	  ALL_MSYMBOLS (objfile, msymbol)
	  {
	    if (MSYMBOL_TYPE (msymbol) == mst_text
		&& DEPRECATED_STREQ (DEPRECATED_SYMBOL_NAME (msymbol), DEPRECATED_SYMBOL_NAME (msym)))
	      {
		function_found = 1;
		break;
	      }
	  }

	  if (function_found)
	    /* the type of msym is correct (mst_solib_trampoline), but
	       the unwind info is wrong, so set it to the correct value */
	    u->stub_unwind.stub_type = EXPORT;
	  else
	    /* the stub type info in the unwind is correct (this is not a
	       trampoline), but the msym type information is wrong, it
	       should be mst_text. So we need to fix the msym, and also
	       get out of this function */
	    {
	      MSYMBOL_TYPE (msym) = mst_text;
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

/*--------------------------------------------------------------------------*/
    }

  /* It's a stub.  Search for a branch and figure out where it goes.
     Note we have to handle multi insn branch sequences like ldil;ble.
     Most (all?) other branches can be determined by examining the contents
     of certain registers and the stack.  */

  loc = pc;
  curr_inst = 0;
  prev_inst = 0;
  while (1)
    {
      /* Make sure we haven't walked outside the range of this stub.  */
      if (u != find_unwind_entry (loc))
	{
	  warning ("Unable to find branch in linker stub");
	  return orig_pc == pc ? 0 : pc & ~0x3;
	}

      prev_inst = curr_inst;
      curr_inst = read_memory_integer (loc, 4);

      /* Does it look like a branch external using %r1?  Then it's the
         branch from the stub to the actual function.  */
      if ((curr_inst & 0xffe0e000) == 0xe0202000)
	{
	  /* Yup.  See if the previous instruction loaded
	     a value into %r1.  If so compute and return the jump address.  */
	  if ((prev_inst & 0xffe00000) == 0x20200000)
	    return (extract_21 (prev_inst) + extract_17 (curr_inst)) & ~0x3;
	  else
	    {
	      warning ("Unable to find ldil X,%%r1 before ble Y(%%sr4,%%r1).");
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

      /* Does it look like a be 0(sr0,%r21)? OR 
         Does it look like a be, n 0(sr0,%r21)? OR 
         Does it look like a bve (r21)? (this is on PA2.0)
         Does it look like a bve, n(r21)? (this is also on PA2.0)
         That's the branch from an
         import stub to an export stub.

         It is impossible to determine the target of the branch via
         simple examination of instructions and/or data (consider
         that the address in the plabel may be the address of the
         bind-on-reference routine in the dynamic loader).

         So we have try an alternative approach.

         Get the name of the symbol at our current location; it should
         be a stub symbol with the same name as the symbol in the
         shared library.

         Then lookup a minimal symbol with the same name; we should
         get the minimal symbol for the target routine in the shared
         library as those take precedence of import/export stubs.  */
      if ((curr_inst == 0xe2a00000) ||
	  (curr_inst == 0xe2a00002) ||
	  (curr_inst == 0xeaa0d000) ||
	  (curr_inst == 0xeaa0d002))
	{
	  struct minimal_symbol *stubsym, *libsym;

	  stubsym = lookup_minimal_symbol_by_pc (loc);
	  if (stubsym == NULL)
	    {
	      warning ("Unable to find symbol for 0x%lx", loc);
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }

	  libsym = lookup_minimal_symbol (DEPRECATED_SYMBOL_NAME (stubsym), NULL, NULL);
	  if (libsym == NULL)
	    {
	      warning ("Unable to find library symbol for %s\n",
		       DEPRECATED_SYMBOL_NAME (stubsym));
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }

	  return SYMBOL_VALUE (libsym);
	}

      /* Does it look like bl X,%rp or bl X,%r0?  Another way to do a
         branch from the stub to the actual function.  */
      /*elz */
      else if ((curr_inst & 0xffe0e000) == 0xe8400000
	       || (curr_inst & 0xffe0e000) == 0xe8000000
	       || (curr_inst & 0xffe0e000) == 0xe800A000)
	return (loc + extract_17 (curr_inst) + 8) & ~0x3;

      /* Does it look like bv (rp)?   Note this depends on the
         current stack pointer being the same as the stack
         pointer in the stub itself!  This is a branch on from the
         stub back to the original caller.  */
      /*else if ((curr_inst & 0xffe0e000) == 0xe840c000) */
      else if ((curr_inst & 0xffe0f000) == 0xe840c000)
	{
	  /* Yup.  See if the previous instruction loaded
	     rp from sp - 8.  */
	  if (prev_inst == 0x4bc23ff1)
	    return (read_memory_integer
		    (read_register (SP_REGNUM) - 8, 4)) & ~0x3;
	  else
	    {
	      warning ("Unable to find restore of %%rp before bv (%%rp).");
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

      /* elz: added this case to capture the new instruction
         at the end of the return part of an export stub used by
         the PA2.0: BVE, n (rp) */
      else if ((curr_inst & 0xffe0f000) == 0xe840d000)
	{
	  return (read_memory_integer
		  (read_register (SP_REGNUM) - 24, TARGET_PTR_BIT / 8)) & ~0x3;
	}

      /* What about be,n 0(sr0,%rp)?  It's just another way we return to
         the original caller from the stub.  Used in dynamic executables.  */
      else if (curr_inst == 0xe0400002)
	{
	  /* The value we jump to is sitting in sp - 24.  But that's
	     loaded several instructions before the be instruction.
	     I guess we could check for the previous instruction being
	     mtsp %r1,%sr0 if we want to do sanity checking.  */
	  return (read_memory_integer
		  (read_register (SP_REGNUM) - 24, TARGET_PTR_BIT / 8)) & ~0x3;
	}

      /* Haven't found the branch yet, but we're still in the stub.
         Keep looking.  */
      loc += 4;
    }
}


/* For the given instruction (INST), return any adjustment it makes
   to the stack pointer or zero for no adjustment. 

   This only handles instructions commonly found in prologues.  */

static int
prologue_inst_adjust_sp (unsigned long inst)
{
  /* This must persist across calls.  */
  static int save_high21;

  /* The most common way to perform a stack adjustment ldo X(sp),sp */
  if ((inst & 0xffffc000) == 0x37de0000)
    return extract_14 (inst);

  /* stwm X,D(sp) */
  if ((inst & 0xffe00000) == 0x6fc00000)
    return extract_14 (inst);

  /* std,ma X,D(sp) */
  if ((inst & 0xffe00008) == 0x73c00008)
    return (inst & 0x1 ? -1 << 13 : 0) | (((inst >> 4) & 0x3ff) << 3);

  /* addil high21,%r1; ldo low11,(%r1),%r30)
     save high bits in save_high21 for later use.  */
  if ((inst & 0xffe00000) == 0x28200000)
    {
      save_high21 = extract_21 (inst);
      return 0;
    }

  if ((inst & 0xffff0000) == 0x343e0000)
    return save_high21 + extract_14 (inst);

  /* fstws as used by the HP compilers.  */
  if ((inst & 0xffffffe0) == 0x2fd01220)
    return extract_5_load (inst);

  /* No adjustment.  */
  return 0;
}

/* Return nonzero if INST is a branch of some kind, else return zero.  */

static int
is_branch (unsigned long inst)
{
  switch (inst >> 26)
    {
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2a:
    case 0x2b:
    case 0x2f:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x38:
    case 0x39:
    case 0x3a:
    case 0x3b:
      return 1;

    default:
      return 0;
    }
}

/* Return the register number for a GR which is saved by INST or
   zero it INST does not save a GR.  */

static int
inst_saves_gr (unsigned long inst)
{
  /* Does it look like a stw?  */
  if ((inst >> 26) == 0x1a || (inst >> 26) == 0x1b
      || (inst >> 26) == 0x1f
      || ((inst >> 26) == 0x1f
	  && ((inst >> 6) == 0xa)))
    return extract_5R_store (inst);

  /* Does it look like a std?  */
  if ((inst >> 26) == 0x1c
      || ((inst >> 26) == 0x03
	  && ((inst >> 6) & 0xf) == 0xb))
    return extract_5R_store (inst);

  /* Does it look like a stwm?  GCC & HPC may use this in prologues. */
  if ((inst >> 26) == 0x1b)
    return extract_5R_store (inst);

  /* Does it look like sth or stb?  HPC versions 9.0 and later use these
     too.  */
  if ((inst >> 26) == 0x19 || (inst >> 26) == 0x18
      || ((inst >> 26) == 0x3
	  && (((inst >> 6) & 0xf) == 0x8
	      || (inst >> 6) & 0xf) == 0x9))
    return extract_5R_store (inst);

  return 0;
}

/* Return the register number for a FR which is saved by INST or
   zero it INST does not save a FR.

   Note we only care about full 64bit register stores (that's the only
   kind of stores the prologue will use).

   FIXME: What about argument stores with the HP compiler in ANSI mode? */

static int
inst_saves_fr (unsigned long inst)
{
  /* is this an FSTD ? */
  if ((inst & 0xfc00dfc0) == 0x2c001200)
    return extract_5r_store (inst);
  if ((inst & 0xfc000002) == 0x70000002)
    return extract_5R_store (inst);
  /* is this an FSTW ? */
  if ((inst & 0xfc00df80) == 0x24001200)
    return extract_5r_store (inst);
  if ((inst & 0xfc000002) == 0x7c000000)
    return extract_5R_store (inst);
  return 0;
}

/* Advance PC across any function entry prologue instructions
   to reach some "real" code. 

   Use information in the unwind table to determine what exactly should
   be in the prologue.  */


CORE_ADDR
skip_prologue_hard_way (CORE_ADDR pc)
{
  char buf[4];
  CORE_ADDR orig_pc = pc;
  unsigned long inst, stack_remaining, save_gr, save_fr, save_rp, save_sp;
  unsigned long args_stored, status, i, restart_gr, restart_fr;
  struct unwind_table_entry *u;

  restart_gr = 0;
  restart_fr = 0;

restart:
  u = find_unwind_entry (pc);
  if (!u)
    return pc;

  /* If we are not at the beginning of a function, then return now. */
  if ((pc & ~0x3) != u->region_start)
    return pc;

  /* This is how much of a frame adjustment we need to account for.  */
  stack_remaining = u->Total_frame_size << 3;

  /* Magic register saves we want to know about.  */
  save_rp = u->Save_RP;
  save_sp = u->Save_SP;

  /* An indication that args may be stored into the stack.  Unfortunately
     the HPUX compilers tend to set this in cases where no args were
     stored too!.  */
  args_stored = 1;

  /* Turn the Entry_GR field into a bitmask.  */
  save_gr = 0;
  for (i = 3; i < u->Entry_GR + 3; i++)
    {
      /* Frame pointer gets saved into a special location.  */
      if (u->Save_SP && i == DEPRECATED_FP_REGNUM)
	continue;

      save_gr |= (1 << i);
    }
  save_gr &= ~restart_gr;

  /* Turn the Entry_FR field into a bitmask too.  */
  save_fr = 0;
  for (i = 12; i < u->Entry_FR + 12; i++)
    save_fr |= (1 << i);
  save_fr &= ~restart_fr;

  /* Loop until we find everything of interest or hit a branch.

     For unoptimized GCC code and for any HP CC code this will never ever
     examine any user instructions.

     For optimzied GCC code we're faced with problems.  GCC will schedule
     its prologue and make prologue instructions available for delay slot
     filling.  The end result is user code gets mixed in with the prologue
     and a prologue instruction may be in the delay slot of the first branch
     or call.

     Some unexpected things are expected with debugging optimized code, so
     we allow this routine to walk past user instructions in optimized
     GCC code.  */
  while (save_gr || save_fr || save_rp || save_sp || stack_remaining > 0
	 || args_stored)
    {
      unsigned int reg_num;
      unsigned long old_stack_remaining, old_save_gr, old_save_fr;
      unsigned long old_save_rp, old_save_sp, next_inst;

      /* Save copies of all the triggers so we can compare them later
         (only for HPC).  */
      old_save_gr = save_gr;
      old_save_fr = save_fr;
      old_save_rp = save_rp;
      old_save_sp = save_sp;
      old_stack_remaining = stack_remaining;

      status = target_read_memory (pc, buf, 4);
      inst = extract_unsigned_integer (buf, 4);

      /* Yow! */
      if (status != 0)
	return pc;

      /* Note the interesting effects of this instruction.  */
      stack_remaining -= prologue_inst_adjust_sp (inst);

      /* There are limited ways to store the return pointer into the
	 stack.  */
      if (inst == 0x6bc23fd9 || inst == 0x0fc212c1)
	save_rp = 0;

      /* These are the only ways we save SP into the stack.  At this time
         the HP compilers never bother to save SP into the stack.  */
      if ((inst & 0xffffc000) == 0x6fc10000
	  || (inst & 0xffffc00c) == 0x73c10008)
	save_sp = 0;

      /* Are we loading some register with an offset from the argument
         pointer?  */
      if ((inst & 0xffe00000) == 0x37a00000
	  || (inst & 0xffffffe0) == 0x081d0240)
	{
	  pc += 4;
	  continue;
	}

      /* Account for general and floating-point register saves.  */
      reg_num = inst_saves_gr (inst);
      save_gr &= ~(1 << reg_num);

      /* Ugh.  Also account for argument stores into the stack.
         Unfortunately args_stored only tells us that some arguments
         where stored into the stack.  Not how many or what kind!

         This is a kludge as on the HP compiler sets this bit and it
         never does prologue scheduling.  So once we see one, skip past
         all of them.   We have similar code for the fp arg stores below.

         FIXME.  Can still die if we have a mix of GR and FR argument
         stores!  */
      if (reg_num >= (TARGET_PTR_BIT == 64 ? 19 : 23) && reg_num <= 26)
	{
	  while (reg_num >= (TARGET_PTR_BIT == 64 ? 19 : 23) && reg_num <= 26)
	    {
	      pc += 4;
	      status = target_read_memory (pc, buf, 4);
	      inst = extract_unsigned_integer (buf, 4);
	      if (status != 0)
		return pc;
	      reg_num = inst_saves_gr (inst);
	    }
	  args_stored = 0;
	  continue;
	}

      reg_num = inst_saves_fr (inst);
      save_fr &= ~(1 << reg_num);

      status = target_read_memory (pc + 4, buf, 4);
      next_inst = extract_unsigned_integer (buf, 4);

      /* Yow! */
      if (status != 0)
	return pc;

      /* We've got to be read to handle the ldo before the fp register
         save.  */
      if ((inst & 0xfc000000) == 0x34000000
	  && inst_saves_fr (next_inst) >= 4
	  && inst_saves_fr (next_inst) <= (TARGET_PTR_BIT == 64 ? 11 : 7))
	{
	  /* So we drop into the code below in a reasonable state.  */
	  reg_num = inst_saves_fr (next_inst);
	  pc -= 4;
	}

      /* Ugh.  Also account for argument stores into the stack.
         This is a kludge as on the HP compiler sets this bit and it
         never does prologue scheduling.  So once we see one, skip past
         all of them.  */
      if (reg_num >= 4 && reg_num <= (TARGET_PTR_BIT == 64 ? 11 : 7))
	{
	  while (reg_num >= 4 && reg_num <= (TARGET_PTR_BIT == 64 ? 11 : 7))
	    {
	      pc += 8;
	      status = target_read_memory (pc, buf, 4);
	      inst = extract_unsigned_integer (buf, 4);
	      if (status != 0)
		return pc;
	      if ((inst & 0xfc000000) != 0x34000000)
		break;
	      status = target_read_memory (pc + 4, buf, 4);
	      next_inst = extract_unsigned_integer (buf, 4);
	      if (status != 0)
		return pc;
	      reg_num = inst_saves_fr (next_inst);
	    }
	  args_stored = 0;
	  continue;
	}

      /* Quit if we hit any kind of branch.  This can happen if a prologue
         instruction is in the delay slot of the first call/branch.  */
      if (is_branch (inst))
	break;

      /* What a crock.  The HP compilers set args_stored even if no
         arguments were stored into the stack (boo hiss).  This could
         cause this code to then skip a bunch of user insns (up to the
         first branch).

         To combat this we try to identify when args_stored was bogusly
         set and clear it.   We only do this when args_stored is nonzero,
         all other resources are accounted for, and nothing changed on
         this pass.  */
      if (args_stored
       && !(save_gr || save_fr || save_rp || save_sp || stack_remaining > 0)
	  && old_save_gr == save_gr && old_save_fr == save_fr
	  && old_save_rp == save_rp && old_save_sp == save_sp
	  && old_stack_remaining == stack_remaining)
	break;

      /* Bump the PC.  */
      pc += 4;
    }

  /* We've got a tenative location for the end of the prologue.  However
     because of limitations in the unwind descriptor mechanism we may
     have went too far into user code looking for the save of a register
     that does not exist.  So, if there registers we expected to be saved
     but never were, mask them out and restart.

     This should only happen in optimized code, and should be very rare.  */
  if (save_gr || (save_fr && !(restart_fr || restart_gr)))
    {
      pc = orig_pc;
      restart_gr = save_gr;
      restart_fr = save_fr;
      goto restart;
    }

  return pc;
}


/* Return the address of the PC after the last prologue instruction if
   we can determine it from the debug symbols.  Else return zero.  */

static CORE_ADDR
after_prologue (CORE_ADDR pc)
{
  struct symtab_and_line sal;
  CORE_ADDR func_addr, func_end;
  struct symbol *f;

  /* If we can not find the symbol in the partial symbol table, then
     there is no hope we can determine the function's start address
     with this code.  */
  if (!find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    return 0;

  /* Get the line associated with FUNC_ADDR.  */
  sal = find_pc_line (func_addr, 0);

  /* There are only two cases to consider.  First, the end of the source line
     is within the function bounds.  In that case we return the end of the
     source line.  Second is the end of the source line extends beyond the
     bounds of the current function.  We need to use the slow code to
     examine instructions in that case. 

     Anything else is simply a bug elsewhere.  Fixing it here is absolutely
     the wrong thing to do.  In fact, it should be entirely possible for this
     function to always return zero since the slow instruction scanning code
     is supposed to *always* work.  If it does not, then it is a bug.  */
  if (sal.end < func_end)
    return sal.end;
  else
    return 0;
}

/* To skip prologues, I use this predicate.  Returns either PC itself
   if the code at PC does not look like a function prologue; otherwise
   returns an address that (if we're lucky) follows the prologue.  If
   LENIENT, then we must skip everything which is involved in setting
   up the frame (it's OK to skip more, just so long as we don't skip
   anything which might clobber the registers which are being saved.
   Currently we must not skip more on the alpha, but we might the lenient
   stuff some day.  */

CORE_ADDR
hppa_skip_prologue (CORE_ADDR pc)
{
  unsigned long inst;
  int offset;
  CORE_ADDR post_prologue_pc;
  char buf[4];

  /* See if we can determine the end of the prologue via the symbol table.
     If so, then return either PC, or the PC after the prologue, whichever
     is greater.  */

  post_prologue_pc = after_prologue (pc);

  /* If after_prologue returned a useful address, then use it.  Else
     fall back on the instruction skipping code.

     Some folks have claimed this causes problems because the breakpoint
     may be the first instruction of the prologue.  If that happens, then
     the instruction skipping code has a bug that needs to be fixed.  */
  if (post_prologue_pc != 0)
    return max (pc, post_prologue_pc);
  else
    return (skip_prologue_hard_way (pc));
}

/* Put here the code to store, into the SAVED_REGS, the addresses of
   the saved registers of frame described by FRAME_INFO.  This
   includes special registers such as pc and fp saved in special ways
   in the stack frame.  sp is even more special: the address we return
   for it IS the sp for the next frame.  */

void
hppa_frame_find_saved_regs (struct frame_info *frame_info,
			    CORE_ADDR frame_saved_regs[])
{
  CORE_ADDR pc;
  struct unwind_table_entry *u;
  unsigned long inst, stack_remaining, save_gr, save_fr, save_rp, save_sp;
  int status, i, reg;
  char buf[4];
  int fp_loc = -1;
  int final_iteration;

  /* Zero out everything.  */
  memset (frame_saved_regs, '\0', SIZEOF_FRAME_SAVED_REGS);

  /* Call dummy frames always look the same, so there's no need to
     examine the dummy code to determine locations of saved registers;
     instead, let find_dummy_frame_regs fill in the correct offsets
     for the saved registers.  */
  if ((get_frame_pc (frame_info) >= get_frame_base (frame_info)
       && (get_frame_pc (frame_info)
	   <= (get_frame_base (frame_info)
	       /* A call dummy is sized in words, but it is actually a
		  series of instructions.  Account for that scaling
		  factor.  */
	       + ((DEPRECATED_REGISTER_SIZE / INSTRUCTION_SIZE)
		  * DEPRECATED_CALL_DUMMY_LENGTH)
	       /* Similarly we have to account for 64bit wide register
		  saves.  */
	       + (32 * DEPRECATED_REGISTER_SIZE)
	       /* We always consider FP regs 8 bytes long.  */
	       + (NUM_REGS - FP0_REGNUM) * 8
	       /* Similarly we have to account for 64bit wide register
		  saves.  */
	       + (6 * DEPRECATED_REGISTER_SIZE)))))
    find_dummy_frame_regs (frame_info, frame_saved_regs);

  /* Interrupt handlers are special too.  They lay out the register
     state in the exact same order as the register numbers in GDB.  */
  if (pc_in_interrupt_handler (get_frame_pc (frame_info)))
    {
      for (i = 0; i < NUM_REGS; i++)
	{
	  /* SP is a little special.  */
	  if (i == SP_REGNUM)
	    frame_saved_regs[SP_REGNUM]
	      = read_memory_integer (get_frame_base (frame_info) + SP_REGNUM * 4,
				     TARGET_PTR_BIT / 8);
	  else
	    frame_saved_regs[i] = get_frame_base (frame_info) + i * 4;
	}
      return;
    }

#ifdef FRAME_FIND_SAVED_REGS_IN_SIGTRAMP
  /* Handle signal handler callers.  */
  if ((get_frame_type (frame_info) == SIGTRAMP_FRAME))
    {
      FRAME_FIND_SAVED_REGS_IN_SIGTRAMP (frame_info, frame_saved_regs);
      return;
    }
#endif

  /* Get the starting address of the function referred to by the PC
     saved in frame.  */
  pc = get_frame_func (frame_info);

  /* Yow! */
  u = find_unwind_entry (pc);
  if (!u)
    return;

  /* This is how much of a frame adjustment we need to account for.  */
  stack_remaining = u->Total_frame_size << 3;

  /* Magic register saves we want to know about.  */
  save_rp = u->Save_RP;
  save_sp = u->Save_SP;

  /* Turn the Entry_GR field into a bitmask.  */
  save_gr = 0;
  for (i = 3; i < u->Entry_GR + 3; i++)
    {
      /* Frame pointer gets saved into a special location.  */
      if (u->Save_SP && i == DEPRECATED_FP_REGNUM)
	continue;

      save_gr |= (1 << i);
    }

  /* Turn the Entry_FR field into a bitmask too.  */
  save_fr = 0;
  for (i = 12; i < u->Entry_FR + 12; i++)
    save_fr |= (1 << i);

  /* The frame always represents the value of %sp at entry to the
     current function (and is thus equivalent to the "saved" stack
     pointer.  */
  frame_saved_regs[SP_REGNUM] = get_frame_base (frame_info);

  /* Loop until we find everything of interest or hit a branch.

     For unoptimized GCC code and for any HP CC code this will never ever
     examine any user instructions.

     For optimized GCC code we're faced with problems.  GCC will schedule
     its prologue and make prologue instructions available for delay slot
     filling.  The end result is user code gets mixed in with the prologue
     and a prologue instruction may be in the delay slot of the first branch
     or call.

     Some unexpected things are expected with debugging optimized code, so
     we allow this routine to walk past user instructions in optimized
     GCC code.  */
  final_iteration = 0;
  while ((save_gr || save_fr || save_rp || save_sp || stack_remaining > 0)
	 && pc <= get_frame_pc (frame_info))
    {
      status = target_read_memory (pc, buf, 4);
      inst = extract_unsigned_integer (buf, 4);

      /* Yow! */
      if (status != 0)
	return;

      /* Note the interesting effects of this instruction.  */
      stack_remaining -= prologue_inst_adjust_sp (inst);

      /* There are limited ways to store the return pointer into the
	 stack.  */
      if (inst == 0x6bc23fd9) /* stw rp,-0x14(sr0,sp) */
	{
	  save_rp = 0;
	  frame_saved_regs[RP_REGNUM] = get_frame_base (frame_info) - 20;
	}
      else if (inst == 0x0fc212c1) /* std rp,-0x10(sr0,sp) */
	{
	  save_rp = 0;
	  frame_saved_regs[RP_REGNUM] = get_frame_base (frame_info) - 16;
	}

      /* Note if we saved SP into the stack.  This also happens to indicate
	 the location of the saved frame pointer.  */
      if (   (inst & 0xffffc000) == 0x6fc10000  /* stw,ma r1,N(sr0,sp) */
          || (inst & 0xffffc00c) == 0x73c10008) /* std,ma r1,N(sr0,sp) */
	{
	  frame_saved_regs[DEPRECATED_FP_REGNUM] = get_frame_base (frame_info);
	  save_sp = 0;
	}

      /* Account for general and floating-point register saves.  */
      reg = inst_saves_gr (inst);
      if (reg >= 3 && reg <= 18
	  && (!u->Save_SP || reg != DEPRECATED_FP_REGNUM))
	{
	  save_gr &= ~(1 << reg);

	  /* stwm with a positive displacement is a *post modify*.  */
	  if ((inst >> 26) == 0x1b
	      && extract_14 (inst) >= 0)
	    frame_saved_regs[reg] = get_frame_base (frame_info);
	  /* A std has explicit post_modify forms.  */
	  else if ((inst & 0xfc00000c) == 0x70000008)
	    frame_saved_regs[reg] = get_frame_base (frame_info);
	  else
	    {
	      CORE_ADDR offset;

	      if ((inst >> 26) == 0x1c)
		offset = (inst & 0x1 ? -1 << 13 : 0) | (((inst >> 4) & 0x3ff) << 3);
	      else if ((inst >> 26) == 0x03)
		offset = low_sign_extend (inst & 0x1f, 5);
	      else
		offset = extract_14 (inst);

	      /* Handle code with and without frame pointers.  */
	      if (u->Save_SP)
		frame_saved_regs[reg]
		  = get_frame_base (frame_info) + offset;
	      else
		frame_saved_regs[reg]
		  = (get_frame_base (frame_info) + (u->Total_frame_size << 3)
		     + offset);
	    }
	}


      /* GCC handles callee saved FP regs a little differently.  

         It emits an instruction to put the value of the start of
         the FP store area into %r1.  It then uses fstds,ma with
         a basereg of %r1 for the stores.

         HP CC emits them at the current stack pointer modifying
         the stack pointer as it stores each register.  */

      /* ldo X(%r3),%r1 or ldo X(%r30),%r1.  */
      if ((inst & 0xffffc000) == 0x34610000
	  || (inst & 0xffffc000) == 0x37c10000)
	fp_loc = extract_14 (inst);

      reg = inst_saves_fr (inst);
      if (reg >= 12 && reg <= 21)
	{
	  /* Note +4 braindamage below is necessary because the FP status
	     registers are internally 8 registers rather than the expected
	     4 registers.  */
	  save_fr &= ~(1 << reg);
	  if (fp_loc == -1)
	    {
	      /* 1st HP CC FP register store.  After this instruction
	         we've set enough state that the GCC and HPCC code are
	         both handled in the same manner.  */
	      frame_saved_regs[reg + FP4_REGNUM + 4] = get_frame_base (frame_info);
	      fp_loc = 8;
	    }
	  else
	    {
	      frame_saved_regs[reg + FP0_REGNUM + 4]
		= get_frame_base (frame_info) + fp_loc;
	      fp_loc += 8;
	    }
	}

      /* Quit if we hit any kind of branch the previous iteration. */
      if (final_iteration)
	break;

      /* We want to look precisely one instruction beyond the branch
	 if we have not found everything yet.  */
      if (is_branch (inst))
	final_iteration = 1;

      /* Bump the PC.  */
      pc += 4;
    }
}

/* XXX - deprecated.  This is a compatibility function for targets
   that do not yet implement DEPRECATED_FRAME_INIT_SAVED_REGS.  */
/* Find the addresses in which registers are saved in FRAME.  */

static void
hppa_frame_init_saved_regs (struct frame_info *frame)
{
  if (deprecated_get_frame_saved_regs (frame) == NULL)
    frame_saved_regs_zalloc (frame);
  hppa_frame_find_saved_regs (frame, deprecated_get_frame_saved_regs (frame));
}

struct hppa_frame_cache
{
  CORE_ADDR base;
  struct trad_frame_saved_reg *saved_regs;
};

static struct hppa_frame_cache *
hppa_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct hppa_frame_cache *cache;
  long saved_gr_mask;
  long saved_fr_mask;
  CORE_ADDR this_sp;
  long frame_size;
  struct unwind_table_entry *u;
  int i;

  if ((*this_cache) != NULL)
    return (*this_cache);
  cache = FRAME_OBSTACK_ZALLOC (struct hppa_frame_cache);
  (*this_cache) = cache;
  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  /* Yow! */
  u = find_unwind_entry (frame_func_unwind (next_frame));
  if (!u)
    return (*this_cache);

  /* Turn the Entry_GR field into a bitmask.  */
  saved_gr_mask = 0;
  for (i = 3; i < u->Entry_GR + 3; i++)
    {
      /* Frame pointer gets saved into a special location.  */
      if (u->Save_SP && i == DEPRECATED_FP_REGNUM)
	continue;
	
      saved_gr_mask |= (1 << i);
    }

  /* Turn the Entry_FR field into a bitmask too.  */
  saved_fr_mask = 0;
  for (i = 12; i < u->Entry_FR + 12; i++)
    saved_fr_mask |= (1 << i);

  /* Loop until we find everything of interest or hit a branch.

     For unoptimized GCC code and for any HP CC code this will never ever
     examine any user instructions.

     For optimized GCC code we're faced with problems.  GCC will schedule
     its prologue and make prologue instructions available for delay slot
     filling.  The end result is user code gets mixed in with the prologue
     and a prologue instruction may be in the delay slot of the first branch
     or call.

     Some unexpected things are expected with debugging optimized code, so
     we allow this routine to walk past user instructions in optimized
     GCC code.  */
  {
    int final_iteration = 0;
    CORE_ADDR pc;
    CORE_ADDR end_pc = skip_prologue_using_sal (pc);
    int looking_for_sp = u->Save_SP;
    int looking_for_rp = u->Save_RP;
    int fp_loc = -1;
    if (end_pc == 0)
      end_pc = frame_pc_unwind (next_frame);
    frame_size = 0;
    for (pc = frame_func_unwind (next_frame);
	 ((saved_gr_mask || saved_fr_mask
	   || looking_for_sp || looking_for_rp
	   || frame_size < (u->Total_frame_size << 3))
	  && pc <= end_pc);
	 pc += 4)
      {
	int reg;
	char buf4[4];
	long status = target_read_memory (pc, buf4, sizeof buf4);
	long inst = extract_unsigned_integer (buf4, sizeof buf4);
	
	/* Note the interesting effects of this instruction.  */
	frame_size += prologue_inst_adjust_sp (inst);
	
	/* There are limited ways to store the return pointer into the
	   stack.  */
	if (inst == 0x6bc23fd9) /* stw rp,-0x14(sr0,sp) */
	  {
	    looking_for_rp = 0;
	    cache->saved_regs[RP_REGNUM].addr = -20;
	  }
	else if (inst == 0x0fc212c1) /* std rp,-0x10(sr0,sp) */
	  {
	    looking_for_rp = 0;
	    cache->saved_regs[RP_REGNUM].addr = -16;
	  }
	
	/* Check to see if we saved SP into the stack.  This also
	   happens to indicate the location of the saved frame
	   pointer.  */
	if ((inst & 0xffffc000) == 0x6fc10000  /* stw,ma r1,N(sr0,sp) */
	    || (inst & 0xffffc00c) == 0x73c10008) /* std,ma r1,N(sr0,sp) */
	  {
	    looking_for_sp = 0;
	    cache->saved_regs[DEPRECATED_FP_REGNUM].addr = 0;
	  }
	
	/* Account for general and floating-point register saves.  */
	reg = inst_saves_gr (inst);
	if (reg >= 3 && reg <= 18
	    && (!u->Save_SP || reg != DEPRECATED_FP_REGNUM))
	  {
	    saved_gr_mask &= ~(1 << reg);
	    if ((inst >> 26) == 0x1b && extract_14 (inst) >= 0)
	      /* stwm with a positive displacement is a _post_
		 _modify_.  */
	      cache->saved_regs[reg].addr = 0;
	    else if ((inst & 0xfc00000c) == 0x70000008)
	      /* A std has explicit post_modify forms.  */
	      cache->saved_regs[reg].addr = 0;
	    else
	      {
		CORE_ADDR offset;
		
		if ((inst >> 26) == 0x1c)
		  offset = (inst & 0x1 ? -1 << 13 : 0) | (((inst >> 4) & 0x3ff) << 3);
		else if ((inst >> 26) == 0x03)
		  offset = low_sign_extend (inst & 0x1f, 5);
		else
		  offset = extract_14 (inst);
		
		/* Handle code with and without frame pointers.  */
		if (u->Save_SP)
		  cache->saved_regs[reg].addr = offset;
		else
		  cache->saved_regs[reg].addr = (u->Total_frame_size << 3) + offset;
	      }
	  }

	/* GCC handles callee saved FP regs a little differently.  
	   
	   It emits an instruction to put the value of the start of
	   the FP store area into %r1.  It then uses fstds,ma with a
	   basereg of %r1 for the stores.

	   HP CC emits them at the current stack pointer modifying the
	   stack pointer as it stores each register.  */
	
	/* ldo X(%r3),%r1 or ldo X(%r30),%r1.  */
	if ((inst & 0xffffc000) == 0x34610000
	    || (inst & 0xffffc000) == 0x37c10000)
	  fp_loc = extract_14 (inst);
	
	reg = inst_saves_fr (inst);
	if (reg >= 12 && reg <= 21)
	  {
	    /* Note +4 braindamage below is necessary because the FP
	       status registers are internally 8 registers rather than
	       the expected 4 registers.  */
	    saved_fr_mask &= ~(1 << reg);
	    if (fp_loc == -1)
	      {
		/* 1st HP CC FP register store.  After this
		   instruction we've set enough state that the GCC and
		   HPCC code are both handled in the same manner.  */
		cache->saved_regs[reg + FP4_REGNUM + 4].addr = 0;
		fp_loc = 8;
	      }
	    else
	      {
		cache->saved_regs[reg + FP0_REGNUM + 4].addr = fp_loc;
		fp_loc += 8;
	      }
	  }
	
	/* Quit if we hit any kind of branch the previous iteration. */
	if (final_iteration)
	  break;
	/* We want to look precisely one instruction beyond the branch
	   if we have not found everything yet.  */
	if (is_branch (inst))
	  final_iteration = 1;
      }
  }

  {
    /* The frame base always represents the value of %sp at entry to
       the current function (and is thus equivalent to the "saved"
       stack pointer.  */
    CORE_ADDR this_sp = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
    /* FIXME: cagney/2004-02-22: This assumes that the frame has been
       created.  If it hasn't everything will be out-of-wack.  */
    if (u->Save_SP && trad_frame_addr_p (cache->saved_regs, SP_REGNUM))
      /* Both we're expecting the SP to be saved and the SP has been
	 saved.  The entry SP value is saved at this frame's SP
	 address.  */
      cache->base = read_memory_integer (this_sp, TARGET_PTR_BIT / 8);
    else
      /* The prologue has been slowly allocating stack space.  Adjust
	 the SP back.  */
      cache->base = this_sp - frame_size;
    trad_frame_set_value (cache->saved_regs, SP_REGNUM, cache->base);
  }

  /* The PC is found in the "return register", "Millicode" uses "r31"
     as the return register while normal code uses "rp".  */
  if (u->Millicode)
    cache->saved_regs[PCOQ_HEAD_REGNUM] = cache->saved_regs[31];
  else
    cache->saved_regs[PCOQ_HEAD_REGNUM] = cache->saved_regs[RP_REGNUM];

  {
    /* Convert all the offsets into addresses.  */
    int reg;
    for (reg = 0; reg < NUM_REGS; reg++)
      {
	if (trad_frame_addr_p (cache->saved_regs, reg))
	  cache->saved_regs[reg].addr += cache->base;
      }
  }

  return (*this_cache);
}

static void
hppa_frame_this_id (struct frame_info *next_frame, void **this_cache,
			   struct frame_id *this_id)
{
  struct hppa_frame_cache *info = hppa_frame_cache (next_frame, this_cache);
  (*this_id) = frame_id_build (info->base, frame_func_unwind (next_frame));
}

static void
hppa_frame_prev_register (struct frame_info *next_frame,
				 void **this_cache,
				 int regnum, int *optimizedp,
				 enum lval_type *lvalp, CORE_ADDR *addrp,
				 int *realnump, void *valuep)
{
  struct hppa_frame_cache *info = hppa_frame_cache (next_frame, this_cache);
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  if (regnum == PCOQ_TAIL_REGNUM)
    {
      /* The PCOQ TAIL, or NPC, needs to be computed from the unwound
	 PC register.  */
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = 0;
      if (valuep)
	{
	  int regsize = register_size (gdbarch, PCOQ_HEAD_REGNUM);
	  CORE_ADDR pc;
	  int optimized;
	  enum lval_type lval;
	  CORE_ADDR addr;
	  int realnum;
	  bfd_byte value[MAX_REGISTER_SIZE];
	  trad_frame_prev_register (next_frame, info->saved_regs,
				    PCOQ_HEAD_REGNUM, &optimized, &lval, &addr,
				    &realnum, &value);
	  pc = extract_unsigned_integer (&value, regsize);
	  store_unsigned_integer (valuep, regsize, pc + 4);
	}
    }
  else
    {
      trad_frame_prev_register (next_frame, info->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, valuep);
    }
}

static const struct frame_unwind hppa_frame_unwind =
{
  NORMAL_FRAME,
  hppa_frame_this_id,
  hppa_frame_prev_register
};

static const struct frame_unwind *
hppa_frame_unwind_sniffer (struct frame_info *next_frame)
{
  return &hppa_frame_unwind;
}

static CORE_ADDR
hppa_frame_base_address (struct frame_info *next_frame,
				void **this_cache)
{
  struct hppa_frame_cache *info = hppa_frame_cache (next_frame,
							   this_cache);
  return info->base;
}

static const struct frame_base hppa_frame_base = {
  &hppa_frame_unwind,
  hppa_frame_base_address,
  hppa_frame_base_address,
  hppa_frame_base_address
};

static const struct frame_base *
hppa_frame_base_sniffer (struct frame_info *next_frame)
{
  return &hppa_frame_base;
}

static struct frame_id
hppa_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (frame_unwind_register_unsigned (next_frame,
							 SP_REGNUM),
			 frame_pc_unwind (next_frame));
}

static CORE_ADDR
hppa_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_signed (next_frame, PC_REGNUM) & ~3;
}

/* Exception handling support for the HP-UX ANSI C++ compiler.
   The compiler (aCC) provides a callback for exception events;
   GDB can set a breakpoint on this callback and find out what
   exception event has occurred. */

/* The name of the hook to be set to point to the callback function */
static char HP_ACC_EH_notify_hook[] = "__eh_notify_hook";
/* The name of the function to be used to set the hook value */
static char HP_ACC_EH_set_hook_value[] = "__eh_set_hook_value";
/* The name of the callback function in end.o */
static char HP_ACC_EH_notify_callback[] = "__d_eh_notify_callback";
/* Name of function in end.o on which a break is set (called by above) */
static char HP_ACC_EH_break[] = "__d_eh_break";
/* Name of flag (in end.o) that enables catching throws */
static char HP_ACC_EH_catch_throw[] = "__d_eh_catch_throw";
/* Name of flag (in end.o) that enables catching catching */
static char HP_ACC_EH_catch_catch[] = "__d_eh_catch_catch";
/* The enum used by aCC */
typedef enum
  {
    __EH_NOTIFY_THROW,
    __EH_NOTIFY_CATCH
  }
__eh_notification;

/* Is exception-handling support available with this executable? */
static int hp_cxx_exception_support = 0;
/* Has the initialize function been run? */
int hp_cxx_exception_support_initialized = 0;
/* Similar to above, but imported from breakpoint.c -- non-target-specific */
extern int exception_support_initialized;
/* Address of __eh_notify_hook */
static CORE_ADDR eh_notify_hook_addr = 0;
/* Address of __d_eh_notify_callback */
static CORE_ADDR eh_notify_callback_addr = 0;
/* Address of __d_eh_break */
static CORE_ADDR eh_break_addr = 0;
/* Address of __d_eh_catch_catch */
static CORE_ADDR eh_catch_catch_addr = 0;
/* Address of __d_eh_catch_throw */
static CORE_ADDR eh_catch_throw_addr = 0;
/* Sal for __d_eh_break */
static struct symtab_and_line *break_callback_sal = 0;

/* Code in end.c expects __d_pid to be set in the inferior,
   otherwise __d_eh_notify_callback doesn't bother to call
   __d_eh_break!  So we poke the pid into this symbol
   ourselves.
   0 => success
   1 => failure  */
int
setup_d_pid_in_inferior (void)
{
  CORE_ADDR anaddr;
  struct minimal_symbol *msymbol;
  char buf[4];			/* FIXME 32x64? */

  /* Slam the pid of the process into __d_pid; failing is only a warning!  */
  msymbol = lookup_minimal_symbol ("__d_pid", NULL, symfile_objfile);
  if (msymbol == NULL)
    {
      warning ("Unable to find __d_pid symbol in object file.");
      warning ("Suggest linking executable with -g (links in /opt/langtools/lib/end.o).");
      return 1;
    }

  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);
  store_unsigned_integer (buf, 4, PIDGET (inferior_ptid)); /* FIXME 32x64? */
  if (target_write_memory (anaddr, buf, 4))	/* FIXME 32x64? */
    {
      warning ("Unable to write __d_pid");
      warning ("Suggest linking executable with -g (links in /opt/langtools/lib/end.o).");
      return 1;
    }
  return 0;
}

/* Initialize exception catchpoint support by looking for the
   necessary hooks/callbacks in end.o, etc., and set the hook value to
   point to the required debug function

   Return 0 => failure
   1 => success          */

static int
initialize_hp_cxx_exception_support (void)
{
  struct symtabs_and_lines sals;
  struct cleanup *old_chain;
  struct cleanup *canonical_strings_chain = NULL;
  int i;
  char *addr_start;
  char *addr_end = NULL;
  char **canonical = (char **) NULL;
  int thread = -1;
  struct symbol *sym = NULL;
  struct minimal_symbol *msym = NULL;
  struct objfile *objfile;
  asection *shlib_info;

  /* Detect and disallow recursion.  On HP-UX with aCC, infinite
     recursion is a possibility because finding the hook for exception
     callbacks involves making a call in the inferior, which means
     re-inserting breakpoints which can re-invoke this code */

  static int recurse = 0;
  if (recurse > 0)
    {
      hp_cxx_exception_support_initialized = 0;
      exception_support_initialized = 0;
      return 0;
    }

  hp_cxx_exception_support = 0;

  /* First check if we have seen any HP compiled objects; if not,
     it is very unlikely that HP's idiosyncratic callback mechanism
     for exception handling debug support will be available!
     This will percolate back up to breakpoint.c, where our callers
     will decide to try the g++ exception-handling support instead. */
  if (!hp_som_som_object_present)
    return 0;

  /* We have a SOM executable with SOM debug info; find the hooks */

  /* First look for the notify hook provided by aCC runtime libs */
  /* If we find this symbol, we conclude that the executable must
     have HP aCC exception support built in.  If this symbol is not
     found, even though we're a HP SOM-SOM file, we may have been
     built with some other compiler (not aCC).  This results percolates
     back up to our callers in breakpoint.c which can decide to
     try the g++ style of exception support instead.
     If this symbol is found but the other symbols we require are
     not found, there is something weird going on, and g++ support
     should *not* be tried as an alternative.

     ASSUMPTION: Only HP aCC code will have __eh_notify_hook defined.  
     ASSUMPTION: HP aCC and g++ modules cannot be linked together. */

  /* libCsup has this hook; it'll usually be non-debuggable */
  msym = lookup_minimal_symbol (HP_ACC_EH_notify_hook, NULL, NULL);
  if (msym)
    {
      eh_notify_hook_addr = SYMBOL_VALUE_ADDRESS (msym);
      hp_cxx_exception_support = 1;
    }
  else
    {
      warning ("Unable to find exception callback hook (%s).", HP_ACC_EH_notify_hook);
      warning ("Executable may not have been compiled debuggable with HP aCC.");
      warning ("GDB will be unable to intercept exception events.");
      eh_notify_hook_addr = 0;
      hp_cxx_exception_support = 0;
      return 0;
    }

  /* Next look for the notify callback routine in end.o */
  /* This is always available in the SOM symbol dictionary if end.o is linked in */
  msym = lookup_minimal_symbol (HP_ACC_EH_notify_callback, NULL, NULL);
  if (msym)
    {
      eh_notify_callback_addr = SYMBOL_VALUE_ADDRESS (msym);
      hp_cxx_exception_support = 1;
    }
  else
    {
      warning ("Unable to find exception callback routine (%s).", HP_ACC_EH_notify_callback);
      warning ("Suggest linking executable with -g (links in /opt/langtools/lib/end.o).");
      warning ("GDB will be unable to intercept exception events.");
      eh_notify_callback_addr = 0;
      return 0;
    }

#ifndef GDB_TARGET_IS_HPPA_20W
  /* Check whether the executable is dynamically linked or archive bound */
  /* With an archive-bound executable we can use the raw addresses we find
     for the callback function, etc. without modification. For an executable
     with shared libraries, we have to do more work to find the plabel, which
     can be the target of a call through $$dyncall from the aCC runtime support
     library (libCsup) which is linked shared by default by aCC. */
  /* This test below was copied from somsolib.c/somread.c.  It may not be a very
     reliable one to test that an executable is linked shared. pai/1997-07-18 */
  shlib_info = bfd_get_section_by_name (symfile_objfile->obfd, "$SHLIB_INFO$");
  if (shlib_info && (bfd_section_size (symfile_objfile->obfd, shlib_info) != 0))
    {
      /* The minsym we have has the local code address, but that's not the
         plabel that can be used by an inter-load-module call. */
      /* Find solib handle for main image (which has end.o), and use that
         and the min sym as arguments to __d_shl_get() (which does the equivalent
         of shl_findsym()) to find the plabel. */

      args_for_find_stub args;
      static char message[] = "Error while finding exception callback hook:\n";

      args.solib_handle = som_solib_get_solib_by_pc (eh_notify_callback_addr);
      args.msym = msym;
      args.return_val = 0;

      recurse++;
      catch_errors (cover_find_stub_with_shl_get, &args, message,
		    RETURN_MASK_ALL);
      eh_notify_callback_addr = args.return_val;
      recurse--;

      exception_catchpoints_are_fragile = 1;

      if (!eh_notify_callback_addr)
	{
	  /* We can get here either if there is no plabel in the export list
	     for the main image, or if something strange happened (?) */
	  warning ("Couldn't find a plabel (indirect function label) for the exception callback.");
	  warning ("GDB will not be able to intercept exception events.");
	  return 0;
	}
    }
  else
    exception_catchpoints_are_fragile = 0;
#endif

  /* Now, look for the breakpointable routine in end.o */
  /* This should also be available in the SOM symbol dict. if end.o linked in */
  msym = lookup_minimal_symbol (HP_ACC_EH_break, NULL, NULL);
  if (msym)
    {
      eh_break_addr = SYMBOL_VALUE_ADDRESS (msym);
      hp_cxx_exception_support = 1;
    }
  else
    {
      warning ("Unable to find exception callback routine to set breakpoint (%s).", HP_ACC_EH_break);
      warning ("Suggest linking executable with -g (link in /opt/langtools/lib/end.o).");
      warning ("GDB will be unable to intercept exception events.");
      eh_break_addr = 0;
      return 0;
    }

  /* Next look for the catch enable flag provided in end.o */
  sym = lookup_symbol (HP_ACC_EH_catch_catch, (struct block *) NULL,
		       VAR_DOMAIN, 0, (struct symtab **) NULL);
  if (sym)			/* sometimes present in debug info */
    {
      eh_catch_catch_addr = SYMBOL_VALUE_ADDRESS (sym);
      hp_cxx_exception_support = 1;
    }
  else
    /* otherwise look in SOM symbol dict. */
    {
      msym = lookup_minimal_symbol (HP_ACC_EH_catch_catch, NULL, NULL);
      if (msym)
	{
	  eh_catch_catch_addr = SYMBOL_VALUE_ADDRESS (msym);
	  hp_cxx_exception_support = 1;
	}
      else
	{
	  warning ("Unable to enable interception of exception catches.");
	  warning ("Executable may not have been compiled debuggable with HP aCC.");
	  warning ("Suggest linking executable with -g (link in /opt/langtools/lib/end.o).");
	  return 0;
	}
    }

  /* Next look for the catch enable flag provided end.o */
  sym = lookup_symbol (HP_ACC_EH_catch_catch, (struct block *) NULL,
		       VAR_DOMAIN, 0, (struct symtab **) NULL);
  if (sym)			/* sometimes present in debug info */
    {
      eh_catch_throw_addr = SYMBOL_VALUE_ADDRESS (sym);
      hp_cxx_exception_support = 1;
    }
  else
    /* otherwise look in SOM symbol dict. */
    {
      msym = lookup_minimal_symbol (HP_ACC_EH_catch_throw, NULL, NULL);
      if (msym)
	{
	  eh_catch_throw_addr = SYMBOL_VALUE_ADDRESS (msym);
	  hp_cxx_exception_support = 1;
	}
      else
	{
	  warning ("Unable to enable interception of exception throws.");
	  warning ("Executable may not have been compiled debuggable with HP aCC.");
	  warning ("Suggest linking executable with -g (link in /opt/langtools/lib/end.o).");
	  return 0;
	}
    }

  /* Set the flags */
  hp_cxx_exception_support = 2;	/* everything worked so far */
  hp_cxx_exception_support_initialized = 1;
  exception_support_initialized = 1;

  return 1;
}

/* Target operation for enabling or disabling interception of
   exception events.
   KIND is either EX_EVENT_THROW or EX_EVENT_CATCH
   ENABLE is either 0 (disable) or 1 (enable).
   Return value is NULL if no support found;
   -1 if something went wrong,
   or a pointer to a symtab/line struct if the breakpointable
   address was found. */

struct symtab_and_line *
child_enable_exception_callback (enum exception_event_kind kind, int enable)
{
  char buf[4];

  if (!exception_support_initialized || !hp_cxx_exception_support_initialized)
    if (!initialize_hp_cxx_exception_support ())
      return NULL;

  switch (hp_cxx_exception_support)
    {
    case 0:
      /* Assuming no HP support at all */
      return NULL;
    case 1:
      /* HP support should be present, but something went wrong */
      return (struct symtab_and_line *) -1;	/* yuck! */
      /* there may be other cases in the future */
    }

  /* Set the EH hook to point to the callback routine */
  store_unsigned_integer (buf, 4, enable ? eh_notify_callback_addr : 0);	/* FIXME 32x64 problem */
  /* pai: (temp) FIXME should there be a pack operation first? */
  if (target_write_memory (eh_notify_hook_addr, buf, 4))	/* FIXME 32x64 problem */
    {
      warning ("Could not write to target memory for exception event callback.");
      warning ("Interception of exception events may not work.");
      return (struct symtab_and_line *) -1;
    }
  if (enable)
    {
      /* Ensure that __d_pid is set up correctly -- end.c code checks this. :-( */
      if (PIDGET (inferior_ptid) > 0)
	{
	  if (setup_d_pid_in_inferior ())
	    return (struct symtab_and_line *) -1;
	}
      else
	{
	  warning ("Internal error: Invalid inferior pid?  Cannot intercept exception events.");
	  return (struct symtab_and_line *) -1;
	}
    }

  switch (kind)
    {
    case EX_EVENT_THROW:
      store_unsigned_integer (buf, 4, enable ? 1 : 0);
      if (target_write_memory (eh_catch_throw_addr, buf, 4))	/* FIXME 32x64? */
	{
	  warning ("Couldn't enable exception throw interception.");
	  return (struct symtab_and_line *) -1;
	}
      break;
    case EX_EVENT_CATCH:
      store_unsigned_integer (buf, 4, enable ? 1 : 0);
      if (target_write_memory (eh_catch_catch_addr, buf, 4))	/* FIXME 32x64? */
	{
	  warning ("Couldn't enable exception catch interception.");
	  return (struct symtab_and_line *) -1;
	}
      break;
    default:
      error ("Request to enable unknown or unsupported exception event.");
    }

  /* Copy break address into new sal struct, malloc'ing if needed. */
  if (!break_callback_sal)
    {
      break_callback_sal = (struct symtab_and_line *) xmalloc (sizeof (struct symtab_and_line));
    }
  init_sal (break_callback_sal);
  break_callback_sal->symtab = NULL;
  break_callback_sal->pc = eh_break_addr;
  break_callback_sal->line = 0;
  break_callback_sal->end = eh_break_addr;

  return break_callback_sal;
}

/* Record some information about the current exception event */
static struct exception_event_record current_ex_event;
/* Convenience struct */
static struct symtab_and_line null_symtab_and_line =
{NULL, 0, 0, 0};

/* Report current exception event.  Returns a pointer to a record
   that describes the kind of the event, where it was thrown from,
   and where it will be caught.  More information may be reported
   in the future */
struct exception_event_record *
child_get_current_exception_event (void)
{
  CORE_ADDR event_kind;
  CORE_ADDR throw_addr;
  CORE_ADDR catch_addr;
  struct frame_info *fi, *curr_frame;
  int level = 1;

  curr_frame = get_current_frame ();
  if (!curr_frame)
    return (struct exception_event_record *) NULL;

  /* Go up one frame to __d_eh_notify_callback, because at the
     point when this code is executed, there's garbage in the
     arguments of __d_eh_break. */
  fi = find_relative_frame (curr_frame, &level);
  if (level != 0)
    return (struct exception_event_record *) NULL;

  select_frame (fi);

  /* Read in the arguments */
  /* __d_eh_notify_callback() is called with 3 arguments:
     1. event kind catch or throw
     2. the target address if known
     3. a flag -- not sure what this is. pai/1997-07-17 */
  event_kind = read_register (ARG0_REGNUM);
  catch_addr = read_register (ARG1_REGNUM);

  /* Now go down to a user frame */
  /* For a throw, __d_eh_break is called by
     __d_eh_notify_callback which is called by
     __notify_throw which is called
     from user code.
     For a catch, __d_eh_break is called by
     __d_eh_notify_callback which is called by
     <stackwalking stuff> which is called by
     __throw__<stuff> or __rethrow_<stuff> which is called
     from user code. */
  /* FIXME: Don't use such magic numbers; search for the frames */
  level = (event_kind == EX_EVENT_THROW) ? 3 : 4;
  fi = find_relative_frame (curr_frame, &level);
  if (level != 0)
    return (struct exception_event_record *) NULL;

  select_frame (fi);
  throw_addr = get_frame_pc (fi);

  /* Go back to original (top) frame */
  select_frame (curr_frame);

  current_ex_event.kind = (enum exception_event_kind) event_kind;
  current_ex_event.throw_sal = find_pc_line (throw_addr, 1);
  current_ex_event.catch_sal = find_pc_line (catch_addr, 1);

  return &current_ex_event;
}

/* Instead of this nasty cast, add a method pvoid() that prints out a
   host VOID data type (remember %p isn't portable).  */

static CORE_ADDR
hppa_pointer_to_address_hack (void *ptr)
{
  gdb_assert (sizeof (ptr) == TYPE_LENGTH (builtin_type_void_data_ptr));
  return POINTER_TO_ADDRESS (builtin_type_void_data_ptr, &ptr);
}

static void
unwind_command (char *exp, int from_tty)
{
  CORE_ADDR address;
  struct unwind_table_entry *u;

  /* If we have an expression, evaluate it and use it as the address.  */

  if (exp != 0 && *exp != 0)
    address = parse_and_eval_address (exp);
  else
    return;

  u = find_unwind_entry (address);

  if (!u)
    {
      printf_unfiltered ("Can't find unwind table entry for %s\n", exp);
      return;
    }

  printf_unfiltered ("unwind_table_entry (0x%s):\n",
		     paddr_nz (hppa_pointer_to_address_hack (u)));

  printf_unfiltered ("\tregion_start = ");
  print_address (u->region_start, gdb_stdout);

  printf_unfiltered ("\n\tregion_end = ");
  print_address (u->region_end, gdb_stdout);

#define pif(FLD) if (u->FLD) printf_unfiltered (" "#FLD);

  printf_unfiltered ("\n\tflags =");
  pif (Cannot_unwind);
  pif (Millicode);
  pif (Millicode_save_sr0);
  pif (Entry_SR);
  pif (Args_stored);
  pif (Variable_Frame);
  pif (Separate_Package_Body);
  pif (Frame_Extension_Millicode);
  pif (Stack_Overflow_Check);
  pif (Two_Instruction_SP_Increment);
  pif (Ada_Region);
  pif (Save_SP);
  pif (Save_RP);
  pif (Save_MRP_in_frame);
  pif (extn_ptr_defined);
  pif (Cleanup_defined);
  pif (MPE_XL_interrupt_marker);
  pif (HP_UX_interrupt_marker);
  pif (Large_frame);

  putchar_unfiltered ('\n');

#define pin(FLD) printf_unfiltered ("\t"#FLD" = 0x%x\n", u->FLD);

  pin (Region_description);
  pin (Entry_FR);
  pin (Entry_GR);
  pin (Total_frame_size);
}

void
hppa_skip_permanent_breakpoint (void)
{
  /* To step over a breakpoint instruction on the PA takes some
     fiddling with the instruction address queue.

     When we stop at a breakpoint, the IA queue front (the instruction
     we're executing now) points at the breakpoint instruction, and
     the IA queue back (the next instruction to execute) points to
     whatever instruction we would execute after the breakpoint, if it
     were an ordinary instruction.  This is the case even if the
     breakpoint is in the delay slot of a branch instruction.

     Clearly, to step past the breakpoint, we need to set the queue
     front to the back.  But what do we put in the back?  What
     instruction comes after that one?  Because of the branch delay
     slot, the next insn is always at the back + 4.  */
  write_register (PCOQ_HEAD_REGNUM, read_register (PCOQ_TAIL_REGNUM));
  write_register (PCSQ_HEAD_REGNUM, read_register (PCSQ_TAIL_REGNUM));

  write_register (PCOQ_TAIL_REGNUM, read_register (PCOQ_TAIL_REGNUM) + 4);
  /* We can leave the tail's space the same, since there's no jump.  */
}

/* Same as hppa32_store_return_value(), but for the PA64 ABI.  */

void
hppa64_store_return_value (struct type *type, char *valbuf)
{
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    deprecated_write_register_bytes
      (DEPRECATED_REGISTER_BYTE (FP4_REGNUM)
        + DEPRECATED_REGISTER_SIZE - TYPE_LENGTH (type),
       valbuf, TYPE_LENGTH (type));
  else if (is_integral_type(type))
    deprecated_write_register_bytes
      (DEPRECATED_REGISTER_BYTE (28)
        + DEPRECATED_REGISTER_SIZE - TYPE_LENGTH (type),
       valbuf, TYPE_LENGTH (type));
  else if (TYPE_LENGTH (type) <= 8)
    deprecated_write_register_bytes
      (DEPRECATED_REGISTER_BYTE (28),valbuf, TYPE_LENGTH (type));
  else if (TYPE_LENGTH (type) <= 16)
    {
      deprecated_write_register_bytes (DEPRECATED_REGISTER_BYTE (28),valbuf, 8);
      deprecated_write_register_bytes
        (DEPRECATED_REGISTER_BYTE (29), valbuf + 8, TYPE_LENGTH (type) - 8);
    }
}

/* Same as hppa32_extract_return_value but for the PA64 ABI case.  */

void
hppa64_extract_return_value (struct type *type, char *regbuf, char *valbuf)
{
  /* RM: Floats are returned in FR4R, doubles in FR4.
         Integral values are in r28, padded on the left.
         Aggregates less that 65 bits are in r28, right padded.
         Aggregates upto 128 bits are in r28 and r29, right padded.  */ 
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    memcpy (valbuf,
            regbuf + DEPRECATED_REGISTER_BYTE (FP4_REGNUM)
             + DEPRECATED_REGISTER_SIZE - TYPE_LENGTH (type),
            TYPE_LENGTH (type));
  else if (is_integral_type(type))
    memcpy (valbuf,
            regbuf + DEPRECATED_REGISTER_BYTE (28)
             + DEPRECATED_REGISTER_SIZE - TYPE_LENGTH (type),
            TYPE_LENGTH (type));
  else if (TYPE_LENGTH (type) <= 8)
    memcpy (valbuf, regbuf + DEPRECATED_REGISTER_BYTE (28),
	    TYPE_LENGTH (type));
  else if (TYPE_LENGTH (type) <= 16)
    {
      memcpy (valbuf, regbuf + DEPRECATED_REGISTER_BYTE (28), 8);
      memcpy (valbuf + 8, regbuf + DEPRECATED_REGISTER_BYTE (29),
	      TYPE_LENGTH (type) - 8);
    }
}

int
hppa_reg_struct_has_addr (int gcc_p, struct type *type)
{
  /* On the PA, any pass-by-value structure > 8 bytes is actually passed
     via a pointer regardless of its type or the compiler used.  */
  return (TYPE_LENGTH (type) > 8);
}

int
hppa_inner_than (CORE_ADDR lhs, CORE_ADDR rhs)
{
  /* Stack grows upward */
  return (lhs > rhs);
}

CORE_ADDR
hppa64_stack_align (CORE_ADDR sp)
{
  /* The PA64 ABI mandates a 16 byte stack alignment.  */
  return ((sp % 16) ? (sp + 15) & -16 : sp);
}

int
hppa_pc_requires_run_before_use (CORE_ADDR pc)
{
  /* Sometimes we may pluck out a minimal symbol that has a negative address.
  
     An example of this occurs when an a.out is linked against a foo.sl.
     The foo.sl defines a global bar(), and the a.out declares a signature
     for bar().  However, the a.out doesn't directly call bar(), but passes
     its address in another call.
  
     If you have this scenario and attempt to "break bar" before running,
     gdb will find a minimal symbol for bar() in the a.out.  But that
     symbol's address will be negative.  What this appears to denote is
     an index backwards from the base of the procedure linkage table (PLT)
     into the data linkage table (DLT), the end of which is contiguous
     with the start of the PLT.  This is clearly not a valid address for
     us to set a breakpoint on.
  
     Note that one must be careful in how one checks for a negative address.
     0xc0000000 is a legitimate address of something in a shared text
     segment, for example.  Since I don't know what the possible range
     is of these "really, truly negative" addresses that come from the
     minimal symbols, I'm resorting to the gross hack of checking the
     top byte of the address for all 1's.  Sigh.  */

  return (!target_has_stack && (pc & 0xFF000000));
}

int
hppa_instruction_nullified (void)
{
  /* brobecker 2002/11/07: Couldn't we use a ULONGEST here? It would
     avoid the type cast.  I'm leaving it as is for now as I'm doing
     semi-mechanical multiarching-related changes.  */
  const int ipsw = (int) read_register (IPSW_REGNUM);
  const int flags = (int) read_register (FLAGS_REGNUM);

  return ((ipsw & 0x00200000) && !(flags & 0x2));
}

int
hppa_register_raw_size (int reg_nr)
{
  /* All registers have the same size.  */
  return DEPRECATED_REGISTER_SIZE;
}

/* Index within the register vector of the first byte of the space i
   used for register REG_NR.  */

int
hppa_register_byte (int reg_nr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  return reg_nr * tdep->bytes_per_address;
}

/* Return the GDB type object for the "standard" data type of data
   in register N.  */

struct type *
hppa32_register_virtual_type (int reg_nr)
{
   if (reg_nr < FP4_REGNUM)
     return builtin_type_int;
   else
     return builtin_type_float;
}

/* Return the GDB type object for the "standard" data type of data
   in register N.  hppa64 version.  */

struct type *
hppa64_register_virtual_type (int reg_nr)
{
   if (reg_nr < FP4_REGNUM)
     return builtin_type_unsigned_long_long;
   else
     return builtin_type_double;
}

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function.  */

void
hppa_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  write_register (28, addr);
}
/* Return True if REGNUM is not a register available to the user
   through ptrace().  */

int
hppa_cannot_store_register (int regnum)
{
  return (regnum == 0
          || regnum == PCSQ_HEAD_REGNUM
          || (regnum >= PCSQ_TAIL_REGNUM && regnum < IPSW_REGNUM)
          || (regnum > IPSW_REGNUM && regnum < FP4_REGNUM));

}

CORE_ADDR
hppa_smash_text_address (CORE_ADDR addr)
{
  /* The low two bits of the PC on the PA contain the privilege level.
     Some genius implementing a (non-GCC) compiler apparently decided
     this means that "addresses" in a text section therefore include a
     privilege level, and thus symbol tables should contain these bits.
     This seems like a bonehead thing to do--anyway, it seems to work
     for our purposes to just ignore those bits.  */

  return (addr &= ~0x3);
}

/* Get the ith function argument for the current function.  */
CORE_ADDR
hppa_fetch_pointer_argument (struct frame_info *frame, int argi, 
			     struct type *type)
{
  CORE_ADDR addr;
  get_frame_register (frame, R0_REGNUM + 26 - argi, &addr);
  return addr;
}

/* Here is a table of C type sizes on hppa with various compiles
   and options.  I measured this on PA 9000/800 with HP-UX 11.11
   and these compilers:

     /usr/ccs/bin/cc    HP92453-01 A.11.01.21
     /opt/ansic/bin/cc  HP92453-01 B.11.11.28706.GP
     /opt/aCC/bin/aCC   B3910B A.03.45
     gcc                gcc 3.3.2 native hppa2.0w-hp-hpux11.11

     cc            : 1 2 4 4 8 : 4 8 -- : 4 4
     ansic +DA1.1  : 1 2 4 4 8 : 4 8 16 : 4 4
     ansic +DA2.0  : 1 2 4 4 8 : 4 8 16 : 4 4
     ansic +DA2.0W : 1 2 4 8 8 : 4 8 16 : 8 8
     acc   +DA1.1  : 1 2 4 4 8 : 4 8 16 : 4 4
     acc   +DA2.0  : 1 2 4 4 8 : 4 8 16 : 4 4
     acc   +DA2.0W : 1 2 4 8 8 : 4 8 16 : 8 8
     gcc           : 1 2 4 4 8 : 4 8 16 : 4 4

   Each line is:

     compiler and options
     char, short, int, long, long long
     float, double, long double
     char *, void (*)()

   So all these compilers use either ILP32 or LP64 model.
   TODO: gcc has more options so it needs more investigation.

   For floating point types, see:

     http://docs.hp.com/hpux/pdf/B3906-90006.pdf
     HP-UX floating-point guide, hpux 11.00

   -- chastain 2003-12-18  */

static struct gdbarch *
hppa_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep;
  struct gdbarch *gdbarch;
  
  /* Try to determine the ABI of the object we are loading.  */
  if (info.abfd != NULL && info.osabi == GDB_OSABI_UNKNOWN)
    {
      /* If it's a SOM file, assume it's HP/UX SOM.  */
      if (bfd_get_flavour (info.abfd) == bfd_target_som_flavour)
	info.osabi = GDB_OSABI_HPUX_SOM;
    }

  /* find a candidate among the list of pre-declared architectures.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return (arches->gdbarch);

  /* If none found, then allocate and initialize one.  */
  tdep = XMALLOC (struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);

  /* Determine from the bfd_arch_info structure if we are dealing with
     a 32 or 64 bits architecture.  If the bfd_arch_info is not available,
     then default to a 32bit machine.  */
  if (info.bfd_arch_info != NULL)
    tdep->bytes_per_address =
      info.bfd_arch_info->bits_per_address / info.bfd_arch_info->bits_per_byte;
  else
    tdep->bytes_per_address = 4;

  /* Some parts of the gdbarch vector depend on whether we are running
     on a 32 bits or 64 bits target.  */
  switch (tdep->bytes_per_address)
    {
      case 4:
        set_gdbarch_num_regs (gdbarch, hppa32_num_regs);
        set_gdbarch_register_name (gdbarch, hppa32_register_name);
        set_gdbarch_deprecated_register_virtual_type
          (gdbarch, hppa32_register_virtual_type);
        break;
      case 8:
        set_gdbarch_num_regs (gdbarch, hppa64_num_regs);
        set_gdbarch_register_name (gdbarch, hppa64_register_name);
        set_gdbarch_deprecated_register_virtual_type
          (gdbarch, hppa64_register_virtual_type);
        break;
      default:
        internal_error (__FILE__, __LINE__, "Unsupported address size: %d",
                        tdep->bytes_per_address);
    }

  /* The following gdbarch vector elements depend on other parts of this
     vector which have been set above, depending on the ABI.  */
  set_gdbarch_deprecated_register_bytes
    (gdbarch, gdbarch_num_regs (gdbarch) * tdep->bytes_per_address);
  set_gdbarch_long_bit (gdbarch, tdep->bytes_per_address * TARGET_CHAR_BIT);
  set_gdbarch_ptr_bit (gdbarch, tdep->bytes_per_address * TARGET_CHAR_BIT);

  /* The following gdbarch vector elements are the same in both ILP32
     and LP64, but might show differences some day.  */
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 128);
  set_gdbarch_long_double_format (gdbarch, &floatformat_ia64_quad_big);

  /* The following gdbarch vector elements do not depend on the address
     size, or in any other gdbarch element previously set.  */
  set_gdbarch_skip_prologue (gdbarch, hppa_skip_prologue);
  set_gdbarch_skip_trampoline_code (gdbarch, hppa_skip_trampoline_code);
  set_gdbarch_in_solib_call_trampoline (gdbarch, hppa_in_solib_call_trampoline);
  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          hppa_in_solib_return_trampoline);
  set_gdbarch_inner_than (gdbarch, hppa_inner_than);
  set_gdbarch_deprecated_register_size (gdbarch, tdep->bytes_per_address);
  set_gdbarch_deprecated_fp_regnum (gdbarch, 3);
  set_gdbarch_sp_regnum (gdbarch, 30);
  set_gdbarch_fp0_regnum (gdbarch, 64);
  set_gdbarch_pc_regnum (gdbarch, PCOQ_HEAD_REGNUM);
  set_gdbarch_deprecated_register_raw_size (gdbarch, hppa_register_raw_size);
  set_gdbarch_deprecated_register_byte (gdbarch, hppa_register_byte);
  set_gdbarch_deprecated_register_virtual_size (gdbarch, hppa_register_raw_size);
  set_gdbarch_deprecated_max_register_raw_size (gdbarch, tdep->bytes_per_address);
  set_gdbarch_deprecated_max_register_virtual_size (gdbarch, 8);
  set_gdbarch_cannot_store_register (gdbarch, hppa_cannot_store_register);
  set_gdbarch_addr_bits_remove (gdbarch, hppa_smash_text_address);
  set_gdbarch_smash_text_address (gdbarch, hppa_smash_text_address);
  set_gdbarch_believe_pcc_promotion (gdbarch, 1);
  set_gdbarch_read_pc (gdbarch, hppa_target_read_pc);
  set_gdbarch_write_pc (gdbarch, hppa_target_write_pc);
  set_gdbarch_deprecated_target_read_fp (gdbarch, hppa_target_read_fp);

  /* Helper for function argument information.  */
  set_gdbarch_fetch_pointer_argument (gdbarch, hppa_fetch_pointer_argument);

  set_gdbarch_print_insn (gdbarch, print_insn_hppa);

  /* When a hardware watchpoint triggers, we'll move the inferior past
     it by removing all eventpoints; stepping past the instruction
     that caused the trigger; reinserting eventpoints; and checking
     whether any watched location changed.  */
  set_gdbarch_have_nonsteppable_watchpoint (gdbarch, 1);

  /* Inferior function call methods.  */
  switch (tdep->bytes_per_address)
    {
    case 4:
      set_gdbarch_push_dummy_call (gdbarch, hppa32_push_dummy_call);
      set_gdbarch_frame_align (gdbarch, hppa32_frame_align);
      break;
    case 8:
      if (0)
	{
	  set_gdbarch_push_dummy_call (gdbarch, hppa64_push_dummy_call);
	  set_gdbarch_frame_align (gdbarch, hppa64_frame_align);
	  break;
	}
      else
	{
	  set_gdbarch_deprecated_call_dummy_breakpoint_offset (gdbarch, hppa64_call_dummy_breakpoint_offset);
	  set_gdbarch_deprecated_call_dummy_length (gdbarch, hppa64_call_dummy_length);
	  set_gdbarch_deprecated_stack_align (gdbarch, hppa64_stack_align);
	  break;
	  set_gdbarch_deprecated_push_dummy_frame (gdbarch, hppa_push_dummy_frame);
	  /* set_gdbarch_deprecated_fix_call_dummy (gdbarch, hppa_fix_call_dummy); */
	  set_gdbarch_deprecated_push_arguments (gdbarch, hppa_push_arguments);
	  set_gdbarch_deprecated_use_generic_dummy_frames (gdbarch, 0);
	  set_gdbarch_deprecated_pc_in_call_dummy (gdbarch, deprecated_pc_in_call_dummy_on_stack);
	  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
	}
      break;
    }
      
  /* Struct return methods.  */
  switch (tdep->bytes_per_address)
    {
    case 4:
      set_gdbarch_return_value (gdbarch, hppa32_return_value);
      break;
    case 8:
      if (0)
	set_gdbarch_return_value (gdbarch, hppa64_return_value);
      else
	{
	  set_gdbarch_deprecated_extract_return_value (gdbarch, hppa64_extract_return_value);
	  set_gdbarch_use_struct_convention (gdbarch, hppa64_use_struct_convention);
	  set_gdbarch_deprecated_store_return_value (gdbarch, hppa64_store_return_value);
	  set_gdbarch_deprecated_store_struct_return (gdbarch, hppa_store_struct_return);
	}
      break;
    default:
      internal_error (__FILE__, __LINE__, "bad switch");
    }
      
  /* Frame unwind methods.  */
  switch (tdep->bytes_per_address)
    {
    case 4:
      set_gdbarch_unwind_dummy_id (gdbarch, hppa_unwind_dummy_id);
      set_gdbarch_unwind_pc (gdbarch, hppa_unwind_pc);
      frame_unwind_append_sniffer (gdbarch, hppa_frame_unwind_sniffer);
      frame_base_append_sniffer (gdbarch, hppa_frame_base_sniffer);
      break;
    case 8:
      set_gdbarch_deprecated_saved_pc_after_call (gdbarch, hppa_saved_pc_after_call);
      set_gdbarch_deprecated_init_frame_pc (gdbarch, deprecated_init_frame_pc_default);
      set_gdbarch_deprecated_frame_init_saved_regs (gdbarch, hppa_frame_init_saved_regs);
      set_gdbarch_deprecated_init_extra_frame_info (gdbarch, hppa_init_extra_frame_info);
      set_gdbarch_deprecated_frame_chain (gdbarch, hppa_frame_chain);
      set_gdbarch_deprecated_frame_chain_valid (gdbarch, hppa_frame_chain_valid);
      set_gdbarch_deprecated_frameless_function_invocation (gdbarch, hppa_frameless_function_invocation);
      set_gdbarch_deprecated_frame_saved_pc (gdbarch, hppa_frame_saved_pc);
      set_gdbarch_deprecated_pop_frame (gdbarch, hppa_pop_frame);
      break;
    default:
      internal_error (__FILE__, __LINE__, "bad switch");
    }

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  return gdbarch;
}

static void
hppa_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{
   /* Nothing to print for the moment.  */
}

void
_initialize_hppa_tdep (void)
{
  struct cmd_list_element *c;
  void break_at_finish_command (char *arg, int from_tty);
  void tbreak_at_finish_command (char *arg, int from_tty);
  void break_at_finish_at_depth_command (char *arg, int from_tty);

  gdbarch_register (bfd_arch_hppa, hppa_gdbarch_init, hppa_dump_tdep);

  add_cmd ("unwind", class_maintenance, unwind_command,
	   "Print unwind table entry at given address.",
	   &maintenanceprintlist);

  deprecate_cmd (add_com ("xbreak", class_breakpoint, 
			  break_at_finish_command,
			  concat ("Set breakpoint at procedure exit. \n\
Argument may be function name, or \"*\" and an address.\n\
If function is specified, break at end of code for that function.\n\
If an address is specified, break at the end of the function that contains \n\
that exact address.\n",
		   "With no arg, uses current execution address of selected stack frame.\n\
This is useful for breaking on return to a stack frame.\n\
\n\
Multiple breakpoints at one place are permitted, and useful if conditional.\n\
\n\
Do \"help breakpoints\" for info on other commands dealing with breakpoints.", NULL)), NULL);
  deprecate_cmd (add_com_alias ("xb", "xbreak", class_breakpoint, 1), NULL);
  deprecate_cmd (add_com_alias ("xbr", "xbreak", class_breakpoint, 1), NULL);
  deprecate_cmd (add_com_alias ("xbre", "xbreak", class_breakpoint, 1), NULL);
  deprecate_cmd (add_com_alias ("xbrea", "xbreak", class_breakpoint, 1), NULL);

  deprecate_cmd (c = add_com ("txbreak", class_breakpoint, 
			      tbreak_at_finish_command,
"Set temporary breakpoint at procedure exit.  Either there should\n\
be no argument or the argument must be a depth.\n"), NULL);
  set_cmd_completer (c, location_completer);
  
  if (xdb_commands)
    deprecate_cmd (add_com ("bx", class_breakpoint, 
			    break_at_finish_at_depth_command,
"Set breakpoint at procedure exit.  Either there should\n\
be no argument or the argument must be a depth.\n"), NULL);
}

