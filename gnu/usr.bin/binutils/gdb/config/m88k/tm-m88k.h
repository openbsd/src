/* Target machine description for generic Motorola 88000, for GDB.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1993
   Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* g++ support is not yet included.  */

/* Define the bit, byte, and word ordering of the machine.  */
#define TARGET_BYTE_ORDER BIG_ENDIAN

/* We cache information about saved registers in the frame structure,
   to save us from having to re-scan function prologues every time
   a register in a non-current frame is accessed.  */

#define EXTRA_FRAME_INFO 	\
	struct frame_saved_regs *fsr;	\
	CORE_ADDR locals_pointer;	\
	CORE_ADDR args_pointer;

/* Zero the frame_saved_regs pointer when the frame is initialized,
   so that FRAME_FIND_SAVED_REGS () will know to allocate and
   initialize a frame_saved_regs struct the first time it is called.
   Set the arg_pointer to -1, which is not valid; 0 and other values
   indicate real, cached values.  */

#define INIT_EXTRA_FRAME_INFO(fromleaf, fi) \
	init_extra_frame_info (fromleaf, fi)
extern void init_extra_frame_info ();

#define IEEE_FLOAT

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

#define SKIP_PROLOGUE(frompc)   \
	{ (frompc) = skip_prologue (frompc); }
extern CORE_ADDR skip_prologue ();

/* The m88k kernel aligns all instructions on 4-byte boundaries.  The
   kernel also uses the least significant two bits for its own hocus
   pocus.  When gdb receives an address from the kernel, it needs to
   preserve those right-most two bits, but gdb also needs to be careful
   to realize that those two bits are not really a part of the address
   of an instruction.  Shrug.  */

#define ADDR_BITS_REMOVE(addr) ((addr) & ~3)

/* Immediately after a function call, return the saved pc.
   Can't always go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

#define SAVED_PC_AFTER_CALL(frame) \
  (ADDR_BITS_REMOVE (read_register (SRP_REGNUM)))

/* Stack grows downward.  */

#define INNER_THAN <

/* Sequence of bytes for breakpoint instruction.  */

/* instruction 0xF000D1FF is 'tb0 0,r0,511'
   If Bit bit 0 of r0 is clear (always true),
   initiate exception processing (trap).
 */
#define BREAKPOINT {0xF0, 0x00, 0xD1, 0xFF}

/* Amount PC must be decremented by after a breakpoint.
   This is often the number of bytes in BREAKPOINT
   but not always.  */

#define DECR_PC_AFTER_BREAK 0

/* Nonzero if instruction at PC is a return instruction.  */
/* 'jmp r1' or 'jmp.n r1' is used to return from a subroutine. */

#define ABOUT_TO_RETURN(pc) (read_memory_integer (pc, 2) == 0xF800)

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places; REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */

#define REGISTER_SIZE 4

/* Number of machine registers */

#define GP_REGS (38)
#define FP_REGS (32)
#define NUM_REGS (GP_REGS + FP_REGS)

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

#define REGISTER_NAMES {\
			  "r0",\
			  "r1",\
			  "r2",\
			  "r3",\
			  "r4",\
			  "r5",\
			  "r6",\
			  "r7",\
			  "r8",\
			  "r9",\
			  "r10",\
			  "r11",\
			  "r12",\
			  "r13",\
			  "r14",\
			  "r15",\
			  "r16",\
			  "r17",\
			  "r18",\
			  "r19",\
			  "r20",\
			  "r21",\
			  "r22",\
			  "r23",\
			  "r24",\
			  "r25",\
			  "r26",\
			  "r27",\
			  "r28",\
			  "r29",\
			  "r30",\
			  "r31",\
			  "psr",\
			  "fpsr",\
			  "fpcr",\
			  "sxip",\
			  "snip",\
			  "sfip",\
			  "x0",\
			  "x1",\
			  "x2",\
			  "x3",\
			  "x4",\
			  "x5",\
			  "x6",\
			  "x7",\
			  "x8",\
			  "x9",\
			  "x10",\
			  "x11",\
			  "x12",\
			  "x13",\
			  "x14",\
			  "x15",\
			  "x16",\
			  "x17",\
			  "x18",\
			  "x19",\
			  "x20",\
			  "x21",\
			  "x22",\
			  "x23",\
			  "x24",\
			  "x25",\
			  "x26",\
			  "x27",\
			  "x28",\
			  "x29",\
			  "x30",\
			  "x31",\
			  "vbr",\
			  "dmt0",\
			  "dmd0",\
			  "dma0",\
			  "dmt1",\
			  "dmd1",\
			  "dma1",\
			  "dmt2",\
			  "dmd2",\
			  "dma2",\
			  "sr0",\
			  "sr1",\
			  "sr2",\
			  "sr3",\
			  "fpecr",\
			  "fphs1",\
			  "fpls1",\
			  "fphs2",\
			  "fpls2",\
			  "fppt",\
			  "fprh",\
			  "fprl",\
			  "fpit",\
			  "fpsr",\
			  "fpcr",\
		      }


