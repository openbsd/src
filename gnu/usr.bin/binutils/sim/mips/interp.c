/*> interp.c <*/
/* Simulator for the MIPS architecture.

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
       $Date: 1996/11/23 03:55:14 $

NOTEs:

We only need to take account of the target endianness when moving data
between the simulator and the host. We do not need to worry about the
endianness of the host, since this sim code and GDB are executing in
the same process.

The IDT monitor (found on the VR4300 board), seems to lie about
register contents. It seems to treat the registers as sign-extended
32-bit values. This cause *REAL* problems when single-stepping 64-bit
code on the hardware.

*/

/* The TRACE and PROFILE manifests enable the provision of extra
   features. If they are not defined then a simpler (quicker)
   simulator is constructed without the required run-time checks,
   etc. */
#if 1 /* 0 to allow user build selection, 1 to force inclusion */
#define TRACE (1)
#define PROFILE (1)
#endif

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <ansidecl.h>
#include <signal.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include "getopt.h"
#include "libiberty.h"

#include "remote-sim.h" /* GDB simulator interface */
#include "callback.h"   /* GDB simulator callback interface */

#include "support.h"    /* internal support manifests */

#include "sysdep.h"

#ifndef SIGBUS
#define SIGBUS SIGSEGV
#endif

/* Get the simulator engine description, without including the code: */
#define SIM_MANIFESTS
#include "engine.c"
#undef SIM_MANIFESTS

/* This variable holds the GDB view of the target endianness: */
extern int target_byte_order;

/* The following reserved instruction value is used when a simulator
   trap is required. NOTE: Care must be taken, since this value may be
   used in later revisions of the MIPS ISA. */
#define RSVD_INSTRUCTION        (0x7C000000)
#define RSVD_INSTRUCTION_AMASK  (0x03FFFFFF)

/* NOTE: These numbers depend on the processor architecture being
   simulated: */
#define Interrupt               (0)
#define TLBModification         (1)
#define TLBLoad                 (2)
#define TLBStore                (3)
#define AddressLoad             (4)
#define AddressStore            (5)
#define InstructionFetch        (6)
#define DataReference           (7)
#define SystemCall              (8)
#define BreakPoint              (9)
#define ReservedInstruction     (10)
#define CoProcessorUnusable     (11)
#define IntegerOverflow         (12)    /* Arithmetic overflow (IDT monitor raises SIGFPE) */
#define Trap                    (13)
#define FPE                     (15)
#define Watch                   (23)

/* The following exception code is actually private to the simulator
   world. It is *NOT* a processor feature, and is used to signal
   run-time errors in the simulator. */
#define SimulatorFault      (0xFFFFFFFF)

/* The following are generic to all versions of the MIPS architecture
   to date: */
/* Memory Access Types (for CCA): */
#define Uncached                (0)
#define CachedNoncoherent       (1)
#define CachedCoherent          (2)
#define Cached                  (3)

#define isINSTRUCTION   (1 == 0) /* FALSE */
#define isDATA          (1 == 1) /* TRUE */

#define isLOAD          (1 == 0) /* FALSE */
#define isSTORE         (1 == 1) /* TRUE */

#define isREAL          (1 == 0) /* FALSE */
#define isRAW           (1 == 1) /* TRUE */

#define isTARGET        (1 == 0) /* FALSE */
#define isHOST          (1 == 1) /* TRUE */

/* The "AccessLength" specifications for Loads and Stores. NOTE: This
   is the number of bytes minus 1. */
#define AccessLength_BYTE       (0)
#define AccessLength_HALFWORD   (1)
#define AccessLength_TRIPLEBYTE (2)
#define AccessLength_WORD       (3)
#define AccessLength_QUINTIBYTE (4)
#define AccessLength_SEXTIBYTE  (5)
#define AccessLength_SEPTIBYTE  (6)
#define AccessLength_DOUBLEWORD (7)

#if defined(HASFPU)
/* FPU registers must be one of the following types. All other values
   are reserved (and undefined). */
typedef enum {
 fmt_single  = 0,
 fmt_double  = 1,
 fmt_word    = 4,
 fmt_long    = 5,
 /* The following are well outside the normal acceptable format
    range, and are used in the register status vector. */
 fmt_unknown       = 0x10000000,
 fmt_uninterpreted = 0x20000000,
} FP_formats;
#endif /* HASFPU */

/* NOTE: We cannot avoid globals, since the GDB "sim_" interface does
   not allow a private variable to be passed around. This means that
   simulators under GDB can only be single-threaded. However, it would
   be possible for the simulators to be multi-threaded if GDB allowed
   for a private pointer to be maintained. i.e. a general "void **ptr"
   variable that GDB passed around in the argument list to all of
   sim_xxx() routines. It could be initialised to NULL by GDB, and
   then updated by sim_open() and used by the other sim_xxx() support
   functions. This would allow new features in the simulator world,
   like storing a context - continuing execution to gather a result,
   and then going back to the point where the context was saved and
   changing some state before continuing. i.e. the ability to perform
   UNDOs on simulations. It would also allow the simulation of
   shared-memory multi-processor systems. */

static host_callback *callback = NULL; /* handle onto the current callback structure */

/* This is nasty, since we have to rely on matching the register
   numbers used by GDB. Unfortunately, depending on the MIPS target
   GDB uses different register numbers. We cannot just include the
   relevant "gdb/tm.h" link, since GDB may not be configured before
   the sim world, and also the GDB header file requires too much other
   state. */
/* TODO: Sort out a scheme for *KNOWING* the mapping between real
   registers, and the numbers that GDB uses. At the moment due to the
   order that the tools are built, we cannot rely on a configured GDB
   world whilst constructing the simulator. This means we have to
   assume the GDB register number mapping. */
#define LAST_EMBED_REGNUM (89)

/* To keep this default simulator simple, and fast, we use a direct
   vector of registers. The internal simulator engine then uses
   manifests to access the correct slot. */
static ut_reg registers[LAST_EMBED_REGNUM + 1];
static int register_widths[LAST_EMBED_REGNUM + 1];

#define GPR     (&registers[0])
#if defined(HASFPU)
#define FGRIDX  (38)
#define FGR     (&registers[FGRIDX])
#endif /* HASFPU */
#define LO      (registers[33])
#define HI      (registers[34])
#define PC      (registers[37])
#define CAUSE   (registers[36])
#define SRIDX   (32)
#define SR      (registers[SRIDX])      /* CPU status register */
#define FCR0IDX  (71)
#define FCR0    (registers[FCR0IDX])    /* really a 32bit register */
#define FCR31IDX (70)
#define FCR31   (registers[FCR31IDX])   /* really a 32bit register */
#define FCSR    (FCR31)
#define COCIDX  (LAST_EMBED_REGNUM + 2) /* special case : outside the normal range */

/* The following are pseudonyms for standard registers */
#define ZERO    (registers[0])
#define V0      (registers[2])
#define A0      (registers[4])
#define A1      (registers[5])
#define A2      (registers[6])
#define A3      (registers[7])
#define SP      (registers[29])
#define RA      (registers[31])

static ut_reg EPC = 0; /* Exception PC */

#if defined(HASFPU)
/* Keep the current format state for each register: */
static FP_formats fpr_state[32];
#endif /* HASFPU */

/* VR4300 CP0 configuration register: */
static unsigned int CONFIG = 0;

/* The following are internal simulator state variables: */
static ut_reg IPC = 0; /* internal Instruction PC */
static ut_reg DSPC = 0;  /* delay-slot PC */


/* TODO : these should be the bitmasks for these bits within the
   status register. At the moment the following are VR4300
   bit-positions: */
#define status_KSU_mask  (0x3)          /* mask for KSU bits */
#define status_KSU_shift (3)            /* shift for field */
#define ksu_kernel       (0x0)
#define ksu_supervisor   (0x1)
#define ksu_user         (0x2)
#define ksu_unknown      (0x3)

#define status_RE        (1 << 25)      /* Reverse Endian in user mode */
#define status_FR        (1 << 26)      /* enables MIPS III additional FP registers */
#define status_SR        (1 << 20)      /* soft reset or NMI */
#define status_BEV       (1 << 22)      /* Location of general exception vectors */
#define status_TS        (1 << 21)      /* TLB shutdown has occurred */
#define status_ERL       (1 <<  2)      /* Error level */
#define status_RP        (1 << 27)      /* Reduced Power mode */

#define config_EP_mask   (0xF)
#define config_EP_shift  (27)
#define config_EP_D      (0x0)
#define config_EP_DxxDxx (0x6)

#define config_BE        (1 << 15)

#define cause_BD        ((unsigned)1 << 31)     /* Exception in branch delay slot */

#if defined(HASFPU)
/* Macro to update FPSR condition-code field. This is complicated by
   the fact that there is a hole in the index range of the bits within
   the FCSR register. Also, the number of bits visible depends on the
   MIPS ISA version being supported. */
#define SETFCC(cc,v) {\
                    int bit = ((cc == 0) ? 23 : (24 + (cc)));\
                    FCSR = ((FCSR & ~(1 << bit)) | ((v) << bit));\
                  }
#define GETFCC(cc) (((((cc) == 0) ? (FCSR & (1 << 23)) : (FCSR & (1 << (24 + (cc))))) != 0) ? 1 : 0)

/* This should be the COC1 value at the start of the preceding
   instruction: */
#define PREVCOC1() ((state & simPCOC1) ? 1 : 0)
#endif /* HASFPU */

/* Standard FCRS bits: */
#define IR (0) /* Inexact Result */
#define UF (1) /* UnderFlow */
#define OF (2) /* OverFlow */
#define DZ (3) /* Division by Zero */
#define IO (4) /* Invalid Operation */
#define UO (5) /* Unimplemented Operation */

/* Get masks for individual flags: */
#if 1 /* SAFE version */
#define FP_FLAGS(b)  (((unsigned)(b) < 5) ? (1 << ((b) + 2)) : 0)
#define FP_ENABLE(b) (((unsigned)(b) < 5) ? (1 << ((b) + 7)) : 0)
#define FP_CAUSE(b)  (((unsigned)(b) < 6) ? (1 << ((b) + 12)) : 0)
#else
#define FP_FLAGS(b)  (1 << ((b) + 2))
#define FP_ENABLE(b) (1 << ((b) + 7))
#define FP_CAUSE(b)  (1 << ((b) + 12))
#endif

#define FP_FS         (1 << 24) /* MIPS III onwards : Flush to Zero */

#define FP_MASK_RM    (0x3)
#define FP_SH_RM      (0)
#define FP_RM_NEAREST (0) /* Round to nearest        (Round) */
#define FP_RM_TOZERO  (1) /* Round to zero           (Trunc) */
#define FP_RM_TOPINF  (2) /* Round to Plus infinity  (Ceil) */
#define FP_RM_TOMINF  (3) /* Round to Minus infinity (Floor) */
#define GETRM()       (int)((FCSR >> FP_SH_RM) & FP_MASK_RM)

/* Slots for delayed register updates. For the moment we just have a
   fixed number of slots (rather than a more generic, dynamic
   system). This keeps the simulator fast. However, we only allow for
   the register update to be delayed for a single instruction
   cycle. */
#define PSLOTS (5) /* Maximum number of instruction cycles */
static int    pending_in;
static int    pending_out;
static int    pending_total;
static int    pending_slot_count[PSLOTS];
static int    pending_slot_reg[PSLOTS];
static ut_reg pending_slot_value[PSLOTS];

/* The following are not used for MIPS IV onwards: */
#define PENDING_FILL(r,v) {\
/* printf("DBG: FILL BEFORE pending_in = %d, pending_out = %d, pending_total = %d\n",pending_in,pending_out,pending_total); */\
                            if (pending_slot_reg[pending_in] != (LAST_EMBED_REGNUM + 1))\
                             sim_warning("Attempt to over-write pending value");\
                            pending_slot_count[pending_in] = 2;\
                            pending_slot_reg[pending_in] = (r);\
                            pending_slot_value[pending_in] = (uword64)(v);\
/*printf("DBG: FILL        reg %d value = 0x%08X%08X\n",(r),WORD64HI(v),WORD64LO(v));*/\
                            pending_total++;\
                            pending_in++;\
                            if (pending_in == PSLOTS)\
                             pending_in = 0;\
/*printf("DBG: FILL AFTER  pending_in = %d, pending_out = %d, pending_total = %d\n",pending_in,pending_out,pending_total);*/\
                          }

static int LLBIT = 0;
/* LLBIT = Load-Linked bit. A bit of "virtual" state used by atomic
   read-write instructions. It is set when a linked load occurs. It is
   tested and cleared by the conditional store. It is cleared (during
   other CPU operations) when a store to the location would no longer
   be atomic. In particular, it is cleared by exception return
   instructions. */

static int HIACCESS = 0;
static int LOACCESS = 0;
/* The HIACCESS and LOACCESS counts are used to ensure that
   corruptions caused by using the HI or LO register to close to a
   following operation are spotted. */
static ut_reg HLPC = 0;

/* TODO: The 4300 has interlocks so we should not need to warn of the possible over-write (CHECK THIS) */
/* If either of the preceding two instructions have accessed the HI or
   LO registers, then the values they see should be
   undefined. However, to keep the simulator world simple, we just let
   them use the value read and raise a warning to notify the user: */
#define CHECKHILO(s)    {\
                          if ((HIACCESS != 0) || (LOACCESS != 0))\
                            sim_warning("%s over-writing HI and LO registers values (PC = 0x%08X%08X HLPC = 0x%08X%08X)\n",(s),(unsigned int)(PC>>32),(unsigned int)(PC&0xFFFFFFFF),(unsigned int)(HLPC>>32),(unsigned int)(HLPC&0xFFFFFFFF));\
                        }

/* NOTE: We keep the following status flags as bit values (1 for true,
   0 for false). This allows them to be used in binary boolean
   operations without worrying about what exactly the non-zero true
   value is. */

/* UserMode */
#define UserMode        ((((SR & status_KSU_mask) >> status_KSU_shift) == ksu_user) ? 1 : 0)

/* BigEndianMem */
/* Hardware configuration. Affects endianness of LoadMemory and
   StoreMemory and the endianness of Kernel and Supervisor mode
   execution. The value is 0 for little-endian; 1 for big-endian. */
#define BigEndianMem    ((CONFIG & config_BE) ? 1 : 0)
/* NOTE: Problems will occur if the simulator memory model does not
   match the host program expectation. i.e. if the host is writing
   big-endian values to a little-endian memory model. */

/* ReverseEndian */
/* This mode is selected if in User mode with the RE bit being set in
   SR (Status Register). It reverses the endianness of load and store
   instructions. */
#define ReverseEndian   (((SR & status_RE) && UserMode) ? 1 : 0)

/* BigEndianCPU */
/* The endianness for load and store instructions (0=little;1=big). In
   User mode this endianness may be switched by setting the state_RE
   bit in the SR register. Thus, BigEndianCPU may be computed as
   (BigEndienMem EOR ReverseEndian). */
#define BigEndianCPU    (BigEndianMem ^ ReverseEndian) /* Already bits */

#if !defined(FASTSIM) || defined(PROFILE)
/* At the moment these values will be the same, since we do not have
   access to the pipeline cycle count information from the simulator
   engine. */
static unsigned int instruction_fetches = 0;
static unsigned int instruction_fetch_overflow = 0;
static unsigned int pipeline_ticks = 0;
#endif

/* Flags in the "state" variable: */
#define simSTOP         (1 << 0)  /* 0 = execute; 1 = stop simulation */
#define simSTEP         (1 << 1)  /* 0 = run; 1 = single-step */
#define simHALTEX       (1 << 2)  /* 0 = run; 1 = halt on exception */
#define simHALTIN       (1 << 3)  /* 0 = run; 1 = halt on interrupt */
#define simTRACE        (1 << 8)  /* 0 = do nothing; 1 = trace address activity */
#define simPROFILE      (1 << 9)  /* 0 = do nothing; 1 = gather profiling samples */
#define simHOSTBE       (1 << 10) /* 0 = little-endian; 1 = big-endian (host endianness) */
/* Whilst simSTOP is not set, the simulator control loop should just
   keep simulating instructions. The simSTEP flag is used to force
   single-step execution. */
#define simBE           (1 << 16) /* 0 = little-endian; 1 = big-endian (target endianness) */
#define simPCOC0        (1 << 17) /* COC[1] from current */
#define simPCOC1        (1 << 18) /* COC[1] from previous */
#define simDELAYSLOT    (1 << 24) /* 0 = do nothing; 1 = delay slot entry exists */
#define simSKIPNEXT     (1 << 25) /* 0 = do nothing; 1 = skip instruction */
#define simEXCEPTION    (1 << 26) /* 0 = no exception; 1 = exception has occurred */
#define simEXIT         (1 << 27) /* 0 = do nothing; 1 = run-time exit() processing */

static unsigned int state = 0;
static unsigned int rcexit = 0; /* _exit() reason code holder */

#define DELAYSLOT()     {\
                          if (state & simDELAYSLOT)\
                            sim_warning("Delay slot already activated (branch in delay slot?)");\
                          state |= simDELAYSLOT;\
                        }

#define NULLIFY()       {\
                          state &= ~simDELAYSLOT;\
                          state |= simSKIPNEXT;\
                        }

#define K0BASE  (0x80000000)
#define K0SIZE  (0x20000000)
#define K1BASE  (0xA0000000)
#define K1SIZE  (0x20000000)

/* Very simple memory model to start with: */
static unsigned char *membank = NULL;
static ut_reg membank_base = K1BASE;
static unsigned membank_size = (1 << 20); /* (16 << 20); */ /* power-of-2 */

