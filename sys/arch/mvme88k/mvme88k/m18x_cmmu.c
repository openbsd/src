/*	$OpenBSD: m18x_cmmu.c,v 1.2 2000/03/03 00:54:53 todd Exp $	*/
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/simplelock.h>
#include <machine/board.h>
#include <machine/cpus.h>
#include <machine/cpu_number.h>
#include <machine/m882xx.h>

/* On some versions of 88200, page size flushes don't work. I am using
 * sledge hammer approach till I find for sure which ones are bad XXX nivas */
#define BROKEN_MMU_MASK	
#define CMMU_DEBUG 1

#ifdef DEBUG
   #define DB_CMMU		0x4000	/* MMU debug */
unsigned int debuglevel = 0;
   #define dprintf(_L_,_X_) { if (debuglevel & (_L_)) { unsigned int psr = disable_interrupts_return_psr(); printf("%d: ", cpu_number()); printf _X_;  set_psr(psr); } }
#else
   #define dprintf(_L_,_X_)
#endif 
#undef	SHADOW_BATC		/* don't use BATCs for now XXX nivas */

struct cmmu_regs {
   /* base + $000 */
   volatile unsigned idr; 
   /* base + $004 */volatile unsigned scr; 
   /* base + $008 */volatile unsigned ssr; 
   /* base + $00C */volatile unsigned sar; 
   /*             */unsigned padding1[0x3D]; 
   /* base + $104 */volatile unsigned sctr; 
   /* base + $108 */volatile unsigned pfSTATUSr; 
   /* base + $10C */volatile unsigned pfADDRr; 
   /*             */unsigned padding2[0x3C]; 
   /* base + $200 */volatile unsigned sapr; 
   /* base + $204 */volatile unsigned uapr; 
   /*             */unsigned padding3[0x7E]; 
   /* base + $400 */volatile unsigned bwp[8]; 
   /*             */unsigned padding4[0xF8]; 
   /* base + $800 */volatile unsigned cdp[4]; 
   /*             */unsigned padding5[0x0C]; 
   /* base + $840 */volatile unsigned ctp[4]; 
   /*             */unsigned padding6[0x0C]; 
   /* base + $880 */volatile unsigned cssp;

   /* The rest for the 88204 */
#define cssp0 cssp
   /*             */ unsigned padding7[0x03]; 
   /* base + $890 */volatile unsigned cssp1; 
   /*             */unsigned padding8[0x03]; 
   /* base + $8A0 */volatile unsigned cssp2; 
   /*             */unsigned padding9[0x03]; 
   /* base + $8B0 */volatile unsigned cssp3;
};

struct cmmu {
   struct cmmu_regs *cmmu_regs;    /* CMMU "base" area */
   unsigned char  cmmu_cpu;        /* cpu number it is attached to */
   unsigned char  which;           /* either INST_CMMU || DATA_CMMU */
   unsigned char  cmmu_access;     /* either CMMU_ACS_{SUPER,USER,BOTH} */
   unsigned char  cmmu_alive;
#define CMMU_DEAD	0		           /* This cmmu not there */
#define CMMU_AVAILABLE	1		     /* It's there, but which cpu's? */
#define CMMU_ALIVE 1               /* It's there. */
#define CMMU_MARRIED	2		        /* Know which cpu it belongs to. */
   vm_offset_t    cmmu_addr;       /* address range */
   vm_offset_t    cmmu_addr_mask;  /* address mask */
   int            cmmu_addr_match; /* return value of address comparison */
#if SHADOW_BATC
   unsigned batc[8];
#endif
}; 
/*
 * We rely upon and use INST_CMMU == 0 and DATA_CMMU == 1
 */
#if INST_CMMU != 0 || DATA_CMMU != 1
error("ack gag barf!");
#endif

/*
 * CMMU(cpu,data) Is the cmmu struct for the named cpu's indicated cmmu.
 * REGS(cpu,data) is the actual register structure.
 */

#define CMMU(cpu, data) cpu_cmmu[(cpu)].pair[(data)?DATA_CMMU:INST_CMMU]
#define REGS(cpu, data) (*CMMU(cpu, data)->cmmu_regs)

/* 
 * This lock protects the cmmu SAR and SCR's; other ports 
 * can be accessed without locking it 
 *
 * May be used from "db_interface.c".
 */

extern unsigned cache_policy;
extern unsigned cpu_sets[];
extern unsigned number_cpus;
extern unsigned master_cpu;
extern int      max_cpus, max_cmmus;
extern int      cpu_cmmu_ratio;

int      vme188_config;

/* FORWARDS */
void m18x_setup_cmmu_config(void);
void m18x_setup_board_config(void);

#ifdef CMMU_DEBUG
void
m18x_show_apr(unsigned value)
{
   union apr_template apr_template;
   apr_template.bits = value;

   printf("table @ 0x%x000", apr_template.field.st_base);
   if (apr_template.field.wt) printf(", writethrough");
   if (apr_template.field.g)  printf(", global");
   if (apr_template.field.ci) printf(", cache inhibit");
   if (apr_template.field.te) printf(", valid");
   else                       printf(", not valid");
   printf("\n");
}

void
m18x_show_sctr(unsigned value)
{
   union {
      unsigned bits;
      struct {
         unsigned :16,
         pe: 1,
         se: 1,
         pr: 1,
         :13;
      } fields;
   } sctr; 
   sctr.bits = value;
   printf("%spe, %sse %spr]\n",
          sctr.fields.pe ? "" : "!",
          sctr.fields.se ? "" : "!",
          sctr.fields.pr ? "" : "!");
}
#endif

/*----------------------------------------------------------------*/

/*
 * The cmmu.c module was initially designed for the Omron Luna 88K
 * layout consisting of 4 CPUs with 2 CMMUs each, one for data
 * and one for instructions.
 *
 * Trying to support a few more board configurations for the
 * Motorola MVME188 we have these layouts:
 *
 *  - config 0: 4 CPUs, 8 CMMUs
 *  - config 1: 2 CPUs, 8 CMMUs
 *  - config 2: 1 CPUs, 8 CMMUs
 *  - config 5: 2 CPUs, 4 CMMUs
 *  - config 6: 1 CPU,  4 CMMUs
 *  - config A: 1 CPU,  2 CMMUs
 *
 * We use these splitup schemas:
 *  - split between data and instructions (always enabled)
 *  - split between user/spv (and A14 in config 2)
 *  - split because of A12 (and A14 in config 2)
 *  - one SRAM supervisor, other rest
 *  - one whole SRAM, other rest
 *
 * The main problem is to find the right suited CMMU for a given
 * CPU number at those configurations.
 *                                         em, 10.5.94
 *
 * WARNING: the code was never tested on a uniprocessor
 * system. All effort was made to support these configuration
 * but the kernel never ran on such a system.
 *
 *					   em, 12.7.94
 */

/*
 * This structure describes the CMMU per CPU split strategies
 * used for data and instruction CMMUs.
 */
struct cmmu_strategy {
   int inst;
   int data;
} cpu_cmmu_strategy[] = {
   /*     inst                 data */
   { CMMU_SPLIT_SPV,      CMMU_SPLIT_SPV},  /* CPU 0 */
   { CMMU_SPLIT_SPV,      CMMU_SPLIT_SPV},  /* CPU 1 */
   { CMMU_SPLIT_ADDRESS,  CMMU_SPLIT_ADDRESS}, /* CPU 2 */
   { CMMU_SPLIT_ADDRESS,  CMMU_SPLIT_ADDRESS}  /* CPU 3 */
};

/*
 * The following list of structs describe the different
 * MVME188 configurations which are supported by this module.
 */
struct board_config {
   int supported;
   int ncpus;
   int ncmmus;
} bd_config[] =
{
   /* sup, CPU MMU */
   {  1,  4,  8}, /* 4P128 - 4P512 */
   {  1,  2,  8}, /* 2P128 - 2P512 */
   {  1,  1,  8}, /* 1P128 - 1P512 */
   { -1, -1, -1},
   { -1, -1, -1},
   {  1,  2,  4}, /* 2P64  - 2P256 */
   {  1,  1,  4}, /* 1P64  - 1P256 */
   { -1, -1, -1},
   { -1, -1, -1},
   { -1, -1, -1},
   {  1,  1,  2}, /* 1P32  - 1P128 */
   { -1, -1, -1},
   { -1, -1, -1},
   { -1, -1, -1},
   { -1, -1, -1},
   { -1, -1, -1}
};

/*
 * Structure for accessing MMUS properly.
 */

