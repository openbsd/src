/*	$OpenBSD: hci.c,v 1.6 2008/11/27 00:51:17 uwe Exp $	*/
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
#include <errno.h>
#include <event.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "btd.h"

struct hci_physif {
	TAILQ_ENTRY(hci_physif) entry;
	struct btd *env;
	int present;
	bdaddr_t addr;
	char xname[16];
	struct event ev;
	int fd;
};

struct hci_state {
	TAILQ_HEAD(, hci_physif) physifs;
	int raw_sock;
};

void hci_update_physifs(struct btd *);
struct hci_physif *hci_find_physif(struct hci_state *, bdaddr_t *);

int hci_unit_up(const char *);
int hci_unit_down(const char *);

void hci_if_config(struct bt_interface *, const struct bt_interface *);
void hci_if_apply(struct bt_interface *);

int hci_interface_open(struct bt_interface *);
void hci_interface_close(struct bt_interface *);
int hci_interface_reinit(struct bt_interface *);

void hci_eventcb(int, short, void *);

int hci_process_pin_code_req(struct hci_physif *,
    struct sockaddr_bt *, const bdaddr_t *);
int hci_process_link_key_req(struct hci_physif *,
    struct sockaddr_bt *, const bdaddr_t *);
int hci_process_link_key_notification(struct hci_physif *,
    struct sockaddr_bt *, const hci_link_key_notification_ep *);
int hci_req(int, uint16_t, uint8_t , void *, size_t, void *, size_t);
int hci_send_cmd(int, struct sockaddr_bt *, uint16_t, size_t, void *);

#define hci_write(fd, opcode, wbuf, wlen) \
	hci_req(fd, opcode, 0, wbuf, wlen, NULL, 0)
#define hci_read(fd, opcode, rbuf, rlen) \
	hci_req(fd, opcode, 0, NULL, 0, rbuf, rlen)
#define hci_cmd(fd, opcode, cbuf, clen) \
	hci_req(fd, opcode, 0, cbuf, clen, NULL, 0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))

void
hci_init(struct btd *env)
{
	struct hci_state *hci;

	hci = env->hci = calloc(1, sizeof(*env->hci));

	hci->raw_sock = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (hci->raw_sock == -1)
		fatal("could not open raw HCI socket");

	TAILQ_INIT(&hci->physifs);
}

void
hci_update_physifs(struct btd *env)
{
	struct btreq btr;
	struct hci_state *hci = env->hci;
	struct hci_physif *physif;
	int sock;

	bzero(&btr, sizeof(btr));
	sock = hci->raw_sock;

	TAILQ_FOREACH(physif, &hci->physifs, entry)
		physif->present = 0;

	for (;;) {
		int flags;

		if (ioctl(sock, SIOCNBTINFO, &btr) < 0)
			break;

		/*
		 * XXX Interfaces must be up, just to determine their
		 * XXX bdaddr. Grrrr...
		 */
		if (!((flags = btr.btr_flags) & BTF_UP) ||
		    bdaddr_any(&btr.btr_bdaddr)) {
			if (hci_unit_up(btr.btr_name) < 0)
				continue;

			if (ioctl(sock, SIOCGBTINFO, &btr) < 0) {
				log_warn("%s: SIOCGBTINFO", btr.btr_name);
				continue;
			}

			if (!(btr.btr_flags & BTF_UP) ||
			    bdaddr_any(&btr.btr_bdaddr)) {
				log_warnx("could not enable %s",
				    btr.btr_name);
				continue;
			}

#if 0
			if (hci_unit_down(btr.btr_name) < 0)
				continue;
#endif
		}

		if ((physif = hci_find_physif(hci, &btr.btr_bdaddr))) {
			physif->present = 1;
			goto found;
		}

		if ((physif = calloc(1, sizeof(*physif))) == NULL)
			fatalx("hci physif calloc");

		physif->env = env;
		physif->fd = -1;
		physif->present = 1;
		bdaddr_copy(&physif->addr, &btr.btr_bdaddr);
		strlcpy(physif->xname, btr.btr_name, sizeof(physif->xname));

		TAILQ_INSERT_TAIL(&hci->physifs, physif, entry);

	found:
		(void)conf_add_interface(env, &physif->addr);
	}

	for (physif = TAILQ_FIRST(&hci->physifs); physif != NULL;) {
		if (!physif->present) {
			struct hci_physif *next;
			struct bt_interface *iface;

			iface = conf_find_interface(env, &physif->addr);
			if (iface != NULL) {
				hci_interface_close(iface);
				conf_delete_interface(iface);
			}

			next = TAILQ_NEXT(physif, entry);
			TAILQ_REMOVE(&hci->physifs, physif, entry);
			free(physif);
			physif = next;
			continue;
		}

		physif = TAILQ_NEXT(physif, entry);
	}
}

