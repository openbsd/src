/* $OpenBSD: ihidev.c,v 1.4 2016/01/13 15:10:35 jcs Exp $ */
/*
 * HID-over-i2c driver
 *
 * http://download.microsoft.com/download/7/d/d/7dd44bb7-2a7a-4505-ac1c-7227d3d96d5b/hid-over-i2c-protocol-spec-v1-0.docx
 *
 * Copyright (c) 2015, 2016 joshua stein <jcs@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/stdint.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/ihidev.h>

#include <dev/hid/hid.h>

/* XXX */
#include <dev/acpi/acpivar.h>

/* #define IHIDEV_DEBUG */

#ifdef IHIDEV_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

/* 7.2 */
enum {
	I2C_HID_CMD_DESCR	= 0x0,
	I2C_HID_CMD_RESET	= 0x1,
	I2C_HID_CMD_GET_REPORT	= 0x2,
	I2C_HID_CMD_SET_REPORT	= 0x3,
	I2C_HID_CMD_GET_IDLE	= 0x4,
	I2C_HID_CMD_SET_IDLE	= 0x5,
	I2C_HID_CMD_GET_PROTO	= 0x6,
	I2C_HID_CMD_SET_PROTO	= 0x7,
	I2C_HID_CMD_SET_POWER	= 0x8,

	/* pseudo commands */
	I2C_HID_REPORT_DESCR	= 0x100,
};

static int I2C_HID_POWER_ON	= 0x0;
static int I2C_HID_POWER_OFF	= 0x1;

union i2c_hid_cmd {
	uint8_t data[0];
	struct cmd {
		uint16_t reg;
		uint8_t reportTypeId;
		uint8_t opcode;
	} __packed c;
};

int	ihidev_match(struct device *, void *, void *);
void	ihidev_attach(struct device *, struct device *, void *);
int	ihidev_detach(struct device *, int);

int	ihidev_hid_command(struct ihidev_softc *, int, void *);
int	ihidev_intr(void *);
int	ihidev_reset(struct ihidev_softc *);
int	ihidev_hid_desc_parse(struct ihidev_softc *);

int	ihidev_maxrepid(void *buf, int len);
int	ihidev_print(void *aux, const char *pnp);
int	ihidev_submatch(struct device *parent, void *cf, void *aux);

struct cfattach ihidev_ca = {
	sizeof(struct ihidev_softc),
	ihidev_match,
	ihidev_attach,
	ihidev_detach,
	NULL
};

struct cfdriver ihidev_cd = {
	NULL, "ihidev", DV_DULL
};

int
ihidev_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "ihidev") == 0)
		return (1);

	return (0);
}

void
ihidev_attach(struct device *parent, struct device *self, void *aux)
{
	struct ihidev_softc *sc = (struct ihidev_softc *)self;
	struct i2c_attach_args *ia = aux;
	struct ihidev_attach_arg iha;
	struct device *dev;
	int repid, repsz;
	int repsizes[256];

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;
	sc->sc_hid_desc_addr = ia->ia_size;

	printf(": int %d", ia->ia_int);

	if (ihidev_hid_command(sc, I2C_HID_CMD_DESCR, NULL) ||
	    ihidev_hid_desc_parse(sc)) {
		printf(", failed fetching initial HID descriptor\n");
		return;
	}

	printf(", vendor 0x%x product 0x%x, %s\n",
	    letoh16(sc->hid_desc.wVendorID), letoh16(sc->hid_desc.wProductID),
	    (char *)ia->ia_cookie);

	sc->sc_nrepid = ihidev_maxrepid(sc->sc_report, sc->sc_reportlen);
	if (sc->sc_nrepid < 0)
		return;

	printf("%s: %d report id%s\n", sc->sc_dev.dv_xname, sc->sc_nrepid,
	    sc->sc_nrepid > 1 ? "s" : "");

	sc->sc_nrepid++;
	sc->sc_subdevs = mallocarray(sc->sc_nrepid, sizeof(struct ihidev *),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_subdevs == NULL) {
		printf("%s: failed allocating memory\n", sc->sc_dev.dv_xname);
		return;
	}

	iha.iaa = ia;
	iha.parent = sc;
	iha.reportid = IHIDEV_CLAIM_ALLREPORTID;

	/* Look for a driver claiming all report IDs first. */
	dev = config_found_sm((struct device *)sc, &iha, NULL,
	    ihidev_submatch);
	if (dev != NULL) {
		for (repid = 0; repid < sc->sc_nrepid; repid++)
			sc->sc_subdevs[repid] = (struct ihidev *)dev;
		return;
	}

	sc->sc_isize = 0;
	for (repid = 0; repid < sc->sc_nrepid; repid++) {
		repsz = hid_report_size(sc->sc_report, sc->sc_reportlen,
		    hid_input, repid);
		repsizes[repid] = repsz;
		if (repsz > sc->sc_isize)
			sc->sc_isize = repsz;

		DPRINTF(("%s: repid %d size %d\n", sc->sc_dev.dv_xname, repid,
		    repsz));

		if (hid_report_size(sc->sc_report, sc->sc_reportlen, hid_input,
		    repid) == 0 &&
		    hid_report_size(sc->sc_report, sc->sc_reportlen,
		    hid_output, repid) == 0 &&
		    hid_report_size(sc->sc_report, sc->sc_reportlen,
		    hid_feature, repid) == 0)
			continue;

		iha.reportid = repid;
		dev = config_found_sm(self, &iha, ihidev_print,
		    ihidev_submatch);
		sc->sc_subdevs[repid] = (struct ihidev *)dev;
	}
	sc->sc_isize += (sc->sc_nrepid != 1); /* one byte for the report ID */

	sc->sc_ibuf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);

	/* register interrupt with system */
	if (ia->ia_int > 0) {
		/* XXX: don't assume this uses acpi_intr_establish */
		sc->sc_ih = acpi_intr_establish(ia->ia_int, ia->ia_int_flags,
		    IPL_BIO, ihidev_intr, sc, sc->sc_dev.dv_xname);
		if (sc->sc_ih == NULL) {
			printf(", failed establishing intr\n");
			return;
		}
	}

	/* power down until we're opened */
	if (ihidev_hid_command(sc, I2C_HID_CMD_SET_POWER, &I2C_HID_POWER_OFF)) {
		printf("%s: failed to power down\n", sc->sc_dev.dv_xname);
		return;
	}
}

