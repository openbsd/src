/*	$OpenBSD: db_trace.c,v 1.5 1999/02/09 06:36:25 smurph Exp $	*/
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

union instruction {
    unsigned rawbits;

    struct {
	unsigned int    : 5;
	unsigned int   n: 1;
	signed int   d26:26;
    } br;

    struct {
	unsigned int      : 4;
	unsigned int isbb1: 1;	/* isbb1==0 means bb0, isbb1==1 means bb1 */
	unsigned int   n  : 1;
	unsigned int  b5  : 5;
	unsigned int  s1  : 5;
	signed   int  d16 :16;
    } bb; /* bcnd too, except "isbb1" makes no sense for bcnd */

    struct {
	unsigned int      : 6;
	unsigned int  b5  : 5;
	unsigned int  s1  : 5;
	unsigned int      : 7;
	unsigned int vec9 : 9;
    } tb; /* tcnd too */

    struct {
	unsigned int      :21;
	unsigned int    n : 1;
	unsigned int      : 5;
	unsigned int   s2 : 5;
    } jump; /* jmp, jsr */

    struct {
	unsigned int      : 6;
        unsigned int    d : 5;
        unsigned int   s1 : 5;
        unsigned int  i16 :16;
    } diatic;	/* general reg/reg/i16 instructions */

    struct {
	unsigned int      : 6;
        unsigned int    d : 5;
        unsigned int   s1 : 5;
        unsigned int      :11;
        unsigned int   s2 : 5;
    } triatic;  /* general reg/reg/reg instructions */

};

static inline unsigned br_dest(unsigned addr, union instruction inst)
{
    return addr + inst.br.d26 * 4;
}


#define TRACE_DEBUG	/* undefine to disable debugging */

#include <machine/db_machdep.h> /* lots of stuff                  */
#include <ddb/db_variables.h>	/* db_variable, DB_VAR_GET, etc.  */
#include <ddb/db_output.h>	/* db_printf                      */
#include <ddb/db_sym.h>		/* DB_STGY_PROC, etc.             */
#include <ddb/db_command.h>	/* db_recover                     */

/*
 * Some macros to tell if the given text is the instruction.
 */
#define JMPN_R1(I)	    ( (I) == 0xf400c401U)	/* jmp.n   r1 */
#define JMP_R1(I)	    ( (I) == 0xf400c001U)	/* jmp     r1 */

/* gets the IMM16 value from an instruction */
#define IMM16VAL(I)	    (((union instruction)(I)).diatic.i16)

/* subu r31, r31, IMM */
#define SUBU_R31_R31_IMM(I) (((I) & 0xffff0000U) == 0x67ff0000U)

/* st r1, r31, IMM */
#define ST_R1_R31_IMM(I)    (((I) & 0xffff0000U) == 0x243f0000U)

static trace_flags = 0;
#define TRACE_DEBUG_FLAG		0x01
#define TRACE_SHOWCALLPRESERVED_FLAG	0x02
#define TRACE_SHOWADDRESS_FLAG		0x04
#define TRACE_SHOWFRAME_FLAG		0x08
#define TRACE_USER_FLAG			0x10

#ifdef TRACE_DEBUG
  #define DEBUGGING_ON (trace_flags & TRACE_DEBUG_FLAG)
#endif

#ifndef TRACE_DEBUG
  #define SHOW_INSTRUCTION(Addr, Inst, Note) 	{ /*nothing*/ }
#else
  #define SHOW_INSTRUCTION(Addr, Inst, Note) if (DEBUGGING_ON) { 	\
    db_printf("%s0x%x: (0x%08x) ", Note, (unsigned)(Addr), (Inst));	\
    m88k_print_instruction((unsigned)(Addr), (Inst));			\
    db_printf("\n");					\
  }
#endif

extern label_t *db_recover;
extern int quiet_db_read_bytes;
/*
 * m88k trace/register state interface for ddb.
 */

/* lifted from mips */
static int
db_setf_regs(
	struct db_variable	*vp,
	db_expr_t		*valuep,
	int			op)		/* read/write */
{
    register int   *regp = (int *) ((char *) DDB_REGS + (int) (vp->valuep));

    if (op == DB_VAR_GET)
	*valuep = *regp;
    else if (op == DB_VAR_SET)
	*regp = *valuep;
}

#define N(s, x)  {s, (long *)&(((db_regs_t *) 0)->x), db_setf_regs}