struct hci_physif *
hci_find_physif(struct hci_state *hci, bdaddr_t *addr)
{
	struct hci_physif *physif;

	TAILQ_FOREACH(physif, &hci->physifs, entry)
		if (bdaddr_same(&physif->addr, addr))
			return physif;

	return NULL;
}

int
hci_reinit(struct btd *env, const struct btd *conf)
{
	struct bt_interface *conf_iface;
	struct bt_interface *iface;
	struct bt_device *conf_btdev;
	struct bt_device *btdev;

	hci_update_physifs(env);

	/*
	 * "Downgrade" all explicit interfaces, which are not in the
	 * configuration anymore.
	 */
	TAILQ_FOREACH(iface, &env->interfaces, entry) {
		conf_iface = conf_find_interface(conf, &iface->addr);
		if (conf_iface == NULL)
			iface->flags &= ~BTIF_EXPLICIT;
	}

	/*
	 * Add new interfaces from the configuration and mark the
	 * changed properties of existing interfaces. The wildcard
	 * interface is not treated specially in this loop.
	 */
	TAILQ_FOREACH(conf_iface, &conf->interfaces, entry) {
		iface = conf_find_interface(env, &conf_iface->addr);

		if (iface == NULL) {
			iface = conf_add_interface(env, &conf_iface->addr);
			if (iface == NULL) {
				int err = errno;
				log_warn("could not add interface %s",
				    bt_ntoa(&conf_iface->addr, NULL));
				errno = err;
				return -1;
			}

			iface->changes |= BTIF_NAME_CHANGED;
		}

		iface->flags |= BTIF_EXPLICIT;

		hci_if_config(iface, conf_iface);
	}

	/*
	 * Apply the changes in the wildcard interface to all
	 * non-explicit interfaces. If there is no wildcard interface,
	 * delete all non-explicit interfaces.
	 */
	conf_iface = conf_find_interface(conf, BDADDR_ANY);
	TAILQ_FOREACH(iface, &env->interfaces, entry) {
		if (!(iface->flags & BTIF_EXPLICIT)) {
			if (conf_iface != NULL)
				hci_if_config(iface, conf_iface);
			else
				iface->changes |= BTIF_DELETED;
		}
	}

	/*
	 * Apply all interface changes now.
	 */
	for (iface = TAILQ_FIRST(&env->interfaces); iface != NULL;) {
		if (iface->changes & BTIF_DELETED) {
			struct bt_interface *next;

			hci_interface_close(iface);

			next = TAILQ_NEXT(iface, entry);
			conf_delete_interface(iface);
			iface = next;
			continue;
		}

		if (!bdaddr_any(&iface->addr))
			hci_if_apply(iface);

		iface = TAILQ_NEXT(iface, entry);
	}

	TAILQ_FOREACH(btdev, &env->devices, entry) {
		conf_btdev = conf_find_device(conf, &btdev->addr);
		if (conf_btdev == NULL) {
			btdev->flags &= ~BTDF_ATTACH;
			btdev->flags |= BTDF_DELETED;
		}
	}

	TAILQ_FOREACH(conf_btdev, &conf->devices, entry) {
		btdev = conf_find_device(env, &conf_btdev->addr);
		if (btdev == NULL)
			btdev = conf_add_device(env, &conf_btdev->addr);
		if (btdev == NULL)
			fatal("hci_reinit conf_add_device");

		btdev->type = conf_btdev->type;

		memcpy(btdev->pin, conf_btdev->pin, sizeof(btdev->pin));
		btdev->pin_size = conf_btdev->pin_size;

		btdev->flags |= conf_btdev->flags & BTDF_ATTACH;

	}

	bt_devices_changed();

	return 0;
}

int
hci_unit_up(const char *xname)
{
	struct btreq btr;

	memset(&btr, 0, sizeof(btr));
	strlcpy(btr.btr_name, xname, sizeof(btr.btr_name));

	btr.btr_flags = BTF_UP;

	if (bt_set_interface_flags(&btr) < 0) {
		log_warn("hci_unit_up: %s", xname);
		return -1;
	}

	return 0;
}

