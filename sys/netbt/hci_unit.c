/*	$OpenBSD: hci_unit.c,v 1.2 2007/05/31 23:50:19 uwe Exp $	*/
/*	$NetBSD: hci_unit.c,v 1.4 2007/03/30 20:47:03 plunky Exp $	*/

/*-
 * Copyright (c) 2005 Iain Hibbert.
 * Copyright (c) 2006 Itronix Inc.
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
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/workq.h>

#include <net/netisr.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>

#define splraiseipl(ipl) splbio() /* XXX */

struct hci_unit_list hci_unit_list = TAILQ_HEAD_INITIALIZER(hci_unit_list);

/*
 * HCI Input Queue max lengths.
 */
int hci_eventq_max = 20;
int hci_aclrxq_max = 50;
int hci_scorxq_max = 50;

/*
 * bluetooth unit functions
 */

void hci_enable_task(void *, void *);
void hci_intr(void *);

struct workq *hci_workq;

void
hci_attach(struct hci_unit *unit)
{
	KASSERT(unit->hci_softc != NULL);
	KASSERT(unit->hci_devname != NULL);
	KASSERT(unit->hci_enable != NULL);
	KASSERT(unit->hci_disable != NULL);
	KASSERT(unit->hci_start_cmd != NULL);
	KASSERT(unit->hci_start_acl != NULL);
	KASSERT(unit->hci_start_sco != NULL);

	unit->hci_eventq.ifq_maxlen = hci_eventq_max;
	unit->hci_aclrxq.ifq_maxlen = hci_aclrxq_max;
	unit->hci_scorxq.ifq_maxlen = hci_scorxq_max;

	TAILQ_INIT(&unit->hci_links);
	LIST_INIT(&unit->hci_memos);

	TAILQ_INSERT_TAIL(&hci_unit_list, unit, hci_next);

	/* XXX enable HCI by default until we have userland tools */
	if (hci_workq == NULL)
		hci_workq = workq_create("hci_enable", 1);
	if (hci_workq != NULL)
		(void)workq_add_task(hci_workq, hci_enable_task, unit,
		    NULL, 0);
}

void
hci_detach(struct hci_unit *unit)
{
	hci_disable(unit);

	TAILQ_REMOVE(&hci_unit_list, unit, hci_next);
}

void
hci_enable_task(void *arg0, void *arg1)
{
	struct hci_unit *unit = arg0;

	if (!(unit->hci_flags & BTF_UP) && hci_enable(unit) == 0)
		unit->hci_flags |= BTF_UP;
}

int
hci_enable(struct hci_unit *unit)
{
	int s, err;

	/*
	 * Bluetooth spec says that a device can accept one
	 * command on power up until they send a Command Status
	 * or Command Complete event with more information, but
	 * it seems that some devices cant and prefer to send a
	 * No-op Command Status packet when they are ready, so
	 * we set this here and allow the driver (bt3c) to zero
	 * it.
	 */
	unit->hci_num_cmd_pkts = 1;
	unit->hci_num_acl_pkts = 0;
	unit->hci_num_sco_pkts = 0;

	/*
	 * only allow the basic packet types until
	 * the features report is in
	 */
	unit->hci_acl_mask = HCI_PKT_DM1 | HCI_PKT_DH1;
	unit->hci_packet_type = unit->hci_acl_mask;

	s = splraiseipl(unit->hci_ipl);
	err = (*unit->hci_enable)(unit);
	splx(s);
	if (err)
		goto bad1;

	/*
	 * Reset the device, this will trigger initialisation
	 * and wake us up.
	 */
	unit->hci_flags |= BTF_INIT;

	err = hci_send_cmd(unit, HCI_CMD_RESET, NULL, 0);
	if (err)
		goto bad2;

	while (unit->hci_flags & BTF_INIT) {
		err = tsleep(unit, PWAIT | PCATCH, __func__, 5 * hz);
		if (err)
			goto bad2;

		/* XXX
		 * "What If", while we were sleeping, the device
		 * was removed and detached? Ho Hum.
		 */
	}

	/*
	 * Attach Bluetooth Device Hub
	 */
	unit->hci_bthub = config_found(unit->hci_softc,
	    &unit->hci_bdaddr, NULL);

	return 0;

bad2:
	s = splraiseipl(unit->hci_ipl);
	(*unit->hci_disable)(unit);
	splx(s);

bad1:
	return err;
}