struct cmmu cmmu[MAX_CMMUS] =
{
   /* addr    cpu       mode           access
 alive   addr mask */
   {(void *)VME_CMMU_I0, -1, INST_CMMU, CMMU_ACS_BOTH,       
      CMMU_DEAD,      0, 0},                                 
   {(void *)VME_CMMU_D0, -1, DATA_CMMU, CMMU_ACS_BOTH,       
      CMMU_DEAD,      0, 0},                                 
   {(void *)VME_CMMU_I1, -1, INST_CMMU, CMMU_ACS_BOTH,       
      CMMU_DEAD,      0, 0},                                 
   {(void *)VME_CMMU_D1, -1, DATA_CMMU, CMMU_ACS_BOTH,       
      CMMU_DEAD,      0, 0},                                 
   {(void *)VME_CMMU_I2, -1, INST_CMMU, CMMU_ACS_BOTH,
      CMMU_DEAD,      0, 0},
   {(void *)VME_CMMU_D2, -1, DATA_CMMU, CMMU_ACS_BOTH,
      CMMU_DEAD,      0, 0},
   {(void *)VME_CMMU_I3, -1, INST_CMMU, CMMU_ACS_BOTH,
      CMMU_DEAD,      0, 0},
   {(void *)VME_CMMU_D3, -1, DATA_CMMU, CMMU_ACS_BOTH,
      CMMU_DEAD,      0, 0}
};

struct cpu_cmmu {
   struct cmmu *pair[2];
} cpu_cmmu[MAX_CPUS];

void 
m18x_setup_board_config(void)
{
   volatile unsigned long *whoami;

   master_cpu = 0; /* temp to get things going */
   switch (cputyp) {
      case CPU_187:
      case CPU_197:
         vme188_config = 10; /* There is no WHOAMI reg on MVME1x7 - fake it... */
         cmmu[0].cmmu_regs = (void *)SBC_CMMU_I;
         cmmu[0].cmmu_cpu = 0;
         cmmu[1].cmmu_regs = (void *)SBC_CMMU_D;
         cmmu[1].cmmu_cpu = 0;
         cmmu[2].cmmu_regs = (void *)NULL;
         cmmu[3].cmmu_regs = (void *)NULL;
         cmmu[4].cmmu_regs = (void *)NULL;
         cmmu[5].cmmu_regs = (void *)NULL;
         cmmu[6].cmmu_regs = (void *)NULL;
         cmmu[7].cmmu_regs = (void *)NULL;
         max_cpus = 1;
         max_cmmus = 2;
         break;
      case CPU_188:
         whoami = (volatile unsigned long *)MVME188_WHOAMI;
         vme188_config = (*whoami & 0xf0) >> 4;
         dprintf(DB_CMMU,("m18x_setup_board_config: WHOAMI @ 0x%08x holds value 0x%08x\n",
                          whoami, *whoami));
         max_cpus = bd_config[vme188_config].ncpus;
         max_cmmus = bd_config[vme188_config].ncmmus;
         break;
      default:
         panic("m18x_setup_board_config: Unknown CPU type.");
   }
   cpu_cmmu_ratio = max_cmmus / max_cpus;
   switch (bd_config[vme188_config].supported) {
      case 0:
         printf("MVME%x board configuration #%X: %d CPUs %d CMMUs\n", cputyp, 
                vme188_config, max_cpus, max_cmmus);
         panic("This configuration is not supported - go and get another OS.\n");
         /* NOTREACHED */
         break;
      case 1:
         printf("MVME%x board configuration #%X: %d CPUs %d CMMUs\n", cputyp,
                vme188_config, max_cpus, max_cmmus);
         m18x_setup_cmmu_config();
         break;
      default:
         panic("UNKNOWN MVME%x board configuration: WHOAMI = 0x%02x\n", cputyp, *whoami);
         /* NOTREACHED */
         break;
   }
   return;
}

/*
 * This routine sets up the CPU/CMMU tables used in the
 * motorola/m88k/m88100/cmmu.c module.
 */
void 
m18x_setup_cmmu_config(void)
{
   volatile unsigned long *pcnfa;
   volatile unsigned long *pcnfb;

   register int num, cmmu_num, val1, val2;

   dprintf(DB_CMMU,("m18x_setup_cmmu_config: initializing with %d CPU(s) and %d CMMU(s)\n",
                    max_cpus, max_cmmus));

   /*
    * Probe for available MMUs
    */
   for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++)
      if (!badwordaddr((vm_offset_t)cmmu[cmmu_num].cmmu_regs)) {
         union cpupid id;

         id.cpupid = cmmu[cmmu_num].cmmu_regs->idr;
         if (id.m88200.type != M88200 && id.m88200.type != M88204) {
            printf("WARNING: non M8820x circuit found at CMMU address 0x%08x\n",
                   cmmu[cmmu_num].cmmu_regs);
            continue;
         }
         cmmu[cmmu_num].cmmu_alive = CMMU_ALIVE;
         dprintf(DB_CMMU,("m18x_setup_cmmu_config: CMMU %d found at 0x%08x\n",
                          cmmu_num, cmmu[cmmu_num].cmmu_regs));
      }

      /*
       * Now that we know which CMMUs are there, let's report on which
       * CPU/CMMU sets seem complete (hopefully all)
       */
   for (num = 0; num < max_cpus; num++) {
      register int i;
      union cpupid id;

      for (i = 0; i < cpu_cmmu_ratio; i++) {
         dprintf(DB_CMMU,("cmmu_init: testing CMMU %d for CPU %d\n",
                          num*cpu_cmmu_ratio+i, num));
         if (!m18x_cmmu_alive(num*cpu_cmmu_ratio + i)) {
            printf("CMMU %d attached to CPU %d is not working\n");
            panic("m18x_setup_cmmu_config");
         }
      }
      cpu_sets[num] = 1;   /* This cpu installed... */
      id.cpupid = cmmu[num*cpu_cmmu_ratio].cmmu_regs->idr;

      if (id.m88200.type == M88204)
         printf("CPU%d is attached with %d MC88204 CMMUs\n",
                num, cpu_cmmu_ratio);
      else
         printf("CPU%d is attached with %d MC88200 CMMUs\n",
                num, cpu_cmmu_ratio);
   }

   for (num = 0; num < max_cpus; num++) {
      cpu_cmmu_strategy[num].inst &= CMMU_SPLIT_MASK;
      cpu_cmmu_strategy[num].data &= CMMU_SPLIT_MASK;
      dprintf(DB_CMMU,("m18x_setup_cmmu_config: CPU %d inst strat %d data strat %d\n",
                       num, cpu_cmmu_strategy[num].inst, cpu_cmmu_strategy[num].data));
   }

   switch (vme188_config) {
      /*
       * These configurations have hardwired CPU/CMMU configurations.
       */
      case CONFIG_0:
      case CONFIG_5:
      case CONFIG_A:
         dprintf(DB_CMMU,("m18x_setup_cmmu_config: resetting strategies\n"));
         for (num = 0; num < max_cpus; num++)
            cpu_cmmu_strategy[num].inst = cpu_cmmu_strategy[num].data =
                                          CMMU_SPLIT_ADDRESS;
         break;
         /*
          * Configure CPU/CMMU strategy into PCNFA and PCNFB board registers.
          */
      case CONFIG_1:
         pcnfa = (volatile unsigned long *)MVME188_PCNFA;
         pcnfb = (volatile unsigned long *)MVME188_PCNFB;
         val1 = (cpu_cmmu_strategy[0].inst << 2) | cpu_cmmu_strategy[0].data;
         val2 = (cpu_cmmu_strategy[1].inst << 2) | cpu_cmmu_strategy[1].data;
         *pcnfa = val1;
         *pcnfb = val2;
         dprintf(DB_CMMU,("m18x_setup_cmmu_config: 2P128: PCNFA = 0x%x, PCNFB = 0x%x\n", val1, val2));
         break;
      case CONFIG_2:
         pcnfa = (volatile unsigned long *)MVME188_PCNFA;
         pcnfb = (volatile unsigned long *)MVME188_PCNFB;
         val1 = (cpu_cmmu_strategy[0].inst << 2) | cpu_cmmu_strategy[0].inst;
         val2 = (cpu_cmmu_strategy[0].data << 2) | cpu_cmmu_strategy[0].data;
         *pcnfa = val1;
         *pcnfb = val2;
         dprintf(DB_CMMU,("m18x_setup_cmmu_config: 1P128: PCNFA = 0x%x, PCNFB = 0x%x\n", val1, val2));
         break;
      case CONFIG_6:
         pcnfa = (volatile unsigned long *)MVME188_PCNFA;
         val1 = (cpu_cmmu_strategy[0].inst << 2) | cpu_cmmu_strategy[0].data;
         *pcnfa = val1;
         dprintf(DB_CMMU,("m18x_setup_cmmu_config: 1P64: PCNFA = 0x%x\n", val1));
         break;
      default:
         panic("m18x_setup_cmmu_config");
         break;
   }

   dprintf(DB_CMMU,("m18x_setup_cmmu_config: PCNFA = 0x%x, PCNFB = 0x%x\n", *pcnfa, *pcnfb));

   /*
    * Calculate the CMMU<->CPU connections
    */
   for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
      cmmu[cmmu_num].cmmu_cpu =
      (int) (((float) cmmu_num) * ((float) max_cpus) / ((float) max_cmmus));
      dprintf(DB_CMMU,("m18x_setup_cmmu_config: CMMU %d connected with CPU %d\n",
                       cmmu_num, cmmu[cmmu_num].cmmu_cpu));
   }

   /*
    * Now set cmmu[].cmmu_access and addr
    */
   for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
      /*
       * We don't set up anything for the hardwired configurations.
       */
      if (cpu_cmmu_ratio == 2) {
         cmmu[cmmu_num].cmmu_addr =
         cmmu[cmmu_num].cmmu_addr_mask = 0;
         cmmu[cmmu_num].cmmu_addr_match = 1;
         cmmu[cmmu_num].cmmu_access = CMMU_ACS_BOTH;
         continue;
      }

      /*
       * First we set the address/mask pairs for the exact address
       * matches.
       */
      switch ((cmmu[cmmu_num].which == INST_CMMU) ?
              cpu_cmmu_strategy[cmmu[cmmu_num].cmmu_cpu].inst :
              cpu_cmmu_strategy[cmmu[cmmu_num].cmmu_cpu].data) {
         case CMMU_SPLIT_ADDRESS:
            cmmu[cmmu_num].cmmu_addr = ((cmmu_num & 0x2) ^ 0x2) << 11;
            cmmu[cmmu_num].cmmu_addr_mask = CMMU_A12_MASK;
            cmmu[cmmu_num].cmmu_addr_match = 1;
            break;
         case CMMU_SPLIT_SPV:
            cmmu[cmmu_num].cmmu_addr =
            cmmu[cmmu_num].cmmu_addr_mask = 0;
            cmmu[cmmu_num].cmmu_addr_match = 1;
            break;
         case CMMU_SPLIT_SRAM_ALL:
            cmmu[cmmu_num].cmmu_addr = CMMU_SRAM;
            cmmu[cmmu_num].cmmu_addr_mask = CMMU_SRAM_MASK;
            cmmu[cmmu_num].cmmu_addr_match = (cmmu_num & 0x2) ? 1 : 0;
            break;
         case CMMU_SPLIT_SRAM_SPV:
            if (cmmu_num & 0x2) {
               cmmu[cmmu_num].cmmu_addr = CMMU_SRAM;
               cmmu[cmmu_num].cmmu_addr_mask = CMMU_SRAM_MASK;
            } else {
               cmmu[cmmu_num].cmmu_addr =
               cmmu[cmmu_num].cmmu_addr_mask = 0;
            }
            cmmu[cmmu_num].cmmu_addr_match = 1;
            break;
      }

      /*
       * For MVME188 single processors, we've got to look at A14.
       * This bit splits the CMMUs independent of the enabled strategy.
       *
       * NOT TESTED!!! - em
       */
      if (cpu_cmmu_ratio > 4) {
         cmmu[cmmu_num].cmmu_addr |= ((cmmu_num & 0x4) ^ 0x4) << 12;
         cmmu[cmmu_num].cmmu_addr_mask |= CMMU_A14_MASK;
      }

      /*
       * Next we cope with the various access modes.
       */
      switch ((cmmu[cmmu_num].which == INST_CMMU) ?
              cpu_cmmu_strategy[cmmu[cmmu_num].cmmu_cpu].inst :
              cpu_cmmu_strategy[cmmu[cmmu_num].cmmu_cpu].data) {
         case CMMU_SPLIT_SPV:
            cmmu[cmmu_num].cmmu_access =
            (cmmu_num & 0x2 ) ? CMMU_ACS_USER : CMMU_ACS_SUPER;
            break;
         case CMMU_SPLIT_SRAM_SPV:
            cmmu[cmmu_num].cmmu_access =
            (cmmu_num & 0x2 ) ? CMMU_ACS_SUPER : CMMU_ACS_BOTH;
            break;
         default:
            cmmu[cmmu_num].cmmu_access = CMMU_ACS_BOTH;
            break;
      }
   }
   return;
}

