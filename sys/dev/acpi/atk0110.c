/*	$OpenBSD: atk0110.c,v 1.2 2010/01/04 17:30:23 deraadt Exp $	*/

/*
 * Copyright (c) 2009 Constantine A. Murenin <cnst+openbsd@bugmail.mojo.ru>
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
#include <sys/sensors.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

/*
 * ASUSTeK AI Booster (ACPI ATK0110).
 *
 * The driver was inspired by Takanori Watanabe's acpi_aiboost driver.
 * http://cvsweb.freebsd.org/src/sys/dev/acpi_support/acpi_aiboost.c
 *
 * Special thanks goes to Sam Fourman Jr. for providing access to several
 * ASUS boxes where the driver could be tested.
 *
 *							-- cnst.su.
 */

#define AIBS_MORE_SENSORS
/* #define AIBS_VERBOSE */

struct aibs_sensor {
	struct ksensor	s;
	int64_t		i;
	int64_t		l;
	int64_t		h;
};

struct aibs_softc {
	struct device		sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	struct aibs_sensor	*sc_asens_volt;
	struct aibs_sensor	*sc_asens_temp;
	struct aibs_sensor	*sc_asens_fan;

	struct ksensordev	sc_sensordev;
};


int	aibs_match(struct device *, void *, void *);
void	aibs_attach(struct device *, struct device *, void *);
void	aibs_refresh(void *);

void	aibs_attach_sif(struct aibs_softc *, enum sensor_type);
void	aibs_refresh_r(struct aibs_softc *, enum sensor_type);


struct cfattach aibs_ca = {
	sizeof(struct aibs_softc), aibs_match, aibs_attach
};

struct cfdriver aibs_cd = {
	NULL, "aibs", DV_DULL
};

static const char* aibs_hids[] = {
	ACPI_DEV_ASUSAIBOOSTER,
	NULL
};

int
aibs_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata		*cf = match;

	return acpi_matchhids(aa, aibs_hids, cf->cf_driver->cd_name);
}

void
aibs_attach(struct device *parent, struct device *self, void *aux)
{
	struct aibs_softc	*sc = (struct aibs_softc *)self;
	struct acpi_attach_args	*aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;

	printf("\n");

	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	aibs_attach_sif(sc, SENSOR_TEMP);
	aibs_attach_sif(sc, SENSOR_FANRPM);
	aibs_attach_sif(sc, SENSOR_VOLTS_DC);

	if (sc->sc_sensordev.sensors_count == 0) {
		printf("%s: no sensors found\n", DEVNAME(sc));
		return;
	}

	if (sensor_task_register(sc, aibs_refresh, 5) == NULL) {
		printf("%s: unable to register update task\n", DEVNAME(sc));
		return;
	}

	sensordev_install(&sc->sc_sensordev);
}

void
aibs_attach_sif(struct aibs_softc *sc, enum sensor_type st)
{
	struct aml_value	res;
	struct aml_value	**v;
	int			i, n;
	char			*name = "?SIF";
	struct aibs_sensor	*as;

	switch (st) {
	case SENSOR_TEMP:
		name[0] = 'T';
		break;
	case SENSOR_FANRPM:
		name[0] = 'F';
		break;
	case SENSOR_VOLTS_DC:
		name[0] = 'V';
		break;
	default:
		return;
	}

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, name, 0, NULL, &res)) {
		printf("%s: %s not found\n", DEVNAME(sc), name);
		aml_freevalue(&res);
		return;
	}

	if (res.type != AML_OBJTYPE_PACKAGE) {
		printf("%s: %s: not a package\n", DEVNAME(sc), name);
		aml_freevalue(&res);
		return;
	}

	v = res.v_package;
	if (v[0]->type != AML_OBJTYPE_INTEGER) {
		printf("%s: %s[0]: invalid type\n", DEVNAME(sc), name);
		aml_freevalue(&res);
		return;
	}

	n = v[0]->v_integer;
	if (res.length - 1 < n) {
		printf("%s: %s: invalid package\n", DEVNAME(sc), name);
		aml_freevalue(&res);
		return;
	} else if (res.length - 1 > n) {
		printf("%s: %s: misformed package: %i/%i",
		    DEVNAME(sc), name, n, res.length - 1);
#ifdef AIBS_MORE_SENSORS
		n = res.length - 1;
#endif
		printf(", assume %i\n", n);
	}
	if (n < 1) {
		printf("%s: %s: no members in the package\n",
		    DEVNAME(sc), name);
		aml_freevalue(&res);
		return;
	}

	as = malloc(sizeof(*as) * n, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (as == NULL) {
		printf("%s: %s: malloc fail\n", DEVNAME(sc), name);
		aml_freevalue(&res);
		return;
	}

	switch (st) {
	case SENSOR_TEMP:
		sc->sc_asens_temp = as;
		break;
	case SENSOR_FANRPM:
		sc->sc_asens_fan = as;
		break;
	case SENSOR_VOLTS_DC:
		sc->sc_asens_volt = as;
		break;
	default:
		/* NOTREACHED */
		return;
	}

	for (i = 0, v++; i < n; i++, v++) {
		struct aml_value	ri;

		if(v[0]->type != AML_OBJTYPE_STRING) {
			printf("%s: %s: %i: not a string: %i type\n",
			    DEVNAME(sc), name, i, v[0]->type);
			continue;
		}
		if (aml_evalname(sc->sc_acpi, sc->sc_devnode, v[0]->v_string,
		    0, NULL, &ri)) {
			printf("%s: %s: %i: %s not found\n",
			    DEVNAME(sc), name, i, v[0]->v_string);
			aml_freevalue(&ri);
			continue;
		}
		if (ri.type != AML_OBJTYPE_PACKAGE) {
			printf("%s: %s: %i: %s: not a package\n",
			    DEVNAME(sc), name, i, v[0]->v_string);
			aml_freevalue(&ri);
			continue;
		}
		if (ri.length != 5 ||
		    ri.v_package[0]->type != AML_OBJTYPE_INTEGER ||
		    ri.v_package[1]->type != AML_OBJTYPE_STRING ||
		    ri.v_package[2]->type != AML_OBJTYPE_INTEGER ||
		    ri.v_package[3]->type != AML_OBJTYPE_INTEGER ||
		    ri.v_package[4]->type != AML_OBJTYPE_INTEGER) {
			printf("%s: %s: %i: %s: invalid package\n",
			    DEVNAME(sc), name, i, v[0]->v_string);
			aml_freevalue(&ri);
			continue;
		}
		as[i].i = ri.v_package[0]->v_integer;
		strlcpy(as[i].s.desc, ri.v_package[1]->v_string,
		    sizeof(as[i].s.desc));
		as[i].l = ri.v_package[2]->v_integer;
		as[i].h = ri.v_package[3]->v_integer;
		as[i].s.type = st;
#ifdef AIBS_VERBOSE
		printf("%s: %s %2i: %4s: "
		    "0x%08llx %20s %5lli / %5lli  0x%llx\n",
		    DEVNAME(sc), name, i, v[0]->v_string,
		    as[i].i, as[i].s.desc, as[i].l, as[i].h,
		    ri.v_package[4]->v_integer);
#endif
		sensor_attach(&sc->sc_sensordev, &as[i].s);
		aml_freevalue(&ri);
	}

	aml_freevalue(&res);
	return;
}

