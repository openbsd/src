/* $OpenBSD: machdep.c,v 1.19 2000/02/22 19:27:55 deraadt Exp $	*/
/*
 * Copyright (c) 1998, 1999 Steve Murphree, Jr.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
   #include <sys/msg.h>
#endif
#ifdef SYSVSEM
   #include <sys/sem.h>
#endif
#ifdef SYSVSHM
   #include <sys/shm.h>
#endif
#include <sys/ioctl.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <net/netisr.h>

#include <mvme88k/dev/sysconreg.h>
#include <mvme88k/dev/pcctworeg.h>
#include <machine/cpu.h>
#include <machine/cpu_number.h>
#include <machine/asm_macro.h>   /* enable/disable interrupts */
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/bug.h>
#include <machine/prom.h>

#include <dev/cons.h>

#include <vm/vm.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#define __IS_MACHDEP_C__
#include <assym.s>			  /* EF_EPSR, etc. */
#include <machine/m88100.h>  			/* DMT_VALID        */
#include <machine/m882xx.h>  			/* CMMU stuff       */
#if DDB
   #include <machine/db_machdep.h>
#endif /* DDB */

#if DDB
   #define DEBUG_MSG db_printf
#else
   #define DEBUG_MSG printf
#endif /* DDB */
static int waittime = -1;

struct intrhand *intr_handlers[256];
vm_offset_t interrupt_stack[MAX_CPUS] = {0};

/* machine dependant function pointers. */
struct funcp mdfp;

/* forwards */
void m88100_Xfp_precise(void);
void m88110_Xfp_precise(void);
void setupiackvectors(void);

unsigned char *ivec[] = {
   (unsigned char *)0xFFFE0003, /* not used, no such thing as int 0 */
   (unsigned char *)0xFFFE0007,
   (unsigned char *)0xFFFE000B,
   (unsigned char *)0xFFFE000F,
   (unsigned char *)0xFFFE0013,
   (unsigned char *)0xFFFE0017,
   (unsigned char *)0xFFFE001B,
   (unsigned char *)0xFFFE001F,
};

#ifdef MVME188
/*
 * *int_mask_reg[CPU]
 * Points to the hardware interrupt status register for each CPU.
 */
volatile unsigned int *int_mask_reg[MAX_CPUS] = {
   (volatile unsigned int *)IEN0_REG,
   (volatile unsigned int *)IEN1_REG,
   (volatile unsigned int *)IEN2_REG,
   (volatile unsigned int *)IEN3_REG
};
#endif /* MVME188 */

u_char *int_mask_level = (u_char *)INT_MASK_LEVEL;
u_char *int_pri_level = (u_char *)INT_PRI_LEVEL;
u_char *iackaddr;
volatile u_char *pcc2intr_mask;
volatile u_char *pcc2intr_ipl;
volatile vm_offset_t bugromva;
volatile vm_offset_t kernelva;
volatile vm_offset_t utilva;
volatile vm_offset_t sramva;
volatile vm_offset_t obiova;
volatile vm_offset_t extiova;

int physmem;      /* available physical memory, in pages */
int cold;         /* boot process flag */
vm_offset_t avail_end, avail_start, avail_next;
int foodebug = 0;    /* for size_memory() */
int longformat = 1;  /* for regdump() */
int BugWorks = 0;
/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

/*
 * iomap stuff is for managing chunks of virtual address space that
 * can be allocated to IO devices.
 * VMEbus drivers use this at this now. Only on-board IO devices' addresses
 * are mapped so that pa == va. XXX smurph.
 */
void *iomapbase;
struct map *iomap;
vm_map_t   iomap_map;
int      niomap;

/*
 * Declare these as initialized data so we can patch them.
 */
int   nswbuf = 0;
#ifdef	NBUF
int   nbuf = NBUF;
#else
int   nbuf = 0;
#endif
#ifdef	BUFPAGES
int   bufpages = BUFPAGES;
#else
int   bufpages = 0;
#endif
int *nofault;

caddr_t allocsys __P((caddr_t));

/*
 * Info for CTL_HW
 */
char  machine[] = "mvme88k";     /* cpu "architecture" */
char  cpu_model[120];
extern unsigned master_cpu;
extern   char version[];

struct bugenv bugargs;
struct kernel {
   void *entry;
   void *symtab;
   void *esym;
   int   bflags;
   int   bdev;
   char *kname;
   void *smini;
   void *emini;
   void *end_load;
}kflags;
char *esym;

int boothowto; /* read in locore.S */
int bootdev;   /* read in locore.S */
int cputyp;
int cpuspeed = 25;   /* 25 MHZ XXX should be read from NVRAM */

#ifndef roundup
   #define roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))
#endif /* roundup */

vm_size_t   mem_size;
vm_size_t   rawmem_size;
vm_offset_t first_addr = 0;
vm_offset_t last_addr = 0;

vm_offset_t avail_start, avail_next, avail_end;
vm_offset_t virtual_avail, virtual_end;
vm_offset_t pcc2consvaddr, clconsvaddr;
vm_offset_t miniroot;

void     *end_loaded;
int      bootdev;
int      no_symbols = 1;

struct proc *lastproc;
pcb_t    curpcb;
extern struct user *proc0paddr;

/* 
 *  XXX this is to fake out the console routines, while 
 *  booting. New and improved! :-) smurph
 */
int  bootcnprobe __P((struct consdev *));
int  bootcninit __P((struct consdev *));
void bootcnputc __P((dev_t, char));
int  bootcngetc __P((dev_t));
extern void nullcnpollc __P((dev_t, int));
#define bootcnpollc nullcnpollc
static struct consdev bootcons = {
NULL, NULL, bootcngetc, bootcnputc,
   bootcnpollc, makedev(14,0), 1};
void  cmmu_init(void);
/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit()
{
   extern struct consdev *cn_tab;
   /*
    * Initialize the console before we print anything out.
    */

   cn_tab = NULL;
   cninit();

#if defined (DDB)
   kdb_init();
   if (boothowto & RB_KDB)
      Debugger();
#endif
}

/*
 * Figure out how much real memory is available.
 * Start looking from the megabyte after the end of the kernel data,
 * until we find non-memory.
 */
vm_offset_t
size_memory(void)
{
   volatile unsigned int *look;
   unsigned int *max;
   extern char *end;
#define PATTERN   0x5a5a5a5a
#define STRIDE    (4*1024) 	/* 4k at a time */
#define Roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))
#if 1
   /*
    * count it up.
    */
   max = (void*)MAXPHYSMEM;
   for (look = (void*)Roundup(end, STRIDE); look < max;
       look = (int*)((unsigned)look + STRIDE)) {
      unsigned save;

      /* if can't access, we've reached the end */
      if (foodebug) printf("%x\n", look);
      if (badwordaddr((vm_offset_t)look)) {
#if defined(DEBUG)
         printf("%x\n", look);
#endif
         look = (int *)((int)look - STRIDE);
         break;
      }

      /*
       * If we write a value, we expect to read the same value back.
       * We'll do this twice, the 2nd time with the opposite bit
       * pattern from the first, to make sure we check all bits.
       */
      save = *look;
      if (*look = PATTERN, *look != PATTERN)
         break;
      if (*look = ~PATTERN, *look != ~PATTERN)
         break;
      *look = save;
   }
#else 
   look = (unsigned int *)0x03FFF000; /* temp hack to fake 32Meg on MVME188 */
#endif 
   physmem = btoc(trunc_page((unsigned)look)); /* in pages */
   return (trunc_page((unsigned)look));
}

int
getcpuspeed(void)
{
   struct bugbrdid brdid;
   int speed = 0;
   int i, c;
   bugbrdid(&brdid);
   for (i=0; i<4; i++) {
      c=(unsigned char)brdid.speed[i];
      c-= '0';
      speed *=10;
      speed +=c;
   }
   speed = speed / 100;
   return (speed);
}