static char *cmmu_strat_string[] = {
   "address split ",
   "user/spv split",
   "spv SRAM split",
   "all SRAM split"
};

void m18x_cmmu_dump_config(void)
{

   volatile unsigned long *pcnfa;
   volatile unsigned long *pcnfb;
   register int cmmu_num;

   if (cputyp != CPU_188) return;

   db_printf("Current CPU/CMMU configuration:\n\n");

   db_printf("VME188 address decoder: PCNFA = 0x%1x, PCNFB = 0x%1x\n\n", *pcnfa & 0xf, *pcnfb & 0xf);
   pcnfa = (volatile unsigned long *)MVME188_PCNFA;
   pcnfb = (volatile unsigned long *)MVME188_PCNFB;
   for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
      db_printf("CMMU #%d: %s CMMU for CPU %d:\n Strategy: %s\n %s access addr 0x%08x mask 0x%08x match %s\n",
                cmmu_num,
                (cmmu[cmmu_num].which == INST_CMMU) ? "inst" : "data",
                cmmu[cmmu_num].cmmu_cpu,
                cmmu_strat_string[(cmmu[cmmu_num].which == INST_CMMU) ?
                                  cpu_cmmu_strategy[cmmu[cmmu_num].cmmu_cpu].inst :
                                  cpu_cmmu_strategy[cmmu[cmmu_num].cmmu_cpu].data],
                (cmmu[cmmu_num].cmmu_access == CMMU_ACS_BOTH) ?   "User and spv" :
                ((cmmu[cmmu_num].cmmu_access == CMMU_ACS_USER) ? "User        " :
                 "Supervisor  "),
                cmmu[cmmu_num].cmmu_addr,
                cmmu[cmmu_num].cmmu_addr_mask,
                cmmu[cmmu_num].cmmu_addr_match ? "TRUE" : "FALSE");
   }
}

/* To be implemented as a macro for speedup - XXX-em */
static void 
m18x_cmmu_store(int mmu, int reg, unsigned val)
{
   *(volatile unsigned *)(reg + (char*)(cmmu[mmu].cmmu_regs)) = val;
}

int m18x_cmmu_alive(int mmu)
{
   return (cmmu[mmu].cmmu_alive == CMMU_ALIVE);
}

unsigned m18x_cmmu_get(int mmu, int reg)
{
   return *(volatile unsigned *)(reg + (char*)(cmmu[mmu].cmmu_regs));
}

/*
 * This function is called by the MMU module and pokes values
 * into the CMMU's registers.
 */
void m18x_cmmu_set(int reg, unsigned val, int flags,
              int num, int mode, int access, vm_offset_t addr)
{
   register int mmu;

   if (flags & NUM_CMMU) {
      /*
       * Special case: user supplied CMMU number directly as argument.
       * Simply store the value away.
       */
      /* assert(num < max_cmmus); */
      m18x_cmmu_store(num, reg, val);
      return;
   }

   /*
    * We scan all CMMUs to find the matching ones and store the
    * values there.
    */
   for (mmu = num*cpu_cmmu_ratio; mmu < (num+1)*cpu_cmmu_ratio; mmu++) {
      if (((flags & MODE_VAL)) &&
          (cmmu[mmu].which != mode))
         continue;
      if (((flags & ACCESS_VAL)) &&
          (cmmu[mmu].cmmu_access != access) &&
          (cmmu[mmu].cmmu_access != CMMU_ACS_BOTH))
         continue;
      if (flags & ADDR_VAL) {
         if (((addr & cmmu[mmu].cmmu_addr_mask) == cmmu[mmu].cmmu_addr)
             != cmmu[mmu].cmmu_addr_match) {
            continue;
         }
      }
      m18x_cmmu_store(mmu, reg, val);
   }
}

#ifdef DDB
/*
 * Used by DDB for cache probe functions
 */
unsigned m18x_cmmu_get_by_mode(int cpu, int mode)
{
   register int mmu;

   for (mmu = cpu*cpu_cmmu_ratio; mmu < (cpu+1)*cpu_cmmu_ratio; mmu++)
      if (cmmu[mmu].which == mode)
         return mmu;
   printf("can't figure out first %s CMMU for CPU %d\n",
          (mode == DATA_CMMU) ? "data" : "instruction", cpu);
   panic("m18x_cmmu_get_by_mode");
}
#endif

static char *mmutypes[8] = {
   "Unknown (0)",
   "Unknown (1)",
   "Unknown (2)",
   "Unknown (3)",
   "Unknown (4)",
   "M88200 (16K)",
   "M88204 (64K)",
   "Unknown (7)"
};

/*
 * Should only be called after the calling cpus knows its cpu
 * number and master/slave status . Should be called first
 * by the master, before the slaves are started.
*/
void m18x_cpu_configuration_print(int master)
{
   int pid = read_processor_identification_register();
   int proctype = (pid & 0xff00) >> 8;
   int procvers = (pid & 0xe) >> 1;
   int mmu, cpu = cpu_number();
   struct simplelock print_lock;

   if (master)
      simple_lock_init(&print_lock);

   simple_lock(&print_lock);

   printf("Processor %d: ", cpu);
   if (proctype)
      printf("Architectural Revision 0x%x UNKNOWN CPU TYPE Version 0x%x\n",
             proctype, procvers);
   else
      printf("M88100 Version 0x%x\n", procvers);

#if ERRATA__XXX_USR == 0
   if (procvers < 2)
      printf("WARNING: M88100 bug workaround code not enabled!!!\n");
#endif

   for (mmu = cpu*cpu_cmmu_ratio; mmu < (cpu+1)*cpu_cmmu_ratio; mmu++) {
      int idr = m18x_cmmu_get(mmu, CMMU_IDR);
      int mmuid = (0xe00000 & idr)>>21;

      printf(" %s %s Cache: ",
             (cmmu[mmu].cmmu_access == CMMU_ACS_BOTH) ?  "Spv and User" :
             ((cmmu[mmu].cmmu_access == CMMU_ACS_USER) ? "User        " :
              "Supervisor  "),
             (cmmu[mmu].which == INST_CMMU) ?   "Instruction" :
             "Data       ");
      if (mmutypes[mmuid][0] == 'U')
         printf("Type 0x%x ", mmuid);
      else
         printf("%s ", mmutypes[mmuid]);
      printf("Version 0x%x\n", (idr & 0x1f0000)>>16);
   }
   printf  (" Configured as %s and started\n", master ? "master" : "slave");

   simple_unlock(&print_lock);
}

