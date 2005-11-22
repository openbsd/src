/*	$OpenBSD: esm.c,v 1.6 2005/11/22 08:33:24 dlg Exp $ */

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

#include <machine/bus.h>

#include <arch/i386/i386/esmreg.h>
#include <arch/i386/i386/esmvar.h>

#define ESM_DEBUG

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

struct esm_sensor_type {
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

	long			es_arg;

	struct sensor		es_sensor;
	TAILQ_ENTRY(esm_sensor)	es_entry;
};

struct esm_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	TAILQ_HEAD(, esm_sensor) sc_sensors;
	struct timeout		sc_timeout;
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

#define ECTRLWR(s, v)	EWRITE((s), ESM2_TC_REG, (v))
#define EDATARD(s)	EREAD((s), ESM2_TBUF_REG)
#define EDATAWR(s, v)	EWRITE((s), ESM2_TBUF_REG, (v))

void		esm_refresh(void *);

int		esm_get_devmap(struct esm_softc *, int, struct esm_devmap *);
void		esm_devmap(struct esm_softc *, struct esm_devmap *);
void		esm_make_sensors(struct esm_softc *, struct esm_devmap *,
		    struct esm_sensor_type *, int);

int		esm_bmc_ready(struct esm_softc *, int, u_int8_t, u_int8_t, int);
int		esm_cmd(struct esm_softc *, void *, size_t, void *, size_t,
		    int);
int		esm_smb_cmd(struct esm_softc *, struct esm_smb_req *,
		    struct esm_smb_resp *, int);

int64_t		esm_val2temp(u_int16_t);
int64_t		esm_val2volts(u_int16_t);

