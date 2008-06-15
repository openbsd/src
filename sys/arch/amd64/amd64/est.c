/*	$OpenBSD: est.c,v 1.6 2008/06/15 05:24:07 gwk Exp $ */
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
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>
#include <machine/bus.h>

#include "acpicpu.h"

#if NACPICPU > 0
#include <dev/acpi/acpidev.h>
#include <dev/acpi/acpivar.h>
#endif

struct est_op {
	uint16_t ctrl;
	uint16_t mhz;
};

struct fqlist {
	unsigned n : 5;
	struct est_op *table;
};


static struct fqlist *est_fqlist;

extern int setperf_prio;
extern int perflevel;

#if NACPICPU > 0
struct fqlist * est_acpi_init(void);
void est_acpi_pss_changed(struct acpicpu_pss *, int);

struct fqlist *
est_acpi_init()
{
	struct acpicpu_pss *pss;
	struct fqlist *acpilist;
	int nstates, i;

	if ((nstates = acpicpu_fetch_pss(&pss)) == 0)
		goto nolist;

	if ((acpilist = malloc(sizeof(struct fqlist), M_DEVBUF, M_NOWAIT))
	    == NULL)
		goto nolist;

	if ((acpilist->table = malloc(sizeof(struct est_op) * nstates, M_DEVBUF,
	   M_NOWAIT)) == NULL)
		goto notable;

	acpilist->n = nstates;

	for (i = 0; i < nstates; i++) {
		acpilist->table[i].mhz = pss[i].pss_core_freq;
		acpilist->table[i].ctrl = pss[i].pss_ctrl;
	}

	acpicpu_set_notify(est_acpi_pss_changed);

	return acpilist;

notable:
	free(acpilist, M_DEVBUF);
	acpilist = NULL;
nolist:
	return NULL;
}

void
est_acpi_pss_changed(struct acpicpu_pss *pss, int npss)
{
	struct fqlist *acpilist;
	int needtran = 1, nstates, i;
	u_int64_t msr;
	u_int16_t cur;

	msr = rdmsr(MSR_PERF_STATUS);
	cur = msr & 0xffff;

	if ((acpilist = malloc(sizeof(struct fqlist), M_DEVBUF, M_NOWAIT))
	    == NULL) {
		printf("est_acpi_pss_changed: cannot allocate memory for new est state");
		return;
	}

	if ((acpilist->table = malloc(sizeof(struct est_op) * nstates, M_DEVBUF,
	   M_NOWAIT)) == NULL) {
		printf("est_acpi_pss_changed: cannot allocate memory for new operating points");
		free(acpilist, M_DEVBUF);
		return;
	}

	for (i = 0; i < nstates; i++) {
		acpilist->table[i].mhz = pss[i].pss_core_freq;
		acpilist->table[i].ctrl = pss[i].pss_ctrl;
		if (pss[i].pss_ctrl == cur)
			needtran = 0;
	}

	free(est_fqlist->table, M_DEVBUF);
	free(est_fqlist, M_DEVBUF);
	est_fqlist = acpilist;

	if (needtran) {
		est_setperf(perflevel);
	}
}
#endif

void
est_init(struct cpu_info *ci)
{
	const char *cpu_device = ci->ci_dev->dv_xname;
	int i, low, high;
	u_int64_t msr;
	u_int16_t idhi, idlo, cur;
	u_int8_t crhi, crlo, crcur;

	if (setperf_prio > 3)
		return;

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


#if NACPICPU > 0
		est_fqlist = est_acpi_init();
#endif
	if (est_fqlist == NULL)
		return;

	printf("%s: Enhanced SpeedStep %d MHz", cpu_device, cpuspeed);

	low = est_fqlist->table[est_fqlist->n - 1].mhz;
	high = est_fqlist->table[0].mhz;
	perflevel = (cpuspeed - low) * 100 / (high - low);

	/*
	 * OK, tell the user the available frequencies.
	 */
	printf(": speeds: ");
	for (i = 0; i < est_fqlist->n; i++)
		printf("%d%s", est_fqlist->table[i].mhz, i < est_fqlist->n - 1 ?
		    ", " : " MHz\n");

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
	msr |= est_fqlist->table[i].ctrl;

	wrmsr(MSR_PERF_CTL, msr);
	cpuspeed = est_fqlist->table[i].mhz;
}