/* Simple run-time monitor support */
static unsigned char *monitor = NULL;
static ut_reg monitor_base = 0xBFC00000;
static unsigned monitor_size = (1 << 11); /* power-of-2 */

static char *logfile = NULL; /* logging disabled by default */
static FILE *logfh = NULL;

#if defined(TRACE)
static char *tracefile = "trace.din"; /* default filename for trace log */
static FILE *tracefh = NULL;
#endif /* TRACE */

#if defined(PROFILE)
static unsigned profile_frequency = 256;
static unsigned profile_nsamples = (128 << 10);
static unsigned short *profile_hist = NULL;
static ut_reg profile_minpc;
static ut_reg profile_maxpc;
static int profile_shift = 0; /* address shift amount */
#endif /* PROFILE */

/* The following are used to provide shortcuts to the required version
   of host<->target copying. This avoids run-time conditionals, which
   would slow the simulator throughput. */
typedef unsigned int (*fnptr_read_word) PARAMS((unsigned char *memory));
typedef unsigned int (*fnptr_swap_word) PARAMS((unsigned int data));
typedef uword64 (*fnptr_read_long) PARAMS((unsigned char *memory));
typedef uword64 (*fnptr_swap_long) PARAMS((uword64 data));

static fnptr_read_word host_read_word;
static fnptr_read_long host_read_long;
static fnptr_swap_word host_swap_word;
static fnptr_swap_long host_swap_long;

/*---------------------------------------------------------------------------*/
/*-- GDB simulator interface ------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static void dotrace PARAMS((FILE *tracefh,int type,unsigned int address,int width,char *comment,...));
static void sim_warning PARAMS((char *fmt,...));
extern void sim_error PARAMS((char *fmt,...));
static void ColdReset PARAMS((void));
static int AddressTranslation PARAMS((uword64 vAddr,int IorD,int LorS,uword64 *pAddr,int *CCA,int host,int raw));
static void StoreMemory PARAMS((int CCA,int AccessLength,uword64 MemElem,uword64 pAddr,uword64 vAddr,int raw));
static uword64 LoadMemory PARAMS((int CCA,int AccessLength,uword64 pAddr,uword64 vAddr,int IorD,int raw));
static void SignalException PARAMS((int exception,...));
static void simulate PARAMS((void));
static long getnum PARAMS((char *value));
extern void sim_size PARAMS((unsigned int newsize));
extern void sim_set_profile PARAMS((int frequency));
static unsigned int power2 PARAMS((unsigned int value));

/*---------------------------------------------------------------------------*/

void
sim_open (args)
     char *args;
{
  if (callback == NULL) {
    fprintf(stderr,"SIM Error: sim_open() called without callbacks attached\n");
    return;
  }

  /* The following ensures that the standard file handles for stdin,
     stdout and stderr are initialised: */
  callback->init(callback);

  state = 0;
  CHECKSIM();
  if (state & simEXCEPTION) {
    fprintf(stderr,"This simulator is not suitable for this host configuration\n");
    exit(1);
  }

  {
    int data = 0x12;
    if (*((char *)&data) != 0x12)
     state |= simHOSTBE; /* big-endian host */
  }

#if defined(HASFPU)
  /* Check that the host FPU conforms to IEEE 754-1985 for the SINGLE
     and DOUBLE binary formats. This is a bit nasty, requiring that we
     trust the explicit manifests held in the source: */
  {
    unsigned int s[2];
    s[state & simHOSTBE ? 0 : 1] = 0x40805A5A;
    s[state & simHOSTBE ? 1 : 0] = 0x00000000;

    /* TODO: We need to cope with the simulated target and the host
       not having the same endianness. This will require the high and
       low words of a (double) to be swapped when converting between
       the host and the simulated target. */

    if (((float)4.01102924346923828125 != *(float *)(s + ((state & simHOSTBE) ? 0 : 1))) || ((double)523.2939453125 != *(double *)s)) {
      fprintf(stderr,"The host executing the simulator does not seem to have IEEE 754-1985 std FP\n");
      fprintf(stderr,"*(float *)s = %.20f (4.01102924346923828125)\n",*(float *)s);
      fprintf(stderr,"*(double *)s = %.20f (523.2939453125)\n",*(double *)s);
      exit(1);
    }
  }
#endif /* HASFPU */

  /* This is NASTY, in that we are assuming the size of specific
     registers: */
  {
    int rn;
    for (rn = 0; (rn < (LAST_EMBED_REGNUM + 1)); rn++) {
      if (rn < 32)
       register_widths[rn] = GPRLEN;
      else if ((rn >= FGRIDX) && (rn < (FGRIDX + 32)))
       register_widths[rn] = GPRLEN;
      else if ((rn >= 33) && (rn <= 37))
       register_widths[rn] = GPRLEN;
      else if ((rn == SRIDX) || (rn == FCR0IDX) || (rn == FCR31IDX) || ((rn >= 72) && (rn <= 89)))
       register_widths[rn] = 32;
      else
       register_widths[rn] = 0;
    }
  }

  /* It would be good if we could select particular named MIPS
     architecture simulators. However, having a pre-built, fixed
     engine would mean including multiple engines. If the simulator is
     changed to a run-time conditional version, then the ability to
     select a particular architecture would be straightforward. */
  if (args != NULL) {
    int c;
    char *cline;
    char **argv;
    int argc;
    static struct option cmdline[] = {
      {"help",     0,0,'h'},
      {"log",      1,0,'l'},
      {"name",     1,0,'n'},
      {"profile",  0,0,'p'},
      {"size",     1,0,'s'},
      {"trace",    0,0,'t'},
      {"tracefile",1,0,'z'},
      {"frequency",1,0,'y'},
      {"samples",  1,0,'x'},
      {0,     0,0,0}
    };

    /* Unfortunately, getopt_long() is designed to be used with
       vectors, where the first option is normally program name (and
       ignored). We cheat by creating a dummy first argument, so that
       we can use the standard argument processing. */
#define DUMMYARG "simulator "
    cline = (char *)malloc(strlen(args) + strlen(DUMMYARG) + 1);
    if (cline == NULL) {
      fprintf(stderr,"Failed to allocate memory for command line buffer\n");
      exit(1);
    }
    sprintf(cline,"%s%s",DUMMYARG,args);
    argv = buildargv(cline);
    for (argc = 0; argv[argc]; argc++);

    /* Unfortunately, getopt_long() assumes that it is ignoring the
       first argument (normally the program name). This means it
       ignores the first option on our "args" line. */
    optind = 0; /* Force reset of argument processing */
    while (1) {
      int option_index = 0;

      c = getopt_long(argc,argv,"hn:s:tp",cmdline,&option_index);
      if (c == -1)
       break;

      switch (c) {
       case 'h':
        callback->printf_filtered(callback,"Usage:\n\t\
target sim [-h] [--log=<file>] [--name=<model>] [--size=<amount>]");
#if defined(TRACE)
        callback->printf_filtered(callback," [-t [--tracefile=<name>]]");
#endif /* TRACE */
#if defined(PROFILE)
        callback->printf_filtered(callback," [-p [--frequency=<count>] [--samples=<count>]]");
#endif /* PROFILE */
        callback->printf_filtered(callback,"\n");
        break;

       case 'l':
        if (optarg != NULL) {
          char *tmp;
          tmp = (char *)malloc(strlen(optarg) + 1);
          if (tmp == NULL)
           callback->printf_filtered(callback,"Failed to allocate buffer for logfile name \"%s\"\n",optarg);
          else {
            strcpy(tmp,optarg);
            logfile = tmp;
          }
        }
        break;

       case 'n':
        callback->printf_filtered(callback,"Explicit model selection not yet available (Ignoring \"%s\")\n",optarg);
        break;

       case 's':
        membank_size = (unsigned)getnum(optarg);
        break;

       case 't':
#if defined(TRACE)
        /* Eventually the simTRACE flag could be treated as a toggle, to
           allow external control of the program points being traced
           (i.e. only from main onwards, excluding the run-time setup,
           etc.). */
        state |= simTRACE;
#else /* !TRACE */
        fprintf(stderr,"\
Simulator constructed without tracing support (for performance).\n\
Re-compile simulator with \"-DTRACE\" to enable this option.\n");
#endif /* !TRACE */
        break;

       case 'z':
#if defined(TRACE)
        if (optarg != NULL) {
          char *tmp;
          tmp = (char *)malloc(strlen(optarg) + 1);
          if (tmp == NULL)
           callback->printf_filtered(callback,"Failed to allocate buffer for tracefile name \"%s\"\n",optarg);
          else {
            strcpy(tmp,optarg);
            tracefile = tmp;
            callback->printf_filtered(callback,"Placing trace information into file \"%s\"\n",tracefile);
          }
        }
#endif /* TRACE */
        break;

       case 'p':
#if defined(PROFILE)
        state |= simPROFILE;
#else /* !PROFILE */
        fprintf(stderr,"\
Simulator constructed without profiling support (for performance).\n\
Re-compile simulator with \"-DPROFILE\" to enable this option.\n");
#endif /* !PROFILE */
        break;

       case 'x':
#if defined(PROFILE)
        profile_nsamples = (unsigned)getnum(optarg);
#endif /* PROFILE */
        break;

       case 'y':
#if defined(PROFILE)
        sim_set_profile((int)getnum(optarg));
#endif /* PROFILE */
        break;

       default:
        callback->printf_filtered(callback,"Warning: Simulator getopt returned unrecognised code 0x%08X\n",c);
       case '?':
        break;
      }
    }

#if 0
    if (optind < argc) {
      callback->printf_filtered(callback,"Warning: Ignoring spurious non-option arguments ");
      while (optind < argc)
       callback->printf_filtered(callback,"\"%s\" ",argv[optind++]);
      callback->printf_filtered(callback,"\n");
    }
#endif

    freeargv(argv);
  }

  if (logfile != NULL) {
    if (strcmp(logfile,"-") == 0)
     logfh = stdout;
    else {
      logfh = fopen(logfile,"wb+");
      if (logfh == NULL) {
        callback->printf_filtered(callback,"Failed to create file \"%s\", writing log information to stderr.\n",tracefile);
        logfh = stderr;
      }
    }
  }

  /* If the host has "mmap" available we could use it to provide a
     very large virtual address space for the simulator, since memory
     would only be allocated within the "mmap" space as it is
     accessed. This can also be linked to the architecture specific
     support, required to simulate the MMU. */
  sim_size(membank_size);
  /* NOTE: The above will also have enabled any profiling state */

  ColdReset();
  /* If we were providing a more complete I/O, co-processor or memory
     simulation, we should perform any "device" initialisation at this
     point. This can include pre-loading memory areas with particular
     patterns (e.g. simulating ROM monitors). */

  /* We can start writing to the memory, now that the processor has
     been reset: */
  monitor = (unsigned char *)calloc(1,monitor_size);
  if (!monitor) {
    fprintf(stderr,"Not enough VM for monitor simulation (%d bytes)\n",monitor_size);
  } else {
    int loop;
    /* Entry into the IDT monitor is via fixed address vectors, and
       not using machine instructions. To avoid clashing with use of
       the MIPS TRAP system, we place our own (simulator specific)
       "undefined" instructions into the relevant vector slots. */
    for (loop = 0; (loop < monitor_size); loop += 4) {
      uword64 vaddr = (monitor_base + loop);
      uword64 paddr;
      int cca;
      if (AddressTranslation(vaddr,isDATA,isSTORE,&paddr,&cca,isTARGET,isRAW))
       StoreMemory(cca,AccessLength_WORD,(RSVD_INSTRUCTION | ((loop >> 2) & RSVD_INSTRUCTION_AMASK)),paddr,vaddr,isRAW);
    }
    /* The PMON monitor uses the same address space, but rather than
       branching into it the address of a routine is loaded. We can
       cheat for the moment, and direct the PMON routine to IDT style
       instructions within the monitor space. This relies on the IDT
       monitor not using the locations from 0xBFC00500 onwards as its
       entry points.*/
    for (loop = 0; (loop < 24); loop++)
      {
        uword64 vaddr = (monitor_base + 0x500 + (loop * 4));
        uword64 paddr;
        int cca;
        unsigned int value = ((0x500 - 8) / 8); /* default UNDEFINED reason code */
        switch (loop)
          {
            case 0: /* read */
              value = 7;
              break;

            case 1: /* write */
              value = 8;
              break;

            case 2: /* open */
              value = 6;
              break;

            case 3: /* close */
              value = 10;
              break;

            case 5: /* printf */
              value = ((0x500 - 16) / 8); /* not an IDT reason code */
              break;

            case 8: /* cliexit */
              value = 17;
              break;
          }
        value = (monitor_base + (value * 8));
        if (AddressTranslation(vaddr,isDATA,isSTORE,&paddr,&cca,isTARGET,isRAW))
          StoreMemory(cca,AccessLength_WORD,value,paddr,vaddr,isRAW);
        else
          sim_error("Failed to write to monitor space 0x%08X%08X",WORD64HI(vaddr),WORD64LO(vaddr));
      }
  }

#if defined(TRACE)
   if (state & simTRACE) {
     tracefh = fopen(tracefile,"wb+");
     if (tracefh == NULL) {
       sim_warning("Failed to create file \"%s\", writing trace information to stderr.",tracefile);
       tracefh = stderr;
     }
   }
#endif /* TRACE */

  return;
}

/* For the profile writing, we write the data in the host
   endianness. This unfortunately means we are assuming that the
   profile file we create is processed on the same host executing the
   simulator. The gmon.out file format should either have an explicit
   endianness, or a method of encoding the endianness in the file
   header. */
static int
writeout32(fh,val)
     FILE *fh;
     unsigned int val;
{
  char buff[4];
  int res = 1;

  if (state & simHOSTBE) {
    buff[3] = ((val >>  0) & 0xFF);
    buff[2] = ((val >>  8) & 0xFF);
    buff[1] = ((val >> 16) & 0xFF);
    buff[0] = ((val >> 24) & 0xFF);
  } else {
    buff[0] = ((val >>  0) & 0xFF);
    buff[1] = ((val >>  8) & 0xFF);
    buff[2] = ((val >> 16) & 0xFF);
    buff[3] = ((val >> 24) & 0xFF);
  }
  if (fwrite(buff,4,1,fh) != 1) {
    sim_warning("Failed to write 4bytes to the profile file");
    res = 0;
  }
  return(res);
}

static int
writeout16(fh,val)
     FILE *fh;
     unsigned short val;
{
  char buff[2];
  int res = 1;
  if (state & simHOSTBE) {
    buff[1] = ((val >>  0) & 0xFF);
    buff[0] = ((val >>  8) & 0xFF);
  } else {
    buff[0] = ((val >>  0) & 0xFF);
    buff[1] = ((val >>  8) & 0xFF);
  }
  if (fwrite(buff,2,1,fh) != 1) {
    sim_warning("Failed to write 2bytes to the profile file");
    res = 0;
  }
  return(res);
}

void
sim_close (quitting)
     int quitting;
{
#ifdef DEBUG
  printf("DBG: sim_close: entered (quitting = %d)\n",quitting);
#endif

  /* Cannot assume sim_kill() has been called */
  /* "quitting" is non-zero if we cannot hang on errors */

  /* Ensure that any resources allocated through the callback
     mechanism are released: */
  callback->shutdown(callback);

#if defined(PROFILE)
  if ((state & simPROFILE) && (profile_hist != NULL)) {
    unsigned short *p = profile_hist;
    FILE *pf = fopen("gmon.out","wb");
    int loop;

    if (pf == NULL)
     sim_warning("Failed to open \"gmon.out\" profile file");
    else {
      int ok;
#ifdef DEBUG
      printf("DBG: minpc = 0x%08X\n",(unsigned int)profile_minpc);
      printf("DBG: maxpc = 0x%08X\n",(unsigned int)profile_maxpc);
#endif /* DEBUG */
      ok = writeout32(pf,(unsigned int)profile_minpc);
      if (ok)
       ok = writeout32(pf,(unsigned int)profile_maxpc);
      if (ok)
       ok = writeout32(pf,(profile_nsamples * 2) + 12); /* size of sample buffer (+ header) */
#ifdef DEBUG
      printf("DBG: nsamples = %d (size = 0x%08X)\n",profile_nsamples,((profile_nsamples * 2) + 12));
#endif /* DEBUG */
      for (loop = 0; (ok && (loop < profile_nsamples)); loop++) {
        ok = writeout16(pf,profile_hist[loop]);
        if (!ok)
         break;
      }

      fclose(pf);
    }

    free(profile_hist);
    profile_hist = NULL;
    state &= ~simPROFILE;
  }
#endif /* PROFILE */

#if defined(TRACE)
  if (tracefh != stderr)
   fclose(tracefh);
  state &= ~simTRACE;
#endif /* TRACE */

  if (logfh != NULL && logfh != stdout && logfh != stderr)
   fclose(logfh);
  logfh = NULL;

  if (membank)
   free(membank); /* cfree not available on all hosts */
  membank = NULL;

  return;
}

void
sim_resume (step,signal)
     int step, signal;
{
#ifdef DEBUG
  printf("DBG: sim_resume entered: step = %d, signal = %d (membank = 0x%08X)\n",step,signal,membank);
#endif /* DEBUG */

  if (step)
   state |= simSTEP; /* execute only a single instruction */
  else
   state &= ~(simSTOP | simSTEP); /* execute until event */

  state |= (simHALTEX | simHALTIN); /* treat interrupt event as exception */

  /* Start executing instructions from the current state (set
     explicitly by register updates, or by sim_create_inferior): */

  simulate();
  return;
}

