/*	$OpenBSD: hci_raw.c,v 1.3 2005/01/17 18:12:49 mickey Exp $	*/

/*
 * ng_btsocket_hci_raw.c
 *
 * Copyright (c) 2001-2002 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/netgraph/bluetooth/socket/ng_btsocket_hci_raw.c,v 1.16 2004/10/18 22:19:42 rwatson Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/filedesc.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>
#include <netbt/bt.h>
#include <netbt/hci_var.h>

#if 0
/* Netgraph node methods */
static ng_constructor_t	ng_btsocket_hci_raw_node_constructor;
static ng_rcvmsg_t	ng_btsocket_hci_raw_node_rcvmsg;
static ng_shutdown_t	ng_btsocket_hci_raw_node_shutdown;
static ng_newhook_t	ng_btsocket_hci_raw_node_newhook;
static ng_connect_t	ng_btsocket_hci_raw_node_connect;
static ng_rcvdata_t	ng_btsocket_hci_raw_node_rcvdata;
static ng_disconnect_t	ng_btsocket_hci_raw_node_disconnect;
#endif

void 			ng_btsocket_hci_raw_output(void *, int);
void			ng_btsocket_hci_raw_savctl(ng_btsocket_hci_raw_pcb_p, 
						   struct mbuf **,
						   struct mbuf *);
int			ng_btsocket_hci_raw_filter(ng_btsocket_hci_raw_pcb_p,
						   struct mbuf *, int);

int ng_btsocket_hci_raw_send_ngmsg(char *, int, void *, int);
int ng_btsocket_hci_raw_send_sync_ngmsg(ng_btsocket_hci_raw_pcb_p, char *,
	int, void *, int);

#define ng_btsocket_hci_raw_wakeup_input_task() \
	taskqueue_enqueue(taskqueue_swi_giant, &ng_btsocket_hci_raw_task)

/* Security filter */
struct ng_btsocket_hci_raw_sec_filter {
	bitstr_t	bit_decl(events, 0xff);
	bitstr_t	bit_decl(commands[0x3f], 0x3ff);
};

#if 0
/* Netgraph type descriptor */
static struct ng_type typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_BTSOCKET_HCI_RAW_NODE_TYPE,
	.constructor =	ng_btsocket_hci_raw_node_constructor,
	.rcvmsg =	ng_btsocket_hci_raw_node_rcvmsg,
	.shutdown =	ng_btsocket_hci_raw_node_shutdown,
	.newhook =	ng_btsocket_hci_raw_node_newhook,
	.connect =	ng_btsocket_hci_raw_node_connect,
	.rcvdata =	ng_btsocket_hci_raw_node_rcvdata,
	.disconnect =	ng_btsocket_hci_raw_node_disconnect,
};
#endif

/* Globals */
extern int					ifqmaxlen;
static u_int32_t				ng_btsocket_hci_raw_debug_level;
static u_int32_t				ng_btsocket_hci_raw_ioctl_timeout;
#if 0
static node_p					ng_btsocket_hci_raw_node;
#endif
static struct ng_bt_itemq			ng_btsocket_hci_raw_queue;
#if 0
static struct mtx				ng_btsocket_hci_raw_queue_mtx;
static struct task				ng_btsocket_hci_raw_task;
#endif
static LIST_HEAD(, ng_btsocket_hci_raw_pcb)	ng_btsocket_hci_raw_sockets;
#if 0
static struct mtx				ng_btsocket_hci_raw_sockets_mtx;
#endif
static u_int32_t				ng_btsocket_hci_raw_token;
#if 0
static struct mtx				ng_btsocket_hci_raw_token_mtx;
#endif
static struct ng_btsocket_hci_raw_sec_filter	*ng_btsocket_hci_raw_sec_filter;

extern struct ifqueue btintrq;

/* Debug */
#define NG_BTSOCKET_HCI_RAW_INFO \
	if (ng_btsocket_hci_raw_debug_level >= NG_BTSOCKET_INFO_LEVEL) \
		printf

#define NG_BTSOCKET_HCI_RAW_WARN \
	if (ng_btsocket_hci_raw_debug_level >= NG_BTSOCKET_WARN_LEVEL) \
		printf

#define NG_BTSOCKET_HCI_RAW_ERR \
	if (ng_btsocket_hci_raw_debug_level >= NG_BTSOCKET_ERR_LEVEL) \
		printf

#define NG_BTSOCKET_HCI_RAW_ALERT \
	if (ng_btsocket_hci_raw_debug_level >= NG_BTSOCKET_ALERT_LEVEL) \
		printf

#if 0
/****************************************************************************
 ****************************************************************************
 **                          Netgraph specific
 ****************************************************************************
 ****************************************************************************/

/*
 * Netgraph node constructor. Do not allow to create node of this type.
 */

int
ng_btsocket_hci_raw_node_constructor(node_p node)
{
	return (EINVAL);
} /* ng_btsocket_hci_raw_node_constructor */

/*
 * Netgraph node destructor. Just let old node go and create new fresh one.
 */

int
ng_btsocket_hci_raw_node_shutdown(node_p node)
{
	int	error = 0;

	NG_NODE_UNREF(node);

	error = ng_make_node_common(&typestruct, &ng_btsocket_hci_raw_node);
	if (error  != 0) {
		NG_BTSOCKET_HCI_RAW_ALERT(
"%s: Could not create Netgraph node, error=%d\n", __func__, error);

		ng_btsocket_hci_raw_node = NULL;

		return (ENOMEM);
        }

	error = ng_name_node(ng_btsocket_hci_raw_node,
				NG_BTSOCKET_HCI_RAW_NODE_TYPE);
	if (error != 0) {
		NG_BTSOCKET_HCI_RAW_ALERT(
"%s: Could not name Netgraph node, error=%d\n", __func__, error);

		NG_NODE_UNREF(ng_btsocket_hci_raw_node);
		ng_btsocket_hci_raw_node = NULL;

		return (EINVAL);
	}

	return (0);
} /* ng_btsocket_hci_raw_node_shutdown */

/*
 * Create new hook. Just say "yes"
 */

int
ng_btsocket_hci_raw_node_newhook(node_p node, hook_p hook, char const *name)
{
	return (0);
} /* ng_btsocket_hci_raw_node_newhook */

/*
 * Connect hook. Just say "yes"
 */

int
ng_btsocket_hci_raw_node_connect(hook_p hook)
{
	return (0);
} /* ng_btsocket_hci_raw_node_connect */

/*
 * Disconnect hook
 */

int
ng_btsocket_hci_raw_node_disconnect(hook_p hook)
{
	return (0);
} /* ng_btsocket_hci_raw_node_disconnect */

/*
 * Receive control message.
 * Make sure it is a message from HCI node and it is a response.
 * Enqueue item and schedule input task.
 */

