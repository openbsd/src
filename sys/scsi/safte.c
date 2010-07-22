/*	$OpenBSD: safte.c,v 1.44 2010/07/22 15:59:47 matthew Exp $ */

/*
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

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/scsiio.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/sensors.h>

#if NBIO > 0
#include <dev/biovar.h>
#endif

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <scsi/safte.h>

#ifdef SAFTE_DEBUG
#define DPRINTF(x)	do { if (safte_debug) printf x ; } while (0)
int	safte_debug = 1;
#else
#define DPRINTF(x)	/* x */
#endif


int	safte_match(struct device *, void *, void *);
void	safte_attach(struct device *, struct device *, void *);
int	safte_detach(struct device *, int);

struct safte_sensor {
	struct ksensor		se_sensor;
	enum {
		SAFTE_T_FAN,
		SAFTE_T_PWRSUP,
		SAFTE_T_DOORLOCK,
		SAFTE_T_ALARM,
		SAFTE_T_TEMP
	}			se_type;
	u_int8_t		*se_field;
};

struct safte_softc {
	struct device		sc_dev;
	struct scsi_link	 *sc_link;
	struct rwlock		sc_lock;

	u_int			sc_encbuflen;
	u_char			*sc_encbuf;

	int			sc_nsensors;
	struct safte_sensor	*sc_sensors;
	struct ksensordev	sc_sensordev;
	struct sensor_task	*sc_sensortask;

	int			sc_celsius;
	int			sc_ntemps;
	struct safte_sensor	*sc_temps;
	u_int8_t		*sc_temperrs;

#if NBIO > 0
	int			sc_nslots;
	u_int8_t		*sc_slots;
#endif
};

struct cfattach safte_ca = {
	sizeof(struct safte_softc), safte_match, safte_attach, safte_detach
};

struct cfdriver safte_cd = {
	NULL, "safte", DV_DULL
};

#define DEVNAME(s)	((s)->sc_dev.dv_xname)

int	safte_read_config(struct safte_softc *);
void	safte_read_encstat(void *);

#if NBIO > 0
int	safte_ioctl(struct device *, u_long, caddr_t);
int	safte_bio_blink(struct safte_softc *, struct bioc_blink *);
#endif

int64_t	safte_temp2uK(u_int8_t, int);

int
safte_match(struct device *parent, void *match, void *aux)
{
	struct scsi_inquiry_data inqbuf;
	struct scsi_attach_args	*sa = aux;
	struct scsi_inquiry_data *inq = sa->sa_inqbuf;
	struct scsi_inquiry *cmd;
	struct scsi_xfer *xs;
	struct safte_inq *si;
	int error, flags = 0, length;

	si = (struct safte_inq *)&inqbuf.extra;

	if (inq == NULL)
		return (0);

	/* match on dell enclosures */
	if ((inq->device & SID_TYPE) == T_PROCESSOR &&
	    SCSISPC(inq->version) == 3)
		return (2);

	if ((inq->device & SID_TYPE) != T_PROCESSOR ||
	    SCSISPC(inq->version) != 2 ||
	    (inq->response_format & SID_ANSII) != 2)
		return (0);

	length = inq->additional_length + SAFTE_EXTRA_OFFSET;
	if (length < SAFTE_INQ_LEN)
		return (0);
	if (length > sizeof(inqbuf))
		length = sizeof(inqbuf);

	if (cold)
		flags |= SCSI_AUTOCONF;
	xs = scsi_xs_get(sa->sa_sc_link, flags | SCSI_DATA_IN);
	if (xs == NULL)
		return (0);
	xs->cmd->opcode = INQUIRY;
	xs->cmdlen = sizeof(*cmd);
	xs->data = (void *)&inqbuf;
	xs->datalen = length;
	xs->retries = 2;
	xs->timeout = 10000;

	cmd = (struct scsi_inquiry *)xs->cmd;
	_lto2b(length, cmd->length);

	memset(&inqbuf, 0, sizeof(inqbuf));
	memset(&inqbuf.extra, ' ', sizeof(inqbuf.extra));

	error = scsi_xs_sync(xs);
	scsi_xs_put(xs);

	if (error)
		return (0);

	if (memcmp(si->ident, SAFTE_IDENT, sizeof(si->ident)) == 0)
		return (2);

	return (0);
}