/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define R0_REGNUM 0		/* Contains the constant zero */
#define SRP_REGNUM 1		/* Contains subroutine return pointer */
#define RV_REGNUM 2		/* Contains simple return values */
#define SRA_REGNUM 12		/* Contains address of struct return values */
#define SP_REGNUM 31		/* Contains address of top of stack */

/* Instruction pointer notes...

   On the m88100:

   * cr04 = sxip.  On exception, contains the excepting pc (probably).
   On rte, is ignored.

   * cr05 = snip.  On exception, contains the NPC (next pc).  On rte,
   pc is loaded from here.

   * cr06 = sfip.  On exception, contains the NNPC (next next pc).  On
   rte, the NPC is loaded from here.

   * lower two bits of each are flag bits.  Bit 1 is V means address
   is valid.  If address is not valid, bit 0 is ignored.  Otherwise,
   bit 0 is E and asks for an exception to be taken if this
   instruction is executed.

   On the m88110:

   * cr04 = exip.  On exception, contains the address of the excepting
   pc (always).  On rte, pc is loaded from here.  Bit 0, aka the D
   bit, is a flag saying that the offending instruction was in a
   branch delay slot.  If set, then cr05 contains the NPC.

   * cr05 = enip.  On exception, if the instruction pointed to by cr04
   was in a delay slot as indicated by the bit 0 of cr04, aka the D
   bit, the cr05 contains the NPC.  Otherwise ignored.

   * cr06 is invalid  */

/* Note that the Harris Unix kernels emulate the m88100's behavior on
   the m88110.  */

#define SXIP_REGNUM 35		/* On m88100, Contains Shadow Execute
				   Instruction Pointer.  */
#define SNIP_REGNUM 36		/* On m88100, Contains Shadow Next
				   Instruction Pointer.  */
#define SFIP_REGNUM 37		/* On m88100, Contains Shadow Fetched
				   Intruction pointer.  */

#define EXIP_REGNUM 35		/* On m88110, Contains Exception
				   Executing Instruction Pointer.  */
#define ENIP_REGNUM 36		/* On m88110, Contains the Exception
				   Next Instruction Pointer.  */

#define PC_REGNUM SXIP_REGNUM	/* Program Counter */
#define NPC_REGNUM SNIP_REGNUM	/* Next Program Counter */
#define NNPC_REGNUM SFIP_REGNUM /* Next Next Program Counter */

#define PSR_REGNUM 32           /* Processor Status Register */
#define FPSR_REGNUM 33		/* Floating Point Status Register */
#define FPCR_REGNUM 34		/* Floating Point Control Register */
#define XFP_REGNUM 38		/* First Extended Float Register */
#define X0_REGNUM XFP_REGNUM	/* Which also contains the constant zero */

/* This is rather a confusing lie.  Our m88k port using a stack pointer value
   for the frame address.  Hence, the frame address and the frame pointer are
   only indirectly related.  The value of this macro is the register number
   fetched by the machine "independent" portions of gdb when they want to know
   about a frame address.  Thus, we lie here and claim that FP_REGNUM is
   SP_REGNUM.  */
#define FP_REGNUM SP_REGNUM	/* Reg fetched to locate frame when pgm stops */
#define ACTUAL_FP_REGNUM 30

/* PSR status bit definitions.  */

#define PSR_MODE		0x80000000
#define PSR_BYTE_ORDER		0x40000000
#define PSR_SERIAL_MODE		0x20000000
#define PSR_CARRY		0x10000000
#define PSR_SFU_DISABLE		0x000003f0
#define PSR_SFU1_DISABLE	0x00000008
#define PSR_MXM			0x00000004
#define PSR_IND			0x00000002
#define PSR_SFRZ		0x00000001



/* The following two comments come from the days prior to the m88110
   port.  The m88110 handles the instruction pointers differently.  I
   do not know what any m88110 kernels do as the m88110 port I'm
   working with is for an embedded system.  rich@cygnus.com
   13-sept-93.  */

