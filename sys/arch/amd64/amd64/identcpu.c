/*	$OpenBSD: identcpu.c,v 1.12 2007/05/06 03:37:08 gwk Exp $	*/
/*	$NetBSD: identcpu.c,v 1.1 2003/04/26 18:39:28 fvdl Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>

/* sysctl wants this. */
char cpu_model[48];
int cpuspeed;

const struct {
	u_int32_t	bit;
	char		str[8];
} cpu_cpuid_features[] = {
	{ CPUID_FPU,	"FPU" },
	{ CPUID_VME,	"VME" },
	{ CPUID_DE,	"DE" },
	{ CPUID_PSE,	"PSE" },
	{ CPUID_TSC,	"TSC" },
	{ CPUID_MSR,	"MSR" },
	{ CPUID_PAE,	"PAE" },
	{ CPUID_MCE,	"MCE" },
	{ CPUID_CX8,	"CX8" },
	{ CPUID_APIC,	"APIC" },
	{ CPUID_SEP,	"SEP" },
	{ CPUID_MTRR,	"MTRR" },
	{ CPUID_PGE,	"PGE" },
	{ CPUID_MCA,	"MCA" },
	{ CPUID_CMOV,	"CMOV" },
	{ CPUID_PAT,	"PAT" },
	{ CPUID_PSE36,	"PSE36" },
	{ CPUID_PN,	"PN" },
	{ CPUID_CFLUSH,	"CFLUSH" },
	{ CPUID_DS,	"DS" },
	{ CPUID_ACPI,	"ACPI" },
	{ CPUID_MMX,	"MMX" },
	{ CPUID_FXSR,	"FXSR" },
	{ CPUID_SSE,	"SSE" },
	{ CPUID_SSE2,	"SSE2" },
	{ CPUID_SS,	"SS" },
	{ CPUID_HTT,	"HTT" },
	{ CPUID_TM,	"TM" },
	{ CPUID_SBF,	"SBF" }
}, cpu_ecpuid_features[] = {
	{ CPUID_MPC,	"MPC" },
	{ CPUID_NXE,	"NXE" },
	{ CPUID_MMXX,	"MMXX" },
	{ CPUID_FFXSR,	"FFXSR" },
	{ CPUID_LONG,	"LONG" },
	{ CPUID_3DNOW2,	"3DNOW2" },
	{ CPUID_3DNOW,	"3DNOW" }
}, cpu_cpuid_ecxfeatures[] = {
	{ CPUIDECX_SSE3,	"SSE3" },
	{ CPUIDECX_MWAIT,	"MWAIT" },
	{ CPUIDECX_DSCPL,	"DS-CPL" },
	{ CPUIDECX_VMX,		"VMX" },
	{ CPUIDECX_EST,		"EST" },
	{ CPUIDECX_TM2,		"TM2" },
	{ CPUIDECX_CNXTID,	"CNXT-ID" },
	{ CPUIDECX_CX16,	"CX16" },
	{ CPUIDECX_XTPR,	"xTPR" }
};

int
cpu_amd64speed(int *freq)
{
	*freq = cpuspeed;
	return (0);
}

void (*setperf_setup)(struct cpu_info *);

void
identifycpu(struct cpu_info *ci)
{
	u_int64_t last_tsc;
	u_int32_t dummy, val, pnfeatset;
	u_int32_t brand[12];
	u_int32_t vendor[4];
	int i, max;
	char *brandstr_from, *brandstr_to;
	int skipspace;

	CPUID(1, ci->ci_signature, val, dummy, ci->ci_feature_flags);
	CPUID(0x80000000, pnfeatset, dummy, dummy, dummy);
	CPUID(0x80000001, dummy, dummy, dummy, ci->ci_feature_eflags);

	vendor[3] = 0;
	CPUID(0, dummy, vendor[0], vendor[2], vendor[1]);	/* yup, 0 2 1 */
	CPUID(0x80000002, brand[0], brand[1], brand[2], brand[3]);
	CPUID(0x80000003, brand[4], brand[5], brand[6], brand[7]);
	CPUID(0x80000004, brand[8], brand[9], brand[10], brand[11]);

	strlcpy(cpu_model, (char *)brand, sizeof(cpu_model));

	/* Remove leading and duplicated spaces from cpu_model */
	brandstr_from = brandstr_to = cpu_model;
	skipspace = 1;
	while (*brandstr_from != '\0') {
		if (!skipspace || *brandstr_from != ' ') {
			skipspace = 0;
			*(brandstr_to++) = *brandstr_from;
		}
		if (*brandstr_from == ' ')
			skipspace = 1;
		brandstr_from++;
	}
	*brandstr_to = '\0';

	if (cpu_model[0] == 0)
		strlcpy(cpu_model, "Opteron or Athlon 64", sizeof(cpu_model));

	last_tsc = rdtsc();
	delay(100000);
	ci->ci_tsc_freq = (rdtsc() - last_tsc) * 10;

	amd_cpu_cacheinfo(ci);

	printf("%s: %s", ci->ci_dev->dv_xname, cpu_model);

	if (ci->ci_tsc_freq != 0)
		printf(", %lu.%02lu MHz", (ci->ci_tsc_freq + 4999) / 1000000,
		    ((ci->ci_tsc_freq + 4999) / 10000) % 100);
	cpuspeed = (ci->ci_tsc_freq + 4999) / 1000000;
	cpu_cpuspeed = cpu_amd64speed;

	printf("\n%s: ", ci->ci_dev->dv_xname);

	max = sizeof(cpu_cpuid_features) / sizeof(cpu_cpuid_features[0]);
	for (i = 0; i < max; i++)
		if (ci->ci_feature_flags & cpu_cpuid_features[i].bit)
			printf("%s%s", i? "," : "", cpu_cpuid_features[i].str);
	max = sizeof(cpu_cpuid_ecxfeatures) / sizeof(cpu_cpuid_ecxfeatures[0]);
	for (i = 0; i < max; i++)
		if (cpu_ecxfeature & cpu_cpuid_ecxfeatures[i].bit)
			printf(",%s", cpu_cpuid_ecxfeatures[i].str);
	max = sizeof(cpu_ecpuid_features) / sizeof(cpu_ecpuid_features[0]);
	for (i = 0; i < max; i++)
		if (ci->ci_feature_eflags & cpu_ecpuid_features[i].bit)
			printf(",%s", cpu_ecpuid_features[i].str);
	printf("\n");

	x86_print_cacheinfo(ci);

	if (pnfeatset > 0x80000007) {
		CPUID(0x80000007, dummy, dummy, dummy, pnfeatset);

		if (pnfeatset & 0x06) {
			if ((ci->ci_signature & 0xF00) == 0xf00)
				setperf_setup = k8_powernow_init;
		}
	}

	/* AuthenticAMD:    h t u A                    i t n e */
	if (vendor[0] == 0x68747541 && vendor[1] == 0x69746e65 &&
	    vendor[2] == 0x444d4163)	/* DMAc */
		amd64_errata(ci);
}

void
cpu_probe_features(struct cpu_info *ci)
{
	ci->ci_feature_flags = cpu_feature;
	ci->ci_signature = 0;
}