void
hci_disable(struct hci_unit *unit)
{
	struct hci_link *link, *next;
	struct hci_memo *memo;
	int s, acl;

	if (unit->hci_bthub) {
		config_detach(unit->hci_bthub, DETACH_FORCE);
		unit->hci_bthub = NULL;
	}

	s = splraiseipl(unit->hci_ipl);
	(*unit->hci_disable)(unit);
	splx(s);

	/*
	 * close down any links, take care to close SCO first since
	 * they may depend on ACL links.
	 */
	for (acl = 0 ; acl < 2 ; acl++) {
		next = TAILQ_FIRST(&unit->hci_links);
		while ((link = next) != NULL) {
			next = TAILQ_NEXT(link, hl_next);
			if (acl || link->hl_type != HCI_LINK_ACL)
				hci_link_free(link, ECONNABORTED);
		}
	}

	while ((memo = LIST_FIRST(&unit->hci_memos)) != NULL)
		hci_memo_free(memo);

	IF_PURGE(&unit->hci_eventq);
	unit->hci_eventqlen = 0;

	IF_PURGE(&unit->hci_aclrxq);
	unit->hci_aclrxqlen = 0;

	IF_PURGE(&unit->hci_scorxq);
	unit->hci_scorxqlen = 0;

	IF_PURGE(&unit->hci_cmdq);
	IF_PURGE(&unit->hci_cmdwait);
	IF_PURGE(&unit->hci_acltxq);
	IF_PURGE(&unit->hci_scotxq);
	IF_PURGE(&unit->hci_scodone);
}

struct hci_unit *
hci_unit_lookup(bdaddr_t *addr)
{
	struct hci_unit *unit;

	TAILQ_FOREACH(unit, &hci_unit_list, hci_next) {
		if ((unit->hci_flags & BTF_UP) == 0)
			continue;

		if (bdaddr_same(&unit->hci_bdaddr, addr))
			break;
	}

	return unit;
}

/*
 * construct and queue a HCI command packet
 */
int
hci_send_cmd(struct hci_unit *unit, uint16_t opcode, void *buf, uint8_t len)
{
	struct mbuf *m;
	hci_cmd_hdr_t *p;

	KASSERT(unit != NULL);

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;

	p = mtod(m, hci_cmd_hdr_t *);
	p->type = HCI_CMD_PKT;
	p->opcode = htole16(opcode);
	p->length = len;
	m->m_pkthdr.len = m->m_len = sizeof(hci_cmd_hdr_t);

	if (len) {
		KASSERT(buf != NULL);

		m_copyback(m, sizeof(hci_cmd_hdr_t), len, buf);
		if (m->m_pkthdr.len != (sizeof(hci_cmd_hdr_t) + len)) {
			m_freem(m);
			return ENOMEM;
		}
	}

	DPRINTFN(2, "(%s) opcode (%3.3x|%4.4x)\n", unit->hci_devname,
		HCI_OGF(opcode), HCI_OCF(opcode));

	/* and send it on */
	if (unit->hci_num_cmd_pkts == 0)
		IF_ENQUEUE(&unit->hci_cmdwait, m);
	else
		hci_output_cmd(unit, m);

	return 0;
}

/*
 * Incoming packet processing. Since the code is single threaded
 * in any case (IPL_SOFTNET), we handle it all in one interrupt function
 * picking our way through more important packets first so that hopefully
 * we will never get clogged up with bulk data.
 */
void
hci_intr(void *arg)
{
	struct hci_unit *unit = arg;
	struct mbuf *m;
	int s;

another:
	s = splraiseipl(unit->hci_ipl);

	if (unit->hci_eventqlen > 0) {
		IF_DEQUEUE(&unit->hci_eventq, m);
		unit->hci_eventqlen--;
		KASSERT(m != NULL);
		splx(s);

		DPRINTFN(10, "(%s) recv event, len = %d\n",
				unit->hci_devname, m->m_pkthdr.len);

		m->m_flags |= M_LINK0;	/* mark incoming packet */
		hci_mtap(m, unit);
		hci_event(m, unit);

		goto another;
	}

	if (unit->hci_scorxqlen > 0) {
		IF_DEQUEUE(&unit->hci_scorxq, m);
		unit->hci_scorxqlen--;
		KASSERT(m != NULL);
		splx(s);

		DPRINTFN(10, "(%s) recv SCO, len = %d\n",
				unit->hci_devname, m->m_pkthdr.len);

		m->m_flags |= M_LINK0;	/* mark incoming packet */
		hci_mtap(m, unit);
		hci_sco_recv(m, unit);

		goto another;
	}

	if (unit->hci_aclrxqlen > 0) {
		IF_DEQUEUE(&unit->hci_aclrxq, m);
		unit->hci_aclrxqlen--;
		KASSERT(m != NULL);
		splx(s);

		DPRINTFN(10, "(%s) recv ACL, len = %d\n",
				unit->hci_devname, m->m_pkthdr.len);

		m->m_flags |= M_LINK0;	/* mark incoming packet */
		hci_mtap(m, unit);
		hci_acl_recv(m, unit);

		goto another;
	}

	IF_DEQUEUE(&unit->hci_scodone, m);
	if (m != NULL) {
		struct hci_link *link;
		splx(s);

		DPRINTFN(11, "(%s) complete SCO\n",
				unit->hci_devname);

		TAILQ_FOREACH(link, &unit->hci_links, hl_next) {
#ifdef notyet			/* XXX */
			if (link == M_GETCTX(m, struct hci_link *)) {
				hci_sco_complete(link, 1);
				break;
			}
#endif
		}

		unit->hci_num_sco_pkts++;
		m_freem(m);

		goto another;
	}

	splx(s);

	DPRINTFN(10, "done\n");
}

