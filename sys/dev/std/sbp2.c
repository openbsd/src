/*	$OpenBSD: sbp2.c,v 1.2 2002/12/13 21:35:11 tdeval Exp $	*/

/*
 * Copyright (c) 2002 Thierry Deval.  All rights reserved.
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
 * sbp2.c
 *
 * Basic implementation of the ANSI NCITS 325-1998 Serial Bus Protocol2 (SBP-2).
 *
 * ANSI NCITS 325-1998
 * Information Technology - Serial Bus Protocol 2 (SBP-2)
 *
 * Defines a protocol for the transport of commands and data over high
 * performance serial bus, as specified in American National Standard for
 * High Performance Serial Bus, ANSI/IEEE 1394-1995. The transport protocol,
 * Serial Bus Protocol 2 or SBP-2, requires implementations to conform to the
 * requirements of the aforementioned standard as well as to International
 * Standard for Control and Status Register (CSR) Architecture for
 * Microcomputer Buses, ISO/IEC 13213:1994 (IEEE 1212-1994), and permits the
 * exchange of commands, data and status between initiators and targets
 * connected to Serial Bus.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#ifdef	__NetBSD__
#include <sys/callout.h>
#else
#include <sys/timeout.h>
#endif
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#ifdef	__OpenBSD__
#include <sys/endian.h>
#endif

#if __NetBSD_Version__ >= 105010000 || !defined(__NetBSD__)
#include <uvm/uvm_extern.h>
#else
#include <vm/vm.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/rndvar.h>

#include <dev/ieee1394/ieee1394reg.h>
#include <dev/ieee1394/fwohcireg.h>
#include <dev/ieee1394/fwnodereg.h>
#include <dev/std/ieee1212reg.h>
#include <dev/std/sbp2reg.h>

#include <dev/ieee1394/ieee1394var.h>
#include <dev/ieee1394/fwohcivar.h>
#include <dev/ieee1394/fwnodevar.h>
#include <dev/std/ieee1212var.h>
#include <dev/std/sbp2var.h>

int sbp2_print_data(struct p1212_data *);
int sbp2_print_dir(struct p1212_dir *);

void sbp2_login_send(struct ieee1394_abuf *, int);
void sbp2_status_resp(struct ieee1394_abuf *, int);
void sbp2_command_send(struct ieee1394_abuf *, int);

#ifdef	SBP2_DEBUG
#include <sys/syslog.h>
extern int log_open;
int sbp2_oldlog;
#define	DPRINTF(x)	do {						\
	if (sbp2debug) {						\
		sbp2_oldlog = log_open; log_open = 1;			\
		addlog x; log_open = sbp2_oldlog;			\
	}								\
} while (0)
#define	DPRINTFN(n,x)	do {						\
	if (sbp2debug>(n)) {						\
		sbp2_oldlog = log_open; log_open = 1;			\
		addlog x; log_open = sbp2_oldlog;			\
	}								\
} while (0)
#ifdef	FW_MALLOC_DEBUG
#define	MPRINTF(x,y)	DPRINTF(("%s[%d]: %s 0x%08x\n",			\
			    __func__, __LINE__, (x), (u_int32_t)(y)))
#else	/* !FW_MALLOC_DEBUG */
#define	MPRINTF(x,y)
#endif	/* FW_MALLOC_DEBUG */

int	sbp2debug = 0;
#else	/* SBP2_DEBUG */
#define	DPRINTF(x)
#define	DPRINTFN(n,x)
#define	MPRINTF(x,y)
#endif	/* ! SBP2_DEBUG */

typedef struct sbp2_orb_element {
	u_int32_t		  elm_hash;
	struct ieee1394_abuf	 *elm_orb_ab;
	struct sbp2_command_orb	 *elm_orb;
	size_t			  elm_orblen;
	void			 *elm_data;
	size_t			  elm_datasize;
	void			(*elm_cb)(void *,
					  struct sbp2_status_notification *);
	void			 *elm_cbarg;
	TAILQ_ENTRY(sbp2_orb_element) elm_chain;
} sbp2_orb_element;

typedef struct sbp2_account {
	struct fwnode_softc	 *ac_softc;
	u_int16_t		  ac_lun;
	u_int16_t		  ac_login;
	u_int16_t		  ac_reconnect_hold;
	u_int16_t		  ac_valid;
	u_int64_t		  ac_mgmt_agent;
	u_int64_t		  ac_fetch_agent;
	u_int64_t		  ac_response;
	void			 *ac_response_block;
	u_int64_t		  ac_status_fifo;
	struct ieee1394_abuf	 *ac_status_ab;
	struct sbp2_status_block *ac_status_block;
	void			(*ac_cb)(void *,
					 struct sbp2_status_notification *);
	void			 *ac_cbarg;
	struct sbp2_task_management_orb *ac_mgmt_orb;
	TAILQ_HEAD(sbp2_orb_tq, sbp2_orb_element) ac_orb_head;
	SLIST_ENTRY(sbp2_account) ac_chain;
} sbp2_account;

static SLIST_HEAD(, sbp2_account) sbp2_ac_head;
static int sbp2_ac_valid;

struct sbp2_account *sbp2_acfind(struct fwnode_softc *, int);
struct sbp2_orb_element *sbp2_elfind_hash(struct sbp2_account *, u_int32_t);
struct sbp2_orb_element *sbp2_elfind_orb(struct sbp2_account *,
    struct sbp2_command_orb *);

