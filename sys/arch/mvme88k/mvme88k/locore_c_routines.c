/* $OpenBSD: locore_c_routines.c,v 1.6 1999/09/27 19:13:22 smurph Exp $	*/
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
/*
 * HISTORY
 *****************************************************************RCS**/
/* This file created by Omron Corporation, 1990. */

#include <machine/cpu_number.h> 		/* cpu_number() */
#include <machine/board.h>          /* m188 bit defines */
#include <machine/m88100.h> 		/* DMT_VALID		*/
#include <assym.s>			 /* EF_NREGS, etc.	*/
#include <machine/asm.h>		 /* END_OF_VECTOR_LIST, etc. */
#include <machine/asm_macro.h>   /* enable/disable interrupts */
#ifdef DDB
   #include <ddb/db_output.h>		 /* db_printf() 	*/
#endif /* DDB */


#if defined(DDB) && defined(JEFF_DEBUG)
   #define DATA_DEBUG  1
#endif


#if DDB
   #define DEBUG_MSG db_printf
#else
   #define DEBUG_MSG printf
#endif /* DDB */

/*
 *  data access emulation for M88100 exceptions
 */
#define DMT_BYTE	1
#define DMT_HALF	2
#define DMT_WORD	4

extern volatile unsigned int * int_mask_reg[MAX_CPUS]; /* in machdep.c */
extern u_char *int_mask_level;   /* in machdep.c */
extern unsigned master_cpu;      /* in cmmu.c */

static struct {
   unsigned char    offset;
   unsigned char    size;
} dmt_en_info[16] =
{
   {0, 0}, {3, DMT_BYTE}, {2, DMT_BYTE}, {2, DMT_HALF},
   {1, DMT_BYTE}, {0, 0}, {0, 0}, {0, 0},
   {0, DMT_BYTE}, {0, 0}, {0, 0}, {0, 0},
   {0, DMT_HALF}, {0, 0}, {0, 0}, {0, DMT_WORD}
};

#if DATA_DEBUG
int data_access_emulation_debug = 0;
static char *bytes[] =
{
   "____", "___x", "__x_", "__xx",
   "_x__", "_x_x", "_xx_", "_xxx",
   "x___", "x__x", "x_x_", "x_xx",
   "xx__", "xx_x", "xxx_", "xxxx",
};
   #define DAE_DEBUG(stuff) {						\
	if (data_access_emulation_debug != 0) { stuff ;}   }
#else
   #define DAE_DEBUG(stuff)
#endif

