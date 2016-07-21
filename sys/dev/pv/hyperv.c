/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
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

#include <uvm/uvm_extern.h>

#include <machine/i82489var.h>

#include <dev/rndvar.h>

#include <dev/pv/pvvar.h>
#include <dev/pv/pvreg.h>
#include <dev/pv/hypervreg.h>
#include <dev/pv/hypervvar.h>

/* Command submission flags */
#define HCF_SLEEPOK	0x0001	/* M_WAITOK */
#define HCF_NOSLEEP	0x0002	/* M_NOWAIT */
#define HCF_NOREPLY	0x0004

struct hv_softc *hv_sc;

int 	hv_match(struct device *, void *, void *);
void	hv_attach(struct device *, struct device *, void *);
void	hv_deferred(void *);
void	hv_fake_version(struct hv_softc *);
u_int	hv_gettime(struct timecounter *);
int	hv_init_hypercall(struct hv_softc *);
uint64_t hv_hypercall(struct hv_softc *, uint64_t, void *, void *);
int	hv_init_interrupts(struct hv_softc *);
int	hv_init_synic(struct hv_softc *);
int	hv_cmd(struct hv_softc *, void *, size_t, void *, size_t, int);
int	hv_start(struct hv_softc *, struct hv_msg *);
int	hv_reply(struct hv_softc *, struct hv_msg *);
uint16_t hv_intr_signal(struct hv_softc *, void *);
void	hv_intr(void);
void	hv_event_intr(struct hv_softc *);
void	hv_message_intr(struct hv_softc *);
int	hv_vmbus_connect(struct hv_softc *);
void	hv_channel_response(struct hv_softc *, struct hv_channel_msg_header *);
void	hv_channel_offer(struct hv_softc *, struct hv_channel_msg_header *);
void	hv_channel_delivered(struct hv_softc *, struct hv_channel_msg_header *);
int	hv_channel_scan(struct hv_softc *);
void	hv_process_offer(struct hv_softc *, struct hv_offer *);
struct hv_channel *
	hv_channel_lookup(struct hv_softc *, uint32_t);
int	hv_channel_ring_create(struct hv_channel *, uint32_t, uint32_t);
void	hv_channel_ring_destroy(struct hv_channel *);
void	hv_attach_internal(struct hv_softc *);
void	hv_heartbeat(void *);
void	hv_kvp_init(struct hv_channel *);
void	hv_kvp(void *);
int	hv_kvop(void *, int, char *, char *, size_t);
void	hv_shutdown_init(struct hv_channel *);
void	hv_shutdown(void *);
void	hv_timesync_init(struct hv_channel *);
void	hv_timesync(void *);
int	hv_attach_devices(struct hv_softc *);

struct {
	int		  hmd_response;
	int		  hmd_request;
	void		(*hmd_handler)(struct hv_softc *,
			    struct hv_channel_msg_header *);
} hv_msg_dispatch[] = {
	{ HV_CHANMSG_INVALID,			0, NULL },
	{ HV_CHANMSG_OFFER_CHANNEL,		0, hv_channel_offer },
	{ HV_CHANMSG_RESCIND_CHANNEL_OFFER,	0, NULL },
	{ HV_CHANMSG_REQUEST_OFFERS,		HV_CHANMSG_OFFER_CHANNEL,
	  NULL },
	{ HV_CHANMSG_ALL_OFFERS_DELIVERED,	0,
	  hv_channel_delivered },
	{ HV_CHANMSG_OPEN_CHANNEL,		0, NULL },
	{ HV_CHANMSG_OPEN_CHANNEL_RESULT,	HV_CHANMSG_OPEN_CHANNEL,
	  hv_channel_response },
	{ HV_CHANMSG_CLOSE_CHANNEL,		0, NULL },
	{ HV_CHANMSG_GPADL_HEADER,		0, NULL },
	{ HV_CHANMSG_GPADL_BODY,		0, NULL },
	{ HV_CHANMSG_GPADL_CREATED,		HV_CHANMSG_GPADL_HEADER,
	  hv_channel_response },
	{ HV_CHANMSG_GPADL_TEARDOWN,		0, NULL },
	{ HV_CHANMSG_GPADL_TORNDOWN,		HV_CHANMSG_GPADL_TEARDOWN,
	  hv_channel_response },
	{ HV_CHANMSG_REL_ID_RELEASED,		0, NULL },
	{ HV_CHANMSG_INITIATED_CONTACT,		0, NULL },
	{ HV_CHANMSG_VERSION_RESPONSE,		HV_CHANMSG_INITIATED_CONTACT,
	  hv_channel_response },
	{ HV_CHANMSG_UNLOAD,			0, NULL },
};

struct timecounter hv_timecounter = {
	hv_gettime, 0, 0xffffffff, 10000000, "hyperv", 9001
};

struct cfdriver hyperv_cd = {
	NULL, "hyperv", DV_DULL
};

const struct cfattach hyperv_ca = {
	sizeof(struct hv_softc), hv_match, hv_attach
};

int
hv_match(struct device *parent, void *match, void *aux)
{
	struct pv_attach_args *pva = aux;
	struct pvbus_hv *hv = &pva->pva_hv[PVBUS_HYPERV];

	if (hv->hv_base == 0)
		return (0);

	return (1);
}

void
hv_attach(struct device *parent, struct device *self, void *aux)
{
	struct hv_softc *sc = (struct hv_softc *)self;
	struct pv_attach_args *pva = aux;
	struct pvbus_hv *hv = &pva->pva_hv[PVBUS_HYPERV];

	sc->sc_pvbus = hv;
	sc->sc_dmat = pva->pva_dmat;

	printf("\n");

	hv_fake_version(sc);

	tc_init(&hv_timecounter);

	if (hv_init_hypercall(sc))
		return;

	/* Wire it up to the global */
	hv_sc = sc;

	if (hv_init_interrupts(sc))
		return;

	startuphook_establish(hv_deferred, sc);
}

void
hv_deferred(void *arg)
{
	struct hv_softc *sc = arg;

	if (hv_vmbus_connect(sc))
		return;

	if (hv_channel_scan(sc))
		return;

	hv_attach_internal(sc);

	if (hv_attach_devices(sc))
		return;
}

void
hv_fake_version(struct hv_softc *sc)
{
	uint64_t ver;

	/* FreeBSD 10 apparently */
	ver = 0x8200ULL << 48;
	ver |= 10 << 16;
	wrmsr(HV_X64_MSR_GUEST_OS_ID, ver);
}

u_int
hv_gettime(struct timecounter *tc)
{
	u_int now = rdmsr(HV_X64_MSR_TIME_REF_COUNT);

	return (now);
}

int
hv_init_hypercall(struct hv_softc *sc)
{
	extern void *hv_hypercall_page;
	uint64_t msr;
	paddr_t pa;

	sc->sc_hc = &hv_hypercall_page;

	if (!pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_hc, &pa)) {
		printf(": hypercall page PA extraction failed\n");
		return (-1);
	}

	msr = (atop(pa) << HV_X64_MSR_HYPERCALL_PASHIFT) |
	    HV_X64_MSR_HYPERCALL_ENABLED;
	wrmsr(HV_X64_MSR_HYPERCALL, msr);

	if (!(rdmsr(HV_X64_MSR_HYPERCALL) & HV_X64_MSR_HYPERCALL_ENABLED)) {
		printf(": failed to set up a hypercall page\n");
		return (-1);
	}

	return (0);
}

uint64_t
hv_hypercall(struct hv_softc *sc, uint64_t control, void *input,
    void *output)
{
	paddr_t input_pa = 0, output_pa = 0;
	uint64_t status = 0;

	if (input != NULL &&
	    pmap_extract(pmap_kernel(), (vaddr_t)input, &input_pa) == 0) {
		printf("%s: hypercall input PA extraction failed\n",
		    sc->sc_dev.dv_xname);
		return (~HV_STATUS_SUCCESS);
	}

	if (output != NULL &&
	    pmap_extract(pmap_kernel(), (vaddr_t)output, &output_pa) == 0) {
		printf("%s: hypercall output PA extraction failed\n",
		    sc->sc_dev.dv_xname);
		return (~HV_STATUS_SUCCESS);
	}

#ifdef __amd64__
	__asm__ __volatile__ ("mov %0, %%r8" : : "r" (output_pa) : "r8");
	__asm__ __volatile__ ("call *%3" : "=a" (status) : "c" (control),
	    "d" (input_pa), "m" (sc->sc_hc));
#else  /* __i386__ */
	{
		uint32_t control_hi = control >> 32;
		uint32_t control_lo = control & 0xfffffffff;
		uint32_t status_hi = 1;
		uint32_t status_lo = 1;

		__asm__ __volatile__ ("call *%8" :
		    "=d" (status_hi), "=a"(status_lo) :
		    "d" (control_hi), "a" (control_lo),
		    "b" (0), "c" (input_pa), "D" (0), "S" (output_pa),
		    "m" (sc->sc_hc));

		status = status_lo | ((uint64_t)status_hi << 32);
	}
#endif	/* __amd64__ */

	return (status);
}

