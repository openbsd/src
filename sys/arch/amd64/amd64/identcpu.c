/*	$OpenBSD: identcpu.c,v 1.110 2018/10/20 20:40:54 kettenis Exp $	*/
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
#include <sys/sysctl.h>

#include "vmm.h"

#include <machine/cpu.h>
#include <machine/cpufunc.h>

void	replacesmap(void);
void	replacemeltdown(void);
uint64_t cpu_freq(struct cpu_info *);
void	tsc_timecounter_init(struct cpu_info *, uint64_t);
#if NVMM > 0
void	cpu_check_vmm_cap(struct cpu_info *);
#endif /* NVMM > 0 */

/* sysctl wants this. */
char cpu_model[48];
int cpuspeed;

int amd64_has_xcrypt;
#ifdef CRYPTO
int amd64_has_pclmul;
int amd64_has_aesni;
#endif
int has_rdrand;
int has_rdseed;

#include "pvbus.h"
#if NPVBUS > 0
#include <dev/pv/pvvar.h>
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
	{ CPUID_MPC,		"MPC" },
	{ CPUID_NXE,		"NXE" },
	{ CPUID_MMXX,		"MMXX" },
	{ CPUID_FFXSR,		"FFXSR" },
	{ CPUID_PAGE1GB,	"PAGE1GB" },
	{ CPUID_RDTSCP,		"RDTSCP" },
	{ CPUID_LONG,		"LONG" },
	{ CPUID_3DNOW2,		"3DNOW2" },
	{ CPUID_3DNOW,		"3DNOW" }
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
	{ CPUIDECX_SDBG,	"SDBG" },
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
	{ CPUIDECX_HV,		"HV" },
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
	{ CPUIDECX_TCE,		"TCE" },
	{ CPUIDECX_NODEID,	"NODEID" },
	{ CPUIDECX_TBM,		"TBM" },
	{ CPUIDECX_TOPEXT,	"TOPEXT" },
	{ CPUIDECX_CPCTR,	"CPCTR" },
	{ CPUIDECX_DBKP,	"DBKP" },
	{ CPUIDECX_PERFTSC,	"PERFTSC" },
	{ CPUIDECX_PCTRL3,	"PCTRL3" },
	{ CPUIDECX_MWAITX,	"MWAITX" },
}, cpu_seff0_ebxfeatures[] = {
	{ SEFF0EBX_FSGSBASE,	"FSGSBASE" },
	{ SEFF0EBX_SGX,		"SGX" },
	{ SEFF0EBX_BMI1,	"BMI1" },
	{ SEFF0EBX_HLE,		"HLE" },
	{ SEFF0EBX_AVX2,	"AVX2" },
	{ SEFF0EBX_SMEP,	"SMEP" },
	{ SEFF0EBX_BMI2,	"BMI2" },
	{ SEFF0EBX_ERMS,	"ERMS" },
	{ SEFF0EBX_INVPCID,	"INVPCID" },
	{ SEFF0EBX_RTM,		"RTM" },
	{ SEFF0EBX_PQM,		"PQM" },
	{ SEFF0EBX_MPX,		"MPX" },
	{ SEFF0EBX_AVX512F,	"AVX512F" },
	{ SEFF0EBX_AVX512DQ,	"AVX512DQ" },
	{ SEFF0EBX_RDSEED,	"RDSEED" },
	{ SEFF0EBX_ADX,		"ADX" },
	{ SEFF0EBX_SMAP,	"SMAP" },
	{ SEFF0EBX_AVX512IFMA,	"AVX512IFMA" },
	{ SEFF0EBX_PCOMMIT,	"PCOMMIT" },
	{ SEFF0EBX_CLFLUSHOPT,	"CLFLUSHOPT" },
	{ SEFF0EBX_CLWB,	"CLWB" },
	{ SEFF0EBX_PT,		"PT" },
	{ SEFF0EBX_AVX512PF,	"AVX512PF" },
	{ SEFF0EBX_AVX512ER,	"AVX512ER" },
	{ SEFF0EBX_AVX512CD,	"AVX512CD" },
	{ SEFF0EBX_SHA,		"SHA" },
	{ SEFF0EBX_AVX512BW,	"AVX512BW" },
	{ SEFF0EBX_AVX512VL,	"AVX512VL" },
}, cpu_seff0_ecxfeatures[] = {
	{ SEFF0ECX_PREFETCHWT1,	"PREFETCHWT1" },
	{ SEFF0ECX_AVX512VBMI,	"AVX512VBMI" },
	{ SEFF0ECX_UMIP,	"UMIP" },
	{ SEFF0ECX_PKU,		"PKU" },
}, cpu_seff0_edxfeatures[] = {
	{ SEFF0EDX_AVX512_4FNNIW, "AVX512FNNIW" },
	{ SEFF0EDX_AVX512_4FMAPS, "AVX512FMAPS" },
	{ SEFF0EDX_IBRS,	"IBRS,IBPB" },
	{ SEFF0EDX_STIBP,	"STIBP" },
	{ SEFF0EDX_L1DF,	"L1DF" },
	 /* SEFF0EDX_ARCH_CAP (not printed) */
	{ SEFF0EDX_SSBD,	"SSBD" },
}, cpu_tpm_eaxfeatures[] = {
	{ TPM_SENSOR,		"SENSOR" },
	{ TPM_ARAT,		"ARAT" },
}, cpu_cpuid_perf_eax[] = {
	{ CPUIDEAX_VERID,	"PERF" },
}, cpu_cpuid_apmi_edx[] = {
	{ CPUIDEDX_ITSC,	"ITSC" },
}, cpu_amdspec_ebxfeatures[] = {
	{ CPUIDEBX_IBPB,	"IBPB" },
	{ CPUIDEBX_IBRS,	"IBRS" },
	{ CPUIDEBX_STIBP,	"STIBP" },
	{ CPUIDEBX_SSBD,	"SSBD" },
	{ CPUIDEBX_VIRT_SSBD,	"VIRTSSBD" },
	{ CPUIDEBX_SSBD_NOTREQ,	"SSBDNR" },
}, cpu_xsave_extfeatures[] = {
	{ XSAVE_XSAVEOPT,	"XSAVEOPT" },
	{ XSAVE_XSAVEC,		"XSAVEC" },
	{ XSAVE_XGETBV1,	"XGETBV1" },
	{ XSAVE_XSAVES,		"XSAVES" },
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
 * Refer to:
 * 64-ia-32-architectures-software-developer-vol-3c-part-3-manual.pdf
 * Section 35 and
 * http://www.intel.com/content/dam/www/public/us/en/documents/
 * white-papers/cpu-monitoring-dts-peci-paper.pdf
 *
 * The temperature on Intel CPUs can be between 70 and 105 degC, since
 * Westmere we can read the TJmax from the die. For older CPUs we have
 * to guess or use undocumented MSRs. Then we subtract the temperature
 * portion of thermal status from max to get current temperature.
 */
void
intelcore_update_sensor(void *args)
{
	struct cpu_info *ci = (struct cpu_info *) args;
	u_int64_t msr;
	int max = 100;

	/* Only some Core family chips have MSR_TEMPERATURE_TARGET. */
	if (ci->ci_model == 0x0e &&
	    (rdmsr(MSR_TEMPERATURE_TARGET_UNDOCUMENTED) &
	     MSR_TEMPERATURE_TARGET_LOW_BIT_UNDOCUMENTED))
		max = 85;

	/*
	 * Newer CPUs can tell you what their max temperature is.
	 * See: '64-ia-32-architectures-software-developer-
	 * vol-3c-part-3-manual.pdf'
	 */
	if (ci->ci_model > 0x17 && ci->ci_model != 0x1c &&
	    ci->ci_model != 0x26 && ci->ci_model != 0x27 &&
	    ci->ci_model != 0x35 && ci->ci_model != 0x36)
		max = MSR_TEMPERATURE_TARGET_TJMAX(
		    rdmsr(MSR_TEMPERATURE_TARGET));

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

void cpu_topology(struct cpu_info *ci);

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

uint64_t
cpu_freq_ctr(struct cpu_info *ci)
{
	uint64_t count, last_count, msr;

	if ((ci->ci_flags & CPUF_CONST_TSC) == 0 ||
	    (cpu_perf_eax & CPUIDEAX_VERID) <= 1 ||
	    CPUIDEDX_NUM_FC(cpu_perf_edx) <= 1)
		return (0);

	msr = rdmsr(MSR_PERF_FIXED_CTR_CTRL);
	if (msr & MSR_PERF_FIXED_CTR_FC(1, MSR_PERF_FIXED_CTR_FC_MASK)) {
		/* some hypervisor is dicking us around */
		return (0);
	}

	msr |= MSR_PERF_FIXED_CTR_FC(1, MSR_PERF_FIXED_CTR_FC_1);
	wrmsr(MSR_PERF_FIXED_CTR_CTRL, msr);

	msr = rdmsr(MSR_PERF_GLOBAL_CTRL) | MSR_PERF_GLOBAL_CTR1_EN;
	wrmsr(MSR_PERF_GLOBAL_CTRL, msr);

	last_count = rdmsr(MSR_PERF_FIXED_CTR1);
	delay(100000);
	count = rdmsr(MSR_PERF_FIXED_CTR1);

	msr = rdmsr(MSR_PERF_FIXED_CTR_CTRL);
	msr &= MSR_PERF_FIXED_CTR_FC(1, MSR_PERF_FIXED_CTR_FC_MASK);
	wrmsr(MSR_PERF_FIXED_CTR_CTRL, msr);

	msr = rdmsr(MSR_PERF_GLOBAL_CTRL);
	msr &= ~MSR_PERF_GLOBAL_CTR1_EN;
	wrmsr(MSR_PERF_GLOBAL_CTRL, msr);

	return ((count - last_count) * 10);
}

uint64_t
cpu_freq(struct cpu_info *ci)
{
	uint64_t last_count, count;

	count = cpu_freq_ctr(ci);
	if (count != 0)
		return (count);

	last_count = rdtsc();
	delay(100000);
	count = rdtsc();

	return ((count - last_count) * 10);
}

void
identifycpu(struct cpu_info *ci)
{
	uint64_t freq = 0;
	u_int32_t dummy, val;
	char mycpu_model[48];
	int i;
	char *brandstr_from, *brandstr_to;
	int skipspace;

	CPUID(1, ci->ci_signature, val, dummy, ci->ci_feature_flags);
	CPUID(0x80000000, ci->ci_pnfeatset, dummy, dummy, dummy);
	if (ci->ci_pnfeatset >= 0x80000001) {
		CPUID(0x80000001, ci->ci_efeature_eax, dummy,
		    ci->ci_efeature_ecx, ci->ci_feature_eflags);
		/* Other bits may clash */
		ci->ci_feature_flags |= (ci->ci_feature_eflags & CPUID_NXE);
		if (ci->ci_flags & CPUF_PRIMARY)
			ecpu_ecxfeature = ci->ci_efeature_ecx;
		/* Let cpu_feature be the common bits */
		cpu_feature &= ci->ci_feature_flags;
	}

	CPUID(0x80000002, ci->ci_brand[0],
	    ci->ci_brand[1], ci->ci_brand[2], ci->ci_brand[3]);
	CPUID(0x80000003, ci->ci_brand[4],
	    ci->ci_brand[5], ci->ci_brand[6], ci->ci_brand[7]);
	CPUID(0x80000004, ci->ci_brand[8],
	    ci->ci_brand[9], ci->ci_brand[10], ci->ci_brand[11]);
	strlcpy(mycpu_model, (char *)ci->ci_brand, sizeof(mycpu_model));

	/* Remove leading, trailing and duplicated spaces from mycpu_model */
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
	if (skipspace && brandstr_to > mycpu_model)
		brandstr_to--;
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

	if (ci->ci_feature_flags && ci->ci_feature_flags & CPUID_TSC) {
		/* Has TSC, check if it's constant */
		if (!strcmp(cpu_vendor, "GenuineIntel")) {
			if ((ci->ci_family == 0x0f && ci->ci_model >= 0x03) ||
			    (ci->ci_family == 0x06 && ci->ci_model >= 0x0e)) {
				ci->ci_flags |= CPUF_CONST_TSC;
			}
		} else if (!strcmp(cpu_vendor, "CentaurHauls")) {
			/* VIA */
			if (ci->ci_model >= 0x0f) {
				ci->ci_flags |= CPUF_CONST_TSC;
			}
		} else if (!strcmp(cpu_vendor, "AuthenticAMD")) {
			if (cpu_apmi_edx & CPUIDEDX_ITSC) {
				/* Invariant TSC indicates constant TSC on
				 * AMD.
				 */
				ci->ci_flags |= CPUF_CONST_TSC;
			}
		}

		/* Check if it's an invariant TSC */
		if (cpu_apmi_edx & CPUIDEDX_ITSC)
			ci->ci_flags |= CPUF_INVAR_TSC;
	}

	freq = cpu_freq(ci);

	amd_cpu_cacheinfo(ci);

	printf("%s: %s", ci->ci_dev->dv_xname, mycpu_model);

	if (freq != 0)
		printf(", %llu.%02llu MHz", (freq + 4999) / 1000000,
		    ((freq + 4999) / 10000) % 100);

	if (ci->ci_flags & CPUF_PRIMARY) {
		cpuspeed = (freq + 4999) / 1000000;
		cpu_cpuspeed = cpu_amd64speed;
	}

	printf(", %02x-%02x-%02x", ci->ci_family, ci->ci_model,
	    ci->ci_signature & 0x0f);

	printf("\n%s: ", ci->ci_dev->dv_xname);

	for (i = 0; i < nitems(cpu_cpuid_features); i++)
		if (ci->ci_feature_flags & cpu_cpuid_features[i].bit)
			printf("%s%s", i? "," : "", cpu_cpuid_features[i].str);
	for (i = 0; i < nitems(cpu_cpuid_ecxfeatures); i++)
		if (cpu_ecxfeature & cpu_cpuid_ecxfeatures[i].bit)
			printf(",%s", cpu_cpuid_ecxfeatures[i].str);
	for (i = 0; i < nitems(cpu_ecpuid_features); i++)
		if (ci->ci_feature_eflags & cpu_ecpuid_features[i].bit)
			printf(",%s", cpu_ecpuid_features[i].str);
	for (i = 0; i < nitems(cpu_ecpuid_ecxfeatures); i++)
		if (ecpu_ecxfeature & cpu_ecpuid_ecxfeatures[i].bit)
			printf(",%s", cpu_ecpuid_ecxfeatures[i].str);
	for (i = 0; i < nitems(cpu_cpuid_perf_eax); i++)
		if (cpu_perf_eax & cpu_cpuid_perf_eax[i].bit)
			printf(",%s", cpu_cpuid_perf_eax[i].str);
	for (i = 0; i < nitems(cpu_cpuid_apmi_edx); i++)
		if (cpu_apmi_edx & cpu_cpuid_apmi_edx[i].bit)
			printf(",%s", cpu_cpuid_apmi_edx[i].str);

	if (cpuid_level >= 0x07) {
		/* "Structured Extended Feature Flags" */
		CPUID_LEAF(0x7, 0, dummy, ci->ci_feature_sefflags_ebx,
		    ci->ci_feature_sefflags_ecx, ci->ci_feature_sefflags_edx);
		for (i = 0; i < nitems(cpu_seff0_ebxfeatures); i++)
			if (ci->ci_feature_sefflags_ebx &
			    cpu_seff0_ebxfeatures[i].bit)
				printf(",%s", cpu_seff0_ebxfeatures[i].str);
		for (i = 0; i < nitems(cpu_seff0_ecxfeatures); i++)
			if (ci->ci_feature_sefflags_ecx &
			    cpu_seff0_ecxfeatures[i].bit)
				printf(",%s", cpu_seff0_ecxfeatures[i].str);
		for (i = 0; i < nitems(cpu_seff0_edxfeatures); i++)
			if (ci->ci_feature_sefflags_edx &
			    cpu_seff0_edxfeatures[i].bit)
				printf(",%s", cpu_seff0_edxfeatures[i].str);
	}

	if (!strcmp(cpu_vendor, "GenuineIntel") && cpuid_level >= 0x06) {
		CPUID(0x06, ci->ci_feature_tpmflags, dummy, dummy, dummy);
		for (i = 0; i < nitems(cpu_tpm_eaxfeatures); i++)
			if (ci->ci_feature_tpmflags &
			    cpu_tpm_eaxfeatures[i].bit)
				printf(",%s", cpu_tpm_eaxfeatures[i].str);
	} else if (!strcmp(cpu_vendor, "AuthenticAMD")) {
		if (ci->ci_family >= 0x12)
			ci->ci_feature_tpmflags |= TPM_ARAT;
	}

	/* AMD speculation control features */
	if (!strcmp(cpu_vendor, "AuthenticAMD")) {
		if (ci->ci_pnfeatset >= 0x80000008) {
			CPUID(0x80000008, dummy, ci->ci_feature_amdspec_ebx,
			    dummy, dummy);
			for (i = 0; i < nitems(cpu_amdspec_ebxfeatures); i++)
				if (ci->ci_feature_amdspec_ebx &
				    cpu_amdspec_ebxfeatures[i].bit)
					printf(",%s",
					    cpu_amdspec_ebxfeatures[i].str);
		}
	}

	/* xsave subfeatures */
	if (cpuid_level >= 0xd) {
		CPUID_LEAF(0xd, 1, val, dummy, dummy, dummy);
		for (i = 0; i < nitems(cpu_xsave_extfeatures); i++)
			if (val & cpu_xsave_extfeatures[i].bit)
				printf(",%s", cpu_xsave_extfeatures[i].str);
	}

	if (cpu_meltdown)
		printf(",MELTDOWN");

	printf("\n");

	replacemeltdown();
	x86_print_cacheinfo(ci);

	/*
	 * "Mitigation G-2" per AMD's Whitepaper "Software Techniques
	 * for Managing Speculation on AMD Processors"
	 *
	 * By setting MSR C001_1029[1]=1, LFENCE becomes a dispatch
	 * serializing instruction.
	 *
	 * This MSR is available on all AMD families >= 10h, except 11h
	 * where LFENCE is always serializing.
	 */
	if (!strcmp(cpu_vendor, "AuthenticAMD")) {
		if (ci->ci_family >= 0x10 && ci->ci_family != 0x11) {
			uint64_t msr;

			msr = rdmsr(MSR_DE_CFG);
			if ((msr & DE_CFG_SERIALIZE_LFENCE) == 0) {
				msr |= DE_CFG_SERIALIZE_LFENCE;
				wrmsr(MSR_DE_CFG, msr);
			}
		}
	}

	/*
	 * Attempt to disable Silicon Debug and lock the configuration
	 * if it's enabled and unlocked.
	 */
	if (!strcmp(cpu_vendor, "GenuineIntel") &&
	    (cpu_ecxfeature & CPUIDECX_SDBG)) {
		uint64_t msr;

		msr = rdmsr(IA32_DEBUG_INTERFACE);
		if ((msr & IA32_DEBUG_INTERFACE_ENABLE) &&
		    (msr & IA32_DEBUG_INTERFACE_LOCK) == 0) {
			msr &= IA32_DEBUG_INTERFACE_MASK;
			msr |= IA32_DEBUG_INTERFACE_LOCK;
			wrmsr(IA32_DEBUG_INTERFACE, msr);
		} else if (msr & IA32_DEBUG_INTERFACE_ENABLE)
			printf("%s: cannot disable silicon debug\n",
			    ci->ci_dev->dv_xname);
	}

	if (ci->ci_flags & CPUF_PRIMARY) {
#ifndef SMALL_KERNEL
		if (!strcmp(cpu_vendor, "AuthenticAMD") &&
		    ci->ci_pnfeatset >= 0x80000007) {
			CPUID(0x80000007, dummy, dummy, dummy, val);

			if (val & 0x06) {
				if ((ci->ci_signature & 0xF00) == 0xF00)
					setperf_setup = k8_powernow_init;
			}
			if (ci->ci_family >= 0x10)
				setperf_setup = k1x_init;
		}

		if (cpu_ecxfeature & CPUIDECX_EST)
			setperf_setup = est_init;
#endif

		if (cpu_ecxfeature & CPUIDECX_RDRAND)
			has_rdrand = 1;

		if (ci->ci_feature_sefflags_ebx & SEFF0EBX_RDSEED)
			has_rdseed = 1;

		if (ci->ci_feature_sefflags_ebx & SEFF0EBX_SMAP)
			replacesmap();
	}
#ifndef SMALL_KERNEL
	if (!strncmp(mycpu_model, "Intel", 5)) {
		u_int32_t cflushsz;

		CPUID(0x01, dummy, cflushsz, dummy, dummy);
		/* cflush cacheline size is equal to bits 15-8 of ebx * 8 */
		ci->ci_cflushsz = ((cflushsz >> 8) & 0xff) * 8;
	}

	if (CPU_IS_PRIMARY(ci) && (ci->ci_feature_tpmflags & TPM_SENSOR)) {
		strlcpy(ci->ci_sensordev.xname, ci->ci_dev->dv_xname,
		    sizeof(ci->ci_sensordev.xname));
		ci->ci_sensor.type = SENSOR_TEMP;
		sensor_task_register(ci, intelcore_update_sensor, 5);
		sensor_attach(&ci->ci_sensordev, &ci->ci_sensor);
		sensordev_install(&ci->ci_sensordev);
	}
#endif

#ifdef CRYPTO
	if (ci->ci_flags & CPUF_PRIMARY) {
		if (cpu_ecxfeature & CPUIDECX_PCLMUL)
			amd64_has_pclmul = 1;

		if (cpu_ecxfeature & CPUIDECX_AES)
			amd64_has_aesni = 1;
	}
#endif

	if (!strcmp(cpu_vendor, "AuthenticAMD"))
		amd64_errata(ci);

	if (CPU_IS_PRIMARY(ci) && !strcmp(cpu_vendor, "CentaurHauls")) {
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

	tsc_timecounter_init(ci, freq);

	cpu_topology(ci);
#if NVMM > 0
	cpu_check_vmm_cap(ci);
#endif /* NVMM > 0 */
}

#ifndef SMALL_KERNEL
/*
 * Base 2 logarithm of an int. returns 0 for 0 (yeye, I know).
 */
static int
log2(unsigned int i)
{
	int ret = 0;

	while (i >>= 1)
		ret++;

	return (ret);
}

static int
mask_width(u_int x)
{
	int bit;
	int mask;
	int powerof2;

	powerof2 = ((x - 1) & x) == 0;
	mask = (x << (1 - powerof2)) - 1;

	/* fls */
	if (mask == 0)
		return (0);
	for (bit = 1; mask != 1; bit++)
		mask = (unsigned int)mask >> 1;

	return (bit);
}
#endif

/*
 * Build up cpu topology for given cpu, must run on the core itself.
 */
void
cpu_topology(struct cpu_info *ci)
{
#ifndef SMALL_KERNEL
	u_int32_t eax, ebx, ecx, edx;
	u_int32_t apicid, max_apicid = 0, max_coreid = 0;
	u_int32_t smt_bits = 0, core_bits, pkg_bits = 0;
	u_int32_t smt_mask = 0, core_mask, pkg_mask = 0;

	/* We need at least apicid at CPUID 1 */
	if (cpuid_level < 1)
		goto no_topology;

	/* Initial apicid */
	CPUID(1, eax, ebx, ecx, edx);
	apicid = (ebx >> 24) & 0xff;

	if (strcmp(cpu_vendor, "AuthenticAMD") == 0) {
		/* We need at least apicid at CPUID 0x80000008 */
		if (ci->ci_pnfeatset < 0x80000008)
			goto no_topology;

		if (ci->ci_pnfeatset >= 0x8000001e) {
			struct cpu_info *ci_other;
			CPU_INFO_ITERATOR cii;

			CPUID(0x8000001e, eax, ebx, ecx, edx);
			ci->ci_core_id = ebx & 0xff;
			ci->ci_pkg_id = ecx & 0xff;
			ci->ci_smt_id = 0;
			CPU_INFO_FOREACH(cii, ci_other) {
				if (ci != ci_other &&
				    ci_other->ci_core_id == ci->ci_core_id &&
				    ci_other->ci_pkg_id == ci->ci_pkg_id)
					ci->ci_smt_id++;
			}
		} else {
			CPUID(0x80000008, eax, ebx, ecx, edx);
			core_bits = (ecx >> 12) & 0xf;
			if (core_bits == 0)
				goto no_topology;
			/* So coreidsize 2 gives 3, 3 gives 7... */
			core_mask = (1 << core_bits) - 1;
			/* Core id is the least significant considering mask */
			ci->ci_core_id = apicid & core_mask;
			/* Pkg id is the upper remaining bits */
			ci->ci_pkg_id = apicid & ~core_mask;
			ci->ci_pkg_id >>= core_bits;
		}
	} else if (strcmp(cpu_vendor, "GenuineIntel") == 0) {
		/* We only support leaf 1/4 detection */
		if (cpuid_level < 4)
			goto no_topology;
		/* Get max_apicid */
		CPUID(1, eax, ebx, ecx, edx);
		max_apicid = (ebx >> 16) & 0xff;
		/* Get max_coreid */
		CPUID_LEAF(4, 0, eax, ebx, ecx, edx);
		max_coreid = ((eax >> 26) & 0x3f) + 1;
		/* SMT */
		smt_bits = mask_width(max_apicid / max_coreid);
		smt_mask = (1 << smt_bits) - 1;
		/* Core */
		core_bits = log2(max_coreid);
		core_mask = (1 << (core_bits + smt_bits)) - 1;
		core_mask ^= smt_mask;
		/* Pkg */
		pkg_bits = core_bits + smt_bits;
		pkg_mask = -1 << core_bits;

		ci->ci_smt_id = apicid & smt_mask;
		ci->ci_core_id = (apicid & core_mask) >> smt_bits;
		ci->ci_pkg_id = (apicid & pkg_mask) >> pkg_bits;
	} else
		goto no_topology;
#ifdef DEBUG
	printf("cpu%d: smt %u, core %u, pkg %u "
		"(apicid 0x%x, max_apicid 0x%x, max_coreid 0x%x, smt_bits 0x%x, smt_mask 0x%x, "
		"core_bits 0x%x, core_mask 0x%x, pkg_bits 0x%x, pkg_mask 0x%x)\n",
		ci->ci_cpuid, ci->ci_smt_id, ci->ci_core_id, ci->ci_pkg_id,
		apicid, max_apicid, max_coreid, smt_bits, smt_mask, core_bits,
		core_mask, pkg_bits, pkg_mask);
#else
	printf("cpu%d: smt %u, core %u, package %u\n", ci->ci_cpuid,
		ci->ci_smt_id, ci->ci_core_id, ci->ci_pkg_id);

#endif
	return;
	/* We can't map, so consider ci_core_id as ci_cpuid */
no_topology:
#endif
	ci->ci_smt_id  = 0;
	ci->ci_core_id = ci->ci_cpuid;
	ci->ci_pkg_id  = 0;
}

#if NVMM > 0
/*
 * cpu_check_vmm_cap
 *
 * Checks for VMM capabilities for 'ci'. Initializes certain per-cpu VMM
 * state in 'ci' if virtualization extensions are found.
 *
 * Parameters:
 *  ci: the cpu being checked
 */
void
cpu_check_vmm_cap(struct cpu_info *ci)
{
	uint64_t msr;
	uint32_t cap, dummy, edx;

	/*
	 * Check for workable VMX
	 */
	if (cpu_ecxfeature & CPUIDECX_VMX) {
		msr = rdmsr(MSR_IA32_FEATURE_CONTROL);

		if (!(msr & IA32_FEATURE_CONTROL_LOCK))
			ci->ci_vmm_flags |= CI_VMM_VMX;
		else {
			if (msr & IA32_FEATURE_CONTROL_VMX_EN)
				ci->ci_vmm_flags |= CI_VMM_VMX;
			else
				ci->ci_vmm_flags |= CI_VMM_DIS;
		}
	}

	/*
	 * Check for EPT (Intel Nested Paging) and other secondary
	 * controls
	 */
	if (ci->ci_vmm_flags & CI_VMM_VMX) {
		/* Secondary controls available? */
		/* XXX should we check true procbased ctls here if avail? */
		msr = rdmsr(IA32_VMX_PROCBASED_CTLS);
		if (msr & (IA32_VMX_ACTIVATE_SECONDARY_CONTROLS) << 32) {
			msr = rdmsr(IA32_VMX_PROCBASED2_CTLS);
			/* EPT available? */
			if (msr & (IA32_VMX_ENABLE_EPT) << 32)
				ci->ci_vmm_flags |= CI_VMM_EPT;
			/* VM Functions available? */
			if (msr & (IA32_VMX_ENABLE_VM_FUNCTIONS) << 32) {
				ci->ci_vmm_cap.vcc_vmx.vmx_vm_func =
				    rdmsr(IA32_VMX_VMFUNC);	
			}
		}
	}

	/*
	 * Check startup config (VMX)
	 */
	if (ci->ci_vmm_flags & CI_VMM_VMX) {
		/* CR0 fixed and flexible bits */
		msr = rdmsr(IA32_VMX_CR0_FIXED0);
		ci->ci_vmm_cap.vcc_vmx.vmx_cr0_fixed0 = msr;
		msr = rdmsr(IA32_VMX_CR0_FIXED1);
		ci->ci_vmm_cap.vcc_vmx.vmx_cr0_fixed1 = msr;

		/* CR4 fixed and flexible bits */
		msr = rdmsr(IA32_VMX_CR4_FIXED0);
		ci->ci_vmm_cap.vcc_vmx.vmx_cr4_fixed0 = msr;
		msr = rdmsr(IA32_VMX_CR4_FIXED1);
		ci->ci_vmm_cap.vcc_vmx.vmx_cr4_fixed1 = msr;

		/* VMXON region revision ID (bits 30:0 of IA32_VMX_BASIC) */
		msr = rdmsr(IA32_VMX_BASIC);
		ci->ci_vmm_cap.vcc_vmx.vmx_vmxon_revision =
			(uint32_t)(msr & 0x7FFFFFFF);

		/* MSR save / load table size */
		msr = rdmsr(IA32_VMX_MISC);
		ci->ci_vmm_cap.vcc_vmx.vmx_msr_table_size =
			(uint32_t)(msr & IA32_VMX_MSR_LIST_SIZE_MASK) >> 25;

		/* CR3 target count size */
		ci->ci_vmm_cap.vcc_vmx.vmx_cr3_tgt_count =
			(uint32_t)(msr & IA32_VMX_CR3_TGT_SIZE_MASK) >> 16;
	}

	/*
	 * Check for workable SVM
	 */
	if (ecpu_ecxfeature & CPUIDECX_SVM) {
		msr = rdmsr(MSR_AMD_VM_CR);

		if (!(msr & AMD_SVMDIS))
			ci->ci_vmm_flags |= CI_VMM_SVM;

		CPUID(CPUID_AMD_SVM_CAP, dummy,
		    ci->ci_vmm_cap.vcc_svm.svm_max_asid, dummy, edx);

		if (ci->ci_vmm_cap.vcc_svm.svm_max_asid > 0xFFF)
			ci->ci_vmm_cap.vcc_svm.svm_max_asid = 0xFFF;

		if (edx & AMD_SVM_FLUSH_BY_ASID_CAP)
			ci->ci_vmm_cap.vcc_svm.svm_flush_by_asid = 1;

		if (edx & AMD_SVM_VMCB_CLEAN_CAP)
			ci->ci_vmm_cap.vcc_svm.svm_vmcb_clean = 1;
	}

	/*
	 * Check for SVM Nested Paging
	 */
	if ((ci->ci_vmm_flags & CI_VMM_SVM) &&
	    ci->ci_pnfeatset >= CPUID_AMD_SVM_CAP) {
		CPUID(CPUID_AMD_SVM_CAP, dummy, dummy, dummy, cap);
		if (cap & AMD_SVM_NESTED_PAGING_CAP)
			ci->ci_vmm_flags |= CI_VMM_RVI;
	}

	/*
	 * Check "L1 flush on VM entry" (Intel L1TF vuln) semantics
	 */
	if (!strcmp(cpu_vendor, "GenuineIntel")) {
		if (ci->ci_feature_sefflags_edx & SEFF0EDX_L1DF)
			ci->ci_vmm_cap.vcc_vmx.vmx_has_l1_flush_msr = 1;
		else
			ci->ci_vmm_cap.vcc_vmx.vmx_has_l1_flush_msr = 0;

		/*
		 * Certain CPUs may have the vulnerability remedied in
		 * hardware, check for that and override the setting
		 * calculated above.
		 */	
		if (ci->ci_feature_sefflags_edx & SEFF0EDX_ARCH_CAP) {
			msr = rdmsr(MSR_ARCH_CAPABILITIES);
			if (msr & ARCH_CAPABILITIES_SKIP_L1DFL_VMENTRY)
				ci->ci_vmm_cap.vcc_vmx.vmx_has_l1_flush_msr =
				    VMX_SKIP_L1D_FLUSH;
		}
	}
}
#endif /* NVMM > 0 */
