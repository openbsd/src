/* $OpenBSD: kern_intrmap.c,v 1.3 2020/06/23 01:40:03 dlg Exp $ */

/*
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if.c	8.3 (Berkeley) 1/4/94
 * $FreeBSD: src/sys/net/if.c,v 1.185 2004/03/13 02:35:03 brooks Exp $
 */

/*
 * This code is adapted from the if_ringmap code in DragonflyBSD,
 * but generalised for use by all types of devices, not just network
 * cards.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>

#include <sys/intrmap.h>

struct intrmap_cpus {
	struct refcnt	  ic_refs;
	unsigned int	  ic_count;
	struct cpu_info **ic_cpumap;
};

struct intrmap {
	unsigned int	 im_count;
	unsigned int	 im_grid;
	struct intrmap_cpus *
			 im_cpus;
	unsigned int	*im_cpumap;
};

/*
 * The CPUs that should be used for interrupts may be a subset of all CPUs.
 */

struct rwlock		 intrmap_lock = RWLOCK_INITIALIZER("intrcpus");
struct intrmap_cpus	*intrmap_cpus = NULL;
int			 intrmap_ncpu = 0;

static void
intrmap_cpus_put(struct intrmap_cpus *ic)
{
	if (ic == NULL)
		return;

	if (refcnt_rele(&ic->ic_refs)) {
		free(ic->ic_cpumap, M_DEVBUF,
		    ic->ic_count * sizeof(*ic->ic_cpumap));
		free(ic, M_DEVBUF, sizeof(*ic));
	}
}

static struct intrmap_cpus *
intrmap_cpus_get(void)
{
	struct intrmap_cpus *oic = NULL;
	struct intrmap_cpus *ic;

	rw_enter_write(&intrmap_lock);
	if (intrmap_ncpu != ncpus) {
		unsigned int icpus = 0;
		struct cpu_info **cpumap;
		CPU_INFO_ITERATOR cii;
		struct cpu_info *ci;

		/*
		 * there's a new "version" of the set of CPUs available, so
		 * we need to figure out which ones we can use for interrupts.
		 */

		cpumap = mallocarray(ncpus, sizeof(*cpumap),
		    M_DEVBUF, M_WAITOK);

		CPU_INFO_FOREACH(cii, ci) {
#ifdef __HAVE_CPU_TOPOLOGY
			if (ci->ci_smt_id > 0)
				continue;
#endif
			cpumap[icpus++] = ci;
		}

		if (icpus < ncpus) {
			/* this is mostly about free(9) needing a size */
			struct cpu_info **icpumap = mallocarray(icpus,
			    sizeof(*icpumap), M_DEVBUF, M_WAITOK);
			memcpy(icpumap, cpumap, icpus * sizeof(*icpumap));
			free(cpumap, M_DEVBUF, ncpus * sizeof(*cpumap));
			cpumap = icpumap;
		}

		ic = malloc(sizeof(*ic), M_DEVBUF, M_WAITOK);
		refcnt_init(&ic->ic_refs);
		ic->ic_count = icpus;
		ic->ic_cpumap = cpumap;

		oic = intrmap_cpus;
		intrmap_cpus = ic; /* give this ref to the global. */
	} else
		ic = intrmap_cpus;

	refcnt_take(&ic->ic_refs); /* take a ref for the caller */
	rw_exit_write(&intrmap_lock);

	intrmap_cpus_put(oic);

	return (ic);
}

static int
intrmap_nintrs(const struct intrmap_cpus *ic, unsigned int nintrs,
    unsigned int maxintrs)
{
	KASSERTMSG(maxintrs > 0, "invalid maximum interrupt count %u",
	    maxintrs);

	if (nintrs == 0 || nintrs > maxintrs)
		nintrs = maxintrs;
	if (nintrs > ic->ic_count)
		nintrs = ic->ic_count;
	return (nintrs);
}

static void
intrmap_set_grid(struct intrmap *im, unsigned int unit, unsigned int grid)
{
	unsigned int i, offset;
	unsigned int *cpumap = im->im_cpumap;
	const struct intrmap_cpus *ic = im->im_cpus;

	KASSERTMSG(grid > 0, "invalid if_ringmap grid %u", grid);
	KASSERTMSG(grid >= im->im_count, "invalid intrmap grid %u, count %u",
	    grid, im->im_count);
	im->im_grid = grid;

	offset = (grid * unit) % ic->ic_count;
	for (i = 0; i < im->im_count; i++) {
		cpumap[i] = offset + i;
		KASSERTMSG(cpumap[i] < ic->ic_count,
		    "invalid cpumap[%u] = %u, offset %u (ncpu %d)", i,
		    cpumap[i], offset, ic->ic_count);
	}
}

struct intrmap *
intrmap_create(const struct device *dv,
    unsigned int nintrs, unsigned int maxintrs, unsigned int flags)
{
	struct intrmap *im;
	unsigned int unit = dv->dv_unit;
	unsigned int i, grid = 0, prev_grid;
	struct intrmap_cpus *ic;

	ic = intrmap_cpus_get();

	nintrs = intrmap_nintrs(ic, nintrs, maxintrs);
	if (ISSET(flags, INTRMAP_POWEROF2))
		nintrs = 1 << (fls(nintrs) - 1);
	im = malloc(sizeof(*im), M_DEVBUF, M_WAITOK | M_ZERO);
	im->im_count = nintrs;
	im->im_cpus = ic; 
	im->im_cpumap = mallocarray(nintrs, sizeof(*im->im_cpumap), M_DEVBUF,
	    M_WAITOK | M_ZERO);

	prev_grid = ic->ic_count;
	for (i = 0; i < ic->ic_count; i++) {
		if (ic->ic_count % (i + 1) != 0)
			continue;

		grid = ic->ic_count / (i + 1);
		if (nintrs > grid) {
			grid = prev_grid;
			break;
		}

		if (nintrs > ic->ic_count / (i + 2))
			break;
		prev_grid = grid;
	}
	intrmap_set_grid(im, unit, grid);

	return (im);
}

