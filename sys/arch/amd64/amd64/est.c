/*	$OpenBSD: est.c,v 1.2 2007/06/01 22:28:21 tedu Exp $ */
/*
 * Copyright (c) 2003 Michael Eriksson.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * This is a driver for Intel's Enhanced SpeedStep, as implemented in
 * Pentium M processors.
 *
 * Reference documentation:
 *
 * - IA-32 Intel Architecture Software Developer's Manual, Volume 3:
 *   System Programming Guide.
 *   Section 13.14, Enhanced Intel SpeedStep technology.
 *   Table B-2, MSRs in Pentium M Processors.
 *   http://www.intel.com/design/pentium4/manuals/245472.htm
 *
 * - Intel Pentium M Processor Datasheet.
 *   Table 5, Voltage and Current Specifications.
 *   http://www.intel.com/design/mobile/datashts/252612.htm
 *
 * - Intel Pentium M Processor on 90 nm Process with 2-MB L2 Cache Datasheet
 *   Table 3-4, Voltage and Current Specifications.
 *   http://www.intel.com/design/mobile/datashts/302189.htm
 *
 * - Linux cpufreq patches, speedstep-centrino.c.
 *   Encoding of MSR_PERF_CTL and MSR_PERF_STATUS.
 *   http://www.codemonkey.org.uk/projects/cpufreq/cpufreq-2.4.22-pre6-1.gz
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#define CPUVENDOR_INTEL 0
#define CPUVENDOR_VIA 8


/* Convert MHz and mV into IDs for passing to the MSR. */
#define ID16(MHz, mV, bus_clk) \
	((((MHz * 100 + 50) / bus_clk) << 8) | ((mV ? mV - 700 : 0) >> 4))

/* Possible bus speeds (multiplied by 100 for rounding) */
#define BUS100 10000
#define BUS133 13333
#define BUS166 16666
#define BUS200 20000

#define MSR2MHZ(msr, bus) \
	(((((int) (msr) >> 8) & 0xff) * (bus) + 50) / 100)
#define MSR2MV(msr) \
	(((int) (msr) & 0xff) * 16 + 700)

struct fqlist {
	int vendor: 5;
	unsigned bus_clk : 1;
	unsigned n : 5;
	const u_int16_t *table;
};

static const struct fqlist *est_fqlist;

static u_int16_t fake_table[3];
static struct fqlist fake_fqlist;

extern int setperf_prio;
extern int perflevel;

int bus_clock;

void p4_get_bus_clock(struct cpu_info *);
void p3_get_bus_clock(struct cpu_info *);

void
p4_get_bus_clock(struct cpu_info *ci)
{
	u_int64_t msr;
	int model, bus;

	model = (ci->ci_signature >> 4) & 15;
	msr = rdmsr(MSR_EBC_FREQUENCY_ID);
	if (model < 2) {
		bus = (msr >> 21) & 0x7;
		switch (bus) {
		case 0:
			bus_clock = 10000;
			break;
		case 1:
			bus_clock = 13333;
			break;
		default:
			printf("%s: unknown Pentium 4 (model %d) "
			    "EBC_FREQUENCY_ID value %d\n",
			    ci->ci_dev->dv_xname, model, bus);
			break;
		}
	} else {
		bus = (msr >> 16) & 0x7;
		switch (bus) {
		case 0:
			bus_clock = (model == 2) ? 10000 : 26666;
			break;
		case 1:
			bus_clock = 13333;
			break;
		case 2:
			bus_clock = 20000;
			break;
		case 3:
			bus_clock = 16666;
			break;
		default:
			printf("%s: unknown Pentium 4 (model %d) "
			    "EBC_FREQUENCY_ID value %d\n",
			    ci->ci_dev->dv_xname, model, bus);
			break;
		}
	}
}

void
p3_get_bus_clock(struct cpu_info *ci)
{
	u_int64_t msr;
	int model, bus;

	model = (ci->ci_signature >> 4) & 15;
	switch (model) {
	case 0xe: /* Core Duo/Solo */
	case 0xf: /* Core Xeon */
		msr = rdmsr(MSR_FSB_FREQ);
		bus = (msr >> 0) & 0x7;
		switch (bus) {
		case 5:
			bus_clock = 10000;
			break;
		case 1:
			bus_clock = 13333;
			break;
		case 3:
			bus_clock = 16666;
			break;
		case 0:
			bus_clock = 26666;
			break;
		case 4:
			bus_clock = 33333;
			break;
		default:
			printf("%s: unknown Core FSB_FREQ value %d",
			    ci->ci_dev->dv_xname, bus);
			break;
		}
		break;
	default: 
		printf("%s: unknown i686 model %d, can't get bus clock",
		    ci->ci_dev->dv_xname, model);
	}
}

