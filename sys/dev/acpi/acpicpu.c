/* $OpenBSD: acpicpu.c,v 1.19 2007/01/31 23:30:51 gwk Exp $ */
/*
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
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
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <sys/sensors.h>

int	acpicpu_match(struct device *, void *, void *);
void	acpicpu_attach(struct device *, struct device *, void *);
int	acpicpu_notify(struct aml_node *, int, void *);
void	acpicpu_setperf(int);

struct acpicpu_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	int			sc_pss_len;
	struct acpicpu_pss	*sc_pss;

	struct acpicpu_pct	sc_pct;
};

int	acpicpu_getpct(struct acpicpu_softc *);
int	acpicpu_getpss(struct acpicpu_softc *);

struct cfattach acpicpu_ca = {
	sizeof(struct acpicpu_softc), acpicpu_match, acpicpu_attach
};

struct cfdriver acpicpu_cd = {
	NULL, "acpicpu", DV_DULL
};

extern int setperf_prio;

#ifdef __i386__
struct acpicpu_softc *acpicpu_sc[I386_MAXPROCS];
#elif __amd64__
struct acpicpu_softc *acpicpu_sc[X86_MAXPROCS];
#endif

int
acpicpu_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata		*cf = match;

	/* sanity */
	if (aa->aaa_name == NULL ||
	    strcmp(aa->aaa_name, cf->cf_driver->cd_name) != 0 ||
	    aa->aaa_table != NULL)
		return (0);

	return (1);
}

void
acpicpu_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpicpu_softc	*sc = (struct acpicpu_softc *)self;
	struct acpi_attach_args *aa = aux;
	int			i;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node->child;

	sc->sc_pss = NULL;

	printf(": %s: ", sc->sc_devnode->parent->name);
	if (acpicpu_getpss(sc)) {
		/* XXX not the right test but has to do for now */
		printf("can't attach, no _PSS\n");
		return;
	}

#ifdef ACPI_DEBUG
	for (i = 0; i < sc->sc_pss_len; i++) {
		dnprintf(20, "%d %d %d %d %d %d\n",
		    sc->sc_pss[i].pss_core_freq,
		    sc->sc_pss[i].pss_power,
		    sc->sc_pss[i].pss_trans_latency,
		    sc->sc_pss[i].pss_bus_latency,
		    sc->sc_pss[i].pss_ctrl,
		    sc->sc_pss[i].pss_status);
	}
	dnprintf(20, "\n");
#endif
	/* XXX this needs to be moved to probe routine */
	if (acpicpu_getpct(sc))
		return;

	for (i = 0; i < sc->sc_pss_len; i++)
		printf("%d%s", sc->sc_pss[i].pss_core_freq,
		    i < sc->sc_pss_len - 1 ? ", " : " MHz\n");

	aml_register_notify(sc->sc_devnode->parent, NULL,
	    acpicpu_notify, sc, ACPIDEV_NOPOLL);

	if (setperf_prio < 30) {
		cpu_setperf = acpicpu_setperf;
		setperf_prio = 30;
		acpi_hasprocfvs = 1;
	}
	acpicpu_sc[sc->sc_dev.dv_unit] = sc;
}

