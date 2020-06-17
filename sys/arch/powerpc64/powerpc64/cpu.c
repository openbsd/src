/*	$OpenBSD: cpu.c,v 1.7 2020/06/17 20:58:20 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

/* CPU Identification. */
#define CPU_IBMPOWER8E		0x004b
#define CPU_IBMPOWER8NVL	0x004c
#define CPU_IBMPOWER8		0x004d
#define CPU_IBMPOWER9		0x004e

#define CPU_VERSION(pvr)	((pvr) >> 16)
#define CPU_REV_MAJ(pvr)	(((pvr) >> 8) & 0xf)
#define CPU_REV_MIN(pvr)	(((pvr) >> 0) & 0xf)

struct cpu_version {
	int		version;
	const char	*name;
};

struct cpu_version cpu_version[] = {
	{ CPU_IBMPOWER8, "IBM POWER8" },
	{ CPU_IBMPOWER8E, "IBM POWER8E" },
	{ CPU_IBMPOWER8NVL, "IBM POWER8NVL" },
	{ CPU_IBMPOWER9, "IBM POWER9" },
	{ 0, NULL }
};

char cpu_model[64];
uint64_t tb_freq = 512000000;	/* POWER8, POWER9 */

struct cpu_info cpu_info_primary;

int	cpu_match(struct device *, void *, void *);
void	cpu_attach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

int
cpu_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;
	char buf[32];

	if (OF_getprop(faa->fa_node, "device_type", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "cpu") != 0)
		return 0;

	if (ncpus < MAXCPUS || faa->fa_reg[0].addr == mfpir())
		return 1;

	return 0;
}

void
cpu_attach(struct device *parent, struct device *dev, void *aux)
{
	struct fdt_attach_args *faa = aux;
	const char *name;
	uint32_t pvr;
	uint32_t iline;
	uint32_t dline;
	int node, level, i;

	printf(" pir %llx", faa->fa_reg[0].addr);

	pvr = mfpvr();

	for (i = 0; cpu_version[i].name; i++) {
		if (CPU_VERSION(pvr) == cpu_version[i].version) {
			name = cpu_version[i].name;
			break;
		}
	}

	if (name) {
		printf(": %s %d.%d", name, CPU_REV_MAJ(pvr), CPU_REV_MIN(pvr));
		snprintf(cpu_model, sizeof(cpu_model), "%s %d.%d",
		    name, CPU_REV_MAJ(pvr), CPU_REV_MIN(pvr));
	} else {
		printf(": Unknown, PVR 0x%x", pvr);
		strlcpy(cpu_model, "Unknown", sizeof(cpu_model));
	}

	node = faa->fa_node;
	iline = OF_getpropint(node, "i-cache-block-size", 128);
	dline = OF_getpropint(node, "d-cache-block-size", 128);
	level = 1;

	while (node) {
		const char *unit = "KB";
		uint32_t isize, iways;
		uint32_t dsize, dways;
		uint32_t cache;

		isize = OF_getpropint(node, "i-cache-size", 0) / 1024;
		iways = OF_getpropint(node, "i-cache-sets", 0);
		dsize = OF_getpropint(node, "d-cache-size", 0) / 1024;
		dways = OF_getpropint(node, "d-cache-sets", 0);

		/* Print large cache sizes in MB. */
		if (isize > 4096 && dsize > 4096) {
			unit = "MB";
			isize /= 1024;
			dsize /= 1024;
		}

		printf("\n%s:", dev->dv_xname);
		
		if (OF_getproplen(node, "cache-unified") == 0) {
			printf(" %d%s %db/line %d-way L%d cache",
			    isize, unit, iline, iways, level);
		} else {
			printf(" %d%s %db/line %d-way L%d I-cache",
			    isize, unit, iline, iways, level);
			printf(", %d%s %db/line %d-way L%d D-cache",
			    dsize, unit, dline, dways, level);
		}

		cache = OF_getpropint(node, "l2-cache", 0);
		node = OF_getnodebyphandle(cache);
		level++;
	}

	printf("\n");

	/* Update timebase frequency to reflect reality. */
	tb_freq = OF_getpropint(faa->fa_node, "timebase-frequency", tb_freq);
}
