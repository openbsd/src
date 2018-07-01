/* $OpenBSD: tpm.c,v 1.3 2018/07/01 19:40:49 mlarkin Exp $ */

/*
 * Minimal interface to Trusted Platform Module chips implementing the
 * TPM Interface Spec 1.2, just enough to tell the TPM to save state before
 * a system suspend.
 *
 * Copyright (c) 2008, 2009 Michael Shalayeff
 * Copyright (c) 2009, 2010 Hans-Joerg Hoexer
 * Copyright (c) 2016 joshua stein <jcs@openbsd.org>
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/apmvar.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

/* #define TPM_DEBUG */

#ifdef TPM_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define TPM_BUFSIZ			1024
#define TPM_HDRSIZE			10
#define TPM_PARAM_SIZE			0x0001

#define TPM_ACCESS			0x0000	/* access register */
#define TPM_ACCESS_ESTABLISHMENT	0x01	/* establishment */
#define TPM_ACCESS_REQUEST_USE		0x02	/* request using locality */
#define TPM_ACCESS_REQUEST_PENDING	0x04	/* pending request */
#define TPM_ACCESS_SEIZE		0x08	/* request locality seize */
#define TPM_ACCESS_SEIZED		0x10	/* locality has been seized */
#define TPM_ACCESS_ACTIVE_LOCALITY	0x20	/* locality is active */
#define TPM_ACCESS_VALID		0x80	/* bits are valid */
#define TPM_ACCESS_BITS	\
    "\020\01EST\02REQ\03PEND\04SEIZE\05SEIZED\06ACT\010VALID"

#define TPM_INTERRUPT_ENABLE		0x0008
#define TPM_GLOBAL_INT_ENABLE		0x80000000 /* enable ints */
#define TPM_CMD_READY_INT		0x00000080 /* cmd ready enable */
#define TPM_INT_EDGE_FALLING		0x00000018
#define TPM_INT_EDGE_RISING		0x00000010
#define TPM_INT_LEVEL_LOW		0x00000008
#define TPM_INT_LEVEL_HIGH		0x00000000
#define TPM_LOCALITY_CHANGE_INT		0x00000004 /* locality change enable */
#define TPM_STS_VALID_INT		0x00000002 /* int on TPM_STS_VALID is set */
#define TPM_DATA_AVAIL_INT		0x00000001 /* int on TPM_STS_DATA_AVAIL is set */
#define TPM_INTERRUPT_ENABLE_BITS \
    "\020\040ENA\010RDY\03LOCH\02STSV\01DRDY"

#define TPM_INT_VECTOR			0x000c	/* 8 bit reg for 4 bit irq vector */
#define TPM_INT_STATUS			0x0010	/* bits are & 0x87 from TPM_INTERRUPT_ENABLE */

#define TPM_INTF_CAPABILITIES		0x0014	/* capability register */
#define TPM_INTF_BURST_COUNT_STATIC	0x0100	/* TPM_STS_BMASK static */
#define TPM_INTF_CMD_READY_INT		0x0080	/* int on ready supported */
#define TPM_INTF_INT_EDGE_FALLING	0x0040	/* falling edge ints supported */
#define TPM_INTF_INT_EDGE_RISING	0x0020	/* rising edge ints supported */
#define TPM_INTF_INT_LEVEL_LOW		0x0010	/* level-low ints supported */
#define TPM_INTF_INT_LEVEL_HIGH		0x0008	/* level-high ints supported */
#define TPM_INTF_LOCALITY_CHANGE_INT	0x0004	/* locality-change int (mb 1) */
#define TPM_INTF_STS_VALID_INT		0x0002	/* TPM_STS_VALID int supported */
#define TPM_INTF_DATA_AVAIL_INT		0x0001	/* TPM_STS_DATA_AVAIL int supported (mb 1) */
#define TPM_CAPSREQ \
  (TPM_INTF_DATA_AVAIL_INT|TPM_INTF_LOCALITY_CHANGE_INT|TPM_INTF_INT_LEVEL_LOW)
#define TPM_CAPBITS \
  "\020\01IDRDY\02ISTSV\03ILOCH\04IHIGH\05ILOW\06IEDGE\07IFALL\010IRDY\011BCST"