void
safte_attach(struct device *parent, struct device *self, void *aux)
{
	struct safte_softc		*sc = (struct safte_softc *)self;
	struct scsi_attach_args		*sa = aux;
	int				i = 0;

	sc->sc_link = sa->sa_sc_link;
	sa->sa_sc_link->device_softc = sc;
	rw_init(&sc->sc_lock, DEVNAME(sc));

	printf("\n");

	sc->sc_encbuf = NULL;
	sc->sc_nsensors = 0;
#if NBIO > 0
	sc->sc_nslots = 0;
#endif

	if (safte_read_config(sc) != 0) {
		printf("%s: unable to read enclosure configuration\n",
		    DEVNAME(sc));
		return;
	}

	if (sc->sc_nsensors > 0) {
		sc->sc_sensortask = sensor_task_register(sc,
		    safte_read_encstat, 10);
		if (sc->sc_sensortask == NULL) {
			printf("%s: unable to register update task\n",
			    DEVNAME(sc));
			sc->sc_nsensors = sc->sc_ntemps = 0;
			free(sc->sc_sensors, M_DEVBUF);
		} else {
			for (i = 0; i < sc->sc_nsensors; i++)
				sensor_attach(&sc->sc_sensordev, 
				    &sc->sc_sensors[i].se_sensor);
			sensordev_install(&sc->sc_sensordev);
		}
	}

#if NBIO > 0
	if (sc->sc_nslots > 0 &&
	    bio_register(self, safte_ioctl) != 0) {
		printf("%s: unable to register ioctl with bio\n", DEVNAME(sc));
		sc->sc_nslots = 0;
	} else
		i++;
#endif

	if (i) /* if we're doing something, then preinit encbuf and sensors */
		safte_read_encstat(sc);
	else {
		free(sc->sc_encbuf, M_DEVBUF);
		sc->sc_encbuf = NULL;
	}
}

int
safte_detach(struct device *self, int flags)
{
	struct safte_softc		*sc = (struct safte_softc *)self;
	int				i;

	rw_enter_write(&sc->sc_lock);

#if NBIO > 0
	if (sc->sc_nslots > 0)
		bio_unregister(self);
#endif

	if (sc->sc_nsensors > 0) {
		sensordev_deinstall(&sc->sc_sensordev);
		sensor_task_unregister(sc->sc_sensortask);

		for (i = 0; i < sc->sc_nsensors; i++)
			sensor_detach(&sc->sc_sensordev, 
			    &sc->sc_sensors[i].se_sensor);
		free(sc->sc_sensors, M_DEVBUF);
	}

	if (sc->sc_encbuf != NULL)
		free(sc->sc_encbuf, M_DEVBUF);

	rw_exit_write(&sc->sc_lock);

	return (0);
}

