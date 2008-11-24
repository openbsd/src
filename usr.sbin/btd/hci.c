/*	$OpenBSD: hci.c,v 1.1 2008/11/24 23:34:42 uwe Exp $	*/
/*	$NetBSD: btconfig.c,v 1.13 2008/07/21 13:36:57 lukem Exp $	*/

/*-
 * Copyright (c) 2008 Uwe Stuehler <uwe@openbsd.org>
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
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

#include <sys/ioctl.h>

#include <assert.h>
#include <bluetooth.h>
#include <errno.h>
#include <event.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "btd.h"

int hci_init_config(struct bt_interface *);

void hci_eventcb(int, short, void *);

int hci_process_pin_code_req(struct bt_interface *,
    struct sockaddr_bt *, const bdaddr_t *);
int hci_process_link_key_req(struct bt_interface *,
    struct sockaddr_bt *, const bdaddr_t *);
int hci_process_link_key_notification(struct bt_interface *, struct sockaddr_bt *,
    const hci_link_key_notification_ep *);
int hci_req(int, uint16_t, uint8_t , void *, size_t, void *, size_t);
int hci_send_cmd(int, struct sockaddr_bt *, uint16_t, size_t, void *);

#define hci_write(iface, opcode, wbuf, wlen) \
	hci_req(iface->fd, opcode, 0, wbuf, wlen, NULL, 0)
#define hci_read(iface, opcode, rbuf, rlen) \
	hci_req(iface->fd, opcode, 0, NULL, 0, rbuf, rlen)
#define hci_cmd(iface, opcode, cbuf, clen) \
	hci_req(iface->fd, opcode, 0, cbuf, clen, NULL, 0)

int
hci_init(struct btd *env)
{
	struct btreq btr;
	struct bt_interface *defaults;
	struct bt_interface *iface;
	int hci;

	hci = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (hci == -1)
		fatal("could not open raw HCI socket");

	defaults = conf_find_interface(env, BDADDR_ANY);
	iface = NULL;

	bzero(&btr, sizeof(btr));
	for (;;) {
		int flags;

		if (ioctl(hci, SIOCNBTINFO, &btr) == -1)
			break;

		/*
		 * Interfaces must be up, just to determine their
		 * bdaddr. Grrrr...
		 */
		if (!((flags = btr.btr_flags) & BTF_UP)) {
			btr.btr_flags |= BTF_UP;
			if (ioctl(hci, SIOCSBTFLAGS, &btr) == -1) {
				log_warn("%s: SIOCSBTFLAGS", btr.btr_name);
				continue;
			}
			if (ioctl(hci, SIOCGBTINFO, &btr) == -1) {
				log_warn("%s: SIOCGBTINFO", btr.btr_name);
				continue;
			}
		}

		if (!(btr.btr_flags & BTF_UP) ||
		    bdaddr_any(&btr.btr_bdaddr)) {
			log_warn("could not enable %s", btr.btr_name);
			goto redisable;
		}

		/*
		 * Discover device driver unit names for explicitly
		 * configured interfaces.
		 */
		iface = conf_find_interface(env, &btr.btr_bdaddr);
		if (iface != NULL) {
			assert(iface->xname == NULL);
			iface->xname = strdup(btr.btr_name);
			if (iface->xname == NULL)
				fatal("hci_init strdup 1");
			goto redisable;
		}

		/*
		 * Add interfaces not explicitly configured.
		 */
		iface = conf_add_interface(env, &btr.btr_bdaddr);
		if (iface == NULL)
			fatalx("hci_init add_interface");

		iface->xname = strdup(btr.btr_name);
		if (iface->xname == NULL)
			fatal("hci_init strdup 2");

	redisable:
		/* See above. Grrrr... */
		if (!(flags & BTF_UP) &&
		    (iface == NULL || iface->disabled)) {
			btr.btr_flags &= ~BTF_UP;
			if (ioctl(hci, SIOCSBTFLAGS, &btr) == -1)
				fatal("hci_init SIOCSBTFLAGS");
		}
	}

	close(hci);

	TAILQ_FOREACH(iface, &env->interfaces, entry) {
		struct sockaddr_bt sa;
		struct hci_filter filter;

		if (bdaddr_any(&iface->addr))
			continue;

		/* Disable interfaces not present in the system. */
		if (iface->xname == NULL) {
			iface->disabled = 1;
			log_info("interface disabled: %s (not present)",
			    bt_ntoa(&iface->addr, NULL));
			continue;
		}

		/* Skip disabled interfaces. */
		if (iface->disabled) {
			log_info("interface disabled: %s (%s)",
			    bt_ntoa(&iface->addr, NULL), iface->xname);
			continue;
		}

		log_info("listening on %s (%s)",
		    bt_ntoa(&iface->addr, NULL), iface->xname);

		iface->fd = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
		if (iface->fd == -1)
			fatal("socket");

		bzero(&sa, sizeof(sa));
		sa.bt_len = sizeof(sa);
		sa.bt_family = AF_BLUETOOTH;
		bdaddr_copy(&sa.bt_bdaddr, &iface->addr);
		if (bind(iface->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
			fatal("bind");

		if (connect(iface->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
			fatal("connect");

		if (hci_init_config(iface) == -1)
			fatalx("hci_init_config");

		bzero(&filter, sizeof(filter));
		hci_filter_set(HCI_EVENT_PIN_CODE_REQ, &filter);
		hci_filter_set(HCI_EVENT_LINK_KEY_REQ, &filter);
		hci_filter_set(HCI_EVENT_LINK_KEY_NOTIFICATION, &filter);
		if (setsockopt(iface->fd, BTPROTO_HCI, SO_HCI_EVT_FILTER,
		    (const void *)&filter, sizeof(filter)) < 0)
			fatal("setsockopt");

		event_set(&iface->ev, iface->fd, EV_READ | EV_PERSIST,
		    hci_eventcb, iface);
		event_add(&iface->ev, NULL);
	}

	/*
	 * Run device discovery on the first available
	 * interface. This should be configurable.
	 */
	TAILQ_FOREACH(iface, &env->interfaces, entry) {
		if (!bdaddr_any(&iface->addr) && !iface->disabled) {
			env->hci.inquiry_interface = iface;
			break;
		}
	}

	return 0;
}

int
hci_init_config(struct bt_interface *iface)
{
	struct btreq btr;
	struct btd *env = iface->env;
	struct bt_interface *defaults;
	const char *name;
	uint8_t val;

	defaults = conf_find_interface(env, BDADDR_ANY);

	if (iface->name != NULL)
		name = iface->name;
	else if (defaults != NULL && defaults->name != NULL)
		name = defaults->name;
	else
		name = NULL;

	if (name != NULL && hci_write(iface, HCI_CMD_WRITE_LOCAL_NAME,
	    (char *)name, HCI_UNIT_NAME_SIZE))
		return -1;

	if (hci_read(iface, HCI_CMD_READ_SCAN_ENABLE, &val, sizeof(val)))
		return -1;

	val |= HCI_PAGE_SCAN_ENABLE;
	val |= HCI_INQUIRY_SCAN_ENABLE;

	if (hci_write(iface, HCI_CMD_WRITE_SCAN_ENABLE, &val, sizeof(val)))
		return -1;

	bdaddr_copy(&btr.btr_bdaddr, &iface->addr);
	if (ioctl(iface->fd, SIOCGBTINFOA, &btr) < 0) {
		log_warn("SIOCGBTINFOA");
		return -1;
	}

	val = btr.btr_link_policy;
	val |= HCI_LINK_POLICY_ENABLE_ROLE_SWITCH;
	btr.btr_link_policy = val;

	if (ioctl(iface->fd, SIOCSBTPOLICY, &btr) < 0) {
		log_warn("SIOCSBTPOLICY");
		return -1;
	}

	return 0;
}

void
hci_eventcb(int fd, short evflags, void *arg)
{
	char buf[HCI_EVENT_PKT_SIZE];
	hci_event_hdr_t *event = (hci_event_hdr_t *)buf;
	struct bt_interface *iface = arg;
	struct sockaddr_bt sa;
	socklen_t size;
	bdaddr_t *addr;
	void *ep;
	int n;

	if (iface == NULL)
		fatal("HCI event on closed socket?");

	size = sizeof(sa);
	n = recvfrom(iface->fd, buf, sizeof(buf), 0,
	    (struct sockaddr *)&sa, &size);
	if (n < 0) {
		log_warn("could not receive from HCI socket");
		return;
	}

	if (event->type != HCI_EVENT_PKT) {
		log_packet(&sa.bt_bdaddr, &iface->addr,
		    "unexpected HCI packet, type=%#x", event->type);
		return;
	}

	addr = (bdaddr_t *)(event + 1);
	ep = (bdaddr_t *)(event + 1);

	switch (event->event) {
	case HCI_EVENT_PIN_CODE_REQ:
		hci_process_pin_code_req(iface, &sa, addr);
		break;

	case HCI_EVENT_LINK_KEY_REQ:
		hci_process_link_key_req(iface, &sa, addr);
		break;

	case HCI_EVENT_LINK_KEY_NOTIFICATION:
		hci_process_link_key_notification(iface, &sa, ep);
		break;

	default:
		log_packet(&sa.bt_bdaddr, &iface->addr,
		    "unexpected HCI event, event=%#x", event->event);
		break;
	}
}

int
hci_process_pin_code_req(struct bt_interface *iface,
    struct sockaddr_bt *sa, const bdaddr_t *addr)
{
	const uint8_t *pin;

	pin = conf_lookup_pin(iface->env, addr);

	if (pin == NULL) {
		log_info("%s: PIN code not found", bt_ntoa(addr, NULL));
		return hci_send_cmd(iface->fd, sa, HCI_CMD_PIN_CODE_NEG_REP,
		    sizeof(bdaddr_t), (void *)addr);
	} else {
		hci_pin_code_rep_cp cp;
		int n;

		bdaddr_copy(&cp.bdaddr, addr);
		memcpy(cp.pin, pin, HCI_PIN_SIZE);

		n = HCI_PIN_SIZE;
		while (n > 0 && pin[n - 1] == 0)
			n--;
		cp.pin_size = n;

		log_info("%s: PIN code found", bt_ntoa(addr, NULL));
		return hci_send_cmd(iface->fd, sa, HCI_CMD_PIN_CODE_REP,
		    sizeof(cp), &cp);
	}
}

int
hci_process_link_key_req(struct bt_interface *iface,
    struct sockaddr_bt *sa, const bdaddr_t *addr)
{
	hci_link_key_rep_cp cp;

	if (db_get_link_key(&iface->env->db, addr, cp.key)) {
		log_info("%s: link key not found", bt_ntoa(addr, NULL));
		return hci_send_cmd(iface->fd, sa, HCI_CMD_LINK_KEY_NEG_REP,
		    sizeof(bdaddr_t), (void *)addr);
	}

	log_info("%s: link key found", bt_ntoa(addr, NULL));
	bdaddr_copy(&cp.bdaddr, addr);

	return hci_send_cmd(iface->fd, sa, HCI_CMD_LINK_KEY_REP,
	    sizeof(cp), &cp);
}

int
hci_process_link_key_notification(struct bt_interface *iface,
    struct sockaddr_bt *sa, const hci_link_key_notification_ep *ep)
{
	const bdaddr_t *addr = &ep->bdaddr;
	int res;

	if ((res = db_put_link_key(&iface->env->db, addr, ep->key)) == 0)
		log_info("%s: link key stored", bt_ntoa(addr, NULL));
	else
		log_info("%s: link key not stored", bt_ntoa(addr, NULL));

	return res;
}

/*
 * Basic HCI cmd request function with argument return.
 *
 * Normally, this will return on COMMAND_STATUS or COMMAND_COMPLETE
 * for the given opcode, but if event is given then it will ignore
 * COMMAND_STATUS (unless error) and wait for the specified event.
 *
 * If rbuf/rlen is given, results will be copied into the result
 * buffer for COMMAND_COMPLETE/event responses.
 */
int
hci_req(int hci, uint16_t opcode, uint8_t event, void *cbuf, size_t clen,
    void *rbuf, size_t rlen)
{
	uint8_t msg[sizeof(hci_cmd_hdr_t) + HCI_CMD_PKT_SIZE];
	hci_event_hdr_t *ep;
	hci_cmd_hdr_t *cp;

	cp = (hci_cmd_hdr_t *)msg;
	cp->type = HCI_CMD_PKT;
	cp->opcode = opcode = htole16(opcode);
	cp->length = clen = MIN(clen, sizeof(msg) - sizeof(hci_cmd_hdr_t));

	if (clen)
		memcpy((cp + 1), cbuf, clen);

	if (send(hci, msg, sizeof(hci_cmd_hdr_t) + clen, 0) < 0) {
		log_warn("HCI send");
		return -1;
	}

	ep = (hci_event_hdr_t *)msg;
	for(;;) {
		if (recv(hci, msg, sizeof(msg), 0) < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;

			log_warn("HCI recv");
			return -1;
		}

		if (ep->event == HCI_EVENT_COMMAND_STATUS) {
			hci_command_status_ep *cs;

			cs = (hci_command_status_ep *)(ep + 1);
			if (cs->opcode != opcode)
				continue;

			if (cs->status) {
				log_warnx("HCI cmd (%4.4x) failed (status %d)",
				    opcode, cs->status);
				return -1;
			}

			if (event == 0)
				break;

			continue;
		}

		if (ep->event == HCI_EVENT_COMMAND_COMPL) {
			hci_command_compl_ep *cc;
			uint8_t *ptr;

			cc = (hci_command_compl_ep *)(ep + 1);
			if (cc->opcode != opcode)
				continue;

			if (rbuf == NULL)
				break;

			ptr = (uint8_t *)(cc + 1);
			if (*ptr) {
				log_warn("HCI cmd (%4.4x) failed (status %d)",
				    opcode, *ptr);
				return -1;
			}

			memcpy(rbuf, ++ptr, rlen);
			break;
		}

		if (ep->event == event) {
			if (rbuf == NULL)
				break;

			memcpy(rbuf, (ep + 1), rlen);
			break;
		}
	}

	return 0;
}

/* Send HCI Command Packet to socket */
int
hci_send_cmd(int sock, struct sockaddr_bt *sa, uint16_t opcode, size_t len,
    void *buf)
{
	char msg[HCI_CMD_PKT_SIZE];
	hci_cmd_hdr_t *h = (hci_cmd_hdr_t *)msg;

	h->type = HCI_CMD_PKT;
	h->opcode = htole16(opcode);
	h->length = len;

	if (len > 0)
		memcpy(msg + sizeof(hci_cmd_hdr_t), buf, len);

	if (sendto(sock, msg, sizeof(hci_cmd_hdr_t) + len, 0,
	    (struct sockaddr *)sa, sizeof(*sa)) == -1) {
		log_warn("HCI send command");
		return -1;
	}

	return 0;
}