int
hci_unit_down(const char *xname)
{
	struct btreq btr;

	memset(&btr, 0, sizeof(btr));
	strlcpy(btr.btr_name, xname, sizeof(btr.btr_name));

	btr.btr_flags = 0;

	if (bt_set_interface_flags(&btr) < 0) {
		log_warn("hci_unit_down: %s", xname);
		return -1;
	}

	return 0;
}

void
hci_if_config(struct bt_interface *iface, const struct bt_interface *other)
{
	if (memcmp(iface->name, other->name, sizeof(iface->name))) {
		iface->changes |= BTIF_NAME_CHANGED;
		memcpy(iface->name, other->name, sizeof(iface->name));
	}

	iface->disabled = other->disabled;
}

void
hci_if_apply(struct bt_interface *iface)
{
	if (iface->disabled) {
		hci_interface_close(iface);
		return;
	}

	if (hci_interface_open(iface) < 0)
		return;

	if (iface->changes != 0) {
		(void)hci_interface_reinit(iface);
		iface->changes = 0;
	}
}

int
hci_interface_open(struct bt_interface *iface)
{
	struct hci_filter filter;
	struct hci_state *hci = iface->env->hci;
	struct hci_physif *physif;
	int err;

	if (iface->physif == NULL) {
		iface->physif = hci_find_physif(hci, &iface->addr);
		if (iface->physif == NULL)
			/* no physical interface to open, but that's ok */
			return 0;
	}

	physif = iface->physif;

	if (physif->fd != -1)
		return 0;

	if (hci_unit_up(physif->xname) < 0)
		return -1;

	bt_priv_msg(IMSG_OPEN_HCI);
	bt_priv_send(&physif->addr, sizeof(bdaddr_t));
	physif->fd = receive_fd(priv_fd);
	bt_priv_recv(&err, sizeof(int));
	if (err != 0) {
		log_warnx("OPEN_HCI failed (%s)", strerror(err));
		return -1;
	}

	memset(&filter, 0, sizeof(filter));
	hci_filter_set(HCI_EVENT_COMMAND_STATUS, &filter);
	hci_filter_set(HCI_EVENT_COMMAND_COMPL, &filter);
	hci_filter_set(HCI_EVENT_PIN_CODE_REQ, &filter);
	hci_filter_set(HCI_EVENT_LINK_KEY_REQ, &filter);
	hci_filter_set(HCI_EVENT_LINK_KEY_NOTIFICATION, &filter);
	if (setsockopt(physif->fd, BTPROTO_HCI, SO_HCI_EVT_FILTER,
	    &filter, sizeof(filter)) < 0) {
		log_warn("SO_HCI_EVT_FILTER");
		return -1;
	}

	event_set(&physif->ev, physif->fd, EV_READ|EV_PERSIST,
	    &hci_eventcb, physif);
	if (event_add(&physif->ev, NULL)) {
		log_warnx("event_add");
		return -1;
	}

	log_info("listening on %s (%s)", bt_ntoa(&physif->addr, NULL),
	    physif->xname);
	return 0;
}

void
hci_interface_close(struct bt_interface *iface)
{
	struct hci_physif *physif = iface->physif;

	if (physif == NULL)
		return;

	if (physif->fd != -1) {
		close(physif->fd);
		physif->fd = -1;
	}

	iface->physif = NULL;

#if 0
	if (hci_unit_down(physif->xname) < 0)
		return;
#endif

	log_info("stopped listening on %s", bt_ntoa(&physif->addr, NULL));
}

int
hci_interface_reinit(struct bt_interface *iface)
{
	struct btreq btr;
	struct hci_physif *physif;
	uint8_t val;
	int err;

	if ((physif = iface->physif) == NULL)
		/* no physical interface, so we keep changes pending */
		return 0;

	if (iface->changes & BTIF_NAME_CHANGED) {
		if (hci_write(physif->fd, HCI_CMD_WRITE_LOCAL_NAME,
		    iface->name, HCI_UNIT_NAME_SIZE))
			return -1;
		iface->changes &= ~BTIF_NAME_CHANGED;
	}

	/* The rest is unconditional configuration. */

	if (hci_read(physif->fd, HCI_CMD_READ_SCAN_ENABLE, &val,
	    sizeof(val)))
		return -1;

	val |= HCI_PAGE_SCAN_ENABLE;
	val |= HCI_INQUIRY_SCAN_ENABLE;

	if (hci_write(physif->fd, HCI_CMD_WRITE_SCAN_ENABLE, &val,
	    sizeof(val)))
		return -1;

	bdaddr_copy(&btr.btr_bdaddr, &physif->addr);

	if (ioctl(physif->fd, SIOCGBTINFOA, &btr) < 0) {
		log_warn("SIOCGBTINFOA");
		return -1;
	}

	btr.btr_link_policy |= HCI_LINK_POLICY_ENABLE_ROLE_SWITCH;

	bt_priv_msg(IMSG_SET_LINK_POLICY);
	bt_priv_send(&physif->addr, sizeof(bdaddr_t));
	bt_priv_send(&btr.btr_link_policy, sizeof(uint16_t));
	bt_priv_recv(&err, sizeof(int));

	if (err != 0) {
		log_warnx("SET_LINK_POLICY failed (%s)", strerror(err));
		return -1;
	}

	return 0;
}

