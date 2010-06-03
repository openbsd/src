/*	$OpenBSD: if_vlan_var.h,v 1.20 2010/06/03 16:15:00 naddy Exp $	*/

/*
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/net/if_vlan_var.h,v 1.3 1999/08/28 00:48:24 peter Exp $
 */

#ifndef _NET_IF_VLAN_VAR_H_
#define _NET_IF_VLAN_VAR_H_

#ifdef _KERNEL
#define mc_enm	mc_u.mcu_enm

struct vlan_mc_entry {
	LIST_ENTRY(vlan_mc_entry)	mc_entries;
	union {
		struct ether_multi	*mcu_enm;
	} mc_u;
	struct sockaddr_storage		mc_addr;
};

struct	ifvlan {
	struct	arpcom ifv_ac;	/* make this an interface */
	struct	ifnet *ifv_p;	/* parent interface of this vlan */
	struct	ifv_linkmib {
		int	ifvm_parent;
		u_int16_t ifvm_proto; /* encapsulation ethertype */
		u_int16_t ifvm_tag; /* tag to apply on packets leaving if */
		u_int16_t ifvm_prio; /* prio to apply on packet leaving if */
		u_int16_t ifvm_type; /* non-standard ethertype or 0x8100 */
	}	ifv_mib;
	LIST_HEAD(__vlan_mchead, vlan_mc_entry)	vlan_mc_listhead;
	LIST_ENTRY(ifvlan) ifv_list;
	int ifv_flags;
	void *lh_cookie;
	void *dh_cookie;
};

#define	ifv_if		ifv_ac.ac_if
#define	ifv_tag		ifv_mib.ifvm_tag
#define	ifv_prio	ifv_mib.ifvm_prio
#define	ifv_type	ifv_mib.ifvm_type
#define	IFVF_PROMISC	0x01
#endif /* _KERNEL */

struct	ether_vlan_header {
	u_char	evl_dhost[ETHER_ADDR_LEN];
	u_char	evl_shost[ETHER_ADDR_LEN];
	u_int16_t evl_encap_proto;
	u_int16_t evl_tag;
	u_int16_t evl_proto;
};

#define	EVL_VLID_MASK	0x0FFF
#define	EVL_VLANOFTAG(tag) ((tag) & EVL_VLID_MASK)
#define	EVL_PRIOFTAG(tag) (((tag) >> EVL_PRIO_BITS) & 7)
#define	EVL_ENCAPLEN	4	/* length in octets of encapsulation */
#define	EVL_PRIO_MAX	7
#define	EVL_PRIO_BITS	13

/* sysctl(3) tags, for compatibility purposes */
#define	VLANCTL_PROTO	1
#define	VLANCTL_MAX	2

/*
 * Configuration structure for SIOCSETVLAN and SIOCGETVLAN ioctls.
 */
struct	vlanreq {
	char	vlr_parent[IFNAMSIZ];
	u_short	vlr_tag;
};
#define	SIOCSETVLAN	SIOCSIFGENERIC
#define	SIOCGETVLAN	SIOCGIFGENERIC

#ifdef _KERNEL
extern	int vlan_input(struct ether_header *eh, struct mbuf *m);
#endif /* _KERNEL */
#endif /* _NET_IF_VLAN_VAR_H_ */