int
getscsiid(void)
{
   struct bugbrdid brdid;
   int scsiid = 0;
   int i, c;
   bugbrdid(&brdid);
   for (i=0; i<2; i++) {
      c=(unsigned char)brdid.scsiid[i];
      scsiid *=10;
      c-= '0';
      scsiid +=c;
   }
   printf("SCSI ID = %d\n", scsiid);
   return (7); /* hack! */
}

void
identifycpu()
{
   cpuspeed = getcpuspeed();
   sprintf(cpu_model, "Motorola MVME%x %dMhz", cputyp, cpuspeed);
   printf("\nModel: %s\n", cpu_model);
}

/* The following two functions assume UPAGES == 4 */
#if	UPAGES != 4
   #error "UPAGES changed?"
#endif

#if	USPACE != (UPAGES * NBPG)
   #error "USPACE changed?"
#endif

/*
 *	Setup u area ptes for u area double mapping.
 */

void
save_u_area(struct proc *p, vm_offset_t va)
{
   int i; 
   for (i=0; i<UPAGES; i++) {
      p->p_md.md_upte[i] = kvtopte(va + (i * NBPG))->bits;
   }
}

void
load_u_area(struct proc *p)
{
   pte_template_t *t;

   int i; 
   for (i=0; i<UPAGES; i++) {
      t = kvtopte(UADDR + (i * NBPG));
      t->bits = p->p_md.md_upte[i];
   }
   for (i=0; i<UPAGES; i++) {
      cmmu_flush_tlb(1, UADDR + (i * NBPG), NBPG);
   }
}

/*
 * Set up real-time clocks.
 * These function pointers are set in dev/clock.c and dev/sclock.c
 */
void 
cpu_initclocks(void)
{
#ifdef DEBUG
   printf("cpu_initclocks(): ");
#endif 
   if (mdfp.clock_init_func != NULL){
#ifdef DEBUG
      printf("[interval clock] ");
#endif 
      (*mdfp.clock_init_func)();
   }
   if (mdfp.statclock_init_func != NULL){
#ifdef DEBUG
      printf("[statistics clock]");
#endif 
      (*mdfp.statclock_init_func)();
   }
#ifdef DEBUG
   printf("\n");
#endif 
}

void
setstatclockrate(int newhz)
{
   /* function stub */
}


void
cpu_startup()
{
   caddr_t v;
   int sz, i;
   vm_size_t size;    
   int base, residual;
   vm_offset_t minaddr, maxaddr, uarea_pages;
   extern vm_offset_t miniroot;
   /*
    * Initialize error message buffer (at end of core).
    * avail_end was pre-decremented in mvme_bootstrap().
    */

   for (i = 0; i < btoc(MSGBUFSIZE); i++)
      pmap_enter(kernel_pmap, (vm_offset_t)msgbufp,
	avail_end + i * NBPG, VM_PROT_READ|VM_PROT_WRITE,
	VM_PROT_READ|VM_PROT_WRITE, TRUE);
   initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

   printf("real mem  = %d\n", ctob(physmem));

   /*
    * Find out how much space we need, allocate it,
    * and then give everything true virtual addresses.
    */
   sz = (int)allocsys((caddr_t)0);
   if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(sz))) == 0)
      panic("startup: no room for tables");
   if (allocsys(v) - v != sz)
      panic("startup: table size inconsistency");

   /*
    * Grab UADDR virtual address
    */

   uarea_pages = UADDR;

   vm_map_find(kernel_map, vm_object_allocate(USPACE), 0,
               (vm_offset_t *)&uarea_pages, USPACE, TRUE);

   if (uarea_pages != UADDR) {
      printf("uarea_pages %x: UADDR not free\n", uarea_pages);
      panic("bad UADDR");
   }

   if (cputyp != CPU_188) {   /* != CPU_188 */
      
      /*
       * Grab the BUGROM space that we hardwired in pmap_bootstrap
       */
      bugromva = BUGROM_START;

      vm_map_find(kernel_map, vm_object_allocate(BUGROM_SIZE), 0,
                  (vm_offset_t *)&bugromva, BUGROM_SIZE, TRUE);

      if (bugromva != BUGROM_START) {
         printf("bugromva %x: BUGROM not free\n", bugromva);
         panic("bad bugromva");
      }

      /*
       * Grab the SRAM space that we hardwired in pmap_bootstrap
       */
      sramva = SRAM_START;

      vm_map_find(kernel_map, vm_object_allocate(SRAM_SIZE), 0,
                  (vm_offset_t *)&sramva, SRAM_SIZE, TRUE);

      if (sramva != SRAM_START) {
         printf("sramva %x: SRAM not free\n", sramva);
         panic("bad sramva");
      }

      /*
       * Grab the OBIO space that we hardwired in pmap_bootstrap
       */
      obiova = OBIO_START;

      vm_map_find(kernel_map, vm_object_allocate(OBIO_SIZE), 0,
                  (vm_offset_t *)&obiova, OBIO_SIZE, TRUE);

      if (obiova != OBIO_START) {
         printf("obiova %x: OBIO not free\n", obiova);
         panic("bad OBIO");
      }
   } else { /* cputyp == CPU_188 */
      /*
       * Grab the UTIL space that we hardwired in pmap_bootstrap
       */
      utilva = MVME188_UTILITY;

      vm_map_find(kernel_map, vm_object_allocate(MVME188_UTILITY_SIZE), 0,
                  (vm_offset_t *)&utilva, MVME188_UTILITY_SIZE, TRUE);

      if (utilva != MVME188_UTILITY) {
         printf("utilva %x: UTILITY area not free\n", utilva);
         panic("bad utilva");
      }
   }

   /*
    * Now allocate buffers proper.  They are different than the above
    * in that they usually occupy more virtual memory than physical.
    */

   size = MAXBSIZE * nbuf;
   buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
                              &maxaddr, size, TRUE);
   minaddr = (vm_offset_t)buffers;
   if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
                   (vm_offset_t *)&minaddr, size, FALSE) != KERN_SUCCESS) {
      panic("startup: cannot allocate buffers");
   }
   if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
      /* don't want to alloc more physical mem than needed */
      bufpages = btoc(MAXBSIZE) * nbuf;
   }
   base = bufpages / nbuf;
   residual = bufpages % nbuf;

   for (i = 0; i < nbuf; i++) {
      vm_size_t curbufsize;
      vm_offset_t curbuf;

      /*
       * First <residual> buffers get (base+1) physical pages
       * allocated for them.  The rest get (base) physical pages.
       *
       * The rest of each buffer occupies virtual space,
       * but has no physical memory allocated for it.
       */
      curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
      curbufsize = CLBYTES * (i < residual ? base+1 : base);

      /* this faults in the required physical pages */
      vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);

      vm_map_simplify(buffer_map, curbuf);
   }

   /*
    * Allocate a submap for exec arguments.  This map effectively
    * limits the number of processes exec'ing at any time.
    */
   exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
                            16*NCARGS, TRUE);
#ifdef DEBUG
   printf("exe_map from 0x%x to 0x%x\n", (unsigned)minaddr, (unsigned)maxaddr);
