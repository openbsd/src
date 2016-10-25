/*-
 * Copyright (c) 2009-2016 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * Copyright (c) 2016 Mike Belopuhov <mike@esdenera.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The OpenBSD port was done under funding by Esdenera Networks GmbH.
 */

#include <sys/param.h>

/* Hyperv requires locked atomic operations */
#ifndef MULTIPROCESSOR
#define _HYPERVMPATOMICS
#define MULTIPROCESSOR
#endif
#include <sys/atomic.h>
#ifdef _HYPERVMPATOMICS
#undef MULTIPROCESSOR
#undef _HYPERVMPATOMICS
#endif

#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/timetc.h>
#include <sys/task.h>
#include <sys/syslog.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>

#include <machine/i82489var.h>

#include <dev/rndvar.h>

#include <dev/pv/pvvar.h>
#include <dev/pv/pvreg.h>
#include <dev/pv/hypervreg.h>
#include <dev/pv/hypervvar.h>
#include <dev/pv/hypervicreg.h>

struct hv_ic_dev;

void	hv_heartbeat(void *);
void	hv_kvp_attach(struct hv_ic_dev *);
void	hv_kvp(void *);
int	hv_kvop(void *, int, char *, char *, size_t);
void	hv_shutdown_attach(struct hv_ic_dev *);
void	hv_shutdown(void *);
void	hv_timesync_attach(struct hv_ic_dev *);
void	hv_timesync(void *);

struct hv_ic_dev {
	const char		 *dv_name;
	const struct hv_guid	 *dv_type;
	void			(*dv_attach)(struct hv_ic_dev *);
	void			(*dv_handler)(void *);
	struct hv_channel	 *dv_ch;
	uint8_t			 *dv_buf;
} hv_ic_devs[] = {
	{
		"Heartbeat",
		&hv_guid_heartbeat,
		NULL,
		hv_heartbeat
	},
	{
		"KVP",
		&hv_guid_kvp,
		hv_kvp_attach,
		hv_kvp
	},
	{
		"Shutdown",
		&hv_guid_shutdown,
		hv_shutdown_attach,
		hv_shutdown
	},
	{
		"Timesync",
		&hv_guid_timesync,
		hv_timesync_attach,
		hv_timesync
	}
};

void
hv_attach_icdevs(struct hv_softc *sc)
{
	struct hv_ic_dev *dv;
	struct hv_channel *ch;
	int i, header = 0;

	for (i = 0; i < nitems(hv_ic_devs); i++) {
		dv = &hv_ic_devs[i];

		TAILQ_FOREACH(ch, &sc->sc_channels, ch_entry) {
			if (ch->ch_state != HV_CHANSTATE_OFFERED)
				continue;
			if (ch->ch_flags & CHF_MONITOR)
				continue;
			if (memcmp(dv->dv_type, &ch->ch_type,
			    sizeof(ch->ch_type)) == 0)
				break;
		}
		if (ch == NULL)
			continue;

		dv->dv_ch = ch;

		/*
		 * These services are not performance critical and
		 * do not need batched reading. Furthermore, some
		 * services such as KVP can only handle one message
		 * from the host at a time.
		 */
		dv->dv_ch->ch_flags &= ~CHF_BATCHED;

		dv->dv_buf = malloc(PAGE_SIZE, M_DEVBUF, M_ZERO |
		    (cold ? M_NOWAIT : M_WAITOK));
		if (dv->dv_buf == NULL) {
			printf("%s: failed to allocate ring buffer for %s",
			    sc->sc_dev.dv_xname, dv->dv_name);
			continue;
		}

		if (hv_channel_open(ch, VMBUS_IC_BUFRINGSIZE, NULL, 0,
		    dv->dv_handler, dv)) {
			free(dv->dv_buf, M_DEVBUF, PAGE_SIZE);
			dv->dv_buf = NULL;
			printf("%s: failed to open channel for %s\n",
			    sc->sc_dev.dv_xname, dv->dv_name);
			continue;
		}
		evcount_attach(&ch->ch_evcnt, dv->dv_name, &sc->sc_idtvec);

		if (dv->dv_attach)
			dv->dv_attach(dv);

		if (!header) {
			printf("%s: %s", sc->sc_dev.dv_xname, dv->dv_name);
			header = 1;
		} else
			printf(", %s", dv->dv_name);
	}
	if (header)
		printf("\n");
}