int
safte_read_config(struct safte_softc *sc)
{
	struct safte_config config;
	struct safte_readbuf_cmd *cmd;
	struct safte_sensor *s;
	struct scsi_xfer *xs;
	int error, flags = 0, i, j;

	if (cold)
		flags |= SCSI_AUTOCONF;
	xs = scsi_xs_get(sc->sc_link, flags | SCSI_DATA_IN | SCSI_SILENT);
	if (xs == NULL)
		return (1);
	xs->cmd->opcode = READ_BUFFER;
	xs->cmdlen = sizeof(*cmd);
	xs->data = (void *)&config;
	xs->datalen = sizeof(config);
	xs->retries = 2;
	xs->timeout = 30000;

	cmd = (struct safte_readbuf_cmd *)xs->cmd;
	cmd->flags |= SAFTE_RD_MODE;
	cmd->bufferid = SAFTE_RD_CONFIG;
	cmd->length = htobe16(sizeof(config));

	error = scsi_xs_sync(xs);
	scsi_xs_put(xs);

	if (error != 0)
		return (1);

	DPRINTF(("%s: nfans: %d npwrsup: %d nslots: %d doorlock: %d ntemps: %d"
	    " alarm: %d celsius: %d ntherm: %d\n", DEVNAME(sc), config.nfans,
	    config.npwrsup, config.nslots, config.doorlock, config.ntemps,
	    config.alarm, SAFTE_CFG_CELSIUS(config.therm),
	    SAFTE_CFG_NTHERM(config.therm)));

	sc->sc_encbuflen = config.nfans * sizeof(u_int8_t) + /* fan status */
	    config.npwrsup * sizeof(u_int8_t) + /* power supply status */
	    config.nslots * sizeof(u_int8_t) + /* device scsi id (lun) */
	    sizeof(u_int8_t) + /* door lock status */
	    sizeof(u_int8_t) + /* speaker status */
	    config.ntemps * sizeof(u_int8_t) + /* temp sensors */
	    sizeof(u_int16_t); /* temp out of range sensors */

	sc->sc_encbuf = malloc(sc->sc_encbuflen, M_DEVBUF, M_NOWAIT);
	if (sc->sc_encbuf == NULL)
		return (1);

	sc->sc_nsensors = config.nfans + config.npwrsup + config.ntemps + 
		(config.doorlock ? 1 : 0) + (config.alarm ? 1 : 0);

	sc->sc_sensors = malloc(sc->sc_nsensors * sizeof(struct safte_sensor),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_sensors == NULL) {
		free(sc->sc_encbuf, M_DEVBUF);
		sc->sc_encbuf = NULL;
		sc->sc_nsensors = 0;
		return (1);
	}

	strlcpy(sc->sc_sensordev.xname, DEVNAME(sc),
	    sizeof(sc->sc_sensordev.xname));

	s = sc->sc_sensors;

	for (i = 0; i < config.nfans; i++) {
		s->se_type = SAFTE_T_FAN;
		s->se_field = (u_int8_t *)(sc->sc_encbuf + i);
		s->se_sensor.type = SENSOR_INDICATOR;
		snprintf(s->se_sensor.desc, sizeof(s->se_sensor.desc),
		    "Fan%d", i);

		s++;
	}
	j = config.nfans;

	for (i = 0; i < config.npwrsup; i++) {
		s->se_type = SAFTE_T_PWRSUP;
		s->se_field = (u_int8_t *)(sc->sc_encbuf + j + i);
		s->se_sensor.type = SENSOR_INDICATOR;
		snprintf(s->se_sensor.desc, sizeof(s->se_sensor.desc),
		    "PSU%d", i);

		s++;
	}
	j += config.npwrsup;

#if NBIO > 0
	sc->sc_nslots = config.nslots;
	sc->sc_slots = (u_int8_t *)(sc->sc_encbuf + j);
#endif
	j += config.nslots;

	if (config.doorlock) {
		s->se_type = SAFTE_T_DOORLOCK;
		s->se_field = (u_int8_t *)(sc->sc_encbuf + j);
		s->se_sensor.type = SENSOR_INDICATOR;
		strlcpy(s->se_sensor.desc, "doorlock",
		    sizeof(s->se_sensor.desc));

		s++;
	}
	j++;

	if (config.alarm) {
		s->se_type = SAFTE_T_ALARM;
		s->se_field = (u_int8_t *)(sc->sc_encbuf + j);
		s->se_sensor.type = SENSOR_INDICATOR;
		strlcpy(s->se_sensor.desc, "alarm", sizeof(s->se_sensor.desc));

		s++;
	}
	j++;

	/*
	 * stash the temp info so we can get out of range status. limit the
	 * number so the out of temp checks cant go into memory it doesnt own
	 */
	sc->sc_ntemps = (config.ntemps > 15) ? 15 : config.ntemps;
	sc->sc_temps = s;
	sc->sc_celsius = SAFTE_CFG_CELSIUS(config.therm);
	for (i = 0; i < config.ntemps; i++) {
		s->se_type = SAFTE_T_TEMP;
		s->se_field = (u_int8_t *)(sc->sc_encbuf + j + i);
		s->se_sensor.type = SENSOR_TEMP;

		s++;
	}
	j += config.ntemps;

	sc->sc_temperrs = (u_int8_t *)(sc->sc_encbuf + j);

	return (0);
}

