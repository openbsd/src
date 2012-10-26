/*	$OpenBSD: ds.c,v 1.5 2012/10/26 18:10:03 kettenis Exp $	*/

/*
 * Copyright (c) 2012 Mark Kettenis
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
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ds.h"
#include "util.h"

void	pri_start(struct ldc_conn *, uint64_t);
void	pri_rx_data(struct ldc_conn *, uint64_t, void *, size_t);

void	mdstore_start(struct ldc_conn *, uint64_t);
void	mdstore_rx_data(struct ldc_conn *, uint64_t, void *, size_t);

struct ds_service ds_service[] = {
	{ "pri", 1, 0, pri_start, pri_rx_data },
#if 0
	{ "mdstore", 1, 0, mdstore_start, mdstore_rx_data },
#endif
	{ NULL, 0, 0 }
};

void	ldc_rx_ctrl_vers(struct ldc_conn *, struct ldc_pkt *);
void	ldc_rx_ctrl_rtr(struct ldc_conn *, struct ldc_pkt *);
void	ldc_rx_ctrl_rts(struct ldc_conn *, struct ldc_pkt *);
void	ldc_rx_ctrl_rdx(struct ldc_conn *, struct ldc_pkt *);

void	ldc_send_ack(struct ldc_conn *);
void	ldc_send_rtr(struct ldc_conn *);
void	ldc_send_rts(struct ldc_conn *);
void	ldc_send_rdx(struct ldc_conn *);

void
ldc_rx_ctrl(struct ldc_conn *lc, struct ldc_pkt *lp)
{
	switch (lp->ctrl) {
	case LDC_VERS:
		ldc_rx_ctrl_vers(lc, lp);
		break;

	case LDC_RTS:
		ldc_rx_ctrl_rts(lc, lp);
		break;

	case LDC_RTR:
		ldc_rx_ctrl_rtr(lc, lp);
		break;

	case LDC_RDX:
		ldc_rx_ctrl_rdx(lc, lp);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/0x%02x\n", lp->stype, lp->ctrl));
		ldc_reset(lc);
		break;
	}
}

void
ldc_rx_ctrl_vers(struct ldc_conn *lc, struct ldc_pkt *lp)
{
	struct ldc_pkt *lvp = (struct ldc_pkt *)lp;

	switch (lp->stype) {
	case LDC_INFO:
		if (lc->lc_state == LDC_RCV_VERS) {
			DPRINTF(("Spurious CTRL/INFO/VERS: state %d\n",
			    lc->lc_state));
			return;
		}
		DPRINTF(("CTRL/INFO/VERS\n"));
		if (lvp->major == LDC_VERSION_MAJOR &&
		    lvp->minor == LDC_VERSION_MINOR)
			ldc_send_ack(lc);
		else
			/* XXX do nothing for now. */
			;
		break;

	case LDC_ACK:
		if (lc->lc_state != LDC_SND_VERS) {
			DPRINTF(("Spurious CTRL/ACK/VERS: state %d\n",
			    lc->lc_state));
			ldc_reset(lc);
			return;
		}
		DPRINTF(("CTRL/ACK/VERS\n"));
		ldc_send_rts(lc);
		break;

	case LDC_NACK:
		DPRINTF(("CTRL/NACK/VERS\n"));
		ldc_reset(lc);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/VERS\n", lp->stype));
		ldc_reset(lc);
		break;
	}
}

void
ldc_rx_ctrl_rts(struct ldc_conn *lc, struct ldc_pkt *lp)
{
	switch (lp->stype) {
	case LDC_INFO:
		if (lc->lc_state != LDC_RCV_VERS) {
			DPRINTF(("Spurious CTRL/INFO/RTS: state %d\n",
			    lc->lc_state));
			ldc_reset(lc);
			return;
		}
		DPRINTF(("CTRL/INFO/RTS\n"));
		if (lp->env != LDC_MODE_RELIABLE) {
			ldc_reset(lc);
			return;
		}
		ldc_send_rtr(lc);
		break;

	case LDC_ACK:
		DPRINTF(("CTRL/ACK/RTS\n"));
		ldc_reset(lc);
		break;

	case LDC_NACK:
		DPRINTF(("CTRL/NACK/RTS\n"));
		ldc_reset(lc);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/RTS\n", lp->stype));
		ldc_reset(lc);
		break;
	}
}