int
ng_btsocket_hci_raw_node_rcvmsg(node_p node, item_p item, hook_p lasthook) 
{
	struct ng_mesg	*msg = NGI_MSG(item); /* item still has message */
	int		 empty, error = 0;

	mtx_lock(&ng_btsocket_hci_raw_sockets_mtx);
	empty = LIST_EMPTY(&ng_btsocket_hci_raw_sockets);
	mtx_unlock(&ng_btsocket_hci_raw_sockets_mtx);

	if (empty) {
		NG_FREE_ITEM(item);
		return (0);
	}

	if (msg != NULL &&
	    msg->header.typecookie == NGM_HCI_COOKIE &&
	    msg->header.flags & NGF_RESP) {
		if (msg->header.token == 0) {
			NG_FREE_ITEM(item);
			return (0);
		}

		mtx_lock(&ng_btsocket_hci_raw_queue_mtx);
		if (NG_BT_ITEMQ_FULL(&ng_btsocket_hci_raw_queue)) {
			NG_BTSOCKET_HCI_RAW_ERR(
"%s: Input queue is full\n", __func__);

			NG_BT_ITEMQ_DROP(&ng_btsocket_hci_raw_queue);
			NG_FREE_ITEM(item);
			error = ENOBUFS;
		} else {
			NG_BT_ITEMQ_ENQUEUE(&ng_btsocket_hci_raw_queue, item);
			error = ng_btsocket_hci_raw_wakeup_input_task();
		}
		mtx_unlock(&ng_btsocket_hci_raw_queue_mtx);
	} else {
		NG_FREE_ITEM(item);
		error = EINVAL;
	}

	return (error);
} /* ng_btsocket_hci_raw_node_rcvmsg */
#endif

/*
 * Receive packet from the one of our hook.
 * Prepend every packet with sockaddr_hci and record sender's node name.
 * Enqueue item and schedule input task.
 */

int
ng_btsocket_hci_raw_node_rcvdata(struct ifnet *ifp, struct mbuf *m)
{
	struct mbuf	*nam = NULL;
	int		 empty, error;
	int s;

	mtx_lock(&ng_btsocket_hci_raw_sockets_mtx);
	empty = LIST_EMPTY(&ng_btsocket_hci_raw_sockets);
	mtx_unlock(&ng_btsocket_hci_raw_sockets_mtx);

	if (empty) {
#if 0
		NG_FREE_ITEM(item);
#endif
		return (0);
	}

	MGET(nam, M_DONTWAIT, MT_SONAME);
	if (nam != NULL) {
		struct sockaddr_hci	*sa = mtod(nam, struct sockaddr_hci *);

		nam->m_len = sizeof(struct sockaddr_hci);

		sa->hci_len = sizeof(*sa);
		sa->hci_family = AF_BLUETOOTH;
#if 0
		strlcpy(sa->hci_node, NG_PEER_NODE_NAME(hook),
			sizeof(sa->hci_node));
#endif
		strlcpy(sa->hci_node, ifp->if_xname, sizeof(sa->hci_node));

#if 0
		NGI_GET_M(item, nam->m_next);
		NGI_M(item) = nam;
#endif
		nam->m_next = m;

		mtx_lock(&ng_btsocket_hci_raw_queue_mtx);
		if (NG_BT_ITEMQ_FULL(&ng_btsocket_hci_raw_queue)) {
			NG_BTSOCKET_HCI_RAW_ERR(
"%s: Input queue is full\n", __func__);

			NG_BT_ITEMQ_DROP(&ng_btsocket_hci_raw_queue);
#if 0
			NG_FREE_ITEM(item);
#endif
			error = ENOBUFS;
		} else {
#if 0
			NG_BT_ITEMQ_ENQUEUE(&ng_btsocket_hci_raw_queue, item);
			error = ng_btsocket_hci_raw_wakeup_input_task();
#endif
			s = splimp();
			IF_INPUT_ENQUEUE(&btintrq, nam);
			splx(s);
			schednetisr(NETISR_BT);
		}
		mtx_unlock(&ng_btsocket_hci_raw_queue_mtx);
	} else {
		NG_BTSOCKET_HCI_RAW_ERR(
"%s: Failed to allocate address mbuf\n", __func__);

#if 0
		NG_FREE_ITEM(item);
#endif
		error = ENOBUFS;
	}

	return (error);
} /* ng_btsocket_hci_raw_node_rcvdata */

/****************************************************************************
 ****************************************************************************
 **                              Sockets specific
 ****************************************************************************
 ****************************************************************************/

/*
 * Get next token. We need token to avoid theoretical race where process
 * submits ioctl() message then interrupts ioctl() and re-submits another
 * ioctl() on the same socket *before* first ioctl() complete.
 */

#if 0
void
ng_btsocket_hci_raw_get_token(u_int32_t *token)
{
	mtx_lock(&ng_btsocket_hci_raw_token_mtx);
  
	if (++ ng_btsocket_hci_raw_token == 0)
		ng_btsocket_hci_raw_token = 1;
 
	*token = ng_btsocket_hci_raw_token;
 
	mtx_unlock(&ng_btsocket_hci_raw_token_mtx);
} /* ng_btsocket_hci_raw_get_token */
#endif

/*
 * Send Netgraph message to the node - do not expect reply
 */

int
ng_btsocket_hci_raw_send_ngmsg(char *path, int cmd, void *arg, int arglen)
{
#if 0
	struct ng_mesg	*msg = NULL;
	int		 error = 0;

	NG_MKMESSAGE(msg, NGM_HCI_COOKIE, cmd, arglen, M_NOWAIT);
	if (msg == NULL)
		return (ENOMEM);

	if (arg != NULL && arglen > 0)
		bcopy(arg, msg->data, arglen);

	NG_SEND_MSG_PATH(error, ng_btsocket_hci_raw_node, msg, path, 0);

	return (error);
#endif
	return (0);
} /* ng_btsocket_hci_raw_send_ngmsg */

/*
 * Send Netgraph message to the node (no data) and wait for reply 
 */

int
ng_btsocket_hci_raw_send_sync_ngmsg(ng_btsocket_hci_raw_pcb_p pcb, char *path,
		int cmd, void *rsp, int rsplen)
{
#if 0
	struct ng_mesg	*msg = NULL;
	int		 error = 0;

	mtx_assert(&pcb->pcb_mtx, MA_OWNED);

	NG_MKMESSAGE(msg, NGM_HCI_COOKIE, cmd, 0, M_NOWAIT);
	if (msg == NULL)
		return (ENOMEM);

	ng_btsocket_hci_raw_get_token(&msg->header.token);
	pcb->token = msg->header.token;
	pcb->msg = NULL;

	NG_SEND_MSG_PATH(error, ng_btsocket_hci_raw_node, msg, path, 0);
	if (error != 0) {
		pcb->token = 0;
		return (error);
	}

	error = msleep(&pcb->msg, &pcb->pcb_mtx, PZERO|PCATCH, "hcictl", 
			ng_btsocket_hci_raw_ioctl_timeout * hz);
	pcb->token = 0;

	if (error != 0)
		return (error);

	if (pcb->msg != NULL && pcb->msg->header.cmd == cmd)
		bcopy(pcb->msg->data, rsp, rsplen);
	else
		error = EINVAL;

	NG_FREE_MSG(pcb->msg); /* checks for != NULL */
#endif

	return (0);
} /* ng_btsocket_hci_raw_send_sync_ngmsg */

/*
 * Create control information for the packet
 */

void
ng_btsocket_hci_raw_savctl(ng_btsocket_hci_raw_pcb_p pcb, struct mbuf **ctl,
		struct mbuf *m) 
{
	int		dir;
#if 0
	struct timeval	tv;
#endif

	mtx_assert(&pcb->pcb_mtx, MA_OWNED);

