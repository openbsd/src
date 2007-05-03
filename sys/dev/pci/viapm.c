/*	$OpenBSD: viapm.c,v 1.8 2007/05/03 09:36:26 dlg Exp $	*/

/*
 * Copyright (c) 2005 Mark Kettenis <kettenis@openbsd.org>
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
 * VIA VT8237 SMBus controller driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/rwlock.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/i2c/i2cvar.h>

/*
 * VIA VT8237 ISA register definitions.
 */

/* PCI configuration registers */
#define VIAPM_SMB_BASE	0xd0		/* SMBus base address */
#define VIAPM_SMB_HOSTC	0xd2		/* host configuration */
#define VIAPM_SMB_HOSTC_HSTEN	(1 << 0)	/* enable host controller */
#define VIAPM_SMB_HOSTC_INTEN	(1 << 1)	/* enable SCI/SMI */
#define VIAPM_SMB_HOSTC_SCIEN	(1 << 3)	/* interrupt type (SCI/SMI) */

/* SMBus I/O registers */
#define VIAPM_SMB_HS	0x00		/* host status */
#define VIAPM_SMB_HS_BUSY	(1 << 0)	/* running a command */
#define VIAPM_SMB_HS_INTR	(1 << 1)	/* command completed */
#define VIAPM_SMB_HS_DEVERR	(1 << 2)	/* command error */
#define VIAPM_SMB_HS_BUSERR	(1 << 3)	/* transaction collision */
#define VIAPM_SMB_HS_FAILED	(1 << 4)	/* failed bus transaction */
#define VIAPM_SMB_HS_INUSE	(1 << 6)	/* bus semaphore */
#define VIAPM_SMB_HS_BITS	\
  "\020\001BUSY\002INTR\003DEVERR\004BUSERR\005FAILED\007INUSE"
#define VIAPM_SMB_HC	0x02		/* host control */
#define VIAPM_SMB_HC_INTREN	(1 << 0)	/* enable interrupts */
#define VIAPM_SMB_HC_KILL	(1 << 1)	/* kill current transaction */
#define VIAPM_SMB_HC_CMD_QUICK	(0 << 2)	/* QUICK command */
#define VIAPM_SMB_HC_CMD_BYTE	(1 << 2)	/* BYTE command */
#define VIAPM_SMB_HC_CMD_BDATA	(2 << 2)	/* BYTE DATA command */
#define VIAPM_SMB_HC_CMD_WDATA	(3 << 2)	/* WORD DATA command */
#define VIAPM_SMB_HC_CMD_PCALL	(4 << 2)	/* PROCESS CALL command */
#define VIAPM_SMB_HC_CMD_BLOCK	(5 << 2)	/* BLOCK command */
#define VIAPM_SMB_HC_START	(1 << 6)	/* start transaction */
#define VIAPM_SMB_HCMD	0x03		/* host command */
#define VIAPM_SMB_TXSLVA	0x04		/* transmit slave address */
#define VIAPM_SMB_TXSLVA_READ	(1 << 0)	/* read direction */
#define VIAPM_SMB_TXSLVA_ADDR(x) (((x) & 0x7f) << 1) /* 7-bit address */
#define VIAPM_SMB_HD0	0x05		/* host data 0 */
#define VIAPM_SMB_HD1	0x06		/* host data 1 */
#define VIAPM_SMB_HBDB	0x07		/* host block data byte */

#define VIAPM_SMB_SIZE	16

#ifdef VIAPM_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define VIAPM_DELAY	100
#define VIAPM_TIMEOUT	1

struct viapm_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	void *			sc_ih;
	int			sc_poll;

	struct i2c_controller	sc_i2c_tag;
	struct rwlock		sc_i2c_lock;
	struct {
		i2c_op_t     op;
		void *       buf;
		size_t       len;
		int          flags;
		volatile int error;
	}			sc_i2c_xfer;
};

int	viapm_match(struct device *, void *, void *);
void	viapm_attach(struct device *, struct device *, void *);

int	viapm_i2c_acquire_bus(void *, int);
void	viapm_i2c_release_bus(void *, int);
int	viapm_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
	    void *, size_t, int);

int	viapm_intr(void *);

struct cfattach viapm_ca = {
	sizeof(struct viapm_softc),
	viapm_match,
	viapm_attach
};

