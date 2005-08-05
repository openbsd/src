/*	$OpenBSD: safte.c,v 1.5 2005/08/05 01:06:15 dlg Exp $ */

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/scsiio.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/kthread.h>
#include <sys/sensors.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
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

struct safte_thread;

struct safte_softc {
	struct device	sc_dev;
	struct scsi_link *sc_link;

	u_int		sc_nfans;
	u_int		sc_npwrsup;
	u_int		sc_nslots;
	u_int		sc_ntemps;
	u_int		sc_ntherm;
	int		sc_flags;
#define SAFTE_FL_DOORLOCK	(1<<0)
#define SAFTE_FL_ALARM		(1<<1)
#define SAFTE_FL_CELSIUS	(1<<2)
	size_t		sc_encstatlen;
	u_char		*sc_encbuf;

	int		sc_nsensors;
	struct sensor	*sc_sensors;
	struct safte_thread *sc_thread;
};

struct cfattach safte_ca = {
	sizeof(struct safte_softc), safte_match, safte_attach, safte_detach
};

struct cfdriver safte_cd = {
	NULL, "safte", DV_DULL
};

#define DEVNAME(s)	((s)->sc_dev.dv_xname)

struct safte_thread {
	struct safte_softc *sc;
	volatile int	running;
};

void	safte_create_thread(void *);
void	safte_refresh(void *);
int	safte_read_config(struct safte_softc *);
int	safte_read_encstat(struct safte_softc *);

int64_t	safte_temp2uK(u_int8_t, int);

int
safte_match(struct device *parent, void *match, void *aux)
{
	struct scsibus_attach_args	*sa = aux;
	struct scsi_inquiry_data	*inq = sa->sa_inqbuf;
	struct scsi_inquiry_data	inqbuf;
	struct scsi_inquiry		cmd;
	struct safte_inq		*si = (struct safte_inq *)&inqbuf.extra;

	if (inq == NULL)
		return (0);

	if ((inq->device & SID_TYPE) != T_PROCESSOR ||
	    (inq->version & SID_ANSII) != SID_ANSII_SCSI2 ||
	    (inq->response_format & SID_ANSII) != SID_ANSII_SCSI2)
		return (0);

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = INQUIRY;
	cmd.length = inq->additional_length + SAFTE_EXTRA_OFFSET;
	if (cmd.length > sizeof(inqbuf) || cmd.length < SAFTE_INQ_LEN)
		return(0);

	memset(&inqbuf, 0, sizeof(inqbuf));
	memset(&inqbuf.extra, ' ', sizeof(inqbuf.extra));

	if (scsi_scsi_cmd(sa->sa_sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), (u_char *)&inqbuf, cmd.length, 2, 10000, NULL,
	    SCSI_DATA_IN) != 0)
		return (0);

	if (memcmp(si->ident, SAFTE_IDENT, sizeof(si->ident)) == 0)
		return (24);

	return (0);
}

void
safte_attach(struct device *parent, struct device *self, void *aux)
{
	struct safte_softc		*sc = (struct safte_softc *)self;
	struct scsibus_attach_args	*sa = aux;

	int				i;

	sc->sc_link = sa->sa_sc_link;

	printf("\n");

	sc->sc_thread = malloc(sizeof(struct safte_thread), M_DEVBUF, M_NOWAIT);
	if (sc->sc_thread == NULL) {
		printf("%s: unable to allocate thread information\n",
		    DEVNAME(sc));
		return;
	}

	sc->sc_thread->sc = sc;
	sc->sc_thread->running = 1;

	if (safte_read_config(sc) != 0) {
		free(sc->sc_thread, M_DEVBUF);
		sc->sc_thread = NULL;
		printf("%s: unable to read enclosure configuration\n",
		    DEVNAME(sc));
		return;
	}

	sc->sc_nsensors = sc->sc_ntemps; /* XXX we could do more than temp */
	if (sc->sc_nsensors == 0) {
		free(sc->sc_thread, M_DEVBUF);
		sc->sc_thread = NULL;
		return;
	}

	sc->sc_sensors = malloc(sc->sc_nsensors * sizeof(struct sensor),
	    M_DEVBUF, M_NOWAIT);
	if (sc->sc_sensors == NULL) {
		free(sc->sc_thread, M_DEVBUF);
		sc->sc_thread = NULL;
		printf("%s: unable to allocate sensor storage\n", DEVNAME(sc));
		return;
	}
	memset(sc->sc_sensors, 0, sc->sc_nsensors * sizeof(struct sensor));

	for (i = 0; i < sc->sc_ntemps; i++) {
		sc->sc_sensors[i].type = SENSOR_TEMP;
		snprintf(sc->sc_sensors[i].desc,
		    sizeof(sc->sc_sensors[i].desc), "temp%d", i);
	}

	sc->sc_encstatlen = sc->sc_nfans * sizeof(u_int8_t) + /* fan status */
	    sc->sc_npwrsup * sizeof(u_int8_t) + /* power supply status */
	    sc->sc_nslots * sizeof(u_int8_t) + /* device scsi id (lun) */
	    sizeof(u_int8_t) + /* door lock status */
	    sizeof(u_int8_t) + /* speaker status */
	    sc->sc_ntemps * sizeof(u_int8_t) + /* temp sensors */
	    sizeof(u_int16_t); /* temp out of range sensors */

	sc->sc_encbuf = malloc(sc->sc_encstatlen, M_DEVBUF, M_NOWAIT);
	if (sc->sc_encbuf == NULL) {
		free(sc->sc_sensors, M_DEVBUF);
		free(sc->sc_thread, M_DEVBUF);
		sc->sc_thread = NULL;
		printf("%s: unable to allocate enclosure status buffer\n",
		    DEVNAME(sc));
		return;
	}

	if (safte_read_encstat(sc) != 0) {
		free(sc->sc_encbuf, M_DEVBUF);
		free(sc->sc_sensors, M_DEVBUF);
		free(sc->sc_thread, M_DEVBUF);
		sc->sc_thread = NULL;
		printf("%s: unable to read enclosure status\n", DEVNAME(sc));
		return;
	}

	for (i = 0; i < sc->sc_nsensors; i++) {
		strlcpy(sc->sc_sensors[i].device, DEVNAME(sc),
		    sizeof(sc->sc_sensors[i].device));
		SENSOR_ADD(&sc->sc_sensors[i]);
	}

	kthread_create_deferred(safte_create_thread, sc);
}