struct db_variable db_regs[] = {
    N("r1", r[1]),     N("r2", r[2]), 	 N("r3", r[3]),	   N("r4", r[4]),
    N("r5", r[5]),     N("r6", r[6]), 	 N("r7", r[7]),	   N("r8", r[8]),
    N("r9", r[9]),     N("r10", r[10]),  N("r11", r[11]),  N("r12", r[12]),
    N("r13", r[13]),   N("r14", r[14]),  N("r15", r[15]),  N("r16", r[16]),
    N("r17", r[17]),   N("r18", r[18]),  N("r19", r[19]),  N("r20", r[20]),
    N("r21", r[21]),   N("r22", r[22]),  N("r23", r[23]),  N("r24", r[24]),
    N("r25", r[25]),   N("r26", r[26]),  N("r27", r[27]),  N("r28", r[28]),
    N("r29", r[29]),   N("r30", r[30]),  N("r31", r[31]),  N("epsr", epsr),
    N("sxip", sxip),   N("snip", snip),  N("sfip", sfip),  N("ssbr", ssbr),
    N("dmt0", dmt0),   N("dmd0", dmd0),  N("dma0", dma0),  N("dmt1", dmt1),
    N("dmd1", dmd1),   N("dma1", dma1),  N("dmt2", dmt2),  N("dmd2", dmd2),
    N("dma2", dma2),   N("fpecr", fpecr),N("fphs1", fphs1),N("fpls1", fpls1),
    N("fphs2", fphs2), N("fpls2", fpls2),N("fppt", fppt),  N("fprh", fprh),
    N("fprl", fprl),   N("fpit", fpit),  N("fpsr", fpsr),  N("fpcr", fpcr),
    N("mask", mask), /* interrupt mask */
    N("mode", mode), /* interrupt mode */
    N("exvc", vector), /* exception vector */
};
#undef N

struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);


#define TRASHES    0x001	/* clobbers instruction field D */
#define STORE      0x002	/* does a store to S1+IMM16 */
#define LOAD       0x004	/* does a load from S1+IMM16 */
#define DOUBLE     0x008	/* double-register */
#define FLOW_CTRL  0x010	/* flow-control instruction */
#define DELAYED    0x020	/* delayed flow control */
#define JSR	   0x040	/* flow-control is a jsr[.n] */
#define BSR	   0x080	/* flow-control is a bsr[.n] */

/*
 * Given a word of instruction text, return some flags about that
 * instruction (flags defined above).
 */
static unsigned
m88k_instruction_info(unsigned instruction)
{
   static struct { unsigned mask, value, flags; } *ptr, control[] =
   {
      /* runs in the same order as 2nd Ed 88100 manual Table 3-14 */
     { 0xf0000000U, 0x00000000U, /* xmem */     TRASHES | STORE | LOAD    },
     { 0xec000000U, 0x00000000U, /* ld.d */     TRASHES | LOAD | DOUBLE   },
     { 0xe0000000U, 0x00000000U, /* load */     TRASHES | LOAD            },
     { 0xfc000000U, 0x20000000U, /* st.d */     STORE | DOUBLE            },
     { 0xf0000000U, 0x20000000U, /* store */    STORE                     },
     { 0xc0000000U, 0x40000000U, /* arith */    TRASHES                   },
     { 0xfc004000U, 0x80004000U, /* ld cr */    TRASHES                   },
     { 0xfc004000U, 0x80000000U, /* st cr */    0                         },
     { 0xfc008060U, 0x84000000U, /* f */        TRASHES                   },
     { 0xfc008060U, 0x84000020U, /* f.d */      TRASHES | DOUBLE          },
     { 0xfc000000U, 0xcc000000U, /* bsr.n */    FLOW_CTRL | DELAYED | BSR },
     { 0xfc000000U, 0xc8000000U, /* bsr */      FLOW_CTRL | BSR           },
     { 0xe4000000U, 0xc4000000U, /* br/bb.n */  FLOW_CTRL | DELAYED       },
     { 0xe4000000U, 0xc0000000U, /* br/bb */    FLOW_CTRL                 },
     { 0xfc000000U, 0xec000000U, /* bcnd.n */   FLOW_CTRL | DELAYED       },
     { 0xfc000000U, 0xe8000000U, /* bcnd */     FLOW_CTRL                 },
     { 0xfc00c000U, 0xf0008000U, /* bits */     TRASHES                   },
     { 0xfc00c000U, 0xf000c000U, /* trap */     0                         },
     { 0xfc00f0e0U, 0xf4002000U, /* st */       0                         },
     { 0xfc00cce0U, 0xf4000000U, /* ld.d */     TRASHES | DOUBLE          },
     { 0xfc00c0e0U, 0xf4000000U, /* ld */       TRASHES                   },
     { 0xfc00c0e0U, 0xf4004000U, /* arith */    TRASHES                   },
     { 0xfc00c3e0U, 0xf4008000U, /* bits */     TRASHES                   },
     { 0xfc00ffe0U, 0xf400cc00U, /* jsr.n */    FLOW_CTRL | DELAYED | JSR },
     { 0xfc00ffe0U, 0xf400c800U, /* jsr */      FLOW_CTRL | JSR           },
     { 0xfc00ffe0U, 0xf400c400U, /* jmp.n */    FLOW_CTRL | DELAYED       },
     { 0xfc00ffe0U, 0xf400c000U, /* jmp */      FLOW_CTRL                 },
     { 0xfc00fbe0U, 0xf400e800U, /* ff */       TRASHES                   },
     { 0xfc00ffe0U, 0xf400f800U, /* tbnd */     0                         },
     { 0xfc00ffe0U, 0xf400fc00U, /* rte */      FLOW_CTRL                 },
     { 0xfc000000U, 0xf8000000U, /* tbnd */     0                         },
    };
    #define ctrl_count (sizeof(control)/sizeof(control[0]))
    for (ptr = &control[0]; ptr < &control[ctrl_count]; ptr++)
	if ((instruction & ptr->mask) == ptr->value)
	    return ptr->flags;
    SHOW_INSTRUCTION(0, instruction, "bad m88k_instruction_info");
    return 0;
}

