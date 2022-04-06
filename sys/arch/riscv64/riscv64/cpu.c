/*	$OpenBSD: cpu.c,v 1.13 2022/04/06 18:59:27 naddy Exp $	*/

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
#include <sys/atomic.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/sysctl.h>

#include <uvm/uvm.h>

#include <machine/fdt.h>
#include <machine/sbi.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/fdt.h>

/* CPU Identification */

#define CPU_VENDOR_SIFIVE	0x489

#define CPU_ARCH_U5		0x0000000000000001
#define CPU_ARCH_U7		0x8000000000000007

/* Architectures */
struct arch {
	uint64_t	id;
	char		*name;
};

struct arch cpu_arch_none[] = {
	{ 0, NULL }
};

struct arch cpu_arch_sifive[] = {
	{ CPU_ARCH_U5, "U5" },
	{ CPU_ARCH_U7, "U7" },
	{ 0, NULL }
};

/* Vendors */
const struct vendor {
	uint32_t	id;
	char		*name;
	struct arch	*archlist;
} cpu_vendors[] = {
	{ CPU_VENDOR_SIFIVE, "SiFive", cpu_arch_sifive },
	{ 0, NULL }
};

char cpu_model[64];
int cpu_node;

struct cpu_info *cpu_info_list = &cpu_info_primary;

int	cpu_match(struct device *, void *, void *);
void	cpu_attach(struct device *, struct device *, void *);

const struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

int cpu_errata_sifive_cip_1200;

void
cpu_identify(struct cpu_info *ci)
{
	char isa[32];
	uint64_t marchid, mimpid;
	uint32_t mvendorid;
	const char *vendor_name = NULL;
	const char *arch_name = NULL;
	struct arch *archlist = cpu_arch_none;
	int i, len;

	mvendorid = sbi_get_mvendorid();
	marchid = sbi_get_marchid();
	mimpid = sbi_get_mimpid();

	for (i = 0; cpu_vendors[i].name; i++) {
		if (mvendorid == cpu_vendors[i].id) {
			vendor_name = cpu_vendors[i].name;
			archlist = cpu_vendors[i].archlist;
			break;
		}
	}

	for (i = 0; archlist[i].name; i++) {
		if (marchid == archlist[i].id) {
			arch_name = archlist[i].name;
			break;
		}
	}

	if (vendor_name)
		printf(": %s", vendor_name);
	else
		printf(": vendor %x", mvendorid);
	if (arch_name)
		printf(" %s", arch_name);
	else
		printf(" arch %llx", marchid);
	printf(" imp %llx", mimpid);

	len = OF_getprop(ci->ci_node, "riscv,isa", isa, sizeof(isa));
	if (len != -1) {
		printf(" %s", isa);
		strlcpy(cpu_model, isa, sizeof(cpu_model));
	}
	printf("\n");

	/* Handle errata. */
	if (mvendorid == CPU_VENDOR_SIFIVE && marchid == CPU_ARCH_U7)
		cpu_errata_sifive_cip_1200 = 1;
}

#ifdef MULTIPROCESSOR
int	cpu_hatch_secondary(struct cpu_info *ci);
#endif
int	cpu_clockspeed(int *);

int
cpu_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;
	char buf[32];

	if (OF_getprop(faa->fa_node, "device_type", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "cpu") != 0)
		return 0;

	if (ncpus < MAXCPUS || faa->fa_reg[0].addr == boot_hart) /* the primary cpu */
		return 1;

	return 0;
}

