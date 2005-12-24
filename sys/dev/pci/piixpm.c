/*	$OpenBSD: piixpm.c,v 1.3 2005/12/24 21:04:20 grange Exp $	*/

/*
 * Copyright (c) 2005 Alexander Yurchenko <grange@openbsd.org>
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
 * Intel PIIX and compatible power management and SMBus controller driver.
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

#include <dev/i2c/i2cvar.h>

#ifdef PIIXPM_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define PIIXPM_DELAY	100
#define PIIXPM_TIMEOUT	1

/* Register definitions */
#define PIIX_SMB_BASE	0x90		/* SMBus base address */
#define PIIX_SMB_HOSTC	0xd0		/* host configuration */
#define PIIX_SMB_HOSTC_HSTEN	(1 << 16)	/* enable host controller */
#define PIIX_SMB_HOSTC_SMI	(0 << 17)	/* SMI */
#define PIIX_SMB_HOSTC_IRQ	(4 << 17)	/* IRQ */

#define PIIX_SMB_HS	0x00		/* host status */
#define PIIX_SMB_HS_BUSY	(1 << 0)	/* running a command */
#define PIIX_SMB_HS_INTR	(1 << 1)	/* command completed */
#define PIIX_SMB_HS_DEVERR	(1 << 2)	/* command error */
#define PIIX_SMB_HS_BUSERR	(1 << 3)	/* transaction collision */
#define PIIX_SMB_HS_FAILED	(1 << 4)	/* failed bus transaction */
#define PIIX_SMB_HS_BITS	"\020\001BUSY\002INTR\003DEVERR\004BUSERR\005FAILED"
#define PIIX_SMB_HC	0x02		/* host control */
#define PIIX_SMB_HC_INTREN	(1 << 0)	/* enable interrupts */
#define PIIX_SMB_HC_KILL	(1 << 1)	/* kill current transaction */
#define PIIX_SMB_HC_CMD_QUICK	(0 << 2)	/* QUICK command */
#define PIIX_SMB_HC_CMD_BYTE	(1 << 2)	/* BYTE command */
#define PIIX_SMB_HC_CMD_BDATA	(2 << 2)	/* BYTE DATA command */
#define PIIX_SMB_HC_CMD_WDATA	(3 << 2)	/* WORD DATA command */
#define PIIX_SMB_HC_CMD_BLOCK	(5 << 2)	/* BLOCK command */
#define PIIX_SMB_HC_START	(1 << 6)	/* start transaction */
#define PIIX_SMB_HCMD	0x03		/* host command */
#define PIIX_SMB_TXSLVA	0x04		/* transmit slave address */
#define PIIX_SMB_TXSLVA_READ	(1 << 0)	/* read direction */
#define PIIX_SMB_TXSLVA_ADDR(x)	(((x) & 0x7f) << 1) /* 7-bit address */
#define PIIX_SMB_HD0	0x05		/* host data 0 */
#define PIIX_SMB_HD1	0x06		/* host data 1 */
#define PIIX_SMB_HBDB	0x07		/* host block data byte */

struct piixpm_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	void *			sc_ih;

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

int	piixpm_match(struct device *, void *, void *);
void	piixpm_attach(struct device *, struct device *, void *);

int	piixpm_i2c_acquire_bus(void *, int);
void	piixpm_i2c_release_bus(void *, int);
int	piixpm_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
	    void *, size_t, int);

int	piixpm_intr(void *);

struct cfattach piixpm_ca = {
	sizeof(struct piixpm_softc),
	piixpm_match,
	piixpm_attach
};

struct cfdriver piixpm_cd = {
	NULL, "piixpm", DV_DULL
};

const struct pci_matchid piixpm_ids[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82371AB_PMC },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82440MX_PM },
	{ PCI_VENDOR_RCC, PCI_PRODUCT_RCC_OSB4 },
	{ PCI_VENDOR_RCC, PCI_PRODUCT_RCC_CSB5 },
	{ PCI_VENDOR_RCC, PCI_PRODUCT_RCC_CSB6 }
};

