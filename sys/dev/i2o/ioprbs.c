/*	$OpenBSD: ioprbs.c,v 1.31 2011/05/02 22:15:11 chl Exp $	*/

/*
 * Copyright (c) 2001 Niklas Hallqvist
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * A driver for I2O "Random block storage" devices, like RAID.
 */

/*
 * This driver would not have been written if it was not for the hardware
 * donation from pi.se.  I want to thank them for their support.  It also
 * had been much harder without Andrew Doran's work in NetBSD's ld_iop.c
 * driver, from which I have both gotten inspiration and actual code.
 * - Niklas Hallqvist
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/i2o/i2o.h>
#include <dev/i2o/iopio.h>
#include <dev/i2o/iopvar.h>
#include <dev/i2o/ioprbsvar.h>

#ifdef I2ODEBUG
#define DPRINTF(x)	printf x
#else
#define	DPRINTF(x)
#endif

void	ioprbs_adjqparam(struct device *, int);
void	ioprbs_attach(struct device *, struct device *, void *);
void	ioprbs_copy_internal_data(struct scsi_xfer *, u_int8_t *,
	    size_t);
void	ioprbs_enqueue_ccb(struct ioprbs_softc *, struct ioprbs_ccb *);
int	ioprbs_exec_ccb(struct ioprbs_ccb *);
void	ioprbs_free_ccb(void *, void *);
void	*ioprbs_get_ccb(void *);
void	ioprbs_internal_cache_cmd(struct scsi_xfer *);
void	ioprbs_intr(struct device *, struct iop_msg *, void *);
void	ioprbs_intr_event(struct device *, struct iop_msg *, void *);
int	ioprbs_match(struct device *, void *, void *);
void	ioprbs_scsi_cmd(struct scsi_xfer *);
int	ioprbs_start(struct ioprbs_ccb *);
void	ioprbs_start_ccbs(struct ioprbs_softc *);
void	ioprbs_timeout(void *);
void	ioprbs_unconfig(struct ioprbs_softc *, int);
void	ioprbs_watchdog(void *);

struct cfdriver ioprbs_cd = {
	NULL, "ioprbs", DV_DULL
};

struct cfattach ioprbs_ca = {
	sizeof(struct ioprbs_softc), ioprbs_match, ioprbs_attach
};

struct scsi_adapter ioprbs_switch = {
	ioprbs_scsi_cmd, scsi_minphys, 0, 0,
};

#ifdef I2OVERBOSE
static const char *const ioprbs_errors[] = { 
	"success", 
	"media error", 
	"access error",
	"device failure",
	"device not ready",
	"media not present",
	"media locked",
	"media failure",
	"protocol failure",
	"bus failure",
	"access violation",
	"media write protected",
	"device reset",
	"volume changed, waiting for acknowledgement",
	"timeout",
};
#endif

/*
 * Match a supported device.
 */
int
ioprbs_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct iop_attach_args *ia = aux;

	return (ia->ia_class == I2O_CLASS_RANDOM_BLOCK_STORAGE);
}

/*
 * Attach a supported device.
 */