static inline void
hv_ic_negotiate(struct vmbus_icmsg_hdr *hdr)
{
	struct vmbus_icmsg_negotiate *msg;

	msg = (struct vmbus_icmsg_negotiate *)hdr;
	if (msg->ic_fwver_cnt >= 2 &&
	    VMBUS_ICVER_MAJOR(msg->ic_ver[1]) == 3) {
		msg->ic_ver[0] = VMBUS_IC_VERSION(3, 0);
		msg->ic_ver[1] = VMBUS_IC_VERSION(3, 0);
	} else {
		msg->ic_ver[0] = VMBUS_IC_VERSION(1, 0);
		msg->ic_ver[1] = VMBUS_IC_VERSION(1, 0);
	}
	msg->ic_fwver_cnt = 1;
	msg->ic_msgver_cnt = 1;
	hdr->ic_dsize = sizeof(*msg) + 2 * sizeof(uint32_t);
}

void
hv_heartbeat(void *arg)
{
	struct hv_ic_dev *dv = arg;
	struct hv_channel *ch = dv->dv_ch;
	struct hv_softc *sc = ch->ch_sc;
	struct vmbus_icmsg_hdr *hdr;
	struct vmbus_icmsg_heartbeat *msg;
	uint64_t rid;
	uint32_t rlen;
	int rv;

	rv = hv_channel_recv(ch, dv->dv_buf, PAGE_SIZE, &rlen, &rid, 0);
	if (rv || rlen == 0) {
		if (rv != EAGAIN)
			DPRINTF("%s: heartbeat rv=%d rlen=%u\n",
			    sc->sc_dev.dv_xname, rv, rlen);
		return;
	}
	if (rlen < sizeof(struct vmbus_icmsg_hdr)) {
		DPRINTF("%s: heartbeat short read rlen=%u\n",
			    sc->sc_dev.dv_xname, rlen);
		return;
	}
	hdr = (struct vmbus_icmsg_hdr *)dv->dv_buf;
	switch (hdr->ic_type) {
	case VMBUS_ICMSG_TYPE_NEGOTIATE:
		hv_ic_negotiate(hdr);
		break;
	case VMBUS_ICMSG_TYPE_HEARTBEAT:
		msg = (struct vmbus_icmsg_heartbeat *)hdr;
		msg->ic_seq += 1;
		break;
	default:
		printf("%s: unhandled heartbeat message type %u\n",
		    sc->sc_dev.dv_xname, hdr->ic_type);
		return;
	}
	hdr->ic_flags = VMBUS_ICMSG_FLAG_TRANSACTION | VMBUS_ICMSG_FLAG_RESPONSE;
	hv_channel_send(ch, dv->dv_buf, rlen, rid, VMBUS_CHANPKT_TYPE_INBAND, 0);
}

static void
hv_shutdown_task(void *arg)
{
	extern int allowpowerdown;

	if (allowpowerdown == 0)
		return;

	suspend_randomness();

	log(LOG_KERN | LOG_NOTICE, "Shutting down in response to "
	    "request from Hyper-V host\n");
	prsignal(initprocess, SIGUSR2);
}

void
hv_shutdown_attach(struct hv_ic_dev *dv)
{
	struct hv_channel *ch = dv->dv_ch;
	struct hv_softc *sc = ch->ch_sc;

	task_set(&sc->sc_sdtask, hv_shutdown_task, sc);
}

void
hv_shutdown(void *arg)
{
	struct hv_ic_dev *dv = arg;
	struct hv_channel *ch = dv->dv_ch;
	struct hv_softc *sc = ch->ch_sc;
	struct vmbus_icmsg_hdr *hdr;
	struct vmbus_icmsg_shutdown *msg;
	uint64_t rid;
	uint32_t rlen;
	int rv, shutdown = 0;

	rv = hv_channel_recv(ch, dv->dv_buf, PAGE_SIZE, &rlen, &rid, 0);
	if (rv || rlen == 0) {
		if (rv != EAGAIN)
			DPRINTF("%s: shutdown rv=%d rlen=%u\n",
			    sc->sc_dev.dv_xname, rv, rlen);
		return;
	}
	if (rlen < sizeof(struct vmbus_icmsg_hdr)) {
		DPRINTF("%s: shutdown short read rlen=%u\n",
			    sc->sc_dev.dv_xname, rlen);
		return;
	}
	hdr = (struct vmbus_icmsg_hdr *)dv->dv_buf;
	switch (hdr->ic_type) {
	case VMBUS_ICMSG_TYPE_NEGOTIATE:
		hv_ic_negotiate(hdr);
		break;
	case VMBUS_ICMSG_TYPE_SHUTDOWN:
		msg = (struct vmbus_icmsg_shutdown *)hdr;
		if (msg->ic_haltflags == 0 || msg->ic_haltflags == 1) {
			shutdown = 1;
			hdr->ic_status = VMBUS_ICMSG_STATUS_OK;
		} else
			hdr->ic_status = VMBUS_ICMSG_STATUS_FAIL;
		break;
	default:
		printf("%s: unhandled shutdown message type %u\n",
		    sc->sc_dev.dv_xname, hdr->ic_type);
		return;
	}

	hdr->ic_flags = VMBUS_ICMSG_FLAG_TRANSACTION | VMBUS_ICMSG_FLAG_RESPONSE;
	hv_channel_send(ch, dv->dv_buf, rlen, rid, VMBUS_CHANPKT_TYPE_INBAND, 0);

	if (shutdown)
		task_add(systq, &sc->sc_sdtask);
}

