/* $OpenBSD: acpicpu.c,v 1.63 2015/03/14 03:38:46 jsg Exp $ */
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
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <machine/bus.h>
#include <machine/cpu.h>
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
void	acpicpu_setperf_ppc_change(struct acpicpu_pss *, int);

#define ACPI_STATE_C0		0x00
#define ACPI_STATE_C1		0x01
#define ACPI_STATE_C2		0x02
#define ACPI_STATE_C3		0x03

#define ACPI_PDC_REVID		0x1
#define ACPI_PDC_SMP		0xa
#define ACPI_PDC_MSR		0x1

/* _PDC Intel capabilities flags from linux */
#define ACPI_PDC_P_FFH		0x0001
#define ACPI_PDC_C_C1_HALT	0x0002
#define ACPI_PDC_T_FFH		0x0004
#define ACPI_PDC_SMP_C1PT	0x0008
#define ACPI_PDC_SMP_C2C3	0x0010
#define ACPI_PDC_SMP_P_SWCOORD	0x0020
#define ACPI_PDC_SMP_C_SWCOORD	0x0040
#define ACPI_PDC_SMP_T_SWCOORD	0x0080
#define ACPI_PDC_C_C1_FFH	0x0100
#define ACPI_PDC_C_C2C3_FFH	0x0200

#define FLAGS_NO_C2		0x01
#define FLAGS_NO_C3		0x02
#define FLAGS_BMCHECK		0x04
#define FLAGS_NOTHROTTLE	0x08
#define FLAGS_NOPSS		0x10
#define FLAGS_NOPCT		0x20

#define CPU_THT_EN		(1L << 4)
#define CPU_MAXSTATE(sc)	(1L << (sc)->sc_duty_wid)
#define CPU_STATE(sc,pct)	((pct * CPU_MAXSTATE(sc) / 100) << (sc)->sc_duty_off)
#define CPU_STATEMASK(sc)	((CPU_MAXSTATE(sc) - 1) << (sc)->sc_duty_off)

#define ACPI_MAX_C2_LATENCY	100
#define ACPI_MAX_C3_LATENCY	1000

/* Make sure throttling bits are valid,a=addr,o=offset,w=width */
#define valid_throttle(o,w,a)	(a && w && (o+w)<=31 && (o>4 || (o+w)<=4))

struct acpi_cstate
{
	int	 type;
	int	 latency;
	int	 power;
	int	 address;

	SLIST_ENTRY(acpi_cstate) link;
};

struct acpicpu_softc {
	struct device		sc_dev;
	int			sc_cpu;

	int			sc_duty_wid;
	int			sc_duty_off;
	int			sc_pblk_addr;
	int			sc_pblk_len;
	int			sc_flags;

	SLIST_HEAD(,acpi_cstate) sc_cstates;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	int			sc_pss_len;
	int			sc_ppc;
	int			sc_level;
	struct acpicpu_pss	*sc_pss;

	struct acpicpu_pct	sc_pct;
	/* save compensation for pct access for lying bios' */
	u_int32_t		sc_pct_stat_as;
	u_int32_t		sc_pct_ctrl_as;
	u_int32_t		sc_pct_stat_len;
	u_int32_t		sc_pct_ctrl_len;
	/*
	 * XXX: _PPC Change listener
	 * PPC changes can occur when for example a machine is disconnected
	 * from AC power and can no loger support the highest frequency or
	 * voltage when driven from the battery.
	 * Should probably be reimplemented as a list for now we assume only
	 * one listener
	 */
	void			(*sc_notify)(struct acpicpu_pss *, int);
};

void    acpicpu_add_cstatepkg(struct aml_value *, void *);
int	acpicpu_getppc(struct acpicpu_softc *);
int	acpicpu_getpct(struct acpicpu_softc *);
int	acpicpu_getpss(struct acpicpu_softc *);
struct acpi_cstate *acpicpu_add_cstate(struct acpicpu_softc *, int, int, int,
    int);
void	acpicpu_set_pdc(struct acpicpu_softc *);

