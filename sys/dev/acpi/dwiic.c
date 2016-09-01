/* $OpenBSD: dwiic.c,v 1.17 2016/09/01 09:38:25 jcs Exp $ */
/*
 * Synopsys DesignWare I2C controller
 *
 * Copyright (c) 2015, 2016 joshua stein <jcs@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
#include <sys/kernel.h>
#include <sys/kthread.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/i2c/i2cvar.h>

/* #define DWIIC_DEBUG */

#ifdef DWIIC_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

/* register offsets */
#define DW_IC_CON		0x0
#define DW_IC_TAR		0x4
#define DW_IC_DATA_CMD		0x10
#define DW_IC_SS_SCL_HCNT	0x14
#define DW_IC_SS_SCL_LCNT	0x18
#define DW_IC_FS_SCL_HCNT	0x1c
#define DW_IC_FS_SCL_LCNT	0x20
#define DW_IC_INTR_STAT		0x2c
#define DW_IC_INTR_MASK		0x30
#define DW_IC_RAW_INTR_STAT	0x34
#define DW_IC_RX_TL		0x38
#define DW_IC_TX_TL		0x3c
#define DW_IC_CLR_INTR		0x40
#define DW_IC_CLR_RX_UNDER	0x44
#define DW_IC_CLR_RX_OVER	0x48
#define DW_IC_CLR_TX_OVER	0x4c
#define DW_IC_CLR_RD_REQ	0x50
#define DW_IC_CLR_TX_ABRT	0x54
#define DW_IC_CLR_RX_DONE	0x58
#define DW_IC_CLR_ACTIVITY	0x5c
#define DW_IC_CLR_STOP_DET	0x60
#define DW_IC_CLR_START_DET	0x64
#define DW_IC_CLR_GEN_CALL	0x68
#define DW_IC_ENABLE		0x6c
#define DW_IC_STATUS		0x70
#define DW_IC_TXFLR		0x74
#define DW_IC_RXFLR		0x78
#define DW_IC_SDA_HOLD		0x7c
#define DW_IC_TX_ABRT_SOURCE	0x80
#define DW_IC_ENABLE_STATUS	0x9c
#define DW_IC_COMP_PARAM_1	0xf4
#define DW_IC_COMP_VERSION	0xf8
#define DW_IC_SDA_HOLD_MIN_VERS	0x3131312A
#define DW_IC_COMP_TYPE		0xfc
#define DW_IC_COMP_TYPE_VALUE	0x44570140

#define DW_IC_CON_MASTER	0x1
#define DW_IC_CON_SPEED_STD	0x2
#define DW_IC_CON_SPEED_FAST	0x4
#define DW_IC_CON_10BITADDR_MASTER 0x10
#define DW_IC_CON_RESTART_EN	0x20
#define DW_IC_CON_SLAVE_DISABLE	0x40

#define DW_IC_DATA_CMD_READ	0x100
#define DW_IC_DATA_CMD_STOP	0x200
#define DW_IC_DATA_CMD_RESTART	0x400

#define DW_IC_INTR_RX_UNDER	0x001
#define DW_IC_INTR_RX_OVER	0x002
#define DW_IC_INTR_RX_FULL	0x004
#define DW_IC_INTR_TX_OVER	0x008
#define DW_IC_INTR_TX_EMPTY	0x010
#define DW_IC_INTR_RD_REQ	0x020
#define DW_IC_INTR_TX_ABRT	0x040
#define DW_IC_INTR_RX_DONE	0x080
#define DW_IC_INTR_ACTIVITY	0x100
#define DW_IC_INTR_STOP_DET	0x200
#define DW_IC_INTR_START_DET	0x400
#define DW_IC_INTR_GEN_CALL	0x800

#define DW_IC_STATUS_ACTIVITY	0x1

/* hardware abort codes from the DW_IC_TX_ABRT_SOURCE register */
#define ABRT_7B_ADDR_NOACK	0
#define ABRT_10ADDR1_NOACK	1
#define ABRT_10ADDR2_NOACK	2
#define ABRT_TXDATA_NOACK	3
#define ABRT_GCALL_NOACK	4
#define ABRT_GCALL_READ		5
#define ABRT_SBYTE_ACKDET	7
#define ABRT_SBYTE_NORSTRT	9
#define ABRT_10B_RD_NORSTRT	10
#define ABRT_MASTER_DIS		11
#define ARB_LOST		12