int
piixpm_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, piixpm_ids,
	    sizeof(piixpm_ids) / sizeof(piixpm_ids[0])));
}

void
piixpm_attach(struct device *parent, struct device *self, void *aux)
{
	struct piixpm_softc *sc = (struct piixpm_softc *)self;
	struct pci_attach_args *pa = aux;
	struct i2cbus_attach_args iba;
	pcireg_t base, conf;
	bus_size_t iosize;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;

	/* Map I/O space */
	base = pci_conf_read(pa->pa_pc, pa->pa_tag, PIIX_SMB_BASE);
	iosize = PCI_MAPREG_IO_SIZE(base);
	sc->sc_iot = pa->pa_iot;
	if (bus_space_map(sc->sc_iot, PCI_MAPREG_IO_ADDR(base),
	    iosize, 0, &sc->sc_ioh)) {
		printf(": can't map I/O space\n");
		return;
	}

	/* Read configuration */
	conf = pci_conf_read(pa->pa_pc, pa->pa_tag, PIIX_SMB_HOSTC);
	DPRINTF((": conf 0x%x", conf));

	/* Install interrupt handler if IRQ enabled */
	if ((conf & PIIX_SMB_HOSTC_IRQ) == PIIX_SMB_HOSTC_IRQ) {
		if (pci_intr_map(pa, &ih)) {
			printf(": can't map interrupt\n");
			goto fail;
		}
		intrstr = pci_intr_string(pa->pa_pc, ih);
		sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
		    piixpm_intr, sc, sc->sc_dev.dv_xname);
		if (sc->sc_ih == NULL) {
			printf(": can't establish interrupt");
			if (intrstr != NULL)
				printf(" at %s", intrstr);
			printf("\n");
			goto fail;
		}
		printf(": %s", intrstr);
	}

	/* Enable controller */
	pci_conf_write(pa->pa_pc, pa->pa_tag, PIIX_SMB_HOSTC,
	    conf | PIIX_SMB_HOSTC_HSTEN);

	printf("\n");

	/* Attach I2C bus */
	lockinit(&sc->sc_i2c_lock, PRIBIO | PCATCH, "iiclk", 0, 0);
	sc->sc_i2c_tag.ic_cookie = sc;
	sc->sc_i2c_tag.ic_acquire_bus = piixpm_i2c_acquire_bus;
	sc->sc_i2c_tag.ic_release_bus = piixpm_i2c_release_bus;
	sc->sc_i2c_tag.ic_exec = piixpm_i2c_exec;
	iba.iba_name = "iic";
	iba.iba_tag = &sc->sc_i2c_tag;
	iba.iba_scan = 1;
	config_found(self, &iba, iicbus_print);

	return;

fail:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
}

int
piixpm_i2c_acquire_bus(void *cookie, int flags)
{
	struct piixpm_softc *sc = cookie;

	if (flags & I2C_F_POLL)
		return (0);

	return (lockmgr(&sc->sc_i2c_lock, LK_EXCLUSIVE, NULL));
}

void
piixpm_i2c_release_bus(void *cookie, int flags)
{
	struct piixpm_softc *sc = cookie;

	if (flags & I2C_F_POLL)
		return;

	lockmgr(&sc->sc_i2c_lock, LK_RELEASE, NULL);
}

