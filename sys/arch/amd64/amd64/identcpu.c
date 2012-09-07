/*	$OpenBSD: identcpu.c,v 1.38 2012/09/07 22:02:34 naddy Exp $	*/
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
#include <sys/sysctl.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>

/* sysctl wants this. */
char cpu_model[48];
int cpuspeed;

int amd64_has_xcrypt;
#ifdef CRYPTO
int amd64_has_aesni;
#endif

const struct {
	u_int32_t	bit;
	char		str[12];
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
	{ CPUID_PSN,	"PSN" },
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
	{ CPUID_PBE,	"PBE" }
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
	{ CPUIDECX_PCLMUL,	"PCLMUL" },
	{ CPUIDECX_DTES64,	"DTES64" },
	{ CPUIDECX_MWAIT,	"MWAIT" },
	{ CPUIDECX_DSCPL,	"DS-CPL" },
	{ CPUIDECX_VMX,		"VMX" },
	{ CPUIDECX_SMX,		"SMX" },
	{ CPUIDECX_EST,		"EST" },
	{ CPUIDECX_TM2,		"TM2" },
	{ CPUIDECX_SSSE3,	"SSSE3" },
	{ CPUIDECX_CNXTID,	"CNXT-ID" },
	{ CPUIDECX_FMA3,	"FMA3" },
	{ CPUIDECX_CX16,	"CX16" },
	{ CPUIDECX_XTPR,	"xTPR" },
	{ CPUIDECX_PDCM,	"PDCM" },
	{ CPUIDECX_PCID,	"PCID" },
	{ CPUIDECX_DCA,		"DCA" },
	{ CPUIDECX_SSE41,	"SSE4.1" },
	{ CPUIDECX_SSE42,	"SSE4.2" },
	{ CPUIDECX_X2APIC,	"x2APIC" },
	{ CPUIDECX_MOVBE,	"MOVBE" },
	{ CPUIDECX_POPCNT,	"POPCNT" },
	{ CPUIDECX_DEADLINE,	"DEADLINE" },
	{ CPUIDECX_AES,		"AES" },
	{ CPUIDECX_XSAVE,	"XSAVE" },
	{ CPUIDECX_OSXSAVE,	"OSXSAVE" },
	{ CPUIDECX_AVX,		"AVX" },
	{ CPUIDECX_F16C,	"F16C" },
	{ CPUIDECX_RDRAND,	"RDRAND" },
}, cpu_ecpuid_ecxfeatures[] = {
	{ CPUIDECX_LAHF,	"LAHF" },
	{ CPUIDECX_CMPLEG,	"CMPLEG" },
	{ CPUIDECX_SVM,		"SVM" },
	{ CPUIDECX_EAPICSP,	"EAPICSP"},
	{ CPUIDECX_AMCR8,	"AMCR8"},
	{ CPUIDECX_ABM,		"ABM" },
	{ CPUIDECX_SSE4A,	"SSE4A" },
	{ CPUIDECX_MASSE,	"MASSE" },
	{ CPUIDECX_3DNOWP,	"3DNOWP" },
	{ CPUIDECX_OSVW,	"OSVW" },
	{ CPUIDECX_IBS,		"IBS" },
	{ CPUIDECX_XOP,		"XOP" },
	{ CPUIDECX_SKINIT,	"SKINIT" },
	{ CPUIDECX_LWP,		"WDT" },
	{ CPUIDECX_FMA4,	"FMA4" },
	{ CPUIDECX_NODEID,	"NODEID" },
	{ CPUIDECX_TBM,		"TBM" },
	{ CPUIDECX_TOPEXT,	"TOPEXT" },
}, cpu_seff0_ebxfeatures[] = {
	{ SEFF0EBX_FSGSBASE,	"FSGSBASE" },
	{ SEFF0EBX_SMEP,	"SMEP" },
	{ SEFF0EBX_EREP,	"EREP" },
	{ SEFF0EBX_INVPCID,	"INVPCID" },
};

int
cpu_amd64speed(int *freq)
{
	*freq = cpuspeed;
	return (0);
}

#ifndef SMALL_KERNEL
void	intelcore_update_sensor(void *args);
/*
 * Temperature read on the CPU is relative to the maximum
 * temperature supported by the CPU, Tj(Max).
 * Poorly documented, refer to:
 * http://softwarecommunity.intel.com/isn/Community/
 * en-US/forums/thread/30228638.aspx
 * Basically, depending on a bit in one msr, the max is either 85 or 100.
 * Then we subtract the temperature portion of thermal status from
 * max to get current temperature.
 */
void
intelcore_update_sensor(void *args)
{
	struct cpu_info *ci = (struct cpu_info *) args;
	u_int64_t msr;
	int max = 100;

	/* Only some Core family chips have MSR_TEMPERATURE_TARGET. */
	if (ci->ci_model == 0xe &&
	    (rdmsr(MSR_TEMPERATURE_TARGET) & MSR_TEMPERATURE_TARGET_LOW_BIT))
		max = 85;

	msr = rdmsr(MSR_THERM_STATUS);
	if (msr & MSR_THERM_STATUS_VALID_BIT) {
		ci->ci_sensor.value = max - MSR_THERM_STATUS_TEMP(msr);
		/* micro degrees */
		ci->ci_sensor.value *= 1000000;
		/* kelvin */
		ci->ci_sensor.value += 273150000;
		ci->ci_sensor.flags &= ~SENSOR_FINVALID;
	} else {
		ci->ci_sensor.value = 0;
		ci->ci_sensor.flags |= SENSOR_FINVALID;
	}
}