int
esm_match(struct device *parent, void *match, void *aux)
{
	struct esm_attach_args		*eaa = aux;

	if (strncmp(eaa->eaa_name, esm_cd.cd_name,
	    sizeof(esm_cd.cd_name)) == 0)
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
	x = EREAD(sc, ESM2_TIM_REG);
	x &= ~(ESM2_TIM_SCI_EN|ESM2_TIM_SMI_EN|ESM2_TIM_NMI2SMI);
	x |= ESM2_TIM_POWER_UP_BITS;
	EWRITE(sc, ESM2_TIM_REG, x);

	/* clear event doorbells */
	x = EREAD(sc, ESM2_TC_REG);
	x &= ~ESM2_TC_HOSTBUSY;
	x |= ESM2_TC_POWER_UP_BITS;
	EWRITE(sc, ESM2_TC_REG, x);

	/* see if card is alive */
	if (esm_bmc_ready(sc, ESM2_TC_REG, ESM2_TC_ECBUSY, 0, 1) != 0) {
		printf("%s: card is not alive\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, 8);
		return;
	}

	printf(": Watchdog shizz goes here\n");

	for (i = 0; i <= 0xff; i++) {
		if (esm_get_devmap(sc, i, &devmap) != 0)
			break; /* XXX not continue? */
		esm_devmap(sc, &devmap);
	}

	if (!TAILQ_EMPTY(&sc->sc_sensors)) {
		DPRINTF("%s: starting refresh\n", DEVNAME(sc));
		timeout_set(&sc->sc_timeout, esm_refresh, sc);
		timeout_add(&sc->sc_timeout, hz * 10);
	}
}

void
esm_refresh(void *arg)
{
	struct esm_softc	*sc = arg;
	struct esm_sensor	*sensor;
	struct esm_smb_req	req;
	struct esm_smb_resp	resp;
	struct esm_smb_resp_val	*val = &resp.resp_val;

	memset(&req, 0, sizeof(req));
	req.h_cmd = ESM2_CMD_SMB_XMIT_RECV;
	req.h_txlen = sizeof(req.req_val);
	req.h_rxlen = sizeof(resp.resp_val);
	req.req_val.v_cmd = ESM2_SMB_SENSOR_VALUE;

	TAILQ_FOREACH(sensor, &sc->sc_sensors, es_entry) {
		req.h_dev = sensor->es_dev;
		req.req_val.v_sensor = sensor->es_id;

		if (esm_smb_cmd(sc, &req, &resp, 1) != 0) {
			sensor->es_sensor.flags |= SENSOR_FINVALID;
			continue;
		}

		switch (sensor->es_sensor.type) {
		case SENSOR_TEMP:
			sensor->es_sensor.value = esm_val2temp(val->v_reading);
			break;
		case SENSOR_VOLTS_DC:
			sensor->es_sensor.value = esm_val2volts(val->v_reading);
			break;
		default:
			sensor->es_sensor.value = val->v_reading;
			break;
		}
	}

	timeout_add(&sc->sc_timeout, hz * 10);
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

struct esm_sensor_type esm_sensors_esm2[] = {
	{ SENSOR_INTEGER,	0,		"Motherboard" },
	{ SENSOR_TEMP,		0,		"CPU 1" },
	{ SENSOR_TEMP,		0,		"CPU 2" },
	{ SENSOR_TEMP,		0,		"CPU 3" },
	{ SENSOR_TEMP,		0,		"CPU 4" },

	{ SENSOR_TEMP,		0,		"Mainboard" },
	{ SENSOR_TEMP,		0,		"Ambient" },
	{ SENSOR_VOLTS_DC,	0,		"CPU 1 Core" },
	{ SENSOR_VOLTS_DC,	0,		"CPU 2 Core" },
	{ SENSOR_VOLTS_DC,	0,		"CPU 3 Core" },

	{ SENSOR_VOLTS_DC,	0,		"CPU 4 Core" },
	{ SENSOR_VOLTS_DC,	0,		"Motherboard +5V" },
	{ SENSOR_VOLTS_DC,	0,		"Motherboard +12V" },
	{ SENSOR_VOLTS_DC,	0,		"Motherboard +3.3V" },
	{ SENSOR_VOLTS_DC,	0,		"Motherboard +2.5V" },

	{ SENSOR_VOLTS_DC,	0,		"Motherboard GTL Term" },
	{ SENSOR_VOLTS_DC,	0,		"Motherboard Battery" },
	{ SENSOR_INDICATOR,	0,		"Chassis Intrusion", },
	{ SENSOR_INTEGER,	0,		"Chassis Fan Ctrl", },
	{ SENSOR_FANRPM,	0,		"Fan 1" },

	{ SENSOR_FANRPM,	0,		"Fan 2" }, /* 20 */
	{ SENSOR_FANRPM,	0,		"Fan 3" },
	{ SENSOR_FANRPM,	0,		"Power Supply Fan" },
	{ SENSOR_VOLTS_DC,	0,		"CPU 1 cache" },
	{ SENSOR_VOLTS_DC,	0,		"CPU 2 cache" },

	{ SENSOR_VOLTS_DC,	0,		"CPU 3 cache" },
	{ SENSOR_VOLTS_DC,	0,		"CPU 4 cache" },
	{ SENSOR_INTEGER,	0,		"Power Ctrl" },
	{ SENSOR_INTEGER,	ESM_A_PWRSUP_1,	"Power Supply 1" },
	{ SENSOR_INTEGER,	ESM_A_PWRSUP_2,	"Power Supply 2" },

        { SENSOR_VOLTS_DC,	0,		"Mainboard +1.5V" }, /* 30 */
        { SENSOR_VOLTS_DC,	0,		"Motherboard +2.8V" },
        { SENSOR_INTEGER,	0,		"HotPlug Status" },
        { SENSOR_INTEGER,	1,		"PCI Slot 1" },
        { SENSOR_INTEGER,	2,		"PCI Slot 2" },

        { SENSOR_INTEGER,	3,		"PCI Slot 3" },
        { SENSOR_INTEGER,	4,		"PCI Slot 4" },
        { SENSOR_INTEGER,	5,		"PCI Slot 5" },
        { SENSOR_INTEGER,	6,		"PCI Slot 6" },
        { SENSOR_INTEGER,	7,		"PCI Slot 7" },

        { SENSOR_VOLTS_DC,	0,		"CPU 1 Cartridge" }, /* 40 */
        { SENSOR_VOLTS_DC,	0,		"CPU 2 Cartridge" },
        { SENSOR_VOLTS_DC,	0,		"CPU 3 Cartridge" },
        { SENSOR_VOLTS_DC,	0,		"CPU 4 Cartridge" },
        { SENSOR_VOLTS_DC,	0,		"Gigabit NIC +1.8V" },

        { SENSOR_VOLTS_DC,	0,		"Gigabit NIC +2.5V" },
        { SENSOR_VOLTS_DC,	0,		"Memory +3.3V" },
        { SENSOR_VOLTS_DC,	0,		"Video +2.5V" },
        { SENSOR_INTEGER,	ESM_A_PWRSUP_3,	"Power Supply 3" },
        { SENSOR_FANRPM,	0,		"Fan 4" },

        { SENSOR_FANRPM,	0,		"Power Supply Fan" }, /* 50 */
        { SENSOR_FANRPM,	0,		"Power Supply Fan" },
        { SENSOR_FANRPM,	0,		"Power Supply Fan" },
        { SENSOR_INTEGER,	0,		"A/C Power Switch" },
        { SENSOR_INTEGER,	0,		"PS Over Temp" }
};

struct esm_sensor_type esm_sensors_backplane[] = {
	{ SENSOR_INTEGER,	0,		"Backplane" },
	{ SENSOR_INTEGER,	0,		"Backplane Control" },
	{ SENSOR_TEMP,		0,		"Backplane Top" },
	{ SENSOR_TEMP,		0,		"Backplane Bottom" },
	{ SENSOR_TEMP,		0,		"Backplane Control Panel" },
	{ SENSOR_VOLTS_DC,	0,		"Backplane Battery" },
	{ SENSOR_VOLTS_DC,	0,		"Backplane +5V" },
	{ SENSOR_VOLTS_DC,	0,		"Backplane +12V" },
	{ SENSOR_VOLTS_DC,	0,		"Backplane Board" },
	{ SENSOR_INTEGER,	0,		"Backplane Intrusion" },
	{ SENSOR_INTEGER,	0,		"Backplane Fan Control" },
	{ SENSOR_FANRPM,	0,		"Backplane Fan 1" },
	{ SENSOR_FANRPM,	0,		"Backplane Fan 2" },
	{ SENSOR_FANRPM,	0,		"Backplane Fan 3" },
	{ SENSOR_INDICATOR,	ESM_A_SCSI_A,	"Backplane SCSI A Connected" },
	{ SENSOR_VOLTS_DC,	ESM_A_SCSI_A,	"Backplane SCSI A External" },
	{ SENSOR_VOLTS_DC,	ESM_A_SCSI_A,	"Backplane SCSI A Internal" },
	{ SENSOR_INDICATOR,	ESM_A_SCSI_B,	"Backplane SCSI B Connected" },
	{ SENSOR_VOLTS_DC,	ESM_A_SCSI_B,	"Backplane SCSI B External" },
	{ SENSOR_VOLTS_DC,	ESM_A_SCSI_B,	"Backplane SCSI B Internal" },
	{ SENSOR_INTEGER,	0x41,		"Drives 1-4" },
	{ SENSOR_INTEGER,	0x45,		"Drives 5-8" },
	{ SENSOR_INTEGER,	0x11,		"Drive 1"},
	{ SENSOR_INTEGER,	0x12,		"Drive 2" },
	{ SENSOR_INTEGER,	0x13,		"Drive 3" },
	{ SENSOR_INTEGER,	0x14,		"Drive 4" },
	{ SENSOR_INTEGER,	0x15,		"Drive 5" },
	{ SENSOR_INTEGER,	0x16,		"Drive 6" },
	{ SENSOR_INTEGER,	0x17,		"Drive 7" },
	{ SENSOR_INTEGER,	0x18,		"Drive 8" },
	{ SENSOR_INTEGER,	0,		"Backplane Control 2" },
	{ SENSOR_VOLTS_DC,	0,		"Backplane +3.3V" },
};

void
esm_devmap(struct esm_softc *sc, struct esm_devmap *devmap)
{
	struct esm_sensor_type	*sensor_types;
	const char		*maj_name, *min_name;
	int			nsensors;

	switch (devmap->dev_major) {
	case ESM2_DEV_ESM2:
		maj_name = "Embedded Server Management";
		sensor_types = esm_sensors_esm2;

		switch (devmap->dev_minor) {
		case ESM2_DEV_ESM2_2300:
			min_name = "PowerEdge 2300";
			nsensors = 23;
			break;
		case ESM2_DEV_ESM2_4300:
			min_name = "PowerEdge 4300";
			nsensors = 27;
			break;
		case ESM2_DEV_ESM2_6300:
			min_name = "PowerEdge 6300";
			nsensors = 27;
			break;
		case ESM2_DEV_ESM2_6400:
			min_name = "PowerEdge 6400";
			nsensors = 44;
			break;
		case ESM2_DEV_ESM2_2550:
			min_name = "PowerEdge 2550";
			nsensors = 48;
			break;
		case ESM2_DEV_ESM2_4350:
			min_name = "PowerEdge 4350";
			nsensors = 27;
			break;
		case ESM2_DEV_ESM2_6350:
			min_name = "PowerEdge 6350";
			nsensors = 27;
			break;
		case ESM2_DEV_ESM2_6450:
			min_name = "PowerEdge 6450";
			nsensors = 44;
			break;
		case ESM2_DEV_ESM2_2400:
			min_name = "PowerEdge 2400";
			nsensors = 30;
			break;
		case ESM2_DEV_ESM2_4400:
			min_name = "PowerEdge 4400";
			nsensors = 44;
			break;
		case ESM2_DEV_ESM2_2500:
			min_name = "PowerEdge 2500";
			nsensors = 55;
			break;
		case ESM2_DEV_ESM2_2450:
			min_name = "PowerEdge 2450";
			nsensors = 27;
			break;
		case ESM2_DEV_ESM2_2400EX:
			min_name = "PowerEdge 2400";
			nsensors = 27;
			break;
		case ESM2_DEV_ESM2_2450EX:
			min_name = "PowerEdge 2450";
			nsensors = 44;
			break;

		default:
			return;
		}

		printf("%s: %s %s %d.%d\n", DEVNAME(sc), min_name, maj_name,
		    devmap->rev_major, devmap->rev_minor);
		break;

	case ESM2_DEV_BACKPLANE3:
		maj_name = "Primary System Backplane";
		sensor_types = esm_sensors_backplane;
		nsensors = sizeof(esm_sensors_backplane) /
		    sizeof(esm_sensors_backplane[0]);

		printf("%s: %s %d.%d\n", DEVNAME(sc), maj_name,
		    devmap->rev_major, devmap->rev_minor);
		break;

	default:
		return;
	}

	esm_make_sensors(sc, devmap, sensor_types, nsensors);
}

void
esm_make_sensors(struct esm_softc *sc, struct esm_devmap *devmap,
    struct esm_sensor_type *sensor_types, int nsensors)
{
	struct esm_smb_req	req;
	struct esm_smb_resp	resp;
	struct esm_smb_resp_val	*val = &resp.resp_val;
	struct esm_sensor	*sensor;
	int			i;

	memset(&req, 0, sizeof(req));
	req.h_cmd = ESM2_CMD_SMB_XMIT_RECV;
	req.h_dev = devmap->index;
	req.h_txlen = sizeof(req.req_val);
	req.h_rxlen = sizeof(resp.resp_val);

	req.req_val.v_cmd = ESM2_SMB_SENSOR_VALUE;

	for (i = 0; i < nsensors; i++) {
		req.req_val.v_sensor = i;
		if (esm_smb_cmd(sc, &req, &resp, 1) != 0)
			continue;

		DPRINTFN(1, "%s: dev: 0x%02x sensor: %d (%s) "
		    "reading: 0x%04x status: 0x%02x cksum: 0x%02x\n",
		    DEVNAME(sc), devmap->index, i, sensor_types[i].name,
		    val->v_reading, val->v_status, val->v_checksum);

		if ((val->v_status & ESM2_VS_VALID) != ESM2_VS_VALID)
			continue;

		sensor = malloc(sizeof(struct esm_sensor), M_DEVBUF, M_NOWAIT);
		if (sensor == NULL)
			goto error;

		memset(sensor, 0, sizeof(struct esm_sensor));
		sensor->es_dev = devmap->index;
		sensor->es_id = i;
		sensor->es_arg = sensor_types[i].arg;
		sensor->es_sensor.type = sensor_types[i].type;
		strlcpy(sensor->es_sensor.device, DEVNAME(sc),
		    sizeof(sensor->es_sensor.device));
		strlcpy(sensor->es_sensor.desc, sensor_types[i].name,
		    sizeof(sensor->es_sensor.desc));
		TAILQ_INSERT_TAIL(&sc->sc_sensors, sensor, es_entry);
		SENSOR_ADD(&sensor->es_sensor);
	}

	return;

error:
	while (!TAILQ_EMPTY(&sc->sc_sensors)) {
		sensor = TAILQ_FIRST(&sc->sc_sensors);
		TAILQ_REMOVE(&sc->sc_sensors, sensor, es_entry);
		free(sensor, M_DEVBUF);
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
	if (esm_bmc_ready(sc, ESM2_TC_REG, ESM2_TC_READY, 0, 1) != 0)
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
	esm_bmc_ready(sc, ESM2_TC_REG, ESM2_TC_EC2HDB, ESM2_TC_EC2HDB, 1);

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
