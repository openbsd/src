/*> gencode.c <*/
/* Instruction handling support for the MIPS architecture simulator.

   This file is part of the MIPS sim

		THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

   $Revision: 1.2 $
     $Author: niklas $
       $Date: 1996/11/23 03:55:12 $
*/

#if 0
#define DEBUG (1) /* Just for testing */
#endif

/* The Makefile currently defines "INSIDE_SIMULATOR" as part of the
   build. It is not currently used by the MIPS simulator world
   though. */

/* All output sent to stdout is for the simulator engine. All program
   related warnings and errors should be sent to stderr. */

/* The simulator decode table is constructed this way to allow the
   minimal code required for a particular instruction type to be
   coded.  This avoids a large simulator source file, with lots of
   build-time conditionals controlling what code is included.  However
   this two-stage process does mean that care must be taken to ensure
   that the correct decoding source is generated for a particular MIPS
   simulator. */

/* Notes:

   We could provide pipeline modelling by splitting the simulation of
   instructions into seperate bytecodes for each pipeline
   stage. e.g. for the VR4300 each instruction would generate 5
   bytecodes, one for each pipeline stage. The simulator control would
   then insert these into the relevant pipeline slots, and execute a
   complete slots worth of bytecodes. However, the shape of the
   pipeline, and what parts of each instruction are executed in each
   pipeline stage, are different between MIPS implementations. If we
   were to construct a simulator for a particular MIPS architecture
   this would be a good solution.

   To avoid having to provide multiple different pipeline models, a
   simple approach for dealing with the delay slots, and register
   dependencies has been used. The "MIPS IV Instruction Set" document
   (Revision 3.1 - January 1995) details the standard MIPS instruction
   set, and it defines operations in instruction (not pipe-line)
   cycles. This means we only need to worry about a few cases where
   the result is not available until after the next instruction, or
   where registers in the previous two instruction cycles may be
   corrupted. The case for corruption only occurs with HI or LO
   register access, so we can just keep a count within the engine for
   upto two cycles before marking the register as safe. We then only
   need to check the safety flag when performing an update that
   involves the HI or LO register. The only other case is the
   BC1F/BC1T instructions in the FP unit. For ISAs I, II and III there
   must be an instruction between the FP CMP and the BC1[FT]. We can
   perform the same instruction cycle count scheme, so we can raise a
   warning if an attempt is made to access the condition code early
   (NOTE: The hardware does not interlock on this operation, so the
   simulator should just raise a warning).

   For the situations where a result is not available until later, we
   implent a slot to hold pending values. After the PC is incremented,
   and before the instruction is decoded we can execute the required
   register update (or remainder of instruction processing). */

/* The FP instruction decoding is also provided by this code.  The
   instructions are marked as "FP" ones so that we can construct a
   simulator without an FPU if required.  Similarly we mark
   instructions as Single or Double precision, since some MIPS
   processors only have single precision FP hardware. */

/* NOTE: Ideally all state should be passed as parameters. This allows
   a single simulator engine to be used for multiple concurrent
   simulations. More importantly, if a suitably powerful control is in
   place it will allow speculative simulation, since the context can
   be saved easily, and then restored after performing some
   simulation. The down-side is that for certain host architectures it
   can slow the simulator down (e.g. if globals can be accessed faster
   than local structures). However, this is not actually the case at
   the moment. The constructed engine uses direct names (that can be
   macro definitions). This keeps the engine source smalled (using
   short-hands), and it also allows the user to control whether they
   want to use global, or indirected memory locations. i.e. whether
   they want a single- or multi-threaded simulator engine. */

/* The constructed simulator engine contains manifests for each of the
   features supported. The code that includes the engine can then
   discover the available features during its build. This information
   can be used to control run-time features provided by the final
   simulator. */

/*---------------------------------------------------------------------------*/

/* Program defaults */
#define DEF_ISA      (3)
#define DEF_PROC64   (1 == 1)
#define DEF_FP       (1 == 1)
#define DEF_FPSINGLE (1 == 0)

#define FEATURE_PROC32      (1 << 0)    /* 0 = 64bit; 1 = 32bit */
#define FEATURE_HASFPU      (1 << 1)    /* 0 = no FPU; 1 = include FPU */
#define FEATURE_FPSINGLE    (1 << 1)    /* 0 = double; 1 = single (only used if FEATURE_HASFPU defined) */
#define FEATURE_GP64        (1 << 2)    /* 0 = GPRLEN 32; 1 = GPRLEN 64 */
#define FEATURE_FAST        (1 << 17)   /* 0 = normal;  1 = disable features that slow performance */
#define FEATURE_WARN_STALL  (1 << 24)   /* 0 = nothing; 1 = generate warnings when pipeline would stall */
#define FEATURE_WARN_LOHI   (1 << 25)   /* 0 = nothing; 1 = generate warnings when LO/HI corrupted */
#define FEATURE_WARN_ZERO   (1 << 26)   /* 0 = nothing; 1 = generate warnings if attempt to write register zero */ 
#define FEATURE_WARN_MEM    (1 << 27)   /* 0 = nothing; 1 = generate warnings when memory problems are noticed */
#define FEATURE_WARN_R31    (1 << 28)   /* 0 = nothing; 1 = generate warnings if r31 used dangerously */
#define FEATURE_WARN_RESULT (1 << 29)   /* 0 = nothing; 1 = generate warnings when undefined results may occur */

#if 1
#define FEATURE_WARNINGS  (FEATURE_WARN_STALL | FEATURE_WARN_LOHI | FEATURE_WARN_ZERO | FEATURE_WARN_R31)
#else
#define FEATURE_WARNINGS  (FEATURE_WARN_STALL | FEATURE_WARN_LOHI | FEATURE_WARN_ZERO | FEATURE_WARN_R31 | FEATURE_WARN_RESULT)
#endif

/* FEATURE_WARN_STALL */
/* If MIPS I we want to raise a warning if an attempt is made to
   access Rn in an instruction immediately following an Rn update
   "WARNING : Invalid value read". The simulator engine is designed
   that the previous value is read in such cases, to allow programs
   that make use of this feature to execute. *
/* If MIPS II or later, attempting to read a register before the
   update has completed will generate a "WARNING : Processor stall"
   message (since the processor will lock the pipeline until the value
   becomes available). */

/* FEATURE_WARN_LOHI */
/* Warn if an attempt is made to read the HI/LO registers before the
   update has completed, or if an attempt is made to update the
   registers whilst an update is occurring. */

/* FEATURE_WARN_ZERO */
/* Notify the user if an attempt is made to use GPR 0 as a destination. */

/* FEATURE_WARN_R31 */
/* Notify the user if register r31 (the default procedure call return
   address) is used unwisely. e.g. If r31 is used as the source in a
   branch-and-link instruction, it would mean that an exception in the
   delay slot instruction would not allow the branch to be re-started
   (since r31 will have been overwritten by the link operation during
   the first execution of the branch). */

/* FEATURE_WARN_RESULT */
/* Certain instructions do not raise exceptions when invalid operands
   are given, they will just result in undefined values being
   generated. This option controls whether the simulator flags such
   events. */

/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include "ansidecl.h"
#include "opcode/mips.h"

/* FIXME: ansidecl.h defines AND.  */
#undef AND

#ifndef ULONG_MAX
#define	ULONG_MAX	((unsigned long)(~0L))		/* 0xFFFFFFFF */
#endif

static unsigned long my_strtoul ();

#if 0
#ifndef TRUE
#define TRUE  (1 == 1)
#define FALSE (1 == 0)
#endif
#endif

/*---------------------------------------------------------------------------*/

/* Holding the instruction table this way makes it easier to check the
   instruction values defined, and to add instructions to the
   system. However, it makes the process of constructing the simulator
   a bit more complicated: */

/* The "bitmap" is encoded as follows (NOTE: Only lower-case
   alphabetic characters should be used, since the letter ordinal is
   used as a bit position): */

typedef struct operand_encoding {
 char id; /* character identifier */
 int fpos; /* first bit position */
 int flen; /* field length in bits */
 char * const type;
 char * const name;
 unsigned int flags;
} operand_encoding;

/* Values for the "flags" field: */
#define OP_NONE   (0 << 0)      /* To keep the source tidy */
#define OP_GPR    (1 << 0)      /* Get operand from integer register bank */
#define OP_SIGNX  (1 << 1)      /* Sign-extend the operand */
#define OP_SHIFT2 (1 << 2)      /* Shift field left by 2 */
#define OP_BITS5  (1 << 3)      /* Only take the lo 5-bits of the operand */

struct operand_encoding opfields[] = {
 {'0',-1,-1,"",      "",              (OP_NONE)},  /* special case for explicit zero */
 {'1',-1,-1,"",      "",              (OP_NONE)},  /* special case for explicit one */
 {'?',-1,-1,"",      "",              (OP_NONE)},  /* undefined (do not care at this level) */
 /* The rest are the explicit operand fields: */
 {'a', 6, 5,"int",   "op1",           (OP_NONE)},  /* shift amount (or hint) */
 {'b',21, 5,"int",   "fr",            (OP_NONE)},   /* fr register */
 {'c',16, 1,"int",   "boolean",       (OP_NONE)},  /* TRUE or FALSE boolean */
 {'d',11, 5,"int",   "destreg",       (OP_NONE)},  /* integer destination/rd register */
 {'e', 0,16,"t_reg", "offset",        (OP_SIGNX)}, /* signed offset (lo-3bits must be zero) */
 {'f',17, 1,"int",   "likely",        (OP_NONE)},  /* set if branch LIKELY */
 {'g',16, 5,"t_reg", "op2",           (OP_GPR)},   /* integer source rt register */
 {'h', 0,16,"t_reg", "offset",        (OP_SIGNX)}, /* signed offset (lo-1bit must be zero) */
 {'i', 0,16,"t_reg", "op2",           (OP_SIGNX)}, /* signed immediate (op2) */
 {'j', 0,26,"ut_reg","op1",           (OP_SHIFT2)},/* shifted left 2 bits and combined with hi-order bits of address in the delay slot */
 {'k',16, 5,"int",   "ft",            (OP_NONE)},
 {'l', 0,16,"t_reg", "offset",        (OP_SIGNX | OP_SHIFT2)}, /* signed offset shifted left 2 to make 18bit signed offset */
 {'m',21, 3,"int",   "format",        (OP_NONE)},  /* FP format field */
 {'n',16, 5,"int",   "hint",          (OP_NONE)},  /* hint */
 {'o',21, 5,"t_reg", "op1",           (OP_GPR | OP_BITS5)},  /* integer source/rs register (but never treated as 32bit word) */
 {'p', 8, 3,"int",   "condition_code",(OP_NONE)},  /* FP condition code field */
 {'q',18, 3,"int",   "condition_code",(OP_NONE)},  /* FP condition code field */
 {'r', 6, 5,"int",   "destreg",       (OP_NONE)},  /* FP fd register */
 {'s',21, 5,"t_reg", "op1",           (OP_GPR)},   /* integer source/rs register */
 {'t',16, 5,"int",   "destreg",       (OP_NONE)},  /* integer target rt (destination) register */
 {'u', 0, 4,"int",   "cmpflags",      (OP_NONE)},  /* FP comparison control flags */
 {'v',11, 5,"int",   "fs",            (OP_NONE)},  /* FP fs register (or PREFX hint) */
 {'w', 0,16,"t_reg", "offset",        (OP_SIGNX)}, /* signed offset (lo-2bits must be zero) */
 {'x',23, 1,"int",   "to",            (OP_NONE)},  /* TRUE if move To; FALSE if move From */
 {'y', 0,16,"t_reg", "offset",        (OP_SIGNX)}, /* signed offset */
 {'z', 0,16,"ut_reg","op2",           (OP_NONE)},  /* unsigned immediate (zero extended) */
};

/* Main instruction encoding types: */
typedef enum {
 NORMAL,
 SPECIAL,
 REGIMM,
 COP1,
 COP1X,
 COP1S, /* These instructions live in the reserved FP format values: 0..15,18-19,22-31 */
 UNKNOWN
} inst_type;

/* Main instruction families: */
typedef enum {
 ADD,			/* res = operand1 + operand2 */
 SUB,                   /* res = operand1 - operand2 */
 MUL,                   /* res = operand1 * operand2 */
 DIV,                   /* res = operand1 / operand2 */
 AND,			/* res = operand1 & operand2 */
 OR,			/* res = operand1 | operand2 */
 XOR,			/* res = operand1 ^ operand2 */
 MOVE,                  /* res = operand1 */
 BRANCH,		/* execute delay slot instruction before branch unless (LIKELY && branch_not_taken) */
 JUMP,                  /* execute delay slot instruction before jump */
 LOAD,                  /* load from memory */
 STORE,                 /* store to memory */
 PREFETCH,              /* prefetch data into cache */
 SET,                   /* set register on result of condition code */
 SHIFT,                 /* perform a logical or arithmetic shift */
 TRAP,                  /* system exception generation */
 BREAK,                 /* system breakpoint exception generation */
 SYSCALL,               /* system exception generation */
 SYNC,                  /* system cache control */
 DECODE,                /* co-processor instruction */
 CACHE,                 /* co-processor 0 CACHE instruction */
 MADD16,                /* VR4100 specific multiply-add extensions */
 FPMOVE,
 FPMOVEC,
 FPFLOOR,               
 FPCEIL,
 FPTRUNC,
 FPROUND,
 FPNEG,
 FPABS,
 FPDIV,
 FPMUL,
 FPSUB,
 FPADD,
 FPPREFX,
 FPRECIP,
 FPSQRT,
 FPCONVERT,
 FPCOMPARE,
 RSVD                   /* "Reserved Instruction" on MIPS IV, or if co-proc 3 absent. Otherwise "Reserved Instruction" */
} opcode_type;

/* Flags field: */
#define NONE		(0 << 0)	/* Zero value (used to keep source tidy) */
#define SIM_SH_SIZE     (0)
#define SIM_MASK_SIZE   (0x7)
#define BYTE            (0)     /*  8bit */
#define HALFWORD        (1)     /* 16bit */
#define WORD            (2)     /* 32bit */
#define DOUBLEWORD      (3)     /* 64bit */
#define SINGLE          (4)     /* single precision FP */
#define DOUBLE          (5)     /* double precision FP */

/* Shorthand to get the size field from the flags value: */
#define GETDATASIZE()   ((MIPS_DECODE[loop].flags >> SIM_SH_SIZE) & SIM_MASK_SIZE)

/* The rest are single bit flags: */
#define MULTIPLY        (1 << 3)        /* actually FP multiply ADD/SUB modifier */
#define EQ		(1 << 4)
#define GT		(1 << 5)
#define LT              (1 << 6)
#define NOT		(1 << 7)
#define LIKELY		(1 << 8)
#define SIGNEXTEND	(1 << 9)
#define OVERFLOW	(1 << 10)
#define LINK		(1 << 11)
#define ATOMIC		(1 << 12)
#define SHIFT16		(1 << 13)
#define REG             (1 << 14)
#define LEFT            (1 << 15)       /* Deliberate explicit encodings to allow check for neither, or both */
#define RIGHT           (1 << 16)       /* Mutually exclusive with "LEFT" */
#define LOGICAL         (1 << 17)
#define ARITHMETIC      (1 << 18)
#define UNSIGNED        (1 << 19)
#define HI32            (1 << 20)
#define HI              (1 << 21)       /* accesses or updates the HI register */
#define LO              (1 << 22)       /* accesses or updates the LO register */
#define WORD32		(1 << 23)
#define FP              (1 << 24)       /* Floating Point operation */
#define FIXED           (1 << 25)       /* fixed point arithmetic */
#define COPROC          (1 << 26)
#define INTEGER         (1 << 27)
#define CONDITIONAL     (1 << 28)
#define RECIP           (1 << 29)
#define CONTROL         (1 << 30)
#define NOARG           (1 << 31)       /* Instruction has no (defined) operands */
/* NOTE: We can overload the use of certain of these flags, since not
   all options are applicable to all instruction types. This will free
   up more space for new flags. */

