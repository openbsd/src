/*	$OpenBSD: sbp2.c,v 1.4 2003/01/08 06:33:38 tdeval Exp $	*/

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

#if 0
void sbp2_print(struct p1212_dir *);
void sbp2_print_node(struct p1212_key *, void *);
#endif

void sbp2_login_send(struct ieee1394_abuf *, int);
void sbp2_status_resp(struct ieee1394_abuf *, int);
void sbp2_command_send(struct ieee1394_abuf *, int);
void sbp2_reconnect(struct ieee1394_softc *);
void sbp2_reconnect_send(struct ieee1394_abuf *, int);

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

typedef struct sbp2_account {
	struct fwnode_softc	 *ac_softc;
	struct ieee1394_abuf	 *ac_status_ab;
	struct sbp2_task_management_orb *ac_mgmt_orb;
	struct sbp2_status_block *ac_status_block;
	void			 *ac_response_block;
	void			(*ac_cb)(void *,
					 struct sbp2_status_notification *);
	void			 *ac_cbarg;
	u_int64_t		  ac_mgmt_agent;
	u_int64_t		  ac_fetch_agent;
	u_int64_t		  ac_response;
	u_int64_t		  ac_status_fifo;
	u_int16_t		  ac_nodeid;
	u_int16_t		  ac_lun;
	u_int16_t		  ac_login;
	u_int16_t		  ac_reconnect_hold;
	u_int16_t		  ac_valid;
	SLIST_ENTRY(sbp2_account) ac_chain;
} sbp2_account;
static SLIST_HEAD(, sbp2_account) sbp2_ac_head;

typedef struct sbp2_orb_element {
	struct sbp2_account	 *elm_ac;
	struct ieee1394_abuf	 *elm_orb_ab;
	struct sbp2_command_orb	 *elm_orb;
	size_t			  elm_orblen;
	u_int32_t		  elm_hash;
	void			 *elm_data;
	size_t			  elm_datasize;
	void			(*elm_cb)(void *,
					  struct sbp2_status_notification *);
	void			 *elm_cbarg;
	TAILQ_ENTRY(sbp2_orb_element) elm_chain;
} sbp2_orb_element;
static TAILQ_HEAD(sbp2_orb_tq, sbp2_orb_element) sbp2_elm_head;

static int sbp2_ac_valid;

struct sbp2_account *sbp2_acfind(struct fwnode_softc *, int);
struct sbp2_orb_element *sbp2_elfind_hash(u_int32_t);
struct sbp2_orb_element *sbp2_elfind_orb(struct sbp2_command_orb *);
struct sbp2_orb_element *sbp2_elfind_first(struct sbp2_account *);
struct sbp2_orb_element *sbp2_elfind_last(struct sbp2_account *);

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
sbp2_elfind_hash(u_int32_t hash)
{
	struct sbp2_orb_element *elm;

	TAILQ_FOREACH(elm, &sbp2_elm_head, elm_chain) {
		if (elm->elm_hash == hash)
			break;
	}

	return (elm);
}

struct sbp2_orb_element *
sbp2_elfind_orb(struct sbp2_command_orb *orb)
{
	struct sbp2_orb_element *elm;

	TAILQ_FOREACH(elm, &sbp2_elm_head, elm_chain) {
		if (elm->elm_orb == orb)
			break;
	}

	return (elm);
}

struct sbp2_orb_element *
sbp2_elfind_first(struct sbp2_account *ac)
{
	struct sbp2_orb_element *elm;

	TAILQ_FOREACH(elm, &sbp2_elm_head, elm_chain) {
		if (elm->elm_ac == ac)
			break;
	}

	return (elm);
}

struct sbp2_orb_element *
sbp2_elfind_last(struct sbp2_account *ac)
{
	struct sbp2_orb_element *elm;

	TAILQ_FOREACH_REVERSE(elm, &sbp2_elm_head, elm_chain, sbp2_orb_tq) {
		if (elm->elm_ac == ac)
			break;
	}

	return (elm);
}

