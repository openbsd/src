/*	$OpenBSD: ofw_power.c,v 1.3 2026/11/06 01:39:54 kettenis Exp $	*/
/*
 * Copyright (c) 2016 Mark Kettenis
 * Copyright (c) 2018 Patrick Wildt <patrick@blueri.se>
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_power.h>

LIST_HEAD(, power_domain_device) power_domain_devices =
	LIST_HEAD_INITIALIZER(power_domain_devices);

void
power_domain_register(struct power_domain_device *pd)
{
	pd->pd_cells = OF_getpropint(pd->pd_node, "#power-domain-cells", 0);
	pd->pd_phandle = OF_getpropint(pd->pd_node, "phandle", 0);
	if (pd->pd_phandle == 0)
		return;

	LIST_INSERT_HEAD(&power_domain_devices, pd, pd_list);
}

void
power_domain_enable_cells(uint32_t *cells, int on)
{
	struct power_domain_device *pd;
	uint32_t phandle = cells[0];

	LIST_FOREACH(pd, &power_domain_devices, pd_list) {
		if (pd->pd_phandle == phandle)
			break;
	}

	if (pd && pd->pd_enable)
		pd->pd_enable(pd->pd_cookie, &cells[1], on);
}

void
power_domain_do_enable_idx(int node, int idx, int on)
{
	uint32_t domain[MAX_POWER_DOMAIN_CELLS];
	int offset = 0;

	for (;;) {
		offset = OF_getnextcells(node, "power-domains",
		    "#power-domain-cells", offset, domain, sizeof(domain));
		if (offset < 0)
			break;
		if (idx <= 0)
			power_domain_enable_cells(domain, on);
		if (idx == 0)
			break;
		idx--;
	}
}

void
power_domain_enable_idx(int node, int idx)
{
	power_domain_do_enable_idx(node, idx, 1);
}

void
power_domain_enable(int node)
{
	power_domain_do_enable_idx(node, 0, 1);
}

void
power_domain_disable_idx(int node, int idx)
{
	power_domain_do_enable_idx(node, idx, 0);
}

void
power_domain_disable(int node)
{
	power_domain_do_enable_idx(node, 0, 0);
}