#if 0
void    acpicpu_set_throttle(struct acpicpu_softc *, int);
struct acpi_cstate *acpicpu_find_cstate(struct acpicpu_softc *, int);
#endif

struct cfattach acpicpu_ca = {
	sizeof(struct acpicpu_softc), acpicpu_match, acpicpu_attach
};

struct cfdriver acpicpu_cd = {
	NULL, "acpicpu", DV_DULL
};

extern int setperf_prio;

struct acpicpu_softc *acpicpu_sc[MAXCPUS];

#if 0
void
acpicpu_set_throttle(struct acpicpu_softc *sc, int level)
{
	uint32_t pbval;

	if (sc->sc_flags & FLAGS_NOTHROTTLE)
		return;

	/* Disable throttling control */
	pbval = inl(sc->sc_pblk_addr);
	outl(sc->sc_pblk_addr, pbval & ~CPU_THT_EN);
	if (level < 100) {
		pbval &= ~CPU_STATEMASK(sc);
		pbval |= CPU_STATE(sc, level);
		outl(sc->sc_pblk_addr, pbval & ~CPU_THT_EN);
		outl(sc->sc_pblk_addr, pbval | CPU_THT_EN);
	}
}

struct acpi_cstate *
acpicpu_find_cstate(struct acpicpu_softc *sc, int type)
{
	struct acpi_cstate	*cx;

	SLIST_FOREACH(cx, &sc->sc_cstates, link)
		if (cx->type == type)
			return cx;
	return (NULL);
}
#endif


void
acpicpu_set_pdc(struct acpicpu_softc *sc)
{
	struct aml_value cmd, osc_cmd[4];
	struct aml_value res;
	uint32_t cap;
	uint32_t buf[3];

	/* 4077A616-290C-47BE-9EBD-D87058713953 */
	static uint8_t cpu_oscuuid[16] = { 0x16, 0xA6, 0x77, 0x40, 0x0C, 0x29,
					   0xBE, 0x47, 0x9E, 0xBD, 0xD8, 0x70,
					   0x58, 0x71, 0x39, 0x53 };
	cap = ACPI_PDC_C_C1_HALT | ACPI_PDC_P_FFH | ACPI_PDC_C_C1_FFH
	    | ACPI_PDC_C_C2C3_FFH | ACPI_PDC_SMP_P_SWCOORD | ACPI_PDC_SMP_C2C3
	    | ACPI_PDC_SMP_C1PT;

	if (aml_searchname(sc->sc_devnode, "_OSC")) {
		/* Query _OSC */
		memset(&osc_cmd, 0, sizeof(cmd) * 4);
		osc_cmd[0].type = AML_OBJTYPE_BUFFER;
		osc_cmd[0].v_buffer = (uint8_t *)&cpu_oscuuid;
		osc_cmd[0].length = sizeof(cpu_oscuuid);

		osc_cmd[1].type = AML_OBJTYPE_INTEGER;
		osc_cmd[1].v_integer = 1;
		osc_cmd[1].length = 1;

		osc_cmd[2].type = AML_OBJTYPE_INTEGER;
		osc_cmd[2].v_integer = 2;
		osc_cmd[2].length = 1;

		buf[0] = 1;
		buf[1] = cap;
		osc_cmd[3].type = AML_OBJTYPE_BUFFER;
		osc_cmd[3].v_buffer = (int8_t *)&buf;
		osc_cmd[3].length = sizeof(buf);

		aml_evalname(sc->sc_acpi, sc->sc_devnode, "_OSC",
		    4, osc_cmd, &res);

		if (res.type != AML_OBJTYPE_BUFFER || res.length < 8) {
			printf(": unable to query capabilities\n");
			return;
		}

		/* Evaluate _OSC */
		memset(&osc_cmd, 0, sizeof(cmd) * 4);
		osc_cmd[0].type = AML_OBJTYPE_BUFFER;
		osc_cmd[0].v_buffer = (uint8_t *)&cpu_oscuuid;
		osc_cmd[0].length = sizeof(cpu_oscuuid);

		osc_cmd[1].type = AML_OBJTYPE_INTEGER;
		osc_cmd[1].v_integer = 1;
		osc_cmd[1].length = 1;

		osc_cmd[2].type = AML_OBJTYPE_INTEGER;
		osc_cmd[2].v_integer = 2;
		osc_cmd[2].length = 1;

		buf[0] = 0;
		buf[1] = (*(uint32_t *)&res.v_buffer[4]) & cap;
		osc_cmd[3].type = AML_OBJTYPE_BUFFER;
		osc_cmd[3].v_buffer = (int8_t *)&buf;
		osc_cmd[3].length = sizeof(buf);

		aml_evalname(sc->sc_acpi, sc->sc_devnode, "_OSC",
		    4, osc_cmd, &res);
	} else {
		/* Evaluate _PDC */
		memset(&cmd, 0, sizeof(cmd));
		cmd.type = AML_OBJTYPE_BUFFER;
		cmd.v_buffer = (uint8_t *)&buf;
		cmd.length = sizeof(buf);

		buf[0] = ACPI_PDC_REVID;
		buf[1] = 1;
		buf[2] = cap;

		aml_evalname(sc->sc_acpi, sc->sc_devnode, "_PDC",
		    1, &cmd, &res);
	}
}