/*
 * CMMU initialization routine
 */
void
m18x_cmmu_init(void)
{
   unsigned tmp, cmmu_num;
   union cpupid id;
   int cpu;

   for (cpu = 0; cpu < max_cpus; cpu++) {
      cpu_cmmu[cpu].pair[INST_CMMU] = cpu_cmmu[cpu].pair[DATA_CMMU] = 0;
   }

   for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++)
      if (m18x_cmmu_alive(cmmu_num)) {
         id.cpupid = cmmu[cmmu_num].cmmu_regs->idr;
			
         cpu_cmmu[cmmu[cmmu_num].cmmu_cpu].pair[cmmu[cmmu_num].which] =
				&cmmu[cmmu_num];
         /*
          * Reset cache data....
          * as per M88200 Manual (2nd Ed.) section 3.11.
          */
         for (tmp = 0; tmp < 255; tmp++) {
            cmmu[cmmu_num].cmmu_regs->sar = tmp << 4;
            cmmu[cmmu_num].cmmu_regs->cssp = 0x3f0ff000;
         }

         /* 88204 has additional cache to clear */
         if (id.m88200.type == M88204) {
            for (tmp = 0; tmp < 255; tmp++) {
               cmmu[cmmu_num].cmmu_regs->sar = tmp<<4;
               cmmu[cmmu_num].cmmu_regs->cssp1 = 0x3f0ff000;
            }
            for (tmp = 0; tmp < 255; tmp++) {
               cmmu[cmmu_num].cmmu_regs->sar = tmp<<4;
               cmmu[cmmu_num].cmmu_regs->cssp2 = 0x3f0ff000;
            }
            for (tmp = 0; tmp < 255; tmp++) {
               cmmu[cmmu_num].cmmu_regs->sar = tmp<<4;
               cmmu[cmmu_num].cmmu_regs->cssp3 = 0x3f0ff000;
            }
         }

         /*
          * Set the SCTR, SAPR, and UAPR to some known state
          * (I don't trust the reset to do it).
          */
         tmp =
         ! CMMU_SCTR_PE |   /* not parity enable */
         ! CMMU_SCTR_SE | /* not snoop enable */
         ! CMMU_SCTR_PR ;  /* not priority arbitration */
         cmmu[cmmu_num].cmmu_regs->sctr = tmp;

         tmp =
         (0x00000 << 12) |  /* segment table base address */
         AREA_D_WT |      /* write through */
         AREA_D_G  | /* global */
         AREA_D_CI | /* cache inhibit */
         ! AREA_D_TE ;   /* not translation enable */
         cmmu[cmmu_num].cmmu_regs->sapr =
         cmmu[cmmu_num].cmmu_regs->uapr = tmp;


#if SHADOW_BATC
         cmmu[cmmu_num].batc[0] =
         cmmu[cmmu_num].batc[1] =
         cmmu[cmmu_num].batc[2] =
         cmmu[cmmu_num].batc[3] =
         cmmu[cmmu_num].batc[4] =
         cmmu[cmmu_num].batc[5] =
         cmmu[cmmu_num].batc[6] =
         cmmu[cmmu_num].batc[7] = 0;
#endif
         cmmu[cmmu_num].cmmu_regs->bwp[0] = 
         cmmu[cmmu_num].cmmu_regs->bwp[1] = 
         cmmu[cmmu_num].cmmu_regs->bwp[2] = 
         cmmu[cmmu_num].cmmu_regs->bwp[3] = 
         cmmu[cmmu_num].cmmu_regs->bwp[4] = 
         cmmu[cmmu_num].cmmu_regs->bwp[5] = 
         cmmu[cmmu_num].cmmu_regs->bwp[6] = 
         cmmu[cmmu_num].cmmu_regs->bwp[7] = 0;
         cmmu[cmmu_num].cmmu_regs->scr = CMMU_FLUSH_CACHE_INV_ALL;
         cmmu[cmmu_num].cmmu_regs->scr = CMMU_FLUSH_SUPER_ALL;
         cmmu[cmmu_num].cmmu_regs->scr = CMMU_FLUSH_USER_ALL;
      }

      /*
       * Enable snooping...
       */
   for (cpu = 0; cpu < max_cpus; cpu++) {
      if (!cpu_sets[cpu])
         continue;

      /*
       * Enable snooping.
       * We enable it for instruction cmmus as well so that we can have
       * breakpoints, etc, and modify code.
       */
      if (cputyp == CPU_188) {
         tmp =
         ! CMMU_SCTR_PE |  /* not parity enable */
         CMMU_SCTR_SE |  /* snoop enable */
         ! CMMU_SCTR_PR ;  /* not priority arbitration */
      } else {
         tmp =
         ! CMMU_SCTR_PE |  /* not parity enable */
         ! CMMU_SCTR_PR ;  /* not priority arbitration */
      }
      m18x_cmmu_set(CMMU_SCTR, tmp, 0, cpu, DATA_CMMU, 0, 0);
      m18x_cmmu_set(CMMU_SCTR, tmp, 0, cpu, INST_CMMU, 0, 0);

      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_SUPER_ALL, ACCESS_VAL,
               cpu, DATA_CMMU, CMMU_ACS_SUPER, 0);
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_SUPER_ALL, ACCESS_VAL,
               cpu, INST_CMMU, CMMU_ACS_SUPER, 0);
   }

   /*
    * Turn on some cache.
    */
   for (cpu = 0; cpu < max_cpus; cpu++) {
      if (!cpu_sets[cpu])
         continue;
      /*
       * Enable some caching for the instruction stream.
       * Can't cache data yet 'cause device addresses can never
       * be cached, and we don't have those no-caching zones
       * set up yet....
       */
      tmp =
      (0x00000 << 12) | /* segment table base address */
      AREA_D_WT |       /* write through */
      AREA_D_G  |       /* global */
      AREA_D_CI |       /* cache inhibit */
      ! AREA_D_TE ;     /* not translation enable */
      /*
      REGS(cpu, INST_CMMU).sapr = tmp;
      */
      m18x_cmmu_set(CMMU_SAPR, tmp, MODE_VAL,
               cpu, INST_CMMU, 0, 0);

      /*
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_SUPER_ALL;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_SUPER_ALL, ACCESS_VAL|MODE_VAL,
               cpu, DATA_CMMU, CMMU_ACS_SUPER, 0);
   }
}


/*
 * Just before poweroff or reset....
 */
void
m18x_cmmu_shutdown_now(void)
{
   unsigned tmp;
   unsigned cmmu_num;

   /*
    * Now set some state as we like...
    */
   for (cmmu_num = 0; cmmu_num < MAX_CMMUS; cmmu_num++) {
      if (cputyp == CPU_188) {
         tmp =
         ! CMMU_SCTR_PE |   /* parity enable */
         ! CMMU_SCTR_SE |   /* snoop enable */
         ! CMMU_SCTR_PR ;   /* priority arbitration */
      } else {
         tmp =
         ! CMMU_SCTR_PE |   /* parity enable */
         ! CMMU_SCTR_PR ;   /* priority arbitration */
      }

      cmmu[cmmu_num].cmmu_regs->sctr = tmp;

      tmp = 
      (0x00000 << 12) |  /* segment table base address */
      ! AREA_D_WT |      /* write through */
      ! AREA_D_G  |      /* global */
      AREA_D_CI |        /* cache inhibit */
      ! AREA_D_TE ;      /* translation disable */

      cmmu[cmmu_num].cmmu_regs->sapr = tmp;
      cmmu[cmmu_num].cmmu_regs->uapr = tmp;
   }
}

#define PARITY_ENABLE
/*
 * enable parity
 */
void m18x_cmmu_parity_enable(void)
{
#ifdef	PARITY_ENABLE
   register int cmmu_num;

   for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
      if (m18x_cmmu_alive(cmmu_num)) {
         register unsigned val1 = m18x_cmmu_get(cmmu_num, CMMU_SCTR);

         /*
         cmmu[cmmu_num].cmmu_regs->sctr |= CMMU_SCTR_PE;
         */
         m18x_cmmu_set(CMMU_SCTR, val1 | CMMU_SCTR_PE, NUM_CMMU,
                  cmmu_num, 0, 0, 0);
      }
   }
#endif  /* PARITY_ENABLE */
}

/*
 * Find out the CPU number from accessing CMMU
 * Better be at splhigh, or even better, with interrupts
 * disabled.
 */
#define ILLADDRESS	U(0x0F000000) 	/* any faulty address */

