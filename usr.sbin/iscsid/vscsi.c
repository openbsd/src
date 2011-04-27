/*	$OpenBSD: vscsi.c,v 1.5 2011/04/27 07:25:26 claudio Exp $ */

/*
 * Copyright (c) 2009 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <scsi/iscsi.h>
#include <scsi/scsi_all.h>
#include <dev/vscsivar.h>

#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "iscsid.h"
#include "log.h"

struct vscsi {
	struct event	ev;
	int		fd;
} v;

struct scsi_task {
	struct task	task;
	int		tag;
	u_int		target;
	u_int		lun;
	size_t		datalen;
};

void vscsi_callback(struct connection *, void *, struct pdu *);
void vscsi_fail(void *arg);
void vscsi_dataout(struct session *, struct scsi_task *, u_int32_t, size_t);

void
vscsi_open(char *dev)
{
	if ((v.fd = open(dev, O_RDWR)) == -1)
		fatal("vscsi_open");

	event_set(&v.ev, v.fd, EV_READ|EV_PERSIST, vscsi_dispatch, NULL);
	event_add(&v.ev, NULL);
}

void
vscsi_dispatch(int fd, short event, void *arg)
{
	struct vscsi_ioc_i2t i2t;
	struct iscsi_pdu_scsi_request *sreq;
	struct session *s;
	struct scsi_task *t;
	struct pdu *p;
#if 0
	char *buf;
	u_int32_t t32;
#endif

	if (!(event & EV_READ)) {
		log_debug("spurious read call");
		return;
	}

	if (ioctl(v.fd, VSCSI_I2T, &i2t) == -1)
		fatal("vscsi_dispatch");

	s = initiator_t2s(i2t.target);
	if (s == NULL)
		fatalx("vscsi_dispatch: unknown target");

	if (!(t = calloc(1, sizeof(*t))))
		fatal("vscsi_dispatch");

	t->tag = i2t.tag;
	t->target = i2t.target;
	t->lun = i2t.lun;

	if (!(p = pdu_new()))
		fatal("vscsi_dispatch");
	if (!(sreq = pdu_gethdr(p)))
		fatal("vscsi_dispatch");

	sreq->opcode = ISCSI_OP_SCSI_REQUEST;
	/* XXX use untagged commands, dlg sais so */
	sreq->flags = ISCSI_SCSI_F_F | ISCSI_SCSI_ATTR_UNTAGGED;
	if (i2t.direction == VSCSI_DIR_WRITE)
		sreq->flags |= ISCSI_SCSI_F_W;
	if (i2t.direction == VSCSI_DIR_READ)
		sreq->flags |= ISCSI_SCSI_F_R;
	sreq->bytes = htonl(i2t.datalen);

	/* LUN handling: currently we only do single level LUNs < 256 */
	if (t->lun >= 256)
		fatal("vscsi_dispatch: I'm sorry, Dave. "
		    "I'm afraid I can't do that.");
	sreq->lun[1] = t->lun;

	bcopy(&i2t.cmd, sreq->cdb, i2t.cmdlen);

#if 0
	if (i2t.direction == VSCSI_DIR_WRITE) {
		if (!(buf = pdu_alloc(i2t.datalen)))
			fatal("vscsi_dispatch");
		t32 = htonl(i2t.datalen);
		bcopy(&t32, &sreq->ahslen, sizeof(t32));
		vscsi_data(VSCSI_DATA_WRITE, i2t.tag, buf, i2t.datalen);
		pdu_addbuf(p, buf, i2t.datalen, PDU_DATA);
	}
#endif

	task_init(&t->task, s, 0, t, vscsi_callback, vscsi_fail);
	task_pdu_add(&t->task, p);
	session_task_issue(s, &t->task);
}

void
vscsi_data(unsigned long req, int tag, void *buf, size_t len)
{
	struct vscsi_ioc_data data;

	data.tag = tag;
	data.data = buf;
	data.datalen = len;

	if (ioctl(v.fd, req, &data) == -1)
		fatal("vscsi_data");
}

void
vscsi_status(int tag, int status, void *buf, size_t len)
{
	struct vscsi_ioc_t2i t2i;

	bzero(&t2i, sizeof(t2i));
	t2i.tag = tag;
	t2i.status = status;
	if (buf) {
		if (len > sizeof(t2i.sense))
			len = sizeof(t2i.sense);
		bcopy(buf, &t2i.sense, len);
	}

	if (ioctl(v.fd, VSCSI_T2I, &t2i) == -1)
		fatal("vscsi_status");
}