/* BCS requires that the SXIP_REGNUM (or PC_REGNUM) contain the
   address of the next instr to be executed when a breakpoint occurs.
   Because the kernel gets the next instr (SNIP_REGNUM), the instr in
   SNIP needs to be put back into SFIP, and the instr in SXIP should
   be shifted to SNIP */

/* Are you sitting down?  It turns out that the 88K BCS (binary
   compatibility standard) folks originally felt that the debugger
   should be responsible for backing up the IPs, not the kernel (as is
   usually done).  Well, they have reversed their decision, and in
   future releases our kernel will be handling the backing up of the
   IPs.  So, eventually, we won't need to do the SHIFT_INST_REGS
   stuff.  But, for now, since there are 88K systems out there that do
   need the debugger to do the IP shifting, and since there will be
   systems where the kernel does the shifting, the code is a little
   more complex than perhaps it needs to be (we still go inside
   SHIFT_INST_REGS, and if the shifting hasn't occurred then gdb goes
   ahead and shifts).  */

extern int target_is_m88110;
#define SHIFT_INST_REGS() \
if (!target_is_m88110) \
{ \
    CORE_ADDR pc = read_register (PC_REGNUM); \
    CORE_ADDR npc = read_register (NPC_REGNUM); \
    if (pc != npc) \
    { \
	write_register (NNPC_REGNUM, npc); \
	write_register (NPC_REGNUM, pc); \
    } \
}

    /* Storing the following registers is a no-op. */
#define CANNOT_STORE_REGISTER(regno)	(((regno) == R0_REGNUM) \
					 || ((regno) == X0_REGNUM))

  /* Number of bytes of storage in the actual machine representation
     for register N.  On the m88k,  the general purpose registers are 4
     bytes and the 88110 extended registers are 10 bytes. */

#define REGISTER_RAW_SIZE(N) ((N) < XFP_REGNUM ? 4 : 10)

  /* Total amount of space needed to store our copies of the machine's
     register state, the array `registers'.  */

#define REGISTER_BYTES ((GP_REGS * REGISTER_RAW_SIZE(0)) \
			+ (FP_REGS * REGISTER_RAW_SIZE(XFP_REGNUM)))

  /* Index within `registers' of the first byte of the space for
     register N.  */

#define REGISTER_BYTE(N) (((N) * REGISTER_RAW_SIZE(0)) \
			  + ((N) >= XFP_REGNUM \
			     ? (((N) - XFP_REGNUM) \
				* REGISTER_RAW_SIZE(XFP_REGNUM)) \
			     : 0))

  /* Number of bytes of storage in the program's representation for
     register N.  On the m88k, all registers are 4 bytes excepting the
     m88110 extended registers which are 8 byte doubles. */

#define REGISTER_VIRTUAL_SIZE(N) ((N) < XFP_REGNUM ? 4 : 8)

  /* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE (REGISTER_RAW_SIZE(XFP_REGNUM))

  /* Largest value REGISTER_VIRTUAL_SIZE can have.
     Are FPS1, FPS2, FPR "virtual" regisers? */

#define MAX_REGISTER_VIRTUAL_SIZE (REGISTER_RAW_SIZE(XFP_REGNUM))

  /* Nonzero if register N requires conversion
     from raw format to virtual format.  */

#define REGISTER_CONVERTIBLE(N) ((N) >= XFP_REGNUM)

#include "floatformat.h"

/* Convert data from raw format for register REGNUM in buffer FROM
   to virtual format with type TYPE in buffer TO.  */

#define REGISTER_CONVERT_TO_VIRTUAL(REGNUM,TYPE,FROM,TO) \
{ \
  double val; \
  floatformat_to_double (&floatformat_m88110_ext, (FROM), &val); \
  store_floating ((TO), TYPE_LENGTH (TYPE), val); \
}

/* Convert data from virtual format with type TYPE in buffer FROM
   to raw format for register REGNUM in buffer TO.  */

#define REGISTER_CONVERT_TO_RAW(TYPE,REGNUM,FROM,TO)	\
{ \
  double val = extract_floating ((FROM), TYPE_LENGTH (TYPE)); \
  floatformat_from_double (&floatformat_m88110_ext, &val, (TO)); \
}

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) \
((N) >= XFP_REGNUM \
 ? builtin_type_double \
 : ((N) == PC_REGNUM || (N) == FP_REGNUM || (N) == SP_REGNUM \
    ? lookup_pointer_type (builtin_type_void) : builtin_type_int))

/* The 88k call/return conventions call for "small" values to be returned
   into consecutive registers starting from r2.  */