void 
dae_print(unsigned *eframe)
{
   register int x;
   register struct dmt_reg *dmtx;
   register unsigned  dmax, dmdx;
   register unsigned  v, reg;
   static char *bytes[] =
   {
      "____", "___x", "__x_", "__xx",
      "_x__", "_x_x", "_xx_", "_xxx",
      "x___", "x__x", "x_x_", "x_xx",
      "xx__", "xx_x", "xxx_", "xxxx",
   };

   if (!(eframe[EF_DMT0] & DMT_VALID))
      return;

   for (x = 0; x < 3; x++) {
      dmtx = (struct dmt_reg *)&eframe[EF_DMT0+x*3];

      if (!dmtx->dmt_valid)
         continue;

      dmdx = eframe[EF_DMD0+x*3];
      dmax = eframe[EF_DMA0+x*3];

      if (dmtx->dmt_write)
         printf("[DMT%d=%x: st.%c %x to %x as [%s] %s %s]\n",
                x, eframe[EF_DMT0+x*3], dmtx->dmt_das ? 's' : 'u',
                dmdx, dmax, bytes[dmtx->dmt_en], 
                dmtx->dmt_doub1 ? "double": "not double",
                dmtx->dmt_lockbar ? "xmem": "not xmem");
      else
         printf("[DMT%d=%x: ld.%c r%d <- %x as [%s] %s %s]\n",
                x, eframe[EF_DMT0+x*3], dmtx->dmt_das ? 's' : 'u',
                dmtx->dmt_dreg, dmax, bytes[dmtx->dmt_en], 
                dmtx->dmt_doub1 ? "double": "not double",
                dmtx->dmt_lockbar ? "xmem": "not xmem");

   }
}
#if defined(MVME187) || defined(MVME188)
void data_access_emulation(unsigned *eframe)
{
   register int x;
   register struct dmt_reg *dmtx;
   register unsigned  dmax, dmdx;
   register unsigned  v, reg;

   if (!(eframe[EF_DMT0] & DMT_VALID))
      return;

   for (x = 0; x < 3; x++) {
      dmtx = (struct dmt_reg *)&eframe[EF_DMT0+x*3];

      if (!dmtx->dmt_valid)
         continue;

      dmdx = eframe[EF_DMD0+x*3];
      dmax = eframe[EF_DMA0+x*3];

      DAE_DEBUG(
               if (dmtx->dmt_write)
               DEBUG_MSG("[DMT%d=%x: st.%c %x to %x as [%s] %s %s]\n",
                         x, eframe[EF_DMT0+x*3], dmtx->dmt_das ? 's' : 'u',
                         dmdx, dmax, bytes[dmtx->dmt_en], 
                         dmtx->dmt_doub1 ? "double": "not double",
                         dmtx->dmt_lockbar ? "xmem": "not xmem");
               else
               DEBUG_MSG("[DMT%d=%x: ld.%c r%d<-%x as [%s] %s %s]\n",
                         x, eframe[EF_DMT0+x*3], dmtx->dmt_das ? 's' : 'u',
                         dmtx->dmt_dreg, dmax, bytes[dmtx->dmt_en], 
                         dmtx->dmt_doub1 ? "double": "not double",
                         dmtx->dmt_lockbar ? "xmem": "not xmem");
               )

      dmax += dmt_en_info[dmtx->dmt_en].offset;
      reg = dmtx->dmt_dreg;

      if ( ! dmtx->dmt_lockbar) {
         /* the fault is not during an XMEM */

         if (x == 2 && dmtx->dmt_doub1) {
            /* pipeline 2 (earliest stage) for a double */

            if (dmtx->dmt_write) {
               /* STORE DOUBLE WILL BE RE-INITIATED BY rte */
               
            }

            else {
               /* EMULATE ld.d INSTRUCTION */
               v = do_load_word(dmax, dmtx->dmt_das);
               if (reg != 0)
                  eframe[EF_R0 + reg] = v;
               v = do_load_word(dmax ^ 4, dmtx->dmt_das);
               if (reg != 31)
                  eframe[EF_R0 + reg + 1] = v;
            }
         } else {  /* not pipeline #2 with a double */
            if (dmtx->dmt_write) {
               switch (dmt_en_info[dmtx->dmt_en].size) {
                  case DMT_BYTE:
                     DAE_DEBUG(DEBUG_MSG("[byte %x -> [%x(%c)]\n",
                                         dmdx & 0xff, dmax, dmtx->dmt_das ? 's' : 'u'))
                     do_store_byte(dmax, dmdx, dmtx->dmt_das);
                     break;
                  case DMT_HALF:
                     DAE_DEBUG(DEBUG_MSG("[half %x -> [%x(%c)]\n",
                                         dmdx & 0xffff, dmax, dmtx->dmt_das ? 's' : 'u'))
                     do_store_half(dmax, dmdx, dmtx->dmt_das);
                     break;
                  case DMT_WORD:
                     DAE_DEBUG(DEBUG_MSG("[word %x -> [%x(%c)]\n",
                                         dmdx, dmax, dmtx->dmt_das ? 's' : 'u'))
                     do_store_word(dmax, dmdx, dmtx->dmt_das);
                     break;
               } 
            } else {  /* else it's a read */
               switch (dmt_en_info[dmtx->dmt_en].size) {
                  case DMT_BYTE:
                     v = do_load_byte(dmax, dmtx->dmt_das);
                     if (!dmtx->dmt_signed)
                        v &= 0x000000ff;
                     break;
                  case DMT_HALF:
                     v = do_load_half(dmax, dmtx->dmt_das);
                     if (!dmtx->dmt_signed)
                        v &= 0x0000ffff;
                     break;
                  case DMT_WORD:
                  default: /* 'default' just to shut up lint */
                     v = do_load_word(dmax, dmtx->dmt_das);
                     break;
               }
               if (reg == 0) {
                  DAE_DEBUG(DEBUG_MSG("[no write to r0 done]\n"));
               } else {
                  DAE_DEBUG(DEBUG_MSG("[r%d <- %x]\n", reg, v));
                  eframe[EF_R0 + reg] = v;
               }
            }
         }
      } else { /* if lockbar is set... it's part of an XMEM */
         /*
          * According to Motorola's "General Information",
          * the dmt_doub1 bit is never set in this case, as it should be.
          * They call this "general information" - I call it a f*cking bug!
          *
          * Anyway, if lockbar is set (as it is if we're here) and if
          * the write is not set, then it's the same as if doub1
          * was set...
          */
         if ( ! dmtx->dmt_write) {
            if (x != 2) {
               /* RERUN xmem WITH DMD(x+1) */
               x++;
               dmdx = eframe[EF_DMD0 + x*3];
            } else {
               /* RERUN xmem WITH DMD2 */
               
            }

            if (dmt_en_info[dmtx->dmt_en].size == DMT_WORD)
               v = do_xmem_word(dmax, dmdx, dmtx->dmt_das);
            else
               v = do_xmem_byte(dmax, dmdx, dmtx->dmt_das);
            eframe[EF_R0 + reg] = v;
         } else {
            if (x == 0) {
               eframe[EF_R0 + reg] = dmdx;
               eframe[EF_SFIP] = eframe[EF_SNIP];
               eframe[EF_SNIP] = eframe[EF_SXIP];
               eframe[EF_SXIP] = 0;
               /* xmem RERUN ON rte */
               eframe[EF_DMT0] = 0;
               return;
            }
         }
      }
   }
   eframe[EF_DMT0] = 0;
}
#endif /* defined(MVME187) || defined(MVME188) */