typedef struct instruction {
 char         *name;   /* ASCII mnemonic name */
 unsigned int  isa;    /* MIPS ISA number where instruction introduced */
 char         *bitmap; /* 32character string describing instruction operands */
 inst_type     mark;   /* type of MIPS instruction encoding */
 opcode_type   type;   /* main instruction family */
 unsigned int  flags;  /* flags describing instruction features */
} instruction;
/* The number of pipeline cycles taken by an instruction varies
   between MIPS processors. This means that the information must be
   encoded elsewhere, in a CPU specific structure. */

/* NOTE: Undefined instructions cause "Reserved Instruction"
   exceptions. i.e. if there is no bit-mapping defined then the
   instruction is deemed to be undefined. */

/* NOTE: The "isa" field is also used to encode flags for particular
   chip architecture extensions. e.g. the NEC VR4100 specific
   instructions. Normally chip extensions are added via the COP0
   space. However, the VR4100 (and possibly other devices) also use
   the normal instruction space. */
#define MASK_ISA (0x000000FF) /* Start by leaving 8bits for the ISA ID */
/* The other bits are allocated downwards, to avoid renumbering if we
   have to extend the bits allocated to the pure ISA number. */
#define ARCH_VR4100       ((unsigned)1 << 31) /* NEC VR4100 extension instructions */

/* The HIBERNATE, STANDBY and SUSPEND instructions are encoded in the
   COP0 space. This means that an external decoder should be added
   when constructing a full VR4100 simulator. However some arithmetic
   instructions are encoded in the normal instruction space. */

struct instruction MIPS_DECODE[] = {
 /* The instructions are alphabetical, and not in instruction bit-order: */
 {"ABS",     1,"01000110mmm00000vvvvvrrrrr000101",COP1,   FPABS,    (FP)},
 {"ADD",     1,"000000sssssgggggddddd00000100000",SPECIAL,ADD,      (WORD | WORD32 | OVERFLOW)}, /* rd = rs + rt */
 {"ADD",     1,"01000110mmmkkkkkvvvvvrrrrr000000",COP1,   FPADD,    (FP)},
 {"ADDI",    1,"001000ssssstttttiiiiiiiiiiiiiiii",NORMAL, ADD,      (WORD | WORD32 | OVERFLOW)},
 {"ADDU",    1,"000000sssssgggggddddd00000100001",SPECIAL,ADD,      (WORD | WORD32)}, /* rd = rs + rt */
 {"ADDIU",   1,"001001ssssstttttiiiiiiiiiiiiiiii",NORMAL, ADD,      (WORD | WORD32)},
 {"AND",     1,"000000sssssgggggddddd00000100100",SPECIAL,AND,      (NONE)}, /* rd = rs AND rt */
 {"ANDI",    1,"001100ssssstttttzzzzzzzzzzzzzzzz",NORMAL, AND,      (NONE)},
 {"BC1",     1,"01000101000qqqfcllllllllllllllll",COP1S,  BRANCH,   (FP)},
 {"BEQ",     1,"000100sssssgggggllllllllllllllll",NORMAL, BRANCH,   (EQ)},
 {"BEQL",    2,"010100sssssgggggllllllllllllllll",NORMAL, BRANCH,   (EQ | LIKELY)},
 {"BGEZ",    1,"000001sssss00001llllllllllllllll",REGIMM, BRANCH,   (GT | EQ)},
 {"BGEZAL",  1,"000001sssss10001llllllllllllllll",REGIMM, BRANCH,   (GT | EQ | LINK)},
 {"BGEZALL", 2,"000001sssss10011llllllllllllllll",REGIMM, BRANCH,   (GT | EQ | LINK)},
 {"BGEZL",   2,"000001sssss00011llllllllllllllll",REGIMM, BRANCH,   (GT | EQ | LIKELY)},
 {"BGTZ",    1,"000111sssss00000llllllllllllllll",NORMAL, BRANCH,   (GT)},
 {"BGTZL",   2,"010111sssss00000llllllllllllllll",NORMAL, BRANCH,   (GT | LIKELY)},
 {"BLEZ",    1,"000110sssss00000llllllllllllllll",NORMAL, BRANCH,   (LT | EQ)},
 {"BLEZL",   2,"010110sssss00000llllllllllllllll",NORMAL, BRANCH,   (LT | EQ | LIKELY)},
 {"BLTZ",    1,"000001sssss00000llllllllllllllll",REGIMM, BRANCH,   (LT)},
 {"BLTZAL",  1,"000001sssss10000llllllllllllllll",REGIMM, BRANCH,   (LT | LINK)},
 {"BLTZALL", 2,"000001sssss10010llllllllllllllll",REGIMM, BRANCH,   (LT | LINK | LIKELY)},
 {"BLTZL",   2,"000001sssss00010llllllllllllllll",REGIMM, BRANCH,   (LT | LIKELY)},
 {"BNE",     1,"000101sssssgggggllllllllllllllll",NORMAL, BRANCH,   (NOT | EQ)},
 {"BNEL",    2,"010101sssssgggggllllllllllllllll",NORMAL, BRANCH,   (NOT | EQ | LIKELY)},
 {"BREAK",   1,"000000????????????????????001101",SPECIAL,BREAK,    (NOARG)},
 {"CEIL.L",  3,"01000110mmm00000vvvvvrrrrr001010",COP1,   FPCEIL,   (FP | FIXED | DOUBLEWORD)},
 {"CEIL.W",  2,"01000110mmm00000vvvvvrrrrr001110",COP1,   FPCEIL,   (FP | FIXED | WORD)},
 {"COP0",    1,"010000??????????????????????????",NORMAL, DECODE,   (NOARG)},
 {"COP2",    1,"010010??????????????????????????",NORMAL, DECODE,   (NOARG)},
 {"CVT.D",   1,"01000110mmm00000vvvvvrrrrr100001",COP1,   FPCONVERT,(FP | DOUBLE)},
 {"CVT.L",   3,"01000110mmm00000vvvvvrrrrr100101",COP1,   FPCONVERT,(FP | FIXED | DOUBLEWORD)},
 {"CVT.S",   1,"01000110mmm00000vvvvvrrrrr100000",COP1,   FPCONVERT,(FP | SINGLE)},
 {"CVT.W",   1,"01000110mmm00000vvvvvrrrrr100100",COP1,   FPCONVERT,(FP | FIXED | WORD)},
 {"C.%s",    1,"01000110mmmkkkkkvvvvvppp0011uuuu",COP1,   FPCOMPARE,(FP)},
 {"CxC1",    1,"01000100x10kkkkkvvvvv00000000000",COP1S,  FPMOVEC,  (FP | WORD | CONTROL)},
 {"DADD",    3,"000000sssssgggggddddd00000101100",SPECIAL,ADD,      (DOUBLEWORD | OVERFLOW)},
 {"DADDI",   3,"011000ssssstttttiiiiiiiiiiiiiiii",NORMAL, ADD,      (DOUBLEWORD | OVERFLOW)},
 {"DADDU",   3,"000000sssssgggggddddd00000101101",SPECIAL,ADD,      (DOUBLEWORD | UNSIGNED)},
 {"DADDIU",  3,"011001ssssstttttiiiiiiiiiiiiiiii",NORMAL, ADD,      (DOUBLEWORD | UNSIGNED)},
 {"DDIV",    3,"000000sssssggggg0000000000011110",SPECIAL,DIV,      (DOUBLEWORD | HI | LO)},
 {"DDIVU",   3,"000000sssssggggg0000000000011111",SPECIAL,DIV,      (DOUBLEWORD | UNSIGNED | HI | LO)},
 {"DIV",     1,"000000sssssggggg0000000000011010",SPECIAL,DIV,      (WORD | WORD32 | SIGNEXTEND | HI | LO)},
 {"DIV",     1,"01000110mmmkkkkkvvvvvrrrrr000011",COP1,   FPDIV,    (FP | WORD | HI | LO)},
 {"DIVU",    1,"000000sssssggggg0000000000011011",SPECIAL,DIV,      (WORD | WORD32 | UNSIGNED | SIGNEXTEND | HI | LO)},
 {"DMADD16", (ARCH_VR4100 | 3),"000000sssssggggg0000000000101001",SPECIAL,MADD16,   (DOUBLEWORD | HI | LO)},
 {"DMULT",   3,"000000sssssggggg0000000000011100",SPECIAL,MUL,      (DOUBLEWORD | HI | LO)},
 {"DMULTU",  3,"000000sssssggggg0000000000011101",SPECIAL,MUL,      (DOUBLEWORD | UNSIGNED | HI | LO)},
 {"DMxC1",   3,"01000100x01kkkkkvvvvv00000000000",COP1S,  FPMOVEC,  (FP | DOUBLEWORD)},
 {"DSLL",    3,"00000000000gggggdddddaaaaa111000",SPECIAL,SHIFT,    (DOUBLEWORD | LEFT | LOGICAL)},
 {"DSLLV",   3,"000000sssssgggggddddd00000010100",SPECIAL,SHIFT,    (DOUBLEWORD | LEFT | LOGICAL)},
 {"DSLL32",  3,"00000000000gggggdddddaaaaa111100",SPECIAL,SHIFT,    (DOUBLEWORD | LEFT | LOGICAL | HI32)}, /* rd = rt << (sa + 32) */
 {"DSRA",    3,"00000000000gggggdddddaaaaa111011",SPECIAL,SHIFT,    (DOUBLEWORD | RIGHT | ARITHMETIC)},
 {"DSRAV",   3,"000000sssssgggggddddd00000010111",SPECIAL,SHIFT,    (DOUBLEWORD | RIGHT | ARITHMETIC)},
 {"DSRA32",  3,"00000000000gggggdddddaaaaa111111",SPECIAL,SHIFT,    (DOUBLEWORD | RIGHT | ARITHMETIC | HI32)}, /* rd = rt >> (sa + 32) */
 {"DSRL",    3,"00000000000gggggdddddaaaaa111010",SPECIAL,SHIFT,    (DOUBLEWORD | RIGHT | LOGICAL)},
 {"DSRLV",   3,"000000sssssgggggddddd00000010110",SPECIAL,SHIFT,    (DOUBLEWORD | RIGHT | LOGICAL)},
 {"DSRL32",  3,"00000000000gggggdddddaaaaa111110",SPECIAL,SHIFT,    (DOUBLEWORD | RIGHT | LOGICAL | HI32)},
 {"DSUB",    3,"000000sssssgggggddddd00000101110",SPECIAL,SUB,      (DOUBLEWORD)},
 {"DSUBU",   3,"000000sssssgggggddddd00000101111",SPECIAL,SUB,      (DOUBLEWORD | UNSIGNED)},
 {"FLOOR.L", 3,"01000110mmm00000vvvvvrrrrr001011",COP1,   FPFLOOR,  (FP | FIXED | DOUBLEWORD)},
 {"FLOOR.W", 2,"01000110mmm00000vvvvvrrrrr001111",COP1,   FPFLOOR,  (FP | FIXED | WORD)},
 {"J",       1,"000010jjjjjjjjjjjjjjjjjjjjjjjjjj",NORMAL, JUMP,     (NONE)},			/* NOTE: boundary case due to delay slot address being used */
 {"JAL",     1,"000011jjjjjjjjjjjjjjjjjjjjjjjjjj",NORMAL, JUMP,     (LINK)},			/* NOTE: boundary case due to delay slot address being used */
 {"JALR",    1,"000000sssss00000ddddd00000001001",SPECIAL,JUMP,     (LINK | REG)},
 {"JR",      1,"000000sssss000000000000000001000",SPECIAL,JUMP,     (NONE)}, /* need to check PC as part of instruction fetch */
 {"LB",      1,"100000ssssstttttyyyyyyyyyyyyyyyy",NORMAL, LOAD,     (BYTE | SIGNEXTEND)},	/* NOTE: "i" rather than "o" because BYTE addressing is allowed */
 {"LBU",     1,"100100ssssstttttyyyyyyyyyyyyyyyy",NORMAL, LOAD,     (BYTE)},	                /* NOTE: See "LB" comment */
 {"LD",      3,"110111sssssttttteeeeeeeeeeeeeeee",NORMAL, LOAD,     (DOUBLEWORD)},
 {"LDC1",    2,"110101sssssttttteeeeeeeeeeeeeeee",NORMAL, LOAD,     (DOUBLEWORD | COPROC)},
 {"LDC2",    2,"110110sssssttttteeeeeeeeeeeeeeee",NORMAL, LOAD,     (DOUBLEWORD | COPROC)},
 {"LDL",     3,"011010ssssstttttyyyyyyyyyyyyyyyy",NORMAL, LOAD,     (DOUBLEWORD | LEFT)},	/* NOTE: See "LB" comment */
 {"LDR",     3,"011011ssssstttttyyyyyyyyyyyyyyyy",NORMAL, LOAD,     (DOUBLEWORD | RIGHT)},	/* NOTE: See "LB" comment */
 {"LDXC1",   4,"010011sssssggggg00000rrrrr000001",COP1X,  LOAD,     (FP | DOUBLEWORD | COPROC | REG)},
 {"LH",      1,"100001sssssttttthhhhhhhhhhhhhhhh",NORMAL, LOAD,     (HALFWORD | SIGNEXTEND)},
 {"LHU",     1,"100101sssssttttthhhhhhhhhhhhhhhh",NORMAL, LOAD,     (HALFWORD)},
 {"LL",      2,"110000ssssstttttwwwwwwwwwwwwwwww",NORMAL, LOAD,     (WORD | ATOMIC | SIGNEXTEND)},
 {"LLD",     3,"110100sssssttttteeeeeeeeeeeeeeee",NORMAL, LOAD,     (DOUBLEWORD | ATOMIC)},
 {"LUI",     1,"00111100000tttttiiiiiiiiiiiiiiii",NORMAL, MOVE,     (SHIFT16)}, /* Cheat and specify sign-extension of immediate field */
 {"LW",      1,"100011ssssstttttwwwwwwwwwwwwwwww",NORMAL, LOAD,     (WORD | SIGNEXTEND)},
 {"LWC1",    1,"110001ssssstttttwwwwwwwwwwwwwwww",NORMAL, LOAD,     (WORD | COPROC)},
 {"LWC2",    1,"110010ssssstttttwwwwwwwwwwwwwwww",NORMAL, LOAD,     (WORD | COPROC)},
 {"LWL",     1,"100010ssssstttttyyyyyyyyyyyyyyyy",NORMAL, LOAD,     (WORD | LEFT)},
 {"LWR",     1,"100110ssssstttttyyyyyyyyyyyyyyyy",NORMAL, LOAD,     (WORD | RIGHT)},
 {"LWU",     3,"100111ssssstttttwwwwwwwwwwwwwwww",NORMAL, LOAD,     (WORD)},
 {"LWXC1",   4,"010011sssssggggg00000rrrrr000000",COP1X,  LOAD,     (FP | WORD | COPROC | REG)},
 {"MADD16",  (ARCH_VR4100 | 3),"000000sssssggggg0000000000101000",SPECIAL,MADD16,   (WORD | HI | LO)},
 {"MADD.D",  4,"010011bbbbbkkkkkvvvvvrrrrr100001",COP1X,  FPADD,    (FP | MULTIPLY | DOUBLE)},
 {"MADD.S",  4,"010011bbbbbkkkkkvvvvvrrrrr100000",COP1X,  FPADD,    (FP | MULTIPLY | SINGLE)},
 {"MFHI",    1,"0000000000000000ddddd00000010000",SPECIAL,MOVE,     (HI)},    /* with following, from and to denoted by usage of "s" or "d" */
 {"MFLO",    1,"0000000000000000ddddd00000010010",SPECIAL,MOVE,     (LO)},
 {"MTHI",    1,"000000sssss000000000000000010001",SPECIAL,MOVE,     (HI)},
 {"MTLO",    1,"000000sssss000000000000000010011",SPECIAL,MOVE,     (LO)},
 {"MOV",     1,"01000110mmm00000vvvvvrrrrr000110",COP1,   FPMOVE,   (FP)},
 {"MOVN",    4,"000000sssssgggggddddd00000001011",SPECIAL,MOVE,     (NOT | EQ)},
 {"MOVN",    4,"01000110mmmgggggvvvvvrrrrr010011",COP1,   FPMOVE,   (FP | NOT | EQ)},
 {"MOV%c",   4,"000000sssssqqq0cddddd00000000001",SPECIAL,FPMOVE,   (FP | CONDITIONAL | INTEGER)},
 {"MOV%c",   4,"01000110mmmqqq0cvvvvvrrrrr010001",COP1,   FPMOVE,   (FP | CONDITIONAL)},
 {"MOVZ",    4,"000000sssssgggggddddd00000001010",SPECIAL,MOVE,     (EQ)},
 {"MOVZ",    4,"01000110mmmgggggvvvvvrrrrr010010",COP1,   FPMOVE,   (FP | EQ)},
 {"MSUB.D",  4,"010011bbbbbkkkkkvvvvvrrrrr101001",COP1X,  FPSUB,    (FP | MULTIPLY | DOUBLE)},
 {"MSUB.S",  4,"010011bbbbbkkkkkvvvvvrrrrr101000",COP1X,  FPSUB,    (FP | MULTIPLY | SINGLE)},
 {"MUL",     1,"01000110mmmkkkkkvvvvvrrrrr000010",COP1,   FPMUL,    (FP | HI | LO)},
 {"MULT",    1,"000000sssssggggg0000000000011000",SPECIAL,MUL,      (WORD | WORD32 | HI | LO)},
 {"MULTU",   1,"000000sssssggggg0000000000011001",SPECIAL,MUL,      (WORD | WORD32 | UNSIGNED | HI | LO)},
 {"MxC1",    1,"01000100x00kkkkkvvvvv00000000000",COP1S,  FPMOVEC,  (FP | WORD)},
 {"NEG",     1,"01000110mmm00000vvvvvrrrrr000111",COP1,   FPNEG,    (FP)},
 {"NMADD.D", 4,"010011bbbbbkkkkkvvvvvrrrrr110001",COP1X,  FPADD,    (FP | NOT | MULTIPLY | DOUBLE)},
 {"NMADD.S", 4,"010011bbbbbkkkkkvvvvvrrrrr110000",COP1X,  FPADD,    (FP | NOT | MULTIPLY | SINGLE)},
 {"NMSUB.D", 4,"010011bbbbbkkkkkvvvvvrrrrr111001",COP1X,  FPSUB,    (FP | NOT | MULTIPLY | DOUBLE)},
 {"NMSUB.S", 4,"010011bbbbbkkkkkvvvvvrrrrr111000",COP1X,  FPSUB,    (FP | NOT | MULTIPLY | SINGLE)},
 {"NOR",     1,"000000sssssgggggddddd00000100111",SPECIAL,OR,       (NOT)},
 {"OR",      1,"000000sssssgggggddddd00000100101",SPECIAL,OR,       (NONE)},
 {"ORI",     1,"001101ssssstttttzzzzzzzzzzzzzzzz",NORMAL, OR,       (NONE)},
 {"PREF",    4,"110011sssssnnnnnyyyyyyyyyyyyyyyy",NORMAL, PREFETCH, (NONE)},
 {"PREFX",   4,"010011sssssgggggvvvvv00000001111",COP1X,  FPPREFX,  (FP)},
 {"RECIP",   4,"01000110mmm00000vvvvvrrrrr010101",COP1,   FPRECIP,  (FP)},
 {"ROUND.L", 3,"01000110mmm00000vvvvvrrrrr001000",COP1,   FPROUND,  (FP | FIXED | DOUBLEWORD)},
 {"ROUND.W", 2,"01000110mmm00000vvvvvrrrrr001100",COP1,   FPROUND,  (FP | FIXED | WORD)},
 {"RSQRT",   4,"01000110mmm00000vvvvvrrrrr010110",COP1,   FPSQRT,   (FP | RECIP)},
 {"SB",      1,"101000sssssgggggyyyyyyyyyyyyyyyy",NORMAL, STORE,    (BYTE)},
 {"SC",      2,"111000sssssgggggwwwwwwwwwwwwwwww",NORMAL, STORE,    (WORD | ATOMIC)},
 {"SCD",     3,"111100sssssgggggeeeeeeeeeeeeeeee",NORMAL, STORE,    (DOUBLEWORD | ATOMIC)},
 {"SD",      3,"111111sssssgggggeeeeeeeeeeeeeeee",NORMAL, STORE,    (DOUBLEWORD)},
 {"SDC1",    2,"111101sssssttttteeeeeeeeeeeeeeee",NORMAL, STORE,    (DOUBLEWORD | COPROC)},
 {"SDC2",    2,"111110sssssttttteeeeeeeeeeeeeeee",NORMAL, STORE,    (DOUBLEWORD | COPROC)},
 {"SDL",     3,"101100sssssgggggyyyyyyyyyyyyyyyy",NORMAL, STORE,    (DOUBLEWORD | LEFT)},
 {"SDR",     3,"101101sssssgggggyyyyyyyyyyyyyyyy",NORMAL, STORE,    (DOUBLEWORD | RIGHT)},
 {"SDXC1",   4,"010011sssssgggggvvvvv00000001001",COP1X,  STORE,    (FP | DOUBLEWORD | COPROC | REG)},
 {"SH",      1,"101001sssssggggghhhhhhhhhhhhhhhh",NORMAL, STORE,    (HALFWORD)},
 {"SLL",     1,"00000000000gggggdddddaaaaa000000",SPECIAL,SHIFT,    (WORD | LEFT | LOGICAL)}, /* rd = rt << sa */
 {"SLLV",    1,"000000ooooogggggddddd00000000100",SPECIAL,SHIFT,    (WORD | LEFT | LOGICAL)}, /* rd = rt << rs - with "SLL" depends on "s" and "a" field values */
 {"SLT",     1,"000000sssssgggggddddd00000101010",SPECIAL,SET,      (LT)},
 {"SLTI",    1,"001010ssssstttttiiiiiiiiiiiiiiii",NORMAL, SET,      (LT)},
 {"SLTU",    1,"000000sssssgggggddddd00000101011",SPECIAL,SET,      (LT | UNSIGNED)},
 {"SLTIU",   1,"001011ssssstttttiiiiiiiiiiiiiiii",NORMAL, SET,      (LT | UNSIGNED)},
 {"SQRT",    2,"01000110mmm00000vvvvvrrrrr000100",COP1,   FPSQRT,   (FP)},
 {"SRA",     1,"00000000000gggggdddddaaaaa000011",SPECIAL,SHIFT,    (WORD | WORD32 | RIGHT | ARITHMETIC)},
 {"SRAV",    1,"000000ooooogggggddddd00000000111",SPECIAL,SHIFT,    (WORD | WORD32 | RIGHT | ARITHMETIC)},
 {"SRL",     1,"00000000000gggggdddddaaaaa000010",SPECIAL,SHIFT,    (WORD | WORD32 | RIGHT | LOGICAL)},
 {"SRLV",    1,"000000ooooogggggddddd00000000110",SPECIAL,SHIFT,    (WORD | WORD32 | RIGHT | LOGICAL)},
 {"SUB",     1,"000000sssssgggggddddd00000100010",SPECIAL,SUB,      (WORD | WORD32 | OVERFLOW)},
 {"SUB",     1,"01000110mmmkkkkkvvvvvrrrrr000001",COP1,   FPSUB,    (FP)},
 {"SUBU",    1,"000000sssssgggggddddd00000100011",SPECIAL,SUB,      (WORD | WORD32)},
 {"SW",      1,"101011sssssgggggwwwwwwwwwwwwwwww",NORMAL, STORE,    (WORD)},
 {"SWC1",    1,"111001ssssstttttwwwwwwwwwwwwwwww",NORMAL, STORE,    (WORD | COPROC)},
 {"SWC2",    1,"111010ssssstttttwwwwwwwwwwwwwwww",NORMAL, STORE,    (WORD | COPROC)},
 {"SWL",     1,"101010sssssgggggyyyyyyyyyyyyyyyy",NORMAL, STORE,    (WORD | LEFT)},
 {"SWR",     1,"101110sssssgggggyyyyyyyyyyyyyyyy",NORMAL, STORE,    (WORD | RIGHT)},
 {"SWXC1",   4,"010011sssssgggggvvvvv00000001000",COP1X,  STORE,    (FP | WORD | COPROC | REG)},
 {"SYNC",    2,"000000000000000000000aaaaa001111",SPECIAL,SYNC,     (NONE)}, /* z = 5bit stype field */
 {"SYSCALL", 1,"000000????????????????????001100",SPECIAL,SYSCALL,  (NOARG)},
 {"TEQ",     2,"000000sssssggggg??????????110100",SPECIAL,TRAP,     (EQ)},
 {"TEQI",    2,"000001sssss01100iiiiiiiiiiiiiiii",REGIMM, TRAP,     (EQ)},
 {"TGE",     2,"000000sssssggggg??????????110000",SPECIAL,TRAP,     (GT | EQ)},
 {"TGEI",    2,"000001sssss01000iiiiiiiiiiiiiiii",REGIMM, TRAP,     (GT | EQ)},
 {"TGEIU",   2,"000001sssss01001iiiiiiiiiiiiiiii",REGIMM, TRAP,     (GT | EQ | UNSIGNED)},
 {"TGEU",    2,"000000sssssggggg??????????110001",SPECIAL,TRAP,     (GT | EQ | UNSIGNED)},
 {"TLT",     2,"000000sssssggggg??????????110010",SPECIAL,TRAP,     (LT)},
 {"TLTI",    2,"000001sssss01010iiiiiiiiiiiiiiii",REGIMM, TRAP,     (LT)},
 {"TLTIU",   2,"000001sssss01011iiiiiiiiiiiiiiii",REGIMM, TRAP,     (LT | UNSIGNED)},
 {"TLTU",    2,"000000sssssggggg??????????110011",SPECIAL,TRAP,     (LT | UNSIGNED)},
 {"TNE",     2,"000000sssssggggg??????????110110",SPECIAL,TRAP,     (NOT | EQ)},
 {"TNEI",    2,"000001sssss01110iiiiiiiiiiiiiiii",REGIMM, TRAP,     (NOT | EQ)},
 {"TRUNC.L", 3,"01000110mmm00000vvvvvrrrrr001001",COP1,   FPTRUNC,  (FP | FIXED | DOUBLEWORD)},
 {"TRUNC.W", 2,"01000110mmm00000vvvvvrrrrr001101",COP1,   FPTRUNC,  (FP | FIXED | WORD)},
 {"XOR",     1,"000000sssssgggggddddd00000100110",SPECIAL,XOR,      (NONE)},
 {"XORI",    1,"001110ssssstttttzzzzzzzzzzzzzzzz",NORMAL, XOR,      (NONE)},
 {"CACHE",   3,"101111sssssnnnnnyyyyyyyyyyyyyyyy",NORMAL, CACHE,    (NONE)},
 {"<INT>",   1,"111011sssssgggggyyyyyyyyyyyyyyyy",NORMAL, RSVD,     (NONE)},
};