struct acpi_cstate *
acpicpu_add_cstate(struct acpicpu_softc *sc, int type, int latency, int power,
    int address)
{
	struct acpi_cstate	*cx;

	dnprintf(10," C%d: latency:.%4x power:%.4x addr:%.8x\n",
	    type, latency, power, address);

	switch (type) {
	case ACPI_STATE_C2:
		if (latency > ACPI_MAX_C2_LATENCY || !address ||
		    (sc->sc_flags & FLAGS_NO_C2))
			goto bad;
		break;
	case ACPI_STATE_C3:
		if (latency > ACPI_MAX_C3_LATENCY || !address ||
		    (sc->sc_flags & FLAGS_NO_C3))
			goto bad;
		break;
	}

	cx = malloc(sizeof(*cx), M_DEVBUF, M_WAITOK | M_ZERO);

	cx->type = type;
	cx->power = power;
	cx->latency = latency;
	cx->address = address;

	SLIST_INSERT_HEAD(&sc->sc_cstates, cx, link);

	return (cx);
 bad:
	dprintf("acpicpu%d: C%d not supported", sc->sc_cpu, type);
	return (NULL);
}

/* Found a _CST object, add new cstate for each entry */
void
acpicpu_add_cstatepkg(struct aml_value *val, void *arg)
{
	struct acpicpu_softc	*sc = arg;

#if defined(ACPI_DEBUG) && !defined(SMALL_KERNEL)
	aml_showvalue(val, 0);
#endif
	if (val->type != AML_OBJTYPE_PACKAGE || val->length != 4)
		return;

	acpicpu_add_cstate(sc, val->v_package[1]->v_integer,
	    val->v_package[2]->v_integer,
	    val->v_package[3]->v_integer, -1);
}


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
	struct aml_value	res;
	int			i;
	struct acpi_cstate	*cx;
	u_int32_t		status = 0;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;
	acpicpu_sc[sc->sc_dev.dv_unit] = sc;

	SLIST_INIT(&sc->sc_cstates);

	sc->sc_pss = NULL;

	if (aml_evalnode(sc->sc_acpi, sc->sc_devnode, 0, NULL, &res) == 0) {
		if (res.type == AML_OBJTYPE_PROCESSOR) {
			sc->sc_cpu = res.v_processor.proc_id;
			sc->sc_pblk_addr = res.v_processor.proc_addr;
			sc->sc_pblk_len = res.v_processor.proc_len;
		}
		aml_freevalue(&res);
	}
	sc->sc_duty_off = sc->sc_acpi->sc_fadt->duty_offset;
	sc->sc_duty_wid = sc->sc_acpi->sc_fadt->duty_width;

	acpicpu_set_pdc(sc);

	if (!valid_throttle(sc->sc_duty_off, sc->sc_duty_wid, sc->sc_pblk_addr))
		sc->sc_flags |= FLAGS_NOTHROTTLE;