void
safte_read_encstat(void *arg)
{
	struct safte_readbuf_cmd *cmd;
	struct safte_sensor *s;
	struct safte_softc *sc = (struct safte_softc *)arg;
	struct scsi_xfer *xs;
	int error, i, flags = 0;
	u_int16_t oot;

	rw_enter_write(&sc->sc_lock);

	if (cold)
		flags |= SCSI_AUTOCONF;
	xs = scsi_xs_get(sc->sc_link, flags | SCSI_DATA_IN | SCSI_SILENT);
	if (xs == NULL) {
		rw_exit_write(&sc->sc_lock);
		return;
	}
	xs->cmd->opcode = READ_BUFFER;
	xs->cmdlen = sizeof(*cmd);
	xs->data = sc->sc_encbuf;
	xs->datalen = sc->sc_encbuflen;
	xs->retries = 2;
	xs->timeout = 30000;

	cmd = (struct safte_readbuf_cmd *)xs->cmd;
	cmd->flags |= SAFTE_RD_MODE;
	cmd->bufferid = SAFTE_RD_ENCSTAT;
	cmd->length = htobe16(sc->sc_encbuflen);

	error = scsi_xs_sync(xs);
	scsi_xs_put(xs);

	if (error != 0) {
		rw_exit_write(&sc->sc_lock);
		return;
	}

	for (i = 0; i < sc->sc_nsensors; i++) {
		s = &sc->sc_sensors[i];
		s->se_sensor.flags &= ~SENSOR_FUNKNOWN;

		DPRINTF(("%s: %d type: %d field: 0x%02x\n", DEVNAME(sc), i,
		    s->se_type, *s->se_field));

		switch (s->se_type) {
		case SAFTE_T_FAN:
			switch (*s->se_field) {
			case SAFTE_FAN_OP:
				s->se_sensor.value = 1;
				s->se_sensor.status = SENSOR_S_OK;
				break;
			case SAFTE_FAN_MF:
				s->se_sensor.value = 0;
				s->se_sensor.status = SENSOR_S_CRIT;
				break;
			case SAFTE_FAN_NOTINST:
			case SAFTE_FAN_UNKNOWN:
			default:
				s->se_sensor.value = 0;
				s->se_sensor.status = SENSOR_S_UNKNOWN;
				s->se_sensor.flags |= SENSOR_FUNKNOWN;
				break;
			}
			break;

		case SAFTE_T_PWRSUP:
			switch (*s->se_field) {
			case SAFTE_PWR_OP_ON:
				s->se_sensor.value = 1;
				s->se_sensor.status = SENSOR_S_OK;
				break;
			case SAFTE_PWR_OP_OFF:
				s->se_sensor.value = 0;
				s->se_sensor.status = SENSOR_S_OK;
				break;
			case SAFTE_PWR_MF_ON:
				s->se_sensor.value = 1;
				s->se_sensor.status = SENSOR_S_CRIT;
				break;
			case SAFTE_PWR_MF_OFF:
				s->se_sensor.value = 0;
				s->se_sensor.status = SENSOR_S_CRIT;
				break;
			case SAFTE_PWR_NOTINST:
			case SAFTE_PWR_PRESENT:
			case SAFTE_PWR_UNKNOWN:
				s->se_sensor.value = 0;
				s->se_sensor.status = SENSOR_S_UNKNOWN;
				s->se_sensor.flags |= SENSOR_FUNKNOWN;
				break;
			}
			break;

		case SAFTE_T_DOORLOCK:
			switch (*s->se_field) {
			case SAFTE_DOOR_LOCKED:
				s->se_sensor.value = 1;
				s->se_sensor.status = SENSOR_S_OK;
				break;
			case SAFTE_DOOR_UNLOCKED:
				s->se_sensor.value = 0;
				s->se_sensor.status = SENSOR_S_CRIT;
				break;
			case SAFTE_DOOR_UNKNOWN:
				s->se_sensor.value = 0;
				s->se_sensor.status = SENSOR_S_CRIT;
				s->se_sensor.flags |= SENSOR_FUNKNOWN;
				break;
			}
			break;

		case SAFTE_T_ALARM:
			switch (*s->se_field) {
			case SAFTE_SPKR_OFF:
				s->se_sensor.value = 0;
				s->se_sensor.status = SENSOR_S_OK;
				break;
			case SAFTE_SPKR_ON:
				s->se_sensor.value = 1;
				s->se_sensor.status = SENSOR_S_CRIT;
				break;
			}
			break;

		case SAFTE_T_TEMP:
			s->se_sensor.value = safte_temp2uK(*s->se_field,
			    sc->sc_celsius);
			break;
		}
	}

	oot = _2btol(sc->sc_temperrs);
	for (i = 0; i < sc->sc_ntemps; i++)
		sc->sc_temps[i].se_sensor.status = 
		    (oot & (1 << i)) ? SENSOR_S_CRIT : SENSOR_S_OK;

	rw_exit_write(&sc->sc_lock);
}