void
ioprbs_attach(struct device *parent, struct device *self, void *aux)
{
	struct iop_attach_args *ia = aux;
	struct ioprbs_softc *sc = (struct ioprbs_softc *)self;
	struct iop_softc *iop = (struct iop_softc *)parent;
	struct scsibus_attach_args saa;
	int rv, state = 0;
	int enable;
	u_int32_t cachesz;
	char *typestr, *fixedstr;
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		union {
			struct	i2o_param_rbs_cache_control cc;
			struct	i2o_param_rbs_device_info bdi;
			struct	i2o_param_rbs_operation op;
		} p;
	} __packed param;
	int i;

	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_ccbq);
	mtx_init(&sc->sc_ccb_mtx, IPL_BIO);
	scsi_iopool_init(&sc->sc_iopool, sc, ioprbs_get_ccb, ioprbs_free_ccb);

	/* Initialize the ccbs */
	for (i = 0; i < IOPRBS_MAX_CCBS; i++)
		TAILQ_INSERT_TAIL(&sc->sc_free_ccb, &sc->sc_ccbs[i],
		    ic_chain);

	/* Register us as an initiator. */
	sc->sc_ii.ii_dv = self;
	sc->sc_ii.ii_intr = ioprbs_intr;
	sc->sc_ii.ii_adjqparam = ioprbs_adjqparam;
	sc->sc_ii.ii_flags = 0;
	sc->sc_ii.ii_tid = ia->ia_tid;
	iop_initiator_register(iop, &sc->sc_ii);

	/* Register another initiator to handle events from the device. */
	sc->sc_eventii.ii_dv = self;
	sc->sc_eventii.ii_intr = ioprbs_intr_event;
	sc->sc_eventii.ii_flags = II_DISCARD | II_UTILITY;
	sc->sc_eventii.ii_tid = ia->ia_tid;
	iop_initiator_register(iop, &sc->sc_eventii);

	rv = iop_util_eventreg(iop, &sc->sc_eventii,
	    I2O_EVENT_GEN_EVENT_MASK_MODIFIED | I2O_EVENT_GEN_DEVICE_RESET |
	    I2O_EVENT_GEN_STATE_CHANGE | I2O_EVENT_GEN_GENERAL_WARNING);
	if (rv != 0) {
		printf("%s: unable to register for events", self->dv_xname);
		goto bad;
	}
	state++;

	/*
	 * Start out with one queued command.  The `iop' driver will adjust
	 * the queue parameters once we're up and running.
	 */
	sc->sc_maxqueuecnt = 1;

	sc->sc_maxxfer = IOP_MAX_XFER;

	/* Say what the device is. */
	printf(":");
	iop_print_ident(iop, ia->ia_tid);

	/*
	 * Claim the device so that we don't get any nasty surprises.  Allow
	 * failure.
	 */
	rv = iop_util_claim(iop, &sc->sc_ii, 0,
	    I2O_UTIL_CLAIM_CAPACITY_SENSITIVE |
	    I2O_UTIL_CLAIM_NO_PEER_SERVICE |
	    I2O_UTIL_CLAIM_NO_MANAGEMENT_SERVICE |
	    I2O_UTIL_CLAIM_PRIMARY_USER);
	sc->sc_flags = rv ? 0 : IOPRBS_CLAIMED;

	rv = iop_param_op(iop, ia->ia_tid, NULL, 0, I2O_PARAM_RBS_DEVICE_INFO,
	    &param, sizeof param);
	if (rv != 0) {
		printf("%s: unable to get parameters (0x%04x; %d)\n",
		   sc->sc_dv.dv_xname, I2O_PARAM_RBS_DEVICE_INFO, rv);
		goto bad;
	}

	sc->sc_secsize = letoh32(param.p.bdi.blocksize);
	sc->sc_secperunit = (int)
	    (letoh64(param.p.bdi.capacity) / sc->sc_secsize);

	switch (param.p.bdi.type) {
	case I2O_RBS_TYPE_DIRECT:
		typestr = "direct access";
		enable = 1;
		break;
	case I2O_RBS_TYPE_WORM:
		typestr = "WORM";
		enable = 0;
		break;
	case I2O_RBS_TYPE_CDROM:
		typestr = "CD-ROM";
		enable = 0;
		break;
	case I2O_RBS_TYPE_OPTICAL:
		typestr = "optical";
		enable = 0;
		break;
	default:
		typestr = "unknown";
		enable = 0;
		break;
	}

	if ((letoh32(param.p.bdi.capabilities) & I2O_RBS_CAP_REMOVABLE_MEDIA)
	    != 0) {
		/* sc->sc_flags = IOPRBS_REMOVABLE; */
		fixedstr = "removable";
		enable = 0;
	} else
		fixedstr = "fixed";

	printf(" %s, %s", typestr, fixedstr);

	/*
	 * Determine if the device has an private cache.  If so, print the
	 * cache size.  Even if the device doesn't appear to have a cache,
	 * we perform a flush at shutdown.
	 */
	rv = iop_param_op(iop, ia->ia_tid, NULL, 0,
	    I2O_PARAM_RBS_CACHE_CONTROL, &param, sizeof(param));
	if (rv != 0) {
		printf("%s: unable to get parameters (0x%04x; %d)\n",
		   sc->sc_dv.dv_xname, I2O_PARAM_RBS_CACHE_CONTROL, rv);
		goto bad;
	}

	if ((cachesz = letoh32(param.p.cc.totalcachesize)) != 0)
		printf(", %dkB cache", cachesz >> 10);

	printf("\n");

	/*
	 * Configure the DDM's timeout functions to time out all commands
	 * after 30 seconds.
	 */
	rv = iop_param_op(iop, ia->ia_tid, NULL, 0, I2O_PARAM_RBS_OPERATION,
	    &param, sizeof(param));
	if (rv != 0) {
		printf("%s: unable to get parameters (0x%04x; %d)\n",
		   sc->sc_dv.dv_xname, I2O_PARAM_RBS_OPERATION, rv);
		goto bad;
	}

	param.p.op.timeoutbase = htole32(IOPRBS_TIMEOUT * 1000); 
	param.p.op.rwvtimeoutbase = htole32(IOPRBS_TIMEOUT * 1000); 
	param.p.op.rwvtimeout = 0; 

	rv = iop_param_op(iop, ia->ia_tid, NULL, 1, I2O_PARAM_RBS_OPERATION,
	    &param, sizeof(param));