int
hv_init_interrupts(struct hv_softc *sc)
{
	struct cpu_info *ci = curcpu();
	int cpu = CPU_INFO_UNIT(ci);

	sc->sc_idtvec = LAPIC_HYPERV_VECTOR;

	TAILQ_INIT(&sc->sc_reqs);
	mtx_init(&sc->sc_reqlck, IPL_NET);

	TAILQ_INIT(&sc->sc_rsps);
	mtx_init(&sc->sc_rsplck, IPL_NET);

	sc->sc_simp[cpu] = km_alloc(PAGE_SIZE, &kv_any, &kp_zero, &kd_nowait);
	if (sc->sc_simp[cpu] == NULL) {
		printf(": failed to allocate SIMP\n");
		return (-1);
	}

	sc->sc_siep[cpu] = km_alloc(PAGE_SIZE, &kv_any, &kp_zero, &kd_nowait);
	if (sc->sc_siep[cpu] == NULL) {
		printf(": failed to allocate SIEP\n");
		km_free(sc->sc_simp[cpu], PAGE_SIZE, &kv_any, &kp_zero);
		return (-1);
	}

	sc->sc_proto = HV_VMBUS_VERSION_WS2008;

	return (hv_init_synic(sc));
}

int
hv_init_synic(struct hv_softc *sc)
{
	struct cpu_info *ci = curcpu();
	int cpu = CPU_INFO_UNIT(ci);
	uint64_t simp, siefp, sctrl, sint, version;
	paddr_t pa;

	version = rdmsr(HV_X64_MSR_SVERSION);

	/*
	 * Setup the Synic's message page
	 */
	if (!pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_simp[cpu], &pa)) {
		printf(": SIMP PA extraction failed\n");
		return (-1);
	}
	simp = rdmsr(HV_X64_MSR_SIMP);
	simp &= (1 << HV_X64_MSR_SIMP_PASHIFT) - 1;
	simp |= (atop(pa) << HV_X64_MSR_SIMP_PASHIFT);
	simp |= HV_X64_MSR_SIMP_ENABLED;
	wrmsr(HV_X64_MSR_SIMP, simp);

	/*
	 * Setup the Synic's event page
	 */
	if (!pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_siep[cpu], &pa)) {
		printf(": SIEP PA extraction failed\n");
		return (-1);
	}
	siefp = rdmsr(HV_X64_MSR_SIEFP);
	siefp &= (1<<HV_X64_MSR_SIEFP_PASHIFT) - 1;
	siefp |= (atop(pa) << HV_X64_MSR_SIEFP_PASHIFT);
	siefp |= HV_X64_MSR_SIEFP_ENABLED;
	wrmsr(HV_X64_MSR_SIEFP, siefp);

	/* HV_SHARED_SINT_IDT_VECTOR + 0x20 */
	sint = sc->sc_idtvec | HV_X64_MSR_SINT_AUTOEOI;
	wrmsr(HV_X64_MSR_SINT0 + HV_MESSAGE_SINT, sint);

	/* Enable the global synic bit */
	sctrl = rdmsr(HV_X64_MSR_SCONTROL);
	sctrl |= HV_X64_MSR_SCONTROL_ENABLED;
	wrmsr(HV_X64_MSR_SCONTROL, sctrl);

	sc->sc_vcpus[cpu] = rdmsr(HV_X64_MSR_VP_INDEX);

	DPRINTF("vcpu%u: SIMP %#llx SIEFP %#llx SCTRL %#llx\n",
	    sc->sc_vcpus[cpu], simp, siefp, sctrl);

	return (0);
}

int
hv_cmd(struct hv_softc *sc, void *cmd, size_t cmdlen, void *rsp,
    size_t rsplen, int flags)
{
	struct hv_msg msg;
	int rv;

	if (cmdlen > HV_MESSAGE_PAYLOAD) {
		printf("%s: payload too large (%ld)\n", sc->sc_dev.dv_xname,
		    cmdlen);
		return (EMSGSIZE);
	}

	memset(&msg, 0, sizeof(msg));

	msg.msg_req.payload_size = cmdlen;
	memcpy(msg.msg_req.payload, cmd, cmdlen);

	if (!(flags & HCF_NOREPLY)) {
		msg.msg_rsp = rsp;
		msg.msg_rsplen = rsplen;
	} else
		msg.msg_flags |= MSGF_NOQUEUE;

	if (flags & HCF_NOSLEEP)
		msg.msg_flags |= MSGF_NOSLEEP;

	if ((rv = hv_start(sc, &msg)) != 0)
		return (rv);
	return (hv_reply(sc, &msg));
}

int
hv_start(struct hv_softc *sc, struct hv_msg *msg)
{
	const int delays[] = { 100, 100, 100, 500, 500, 5000, 5000, 5000 };
	const char *wchan = "hvstart";
	uint16_t status;
	int i, s;

	msg->msg_req.connection_id = HV_MESSAGE_CONNECTION_ID;
	msg->msg_req.message_type = 1;

	if (!(msg->msg_flags & MSGF_NOQUEUE)) {
		mtx_enter(&sc->sc_reqlck);
		TAILQ_INSERT_TAIL(&sc->sc_reqs, msg, msg_entry);
		mtx_leave(&sc->sc_reqlck);
	}

	for (i = 0; i < nitems(delays); i++) {
		status = hv_hypercall(sc, HV_CALL_POST_MESSAGE,
		    &msg->msg_req, NULL);
		if (status != HV_STATUS_INSUFFICIENT_BUFFERS)
			break;
		if (msg->msg_flags & MSGF_NOSLEEP) {
			delay(delays[i]);
			s = splnet();
			hv_intr();
			splx(s);
		} else
			tsleep(wchan, PRIBIO, wchan, 1);
	}
	if (status != 0) {
		printf("%s: posting vmbus message failed with %d\n",
		    sc->sc_dev.dv_xname, status);
		if (!(msg->msg_flags & MSGF_NOQUEUE)) {
			mtx_enter(&sc->sc_reqlck);
			TAILQ_REMOVE(&sc->sc_reqs, msg, msg_entry);
			mtx_leave(&sc->sc_reqlck);
		}
		return (EIO);
	}

	return (0);
}

int
hv_reply(struct hv_softc *sc, struct hv_msg *msg)
{
	const char *wchan = "hvreply";
	struct hv_msg *m, *tmp;
	int i, s;

	if (msg->msg_flags & MSGF_NOQUEUE)
		return (0);

	for (i = 0; i < 1000; i++) {
		mtx_enter(&sc->sc_rsplck);
		TAILQ_FOREACH_SAFE(m, &sc->sc_rsps, msg_entry, tmp) {
			if (m == msg) {
				TAILQ_REMOVE(&sc->sc_rsps, m, msg_entry);
				break;
			}
		}
		mtx_leave(&sc->sc_rsplck);
		if (m != NULL)
			return (0);
		if (msg->msg_flags & MSGF_NOSLEEP) {
			delay(100000);
			s = splnet();
			hv_intr();
			splx(s);
		} else {
			s = tsleep(&msg, PRIBIO | PCATCH, wchan, 1);
			if (s != EWOULDBLOCK)
				return (EINTR);
		}
	}
	mtx_enter(&sc->sc_rsplck);
	TAILQ_FOREACH_SAFE(m, &sc->sc_reqs, msg_entry, tmp) {
		if (m == msg) {
			TAILQ_REMOVE(&sc->sc_reqs, m, msg_entry);
			break;
		}
	}
	mtx_leave(&sc->sc_rsplck);
	return (ETIMEDOUT);
}

uint16_t
hv_intr_signal(struct hv_softc *sc, void *con)
{
	uint64_t status;

	status = hv_hypercall(sc, HV_CALL_SIGNAL_EVENT, con, NULL);
	return ((uint16_t)status);
}

void
hv_intr(void)
{
	struct hv_softc *sc = hv_sc;

	hv_event_intr(sc);
	hv_message_intr(sc);
}

