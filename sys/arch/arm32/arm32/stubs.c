/* $NetBSD: stubs.c,v 1.4 1996/03/08 18:41:52 mark Exp $ */

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * stubs.c
 *
 * Routines that are temporary or do not have a home yet.
 *
 * Created      : 17/09/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/conf.h> 
#include <sys/reboot.h> 
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <dev/ramdisk.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>
#include <machine/irqhandler.h>
#include <machine/vidc.h>
#include <machine/bootconfig.h>
#include <machine/katelib.h>
#include <machine/psl.h>

#include "fdc.h" 
#include "rd.h" 

#define VM_MAXKERN_ADDRESS 0xf3000000

extern int ffs_mountroot();
extern int cd9660_mountroot();
int do_mountroot();
int (*mountroot)() = do_mountroot;

extern u_int soft_interrupts;

extern int msgbufmapped;
extern dev_t rootdev;
extern dev_t dumpdev;
#ifdef RAMDISK_HOOKS
extern struct rd_conf *bootrd;
#endif

extern BootConfig bootconfig;
extern videomemory_t videomemory;

int
do_mountroot()
{
	struct buf *bp;
	int loop;
	int s;
	int type;
	int floppysize;
	int error;

#if (NFDC > 0 && NRD > 0 && defined(RAMDISK_HOOKS))

/*
 * Ok ideally the ramdisc would be loaded via the rd_open_hook() but since
 * we are loading the ramdisc from floppy we only want to load it during
 * the boot and not at any other time.
 */

/*
 * Ok bit of bodging here. The ramdisc minor is the unit number. However if booting
 * from the ramdisc we limit to always booting off minor 0 i.e. rd0 The ramdisc
 * device passed as the root device is only used to identify the ramdisc major. The
 * minor, instead of indicating the ramdisc unit is used to indicate the floppy
 * minor that should be used for loading the boot ramdisc which is unit 0.
 */

	if (major(rootdev) == 18 && bootrd) {
		if (load_ramdisc_from_floppy(bootrd, makedev(17, minor(rootdev))) != 0)
			panic("Failed to load ramdisc");
		boothowto |= RB_SINGLE;
		rootdev = makedev(major(rootdev), 0);
	}
#endif

/*
 * Slight bug with mounting CD's sometimes. The first mount may fail
 * so we will try again. This only happens with the temporary ATAPI
 * CDROM driver we are using
 */

#ifdef CD9660
	if (major(rootdev) == 20 || major(rootdev) == 26) {
		error = cd9660_mountroot();
		if (error)
			error = cd9660_mountroot();
	}
	else {
#endif
#ifdef FFS
		error = ffs_mountroot();
#else
#error	FFS not configured
#endif
#ifdef CD9660
		if (error)
			error = cd9660_mountroot();
	}
#endif
	return(error);
} 


/*
 * All the copyin and copyout and copystr functions need to be recoded in assembly
 * at some point. The guts of the functions now use an assembly bcopy routine but
 * it would be nice to code them completely in assembly. The main reason they have
 * not been is purely because it is easier to add debugging code to check the
 * parameters in C.
 */

#if 0
int
copystr(from, to, maxlen, lencopied)
	void *from;
	void *to;
	size_t maxlen;
	size_t *lencopied;
{
	int byte = 0;
	int len;
    
	len = 0;

	do {
		if (len == maxlen)
			break;
		byte = *((caddr_t)from)++;
		*((caddr_t)to)++ = byte;
		++len;
	} while (byte != 0);

	if (lencopied)
		*lencopied = len;
     
	if (byte == 0)
		return(0);
	else
		return(ENAMETOOLONG);
}


#else
int
copystr(from, to, maxlen, lencopied)
	void *from;
	void *to;
	size_t maxlen;
	size_t *lencopied;

{
	return(copystrinout(from, to, maxlen, lencopied));
}
#endif

int copystrinout __P((void */*from*/, void */*to*/, size_t /*maxlen*/, size_t */*lencopied*/));


int
copyinstr(udaddr, kaddr, len, done)
	void *udaddr;
	void *kaddr;
	u_int len;
	u_int *done;
{
	if (udaddr < (void *)VM_MIN_ADDRESS
	    || udaddr >= (void *)VM_MAXUSER_ADDRESS) {
		printf("akt: copyinstr: udaddr=%08x kaddr=%08x\n",
		    (u_int)udaddr, (u_int)kaddr);
		return(EFAULT);
	}
	if (kaddr < (void *)VM_MAXUSER_ADDRESS
	    || kaddr >= (void *)VM_MAXKERN_ADDRESS) {
		printf("akt: copyinstr: udaddr=%08x kaddr=%08x\n",
		    (u_int)udaddr, (u_int)kaddr);
		return(EFAULT);
	}
	return(copystrinout(udaddr, kaddr, len, done));
}