/*---------------------------------------------------------------------------*/

/* We use the letter ordinal as the bit-position in our flags field: */
#define fieldval(l)     (1 << ((l) - 'a'))

unsigned int
convert_bitmap(bitmap,onemask,zeromask,dontmask)
     char *bitmap;
     unsigned int *onemask, *zeromask, *dontmask;
{
  unsigned int flags = 0x00000000;
  int loop; /* current bitmap position */
  int lastsp = -1; /* last bitmap field starting position */
  int lastoe = -1; /* last bitmap field encoding */

  *onemask = 0x00000000;
  *zeromask = 0x00000000;
  *dontmask = 0x00000000;

  if (strlen(bitmap) != 32) {
    fprintf(stderr,"Invalid bitmap string - not 32 characters long \"%s\"\n",bitmap);
    exit(3);
  }

  for (loop = 0; (loop < 32); loop++) {
    int oefield ;
    for (oefield = 0; (oefield < (sizeof(opfields) / sizeof(struct operand_encoding))); oefield++)
     if (bitmap[31 - loop] == opfields[oefield].id)
      break;
    if (oefield < (sizeof(opfields) / sizeof(struct operand_encoding))) {
      if ((lastoe != -1) && (lastoe != oefield))
       if ((loop - lastsp) != (opfields[lastoe].flen)) {
         fprintf(stderr,"Invalid field length %d for bitmap field '%c' (0x%02X) (should be %d) : bitmap = \"%s\"\n",(loop - lastsp),(((bitmap[31 - loop] < 0x20) || (bitmap[31 - loop] >= 0x7F)) ? '.' : bitmap[31 - loop]),bitmap[31 - loop],opfields[lastoe].flen,bitmap);
         exit(4);
       }
         
      switch (bitmap[31 - loop]) {
        case '0' : /* fixed value */
         *zeromask |= (1 << loop);
         lastsp = loop;
         lastoe = -1;
         break;

        case '1' : /* fixed value */
         *onemask |= (1 << loop);
         lastsp = loop;
         lastoe = -1;
         break;

        case '?' : /* fixed value */
         *dontmask |= (1 << loop);
         lastsp = loop;
         lastoe = -1;
         break;

        default : /* check character encoding */
         {
           if (opfields[oefield].fpos != -1) {
             /* If flag not set, then check starting position: */
             if (!(flags & fieldval(bitmap[31 - loop]))) {
               if (loop != opfields[oefield].fpos) {
                 fprintf(stderr,"Bitmap field '%c' (0x%02X) at wrong offset %d in bitmap \"%s\"\n",(((bitmap[31 - loop] < 0x20) || (bitmap[31 - loop] >= 0x7F)) ? '.' : bitmap[31 - loop]),bitmap[31 - loop],loop,bitmap);
                 exit(4);
               }
               flags |= fieldval(bitmap[31 - loop]);
               lastsp = loop;
               lastoe = oefield;
             }
           }
           *dontmask |= (1 << loop);
         }
         break;
       }
    } else {
      fprintf(stderr,"Unrecognised bitmap character '%c' (0x%02X) at offset %d in bitmap \"%s\"\n",(((bitmap[31 - loop] < 0x20) || (bitmap[31 - loop] >= 0x7F)) ? '.' : bitmap[31 - loop]),bitmap[31 - loop],loop,bitmap);
      exit(4);
    }
  }

 /* NOTE: Since we check for the position and size of fields when
    parsing the "bitmap" above, we do *NOT* need to check that invalid
    field combinations have been used. */

 return(flags);
}

/*---------------------------------------------------------------------------*/