#ifdef ACPI_DEBUG
	printf(": %s: ", sc->sc_devnode->name);
	printf("\n: hdr:%x pblk:%x,%x duty:%x,%x pstate:%x "
	       "(%ld throttling states)\n", sc->sc_acpi->sc_fadt->hdr_revision,
		sc->sc_pblk_addr, sc->sc_pblk_len, sc->sc_duty_off,
		sc->sc_duty_wid, sc->sc_acpi->sc_fadt->pstate_cnt,
		CPU_MAXSTATE(sc));
#endif

	/* Get C-States from _CST or FADT */
	if (!aml_evalname(sc->sc_acpi, sc->sc_devnode, "_CST", 0, NULL, &res)) {
		aml_foreachpkg(&res, 1, acpicpu_add_cstatepkg, sc);
		aml_freevalue(&res);
	}
	else {
		/* Some systems don't export a full PBLK reduce functionality */
		if (sc->sc_pblk_len < 5)
			sc->sc_flags |= FLAGS_NO_C2;
		if (sc->sc_pblk_len < 6)
			sc->sc_flags |= FLAGS_NO_C3;
		acpicpu_add_cstate(sc, ACPI_STATE_C2,
		    sc->sc_acpi->sc_fadt->p_lvl2_lat, -1,
		    sc->sc_pblk_addr + 4);
		acpicpu_add_cstate(sc, ACPI_STATE_C3,
		    sc->sc_acpi->sc_fadt->p_lvl3_lat, -1,
		    sc->sc_pblk_addr + 5);
	}
	if (acpicpu_getpss(sc)) {
		sc->sc_flags |= FLAGS_NOPSS;
	} else {
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
		if (sc->sc_pss_len == 0) {
			/* this should never happen */
			printf("%s: invalid _PSS length\n", DEVNAME(sc));
			sc->sc_flags |= FLAGS_NOPSS;
		}

		acpicpu_getppc(sc);
		if (acpicpu_getpct(sc))
			sc->sc_flags |= FLAGS_NOPCT;
		else if (sc->sc_pss_len > 0) {
			/* Notify BIOS we are handing p-states */
			if (sc->sc_acpi->sc_fadt->pstate_cnt)
				acpi_write_pmreg(sc->sc_acpi, ACPIREG_SMICMD, 0,
				sc->sc_acpi->sc_fadt->pstate_cnt);

			aml_register_notify(sc->sc_devnode, NULL,
			    acpicpu_notify, sc, ACPIDEV_NOPOLL);

			acpi_gasio(sc->sc_acpi, ACPI_IOREAD,
			    sc->sc_pct.pct_status.grd_gas.address_space_id,
			    sc->sc_pct.pct_status.grd_gas.address,
			    sc->sc_pct_stat_as, sc->sc_pct_stat_as, &status);
			sc->sc_level = (100 / sc->sc_pss_len) *
			    (sc->sc_pss_len - status);
			dnprintf(20, "%s: cpu index %d, percentage %d\n",
			    DEVNAME(sc), status, sc->sc_level);
			if (setperf_prio < 30) {
				cpu_setperf = acpicpu_setperf;
				acpicpu_set_notify(acpicpu_setperf_ppc_change);
				setperf_prio = 30;
				acpi_hasprocfvs = 1;
			}
		}
	}

	/*
	 * Nicely enumerate what power management capabilities
	 * ACPI CPU provides.
	 */
	if (!SLIST_EMPTY(&sc->sc_cstates)) {
		printf(":");

		i = 0;
		SLIST_FOREACH(cx, &sc->sc_cstates, link) {
			if (i++)
				printf(",");
			switch (cx->type) {
			case ACPI_STATE_C0:
				printf(" C0");
				break;
			case ACPI_STATE_C1:
				printf(" C1");
				break;
			case ACPI_STATE_C2:
				printf(" C2");
				break;
			case ACPI_STATE_C3:
				printf(" C3");
				break;
			}
		}
	}

	if (!(sc->sc_flags & (FLAGS_NOPSS | FLAGS_NOPCT)) ||
	    !(sc->sc_flags & FLAGS_NOPSS)) {
		printf("%c ", SLIST_EMPTY(&sc->sc_cstates) ? ':' : ',');

		/*
		 * If acpicpu is itself providing the capability to transition
		 * states, enumerate them in the fashion that est and powernow
		 * would.
		 */
		if (!(sc->sc_flags & (FLAGS_NOPSS | FLAGS_NOPCT))) {
			printf("FVS, ");
			for (i = 0; i < sc->sc_pss_len - 1; i++)
				printf("%d, ", sc->sc_pss[i].pss_core_freq);
			printf("%d MHz", sc->sc_pss[i].pss_core_freq);
		} else
			printf("PSS");
	}

	printf("\n");
}