int
sim_write (addr,buffer,size)
     SIM_ADDR addr;
     unsigned char *buffer;
     int size;
{
  int index = size;
  uword64 vaddr = (uword64)addr;

  /* Return the number of bytes written, or zero if error. */
#ifdef DEBUG
  callback->printf_filtered(callback,"sim_write(0x%08X%08X,buffer,%d);\n",WORD64HI(addr),WORD64LO(addr),size);
#endif

  /* We provide raw read and write routines, since we do not want to
     count the GDB memory accesses in our statistics gathering. */

  /* There is a lot of code duplication in the individual blocks
     below, but the variables are declared locally to a block to give
     the optimiser the best chance of improving the code. We have to
     perform slow byte reads from the host memory, to ensure that we
     get the data into the correct endianness for the (simulated)
     target memory world. */

  /* Mask count to get odd byte, odd halfword, and odd word out of the
     way. We can then perform doubleword transfers to and from the
     simulator memory for optimum performance. */
  if (index && (index & 1)) {
    uword64 paddr;
    int cca;
    if (AddressTranslation(vaddr,isDATA,isSTORE,&paddr,&cca,isTARGET,isRAW)) {
      uword64 value = ((uword64)(*buffer++));
      StoreMemory(cca,AccessLength_BYTE,value,paddr,vaddr,isRAW);
    }
    vaddr++;
    index &= ~1; /* logical operations usually quicker than arithmetic on RISC systems */
  }
  if (index && (index & 2)) {
    uword64 paddr;
    int cca;
    if (AddressTranslation(vaddr,isDATA,isSTORE,&paddr,&cca,isTARGET,isRAW)) {
      uword64 value;
      /* We need to perform the following magic to ensure that that
         bytes are written into same byte positions in the target memory
         world, regardless of the endianness of the host. */
      if (BigEndianMem) {
        value =  ((uword64)(*buffer++) << 8);
        value |= ((uword64)(*buffer++) << 0);
      } else {
        value =  ((uword64)(*buffer++) << 0);
        value |= ((uword64)(*buffer++) << 8);
      }
      StoreMemory(cca,AccessLength_HALFWORD,value,paddr,vaddr,isRAW);
    }
    vaddr += 2;
    index &= ~2;
  }
  if (index && (index & 4)) {
    uword64 paddr;
    int cca;
    if (AddressTranslation(vaddr,isDATA,isSTORE,&paddr,&cca,isTARGET,isRAW)) {
      uword64 value;
      if (BigEndianMem) {
        value =  ((uword64)(*buffer++) << 24);
        value |= ((uword64)(*buffer++) << 16);
        value |= ((uword64)(*buffer++) << 8);
        value |= ((uword64)(*buffer++) << 0);
      } else {
        value =  ((uword64)(*buffer++) << 0);
        value |= ((uword64)(*buffer++) << 8);
        value |= ((uword64)(*buffer++) << 16);
        value |= ((uword64)(*buffer++) << 24);
      }
      StoreMemory(cca,AccessLength_WORD,value,paddr,vaddr,isRAW);
    }
    vaddr += 4;
    index &= ~4;
  }
  for (;index; index -= 8) {
    uword64 paddr;
    int cca;
    if (AddressTranslation(vaddr,isDATA,isSTORE,&paddr,&cca,isTARGET,isRAW)) {
      uword64 value;
      if (BigEndianMem) {
        value =  ((uword64)(*buffer++) << 56);
        value |= ((uword64)(*buffer++) << 48);
        value |= ((uword64)(*buffer++) << 40);
        value |= ((uword64)(*buffer++) << 32);
        value |= ((uword64)(*buffer++) << 24);
        value |= ((uword64)(*buffer++) << 16);
        value |= ((uword64)(*buffer++) << 8);
        value |= ((uword64)(*buffer++) << 0);
      } else {
        value =  ((uword64)(*buffer++) << 0);
        value |= ((uword64)(*buffer++) << 8);
        value |= ((uword64)(*buffer++) << 16);
        value |= ((uword64)(*buffer++) << 24);
        value |= ((uword64)(*buffer++) << 32);
        value |= ((uword64)(*buffer++) << 40);
        value |= ((uword64)(*buffer++) << 48);
        value |= ((uword64)(*buffer++) << 56);
      }
      StoreMemory(cca,AccessLength_DOUBLEWORD,value,paddr,vaddr,isRAW);
    }
    vaddr += 8;
  }

  return(size);
}

int
sim_read (addr,buffer,size)
     SIM_ADDR addr;
     unsigned char *buffer;
     int size;
{
  int index;

  /* Return the number of bytes read, or zero if error. */
#ifdef DEBUG
  callback->printf_filtered(callback,"sim_read(0x%08X%08X,buffer,%d);\n",WORD64HI(addr),WORD64LO(addr),size);
#endif /* DEBUG */

  /* TODO: Perform same optimisation as the sim_write() code
     above. NOTE: This will require a bit more work since we will need
     to ensure that the source physical address is doubleword aligned
     before, and then deal with trailing bytes. */
  for (index = 0; (index < size); index++) {
    uword64 vaddr,paddr,value;
    int cca;
    vaddr = (uword64)addr + index;
    if (AddressTranslation(vaddr,isDATA,isLOAD,&paddr,&cca,isTARGET,isRAW)) {
      value = LoadMemory(cca,AccessLength_BYTE,paddr,vaddr,isDATA,isRAW);
      buffer[index] = (unsigned char)(value&0xFF);
    } else
     break;
  }

  return(index);
}

void
sim_store_register (rn,memory)
     int rn;
     unsigned char *memory;
{
#ifdef DEBUG
  callback->printf_filtered(callback,"sim_store_register(%d,*memory=0x%08X%08X);\n",rn,*((unsigned int *)memory),*((unsigned int *)(memory + 4)));
#endif /* DEBUG */

  /* Unfortunately this suffers from the same problem as the register
     numbering one. We need to know what the width of each logical
     register number is for the architecture being simulated. */
  if (register_widths[rn] == 0)
   sim_warning("Invalid register width for %d (register store ignored)",rn);
  else {
    if (register_widths[rn] == 32)
     registers[rn] = host_read_word(memory);
    else
     registers[rn] = host_read_long(memory);
  }

  return;
}

void
sim_fetch_register (rn,memory)
     int rn;
     unsigned char *memory;
{
#ifdef DEBUG
  callback->printf_filtered(callback,"sim_fetch_register(%d=0x%08X%08X,mem) : place simulator registers into memory\n",rn,WORD64HI(registers[rn]),WORD64LO(registers[rn]));
#endif /* DEBUG */

  if (register_widths[rn] == 0)
   sim_warning("Invalid register width for %d (register fetch ignored)",rn);
  else {
    if (register_widths[rn] == 32)
     *((unsigned int *)memory) = host_swap_word(registers[rn] & 0xFFFFFFFF);
    else /* 64bit register */
     *((uword64 *)memory) = host_swap_long(registers[rn]);
  }
  return;
}

void
sim_stop_reason (reason,sigrc)
     enum sim_stop *reason;
     int *sigrc;
{
/* We can have "*reason = {sim_exited, sim_stopped, sim_signalled}", so
	sim_exited        *sigrc = argument to exit()
	sim_stopped       *sigrc = exception number
	sim_signalled     *sigrc = signal number
*/
  if (state & simEXCEPTION) {
    /* If "sim_signalled" is used, GDB expects normal SIGNAL numbers,
       and not the MIPS specific exception codes. */
#if 1
    /* For some reason, sending GDB a sim_signalled reason cause it to
       terminate out. */
    *reason = sim_stopped;
#else
    *reason = sim_signalled;
#endif
    switch ((CAUSE >> 2) & 0x1F) {
      case Interrupt:
       *sigrc = SIGINT; /* wrong type of interrupt, but it will do for the moment */
       break;

      case TLBModification:
      case TLBLoad:
      case TLBStore:
      case AddressLoad:
      case AddressStore:
      case InstructionFetch:
      case DataReference:
       *sigrc = SIGBUS;
       break;

      case ReservedInstruction:
      case CoProcessorUnusable:
       *sigrc = SIGILL;
       break;

      case IntegerOverflow:
      case FPE:
       *sigrc = SIGFPE;
       break;

      case Trap:
      case Watch:
      case SystemCall:
      case BreakPoint:
       *sigrc = SIGTRAP;
       break;

      default : /* Unknown internal exception */
       *sigrc = SIGQUIT;
       break;
    }
  } else if (state & simEXIT) {
#if 0
    printf("DBG: simEXIT (%d)\n",rcexit);
#endif
    *reason = sim_exited;
    *sigrc = rcexit;
  } else { /* assume single-stepping */
    *reason = sim_stopped;
    *sigrc = SIGTRAP;
  }
  state &= ~(simEXCEPTION | simEXIT);
  return;
}

void
sim_info (verbose)
     int verbose;
{
  /* Accessed from the GDB "info files" command: */

  callback->printf_filtered(callback,"MIPS %d-bit simulator\n",(PROCESSOR_64BIT ? 64 : 32));

  callback->printf_filtered(callback,"%s endian memory model\n",(BigEndianMem ? "Big" : "Little"));

  callback->printf_filtered(callback,"0x%08X bytes of memory at 0x%08X%08X\n",(unsigned int)membank_size,WORD64HI(membank_base),WORD64LO(membank_base));

#if !defined(FASTSIM)
  if (instruction_fetch_overflow != 0)
    callback->printf_filtered(callback,"Instruction fetches = 0x%08X%08X\n",instruction_fetch_overflow,instruction_fetches);
  else
    callback->printf_filtered(callback,"Instruction fetches = %d\n",instruction_fetches);
  callback->printf_filtered(callback,"Pipeline ticks = %d\n",pipeline_ticks);
  /* It would be a useful feature, if when performing multi-cycle
     simulations (rather than single-stepping) we keep the start and
     end times of the execution, so that we can give a performance
     figure for the simulator. */
#endif /* !FASTSIM */

  /* print information pertaining to MIPS ISA and architecture being simulated */
  /* things that may be interesting */
  /* instructions executed - if available */
  /* cycles executed - if available */
  /* pipeline stalls - if available */
  /* virtual time taken */
  /* profiling size */
  /* profiling frequency */
  /* profile minpc */
  /* profile maxpc */

  return;
}

int
sim_load (prog,from_tty)
     char *prog;
     int from_tty;
{
  /* Return non-zero if the caller should handle the load. Zero if
     we have loaded the image. */
  return(-1);
}

void
sim_create_inferior (start_address,argv,env)
     SIM_ADDR start_address;
     char **argv;
     char **env;
{
#ifdef DEBUG
  printf("DBG: sim_create_inferior entered: start_address = 0x%08X\n",start_address);
#endif /* DEBUG */

  /* Prepare to execute the program to be simulated */
  /* argv and env are NULL terminated lists of pointers */

#if 1
  PC = (uword64)start_address;
#else
  /* TODO: Sort this properly. SIM_ADDR may already be a 64bit value: */
  PC = SIGNEXTEND(start_address,32);
#endif
  /* NOTE: GDB normally sets the PC explicitly. However, this call is
     used by other clients of the simulator. */

  if (argv || env) {
#if 0 /* def DEBUG */
    callback->printf_filtered(callback,"sim_create_inferior() : passed arguments ignored\n");
    {
     char **cptr;
     for (cptr = argv; (cptr && *cptr); cptr++)
      printf("DBG: arg \"%s\"\n",*cptr);
    }
#endif /* DEBUG */
    /* We should really place the argv slot values into the argument
       registers, and onto the stack as required. However, this
       assumes that we have a stack defined, which is not necessarily
       true at the moment. */
  }

  return;
}

void
sim_kill ()
{
#if 1
  /* This routine should be for terminating any existing simulation
     thread. Since we are single-threaded only at the moment, this is
     not an issue. It should *NOT* be used to terminate the
     simulator. */
#else /* do *NOT* call sim_close */
  sim_close(1); /* Do not hang on errors */
  /* This would also be the point where any memory mapped areas used
     by the simulator should be released. */
#endif
  return;
}

int
sim_get_quit_code ()
{
  /* The standard MIPS PCS (Procedure Calling Standard) uses V0(r2) as
     the function return value. However, it may be more correct for
     this to return the argument to the exit() function (if
     called). */
  return(V0);
}

void
sim_set_callbacks (p)
     host_callback *p;
{
  callback = p;
  return;
}

typedef enum {e_terminate,e_help,e_setmemsize,e_reset} e_cmds;

static struct t_sim_command {
 e_cmds id;
 const char *name;
 const char *help;
} sim_commands[] = {
  {e_help,      "help",           ": Show MIPS simulator private commands"},
  {e_setmemsize,"set-memory-size","<n> : Specify amount of memory simulated"},
  {e_reset,     "reset-system",   ": Reset the simulated processor"},
  {e_terminate, NULL}
};

void
sim_do_command (cmd)
     char *cmd;
{
  struct t_sim_command *cptr;

  if (callback == NULL) {
    fprintf(stderr,"Simulator not enabled: \"target sim\" should be used to activate\n");
    return;
  }

  if (!(cmd && *cmd != '\0'))
   cmd = "help";

  /* NOTE: Accessed from the GDB "sim" commmand: */
  for (cptr = sim_commands; cptr && cptr->name; cptr++)
   if (strncmp(cmd,cptr->name,strlen(cptr->name)) == 0) {
     cmd += strlen(cptr->name);
     switch (cptr->id) {
       case e_help: /* no arguments */
        { /* no arguments */
          struct t_sim_command *lptr;
          callback->printf_filtered(callback,"List of MIPS simulator commands:\n");
          for (lptr = sim_commands; lptr->name; lptr++)
           callback->printf_filtered(callback,"%s %s\n",lptr->name,lptr->help);
        }
        break;

       case e_setmemsize: /* memory size argument */
        {
          unsigned int newsize = (unsigned int)getnum(cmd);
          sim_size(newsize);
        }
        break;

       case e_reset: /* no arguments */
        ColdReset();
        /* NOTE: See the comments in sim_open() relating to device
           initialisation. */
        break;

       default:
        callback->printf_filtered(callback,"FATAL: Matched \"%s\", but failed to match command id %d.\n",cmd,cptr->id);
        break;
     }
     break;
   }

  if (!(cptr->name))
    callback->printf_filtered(callback,"Error: \"%s\" is not a valid MIPS simulator command.\n",cmd);

  return;
}

/*---------------------------------------------------------------------------*/
/* NOTE: The following routines do not seem to be used by GDB at the
   moment. However, they may be useful to the standalone simulator
   world. */


/* The profiling format is described in the "gmon_out.h" header file */
void
sim_set_profile (n)
     int n;
{
#if defined(PROFILE)
  profile_frequency = n;
  state |= simPROFILE;
#endif /* PROFILE */
  return;
}

void
sim_set_profile_size (n)
     int n;
{
#if defined(PROFILE)
  if (state & simPROFILE) {
    int bsize;

    /* Since we KNOW that the memory banks are a power-of-2 in size: */
    profile_nsamples = power2(n);
    profile_minpc = membank_base;
    profile_maxpc = (membank_base + membank_size);

    /* Just in-case we are sampling every address: NOTE: The shift
       right of 2 is because we only have word-aligned PC addresses. */
    if (profile_nsamples > (membank_size >> 2))
     profile_nsamples = (membank_size >> 2);

    /* Since we are dealing with power-of-2 values: */
    profile_shift = (((membank_size >> 2) / profile_nsamples) - 1);

    bsize = (profile_nsamples * sizeof(unsigned short));
    if (profile_hist == NULL)
     profile_hist = (unsigned short *)calloc(64,(bsize / 64));
    else
     profile_hist = (unsigned short *)realloc(profile_hist,bsize);
    if (profile_hist == NULL) {
      sim_warning("Failed to allocate VM for profiling buffer (0x%08X bytes)",bsize);
      state &= ~simPROFILE;
    }
  }
#endif /* PROFILE */

  return;
}

void
sim_size(newsize)
     unsigned int newsize;
{
  char *new;
  /* Used by "run", and internally, to set the simulated memory size */
  if (newsize == 0) {
    callback->printf_filtered(callback,"Zero not valid: Memory size still 0x%08X bytes\n",membank_size);
    return;
  }
  newsize = power2(newsize);
  if (membank == NULL)
   new = (char *)calloc(64,(membank_size / 64));
  else
   new = (char *)realloc(membank,newsize);
  if (new == NULL) {
    if (membank == NULL)
     sim_error("Not enough VM for simulation memory of 0x%08X bytes",membank_size);
    else
     sim_warning("Failed to resize memory (still 0x%08X bytes)",membank_size);
  } else {
    membank_size = (unsigned)newsize;
    membank = new;
#if defined(PROFILE)
    /* Ensure that we sample across the new memory range */
    sim_set_profile_size(profile_nsamples);
#endif /* PROFILE */
  }

  return;
}

int
sim_trace()
{
  /* This routine is called by the "run" program, when detailed
     execution information is required. Rather than executing a single
     instruction, and looping around externally... we just start
     simulating, returning TRUE when the simulator stops (for whatever
     reason). */

#if defined(TRACE)
  /* Ensure tracing is enabled, if available */
  if (tracefh != NULL)
   state |= simTRACE;
#endif /* TRACE */

  state &= ~(simSTOP | simSTEP); /* execute until event */
  state |= (simHALTEX | simHALTIN); /* treat interrupt event as exception */
  /* Start executing instructions from the current state (set
     explicitly by register updates, or by sim_create_inferior): */
  simulate();

  return(1);
}

