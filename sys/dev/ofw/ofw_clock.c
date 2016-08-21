/*	$OpenBSD: ofw_clock.c,v 1.1 2016/08/21 21:38:05 kettenis Exp $	*/
/*
 * Copyright (c) 2016 Mark Kettenis
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
#include <dev/ofw/ofw_clock.h>

LIST_HEAD(, clock_device) clock_devices =
	LIST_HEAD_INITIALIZER(clock_devices);

void
clock_register(struct clock_device *cd)
{
	cd->cd_cells = OF_getpropint(cd->cd_node, "#clock-cells", 0);
	cd->cd_phandle = OF_getpropint(cd->cd_node, "phandle", 0);
	if (cd->cd_phandle == 0)
		return;

	LIST_INSERT_HEAD(&clock_devices, cd, cd_list);
}

uint32_t
clock_get_frequency_cells(uint32_t *cells)
{
	struct clock_device *cd;
	uint32_t phandle = cells[0];
	int node;

	LIST_FOREACH(cd, &clock_devices, cd_list) {
		if (cd->cd_phandle == phandle)
			break;
	}

	if (cd && cd->cd_get_frequency)
		return cd->cd_get_frequency(cd->cd_cookie, &cells[1]);

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return 0;

	if (OF_is_compatible(node, "fixed-clock"))
		return OF_getpropint(node, "clock-frequency", 0);

	if (OF_is_compatible(node, "fixed-factor-clock")) {
		uint32_t mult, div, freq;

		mult = OF_getpropint(node, "clock-mult", 1);
		div = OF_getpropint(node, "clock-div", 1);
		freq = clock_get_frequency(node, NULL);
		return (freq * mult) / div;
	}

	return 0;
}

void
clock_enable_cells(uint32_t *cells)
{
	struct clock_device *cd;
	uint32_t phandle = cells[0];

	LIST_FOREACH(cd, &clock_devices, cd_list) {
		if (cd->cd_phandle == phandle)
			break;
	}

	if (cd && cd->cd_enable)
		cd->cd_enable(cd->cd_cookie, &cells[1], 1);
}

uint32_t *
clock_next_clock(uint32_t *cells)
{
	uint32_t phandle = cells[0];
	int node, ncells;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return NULL;

	ncells = OF_getpropint(node, "#clock-cells", 0);
	return cells + ncells + 1;
}

int
clock_index(int node, const char *clock)
{
	char *names;
	char *name;
	char *end;
	int idx = 0;
	int len;

	if (clock == NULL)
		return 0;

	len = OF_getproplen(node, "clock-names");
	if (len <= 0)
		return -1;

	names = malloc(len, M_TEMP, M_WAITOK);
	OF_getprop(node, "clock-names", names, len);
	end = names + len;
	name = names;
	while (name < end) {
		if (strcmp(name, clock) == 0) {
			free(names, M_TEMP, len);
			return idx;
		}
		name += strlen(name) + 1;
		idx++;
	}
	free(names, M_TEMP, len);
	return -1;
}

uint32_t
clock_get_frequency_idx(int node, int idx)
{
	uint32_t *clocks;
	uint32_t *clock;
	uint32_t freq = 0;
	int len;

	len = OF_getproplen(node, "clocks");
	if (len <= 0)
		return 0;

	clocks = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "clocks", clocks, len);

	clock = clocks;
	while (clock && clock < clocks + (len / sizeof(uint32_t))) {
		if (idx == 0) {
			freq = clock_get_frequency_cells(clock);
			break;
		}
		clock = clock_next_clock(clock);
		idx--;
	}

	free(clocks, M_TEMP, len);
	return freq;
}

uint32_t
clock_get_frequency(int node, const char *name)
{
	int idx;

	idx = clock_index(node, name);
	if (idx == -1)
		return 0;

	return clock_get_frequency_idx(node, idx);
}

void
clock_enable_idx(int node, int idx)
{
	uint32_t *clocks;
	uint32_t *clock;
	int len;

	len = OF_getproplen(node, "clocks");
	if (len <= 0)
		return;

	clocks = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "clocks", clocks, len);

	clock = clocks;
	while (clock && clock < clocks + (len / sizeof(uint32_t))) {
		if (idx == 0) {
			clock_enable_cells(clock);
			break;
		}
		clock = clock_next_clock(clock);
		idx--;
	}

	free(clocks, M_TEMP, len);
}

void
clock_enable(int node, const char *name)
{
	int idx;

	idx = clock_index(node, name);
	if (idx == -1)
		return;

	clock_enable_idx(node, idx);
}
