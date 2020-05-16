/*	$OpenBSD: mips64_machdep.c,v 1.29 2020/05/16 03:35:52 visa Exp $ */

/*
 * Copyright (c) 2009, 2010, 2012 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/timetc.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <mips64/cache.h>
#include <mips64/mips_cpu.h>
#include <mips64/mips_opcode.h>

#include <uvm/uvm_extern.h>

#include <mips64/dev/clockvar.h>
#include <dev/clock_subr.h>

/*
 * Build a tlb trampoline
 */
void
build_trampoline(vaddr_t addr, vaddr_t dest)
{
	const uint32_t insns[] = {
		0x3c1a0000,	/* lui k0, imm16 */
		0x675a0000,	/* daddiu k0, k0, imm16 */
		0x001ad438,	/* dsll k0, k0, 0x10 */
		0x675a0000,	/* daddiu k0, k0, imm16 */
		0x001ad438,	/* dsll k0, k0, 0x10 */
		0x675a0000,	/* daddiu k0, k0, imm16 */
		0x03400008,	/* jr k0 */
		0x00000000	/* nop */
	};
	uint32_t *dst = (uint32_t *)addr;
	const uint32_t *src = insns;
	uint32_t a, b, c, d;

	/*
	 * Decompose the handler address in the four components which,
	 * added with sign extension, will produce the correct address.
	 */
	d = dest & 0xffff;
	dest >>= 16;
	if (d & 0x8000)
		dest++;
	c = dest & 0xffff;
	dest >>= 16;
	if (c & 0x8000)
		dest++;
	b = dest & 0xffff;
	dest >>= 16;
	if (b & 0x8000)
		dest++;
	a = dest & 0xffff;

	/*
	 * Build the trampoline, skipping noop computations.
	 */
	*dst++ = *src++ | a;
	if (b != 0)
		*dst++ = *src++ | b;
	else
		src++;
	*dst++ = *src++;
	if (c != 0)
		*dst++ = *src++ | c;
	else
		src++;
	*dst++ = *src++;
	if (d != 0)
		*dst++ = *src++ | d;
	else
		src++;
	*dst++ = *src++;
	*dst++ = *src++;

	/*
	 * Note that we keep the delay slot instruction a nop, instead
	 * of branching to the second instruction of the handler and
	 * having its first instruction in the delay slot, so that the
	 * tlb handler is free to use k0 immediately.
	 */
}

/*
 * Prototype status registers value for userland processes.
 */
register_t protosr = SR_FR_32 | SR_XX | SR_UX | SR_KSU_USER | SR_EXL |
#ifdef CPU_R8000
    SR_SERIALIZE_FPU |
#else
    SR_KX |
#endif
    SR_INT_ENAB;

/*
 * Set registers on exec for native exec format. For o64/64.
 */
void
setregs(struct proc *p, struct exec_package *pack, u_long stack,
    register_t *retval)
{
	struct cpu_info *ci = curcpu();

	bzero((caddr_t)p->p_md.md_regs, sizeof(struct trapframe));
	p->p_md.md_regs->sp = stack;
	p->p_md.md_regs->pc = pack->ep_entry & ~3;
	p->p_md.md_regs->t9 = pack->ep_entry & ~3; /* abicall req */
	p->p_md.md_regs->sr = protosr | (idle_mask & SR_INT_MASK);
	p->p_md.md_regs->ic = (idle_mask << 8) & IC_INT_MASK;
	if (CPU_HAS_FPU(ci))
		p->p_md.md_flags &= ~MDP_FPUSED;
	if (ci->ci_fpuproc == p)
		ci->ci_fpuproc = NULL;

	retval[1] = 0;
}

int
exec_md_map(struct proc *p, struct exec_package *pack)
{
#ifdef FPUEMUL
	struct cpu_info *ci = curcpu();
	vaddr_t va;
	int rc;

	if (CPU_HAS_FPU(ci))
		return 0;

	/*
	 * If we are running with FPU instruction emulation, we need
	 * to allocate a special page in the process' address space,
	 * in order to be able to emulate delay slot instructions of
	 * successful conditional branches.
	 */

	va = 0;
	rc = uvm_map(&p->p_vmspace->vm_map, &va, PAGE_SIZE, NULL,
	    UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(PROT_NONE, PROT_MASK, MAP_INHERIT_COPY,
	      MADV_NORMAL, UVM_FLAG_COPYONW));
	if (rc != 0)
		return rc;
#ifdef DEBUG
	printf("%s: p %p fppgva %p\n", __func__, p, (void *)va);
#endif
	p->p_md.md_fppgva = va;
#endif

	return 0;
}

/*
 * Initial TLB setup for the current processor.
 */
