/* Parameters for execution on any Hewlett-Packard PA-RISC machine.

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

#include "regcache.h"

/* Wonder if this is correct?  Should be using push_dummy_call().  */
#define DEPRECATED_DUMMY_WRITE_SP(SP) deprecated_write_sp (SP)

#define GDB_MULTI_ARCH 1

/* Hack, get around problem with including "arch-utils.h".  */
struct frame_info;

/* Forward declarations of some types we use in prototypes */

struct frame_info;
struct frame_saved_regs;
struct value;
struct type;
struct inferior_status;

/* Sequence of bytes for breakpoint instruction.  */

const unsigned char *hppa_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr);
#define BREAKPOINT_FROM_PC(PCPTR,LENPTR) hppa_breakpoint_from_pc ((PCPTR), (LENPTR))
#define BREAKPOINT32 0x10004

extern int hppa_pc_requires_run_before_use (CORE_ADDR pc);
#define PC_REQUIRES_RUN_BEFORE_USE(pc) hppa_pc_requires_run_before_use (pc)

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define R0_REGNUM 0		/* Doesn't actually exist, used as base for
				   other r registers.  */
#define FLAGS_REGNUM 0		/* Various status flags */
#define RP_REGNUM 2		/* return pointer */
#define SAR_REGNUM 32		/* Shift Amount Register */
#define IPSW_REGNUM 41		/* Interrupt Processor Status Word */
#define PCOQ_HEAD_REGNUM 33	/* instruction offset queue head */
#define PCSQ_HEAD_REGNUM 34	/* instruction space queue head */
#define PCOQ_TAIL_REGNUM 35	/* instruction offset queue tail */
#define PCSQ_TAIL_REGNUM 36	/* instruction space queue tail */
#define EIEM_REGNUM 37		/* External Interrupt Enable Mask */
#define IIR_REGNUM 38		/* Interrupt Instruction Register */
#define IOR_REGNUM 40		/* Interrupt Offset Register */
#define SR4_REGNUM 43		/* space register 4 */
#define RCR_REGNUM 51		/* Recover Counter (also known as cr0) */
#define CCR_REGNUM 54		/* Coprocessor Configuration Register */
#define TR0_REGNUM 57		/* Temporary Registers (cr24 -> cr31) */
#define CR27_REGNUM 60		/* Base register for thread-local storage, cr27 */
#define FP4_REGNUM 72

#define ARG0_REGNUM 26		/* The first argument of a callee. */
#define ARG1_REGNUM 25		/* The second argument of a callee. */
#define ARG2_REGNUM 24		/* The third argument of a callee. */
#define ARG3_REGNUM 23		/* The fourth argument of a callee. */

/* When fetching register values from an inferior or a core file,
   clean them up using this macro.  BUF is a char pointer to
   the raw value of the register in the registers[] array.  */

#define	DEPRECATED_CLEAN_UP_REGISTER_VALUE(regno, buf) \
  do {	\
    if ((regno) == PCOQ_HEAD_REGNUM || (regno) == PCOQ_TAIL_REGNUM) \
      (buf)[sizeof(CORE_ADDR) -1] &= ~0x3; \
  } while (0)

/* Define DEPRECATED_DO_REGISTERS_INFO() to do machine-specific
   formatting of register dumps. */

#define DEPRECATED_DO_REGISTERS_INFO(_regnum, fp) pa_do_registers_info (_regnum, fp)
extern void pa_do_registers_info (int, int);

/* PA specific macro to see if the current instruction is nullified. */
#ifndef INSTRUCTION_NULLIFIED
extern int hppa_instruction_nullified (void);
#define INSTRUCTION_NULLIFIED hppa_instruction_nullified ()
#endif

#define INSTRUCTION_SIZE 4