static void
build_operands(flags)
     unsigned int flags;
{
  int loop;
  for (loop = 0; (loop < (sizeof(opfields) / sizeof(operand_encoding))); loop++)
   if ((opfields[loop].fpos != -1) && (flags & fieldval(opfields[loop].id))) {
     printf("  %s %s = ",opfields[loop].type,opfields[loop].name);

     if (opfields[loop].flags & OP_SIGNX)
      printf("SIGNEXTEND((%s)",opfields[loop].type);

     if (opfields[loop].flags & OP_GPR)
      printf("GPR[");

     if (opfields[loop].flags & OP_SHIFT2)
      printf("(");

     printf("((instruction >> %d) & 0x%08X)",opfields[loop].fpos,((1 << opfields[loop].flen) - 1));

     if (opfields[loop].flags & OP_SHIFT2)
      printf(" << 2)");

     if (opfields[loop].flags & OP_GPR)
      printf("]");

     if (opfields[loop].flags & OP_BITS5)
      printf("&0x1F");

     if (opfields[loop].flags & OP_SIGNX)
      printf(",%d)",(opfields[loop].flen + ((opfields[loop].flags & OP_SHIFT2) ? 2 : 0)));

     printf(";\n");
   }

 return;
}

/*---------------------------------------------------------------------------*/

typedef enum {
  s_left,
  s_right
} e_endshift;

static void
build_endian_shift(proc64,datalen,endbit,direction,shift)
     int proc64;
     int datalen;
     int endbit;
     e_endshift direction;
     int shift;
{
  if (proc64 && (datalen == 4)) {
    printf("    if ((vaddr & (1 << %d)) ^ (BigEndianCPU << %d)) {\n",endbit,endbit);
    printf("     memval %s= %d;\n",direction == s_left ? "<<" : ">>",shift);
    printf("    }\n");
  }

  return;
}

/*---------------------------------------------------------------------------*/
/* doisa = number of MIPS ISA simulator table is being constructed for.
 * proc64 = TRUE if constructing 64bit processor world.
 * dofp = boolean, TRUE if FP instructions are to be included.
 * fpsingle = boolean, TRUE if only single precision FP instructions to be included.
 */

