/*	$OpenBSD: cpu.c,v 1.66 2022/08/05 12:52:35 robert Exp $	*/

/*
 * Copyright (c) 2016 Dale Rahn <drahn@dalerahn.com>
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/sysctl.h>
#include <sys/task.h>
#include <sys/user.h>

#include <uvm/uvm.h>

#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/ofw_thermal.h>
#include <dev/ofw/fdt.h>

#include <machine/cpufunc.h>
#include <machine/fdt.h>

#include "psci.h"
#if NPSCI > 0
#include <dev/fdt/pscivar.h>
#endif

/* CPU Identification */
#define CPU_IMPL_ARM		0x41
#define CPU_IMPL_CAVIUM		0x43
#define CPU_IMPL_AMCC		0x50
#define CPU_IMPL_APPLE		0x61

/* ARM */
#define CPU_PART_CORTEX_A34	0xd02
#define CPU_PART_CORTEX_A53	0xd03
#define CPU_PART_CORTEX_A35	0xd04
#define CPU_PART_CORTEX_A55	0xd05
#define CPU_PART_CORTEX_A65	0xd06
#define CPU_PART_CORTEX_A57	0xd07
#define CPU_PART_CORTEX_A72	0xd08
#define CPU_PART_CORTEX_A73	0xd09
#define CPU_PART_CORTEX_A75	0xd0a
#define CPU_PART_CORTEX_A76	0xd0b
#define CPU_PART_NEOVERSE_N1	0xd0c
#define CPU_PART_CORTEX_A77	0xd0d
#define CPU_PART_CORTEX_A76AE	0xd0e
#define CPU_PART_NEOVERSE_V1	0xd40
#define CPU_PART_CORTEX_A78	0xd41
#define CPU_PART_CORTEX_A78AE	0xd42
#define CPU_PART_CORTEX_A65AE	0xd43
#define CPU_PART_CORTEX_X1	0xd44
#define CPU_PART_CORTEX_A510	0xd46
#define CPU_PART_CORTEX_A710	0xd47
#define CPU_PART_CORTEX_X2	0xd48
#define CPU_PART_NEOVERSE_N2	0xd49
#define CPU_PART_NEOVERSE_E1	0xd4a
#define CPU_PART_CORTEX_A78C	0xd4b
#define CPU_PART_CORTEX_X1C	0xd4c
#define CPU_PART_CORTEX_A715	0xd4d
#define CPU_PART_CORTEX_X3	0xd4e

/* Cavium */
#define CPU_PART_THUNDERX_T88	0x0a1
#define CPU_PART_THUNDERX_T81	0x0a2
#define CPU_PART_THUNDERX_T83	0x0a3
#define CPU_PART_THUNDERX2_T99	0x0af

/* Applied Micro */
#define CPU_PART_X_GENE		0x000

/* Apple */
#define CPU_PART_ICESTORM	0x022
#define CPU_PART_FIRESTORM	0x023
#define CPU_PART_ICESTORM_PRO	0x024
#define CPU_PART_FIRESTORM_PRO	0x025
#define CPU_PART_ICESTORM_MAX	0x028
#define CPU_PART_FIRESTORM_MAX	0x029

#define CPU_IMPL(midr)  (((midr) >> 24) & 0xff)
#define CPU_PART(midr)  (((midr) >> 4) & 0xfff)
#define CPU_VAR(midr)   (((midr) >> 20) & 0xf)
#define CPU_REV(midr)   (((midr) >> 0) & 0xf)

struct cpu_cores {
	int	id;
	char	*name;
};

struct cpu_cores cpu_cores_none[] = {
	{ 0, NULL },
};

struct cpu_cores cpu_cores_arm[] = {
	{ CPU_PART_CORTEX_A34, "Cortex-A34" },
	{ CPU_PART_CORTEX_A35, "Cortex-A35" },
	{ CPU_PART_CORTEX_A53, "Cortex-A53" },
	{ CPU_PART_CORTEX_A55, "Cortex-A55" },
	{ CPU_PART_CORTEX_A57, "Cortex-A57" },
	{ CPU_PART_CORTEX_A65, "Cortex-A65" },
	{ CPU_PART_CORTEX_A65AE, "Cortex-A65AE" },
	{ CPU_PART_CORTEX_A72, "Cortex-A72" },
	{ CPU_PART_CORTEX_A73, "Cortex-A73" },
	{ CPU_PART_CORTEX_A75, "Cortex-A75" },
	{ CPU_PART_CORTEX_A76, "Cortex-A76" },
	{ CPU_PART_CORTEX_A76AE, "Cortex-A76AE" },
	{ CPU_PART_CORTEX_A77, "Cortex-A77" },
	{ CPU_PART_CORTEX_A78, "Cortex-A78" },
	{ CPU_PART_CORTEX_A78AE, "Cortex-A78AE" },
	{ CPU_PART_CORTEX_A78C, "Cortex-A78C" },
	{ CPU_PART_CORTEX_A510, "Cortex-A510" },
	{ CPU_PART_CORTEX_A710, "Cortex-A710" },
	{ CPU_PART_CORTEX_A715, "Cortex-A715" },
	{ CPU_PART_CORTEX_X1, "Cortex-X1" },
	{ CPU_PART_CORTEX_X1C, "Cortex-X1C" },
	{ CPU_PART_CORTEX_X2, "Cortex-X2" },
	{ CPU_PART_CORTEX_X3, "Cortex-X3" },
	{ CPU_PART_NEOVERSE_E1, "Neoverse E1" },
	{ CPU_PART_NEOVERSE_N1, "Neoverse N1" },
	{ CPU_PART_NEOVERSE_N2, "Neoverse N2" },
	{ CPU_PART_NEOVERSE_V1, "Neoverse V1" },
	{ 0, NULL },
};