/*
 ***********************************************************************
 ***********************************************************************
 */
#define SIGSYS_MAX      501
#define SIGTRAP_MAX     510

#define EMPTY_BR	0xC0000000U      /* empty "br" instruction */
#define NO_OP 		0xf4005800U      /* "or r0, r0, r0" */

typedef struct {
   unsigned word_one,
   word_two;
} m88k_exception_vector_area;

#define BRANCH(FROM, TO) (EMPTY_BR | ((unsigned)(TO) - (unsigned)(FROM)) >> 2)

#if 0
   #define SET_VECTOR(NUM, to, VALUE) {                                       \
	unsigned _NUM = (unsigned)(NUM);                                   \
	unsigned _VALUE = (unsigned)(VALUE);                               \
	vector[_NUM].word_one = NO_OP; 	                                   \
	vector[_NUM].word_two = BRANCH(&vector[_NUM].word_two, _VALUE);    \
}
#else
   #define SET_VECTOR(NUM, to, VALUE) {                                       \
	vector[NUM].word_one = NO_OP; 	                                   \
	vector[NUM].word_two = BRANCH(&vector[NUM].word_two, VALUE);    \
}
#endif 
/*
 * vector_init(vector, vector_init_list)
 *
 * This routine sets up the m88k vector table for the running processor.
 * It is called with a very little stack, and interrupts disabled,
 * so don't call any other functions!
 *	XXX clean this - nivas
 */