#define TPM_STS				0x0018	   /* status register */
#define TPM_STS_MASK			0x000000ff /* status bits */
#define TPM_STS_BMASK			0x00ffff00 /* ro io burst size */
#define TPM_STS_VALID			0x00000080 /* ro other bits are valid */
#define TPM_STS_CMD_READY		0x00000040 /* rw chip/signal ready */
#define TPM_STS_GO			0x00000020 /* wo start the command */
#define TPM_STS_DATA_AVAIL		0x00000010 /* ro data available */
#define TPM_STS_DATA_EXPECT		0x00000008 /* ro more data to be written */
#define TPM_STS_RESP_RETRY		0x00000002 /* wo resend the response */
#define TPM_STS_BITS	"\020\010VALID\07RDY\06GO\05DRDY\04EXPECT\02RETRY"

#define TPM_DATA			0x0024
#define TPM_ID				0x0f00
#define TPM_REV				0x0f04
#define TPM_SIZE			0x5000	/* five pages of the above */

#define TPM_ACCESS_TMO			2000	/* 2sec */
#define TPM_READY_TMO			2000	/* 2sec */
#define TPM_READ_TMO			120000	/* 2 minutes */
#define TPM_BURST_TMO			2000	/* 2sec */

struct tpm_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_bt;
	bus_space_handle_t	sc_bh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	uint32_t		sc_devid;
	uint32_t		sc_rev;

	int			sc_enabled;
};

struct tpm_crs {
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

const struct {
	uint32_t devid;
	char name[32];
} tpm_devs[] = {
	{ 0x000615d1, "Infineon SLD9630 1.1" },
	{ 0x000b15d1, "Infineon SLB9635 1.2" },
	{ 0x100214e4, "Broadcom BCM0102" },
	{ 0x00fe1050, "WEC WPCT200" },
	{ 0x687119fa, "SNS SSX35" },
	{ 0x2e4d5453, "STM ST19WP18" },
	{ 0x32021114, "Atmel 97SC3203" },
	{ 0x10408086, "Intel INTC0102" },
	{ 0, "" },
};

int	tpm_match(struct device *, void *, void *);
void	tpm_attach(struct device *, struct device *, void *);
int	tpm_activate(struct device *, int);
int	tpm_parse_crs(int, union acpi_resource *, void *);

int	tpm_probe(bus_space_tag_t, bus_space_handle_t);
int	tpm_init(struct tpm_softc *);
int	tpm_read(struct tpm_softc *, void *, int, size_t *, int);
int	tpm_write(struct tpm_softc *, void *, int);
int	tpm_suspend(struct tpm_softc *);
int	tpm_resume(struct tpm_softc *);

int	tpm_waitfor(struct tpm_softc *, uint8_t, int);
int	tpm_request_locality(struct tpm_softc *, int);
void	tpm_release_locality(struct tpm_softc *);
int	tpm_getburst(struct tpm_softc *);
uint8_t	tpm_status(struct tpm_softc *);
int	tpm_tmotohz(int);

struct cfattach tpm_ca = {
	sizeof(struct tpm_softc),
	tpm_match,
	tpm_attach,
	NULL,
	tpm_activate
};

struct cfdriver tpm_cd = {
	NULL, "tpm", DV_DULL
};

const char *tpm_hids[] = {
	"PNP0C31",
	"ATM1200",
	"IFX0102",
	"BCM0101",
	"BCM0102",
	"NSC1200",
	"ICO0102",
	NULL
};

int
tpm_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata		*cf = match;

	return (acpi_matchhids(aa, tpm_hids, cf->cf_driver->cd_name));
}

