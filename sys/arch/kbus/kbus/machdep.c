/*	$NetBSD: machdep.c,v 1.43 1996/01/15 05:30:47 phil Exp $	*/

/*-
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *
 *	@(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

/*
 * Modified for the pc532 by Phil Nelson.  2/3/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <dev/cons.h>

#include <net/netisr.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/asi.h>
#include <machine/asm.h>
#include <machine/prom.h>
#include <machine/dvma.h>
#include <machine/autoconf.h>

extern vm_offset_t avail_end;
extern struct user *proc0paddr;

vm_map_t buffer_map;

/* A local function... */
void rom_reset	__P((void));
void romhalt	__P((void));
void romboot	__P((const char *));
void rom_reset_c	__P((void));
void rom_warm_c		__P((void));
void dumpsys	__P((void));
void memerr	__P((int type, int ser, int sva, int aer, int ava));
void stackdump __P((void));
void init_series5 __P((void));

void sic_init __P((void));
void zs_init __P((void));
void db_machine_init __P((void));
void ddb_init __P((void));
void _low_level_init __P((int _argc, char *_argv[], char *_envp[]));
caddr_t allocsys __P((caddr_t));

/* the following is used externally (sysctl_hw) */
char machine[] = "kbus";
char cpu_model[] = "sparc";

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
int	msgbufmapped = 0;		/* set when safe to use msgbuf */

static int _mapped;	/* TG: FIXME.  */

/*  Real low level initialization.  This is called in unmapped mode and
    sets up the inital page directory and page tables for the kernel.
    This routine is the first to be called by locore.s to get the
    kernel running at the correct place in memory.
 */

extern char end[], _edata[];
extern vm_offset_t   avail_start;
extern vm_offset_t   avail_end;

/*
 * dvmamap is used to manage DVMA memory. Note: this coincides with
 * the memory range in `phys_map' (which is mostly a place-holder).
 */
struct map *dvmamap;
static int ndvmamap;	/* # of entries in dvmamap */

int physmem = 0;

struct segtab *KPTphys;

int IdlePTD;
int start_page;
static int argc;
static char **argv;
static char **environ;

int low_mem_map;

/* Support for VERY low debugging ... in case we get NO output.
   (e.g. in case pmap does not work and can't do regular mapped
   output. */
#define VERYLOWDEBUG 1

#if VERYLOWDEBUG
#include <kbus/umprintf.c> 
#endif

static void
set_pde (daddr_t va, daddr_t pa)
{
  KPTphys->seg_pde[(va & PD_MASK) >> PD_SHIFT].pd_entry
    = (pa & ~SERIES5_KERN_WINDOW) | PD_V;
  KPTphys->seg_tab[(va & PD_MASK) >> PD_SHIFT] = (pt_entry_t *)pa;
}

static void
set_pte (unsigned long *pte, daddr_t va, daddr_t addr, unsigned long prot)
{
  int index = (va & PT_MASK) >> PG_SHIFT;
  pte[index] = (addr & PG_FRAME) | prot;
}

