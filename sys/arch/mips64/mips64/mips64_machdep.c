/*	$OpenBSD: mips64_machdep.c,v 1.17 2014/11/16 12:30:58 deraadt Exp $ */

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
#include <mips64/mips_cpu.h>
#include <mips64/mips_opcode.h>

#include <uvm/uvm_extern.h>

#include <mips64/dev/clockvar.h>

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
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	struct cpu_info *ci = curcpu();

	bzero((caddr_t)p->p_md.md_regs, sizeof(struct trap_frame));
	p->p_md.md_regs->sp = stack;
	p->p_md.md_regs->pc = pack->ep_entry & ~3;
	p->p_md.md_regs->t9 = pack->ep_entry & ~3; /* abicall req */
	p->p_md.md_regs->sr = protosr | (idle_mask & SR_INT_MASK);
	p->p_md.md_regs->ic = (idle_mask << 8) & IC_INT_MASK;
#ifndef FPUEMUL
	p->p_md.md_flags &= ~MDP_FPUSED;
#endif
	if (ci->ci_fpuproc == p)
		ci->ci_fpuproc = NULL;
	p->p_md.md_ss_addr = 0;
	p->p_md.md_pc_ctrl = 0;
	p->p_md.md_watch_1 = 0;
	p->p_md.md_watch_2 = 0;

	retval[1] = 0;
}

int
exec_md_map(struct proc *p, struct exec_package *pack)
{
#ifdef FPUEMUL
	int rc;
	vaddr_t va;

	/*
	 * If we are running with FPU instruction emulation, we need
	 * to allocate a special page in the process' address space,
	 * in order to be able to emulate delay slot instructions of
	 * successful conditional branches.
	 */

	va = 0;
	rc = uvm_map(&p->p_vmspace->vm_map, &va, PAGE_SIZE, NULL,
	    UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(PROT_NONE, PROT_MASK, UVM_INH_COPY,
	      UVM_ADV_NORMAL, UVM_FLAG_COPYONW));
	if (rc != 0)
		return rc;
#ifdef DEBUG
	printf("%s: p %p fppgva %p\n", __func__, p, va);
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
#ifdef CPU_R8000
	Mips_InvalidateICache(ci, 0, ci->ci_l1inst.size);
#endif
}

/*
 *	Mips machine independent clock routines.
 */

struct tod_desc sys_tod;
void (*md_startclock)(struct cpu_info *);

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
	struct tod_desc *cd = &sys_tod;
	struct tod_time ct;
	u_int first_cp0, second_cp0, cycles_per_sec;
	int first_sec;

	if (cd->tod_get == NULL)
		return;

	(*cd->tod_get)(cd->tod_cookie, 0, &ct);
	first_sec = ct.sec;

	/* Let the clock tick one second. */
	do {
		first_cp0 = cp0_get_count();
		(*cd->tod_get)(cd->tod_cookie, 0, &ct);
	} while (ct.sec == first_sec);
	first_sec = ct.sec;
	/* Let the clock tick one more second. */
	do {
		second_cp0 = cp0_get_count();
		(*cd->tod_get)(cd->tod_cookie, 0, &ct);
	} while (ct.sec == first_sec);

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

/* XXX switch to kern/clock_subr.c routines */
/*
 * This code is defunct after 2099. Will Unix still be here then??
 */
static short dayyr[12] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

#define	SECMIN	(60)		/* seconds per minute */
#define	SECHOUR	(60*SECMIN)	/* seconds per hour */

#define	YEARDAYS(year)	(((((year) + 1900) % 4) == 0 && \
			 ((((year) + 1900) % 100) != 0 || \
			  (((year) + 1900) % 400) == 0)) ? 366 : 365)

/*
 * Initialize the time of day register, based on the time base which
 * is, e.g. from a filesystem.
 */
void
inittodr(time_t base)
{
	struct timespec ts;
	struct tod_time c;
	struct tod_desc *cd = &sys_tod;
	int days, yr;

	ts.tv_nsec = 0;

	if (base < 35 * SECYR) {
		printf("WARNING: preposterous time in file system");
		/* read the system clock anyway */
		base = 40 * SECYR;	/* 2010 */
	}

	/*
	 * Read RTC chip registers NOTE: Read routines are responsible
	 * for sanity checking clock. Dates after 19991231 should be
	 * returned as year >= 100.
	 */
	if (cd->tod_get) {
		(*cd->tod_get)(cd->tod_cookie, base, &c);
	} else {
		printf("WARNING: No TOD clock, believing file system.\n");
		goto bad;
	}

	days = 0;
	for (yr = 70; yr < c.year; yr++) {
		days += YEARDAYS(yr);
	}

	days += dayyr[c.mon - 1] + c.day - 1;
	if (YEARDAYS(c.year) == 366 && c.mon > 2) {
		days++;
	}

	/* now have days since Jan 1, 1970; the rest is easy... */
	ts.tv_sec = days * SECDAY + c.hour * 3600 + c.min * 60 + c.sec;

	/*
	 * See if we gained/lost time.
	 */
	if (base < ts.tv_sec - 5*SECYR) {
		printf("WARNING: file system time much less than clock time\n");
	} else if (base > ts.tv_sec + 5*SECYR) {
		printf("WARNING: clock time much less than file system time\n");
		printf("WARNING: using file system time\n");
	} else {
		tc_setclock(&ts);
		cd->tod_valid = 1;
		return;
	}

bad:
	ts.tv_sec = base;
	tc_setclock(&ts);
	cd->tod_valid = 1;
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the TOD clock. This is done when the system is halted or
 * when the time is reset by the stime system call.
 */
void
resettodr()
{
	struct tod_time c;
	struct tod_desc *cd = &sys_tod;
	int t, t2;

	/*
	 *  Don't reset TOD if time has not been set!
	 */
	if (!cd->tod_valid)
		return;

	/* compute the day of week. 1 is Sunday*/
	t2 = time_second / SECDAY;
	c.dow = (t2 + 5) % 7 + 1;	/* 1/1/1970 was thursday */

	/* compute the year */
	t = 0;
	t2 = time_second / SECDAY;
	c.year = 69;
	while (t2 >= 0) {	/* whittle off years */
		t = t2;
		c.year++;
		t2 -= YEARDAYS(c.year);
	}

	/* t = month + day; separate */
	t2 = YEARDAYS(c.year);
	for (c.mon = 1; c.mon < 12; c.mon++) {
		if (t < dayyr[c.mon] + (t2 == 366 && c.mon > 1))
			break;
	}

	c.day = t - dayyr[c.mon - 1] + 1;
	if (t2 == 366 && c.mon > 2) {
		c.day--;
	}

	t = time_second % SECDAY;
	c.hour = t / 3600;
	t %= 3600;
	c.min = t / 60;
	c.sec = t % 60;

	if (cd->tod_set)
		(*cd->tod_set)(cd->tod_cookie, &c);
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