void
ldc_rx_ctrl_rtr(struct ldc_conn *lc, struct ldc_pkt *lp)
{
	switch (lp->stype) {
	case LDC_INFO:
		if (lc->lc_state != LDC_SND_RTS) {
			DPRINTF(("Spurious CTRL/INFO/RTR: state %d\n",
			    lc->lc_state));
			ldc_reset(lc);
			return;
		}
		DPRINTF(("CTRL/INFO/RTR\n"));
		if (lp->env != LDC_MODE_RELIABLE) {
			ldc_reset(lc);
			return;
		}
		ldc_send_rdx(lc);
#if 0
		lc->lc_start(lc);
#endif
		break;

	case LDC_ACK:
		DPRINTF(("CTRL/ACK/RTR\n"));
		ldc_reset(lc);
		break;

	case LDC_NACK:
		DPRINTF(("CTRL/NACK/RTR\n"));
		ldc_reset(lc);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/RTR\n", lp->stype));
		ldc_reset(lc);
		break;
	}
}

void
ldc_rx_ctrl_rdx(struct ldc_conn *lc, struct ldc_pkt *lp)
{
	switch (lp->stype) {
	case LDC_INFO:
		if (lc->lc_state != LDC_SND_RTR) {
			DPRINTF(("Spurious CTRL/INFO/RTR: state %d\n",
			    lc->lc_state));
			ldc_reset(lc);
			return;
		}
		DPRINTF(("CTRL/INFO/RDX\n"));
#if 0
		lc->lc_start(lc);
#endif
		break;

	case LDC_ACK:
		DPRINTF(("CTRL/ACK/RDX\n"));
		ldc_reset(lc);
		break;

	case LDC_NACK:
		DPRINTF(("CTRL/NACK/RDX\n"));
		ldc_reset(lc);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/RDX\n", lp->stype));
		ldc_reset(lc);
		break;
	}
}

void
ldc_rx_data(struct ldc_conn *lc, struct ldc_pkt *lp)
{
	size_t len;

	if (lp->stype != LDC_INFO && lp->stype != LDC_ACK) {
		DPRINTF(("DATA/0x%02x\n", lp->stype));
		ldc_reset(lc);
		return;
	}

	if (lc->lc_state != LDC_SND_RTR &&
	    lc->lc_state != LDC_SND_RDX) {
		DPRINTF(("Spurious DATA/INFO: state %d\n", lc->lc_state));
		ldc_reset(lc);
		return;
	}

	if (lp->ackid) {
		int i;

		for (i = 0; ds_service[i].ds_svc_id; i++) {
			if (ds_service[i].ds_ackid &&
			    lp->ackid >= ds_service[i].ds_ackid) {
				ds_service[i].ds_ackid = 0;
				ds_service[i].ds_start(lc, ds_service[i].ds_svc_handle);
			}
		}
	}
	if (lp->stype == LDC_ACK)
		return;

	if (lp->env & LDC_FRAG_START) {
		lc->lc_len = (lp->env & LDC_LEN_MASK);
		memcpy((uint8_t *)lc->lc_msg, &lp->data, lc->lc_len);
	} else {
		len = (lp->env & LDC_LEN_MASK);
		if (lc->lc_len + len > sizeof(lc->lc_msg)) {
			DPRINTF(("Buffer overrun\n"));
			ldc_reset(lc);
			return;
		}
		memcpy((uint8_t *)lc->lc_msg + lc->lc_len, &lp->data, len);
		lc->lc_len += len;
	}

	if (lp->env & LDC_FRAG_STOP) {
		ldc_ack(lc, lp->seqid);
		lc->lc_rx_data(lc, lc->lc_msg, lc->lc_len);
	}
}

void
ldc_send_vers(struct ldc_conn *lc)
{
	struct ldc_pkt lp;
	ssize_t nbytes;

	bzero(&lp, sizeof(lp));
	lp.type = LDC_CTRL;
	lp.stype = LDC_INFO;
	lp.ctrl = LDC_VERS;
	lp.major = 1;
	lp.minor = 0;

	nbytes = write(lc->lc_fd, &lp, sizeof(lp));
	if (nbytes != sizeof(lp))
		err(1, "write");

	lc->lc_state = LDC_SND_VERS;
}