void
_low_level_init (int _argc, char *_argv[], char *_envp[])
{
  unsigned long *pio;
  unsigned long *pk;
  unsigned addr;
  int i;
  unsigned int val;
  extern char *zs_conschan;

#if VERYLOWDEBUG
  umprintf ("starting low level init\n");
#endif

  mem_size = 64 * 1024 * 1024; /* ram_size(end); */
  physmem = btoc(mem_size);
  start_page = (((int)&end + SERIES5_PAGE_SIZE) & ~(SERIES5_PAGE_SIZE-1))
    & 0x00ffffff;
  avail_start = start_page; 
  avail_end   = mem_size - SERIES5_PAGE_SIZE;

  argc = _argc;
  argv = _argv;
  environ = _envp;

#if 0 /* VERYLOWDEBUG */
  umprintf ("mem_size = 0x%x\n"
	    "physmem=%x\n"
	    "start_page=0x%x\n"
	    "avail_end=0x%x\n",
	    mem_size, physmem, start_page, avail_end);
  umprintf ("args(%d):", argc);
  for (i = 0; i < argc; i++)
    umprintf (" %s", argv[i]);
  umprintf ("\nenviron:\n");
  for (c = environ; *c; c++)
    umprintf ("%s\n", *c);
#endif


  /* Initialize the mmu with a simple memory map. */

  /* The page directory that starts the entire mapping. */
  IdlePTD = (int) avail_start;
  KPTphys = (struct segtab *)(avail_start | SERIES5_KERN_WINDOW);
  avail_start += SERIES5_PAGE_SIZE;

  /* umprintf ("KPTphys = 0x%x\n", KPTphys); */

  /* Initialize the page directory.  */
  bzero((char *)(KPTphys), SERIES5_PDT_SIZE);

  /* umprintf ("KPTphys bzero done\n"); */

  /* The order is important.  */
  /* The page table for kernel, ... */
  pk = (unsigned long *) (avail_start | SERIES5_KERN_WINDOW);
  avail_start += SERIES5_PAGE_SIZE;
  bzero ((char *)pk, SERIES5_PGT_SIZE);

  /* Now for the memory mapped I/O, the ICU and the eprom. */
  pio = (unsigned long *) (avail_start | SERIES5_KERN_WINDOW);
  avail_start += SERIES5_PAGE_SIZE;
  bzero ((char *)pio, SERIES5_PGT_SIZE);
  
  Sysmap = (pt_entry_t *)pk;

  umprintf ("pio = 0x%x; pio bzero done\n", pio);

  /* Addresses here start at Fd000000... */

  /* zs0 is used for console.  */
  set_pte (pio, 0xfdffe000, 0x17012000, PG_V | PG_IO);

  /* Add the memory mapped I/O entry in the directory. */
  set_pde (0xfd000000, (daddr_t)pio);
  set_pde (0xfc000000, (daddr_t)pk);

/*  umprintf ("KPTphys = 0x%x\n", KPTphys); */

  /* Start mapping. */
  _mapped = 1;

  LOAD_PTDB (((daddr_t) KPTphys) & ~SERIES5_KERN_WINDOW);
  val = lda (ASI_MMCR, 0);
  val |= MMCR_ME;
  sta (ASI_MMCR, 0, val);
  sta (ASI_FGTLB_INV, 0, 0);

  zs_conschan = (void *) 0xfdffe020;
  umprintf ("Mmu setup\n");
  /* umprintf ("proc0paddr = %x\n", proc0paddr); */

  proc0.p_addr = proc0paddr;

  addr = ((unsigned)proc0paddr) & SERIES5_KERN_MASK;
  for (i = 0; i < UPAGES; i++)
    {
      curproc->p_md.md_upte[i] = addr | PG_WINDOW;
      sta (ASI_FGTLB_VALD, UADDR + (i << PGSHIFT), addr | PG_WINDOW);
      addr += NBPG;
    }
  sta (ASI_FGTLB_VALD, SERIES5_RED_ZONE, -1); /* Hope it works.  */

  /* So easy to do, but FIXME.  */
  msgbufp = (struct msgbuf *)0xff056000;
  msgbufmapped = 1;
  *(unsigned int *)ROM_MSGBUFP = (unsigned int)msgbufp;
}

/* init_series5 is the first procedure called in mapped mode by locore.s
 */  
void
init_series5 ()
{
  /* umprintf ("init series5...\n"); */

/*#include "ddb.h" */
#if NDDB > 0
	kdb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif

  /* Initialize the pmap stuff.... */
  pmap_bootstrap (avail_start);

  
  printf ("First try\n");
  umprintf ("printf tried\n");

  /* now running on new page tables, configured, and u/iom is accessible */

#ifdef DDB
  db_machine_init ();
  ddb_init ();
#endif
} 


/*
 * Machine-dependent startup code
 */
int boothowto = 0;
long dumplo;
int physmem;

extern int bootdev;
extern cyloffset;

/* pmap_enter prototype */
void pmap_enter __P((register pmap_t, vm_offset_t, register vm_offset_t,
	vm_prot_t, boolean_t));