unsigned m18x_cmmu_cpu_number(void)
{
   register unsigned cmmu_no;
   int i;


   for (i=0; i < 10; i++) {
      /* clear CMMU p-bus status registers */
      for (cmmu_no = 0; cmmu_no < MAX_CMMUS; cmmu_no++) {
         if (cmmu[cmmu_no].cmmu_alive == CMMU_AVAILABLE &&
             cmmu[cmmu_no].which == DATA_CMMU)
            cmmu[cmmu_no].cmmu_regs->pfSTATUSr = 0;
      }

      /* access faulting address */
      badwordaddr((void *)ILLADDRESS);

      /* check which CMMU reporting the fault  */
      for (cmmu_no = 0; cmmu_no < MAX_CMMUS; cmmu_no++) {
         if (cmmu[cmmu_no].cmmu_alive == CMMU_AVAILABLE &&
             cmmu[cmmu_no].which == DATA_CMMU &&
             cmmu[cmmu_no].cmmu_regs->pfSTATUSr & 0x70000) {
            if (cmmu[cmmu_no].cmmu_regs->pfSTATUSr & 0x70000) {
               cmmu[cmmu_no].cmmu_regs->pfSTATUSr = 0; /* to be clean */
               cmmu[cmmu_no].cmmu_alive = CMMU_MARRIED;
               return cmmu[cmmu_no].cmmu_cpu;
            }
         }
      }
   }
   panic("m18x_cmmu_cpu_number: could not determine my cpu number");
   return 0; /* to make compiler happy */
}

/**
 **	Funcitons that actually modify CMMU registers.
 **/

#if !DDB
static
#endif
void
m18x_cmmu_remote_set(unsigned cpu, unsigned r, unsigned data, unsigned x)
{
   *(volatile unsigned *)(r + (char*)&REGS(cpu,data)) = x;
}

/*
 * cmmu_cpu_lock should be held when called if read
 * the CMMU_SCR or CMMU_SAR.
 */
#if !DDB
static
#endif
unsigned
m18x_cmmu_remote_get(unsigned cpu, unsigned r, unsigned data)
{
   return (*(volatile unsigned *)(r + (char*)&REGS(cpu,data)));
}

/* Needs no locking - read only registers */
unsigned
m18x_cmmu_get_idr(unsigned data)
{
   int cpu;
   cpu = cpu_number();
   return REGS(cpu,data).idr;
}

void
m18x_cmmu_set_sapr(unsigned ap)
{
   int cpu;
   cpu = cpu_number();

   if (cache_policy & CACHE_INH)
      ap |= AREA_D_CI;
   /*
  REGS(cpu, INST_CMMU).sapr = ap;
  REGS(cpu, DATA_CMMU).sapr = ap;
   */
   m18x_cmmu_set(CMMU_SAPR, ap, ACCESS_VAL,
            cpu, 0, CMMU_ACS_SUPER, 0);
}

void
m18x_cmmu_remote_set_sapr(unsigned cpu, unsigned ap)
{
   if (cache_policy & CACHE_INH)
      ap |= AREA_D_CI;

   /*
  REGS(cpu, INST_CMMU).sapr = ap;
  REGS(cpu, DATA_CMMU).sapr = ap;
   */
   m18x_cmmu_set(CMMU_SAPR, ap, ACCESS_VAL,
            cpu, 0, CMMU_ACS_SUPER, 0);
}

void
m18x_cmmu_set_uapr(unsigned ap)
{
   int cpu;
   cpu = cpu_number();

   /* this functionality also mimiced in m18x_cmmu_pmap_activate() */
   /*
  REGS(cpu, INST_CMMU).uapr = ap;
  REGS(cpu, DATA_CMMU).uapr = ap;
   */
   m18x_cmmu_set(CMMU_UAPR, ap, ACCESS_VAL,
            cpu, 0, CMMU_ACS_USER, 0);
}

/*
 * Set batc entry number entry_no to value in 
 * the data or instruction cache depending on data.
 *
 * Except for the cmmu_init, this function, m18x_cmmu_set_pair_batc_entry,
 * and m18x_cmmu_pmap_activate are the only functions which may set the
 * batc values.
 */
void
m18x_cmmu_set_batc_entry(
                   unsigned cpu,
                   unsigned entry_no,
                   unsigned data,   /* 1 = data, 0 = instruction */
                   unsigned value)  /* the value to stuff into the batc */
{
   /*
   REGS(cpu,data).bwp[entry_no] = value;
   */
   m18x_cmmu_set(CMMU_BWP(entry_no), value, MODE_VAL|ACCESS_VAL,
            cpu, data, CMMU_ACS_USER, 0);
#if SHADOW_BATC
   CMMU(cpu,data)->batc[entry_no] = value;
#endif
#if 0 /* was for debugging piece (peace?) of mind */
   REGS(cpu,data).scr = CMMU_FLUSH_SUPER_ALL;
   REGS(cpu,data).scr = CMMU_FLUSH_USER_ALL;
#endif
}

/*
 * Set batc entry number entry_no to value in 
 * the data and instruction cache for the named CPU.
 */
void
m18x_cmmu_set_pair_batc_entry(
                        unsigned cpu,
                        unsigned entry_no,
                        unsigned value)  /* the value to stuff into the batc */
{

   /*
   REGS(cpu,DATA_CMMU).bwp[entry_no] = value;
   */
   m18x_cmmu_set(CMMU_BWP(entry_no), value, MODE_VAL|ACCESS_VAL,
            cpu, DATA_CMMU, CMMU_ACS_USER, 0);
#if SHADOW_BATC
   CMMU(cpu,DATA_CMMU)->batc[entry_no] = value;
#endif
   /*
   REGS(cpu,INST_CMMU).bwp[entry_no] = value;
   */
   m18x_cmmu_set(CMMU_BWP(entry_no), value, MODE_VAL|ACCESS_VAL,
            cpu, INST_CMMU, CMMU_ACS_USER, 0);
#if SHADOW_BATC
   CMMU(cpu,INST_CMMU)->batc[entry_no] = value;
#endif

#if 0  /* was for debugging piece (peace?) of mind */
   REGS(cpu,INST_CMMU).scr = CMMU_FLUSH_SUPER_ALL;
   REGS(cpu,INST_CMMU).scr = CMMU_FLUSH_USER_ALL;
   REGS(cpu,DATA_CMMU).scr = CMMU_FLUSH_SUPER_ALL;
   REGS(cpu,DATA_CMMU).scr = CMMU_FLUSH_USER_ALL;
#endif
}

/**
 **	Functions that invalidate TLB entries.
 **/

/*
 *	flush any tlb
 *	Some functionality mimiced in m18x_cmmu_pmap_activate.
 */
void
m18x_cmmu_flush_remote_tlb(unsigned cpu, unsigned kernel, vm_offset_t vaddr, int size)
{
   register s = splhigh();

   if (cpu > max_cpus) {
      cpu = cpu_number();
   }

   if ((unsigned)size > M88K_PGBYTES) {
      /*
      REGS(cpu, INST_CMMU).scr =
      REGS(cpu, DATA_CMMU).scr =
      kernel ? CMMU_FLUSH_SUPER_ALL : CMMU_FLUSH_USER_ALL;
          */

      m18x_cmmu_set(CMMU_SCR, kernel ? CMMU_FLUSH_SUPER_ALL : CMMU_FLUSH_USER_ALL, ACCESS_VAL,
               cpu, 0, kernel ? CMMU_ACS_SUPER : CMMU_ACS_USER, 0);
   } else { /* a page or smaller */


      /*
      REGS(cpu, INST_CMMU).sar = (unsigned)vaddr;
      REGS(cpu, DATA_CMMU).sar = (unsigned)vaddr;
      */
      m18x_cmmu_set(CMMU_SAR, vaddr, ADDR_VAL|ACCESS_VAL,
               cpu, 0, kernel ? CMMU_ACS_SUPER : CMMU_ACS_USER, vaddr);

      /*
      REGS(cpu, INST_CMMU).scr =
      REGS(cpu, DATA_CMMU).scr =
      kernel ? CMMU_FLUSH_SUPER_PAGE : CMMU_FLUSH_USER_PAGE;
          */
      m18x_cmmu_set(CMMU_SCR, kernel ? CMMU_FLUSH_SUPER_PAGE : CMMU_FLUSH_USER_PAGE, ADDR_VAL|ACCESS_VAL,
               cpu, 0, kernel ? CMMU_ACS_SUPER : CMMU_ACS_USER, vaddr);
   }
   
   splx(s);
}

/*
 *	flush my personal tlb
 */
void
m18x_cmmu_flush_tlb(unsigned kernel, vm_offset_t vaddr, int size)
{
   int cpu;
   cpu = cpu_number();
   m18x_cmmu_flush_remote_tlb(cpu, kernel, vaddr, size);
}

/*
 * New fast stuff for pmap_activate.
 * Does what a few calls used to do.
 * Only called from pmap.c's _pmap_activate().
 */