struct dwiic_crs {
	int irq_int;
	uint8_t irq_flags;
	uint32_t addr_min;
	uint32_t addr_bas;
	uint32_t addr_len;
	uint16_t i2c_addr;
	struct aml_node *devnode;
	struct aml_node *gpio_int_node;
	uint16_t gpio_int_pin;
	uint16_t gpio_int_flags;
};

struct dwiic_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;
	void			*sc_ih;

	struct i2cbus_attach_args sc_iba;
	struct device		*sc_iic;

	int			sc_poll;
	int			sc_busy;
	int			sc_readwait;
	int			sc_writewait;

	uint32_t		master_cfg;
	uint16_t		ss_hcnt, ss_lcnt, fs_hcnt, fs_lcnt;
	uint32_t		sda_hold_time;
	int			tx_fifo_depth;
	int			rx_fifo_depth;

	struct i2c_controller	sc_i2c_tag;
	struct rwlock		sc_i2c_lock;
	struct {
		i2c_op_t	op;
		void		*buf;
		size_t		len;
		int		flags;
		volatile int	error;
	} sc_i2c_xfer;
};

int		dwiic_match(struct device *, void *, void *);
void		dwiic_attach(struct device *, struct device *, void *);
int		dwiic_detach(struct device *, int);
int		dwiic_activate(struct device *, int);

int		dwiic_init(struct dwiic_softc *);
void		dwiic_enable(struct dwiic_softc *, int);
int		dwiic_intr(void *);

void *		dwiic_i2c_intr_establish(void *, void *, int,
		    int (*)(void *), void *, const char *);
const char *	dwiic_i2c_intr_string(void *, void *);

int		dwiic_acpi_parse_crs(union acpi_resource *, void *);
int		dwiic_acpi_found_hid(struct aml_node *, void *);
int		dwiic_acpi_found_ihidev(struct dwiic_softc *,
		    struct aml_node *, char *, struct dwiic_crs);
void		dwiic_acpi_get_params(struct dwiic_softc *, char *, uint16_t *,
		    uint16_t *, uint32_t *);
void		dwiic_acpi_power(struct dwiic_softc *, int);
void		dwiic_bus_scan(struct device *, struct i2cbus_attach_args *,
		    void *);

int		dwiic_i2c_acquire_bus(void *, int);
void		dwiic_i2c_release_bus(void *, int);
uint32_t	dwiic_read(struct dwiic_softc *, int);
void		dwiic_write(struct dwiic_softc *, int, uint32_t);
int		dwiic_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *,
		    size_t, void *, size_t, int);
void		dwiic_xfer_msg(struct dwiic_softc *);

struct cfattach dwiic_ca = {
	sizeof(struct dwiic_softc),
	dwiic_match,
	dwiic_attach,
	NULL,
	dwiic_activate
};

struct cfdriver dwiic_cd = {
	NULL, "dwiic", DV_DULL
};

const char *dwiic_hids[] = {
	"INT33C2",
	"INT33C3",
	"INT3432",
	"INT3433",
	"80860F41",
	"808622C1",
	NULL
};

const char *ihidev_hids[] = {
	"PNP0C50",
	"ACPI0C50",
	NULL
};

int
dwiic_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, dwiic_hids, cf->cf_driver->cd_name);
}