int
piixpm_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
	struct piixpm_softc *sc = cookie;
	u_int8_t *b;
	u_int8_t ctl, st;
	int retries;

	DPRINTF(("%s: exec op %d, addr 0x%x, cmdlen %d, len %d, "
	    "flags 0x%x, status 0x%b\n", sc->sc_dev.dv_xname, op, addr,
	    cmdlen, len, flags, bus_space_read_1(sc->sc_iot, sc->sc_ioh,
	    PIIX_SMB_HS), PIIX_SMB_HS_BITS));

	if (!I2C_OP_STOP_P(op) || cmdlen > 1 || len > 2)
		return (1);

	/* Setup transfer */
	sc->sc_i2c_xfer.op = op;
	sc->sc_i2c_xfer.buf = buf;
	sc->sc_i2c_xfer.len = len;
	sc->sc_i2c_xfer.flags = flags;
	sc->sc_i2c_xfer.error = 0;

	/* Set slave address and transfer direction */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, PIIX_SMB_TXSLVA,
	    PIIX_SMB_TXSLVA_ADDR(addr) |
	    (I2C_OP_READ_P(op) ? PIIX_SMB_TXSLVA_READ : 0));

	b = (void *)cmdbuf;
	if (cmdlen > 0)
		/* Set command byte */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, PIIX_SMB_HCMD, b[0]);

	if (I2C_OP_WRITE_P(op)) {
		/* Write data */
		b = buf;
		if (len > 0)
			bus_space_write_1(sc->sc_iot, sc->sc_ioh,
			    PIIX_SMB_HD0, b[0]);
		if (len > 1)
			bus_space_write_1(sc->sc_iot, sc->sc_ioh,
			    PIIX_SMB_HD1, b[1]);
	}

	/* Set SMBus command */
	if (len == 0)
		ctl = PIIX_SMB_HC_CMD_BYTE;
	else if (len == 1)
		ctl = PIIX_SMB_HC_CMD_BDATA;
	else if (len == 2)
		ctl = PIIX_SMB_HC_CMD_WDATA;

	if ((flags & I2C_F_POLL) == 0)
		ctl |= PIIX_SMB_HC_INTREN;

	/* Start transaction */
	ctl |= PIIX_SMB_HC_START;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, PIIX_SMB_HC, ctl);

	if (flags & I2C_F_POLL) {
		/* Poll for completion */
		DELAY(PIIXPM_DELAY);
		for (retries = 1000; retries > 0; retries--) {
			st = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    PIIX_SMB_HS);
			if ((st & PIIX_SMB_HS_BUSY) == 0)
				break;
			DELAY(PIIXPM_DELAY);
		}
		if (st & PIIX_SMB_HS_BUSY) {
			printf("%s: timeout, status 0x%b\n",
			    sc->sc_dev.dv_xname, st, PIIX_SMB_HS_BITS);
			return (1);
		}
		piixpm_intr(sc);
	} else {
		/* Wait for interrupt */
		if (tsleep(sc, PRIBIO, "iicexec", PIIXPM_TIMEOUT * hz))
			return (1);
	}	

	if (sc->sc_i2c_xfer.error)
		return (1);

	return (0);
}

int
piixpm_intr(void *arg)
{
	struct piixpm_softc *sc = arg;
	u_int8_t st;
	u_int8_t *b;
	size_t len;

	/* Read status */
	st = bus_space_read_1(sc->sc_iot, sc->sc_ioh, PIIX_SMB_HS);
	if ((st & PIIX_SMB_HS_BUSY) != 0 || (st & (PIIX_SMB_HS_INTR |
	    PIIX_SMB_HS_DEVERR | PIIX_SMB_HS_BUSERR |
	    PIIX_SMB_HS_FAILED)) == 0)
		/* Interrupt was not for us */
		return (0);

	DPRINTF(("%s: intr st 0x%b\n", sc->sc_dev.dv_xname, st,
	    PIIX_SMB_HS_BITS));

	/* Clear status bits */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, PIIX_SMB_HS, st);

	/* Check for errors */
	if (st & (PIIX_SMB_HS_DEVERR | PIIX_SMB_HS_BUSERR |
	    PIIX_SMB_HS_FAILED)) {
		sc->sc_i2c_xfer.error = 1;
		goto done;
	}

	if (st & PIIX_SMB_HS_INTR) {
		if (I2C_OP_WRITE_P(sc->sc_i2c_xfer.op))
			goto done;

		/* Read data */
		b = sc->sc_i2c_xfer.buf;
		len = sc->sc_i2c_xfer.len;
		if (len > 0)
			b[0] = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    PIIX_SMB_HD0);
		if (len > 1)
			b[1] = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    PIIX_SMB_HD1);
	}

done:
	if ((sc->sc_i2c_xfer.flags & I2C_F_POLL) == 0)
		wakeup(sc);
	return (1);
}
