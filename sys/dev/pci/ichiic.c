/*	$OpenBSD: ichiic.c,v 1.12 2006/01/15 10:28:16 grange Exp $	*/

/*
 * Copyright (c) 2005, 2006 Alexander Yurchenko <grange@openbsd.org>
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

/*
 * Intel ICH SMBus controller driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/ichreg.h>

#include <dev/i2c/i2cvar.h>

#ifdef ICHIIC_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define ICHIIC_DELAY	100
#define ICHIIC_TIMEOUT	1

struct ichiic_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	void *			sc_ih;
	int			sc_poll;

	struct i2c_controller	sc_i2c_tag;
	struct lock		sc_i2c_lock;
	struct {
		i2c_op_t     op;
		void *       buf;
		size_t       len;
		int          flags;
		volatile int error;
	}			sc_i2c_xfer;
};

int	ichiic_match(struct device *, void *, void *);
void	ichiic_attach(struct device *, struct device *, void *);

int	ichiic_i2c_acquire_bus(void *, int);
void	ichiic_i2c_release_bus(void *, int);
int	ichiic_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
	    void *, size_t, int);

int	ichiic_intr(void *);

struct cfattach ichiic_ca = {
	sizeof(struct ichiic_softc),
	ichiic_match,
	ichiic_attach
};

struct cfdriver ichiic_cd = {
	NULL, "ichiic", DV_DULL
};

const struct pci_matchid ichiic_ids[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_6300ESB_SMB },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801AA_SMB },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801AB_SMB },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801BA_SMB },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801CA_SMB },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801DB_SMB },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801E_SMB },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801EB_SMB },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801FB_SMB },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82801GB_SMB },
};

int
ichiic_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, ichiic_ids,
	    sizeof(ichiic_ids) / sizeof(ichiic_ids[0])));
}

void
ichiic_attach(struct device *parent, struct device *self, void *aux)
{
	struct ichiic_softc *sc = (struct ichiic_softc *)self;
	struct pci_attach_args *pa = aux;
	struct i2cbus_attach_args iba;
	pcireg_t conf;
	bus_size_t iosize;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;

	/* Read configuration */
	conf = pci_conf_read(pa->pa_pc, pa->pa_tag, ICH_SMB_HOSTC);
	DPRINTF((": conf 0x%x", conf));

	if ((conf & ICH_SMB_HOSTC_HSTEN) == 0) {
		printf(": SMBus disabled\n");
		return;
	}

	/* Map I/O space */
	if (pci_mapreg_map(pa, ICH_SMB_BASE, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &iosize, 0)) {
		printf(": can't map I/O space\n");
		return;
	}

	sc->sc_poll = 1;
	if (conf & ICH_SMB_HOSTC_SMIEN) {
		/* No PCI IRQ */
		printf(": SMI");
	} else {
		/* Install interrupt handler */
		if (pci_intr_map(pa, &ih) == 0) {
			intrstr = pci_intr_string(pa->pa_pc, ih);
			sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
			    ichiic_intr, sc, sc->sc_dev.dv_xname);
			if (sc->sc_ih != NULL) {
				printf(": %s", intrstr);
				sc->sc_poll = 0;
			}
		}
		if (sc->sc_poll)
			printf(": polling");
	}

	printf("\n");

	/* Attach I2C bus */
	lockinit(&sc->sc_i2c_lock, PRIBIO | PCATCH, "iiclk", 0, 0);
	sc->sc_i2c_tag.ic_cookie = sc;
	sc->sc_i2c_tag.ic_acquire_bus = ichiic_i2c_acquire_bus;
	sc->sc_i2c_tag.ic_release_bus = ichiic_i2c_release_bus;
	sc->sc_i2c_tag.ic_exec = ichiic_i2c_exec;

	bzero(&iba, sizeof(iba));
	iba.iba_name = "iic";
	iba.iba_tag = &sc->sc_i2c_tag;
	config_found(self, &iba, iicbus_print);

	return;
}

int
ichiic_i2c_acquire_bus(void *cookie, int flags)
{
	struct ichiic_softc *sc = cookie;

	if (cold || sc->sc_poll || (flags & I2C_F_POLL))
		return (0);

	return (lockmgr(&sc->sc_i2c_lock, LK_EXCLUSIVE, NULL));
}

void
ichiic_i2c_release_bus(void *cookie, int flags)
{
	struct ichiic_softc *sc = cookie;

	if (cold || sc->sc_poll || (flags & I2C_F_POLL))
		return;

	lockmgr(&sc->sc_i2c_lock, LK_RELEASE, NULL);
}