void
dwiic_attach(struct device *parent, struct device *self, void *aux)
{
	struct dwiic_softc *sc = (struct dwiic_softc *)self;
	struct acpi_attach_args *aa = aux;
	struct aml_value res;
	struct dwiic_crs crs;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;

	printf(": %s", sc->sc_devnode->name);

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_CRS", 0, NULL, &res)) {
		printf(", no _CRS method\n");
		return;
	}
	if (res.type != AML_OBJTYPE_BUFFER || res.length < 5) {
		printf(", invalid _CRS object (type %d len %d)\n",
		    res.type, res.length);
		aml_freevalue(&res);
		return;
	}
	memset(&crs, 0, sizeof(crs));
	crs.devnode = sc->sc_devnode;
	aml_parse_resource(&res, dwiic_acpi_parse_crs, &crs);
	aml_freevalue(&res);

	if (crs.addr_bas == 0) {
		printf(", can't find address\n");
		return;
	}

	printf(" addr 0x%x/0x%x", crs.addr_bas, crs.addr_len);

	sc->sc_iot = aa->aaa_memt;
	if (bus_space_map(sc->sc_iot, crs.addr_bas, crs.addr_len, 0,
	    &sc->sc_ioh)) {
		printf(", failed mapping at 0x%x\n", crs.addr_bas);
		return;
	}

	/* power up the controller */
	dwiic_acpi_power(sc, 1);

	/* fetch timing parameters */
	sc->ss_hcnt = dwiic_read(sc, DW_IC_SS_SCL_HCNT);
	sc->ss_lcnt = dwiic_read(sc, DW_IC_SS_SCL_LCNT);
	sc->fs_hcnt = dwiic_read(sc, DW_IC_FS_SCL_HCNT);
	sc->fs_lcnt = dwiic_read(sc, DW_IC_FS_SCL_LCNT);
	sc->sda_hold_time = dwiic_read(sc, DW_IC_SDA_HOLD);
	dwiic_acpi_get_params(sc, "SSCN", &sc->ss_hcnt, &sc->ss_lcnt, NULL);
	dwiic_acpi_get_params(sc, "FMCN", &sc->fs_hcnt, &sc->fs_lcnt,
	    &sc->sda_hold_time);

	if (dwiic_init(sc)) {
		printf(", failed initializing\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, crs.addr_len);
		return;
	}

	/* leave the controller disabled */
	dwiic_write(sc, DW_IC_INTR_MASK, 0);
	dwiic_enable(sc, 0);
	dwiic_read(sc, DW_IC_CLR_INTR);

	/* try to register interrupt with apic, but not fatal without it */
	if (crs.irq_int > 0) {
		printf(" irq %d", crs.irq_int);

		sc->sc_ih = acpi_intr_establish(crs.irq_int, crs.irq_flags,
		    IPL_BIO, dwiic_intr, sc, sc->sc_dev.dv_xname);
		if (sc->sc_ih == NULL)
			printf(", can't establish interrupt");
	}

	printf("\n");

	rw_init(&sc->sc_i2c_lock, "iiclk");

	/* setup and attach iic bus */
	sc->sc_i2c_tag.ic_cookie = sc;
	sc->sc_i2c_tag.ic_acquire_bus = dwiic_i2c_acquire_bus;
	sc->sc_i2c_tag.ic_release_bus = dwiic_i2c_release_bus;
	sc->sc_i2c_tag.ic_exec = dwiic_i2c_exec;
	sc->sc_i2c_tag.ic_intr_establish = dwiic_i2c_intr_establish;
	sc->sc_i2c_tag.ic_intr_string = dwiic_i2c_intr_string;

	bzero(&sc->sc_iba, sizeof(sc->sc_iba));
	sc->sc_iba.iba_name = "iic";
	sc->sc_iba.iba_tag = &sc->sc_i2c_tag;
	sc->sc_iba.iba_bus_scan = dwiic_bus_scan;
	sc->sc_iba.iba_bus_scan_arg = sc;

	config_found((struct device *)sc, &sc->sc_iba, iicbus_print);

	return;
}

int
dwiic_detach(struct device *self, int flags)
{
	struct dwiic_softc *sc = (struct dwiic_softc *)self;

	if (sc->sc_ih != NULL) {
		intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}

	return 0;
}

int
dwiic_activate(struct device *self, int act)
{
	struct dwiic_softc *sc = (struct dwiic_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		/* disable controller */
		dwiic_enable(sc, 0);

		/* disable interrupts */
		dwiic_write(sc, DW_IC_INTR_MASK, 0);
		dwiic_read(sc, DW_IC_CLR_INTR);

		/* power down the controller */
		dwiic_acpi_power(sc, 0);

		break;
	case DVACT_WAKEUP:
		/* power up the controller */
		dwiic_acpi_power(sc, 1);

		dwiic_init(sc);

		break;
	}

	config_activate_children(self, act);

	return 0;
}