struct cpu_cores cpu_cores_cavium[] = {
	{ CPU_PART_THUNDERX_T88, "ThunderX T88" },
	{ CPU_PART_THUNDERX_T81, "ThunderX T81" },
	{ CPU_PART_THUNDERX_T83, "ThunderX T83" },
	{ CPU_PART_THUNDERX2_T99, "ThunderX2 T99" },
	{ 0, NULL },
};

struct cpu_cores cpu_cores_amcc[] = {
	{ CPU_PART_X_GENE, "X-Gene" },
	{ 0, NULL },
};

struct cpu_cores cpu_cores_apple[] = {
	{ CPU_PART_ICESTORM, "Icestorm" },
	{ CPU_PART_FIRESTORM, "Firestorm" },
	{ CPU_PART_ICESTORM_PRO, "Icestorm Pro" },
	{ CPU_PART_FIRESTORM_PRO, "Firestorm Pro" },
	{ CPU_PART_ICESTORM_MAX, "Icestorm Max" },
	{ CPU_PART_FIRESTORM_MAX, "Firestorm Max" },
	{ 0, NULL },
};

/* arm cores makers */
const struct implementers {
	int			id;
	char			*name;
	struct cpu_cores	*corelist;
} cpu_implementers[] = {
	{ CPU_IMPL_ARM,	"ARM", cpu_cores_arm },
	{ CPU_IMPL_CAVIUM, "Cavium", cpu_cores_cavium },
	{ CPU_IMPL_AMCC, "Applied Micro", cpu_cores_amcc },
	{ CPU_IMPL_APPLE, "Apple", cpu_cores_apple },
	{ 0, NULL },
};

char cpu_model[64];
int cpu_node;

uint64_t cpu_id_aa64isar0;
uint64_t cpu_id_aa64isar1;

#ifdef CRYPTO
int arm64_has_aes;
#endif

struct cpu_info *cpu_info_list = &cpu_info_primary;

int	cpu_match(struct device *, void *, void *);
void	cpu_attach(struct device *, struct device *, void *);

const struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

void	cpu_opp_init(struct cpu_info *, uint32_t);

void	cpu_flush_bp_noop(void);
void	cpu_flush_bp_psci(void);