int
ihidev_detach(struct device *self, int flags)
{
	struct ihidev_softc *sc = (struct ihidev_softc *)self;

	if (sc->sc_ih != NULL) {
		intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (sc->sc_ibuf != NULL) {
		free(sc->sc_ibuf, M_DEVBUF, 0);
		sc->sc_ibuf = NULL;
	}

	if (sc->sc_report != NULL)
		free(sc->sc_report, M_DEVBUF, sc->sc_reportlen);

	return (0);
}

int
ihidev_hid_command(struct ihidev_softc *sc, int hidcmd, void *arg)
{
	int i, res = 1;

	iic_acquire_bus(sc->sc_tag, 0);

	switch (hidcmd) {
	case I2C_HID_CMD_DESCR: {
		/*
		 * 5.2.2 - HID Descriptor Retrieval
		 * register is passed from the controller, and is probably just
		 * the address of the device
		 */
		uint8_t cmdbuf[] = { htole16(sc->sc_hid_desc_addr), 0x0 };

		DPRINTF(("%s: HID command I2C_HID_CMD_DESCR at 0x%x\n",
		    sc->sc_dev.dv_xname, htole16(sc->sc_hid_desc_addr)));

		/* 20 00 */
		res = iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_addr, &cmdbuf,
		    sizeof(cmdbuf), &sc->hid_desc_buf,
		    sizeof(struct i2c_hid_desc), 0);

		DPRINTF(("%s: HID descriptor:", sc->sc_dev.dv_xname));
		for (i = 0; i < sizeof(struct i2c_hid_desc); i++)
			DPRINTF((" %.2x", sc->hid_desc_buf[i]));
		DPRINTF(("\n"));

		break;
	}
	case I2C_HID_CMD_RESET: {
		uint8_t cmdbuf[4] = { 0 };
		union i2c_hid_cmd *cmd = (union i2c_hid_cmd *)cmdbuf;

		DPRINTF(("%s: HID command I2C_HID_CMD_RESET\n",
		    sc->sc_dev.dv_xname));

		cmd->data[0] = sc->hid_desc_buf[offsetof(struct i2c_hid_desc,
			wCommandRegister)];
		cmd->data[1] = sc->hid_desc_buf[offsetof(struct i2c_hid_desc,
			wCommandRegister) + 1];
		cmd->c.opcode = I2C_HID_CMD_RESET;

		/* 22 00 00 01 */
		res = iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_addr, &cmdbuf,
		    sizeof(cmdbuf), NULL, 0, 0);

		break;
	}
	case I2C_HID_CMD_SET_POWER: {
		uint8_t cmdbuf[4] = { 0 };
		union i2c_hid_cmd *cmd = (union i2c_hid_cmd *)cmdbuf;
		int power = *(int *)arg;

		DPRINTF(("%s: HID command I2C_HID_CMD_SET_POWER(%d)\n",
		    sc->sc_dev.dv_xname, power));

		cmd->data[0] = sc->hid_desc_buf[offsetof(struct i2c_hid_desc,
			wCommandRegister)];
		cmd->data[1] = sc->hid_desc_buf[offsetof(struct i2c_hid_desc,
			wCommandRegister) + 1];
		cmd->c.opcode = I2C_HID_CMD_SET_POWER;
		cmd->c.reportTypeId = power;

		/* 22 00 00 08 */
		res = iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_addr, &cmdbuf,
		    sizeof(cmdbuf), NULL, 0, 0);

		break;
	}
	case I2C_HID_REPORT_DESCR: {
		uint8_t cmdbuf[] = {
		    sc->hid_desc_buf[offsetof(struct i2c_hid_desc,
		    wReportDescRegister)], 0 };

		DPRINTF(("%s: HID command I2C_HID_REPORT_DESCR at 0x%x with "
		    "size %d\n", sc->sc_dev.dv_xname, cmdbuf[0],
		    sc->sc_reportlen));

		/* 20 00 */
		res = iic_exec(sc->sc_tag, I2C_OP_WRITE, sc->sc_addr, &cmdbuf,
		    sizeof(cmdbuf), sc->sc_report, sc->sc_reportlen, 0);

		DPRINTF(("%s: HID report descriptor:", sc->sc_dev.dv_xname));
		for (i = 0; i < sc->sc_reportlen; i++)
			DPRINTF((" %.2x", sc->sc_report[i]));
		DPRINTF(("\n"));

		break;
	}
	default:
		printf("%s: unknown command %d\n", sc->sc_dev.dv_xname,
		    hidcmd);
	}

	iic_release_bus(sc->sc_tag, 0);

	return (res);
}

