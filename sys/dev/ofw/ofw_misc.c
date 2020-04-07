/*	$OpenBSD: ofw_misc.c,v 1.19 2020/04/07 09:08:15 kettenis Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis
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

#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_regulator.h>

/*
 * Register maps.
 */

struct regmap {
	int			rm_node;
	uint32_t		rm_phandle;
	bus_space_tag_t		rm_tag;
	bus_space_handle_t	rm_handle;
	bus_size_t		rm_size;
	
	LIST_ENTRY(regmap)	rm_list;
};

LIST_HEAD(, regmap) regmaps = LIST_HEAD_INITIALIZER(regmap);

void
regmap_register(int node, bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t size)
{
	struct regmap *rm;

	rm = malloc(sizeof(struct regmap), M_DEVBUF, M_WAITOK);
	rm->rm_node = node;
	rm->rm_phandle = OF_getpropint(node, "phandle", 0);
	rm->rm_tag = tag;
	rm->rm_handle = handle;
	rm->rm_size = size;
	LIST_INSERT_HEAD(&regmaps, rm, rm_list);
}

struct regmap *
regmap_bycompatible(char *compatible)
{
	struct regmap *rm;

	LIST_FOREACH(rm, &regmaps, rm_list) {
		if (OF_is_compatible(rm->rm_node, compatible))
			return rm;
	}

	return NULL;
}

struct regmap *
regmap_bynode(int node)
{
	struct regmap *rm;

	LIST_FOREACH(rm, &regmaps, rm_list) {
		if (rm->rm_node == node)
			return rm;
	}

	return NULL;
}

struct regmap *
regmap_byphandle(uint32_t phandle)
{
	struct regmap *rm;

	LIST_FOREACH(rm, &regmaps, rm_list) {
		if (rm->rm_phandle == phandle)
			return rm;
	}

	return NULL;
}

void
regmap_write_4(struct regmap *rm, bus_size_t offset, uint32_t value)
{
	KASSERT(offset <= rm->rm_size - sizeof(uint32_t));
	bus_space_write_4(rm->rm_tag, rm->rm_handle, offset, value);
}

uint32_t
regmap_read_4(struct regmap *rm, bus_size_t offset)
{
	KASSERT(offset <= rm->rm_size - sizeof(uint32_t));
	return bus_space_read_4(rm->rm_tag, rm->rm_handle, offset);
}


/*
 * PHY support.
 */

LIST_HEAD(, phy_device) phy_devices =
	LIST_HEAD_INITIALIZER(phy_devices);

void
phy_register(struct phy_device *pd)
{
	pd->pd_cells = OF_getpropint(pd->pd_node, "#phy-cells", 0);
	pd->pd_phandle = OF_getpropint(pd->pd_node, "phandle", 0);
	if (pd->pd_phandle == 0)
		return;

	LIST_INSERT_HEAD(&phy_devices, pd, pd_list);
}

int
phy_usb_nop_enable(int node)
{
	uint32_t vcc_supply;
	uint32_t *gpio;
	int len;

	vcc_supply = OF_getpropint(node, "vcc-supply", 0);
	if (vcc_supply)
		regulator_enable(vcc_supply);

	len = OF_getproplen(node, "reset-gpios");
	if (len <= 0)
		return 0;

	/* There should only be a single GPIO pin. */
	gpio = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "reset-gpios", gpio, len);

	gpio_controller_config_pin(gpio, GPIO_CONFIG_OUTPUT);
	gpio_controller_set_pin(gpio, 1);
	delay(10000);
	gpio_controller_set_pin(gpio, 0);

	free(gpio, M_TEMP, len);

	return 0;
}

int
phy_enable_cells(uint32_t *cells)
{
	struct phy_device *pd;
	uint32_t phandle = cells[0];
	int node;

	LIST_FOREACH(pd, &phy_devices, pd_list) {
		if (pd->pd_phandle == phandle)
			break;
	}

	if (pd && pd->pd_enable)
		return pd->pd_enable(pd->pd_cookie, &cells[1]);

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return ENXIO;

	if (OF_is_compatible(node, "usb-nop-xceiv"))
		return phy_usb_nop_enable(node);

	return ENXIO;
}