void
vscsi_event(unsigned long req, u_int target, u_int lun)
{
	struct vscsi_ioc_devevent devev;

	devev.target = target;
	devev.lun = lun;

	if (ioctl(v.fd, req, &devev) == -1)
		fatal("vscsi_event");
}

void
vscsi_callback(struct connection *c, void *arg, struct pdu *p)
{
	struct scsi_task *t = arg;
	struct iscsi_pdu_scsi_response *sresp;
	struct iscsi_pdu_rt2 *r2t;
	int status = VSCSI_STAT_DONE;
	u_char *buf = NULL;
	size_t size = 0, n;
	int tag;

	sresp = pdu_getbuf(p, NULL, PDU_HEADER);
	switch (ISCSI_PDU_OPCODE(sresp->opcode)) {
	case ISCSI_OP_SCSI_RESPONSE:
		task_cleanup(&t->task, c);
		tag = t->tag;
		free(t);

		if (!(sresp->flags & 0x80) || (sresp->flags & 0x06) == 0x06 ||
		    (sresp->flags & 0x18) == 0x18) {
			log_debug("vscsi_callback: bad scsi response");
			conn_fail(c);
			break;
		}
		/* XXX handle the various serial numbers */
		if (sresp->response) {
			status = VSCSI_STAT_ERR;
			goto send_status;
		}
		switch (sresp->status) {
		case ISCSI_SCSI_STAT_GOOD:
			break;
		case ISCSI_SCSI_STAT_CHCK_COND:
			status = VSCSI_STAT_SENSE;
			/* stupid encoding of sense data in the data segment */
			buf = pdu_getbuf(p, &n, PDU_DATA);
			if (buf) {
				size = buf[0] << 8 | buf[1];
				buf += 2;
			}
			break;
		default:
			status = VSCSI_STAT_ERR;
			break;
		}
send_status:
		vscsi_status(tag, status, buf, size);
		break;
	case ISCSI_OP_DATA_IN:
		buf = pdu_getbuf(p, &n, PDU_DATA);
		size = sresp->datalen[0] << 16 | sresp->datalen[1] << 8 |
		    sresp->datalen[2];
		if (size > n)
			fatal("This does not work as it should");
		vscsi_data(VSCSI_DATA_READ, t->tag, buf, size);
		if (sresp->flags & 1) {
			task_cleanup(&t->task, c);
			vscsi_status(t->tag, status, NULL, 0);
			free(t);
		}
		break;
	case ISCSI_OP_R2T:
		task_cleanup(&t->task, c);
		r2t = (struct iscsi_pdu_rt2 *)sresp;
		if (ntohl(r2t->buffer_offs))
			fatalx("vscsi: r2t bummer failure");
		size = ntohl(r2t->desired_datalen);

		vscsi_dataout(c->session, t, r2t->ttt, size);
		break;
	default:
		log_debug("scsi task: tag %d, target %d lun %d", t->tag,
		    t->target, t->lun);
		log_pdu(p, 1);
	}
	pdu_free(p);
}

void
vscsi_fail(void *arg)
{
	struct scsi_task *t = arg;
	int tag;

	task_cleanup(&t->task, NULL);
	tag = t->tag;
	free(t);
	vscsi_status(tag, VSCSI_STAT_RESET, NULL, 0);
}

void
vscsi_dataout(struct session *s, struct scsi_task *t, u_int32_t ttt, size_t len)
{
	struct pdu *p;
	struct iscsi_pdu_data_out *dout;
	u_char *buf = NULL;
	size_t size, off;
	u_int32_t t32, dsn = 0;

	for (off = 0; off < len; off += size) {
		/* XXX hardcoded numbers, bad bad bad */
		size = len - off > 8 * 1024 ? 8 * 1024 : len - off;

		if (!(p = pdu_new()))
			fatal("vscsi_r2t");
		if (!(dout = pdu_gethdr(p)))
			fatal("vscsi_r2t");

		dout->opcode = ISCSI_OP_DATA_OUT;
		if (off + size == len)
			dout->flags = 0x80; /* XXX magic value: F flag*/
		/* LUN handling: currently we only do single level LUNs < 256 */
		dout->lun[1] = t->lun;
		dout->ttt = ttt;
		dout->datasn = htonl(dsn++);
		t32 = htonl(size);
		bcopy(&t32, &dout->ahslen, sizeof(t32));

		dout->buffer_offs = htonl(off);
		if (!(buf = pdu_alloc(size)))
			fatal("vscsi_r2t");

		vscsi_data(VSCSI_DATA_WRITE, t->tag, buf, size);
		pdu_addbuf(p, buf, size, PDU_DATA);
		task_pdu_add(&t->task, p);
	}
	session_task_issue(s, &t->task);
}