static int
hex_value_needs_0x(unsigned value)
{
    int i;
    unsigned last = 0;
    unsigned char c; 
    unsigned have_a_hex_digit = 0;

    if (value <= 9)
	return 0;

    for (i = 0; i < 8; i++) {
	c = value & 0xf;
	value >>= 4;
	if (c)
	    last = c;
	if (c > 9)
	    have_a_hex_digit = 1;
    }
    if (have_a_hex_digit == 0)
	return 1;
    if (last > 9)
	return 1;
    return 0;
}


/*
 * returns
 *   1 if regs seems to be a reasonable kernel exception frame.
 *   2 if regs seems to be a reasonable user exception frame
 * 	(in the current task).
 *   0 if this looks like neither.
 */
int
frame_is_sane(db_regs_t *regs)
{
    /* no good if we can't read the whole frame */
    if (badwordaddr((vm_offset_t)regs) || badwordaddr((vm_offset_t)&regs->mode))
	return 0;

#ifndef DIAGNOSTIC
    /* disabled for now  -- see fpu_enable in luna88k/eh.s */
    /* r0 must be 0 (obviously) */
    if (regs->r[0] != 0)
	return 0;
#endif

    /* stack sanity ... r31 must be nonzero, but must be word aligned */
    if (regs->r[31] == 0 || (regs->r[31] & 3) != 0)
	return 0;

    /* sxip is reasonable */
#if 0
    if ((regs->sxip & 1) == 1)
	return 0;
#endif
    /* snip is reasonable */
    if ((regs->snip & 3) != 2)
	return 0;
    /* sfip is reasonable */
    if ((regs->sfip & 3) != 2)
	return 0;

    /* epsr sanity */
    if ((regs->epsr & 0x8FFFFFF5U) == 0x800003f0U) /* kernel mode */
    {
	if (regs->epsr & 0x40000000) 
	  db_printf("[WARNING: byte order in kernel frame at %x "
		    "is little-endian!]\n", regs);
	return 1;
    }
    if ((regs->epsr & 0x8FFFFFFFU) == 0x000003f0U) /* user mode */
    {
	if (regs->epsr & 0x40000000) 
	  db_printf("[WARNING: byte order in user frame at %x "
		    "is little-endian!]\n", regs);
	return 2;
    }
    return 0;
}

char
*m88k_exception_name(unsigned vector)
{
    switch  (vector)
    {
	default:
	case   0: return "Reset";
	case   1: return "Interrupt";
	case   2: return "Instruction Access Exception";
	case   3: return "Data Access Exception";
	case   4: return "Misaligned Access Exception";
	case   5: return "Unimplemented Opcode Exception";
	case   6: return "Privilege Violation";
	case   7: return "Bounds Check";
	case   8: return "Integer Divide Exception";
	case   9: return "Integer Overflow Exception";
	case  10: return "Error Exception";
	case 114: return "FPU precise";
	case 115: return "FPU imprecise";
	case 130: return "Ddb break";
	case 131: return "Ddb trace";
	case 132: return "Ddb trap";
	case 451: return "Syscall";
    }
}

/*
 * Read a word at address addr.
 * Return 1 if was able to read, 0 otherwise.
 */
unsigned
db_trace_get_val(vm_offset_t addr, unsigned *ptr)
{
    label_t db_jmpbuf;
    label_t *prev = db_recover;
    boolean_t old_quiet_db_read_bytes = quiet_db_read_bytes;

    quiet_db_read_bytes = 1;

    if (setjmp(*(db_recover = &db_jmpbuf)) != 0) {
	db_recover = prev;
        quiet_db_read_bytes = old_quiet_db_read_bytes;
	return 0;
    } else {
	db_read_bytes((char*)addr, 4, (char*)ptr);
	db_recover = prev;
        quiet_db_read_bytes = old_quiet_db_read_bytes;
	return 1;
    }
}


#define FIRST_CALLPRESERVED_REG 14
#define LAST_CALLPRESERVED_REG  29
#define FIRST_ARG_REG       2
#define LAST_ARG_REG        9
#define RETURN_VAL_REG           1

static unsigned global_saved_list = 0x0; /* one bit per register */
static unsigned local_saved_list  = 0x0; /* one bit per register */
static unsigned trashed_list      = 0x0; /* one bit per register */
static unsigned saved_reg[32];		 /* one value per register */