#ifdef notdef
	/*
	 * Intel RAID adapters don't like the above, but do post a
	 * `parameter changed' event.  Perhaps we're doing something
	 * wrong...
	 */
	if (rv != 0) {
		printf("%s: unable to set parameters (0x%04x; %d)\n",
		   sc->sc_dv.dv_xname, I2O_PARAM_RBS_OPERATION, rv);
		goto bad;
	}
#endif

	if (enable)
		sc->sc_flags |= IOPRBS_ENABLED;
	else
		printf("%s: device not yet supported\n", self->dv_xname);

	/* Fill in the prototype scsi_link. */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter = &ioprbs_switch;
	sc->sc_link.openings = 1;
	sc->sc_link.adapter_buswidth = 1;
	sc->sc_link.adapter_target = 1;
	sc->sc_link.pool = &sc->sc_iopool;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	config_found(&sc->sc_dv, &saa, scsiprint);

	return;

 bad:
	ioprbs_unconfig(sc, state > 0);
}

void
ioprbs_unconfig(struct ioprbs_softc *sc, int evreg)
{
	struct iop_softc *iop;
	int s;

	iop = (struct iop_softc *)sc->sc_dv.dv_parent;

	if ((sc->sc_flags & IOPRBS_CLAIMED) != 0)
		iop_util_claim(iop, &sc->sc_ii, 1,
		    I2O_UTIL_CLAIM_PRIMARY_USER);

	if (evreg) {
		/*
		 * Mask off events, and wait up to 5 seconds for a reply. 
		 * Note that some adapters won't reply to this (XXX We
		 * should check the event capabilities).
		 */
		sc->sc_flags &= ~IOPRBS_NEW_EVTMASK;
		iop_util_eventreg(iop, &sc->sc_eventii,
		    I2O_EVENT_GEN_EVENT_MASK_MODIFIED);
		s = splbio();
		if ((sc->sc_flags & IOPRBS_NEW_EVTMASK) == 0)
			tsleep(&sc->sc_eventii, PRIBIO, "ioprbsevt", hz * 5);
		splx(s);
#ifdef I2ODEBUG
		if ((sc->sc_flags & IOPRBS_NEW_EVTMASK) == 0)
			printf("%s: didn't reply to event unregister",
			    sc->sc_dv.dv_xname);
#endif
	}

	iop_initiator_unregister(iop, &sc->sc_eventii);
	iop_initiator_unregister(iop, &sc->sc_ii);
}