void
cpu_startup(void)
{
	register unsigned i;
	register caddr_t v;
	int base, residual;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size = 0;
	extern struct user *proc0paddr;

/*	verbose_rom_reset (); */

#ifdef KDB
	kdb_init();			/* startup kernel debugger */
#endif
	proc0.p_addr = proc0paddr;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("\nreal mem  = 0x%x\n", ctob(physmem));

	/*
	 * Allocate space for system data structures.
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 * As pages of memory are allocated and cleared,
	 * "firstaddr" is incremented.
	 * An index into the kernel page table corresponding to the
	 * virtual memory address maintained in "v" is kept in "mapaddr".
	 */

	/*
	 * Make two passes.  The first pass calculates how much memory is
	 * needed and allocates it.  The second pass assigns virtual
	 * addresses to the various data structures.
	 */
	size = (int)allocsys ((caddr_t)0);
	if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(size))) == 0)
	  panic("startup: no room for tables");
	
	if (allocsys(v) - v != size)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;

	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
				   &maxaddr, size, TRUE);
	minaddr = (vm_offset_t)buffers;
	vm_map_print (buffer_map, 1);
	printf ("Buffer size: %d bytes, %d kb, minaddr = %p, buffer_map = %p\n",
		size, size / 1024, minaddr, buffer_map);
	{
	  vm_object_t vmo;
	  vmo = vm_object_allocate (size);
	  printf ("vmo: %p, buffer_map: %p\n", vmo, buffer_map);
	  i = vm_map_find(buffer_map, vmo, (vm_offset_t)0,
			  &minaddr, size, FALSE);
	}
	if (i != KERN_SUCCESS)
	  {
	    printf ("min: %p, max: %p\n",
		    buffer_map->min_offset, buffer_map->max_offset);
	    printf ("vm_map_find: error %d\n", i);
	    panic("startup: cannot allocate buffers");
	  }
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	if (base >= MAXBSIZE) {
		/* don't want to alloc more physical mem than needed */
		base = MAXBSIZE;
		residual = 0;
	}
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
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}


	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				8*NCARGS, TRUE);

	/*
	 * Allocate a map for physio.  Others use a submap of the kernel
	 * map, but we want one completely separate, even though it uses
	 * the same pmap.
	 */
	phys_map = vm_map_create(pmap_kernel(),
				 VM_MIN_IO_ADDRESS, VM_MAX_IO_ADDRESS, 1);
	if (phys_map == NULL)
		panic("unable to create phys_map");
	/*
	 * Allocate DVMA space and dump into a privately managed
	 * resource map for double mappings which is usable from
	 * interrupt contexts.
	 */
	rminit(dvmamap, btoc(1 << 20), 1, "dvmamap", ndvmamap);

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

	printf("avail mem = 0x%x\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Configure the system.
	 */
	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}
	sic_init ();

	/* Drivers that use those OBIO mappings from the PROM */
	zs_init();

	configure();
#if 0
	Debugger ();
#endif
}

caddr_t
allocsys(v)
     caddr_t v;
{
#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))
	valloc(callout, struct callout, ncallout);
	valloc(swapmap, struct map, nswapmap = maxproc * 2);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
	/*
	 * Determine how many buffers to allocate.
	 * Use 10% of memory for the first 2 Meg, 5% of the remaining
	 * memory. Insure a minimum of 16 buffers.
	 * We allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		if (physmem < (2 * 1024 * 1024))
			bufpages = physmem / 10 / CLSIZE;
		else
			bufpages = ((2 * 1024 * 1024 + physmem) / 40) / CLSIZE;

	bufpages = min(NKMEMCLUSTERS*2/5, bufpages);  /* XXX ? - cgd */

	if (nbuf == 0) {
		nbuf = bufpages / 2;
		if (nbuf < 16) {
			nbuf = 16;
			/* XXX (cgd) -- broken vfs_bio currently demands this */
			bufpages = 32;
		}
	}

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);

	/*
	 * Allocate DVMA slots for 1/4 of the number of i/o buffers
	 * and one for each process too (PHYSIO).
	 */
	valloc(dvmamap, struct map, ndvmamap = maxproc + ((nbuf / 4) &~ 1));

	return v;
}

#ifdef PGINPROF
/*
 * Return the difference (in microseconds)
 * between the  current time and a previous
 * time as represented  by the arguments.
 * If there is a pending clock interrupt
 * which has not been serviced due to high
 * ipl, return error code.
 */