void
cpu_identify(struct cpu_info *ci)
{
	uint64_t midr, impl, part;
	uint64_t clidr, id;
	uint32_t ctr, ccsidr, sets, ways, line;
	const char *impl_name = NULL;
	const char *part_name = NULL;
	const char *il1p_name = NULL;
	const char *sep;
	struct cpu_cores *coreselecter = cpu_cores_none;
	int i;

	midr = READ_SPECIALREG(midr_el1);
	impl = CPU_IMPL(midr);
	part = CPU_PART(midr);

	for (i = 0; cpu_implementers[i].name; i++) {
		if (impl == cpu_implementers[i].id) {
			impl_name = cpu_implementers[i].name;
			coreselecter = cpu_implementers[i].corelist;
			break;
		}
	}

	for (i = 0; coreselecter[i].name; i++) {
		if (part == coreselecter[i].id) {
			part_name = coreselecter[i].name;
			break;
		}
	}

	if (impl_name && part_name) {
		printf(" %s %s r%llup%llu", impl_name, part_name, CPU_VAR(midr),
		    CPU_REV(midr));

		if (CPU_IS_PRIMARY(ci))
			snprintf(cpu_model, sizeof(cpu_model),
			    "%s %s r%llup%llu", impl_name, part_name,
			    CPU_VAR(midr), CPU_REV(midr));
	} else {
		printf(" Unknown, MIDR 0x%llx", midr);

		if (CPU_IS_PRIMARY(ci))
			snprintf(cpu_model, sizeof(cpu_model), "Unknown");
	}

	/* Print cache information. */

	ctr = READ_SPECIALREG(ctr_el0);
	switch (ctr & CTR_IL1P_MASK) {
	case CTR_IL1P_AIVIVT:
		il1p_name = "AIVIVT ";
		break;
	case CTR_IL1P_VIPT:
		il1p_name = "VIPT ";
		break;
	case CTR_IL1P_PIPT:
		il1p_name = "PIPT ";
		break;
	}

	clidr = READ_SPECIALREG(clidr_el1);
	for (i = 0; i < 7; i++) {
		if ((clidr & CLIDR_CTYPE_MASK) == 0)
			break;
		printf("\n%s:", ci->ci_dev->dv_xname);
		sep = "";
		if (clidr & CLIDR_CTYPE_INSN) {
			WRITE_SPECIALREG(csselr_el1,
			    i << CSSELR_LEVEL_SHIFT | CSSELR_IND);
			__asm volatile("isb");
			ccsidr = READ_SPECIALREG(ccsidr_el1);
			sets = CCSIDR_SETS(ccsidr);
			ways = CCSIDR_WAYS(ccsidr);
			line = CCSIDR_LINE_SIZE(ccsidr);
			printf("%s %dKB %db/line %d-way L%d %sI-cache", sep,
			    (sets * ways * line) / 1024, line, ways, (i + 1),
			    il1p_name);
			il1p_name = "";
			sep = ",";
		}
		if (clidr & CLIDR_CTYPE_DATA) {
			WRITE_SPECIALREG(csselr_el1, i << CSSELR_LEVEL_SHIFT);
			__asm volatile("isb");
			ccsidr = READ_SPECIALREG(ccsidr_el1);
			sets = CCSIDR_SETS(ccsidr);
			ways = CCSIDR_WAYS(ccsidr);
			line = CCSIDR_LINE_SIZE(ccsidr);
			printf("%s %dKB %db/line %d-way L%d D-cache", sep,
			    (sets * ways * line) / 1024, line, ways, (i + 1));
			sep = ",";
		}
		if (clidr & CLIDR_CTYPE_UNIFIED) {
			WRITE_SPECIALREG(csselr_el1, i << CSSELR_LEVEL_SHIFT);
			__asm volatile("isb");
			ccsidr = READ_SPECIALREG(ccsidr_el1);
			sets = CCSIDR_SETS(ccsidr);
			ways = CCSIDR_WAYS(ccsidr);
			line = CCSIDR_LINE_SIZE(ccsidr);
			printf("%s %dKB %db/line %d-way L%d cache", sep,
			    (sets * ways * line) / 1024, line, ways, (i + 1));
		}
		clidr >>= 3;
	}

	/*
	 * Some ARM processors are vulnerable to branch target
	 * injection attacks (CVE-2017-5715).
	 */
	switch (impl) {
	case CPU_IMPL_ARM:
		switch (part) {
		case CPU_PART_CORTEX_A35:
		case CPU_PART_CORTEX_A53:
		case CPU_PART_CORTEX_A55:
			/* Not vulnerable. */
			ci->ci_flush_bp = cpu_flush_bp_noop;
			break;
		default:
			/*
			 * Potentially vulnerable; call into the
			 * firmware and hope we're running on top of
			 * Arm Trusted Firmware with a fix for
			 * Security Advisory TFV 6.
			 */
			ci->ci_flush_bp = cpu_flush_bp_psci;
			break;
		}
		break;
	default:
		/* Not much we can do for an unknown processor.  */
		ci->ci_flush_bp = cpu_flush_bp_noop;
		break;
	}

	/*
	 * The architecture has been updated to explicitly tell us if
	 * we're not vulnerable.
	 */

	id = READ_SPECIALREG(id_aa64pfr0_el1);
	if (ID_AA64PFR0_CSV2(id) >= ID_AA64PFR0_CSV2_IMPL)
		ci->ci_flush_bp = cpu_flush_bp_noop;

	/*
	 * Print CPU features encoded in the ID registers.
	 */

	if (READ_SPECIALREG(id_aa64isar0_el1) != cpu_id_aa64isar0) {
		printf("\n%s: mismatched ID_AA64ISAR0_EL1",
		    ci->ci_dev->dv_xname);
	}
	if (READ_SPECIALREG(id_aa64isar1_el1) != cpu_id_aa64isar1) {
		printf("\n%s: mismatched ID_AA64ISAR1_EL1",
		    ci->ci_dev->dv_xname);
	}

	printf("\n%s: ", ci->ci_dev->dv_xname);

	/*
	 * ID_AA64ISAR0
	 */
	id = READ_SPECIALREG(id_aa64isar0_el1);
	sep = "";

	if (ID_AA64ISAR0_RNDR(id) >= ID_AA64ISAR0_RNDR_IMPL) {
		printf("%sRNDR", sep);
		sep = ",";
	}

	if (ID_AA64ISAR0_TLB(id) >= ID_AA64ISAR0_TLB_IOS) {
		printf("%sTLBIOS", sep);
		sep = ",";
	}
	if (ID_AA64ISAR0_TLB(id) >= ID_AA64ISAR0_TLB_IRANGE)
		printf("+IRANGE");

	if (ID_AA64ISAR0_TS(id) >= ID_AA64ISAR0_TS_BASE) {
		printf("%sTS", sep);
		sep = ",";
	}
	if (ID_AA64ISAR0_TS(id) >= ID_AA64ISAR0_TS_AXFLAG)
		printf("+AXFLAG");

	if (ID_AA64ISAR0_FHM(id) >= ID_AA64ISAR0_FHM_IMPL) {
		printf("%sFHM", sep);
		sep = ",";
	}

	if (ID_AA64ISAR0_DP(id) >= ID_AA64ISAR0_DP_IMPL) {
		printf("%sDP", sep);
		sep = ",";
	}

	if (ID_AA64ISAR0_SM4(id) >= ID_AA64ISAR0_SM4_IMPL) {
		printf("%sSM4", sep);
		sep = ",";
	}

	if (ID_AA64ISAR0_SM3(id) >= ID_AA64ISAR0_SM3_IMPL) {
		printf("%sSM3", sep);
		sep = ",";
	}

	if (ID_AA64ISAR0_SHA3(id) >= ID_AA64ISAR0_SHA3_IMPL) {
		printf("%sSHA3", sep);
		sep = ",";
	}

	if (ID_AA64ISAR0_RDM(id) >= ID_AA64ISAR0_RDM_IMPL) {
		printf("%sRDM", sep);
		sep = ",";
	}

	if (ID_AA64ISAR0_ATOMIC(id) >= ID_AA64ISAR0_ATOMIC_IMPL) {
		printf("%sAtomic", sep);
		sep = ",";
	}

	if (ID_AA64ISAR0_CRC32(id) >= ID_AA64ISAR0_CRC32_BASE) {
		printf("%sCRC32", sep);
		sep = ",";
	}

	if (ID_AA64ISAR0_SHA2(id) >= ID_AA64ISAR0_SHA2_BASE) {
		printf("%sSHA2", sep);
		sep = ",";
	}
	if (ID_AA64ISAR0_SHA2(id) >= ID_AA64ISAR0_SHA2_512)
		printf("+SHA512");

	if (ID_AA64ISAR0_SHA1(id) >= ID_AA64ISAR0_SHA1_BASE) {
		printf("%sSHA1", sep);
		sep = ",";
	}

	if (ID_AA64ISAR0_AES(id) >= ID_AA64ISAR0_AES_BASE) {
		printf("%sAES", sep);
		sep = ",";
#ifdef CRYPTO
		arm64_has_aes = 1;
#endif
	}
	if (ID_AA64ISAR0_AES(id) >= ID_AA64ISAR0_AES_PMULL)
		printf("+PMULL");

	/*
	 * ID_AA64ISAR1
	 */
	id = READ_SPECIALREG(id_aa64isar1_el1);

	if (ID_AA64ISAR1_SPECRES(id) >= ID_AA64ISAR1_SPECRES_IMPL) {
		printf("%sSPECRES", sep);
		sep = ",";
	}

	if (ID_AA64ISAR1_SB(id) >= ID_AA64ISAR1_SB_IMPL) {
		printf("%sSB", sep);
		sep = ",";
	}

	if (ID_AA64ISAR1_FRINTTS(id) >= ID_AA64ISAR1_FRINTTS_IMPL) {
		printf("%sFRINTTS", sep);
		sep = ",";
	}

	if (ID_AA64ISAR1_GPI(id) >= ID_AA64ISAR1_GPI_IMPL) {
		printf("%sGPI", sep);
		sep = ",";
	}

	if (ID_AA64ISAR1_GPA(id) >= ID_AA64ISAR1_GPA_IMPL) {
		printf("%sGPA", sep);
		sep = ",";
	}

	if (ID_AA64ISAR1_LRCPC(id) >= ID_AA64ISAR1_LRCPC_BASE) {
		printf("%sLRCPC", sep);
		sep = ",";
	}
	if (ID_AA64ISAR1_LRCPC(id) >= ID_AA64ISAR1_LRCPC_LDAPUR)
		printf("+LDAPUR");

	if (ID_AA64ISAR1_FCMA(id) >= ID_AA64ISAR1_FCMA_IMPL) {
		printf("%sFCMA", sep);
		sep = ",";
	}

	if (ID_AA64ISAR1_JSCVT(id) >= ID_AA64ISAR1_JSCVT_IMPL) {
		printf("%sJSCVT", sep);
		sep = ",";
	}

	if (ID_AA64ISAR1_API(id) >= ID_AA64ISAR1_API_BASE) {
		printf("%sAPI", sep);
		sep = ",";
	}
	if (ID_AA64ISAR1_API(id) >= ID_AA64ISAR1_API_PAC)
		printf("+PAC");

	if (ID_AA64ISAR1_APA(id) >= ID_AA64ISAR1_APA_BASE) {
		printf("%sAPA", sep);
		sep = ",";
	}
	if (ID_AA64ISAR1_APA(id) >= ID_AA64ISAR1_APA_PAC)
		printf("+PAC");

	if (ID_AA64ISAR1_DPB(id) >= ID_AA64ISAR1_DPB_IMPL) {
		printf("%sDPB", sep);
		sep = ",";
	}

	/*
	 * ID_AA64MMFR0
	 *
	 * We only print ASIDBits for now.
	 */
	id = READ_SPECIALREG(id_aa64mmfr0_el1);

	if (ID_AA64MMFR0_ASID_BITS(id) == ID_AA64MMFR0_ASID_BITS_16) {
		printf("%sASID16", sep);
		sep = ",";
	}

	/*
	 * ID_AA64MMFR1
	 *
	 * We omit printing virtualization related fields like XNX, VH
	 * and VMIDBits as they are not really relevant for us.
	 */
	id = READ_SPECIALREG(id_aa64mmfr1_el1);

	if (ID_AA64MMFR1_SPECSEI(id) >= ID_AA64MMFR1_SPECSEI_IMPL) {
		printf("%sSpecSEI", sep);
		sep = ",";
	}

	if (ID_AA64MMFR1_PAN(id) >= ID_AA64MMFR1_PAN_IMPL) {
		printf("%sPAN", sep);
		sep = ",";
	}
	if (ID_AA64MMFR1_PAN(id) >= ID_AA64MMFR1_PAN_ATS1E1)
		printf("+ATS1E1");

	if (ID_AA64MMFR1_LO(id) >= ID_AA64MMFR1_LO_IMPL) {
		printf("%sLO", sep);
		sep = ",";
	}

	if (ID_AA64MMFR1_HPDS(id) >= ID_AA64MMFR1_HPDS_IMPL) {
		printf("%sHPDS", sep);
		sep = ",";
	}

	if (ID_AA64MMFR1_HAFDBS(id) >= ID_AA64MMFR1_HAFDBS_AF) {
		printf("%sHAF", sep);
		sep = ",";
	}
	if (ID_AA64MMFR1_HAFDBS(id) >= ID_AA64MMFR1_HAFDBS_AF_DBS)
		printf("DBS");

	/*
	 * ID_AA64PFR0
	 */
	id = READ_SPECIALREG(id_aa64pfr0_el1);

	if (ID_AA64PFR0_CSV3(id) >= ID_AA64PFR0_CSV3_IMPL) {
		printf("%sCSV3", sep);
		sep = ",";
	}

	if (ID_AA64PFR0_CSV2(id) >= ID_AA64PFR0_CSV2_IMPL) {
		printf("%sCSV2", sep);
		sep = ",";
	}
	if (ID_AA64PFR0_CSV2(id) >= ID_AA64PFR0_CSV2_SCXT)
		printf("+SCTX");

#ifdef CPU_DEBUG
	id = READ_SPECIALREG(id_aa64afr0_el1);
	printf("\nID_AA64AFR0_EL1: 0x%016llx", id);
	id = READ_SPECIALREG(id_aa64afr1_el1);
	printf("\nID_AA64AFR1_EL1: 0x%016llx", id);
	id = READ_SPECIALREG(id_aa64dfr0_el1);
	printf("\nID_AA64DFR0_EL1: 0x%016llx", id);
	id = READ_SPECIALREG(id_aa64dfr1_el1);
	printf("\nID_AA64DFR1_EL1: 0x%016llx", id);
	id = READ_SPECIALREG(id_aa64isar0_el1);
	printf("\nID_AA64ISAR0_EL1: 0x%016llx", id);
	id = READ_SPECIALREG(id_aa64isar1_el1);
	printf("\nID_AA64ISAR1_EL1: 0x%016llx", id);
	id = READ_SPECIALREG(id_aa64mmfr0_el1);
	printf("\nID_AA64MMFR0_EL1: 0x%016llx", id);
	id = READ_SPECIALREG(id_aa64mmfr1_el1);
	printf("\nID_AA64MMFR1_EL1: 0x%016llx", id);
	id = READ_SPECIALREG(id_aa64mmfr2_el1);
	printf("\nID_AA64MMFR2_EL1: 0x%016llx", id);
	id = READ_SPECIALREG(id_aa64pfr0_el1);
	printf("\nID_AA64PFR0_EL1: 0x%016llx", id);
	id = READ_SPECIALREG(id_aa64pfr1_el1);
	printf("\nID_AA64PFR1_EL1: 0x%016llx", id);
#endif
}