int
acpicpu_getppc(struct acpicpu_softc *sc)
{
	struct aml_value	res;

	sc->sc_ppc = 0;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_PPC", 0, NULL, &res)) {
		dnprintf(10, "%s: no _PPC\n", DEVNAME(sc));
		return (1);
	}

	sc->sc_ppc = aml_val2int(&res);
	dnprintf(10, "%s: _PPC: %d\n", DEVNAME(sc), sc->sc_ppc);
	aml_freevalue(&res);

	return (0);
}

int
acpicpu_getpct(struct acpicpu_softc *sc)
{
	struct aml_value	res;
	int			rv = 1;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_PCT", 0, NULL, &res)) {
		dnprintf(20, "%s: no _PCT\n", DEVNAME(sc));
		return (1);
	}

	if (res.length != 2) {
		dnprintf(20, "%s: %s: invalid _PCT length\n", DEVNAME(sc),
		    sc->sc_devnode->name);
		return (1);
	}

	memcpy(&sc->sc_pct.pct_ctrl, res.v_package[0]->v_buffer,
	    sizeof sc->sc_pct.pct_ctrl);
	if (sc->sc_pct.pct_ctrl.grd_gas.address_space_id ==
	    GAS_FUNCTIONAL_FIXED) {
		dnprintf(20, "CTRL GASIO is functional fixed hardware.\n");
		goto ffh;
	}

	memcpy(&sc->sc_pct.pct_status, res.v_package[1]->v_buffer,
	    sizeof sc->sc_pct.pct_status);
	if (sc->sc_pct.pct_status.grd_gas.address_space_id ==
	    GAS_FUNCTIONAL_FIXED) {
		dnprintf(20, "CTRL GASIO is functional fixed hardware.\n");
		goto ffh;
	}

	dnprintf(10, "_PCT(ctrl)  : %02x %04x %02x %02x %02x %02x %016llx\n",
	    sc->sc_pct.pct_ctrl.grd_descriptor,
	    sc->sc_pct.pct_ctrl.grd_length,
	    sc->sc_pct.pct_ctrl.grd_gas.address_space_id,
	    sc->sc_pct.pct_ctrl.grd_gas.register_bit_width,
	    sc->sc_pct.pct_ctrl.grd_gas.register_bit_offset,
	    sc->sc_pct.pct_ctrl.grd_gas.access_size,
	    sc->sc_pct.pct_ctrl.grd_gas.address);

	dnprintf(10, "_PCT(status): %02x %04x %02x %02x %02x %02x %016llx\n",
	    sc->sc_pct.pct_status.grd_descriptor,
	    sc->sc_pct.pct_status.grd_length,
	    sc->sc_pct.pct_status.grd_gas.address_space_id,
	    sc->sc_pct.pct_status.grd_gas.register_bit_width,
	    sc->sc_pct.pct_status.grd_gas.register_bit_offset,
	    sc->sc_pct.pct_status.grd_gas.access_size,
	    sc->sc_pct.pct_status.grd_gas.address);

	/* if not set assume single 32 bit access */
	sc->sc_pct_stat_as = sc->sc_pct.pct_status.grd_gas.register_bit_width
	    / 8;
	if (sc->sc_pct_stat_as == 0)
		sc->sc_pct_stat_as = 4;
	sc->sc_pct_ctrl_as = sc->sc_pct.pct_ctrl.grd_gas.register_bit_width / 8;
	if (sc->sc_pct_ctrl_as == 0)
		sc->sc_pct_ctrl_as = 4;
	sc->sc_pct_stat_len = sc->sc_pct.pct_status.grd_gas.access_size;
	if (sc->sc_pct_stat_len == 0)
		sc->sc_pct_stat_len = sc->sc_pct_stat_as;
	sc->sc_pct_ctrl_len = sc->sc_pct.pct_ctrl.grd_gas.access_size;
	if (sc->sc_pct_ctrl_len == 0)
		sc->sc_pct_ctrl_len = sc->sc_pct_ctrl_as;

	rv = 0;