int
safte_detach(struct device *self, int flags)
{
	struct safte_softc		*sc = (struct safte_softc *)self;
	int				i;

	if (sc->sc_thread != NULL) {
		sc->sc_thread->running = 0;
		wakeup(sc->sc_thread);
		sc->sc_thread = NULL;

		/*
		 * we can't free the sensors since there is no mechanism to
		 * take them out of the sensor list. mark them invalid instead.
		 */
		for (i = 0; i < sc->sc_nsensors; i++)
			sc->sc_sensors[i].flags |= SENSOR_FINVALID;

		free(sc->sc_encbuf, M_DEVBUF);
	}

	return (0);
}

void
safte_create_thread(void *arg)
{
	struct safte_softc		*sc = arg;

	if (kthread_create(safte_refresh, sc->sc_thread, NULL,
	    DEVNAME(sc)) != 0)
		panic("safte thread");
}

void
safte_refresh(void *arg)
{
	struct safte_thread		*thread = arg;
	struct safte_softc		*sc = thread->sc;
	int				ok = 1;

	while (thread->running) {
		if (safte_read_encstat(sc) != 0) {
			if (ok)
				printf("%s: error getting enclosure status\n",
				    DEVNAME(sc));
			ok = 0;
		} else {
			if (!ok)
				printf("%s: enclosure back online\n",
				    DEVNAME(sc));
			ok = 1;
		}
		tsleep(thread, PWAIT, "timeout", 10 * hz);
	}

	free(thread, M_DEVBUF);

	kthread_exit(0);
}

int
safte_read_config(struct safte_softc *sc)
{
	struct safte_readbuf_cmd	cmd;
	struct safte_config		config;
	int				flags;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = READ_BUFFER;
	cmd.flags |= SAFTE_RD_MODE;
	cmd.bufferid = SAFTE_RD_CONFIG;
	cmd.length = htobe16(sizeof(config));
	flags = SCSI_DATA_IN;
#ifndef SCSIDEBUG
	flags |= SCSI_SILENT;
#endif

	if (scsi_scsi_cmd(sc->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), (u_char *)&config, sizeof(config), 2, 30000, NULL,
	    flags) != 0)
		return (1);

	DPRINTF(("%s: nfans: %d npwrsup: %d nslots: %d doorlock: %d ntemps: %d"
	    " alarm: %d celsius: %d ntherm: %d\n", DEVNAME(sc), config.nfans,
	    config.npwrsup, config.nslots, config.doorlock, config.ntemps,
	    config.alarm, SAFTE_CFG_CELSIUS(config.therm),
	    SAFTE_CFG_NTHERM(config.therm)));

	sc->sc_nfans = config.nfans;
	sc->sc_npwrsup = config.npwrsup;
	sc->sc_nslots = config.nslots;
	sc->sc_ntemps = config.ntemps;
	sc->sc_ntherm = SAFTE_CFG_NTHERM(config.therm);
	sc->sc_flags = (config.doorlock ? SAFTE_FL_DOORLOCK : 0) |
	    (config.alarm ? SAFTE_FL_ALARM : 0) |
	    (SAFTE_CFG_CELSIUS(config.therm) ? SAFTE_FL_CELSIUS : 0);

	return (0);
}

int
safte_read_encstat(struct safte_softc *sc)
{
	struct safte_readbuf_cmd	cmd;
	int				flags, i;
	u_int8_t			*p = sc->sc_encbuf;
	struct sensor			*s = sc->sc_sensors;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = READ_BUFFER;
	cmd.flags |= SAFTE_RD_MODE;
	cmd.bufferid = SAFTE_RD_ENCSTAT;
	cmd.length = htobe16(sc->sc_encstatlen);
	flags = SCSI_DATA_IN;
#ifndef SCSIDEBUG
	flags |= SCSI_SILENT;
#endif

	if (scsi_scsi_cmd(sc->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), sc->sc_encbuf, sc->sc_encstatlen, 2, 30000, NULL,
	    flags) != 0)
		return (1);

	i = 0;
	while (i < sc->sc_nfans) {
		i++;
		p++;
	}

	i = 0;
	while (i < sc->sc_npwrsup) {
		i++;
		p++;
	}

	i = 0;
	while (i < sc->sc_nslots) {
		i++;
		p++;
	}

	/* doorlock */
	p++;
	/* alarm */
	p++;

	i = 0;
	while (i < sc->sc_ntemps) {
		s->value = safte_temp2uK(*p, sc->sc_flags & SAFTE_FL_CELSIUS);
		i++;
		s++;
		p++;
	}

	/* temp over threshold (u_int16_t) */

	return(0);
}

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