void
ioprbs_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct ioprbs_softc *sc = link->adapter_softc;
	struct ioprbs_ccb *ccb = xs->io;
	u_int64_t blockno;
	u_int32_t blockcnt;
	int s;

	xs->error = XS_NOERROR;

	switch (xs->cmd->opcode) {
	case TEST_UNIT_READY:
	case REQUEST_SENSE:
	case INQUIRY:
	case MODE_SENSE:
	case START_STOP:
	case READ_CAPACITY:
#if 0
	case VERIFY:
#endif
		ioprbs_internal_cache_cmd(xs);
		scsi_done(xs);
		return;

	case PREVENT_ALLOW:
		DPRINTF(("PREVENT/ALLOW "));
		/* XXX Not yet implemented */
		xs->error = XS_NOERROR;
		scsi_done(xs);
		return;

	case SYNCHRONIZE_CACHE:
		DPRINTF(("SYNCHRONIZE_CACHE "));
		/* XXX Not yet implemented */
		xs->error = XS_NOERROR;
		scsi_done(xs);
		return;

	default:
		DPRINTF(("unknown opc %d ", xs->cmd->opcode));
		/* XXX Not yet implemented */
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return;

	case READ_COMMAND:
	case READ_BIG:
	case WRITE_COMMAND:
	case WRITE_BIG:
		DPRINTF(("rw opc %d ", xs->cmd->opcode));

		scsi_cmd_rw_decode(xs->cmd, &blockno, &blockcnt);
		if (blockno >= sc->sc_secperunit ||
		    blockcnt > sc->sc_secperunit - blockno) {
			printf("%s: out of bounds %llu-%u >= %u\n",
			    sc->sc_dv.dv_xname, blockno, blockcnt,
			    sc->sc_secperunit);
			/*
			 * XXX Should be XS_SENSE but that
			 * would require setting up a faked
			 * sense too.
			 */
			xs->error = XS_DRIVER_STUFFUP;
			scsi_done(xs);
			return;
		}

		ccb->ic_blockno = blockno;
		ccb->ic_blockcnt = blockcnt;
		ccb->ic_xs = xs;
		ccb->ic_timeout = xs->timeout;

		s = splbio();
		ioprbs_enqueue_ccb(sc, ccb);
		splx(s);

		if (xs->flags & SCSI_POLL) {
			/* XXX Should actually poll... */
		}
	}
}

void
ioprbs_intr(struct device *dv, struct iop_msg *im, void *reply)
{
	struct i2o_rbs_reply *rb = reply;
	struct ioprbs_ccb *ccb = im->im_dvcontext;
	struct scsi_xfer *xs = ccb->ic_xs;
	struct iop_softc *iop = (struct iop_softc *)dv->dv_parent;
	int err, detail;
#ifdef I2OVERBOSE
	const char *errstr;
#endif

	DPRINTF(("ioprbs_intr(%p, %p, %p) ", dv, im, reply));

	timeout_del(&xs->stimeout);

	err = ((rb->msgflags & I2O_MSGFLAGS_FAIL) != 0);

	if (!err && rb->reqstatus != I2O_STATUS_SUCCESS) {
		detail = letoh16(rb->detail);
#ifdef I2OVERBOSE
		if (detail >= sizeof(ioprbs_errors) / sizeof(ioprbs_errors[0]))
			errstr = "<unknown>";
		else
			errstr = ioprbs_errors[detail];
		printf("%s: error 0x%04x: %s\n", dv->dv_xname, detail, errstr);
#else
		printf("%s: error 0x%04x\n", dv->dv_xname, detail);
#endif
		err = 1;
	}

	if (err)
		xs->error = XS_DRIVER_STUFFUP;
	else
		xs->resid = xs->datalen - letoh32(rb->transfercount);

	iop_msg_unmap(iop, im);
	iop_msg_free(iop, im);
	scsi_done(xs);
}