int
ichiic_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
	struct ichiic_softc *sc = cookie;
	u_int8_t *b;
	u_int8_t ctl, st;
	int retries;

	DPRINTF(("%s: exec: op %d, addr 0x%x, cmdlen %d, len %d, flags 0x%x\n",
	    sc->sc_dev.dv_xname, op, addr, cmdlen, len, flags));

	/* Wait for bus to be idle */
	for (retries = 100; retries > 0; retries--) {
		st = bus_space_read_1(sc->sc_iot, sc->sc_ioh, ICH_SMB_HS);
		if (!(st & ICH_SMB_HS_BUSY))
			break;
		DELAY(ICHIIC_DELAY);
	}
	DPRINTF(("%s: exec: st 0x%b\n", sc->sc_dev.dv_xname, st,
	    ICH_SMB_HS_BITS));
	if (st & ICH_SMB_HS_BUSY)
		return (1);

	if (cold || sc->sc_poll)
		flags |= I2C_F_POLL;

	if (!I2C_OP_STOP_P(op) || cmdlen > 1 || len > 2)
		return (1);

	/* Setup transfer */
	sc->sc_i2c_xfer.op = op;
	sc->sc_i2c_xfer.buf = buf;
	sc->sc_i2c_xfer.len = len;
	sc->sc_i2c_xfer.flags = flags;
	sc->sc_i2c_xfer.error = 0;

	/* Set slave address and transfer direction */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ICH_SMB_TXSLVA,
	    ICH_SMB_TXSLVA_ADDR(addr) |
	    (I2C_OP_READ_P(op) ? ICH_SMB_TXSLVA_READ : 0));

	b = (void *)cmdbuf;
	if (cmdlen > 0)
		/* Set command byte */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, ICH_SMB_HCMD, b[0]);

	if (I2C_OP_WRITE_P(op)) {
		/* Write data */
		b = buf;
		if (len > 0)
			bus_space_write_1(sc->sc_iot, sc->sc_ioh,
			    ICH_SMB_HD0, b[0]);
		if (len > 1)
			bus_space_write_1(sc->sc_iot, sc->sc_ioh,
			    ICH_SMB_HD1, b[1]);
	}

	/* Set SMBus command */
	if (len == 0)
		ctl = ICH_SMB_HC_CMD_BYTE;
	else if (len == 1)
		ctl = ICH_SMB_HC_CMD_BDATA;
	else if (len == 2)
		ctl = ICH_SMB_HC_CMD_WDATA;

	if ((flags & I2C_F_POLL) == 0)
		ctl |= ICH_SMB_HC_INTREN;

	/* Start transaction */
	ctl |= ICH_SMB_HC_START;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ICH_SMB_HC, ctl);

	if (flags & I2C_F_POLL) {
		/* Poll for completion */
		DELAY(ICHIIC_DELAY);
		for (retries = 1000; retries > 0; retries--) {
			st = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    ICH_SMB_HS);
			if ((st & ICH_SMB_HS_BUSY) == 0)
				break;
			DELAY(ICHIIC_DELAY);
		}
		if (st & ICH_SMB_HS_BUSY)
			goto timeout;
		ichiic_intr(sc);
	} else {
		/* Wait for interrupt */
		if (tsleep(sc, PRIBIO, "iicexec", ICHIIC_TIMEOUT * hz))
			goto timeout;
	}

	if (sc->sc_i2c_xfer.error)
		return (1);

	return (0);

timeout:
	/*
	 * Transfer timeout. Kill the transaction and clear status bits.
	 */
	printf("%s: timeout, status 0x%b\n", sc->sc_dev.dv_xname, st,
	    ICH_SMB_HS_BITS);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ICH_SMB_HC,
	    ICH_SMB_HC_KILL);
	DELAY(ICHIIC_DELAY);
	st = bus_space_read_1(sc->sc_iot, sc->sc_ioh, ICH_SMB_HS);
	if ((st & ICH_SMB_HS_FAILED) == 0)
		printf("%s: transaction abort failed, status 0x%b\n",
		    sc->sc_dev.dv_xname, st, ICH_SMB_HS_BITS);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ICH_SMB_HS, st);
	return (1);
}

int
ichiic_intr(void *arg)
{
	struct ichiic_softc *sc = arg;
	u_int8_t st;
	u_int8_t *b;
	size_t len;

	/* Read status */
	st = bus_space_read_1(sc->sc_iot, sc->sc_ioh, ICH_SMB_HS);
	if ((st & ICH_SMB_HS_BUSY) != 0 || (st & (ICH_SMB_HS_INTR |
	    ICH_SMB_HS_DEVERR | ICH_SMB_HS_BUSERR | ICH_SMB_HS_FAILED |
	    ICH_SMB_HS_SMBAL | ICH_SMB_HS_BDONE)) == 0)
		/* Interrupt was not for us */
		return (0);

	DPRINTF(("%s: intr st 0x%b\n", sc->sc_dev.dv_xname, st,
	    ICH_SMB_HS_BITS));

	/* Clear status bits */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ICH_SMB_HS, st);

	/* Check for errors */
	if (st & (ICH_SMB_HS_DEVERR | ICH_SMB_HS_BUSERR | ICH_SMB_HS_FAILED)) {
		sc->sc_i2c_xfer.error = 1;
		goto done;
	}

	if (st & ICH_SMB_HS_INTR) {
		if (I2C_OP_WRITE_P(sc->sc_i2c_xfer.op))
			goto done;

		/* Read data */
		b = sc->sc_i2c_xfer.buf;
		len = sc->sc_i2c_xfer.len;
		if (len > 0)
			b[0] = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    ICH_SMB_HD0);
		if (len > 1)
			b[1] = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    ICH_SMB_HD1);
	}

done:
	if ((sc->sc_i2c_xfer.flags & I2C_F_POLL) == 0)
		wakeup(sc);
	return (1);
}