int
dwiic_acpi_parse_crs(union acpi_resource *crs, void *arg)
{
	struct dwiic_crs *sc_crs = arg;
	struct aml_node *node;
	uint16_t pin;

	switch (AML_CRSTYPE(crs)) {
	case SR_IRQ:
		sc_crs->irq_int = ffs(letoh16(crs->sr_irq.irq_mask)) - 1;
		sc_crs->irq_flags = crs->sr_irq.irq_flags;
		break;

	case LR_EXTIRQ:
		sc_crs->irq_int = letoh32(crs->lr_extirq.irq[0]);
		sc_crs->irq_flags = crs->lr_extirq.flags;
		break;

	case LR_GPIO:
		node = aml_searchname(sc_crs->devnode,
		    (char *)&crs->pad[crs->lr_gpio.res_off]);
		pin = *(uint16_t *)&crs->pad[crs->lr_gpio.pin_off];
		if (crs->lr_gpio.type == LR_GPIO_INT) {
			sc_crs->gpio_int_node = node;
			sc_crs->gpio_int_pin = pin;
			sc_crs->gpio_int_flags = crs->lr_gpio.tflags;
		}
		break;

	case LR_MEM32:
		sc_crs->addr_min = letoh32(crs->lr_m32._min);
		sc_crs->addr_len = letoh32(crs->lr_m32._len);
		break;

	case LR_MEM32FIXED:
		sc_crs->addr_bas = letoh32(crs->lr_m32fixed._bas);
		sc_crs->addr_len = letoh32(crs->lr_m32fixed._len);
		break;

	case LR_SERBUS:
		if (crs->lr_serbus.type == LR_SERBUS_I2C)
			sc_crs->i2c_addr = letoh16(crs->lr_i2cbus._adr);
		break;

	default:
		DPRINTF(("%s: unknown resource type %d\n", __func__,
		    AML_CRSTYPE(crs)));
	}

	return 0;
}

void
dwiic_acpi_get_params(struct dwiic_softc *sc, char *method, uint16_t *hcnt,
    uint16_t *lcnt, uint32_t *sda_hold_time)
{
	struct aml_value res;

	if (!aml_searchname(sc->sc_devnode, method))
		return;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, method, 0, NULL, &res)) {
		printf(": eval of %s at %s failed", method,
		    aml_nodename(sc->sc_devnode));
		return;
	}

	if (res.type != AML_OBJTYPE_PACKAGE) {
		printf(": %s is not a package (%d)", method, res.type);
		aml_freevalue(&res);
		return;
	}

	if (res.length <= 2) {
		printf(": %s returned package of len %d", method, res.length);
		aml_freevalue(&res);
		return;
	}

	*hcnt = aml_val2int(res.v_package[0]);
	*lcnt = aml_val2int(res.v_package[1]);
	if (sda_hold_time)
		*sda_hold_time = aml_val2int(res.v_package[2]);
	aml_freevalue(&res);
}

void
dwiic_bus_scan(struct device *iic, struct i2cbus_attach_args *iba, void *aux)
{
	struct dwiic_softc *sc = (struct dwiic_softc *)aux;

	sc->sc_iic = iic;
	aml_find_node(sc->sc_devnode, "_HID", dwiic_acpi_found_hid, sc);
}

int
dwiic_i2c_print(void *aux, const char *pnp)
{
	struct i2c_attach_args *ia = aux;

	if (pnp != NULL)
		printf("%s at %s", ia->ia_name, pnp);

	printf(" addr 0x%x", ia->ia_addr);

	return UNCONF;
}

void *
dwiic_i2c_intr_establish(void *cookie, void *ih, int level,
    int (*func)(void *), void *arg, const char *name)
{
	struct dwiic_crs *crs = ih;

	if (crs->gpio_int_node && crs->gpio_int_node->gpio) {
		struct acpi_gpio *gpio = crs->gpio_int_node->gpio;
		gpio->intr_establish(gpio->cookie, crs->gpio_int_pin,
				     crs->gpio_int_flags, func, arg);
		return ih;
	}

	return acpi_intr_establish(crs->irq_int, crs->irq_flags,
	    level, func, arg, name);
}

const char *
dwiic_i2c_intr_string(void *cookie, void *ih)
{
	struct dwiic_crs *crs = ih;
	static char irqstr[64];

	if (crs->gpio_int_node && crs->gpio_int_node->gpio)
		snprintf(irqstr, sizeof(irqstr), "gpio %d", crs->gpio_int_pin);
	else
		snprintf(irqstr, sizeof(irqstr), "irq %d", crs->irq_int);

	return irqstr;
}

int
dwiic_matchhids(const char *hid, const char *hids[])
{
	int i;

	for (i = 0; hids[i]; i++)
		if (!strcmp(hid, hids[i]))
			return (1);

	return (0);
}