void
hv_event_intr(struct hv_softc *sc)
{
	struct hv_synic_event_flags *evt;
	struct cpu_info *ci = curcpu();
	int cpu = CPU_INFO_UNIT(ci);
	int bit, dword, maxdword, relid;
	struct hv_channel *ch;
	uint32_t *revents;

	evt = (struct hv_synic_event_flags *)sc->sc_siep[cpu] + HV_MESSAGE_SINT;
	if ((sc->sc_proto == HV_VMBUS_VERSION_WS2008) ||
	    (sc->sc_proto == HV_VMBUS_VERSION_WIN7)) {
		if (atomic_clearbit_ptr(&evt->flags[0], 0) == 0)
			return;
		maxdword = HV_MAX_NUM_CHANNELS_SUPPORTED >> 5;
		/*
		 * receive size is 1/2 page and divide that by 4 bytes
		 */
		revents = sc->sc_revents;
	} else {
		maxdword = nitems(evt->flags);
		/*
		 * On Host with Win8 or above, the event page can be
		 * checked directly to get the id of the channel
		 * that has the pending interrupt.
		 */
		revents = &evt->flags[0];
	}

	for (dword = 0; dword < maxdword; dword++) {
		if (revents[dword] == 0)
			continue;
		for (bit = 0; bit < 32; bit++) {
			if (!atomic_clearbit_ptr(&revents[dword], bit))
				continue;
			relid = (dword << 5) + bit;
			/* vmbus channel protocol message */
			if (relid == 0)
				continue;
			ch = hv_channel_lookup(sc, relid);
			if (ch == NULL) {
				printf("%s: unhandled event on %u\n",
				    sc->sc_dev.dv_xname, relid);
				continue;
			}
			if (ch->ch_state != HV_CHANSTATE_OPENED) {
				printf("%s: channel %u is not active\n",
				    sc->sc_dev.dv_xname, relid);
				continue;
			}
			ch->ch_evcnt.ec_count++;
			if (ch->ch_handler)
				ch->ch_handler(ch->ch_ctx);
		}
	}
}

void
hv_message_intr(struct hv_softc *sc)
{
	struct hv_vmbus_message *msg;
	struct hv_channel_msg_header *hdr;
	struct cpu_info *ci = curcpu();
	int cpu = CPU_INFO_UNIT(ci);

	for (;;) {
		msg = (struct hv_vmbus_message *)sc->sc_simp[cpu] +
		    HV_MESSAGE_SINT;
		if (msg->header.message_type == HV_MESSAGE_TYPE_NONE)
			break;

		hdr = (struct hv_channel_msg_header *)msg->payload;
		if (hdr->message_type >= HV_CHANMSG_COUNT) {
			printf("%s: unhandled message type %d flags %#x\n",
			    sc->sc_dev.dv_xname, hdr->message_type,
			    msg->header.message_flags);
			goto skip;
		}
		if (hv_msg_dispatch[hdr->message_type].hmd_handler)
			hv_msg_dispatch[hdr->message_type].hmd_handler(sc, hdr);
		else
			printf("%s: unhandled message type %d\n",
			    sc->sc_dev.dv_xname, hdr->message_type);
 skip:
		msg->header.message_type = HV_MESSAGE_TYPE_NONE;
		membar_sync();
		if (msg->header.message_flags & HV_SYNIC_MHF_PENDING)
			wrmsr(HV_X64_MSR_EOM, 0);
	}
}

void
hv_channel_response(struct hv_softc *sc, struct hv_channel_msg_header *rsphdr)
{
	struct hv_msg *msg, *tmp;
	struct hv_channel_msg_header *reqhdr;
	int req;

	req = hv_msg_dispatch[rsphdr->message_type].hmd_request;
	mtx_enter(&sc->sc_reqlck);
	TAILQ_FOREACH_SAFE(msg, &sc->sc_reqs, msg_entry, tmp) {
		reqhdr = (struct hv_channel_msg_header *)&msg->msg_req.payload;
		if (reqhdr->message_type == req) {
			TAILQ_REMOVE(&sc->sc_reqs, msg, msg_entry);
			break;
		}
	}
	mtx_leave(&sc->sc_reqlck);
	if (msg != NULL) {
		memcpy(msg->msg_rsp, rsphdr, msg->msg_rsplen);
		mtx_enter(&sc->sc_rsplck);
		TAILQ_INSERT_TAIL(&sc->sc_rsps, msg, msg_entry);
		mtx_leave(&sc->sc_rsplck);
		wakeup(msg);
	}
}