void
sbp2_print_data(struct p1212_data *data)
{
	struct p1212_key *key = (struct p1212_key *)data;

	switch (key->key_value) {
	case SBP2_KEYVALUE_Command_Set:
		DPRINTF(("SBP2 Command Set: "));
		if (key->val == 0x104D8)
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
	}
}

void
sbp2_print_dir(struct p1212_dir *dir)
{
	u_int8_t dir_type = ((struct p1212_key *)dir)->key_type;

	switch(dir_type) {
	case SBP2_KEYVALUE_Logical_Unit_Directory:
		DPRINTF(("Logical Unit "));
		break;
	default:
	}
}

int
sbp2_init(struct fwnode_softc *sc, struct p1212_dir *unitdir)
{
	struct p1212_key **key;
	struct p1212_dir *dir;
	struct sbp2_account *ac;
	int loc;

#ifdef	SBP2_DEBUG
	if (sbp2debug > 1)
		p1212_print(unitdir);
#endif

	key = p1212_find(unitdir, P1212_KEYTYPE_Offset,
	    SBP2_KEYVALUE_Management_Agent, 0);
	if (key == NULL) return (-1);

	if (!sbp2_ac_valid) {
		SLIST_INIT(&sbp2_ac_head);
		TAILQ_INIT(&sbp2_elm_head);
		sbp2_ac_valid = 1;
	}

	MALLOC(ac, struct sbp2_account *, sizeof(*ac), M_1394CTL, M_WAITOK);
	MPRINTF("MALLOC(1394CTL)", ac);
	bzero(ac, sizeof(*ac));

	loc = key[0]->val;
	DPRINTF(("%s: Node %d (sc 0x%08x):"
	    " UID %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
	    __func__, sc->sc_sc1394.sc1394_node_id, sc,
	    sc->sc_sc1394.sc1394_guid[0], sc->sc_sc1394.sc1394_guid[1],
	    sc->sc_sc1394.sc1394_guid[2], sc->sc_sc1394.sc1394_guid[3],
	    sc->sc_sc1394.sc1394_guid[4], sc->sc_sc1394.sc1394_guid[5],
	    sc->sc_sc1394.sc1394_guid[6], sc->sc_sc1394.sc1394_guid[7]));
	free(key, M_DEVBUF);
	MPRINTF("free(DEVBUF)", key);
	key = NULL;	/* XXX */

	ac->ac_softc = sc;
	ac->ac_nodeid = sc->sc_sc1394.sc1394_node_id;
	ac->ac_login = 0;
	ac->ac_mgmt_agent = CSR_BASE + 4 * loc;
	DPRINTF(("%s: mgmt_agent = 0x%012qx\n", __func__, ac->ac_mgmt_agent));

	if ((key = p1212_find(unitdir, P1212_KEYTYPE_Immediate,
	    SBP2_KEYVALUE_Logical_Unit_Number, 0)) != NULL) {
		ac->ac_lun = (*key)->val & 0xFFFF;
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
			ac->ac_lun = (*key)->val & 0xFFFF;
			free(key, M_DEVBUF);
			MPRINTF("free(DEVBUF)", key);
			key = NULL;	/* XXX */
		}
	}
	DPRINTF(("%s: lun = %04x\n", __func__, ac->ac_lun));

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

	if ((key = p1212_find(unitdir, P1212_KEYTYPE_Immediate,
	    SBP2_KEYVALUE_Logical_Unit_Number, 0)) != NULL) {
		lun = (*key)->val & 0xFFFF;
		if ((ac = sbp2_acfind(sc, lun)) != NULL) {
			DPRINTF(("%s: clean lun %d\n", __func__, lun));
			i = 0;
			TAILQ_FOREACH_REVERSE(elm, &sbp2_elm_head, elm_chain,
			    sbp2_orb_tq) {
				DPRINTF(("%s%d", i++?" ":"", i));
				if (elm != NULL && elm->elm_ac == ac) {
					TAILQ_REMOVE(&sbp2_elm_head, elm,
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
				lun = (*key)->val & 0xFFFF;
				if ((ac = sbp2_acfind(sc, lun)) != NULL) {
					DPRINTF(("%s: clean lun %d\n",
					    __func__, lun));
					TAILQ_FOREACH(elm, &sbp2_elm_head,
					    elm_chain) {
						if (elm != NULL &&
						    elm->elm_ac == ac) {
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
	u_int64_t addr;

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

	addr = SBP2_MGMT_ORB +
	    ((u_int64_t)ac->ac_nodeid << SBP2_NODE_SHIFT) +
	    ((u_int64_t)ac->ac_lun << SBP2_LUN_SHIFT);
	ab->ab_data[0] = htonl((u_int32_t)(addr >> 32));
	ab->ab_data[1] = htonl((u_int32_t)(addr & 0xFFFFFFFF));
	DPRINTF((" CSR = 0x%012qx", ac->ac_mgmt_agent));
	DPRINTF((", ORB = 0x%012qx\n", addr));

	ab2->ab_length = sizeof(struct sbp2_login_orb);
	ab2->ab_tcode = IEEE1394_TCODE_READ_REQUEST_DATABLOCK;
	ab2->ab_retlen = 0;
	ab2->ab_data = NULL;
	ab2->ab_addr = addr;
	ab2->ab_cb = sbp2_login_send;
	ab2->ab_cbarg = ac;
	ab2->ab_req = (struct ieee1394_softc *)sc;

	sc->sc1394_inreg(ab2, FALSE);
	sc->sc1394_write(ab);
	DPRINTF(("%s: LOGIN submitted\n", __func__));
}

void
sbp2_login_send(struct ieee1394_abuf *ab, int rcode)
{
	struct fwnode_softc *sc = (struct fwnode_softc *)ab->ab_req;
	struct sbp2_account *ac = ab->ab_cbarg;
	struct sbp2_login_orb *login_orb;
	struct ieee1394_abuf *stat_ab, *resp_ab, *orb_ab;
	u_int64_t addr;
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

	/* Fill in a login packet. First 2 quads are 0 for password. */
	login_orb = (struct sbp2_login_orb *)ac->ac_mgmt_orb;

	/* Addr for response. */
	addr = SBP2_RESP_BLOCK +
	    ((u_int64_t)ac->ac_nodeid << SBP2_NODE_SHIFT) +
	    ((u_int64_t)ac->ac_lun << SBP2_LUN_SHIFT);
	resp_ab->ab_length = sizeof(struct sbp2_login_response);
	resp_ab->ab_tcode = IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK;
	resp_ab->ab_retlen = 0;
	resp_ab->ab_data = NULL;
	resp_ab->ab_addr = addr;
	resp_ab->ab_cb = sbp2_status_resp;
	resp_ab->ab_cbarg = ac;
	resp_ab->ab_req = orb_ab->ab_req;

	login_orb->login_response.hi = htons((u_int16_t)(addr >> 32));
	login_orb->login_response.lo = htonl((u_int32_t)(addr & 0xFFFFFFFF));
	DPRINTF(("%s: RESP_ORB = 0x%012qx", __func__, addr));

	sc->sc1394_inreg(resp_ab, FALSE);

	/* Set notify and exclusive use bits. */
	login_orb->options = htons(0x8000);
	login_orb->lun = htons(ac->ac_lun);

	/* Password length (0) and login response length (16) */
	login_orb->password_length = htons(0);
	login_orb->login_response_length = htons(16);

	/* Addr for status packet. */
#if 0
	addr = SBP2_STATUS_BLOCK +
	    ((u_int64_t)ac->ac_nodeid << SBP2_NODE_SHIFT) +
	    ((u_int64_t)ac->ac_lun << SBP2_LUN_SHIFT);
#else
	addr = SBP2_STATUS_BLOCK;
#endif
	stat_ab->ab_length = sizeof(struct sbp2_status_block);
	stat_ab->ab_tcode = IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK;
	stat_ab->ab_retlen = 0;
	stat_ab->ab_data = NULL;
	stat_ab->ab_addr = addr;
	stat_ab->ab_cb = sbp2_status_resp;
	stat_ab->ab_cbarg = ac;
	stat_ab->ab_req = orb_ab->ab_req;

	ac->ac_status_ab = stat_ab;
	sc->sc1394_inreg(stat_ab, FALSE);

	login_orb->status_fifo.hi = htons((u_int16_t)(addr >> 32));
	login_orb->status_fifo.lo = htonl((u_int32_t)(addr & 0xFFFFFFFF));
	DPRINTF((", STATUS_ORB = 0x%012qx", addr));

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
	u_int64_t csr, stat_addr = 0;
#ifdef	SBP2_DEBUG
	int i;
#endif	/* SBP2_DEBUG */

	if (!ac) {
		DPRINTF(("%s: Callback Arg is NULL\n", __func__));

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
#if 0
	stat_addr = SBP2_STATUS_BLOCK +
	    ((u_int64_t)ac->ac_nodeid << SBP2_NODE_SHIFT) +
	    ((u_int64_t)ac->ac_lun << SBP2_LUN_SHIFT);
#else
	stat_addr = SBP2_STATUS_BLOCK;
#endif

	if (rcode) {
		DPRINTF(("%s: Bad return code: %d\n", __func__, rcode));

		if (ab->ab_data) {
			free(ab->ab_data, M_1394DATA);
			MPRINTF("free(1394DATA)", ab->ab_data);
			ab->ab_data = NULL;	/* XXX */
		}
		if (ab->ab_addr != stat_addr) {
			FREE(ab, M_1394DATA);
			MPRINTF("FREE(1394DATA)", ab);
			ab = NULL;	/* XXX */
		}
		return;
	}

#ifdef	SBP2_DEBUG
	DPRINTF(("%s: CSR = 0x%012qx, ac = 0x%08x", __func__,
	    (quad_t)ab->ab_addr, (u_int32_t)ac));
	for (i = 0; i < (ab->ab_retlen / 4); i++) {
		if ((i % 8) == 0) DPRINTFN(2, ("\n   "));
		DPRINTFN(2, (" %08x", ntohl(ab->ab_data[i])));
	}
	DPRINTF(("\n"));
#endif	/* SBP2_DEBUG */

	if (ab->ab_addr == stat_addr) {
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
		DPRINTF(("  status -- src: %d, resp: %d, dead: %d, len: %d, "
		    "status: %d\n  status -- csr: 0x%012qx\n", src, resp, dead,
		    (len + 1) * 4, status, (quad_t)csr));
		if (ac->ac_valid & 4) {
			DPRINTF(("Notify callback\n"));
			elm = sbp2_elfind_hash((u_int32_t)(csr >>
			    (SBP2_NODE_SHIFT - 8 * sizeof(u_int32_t))));
			if (elm == NULL) {
				DPRINTF(("%s: no element found for hash"
				    " 0x%08x\n", __func__,
				    (u_int32_t)(csr >> (SBP2_NODE_SHIFT -
				     8 * sizeof(u_int32_t)))));
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
		    (status == 0) && (csr == (SBP2_MGMT_ORB +
		     ((u_int64_t)ac->ac_nodeid << SBP2_NODE_SHIFT) +
		     ((u_int64_t)ac->ac_lun << SBP2_LUN_SHIFT)))) {
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

	} else if (ab->ab_addr == (SBP2_RESP_BLOCK +
	    ((u_int64_t)ac->ac_nodeid << SBP2_NODE_SHIFT) +
	    ((u_int64_t)ac->ac_lun << SBP2_LUN_SHIFT))) {
		login_resp = (struct sbp2_login_response *)ab->ab_data;

		ac->ac_response_block = login_resp;
		ac->ac_login = ntohs(login_resp->login_id);
		ac->ac_fetch_agent =
		    ((u_int64_t)(ntohs(login_resp->fetch_agent.hi)) << 32) +
		    ntohl(login_resp->fetch_agent.lo);
		ac->ac_reconnect_hold = ntohs(login_resp->reconnect_hold);

		sc->sc_sc1394.sc1394_callback.cb1394_busreset = sbp2_reconnect;

		DPRINTF(("Got a valid response\n"));
		DPRINTF(("Login ID : 0x%04x, Command Agent : 0x%012qx\n",
		    ac->ac_login, ac->ac_fetch_agent));

		ac->ac_valid |= 1;
	}
	if ((ac->ac_valid & 7) == 3) {
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

	if (ab->ab_addr != stat_addr) {
		sc->sc1394_unreg(ab, FALSE);
		FREE(ab, M_1394DATA);
		MPRINTF("FREE(1394DATA)", ab);
		ab = NULL;	/* XXX */
	}

}

void
sbp2_query_logins(struct fwnode_softc *sc, struct sbp2_query_logins_orb *orb,
    void (*cb)(void *, struct sbp2_status_notification *), void *arg)
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
	u_int64_t addr;
	u_int32_t ehash;

	if ((ac = sbp2_acfind(sc, lun)) == NULL) {
		DPRINTF(("%s: destination not initialized\n", __func__));
		return;
	}

	DPRINTF(("%s:\n", __func__));

	/* Initialise orb address hash. */
	do {
		ehash = arc4random();
	} while (sbp2_elfind_hash(ehash) != TAILQ_END(&sbp2_elm_head));

	addr = SBP2_CMD_ORB +
	    ((u_int64_t)sc->sc_sc1394.sc1394_node_id << SBP2_NODE_SHIFT) +
	    ((u_int64_t)ehash << (SBP2_NODE_SHIFT - 8 * sizeof(u_int32_t)));
	orb->next_orb.flag = htons(SBP2_NULL_ORB);

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
	TAILQ_INSERT_TAIL(&sbp2_elm_head, elm, elm_chain);

	MALLOC(ab2, struct ieee1394_abuf *, sizeof(*ab2), M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", ab2);
	bzero(ab2, sizeof(*ab2));

	ab2->ab_length = sizeof(u_int32_t) * qlen;
	ab2->ab_tcode = IEEE1394_TCODE_READ_REQUEST_DATABLOCK;
	ab2->ab_retlen = 0;
	ab2->ab_data = NULL;
	ab2->ab_addr = addr;
	ab2->ab_cb = sbp2_command_send;
	ab2->ab_cbarg = ac;
	ab2->ab_req = (struct ieee1394_softc *)sc;

	elm->elm_orb_ab = ab2;
	sc->sc1394_inreg(ab2, FALSE);

	elast = sbp2_elfind_last(ac);
	if (elast != TAILQ_END(&sbp2_elm_head)) {
		DPRINTF(("%s: chaining to orb 0x%08x", __func__,
		    elast->elm_orb));
		elast->elm_orb->next_orb.flag = 0;
		elast->elm_orb->next_orb.hi = htons((u_int16_t)(addr >> 32));
		elast->elm_orb->next_orb.lo =
		    htonl((u_int32_t)(addr & 0xFFFFFFFF));
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
		ab->ab_data[0] = htonl((u_int32_t)(addr >> 32));
		ab->ab_data[1] = htonl((u_int32_t)(addr & 0xFFFFFFFF));
		ac->ac_valid |= 8;

		sbp2_agent_reset(sc, lun);

		DPRINTF(("%s: CSR = 0x%012qx", __func__, ab->ab_addr));
		sc->sc1394_write(ab);
	}

	DPRINTF((", ORB = 0x%012qx", addr));
	DPRINTF((", orb = 0x%08x", (u_int32_t)orb));
	DPRINTF((", ac = 0x%08x\n", (u_int32_t)ac));
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

	if ((elm = sbp2_elfind_orb(orb)) == TAILQ_END(&sbp2_elm_head)) {
#ifdef	SBP2_DEBUG
		DPRINTF((" ORB not found: 0x%08x\n", (u_int32_t)orb));
#endif	/* SBP2_DEBUG */
		return;
	}

	DPRINTF((" orb=0x%08x hash=0x%08x len=%d data=0x%08x size=%d\n",
	    (u_int32_t)(elm->elm_orb), elm->elm_hash, elm->elm_orblen,
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

	TAILQ_REMOVE(&sbp2_elm_head, elm, elm_chain);
	FREE(elm, M_1394CTL);
	MPRINTF("FREE(1394CTL)", elm);
	elm = NULL;	/* XXX */

	if (sbp2_elfind_last(ac) == TAILQ_END(&sbp2_elm_head))
		ac->ac_valid &= ~8;
}

void
sbp2_command_send(struct ieee1394_abuf *ab, int rcode)
{
	struct fwnode_softc *sc = (struct fwnode_softc *)ab->ab_req;
	struct sbp2_account *ac = ab->ab_cbarg;
	struct sbp2_orb_element *elm;
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

	if ((elm = sbp2_elfind_hash((u_int32_t)(ab->ab_addr >>
	     (SBP2_NODE_SHIFT - 8 * sizeof(u_int32_t)))))
	    == TAILQ_END(&sbp2_elm_head)) {
#ifdef	SBP2_DEBUG
		DPRINTF(("%s: ORB not found: 0x%012qx\n", __func__,
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

	DPRINTF(("%s: orb=0x%08x hash=0x%08x len=%d data=0x%08x size=%d\n",
	    __func__, (u_int32_t)(elm->elm_orb), elm->elm_hash,
	    elm->elm_orblen, (u_int32_t)(elm->elm_data), elm->elm_datasize));

	MALLOC(cmd_ab, struct ieee1394_abuf *, sizeof(*cmd_ab),
	    M_1394DATA, M_WAITOK);
	MPRINTF("MALLOC(1394DATA)", cmd_ab);
	bcopy(ab, cmd_ab, sizeof(*cmd_ab));

	cmd_ab->ab_retlen = 0;
	cmd_ab->ab_cb = NULL;
	cmd_ab->ab_cbarg = NULL;
	cmd_ab->ab_tcode = IEEE1394_TCODE_READ_RESPONSE_DATABLOCK;
	cmd_ab->ab_length = ab->ab_retlen;

	cmd_ab->ab_data = malloc(elm->elm_orblen * 4, M_1394DATA, M_WAITOK);
	MPRINTF("malloc(1394DATA)", cmd_ab->ab_data);
	bcopy(elm->elm_orb, cmd_ab->ab_data, elm->elm_orblen * 4);
	for (i = 0; i < elm->elm_orblen; i++) {
		if ((i % 8) == 0) DPRINTF(("   "));
		DPRINTF((" %08x", ntohl(cmd_ab->ab_data[i])));
		if ((i % 8) == 7 && i != (elm->elm_orblen - 1)) DPRINTF(("\n"));
	}
	DPRINTF(("\n"));

	sc->sc1394_write(cmd_ab);
}

void
sbp2_reconnect(struct ieee1394_softc *sc)
{
	struct ieee1394_abuf *ab, *orb_ab, *stat_ab;
	struct sbp2_account *ac;
	struct sbp2_reconnect_orb *orb;
	struct fwnode_softc *fwsc = (struct fwnode_softc *)sc;
	u_int16_t old_nodeid;
	u_int64_t addr;

	DPRINTF(("%s:", __func__));

	SLIST_FOREACH(ac, &sbp2_ac_head, ac_chain) {
		if (ac != NULL && ac->ac_softc == fwsc) {

			MALLOC(ab, struct ieee1394_abuf *, sizeof(*ab),
			    M_1394DATA, M_NOWAIT);
			MPRINTF("MALLOC(1394DATA)", ab);
			if (!ab) {
				printf("%s: memory allocation failure.\n",
				    __func__);
				return;
			}
			bzero(ab, sizeof(*ab));
			ab->ab_data = malloc(8, M_1394DATA, M_NOWAIT);
			MPRINTF("malloc(1394DATA)", ab->ab_data);
			if (!ab->ab_data) {
				printf("%s: memory allocation failure.\n",
				    __func__);
				FREE(ab, M_1394DATA);
				MPRINTF("FREE(1394DATA)", ab);
				return;
			}
			MALLOC(orb_ab, struct ieee1394_abuf *, sizeof(*orb_ab),
			    M_1394DATA, M_NOWAIT);
			MPRINTF("MALLOC(1394DATA)", orb_ab);
			if (!orb_ab) {
				printf("%s: memory allocation failure.\n",
				    __func__);
				free(ab->ab_data, M_1394DATA);
				MPRINTF("free(1394DATA)", ab->ab_data);
				FREE(ab, M_1394DATA);
				MPRINTF("FREE(1394DATA)", ab);
				return;
			}
			bzero(orb_ab, sizeof(*orb_ab));
			MALLOC(stat_ab, struct ieee1394_abuf *,
			    sizeof(*stat_ab), M_1394DATA, M_NOWAIT);
			MPRINTF("MALLOC(1394DATA)", stat_ab);
			if (!stat_ab) {
				printf("%s: memory allocation failure.\n",
				    __func__);
				FREE(orb_ab, M_1394DATA);
				MPRINTF("FREE(1394DATA)", orb_ab);
				free(ab->ab_data, M_1394DATA);
				MPRINTF("free(1394DATA)", ab->ab_data);
				FREE(ab, M_1394DATA);
				MPRINTF("FREE(1394DATA)", ab);
				return;
			}
			bzero(stat_ab, sizeof(*stat_ab));
			orb = malloc(sizeof(*orb), M_1394DATA, M_NOWAIT);
			MPRINTF("malloc(1394DATA)", orb);
			if (!orb) {
				printf("%s: memory allocation failure.\n",
				    __func__);
				FREE(stat_ab, M_1394DATA);
				MPRINTF("FREE(1394DATA)", stat_ab);
				FREE(orb_ab, M_1394DATA);
				MPRINTF("FREE(1394DATA)", orb_ab);
				free(ab->ab_data, M_1394DATA);
				MPRINTF("free(1394DATA)", ab->ab_data);
				FREE(ab, M_1394DATA);
				MPRINTF("FREE(1394DATA)", ab);
				return;
			}
			bzero(orb, sizeof(*orb));

			old_nodeid = ac->ac_nodeid;
			ac->ac_nodeid = sc->sc1394_node_id;
			//ac->ac_valid = 1;

			/* Re-register the status block with the new nodeid. */
			sc->sc1394_node_id = old_nodeid;
			fwsc->sc1394_unreg(ac->ac_status_ab, FALSE);
			sc->sc1394_node_id = ac->ac_nodeid;
			fwsc->sc1394_inreg(ac->ac_status_ab, FALSE);

			/* Register a transient status block. */
			addr = SBP2_STATUS_BLOCK +
			    ((u_int64_t)ac->ac_nodeid << SBP2_NODE_SHIFT) +
			    ((u_int64_t)ac->ac_lun << SBP2_LUN_SHIFT) +
			    SBP2_RECONNECT_OFFSET;
			stat_ab->ab_length = sizeof(struct sbp2_status_block);
			stat_ab->ab_tcode =
			    IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK;
			stat_ab->ab_retlen = 0;
			stat_ab->ab_data = NULL;
			stat_ab->ab_addr = addr;
			stat_ab->ab_cb = sbp2_status_resp;
			stat_ab->ab_cbarg = ac;
			stat_ab->ab_req = sc;

			fwsc->sc1394_inreg(stat_ab, FALSE);

			/* Construct the RECONNECT orb. */
			orb->options = htons(SBP2_ORB_RECONNECT);
			orb->login_id = htons(ac->ac_login);
			orb->status_fifo.hi =
			    htons((u_int16_t)(addr >> 32));
			orb->status_fifo.lo =
			    htonl((u_int32_t)(addr & 0xFFFFFFFF));

			addr = SBP2_MGMT_ORB +
			    ((u_int64_t)ac->ac_nodeid << SBP2_NODE_SHIFT) +
			    ((u_int64_t)ac->ac_lun << SBP2_LUN_SHIFT);
			orb_ab->ab_length = sizeof(struct sbp2_reconnect_orb);
			orb_ab->ab_tcode =
			    IEEE1394_TCODE_READ_REQUEST_DATABLOCK;
			orb_ab->ab_retlen = 0;
			orb_ab->ab_data = NULL;
			orb_ab->ab_addr = addr;
			orb_ab->ab_cb = sbp2_reconnect_send;
			orb_ab->ab_cbarg = orb;
			orb_ab->ab_req = sc;

			fwsc->sc1394_inreg(orb_ab, FALSE);

			/* Invoque the RECONNECT management command. */
			ab->ab_req = sc;
			ab->ab_length = 8;
			ab->ab_retlen = 0;
			ab->ab_cb = NULL;
			ab->ab_cbarg = NULL;
			ab->ab_addr = ac->ac_mgmt_agent;
			ab->ab_tcode = IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK;
			ab->ab_data[0] = htonl((u_int32_t)(addr >> 32));
			ab->ab_data[1] = htonl((u_int32_t)(addr & 0xFFFFFFFF));
			DPRINTF((" CSR = 0x%012qx", ac->ac_mgmt_agent));
			DPRINTF((", ORB = 0x%012qx\n", addr));

			fwsc->sc1394_write(ab);
			DPRINTF(("%s: %04x RECONNECT submitted\n", __func__,
			    ac->ac_login));
		}
	}
}

void
sbp2_reconnect_send(struct ieee1394_abuf *ab, int rcode)
{
	struct ieee1394_abuf *orb_ab;
	struct fwnode_softc *sc = (struct fwnode_softc *)ab->ab_req;

	if (rcode) {
		DPRINTF(("%s: Bad return code: %d\n", __func__, rcode));
		if (ab->ab_data > (u_int32_t *)1) {
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
	    M_1394DATA, M_NOWAIT);
	MPRINTF("MALLOC(1394DATA)", orb_ab);
	if (!orb_ab) {
		printf("%s: memory allocation failure.\n",
		    __func__);
		return;
	}

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

	if (!orb_ab->ab_cbarg) {
		DPRINTF(("%s: orb lost !\n", __func__));
		FREE(orb_ab, M_1394DATA);
		MPRINTF("FREE(1394DATA)", orb_ab);
		orb_ab = NULL;	/* XXX */
		return;
	}
	orb_ab->ab_data = (u_int32_t *)orb_ab->ab_cbarg;
	orb_ab->ab_length = sizeof(struct sbp2_reconnect_orb);
	orb_ab->ab_retlen = 0;
	orb_ab->ab_cb = NULL;
	orb_ab->ab_cbarg = NULL;
	orb_ab->ab_tcode = IEEE1394_TCODE_READ_RESPONSE_DATABLOCK;

	sc->sc1394_write(orb_ab);
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
	DPRINTF((" CSR = 0x%012qx\n", ab->ab_addr));

	sc->sc1394_write(ab);
	DPRINTF(("%s: AGENT_RESET submitted\n", __func__));

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
	DPRINTF((" CSR = 0x%012qx\n", ab->ab_addr));

	sc->sc1394_write(ab);
	DPRINTF(("%s: DOORBEL submitted\n", __func__));

}

int
sbp2_agent_state(struct fwnode_softc *sc, int lun)
{
	return (0);
}

void
sbp2_task_management(struct fwnode_softc *sc,
    struct sbp2_task_management_orb *orb,
    void (*cb)(void *, struct sbp2_status_notification *), void *arg)
{
}