void
process_instructions(doarch,features)
     unsigned int doarch;
     unsigned int features;
{
  int doisa = (doarch & MASK_ISA);
  int limit = (sizeof(MIPS_DECODE) / sizeof(instruction));
  int gprlen=((features & FEATURE_GP64) ? 64 : 32);
  int proc64 = ((features & FEATURE_PROC32) ? 0 : -1);
  int dofp = (features & FEATURE_HASFPU);
  int fpsingle = (features & FEATURE_FPSINGLE);
  int maxisa;
  int loop;

  if (limit < 1) {
    fprintf(stderr,"process_instructions: invalid structure length\n");
    exit(1);
  }

  if (proc64 && (gprlen != 64)) {
    fprintf(stderr,"Error: 64bit processor build specified, with MIPS ISA I or II\n");
    exit(3);
  }

  /* NOTE: "proc64" also differentiates between 32- and 64-bit wide memory */

  maxisa = 0;
  for (loop = 0; (loop < limit); loop++)
   if ((MIPS_DECODE[loop].isa & MASK_ISA) > maxisa)
    maxisa = (MIPS_DECODE[loop].isa & MASK_ISA);

  if (doisa == 0)
   doisa = maxisa;

  printf("#if defined(SIM_MANIFESTS)\n");
  printf("#define MIPSISA (%d)\n",doisa);
  if (proc64)
   printf("#define PROCESSOR_64BIT (1 == 1)\n");
  else
   printf("#define PROCESSOR_64BIT (1 == 0)\n");
#if 1 /* cheat: We only have a 64bit LoadMemory and StoreMemory routines at the moment */
  printf("#define LOADDRMASK (0x%08X)\n",0x7);
#else
  printf("#define LOADDRMASK (0x%08X)\n",(proc64 ? 0x7 : 0x3));
#endif
  /* The FP registers are the same width as the CPU registers: */
  printf("#define GPRLEN (%d)\n",gprlen);
  printf("typedef %s t_reg;\n",((gprlen == 64) ? "word64" : "int"));
  printf("typedef %s ut_reg;\n",((gprlen == 64) ? "uword64" : "unsigned int"));
  printf("typedef %s t_fpreg;\n",((gprlen == 64) ? "word64" : "int"));
  if (dofp)
   printf("#define HASFPU (1 == 1)\n");
  if (features & FEATURE_FAST)
   printf("#define FASTSIM (1 == 1)\n");
  if (features & FEATURE_WARN_STALL)
   printf("#define WARN_STALL (1 == 1)\n");
  if (features & FEATURE_WARN_LOHI)
   printf("#define WARN_LOHI (1 == 1)\n");
  if (features & FEATURE_WARN_ZERO)
   printf("#define WARN_ZERO (1 == 1)\n");
  if (features & FEATURE_WARN_MEM)
   printf("#define WARN_MEM (1 == 1)\n");
  if (features & FEATURE_WARN_R31)
   printf("#define WARN_R31 (1 == 1)\n");
  if (features & FEATURE_WARN_RESULT)
   printf("#define WARN_RESULT (1 == 1)\n");

  printf("#else /* simulator engine */\n");

  printf("/* Engine generated by \"%s\" at %s */\n","<SHOW PROGRAM ARGS>","<SHOW CURRENT DATE AND TIME>");
  printf("/* Main instruction decode for %d-bit MIPS ISA %d (Table entry limit = %d) */\n",(proc64 ? 64 : 32),doisa,limit);
  if (dofp)
   printf("/* %sFP instructions included */\n",(fpsingle ? "Single precision " : ""));
  printf("/* NOTE: \"DSPC\" is the delay slot PC address */\n");

 if (proc64) {
   printf("#if !defined(PROCESSOR_64BIT)\n");
   printf("#error \"Automatically constructed decoder has been built for a 64bit processor\"\n");
   printf("#endif\n");
 }

 printf("/* Actual instruction decoding block */\n");
 printf("{\n");
  {
    int limit;
    printf("int num = ((instruction >> %d) & 0x%08X);\n",OP_SH_OP,OP_MASK_OP);
    limit = (OP_MASK_OP + 1);
    printf("if (num == 0x00) num = (%d + ((instruction >> %d) & 0x%08X));\n",limit,OP_SH_SPEC,OP_MASK_SPEC);
    limit += (OP_MASK_SPEC + 1);
    printf("else if (num == 0x01) num = (%d + ((instruction >> %d) & 0x%08X));\n",limit,OP_SH_RT,OP_MASK_RT);
    limit += (OP_MASK_RT + 1);
    printf("else if (num == 0x11) {\n");
    printf(" if ((instruction & (0x%08X << %d)) == 0x%08X)\n",OP_MASK_COP1NORM,OP_SH_COP1NORM,(OP_MASK_COP1NORM << OP_SH_COP1NORM));
    printf("  if ((instruction & (0x%08X << %d)) == 0x%08X)\n",OP_MASK_COP1CMP,OP_SH_COP1CMP,(OP_MASK_COP1CMP << OP_SH_COP1CMP));
    printf("   num = (%d + ((instruction >> %d) & 0x%08X));\n",limit,OP_SH_SPEC,(OP_MASK_SPEC & (OP_MASK_COP1CMP << OP_SH_COP1CMP)));
    printf("  else\n");
    printf("   num = (%d + ((instruction >> %d) & 0x%08X));\n",limit,OP_SH_SPEC,OP_MASK_SPEC);
    limit += (OP_MASK_SPEC + 1);
    printf(" else\n");
    /* To keep this code quick, we just clear out the "to" bit
       here. The proper (though slower) code would be to have another
       conditional, checking whether this instruction is a branch or
       not, before limiting the range to the bottom two bits of the
       move operation. */
    printf("  num = (%d + (((instruction >> %d) & 0x%08X) & ~0x%08X));\n",limit,OP_SH_COP1SPEC,OP_MASK_COP1SPEC,OP_MASK_COP1SCLR);
    limit += (OP_MASK_COP1SPEC + 1);
    printf("} else if (num == 0x13) num = (%d + ((instruction >> %d) & 0x%08X));\n",limit,OP_SH_SPEC,OP_MASK_SPEC);
    limit += (OP_MASK_SPEC + 1);
    printf("/* Total possible switch entries: %d */\n",limit) ;
  }
 printf("switch (num)\n") ;
 printf("{\n");

 for (loop = 0; (loop < limit); loop++) {
   /* First check that the ISA number we are constructing for is
      valid, before checking if the instruction matches any of the
      architecture specific flags. NOTE: We allow a selected ISA of
      zero to be used to match all standard instructions. */
   if ((((MIPS_DECODE[loop].isa & MASK_ISA) <= doisa) && (((MIPS_DECODE[loop].isa & ~MASK_ISA) == 0) || ((MIPS_DECODE[loop].isa & ~MASK_ISA) & doarch) != 0)) && (!(MIPS_DECODE[loop].flags & FP) || ((MIPS_DECODE[loop].flags & FP) && dofp))) {
     unsigned int onemask;
     unsigned int zeromask;
     unsigned int dontmask;
     unsigned int mask;
     unsigned int number;
     unsigned int flags = convert_bitmap(MIPS_DECODE[loop].bitmap,&onemask,&zeromask,&dontmask);
     char *regtype = ((gprlen == 64) ? "uword64" : "unsigned int");

     if (!(MIPS_DECODE[loop].flags & COPROC) && ((GETDATASIZE() == DOUBLEWORD) && !proc64)) {
       fprintf(stderr,"DOUBLEWORD width specified for non 64-bit processor for instruction \"%s\"\n",MIPS_DECODE[loop].name);
       exit(4);
     }

#if defined(DEBUG)
     printf("/* DEBUG: onemask  0x%08X */\n",onemask) ;
     printf("/* DEBUG: zeromask 0x%08X */\n",zeromask) ;
     printf("/* DEBUG: dontmask 0x%08X */\n",dontmask) ;
#endif

     switch (MIPS_DECODE[loop].mark) {
       case NORMAL :
        mask = (OP_MASK_OP << OP_SH_OP) ;
        number = ((onemask >> OP_SH_OP) & OP_MASK_OP) ;
        break ;

       case SPECIAL :
        mask = ((OP_MASK_OP << OP_SH_OP) | (OP_MASK_SPEC << OP_SH_SPEC)) ;
        number = ((OP_MASK_OP + 1) + ((onemask >> OP_SH_SPEC) & OP_MASK_SPEC)) ;
        break ;

       case REGIMM :
        mask = ((OP_MASK_OP << OP_SH_OP) | (OP_MASK_RT << OP_SH_RT)) ;
        number = (((OP_MASK_OP + 1) + (OP_MASK_SPEC + 1)) + ((onemask >> OP_SH_RT) & OP_MASK_RT)) ;
        break ;

       case COP1 :
        mask = ((OP_MASK_OP << OP_SH_OP) | (OP_MASK_SPEC << OP_SH_SPEC)) ;
        number = (((OP_MASK_OP + 1) + (OP_MASK_SPEC + 1) + (OP_MASK_RT + 1)) + ((onemask >> OP_SH_SPEC) & OP_MASK_SPEC)) ;
        break ;

       case COP1S :
        mask = ((OP_MASK_OP << OP_SH_OP) | (OP_MASK_COP1SPEC << OP_SH_COP1SPEC)) ;
        number = (((OP_MASK_OP + 1) + (OP_MASK_SPEC + 1) + (OP_MASK_RT + 1) + (OP_MASK_SPEC + 1)) + ((onemask >> OP_SH_COP1SPEC) & OP_MASK_COP1SPEC)) ;
        break;

       case COP1X :
        mask = ((OP_MASK_OP << OP_SH_OP) | (OP_MASK_SPEC << OP_SH_SPEC)) ;
        number = (((OP_MASK_OP + 1) + (OP_MASK_SPEC + 1) + (OP_MASK_RT + 1) + (OP_MASK_COP1SPEC + 1) + (OP_MASK_SPEC + 1)) + ((onemask >> OP_SH_SPEC) & OP_MASK_SPEC)) ;
        break ;

       default :
        fprintf(stderr,"Unrecognised opcode mark %d in table slot %d \"%s\"\n",MIPS_DECODE[loop].mark,loop,MIPS_DECODE[loop].name) ;
        exit(5) ;
      }

     printf("case %d : /* \"%s\" %s */\n",number,MIPS_DECODE[loop].name,MIPS_DECODE[loop].bitmap) ;

#if defined(DEBUG)
     printf("/* DEBUG: mask 0x%08X */\n",mask) ;
     printf(" printf(\"\\\"%s\\\"\\n\");\n",MIPS_DECODE[loop].name);
#endif

     /* Check if there are any other explicit bits in the instruction: */
     if ((~mask & (onemask | zeromask)) != 0x00000000) {
       printf(" if ((instruction & 0x%08X) != 0x%08X)\n",(onemask | zeromask),onemask) ;
       printf(" {\n") ;
       printf("  SignalException(ReservedInstruction,instruction);\n") ;
       printf(" }\n") ;
       printf(" else\n") ;
     }

     if ((flags == 0) && !(MIPS_DECODE[loop].flags & NOARG)) {
       fprintf(stderr,"Bitmap error: Instruction with no operand fields \"%s\"\n",MIPS_DECODE[loop].name) ;
       exit(5) ;
     }

     printf(" {\n") ;

     /* Get hold of the operands */
     /* NOTE: If we wanted to make the simulator code smaller, we
      * could pull these into a common sequence before we perform
      * the instruction decoding. However, this would affect the
      * performance since unnecessary field extraction would be
      * occurring for certain instructions.
      *
      * Also we do not perform checking for multiple definitions of a
      * particular operand here, since they are caught by the
      * compilation of the produced code.
      */
     build_operands(flags);

     /* Finish constructing the jump address if required: */
     if (flags & fieldval('j'))
      printf("  op1 |= (PC & ~0x0FFFFFFF); /* address of instruction in delay slot for the jump */\n");

     /* Now perform required operand checks: */

/* The following code has been removed, since it seems perfectly
   reasonable to have a non-aligned offset that is added to another
   non-aligned base to create an aligned address. Some more
   information on exactly what the MIPS IV specification requires is
   needed before deciding on the best strategy. Experimentation with a
   VR4300 suggests that we do not need to raise the warning. */
#if 0
     /* For MIPS IV (and onwards), certain instruction operand values
        will give undefined results. For the simulator we could
        generate explicit exceptions (i.e. ReservedInstruction) to
        make it easier to spot invalid use. However, for the moment we
        just raise a warning. NOTE: This is a different check to the
        later decoding, which checks for the final address being
        valid. */
     if ((flags & (fieldval('e') | fieldval('w') | fieldval('h'))) && (doisa >= 4)) {
       printf("  if (instruction & 0x%1X)\n",((flags & fieldval('e')) ? 0x7 : ((flags & fieldval('w')) ? 0x3 : 0x1)));
       printf("  {\n");
       /* NOTE: If we change this to a SignalException(), we must
          ensure that the following opcode processing is not
          executed. i.e. the code falls straight out to the simulator
          control loop. */
       printf("   sim_warning(\"Instruction has lo-order offset bits set in instruction\");\n");
       printf("  }\n");
     }
#endif

     /* The extended condition codes only appeared in ISA IV */
     if ((flags & fieldval('p')) && (doisa < 4)) {
       printf("  if (condition_code != 0)\n");
       printf("  {\n");
       printf("   SignalException(ReservedInstruction,instruction);\n");
       printf("  }\n");
       printf("  else\n");
     }

     if ((MIPS_DECODE[loop].flags & WORD32) && (GETDATASIZE() != WORD)) {
       fprintf(stderr,"Error in opcode table: WORD32 set for non-WORD opcode\n");
       exit(1);
     }

#if 1
     /* The R4000 book differs slightly from the MIPS IV ISA
        manual. An example is the sign-extension of a 64-bit processor
        SUBU operation, and what is meant by an Undefined Result. This
        is now provided purely as a warning. After examining a HW
        implementation, this is now purely a warning... and the actual
        operation is performed, with possibly undefined results. */
     if (((MIPS_DECODE[loop].flags & WORD32) && proc64) && (features & FEATURE_WARN_RESULT)) {
       /* The compiler should optimise out an OR with zero */
       printf("  if (%s | %s)\n",((flags & fieldval('s')) ? "NOTWORDVALUE(op1)" : "0"),((flags & fieldval('g')) ? "NOTWORDVALUE(op2)" : "0"));
       printf("   UndefinedResult();\n") ;
     }
#else
     /* Check that the source is a 32bit value */
     if ((MIPS_DECODE[loop].flags & WORD32) && proc64) {
       /* The compiler should optimise out an OR with zero */
       printf("  if (%s | %s)\n",((flags & fieldval('s')) ? "NOTWORDVALUE(op1)" : "0"),((flags & fieldval('g')) ? "NOTWORDVALUE(op2)" : "0"));
       printf("   UndefinedResult();\n") ;
       printf("  else\n") ;
     }
#endif

     printf("  {\n") ;

     switch (MIPS_DECODE[loop].type) {
       /* TODO: To make these easier to edit and maintain, they should
          actually be provided as source macros (or inline functions)
          OUTSIDE this main switch statement. The PPC simulator has a
          neater scheme for describing the instruction sequences. */

       case ADD:
       case SUB:
        {
          char *signed_basetype = "unknown";
          char *unsigned_basetype = "unknown";

          switch (GETDATASIZE()) {
            case WORD :
             signed_basetype = "signed int";
             unsigned_basetype = "unsigned int";
             break;
            case DOUBLEWORD :
             signed_basetype = "word64";
             unsigned_basetype = "uword64";
             break;
            default :
             fprintf(stderr,"Opcode table error: size of ADD/SUB operands not known (%d)\n",GETDATASIZE());
             exit(1);
          }

          if ((MIPS_DECODE[loop].type) == ADD) {
            printf("   %s temp = (%s)(op1 + op2);\n", unsigned_basetype, unsigned_basetype);
            printf("   %s tempS = (%s)temp;\n", signed_basetype, signed_basetype);
            if (MIPS_DECODE[loop].flags & OVERFLOW) {
              printf("   if (((op1 < 0) == (op2 < 0)) && ((tempS < 0) != (op1 < 0)))\n");
              printf("    SignalException(IntegerOverflow);\n");
              printf("   else\n");
            }
            if (!proc64 || (MIPS_DECODE[loop].flags & UNSIGNED) || (GETDATASIZE() == DOUBLEWORD))
             printf("   GPR[destreg] = (%s)temp;\n",regtype);
            else /* only sign-extend when placing 32bit result in 64bit processor */
             printf("   GPR[destreg] = SIGNEXTEND(((%s)temp),32);\n",regtype);
          } else { /* SUB */
            printf("   %s temp = (%s)(op1 - op2);\n", unsigned_basetype, unsigned_basetype);
            printf("   %s tempS = (%s)temp;\n", signed_basetype, signed_basetype);
            if (MIPS_DECODE[loop].flags & OVERFLOW) { /* different signs => overflow if result_sign != arg_sign */
              printf("   if (((op1 < 0) != (op2 < 0)) && ((tempS < 0) == (op1 < 0)))\n");
              printf("    SignalException(IntegerOverflow);\n");
              printf("   else\n");
            }
            /* UNSIGNED 32bit operations on a 64bit processor should
               *STILL* be sign-extended. We have cheated in the
               data-structure, by not marking it with UNSIGNED, and not
               setting OVERFLOW. */
            if (!proc64 || (MIPS_DECODE[loop].flags & UNSIGNED) || (GETDATASIZE() == DOUBLEWORD))
             printf("   GPR[destreg] = (%s)temp;\n",regtype);
            else /* only sign-extend when placing 32bit result in 64bit processor */
             printf("   GPR[destreg] = SIGNEXTEND(((%s)temp),32);\n",regtype);
          }
        }
        break ;

       case MUL:
        if (features & FEATURE_WARN_LOHI) {
          printf("   CHECKHILO(\"Multiplication\");\n");
        }
        printf("   {\n");
        if (GETDATASIZE() == DOUBLEWORD) {
          printf("   uword64 mid;\n");
	  printf("   uword64 midhi;\n");
          printf("   uword64 temp;\n");
	  if ((MIPS_DECODE[loop].flags & UNSIGNED) == 0)
	    {
	      printf("   int sign = 0;\n");
	      printf("   if (op1 < 0) { op1 = - op1; ++sign; }\n");
	      printf("   if (op2 < 0) { op2 = - op2; ++sign; }\n");
	    }
          printf("   LO = ((uword64)WORD64LO(op1) * WORD64LO(op2));\n");
          printf("   HI = ((uword64)WORD64HI(op1) * WORD64HI(op2));\n");
          printf("   mid = ((uword64)WORD64HI(op1) * WORD64LO(op2));\n");
	  printf("   midhi = SET64HI(WORD64LO(mid));\n");
          printf("   temp = (LO + midhi);\n");
          printf("   if ((temp == midhi) ? (LO != 0) : (temp < midhi))\n");
          printf("    HI += 1;\n");
          printf("   HI += WORD64HI(mid);\n");
          printf("   mid = ((uword64)WORD64LO(op1) * WORD64HI(op2));\n");
	  printf("   midhi = SET64HI(WORD64LO(mid));\n");
          printf("   LO = (temp + midhi);\n");
          printf("   if ((LO == midhi) ? (temp != 0) : (LO < midhi))\n");
          printf("    HI += 1;\n");
          printf("   HI += WORD64HI(mid);\n");
	  if ((MIPS_DECODE[loop].flags & UNSIGNED) == 0)
	    printf("   if (sign & 1) { LO = - LO; HI = (LO == 0 ? 0 : -1) - HI; }\n");
        } else {
	  if (MIPS_DECODE[loop].flags & UNSIGNED)
	    printf("   uword64 temp = ((uword64)(op1 & 0xffffffff) * (uword64)(op2 & 0xffffffff));\n");
	  else
	    printf("   uword64 temp = (op1 * op2);\n");
          printf("   LO = SIGNEXTEND((%s)WORD64LO(temp),32);\n",regtype);
          printf("   HI = SIGNEXTEND((%s)WORD64HI(temp),32);\n",regtype);
        }
        printf("   }\n");
        break ;

       case DIV:
        {
         int boolU = (MIPS_DECODE[loop].flags & UNSIGNED);

         if (features & FEATURE_WARN_LOHI) {
           printf("   CHECKHILO(\"Division\");\n");
         }
         printf("   {\n");
         if (GETDATASIZE() == DOUBLEWORD) {
           printf("   LO = ((%sword64)op1 / (%sword64)op2);\n",(boolU ? "u" : ""),(boolU ? "u" : ""));
           printf("   HI = ((%sword64)op1 %c (%sword64)op2);\n",(boolU ? "u" : ""),'%',(boolU ? "u" : ""));
         } else {
           printf("   LO = SIGNEXTEND(((%sint)op1 / (%sint)op2),32);\n",(boolU ? "unsigned " : ""),(boolU ? "unsigned " : ""));
           printf("   HI = SIGNEXTEND(((%sint)op1 %c (%sint)op2),32);\n",(boolU ? "unsigned " : ""),'%',(boolU ? "unsigned " : ""));
         }
         printf("   }\n");
        }
        break ;

       case SHIFT:
        {
         int datalen = GETDATASIZE();
         int bits = ((datalen == WORD) ? 32 : 64);
         char *ltype = ((datalen == WORD) ? "unsigned int" : "uword64");

         /* Check that the specified SHIFT is valid: */
         if ((datalen == BYTE) || (datalen == HALFWORD)) {
           fprintf(stderr,"Shift \"%s\" specified with BYTE or HALFWORD\n",MIPS_DECODE[loop].name);
           exit(9);
         }
         if ((MIPS_DECODE[loop].flags & LEFT) && (MIPS_DECODE[loop].flags & RIGHT)) {
           fprintf(stderr,"Shift \"%s\" specified with both LEFT and RIGHT\n",MIPS_DECODE[loop].name);
           exit(9);
         }
         if (!(MIPS_DECODE[loop].flags & LEFT) && !(MIPS_DECODE[loop].flags & RIGHT)) {
           fprintf(stderr,"Shift \"%s\" specified with neither LEFT or RIGHT\n",MIPS_DECODE[loop].name);
           exit(9);
         }
         if ((MIPS_DECODE[loop].flags & LOGICAL) && (MIPS_DECODE[loop].flags & ARITHMETIC)) {
           fprintf(stderr,"Shift \"%s\" specified with both LOGICAL and ARITHMETIC\n",MIPS_DECODE[loop].name);
           exit(9);
         }
         if (!(MIPS_DECODE[loop].flags & LOGICAL) && !(MIPS_DECODE[loop].flags & ARITHMETIC)) {
           fprintf(stderr,"Shift \"%s\" specified with neither LOGICAL or ARITHMETIC\n",MIPS_DECODE[loop].name);
           exit(9);
         }
         if ((MIPS_DECODE[loop].flags & LEFT) && (MIPS_DECODE[loop].flags & ARITHMETIC)) {
           fprintf(stderr,"Arithmetic LEFT shift \"%s\" specified\n",MIPS_DECODE[loop].name);
           exit(9);
         }

         /* If register specified shift, then extract the relevant shift amount: */
         if (flags & fieldval('s'))
          printf("   op1 &= 0x%02X;\n",(bits - 1));

         /* If HI32 specified, then shift range is 32..63 */
         if (MIPS_DECODE[loop].flags & HI32)
          printf("   op1 |= (1 << 5);\n");

         /* We do not need to perform pre-masking with 0xFFFFFFFF when
            dealing with 32bit shift lefts, since the sign-extension
            code will replace any remaining hi-bits: */
         if (MIPS_DECODE[loop].flags & LEFT)
          printf("   GPR[destreg] = ((uword64)op2 << op1);\n");
         else
          printf("   GPR[destreg] = ((uword64)(op2%s) >> op1);\n",((bits == 32) ? " & 0xFFFFFFFF" : ""));

         /* For ARITHMETIC shifts, we must duplicate the sign-bit.  We
            don't do this if op1 is zero, since it is not needed and
            since that would cause an undefined shift of the number of
            bits in the type.  */
         if (MIPS_DECODE[loop].flags & ARITHMETIC)
          printf("   GPR[destreg] |= (op1 != 0 && (op2 & ((%s)1 << %d)) ? ((((%s)1 << op1) - 1) << (%d - op1)) : 0);\n",ltype,(bits - 1),ltype,bits);

         /* Ensure WORD values are sign-extended into 64bit registers */
         if ((bits == 32) && (gprlen == 64))
          printf("   GPR[destreg] = SIGNEXTEND(GPR[destreg],%d);\n",bits);
        }
        break ;

       case MOVE:
        if (MIPS_DECODE[loop].flags & (HI | LO)) {
          char *regname = ((MIPS_DECODE[loop].flags & LO) ? "LO" : "HI");
          if (flags & fieldval('d'))
           printf("   GPR[destreg] = %s;\n",regname);
          else {
            if (features & FEATURE_WARN_LOHI) {
              printf("   if (%sACCESS != 0)\n",regname);
              printf("     sim_warning(\"MT (move-to) over-writing %s register value\");\n",regname);
            }
            printf("   %s = op1;\n",regname);
          }
          if (features & FEATURE_WARN_LOHI)
           printf("   %sACCESS = 3; /* 3rd instruction will be safe */\n",regname);
        } else
         if (MIPS_DECODE[loop].flags & SHIFT16)
          printf("   GPR[destreg] = (op2 << 16);\n");
         else {
           /* perform conditional move */
           if (!(MIPS_DECODE[loop].flags & EQ)) {
             fprintf(stderr,"Standard conditional %s does not have the equality flag\n",MIPS_DECODE[loop].name);
             exit(8);
           }
           printf("   if (op2 %c= 0)\n",((MIPS_DECODE[loop].flags & NOT) ? '!' : '='));
           printf("    GPR[destreg] = op1;\n");
         }
        break ;

       case SYNC:
        printf("   SyncOperation(op1);\n");
        break ;

       case SYSCALL:
        printf("   SignalException(SystemCall,instruction);\n");
        break ;

       case BREAK:
        printf("   SignalException(BreakPoint,instruction);\n");
        break ;

       case TRAP:
        {
         int boolNOT = (MIPS_DECODE[loop].flags & NOT);
         int boolEQ  = (MIPS_DECODE[loop].flags & EQ);
         int boolGT  = (MIPS_DECODE[loop].flags & GT);
         int boolLT  = (MIPS_DECODE[loop].flags & LT);
         int boolU   = (MIPS_DECODE[loop].flags & UNSIGNED);

         if (boolGT && boolLT) {
           fprintf(stderr,"GT and LT specified for \"%s\"\n",MIPS_DECODE[loop].name);
           exit(8);
         }

         if (boolNOT && (boolGT || boolLT)) {
           fprintf(stderr,"NOT specified with GT or LT specified for \"%s\"\n",MIPS_DECODE[loop].name);
           exit(8);
         }

         printf("  if ((%sword64)op1 ",(boolU ? "u" : ""));
         printf("%c%s",(boolNOT ? '!' : (boolLT ? '<' : (boolGT ? '>' : '='))),(boolEQ ? "=" : ""));
         printf(" (%sword64)op2)\n",(boolU ? "u" : ""));
         printf("   SignalException(Trap,instruction);\n");
        }
        break ;

       case SET:
        {
         int boolU = (MIPS_DECODE[loop].flags & UNSIGNED);

         if (!(MIPS_DECODE[loop].flags & LT)) {
           fprintf(stderr,"Set instruction without LT specified \"%s\"\n",MIPS_DECODE[loop].name);
           exit(8);
         }

         printf("   if ((%sword64)op1 < (%sword64)op2)\n",(boolU ? "u" : ""),(boolU ? "u" : ""));
         printf("    GPR[destreg] = 1;\n");
         printf("   else\n");
         printf("    GPR[destreg] = 0;\n");
        }
	break ;

       case AND:
        printf("   GPR[destreg] = (op1 & op2);\n");
  	break ;

       case OR:
        printf("   GPR[destreg] = %s(op1 | op2);\n",((MIPS_DECODE[loop].flags & NOT) ? "~" : ""));
	break ;

       case XOR:
        printf("   GPR[destreg] = (op1 ^ op2);\n");
	break ;

       case DECODE:
        printf("   decode_coproc(instruction);\n");
        break ;

       case CACHE:
        /* 16-bit offset is sign-extended and added to the base register to make a virtual address */
        /* The virtual address is translated to a physical address using the TLB */
        /* The hint specifies a cache operation for that address */
        printf("    uword64 vaddr = (op1 + offset);\n");
        printf("    uword64 paddr;\n");
        printf("    int uncached;\n");
        /* NOTE: We are assuming that the AddressTranslation is a load: */
        printf("    if (AddressTranslation(vaddr,isDATA,isLOAD,&paddr,&uncached,isTARGET,isREAL))\n");
        printf("      CacheOp(hint,vaddr,paddr,instruction);\n");
        break;

       case MADD16: /* VR4100 specific multiply-add instructions */
        /* Some of this code is shared with the standard multiply
           routines, so an effort should be made to merge where
           possible. */
        if (features & FEATURE_WARN_LOHI) {
          printf("   CHECKHILO(\"Multiply-Add\");\n");
        }
        if (features & FEATURE_WARN_RESULT) {
          /* Give user a warning if either op1 or op2 are not 16bit signed integers */
          printf("   if (NOTHALFWORDVALUE(op1) || NOTHALFWORDVALUE(op2))\n");
          printf("     sim_warning(\"MADD16 operation with non-16bit operands\");\n");
        }
        printf("   {\n");
        printf("    uword64 temp = (op1 * op2);\n"); /* 16x16 multiply */
        if (GETDATASIZE() == DOUBLEWORD) {
          printf("   LO = LO + temp;\n");
        } else { /* WORD */
          printf("   temp += (SET64HI(WORD64LO(HI)) | WORD64LO(LO));\n");
          printf("   LO = SIGNEXTEND((%s)WORD64LO(temp),32);\n",regtype);
          printf("   HI = SIGNEXTEND((%s)WORD64HI(temp),32);\n",regtype);
        }
        printf("   }\n");
        break;

       case RSVD: /* "Reserved Instruction" on MIPS IV, or if co-proc 3 absent. Otherwise "CoProcessorUnusable" */
        if (doisa < 4) {
          printf("   if (CoProcPresent(3))\n");
          printf("    SignalException(CoProcessorUnusable);\n");
          printf("   else\n");
        }
        printf("   SignalException(ReservedInstruction,instruction);\n");
  	break ;

       case JUMP:
        if (MIPS_DECODE[loop].flags & LINK) {
          if (!(MIPS_DECODE[loop].flags & REG))
           printf("   int destreg = 31;\n");
          printf("   GPR[destreg] = (PC + 4); /* NOTE: The PC is already 4 ahead within the simulator */\n");
        }

        printf("   /* NOTE: The jump occurs AFTER the next instruction has been executed */\n");
        printf("   DSPC = op1;\n");
        printf("   DELAYSLOT();\n");
	break ;

       case BRANCH: /* execute delay slot instruction before branch unless (LIKELY && branch_not_taken) */
        if (MIPS_DECODE[loop].flags & FP) {
          if (doisa < 4) {
            printf("  if (condition_code != 0)\n");
            printf("   SignalException(ReservedInstruction,instruction);\n");
            printf("  else {\n");
          }
          /* "PREVCOC1()" should be the COC1 value at the start of the preceding instruction */
          printf("   int condition = (%s == boolean);\n",((doisa < 4) ? "PREVCOC1()" : "GETFCC(condition_code)"));
        } else {
          if ((MIPS_DECODE[loop].flags & NOT) && !(MIPS_DECODE[loop].flags & EQ)) {
            fprintf(stderr,"NOT specified when not EQ in \"%s\"\n",MIPS_DECODE[loop].name);
            exit(7);
          }
          if ((MIPS_DECODE[loop].flags & NOT) && (MIPS_DECODE[loop].flags & (GT | LT))) {
            fprintf(stderr,"NOT specified with GT or LT in \"%s\"\n",MIPS_DECODE[loop].name);
            exit(7);
          }            
          /* GT  LT */
          if (MIPS_DECODE[loop].flags & GT)
           printf("   int condition = (op1 >%s 0);\n",((MIPS_DECODE[loop].flags & EQ) ? "=" : ""));
          else
           if (MIPS_DECODE[loop].flags & LT)
            printf("   int condition = (op1 <%s 0);\n",((MIPS_DECODE[loop].flags & EQ) ? "=" : ""));
           else
            if (MIPS_DECODE[loop].flags & EQ)
             printf("   int condition = (op1 %c= op2);\n",((MIPS_DECODE[loop].flags & NOT) ? '!' : '='));
        }

        if (MIPS_DECODE[loop].flags & LINK) {
          if (features & FEATURE_WARN_R31) {
            printf("   if (((instruction >> %d) & 0x%08X) == 31)\n",OP_SH_RS,OP_MASK_RS);
            printf("    sim_warning(\"Branch with link using r31 as source operand\");\n");
          }
          printf("   GPR[31] = (PC + 4); /* NOTE: PC is already 8 ahead */\n");
        }

        printf("   /* NOTE: The branch occurs AFTER the next instruction has been executed */\n");
        printf("   if (condition) {\n");
        printf("    DSPC = (PC + offset);\n");
        printf("    DELAYSLOT();\n");
        printf("   }\n");
        if ((MIPS_DECODE[loop].flags & FP) && (doisa != 1)) {
          printf("   else if (likely) {\n");
          printf("    NULLIFY();\n");
          printf("   }\n");
        } else if (MIPS_DECODE[loop].flags & LIKELY) {
          printf("   else\n");
          printf("    NULLIFY();\n");
        }
        if ((MIPS_DECODE[loop].flags & FP) && (doisa < 4))
         printf("   }\n");
	break ;

       case PREFETCH: /* The beginning is shared with normal load operations */
       case LOAD:
       case STORE:
        {
         int isload = ((MIPS_DECODE[loop].type == LOAD) || (MIPS_DECODE[loop].type == PREFETCH));
         int datalen;
         char *accesslength = "<UNKNOWN>";

         switch (GETDATASIZE()) {
           case BYTE :
            datalen = 1;
            accesslength = "AccessLength_BYTE";
            break ;

           case HALFWORD :
            datalen = 2;
            accesslength = "AccessLength_HALFWORD";
            break ;

           case WORD :
            datalen = 4;
            accesslength = "AccessLength_WORD";
            break ;

           case DOUBLEWORD :
            datalen = 8;
            accesslength = "AccessLength_DOUBLEWORD";
            break ;
         }

         if (MIPS_DECODE[loop].flags & REG)
          printf("   uword64 vaddr = ((uword64)op1 + op2);\n");
         else
          printf("   uword64 vaddr = ((uword64)op1 + offset);\n");
         printf("   uword64 paddr;\n");
         printf("   int uncached;\n");

         /* The following check should only occur on normal (non-shifted) memory loads */
         if ((datalen != 1) && !(MIPS_DECODE[loop].flags & (LEFT | RIGHT))) {
           printf("   if ((vaddr & %d) != 0)\n",(datalen - 1));
           printf("    SignalException(%s);\n",(isload ? "AddressLoad" : "AddressStore"));
           printf("   else\n") ;
         }

         printf("   {\n");
         printf("    if (AddressTranslation(vaddr,isDATA,%s,&paddr,&uncached,isTARGET,isREAL))\n",(isload ? "isLOAD" : "isSTORE"));

         if (MIPS_DECODE[loop].type == PREFETCH)
          printf("     Prefetch(uncached,paddr,vaddr,isDATA,hint);\n");
         else {
           printf("    {\n");
           printf("     %s memval;\n",(proc64 ? "uword64" : "unsigned int"));

           if ((MIPS_DECODE[loop].flags & COPROC) && ((datalen != 4) && (datalen != 8))) {
             fprintf(stderr,"Co-processor transfer operation not WORD or DOUBLEWORD in length \"%s\"\n",MIPS_DECODE[loop].name);
             exit(6);
           }

           if (MIPS_DECODE[loop].flags & (LEFT | RIGHT)) {
             if ((MIPS_DECODE[loop].flags & LEFT) && (MIPS_DECODE[loop].flags & RIGHT)) {
               fprintf(stderr,"Memory transfer with both LEFT and RIGHT specified \"%s\"\n",MIPS_DECODE[loop].name);
               exit(4);
             }

             switch (datalen) {
              case 8:
               if (!proc64) {
                 fprintf(stderr,"DOUBLEWORD shifted memory transfers only valid for 64-bit processors \"%s\"\n",MIPS_DECODE[loop].name);
                 exit(4);
               }
               /* fall through to... */
              case 4:
               {
                 char *maskstr = ((datalen == 8) ? "((uword64)-1)" : "0xFFFFFFFF");

                 printf("     uword64 mask = %d;\n",((datalen == 8) ? 0x7 : 0x3));
                 printf("     unsigned int reverse = (ReverseEndian ? mask : 0);\n");
                 printf("     unsigned int bigend = (BigEndianCPU ? mask : 0);\n");
                 printf("     int byte;\n");
                 printf("     paddr = ((paddr & ~mask) | ((paddr & mask) ^ reverse));\n");
                 printf("     byte = ((vaddr & mask) ^ bigend);\n");
                 printf("     if (%sBigEndianMem)\n",((MIPS_DECODE[loop].flags & LEFT) ? "!" : ""));
                 printf("      paddr &= ~mask;\n");

                 if (isload) {
                   if (MIPS_DECODE[loop].flags & LEFT)
                    printf("     memval = LoadMemory(uncached,byte,paddr,vaddr,isDATA,isREAL);\n");
                   else
                    printf("     memval = LoadMemory(uncached,(%d - byte),paddr,vaddr,isDATA,isREAL);\n",(datalen - 1));
                 }

                 if (MIPS_DECODE[loop].flags & LEFT) {
                   if (isload) {
                     /* For WORD transfers work out if the value will
                        be in the top or bottom of the DOUBLEWORD
                        returned: */
#if 1
                     build_endian_shift(proc64,datalen,2,s_right,32);
#else
                     if (proc64 && (datalen == 4)) {
                       printf("     if ((vaddr & (1 << 2)) ^ (BigEndianCPU << 2)) {\n");
                       printf("      memval >>= 32;\n");
                       printf("     }\n");
                     }
#endif
                     printf("     GPR[destreg] = ((memval << ((%d - byte) * 8)) | (GPR[destreg] & (((uword64)1 << ((%d - byte) * 8)) - 1)));\n",(datalen - 1),(datalen - 1));
                     if (proc64 && (datalen == 4))
                      printf("     GPR[destreg] = SIGNEXTEND(GPR[destreg],32);\n");
                   } else { /* store */
                     printf("     memval = (op2 >> (8 * (%d - byte)));\n",(datalen - 1));
#if 1
                     build_endian_shift(proc64,datalen,2,s_left,32);
#else
                     /* TODO: This is duplicated in the LOAD code
                        above - and the RIGHT LOAD and STORE code
                        below. It should be merged if possible. */
                     if (proc64 && (datalen == 4)) {
                       printf("    if ((vaddr & (1 << 2)) ^ (BigEndianCPU << 2)) {\n");
                       printf("     memval <<= 32;\n");
                       printf("    }\n");
                     }
#endif
                     printf("     StoreMemory(uncached,byte,memval,paddr,vaddr,isREAL);\n");
                   }
                 } else { /* RIGHT */
                   if (isload) {
#if 1
                     build_endian_shift(proc64,datalen,2,s_right,32);
#else
                     if (proc64 && (datalen == 4)) {
                       printf("     if ((vaddr & (1 << 2)) ^ (BigEndianCPU << 2)) {\n");
                       printf("      memval >>= 32;\n");
                       printf("     }\n");
                     }
#endif
                     printf("     {\n");
                     printf("      uword64 srcmask;\n");
                     /* All of this extra code is just a bodge
                        required because some hosts don't allow
                        ((v) << 64). The SPARC just leaves the (v)
                        value un-touched. */
                     printf("      if (byte == 0)\n");
                     printf("       srcmask = 0;\n");
                     printf("      else\n");
                     printf("       srcmask = ((uword64)-1 << (8 * (%d - byte)));\n",datalen);
                     printf("      GPR[destreg] = ((GPR[destreg] & srcmask) | (memval >> (8 * byte)));\n",datalen);
                     printf("     }\n");
                     if (proc64 && (datalen == 4))
                      printf("     GPR[destreg] = SIGNEXTEND(GPR[destreg],32);\n");
                   } else { /* store */
                     printf("     memval = (op2 << (byte * 8));\n");
		     build_endian_shift(proc64,datalen,2,s_left,32);
                     printf("     StoreMemory(uncached,(%s - byte),memval,paddr,vaddr,isREAL);\n",accesslength);
                   }
                 }
               }
               break;

              default:
               fprintf(stderr,"Shifted memory transfer not WORD or DOUBLEWORD in length \"%s\"\n",MIPS_DECODE[loop].name);
               exit(6);
             }
           } else { /* normal memory transfer */
             if (!(MIPS_DECODE[loop].flags & COPROC) && ((datalen == 8) || ((datalen == 4) & (MIPS_DECODE[loop].flags & UNSIGNED))) && !proc64) {
               fprintf(stderr,"Operation not available with 32bit wide memory access \"%s\"\n",MIPS_DECODE[loop].name);
               exit(4);
               /* TODO: The R4000 documentation states that a LWU
                  instruction executed when in a 32bit processor mode
                  should cause a ReservedInstruction exception. This
                  will mean adding a run-time check into the code
                  sequence. */
             }

             if (isload) {
#if 1 /* see the comments attached to LOADDRMASK above */
               printf("     uword64 mask = 0x7;\n");
#else
               printf("     uword64 mask = %d;\n",(proc64 ? 0x7 : 0x3));
#endif
               printf("     unsigned int shift = %d;\n",(datalen >> 1));
               printf("     unsigned int reverse = (ReverseEndian ? (mask >> shift) : 0);\n");
               printf("     unsigned int bigend = (BigEndianCPU ? (mask >> shift) : 0);\n");
               printf("     unsigned int byte;\n");

/* TODO: This should really also check for 32bit world performing 32bit access */
               if (datalen != 8) /* not for DOUBLEWORD */
                printf("     paddr = ((paddr & ~mask) | ((paddr & mask) ^ (reverse << shift)));\n");

               printf("     memval = LoadMemory(uncached,%s,paddr,vaddr,isDATA,isREAL);\n",accesslength);

               /* The following will only make sense if the
                  "LoadMemory" above returns a DOUBLEWORD entity */
               if (datalen != 8) { /* not for DOUBLEWORD */
                 int valmask;
                 switch (datalen) {
                  case 1:
                   valmask = 0xFF;
                   break;

                  case 2:
                   valmask = 0xFFFF;
                   break;

                  case 4:
                   valmask = 0xFFFFFFFF;
                   break;

                  default:
                   fprintf(stderr,"Unrecognised datalen (%d) when processing \"%s\"\n",datalen,MIPS_DECODE[loop].name);
                   exit(4);
                 }
                 printf("     byte = ((vaddr & mask) ^ (bigend << shift));\n");
                 /* NOTE: The R4000 user manual has the COP_LW
                    occuring in the same cycle as the rest of the
                    instruction, yet the MIPS IV shows the operation
                    happening on the next cycle. To keep the simulator
                    simple, this code follows the R4000
                    manual. Experimentation with a silicon
                    implementation will be needed to ascertain the
                    correct operation. */
                 if (MIPS_DECODE[loop].flags & COPROC)
                  printf("     COP_LW(%s,destreg,(unsigned int)",
			 ((MIPS_DECODE[loop].flags & REG)
			  ? "1"
			  : "((instruction >> 26) & 0x3)"));
                 else
                  printf("     GPR[destreg] = (");

                 if (MIPS_DECODE[loop].flags & SIGNEXTEND)
                  printf("SIGNEXTEND(");
                 printf("((memval >> (8 * byte)) & 0x%08X)",valmask);
                 if (MIPS_DECODE[loop].flags & SIGNEXTEND)
                  printf(",%d)",(datalen * 8));
                 printf(");\n");
               } else {
                 if (MIPS_DECODE[loop].flags & COPROC)
                  printf("     COP_LD(%s,destreg,memval);;\n",
			 ((MIPS_DECODE[loop].flags & REG)
			  ? "1"
			  : "((instruction >> 26) & 0x3)"));
                 else
                  printf("     GPR[destreg] = memval;\n");
               }
             } else { /* store operation */
               if ((datalen == 1) || (datalen == 2)) {
                 /* SH and SB */
#if 1 /* see the comments attached to LOADDRMASK above */
                 printf("     uword64 mask = 0x7;\n");
#else
                 printf("     uword64 mask = %d;\n",(proc64 ? 0x7 : 0x3));
#endif
                 printf("     unsigned int shift = %d;\n",(datalen >> 1));
                 printf("     unsigned int reverse = (ReverseEndian ? (mask >> shift) : 0);\n");
                 printf("     unsigned int bigend = (BigEndianCPU ? (mask >> shift) : 0);\n");
                 printf("     unsigned int byte;\n");

                 printf("     paddr = ((paddr & ~mask) | ((paddr & mask) ^ (reverse << shift)));\n");
                 printf("     byte = ((vaddr & mask) ^ (bigend << shift));\n");
                 printf("     memval = (op2 << (8 * byte));\n");
               } else
                if (proc64 && (datalen == 4)) { /* proc64 SC and SW */
#if 1 /* see the comments attached to LOADDRMASK above */
                  printf("     uword64 mask = 0x7;\n");
#else
                  printf("     uword64 mask = %d;\n",(proc64 ? 0x7 : 0x3));
#endif
                  printf("     unsigned int byte;\n");
                  printf("     paddr = ((paddr & ~mask) | ((paddr & mask) ^ (ReverseEndian << 2)));\n");
                  printf("     byte = ((vaddr & mask) ^ (BigEndianCPU << 2));\n");
                  if (MIPS_DECODE[loop].flags & COPROC)
                   printf("     memval = (((uword64)COP_SW(%s,%s)) << (8 * byte));\n",
			  ((MIPS_DECODE[loop].flags & REG)
			   ? "1"
			   : "((instruction >> 26) & 0x3)"),
			  ((MIPS_DECODE[loop].flags & FP) ? "fs" : "destreg"));
                  else
                   printf("     memval = (op2 << (8 * byte));\n");
                } else { /* !proc64 SC and SW, plus proc64 SD and SCD */
                  if (MIPS_DECODE[loop].flags & COPROC)
                   printf("     memval = (uword64)COP_S%c(%s,%s);\n",
			  ((datalen == 8) ? 'D' : 'W'),
			  ((MIPS_DECODE[loop].flags & REG)
			   ? "1"
			   : "((instruction >> 26) & 0x3)"),
			  ((MIPS_DECODE[loop].flags & FP) ? "fs" : "destreg"));
                  else
                   printf("     memval = op2;\n");
                }

               if (MIPS_DECODE[loop].flags & ATOMIC)
                printf("      if (LLBIT)\n");

               printf("      {\n");
               printf("       StoreMemory(uncached,%s,memval,paddr,vaddr,isREAL);\n",accesslength);
               printf("      }\n");
             }

             if (MIPS_DECODE[loop].flags & ATOMIC) {
               if ((datalen != 4) && (datalen != 8)) {
                 fprintf(stderr,"ATOMIC can only be applied to WORD and DOUBLEWORD instructions \"%s\"\n",MIPS_DECODE[loop].name);
                 exit(4);
               } else
                if (isload)
                 printf("     LLBIT = 1;\n");
                else {
                  /* The documentation states that:

                     SC *WILL* fail if coherent store into the same
                     block occurs, or if an exception occurs between
                     the LL and SC instructions.

                     SC *MAY* fail if a load, store or prefetch is
                     executed on the processor (VR4300 doesn't seem
                     to), or if the instructions between the LL and
                     SC are not in a 2048byte contiguous VM range.

                     SC *MUST* have been preceded by an LL
                     (i.e. LLBIT will be set), and it must use the
                     same Vaddr, Paddr and cache-coherence algorithm
                     as the LL (which means we should store this
                     information from the load-conditional).
                     */
                  printf("     GPR[(instruction >> %d) & 0x%08X] = LLBIT;\n",OP_SH_RT,OP_MASK_RT);
                }
             }
           }
           printf("    }\n");
         }
         printf("   }\n");
        }
	break ;

       case FPPREFX:
        /* This code could be merged with the PREFIX generation above: */
        printf("   uword64 vaddr = ((uword64)op1 + (uword64)op2);\n");
        printf("   uword64 paddr;\n");
        printf("   int uncached;\n");
        printf("   if (AddressTranslation(vaddr,isDATA,isLOAD,&paddr,&uncached,isTARGET,isREAL))\n");
        printf("    Prefetch(uncached,paddr,vaddr,isDATA,fs);\n");
        break ;

       case FPMOVEC:
        if (MIPS_DECODE[loop].flags & CONTROL) {
          /* The following "magic" of interpreting the FP
             control-register number would not be needed if we were not
             trying to match our internal register numbers with those
             used by GDB. */
          printf("    if (to) {\n");
          if (doisa < 4) {
            printf("     if (fs == 0) {\n");
            printf("      PENDING_FILL((fs + FCR0IDX),WORD64LO(GPR[ft]));\n");
            printf("     } else if (fs == 31) {\n");
            printf("      PENDING_FILL((fs + FCR31IDX),WORD64LO(GPR[ft]));\n");
            printf("     } /* else NOP */\n");
            printf("     PENDING_FILL(COCIDX,0); /* special case */\n");
          } else {
            printf("     if (fs == 0) {\n");
            printf("      FCR0 = WORD64LO(GPR[ft]);\n");
            printf("     } else if (fs == 31) {\n");
            printf("      FCR31 = WORD64LO(GPR[ft]);\n");
            printf("     } /* else NOP */\n");
            printf("     SETFCC(0,((FCR31 & (1 << 23)) ? 1 : 0)); /* COC[1] */\n");
          }
          printf("    } else { /* control from */\n");
          if (doisa < 4) {
            printf("     if (fs == 0) {\n");
            printf("      PENDING_FILL(ft,SIGNEXTEND(FCR0,32));\n");
            printf("     } else if (fs == 31) {\n");
            printf("      PENDING_FILL(ft,SIGNEXTEND(FCR31,32));\n");
            printf("     } /* else NOP */\n");
          } else {
            printf("     if (fs == 0) {\n");
            printf("      GPR[ft] = SIGNEXTEND(FCR0,32);\n");
            printf("     } else if (fs == 31) {\n");
            printf("      GPR[ft] = SIGNEXTEND(FCR31,32);\n");
            printf("     } /* else NOP */\n");
          }
          printf("    }\n");
        } else {
          printf("    if (to) {\n");
          if (GETDATASIZE() == WORD) {
            if (doisa < 4) { 
              printf("     if (SizeFGR() == 64) {\n");
              printf("      PENDING_FILL((fs + FGRIDX),(SET64HI(0xDEADC0DE) | WORD64LO(GPR[ft])));\n");
              printf("     } else { \n");
              printf("      PENDING_FILL((fs + FGRIDX),WORD64LO(GPR[ft]));\n");
              printf("     }\n");
            } else {
              printf("     if (SizeFGR() == 64)\n");
              printf("      FGR[fs] = (SET64HI(0xDEADC0DE) | WORD64LO(GPR[ft]));\n");
              printf("     else\n");
              printf("      FGR[fs] = WORD64LO(GPR[ft]);\n");
              printf("     fpr_state[fs] = fmt_uninterpreted;\n");
            }
          } else if (GETDATASIZE() == DOUBLEWORD) {
            if (doisa < 4) {
              printf("     if (SizeFGR() == 64) {\n");
              printf("      PENDING_FILL((fs + FGRIDX),GPR[ft]);\n");
              printf("     } else\n");
              printf("      if ((fs & 0x1) == 0)\n");
              printf("       {\n");
              printf("        PENDING_FILL(((fs + 1) + FGRIDX),WORD64HI(GPR[ft]));\n");
              printf("        PENDING_FILL((fs + FGRIDX),WORD64LO(GPR[ft]));\n");
              printf("       }\n");
              if (features & FEATURE_WARN_RESULT) {
                printf("      else\n");
                printf("       UndefinedResult();\n");
              }
            } else {
              printf("     if (SizeFGR() == 64) {\n");
              printf("      FGR[fs] = GPR[ft];\n");
	      printf("      fpr_state[fs] = fmt_uninterpreted;\n");
              printf("     } else\n");
              printf("      if ((fs & 0x1) == 0)\n");
              printf("       {\n");
              printf("        FGR[fs + 1] = WORD64HI(GPR[ft]);\n");
              printf("        FGR[fs] = WORD64LO(GPR[ft]);\n");
	      printf("        fpr_state[fs + 1] = fmt_uninterpreted;\n");
	      printf("        fpr_state[fs] = fmt_uninterpreted;\n");
              printf("       }\n");
              if (features & FEATURE_WARN_RESULT) {
                printf("      else\n");
                printf("       UndefinedResult();\n");
              }
            }
          } else {
            fprintf(stderr,"Invalid data width specified in FPU Move operation\n");
            exit(1);
          }
          printf("    } else {\n");
          if (GETDATASIZE() == WORD) {
            if (doisa < 4) /* write-back occurs in next cycle */
             printf("     PENDING_FILL(ft,SIGNEXTEND(FGR[fs],32));\n");
            else /* in this cycle */
             printf("     GPR[ft] = SIGNEXTEND(FGR[fs],32);\n");
          } else if (GETDATASIZE() == DOUBLEWORD) {
            if (doisa < 4) { 
              printf("     if (SizeFGR() == 64) {\n");
              printf("      PENDING_FILL(ft,FGR[fs]);\n");
              printf("     } else\n");
              printf("      if ((fs & 0x1) == 0) {\n");
              printf("       PENDING_FILL(ft,(SET64HI(FGR[fs+1]) | FGR[fs]));\n");
              printf("      } else {\n");
              printf("       PENDING_FILL(ft,SET64HI(0xDEADC0DE) | 0xBAD0BAD0);\n");
              if (features & FEATURE_WARN_RESULT)
              printf("        UndefinedResult();\n");
              printf("      }\n");
            } else {
              printf("     if (SizeFGR() == 64)\n");
              printf("      GPR[ft] = FGR[fs];\n");
              printf("     else\n");
              printf("      if ((fs & 0x1) == 0)\n");
              printf("       GPR[ft] = (SET64HI(FGR[fs + 1]) | FGR[fs]);\n");
              printf("      else {\n");
              printf("       GPR[ft] = (SET64HI(0xDEADC0DE) | 0xBAD0BAD0);\n");
              if (features & FEATURE_WARN_RESULT)
              printf("       UndefinedResult();\n");
              printf("      }\n");
            }
          } else {
            fprintf(stderr,"Invalid data width specified in FPU Move operation\n");
            exit(1);
          }
          printf("    }\n");
        }
        break ;

       case FPMOVE:
        if (MIPS_DECODE[loop].flags & CONDITIONAL) {
          if (MIPS_DECODE[loop].flags & INTEGER) { /* moving GPR - testing FGR */
            printf("   if (GETFCC(condition_code) == boolean)\n");
            printf("    GPR[destreg] = op1;\n");
          } else {
            if (MIPS_DECODE[loop].flags & EQ) /* moving FGR - testing GPR */
             printf("   if (op2 %c= 0)\n",((MIPS_DECODE[loop].flags & NOT) ? '!' : '='));
            else
             printf("   if (GETFCC(condition_code) == boolean)\n");
            printf("    StoreFPR(destreg,format,ValueFPR(fs,format));\n");
            printf("   else\n");
            printf("    StoreFPR(destreg,format,ValueFPR(destreg,format));\n");
          }
        } else { /* simple MOVE */
          printf("   StoreFPR(destreg,format,ValueFPR(fs,format));\n");
        }
        break ;

       case FPNEG:
        printf("  if ((format != fmt_single) && (format != fmt_double))\n");
        printf("   SignalException(ReservedInstruction,instruction);\n");
        printf("  else\n");
        printf("   StoreFPR(destreg,format,Negate(ValueFPR(fs,format),format));\n");
        break ;

       case FPABS:
        printf("  if ((format != fmt_single) && (format != fmt_double))\n");
        printf("   SignalException(ReservedInstruction,instruction);\n");
        printf("  else\n");
        printf("   StoreFPR(destreg,format,AbsoluteValue(ValueFPR(fs,format),format));\n");
        break ;

       case FPDIV:
        printf("  if ((format != fmt_single) && (format != fmt_double))\n");
        printf("   SignalException(ReservedInstruction,instruction);\n");
        printf("  else\n");
        printf("   StoreFPR(destreg,format,Divide(ValueFPR(fs,format),ValueFPR(ft,format),format));\n");
        break ;

       case FPMUL:
        printf("  if ((format != fmt_single) && (format != fmt_double))\n");
        printf("   SignalException(ReservedInstruction,instruction);\n");
        printf("  else\n");
        printf("   StoreFPR(destreg,format,Multiply(ValueFPR(fs,format),ValueFPR(ft,format),format));\n");
        break ;

       case FPRECIP:
        printf("  if ((format != fmt_single) && (format != fmt_double))\n");
        printf("   SignalException(ReservedInstruction,instruction);\n");
        printf("  else\n");
        printf("   StoreFPR(destreg,format,Recip(ValueFPR(fs,format),format));\n");
        break ;

       case FPSQRT:
        printf("  if ((format != fmt_single) && (format != fmt_double))\n");
        printf("   SignalException(ReservedInstruction,instruction);\n");
        printf("  else\n");
        printf("   StoreFPR(destreg,format,%s(SquareRoot(ValueFPR(fs,format),format)));\n",((MIPS_DECODE[loop].flags & RECIP) ? "Recip" : ""));
        break ;

       case FPCEIL:
       case FPFLOOR:
       case FPTRUNC:
       case FPROUND:
        {
          char *op = "";
          char *type = "";

          switch (MIPS_DECODE[loop].type) {
            case FPCEIL:
             op = "FP_RM_TOPINF";
             break;
            case FPFLOOR:
             op = "FP_RM_TOMINF";
             break;
            case FPTRUNC:
             op = "FP_RM_TOZERO";
             break;
            case FPROUND:
             op = "FP_RM_NEAREST";
             break;
            default:
             fprintf(stderr,"Error: Handled missing for FP reason code %d\n",MIPS_DECODE[loop].type);
             exit(1);
          }

          switch (GETDATASIZE()) {
            case WORD :
             type = "fmt_word";
             break;
            case DOUBLEWORD :
             type = "fmt_long";
             break;
            default:
             fprintf(stderr,"Error in instruction encoding table for FP %s operation (not WORD or DOUBLEWORD)\n",op);
             exit(1);
          }
          printf("  if ((format != fmt_single) && (format != fmt_double))\n");
          printf("   SignalException(ReservedInstruction,instruction);\n");
          printf("  else\n");
          printf("   StoreFPR(destreg,%s,Convert(%s,ValueFPR(fs,format),format,%s));\n",type,op,type);
        }
        break ;

       case FPCONVERT:
        {
          char *type = "";
          switch (GETDATASIZE()) {
            case SINGLE:
             type = "fmt_single";
             break;
            case DOUBLE:
             type = "fmt_double";
             break;
            case WORD:
             type = "fmt_word";
             break;
            case DOUBLEWORD:
             type = "fmt_long";
             break;
            default :
             fprintf(stderr,"Error: Unknown data size %d in FPCONVERT instruction\n",GETDATASIZE());
             exit(1);
           }

          /* Not all combinations of conversion are valid at the
             moment: When converting to a fixed-point format, only
             floating-point sources are allowed. */
          printf("   if ((format == %s) | %s)\n",type,((MIPS_DECODE[loop].flags & FIXED) ? "((format == fmt_long) || (format == fmt_word))": "0"));
          printf("    SignalException(ReservedInstruction,instruction);\n");
          printf("   else\n");
          printf("    StoreFPR(destreg,%s,Convert(GETRM(),ValueFPR(fs,format),format,%s));\n",type,type);
        }
        break ;

       case FPSUB:
        if (MIPS_DECODE[loop].flags & MULTIPLY) {
          char *type = "";
          switch (GETDATASIZE()) {
            case SINGLE:
             type = "fmt_single";
             break;
            case DOUBLE:
             type = "fmt_double";
             break;
            default:
             fprintf(stderr,"Error: Invalid data size %d for FPSUB operation\n",GETDATASIZE());
             exit(1);
          }
          printf("   StoreFPR(destreg,%s,%s(Sub(Multiply(ValueFPR(fs,%s),ValueFPR(ft,%s),%s),ValueFPR(fr,%s),%s),%s));\n",type,((MIPS_DECODE[loop].flags & NOT) ? "Negate" : ""),type,type,type,type,type,type);
        } else {
          printf("  if ((format != fmt_single) && (format != fmt_double))\n");
          printf("   SignalException(ReservedInstruction,instruction);\n");
          printf("  else\n");
          printf("   StoreFPR(destreg,format,Sub(ValueFPR(fs,format),ValueFPR(ft,format),format));\n");
        }
        break ;

       case FPADD:
        if (MIPS_DECODE[loop].flags & MULTIPLY) {
          char *type = "";
          switch (GETDATASIZE()) {
            case SINGLE:
             type = "fmt_single";
             break;
            case DOUBLE:
             type = "fmt_double";
             break;
            default:
             fprintf(stderr,"Error: Invalid data size %d for FPADD operation in instruction table\n",GETDATASIZE());
             exit(1);
          }
	  if (MIPS_DECODE[loop].flags & NOT)
	    printf ("   StoreFPR(destreg,%s,Negate(Add(Multiply(ValueFPR(fs,%s),ValueFPR(ft,%s),%s),ValueFPR(fr,%s),%s),%s));\n",
		    type, type, type, type, type, type, type);
	  else
	    printf ("   StoreFPR(destreg,%s,Add(Multiply(ValueFPR(fs,%s),ValueFPR(ft,%s),%s),ValueFPR(fr,%s),%s));\n",
		    type, type, type, type, type, type);
        } else {
          printf("  if ((format != fmt_single) && (format != fmt_double))\n");
          printf("   SignalException(ReservedInstruction,instruction);\n");
          printf("  else\n");
          printf("   StoreFPR(destreg,format,Add(ValueFPR(fs,format),ValueFPR(ft,format),format));\n");
        }
        break ;

       case FPCOMPARE:
        /* For the MIPS I,II or III there *MUST* be at least one
           instruction between the compare that sets a condition code
           and the branch that tests it. NOTE: However the hardware
           does not detect this condition. */
        /* Explicitly limit the operation to S and D formats: */
        printf("   if ((format != fmt_single) && (format != fmt_double))\n");
        printf("    SignalException(ReservedInstruction,instruction);\n") ;
        printf("   else {\n");
        if (doisa < 4) {
          printf("    if ((cmpflags & (1 << 3)) || (condition_code != 0))\n");
          printf("     SignalException(ReservedInstruction,instruction);\n") ;
          printf("    else\n");
        }
        printf("     {\n");
        printf("      int ignore = 0;\n");
        printf("      int less = 0;\n");
        printf("      int equal = 0;\n");
        printf("      int unordered = 1;\n");
        printf("      uword64 ofs = ValueFPR(fs,format);\n");
        printf("      uword64 oft = ValueFPR(ft,format);\n");
        printf("      if (NaN(ofs,format) || NaN(oft,format)) {\n");
        printf("      if (FCSR & FP_ENABLE(IO)) {\n");
        printf("       FCSR |= FP_CAUSE(IO);\n");
        printf("       SignalException(FPE);\n");
        printf("       ignore = 1;\n");
        printf("      }\n");
        printf("     } else {\n");
        printf("      less = Less(ofs,oft,format);\n");
        printf("      equal = Equal(ofs,oft,format);\n");
        printf("      unordered = 0;\n");
        printf("     }\n");
        printf("     if (!ignore) {\n");
        printf("      int condition = (((cmpflags & (1 << 2)) && less) || ((cmpflags & (1 << 1)) && equal) || ((cmpflags & (1 << 0)) && unordered));\n");
        printf("      SETFCC(condition_code,condition);\n");
        printf("     }\n");
        printf("    }\n");
        printf("   }\n");
        break ;

       default:
	fprintf(stderr,"Unrecognised opcode type %d\n",MIPS_DECODE[loop].type) ;
        exit(6) ;
      }
     printf("  }\n") ;
     printf(" }\n") ;
     printf(" break ;\n") ;
    }
  }

 printf("default : /* Unrecognised instruction */\n") ;
 printf(" SignalException(ReservedInstruction,instruction);\n") ;
 printf(" break ;\n") ;
 printf("}\n}\n") ;
 printf("#endif /* simulator engine */\n");

 return ;
}

