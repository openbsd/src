/* Parameters for execution on an HP PA-RISC machine running OSF1, for GDB.
   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).  */

/* Define offsets to access CPROC stack when it does not have
 * a kernel thread.
 */
#define MACHINE_CPROC_SP_OFFSET 20
#define MACHINE_CPROC_PC_OFFSET 16
#define MACHINE_CPROC_FP_OFFSET 12

/*
 * Software defined PSW masks.
 */
#define PSW_SS  0x10000000      /* Kernel managed single step */

/* Thread flavors used in re-setting the T bit.
 * @@ this is also bad for cross debugging.
 */
#define TRACE_FLAVOR		HP800_THREAD_STATE
#define TRACE_FLAVOR_SIZE	HP800_THREAD_STATE_COUNT
#define TRACE_SET(x,state) \
	((struct hp800_thread_state *)state)->cr22 |= PSW_SS
#define TRACE_CLEAR(x,state) \
  	((((struct hp800_thread_state *)state)->cr22 &= ~PSW_SS), 1)

/* For OSF1 (Should be close if not identical to BSD, but I haven't
   tested it yet):

   The signal context structure pointer is always saved at the base
   of the frame + 0x4.

   We get the PC & SP directly from the sigcontext structure itself.
   For other registers we have to dive in a little deeper: 

   The hardware save state pointer is at offset 0x10 within the 
   signal context structure.

   Within the hardware save state, registers are found in the same order
   as the register numbers in GDB. */

#define FRAME_SAVED_PC_IN_SIGTRAMP(FRAME, TMP) \
{ \
  *(TMP) = read_memory_integer ((FRAME)->frame + 0x4, 4); \
  *(TMP) = read_memory_integer (*(TMP) + 0x18, 4); \
}

#define FRAME_BASE_BEFORE_SIGTRAMP(FRAME, TMP) \
{ \
  *(TMP) = read_memory_integer ((FRAME)->frame + 0x4, 4); \
  *(TMP) = read_memory_integer (*(TMP) + 0x8, 4); \
}

#define FRAME_FIND_SAVED_REGS_IN_SIGTRAMP(FRAME, FSR) \
{ \
  int i; \
  CORE_ADDR TMP; \
  TMP = read_memory_integer ((FRAME)->frame + 0x4, 4); \
  TMP = read_memory_integer (TMP + 0x10, 4); \
  for (i = 0; i < NUM_REGS; i++) \
    { \
      if (i == SP_REGNUM) \
	(FSR)->regs[SP_REGNUM] = read_memory_integer (TMP + SP_REGNUM * 4, 4); \
      else \
	(FSR)->regs[i] = TMP + i * 4; \
    } \
}

/* OSF1 does not need the pc space queue restored.  */
#define NO_PC_SPACE_QUEUE_RESTORE

/* The mach kernel uses the recovery counter to implement single
   stepping.  While this greatly simplifies the kernel support
   necessary for single stepping, it unfortunately does the wrong
   thing in the presense of a nullified instruction (gives control
   back two insns after the nullifed insn).  This is an artifact
   of the HP architecture (recovery counter doesn't tick for
   nullified insns).

   Do our best to avoid losing in such situations.  */
#define INSTRUCTION_NULLIFIED \
(({ \
    int ipsw = (int)read_register(IPSW_REGNUM); \
    if (ipsw & PSW_N)  \
      { \
        int pcoqt = (int)read_register(PCOQ_TAIL_REGNUM); \
        write_register(PCOQ_HEAD_REGNUM, pcoqt); \
        write_register(PCOQ_TAIL_REGNUM, pcoqt + 0x4); \
        write_register(IPSW_REGNUM, ipsw & ~(PSW_N | PSW_B | PSW_X)); \
        stop_pc = pcoqt; \
      } \
   }), 0) 

/* It's mostly just the common stuff.  */

#include "pa/tm-hppa.h"