/*---------------------------------------------------------------------------*/
/*-- Private simulator support interface ------------------------------------*/
/*---------------------------------------------------------------------------*/

/* Simple monitor interface (currently setup for the IDT and PMON monitors) */
static void
sim_monitor(reason)
     unsigned int reason;
{
  /* The IDT monitor actually allows two instructions per vector
     slot. However, the simulator currently causes a trap on each
     individual instruction. We cheat, and lose the bottom bit. */
  reason >>= 1;

  /* The following callback functions are available, however the
     monitor we are simulating does not make use of them: get_errno,
     isatty, lseek, rename, system, time and unlink */
  switch (reason) {
    case 6: /* int open(char *path,int flags) */
      {
        const char *ptr;
        uword64 paddr;
        int cca;
        if (AddressTranslation(A0,isDATA,isLOAD,&paddr,&cca,isHOST,isREAL))
         V0 = callback->open(callback,(char *)((int)paddr),(int)A1);
        else
         sim_error("Attempt to pass pointer that does not reference simulated memory");
      }
      break;

    case 7: /* int read(int file,char *ptr,int len) */
      {
        const char *ptr;
        uword64 paddr;
        int cca;
        if (AddressTranslation(A1,isDATA,isLOAD,&paddr,&cca,isHOST,isREAL))
         V0 = callback->read(callback,(int)A0,(char *)((int)paddr),(int)A2);
        else
         sim_error("Attempt to pass pointer that does not reference simulated memory");
      }
      break;

    case 8: /* int write(int file,char *ptr,int len) */
      {
        const char *ptr;
        uword64 paddr;
        int cca;
        if (AddressTranslation(A1,isDATA,isLOAD,&paddr,&cca,isHOST,isREAL))
         V0 = callback->write(callback,(int)A0,(const char *)((int)paddr),(int)A2);
        else
         sim_error("Attempt to pass pointer that does not reference simulated memory");
      }
      break;

    case 10: /* int close(int file) */
      V0 = callback->close(callback,(int)A0);
      break;

    case 11: /* char inbyte(void) */
      {
        char tmp;
        if (callback->read_stdin(callback,&tmp,sizeof(char)) != sizeof(char)) {
          sim_error("Invalid return from character read");
          V0 = -1;
        }
        else
         V0 = tmp;
      }
      break;

    case 12: /* void outbyte(char chr) : write a byte to "stdout" */
      {
        char tmp = (char)(A0 & 0xFF);
        callback->write_stdout(callback,&tmp,sizeof(char));
      }
      break;

    case 17: /* void _exit() */
      sim_warning("sim_monitor(17): _exit(int reason) to be coded");
      state |= (simSTOP | simEXIT); /* stop executing code */
      rcexit = (unsigned int)(A0 & 0xFFFFFFFF);
      break;

    case 55: /* void get_mem_info(unsigned int *ptr) */
      /* in:  A0 = pointer to three word memory location */
      /* out: [A0 + 0] = size */
      /*      [A0 + 4] = instruction cache size */
      /*      [A0 + 8] = data cache size */
      {
        uword64 vaddr = A0;
        uword64 paddr, value;
        int cca;
        int failed = 0;

        /* NOTE: We use RAW memory writes here, but since we are not
           gathering statistics for the monitor calls we are simulating,
           it is not an issue. */

        /* Memory size */
        if (AddressTranslation(vaddr,isDATA,isSTORE,&paddr,&cca,isTARGET,isREAL)) {
          value = (uword64)membank_size;
          StoreMemory(cca,AccessLength_WORD,value,paddr,vaddr,isRAW);
          /* We re-do the address translations, in-case the block
             overlaps a memory boundary: */
          value = 0;
          vaddr += (AccessLength_WORD + 1);
          if (AddressTranslation(vaddr,isDATA,isSTORE,&paddr,&cca,isTARGET,isREAL)) {
            StoreMemory(cca,AccessLength_WORD,value,paddr,vaddr,isRAW);
            vaddr += (AccessLength_WORD + 1);
            if (AddressTranslation(vaddr,isDATA,isSTORE,&paddr,&cca,isTARGET,isREAL))
             StoreMemory(cca,AccessLength_WORD,value,paddr,vaddr,isRAW);
            else
             failed = -1;
          } else
           failed = -1;
        } else
         failed = -1;

        if (failed)
         sim_error("Invalid pointer passed into monitor call");
      }
      break;

    case 158 : /* PMON printf */
      /* in:  A0 = pointer to format string */
      /*      A1 = optional argument 1 */
      /*      A2 = optional argument 2 */
      /*      A3 = optional argument 3 */
      /* out: void */
      /* The following is based on the PMON printf source */
      {
        uword64 paddr;
        int cca;
        /* This isn't the quickest way, since we call the host print
           routine for every character almost. But it does avoid
           having to allocate and manage a temporary string buffer. */
        if (AddressTranslation(A0,isDATA,isLOAD,&paddr,&cca,isHOST,isREAL)) {
          char *s = (char *)((int)paddr);
          ut_reg *ap = &A1; /* 1st argument */
          /* TODO: Include check that we only use three arguments (A1, A2 and A3) */
          for (; *s;) {
            if (*s == '%') {
              char tmp[40];
              enum {FMT_RJUST, FMT_LJUST, FMT_RJUST0, FMT_CENTER} fmt = FMT_RJUST;
              int width = 0, trunc = 0, haddot = 0, longlong = 0;
              int base = 10;
              s++;
              for (; *s; s++) {
                if (strchr ("dobxXulscefg%", *s))
                  break;
		else if (*s == '-')
                  fmt = FMT_LJUST;
		else if (*s == '0')
                  fmt = FMT_RJUST0;
		else if (*s == '~')
                  fmt = FMT_CENTER;
		else if (*s == '*') {
                  if (haddot)
                    trunc = (int)*ap++;
                  else
                    width = (int)*ap++;
		} else if (*s >= '1' && *s <= '9') {
                  char *t;
                  unsigned int n;
                  for (t = s; isdigit (*s); s++);
                  strncpy (tmp, t, s - t);
                  tmp[s - t] = '\0';
                  n = (unsigned int)strtol(tmp,NULL,10);
                  if (haddot)
                   trunc = n;
                  else
                   width = n;
                  s--;
		} else if (*s == '.')
                  haddot = 1;
              }
              if (*s == '%') {
                callback->printf_filtered(callback,"%%");
              } else if (*s == 's') {
                if ((int)*ap != 0) {
                  if (AddressTranslation(*ap++,isDATA,isLOAD,&paddr,&cca,isHOST,isREAL)) {
                    char *p = (char *)((int)paddr);;
                    callback->printf_filtered(callback,p);
                  } else {
                    ap++;
                    sim_error("Attempt to pass pointer that does not reference simulated memory");
                  }
                }
		else
                  callback->printf_filtered(callback,"(null)");
              } else if (*s == 'c') {
                int n = (int)*ap++;
		callback->printf_filtered(callback,"%c",n);
              } else {
		if (*s == 'l') {
                  if (*++s == 'l') {
                    longlong = 1;
                    ++s;
                  }
		}
		if (strchr ("dobxXu", *s)) {
                  long long lv = (long long)*ap++;
                  if (*s == 'b')
                    callback->printf_filtered(callback,"<binary not supported>");
                  else {
                    sprintf(tmp,"%%%s%c",longlong ? "ll" : "",*s);
                    if (longlong)
                      callback->printf_filtered(callback,tmp,lv);
                    else
                      callback->printf_filtered(callback,tmp,(int)lv);
                  }
		} else if (strchr ("eEfgG", *s)) {
                  double dbl = (double)*ap++;
                  sprintf(tmp,"%%%d.%d%c",width,trunc,*s);
                  callback->printf_filtered(callback,tmp,dbl);
                  trunc = 0;
		}
              }
              s++;
            } else
             callback->printf_filtered(callback,"%c",*s++);
          }
        } else
         sim_error("Attempt to pass pointer that does not reference simulated memory");
      }
      break;

    default:
      sim_warning("TODO: sim_monitor(%d) : PC = 0x%08X%08X",reason,WORD64HI(IPC),WORD64LO(IPC));
      sim_warning("(Arguments : A0 = 0x%08X%08X : A1 = 0x%08X%08X : A2 = 0x%08X%08X : A3 = 0x%08X%08X)",WORD64HI(A0),WORD64LO(A0),WORD64HI(A1),WORD64LO(A1),WORD64HI(A2),WORD64LO(A2),WORD64HI(A3),WORD64LO(A3));
      break;
  }
  return;
}

void
sim_warning(fmt)
     char *fmt;
{
  va_list ap;
  va_start(ap,fmt);
  if (logfh != NULL) {
#if 1
    fprintf(logfh,"SIM Warning: ");
    fprintf(logfh,fmt,ap);
    fprintf(logfh,"\n");
#else /* we should provide a method of routing log messages to the simulator output stream */
    callback->printf_filtered(callback,"SIM Warning: ");
    callback->printf_filtered(callback,fmt,ap);
#endif
  }
  va_end(ap);
  SignalException(SimulatorFault,"");
  return;
}

void
sim_error(fmt)
     char *fmt;
{
  va_list ap;
  va_start(ap,fmt);
  callback->printf_filtered(callback,"SIM Error: ");
  callback->printf_filtered(callback,fmt,ap);
  va_end(ap);
  SignalException(SimulatorFault,"");
  return;
}

static unsigned int
power2(value)
     unsigned int value;
{
  int loop,tmp;

  /* Round *UP* to the nearest power-of-2 if not already one */
  if (value != (value & ~(value - 1))) {
    for (tmp = value, loop = 0; (tmp != 0); loop++)
     tmp >>= 1;
    value = (1 << loop);
  }

  return(value);
}

static long
getnum(value)
     char *value;
{
  long num;
  char *end;

  num = strtol(value,&end,10);
  if (end == value)
   callback->printf_filtered(callback,"Warning: Invalid number \"%s\" ignored, using zero\n",value);
  else {
    if (*end && ((tolower(*end) == 'k') || (tolower(*end) == 'm'))) {
      if (tolower(*end) == 'k')
       num *= (1 << 10);
      else
       num *= (1 << 20);
      end++;
    }
    if (*end)
     callback->printf_filtered(callback,"Warning: Spurious characters \"%s\" at end of number ignored\n",end);
  }

  return(num);
}

/*-- trace support ----------------------------------------------------------*/

/* The TRACE support is provided (if required) in the memory accessing
   routines. Since we are also providing the architecture specific
   features, the architecture simulation code can also deal with
   notifying the TRACE world of cache flushes, etc. Similarly we do
   not need to provide profiling support in the simulator engine,
   since we can sample in the instruction fetch control loop. By
   defining the TRACE manifest, we add tracing as a run-time
   option. */

#if defined(TRACE)
/* Tracing by default produces "din" format (as required by
   dineroIII). Each line of such a trace file *MUST* have a din label
   and address field. The rest of the line is ignored, so comments can
   be included if desired. The first field is the label which must be
   one of the following values:

	0       read data
        1       write data
        2       instruction fetch
        3       escape record (treated as unknown access type)
        4       escape record (causes cache flush)

   The address field is a 32bit (lower-case) hexadecimal address
   value. The address should *NOT* be preceded by "0x".

   The size of the memory transfer is not important when dealing with
   cache lines (as long as no more than a cache line can be
   transferred in a single operation :-), however more information
   could be given following the dineroIII requirement to allow more
   complete memory and cache simulators to provide better
   results. i.e. the University of Pisa has a cache simulator that can
   also take bus size and speed as (variable) inputs to calculate
   complete system performance (a much more useful ability when trying
   to construct an end product, rather than a processor). They
   currently have an ARM version of their tool called ChARM. */

static
void dotrace(tracefh,type,address,width,comment)
     FILE *tracefh;
     int type;
     unsigned int address;
     int width;
     char *comment;
{
  if (state & simTRACE) {
    va_list ap;
    fprintf(tracefh,"%d %08x ; width %d ; ",type,address,width);
    va_start(ap,comment);
    fprintf(tracefh,comment,ap);
    va_end(ap);
    fprintf(tracefh,"\n");
  }
  /* NOTE: Since the "din" format will only accept 32bit addresses, and
     we may be generating 64bit ones, we should put the hi-32bits of the
     address into the comment field. */

  /* TODO: Provide a buffer for the trace lines. We can then avoid
     performing writes until the buffer is filled, or the file is
     being closed. */

  /* NOTE: We could consider adding a comment field to the "din" file
     produced using type 3 markers (unknown access). This would then
     allow information about the program that the "din" is for, and
     the MIPs world that was being simulated, to be placed into the
     trace file. */

  return;
}
#endif /* TRACE */

/*---------------------------------------------------------------------------*/
/*-- host<->target transfers ------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* The following routines allow conditionals to be avoided during the
   simulation, at the cost of increasing the image and source size. */

static unsigned int
xfer_direct_word(memory)
     unsigned char *memory;
{
  return *((unsigned int *)memory);
}

static uword64
xfer_direct_long(memory)
     unsigned char *memory;
{
  return *((uword64 *)memory);
}

static unsigned int
swap_direct_word(data)
     unsigned int data;
{
  return data;
}

static uword64
swap_direct_long(data)
     uword64 data;
{
  return data;
}

static unsigned int
xfer_big_word(memory)
     unsigned char *memory;
{
  return ((memory[0] << 24) | (memory[1] << 16) | (memory[2] << 8) | memory[3]);
}

static uword64
xfer_big_long(memory)
     unsigned char *memory;
{
  return (((uword64)memory[0] << 56) | ((uword64)memory[1] << 48)
          | ((uword64)memory[2] << 40) | ((uword64)memory[3] << 32)
          | ((memory[4] << 24) & 0xff000000) | (memory[5] << 16)
	  | (memory[6] << 8) | memory[7]);
}

static unsigned int
xfer_little_word(memory)
     unsigned char *memory;
{
  return ((memory[3] << 24) | (memory[2] << 16) | (memory[1] << 8) | memory[0]);
}

static uword64
xfer_little_long(memory)
     unsigned char *memory;
{
  return (((uword64)memory[7] << 56) | ((uword64)memory[6] << 48)
          | ((uword64)memory[5] << 40) | ((uword64)memory[4] << 32)
          | (memory[3] << 24) | (memory[2] << 16) | (memory[1] << 8) | memory[0]);
}

static unsigned int
swap_word(data)
     unsigned int data;
{
  unsigned int result;
  result = data ^ ((data << 16) | (data >> 16));
  result &= ~0x00FF0000;
  data = (data << 24) | (data >> 8);
  return data ^ (result >> 8);
}

static uword64
swap_long(data)
     uword64 data;
{
  unsigned int tmphi = WORD64HI(data);
  unsigned int tmplo = WORD64LO(data);
  tmphi = swap_word(tmphi);
  tmplo = swap_word(tmplo);
  /* Now swap the HI and LO parts */
  return SET64LO(tmphi) | SET64HI(tmplo);
}

/*---------------------------------------------------------------------------*/
/*-- simulator engine -------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static void
ColdReset()
{
  /* RESET: Fixed PC address: */
  PC = (((uword64)0xFFFFFFFF<<32) | 0xBFC00000);
  /* The reset vector address is in the unmapped, uncached memory space. */

  SR &= ~(status_SR | status_TS | status_RP);
  SR |= (status_ERL | status_BEV);
  /* VR4300 starts in Big-Endian mode */
  CONFIG &= ~(config_EP_mask << config_EP_shift);
  CONFIG |= ((config_EP_D << config_EP_shift) | config_BE);
  /* TODO: The VR4300 CONFIG register is not modelled fully at the moment */

#if defined(HASFPU) && (GPRLEN == (64))
  /* Cheat and allow access to the complete register set immediately: */
  SR |= status_FR; /* 64bit registers */
#endif /* HASFPU and 64bit FP registers */

  /* Ensure that any instructions with pending register updates are
     cleared: */
  {
    int loop;
    for (loop = 0; (loop < PSLOTS); loop++)
     pending_slot_reg[loop] = (LAST_EMBED_REGNUM + 1);
    pending_in = pending_out = pending_total = 0;
  }

#if defined(HASFPU)
  /* Initialise the FPU registers to the unknown state */
  {
    int rn;
    for (rn = 0; (rn < 32); rn++)
     fpr_state[rn] = fmt_uninterpreted;
  }
#endif /* HASFPU */

  /* In reality this check should be performed at various points
     within the simulation, since it is possible to change the
     endianness of user programs. However, we perform the check here
     to ensure that the start-of-day values agree: */
  state |= (BigEndianCPU ? simBE : 0);
  if ((target_byte_order == 1234) != !(state & simBE)) {
    fprintf(stderr,"ColdReset: GDB (%s) and simulator (%s) do not agree on target endianness\n",
            target_byte_order == 1234 ? "little" : "big",
            state & simBE ? "big" : "little");
    exit(1);
  }

  if (!(state & simHOSTBE) == !(state & simBE)) {
    host_read_word = xfer_direct_word;
    host_read_long = xfer_direct_long;
    host_swap_word = swap_direct_word;
    host_swap_long = swap_direct_long;
  } else if (state & simHOSTBE) {
    host_read_word = xfer_little_word;
    host_read_long = xfer_little_long;
    host_swap_word = swap_word;
    host_swap_long = swap_long;
  } else { /* HOST little-endian */
    host_read_word = xfer_big_word;
    host_read_long = xfer_big_long;
    host_swap_word = swap_word;
    host_swap_long = swap_long;
  }

  return;
}

