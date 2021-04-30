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
#include <machine/elf.h>
#include <machine/cpufunc.h>
#include <machine/riscvreg.h>
#include "../dev/timer.h"

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/ofw_thermal.h>
#include <dev/ofw/fdt.h>

#if 0
#include "psci.h"
#if NPSCI > 0
#include <dev/fdt/pscivar.h>
#endif
#endif

/* CPU Identification */

// from FreeBSD
/*
 * 0x0000         CPU ID unimplemented
 * 0x0001         UC Berkeley Rocket repo
 * 0x0002­0x7FFE  Reserved for open-source repos
 * 0x7FFF         Reserved for extension
 * 0x8000         Reserved for anonymous source
 * 0x8001­0xFFFE  Reserved for proprietary implementations
 * 0xFFFF         Reserved for extension
 */

#define	CPU_IMPL_SHIFT		0
#define	CPU_IMPL_MASK		(0xffff << CPU_IMPL_SHIFT)
#define	CPU_IMPL(mimpid)	((mimpid & CPU_IMPL_MASK) >> CPU_IMPL_SHIFT)

#define	CPU_PART_SHIFT		62
#define	CPU_PART_MASK		(0x3ul << CPU_PART_SHIFT)
#define	CPU_PART(misa)		((misa & CPU_PART_MASK) >> CPU_PART_SHIFT)

#define	CPU_IMPL_UNIMPLEMEN	0x00
#define CPU_IMPL_QEMU		0x01
#define	CPU_IMPL_UCB_ROCKET	0x02
#define CPU_IMPL_SIFIVE		0x03

#define	CPU_PART_RV32		0x01
#define	CPU_PART_RV64		0x02
#define	CPU_PART_RV128		0x03

/*
 * PART ID has only 2 bits
 *
#define CPU_PART_QEMU_SPIKE_V1_9	0x0
#define CPU_PART_QEMU_SPIKE_V1_10	0x0
*/
#define CPU_PART_QEMU_SIFIVE_E		0x01
#define CPU_PART_QEMU_SIFIVE_U		0x02
#define CPU_PART_QEMU_VIRT		0x03

/* Hardware implementation info. These values may be empty. */
register_t mvendorid;	/* The CPU's JEDEC vendor ID */
register_t marchid;	/* The architecture ID */
register_t mimpid;	/* The implementation ID */

struct cpu_desc {
	int		cpu_impl;
	int		cpu_part_num;
	const char	*cpu_impl_name;
	const char	*cpu_part_name;
};

struct cpu_desc cpu_desc[MAXCPUS];

struct cpu_parts {
	int	part_id;
	char	*part_name;
};

#define	CPU_PART_NONE	{ -1, "Unknown Processor" }

struct cpu_parts cpu_parts_std[] = {
	{ CPU_PART_RV32,	"RV32" },
	{ CPU_PART_RV64,	"RV64" },
	{ CPU_PART_RV128,	"RV128" },
	CPU_PART_NONE,
};

struct cpu_parts cpu_parts_qemu[] = {
/*
	{ CPU_PART_QEMU_SPIKE_V1_9, "qemu-spike-V1.9.1" },
	{ CPU_PART_QEMU_SPIKE_V1_10, "qemu-spike-V1.10" },
*/
	{ CPU_PART_QEMU_SIFIVE_E, "qemu-sifive-e" },
	{ CPU_PART_QEMU_SIFIVE_U, "qemu-sifive-u" },
	{ CPU_PART_QEMU_VIRT, "qemu-virt" },
	CPU_PART_NONE,
};

struct cpu_parts cpu_parts_rocket[] = {//placeholder
	CPU_PART_NONE,
};

struct cpu_parts cpu_parts_sifive[] = {//placeholder
	CPU_PART_NONE,
};

/* riscv parts makers */
const struct implementers {
	int			impl_id;
	char			*impl_name;
	struct cpu_parts	*impl_partlist;
} cpu_implementers[] = {
	{ CPU_IMPL_QEMU, "QEMU", cpu_parts_qemu },
	{ CPU_IMPL_UCB_ROCKET,	"UC Berkeley Rocket", cpu_parts_rocket },
	{ CPU_IMPL_SIFIVE, "SiFive", cpu_parts_sifive },
	{ CPU_IMPL_UNIMPLEMEN, "Unknown Implementer", cpu_parts_std },
};

char cpu_model[64];
int cpu_node;
uint64_t elf_hwcap;//will need it for multiple heter-processors

struct cpu_info *cpu_info_list = &cpu_info_primary;

int	cpu_match(struct device *, void *, void *);
void	cpu_attach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};
#if 0 //XXX
void	cpu_flush_bp_psci(void);
#endif

/*
 * The ISA string is made up of a small prefix (e.g. rv64) and up to 26 letters
 * indicating the presence of the 26 possible standard extensions. Therefore 32
 * characters will be sufficient.
 */
