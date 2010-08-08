/* $OpenBSD: acpiec.c,v 1.43 2010/08/08 17:25:41 kettenis Exp $ */
/*
 * Copyright (c) 2006 Can Erkin Acar <canacar@openbsd.org>
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
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <sys/sensors.h>

int		acpiec_match(struct device *, void *, void *);
void		acpiec_attach(struct device *, struct device *, void *);

u_int8_t	acpiec_status(struct acpiec_softc *);
u_int8_t	acpiec_read_data(struct acpiec_softc *);
void		acpiec_write_cmd(struct acpiec_softc *, u_int8_t);
void		acpiec_write_data(struct acpiec_softc *, u_int8_t);
void		acpiec_burst_enable(struct acpiec_softc *sc);
void		acpiec_burst_disable(struct acpiec_softc *sc);

u_int8_t	acpiec_read_1(struct acpiec_softc *, u_int8_t);
void		acpiec_write_1(struct acpiec_softc *, u_int8_t, u_int8_t);

void		acpiec_read(struct acpiec_softc *, u_int8_t, int, u_int8_t *);
void		acpiec_write(struct acpiec_softc *, u_int8_t, int, u_int8_t *);

int		acpiec_getcrs(struct acpiec_softc *,
		    struct acpi_attach_args *);
int		acpiec_getregister(const u_int8_t *, int, int *, bus_size_t *);

void		acpiec_wait(struct acpiec_softc *, u_int8_t, u_int8_t);
void		acpiec_sci_event(struct acpiec_softc *);

void		acpiec_get_events(struct acpiec_softc *);

int		acpiec_gpehandler(struct acpi_softc *, int, void *);

/* EC Status bits */
#define		EC_STAT_SMI_EVT	0x40	/* SMI event pending */
#define		EC_STAT_SCI_EVT	0x20	/* SCI event pending */
#define		EC_STAT_BURST	0x10	/* Controller in burst mode */
#define		EC_STAT_CMD	0x08	/* data is command */
#define		EC_STAT_IBF	0x02	/* input buffer full */
#define		EC_STAT_OBF	0x01	/* output buffer full */

/* EC Commands */
#define		EC_CMD_RD	0x80	/* Read */
#define		EC_CMD_WR	0x81	/* Write */
#define		EC_CMD_BE	0x82	/* Burst Enable */
#define		EC_CMD_BD	0x83	/* Burst Disable */
#define		EC_CMD_QR	0x84	/* Query */

#define		REG_TYPE_EC	3

int	acpiec_reg(struct acpiec_softc *);

struct cfattach acpiec_ca = {
	sizeof(struct acpiec_softc), acpiec_match, acpiec_attach
};

struct cfdriver acpiec_cd = {
	NULL, "acpiec", DV_DULL
};

const char *acpiec_hids[] = { ACPI_DEV_ECD, 0 };

void
acpiec_wait(struct acpiec_softc *sc, u_int8_t mask, u_int8_t val)
{
	static int acpiecnowait;
	u_int8_t		stat;

	dnprintf(40, "%s: EC wait_ns for: %b == %02x\n",
	    DEVNAME(sc), (int)mask,
	    "\20\x8IGN\x7SMI\x6SCI\05BURST\04CMD\03IGN\02IBF\01OBF", (int)val);

	while (((stat = acpiec_status(sc)) & mask) != val) {
		if (stat & EC_STAT_SCI_EVT)
			sc->sc_gotsci = 1;
		if (cold || (stat & EC_STAT_BURST))
			delay(1);
		else
			tsleep(&acpiecnowait, PWAIT, "acpiec", 1);
	}

	dnprintf(40, "%s: EC wait_ns, stat: %b\n", DEVNAME(sc), (int)stat,
	    "\20\x8IGN\x7SMI\x6SCI\05BURST\04CMD\03IGN\02IBF\01OBF");
}

u_int8_t
acpiec_status(struct acpiec_softc *sc)
{
	return (bus_space_read_1(sc->sc_cmd_bt, sc->sc_cmd_bh, 0));
}

void
acpiec_write_data(struct acpiec_softc *sc, u_int8_t val)
{
	acpiec_wait(sc, EC_STAT_IBF, 0);
	dnprintf(40, "acpiec: write_data -- %d\n", (int)val);
	bus_space_write_1(sc->sc_data_bt, sc->sc_data_bh, 0, val);
}