void
aibs_refresh(void *arg)
{
	struct aibs_softc *sc = arg;

	aibs_refresh_r(sc, SENSOR_TEMP);
	aibs_refresh_r(sc, SENSOR_FANRPM);
	aibs_refresh_r(sc, SENSOR_VOLTS_DC);
}

void
aibs_refresh_r(struct aibs_softc *sc, enum sensor_type st)
{
	struct aml_node		*node;
	int			i, n = sc->sc_sensordev.maxnumt[st];
	char			*name;
	struct aibs_sensor	*as;

	switch (st) {
	case SENSOR_TEMP:
		name = "RTMP";
		as = sc->sc_asens_temp;
		break;
	case SENSOR_FANRPM:
		name = "RFAN";
		as = sc->sc_asens_fan;
		break;
	case SENSOR_VOLTS_DC:
		name = "RVLT";
		as = sc->sc_asens_volt;
		break;
	default:
		return;
	}

	if (as == NULL)
		return;

	node = aml_searchname(sc->sc_devnode, name);
	if (node == NULL || node->value == NULL ||
	    node->value->type != AML_OBJTYPE_METHOD) {
		dprintf("%s: %s: method node not found\n",
		    DEVNAME(sc), name);
		for (i = 0; i < n; i++)
			as[i].s.flags |= SENSOR_FINVALID;
		return;
	}

	for (i = 0; i < n; i++) {
		struct aml_value	req, res;
		struct ksensor		*s;
		int64_t			v, l, h;

		req.type = AML_OBJTYPE_INTEGER;
		req.v_integer = as[i].i;
		if (aml_evalnode(sc->sc_acpi, node, 1, &req, &res)) {
			dprintf("%s: %s: %i: evaluation failed\n",
			    DEVNAME(sc), name, i);
			aml_freevalue(&res);
			s->flags |= SENSOR_FINVALID;
			continue;
		}
		if (res.type != AML_OBJTYPE_INTEGER) {
			dprintf("%s: %s: %i: not an integer: type %i\n",
			    DEVNAME(sc), name, i, res.type);
			aml_freevalue(&res);
			s->flags |= SENSOR_FINVALID;
			continue;
		}

		v = res.v_integer;
		s = &as[i].s;
		l = as[i].l;
		h = as[i].h;

		switch (st) {
		case SENSOR_TEMP:
			s->value = v * 100 * 1000 + 273150000;
			if (v == 0) {
				s->status = SENSOR_S_UNKNOWN;
				s->flags |= SENSOR_FINVALID;
			} else {
				if (v > h)
					s->status = SENSOR_S_CRIT;
				else if (v > l)
					s->status = SENSOR_S_WARN;
				else
					s->status = SENSOR_S_OK;
				s->flags &= ~SENSOR_FINVALID;
			}
			break;
		case SENSOR_FANRPM:
			s->value = v;
			/* some boards have strange limits for fans */
			if ((l != 0 && l < v && v < h) ||
			    (l == 0 && v > h))
				s->status = SENSOR_S_OK;
			else
				s->status = SENSOR_S_WARN;
			s->flags &= ~SENSOR_FINVALID;
			break;
		case SENSOR_VOLTS_DC:
			s->value = v * 1000;
			if (l < v && v < h)
				s->status = SENSOR_S_OK;
			else
				s->status = SENSOR_S_WARN;
			s->flags &= ~SENSOR_FINVALID;
			break;
		default:
			/* NOTREACHED */
			break;
		}
		aml_freevalue(&res);
	}

	return;
}