void
tpm_attach(struct device *parent, struct device *self, void *aux)
{
	struct tpm_softc	*sc = (struct tpm_softc *)self;
	struct acpi_attach_args *aa = aux;
	struct tpm_crs	crs;
	struct aml_value	res;
	int64_t			st;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;
	sc->sc_enabled = 0;

	printf(": %s", sc->sc_devnode->name);

	if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "_STA", 0, NULL, &st))
		st = STA_PRESENT | STA_ENABLED | STA_DEV_OK;
	if ((st & (STA_PRESENT | STA_ENABLED | STA_DEV_OK)) !=
	    (STA_PRESENT | STA_ENABLED | STA_DEV_OK)) {
		printf(", not enabled\n");
		return;
	}

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
	aml_parse_resource(&res, tpm_parse_crs, &crs);
	aml_freevalue(&res);

	if (crs.addr_bas == 0) {
		printf(", can't find address\n");
		return;
	}

	printf(" addr 0x%x/0x%x", crs.addr_bas, crs.addr_len);

	sc->sc_bt = aa->aaa_memt;
	if (bus_space_map(sc->sc_bt, crs.addr_bas, crs.addr_len, 0,
	    &sc->sc_bh)) {
		printf(", failed mapping at 0x%x\n", crs.addr_bas);
		return;
	}

	if (!tpm_probe(sc->sc_bt, sc->sc_bh)) {
		printf(", probe failed\n");
		return;
	}

	if (tpm_init(sc) != 0) {
		printf(", init failed\n");
		return;
	}

	sc->sc_enabled = 1;
}

int
tpm_parse_crs(int crsidx, union acpi_resource *crs, void *arg)
{
	struct tpm_crs *sc_crs = arg;

	switch (AML_CRSTYPE(crs)) {
	case LR_MEM32:
		sc_crs->addr_min = letoh32(crs->lr_m32._min);
		sc_crs->addr_len = letoh32(crs->lr_m32._len);
		break;

	case LR_MEM32FIXED:
		sc_crs->addr_bas = letoh32(crs->lr_m32fixed._bas);
		sc_crs->addr_len = letoh32(crs->lr_m32fixed._len);
		break;

	case SR_IOPORT:
	case SR_IRQ:
	case LR_EXTIRQ:
	case LR_GPIO:
		break;

	default:
		DPRINTF(("%s: unknown resource type %d\n", __func__,
		    AML_CRSTYPE(crs)));
	}

	return 0;
}

int
tpm_activate(struct device *self, int act)
{
	struct tpm_softc	*sc = (struct tpm_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		if (!sc->sc_enabled) {
			DPRINTF(("%s: suspend, but not enabled\n",
			    sc->sc_dev.dv_xname));
			return 0;
		}
		tpm_suspend(sc);
		break;

	case DVACT_WAKEUP:
		if (!sc->sc_enabled) {
			DPRINTF(("%s: wakeup, but not enabled\n",
			    sc->sc_dev.dv_xname));
			return 0;
		}
		tpm_resume(sc);
		break;
	}

	return 0;
}

int
tpm_suspend(struct tpm_softc *sc)
{
	uint8_t command[] = {
	    0, 0xc1,		/* TPM_TAG_RQU_COMMAND */
	    0, 0, 0, 10,	/* Length in bytes */
	    0, 0, 0, 0x98	/* TPM_ORD_SaveStates */
	};

	DPRINTF(("%s: saving state preparing for suspend\n",
	    sc->sc_dev.dv_xname));

	/*
	 * Tell the chip to save its state so the BIOS can then restore it upon
	 * resume.
	 */
	tpm_write(sc, &command, sizeof(command));
	tpm_read(sc, &command, sizeof(command), NULL, TPM_HDRSIZE);

	return 0;
}

int
tpm_resume(struct tpm_softc *sc)
{
	/*
	 * TODO: The BIOS should have restored the chip's state for us already,
	 * but we should tell the chip to do a self-test here (according to the
	 * Linux driver).
	 */

	DPRINTF(("%s: resume\n", sc->sc_dev.dv_xname));
	return 0;
}

int
tpm_probe(bus_space_tag_t bt, bus_space_handle_t bh)
{
	uint32_t r;
	int tries = 10000;

	/* wait for chip to settle */
	while (tries--) {
		if (bus_space_read_1(bt, bh, TPM_ACCESS) & TPM_ACCESS_VALID)
			break;
		else if (!tries) {
			printf(": timed out waiting for validity\n");
			return 1;
		}

		DELAY(10);
	}

	r = bus_space_read_4(bt, bh, TPM_INTF_CAPABILITIES);
	if (r == 0xffffffff)
		return 0;

	return 1;
}