void	cpu_init(void);
int	cpu_start_secondary(struct cpu_info *ci, int, uint64_t);
int	cpu_clockspeed(int *);

int
cpu_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;
	uint64_t mpidr = READ_SPECIALREG(mpidr_el1);
	char buf[32];

	if (OF_getprop(faa->fa_node, "device_type", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "cpu") != 0)
		return 0;

	if (ncpus < MAXCPUS || faa->fa_reg[0].addr == (mpidr & MPIDR_AFF))
		return 1;

	return 0;
}

void
cpu_attach(struct device *parent, struct device *dev, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct cpu_info *ci;
	void *kstack;
#ifdef MULTIPROCESSOR
	uint64_t mpidr = READ_SPECIALREG(mpidr_el1);
#endif
	uint32_t opp;

	KASSERT(faa->fa_nreg > 0);

#ifdef MULTIPROCESSOR
	if (faa->fa_reg[0].addr == (mpidr & MPIDR_AFF)) {
		ci = &cpu_info_primary;
		ci->ci_flags |= CPUF_RUNNING | CPUF_PRESENT | CPUF_PRIMARY;
	} else {
		ci = malloc(sizeof(*ci), M_DEVBUF, M_WAITOK | M_ZERO);
		cpu_info[dev->dv_unit] = ci;
		ci->ci_next = cpu_info_list->ci_next;
		cpu_info_list->ci_next = ci;
		ci->ci_flags |= CPUF_AP;
		ncpus++;
	}
#else
	ci = &cpu_info_primary;
#endif

	ci->ci_dev = dev;
	ci->ci_cpuid = dev->dv_unit;
	ci->ci_mpidr = faa->fa_reg[0].addr;
	ci->ci_node = faa->fa_node;
	ci->ci_self = ci;

	printf(" mpidr %llx:", ci->ci_mpidr);

	kstack = km_alloc(USPACE, &kv_any, &kp_zero, &kd_waitok);
	ci->ci_el1_stkend = (vaddr_t)kstack + USPACE - 16;

#ifdef MULTIPROCESSOR
	if (ci->ci_flags & CPUF_AP) {
		char buf[32];
		uint64_t spinup_data = 0;
		int spinup_method = 0;
		int timeout = 10000;
		int len;

		len = OF_getprop(ci->ci_node, "enable-method",
		    buf, sizeof(buf));
		if (strcmp(buf, "psci") == 0) {
			spinup_method = 1;
		} else if (strcmp(buf, "spin-table") == 0) {
			spinup_method = 2;
			spinup_data = OF_getpropint64(ci->ci_node,
			    "cpu-release-addr", 0);
		}

		sched_init_cpu(ci);
		if (cpu_start_secondary(ci, spinup_method, spinup_data)) {
			atomic_setbits_int(&ci->ci_flags, CPUF_IDENTIFY);
			__asm volatile("dsb sy; sev" ::: "memory");

			while ((ci->ci_flags & CPUF_IDENTIFIED) == 0 &&
			    --timeout)
				delay(1000);
			if (timeout == 0) {
				printf(" failed to identify");
				ci->ci_flags = 0;
			}
		} else {
			printf(" failed to spin up");
			ci->ci_flags = 0;
		}
	} else {
#endif
		cpu_id_aa64isar0 = READ_SPECIALREG(id_aa64isar0_el1);
		cpu_id_aa64isar1 = READ_SPECIALREG(id_aa64isar1_el1);

		cpu_identify(ci);

		if (OF_getproplen(ci->ci_node, "clocks") > 0) {
			cpu_node = ci->ci_node;
			cpu_cpuspeed = cpu_clockspeed;
		}

		cpu_init();
#ifdef MULTIPROCESSOR
	}
#endif

	opp = OF_getpropint(ci->ci_node, "operating-points-v2", 0);
	if (opp)
		cpu_opp_init(ci, opp);

	printf("\n");
}