void
tlb_init(unsigned int tlbsize)
{
#ifdef CPU_R8000
	register_t sr;

	sr = getsr();
	sr &= ~(((uint64_t)SR_PGSZ_MASK << SR_KPGSZ_SHIFT) |
	        ((uint64_t)SR_PGSZ_MASK << SR_UPGSZ_SHIFT));
	sr |= ((uint64_t)SR_PGSZ_16K << SR_KPGSZ_SHIFT) |
	    ((uint64_t)SR_PGSZ_16K << SR_UPGSZ_SHIFT);
	protosr |= ((uint64_t)SR_PGSZ_16K << SR_KPGSZ_SHIFT) |
	    ((uint64_t)SR_PGSZ_16K << SR_UPGSZ_SHIFT);
	setsr(sr);
#else
	tlb_set_page_mask(TLB_PAGE_MASK);
#endif
	tlb_set_wired(0);
	tlb_flush(tlbsize);
#if UPAGES > 1
	tlb_set_wired(UPAGES / 2);
#endif
}

/*
 * Handle an ASID wrap.
 */
void
tlb_asid_wrap(struct cpu_info *ci)
{
	tlb_flush(ci->ci_hw.tlbsize);
#if defined(CPU_OCTEON) || defined(CPU_R8000)
	Mips_InvalidateICache(ci, 0, ci->ci_l1inst.size);
#endif
}

/*
 *	Mips machine independent clock routines.
 */

struct tod_desc sys_tod;
void (*md_startclock)(struct cpu_info *);

extern todr_chip_handle_t todr_handle;
struct todr_chip_handle rtc_todr;

int
rtc_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct tod_desc *cd = &sys_tod;
	struct clock_ymdhms dt;
	struct tod_time c;

	KASSERT(cd->tod_get);

	/*
	 * Read RTC chip registers NOTE: Read routines are responsible
	 * for sanity checking clock. Dates after 19991231 should be
	 * returned as year >= 100.
	 */
	(*cd->tod_get)(cd->tod_cookie, tv->tv_sec, &c);

	dt.dt_sec = c.sec;
	dt.dt_min = c.min;
	dt.dt_hour = c.hour;
	dt.dt_day = c.day;
	dt.dt_mon = c.mon;
	dt.dt_year = c.year + 1900;

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;
	return 0;
}

int
rtc_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct tod_desc *cd = &sys_tod;
	struct clock_ymdhms dt;
	struct tod_time c;
	
	KASSERT(cd->tod_set);

	clock_secs_to_ymdhms(tv->tv_sec, &dt);
	
	c.sec = dt.dt_sec;
	c.min = dt.dt_min;
	c.hour = dt.dt_hour;
	c.day = dt.dt_day;
	c.mon = dt.dt_mon;
	c.year = dt.dt_year - 1900;
	c.dow = dt.dt_wday + 1;

	(*cd->tod_set)(cd->tod_cookie, &c);
	return 0;
}

/*
 * Wait "n" microseconds.
 */
void
delay(int n)
{
	int dly;
	int p, c;
	struct cpu_info *ci = curcpu();
	uint32_t delayconst;

	delayconst = ci->ci_delayconst;
	if (delayconst == 0)
		delayconst = bootcpu_hwinfo.clock / CP0_CYCLE_DIVIDER;
	p = cp0_get_count();
	dly = (delayconst / 1000000) * n;
	while (dly > 0) {
		c = cp0_get_count();
		dly -= c - p;
		p = c;
	}
}

#ifndef MULTIPROCESSOR
u_int cp0_get_timecount(struct timecounter *);

struct timecounter cp0_timecounter = {
	cp0_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffffff,		/* counter_mask */
	0,			/* frequency */
	"CP0",			/* name */
	0			/* quality */
};

u_int
cp0_get_timecount(struct timecounter *tc)
{
	return (cp0_get_count());
}
#endif

/*
 * Calibrate cpu internal counter against the TOD clock if available.
 */
void
cp0_calibrate(struct cpu_info *ci)
{
	struct timeval rtctime;
	u_int first_cp0, second_cp0, cycles_per_sec;
	int first_sec;

	if (todr_handle == NULL)
		return;

	if (todr_gettime(todr_handle, &rtctime) != 0)
		return;
	first_sec = rtctime.tv_sec;

	/* Let the clock tick one second. */
	do {
		first_cp0 = cp0_get_count();
		if (todr_gettime(todr_handle, &rtctime) != 0)
			return;
	} while (rtctime.tv_sec == first_sec);
	first_sec = rtctime.tv_sec;
	/* Let the clock tick one more second. */
	do {
		second_cp0 = cp0_get_count();
		if (todr_gettime(todr_handle, &rtctime) != 0)
			return;
	} while (rtctime.tv_sec == first_sec);

	cycles_per_sec = second_cp0 - first_cp0;
	ci->ci_hw.clock = cycles_per_sec * CP0_CYCLE_DIVIDER;
	ci->ci_delayconst = cycles_per_sec;
}

/*
 * Start the real-time and statistics clocks.
 */