ffh:
	aml_freevalue(&res);
	return (rv);
}

int
acpicpu_getpss(struct acpicpu_softc *sc)
{
	struct aml_value	res;
	int			i, c, cf;

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_PSS", 0, NULL, &res)) {
		dprintf("%s: no _PSS\n", DEVNAME(sc));
		return (1);
	}

	if (sc->sc_pss)
		free(sc->sc_pss, M_DEVBUF, 0);

	sc->sc_pss = mallocarray(res.length, sizeof(*sc->sc_pss), M_DEVBUF,
	    M_WAITOK | M_ZERO);

	c = 0;
	for (i = 0; i < res.length; i++) {
		cf = aml_val2int(res.v_package[i]->v_package[0]);

		/* This heuristic comes from FreeBSDs
		 * dev/acpica/acpi_perf.c to weed out invalid PSS entries.
		 */
		if (cf == sc->sc_pss[c].pss_core_freq) {
			printf("%s: struck PSS entry, core frequency equals "
			    " last\n", sc->sc_dev.dv_xname);
			continue;
		}

		if (cf == 0xFFFF || cf == 0x9999 || cf == 99999 || cf == 0) {
			printf("%s: struck PSS entry, inappropriate core "
			    "frequency value\n", sc->sc_dev.dv_xname);
			continue;
		}

		sc->sc_pss[c].pss_core_freq = cf;
		sc->sc_pss[c].pss_power = aml_val2int(
		    res.v_package[i]->v_package[1]);
		sc->sc_pss[c].pss_trans_latency = aml_val2int(
		    res.v_package[i]->v_package[2]);
		sc->sc_pss[c].pss_bus_latency = aml_val2int(
		    res.v_package[i]->v_package[3]);
		sc->sc_pss[c].pss_ctrl = aml_val2int(
		    res.v_package[i]->v_package[4]);
		sc->sc_pss[c].pss_status = aml_val2int(
		    res.v_package[i]->v_package[5]);
		c++;
	}
	sc->sc_pss_len = c;

	aml_freevalue(&res);

	return (0);
}

int
acpicpu_fetch_pss(struct acpicpu_pss **pss)
{
	struct acpicpu_softc	*sc;

	/*
	 * XXX: According to the ACPI spec in an SMP system all processors
	 * are supposed to support the same states. For now we pray
	 * the bios ensures this...
	 */

	sc = acpicpu_sc[0];
	if (!sc)
		return 0;
	*pss = sc->sc_pss;

	return (sc->sc_pss_len);
}

int
acpicpu_notify(struct aml_node *node, int notify_type, void *arg)
{
	struct acpicpu_softc	*sc = arg;

	dnprintf(10, "acpicpu_notify: %.2x %s\n", notify_type,
	    sc->sc_devnode->name);

	switch (notify_type) {
	case 0x80:	/* _PPC changed, retrieve new values */
		acpicpu_getppc(sc);
		acpicpu_getpss(sc);
		if (sc->sc_notify)
			sc->sc_notify(sc->sc_pss, sc->sc_pss_len);

		break;
	default:
		printf("%s: unhandled cpu event %x\n", DEVNAME(sc),
		    notify_type);
		break;
	}

	return (0);
}

void
acpicpu_set_notify(void (*func)(struct acpicpu_pss *, int))
{
	struct acpicpu_softc    *sc;

	sc = acpicpu_sc[0];
	if (sc != NULL)
		sc->sc_notify = func;
}