void
ldc_send_ack(struct ldc_conn *lc)
{
	struct ldc_pkt lp;
	ssize_t nbytes;

	bzero(&lp, sizeof(lp));
	lp.type = LDC_CTRL;
	lp.stype = LDC_ACK;
	lp.ctrl = LDC_VERS;
	lp.major = 1;
	lp.minor = 0;

	nbytes = write(lc->lc_fd, &lp, sizeof(lp));
	if (nbytes != sizeof(lp))
		err(1, "write");

	lc->lc_state = LDC_RCV_VERS;
}

void
ldc_send_rts(struct ldc_conn *lc)
{
	struct ldc_pkt lp;
	ssize_t nbytes;

	bzero(&lp, sizeof(lp));
	lp.type = LDC_CTRL;
	lp.stype = LDC_INFO;
	lp.ctrl = LDC_RTS;
	lp.env = LDC_MODE_RELIABLE;
	lp.seqid = lc->lc_tx_seqid++;

	nbytes = write(lc->lc_fd, &lp, sizeof(lp));
	if (nbytes != sizeof(lp))
		err(1, "write");

	lc->lc_state = LDC_SND_RTS;
}

void
ldc_send_rtr(struct ldc_conn *lc)
{
	struct ldc_pkt lp;
	ssize_t nbytes;

	bzero(&lp, sizeof(lp));
	lp.type = LDC_CTRL;
	lp.stype = LDC_INFO;
	lp.ctrl = LDC_RTR;
	lp.env = LDC_MODE_RELIABLE;
	lp.seqid = lc->lc_tx_seqid++;

	nbytes = write(lc->lc_fd, &lp, sizeof(lp));
	if (nbytes != sizeof(lp))
		err(1, "write");

	lc->lc_state = LDC_SND_RTR;
}

void
ldc_send_rdx(struct ldc_conn *lc)
{
	struct ldc_pkt lp;
	ssize_t nbytes;

	bzero(&lp, sizeof(lp));
	lp.type = LDC_CTRL;
	lp.stype = LDC_INFO;
	lp.ctrl = LDC_RDX;
	lp.env = LDC_MODE_RELIABLE;
	lp.seqid = lc->lc_tx_seqid++;

	nbytes = write(lc->lc_fd, &lp, sizeof(lp));
	if (nbytes != sizeof(lp))
		err(1, "write");

	lc->lc_state = LDC_SND_RDX;
}

void
ldc_reset(struct ldc_conn *lc)
{
	lc->lc_tx_seqid = 0;
	lc->lc_state = 0;
#if 0
	lc->lc_reset(lc);
#endif
}

void
ldc_ack(struct ldc_conn *lc, uint32_t ackid)
{
	struct ldc_pkt lp;
	ssize_t nbytes;

	bzero(&lp, sizeof(lp));
	lp.type = LDC_DATA;
	lp.stype = LDC_ACK;
	lp.seqid = lc->lc_tx_seqid++;
	lp.ackid = ackid;
	nbytes = write(lc->lc_fd, &lp, sizeof(lp));
	if (nbytes != sizeof(lp))
		err(1, "write");
}

void
ds_rx_msg(struct ldc_conn *lc, void *data, size_t len)
{
	struct ds_msg *dm = data;

	switch(dm->msg_type) {
	case DS_INIT_REQ:
	{
		struct ds_init_req *dr = data;

		DPRINTF(("DS_INIT_REQ %d.%d\n", dr->major_vers,
		    dr->minor_vers));
		if (dr->major_vers != 1 || dr->minor_vers != 0){
			ldc_reset(lc);
			return;
		}
		ds_init_ack(lc);
		break;
	}

	case DS_REG_REQ:
	{
		struct ds_reg_req *dr = data;
		int i;

		DPRINTF(("DS_REG_REQ %s %d.%d 0x%016llx\n", dr->svc_id,
		    dr->major_vers, dr->minor_vers, dr->svc_handle));
		for (i = 0; ds_service[i].ds_svc_id; i++)
			if (strcmp(dr->svc_id, ds_service[i].ds_svc_id) == 0) {
				ds_service[i].ds_svc_handle = dr->svc_handle;
				ds_service[i].ds_ackid = lc->lc_tx_seqid;
				ds_reg_ack(lc, dr->svc_handle);
				return;
			}

		ds_reg_nack(lc, dr->svc_handle);
		break;
	}

	case DS_DATA:
	{
		struct ds_data *dd = data;
		int i;

		DPRINTF(("DS_DATA 0x%016llx\n", dd->svc_handle));
		for (i = 0; ds_service[i].ds_svc_id; i++)
			if (ds_service[i].ds_svc_handle == dd->svc_handle)
				ds_service[i].ds_rx_data(lc, dd->svc_handle,
				    data, len);
		break;
	}

	default:
		DPRINTF(("Unknown DS message type 0x%x\n", dm->msg_type));
		ldc_reset(lc);
		break;
	}
}