	if (pcb->flags & NG_BTSOCKET_HCI_RAW_DIRECTION) {
		dir = (m->m_flags & M_PROTO1)? 1 : 0;
		*ctl = sbcreatecontrol((caddr_t) &dir, sizeof(dir),
					SCM_HCI_RAW_DIRECTION, SOL_HCI_RAW);
		if (*ctl != NULL)
			ctl = &((*ctl)->m_next);
	}

#if 0
	if (pcb->so->so_options & SO_TIMESTAMP) {
		microtime(&tv);
		*ctl = sbcreatecontrol((caddr_t) &tv, sizeof(tv),
					SCM_TIMESTAMP, SOL_SOCKET);
		if (*ctl != NULL)
			ctl = &((*ctl)->m_next);
	}
#endif
} /* ng_btsocket_hci_raw_savctl */

/*
 * Raw HCI sockets data input routine
 */

void
ng_btsocket_hci_raw_data_input(struct mbuf *nam)
{
	ng_btsocket_hci_raw_pcb_p	 pcb = NULL;
	struct mbuf			*m0 = NULL, *m = NULL;
	struct sockaddr_hci		*sa = NULL;

	m0 = nam->m_next;
	nam->m_next = NULL;

	KASSERT((nam->m_type == MT_SONAME),
		("%s: m_type=%d\n", __func__, nam->m_type));
	KASSERT((m0->m_flags & M_PKTHDR),
		("%s: m_flags=%#x\n", __func__, m0->m_flags));

	sa = mtod(nam, struct sockaddr_hci *);

	mtx_lock(&ng_btsocket_hci_raw_sockets_mtx);

	LIST_FOREACH(pcb, &ng_btsocket_hci_raw_sockets, next) {

		mtx_lock(&pcb->pcb_mtx);

		/*
		 * If socket was bound then check address and
		 *  make sure it matches.
		 */

		if (pcb->addr.hci_node[0] != 0 &&
		    strcmp(sa->hci_node, pcb->addr.hci_node) != 0)
			goto next;

		/*
		 * Check packet against filters
		 * XXX do we have to call m_pullup() here?
		 */

#if 0
		if (ng_btsocket_hci_raw_filter(pcb, m0, 1) != 0)
			goto next;
#endif

		/*
		 * Make a copy of the packet, append to the socket's
		 * receive queue and wakeup socket. sbappendaddr()
		 * will check if socket has enough buffer space.
		 */

#if 0
		m = m_dup(m0, M_DONTWAIT);
#endif
		m = m_copym2(m0, 0, M_COPYALL, M_DONTWAIT);
		if (m != NULL) {
			struct mbuf	*ctl = NULL;

			ng_btsocket_hci_raw_savctl(pcb, &ctl, m);

			if (sbappendaddr(&pcb->so->so_rcv, 
					(struct sockaddr *) sa, m, ctl)) {
				sorwakeup(pcb->so);
			} else {
				NG_BTSOCKET_HCI_RAW_INFO(
"%s: sbappendaddr() failed\n", __func__);

				NG_FREE_M(m);
				NG_FREE_M(ctl);
			}
		}
next:
		mtx_unlock(&pcb->pcb_mtx);
	}

	mtx_unlock(&ng_btsocket_hci_raw_sockets_mtx);

	NG_FREE_M(nam);
	NG_FREE_M(m0);
} /* ng_btsocket_hci_raw_data_input */

#if 0
/*
 * Raw HCI sockets message input routine
 */

void
ng_btsocket_hci_raw_msg_input(struct ng_mesg *msg)
{
	ng_btsocket_hci_raw_pcb_p	 pcb = NULL;

	mtx_lock(&ng_btsocket_hci_raw_sockets_mtx);

	LIST_FOREACH(pcb, &ng_btsocket_hci_raw_sockets, next) {
		mtx_lock(&pcb->pcb_mtx);

		if (msg->header.token == pcb->token) {
			pcb->msg = msg;
			wakeup(&pcb->msg);

			mtx_unlock(&pcb->pcb_mtx);
			mtx_unlock(&ng_btsocket_hci_raw_sockets_mtx);

			return;
		}

		mtx_unlock(&pcb->pcb_mtx);
	}

	mtx_unlock(&ng_btsocket_hci_raw_sockets_mtx);

#if 0
	NG_FREE_MSG(msg); /* checks for != NULL */
#endif
} /* ng_btsocket_hci_raw_msg_input */
#endif

#if 0
/*
 * Raw HCI sockets input routines
 */

void
ng_btsocket_hci_raw_input(void *context, int pending)
{
	item_p	item = NULL;

	for (;;) {
		mtx_lock(&ng_btsocket_hci_raw_queue_mtx);
		NG_BT_ITEMQ_DEQUEUE(&ng_btsocket_hci_raw_queue, item);
		mtx_unlock(&ng_btsocket_hci_raw_queue_mtx);

		if (item == NULL)
			break;

		switch(item->el_flags & NGQF_TYPE) {
		case NGQF_DATA: {
			struct mbuf	*m = NULL;

			NGI_GET_M(item, m);
			ng_btsocket_hci_raw_data_input(m);
			} break;

		case NGQF_MESG: {
			struct ng_mesg	*msg = NULL;

			NGI_GET_MSG(item, msg);
			ng_btsocket_hci_raw_msg_input(msg);
			} break;

		default:
			KASSERT(0, 
("%s: invalid item type=%ld\n", __func__, (item->el_flags & NGQF_TYPE)));
			break;
		}

		NG_FREE_ITEM(item);
	}
} /* ng_btsocket_hci_raw_input */
#endif

/*
 * Raw HCI sockets output routine
 */

void
ng_btsocket_hci_raw_output(void *arg1, int arg2)
{
	struct mbuf		*nam = (struct mbuf *) arg1, *m = NULL;
	struct sockaddr_hci	*sa = NULL;
	struct ifnet		*ifp;
	int			 error;
	int			s;

	m = nam->m_next;
	nam->m_next = NULL;

	KASSERT((nam->m_type == MT_SONAME),
		("%s: m_type=%d\n", __func__, nam->m_type));
	KASSERT((m->m_flags & M_PKTHDR),
		("%s: m_flags=%#x\n", __func__, m->m_flags));

	sa = mtod(nam, struct sockaddr_hci *);

	/*
	 * Find downstream hook
	 * XXX For now access node hook list directly. Should be safe because
	 * we used ng_send_fn() and we should have exclusive lock on the node.
	 */

#if 0
	LIST_FOREACH(hook, &node->nd_hooks, hk_hooks) {
		if (hook == NULL || NG_HOOK_NOT_VALID(hook) || 
		    NG_NODE_NOT_VALID(NG_PEER_NODE(hook)))
			continue;

		if (strcmp(sa->hci_node, NG_PEER_NODE_NAME(hook)) == 0) {
			NG_SEND_DATA_ONLY(error, hook, m); /* sets m to NULL */
			break;
		}
	}
#endif
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_type != IFT_BLUETOOTH)
			continue;

		if (strcmp(sa->hci_node, ifp->if_xname) == 0) {
			s = splimp();
			IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
			if (error) {
				splx(s);
				return;
			}
			if ((ifp->if_flags & IFF_OACTIVE) == 0)
				(*ifp->if_start)(ifp);
			splx(s);
			break;
		}
	}

	NG_FREE_M(nam); /* check for != NULL */
#if 0
	NG_FREE_M(m);
#endif
} /* ng_btsocket_hci_raw_output */

/*
 * Check frame against security and socket filters. 
 * d (direction bit) == 1 means incoming frame.
 */