struct cfdriver viapm_cd = {
	NULL, "viapm", DV_DULL
};

const struct pci_matchid viapm_ids[] = {
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8233_ISA },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8233A_ISA },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8235_ISA },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8237_ISA },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8237A_ISA },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8251_ISA },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_CX700_ISA }
};

int
viapm_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, viapm_ids,
	    sizeof(viapm_ids) / sizeof(viapm_ids[0])));
}

void
viapm_attach(struct device *parent, struct device *self, void *aux)
{
	struct viapm_softc *sc = (struct viapm_softc *)self;
	struct pci_attach_args *pa = aux;
	struct i2cbus_attach_args iba;
	pcireg_t conf, iobase;
#if 0
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
#endif

	/* Map I/O space */
	sc->sc_iot = pa->pa_iot;
	iobase = pci_conf_read(pa->pa_pc, pa->pa_tag, VIAPM_SMB_BASE);
	if (iobase == 0 ||
	    bus_space_map(sc->sc_iot, iobase & 0xfffe,
	    VIAPM_SMB_SIZE, 0, &sc->sc_ioh)) {
		printf(": can't map I/O space\n");
		return;
	}

	/* Read configuration */
	conf = (iobase >> 16);
	DPRINTF((": conf 0x%x", conf));

	if ((conf & VIAPM_SMB_HOSTC_HSTEN) == 0) {
		printf(": SMBus host disabled\n");
		goto fail;
	}

	if (conf & VIAPM_SMB_HOSTC_INTEN) {
		if (conf & VIAPM_SMB_HOSTC_SCIEN)
			printf(": SCI");
		else
			printf(": SMI");
		sc->sc_poll = 1;
	} else {
#if 0
		/* Install interrupt handler */
		if (pci_intr_map(pa, &ih)) {
			printf(": can't map interrupt\n");
			goto fail;
		}
		intrstr = pci_intr_string(pa->pa_pc, ih);
		sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
		    viapm_intr, sc, sc->sc_dev.dv_xname);
		if (sc->sc_ih == NULL) {
			printf(": can't establish interrupt");
			if (intrstr != NULL)
				printf(" at %s", intrstr);
			printf("\n");
			goto fail;
		}
		printf(": %s", intrstr);
#endif
		sc->sc_poll = 1;
	}

	printf("\n");

	/* Attach I2C bus */
	rw_init(&sc->sc_i2c_lock, "iiclk");
	sc->sc_i2c_tag.ic_cookie = sc;
	sc->sc_i2c_tag.ic_acquire_bus = viapm_i2c_acquire_bus;
	sc->sc_i2c_tag.ic_release_bus = viapm_i2c_release_bus;
	sc->sc_i2c_tag.ic_exec = viapm_i2c_exec;

	bzero(&iba, sizeof iba);
	iba.iba_name = "iic";
	iba.iba_tag = &sc->sc_i2c_tag;
	config_found(self, &iba, iicbus_print);

	return;

fail:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, VIAPM_SMB_SIZE);
}

int
viapm_i2c_acquire_bus(void *cookie, int flags)
{
	struct viapm_softc *sc = cookie;

	if (cold || sc->sc_poll || (flags & I2C_F_POLL))
		return (0);

	return (rw_enter(&sc->sc_i2c_lock, RW_WRITE | RW_INTR));
}

void
viapm_i2c_release_bus(void *cookie, int flags)
{
	struct viapm_softc *sc = cookie;

	if (cold || sc->sc_poll || (flags & I2C_F_POLL))
		return;

	rw_exit(&sc->sc_i2c_lock);
}