#define reg_bit(reg) (1<<((reg)%32))

static void
save_reg(int reg, unsigned value)
{
    #ifdef TRACE_DEBUG
	if (DEBUGGING_ON) db_printf("save_reg(%d, %x)\n", reg, value);
    #endif
    if (trashed_list & reg_bit(reg)) {
	#ifdef TRACE_DEBUG
	    if (DEBUGGING_ON) db_printf("<trashed>\n");
	#endif
	return; /* don't save trashed registers */
    }
    saved_reg[(reg%32)] = value;
    global_saved_list |= reg_bit(reg);
    local_saved_list  |= reg_bit(reg);
}

#define mark_reg_trashed(reg)	(trashed_list |= reg_bit(reg))

#define have_global_reg(reg) (global_saved_list & (1<<(reg)))
#define have_local_reg(reg)  (local_saved_list & (1<<(reg)))

#define clear_local_saved_regs()  { local_saved_list = trashed_list =      0; }
#define clear_global_saved_regs() { local_saved_list = global_saved_list = 0; }

#define saved_reg_value(reg) (saved_reg[(reg)])

/*
 * Show any arguments that we might have been able to determine.
 */
static void
print_args(void)
{
    int reg, last_arg;

    /* find the highest argument register saved */
    for (last_arg = LAST_ARG_REG; last_arg >= FIRST_ARG_REG; last_arg--)
	if (have_local_reg(last_arg))
	    break;
    if (last_arg < FIRST_ARG_REG)
	return; /* none were saved */

    db_printf("(");

    /* print each one, up to the highest */
    for (reg = FIRST_ARG_REG; /*nothing */; reg++)
    {
	if (!have_local_reg(reg))
	    db_printf("?");
	else {
	    unsigned value = saved_reg_value(reg);
	    db_printf("%s%x", hex_value_needs_0x(value)  ? "0x" : "", value);
	}
	if (reg == last_arg)
	    break;
	else
	    db_printf(", ");
    }
    db_printf(")");
}


#define JUMP_SOURCE_IS_BAD		0
#define JUMP_SOURCE_IS_OK		1
#define JUMP_SOURCE_IS_UNLIKELY		2

/*
 * Give an address to where we return, and an address to where we'd jumped,
 * Decided if it all makes sense.
 *
 * Gcc sometimes optimized something like
 *	if (condition)
 *		func1();
 *	else
 *		OtherStuff...
 * to
 *	bcnd   !condition  mark
 *	bsr.n  func1
 *	or     r1, r0, mark2
 *    mark:
 *	OtherStuff...
 *    mark2:
 *
 * So RETURN_TO will be MARK2, even though we really did branch via
 * 'bsr.n func1', so this makes it difficult to be certaian about being
 * wrong.
 */
static int
is_jump_source_ok(unsigned return_to, unsigned jump_to)
{
    unsigned flags;
    union instruction instruction; 

    /*
     * Delayed branches are most common... look two instructions before
     * where we were going to return to to see if it's a delayed branch.
     */
    if (!db_trace_get_val(return_to - 8, &instruction.rawbits))
	return JUMP_SOURCE_IS_BAD;
    flags = m88k_instruction_info(instruction.rawbits);

    if ((flags & FLOW_CTRL) && (flags & DELAYED) && (flags & (JSR|BSR))) {
	if (flags & JSR)
	    return JUMP_SOURCE_IS_OK; /* have to assume it's correct */
	/* calculate the offset */
	if (br_dest(return_to - 8, instruction) == jump_to)
	    return JUMP_SOURCE_IS_OK; /* exactamundo! */
	else
	    return JUMP_SOURCE_IS_UNLIKELY; /* seems wrong */
    }

    /*
     * Try again, looking for a non-delayed jump one back.
     */
    if (!db_trace_get_val(return_to - 4, &instruction.rawbits))
	return JUMP_SOURCE_IS_BAD;
    flags = m88k_instruction_info(instruction.rawbits);

    if ((flags & FLOW_CTRL) && !(flags & DELAYED) && (flags & (JSR|BSR))) {
	if (flags & JSR)
	    return JUMP_SOURCE_IS_OK; /* have to assume it's correct */
	/* calculate the offset */
	if (br_dest(return_to - 4, instruction) == jump_to)
	    return JUMP_SOURCE_IS_OK; /* exactamundo! */
	else
	    return JUMP_SOURCE_IS_UNLIKELY; /* seems wrong */
    }

    return JUMP_SOURCE_IS_UNLIKELY;
}

static char *note = 0;
static int next_address_likely_wrong = 0;

/* How much slop we expect in the stack trace */
#define FRAME_PLAY 8

/*
 *  Stack decode -
 *	unsigned addr;    program counter
 *	unsigned *stack; IN/OUT stack pointer
 *
 * 	given an address within a function and a stack pointer,
 *	try to find the function from which this one was called
 *	and the stack pointer for that function.
 *
 *	The return value is zero (if we get confused) or
 *	we determine that the return address has not yet
 *	been saved (early in the function prologue). Otherwise
 *	the return value is the address from which this function
 *	was called.
 *
 *	Note that even is zero is returned (the second case) the
 *	stack pointer can be adjusted.
 *
 */