int
copyoutstr(kaddr, udaddr, len, done)
	void *kaddr;
	void *udaddr;
	u_int len;
	u_int *done;
{
	if (udaddr < (void *)VM_MIN_ADDRESS
	    || udaddr >= (void*)VM_MAXUSER_ADDRESS) {
		printf("akt: copyoutstr: udaddr=%08x kaddr=%08x\n",
		    (u_int)udaddr, (u_int)kaddr);
		return(EFAULT);
	}
	if (kaddr < (void *)VM_MAXUSER_ADDRESS
	    || kaddr >= (void *)VM_MAXKERN_ADDRESS) {
		printf("akt: copyoutstr: udaddr=%08x kaddr=%08x\n",
		    (u_int)udaddr, (u_int)kaddr);
		return(EFAULT);
	}
	return(copystrinout(kaddr, udaddr, len, done));
}

int bcopyinout __P((void *, void *, u_int));
  
int
copyin(udaddr, kaddr, len)
	void *udaddr;
	void *kaddr;
	u_int len;
{
	int error;
    
	if (udaddr < (void *)VM_MIN_ADDRESS
	    || udaddr >= (void *)VM_MAXUSER_ADDRESS) {
		printf("akt: copyin: udaddr=%08x kaddr=%08x\n",
		    (u_int)udaddr, (u_int)kaddr);
		return(EFAULT);
	}
	if (kaddr < (void *)VM_MAXUSER_ADDRESS
	    || kaddr >= (void *)VM_MAXKERN_ADDRESS) {
		printf("akt: copyin: udaddr=%08x kaddr=%08x\n",
		    (u_int)udaddr, (u_int)kaddr);
		return(EFAULT);
	}
	error = bcopyinout(udaddr, kaddr, len);
	if (error)
		printf("akt: copyin(%08x,%08x,%08x) failed\n",
		    (u_int)udaddr, (u_int)kaddr, len);
	return(error);
}
  
int
copyout(kaddr, udaddr, len)
	void *kaddr;
	void *udaddr;
	u_int len;
{
	int error;
    
	if (udaddr < (void*)VM_MIN_ADDRESS
	    || udaddr >= (void*)VM_MAXUSER_ADDRESS) {
		printf("akt: copyout: udaddr=%08x kaddr=%08x\n",
		    (u_int)udaddr, (u_int)kaddr);
		return(EFAULT);
	}
	if (kaddr < (void *)VM_MAXUSER_ADDRESS
	    || kaddr >= (void *)VM_MAXKERN_ADDRESS) {
		printf("akt: copyout: udaddr=%08x kaddr=%08x\n",
		    (u_int)udaddr, (u_int)kaddr);
		return(EFAULT);
	}
	error = bcopyinout(kaddr, udaddr, len);
	if (error) {
		printf("akt: copyout(%08x,%08x,%08x) failed\n",
		    (u_int)kaddr, (u_int)udaddr, len);
		traceback();
	}
	return(error);
}

void
setsoftnet()
{
	soft_interrupts |= IRQMASK_SOFTNET;
}

int astpending;

void
setsoftast()
{
	astpending = 1;
}

extern int want_resched;

void
need_resched(void)
{
	want_resched = 1;
	setsoftast();
}


struct queue {
	struct queue *q_next, *q_prev;
};

/*
 * insert an element into a queue
 */

void
_insque(v1, v2)
	void *v1;
	void *v2;
{
	register struct queue *elem = v1, *head = v2;
	register struct queue *next;

	next = head->q_next;
	elem->q_next = next;
	head->q_next = elem;
	elem->q_prev = head;
	next->q_prev = elem;
}

/*
 * remove an element from a queue
 */

void
_remque(v)
	void *v;
{
	register struct queue *elem = v;
	register struct queue *next, *prev;

	next = elem->q_next;
	prev = elem->q_prev;
	next->q_prev = prev;
	prev->q_next = next;
	elem->q_prev = 0;
}



/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int 	dumpsize = 0;		/* pages */
long	dumplo = 0; 		/* blocks */

/*
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
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

	/* Always skip the first CLBYTES, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}


extern pagehook_t page_hook0;

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */

