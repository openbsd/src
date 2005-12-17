/*	$OpenBSD: ppc1_machdep.c,v 1.16 2005/12/17 07:31:26 miod Exp $	*/
/*	$NetBSD: ofw_machdep.c,v 1.1 1996/09/30 16:34:50 ws Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/extent.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/powerpc.h>
#include <machine/autoconf.h>
#include <machine/bugio.h>

#include <mvmeppc/dev/nvramreg.h>
#include <mvmeppc/dev/ravenreg.h>

#include <dev/cons.h>

void PPC1_exit(void) __attribute__((__noreturn__));
void PPC1_boot(char *bootspec) __attribute__((__noreturn__));
void PPC1_mem_regions(struct mem_region **memp, struct mem_region **availp);
void PPC1_vmon(void);

unsigned char PPC1_nvram_rd(unsigned long offset);
void PPC1_nvram_wr(unsigned long offset, unsigned char val);
unsigned long PPC1_tps(void);

int PPC1_clock_read(int *sec, int *min, int *hour, int *day, int *mon, int *yr);
int PPC1_clock_write(int sec, int min, int hour, int day, int mon, int yr);

vsize_t size_memory(void);

struct firmware ppc1_firmware = {
	PPC1_mem_regions,
	PPC1_exit,
	PPC1_boot,
	PPC1_vmon,
#ifdef FW_HAS_PUTC
	mvmeprom_outchar,
#endif
};

#define	PPC1_REGIONS	32
struct mem_region PPC1mem[PPC1_REGIONS + 1], PPC1avail[PPC1_REGIONS + 3];

/*
 * 1 - Figure and find the end of local memory. This is now a Bug call.  
 *     This requires that the correct amount of local memory be entered 
 *     in the Bug environment.  (see: Bug ENV command.)
 * 2 - Start looking from the megabyte after the end of the kernel data,
 *     until we find non-memory to figure the total memory available.
 * 3 - Initialize from the end of local memory to the end of total 
 *     memory.  (As required by some VME memory boards) - smurph
 */
#define MAXPHYSMEM	0x10000000	/* max physical memory */

