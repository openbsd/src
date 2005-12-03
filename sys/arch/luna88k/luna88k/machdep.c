/*	$OpenBSD: machdep.c,v 1.27 2005/12/03 14:30:05 miod Exp $	*/
/*
 * Copyright (c) 1998, 1999, 2000, 2001 Steve Murphree, Jr.
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/extent.h>
#include <sys/core.h>
#include <sys/kcore.h>

#include <net/netisr.h>

#include <machine/asm.h>
#include <machine/asm_macro.h>
#include <machine/board.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/kcore.h>
#include <machine/locore.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/m88100.h>

#include <luna88k/luna88k/isr.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include "ksyms.h"
#if DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>		/* db_printf()		*/
#endif /* DDB */

typedef struct {
	unsigned word_one, word_two;
} m88k_exception_vector_area;

caddr_t	allocsys(caddr_t);
void	consinit(void);
void	dosoftint(void);
void	dumpconf(void);
void	dumpsys(void);
int	getcpuspeed(void);
vaddr_t	get_slave_stack(void);
void	identifycpu(void);
void	luna88k_bootstrap(void);
u_int	safe_level(u_int, u_int);
void	savectx(struct pcb *);
void	setlevel(unsigned int);
void	slave_pre_main(void);
int	slave_main(void);
void	vector_init(m88k_exception_vector_area *, unsigned *);

vaddr_t size_memory(void);
void powerdown(void);
void get_fuse_rom_data(void);
void get_nvram_data(void);
char *nvram_by_symbol(char *);
void get_autoboot_device(void);			/* in disksubr.c */
int clockintr(void *);				/* in clock.c */

/*
 * *int_mask_reg[CPU]
 * Points to the hardware interrupt status register for each CPU.
 */
unsigned int *volatile int_mask_reg[] = {
	(unsigned int *)INT_ST_MASK0,
	(unsigned int *)INT_ST_MASK1,
	(unsigned int *)INT_ST_MASK2,
	(unsigned int *)INT_ST_MASK3
};

unsigned int luna88k_curspl[] = {0, 0, 0, 0};

unsigned int int_mask_val[INT_LEVEL] = {
	INT_MASK_LV0,
	INT_MASK_LV1,
	INT_MASK_LV2,
	INT_MASK_LV3,
	INT_MASK_LV4,
	INT_MASK_LV5,
	INT_MASK_LV6,
	INT_MASK_LV7
};

unsigned int int_set_val[INT_LEVEL] = {
	INT_SET_LV0,
	INT_SET_LV1,
	INT_SET_LV2,
	INT_SET_LV3,
	INT_SET_LV4,
	INT_SET_LV5,
	INT_SET_LV6,
	INT_SET_LV7
};

/*
 * *clock_reg[CPU]
 */
unsigned int *volatile clock_reg[] = {
	(unsigned int *)OBIO_CLOCK0,
	(unsigned int *)OBIO_CLOCK1,
	(unsigned int *)OBIO_CLOCK2,
	(unsigned int *)OBIO_CLOCK3
};

/*
 * FUSE ROM and NVRAM data
 */
struct fuse_rom_byte {
	u_int32_t h;
	u_int32_t l;
};
#define FUSE_ROM_BYTES        (FUSE_ROM_SPACE / sizeof(struct fuse_rom_byte))
char fuse_rom_data[FUSE_ROM_BYTES];

#define NNVSYM		8
#define NVSYMLEN	16
#define NVVALLEN	16
struct nvram_t {
	char symbol[NVSYMLEN];
	char value[NVVALLEN];
} nvram[NNVSYM];

vaddr_t obiova;

int ssir;

int physmem;	  /* available physical memory, in pages */

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

/*
 * Declare these as initialized data so we can patch them.
 */
#ifdef	NBUF
int nbuf = NBUF;
#else
int nbuf = 0;
#endif

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

#ifdef	BUFPAGES
int bufpages = BUFPAGES;
#else
int bufpages = 0;
#endif
int bufcachepercent = BUFCACHEPERCENT;

/*
 * Info for CTL_HW
 */
char  machine[] = MACHINE;	 /* cpu "architecture" */
char  cpu_model[120];