/* Description from page A-22 of the "MIPS IV Instruction Set" manual (revision 3.1) */
/* Translate a virtual address to a physical address and cache
   coherence algorithm describing the mechanism used to resolve the
   memory reference. Given the virtual address vAddr, and whether the
   reference is to Instructions ot Data (IorD), find the corresponding
   physical address (pAddr) and the cache coherence algorithm (CCA)
   used to resolve the reference. If the virtual address is in one of
   the unmapped address spaces the physical address and the CCA are
   determined directly by the virtual address. If the virtual address
   is in one of the mapped address spaces then the TLB is used to
   determine the physical address and access type; if the required
   translation is not present in the TLB or the desired access is not
   permitted the function fails and an exception is taken.

   NOTE: This function is extended to return an exception state. This,
   along with the exception generation is used to notify whether a
   valid address translation occured */

static int
AddressTranslation(vAddr,IorD,LorS,pAddr,CCA,host,raw)
     uword64 vAddr;
     int IorD;
     int LorS;
     uword64 *pAddr;
     int *CCA;
     int host;
     int raw;
{
  int res = -1; /* TRUE : Assume good return */

#ifdef DEBUG
  callback->printf_filtered(callback,"AddressTranslation(0x%08X%08X,%s,%s,...);\n",WORD64HI(vAddr),WORD64LO(vAddr),(IorD ? "isDATA" : "isINSTRUCTION"),(LorS ? "iSTORE" : "isLOAD"));
#endif

  /* Check that the address is valid for this memory model */

  /* For a simple (flat) memory model, we simply pass virtual
     addressess through (mostly) unchanged. */
  vAddr &= 0xFFFFFFFF;

  /* Treat the kernel memory spaces identically for the moment: */
  if ((membank_base == K1BASE) && (vAddr >= K0BASE) && (vAddr < (K0BASE + K0SIZE)))
    vAddr += (K1BASE - K0BASE);

  /* Also assume that the K1BASE memory wraps. This is required to
     allow the PMON run-time __sizemem() routine to function (without
     having to provide exception simulation). NOTE: A kludge to work
     around the fact that the monitor memory is currently held in the
     K1BASE space. */
  if (((vAddr < monitor_base) || (vAddr >= (monitor_base + monitor_size))) && (vAddr >= K1BASE && vAddr < (K1BASE + K1SIZE)))
    vAddr = (K1BASE | (vAddr & (membank_size - 1)));

  *pAddr = vAddr; /* default for isTARGET */
  *CCA = Uncached; /* not used for isHOST */

  /* NOTE: This is a duplicate of the code that appears in the
     LoadMemory and StoreMemory functions. They should be merged into
     a single function (that can be in-lined if required). */
  if ((vAddr >= membank_base) && (vAddr < (membank_base + membank_size))) {
    if (host)
     *pAddr = (int)&membank[((unsigned int)(vAddr - membank_base) & (membank_size - 1))];
  } else if ((vAddr >= monitor_base) && (vAddr < (monitor_base + monitor_size))) {
    if (host)
     *pAddr = (int)&monitor[((unsigned int)(vAddr - monitor_base) & (monitor_size - 1))];
  } else {
#if 1 /* def DEBUG */
    sim_warning("Failed: AddressTranslation(0x%08X%08X,%s,%s,...) IPC = 0x%08X%08X",WORD64HI(vAddr),WORD64LO(vAddr),(IorD ? "isDATA" : "isINSTRUCTION"),(LorS ? "isSTORE" : "isLOAD"),WORD64HI(IPC),WORD64LO(IPC));
#endif /* DEBUG */
    res = 0; /* AddressTranslation has failed */
    *pAddr = -1;
    if (!raw) /* only generate exceptions on real memory transfers */
     SignalException((LorS == isSTORE) ? AddressStore : AddressLoad);
    else
     sim_warning("AddressTranslation for %s %s from 0x%08X%08X failed",(IorD ? "data" : "instruction"),(LorS ? "store" : "load"),WORD64HI(vAddr),WORD64LO(vAddr));
  }

  return(res);
}

/* Description from page A-23 of the "MIPS IV Instruction Set" manual (revision 3.1) */
/* Prefetch data from memory. Prefetch is an advisory instruction for
   which an implementation specific action is taken. The action taken
   may increase performance, but must not change the meaning of the
   program, or alter architecturally-visible state. */
static void
Prefetch(CCA,pAddr,vAddr,DATA,hint)
     int CCA;
     uword64 pAddr;
     uword64 vAddr;
     int DATA;
     int hint;
{
#ifdef DEBUG
  callback->printf_filtered(callback,"Prefetch(%d,0x%08X%08X,0x%08X%08X,%d,%d);\n",CCA,WORD64HI(pAddr),WORD64LO(pAddr),WORD64HI(vAddr),WORD64LO(vAddr),DATA,hint);
#endif /* DEBUG */

  /* For our simple memory model we do nothing */
  return;
}

/* Description from page A-22 of the "MIPS IV Instruction Set" manual (revision 3.1) */
/* Load a value from memory. Use the cache and main memory as
   specified in the Cache Coherence Algorithm (CCA) and the sort of
   access (IorD) to find the contents of AccessLength memory bytes
   starting at physical location pAddr. The data is returned in the
   fixed width naturally-aligned memory element (MemElem). The
   low-order two (or three) bits of the address and the AccessLength
   indicate which of the bytes within MemElem needs to be given to the
   processor. If the memory access type of the reference is uncached
   then only the referenced bytes are read from memory and valid
   within the memory element. If the access type is cached, and the
   data is not present in cache, an implementation specific size and
   alignment block of memory is read and loaded into the cache to
   satisfy a load reference. At a minimum, the block is the entire
   memory element. */
static uword64
LoadMemory(CCA,AccessLength,pAddr,vAddr,IorD,raw)
     int CCA;
     int AccessLength;
     uword64 pAddr;
     uword64 vAddr;
     int IorD;
     int raw;
{
  uword64 value;

#ifdef DEBUG
  if (membank == NULL)
   callback->printf_filtered(callback,"DBG: LoadMemory(%d,%d,0x%08X%08X,0x%08X%08X,%s,%s)\n",CCA,AccessLength,WORD64HI(pAddr),WORD64LO(pAddr),WORD64HI(vAddr),WORD64LO(vAddr),(IorD ? "isDATA" : "isINSTRUCTION"),(raw ? "isRAW" : "isREAL"));
#endif /* DEBUG */

#if defined(WARN_MEM)
  if (CCA != uncached)
   sim_warning("LoadMemory CCA (%d) is not uncached (currently all accesses treated as cached)",CCA);

  if (((pAddr & LOADDRMASK) + AccessLength) > LOADDRMASK) {
    /* In reality this should be a Bus Error */
    sim_error("AccessLength of %d would extend over %dbit aligned boundary for physical address 0x%08X%08X\n",AccessLength,(LOADDRMASK + 1)<<2,WORD64HI(pAddr),WORD64LO(pAddr));
  }
#endif /* WARN_MEM */

  /* Decide which physical memory locations are being dealt with. At
     this point we should be able to split the pAddr bits into the
     relevant address map being simulated. If the "raw" variable is
     set, the memory read being performed should *NOT* update any I/O
     state or affect the CPU state. This also includes avoiding
     affecting statistics gathering. */

  /* If instruction fetch then we need to check that the two lo-order
     bits are zero, otherwise raise a InstructionFetch exception: */
  if ((IorD == isINSTRUCTION) && ((pAddr & 0x3) != 0))
   SignalException(InstructionFetch);
  else {
    unsigned int index;
    unsigned char *mem = NULL;

#if defined(TRACE)
    if (!raw)
     dotrace(tracefh,((IorD == isDATA) ? 0 : 2),(unsigned int)(pAddr&0xFFFFFFFF),(AccessLength + 1),"load%s",((IorD == isDATA) ? "" : " instruction"));
#endif /* TRACE */

    /* NOTE: Quicker methods of decoding the address space can be used
       when a real memory map is being simulated (i.e. using hi-order
       address bits to select device). */
    if ((pAddr >= membank_base) && (pAddr < (membank_base + membank_size))) {
      index = ((unsigned int)(pAddr - membank_base) & (membank_size - 1));
      mem = membank;
    } else if ((pAddr >= monitor_base) && (pAddr < (monitor_base + monitor_size))) {
      index = ((unsigned int)(pAddr - monitor_base) & (monitor_size - 1));
      mem = monitor;
    }
    if (mem == NULL)
     sim_error("Simulator memory not found for physical address 0x%08X%08X\n",WORD64HI(pAddr),WORD64LO(pAddr));
    else {
      /* If we obtained the endianness of the host, and it is the same
         as the target memory system we can optimise the memory
         accesses. However, without that information we must perform
         slow transfer, and hope that the compiler optimisation will
         merge successive loads. */
      value = 0; /* no data loaded yet */

      /* In reality we should always be loading a doubleword value (or
         word value in 32bit memory worlds). The external code then
         extracts the required bytes. However, to keep performance
         high we only load the required bytes into the relevant
         slots. */
      if (BigEndianMem)
       switch (AccessLength) { /* big-endian memory */
         case AccessLength_DOUBLEWORD :
          value |= ((uword64)mem[index++] << 56);
         case AccessLength_SEPTIBYTE :
          value |= ((uword64)mem[index++] << 48);
         case AccessLength_SEXTIBYTE :
          value |= ((uword64)mem[index++] << 40);
         case AccessLength_QUINTIBYTE :
          value |= ((uword64)mem[index++] << 32);
         case AccessLength_WORD :
          value |= ((unsigned int)mem[index++] << 24);
         case AccessLength_TRIPLEBYTE :
          value |= ((unsigned int)mem[index++] << 16);
         case AccessLength_HALFWORD :
          value |= ((unsigned int)mem[index++] << 8);
         case AccessLength_BYTE :
          value |= mem[index];
          break;
       }
      else {
        index += (AccessLength + 1);
        switch (AccessLength) { /* little-endian memory */
          case AccessLength_DOUBLEWORD :
           value |= ((uword64)mem[--index] << 56);
          case AccessLength_SEPTIBYTE :
           value |= ((uword64)mem[--index] << 48);
          case AccessLength_SEXTIBYTE :
           value |= ((uword64)mem[--index] << 40);
          case AccessLength_QUINTIBYTE :
           value |= ((uword64)mem[--index] << 32);
          case AccessLength_WORD :
           value |= ((uword64)mem[--index] << 24);
          case AccessLength_TRIPLEBYTE :
           value |= ((uword64)mem[--index] << 16);
          case AccessLength_HALFWORD :
           value |= ((uword64)mem[--index] << 8);
          case AccessLength_BYTE :
           value |= ((uword64)mem[--index] << 0);
           break;
        }
      }

#ifdef DEBUG
      printf("DBG: LoadMemory() : (offset %d) : value = 0x%08X%08X\n",(int)(pAddr & LOADDRMASK),WORD64HI(value),WORD64LO(value));
#endif /* DEBUG */

      /* TODO: We could try and avoid the shifts when dealing with raw
         memory accesses. This would mean updating the LoadMemory and
         StoreMemory routines to avoid shifting the data before
         returning or using it. */
      if (!raw) { /* do nothing for raw accessess */
        if (BigEndianMem)
         value <<= (((7 - (pAddr & LOADDRMASK)) - AccessLength) * 8);
        else /* little-endian only needs to be shifted up to the correct byte offset */
         value <<= ((pAddr & LOADDRMASK) * 8);
      }

#ifdef DEBUG
      printf("DBG: LoadMemory() : shifted value = 0x%08X%08X\n",WORD64HI(value),WORD64LO(value));
#endif /* DEBUG */
    }
  }

  return(value);
}

/* Description from page A-23 of the "MIPS IV Instruction Set" manual (revision 3.1) */
/* Store a value to memory. The specified data is stored into the
   physical location pAddr using the memory hierarchy (data caches and
   main memory) as specified by the Cache Coherence Algorithm
   (CCA). The MemElem contains the data for an aligned, fixed-width
   memory element (word for 32-bit processors, doubleword for 64-bit
   processors), though only the bytes that will actually be stored to
   memory need to be valid. The low-order two (or three) bits of pAddr
   and the AccessLength field indicates which of the bytes within the
   MemElem data should actually be stored; only these bytes in memory
   will be changed. */
static void
StoreMemory(CCA,AccessLength,MemElem,pAddr,vAddr,raw)
     int CCA;
     int AccessLength;
     uword64 MemElem;
     uword64 pAddr;
     uword64 vAddr;
     int raw;
{
#ifdef DEBUG
  callback->printf_filtered(callback,"DBG: StoreMemory(%d,%d,0x%08X%08X,0x%08X%08X,0x%08X%08X,%s)\n",CCA,AccessLength,WORD64HI(MemElem),WORD64LO(MemElem),WORD64HI(pAddr),WORD64LO(pAddr),WORD64HI(vAddr),WORD64LO(vAddr),(raw ? "isRAW" : "isREAL"));
#endif /* DEBUG */

#if defined(WARN_MEM)
  if (CCA != uncached)
   sim_warning("StoreMemory CCA (%d) is not uncached (currently all accesses treated as cached)",CCA);
 
  if (((pAddr & LOADDRMASK) + AccessLength) > LOADDRMASK)
   sim_error("AccessLength of %d would extend over %dbit aligned boundary for physical address 0x%08X%08X\n",AccessLength,(LOADDRMASK + 1)<<2,WORD64HI(pAddr),WORD64LO(pAddr));
#endif /* WARN_MEM */

#if defined(TRACE)
  if (!raw)
   dotrace(tracefh,1,(unsigned int)(pAddr&0xFFFFFFFF),(AccessLength + 1),"store");
#endif /* TRACE */

  /* See the comments in the LoadMemory routine about optimising
     memory accesses. Also if we wanted to make the simulator smaller,
     we could merge a lot of this code with the LoadMemory
     routine. However, this would slow the simulator down with
     run-time conditionals. */
  {
    unsigned int index;
    unsigned char *mem = NULL;

    if ((pAddr >= membank_base) && (pAddr < (membank_base + membank_size))) {
      index = ((unsigned int)(pAddr - membank_base) & (membank_size - 1));
      mem = membank;
    } else if ((pAddr >= monitor_base) && (pAddr < (monitor_base + monitor_size))) {
      index = ((unsigned int)(pAddr - monitor_base) & (monitor_size - 1));
      mem = monitor;
    }

    if (mem == NULL)
     sim_error("Simulator memory not found for physical address 0x%08X%08X\n",WORD64HI(pAddr),WORD64LO(pAddr));
    else {
      int shift = 0;

#ifdef DEBUG
      printf("DBG: StoreMemory: offset = %d MemElem = 0x%08X%08X\n",(unsigned int)(pAddr & LOADDRMASK),WORD64HI(MemElem),WORD64LO(MemElem));
#endif /* DEBUG */

      if (BigEndianMem) {
        if (raw)
         shift = ((7 - AccessLength) * 8);
        else /* real memory access */
         shift = ((pAddr & LOADDRMASK) * 8);
        MemElem <<= shift;
      } else {
        /* no need to shift raw little-endian data */
        if (!raw)
         MemElem >>= ((pAddr & LOADDRMASK) * 8);
      }

#ifdef DEBUG
      printf("DBG: StoreMemory: shift = %d MemElem = 0x%08X%08X\n",shift,WORD64HI(MemElem),WORD64LO(MemElem));
#endif /* DEBUG */

      if (BigEndianMem) {
        switch (AccessLength) { /* big-endian memory */
          case AccessLength_DOUBLEWORD :
           mem[index++] = (unsigned char)(MemElem >> 56);
           MemElem <<= 8;
          case AccessLength_SEPTIBYTE :
           mem[index++] = (unsigned char)(MemElem >> 56);
           MemElem <<= 8;
          case AccessLength_SEXTIBYTE :
           mem[index++] = (unsigned char)(MemElem >> 56);
           MemElem <<= 8;
          case AccessLength_QUINTIBYTE :
           mem[index++] = (unsigned char)(MemElem >> 56);
           MemElem <<= 8;
          case AccessLength_WORD :
           mem[index++] = (unsigned char)(MemElem >> 56);
           MemElem <<= 8;
          case AccessLength_TRIPLEBYTE :
           mem[index++] = (unsigned char)(MemElem >> 56);
           MemElem <<= 8;
          case AccessLength_HALFWORD :
           mem[index++] = (unsigned char)(MemElem >> 56);
           MemElem <<= 8;
          case AccessLength_BYTE :
           mem[index++] = (unsigned char)(MemElem >> 56);
           break;
        }
      } else {
        index += (AccessLength + 1);
        switch (AccessLength) { /* little-endian memory */
          case AccessLength_DOUBLEWORD :
           mem[--index] = (unsigned char)(MemElem >> 56);
          case AccessLength_SEPTIBYTE :
           mem[--index] = (unsigned char)(MemElem >> 48);
          case AccessLength_SEXTIBYTE :
           mem[--index] = (unsigned char)(MemElem >> 40);
          case AccessLength_QUINTIBYTE :
           mem[--index] = (unsigned char)(MemElem >> 32);
          case AccessLength_WORD :
           mem[--index] = (unsigned char)(MemElem >> 24);
          case AccessLength_TRIPLEBYTE :
           mem[--index] = (unsigned char)(MemElem >> 16);
          case AccessLength_HALFWORD :
           mem[--index] = (unsigned char)(MemElem >> 8);
          case AccessLength_BYTE :
           mem[--index] = (unsigned char)(MemElem >> 0);
           break;
        }
      }
    }
  }

  return;
}