static int
stack_decode(unsigned addr, unsigned *stack)
{
    db_sym_t proc;
    unsigned offset_from_proc;
    unsigned instructions_to_search;
    unsigned check_addr;
    unsigned function_addr;	/* start of function */
    unsigned r31 = *stack;	/* the r31 of the function */
    unsigned inst;		/* text of an instruction */
    unsigned ret_addr;		/* address to which we return */
    unsigned tried_to_save_r1 = 0;

    #ifdef TRACE_DEBUG
      if (DEBUGGING_ON)
	db_printf("\n>>>stack_decode(addr=%x, stack=%x)\n",
		addr, *stack);
    #endif

      /* get what we hope will be the db_sym_t for the function name */
    proc = db_search_symbol(addr, DB_STGY_PROC, &offset_from_proc);
    if (offset_from_proc == addr) /* i.e. no symbol found */
	proc = DB_SYM_NULL;

    /*
     * Somehow, find the start of this function.
     * If we found a symbol above, it'll have the address.
     * Otherwise, we've got to search for it....
     */
    if (proc != DB_SYM_NULL)
    {
	char *names;
	db_symbol_values(proc, &names, &function_addr);
	if (names == 0)
	    return 0;
	#ifdef TRACE_DEBUG
	    if (DEBUGGING_ON) db_printf("name %s address 0x%x\n",
		names, function_addr);
	#endif
    }
    else
    {
	int instructions_to_check = 400;
	/*
	 * hmm - unable to find symbol. Search back
	 * looking for a function prolog.
	 */
	for (check_addr = addr; instructions_to_check-- > 0; check_addr -= 4)
	{
	    if (!db_trace_get_val(check_addr, &inst))
		break;

	    if (SUBU_R31_R31_IMM(inst))
	    {
		#if 0
		    /*
		     * If the next instruction is "st r1, r31, ####"
		     * then we can feel safe we have the start of
		     * a function.
		     */
		    if (!db_trace_get_val(check_addr + 4, &inst))
			continue;
		    if (ST_R1_R31_IMM(instr))
			break; /* sucess */
		#else
		    /*
		     * Latest GCC optimizer is just too good... the store
		     * of r1 might come much later... so we'll have to
		     * settle for just the "subr r31, r31, ###" to mark
		     * the start....
		     */
		     break;
		#endif
	    }
	    /*
	     * if we come across a [jmp r1] or [jmp.n r1] assume we have hit
	     * the previous functions epilogue and stop our search.
	     * Since we know we would have hit the "subr r31, r31" if it was
	     * right in front of us, we know this doesn't have one so
	     * we just return failure....
	     */
	    if (JMP_R1(inst) || JMPN_R1(inst)) {
		#ifdef TRACE_DEBUG
		    if (DEBUGGING_ON)
			db_printf("ran into a [jmp r1] at %x (addr=%x)\n",
				check_addr, addr);
		#endif
		return 0;
	    }
	}
	if (instructions_to_check < 0) {
	    #ifdef TRACE_DEBUG
		if (DEBUGGING_ON)
		    db_printf("couldn't find func start (addr=%x)\n", addr);
	    #endif
	    return 0; /* bummer, couldn't find it */
	}
	function_addr = check_addr;
    }

    /*
     * We now know the start of the function (function_addr).
     * If we're stopped right there, or if it's not a
     *		subu r31, r31, ####
     * then we're done.
     */
    if (addr == function_addr) {
	#ifdef TRACE_DEBUG
	    if (DEBUGGING_ON) db_printf("at start of func\n");
	#endif
	return 0;
    }
    if (!db_trace_get_val(function_addr, &inst)) {
	#ifdef TRACE_DEBUG
	    if (DEBUGGING_ON) db_printf("couldn't read %x at line %d\n",
		function_addr, __LINE__);
	#endif
	return 0;
    }
    SHOW_INSTRUCTION(function_addr, inst, "start of function: ");
    if (!SUBU_R31_R31_IMM(inst)) {
	#ifdef TRACE_DEBUG
	    if (DEBUGGING_ON) db_printf("<not subu,r31,r31,imm>\n");
	#endif
	return 0;
    }

    /* add the size of this frame to the stack (for the next frame) */
    *stack += IMM16VAL(inst);

    /*
     * Search from the beginning of the function (funstart) to where we are
     * in the function (addr) looking to see what kind of registers have
     * been saved on the stack.
     *
     * We'll stop looking before we get to ADDR if we hit a branch.
     */
    clear_local_saved_regs();
    check_addr = function_addr + 4; /* we know the first inst isn't a store */

    for (instructions_to_search = (addr - check_addr)/sizeof(long);
	instructions_to_search-- > 0;
	check_addr += 4)
    {
 	union instruction instruction;
	unsigned flags;

	/* read the instruction */
        if (!db_trace_get_val(check_addr, &instruction.rawbits)) {
	    #ifdef TRACE_DEBUG
		if (DEBUGGING_ON) db_printf("couldn't read %x at line %d\n",
		    check_addr, __LINE__);
	    #endif
            break;
	}

	SHOW_INSTRUCTION(check_addr, instruction.rawbits, "prolog: ");

	/* find out the particulars about this instruction */
	flags = m88k_instruction_info(instruction.rawbits);

	/* if a store to something off the stack pointer, note the value */
	if ((flags & STORE) && instruction.diatic.s1 == /*stack pointer*/31)
	{
	    unsigned value;
	    if (!have_local_reg(instruction.diatic.d)) {
		if (instruction.diatic.d == 1)
			tried_to_save_r1 = r31 + instruction.diatic.i16 ;
		if (db_trace_get_val(r31 + instruction.diatic.i16, &value))
		    save_reg(instruction.diatic.d, value);
	    }
	    if ((flags & DOUBLE) && !have_local_reg(instruction.diatic.d + 1)) {
		if (instruction.diatic.d == 0)
		    tried_to_save_r1 = r31+instruction.diatic.i16 +4;
		if (db_trace_get_val(r31+instruction.diatic.i16 +4, &value))
		    save_reg(instruction.diatic.d + 1, value);
	    }
	}

	/* if an inst that kills D (and maybe D+1), note that */
	if (flags & TRASHES) {
	    mark_reg_trashed(instruction.diatic.d);
	    if (flags & DOUBLE)
	        mark_reg_trashed(instruction.diatic.d + 1);
	}

	/* if a flow control instruction, stop now (or next if delayed) */
	if ((flags & FLOW_CTRL) && instructions_to_search != 0)
	    instructions_to_search = (flags & DELAYED) ? 1 : 0;
    }

    /*
     * If we didn't save r1 at some point, we're hosed.
     */
    if (!have_local_reg(1)) {
	if (tried_to_save_r1) {
	    db_printf("    <return value of next fcn unreadable in %08x>\n",
		tried_to_save_r1);
	}
	#ifdef TRACE_DEBUG
	    if (DEBUGGING_ON) db_printf("didn't save r1\n");
	#endif
	return 0;
    }

    ret_addr = saved_reg_value(1);

    #ifdef TRACE_DEBUG
    if (DEBUGGING_ON)
	db_printf("Return value is = %x, function_addr is %x.\n",
	    ret_addr, function_addr);
    #endif

    /*
     * In support of this, continuation.s puts the low bit on the
     * return address for continuations (the return address will never
     * be used, so it's ok to do anything you want to it).
     */
    if (ret_addr & 1) {
	note = "<<can not trace past a continuation>>";
	ret_addr = 0;
    } else if (ret_addr != 0x00) {
	switch(is_jump_source_ok(ret_addr, function_addr)) {
	  case JUMP_SOURCE_IS_OK:
		break; /* excellent */

	  case JUMP_SOURCE_IS_BAD:
		#ifdef TRACE_DEBUG
		    if (DEBUGGING_ON) db_printf("jump is bad\n");
		#endif
		return 0; /* bummer */

	  case JUMP_SOURCE_IS_UNLIKELY:
		next_address_likely_wrong = 1;;
		break;
	}
    }

    return ret_addr;
}