#if defined(DDB) || NKSYMS > 0
extern char *esym;
#endif

int machtype = LUNA_88K;	/* may be overwritten in cpu_startup() */
int cputyp = CPU_88100;		/* XXX: aoyama */
int boothowto;			/* XXX: should be set in boot loader and locore.S */
int bootdev;			/* XXX: should be set in boot loader and locore.S */
int cpuspeed = 33;		/* safe guess */
int sysconsole = 1;		/* 0 = ttya, 1 = keyboard/mouse, used in dev/sio.c */
u_int16_t dipswitch = 0;	/* set in locore.S */
int hwplanebits;		/* set in locore.S */

int netisr;

extern struct consdev syscons;	/* in dev/siotty.c */

extern void greeting(void);	/* in dev/lcd.c */
extern void syscnattach(int);	/* in dev/siotty.c */
extern int omfb_cnattach(void);	/* in dev/lunafb.c */
extern void ws_cnattach(void);	/* in dev/lunaws.c */

vaddr_t first_addr;
vaddr_t last_addr;

vaddr_t avail_start, avail_end;
vaddr_t virtual_avail, virtual_end;

extern struct user *proc0paddr;

/*
 * This is to fake out the console routines, while booting.
 * We could use directly the romtty console, but we want to be able to
 * configure a kernel without romtty since we do not necessarily need a
 * full-blown console driver.
 */
cons_decl(romtty);
extern void nullcnpollc(dev_t, int);

struct consdev romttycons = {
	NULL, 
	NULL, 
	romttycngetc, 
	romttycnputc,
	nullcnpollc,
	NULL,
	makedev(14, 0),
	CN_NORMAL,
};

/*
 * Early console initialization: called early on from main, before vm init.
 */
void
consinit()
{
	/*
	 * Initialize the console before we print anything out.
	 */
	if (sysconsole == 0) {
                syscnattach(0);
        } else {
                omfb_cnattach();
                ws_cnattach();
        }

#if defined(DDB)
	db_machine_init();
	ddb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
}

/*
 * Figure out how much real memory is available.
 * Start looking from the megabyte after the end of the kernel data,
 * until we find non-memory.
 */