void
acpiec_write_cmd(struct acpiec_softc *sc, u_int8_t val)
{
	acpiec_wait(sc, EC_STAT_IBF, 0);
	dnprintf(40, "acpiec: write_cmd -- %d\n", (int)val);
	bus_space_write_1(sc->sc_cmd_bt, sc->sc_cmd_bh, 0, val);
}

u_int8_t
acpiec_read_data(struct acpiec_softc *sc)
{
	u_int8_t		val;

	acpiec_wait(sc, EC_STAT_OBF, EC_STAT_OBF);
	dnprintf(40, "acpiec: read_data\n", (int)val);
	val = bus_space_read_1(sc->sc_data_bt, sc->sc_data_bh, 0);

	return (val);
}

void
acpiec_sci_event(struct acpiec_softc *sc)
{
	u_int8_t		evt;

	sc->sc_gotsci = 0;

	acpiec_wait(sc, EC_STAT_IBF, 0);
	bus_space_write_1(sc->sc_cmd_bt, sc->sc_cmd_bh, 0, EC_CMD_QR);

	acpiec_wait(sc, EC_STAT_OBF, EC_STAT_OBF);
	evt = bus_space_read_1(sc->sc_data_bt, sc->sc_data_bh, 0);

	if (evt) {
		dnprintf(10, "%s: sci_event: 0x%02x\n", DEVNAME(sc), (int)evt);
		aml_evalnode(sc->sc_acpi, sc->sc_events[evt].event, 0, NULL,
		    NULL);
	}
}

u_int8_t
acpiec_read_1(struct acpiec_softc *sc, u_int8_t addr)
{
	u_int8_t		val;

	if ((acpiec_status(sc) & EC_STAT_SCI_EVT) == EC_STAT_SCI_EVT)
		sc->sc_gotsci = 1;

	acpiec_write_cmd(sc, EC_CMD_RD);
	acpiec_write_data(sc, addr);

	val = acpiec_read_data(sc);

	return (val);
}

void
acpiec_write_1(struct acpiec_softc *sc, u_int8_t addr, u_int8_t data)
{
	if ((acpiec_status(sc) & EC_STAT_SCI_EVT) == EC_STAT_SCI_EVT)
		sc->sc_gotsci = 1;

	acpiec_write_cmd(sc, EC_CMD_WR);
	acpiec_write_data(sc, addr);
	acpiec_write_data(sc, data);
}

void
acpiec_burst_enable(struct acpiec_softc *sc)
{
	acpiec_write_cmd(sc, EC_CMD_BE);
	acpiec_read_data(sc);
}

void
acpiec_burst_disable(struct acpiec_softc *sc)
{
	if ((acpiec_status(sc) & EC_STAT_BURST) == EC_STAT_BURST)
		acpiec_write_cmd(sc, EC_CMD_BD);
}

void
acpiec_read(struct acpiec_softc *sc, u_int8_t addr, int len, u_int8_t *buffer)
{
	int			reg;

	/*
	 * this works because everything runs in the acpi thread context.
	 * at some point add a lock to deal with concurrency so that a
	 * transaction does not get interrupted.
	 */
	dnprintf(20, "%s: read %d, %d\n", DEVNAME(sc), (int)addr, len);
	sc->sc_ecbusy = 1;
	acpiec_burst_enable(sc);
	for (reg = 0; reg < len; reg++)
		buffer[reg] = acpiec_read_1(sc, addr + reg);
	acpiec_burst_disable(sc);
	sc->sc_ecbusy = 0;
}

void
acpiec_write(struct acpiec_softc *sc, u_int8_t addr, int len, u_int8_t *buffer)
{
	int			reg;

	/*
	 * this works because everything runs in the acpi thread context.
	 * at some point add a lock to deal with concurrency so that a
	 * transaction does not get interrupted.
	 */
	dnprintf(20, "%s: write %d, %d\n", DEVNAME(sc), (int)addr, len);
	sc->sc_ecbusy = 1;
	acpiec_burst_enable(sc);
	for (reg = 0; reg < len; reg++)
		acpiec_write_1(sc, addr + reg, buffer[reg]);
	acpiec_burst_disable(sc);
	sc->sc_ecbusy = 0;
}