void
m18x_cmmu_pmap_activate(
                  unsigned cpu,
                  unsigned uapr,
                  batc_template_t i_batc[BATC_MAX],
                  batc_template_t d_batc[BATC_MAX])
{
   int entry_no;


   /* the following is from m18x_cmmu_set_uapr */
   /*
   REGS(cpu, INST_CMMU).uapr = uapr;
   REGS(cpu, DATA_CMMU).uapr = uapr;
   */
   m18x_cmmu_set(CMMU_UAPR, uapr, ACCESS_VAL,
            cpu, 0, CMMU_ACS_USER, 0);

   for (entry_no = 0; entry_no < BATC_MAX; entry_no++) {
      /*
      REGS(cpu,INST_CMMU).bwp[entry_no] = i_batc[entry_no].bits;
      REGS(cpu,DATA_CMMU).bwp[entry_no] = d_batc[entry_no].bits;
      */
      m18x_cmmu_set(CMMU_BWP(entry_no), i_batc[entry_no].bits, MODE_VAL|ACCESS_VAL,
               cpu, INST_CMMU, CMMU_ACS_USER, 0);
      m18x_cmmu_set(CMMU_BWP(entry_no), d_batc[entry_no].bits, MODE_VAL|ACCESS_VAL,
               cpu, DATA_CMMU, CMMU_ACS_USER, 0);
#if SHADOW_BATC
      CMMU(cpu,INST_CMMU)->batc[entry_no] = i_batc[entry_no].bits;
      CMMU(cpu,DATA_CMMU)->batc[entry_no] = d_batc[entry_no].bits;
#endif
   }
   

   /*
    * Flush the user TLB.
    * IF THE KERNEL WILL EVER CARE ABOUT THE BATC ENTRIES,
    * THE SUPERVISOR TLBs SHOULB EE FLUSHED AS WELL.
    */
   /*
   REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_USER_ALL;
   REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_USER_ALL;
   */
   m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_USER_ALL, ACCESS_VAL,
            cpu, 0, CMMU_ACS_USER, 0);
}

/**
 **	Functions that invalidate caches.
 **
 ** Cache invalidates require physical addresses.  Care must be exercised when
 ** using segment invalidates.  This implies that the starting physical address
 ** plus the segment length should be invalidated.  A typical mistake is to
 ** extract the first physical page of a segment from a virtual address, and
 ** then expecting to invalidate when the pages are not physically contiguous.
 **
 ** We don't push Instruction Caches prior to invalidate because they are not
 ** snooped and never modified (I guess it doesn't matter then which form
 ** of the command we use then).
 **/
/*
 *	flush both Instruction and Data caches
 */
void
m18x_cmmu_flush_remote_cache(int cpu, vm_offset_t physaddr, int size)
{
   register s = splhigh();


#if !defined(BROKEN_MMU_MASK)

   if (size < 0 || size > NBSG ) {


      /*
         REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
         REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, 0,
               cpu, 0, 0, 0);
   } else if (size <= 16) {


      /*
         REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
         REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, ADDR_VAL,
               cpu, 0, 0, (unsigned)physaddr);
      /*
         REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_LINE;
         REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_LINE;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE , ADDR_VAL,
               cpu, 0, 0, (unsigned)physaddr);
   } else if (size <= NBPG) {


      /*
         REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
         REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, ADDR_VAL,
               cpu, 0, 0, (unsigned)physaddr);
      /*
         REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_PAGE;
         REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_PAGE;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE , ADDR_VAL,
               cpu, 0, 0, (unsigned)physaddr);
   } else {
      /*
         REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
         REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, 0,
               cpu, 0, 0, 0);
      /*
         REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_SEGMENT;
         REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_SEGMENT;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT, 0,
               cpu, 0, 0, 0);
   }

#else
   /*
   REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
   REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
   */
   m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, 0,
            cpu, 0, 0, 0);
#endif /* !BROKEN_MMU_MASK */

   
   splx(s);
}

/*
 *	flush both Instruction and Data caches
 */
void
m18x_cmmu_flush_cache(vm_offset_t physaddr, int size)
{
   int cpu = cpu_number();
   m18x_cmmu_flush_remote_cache(cpu, physaddr, size);
}

/*
 *	flush Instruction caches
 */
void
m18x_cmmu_flush_remote_inst_cache(int cpu, vm_offset_t physaddr, int size)
{
   register s = splhigh();



#if !defined(BROKEN_MMU_MASK)
   if (size < 0 || size > NBSG ) {
      /*
         REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
   } else if (size <= 16) {

      /*
         REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      /*
         REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_LINE;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
   } else if (size <= NBPG) {
      /*
         REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      /*
         REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_PAGE;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
   } else {
      /*
         REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
      /*
         REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_SEGMENT;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
   }
#else
   /*
   REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
   */
   m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
            cpu, INST_CMMU, 0, 0);
#endif /* !BROKEN_MMU_MASK */
   
   splx(s);
}

/*
 *	flush Instruction caches
 */
void
m18x_cmmu_flush_inst_cache(vm_offset_t physaddr, int size)
{
   int cpu;
   cpu = cpu_number();
   m18x_cmmu_flush_remote_inst_cache(cpu, physaddr, size);
}

void
m18x_cmmu_flush_remote_data_cache(int cpu, vm_offset_t physaddr, int size)
{ 
   register s = splhigh();



#if !defined(BROKEN_MMU_MASK)
   if (size < 0 || size > NBSG ) {

      /*
         REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   } else if (size <= 16) {
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_LINE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);

   } else if (size <= NBPG) {
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_PAGE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
   } else {
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_SEGMENT;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   }
#else
   /*
   REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
   */
   m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
            cpu, DATA_CMMU, 0, 0);
#endif /* !BROKEN_MMU_MASK */

   

   splx(s);
}

/*
 * flush data cache
 */ 
void
m18x_cmmu_flush_data_cache(vm_offset_t physaddr, int size)
{ 
   int cpu;
   cpu = cpu_number();
   m18x_cmmu_flush_remote_data_cache(cpu, physaddr, size);
}

/*
 * sync dcache (and icache too)
 */
void
m18x_cmmu_sync_cache(vm_offset_t physaddr, int size)
{
   register s = splhigh();
   int cpu;
   cpu = cpu_number();



#if !defined(BROKEN_MMU_MASK)
   if (size < 0 || size > NBSG ) {
      /*
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CB_ALL;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CB_ALL;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
   } else if (size <= 16) {
      /*
      REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CB_LINE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_LINE, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CB_LINE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_LINE, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   } else if (size <= NBPG) {
      /*
      REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CB_PAGE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_PAGE, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CB_PAGE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_PAGE, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   } else {
      /*
      REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CB_SEGMENT;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_SEGMENT, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CB_SEGMENT;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_SEGMENT, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   }
#else
   /*
   REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CB_ALL;
   REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CB_ALL;
   */
   m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
            cpu, DATA_CMMU, 0, 0);
   m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
            cpu, INST_CMMU, 0, 0);
#endif /* !BROKEN_MMU_MASK */

   

   splx(s);
}

void
m18x_cmmu_sync_inval_cache(vm_offset_t physaddr, int size)
{
   register s = splhigh();
   int cpu;
   cpu = cpu_number();



#if !defined(BROKEN_MMU_MASK)
   if (size < 0 || size > NBSG ) {
      /*
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
   } else if (size <= 16) {
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_LINE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_LINE, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
      /*
      REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_LINE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_LINE, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   } else if (size <= NBPG) {
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_PAGE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_PAGE, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
      /*
      REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_PAGE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_PAGE, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   } else {
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_SEGMENT;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_SEGMENT, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
      /*
      REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_SEGMENT;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_SEGMENT, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   }

#else
   /*
   REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
   REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_CBI_ALL;
   */
   m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
            cpu, DATA_CMMU, 0, 0);
   m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
            cpu, INST_CMMU, 0, 0);
#endif /* !BROKEN_MMU_MASK */

   

   splx(s);
}

void
m18x_cmmu_inval_cache(vm_offset_t physaddr, int size)
{
   register s = splhigh();
   int cpu;
   cpu = cpu_number();



#if !defined(BROKEN_MMU_MASK)
   if (size < 0 || size > NBSG ) {
      /*
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_INV_ALL;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_INV_ALL;
      */
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
      m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
   } else if (size <= 16) {
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_INV_LINE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_LINE, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
      /*
      REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_INV_LINE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_LINE, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   } else if (size <= NBPG) {
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_INV_PAGE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_PAGE, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
      /*
      REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_INV_PAGE;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_PAGE, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   } else {
      /*
      REGS(cpu, DATA_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_INV_SEGMENT;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, INST_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_SEGMENT, MODE_VAL,
               cpu, INST_CMMU, 0, 0);
      /*
      REGS(cpu, INST_CMMU).sar = (unsigned)physaddr;
      REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_INV_SEGMENT;
      */
      m18x_cmmu_set(CMMU_SAR, (unsigned)physaddr, MODE_VAL|ADDR_VAL,
               cpu, DATA_CMMU, 0, (unsigned)physaddr);
      m18x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_SEGMENT, MODE_VAL,
               cpu, DATA_CMMU, 0, 0);
   }
#else
   /*
   REGS(cpu, DATA_CMMU).scr = CMMU_FLUSH_CACHE_INV_ALL;
   REGS(cpu, INST_CMMU).scr = CMMU_FLUSH_CACHE_INV_ALL;
   */
   m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
            cpu, DATA_CMMU, 0, 0);
   m18x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
            cpu, INST_CMMU, 0, 0);