int
viapm_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
	struct viapm_softc *sc = cookie;
	u_int8_t *b;
	u_int8_t ctl, st;
	int retries;

	DPRINTF(("%s: exec op %d, addr 0x%x, cmdlen %d, len %d, "
	    "flags 0x%x, status 0x%b\n", sc->sc_dev.dv_xname, op, addr,
	    cmdlen, len, flags, bus_space_read_1(sc->sc_iot, sc->sc_ioh,
	    VIAPM_SMB_HS), VIAPM_SMB_HS_BITS));

	/* Check if there's a transfer already running */
	st = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAPM_SMB_HS);
	DPRINTF(("%s: exec: st 0x%b\n", sc->sc_dev.dv_xname, st,
	    VIAPM_SMB_HS_BITS));
	if (st & VIAPM_SMB_HS_BUSY)
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
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAPM_SMB_TXSLVA,
	    VIAPM_SMB_TXSLVA_ADDR(addr) |
	    (I2C_OP_READ_P(op) ? VIAPM_SMB_TXSLVA_READ : 0));

	b = (void *)cmdbuf;
	if (cmdlen > 0)
		/* Set command byte */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    VIAPM_SMB_HCMD, b[0]);

	if (I2C_OP_WRITE_P(op)) {
		/* Write data */
		b = buf;
		if (len > 0)
			bus_space_write_1(sc->sc_iot, sc->sc_ioh,
			    VIAPM_SMB_HD0, b[0]);
		if (len > 1)
			bus_space_write_1(sc->sc_iot, sc->sc_ioh,
			    VIAPM_SMB_HD1, b[1]);
	}

	/* Set SMBus command */
	if (len == 0)
		ctl = VIAPM_SMB_HC_CMD_BYTE;
	else if (len == 1)
		ctl = VIAPM_SMB_HC_CMD_BDATA;
	else if (len == 2)
		ctl = VIAPM_SMB_HC_CMD_WDATA;

	if ((flags & I2C_F_POLL) == 0)
		ctl |= VIAPM_SMB_HC_INTREN;

	/* Start transaction */
	ctl |= VIAPM_SMB_HC_START;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAPM_SMB_HC, ctl);

	if (flags & I2C_F_POLL) {
		/* Poll for completion */
		DELAY(VIAPM_DELAY);
		for (retries = 1000; retries > 0; retries--) {
			st = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    VIAPM_SMB_HS);
			if ((st & VIAPM_SMB_HS_BUSY) == 0)
				break;
			DELAY(VIAPM_DELAY);
		}
		if (st & VIAPM_SMB_HS_BUSY)
			goto timeout;
		viapm_intr(sc);
	} else {
		/* Wait for interrupt */
		if (tsleep(sc, PRIBIO, "iicexec", VIAPM_TIMEOUT * hz))
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
	    VIAPM_SMB_HS_BITS);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAPM_SMB_HC,
	    VIAPM_SMB_HC_KILL);
	DELAY(VIAPM_DELAY);
	st = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAPM_SMB_HS);
	if ((st & VIAPM_SMB_HS_FAILED) == 0)
		printf("%s: transaction abort failed, status 0x%b\n",
		    sc->sc_dev.dv_xname, st, VIAPM_SMB_HS_BITS);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAPM_SMB_HS, st);
	return (1);
}

int
viapm_intr(void *arg)
{
	struct viapm_softc *sc = arg;
	u_int8_t st;
	u_int8_t *b;
	size_t len;

	/* Read status */
	st = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAPM_SMB_HS);
	if ((st & VIAPM_SMB_HS_BUSY) != 0 || (st & (VIAPM_SMB_HS_INTR |
	    VIAPM_SMB_HS_DEVERR | VIAPM_SMB_HS_BUSERR |
	    VIAPM_SMB_HS_FAILED)) == 0)
		/* Interrupt was not for us */
		return (0);

	DPRINTF(("%s: intr st 0x%b\n", sc->sc_dev.dv_xname, st,
	    VIAPM_SMB_HS_BITS));

	/* Clear status bits */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, VIAPM_SMB_HS, st);

	/* Check for errors */
	if (st & (VIAPM_SMB_HS_DEVERR | VIAPM_SMB_HS_BUSERR |
	    VIAPM_SMB_HS_FAILED)) {
		sc->sc_i2c_xfer.error = 1;
		goto done;
	}

	if (st & VIAPM_SMB_HS_INTR) {
		if (I2C_OP_WRITE_P(sc->sc_i2c_xfer.op))
			goto done;

		/* Read data */
		b = sc->sc_i2c_xfer.buf;
		len = sc->sc_i2c_xfer.len;
		if (len > 0)
			b[0] = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    VIAPM_SMB_HD0);
		if (len > 1)
			b[1] = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    VIAPM_SMB_HD1);
	}

done:
	if ((sc->sc_i2c_xfer.flags & I2C_F_POLL) == 0)
		wakeup(sc);
	return (1);
}