int
dwiic_acpi_found_hid(struct aml_node *node, void *arg)
{
	struct dwiic_softc *sc = (struct dwiic_softc *)arg;
	struct dwiic_crs crs;
	struct aml_value res;
	int64_t sta;
	char cdev[16], dev[16];

	if (acpi_parsehid(node, arg, cdev, dev, 16) != 0)
		return 0;

	if (aml_evalinteger(acpi_softc, node->parent, "_STA", 0, NULL, &sta))
		sta = STA_PRESENT | STA_ENABLED | STA_DEV_OK | 0x1000;

	if ((sta & STA_PRESENT) == 0)
		return 0;

	DPRINTF(("%s: found HID %s at %s\n", sc->sc_dev.dv_xname, dev,
	    aml_nodename(node)));

	if (aml_evalname(acpi_softc, node->parent, "_CRS", 0, NULL, &res)) {
		printf("%s: no _CRS method at %s\n", sc->sc_dev.dv_xname,
		    aml_nodename(node->parent));
		return (0);
	}
	if (res.type != AML_OBJTYPE_BUFFER || res.length < 5) {
		printf("%s: invalid _CRS object (type %d len %d)\n",
		    sc->sc_dev.dv_xname, res.type, res.length);
		aml_freevalue(&res);
		return (0);
	}
	memset(&crs, 0, sizeof(crs));
	crs.devnode = sc->sc_devnode;
	aml_parse_resource(&res, dwiic_acpi_parse_crs, &crs);
	aml_freevalue(&res);

	if (dwiic_matchhids(cdev, ihidev_hids))
		return dwiic_acpi_found_ihidev(sc, node, dev, crs);

	return 0;
}

int
dwiic_acpi_found_ihidev(struct dwiic_softc *sc, struct aml_node *node,
    char *dev, struct dwiic_crs crs)
{
	struct i2c_attach_args ia;
	struct aml_value cmd[4], res;

	/* 3cdff6f7-4267-4555-ad05-b30a3d8938de */
	static uint8_t i2c_hid_guid[] = {
		0xF7, 0xF6, 0xDF, 0x3C, 0x67, 0x42, 0x55, 0x45,
		0xAD, 0x05, 0xB3, 0x0A, 0x3D, 0x89, 0x38, 0xDE,
	};

	if (!aml_searchname(node->parent, "_DSM")) {
		printf("%s: couldn't find _DSM at %s\n", sc->sc_dev.dv_xname,
		    aml_nodename(node->parent));
		return 0;
	}

	bzero(&cmd, sizeof(cmd));
	cmd[0].type = AML_OBJTYPE_BUFFER;
	cmd[0].v_buffer = (uint8_t *)&i2c_hid_guid;
	cmd[0].length = sizeof(i2c_hid_guid);
	/* rev */
	cmd[1].type = AML_OBJTYPE_INTEGER;
	cmd[1].v_integer = 1;
	cmd[1].length = 1;
	/* func */
	cmd[2].type = AML_OBJTYPE_INTEGER;
	cmd[2].v_integer = 1; /* HID */
	cmd[2].length = 1;
	/* not used */
	cmd[3].type = AML_OBJTYPE_PACKAGE;
	cmd[3].length = 0;

	if (aml_evalname(acpi_softc, node->parent, "_DSM", 4, cmd, &res)) {
		printf("%s: eval of _DSM at %s failed\n",
		    sc->sc_dev.dv_xname, aml_nodename(node->parent));
		return 0;
	}

	if (res.type != AML_OBJTYPE_INTEGER) {
		printf("%s: bad _DSM result at %s: %d\n",
		    sc->sc_dev.dv_xname, aml_nodename(node->parent), res.type);
		aml_freevalue(&res);
		return 0;
	}

	memset(&ia, 0, sizeof(ia));
	ia.ia_tag = sc->sc_iba.iba_tag;
	ia.ia_size = 1;
	ia.ia_name = "ihidev";
	ia.ia_size = aml_val2int(&res); /* hid descriptor address */
	ia.ia_addr = crs.i2c_addr;
	ia.ia_cookie = dev;

	aml_freevalue(&res);

	if (crs.irq_int <= 0 && crs.gpio_int_node == NULL) {
		printf("%s: couldn't find irq for %s\n", sc->sc_dev.dv_xname,
		    aml_nodename(node->parent));
		return 0;
	}
	ia.ia_intr = &crs;

	if (config_found(sc->sc_iic, &ia, dwiic_i2c_print))
		return 0;

	return 1;
}

uint32_t
dwiic_read(struct dwiic_softc *sc, int offset)
{
	u_int32_t b = bus_space_read_4(sc->sc_iot, sc->sc_ioh, offset);

	DPRINTF(("%s: read at 0x%x = 0x%x\n", sc->sc_dev.dv_xname, offset, b));

	return b;
}

void
dwiic_write(struct dwiic_softc *sc, int offset, uint32_t val)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, offset, val);

	DPRINTF(("%s: write at 0x%x: 0x%x\n", sc->sc_dev.dv_xname, offset,
	    val));
}