vsize_t
size_memory(void)
{
	volatile unsigned int *look;
	unsigned int *max;
	extern char *end;
	vsize_t total_mem;
#ifdef USE_BUG
	vsize_t local_mem;
#endif

#ifdef USE_BUG
	bugenvrd();	/* read the bug environment */
	local_mem = (vsize_t)bug_localmemsize();
#endif 
#define PATTERN   0x5a5a5a5a
#define STRIDE    (4*1024) 	/* 4k at a time */
#define Roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))
	/*
	 * count it up.
	 */
	max = (void *)MAXPHYSMEM;
	for (look = (void *)Roundup(&end, STRIDE); look < max;
		 look = (int *)((unsigned)look + STRIDE)) {
		unsigned save;

		/* if can't access, we've reached the end */
		if (badaddr((char *)look, 4)) {
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
	physmem = btoc(trunc_page((unsigned)look)); /* in pages */
	total_mem = trunc_page((unsigned)look);
#ifdef USE_BUG
	/* Initialize the off-board (non-local) memory. */
	printf("Initializing %d bytes of off-board memory.\n", total_mem - local_mem);
	bzero((void *)local_mem, total_mem - local_mem);
#endif
	return (total_mem);
}

/*
 * This is called during initppc, before the system is really initialized.
 * It shall provide the total and the available regions of RAM.
 * Both lists must have a zero-size entry as terminator.
 * The available regions need not take the kernel into account, but needs
 * to provide space for two additional entry beyond the terminating one.
 */
void
PPC1_mem_regions(memp, availp)
	struct mem_region **memp, **availp;
{
	extern char *start;

	/*
	 * Get memory.
	 */
	PPC1mem[0].start = 0;
	PPC1mem[0].size = size_memory();

	/*
	 * PPC1Bug manual states that the BUG uses ``about 768KB'' at the
	 * top at the physical memory, but then only shows 512KB in the
	 * example!
	 * Reserve 1MB to be on the safe side.
	 *
	 * We also need to reserve space below kernelstart, if only because
	 * the trap vectors lie there.
	 */
	PPC1avail[0].start = (u_long)&start;
	PPC1avail[0].size = PPC1mem[0].size - 1024 * 1024 - PPC1avail[0].start;

	*memp = PPC1mem;
	*availp = PPC1avail;
}

void
PPC1_vmon()
{
	/*
	 * Now is a good time to setup the clock callbacks, though this
	 * could have been done earlier...
	 */
	clock_read = PPC1_clock_read;
	clock_write = PPC1_clock_write;
	tps = PPC1_tps;
}

void
PPC1_exit()
{
	mvmeprom_return();
	for (;;) ;
}

void
PPC1_boot(bootspec)
	char *bootspec;
{
	u_int32_t msr, i = 10000;

	/* set exception prefix high - to the prom */
	msr = ppc_mfmsr();
	msr |= PSL_IP;
	ppc_mtmsr(msr);

	/* make sure bit 0 (reset) is a 0 */
	outb(0x80000092, inb(0x80000092) & ~1L);
	/* signal a reset to system control port A - soft reset */
	outb(0x80000092, inb(0x92) | 1);

	while (i != 0) i++;
	panic("restart failed");
	mvmeprom_return();
	printf("PPC1_boot returned!");		/* just in case */
	for (;;) ;
}

/*
 * Clock and NVRAM functions
 *
 * This needs to become a real device, but it needs to be mapped early
 * because we need to setup the clocks before autoconf.
 */

vaddr_t	isaspace_va;

void
nvram_map()
{
	int error;
	extern struct extent *devio_ex;
	extern int ppc_malloc_ok;

	if ((error = extent_alloc_region(devio_ex, RAVEN_P_ISA_IO_SPACE,
	    ISA_SIZE, EX_NOWAIT | (ppc_malloc_ok ? EX_MALLOCOK : 0))) != 0)
		panic("nvram_map: can't map ISA space, extent error %d", error);

	if ((isaspace_va = (vaddr_t)mapiodev(RAVEN_P_ISA_IO_SPACE,
	    ISA_SIZE)) == NULL)
		panic("nvram_map: map failed");
}

unsigned char
PPC1_nvram_rd(addr)
	unsigned long addr;
{
	outb(isaspace_va + NVRAM_S0, addr);
	outb(isaspace_va + NVRAM_S1, addr>>8);
	return inb(isaspace_va + NVRAM_DATA);
}

void
PPC1_nvram_wr(addr, val)
	unsigned long addr; 
	unsigned char val;
{
	outb(isaspace_va + NVRAM_S0, addr);
	outb(isaspace_va + NVRAM_S1, addr>>8);
	outb(isaspace_va + NVRAM_DATA, val);
}

/* Function to get ticks per second. */
unsigned long
PPC1_tps()
{
	unsigned long start_val, ticks;
	unsigned char val, sec;

	/* Start RTC */
	val = PPC1_nvram_rd(RTC_CONTROLB);
	PPC1_nvram_wr(RTC_CONTROLA, (val & (~RTC_CB_STOP)));
	val = PPC1_nvram_rd(RTC_CONTROLA);
	PPC1_nvram_wr(RTC_CONTROLA, (val & (~RTC_CA_READ)));

	/* look at seconds. */
	sec = PPC1_nvram_rd(RTC_SECONDS);
	for (;;) {
		if (PPC1_nvram_rd(RTC_SECONDS) != sec)
			break;
	}

	start_val = ppc_mfdec();

	/* wait until it changes. */
	sec = PPC1_nvram_rd(RTC_SECONDS);
	for (;;) {
		if (PPC1_nvram_rd(RTC_SECONDS) != sec)
			break;
	}
	ticks = start_val - ppc_mfdec();
	return (ticks);
}

int
PPC1_clock_write(int sec, int min, int hour, int day, int mon, int yr)
{
	unsigned char val;

	/* write command */
	val = PPC1_nvram_rd(RTC_CONTROLA);
	PPC1_nvram_wr(RTC_CONTROLA, (val | RTC_CA_WRITE));

	PPC1_nvram_wr(RTC_SECONDS, sec);
	PPC1_nvram_wr(RTC_MINUTES, min);
	PPC1_nvram_wr(RTC_HOURS, hour);
	PPC1_nvram_wr(RTC_MONTH, mon);
	PPC1_nvram_wr(RTC_DAY_OF_MONTH, day);
	PPC1_nvram_wr(RTC_YEAR, yr);

	/* cancel write */
	PPC1_nvram_wr(RTC_CONTROLA, val);

	return 0;
}

int
PPC1_clock_read(int *sec, int *min, int *hour, int *day, int *mon, int *yr)
{
	unsigned char val;
	int i;

	/* Is there time? */
	val = PPC1_nvram_rd(RTC_CONTROLB);
	PPC1_nvram_wr(RTC_CONTROLA, (val & (~RTC_CB_STOP)));
	val = PPC1_nvram_rd(RTC_CONTROLA);
	val &= ~RTC_CA_READ;
	PPC1_nvram_wr(RTC_CONTROLA, val);

	/* Read the seconds value. */
	*sec = PPC1_nvram_rd(RTC_SECONDS);

	/* Wait for a new second. */
	for (i = 0 ; i < 1000000 ; i++) { 
		if (PPC1_nvram_rd(RTC_SECONDS) != *sec) {
			break;
		}
	}

	/* stop time. */
	PPC1_nvram_wr(RTC_CONTROLA, (val | RTC_CA_READ));

	*sec = PPC1_nvram_rd(RTC_SECONDS);
	*min = PPC1_nvram_rd(RTC_MINUTES);
	*hour = PPC1_nvram_rd(RTC_HOURS);
	*day = PPC1_nvram_rd(RTC_DAY_OF_MONTH);
	*mon = PPC1_nvram_rd(RTC_MONTH);
	*yr = PPC1_nvram_rd(RTC_YEAR);

	/* restart time. */
	PPC1_nvram_wr(RTC_CONTROLA, val);
	return 0;
}

/*
 * Boot console routines: 
 * Enables printing of boot messages before consinit().
 */
cons_decl(boot);

void
bootcnprobe(struct consdev *cp)
{
	cp->cn_dev = makedev(14, 0);
	cp->cn_pri = CN_NORMAL;
}

void
bootcninit(struct consdev *cp)
{
	/* Nothing to do */
}

int
bootcngetc(dev_t dev)
{
	return (mvmeprom_getchar());
}

void
bootcnputc(dev_t dev, int c)
{
	mvmeprom_outchar(c);
}