#endif

void (*setperf_setup)(struct cpu_info *);

void via_nano_setup(struct cpu_info *ci);

void
via_nano_setup(struct cpu_info *ci)
{
	u_int32_t regs[4], val;
	u_int64_t msreg;
	int model = (ci->ci_signature >> 4) & 15;

	if (model >= 9) {
		CPUID(0xC0000000, regs[0], regs[1], regs[2], regs[3]);
		val = regs[0];
		if (val >= 0xC0000001) {
			CPUID(0xC0000001, regs[0], regs[1], regs[2], regs[3]);
			val = regs[3];
		} else
			val = 0;

		if (val & (C3_CPUID_HAS_RNG | C3_CPUID_HAS_ACE))
			printf("%s:", ci->ci_dev->dv_xname);

		/* Enable RNG if present and disabled */
		if (val & C3_CPUID_HAS_RNG) {
			extern int viac3_rnd_present;

			if (!(val & C3_CPUID_DO_RNG)) {
				msreg = rdmsr(0x110B);
				msreg |= 0x40;
				wrmsr(0x110B, msreg);
			}
			viac3_rnd_present = 1;
			printf(" RNG");
		}

		/* Enable AES engine if present and disabled */
		if (val & C3_CPUID_HAS_ACE) {
#ifdef CRYPTO
			if (!(val & C3_CPUID_DO_ACE)) {
				msreg = rdmsr(0x1107);
				msreg |= (0x01 << 28);
				wrmsr(0x1107, msreg);
			}
			amd64_has_xcrypt |= C3_HAS_AES;
#endif /* CRYPTO */
			printf(" AES");
		}

		/* Enable ACE2 engine if present and disabled */
		if (val & C3_CPUID_HAS_ACE2) {
#ifdef CRYPTO
			if (!(val & C3_CPUID_DO_ACE2)) {
				msreg = rdmsr(0x1107);
				msreg |= (0x01 << 28);
				wrmsr(0x1107, msreg);
			}
			amd64_has_xcrypt |= C3_HAS_AESCTR;
#endif /* CRYPTO */
			printf(" AES-CTR");
		}

		/* Enable SHA engine if present and disabled */
		if (val & C3_CPUID_HAS_PHE) {
#ifdef CRYPTO
			if (!(val & C3_CPUID_DO_PHE)) {
				msreg = rdmsr(0x1107);
				msreg |= (0x01 << 28/**/);
				wrmsr(0x1107, msreg);
			}
			amd64_has_xcrypt |= C3_HAS_SHA;
#endif /* CRYPTO */
			printf(" SHA1 SHA256");
		}

		/* Enable MM engine if present and disabled */
		if (val & C3_CPUID_HAS_PMM) {
#ifdef CRYPTO
			if (!(val & C3_CPUID_DO_PMM)) {
				msreg = rdmsr(0x1107);
				msreg |= (0x01 << 28/**/);
				wrmsr(0x1107, msreg);
			}
			amd64_has_xcrypt |= C3_HAS_MM;
#endif /* CRYPTO */
			printf(" RSA");
		}

		printf("\n");
	}
}

#ifndef SMALL_KERNEL
void via_update_sensor(void *args);
void
via_update_sensor(void *args)
{
	struct cpu_info *ci = (struct cpu_info *) args;
	u_int64_t msr;

	msr = rdmsr(MSR_CENT_TMTEMPERATURE);
	ci->ci_sensor.value = (msr & 0xffffff);
	/* micro degrees */
	ci->ci_sensor.value *= 1000000;
	ci->ci_sensor.value += 273150000;
	ci->ci_sensor.flags &= ~SENSOR_FINVALID;
}
#endif