int
tpm_init(struct tpm_softc *sc)
{
	uint32_t r, intmask;
	int i;

	r = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTF_CAPABILITIES);
	if ((r & TPM_CAPSREQ) != TPM_CAPSREQ ||
	    !(r & (TPM_INTF_INT_EDGE_RISING | TPM_INTF_INT_LEVEL_LOW))) {
		DPRINTF((": caps too low (caps=%b)\n", r, TPM_CAPBITS));
		return 0;
	}

	/* ack and disable all interrupts, we'll be using polling only */
	intmask = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE);
	intmask |= TPM_INTF_CMD_READY_INT | TPM_INTF_LOCALITY_CHANGE_INT |
	    TPM_INTF_DATA_AVAIL_INT | TPM_INTF_STS_VALID_INT;
	intmask &= ~TPM_GLOBAL_INT_ENABLE;
	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE, intmask);

	if (tpm_request_locality(sc, 0)) {
		printf(", requesting locality failed\n");
		return 1;
	}

	sc->sc_devid = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_ID);
	sc->sc_rev = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_REV);

	for (i = 0; tpm_devs[i].devid; i++)
		if (tpm_devs[i].devid == sc->sc_devid)
			break;

	if (tpm_devs[i].devid)
		printf(": %s rev 0x%x\n", tpm_devs[i].name, sc->sc_rev);
	else
		printf(": device 0x%08x rev 0x%x\n", sc->sc_devid, sc->sc_rev);

	return 0;
}

int
tpm_request_locality(struct tpm_softc *sc, int l)
{
	uint32_t r;
	int to;

	if (l != 0)
		return EINVAL;

	if ((bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS) &
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) ==
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY))
		return 0;

	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS,
	    TPM_ACCESS_REQUEST_USE);

	to = tpm_tmotohz(TPM_ACCESS_TMO);

	while ((r = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS) &
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) !=
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY) && to--) {
		DELAY(10);
	}

	if ((r & (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) !=
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) {
		DPRINTF(("%s: %s: access %b\n", sc->sc_dev.dv_xname, __func__,
		    r, TPM_ACCESS_BITS));
		return EBUSY;
	}

	return 0;
}

void
tpm_release_locality(struct tpm_softc *sc)
{
	if ((bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS) &
	    (TPM_ACCESS_REQUEST_PENDING|TPM_ACCESS_VALID)) ==
	    (TPM_ACCESS_REQUEST_PENDING|TPM_ACCESS_VALID)) {
		DPRINTF(("%s: releasing locality\n", sc->sc_dev.dv_xname));
		bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS,
		    TPM_ACCESS_ACTIVE_LOCALITY);
	}
}

int
tpm_getburst(struct tpm_softc *sc)
{
	int burst, burst2, to;

	to = tpm_tmotohz(TPM_BURST_TMO);

	burst = 0;
	while (burst == 0 && to--) {
		/*
		 * Burst count has to be read from bits 8 to 23 without
		 * touching any other bits, eg. the actual status bits 0 to 7.
		 */
		burst = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_STS + 1);
		DPRINTF(("%s: %s: read1(0x%x): 0x%x\n", sc->sc_dev.dv_xname,
		    __func__, TPM_STS + 1, burst));
		burst2 = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_STS + 2);
		DPRINTF(("%s: %s: read1(0x%x): 0x%x\n", sc->sc_dev.dv_xname,
		    __func__, TPM_STS + 2, burst2));
		burst |= burst2 << 8;
		if (burst)
			return burst;

		DELAY(10);
	}

	DPRINTF(("%s: getburst timed out\n", sc->sc_dev.dv_xname));

	return 0;
}

uint8_t
tpm_status(struct tpm_softc *sc)
{
	return bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_STS) & TPM_STS_MASK;
}

int
tpm_tmotohz(int tmo)
{
	struct timeval tv;

	tv.tv_sec = tmo / 1000;
	tv.tv_usec = 1000 * (tmo % 1000);

	return tvtohz(&tv);
}