int
acpiec_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata		*cf = match;
	struct acpi_ecdt	*ecdt = aa->aaa_table;
	struct acpi_softc	*acpisc = (struct acpi_softc *)parent;

	/* Check for early ECDT table attach */
	if (ecdt && !memcmp(ecdt->hdr.signature, ECDT_SIG, sizeof(ECDT_SIG) - 1))
		return (1);
	if (acpisc->sc_ec)
		return (0);

	/* sanity */
	return (acpi_matchhids(aa, acpiec_hids, cf->cf_driver->cd_name));
}

void
acpiec_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpiec_softc	*sc = (struct acpiec_softc *)self;
	struct acpi_attach_args *aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;

	if (acpiec_getcrs(sc, aa)) {
		printf(": Failed to read resource settings\n");
		return;
	}

	sc->sc_acpi->sc_ec = sc;

	if (acpiec_reg(sc)) {
		printf(": Failed to register address space\n");
		return;
	}

	acpiec_get_events(sc);

	dnprintf(10, "%s: GPE: %d\n", DEVNAME(sc), sc->sc_gpe);

#ifndef SMALL_KERNEL
	acpi_set_gpehandler(sc->sc_acpi, sc->sc_gpe, acpiec_gpehandler,
	    sc, 1);
#endif

	printf("\n");
}

void
acpiec_get_events(struct acpiec_softc *sc)
{
	int			idx;
	char			name[16];

	memset(sc->sc_events, 0, sizeof(sc->sc_events));
	for (idx = 0; idx < ACPIEC_MAX_EVENTS; idx++) {
		snprintf(name, sizeof(name), "_Q%02X", idx);
		sc->sc_events[idx].event = aml_searchname(sc->sc_devnode, name);
		if (sc->sc_events[idx].event != NULL)
			dnprintf(10, "%s: Found event %s\n", DEVNAME(sc), name);
	}
}

int
acpiec_gpehandler(struct acpi_softc *acpi_sc, int gpe, void *arg)
{
	struct acpiec_softc	*sc = arg;
	u_int8_t		mask, stat, en;
	int			s;

	KASSERT(sc->sc_ecbusy == 0);
	dnprintf(10, "ACPIEC: got gpe\n");

	do {
		if (sc->sc_gotsci)
			acpiec_sci_event(sc);

		stat = acpiec_status(sc);
		dnprintf(40, "%s: EC interrupt, stat: %b\n",
		    DEVNAME(sc), (int)stat,
		    "\20\x8IGN\x7SMI\x6SCI\05BURST\04CMD\03IGN\02IBF\01OBF");

		if (stat & EC_STAT_SCI_EVT)
			sc->sc_gotsci = 1;
	} while (sc->sc_gotsci);

	/* Unmask the GPE which was blocked at interrupt time */
	s = spltty();
	mask = (1L << (gpe & 7));
	en = acpi_read_pmreg(acpi_sc, ACPIREG_GPE_EN, gpe>>3);
	acpi_write_pmreg(acpi_sc, ACPIREG_GPE_EN, gpe>>3, en | mask);
	splx(s);

	return (0);
}

/* parse the resource buffer to get a 'register' value */
int
acpiec_getregister(const u_int8_t *buf, int size, int *type, bus_size_t *addr)
{
	int			len, hlen;

#define RES_TYPE_MASK 0x80
#define RES_LENGTH_MASK 0x07
#define RES_TYPE_IOPORT	0x47
#define RES_TYPE_ENDTAG	0x79

	if (size <= 0)
		return (0);

	if (*buf & RES_TYPE_MASK) {
		/* large resource */
		if (size < 3)
			return (1);
		len = (int)buf[1] + 256 * (int)buf[2];
		hlen = 3;
	} else {
		/* small resource */
		len = buf[0] & RES_LENGTH_MASK;
		hlen = 1;
	}

	/* XXX todo: decode other types */
	if (*buf != RES_TYPE_IOPORT)
		return (0);

	if (size < hlen + len)
		return (0);

	/* XXX validate? */
	*type = GAS_SYSTEM_IOSPACE;
	*addr = (int)buf[2] + 256 * (int)buf[3];

	return (hlen + len);
}