int
acpicpu_getpct(struct acpicpu_softc *sc)
{
	struct aml_value	res;
	int			rv = 1;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_PPC", 0, NULL, &res)) {
		dnprintf(20, "%s: no _PPC\n", DEVNAME(sc));
		printf("%s: no _PPC\n", DEVNAME(sc));
		return (1);
	}

	dnprintf(10, "_PPC: %d\n", aml_val2int(&res));
	aml_freevalue(&res);

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_PCT", 0, NULL, &res)) {
		printf("%s: no _PCT\n", DEVNAME(sc));
		return (1);
	}

	if (res.length != 2) {
		printf("%s: %s: invalid _PCT length\n", DEVNAME(sc),
		    sc->sc_devnode->parent->name);
		return (1);
	}

	memcpy(&sc->sc_pct.pct_ctrl, res.v_package[0]->v_buffer,
	    sizeof sc->sc_pct.pct_ctrl);
	if (sc->sc_pct.pct_ctrl.grd_gas.address_space_id ==
	    GAS_FUNCTIONAL_FIXED) {
		printf("CTRL GASIO is CPU manufacturer overridden\n");
		goto bad;
	}

	memcpy(&sc->sc_pct.pct_status, res.v_package[1]->v_buffer,
	    sizeof sc->sc_pct.pct_status);
	if (sc->sc_pct.pct_status.grd_gas.address_space_id ==
	    GAS_FUNCTIONAL_FIXED) {
		printf("STATUS GASIO is CPU manufacturer overridden\n");
		goto bad;
	}

	dnprintf(10, "_PCT(ctrl)  : %02x %04x %02x %02x %02x %02x %016x\n",
	    sc->sc_pct.pct_ctrl.grd_descriptor,
	    sc->sc_pct.pct_ctrl.grd_length,
	    sc->sc_pct.pct_ctrl.grd_gas.address_space_id,
	    sc->sc_pct.pct_ctrl.grd_gas.register_bit_width,
	    sc->sc_pct.pct_ctrl.grd_gas.register_bit_offset,
	    sc->sc_pct.pct_ctrl.grd_gas.access_size,
	    sc->sc_pct.pct_ctrl.grd_gas.address);

	dnprintf(10, "_PCT(status): %02x %04x %02x %02x %02x %02x %016x\n",
	    sc->sc_pct.pct_status.grd_descriptor,
	    sc->sc_pct.pct_status.grd_length,
	    sc->sc_pct.pct_status.grd_gas.address_space_id,
	    sc->sc_pct.pct_status.grd_gas.register_bit_width,
	    sc->sc_pct.pct_status.grd_gas.register_bit_offset,
	    sc->sc_pct.pct_status.grd_gas.access_size,
	    sc->sc_pct.pct_status.grd_gas.address);

	rv = 0;
bad:
	aml_freevalue(&res);
	return (rv);
}

int
acpicpu_getpss(struct acpicpu_softc *sc)
{
	struct aml_value	res;
	int			i;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_PSS", 0, NULL, &res)) {
		dnprintf(20, "%s: no _PSS\n", DEVNAME(sc));
		return (1);
	}

	if (sc->sc_pss)
		free(sc->sc_pss, M_DEVBUF);

	sc->sc_pss = malloc(res.length * sizeof *sc->sc_pss, M_DEVBUF,
	    M_WAITOK);

	memset(sc->sc_pss, 0, res.length * sizeof *sc->sc_pss);

	for (i = 0; i < res.length; i++) {
		sc->sc_pss[i].pss_core_freq = aml_val2int(
		    res.v_package[i]->v_package[0]);
		sc->sc_pss[i].pss_power = aml_val2int(
		    res.v_package[i]->v_package[1]);
		sc->sc_pss[i].pss_trans_latency = aml_val2int(
		    res.v_package[i]->v_package[2]);
		sc->sc_pss[i].pss_bus_latency = aml_val2int(
		    res.v_package[i]->v_package[3]);
		sc->sc_pss[i].pss_ctrl = aml_val2int(
		    res.v_package[i]->v_package[4]);
		sc->sc_pss[i].pss_status = aml_val2int(
		    res.v_package[i]->v_package[5]);
	}
	aml_freevalue(&res);

	sc->sc_pss_len = res.length;

	return (0);
}

int
acpicpu_notify(struct aml_node *node, int notify_type, void *arg)
{
	struct acpicpu_softc	*sc = arg;

	dnprintf(10, "acpicpu_notify: %.2x %s\n", notify_type,
	    sc->sc_devnode->parent->name);

	switch (notify_type) {
	case 0x80:	/* _PPC changed, retrieve new values */
		acpicpu_getpct(sc);
		acpicpu_getpss(sc);
		break;
	default:
		printf("%s: unhandled cpu event %x\n", DEVNAME(sc),
		    notify_type);
		break;
	}

	return (0);
}

