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
 */

/*
 *	Basic initialization for vme187.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/reboot.h>
#include <sys/exec.h>
#include <vm/pmap.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>
#include <machine/bug.h>

#define INITIAL_MHZ_GUESS 25.0

struct bugenv bugargs;
struct kernel{
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

int boothowto;	/* read in kern/bootstrap */
int machineid;

#ifndef roundup
#define roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))
#endif /* roundup */

vm_size_t	mem_size;
vm_size_t	rawmem_size;
vm_offset_t	first_addr = 0;
vm_offset_t	last_addr = 0;

vm_offset_t	avail_start, avail_next, avail_end;
vm_offset_t	virtual_avail, virtual_end;

void		*end_loaded;
int		bootdev;
int		no_symbols;
vm_offset_t	miniroot;

struct proc	*lastproc;
pcb_t	curpcb;

void	cmmu_init(void);

double cycles_per_microsecond = INITIAL_MHZ_GUESS;

extern struct user *proc0paddr;

int	bcd2int	__P((unsigned int));

/*
 * Called from locore.S during boot,
 * this is the first C code that's run.
 */

void
m187_bootstrap(void)
{
    extern char version[];
    extern char *edata, *end;
    extern int cold;
    extern int kernelstart;
    extern vm_offset_t size_memory(void);
    struct bugbrdid brdid;

    cold = 1;	/* we are still booting */

    bugbrdid(&brdid);
    machineid = brdid.brdno;

    vm_set_page_size();

#if 0
    esym  = kflags.esym;
    boothowto = kflags.bflags;
    bootdev = kflags.bdev;
#endif /* 0 */
    
#if 0
    end_loaded = kflags.end_load;
    if (esym != NULL) {
    	end = (char *)((int)(kflags.symtab));
    } else {
    	first_addr = (vm_offset_t)&end;
    }
#endif

    first_addr = m88k_round_page(first_addr);

    if (!no_symbols)
	boothowto |= RB_KDB;

    printf("about to probe\n");
#if 1
    last_addr = size_memory();
#else
    last_addr = (vm_offset_t)0x01000000;
    physmem = btoc(last_addr);
#endif

    printf("probing done\n");
    cmmu_init();

    avail_start = first_addr;
    avail_end = last_addr;
    printf("%s",version);
    printf("M187 boot: memory from 0x%x to 0x%x\n", avail_start, avail_end);

    /*
     * Steal one page at the top of physical memory for msgbuf
     */

    avail_end -= PAGE_SIZE;

    pmap_bootstrap((vm_offset_t)&kernelstart - GOOFYLDOFFSET /* loadpt */, 
		   &avail_start, &avail_end, &virtual_avail,
		   &virtual_end);
    printf("returned from pmap_bootstrap\n");

    /*
     * Must initialize p_addr before autoconfig or
     * the fault handler will get a NULL reference.
     */
    proc0.p_addr = proc0paddr;
    curproc = &proc0;
    curpcb = &proc0paddr->u_pcb;

    /* Initialize cached PTEs for u-area mapping. */
    save_u_area(&proc0, proc0paddr);

    /*
     * Map proc0's u-area at the standard address (UADDR).
     */
    load_u_area(&proc0);

    /* Initialize the "u-area" pages. */
    bzero((caddr_t)UADDR, UPAGES*NBPG);
    printf("returning from init\n");
}

#ifdef notneeded
ipow(int base, int i)
{
	int cnt = 1;
	while (i--) {
		cnt *= base;
	}
	return cnt;	
}

int
bcd2int(unsigned int i)
{
	unsigned val = 0;
	int	cnt = 0;
	while (i) {
		val += (i&0xf) * ipow(10,cnt);
		cnt++;
		i >>= 4;
	}
	return val;
}
#endif /* notneeded */
