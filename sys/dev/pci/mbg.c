/*	$OpenBSD: mbg.c,v 1.2 2006/12/18 07:58:22 mbalmer Exp $ */

/*
 * Copyright (c) 2006 Marc Balmer <mbalmer@openbsd.org>
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
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sensors.h>
#include <sys/syslog.h>
#include <sys/time.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

struct mbg_softc {
	struct device		mbg_dev;
	bus_space_tag_t		iot;
	bus_space_handle_t	ioh;

	struct sensor		timedelta;
	struct sensor		signal;
	u_int8_t		status;
};

struct mbg_time {
	u_int8_t		hundreds;
	u_int8_t		sec;
	u_int8_t		min;
	u_int8_t		hour;
	u_int8_t		mday;
	u_int8_t		wday;
	u_int8_t		mon;
	u_int8_t		year;
	u_int8_t		status;
	u_int8_t		signal;
	int8_t			utc_off;
};

/* mbg_time.status bits */
#define MBG_FREERUN		0x01	/* clock running on xtal */
#define MBG_DST_ENA		0x02	/* DST enabled */
#define MBG_SYNC		0x04	/* clock synced at least once */
#define MBG_DST_CHG		0x08	/* DST change announcement */
#define MBG_UTC			0x10	/* special UTC firmware is installed */
#define MBG_LEAP		0x20	/* announcement of a leap second */
#define MBG_IFTM		0x40	/* current time was set from host */
#define MBG_INVALID		0x80	/* time is invalid */
 
/* AMCC S5933 registers */
#define AMCC_OMB1		0x00	/* outgoing mailbox 1 */
#define AMCC_IMB4		0x1c	/* incoming mailbox 4 */
#define AMCC_FIFO		0x20	/* FIFO register */
#define AMCC_INTCSR		0x38	/* interrupt control/status register */
#define	AMCC_MCSR		0x3c	/* master control/status register */

/* commands */
#define MBG_GET_TIME		0x00
#define MBG_GET_SYNC_TIME	0x02
#define MBG_GET_HR_TIME		0x03
#define MBG_GET_FW_ID_1		0x40
#define MBG_GET_FW_ID_2		0x41
#define MBG_GET_SERNUM		0x42

/* misc. constants */
#define MBG_FIFO_LEN		16
#define MBG_ID_LEN		(2 * MBG_FIFO_LEN + 1)
#define	MBG_BUSY		0x01
#define MBG_SIG_BIAS		55
#define MBG_SIG_MAX		68

int	mbg_probe(struct device *, void *, void *);
void	mbg_attach(struct device *, struct device *, void *);
int	mbg_read(struct mbg_softc *, int cmd, char *buf, size_t len,
    struct timespec *tstamp);
void	mbg_task(void *);

struct cfattach mbg_ca = {
	sizeof(struct mbg_softc), mbg_probe, mbg_attach
};

struct cfdriver mbg_cd = {
	NULL, "mbg", DV_DULL
};

const struct pci_matchid mbg_devices[] = {
	{ PCI_VENDOR_MEINBERG, PCI_PRODUCT_MEINBERG_PCI32 }
};

int
mbg_probe(struct device *parent, void *match, void *aux)
{
	return pci_matchbyid((struct pci_attach_args *)aux, mbg_devices,
	    sizeof(mbg_devices) / sizeof(mbg_devices[0]));
}

void
mbg_attach(struct device *parent, struct device *self, void *aux)
{
	struct mbg_softc *mbg = (struct mbg_softc *)self;
	struct pci_attach_args *const pa = (struct pci_attach_args *)aux;
	struct mbg_time tframe;
	pcireg_t memtype;
	bus_size_t iosize;
	char fw_id[MBG_ID_LEN];

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, PCI_MAPREG_START);
	if (pci_mapreg_map(pa, PCI_MAPREG_START, memtype, 0, &mbg->iot,
	    &mbg->ioh, NULL, &iosize, 0)) {
		printf("\n%s: PCI %s region not found\n",
		    mbg->mbg_dev.dv_xname,
		    memtype == PCI_MAPREG_TYPE_IO ? "I/O" : "memory");
		return;
	}

	if (mbg_read(mbg, MBG_GET_FW_ID_1, fw_id, MBG_FIFO_LEN, NULL) ||
	    mbg_read(mbg, MBG_GET_FW_ID_2, &fw_id[MBG_FIFO_LEN], MBG_FIFO_LEN,
	    NULL))
		printf(": firmware unknown, ", mbg->mbg_dev.dv_xname);
	else {
		fw_id[MBG_ID_LEN - 1] = '\0';
		printf(": firmware %s, ", fw_id);
	}

	if (mbg_read(mbg, MBG_GET_TIME, (char *)&tframe,
	    sizeof(struct mbg_time), NULL)) {
		printf("unknown status\n");
		mbg->status = 0;
	} else {
		if (tframe.status & MBG_FREERUN)
			printf("free running on xtal\n");
		else if (tframe.status & MBG_SYNC)
			printf("synchronised\n");
		else if (tframe.status & MBG_INVALID)
			printf("invalid\n");
		mbg->status = tframe.status;
	}
	strlcpy(mbg->timedelta.device, mbg->mbg_dev.dv_xname,
	    sizeof(mbg->timedelta.device));
	mbg->timedelta.type = SENSOR_TIMEDELTA;
	mbg->timedelta.status = SENSOR_S_UNKNOWN;
	mbg->timedelta.value = 0LL;
	mbg->timedelta.flags = 0;
	strlcpy(mbg->timedelta.desc, "DCF77", sizeof(mbg->timedelta.desc));
	sensor_add(&mbg->timedelta);

	strlcpy(mbg->signal.device, mbg->mbg_dev.dv_xname,
	    sizeof(mbg->signal.device));
	mbg->signal.type = SENSOR_PERCENT;
	mbg->signal.status = SENSOR_S_UNKNOWN;
	mbg->signal.value = 0LL;
	mbg->signal.flags = 0;
	strlcpy(mbg->signal.desc, "Signal strength", sizeof(mbg->signal.desc));
	sensor_add(&mbg->signal);

	sensor_task_register(mbg, mbg_task, 10);
}