void
cpu_init(void)
{
	uint64_t id_aa64mmfr1, sctlr;
	uint64_t tcr;

	WRITE_SPECIALREG(ttbr0_el1, pmap_kernel()->pm_pt0pa);
	__asm volatile("isb");
	tcr = READ_SPECIALREG(tcr_el1);
	tcr &= ~TCR_T0SZ(0x3f);
	tcr |= TCR_T0SZ(64 - USER_SPACE_BITS);
	tcr |= TCR_A1;
	WRITE_SPECIALREG(tcr_el1, tcr);
	cpu_tlb_flush();

	/* Enable PAN. */
	id_aa64mmfr1 = READ_SPECIALREG(id_aa64mmfr1_el1);
	if (ID_AA64MMFR1_PAN(id_aa64mmfr1) >= ID_AA64MMFR1_PAN_IMPL) {
		sctlr = READ_SPECIALREG(sctlr_el1);
		sctlr &= ~SCTLR_SPAN;
		WRITE_SPECIALREG(sctlr_el1, sctlr);
	}

	/* Initialize debug registers. */
	WRITE_SPECIALREG(mdscr_el1, DBG_MDSCR_TDCC);
	WRITE_SPECIALREG(oslar_el1, 0);
}

void
cpu_flush_bp_noop(void)
{
}

void
cpu_flush_bp_psci(void)
{
#if NPSCI > 0
	psci_flush_bp();
#endif
}

int
cpu_clockspeed(int *freq)
{
	*freq = clock_get_frequency(cpu_node, NULL) / 1000000;
	return 0;
}