int
ng_btsocket_hci_raw_filter(ng_btsocket_hci_raw_pcb_p pcb, struct mbuf *m, int d)
{
	int	type, event, opcode;

	mtx_assert(&pcb->pcb_mtx, MA_OWNED);

	switch ((type = *mtod(m, u_int8_t *))) {
	case NG_HCI_CMD_PKT:
		if (!(pcb->flags & NG_BTSOCKET_HCI_RAW_PRIVILEGED)) {
			opcode = letoh16(mtod(m, ng_hci_cmd_pkt_t *)->opcode);
		
			if (!bit_test(
ng_btsocket_hci_raw_sec_filter->commands[NG_HCI_OGF(opcode) - 1],
NG_HCI_OCF(opcode) - 1))
				return (EPERM);
		}

		if (d && !bit_test(pcb->filter.packet_mask, NG_HCI_CMD_PKT - 1))
			return (EPERM);
		break;

	case NG_HCI_ACL_DATA_PKT:
	case NG_HCI_SCO_DATA_PKT:
		if (!(pcb->flags & NG_BTSOCKET_HCI_RAW_PRIVILEGED) ||
		    !bit_test(pcb->filter.packet_mask, type - 1) ||
		    !d)
			return (EPERM);
		break;

	case NG_HCI_EVENT_PKT:
		if (!d)
			return (EINVAL);

		event = mtod(m, ng_hci_event_pkt_t *)->event - 1;

		if (!(pcb->flags & NG_BTSOCKET_HCI_RAW_PRIVILEGED))
			if (!bit_test(ng_btsocket_hci_raw_sec_filter->events, event))
				return (EPERM);

		if (!bit_test(pcb->filter.event_mask, event))
			return (EPERM);
		break;

	default:
		return (EINVAL);
	}

	return (0);
} /* ng_btsocket_hci_raw_filter */

/*
 * Initialize everything
 */

void
hci_raw_init(void)
{
	bitstr_t	*f = NULL;
#if 0
	int		 error = 0;
#endif

#if 0
	ng_btsocket_hci_raw_node = NULL;
#endif
	ng_btsocket_hci_raw_debug_level = NG_BTSOCKET_WARN_LEVEL;
	ng_btsocket_hci_raw_ioctl_timeout = 5;

#if 0
	/* Register Netgraph node type */
	error = ng_newtype(&typestruct);
	if (error != 0) {
		NG_BTSOCKET_HCI_RAW_ALERT(
"%s: Could not register Netgraph node type, error=%d\n", __func__, error);

		return;
	}

	/* Create Netgrapg node */
	error = ng_make_node_common(&typestruct, &ng_btsocket_hci_raw_node);
	if (error != 0) {
		NG_BTSOCKET_HCI_RAW_ALERT(
"%s: Could not create Netgraph node, error=%d\n", __func__, error);

		ng_btsocket_hci_raw_node = NULL;

		return;
        }

	error = ng_name_node(ng_btsocket_hci_raw_node,
				NG_BTSOCKET_HCI_RAW_NODE_TYPE);
	if (error != 0) {
		NG_BTSOCKET_HCI_RAW_ALERT(
"%s: Could not name Netgraph node, error=%d\n", __func__, error);

		NG_NODE_UNREF(ng_btsocket_hci_raw_node);
		ng_btsocket_hci_raw_node = NULL;

		return;
	}
#endif

	/* Create input queue */
	NG_BT_ITEMQ_INIT(&ng_btsocket_hci_raw_queue, ifqmaxlen);
	mtx_init(&ng_btsocket_hci_raw_queue_mtx,
		"btsocks_hci_raw_queue_mtx", NULL, MTX_DEF);
#if 0
	TASK_INIT(&ng_btsocket_hci_raw_task, 0,
		ng_btsocket_hci_raw_input, NULL);
#endif

	/* Create list of sockets */
	LIST_INIT(&ng_btsocket_hci_raw_sockets);
	mtx_init(&ng_btsocket_hci_raw_sockets_mtx,
		"btsocks_hci_raw_sockets_mtx", NULL, MTX_DEF);

	/* Tokens */
	ng_btsocket_hci_raw_token = 0;
	mtx_init(&ng_btsocket_hci_raw_token_mtx,
		"btsocks_hci_raw_token_mtx", NULL, MTX_DEF);

	/* 
	 * Security filter
	 * XXX never FREE()ed
	 */

	ng_btsocket_hci_raw_sec_filter = NULL;

	MALLOC(ng_btsocket_hci_raw_sec_filter, 
		struct ng_btsocket_hci_raw_sec_filter *,
		sizeof(struct ng_btsocket_hci_raw_sec_filter), 
		M_BLUETOOTH, M_NOWAIT);
	if (ng_btsocket_hci_raw_sec_filter == NULL) {
		printf("%s: Could not allocate security filter!\n", __func__);
		return;
	}
	bzero(ng_btsocket_hci_raw_sec_filter,
	    sizeof(struct ng_btsocket_hci_raw_sec_filter));

	/*
	 * XXX How paranoid can we get? 
	 *
	 * Initialize security filter. If bit is set in the mask then
	 * unprivileged socket is allowed to send (receive) this command
	 * (event).
	 */

	/* Enable all events */
	memset(&ng_btsocket_hci_raw_sec_filter->events, 0xff,
		sizeof(ng_btsocket_hci_raw_sec_filter->events)/
			sizeof(ng_btsocket_hci_raw_sec_filter->events[0]));

	/* Disable some critical events */
	f = ng_btsocket_hci_raw_sec_filter->events;
	bit_clear(f, NG_HCI_EVENT_RETURN_LINK_KEYS - 1);
	bit_clear(f, NG_HCI_EVENT_LINK_KEY_NOTIFICATION - 1);
	bit_clear(f, NG_HCI_EVENT_VENDOR - 1);

	/* Commands - Link control */
	f = ng_btsocket_hci_raw_sec_filter->commands[NG_HCI_OGF_LINK_CONTROL-1];
	bit_set(f, NG_HCI_OCF_INQUIRY - 1);
	bit_set(f, NG_HCI_OCF_INQUIRY_CANCEL - 1);
	bit_set(f, NG_HCI_OCF_PERIODIC_INQUIRY - 1);
	bit_set(f, NG_HCI_OCF_EXIT_PERIODIC_INQUIRY - 1);
	bit_set(f, NG_HCI_OCF_REMOTE_NAME_REQ - 1);
	bit_set(f, NG_HCI_OCF_READ_REMOTE_FEATURES - 1);
	bit_set(f, NG_HCI_OCF_READ_REMOTE_VER_INFO - 1);
	bit_set(f, NG_HCI_OCF_READ_CLOCK_OFFSET - 1);

	/* Commands - Link policy */
	f = ng_btsocket_hci_raw_sec_filter->commands[NG_HCI_OGF_LINK_POLICY-1];
	bit_set(f, NG_HCI_OCF_ROLE_DISCOVERY - 1);
	bit_set(f, NG_HCI_OCF_READ_LINK_POLICY_SETTINGS - 1);

	/* Commands - Host controller and baseband */
	f = ng_btsocket_hci_raw_sec_filter->commands[NG_HCI_OGF_HC_BASEBAND-1];
	bit_set(f, NG_HCI_OCF_READ_PIN_TYPE - 1);
	bit_set(f, NG_HCI_OCF_READ_LOCAL_NAME - 1);
	bit_set(f, NG_HCI_OCF_READ_CON_ACCEPT_TIMO - 1);
	bit_set(f, NG_HCI_OCF_READ_PAGE_TIMO - 1);
	bit_set(f, NG_HCI_OCF_READ_SCAN_ENABLE - 1);
	bit_set(f, NG_HCI_OCF_READ_PAGE_SCAN_ACTIVITY - 1);
	bit_set(f, NG_HCI_OCF_READ_INQUIRY_SCAN_ACTIVITY - 1);
	bit_set(f, NG_HCI_OCF_READ_AUTH_ENABLE - 1);
	bit_set(f, NG_HCI_OCF_READ_ENCRYPTION_MODE - 1);
	bit_set(f, NG_HCI_OCF_READ_UNIT_CLASS - 1);
	bit_set(f, NG_HCI_OCF_READ_VOICE_SETTINGS - 1);
	bit_set(f, NG_HCI_OCF_READ_AUTO_FLUSH_TIMO - 1);
	bit_set(f, NG_HCI_OCF_READ_NUM_BROADCAST_RETRANS - 1);
	bit_set(f, NG_HCI_OCF_READ_HOLD_MODE_ACTIVITY - 1);
	bit_set(f, NG_HCI_OCF_READ_XMIT_LEVEL - 1);
	bit_set(f, NG_HCI_OCF_READ_SCO_FLOW_CONTROL - 1);
	bit_set(f, NG_HCI_OCF_READ_LINK_SUPERVISION_TIMO - 1);
	bit_set(f, NG_HCI_OCF_READ_SUPPORTED_IAC_NUM - 1);
	bit_set(f, NG_HCI_OCF_READ_IAC_LAP - 1);
	bit_set(f, NG_HCI_OCF_READ_PAGE_SCAN_PERIOD - 1);
	bit_set(f, NG_HCI_OCF_READ_PAGE_SCAN - 1);

	/* Commands - Informational */
	f = ng_btsocket_hci_raw_sec_filter->commands[NG_HCI_OGF_INFO - 1];
	bit_set(f, NG_HCI_OCF_READ_LOCAL_VER - 1);
	bit_set(f, NG_HCI_OCF_READ_LOCAL_FEATURES - 1);
	bit_set(f, NG_HCI_OCF_READ_BUFFER_SIZE - 1);
	bit_set(f, NG_HCI_OCF_READ_COUNTRY_CODE - 1);
	bit_set(f, NG_HCI_OCF_READ_BDADDR - 1);

	/* Commands - Status */
	f = ng_btsocket_hci_raw_sec_filter->commands[NG_HCI_OGF_STATUS - 1];
	bit_set(f, NG_HCI_OCF_READ_FAILED_CONTACT_CNTR - 1);
	bit_set(f, NG_HCI_OCF_GET_LINK_QUALITY - 1);
	bit_set(f, NG_HCI_OCF_READ_RSSI - 1);

	/* Commands - Testing */
	f = ng_btsocket_hci_raw_sec_filter->commands[NG_HCI_OGF_TESTING - 1];
	bit_set(f, NG_HCI_OCF_READ_LOOPBACK_MODE - 1);
} /* ng_btsocket_hci_raw_init */