/*---------------------------------------------------------------------------*/

/* The command-line feature controls are presented in a similar style
   to those offered by GCC, in the aim of providing a consistent
   interface to the user. */
typedef enum {
  T_NONE,  /* no argument - mask and value fields control "feature" definition */
  T_NUM,   /* numeric argument - optionally preceded by '=' - mask field defines maximum value */
  T_STRING /* string argument - optionally prcededed by '=' */
} mactypes;

struct {
  char *name;
  mactypes type;
  unsigned int mask;
  unsigned int value;
  char *desc;
} machine_options[] = {
  {"ips",         T_NUM,   MASK_ISA,0,"\tSelect MIPS ISA version"},
  {"cpu",         T_STRING,0,0,"\t\tSelect particular MIPS architecture"},
  {"gp64",        T_NONE,  FEATURE_GP64,FEATURE_GP64,"\t\t\tSelect 64bit GP registers"},
  {"gp32",        T_NONE,  FEATURE_GP64,0,"\t\t\tSelect 32bit GP registers"},
  {"no-fp",       T_NONE,  FEATURE_HASFPU,0,"\t\tDisable FP simulation"},
  {"single-float",T_NONE,  (FEATURE_FPSINGLE | FEATURE_HASFPU),(FEATURE_FPSINGLE | FEATURE_HASFPU),"\t\tSelect single precision only FPU"},
  {"double-float",T_NONE,  (FEATURE_FPSINGLE | FEATURE_HASFPU),FEATURE_HASFPU,"\t\tSelect double precision FPU"},
  {0,             T_NONE,  0,0}
};