int
ihidev_reset(struct ihidev_softc *sc)
{
	DPRINTF(("%s: resetting\n", sc->sc_dev.dv_xname));

	if (ihidev_hid_command(sc, I2C_HID_CMD_SET_POWER, &I2C_HID_POWER_ON)) {
		printf("%s: failed to power on\n", sc->sc_dev.dv_xname);
		return (1);
	}

	DELAY(1000);

	if (ihidev_hid_command(sc, I2C_HID_CMD_RESET, 0)) {
		printf("%s: failed to reset hardware\n", sc->sc_dev.dv_xname);

		ihidev_hid_command(sc, I2C_HID_CMD_SET_POWER,
		    &I2C_HID_POWER_OFF);

		return (1);
	}

	DELAY(1000);

	return (0);
}

/*
 * 5.2.2 - HID Descriptor Retrieval
 *
 * parse HID Descriptor that has already been read into hid_desc with
 * I2C_HID_CMD_DESCR
 */
int
ihidev_hid_desc_parse(struct ihidev_softc *sc)
{
	int retries = 3;

	/* must be v01.00 */
	if (letoh16(sc->hid_desc.bcdVersion) != 0x0100) {
		printf("%s: bad HID descriptor bcdVersion (0x%x)\n",
		    sc->sc_dev.dv_xname,
		    letoh16(sc->hid_desc.bcdVersion));
		return (1);
	}

	/* must be 30 bytes for v1.00 */
	if (letoh16(sc->hid_desc.wHIDDescLength !=
	    sizeof(struct i2c_hid_desc))) {
		printf("%s: bad HID descriptor size (%d != %zu)\n",
		    sc->sc_dev.dv_xname,
		    letoh16(sc->hid_desc.wHIDDescLength),
		    sizeof(struct i2c_hid_desc));
		return (1);
	}

	if (letoh16(sc->hid_desc.wReportDescLength) <= 0) {
		printf("%s: bad HID report descriptor size (%d)\n",
		    sc->sc_dev.dv_xname,
		    letoh16(sc->hid_desc.wReportDescLength));
		return (1);
	}

	while (retries-- > 0) {
		if (ihidev_reset(sc)) {
			if (retries == 0)
				return(1);

			DELAY(1000);
		}
		else
			break;
	}

	sc->sc_reportlen = letoh16(sc->hid_desc.wReportDescLength);
	sc->sc_report = malloc(sc->sc_reportlen, M_DEVBUF, M_NOWAIT | M_ZERO);

	if (ihidev_hid_command(sc, I2C_HID_REPORT_DESCR, 0)) {
		printf("%s: failed fetching HID report\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}

	return (0);
}

int
ihidev_intr(void *arg)
{
	struct ihidev_softc *sc = arg;
	struct ihidev *scd;
	size_t size, psize;
	int res, i;
	u_char *p;
	u_int rep = 0;

	size = letoh16(sc->hid_desc.wMaxInputLength);
	if (size > sc->sc_isize);
		size = sc->sc_isize;

	iic_acquire_bus(sc->sc_tag, 0);

	/* XXX: force I2C_F_POLL for now to avoid dwiic interrupting while we
	 * are interrupting */
	res = iic_exec(sc->sc_tag, I2C_OP_READ, sc->sc_addr, NULL, 0,
	    sc->sc_ibuf, size, I2C_F_POLL);

	iic_release_bus(sc->sc_tag, 0);

	DPRINTF(("%s: ihidev_intr: hid input:", sc->sc_dev.dv_xname));
	for (i = 0; i < size; i++)
		DPRINTF((" %.2x", sc->sc_ibuf[i]));
	DPRINTF(("\n"));

	psize = sc->sc_ibuf[0] | sc->sc_ibuf[1] << 8;
	if (!psize) {
		DPRINTF(("%s: %s: invalid packet size\n", sc->sc_dev.dv_xname,
		    __func__));
		return (1);
	}

	if (psize > size) {
		DPRINTF(("%s: %s: truncated packet (%zu > %zu)\n",
		    sc->sc_dev.dv_xname, __func__, psize, size));
		return (1);
	}

	/* report id is 3rd byte */
	p = sc->sc_ibuf + 2;
	psize -= 2;
	if (sc->sc_nrepid != 1)
		rep = *p++; psize--;

	if (rep >= sc->sc_nrepid) {
		printf("%s: %s: bad repid %d\n", sc->sc_dev.dv_xname, __func__,
		    rep);
		return (1);
	}

	scd = sc->sc_subdevs[rep];
	if (scd == NULL || !(scd->sc_state & IHIDEV_OPEN))
		return (1);

	scd->sc_intr(scd, p, psize);

	return (1);
}

int
ihidev_maxrepid(void *buf, int len)
{
	struct hid_data *d;
	struct hid_item h;
	int maxid;

	maxid = -1;
	h.report_ID = 0;
	for (d = hid_start_parse(buf, len, hid_none); hid_get_item(d, &h); )
		if (h.report_ID > maxid)
			maxid = h.report_ID;
	hid_end_parse(d);

	return (maxid);
}

int
ihidev_print(void *aux, const char *pnp)
{
	struct ihidev_attach_arg *iha = aux;

	if (pnp)
		printf("hid at %s", pnp);

	if (iha->reportid != 0 && iha->reportid != IHIDEV_CLAIM_ALLREPORTID)
		printf(" reportid %d", iha->reportid);

	return (UNCONF);
}

int
ihidev_submatch(struct device *parent, void *match, void *aux)
{
	struct ihidev_attach_arg *iha = aux;
        struct cfdata *cf = match;

	if (cf->ihidevcf_reportid != IHIDEV_UNK_REPORTID &&
	    cf->ihidevcf_reportid != iha->reportid)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

int
ihidev_open(struct ihidev *scd)
{
	struct ihidev_softc *sc = scd->sc_parent;

	DPRINTF(("%s: %s: state=%d refcnt=%d\n", sc->sc_dev.dv_xname,
	    __func__, scd->sc_state, sc->sc_refcnt));

	if (scd->sc_state & IHIDEV_OPEN)
		return (EBUSY);

	scd->sc_state |= IHIDEV_OPEN;

	if (sc->sc_refcnt++ || sc->sc_isize == 0)
		return (0);

	/* power on */
	ihidev_reset(sc);

	return (0);
}

void
ihidev_close(struct ihidev *scd)
{
	struct ihidev_softc *sc = scd->sc_parent;

	DPRINTF(("%s: %s: state=%d refcnt=%d\n", sc->sc_dev.dv_xname,
	    __func__, scd->sc_state, sc->sc_refcnt));

	if (!(scd->sc_state & IHIDEV_OPEN))
		return;

	scd->sc_state &= ~IHIDEV_OPEN;

	if (--sc->sc_refcnt)
		return;

	if (ihidev_hid_command(sc, I2C_HID_CMD_SET_POWER, &I2C_HID_POWER_OFF))
		printf("%s: failed to power down\n", sc->sc_dev.dv_xname);
}

int
ihidev_ioctl(struct ihidev *sc, u_long cmd, caddr_t addr, int flag,
    struct proc *p)
{
	return -1;
}

void
ihidev_get_report_desc(struct ihidev_softc *sc, void **desc, int *size)
{
	*desc = sc->sc_report;
	*size = sc->sc_reportlen;
}