/*ARGSUSED*/
vmtime(otime, olbolt, oicr)
	register int otime, olbolt, oicr;
{

	return (((time.tv_sec-otime)*60 + lbolt-olbolt)*16667);
}
#endif

void
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splhigh();

	*tvp = time;
	tvp->tv_usec += tick;
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	splx(s);
}


#if 0
/*
 * insert an element into a queue 
 */
#undef insque
_insque(element, head)
	register struct prochd *element, *head;
{
	element->ph_link = head->ph_link;
	head->ph_link = (struct proc *)element;
	element->ph_rlink = (struct proc *)head;
	((struct prochd *)(element->ph_link))->ph_rlink=(struct proc *)element;
}

/*
 * remove an element from a queue
 */
#undef remque
_remque(element)
	register struct prochd *element;
{
	((struct prochd *)(element->ph_link))->ph_rlink = element->ph_rlink;
	((struct prochd *)(element->ph_rlink))->ph_link = element->ph_link;
	element->ph_rlink = (struct proc *)0;
}
#endif

/* vmunaccess() {printf ("vmunaccess!\n");} */

#if 0
/*
 * Below written in C to allow access to debugging code
 */
copyinstr (fromaddr, toaddr, maxlength, lencopied)
     size_t *lencopied, maxlength;
     void *toaddr, *fromaddr;
{
       int c,tally;

	tally = 0;
	while (maxlength--) {
		c = fubyte(fromaddr++);
		if (c == -1) {
			if(lencopied) *lencopied = tally;
			return(EFAULT);
		}
		tally++;
		*(char *)toaddr++ = (char) c;
		if (c == 0){
			if(lencopied) *lencopied = (u_int)tally;
			return(0);
		}
	}
	if(lencopied) *lencopied = (u_int)tally;
	return(ENAMETOOLONG);
}

copyoutstr (fromaddr, toaddr, maxlength, lencopied)
     size_t *lencopied, maxlength;
     void *fromaddr, *toaddr;
{
	int c;
	int tally;

	tally = 0;
	while (maxlength--) {
		c = subyte(toaddr++, *(char *)fromaddr);
		if (c == -1) return(EFAULT);
		tally++;
		if (*(char *)fromaddr++ == 0){
			if(lencopied) *lencopied = tally;
			return(0);
		}
	}
	if(lencopied) *lencopied = tally;
	return(ENAMETOOLONG);
}

copystr (fromaddr, toaddr, maxlength, lencopied)
     size_t *lencopied, maxlength;
     void *fromaddr, *toaddr;
{
	u_int tally;

	tally = 0;
	while (maxlength--) {
		*(u_char *)toaddr = *(u_char *)fromaddr++;
		tally++;
		if (*(u_char *)toaddr++ == 0) {
			if(lencopied) *lencopied = tally;
			return(0);
		}
	}
	if(lencopied) *lencopied = tally;
	return(ENAMETOOLONG);
}
#endif

/*
 * consinit:
 * initialize the system console.
 * XXX - shouldn't deal with this cons_initted thing, but then,
 * it shouldn't be called from init386 either.
 */
static int cons_initted;

void
consinit()
{
	if (!cons_initted) {
		cninit();
		cons_initted = 1;
	}
}

/*
 * Set up registers on exec.
 *
 * XXX this entire mess must be fixed
 */
/* ARGSUSED */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	register struct trapframe *tf = p->p_md.md_tf;
	register struct fpstate *fs;
	register int psr;

	/*
	 * The syscall will ``return'' to npc or %g7 or %g2; set them all.
	 * Set the rest of the registers to 0 except for %o6 (stack pointer,
	 * built in exec()) and psr (retain CWP and PSR_S bits).
	 */
	psr = tf->tf_psr & (PSR_S | PSR_CWP);
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
	bzero((caddr_t)tf, sizeof *tf);
	tf->tf_psr = psr;
	tf->tf_npc = pack->ep_entry & ~3;
	tf->tf_global[1] = (int)PS_STRINGS;
	tf->tf_global[2] = tf->tf_global[7] = tf->tf_npc;
	stack -= sizeof(struct rwindow);
	tf->tf_out[6] = stack;
	retval[1] = 0;
}

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