/* Description from page A-26 of the "MIPS IV Instruction Set" manual (revision 3.1) */
/* Order loads and stores to synchronise shared memory. Perform the
   action necessary to make the effects of groups of synchronizable
   loads and stores indicated by stype occur in the same order for all
   processors. */
static void
SyncOperation(stype)
     int stype;
{
#ifdef DEBUG
  callback->printf_filtered(callback,"SyncOperation(%d) : TODO\n",stype);
#endif /* DEBUG */
  return;
}

/* Description from page A-26 of the "MIPS IV Instruction Set" manual (revision 3.1) */
/* Signal an exception condition. This will result in an exception
   that aborts the instruction. The instruction operation pseudocode
   will never see a return from this function call. */
static void
SignalException(exception)
     int exception;
{
  /* Ensure that any active atomic read/modify/write operation will fail: */
  LLBIT = 0;

  switch (exception) {
    /* TODO: For testing purposes I have been ignoring TRAPs. In
       reality we should either simulate them, or allow the user to
       ignore them at run-time. */
    case Trap :
     sim_warning("Ignoring instruction TRAP (PC 0x%08X%08X)",WORD64HI(IPC),WORD64LO(IPC));
     break;

    case ReservedInstruction :
     {
       va_list ap;
       unsigned int instruction;
       va_start(ap,exception);
       instruction = va_arg(ap,unsigned int);
       va_end(ap);
       /* Provide simple monitor support using ReservedInstruction
          exceptions. The following code simulates the fixed vector
          entry points into the IDT monitor by causing a simulator
          trap, performing the monitor operation, and returning to
          the address held in the $ra register (standard PCS return
          address). This means we only need to pre-load the vector
          space with suitable instruction values. For systems were
          actual trap instructions are used, we would not need to
          perform this magic. */
       if ((instruction & ~RSVD_INSTRUCTION_AMASK) == RSVD_INSTRUCTION) {
         sim_monitor(instruction & RSVD_INSTRUCTION_AMASK);
         PC = RA; /* simulate the return from the vector entry */
         /* NOTE: This assumes that a branch-and-link style
            instruction was used to enter the vector (which is the
            case with the current IDT monitor). */
         break; /* out of the switch statement */
       } /* else fall through to normal exception processing */
       sim_warning("ReservedInstruction 0x%08X at IPC = 0x%08X%08X",instruction,WORD64HI(IPC),WORD64LO(IPC));
     }

    default:
#ifdef DEBUG
     if (exception != BreakPoint)
      callback->printf_filtered(callback,"DBG: SignalException(%d) IPC = 0x%08X%08X\n",exception,WORD64HI(IPC),WORD64LO(IPC));
#endif /* DEBUG */
     /* Store exception code into current exception id variable (used
        by exit code): */

     /* TODO: If not simulating exceptions then stop the simulator
        execution. At the moment we always stop the simulation. */
     state |= (simSTOP | simEXCEPTION);

     /* Keep a copy of the current A0 in-case this is the program exit
        breakpoint:  */
     if (exception == BreakPoint) {
       va_list ap;
       unsigned int instruction;
       va_start(ap,exception);
       instruction = va_arg(ap,unsigned int);
       va_end(ap);
       /* Check for our special terminating BREAK: */
       if ((instruction & 0x03FFFFC0) == 0x03ff0000) {
         rcexit = (unsigned int)(A0 & 0xFFFFFFFF);
         state &= ~simEXCEPTION;
         state |= simEXIT;
       }
     }

     /* Store exception code into current exception id variable (used
        by exit code): */
     CAUSE = (exception << 2);
     if (state & simDELAYSLOT) {
       CAUSE |= cause_BD;
       EPC = (IPC - 4); /* reference the branch instruction */
     } else
      EPC = IPC;
     /* The following is so that the simulator will continue from the
        exception address on breakpoint operations. */
     PC = EPC;
     break;

    case SimulatorFault:
     {
       va_list ap;
       char *msg;
       va_start(ap,exception);
       msg = va_arg(ap,char *);
       fprintf(stderr,"FATAL: Simulator error \"%s\"\n",msg);
       va_end(ap);
     }
     exit(1);
   }

  return;
}

#if defined(WARN_RESULT)
/* Description from page A-26 of the "MIPS IV Instruction Set" manual (revision 3.1) */
/* This function indicates that the result of the operation is
   undefined. However, this should not affect the instruction
   stream. All that is meant to happen is that the destination
   register is set to an undefined result. To keep the simulator
   simple, we just don't bother updating the destination register, so
   the overall result will be undefined. If desired we can stop the
   simulator by raising a pseudo-exception. */
static void
UndefinedResult()
{
  sim_warning("UndefinedResult: IPC = 0x%08X%08X",WORD64HI(IPC),WORD64LO(IPC));
#if 0 /* Disabled for the moment, since it actually happens a lot at the moment. */
  state |= simSTOP;
#endif
  return;
}
#endif /* WARN_RESULT */

static void
CacheOp(op,pAddr,vAddr,instruction)
     int op;
     uword64 pAddr;
     uword64 vAddr;
     unsigned int instruction;
{
#if 1 /* stop warning message being displayed (we should really just remove the code) */
  static int icache_warning = 1;
  static int dcache_warning = 1;
#else
  static int icache_warning = 0;
  static int dcache_warning = 0;
#endif

  /* If CP0 is not useable (User or Supervisor mode) and the CP0
     enable bit in the Status Register is clear - a coprocessor
     unusable exception is taken. */
#if 0
  callback->printf_filtered(callback,"TODO: Cache availability checking (PC = 0x%08X%08X)\n",WORD64HI(IPC),WORD64LO(IPC));
#endif

  switch (op & 0x3) {
    case 0: /* instruction cache */
      switch (op >> 2) {
        case 0: /* Index Invalidate */
        case 1: /* Index Load Tag */
        case 2: /* Index Store Tag */
        case 4: /* Hit Invalidate */
        case 5: /* Fill */
        case 6: /* Hit Writeback */
          if (!icache_warning)
            {
              sim_warning("Instruction CACHE operation %d to be coded",(op >> 2));
              icache_warning = 1;
            }
          break;

        default:
          SignalException(ReservedInstruction,instruction);
          break;
      }
      break;

    case 1: /* data cache */
      switch (op >> 2) {
        case 0: /* Index Writeback Invalidate */
        case 1: /* Index Load Tag */
        case 2: /* Index Store Tag */
        case 3: /* Create Dirty */
        case 4: /* Hit Invalidate */
        case 5: /* Hit Writeback Invalidate */
        case 6: /* Hit Writeback */ 
          if (!dcache_warning)
            {
              sim_warning("Data CACHE operation %d to be coded",(op >> 2));
              dcache_warning = 1;
            }
          break;

        default:
          SignalException(ReservedInstruction,instruction);
          break;
      }
      break;

    default: /* unrecognised cache ID */
      SignalException(ReservedInstruction,instruction);
      break;
  }

  return;
}

/*-- FPU support routines ---------------------------------------------------*/

#if defined(HASFPU) /* Only needed when building FPU aware simulators */

#if 1
#define SizeFGR() (GPRLEN)
#else
/* They depend on the CPU being simulated */
#define SizeFGR() ((PROCESSOR_64BIT && ((SR & status_FR) == 1)) ? 64 : 32)
#endif

/* Numbers are held in normalized form. The SINGLE and DOUBLE binary
   formats conform to ANSI/IEEE Std 754-1985. */
/* SINGLE precision floating:
 *    seeeeeeeefffffffffffffffffffffff
 *      s =  1bit  = sign
 *      e =  8bits = exponent
 *      f = 23bits = fraction
 */
/* SINGLE precision fixed:
 *    siiiiiiiiiiiiiiiiiiiiiiiiiiiiiii
 *      s =  1bit  = sign
 *      i = 31bits = integer
 */
/* DOUBLE precision floating:
 *    seeeeeeeeeeeffffffffffffffffffffffffffffffffffffffffffffffffffff
 *      s =  1bit  = sign
 *      e = 11bits = exponent
 *      f = 52bits = fraction
 */
/* DOUBLE precision fixed:
 *    siiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii
 *      s =  1bit  = sign
 *      i = 63bits = integer
 */

/* Extract sign-bit: */
#define FP_S_s(v)    (((v) & ((unsigned)1 << 31)) ? 1 : 0)
#define FP_D_s(v)    (((v) & ((uword64)1 << 63)) ? 1 : 0)
/* Extract biased exponent: */
#define FP_S_be(v)   (((v) >> 23) & 0xFF)
#define FP_D_be(v)   (((v) >> 52) & 0x7FF)
/* Extract unbiased Exponent: */
#define FP_S_e(v)    (FP_S_be(v) - 0x7F)
#define FP_D_e(v)    (FP_D_be(v) - 0x3FF)
/* Extract complete fraction field: */
#define FP_S_f(v)    ((v) & ~((unsigned)0x1FF << 23))
#define FP_D_f(v)    ((v) & ~((uword64)0xFFF << 52))
/* Extract numbered fraction bit: */
#define FP_S_fb(b,v) (((v) & (1 << (23 - (b)))) ? 1 : 0)
#define FP_D_fb(b,v) (((v) & (1 << (52 - (b)))) ? 1 : 0)

/* Explicit QNaN values used when value required: */
#define FPQNaN_SINGLE   (0x7FBFFFFF)
#define FPQNaN_WORD     (0x7FFFFFFF)
#define FPQNaN_DOUBLE   (((uword64)0x7FF7FFFF << 32) | 0xFFFFFFFF)
#define FPQNaN_LONG     (((uword64)0x7FFFFFFF << 32) | 0xFFFFFFFF)

/* Explicit Infinity values used when required: */
#define FPINF_SINGLE    (0x7F800000)
#define FPINF_DOUBLE    (((uword64)0x7FF00000 << 32) | 0x00000000)

#if 1 /* def DEBUG */
#define RMMODE(v) (((v) == FP_RM_NEAREST) ? "Round" : (((v) == FP_RM_TOZERO) ? "Trunc" : (((v) == FP_RM_TOPINF) ? "Ceil" : "Floor")))
#define DOFMT(v)  (((v) == fmt_single) ? "single" : (((v) == fmt_double) ? "double" : (((v) == fmt_word) ? "word" : (((v) == fmt_long) ? "long" : (((v) == fmt_unknown) ? "<unknown>" : (((v) == fmt_uninterpreted) ? "<uninterpreted>" : "<format error>"))))))
#endif /* DEBUG */

static uword64
ValueFPR(fpr,fmt)
         int fpr;
         FP_formats fmt;
{
  uword64 value;
  int err = 0;

  /* Treat unused register values, as fixed-point 64bit values: */
  if ((fmt == fmt_uninterpreted) || (fmt == fmt_unknown))
#if 1
   /* If request to read data as "uninterpreted", then use the current
      encoding: */
   fmt = fpr_state[fpr];
#else
   fmt = fmt_long;
#endif

  /* For values not yet accessed, set to the desired format: */
  if (fpr_state[fpr] == fmt_uninterpreted) {
    fpr_state[fpr] = fmt;
#ifdef DEBUG
    printf("DBG: Register %d was fmt_uninterpreted. Now %s\n",fpr,DOFMT(fmt));
#endif /* DEBUG */
  }
  if (fmt != fpr_state[fpr]) {
    sim_warning("FPR %d (format %s) being accessed with format %s - setting to unknown (PC = 0x%08X%08X)",fpr,DOFMT(fpr_state[fpr]),DOFMT(fmt),WORD64HI(IPC),WORD64LO(IPC));
    fpr_state[fpr] = fmt_unknown;
  }

  if (fpr_state[fpr] == fmt_unknown) {
   /* Set QNaN value: */
   switch (fmt) {
    case fmt_single:
     value = FPQNaN_SINGLE;
     break;

    case fmt_double:
     value = FPQNaN_DOUBLE;
     break;

    case fmt_word:
     value = FPQNaN_WORD;
     break;

    case fmt_long:
     value = FPQNaN_LONG;
     break;

    default:
     err = -1;
     break;
   }
  } else if (SizeFGR() == 64) {
    switch (fmt) {
     case fmt_single:
     case fmt_word:
      value = (FGR[fpr] & 0xFFFFFFFF);
      break;

     case fmt_uninterpreted:
     case fmt_double:
     case fmt_long:
      value = FGR[fpr];
      break;

     default :
      err = -1;
      break;
    }
  } else if ((fpr & 1) == 0) { /* even registers only */
    switch (fmt) {
     case fmt_single:
     case fmt_word:
      value = (FGR[fpr] & 0xFFFFFFFF);
      break;

     case fmt_uninterpreted:
     case fmt_double:
     case fmt_long:
      value = ((((uword64)FGR[fpr+1]) << 32) | (FGR[fpr] & 0xFFFFFFFF));
      break;

     default :
      err = -1;
      break;
    }
  }

  if (err)
   SignalException(SimulatorFault,"Unrecognised FP format in ValueFPR()");

#ifdef DEBUG
  printf("DBG: ValueFPR: fpr = %d, fmt = %s, value = 0x%08X%08X : PC = 0x%08X%08X : SizeFGR() = %d\n",fpr,DOFMT(fmt),WORD64HI(value),WORD64LO(value),WORD64HI(IPC),WORD64LO(IPC),SizeFGR());
#endif /* DEBUG */

  return(value);
}

static void
StoreFPR(fpr,fmt,value)
     int fpr;
     FP_formats fmt;
     uword64 value;
{
  int err = 0;

#ifdef DEBUG
  printf("DBG: StoreFPR: fpr = %d, fmt = %s, value = 0x%08X%08X : PC = 0x%08X%08X : SizeFGR() = %d\n",fpr,DOFMT(fmt),WORD64HI(value),WORD64LO(value),WORD64HI(IPC),WORD64LO(IPC),SizeFGR());
#endif /* DEBUG */

  if (SizeFGR() == 64) {
    switch (fmt) {
      case fmt_single :
      case fmt_word :
       FGR[fpr] = (((uword64)0xDEADC0DE << 32) | (value & 0xFFFFFFFF));
       fpr_state[fpr] = fmt;
       break;

      case fmt_uninterpreted:
      case fmt_double :
      case fmt_long :
       FGR[fpr] = value;
       fpr_state[fpr] = fmt;
       break;

      default :
       fpr_state[fpr] = fmt_unknown;
       err = -1;
       break;
    }
  } else if ((fpr & 1) == 0) { /* even register number only */
    switch (fmt) {
      case fmt_single :
      case fmt_word :
       FGR[fpr+1] = 0xDEADC0DE;
       FGR[fpr] = (value & 0xFFFFFFFF);
       fpr_state[fpr + 1] = fmt;
       fpr_state[fpr] = fmt;
       break;

      case fmt_uninterpreted:
      case fmt_double :
      case fmt_long :
       FGR[fpr+1] = (value >> 32);
       FGR[fpr] = (value & 0xFFFFFFFF);
       fpr_state[fpr + 1] = fmt;
       fpr_state[fpr] = fmt;
       break;

      default :
       fpr_state[fpr] = fmt_unknown;
       err = -1;
       break;
    }
  }
#if defined(WARN_RESULT)
  else
    UndefinedResult();
#endif /* WARN_RESULT */

  if (err)
   SignalException(SimulatorFault,"Unrecognised FP format in StoreFPR()");

#ifdef DEBUG
  printf("DBG: StoreFPR: fpr[%d] = 0x%08X%08X (format %s)\n",fpr,WORD64HI(FGR[fpr]),WORD64LO(FGR[fpr]),DOFMT(fmt));
#endif /* DEBUG */

  return;
}

static int
NaN(op,fmt)
     uword64 op;
     FP_formats fmt; 
{
  int boolean = 0;

  /* Check if (((E - bias) == (E_max + 1)) && (fraction != 0)). We
     know that the exponent field is biased... we we cheat and avoid
     removing the bias value. */
  switch (fmt) {
   case fmt_single:
    boolean = ((FP_S_be(op) == 0xFF) && (FP_S_f(op) != 0));
    /* We could use "FP_S_fb(1,op)" to ascertain whether we are
       dealing with a SNaN or QNaN */
    break;
   case fmt_double:
    boolean = ((FP_D_be(op) == 0x7FF) && (FP_D_f(op) != 0));
    /* We could use "FP_S_fb(1,op)" to ascertain whether we are
       dealing with a SNaN or QNaN */
    break;
   case fmt_word:
    boolean = (op == FPQNaN_WORD);
    break;
   case fmt_long:
    boolean = (op == FPQNaN_LONG);
    break;
  }

#ifdef DEBUG
printf("DBG: NaN: returning %d for 0x%08X%08X (format = %s)\n",boolean,WORD64HI(op),WORD64LO(op),DOFMT(fmt));
#endif /* DEBUG */

  return(boolean);
}

static int
Infinity(op,fmt)
     uword64 op;
     FP_formats fmt; 
{
  int boolean = 0;

#ifdef DEBUG
  printf("DBG: Infinity: format %s 0x%08X%08X (PC = 0x%08X%08X)\n",DOFMT(fmt),WORD64HI(op),WORD64LO(op),WORD64HI(IPC),WORD64LO(IPC));
#endif /* DEBUG */

  /* Check if (((E - bias) == (E_max + 1)) && (fraction == 0)). We
     know that the exponent field is biased... we we cheat and avoid
     removing the bias value. */
  switch (fmt) {
   case fmt_single:
    boolean = ((FP_S_be(op) == 0xFF) && (FP_S_f(op) == 0));
    break;
   case fmt_double:
    boolean = ((FP_D_be(op) == 0x7FF) && (FP_D_f(op) == 0));
    break;
   default:
    printf("DBG: TODO: unrecognised format (%s) for Infinity check\n",DOFMT(fmt));
    break;
  }

#ifdef DEBUG
  printf("DBG: Infinity: returning %d for 0x%08X%08X (format = %s)\n",boolean,WORD64HI(op),WORD64LO(op),DOFMT(fmt));
#endif /* DEBUG */

  return(boolean);
}