void
hci_eventcb(int fd, short evflags, void *arg)
{
	char buf[HCI_EVENT_PKT_SIZE];
	hci_event_hdr_t *event = (hci_event_hdr_t *)buf;
	struct hci_physif *physif = arg;
	struct sockaddr_bt sa;
	socklen_t size;
	bdaddr_t *addr;
	void *ep;
	int n;

	if (physif == NULL)
		fatal("HCI event on closed socket?");

	size = sizeof(sa);
	n = recvfrom(physif->fd, buf, sizeof(buf), 0,
	    (struct sockaddr *)&sa, &size);
	if (n < 0) {
		log_warn("could not receive from HCI socket");
		return;
	}

	if (n < sizeof(hci_event_hdr_t)) {
		log_warnx("short HCI packet");
		return;
	}

	if (event->type != HCI_EVENT_PKT) {
		log_packet(&sa.bt_bdaddr, &physif->addr,
		    "unexpected HCI packet, type=%#x", event->type);
		return;
	}

	addr = (bdaddr_t *)(event + 1);
	ep = (bdaddr_t *)(event + 1);

	/* XXX check packet size */
	switch (event->event) {
	case HCI_EVENT_COMMAND_STATUS:
	case HCI_EVENT_COMMAND_COMPL:
		break;

	case HCI_EVENT_PIN_CODE_REQ:
		hci_process_pin_code_req(physif, &sa, addr);
		break;

	case HCI_EVENT_LINK_KEY_REQ:
		hci_process_link_key_req(physif, &sa, addr);
		break;

	case HCI_EVENT_LINK_KEY_NOTIFICATION:
		hci_process_link_key_notification(physif, &sa, ep);
		break;

	default:
		log_packet(&sa.bt_bdaddr, &physif->addr,
		    "unexpected HCI event, event=%#x", event->event);
		break;
	}
}

int
hci_process_pin_code_req(struct hci_physif *physif,
    struct sockaddr_bt *sa, const bdaddr_t *addr)
{
	hci_pin_code_rep_cp cp;
	struct btd *env = physif->env;
	int fd = physif->fd;

	conf_lookup_pin(env, addr, cp.pin, &cp.pin_size);

	if (cp.pin_size == 0) {
		log_info("%s: PIN code not found", bt_ntoa(addr, NULL));
		return hci_send_cmd(fd, sa, HCI_CMD_PIN_CODE_NEG_REP,
		    sizeof(bdaddr_t), (void *)addr);
	} else {
		log_info("%s: PIN code found", bt_ntoa(addr, NULL));
		bdaddr_copy(&cp.bdaddr, addr);
		return hci_send_cmd(fd, sa, HCI_CMD_PIN_CODE_REP,
		    sizeof(cp), &cp);
	}
}

int
hci_process_link_key_req(struct hci_physif *physif, struct sockaddr_bt *sa,
    const bdaddr_t *addr)
{
	hci_link_key_rep_cp cp;
	struct btd *env = physif->env;
	int fd = physif->fd;

	if (db_get_link_key(&env->db, addr, cp.key)) {
		log_info("%s: link key not found", bt_ntoa(addr, NULL));
		return hci_send_cmd(fd, sa, HCI_CMD_LINK_KEY_NEG_REP,
		    sizeof(bdaddr_t), (void *)addr);
	}

	log_info("%s: link key found", bt_ntoa(addr, NULL));
	bdaddr_copy(&cp.bdaddr, addr);
	return hci_send_cmd(fd, sa, HCI_CMD_LINK_KEY_REP, sizeof(cp), &cp);
}

int
hci_process_link_key_notification(struct hci_physif *physif,
    struct sockaddr_bt *sa, const hci_link_key_notification_ep *ep)
{
	const bdaddr_t *addr = &ep->bdaddr;
	struct btd *env = physif->env;
	int res;

	if ((res = db_put_link_key(&env->db, addr, ep->key)) == 0)
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