#if NBIO > 0
int
safte_ioctl(struct device *dev, u_long cmd, caddr_t addr)
{
	struct safte_softc		*sc = (struct safte_softc *)dev;
	int				error = 0;

	switch (cmd) {
	case BIOCBLINK:
		error = safte_bio_blink(sc, (struct bioc_blink *)addr);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

int
safte_bio_blink(struct safte_softc *sc, struct bioc_blink *blink)
{
	struct safte_writebuf_cmd *cmd;
	struct safte_slotop *op;
	struct scsi_xfer *xs;
	int error, slot, flags = 0, wantblink;

	switch (blink->bb_status) {
	case BIOC_SBBLINK:
		wantblink = 1;
		break;
	case BIOC_SBUNBLINK:
		wantblink = 0;
		break;
	default:
		return (EINVAL);
	}

	rw_enter_read(&sc->sc_lock);
	for (slot = 0; slot < sc->sc_nslots; slot++) {
		if (sc->sc_slots[slot] == blink->bb_target)
			break;
	}
	rw_exit_read(&sc->sc_lock);

	if (slot >= sc->sc_nslots)
		return (ENODEV);

	op = malloc(sizeof(*op), M_TEMP, M_ZERO);

	op->opcode = SAFTE_WRITE_SLOTOP;
	op->slot = slot;
	op->flags |= wantblink ? SAFTE_SLOTOP_IDENTIFY : 0;

	if (cold)
		flags |= SCSI_AUTOCONF;
	xs = scsi_xs_get(sc->sc_link, flags | SCSI_DATA_OUT | SCSI_SILENT);
	if (xs == NULL) {
		free(op, M_TEMP);
		return (ENOMEM);
	}
	xs->cmd->opcode = WRITE_BUFFER;
	xs->cmdlen = sizeof(*cmd);
	xs->data = (void *)op;
	xs->datalen = sizeof(*op);
	xs->retries = 2; 
	xs->timeout = 30000;

	cmd = (struct safte_writebuf_cmd *)xs->cmd;
	cmd->flags |= SAFTE_WR_MODE;
	cmd->length = htobe16(sizeof(struct safte_slotop));

	error = scsi_xs_sync(xs);
	scsi_xs_put(xs);

	if (error != 0) {
		error = EIO;
	}
	free(op, M_TEMP);

	return (error);
}
#endif /* NBIO > 0 */

int64_t
safte_temp2uK(u_int8_t measured, int celsius)
{
	int64_t				temp;

	temp = (int64_t)measured;
	temp += SAFTE_TEMP_OFFSET;
	temp *= 1000000; /* convert to micro (mu) degrees */
	if (!celsius)
		temp = ((temp - 32000000) * 5) / 9; /* convert to Celsius */

	temp += 273150000; /* convert to kelvin */

	return (temp);
}
