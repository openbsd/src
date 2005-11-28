/*	$OpenBSD: esm.c,v 1.19 2005/11/28 22:11:07 jordan Exp $ */

/*
 * Copyright (c) 2005 Jordan Hargrave <jordan@openbsd.org>
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/queue.h>
#include <sys/sensors.h>

#include <dev/isa/isareg.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <arch/i386/i386/esmvar.h>
#include <arch/i386/i386/esmreg.h>
#include <arch/i386/isa/isa_machdep.h>

/* #define ESM_DEBUG */

#ifdef ESM_DEBUG
#define DPRINTF(x...)		do { if (esmdebug) printf(x); } while (0)
#define DPRINTFN(n,x...)	do { if (esmdebug > (n)) printf(x); } while (0)
int	esmdebug = 2;
#else
#define DPRINTF(x...)		/* x */
#define DPRINTFN(n,x...)	/* n: x */
#endif

int		esm_match(struct device *, void *, void *);
void		esm_attach(struct device *, struct device *, void *);

enum esm_sensor_type {
	ESM_S_UNKNOWN, /* XXX */
	ESM_S_INTRUSION,
	ESM_S_TEMP,
	ESM_S_FANRPM,
	ESM_S_VOLTS,
	ESM_S_AMPS,
	ESM_S_PWRSUP,
	ESM_S_PCISLOT,
	ESM_S_SCSICONN,
	ESM_S_DRIVES, /* argument is the base index of the drive */
	ESM_S_DRIVE,
	ESM_S_HPSLOT,
	ESM_S_ACSWITCH
};

struct esm_sensor_map {
	enum sensor_type	type;
	long			arg;
#define ESM_A_PWRSUP_1		0x10
#define ESM_A_PWRSUP_2		0x20
#define ESM_A_PWRSUP_3		0x40
#define ESM_A_PWRSUP_4		0x80
#define ESM_A_SCSI_A		0x01
#define ESM_A_SCSI_B		0x02
	const char		*name;
};

struct esm_sensor {
	u_int8_t		es_dev;
	u_int8_t		es_id;

	enum esm_sensor_type	es_type;

	struct sensor		*es_sensor;
	TAILQ_ENTRY(esm_sensor)	es_entry;
};

struct esm_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	TAILQ_HEAD(, esm_sensor) sc_sensors;
	struct esm_sensor	*sc_nextsensor;
	int			sc_retries;
	struct timeout		sc_timeout;

	int 			sc_wdog_period;
};

struct cfattach esm_ca = {
	sizeof(struct esm_softc), esm_match, esm_attach
};

struct cfdriver esm_cd = {
	NULL, "esm", DV_DULL
};

#define DEVNAME(s)	((s)->sc_dev.dv_xname)

#define EREAD(s, r)	bus_space_read_1((s)->sc_iot, (s)->sc_ioh, (r))
#define EWRITE(s, r, v)	bus_space_write_1((s)->sc_iot, (s)->sc_ioh, (r), (v))

#define ECTRLWR(s, v)	EWRITE((s), ESM2_CTRL_REG, (v))
#define EDATARD(s)	EREAD((s), ESM2_DATA_REG)
#define EDATAWR(s, v)	EWRITE((s), ESM2_DATA_REG, (v))

int		esm_watchdog(void *, int);

void		esm_refresh(void *);

int		esm_get_devmap(struct esm_softc *, int, struct esm_devmap *);
void		esm_devmap(struct esm_softc *, struct esm_devmap *);
void		esm_make_sensors(struct esm_softc *, struct esm_devmap *,
		    struct esm_sensor_map *, int);

int		esm_bmc_ready(struct esm_softc *, int, u_int8_t, u_int8_t, int);
int		esm_cmd(struct esm_softc *, void *, size_t, void *, size_t,
		    int);
int		esm_smb_cmd(struct esm_softc *, struct esm_smb_req *,
		    struct esm_smb_resp *, int);

int64_t		esm_val2temp(u_int16_t);
int64_t		esm_val2volts(u_int16_t);