int
acpiec_getcrs(struct acpiec_softc *sc, struct acpi_attach_args *aa)
{
	struct aml_value	res;
	bus_size_t		ec_sc, ec_data;
	int			dtype, ctype;
	char			*buf;
	int			size, ret;
	int64_t			gpe;
	struct acpi_ecdt	*ecdt = aa->aaa_table;
	extern struct aml_node	aml_root;

	/* Check if this is ECDT initialization */
	if (ecdt) {
		/* Get GPE, Data and Control segments */
		sc->sc_gpe = ecdt->gpe_bit;

		ctype = ecdt->ec_control.address_space_id;
		ec_sc = ecdt->ec_control.address;

		dtype = ecdt->ec_data.address_space_id;
		ec_data = ecdt->ec_data.address;

		/* Get devnode from header */
		sc->sc_devnode = aml_searchname(&aml_root, ecdt->ec_id);

		goto ecdtdone;
	}

	if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode, "_GPE", 0, NULL, &gpe)) {
		dnprintf(10, "%s: no _GPE\n", DEVNAME(sc));
		return (1);
	}

	sc->sc_gpe = gpe;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_CRS", 0, NULL, &res)) {
		dnprintf(10, "%s: no _CRS\n", DEVNAME(sc));
		return (1);
	}

	/* Parse CRS to get control and data registers */

	if (res.type != AML_OBJTYPE_BUFFER) {
		dnprintf(10, "%s: unknown _CRS type %d\n",
		    DEVNAME(sc), res.type);
		aml_freevalue(&res);
		return (1);
	}

	size = res.length;
	buf = res.v_buffer;

	ret = acpiec_getregister(buf, size, &dtype, &ec_data);
	if (ret <= 0) {
		dnprintf(10, "%s: failed to read DATA from _CRS\n",
		    DEVNAME(sc));
		aml_freevalue(&res);
		return (1);
	}

	buf += ret;
	size -= ret;

	ret = acpiec_getregister(buf, size, &ctype, &ec_sc);
	if (ret <= 0) {
		dnprintf(10, "%s: failed to read S/C from _CRS\n",
		    DEVNAME(sc));
		aml_freevalue(&res);
		return (1);
	}

	buf += ret;
	size -= ret;

	if (size != 2 || *buf != RES_TYPE_ENDTAG) {
		dnprintf(10, "%s: no _CRS end tag\n", DEVNAME(sc));
		aml_freevalue(&res);
		return (1);
	}
	aml_freevalue(&res);

	/* XXX: todo - validate _CRS checksum? */
ecdtdone:

	dnprintf(10, "%s: Data: 0x%x, S/C: 0x%x\n",
	    DEVNAME(sc), ec_data, ec_sc);

	if (ctype == GAS_SYSTEM_IOSPACE)
		sc->sc_cmd_bt = aa->aaa_iot;
	else
		sc->sc_cmd_bt = aa->aaa_memt;

	if (bus_space_map(sc->sc_cmd_bt, ec_sc, 1, 0, &sc->sc_cmd_bh)) {
		dnprintf(10, "%s: failed to map S/C reg.\n", DEVNAME(sc));
		return (1);
	}

	if (dtype == GAS_SYSTEM_IOSPACE)
		sc->sc_data_bt = aa->aaa_iot;
	else
		sc->sc_data_bt = aa->aaa_memt;

	if (bus_space_map(sc->sc_data_bt, ec_data, 1, 0, &sc->sc_data_bh)) {
		dnprintf(10, "%s: failed to map DATA reg.\n", DEVNAME(sc));
		bus_space_unmap(sc->sc_cmd_bt, sc->sc_cmd_bh, 1);
		return (1);
	}

	return (0);
}

int
acpiec_reg(struct acpiec_softc *sc)
{
	struct aml_value arg[2];

	memset(&arg, 0, sizeof(arg));
	arg[0].type = AML_OBJTYPE_INTEGER;
	arg[0].v_integer = REG_TYPE_EC;
	arg[1].type = AML_OBJTYPE_INTEGER;
	arg[1].v_integer = 1;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_REG", 2,
	    arg, NULL) != 0) {
		dnprintf(10, "%s: eval method _REG failed\n", DEVNAME(sc));
		return (1);
	}

	return (0);
}