/*
 * Abort connection on socket
 */

int
ng_btsocket_hci_raw_abort(struct socket *so)
{
	return (ng_btsocket_hci_raw_detach(so));
} /* ng_btsocket_hci_raw_abort */

/*
 * Create new raw HCI socket
 */

int
ng_btsocket_hci_raw_attach(struct socket *so, int proto, struct proc *p)
{
	ng_btsocket_hci_raw_pcb_p	pcb = so2hci_raw_pcb(so);
	int				error = 0;

	if (pcb != NULL)
		return (EISCONN);

#if 0
	if (ng_btsocket_hci_raw_node == NULL)
		return (EPROTONOSUPPORT);
#endif
	if (proto != BLUETOOTH_PROTO_HCI)
		return (EPROTONOSUPPORT);
	if (so->so_type != SOCK_RAW)
		return (ESOCKTNOSUPPORT);

	error = soreserve(so, NG_BTSOCKET_HCI_RAW_SENDSPACE,
				NG_BTSOCKET_HCI_RAW_RECVSPACE);
	if (error != 0)
		return (error);

	MALLOC(pcb, ng_btsocket_hci_raw_pcb_p, sizeof(*pcb), 
		M_BLUETOOTH, M_NOWAIT);
	if (pcb == NULL)
		return (ENOMEM);
	bzero(pcb, sizeof(*pcb));

	so->so_pcb = (caddr_t) pcb;
	pcb->so = so;

	if (suser(p, 0) == 0)
		pcb->flags |= NG_BTSOCKET_HCI_RAW_PRIVILEGED;

	/*
	 * Set default socket filter. By default socket only accepts HCI
	 * Command_Complete and Command_Status event packets.
	 */

	bit_set(pcb->filter.event_mask, NG_HCI_EVENT_COMMAND_COMPL - 1);
	bit_set(pcb->filter.event_mask, NG_HCI_EVENT_COMMAND_STATUS - 1);

	mtx_init(&pcb->pcb_mtx, "btsocks_hci_raw_pcb_mtx", NULL, MTX_DEF);

	mtx_lock(&ng_btsocket_hci_raw_sockets_mtx);
	LIST_INSERT_HEAD(&ng_btsocket_hci_raw_sockets, pcb, next);
	mtx_unlock(&ng_btsocket_hci_raw_sockets_mtx);

	return (0);
} /* ng_btsocket_hci_raw_attach */

/*
 * Bind raw HCI socket
 */

int
ng_btsocket_hci_raw_bind(struct socket *so, struct sockaddr *nam,
		struct proc *p)
{
	ng_btsocket_hci_raw_pcb_p	 pcb = so2hci_raw_pcb(so);
	struct sockaddr_hci		*sa = (struct sockaddr_hci *) nam;

	if (pcb == NULL)
		return (EINVAL);
#if 0
	if (ng_btsocket_hci_raw_node == NULL)
		return (EINVAL);
#endif

	if (sa == NULL)
		return (EINVAL);
	if (sa->hci_family != AF_BLUETOOTH)
		return (EAFNOSUPPORT);
	if (sa->hci_len != sizeof(*sa))
		return (EINVAL);
	if (sa->hci_node[0] == 0)
		return (EINVAL);

	bcopy(sa, &pcb->addr, sizeof(pcb->addr));

	return (0);
} /* ng_btsocket_hci_raw_bind */

/*
 * Connect raw HCI socket
 */

int
ng_btsocket_hci_raw_connect(struct socket *so, struct sockaddr *nam,
		struct proc *p)
{
	ng_btsocket_hci_raw_pcb_p	 pcb = so2hci_raw_pcb(so);
	struct sockaddr_hci		*sa = (struct sockaddr_hci *) nam;

	if (pcb == NULL)
		return (EINVAL);
#if 0
	if (ng_btsocket_hci_raw_node == NULL)
		return (EINVAL);
#endif

	if (sa == NULL)
		return (EINVAL);
	if (sa->hci_family != AF_BLUETOOTH)
		return (EAFNOSUPPORT);
	if (sa->hci_len != sizeof(*sa))
		return (EINVAL);
	if (sa->hci_node[0] == 0)
		return (EDESTADDRREQ);
	if (bcmp(sa, &pcb->addr, sizeof(pcb->addr)) != 0)
		return (EADDRNOTAVAIL);

	soisconnected(so);

	return (0);
} /* ng_btsocket_hci_raw_connect */

/*
 * Process ioctl on socket
 */