int
tpm_waitfor(struct tpm_softc *sc, uint8_t mask, int tries)
{
	uint8_t status;

	while (((status = tpm_status(sc)) & mask) != mask) {
		if (tries == 0) {
			DPRINTF(("%s: %s: timed out, status 0x%x != 0x%x\n",
			    sc->sc_dev.dv_xname, __func__, status, mask));
			return status;
		}

		tries--;
		DELAY(1);
	}

	return 0;
}

int
tpm_read(struct tpm_softc *sc, void *buf, int len, size_t *count,
    int flags)
{
	uint8_t *p = buf;
	uint8_t c;
	size_t cnt;
	int rv, n, bcnt;

	DPRINTF(("%s: %s %d:", sc->sc_dev.dv_xname, __func__, len));

	cnt = 0;
	while (len > 0) {
		if ((rv = tpm_waitfor(sc, TPM_STS_DATA_AVAIL | TPM_STS_VALID,
		    TPM_READ_TMO)))
			return rv;

		bcnt = tpm_getburst(sc);
		n = MIN(len, bcnt);

		for (; n--; len--) {
			c = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_DATA);
			DPRINTF((" %02x", c));
			*p++ = c;
			cnt++;
		}

		if ((flags & TPM_PARAM_SIZE) == 0 && cnt >= 6)
			break;
	}

	DPRINTF(("\n"));

	if (count)
		*count = cnt;

	return 0;
}

int
tpm_write(struct tpm_softc *sc, void *buf, int len)
{
	uint8_t *p = buf;
	uint8_t status;
	size_t count = 0;
	int rv, r;

	if ((rv = tpm_request_locality(sc, 0)) != 0)
		return rv;

	DPRINTF(("%s: %s %d:", sc->sc_dev.dv_xname, __func__, len));
	for (r = 0; r < len; r++)
		DPRINTF((" %02x", (uint8_t)(*(p + r))));
	DPRINTF(("\n"));

	/* read status */
	status = tpm_status(sc);
	if ((status & TPM_STS_CMD_READY) == 0) {
		/* abort! */
		bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS,
		    TPM_STS_CMD_READY);
		if ((rv = tpm_waitfor(sc, TPM_STS_CMD_READY, TPM_READ_TMO))) {
			DPRINTF(("%s: failed waiting for ready after abort "
			    "(0x%x)\n", sc->sc_dev.dv_xname, rv));
			return rv;
		}
	}

	while (count < len - 1) {
		for (r = tpm_getburst(sc); r > 0 && count < len - 1; r--) {
			DPRINTF(("%s: %s: write1(0x%x, 0x%x)\n",
			    sc->sc_dev.dv_xname, __func__, TPM_DATA, *p));
			bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_DATA, *p++);
			count++;
		}
		if ((rv = tpm_waitfor(sc, TPM_STS_VALID | TPM_STS_DATA_EXPECT,
		    TPM_READ_TMO))) {
			DPRINTF(("%s: %s: failed waiting for next byte (%d)\n",
			    sc->sc_dev.dv_xname, __func__, rv));
			return rv;
		}
	}

	DPRINTF(("%s: %s: write1(0x%x, 0x%x)\n", sc->sc_dev.dv_xname, __func__,
	    TPM_DATA, *p));
	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_DATA, *p);
	count++;

	if ((rv = tpm_waitfor(sc, TPM_STS_VALID, TPM_READ_TMO))) {
		DPRINTF(("%s: %s: failed after last byte (%d)\n",
		    sc->sc_dev.dv_xname, __func__, rv));
		return rv;
	}

	if ((status = tpm_status(sc)) & TPM_STS_DATA_EXPECT) {
		DPRINTF(("%s: %s: final status still expecting data: %b\n",
		    sc->sc_dev.dv_xname, __func__, status, TPM_STS_BITS));
		return status;
	}

	DPRINTF(("%s: final status after write: %b\n", sc->sc_dev.dv_xname,
	    status, TPM_STS_BITS));

	/* XXX: are we ever sending non-command data? */
	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS, TPM_STS_GO);

	return 0;
}