void
dumpsys()
{
	daddr_t blkno;
	int psize;
	int error;
	int addr;
	int block;
	vm_offset_t dumpspace;

	msgbufmapped = 0;
	if (dumpdev == NODEV)
		return;
	if (dumpsize == 0) {
		dumpconf();
		if (dumpsize == 0)
			return;
	}
	printf("\ndumping to dev %x, offset %d\n", (u_int)dumpdev,
	    (u_int)dumplo);

	blkno = dumplo;
	dumpspace = page_hook0.va;

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

	error = 0;
	for (block = 0; block < bootconfig.dramblocks && error == 0; ++block) {
		for (addr = bootconfig.dram[block].address;
		    addr < bootconfig.dram[block].address
		    + bootconfig.dram[block].pages * NBPG; addr += NBPG) {
	                pmap_map(dumpspace, addr, addr + NBPG, VM_PROT_READ);
			error = (*bdevsw[major(dumpdev)].d_dump)(dumpdev, blkno, (caddr_t) dumpspace, NBPG);
			if (error) break;
			blkno += btodb(NBPG);
		}
	}

	if (error == 0 && videomemory.vidm_type == VIDEOMEM_TYPE_VRAM) {
		for (addr = videomemory.vidm_pbase; addr < videomemory.vidm_pbase
		    + videomemory.vidm_size; addr += NBPG) {
     	           pmap_map(dumpspace, addr, addr + NBPG, VM_PROT_READ);
	                error = (*bdevsw[major(dumpdev)].d_dump)(dumpdev, blkno, (caddr_t) dumpspace, NBPG);
	                if (error) break;
			blkno += btodb(NBPG);
		}
	}                         

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

	case EINTR:
		printf("aborted from console\n");
		break;

	default:
		printf("succeeded\n");
		break;
	}
	printf("\n\n");
	delay(1000000);
}

/*
 * Dummy function is case no audio device has been configured
 * Need to fix the code that uses this function (console) to check NBEEP.
 */

#include "beep.h"
#if NBEEP == 0
void
beep_generate()
{
}
#endif


#if 0
/* Debugging functions to dump the buffers linked to a vnode */

void
dumpvndbuf(vp)
	register struct vnode *vp;
{
	register struct buf *bp, *nbp;
	int s;

	s = splbio();
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;

		printf("buf=%08x\n", bp);
		printf("flags=%08x proc=%08x bufsize=%08x dev=%04x\n", bp->b_flags, bp->b_proc, bp->b_bufsize, bp->b_dev);
		printf("vp=%08x resid=%08x count=%08x addr=%08x\n", bp->b_vp, bp->b_resid, bp->b_bcount, bp->b_un.b_addr);
	}
	(void)splx(s);
}


void
dumpvncbuf(vp)
	register struct vnode *vp;
{
	register struct buf *bp, *nbp;
	int s;

	s = splbio();
	for (bp = vp->v_cleanblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;

		printf("buf=%08x\n", bp);
		printf("flags=%08x proc=%08x bufsize=%08x dev=%04x\n", bp->b_flags, bp->b_proc, bp->b_bufsize, bp->b_dev);
		printf("vp=%08x resid=%08x count=%08x addr=%08x\n", bp->b_vp, bp->b_resid, bp->b_bcount, bp->b_un.b_addr);
	}
	(void)splx(s);
}
#endif


extern u_int spl_mask;

int current_spl_level = SPL_0;
u_int spl_masks[8];

int safepri = SPL_0;

void
set_spl_masks()
{
	spl_masks[SPL_0]	= 0xffffffff;
	spl_masks[SPL_SOFT]	= ~IRQMASK_SOFTNET;
	spl_masks[SPL_BIO]	= irqmasks[IPL_BIO];
	spl_masks[SPL_NET]	= irqmasks[IPL_NET];
	spl_masks[SPL_TTY]	= irqmasks[IPL_TTY];
	spl_masks[SPL_CLOCK]	= irqmasks[IPL_CLOCK];
	spl_masks[SPL_IMP]	= irqmasks[IPL_IMP];
	spl_masks[SPL_HIGH]	= 0x00000000;
}

void
dump_spl_masks()
{
	printf("spl0=%08x splsoft=%08x splbio=%08x splnet=%08x\n",
	    spl_masks[SPL_0], spl_masks[SPL_SOFT], spl_masks[SPL_BIO], spl_masks[SPL_NET]);
	printf("spltty=%08x splclock=%08x splimp=%08x splhigh=%08x\n",
	    spl_masks[SPL_TTY], spl_masks[SPL_CLOCK], spl_masks[SPL_IMP], spl_masks[SPL_HIGH]);
}

/*
 * Ok things are broken here. If we lower the spl level to SPL_SOFT
 * then, for the most things work. However wd interrupts start to get
 * lost ... i.e. you either get wdc interrupt lost messages or
 * wdc timeout messages.
 * The fault is the CLKF_FRAME macro uses in kern_clock.c. This
 * currently always returns 1 thus splsoftclock() is always
 * called before calling softclock().
 *
 * This is about to be fixed
 */

int
splsoftclock()
{
/*	return(lowerspl(SPL_SOFT));*/
	return(current_spl_level);
}

/* End of stubs.c */