struct sigframe {
	int	sf_signo;		/* signal number */
	siginfo_t *sf_sip;		/* points to siginfo_t */
#ifdef COMPAT_SUNOS
	struct	sigcontext *sf_scp;	/* points to user addr of sigcontext */
#else
	int	sf_xxx;			/* placeholder */
#endif
	caddr_t	sf_addr;		/* SunOS compat */
	struct	sigcontext sf_sc;	/* actual sigcontext */
	siginfo_t sf_si;
};

/*
 * machine dependent system variables.
 */
int
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
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{
	register struct proc *p = curproc;
	register struct sigacts *psp = p->p_sigacts;
	register struct sigframe *fp;
	register struct trapframe *tf;
	register int caddr, oonstack, oldsp, newsp;
	struct sigframe sf;
	extern char sigcode[], esigcode[];
#define	szsigcode	(esigcode - sigcode)
#ifdef COMPAT_SUNOS
	extern struct emul emul_sunos;
#endif

	tf = p->p_md.md_tf;
	oldsp = tf->tf_out[6];
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	/*
	 * Compute new user stack addresses, subtract off
	 * one signal frame, and align.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
					 psp->ps_sigstk.ss_size);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)oldsp;
	fp = (struct sigframe *)((int)(fp - 1) & ~7);

#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig: %s[%d] sig %d newusp %p scp %p\n",
		    p->p_comm, p->p_pid, sig, fp, &fp->sf_sc);
#endif
	/*
	 * Now set up the signal frame.  We build it in kernel space
	 * and then copy it out.  We probably ought to just build it
	 * directly in user space....
	 */
	sf.sf_signo = sig;
	sf.sf_sip = NULL;
#ifdef COMPAT_SUNOS
	if (p->p_emul == &emul_sunos) {
		sf.sf_sip = (void *)code;	/* SunOS has "int code" */
		sf.sf_scp = &fp->sf_sc;
		sf.sf_addr = val.sival_ptr;
	}
#endif

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_sc.sc_onstack = oonstack;
	sf.sf_sc.sc_mask = mask;
	sf.sf_sc.sc_sp = oldsp;
	sf.sf_sc.sc_pc = tf->tf_pc;
	sf.sf_sc.sc_npc = tf->tf_npc;
	sf.sf_sc.sc_psr = tf->tf_psr;
	sf.sf_sc.sc_g1 = tf->tf_global[1];
	sf.sf_sc.sc_o0 = tf->tf_out[0];

	if (psp->ps_siginfo & sigmask(sig)) {
		sf.sf_sip = &fp->sf_si;
		initsiginfo(&sf.sf_si, sig, code, type, val);
	}

	/*
	 * Put the stack in a consistent state before we whack away
	 * at it.  Note that write_user_windows may just dump the
	 * registers into the pcb; we need them in the process's memory.
	 * We also need to make sure that when we start the signal handler,
	 * its %i6 (%fp), which is loaded from the newly allocated stack area,
	 * joins seamlessly with the frame it was in when the signal occurred,
	 * so that the debugger and _longjmp code can back up through it.
	 */
	newsp = (int)fp - sizeof(struct rwindow);
	write_user_windows();
	/* XXX do not copyout siginfo if not needed */
	if (rwindow_save(p) || copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf) ||
	    suword(&((struct rwindow *)newsp)->rw_in[6], oldsp)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig: window save or copyout error\n");
#endif
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig: %s[%d] sig %d scp %p\n",
		       p->p_comm, p->p_pid, sig, &fp->sf_sc);
#endif
	/*
	 * Arrange to continue execution at the code copied out in exec().
	 * It needs the function to call in %g1, and a new stack pointer.
	 */
#ifdef COMPAT_SUNOS
	if (psp->ps_usertramp & sigmask(sig)) {
		caddr = (int)catcher;	/* user does his own trampolining */
	} else
#endif
	{
		caddr = (int)PS_STRINGS - szsigcode;
		tf->tf_global[1] = (int)catcher;
	}
	tf->tf_pc = caddr;
	tf->tf_npc = caddr + 4;
	tf->tf_out[6] = newsp;
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig: about to return to catcher\n");
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above),
 * and return to the given trap frame (if there is one).
 * Check carefully to make sure that the user has not
 * modified the state to gain improper privileges or to cause
 * a machine fault.
 */