#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  memcpy ((VALBUF), &(((char *)REGBUF)[REGISTER_BYTE(RV_REGNUM)]), TYPE_LENGTH (TYPE))

#define EXTRACT_STRUCT_VALUE_ADDRESS(REGBUF) (*(int *)(REGBUF))

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  */

#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  write_register_bytes (2*REGISTER_RAW_SIZE(0), (VALBUF), TYPE_LENGTH (TYPE))

/* In COFF, if PCC says a parameter is a short or a char, do not
   change it to int (it seems the convention is to change it). */

#define BELIEVE_PCC_PROMOTION 1

/* Describe the pointer in each stack frame to the previous stack frame
   (its caller).  */

/* FRAME_CHAIN takes a frame's nominal address
   and produces the frame's chain-pointer.

   However, if FRAME_CHAIN_VALID returns zero,
   it means the given frame is the outermost one and has no caller.  */

extern CORE_ADDR frame_chain ();
extern int frame_chain_valid ();
extern int frameless_function_invocation ();

#define FRAME_CHAIN(thisframe) \
	frame_chain (thisframe)

#define	FRAMELESS_FUNCTION_INVOCATION(frame, fromleaf)	\
	fromleaf = frameless_function_invocation (frame)

/* Define other aspects of the stack frame.  */

#define FRAME_SAVED_PC(FRAME)	\
	frame_saved_pc (FRAME)
extern CORE_ADDR frame_saved_pc ();

#define FRAME_ARGS_ADDRESS(fi)	\
	frame_args_address (fi)
extern CORE_ADDR frame_args_address ();

#define FRAME_LOCALS_ADDRESS(fi) \
	frame_locals_address (fi)
extern CORE_ADDR frame_locals_address ();

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */

#define FRAME_NUM_ARGS(numargs, fi)  ((numargs) = -1)

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 0

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

/* On the 88k, parameter registers get stored into the so called "homing"
   area.  This *always* happens when you compiled with GCC and use -g.
   Also, (with GCC and -g) the saving of the parameter register values
   always happens right within the function prologue code, so these register
   values can generally be relied upon to be already copied into their
   respective homing slots by the time you will normally try to look at
   them (we hope).

   Note that homing area stack slots are always at *positive* offsets from
   the frame pointer.  Thus, the homing area stack slots for the parameter
   registers (passed values) for a given function are actually part of the
   frame area of the caller.  This is unusual, but it should not present
   any special problems for GDB.

   Note also that on the 88k, we are only interested in finding the
   registers that might have been saved in memory.  This is a subset of
   the whole set of registers because the standard calling sequence allows
   the called routine to clobber many registers.

   We could manage to locate values for all of the so called "preserved"
   registers (some of which may get saved within any particular frame) but
   that would require decoding all of the tdesc information.  That would be
   nice information for GDB to have, but it is not strictly manditory if we
   can live without the ability to look at values within (or backup to)
   previous frames.
*/

struct frame_saved_regs;
struct frame_info;

void frame_find_saved_regs PARAMS((struct frame_info *fi,
				   struct frame_saved_regs *fsr));

#define FRAME_FIND_SAVED_REGS(frame_info, frame_saved_regs) \
        frame_find_saved_regs (frame_info, &frame_saved_regs)


#define POP_FRAME pop_frame ()
extern void pop_frame ();

/* Call function stuff contributed by Kevin Buettner of Motorola.  */

#define CALL_DUMMY_LOCATION AFTER_TEXT_END

extern void m88k_push_dummy_frame();
#define PUSH_DUMMY_FRAME	m88k_push_dummy_frame()