void
cpu_initclocks()
{
	struct cpu_info *ci = curcpu();
	struct tod_desc *cd = &sys_tod;

	if (todr_handle == NULL && cd->tod_get) {
		rtc_todr.todr_gettime = rtc_gettime;
		rtc_todr.todr_settime = rtc_settime;
		todr_handle = &rtc_todr;
	}

	profhz = hz;

	tick = 1000000 / hz;	/* number of micro-seconds between interrupts */
	tickadj = 240000 / (60 * hz);		/* can adjust 240ms in 60s */

	cp0_calibrate(ci);

#ifndef MULTIPROCESSOR
	if (cpu_setperf == NULL) {
		cp0_timecounter.tc_frequency =
		    (uint64_t)ci->ci_hw.clock / CP0_CYCLE_DIVIDER;
		tc_init(&cp0_timecounter);
	}
#endif

#ifdef DIAGNOSTIC
	if (md_startclock == NULL)
		panic("no clock");
#endif
	(*md_startclock)(ci);
}

/*
 * We assume newhz is either stathz or profhz, and that neither will
 * change after being set up above.  Could recalculate intervals here
 * but that would be a drag.
 */
void
setstatclockrate(int newhz)
{
}

/*
 * Decode instruction and figure out type.
 */
int
classify_insn(uint32_t insn)
{
	InstFmt	inst;

	inst.word = insn;
	switch (inst.JType.op) {
	case OP_SPECIAL:
		switch (inst.RType.func) {
		case OP_JR:
			return INSNCLASS_BRANCH;
		case OP_JALR:
			return INSNCLASS_CALL;
		}
		break;

	case OP_BCOND:
		switch (inst.IType.rt) {
		case OP_BLTZ:
		case OP_BLTZL:
		case OP_BGEZ:
		case OP_BGEZL:
			return INSNCLASS_BRANCH;
		case OP_BLTZAL:
		case OP_BLTZALL:
		case OP_BGEZAL:
		case OP_BGEZALL:
			return INSNCLASS_CALL;
		}
		break;

	case OP_JAL:
		return INSNCLASS_CALL;

	case OP_J:
	case OP_BEQ:
	case OP_BEQL:
	case OP_BNE:
	case OP_BNEL:
	case OP_BLEZ:
	case OP_BLEZL:
	case OP_BGTZ:
	case OP_BGTZL:
		return INSNCLASS_BRANCH;

	case OP_COP1:
		switch (inst.RType.rs) {
		case OP_BC:
			return INSNCLASS_BRANCH;
		}
		break;
	}

	return INSNCLASS_NEUTRAL;
}

/*
 * Smash the startup code. There is no way to really unmap it
 * because the kernel runs in the kseg0 or xkphys space.
 */
void
unmap_startup(void)
{
	extern uint32_t kernel_text[], endboot[];
	uint32_t *word = kernel_text;

	while (word < endboot)
		*word++ = 0x00000034u;	/* TEQ zero, zero */
}


todr_chip_handle_t todr_handle;

#define MINYEAR		((OpenBSD / 100) - 1)	/* minimum plausible year */

/*
 * inittodr:
 *
 *      Initialize time from the time-of-day register.
 */
void
inittodr(time_t base)
{
	time_t deltat;
	struct timeval rtctime;
	struct timespec ts;
	int badbase;

	if (base < (MINYEAR - 1970) * SECYR) {
		printf("WARNING: preposterous time in file system\n");
		/* read the system clock anyway */
		base = (MINYEAR - 1970) * SECYR;
		badbase = 1;
	} else
		badbase = 0;

	rtctime.tv_sec = base;
	rtctime.tv_usec = 0;

	if (todr_handle == NULL ||
	    todr_gettime(todr_handle, &rtctime) != 0 ||
	    rtctime.tv_sec < (MINYEAR - 1970) * SECYR) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		rtctime.tv_sec = base;
		rtctime.tv_usec = 0;
		if (todr_handle != NULL && !badbase)
			printf("WARNING: bad clock chip time\n");
		ts.tv_sec = rtctime.tv_sec;
		ts.tv_nsec = rtctime.tv_usec * 1000;
		tc_setclock(&ts);
		goto bad;
	} else {
		ts.tv_sec = rtctime.tv_sec;
		ts.tv_nsec = rtctime.tv_usec * 1000;
		tc_setclock(&ts);
	}

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days; if
		 * so, assume something is amiss.
		 */
		deltat = rtctime.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;         /* all is well */
#ifndef SMALL_KERNEL
		printf("WARNING: clock %s %lld days\n",
		    rtctime.tv_sec < base ? "lost" : "gained",
		    (long long)(deltat / SECDAY));
#endif
	}
 bad:
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * resettodr:
 *
 *      Reset the time-of-day register with the current time.
 */
void
resettodr(void)
{
	struct timeval rtctime;

	if (time_second == 1)
		return;

	microtime(&rtctime);

	if (todr_handle != NULL &&
	    todr_settime(todr_handle, &rtctime) != 0)
		printf("WARNING: can't update clock chip time\n");
}