void vector_init(
                m88k_exception_vector_area *vector,
                unsigned *vector_init_list)
{
   unsigned num;
   unsigned vec;
#if defined(MVME187) || defined(MVME188)
   extern void sigsys(), sigtrap(), stepbpt(), userbpt();
   extern void syscall_handler();
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   extern void m197_sigsys(), m197_sigtrap(), m197_stepbpt(), m197_userbpt();
   extern void m197_syscall_handler();
#endif /* MVME197 */
   
   for (num = 0; (vec = vector_init_list[num]) != END_OF_VECTOR_LIST; num++) {
      if (vec != PREDEFINED_BY_ROM)
         SET_VECTOR(num, to, vec);
         asm ("or  r0, r0, r0");
         asm ("or  r0, r0, r0");
         asm ("or  r0, r0, r0");
         asm ("or  r0, r0, r0");
   }

   switch (cputyp) {
#ifdef MVME197
   case CPU_197:
      while (num < 496){
         SET_VECTOR(num, to, m197_sigsys);
         num++;
      }
      num++; /* skip 496, BUG ROM vector */
      SET_VECTOR(450, to, m197_syscall_handler);
   
      while (num <= SIGSYS_MAX)
         SET_VECTOR(num++, to, m197_sigsys);
   
      while (num <= SIGTRAP_MAX)
         SET_VECTOR(num++, to, m197_sigtrap);
   
      SET_VECTOR(504, to, m197_stepbpt);
      SET_VECTOR(511, to, m197_userbpt);
      break;
#endif /* MVME197 */
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      while (num < 496){
         SET_VECTOR(num, to, sigsys);
         num++;
      }
      num++; /* skip 496, BUG ROM vector */
   
      SET_VECTOR(450, to, syscall_handler);

      while (num <= SIGSYS_MAX)
         SET_VECTOR(num++, to, sigsys);

      while (num <= SIGTRAP_MAX)
         SET_VECTOR(num++, to, sigtrap);

      SET_VECTOR(504, to, stepbpt);
      SET_VECTOR(511, to, userbpt);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
   }
}

#ifdef MVME188
unsigned int int_mask_shadow[MAX_CPUS] = {0,0,0,0};
unsigned int m188_curspl[MAX_CPUS] = {0,0,0,0};
unsigned int blocked_interrupts_mask;

unsigned int int_mask_val[INT_LEVEL] = {
   MASK_LVL_0, 
   MASK_LVL_1, 
   MASK_LVL_2, 
   MASK_LVL_3, 
   MASK_LVL_4, 
   MASK_LVL_5, 
   MASK_LVL_6, 
   MASK_LVL_7
};


/*
 * return next safe spl to reenable interrupts.
 */
unsigned int 
safe_level(mask, curlevel)
unsigned mask;
unsigned curlevel;
{
   register int i;

   for (i = curlevel; i < 8; i++)
      if (! (int_mask_val[i] & mask))
         return i;
   printf("safe_level: no safe level for mask 0x%08x level %d found\n",
          mask, curlevel);
   panic("safe_level");
}

void
setlevel(int level)
{
   m88k_psr_type psr;
   register unsigned int mask;
   register int cpu = 0; /* cpu_number(); */

   mask = int_mask_val[level];

   if (cpu != master_cpu)
      mask &= SLAVE_MASK;

   mask &= ISR_SOFTINT_EXCEPT_MASK(cpu);

   mask &= ~blocked_interrupts_mask;

   *int_mask_reg[cpu] = mask;
   int_mask_shadow[cpu] = mask;

   m188_curspl[cpu] = level;
}

#ifdef DDB
void
db_setlevel(int level)
{
   m88k_psr_type psr;
   register unsigned int mask;
   register int cpu = 0; /* cpu_number(); */

   mask = int_mask_val[level];

   if (cpu != master_cpu)
      mask &= SLAVE_MASK;

   mask &= ISR_SOFTINT_EXCEPT_MASK(cpu);

   mask &= ~blocked_interrupts_mask;

   *int_mask_reg[cpu] = mask;
   int_mask_shadow[cpu] = mask;

   m188_curspl[cpu] = level;
}
#endif /* DDB */

void block_obio_interrupt(unsigned mask)
{
   blocked_interrupts_mask |= mask;
}

void unblock_obio_interrupt(unsigned mask)
{
   blocked_interrupts_mask |= ~mask;
}
#endif  /* MVME188 */