void
identifycpu(struct cpu_info *ci)
{
	u_int64_t last_tsc;
	u_int32_t dummy, val, pnfeatset;
	u_int32_t brand[12];
	char mycpu_model[48];
	int i, max;
	char *brandstr_from, *brandstr_to;
	int skipspace;

	CPUID(1, ci->ci_signature, val, dummy, ci->ci_feature_flags);
	CPUID(0x80000000, pnfeatset, dummy, dummy, dummy);
	if (pnfeatset >= 0x80000001) {
		u_int32_t ecx;

		CPUID(0x80000001, dummy, dummy,
		    ecx, ci->ci_feature_eflags);
		if (ci->ci_flags & CPUF_PRIMARY)
			ecpu_ecxfeature = ecx;
		/* Let cpu_fature be the common bits */
		cpu_feature &= ci->ci_feature_flags;
	}

	CPUID(0x80000002, brand[0], brand[1], brand[2], brand[3]);
	CPUID(0x80000003, brand[4], brand[5], brand[6], brand[7]);
	CPUID(0x80000004, brand[8], brand[9], brand[10], brand[11]);
	strlcpy(mycpu_model, (char *)brand, sizeof(mycpu_model));

	/* Remove leading and duplicated spaces from mycpu_model */
	brandstr_from = brandstr_to = mycpu_model;
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

	if (mycpu_model[0] == 0)
		strlcpy(mycpu_model, "Opteron or Athlon 64",
		    sizeof(mycpu_model));

	/* If primary cpu, fill in the global cpu_model used by sysctl */
	if (ci->ci_flags & CPUF_PRIMARY)
		strlcpy(cpu_model, mycpu_model, sizeof(cpu_model));

	ci->ci_family = (ci->ci_signature >> 8) & 0x0f;
	ci->ci_model = (ci->ci_signature >> 4) & 0x0f;
	if (ci->ci_family == 0x6 || ci->ci_family == 0xf) {
		ci->ci_family += (ci->ci_signature >> 20) & 0xff;
		ci->ci_model += ((ci->ci_signature >> 16) & 0x0f) << 4;
	}

	last_tsc = rdtsc();
	delay(100000);
	ci->ci_tsc_freq = (rdtsc() - last_tsc) * 10;

	amd_cpu_cacheinfo(ci);

	printf("%s: %s", ci->ci_dev->dv_xname, mycpu_model);

	if (ci->ci_tsc_freq != 0)
		printf(", %lu.%02lu MHz", (ci->ci_tsc_freq + 4999) / 1000000,
		    ((ci->ci_tsc_freq + 4999) / 10000) % 100);

	if (ci->ci_flags & CPUF_PRIMARY) {
		cpuspeed = (ci->ci_tsc_freq + 4999) / 1000000;
		cpu_cpuspeed = cpu_amd64speed;
	}

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
	max = sizeof(cpu_ecpuid_ecxfeatures) / sizeof(cpu_ecpuid_ecxfeatures[0]);
	for (i = 0; i < max; i++)
		if (ecpu_ecxfeature & cpu_ecpuid_ecxfeatures[i].bit)
			printf(",%s", cpu_ecpuid_ecxfeatures[i].str);

	if (cpuid_level >= 0x07) {
		/* "Structured Extended Feature Flags" */
		CPUID_LEAF(0x7, 0, dummy, val, dummy, dummy);
		max = sizeof(cpu_seff0_ebxfeatures) /
		    sizeof(cpu_seff0_ebxfeatures[0]);
		for (i = 0; i < max; i++)
			if (val & cpu_seff0_ebxfeatures[i].bit)
				printf(",%s", cpu_seff0_ebxfeatures[i].str);
	}

	printf("\n");

	x86_print_cacheinfo(ci);

#ifndef SMALL_KERNEL
	if (ci->ci_flags & CPUF_PRIMARY) {
		if (pnfeatset > 0x80000007) {
			CPUID(0x80000007, dummy, dummy, dummy, pnfeatset);

			if (pnfeatset & 0x06) {
				if ((ci->ci_signature & 0xF00) == 0xf00)
					setperf_setup = k8_powernow_init;
			}
			if (ci->ci_family >= 0x10)
				setperf_setup = k1x_init;
		}

		if (cpu_ecxfeature & CPUIDECX_EST)
			setperf_setup = est_init;

		if (cpu_ecxfeature & CPUIDECX_AES)
			amd64_has_aesni = 1;
	}
	if (!strncmp(mycpu_model, "Intel", 5)) {
		u_int32_t cflushsz;

		CPUID(0x01, dummy, cflushsz, dummy, dummy);
		/* cflush cacheline size is equal to bits 15-8 of ebx * 8 */
		ci->ci_cflushsz = ((cflushsz >> 8) & 0xff) * 8;
		CPUID(0x06, val, dummy, dummy, dummy);
		if (val & 0x1) {
			strlcpy(ci->ci_sensordev.xname, ci->ci_dev->dv_xname,
			    sizeof(ci->ci_sensordev.xname));
			ci->ci_sensor.type = SENSOR_TEMP;
			sensor_task_register(ci, intelcore_update_sensor, 5);
			sensor_attach(&ci->ci_sensordev, &ci->ci_sensor);
			sensordev_install(&ci->ci_sensordev);
		}
	}

#endif

	if (!strcmp(cpu_vendor, "AuthenticAMD"))
		amd64_errata(ci);

	if (strncmp(mycpu_model, "VIA Nano processor", 18) == 0) {
		ci->cpu_setup = via_nano_setup;
#ifndef SMALL_KERNEL
		strlcpy(ci->ci_sensordev.xname, ci->ci_dev->dv_xname,
		    sizeof(ci->ci_sensordev.xname));
		ci->ci_sensor.type = SENSOR_TEMP;
		sensor_task_register(ci, via_update_sensor, 5);
		sensor_attach(&ci->ci_sensordev, &ci->ci_sensor);
		sensordev_install(&ci->ci_sensordev);
#endif
	}
}