/* The following architecture identies are those accepted by the "-mcpu" option: */
struct architectures {
 const char *name;    /* ASCII string identifier for command-line, no white-space allowed */
 unsigned int idflag; /* or-ed into "isa" value */
};

static const struct architectures available_architectures[] = {
  {"4100",ARCH_VR4100}, /* NEC MIPS VR4100 */
  {0,     0}            /* terminator */
};

/*---------------------------------------------------------------------------*/

static void
usage(name)
     char *name;
{
 int loop;

 fprintf(stderr,"%s: Construct a MIPS simulator engine.\n",name);

 fprintf(stderr,"\
The output of this program is a block of 'C' code designed to be\n\
included into the main simulation control loop of a device specific\n\
simulator.\n");

 fprintf(stderr,"\nOptions:\n");
 fprintf(stderr," -h --help\t\tProvide this help text\n");
 fprintf(stderr," -f --fast\t\tProvide the fastest possible engine (i.e. no statistics)\n");
 fprintf(stderr," -w --warnings\t\tEnable all the simulator engine warnings\n");

 for (loop = 0; (machine_options[loop].name != 0); loop++) {
   fprintf(stderr," -m%s",machine_options[loop].name);
   switch (machine_options[loop].type) {
     case T_NUM :
       fprintf(stderr,"N (range 0..%d)",machine_options[loop].mask);
     case T_NONE :
       break;

     case T_STRING :
       fprintf(stderr,"=name");
       break;

     default :
       fprintf(stderr,"%s: FATAL error: unrecognised machine option type ID %d\n",machine_options[loop].type);
       exit(1);
   }
   fprintf(stderr,"%s\n",machine_options[loop].desc);
 }

 fprintf(stderr,"\nAvailable \"-mcpu\" architectures: ");
 for (loop = 0; (available_architectures[loop].name != 0); loop++)
   fprintf(stderr,"%s ",available_architectures[loop].name);
 fprintf(stderr,"\n\n");

 fprintf(stderr,"\
The \"trace\" and \"warnings\" options do not define the output stream.\n\
They only inform the code that includes the constructed engine to provide\n\
the required features.\n\n\
The \"-mips0\" option forces the construction of a simulator supporting\n\
the highest available MIPS ISA supported.\n");

 return;
}