#ifdef MULTIPROCESSOR

void cpu_boot_secondary(struct cpu_info *ci);
void cpu_hatch_secondary(void);

void
cpu_boot_secondary_processors(void)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) {
		if ((ci->ci_flags & CPUF_AP) == 0)
			continue;
		if (ci->ci_flags & CPUF_PRIMARY)
			continue;

		ci->ci_randseed = (arc4random() & 0x7fffffff) + 1;
		cpu_boot_secondary(ci);
	}
}

void
cpu_start_spin_table(struct cpu_info *ci, uint64_t start, uint64_t data)
{
	/* this reuses the zero page for the core */
	vaddr_t start_pg = zero_page + (PAGE_SIZE * ci->ci_cpuid);
	paddr_t pa = trunc_page(data);
	uint64_t offset = data - pa;
	uint64_t *startvec = (uint64_t *)(start_pg + offset);

	pmap_kenter_cache(start_pg, pa, PROT_READ|PROT_WRITE, PMAP_CACHE_CI);

	*startvec = start;
	__asm volatile("dsb sy; sev" ::: "memory");

	pmap_kremove(start_pg, PAGE_SIZE);
}

int
cpu_start_secondary(struct cpu_info *ci, int method, uint64_t data)
{
	extern uint64_t pmap_avail_kvo;
	extern paddr_t cpu_hatch_ci;
	paddr_t startaddr;
	uint64_t ttbr1;
	int rc = 0;

	pmap_extract(pmap_kernel(), (vaddr_t)ci, &cpu_hatch_ci);

	__asm("mrs %x0, ttbr1_el1": "=r"(ttbr1));
	ci->ci_ttbr1 = ttbr1;

	cpu_dcache_wb_range((vaddr_t)&cpu_hatch_ci, sizeof(paddr_t));
	cpu_dcache_wb_range((vaddr_t)ci, sizeof(*ci));

	startaddr = (vaddr_t)cpu_hatch_secondary + pmap_avail_kvo;

	switch (method) {
	case 1:
		/* psci  */
#if NPSCI > 0
		rc = (psci_cpu_on(ci->ci_mpidr, startaddr, 0) == PSCI_SUCCESS);
#endif
		break;
	case 2:
		/* spin-table */
		cpu_start_spin_table(ci, startaddr, data);
		rc = 1;
		break;
	default:
		/* no method to spin up CPU */
		ci->ci_flags = 0;	/* mark cpu as not AP */
	}

	return rc;
}

void
cpu_boot_secondary(struct cpu_info *ci)
{
	atomic_setbits_int(&ci->ci_flags, CPUF_GO);
	__asm volatile("dsb sy; sev" ::: "memory");

	while ((ci->ci_flags & CPUF_RUNNING) == 0)
		__asm volatile("wfe");
}

void
cpu_init_secondary(struct cpu_info *ci)
{
	int s;

	ci->ci_flags |= CPUF_PRESENT;
	__asm volatile("dsb sy" ::: "memory");

	if ((ci->ci_flags & CPUF_IDENTIFIED) == 0) {
		while ((ci->ci_flags & CPUF_IDENTIFY) == 0)
			__asm volatile("wfe");

		cpu_identify(ci);
		atomic_setbits_int(&ci->ci_flags, CPUF_IDENTIFIED);
		__asm volatile("dsb sy" ::: "memory");
	}

	while ((ci->ci_flags & CPUF_GO) == 0)
		__asm volatile("wfe");

	cpu_init();

	s = splhigh();
	arm_intr_cpu_enable();
	cpu_startclock();

	nanouptime(&ci->ci_schedstate.spc_runtime);

	atomic_setbits_int(&ci->ci_flags, CPUF_RUNNING);
	__asm volatile("dsb sy; sev" ::: "memory");

	spllower(IPL_NONE);

	SCHED_LOCK(s);
	cpu_switchto(NULL, sched_chooseproc());
}

void
cpu_halt(void)
{
	struct cpu_info *ci = curcpu();

	KERNEL_ASSERT_UNLOCKED();
	SCHED_ASSERT_UNLOCKED();

	intr_disable();
	ci->ci_flags &= ~CPUF_RUNNING;
#if NPSCI > 0
	psci_cpu_off();
#endif
	for (;;)
		__asm volatile("wfi");
	/* NOTREACHED */
}

void
cpu_kick(struct cpu_info *ci)
{
	/* force cpu to enter kernel */
	if (ci != curcpu())
		arm_send_ipi(ci, ARM_IPI_NOP);
}

void
cpu_unidle(struct cpu_info *ci)
{
	/*
	 * This could send IPI or SEV depending on if the other
	 * processor is sleeping (WFI or WFE), in userland, or if the
	 * cpu is in other possible wait states?
	 */
	if (ci != curcpu())
		arm_send_ipi(ci, ARM_IPI_NOP);
}

#endif

#ifdef SUSPEND

void cpu_hatch_primary(void);

label_t cpu_suspend_jmpbuf;
int cpu_suspended;

void
cpu_init_primary(void)
{
	cpu_init();

	cpu_startclock();

	cpu_suspended = 1;
	longjmp(&cpu_suspend_jmpbuf);
}

int
cpu_suspend_primary(void)
{
	extern uint64_t pmap_avail_kvo;
	struct cpu_info *ci = curcpu();
	paddr_t startaddr, data;
	uint64_t ttbr1;

	cpu_suspended = 0;
	setjmp(&cpu_suspend_jmpbuf);
	if (cpu_suspended) {
		/* XXX wait for debug output from SCP on Allwinner A64 */
		delay(200000);
		return 0;
	}

	pmap_extract(pmap_kernel(), (vaddr_t)ci, &data);

	__asm("mrs %x0, ttbr1_el1": "=r"(ttbr1));
	ci->ci_ttbr1 = ttbr1;

	cpu_dcache_wb_range((vaddr_t)&data, sizeof(paddr_t));
	cpu_dcache_wb_range((vaddr_t)ci, sizeof(*ci));

	startaddr = (vaddr_t)cpu_hatch_primary + pmap_avail_kvo;

#if NPSCI > 0
	psci_system_suspend(startaddr, data);
#endif

	return EOPNOTSUPP;
}