int
dwiic_i2c_acquire_bus(void *cookie, int flags)
{
	struct dwiic_softc *sc = cookie;

	if (cold || sc->sc_poll || (flags & I2C_F_POLL))
		return (0);

	return rw_enter(&sc->sc_i2c_lock, RW_WRITE | RW_INTR);
}

void
dwiic_i2c_release_bus(void *cookie, int flags)
{
	struct dwiic_softc *sc = cookie;

	if (cold || sc->sc_poll || (flags & I2C_F_POLL))
		return;

	rw_exit(&sc->sc_i2c_lock);
}

int
dwiic_init(struct dwiic_softc *sc)
{
	uint32_t reg;

	/* make sure we're talking to a device we know */
	reg = dwiic_read(sc, DW_IC_COMP_TYPE);
	if (reg != DW_IC_COMP_TYPE_VALUE) {
		DPRINTF(("%s: invalid component type 0x%x\n",
		    sc->sc_dev.dv_xname, reg));
		return 1;
	}

	/* disable the adapter */
	dwiic_enable(sc, 0);

	/* write standard-mode SCL timing parameters */
	dwiic_write(sc, DW_IC_SS_SCL_HCNT, sc->ss_hcnt);
	dwiic_write(sc, DW_IC_SS_SCL_LCNT, sc->ss_lcnt);

	/* and fast-mode SCL timing parameters */
	dwiic_write(sc, DW_IC_FS_SCL_HCNT, sc->fs_hcnt);
	dwiic_write(sc, DW_IC_FS_SCL_LCNT, sc->fs_lcnt);

	/* SDA hold time */
	reg = dwiic_read(sc, DW_IC_COMP_VERSION);
	if (reg >= DW_IC_SDA_HOLD_MIN_VERS)
		dwiic_write(sc, DW_IC_SDA_HOLD, sc->sda_hold_time);

	/* FIFO threshold levels */
	sc->tx_fifo_depth = 32;
	sc->rx_fifo_depth = 32;
	dwiic_write(sc, DW_IC_TX_TL, sc->tx_fifo_depth / 2);
	dwiic_write(sc, DW_IC_RX_TL, 0);

	/* configure as i2c master with fast speed */
	sc->master_cfg = DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
	    DW_IC_CON_RESTART_EN | DW_IC_CON_SPEED_FAST;
	dwiic_write(sc, DW_IC_CON, sc->master_cfg);

	return 0;
}

void
dwiic_enable(struct dwiic_softc *sc, int enable)
{
	int retries;

	for (retries = 100; retries > 0; retries--) {
		dwiic_write(sc, DW_IC_ENABLE, enable);
		if ((dwiic_read(sc, DW_IC_ENABLE_STATUS) & 1) == enable)
			return;

		DELAY(25);
	}

	printf("%s: failed to %sable\n", sc->sc_dev.dv_xname,
	    (enable ? "en" : "dis"));
}

void
dwiic_acpi_power(struct dwiic_softc *sc, int power)
{
	char ps[] = "_PS0";

	if (!power)
		ps[3] = '3';

	if (aml_searchname(sc->sc_devnode, ps)) {
		if (aml_evalname(sc->sc_acpi, sc->sc_devnode, ps, 0, NULL,
		    NULL)) {
			printf("%s: failed powering %s with %s\n",
			    sc->sc_dev.dv_xname, power ? "on" : "off",
			    ps);
			return;
		}

		DELAY(10000); /* 10 milliseconds */
	} else
		DPRINTF(("%s: no %s method\n", sc->sc_dev.dv_xname, ps));
}

