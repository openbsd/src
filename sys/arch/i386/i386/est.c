/*	$OpenBSD: est.c,v 1.8 2004/06/06 17:34:37 grange Exp $ */
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


struct fq_info {
	u_short mhz;
	u_short mv;
};

/* Ultra Low Voltage Intel Pentium M processor 900 MHz */
static const struct fq_info pentium_m_900[] = {
	{  900, 1004 },
	{  800,  988 },
	{  600,  844 },
};

/* Ultra Low Voltage Intel Pentium M processor 1.00 GHz */
static const struct fq_info pentium_m_1000[] = {
	{ 1000, 1004 },
	{  900,  988 },
	{  800,  972 },
	{  600,  844 },
};

/* Low Voltage Intel Pentium M processor 1.10 GHz */
static const struct fq_info pentium_m_1100[] = {
	{ 1100, 1180 },
	{ 1000, 1164 },
	{  900, 1100 },
	{  800, 1020 },
	{  600,  956 },
};

/* Low Voltage Intel Pentium M processor 1.20 GHz */
static const struct fq_info pentium_m_1200[] = {
	{ 1200, 1180 },
	{ 1100, 1164 },
	{ 1000, 1100 },
	{  900, 1020 },
	{  800, 1004 },
	{  600,  956 },
};

/* Intel Pentium M processor 1.30 GHz */
static const struct fq_info pentium_m_1300[] = {
	{ 1300, 1388 },
	{ 1200, 1356 },
	{ 1000, 1292 },
	{  800, 1260 },
	{  600,  956 },
};

/* Intel Pentium M processor 1.40 GHz */
static const struct fq_info pentium_m_1400[] = {
	{ 1400, 1484 },
	{ 1200, 1436 },
	{ 1000, 1308 },
	{  800, 1180 },
	{  600,  956 }
};

/* Intel Pentium M processor 1.50 GHz */
static const struct fq_info pentium_m_1500[] = {
	{ 1500, 1484 },
	{ 1400, 1452 },
	{ 1200, 1356 },
	{ 1000, 1228 },
	{  800, 1116 },
	{  600,  956 }
};

/* Intel Pentium M processor 1.60 GHz */
static const struct fq_info pentium_m_1600[] = {
	{ 1600, 1484 },
	{ 1400, 1420 },
	{ 1200, 1276 },
	{ 1000, 1164 },
	{  800, 1036 },
	{  600,  956 }
};

/* Intel Pentium M processor 1.70 GHz */
static const struct fq_info pentium_m_1700[] = {
	{ 1700, 1484 },
	{ 1400, 1308 },
	{ 1200, 1228 },
	{ 1000, 1116 },
	{  800, 1004 },
	{  600,  956 }
};


struct fqlist {
	const char *brand_tag;
	const struct fq_info *table;
	u_int n;
};

static const struct fqlist pentium_m[] = {
#define ENTRY(s, v)	{ s, v, sizeof(v) / sizeof((v)[0]) }
	ENTRY(" 900", pentium_m_900),
	ENTRY("1000", pentium_m_1000),
	ENTRY("1100", pentium_m_1100),
	ENTRY("1200", pentium_m_1200),
	ENTRY("1300", pentium_m_1300),
	ENTRY("1400", pentium_m_1400),
	ENTRY("1500", pentium_m_1500),
	ENTRY("1600", pentium_m_1600),
	ENTRY("1700", pentium_m_1700),
#undef ENTRY
};


struct est_cpu {
	const char *brand_prefix;
	const char *brand_suffix;
	const struct fqlist *list;
	int n;
};

static const struct est_cpu est_cpus[] = {
	{
		"Intel(R) Pentium(R) M processor ", "MHz",
		pentium_m,
		(sizeof(pentium_m) / sizeof(pentium_m[0])),
	},
};

#define NESTCPUS	  (sizeof(est_cpus) / sizeof(est_cpus[0]))


#define MSRVALUE(mhz, mv)	((((mhz) / 100) << 8) | (((mv) - 700) / 16))
#define MSR2MHZ(msr)		((((int) (msr) >> 8) & 0xff) * 100)
#define MSR2MV(msr)		(((int) (msr) & 0xff) * 16 + 700)

static const struct fqlist *est_fqlist;

extern int setperf_prio;

void
est_init(const char *cpu_device)
{
	int i, j, n, mhz, mv;
	const struct est_cpu *cpu;
	u_int64_t msr;
	char *tag;
	const struct fqlist *fql;
	extern char cpu_brandstr[];

	if (setperf_prio > 3)
		return;

	if ((cpu_ecxfeature & CPUIDECX_EST) == 0)
		return;

	msr = rdmsr(MSR_PERF_STATUS);
	mhz = MSR2MHZ(msr);
	mv = MSR2MV(msr);
	printf("%s: Enhanced SpeedStep %d MHz (%d mV)",
	     cpu_device, mhz, mv);

	/*
	 * Look for a CPU matching cpu_brandstr.
	 */
	for (i = 0; est_fqlist == NULL && i < NESTCPUS; i++) {
		cpu = &est_cpus[i];
		n = strlen(cpu->brand_prefix);
		if (strncmp(cpu->brand_prefix, cpu_brandstr, n) != 0)
			continue;
		tag = cpu_brandstr + n;
		for (j = 0; j < cpu->n; j++) {
			fql = &cpu->list[j];
			n = strlen(fql->brand_tag);
			if (!strncmp(fql->brand_tag, tag, n) &&
			    !strcmp(cpu->brand_suffix, tag + n)) {
				est_fqlist = fql;
				break;
			}
		}
	}
	if (est_fqlist == NULL) {
		printf(": unknown EST cpu, no changes possible\n");
		return;
	}

	/*
	 * Check that the current operating point is in our list.
	 */
	for (i = est_fqlist->n - 1; i >= 0; i--)
		if (est_fqlist->table[i].mhz == mhz &&
		    est_fqlist->table[i].mv == mv)
			break;
	if (i < 0) {
		printf(" (not in table)\n");
		return;
	}

	/*
	 * OK, tell the user the available frequencies.
	 */
	printf(": speeds: ");
	for (i = 0; i < est_fqlist->n; i++)
		printf("%d%s", est_fqlist->table[i].mhz,
		    i < est_fqlist->n - 1 ? ", " : " MHz\n");

	cpu_setperf = est_setperf;
	setperf_prio = 3;
}

int
est_setperf(int level)
{
	int low, high, i, fq;
	uint64_t msr;

	if (est_fqlist == NULL)
		return (EOPNOTSUPP);

	low = est_fqlist->table[est_fqlist->n - 1].mhz;
	high = est_fqlist->table[0].mhz;
	fq = low + (high - low) * level / 100;

	for (i = est_fqlist->n - 1; i > 0; i--)
		if (est_fqlist->table[i].mhz >= fq)
			break;
	msr = (rdmsr(MSR_PERF_CTL) & ~0xffffULL) |
	    MSRVALUE(est_fqlist->table[i].mhz, est_fqlist->table[i].mv);
	wrmsr(MSR_PERF_CTL, msr);
	pentium_mhz = est_fqlist->table[i].mhz;
	
	return (0);
}