/* ARGSUSED */
int
sys_sigreturn(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	register struct sigcontext *scp;
	register struct trapframe *tf;

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();
	if (rwindow_save(p))
		sigexit(p, SIGILL);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: %s[%d], sigcntxp %p\n",
		    p->p_comm, p->p_pid, SCARG(uap, sigcntxp));
#endif
	scp = SCARG(uap, sigcntxp);
	if ((int)scp & 3 || useracc((caddr_t)scp, sizeof *scp, B_WRITE) == 0)
		return (EINVAL);
	tf = p->p_md.md_tf;
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((scp->sc_pc | scp->sc_npc) & 3) != 0)
		return (EINVAL);
	/* take only psr ICC field */
	tf->tf_psr = (tf->tf_psr & ~PSR_ICC) | (scp->sc_psr & PSR_ICC);
	tf->tf_pc = scp->sc_pc;
	tf->tf_npc = scp->sc_npc;
	tf->tf_global[1] = scp->sc_g1;
	tf->tf_out[0] = scp->sc_o0;
	tf->tf_out[6] = scp->sc_sp;
	if (scp->sc_onstack & 1)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask & ~sigcantmask;
	return (EJUSTRETURN);
}

int	waittime = -1;

void
boot(howto)
	register int howto;
{
	int i;
	static char str[4];	/* room for "-sd\0" */
	extern int cold;

	if (cold) {
		printf("halted\n\n");
		romhalt();
	}

/*	fb_unblank(); */
	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
#if 1
		extern struct proc proc0;

		/* protect against curproc->p_stats.foo refs in sync()   XXX */
		if (curproc == NULL)
			curproc = &proc0;
#endif
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now unless
		 * the system was sitting in ddb.
		 */
		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}
	(void) splhigh();		/* ??? */
	if (howto & RB_HALT) {
		doshutdownhooks();
		printf("halted\n\n");
		romhalt();
	}
	if (howto & RB_DUMP)
		dumpsys();

	doshutdownhooks();
	printf("rebooting\n\n");
	i = 1;
	if (howto & RB_SINGLE)
		str[i++] = 's';
	if (howto & RB_KDB)
		str[i++] = 'd';
	if (i > 1) {
		str[0] = '-';
		str[i] = 0;
	} else
		str[0] = 0;
	romboot (str);
	/*NOTREACHED*/
	while (1)
	  ;
}

#if 0
u_long	dumpmag = 0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumplo = 0;

void
dumpconf()
{
	register int nblks, nmem;
	register struct memarr *mp;

	dumpsize = physmem;

	/*
	 * savecore views the image in units of pages (i.e., dumpsize is in
	 * pages) so we round the two mmu entities into page-sized chunks.
	 * The PMEGs (32kB) and the segment table (512 bytes plus padding)
	 * are appending to the end of the crash dump.
	 */
	/* dumpsize += pmap_dumpsize(); TG:FIXME */
	if (dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize) {
		nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
		/*
		 * Don't dump on the first CLBYTES (why CLBYTES?)
		 * in case the dump device includes a disk label.
		 */
		if (dumplo < btodb(CLBYTES))
			dumplo = btodb(CLBYTES);

		/*
		 * If dumpsize is too big for the partition, truncate it.
		 * Otherwise, put the dump at the end of the partition
		 * by making dumplo as large as possible.
		 */
		if (dumpsize > btoc(dbtob(nblks - dumplo)))
			dumpsize = btoc(dbtob(nblks - dumplo));
		else if (dumplo + ctod(dumpsize) > nblks)
			dumplo = nblks - ctod(dumpsize);
	}
}

#define	BYTES_PER_DUMP	(32 * 1024)	/* must be a multiple of pagesize */
static vm_offset_t dumpspace;

caddr_t
reserve_dumppages(p)
	caddr_t p;
{

	dumpspace = (vm_offset_t)p;
	return (p + BYTES_PER_DUMP);
}
#endif

/*
 * Write a crash dump.
 */