/**********************************************************************
 *
 * IO routines
 *
 * input & complete routines will be called from device driver
 * (at unit->hci_ipl)
 */

void
hci_input_event(struct hci_unit *unit, struct mbuf *m)
{
	if (unit->hci_eventqlen > hci_eventq_max) {
		DPRINTF("(%s) dropped event packet.\n", unit->hci_devname);
		unit->hci_stats.err_rx++;
		m_freem(m);
	} else {
		unit->hci_eventqlen++;
		IF_ENQUEUE(&unit->hci_eventq, m);
		schednetisr(NETISR_BT);
	}
}

void
hci_input_acl(struct hci_unit *unit, struct mbuf *m)
{
	if (unit->hci_aclrxqlen > hci_aclrxq_max) {
		DPRINTF("(%s) dropped ACL packet.\n", unit->hci_devname);
		unit->hci_stats.err_rx++;
		m_freem(m);
	} else {
		unit->hci_aclrxqlen++;
		IF_ENQUEUE(&unit->hci_aclrxq, m);
		schednetisr(NETISR_BT);
	}
}

void
hci_input_sco(struct hci_unit *unit, struct mbuf *m)
{
	if (unit->hci_scorxqlen > hci_scorxq_max) {
		DPRINTF("(%s) dropped SCO packet.\n", unit->hci_devname);
		unit->hci_stats.err_rx++;
		m_freem(m);
	} else {
		unit->hci_scorxqlen++;
		IF_ENQUEUE(&unit->hci_scorxq, m);
		schednetisr(NETISR_BT);
	}
}

void
hci_output_cmd(struct hci_unit *unit, struct mbuf *m)
{
#ifdef notyet
	void *arg;
#endif
	int s;

	hci_mtap(m, unit);

	DPRINTFN(10, "(%s) num_cmd_pkts=%d\n", unit->hci_devname,
					       unit->hci_num_cmd_pkts);

	unit->hci_num_cmd_pkts--;

	/*
	 * If context is set, this was from a HCI raw socket
	 * and a record needs to be dropped from the sockbuf.
	 */
#ifdef notyet			/* XXX */
	arg = M_GETCTX(m, void *);
	if (arg != NULL)
		hci_drop(arg);
#endif

	s = splraiseipl(unit->hci_ipl);
	IF_ENQUEUE(&unit->hci_cmdq, m);
	if ((unit->hci_flags & BTF_XMIT_CMD) == 0)
		(*unit->hci_start_cmd)(unit);

	splx(s);
}

void
hci_output_acl(struct hci_unit *unit, struct mbuf *m)
{
	int s;

	hci_mtap(m, unit);

	DPRINTFN(10, "(%s) num_acl_pkts=%d\n", unit->hci_devname,
					       unit->hci_num_acl_pkts);

	unit->hci_num_acl_pkts--;

	s = splraiseipl(unit->hci_ipl);
	IF_ENQUEUE(&unit->hci_acltxq, m);
	if ((unit->hci_flags & BTF_XMIT_ACL) == 0)
		(*unit->hci_start_acl)(unit);

	splx(s);
}

void
hci_output_sco(struct hci_unit *unit, struct mbuf *m)
{
	int s;

	hci_mtap(m, unit);

	DPRINTFN(10, "(%s) num_sco_pkts=%d\n", unit->hci_devname,
					       unit->hci_num_sco_pkts);

	unit->hci_num_sco_pkts--;

	s = splraiseipl(unit->hci_ipl);
	IF_ENQUEUE(&unit->hci_scotxq, m);
	if ((unit->hci_flags & BTF_XMIT_SCO) == 0)
		(*unit->hci_start_sco)(unit);

	splx(s);
}

void
hci_complete_sco(struct hci_unit *unit, struct mbuf *m)
{
	IF_ENQUEUE(&unit->hci_scodone, m);
	schednetisr(NETISR_BT);
}