int
ng_btsocket_hci_raw_control(struct socket *so, u_long cmd, caddr_t data,
		struct ifnet *ifp, struct proc *p)
{
	ng_btsocket_hci_raw_pcb_p	 pcb = so2hci_raw_pcb(so);
	char				 path[256];
#if 0
	struct ng_mesg			*msg = NULL;
#endif
	int				 error = 0;

	if (pcb == NULL)
		return (EINVAL);
#if 0
	if (ng_btsocket_hci_raw_node == NULL)
		return (EINVAL);
#endif

	mtx_lock(&pcb->pcb_mtx);

	/* Check if we have device name */
	if (pcb->addr.hci_node[0] == 0) {
		mtx_unlock(&pcb->pcb_mtx);
		return (EHOSTUNREACH);
	}

	/* Check if we have pending ioctl() */
	if (pcb->token != 0) {
		mtx_unlock(&pcb->pcb_mtx);
		return (EBUSY);
	}

	snprintf(path, sizeof(path), "%s:", pcb->addr.hci_node);

	switch (cmd) {
	case SIOC_HCI_RAW_NODE_GET_STATE: {
		struct ng_btsocket_hci_raw_node_state	*p =
			(struct ng_btsocket_hci_raw_node_state *) data;

		error = ng_btsocket_hci_raw_send_sync_ngmsg(pcb, path, 
				NGM_HCI_NODE_GET_STATE,
				&p->state, sizeof(p->state));
		} break;

	case SIOC_HCI_RAW_NODE_INIT:
		if (pcb->flags & NG_BTSOCKET_HCI_RAW_PRIVILEGED)
			error = ng_btsocket_hci_raw_send_ngmsg(path,
					NGM_HCI_NODE_INIT, NULL, 0);
		else
			error = EPERM;
		break;

	case SIOC_HCI_RAW_NODE_GET_DEBUG: {
		struct ng_btsocket_hci_raw_node_debug	*p = 
			(struct ng_btsocket_hci_raw_node_debug *) data;

		error = ng_btsocket_hci_raw_send_sync_ngmsg(pcb, path,
				NGM_HCI_NODE_GET_DEBUG,
				&p->debug, sizeof(p->debug));
		} break;

	case SIOC_HCI_RAW_NODE_SET_DEBUG: {
		struct ng_btsocket_hci_raw_node_debug	*p = 
			(struct ng_btsocket_hci_raw_node_debug *) data;

		if (pcb->flags & NG_BTSOCKET_HCI_RAW_PRIVILEGED)
			error = ng_btsocket_hci_raw_send_ngmsg(path,
					NGM_HCI_NODE_SET_DEBUG, &p->debug,
					sizeof(p->debug));
		else
			error = EPERM;
		} break;

	case SIOC_HCI_RAW_NODE_GET_BUFFER: {
		struct ng_btsocket_hci_raw_node_buffer	*p = 
			(struct ng_btsocket_hci_raw_node_buffer *) data;

		error = ng_btsocket_hci_raw_send_sync_ngmsg(pcb, path,
				NGM_HCI_NODE_GET_BUFFER,
				&p->buffer, sizeof(p->buffer));
		} break;

	case SIOC_HCI_RAW_NODE_GET_BDADDR: {
		struct ng_btsocket_hci_raw_node_bdaddr	*p = 
			(struct ng_btsocket_hci_raw_node_bdaddr *) data;

		error = ng_btsocket_hci_raw_send_sync_ngmsg(pcb, path,
				NGM_HCI_NODE_GET_BDADDR,
				&p->bdaddr, sizeof(p->bdaddr));
		} break;

	case SIOC_HCI_RAW_NODE_GET_FEATURES: {
		struct ng_btsocket_hci_raw_node_features	*p = 
			(struct ng_btsocket_hci_raw_node_features *) data;

		error = ng_btsocket_hci_raw_send_sync_ngmsg(pcb, path,
				NGM_HCI_NODE_GET_FEATURES,
				&p->features, sizeof(p->features));
		} break;

	case SIOC_HCI_RAW_NODE_GET_STAT: {
		struct ng_btsocket_hci_raw_node_stat	*p = 
			(struct ng_btsocket_hci_raw_node_stat *) data;

		error = ng_btsocket_hci_raw_send_sync_ngmsg(pcb, path,
				NGM_HCI_NODE_GET_STAT,
				&p->stat, sizeof(p->stat));
		} break;

	case SIOC_HCI_RAW_NODE_RESET_STAT:
		if (pcb->flags & NG_BTSOCKET_HCI_RAW_PRIVILEGED)
			error = ng_btsocket_hci_raw_send_ngmsg(path,
					NGM_HCI_NODE_RESET_STAT, NULL, 0);
		else
			error = EPERM;
		break;

	case SIOC_HCI_RAW_NODE_FLUSH_NEIGHBOR_CACHE:
		if (pcb->flags & NG_BTSOCKET_HCI_RAW_PRIVILEGED)
			error = ng_btsocket_hci_raw_send_ngmsg(path,
					NGM_HCI_NODE_FLUSH_NEIGHBOR_CACHE,
					NULL, 0);
		else
			error = EPERM;
		break;

	case SIOC_HCI_RAW_NODE_GET_NEIGHBOR_CACHE:  {
		struct ng_btsocket_hci_raw_node_neighbor_cache	*p = 
			(struct ng_btsocket_hci_raw_node_neighbor_cache *) data;
#if 0
		ng_hci_node_get_neighbor_cache_ep		*p1 = NULL;
		ng_hci_node_neighbor_cache_entry_ep		*p2 = NULL;
#endif

		if (p->num_entries <= 0 || 
		    p->num_entries > NG_HCI_MAX_NEIGHBOR_NUM ||
		    p->entries == NULL) {
			error = EINVAL;
			break;
		}

#if 0
		NG_MKMESSAGE(msg, NGM_HCI_COOKIE,
			NGM_HCI_NODE_GET_NEIGHBOR_CACHE, 0, M_NOWAIT);
		if (msg == NULL) {
			error = ENOMEM;
			break;
		}
		ng_btsocket_hci_raw_get_token(&msg->header.token);
		pcb->token = msg->header.token;
		pcb->msg = NULL;

		NG_SEND_MSG_PATH(error, ng_btsocket_hci_raw_node, msg, path, 0);
		if (error != 0) {
			pcb->token = 0;
			break;
		}
#endif

		error = tsleep(&pcb->msg,
				PZERO|PCATCH, "hcictl", 
				ng_btsocket_hci_raw_ioctl_timeout * hz);
		pcb->token = 0;

		if (error != 0)
			break;

#if 0
		if (pcb->msg != NULL &&
		    pcb->msg->header.cmd == NGM_HCI_NODE_GET_NEIGHBOR_CACHE) {
			/* Return data back to user space */
			p1 = (ng_hci_node_get_neighbor_cache_ep *)
				(pcb->msg->data);
			p2 = (ng_hci_node_neighbor_cache_entry_ep *)
				(p1 + 1);

			p->num_entries = min(p->num_entries, p1->num_entries);
			if (p->num_entries > 0)
				error = copyout((caddr_t) p2, 
						(caddr_t) p->entries,
						p->num_entries * sizeof(*p2));
		} else
			error = EINVAL;
#endif

#if 0
		NG_FREE_MSG(pcb->msg); /* checks for != NULL */
#endif
		}break;

	case SIOC_HCI_RAW_NODE_GET_CON_LIST: {
		struct ng_btsocket_hci_raw_con_list	*p = 
			(struct ng_btsocket_hci_raw_con_list *) data;
#if 0
		ng_hci_node_con_list_ep			*p1 = NULL;
		ng_hci_node_con_ep			*p2 = NULL;
#endif

		if (p->num_connections == 0 ||
		    p->num_connections > NG_HCI_MAX_CON_NUM ||
		    p->connections == NULL) {
			error = EINVAL;
			break;
		}

#if 0
		NG_MKMESSAGE(msg, NGM_HCI_COOKIE, NGM_HCI_NODE_GET_CON_LIST,
			0, M_NOWAIT);
		if (msg == NULL) {
			error = ENOMEM;
			break;
		}
		ng_btsocket_hci_raw_get_token(&msg->header.token);
		pcb->token = msg->header.token;
		pcb->msg = NULL;

		NG_SEND_MSG_PATH(error, ng_btsocket_hci_raw_node, msg, path, 0);
		if (error != 0) {
			pcb->token = 0;
			break;
		}
#endif

		error = tsleep(&pcb->msg,
				PZERO|PCATCH, "hcictl", 
				ng_btsocket_hci_raw_ioctl_timeout * hz);
		pcb->token = 0;

		if (error != 0)
			break;

#if 0
		if (pcb->msg != NULL &&
		    pcb->msg->header.cmd == NGM_HCI_NODE_GET_CON_LIST) {
			/* Return data back to user space */
			p1 = (ng_hci_node_con_list_ep *)(pcb->msg->data);
			p2 = (ng_hci_node_con_ep *)(p1 + 1);

			p->num_connections = min(p->num_connections,
						p1->num_connections);
			if (p->num_connections > 0)
				error = copyout((caddr_t) p2, 
					(caddr_t) p->connections,
					p->num_connections * sizeof(*p2));
		} else
			error = EINVAL;
#endif

#if 0
		NG_FREE_MSG(pcb->msg); /* checks for != NULL */
#endif
		} break;

	case SIOC_HCI_RAW_NODE_GET_LINK_POLICY_MASK: {
		struct ng_btsocket_hci_raw_node_link_policy_mask	*p = 
			(struct ng_btsocket_hci_raw_node_link_policy_mask *) 
				data;

		error = ng_btsocket_hci_raw_send_sync_ngmsg(pcb, path,
				NGM_HCI_NODE_GET_LINK_POLICY_SETTINGS_MASK,
				&p->policy_mask, sizeof(p->policy_mask));
		} break;

	case SIOC_HCI_RAW_NODE_SET_LINK_POLICY_MASK: {
		struct ng_btsocket_hci_raw_node_link_policy_mask	*p = 
			(struct ng_btsocket_hci_raw_node_link_policy_mask *) 
				data;

		if (pcb->flags & NG_BTSOCKET_HCI_RAW_PRIVILEGED)
			error = ng_btsocket_hci_raw_send_ngmsg(path,
					NGM_HCI_NODE_SET_LINK_POLICY_SETTINGS_MASK,
					&p->policy_mask,
					sizeof(p->policy_mask));
		else
			error = EPERM;
		} break;

	case SIOC_HCI_RAW_NODE_GET_PACKET_MASK: {
		struct ng_btsocket_hci_raw_node_packet_mask	*p = 
			(struct ng_btsocket_hci_raw_node_packet_mask *) data;

		error = ng_btsocket_hci_raw_send_sync_ngmsg(pcb, path,
				NGM_HCI_NODE_GET_PACKET_MASK,
				&p->packet_mask, sizeof(p->packet_mask));
		} break;

	case SIOC_HCI_RAW_NODE_SET_PACKET_MASK: {
		struct ng_btsocket_hci_raw_node_packet_mask	*p = 
			(struct ng_btsocket_hci_raw_node_packet_mask *) data;

		if (pcb->flags & NG_BTSOCKET_HCI_RAW_PRIVILEGED)
			error = ng_btsocket_hci_raw_send_ngmsg(path,
					NGM_HCI_NODE_SET_PACKET_MASK,
					&p->packet_mask,
					sizeof(p->packet_mask));
		else
			error = EPERM;
		} break;

	case SIOC_HCI_RAW_NODE_GET_ROLE_SWITCH: {
		struct ng_btsocket_hci_raw_node_role_switch	*p = 
			(struct ng_btsocket_hci_raw_node_role_switch *) data;

		error = ng_btsocket_hci_raw_send_sync_ngmsg(pcb, path,
				NGM_HCI_NODE_GET_ROLE_SWITCH,
				&p->role_switch, sizeof(p->role_switch));
		} break;

	case SIOC_HCI_RAW_NODE_SET_ROLE_SWITCH: {
		struct ng_btsocket_hci_raw_node_role_switch	*p = 
			(struct ng_btsocket_hci_raw_node_role_switch *) data;

		if (pcb->flags & NG_BTSOCKET_HCI_RAW_PRIVILEGED)
			error = ng_btsocket_hci_raw_send_ngmsg(path,
					NGM_HCI_NODE_SET_ROLE_SWITCH,
					&p->role_switch,
					sizeof(p->role_switch));
		else
			error = EPERM;
		} break;

	default:
		error = EINVAL;
		break;
	}