void
dumpsys()
{
#if 0
	register int psize;
	register daddr_t blkno;
	register int (*dump)	__P((dev_t, daddr_t, caddr_t, size_t));
	int error = 0;
	register struct memarr *mp;
	register int nmem;
	extern struct memarr pmemarr[];
	extern int npmemarr;

	/* copy registers to memory */
	snapshot(cpcb);
	stackdump();

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
	printf("\ndumping to dev %x, offset %ld\n", dumpdev, dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;

	for (mp = pmemarr, nmem = npmemarr; --nmem >= 0; mp++) {
		register unsigned i = 0, n;
		register maddr = mp->addr;

		if (maddr == 0) {
			/* Skip first page at physical address 0 */
			maddr += NBPG;
			i += NBPG;
			blkno += btodb(NBPG);
		}

		for (; i < mp->len; i += n) {
			n = mp->len - i;
			if (n > BYTES_PER_DUMP)
				 n = BYTES_PER_DUMP;

			/* print out how many MBs we have dumped */
			if (i && (i % (1024*1024)) == 0)
				printf("%d ", i / (1024*1024));

			(void) pmap_map(dumpspace, maddr, maddr + n,
					VM_PROT_READ);
			error = (*dump)(dumpdev, blkno,
					(caddr_t)dumpspace, (int)n);
			pmap_remove(pmap_kernel(), dumpspace, dumpspace + n);
			if (error)
				break;
			maddr += n;
			blkno += btodb(n);
		}
	}
	if (!error)
		error = pmap_dumpmmu(dump, blkno);

	switch (error) {

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

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
#endif
}

/*
 * get the fp and dump the stack as best we can.  don't leave the
 * current stack page
 */
void
stackdump()
{
	struct frame *fp = (struct frame *) getfp(), *sfp;

	sfp = fp;
	printf("Frame pointer is at %p\n", fp);
	printf("Call traceback:\n");
	while (fp && ((u_long)fp >> PGSHIFT) == ((u_long)sfp >> PGSHIFT)) {
		printf("  pc = %x  args = (%x, %x, %x, %x, %x, %x, %x) fp = %p\n",
		    fp->fr_pc, fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2],
		    fp->fr_arg[3], fp->fr_arg[4], fp->fr_arg[5], fp->fr_arg[6],
		    fp->fr_fp);
		fp = fp->fr_fp;
	}
}

int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error = ENOEXEC;

#ifdef COMPAT_SUNOS
	extern sunos_exec_aout_makecmds __P((struct proc *, struct exec_package *));
	if ((error = sunos_exec_aout_makecmds(p, epp)) == 0)
		return 0;
#endif
	return error;
}

#if 0
int
ldcontrolb(addr)
caddr_t addr;
{
	struct pcb *xpcb;
	extern struct user *proc0paddr;
	u_long saveonfault;
	int res;
	int s;

	printf("warning: ldcontrolb called in sun4m\n");
	return 0;

	s = splhigh();
	if (curproc == NULL)
		xpcb = (struct pcb *)proc0paddr;
	else
		xpcb = &curproc->p_addr->u_pcb;

	saveonfault = (u_long)xpcb->pcb_onfault;
        res = xldcontrolb(addr, xpcb);
	xpcb->pcb_onfault = (caddr_t)saveonfault;

	splx(s);
	return (res);
}
#endif

void
wzero(vb, l)
	void *vb;
	u_int l;
{
	u_char *b = vb;
	u_char *be = b + l;
	u_short *sp;

	if (l == 0)
		return;

	/* front, */
	if ((u_long)b & 1)
		*b++ = 0;

	/* back, */
	if (b != be && ((u_long)be & 1) != 0) {
		be--;
		*be = 0;
	}

	/* and middle. */
	sp = (u_short *)b;
	while (sp != (u_short *)be)
		*sp++ = 0;
}