vaddr_t
size_memory()
{
	unsigned int *volatile look;
	unsigned int *max;
#if 0
	extern char *end;
#endif
#define PATTERN   0x5a5a5a5a
#define STRIDE    (4*1024) 	/* 4k at a time */
#define Roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))
	/*
	 * count it up.
	 */
	max = (void *)MAXPHYSMEM;
#if 0
	for (look = (void *)Roundup(end, STRIDE); look < max;
#else
	for (look = (void *)first_addr; look < max;
#endif
	    look = (int *)((unsigned)look + STRIDE)) {
		unsigned save;

		/* if can't access, we've reached the end */
		if (badwordaddr((vaddr_t)look)) {
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

	return (trunc_page((unsigned)look));
}

int
getcpuspeed()
{
	switch(machtype) {
	case LUNA_88K:
		return 25;
	case LUNA_88K2:
		return 33;
	default:
		panic("getcpuspeed: can not determine CPU speed");
	}
}

void
identifycpu()
{
	cpuspeed = getcpuspeed();
	snprintf(cpu_model, sizeof cpu_model,
	    "OMRON LUNA-88K%s, %dMHz", 
	    machtype == LUNA_88K2 ? "2" : "", cpuspeed);
}

void
cpu_startup()
{
	caddr_t v;
	int sz, i;
	vsize_t size;
	int base, residual;
	vaddr_t minaddr, maxaddr;

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in luna88k_bootstrap() to compensate.
	 */
	for (i = 0; i < btoc(MSGBUFSIZE); i++)
		pmap_kenter_pa((paddr_t)msgbufp + i * PAGE_SIZE,
		    avail_end + i * PAGE_SIZE, VM_PROT_READ | VM_PROT_WRITE);
	pmap_update(pmap_kernel());
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

	/* Determine the machine type from FUSE ROM data.  */
	get_fuse_rom_data();
	if (strncmp(fuse_rom_data, "MNAME=LUNA88K+", 14) == 0) {
		machtype = LUNA_88K2;
	}

        /* Determine the 'auto-boot' device from NVRAM data */
        get_nvram_data();
        get_autoboot_device();

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	printf("real mem  = %d\n", ctob(physmem));

	/*
	 * Check front DIP switch setting
	 */
#ifdef DEBUG
	printf("dipsw = 0x%x\n", dipswitch);
#endif

	/* Check DIP switch 1 - 1 */
	if ((0x8000 & dipswitch) == 0) {
		boothowto |= RB_SINGLE;
	}

	/* Check DIP switch 1 - 3 */
	if ((0x2000 & dipswitch) == 0) {
		boothowto |= RB_ASKNAME;
	}

	/* Check DIP switch 1 - 4 */
	if ((0x1000 & dipswitch) == 0) {
		boothowto |= RB_CONFIG;
	}

	/*
	 * Check frame buffer depth.
	 */
	switch (hwplanebits) {
	case 0:				/* No frame buffer */
	case 1:
	case 4:
	case 8:
		break;
	default:
		printf("unexpected frame buffer depth = %d\n", hwplanebits);
		hwplanebits = 0;
		break;
	}

#if 0 /* just for test */
	/*
	 * Get boot arguments
	 */
	{
		char buf[256];
		char **p = (volatile char **)0x00001120;

		strncpy(buf, *p, 256);
		if (buf[255] != '\0')
			buf[255] = '\0';

		printf("boot arg: (0x%x) %s\n", *p, buf);
	}
#endif

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);

	if ((v = (caddr_t)uvm_km_zalloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Grab the OBIO space that we hardwired in pmap_bootstrap
	 */
	obiova = OBIO_START;
	uvm_map(kernel_map, (vaddr_t *)&obiova, OBIO_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	      UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	        UVM_ADV_NORMAL, 0));
	if (obiova != OBIO_START)
		panic("obiova %lx: OBIO not free", obiova);

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
	    NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_NONE,
	      UVM_PROT_NONE, UVM_INH_NONE, UVM_ADV_NORMAL, 0)))
		panic("cpu_startup: cannot allocate VM for buffers");
	minaddr = (vaddr_t)buffers;

	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;

	for (i = 0; i < nbuf; i++) {
		vsize_t curbufsize;
		vaddr_t curbuf;
		struct vm_page *pg;

		/*
		 * Each buffer has MAXBSIZE bytes of VM space allocated.  Of
		 * that MAXBSIZE space, we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vaddr_t)buffers + (i * MAXBSIZE);
		curbufsize = PAGE_SIZE * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("cpu_startup: not enough memory for "
				      "buffer cache");
			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
			    VM_PROT_READ | VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	pmap_update(pmap_kernel());

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    16 * NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate map for physio.
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);

	printf("avail mem = %ld (%d pages)\n", ptoa(uvmexp.free), uvmexp.free);
	printf("using %d buffers containing %d bytes of memory\n", nbuf,
	    bufpages * PAGE_SIZE);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Initialize the autovectored interrupt list.
	 */
	isrinit();

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

	/*
	 * Say hello to the world on LCD.
	 */
	greeting();
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
	caddr_t v;
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))

#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Determine how many buffers to allocate.  We use 10% of the
	 * first 2MB of memory, and 5% of the rest, with a minimum of 16
	 * buffers.  We allocate 1/2 as many swap buffer headers as file
	 * i/o buffers.
	 */
	if (bufpages == 0) {
		bufpages = (btoc(2 * 1024 * 1024) + physmem) *
		    bufcachepercent / 100;
	}
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

	/* Restrict to at most 70% filled kvm */
	if (nbuf >
	    (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) / MAXBSIZE * 7 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 7 / 10;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / PAGE_SIZE)
		bufpages = nbuf * MAXBSIZE / PAGE_SIZE;

	valloc(buf, struct buf, nbuf);

	return v;
}

