/*	$OpenBSD: cpu.c,v 1.25 2018/12/31 18:00:53 kettenis Exp $	*/

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

#include <uvm/uvm.h>

#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_regulator.h>
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

#define CPU_PART_CORTEX_A53	0xd03
#define CPU_PART_CORTEX_A35	0xd04
#define CPU_PART_CORTEX_A55	0xd05
#define CPU_PART_CORTEX_A57	0xd07
#define CPU_PART_CORTEX_A72	0xd08
#define CPU_PART_CORTEX_A73	0xd09
#define CPU_PART_CORTEX_A75	0xd0a
#define CPU_PART_CORTEX_A76	0xd0b

#define CPU_PART_THUNDERX_T88	0x0a1
#define CPU_PART_THUNDERX_T81	0x0a2
#define CPU_PART_THUNDERX_T83	0x0a3
#define CPU_PART_THUNDERX2_T99	0x0af

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
	{ CPU_PART_CORTEX_A35, "Cortex-A35" },
	{ CPU_PART_CORTEX_A53, "Cortex-A53" },
	{ CPU_PART_CORTEX_A55, "Cortex-A55" },
	{ CPU_PART_CORTEX_A57, "Cortex-A57" },
	{ CPU_PART_CORTEX_A72, "Cortex-A72" },
	{ CPU_PART_CORTEX_A73, "Cortex-A73" },
	{ CPU_PART_CORTEX_A75, "Cortex-A75" },
	{ CPU_PART_CORTEX_A76, "Cortex-A76" },
	{ 0 },
};

struct cpu_cores cpu_cores_cavium[] = {
	{ CPU_PART_THUNDERX_T88, "ThunderX T88" },
	{ CPU_PART_THUNDERX_T81, "ThunderX T81" },
	{ CPU_PART_THUNDERX_T83, "ThunderX T83" },
	{ CPU_PART_THUNDERX2_T99, "ThunderX2 T99" },
	{ 0 },
};

/* arm cores makers */
const struct implementers {
	int			id;
	char			*name;
	struct cpu_cores	*corelist;
} cpu_implementers[] = {
	{ CPU_IMPL_ARM,	"ARM", cpu_cores_arm },
	{ CPU_IMPL_CAVIUM, "Cavium", cpu_cores_cavium },
	{ 0 },
};

char cpu_model[64];
int cpu_node;

struct cpu_info *cpu_info_list = &cpu_info_primary;

int	cpu_match(struct device *, void *, void *);
void	cpu_attach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
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
	uint64_t clidr;
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

	for (i = 0; cpu_implementers[i].id != 0; i++) {
		if (impl == cpu_implementers[i].id) {
			impl_name = cpu_implementers[i].name;
			coreselecter = cpu_implementers[i].corelist;
			break;
		}
	}

	for (i = 0; coreselecter[i].id != 0; i++) {
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
		case CPU_PART_CORTEX_A57:
		case CPU_PART_CORTEX_A72:
		case CPU_PART_CORTEX_A73:
		case CPU_PART_CORTEX_A75:
		default:
			/*
			 * Vulnerable; call into the firmware and hope
			 * we're running on top of Arm Trusted
			 * Firmware with a fix for Security Advisory
			 * TFV 6.
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
}

int	cpu_hatch_secondary(struct cpu_info *ci, int, uint64_t);
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
	uint64_t mpidr = READ_SPECIALREG(mpidr_el1);
	uint32_t opp;

	KASSERT(faa->fa_nreg > 0);

	if (faa->fa_reg[0].addr == (mpidr & MPIDR_AFF)) {
		ci = &cpu_info_primary;
#ifdef MULTIPROCESSOR
		ci->ci_flags |= CPUF_RUNNING | CPUF_PRESENT | CPUF_PRIMARY;
#endif
	}
#ifdef MULTIPROCESSOR
	else {
		ci = malloc(sizeof(*ci), M_DEVBUF, M_WAITOK | M_ZERO);
		cpu_info[dev->dv_unit] = ci;
		ci->ci_next = cpu_info_list->ci_next;
		cpu_info_list->ci_next = ci;
		ci->ci_flags |= CPUF_AP;
		ncpus++;
	}
#endif

	ci->ci_dev = dev;
	ci->ci_cpuid = dev->dv_unit;
	ci->ci_mpidr = faa->fa_reg[0].addr;
	ci->ci_node = faa->fa_node;
	ci->ci_self = ci;

	printf(" mpidr %llx:", ci->ci_mpidr);

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
		if (cpu_hatch_secondary(ci, spinup_method, spinup_data)) {
			atomic_setbits_int(&ci->ci_flags, CPUF_IDENTIFY);
			__asm volatile("dsb sy; sev");

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
		cpu_identify(ci);

		if (OF_getproplen(ci->ci_node, "clocks") > 0) {
			cpu_node = ci->ci_node;
			cpu_cpuspeed = cpu_clockspeed;
		}

		/* Initialize debug registers. */
		WRITE_SPECIALREG(mdscr_el1, DBG_MDSCR_TDCC);
		WRITE_SPECIALREG(oslar_el1, 0);
#ifdef MULTIPROCESSOR
	}
#endif

	opp = OF_getpropint(ci->ci_node, "operating-points-v2", 0);
	if (opp)
		cpu_opp_init(ci, opp);

	printf("\n");
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
void cpu_hatch(void);

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
cpu_hatch_spin_table(struct cpu_info *ci, uint64_t start, uint64_t data)
{
	/* this reuses the zero page for the core */
	vaddr_t start_pg = zero_page + (PAGE_SIZE * ci->ci_cpuid);
	paddr_t pa = trunc_page(data);
	uint64_t offset = data - pa;
	uint64_t *startvec = (uint64_t *)(start_pg + offset);

	pmap_kenter_cache(start_pg, pa, PROT_READ|PROT_WRITE, PMAP_CACHE_CI);

	*startvec = start;
	__asm volatile("dsb sy; sev");

	pmap_kremove(start_pg, PAGE_SIZE);
}