void
acpicpu_setperf(int level) {
	struct acpicpu_softc	*sc;
	struct acpicpu_pss	*pss = NULL;
	int			idx;
	u_int32_t		stat_as, ctrl_as, stat_len, ctrl_len;
	u_int32_t		status = 0;

	sc = acpicpu_sc[cpu_number()];

	dnprintf(10, "%s: acpicpu setperf level %d\n",
	    sc->sc_devnode->parent->name, level);

	if (level < 0 || level > 100) {
		dnprintf(10, "%s: acpicpu setperf illegal percentage\n",
		    sc->sc_devnode->parent->name);
		return;
	}

	idx = (sc->sc_pss_len - 1) - (level / (100 / sc->sc_pss_len));
	if (idx < 0)
		idx = 0; /* compensate */
	if (idx > sc->sc_pss_len) {
		/* XXX should never happen */
		printf("%s: acpicpu setperf index out of range\n",
		    sc->sc_devnode->parent->name);
		return;
	}

	dnprintf(10, "%s: acpicpu setperf index %d\n",
	    sc->sc_devnode->parent->name, idx);

	pss = &sc->sc_pss[idx];

	/* if not set assume single 32 bit access */
	stat_as = sc->sc_pct.pct_status.grd_gas.register_bit_width / 8;
	if (stat_as == 0)
		stat_as = 4;
	ctrl_as = sc->sc_pct.pct_ctrl.grd_gas.register_bit_width / 8;
	if (ctrl_as == 0)
		ctrl_as = 4;
	stat_len = sc->sc_pct.pct_status.grd_gas.access_size;
	if (stat_len == 0)
		stat_len = stat_as;
	ctrl_len = sc->sc_pct.pct_ctrl.grd_gas.access_size;
	if (ctrl_len == 0)
		ctrl_len = ctrl_as;

#ifdef ACPI_DEBUG
	/* keep this for now since we will need this for debug in the field */
	printf("0 status: %x %llx %u %u ctrl: %x %llx %u %u\n",
	    sc->sc_pct.pct_status.grd_gas.address_space_id,
	    sc->sc_pct.pct_status.grd_gas.address,
	    stat_as, stat_len,
	    sc->sc_pct.pct_ctrl.grd_gas.address_space_id,
	    sc->sc_pct.pct_ctrl.grd_gas.address,
	    ctrl_as, ctrl_len);
#endif
	acpi_gasio(sc->sc_acpi, ACPI_IOREAD,
	    sc->sc_pct.pct_status.grd_gas.address_space_id,
	    sc->sc_pct.pct_status.grd_gas.address, stat_as, stat_len,
	    &status);
	dnprintf(20, "status: %u <- %u\n", status, pss->pss_status);

	/* Are we already at the requested frequency? */
	if (status == pss->pss_status)
		return;

	acpi_gasio(sc->sc_acpi, ACPI_IOWRITE,
	    sc->sc_pct.pct_ctrl.grd_gas.address_space_id,
	    sc->sc_pct.pct_ctrl.grd_gas.address, ctrl_as, ctrl_len,
	    &pss->pss_ctrl);
	dnprintf(20, "pss_ctrl: %x\n", pss->pss_ctrl);

	acpi_gasio(sc->sc_acpi, ACPI_IOREAD,
	    sc->sc_pct.pct_status.grd_gas.address_space_id,
	    sc->sc_pct.pct_status.grd_gas.address, stat_as, stat_as,
	    &status);
	dnprintf(20, "3 status: %d\n", status);

	/* Did the transition succeed? */
	 if (status == pss->pss_status)
		cpuspeed = pss->pss_core_freq;
	else
		printf("%s: acpicpu setperf failed to alter frequency\n",
		    sc->sc_devnode->parent->name);
}