void
est_init(struct cpu_info *ci)
{
	const char *cpu_device = ci->ci_dev->dv_xname;
	int vendor = -1;
	int i, mhz, mv, low, high, family;
	u_int64_t msr;
	u_int16_t idhi, idlo, cur;
	u_int8_t crhi, crlo, crcur;

	if (setperf_prio > 3)
		return;

	family = (ci->ci_signature >> 8) & 15;
	if (family == 0xf) {
		p4_get_bus_clock(ci);
	} else if (family == 6) {
		p3_get_bus_clock(ci);
	}
	if (bus_clock == 0) {
		printf("%s: EST: unknown system bus clock\n", cpu_device);
		return;
	}

	msr = rdmsr(MSR_PERF_STATUS);
	idhi = (msr >> 32) & 0xffff;
	idlo = (msr >> 48) & 0xffff;
	cur = msr & 0xffff;
	crhi = (idhi  >> 8) & 0xff;
	crlo = (idlo  >> 8) & 0xff;
	crcur = (cur >> 8) & 0xff;
	if (crlo == 0 || crhi == crlo) {
		/*
		 * Don't complain about these cases, and silently disable EST:
		 * - A lowest clock ratio of 0, which seems to happen on all
		 *   Pentium 4's that report EST.
		 * - An equal highest and lowest clock ratio, which happens on
		 *   at least the Core 2 Duo X6800, maybe on newer models too.
		 */
		return;
	}
	if (crhi == 0 || crcur == 0 || crlo > crhi ||
	    crcur < crlo || crcur > crhi) {
		/*
		 * Do complain about other weirdness, because we first want to
		 * know about it, before we decide what to do with it.
		 */
		printf("%s: EST: strange msr value 0x%016llx\n",
		    cpu_device, msr);
		return;
	}
	if (est_fqlist == NULL) {
		printf("%s: unknown Enhanced SpeedStep CPU, msr 0x%016llx\n",
		    cpu_device, msr);

		/*
		 * Generate a fake table with the power states we know.
		 */
		fake_table[0] = idhi;
		if (cur == idhi || cur == idlo) {
			printf("%s: using only highest and lowest power "
			    "states\n", cpu_device);

			fake_table[1] = idlo;
			fake_fqlist.n = 2;
		} else {
			printf("%s: using only highest, current and lowest "
			    "power states\n", cpu_device);

			fake_table[1] = cur;
			fake_table[2] = idlo;
			fake_fqlist.n = 3;
		}
		fake_fqlist.vendor = vendor;
		fake_fqlist.table = fake_table;
		est_fqlist = &fake_fqlist;
	}

	mhz = MSR2MHZ(cur, bus_clock);
	mv = MSR2MV(cur);
	printf("%s: Enhanced SpeedStep %d MHz (%d mV)", cpu_device, mhz, mv);

	low = MSR2MHZ(est_fqlist->table[est_fqlist->n - 1], bus_clock);
	high = MSR2MHZ(est_fqlist->table[0], bus_clock);
	perflevel = (mhz - low) * 100 / (high - low);

	/*
	 * OK, tell the user the available frequencies.
	 */
	printf(": speeds: ");
	for (i = 0; i < est_fqlist->n; i++)
		printf("%d%s", MSR2MHZ(est_fqlist->table[i], bus_clock),
		    i < est_fqlist->n - 1 ? ", " : " MHz\n");

	cpu_setperf = est_setperf;
	setperf_prio = 3;
}

void
est_setperf(int level)
{
	int i;
	uint64_t msr;

	if (est_fqlist == NULL)
		return;

	i = ((level * est_fqlist->n) + 1) / 101;
	if (i >= est_fqlist->n)
		i = est_fqlist->n - 1;
	i = est_fqlist->n - 1 - i;

	msr = rdmsr(MSR_PERF_CTL);
	msr &= ~0xffffULL;
	msr |= est_fqlist->table[i];
	wrmsr(MSR_PERF_CTL, msr);
	cpuspeed = MSR2MHZ(est_fqlist->table[i], bus_clock);
}