/*---------------------------------------------------------------------------*/

int
main(argc,argv)
     int argc;
     char **argv;
{
  int c;
  char *progname = argv[0];
  unsigned int doarch = DEF_ISA;
  unsigned int features = 0; /* default state */

  if (DEF_FP)
   features |= FEATURE_HASFPU;
  if (!DEF_PROC64)
   features |= FEATURE_PROC32;
  if (DEF_FPSINGLE)
   features |= FEATURE_FPSINGLE;

  if (features & FEATURE_PROC32)
   features &= ~FEATURE_GP64;
  else
   features |= FEATURE_GP64;

  while (1) {
    int this_option_optind = (optind ? optind : 1);
    int option_index = 0;
    static struct option cmdline[] = {
      {"fast",    0,0,'f'},
      {"help",    0,0,'h'},
      {"warnings",0,0,'w'},
      {0,         0,0,0}
    };
   
    c = getopt_long(argc,argv,"hm:tw",cmdline,&option_index);
    if (c == -1)
     break ; /* out of the while loop */

    switch (c) {
      case 'h' : /* help */
       usage(progname);
       exit(0);

      case 'f' : /* fast */
       features |= FEATURE_FAST;
       break;

      case 'w' : /* warnings */
       features |= FEATURE_WARNINGS;
       /* TODO: Future extension: Allow better control over the warnings generated:
        disable warnings                -wnone                                  ~FEATURE_WARNINGS
        all possible warnings           -wall                                   FEATURE_WARNINGS
	pipeline stall occuring         -wstall                                 FEATURE_WARN_STALL
	LO/HI corruption                -wlo or -whi or -wlohi or -whilo        FEATURE_WARN_HILO
        write to zero                   -wzero                                  FEATURE_WARN_ZERO       actually performed in external code - though we should set a manifest
        bad r31 use                     -wr31                                   FEATURE_WARN_R31
        undefined results               -wresult                                FEATURE_WARN_RESULT
       */
       break;

      case 'm' : /* machine options */
       {
         int loop;

         for (loop = 0; (machine_options[loop].name != 0); loop++)
          if (strncmp(machine_options[loop].name,optarg,strlen(machine_options[loop].name)) == 0) {
            char *loptarg = (optarg + strlen(machine_options[loop].name));
            switch (machine_options[loop].type) {
              case T_NONE :
               if (*loptarg) {
                 fprintf(stderr,"%s: Spurious characters \"%s\" at end of -m%s option\n",progname,loptarg,machine_options[loop].name);
                 exit(1);
               }
               features &= ~(machine_options[loop].mask);
               features |= machine_options[loop].value;
               break;

              case T_NUM :
               if (*loptarg && *loptarg == '=')
                loptarg++;

               if (strcmp(machine_options[loop].name,"ips") == 0) {
                 unsigned int num;

                 if (!*loptarg) {
                   fprintf(stderr,"%s: ISA number expected after -mips\n",progname);
                   exit(1);
                 }

                 num = my_strtoul(loptarg,&loptarg,10);

                 if ((num == ULONG_MAX) && (errno = ERANGE)) {
                   fprintf(stderr,"%s: Invalid number given to -mips option\n",progname);
                   exit(1);
                 }

                 if (*loptarg) {
                   fprintf(stderr,"%s: Spurious trailing characters after ISA number \"%s\"\n",progname,loptarg);
                   exit(1);
                 }

                 if (num > MASK_ISA) {
                   fprintf(stderr,"%s: ISA number %d outside acceptable range (0..%d)\n",progname,num,MASK_ISA);
                   exit(1);
                 }

                 doarch = ((doarch & ~MASK_ISA) | num);
                 if ((num == 0) || (num > 2)) {
                   if ((features & FEATURE_PROC32) || !(features & FEATURE_GP64))
                    fprintf(stderr,"%s: Warning: -mips%d forcing -mgp64\n",progname,num);
                   features |= FEATURE_GP64;
                   features &= ~FEATURE_PROC32;
                 } else {
                   if (!(features & FEATURE_PROC32) || (features & FEATURE_GP64))
                    fprintf(stderr,"%s: Warning: -mips%d forcing -mgp32\n",progname,num);
                   features &= ~FEATURE_GP64;
                   features |= FEATURE_PROC32;
                 }
               } else {
                 fprintf(stderr,"%s: FATAL: Unrecognised (numeric) machine option -m%s\n",progname,optarg);
                 exit(1);
               }
               break;

              case T_STRING :
               if (*loptarg && *loptarg == '=')
                loptarg++;

               if (strcmp(machine_options[loop].name,"cpu") == 0) {
                 int archloop;

                 if (!*loptarg) {
                   fprintf(stderr,"%s: Architecture identifier expected after -mcpu\n",progname);
                   exit(1);
                 }

                 for (archloop = 0; (available_architectures[archloop].name != 0); archloop++) {
                   if ((*loptarg == 'v') || (*loptarg == 'V'))
                    *loptarg++;

                   if (*loptarg && (*loptarg == 'r') || (*loptarg == 'R'))
                    *loptarg++;

                   if (strcmp(available_architectures[archloop].name,loptarg) == 0) {
                     doarch |= available_architectures[archloop].idflag;
                     break;
                   }
                 }

                 if (available_architectures[archloop].name == 0) {
                   fprintf(stderr,"%s: Unrecognised MIPS architecture \"%s\"\n",progname,loptarg);
                   exit(1);
                 }
               } else {
                 fprintf(stderr,"%s: FATAL: Unrecognised (string) machine option -m%s\n",progname,optarg);
                 exit(1);
               }
               break;

              default :
               fprintf(stderr,"%s: FATAL error: unrecognised machine option type ID %d\n",progname,machine_options[loop].type);
               exit(1);
            }
            break;
          }

         if (machine_options[loop].name == 0) {
           fprintf(stderr,"%s: Unrecognised option: -m%s\n",progname,optarg);
           exit(1);
         }
       }
       break;

      case '?' :
       /* An error message should already have been displayed */
       exit(1);

      default :
       fprintf(stderr,"%s: FATAL: getopt returned unrecognised code 0x%08X\n",progname,c);
       exit(1);
    }
  }

  if (optind < argc) {
    fprintf(stderr,"%s: Spurios non-option arguments ",progname);
    while (optind < argc)
     fprintf(stderr,"\"%s\" ",argv[optind++]);
    fprintf(stderr,"\n");
    exit(1);
  }

  if ((features & FEATURE_FAST) && (features & FEATURE_WARNINGS))
   fprintf(stderr,"Warning: Fast model generation selected, along with trace or warnings.\n");

  process_instructions(doarch,features) ;
  return(0) ;
}

/*---------------------------------------------------------------------------*/

/* We can't assume that the compiler for the build system has strtoul,
   so we provide our own copy.  */

/*
 * Copyright (c) 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Convert a string to an unsigned long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
static unsigned long
my_strtoul(nptr, endptr, base)
	const char *nptr;
	char **endptr;
	register int base;
{
	register const char *s = nptr;
	register unsigned long acc;
	register int c;
	register unsigned long cutoff;
	register int neg = 0, any, cutlim;

	/*
	 * See strtol for comments as to the logic used.
	 */
	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;
	cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
	cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || acc == cutoff && c > cutlim)
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = ULONG_MAX;
		errno = ERANGE;
	} else if (neg)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *) (any ? s - 1 : nptr);
	return (acc);
}

/*---------------------------------------------------------------------------*/

/*> EOF gencode.c <*/