__dead void
boot(howto)
	int howto;
{
	/* take a snapshot before clobbering any registers */
	if (curproc && curproc->p_addr)
		savectx(curpcb);

	/* If system is cold, just halt. */
	if (cold) {
		/* (Unless the user explicitly asked for reboot.) */
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0) {
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now unless
		 * the system was sitting in ddb.
		 */
		if ((howto & RB_TIMEBAD) == 0)
			resettodr();
		else
			printf("WARNING: not updating battery clock\n");
	}

	/* Disable interrupts. */
	splhigh();

	/* If rebooting and a dump is requested, do it. */
	if (howto & RB_DUMP)
		dumpsys();

haltsys:
	/* Run any shutdown hooks. */
	doshutdownhooks();

	/* Luna88k supports automatic powerdown */
	if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
		printf("attempting to power down...\n");
		powerdown();
		/* if failed, fall through. */
	}

	if (howto & RB_HALT) {
		printf("halted\n\n");
	} else {
		/* Reset all cpus, which causes reboot */
		*((volatile unsigned *)0x6d000010) = 0;
	}

	for (;;);  /* to keep compiler happy, and me from going crazy */
	/*NOTREACHED*/
}

unsigned dumpmag = 0x8fca0101;	 /* magic number for savecore */
int   dumpsize = 0;	/* also for savecore */
long  dumplo = 0;
cpu_kcore_hdr_t cpu_kcore_hdr;

