/* $OpenBSD: longrun.c,v 1.5 2003/10/24 09:03:20 grange Exp $ */
/*
 * Copyright (c) 2003 Ted Unangst
 * Copyright (c) 2001 Tamotsu Hattori
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/cpufunc.h>

#include <machine/longrun.h>

static void	longrun_getmode(u_int32_t *, u_int32_t *, u_int32_t *);
static void	longrun_setmode(u_int32_t, u_int32_t, u_int32_t);
int		longrun_sysctl(void *, size_t *, void *, size_t);

int longrun_enabled;

union msrinfo {
	u_int64_t msr;
	uint32_t regs[2];
};

/*
 * Crusoe model specific registers which interest us.
 */
#define MSR_TMx86_LONGRUN       0x80868010
#define MSR_TMx86_LONGRUN_FLAGS 0x80868011

#define LONGRUN_MODE_MASK(x) ((x) & 0x000000007f)
#define LONGRUN_MODE_RESERVED(x) ((x) & 0xffffff80)
#define LONGRUN_MODE_WRITE(x, y) (LONGRUN_MODE_RESERVED(x) | LONGRUN_MODE_MASK(y))

/* 
 * sysctl handler and entry point.  Just call the right function
 */
int longrun_sysctl(void *oldp, size_t *oldlenp, void *newp, size_t newlen)
{
	struct longrun oinfo;
	struct longrun ninfo;
	int error;

	if (!longrun_enabled)
		return (EINVAL);

	if (oldp && *oldlenp < sizeof(oinfo))
		return (ENOMEM);
	*oldlenp = sizeof(oinfo);

	if (newp != NULL) {
		error = copyin(newp, &ninfo, sizeof(ninfo));
		if (error)
			return (error);
		longrun_setmode(ninfo.low, ninfo.high, ninfo.mode);
	}
	
	if (oldp != NULL) {
		memset(&oinfo, 0, sizeof(oinfo));
		longrun_getmode(&oinfo.freq, &oinfo.voltage, &oinfo.percent);
		error = copyout(&oinfo, oldp, sizeof(oinfo));
	}

	return (error);

}

/*
 * These are the instantaneous values used by the CPU.
 * Frequency is self-evident.
 * Voltage is returned in millivolts.
 * Percent is amount of performance window being used, not percentage
 * of top megahertz.  (0 values are typical.)
 */
static void
longrun_getmode(u_int32_t *freq, u_int32_t *voltage, u_int32_t *percent)
{
	u_long eflags;
	u_int32_t regs[4];

	eflags = read_eflags();
	disable_intr();

	cpuid(0x80860007, regs);
	*freq = regs[0];
	*voltage = regs[1];
	*percent = regs[2];

	enable_intr();
	write_eflags(eflags);
}

/*
 * Transmeta documentation says performance window boundries
 * must be between 0 and 100 or a GP0 exception is generated.
 * mode is really only a bit, 0 or 1
 * These values will be rounded by the CPU to within the
 * limits it handles.  Typically, there are about 5 performance
 * levels selectable.
 */
static void 
longrun_setmode(u_int32_t low, u_int32_t high, u_int32_t mode)
{
 	u_long		eflags;
 	union msrinfo	msrinfo;

	if (low > 100 || high > 100 || low > high)
		return;
	if (mode != 0 && mode != 1)
		return;

	eflags = read_eflags();
	disable_intr();
	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN);
	msrinfo.regs[0] = LONGRUN_MODE_WRITE(msrinfo.regs[0], low);
	msrinfo.regs[1] = LONGRUN_MODE_WRITE(msrinfo.regs[1], high);
	wrmsr(MSR_TMx86_LONGRUN, msrinfo.msr);

	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN_FLAGS);
	msrinfo.regs[0] = (msrinfo.regs[0] & ~0x01) | mode;
	wrmsr(MSR_TMx86_LONGRUN_FLAGS, msrinfo.msr);

	enable_intr();
	write_eflags(eflags);
}