static int
Less(op1,op2,fmt)
     uword64 op1;
     uword64 op2;
     FP_formats fmt; 
{
  int boolean = 0;

  /* Argument checking already performed by the FPCOMPARE code */

#ifdef DEBUG
  printf("DBG: Less: %s: op1 = 0x%08X%08X : op2 = 0x%08X%08X\n",DOFMT(fmt),WORD64HI(op1),WORD64LO(op1),WORD64HI(op2),WORD64LO(op2));
#endif /* DEBUG */

  /* The format type should already have been checked: */
  switch (fmt) {
   case fmt_single:
    {
      unsigned int wop1 = (unsigned int)op1;
      unsigned int wop2 = (unsigned int)op2;
      boolean = (*(float *)&wop1 < *(float *)&wop2);
    }
    break;
   case fmt_double:
    boolean = (*(double *)&op1 < *(double *)&op2);
    break;
  }

#ifdef DEBUG
  printf("DBG: Less: returning %d (format = %s)\n",boolean,DOFMT(fmt));
#endif /* DEBUG */

  return(boolean);
}

static int
Equal(op1,op2,fmt)
     uword64 op1;
     uword64 op2;
     FP_formats fmt; 
{
  int boolean = 0;

  /* Argument checking already performed by the FPCOMPARE code */

#ifdef DEBUG
  printf("DBG: Equal: %s: op1 = 0x%08X%08X : op2 = 0x%08X%08X\n",DOFMT(fmt),WORD64HI(op1),WORD64LO(op1),WORD64HI(op2),WORD64LO(op2));
#endif /* DEBUG */

  /* The format type should already have been checked: */
  switch (fmt) {
   case fmt_single:
    boolean = ((op1 & 0xFFFFFFFF) == (op2 & 0xFFFFFFFF));
    break;
   case fmt_double:
    boolean = (op1 == op2);
    break;
  }

#ifdef DEBUG
  printf("DBG: Equal: returning %d (format = %s)\n",boolean,DOFMT(fmt));
#endif /* DEBUG */

  return(boolean);
}

static uword64
AbsoluteValue(op,fmt)
     uword64 op;
     FP_formats fmt; 
{
  uword64 result;

#ifdef DEBUG
  printf("DBG: AbsoluteValue: %s: op = 0x%08X%08X\n",DOFMT(fmt),WORD64HI(op),WORD64LO(op));
#endif /* DEBUG */

  /* The format type should already have been checked: */
  switch (fmt) {
   case fmt_single:
    {
      unsigned int wop = (unsigned int)op;
      float tmp = ((float)fabs((double)*(float *)&wop));
      result = (uword64)*(unsigned int *)&tmp;
    }
    break;
   case fmt_double:
    {
      double tmp = (fabs(*(double *)&op));
      result = *(uword64 *)&tmp;
    }
  }

  return(result);
}

static uword64
Negate(op,fmt)
     uword64 op;
     FP_formats fmt; 
{
  uword64 result;

#ifdef DEBUG
  printf("DBG: Negate: %s: op = 0x%08X%08X\n",DOFMT(fmt),WORD64HI(op),WORD64LO(op));
#endif /* DEBUG */

  /* The format type should already have been checked: */
  switch (fmt) {
   case fmt_single:
    {
      unsigned int wop = (unsigned int)op;
      float tmp = ((float)0.0 - *(float *)&wop);
      result = (uword64)*(unsigned int *)&tmp;
    }
    break;
   case fmt_double:
    {
      double tmp = ((double)0.0 - *(double *)&op);
      result = *(uword64 *)&tmp;
    }
    break;
  }

  return(result);
}

static uword64
Add(op1,op2,fmt)
     uword64 op1;
     uword64 op2;
     FP_formats fmt; 
{
  uword64 result;

#ifdef DEBUG
  printf("DBG: Add: %s: op1 = 0x%08X%08X : op2 = 0x%08X%08X\n",DOFMT(fmt),WORD64HI(op1),WORD64LO(op1),WORD64HI(op2),WORD64LO(op2));
#endif /* DEBUG */

  /* The registers must specify FPRs valid for operands of type
     "fmt". If they are not valid, the result is undefined. */

  /* The format type should already have been checked: */
  switch (fmt) {
   case fmt_single:
    {
      unsigned int wop1 = (unsigned int)op1;
      unsigned int wop2 = (unsigned int)op2;
      float tmp = (*(float *)&wop1 + *(float *)&wop2);
      result = (uword64)*(unsigned int *)&tmp;
    }
    break;
   case fmt_double:
    {
      double tmp = (*(double *)&op1 + *(double *)&op2);
      result = *(uword64 *)&tmp;
    }
    break;
  }

#ifdef DEBUG
  printf("DBG: Add: returning 0x%08X%08X (format = %s)\n",WORD64HI(result),WORD64LO(result),DOFMT(fmt));
#endif /* DEBUG */

  return(result);
}

static uword64
Sub(op1,op2,fmt)
     uword64 op1;
     uword64 op2;
     FP_formats fmt; 
{
  uword64 result;

#ifdef DEBUG
  printf("DBG: Sub: %s: op1 = 0x%08X%08X : op2 = 0x%08X%08X\n",DOFMT(fmt),WORD64HI(op1),WORD64LO(op1),WORD64HI(op2),WORD64LO(op2));
#endif /* DEBUG */

  /* The registers must specify FPRs valid for operands of type
     "fmt". If they are not valid, the result is undefined. */

  /* The format type should already have been checked: */
  switch (fmt) {
   case fmt_single:
    {
      unsigned int wop1 = (unsigned int)op1;
      unsigned int wop2 = (unsigned int)op2;
      float tmp = (*(float *)&wop1 - *(float *)&wop2);
      result = (uword64)*(unsigned int *)&tmp;
    }
    break;
   case fmt_double:
    {
      double tmp = (*(double *)&op1 - *(double *)&op2);
      result = *(uword64 *)&tmp;
    }
    break;
  }

#ifdef DEBUG
  printf("DBG: Sub: returning 0x%08X%08X (format = %s)\n",WORD64HI(result),WORD64LO(result),DOFMT(fmt));
#endif /* DEBUG */

  return(result);
}

static uword64
Multiply(op1,op2,fmt)
     uword64 op1;
     uword64 op2;
     FP_formats fmt; 
{
  uword64 result;

#ifdef DEBUG
  printf("DBG: Multiply: %s: op1 = 0x%08X%08X : op2 = 0x%08X%08X\n",DOFMT(fmt),WORD64HI(op1),WORD64LO(op1),WORD64HI(op2),WORD64LO(op2));
#endif /* DEBUG */

  /* The registers must specify FPRs valid for operands of type
     "fmt". If they are not valid, the result is undefined. */

  /* The format type should already have been checked: */
  switch (fmt) {
   case fmt_single:
    {
      unsigned int wop1 = (unsigned int)op1;
      unsigned int wop2 = (unsigned int)op2;
      float tmp = (*(float *)&wop1 * *(float *)&wop2);
      result = (uword64)*(unsigned int *)&tmp;
    }
    break;
   case fmt_double:
    {
      double tmp = (*(double *)&op1 * *(double *)&op2);
      result = *(uword64 *)&tmp;
    }
    break;
  }

#ifdef DEBUG
  printf("DBG: Multiply: returning 0x%08X%08X (format = %s)\n",WORD64HI(result),WORD64LO(result),DOFMT(fmt));
#endif /* DEBUG */

  return(result);
}

static uword64
Divide(op1,op2,fmt)
     uword64 op1;
     uword64 op2;
     FP_formats fmt; 
{
  uword64 result;

#ifdef DEBUG
  printf("DBG: Divide: %s: op1 = 0x%08X%08X : op2 = 0x%08X%08X\n",DOFMT(fmt),WORD64HI(op1),WORD64LO(op1),WORD64HI(op2),WORD64LO(op2));
#endif /* DEBUG */

  /* The registers must specify FPRs valid for operands of type
     "fmt". If they are not valid, the result is undefined. */

  /* The format type should already have been checked: */
  switch (fmt) {
   case fmt_single:
    {
      unsigned int wop1 = (unsigned int)op1;
      unsigned int wop2 = (unsigned int)op2;
      float tmp = (*(float *)&wop1 / *(float *)&wop2);
      result = (uword64)*(unsigned int *)&tmp;
    }
    break;
   case fmt_double:
    {
      double tmp = (*(double *)&op1 / *(double *)&op2);
      result = *(uword64 *)&tmp;
    }
    break;
  }

#ifdef DEBUG
  printf("DBG: Divide: returning 0x%08X%08X (format = %s)\n",WORD64HI(result),WORD64LO(result),DOFMT(fmt));
#endif /* DEBUG */

  return(result);
}

static uword64
Recip(op,fmt)
     uword64 op;
     FP_formats fmt; 
{
  uword64 result;

#ifdef DEBUG
  printf("DBG: Recip: %s: op = 0x%08X%08X\n",DOFMT(fmt),WORD64HI(op),WORD64LO(op));
#endif /* DEBUG */

  /* The registers must specify FPRs valid for operands of type
     "fmt". If they are not valid, the result is undefined. */

  /* The format type should already have been checked: */
  switch (fmt) {
   case fmt_single:
    {
      unsigned int wop = (unsigned int)op;
      float tmp = ((float)1.0 / *(float *)&wop);
      result = (uword64)*(unsigned int *)&tmp;
    }
    break;
   case fmt_double:
    {
      double tmp = ((double)1.0 / *(double *)&op);
      result = *(uword64 *)&tmp;
    }
    break;
  }

#ifdef DEBUG
  printf("DBG: Recip: returning 0x%08X%08X (format = %s)\n",WORD64HI(result),WORD64LO(result),DOFMT(fmt));
#endif /* DEBUG */

  return(result);
}

static uword64
SquareRoot(op,fmt)
     uword64 op;
     FP_formats fmt; 
{
  uword64 result;

#ifdef DEBUG
  printf("DBG: SquareRoot: %s: op = 0x%08X%08X\n",DOFMT(fmt),WORD64HI(op),WORD64LO(op));
#endif /* DEBUG */

  /* The registers must specify FPRs valid for operands of type
     "fmt". If they are not valid, the result is undefined. */

  /* The format type should already have been checked: */
  switch (fmt) {
   case fmt_single:
    {
      unsigned int wop = (unsigned int)op;
#ifdef HAVE_SQRT
      float tmp = ((float)sqrt((double)*(float *)&wop));
      result = (uword64)*(unsigned int *)&tmp;
#else
      /* TODO: Provide square-root */
      result = (uword64)0;
#endif
    }
    break;
   case fmt_double:
    {
#ifdef HAVE_SQRT
      double tmp = (sqrt(*(double *)&op));
      result = *(uword64 *)&tmp;
#else
      /* TODO: Provide square-root */
      result = (uword64)0;
#endif
    }
    break;
  }

#ifdef DEBUG
  printf("DBG: SquareRoot: returning 0x%08X%08X (format = %s)\n",WORD64HI(result),WORD64LO(result),DOFMT(fmt));
#endif /* DEBUG */

  return(result);
}

static uword64
Convert(rm,op,from,to)
     int rm;
     uword64 op;
     FP_formats from; 
     FP_formats to; 
{
  uword64 result;

#ifdef DEBUG
  printf("DBG: Convert: mode %s : op 0x%08X%08X : from %s : to %s : (PC = 0x%08X%08X)\n",RMMODE(rm),WORD64HI(op),WORD64LO(op),DOFMT(from),DOFMT(to),WORD64HI(IPC),WORD64LO(IPC));
#endif /* DEBUG */

  /* The value "op" is converted to the destination format, rounding
     using mode "rm". When the destination is a fixed-point format,
     then a source value of Infinity, NaN or one which would round to
     an integer outside the fixed point range then an IEEE Invalid
     Operation condition is raised. */
  switch (to) {
   case fmt_single:
    {
      float tmp;
      switch (from) {
       case fmt_double:
        tmp = (float)(*(double *)&op);
        break;

       case fmt_word:
        tmp = (float)((int)(op & 0xFFFFFFFF));
        break;

       case fmt_long:
        tmp = (float)((word64)op);
        break;
      }

#if 0
      /* FIXME: This code is incorrect.  The rounding mode does not
         round to integral values; it rounds to the nearest
         representable value in the format.  */

      switch (rm) {
       case FP_RM_NEAREST:
        /* Round result to nearest representable value. When two
           representable values are equally near, round to the value
           that has a least significant bit of zero (i.e. is even). */
#ifdef HAVE_ANINT
        tmp = (float)anint((double)tmp);
#else
        /* TODO: Provide round-to-nearest */
#endif
        break;

       case FP_RM_TOZERO:
        /* Round result to the value closest to, and not greater in
           magnitude than, the result. */
#ifdef HAVE_AINT
        tmp = (float)aint((double)tmp);
#else
        /* TODO: Provide round-to-zero */
#endif
        break;

       case FP_RM_TOPINF:
        /* Round result to the value closest to, and not less than,
           the result. */
        tmp = (float)ceil((double)tmp);
        break;

       case FP_RM_TOMINF:
        /* Round result to the value closest to, and not greater than,
           the result. */
        tmp = (float)floor((double)tmp);
        break;
      }
#endif /* 0 */

      result = (uword64)*(unsigned int *)&tmp;
    }
    break;

   case fmt_double:
    {
      double tmp;
      word64 xxx;

      switch (from) {
       case fmt_single:
        {
          unsigned int wop = (unsigned int)op;
          tmp = (double)(*(float *)&wop);
        }
        break;

       case fmt_word:
        xxx = SIGNEXTEND((op & 0xFFFFFFFF),32);
        tmp = xxx;
        break;

       case fmt_long:
        tmp = (double)((word64)op);
        break;
      }

#if 0
      /* FIXME: This code is incorrect.  The rounding mode does not
         round to integral values; it rounds to the nearest
         representable value in the format.  */

      switch (rm) {
       case FP_RM_NEAREST:
#ifdef HAVE_ANINT
        tmp = anint(*(double *)&tmp);
#else
        /* TODO: Provide round-to-nearest */
#endif
        break;

       case FP_RM_TOZERO:
#ifdef HAVE_AINT
        tmp = aint(*(double *)&tmp);
#else
        /* TODO: Provide round-to-zero */
#endif
        break;

       case FP_RM_TOPINF:
        tmp = ceil(*(double *)&tmp);
        break;

       case FP_RM_TOMINF:
        tmp = floor(*(double *)&tmp);
        break;
      }
#endif /* 0 */

      result = *(uword64 *)&tmp;
    }
    break;

   case fmt_word:
   case fmt_long:
    if (Infinity(op,from) || NaN(op,from) || (1 == 0/*TODO: check range */)) {
      printf("DBG: TODO: update FCSR\n");
      SignalException(FPE);
    } else {
      if (to == fmt_word) {
        int tmp;
        switch (from) {
         case fmt_single:
          {
            unsigned int wop = (unsigned int)op;
            tmp = (int)*((float *)&wop);
          }
          break;
         case fmt_double:
          tmp = (int)*((double *)&op);
#ifdef DEBUG
          printf("DBG: from double %.30f (0x%08X%08X) to word: 0x%08X\n",*((double *)&op),WORD64HI(op),WORD64LO(op),tmp);
#endif /* DEBUG */
          break;
        }
        result = (uword64)tmp;
      } else { /* fmt_long */
	word64 tmp;
        switch (from) {
         case fmt_single:
          {
            unsigned int wop = (unsigned int)op;
            tmp = (word64)*((float *)&wop);
          }
          break;
         case fmt_double:
          tmp = (word64)*((double *)&op);
          break;
        }
	result = (uword64)tmp;
      }
    }
    break;
  }

#ifdef DEBUG
  printf("DBG: Convert: returning 0x%08X%08X (to format = %s)\n",WORD64HI(result),WORD64LO(result),DOFMT(to));
#endif /* DEBUG */

  return(result);
}
#endif /* HASFPU */

/*-- co-processor support routines ------------------------------------------*/

static int
CoProcPresent(coproc_number)
     unsigned int coproc_number;
{
  /* Return TRUE if simulator provides a model for the given co-processor number */
  return(0);
}

static void
COP_LW(coproc_num,coproc_reg,memword)
     int coproc_num, coproc_reg;
     unsigned int memword;
{
  switch (coproc_num) {
#if defined(HASFPU)
    case 1:
#ifdef DEBUG
    printf("DBG: COP_LW: memword = 0x%08X (uword64)memword = 0x%08X%08X\n",memword,WORD64HI(memword),WORD64LO(memword));
#endif
     StoreFPR(coproc_reg,fmt_uninterpreted,(uword64)memword);
     break;
#endif /* HASFPU */

    default:
#if 0 /* this should be controlled by a configuration option */
     callback->printf_filtered(callback,"COP_LW(%d,%d,0x%08X) at IPC = 0x%08X%08X : TODO (architecture specific)\n",coproc_num,coproc_reg,memword,WORD64HI(IPC),WORD64LO(IPC));
#endif
     break;
  }

  return;
}