struct sbp2_account *
sbp2_acfind(struct fwnode_softc *sc, int lun)
{
	struct sbp2_account *ac;

	SLIST_FOREACH(ac, &sbp2_ac_head, ac_chain) {
		if (ac != NULL && ac->ac_softc == sc && ac->ac_lun == lun)
			break;
	}

	return (ac);
}

struct sbp2_orb_element *
sbp2_elfind_hash(struct sbp2_account *ac, u_int32_t hash)
{
	struct sbp2_orb_element *elm;

	TAILQ_FOREACH(elm, &ac->ac_orb_head, elm_chain) {
		if (elm->elm_hash == hash)
			break;
	}

	return (elm);
}

struct sbp2_orb_element *
sbp2_elfind_orb(struct sbp2_account *ac, struct sbp2_command_orb *orb)
{
	struct sbp2_orb_element *elm;

	TAILQ_FOREACH(elm, &ac->ac_orb_head, elm_chain) {
		if (elm->elm_orb == orb)
			break;
	}

	return (elm);
}

int
sbp2_print_data(struct p1212_data *data)
{
	struct p1212_key *key = (struct p1212_key *)data;

	switch (key->key_value) {
	case SBP2_KEYVALUE_Command_Set:
		DPRINTF(("SBP2 Command Set: "));
		if (key->val == 0x104d8)
			DPRINTF(("SCSI 2\n"));
		else
			DPRINTF(("0x%08x\n", key->val));
		break;
	case SBP2_KEYVALUE_Unit_Characteristics:
		DPRINTF(("SBP2 Unit Characteristics: 0x%08x\n", key->val));
		break;
	case SBP2_KEYVALUE_Command_Set_Revision:
		DPRINTF(("SBP2 Command Set Revision: 0x%08x\n", key->val));
		break;
	case SBP2_KEYVALUE_Command_Set_Spec_Id:
		DPRINTF(("SBP2 Command Set Spec Id: 0x%08x\n", key->val));
		break;
	case SBP2_KEYVALUE_Firmware_Revision:
		DPRINTF(("SBP2 Firmware Revision: 0x%08x\n", key->val));
		break;
	case SBP2_KEYVALUE_Reconnect_Timeout:
		DPRINTF(("SBP2 Reconnect Timeout: 0x%08x\n", key->val));
		break;
	case SBP2_KEYVALUE_Unit_Unique_Id:
		DPRINTF(("SBP2 Unit Unique Id: 0x%08x\n", key->val));
		break;
	case P1212_KEYVALUE_Unit_Dependent_Info:
		if (key->key_type == P1212_KEYTYPE_Immediate)
			DPRINTF(("SBP2 Logical Unit Number: 0x%08x\n", key->val));
		else if (key->key_type == P1212_KEYTYPE_Offset)
			DPRINTF(("SBP2 Management Agent: 0x%08x\n", key->val));
		break;
	default:
		return 0;
	}
	return 1;
}

int
sbp2_print_dir(struct p1212_dir *dir)
{
	u_int8_t dir_type = ((struct p1212_key *)dir)->key_type;

	switch(dir_type) {
	case SBP2_KEYVALUE_Logical_Unit_Directory:
		DPRINTF(("Logical Unit "));
		break;
	default:
		return 0;
	}
	return 1;
}

int
sbp2_init(struct fwnode_softc *sc, struct p1212_dir *unitdir)
{
	struct p1212_key **key;
	struct p1212_dir *dir;
	struct sbp2_account *ac;
	int loc;

	key = p1212_find(unitdir, P1212_KEYTYPE_Offset,
	    SBP2_KEYVALUE_Management_Agent, 0);
	if (key == NULL) return (-1);

	if (!sbp2_ac_valid) {
		SLIST_INIT(&sbp2_ac_head);
		sbp2_ac_valid = 1;
	}

	MALLOC(ac, struct sbp2_account *, sizeof(*ac), M_1394CTL, M_WAITOK);
	MPRINTF("MALLOC(1394CTL)", ac);
	bzero(ac, sizeof(*ac));

	loc = key[0]->val;
	DPRINTF(("%s: Node %d: UID %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
	    __func__, sc->sc_sc1394.sc1394_node_id,
	    sc->sc_sc1394.sc1394_guid[0], sc->sc_sc1394.sc1394_guid[1],
	    sc->sc_sc1394.sc1394_guid[2], sc->sc_sc1394.sc1394_guid[3],
	    sc->sc_sc1394.sc1394_guid[4], sc->sc_sc1394.sc1394_guid[5],
	    sc->sc_sc1394.sc1394_guid[6], sc->sc_sc1394.sc1394_guid[7]));
	free(key, M_DEVBUF);
	MPRINTF("free(DEVBUF)", key);
	key = NULL;	/* XXX */

	ac->ac_softc = sc;
	ac->ac_login = sc->sc_sc1394.sc1394_node_id;
	ac->ac_mgmt_agent = CSR_BASE + 4 * loc;
	DPRINTF(("%s: mgmt_agent = 0x%016qx\n", __func__, ac->ac_mgmt_agent));

	if ((key = p1212_find(unitdir, P1212_KEYTYPE_Immediate,
	    SBP2_KEYVALUE_Logical_Unit_Number, 0)) != NULL) {
		ac->ac_lun = (*key)->val & 0xffff;
		free(key, M_DEVBUF);
		MPRINTF("free(DEVBUF)", key);
		key = NULL;	/* XXX */
	} else if ((key = p1212_find(unitdir, P1212_KEYTYPE_Directory,
	    SBP2_KEYVALUE_Logical_Unit_Directory, 0)) != NULL) {
		dir = (struct p1212_dir *)*key;
		free(key, M_DEVBUF);
		MPRINTF("free(DEVBUF)", key);
		key = NULL;	/* XXX */
		key = p1212_find(dir, P1212_KEYTYPE_Immediate,
		    SBP2_KEYVALUE_Logical_Unit_Number, 0);
		if (key != NULL) {
			ac->ac_lun = (*key)->val & 0xffff;
			free(key, M_DEVBUF);
			MPRINTF("free(DEVBUF)", key);
			key = NULL;	/* XXX */
		}
	}
	DPRINTF(("%s: lun = %d\n", __func__, ac->ac_lun));

	TAILQ_INIT(&ac->ac_orb_head);

	SLIST_INSERT_HEAD(&sbp2_ac_head, ac, ac_chain);

	return (ac->ac_lun);
}