/* Determine if this is a Dell server */
int
esm_probe(void *aux)
{
	const char *pdellstr;
	struct dell_sysid *pdellid;
	uint16_t sysid;

	pdellstr = (const char *)ISA_HOLE_VADDR(DELL_SYSSTR_ADDR);
	DPRINTF("Dell String: %s\n", pdellstr);
	if (strncmp(pdellstr, "Dell System", 11))
		return (0); 

	pdellid = (struct dell_sysid *)ISA_HOLE_VADDR(DELL_SYSID_ADDR);
	if ((sysid = pdellid->sys_id) == DELL_SYSID_EXT)
		sysid = pdellid->ext_id;
	DPRINTF("SysId: %x\n", sysid);

 	switch(sysid) {
	case DELL_SYSID_2300:
	case DELL_SYSID_4300:
	case DELL_SYSID_4350:
	case DELL_SYSID_6300:
	case DELL_SYSID_6350:
	case DELL_SYSID_2400:
	case DELL_SYSID_2450:
	case DELL_SYSID_4400:
	case DELL_SYSID_6400:
	case DELL_SYSID_6450:
	case DELL_SYSID_2500:
	case DELL_SYSID_2550:
	case DELL_SYSID_PV530F:
	case DELL_SYSID_PV735N:
	case DELL_SYSID_PV750N:
	case DELL_SYSID_PV755N:
	case DELL_SYSID_PA200:
		return (1);
	}

	return (0);
}

int
esm_match(struct device *parent, void *match, void *aux)
{
	struct esm_attach_args		*eaa = aux;

	if (strncmp(eaa->eaa_name, esm_cd.cd_name, sizeof(esm_cd.cd_name)) == 0 &&
	    esm_probe(eaa))
		return (1);

	return (0);
}