uint32_t *
phy_next_phy(uint32_t *cells)
{
	uint32_t phandle = cells[0];
	int node, ncells;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return NULL;

	ncells = OF_getpropint(node, "#phy-cells", 0);
	return cells + ncells + 1;
}

int
phy_enable_idx(int node, int idx)
{
	uint32_t *phys;
	uint32_t *phy;
	int rv = -1;
	int len;

	len = OF_getproplen(node, "phys");
	if (len <= 0)
		return -1;

	phys = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "phys", phys, len);

	phy = phys;
	while (phy && phy < phys + (len / sizeof(uint32_t))) {
		if (idx <= 0)
			rv = phy_enable_cells(phy);
		if (idx == 0)
			break;
		phy = phy_next_phy(phy);
		idx--;
	}

	free(phys, M_TEMP, len);
	return rv;
}

int
phy_enable(int node, const char *name)
{
	int idx;

	idx = OF_getindex(node, name, "phy-names");
	if (idx == -1)
		return -1;

	return phy_enable_idx(node, idx);
}

/*
 * I2C support.
 */

LIST_HEAD(, i2c_bus) i2c_busses =
	LIST_HEAD_INITIALIZER(i2c_bus);

void
i2c_register(struct i2c_bus *ib)
{
	ib->ib_phandle = OF_getpropint(ib->ib_node, "phandle", 0);
	if (ib->ib_phandle == 0)
		return;

	LIST_INSERT_HEAD(&i2c_busses, ib, ib_list);
}

struct i2c_controller *
i2c_bynode(int node)
{
	struct i2c_bus *ib;

	LIST_FOREACH(ib, &i2c_busses, ib_list) {
		if (ib->ib_node == node)
			return ib->ib_ic;
	}

	return NULL;
}

struct i2c_controller *
i2c_byphandle(uint32_t phandle)
{
	struct i2c_bus *ib;

	LIST_FOREACH(ib, &i2c_busses, ib_list) {
		if (ib->ib_phandle == phandle)
			return ib->ib_ic;
	}

	return NULL;
}

/*
 * SFP support.
 */

LIST_HEAD(, sfp_device) sfp_devices =
	LIST_HEAD_INITIALIZER(sfp_devices);

void
sfp_register(struct sfp_device *sd)
{
	sd->sd_phandle = OF_getpropint(sd->sd_node, "phandle", 0);
	if (sd->sd_phandle == 0)
		return;

	LIST_INSERT_HEAD(&sfp_devices, sd, sd_list);
}

int
sfp_get_sffpage(uint32_t phandle, struct if_sffpage *sff)
{
	struct sfp_device *sd;

	LIST_FOREACH(sd, &sfp_devices, sd_list) {
		if (sd->sd_phandle == phandle)
			return sd->sd_get_sffpage(sd->sd_cookie, sff);
	}

	return ENXIO;
}

/*
 * PWM support.
 */

LIST_HEAD(, pwm_device) pwm_devices =
	LIST_HEAD_INITIALIZER(pwm_devices);

void
pwm_register(struct pwm_device *pd)
{
	pd->pd_cells = OF_getpropint(pd->pd_node, "#pwm-cells", 0);
	pd->pd_phandle = OF_getpropint(pd->pd_node, "phandle", 0);
	if (pd->pd_phandle == 0)
		return;

	LIST_INSERT_HEAD(&pwm_devices, pd, pd_list);

}

int
pwm_init_state(uint32_t *cells, struct pwm_state *ps)
{
	struct pwm_device *pd;

	LIST_FOREACH(pd, &pwm_devices, pd_list) {
		if (pd->pd_phandle == cells[0]) {
			memset(ps, 0, sizeof(struct pwm_state));
			pd->pd_get_state(pd->pd_cookie, &cells[1], ps);
			ps->ps_pulse_width = 0;
			if (pd->pd_cells >= 2)
				ps->ps_period = cells[2];
			if (pd->pd_cells >= 3)
				ps->ps_flags = cells[3];
			return 0;
		}
	}

	return ENXIO;
}