	mtx_unlock(&pcb->pcb_mtx);

	return (error);
} /* ng_btsocket_hci_raw_control */

/*
 * Process getsockopt/setsockopt system calls
 */

int
hci_raw_ctloutput(int op, struct socket *so, int level,
    int optname, struct mbuf **m)
{
	ng_btsocket_hci_raw_pcb_p		pcb = so2hci_raw_pcb(so);
	struct ng_btsocket_hci_raw_filter	filter;
	int					error = 0, dir;
	size_t					len;

	if (pcb == NULL)
		return (EINVAL);
#if  0
	if (ng_btsocket_hci_raw_node == NULL)
		return (EINVAL);
#endif

	if (level != SOL_HCI_RAW)
		return (0);

	mtx_lock(&pcb->pcb_mtx);

	switch (op) {
	case PRCO_GETOPT:
		switch (optname) {
		case SO_HCI_RAW_FILTER:
			len = min((*m)->m_len, sizeof(pcb->filter));
			bcopy(&pcb->filter, mtod(*m, void *), len);
			break;

		case SO_HCI_RAW_DIRECTION:
			len = min((*m)->m_len, sizeof(dir));
			dir = (pcb->flags & NG_BTSOCKET_HCI_RAW_DIRECTION)?1:0;
			bcopy(&dir, mtod(*m, void *), len);
			break;

		default:
			error = EINVAL;
			break;
		}
		break;

	case PRCO_SETOPT:
		switch (optname) {
		case SO_HCI_RAW_FILTER:
			len = min((*m)->m_len, sizeof(pcb->filter));
			bcopy(&filter, &pcb->filter, len);
			break;

		case SO_HCI_RAW_DIRECTION:
			len = min((*m)->m_len, sizeof(dir));
			bcopy(mtod(*m, void *), &dir, len);

			if (dir)
				pcb->flags |= NG_BTSOCKET_HCI_RAW_DIRECTION;
			else
				pcb->flags &= ~NG_BTSOCKET_HCI_RAW_DIRECTION;
			break;

		default:
			error = EINVAL;
			break;
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	mtx_unlock(&pcb->pcb_mtx);
	
	return (error);
} /* ng_btsocket_hci_raw_ctloutput */

/*
 * Detach raw HCI socket
 */

int
ng_btsocket_hci_raw_detach(struct socket *so)
{
	ng_btsocket_hci_raw_pcb_p	pcb = so2hci_raw_pcb(so);

	if (pcb == NULL)
		return (EINVAL);
#if 0
	if (ng_btsocket_hci_raw_node == NULL)
		return (EINVAL);
#endif

	mtx_lock(&ng_btsocket_hci_raw_sockets_mtx);
	mtx_lock(&pcb->pcb_mtx);

	LIST_REMOVE(pcb, next);

	mtx_unlock(&pcb->pcb_mtx);
	mtx_unlock(&ng_btsocket_hci_raw_sockets_mtx);

	mtx_destroy(&pcb->pcb_mtx);

	bzero(pcb, sizeof(*pcb));
	FREE(pcb, M_BLUETOOTH);

	ACCEPT_LOCK();
	SOCK_LOCK(so);
	so->so_pcb = NULL;
	sofree(so);

	return (0);
} /* ng_btsocket_hci_raw_detach */

/*
 * Disconnect raw HCI socket
 */

int
ng_btsocket_hci_raw_disconnect(struct socket *so)
{
	ng_btsocket_hci_raw_pcb_p	 pcb = so2hci_raw_pcb(so);

	if (pcb == NULL)
		return (EINVAL);
#if 0
	if (ng_btsocket_hci_raw_node == NULL)
		return (EINVAL);
#endif

	soisdisconnected(so);

	return (0);
} /* ng_btsocket_hci_raw_disconnect */

/*
 * Get socket peer's address
 */

int
ng_btsocket_hci_raw_peeraddr(struct socket *so, struct sockaddr **nam)
{
	return (ng_btsocket_hci_raw_sockaddr(so, nam));
} /* ng_btsocket_hci_raw_peeraddr */

/*
 * Send data
 */

int
ng_btsocket_hci_raw_send(struct socket *so, int flags, struct mbuf *m,
		struct sockaddr *sa, struct mbuf *control, struct proc *p)
{
	ng_btsocket_hci_raw_pcb_p	 pcb = so2hci_raw_pcb(so);
	struct mbuf			*nam = NULL;
	int				 error = 0;

#if 0
	if (ng_btsocket_hci_raw_node == NULL) {
		error = ENETDOWN;
		goto drop;
	}
#endif
	if (pcb == NULL) {
		error = EINVAL;
		goto drop;
	}
	if (control != NULL) {
		error = EINVAL;
		goto drop;
	}

	if (m->m_pkthdr.len < sizeof(ng_hci_cmd_pkt_t) ||
	    m->m_pkthdr.len > sizeof(ng_hci_cmd_pkt_t) + NG_HCI_CMD_PKT_SIZE) {
		error = EMSGSIZE;
		goto drop;
	}

	if (m->m_len < sizeof(ng_hci_cmd_pkt_t)) {
		if ((m = m_pullup(m, sizeof(ng_hci_cmd_pkt_t))) == NULL) {
			error = ENOBUFS;
			goto drop;
		}
	}
	if (*mtod(m, u_int8_t *) != NG_HCI_CMD_PKT) {
		error = EOPNOTSUPP;
		goto drop;
	}

	mtx_lock(&pcb->pcb_mtx);

	error = ng_btsocket_hci_raw_filter(pcb, m, 0);
	if (error != 0) {
		mtx_unlock(&pcb->pcb_mtx);
		goto drop;
	}

	if (sa == NULL) {
		if (pcb->addr.hci_node[0] == 0) {
			mtx_unlock(&pcb->pcb_mtx);
			error = EDESTADDRREQ;
			goto drop;
		}

		sa = (struct sockaddr *) &pcb->addr;
	}

	MGET(nam, M_DONTWAIT, MT_SONAME);
	if (nam == NULL) {
		mtx_unlock(&pcb->pcb_mtx);
		error = ENOBUFS;
		goto drop;
	}

	nam->m_len = sizeof(struct sockaddr_hci);
	bcopy(sa,mtod(nam, struct sockaddr_hci *),sizeof(struct sockaddr_hci));

	nam->m_next = m;
	m = NULL;

	mtx_unlock(&pcb->pcb_mtx);

#if 0
	return (ng_send_fn(ng_btsocket_hci_raw_node, NULL, 
				ng_btsocket_hci_raw_output, nam, 0));
#endif
	ng_btsocket_hci_raw_output(nam, 0);
	return (0);

drop:
	NG_FREE_M(control); /* NG_FREE_M checks for != NULL */
	NG_FREE_M(nam);
	NG_FREE_M(m);
	
	return (error);
} /* ng_btsocket_hci_raw_send */

/*
 * Get socket address
 */

int
ng_btsocket_hci_raw_sockaddr(struct socket *so, struct sockaddr **nam)
{
	ng_btsocket_hci_raw_pcb_p	pcb = so2hci_raw_pcb(so);
	struct sockaddr_hci		sa;

	if (pcb == NULL)
		return (EINVAL);
#if 0
	if (ng_btsocket_hci_raw_node == NULL)
		return (EINVAL);
#endif

	bzero(&sa, sizeof(sa));
	sa.hci_len = sizeof(sa);
	sa.hci_family = AF_BLUETOOTH;
	strlcpy(sa.hci_node, pcb->addr.hci_node, sizeof(sa.hci_node));

#if 0
	*nam = sodupsockaddr((struct sockaddr *) &sa, M_NOWAIT);
#endif

	return ((*nam == NULL)? ENOMEM : 0);
} /* ng_btsocket_hci_raw_sockaddr */

int
hci_raw_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control)
{
	ng_btsocket_hci_raw_pcb_p pcb = so2hci_raw_pcb(so);
	struct sockaddr *sa;
	int error = 0;

	/* XXX: restrict AF_BLUETOOTH sockets to root for now */
	if ((error = suser(curproc, 0)) != 0)
		return (error);

	if (req == PRU_CONTROL)
		return (ng_btsocket_hci_raw_control(so, (u_long)m,
		    (caddr_t)nam, (struct ifnet *)control, curproc));

	if (pcb == NULL && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}

	switch (req) {
	case PRU_ATTACH:
		return (ng_btsocket_hci_raw_attach(so,
		    so->so_proto->pr_protocol, curproc));
	case PRU_DISCONNECT:
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			break;
		}
		/* FALLTHROUGH */
	case PRU_ABORT:
		soisdisconnected(so);
		/* FALLTHROUGH */
	case PRU_DETACH:
		return (ng_btsocket_hci_raw_detach(so));
	case PRU_BIND:
		sa = mtod(nam, struct sockaddr *);
		return (ng_btsocket_hci_raw_bind(so, sa, curproc));
	case PRU_CONNECT:
		sa = mtod(nam, struct sockaddr *);
		return (ng_btsocket_hci_raw_connect(so, sa, curproc));
	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;
	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;
	case PRU_SEND:
		sa = nam != NULL ? mtod(nam, struct sockaddr *) : NULL;
		return (ng_btsocket_hci_raw_send(so, 0, m, sa, control,
		    curproc));
	case PRU_SENSE:
		return (0);
	case PRU_RCVOOB:
	case PRU_RCVD:
	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;
	case PRU_SOCKADDR:
		sa = mtod(nam, struct sockaddr *);
		return (ng_btsocket_hci_raw_sockaddr(so, &sa));
	case PRU_PEERADDR:
		sa = mtod(nam, struct sockaddr *);
		return (ng_btsocket_hci_raw_peeraddr(so, &sa));
	default:
		panic("hci_raw_usrreq: unexpected request %d", req);
	}

release:
	if (m != NULL)
		m_freem(m);
	return (error);
}