/*
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first PAGE_SIZE of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
dumpconf()
{
	int nblks;	/* size of dump area */
	int maj;

	if (dumpdev == NODEV)
		return;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		return;

	dumpsize = physmem;

	/* luna88k only uses a single segment. */
	cpu_kcore_hdr.ram_segs[0].start = 0;
	cpu_kcore_hdr.ram_segs[0].size = ctob(physmem);
	cpu_kcore_hdr.cputype = cputyp;

	/*
	 * Don't dump on the first block
	 * in case the dump device includes a disk label.
	 */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize + 1 > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo) - 1;
	if (dumplo < nblks - ctod(dumpsize) - 1)
		dumplo = nblks - ctod(dumpsize) - 1;
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
void
dumpsys()
{
	int maj;
	int psize;
	daddr_t blkno;		/* current block to write */
				/* dump routine */
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
	int pg;			/* page being dumped */
	paddr_t maddr;		/* PA being dumped */
	int error;		/* error code from (*dump)() */
	kcore_seg_t *kseg_p;
	cpu_kcore_hdr_t *chdr_p;
	char dump_hdr[dbtob(1)];	/* XXX assume hdr fits in 1 block */

	extern int msgbufmapped;

	msgbufmapped = 0;

	/* Make sure dump device is valid. */
	if (dumpdev == NODEV)
		return;
	if (dumpsize == 0) {
		dumpconf();
		if (dumpsize == 0)
			return;
	}
	maj = major(dumpdev);
	if (dumplo < 0) {
		printf("\ndump to dev %u,%u not possible\n", maj,
		    minor(dumpdev));
		return;
	}
	dump = bdevsw[maj].d_dump;
	blkno = dumplo;

	printf("\ndumping to dev %u,%u offset %ld\n", maj,
	    minor(dumpdev), dumplo);

	/* Setup the dump header */
	kseg_p = (kcore_seg_t *)dump_hdr;
	chdr_p = (cpu_kcore_hdr_t *)&dump_hdr[ALIGN(sizeof(*kseg_p))];
	bzero(dump_hdr, sizeof(dump_hdr));

	CORE_SETMAGIC(*kseg_p, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	kseg_p->c_size = dbtob(1) - ALIGN(sizeof(*kseg_p));
	*chdr_p = cpu_kcore_hdr;

	printf("dump ");
	psize = (*bdevsw[maj].d_psize)(dumpdev);
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

	/* Dump the header. */
	error = (*dump)(dumpdev, blkno++, (caddr_t)dump_hdr, dbtob(1));
	if (error != 0)
		goto abort;

	maddr = (paddr_t)0;
	for (pg = 0; pg < dumpsize; pg++) {
#define NPGMB	(1024 * 1024 / PAGE_SIZE)
		/* print out how many MBs we have dumped */
		if (pg != 0 && (pg % NPGMB) == 0)
			printf("%d ", pg / NPGMB);
#undef NPGMB
		pmap_enter(pmap_kernel(), (vaddr_t)vmmap, maddr,
		    VM_PROT_READ, VM_PROT_READ|PMAP_WIRED);

		error = (*dump)(dumpdev, blkno, vmmap, PAGE_SIZE);
		if (error == 0) {
			maddr += PAGE_SIZE;
			blkno += btodb(PAGE_SIZE);
		} else
			break;
	}
abort:
	switch (error) {
	case 0:
		printf("succeeded\n");
		break;

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

	case EINTR:
		printf("aborted from console\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
}

/* gets an interrupt stack for slave processors */
vaddr_t
get_slave_stack()
{
	vaddr_t addr;

	addr = (vaddr_t)uvm_km_zalloc(kernel_map, INTSTACK_SIZE);

	if (addr == NULL)
		panic("Cannot allocate slave stack for cpu %d",
		    cpu_number());

	return addr;
}

/*
 * Slave CPU pre-main routine.
 * Determine CPU number and set it.
 *
 * Running on an interrupt stack here; do nothing fancy.
 */
void
slave_pre_main()
{
	set_cpu_number(cmmu_cpu_number()); /* Determine cpu number by CMMU */
	splhigh();
	set_psr(get_psr() & ~PSR_IND);
}

/* dummy main routine for slave processors */
int
slave_main()
{
	printf("slave CPU%d started\n", cpu_number());
	while (1); /* spin forever */
	return 0;
}

/*
 *	Device interrupt handler for LUNA88K
 */

void 
luna88k_ext_int(u_int v, struct trapframe *eframe)
{
	int cpu = cpu_number();
	unsigned int cur_mask, cur_int;
	unsigned int level, old_spl;

	cur_mask = *int_mask_reg[cpu];
	old_spl = luna88k_curspl[cpu];
	eframe->tf_mask = old_spl;

	cur_int = cur_mask >> 29;

	if (cur_int == 0) {
		/*
		 * Spurious interrupts - may be caused by debug output clearing
		 * serial port interrupts.
		 */
		printf("luna88k_ext_int(): Spurious interrupts?\n");
		flush_pipeline();
		goto out;
	}
 
	uvmexp.intrs++;

	/* 
	 * We want to service all interrupts marked in the IST register
	 * They are all valid because the mask would have prevented them
	 * from being generated otherwise.  We will service them in order of
	 * priority. 
	 */

	/* XXX: This is very rough. Should be considered more. (aoyama) */
	do {
		level = (cur_int > old_spl ? cur_int : old_spl);
		if (level >= 8) {
			register int i;

			printf("safe level %d <= old level %d\n", level, old_spl);
			printf("cur_int = 0x%x\n", cur_int);

			for (i = 0; i < 4; i++)
				printf("IEN%d = 0x%x  ", i, *int_mask_reg[i]);
			printf("\nCPU0 spl %d  CPU1 spl %d  CPU2 spl %d  CPU3 spl %d\n",
			       luna88k_curspl[0], luna88k_curspl[1],
			       luna88k_curspl[2], luna88k_curspl[3]);
			for (i = 0; i < 8; i++)
				printf("int_mask[%d] = 0x%08x\n", i, int_mask_val[i]);
			printf("--CPU %d halted--\n", cpu_number());
			setipl(IPL_ABORT);
			for(;;) ;
		}

#ifdef DEBUG
		if (level > 7 || (char)level < 0) {
			panic("int level (%x) is not between 0 and 7", level);
		}
#endif

		setipl(level);
	  
		set_psr(get_psr() & ~PSR_IND);

		switch(cur_int) {
		case CLOCK_INT_LEVEL:
			clockintr((void *)eframe);
			break;
		case 5:
		case 4:
		case 3:
			isrdispatch_autovec(cur_int);
			break;
		default:
			printf("luna88k_ext_int(): level %d interrupt.\n", cur_int);
			break;
		}
	} while ((cur_int = (*int_mask_reg[cpu]) >> 29) != 0);

	/*
	 * process any remaining data access exceptions before
	 * returning to assembler
	 */
	set_psr(get_psr() | PSR_IND);
out:
	if (eframe->tf_dmt0 & DMT_VALID)
		m88100_trap(T_DATAFLT, eframe);

	/*
	 * Restore the mask level to what it was when the interrupt
	 * was taken.
	 */
	setipl(eframe->tf_mask);
}

int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{

	return (ENOEXEC);
}

int
sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_sysarch_args	/* {
	   syscallarg(int) op;
	   syscallarg(char *) parm;
	} */ *uap = v;
#endif

	return (ENOSYS);
}

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
	dev_t consdev;

	/* all sysctl names are this level are terminal */
	if (namelen != 1)
		return (ENOTDIR); /* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
		    sizeof consdev));
	default:
		return (EOPNOTSUPP);
	}
	/*NOTREACHED*/
}

void
dosoftint()
{
	if (ssir & SIR_NET) {
		siroff(SIR_NET);
		uvmexp.softs++;
#define DONETISR(bit, fn) \
	do { \
		if (netisr & (1 << bit)) { \
			netisr &= ~(1 << bit); \
			fn(); \
		} \
	} while (0)
#include <net/netisr_dispatch.h>
#undef DONETISR
	}

	if (ssir & SIR_CLOCK) {
		siroff(SIR_CLOCK);
		uvmexp.softs++;
		softclock();
	}
}

int
spl0()
{
	int x;
	x = splsoftclock();

	if (ssir) {
		dosoftint();
	}

	setipl(0);

	return (x);
}

/*
 * Called from locore.S during boot,
 * this is the first C code that's run.
 */
void
luna88k_bootstrap()
{
	extern int kernelstart;
	extern struct consdev *cn_tab;
	extern struct cmmu_p cmmu8820x;
	extern char *end;
#ifndef MULTIPROCESSOR
	u_int master_cpu;
#endif

	cmmu = &cmmu8820x;

	/* clear and disable all interrupts */
	*int_mask_reg[0] = 0;
	*int_mask_reg[1] = 0;
	*int_mask_reg[2] = 0;
	*int_mask_reg[3] = 0;

	/* startup fake console driver.  It will be replaced by consinit() */
	cn_tab = &romttycons;

	uvmexp.pagesize = PAGE_SIZE;
	uvm_setpagesize();

	first_addr = round_page((vaddr_t)&end);	/* XXX temp until symbols */
	last_addr = size_memory();
	physmem = btoc(last_addr);

	cmmu_parity_enable();

	setup_board_config();
	cmmu_init();
	master_cpu = cmmu_cpu_number();
	set_cpu_number(master_cpu);

	/*
	 * Now that set_cpu_number() set us with a valid cpu_info pointer,
	 * we need to initialize p_addr and curpcb before autoconf, for the
	 * fault handler to behave properly [except for badaddr() faults,
	 * which can be taken care of without a valid curcpu()].
	 */
	proc0.p_addr = proc0paddr;
	curproc = &proc0;
	curpcb = &proc0paddr->u_pcb;

	/*
	 * We may have more than one CPU, so mention which one is the master.
	 * We will also want to spin up slave CPUs on the long run...
	 */
	printf("CPU%d is master CPU\n", master_cpu);

#if 0
	int i;
	for (i = 0; i < MAX_CPUS; i++) {
		if (!spin_cpu(i))
			printf("CPU%d started\n", i);
	}
#endif

	avail_start = first_addr;
	avail_end = last_addr;

	/*
	 * Steal MSGBUFSIZE at the top of physical memory for msgbuf
	 */
	avail_end -= round_page(MSGBUFSIZE);

#ifdef DEBUG
	printf("LUNA88K boot: memory from 0x%x to 0x%x\n",
	    avail_start, avail_end);
#endif
	pmap_bootstrap((vaddr_t)trunc_page((unsigned)&kernelstart));

	/*
	 * Tell the VM system about available physical memory.
	 * luna88k only has one segment.
	 */
	uvm_page_physload(atop(avail_start), atop(avail_end),
	    atop(avail_start), atop(avail_end),VM_FREELIST_DEFAULT);

	/* Initialize the "u-area" pages. */
	bzero((caddr_t)curpcb, USPACE);
#ifdef DEBUG
	printf("leaving luna88k_bootstrap()\n");
#endif
}

/*
 * Rom console routines: 
 * Enables printing of boot messages before consinit().
 */

#define __ROM_FUNC_TABLE	((int **)0x00001100)
#define ROMGETC()	(*(int (*)(void))__ROM_FUNC_TABLE[3])()
#define ROMPUTC(x)	(*(void (*)(int))__ROM_FUNC_TABLE[4])(x)

void
romttycnprobe(cp)
	struct consdev *cp;
{
	cp->cn_dev = makedev(14, 0);
	cp->cn_pri = CN_NORMAL;
}

void
romttycninit(cp)
	struct consdev *cp;
{
	/* Nothing to do */
}

int
romttycngetc(dev)
	dev_t dev;
{
	int s, c;

	do {
		s = splhigh();
		c = ROMGETC();
		splx(s);
	} while (c == -1);
	return c;
}

void
romttycnputc(dev, c)
	dev_t dev;
	int c;
{
	int s;

#if 0
	if ((char)c == '\n')
		ROMPUTC('\r');
#endif
	s = splhigh();
	ROMPUTC(c);
	splx(s);
}

/* taken from NetBSD/luna68k */
void
microtime(tvp)
        register struct timeval *tvp;
{
        int s = splclock();
        static struct timeval lasttime;

        *tvp = time;
#ifdef notdef
        tvp->tv_usec += clkread();
        while (tvp->tv_usec >= 1000000) {
                tvp->tv_sec++;
                tvp->tv_usec -= 1000000;
        }
#endif
        if (tvp->tv_sec == lasttime.tv_sec &&
            tvp->tv_usec <= lasttime.tv_usec &&
            (tvp->tv_usec = lasttime.tv_usec + 1) >= 1000000) {
                tvp->tv_sec++;
                tvp->tv_usec -= 1000000;
        }
        lasttime = *tvp;
        splx(s);
}

/* powerdown */

struct pio {
	volatile u_int8_t portA;
	volatile unsigned : 24;
	volatile u_int8_t portB;
	volatile unsigned : 24;
	volatile u_int8_t portC;
	volatile unsigned : 24;
	volatile u_int8_t cntrl;
	volatile unsigned : 24;
};

#define	PIO1_POWER	0x04

#define	PIO1_ENABLE	0x01
#define	PIO1_DISABLE	0x00

void
powerdown(void) 
{
	struct pio *p1 = (struct pio *)OBIO_PIO1_BASE;

	DELAY(100000);
	p1->cntrl = (PIO1_POWER << 1) | PIO1_DISABLE;
	*(volatile u_int8_t *)&p1->portC;
}

/* Get data from FUSE ROM */

void
get_fuse_rom_data(void)
{
	int i;
	struct fuse_rom_byte *p = (struct fuse_rom_byte *)FUSE_ROM_ADDR;

	for (i = 0; i < FUSE_ROM_BYTES; i++) {
		fuse_rom_data[i] =
		    (char)((((p->h) >> 24) & 0x000000f0) |
		           (((p->l) >> 28) & 0x0000000f));
		p++;                                                                            
	}
}

/* Get data from NVRAM */

void
get_nvram_data(void)
{
	int i, j;
	u_int8_t *page;
	char buf[NVSYMLEN], *data;

	if (machtype == LUNA_88K) {
		data = (char *)(NVRAM_ADDR + 0x80);

		for (i = 0; i < NNVSYM; i++) {
			for (j = 0; j < NVSYMLEN; j++) {
				buf[j] = *data;
				data += 4;
			}
			strlcpy(nvram[i].symbol, buf, sizeof(nvram[i].symbol));

			for (j = 0; j < NVVALLEN; j++) {
				buf[j] = *data;
				data += 4;
			}
			strlcpy(nvram[i].value, buf, sizeof(nvram[i].value));
		}
	} else if (machtype == LUNA_88K2) {
		page = (u_int8_t *)(NVRAM_ADDR_88K2 + 0x20);

		for (i = 0; i < NNVSYM; i++) {
			*page = (u_int8_t)i;

			data = (char *)NVRAM_ADDR_88K2;
			strlcpy(nvram[i].symbol, data, sizeof(nvram[i].symbol));

			data = (char *)(NVRAM_ADDR_88K2 + 0x10);
			strlcpy(nvram[i].value, data, sizeof(nvram[i].value));
		}
	}
}

char *
nvram_by_symbol(symbol)
	char *symbol;
{
	char *value;
	int i;

	value = NULL;

	for (i = 0; i < NNVSYM; i++) {
		if (strncmp(nvram[i].symbol, symbol, NVSYMLEN) == 0) {
			value = nvram[i].value;
			break;
		}
	}

	return value;
}

#define SIGSYS_MAX	501
#define SIGTRAP_MAX	510

#define EMPTY_BR	0xc0000000	/* empty "br" instruction */
#define NO_OP 		0xf4005800	/* "or r0, r0, r0" */

#define BRANCH(FROM, TO) \
	(EMPTY_BR | ((unsigned)(TO) - (unsigned)(FROM)) >> 2)

#define SET_VECTOR(NUM, VALUE) \
	do { \
		vector[NUM].word_one = NO_OP; \
		vector[NUM].word_two = BRANCH(&vector[NUM].word_two, VALUE); \
	} while (0)

/*
 * vector_init(vector, vector_init_list)
 *
 * This routine sets up the m88k vector table for the running processor.
 * It is called with a very little stack, and interrupts disabled,
 * so don't call any other functions!
 */
void
vector_init(m88k_exception_vector_area *vector, unsigned *vector_init_list)
{
	unsigned num;
	unsigned vec;

	for (num = 0; (vec = vector_init_list[num]) != END_OF_VECTOR_LIST;
	    num++) {
		if (vec != UNKNOWN_HANDLER)
			SET_VECTOR(num, vec);
	}

	for (; num <= SIGSYS_MAX; num++)
		SET_VECTOR(num, sigsys);

	for (; num <= SIGTRAP_MAX; num++)
		SET_VECTOR(num, sigtrap);

	SET_VECTOR(450, syscall_handler);
	SET_VECTOR(451, cache_flush_handler);
	SET_VECTOR(504, stepbpt);
	SET_VECTOR(511, userbpt);

	/* GCC will by default produce explicit trap 503 for division by zero */
	SET_VECTOR(503, vector_init_list[T_ZERODIV]);
}

/*
 * return next safe spl to reenable interrupts.
 */
u_int
safe_level(u_int mask, u_int curlevel)
{
	int i;

	for (i = curlevel; i < 8; i++)
		if (!(int_mask_val[i] & mask))
			return i;

	panic("safe_level: no safe level for mask 0x%08x level %d found",
	       mask, curlevel);
	/* NOTREACHED */
}

void
setlevel(unsigned int level)
{
	unsigned int set_value;
	int cpu = cpu_number();

	set_value = int_set_val[level];

#ifdef MULTIPROCESSOR
	if (cpu != master_cpu)
		set_value &= INT_SLAVE_MASK;
#endif

	*int_mask_reg[cpu] = set_value;
	luna88k_curspl[cpu] = level;
}

unsigned
getipl(void)
{
	unsigned int curspl, psr;

	disable_interrupt(psr);
	curspl = luna88k_curspl[cpu_number()];
	set_psr(psr);
	return curspl;
}

unsigned
setipl(unsigned level)
{
	unsigned int curspl, psr;

	disable_interrupt(psr);
	curspl = luna88k_curspl[cpu_number()];
	setlevel(level);

	/*
	 * The flush pipeline is required to make sure the above write gets
	 * through the data pipe and to the hardware; otherwise, the next
	 * bunch of instructions could execute at the wrong spl protection.
	 */
	flush_pipeline();

	set_psr(psr);
	return curspl;
}

unsigned
raiseipl(unsigned level)
{
	unsigned int curspl, psr;

	disable_interrupt(psr);
	curspl = luna88k_curspl[cpu_number()];
	if (curspl < level)
		setlevel(level);

	/*
	 * The flush pipeline is required to make sure the above write gets
	 * through the data pipe and to the hardware; otherwise, the next
	 * bunch of instructions could execute at the wrong spl protection.
	 */
	flush_pipeline();

	set_psr(psr);
	return curspl;
}