void
ioprbs_intr_event(struct device *dv, struct iop_msg *im, void *reply)
{
	struct i2o_util_event_register_reply *rb;
	struct ioprbs_softc *sc;
	u_int event;

	rb = reply;

	if ((rb->msgflags & I2O_MSGFLAGS_FAIL) != 0)
		return;

	event = letoh32(rb->event);
	sc = (struct ioprbs_softc *)dv;

	if (event == I2O_EVENT_GEN_EVENT_MASK_MODIFIED) {
		sc->sc_flags |= IOPRBS_NEW_EVTMASK;
		wakeup(&sc->sc_eventii);
#ifndef I2ODEBUG
		return;
#endif
	}

	printf("%s: event 0x%08x received\n", dv->dv_xname, event);
}

void
ioprbs_adjqparam(struct device *dv, int mpi)
{
#if 0
	struct iop_softc *iop;

	/*
	 * AMI controllers seem to lose the plot if you hand off lots of
	 * queued commands.
	 */
	iop = (struct iop_softc *)dv->dv_parent;
	if (letoh16(I2O_ORG_AMI) == iop->sc_status.orgid && mpi > 64)
		mpi = 64;

	ldadjqparam((struct ld_softc *)dv, mpi);
#endif
}

void
ioprbs_copy_internal_data(xs, data, size)
	struct scsi_xfer *xs;
	u_int8_t *data;
	size_t size;
{
	size_t copy_cnt;

	DPRINTF(("ioprbs_copy_internal_data "));

	if (!xs->datalen)
		printf("uio move not yet supported\n");
	else {
		copy_cnt = MIN(size, xs->datalen);
		bcopy(data, xs->data, copy_cnt);
	}
}

/* Emulated SCSI operation on cache device */
void
ioprbs_internal_cache_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *link = xs->sc_link;
	struct ioprbs_softc *sc = link->adapter_softc;
	u_int8_t target = link->target;
	struct scsi_inquiry_data inq;
	struct scsi_sense_data sd;
	struct scsi_read_cap_data rcd;

	DPRINTF(("ioprbs_internal_cache_cmd "));

	xs->error = XS_NOERROR;

	if (target > 0 || link->lun != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		return;
	}

	switch (xs->cmd->opcode) {
	case TEST_UNIT_READY:
	case START_STOP:
#if 0
	case VERIFY:
#endif
		DPRINTF(("opc %d tgt %d ", xs->cmd->opcode, target));
		break;

	case REQUEST_SENSE:
		DPRINTF(("REQUEST SENSE tgt %d ", target));
		bzero(&sd, sizeof sd);
		sd.error_code = SSD_ERRCODE_CURRENT;
		sd.segment = 0;
		sd.flags = SKEY_NO_SENSE;
		bzero(sd.info, sizeof sd.info);
		sd.extra_len = 0;
		ioprbs_copy_internal_data(xs, (u_int8_t *)&sd, sizeof sd);
		break;

	case INQUIRY:
		DPRINTF(("INQUIRY tgt %d", target));
		bzero(&inq, sizeof inq);
		/* XXX How do we detect removable/CD-ROM devices?  */
		inq.device = T_DIRECT;
		inq.dev_qual2 = 0;
		inq.version = 2;
		inq.response_format = 2;
		inq.additional_length = 32;
		inq.flags |= SID_CmdQue;
		strlcpy(inq.vendor, "I2O", sizeof inq.vendor);
		snprintf(inq.product, sizeof inq.product, "Container #%02d",
		    target);
		strlcpy(inq.revision, "   ", sizeof inq.revision);
		ioprbs_copy_internal_data(xs, (u_int8_t *)&inq, sizeof inq);
		break;

	case READ_CAPACITY:
		DPRINTF(("READ CAPACITY tgt %d ", target));
		bzero(&rcd, sizeof rcd);
		_lto4b(sc->sc_secperunit - 1, rcd.addr);
		_lto4b(IOPRBS_BLOCK_SIZE, rcd.length);
		ioprbs_copy_internal_data(xs, (u_int8_t *)&rcd, sizeof rcd);
		break;

	default:
		DPRINTF(("unsupported scsi command %#x tgt %d ",
		    xs->cmd->opcode, target));
		xs->error = XS_DRIVER_STUFFUP;
		return;
	}

	xs->error = XS_NOERROR;
}