void
ds_init_ack(struct ldc_conn *lc)
{
	struct ds_init_ack da;

	bzero(&da, sizeof(da));
	da.msg_type = DS_INIT_ACK;
	da.payload_len = sizeof(da) - 8;
	da.minor_vers = 0;
	ds_send_msg(lc, &da, sizeof(da));
}

void
ds_reg_ack(struct ldc_conn *lc, uint64_t svc_handle)
{
	struct ds_reg_ack da;

	bzero(&da, sizeof(da));
	da.msg_type = DS_REG_ACK;
	da.payload_len = sizeof(da) - 8;
	da.svc_handle = svc_handle;
	da.minor_vers = 0;
	ds_send_msg(lc, &da, sizeof(da));
}

void
ds_reg_nack(struct ldc_conn *lc, uint64_t svc_handle)
{
	struct ds_reg_nack dn;

	bzero(&dn, sizeof(dn));
	dn.msg_type = DS_REG_NACK;
	dn.payload_len = sizeof(dn) - 8;
	dn.svc_handle = svc_handle;
	dn.result = DS_REG_VER_NACK;
	dn.major_vers = 0;
	ds_send_msg(lc, &dn, sizeof(dn));
}

void
ds_receive_msg(struct ldc_conn *lc, void *buf, size_t len)
{
	int env = LDC_FRAG_START;
	struct ldc_pkt lp;
	uint8_t *p = buf;
	ssize_t nbytes;

	while (len > 0) {
		nbytes = read(lc->lc_fd, &lp, sizeof(lp));
		if (nbytes != sizeof(lp))
			err(1, "read");

		if (lp.type != LDC_DATA &&
		    lp.stype != LDC_INFO) {
			ldc_reset(lc);
			return;
		}

		if ((lp.env & LDC_FRAG_START) != env) {
			ldc_reset(lc);
			return;
		}

		bcopy(&lp.data, p, (lp.env & LDC_LEN_MASK));
		p += (lp.env & LDC_LEN_MASK);
		len -= (lp.env & LDC_LEN_MASK);

		if (lp.env & LDC_FRAG_STOP)
			ldc_ack(lc, lp.seqid);

		env = (lp.env & LDC_FRAG_STOP) ? LDC_FRAG_START : 0;
	}
}

void
ldc_send_msg(struct ldc_conn *lc, void *buf, size_t len)
{
	struct ldc_pkt lp;
	uint8_t *p = buf;
	ssize_t nbytes;

	while (len > 0) {
		bzero(&lp, sizeof(lp));
		lp.type = LDC_DATA;
		lp.stype = LDC_INFO;
		lp.env = min(len, LDC_PKT_PAYLOAD);
		if (p == buf)
			lp.env |= LDC_FRAG_START;
		if (len <= LDC_PKT_PAYLOAD)
			lp.env |= LDC_FRAG_STOP;
		lp.seqid = lc->lc_tx_seqid++;
		bcopy(p, &lp.data, min(len, LDC_PKT_PAYLOAD));

		nbytes = write(lc->lc_fd, &lp, sizeof(lp));
		if (nbytes != sizeof(lp))
			err(1, "write");
		p += min(len, LDC_PKT_PAYLOAD);
		len -= min(len, LDC_PKT_PAYLOAD);
	}
}

void
ds_send_msg(struct ldc_conn *lc, void *buf, size_t len)
{
	uint8_t *p = buf;
#if 0
	struct ldc_pkt lp;
	ssize_t nbytes;
#endif

	while (len > 0) {
		ldc_send_msg(lc, p, min(len, LDC_MSG_MAX));
		p += min(len, LDC_MSG_MAX);
		len -= min(len, LDC_MSG_MAX);

#if 0
		/* Consume ACK. */
		nbytes = read(lc->lc_fd, &lp, sizeof(lp));
		if (nbytes != sizeof(lp))
			err(1, "read");

	{
		uint64_t *msg = (uint64_t *)&lp;
		int i;

		for (i = 0; i < 8; i++)
			printf("%02x: %016llx\n", i, msg[i]);
	}
#endif
	}
}