unsigned spl(void)
{
   unsigned curspl;
   m88k_psr_type psr; /* proccessor status register */
   int cpu = 0;

   psr = disable_interrupts_return_psr();
   switch (cputyp) {
#ifdef MVME188
      case CPU_188:
         /*cpu = cpu_number();*/
         curspl = m188_curspl[cpu];
         break;
#endif /* MVME188 */
#if defined(MVME187) || defined(MVME197)
      case CPU_187:
      case CPU_197:
         curspl = *int_mask_level;
         break;
#endif /* defined(MVME187) || defined(MVME197) */
      default:
         panic("spl: Can't determine cpu type!");
   }
   set_psr(psr);
   return curspl;
}

#if DDB
unsigned db_spl(void)
{
   unsigned curspl;
   m88k_psr_type psr; /* proccessor status register */
   int cpu = 0;

   psr = disable_interrupts_return_psr();
   switch (cputyp) {
   #ifdef MVME188
      case CPU_188:
         /*cpu = cpu_number();*/
         curspl = m188_curspl[cpu];
         break;
   #endif /* MVME188 */
   #if defined(MVME187) || defined(MVME197)
      case CPU_187:
      case CPU_197:
         curspl = *int_mask_level;
         break;
   #endif /* defined(MVME187) || defined(MVME197) */
      default:
         panic("db_spl: Can't determine cpu type!");
   }
   set_psr(psr);
   return curspl;
}
#endif /* DDB */

unsigned getipl(void)
{
   return (spl());
}

#if DDB
unsigned db_getipl(void)
{
   return (db_spl());
}
#endif /* DDB */

unsigned setipl(unsigned level)
{
   unsigned curspl;
   m88k_psr_type psr; /* proccessor status register */
   int cpu = 0;

   psr = disable_interrupts_return_psr();
   switch (cputyp) {
#ifdef MVME188
      case CPU_188:
         /*cpu = cpu_number();*/
         curspl = m188_curspl[cpu];
         setlevel(level);
         break;
#endif /* MVME188 */
#if defined(MVME187) || defined(MVME197)
      case CPU_187:
      case CPU_197:
         curspl = *int_mask_level;
         *int_mask_level = level;
         break;
#endif /* defined(MVME187) || defined(MVME197) */
      default:
         panic("setipl: Can't determine cpu type!");
   }

   flush_pipeline();

   /* The flush pipeline is required to make sure the above write gets
    * through the data pipe and to the hardware; otherwise, the next
    * bunch of instructions could execute at the wrong spl protection
    */
   set_psr(psr);
   return curspl;
}

#ifdef DDB
unsigned db_setipl(unsigned level)
{
   unsigned curspl;
   m88k_psr_type psr; /* proccessor status register */
   int cpu = 0;

   psr = disable_interrupts_return_psr();
   switch (cputyp) {
#ifdef MVME188
      case CPU_188:
         /*cpu = cpu_number();*/
         curspl = m188_curspl[cpu];
         db_setlevel(level);
         break;
#endif /* MVME188 */
#if defined(MVME187) || defined(MVME197)
      case CPU_187:
      case CPU_197:
         curspl = *int_mask_level;
         *int_mask_level = level;
         break;
#endif /* defined(MVME187) || defined(MVME197) */
      default:
         panic("db_setipl: Can't determine cpu type!");
   }

   flush_pipeline();

   /* The flush pipeline is required to make sure the above write gets
    * through the data pipe and to the hardware; otherwise, the next
    * bunch of instructions could execute at the wrong spl protection
    */
   set_psr(psr);
   return curspl;
}
#endif /* DDB */

#if NCPUS > 1
   #include <sys/simplelock.h>
void
simple_lock_init(lkp)
__volatile struct simplelock *lkp;
{
   lkp->lock_data = 0;
}

int test_and_set(lock)
__volatile int *lock;
{   
/*
   int oldlock = *lock;
   if (*lock == 0) {
      *lock = 1;
      return 0;
   }
*/
   return *lock;
   *lock = 1;
   return 0;
}
#endif