#endif 
   /*
    * Allocate map for physio.
    */

   phys_map = vm_map_create(kernel_pmap, PHYSIO_MAP_START,
                            PHYSIO_MAP_START + PHYSIO_MAP_SIZE, TRUE);
   if (phys_map == NULL) {
      panic("cpu_startup: unable to create phys_map");
   }

   /* 
    * Allocate map for external I/O XXX new code - smurph 
    */
   /*   
    * IOMAP_MAP_START was used for the base address of this map, but
    * IOMAP_MAP_START == 0xEF000000, which is larger than a signed 
    * long (int on 88k). This causes rminit() to break when DIAGNOSTIC is 
    * defined, as it checks (long)addr < 0.  So as a workaround, I use 
    * 0x10000000 as a base address. XXX smurph
    */
   
   iomap_map = vm_map_create(kernel_pmap, (u_long)0x10000000,
                             (u_long)0x10000000 + IOMAP_SIZE, TRUE);
   if (iomap_map == NULL) {
      panic("cpu_startup: unable to create iomap_map");
   }
	iomapbase = (void *)kmem_alloc_wait(iomap_map, IOMAP_SIZE);
   rminit(iomap, IOMAP_SIZE, (u_long)iomapbase, "iomap", NIOPMAP);

   /*
    * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
    * we use the more space efficient malloc in place of kmem_alloc.
    */
   mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
                              M_MBUF, M_NOWAIT);
   bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
   mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
                          VM_MBUF_SIZE, FALSE);

   /*
    * Initialize callouts
    */
   callfree = callout;
   for (i = 1; i < ncallout; i++)
      callout[i-1].c_next = &callout[i];
   callout[i-1].c_next = NULL;

   printf("avail mem = %d\n", ptoa(cnt.v_free_count));
   printf("using %d buffers containing %d bytes of memory\n",
          nbuf, bufpages * CLBYTES);

#if 0 /* #ifdef MFS */
   /*
    * Check to see if a mini-root was loaded into memory. It resides
    * at the start of the next page just after the end of BSS.
    */
   {
      extern void *smini;

      if (miniroot && (boothowto & RB_MINIROOT)) {
         boothowto |= RB_DFLTROOT;
         mfs_initminiroot(miniroot);
      }
   }
#endif

   /*
    * Set up buffers, so they can be used to read disk labels.
    */
   bufinit();

   /*
    * Configure the system.
    */
   nofault = NULL;

   /*
    * zero out intr_handlers
    */
   bzero((void *)intr_handlers, 256 * sizeof(struct intrhand *));
   setupiackvectors();
   configure();
}

/*
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * We call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */
caddr_t
allocsys(v)
register caddr_t v;
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))

#ifdef REAL_CLISTS
   valloc(cfree, struct cblock, nclist);
#endif
   valloc(callout, struct callout, ncallout);
#if 0
   valloc(swapmap, struct map, nswapmap = maxproc * 2);
#endif 
#ifdef SYSVSHM
   valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
   valloc(sema, struct semid_ds, seminfo.semmni);
   valloc(sem, struct sem, seminfo.semmns);
   /* This is pretty disgusting! */
   valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
   valloc(msgpool, char, msginfo.msgmax);
   valloc(msgmaps, struct msgmap, msginfo.msgseg);
   valloc(msghdrs, struct msg, msginfo.msgtql);
   valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

   /*
    * Determine how many buffers to allocate (enough to
    * hold 5% of total physical memory, but at least 16).
    * Allocate 1/2 as many swap buffer headers as file i/o buffers.
    */
   if (bufpages == 0)
      if (physmem < btoc(2 * 1024 * 1024))
         bufpages = (physmem / 10) / CLSIZE;
      else
         bufpages = (physmem / 20) / CLSIZE;
   if (nbuf == 0) {
      nbuf = bufpages;
      if (nbuf < 16)
         nbuf = 16;
   }
   if (nswbuf == 0) {
      nswbuf = (nbuf / 2) &~ 1;  /* force even */
      if (nswbuf > 256)
         nswbuf = 256;     /* sanity */
   }
   valloc(swbuf, struct buf, nswbuf);
   valloc(buf, struct buf, nbuf);

#if 1 /*XXX_FUTURE*/
   /*
    * Arbitrarily limit the number of devices mapping
    * the IO space at a given time to NIOPMAP (= 32, default).
    */
   valloc(iomap, struct map, niomap = NIOPMAP);
#endif
   return v;
}

/*
 * Set registers on exec.
 * Clear all except sp and pc.
 */

/* MVME197 TODO list :-) smurph */

void
setregs(p, pack, stack, retval)
struct proc *p;
struct exec_package *pack;
u_long stack;
int retval[2];
{
   register struct trapframe *tf = USER_REGS(p);

/*	printf("stack at %x\n", stack);
   printf("%x - %x\n", USRSTACK - MAXSSIZ, USRSTACK);
*/
   /*
    * The syscall will ``return'' to snip; set it.
    * argc, argv, envp are placed on the stack by copyregs.
    * Point r2 to the stack. crt0 should extract envp from
    * argc & argv before calling user's main.
    */
#if 0
   /*
    * I don't think I need to mess with fpstate on 88k because
    * we make sure the floating point pipeline is drained in
    * the trap handlers. Should check on this later. XXX Nivas.
    */

   if ((fs = p->p_md.md_fpstate) != NULL) {
      /*
       * We hold an FPU state.  If we own *the* FPU chip state
       * we must get rid of it, and the only way to do that is
       * to save it.  In any case, get rid of our FPU state.
       */
      if (p == fpproc) {
         savefpstate(fs);
         fpproc = NULL;
      }
      free((void *)fs, M_SUBPROC);
      p->p_md.md_fpstate = NULL;
   }
#endif /* 0 */
   bzero((caddr_t)tf, sizeof *tf);
   tf->epsr = 0x3f0;  /* user mode, interrupts enabled, fp enabled */
/*	tf->epsr = 0x3f4;*/  /* user mode, interrupts enabled, fp enabled, MXM Mask */

   /*
    * We want to start executing at pack->ep_entry. The way to
    * do this is force the processor to fetch from ep_entry. Set
    * NIP to something bogus and invalid so that it will be a NOOP.
    * And set sfip to ep_entry with valid bit on so that it will be
    * fetched.
    */

   tf->snip = pack->ep_entry & ~3;
   tf->sfip = (pack->ep_entry & ~3) | FIP_V;
   tf->r[2] = stack;
   tf->r[31] = stack;
   retval[1] = 0;
}

struct sigstate {
   int   ss_flags;      /* which of the following are valid */
   struct   trapframe ss_frame;  /* original exception frame */
};

/*
 * WARNING: code in locore.s assumes the layout shown for sf_signo
 * thru sf_handler so... don't screw with them!
 */
struct sigframe {
   int   sf_signo;      /* signo for handler */
   siginfo_t *sf_sip;
   struct   sigcontext *sf_scp;  /* context ptr for handler */
   sig_t sf_handler;    /* handler addr for u_sigc */
   struct   sigcontext sf_sc; /* actual context */
   siginfo_t sf_si;
};

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
   #define SDB_FOLLOW	0x01
   #define SDB_KSTACK	0x02
   #define SDB_FPSTATE	0x04
#endif

/*
 * Send an interrupt to process.
 */
/* MVME197 TODO list :-) smurph */
void
sendsig(catcher, sig, mask, code, type, val)
sig_t catcher;
int sig, mask;
unsigned long code;
int type;
union sigval val;
{
   register struct proc *p = curproc;
   register struct trapframe *tf;
   register struct sigacts *psp = p->p_sigacts;
   struct sigframe *fp;
   int oonstack, fsize;
   struct sigframe sf;
   int addr;
   extern char sigcode[], esigcode[];

#define szsigcode (esigcode - sigcode)

   tf = p->p_md.md_tf;
   oonstack = psp->ps_sigstk.ss_flags & SA_ONSTACK;
   /*
    * Allocate and validate space for the signal handler
    * context. Note that if the stack is in data space, the
    * call to grow() is a nop, and the copyout()
    * will fail if the process has not already allocated
    * the space with a `brk'.
    */
   fsize = sizeof(struct sigframe);
   if ((psp->ps_flags & SAS_ALTSTACK) &&
       (psp->ps_sigstk.ss_flags & SA_ONSTACK) == 0 &&
       (psp->ps_sigonstack & sigmask(sig))) {
      fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
                               psp->ps_sigstk.ss_size - fsize);
      psp->ps_sigstk.ss_flags |= SA_ONSTACK;
   } else
      fp = (struct sigframe *)(tf->r[31] - fsize);
   if ((unsigned)fp <= USRSTACK - ctob(p->p_vmspace->vm_ssize))
      (void)grow(p, (unsigned)fp);