static void
COP_LD(coproc_num,coproc_reg,memword)
     int coproc_num, coproc_reg;
     uword64 memword;
{
  switch (coproc_num) {
#if defined(HASFPU)
    case 1:
     StoreFPR(coproc_reg,fmt_uninterpreted,memword);
     break;
#endif /* HASFPU */

    default:
#if 0 /* this message should be controlled by a configuration option */
     callback->printf_filtered(callback,"COP_LD(%d,%d,0x%08X%08X) at IPC = 0x%08X%08X : TODO (architecture specific)\n",coproc_num,coproc_reg,WORD64HI(memword),WORD64LO(memword),WORD64HI(IPC),WORD64LO(IPC));
#endif
     break;
  }

  return;
}

static unsigned int
COP_SW(coproc_num,coproc_reg)
     int coproc_num, coproc_reg;
{
  unsigned int value = 0;
  switch (coproc_num) {
#if defined(HASFPU)
    case 1:
#if 1
     value = (unsigned int)ValueFPR(coproc_reg,fmt_uninterpreted);
#else
#if 1
     value = (unsigned int)ValueFPR(coproc_reg,fpr_state[coproc_reg]);
#else
#ifdef DEBUG
     printf("DBG: COP_SW: reg in format %s (will be accessing as single)\n",DOFMT(fpr_state[coproc_reg])); 
#endif /* DEBUG */
     value = (unsigned int)ValueFPR(coproc_reg,fmt_single);
#endif
#endif
     break;
#endif /* HASFPU */

    default:
#if 0 /* should be controlled by configuration option */
     callback->printf_filtered(callback,"COP_SW(%d,%d) at IPC = 0x%08X%08X : TODO (architecture specific)\n",coproc_num,coproc_reg,WORD64HI(IPC),WORD64LO(IPC));
#endif
     break;
  }

  return(value);
}

static uword64
COP_SD(coproc_num,coproc_reg)
     int coproc_num, coproc_reg;
{
  uword64 value = 0;
  switch (coproc_num) {
#if defined(HASFPU)
    case 1:
#if 1
     value = ValueFPR(coproc_reg,fmt_uninterpreted);
#else
#if 1
     value = ValueFPR(coproc_reg,fpr_state[coproc_reg]);
#else
#ifdef DEBUG
     printf("DBG: COP_SD: reg in format %s (will be accessing as double)\n",DOFMT(fpr_state[coproc_reg]));
#endif /* DEBUG */
     value = ValueFPR(coproc_reg,fmt_double);
#endif
#endif
     break;
#endif /* HASFPU */

    default:
#if 0 /* should be controlled by configuration option */
     callback->printf_filtered(callback,"COP_SD(%d,%d) at IPC = 0x%08X%08X : TODO (architecture specific)\n",coproc_num,coproc_reg,WORD64HI(IPC),WORD64LO(IPC));
#endif
     break;
  }

  return(value);
}

static void
decode_coproc(instruction)
     unsigned int instruction;
{
  int coprocnum = ((instruction >> 26) & 3);

  switch (coprocnum) {
    case 0: /* standard CPU control and cache registers */
      {
        /* NOTEs:
           Standard CP0 registers
           	0 = Index               R4000   VR4100  VR4300
                1 = Random              R4000   VR4100  VR4300
                2 = EntryLo0            R4000   VR4100  VR4300
                3 = EntryLo1            R4000   VR4100  VR4300
                4 = Context             R4000   VR4100  VR4300
                5 = PageMask            R4000   VR4100  VR4300
                6 = Wired               R4000   VR4100  VR4300
                8 = BadVAddr            R4000   VR4100  VR4300
                9 = Count               R4000   VR4100  VR4300
                10 = EntryHi            R4000   VR4100  VR4300
                11 = Compare            R4000   VR4100  VR4300
                12 = SR                 R4000   VR4100  VR4300
                13 = Cause              R4000   VR4100  VR4300
                14 = EPC                R4000   VR4100  VR4300
                15 = PRId               R4000   VR4100  VR4300
                16 = Config             R4000   VR4100  VR4300
                17 = LLAddr             R4000   VR4100  VR4300
                18 = WatchLo            R4000   VR4100  VR4300
                19 = WatchHi            R4000   VR4100  VR4300
                20 = XContext           R4000   VR4100  VR4300
                26 = PErr or ECC        R4000   VR4100  VR4300
                27 = CacheErr           R4000   VR4100
                28 = TagLo              R4000   VR4100  VR4300
                29 = TagHi              R4000   VR4100  VR4300
                30 = ErrorEPC           R4000   VR4100  VR4300
        */
        int code = ((instruction >> 21) & 0x1F);
        /* R4000 Users Manual (second edition) lists the following CP0
           instructions:
           	DMFC0   Doubleword Move From CP0        (VR4100 = 01000000001tttttddddd00000000000)
                DMTC0   Doubleword Move To CP0          (VR4100 = 01000000101tttttddddd00000000000)
                MFC0    word Move From CP0              (VR4100 = 01000000000tttttddddd00000000000)
                MTC0    word Move To CP0                (VR4100 = 01000000100tttttddddd00000000000)
                TLBR    Read Indexed TLB Entry          (VR4100 = 01000010000000000000000000000001)
                TLBWI   Write Indexed TLB Entry         (VR4100 = 01000010000000000000000000000010)
                TLBWR   Write Random TLB Entry          (VR4100 = 01000010000000000000000000000110)
                TLBP    Probe TLB for Matching Entry    (VR4100 = 01000010000000000000000000001000)
                CACHE   Cache operation                 (VR4100 = 101111bbbbbpppppiiiiiiiiiiiiiiii)
                ERET    Exception return                (VR4100 = 01000010000000000000000000011000)
        */
        if (((code == 0x00) || (code == 0x04)) && ((instruction & 0x7FF) == 0)) {
          int rt = ((instruction >> 16) & 0x1F);
          int rd = ((instruction >> 11) & 0x1F);
          if (code == 0x00) { /* MF : move from */
#if 0 /* message should be controlled by configuration option */
            callback->printf_filtered(callback,"Warning: MFC0 %d,%d not handled yet (architecture specific)\n",rt,rd);
#endif
            GPR[rt] = 0xDEADC0DE; /* CPR[0,rd] */
          } else { /* MT : move to */
            /* CPR[0,rd] = GPR[rt]; */
#if 0 /* should be controlled by configuration option */
            callback->printf_filtered(callback,"Warning: MTC0 %d,%d not handled yet (architecture specific)\n",rt,rd);
#endif
          }
        } else
         sim_warning("Unrecognised COP0 instruction 0x%08X at IPC = 0x%08X%08X : No handler present",instruction,WORD64HI(IPC),WORD64LO(IPC));
        /* TODO: When executing an ERET or RFE instruction we should
           clear LLBIT, to ensure that any out-standing atomic
           read/modify/write sequence fails. */
      }
      break;

    case 2: /* undefined co-processor */
      sim_warning("COP2 instruction 0x%08X at IPC = 0x%08X%08X : No handler present",instruction,WORD64HI(IPC),WORD64LO(IPC));
      break;

    case 1: /* should not occur (FPU co-processor) */
    case 3: /* should not occur (FPU co-processor) */
      SignalException(ReservedInstruction,instruction);
      break;
  }

  return;
}

/*-- instruction simulation -------------------------------------------------*/

static void
simulate ()
{
  unsigned int pipeline_count = 1;

#ifdef DEBUG
  if (membank == NULL) {
    printf("DBG: simulate() entered with no memory\n");
    exit(1);
  }
#endif /* DEBUG */

#if 0 /* Disabled to check that everything works OK */
  /* The VR4300 seems to sign-extend the PC on its first
     access. However, this may just be because it is currently
     configured in 32bit mode. However... */
  PC = SIGNEXTEND(PC,32);
#endif

  /* main controlling loop */
  do {
    /* Fetch the next instruction from the simulator memory: */
    uword64 vaddr = (uword64)PC;
    uword64 paddr;
    int cca;
    unsigned int instruction;
    int dsstate = (state & simDELAYSLOT);

#ifdef DEBUG
    {
      printf("DBG: state = 0x%08X :",state);
      if (state & simSTOP) printf(" simSTOP");
      if (state & simSTEP) printf(" simSTEP");
      if (state & simHALTEX) printf(" simHALTEX");
      if (state & simHALTIN) printf(" simHALTIN");
      if (state & simBE) printf(" simBE");
    }
#endif /* DEBUG */

#ifdef DEBUG
    if (dsstate)
     callback->printf_filtered(callback,"DBG: DSPC = 0x%08X%08X\n",WORD64HI(DSPC),WORD64LO(DSPC));
#endif /* DEBUG */

    if (AddressTranslation(PC,isINSTRUCTION,isLOAD,&paddr,&cca,isTARGET,isREAL)) { /* Copy the action of the LW instruction */
      unsigned int reverse = (ReverseEndian ? (LOADDRMASK >> 2) : 0);
      unsigned int bigend = (BigEndianCPU ? (LOADDRMASK >> 2) : 0);
      uword64 value;
      unsigned int byte;
      paddr = ((paddr & ~LOADDRMASK) | ((paddr & LOADDRMASK) ^ (reverse << 2)));
      value = LoadMemory(cca,AccessLength_WORD,paddr,vaddr,isINSTRUCTION,isREAL);
      byte = ((vaddr & LOADDRMASK) ^ (bigend << 2));
      instruction = ((value >> (8 * byte)) & 0xFFFFFFFF);
    } else {
      fprintf(stderr,"Cannot translate address for PC = 0x%08X%08X failed\n",WORD64HI(PC),WORD64LO(PC));
      exit(1);
    }

#ifdef DEBUG
    callback->printf_filtered(callback,"DBG: fetched 0x%08X from PC = 0x%08X%08X\n",instruction,WORD64HI(PC),WORD64LO(PC));
#endif /* DEBUG */

#if !defined(FASTSIM) || defined(PROFILE)
    instruction_fetches++;
    /* Since we increment above, the value should only ever be zero if
       we have just overflowed: */
    if (instruction_fetches == 0)
      instruction_fetch_overflow++;
#if defined(PROFILE)
    if ((state & simPROFILE) && ((instruction_fetches % profile_frequency) == 0) && profile_hist) {
      int n = ((unsigned int)(PC - profile_minpc) >> (profile_shift + 2));
      if (n < profile_nsamples) {
        /* NOTE: The counts for the profiling bins are only 16bits wide */
        if (profile_hist[n] != USHRT_MAX)
         (profile_hist[n])++;
      }
    }
#endif /* PROFILE */
#endif /* !FASTSIM && PROFILE */

    IPC = PC; /* copy PC for this instruction */
    /* This is required by exception processing, to ensure that we can
       cope with exceptions in the delay slots of branches that may
       already have changed the PC. */
    PC += 4; /* increment ready for the next fetch */
    /* NOTE: If we perform a delay slot change to the PC, this
       increment is not requuired. However, it would make the
       simulator more complicated to try and avoid this small hit. */

    /* Currently this code provides a simple model. For more
       complicated models we could perform exception status checks at
       this point, and set the simSTOP state as required. This could
       also include processing any hardware interrupts raised by any
       I/O model attached to the simulator context.

       Support for "asynchronous" I/O events within the simulated world
       could be providing by managing a counter, and calling a I/O
       specific handler when a particular threshold is reached. On most
       architectures a decrement and check for zero operation is
       usually quicker than an increment and compare. However, the
       process of managing a known value decrement to zero, is higher
       than the cost of using an explicit value UINT_MAX into the
       future. Which system is used will depend on how complicated the
       I/O model is, and how much it is likely to affect the simulator
       bandwidth.

       If events need to be scheduled further in the future than
       UINT_MAX event ticks, then the I/O model should just provide its
       own counter, triggered from the event system. */

    /* MIPS pipeline ticks. To allow for future support where the
       pipeline hit of individual instructions is known, this control
       loop manages a "pipeline_count" variable. It is initialised to
       1 (one), and will only be changed by the simulator engine when
       executing an instruction. If the engine does not have access to
       pipeline cycle count information then all instructions will be
       treated as using a single cycle. NOTE: A standard system is not
       provided by the default simulator because different MIPS
       architectures have different cycle counts for the same
       instructions. */

#if defined(HASFPU)
    /* Set previous flag, depending on current: */
    if (state & simPCOC0)
     state |= simPCOC1;
    else
     state &= ~simPCOC1;
    /* and update the current value: */
    if (GETFCC(0))
     state |= simPCOC0;
    else
     state &= ~simPCOC0;
#endif /* HASFPU */

/* NOTE: For multi-context simulation environments the "instruction"
   variable should be local to this routine. */

/* Shorthand accesses for engine. Note: If we wanted to use global
   variables (and a single-threaded simulator engine), then we can
   create the actual variables with these names. */

    if (!(state & simSKIPNEXT)) {
      /* Include the simulator engine */
#include "engine.c"
#if ((GPRLEN == 64) && !PROCESSOR_64BIT) || ((GPRLEN == 32) && PROCESSOR_64BIT)
#error "Mismatch between run-time simulator code and simulation engine"
#endif

#if defined(WARN_LOHI)
      /* Decrement the HI/LO validity ticks */
      if (HIACCESS > 0)
       HIACCESS--;
      if (LOACCESS > 0)
       LOACCESS--;
#endif /* WARN_LOHI */

#if defined(WARN_ZERO)
      /* For certain MIPS architectures, GPR[0] is hardwired to zero. We
         should check for it being changed. It is better doing it here,
         than within the simulator, since it will help keep the simulator
         small. */
      if (ZERO != 0) {
        sim_warning("The ZERO register has been updated with 0x%08X%08X (PC = 0x%08X%08X) (reset back to zero)",WORD64HI(ZERO),WORD64LO(ZERO),WORD64HI(IPC),WORD64LO(IPC));
        ZERO = 0; /* reset back to zero before next instruction */
      }
#endif /* WARN_ZERO */
    } else /* simSKIPNEXT check */
     state &= ~simSKIPNEXT;

    /* If the delay slot was active before the instruction is
       executed, then update the PC to its new value: */
    if (dsstate) {
#ifdef DEBUG
      printf("DBG: dsstate set before instruction execution - updating PC to 0x%08X%08X\n",WORD64HI(DSPC),WORD64LO(DSPC));
#endif /* DEBUG */
      PC = DSPC;
      state &= ~simDELAYSLOT;
    }

    if (MIPSISA < 4) { /* The following is only required on pre MIPS IV processors: */
      /* Deal with pending register updates: */
#ifdef DEBUG
      printf("DBG: EMPTY BEFORE pending_in = %d, pending_out = %d, pending_total = %d\n",pending_in,pending_out,pending_total);
#endif /* DEBUG */
      if (pending_out != pending_in) {
        int loop;
        int index = pending_out;
        int total = pending_total;
        if (pending_total == 0) {
          fprintf(stderr,"FATAL: Mis-match on pending update pointers\n");
          exit(1);
        }
        for (loop = 0; (loop < total); loop++) {
#ifdef DEBUG
          printf("DBG: BEFORE index = %d, loop = %d\n",index,loop);
#endif /* DEBUG */
          if (pending_slot_reg[index] != (LAST_EMBED_REGNUM + 1)) {
#ifdef DEBUG
            printf("pending_slot_count[%d] = %d\n",index,pending_slot_count[index]);
#endif /* DEBUG */
            if (--(pending_slot_count[index]) == 0) {
#ifdef DEBUG
              printf("pending_slot_reg[%d] = %d\n",index,pending_slot_reg[index]);
              printf("pending_slot_value[%d] = 0x%08X%08X\n",index,WORD64HI(pending_slot_value[index]),WORD64LO(pending_slot_value[index]));
#endif /* DEBUG */
              if (pending_slot_reg[index] == COCIDX) {
                SETFCC(0,((FCR31 & (1 << 23)) ? 1 : 0));
              } else {
                registers[pending_slot_reg[index]] = pending_slot_value[index];
#if defined(HASFPU)
                /* The only time we have PENDING updates to FPU
                   registers, is when performing binary transfers. This
                   means we should update the register type field.  */
                if ((pending_slot_reg[index] >= FGRIDX) && (pending_slot_reg[index] < (FGRIDX + 32)))
                 fpr_state[pending_slot_reg[index]] = fmt_uninterpreted;
#endif /* HASFPU */
              }
#ifdef DEBUG
              printf("registers[%d] = 0x%08X%08X\n",pending_slot_reg[index],WORD64HI(registers[pending_slot_reg[index]]),WORD64LO(registers[pending_slot_reg[index]]));
#endif /* DEBUG */
              pending_slot_reg[index] = (LAST_EMBED_REGNUM + 1);
              pending_out++;
              if (pending_out == PSLOTS)
               pending_out = 0;
              pending_total--;
            }
          }
#ifdef DEBUG
          printf("DBG: AFTER  index = %d, loop = %d\n",index,loop);
#endif /* DEBUG */
          index++;
          if (index == PSLOTS)
           index = 0;
        }
      }
#ifdef DEBUG
      printf("DBG: EMPTY AFTER  pending_in = %d, pending_out = %d, pending_total = %d\n",pending_in,pending_out,pending_total);
#endif /* DEBUG */
    }

#if !defined(FASTSIM)
    pipeline_ticks += pipeline_count;
#endif /* FASTSIM */

    if (state & simSTEP)
     state |= simSTOP;
  } while (!(state & simSTOP));

#ifdef DEBUG
  if (membank == NULL) {
    printf("DBG: simulate() LEAVING with no memory\n");
    exit(1);
  }
#endif /* DEBUG */

  return;
}

/*---------------------------------------------------------------------------*/
/*> EOF interp.c <*/