int
dwiic_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr, const void *cmdbuf,
    size_t cmdlen, void *buf, size_t len, int flags)
{
	struct dwiic_softc *sc = cookie;
	u_int32_t ic_con, st, cmd, resp;
	int retries, tx_limit, rx_avail, x, readpos;
	uint8_t *b;

	if (sc->sc_busy)
		return 1;

	sc->sc_busy++;

	DPRINTF(("%s: %s: op %d, addr 0x%02x, cmdlen %zu, len %zu, "
	    "flags 0x%02x\n", sc->sc_dev.dv_xname, __func__, op, addr, cmdlen,
	    len, flags));

	/* setup transfer */
	sc->sc_i2c_xfer.op = op;
	sc->sc_i2c_xfer.buf = buf;
	sc->sc_i2c_xfer.len = len;
	sc->sc_i2c_xfer.flags = flags;
	sc->sc_i2c_xfer.error = 0;

	/* wait for bus to be idle */
	for (retries = 100; retries > 0; retries--) {
		st = dwiic_read(sc, DW_IC_STATUS);
		if (!(st & DW_IC_STATUS_ACTIVITY))
			break;
		DELAY(1000);
	}
	DPRINTF(("%s: %s: status 0x%x\n", sc->sc_dev.dv_xname, __func__, st));
	if (st & DW_IC_STATUS_ACTIVITY) {
		sc->sc_busy = 0;
		return (1);
	}

	if (cold || sc->sc_poll)
		flags |= I2C_F_POLL;

	/* disable controller */
	dwiic_enable(sc, 0);

	/* set slave address */
	ic_con = dwiic_read(sc, DW_IC_CON);
	ic_con &= ~DW_IC_CON_10BITADDR_MASTER;
	dwiic_write(sc, DW_IC_CON, ic_con);
	dwiic_write(sc, DW_IC_TAR, addr);

	/* disable interrupts */
	dwiic_write(sc, DW_IC_INTR_MASK, 0);

	/* enable controller */
	dwiic_enable(sc, 1);

	/* wait until the controller is ready for commands */
	if (flags & I2C_F_POLL)
		DELAY(200);
	else {
		dwiic_read(sc, DW_IC_CLR_INTR);
		dwiic_write(sc, DW_IC_INTR_MASK, DW_IC_INTR_TX_EMPTY);

		if (tsleep(&sc->sc_writewait, PRIBIO, "dwiic", hz / 2) != 0)
			printf("%s: timed out waiting for tx_empty intr\n",
			    sc->sc_dev.dv_xname);
	}

	/* send our command, one byte at a time */
	if (cmdlen > 0) {
		b = (void *)cmdbuf;

		DPRINTF(("%s: %s: sending cmd (len %zu):", sc->sc_dev.dv_xname,
		    __func__, cmdlen));
		for (x = 0; x < cmdlen; x++)
			DPRINTF((" %02x", b[x]));
		DPRINTF(("\n"));

		tx_limit = sc->tx_fifo_depth - dwiic_read(sc, DW_IC_TXFLR);
		if (cmdlen > tx_limit) {
			/* TODO */
			printf("%s: can't write %zu (> %d)\n",
			    sc->sc_dev.dv_xname, cmdlen, tx_limit);
			sc->sc_i2c_xfer.error = 1;
			sc->sc_busy = 0;
			return (1);
		}

		for (x = 0; x < cmdlen; x++) {
			cmd = b[x];
			/*
			 * Generate STOP condition if this is the last
			 * byte of the transfer.
			 */
			if (x == (cmdlen - 1) && len == 0 && I2C_OP_STOP_P(op))
				cmd |= DW_IC_DATA_CMD_STOP;
			dwiic_write(sc, DW_IC_DATA_CMD, cmd);
		}
	}

	b = (void *)buf;
	x = readpos = 0;
	tx_limit = sc->tx_fifo_depth - dwiic_read(sc, DW_IC_TXFLR);

	DPRINTF(("%s: %s: need to read %zu bytes, can send %d read reqs\n",
		sc->sc_dev.dv_xname, __func__, len, tx_limit));

	while (x < len) {
		if (I2C_OP_WRITE_P(op))
			cmd = b[x];
		else
			cmd = DW_IC_DATA_CMD_READ;

		/*
		 * Generate RESTART condition if we're reversing
		 * direction.
		 */
		if (x == 0 && cmdlen > 0 && I2C_OP_READ_P(op))
			cmd |= DW_IC_DATA_CMD_RESTART;
		/*
		 * Generate STOP conditon on the last byte of the
		 * transfer.
		 */
		if (x == (len - 1) && I2C_OP_STOP_P(op))
			cmd |= DW_IC_DATA_CMD_STOP;

		dwiic_write(sc, DW_IC_DATA_CMD, cmd);

		tx_limit--;
		x++;

		/*
		 * As TXFLR fills up, we need to clear it out by reading all
		 * available data.
		 */
		while (tx_limit == 0 || x == len) {
			DPRINTF(("%s: %s: tx_limit %d, sent %d read reqs\n",
			    sc->sc_dev.dv_xname, __func__, tx_limit, x));

			if (flags & I2C_F_POLL) {
				for (retries = 100; retries > 0; retries--) {
					rx_avail = dwiic_read(sc, DW_IC_RXFLR);
					if (rx_avail > 0)
						break;
					DELAY(50);
				}
			} else {
				dwiic_read(sc, DW_IC_CLR_INTR);
				dwiic_write(sc, DW_IC_INTR_MASK,
				    DW_IC_INTR_RX_FULL);

				if (tsleep(&sc->sc_readwait, PRIBIO, "dwiic",
				    hz / 2) != 0)
					printf("%s: timed out waiting for "
					    "rx_full intr\n",
					    sc->sc_dev.dv_xname);

				rx_avail = dwiic_read(sc, DW_IC_RXFLR);
			}

			if (rx_avail == 0) {
				printf("%s: timed out reading remaining %d\n",
				    sc->sc_dev.dv_xname,
				    (int)(len - 1 - readpos));
				sc->sc_i2c_xfer.error = 1;
				sc->sc_busy = 0;
				return (1);
			}

			DPRINTF(("%s: %s: %d avail to read (%zu remaining)\n",
			    sc->sc_dev.dv_xname, __func__, rx_avail,
			    len - readpos));

			while (rx_avail > 0) {
				resp = dwiic_read(sc, DW_IC_DATA_CMD);
				if (readpos < len) {
					b[readpos] = resp;
					readpos++;
				}
				rx_avail--;
			}

			if (readpos >= len)
				break;

			DPRINTF(("%s: still need to read %d bytes\n",
			    sc->sc_dev.dv_xname, (int)(len - readpos)));
			tx_limit = sc->tx_fifo_depth -
			    dwiic_read(sc, DW_IC_TXFLR);
		}
	}

	sc->sc_busy = 0;

	return 0;
}