void *
ioprbs_get_ccb(void *cookie)
{
	struct ioprbs_softc *sc = cookie;
	struct ioprbs_ccb *ccb;

	mtx_enter(&sc->sc_ccb_mtx);
	ccb = TAILQ_FIRST(&sc->sc_free_ccb);
	if (ccb)
		TAILQ_REMOVE(&sc->sc_free_ccb, ccb, ic_chain);
	mtx_leave(&sc->sc_ccb_mtx);

	/* initialise the command */
	if (ccb)
		ccb->ic_flags = 0;

	return (ccb);
}

void
ioprbs_free_ccb(void *cookie, void *io)
{
	struct ioprbs_softc *sc = cookie;
	struct ioprbs_ccb *ccb = io;

	mtx_enter(&sc->sc_ccb_mtx);
	TAILQ_INSERT_HEAD(&sc->sc_free_ccb, ccb, ic_chain);
	mtx_leave(&sc->sc_ccb_mtx);
}

void
ioprbs_enqueue_ccb(sc, ccb)
	struct ioprbs_softc *sc;
	struct ioprbs_ccb *ccb;
{
	DPRINTF(("ioprbs_enqueue_ccb(%p, %p) ", sc, ccb));

	timeout_set(&ccb->ic_xs->stimeout, ioprbs_timeout, ccb);
	TAILQ_INSERT_TAIL(&sc->sc_ccbq, ccb, ic_chain);
	ioprbs_start_ccbs(sc);
}

void
ioprbs_start_ccbs(sc)
	struct ioprbs_softc *sc;
{
	struct ioprbs_ccb *ccb;
	struct scsi_xfer *xs;

	DPRINTF(("ioprbs_start_ccbs(%p) ", sc));

	while ((ccb = TAILQ_FIRST(&sc->sc_ccbq)) != NULL) {

		xs = ccb->ic_xs;
		if (ccb->ic_flags & IOPRBS_ICF_WATCHDOG)
			timeout_del(&xs->stimeout);

		if (ioprbs_exec_ccb(ccb) == 0) {
			ccb->ic_flags |= IOPRBS_ICF_WATCHDOG;
			timeout_set(&ccb->ic_xs->stimeout, ioprbs_watchdog,
			    ccb);
			timeout_add_msec(&xs->stimeout, IOPRBS_WATCH_TIMEOUT);
			break;
		}
		TAILQ_REMOVE(&sc->sc_ccbq, ccb, ic_chain);

		if ((xs->flags & SCSI_POLL) == 0) {
			timeout_set(&ccb->ic_xs->stimeout, ioprbs_timeout,
			    ccb);
			timeout_add_msec(&xs->stimeout, ccb->ic_timeout);
		}
	}
}

int
ioprbs_exec_ccb(ccb)
	struct ioprbs_ccb *ccb;
{
	struct scsi_xfer *xs = ccb->ic_xs;

	DPRINTF(("ioprbs_exec_ccb(%p, %p) ", xs, ccb));

	ioprbs_start(ccb);

	xs->error = XS_NOERROR;
	xs->resid = 0;
	return (1);
}