void
intrmap_destroy(struct intrmap *im)
{
	free(im->im_cpumap, M_DEVBUF, im->im_count * sizeof(*im->im_cpumap));
	intrmap_cpus_put(im->im_cpus);
	free(im, M_DEVBUF, sizeof(*im));
}

/*
 * Align the two ringmaps.
 *
 * e.g. 8 netisrs, rm0 contains 4 rings, rm1 contains 2 rings.
 *
 * Before:
 *
 * CPU      0  1  2  3   4  5  6  7
 * NIC_RX               n0 n1 n2 n3
 * NIC_TX        N0 N1
 *
 * After:
 *
 * CPU      0  1  2  3   4  5  6  7
 * NIC_RX               n0 n1 n2 n3
 * NIC_TX               N0 N1
 */
void
intrmap_align(const struct device *dv,
    struct intrmap *im0, struct intrmap *im1)
{
	unsigned int unit = dv->dv_unit;

	KASSERT(im0->im_cpus == im1->im_cpus);

	if (im0->im_grid > im1->im_grid)
		intrmap_set_grid(im1, unit, im0->im_grid);
	else if (im0->im_grid < im1->im_grid)
		intrmap_set_grid(im0, unit, im1->im_grid);
}

void
intrmap_match(const struct device *dv,
    struct intrmap *im0, struct intrmap *im1)
{
	unsigned int unit = dv->dv_unit;
	const struct intrmap_cpus *ic;
	unsigned int subset_grid, cnt, divisor, mod, offset, i;
	struct intrmap *subset_im, *im;
	unsigned int old_im0_grid, old_im1_grid;

	KASSERT(im0->im_cpus == im1->im_cpus);
	if (im0->im_grid == im1->im_grid)
		return;

	/* Save grid for later use */
	old_im0_grid = im0->im_grid;
	old_im1_grid = im1->im_grid;

	intrmap_align(dv, im0, im1);

	/*
	 * Re-shuffle rings to get more even distribution.
	 *
	 * e.g. 12 netisrs, rm0 contains 4 rings, rm1 contains 2 rings.
	 *
	 * CPU       0  1  2  3   4  5  6  7   8  9 10 11
	 *
	 * NIC_RX   a0 a1 a2 a3  b0 b1 b2 b3  c0 c1 c2 c3
	 * NIC_TX   A0 A1        B0 B1        C0 C1
	 *
	 * NIC_RX   d0 d1 d2 d3  e0 e1 e2 e3  f0 f1 f2 f3
	 * NIC_TX         D0 D1        E0 E1        F0 F1
	 */

	if (im0->im_count >= (2 * old_im1_grid)) {
		cnt = im0->im_count;
		subset_grid = old_im1_grid;
		subset_im = im1;
		im = im0;
	} else if (im1->im_count > (2 * old_im0_grid)) {
		cnt = im1->im_count;
		subset_grid = old_im0_grid;
		subset_im = im0;
		im = im1;
	} else {
		/* No space to shuffle. */
		return;
	}

	ic = im0->im_cpus;

	mod = cnt / subset_grid;
	KASSERT(mod >= 2);
	divisor = ic->ic_count / im->im_grid;
	offset = ((unit / divisor) % mod) * subset_grid;

	for (i = 0; i < subset_im->im_count; i++) {
		subset_im->im_cpumap[i] += offset;
		KASSERTMSG(subset_im->im_cpumap[i] < ic->ic_count,
		    "match: invalid cpumap[%d] = %d, offset %d",
		     i, subset_im->im_cpumap[i], offset);
	}
#ifdef DIAGNOSTIC
	for (i = 0; i < subset_im->im_count; i++) {
		unsigned int j;

		for (j = 0; j < im->im_count; j++) {
			if (im->im_cpumap[j] == subset_im->im_cpumap[i])
				break;
		}
		KASSERTMSG(j < im->im_count,
		    "subset cpumap[%u] = %u not found in superset",
		     i, subset_im->im_cpumap[i]);
	}
#endif
}

unsigned int
intrmap_count(const struct intrmap *im)
{
	return (im->im_count);
}

struct cpu_info *
intrmap_cpu(const struct intrmap *im, unsigned int ring)
{
	const struct intrmap_cpus *ic = im->im_cpus;
	unsigned int icpu;
	KASSERTMSG(ring < im->im_count, "invalid ring %u", ring);
	icpu = im->im_cpumap[ring];
	KASSERTMSG(icpu < ic->ic_count, "invalid interrupt cpu %u for ring %u"
	    " (intrmap %p)", icpu, ring, im);
	return (ic->ic_cpumap[icpu]);
}

struct cpu_info *
intrmap_one(const struct device *dv)
{
	unsigned int unit = dv->dv_unit;
	struct intrmap_cpus *ic;
	struct cpu_info *ci;

	ic = intrmap_cpus_get();
	ci = ic->ic_cpumap[unit % ic->ic_count];
	intrmap_cpus_put(ic);

	return (ci);
}