void
acpicpu_setperf_ppc_change(struct acpicpu_pss *pss, int npss)
{
	struct acpicpu_softc    *sc;

	sc = acpicpu_sc[0];

	if (sc != NULL)
		cpu_setperf(sc->sc_level);
}

void
acpicpu_setperf(int level)
{
	struct acpicpu_softc	*sc;
	struct acpicpu_pss	*pss = NULL;
	int			idx, len;
	u_int32_t		status = 0;

	sc = acpicpu_sc[cpu_number()];

	dnprintf(10, "%s: acpicpu setperf level %d\n",
	    sc->sc_devnode->name, level);

	if (level < 0 || level > 100) {
		dnprintf(10, "%s: acpicpu setperf illegal percentage\n",
		    sc->sc_devnode->name);
		return;
	}

	/*
	 * XXX this should be handled more gracefully and it needs to also do
	 * the duty cycle method instead of pss exclusively
	 */
	if (sc->sc_flags & FLAGS_NOPSS || sc->sc_flags & FLAGS_NOPCT) {
		dnprintf(10, "%s: acpicpu no _PSS or _PCT\n",
		    sc->sc_devnode->name);
		return;
	}

	if (sc->sc_ppc)
		len = sc->sc_ppc;
	else
		len = sc->sc_pss_len;
	idx = (len - 1) - (level / (100 / len));
	if (idx < 0)
		idx = 0;

	if (sc->sc_ppc)
		idx += sc->sc_pss_len - sc->sc_ppc;

	if (idx > sc->sc_pss_len)
		idx = sc->sc_pss_len - 1;

	dnprintf(10, "%s: acpicpu setperf index %d pss_len %d ppc %d\n",
	    sc->sc_devnode->name, idx, sc->sc_pss_len, sc->sc_ppc);

	pss = &sc->sc_pss[idx];

#ifdef ACPI_DEBUG
	/* keep this for now since we will need this for debug in the field */
	printf("0 status: %x %llx %u %u ctrl: %x %llx %u %u\n",
	    sc->sc_pct.pct_status.grd_gas.address_space_id,
	    sc->sc_pct.pct_status.grd_gas.address,
	    sc->sc_pct_stat_as, sc->sc_pct_stat_len,
	    sc->sc_pct.pct_ctrl.grd_gas.address_space_id,
	    sc->sc_pct.pct_ctrl.grd_gas.address,
	    sc->sc_pct_ctrl_as, sc->sc_pct_ctrl_len);
#endif
	acpi_gasio(sc->sc_acpi, ACPI_IOREAD,
	    sc->sc_pct.pct_status.grd_gas.address_space_id,
	    sc->sc_pct.pct_status.grd_gas.address, sc->sc_pct_stat_as,
	    sc->sc_pct_stat_len, &status);
	dnprintf(20, "1 status: %u <- %u\n", status, pss->pss_status);

	/* Are we already at the requested frequency? */
	if (status == pss->pss_status)
		return;

	acpi_gasio(sc->sc_acpi, ACPI_IOWRITE,
	    sc->sc_pct.pct_ctrl.grd_gas.address_space_id,
	    sc->sc_pct.pct_ctrl.grd_gas.address, sc->sc_pct_ctrl_as,
	    sc->sc_pct_ctrl_len, &pss->pss_ctrl);
	dnprintf(20, "pss_ctrl: %x\n", pss->pss_ctrl);

	acpi_gasio(sc->sc_acpi, ACPI_IOREAD,
	    sc->sc_pct.pct_status.grd_gas.address_space_id,
	    sc->sc_pct.pct_status.grd_gas.address, sc->sc_pct_stat_as,
	    sc->sc_pct_stat_as, &status);
	dnprintf(20, "2 status: %d\n", status);

	/* Did the transition succeed? */
	 if (status == pss->pss_status) {
		cpuspeed = pss->pss_core_freq;
		sc->sc_level = level;
	} else
		printf("%s: acpicpu setperf failed to alter frequency\n",
		    sc->sc_devnode->name);
}