/*
 * Deliver a command to the controller; allocate controller resources at the
 * last moment when possible.
 */
int
ioprbs_start(struct ioprbs_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->ic_xs;
	struct scsi_link *link = xs->sc_link;
	struct ioprbs_softc *sc = link->adapter_softc;
#ifdef I2ODEBUG
	u_int8_t target = link->target;
#endif
	struct iop_msg *im;
	struct iop_softc *iop = (struct iop_softc *)sc->sc_dv.dv_parent;
	struct i2o_rbs_block_read *mf;
	u_int rv, flags = 0, mode = I2O_RBS_BLOCK_READ;
	u_int64_t ba;
	u_int32_t mb[IOP_MAX_MSG_SIZE / sizeof(u_int32_t)];

	im = iop_msg_alloc(iop, &sc->sc_ii, 0);
	im->im_dvcontext = ccb;

	switch (xs->cmd->opcode) {
	case PREVENT_ALLOW:
	case SYNCHRONIZE_CACHE:
		if (xs->cmd->opcode == PREVENT_ALLOW) {
			/* XXX PREVENT_ALLOW support goes here */
		} else {
			DPRINTF(("SYNCHRONIZE CACHE tgt %d ", target));
		}
		break;

	case WRITE_COMMAND:
	case WRITE_BIG:
		flags = I2O_RBS_BLOCK_WRITE_CACHE_WB;
		mode = I2O_RBS_BLOCK_WRITE;
		/* FALLTHROUGH */

	case READ_COMMAND:
	case READ_BIG:
		ba = (u_int64_t)ccb->ic_blockno * DEV_BSIZE;

		/*
		 * Fill the message frame.  We can use the block_read
		 * structure for both reads and writes, as it's almost
		 * identical to the * block_write structure.
		 */
		mf = (struct i2o_rbs_block_read *)mb;
		mf->msgflags = I2O_MSGFLAGS(i2o_rbs_block_read);
		mf->msgfunc = I2O_MSGFUNC(sc->sc_ii.ii_tid, mode);
		mf->msgictx = sc->sc_ii.ii_ictx;
		mf->msgtctx = im->im_tctx;
		mf->flags = flags | (1 << 16);	/* flags & time multiplier */
		mf->datasize = ccb->ic_blockcnt * DEV_BSIZE;
		mf->lowoffset = (u_int32_t)ba;
		mf->highoffset = (u_int32_t)(ba >> 32);

		/* Map the data transfer and enqueue the command. */
		rv = iop_msg_map_bio(iop, im, mb, xs->data,
		    ccb->ic_blockcnt * DEV_BSIZE, mode == I2O_RBS_BLOCK_WRITE);
		if (rv == 0) {
			if ((rv = iop_msg_post(iop, im, mb, 0)) != 0) {
				iop_msg_unmap(iop, im);
				iop_msg_free(iop, im);
			}
		}
		break;
	}
	return (0);
}

void
ioprbs_timeout(arg)
	void *arg;
{
	struct ioprbs_ccb *ccb = arg;
	struct scsi_link *link = ccb->ic_xs->sc_link;
	struct ioprbs_softc *sc = link->adapter_softc;
	int s;

	sc_print_addr(link);
	printf("timed out\n");

	/* XXX Test for multiple timeouts */

	ccb->ic_xs->error = XS_TIMEOUT;
	s = splbio();
	ioprbs_enqueue_ccb(sc, ccb);
	splx(s);
}

void
ioprbs_watchdog(arg)
	void *arg;
{
	struct ioprbs_ccb *ccb = arg;
	struct scsi_link *link = ccb->ic_xs->sc_link;
	struct ioprbs_softc *sc = link->adapter_softc;
	int s;

	s = splbio();
	ccb->ic_flags &= ~IOPRBS_ICF_WATCHDOG;
	ioprbs_start_ccbs(sc);
	splx(s);
}