static void
db_stack_trace_cmd2(db_regs_t *regs)
{
    unsigned stack;
    unsigned depth=1;
    unsigned where;
    unsigned ft;
    unsigned pair[2];
    int i;

    /*
     * Frame_is_sane returns:
     *   1 if regs seems to be a reasonable kernel exception frame.
     *   2 if regs seems to be a reasonable user exception frame
     *      (in the current task).
     *   0 if this looks like neither.
     */
    if (ft = frame_is_sane(regs), ft == 0)
    {
	db_printf("Register frame 0x%x is suspicous; skipping trace\n", regs);
	return;
    }

    /* if user space and no user space trace specified, puke */
    if (ft == 2 && !(trace_flags & TRACE_USER_FLAG))
	return;

    /* fetch address */
    /* use sxip if valid, otherwise try snip or sfip */
    where = ((regs->sxip & 2) ? regs->sxip :
	    ((regs->snip & 2) ? regs->snip :
	      regs->sfip) ) & ~3;
    stack = regs->r[31];
    db_printf("stack base = 0x%x\n", stack);
    db_printf("(0) "); /*depth of trace */
    if (trace_flags & TRACE_SHOWADDRESS_FLAG)
	db_printf("%08x ", where);
    db_printsym(where, DB_STGY_PROC);
    clear_global_saved_regs();

    /* see if this routine had a stack frame */
    if ((where=stack_decode(where, &stack))==0)
    {
	where = regs->r[1];
	db_printf("(stackless)");
    }
    else
    {
	print_args();
	if (trace_flags & TRACE_SHOWFRAME_FLAG)
	    db_printf(" [frame 0x%x]", stack);
    }
    db_printf("\n");
    if (note) {
	db_printf("   %s\n", note);
	note = 0;
    }

    do
    {
	/*
	 * If requested, show preserved registers at the time
	 * the next-shown call was made. Only registers known to have
	 * changed from the last exception frame are shown, as others
	 * can be gotten at by looking at the exception frame.
	 */
	if (trace_flags & TRACE_SHOWCALLPRESERVED_FLAG)
	{
	    int r, title_printed = 0;

	    for (r = FIRST_CALLPRESERVED_REG; r<=LAST_CALLPRESERVED_REG; r++) {
		if (have_global_reg(r)) {
		    unsigned value = saved_reg_value(r);
		    if (title_printed == 0) {
		       title_printed = 1;
		       db_printf("[in next func:");
		    }
		    if (value == 0)
			db_printf(" r%d", r);
		    else if (value <= 9)
			db_printf(" r%d=%x", r, value);
		    else
			db_printf(" r%d=x%x", r, value);
		}
	    }
	    if (title_printed)
		db_printf("]\n");
	}

	db_printf("(%d)%c", depth++, next_address_likely_wrong ? '?':' ');
	next_address_likely_wrong = 0;

	if (trace_flags & TRACE_SHOWADDRESS_FLAG)
	    db_printf("%08x ", where);
	db_printsym(where, DB_STGY_PROC);
	where = stack_decode(where, &stack);
	print_args();
	if (trace_flags & TRACE_SHOWFRAME_FLAG)
	    db_printf(" [frame 0x%x]", stack);
	db_printf("\n");
	if (note) {
	    db_printf("   %s\n", note);
	    note = 0;
	}
    } while (where);

    /* try to trace back over trap/exception */

    stack &= ~7; /* double word aligned */
    /* take last top of stack, and try to find an exception frame near it */

    i = FRAME_PLAY;

    #ifdef TRACE_DEBUG
	if (DEBUGGING_ON)
	    db_printf("(searching for exception frame at 0x%x)\n", stack);
    #endif

    while (i)
    {
	/*
	 * On the stack, a pointer to the exception frame is written
	 * in two adjacent words. In the case of a fault from the kernel,
	 * this should point to the frame right above them:
	 *
	 * Exception Frame Top
	 * ..
	 * Exception Frame Bottom  <-- frame addr
	 * frame addr
	 * frame addr		<-- stack pointer
	 *
	 * In the case of a fault from user mode, the top of stack
	 * will just have the address of the frame
	 * replicated twice.
	 *
	 * frame addr		<-- top of stack
	 * frame addr
	 *
	 * Here we are just looking for kernel exception frames.
	 */

	if (badwordaddr((vm_offset_t)stack) ||
	    badwordaddr((vm_offset_t)(stack+4)))
		    break;

	db_read_bytes((char*)stack, 2*sizeof(int), (char*)pair);

	/* the pairs should match and equal stack+8 */
	if (pair[0] == pair[1])
	{
	    if (pair[0] != stack+8)
	    {
		/*
		if (!badwordaddr((vm_offset_t)pair[0]) && (pair[0]!=0))
		db_printf("stack_trace:found pair 0x%x but != to stack+8\n",
		pair[0]);
		*/
	    }
	    else if (frame_is_sane((db_regs_t*)pair[0]))
	    {
		db_regs_t *frame = (db_regs_t *) pair[0];
		char *cause = m88k_exception_name(frame -> vector);

		db_printf("-------------- %s [EF: 0x%x] -------------\n",
		      cause, frame);
		db_stack_trace_cmd2(frame);
		return;
	    }
	    #ifdef TRACE_DEBUG
		else if (DEBUGGING_ON)
		    db_printf("pair matched, but frame at 0x%x looks insane\n",
			stack+8);
	    #endif
	}
	stack += 8;
	i--;
    }

    /*
     * If we go here, crawling back on the stack failed to find us
     * a previous exception frame. Look for a user frame pointer
     * pointed to by a word 8 bytes off of the top of the stack
     * if the "u" option was specified.
     */
    if (trace_flags & TRACE_USER_FLAG)
    {
	db_regs_t *user;

	/* Make sure we are back on the right page */
	stack -= 4*FRAME_PLAY;
	stack = stack & ~(KERNEL_STACK_SIZE-1); /* point to the bottom */
	stack += KERNEL_STACK_SIZE - 8;

	if (badwordaddr((vm_offset_t)stack) ||
	    badwordaddr((vm_offset_t)stack))
		    return;

	db_read_bytes((char*)stack, 2*sizeof(int), (char*)pair);
	if (pair[0] != pair[1])
	    return;

	/* have a hit */
	user = *((db_regs_t **) stack);

	if (frame_is_sane(user) == 2)
	{
	    db_printf("---------------- %s [EF : 0x%x] -------------\n",
		m88k_exception_name(user->vector), user);
	    db_stack_trace_cmd2(user);
	}
    }
}