#ifdef MULTIPROCESSOR

void
cpu_resume_secondary(struct cpu_info *ci)
{
	struct proc *p;
	struct pcb *pcb;
	struct trapframe *tf;
	struct switchframe *sf;
	int timeout = 10000;

	ci->ci_curproc = NULL;
	ci->ci_curpcb = NULL;
	ci->ci_curpm = NULL;
	ci->ci_cpl = IPL_NONE;
	ci->ci_ipending = 0;
	ci->ci_idepth = 0;
	ci->ci_flags &= ~CPUF_PRESENT;

#ifdef DIAGNOSTIC
	ci->ci_mutex_level = 0;
#endif
	ci->ci_ttbr1 = 0;

	p = ci->ci_schedstate.spc_idleproc;
	pcb = &p->p_addr->u_pcb;

	tf = (struct trapframe *)((u_long)p->p_addr
	    + USPACE
	    - sizeof(struct trapframe)
	    - 0x10);

	tf = (struct trapframe *)STACKALIGN(tf);
	pcb->pcb_tf = tf;

	sf = (struct switchframe *)tf - 1;
	sf->sf_x19 = (uint64_t)sched_idle;
	sf->sf_x20 = (uint64_t)ci;
	sf->sf_lr = (uint64_t)proc_trampoline;
	pcb->pcb_sp = (uint64_t)sf;

	cpu_start_secondary(ci, 1, 0);
	while ((ci->ci_flags & CPUF_PRESENT) == 0 && --timeout)
		delay(1000);
	if (timeout == 0) {
		printf("%s: failed to spin up\n",
		    ci->ci_dev->dv_xname);
		ci->ci_flags = 0;
	}
}

#endif

#endif

/*
 * Dynamic voltage and frequency scaling implementation.
 */

extern int perflevel;

struct opp {
	uint64_t opp_hz;
	uint32_t opp_microvolt;
};

struct opp_table {
	LIST_ENTRY(opp_table) ot_list;
	uint32_t ot_phandle;

	struct opp *ot_opp;
	u_int ot_nopp;
	uint64_t ot_opp_hz_min;
	uint64_t ot_opp_hz_max;

	struct cpu_info *ot_master;
};

LIST_HEAD(, opp_table) opp_tables = LIST_HEAD_INITIALIZER(opp_tables);
struct task cpu_opp_task;

void	cpu_opp_mountroot(struct device *);
void	cpu_opp_dotask(void *);
void	cpu_opp_setperf(int);

uint32_t cpu_opp_get_cooling_level(void *, uint32_t *);
void	cpu_opp_set_cooling_level(void *, uint32_t *, uint32_t);

void
cpu_opp_init(struct cpu_info *ci, uint32_t phandle)
{
	struct opp_table *ot;
	struct cooling_device *cd;
	int count, node, child;
	uint32_t opp_hz, opp_microvolt;
	uint32_t values[3];
	int i, j, len;

	LIST_FOREACH(ot, &opp_tables, ot_list) {
		if (ot->ot_phandle == phandle) {
			ci->ci_opp_table = ot;
			return;
		}
	}

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return;

	if (!OF_is_compatible(node, "operating-points-v2"))
		return;

	count = 0;
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (OF_getproplen(child, "turbo-mode") == 0)
			continue;
		count++;
	}
	if (count == 0)
		return;

	ot = malloc(sizeof(struct opp_table), M_DEVBUF, M_ZERO | M_WAITOK);
	ot->ot_phandle = phandle;
	ot->ot_opp = mallocarray(count, sizeof(struct opp),
	    M_DEVBUF, M_ZERO | M_WAITOK);
	ot->ot_nopp = count;

	count = 0;
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (OF_getproplen(child, "turbo-mode") == 0)
			continue;
		opp_hz = OF_getpropint64(child, "opp-hz", 0);
		len = OF_getpropintarray(child, "opp-microvolt",
		    values, sizeof(values));
		opp_microvolt = 0;
		if (len == sizeof(uint32_t) || len == 3 * sizeof(uint32_t))
			opp_microvolt = values[0];

		/* Insert into the array, keeping things sorted. */
		for (i = 0; i < count; i++) {
			if (opp_hz < ot->ot_opp[i].opp_hz)
				break;
		}
		for (j = count; j > i; j--)
			ot->ot_opp[j] = ot->ot_opp[j - 1];
		ot->ot_opp[i].opp_hz = opp_hz;
		ot->ot_opp[i].opp_microvolt = opp_microvolt;
		count++;
	}

	ot->ot_opp_hz_min = ot->ot_opp[0].opp_hz;
	ot->ot_opp_hz_max = ot->ot_opp[count - 1].opp_hz;

	if (OF_getproplen(node, "opp-shared") == 0)
		ot->ot_master = ci;

	LIST_INSERT_HEAD(&opp_tables, ot, ot_list);

	ci->ci_opp_table = ot;
	ci->ci_opp_max = ot->ot_nopp - 1;
	ci->ci_cpu_supply = OF_getpropint(ci->ci_node, "cpu-supply", 0);

	cd = malloc(sizeof(struct cooling_device), M_DEVBUF, M_ZERO | M_WAITOK);
	cd->cd_node = ci->ci_node;
	cd->cd_cookie = ci;
	cd->cd_get_level = cpu_opp_get_cooling_level;
	cd->cd_set_level = cpu_opp_set_cooling_level;
	cooling_device_register(cd);

	/*
	 * Do additional checks at mountroot when all the clocks and
	 * regulators are available.
	 */
	config_mountroot(ci->ci_dev, cpu_opp_mountroot);
}