#define CALL_DUMMY { 				\
0x67ff00c0,	/*   0:   subu	#sp,#sp,0xc0 */ \
0x243f0004,	/*   4:   st	#r1,#sp,0x4 */ \
0x245f0008,	/*   8:   st	#r2,#sp,0x8 */ \
0x247f000c,	/*   c:   st	#r3,#sp,0xc */ \
0x249f0010,	/*  10:   st	#r4,#sp,0x10 */ \
0x24bf0014,	/*  14:   st	#r5,#sp,0x14 */ \
0x24df0018,	/*  18:   st	#r6,#sp,0x18 */ \
0x24ff001c,	/*  1c:   st	#r7,#sp,0x1c */ \
0x251f0020,	/*  20:   st	#r8,#sp,0x20 */ \
0x253f0024,	/*  24:   st	#r9,#sp,0x24 */ \
0x255f0028,	/*  28:   st	#r10,#sp,0x28 */ \
0x257f002c,	/*  2c:   st	#r11,#sp,0x2c */ \
0x259f0030,	/*  30:   st	#r12,#sp,0x30 */ \
0x25bf0034,	/*  34:   st	#r13,#sp,0x34 */ \
0x25df0038,	/*  38:   st	#r14,#sp,0x38 */ \
0x25ff003c,	/*  3c:   st	#r15,#sp,0x3c */ \
0x261f0040,	/*  40:   st	#r16,#sp,0x40 */ \
0x263f0044,	/*  44:   st	#r17,#sp,0x44 */ \
0x265f0048,	/*  48:   st	#r18,#sp,0x48 */ \
0x267f004c,	/*  4c:   st	#r19,#sp,0x4c */ \
0x269f0050,	/*  50:   st	#r20,#sp,0x50 */ \
0x26bf0054,	/*  54:   st	#r21,#sp,0x54 */ \
0x26df0058,	/*  58:   st	#r22,#sp,0x58 */ \
0x26ff005c,	/*  5c:   st	#r23,#sp,0x5c */ \
0x271f0060,	/*  60:   st	#r24,#sp,0x60 */ \
0x273f0064,	/*  64:   st	#r25,#sp,0x64 */ \
0x275f0068,	/*  68:   st	#r26,#sp,0x68 */ \
0x277f006c,	/*  6c:   st	#r27,#sp,0x6c */ \
0x279f0070,	/*  70:   st	#r28,#sp,0x70 */ \
0x27bf0074,	/*  74:   st	#r29,#sp,0x74 */ \
0x27df0078,	/*  78:   st	#r30,#sp,0x78 */ \
0x63df0000,	/*  7c:   addu	#r30,#sp,0x0 */ \
0x145f0000,	/*  80:   ld	#r2,#sp,0x0 */ \
0x147f0004,	/*  84:   ld	#r3,#sp,0x4 */ \
0x149f0008,	/*  88:   ld	#r4,#sp,0x8 */ \
0x14bf000c,	/*  8c:   ld	#r5,#sp,0xc */ \
0x14df0010,	/*  90:   ld	#r6,#sp,0x10 */ \
0x14ff0014,	/*  94:   ld	#r7,#sp,0x14 */ \
0x151f0018,	/*  98:   ld	#r8,#sp,0x18 */ \
0x153f001c,	/*  9c:   ld	#r9,#sp,0x1c */ \
0x5c200000,	/*  a0:   or.u	#r1,#r0,0x0 */ \
0x58210000,	/*  a4:   or	#r1,#r1,0x0 */ \
0xf400c801,	/*  a8:   jsr	#r1 */ \
0xf000d1ff	/*  ac:   tb0	0x0,#r0,0x1ff */ \
}

#define CALL_DUMMY_START_OFFSET 0x80
#define CALL_DUMMY_LENGTH 0xb0

/* FIXME: byteswapping.  */
#define FIX_CALL_DUMMY(dummy, pc, fun, nargs, args, type, gcc_p)	\
{ 									\
  *(unsigned long *)((char *) (dummy) + 0xa0) |=			\
	(((unsigned long) (fun)) >> 16);				\
  *(unsigned long *)((char *) (dummy) + 0xa4) |=			\
	(((unsigned long) (fun)) & 0xffff);				\
  pc = text_end;							\
}

/* Stack must be aligned on 64-bit boundaries when synthesizing
   function calls. */

#define STACK_ALIGN(addr) (((addr) + 7) & -8)

#define STORE_STRUCT_RETURN(addr, sp) \
    write_register (SRA_REGNUM, (addr))

#define NEED_TEXT_START_END 1

/* According to the MC88100 RISC Microprocessor User's Manual, section
   6.4.3.1.2:

   	... can be made to return to a particular instruction by placing a
	valid instruction address in the SNIP and the next sequential
	instruction address in the SFIP (with V bits set and E bits clear).
	The rte resumes execution at the instruction pointed to by the 
	SNIP, then the SFIP.

   The E bit is the least significant bit (bit 0).  The V (valid) bit is
   bit 1.  This is why we logical or 2 into the values we are writing
   below.  It turns out that SXIP plays no role when returning from an
   exception so nothing special has to be done with it.  We could even
   (presumably) give it a totally bogus value.

   -- Kevin Buettner
*/
 
#define TARGET_WRITE_PC(val, pid) { \
  write_register_pid(SXIP_REGNUM, (long) val, pid); \
  write_register_pid(SNIP_REGNUM, (long) val | 2, pid); \
  write_register_pid(SFIP_REGNUM, ((long) val | 2) + 4, pid); \
}