#define	ISA_NAME_MAXLEN		32
#define	ISA_PREFIX		"rv64"	// ("rv" __XSTRING(XLEN))
#define	ISA_PREFIX_LEN		(sizeof(ISA_PREFIX) - 1)

void
cpu_identify(struct cpu_info *ci)
{
	const struct cpu_parts *cpu_partsp;
	int part_id;
	int impl_id;
	uint64_t mimpid;
	uint64_t misa;
	int cpu, i, node, len;

	uint64_t *caps;
	uint64_t hwcap;
	char isa[ISA_NAME_MAXLEN];

	cpu_partsp = NULL;

	/* TODO: can we get mimpid and misa somewhere ? */
	mimpid = 1;// for qemu
	misa = (0x3ul << CPU_PART_SHIFT);// for virt

	cpu = cpu_number();

	caps = mallocarray(256, sizeof(uint64_t), M_DEVBUF, M_ZERO | M_WAITOK);

	// identify vendor
	impl_id	= CPU_IMPL(mimpid);
	for (i = 0; i < nitems(cpu_implementers); i++) {
		if (impl_id == cpu_implementers[i].impl_id) {
			cpu_desc[cpu].cpu_impl = impl_id;
			cpu_desc[cpu].cpu_impl_name = cpu_implementers[i].impl_name;
			cpu_partsp = cpu_implementers[i].impl_partlist;
			break;
		}
	}

	// identify part number
	part_id = CPU_PART(misa);
	for (i = 0; &cpu_partsp[i] != NULL; i++) {
		if (part_id == cpu_partsp[i].part_id) { 
			cpu_desc[cpu].cpu_part_num = part_id;
			cpu_desc[cpu].cpu_part_name = cpu_partsp[i].part_name;
			break;
		}
	}

	// identify supported isa set
	node = OF_finddevice("/cpus");
	if (node == -1) {
		printf("fill_elf_hwcap: Can't find cpus node\n");
		return;
	}

	caps['i'] = caps['I'] = HWCAP_ISA_I;
	caps['m'] = caps['M'] = HWCAP_ISA_M;
	caps['a'] = caps['A'] = HWCAP_ISA_A;
	caps['f'] = caps['F'] = HWCAP_ISA_F;
	caps['d'] = caps['D'] = HWCAP_ISA_D;
	caps['c'] = caps['C'] = HWCAP_ISA_C;

	/*
	 * Iterate through the CPUs and examine their ISA string. While we
	 * could assign elf_hwcap to be whatever the boot CPU supports, to
	 * handle the (unusual) case of running a system with hetergeneous
	 * ISAs, keep only the extension bits that are common to all harts.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		/* Skip any non-CPU nodes, such as cpu-map. */
		if (!OF_is_compatible(node, "riscv"))
			continue;

		len = OF_getprop(node, "riscv,isa", isa, sizeof(isa));
		KASSERT(len <= ISA_NAME_MAXLEN);
		if (len == -1) {
			printf("Can't find riscv,isa property\n");
			return;
		} else if (strncmp(isa, ISA_PREFIX, ISA_PREFIX_LEN) != 0) {
			printf("Unsupported ISA string: %s\n", isa);
			return;
		}

		hwcap = 0;
		for (i = ISA_PREFIX_LEN; i < len; i++)
			hwcap |= caps[(unsigned char)isa[i]];

		if (elf_hwcap != 0)
			elf_hwcap &= hwcap;
		else
			elf_hwcap = hwcap;
	}

	/* Print details for boot CPU */
	if (cpu == 0) {
		printf(": %s %s %s\n",
		    cpu_desc[cpu].cpu_impl_name,
		    cpu_desc[cpu].cpu_part_name,
		    isa);
	}
}

#if 0//XXX
int	cpu_hatch_secondary(struct cpu_info *ci, int, uint64_t);
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

	KASSERT(faa->fa_nreg > 0);

	if (faa->fa_reg[0].addr == boot_hart) {/* the primary cpu */
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
	ci->ci_node = faa->fa_node;
	ci->ci_self = ci;

#ifdef MULTIPROCESSOR // XXX TBD: CMPE
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

		/*
		 * attach cpu-embedded timer
		 * Trick: timer has no fdt node to match,
		 * riscv_timer_match will always return 1 at first call,
		 * and return 0 for all following calls,
		 * therefore, must attach timer before any node
		 */
		config_found_sm(dev, NULL, NULL, riscv_timer_match);

		/*
		 * attach cpu's children node, so far there is only the
		 * cpu-embedded interrupt controller
		 */
		struct fdt_attach_args	 fa_intc;
		int node;
		for (node = OF_child(faa->fa_node); node; node = OF_peer(node)) {
			fa_intc.fa_node = node;
			/* no specifying match func, will call cfdata's match func*/
			config_found(dev, &fa_intc, NULL);
		}

#ifdef MULTIPROCESSOR
	}
#endif
}

int
cpu_clockspeed(int *freq)
{
	*freq = clock_get_frequency(cpu_node, NULL) / 1000000;
	return 0;
}