int
cpu_hatch_secondary(struct cpu_info *ci, int method, uint64_t data)
{
	extern uint64_t pmap_avail_kvo;
	extern paddr_t cpu_hatch_ci;
	paddr_t startaddr;
	void *kstack;
	uint64_t ttbr1;
	int rc = 0;

	kstack = km_alloc(USPACE, &kv_any, &kp_zero, &kd_waitok);
	ci->ci_el1_stkend = (vaddr_t)kstack + USPACE - 16;

	pmap_extract(pmap_kernel(), (vaddr_t)ci, &cpu_hatch_ci);

	__asm("mrs %x0, ttbr1_el1": "=r"(ttbr1));
	ci->ci_ttbr1 = ttbr1;

	cpu_dcache_wb_range((vaddr_t)&cpu_hatch_ci, sizeof(paddr_t));
	cpu_dcache_wb_range((vaddr_t)ci, sizeof(*ci));

	startaddr = (vaddr_t)cpu_hatch + pmap_avail_kvo;

	switch (method) {
	case 1:
		/* psci  */
#if NPSCI > 0
		rc = (psci_cpu_on(ci->ci_mpidr, startaddr, 0) == PSCI_SUCCESS);
#endif
		break;
	case 2:
		/* spin-table */
		cpu_hatch_spin_table(ci, startaddr, data);
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
	__asm volatile("dsb sy; sev");

	while ((ci->ci_flags & CPUF_RUNNING) == 0)
		__asm volatile("wfe");
}

void
cpu_start_secondary(struct cpu_info *ci)
{
	uint64_t tcr;
	int s;

	ci->ci_flags |= CPUF_PRESENT;
	__asm volatile("dsb sy");

	while ((ci->ci_flags & CPUF_IDENTIFY) == 0)
		__asm volatile("wfe");

	cpu_identify(ci);
	atomic_setbits_int(&ci->ci_flags, CPUF_IDENTIFIED);
	__asm volatile("dsb sy");

	while ((ci->ci_flags & CPUF_GO) == 0)
		__asm volatile("wfe");

	tcr = READ_SPECIALREG(tcr_el1);
	tcr &= ~TCR_T0SZ(0x3f);
	tcr |= TCR_T0SZ(64 - USER_SPACE_BITS);
	tcr |= TCR_A1;
	WRITE_SPECIALREG(tcr_el1, tcr);

	/* Initialize debug registers. */
	WRITE_SPECIALREG(mdscr_el1, DBG_MDSCR_TDCC);
	WRITE_SPECIALREG(oslar_el1, 0);

	s = splhigh();
	arm_intr_cpu_enable();
	cpu_startclock();

	nanouptime(&ci->ci_schedstate.spc_runtime);

	atomic_setbits_int(&ci->ci_flags, CPUF_RUNNING);
	__asm volatile("dsb sy; sev");

	spllower(IPL_NONE);

	SCHED_LOCK(s);
	cpu_switchto(NULL, sched_chooseproc());
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

void
cpu_opp_init(struct cpu_info *ci, uint32_t phandle)
{
	struct opp_table *ot;
	int count, node, child;
	uint32_t values[3];
	int i, len;

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
		ot->ot_opp[count].opp_hz =
		    OF_getpropint64(child, "opp-hz", 0);
		len = OF_getpropintarray(child, "opp-microvolt",
		    values, sizeof(values));
		if (len == sizeof(uint32_t) || len == 3 * sizeof(uint32_t))
			ot->ot_opp[count].opp_microvolt = values[0];
		count++;
	}

	ot->ot_opp_hz_min = ot->ot_opp[0].opp_hz;
	ot->ot_opp_hz_max = ot->ot_opp[0].opp_hz;
	for (i = 1; i < ot->ot_nopp; i++) {
		if (ot->ot_opp[i].opp_hz < ot->ot_opp_hz_min)
			ot->ot_opp_hz_min = ot->ot_opp[i].opp_hz;
		if (ot->ot_opp[i].opp_hz > ot->ot_opp_hz_max)
			ot->ot_opp_hz_max = ot->ot_opp[i].opp_hz;
	}

	if (OF_getproplen(node, "opp-shared") == 0)
		ot->ot_master = ci;

	LIST_INSERT_HEAD(&opp_tables, ot, ot_list);

	ci->ci_opp_table = ot;
	ci->ci_cpu_supply = OF_getpropint(ci->ci_node, "cpu-supply", 0);

	/*
	 * Do addional checks at mountroot when all the clocks and
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
		error = ENODEV;
		if (curr_microvolt != 0)
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

		opp_idx = ci->ci_opp_idx;
		opp_hz = ot->ot_opp[opp_idx].opp_hz;
		opp_microvolt = ot->ot_opp[opp_idx].opp_microvolt;

		curr_hz = clock_get_frequency(ci->ci_node, NULL);
		curr_microvolt = regulator_get_voltage(ci->ci_cpu_supply);

		if (error == 0 && opp_hz < curr_hz)
			error = clock_set_frequency(ci->ci_node, NULL, opp_hz);
		if (error == 0 && opp_microvolt != 0 &&
		    opp_microvolt != curr_microvolt) {
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