/* This sequence of words is the instructions

   ; Call stack frame has already been built by gdb. Since we could be calling 
   ; a varargs function, and we do not have the benefit of a stub to put things in
   ; the right place, we load the first 4 word of arguments into both the general
   ; and fp registers.
   call_dummy
   ldw -36(sp), arg0
   ldw -40(sp), arg1
   ldw -44(sp), arg2
   ldw -48(sp), arg3
   ldo -36(sp), r1
   fldws 0(0, r1), fr4
   fldds -4(0, r1), fr5
   fldws -8(0, r1), fr6
   fldds -12(0, r1), fr7
   ldil 0, r22                  ; FUNC_LDIL_OFFSET must point here
   ldo 0(r22), r22                      ; FUNC_LDO_OFFSET must point here
   ldsid (0,r22), r4
   ldil 0, r1                   ; SR4EXPORT_LDIL_OFFSET must point here
   ldo 0(r1), r1                        ; SR4EXPORT_LDO_OFFSET must point here
   ldsid (0,r1), r20
   combt,=,n r4, r20, text_space        ; If target is in data space, do a
   ble 0(sr5, r22)                      ; "normal" procedure call
   copy r31, r2
   break 4, 8 
   mtsp r21, sr0
   ble,n 0(sr0, r22)
   text_space                           ; Otherwise, go through _sr4export,
   ble (sr4, r1)                        ; which will return back here.
   stw r31,-24(r30)
   break 4, 8
   mtsp r21, sr0
   ble,n 0(sr0, r22)
   nop                          ; To avoid kernel bugs 
   nop                          ; and keep the dummy 8 byte aligned

   The dummy decides if the target is in text space or data space. If
   it's in data space, there's no problem because the target can
   return back to the dummy. However, if the target is in text space,
   the dummy calls the secret, undocumented routine _sr4export, which
   calls a function in text space and can return to any space. Instead
   of including fake instructions to represent saved registers, we
   know that the frame is associated with the call dummy and treat it
   specially.

   The trailing NOPs are needed to avoid a bug in HPUX, BSD and OSF1 
   kernels.   If the memory at the location pointed to by the PC is
   0xffffffff then a ptrace step call will fail (even if the instruction
   is nullified).

   The code to pop a dummy frame single steps three instructions
   starting with the last mtsp.  This includes the nullified "instruction"
   following the ble (which is uninitialized junk).  If the 
   "instruction" following the last BLE is 0xffffffff, then the ptrace
   will fail and the dummy frame is not correctly popped.

   By placing a NOP in the delay slot of the BLE instruction we can be 
   sure that we never try to execute a 0xffffffff instruction and
   avoid the kernel bug.  The second NOP is needed to keep the call
   dummy 8 byte aligned.  */

#define CALL_DUMMY {0x4BDA3FB9, 0x4BD93FB1, 0x4BD83FA9, 0x4BD73FA1,\
                    0x37C13FB9, 0x24201004, 0x2C391005, 0x24311006,\
                    0x2C291007, 0x22C00000, 0x36D60000, 0x02C010A4,\
                    0x20200000, 0x34210000, 0x002010b4, 0x82842022,\
                    0xe6c06000, 0x081f0242, 0x00010004, 0x00151820,\
                    0xe6c00002, 0xe4202000, 0x6bdf3fd1, 0x00010004,\
                    0x00151820, 0xe6c00002, 0x08000240, 0x08000240}

#define REG_PARM_STACK_SPACE 16

/* If we've reached a trap instruction within the call dummy, then
   we'll consider that to mean that we've reached the call dummy's
   end after its successful completion. */
#define DEPRECATED_CALL_DUMMY_HAS_COMPLETED(pc, sp, frame_address) \
  (DEPRECATED_PC_IN_CALL_DUMMY((pc), (sp), (frame_address)) && \
   (read_memory_integer((pc), 4) == BREAKPOINT32))

/* Insert the specified number of args and function address into a
   call sequence of the above form stored at DUMMYNAME.

   On the hppa we need to call the stack dummy through $$dyncall.
   Therefore our version of DEPRECATED_FIX_CALL_DUMMY takes an extra
   argument, real_pc, which is the location where gdb should start up
   the inferior to do the function call.  */

/* FIXME: brobecker 2002-12-26.  This macro is going to cause us some
   problems before we can go to multiarch partial as it has been
   diverted on HPUX to return the value of the PC!  */
/* NOTE: cagney/2003-05-03: This has been replaced by push_dummy_code.
   Hopefully that has all the parameters HP/UX needs.  */
#define DEPRECATED_FIX_CALL_DUMMY hppa_fix_call_dummy
extern CORE_ADDR hppa_fix_call_dummy (char *, CORE_ADDR, CORE_ADDR, int,
		                      struct value **, struct type *, int);

#define	GDB_TARGET_IS_HPPA

/*
 * Unwind table and descriptor.
 */