#define PRI_REQUEST	0x00

struct pri_msg {
	uint32_t	msg_type;
	uint32_t	payload_len;
	uint64_t	svc_handle;
	uint64_t	reqnum;
	uint64_t	type;
} __packed;

#define PRI_DATA	0x01

struct pri_data {
	uint32_t	msg_type;
	uint32_t	payload_len;
	uint64_t	svc_handle;
	uint64_t	reqnum;
	uint64_t	type;
	char		data[1];
} __packed;

#define PRI_UPDATE	0x02

struct pri_update {
	uint32_t	msg_type;
	uint32_t	payload_len;
	uint64_t	svc_handle;
	uint64_t	reqnum;
	uint64_t	type;
} __packed;

void
pri_start(struct ldc_conn *lc, uint64_t svc_handle)
{
	struct pri_msg pm;

	bzero(&pm, sizeof(pm));
	pm.msg_type = DS_DATA;
	pm.payload_len = sizeof(pm) - 8;
	pm.svc_handle = svc_handle;
	pm.reqnum = 0;
	pm.type = PRI_REQUEST;
	ds_send_msg(lc, &pm, sizeof(pm));
}

void *pri_buf;
size_t pri_len;

void
pri_rx_data(struct ldc_conn *lc, uint64_t svc_handle, void *data, size_t len)
{
	struct pri_data *pd = data;

	if (pd->type != PRI_DATA) {
		DPRINTF(("Unexpected PRI message type 0x%02llx\n", pd->type));
		return;
	}

	pri_len = pd->payload_len - 24;
	pri_buf = xmalloc(pri_len);

	len -= sizeof(struct pri_msg);
	bcopy(&pd->data, pri_buf, len);
	ds_receive_msg(lc, pri_buf + len, pri_len - len);
}

#define MDSET_LIST_REQUEST	0x0004

struct mdstore_msg {
	uint32_t	msg_type;
	uint32_t	payload_len;
	uint64_t	svc_handle;
	uint64_t	reqnum;
	uint16_t	command;
} __packed;

#define MDSET_LIST_REPLY	0x0104

struct mdstore_list_resp {
	uint32_t	msg_type;
	uint32_t	payload_len;
	uint64_t	svc_handle;
	uint64_t	reqnum;
	uint32_t	result;
	uint16_t	booted_set;
	uint16_t	boot_set;
	char		sets[1];
} __packed;

#define MDST_SUCCESS		0x0
#define MDST_FAILURE		0x1
#define MDST_INVALID_MSG	0x2
#define MDST_MAX_MDS_ERR	0x3
#define MDST_BAD_NAME_ERR	0x4
#define MDST_SET_EXISTS_ERR	0x5
#define MDST_ALLOC_SET_ERR	0x6
#define MDST_ALLOC_MD_ERR	0x7
#define MDST_MD_COUNT_ERR	0x8
#define MDST_MD_SIZE_ERR	0x9
#define MDST_MD_TYPE_ERR	0xa
#define MDST_NOT_EXIST_ERR	0xb

void
mdstore_start(struct ldc_conn *lc, uint64_t svc_handle)
{
	struct mdstore_msg mm;

	bzero(&mm, sizeof(mm));
	mm.msg_type = DS_DATA;
	mm.payload_len = sizeof(mm) - 8;
	mm.svc_handle = svc_handle;
	mm.reqnum = 0;
	mm.command = MDSET_LIST_REQUEST;
	ds_send_msg(lc, &mm, sizeof(mm));
}

void
mdstore_rx_data(struct ldc_conn *lc, uint64_t svc_handle, void *data,
    size_t len)
{
	struct mdstore_list_resp *mr = data;
	int idx = 0;

	if (mr->result != MDST_SUCCESS) {
		DPRINTF(("Unexpected result 0x%x\n", mr->result));
		return;
	}

	len = 0;
	for (idx = 0; len < mr->payload_len - 24; idx++) {
		printf("%s", &mr->sets[len]);
		if (idx == mr->booted_set)
			printf(" [current]");
		else if (idx == mr->boot_set)
			printf(" [next]");
		printf("\n");
		len += strlen(&mr->sets[len]) + 1;
	}

	exit(0);
}