void
mbg_task(void *arg)
{
	struct mbg_softc *mbg = (struct mbg_softc *)arg;
	struct mbg_time tframe;
	struct clock_ymdhms ymdhms;
	struct timespec tstamp;
	time_t trecv;
	int signal;

	if (mbg_read(mbg, MBG_GET_TIME, (char *)&tframe, sizeof(tframe),
	    &tstamp)) {
		log(LOG_ERR, "%s: error reading time\n",
		    mbg->mbg_dev.dv_xname);
		return;
	}
	if (tframe.status & MBG_INVALID) {
		log(LOG_INFO, "%s: inavlid time, battery was disconnected\n",
		    mbg->mbg_dev.dv_xname);
		return;
	}
	ymdhms.dt_year = tframe.year + 2000;
	ymdhms.dt_mon = tframe.mon;
	ymdhms.dt_day = tframe.mday;
	ymdhms.dt_hour = tframe.hour;
	ymdhms.dt_min = tframe.min;
	ymdhms.dt_sec = tframe.sec;
	trecv = clock_ymdhms_to_secs(&ymdhms) - tframe.utc_off * 3600;

	mbg->timedelta.value = (int64_t)((tstamp.tv_sec - trecv) * 100
	    - tframe.hundreds) * 10000000LL + tstamp.tv_nsec;
	mbg->timedelta.status = SENSOR_S_OK;
	mbg->timedelta.tv.tv_sec = tstamp.tv_sec;
	mbg->timedelta.tv.tv_usec = tstamp.tv_nsec / 1000;

	signal = tframe.signal - MBG_SIG_BIAS;
	if (signal < 0)
		signal = 0;
	else if (signal > MBG_SIG_MAX)
		signal = MBG_SIG_MAX;

	mbg->signal.value = signal * 100000 / MBG_SIG_MAX;
	mbg->signal.status = SENSOR_S_OK;
	mbg->signal.tv.tv_sec = mbg->timedelta.tv.tv_sec;
	mbg->signal.tv.tv_usec = mbg->timedelta.tv.tv_usec;

	if (tframe.status != mbg->status) {
		if (tframe.status & MBG_SYNC)
			log(LOG_INFO, "%s: clock is synchronized",
			    mbg->mbg_dev.dv_xname);
		else if (tframe.status & MBG_FREERUN)
			log(LOG_INFO, "%s: clock is free running on xtal",
			    mbg->mbg_dev.dv_xname);
		mbg->status = tframe.status;
	}
}

int
mbg_read(struct mbg_softc *mbg, int cmd, char *buf, size_t len,
    struct timespec *tstamp)
{
	long timer, tmax;
	size_t n;
	u_int8_t status;

	/* reset inbound mailbox and clear FIFO status */
	bus_space_write_1(mbg->iot, mbg->ioh, AMCC_MCSR + 3, 0x0c);

	/* set FIFO */
	bus_space_write_1(mbg->iot, mbg->ioh, AMCC_INTCSR + 3, 0x3c);

	/* write the command, optionally taking a timestamp */
	if (tstamp)
		nanotime(tstamp);
	bus_space_write_1(mbg->iot, mbg->ioh, AMCC_OMB1, cmd);

	/* wait for the BUSY flag to go low (approx 70 us on i386) */
	timer = 0;
	tmax = cold ? 50 : hz / 10;
	do {
		if (cold)
			delay(20);
		else
			tsleep(tstamp, 0, "mbg", 1);
		status = bus_space_read_1(mbg->iot, mbg->ioh, AMCC_IMB4 + 3);
	} while ((status & MBG_BUSY) && timer++ < tmax);

	if (status & MBG_BUSY)
		return -1;

	/* read data from the device FIFO */
	for (n = 0; n < len; n++) {
		if (bus_space_read_2(mbg->iot, mbg->ioh, AMCC_MCSR) & 0x20) {
			printf("%s: FIFO error\n", mbg->mbg_dev.dv_xname);
			return -1;
		}
		buf[n] = bus_space_read_1(mbg->iot, mbg->ioh,
		    AMCC_FIFO + (n % 4));
	}
	return 0;
}