void
esm_attach(struct device *parent, struct device *self, void *aux)
{
	struct esm_softc		*sc = (struct esm_softc *)self;
	struct esm_attach_args		*eaa = aux;
	u_int8_t			x;

	struct esm_devmap		devmap;
	int				i;

	sc->sc_iot = eaa->eaa_iot;
	TAILQ_INIT(&sc->sc_sensors);

	if (bus_space_map(sc->sc_iot, ESM2_BASE_PORT, 8, 0, &sc->sc_ioh) != 0) {
		printf(": unable to map memory\n");
		return;
	}

	/* turn off interrupts here */
	x = EREAD(sc, ESM2_INTMASK_REG);
	x &= ~(ESM2_TIM_SCI_EN|ESM2_TIM_SMI_EN|ESM2_TIM_NMI2SMI);
	x |= ESM2_TIM_POWER_UP_BITS;
	EWRITE(sc, ESM2_INTMASK_REG, x);

	/* clear event doorbells */
	x = EREAD(sc, ESM2_CTRL_REG);
	x &= ~ESM2_TC_HOSTBUSY;
	x |= ESM2_TC_POWER_UP_BITS;
	EWRITE(sc, ESM2_CTRL_REG, x);

	/* see if card is alive */
	if (esm_bmc_ready(sc, ESM2_CTRL_REG, ESM2_TC_ECBUSY, 0, 1) != 0) {
		printf("%s: card is not alive\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, 8);
		return;
	}

	sc->sc_wdog_period = 0;
	wdog_register(sc, esm_watchdog);
	printf(": Hardware Watchdog\n");

	for (i = 0; i <= 0xff; i++) {
		if (esm_get_devmap(sc, i, &devmap) != 0)
			break; /* XXX not continue? */
		esm_devmap(sc, &devmap);
	}

	if (!TAILQ_EMPTY(&sc->sc_sensors)) {
		DPRINTF("%s: starting refresh\n", DEVNAME(sc));
		sc->sc_nextsensor = TAILQ_FIRST(&sc->sc_sensors);
		sc->sc_retries = 0;
		timeout_set(&sc->sc_timeout, esm_refresh, sc);
		timeout_add(&sc->sc_timeout, hz * 10);
	}
}


int
esm_watchdog(void *arg, int period)
{
	struct esm_softc	*sc = arg;
	struct esm_wdog_prop	prop;
	struct esm_wdog_state	state;
	int			s;

	if (sc->sc_wdog_period == period) {
		if (period != 0) {
			s = splsoftclock();
			/* tickle the watchdog */
			EWRITE(sc, ESM2_CTRL_REG, ESM2_TC_HBDB);
			splx(s);
		}
		return (period);
	}

	memset(&prop, 0, sizeof(prop));
	memset(&state, 0, sizeof(state));

	if (period < 10 && period > 0)
		period = 10;

	s = splsoftclock();

	prop.cmd = ESM2_CMD_HWDC;
	prop.subcmd = ESM2_HWDC_WRITE_PROPERTY;
	prop.action = (period == 0) ? ESM_WDOG_DISABLE : ESM_WDOG_RESET;
	prop.time = period;

	if (esm_cmd(sc, &prop, sizeof(prop), NULL, 0, 1) != 0) {
		splx(s);
		return (0);
	}

	state.cmd = ESM2_CMD_HWDC;
	state.subcmd = ESM2_HWDC_WRITE_STATE;
	state.state = (period == 0) ? 0 : 1;

	/* we have the hw, this can't (shouldn't) fail */
	esm_cmd(sc, &state, sizeof(state), NULL, 0, 1);

	splx(s);

	sc->sc_wdog_period = period;
	return (period);
}

void
esm_refresh(void *arg)
{
	struct esm_softc	*sc = arg;
	struct esm_sensor	*es = sc->sc_nextsensor;
	struct esm_smb_req	req;
	struct esm_smb_resp	resp;
	struct esm_smb_resp_val	*val = &resp.resp_val;
	int			i;

	memset(&req, 0, sizeof(req));
	req.h_cmd = ESM2_CMD_SMB_XMIT_RECV;
	req.h_dev = es->es_dev;
	req.h_txlen = sizeof(req.req_val);
	req.h_rxlen = sizeof(resp.resp_val);
	req.req_val.v_cmd = ESM2_SMB_SENSOR_VALUE;
	req.req_val.v_sensor = es->es_id;

	if (esm_smb_cmd(sc, &req, &resp, 0) != 0) {
		if (++sc->sc_retries < 10)
			goto tick;

		switch (es->es_type) {
		case ESM_S_DRIVES:
			for (i = 0; i < 4; i++)
				es->es_sensor[i].flags |= SENSOR_FINVALID;
			break;
		default:
			es->es_sensor->flags |= SENSOR_FINVALID;
			break;
		}

	} else {
		switch (es->es_type) {
		case ESM_S_TEMP:
			es->es_sensor->value = esm_val2temp(val->v_reading);
			es->es_sensor->flags &= ~SENSOR_FINVALID;
			break;
		case ESM_S_VOLTS:
			es->es_sensor->value = esm_val2volts(val->v_reading);
			es->es_sensor->flags &= ~SENSOR_FINVALID;
			break;
		case ESM_S_DRIVES:
			for (i = 0; i < 4; i++) {
				es->es_sensor[i].value =
				    (val->v_reading >> i * 8) & 0xf;
				es->es_sensor[i].flags &= ~SENSOR_FINVALID;
			}
			break;
		default:
			es->es_sensor->value = val->v_reading;
			es->es_sensor->flags &= ~SENSOR_FINVALID;
			break;
		}
	}

	sc->sc_nextsensor = TAILQ_NEXT(es, es_entry);
	sc->sc_retries = 0;

	if (sc->sc_nextsensor == NULL) {
		sc->sc_nextsensor = TAILQ_FIRST(&sc->sc_sensors);
		timeout_add(&sc->sc_timeout, hz * 10);
		return;
	}
tick:
	timeout_add(&sc->sc_timeout, hz / 100);
	
}

int
esm_get_devmap(struct esm_softc *sc, int dev, struct esm_devmap *devmap)
{
	struct esm_devmap_req	req;
	struct esm_devmap_resp	resp;
#ifdef ESM_DEBUG
	int			i;
#endif

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.cmd = ESM2_CMD_DEVICEMAP;
	req.action = ESM2_DEVICEMAP_READ;
	req.index = dev;
	req.ndev = 1;

	if (esm_cmd(sc, &req, sizeof(req), &resp, sizeof(resp), 1) != 0)
		return (1);

	if (resp.status != 0)
		return (1);

	memcpy(devmap, &resp.devmap[0], sizeof(struct esm_devmap));

#ifdef ESM_DEBUG
	if (esmdebug > 5) {
		printf("\n");
		printf("Device Map(%d) returns:\n", dev);
		printf("  status: %.2x\n", resp.status);
		printf("  #devs : %.2x\n", resp.ndev);
		printf("   index: %.2x\n", resp.devmap[0].index);
		printf("   Type : %.2x.%.2x\n", resp.devmap[0].dev_major,
		    resp.devmap[0].dev_minor);
		printf("   Rev  : %.2x.%.2x\n", resp.devmap[0].rev_major,
		    resp.devmap[0].rev_minor);
		printf("   ROM  : %.2x\n", resp.devmap[0].rev_rom);
		printf("   SMB  : %.2x\n", resp.devmap[0].smb_addr);
		printf("   Stat : %.2x\n", resp.devmap[0].status);
		printf("   MonTy: %.2x\n", resp.devmap[0].monitor_type);
		printf("   Poll : %.2x\n", resp.devmap[0].pollcycle);
		printf("   UUID : ");
		for (i = 0; i < ESM2_UUID_LEN; i++) {
			printf("%02x", resp.devmap[0].uniqueid[i]);
		}
		printf("\n");
	}
#endif /* ESM_DEBUG */

	return (0);
}

struct esm_sensor_map esm_sensors_esm2[] = {
	{ ESM_S_UNKNOWN,	0,		"Motherboard" },
	{ ESM_S_TEMP,		0,		"CPU 1" },
	{ ESM_S_TEMP,		0,		"CPU 2" },
	{ ESM_S_TEMP,		0,		"CPU 3" },
	{ ESM_S_TEMP,		0,		"CPU 4" },

	{ ESM_S_TEMP,		0,		"Mainboard" },
	{ ESM_S_TEMP,		0,		"Ambient" },
	{ ESM_S_VOLTS,		0,		"CPU 1 Core" },
	{ ESM_S_VOLTS,		0,		"CPU 2 Core" },
	{ ESM_S_VOLTS,		0,		"CPU 3 Core" },

	{ ESM_S_VOLTS,		0,		"CPU 4 Core" },
	{ ESM_S_VOLTS,		0,		"Motherboard +5V" },
	{ ESM_S_VOLTS,		0,		"Motherboard +12V" },
	{ ESM_S_VOLTS,		0,		"Motherboard +3.3V" },
	{ ESM_S_VOLTS,		0,		"Motherboard +2.5V" },

	{ ESM_S_VOLTS,		0,		"Motherboard GTL Term" },
	{ ESM_S_VOLTS,		0,		"Motherboard Battery" },
	{ ESM_S_INTRUSION,	0,		"Chassis Intrusion", },
	{ ESM_S_UNKNOWN,	0,		"Chassis Fan Ctrl", },
	{ ESM_S_FANRPM,		0,		"Fan 1" },

	{ ESM_S_FANRPM,		0,		"Fan 2" }, /* 20 */
	{ ESM_S_FANRPM,		0,		"Fan 3" },
	{ ESM_S_FANRPM,		0,		"Power Supply Fan" },
	{ ESM_S_VOLTS,		0,		"CPU 1 cache" },
	{ ESM_S_VOLTS,		0,		"CPU 2 cache" },

	{ ESM_S_VOLTS,		0,		"CPU 3 cache" },
	{ ESM_S_VOLTS,		0,		"CPU 4 cache" },
	{ ESM_S_UNKNOWN,	0,		"Power Ctrl" },
	{ ESM_S_PWRSUP,		ESM_A_PWRSUP_1,	"Power Supply 1" },
	{ ESM_S_PWRSUP,		ESM_A_PWRSUP_2,	"Power Supply 2" },

	{ ESM_S_VOLTS,		0,		"Mainboard +1.5V" }, /* 30 */
	{ ESM_S_VOLTS,		0,		"Motherboard +2.8V" },
	{ ESM_S_UNKNOWN,	0,		"HotPlug Status" },
	{ ESM_S_PCISLOT,	0,		"PCI Slot 1" },
	{ ESM_S_PCISLOT,	0,		"PCI Slot 2" },

	{ ESM_S_PCISLOT,	0,		"PCI Slot 3" },
	{ ESM_S_PCISLOT,	0,		"PCI Slot 4" },
	{ ESM_S_PCISLOT,	0,		"PCI Slot 5" },
	{ ESM_S_PCISLOT,	0,		"PCI Slot 6" },
	{ ESM_S_PCISLOT,	0,		"PCI Slot 7" },

	{ ESM_S_VOLTS,		0,		"CPU 1 Cartridge" }, /* 40 */
	{ ESM_S_VOLTS,		0,		"CPU 2 Cartridge" },
	{ ESM_S_VOLTS,		0,		"CPU 3 Cartridge" },
	{ ESM_S_VOLTS,		0,		"CPU 4 Cartridge" },
	{ ESM_S_VOLTS,		0,		"Gigabit NIC +1.8V" },

	{ ESM_S_VOLTS,		0,		"Gigabit NIC +2.5V" },
	{ ESM_S_VOLTS,		0,		"Memory +3.3V" },
	{ ESM_S_VOLTS,		0,		"Video +2.5V" },
	{ ESM_S_PWRSUP,		ESM_A_PWRSUP_3,	"Power Supply 3" },
	{ ESM_S_FANRPM,		0,		"Fan 4" },

	{ ESM_S_FANRPM,		0,		"Power Supply Fan" }, /* 50 */
	{ ESM_S_FANRPM,		0,		"Power Supply Fan" },
	{ ESM_S_FANRPM,		0,		"Power Supply Fan" },
	{ ESM_S_ACSWITCH,	0,		"A/C Power Switch" },
	{ ESM_S_UNKNOWN,	0,		"PS Over Temp" }
};

struct esm_sensor_map esm_sensors_backplane[] = {
	{ ESM_S_UNKNOWN,	0,		"Backplane" },
	{ ESM_S_UNKNOWN,	0,		"Backplane Control" },
	{ ESM_S_TEMP,		0,		"Backplane Top" },
	{ ESM_S_TEMP,		0,		"Backplane Bottom" },
	{ ESM_S_TEMP,		0,		"Backplane Control Panel" },
	{ ESM_S_VOLTS,		0,		"Backplane Battery" },
	{ ESM_S_VOLTS,		0,		"Backplane +5V" },
	{ ESM_S_VOLTS,		0,		"Backplane +12V" },
	{ ESM_S_VOLTS,		0,		"Backplane Board" },
	{ ESM_S_INTRUSION,	0,		"Backplane Intrusion" },
	{ ESM_S_UNKNOWN,	0,		"Backplane Fan Control" },
	{ ESM_S_FANRPM,		0,		"Backplane Fan 1" },
	{ ESM_S_FANRPM,		0,		"Backplane Fan 2" },
	{ ESM_S_FANRPM,		0,		"Backplane Fan 3" },
	{ ESM_S_SCSICONN,	ESM_A_SCSI_A,	"Backplane SCSI A Connected" },
	{ ESM_S_VOLTS,		ESM_A_SCSI_A,	"Backplane SCSI A External" },
	{ ESM_S_VOLTS,		ESM_A_SCSI_A,	"Backplane SCSI A Internal" },
	{ ESM_S_SCSICONN,	ESM_A_SCSI_B,	"Backplane SCSI B Connected" },
	{ ESM_S_VOLTS,		ESM_A_SCSI_B,	"Backplane SCSI B External" },
	{ ESM_S_VOLTS,		ESM_A_SCSI_B,	"Backplane SCSI B Internal" },
	{ ESM_S_DRIVES,		1,		"Drive %d" },
	{ ESM_S_DRIVES,		5,		"Drive %d" },
	{ ESM_S_DRIVE,		0,		"Drive 1"},
	{ ESM_S_DRIVE,		0,		"Drive 2" },
	{ ESM_S_DRIVE,		0,		"Drive 3" },
	{ ESM_S_DRIVE,		0,		"Drive 4" },
	{ ESM_S_DRIVE,		0,		"Drive 5" },
	{ ESM_S_DRIVE,		0,		"Drive 6" },
	{ ESM_S_DRIVE,		0,		"Drive 7" },
	{ ESM_S_DRIVE,		0,		"Drive 8" },
	{ ESM_S_UNKNOWN,	0,		"Backplane Control 2" },
	{ ESM_S_VOLTS,		0,		"Backplane +3.3V" },
};

struct esm_sensor_map esm_sensors_powerunit[] = {
	{ ESM_S_UNKNOWN,	0,		"Power Unit" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_1,	"Power Supply 1 +5V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_1,	"Power Supply 1 +12V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_1,	"Power Supply 1 +3.3V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_1,	"Power Supply 1 -5V" },

	{ ESM_S_VOLTS,		ESM_A_PWRSUP_1,	"Power Supply 1 -12V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_2,	"Power Supply 2 +5V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_2,	"Power Supply 2 +12V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_2,	"Power Supply 2 +3.3V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_2,	"Power Supply 2 -5V" },

	{ ESM_S_VOLTS,		ESM_A_PWRSUP_2,	"Power Supply 2 -12V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_3,	"Power Supply 3 +5V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_3,	"Power Supply 3 +12V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_3,	"Power Supply 3 +3.3V" },
	{ ESM_S_VOLTS,		ESM_A_PWRSUP_3,	"Power Supply 3 -5V" },

	{ ESM_S_VOLTS,		ESM_A_PWRSUP_3,	"Power Supply 3 -12V" },
	{ ESM_S_VOLTS,		0,		"System power supply +5V" },
	{ ESM_S_VOLTS,		0,		"System power supply +3.3V" },
	{ ESM_S_VOLTS,		0,		"System power supply +12V" },
	{ ESM_S_VOLTS,		0,		"System power supply -5V" },

	{ ESM_S_VOLTS,		0,		"System power supply -12V" },
	{ ESM_S_VOLTS,		0,		"System power supply +5V aux" },
	{ ESM_S_AMPS,		ESM_A_PWRSUP_1,	"Power Supply 1 +5V" },
	{ ESM_S_AMPS,		ESM_A_PWRSUP_1,	"Power Supply 1 +12V" },
	{ ESM_S_AMPS,		ESM_A_PWRSUP_1,	"Power Supply 1 +3.3V" },

	{ ESM_S_AMPS,		ESM_A_PWRSUP_2,	"Power Supply 2 +5V" },
	{ ESM_S_AMPS,		ESM_A_PWRSUP_2,	"Power Supply 2 +12V" },
	{ ESM_S_AMPS,		ESM_A_PWRSUP_2,	"Power Supply 2 +3.3V" },
	{ ESM_S_AMPS,		ESM_A_PWRSUP_3,	"Power Supply 3 +5V" },
	{ ESM_S_AMPS,		ESM_A_PWRSUP_3,	"Power Supply 3 +12V" },

	{ ESM_S_AMPS,		ESM_A_PWRSUP_3,	"Power Supply 3 +3.3V" },
	{ ESM_S_FANRPM,		ESM_A_PWRSUP_1,	"Power supply 1 Fan" },
	{ ESM_S_FANRPM,		ESM_A_PWRSUP_2,	"Power supply 2 Fan" },
	{ ESM_S_FANRPM,		ESM_A_PWRSUP_3,	"Power supply 3 Fan" },
	{ ESM_S_PWRSUP,		ESM_A_PWRSUP_1,	"Power Supply 1" },

	{ ESM_S_PWRSUP,		ESM_A_PWRSUP_2,	"Power Supply 2" },
	{ ESM_S_PWRSUP,		ESM_A_PWRSUP_3,	"Power Supply 3" },
	{ ESM_S_UNKNOWN,	0,		"PSPB Fan Control" },
	{ ESM_S_FANRPM,		0,		"Fan 1" },
	{ ESM_S_FANRPM,		0,		"Fan 2" },

	{ ESM_S_FANRPM,		0,		"Fan 3" },
	{ ESM_S_FANRPM,		0,		"Fan 4" },
	{ ESM_S_FANRPM,		0,		"Fan 5" },
	{ ESM_S_FANRPM,		0,		"Fan 6" },
	{ ESM_S_UNKNOWN,	0,		"Fan Enclosure" },
};

void
esm_devmap(struct esm_softc *sc, struct esm_devmap *devmap)
{
	struct esm_sensor_map	*sensor_map;
	const char		*name;
	int			mapsize;

	switch (devmap->dev_major) {
	case ESM2_DEV_ESM2:
		sensor_map = esm_sensors_esm2;

		switch (devmap->dev_minor) {
		case ESM2_DEV_ESM2_2300:
			name = "PowerEdge 2300";
			mapsize = 23;
			break;
		case ESM2_DEV_ESM2_4300:
			name = "PowerEdge 4300";
			mapsize = 27;
			break;
		case ESM2_DEV_ESM2_6300:
			name = "PowerEdge 6300";
			mapsize = 27;
			break;
		case ESM2_DEV_ESM2_6400:
			name = "PowerEdge 6400";
			mapsize = 44;
			break;
		case ESM2_DEV_ESM2_2550:
			name = "PowerEdge 2550";
			mapsize = 48;
			break;
		case ESM2_DEV_ESM2_4350:
			name = "PowerEdge 4350";
			mapsize = 27;
			break;
		case ESM2_DEV_ESM2_6350:
			name = "PowerEdge 6350";
			mapsize = 27;
			break;
		case ESM2_DEV_ESM2_6450:
			name = "PowerEdge 6450";
			mapsize = 44;
			break;
		case ESM2_DEV_ESM2_2400:
			name = "PowerEdge 2400";
			mapsize = 30;
			break;
		case ESM2_DEV_ESM2_4400:
			name = "PowerEdge 4400";
			mapsize = 44;
			break;
		case ESM2_DEV_ESM2_2500:
			name = "PowerEdge 2500";
			mapsize = 55;
			break;
		case ESM2_DEV_ESM2_2450:
			name = "PowerEdge 2450";
			mapsize = 27;
			break;
		case ESM2_DEV_ESM2_2400EX:
			name = "PowerEdge 2400";
			mapsize = 27;
			break;
		case ESM2_DEV_ESM2_2450EX:
			name = "PowerEdge 2450";
			mapsize = 44;
			break;

		default:
			return;
		}

		printf("%s: %s Embedded Server Management %d.%d\n",
		    DEVNAME(sc), name, devmap->rev_major, devmap->rev_minor);
		break;

	case ESM2_DEV_DRACII:
		printf("%s: Dell Remote Assistance Card II %d.%d\n",
		    DEVNAME(sc), devmap->rev_major, devmap->rev_minor);
		return;

	case ESM2_DEV_FRONT_PANEL:
		printf("%s: Front Panel %d.%d\n", DEVNAME(sc),
		    devmap->rev_major, devmap->rev_minor);
		return;

	case ESM2_DEV_BACKPLANE2:
		sensor_map = esm_sensors_backplane;
		mapsize = 22;

		printf("%s: Primary System Backplane %d.%d\n", DEVNAME(sc),
		    devmap->rev_major, devmap->rev_minor);
		break;

	case ESM2_DEV_POWERUNIT2:
		sensor_map = esm_sensors_powerunit;
		mapsize = sizeof(esm_sensors_powerunit) /
		    sizeof(esm_sensors_powerunit[0]);

		printf("%s: Power Unit %d.%d\n", DEVNAME(sc),
		    devmap->rev_major, devmap->rev_minor);
		break;

	case ESM2_DEV_ENCL2_BACKPLANE:
	case ESM2_DEV_ENCL1_BACKPLANE:
		printf("%s: Enclosure Backplane %d.%d\n", DEVNAME(sc),
		    devmap->rev_major, devmap->rev_minor);
		return;

	case ESM2_DEV_ENCL2_POWERUNIT:
	case ESM2_DEV_ENCL1_POWERUNIT:
		printf("%s: Enclosure Powerunit %d.%d\n", DEVNAME(sc),
		    devmap->rev_major, devmap->rev_minor);
		return;

	case ESM2_DEV_HPPCI: /* nfi what this is */
		printf("%s: HPPCI %d.%d\n", DEVNAME(sc),
		    devmap->rev_major, devmap->rev_minor);
		return;

	case ESM2_DEV_BACKPLANE3:
		sensor_map = esm_sensors_backplane;
		mapsize = sizeof(esm_sensors_backplane) /
		    sizeof(esm_sensors_backplane[0]);

		printf("%s: Primary System Backplane %d.%d\n", DEVNAME(sc),
		    devmap->rev_major, devmap->rev_minor);
		break;

	default:
		return;
	}

	esm_make_sensors(sc, devmap, sensor_map, mapsize);
}

enum sensor_type esm_typemap[] = {
	SENSOR_INTEGER,
	SENSOR_INDICATOR,
	SENSOR_TEMP,
	SENSOR_FANRPM,
	SENSOR_VOLTS_DC,
	SENSOR_AMPS,
	SENSOR_INTEGER,
	SENSOR_INTEGER,
	SENSOR_INDICATOR,
	SENSOR_INTEGER,
	SENSOR_INTEGER,
	SENSOR_INTEGER,
	SENSOR_INDICATOR
};

void
esm_make_sensors(struct esm_softc *sc, struct esm_devmap *devmap,
    struct esm_sensor_map *sensor_map, int mapsize)
{
	struct esm_smb_req	req;
	struct esm_smb_resp	resp;
	struct esm_smb_resp_val	*val = &resp.resp_val;
	struct esm_sensor	*es;
	struct sensor		*s;
	int			nsensors, i, j;

	memset(&req, 0, sizeof(req));
	req.h_cmd = ESM2_CMD_SMB_XMIT_RECV;
	req.h_dev = devmap->index;
	req.h_txlen = sizeof(req.req_val);
	req.h_rxlen = sizeof(resp.resp_val);

	req.req_val.v_cmd = ESM2_SMB_SENSOR_VALUE;

	for (i = 0; i < mapsize; i++) {
		req.req_val.v_sensor = i;
		if (esm_smb_cmd(sc, &req, &resp, 1) != 0)
			continue;

		DPRINTFN(1, "%s: dev: 0x%02x sensor: %d (%s) "
		    "reading: 0x%04x status: 0x%02x cksum: 0x%02x\n",
		    DEVNAME(sc), devmap->index, i, sensor_map[i].name,
		    val->v_reading, val->v_status, val->v_checksum);

		if ((val->v_status & ESM2_VS_VALID) != ESM2_VS_VALID)
			continue;
		
		es = malloc(sizeof(struct esm_sensor), M_DEVBUF, M_NOWAIT);
		if (es == NULL)
			return;

		memset(es, 0, sizeof(struct esm_sensor));
		es->es_dev = devmap->index;
		es->es_id = i;
		es->es_type = sensor_map[i].type;

		switch (es->es_type) {
		case ESM_S_DRIVES:
			/* 
			 * this esm sensor represents 4 kernel sensors, so we
			 * go through these hoops to deal with it.
			 */
			nsensors = 4;
			s = malloc(sizeof(struct sensor) * nsensors, M_DEVBUF,
			    M_NOWAIT);
			if (s == NULL) {
				free(es, M_DEVBUF);
				return;
			}
			memset(s, 0, sizeof(struct sensor) * nsensors);

			for (j = 0; j < nsensors; j++) {
				snprintf(s[j].desc, sizeof(s[j].desc),
				    sensor_map[i].name, sensor_map[i].arg + j);
			}
			break;

		default:
			nsensors = 1;
			s = malloc(sizeof(struct sensor), M_DEVBUF, M_NOWAIT);
			if (s == NULL) {
				free(es, M_DEVBUF);
				return;
			}
			memset(s, 0, sizeof(struct sensor));

			strlcpy(s->desc, sensor_map[i].name, sizeof(s->desc));
			break;
		}

		for (j = 0; j < nsensors; j++) {
			s->type = esm_typemap[es->es_type];
			strlcpy(s[j].device, DEVNAME(sc), sizeof(s[j].device));
			SENSOR_ADD(&s[j]);
		}

		es->es_sensor = s;
		TAILQ_INSERT_TAIL(&sc->sc_sensors, es, es_entry);
	}
}


int
esm_bmc_ready(struct esm_softc *sc, int port, u_int8_t mask, u_int8_t val,
    int wait)
{
	unsigned int		count = wait ? 0 : 0xfffff;

	do {
		if ((EREAD(sc, port) & mask) == val)
			return (0);
	} while (count++ < 0xfffff);

	return (1);
}

int
esm_cmd(struct esm_softc *sc, void *cmd, size_t cmdlen, void *resp,
    size_t resplen, int wait)
{
	u_int8_t		*tx = (u_int8_t *)cmd;
	u_int8_t		*rx = (u_int8_t *)resp;
	int			i;

	/* Wait for card ready */
	if (esm_bmc_ready(sc, ESM2_CTRL_REG, ESM2_TC_READY, 0, wait) != 0)
		return (1); /* busy */

	/* Write command data to port */
	ECTRLWR(sc, ESM2_TC_CLR_WPTR);
	for (i = 0; i < cmdlen; i++) {
		DPRINTFN(2, "write: %.2x\n", *tx);
		EDATAWR(sc, *tx);
		tx++;
	}

	/* Ring doorbell and wait */
	ECTRLWR(sc, ESM2_TC_H2ECDB);
	esm_bmc_ready(sc, ESM2_CTRL_REG, ESM2_TC_EC2HDB, ESM2_TC_EC2HDB, 1);

	/* Set host busy semaphore and clear doorbell */
	ECTRLWR(sc, ESM2_TC_HOSTBUSY);
	ECTRLWR(sc, ESM2_TC_EC2HDB);

	/* Read response data from port */
	ECTRLWR(sc, ESM2_TC_CLR_RPTR);
	for (i = 0; i < resplen; i++) {
		*rx = EDATARD(sc);
		DPRINTFN(2, "read = %.2x\n", *rx);
		rx++;
	}

	/* release semaphore */
	ECTRLWR(sc, ESM2_TC_HOSTBUSY);

	return (0);
}

int
esm_smb_cmd(struct esm_softc *sc, struct esm_smb_req *req,
    struct esm_smb_resp *resp, int wait)
{
	memset(resp, 0, sizeof(struct esm_smb_resp));

	if (esm_cmd(sc, req, sizeof(req->hdr) + req->h_txlen, resp,
	    sizeof(resp->hdr) + req->h_rxlen, wait) != 0)
		return (1);

	if (resp->h_status != 0 || resp->h_i2csts != 0) {
		DPRINTFN(3, "%s: dev: 0x%02x error status: 0x%02x "
		    "i2csts: 0x%02x procsts: 0x%02x tx: 0x%02x rx: 0x%02x\n",
		    __func__, req->h_dev, resp->h_status, resp->h_i2csts,
		    resp->h_procsts, resp->h_rx, resp->h_tx);
		return (1);
	}

	return (0);
}

int64_t
esm_val2temp(u_int16_t value)
{
	return (((int64_t)value/10 * 1000000) + 273150000);
}

int64_t
esm_val2volts(u_int16_t value)
{
	return ((int64_t)value * 1000);
}