int
pwm_get_state(uint32_t *cells, struct pwm_state *ps)
{
	struct pwm_device *pd;

	LIST_FOREACH(pd, &pwm_devices, pd_list) {
		if (pd->pd_phandle == cells[0])
			return pd->pd_get_state(pd->pd_cookie, &cells[1], ps);
	}

	return ENXIO;
}

int
pwm_set_state(uint32_t *cells, struct pwm_state *ps)
{
	struct pwm_device *pd;

	LIST_FOREACH(pd, &pwm_devices, pd_list) {
		if (pd->pd_phandle == cells[0])
			return pd->pd_set_state(pd->pd_cookie, &cells[1], ps);
	}

	return ENXIO;
}

/*
 * Non-volatile memory support.
 */

LIST_HEAD(, nvmem_device) nvmem_devices =
	LIST_HEAD_INITIALIZER(nvmem_devices);

struct nvmem_cell {
	uint32_t	nc_phandle;
	struct nvmem_device *nc_nd;
	bus_addr_t	nc_addr;
	bus_size_t	nc_size;

	LIST_ENTRY(nvmem_cell) nc_list;
};

LIST_HEAD(, nvmem_cell) nvmem_cells =
	LIST_HEAD_INITIALIZER(nvmem_cells);

void
nvmem_register_child(int node, struct nvmem_device *nd)
{
	struct nvmem_cell *nc;
	uint32_t phandle;
	uint32_t reg[2];

	phandle = OF_getpropint(node, "phandle", 0);
	if (phandle == 0)
		return;

	if (OF_getpropintarray(node, "reg", reg, sizeof(reg)) != sizeof(reg))
		return;

	nc = malloc(sizeof(struct nvmem_cell), M_DEVBUF, M_WAITOK);
	nc->nc_phandle = phandle;
	nc->nc_nd = nd;
	nc->nc_addr = reg[0];
	nc->nc_size = reg[1];
	LIST_INSERT_HEAD(&nvmem_cells, nc, nc_list);
}

void
nvmem_register(struct nvmem_device *nd)
{
	int node;

	nd->nd_phandle = OF_getpropint(nd->nd_node, "phandle", 0);
	if (nd->nd_phandle)
		LIST_INSERT_HEAD(&nvmem_devices, nd, nd_list);

	for (node = OF_child(nd->nd_node); node; node = OF_peer(node))
		nvmem_register_child(node, nd);
}

int
nvmem_read(uint32_t phandle, bus_addr_t addr, void *data, bus_size_t size)
{
	struct nvmem_device *nd;

	LIST_FOREACH(nd, &nvmem_devices, nd_list) {
		if (nd->nd_phandle == phandle)
			return nd->nd_read(nd->nd_cookie, addr, data, size);
	}

	return ENXIO;
}

int
nvmem_read_cell(int node, const char *name, void *data, bus_size_t size)
{
	struct nvmem_device *nd;
	struct nvmem_cell *nc;
	uint32_t phandle, *phandles;
	int id, len;

	id = OF_getindex(node, name, "nvmem-cell-names");
	if (id < 0)
		return ENXIO;

	len = OF_getproplen(node, "nvmem-cells");
	if (len <= 0)
		return ENXIO;

	phandles = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "nvmem-cells", phandles, len);
	phandle = phandles[id];
	free(phandles, M_TEMP, len);

	LIST_FOREACH(nc, &nvmem_cells, nc_list) {
		if (nc->nc_phandle == phandle)
			break;
	}
	if (nc == NULL)
		return ENXIO;

	if (size > nc->nc_size)
		return EINVAL;

	nd = nc->nc_nd;
	return nd->nd_read(nd->nd_cookie, nc->nc_addr, data, size);
}

/* Port/endpoint interface support */

LIST_HEAD(, endpoint) endpoints =
	LIST_HEAD_INITIALIZER(endpoints);