/*
 * stack trace - needs a pointer to a m88k saved state.
 *
 * If argument f is given, the stack pointer of each call frame is
 * printed.
 */
void
db_stack_trace_cmd(
    db_regs_t *addr,
    int have_addr,
    db_expr_t count,
    char *modif)
{
    enum { Default, Stack, Proc, Frame } style = Default;
    db_regs_t frame; /* a m88100_saved_state */
    db_regs_t *regs;
    union {
	db_regs_t *frame;
	struct proc *proc;
	unsigned num;
    } arg;
    arg.frame = addr;

    trace_flags = 0; /* flags will be set via modifers */

    while (modif && *modif) {
	 switch (*modif++)
    	 {
	  case 'd':
	    #ifdef TRACE_DEBUG
		trace_flags |= TRACE_DEBUG_FLAG;
	    #else
		db_printtf("<debug trace not compiled in, ignoring>\n");
	    #endif
	    break;

	  case 's': style = Stack  ; break;
	  case 'f': style = Frame  ; break;
	  case 'p': trace_flags |= TRACE_SHOWCALLPRESERVED_FLAG; break;
	  case 'a': trace_flags |= TRACE_SHOWADDRESS_FLAG; break;
	  case 'F': trace_flags |= TRACE_SHOWFRAME_FLAG; break;
	  case 'u': trace_flags |= TRACE_USER_FLAG; break;
	  default:
	    db_printf("unknown trace modifier [%c]\n", modif[-1]);
	    /*FALLTHROUGH*/
	  case 'h':
	    db_printf("usage: trace/[MODIFIER]  [ARG]\n");
	    db_printf("  u = include user trace\n");
	    db_printf("  F = print stack frames\n");
	    db_printf("  a = show return addresses\n");
	    db_printf("  p = show call-preserved registers\n");
	    db_printf("  s = ARG is a stack pointer\n");
	    db_printf("  f = ARG is a frame pointer\n");
	    #ifdef TRACE_DEBUG
		db_printf("  d = trace-debugging output\n");
	    #endif
	    return;
	}
    }

    if (!have_addr && style != Default) {
	db_printf("expecting argument with /s or /f\n");
	return;
    }
    if (have_addr && style == Default)
	style = Proc;

    switch(style)
    {
      case Default:
	regs = DDB_REGS;
	break;

      case Frame:
	regs = arg.frame;
	break;

      case Stack:
      {
	unsigned val1, val2, sxip;
	unsigned ptr;
	bzero((void*)&frame, sizeof(frame));
	#define REASONABLE_FRAME_DISTANCE 2048

	/*
	 * We've got to find the top of a stack frame so we can get both
	 * a PC and and real SP.
	 */
	for (ptr = arg.num;/**/; ptr += 4) {
	    /* Read a word from the named stack */
	    if (db_trace_get_val(ptr, &val1) == 0) {
		db_printf("can't read from %x, aborting.\n", ptr);
		return;
	    }

	    /*
	     * See if it's a frame pointer.... if so it will be larger than
	     * the address it was taken from (i.e. point back up the stack)
	     * and we'll be able to read where it points.
	     */
	    if (val1 <= ptr ||
		(val1 & 3)  ||
		val1 > (ptr + REASONABLE_FRAME_DISTANCE))
		    continue;

	    /* peek at the next word to see if it could be a return address */
	    if (db_trace_get_val(ptr, &sxip) == 0) {
		db_printf("can't read from %x, aborting.\n", ptr);
		return;
	    }
	    if (sxip == 0 || !db_trace_get_val(sxip, &val2))
		continue;

	    if (db_trace_get_val(val1, &val2) == 0) {
		db_printf("can't read from %x, aborting.\n", val1);
		continue;
	    }

	    /*
	     * The value we've just read will be either another frame pointer,
	     * or the start of another exception frame.
	     */
	    if (
		#ifdef JEFF_DEBUG
		    val2 == 0
		#else
		    val2 == 0x12345678
		#endif
	        && db_trace_get_val(val1-4, &val2) && val2 == val1
	        && db_trace_get_val(val1-8, &val2) && val2 == val1)
	    {
		    /* we've found a frame, so the stack must have been good */
		    db_printf("%x looks like a frame, accepting %x\n",val1,ptr);
		    break;
	    }

	    if (val2 > val1 && (val2 & 3) == 0) {
		/* well, looks close enough to be another frame pointer */
		db_printf("*%x = %x looks like a stack frame pointer, accepting %x\n", val1, val2, ptr);
		break;
	    }
	}

	frame.r[31] = ptr;
	frame.epsr = 0x800003f0U;
	frame.sxip = sxip | 2;
	frame.snip = frame.sxip + 4;
	frame.sfip = frame.snip + 4;
db_printf("[r31=%x, sxip=%x]\n", frame.r[31], frame.sxip);
	regs = &frame;
      }
    }

    db_stack_trace_cmd2(regs);
}