#endif /* !BROKEN_MMU_MASK */

   

   splx(s);
}

void
m18x_dma_cachectl(vm_offset_t va, int size, int op)
{
   int count;

#if !defined(BROKEN_MMU_MASK)
   while (size) {

      count = NBPG - ((int)va & PGOFSET);

      if (size < count)
         count = size;

      if (op == DMA_CACHE_SYNC)
         m18x_cmmu_sync_cache(kvtop(va), count);
      else if (op == DMA_CACHE_SYNC_INVAL)
         m18x_cmmu_sync_inval_cache(kvtop(va), count);
      else
         m18x_cmmu_inval_cache(kvtop(va), count);

      va = (vm_offset_t)((int)va + count);
      size -= count;
   }
#else

   if (op == DMA_CACHE_SYNC)
      m18x_cmmu_sync_cache(kvtop(va), size);
   else if (op == DMA_CACHE_SYNC_INVAL)
      m18x_cmmu_sync_inval_cache(kvtop(va), size);
   else
      m18x_cmmu_inval_cache(kvtop(va), size);
#endif /* !BROKEN_MMU_MASK */
}

#if DDB
union ssr {
   unsigned bits;
   struct {
      unsigned  :16,
      ce:1,
      be:1,
      :4,
      wt:1,
      sp:1,
      g:1,
      ci:1,
      :1,
      m:1,
      u:1,
      wp:1,
      bh:1,
      v:1;
   } field;
};

union cssp {
   unsigned bits;
   struct {
      unsigned   : 2,
      l: 6,
      d3: 1,
      d2: 1,
      d1: 1,
      d0: 1,
      vv3: 2,
      vv2: 2,
      vv1: 2,
      vv0: 2,
      :12;
   } field;
};

union batcu {
   unsigned bits;
   struct {              /* block address translation register */
      unsigned int
      lba:13,            /* logical block address */
      pba:13,            /* physical block address */
      s:1,               /* supervisor */
      wt:4,              /* write through */
      g:1,               /* global */
      ci:1,              /* cache inhibit */
      wp:1,              /* write protect */
      v:1;               /* valid */
   } field;
};

   #define VV_EX_UNMOD		0
   #define VV_EX_MOD		1
   #define VV_SHARED_UNMOD		2
   #define VV_INVALID		3

   #define D(UNION, LINE) \
	((LINE) == 3 ? (UNION).field.d3 : \
	 ((LINE) == 2 ? (UNION).field.d2 : \
	  ((LINE) == 1 ? (UNION).field.d1 : \
	   ((LINE) == 0 ? (UNION).field.d0 : ~0))))
   #define VV(UNION, LINE) \
	((LINE) == 3 ? (UNION).field.vv3 : \
	 ((LINE) == 2 ? (UNION).field.vv2 : \
	  ((LINE) == 1 ? (UNION).field.vv1 : \
	   ((LINE) == 0 ? (UNION).field.vv0 : ~0))))


   #undef VEQR_ADDR
   #define  VEQR_ADDR 0

/*
 * Show (for debugging) how the given CMMU translates the given ADDRESS.
 * If cmmu == -1, the data cmmu for the current cpu is used.
 */