void
endpoint_register(int node, struct device_port *dp, enum endpoint_type type)
{
	struct endpoint *ep;

	ep = malloc(sizeof(*ep), M_DEVBUF, M_WAITOK);
	ep->ep_node = node;
	ep->ep_phandle = OF_getpropint(node, "phandle", 0);
	ep->ep_reg = OF_getpropint(node, "reg", -1);
	ep->ep_port = dp;
	ep->ep_type = type;

	LIST_INSERT_HEAD(&endpoints, ep, ep_list);
	LIST_INSERT_HEAD(&dp->dp_endpoints, ep, ep_plist);
}

void
device_port_register(int node, struct device_ports *ports,
    enum endpoint_type type)
{
	struct device_port *dp;

	dp = malloc(sizeof(*dp), M_DEVBUF, M_WAITOK);
	dp->dp_node = node;
	dp->dp_phandle = OF_getpropint(node, "phandle", 0);
	dp->dp_reg = OF_getpropint(node, "reg", -1);
	dp->dp_ports = ports;
	LIST_INIT(&dp->dp_endpoints);
	for (node = OF_child(node); node; node = OF_peer(node))
		endpoint_register(node, dp, type);

	LIST_INSERT_HEAD(&ports->dp_ports, dp, dp_list);
}

void
device_ports_register(struct device_ports *ports,
    enum endpoint_type type)
{
	int node;

	LIST_INIT(&ports->dp_ports);

	node = OF_getnodebyname(ports->dp_node, "ports");
	if (node == 0) {
		node = OF_getnodebyname(ports->dp_node, "port");
		if (node == 0)
			return;
		
		device_port_register(node, ports, type);
		return;
	}

	for (node = OF_child(node); node; node = OF_peer(node))
		device_port_register(node, ports, type);
}

struct endpoint *
endpoint_byphandle(uint32_t phandle)
{
	struct endpoint *ep;

	LIST_FOREACH(ep, &endpoints, ep_list) {
		if (ep->ep_phandle == phandle)
			return ep;
	}

	return NULL;
}

struct endpoint *
endpoint_byreg(struct device_ports *ports, uint32_t dp_reg, uint32_t ep_reg)
{
	struct device_port *dp;
	struct endpoint *ep;

	LIST_FOREACH(dp, &ports->dp_ports, dp_list) {
		if (dp->dp_reg != dp_reg)
			continue;
		LIST_FOREACH(ep, &dp->dp_endpoints, ep_list) {
			if (ep->ep_reg != ep_reg)
				continue;
			return ep;
		}
	}

	return NULL;
}

struct endpoint *
endpoint_remote(struct endpoint *ep)
{
	struct endpoint *rep;
	int phandle;

	phandle = OF_getpropint(ep->ep_node, "remote-endpoint", 0);
	if (phandle == 0)
		return NULL;

	LIST_FOREACH(rep, &endpoints, ep_list) {
		if (rep->ep_phandle == phandle)
			return rep;
	}

	return NULL;
}

int
endpoint_activate(struct endpoint *ep, void *arg)
{
	struct device_ports *ports = ep->ep_port->dp_ports;
	return ports->dp_ep_activate(ports->dp_cookie, ep, arg);
}

void *
endpoint_get_cookie(struct endpoint *ep)
{
	struct device_ports *ports = ep->ep_port->dp_ports;
	return ports->dp_ep_get_cookie(ports->dp_cookie, ep);
}

int
device_port_activate(uint32_t phandle, void *arg)
{
	struct device_port *dp = NULL;
	struct endpoint *ep, *rep;
	int count;
	int error;

	LIST_FOREACH(ep, &endpoints, ep_list) {
		if (ep->ep_port->dp_phandle == phandle) {
			dp = ep->ep_port;
			break;
		}
	}
	if (dp == NULL)
		return ENXIO;

	count = 0;
	LIST_FOREACH(ep, &dp->dp_endpoints, ep_plist) {
		rep = endpoint_remote(ep);
		if (rep == NULL)
			continue;

		error = endpoint_activate(ep, arg);
		if (error)
			continue;
		error = endpoint_activate(rep, arg);
		if (error)
			continue;
		count++;
	}

	return count ? 0 : ENXIO;
}