void
sbp2_clean(struct fwnode_softc *sc, struct p1212_dir *unitdir, int logout)
{
	struct sbp2_account *ac;
	struct sbp2_orb_element *elm;
	struct p1212_key **key;
	struct p1212_dir **dir,*d;
	int lun,i;

	DPRINTF(("%s: start\n", __func__));
#ifdef	SBP2_DEBUG
	if (sbp2debug)
		p1212_print(unitdir);
#endif

	if ((key = p1212_find(unitdir, P1212_KEYTYPE_Immediate,
	    SBP2_KEYVALUE_Logical_Unit_Number, 0)) != NULL) {
		lun = (*key)->val & 0xffff;
		if ((ac = sbp2_acfind(sc, lun)) != NULL) {
			DPRINTF(("%s: clean lun %d\n", __func__, lun));
			i = 0;
			TAILQ_FOREACH_REVERSE(elm, &ac->ac_orb_head, elm_chain,
			    sbp2_orb_tq) {
				DPRINTF(("%s%d", i++?" ":"", i));
				if (elm != NULL) {
					TAILQ_REMOVE(&ac->ac_orb_head, elm,
					    elm_chain);
					FREE(elm, M_1394CTL);
					MPRINTF("FREE(1394CTL)", elm);
					//elm = NULL;	/* XXX */
				}
			}
			if (i) DPRINTF(("\n"));
			SLIST_REMOVE(&sbp2_ac_head, ac, sbp2_account, ac_chain);
			if (ac->ac_status_ab) {
				sc->sc1394_unreg(ac->ac_status_ab, FALSE);
				FREE(ac->ac_status_ab, M_1394DATA);
				MPRINTF("FREE(1394DATA)", ac->ac_status_ab);
				ac->ac_status_ab = NULL;	/* XXX */
			}
			if (ac->ac_status_block) {
				FREE(ac->ac_status_block, M_1394DATA);
				MPRINTF("FREE(1394DATA)", ac->ac_status_block);
				ac->ac_status_block = NULL;	/* XXX */
			}
			FREE(ac, M_1394CTL);
			MPRINTF("FREE(1394CTL)", ac);
			ac = NULL;	/* XXX */
		}
		free(key, M_DEVBUF);
		MPRINTF("free(DEVBUF)", key);
		key = NULL;	/* XXX */
	} else if ((key = p1212_find(unitdir, P1212_KEYTYPE_Directory,
	    SBP2_KEYVALUE_Logical_Unit_Directory, 0)) != NULL) {
		dir = (struct p1212_dir **)key;
		i = 0;
		d = dir[i++];
		while (d != NULL) {
			key = p1212_find(d, P1212_KEYTYPE_Immediate,
			    SBP2_KEYVALUE_Logical_Unit_Number, 0);
			if (key != NULL) {
				lun = (*key)->val & 0xffff;
				if ((ac = sbp2_acfind(sc, lun)) != NULL) {
					DPRINTF(("%s: clean lun %d\n",
					    __func__, lun));
					TAILQ_FOREACH(elm, &ac->ac_orb_head,
					    elm_chain) {
						if (elm != NULL) {
							FREE(elm, M_1394CTL);
							MPRINTF("FREE(1394CTL)", elm);
							elm = NULL;	/* XXX */
						}
					}
					SLIST_REMOVE(&sbp2_ac_head, ac,
					    sbp2_account, ac_chain);
					if (ac->ac_status_ab) {
						sc->sc1394_unreg(ac
						    ->ac_status_ab, FALSE);
						FREE(ac->ac_status_ab,
						    M_1394DATA);
						MPRINTF("FREE(1394DATA)", ac->ac_status_ab);
						ac->ac_status_ab = NULL;	/* XXX */
					}
					if (ac->ac_status_block) {
						FREE(ac->ac_status_block,
						    M_1394DATA);
						MPRINTF("FREE(1394DATA)", ac->ac_status_block);
						ac->ac_status_block = NULL;	/* XXX */
					}
					FREE(ac, M_1394CTL);
					MPRINTF("FREE(1394CTL)", ac);
					ac = NULL;	/* XXX */
				}
				free(key, M_DEVBUF);
				MPRINTF("free(DEVBUF)", key);
				key = NULL;	/* XXX */
			}
			d = dir[i++];
		}
		free(dir, M_DEVBUF);
		MPRINTF("free(DEVBUF)", dir);
		dir = NULL;	/* XXX */
	} else {
		DPRINTF(("%s: no LUN in configrom 0x%08x ???\n", __func__,
		    (u_int32_t)sc->sc_configrom->root));
	}

	DPRINTF(("%s: end\n", __func__));
}