void
hv_channel_offer(struct hv_softc *sc, struct hv_channel_msg_header *hdr)
{
	struct hv_offer *co;

	co = malloc(sizeof(*co), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (co == NULL) {
		printf("%s: failed to allocate an offer object\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	memcpy(&co->co_chan, hdr, sizeof(co->co_chan));

	mtx_enter(&sc->sc_offerlck);
	SIMPLEQ_INSERT_TAIL(&sc->sc_offers, co, co_entry);
	mtx_leave(&sc->sc_offerlck);
}

void
hv_channel_delivered(struct hv_softc *sc, struct hv_channel_msg_header *hdr)
{
	atomic_setbits_int(&sc->sc_flags, HSF_OFFERS_DELIVERED);
	wakeup(hdr);
}

int
hv_vmbus_connect(struct hv_softc *sc)
{
	const uint32_t versions[] = { HV_VMBUS_VERSION_WIN8_1,
	    HV_VMBUS_VERSION_WIN8, HV_VMBUS_VERSION_WIN7,
	    HV_VMBUS_VERSION_WS2008, HV_VMBUS_VERSION_INVALID
	};
	struct hv_channel_initiate_contact cmd;
	struct hv_channel_version_response rsp;
	paddr_t epa, mpa1, mpa2;
	int i;

	sc->sc_events = km_alloc(PAGE_SIZE, &kv_any, &kp_zero, &kd_nowait);
	if (sc->sc_events == NULL) {
		printf(": failed to allocate channel port events page\n");
		goto errout;
	}
	if (!pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_events, &epa)) {
		printf(": channel port events page PA extraction failed\n");
		goto errout;
	}

	sc->sc_wevents = (uint32_t *)sc->sc_events;
	sc->sc_revents = (uint32_t *)sc->sc_events + (PAGE_SIZE >> 1);

	sc->sc_monitor[0] = km_alloc(PAGE_SIZE, &kv_any, &kp_zero, &kd_nowait);
	if (sc->sc_monitor == NULL) {
		printf(": failed to allocate monitor page 1\n");
		goto errout;
	}
	if (!pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_monitor[0], &mpa1)) {
		printf(": monitor page 1 PA extraction failed\n");
		goto errout;
	}

	sc->sc_monitor[1] = km_alloc(PAGE_SIZE, &kv_any, &kp_zero, &kd_nowait);
	if (sc->sc_monitor == NULL) {
		printf(": failed to allocate monitor page 2\n");
		goto errout;
	}
	if (!pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_monitor[1], &mpa2)) {
		printf(": monitor page 2 PA extraction failed\n");
		goto errout;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.hdr.message_type = HV_CHANMSG_INITIATED_CONTACT;
	cmd.interrupt_page = (uint64_t)epa;
	cmd.monitor_page_1 = (uint64_t)mpa1;
	cmd.monitor_page_2 = (uint64_t)mpa2;

	memset(&rsp, 0, sizeof(rsp));

	for (i = 0; versions[i] != HV_VMBUS_VERSION_INVALID; i++) {
		cmd.vmbus_version_requested = versions[i];
		if (hv_cmd(sc, &cmd, sizeof(cmd), &rsp, sizeof(rsp),
		    HCF_NOSLEEP)) {
			DPRINTF("%s: INITIATED_CONTACT failed\n",
			    sc->sc_dev.dv_xname);
			goto errout;
		}
		if (rsp.version_supported) {
			sc->sc_flags |= HSF_CONNECTED;
			sc->sc_proto = versions[i];
			sc->sc_handle = 0xe1e10 - 1; /* magic! */
			DPRINTF("%s: protocol version %#x\n",
			    sc->sc_dev.dv_xname, versions[i]);
			break;
		}
	}
	if (versions[i] == HV_VMBUS_VERSION_INVALID) {
		printf("%s: failed to negotiate protocol version\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}

	return (0);

 errout:
	if (sc->sc_events) {
		km_free(sc->sc_events, PAGE_SIZE, &kv_any, &kp_zero);
		sc->sc_events = NULL;
		sc->sc_wevents = NULL;
		sc->sc_revents = NULL;
	}
	if (sc->sc_monitor[0]) {
		km_free(sc->sc_monitor[0], PAGE_SIZE, &kv_any, &kp_zero);
		sc->sc_monitor[0] = NULL;
	}
	if (sc->sc_monitor[1]) {
		km_free(sc->sc_monitor[1], PAGE_SIZE, &kv_any, &kp_zero);
		sc->sc_monitor[1] = NULL;
	}
	return (-1);
}

const struct hv_guid hv_guid_network = {
	{ 0x63, 0x51, 0x61, 0xf8, 0x3e, 0xdf, 0xc5, 0x46,
	  0x91, 0x3f, 0xf2, 0xd2, 0xf9, 0x65, 0xed, 0x0e }
};

const struct hv_guid hv_guid_ide = {
	{ 0x32, 0x26, 0x41, 0x32, 0xcb, 0x86, 0xa2, 0x44,
	  0x9b, 0x5c, 0x50, 0xd1, 0x41, 0x73, 0x54, 0xf5 }
};

const struct hv_guid hv_guid_scsi = {
	{ 0xd9, 0x63, 0x61, 0xba, 0xa1, 0x04, 0x29, 0x4d,
	  0xb6, 0x05, 0x72, 0xe2, 0xff, 0xb1, 0xdc, 0x7f }
};

const struct hv_guid hv_guid_shutdown = {
	{ 0x31, 0x60, 0x0b, 0x0e, 0x13, 0x52, 0x34, 0x49,
	  0x81, 0x8b, 0x38, 0xd9, 0x0c, 0xed, 0x39, 0xdb }
};

const struct hv_guid hv_guid_timesync = {
	{ 0x30, 0xe6, 0x27, 0x95, 0xae, 0xd0, 0x7b, 0x49,
	  0xad, 0xce, 0xe8, 0x0a, 0xb0, 0x17, 0x5c, 0xaf }
};

const struct hv_guid hv_guid_heartbeat = {
	{ 0x39, 0x4f, 0x16, 0x57, 0x15, 0x91, 0x78, 0x4e,
	  0xab, 0x55, 0x38, 0x2f, 0x3b, 0xd5, 0x42, 0x2d }
};

const struct hv_guid hv_guid_kvp = {
	{ 0xe7, 0xf4, 0xa0, 0xa9, 0x45, 0x5a, 0x96, 0x4d,
	  0xb8, 0x27, 0x8a, 0x84, 0x1e, 0x8c, 0x03, 0xe6 }
};

#ifdef HYPERV_DEBUG
const struct hv_guid hv_guid_vss = {
	{ 0x29, 0x2e, 0xfa, 0x35, 0x23, 0xea, 0x36, 0x42,
	  0x96, 0xae, 0x3a, 0x6e, 0xba, 0xcb, 0xa4, 0x40 }
};

const struct hv_guid hv_guid_dynmem = {
	{ 0xdc, 0x74, 0x50, 0x52, 0x85, 0x89, 0xe2, 0x46,
	  0x80, 0x57, 0xa3, 0x07, 0xdc, 0x18, 0xa5, 0x02 }
};

const struct hv_guid hv_guid_mouse = {
	{ 0x9e, 0xb6, 0xa8, 0xcf, 0x4a, 0x5b, 0xc0, 0x4c,
	  0xb9, 0x8b, 0x8b, 0xa1, 0xa1, 0xf3, 0xf9, 0x5a }
};

const struct hv_guid hv_guid_kbd = {
	{ 0x6d, 0xad, 0x12, 0xf9, 0x17, 0x2b, 0xea, 0x48,
	  0xbd, 0x65, 0xf9, 0x27, 0xa6, 0x1c, 0x76, 0x84 }
};

const struct hv_guid hv_guid_video = {
	{ 0x02, 0x78, 0x0a, 0xda, 0x77, 0xe3, 0xac, 0x4a,
	  0x8e, 0x77, 0x05, 0x58, 0xeb, 0x10, 0x73, 0xf8 }
};

const struct hv_guid hv_guid_fc = {
	{ 0x4a, 0xcc, 0x9b, 0x2f, 0x69, 0x00, 0xf3, 0x4a,
	  0xb7, 0x6b, 0x6f, 0xd0, 0xbe, 0x52, 0x8c, 0xda }
};

const struct hv_guid hv_guid_fcopy = {
	{ 0xe3, 0x4b, 0xd1, 0x34, 0xe4, 0xde, 0xc8, 0x41,
	  0x9a, 0xe7, 0x6b, 0x17, 0x49, 0x77, 0xc1, 0x92 }
};

const struct hv_guid hv_guid_pcie = {
	{ 0x1d, 0xf6, 0xc4, 0x44, 0x44, 0x44, 0x00, 0x44,
	  0x9d, 0x52, 0x80, 0x2e, 0x27, 0xed, 0xe1, 0x9f }
};

const struct hv_guid hv_guid_netdir = {
	{ 0x3d, 0xaf, 0x2e, 0x8c, 0xa7, 0x32, 0x09, 0x4b,
	  0xab, 0x99, 0xbd, 0x1f, 0x1c, 0x86, 0xb5, 0x01 }
};

const struct hv_guid hv_guid_rdesktop = {
	{ 0xf4, 0xac, 0x6a, 0x27, 0x15, 0xac, 0x6c, 0x42,
	  0x98, 0xdd, 0x75, 0x21, 0xad, 0x3f, 0x01, 0xfe }
};

/* Automatic Virtual Machine Activation (AVMA) Services */
const struct hv_guid hv_guid_avma1 = {
	{ 0x55, 0xb2, 0x87, 0x44, 0x8c, 0xb8, 0x3f, 0x40,
	  0xbb, 0x51, 0xd1, 0xf6, 0x9c, 0xf1, 0x7f, 0x87 }
};

const struct hv_guid hv_guid_avma2 = {
	{ 0xf4, 0xba, 0x75, 0x33, 0x15, 0x9e, 0x30, 0x4b,
	  0xb7, 0x65, 0x67, 0xac, 0xb1, 0x0d, 0x60, 0x7b }
};

const struct hv_guid hv_guid_avma3 = {
	{ 0xa0, 0x1f, 0x22, 0x99, 0xad, 0x24, 0xe2, 0x11,
	  0xbe, 0x98, 0x00, 0x1a, 0xa0, 0x1b, 0xbf, 0x6e }
};

const struct hv_guid hv_guid_avma4 = {
	{ 0x16, 0x57, 0xe6, 0xf8, 0xb3, 0x3c, 0x06, 0x4a,
	  0x9a, 0x60, 0x18, 0x89, 0xc5, 0xcc, 0xca, 0xb5 }
};

static inline char *
guidprint(struct hv_guid *a)
{
	/* 3     0  5  4 7 6  8 9  10        15 */
	/* 33221100-5544-7766-9988-FFEEDDCCBBAA */
	static char buf[16 * 2 + 4 + 1];
	int i, j = 0;

	for (i = 3; i != -1; i -= 1, j += 2)
		snprintf(&buf[j], 3, "%02x", (uint8_t)a->data[i]);
	buf[j++] = '-';
	for (i = 5; i != 3; i -= 1, j += 2)
		snprintf(&buf[j], 3, "%02x", (uint8_t)a->data[i]);
	buf[j++] = '-';
	for (i = 7; i != 5; i -= 1, j += 2)
		snprintf(&buf[j], 3, "%02x", (uint8_t)a->data[i]);
	buf[j++] = '-';
	for (i = 8; i < 10; i += 1, j += 2)
		snprintf(&buf[j], 3, "%02x", (uint8_t)a->data[i]);
	buf[j++] = '-';
	for (i = 10; i < 16; i += 1, j += 2)
		snprintf(&buf[j], 3, "%02x", (uint8_t)a->data[i]);
	return (&buf[0]);
}
#endif	/* HYPERV_DEBUG */

void
hv_guid_sprint(struct hv_guid *guid, char *str, size_t size)
{
	const struct {
		const struct hv_guid	*guid;
		const char		*ident;
	} map[] = {
		{ &hv_guid_network,	"network" },
		{ &hv_guid_ide,		"ide" },
		{ &hv_guid_scsi,	"scsi" },
		{ &hv_guid_shutdown,	"shutdown" },
		{ &hv_guid_timesync,	"timesync" },
		{ &hv_guid_heartbeat,	"heartbeat" },
		{ &hv_guid_kvp,		"kvp" },
#ifdef HYPERV_DEBUG
		{ &hv_guid_vss,		"vss" },
		{ &hv_guid_dynmem,	"dynamic-memory" },
		{ &hv_guid_mouse,	"mouse" },
		{ &hv_guid_kbd,		"keyboard" },
		{ &hv_guid_video,	"video" },
		{ &hv_guid_fc,		"fiber-channel" },
		{ &hv_guid_fcopy,	"file-copy" },
		{ &hv_guid_pcie,	"pcie-passthrough" },
		{ &hv_guid_netdir,	"network-direct" },
		{ &hv_guid_rdesktop,	"remote-desktop" },
		{ &hv_guid_avma1,	"avma-1" },
		{ &hv_guid_avma2,	"avma-2" },
		{ &hv_guid_avma3,	"avma-3" },
		{ &hv_guid_avma4,	"avma-4" },
#endif
	};
	int i;

	for (i = 0; i < nitems(map); i++) {
		if (memcmp(guid, map[i].guid, sizeof(*guid)) == 0) {
			strlcpy(str, map[i].ident, size);
			return;
		}
	}
#ifdef HYPERV_DEBUG
	strlcpy(str, guidprint(guid), size);
#endif
}

int
hv_channel_scan(struct hv_softc *sc)
{
	struct hv_channel_msg_header hdr;
	struct hv_channel_offer_channel rsp, *offer;
	struct hv_offer *co;

	SIMPLEQ_INIT(&sc->sc_offers);
	mtx_init(&sc->sc_offerlck, IPL_NET);

	hdr.message_type = HV_CHANMSG_REQUEST_OFFERS;
	hdr.padding = 0;

	if (hv_cmd(sc, &hdr, sizeof(hdr), &rsp, sizeof(rsp), HCF_NOREPLY)) {
		DPRINTF("%s: REQUEST_OFFERS failed\n", sc->sc_dev.dv_xname);
		return (-1);
	}

	while ((sc->sc_flags & HSF_OFFERS_DELIVERED) == 0)
		tsleep(offer, PRIBIO, "hvoffers", 1);

	TAILQ_INIT(&sc->sc_channels);
	mtx_init(&sc->sc_channelck, IPL_NET);

	mtx_enter(&sc->sc_offerlck);
	while (!SIMPLEQ_EMPTY(&sc->sc_offers)) {
		co = SIMPLEQ_FIRST(&sc->sc_offers);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_offers, co_entry);
		mtx_leave(&sc->sc_offerlck);

		hv_process_offer(sc, co);
		free(co, M_DEVBUF, sizeof(*co));

		mtx_enter(&sc->sc_offerlck);
	}
	mtx_leave(&sc->sc_offerlck);

	return (0);
}

void
hv_process_offer(struct hv_softc *sc, struct hv_offer *co)
{
	struct hv_channel *ch, *nch;

	nch = malloc(sizeof(*nch), M_DEVBUF, M_ZERO | M_NOWAIT);
	if (nch == NULL) {
		printf("%s: failed to allocate memory for the channel\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	nch->ch_sc = sc;
	hv_guid_sprint(&co->co_chan.offer.interface_type, nch->ch_ident,
	    sizeof(nch->ch_ident));

	/*
	 * By default we setup state to enable batched reading.
	 * A specific service can choose to disable this prior
	 * to opening the channel.
	 */
	nch->ch_flags |= CHF_BATCHED;

	KASSERT((((vaddr_t)&nch->ch_sigevt) & 0x7) == 0);
	memset(&nch->ch_sigevt, 0, sizeof(nch->ch_sigevt));
	nch->ch_sigevt.connection_id = HV_EVENT_CONNECTION_ID;

	if (sc->sc_proto != HV_VMBUS_VERSION_WS2008) {
		if (co->co_chan.is_dedicated_interrupt)
			nch->ch_flags |= CHF_DEDICATED;
		nch->ch_sigevt.connection_id = co->co_chan.connection_id;
	}

	if (co->co_chan.monitor_allocated) {
		nch->ch_mgroup = co->co_chan.monitor_id >> 5;
		nch->ch_mindex = co->co_chan.monitor_id & 0x1f;
		nch->ch_flags |= CHF_MONITOR;
	}

	nch->ch_relid = co->co_chan.child_rel_id;

	memcpy(&nch->ch_type, &co->co_chan.offer.interface_type,
	    sizeof(ch->ch_type));
	memcpy(&nch->ch_inst, &co->co_chan.offer.interface_instance,
	    sizeof(ch->ch_inst));

	mtx_enter(&sc->sc_channelck);
	TAILQ_FOREACH(ch, &sc->sc_channels, ch_entry) {
		if (!memcmp(&ch->ch_type, &nch->ch_type, sizeof(ch->ch_type)) &&
		    !memcmp(&ch->ch_inst, &nch->ch_inst, sizeof(ch->ch_inst)))
			break;
	}
	if (ch != NULL) {
		if (co->co_chan.offer.sub_channel_index == 0) {
			printf("%s: unknown offer \"%s\"\n",
			    sc->sc_dev.dv_xname, nch->ch_ident);
			mtx_leave(&sc->sc_channelck);
			free(nch, M_DEVBUF, sizeof(*nch));
			return;
		}
#ifdef HYPERV_DEBUG
		printf("%s: subchannel %u for \"%s\"\n", sc->sc_dev.dv_xname,
		    co->co_chan.offer.sub_channel_index, ch->ch_ident);
#endif
		mtx_leave(&sc->sc_channelck);
		free(nch, M_DEVBUF, sizeof(*nch));
		return;
	}

	nch->ch_state = HV_CHANSTATE_OFFERED;

	TAILQ_INSERT_TAIL(&sc->sc_channels, nch, ch_entry);
	mtx_leave(&sc->sc_channelck);

#ifdef HYPERV_DEBUG
	printf("%s: channel %u: \"%s\"", sc->sc_dev.dv_xname, nch->ch_relid,
	    nch->ch_ident);
	if (co->co_chan.monitor_allocated)
		printf(", monitor %u\n", co->co_chan.monitor_id);
	else
		printf("\n");
#endif
}

struct hv_channel *
hv_channel_lookup(struct hv_softc *sc, uint32_t relid)
{
	struct hv_channel *ch;

	TAILQ_FOREACH(ch, &sc->sc_channels, ch_entry) {
		if (ch->ch_relid == relid)
			return (ch);
	}
	return (NULL);
}

int
hv_channel_ring_create(struct hv_channel *ch, uint32_t sndbuflen,
    uint32_t rcvbuflen)
{
	struct hv_softc *sc = ch->ch_sc;

	sndbuflen = roundup(sndbuflen, PAGE_SIZE);
	rcvbuflen = roundup(rcvbuflen, PAGE_SIZE);
	ch->ch_ring = km_alloc(sndbuflen + rcvbuflen, &kv_any, &kp_zero,
	    cold ? &kd_nowait : &kd_waitok);
	if (ch->ch_ring == NULL) {
		printf("%s: failed to allocate channel ring\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	ch->ch_ring_size = sndbuflen + rcvbuflen;
	ch->ch_ring_npg = ch->ch_ring_size >> PAGE_SHIFT;

	memset(&ch->ch_wrd, 0, sizeof(ch->ch_wrd));
	ch->ch_wrd.rd_ring = (struct hv_ring_buffer *)ch->ch_ring;
	ch->ch_wrd.rd_size = sndbuflen;
	ch->ch_wrd.rd_data_size = sndbuflen - sizeof(struct hv_ring_buffer);
	mtx_init(&ch->ch_wrd.rd_lock, IPL_NET);

	memset(&ch->ch_rrd, 0, sizeof(ch->ch_rrd));
	ch->ch_rrd.rd_ring = (struct hv_ring_buffer *)((uint8_t *)ch->ch_ring +
	    sndbuflen);
	ch->ch_rrd.rd_size = rcvbuflen;
	ch->ch_rrd.rd_data_size = rcvbuflen - sizeof(struct hv_ring_buffer);
	mtx_init(&ch->ch_rrd.rd_lock, IPL_NET);

	if (hv_handle_alloc(ch, ch->ch_ring, sndbuflen + rcvbuflen,
	    &ch->ch_ring_hndl)) {
		printf("%s: failed to obtain a PA handle for the ring\n",
		    sc->sc_dev.dv_xname);
		hv_channel_ring_destroy(ch);
		return (-1);
	}

	return (0);
}

void
hv_channel_ring_destroy(struct hv_channel *ch)
{
	km_free(ch->ch_ring, ch->ch_wrd.rd_size + ch->ch_rrd.rd_size,
	    &kv_any, &kp_zero);
	ch->ch_ring = NULL;
	hv_handle_free(ch, ch->ch_ring_hndl);

	memset(&ch->ch_wrd, 0, sizeof(ch->ch_wrd));
	memset(&ch->ch_rrd, 0, sizeof(ch->ch_rrd));
}

int
hv_channel_open(struct hv_channel *ch, void *udata, size_t udatalen,
    void (*handler)(void *), void *arg)
{
	struct hv_softc *sc = ch->ch_sc;
	struct hv_channel_open cmd;
	struct hv_channel_open_result rsp;
	int rv;

	if (ch->ch_ring == NULL &&
	    hv_channel_ring_create(ch, PAGE_SIZE * 4, PAGE_SIZE * 4)) {
		DPRINTF(": failed to create channel ring\n");
		return (-1);
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.header.message_type = HV_CHANMSG_OPEN_CHANNEL;
	cmd.open_id = ch->ch_relid;
	cmd.child_rel_id = ch->ch_relid;
	cmd.ring_buffer_gpadl_handle = ch->ch_ring_hndl;
	cmd.downstream_ring_buffer_page_offset =
	    ch->ch_wrd.rd_size >> PAGE_SHIFT;
	cmd.target_vcpu = ch->ch_vcpu;

	if (udata && udatalen > 0)
		memcpy(&cmd.user_data, udata, udatalen);

	memset(&rsp, 0, sizeof(rsp));

	ch->ch_handler = handler;
	ch->ch_ctx = arg;

	ch->ch_state = HV_CHANSTATE_OPENED;

	rv = hv_cmd(sc, &cmd, sizeof(cmd), &rsp, sizeof(rsp), 0);
	if (rv) {
		hv_channel_ring_destroy(ch);
		DPRINTF("%s: OPEN_CHANNEL failed with %d\n",
		    sc->sc_dev.dv_xname, rv);
		ch->ch_handler = NULL;
		ch->ch_ctx = NULL;
		ch->ch_state = HV_CHANSTATE_OFFERED;
		return (-1);
	}

	return (0);
}

int
hv_channel_close(struct hv_channel *ch)
{
	struct hv_softc *sc = ch->ch_sc;
	struct hv_channel_close cmd;
	int rv;

	memset(&cmd, 0, sizeof(cmd));
	cmd.header.message_type = HV_CHANMSG_CLOSE_CHANNEL;
	cmd.child_rel_id = ch->ch_relid;

	ch->ch_state = HV_CHANSTATE_CLOSING;
	rv = hv_cmd(sc, &cmd, sizeof(cmd), NULL, 0, HCF_NOREPLY);
	if (rv) {
		DPRINTF("%s: CLOSE_CHANNEL failed with %d\n",
		    sc->sc_dev.dv_xname, rv);
		return (-1);
	}
	ch->ch_state = HV_CHANSTATE_CLOSED;
	hv_channel_ring_destroy(ch);
	return (0);
}

static inline void
hv_channel_setevent(struct hv_softc *sc, struct hv_channel *ch)
{
	struct hv_monitor_trigger_group *mtg;

	/* Each uint32_t represents 32 channels */
	atomic_setbit_ptr((uint32_t *)sc->sc_wevents + (ch->ch_relid >> 5),
	    ch->ch_relid & 31);
	if (ch->ch_flags & CHF_MONITOR) {
		mtg = &sc->sc_monitor[1]->trigger_group[ch->ch_mgroup];
		atomic_setbit_ptr((uint32_t *)&mtg->pending, ch->ch_mindex);
	} else
		hv_intr_signal(sc, &ch->ch_sigevt);
}

static inline void
hv_ring_put(struct hv_ring_data *wrd, uint8_t *data, uint32_t datalen)
{
	int left = MIN(datalen, wrd->rd_data_size - wrd->rd_prod);

	memcpy(&wrd->rd_ring->buffer[wrd->rd_prod], data, left);
	memcpy(&wrd->rd_ring->buffer[0], data + left, datalen - left);
	wrd->rd_prod += datalen;
	wrd->rd_prod &= wrd->rd_data_size - 1;
}

static inline void
hv_ring_get(struct hv_ring_data *rrd, uint8_t *data, uint32_t datalen,
    int peek)
{
	int left = MIN(datalen, rrd->rd_data_size - rrd->rd_cons);

	memcpy(data, &rrd->rd_ring->buffer[rrd->rd_cons], left);
	memcpy(data + left, &rrd->rd_ring->buffer[0], datalen - left);
	if (!peek) {
		rrd->rd_cons += datalen;
		rrd->rd_cons &= rrd->rd_data_size - 1;
	}
}

#define	HV_BYTES_AVAIL_TO_WRITE(r, w, z)			\
	((w) >= (r)) ? ((z) - ((w) - (r))) : ((r) - (w))

static inline void
hv_ring_avail(struct hv_ring_data *rd, uint32_t *towrite, uint32_t *toread)
{
	uint32_t ridx = rd->rd_ring->read_index;
	uint32_t widx = rd->rd_ring->write_index;
	uint32_t r, w;

	w =  HV_BYTES_AVAIL_TO_WRITE(ridx, widx, rd->rd_data_size);
	r = rd->rd_data_size - w;
	if (towrite)
		*towrite = w;
	if (toread)
		*toread = r;
}

int
hv_ring_write(struct hv_ring_data *wrd, struct iovec *iov, int iov_cnt,
    int *needsig)
{
	uint64_t indices = 0;
	uint32_t avail, oprod, datalen = sizeof(indices);
	int i;

	for (i = 0; i < iov_cnt; i++)
		datalen += iov[i].iov_len;

	KASSERT(datalen <= wrd->rd_data_size);

	hv_ring_avail(wrd, &avail, NULL);
	if (avail < datalen) {
		printf("%s: avail %u datalen %u\n", __func__, avail, datalen);
		return (EAGAIN);
	}

	mtx_enter(&wrd->rd_lock);

	oprod = wrd->rd_prod;

	for (i = 0; i < iov_cnt; i++)
		hv_ring_put(wrd, iov[i].iov_base, iov[i].iov_len);

	indices = (uint64_t)wrd->rd_prod << 32;
	hv_ring_put(wrd, (uint8_t *)&indices, sizeof(indices));

	membar_sync();
	wrd->rd_ring->write_index = wrd->rd_prod;

	mtx_leave(&wrd->rd_lock);

	/* Signal when the ring transitions from being empty to non-empty */
	if (wrd->rd_ring->interrupt_mask == 0 &&
	    wrd->rd_ring->read_index == oprod)
		*needsig = 1;
	else
		*needsig = 0;

	return (0);
}

int
hv_channel_send(struct hv_channel *ch, void *data, uint32_t datalen,
    uint64_t rid, int type, uint32_t flags)
{
	struct hv_softc *sc = ch->ch_sc;
	struct hv_pktdesc d;
	struct iovec iov[3];
	uint32_t pktlen, pktlen_aligned;
	uint64_t zeropad = 0;
	int rv, needsig = 0;

	pktlen = sizeof(d) + datalen;
	pktlen_aligned = roundup(pktlen, sizeof(uint64_t));

	d.type = type;
	d.flags = flags;
	d.offset = sizeof(d) >> 3;
	d.length = pktlen_aligned >> 3;
	d.tid = rid;

	iov[0].iov_base = &d;
	iov[0].iov_len = sizeof(d);

	iov[1].iov_base = data;
	iov[1].iov_len = datalen;

	iov[2].iov_base = &zeropad;
	iov[2].iov_len = pktlen_aligned - pktlen;

	rv = hv_ring_write(&ch->ch_wrd, iov, 3, &needsig);
	if (rv == 0 && needsig)
		hv_channel_setevent(sc, ch);

	return (rv);
}

int
hv_channel_sendbuf(struct hv_channel *ch, struct hv_page_buffer *pb,
    uint32_t npb, void *data, uint32_t datalen, uint64_t rid)
{
	struct hv_softc *sc = ch->ch_sc;
	struct hv_gpadesc d;
	struct iovec iov[4];
	uint32_t buflen, pktlen, pktlen_aligned;
	uint64_t zeropad = 0;
	int rv, needsig = 0;

	buflen = sizeof(struct hv_page_buffer) * npb;
	pktlen = sizeof(d) + datalen + buflen;
	pktlen_aligned = roundup(pktlen, sizeof(uint64_t));

	d.type = HV_PKT_DATA_USING_GPA_DIRECT;
	d.flags = HV_PKTFLAG_COMPLETION_REQUESTED;
	d.offset = (sizeof(d) + buflen) >> 3;
	d.length = pktlen_aligned >> 3;
	d.tid = rid;
	d.range_count = npb;

	iov[0].iov_base = &d;
	iov[0].iov_len = sizeof(d);

	iov[1].iov_base = pb;
	iov[1].iov_len = buflen;

	iov[2].iov_base = data;
	iov[2].iov_len = datalen;

	iov[3].iov_base = &zeropad;
	iov[3].iov_len = pktlen_aligned - pktlen;

	rv = hv_ring_write(&ch->ch_wrd, iov, 4, &needsig);
	if (rv == 0 && needsig)
		hv_channel_setevent(sc, ch);

	return (rv);
}

int
hv_ring_peek(struct hv_ring_data *rrd, void *data, uint32_t datalen)
{
	uint32_t avail;

	KASSERT(datalen <= rrd->rd_data_size);

	hv_ring_avail(rrd, NULL, &avail);
	if (avail < datalen)
		return (EAGAIN);

	mtx_enter(&rrd->rd_lock);
	hv_ring_get(rrd, (uint8_t *)data, datalen, 1);
	mtx_leave(&rrd->rd_lock);
	return (0);
}

int
hv_ring_read(struct hv_ring_data *rrd, void *data, uint32_t datalen,
    uint32_t offset)
{
	uint64_t indices;
	uint32_t avail;

	KASSERT(datalen <= rrd->rd_data_size);

	hv_ring_avail(rrd, NULL, &avail);
	if (avail < datalen) {
		printf("%s: avail %u datalen %u\n", __func__, avail, datalen);
		return (EAGAIN);
	}

	mtx_enter(&rrd->rd_lock);

	if (offset) {
		rrd->rd_cons += offset;
		rrd->rd_cons &= rrd->rd_data_size - 1;
	}

	hv_ring_get(rrd, (uint8_t *)data, datalen, 0);
	hv_ring_get(rrd, (uint8_t *)&indices, sizeof(indices), 0);

	membar_sync();
	rrd->rd_ring->read_index = rrd->rd_cons;

	mtx_leave(&rrd->rd_lock);

	return (0);
}

int
hv_channel_recv(struct hv_channel *ch, void *data, uint32_t datalen,
    uint32_t *rlen, uint64_t *rid, int raw)
{
	struct hv_pktdesc d;
	uint32_t offset, pktlen;
	int rv;

	*rlen = 0;

	if ((rv = hv_ring_peek(&ch->ch_rrd, &d, sizeof(d))) != 0)
		return (rv);

	offset = raw ? 0 : (d.offset << 3);
	pktlen = (d.length << 3) - offset;
	if (pktlen > datalen) {
		printf("%s: pktlen %u datalen %u\n", __func__, pktlen, datalen);
		return (EINVAL);
	}

	rv = hv_ring_read(&ch->ch_rrd, data, pktlen, offset);
	if (rv == 0) {
		*rlen = pktlen;
		*rid = d.tid;
	}

	return (rv);
}

int
hv_handle_alloc(struct hv_channel *ch, void *buffer, uint32_t buflen,
    uint32_t *handle)
{
	struct hv_softc *sc = ch->ch_sc;
	struct hv_gpadl_header *hdr;
	struct hv_gpadl_body *body, *cmd;
	struct hv_gpadl_created rsp;
	struct hv_msg *msg;
	int i, j, last, left, rv;
	int bodylen = 0, ncmds = 0, pfn = 0;
	int waitok = cold ? M_NOWAIT : M_WAITOK;
	uint64_t *frames;
	paddr_t pa;
	/* Total number of pages to reference */
	int total = atop(buflen);
	/* Number of pages that will fit the header */
	int inhdr = MIN(total, HV_NPFNHDR);

	KASSERT((buflen & (PAGE_SIZE - 1)) == 0);

	if ((msg = malloc(sizeof(*msg), M_DEVBUF, M_ZERO | waitok)) == NULL)
		return (ENOMEM);

	/* Prepare array of frame addresses */
	if ((frames = mallocarray(total, sizeof(*frames), M_DEVBUF, M_ZERO |
	    waitok)) == NULL) {
		free(msg, M_DEVBUF, sizeof(*msg));
		return (ENOMEM);
	}
	for (i = 0; i < total; i++) {
		if (!pmap_extract(pmap_kernel(), (vaddr_t)buffer +
		    PAGE_SIZE * i, &pa)) {
			free(msg, M_DEVBUF, sizeof(*msg));
			free(frames, M_DEVBUF, total * sizeof(*frames));
			return (EFAULT);
		}
		frames[i] = atop(pa);
	}

	msg->msg_req.payload_size = sizeof(struct hv_gpadl_header) +
	    sizeof(struct hv_gpa_range) + inhdr * sizeof(uint64_t);
	hdr = (struct hv_gpadl_header *)msg->msg_req.payload;
	msg->msg_rsp = &rsp;
	msg->msg_rsplen = sizeof(rsp);
	if (!waitok)
		msg->msg_flags = MSGF_NOSLEEP;

	left = total - inhdr;

	/* Allocate additional gpadl_body structures if required */
	if (left > 0) {
		ncmds = MAX(1, left / HV_NPFNBODY + left % HV_NPFNBODY);
		bodylen = ncmds * HV_MESSAGE_PAYLOAD;
		body = malloc(bodylen, M_DEVBUF, M_ZERO | waitok);
		if (body == NULL) {
			free(msg, M_DEVBUF, sizeof(*msg));
			free(frames, M_DEVBUF, atop(buflen) * sizeof(*frames));
			return (ENOMEM);
		}
	}

	*handle = atomic_inc_int_nv(&sc->sc_handle);

	hdr->header.message_type = HV_CHANMSG_GPADL_HEADER;
	hdr->child_rel_id = ch->ch_relid;
	hdr->gpadl = *handle;

	/* Single range for a contiguous buffer */
	hdr->range_count = 1;
	hdr->range_buf_len = sizeof(struct hv_gpa_range) + total *
	    sizeof(uint64_t);
	hdr->range[0].byte_offset = 0;
	hdr->range[0].byte_count = buflen;

	/* Fit as many pages as possible into the header */
	for (i = 0; i < inhdr; i++)
		hdr->range[0].pfn_array[i] = frames[pfn++];

	for (i = 0; i < ncmds; i++) {
		cmd = (struct hv_gpadl_body *)((caddr_t)body +
		    HV_MESSAGE_PAYLOAD * i);
		cmd->header.message_type = HV_CHANMSG_GPADL_BODY;
		cmd->gpadl = *handle;
		last = MIN(left, HV_NPFNBODY);
		for (j = 0; j < last; j++)
			cmd->pfn[j] = frames[pfn++];
		left -= last;
	}

	rv = hv_start(sc, msg);
	if (rv != 0) {
		DPRINTF("%s: GPADL_HEADER failed\n", sc->sc_dev.dv_xname);
		goto out;
	}
	for (i = 0; i < ncmds; i++) {
		int cmdlen = sizeof(*cmd);
		cmd = (struct hv_gpadl_body *)((caddr_t)body +
		    HV_MESSAGE_PAYLOAD * i);
		/* Last element can be short */
		if (i == ncmds - 1)
			cmdlen += last * sizeof(uint64_t);
		else
			cmdlen += HV_NPFNBODY * sizeof(uint64_t);
		rv = hv_cmd(sc, cmd, cmdlen, NULL, 0, waitok | HCF_NOREPLY);
		if (rv != 0) {
			DPRINTF("%s: GPADL_BODY (iteration %d/%d) failed "
			    "with %d\n", sc->sc_dev.dv_xname, i, ncmds, rv);
			goto out;
		}
	}
	rv = hv_reply(sc, msg);
	if (rv != 0)
		DPRINTF("%s: GPADL allocation failed with %d\n",
		    sc->sc_dev.dv_xname, rv);

 out:
	free(msg, M_DEVBUF, sizeof(*msg));
	free(frames, M_DEVBUF, total * sizeof(*frames));
	if (bodylen > 0)
		free(body, M_DEVBUF, bodylen);
	if (rv != 0)
		return (rv);

	KASSERT(*handle == rsp.gpadl);

	return (0);
}

void
hv_handle_free(struct hv_channel *ch, uint32_t handle)
{
	struct hv_softc *sc = ch->ch_sc;
	struct hv_gpadl_teardown cmd;
	struct hv_gpadl_torndown rsp;
	int rv;

	memset(&cmd, 0, sizeof(cmd));
	cmd.header.message_type = HV_CHANMSG_GPADL_TEARDOWN;
	cmd.child_rel_id = ch->ch_relid;
	cmd.gpadl = handle;

	rv = hv_cmd(sc, &cmd, sizeof(cmd), &rsp, sizeof(rsp), 0);
	if (rv)
		DPRINTF("%s: GPADL_TEARDOWN failed with %d\n",
		    sc->sc_dev.dv_xname, rv);
}

const struct {
	const char		 *id_name;
	const struct hv_guid	 *id_type;
	void			(*id_init)(struct hv_channel *);
	void			(*id_handler)(void *);
} hv_internal_devs[] = {
	{ "heartbeat",	&hv_guid_heartbeat, NULL,		hv_heartbeat },
	{ "kvp",	&hv_guid_kvp,	    hv_kvp_init,	hv_kvp },
	{ "shutdown",	&hv_guid_shutdown,  hv_shutdown_init,	hv_shutdown },
	{ "timesync",	&hv_guid_timesync,  hv_timesync_init,	hv_timesync }
};

void
hv_attach_internal(struct hv_softc *sc)
{
	struct hv_channel *ch;
	int i;

	TAILQ_FOREACH(ch, &sc->sc_channels, ch_entry) {
		if (ch->ch_state != HV_CHANSTATE_OPENED)
			continue;
		if (ch->ch_flags & CHF_MONITOR)
			continue;
		for (i = 0; i < nitems(hv_internal_devs); i++) {
			if (memcmp(hv_internal_devs[i].id_type, &ch->ch_type,
			    sizeof(ch->ch_type)) != 0)
				continue;
			/*
			 * These services are not performance critical and
			 * do not need batched reading. Furthermore, some
			 * services such as KVP can only handle one message
			 * from the host at a time.
			 */
			ch->ch_flags &= ~CHF_BATCHED;

			if (hv_internal_devs[i].id_init)
				hv_internal_devs[i].id_init(ch);

			ch->ch_buf = km_alloc(PAGE_SIZE, &kv_any, &kp_zero,
			    (cold ? &kd_nowait : &kd_waitok));
			if (ch->ch_buf == NULL) {
				hv_channel_ring_destroy(ch);
				printf("%s: failed to allocate channel data "
				    "buffer for \"%s\"", sc->sc_dev.dv_xname,
				    hv_internal_devs[i].id_name);
				continue;
			}
			ch->ch_buflen = PAGE_SIZE;

			if (hv_channel_open(ch, NULL, 0,
			    hv_internal_devs[i].id_handler, ch)) {
				km_free(ch->ch_buf, PAGE_SIZE, &kv_any,
				    &kp_zero);
				ch->ch_buf = NULL;
				ch->ch_buflen = 0;
				printf("%s: failed to open channel for \"%s\"\n",
				    sc->sc_dev.dv_xname,
				    hv_internal_devs[i].id_name);
			}
			evcount_attach(&ch->ch_evcnt,
			    hv_internal_devs[i].id_name, &sc->sc_idtvec);
			break;
		}
	}
}

int
hv_service_common(struct hv_channel *ch, uint32_t *rlen, uint64_t *rid,
    struct hv_icmsg_hdr **hdr)
{
	struct hv_icmsg_negotiate *msg;
	int rv;

	rv = hv_channel_recv(ch, ch->ch_buf, ch->ch_buflen, rlen, rid, 0);
	if ((rv && rv != EAGAIN) || *rlen == 0)
		return (rv);
	*hdr = (struct hv_icmsg_hdr *)&ch->ch_buf[sizeof(struct hv_pipe_hdr)];
	if ((*hdr)->icmsgtype == HV_ICMSGTYPE_NEGOTIATE) {
		msg = (struct hv_icmsg_negotiate *)(*hdr + 1);
		if (msg->icframe_vercnt >= 2 &&
		    msg->icversion_data[1].major == 3) {
			msg->icversion_data[0].major = 3;
			msg->icversion_data[0].minor = 0;
			msg->icversion_data[1].major = 3;
			msg->icversion_data[1].minor = 0;
		} else {
			msg->icversion_data[0].major = 1;
			msg->icversion_data[0].minor = 0;
			msg->icversion_data[1].major = 1;
			msg->icversion_data[1].minor = 0;
		}
		msg->icframe_vercnt = 1;
		msg->icmsg_vercnt = 1;
		(*hdr)->icmsgsize = 0x10;
	}
	return (0);
}

void
hv_heartbeat(void *arg)
{
	struct hv_channel *ch = arg;
	struct hv_softc *sc = ch->ch_sc;
	struct hv_icmsg_hdr *hdr;
	struct hv_heartbeat_msg *msg;
	uint64_t rid;
	uint32_t rlen;
	int rv;

	rv = hv_service_common(ch, &rlen, &rid, &hdr);
	if (rv || rlen == 0) {
		if (rv != EAGAIN)
			printf("heartbeat: rv=%d rlen=%u\n", rv, rlen);
		return;
	}
	if (hdr->icmsgtype == HV_ICMSGTYPE_HEARTBEAT) {
		msg = (struct hv_heartbeat_msg *)(hdr + 1);
		msg->seq_num += 1;
	} else if (hdr->icmsgtype != HV_ICMSGTYPE_NEGOTIATE) {
		printf("%s: unhandled heartbeat message type %u\n",
		    sc->sc_dev.dv_xname, hdr->icmsgtype);
	}
	hdr->icflags = HV_ICMSGHDRFLAG_TRANSACTION | HV_ICMSGHDRFLAG_RESPONSE;
	hv_channel_send(ch, ch->ch_buf, rlen, rid, HV_PKT_DATA_IN_BAND, 0);
}

void
hv_kvp_init(struct hv_channel *ch)
{
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
hv_shutdown_init(struct hv_channel *ch)
{
	struct hv_softc *sc = ch->ch_sc;

	task_set(&sc->sc_sdtask, hv_shutdown_task, sc);
}

void
hv_shutdown(void *arg)
{
	struct hv_channel *ch = arg;
	struct hv_softc *sc = ch->ch_sc;
	struct hv_icmsg_hdr *hdr;
	struct hv_shutdown_msg *msg;
	uint64_t rid;
	uint32_t rlen;
	int rv, shutdown = 0;

	rv = hv_service_common(ch, &rlen, &rid, &hdr);
	if (rv || rlen == 0) {
		if (rv != EAGAIN)
			printf("shutdown: rv=%d rlen=%u\n", rv, rlen);
		return;
	}
	if (hdr->icmsgtype == HV_ICMSGTYPE_SHUTDOWN) {
		msg = (struct hv_shutdown_msg *)(hdr + 1);
		if (msg->flags == 0 || msg->flags == 1) {
			shutdown = 1;
			hdr->status = HV_S_OK;
		} else
			hdr->status = HV_E_FAIL;
	} else if (hdr->icmsgtype != HV_ICMSGTYPE_NEGOTIATE) {
		printf("%s: unhandled shutdown message type %u\n",
		    sc->sc_dev.dv_xname, hdr->icmsgtype);
	}

	hdr->icflags = HV_ICMSGHDRFLAG_TRANSACTION | HV_ICMSGHDRFLAG_RESPONSE;
	hv_channel_send(ch, ch->ch_buf, rlen, rid, HV_PKT_DATA_IN_BAND, 0);

	if (shutdown)
		task_add(systq, &sc->sc_sdtask);
}

void
hv_timesync_init(struct hv_channel *ch)
{
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
	struct hv_channel *ch = arg;
	struct hv_softc *sc = ch->ch_sc;
	struct hv_icmsg_hdr *hdr;
	struct hv_timesync_msg *msg;
	struct timespec guest, host, diff;
	uint64_t tns;
	uint64_t rid;
	uint32_t rlen;
	int rv;

	rv = hv_service_common(ch, &rlen, &rid, &hdr);
	if (rv || rlen == 0) {
		if (rv != EAGAIN)
			printf("timesync: rv=%d rlen=%u\n", rv, rlen);
		return;
	}
	if (hdr->icmsgtype == HV_ICMSGTYPE_TIMESYNC) {
		msg = (struct hv_timesync_msg *)(hdr + 1);
		if (msg->flags == HV_TIMESYNC_SYNC ||
		    msg->flags == HV_TIMESYNC_SAMPLE) {
			microtime(&sc->sc_sensor.tv);
			nanotime(&guest);
			tns = (msg->parent_time - 116444736000000000LL) * 100;
			host.tv_sec = tns / 1000000000LL;
			host.tv_nsec = tns % 1000000000LL;
			timespecsub(&guest, &host, &diff);
			sc->sc_sensor.value = (int64_t)diff.tv_sec *
			    1000000000LL + diff.tv_nsec;
			sc->sc_sensor.status = SENSOR_S_OK;
		}
	} else if (hdr->icmsgtype != HV_ICMSGTYPE_NEGOTIATE) {
		printf("%s: unhandled timesync message type %u\n",
		    sc->sc_dev.dv_xname, hdr->icmsgtype);
	}

	hdr->icflags = HV_ICMSGHDRFLAG_TRANSACTION | HV_ICMSGHDRFLAG_RESPONSE;
	hv_channel_send(ch, ch->ch_buf, rlen, rid, HV_PKT_DATA_IN_BAND, 0);
}

static int
hv_attach_print(void *aux, const char *name)
{
	struct hv_attach_args *aa = aux;

	if (name)
		printf("\"%s\" at %s", aa->aa_ident, name);

	return (UNCONF);
}

int
hv_attach_devices(struct hv_softc *sc)
{
	struct hv_dev *dv;
	struct hv_channel *ch;

	SLIST_INIT(&sc->sc_devs);
	mtx_init(&sc->sc_devlck, IPL_NET);

	TAILQ_FOREACH(ch, &sc->sc_channels, ch_entry) {
		if (ch->ch_state != HV_CHANSTATE_OFFERED)
			continue;
		if (!(ch->ch_flags & CHF_MONITOR))
			continue;
		dv = malloc(sizeof(*dv), M_DEVBUF, M_ZERO | M_NOWAIT);
		if (dv == NULL) {
			printf("%s: failed to allocate device object\n",
			    sc->sc_dev.dv_xname);
			return (-1);
		}
		dv->dv_aa.aa_parent = sc;
		dv->dv_aa.aa_type = &ch->ch_type;
		dv->dv_aa.aa_inst = &ch->ch_inst;
		dv->dv_aa.aa_ident = ch->ch_ident;
		dv->dv_aa.aa_chan = ch;
		dv->dv_aa.aa_dmat = sc->sc_dmat;
		mtx_enter(&sc->sc_devlck);
		SLIST_INSERT_HEAD(&sc->sc_devs, dv, dv_entry);
		mtx_leave(&sc->sc_devlck);
		config_found((struct device *)sc, &dv->dv_aa, hv_attach_print);
	}
	return (0);
}