#ifdef DEBUG
   if ((sigdebug & SDB_FOLLOW) ||
       (sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
      printf("sendsig(%d): sig %d ssp %x usp %x scp %x\n",
             p->p_pid, sig, &oonstack, fp, &fp->sf_sc);
#endif
   /*
    * Build the signal context to be used by sigreturn.
    */
   sf.sf_signo = sig;
   sf.sf_scp = &fp->sf_sc;
   sf.sf_handler = catcher;
   sf.sf_sc.sc_onstack = oonstack;
   sf.sf_sc.sc_mask = mask;

   if (psp->ps_siginfo & sigmask(sig)) {
      sf.sf_sip = &fp->sf_si;
      initsiginfo(&sf.sf_si, sig, code, type, val);
   }


   /*
    * Copy the whole user context into signal context that we
    * are building.
    */
   bcopy((caddr_t)tf->r, (caddr_t)sf.sf_sc.sc_regs,
         sizeof(sf.sf_sc.sc_regs));
   sf.sf_sc.sc_xip = tf->sxip & ~3;
   sf.sf_sc.sc_nip = tf->snip & ~3;
   sf.sf_sc.sc_fip = tf->sfip & ~3;
   sf.sf_sc.sc_ps = tf->epsr;
   sf.sf_sc.sc_sp  = tf->r[31];
   sf.sf_sc.sc_fpsr = tf->fpsr;
   sf.sf_sc.sc_fpcr = tf->fpcr;
   sf.sf_sc.sc_ssbr = tf->ssbr;
   sf.sf_sc.sc_dmt0 = tf->dmt0;
   sf.sf_sc.sc_dmd0 = tf->dmd0;
   sf.sf_sc.sc_dma0 = tf->dma0;
   sf.sf_sc.sc_dmt1 = tf->dmt1;
   sf.sf_sc.sc_dmd1 = tf->dmd1;
   sf.sf_sc.sc_dma1 = tf->dma1;
   sf.sf_sc.sc_dmt2 = tf->dmt2;
   sf.sf_sc.sc_dmd2 = tf->dmd2;
   sf.sf_sc.sc_dma2 = tf->dma2;
   sf.sf_sc.sc_fpecr = tf->fpecr;
   sf.sf_sc.sc_fphs1 = tf->fphs1;
   sf.sf_sc.sc_fpls1 = tf->fpls1;
   sf.sf_sc.sc_fphs2 = tf->fphs2;
   sf.sf_sc.sc_fpls2 = tf->fpls2;
   sf.sf_sc.sc_fppt = tf->fppt;
   sf.sf_sc.sc_fprh = tf->fprh;
   sf.sf_sc.sc_fprl = tf->fprl;
   sf.sf_sc.sc_fpit = tf->fpit;
   if (copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf)) {
      /*
       * Process has trashed its stack; give it an illegal
       * instruction to halt it in its tracks.
       */
      SIGACTION(p, SIGILL) = SIG_DFL;
      sig = sigmask(SIGILL);
      p->p_sigignore &= ~sig;
      p->p_sigcatch &= ~sig;
      p->p_sigmask &= ~sig;
      psignal(p, SIGILL);
      return;
   }
   /* 
    * Build the argument list for the signal handler.
    * Signal trampoline code is at base of user stack.
    */
   addr = (int)PS_STRINGS - szsigcode;
   tf->snip = (addr & ~3) | NIP_V;
   tf->sfip = (tf->snip + 4) | FIP_V;
   tf->r[31] = (unsigned)fp;
#ifdef DEBUG
   if ((sigdebug & SDB_FOLLOW) ||
       (sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
      printf("sendsig(%d): sig %d returns\n", p->p_pid, sig);
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper priviledges or to cause
 * a machine fault.
 */
/* ARGSUSED */

/* MVME197 TODO list :-) smurph */

sys_sigreturn(p, v, retval)
struct proc *p;
void *v;
register_t *retval;
{
   struct sys_sigreturn_args /* {
      syscallarg(struct sigcontext *) sigcntxp;
   } */ *uap = v;
   register struct sigcontext *scp;
   register struct trapframe *tf;
   struct sigcontext ksc;
   int error;

   scp = (struct sigcontext *)SCARG(uap, sigcntxp);
#ifdef DEBUG
   if (sigdebug & SDB_FOLLOW)
      printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
   if ((int)scp & 3 || useracc((caddr_t)scp, sizeof *scp, B_WRITE) == 0 ||
       copyin((caddr_t)scp, (caddr_t)&ksc, sizeof(struct sigcontext)))
      return (EINVAL);

   tf = p->p_md.md_tf;
   scp = &ksc;
   /*
    * xip, nip and fip must be multiples of 4.  This is all
    * that is required; if it holds, just do it.
    */
#if 0
   if (((scp->sc_xip | scp->sc_nip | scp->sc_fip) & 3) != 0)
      return (EINVAL);
#endif /* 0 */
   if (((scp->sc_xip | scp->sc_nip | scp->sc_fip) & 3) != 0)
      printf("xip %x nip %x fip %x\n",
             scp->sc_xip, scp->sc_nip, scp->sc_fip);

   /*
    * this can be improved by doing
    *	 bcopy(sc_reg to tf, sizeof sigcontext - 2 words)
    * XXX nivas
    */

   bcopy((caddr_t)scp->sc_regs, (caddr_t)tf->r, sizeof(scp->sc_regs));
   tf->sxip = (scp->sc_xip) | XIP_V;
   tf->snip = (scp->sc_nip) | NIP_V;
   tf->sfip = (scp->sc_fip) | FIP_V;
   tf->epsr = scp->sc_ps;
   tf->r[31] = scp->sc_sp;
   tf->fpsr = scp->sc_fpsr;
   tf->fpcr = scp->sc_fpcr;
   tf->ssbr = scp->sc_ssbr;
   tf->dmt0 = scp->sc_dmt0;
   tf->dmd0 = scp->sc_dmd0;
   tf->dma0 = scp->sc_dma0;
   tf->dmt1 = scp->sc_dmt1;
   tf->dmd1 = scp->sc_dmd1;
   tf->dma1 = scp->sc_dma1;
   tf->dmt2 = scp->sc_dmt2;
   tf->dmd2 = scp->sc_dmd2;
   tf->dma2 = scp->sc_dma2;
   tf->fpecr = scp->sc_fpecr;
   tf->fphs1 = scp->sc_fphs1;
   tf->fpls1 = scp->sc_fpls1;
   tf->fphs2 = scp->sc_fphs2;
   tf->fpls2 = scp->sc_fpls2;
   tf->fppt = scp->sc_fppt;
   tf->fprh = scp->sc_fprh;
   tf->fprl = scp->sc_fprl;
   tf->fpit = scp->sc_fpit;

   tf->epsr = scp->sc_ps;
   /*
    * Restore the user supplied information
    */
   if (scp->sc_onstack & 01)
      p->p_sigacts->ps_sigstk.ss_flags |= SA_ONSTACK;
   else
      p->p_sigacts->ps_sigstk.ss_flags &= ~SA_ONSTACK;
   p->p_sigmask = scp->sc_mask & ~sigcantmask;
   return (EJUSTRETURN);
}

_doboot()
{
   cmmu_shutdown_now();
   bugreturn();
}

void
boot(howto)
register int howto;
{
   /* take a snap shot before clobbering any registers */
   if (curproc)
      savectx(curproc->p_addr, 0);

   boothowto = howto;
   if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
      extern struct proc proc0;

      /* protect against curproc->p_stats.foo refs in sync()   XXX */
      if (curproc == NULL)
         curproc = &proc0;

      waittime = 0;
      vfs_shutdown();

      /*
       * If we've been adjusting the clock, the todr
       * will be out of synch; adjust it now.
       */
      resettodr();
   }
   splhigh();        /* extreme priority */
   if (howto & RB_HALT) {
      printf("halted\n\n");
      bugreturn();
   } else {
      if (howto & RB_DUMP)
         dumpsys();
      doboot();
      /*NOTREACHED*/
   }
   /*NOTREACHED*/
   while (1);  /* to keep compiler happy, and me from going crazy */
}

#ifdef MVME188
void 
m188_reset(void)
{
   volatile int cnt;
   
   *sys_syscon->ien0 = 0;
   *sys_syscon->ien1 = 0;
   *sys_syscon->ien2 = 0;
   *sys_syscon->ien3 = 0;
   *sys_syscon->glbres = 1;  /* system reset */
   *sys_syscon->ucsr |= 0x2000; /* clear SYSFAIL* */
   for (cnt = 0; cnt < 5*1024*1024; cnt++)
      ;
   *sys_syscon->ucsr |= 0x2000; /* clear SYSFAIL* */
}
#endif   /* MVME188 */

unsigned dumpmag = 0x8fca0101;   /* magic number for savecore */
int   dumpsize = 0;     /* also for savecore */
long  dumplo = 0;

dumpconf()
{
   int nblks;

   dumpsize = physmem;
   if (dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize) {
      nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
      if (dumpsize > btoc(dbtob(nblks - dumplo)))
         dumpsize = btoc(dbtob(nblks - dumplo));
      else if (dumplo == 0)
         dumplo = nblks - btodb(ctob(physmem));
   }
   /*
    * Don't dump on the first CLBYTES (why CLBYTES?)
    * in case the dump device includes a disk label.
    */
   if (dumplo < btodb(CLBYTES))
      dumplo = btodb(CLBYTES);
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
dumpsys()
{
   extern int msgbufmapped;

   msgbufmapped = 0;
   if (dumpdev == NODEV)
      return;
   /*
    * For dumps during autoconfiguration,
    * if dump device has already configured...
    */
   if (dumpsize == 0)
      dumpconf();
   if (dumplo < 0)
      return;
   printf("\ndumping to dev %x, offset %d\n", dumpdev, dumplo);
   printf("dump ");
   switch ((*bdevsw[major(dumpdev)].d_dump)(dumpdev)) {
      
      case ENXIO:
         printf("device bad\n");
         break;

      case EFAULT:
         printf("device not ready\n");
         break;

      case EINVAL:
         printf("area improper\n");
         break;

      case EIO:
         printf("i/o error\n");
         break;

      default:
         printf("succeeded\n");
         break;
   }
}

/*
 * fill up ivec array with interrupt response vector addresses.
 */
void
setupiackvectors()
{
   register u_char *vaddr;
#undef MAP_VEC /* Swicthing to new virtual addresses XXX smurph */
#ifdef MAP_VEC
   extern vm_offset_t iomap_mapin(vm_offset_t, vm_size_t,  boolean_t);
#endif

   /*
    * map a page in for phys address 0xfffe0000 and set the
    * addresses for various levels.
    */
   switch (cputyp) {
      case CPU_187:
#ifdef MAP_VEC /* do for MVME188 too */
         vaddr = (u_char *)iomap_mapin(M187_IACK, NBPG, 1);
#else
         vaddr = (u_char *)M187_IACK;
#endif
         break;
      case CPU_188:
#ifdef MAP_VEC /* do for MVME188 too */
         vaddr = (u_char *)iomap_mapin(M188_IACK, NBPG, 1);
#else
         vaddr = (u_char *)M188_IACK;
#endif
         break;
      case CPU_197:
#ifdef MAP_VEC /* do for MVME188 too */
         vaddr = (u_char *)iomap_mapin(M197_IACK, NBPG, 1);
#else
         vaddr = (u_char *)M197_IACK;
#endif
         break;
   }
#ifdef DEBUG
   printf("interrupt ACK address mapped at 0x%x\n", vaddr);
#endif 
   ivec[0] = vaddr + 0x03;
   ivec[1] = vaddr + 0x07;
   ivec[2] = vaddr + 0x0b;
   ivec[3] = vaddr + 0x0f;
   ivec[4] = vaddr + 0x13;
   ivec[5] = vaddr + 0x17;
   ivec[6] = vaddr + 0x1b;
   ivec[7] = vaddr + 0x1f;
}

/* gets an interrupt stack for slave processors */
vm_offset_t 
get_slave_stack(void)
{
   vm_offset_t addr = 0;

   addr = (vm_offset_t)kmem_alloc(kernel_map, INTSTACK_SIZE + 4096);

   if (addr == NULL)
      panic("Cannot allocate slave stack");

   if (interrupt_stack[0] == 0)
      interrupt_stack[0] = (vm_offset_t) intstack;
   interrupt_stack[cpu_number()] = addr;
   return addr;
}

/* dummy main routine for slave processors */
int
slave_main(void)
{
   printf("slave CPU%d started\n", cpu_number());
   while (-1); /* spin forever */
   return 0;
}

/*
 * find a useable interrupt vector in the range start, end. It starts at
 * the end of the range, and searches backwards (to increase the chances
 * of not conflicting with more normal users)  
 * 
 * XXX This is not used yet.  It will provide a facility to 'autovector'
 * VME boards. smurph
 */
int
intr_findvec(start, end)
int start, end;
{
   register struct intrhand *intr;
   int vec;

   if (start < 0 || end > 255 || start > end)
      return (-1);
   for (vec = end; vec > start; --vec)
      if (intr_handlers[vec] == (struct intrhand *)0) 
         return (vec);
   return (-1);
}

/*
 * Insert ihand in the list of handlers at vector vec.
 * Return return different error codes for the different
 * errors and let the caller decide what to do.
 */
int
intr_establish(int vec, struct intrhand *ihand)
{
   register struct intrhand *intr;

   if (vec < 0 || vec > 255) {
   #if DIAGNOSTIC
      panic("intr_establish: vec (%x) not between 0 and 0xff",
            vec);
   #endif /* DIAGNOSTIC */
      return (INTR_EST_BADVEC);
   }

   if (intr = intr_handlers[vec]) {
      if (intr->ih_ipl != ihand->ih_ipl) {
   #if DIAGNOSTIC
         panic("intr_establish: there are other handlers with vec (%x) at ipl %x, but you want it at %x",
               intr->ih_ipl, vec, ihand->ih_ipl);
   #endif /* DIAGNOSTIC */
         return (INTR_EST_BADIPL);
      }

      /*
       * Go to the end of the chain
       */
      while (intr->ih_next)
         intr = intr->ih_next;
   }

   ihand->ih_next = 0;

   if (intr)
      intr->ih_next = ihand;
   else
      intr_handlers[vec] = ihand;

   return (INTR_EST_SUCC);
}

#ifdef MVME188

/*
 *	Device interrupt handler for MVME188
 *
 *      when we enter, interrupts are disabled;
 *      when we leave, they should be disabled,
 *      but they need not be disabled throughout
 *      the routine.
 */

/* Hard coded vector table for onboard devices. */
unsigned obio_vec[32] = {SYSCV_ABRT,SYSCV_ACF,0,SYSCV_TIMER1,0,0,0,0,
                    0,0,SYSCV_TIMER2,SYSCV_SYSF,0,0,SYSCV_SCC,0,
                    0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0, 
};
#define GET_MASK(cpu, val)	*int_mask_reg[cpu] & (val)

void 
m188_ext_int(u_int v, struct m88100_saved_state *eframe)
{
   register int cpu = 0; /*cpu_number();*/
   register unsigned int cur_mask;
   register unsigned int level, old_spl;
   register struct intrhand *intr;
   int ret, intnum;
   unsigned vec;

   cur_mask = ISR_GET_CURRENT_MASK(cpu);
   old_spl = m188_curspl[cpu];
   eframe->mask = old_spl;

   if (! cur_mask) {
      /*
       * Spurious interrupts - may be caused by debug output clearing
       * DUART interrupts.
       */
      flush_pipeline();
      return;
   }

   /* We want to service all interrupts marked in the IST register  */
   /* They are all valid because the mask would have prevented them */
   /* from being generated otherwise.  We will service them in order of       */
   /* priority. */
   do {
      /*
      printf("interrupt: mask = 0x%08x spl = %d imr = 0x%x\n", ISR_GET_CURRENT_MASK(cpu),
                        old_spl, *int_mask_reg[cpu]);
      */
      level = safe_level(cur_mask, old_spl);

      if (old_spl >= level) {
         register int i;

         printf("safe level %d <= old level %d\n", level, old_spl);
         printf("cur_mask = 0x%b\n", cur_mask, IST_STRING);
         for (i = 0; i < 4; i++)
            printf("IEN%d = 0x%b  ", i, *int_mask_reg[i], IST_STRING);
         printf("\nCPU0 spl %d  CPU1 spl %d  CPU2 spl %d  CPU3 spl %d\n",
                m188_curspl[0], m188_curspl[1],
                m188_curspl[2], m188_curspl[3]);
         for (i = 0; i < 8; i++)
            printf("int_mask[%d] = 0x%08x\n", i, int_mask_val[i]);
         printf("--CPU %d halted--", cpu_number());
         spl7();
         while (1)
            ;
      }

      setipl((u_char)level);

      if (level > 7 || (char)level < 0) {
         panic("int level (%x) is not between 0 and 7", level);
      }
      
      /* generate IACK and get the vector */
      
      /* 
       * This is tricky.  If you don't catch all the 
       * interrupts, you die. Game over. Insert coin... 
       * XXX smurph
       */
      
      intnum = ff1(cur_mask);
      if (intnum & OBIO_INTERRUPT_MASK) {
         vec = obio_vec[intnum];
         if (vec = 0) {
            printf("unknown onboard interrupt: mask = 0x%b\n", 1 << intnum, IST_STRING);
            panic("m188_ext_int");
         }
      } else if (intnum & HW_FAILURE_MASK) {
         vec = obio_vec[intnum];
         if (vec = 0) {
            printf("unknown hadware failure: mask = 0x%b\n", 1 << intnum, IST_STRING);
            panic("m188_ext_int");
         }
      } else if (intnum & VME_INTERRUPT_MASK) {
         asm volatile("tb1	0, r0, 0"); 
         if (guarded_access(ivec[level], 1, &vec) == EFAULT) {
            printf("Unable to get vector for this vmebus interrupt (level %x)\n", level);
            goto out_m188;
         }
      } else {
         printf("unknown interrupt: mask = 0x%b\n", 1 << intnum, IST_STRING);
         panic("m188_ext_int");
      }
#if 0
      if (cur_mask & ABRT_BIT) { /* abort button interrupt */
         vec = 110;
      } else if (cur_mask & DTI_BIT) { /* interval timer interrupt */
         vec = SYSCV_TIMER1; 
      } else if (cur_mask & CIOI_BIT) { /* statistics timer interrupt */
         vec = SYSCV_TIMER2; 
      } else if (cur_mask & DI_BIT) { /* duart interrupt */
         vec = SYSCV_SCC; 
      } else { /* vmebus interrupt */
         asm volatile("tb1	0, r0, 0"); 
         if (guarded_access(ivec[level], 1, &vec) == EFAULT) {
            printf("Unable to get vector for this vmebus interrupt (level %x)\n", level);
            goto out_m188;
         }
      }
#endif
      asm volatile("tb1	0, r0, 0"); 
      asm volatile("tb1	0, r0, 0"); 
      asm volatile("tb1	0, r0, 0"); 
      if (vec > 0xFF) {
         panic("interrupt vector %x greater than 255", vec);
      }
#if 0
      enable_interrupt(); /* should we ?? */
#endif 

      if ((intr = intr_handlers[vec]) == 0)
         printf("Spurious interrupt (level %x and vec %x)\n", level, vec);
      
      /*
       * Walk through all interrupt handlers in the chain for the
       * given vector, calling each handler in turn, till some handler
       * returns a value != 0.
       */
      for (ret = 0; intr; intr = intr->ih_next) {
         if (intr->ih_wantframe)
            ret = (*intr->ih_fn)(intr->ih_arg, (void *)eframe);
         else
            ret = (*intr->ih_fn)(intr->ih_arg);
         if (ret)
            break;
      }
      if (ret == 0) 
         printf("Unclaimed interrupt (level %x and vec %x)\n", level, vec);
   } while (cur_mask = ISR_GET_CURRENT_MASK(cpu));


   /*
    * process any remaining data access exceptions before
    * returning to assembler
    */
   disable_interrupt();
out_m188:
   if (eframe->dmt0 & DMT_VALID) {
      trap(T_DATAFLT, eframe);
      data_access_emulation(eframe);
      eframe->dmt0 &= ~DMT_VALID;
   }

   /*
    * Restore the mask level to what it was when the interrupt
    * was taken.
    */
   setipl((u_char)eframe->mask);
   flush_pipeline();
   return;
}

#endif /* MVME188 */

/*
 *	Device interrupt handler for MVME1x7
 *
 *      when we enter, interrupts are disabled;
 *      when we leave, they should be disabled,
 *      but they need not be disabled throughout
 *      the routine.
 */

#if defined(MVME187) || defined(MVME197)
void
sbc_ext_int(u_int v, struct m88100_saved_state *eframe)
{
   register u_char mask, level, xxxvec;
   register struct intrhand *intr;
   int ret;
   u_char vec;

   /* get level and mask */
   asm volatile("ld.b	%0,%1" : "=r" (mask) : "" (*pcc2intr_mask));
   asm volatile("ld.b	%0,%1" : "=r" (level) : "" (*pcc2intr_ipl));

   /*
    * It is really bizarre for the mask and level to the be the same.
    * pcc2 for 187 blocks all interrupts at and below the mask value,
    * so we should not be getting an interrupt at the level that is
    * already blocked. I can't explain this case XXX nivas
    */

   if ((mask == level) && level) {
      printf("mask == level, %d\n", level);
      goto beatit;
   }

   /*
    * Interrupting level cannot be 0--0 doesn't produce an interrupt.
    * Weird! XXX nivas
    */

   if (level == 0) {
      printf("Bogons... level %x and mask %x\n", level, mask);
      goto beatit;
   }

   /* and block interrupts at level or lower */
   setipl((u_char)level);
   /* and stash it away in the trap frame */
   eframe->mask = mask;

   if (level > 7 || (char)level < 0) {
      panic("int level (%x) is not between 0 and 7", level);
   }

   /* generate IACK and get the vector */
   asm volatile("tb1	0, r0, 0"); 
   if (guarded_access(ivec[level], 1, &vec) == EFAULT) {
      printf("Unable to get vector for this interrupt (level %x)\n", level);
      goto out;
   }
   asm volatile("tb1	0, r0, 0"); 
   asm volatile("tb1	0, r0, 0"); 
   asm volatile("tb1	0, r0, 0"); 
   /*vec = xxxvec;*/

   if (vec > 0xFF) {
      panic("interrupt vector %x greater than 255", vec);
   }

   enable_interrupt();

   if ((intr = intr_handlers[vec]) == 0) {
      printf("Spurious interrupt (level %x and vec %x)\n",
             level, vec);
   }
   if (intr && intr->ih_ipl != level) {
      panic("Handler ipl %x not the same as level %x. vec = 0x%x",
            intr->ih_ipl, level, vec);
   }

   /*
    * Walk through all interrupt handlers in the chain for the
    * given vector, calling each handler in turn, till some handler
    * returns a value != 0.
    */

   for (ret = 0; intr; intr = intr->ih_next) {
      if (intr->ih_wantframe)
         ret = (*intr->ih_fn)(intr->ih_arg, (void *)eframe);
      else
         ret = (*intr->ih_fn)(intr->ih_arg);
      if (ret)
         break;
   }

   if (ret == 0) {
      printf("Unclaimed interrupt (level %x and vec %x)\n",
             level, vec);
   }

   /*
    * process any remaining data access exceptions before
    * returning to assembler
    */
   disable_interrupt();

out:
   if (cputyp != CPU_197) {
      if (eframe->dmt0 & DMT_VALID) {
         trap(T_DATAFLT, eframe);
         data_access_emulation(eframe);
         eframe->dmt0 &= ~DMT_VALID;
      }
   }
   mask = eframe->mask;

   /*
    * Restore the mask level to what it was when the interrupt
    * was taken.
    */
   setipl((u_char)mask);

beatit:
   return;
}
#endif /* defined(MVME187) || defined(MVME197) */

cpu_exec_aout_makecmds(p, epp)
struct proc *p;
struct exec_package *epp;
{
   return ENOEXEC;
}

sys_sysarch(p, v, retval)
struct proc *p;
void *v;
register_t *retval;
{
   struct sys_sysarch_args /* {
      syscallarg(int) op;
      syscallarg(char *) parm;
   } */ *uap = v;
   int error = 0;

   switch ((int)SCARG(uap, op)) {
      default:
         error = EINVAL;
         break;
   }
   return (error);
}

/*
 * machine dependent system variables.
 */

cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
int *name;
u_int namelen;
void *oldp;
size_t *oldlenp;
void *newp;
size_t newlen;
struct proc *p;
{

   /* all sysctl names are this level are terminal */
   if (namelen != 1)
      return (ENOTDIR); /* overloaded */

   switch (name[0]) {
      default:
         return (EOPNOTSUPP);
   }
   /*NOTREACHED*/
}

/*
 * insert an element into a queue 
 */

void
_insque(velement, vhead)
void *velement, *vhead;
{
   register struct prochd *element, *head;
   element = velement;
   head = vhead;
   element->ph_link = head->ph_link;
   head->ph_link = (struct proc *)element;
   element->ph_rlink = (struct proc *)head;
   ((struct prochd *)(element->ph_link))->ph_rlink=(struct proc *)element;
}

/*
 * remove an element from a queue
 */

void
_remque(velement)
void *velement;
{
   register struct prochd *element;
   element = velement;
   ((struct prochd *)(element->ph_link))->ph_rlink = element->ph_rlink;
   ((struct prochd *)(element->ph_rlink))->ph_link = element->ph_link;
   element->ph_rlink = (struct proc *)0;
}

int
copystr(fromaddr, toaddr, maxlength, lencopied)
const void *fromaddr;
void *toaddr;
size_t maxlength;
size_t *lencopied;
{
   u_int tally;

   tally = 0;

   while (maxlength--) {
      *(u_char *)toaddr = *(u_char *)fromaddr++;
      tally++;
      if (*(u_char *)toaddr++ == 0) {
         if (lencopied) *lencopied = tally;
         return (0);
      }
   }

   if (lencopied)
      *lencopied = tally;

   return (ENAMETOOLONG);
}

void
setrunqueue(p)
register struct proc *p;
{
   register struct prochd *q;
   register struct proc *oldlast;
   register int which = p->p_priority >> 2;

   if (p->p_back != NULL)
      panic("setrunqueue %x", p);
   q = &qs[which];
   whichqs |= 1 << which;
   p->p_forw = (struct proc *)q;
   p->p_back = oldlast = q->ph_rlink;
   q->ph_rlink = p;
   oldlast->p_forw = p;
}

/*
 * Remove process p from its run queue, which should be the one
 * indicated by its priority.  Calls should be made at splstatclock().
 */
void
remrunqueue(vp)
struct proc *vp;
{
   register struct proc *p = vp;
   register int which = p->p_priority >> 2;
   register struct prochd *q;

   if ((whichqs & (1 << which)) == 0)
      panic("remrq %x", p);
   p->p_forw->p_back = p->p_back;
   p->p_back->p_forw = p->p_forw;
   p->p_back = NULL;
   q = &qs[which];
   if (q->ph_link == (struct proc *)q)
      whichqs &= ~(1 << which);
}

/* dummys for now */

bugsyscall()
{
}

void
myetheraddr(cp)
u_char *cp;
{
   struct bugniocall niocall;
   struct bugbrdid brdid;
   bugbrdid(&brdid);
   bcopy(&brdid.etheraddr, cp, 6);
}

void netintr()
{
#ifdef INET
   if (netisr & (1 << NETISR_ARP)) {
      netisr &= ~(1 << NETISR_ARP);
      arpintr();
   }
   if (netisr & (1 << NETISR_IP)) {
      netisr &= ~(1 << NETISR_IP);
      ipintr();
   }
#endif
#ifdef INET6
   if (netisr & (1 << NETISR_IPV6)) {
      netisr &= ~(1 << NETISR_IPV6);
      ip6intr();
   }
#endif
#ifdef NETATALK
   if (netisr & (1 << NETISR_ATALK)) {
      netisr &= ~(1 << NETISR_ATALK);
      atintr();
   }
#endif
#ifdef NS
   if (netisr & (1 << NETISR_NS)) {
      netisr &= ~(1 << NETISR_NS);
      nsintr();
   }
#endif
#ifdef ISO
   if (netisr & (1 << NETISR_ISO)) {
      netisr &= ~(1 << NETISR_ISO);
      clnlintr();
   }
#endif
#ifdef CCITT
   if (netisr & (1 << NETISR_CCITT)) {
      netisr &= ~(1 << NETISR_CCITT);
      ccittintr();
   }
#endif
#include "ppp.h"
#if NPPP > 0
   if (netisr & (1 << NETISR_PPP)) {
      netisr &= ~(1 << NETISR_PPP);
      pppintr();
   }
#endif
#include "bridge.h"
#if NBRIDGE > 0
   if (netisr & (1 << NETISR_BRIDGE)) {
      netisr &= ~(1 << NETISR_BRIDGE);
      bridgeintr();
   }
#endif
}

void
dosoftint()
{
   if (ssir & SIR_NET) {
      siroff(SIR_NET);
      cnt.v_soft++;
      netintr();
   }

   if (ssir & SIR_CLOCK) {
      siroff(SIR_CLOCK);
      cnt.v_soft++;
      softclock();
   }

   return;
}

int
spl0()
{
   int x;
   int level = 0;
   x = splsoftclock();

   if (ssir) {
      dosoftint();
   }

   setipl(0);

   return (x);
}

badwordaddr(void *addr)
{
   return badaddr((vm_offset_t)addr, 4);
}

MY_info(f, p, flags, s)
struct trapframe  *f;
caddr_t     p;
int         flags;
char        *s;
{
   regdump(f);
   printf("proc %x flags %x type %s\n", p, flags, s);
}  

MY_info_done(f, flags)
struct trapframe  *f;
int         flags;
{
   regdump(f);
}  

void
nmihand(void *framep)
{
   struct m88100_saved_state *frame = framep;

#if DDB
   DEBUG_MSG("Abort Pressed\n");
   Debugger();
#else
   DEBUG_MSG("Spurious NMI?\n");
#endif /* DDB */
}

regdump(struct trapframe *f)
{
#define R(i) f->r[i]
   printf("R00-05: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
          R(0),R(1),R(2),R(3),R(4),R(5));
   printf("R06-11: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
          R(6),R(7),R(8),R(9),R(10),R(11));
   printf("R12-17: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
          R(12),R(13),R(14),R(15),R(16),R(17));
   printf("R18-23: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
          R(18),R(19),R(20),R(21),R(22),R(23));
   printf("R24-29: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
          R(24),R(25),R(26),R(27),R(28),R(29));
   printf("R30-31: 0x%08x  0x%08x\n",R(30),R(31));
   if (cputyp == CPU_197) {
      printf("exip %x enip %x\n", f->sxip, f->snip);
   } else {
      printf("sxip %x snip %x sfip %x\n", f->sxip, f->snip, f->sfip);
   }
   if (f->vector == 0x3 && cputyp != CPU_197) { 
      /* print dmt stuff for data access fault */
      printf("dmt0 %x dmd0 %x dma0 %x\n", f->dmt0, f->dmd0, f->dma0);
      printf("dmt1 %x dmd1 %x dma1 %x\n", f->dmt1, f->dmd1, f->dma1);
      printf("dmt2 %x dmd2 %x dma2 %x\n", f->dmt2, f->dmd2, f->dma2);
      printf("fault type %d\n", (f->dpfsr >> 16) & 0x7);
      dae_print(f);
   }
   if (longformat && cputyp != CPU_197) {
      printf("fpsr %x ", f->fpsr);
      printf("fpcr %x ", f->fpcr);
      printf("epsr %x ", f->epsr);
      printf("ssbr %x\n", f->ssbr);
      printf("fpecr %x ", f->fpecr);
      printf("fphs1 %x ", f->fphs1);
      printf("fpls1 %x ", f->fpls1);
      printf("fphs2 %x ", f->fphs2);
      printf("fpls2 %x\n", f->fpls2);
      printf("fppt %x ", f->fppt);
      printf("fprh %x ", f->fprh);
      printf("fprl %x ", f->fprl);
      printf("fpit %x\n", f->fpit);
      printf("vector %x ", f->vector);
      printf("mask %x ", f->mask);
      printf("mode %x ", f->mode);
      printf("scratch1 %x ", f->scratch1);
      printf("pad %x\n", f->pad);
   }
   if (longformat && cputyp == CPU_197) {
      printf("fpsr %x ", f->fpsr);
      printf("fpcr %x ", f->fpcr);
      printf("fpecr %x ", f->fpecr);
      printf("epsr %x\n", f->epsr);
      printf("dsap %x ", f->dmt1);
      printf("dsr %x ", f->dsr);
      printf("dlar %x ", f->dlar);
      printf("dpar %x\n", f->dpar);
      printf("isap %x ", f->dmt0);
      printf("isr %x ", f->isr);
      printf("ilar %x ", f->ilar);
      printf("ipar %x\n", f->ipar);
      printf("vector %x ", f->vector);
      printf("mask %x ", f->mask);
      printf("mode %x ", f->mode);
      printf("scratch1 %x ", f->scratch1);
      printf("pad %x\n", f->pad);
   }
   if (cputyp == CPU_188 ) {
      unsigned int istr, cur_mask;
      
      istr = *(volatile int *)IST_REG;
      cur_mask = GET_MASK(0, istr);
      printf("emask = 0x%b\n", f->mask, IST_STRING);
      printf("istr  = 0x%b\n", istr, IST_STRING);
      printf("cmask = 0x%b\n", cur_mask, IST_STRING);
   }
}

#if DDB
inline int
db_splhigh(void)
{
   return (db_setipl(IPL_HIGH));
}

inline int
db_splx(int s)
{
   return (db_setipl(s));
}
#endif /* DDB */	


/*
 * Called from locore.S during boot,
 * this is the first C code that's run.
 */


void
mvme_bootstrap(void)
{
   extern char *edata, *end;
   extern int cold;
   extern unsigned number_cpus;
   extern int kernelstart;
   extern int lock_wait_time;
   extern vm_offset_t size_memory(void);
   extern struct consdev *cn_tab;
   extern unsigned vector_list;
   struct bugbrdid brdid;

   cold = 1;  /* we are still booting */
   
   /* zreo out the machine dependant function pointers */
   bzero(&mdfp, sizeof(struct funcp));
   
   buginit(); /* init the bug routines */
   bugbrdid(&brdid);
   cputyp = brdid.brdno;
   
   /* to support the M8120.  It's based off of MVME187 */
   if (cputyp == 0x8120)
      cputyp = CPU_187;
   
   /* 
    * set up interrupt and fp exception handlers 
    * based on the machine.
    */
   switch (cputyp) {
#ifdef MVME188
   case CPU_188:
      mdfp.interrupt_func = &m188_ext_int;
      mdfp.fp_precise_func = &m88100_Xfp_precise;
      /* clear and disable all interrupts */
      *int_mask_reg[0] = 0;
      *int_mask_reg[1] = 0;
      *int_mask_reg[2] = 0;
      *int_mask_reg[3] = 0;
      break;
#endif /* MVME188 */
#ifdef MVME187
   case CPU_187:
      mdfp.interrupt_func = &sbc_ext_int;
      mdfp.fp_precise_func = &m88100_Xfp_precise;
      break;
#endif /* MVME187 */
#ifdef MVME197
   case CPU_197:
      mdfp.interrupt_func = &sbc_ext_int;
      mdfp.fp_precise_func = &m88110_Xfp_precise;
      set_tcfp(); /* Set Time Critical Floating Point Mode */
      break;
#endif /* MVME197 */
   default:
      panic("mvme_bootstrap: Can't determine cpu type.");
   }

   /* startup fake console driver.  It will be replaced by consinit() */
   cn_tab = &bootcons;

   vm_set_page_size();

   first_addr = m88k_round_page(first_addr);

   if (!no_symbols) boothowto |= RB_KDB;

   last_addr = size_memory();
   cmmu_parity_enable();

   printf("%s",version);
   identifycpu();
   setup_board_config();
   cmmu_init();
   master_cpu = cmmu_cpu_number();
   set_cpu_number(master_cpu);
   printf("CPU%d is master CPU\n", master_cpu);
   
#ifdef notevenclose
   if (cputyp == CPU_188 && (boothowto & RB_MINIROOT)) {
      int i;
      for (i=0; i<MAX_CPUS; i++) {
         if(!spin_cpu(i))
            printf("CPU%d started\n", i);
      }
   }
#endif 
   avail_start = first_addr;
   avail_end = last_addr;
#ifdef DEBUG
   printf("MVME%x boot: memory from 0x%x to 0x%x\n", cputyp, avail_start, avail_end);
#endif 
   /*
    * Steal one page at the top of physical memory for msgbuf
    */
   avail_end -= PAGE_SIZE;
   pmap_bootstrap((vm_offset_t)M88K_TRUNC_PAGE((unsigned)&kernelstart) /* = loadpt */, 
                  &avail_start, &avail_end, &virtual_avail,
                  &virtual_end);

   /*
    * Must initialize p_addr before autoconfig or
    * the fault handler will get a NULL reference.
    */
   proc0.p_addr = proc0paddr;
   curproc = &proc0;
   curpcb = &proc0paddr->u_pcb;

   /* Initialize cached PTEs for u-area mapping. */
   save_u_area(&proc0, (vm_offset_t)proc0paddr);

   /*
    * Map proc0's u-area at the standard address (UADDR).
    */
   load_u_area(&proc0);

   /* Initialize the "u-area" pages. */
   bzero((caddr_t)UADDR, UPAGES*NBPG);
#ifdef DEBUG
   printf("leaving mvme_bootstrap()\n");
#endif 
}

/*
 * Boot console routines: 
 * Enables printing of boot messages before consinit().
 */
int
bootcnprobe(cp)
struct consdev *cp;
{
   cp->cn_dev = makedev(14, 0);
   cp->cn_pri = CN_NORMAL;
   return (1);
}

int
bootcninit(cp)
struct consdev *cp;
{
   /* Nothing to do */
}

int
bootcngetc(dev)
dev_t dev;
{
   return (buginchr());
}

void
bootcnputc(dev, c)
dev_t dev;
char c;
{
   int s;

   if (c == '\n')
      bugoutchr('\r');
   bugoutchr(c);
}