void
hv_timesync_attach(struct hv_ic_dev *dv)
{
	struct hv_channel *ch = dv->dv_ch;
	struct hv_softc *sc = ch->ch_sc;

	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	sc->sc_sensor.type = SENSOR_TIMEDELTA;
	sc->sc_sensor.status = SENSOR_S_UNKNOWN;

	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor);
	sensordev_install(&sc->sc_sensordev);
}

void
hv_timesync(void *arg)
{
	struct hv_ic_dev *dv = arg;
	struct hv_channel *ch = dv->dv_ch;
	struct hv_softc *sc = ch->ch_sc;
	struct vmbus_icmsg_hdr *hdr;
	struct vmbus_icmsg_timesync *msg;
	struct timespec guest, host, diff;
	uint64_t tns;
	uint64_t rid;
	uint32_t rlen;
	int rv;

	rv = hv_channel_recv(ch, dv->dv_buf, PAGE_SIZE, &rlen, &rid, 0);
	if (rv || rlen == 0) {
		if (rv != EAGAIN)
			DPRINTF("%s: timesync rv=%d rlen=%u\n",
			    sc->sc_dev.dv_xname, rv, rlen);
		return;
	}
	if (rlen < sizeof(struct vmbus_icmsg_hdr)) {
		DPRINTF("%s: timesync short read rlen=%u\n",
			    sc->sc_dev.dv_xname, rlen);
		return;
	}
	hdr = (struct vmbus_icmsg_hdr *)dv->dv_buf;
	switch (hdr->ic_type) {
	case VMBUS_ICMSG_TYPE_NEGOTIATE:
		hv_ic_negotiate(hdr);
		break;
	case VMBUS_ICMSG_TYPE_TIMESYNC:
		msg = (struct vmbus_icmsg_timesync *)hdr;
		if (msg->ic_tsflags == VMBUS_ICMSG_TS_FLAG_SYNC ||
		    msg->ic_tsflags == VMBUS_ICMSG_TS_FLAG_SAMPLE) {
			microtime(&sc->sc_sensor.tv);
			nanotime(&guest);
			tns = (msg->ic_hvtime - 116444736000000000LL) * 100;
			host.tv_sec = tns / 1000000000LL;
			host.tv_nsec = tns % 1000000000LL;
			timespecsub(&guest, &host, &diff);
			sc->sc_sensor.value = (int64_t)diff.tv_sec *
			    1000000000LL + diff.tv_nsec;
			sc->sc_sensor.status = SENSOR_S_OK;
		}
		break;
	default:
		printf("%s: unhandled timesync message type %u\n",
		    sc->sc_dev.dv_xname, hdr->ic_type);
		return;
	}

	hdr->ic_flags = VMBUS_ICMSG_FLAG_TRANSACTION | VMBUS_ICMSG_FLAG_RESPONSE;
	hv_channel_send(ch, dv->dv_buf, rlen, rid, VMBUS_CHANPKT_TYPE_INBAND, 0);
}

void
hv_kvp_attach(struct hv_ic_dev *dv)
{
	struct hv_channel *ch = dv->dv_ch;
	struct hv_softc *sc = ch->ch_sc;

	sc->sc_pvbus->hv_kvop = hv_kvop;
	sc->sc_pvbus->hv_arg = sc;
}

void
hv_kvp(void *arg)
{
}

int
hv_kvop(void *arg, int op, char *key, char *value, size_t valuelen)
{
	switch (op) {
	case PVBUS_KVWRITE:
	case PVBUS_KVREAD:
	default:
		return (EOPNOTSUPP);
	}
}