void
wcopy(vb1, vb2, l)
	const void *vb1;
	void *vb2;
	u_int l;
{
	const u_char *b1e, *b1 = vb1;
	u_char *b2 = vb2;
	u_short *sp;
	int bstore = 0;

	if (l == 0)
		return;

	/* front, */
	if ((u_long)b1 & 1) {
		*b2++ = *b1++;
		l--;
	}

	/* middle, */
	sp = (u_short *)b1;
	b1e = b1 + l;
	if (l & 1)
		b1e--;
	bstore = (u_long)b2 & 1;

	while (sp < (u_short *)b1e) {
		if (bstore) {
			b2[1] = *sp & 0xff;
			b2[0] = *sp >> 8;
		} else
			*((short *)b2) = *sp;
		sp++;
		b2 += 2;
	}

	/* and back. */
	if (l & 1)
		*b2 = *b1e;
}

void
memerr (int type, int ser, int sva, int aer, int ava)
{
  panic ("memory error");
}


#define CALL_ROM_COMMAND (**(void (**)(void))ROM_COMMAND)()
#define GET_ROM_COMMAREA (*(struct prom_command_area **)ROM_COMM_AREA)

void
rom_reset_c (void)
{
  char *command = "reset intr";

  if (!*(int *)ROM_COMMAND)
    {
      *(int *)ROM_COMMAND = 0xfe002a80;
      *(int *)ROM_COMM_AREA = 0xff00e018;
    }
  GET_ROM_COMMAREA->command_ptr = command;
  CALL_ROM_COMMAND;
}

void
rom_warm_c (void)
{
  char *command = "reset warm";

  if (!*(int *)ROM_COMMAND)
    {
      *(int *)ROM_COMMAND = 0xfe002a80;
      *(int *)ROM_COMM_AREA = 0xff00e018;
    }
  GET_ROM_COMMAREA->command_ptr = command;
  CALL_ROM_COMMAND;
}

void
verbose_rom_reset (void)
{
  umprintf ("rom_version: %x\n", *(unsigned *)ROM_VERSION);
  umprintf ("rom_commarea: %x\n", *(unsigned *)ROM_COMM_AREA);
  umprintf ("rom_command: %x\n", *(unsigned *)ROM_COMMAND);
  umprintf ("rom_msgbufp: %x\n", *(unsigned *)ROM_MSGBUFP);
  umprintf ("rom_dgram: %x\n", *(unsigned *)ROM_DGRAM);
  umprintf ("rom_eeversion: %x\n", *(unsigned *)ROM_EEVERSION);
  umprintf ("rom_revision: %x\n", *(unsigned *)ROM_REVISION);
}

void
romhalt (void)
{
  rom_reset ();
}

void
romboot (str)
	const char *str;
{
  rom_reset ();
}

static char *
getenv (const char *str)
{
  char **c;
  size_t len = strlen (str);

  for (c = environ; *c; c++)
    {
      if ((*c)[len] == '=' && strncmp (*c, str, len) == 0)
        return (*c) + len + 1;
    }
  return NULL;
}

void
idprom_etheraddr(ether)
        u_char *ether;
{
  char *ea = getenv ("ENETADDR");
  u_char c;
  int i;

  if (ea == NULL)
    panic ("ERROR: ethernet address not set!");

  for (i = 0; i < 6; i++)
    {
      if (*ea >= '0' && *ea <= '9')
        c = *ea - '0';
      else if (*ea >= 'a' && *ea <= 'f')
        c = *ea - 'a' + 10;
      else if (*ea >= 'A' && *ea <= 'F')
        c = *ea - 'A' + 10;
      else
        panic ("Bad character for enet addr (%s)", ea);
      ea++;
      if (i == 5 && *ea == 0)
	break;
            if (i < 5 && *ea == ':')
	      {
          *ether++ = c;
          ea++;
          continue;
        }
      c <<= 4;
      if (*ea >= '0' && *ea <= '9')
        c |= *ea - '0';
      else if (*ea >= 'a' && *ea <= 'f')
        c |= *ea - 'a' + 10;
      else if (*ea >= 'A' && *ea <= 'F')
        c |= *ea - 'A' + 10;
      else
        panic ("Bad character for enet addr (%s)", ea);
      ea++;
      if (i != 5 && *ea != ':')
        panic ("Bad character for enet addr (%s)", ea);
      ea++;
      *ether++ = c;
    }

}

#ifndef FFS
void
ffs_mountroot (void)
{
  panic ("ffs_mountroot");
}
#endif
