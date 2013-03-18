/*	$OpenBSD: intel_i2c.c,v 1.1 2013/03/18 12:36:52 jsg Exp $	*/
/*
 * Copyright (c) 2012 Mark Kettenis <kettenis@openbsd.org>
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

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/drm.h>
#include "i915_drv.h"
#include "i915_reg.h"

#include <dev/i2c/i2cvar.h>

int	gmbus_i2c_acquire_bus(void *, int);
void	gmbus_i2c_release_bus(void *, int);
int	gmbus_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
	    void *buf, size_t, int);
void	i915_i2c_probe(struct inteldrm_softc *);

int
gmbus_i2c_acquire_bus(void *cookie, int flags)
{
	struct gmbus_port *gp = cookie;
	struct inteldrm_softc *dev_priv = gp->dev_priv;

	I915_WRITE(dev_priv->gpio_mmio_base + GMBUS0,
	    GMBUS_RATE_100KHZ | gp->port);

	return (0);
}

void
gmbus_i2c_release_bus(void *cookie, int flags)
{
	struct gmbus_port *gp = cookie;
	struct inteldrm_softc *dev_priv = gp->dev_priv;

	I915_WRITE(dev_priv->gpio_mmio_base + GMBUS0, 0);
}

int
gmbus_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
	struct gmbus_port *gp = cookie;
	struct inteldrm_softc *dev_priv = gp->dev_priv;
	uint32_t reg, st, val;
	int reg_offset = dev_priv->gpio_mmio_base;
	uint8_t *b;
	int i, retries;
	uint16_t rem = len;
	int bus_err = 0;

	if (cmdlen > 1)
		return (EOPNOTSUPP);

	if (I2C_OP_WRITE_P(op)) {
		val = 0;	

		b = buf;
		for (i = 0; i < 4 && rem > 0; i++, rem--) {
			val |= *b++ << (8 * i);
		}

		I915_WRITE(GMBUS3 + reg_offset, val);
	}

	reg = 0;
	if (len > 0)
		reg |= GMBUS_CYCLE_WAIT;
	if (I2C_OP_STOP_P(op))
		reg |= GMBUS_CYCLE_STOP;
	if (I2C_OP_READ_P(op)) {
		reg |= GMBUS_SLAVE_READ;
		if (cmdlen > 0)
			reg |= GMBUS_CYCLE_INDEX;
		b = (void *)cmdbuf;
		if (cmdlen > 0)
			reg |= (b[0] << GMBUS_SLAVE_INDEX_SHIFT);
	}
	if (I2C_OP_WRITE_P(op))
		reg |= GMBUS_SLAVE_WRITE;
	reg |= (addr << GMBUS_SLAVE_ADDR_SHIFT);
	reg |= (len << GMBUS_BYTE_COUNT_SHIFT);
	I915_WRITE(GMBUS1 + reg_offset, reg | GMBUS_SW_RDY);

	if (I2C_OP_READ_P(op)) {
		b = buf;
		while (len > 0) {
			for (retries = 50; retries > 0; retries--) {
				st = I915_READ(GMBUS2 + reg_offset);
				if (st & (GMBUS_SATOER | GMBUS_HW_RDY))
					break;
				DELAY(1000);
			}
			if (st & GMBUS_SATOER) {
				bus_err = 1;
				goto out;
			}
			if ((st & GMBUS_HW_RDY) == 0)
				return (ETIMEDOUT);

			val = I915_READ(GMBUS3 + reg_offset);
			for (i = 0; i < 4 && len > 0; i++, len--) {
				*b++ = val & 0xff;
				val >>= 8;
			}
		}
	}

	if (I2C_OP_WRITE_P(op)) {
		while (rem > 0) {
			val = 0;
			for (i = 0; i < 4 && rem > 0; i++, rem--) {
				val |= *b++ << (8 * i);
			}
			I915_WRITE(GMBUS3 + reg_offset, val);
		}
	}

out:
	for (retries = 10; retries > 0; retries--) {
		st = I915_READ(GMBUS2 + reg_offset);
		if ((st & GMBUS_ACTIVE) == 0)
			break;
		DELAY(1000);
	}
	if (st & GMBUS_ACTIVE)
		return (ETIMEDOUT);

	/* after the bus is idle clear the bus error */
	if (bus_err) {
		I915_WRITE(GMBUS1 + reg_offset, GMBUS_SW_CLR_INT);
		I915_WRITE(GMBUS1 + reg_offset, 0);
		I915_WRITE(GMBUS0 + reg_offset, 0);
		return (ENXIO);
	}

	return (0);
}

void
i915_i2c_probe(struct inteldrm_softc *dev_priv)
{
	struct drm_device *dev = (struct drm_device *)dev_priv->drmdev;
	struct gmbus_port gp;
	struct i2c_controller ic;
	uint8_t buf[128];
	uint8_t cmd;
	int err, i;

	if (HAS_PCH_SPLIT(dev))
		dev_priv->gpio_mmio_base = PCH_GPIOA - GPIOA;
	else
		dev_priv->gpio_mmio_base = 0;

	gp.dev_priv = dev_priv;
	gp.port = GMBUS_PORT_PANEL;

	ic.ic_cookie = &gp;
	ic.ic_acquire_bus = gmbus_i2c_acquire_bus;
	ic.ic_release_bus = gmbus_i2c_release_bus;
	ic.ic_exec = gmbus_i2c_exec;

	bzero(buf, sizeof(buf));
	iic_acquire_bus(&ic, 0);
	cmd = 0;
	err = iic_exec(&ic, I2C_OP_READ_WITH_STOP, 0x50, &cmd, 1, buf, 128, 0);
	if (err)
		printf("err %d\n", err);
	iic_release_bus(&ic, 0);
	for (i = 0; i < sizeof(buf); i++)
		printf(" 0x%02x", buf[i]);
	printf("\n");
}

int
intel_setup_gmbus(struct inteldrm_softc *dev_priv)
{
	struct drm_device *dev = (struct drm_device *)dev_priv->drmdev;
	int i;

	if (HAS_PCH_SPLIT(dev))
		dev_priv->gpio_mmio_base = PCH_GPIOA - GPIOA;
	else
		dev_priv->gpio_mmio_base = 0;

	for (i = 0; i < GMBUS_NUM_PORTS; i++) {
		struct intel_gmbus *bus = &dev_priv->gmbus[i];

		bus->gp.dev_priv = dev_priv;
		bus->gp.port = i + 1;

		bus->controller.ic_cookie = &bus->gp;
		bus->controller.ic_acquire_bus = gmbus_i2c_acquire_bus;
		bus->controller.ic_release_bus = gmbus_i2c_release_bus;
		bus->controller.ic_exec = gmbus_i2c_exec;
	}

	return (0);
}

struct i2c_controller *
intel_gmbus_get_adapter(drm_i915_private_t *dev_priv, unsigned port)
{
	/* -1 to map pin pair to gmbus index */
	return (intel_gmbus_is_port_valid(port)) ?
	    &dev_priv->gmbus[port - 1].controller : NULL;
}
