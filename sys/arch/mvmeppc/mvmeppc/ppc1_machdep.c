/*	$OpenBSD: ppc1_machdep.c,v 1.3 2001/11/06 19:53:15 miod Exp $	*/
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
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/powerpc.h>
#include <machine/autoconf.h>
#include <machine/prom.h>
#include <mvmeppc/dev/nvramreg.h>

#include <dev/cons.h>

void PPC1_exit __P((void)) __attribute__((__noreturn__));
void PPC1_boot __P((char *bootspec)) __attribute__((__noreturn__));
void PPC1_mem_regions __P((struct mem_region **memp, struct mem_region **availp));
void PPC1_vmon __P((void));
unsigned char PPC1_nvram_rd __P((unsigned long offset));
void PPC1_nvram_wr __P((unsigned long offset, unsigned char val));
unsigned long PPC1_tps __P((void));

int PPC1_clock_read __P((int *sec, int *min, int *hour, int *day,
								 int *mon, int *yr));
int PPC1_clock_write __P((int sec, int min, int hour, int day,
								  int mon, int yr));

struct firmware ppc1_firmware = {
	PPC1_mem_regions,
	PPC1_exit,
	PPC1_boot,
	PPC1_vmon,
	PPC1_nvram_rd,
	PPC1_nvram_wr,
	PPC1_tps,
	PPC1_clock_read,
	PPC1_clock_write,
	NULL,
	NULL,
#ifdef FW_HAS_PUTC
	mvmeprom_outchar;
#endif
};

#define	PPC1_REGIONS	32
static struct mem_region PPC1mem[PPC1_REGIONS + 1], PPC1avail[PPC1_REGIONS + 3];

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

vm_offset_t
size_memory(void)
{
	volatile unsigned int *look;
	unsigned int *max;
	extern char *end;
	vm_offset_t local_mem, total_mem;
#ifdef USE_BUG
	bugenvrd();	/* read the bug environment */
	local_mem = (vm_offset_t)bug_localmemsize();
#endif 
#define PATTERN   0x5a5a5a5a
#define STRIDE    (4*1024) 	/* 4k at a time */
#define Roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))
	/*
	 * count it up.
	 */
	max = (void*)MAXPHYSMEM;
	for (look = (void*)Roundup(end, STRIDE); look < max;
		 look = (int*)((unsigned)look + STRIDE)) {
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
	look = (unsigned int *)0x02000000;
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
	int phandle, i, j, cnt;
	extern int avail_start;

	bzero(&PPC1mem[0], sizeof(struct mem_region) * PPC1_REGIONS);
	bzero(&PPC1avail[0], sizeof(struct mem_region) * PPC1_REGIONS);
	/*
	 * Get memory.
	 */
	PPC1mem[0].start = 0;
	PPC1mem[0].size = size_memory();

	PPC1avail[0].start = avail_start;
	PPC1avail[0].size = (PPC1mem[0].size - avail_start);

	*memp = PPC1mem;
	*availp = PPC1avail;
}

void
PPC1_vmon()
{
}

void
PPC1_exit()
{
	mvmeprom_return();
	panic ("PPC1_exit returned!");		/* just in case */
	while (1);
}
void
PPC1_boot(bootspec)
char *bootspec;
{
	u_int32_t msr, i = 10000;

	/* set exception prefix high - to the prom */
	msr = ppc_get_msr();
	msr |= PSL_IP;
	ppc_set_msr(msr);

	/* make sure bit 0 (reset) is a 0 */
	outb(0x80000092, inb(0x80000092) & ~1L);
	/* signal a reset to system control port A - soft reset */
	outb(0x80000092, inb(0x92) | 1);

	while ( i != 0 ) i++;
	panic("restart failed\n");
	mvmeprom_return();
	printf ("PPC1_boot returned!");		/* just in case */
	while (1);
}

unsigned char PPC1_nvram_rd(addr)
unsigned long addr;
{
	outb(NVRAM_S0, addr);
	outb(NVRAM_S1, addr>>8);
	return inb(NVRAM_DATA);
}

void PPC1_nvram_wr(addr, val)
unsigned long addr; 
unsigned char val;
{
	outb(NVRAM_S0, addr);
	outb(NVRAM_S1, addr>>8);
	outb(NVRAM_DATA, val);
}

/* Function to get ticks per second. */

unsigned long PPC1_tps(void)
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
	while (1) {
		if (PPC1_nvram_rd(RTC_SECONDS) != sec)
			break;
	}

	start_val = ppc_get_spr(SPR_DEC);

	/* wait until it changes. */
	sec = PPC1_nvram_rd(RTC_SECONDS);
	while (1) {
		if (PPC1_nvram_rd(RTC_SECONDS) != sec)
			break;
	}
	ticks = start_val - ppc_get_spr(SPR_DEC);
	return (ticks);
}

int PPC1_clock_write(int sec, int min, int hour, int day,
							int mon, int yr)
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

int PPC1_clock_read(int *sec, int *min, int *hour, int *day,
						  int *mon, int *yr)
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
	return (1);
}

int
bootcngetc(dev)
dev_t dev;
{
	return (mvmeprom_getchar());
}

void
bootcnputc(dev, c)
dev_t dev;
char c;
{
	if (c == '\n')
		mvmeprom_outchar('\r');
	mvmeprom_outchar(c);
}

