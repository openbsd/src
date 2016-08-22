/*	$OpenBSD: ofw_clock.c,v 1.4 2016/08/22 19:28:27 kettenis Exp $	*/
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

/*
 * Clock functionality.
 */

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

int
clock_set_frequency_cells(uint32_t *cells, uint32_t freq)
{
	struct clock_device *cd;
	uint32_t phandle = cells[0];

	LIST_FOREACH(cd, &clock_devices, cd_list) {
		if (cd->cd_phandle == phandle)
			break;
	}

	if (cd && cd->cd_set_frequency)
		return cd->cd_set_frequency(cd->cd_cookie, &cells[1], freq);

	return -1;
}

void
clock_enable_cells(uint32_t *cells, int on)
{
	struct clock_device *cd;
	uint32_t phandle = cells[0];

	LIST_FOREACH(cd, &clock_devices, cd_list) {
		if (cd->cd_phandle == phandle)
			break;
	}

	if (cd && cd->cd_enable)
		cd->cd_enable(cd->cd_cookie, &cells[1], on);
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

int
clock_set_frequency_idx(int node, int idx, uint32_t freq)
{
	uint32_t *clocks;
	uint32_t *clock;
	int rv = -1;
	int len;

	len = OF_getproplen(node, "clocks");
	if (len <= 0)
		return 0;

	clocks = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "clocks", clocks, len);

	clock = clocks;
	while (clock && clock < clocks + (len / sizeof(uint32_t))) {
		if (idx == 0) {
			rv = clock_set_frequency_cells(clock, freq);
			break;
		}
		clock = clock_next_clock(clock);
		idx--;
	}

	free(clocks, M_TEMP, len);
	return rv;
}

int
clock_set_frequency(int node, const char *name, uint32_t freq)
{
	int idx;

	idx = clock_index(node, name);
	if (idx == -1)
		return 0;

	return clock_set_frequency_idx(node, idx, freq);
}

void
clock_do_enable_idx(int node, int idx, int on)
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
		if (idx <= 0)
			clock_enable_cells(clock, on);
		if (idx == 0)
			break;
		clock = clock_next_clock(clock);
		idx--;
	}

	free(clocks, M_TEMP, len);
}

void
clock_do_enable(int node, const char *name, int on)
{
	int idx;

	idx = clock_index(node, name);
	if (idx == -1)
		return;

	clock_do_enable_idx(node, idx, on);
}

void
clock_enable_idx(int node, int idx)
{
	clock_do_enable_idx(node, idx, 1);
}

void
clock_enable(int node, const char *name)
{
	clock_do_enable(node, name, 1);
}

void
clock_disable_idx(int node, int idx)
{
	clock_do_enable_idx(node, idx, 0);
}

void
clock_disable(int node, const char *name)
{
	clock_do_enable(node, name, 0);
}

/*
 * Reset functionality.
 */

LIST_HEAD(, reset_device) reset_devices =
	LIST_HEAD_INITIALIZER(reset_devices);

void
reset_register(struct reset_device *rd)
{
	rd->rd_cells = OF_getpropint(rd->rd_node, "#reset-cells", 0);
	rd->rd_phandle = OF_getpropint(rd->rd_node, "phandle", 0);
	if (rd->rd_phandle == 0)
		return;

	LIST_INSERT_HEAD(&reset_devices, rd, rd_list);
}

void
reset_assert_cells(uint32_t *cells, int assert)
{
	struct reset_device *rd;
	uint32_t phandle = cells[0];

	LIST_FOREACH(rd, &reset_devices, rd_list) {
		if (rd->rd_phandle == phandle)
			break;
	}

	rd->rd_reset(rd->rd_cookie, &cells[1], assert);
}

uint32_t *
reset_next_reset(uint32_t *cells)
{
	uint32_t phandle = cells[0];
	int node, ncells;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return NULL;

	ncells = OF_getpropint(node, "#reset-cells", 0);
	return cells + ncells + 1;
}

int
reset_index(int node, const char *reset)
{
	char *names;
	char *name;
	char *end;
	int idx = 0;
	int len;

	if (reset == NULL)
		return 0;

	len = OF_getproplen(node, "reset-names");
	if (len <= 0)
		return -1;

	names = malloc(len, M_TEMP, M_WAITOK);
	OF_getprop(node, "reset-names", names, len);
	end = names + len;
	name = names;
	while (name < end) {
		if (strcmp(name, reset) == 0) {
			free(names, M_TEMP, len);
			return idx;
		}
		name += strlen(name) + 1;
		idx++;
	}
	free(names, M_TEMP, len);
	return -1;
}

void
reset_do_assert_idx(int node, int idx, int assert)
{
	uint32_t *resets;
	uint32_t *reset;
	int len;

	len = OF_getproplen(node, "resets");
	if (len <= 0)
		return;

	resets = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "resets", resets, len);

	reset = resets;
	while (reset && resets < resets + (len / sizeof(uint32_t))) {
		if (idx <= 0)
			reset_assert_cells(reset, assert);
		if (idx == 0)
			break;
		reset = reset_next_reset(reset);
		idx--;
	}

	free(resets, M_TEMP, len);
}

void
reset_do_assert(int node, const char *name, int assert)
{
	int idx;

	idx = reset_index(node, name);
	if (idx == -1)
		return;

	reset_do_assert_idx(node, idx, assert);
}

void
reset_assert(int node, const char *name)
{
	reset_do_assert(node, name, 1);
}

void
reset_deassert(int node, const char *name)
{
	reset_do_assert(node, name, 0);
}