void
cpu_opp_mountroot(struct device *self)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;
	int count = 0;
	int level = 0;

	if (cpu_setperf)
		return;

	CPU_INFO_FOREACH(cii, ci) {
		struct opp_table *ot = ci->ci_opp_table;
		uint64_t curr_hz;
		uint32_t curr_microvolt;
		int error;

		if (ot == NULL)
			continue;

		/* Skip if this table is shared and we're not the master. */
		if (ot->ot_master && ot->ot_master != ci)
			continue;

		/* PWM regulators may need to be explicitly enabled. */
		regulator_enable(ci->ci_cpu_supply);

		curr_hz = clock_get_frequency(ci->ci_node, NULL);
		curr_microvolt = regulator_get_voltage(ci->ci_cpu_supply);

		/* Disable if clock isn't implemented. */
		error = ENODEV;
		if (curr_hz != 0)
			error = clock_set_frequency(ci->ci_node, NULL, curr_hz);
		if (error) {
			ci->ci_opp_table = NULL;
			printf("%s: clock not implemented\n",
			       ci->ci_dev->dv_xname);
			continue;
		}

		/* Disable if regulator isn't implemented. */
		error = ci->ci_cpu_supply ? ENODEV : 0;
		if (ci->ci_cpu_supply && curr_microvolt != 0)
			error = regulator_set_voltage(ci->ci_cpu_supply,
			    curr_microvolt);
		if (error) {
			ci->ci_opp_table = NULL;
			printf("%s: regulator not implemented\n",
			    ci->ci_dev->dv_xname);
			continue;
		}

		/*
		 * Initialize performance level based on the current
		 * speed of the first CPU that supports DVFS.
		 */
		if (level == 0) {
			uint64_t min, max;
			uint64_t level_hz;

			min = ot->ot_opp_hz_min;
			max = ot->ot_opp_hz_max;
			level_hz = clock_get_frequency(ci->ci_node, NULL);
			level = howmany(100 * (level_hz - min), (max - min));
		}

		count++;
	}

	if (count > 0) {
		task_set(&cpu_opp_task, cpu_opp_dotask, NULL);
		cpu_setperf = cpu_opp_setperf;

		perflevel = (level > 0) ? level : 0;
		cpu_setperf(perflevel);
	}
}

void
cpu_opp_dotask(void *arg)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) {
		struct opp_table *ot = ci->ci_opp_table;
		uint64_t curr_hz, opp_hz;
		uint32_t curr_microvolt, opp_microvolt;
		int opp_idx;
		int error = 0;

		if (ot == NULL)
			continue;

		/* Skip if this table is shared and we're not the master. */
		if (ot->ot_master && ot->ot_master != ci)
			continue;

		opp_idx = MIN(ci->ci_opp_idx, ci->ci_opp_max);
		opp_hz = ot->ot_opp[opp_idx].opp_hz;
		opp_microvolt = ot->ot_opp[opp_idx].opp_microvolt;

		curr_hz = clock_get_frequency(ci->ci_node, NULL);
		curr_microvolt = regulator_get_voltage(ci->ci_cpu_supply);

		if (error == 0 && opp_hz < curr_hz)
			error = clock_set_frequency(ci->ci_node, NULL, opp_hz);
		if (error == 0 && ci->ci_cpu_supply &&
		    opp_microvolt != 0 && opp_microvolt != curr_microvolt) {
			error = regulator_set_voltage(ci->ci_cpu_supply,
			    opp_microvolt);
		}
		if (error == 0 && opp_hz > curr_hz)
			error = clock_set_frequency(ci->ci_node, NULL, opp_hz);

		if (error)
			printf("%s: DVFS failed\n", ci->ci_dev->dv_xname);
	}
}

void
cpu_opp_setperf(int level)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) {
		struct opp_table *ot = ci->ci_opp_table;
		uint64_t min, max;
		uint64_t level_hz, opp_hz;
		int opp_idx = -1;
		int i;

		if (ot == NULL)
			continue;

		/* Skip if this table is shared and we're not the master. */
		if (ot->ot_master && ot->ot_master != ci)
			continue;

		min = ot->ot_opp_hz_min;
		max = ot->ot_opp_hz_max;
		level_hz = min + (level * (max - min)) / 100;
		opp_hz = min;
		for (i = 0; i < ot->ot_nopp; i++) {
			if (ot->ot_opp[i].opp_hz <= level_hz &&
			    ot->ot_opp[i].opp_hz >= opp_hz)
				opp_hz = ot->ot_opp[i].opp_hz;
		}

		/* Find index of selected operating point. */
		for (i = 0; i < ot->ot_nopp; i++) {
			if (ot->ot_opp[i].opp_hz == opp_hz) {
				opp_idx = i;
				break;
			}
		}
		KASSERT(opp_idx >= 0);

		ci->ci_opp_idx = opp_idx;
	}

	/*
	 * Update the hardware from a task since setting the
	 * regulators might need process context.
	 */
	task_add(systq, &cpu_opp_task);
}

uint32_t
cpu_opp_get_cooling_level(void *cookie, uint32_t *cells)
{
	struct cpu_info *ci = cookie;
	struct opp_table *ot = ci->ci_opp_table;
	
	return ot->ot_nopp - ci->ci_opp_max - 1;
}

void
cpu_opp_set_cooling_level(void *cookie, uint32_t *cells, uint32_t level)
{
	struct cpu_info *ci = cookie;
	struct opp_table *ot = ci->ci_opp_table;
	int opp_max;

	if (level > (ot->ot_nopp - 1))
		level = ot->ot_nopp - 1;

	opp_max = (ot->ot_nopp - level - 1);
	if (ci->ci_opp_max != opp_max) {
		ci->ci_opp_max = opp_max;
		task_add(systq, &cpu_opp_task);
	}
}