struct unwind_table_entry
  {
    CORE_ADDR region_start;
    CORE_ADDR region_end;

    unsigned int Cannot_unwind:1;	/* 0 */
    unsigned int Millicode:1;	/* 1 */
    unsigned int Millicode_save_sr0:1;	/* 2 */
    unsigned int Region_description:2;	/* 3..4 */
    unsigned int reserved1:1;	/* 5 */
    unsigned int Entry_SR:1;	/* 6 */
    unsigned int Entry_FR:4;	/* number saved *//* 7..10 */
    unsigned int Entry_GR:5;	/* number saved *//* 11..15 */
    unsigned int Args_stored:1;	/* 16 */
    unsigned int Variable_Frame:1;	/* 17 */
    unsigned int Separate_Package_Body:1;	/* 18 */
    unsigned int Frame_Extension_Millicode:1;	/* 19 */
    unsigned int Stack_Overflow_Check:1;	/* 20 */
    unsigned int Two_Instruction_SP_Increment:1;	/* 21 */
    unsigned int Ada_Region:1;	/* 22 */
    unsigned int cxx_info:1;	/* 23 */
    unsigned int cxx_try_catch:1;	/* 24 */
    unsigned int sched_entry_seq:1;	/* 25 */
    unsigned int reserved2:1;	/* 26 */
    unsigned int Save_SP:1;	/* 27 */
    unsigned int Save_RP:1;	/* 28 */
    unsigned int Save_MRP_in_frame:1;	/* 29 */
    unsigned int extn_ptr_defined:1;	/* 30 */
    unsigned int Cleanup_defined:1;	/* 31 */

    unsigned int MPE_XL_interrupt_marker:1;	/* 0 */
    unsigned int HP_UX_interrupt_marker:1;	/* 1 */
    unsigned int Large_frame:1;	/* 2 */
    unsigned int Pseudo_SP_Set:1;	/* 3 */
    unsigned int reserved4:1;	/* 4 */
    unsigned int Total_frame_size:27;	/* 5..31 */

    /* This is *NOT* part of an actual unwind_descriptor in an object
       file.  It is *ONLY* part of the "internalized" descriptors that
       we create from those in a file.
     */
    struct
      {
	unsigned int stub_type:4;	/* 0..3 */
	unsigned int padding:28;	/* 4..31 */
      }
    stub_unwind;
  };

/* HP linkers also generate unwinds for various linker-generated stubs.
   GDB reads in the stubs from the $UNWIND_END$ subspace, then 
   "converts" them into normal unwind entries using some of the reserved
   fields to store the stub type.  */

/* The gaps represent linker stubs used in MPE and space for future
   expansion.  */
enum unwind_stub_types
  {
    LONG_BRANCH = 1,
    PARAMETER_RELOCATION = 2,
    EXPORT = 10,
    IMPORT = 11,
    IMPORT_SHLIB = 12,
  };

/* We use the objfile->obj_private pointer for two things:

 * 1.  An unwind table;
 *
 * 2.  A pointer to any associated shared library object.
 *
 * #defines are used to help refer to these objects.
 */

/* Info about the unwind table associated with an object file.

 * This is hung off of the "objfile->obj_private" pointer, and
 * is allocated in the objfile's psymbol obstack.  This allows
 * us to have unique unwind info for each executable and shared
 * library that we are debugging.
 */
struct obj_unwind_info
  {
    struct unwind_table_entry *table;	/* Pointer to unwind info */
    struct unwind_table_entry *cache;	/* Pointer to last entry we found */
    int last;			/* Index of last entry */
  };

typedef struct obj_private_struct
  {
    struct obj_unwind_info *unwind_info;	/* a pointer */
    struct so_list *so_info;	/* a pointer  */
    CORE_ADDR dp;
  }
obj_private_data_t;

/* For a number of horrible reasons we may have to adjust the location
   of variables on the stack.  Ugh.  */
#define HPREAD_ADJUST_STACK_ADDRESS(ADDR) hpread_adjust_stack_address(ADDR)
extern int hpread_adjust_stack_address (CORE_ADDR);

/* Here's how to step off a permanent breakpoint.  */
#define SKIP_PERMANENT_BREAKPOINT (hppa_skip_permanent_breakpoint)
extern void hppa_skip_permanent_breakpoint (void);

/* On HP-UX, certain system routines (millicode) have names beginning
   with $ or $$, e.g. $$dyncall, which handles inter-space procedure
   calls on PA-RISC.  Tell the expression parser to check for those
   when parsing tokens that begin with "$".  */
#define SYMBOLS_CAN_START_WITH_DOLLAR (1)