void
sbp2_login(struct fwnode_softc *sc, struct sbp2_login_orb *orb,
    void (*cb)(void *, struct sbp2_status_notification *), void *cbarg)
{
	struct ieee1394_abuf *ab, *ab2;
	struct sbp2_account *ac;

	if ((ac = sbp2_acfind(sc, ntohs(orb->lun))) == NULL) {
		DPRINTF(("%s: destination not initialized\n", __func__));
		return;
	}

	DPRINTF(("%s:", __func__));

	ac->ac_mgmt_orb = (struct sbp2_task_management_orb *)orb;
	if (cb != NULL) {
		ac->ac_cb = cb;
		ac->ac_cbarg = cbarg;
	}

	MALLOC(ab, struct ieee1394_abuf *, sizeof(*ab), M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", ab);
	bzero(ab, sizeof(*ab));
	MALLOC(ab2, struct ieee1394_abuf *, sizeof(*ab2), M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", ab2);
	bzero(ab2, sizeof(*ab2));

	ab->ab_req = (struct ieee1394_softc *)sc;
	ab->ab_length = 8;
	ab->ab_retlen = 0;
	ab->ab_cb = NULL;
	ab->ab_cbarg = NULL;
	ab->ab_addr = ac->ac_mgmt_agent;
	ab->ab_tcode = IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK;

	ab->ab_data = malloc(8, M_1394DATA, M_WAITOK);
	MPRINTF("malloc(1394DATA)", ab->ab_data);
	ab->ab_data[0] = htonl((u_int32_t)(SBP2_LOGIN_ORB >> 32));
	ab->ab_data[1] = htonl((u_int32_t)(SBP2_LOGIN_ORB & 0xFFFFFFFF));
	DPRINTF((" CSR = 0x%016qx", ac->ac_mgmt_agent));
	DPRINTF((", ORB = 0x%016qx\n", SBP2_LOGIN_ORB + (u_int32_t)orb));

	ab2->ab_length = sizeof(struct sbp2_login_orb);
	ab2->ab_tcode = IEEE1394_TCODE_READ_REQUEST_DATABLOCK;
	ab2->ab_retlen = 0;
	ab2->ab_data = NULL;
	ab2->ab_addr = SBP2_LOGIN_ORB;
	ab2->ab_cb = sbp2_login_send;
	ab2->ab_cbarg = ac;
	ab2->ab_req = (struct ieee1394_softc *)sc;

	sc->sc1394_inreg(ab2, FALSE);
	sc->sc1394_write(ab);
	DPRINTF(("%s: LOGIN submitted\n", __func__));
	return;
}

void
sbp2_login_send(struct ieee1394_abuf *ab, int rcode)
{
	struct fwnode_softc *sc = (struct fwnode_softc *)ab->ab_req;
	struct sbp2_account *ac = ab->ab_cbarg;
	struct sbp2_login_orb *login_orb;
	struct ieee1394_abuf *stat_ab, *resp_ab, *orb_ab;
#ifdef	SBP2_DEBUG
	int i;
#endif	/* SBP2_DEBUG */

	/* Got a read so allocate the buffer and write out the response. */

	if (rcode || (ac == NULL)) {
		DPRINTF(("%s: Bad return code: %d\n", __func__, rcode));
		if (ab->ab_data) {
			free(ab->ab_data, M_1394DATA);
			MPRINTF("free(1394DATA)", ab->ab_data);
			ab->ab_data = NULL;	/* XXX */
		}
		FREE(ab, M_1394DATA);
		MPRINTF("FREE(1394DATA)", ab);
		ab = NULL;	/* XXX */
		return;
	}

	MALLOC(orb_ab, struct ieee1394_abuf *, sizeof(*orb_ab),
	    M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", orb_ab);
	bcopy(ab, orb_ab, sizeof(*orb_ab));

	sc->sc1394_unreg(ab, FALSE);

	if (ab->ab_data) {
		free(ab->ab_data, M_1394DATA);
		MPRINTF("free(1394DATA)", ab->ab_data);
		ab->ab_data = NULL;	/* XXX */
		orb_ab->ab_data = NULL;
	}
	FREE(ab, M_1394DATA);
	MPRINTF("FREE(1394DATA)", ab);
	ab = NULL;	/* XXX */

	MALLOC(resp_ab, struct ieee1394_abuf *, sizeof(*resp_ab),
	    M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", resp_ab);
	MALLOC(stat_ab, struct ieee1394_abuf *, sizeof(*stat_ab),
	    M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", stat_ab);
	bzero(resp_ab, sizeof(*resp_ab));
	bzero(stat_ab, sizeof(*stat_ab));

	resp_ab->ab_length = sizeof(struct sbp2_login_response);
	resp_ab->ab_tcode = IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK;
	resp_ab->ab_retlen = 0;
	resp_ab->ab_data = NULL;
	resp_ab->ab_addr = SBP2_LOGIN_RESP;
	resp_ab->ab_cb = sbp2_status_resp;
	resp_ab->ab_cbarg = ac;
	resp_ab->ab_req = orb_ab->ab_req;

	sc->sc1394_inreg(resp_ab, FALSE);

	stat_ab->ab_length = sizeof(struct sbp2_status_block);
	stat_ab->ab_tcode = IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK;
	stat_ab->ab_retlen = 0;
	stat_ab->ab_data = NULL;
	stat_ab->ab_addr = SBP2_LOGIN_STATUS;
	stat_ab->ab_cb = sbp2_status_resp;
	stat_ab->ab_cbarg = ac;
	stat_ab->ab_req = orb_ab->ab_req;

	ac->ac_status_ab = stat_ab;
	sc->sc1394_inreg(stat_ab, FALSE);

	/* Fill in a login packet. First 2 quads are 0 for password. */
	login_orb = (struct sbp2_login_orb *)ac->ac_mgmt_orb;

	/* Addr for response. */
	login_orb->login_response.hi = htons((SBP2_LOGIN_RESP >> 32) & 0xffff);
	login_orb->login_response.lo = htonl(SBP2_LOGIN_RESP & 0xffffffff);
	DPRINTF(("%s: RESP_ORB = 0x%016qx", __func__, SBP2_LOGIN_RESP));

	/* Set notify and exclusive use bits. Login to lun 0 (XXX) */
	login_orb->options = htons(0x8000);
	login_orb->lun = htons(ac->ac_lun);

	/* Password length (0) and login response length (16) */
	login_orb->password_length = htons(0);
	login_orb->login_response_length = htons(16);

	/* Addr for status packet. */
	login_orb->status_fifo.hi = htons((SBP2_LOGIN_STATUS >> 32) & 0xffff);
	login_orb->status_fifo.lo = htonl(SBP2_LOGIN_STATUS & 0xffffffff);
	DPRINTF((", STATUS_ORB = 0x%016qx", SBP2_LOGIN_STATUS));

	orb_ab->ab_data = malloc(sizeof(*login_orb), M_1394DATA, M_WAITOK);
	MPRINTF("malloc(1394DATA)", orb_ab->ab_data);
	bcopy(login_orb, orb_ab->ab_data, sizeof(*login_orb));
#ifdef	SBP2_DEBUG
	for (i = 0; i < (sizeof(*login_orb) / 4); i++) {
		if ((i % 8) == 0) DPRINTFN(2, ("\n   "));
		DPRINTFN(2, (" %08x", ntohl(orb_ab->ab_data[i])));
	}
#endif	/* SBP2_DEBUG */
	DPRINTF(("\n"));

	orb_ab->ab_retlen = 0;
	orb_ab->ab_cb = NULL;
	orb_ab->ab_cbarg = NULL;
	orb_ab->ab_tcode = IEEE1394_TCODE_READ_RESPONSE_DATABLOCK;
	orb_ab->ab_length = sizeof(struct sbp2_login_orb);

	sc->sc1394_write(orb_ab);
}

void
sbp2_status_resp(struct ieee1394_abuf *ab, int rcode)
{
	struct fwnode_softc *sc = (struct fwnode_softc *)ab->ab_req;
	struct sbp2_account *ac = ab->ab_cbarg;
	struct sbp2_login_response *login_resp;
	struct sbp2_status_block *cmd_status;
	struct sbp2_status_notification *status_notify;
	struct sbp2_orb_element *elm;
	int resp, src, status, len, dead;
	u_int64_t csr;
#ifdef	SBP2_DEBUG
	int i;
#endif	/* SBP2_DEBUG */

	if (rcode || (ac == NULL)) {
		DPRINTF(("%s: Bad return code: %d\n", __func__, rcode));
		if (ab->ab_data) {
			free(ab->ab_data, M_1394DATA);
			MPRINTF("free(1394DATA)", ab->ab_data);
			ab->ab_data = NULL;	/* XXX */
		}
		if (ab->ab_addr != SBP2_LOGIN_STATUS) {
			FREE(ab, M_1394DATA);
			MPRINTF("FREE(1394DATA)", ab);
			ab = NULL;	/* XXX */
		}
		return;
	}

#ifdef	SBP2_DEBUG
	DPRINTF(("%s: CSR = 0x%016qx", __func__, (quad_t)ab->ab_addr));
	for (i = 0; i < (ab->ab_retlen / 4); i++) {
		if ((i % 8) == 0) DPRINTFN(2, ("\n   "));
		DPRINTFN(2, (" %08x", ntohl(ab->ab_data[i])));
	}
	DPRINTF(("\n"));
#endif	/* SBP2_DEBUG */

	if (ab->ab_addr == SBP2_LOGIN_RESP) {
		login_resp = (struct sbp2_login_response *)ab->ab_data;

//		ac->ac_response = ab->ab_addr;
		ac->ac_response_block = login_resp;
		ac->ac_login = ntohs(login_resp->login_id);
		ac->ac_fetch_agent =
		    ((u_int64_t)(ntohs(login_resp->fetch_agent.hi)) << 32) +
		    ntohl(login_resp->fetch_agent.lo);
		ac->ac_reconnect_hold = ntohs(login_resp->reconnect_hold);

		DPRINTF(("Got a valid response\n"));
		DPRINTF(("Login ID : 0x%04x, Command Agent : 0x%016qx\n",
		    ac->ac_login, ac->ac_fetch_agent));

		ac->ac_valid |= 1;
	}
	if (ab->ab_addr == SBP2_LOGIN_STATUS) {
		MALLOC(cmd_status, struct sbp2_status_block *,
		    sizeof(*cmd_status), M_1394DATA, M_WAITOK);
		MPRINTF("MALLOC(1394DATA)", cmd_status);
		bzero(cmd_status, sizeof(*cmd_status));
		bcopy(ab->ab_data, cmd_status, ab->ab_retlen);

		/* Reset status ab for subsequent notifications. */
		ab->ab_retlen = 0;
		ab->ab_length = sizeof(*cmd_status);

		src = (cmd_status->flags >> 6) & 0x3;
		resp = (cmd_status->flags >> 4) & 0x3;
		dead = (cmd_status->flags >> 3) & 0x1;
		len = cmd_status->flags & 0x7;
		status = cmd_status->status;
		csr = ((u_int64_t)(ntohs(cmd_status->orb_offset_hi)) << 32) +
		    ntohl(cmd_status->orb_offset_lo);
		DPRINTF(("status -- src: %d, resp: %d, dead: %d, len: %d, "
		    "status: %d\nstatus -- csr: 0x%016qx\n", src, resp, dead,
		    (len + 1) * 4, status, (quad_t)csr));
		if (ac->ac_valid & 4) {
			DPRINTF(("Notify callback\n"));
			elm = sbp2_elfind_hash(ac,
			    (u_int32_t)(csr & 0xFFFFFFFF));
			if (elm == NULL) {
				DPRINTF(("%s: no element found for hash"
				    " 0x%08x\n", __func__,
				    (u_int32_t)(csr & 0xFFFFFFFF)));
				FREE(cmd_status, M_1394DATA);
				MPRINTF("FREE(1394DATA)", cmd_status);
				cmd_status = NULL;	/* XXX */
				goto leave;
			}
			MALLOC(status_notify, struct sbp2_status_notification *,
			    sizeof(*status_notify), M_1394CTL, M_WAITOK);
			MPRINTF("MALLOC(1394CTL)", status_notify);
			status_notify->origin = elm->elm_orb;
			status_notify->status = cmd_status;
			elm->elm_cb(elm->elm_cbarg, status_notify);
			FREE(status_notify, M_1394CTL);
			MPRINTF("FREE(1394CTL)", status_notify);
			status_notify = NULL;	/* XXX */
		} else if (((src & 2) == 0) && (resp == 0) && (dead == 0) &&
		    (status == 0) && (csr == SBP2_LOGIN_ORB)) {
			if (ac->ac_status_block) {
				FREE(ac->ac_status_block, M_1394DATA);
				MPRINTF("FREE(1394DATA)", ac->ac_status_block);
				ac->ac_status_block = NULL;	/* XXX */
			}
			ac->ac_status_block = cmd_status;
			DPRINTF(("Got a valid status\n"));
			ac->ac_valid |= 2;
		} else {
			FREE(cmd_status, M_1394DATA);
			MPRINTF("FREE(1394DATA)", cmd_status);
			cmd_status = NULL;	/* XXX */
		}
	}
	if (ac->ac_valid == 3) {
		DPRINTF(("Valid response : notify callback\n"));
		if (ac->ac_cb != NULL) {
			MALLOC(status_notify, struct sbp2_status_notification *,
			    sizeof(*status_notify), M_1394CTL, M_WAITOK);
			MPRINTF("MALLOC(1394CTL)", status_notify);
			status_notify->origin = ac->ac_mgmt_orb;
			status_notify->status = cmd_status;
			ac->ac_cb(ac->ac_cbarg, status_notify);
			FREE(status_notify, M_1394CTL);
			MPRINTF("FREE(1394CTL)", status_notify);
			status_notify = NULL;	/* XXX */
		}
		ac->ac_valid |= 4;
	}

	/*
	 * Leave the handler for status since unsolicited status will get sent
	 * to the addr specified in the login packet.
	 */

leave:
	if (ab->ab_data != NULL) {
		free(ab->ab_data, M_1394DATA);
		MPRINTF("free(1394DATA)", ab->ab_data);
		ab->ab_data = NULL;
	}

	if (ab->ab_addr != SBP2_LOGIN_STATUS) {
		sc->sc1394_unreg(ab, FALSE);
		FREE(ab, M_1394DATA);
		MPRINTF("FREE(1394DATA)", ab);
		ab = NULL;	/* XXX */
	}

}

void
sbp2_query_logins(struct fwnode_softc *sc, struct sbp2_query_logins_orb *orb,
    void (*cb)(struct sbp2_status_notification *))
{
}

void
sbp2_command_add(struct fwnode_softc *sc, int lun,
    struct sbp2_command_orb *orb, int qlen, void *data,
    void (*cb)(void *, struct sbp2_status_notification *), void *cbarg)
{
	struct ieee1394_abuf *ab, *ab2;
	struct sbp2_account *ac;
	struct sbp2_orb_element *elm, *elast;
	u_int32_t ehash;

	if ((ac = sbp2_acfind(sc, lun)) == NULL) {
		DPRINTF(("%s: destination not initialized\n", __func__));
		return;
	}

	DPRINTF(("%s:", __func__));

	/* Initialise orb address hash. */
	do {
		ehash = arc4random() & 0xFFFFFFFC;	/* "quadlet" addr */
	} while (sbp2_elfind_hash(ac, ehash) != NULL);

	orb->next_orb.flag = htons(SBP2_NULL_ORB);
	elast = TAILQ_LAST(&ac->ac_orb_head, sbp2_orb_tq);
	if (elast != TAILQ_END(&ac->ac_orb_head)) {
		elast->elm_orb->next_orb.hi = htons(SBP2_CMD_ORB >> 32);
		elast->elm_orb->next_orb.lo = htonl(ehash);
	}

	MALLOC(elm, struct sbp2_orb_element *, sizeof(*elm),
	    M_1394CTL, M_WAITOK);
	MPRINTF("MALLOC(1394CTL)", elm);
	bzero(elm, sizeof(*elm));
	elm->elm_hash = ehash;
	elm->elm_orb = orb;
	elm->elm_orblen = qlen;
	elm->elm_data = data;
	elm->elm_datasize = ntohs(orb->data_size);
	elm->elm_cb = cb;
	elm->elm_cbarg = cbarg;
	TAILQ_INSERT_TAIL(&ac->ac_orb_head, elm, elm_chain);

	MALLOC(ab2, struct ieee1394_abuf *, sizeof(*ab2), M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", ab2);
	bzero(ab2, sizeof(*ab2));

	ab2->ab_length = sizeof(u_int32_t) * qlen;
	ab2->ab_tcode = IEEE1394_TCODE_READ_REQUEST_DATABLOCK;
	ab2->ab_retlen = 0;
	ab2->ab_data = NULL;
	ab2->ab_addr = SBP2_CMD_ORB + ehash;
	ab2->ab_cb = sbp2_command_send;
	ab2->ab_cbarg = ac;
	ab2->ab_req = (struct ieee1394_softc *)sc;

	elm->elm_orb_ab = ab2;
	sc->sc1394_inreg(ab2, FALSE);

	if (ac->ac_valid & 8) {
		sbp2_agent_tickle(sc, lun);
	} else {
		MALLOC(ab, struct ieee1394_abuf *, sizeof(*ab),
		    M_1394DATA, M_WAITOK);
		MPRINTF("MALLOC(1394DATA)", ab);
		bzero(ab, sizeof(*ab));

		ab->ab_req = (struct ieee1394_softc *)sc;
		ab->ab_length = 8;
		ab->ab_retlen = 0;
		ab->ab_cb = NULL;
		ab->ab_cbarg = NULL;
		ab->ab_addr = ac->ac_fetch_agent + SBP2_ORB_POINTER;
		ab->ab_tcode = IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK;

		ab->ab_data = malloc(8, M_1394DATA, M_WAITOK);
		MPRINTF("malloc(1394DATA)", ab->ab_data);
		ab->ab_data[0] = htonl((u_int32_t)(SBP2_CMD_ORB >> 32));
		ab->ab_data[1] = htonl(ehash);
		DPRINTF((" CSR = 0x%016qx", ab->ab_addr));
		DPRINTF((", ORB = 0x%016qx\n", SBP2_CMD_ORB + ehash));
		ac->ac_valid |= 8;

		sbp2_agent_reset(sc, lun);

		sc->sc1394_write(ab);
	}

	DPRINTF(("%s: COMMAND submitted\n", __func__));

	return;
}

void
sbp2_command_del(struct fwnode_softc *sc, int lun, struct sbp2_command_orb *orb)
{
	struct sbp2_account *ac;
	struct sbp2_orb_element *elm;

	if ((ac = sbp2_acfind(sc, lun)) == NULL) {
		DPRINTF(("%s: destination not initialized\n", __func__));
		return;
	}

	DPRINTF(("%s:", __func__));

	if ((elm = sbp2_elfind_orb(ac, orb)) == NULL) {
#ifdef	SBP2_DEBUG
		DPRINTF((" ORB not found: 0x%08x\n", (u_int32_t)orb));
#endif	/* SBP2_DEBUG */
		return;
	}

	DPRINTF((" orb=0x%08x len=%d data=0x%08x size=%d\n",
	    (u_int32_t)(elm->elm_orb), elm->elm_orblen,
	    (u_int32_t)(elm->elm_data), elm->elm_datasize));

	if (elm->elm_orb_ab != NULL) {
		sc->sc1394_unreg(elm->elm_orb_ab, FALSE);
		if (elm->elm_orb_ab->ab_data != NULL) {
			free(elm->elm_orb_ab->ab_data, M_1394DATA);
			MPRINTF("free(1394DATA)", elm->elm_orb_ab->ab_data);
			elm->elm_orb_ab->ab_data = NULL;	/* XXX */
		}
		FREE(elm->elm_orb_ab, M_1394DATA);
		MPRINTF("FREE(1394DATA)", elm->elm_orb_ab);
		elm->elm_orb_ab = NULL;	/* XXX */
	}

	TAILQ_REMOVE(&ac->ac_orb_head, elm, elm_chain);
	FREE(elm, M_1394CTL);
	MPRINTF("FREE(1394CTL)", elm);
	elm = NULL;	/* XXX */

	if (TAILQ_EMPTY(&ac->ac_orb_head))
		ac->ac_valid &= ~8;
}

void
sbp2_command_send(struct ieee1394_abuf *ab, int rcode)
{
	struct fwnode_softc *sc = (struct fwnode_softc *)ab->ab_req;
	struct sbp2_account *ac = ab->ab_cbarg;
	struct sbp2_orb_element *elm, *next_elm;
	struct sbp2_command_orb *cmd_orb, *next_orb;
	struct ieee1394_abuf *cmd_ab;
	int i;

	/* Got a read so allocate the buffer and write out the response. */

	if (rcode || (ac == NULL)) {
#ifdef	SBP2_DEBUG
		DPRINTF(("%s: Bad return code: %d\n", __func__, rcode));
#endif	/* SBP2_DEBUG */
		if (ab->ab_data) {
			free(ab->ab_data, M_1394DATA);
			MPRINTF("free(1394DATA)", ab->ab_data);
			ab->ab_data = NULL;	/* XXX */
		}
		FREE(ab, M_1394DATA);
		MPRINTF("FREE(1394DATA)", ab);
		ab = NULL;	/* XXX */
		return;
	}

	if (ab->ab_data != NULL) {
		free(ab->ab_data, M_1394DATA);
		MPRINTF("free(1394DATA)", ab->ab_data);
		ab->ab_data = NULL;
	}

	if ((elm = sbp2_elfind_hash(ac, (u_int32_t)(ab->ab_addr & 0xFFFFFFFF)))
	    == NULL) {
#ifdef	SBP2_DEBUG
		DPRINTF(("%s: ORB not found: 0x%016qx\n", __func__,
		    ab->ab_addr));
#endif	/* SBP2_DEBUG */
		if (ab->ab_data != NULL) {
			free(ab->ab_data, M_1394DATA);
			MPRINTF("free(1394DATA)", ab->ab_data);
			ab->ab_data = NULL;	/* XXX */
		}
		FREE(ab, M_1394DATA);
		MPRINTF("FREE(1394DATA)", ab);
		ab = NULL;	/* XXX */
		return;
	}

	DPRINTF(("%s: orb=0x%08x len=%d data=0x%08x size=%d (l=%d rl=%d)\n",
	    __func__, (u_int32_t)(elm->elm_orb), elm->elm_orblen,
	    (u_int32_t)(elm->elm_data), elm->elm_datasize,
	    ab->ab_length, ab->ab_retlen));

	MALLOC(cmd_ab, struct ieee1394_abuf *, sizeof(*cmd_ab),
	    M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", cmd_ab);
	bcopy(ab, cmd_ab, sizeof(*cmd_ab));

	/* Fill in a command packet. */
	cmd_orb = elm->elm_orb;
	if ((next_elm = TAILQ_NEXT(elm, elm_chain))
	    != TAILQ_END(&ac->ac_orb_head)) {
		next_orb = next_elm->elm_orb;
		cmd_orb->next_orb.flag = 0x0000;
		cmd_orb->next_orb.hi = htons(SBP2_CMD_ORB >> 32);
		cmd_orb->next_orb.lo = htonl(next_elm->elm_hash);
	} else {
		cmd_orb->next_orb.flag = htons(SBP2_NULL_ORB);
		cmd_orb->next_orb.hi = 0x0000;
		cmd_orb->next_orb.lo = 0x00000000;
	}

	cmd_ab->ab_retlen = 0;
	cmd_ab->ab_cb = NULL;
	cmd_ab->ab_cbarg = NULL;
	cmd_ab->ab_tcode = IEEE1394_TCODE_READ_RESPONSE_DATABLOCK;
	cmd_ab->ab_length = ab->ab_retlen;

	cmd_ab->ab_data = malloc(elm->elm_orblen * 4, M_1394DATA, M_WAITOK);
	MPRINTF("malloc(1394DATA)", cmd_ab->ab_data);
	bcopy(cmd_orb, cmd_ab->ab_data, elm->elm_orblen * 4);
	for (i = 0; i < elm->elm_orblen; i++) {
		if ((i % 8) == 0) DPRINTF(("   "));
		DPRINTF((" %08x", ntohl(cmd_ab->ab_data[i])));
		if ((i % 8) == 7 && i != (elm->elm_orblen - 1)) DPRINTF(("\n"));
	}
	DPRINTF(("\n"));

	sc->sc1394_write(cmd_ab);
}

void
sbp2_agent_reset(struct fwnode_softc *sc, int lun)
{
	struct ieee1394_abuf *ab;
	struct sbp2_account *ac;

	if ((ac = sbp2_acfind(sc, lun)) == NULL) {
		DPRINTF(("%s: destination not initialized\n", __func__));
		return;
	}

	DPRINTF(("%s:", __func__));

	MALLOC(ab, struct ieee1394_abuf *, sizeof(*ab), M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", ab);
	bzero(ab, sizeof(*ab));

	ab->ab_req = (struct ieee1394_softc *)sc;
	ab->ab_length = 4;
	ab->ab_retlen = 0;
	ab->ab_cb = NULL;
	ab->ab_cbarg = NULL;
	ab->ab_addr = ac->ac_fetch_agent + SBP2_AGENT_RESET;
	ab->ab_tcode = IEEE1394_TCODE_WRITE_REQUEST_QUADLET;

	ab->ab_data = malloc(4, M_1394DATA, M_WAITOK);
	MPRINTF("malloc(1394DATA)", ab->ab_data);
	ab->ab_data[0] = 0;
	DPRINTF((" CSR = 0x%016qx\n", ab->ab_addr));

	sc->sc1394_write(ab);
	DPRINTF(("%s: AGENT_RESET submitted\n", __func__));

	return;
}

void
sbp2_agent_tickle(struct fwnode_softc *sc, int lun)
{
	struct ieee1394_abuf *ab;
	struct sbp2_account *ac;

	if ((ac = sbp2_acfind(sc, lun)) == NULL) {
		DPRINTF(("%s: destination not initialized\n", __func__));
		return;
	}

	DPRINTF(("%s:", __func__));

	MALLOC(ab, struct ieee1394_abuf *, sizeof(*ab), M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", ab);
	bzero(ab, sizeof(*ab));

	ab->ab_req = (struct ieee1394_softc *)sc;
	ab->ab_length = 4;
	ab->ab_retlen = 0;
	ab->ab_cb = NULL;
	ab->ab_cbarg = NULL;
	ab->ab_addr = ac->ac_fetch_agent + SBP2_DOORBEL;
	ab->ab_tcode = IEEE1394_TCODE_WRITE_REQUEST_QUADLET;

	ab->ab_data = malloc(4, M_1394DATA, M_WAITOK);
	MPRINTF("malloc(1394DATA)", ab->ab_data);
	ab->ab_data[0] = 0;
	DPRINTF((" CSR = 0x%016qx\n", ab->ab_addr));

	sc->sc1394_write(ab);
	DPRINTF(("%s: DOORBEL submitted\n", __func__));

	return;
}

int
sbp2_agent_state(struct fwnode_softc *sc, int lun)
{
	return (0);
}

void
sbp2_task_management(struct fwnode_softc *sc,
    struct sbp2_task_management_orb *orb,
    void (*cb)(struct sbp2_status_notification *))
{
}