void
m18x_cmmu_show_translation(
                     unsigned address,
                     unsigned supervisor_flag,
                     unsigned verbose_flag,
                     int cmmu_num)
{
   /*
    * A virtual address is split into three fields. Two are used as
    * indicies into tables (segment and page), and one is an offset into
    * a page of memory.
    */
   union {
      unsigned bits;
      struct {
         unsigned segment_table_index:10,
         page_table_index:10,
         page_offset:12;
      } field;
   } virtual_address;
   unsigned value;

   if (verbose_flag)
      db_printf("-------------------------------------------\n");



   /****** ACCESS PROPER CMMU or THREAD ***********/
   #if 0 /* no thread */
   if (thread != 0) {
      /* the following tidbit from _pmap_activate in m88k/pmap.c */
      register apr_template_t apr_data;
      supervisor_flag = 0; /* thread implies user */

      if (thread->task == 0) {
         db_printf("[thread %x has empty task pointer]\n", thread);
         return;
      } else if (thread->task->map == 0) {
         db_printf("[thread/task %x/%x has empty map pointer]\n",
                   thread, thread->task);
         return;
      } else if (thread->task->map->pmap == 0) {
         db_printf("[thread/task/map %x/%x/%x has empty pmap pointer]\n",
                   thread, thread->task, thread->task->map);
         return;
      }
      if (thread->task->map->pmap->lock.lock_data) {
         db_printf("[Warning: thread %x's task %x's map %x's "
                   "pmap %x is locked]\n", thread, thread->task,
                   thread->task->map, thread->task->map->pmap);
      }
      apr_data.bits = 0;
      apr_data.field.st_base = M88K_BTOP(thread->task->map->pmap->sdt_paddr);
      apr_data.field.wt = 0;
      apr_data.field.g  = 1;
      apr_data.field.ci = 0;
      apr_data.field.te = 1;
      value = apr_data.bits;
      if (verbose_flag) {
         db_printf("[thread %x task %x map %x pmap %x UAPR is %x]\n",
                   thread, thread->task, thread->task->map,
                   thread->task->map->pmap, value);
      }
   } else
   #endif /* 0 */
   {
      if (cmmu_num == -1) {
         if (cpu_cmmu[0].pair[DATA_CMMU] == 0) {
            db_printf("ack! can't figure my own data cmmu number.\n");
            return;
         }
         cmmu_num = cpu_cmmu[0].pair[DATA_CMMU] - cmmu;
         if (verbose_flag)
            db_printf("The data cmmu for cpu#%d is cmmu#%d.\n",
                      0, cmmu_num);
      } else if (cmmu_num < 0 || cmmu_num >= MAX_CMMUS) {
         db_printf("invalid cpu number [%d]... must be in range [0..%d]\n",
                   cmmu_num, MAX_CMMUS - 1);
         
         return;
      }

      if (cmmu[cmmu_num].cmmu_alive == 0) {
         db_printf("warning: cmmu %d is not alive.\n", cmmu_num);
   #if 0
         
         return;
   #endif
      }

      if (!verbose_flag) {
         if (!(cmmu[cmmu_num].cmmu_regs->sctr & CMMU_SCTR_SE))
            db_printf("WARNING: snooping not enabled for CMMU#%d.\n",
                      cmmu_num);
      } else {
         int i;
         for (i=0; i<MAX_CMMUS; i++)
            if ((i == cmmu_num || cmmu[i].cmmu_alive) &&
                (verbose_flag>1 || !(cmmu[i].cmmu_regs->sctr&CMMU_SCTR_SE))) {
               db_printf("CMMU#%d (cpu %d %s) snooping %s\n", i,
                         cmmu[i].cmmu_cpu, cmmu[i].which ? "data" : "inst",
                         (cmmu[i].cmmu_regs->sctr & CMMU_SCTR_SE) ? "on":"OFF");
            }
      }

      if (supervisor_flag)
         value = cmmu[cmmu_num].cmmu_regs->sapr;
      else
         value = cmmu[cmmu_num].cmmu_regs->uapr;

   }

   /******* LOOK AT THE BATC ** (if not a thread) **************/
   #if 0
      #if SHADOW_BATC
   if (thread == 0) {
      int i;
      union batcu batc;
      for (i = 0; i < 8; i++) {
         batc.bits = cmmu[cmmu_num].batc[i];
         if (batc.field.v == 0) {
            if (verbose_flag>1)
               db_printf("cmmu #%d batc[%d] invalid.\n", cmmu_num, i);
         } else {
            db_printf("cmmu#%d batc[%d] v%08x p%08x", cmmu_num, i,
                      batc.field.lba << 18, batc.field.pba);
            if (batc.field.s)  db_printf(", supervisor");
            if (batc.field.wt) db_printf(", wt.th");
            if (batc.field.g)  db_printf(", global");
            if (batc.field.ci) db_printf(", cache inhibit");
            if (batc.field.wp) db_printf(", write protect");
         }
      }
   }
      #endif
   #endif /* 0 */

   /******* SEE WHAT A PROBE SAYS (if not a thread) ***********/
   #if 0
   if (thread == 0)
   #endif /* 0 */
   {
      union ssr ssr;
      struct cmmu_regs *cmmu_regs = cmmu[cmmu_num].cmmu_regs;
      cmmu_regs->sar = address;
      cmmu_regs->scr = supervisor_flag ? CMMU_PROBE_SUPER : CMMU_PROBE_USER;
      ssr.bits = cmmu_regs->ssr;
      if (verbose_flag > 1)
         db_printf("probe of 0x%08x returns ssr=0x%08x\n",
                   address, ssr.bits);
      if (ssr.field.v)
         db_printf("PROBE of 0x%08x returns phys=0x%x",
                   address, cmmu_regs->sar);
      else
         db_printf("PROBE fault at 0x%x", cmmu_regs->pfADDRr);
      if (ssr.field.ce) db_printf(", copyback err");
      if (ssr.field.be) db_printf(", bus err");
      if (ssr.field.wt) db_printf(", writethrough");
      if (ssr.field.sp) db_printf(", sup prot");
      if (ssr.field.g)  db_printf(", global");
      if (ssr.field.ci) db_printf(", cache inhibit");
      if (ssr.field.m)  db_printf(", modified");
      if (ssr.field.u)  db_printf(", used");
      if (ssr.field.wp) db_printf(", write prot");
      if (ssr.field.bh) db_printf(", BATC");
      db_printf(".\n");
   }

   /******* INTERPRET AREA DESCRIPTOR *********/
   {
      union apr_template apr_template;
      apr_template.bits = value;
      if (verbose_flag > 1) {
         db_printf("CMMU#%d", cmmu_num);
   #if 0
         if (thread == 0)
            db_printf("CMMU#%d", cmmu_num);
         else
            db_printf("THREAD %x", thread);
   #endif /* 0 */
         db_printf(" %cAPR is 0x%08x\n",
                   supervisor_flag ? 'S' : 'U', apr_template.bits);
      }
      db_printf("CMMU#%d", cmmu_num);
   #if 0
      if (thread == 0)
         db_printf("CMMU#%d", cmmu_num);
      else
         db_printf("THREAD %x", thread);
   #endif /* 0 */
      db_printf(" %cAPR: SegTbl: 0x%x000p",
                supervisor_flag ? 'S' : 'U', apr_template.field.st_base);
      if (apr_template.field.wt) db_printf(", WTHRU");
      else                       db_printf(", !wthru");
      if (apr_template.field.g)  db_printf(", GLOBAL");
      else                       db_printf(", !global");
      if (apr_template.field.ci) db_printf(", $INHIBIT");
      else                       db_printf(", $ok");
      if (apr_template.field.te) db_printf(", VALID");
      else                       db_printf(", !valid");
      db_printf(".\n");

      /* if not valid, done now */
      if (apr_template.field.te == 0) {
         db_printf("<would report an error, valid bit not set>\n");
         
         return;
      }

      value = apr_template.field.st_base << 12; /* now point to seg page */
   }

   /* translate value from physical to virtual */
   if (verbose_flag)
      db_printf("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
   value += VEQR_ADDR;

   virtual_address.bits = address;

   /****** ACCESS SEGMENT TABLE AND INTERPRET SEGMENT DESCRIPTOR  *******/
   {
      union sdt_entry_template std_template;
      if (verbose_flag)
         db_printf("will follow to entry %d of page at 0x%x...\n",
                   virtual_address.field.segment_table_index, value);
      value |= virtual_address.field.segment_table_index *
               sizeof(struct sdt_entry);

      if (badwordaddr(value)) {
         db_printf("ERROR: unable to access page at 0x%08x.\n", value);
         
         return;
      }

      std_template.bits = *(unsigned *)value;
      if (verbose_flag > 1)
         db_printf("SEG DESC @0x%x is 0x%08x\n", value, std_template.bits);
      db_printf("SEG DESC @0x%x: PgTbl: 0x%x000",
                value, std_template.sdt_desc.table_addr);
      if (std_template.sdt_desc.wt)       db_printf(", WTHRU");
      else                                db_printf(", !wthru");
      if (std_template.sdt_desc.sup)      db_printf(", S-PROT");
      else                                db_printf(", UserOk");
      if (std_template.sdt_desc.g)        db_printf(", GLOBAL");
      else                                db_printf(", !global");
      if (std_template.sdt_desc.no_cache) db_printf(", $INHIBIT");
      else                                db_printf(", $ok");
      if (std_template.sdt_desc.prot)     db_printf(", W-PROT");
      else                                db_printf(", WriteOk");
      if (std_template.sdt_desc.dtype)    db_printf(", VALID");
      else                                db_printf(", !valid");
      db_printf(".\n");

      /* if not valid, done now */
      if (std_template.sdt_desc.dtype == 0) {
         db_printf("<would report an error, STD entry not valid>\n");
         
         return;
      }

      value = std_template.sdt_desc.table_addr << 12;
   }

   /* translate value from physical to virtual */
   if (verbose_flag)
      db_printf("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
   value += VEQR_ADDR;

   /******* PAGE TABLE *********/
   {
      union pte_template pte_template;
      if (verbose_flag)
         db_printf("will follow to entry %d of page at 0x%x...\n",
                   virtual_address.field.page_table_index, value);
      value |= virtual_address.field.page_table_index *
               sizeof(struct pt_entry);

      if (badwordaddr(value)) {
         db_printf("error: unable to access page at 0x%08x.\n", value);
         
         return;
      }

      pte_template.bits = *(unsigned *)value;
      if (verbose_flag > 1)
         db_printf("PAGE DESC @0x%x is 0x%08x.\n", value, pte_template.bits);
      db_printf("PAGE DESC @0x%x: page @%x000",
                value, pte_template.pte.pfn);
      if (pte_template.pte.wired)    db_printf(", WIRE");
      else                           db_printf(", !wire");
      if (pte_template.pte.wt)       db_printf(", WTHRU");
      else                           db_printf(", !wthru");
      if (pte_template.pte.sup)      db_printf(", S-PROT");
      else                           db_printf(", UserOk");
      if (pte_template.pte.g)        db_printf(", GLOBAL");
      else                           db_printf(", !global");
      if (pte_template.pte.ci)       db_printf(", $INHIBIT");
      else                           db_printf(", $ok");
      if (pte_template.pte.modified) db_printf(", MOD");
      else                           db_printf(", !mod");
      if (pte_template.pte.pg_used)  db_printf(", USED");
      else                           db_printf(", !used");
      if (pte_template.pte.prot)     db_printf(", W-PROT");
      else                           db_printf(", WriteOk");
      if (pte_template.pte.dtype)    db_printf(", VALID");
      else                           db_printf(", !valid");
      db_printf(".\n");

      /* if not valid, done now */
      if (pte_template.pte.dtype == 0) {
         db_printf("<would report an error, PTE entry not valid>\n");
         
         return;
      }

      value = pte_template.pte.pfn << 12;
      if (verbose_flag)
         db_printf("will follow to byte %d of page at 0x%x...\n",
                   virtual_address.field.page_offset, value);
      value |= virtual_address.field.page_offset;

      if (badwordaddr(value)) {
         db_printf("error: unable to access page at 0x%08x.\n", value);
         
         return;
      }
   }

   /* translate value from physical to virtual */
   if (verbose_flag)
      db_printf("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
   value += VEQR_ADDR;

   db_printf("WORD at 0x%x is 0x%08x.\n", value, *(unsigned *)value);
   
}


void
m18x_cmmu_cache_state(unsigned addr, unsigned supervisor_flag)
{
   static char *vv_name[4] =
   {"exclu-unmod", "exclu-mod", "shared-unmod", "invalid"};
   int cmmu_num;

   for (cmmu_num = 0; cmmu_num < MAX_CMMUS; cmmu_num++) {
      union ssr ssr;
      union cssp cssp;
      struct cmmu_regs *R;
      unsigned tag, line;
      if (!cmmu[cmmu_num].cmmu_alive)
         continue;
      R = cmmu[cmmu_num].cmmu_regs;
      db_printf("cmmu #%d %s cmmu for cpu %d.\n", cmmu_num,
                cmmu[cmmu_num].which ? "data" : "inst", 
                cmmu[cmmu_num].cmmu_cpu);
      R->sar = addr;
      R->scr = supervisor_flag ? CMMU_PROBE_SUPER : CMMU_PROBE_USER;

      ssr.bits = R->ssr;
      if (!ssr.field.v) {
         db_printf("PROBE of 0x%08x faults.\n",addr);
         continue;
      }
      db_printf("PROBE of 0x%08x returns phys=0x%x", addr, R->sar);

      tag = R->sar & ~0xfff;
      cssp.bits = R->cssp;

      /* check to see if any of the tags for the set match the address */
      for (line = 0; line < 4; line++) {
         if (VV(cssp, line) == VV_INVALID) {
            db_printf("line %d invalid.\n", line);
            continue; /* line is invalid */
         }
         if (D(cssp, line)) {
            db_printf("line %d disabled.\n", line);
            continue; /* line is disabled */
         }

         if ((R->ctp[line] & ~0xfff) != tag) {
            db_printf("line %d address tag is %x.\n", line,
                      (R->ctp[line] & ~0xfff));
            continue;
         }
         db_printf("found in line %d as %08x (%s).\n",
                   line, R->cdp[line], vv_name[VV(cssp, line)]);
      }
   }
   
}

void
m18x_show_cmmu_info(unsigned addr)
{
   int cmmu_num;
   m18x_cmmu_cache_state(addr, 1);

   for (cmmu_num = 0; cmmu_num < MAX_CMMUS; cmmu_num++)
      if (cmmu[cmmu_num].cmmu_alive) {
         db_printf("cmmu #%d %s cmmu for cpu %d: ", cmmu_num,
                   cmmu[cmmu_num].which ? "data" : "inst", 
                   cmmu[cmmu_num].cmmu_cpu);
         m18x_cmmu_show_translation(addr, 1, 0, cmmu_num);
      }
}
#endif /* end if DDB */
