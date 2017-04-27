/*	$OpenBSD: cpu.c,v 1.1 2017/04/27 10:23:19 kettenis Exp $	*/

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
#include <sys/device.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

/* CPU Identification */
#define CPU_IMPL_ARM            0x41

#define CPU_PART_CORTEX_A53     0xd03
#define CPU_PART_CORTEX_A35     0xd04
#define CPU_PART_CORTEX_A57     0xd07
#define CPU_PART_CORTEX_A72     0xd08
#define CPU_PART_CORTEX_A73     0xd09

#define CPU_IMPL(midr)  (((midr) >> 24) & 0xff)
#define CPU_PART(midr)  (((midr) >> 4) & 0xfff)
#define CPU_VAR(midr)   (((midr) >> 20) & 0xf)
#define CPU_REV(midr)   (((midr) >> 0) & 0xf)

struct cpu_cores {
	int	id;
	char	*name;
};

struct cpu_cores cpu_cores_none[] = {
	{ 0x0, "Unknown" },
};

struct cpu_cores cpu_cores_arm[] = {
	{ CPU_PART_CORTEX_A35, "Cortex-A35" },
	{ CPU_PART_CORTEX_A53, "Cortex-A53" },
	{ CPU_PART_CORTEX_A57, "Cortex-A57" },
	{ CPU_PART_CORTEX_A72, "Cortex-A72" },
	{ CPU_PART_CORTEX_A73, "Cortex-A73" },
	{ 0x0, "Unknown" },
};

/* arm cores makers */
const struct implementers {
	int			id;
	char			*name;
	struct cpu_cores	*corelist;
} cpu_implementers[] = {
	{ CPU_IMPL_ARM,		"ARM",	cpu_cores_arm },
	{ 0,			"",	NULL },
};

int	cpu_match(struct device *, void *, void *);
void	cpu_attach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

void
cpu_identify(struct cpu_info *ci)
{
	uint64_t midr, impl, part;
	char *impl_name = "Unknown";
	char *part_name = "Unknown";
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

	printf(" %s %s r%dp%d", impl_name, part_name, CPU_VAR(midr),
	    CPU_REV(midr));
}

int
cpu_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;
	char buf[32];

	if (OF_getprop(faa->fa_node, "device_type", buf, sizeof(buf)) > 0 &&
	    strcmp(buf, "cpu") == 0)
		return 1;

	return 0;
}

void
cpu_attach(struct device *parent, struct device *dev, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct cpu_info *ci;
	uint64_t mpidr = READ_SPECIALREG(mpidr_el1);

	KASSERT(faa->fa_nreg > 0);

	if (faa->fa_reg[0].addr == (mpidr & MPIDR_AFF)) {
		ci = &cpu_info_primary;
		ci->ci_cpuid = dev->dv_unit;
		ci->ci_dev = dev;

		printf(":");
		cpu_identify(ci);
	} else {
		printf(": not configured");
	}

	printf("\n");
}