uint32_t
dwiic_read_clear_intrbits(struct dwiic_softc *sc)
{
       uint32_t stat;

       stat = dwiic_read(sc, DW_IC_INTR_STAT);

       if (stat & DW_IC_INTR_RX_UNDER)
	       dwiic_read(sc, DW_IC_CLR_RX_UNDER);
       if (stat & DW_IC_INTR_RX_OVER)
	       dwiic_read(sc, DW_IC_CLR_RX_OVER);
       if (stat & DW_IC_INTR_TX_OVER)
	       dwiic_read(sc, DW_IC_CLR_TX_OVER);
       if (stat & DW_IC_INTR_RD_REQ)
	       dwiic_read(sc, DW_IC_CLR_RD_REQ);
       if (stat & DW_IC_INTR_TX_ABRT)
	       dwiic_read(sc, DW_IC_CLR_TX_ABRT);
       if (stat & DW_IC_INTR_RX_DONE)
	       dwiic_read(sc, DW_IC_CLR_RX_DONE);
       if (stat & DW_IC_INTR_ACTIVITY)
	       dwiic_read(sc, DW_IC_CLR_ACTIVITY);
       if (stat & DW_IC_INTR_STOP_DET)
	       dwiic_read(sc, DW_IC_CLR_STOP_DET);
       if (stat & DW_IC_INTR_START_DET)
	       dwiic_read(sc, DW_IC_CLR_START_DET);
       if (stat & DW_IC_INTR_GEN_CALL)
	       dwiic_read(sc, DW_IC_CLR_GEN_CALL);

       return stat;
}

int
dwiic_intr(void *arg)
{
	struct dwiic_softc *sc = arg;
	uint32_t en, stat;

	en = dwiic_read(sc, DW_IC_ENABLE);
	/* probably for the other controller */
	if (!en)
		return 0;

	stat = dwiic_read_clear_intrbits(sc);
	DPRINTF(("%s: %s: enabled=0x%x stat=0x%x\n", sc->sc_dev.dv_xname,
	    __func__, en, stat));
	if (!(stat & ~DW_IC_INTR_ACTIVITY))
		return 1;

	if (stat & DW_IC_INTR_TX_ABRT)
		sc->sc_i2c_xfer.error = 1;

	if (sc->sc_i2c_xfer.flags & I2C_F_POLL)
		DPRINTF(("%s: %s: intr in poll mode?\n", sc->sc_dev.dv_xname,
		    __func__));
	else {
		if (stat & DW_IC_INTR_RX_FULL) {
			dwiic_write(sc, DW_IC_INTR_MASK, 0);
			DPRINTF(("%s: %s: waking up reader\n",
			    sc->sc_dev.dv_xname, __func__));
			wakeup(&sc->sc_readwait);
		}
		if (stat & DW_IC_INTR_TX_EMPTY) {
			dwiic_write(sc, DW_IC_INTR_MASK, 0);
			DPRINTF(("%s: %s: waking up writer\n",
			    sc->sc_dev.dv_xname, __func__));
			wakeup(&sc->sc_writewait);
		}
	}

	return 1;
}