void
cpu_attach(struct device *parent, struct device *dev, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct cpu_info *ci;
	int node, level;

	KASSERT(faa->fa_nreg > 0);

#ifdef MULTIPROCESSOR
	if (faa->fa_reg[0].addr == boot_hart) {
		ci = &cpu_info_primary;
		ci->ci_flags |= CPUF_RUNNING | CPUF_PRESENT | CPUF_PRIMARY;
		csr_set(sie, SIE_SSIE);
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
	ci->ci_hartid = faa->fa_reg[0].addr;
	ci->ci_node = faa->fa_node;
	ci->ci_self = ci;

#ifdef MULTIPROCESSOR
	if (ci->ci_flags & CPUF_AP) {
		int timeout = 10000;

		sched_init_cpu(ci);
		if (cpu_hatch_secondary(ci)) {
			atomic_setbits_int(&ci->ci_flags, CPUF_IDENTIFY);
			membar_producer();

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

		/*
		 * attach cpu's children node, so far there is only the
		 * cpu-embedded interrupt controller
		 */
		struct fdt_attach_args	 fa_intc;
		for (node = OF_child(faa->fa_node); node; node = OF_peer(node)) {
			fa_intc.fa_node = node;
			/* no specifying match func, will call cfdata's match func*/
			config_found(dev, &fa_intc, NULL);
		}

#ifdef MULTIPROCESSOR
	}
#endif

	node = faa->fa_node;

	level = 1;

	while (node) {
		const char *unit = "KB";
		uint32_t line, iline, dline;
		uint32_t size, isize, dsize;
		uint32_t ways, iways, dways;
		uint32_t cache;

		line = OF_getpropint(node, "cache-block-size", 0);
		size = OF_getpropint(node, "cache-size", 0);
		ways = OF_getpropint(node, "cache-sets", 0);
		iline = OF_getpropint(node, "i-cache-block-size", line);
		isize = OF_getpropint(node, "i-cache-size", size) / 1024;
		iways = OF_getpropint(node, "i-cache-sets", ways);
		dline = OF_getpropint(node, "d-cache-block-size", line);
		dsize = OF_getpropint(node, "d-cache-size", size) / 1024;
		dways = OF_getpropint(node, "d-cache-sets", ways);

		if (isize == 0 && dsize == 0)
			break;

		/* Print large cache sizes in MB. */
		if (isize > 4096 && dsize > 4096) {
			unit = "MB";
			isize /= 1024;
			dsize /= 1024;
		}

		printf("%s:", dev->dv_xname);
		
		if (OF_getproplen(node, "cache-unified") == 0) {
			printf(" %d%s %db/line %d-way L%d cache",
			    isize, unit, iline, iways, level);
		} else {
			printf(" %d%s %db/line %d-way L%d I-cache",
			    isize, unit, iline, iways, level);
			printf(", %d%s %db/line %d-way L%d D-cache",
			    dsize, unit, dline, dways, level);
		}

		cache = OF_getpropint(node, "next-level-cache", 0);
		node = OF_getnodebyphandle(cache);
		level++;

		printf("\n");
	}
}

int
cpu_clockspeed(int *freq)
{
	*freq = clock_get_frequency(cpu_node, NULL) / 1000000;
	return 0;
}

void
cpu_cache_nop_range(paddr_t pa, psize_t len)
{
}

void (*cpu_dcache_wbinv_range)(paddr_t, psize_t) = cpu_cache_nop_range;
void (*cpu_dcache_inv_range)(paddr_t, psize_t) = cpu_cache_nop_range;
void (*cpu_dcache_wb_range)(paddr_t, psize_t) = cpu_cache_nop_range;

#ifdef MULTIPROCESSOR

void	cpu_hatch(void);

void
cpu_boot_secondary(struct cpu_info *ci)
{
	atomic_setbits_int(&ci->ci_flags, CPUF_GO);
	membar_producer();

	while ((ci->ci_flags & CPUF_RUNNING) == 0)
		CPU_BUSY_CYCLE();
}

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

int
cpu_hatch_secondary(struct cpu_info *ci)
{
	paddr_t start_addr, a1;
	void *kstack;
	int error;

	kstack = km_alloc(USPACE, &kv_any, &kp_zero, &kd_waitok);
	ci->ci_initstack_end = (vaddr_t)kstack + USPACE - 16;

	pmap_extract(pmap_kernel(), (vaddr_t)cpu_hatch, &start_addr);
	pmap_extract(pmap_kernel(), (vaddr_t)ci, &a1);

	ci->ci_satp = pmap_kernel()->pm_satp;

	error = sbi_hsm_hart_start(ci->ci_hartid, start_addr, a1);
	return (error == SBI_SUCCESS);
}

void cpu_startclock(void);

void
cpu_start_secondary(void)
{
	struct cpu_info *ci = curcpu();
	int s;

	ci->ci_flags |= CPUF_PRESENT;
	membar_producer();

	while ((ci->ci_flags & CPUF_IDENTIFY) == 0)
		membar_consumer();

	cpu_identify(ci);

	atomic_setbits_int(&ci->ci_flags, CPUF_IDENTIFIED);
	membar_producer();

	while ((ci->ci_flags & CPUF_GO) == 0)
		membar_consumer();

	s = splhigh();
	riscv_intr_cpu_enable();
	cpu_startclock();

	csr_clear(sstatus, SSTATUS_FS_MASK);
	csr_set(sie, SIE_SSIE);

	nanouptime(&ci->ci_schedstate.spc_runtime);

	atomic_setbits_int(&ci->ci_flags, CPUF_RUNNING);
	membar_producer();

	spllower(IPL_NONE);
	intr_enable();

	SCHED_LOCK(s);
	cpu_switchto(NULL, sched_chooseproc());
}

void
cpu_kick(struct cpu_info *ci)
{
	if (ci != curcpu())
		intr_send_ipi(ci, IPI_NOP);
}

void
cpu_unidle(struct cpu_info *ci)
{
	if (ci != curcpu())
		intr_send_ipi(ci, IPI_NOP);
}

#endif
