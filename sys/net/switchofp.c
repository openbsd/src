/*	$OpenBSD: switchofp.c,v 1.72 2018/12/28 14:32:47 bluhm Exp $	*/

/*
 * Copyright (c) 2016 Kazuya GODA <goda@openbsd.org>
 * Copyright (c) 2015, 2016 YASUOKA Masahiko <yasuoka@openbsd.org>
 * Copyright (c) 2015, 2016 Reyk Floeter <reyk@openbsd.org>
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/selinfo.h>
#include <sys/stdint.h>
#include <sys/time.h>
#include <sys/pool.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet/if_ether.h>
#include <net/if_bridge.h>
#include <net/if_switch.h>
#include <net/if_vlan_var.h>
#include <net/ofp.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

/*
 * Per-frame debugging can be done at any time using the BPF
 * hook of DLT_OPENFLOW (eg. tcpdump -y openflow -i switch0)
 */
#ifdef DEBUG
#define DPRINTF(__sc, ...)				\
do {							\
	struct switch_softc *_sc = __sc;		\
	log(LOG_DEBUG, "%s: ", _sc->sc_if.if_xname);	\
	addlog(__VA_ARGS__);				\
} while(/*CONSTCOND*/0)
#else
#define DPRINTF(__sc, ...)	do {} while(0)
#endif

struct swofp_flow_entry {
	uint64_t				 swfe_cookie;
	uint16_t				 swfe_priority;
	uint8_t					 swfe_table_id;
	struct ofp_match			*swfe_match;
	LIST_ENTRY(swofp_flow_entry)		 swfe_next;
	uint64_t				 swfe_packet_cnt;
	uint64_t				 swfe_byte_cnt;
	struct ofp_instruction_goto_table	*swfe_goto_table;
	struct ofp_instruction_write_metadata	*swfe_write_metadata;
	struct ofp_instruction_actions		*swfe_write_actions;
	struct ofp_instruction_actions		*swfe_apply_actions;
	struct ofp_instruction_actions		*swfe_clear_actions;
	struct ofp_instruction_meter		*swfe_meter;
	struct ofp_instruction_experimenter	*swfe_experimenter;
	struct timespec				 swfe_installed_time;
	struct timespec				 swfe_idle_time;
	uint16_t				 swfe_idle_timeout;
	uint16_t				 swfe_hard_timeout;
	uint16_t				 swfe_flags;
	int					 swfe_tablemiss;
};

struct swofp_flow_table {
	uint32_t			 swft_table_id;
	TAILQ_ENTRY(swofp_flow_table)	 swft_table_next;
	uint64_t			 swft_lookup_count;
	uint64_t			 swft_matched_count;
	LIST_HEAD(, swofp_flow_entry)	 swft_flow_list;
	uint32_t			 swft_flow_num;
};

struct swofp_group_entry {
	uint32_t			 swge_group_id;
	LIST_ENTRY(swofp_group_entry)	 swge_next;
	uint64_t			 swge_packet_count;
	uint64_t			 swge_byte_count;
	uint32_t			 swge_ref_count;
	uint8_t				 swge_type;
	uint32_t			 swge_buckets_len;
	struct ofp_bucket		*swge_buckets;
	struct ofp_bucket_counter	*swge_bucket_counter; /* XXX */
	struct timespec			 swge_installed_time;
};

struct swofp_action_set {
	uint16_t			 swas_type;
	struct ofp_action_header	*swas_action;
};

/* Same as total number of OFP_ACTION_ */
#define SWOFP_ACTION_SET_MAX		18
struct swofp_pipeline_desc {
	uint32_t			 swpld_table_id;
	uint64_t			 swpld_cookie;
	uint64_t			 swpld_metadata;
	struct switch_flow_classify	*swpld_swfcl;
	struct switch_flow_classify	 swpld_pre_swfcl;
	struct switch_fwdp_queue	 swpld_fwdp_q;
	struct swofp_action_set		 swpld_action_set[SWOFP_ACTION_SET_MAX];
	struct ofp_action_header	*swpld_set_fields[OFP_XM_T_MAX];
	int				 swpld_tablemiss;
};

struct swofp_ofs {
	/* There are parameters by OpenFlow */
	uint32_t			 swofs_xidnxt;
	uint64_t			 swofs_datapath_id;
	struct ofp_switch_config	 swofs_switch_config;
	struct timeout			 swofs_flow_timeout;
	uint32_t			 swofs_flow_max_entry;
	TAILQ_HEAD(, swofp_flow_table)	 swofs_table_list;
	uint32_t			 swofs_group_max_table;
	int				 swofs_group_table_num;
	LIST_HEAD(, swofp_group_entry)	 swofs_group_table;
	int				(*swofp_cp_init)(struct switch_softc *);
};

struct swofp_mpmsg {
	struct mbuf		*swmp_hdr;
	struct mbuf_list	 swmp_body;
};
#define SWOFP_MPMSG_MAX		0xffef

typedef int	(*ofp_msg_handler)(struct switch_softc *, struct mbuf *);

void	 swofp_forward_ofs(struct switch_softc *, struct switch_flow_classify *,
	    struct mbuf *);

int	 swofp_input(struct switch_softc *, struct mbuf *);
int	 swofp_output(struct switch_softc *, struct mbuf *);
void	 swofp_timer(void *);

struct ofp_oxm_class
	*swofp_lookup_oxm_handler(struct ofp_ox_match *);
ofp_msg_handler
	swofp_lookup_msg_handler(uint8_t);
ofp_msg_handler
	swofp_lookup_mpmsg_handler(uint16_t);
struct ofp_action_handler
	*swofp_lookup_action_handler(uint16_t);
ofp_msg_handler
	*swofp_flow_mod_lookup_handler(uint8_t);
struct swofp_pipeline_desc
	*swofp_pipeline_desc_create(struct switch_flow_classify *);
void	 swofp_pipeline_desc_destroy(struct swofp_pipeline_desc *);
int	 swofp_flow_match_by_swfcl(struct ofp_match *,
	    struct switch_flow_classify *);
struct swofp_flow_entry
	*swofp_flow_lookup(struct swofp_flow_table *,
	    struct switch_flow_classify *);

/*
 * Flow table
 */
struct swofp_flow_table
	*swofp_flow_table_lookup(struct switch_softc *, uint16_t);
struct swofp_flow_table
	*swofp_flow_table_add(struct switch_softc *, uint16_t);
int	 swofp_flow_table_delete(struct switch_softc *, uint16_t);
void	 swofp_flow_table_delete_all(struct switch_softc *);
void	 swofp_flow_delete_on_table_by_group(struct switch_softc *,
	    struct swofp_flow_table *, uint32_t);
void	 swofp_flow_delete_on_table(struct switch_softc *,
	    struct swofp_flow_table *, struct ofp_match *, uint16_t,
	    uint64_t, uint64_t cookie_mask, uint32_t,
	    uint32_t, int);

/*
 * Group table
 */
struct swofp_group_entry
	*swofp_group_entry_lookup(struct switch_softc *, uint32_t);
int	 swofp_group_entry_add(struct switch_softc *,
	    struct swofp_group_entry *);
int	 swofp_group_entry_delete(struct switch_softc *,
	    struct swofp_group_entry *);
int	 swofp_group_entry_delete_all(struct switch_softc *);
int	 swofp_validate_buckets(struct switch_softc *, struct mbuf *, uint8_t,
	    uint16_t *, uint16_t *);

/*
 * Flow entry
 */
int	 swofp_flow_entry_put_instructions(struct switch_softc *,
	    struct mbuf *, struct swofp_flow_entry *, uint16_t *, uint16_t *);
void	 swofp_flow_entry_table_free(struct ofp_instruction **);
void	 swofp_flow_entry_instruction_free(struct swofp_flow_entry *);
void	 swofp_flow_entry_free(struct swofp_flow_entry **);
void	 swofp_flow_entry_add(struct switch_softc *, struct swofp_flow_table *,
	    struct swofp_flow_entry *);
void	 swofp_flow_entry_delete(struct switch_softc *,
	    struct swofp_flow_table *, struct swofp_flow_entry *, uint8_t);
int	 swofp_flow_mod_cmd_common_modify(struct switch_softc *,
	    struct mbuf *, int );
int	 swofp_flow_cmp_non_strict(struct swofp_flow_entry *,
	    struct ofp_match *);
int	 swofp_flow_cmp_strict(struct swofp_flow_entry *, struct ofp_match *,
	    uint32_t);
int	 swofp_flow_cmp_common(struct swofp_flow_entry *,
	    struct ofp_match *, int);
struct swofp_flow_entry
	*swofp_flow_search_by_table(struct swofp_flow_table *,
	    struct ofp_match *, uint16_t);
int	 swofp_flow_has_group(struct ofp_instruction_actions *, uint32_t);
int	 swofp_flow_filter_out_port(struct ofp_instruction_actions *,
	    uint32_t);
int	 swofp_flow_filter(struct swofp_flow_entry *, uint64_t, uint64_t,
	    uint32_t, uint32_t);
void	 swofp_flow_timeout(struct switch_softc *);
int	 swofp_validate_oxm(struct ofp_ox_match *, uint16_t *);
int	 swofp_validate_flow_match(struct ofp_match *, uint16_t *);
int	 swofp_validate_flow_instruction(struct switch_softc *,
	    struct ofp_instruction *, size_t, uint16_t *, uint16_t *);
int	 swofp_validate_action(struct switch_softc *sc,
	    struct ofp_action_header *, size_t, uint16_t *);

/*
 * OpenFlow protocol compare oxm
 */
int	swofp_ox_cmp_data(struct ofp_ox_match *,
	    struct ofp_ox_match *, int);
int	swofp_ox_cmp_ipv6_addr(struct ofp_ox_match *,
	    struct ofp_ox_match *, int);
int	swofp_ox_cmp_ipv4_addr(struct ofp_ox_match *,
	    struct ofp_ox_match *, int);
int	swofp_ox_cmp_vlan_vid(struct ofp_ox_match *,
	    struct ofp_ox_match *, int);
int	swofp_ox_cmp_ether_addr(struct ofp_ox_match *,
	    struct ofp_ox_match *, int);
/*
 * OpenFlow protocol match field handlers
 */
int	 swofp_ox_match_ether_addr(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_match_vlan_vid(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_match_ipv6_addr(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_match_ipv4_addr(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_match_uint8(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_match_uint16(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_match_uint32(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_match_uint64(struct switch_flow_classify *,
	    struct ofp_ox_match *);

void	 swofp_ox_match_put_start(struct ofp_match *);
int	 swofp_ox_match_put_end(struct ofp_match *);
int	 swofp_ox_match_put_uint32(struct ofp_match *, uint8_t, uint32_t);
int	 swofp_ox_match_put_uint64(struct ofp_match *, uint8_t, uint64_t);
int	 swofp_nx_match_put(struct ofp_match *, uint8_t, int, caddr_t);

/*
 * OpenFlow protocol push/pop tag action handlers
 */
struct mbuf
	*swofp_action_push_vlan(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *, struct ofp_action_header *);
struct mbuf
	*swofp_action_pop_vlan(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *, struct ofp_action_header *);
struct mbuf
	*swofp_expand_8021q_tag(struct mbuf *);

/*
 * OpenFlow protocol set field action handlers
 */
struct mbuf
	*swofp_apply_set_field_udp(struct mbuf *, int,
	    struct switch_flow_classify *, struct switch_flow_classify *);
struct mbuf
	*swofp_apply_set_field_tcp(struct mbuf *, int,
	    struct switch_flow_classify *, struct switch_flow_classify *);
struct mbuf
	*swofp_apply_set_field_nd6(struct mbuf *, int,
	    struct switch_flow_classify *, struct switch_flow_classify *);
struct mbuf
	*swofp_apply_set_field_icmpv6(struct mbuf *m, int,
	    struct switch_flow_classify *, struct switch_flow_classify *);
struct mbuf
	*swofp_apply_set_field_icmpv4(struct mbuf *, int,
	    struct switch_flow_classify *, struct switch_flow_classify *);
struct mbuf
	*swofp_apply_set_field_ipv6(struct mbuf *, int,
	    struct switch_flow_classify *, struct switch_flow_classify *);
struct mbuf
	*swofp_apply_set_field_ipv4(struct mbuf *, int,
	    struct switch_flow_classify *, struct switch_flow_classify *);
struct mbuf
	*swofp_apply_set_field_arp(struct mbuf *, int,
	    struct switch_flow_classify *, struct switch_flow_classify *);
struct mbuf
	*swofp_apply_set_field_ether(struct mbuf *, int,
	    struct switch_flow_classify *, struct switch_flow_classify *);
struct mbuf
	*swofp_apply_set_field_tunnel(struct mbuf *, int,
	    struct switch_flow_classify *, struct switch_flow_classify *);
struct mbuf
	*swofp_apply_set_field(struct mbuf *, struct swofp_pipeline_desc *);
int	 swofp_ox_set_vlan_vid(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_set_uint8(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_set_uint16(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_set_uint32(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_set_uint64(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_set_ether_addr(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_set_ipv4_addr(struct switch_flow_classify *,
	    struct ofp_ox_match *);
int	 swofp_ox_set_ipv6_addr(struct switch_flow_classify *,
	    struct ofp_ox_match *);

/*
 * OpenFlow protocol execute action handlers
 */
int	 swofp_action_output_controller(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *, uint16_t, uint8_t);
struct mbuf
	*swofp_action_output(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *, struct ofp_action_header *);
struct mbuf
	*swofp_action_group_all(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *, struct swofp_group_entry *);
struct mbuf
	*swofp_action_group(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *, struct ofp_action_header *);
struct mbuf
	*swofp_action_set_field(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *, struct ofp_action_header *);
struct mbuf
	*swofp_execute_action(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *, struct ofp_action_header *);
struct mbuf
	*swofp_execute_action_set_field(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *, struct ofp_action_header *);
struct mbuf
	*swofp_execute_action_set(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *);
struct mbuf
	*swofp_apply_actions(struct switch_softc *, struct mbuf *,
	    struct swofp_pipeline_desc *, struct ofp_instruction_actions *);
struct swofp_action_set
	*swofp_lookup_action_set(struct swofp_pipeline_desc *, uint16_t);
void	 swofp_write_actions_set_field(struct swofp_action_set *,
	    struct ofp_action_header *);
int	 swofp_write_actions(struct ofp_instruction_actions *,
	    struct swofp_pipeline_desc *);
void	 swofp_clear_actions_set_field(struct swofp_action_set *,
	    struct ofp_action_header *);
int	 swofp_clear_actions(struct ofp_instruction_actions *,
	    struct swofp_pipeline_desc *);
void	 swofp_write_metadata(struct ofp_instruction_write_metadata *,
	    struct swofp_pipeline_desc *);

/*
 * OpenFlow protocol message handlers
 */
void	 swofp_send_hello(struct switch_softc *);
int	 swofp_recv_hello(struct switch_softc *, struct mbuf *);
int	 swofp_send_echo(struct switch_softc *, struct mbuf *);
int	 swofp_recv_echo(struct switch_softc *, struct mbuf *);
void	 swofp_send_error(struct switch_softc *, struct mbuf *,
	    uint16_t, uint16_t);
int	 swofp_recv_features_req(struct switch_softc *, struct mbuf *);
int	 swofp_recv_config_req(struct switch_softc *, struct mbuf *);
int	 swofp_recv_set_config(struct switch_softc *, struct mbuf *);
int	 swofp_send_flow_removed(struct switch_softc *,
	    struct swofp_flow_entry *, uint8_t);
int	 swofp_recv_packet_out(struct switch_softc *, struct mbuf *);
int	 swofp_flow_mod_cmd_add(struct switch_softc *, struct mbuf *);
int	 swofp_flow_mod_cmd_common_modify(struct switch_softc *,
	    struct mbuf *, int);
int	 swofp_flow_mod_cmd_modify(struct switch_softc *, struct mbuf *);
int	 swofp_flow_mod_cmd_modify_strict(struct switch_softc *, struct mbuf *);
int	 swofp_flow_mod_cmd_common_delete(struct switch_softc *,
	    struct mbuf *, int);
int	 swofp_flow_mod_cmd_delete(struct switch_softc *, struct mbuf *);
int	 swofp_flow_mod_cmd_delete_strict(struct switch_softc *, struct mbuf *);
int	 swofp_flow_mod(struct switch_softc *, struct mbuf *);
int	 swofp_group_mod_add(struct switch_softc *, struct mbuf *);
int	 swofp_group_mod_modify(struct switch_softc *, struct mbuf *);
int	 swofp_group_mod_delete(struct switch_softc *, struct mbuf *);
int	 swofp_group_mod(struct switch_softc *, struct mbuf *);
int	 swofp_multipart_req(struct switch_softc *, struct mbuf *);
int	 swofp_barrier_req(struct switch_softc *, struct mbuf *);
void	 swofp_barrier_reply(struct switch_softc *, struct mbuf *);

/*
 * OpenFlow protocol multipart message handlers
 */
int	swofp_mpmsg_reply_create(struct ofp_multipart *, struct swofp_mpmsg *);
int	swofp_mpmsg_put(struct swofp_mpmsg *, caddr_t, size_t);
int	swofp_mpmsg_m_put(struct swofp_mpmsg *, struct mbuf *);
void	swofp_mpmsg_destroy(struct swofp_mpmsg *);
int	swofp_multipart_reply(struct switch_softc *, struct swofp_mpmsg *);

int	swofp_put_flow(struct mbuf *, struct swofp_flow_table *,
	    struct swofp_flow_entry *);
int	swofp_put_flows_from_table(struct swofp_mpmsg *,
	    struct swofp_flow_table *, struct ofp_flow_stats_request *);
void	swofp_aggregate_stat_from_table(struct ofp_aggregate_stats *,
	    struct swofp_flow_table *, struct ofp_aggregate_stats_request *);
int	swofp_table_features_put_oxm(struct mbuf *, int *, uint16_t);
int	swofp_table_features_put_actions(struct mbuf *, int *, uint16_t);
int	swofp_table_features_put_instruction(struct mbuf *, int *, uint16_t);

int	swofp_mp_recv_desc(struct switch_softc *, struct mbuf *);
int	swofp_mp_recv_flow(struct switch_softc *, struct mbuf *);
int	swofp_mp_recv_aggregate_flow_stat(struct switch_softc *, struct mbuf *);
int	swofp_mp_recv_table_stats(struct switch_softc *, struct mbuf *);
int	swofp_mp_recv_table_features(struct switch_softc *, struct mbuf *);
int	swofp_mp_recv_port_stats(struct switch_softc *, struct mbuf *);
int	swofp_mp_recv_port_desc(struct switch_softc *, struct mbuf *);
int	swofp_mp_recv_group_desc(struct switch_softc *, struct mbuf *);

#define OFP_ALIGNMENT 8
/*
 * OXM (OpenFlow Extensible Match) structures appear in ofp_match structure
 * and ofp_instruction_{apply|write}_action structure.
 */
#define OFP_OXM_FOREACH(hdr, len, curr)					\
for ((curr) = (struct ofp_ox_match *)((caddr_t)(hdr) + sizeof(*hdr));	\
     (caddr_t)(curr) < ((caddr_t)(hdr) + (len));			\
     (curr) = (struct ofp_ox_match *)((caddr_t)(curr) +			\
	 (curr)->oxm_length + sizeof(*curr)))

#define OFP_OXM_TERMINATED(hdr, len, curr)		\
	(((caddr_t)(hdr) + (len)) <= (caddr_t)(curr))


#define OFP_ACTION_FOREACH(head, len, curr)				\
/* struct ofp_action_header head, curr */				\
for ((curr) = (head); (caddr_t)curr < ((caddr_t)head + (len));		\
    (curr) = (struct ofp_action_header *)((caddr_t)curr +		\
	ntohs((curr)->ah_len)))

/*
 * Get instruction offset in ofp_flow_mod
 */
#define OFP_FLOW_MOD_MSG_INSTRUCTION_OFFSET(ofm)		\
	(offsetof(struct ofp_flow_mod, fm_match) +		\
	    OFP_ALIGN(ntohs((ofm)->fm_match.om_length)))

/*
 * Instructions "FOREACH" in ofp_flow_mod. You can get header using
 * OFP_FLOW_MOD_MSG_INSTRUCTION_PTR macro
 */
#define OFP_FLOW_MOD_INSTRUCTON_FOREACH(hadr, end, curr)			\
for ((curr) = (head); (caddr_t)curr < ((caddr_t)head + (end));			\
     (curr) = (struct ofp_instruction *)((caddr_t)(curr) + (curr)->i_len))


#define OFP_I_ACTIONS_FOREACH(head, curr)					\
/* struct ofp_match head, struct ofp_ox_match curr */				\
for ((curr) = (struct ofp_action_header *)((caddr_t)head + sizeof(*head));	\
    (caddr_t)curr < ((caddr_t)head + ntohs((head)->ia_len));			\
    (curr) = (struct ofp_action_header *)((caddr_t)curr +			\
	 ntohs((curr)->ah_len)))

#define OFP_BUCKETS_FOREACH(head, end, curr)					\
for ((curr) = (head); (caddr_t)curr < ((caddr_t)head + (end));			\
     (curr) = (struct ofp_bucket *)((caddr_t)(curr) + ntohs((curr)->b_len)))


/*
 * OpenFlow protocol message handlers
 */
struct ofp_msg_class {
	uint8_t		msg_type;
	ofp_msg_handler msg_handler;
};
struct ofp_msg_class ofp_msg_table[] = {
	{ OFP_T_HELLO,				swofp_recv_hello },
	{ OFP_T_ERROR,				NULL /* unsupported */ },
	{ OFP_T_ECHO_REQUEST,			swofp_recv_echo },
	{ OFP_T_ECHO_REPLY,			NULL /* from switch */ },
	{ OFP_T_EXPERIMENTER,			NULL /* unsupported */ },
	{ OFP_T_FEATURES_REQUEST,		swofp_recv_features_req },
	{ OFP_T_FEATURES_REPLY,			NULL /* from switch */ },
	{ OFP_T_GET_CONFIG_REQUEST,		swofp_recv_config_req },
	{ OFP_T_GET_CONFIG_REPLY,		NULL /* from switch */ },
	{ OFP_T_SET_CONFIG,			swofp_recv_set_config },
	{ OFP_T_PACKET_IN,			NULL /* from switch */ },
	{ OFP_T_FLOW_REMOVED,			NULL /* from switch */ },
	{ OFP_T_PORT_STATUS,			NULL /* from switch */ },
	{ OFP_T_PACKET_OUT,			swofp_recv_packet_out },
	{ OFP_T_FLOW_MOD,			swofp_flow_mod },
	{ OFP_T_GROUP_MOD,			swofp_group_mod },
	{ OFP_T_PORT_MOD,			NULL /* unsupported */ },
	{ OFP_T_TABLE_MOD,			NULL /* unsupported */ },
	{ OFP_T_MULTIPART_REQUEST,		swofp_multipart_req },
	{ OFP_T_MULTIPART_REPLY,		NULL /* from switch */ },
	{ OFP_T_BARRIER_REQUEST,		swofp_barrier_req },
	{ OFP_T_BARRIER_REPLY,			NULL /* from switch */ },
	{ OFP_T_QUEUE_GET_CONFIG_REQUEST,	NULL /* unsupported */ },
	{ OFP_T_QUEUE_GET_CONFIG_REPLY,		NULL /* from switch */ },
	{ OFP_T_ROLE_REQUEST,			NULL /* unsupported */ },
	{ OFP_T_ROLE_REPLY,			NULL /* from switch */ },
	{ OFP_T_GET_ASYNC_REQUEST,		NULL /* unsupported */ },
	{ OFP_T_GET_ASYNC_REPLY,		NULL /* from switch */ },
	{ OFP_T_SET_ASYNC,			NULL /* unsupported */ },
	{ OFP_T_METER_MOD,			NULL /* unsupported */ }
};


/*
* OpenFlow protocol modification flow message command handlers
*/
struct ofp_flow_mod_class {
	uint8_t		 ofm_cmd_type;
	ofp_msg_handler	 ofm_cmd_handler;
};
struct ofp_flow_mod_class ofp_flow_mod_table[] = {
	{ OFP_FLOWCMD_ADD,		swofp_flow_mod_cmd_add },
	{ OFP_FLOWCMD_MODIFY,		swofp_flow_mod_cmd_modify },
	{ OFP_FLOWCMD_MODIFY_STRICT,	swofp_flow_mod_cmd_modify_strict },
	{ OFP_FLOWCMD_DELETE,		swofp_flow_mod_cmd_delete },
	{ OFP_FLOWCMD_DELETE_STRICT,	swofp_flow_mod_cmd_delete_strict },
};

/*
 * OpenFlow protocol multipart handlers
 */
struct ofp_mpmsg_class {
	uint8_t		 msgsg_type;
	ofp_msg_handler	 mpmsg_handler;
};
struct ofp_mpmsg_class ofp_mpmsg_table[] = {
	{ OFP_MP_T_DESC,		swofp_mp_recv_desc },
	{ OFP_MP_T_FLOW,		swofp_mp_recv_flow },
	{ OFP_MP_T_AGGREGATE,		swofp_mp_recv_aggregate_flow_stat },
	{ OFP_MP_T_TABLE,		swofp_mp_recv_table_stats },
	{ OFP_MP_T_PORT_STATS,		swofp_mp_recv_port_stats },
	{ OFP_MP_T_QUEUE,		NULL },
	{ OFP_MP_T_GROUP,		NULL },
	{ OFP_MP_T_GROUP_DESC,		swofp_mp_recv_group_desc },
	{ OFP_MP_T_GROUP_FEATURES,	NULL },
	{ OFP_MP_T_METER,		NULL },
	{ OFP_MP_T_METER_CONFIG,	NULL },
	{ OFP_MP_T_METER_FEATURES,	NULL },
	{ OFP_MP_T_TABLE_FEATURES,	swofp_mp_recv_table_features },
	{ OFP_MP_T_PORT_DESC,		swofp_mp_recv_port_desc }
};

/*
 * OpenFlow OXM match handlers
 */
#define SWOFP_MATCH_MASK	0x1
#define SWOFP_MATCH_WILDCARD	0x2

struct ofp_oxm_class {
	uint8_t	 oxm_field;
	uint8_t	 oxm_len; /* This length defined by speficication */
	uint8_t	 oxm_flags;
	int	(*oxm_match)(struct switch_flow_classify *,
		    struct ofp_ox_match *);
	int	(*oxm_set)(struct switch_flow_classify *,
		    struct ofp_ox_match *);
	int	(*oxm_cmp)(struct ofp_ox_match *,
		    struct ofp_ox_match *, int);
};

/*
 * Handlers for basic class for OpenFlow
 */
struct ofp_oxm_class ofp_oxm_handlers[] = {
	{
		OFP_XM_T_IN_PORT,
		sizeof(uint32_t),
		0,
		swofp_ox_match_uint32,		NULL,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_IN_PHY_PORT,
		sizeof(uint32_t),
		0,
		NULL,				NULL,
		NULL
	},
	{
		OFP_XM_T_META,
		sizeof(uint64_t),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint64,	NULL,
		NULL
	},
	{
		OFP_XM_T_ETH_DST,
		ETHER_ADDR_LEN,
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ether_addr,	swofp_ox_set_ether_addr,
		swofp_ox_cmp_ether_addr
	},
	{
		OFP_XM_T_ETH_SRC,
		ETHER_ADDR_LEN,
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ether_addr,	swofp_ox_set_ether_addr,
		swofp_ox_cmp_ether_addr
	},
	{
		OFP_XM_T_ETH_TYPE,
		sizeof(uint16_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint16,		swofp_ox_set_uint16,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_VLAN_VID,
		sizeof(uint16_t),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_vlan_vid,	swofp_ox_set_vlan_vid,
		swofp_ox_cmp_vlan_vid
	},
	{
		OFP_XM_T_VLAN_PCP,
		sizeof(uint8_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint8,		swofp_ox_set_uint16,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_IP_DSCP,
		sizeof(uint8_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint8,		swofp_ox_set_uint8,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_IP_ECN,
		sizeof(uint8_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint8,		swofp_ox_set_uint8,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_IP_PROTO,
		sizeof(uint8_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint8,		swofp_ox_set_uint8,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_IPV4_SRC,
		sizeof(uint32_t),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv4_addr,	swofp_ox_set_ipv4_addr,
		swofp_ox_cmp_ipv4_addr
	},
	{
		OFP_XM_T_IPV4_DST,
		sizeof(uint32_t),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv4_addr,	swofp_ox_set_ipv4_addr,
		swofp_ox_cmp_ipv4_addr
	},
	{
		OFP_XM_T_TCP_SRC,
		sizeof(uint16_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint16,		swofp_ox_set_uint16,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_TCP_DST,
		sizeof(uint16_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint16,		swofp_ox_set_uint16,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_UDP_SRC,
		sizeof(uint16_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint16,		swofp_ox_set_uint16,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_UDP_DST,
		sizeof(uint16_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint16,		swofp_ox_set_uint16,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_SCTP_SRC,
		sizeof(uint16_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint16,		swofp_ox_set_uint16,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_SCTP_DST,
		sizeof(uint16_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint16,		swofp_ox_set_uint16,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_ICMPV4_TYPE,
		sizeof(uint8_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint8,		swofp_ox_set_uint8,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_ICMPV4_CODE,
		sizeof(uint8_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint8,		swofp_ox_set_uint8,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_ARP_OP,
		sizeof(uint16_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint16,		swofp_ox_set_uint16,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_ARP_SPA,
		sizeof(uint32_t),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv4_addr,	swofp_ox_set_ipv4_addr,
		swofp_ox_cmp_ipv4_addr
	},
	{
		OFP_XM_T_ARP_TPA,
		sizeof(uint32_t),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv4_addr,	swofp_ox_set_ipv4_addr,
		swofp_ox_cmp_ipv4_addr
	},
	{
		OFP_XM_T_ARP_SHA,
		ETHER_ADDR_LEN,
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ether_addr,	swofp_ox_set_ether_addr,
		swofp_ox_cmp_ether_addr
	},
	{
		OFP_XM_T_ARP_THA,
		ETHER_ADDR_LEN,
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ether_addr,	swofp_ox_set_ether_addr,
		swofp_ox_cmp_ether_addr
	},
#ifdef INET6
	{
		OFP_XM_T_IPV6_SRC,
		sizeof(struct in6_addr),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv6_addr,	swofp_ox_set_ipv6_addr,
		swofp_ox_cmp_ipv6_addr
	},
	{
		OFP_XM_T_IPV6_DST,
		sizeof(struct in6_addr),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv6_addr,	swofp_ox_set_ipv6_addr,
		swofp_ox_cmp_ipv6_addr
	},
	{
		OFP_XM_T_IPV6_FLABEL,
		sizeof(uint32_t),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint32,		swofp_ox_set_uint32,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_ICMPV6_TYPE,
		sizeof(uint8_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint8,		swofp_ox_set_uint8,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_ICMPV6_CODE,
		sizeof(uint8_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint8,		swofp_ox_set_uint8,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_IPV6_ND_TARGET,
		sizeof(struct in6_addr),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv6_addr,	swofp_ox_set_ipv6_addr,
		swofp_ox_cmp_ipv6_addr
	},
	{
		OFP_XM_T_IPV6_ND_SLL,
		ETHER_ADDR_LEN,
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ether_addr,	swofp_ox_set_ether_addr,
		swofp_ox_cmp_ether_addr
	},
	{
		OFP_XM_T_IPV6_ND_TLL,
		ETHER_ADDR_LEN,
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ether_addr,	swofp_ox_set_ether_addr,
		swofp_ox_cmp_ether_addr
	},
#endif /* INET6 */
	{
		OFP_XM_T_MPLS_LABEL,
		sizeof(uint32_t),
		SWOFP_MATCH_WILDCARD,
		NULL,				NULL,
		NULL
	},
	{
		OFP_XM_T_MPLS_TC,
		sizeof(uint8_t),
		SWOFP_MATCH_WILDCARD,
		NULL,				NULL,
		NULL
	},
	{
		OFP_XM_T_MPLS_BOS,
		sizeof(uint8_t),
		SWOFP_MATCH_WILDCARD,
		NULL,				NULL,
		NULL
	},
	{
		OFP_XM_T_PBB_ISID,
		3,
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		NULL,				NULL,
		NULL
	},
	{
		OFP_XM_T_TUNNEL_ID,
		sizeof(uint64_t),
		SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint64,		swofp_ox_set_uint64,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_T_IPV6_EXTHDR,
		sizeof(uint16_t),
		SWOFP_MATCH_WILDCARD,
		NULL,				NULL,
		NULL
	},
};

/*
 * Handlers for backward compatibility with NXM
 */
struct ofp_oxm_class ofp_oxm_nxm_handlers[] = {
	{
		OFP_XM_NXMT_TUNNEL_ID,
		sizeof(uint64_t),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_uint64,		swofp_ox_set_uint64,
		swofp_ox_cmp_data
	},
	{
		OFP_XM_NXMT_TUNNEL_IPV4_SRC,
		sizeof(uint32_t),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv4_addr,	swofp_ox_set_ipv4_addr,
		swofp_ox_cmp_ipv4_addr
	},
	{
		OFP_XM_NXMT_TUNNEL_IPV4_DST,
		sizeof(uint32_t),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv4_addr,	swofp_ox_set_ipv4_addr,
		swofp_ox_cmp_ipv4_addr
	},
#ifdef INET6
	{
		OFP_XM_NXMT_TUNNEL_IPV6_SRC,
		sizeof(struct in6_addr),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv6_addr,	swofp_ox_set_ipv6_addr,
		swofp_ox_cmp_ipv6_addr
	},
	{
		OFP_XM_NXMT_TUNNEL_IPV6_DST,
		sizeof(struct in6_addr),
		SWOFP_MATCH_MASK|SWOFP_MATCH_WILDCARD,
		swofp_ox_match_ipv6_addr,	swofp_ox_set_ipv6_addr,
		swofp_ox_cmp_ipv6_addr
	},
#endif /* INET6 */
};

/*
 * OpenFlow action handlers
 */
struct ofp_action_handler {
	uint16_t	 action_type;
	struct mbuf *	(*action)(struct switch_softc *,  struct mbuf *,
	    struct swofp_pipeline_desc *, struct ofp_action_header *);
};
struct ofp_action_handler ofp_action_handlers[] = {
	/*
	 * Following order complies with action set order in
	 * OpenFlow Switch Specification (ver. 1.3.5)
	 */
	{
		OFP_ACTION_COPY_TTL_IN,
		NULL
	},
	{
		OFP_ACTION_POP_MPLS,
		NULL
	},
	{
		OFP_ACTION_POP_PBB,
		NULL
	},
	{
		OFP_ACTION_POP_VLAN,
		swofp_action_pop_vlan
	},
	{
		OFP_ACTION_PUSH_MPLS,
		NULL
	},
	{
		OFP_ACTION_PUSH_PBB,
		NULL
	},
	{
		OFP_ACTION_PUSH_VLAN,
		swofp_action_push_vlan
	},
	{
		OFP_ACTION_COPY_TTL_OUT,
		NULL
	},
	{
		OFP_ACTION_DEC_NW_TTL,
		NULL
	},
	{
		OFP_ACTION_DEC_MPLS_TTL,
		NULL
	},
	{
		OFP_ACTION_SET_MPLS_TTL,
		NULL
	},
	{
		OFP_ACTION_SET_NW_TTL,
		NULL
	},
	{
		OFP_ACTION_SET_FIELD,
		swofp_action_set_field
	},
	{
		OFP_ACTION_SET_QUEUE,
		NULL
	},
	{
		OFP_ACTION_GROUP,
		swofp_action_group
	},
	{
		OFP_ACTION_OUTPUT,
		swofp_action_output
	},
	{
		OFP_ACTION_EXPERIMENTER,
		NULL
	}, /* XXX Where is best position? */
};

extern struct pool swfcl_pool;
struct pool swpld_pool;

void
swofp_attach(void)
{
	pool_init(&swpld_pool, sizeof(struct swofp_pipeline_desc), 0, 0, 0,
	    "swpld", NULL);
}


int
swofp_create(struct switch_softc *sc)
{
	struct swofp_ofs	*swofs;
	int			 error;

	swofs = malloc(sizeof(*swofs), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (swofs == NULL)
		return (ENOMEM);

	sc->sc_ofs = swofs;

	TAILQ_INIT(&swofs->swofs_table_list);

	/*
	 * A table with id 0 must exist
	 */
	if ((swofp_flow_table_add(sc, 0)) == NULL) {
		error = ENOBUFS;
		free(swofs, M_DEVBUF, sizeof(*swofs));
		return (error);
	}

	swofs->swofs_xidnxt = 1;
	arc4random_buf(&swofs->swofs_datapath_id,
	    sizeof(swofs->swofs_datapath_id));

	timeout_set(&swofs->swofs_flow_timeout, swofp_timer, sc);
	timeout_add_sec(&swofs->swofs_flow_timeout, 10);

	/* TODO: Configured from ifconfig  */
	swofs->swofs_group_max_table = 1000;
	swofs->swofs_flow_max_entry = 10000;

	sc->sc_capabilities |= SWITCH_CAP_OFP;
	sc->switch_process_forward = swofp_forward_ofs;

#if NBPFILTER > 0
	bpfattach(&sc->sc_ofbpf, &sc->sc_if, DLT_OPENFLOW,
	    sizeof(struct ofp_header));
#endif

	return (0);
}

void
swofp_destroy(struct switch_softc *sc)
{
	struct swofp_ofs	*swofs = sc->sc_ofs;

	if ((sc->sc_capabilities & SWITCH_CAP_OFP) == 0 || swofs == NULL)
		return;

	timeout_del(&swofs->swofs_flow_timeout);

	sc->sc_capabilities &= ~SWITCH_CAP_OFP;
	sc->switch_process_forward = NULL;

	swofp_group_entry_delete_all(sc);

	free(swofs, M_DEVBUF, sizeof(*swofs));
}

int
swofp_init(struct switch_softc *sc)
{
	sc->sc_swdev->swdev_input = swofp_input;
	swofp_send_hello(sc);
	return (0);
}

uint32_t
swofp_assign_portno(struct switch_softc *sc, uint32_t req)
{
	struct switch_port	*swpo;
	uint32_t		 candidate;

	TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
		if (swpo->swpo_port_no == req)
			break;
	}
	if (swpo == NULL)
		return (req);

	/* XXX
	 * OS's ifindex is "short", so it expect that floowing is unique
	 */
	candidate = (req << 16) | req;
	while (1) {
		TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
			if (swpo->swpo_port_no == candidate)
				break;
		}
		if (swpo == NULL)
			return (candidate);

		if (candidate < OFP_PORT_MAX)
			candidate++;
		else
			candidate = 0;
	}
}

int
swofp_ioctl(struct ifnet *ifp, unsigned long cmd, caddr_t data)
{
	struct switch_softc	*sc = ifp->if_softc;
	struct swofp_ofs	*swofs = sc->sc_ofs;
	struct ifbrparam	*bparam = (struct ifbrparam *)data;
	struct ifbreq		*breq = (struct ifbreq *)data;
	struct switch_port	*swpo;
	struct ifnet		*ifs;
	int			 error = 0;

	switch (cmd) {
	case SIOCSWGDPID:
		memcpy(&bparam->ifbrp_datapath, &swofs->swofs_datapath_id,
		    sizeof(uint64_t));
		break;
	case SIOCSWSDPID:
		if ((error = suser(curproc)) != 0)
			break;
		memcpy(&swofs->swofs_datapath_id, &bparam->ifbrp_datapath,
		    sizeof(uint64_t));
		break;
	case SIOCSWGMAXFLOW:
		bparam->ifbrp_maxflow = swofs->swofs_flow_max_entry;
		break;
	case SIOCSWGMAXGROUP:
		bparam->ifbrp_maxgroup = swofs->swofs_group_max_table;
		break;
	case SIOCSWSPORTNO:
		if ((error = suser(curproc)) != 0)
			break;

		if (breq->ifbr_portno >= OFP_PORT_MAX)
			return (EINVAL);

		if ((ifs = ifunit(breq->ifbr_ifsname)) == NULL)
			return (ENOENT);

		if (ifs->if_switchport == NULL)
			return (ENOENT);

		TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
			if (swpo->swpo_port_no == breq->ifbr_portno)
				return (EEXIST);
		}

		swpo = (struct switch_port *)ifs->if_switchport;
		swpo->swpo_port_no = breq->ifbr_portno;

		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

/* TODO: Optimization */
struct ofp_oxm_class *
swofp_lookup_oxm_handler(struct ofp_ox_match *oxm)
{
	struct ofp_oxm_class	*handlers;
	uint8_t			 oxm_field;
	int			 i, len;

	switch (ntohs(oxm->oxm_class)) {
	case OFP_OXM_C_OPENFLOW_BASIC:
		handlers = ofp_oxm_handlers;
		len = nitems(ofp_oxm_handlers);
		break;
	case OFP_OXM_C_NXM_1:
		handlers = ofp_oxm_nxm_handlers;
		len = nitems(ofp_oxm_nxm_handlers);
		break;
	default:
		return (NULL);
	}

	oxm_field = OFP_OXM_GET_FIELD(oxm);

	for (i = 0; i < len ; i++) {
		if (handlers[i].oxm_field == oxm_field)
			return (&handlers[i]);
	}

	return (NULL);
}

ofp_msg_handler
swofp_lookup_msg_handler(uint8_t type)
{
	if (type >= OFP_T_TYPE_MAX)
		return (NULL);
	else
		return (ofp_msg_table[type].msg_handler);
}

ofp_msg_handler
swofp_lookup_mpmsg_handler(uint16_t type)
{
	if (type >= nitems(ofp_mpmsg_table))
		return (NULL);
	else
		return (ofp_mpmsg_table[type].mpmsg_handler);
}

struct ofp_action_handler *
swofp_lookup_action_handler(uint16_t type)
{
	int	i;

	for (i = 0; i < nitems(ofp_action_handlers); i++) {
		if (ofp_action_handlers[i].action_type == type)
			return &(ofp_action_handlers[i]);
	}

	return (NULL);
}

struct swofp_pipeline_desc *
swofp_pipeline_desc_create(struct switch_flow_classify *swfcl)
{
	struct swofp_pipeline_desc	*swpld = NULL;
	struct swofp_action_set		*swas = NULL;
	int				 i;

	swpld = pool_get(&swpld_pool, PR_NOWAIT|PR_ZERO);
	if (swpld == NULL)
		return (NULL);

	/*
	 * ofp_action_handlers is sorted by applying action-set order,
	 * so it can be used for initializer for action-set.
	 */
	swas = swpld->swpld_action_set;
	for (i = 0; i < nitems(ofp_action_handlers); i++) {
		swas[i].swas_type = ofp_action_handlers[i].action_type;
		if (swas[i].swas_type == OFP_ACTION_SET_FIELD)
			swas[i].swas_action = (struct ofp_action_header *)
			    swpld->swpld_set_fields;
		else
			swas[i].swas_action = NULL;
	}

	swpld->swpld_swfcl = swfcl;

	return (swpld);
}

void
swofp_pipeline_desc_destroy(struct swofp_pipeline_desc *swpld)
{
	switch_swfcl_free(&swpld->swpld_pre_swfcl);
	pool_put(&swpld_pool, swpld);
}

struct swofp_flow_table *
swofp_flow_table_lookup(struct switch_softc *sc, uint16_t table_id)
{
	struct swofp_ofs	*ofs = sc->sc_ofs;
	struct swofp_flow_table	*swft;

	TAILQ_FOREACH(swft, &ofs->swofs_table_list, swft_table_next) {
		if (swft->swft_table_id == table_id)
			return (swft);
	}

	return (NULL);
}

struct swofp_flow_table *
swofp_flow_table_add(struct switch_softc *sc, uint16_t table_id)
{
	struct swofp_ofs	*ofs = sc->sc_ofs;
	struct swofp_flow_table *swft, *new;

	if ((swft = swofp_flow_table_lookup(sc, table_id)) != NULL)
		return (swft);

	if ((new = malloc(sizeof(*new), M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (NULL);

	new->swft_table_id = table_id;
	TAILQ_FOREACH(swft, &ofs->swofs_table_list, swft_table_next) {
		if (table_id < swft->swft_table_id)
			break;
	}

	if (swft)
		TAILQ_INSERT_BEFORE(swft, new, swft_table_next);
	else
		TAILQ_INSERT_TAIL(&ofs->swofs_table_list, new, swft_table_next);

	DPRINTF(sc, "add openflow flow table (id:%d)\n", table_id);

	return (new);
}

int
swofp_flow_table_delete(struct switch_softc *sc, uint16_t table_id)
{
	struct swofp_ofs	*ofs = sc->sc_ofs;
	struct swofp_flow_table *swft;
	struct swofp_flow_entry *swfe, *tswfe;

	if ((swft = swofp_flow_table_lookup(sc, table_id)) == NULL)
		return ENOENT;

	LIST_FOREACH_SAFE(swfe, &swft->swft_flow_list, swfe_next, tswfe) {
		/*
		 * Flows are deleted force becouse of deleting table,
		 * s it's not necessary to send flow remove message.
		 */
		swfe->swfe_flags &= ~(OFP_FLOWFLAG_SEND_FLOW_REMOVED);
		swofp_flow_entry_delete(sc, swft, swfe,
		    OFP_FLOWREM_REASON_DELETE);
	}

	TAILQ_REMOVE(&ofs->swofs_table_list, swft, swft_table_next);
	free(swft, M_DEVBUF, sizeof(*swft));

	DPRINTF(sc, "delete flow table (id:%d)\n", table_id);

	return 0;
}

void
swofp_flow_table_delete_all(struct switch_softc *sc)
{
	struct swofp_ofs	*ofs = sc->sc_ofs;
	struct swofp_flow_table *swft, *tswft;
	int			 error;

	TAILQ_FOREACH_SAFE(swft, &ofs->swofs_table_list,
	    swft_table_next, tswft) {
		if ((error = swofp_flow_table_delete(sc,swft->swft_table_id)))
			log(LOG_ERR, "can't delete table id:%d (error:%d)\n",
			    swft->swft_table_id, error);
	}
}

struct swofp_group_entry *
swofp_group_entry_lookup(struct switch_softc *sc, uint32_t group_id)
{
	struct swofp_ofs		*ofs = sc->sc_ofs;
	struct swofp_group_entry	*swge;

	LIST_FOREACH(swge, &ofs->swofs_group_table, swge_next) {
		if (swge->swge_group_id == group_id)
			return swge;
	}

	return (NULL);
}

int
swofp_group_entry_add(struct switch_softc *sc, struct swofp_group_entry *swge)
{
	struct swofp_ofs *ofs = sc->sc_ofs;

	LIST_INSERT_HEAD(&ofs->swofs_group_table, swge, swge_next);
	ofs->swofs_group_table_num++;

	DPRINTF(sc, "add group %d in group table (total %d)\n",
	    swge->swge_group_id, ofs->swofs_group_table_num);

	return (0);
}

int
swofp_group_entry_delete(struct switch_softc *sc,
    struct swofp_group_entry *swge)
{
	struct swofp_ofs	*ofs = sc->sc_ofs;
	struct swofp_flow_table *swft;

	DPRINTF(sc, "delete group %d in group table (total %d)\n",
	    swge->swge_group_id, ofs->swofs_group_table_num);

	LIST_REMOVE(swge, swge_next);
	ofs->swofs_group_table_num--;

	TAILQ_FOREACH(swft, &ofs->swofs_table_list, swft_table_next) {
		swofp_flow_delete_on_table_by_group(sc, swft,
		    swge->swge_group_id);
	}

	free(swge->swge_buckets, M_DEVBUF, swge->swge_buckets_len);
	free(swge, M_DEVBUF, sizeof(*swge));

	return (0);
}

int
swofp_group_entry_delete_all(struct switch_softc *sc)
{
	struct swofp_ofs		*ofs = sc->sc_ofs;
	struct swofp_group_entry	*swge, *tswge;

	LIST_FOREACH_SAFE(swge, &ofs->swofs_group_table, swge_next, tswge) {
		swofp_group_entry_delete(sc, swge);
	}

	return (0);
}

int
swofp_validate_buckets(struct switch_softc *sc, struct mbuf *m, uint8_t type,
    uint16_t *etype, uint16_t *error)
{
	struct ofp_group_mod	*ogm;
	struct ofp_bucket	*bucket;
	struct ofp_action_header *ah;
	uint16_t		weight, remaining;
	int			start, len, off, num;
	size_t			blen;

	*etype = OFP_ERRTYPE_GROUP_MOD_FAILED;

	ogm = mtod(m, struct ofp_group_mod *);
	start = offsetof(struct ofp_group_mod, gm_buckets);
	remaining = len = ntohs(ogm->gm_oh.oh_length) - start;

	/* Invalid packet size. */
	if (len < 0) {
		*error = OFP_ERRGROUPMOD_INVALID_GROUP;
		return (-1);
	}

	/* Indirect group type must always have one bucket. */
	if (len < sizeof(*bucket) && type == OFP_GROUP_T_INDIRECT) {
		*error = OFP_ERRGROUPMOD_INVALID_GROUP;
		return (-1);
	}

	for (off = start, num = 0; off < start + len; off += blen, num++) {
		bucket = (struct ofp_bucket *)(mtod(m, caddr_t) + off);

		blen = ntohs(bucket->b_len);
		if (blen < sizeof(*bucket)) {
			*error = OFP_ERRGROUPMOD_BAD_BUCKET;
			return (-1);
		}

		/* Validate that the bucket is smaller than the payload. */
		if (blen > remaining) {
			*etype = OFP_ERRTYPE_BAD_REQUEST;
			*error = OFP_ERRREQ_BAD_LEN;
			return (-1);
		}
		remaining -= blen;

		/*
		 * Validate weight
		 */
		switch (type) {
		case OFP_GROUP_T_ALL:
		case OFP_GROUP_T_INDIRECT:
		case OFP_GROUP_T_FAST_FAILOVER:
			if (ntohs(bucket->b_weight) != 0) {
				*error = OFP_ERRGROUPMOD_BAD_BUCKET;
				return (-1);
			}
			break;
		case OFP_GROUP_T_SELECT:
			if (num > 1 && weight != ntohs(bucket->b_weight)) {
				*error = OFP_ERRGROUPMOD_WEIGHT_UNSUPP;
				return (-1);
			}
			break;
		}

		/*
		 * INDIRECT type has only one bucket
		 */
		if (type == OFP_GROUP_T_INDIRECT && num > 1) {
			*error = OFP_ERRGROUPMOD_BAD_BUCKET;
			return (-1);
		}

		weight = ntohs(bucket->b_weight);

		/* Skip if there are no actions to validate. */
		if (blen == sizeof(*bucket))
			continue;

		ah = (struct ofp_action_header *)
		    (mtod(m, caddr_t) + off + sizeof(*bucket));
		if (swofp_validate_action(sc, ah, blen - sizeof(*bucket),
		    error)) {
			*etype = OFP_ERRTYPE_BAD_ACTION;
			return (-1);
		}
	}

	return (0);
}

void
swofp_flow_entry_table_free(struct ofp_instruction **table)
{
	if (*table) {
		free(*table, M_DEVBUF, ntohs((*table)->i_len));
		*table = NULL;
	}
}

void
swofp_flow_entry_instruction_free(struct swofp_flow_entry *swfe)
{
	swofp_flow_entry_table_free((struct ofp_instruction **)
	    &swfe->swfe_goto_table);
	swofp_flow_entry_table_free((struct ofp_instruction **)
	    &swfe->swfe_write_metadata);
	swofp_flow_entry_table_free((struct ofp_instruction **)
	    &swfe->swfe_apply_actions);
	swofp_flow_entry_table_free((struct ofp_instruction **)
	    &swfe->swfe_write_actions);
	swofp_flow_entry_table_free((struct ofp_instruction **)
	    &swfe->swfe_clear_actions);
	swofp_flow_entry_table_free((struct ofp_instruction **)
	    &swfe->swfe_experimenter);
	swofp_flow_entry_table_free((struct ofp_instruction **)
	    &swfe->swfe_meter);
}

void
swofp_flow_entry_free(struct swofp_flow_entry **swfe)
{
	if ((*swfe)->swfe_match)
		free((*swfe)->swfe_match, M_DEVBUF,
		    ntohs((*swfe)->swfe_match->om_length));

	swofp_flow_entry_instruction_free(*swfe);

	free((*swfe), M_DEVBUF, sizeof(**swfe));
}

void
swofp_flow_entry_add(struct switch_softc *sc, struct swofp_flow_table *swft,
    struct swofp_flow_entry *swfe)
{
	swfe->swfe_table_id = swft->swft_table_id;
	LIST_INSERT_HEAD(&swft->swft_flow_list, swfe, swfe_next);
	swft->swft_flow_num++;

	DPRINTF(sc, "add flow in table %d (total %d)\n",
	    swft->swft_table_id, swft->swft_flow_num);
}

void
swofp_flow_entry_delete(struct switch_softc *sc, struct swofp_flow_table *swft,
    struct swofp_flow_entry *swfe, uint8_t reason)
{
	if (swfe->swfe_flags & OFP_FLOWFLAG_SEND_FLOW_REMOVED)
		swofp_send_flow_removed(sc, swfe, reason);

	LIST_REMOVE(swfe, swfe_next);
	swofp_flow_entry_free(&swfe);
	swft->swft_flow_num--;

	DPRINTF(sc, "delete flow from table %d (total %d)\n",
	    swft->swft_table_id, swft->swft_flow_num);
}

void
swofp_flow_timeout(struct switch_softc *sc)
{
	struct swofp_ofs	*ofs = sc->sc_ofs;
	struct swofp_flow_table	*swft;
	struct swofp_flow_entry	*swfe, *tswfe;
	struct timespec		 now, duration, idle;

	nanouptime(&now);

	TAILQ_FOREACH(swft, &ofs->swofs_table_list, swft_table_next) {
		LIST_FOREACH_SAFE(swfe, &swft->swft_flow_list,
		    swfe_next, tswfe) {
			if (swfe->swfe_idle_timeout) {
				timespecsub(&now, &swfe->swfe_idle_time, &idle);
				if (swfe->swfe_idle_timeout < idle.tv_sec) {
					DPRINTF(sc, "flow expired "
					    "by idle timeout\n");
					swofp_flow_entry_delete(sc, swft, swfe,
					    OFP_FLOWREM_REASON_IDLE_TIMEOUT);
					continue;
				}
			}
			if (swfe->swfe_hard_timeout) {
				timespecsub(&now, &swfe->swfe_installed_time,
				    &duration);
				if (swfe->swfe_hard_timeout < duration.tv_sec) {
					DPRINTF(sc, "flow expired "
					    "by hard timeout\n");
					swofp_flow_entry_delete(sc, swft, swfe,
					    OFP_FLOWREM_REASON_HARD_TIMEOUT);
				}
			}
		}
	}
}

void
swofp_timer(void *v)
{
	struct switch_softc	*sc = (struct switch_softc *)v;
	struct swofp_ofs	*swofs = sc->sc_ofs;

	swofp_flow_timeout(sc);
	timeout_add_sec(&swofs->swofs_flow_timeout, 10);
}

int
swofp_ox_cmp_data(struct ofp_ox_match *target,
    struct ofp_ox_match *key, int strict)
{
	uint64_t	 tmth, tmask, kmth, kmask;
	uint64_t	 dummy_mask = UINT64_MAX;
	int		 len;

	if (OFP_OXM_GET_FIELD(target) != OFP_OXM_GET_FIELD(key))
		return (1);

	switch (OFP_OXM_GET_FIELD(target)) {
	case OFP_XM_T_VLAN_PCP:
	case OFP_XM_T_IP_DSCP:
	case OFP_XM_T_IP_ECN:
	case OFP_XM_T_IP_PROTO:
	case OFP_XM_T_ICMPV4_CODE:
	case OFP_XM_T_ICMPV4_TYPE:
	case OFP_XM_T_ICMPV6_CODE:
	case OFP_XM_T_ICMPV6_TYPE:
		len = sizeof(uint8_t);
		break;
	case OFP_XM_T_ETH_TYPE:
	case OFP_XM_T_TCP_SRC:
	case OFP_XM_T_TCP_DST:
	case OFP_XM_T_UDP_SRC:
	case OFP_XM_T_UDP_DST:
	case OFP_XM_T_ARP_OP:
		len = sizeof(uint16_t);
		break;
	case OFP_XM_T_IN_PORT:
	case OFP_XM_T_IPV6_FLABEL:
		len = sizeof(uint32_t);
		break;
	case OFP_XM_T_TUNNEL_ID: /* alias OFP_XM_NXMT_TUNNEL_ID */
		len = sizeof(uint64_t);
		break;
	default:
		return (1);
	}

	tmth = tmask = kmth = kmask = 0;

	memcpy(&tmth, ((caddr_t)target + sizeof(*target)), len);
	if (OFP_OXM_GET_HASMASK(target))
		memcpy(&tmask, ((caddr_t)target + sizeof(*target) + len), len);
	else
		memcpy(&tmask, &dummy_mask, len);

	memcpy(&kmth, ((caddr_t)key + sizeof(*key)), len);
	if (OFP_OXM_GET_HASMASK(key))
		memcpy(&kmask, ((caddr_t)key + sizeof(*key) + len), len);
	else
		memcpy(&kmask, &dummy_mask, len);

	if (strict) {
		if (tmask != kmask)
			return (1);
	} else {
		if ((tmask & kmask) != kmask)
			return (1);
	}

	return !((tmth & tmask) == (kmth & kmask));
}

#ifdef INET6
int
swofp_ox_cmp_ipv6_addr(struct ofp_ox_match *target,
    struct ofp_ox_match *key, int strict)
{
	struct in6_addr	tmth, tmask, kmth, kmask;
	struct in6_addr	mask = in6mask128;

	if (OFP_OXM_GET_FIELD(target) != OFP_OXM_GET_FIELD(key))
		return (1);

	switch (OFP_OXM_GET_FIELD(target)) {
	case OFP_XM_NXMT_TUNNEL_IPV6_SRC:
	case OFP_XM_NXMT_TUNNEL_IPV6_DST:
	case OFP_XM_T_IPV6_SRC:
	case OFP_XM_T_IPV6_DST:
	case OFP_XM_T_IPV6_ND_TARGET:
		break;
	default:
		return (1);
	}

	memcpy(&kmth, ((caddr_t)key + sizeof(*key)), sizeof(kmth));
	if (OFP_OXM_GET_HASMASK(key))
		memcpy(&kmask, ((caddr_t)key + sizeof(*key) + sizeof(kmask)),
		    sizeof(kmask));
	else
		kmask = mask;

	memcpy(&tmth, ((caddr_t)target + sizeof(*target)), sizeof(tmth));
	if (OFP_OXM_GET_HASMASK(target))
		memcpy(&tmask, ((caddr_t)target + sizeof(*target) +
		    sizeof(tmask)), sizeof(tmask));
	else
		tmask = mask;

	if (strict) {
		if (memcmp(&tmask, &kmask, sizeof(tmask)) != 0)
			return (1);

		tmth.s6_addr32[0] &= tmask.s6_addr32[0];
		tmth.s6_addr32[1] &= tmask.s6_addr32[1];
		tmth.s6_addr32[2] &= tmask.s6_addr32[2];
		tmth.s6_addr32[3] &= tmask.s6_addr32[3];

		kmth.s6_addr32[0] &= kmask.s6_addr32[0];
		kmth.s6_addr32[1] &= kmask.s6_addr32[1];
		kmth.s6_addr32[2] &= kmask.s6_addr32[2];
		kmth.s6_addr32[3] &= kmask.s6_addr32[3];

	} else {
		tmask.s6_addr32[0] &= kmask.s6_addr32[0];
		tmask.s6_addr32[1] &= kmask.s6_addr32[1];
		tmask.s6_addr32[2] &= kmask.s6_addr32[2];
		tmask.s6_addr32[3] &= kmask.s6_addr32[3];

		if (memcmp(&tmask, &kmask, sizeof(tmask)) != 0)
			return (1);

		tmth.s6_addr32[0] &= kmask.s6_addr32[0];
		tmth.s6_addr32[1] &= kmask.s6_addr32[1];
		tmth.s6_addr32[2] &= kmask.s6_addr32[2];
		tmth.s6_addr32[3] &= kmask.s6_addr32[3];

		kmth.s6_addr32[0] &= kmask.s6_addr32[0];
		kmth.s6_addr32[1] &= kmask.s6_addr32[1];
		kmth.s6_addr32[2] &= kmask.s6_addr32[2];
		kmth.s6_addr32[3] &= kmask.s6_addr32[3];

	}

	return memcmp(&tmth, &kmth, sizeof(tmth));
}
#endif /* INET6 */

int
swofp_ox_cmp_ipv4_addr(struct ofp_ox_match *target,
    struct ofp_ox_match *key, int strict)
{
	uint32_t	tmth, tmask, kmth, kmask;

	if (OFP_OXM_GET_FIELD(target) != OFP_OXM_GET_FIELD(key))
		return (1);

	switch (OFP_OXM_GET_FIELD(target)) {
	case OFP_XM_NXMT_TUNNEL_IPV4_SRC:
	case OFP_XM_NXMT_TUNNEL_IPV4_DST:
	case OFP_XM_T_IPV4_SRC:
	case OFP_XM_T_IPV4_DST:
	case OFP_XM_T_ARP_SPA:
	case OFP_XM_T_ARP_TPA:
		break;
	default:
		return (1);
	}

	memcpy(&tmth, ((caddr_t)target + sizeof(*target)), sizeof(uint32_t));
	if (OFP_OXM_GET_HASMASK(target))
		memcpy(&tmask, ((caddr_t)target + sizeof(*target) +
		    sizeof(uint32_t)), sizeof(uint32_t));
	else
		tmask = UINT32_MAX;

	memcpy(&kmth, ((caddr_t)key + sizeof(*key)), sizeof(uint32_t));
	if (OFP_OXM_GET_HASMASK(key))
		memcpy(&kmask, ((caddr_t)key + sizeof(*key) +
		    sizeof(uint32_t)), sizeof(uint32_t));
	else
		kmask = UINT32_MAX;

	if (strict) {
		if (tmask != kmask)
			return (1);
	} else {
		if ((tmask & kmask) != kmask)
			return (1);
	}

	return !((tmth & kmask) == (kmth & kmask));
}

int
swofp_ox_cmp_vlan_vid(struct ofp_ox_match *target,
    struct ofp_ox_match *key, int strict)
{
	uint16_t tmth, tmask, kmth, kmask;

	if (OFP_OXM_GET_FIELD(target) != OFP_OXM_GET_FIELD(key) ||
	    OFP_OXM_GET_FIELD(target) != OFP_XM_T_VLAN_VID)
		return (1);

	memcpy(&tmth, ((caddr_t)target + sizeof(*target)), sizeof(uint16_t));
	if (OFP_OXM_GET_HASMASK(target))
		memcpy(&tmask, ((caddr_t)target + sizeof(*target)
		    + sizeof(uint16_t)), sizeof(uint16_t));
	else
		tmask = UINT16_MAX;

	memcpy(&kmth, ((caddr_t)key + sizeof(*key)), sizeof(uint16_t));
	if (OFP_OXM_GET_HASMASK(key))
		memcpy(&kmask, ((caddr_t)key + sizeof(*key) +
		    sizeof(uint16_t)), sizeof(uint16_t));
	else
		kmask = UINT16_MAX;

	tmth &= htons(EVL_VLID_MASK);
	tmask &= htons(EVL_VLID_MASK);
	kmth &= htons(EVL_VLID_MASK);
	kmask &= htons(EVL_VLID_MASK);

	if (strict) {
		if (tmask != kmask)
			return (1);
	} else {
		if ((tmask & kmask) != kmask)
			return (1);
	}

	return !((tmth & kmask) == (kmth & kmask));
}

int
swofp_ox_cmp_ether_addr(struct ofp_ox_match *target,
    struct ofp_ox_match *key, int strict)
{
	uint64_t	 tmth, tmask, kmth, kmask;
	uint64_t	 eth_mask = 0x0000FFFFFFFFFFFFULL;


	if (OFP_OXM_GET_FIELD(target) != OFP_OXM_GET_FIELD(key))
		return (1);

	switch (OFP_OXM_GET_FIELD(target)) {
	case OFP_XM_T_ETH_SRC:
	case OFP_XM_T_ETH_DST:
	case OFP_XM_T_ARP_SHA:
	case OFP_XM_T_ARP_THA:
	case OFP_XM_T_IPV6_ND_SLL:
	case OFP_XM_T_IPV6_ND_TLL:
		break;
	default:
		return (1);
	}

	memcpy(&tmth, ((caddr_t)target + sizeof(*target)), ETHER_ADDR_LEN);
	if (OFP_OXM_GET_HASMASK(target))
		memcpy(&tmask, ((caddr_t)target + sizeof(*target) +
		    ETHER_ADDR_LEN), ETHER_ADDR_LEN);
	else
		tmask = UINT64_MAX;

	memcpy(&kmth, ((caddr_t)key + sizeof(*key)), ETHER_ADDR_LEN);
	if (OFP_OXM_GET_HASMASK(key))
		memcpy(&kmask, ((caddr_t)key + sizeof(*key) +
		    ETHER_ADDR_LEN), ETHER_ADDR_LEN);
	else
		kmask = UINT64_MAX;

	tmask &= eth_mask;
	tmth &= eth_mask;
	kmask &= eth_mask;
	kmth &= eth_mask;

	if (strict) {
		if (tmask != kmask)
			return (1);
	} else {
		if ((tmask & kmask) != kmask)
			return (1);
	}

	return !((tmth & kmask) == (kmth & kmask));
}

int
swofp_validate_oxm(struct ofp_ox_match *oxm, uint16_t *err)
{
	struct ofp_oxm_class	*handler;
	int			 hasmask;
	int			 neededlen;

	handler = swofp_lookup_oxm_handler(oxm);
	if (handler == NULL || handler->oxm_match == NULL) {
		*err = OFP_ERRMATCH_BAD_FIELD;
		return (-1);
	}

	hasmask = OFP_OXM_GET_HASMASK(oxm);

	neededlen = (hasmask) ?
	    (handler->oxm_len * 2) : (handler->oxm_len);
	if (oxm->oxm_length != neededlen) {
		*err = OFP_ERRMATCH_BAD_LEN;
		return (-1);
	}

	return (0);
}

int
swofp_validate_flow_match(struct ofp_match *om, uint16_t *err)
{
	struct ofp_ox_match *oxm;

	/*
	 * TODO this function is missing checks for:
	 * - OFP_ERRMATCH_BAD_TAG;
	 * - OFP_ERRMATCH_BAD_VALUE;
	 * - OFP_ERRMATCH_BAD_MASK;
	 * - OFP_ERRMATCH_BAD_PREREQ;
	 * - OFP_ERRMATCH_DUP_FIELD;
	 */
	OFP_OXM_FOREACH(om, ntohs(om->om_length), oxm) {
		if (swofp_validate_oxm(oxm, err))
			return (*err);
	}

	return (0);
}

int
swofp_validate_flow_instruction(struct switch_softc *sc,
    struct ofp_instruction *oi, size_t total, uint16_t *etype,
    uint16_t *err)
{
	struct ofp_action_header	*oah;
	struct ofp_instruction_actions	*oia;
	int				 ilen;

	*etype = OFP_ERRTYPE_BAD_INSTRUCTION;

	ilen = ntohs(oi->i_len);
	/* Check for bigger than packet or smaller than header. */
	if (ilen > total || ilen < sizeof(*oi)) {
		*err = OFP_ERRINST_BAD_LEN;
		return (-1);
	}

	switch (ntohs(oi->i_type)) {
	case OFP_INSTRUCTION_T_GOTO_TABLE:
		if (ilen != sizeof(struct ofp_instruction_goto_table)) {
			*err = OFP_ERRINST_BAD_LEN;
			return (-1);
		}
		break;
	case OFP_INSTRUCTION_T_WRITE_META:
		if (ilen != sizeof(struct ofp_instruction_write_metadata)) {
			*err = OFP_ERRINST_BAD_LEN;
			return (-1);
		}
		break;
	case OFP_INSTRUCTION_T_METER:
		if (ilen != sizeof(struct ofp_instruction_meter)) {
			*err = OFP_ERRINST_BAD_LEN;
			return (-1);
		}
		break;

	case OFP_INSTRUCTION_T_WRITE_ACTIONS:
	case OFP_INSTRUCTION_T_CLEAR_ACTIONS:
	case OFP_INSTRUCTION_T_APPLY_ACTIONS:
		if (ilen < sizeof(*oia)) {
			*err = OFP_ERRINST_BAD_LEN;
			return (-1);
		}

		oia = (struct ofp_instruction_actions *)oi;

		/* Validate actions before iterating over them. */
		oah = (struct ofp_action_header *)
		    ((uint8_t *)oia + sizeof(*oia));
		if (swofp_validate_action(sc, oah, ilen - sizeof(*oia),
		    err)) {
			*etype = OFP_ERRTYPE_BAD_ACTION;
			return (-1);
		}
		break;

	case OFP_INSTRUCTION_T_EXPERIMENTER:
		/* FALLTHROUGH */
	default:
		*err = OFP_ERRINST_UNKNOWN_INST;
		return (-1);
	}

	return (0);
}

int
swofp_validate_action(struct switch_softc *sc, struct ofp_action_header *ah,
    size_t ahtotal, uint16_t *err)
{
	struct ofp_action_handler	*oah;
	struct ofp_ox_match		*oxm;
	struct ofp_action_push		*ap;
	struct ofp_action_group		*ag;
	struct ofp_action_output	*ao;
	struct switch_port		*swpo;
	uint8_t				*dptr;
	int				 ahtype, ahlen, oxmlen;

	/* No actions. */
	if (ahtotal == 0)
		return (0);

	/* Check if we have at least the first header. */
	if (ahtotal < sizeof(*ah)) {
		*err = OFP_ERRACTION_LEN;
		return (-1);
	}

 parse_next_action:
	ahtype = ntohs(ah->ah_type);
	ahlen = ntohs(ah->ah_len);
	if (ahlen < sizeof(*ah) || ahlen > ahtotal) {
		*err = OFP_ERRACTION_LEN;
		return (-1);
	}

	switch (ahtype) {
	case OFP_ACTION_OUTPUT:
		if (ahlen != sizeof(struct ofp_action_output)) {
			*err = OFP_ERRACTION_LEN;
			return (-1);
		}

		ao = (struct ofp_action_output *)ah;
		switch (ntohl(ao->ao_port)) {
		case OFP_PORT_ANY:
			*err = OFP_ERRACTION_OUT_PORT;
			return (-1);

		case OFP_PORT_ALL:
		case OFP_PORT_NORMAL:
			/* TODO implement port ALL and NORMAL. */
			*err = OFP_ERRACTION_OUT_PORT;
			return (-1);

		case OFP_PORT_CONTROLLER:
		case OFP_PORT_FLOWTABLE:
		case OFP_PORT_FLOOD:
		case OFP_PORT_INPUT:
		case OFP_PORT_LOCAL:
			break;

		default:
			TAILQ_FOREACH(swpo, &sc->sc_swpo_list,
			    swpo_list_next) {
				if (swpo->swpo_port_no ==
				    ntohl(ao->ao_port))
					break;
			}
			if (swpo == NULL) {
				*err = OFP_ERRACTION_OUT_PORT;
				return (-1);
			}
			break;
		}
		break;
	case OFP_ACTION_GROUP:
		if (ahlen != sizeof(struct ofp_action_group)) {
			*err = OFP_ERRACTION_LEN;
			return (-1);
		}

		ag = (struct ofp_action_group *)ah;
		if (swofp_group_entry_lookup(sc,
		    ntohl(ag->ag_group_id)) == NULL) {
			*err = OFP_ERRACTION_BAD_OUT_GROUP;
			return (-1);
		}
		break;
	case OFP_ACTION_SET_QUEUE:
		if (ahlen != sizeof(struct ofp_action_set_queue)) {
			*err = OFP_ERRACTION_LEN;
			return (-1);
		}
		break;
	case OFP_ACTION_SET_MPLS_TTL:
		if (ahlen != sizeof(struct ofp_action_mpls_ttl)) {
			*err = OFP_ERRACTION_LEN;
			return (-1);
		}
		break;
	case OFP_ACTION_SET_NW_TTL:
		if (ahlen != sizeof(struct ofp_action_nw_ttl)) {
			*err = OFP_ERRACTION_LEN;
			return (-1);
		}
		break;
	case OFP_ACTION_COPY_TTL_OUT:
	case OFP_ACTION_COPY_TTL_IN:
	case OFP_ACTION_DEC_MPLS_TTL:
	case OFP_ACTION_POP_VLAN:
		if (ahlen != sizeof(struct ofp_action_header)) {
			*err = OFP_ERRACTION_LEN;
			return (-1);
		}
		break;
	case OFP_ACTION_PUSH_VLAN:
	case OFP_ACTION_PUSH_MPLS:
	case OFP_ACTION_PUSH_PBB:
		if (ahlen != sizeof(struct ofp_action_push)) {
			*err = OFP_ERRACTION_LEN;
			return (-1);
		}

		ap = (struct ofp_action_push *)ah;
		switch (ntohs(ap->ap_type)) {
		case OFP_ACTION_PUSH_VLAN:
			if (ntohs(ap->ap_ethertype) != ETHERTYPE_VLAN &&
			    ntohs(ap->ap_ethertype) != ETHERTYPE_QINQ) {
				*err = OFP_ERRACTION_ARGUMENT;
				return (-1);
			}
			break;

		case OFP_ACTION_PUSH_MPLS:
		case OFP_ACTION_PUSH_PBB:
			/* Not implemented yet. */
		default:
			*err = OFP_ERRACTION_TYPE;
			return (-1);
		}
		break;
	case OFP_ACTION_POP_MPLS:
		if (ahlen != sizeof(struct ofp_action_pop_mpls)) {
			*err = OFP_ERRACTION_LEN;
			return (-1);
		}
		break;
	case OFP_ACTION_SET_FIELD:
		if (ahlen < sizeof(struct ofp_action_set_field)) {
			*err = OFP_ERRACTION_LEN;
			return (-1);
		}

		oxmlen = ahlen - (sizeof(struct ofp_action_set_field) -
		    offsetof(struct ofp_action_set_field, asf_field));
		if (oxmlen < sizeof(*oxm)) {
			*err = OFP_ERRACTION_LEN;
			return (-1);
		}

		dptr = (uint8_t *)ah;
		dptr += sizeof(struct ofp_action_set_field) -
		    offsetof(struct ofp_action_set_field, asf_field);
		while (oxmlen > 0) {
			oxm = (struct ofp_ox_match *)dptr;
			if (swofp_validate_oxm(oxm, err)) {
				if (*err == OFP_ERRMATCH_BAD_LEN)
					*err = OFP_ERRACTION_SET_LEN;
				else
					*err = OFP_ERRACTION_SET_TYPE;

				return (-1);
			}

			dptr += sizeof(*oxm) + oxm->oxm_length;
			oxmlen -= sizeof(*oxm) + oxm->oxm_length;
		}
		break;

	default:
		/* Unknown/unsupported action. */
		*err = OFP_ERRACTION_TYPE;
		return (-1);
	}

	oah = swofp_lookup_action_handler(ahtype);
	/* Unknown/unsupported action. */
	if (oah == NULL) {
		*err = OFP_ERRACTION_TYPE;
		return (-1);
	}

	ahtotal -= min(ahlen, ahtotal);
	if (ahtotal) {
		ah = (struct ofp_action_header *)((uint8_t *)ah + ahlen);
		goto parse_next_action;
	}

	return (0);
}

int
swofp_flow_filter_out_port(struct ofp_instruction_actions *oia,
    uint32_t out_port)
{
	struct ofp_action_header	*oah;
	struct ofp_action_output	*oao;

	if (oia == NULL)
		return (0);

	OFP_I_ACTIONS_FOREACH((struct ofp_instruction_actions *)oia, oah) {
		if (ntohs(oah->ah_type) == OFP_ACTION_OUTPUT) {
			oao = (struct ofp_action_output *)oah;
			if (ntohl(oao->ao_port) == out_port)
				return (1);
		}
	}

	return (0);
}

int
swofp_flow_filter(struct swofp_flow_entry *swfe, uint64_t cookie,
    uint64_t cookie_mask, uint32_t out_port, uint32_t out_group)
{

	if (cookie_mask != 0 &&
	    ((swfe->swfe_cookie & cookie_mask) != (cookie & cookie_mask)))
		return (0);

	if ((out_port == OFP_PORT_ANY) && (out_group == OFP_GROUP_ID_ALL))
		return (1);

	if ((out_port != OFP_PORT_ANY) &&
	    !(swofp_flow_filter_out_port(swfe->swfe_write_actions, out_port) ||
	    swofp_flow_filter_out_port(swfe->swfe_apply_actions, out_port)))
	    return (0);

	if (out_port != OFP_GROUP_ID_ALL) {
		/* XXX ignore group */
	}

	return (1);
}

int
swofp_flow_cmp_common(struct swofp_flow_entry *swfe, struct ofp_match *key,
    int strict)
{
	struct ofp_match	*target = swfe->swfe_match;
	struct ofp_oxm_class	*khandler;
	struct ofp_ox_match	*toxm, *koxm;
	void			*kmask;
	int			 len;
	/* maximam payload size is size of struct in6_addr */
	uint8_t			 dummy_unmask[sizeof(struct in6_addr)];

	memset(dummy_unmask, 0, sizeof(dummy_unmask));

	OFP_OXM_FOREACH(key, ntohs(key->om_length), koxm) {
		khandler = swofp_lookup_oxm_handler(koxm);
		if (khandler == NULL || khandler->oxm_match == NULL)
			return (0);

		len = khandler->oxm_len;

		/*
		 * OpenFlow Switch Specification 1.3.5 says:
		 *  - An all-zero-bits oxm_mask is equivalent to omitting
		 *    the OXM TLV entirely
		 */
		if (strict && OFP_OXM_GET_HASMASK(koxm)) {
			kmask = (void *)((caddr_t)koxm + sizeof(*koxm) + len);
			if (memcmp(kmask, dummy_unmask, len) == 0)
				continue;
		}

		OFP_OXM_FOREACH(target, ntohs(target->om_length), toxm) {
			if (khandler->oxm_cmp(toxm, koxm, strict) == 0)
				break;
		}
		if (OFP_OXM_TERMINATED(target, ntohs(target->om_length), toxm))
			return (0);
	}

	return (1);
}

int
swofp_flow_cmp_non_strict(struct swofp_flow_entry *swfe, struct ofp_match *key)
{
	/* Every oxm matching is wildcard */
	if (key == NULL)
		return (1);

	return swofp_flow_cmp_common(swfe, key, 0);
}

int
swofp_flow_cmp_strict(struct swofp_flow_entry *swfe, struct ofp_match *key,
    uint32_t priority)
{
	struct ofp_match	*target = swfe->swfe_match;
	struct ofp_ox_match	*toxm, *koxm;
	int			 key_matches, target_matches;

	/*
	 * Both target and key values are put on network byte order,
	 * so it's ok that those are compared without changing byte order
	 */
	if (swfe->swfe_priority != priority)
		return (0);

	key_matches = target_matches = 0;
	OFP_OXM_FOREACH(key, ntohs(key->om_length), koxm)
		key_matches++;

	OFP_OXM_FOREACH(target, ntohs(target->om_length), toxm)
		target_matches++;

	if (key_matches != target_matches)
		return (0);

	return swofp_flow_cmp_common(swfe, key, 1);
}

struct swofp_flow_entry *
swofp_flow_search_by_table(struct swofp_flow_table *swft, struct ofp_match *key,
    uint16_t priority)
{
	struct swofp_flow_entry	*swfe;

	LIST_FOREACH(swfe, &swft->swft_flow_list, swfe_next) {
		if (swofp_flow_cmp_strict(swfe, key, priority))
			return (swfe);
	}

	return (NULL);
}

int
swofp_flow_has_group(struct ofp_instruction_actions *oia, uint32_t group_id)
{
	struct ofp_action_header	*oah;
	struct ofp_action_group		*oag;

	if (oia == NULL)
		return (0);

	OFP_I_ACTIONS_FOREACH((struct ofp_instruction_actions *)oia, oah) {
		if (ntohs(oah->ah_type) == OFP_ACTION_GROUP) {
			oag = (struct ofp_action_group *)oah;
			if (ntohl(oag->ag_group_id) == group_id)
				return (1);
		}
	}

	return (0);
}

void
swofp_flow_delete_on_table_by_group(struct switch_softc *sc,
    struct swofp_flow_table *swft, uint32_t group_id)
{
	struct swofp_flow_entry	*swfe, *tswfe;

	LIST_FOREACH_SAFE(swfe, &swft->swft_flow_list, swfe_next, tswfe) {
		if (swofp_flow_has_group(swfe->swfe_apply_actions, group_id) ||
		    swofp_flow_has_group(swfe->swfe_write_actions, group_id)) {
			swofp_flow_entry_delete(sc, swft, swfe,
			    OFP_FLOWREM_REASON_GROUP_DELETE);
		}
	}
}

void
swofp_flow_delete_on_table(struct switch_softc *sc,
    struct swofp_flow_table *swft, struct ofp_match *key, uint16_t priority,
    uint64_t cookie, uint64_t cookie_mask, uint32_t out_port,
    uint32_t out_group, int strict)
{
	struct swofp_flow_entry	*swfe, *tswfe;

	LIST_FOREACH_SAFE(swfe, &swft->swft_flow_list, swfe_next, tswfe) {
		if (strict && !swofp_flow_cmp_strict(swfe, key, priority))
			continue;
		else if (!swofp_flow_cmp_non_strict(swfe, key))
			continue;

		if (!swofp_flow_filter(swfe, cookie, cookie_mask,
		    out_port, out_group))
			continue;

		swofp_flow_entry_delete(sc, swft, swfe,
		    OFP_FLOWREM_REASON_DELETE);
	}
}

void
swofp_ox_match_put_start(struct ofp_match *om)
{
	om->om_type = htons(OFP_MATCH_OXM);
	om->om_length = htons(sizeof(*om));
}

/*
 * Return ofp_match length include "PADDING" byte
 */
int
swofp_ox_match_put_end(struct ofp_match *om)
{
	int	 tsize = ntohs(om->om_length);
	int	 padding;

	padding = OFP_ALIGN(tsize) - tsize;
	if (padding)
		memset((caddr_t)om + tsize, 0, padding);

	return tsize + padding;
}

int
swofp_ox_match_put_uint32(struct ofp_match *om, uint8_t type, uint32_t val)
{
	int	 off = ntohs(om->om_length);
	struct ofp_ox_match *oxm;

	val = htonl(val);
	oxm = (struct ofp_ox_match *)((caddr_t)om + off);
	oxm->oxm_class = htons(OFP_OXM_C_OPENFLOW_BASIC);
	OFP_OXM_SET_FIELD(oxm, type);
	oxm->oxm_length = sizeof(uint32_t);
	memcpy(oxm->oxm_value, &val, sizeof(val));
	om->om_length = htons(ntohs(om->om_length) +
	    sizeof(*oxm) + sizeof(uint32_t));

	return ntohs(om->om_length);
}

int
swofp_ox_match_put_uint64(struct ofp_match *om, uint8_t type, uint64_t val)
{
	struct ofp_ox_match	*oxm;
	int			 off = ntohs(om->om_length);

	val = htobe64(val);
	oxm = (struct ofp_ox_match *)((caddr_t)om + off);
	oxm->oxm_class = htons(OFP_OXM_C_OPENFLOW_BASIC);
	OFP_OXM_SET_FIELD(oxm, type);
	oxm->oxm_length = sizeof(uint64_t);
	memcpy(oxm->oxm_value, &val, sizeof(val));
	om->om_length = htons(ntohs(om->om_length) +
	    sizeof(*oxm) + sizeof(uint64_t));

	return ntohs(om->om_length);
}

int
swofp_nx_match_put(struct ofp_match *om, uint8_t type, int len,
    caddr_t val)
{
	struct ofp_ox_match	*oxm;
	int			 off = ntohs(om->om_length);

	oxm = (struct ofp_ox_match *)((caddr_t)om + off);
	oxm->oxm_class = htons(OFP_OXM_C_NXM_1);
	OFP_OXM_SET_FIELD(oxm, type);
	oxm->oxm_length = len;
	memcpy((void *)oxm->oxm_value, val, len);

	om->om_length = htons(ntohs(om->om_length) + sizeof(*oxm) + len);

	return ntohs(om->om_length);
}

int
swofp_ox_set_vlan_vid(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint16_t	 val;

	val = *(uint16_t *)oxm->oxm_value;
	swfcl->swfcl_vlan->vlan_vid = (val & htons(EVL_VLID_MASK));

	return (0);
}

int
swofp_ox_set_uint8(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint8_t		 val;

	val = *(uint8_t *)oxm->oxm_value;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_IP_DSCP:
		if (swfcl->swfcl_ipv4)
			swfcl->swfcl_ipv4->ipv4_tos = ((val << 2) |
			    (swfcl->swfcl_ipv4->ipv4_tos & IPTOS_ECN_MASK));
		else
			swfcl->swfcl_ipv6->ipv6_tclass = ((val << 2) |
			    (swfcl->swfcl_ipv6->ipv6_tclass & IPTOS_ECN_MASK));
		break;
	case OFP_XM_T_IP_ECN:
		if (swfcl->swfcl_ipv4)
			swfcl->swfcl_ipv4->ipv4_tos = ((val & IPTOS_ECN_MASK) |
			    (swfcl->swfcl_ipv4->ipv4_tos & ~IPTOS_ECN_MASK));
		else
			swfcl->swfcl_ipv6->ipv6_tclass = (
			    (val & IPTOS_ECN_MASK) |
			    (swfcl->swfcl_ipv6->ipv6_tclass & ~IPTOS_ECN_MASK));
		break;
	case OFP_XM_T_IP_PROTO:
		if (swfcl->swfcl_ipv4)
			swfcl->swfcl_ipv4->ipv4_proto = val;
		else
			swfcl->swfcl_ipv6->ipv6_nxt = val;
		break;
	case OFP_XM_T_ICMPV4_TYPE:
		swfcl->swfcl_icmpv4->icmpv4_type = val;
		break;
	case OFP_XM_T_ICMPV4_CODE:
		swfcl->swfcl_icmpv4->icmpv4_code = val;
		break;
	case OFP_XM_T_ICMPV6_TYPE:
		swfcl->swfcl_icmpv6->icmpv6_type = val;
		break;
	case OFP_XM_T_ICMPV6_CODE:
		swfcl->swfcl_icmpv6->icmpv6_code = val;
		break;
	}

	return (0);
}

int
swofp_ox_set_uint16(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint16_t	 val;

	val = *(uint16_t *)oxm->oxm_value;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_ETH_TYPE:
		swfcl->swfcl_ether->eth_type = val;
		break;
	case OFP_XM_T_VLAN_PCP:
		swfcl->swfcl_vlan->vlan_pcp = val;
		break;
	case OFP_XM_T_TCP_SRC:
		swfcl->swfcl_tcp->tcp_src = val;
		break;
	case OFP_XM_T_TCP_DST:
		swfcl->swfcl_tcp->tcp_dst = val;
		break;
	case OFP_XM_T_UDP_SRC:
		swfcl->swfcl_udp->udp_src = val;
		break;
	case OFP_XM_T_UDP_DST:
		swfcl->swfcl_udp->udp_dst = val;
		break;
	case OFP_XM_T_ARP_OP:
		swfcl->swfcl_arp->_arp_op = val;
	}

	return (0);
}

int
swofp_ox_set_uint32(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint32_t	val;

	val = *(uint32_t *)oxm->oxm_value;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_IPV6_FLABEL:
		swfcl->swfcl_ipv6->ipv6_flow_label = val;
		break;
	}

	return (0);
}

int
swofp_ox_set_uint64(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint64_t	 val;

	val = *(uint64_t *)oxm->oxm_value;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_TUNNEL_ID: /* alias OFP_XM_NXMT_TUNNEL_ID */
		swfcl->swfcl_tunnel->tun_key = val;
		break;
	}

	return (0);
}

int
swofp_ox_set_ipv6_addr(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	struct in6_addr	 val;

	memcpy(&val, oxm->oxm_value, sizeof(val));

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_NXMT_TUNNEL_IPV6_SRC:
		swfcl->swfcl_tunnel->tun_ipv6_src = val;
		break;
	case OFP_XM_NXMT_TUNNEL_IPV6_DST:
		swfcl->swfcl_tunnel->tun_ipv6_dst = val;
		break;
	case OFP_XM_T_IPV6_SRC:
		swfcl->swfcl_ipv6->ipv6_src = val;
		break;
	case OFP_XM_T_IPV6_DST:
		swfcl->swfcl_ipv6->ipv6_dst = val;
		break;
	case OFP_XM_T_IPV6_ND_TARGET:
		swfcl->swfcl_nd6->nd6_target = val;
		break;
	}

	return (0);
}

int
swofp_ox_set_ipv4_addr(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint32_t	 val;

	val = *(uint32_t *)oxm->oxm_value;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_NXMT_TUNNEL_IPV4_SRC:
		swfcl->swfcl_tunnel->tun_ipv4_src = *(struct in_addr *)&val;
		break;
	case OFP_XM_NXMT_TUNNEL_IPV4_DST:
		swfcl->swfcl_tunnel->tun_ipv4_dst = *(struct in_addr *)&val;
		break;
	case OFP_XM_T_IPV4_SRC:
		swfcl->swfcl_ipv4->ipv4_src = val;
		break;
	case OFP_XM_T_IPV4_DST:
		swfcl->swfcl_ipv4->ipv4_dst = val;
		break;
	case OFP_XM_T_ARP_SPA:
		swfcl->swfcl_arp->arp_sip = val;
		break;
	case OFP_XM_T_ARP_TPA:
		swfcl->swfcl_arp->arp_tip = val;
		break;
	}

	return (0);
}

int
swofp_ox_set_ether_addr(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	caddr_t		eth_addr;

	eth_addr = oxm->oxm_value;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_ETH_SRC:
		memcpy(swfcl->swfcl_ether->eth_src, eth_addr, ETHER_ADDR_LEN);
		break;
	case OFP_XM_T_ETH_DST:
		memcpy(swfcl->swfcl_ether->eth_dst, eth_addr, ETHER_ADDR_LEN);
		break;
	case OFP_XM_T_ARP_SHA:
		memcpy(swfcl->swfcl_arp->arp_sha, eth_addr, ETHER_ADDR_LEN);
		break;
	case OFP_XM_T_ARP_THA:
		memcpy(swfcl->swfcl_arp->arp_tha, eth_addr, ETHER_ADDR_LEN);
		break;
	case OFP_XM_T_IPV6_ND_TLL:
	case OFP_XM_T_IPV6_ND_SLL:
		memcpy(swfcl->swfcl_nd6->nd6_lladdr, eth_addr, ETHER_ADDR_LEN);
		break;
	}

	return (0);
}

#ifdef INET6
int
swofp_ox_match_ipv6_addr(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	struct in6_addr	 in, mth, mask = in6mask128;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_NXMT_TUNNEL_IPV6_SRC:
	case OFP_XM_NXMT_TUNNEL_IPV6_DST:
		if (swfcl->swfcl_tunnel == NULL)
			return (1);
		break;
	case OFP_XM_T_IPV6_SRC:
	case OFP_XM_T_IPV6_DST:
		if (swfcl->swfcl_ipv6 == NULL)
			return (1);
		break;
	case OFP_XM_T_IPV6_ND_TARGET:
		if (swfcl->swfcl_nd6 == NULL)
			return (1);
		break;
	default:
		return (1);
	}

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_NXMT_TUNNEL_IPV6_SRC:
		in = swfcl->swfcl_tunnel->tun_ipv6_src;
		break;
	case OFP_XM_NXMT_TUNNEL_IPV6_DST:
		in = swfcl->swfcl_tunnel->tun_ipv6_dst;
		break;
	case OFP_XM_T_IPV6_SRC:
		in = swfcl->swfcl_ipv6->ipv6_src;
		break;
	case OFP_XM_T_IPV6_DST:
		in = swfcl->swfcl_ipv6->ipv6_dst;
		break;
	case OFP_XM_T_IPV6_ND_TARGET:
		in = swfcl->swfcl_nd6->nd6_target;
		break;
	}

	memcpy(&mth, oxm->oxm_value, sizeof(mth));

	if (OFP_OXM_GET_HASMASK(oxm)) {
		memcpy(&mask, oxm->oxm_value + sizeof(mth),
		    sizeof(mask));

		in.s6_addr32[0] &= mask.s6_addr32[0];
		in.s6_addr32[1] &= mask.s6_addr32[1];
		in.s6_addr32[2] &= mask.s6_addr32[2];
		in.s6_addr32[3] &= mask.s6_addr32[3];

		mth.s6_addr32[0] &= mask.s6_addr32[0];
		mth.s6_addr32[1] &= mask.s6_addr32[1];
		mth.s6_addr32[2] &= mask.s6_addr32[2];
		mth.s6_addr32[3] &= mask.s6_addr32[3];
	}

	return memcmp(&in, &mth, sizeof(in));
}
#endif /* INET6 */

int
swofp_ox_match_ipv4_addr(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint32_t	 in, mth, mask;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_NXMT_TUNNEL_IPV4_SRC:
	case OFP_XM_NXMT_TUNNEL_IPV4_DST:
		if (swfcl->swfcl_tunnel == NULL)
			return (1);
		break;
	case OFP_XM_T_IPV4_SRC:
	case OFP_XM_T_IPV4_DST:
		if (swfcl->swfcl_ipv4 == NULL)
			return (1);
		break;
	case OFP_XM_T_ARP_SPA:
	case OFP_XM_T_ARP_TPA:
		if (swfcl->swfcl_arp == NULL)
			return (1);
		break;
	default:
		return (1);
	}

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_NXMT_TUNNEL_IPV4_SRC:
		in = swfcl->swfcl_tunnel->tun_ipv4_src.s_addr;
		break;
	case OFP_XM_NXMT_TUNNEL_IPV4_DST:
		in = swfcl->swfcl_tunnel->tun_ipv4_dst.s_addr;
		break;
	case OFP_XM_T_IPV4_SRC:
		in = swfcl->swfcl_ipv4->ipv4_src;
		break;
	case OFP_XM_T_IPV4_DST:
		in = swfcl->swfcl_ipv4->ipv4_dst;
		break;
	case OFP_XM_T_ARP_SPA:
		in = swfcl->swfcl_arp->arp_sip;
		break;
	case OFP_XM_T_ARP_TPA:
		in = swfcl->swfcl_arp->arp_tip;
		break;
	}

	memcpy(&mth, oxm->oxm_value, sizeof(uint32_t));

	if (OFP_OXM_GET_HASMASK(oxm))
		memcpy(&mask, oxm->oxm_value + sizeof(uint32_t),
		    sizeof(uint32_t));
	else
		mask = UINT32_MAX;

	return !((in & mask) == (mth & mask));
}

int
swofp_ox_match_vlan_vid(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint16_t	 in, mth, mask = 0;

	if (swfcl->swfcl_vlan == NULL)
		return (1);

	in = swfcl->swfcl_vlan->vlan_vid;
	memcpy(&mth, oxm->oxm_value, sizeof(uint16_t));

	if (OFP_OXM_GET_HASMASK(oxm))
		memcpy(&mask, oxm->oxm_value + sizeof(uint16_t),
		    sizeof(uint16_t));
	else
		mask = UINT16_MAX;

	/*
	 * OpenFlow Switch Specification ver 1.3.5 says if oxm value
	 * is OFP_XM_VID_NONE, matches only packets without a VLAN tag
	 */
	if (mth == htons(OFP_XM_VID_NONE))
		return (1);

	/*
	 * OpenFlow Switch Specification ver 1.3.5 says if oxm value and mask
	 * is OFP_XM_VID_PRESENT, matches only packets with a VLAN tag
	 * regardless of its value.
	 */
	if (ntohs(mth) == OFP_XM_VID_PRESENT &&
	    ntohs(mask) == OFP_XM_VID_PRESENT)
		return (0);

	in &= htons(EVL_VLID_MASK);
	mth &= htons(EVL_VLID_MASK);

	return !((in & mask) == (mth & mask));
}

int
swofp_ox_match_uint8(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint8_t		 in, mth;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_VLAN_PCP:
		if (swfcl->swfcl_vlan == NULL)
			return (1);
		break;
	case OFP_XM_T_IP_DSCP:
	case OFP_XM_T_IP_ECN:
	case OFP_XM_T_IP_PROTO:
		if ((swfcl->swfcl_ipv4 == NULL &&
		    swfcl->swfcl_ipv6 == NULL))
			return (1);
		break;
	case OFP_XM_T_ICMPV4_CODE:
	case OFP_XM_T_ICMPV4_TYPE:
		if (swfcl->swfcl_icmpv4 == NULL)
			return (1);
		break;
	case OFP_XM_T_ICMPV6_CODE:
	case OFP_XM_T_ICMPV6_TYPE:
		if (swfcl->swfcl_icmpv6 == NULL)
			return (1);
		break;
	default:
		return (1);
	}

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_VLAN_PCP:
		in = swfcl->swfcl_vlan->vlan_pcp;
		break;
	case OFP_XM_T_IP_DSCP:
		if (swfcl->swfcl_ipv4)
			in = swfcl->swfcl_ipv4->ipv4_tos >> 2;
		else
			in = swfcl->swfcl_ipv6->ipv6_tclass >> 2;
		break;
	case OFP_XM_T_IP_ECN:
		if (swfcl->swfcl_ipv4)
			in = (swfcl->swfcl_ipv4->ipv4_tos) & IPTOS_ECN_MASK;
		else
			in = (swfcl->swfcl_ipv6->ipv6_tclass) & IPTOS_ECN_MASK;
		break;
	case OFP_XM_T_IP_PROTO:
		if (swfcl->swfcl_ipv4)
			in = swfcl->swfcl_ipv4->ipv4_proto;
		else
			in = swfcl->swfcl_ipv6->ipv6_nxt;
		break;
	case OFP_XM_T_ICMPV4_CODE:
		in = swfcl->swfcl_icmpv4->icmpv4_code;
		break;
	case OFP_XM_T_ICMPV4_TYPE:
		in = swfcl->swfcl_icmpv4->icmpv4_type;
		break;
	case OFP_XM_T_ICMPV6_CODE:
		in = swfcl->swfcl_icmpv6->icmpv6_code;
		break;
	case OFP_XM_T_ICMPV6_TYPE:
		in = swfcl->swfcl_icmpv6->icmpv6_type;
		break;
	}

	memcpy(&mth, oxm->oxm_value, sizeof(uint8_t));

	return !(in == mth);

}

int
swofp_ox_match_uint16(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint16_t	 in, mth;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_ETH_TYPE:
		if (swfcl->swfcl_ether == NULL)
			return (1);
		break;
	case OFP_XM_T_TCP_SRC:
	case OFP_XM_T_TCP_DST:
		if (swfcl->swfcl_tcp == NULL)
			return (1);
		break;
	case OFP_XM_T_UDP_SRC:
	case OFP_XM_T_UDP_DST:
		if (swfcl->swfcl_udp == NULL)
			return (1);
		break;
	case OFP_XM_T_ARP_OP:
		if (swfcl->swfcl_arp == NULL)
			return (1);
		break;
	default:
		return (1);
	}

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_ETH_TYPE:
		in = swfcl->swfcl_ether->eth_type;
		break;
	case OFP_XM_T_TCP_SRC:
		in = swfcl->swfcl_tcp->tcp_src;
		break;
	case OFP_XM_T_TCP_DST:
		in = swfcl->swfcl_tcp->tcp_dst;
		break;
	case OFP_XM_T_UDP_SRC:
		in = swfcl->swfcl_udp->udp_src;
		break;
	case OFP_XM_T_UDP_DST:
		in = swfcl->swfcl_udp->udp_dst;
		break;
	case OFP_XM_T_ARP_OP:
		in = swfcl->swfcl_arp->_arp_op;
		break;
	}

	memcpy(&mth, oxm->oxm_value, sizeof(uint16_t));

	return !(in == mth);
}

int
swofp_ox_match_uint32(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint32_t	 in, mth, mask, nomask = UINT32_MAX;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_IN_PORT:
		/* in_port field is always exist in swfcl */
		break;
	case OFP_XM_T_IPV6_FLABEL:
		if (swfcl->swfcl_ipv6 == NULL)
			return (1);
		break;
	default:
		return (1);
	}

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_IN_PORT:
		/*
		 * in_port isn't network byte order becouse
		 * it's pipeline match field.
		 */
		in = htonl(swfcl->swfcl_in_port);
		break;
	case OFP_XM_T_IPV6_FLABEL:
		in = swfcl->swfcl_ipv6->ipv6_flow_label;
		nomask &= IPV6_FLOWLABEL_MASK;
		break;
	}

	memcpy(&mth, oxm->oxm_value, sizeof(uint32_t));

	if (OFP_OXM_GET_HASMASK(oxm))
		memcpy(&mask, oxm->oxm_value + sizeof(uint32_t),
		    sizeof(uint32_t));
	else
		mask = nomask;

	return !((in & mask) == (mth & mask));
}

int
swofp_ox_match_uint64(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint64_t	 in, mth, mask;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_META:
		break;
	case OFP_XM_T_TUNNEL_ID:
		if (swfcl->swfcl_tunnel == NULL)
			return (1);
		break;
	default:
		return (1);
	}

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_META:
		in = swfcl->swfcl_metadata;
		break;
	case OFP_XM_T_TUNNEL_ID:
		in = swfcl->swfcl_tunnel->tun_key;
		break;
	}

	memcpy(&mth, oxm->oxm_value, sizeof(uint64_t));

	if (OFP_OXM_GET_HASMASK(oxm))
		memcpy(&mask, oxm->oxm_value + sizeof(uint64_t),
		    sizeof(uint64_t));
	else
		mask = UINT64_MAX;

	return !((in & mask) == (mth & mask));
}

int
swofp_ox_match_ether_addr(struct switch_flow_classify *swfcl,
    struct ofp_ox_match *oxm)
{
	uint64_t	 eth_mask = 0x0000FFFFFFFFFFFFULL;
	uint64_t	 in, mth, mask;

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_ETH_SRC:
	case OFP_XM_T_ETH_DST:
		if (swfcl->swfcl_ether == NULL)
			return (1);
		break;
	case OFP_XM_T_ARP_SHA:
	case OFP_XM_T_ARP_THA:
		if (swfcl->swfcl_arp == NULL)
			return (1);
		break;
	case OFP_XM_T_IPV6_ND_SLL:
	case OFP_XM_T_IPV6_ND_TLL:
		if (swfcl->swfcl_nd6 == NULL)
			return (1);
		break;
	default:
		return (1);
	}

	switch (OFP_OXM_GET_FIELD(oxm)) {
	case OFP_XM_T_ETH_SRC:
		in = *(uint64_t *)(swfcl->swfcl_ether->eth_src);
		break;
	case OFP_XM_T_ETH_DST:
		in = *(uint64_t *)(swfcl->swfcl_ether->eth_dst);
		break;
	case OFP_XM_T_ARP_SHA:
		in = *(uint64_t *)(swfcl->swfcl_arp->arp_sha);
		break;
	case OFP_XM_T_ARP_THA:
		in = *(uint64_t *)(swfcl->swfcl_arp->arp_tha);
		break;
	case OFP_XM_T_IPV6_ND_SLL:
	case OFP_XM_T_IPV6_ND_TLL:
		in = *(uint64_t *)(swfcl->swfcl_nd6->nd6_lladdr);
		break;
	}

	memcpy(&mth, oxm->oxm_value, ETHER_ADDR_LEN);
	if (OFP_OXM_GET_HASMASK(oxm))
		memcpy(&mask, oxm->oxm_value + ETHER_ADDR_LEN,
		    ETHER_ADDR_LEN);
	else
		mask = UINT64_MAX;

	return !((in & mask & eth_mask) == (mth & mask & eth_mask));
}

int
swofp_flow_match_by_swfcl(struct ofp_match *om,
    struct switch_flow_classify *swfcl)
{
	struct ofp_oxm_class	*oxm_handler;
	struct ofp_ox_match	*oxm;

	OFP_OXM_FOREACH(om, ntohs(om->om_length), oxm) {
		oxm_handler = swofp_lookup_oxm_handler(oxm);
		if ((oxm_handler == NULL) ||
		    (oxm_handler->oxm_match == NULL))
			continue;

		if (oxm_handler->oxm_match(swfcl, oxm))
			return (1);
	}

	return (0);
}

/* TODO: Optimization */
struct swofp_flow_entry *
swofp_flow_lookup(struct swofp_flow_table *swft,
    struct switch_flow_classify *swfcl)
{
	struct swofp_flow_entry	*swfe, *interim = NULL;

	LIST_FOREACH(swfe, &swft->swft_flow_list, swfe_next) {
		if (swofp_flow_match_by_swfcl(swfe->swfe_match, swfcl) != 0)
			continue;

		if (interim == NULL ||
		    (interim->swfe_priority < swfe->swfe_priority))
			interim = swfe;
	}

	swft->swft_lookup_count++;
	if (interim)
		swft->swft_matched_count++;

	return interim;
}

/*
 * OpenFlow protocol push/pop VLAN
 */

/* Expand 802.1Q VLAN header from M_VLANTAG mtag if it is exist. */
struct mbuf *
swofp_expand_8021q_tag(struct mbuf *m)
{
	if ((m->m_flags & M_VLANTAG) == 0)
		return (m);

	/* H/W tagging supports only 802.1Q */
	return (vlan_inject(m, ETHERTYPE_VLAN,
	    EVL_VLANOFTAG(m->m_pkthdr.ether_vtag) |
	    EVL_PRIOFTAG(m->m_pkthdr.ether_vtag)));
}

struct mbuf *
swofp_action_pop_vlan(struct switch_softc *sc, struct mbuf *m,
    struct swofp_pipeline_desc *swpld, struct ofp_action_header *oah)
{
	struct switch_flow_classify	*swfcl = swpld->swpld_swfcl;
	struct ether_vlan_header	*evl;
	struct ether_header		 eh;

	/* no vlan tag existing */
	if (swfcl->swfcl_vlan == NULL) {
		m_freem(m);
		return (NULL);
	}

	if ((m->m_flags & M_VLANTAG)) {
		m->m_flags &= ~M_VLANTAG;
		return (m);
	}

	if (m->m_len < sizeof(*evl) &&
	    (m = m_pullup(m, sizeof(*evl))) == NULL)
		return (NULL);
	evl = mtod(m, struct ether_vlan_header *);

	if ((ntohs(evl->evl_encap_proto) != ETHERTYPE_VLAN) &&
	    (ntohs(evl->evl_encap_proto) != ETHERTYPE_QINQ)) {
		m_freem(m);
		return (NULL);
	}

	m_copydata(m, 0, ETHER_HDR_LEN, (caddr_t)&eh);
	eh.ether_type = evl->evl_proto;

	m_adj(m, sizeof(*evl));
	M_PREPEND(m, sizeof(eh), M_DONTWAIT);
	if (m == NULL)
		return (NULL);
	m_copyback(m, 0, sizeof(eh), &eh, M_NOWAIT);

	/*
	 * Update classify for vlan
	 */
	if (m->m_len < sizeof(*evl) &&
	    (m = m_pullup(m, sizeof(*evl))) == NULL)
		return (NULL);
	evl = mtod(m, struct ether_vlan_header *);

	if (ntohs(evl->evl_encap_proto) == ETHERTYPE_VLAN) {
		swfcl->swfcl_vlan->vlan_tpid = htons(ETHERTYPE_VLAN);
		swfcl->swfcl_vlan->vlan_vid =
		    (evl->evl_tag & htons(EVL_VLID_MASK));
		swfcl->swfcl_vlan->vlan_pcp =
		    EVL_PRIOFTAG(ntohs(evl->evl_tag));
	} else {
		pool_put(&swfcl_pool, swfcl->swfcl_vlan);
		swfcl->swfcl_vlan = NULL;
	}

	return (m);
}

struct mbuf *
swofp_action_push_vlan(struct switch_softc *sc, struct mbuf *m,
    struct swofp_pipeline_desc *swpld, struct ofp_action_header *oah)
{
	struct switch_flow_classify	*swfcl = swpld->swpld_swfcl;
	struct ofp_action_push		*oap;
	struct ether_header		*eh;
	struct ether_vlan_header	 evh;

	/*
	 * Expands 802.1Q VLAN header from M_VLANTAG because switch(4) doesn't
	 * use H/W tagging on port currently.
	 */
	m = swofp_expand_8021q_tag(m);
	if (m == NULL)
		return (NULL);

	oap = (struct ofp_action_push *)oah;

	if ((m->m_len < sizeof(*eh)) &&
	    ((m = m_pullup(m, sizeof(*eh))) == NULL)) {
		return (NULL);
	}
	eh = mtod(m, struct ether_header *);

	switch (ntohs(oap->ap_ethertype)) {
	case ETHERTYPE_VLAN:
		if ((ntohs(eh->ether_type) == ETHERTYPE_VLAN) ||
		    (ntohs(eh->ether_type) == ETHERTYPE_QINQ)) {
			m_freem(m);
			return (NULL);
		}
		break;
	case ETHERTYPE_QINQ:
		if (ntohs(eh->ether_type) != ETHERTYPE_VLAN) {
			m_freem(m);
			return (NULL);
		}
		break;
	default:
		m_freem(m);
		return (NULL);
	}

	if (swfcl->swfcl_vlan == NULL) {
		swfcl->swfcl_vlan = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (swfcl->swfcl_vlan == NULL) {
			m_freem(m);
			return (NULL);
		}
		/* puts default vlan */
		swfcl->swfcl_vlan->vlan_vid = htons(1);
	}

	m_copydata(m, 0, ETHER_HDR_LEN, (caddr_t)&evh);
	evh.evl_proto = evh.evl_encap_proto;
	evh.evl_encap_proto = oap->ap_ethertype;
	evh.evl_tag = (swfcl->swfcl_vlan->vlan_vid |
	    htons(swfcl->swfcl_vlan->vlan_pcp << EVL_PRIO_BITS));

	m_adj(m, ETHER_HDR_LEN);
	M_PREPEND(m, sizeof(evh), M_DONTWAIT);
	if (m == NULL)
		return (NULL);

	m_copyback(m, 0, sizeof(evh), &evh, M_NOWAIT);

	/*
	 * Update VLAN classification
	 */
	swfcl->swfcl_vlan->vlan_tpid = oap->ap_ethertype;
	swfcl->swfcl_vlan->vlan_vid = evh.evl_tag & htons(EVL_VLID_MASK);
	swfcl->swfcl_vlan->vlan_pcp = EVL_PRIOFTAG(ntohs(evh.evl_tag));

	return (m);
}


/*
 * OpenFlow protocol packet in
 */
int
swofp_action_output_controller(struct switch_softc *sc, struct mbuf *m0,
    struct swofp_pipeline_desc *swpld , uint16_t frame_max_len, uint8_t reason)
{
	struct swofp_ofs		*swofs = sc->sc_ofs;
	struct switch_flow_classify	*swfcl = swpld->swpld_swfcl;
	struct ofp_packet_in		*pin;
	struct ofp_match		*om;
	struct ofp_ox_match		*oxm;
	struct mbuf			*m;
	caddr_t				 tail;
	int				 match_len;

	if (reason != OFP_PKTIN_REASON_ACTION)
		frame_max_len = swofs->swofs_switch_config.cfg_miss_send_len;
	/*
	 * ofp_match in packet_in has only OFP_XM_T_INPORT, OFP_MX_T_META,
	 * OFP_XM_NXMT_TUNNEL_IPV{4|6}_{SRC|DST} and OFP_MX_T_TUNNEL_ID that
	 * if exist, so ofp_match length is determined here.
	 */
	match_len = (
	    sizeof(*om) +			/* struct ofp_match */
	    (sizeof(*oxm) + sizeof(uint32_t)) +	/* OFP_MX_T_IMPORT */
	    (sizeof(*oxm) + sizeof(uint64_t))	/* OFP_MX_T_META */
	);

	if (swfcl->swfcl_tunnel) {
		/* OFP_MX_T_TUNNEL_ID */
		match_len += (sizeof(*oxm) + sizeof(uint64_t));

		/* OFP_XM_NXMT_TUNNEL_IPV{4|6}_{SRC|DST} */
		if (swfcl->swfcl_tunnel->tun_af == AF_INET)
			match_len += (sizeof(*oxm) + sizeof(uint32_t)) * 2;
		else if (swfcl->swfcl_tunnel->tun_af == AF_INET6)
			match_len += (sizeof(*oxm) +
			    sizeof(struct in6_addr)) * 2;
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		m_freem(m0);
		return (ENOBUFS);
	}
	if ((sizeof(*pin) + match_len) >= MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			m_freem(m0);
			return (ENOBUFS);
		}
	}

	pin = mtod(m, struct ofp_packet_in *);
	memset(pin, 0, sizeof(*pin));

	pin->pin_oh.oh_version = OFP_V_1_3;
	pin->pin_oh.oh_type = OFP_T_PACKET_IN;
	pin->pin_oh.oh_xid = htonl(swofs->swofs_xidnxt++);

	pin->pin_buffer_id = htonl(OFP_PKTOUT_NO_BUFFER);
	pin->pin_table_id = swpld->swpld_table_id;
	pin->pin_cookie = swpld->swpld_cookie;
	pin->pin_reason = reason;

	if (frame_max_len) {
		/*
		 * The switch should only truncate packets if it implements
		 * buffering or the controller might end up sending PACKET_OUT
		 * responses with truncated packets that will eventually end
		 * up on the network.
		 */
		if (frame_max_len < m0->m_pkthdr.len) {
			m_freem(m);
			m_freem(m0);
			return (EMSGSIZE);
		}
		pin->pin_total_len = htons(m0->m_pkthdr.len);
	}

	/*
	 * It's ensured continuous memory space between ofp_mach space
	 */
	om = &pin->pin_match;
	swofp_ox_match_put_start(om);
	swofp_ox_match_put_uint32(om, OFP_XM_T_IN_PORT, swfcl->swfcl_in_port);
	swofp_ox_match_put_uint64(om, OFP_XM_T_META, swpld->swpld_metadata);
	if (swfcl->swfcl_tunnel) {
		swofp_ox_match_put_uint64(om, OFP_XM_T_TUNNEL_ID,
		    be64toh(swfcl->swfcl_tunnel->tun_key));

		if (swfcl->swfcl_tunnel->tun_af == AF_INET) {
			swofp_nx_match_put(om, OFP_XM_NXMT_TUNNEL_IPV4_SRC,
			    sizeof(uint32_t),
			    (caddr_t)&swfcl->swfcl_tunnel->tun_ipv4_src);
			swofp_nx_match_put(om, OFP_XM_NXMT_TUNNEL_IPV4_DST,
			    sizeof(uint32_t),
			    (caddr_t)&swfcl->swfcl_tunnel->tun_ipv4_dst);
		} else if (swfcl->swfcl_tunnel->tun_af == AF_INET6) {
			swofp_nx_match_put(om, OFP_XM_NXMT_TUNNEL_IPV6_SRC,
			    sizeof(struct in6_addr),
			    (caddr_t)&swfcl->swfcl_tunnel->tun_ipv6_src);
			swofp_nx_match_put(om, OFP_XM_NXMT_TUNNEL_IPV6_DST,
			    sizeof(struct in6_addr),
			    (caddr_t)&swfcl->swfcl_tunnel->tun_ipv6_dst);
		}
	}
	match_len = swofp_ox_match_put_end(om); /* match_len include padding */

	/*
	 * Adjust alignment for Ethernet header
	 */
	tail = (caddr_t)pin +
	    offsetof(struct ofp_packet_in, pin_match) + match_len;
	memset(tail, 0, ETHER_ALIGN);

	m->m_len = m->m_pkthdr.len =
	    offsetof(struct ofp_packet_in, pin_match) + match_len + ETHER_ALIGN;

	pin->pin_oh.oh_length =
	    htons(m->m_pkthdr.len + ntohs(pin->pin_total_len));

	if (frame_max_len) {
		/* m_cat() doesn't update the m_pkthdr.len */
		m_cat(m, m0);
		m->m_pkthdr.len += ntohs(pin->pin_total_len);
	}

	(void)swofp_output(sc, m);

	return (0);
}

struct mbuf *
swofp_action_output(struct switch_softc *sc, struct mbuf *m,
    struct swofp_pipeline_desc *swpld, struct ofp_action_header *oah)
{
	struct ofp_action_output	*oao;
	struct switch_port		*swpo;
	struct mbuf			*mc;

	m->m_pkthdr.csum_flags = 0;

	if ((m = swofp_apply_set_field(m, swpld)) == NULL)
		return (NULL);

	oao = (struct ofp_action_output *)oah;

	switch (ntohl(oao->ao_port)) {
	case OFP_PORT_CONTROLLER:
	case OFP_PORT_FLOWTABLE:
		if ((mc = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT)) == NULL) {
			m_freem(m);
			return (NULL);
		}
	}

	switch (ntohl(oao->ao_port)) {
	case OFP_PORT_CONTROLLER:
		swofp_action_output_controller(sc, mc, swpld,
		    ntohs(oao->ao_max_len), swpld->swpld_tablemiss ?
		    OFP_PKTIN_REASON_NO_MATCH : OFP_PKTIN_REASON_ACTION);
		break;
	case OFP_PORT_FLOWTABLE:
		swofp_forward_ofs(sc, swpld->swpld_swfcl, mc);
		break;
	case OFP_PORT_FLOOD:
		TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
			if (swpo->swpo_port_no !=
			    swpld->swpld_swfcl->swfcl_in_port)
				TAILQ_INSERT_HEAD(&swpld->swpld_fwdp_q, swpo,
				    swpo_fwdp_next);
		}
		break;
	case OFP_PORT_INPUT:
		TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
			if (swpo->swpo_port_no ==
			    swpld->swpld_swfcl->swfcl_in_port) {
				TAILQ_INSERT_HEAD(&swpld->swpld_fwdp_q, swpo,
				    swpo_fwdp_next);
				break;
			}
		}
		break;
	case OFP_PORT_NORMAL:
	case OFP_PORT_ALL:
	case OFP_PORT_ANY:
		/* no support yet */
		break;
	case OFP_PORT_LOCAL:
		TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
			if (swpo->swpo_flags & IFBIF_LOCAL) {
				TAILQ_INSERT_HEAD(&swpld->swpld_fwdp_q, swpo,
				    swpo_fwdp_next);
				break;
			}
		}
		break;
	default:
		TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
			if (swpo->swpo_port_no == ntohl(oao->ao_port))
				TAILQ_INSERT_HEAD(&swpld->swpld_fwdp_q, swpo,
				    swpo_fwdp_next);
		}
		break;
	}

	if (!TAILQ_EMPTY(&swpld->swpld_fwdp_q)) {
		if ((mc = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT)) == NULL) {
			m_freem(m);
			return (NULL);
		}
		switch_port_egress(sc, &swpld->swpld_fwdp_q, mc);
		TAILQ_INIT(&swpld->swpld_fwdp_q);
	}

	return (m);
}


struct mbuf *
swofp_action_group_all(struct switch_softc *sc, struct mbuf *m,
    struct swofp_pipeline_desc *swpld, struct swofp_group_entry *swge)
{
	struct ofp_bucket		*bucket;
	struct ofp_action_header	*ah;
	int				 actions_len;
	struct swofp_pipeline_desc	*clean_swpld = NULL;
	struct switch_flow_classify	 swfcl;
	struct mbuf			*n;

	/* Don't do anything if we don't have buckets. */
	if (swge->swge_buckets == NULL)
		return (m);

	OFP_BUCKETS_FOREACH(swge->swge_buckets,
	    swge->swge_buckets_len, bucket) {
		if (switch_swfcl_dup(swpld->swpld_swfcl, &swfcl) != 0)
			goto failed;

		clean_swpld = swofp_pipeline_desc_create(&swfcl);
		if (clean_swpld == NULL)
			goto failed;

		if ((n = m_dup_pkt(m,  ETHER_ALIGN, M_NOWAIT)) == NULL)
			goto failed;

		actions_len = (ntohs(bucket->b_len) -
		    (offsetof(struct ofp_bucket, b_actions)));

		OFP_ACTION_FOREACH(bucket->b_actions, actions_len, ah) {
			n = swofp_execute_action(sc, n, clean_swpld, ah);
			if (n == NULL)
				goto failed;
		}

		m_freem(n);
		swofp_pipeline_desc_destroy(clean_swpld);
		clean_swpld = NULL;
		switch_swfcl_free(&swfcl);
	}

	return (m);

 failed:
	m_freem(m);
	if (clean_swpld)
		swofp_pipeline_desc_destroy(clean_swpld);
	return (NULL);
}

struct mbuf *
swofp_action_group(struct switch_softc *sc, struct mbuf *m,
    struct swofp_pipeline_desc *swpld, struct ofp_action_header *oah)
{
	struct ofp_action_group		*oag;
	struct swofp_group_entry	*swge;

	oag = (struct ofp_action_group *)oah;

	swge = swofp_group_entry_lookup(sc, ntohl(oag->ag_group_id));
	if (swge == NULL) {
		m_freem(m);
		return (NULL);
	}

	swge->swge_packet_count++;
	swge->swge_byte_count += m->m_pkthdr.len;

	switch (swge->swge_type) {
	case OFP_GROUP_T_ALL:
		return swofp_action_group_all(sc, m, swpld, swge);
	case OFP_GROUP_T_INDIRECT:
	case OFP_GROUP_T_FAST_FAILOVER:
	case OFP_GROUP_T_SELECT:
		m_freem(m);
		return (NULL);
	}

	return (m);
}

struct mbuf *
swofp_apply_set_field_udp(struct mbuf *m, int off,
    struct switch_flow_classify *pre_swfcl, struct switch_flow_classify *swfcl)
{
	struct udphdr *uh;

	if (m->m_len < (off + sizeof(*uh)) &&
	    (m = m_pullup(m, off + sizeof(*uh))) == NULL)
		return NULL;

	uh = (struct udphdr *)((m)->m_data + off);

	if (pre_swfcl->swfcl_udp) {
		uh->uh_sport = pre_swfcl->swfcl_udp->udp_src;
		uh->uh_dport = pre_swfcl->swfcl_udp->udp_dst;
		memcpy(swfcl->swfcl_udp, pre_swfcl->swfcl_udp,
		    sizeof(*swfcl->swfcl_udp));
	}

	return (m);
}

struct mbuf *
swofp_apply_set_field_tcp(struct mbuf *m, int off,
    struct switch_flow_classify *pre_swfcl, struct switch_flow_classify *swfcl)
{
	struct tcphdr *th;

	if (m->m_len < (off + sizeof(*th)) &&
	    (m = m_pullup(m, off + sizeof(*th))) == NULL)
		return NULL;

	th = (struct tcphdr *)((m)->m_data + off);

	if (pre_swfcl->swfcl_tcp) {
		th->th_sport = pre_swfcl->swfcl_tcp->tcp_src;
		th->th_dport = pre_swfcl->swfcl_tcp->tcp_dst;

		memcpy(swfcl->swfcl_tcp, pre_swfcl->swfcl_tcp,
		    sizeof(*swfcl->swfcl_tcp));
	}

	return (m);
}

#ifdef INET6
struct mbuf *
swofp_apply_set_field_nd6(struct mbuf *m, int off,
    struct switch_flow_classify *pre_swfcl, struct switch_flow_classify *swfcl)
{
	struct icmp6_hdr		*icmp6;
	struct nd_neighbor_advert	*nd_na;
	struct nd_neighbor_solicit	*nd_ns;
	union nd_opts			 ndopts;
	int				 icmp6len = m->m_pkthdr.len - off;
	int				 lladdrlen;
	uint8_t				*lladdr;

	if (pre_swfcl->swfcl_nd6 == NULL)
		return (m);

	IP6_EXTHDR_GET(icmp6, struct icmp6_hdr *, m, off, sizeof(*icmp6));
	if (icmp6 == NULL)
		goto failed;

	switch (icmp6->icmp6_type) {
	case ND_NEIGHBOR_ADVERT:
		if (icmp6len < sizeof(struct nd_neighbor_advert))
			goto failed;
		break;
	case ND_NEIGHBOR_SOLICIT:
		if (icmp6len < sizeof(struct nd_neighbor_solicit))
			goto failed;
		break;
	}

	switch (icmp6->icmp6_type) {
	case ND_NEIGHBOR_ADVERT:
		IP6_EXTHDR_GET(nd_na, struct nd_neighbor_advert *, m,
		    off, icmp6len);
		if (nd_na == NULL)
			goto failed;

		nd_na->nd_na_target = pre_swfcl->swfcl_nd6->nd6_target;
		icmp6len -= sizeof(*nd_na);
		nd6_option_init(nd_na + 1, icmp6len, &ndopts);
		if (nd6_options(&ndopts) < 0)
			goto failed;

		if (!ndopts.nd_opts_tgt_lladdr)
			goto failed;

		lladdr = (char *)(ndopts.nd_opts_tgt_lladdr + 1);
		lladdrlen = (ndopts.nd_opts_tgt_lladdr->nd_opt_len << 3) - 2;

		/* switch(4) only supports Ethernet interfaces */
		if (lladdrlen != ETHER_ADDR_LEN)
			goto failed;

		memcpy(lladdr, pre_swfcl->swfcl_nd6->nd6_lladdr,
		    ETHER_ADDR_LEN);
		break;
	case ND_NEIGHBOR_SOLICIT:
		IP6_EXTHDR_GET(nd_ns, struct nd_neighbor_solicit *, m,
		    off, icmp6len);
		if (nd_ns == NULL)
			goto failed;

		nd_ns->nd_ns_target = pre_swfcl->swfcl_nd6->nd6_target;
		icmp6len -= sizeof(*nd_ns);

		nd6_option_init(nd_ns + 1, icmp6len, &ndopts);
		if (nd6_options(&ndopts) < 0)
			goto failed;

		if (!ndopts.nd_opts_src_lladdr)
			goto failed;

		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = (ndopts.nd_opts_src_lladdr->nd_opt_len << 3) - 2;

		/* switch(4) only supports Ethernet interfaces */
		if (lladdrlen != ETHER_ADDR_LEN)
			goto failed;
		memcpy(lladdr, pre_swfcl->swfcl_nd6->nd6_lladdr,
		    ETHER_ADDR_LEN);
		break;
	}

	memcpy(swfcl->swfcl_nd6, pre_swfcl->swfcl_nd6,
	    sizeof(*swfcl->swfcl_nd6));

	return (m);

 failed:
	m_freem(m);
	return (NULL);
}

struct mbuf *
swofp_apply_set_field_icmpv6(struct mbuf *m, int off,
    struct switch_flow_classify *pre_swfcl, struct switch_flow_classify *swfcl)
{
	struct icmp6_hdr	*icmp6;

	IP6_EXTHDR_GET(icmp6, struct icmp6_hdr *, m, off, sizeof(*icmp6));
	if (icmp6 == NULL)
		return (NULL); /* m was already freed */

	if (pre_swfcl->swfcl_icmpv6) {
		icmp6->icmp6_type = pre_swfcl->swfcl_icmpv6->icmpv6_type;
		icmp6->icmp6_code = pre_swfcl->swfcl_icmpv6->icmpv6_code;

		memcpy(swfcl->swfcl_icmpv6, pre_swfcl->swfcl_icmpv6,
		    sizeof(*swfcl->swfcl_icmpv6));
	}

	m->m_pkthdr.csum_flags |= M_ICMP_CSUM_OUT;

	switch (icmp6->icmp6_type) {
	case ND_NEIGHBOR_ADVERT:
	case ND_NEIGHBOR_SOLICIT:
		return swofp_apply_set_field_nd6(m, off, pre_swfcl, swfcl);
	}

	return (m);
}
#endif /* INET6 */

struct mbuf *
swofp_apply_set_field_icmpv4(struct mbuf *m, int off,
    struct switch_flow_classify *pre_swfcl, struct switch_flow_classify *swfcl)
{
	struct icmp *icmp;

	if (m->m_len < (off + ICMP_MINLEN) &&
	    (m = m_pullup(m, (off + ICMP_MINLEN))) == NULL)
		return NULL;

	icmp = (struct icmp *)((m)->m_data + off);

	if (pre_swfcl->swfcl_icmpv4) {
		icmp->icmp_type = pre_swfcl->swfcl_icmpv4->icmpv4_type;
		icmp->icmp_code = pre_swfcl->swfcl_icmpv4->icmpv4_code;

		memcpy(swfcl->swfcl_icmpv4, pre_swfcl->swfcl_icmpv4,
		    sizeof(*swfcl->swfcl_icmpv4));
	}

	m->m_pkthdr.csum_flags |= M_ICMP_CSUM_OUT;

	return (m);
}

#ifdef INET6
struct mbuf *
swofp_apply_set_field_ipv6(struct mbuf *m, int off,
    struct switch_flow_classify *pre_swfcl, struct switch_flow_classify *swfcl)
{
	struct ip6_hdr	*ip6;
	int		 hlen;
	/* max size is 802.1ad header size */
	u_char		 eh_bk[sizeof(struct ether_vlan_header) + EVL_ENCAPLEN];

	if (m->m_len < (off + sizeof(*ip6)) &&
	    (m = m_pullup(m, off + sizeof(*ip6))) == NULL)
		return (NULL);

	ip6 = (struct ip6_hdr *)(mtod(m, caddr_t) + off);

	if (pre_swfcl->swfcl_ipv6) {
		/* set version, class and flow label at once */
		ip6->ip6_flow =  (IPV6_VERSION |
		    (pre_swfcl->swfcl_ipv6->ipv6_flow_label &
		    IPV6_FLOWLABEL_MASK) |
		    htonl(pre_swfcl->swfcl_ipv6->ipv6_tclass << 20));
		ip6->ip6_hlim = pre_swfcl->swfcl_ipv6->ipv6_hlimit;
		ip6->ip6_nxt = pre_swfcl->swfcl_ipv6->ipv6_nxt;

		ip6->ip6_src = pre_swfcl->swfcl_ipv6->ipv6_src;
		ip6->ip6_dst = pre_swfcl->swfcl_ipv6->ipv6_dst;

		memcpy(pre_swfcl->swfcl_ipv6, swfcl->swfcl_ipv6,
		    sizeof(*pre_swfcl->swfcl_ipv6));
	}

	hlen = sizeof(*ip6);

	switch (swfcl->swfcl_ipv6->ipv6_nxt) {
	case IPPROTO_UDP:
		m = swofp_apply_set_field_udp(m, (off + hlen),
		    pre_swfcl, swfcl);
		if (m == NULL)
			return (NULL);
		m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;
		break;
	case IPPROTO_TCP:
		m =  swofp_apply_set_field_tcp(m, (off + hlen),
		    pre_swfcl, swfcl);
		if (m == NULL)
			return (NULL);
		m->m_pkthdr.csum_flags |= M_TCP_CSUM_OUT;
		break;
	case IPPROTO_ICMPV6:
		m =  swofp_apply_set_field_icmpv6(m, (off + hlen),
		    pre_swfcl, swfcl);
		if (m == NULL)
			return (NULL);
		break;
	}

	/*
	 * Recalculate checksums:
	 * It doesn't use H/W offload because doesn't know to send a frame
	 * from which interface.
	 */
	m_copydata(m, 0, off, eh_bk);
	m_adj(m, off);

	if (m->m_len < hlen && ((m = m_pullup(m, hlen)) == NULL))
		return (NULL);

	in6_proto_cksum_out(m, NULL);

	M_PREPEND(m, off, M_DONTWAIT);
	if (m == NULL)
		return (NULL);

	m_copyback(m, 0, off, eh_bk, M_DONTWAIT);

	return (m);
}
#endif /* INET6 */

struct mbuf *
swofp_apply_set_field_ipv4(struct mbuf *m, int off,
    struct switch_flow_classify *pre_swfcl, struct switch_flow_classify *swfcl)
{
	struct ip	*ip;
	int		 hlen;
	/* max size is 802.1ad header size */
	u_char		 eh_bk[sizeof(struct ether_vlan_header) + EVL_ENCAPLEN];

	if (m->m_len < (off + sizeof(*ip)) &&
	    (m = m_pullup(m, off + sizeof(*ip))) == NULL)
		return (NULL);

	ip = (struct ip *)(mtod(m, caddr_t) + off);

	if (pre_swfcl->swfcl_ipv4) {
		ip->ip_p = pre_swfcl->swfcl_ipv4->ipv4_proto;
		ip->ip_tos = pre_swfcl->swfcl_ipv4->ipv4_tos;

		memcpy(&ip->ip_src.s_addr, &pre_swfcl->swfcl_ipv4->ipv4_src,
		    sizeof(uint32_t));
		memcpy(&ip->ip_dst.s_addr, &pre_swfcl->swfcl_ipv4->ipv4_dst,
		    sizeof(uint32_t));

		memcpy(pre_swfcl->swfcl_ipv4, swfcl->swfcl_ipv4,
		    sizeof(*pre_swfcl->swfcl_ipv4));
	}

	hlen = (ip->ip_hl << 2);

	switch (swfcl->swfcl_ipv4->ipv4_proto) {
	case IPPROTO_UDP:
		m = swofp_apply_set_field_udp(m, (off + hlen),
		    pre_swfcl, swfcl);
		if (m == NULL)
			return (NULL);
		m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;
		break;
	case IPPROTO_TCP:
		m =  swofp_apply_set_field_tcp(m, (off + hlen),
		    pre_swfcl, swfcl);
		if (m == NULL)
			return (NULL);
		m->m_pkthdr.csum_flags |= M_TCP_CSUM_OUT;
		break;
	case IPPROTO_ICMP:
		m =  swofp_apply_set_field_icmpv4(m, (off + hlen),
		    pre_swfcl, swfcl);
		if (m == NULL)
			return (NULL);
		break;
	}

	/*
	 * Recalculate checksums:
	 * It doesn't use H/W offload because doesn't know to send a frame
	 * from which interface.
	 */
	m_copydata(m, 0, off, eh_bk);
	m_adj(m, off);

	if (m->m_len < hlen && ((m = m_pullup(m, hlen)) == NULL))
		return (NULL);
	ip = mtod(m, struct ip *);

	ip->ip_sum = 0;
	in_proto_cksum_out(m, NULL);
	ip->ip_sum = in_cksum(m, hlen);

	M_PREPEND(m, off, M_DONTWAIT);
	if (m == NULL)
		return (NULL);

	m_copyback(m, 0, off, eh_bk, M_DONTWAIT);

	return (m);
}

struct mbuf *
swofp_apply_set_field_arp( struct mbuf *m, int off,
    struct switch_flow_classify *pre_swfcl, struct switch_flow_classify *swfcl)
{
	struct ether_arp *ea;

	if (m->m_len < (off + sizeof(*ea)) &&
	    (m = m_pullup(m, off + sizeof(*ea))) == NULL)
		return (NULL);

	ea = (struct ether_arp *)((m)->m_data + off);

	if (pre_swfcl->swfcl_arp) {
		ea->arp_op = pre_swfcl->swfcl_arp->_arp_op;

		memcpy(&ea->arp_sha, pre_swfcl->swfcl_arp->arp_sha,
		    ETHER_ADDR_LEN);
		memcpy(&ea->arp_tha, pre_swfcl->swfcl_arp->arp_tha,
		    ETHER_ADDR_LEN);
		memcpy(&ea->arp_spa, &pre_swfcl->swfcl_arp->arp_sip,
		    sizeof(uint32_t));
		memcpy(&ea->arp_tpa, &pre_swfcl->swfcl_arp->arp_tip,
		    sizeof(uint32_t));
		memcpy(swfcl->swfcl_arp, pre_swfcl->swfcl_arp,
		    sizeof(*swfcl->swfcl_arp));
	}

	return (m);
}

struct mbuf *
swofp_apply_set_field_ether(struct mbuf *m, int off,
    struct switch_flow_classify *pre_swfcl, struct switch_flow_classify *swfcl)
{
	struct ether_header		*eh;
	struct ether_vlan_header	*evl = NULL;
	uint16_t			*ether_type;

	m = swofp_expand_8021q_tag(m);
	if (m == NULL)
		return (NULL);

	/*
	 * pullup to maximum size QinQ header
	 */
	if ((m = m_pullup(m, (sizeof(*evl) + EVL_ENCAPLEN))) == NULL)
		return (NULL);

	eh = mtod(m, struct ether_header *);

	switch (ntohs(eh->ether_type)) {
	case ETHERTYPE_QINQ:
		off = EVL_ENCAPLEN + sizeof(struct ether_vlan_header);
		evl = mtod(m, struct ether_vlan_header *);
		ether_type = (uint16_t *)(mtod(m, caddr_t) + EVL_ENCAPLEN +
		    offsetof(struct ether_vlan_header, evl_proto));
		break;
	case ETHERTYPE_VLAN:
		off = sizeof(struct ether_vlan_header);
		evl = mtod(m, struct ether_vlan_header *);
		ether_type = &evl->evl_proto;
		break;
	default:
		off = sizeof(struct ether_header);
		ether_type = &eh->ether_type;
		break;
	}

	if (pre_swfcl->swfcl_vlan) {
		switch (ntohs(eh->ether_type)) {
		case ETHERTYPE_QINQ:
		case ETHERTYPE_VLAN:
			evl->evl_tag = (pre_swfcl->swfcl_vlan->vlan_vid |
			    htons(pre_swfcl->swfcl_vlan->vlan_pcp <<
			    EVL_PRIO_BITS));
			break;
		default:
			break;
		}

		/* Update the classifier if it exists. */
		if (swfcl->swfcl_vlan)
			memcpy(swfcl->swfcl_vlan, pre_swfcl->swfcl_vlan,
			    sizeof(*swfcl->swfcl_vlan));
	}

	if (pre_swfcl->swfcl_ether) {
		memcpy(eh->ether_shost, pre_swfcl->swfcl_ether->eth_src,
		    ETHER_ADDR_LEN);
		memcpy(eh->ether_dhost, pre_swfcl->swfcl_ether->eth_dst,
		    ETHER_ADDR_LEN);
		*ether_type = pre_swfcl->swfcl_ether->eth_type;
		memcpy(swfcl->swfcl_ether, pre_swfcl->swfcl_ether,
		    sizeof(*swfcl->swfcl_ether));
	}

	switch (ntohs(*ether_type)) {
	case ETHERTYPE_ARP:
		return swofp_apply_set_field_arp(m, off, pre_swfcl, swfcl);
	case ETHERTYPE_IP:
		return swofp_apply_set_field_ipv4(m, off, pre_swfcl, swfcl);
#ifdef INET6
	case ETHERTYPE_IPV6:
		return swofp_apply_set_field_ipv6(m, off, pre_swfcl, swfcl);
#endif /* INET6 */
	case ETHERTYPE_MPLS:
		/* unsupported yet */
		break;
	}

	return (m);
}

struct mbuf *
swofp_apply_set_field_tunnel(struct mbuf *m, int off,
    struct switch_flow_classify *pre_swfcl, struct switch_flow_classify *swfcl)
{
	struct bridge_tunneltag	*brtag;

	if (pre_swfcl->swfcl_tunnel) {
		if ((brtag = bridge_tunneltag(m)) == NULL) {
			m_freem(m);
			return (NULL);
		}

		brtag->brtag_id = be64toh(pre_swfcl->swfcl_tunnel->tun_key);

		if (pre_swfcl->swfcl_tunnel->tun_ipv4_dst.s_addr !=
		    INADDR_ANY) {
			brtag->brtag_peer.sin.sin_family =
			    brtag->brtag_local.sin.sin_family = AF_INET;
			brtag->brtag_local.sin.sin_addr =
			    pre_swfcl->swfcl_tunnel->tun_ipv4_src;
			brtag->brtag_peer.sin.sin_addr =
			    pre_swfcl->swfcl_tunnel->tun_ipv4_dst;
		} else if (!IN6_IS_ADDR_UNSPECIFIED(
		    &pre_swfcl->swfcl_tunnel->tun_ipv6_dst)) {
			brtag->brtag_peer.sin6.sin6_family =
			    brtag->brtag_local.sin.sin_family = AF_INET6;
			brtag->brtag_local.sin6.sin6_addr =
			    pre_swfcl->swfcl_tunnel->tun_ipv6_src;
			brtag->brtag_peer.sin6.sin6_addr =
			    pre_swfcl->swfcl_tunnel->tun_ipv6_dst;
		} else {
			bridge_tunneluntag(m);
			m_freem(m);
			return (NULL);
		}

		/*
		 * It can't be used by apply-action instruction.
		 */
		if (swfcl->swfcl_tunnel) {
			memcpy(swfcl->swfcl_tunnel, pre_swfcl->swfcl_tunnel,
			    sizeof(*pre_swfcl->swfcl_tunnel));
		}
	}

	return swofp_apply_set_field_ether(m, 0, pre_swfcl, swfcl);
}

struct mbuf *
swofp_apply_set_field(struct mbuf *m, struct swofp_pipeline_desc *swpld)
{
	return swofp_apply_set_field_tunnel(m, 0,
	    &swpld->swpld_pre_swfcl, swpld->swpld_swfcl);
}

struct mbuf *
swofp_action_set_field(struct switch_softc *sc, struct mbuf *m,
    struct swofp_pipeline_desc *swpld, struct ofp_action_header *oah)
{
	struct ofp_oxm_class *oxm_handler;
	struct ofp_action_set_field *oasf;
	struct ofp_ox_match *oxm;
	struct switch_flow_classify *swfcl, *pre_swfcl;

	oasf = (struct ofp_action_set_field *)oah;
	oxm = (struct ofp_ox_match *)oasf->asf_field;

	oxm_handler = swofp_lookup_oxm_handler(oxm);
	if ((oxm_handler == NULL) ||
	    (oxm_handler->oxm_set == NULL))
		goto failed;

	swfcl = swpld->swpld_swfcl;
	pre_swfcl = &swpld->swpld_pre_swfcl;

	if (oxm->oxm_class == htons(OFP_OXM_C_NXM_1)) {
		switch (OFP_OXM_GET_FIELD(oxm)) {
		case OFP_XM_NXMT_TUNNEL_ID: /* alias OFP_XM_T_TUNNEL_ID */
		case OFP_XM_NXMT_TUNNEL_IPV4_SRC:
		case OFP_XM_NXMT_TUNNEL_IPV4_DST:
		case OFP_XM_NXMT_TUNNEL_IPV6_SRC:
		case OFP_XM_NXMT_TUNNEL_IPV6_DST:
			if (pre_swfcl->swfcl_tunnel)
				break;
			pre_swfcl->swfcl_tunnel = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_tunnel == NULL)
				goto failed;
			if (swfcl->swfcl_tunnel)
				memcpy(pre_swfcl->swfcl_tunnel,
				    swfcl->swfcl_tunnel,
				    sizeof(*swfcl->swfcl_tunnel));
			break;
		}
	} else {
		switch (OFP_OXM_GET_FIELD(oxm)) {
		case OFP_XM_T_ETH_SRC:
		case OFP_XM_T_ETH_DST:
		case OFP_XM_T_ETH_TYPE:
			if (pre_swfcl->swfcl_ether)
				break;
			pre_swfcl->swfcl_ether = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_ether == NULL)
				goto failed;
			memcpy(pre_swfcl->swfcl_ether, swfcl->swfcl_ether,
			    sizeof(*swfcl->swfcl_ether));
		break;
		case OFP_XM_T_VLAN_VID:
		case OFP_XM_T_VLAN_PCP:
			if (pre_swfcl->swfcl_vlan)
				break;
			pre_swfcl->swfcl_vlan = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_vlan == NULL)
				goto failed;
			if (swfcl->swfcl_vlan)
				memcpy(pre_swfcl->swfcl_vlan, swfcl->swfcl_vlan,
				    sizeof(*swfcl->swfcl_vlan));
			break;
		case OFP_XM_T_ARP_SHA:
		case OFP_XM_T_ARP_THA:
		case OFP_XM_T_ARP_SPA:
		case OFP_XM_T_ARP_TPA:
		case OFP_XM_T_ARP_OP:
			if (pre_swfcl->swfcl_arp)
				break;
			pre_swfcl->swfcl_arp = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (swfcl->swfcl_arp == NULL)
				goto failed;
			memcpy(pre_swfcl->swfcl_arp, swfcl->swfcl_arp,
			    sizeof(*swfcl->swfcl_arp));
			break;
		case OFP_XM_T_IP_DSCP:
		case OFP_XM_T_IP_ECN:
		case OFP_XM_T_IP_PROTO:
			if (swfcl->swfcl_ipv4) {
				if (pre_swfcl->swfcl_ipv4)
					break;
				pre_swfcl->swfcl_ipv4 = pool_get(&swfcl_pool,
				    PR_NOWAIT|PR_ZERO);
				if (pre_swfcl->swfcl_ipv4 == NULL)
					goto failed;
				memcpy(pre_swfcl->swfcl_ipv4, swfcl->swfcl_ipv4,
				    sizeof(*swfcl->swfcl_ipv4));
			} else if (swfcl->swfcl_ipv6) {
				if (pre_swfcl->swfcl_ipv6)
					break;
				pre_swfcl->swfcl_ipv6 = pool_get(&swfcl_pool,
				    PR_NOWAIT|PR_ZERO);
				if (pre_swfcl->swfcl_ipv6 == NULL)
					goto failed;
				memcpy(pre_swfcl->swfcl_ipv6, swfcl->swfcl_ipv6,
				    sizeof(*swfcl->swfcl_ipv6));
			}
			break;
		case OFP_XM_T_IPV4_SRC:
		case OFP_XM_T_IPV4_DST:
			if (pre_swfcl->swfcl_ipv4)
				break;
			pre_swfcl->swfcl_ipv4 =  pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_ipv4 == NULL)
				goto failed;
			memcpy(pre_swfcl->swfcl_ipv4, swfcl->swfcl_ipv4,
			    sizeof(*swfcl->swfcl_ipv4));
			break;
		case OFP_XM_T_IPV6_SRC:
		case OFP_XM_T_IPV6_DST:
		case OFP_XM_T_IPV6_FLABEL:
			if (pre_swfcl->swfcl_ipv6)
				break;
			pre_swfcl->swfcl_ipv6 = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_ipv6 == NULL)
				goto failed;
			memcpy(pre_swfcl->swfcl_ipv6, swfcl->swfcl_ipv6,
			    sizeof(*swfcl->swfcl_ipv6));
			break;
		case OFP_XM_T_UDP_SRC:
		case OFP_XM_T_UDP_DST:
			if (pre_swfcl->swfcl_udp)
				break;
			pre_swfcl->swfcl_udp = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_udp == NULL)
				goto failed;
			memcpy(pre_swfcl->swfcl_udp, swfcl->swfcl_udp,
			    sizeof(*swfcl->swfcl_udp));
			break;
		case OFP_XM_T_TCP_SRC:
		case OFP_XM_T_TCP_DST:
			if (pre_swfcl->swfcl_tcp)
				break;
			pre_swfcl->swfcl_tcp = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_tcp == NULL)
				goto failed;
			memcpy(pre_swfcl->swfcl_tcp, swfcl->swfcl_tcp,
			    sizeof(*swfcl->swfcl_tcp));
			break;
		case OFP_XM_T_ICMPV4_CODE:
		case OFP_XM_T_ICMPV4_TYPE:
			if (pre_swfcl->swfcl_icmpv4)
				break;
			pre_swfcl->swfcl_icmpv4 = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_icmpv4 == NULL)
				goto failed;
			memcpy(pre_swfcl->swfcl_icmpv4, swfcl->swfcl_icmpv4,
			    sizeof(*swfcl->swfcl_icmpv4));
			break;
		case OFP_XM_T_ICMPV6_CODE:
		case OFP_XM_T_ICMPV6_TYPE:
			if (pre_swfcl->swfcl_icmpv6)
				break;
			pre_swfcl->swfcl_icmpv6 = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_icmpv6 == NULL)
				goto failed;
			memcpy(pre_swfcl->swfcl_icmpv6, swfcl->swfcl_icmpv6,
			    sizeof(*swfcl->swfcl_icmpv6));
			break;
		case OFP_XM_T_IPV6_ND_SLL:
		case OFP_XM_T_IPV6_ND_TLL:
		case OFP_XM_T_IPV6_ND_TARGET:
			if (pre_swfcl->swfcl_nd6)
				break;
			pre_swfcl->swfcl_nd6 = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_nd6 == NULL)
				goto failed;
			memcpy(pre_swfcl->swfcl_nd6, swfcl->swfcl_nd6,
			    sizeof(*swfcl->swfcl_nd6));
			break;
		case OFP_XM_T_TUNNEL_ID: /* alias OFP_XM_T_TUNNEL_ID */
			if (pre_swfcl->swfcl_tunnel)
				break;
			pre_swfcl->swfcl_tunnel = pool_get(&swfcl_pool,
			    PR_NOWAIT|PR_ZERO);
			if (pre_swfcl->swfcl_tunnel == NULL)
				goto failed;
			if (swfcl->swfcl_tunnel)
				memcpy(pre_swfcl->swfcl_tunnel,
				    swfcl->swfcl_tunnel,
				    sizeof(*swfcl->swfcl_tunnel));
			break;
		}
	}

	if (oxm_handler->oxm_set(pre_swfcl, oxm))
		goto failed;

	return (m);
 failed:
	m_freem(m);
	return (NULL);
}

struct mbuf *
swofp_execute_action(struct switch_softc *sc, struct mbuf *m,
    struct swofp_pipeline_desc *swpld, struct ofp_action_header *oah)
{
	struct ofp_action_handler	*handler;

	handler = swofp_lookup_action_handler(ntohs(oah->ah_type));
	if ((handler == NULL) || (handler->action == NULL)) {
		DPRINTF(sc, "unknown action (type %d)\n",
		    ntohs(oah->ah_type));
		m_freem(m);
		return (NULL);
	}

	m = handler->action(sc, m, swpld, oah);
	if (m == NULL)
		return (NULL);

	return (m);
}

struct mbuf *
swofp_execute_action_set_field(struct switch_softc *sc, struct mbuf *m,
    struct swofp_pipeline_desc *swpld, struct ofp_action_header *oah)
{
	struct ofp_action_header	**set_fields;
	int i;

	set_fields = (struct ofp_action_header **)oah;

	for (i = 0; i < OFP_XM_T_MAX; i++) {
		if (set_fields[i] == NULL)
			continue;

		m = swofp_execute_action(sc, m, swpld, set_fields[i]);
		if (m == NULL)
			return (NULL);
	}

	return (m);
}


struct mbuf *
swofp_execute_action_set(struct switch_softc *sc, struct mbuf *m,
    struct swofp_pipeline_desc *swpld)
{
	struct swofp_action_set	*swas;
	int			 i;

	TAILQ_INIT(&swpld->swpld_fwdp_q);
	swas = swpld->swpld_action_set;
	swpld->swpld_swfcl->swfcl_cookie = UINT64_MAX;

	for (i = 0 ; i < nitems(ofp_action_handlers); i++) {
		if (swas[i].swas_action == NULL)
			continue;

		if (swas[i].swas_type == OFP_ACTION_SET_FIELD)
			m = swofp_execute_action_set_field(sc, m, swpld,
			    swas[i].swas_action);
		else
			m = swofp_execute_action(sc, m, swpld,
			    swas[i].swas_action);

		if (m == NULL)
			break;
	}

	m_freem(m);

	return (NULL);
}

struct mbuf *
swofp_apply_actions(struct switch_softc *sc, struct mbuf *m,
    struct swofp_pipeline_desc *swpld, struct ofp_instruction_actions *oia)
{
	struct ofp_action_header	*oah;

	TAILQ_INIT(&swpld->swpld_fwdp_q);

	OFP_I_ACTIONS_FOREACH(oia, oah) {
		m = swofp_execute_action(sc, m, swpld, oah);
		if (m == NULL)
			return (NULL);
	}

	return (m);
}

struct swofp_action_set *
swofp_lookup_action_set(struct swofp_pipeline_desc *swpld, uint16_t type)
{
	int	i;

	for (i = 0; i < nitems(ofp_action_handlers); i ++) {
		if (swpld->swpld_action_set[i].swas_type == type)
			return (&swpld->swpld_action_set[i]);
	}

	return (NULL);
}

void
swofp_write_actions_set_field(struct swofp_action_set *swas,
    struct ofp_action_header *oah)
{
	struct ofp_action_header	**set_fields;
	struct ofp_action_set_field	*oasf;
	struct ofp_ox_match		*oxm;

	set_fields = (struct ofp_action_header **)swas->swas_action;

	oasf = (struct ofp_action_set_field *)oah;
	oxm = (struct ofp_ox_match *)oasf->asf_field;

	set_fields[OFP_OXM_GET_FIELD(oxm)] = oah;
}

int
swofp_write_actions(struct ofp_instruction_actions *oia,
    struct swofp_pipeline_desc *swpld)
{
	struct swofp_action_set		*swas;
	struct ofp_action_header	*oah;

	OFP_I_ACTIONS_FOREACH(oia, oah) {
		swas = swofp_lookup_action_set(swpld, ntohs(oah->ah_type));
		if (swas == NULL)
			return (ENOENT);

		if (ntohs(oah->ah_type) == OFP_ACTION_SET_FIELD)
			swofp_write_actions_set_field(swas, oah);
		else
			swas->swas_action = oah;
	}

	return (0);
}

void
swofp_clear_actions_set_field(struct swofp_action_set *swas,
    struct ofp_action_header *oah)
{
	struct ofp_action_header	**set_fields;
	struct ofp_action_set_field	*oasf;
	struct ofp_ox_match		*oxm;

	set_fields = (struct ofp_action_header **)swas->swas_action;

	oasf = (struct ofp_action_set_field *)oah;
	oxm = (struct ofp_ox_match *)oasf->asf_field;

	set_fields[OFP_OXM_GET_FIELD(oxm)] = NULL;
}

int
swofp_clear_actions(struct ofp_instruction_actions *oia,
    struct swofp_pipeline_desc *swpld)
{
	struct swofp_action_set		*swas;
	struct ofp_action_header	*oah;

	OFP_I_ACTIONS_FOREACH(oia, oah) {
		swas = swofp_lookup_action_set(swpld, ntohs(oah->ah_type));
		if (swas == NULL)
			return (ENOENT);

		if (ntohs(oah->ah_type) == OFP_ACTION_SET_FIELD)
			swofp_clear_actions_set_field(swas, oah);
		else
			swas->swas_action = NULL;
	}

	return (0);
}

void
swofp_write_metadata(struct ofp_instruction_write_metadata *iowm,
    struct swofp_pipeline_desc *swpld)
{
	uint64_t val, mask;

	val = iowm->iwm_metadata;
	mask = iowm->iwm_metadata_mask;

	swpld->swpld_metadata = (val & mask);
}

void
swofp_forward_ofs(struct switch_softc *sc, struct switch_flow_classify *swfcl,
    struct mbuf *m)
{
	struct swofp_ofs		*ofs = sc->sc_ofs;
	struct swofp_flow_entry		*swfe;
	struct swofp_flow_table		*swft;
	struct swofp_pipeline_desc	*swpld;
	int				 error;
	uint8_t				 next_table_id = 0;

	swpld = swofp_pipeline_desc_create(swfcl);
	if (swpld == NULL) {
		m_freem(m);
		return;
	}

	TAILQ_FOREACH(swft, &ofs->swofs_table_list, swft_table_next) {
		if (swft->swft_table_id != next_table_id)
			continue;

		/* XXX
		 * The metadata is pipeline parameters but it uses same match
		 * framework for matching so it is copy to flow classify.
		 */
		swpld->swpld_swfcl->swfcl_metadata = swpld->swpld_metadata;

		if ((swfe = swofp_flow_lookup(swft,
		    swpld->swpld_swfcl)) == NULL)
			break;

		/* Set pipeline parameters */
		swpld->swpld_cookie = swfe->swfe_cookie;
		swpld->swpld_table_id = swft->swft_table_id;
		swpld->swpld_tablemiss = swfe->swfe_tablemiss;

		/* Update statistics */
		nanouptime(&swfe->swfe_idle_time);
		swfe->swfe_packet_cnt++;
		swfe->swfe_byte_cnt += m->m_pkthdr.len;

		if (swfe->swfe_meter) {
			/* TODO: Here is meter instruction */
		}

		if (swfe->swfe_apply_actions) {
			m = swofp_apply_actions(sc, m, swpld,
			    swfe->swfe_apply_actions);
			if (m == NULL)
				goto out;
		}

		if (swfe->swfe_clear_actions) {
			error = swofp_clear_actions(
			    swfe->swfe_clear_actions, swpld);
			if (error)
				goto out;
		}

		if (swfe->swfe_write_actions) {
			error = swofp_write_actions(
			    swfe->swfe_write_actions, swpld);
			if (error)
				goto out;
		}

		if (swfe->swfe_write_metadata)
			swofp_write_metadata(swfe->swfe_write_metadata, swpld);

		if (swfe->swfe_goto_table)
			next_table_id = swfe->swfe_goto_table->igt_table_id;
		else
			break;
	}

	m = swofp_execute_action_set(sc, m, swpld);
 out:
	m_freem(m);
	swofp_pipeline_desc_destroy(swpld);
}

int
swofp_input(struct switch_softc *sc, struct mbuf *m)
{
	struct swofp_ofs	*swofs = sc->sc_ofs;
	struct ofp_header	*oh;
	ofp_msg_handler		 handler;
	uint16_t		 ohlen;

	if (m->m_len < sizeof(*oh) &&
	    (m = m_pullup(m, sizeof(*oh))) == NULL)
		return (ENOBUFS);

	oh = mtod(m, struct ofp_header *);

	ohlen = ntohs(oh->oh_length);
	/* Validate that we have a sane header. */
	KASSERT(m->m_flags & M_PKTHDR);
	if (ohlen < sizeof(*oh) || m->m_pkthdr.len < ohlen) {
		swofp_send_error(sc, m, OFP_ERRTYPE_BAD_REQUEST,
		    OFP_ERRREQ_BAD_LEN);
		return (0);
	}

	if (m->m_len < ohlen && (m = m_pullup(m, ohlen)) == NULL)
		return (ENOBUFS);

#if NBPFILTER > 0
	if (sc->sc_ofbpf)
		switch_mtap(sc->sc_ofbpf, m, BPF_DIRECTION_IN,
		    swofs->swofs_datapath_id);
#endif

	handler = swofp_lookup_msg_handler(oh->oh_type);
	if (handler)
		(*handler)(sc, m);
	else
		swofp_send_error(sc, m, OFP_ERRTYPE_BAD_REQUEST,
		    OFP_ERRREQ_TYPE);

	return (0);
}

int
swofp_output(struct switch_softc *sc, struct mbuf *m)
{
	struct swofp_ofs	*swofs = sc->sc_ofs;

	if (sc->sc_swdev == NULL) {
		m_freem(m);
		return (ENXIO);
	}

#if NBPFILTER > 0
	if (sc->sc_ofbpf)
		switch_mtap(sc->sc_ofbpf, m, BPF_DIRECTION_OUT,
		    swofs->swofs_datapath_id);
#endif

	if (sc->sc_swdev->swdev_output(sc, m) != 0)
		return (ENOBUFS);

	return (0);
}

/*
 * OpenFlow protocol HELLO message handler
 */
int
swofp_recv_hello(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_header	*oh;

	oh = mtod(m, struct ofp_header *);

	if (oh->oh_version != OFP_V_1_3)
		swofp_send_error(sc, m,
		    OFP_ERRTYPE_HELLO_FAILED, OFP_ERRHELLO_INCOMPATIBLE);
	else
		m_freem(m);

	return (0);
}

void
swofp_send_hello(struct switch_softc *sc)
{
	struct swofp_ofs		*swofs = sc->sc_ofs;
	struct mbuf			*m;
	struct ofp_header		*oh;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;		/* XXX */

	oh = mtod(m, struct ofp_header *);

	oh->oh_version = OFP_V_1_3;
	oh->oh_type = OFP_T_HELLO;
	oh->oh_length = htons(sizeof(*oh));
	oh->oh_xid = htonl(swofs->swofs_xidnxt++);
	m->m_len = m->m_pkthdr.len = sizeof(*oh);

	(void)swofp_output(sc, m);
}

/*
 * OpenFlow protocol Error message
 */
void
swofp_send_error(struct switch_softc *sc, struct mbuf *m,
    uint16_t type, uint16_t code)
{
	struct mbuf		*n;
	struct ofp_header	*oh;
	struct ofp_error	*oe;
	int			 off;
	uint32_t		 xid;
	uint16_t		 len;

	/* Reuse mbuf from request message */
	oh = mtod(m, struct ofp_header *);

	/* Save data for the response and copy back later. */
	len = min(ntohs(oh->oh_length), OFP_ERRDATA_MAX);
	if (len < m->m_pkthdr.len)
		m_adj(m, len - m->m_pkthdr.len);
	xid = oh->oh_xid;

	if ((n = m_makespace(m, 0, sizeof(struct ofp_error), &off)) == NULL) {
		m_freem(m);
		return;
	}
	/* if skip is 0, off is also 0 */
	KASSERT(off == 0);

	oe = mtod(n, struct ofp_error *);

	oe->err_oh.oh_version = OFP_V_1_3;
	oe->err_oh.oh_type = OFP_T_ERROR;
	oe->err_oh.oh_length = htons(sizeof(struct ofp_error) + len);
	oe->err_oh.oh_xid = xid;
	oe->err_type = htons(type);
	oe->err_code = htons(code);

	(void)swofp_output(sc, m);
}


/*
 * OpenFlow protocol Echo message
 */
int
swofp_recv_echo(struct switch_softc *sc, struct mbuf *m)
{
	return swofp_send_echo(sc, m);
}

int
swofp_send_echo(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_header	*oh;

	oh = mtod(m, struct ofp_header *);
	oh->oh_type = OFP_T_ECHO_REPLY;

	return (swofp_output(sc, m));
}


/*
 * Feature request handler
 */
int
swofp_recv_features_req(struct switch_softc *sc, struct mbuf *m)
{

	struct swofp_ofs		*swofs = sc->sc_ofs;
	struct ofp_header		*oh;
	struct ofp_switch_features	 swf;

	oh = mtod(m, struct ofp_header *);

	memset(&swf, 0, sizeof(swf));
	memcpy(&swf, oh, sizeof(*oh));
	swf.swf_oh.oh_type = OFP_T_FEATURES_REPLY;
	swf.swf_oh.oh_length = htons(sizeof(swf));

	swf.swf_datapath_id = htobe64(swofs->swofs_datapath_id);
	swf.swf_nbuffers = htonl(0); /* no buffer now */
	swf.swf_ntables = OFP_TABLE_ID_MAX;
	swf.swf_aux_id = 0;
	swf.swf_capabilities = htonl(OFP_SWCAP_FLOW_STATS |
	    OFP_SWCAP_TABLE_STATS | OFP_SWCAP_PORT_STATS |
	    OFP_SWCAP_GROUP_STATS);

	m_copyback(m, 0, sizeof(swf), (caddr_t)&swf, M_WAIT);

	return (swofp_output(sc, m));
}

/*
 * Get config handler
 */
int
swofp_recv_config_req(struct switch_softc *sc, struct mbuf *m)
{
	struct swofp_ofs		*swofs = sc->sc_ofs;
	struct ofp_switch_config	 osc;

	memcpy(&osc.cfg_oh, mtod(m, caddr_t), sizeof(struct ofp_header));
	osc.cfg_oh.oh_type = OFP_T_GET_CONFIG_REPLY;
	osc.cfg_oh.oh_length = htons(sizeof(osc));

	osc.cfg_flags = htons(swofs->swofs_switch_config.cfg_flags);
	osc.cfg_miss_send_len =
	    htons(swofs->swofs_switch_config.cfg_miss_send_len);
	if (m_copyback(m, 0, sizeof(osc), &osc, M_NOWAIT)) {
		m_freem(m);
		return (-1);
	}

	return (swofp_output(sc, m));
}

/*
 * Set config handler
 */
int
swofp_recv_set_config(struct switch_softc *sc, struct mbuf *m)
{
	struct swofp_ofs		*swofs = sc->sc_ofs;
	struct ofp_switch_config	*swc;

	swc = mtod(m, struct ofp_switch_config *);
	if (ntohs(swc->cfg_oh.oh_length) < sizeof(*swc)) {
		swofp_send_error(sc, m, OFP_ERRTYPE_BAD_REQUEST,
		    OFP_ERRREQ_BAD_LEN);
		return (-1);
	}

	/*
	 * Support only "normal" fragment handle
	 */
	swofs->swofs_switch_config.cfg_flags = OFP_CONFIG_FRAG_NORMAL;
	swofs->swofs_switch_config.cfg_miss_send_len =
	    ntohs(swc->cfg_miss_send_len);

	m_freem(m);

	return (0);
}

/*
 * OpenFlow protocol FLOW REMOVE message handlers
 */
int
swofp_send_flow_removed(struct switch_softc *sc, struct swofp_flow_entry *swfe,
    uint8_t reason)
{
	struct ofp_flow_removed	*ofr;
	struct timespec		 now, duration;
	struct mbuf		*m;
	int			 match_len;

	match_len = ntohs(swfe->swfe_match->om_length);

	MGETHDR(m, M_WAITOK, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	if ((sizeof(*ofr) + match_len) >= MHLEN) {
		MCLGET(m, M_WAITOK);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return (ENOBUFS);
		}
	}

	ofr = mtod(m, struct ofp_flow_removed *);

	ofr->fr_oh.oh_version = OFP_V_1_3;
	ofr->fr_oh.oh_type = OFP_T_FLOW_REMOVED;
	ofr->fr_oh.oh_xid = htonl(sc->sc_ofs->swofs_xidnxt++);

	ofr->fr_cookie = htobe64(swfe->swfe_cookie);
	ofr->fr_priority = htons(swfe->swfe_priority);
	ofr->fr_reason = reason;
	ofr->fr_table_id = swfe->swfe_table_id;

	nanouptime(&now);
	timespecsub(&now, &swfe->swfe_installed_time, &duration);
	ofr->fr_duration_sec = ntohl((int)duration.tv_sec);
	ofr->fr_priority = htons(swfe->swfe_priority);
	ofr->fr_idle_timeout = htons(swfe->swfe_idle_timeout);
	ofr->fr_hard_timeout = htons(swfe->swfe_hard_timeout);
	ofr->fr_byte_count = htobe64(swfe->swfe_byte_cnt);
	ofr->fr_packet_count = htobe64(swfe->swfe_packet_cnt);

	memcpy(&ofr->fr_match, swfe->swfe_match, match_len);

	/* match_len inclusive ofp_match header length*/
	ofr->fr_oh.oh_length =
	    htons(sizeof(*ofr) + match_len - sizeof(struct ofp_match));

	m->m_len = m->m_pkthdr.len =
	    sizeof(*ofr) + match_len - sizeof(struct ofp_match);

	return (swofp_output(sc, m));
}

/*
 * OpenFlow protocol FLOW MOD message handlers
 */
int
swofp_flow_entry_put_instructions(struct switch_softc *sc, struct mbuf *m,
    struct swofp_flow_entry *swfe, uint16_t *etype, uint16_t *error)
{
	struct ofp_flow_mod	*ofm;
	struct ofp_instruction	*oi;
	caddr_t			 inst;
	int			 start, len, off;

	*etype = OFP_ERRTYPE_BAD_INSTRUCTION;

	ofm = mtod(m, struct ofp_flow_mod *);

	/*
	 * Clear instructions from flow entry. It's necessary to only modify
	 * flow but it's no problem to clear on adding flow because the flow
	 * entry doesn't have any instructions. So it's always called.
	 */
	swofp_flow_entry_instruction_free(swfe);

	start = OFP_FLOW_MOD_MSG_INSTRUCTION_OFFSET(ofm);
	len = ntohs(ofm->fm_oh.oh_length) - start;
	for (off = start; off < start + len; off += ntohs(oi->i_len)) {
		oi = (struct ofp_instruction *)(mtod(m, caddr_t) + off);

		if (swofp_validate_flow_instruction(sc, oi,
		    len - (off - start), etype, error))
			return (-1);

		if ((inst = malloc(ntohs(oi->i_len), M_DEVBUF,
		    M_DONTWAIT|M_ZERO)) == NULL) {
			*error = OFP_ERRFLOWMOD_UNKNOWN;
			return (-1);
		}
		memcpy(inst, oi, ntohs(oi->i_len));

		switch (ntohs(oi->i_type)) {
		case OFP_INSTRUCTION_T_GOTO_TABLE:
			if (swfe->swfe_goto_table)
				free(swfe->swfe_goto_table, M_DEVBUF,
				    ntohs(swfe->swfe_goto_table->igt_len));

			swfe->swfe_goto_table =
			    (struct ofp_instruction_goto_table *)inst;
			break;
		case OFP_INSTRUCTION_T_WRITE_META:
			if (swfe->swfe_write_metadata)
				free(swfe->swfe_write_metadata, M_DEVBUF,
				    ntohs(swfe->swfe_write_metadata->iwm_len));

			swfe->swfe_write_metadata =
			    (struct ofp_instruction_write_metadata *)inst;
			break;
		case OFP_INSTRUCTION_T_WRITE_ACTIONS:
			if (swfe->swfe_write_actions)
				free(swfe->swfe_write_actions, M_DEVBUF,
				    ntohs(swfe->swfe_write_actions->ia_len));

			swfe->swfe_write_actions =
			    (struct ofp_instruction_actions *)inst;
			break;
		case OFP_INSTRUCTION_T_APPLY_ACTIONS:
			if (swfe->swfe_apply_actions)
				free(swfe->swfe_apply_actions, M_DEVBUF,
				    ntohs(swfe->swfe_apply_actions->ia_len));

			swfe->swfe_apply_actions =
			    (struct ofp_instruction_actions *)inst;
			break;
		case OFP_INSTRUCTION_T_CLEAR_ACTIONS:
			if (swfe->swfe_clear_actions)
				free(swfe->swfe_clear_actions, M_DEVBUF,
				    ntohs(swfe->swfe_clear_actions->ia_len));

			swfe->swfe_clear_actions =
			    (struct ofp_instruction_actions *)inst;
			break;
		case OFP_INSTRUCTION_T_METER:
			if (swfe->swfe_meter)
				free(swfe->swfe_meter, M_DEVBUF,
				    ntohs(swfe->swfe_meter->im_len));

			swfe->swfe_meter = (struct ofp_instruction_meter *)inst;
			break;
		case OFP_INSTRUCTION_T_EXPERIMENTER:
			if (swfe->swfe_experimenter)
				free(swfe->swfe_experimenter, M_DEVBUF,
				    ntohs(swfe->swfe_experimenter->ie_len));

			swfe->swfe_experimenter =
			    (struct ofp_instruction_experimenter *)inst;
			break;
		default:
			free(inst, M_DEVBUF, ntohs(oi->i_len));
			break;
		}
	}

	return (0);
}

int
swofp_flow_mod_cmd_add(struct switch_softc *sc, struct mbuf *m)
{
	struct swofp_ofs		*ofs = sc->sc_ofs;
	struct ofp_flow_mod		*ofm;
	struct ofp_match		*om;
	struct swofp_flow_entry		*swfe, *old_swfe;
	struct swofp_flow_table		*swft;
	int				 omlen;
	uint16_t			 error, etype;

	etype = OFP_ERRTYPE_FLOW_MOD_FAILED;
	ofm = mtod(m, struct ofp_flow_mod *);
	om = &ofm->fm_match;

	if (OFP_TABLE_ID_MAX < ofm->fm_table_id) {
		error = OFP_ERRFLOWMOD_TABLE_ID;
		goto ofp_error;
	}

	if (ofm->fm_cookie == UINT64_MAX) {
		 /* XXX What is best error code? */
		error = OFP_ERRFLOWMOD_UNKNOWN;
		goto ofp_error;
	}

	omlen = ntohs(om->om_length);
	/*
	 * 1) ofp_match header must have at least its own size,
	 *    otherwise memcpy() will fail later;
	 * 2) OXM filters can't be bigger than the packet.
	 */
	if (omlen < sizeof(*om) ||
	    omlen >= (m->m_len - sizeof(*ofm))) {
		etype = OFP_ERRTYPE_BAD_MATCH;
		error = OFP_ERRMATCH_BAD_LEN;
		goto ofp_error;
	}

	if ((swft = swofp_flow_table_add(sc, ofm->fm_table_id)) == NULL) {
		error = OFP_ERRFLOWMOD_TABLE_ID;
		goto ofp_error;
	}

	if (swft->swft_flow_num >= ofs->swofs_flow_max_entry) {
		error = OFP_ERRFLOWMOD_TABLE_FULL;
		goto ofp_error;
	}

	/* Validate that the OXM are in-place and correct. */
	if (swofp_validate_flow_match(om, &error)) {
		etype = OFP_ERRTYPE_BAD_MATCH;
		goto ofp_error;
	}

	if ((old_swfe = swofp_flow_search_by_table(swft, om,
	    ntohs(ofm->fm_priority)))) {
		if (ntohs(ofm->fm_flags) & OFP_FLOWFLAG_CHECK_OVERLAP) {
			error = OFP_ERRFLOWMOD_OVERLAP;
			goto ofp_error;
		}
	}

	if ((swfe = malloc(sizeof(*swfe), M_DEVBUF,
	    M_DONTWAIT|M_ZERO)) == NULL) {
		error = OFP_ERRFLOWMOD_UNKNOWN;
		goto ofp_error;
	}

	swfe->swfe_priority = ntohs(ofm->fm_priority);
	swfe->swfe_cookie =  be64toh(ofm->fm_cookie);
	swfe->swfe_flags = ntohs(ofm->fm_flags);
	swfe->swfe_idle_timeout = ntohs(ofm->fm_idle_timeout);
	swfe->swfe_hard_timeout = ntohs(ofm->fm_hard_timeout);
	nanouptime(&swfe->swfe_installed_time);
	nanouptime(&swfe->swfe_idle_time);

	if ((swfe->swfe_match = malloc(omlen, M_DEVBUF,
	    M_DONTWAIT|M_ZERO)) == NULL) {
		error = OFP_ERRFLOWMOD_UNKNOWN;
		goto ofp_error_free_flow;
	}
	memcpy(swfe->swfe_match, om, omlen);

	/*
	 * If the ofp_match structure is empty and priority is zero, then
	 * this is a special flow type called table-miss which is the last
	 * flow to match.
	 */
	if (omlen == sizeof(*om) && swfe->swfe_priority == 0)
		swfe->swfe_tablemiss = 1;

	if (swofp_flow_entry_put_instructions(sc, m, swfe, &etype, &error))
		goto ofp_error_free_flow;

	if (old_swfe) {
		if (!(ntohs(ofm->fm_flags) & OFP_FLOWFLAG_RESET_COUNTS)) {
			swfe->swfe_packet_cnt = old_swfe->swfe_packet_cnt;
			swfe->swfe_byte_cnt = old_swfe->swfe_byte_cnt;
		}
		/*
		 * Doesn't need to send flow remove message because
		 * this deleted flow cause by internal reason
		 */
		swfe->swfe_flags &= ~(OFP_FLOWFLAG_SEND_FLOW_REMOVED);
		swofp_flow_entry_delete(sc, swft, old_swfe,
		    OFP_FLOWREM_REASON_DELETE);
	}

	swofp_flow_entry_add(sc, swft, swfe);

	m_freem(m);
	return (0);

 ofp_error_free_flow:
	swofp_flow_entry_free(&swfe);
 ofp_error:
	swofp_send_error(sc, m, etype, error);
	return (0);
}

int
swofp_flow_mod_cmd_common_modify(struct switch_softc *sc, struct mbuf *m,
    int strict)
{
	struct ofp_flow_mod		*ofm;
	struct ofp_match		*om;
	struct swofp_flow_entry		*swfe;
	struct swofp_flow_table		*swft;
	int				 omlen;
	uint16_t			 error, etype;

	etype = OFP_ERRTYPE_FLOW_MOD_FAILED;

	ofm = mtod(m, struct ofp_flow_mod *);
	om = &ofm->fm_match;

	if (OFP_TABLE_ID_MAX < ofm->fm_table_id) {
		error = OFP_ERRFLOWMOD_TABLE_ID;
		goto ofp_error;
	}

	if (ofm->fm_cookie == UINT64_MAX) {
		/* XXX What is best error code? */
		error = OFP_ERRFLOWMOD_UNKNOWN;
		goto ofp_error;
	}

	omlen = ntohs(om->om_length);
	/*
	 * 1) ofp_match header must have at least its own size,
	 *    otherwise memcpy() will fail later;
	 * 2) OXM filters can't be bigger than the packet.
	 */
	if (omlen < sizeof(*om) ||
	    omlen >= (m->m_len - sizeof(*ofm))) {
		etype = OFP_ERRTYPE_BAD_MATCH;
		error = OFP_ERRMATCH_BAD_LEN;
		goto ofp_error;
	}

	/* Validate that the OXM are in-place and correct. */
	if (swofp_validate_flow_match(om, &error)) {
		etype = OFP_ERRTYPE_BAD_MATCH;
		goto ofp_error;
	}

	if ((swft = swofp_flow_table_lookup(sc, ofm->fm_table_id)) == NULL) {
		error = OFP_ERRFLOWMOD_TABLE_ID;
		goto ofp_error;
	}

	LIST_FOREACH(swfe, &swft->swft_flow_list, swfe_next) {
		if (strict && !swofp_flow_cmp_strict(swfe, om,
		    ntohs(ofm->fm_priority)))
			continue;
		else if (!swofp_flow_cmp_non_strict(swfe, om))
			continue;

		if (!swofp_flow_filter(swfe, be64toh(ofm->fm_cookie),
		    be64toh(ofm->fm_cookie_mask), ntohl(ofm->fm_out_port),
		    ntohl(ofm->fm_out_group)))
			continue;

		if (swofp_flow_entry_put_instructions(sc, m, swfe, &etype,
		    &error)) {
			/*
			 * If error occurs in swofp_flow_entry_put_instructions,
			 * the flow entry might be half-way modified. So the
			 * flow entry is delete here.
			 */
			swofp_flow_entry_delete(sc, swft, swfe,
			    OFP_FLOWREM_REASON_DELETE);
			etype = OFP_ERRTYPE_BAD_INSTRUCTION;
			goto ofp_error;
		}

		if (ntohs(ofm->fm_flags) & OFP_FLOWFLAG_RESET_COUNTS) {
			swfe->swfe_packet_cnt = 0;
			swfe->swfe_byte_cnt = 0;
		}
	}

	m_freem(m);
	return (0);

 ofp_error:
	swofp_send_error(sc, m, etype, error);
	return (0);
}

int
swofp_flow_mod_cmd_modify(struct switch_softc *sc, struct mbuf *m)
{
	return swofp_flow_mod_cmd_common_modify(sc, m, 0);
}

int
swofp_flow_mod_cmd_modify_strict(struct switch_softc *sc, struct mbuf *m)
{
	return swofp_flow_mod_cmd_common_modify(sc, m, 1);
}


int
swofp_flow_mod_cmd_common_delete(struct switch_softc *sc, struct mbuf *m,
    int strict)
{
	struct swofp_ofs	*ofs = sc->sc_ofs;
	struct ofp_flow_mod	*ofm;
	struct ofp_match	*om;
	struct swofp_flow_table	*swft;
	int			 omlen;
	uint16_t		 error, etype = OFP_ERRTYPE_FLOW_MOD_FAILED;

	ofm = (struct ofp_flow_mod *)(mtod(m, caddr_t));
	om = &ofm->fm_match;

	omlen = ntohs(om->om_length);
	/*
	 * 1) ofp_match header must have at least its own size,
	 *    otherwise memcpy() will fail later;
	 * 2) OXM filters can't be bigger than the packet.
	 */
	if (omlen < sizeof(*om) ||
	    omlen >= (m->m_len - sizeof(*ofm))) {
		etype = OFP_ERRTYPE_BAD_MATCH;
		error = OFP_ERRMATCH_BAD_LEN;
		goto ofp_error;
	}

	/* Validate that the OXM are in-place and correct. */
	if (swofp_validate_flow_match(om, &error)) {
		etype = OFP_ERRTYPE_BAD_MATCH;
		goto ofp_error;
	}

	TAILQ_FOREACH(swft, &ofs->swofs_table_list, swft_table_next) {
		if ((ofm->fm_table_id != OFP_TABLE_ID_ALL) &&
		    (ofm->fm_table_id != swft->swft_table_id))
			continue;

		swofp_flow_delete_on_table(sc, swft, om,
		    ntohs(ofm->fm_priority),
		    be64toh(ofm->fm_cookie),
		    be64toh(ofm->fm_cookie_mask),
		    ntohl(ofm->fm_out_port),
		    ntohl(ofm->fm_out_group), strict);
	}

	m_freem(m);
	return (0);

 ofp_error:
	swofp_send_error(sc, m, etype, error);
	return (-1);
}

int
swofp_flow_mod_cmd_delete(struct switch_softc *sc, struct mbuf *m)
{
	return swofp_flow_mod_cmd_common_delete(sc, m, 0);
}

int
swofp_flow_mod_cmd_delete_strict(struct switch_softc *sc, struct mbuf *m)
{
	return swofp_flow_mod_cmd_common_delete(sc, m, 1);
}

ofp_msg_handler *
swofp_flow_mod_lookup_handler(uint8_t cmd)
{
	if (cmd >= nitems(ofp_flow_mod_table))
		return (NULL);
	else
		return (&ofp_flow_mod_table[cmd].ofm_cmd_handler);
}

int
swofp_flow_mod(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_flow_mod	*ofm;
	ofp_msg_handler		*handler;
	uint16_t		 ohlen;

	ofm = mtod(m, struct ofp_flow_mod *);
	ohlen = ntohs(ofm->fm_oh.oh_length);
	if (ohlen < sizeof(*ofm) ||
	    ohlen < (sizeof(*ofm) + ntohs(ofm->fm_match.om_length))) {
		swofp_send_error(sc, m, OFP_ERRTYPE_BAD_REQUEST,
		    OFP_ERRREQ_BAD_LEN);
		return (-1);
	}

	handler = swofp_flow_mod_lookup_handler(ofm->fm_command);
	if (handler) {
		(*handler)(sc, m);
	} else {
		swofp_send_error(sc, m, OFP_ERRTYPE_FLOW_MOD_FAILED,
				 OFP_ERRFLOWMOD_BAD_COMMAND);
	}

	return (0);
}

int
swofp_group_mod_add(struct switch_softc *sc, struct mbuf *m)
{
	struct swofp_ofs		*ofs = sc->sc_ofs;
	struct ofp_group_mod		*ogm;
	struct swofp_group_entry	*swge;
	uint16_t			 error, etype;

	etype = OFP_ERRTYPE_GROUP_MOD_FAILED;
	ogm = mtod(m, struct ofp_group_mod *);

	if (ofs->swofs_group_table_num >= ofs->swofs_group_max_table) {
		error = OFP_ERRGROUPMOD_OUT_OF_GROUPS;
		goto failed;
	}

	if (ntohl(ogm->gm_group_id) > OFP_GROUP_ID_MAX) {
		error = OFP_ERRGROUPMOD_INVALID_GROUP;
		goto failed;
	}

	if ((swge = swofp_group_entry_lookup(sc,
	    ntohl(ogm->gm_group_id)))) {
		error = OFP_ERRGROUPMOD_GROUP_EXISTS;
		goto failed;
	}

	if (ogm->gm_type != OFP_GROUP_T_ALL) {
		/* support ALL group only now*/
		error = OFP_ERRGROUPMOD_BAD_TYPE;
		goto failed;
	}

	if (swofp_validate_buckets(sc, m, ogm->gm_type, &etype, &error))
		goto failed;

	if ((swge = malloc(sizeof(*swge), M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL) {
		error = OFP_ERRGROUPMOD_UNKNOWN_GROUP;
		goto failed;
	}

	swge->swge_group_id = ntohl(ogm->gm_group_id);
	swge->swge_type = ogm->gm_type;

	swge->swge_buckets_len = (ntohs(ogm->gm_oh.oh_length) -
	    offsetof(struct ofp_group_mod, gm_buckets));

	if (swge->swge_buckets_len > 0) {
		if ((swge->swge_buckets = malloc(swge->swge_buckets_len,
		    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL) {
			free(swge, M_DEVBUF, sizeof(*swge));
			error = OFP_ERRGROUPMOD_UNKNOWN_GROUP;
			goto failed;
		}

		m_copydata(m, offsetof(struct ofp_group_mod, gm_buckets),
		    swge->swge_buckets_len, (caddr_t)swge->swge_buckets);
	}

	swofp_group_entry_add(sc, swge);

	m_freem(m);
	return (0);

 failed:
	swofp_send_error(sc, m, etype, error);
	return (0);
}

int
swofp_group_mod_modify(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_group_mod		*ogm;
	struct swofp_group_entry	*swge;
	uint16_t			 error, etype;
	uint32_t			 obucketlen;

	etype = OFP_ERRTYPE_GROUP_MOD_FAILED;
	ogm = mtod(m, struct ofp_group_mod *);

	if (ogm->gm_type != OFP_GROUP_T_ALL) {
		/* support ALL group only now */
		error = OFP_ERRGROUPMOD_BAD_TYPE;
		goto failed;
	}

	if ((swge = swofp_group_entry_lookup(sc,
	    ntohl(ogm->gm_group_id))) == NULL) {
		error = OFP_ERRGROUPMOD_UNKNOWN_GROUP;
		goto failed;
	}

	if (swofp_validate_buckets(sc, m, ogm->gm_type, &etype, &error))
		goto failed;

	swge->swge_type = ogm->gm_type;

	obucketlen = swge->swge_buckets_len;
	swge->swge_buckets_len = (ntohs(ogm->gm_oh.oh_length) -
	    offsetof(struct ofp_group_mod, gm_buckets));

	if (obucketlen != swge->swge_buckets_len) {
		free(swge->swge_buckets, M_DEVBUF, obucketlen);
		swge->swge_buckets = NULL;

		if (swge->swge_buckets_len > 0 &&
		    (swge->swge_buckets = malloc(swge->swge_buckets_len,
		    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL) {
			free(swge, M_DEVBUF, sizeof(*swge));
			error = OFP_ERRGROUPMOD_UNKNOWN_GROUP;
			goto failed;
		}
	}

	if (swge->swge_buckets != NULL)
		m_copydata(m, offsetof(struct ofp_group_mod, gm_buckets),
		    swge->swge_buckets_len, (caddr_t)swge->swge_buckets);

	m_freem(m);
	return (0);
failed:
	swofp_send_error(sc, m, etype, error);
	return (0);
}

int
swofp_group_mod_delete(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_group_mod		*ogm;
	struct swofp_group_entry	*swge;
	int				 group_id;

	ogm = mtod(m, struct ofp_group_mod *);
	group_id = ntohl(ogm->gm_group_id);

	if (group_id == OFP_GROUP_ID_ALL)
		swofp_group_entry_delete_all(sc);
	else if ((swge = swofp_group_entry_lookup(sc, group_id)) != NULL)
		swofp_group_entry_delete(sc, swge);
	else {
		swofp_send_error(sc, m, OFP_ERRTYPE_GROUP_MOD_FAILED,
		    OFP_ERRGROUPMOD_UNKNOWN_GROUP);
		return (0);
	}

	m_freem(m);
	return (0);
}

int
swofp_group_mod(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_group_mod	*ogm;
	uint16_t		 cmd;

	ogm = mtod(m, struct ofp_group_mod *);
	if (ntohs(ogm->gm_oh.oh_length) < sizeof(*ogm)) {
		swofp_send_error(sc, m, OFP_ERRTYPE_BAD_REQUEST,
		    OFP_ERRREQ_BAD_LEN);
		return (-1);
	}

	cmd = ntohs(ogm->gm_command);

	switch (cmd) {
	case OFP_GROUPCMD_ADD:
		return swofp_group_mod_add(sc, m);
	case OFP_GROUPCMD_MODIFY:
		return swofp_group_mod_modify(sc, m);
	case OFP_GROUPCMD_DELETE:
		return swofp_group_mod_delete(sc, m);
	default:
		swofp_send_error(sc, m, OFP_ERRTYPE_GROUP_MOD_FAILED,
		    OFP_ERRGROUPMOD_BAD_COMMAND);
		break;
	}

	return (0);
}



/*
 * OpenFlow protocol PACKET OUT message handler
 */
int
swofp_recv_packet_out(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_packet_out		*pout;
	struct ofp_action_header	*ah;
	struct mbuf			*mc = NULL, *mcn;
	int				 al_start, al_len, off;
	uint16_t			 ohlen, error;
	struct switch_flow_classify	 swfcl = {};
	struct swofp_pipeline_desc	 swpld = { .swpld_swfcl = &swfcl };

	pout = mtod(m, struct ofp_packet_out *);
	ohlen = ntohs(pout->pout_oh.oh_length);
	if (ohlen < sizeof(*pout) ||
	    ohlen < (sizeof(*pout) + ntohs(pout->pout_actions_len))) {
		swofp_send_error(sc, m, OFP_ERRTYPE_BAD_REQUEST,
		    OFP_ERRREQ_BAD_LEN);
		return (-1);
	}

	al_len = ntohs(pout->pout_actions_len);
	al_start = offsetof(struct ofp_packet_out, pout_actions);

       /* Validate actions before anything else. */
	ah = (struct ofp_action_header *)
	    ((uint8_t *)pout + sizeof(*pout));
	if (swofp_validate_action(sc, ah, al_len, &error)) {
		swofp_send_error(sc, m, OFP_ERRTYPE_BAD_ACTION, error);
		return (EINVAL);
	}

	if (pout->pout_buffer_id == OFP_PKTOUT_NO_BUFFER) {
		/*
		 * It's not necessary to deep copy at here because it's done
		 * in m_dup_pkt().
		 */
		if ((mc = m_split(m, (al_start + al_len), M_NOWAIT)) == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}

		mcn = m_dup_pkt(mc, ETHER_ALIGN, M_NOWAIT);
		m_freem(mc);
		if (mcn == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}

		mc = mcn;
	} else {
		/* TODO We don't do buffering yet. */
		swofp_send_error(sc, m, OFP_ERRTYPE_BAD_REQUEST,
		    OFP_ERRREQ_BUFFER_UNKNOWN);
		return (0);
	}

	mc = switch_flow_classifier(mc, pout->pout_in_port, &swfcl);
	if (mc == NULL) {
		m_freem(m);
		return (0);
	}

	TAILQ_INIT(&swpld.swpld_fwdp_q);
	swfcl.swfcl_in_port = ntohl(pout->pout_in_port);
	for (off = al_start; off < al_start + al_len;
	    off += ntohs(ah->ah_len)) {
		ah = (struct ofp_action_header *)(mtod(m, caddr_t) + off);

		mc = swofp_execute_action(sc, mc, &swpld, ah);
		if (mc == NULL)
			break;
	}

	if (mc)
		switch_port_egress(sc, &swpld.swpld_fwdp_q, mc);

	m_freem(m);

	return (0);
}

/*
 * OpenFlow protocol MULTIPART message:
 *
 *  Multipart messages are used to carry a large amount of data because a single
 *  OpenFlow message is limited to 64KB. If a message is over 64KB, it is
 *  splited some OpenFlow messages. OpenFlow Switch Specification says that
 *  "NO OBJECT CAN BE SPLIT ACROSS TWO MESSAGES". In other words, point of
 *  splittig is different per reply, so switch(4) builds multipart message using
 *  swofp_mpms_* functions which splits messsages not to object across
 *  two messages.
 */
int
swofp_mpmsg_reply_create(struct ofp_multipart *req, struct swofp_mpmsg *swmp)
{
	struct mbuf		*hdr, *body;
	struct ofp_multipart	*omp;

	memset(swmp, 0, sizeof(*swmp));

	ml_init(&swmp->swmp_body);

	MGETHDR(hdr, M_DONTWAIT, MT_DATA);
	if (hdr == NULL)
		goto error;

	memset(mtod(hdr, caddr_t), 0, sizeof(*omp));
	omp = mtod(hdr, struct ofp_multipart *);
	omp->mp_oh.oh_version = req->mp_oh.oh_version;
	omp->mp_oh.oh_xid = req->mp_oh.oh_xid;
	omp->mp_oh.oh_type = OFP_T_MULTIPART_REPLY;
	omp->mp_type = req->mp_type;
	hdr->m_len = hdr->m_pkthdr.len = sizeof(*omp);
	swmp->swmp_hdr = hdr;

	MGETHDR(body, M_DONTWAIT, MT_DATA);
	if (body == NULL)
		goto error;
	MCLGET(body, M_DONTWAIT);
	if ((body->m_flags & M_EXT) == 0) {
		m_freem(body);
		goto error;
	}
	body->m_len = body->m_pkthdr.len = 0;

	ml_enqueue(&swmp->swmp_body, body);

	return (0);

 error:
	m_freem(hdr);
	swmp->swmp_hdr = NULL;
	return (ENOBUFS);
}

/*
 * Copy data from a buffer back into the indicated swmp's body buffer
 */
int
swofp_mpmsg_put(struct swofp_mpmsg *swmp, caddr_t data, size_t len)
{
	struct mbuf	*m, *n;
	int		 error;

	KASSERT(swmp->swmp_hdr != NULL);

	m = swmp->swmp_body.ml_tail;
	if (m == NULL)
		return (ENOBUFS);

	if (m->m_pkthdr.len + len > SWOFP_MPMSG_MAX) {
		MGETHDR(n, M_DONTWAIT, MT_DATA);
		if (n == NULL)
			return (ENOBUFS);
		MCLGET(n, M_DONTWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			m_freem(n);
			return (ENOBUFS);
		}
		n->m_len = n->m_pkthdr.len = 0;

		ml_enqueue(&swmp->swmp_body, n);

		m = n;
	}

	if ((error = m_copyback(m, m->m_pkthdr.len, len, data, M_NOWAIT)))
		return (error);

	return (0);
}

/*
 * Copy data from a mbuf back into the indicated swmp's body buffer
 */
int
swofp_mpmsg_m_put(struct swofp_mpmsg *swmp, struct mbuf *msg)
{
	struct mbuf	*m, *n;
	int		 len;

	KASSERT(swmp->swmp_hdr != NULL);

	m = swmp->swmp_body.ml_tail;
	if (m == NULL)
		return (ENOBUFS);

	if (m->m_pkthdr.len + msg->m_pkthdr.len > SWOFP_MPMSG_MAX) {
		MGETHDR(n, M_DONTWAIT, MT_DATA);
		if (n == NULL)
			return (ENOBUFS);
		MCLGET(n, M_DONTWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			m_freem(n);
			return (ENOBUFS);
		}
		n->m_len = n->m_pkthdr.len = 0;

		ml_enqueue(&swmp->swmp_body, n);

		m = n;
	}

	len = m->m_pkthdr.len + msg->m_pkthdr.len;
	m_cat(m, msg);
	m->m_pkthdr.len = len;

	return (0);
}

void
swofp_mpmsg_destroy(struct swofp_mpmsg *swmp)
{
	m_freem(swmp->swmp_hdr);
	ml_purge(&swmp->swmp_body);
}

int
swofp_multipart_req(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_multipart	*omp;
	ofp_msg_handler		 handler;

	omp = mtod(m, struct ofp_multipart *);
	if (ntohs(omp->mp_oh.oh_length) < sizeof(*omp)) {
		swofp_send_error(sc, m, OFP_ERRTYPE_BAD_REQUEST,
		    OFP_ERRREQ_BAD_LEN);
		return (-1);
	}

	if (omp->mp_flags & OFP_T_MULTIPART_REQUEST) {
		/* multipart message re-assembly iss not supported yet */
		m_freem(m);
		return (ENOBUFS);
	}

	handler = swofp_lookup_mpmsg_handler(ntohs(omp->mp_type));
	if (handler)
		(*handler)(sc, m);
	else
		swofp_send_error(sc, m, OFP_ERRTYPE_BAD_REQUEST,
		    OFP_ERRREQ_MULTIPART);

	return (0);
}

int
swofp_multipart_reply(struct switch_softc *sc, struct swofp_mpmsg *swmp)
{
	struct ofp_multipart	*omp;
	struct mbuf		*hdr, *body;
	int			 len, error = 0;

	KASSERT(swmp->swmp_hdr != NULL);

	omp = mtod(swmp->swmp_hdr, struct ofp_multipart *);

	while ((body = ml_dequeue(&swmp->swmp_body)) != NULL) {
		omp->mp_oh.oh_length = htons(sizeof(*omp) + body->m_pkthdr.len);

		if (swmp->swmp_body.ml_tail != NULL) {
			omp->mp_flags |= htons(OFP_MP_FLAG_REPLY_MORE);
			if ((hdr = m_dup_pkt(swmp->swmp_hdr, 0,
			    M_WAITOK)) == NULL) {
				error = ENOBUFS;
				goto error;
			}
		} else {
			omp->mp_flags &= ~htons(OFP_MP_FLAG_REPLY_MORE);
			hdr = swmp->swmp_hdr;
		}

		if (body->m_pkthdr.len) {
			len = hdr->m_pkthdr.len + body->m_pkthdr.len;
			m_cat(hdr, body);
			hdr->m_pkthdr.len = len;
		} else
			m_freem(body);

		(void)swofp_output(sc, hdr);
	}

	return (0);
 error:
	swofp_mpmsg_destroy(swmp);
	return (error);
}

int
swofp_put_flow(struct mbuf *m, struct swofp_flow_table *swft,
    struct swofp_flow_entry *swfe)
{
	struct ofp_flow_stats	*pofs, ofs;
	struct timespec		 now, duration;
	const uint8_t		 pad_data[OFP_ALIGNMENT] = {};
	struct mbuf		*n;
	int			 start, off, error, offp, pad = 0;
	int			 omlen;

	memset(&ofs, 0, sizeof(ofs));

	ofs.fs_table_id = swft->swft_table_id;
	nanouptime(&now);
	timespecsub(&now, &swfe->swfe_installed_time, &duration);
	ofs.fs_duration_sec = ntohl((int)duration.tv_sec);
	ofs.fs_priority = htons(swfe->swfe_priority);
	ofs.fs_idle_timeout = htons(swfe->swfe_idle_timeout);
	ofs.fs_hard_timeout = htons(swfe->swfe_hard_timeout);
	ofs.fs_cookie = htobe64(swfe->swfe_cookie);
	ofs.fs_byte_count = htobe64(swfe->swfe_byte_cnt);
	ofs.fs_packet_count = htobe64(swfe->swfe_packet_cnt);

	/*
	 * struct ofp_flow_statsu has some fields which is variable length ,
	 * so the length is determined by all fields puts.
	 */
	start = off = m->m_pkthdr.len;

	/*
	 * Put ofp_flow_stat exclusive ofp_match because ofp_match is put
	 * with ofp_matches
	 */
	if ((error = m_copyback(m, off, (sizeof(ofs) -
	    sizeof(struct ofp_match)), &ofs, M_NOWAIT)))
		goto failed;
	off += (sizeof(ofs) - sizeof(struct ofp_match));

	/*
	 * Put ofp_match include ofp_ox_matches and pad
	 */
	omlen = ntohs(swfe->swfe_match->om_length);
	pad = OFP_ALIGN(omlen) - omlen;
	if ((error = m_copyback(m, off, omlen, swfe->swfe_match, M_NOWAIT)))
		goto failed;
	off += omlen;
	if ((error = m_copyback(m, off, pad, pad_data, M_NOWAIT)))
		goto failed;
	off += pad;

	/*
	 * Put instructions
	 */
	if (swfe->swfe_goto_table) {
		if ((error = m_copyback(m, off,
		    ntohs(swfe->swfe_goto_table->igt_len),
		    swfe->swfe_goto_table, M_NOWAIT)))
			goto failed;
		off += ntohs(swfe->swfe_goto_table->igt_len);
	}
	if (swfe->swfe_write_metadata) {
		if ((error = m_copyback(m, off,
		    ntohs(swfe->swfe_write_metadata->iwm_len),
		    swfe->swfe_write_metadata, M_NOWAIT)))
			goto failed;
		off += ntohs(swfe->swfe_write_metadata->iwm_len);
	}
	if (swfe->swfe_apply_actions) {
		if ((error = m_copyback(m, off,
		    ntohs(swfe->swfe_apply_actions->ia_len),
		    swfe->swfe_apply_actions, M_NOWAIT)))
			goto failed;
		off += ntohs(swfe->swfe_apply_actions->ia_len);
	}
	if (swfe->swfe_write_actions) {
		if ((error = m_copyback(m, off,
		    ntohs(swfe->swfe_write_actions->ia_len),
		    swfe->swfe_write_actions, M_NOWAIT)))
			goto failed;
		off += ntohs(swfe->swfe_write_actions->ia_len);
	}
	if (swfe->swfe_clear_actions) {
		if ((error = m_copyback(m, off,
		    ntohs(swfe->swfe_clear_actions->ia_len),
		    swfe->swfe_clear_actions, M_NOWAIT)))
			goto failed;
		off += ntohs(swfe->swfe_clear_actions->ia_len);
	}

	/*
	 * Set ofp_flow_stat length
	 */
	if ((n = m_pulldown(m, start, sizeof(*pofs), &offp)) == NULL)
		return (ENOBUFS);
	pofs = (struct ofp_flow_stats *)(mtod(n, caddr_t) + offp);
	pofs->fs_length = htons(off - start);

	return (0);

 failed:
	m_freem(m);
	return (error);
}

int
swofp_mp_recv_desc(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_desc		 od;
	struct swofp_mpmsg	 swmp;
	int			 error;

	if ((error = swofp_mpmsg_reply_create(
	    mtod(m, struct ofp_multipart *), &swmp)))
		goto failed;

	memset(&od, 0, sizeof(od));

	strlcpy(od.d_mfr_desc, "openbsd.org", OFP_DESC_STR_LEN);
	strlcpy(od.d_hw_desc, "openbsd", OFP_DESC_STR_LEN);
	strlcpy(od.d_sw_desc, "openbsd", OFP_DESC_STR_LEN);
	strlcpy(od.d_serial_num, "0", OFP_SERIAL_NUM_LEN);
	strlcpy(od.d_dp_desc, sc->sc_if.if_xname, OFP_DESC_STR_LEN);

	if ((error = swofp_mpmsg_put(&swmp, (caddr_t)&od, sizeof(od))))
		goto failed;

	m_freem(m);
	return swofp_multipart_reply(sc, &swmp);

 failed:
	m_freem(m);
	swofp_mpmsg_destroy(&swmp);
	return (error);
}

int
swofp_put_flows_from_table(struct swofp_mpmsg *swmp,
    struct swofp_flow_table *swft, struct ofp_flow_stats_request *ofsr)
{
	struct swofp_flow_entry *swfe;
	struct mbuf		*m;
	int			 error = 0;

	LIST_FOREACH(swfe, &swft->swft_flow_list, swfe_next) {
		if (!swofp_flow_cmp_non_strict(swfe, &ofsr->fsr_match))
			continue;

		if (!swofp_flow_filter(swfe, be64toh(ofsr->fsr_cookie),
		    be64toh(ofsr->fsr_cookie_mask), ntohl(ofsr->fsr_out_port),
		    ntohl(ofsr->fsr_out_group)))
			continue;

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			return (ENOBUFS);
		m->m_len = m->m_pkthdr.len = 0;

		if ((error = swofp_put_flow(m, swft, swfe)))
			break;

		if ((error = swofp_mpmsg_m_put(swmp, m))) {
			/* swofp_mpmsg_m_put() doesn't free m on error */
			m_freem(m);
			break;
		}
	}

	return (error);
}

int
swofp_mp_recv_flow(struct switch_softc *sc, struct mbuf *m)
{
	struct swofp_ofs		*ofs = sc->sc_ofs;
	struct ofp_flow_stats_request	*ofsr;
	struct swofp_flow_table		*swft;
	struct swofp_mpmsg		 swmp;
	int				 error;

	if ((error = swofp_mpmsg_reply_create(
	    mtod(m, struct ofp_multipart *), &swmp)))
		goto failed;

	ofsr = (struct ofp_flow_stats_request *)
	    (mtod(m, caddr_t) + sizeof(struct ofp_multipart));

	TAILQ_FOREACH(swft, &ofs->swofs_table_list, swft_table_next) {
		if ((ofsr->fsr_table_id != OFP_TABLE_ID_ALL) &&
		    (ofsr->fsr_table_id != swft->swft_table_id))
			continue;

		if ((error = swofp_put_flows_from_table(&swmp, swft, ofsr)))
			goto failed;
	}

	m_freem(m);
	return swofp_multipart_reply(sc, &swmp);

 failed:
	m_freem(m);
	swofp_mpmsg_destroy(&swmp);
	return (error);
}

void
swofp_aggregate_stat_from_table(struct ofp_aggregate_stats *aggstat,
    struct swofp_flow_table *swft, struct ofp_aggregate_stats_request *oasr)
{
	struct swofp_flow_entry *swfe;
	uint64_t		 packet_cnt = 0, byte_cnt = 0;
	uint32_t		 flow_cnt = 0;

	LIST_FOREACH(swfe, &swft->swft_flow_list, swfe_next) {
		if (!swofp_flow_cmp_non_strict(swfe, &oasr->asr_match))
			continue;

		if (!swofp_flow_filter(swfe, be64toh(oasr->asr_cookie),
		    be64toh(oasr->asr_cookie_mask), ntohl(oasr->asr_out_port),
		    ntohl(oasr->asr_out_group)))
			continue;

		packet_cnt += swfe->swfe_packet_cnt;
		byte_cnt += swfe->swfe_byte_cnt;
		flow_cnt++;
	}

	aggstat->as_packet_count = htobe64(packet_cnt);
	aggstat->as_byte_count = htobe64(byte_cnt);
	aggstat->as_flow_count = htonl(flow_cnt++);
}

int
swofp_mp_recv_aggregate_flow_stat(struct switch_softc *sc, struct mbuf *m)
{
	struct swofp_ofs			*ofs = sc->sc_ofs;
	struct ofp_aggregate_stats_request	*oasr;
	struct swofp_flow_table			*swft;
	struct swofp_mpmsg			 swmp;
	struct ofp_aggregate_stats		 aggstat;
	int					 error;

	if ((error = swofp_mpmsg_reply_create(
	    mtod(m, struct ofp_multipart *), &swmp)))
		goto failed;

	memset(&aggstat, 0, sizeof(aggstat));

	oasr = (struct ofp_aggregate_stats_request *)
	    (mtod(m, caddr_t) + sizeof(struct ofp_multipart));

	TAILQ_FOREACH(swft, &ofs->swofs_table_list, swft_table_next) {
		if ((oasr->asr_table_id != OFP_TABLE_ID_ALL) &&
		    (oasr->asr_table_id != swft->swft_table_id))
			continue;
		swofp_aggregate_stat_from_table(&aggstat, swft, oasr);
	}

	if ((error = swofp_mpmsg_put(&swmp, (caddr_t)&aggstat,
	    sizeof(aggstat))))
		goto failed;

	m_freem(m);
	return swofp_multipart_reply(sc, &swmp);

 failed:
	m_freem(m);
	swofp_mpmsg_destroy(&swmp);
	return (error);
}

int
swofp_mp_recv_table_stats(struct switch_softc *sc, struct mbuf *m)
{
	struct swofp_ofs	*ofs = sc->sc_ofs;
	struct ofp_table_stats	 tblstat;
	struct swofp_flow_table	*swft;
	struct swofp_mpmsg	 swmp;
	int			 error;


	if ((error = swofp_mpmsg_reply_create(
	    mtod(m, struct ofp_multipart *), &swmp)))
		goto failed;

	TAILQ_FOREACH(swft, &ofs->swofs_table_list, swft_table_next) {
		memset(&tblstat, 0, sizeof(tblstat));

		tblstat.ts_table_id = swft->swft_table_id;
		tblstat.ts_active_count = htonl((uint32_t)swft->swft_flow_num);
		tblstat.ts_lookup_count = htobe64(swft->swft_lookup_count);
		tblstat.ts_matched_count = htobe64(swft->swft_matched_count);

		if ((error = swofp_mpmsg_put(&swmp, (caddr_t)&tblstat,
		    sizeof(tblstat))))
			goto failed;
	}

	m_freem(m);
	return swofp_multipart_reply(sc, &swmp);

 failed:
	m_freem(m);
	swofp_mpmsg_destroy(&swmp);
	return (error);
}

int
swofp_mp_recv_port_stats(struct switch_softc *sc, struct mbuf *m)
{
	struct switch_port	*swpo;
	struct swofp_mpmsg	 swmp;
	struct ifnet		*ifs;
	struct ofp_port_stats	 postat;
	int			 error;
	struct timespec		 now, duration;

	if ((error = swofp_mpmsg_reply_create(
	    mtod(m, struct ofp_multipart *), &swmp)))
		goto failed;

	nanouptime(&now);

	TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
		memset(&postat, 0, sizeof(postat));
		ifs = if_get(swpo->swpo_ifindex);
		if (ifs == NULL)
			continue;

		if (swpo->swpo_flags & IFBIF_LOCAL)
			postat.pt_port_no = htonl(OFP_PORT_LOCAL);
		else
			postat.pt_port_no = htonl(swpo->swpo_port_no);
		postat.pt_rx_packets = htobe64(ifs->if_ipackets);
		postat.pt_tx_packets = htobe64(ifs->if_opackets);
		postat.pt_rx_bytes = htobe64(ifs->if_obytes);
		postat.pt_tx_bytes = htobe64(ifs->if_ibytes);
		postat.pt_rx_dropped = htobe64(ifs->if_iqdrops);
		postat.pt_tx_dropped = htobe64(ifs->if_oqdrops);
		postat.pt_rx_errors = htobe64(ifs->if_ierrors);
		postat.pt_tx_errors = htobe64(ifs->if_oerrors);
		postat.pt_rx_frame_err = htobe64(0);
		postat.pt_rx_over_err = htobe64(0);
		postat.pt_rx_crc_err = htobe64(0);
		postat.pt_collision = htobe64(ifs->if_collisions);
		timespecsub(&now, &swpo->swpo_appended, &duration);
		postat.pt_duration_sec = htonl((uint32_t)duration.tv_sec);
		postat.pt_duration_nsec = htonl(duration.tv_nsec);

		if_put(ifs);

		if ((error = swofp_mpmsg_put(&swmp, (caddr_t)&postat,
		    sizeof(postat))))
			goto failed;
	}

	m_freem(m);
	return swofp_multipart_reply(sc, &swmp);

 failed:
	m_freem(m);
	swofp_mpmsg_destroy(&swmp);
	return (error);
}

int
swofp_table_features_put_oxm(struct mbuf *m, int *off, uint16_t tp_type)
{
	struct ofp_table_feature_property	 tp;
	struct ofp_ox_match			 oxm;
	uint32_t				 padding = 0;
	int					 i, supported = 0;

	for (i = 0 ; i < nitems(ofp_oxm_handlers); i++) {
		switch (tp_type) {
		case OFP_TABLE_FEATPROP_MATCH:
			if (ofp_oxm_handlers[i].oxm_match == NULL)
				continue;
			break;
		case OFP_TABLE_FEATPROP_WILDCARDS:
			if (ofp_oxm_handlers[i].oxm_match == NULL ||
			    !(ofp_oxm_handlers[i].oxm_flags &
			    SWOFP_MATCH_WILDCARD))
				continue;
			break;
		case OFP_TABLE_FEATPROP_APPLY_SETFIELD:
		case OFP_TABLE_FEATPROP_APPLY_SETFIELD_MISS:
		case OFP_TABLE_FEATPROP_WRITE_SETFIELD:
		case OFP_TABLE_FEATPROP_WRITE_SETFIELD_MISS:
			if (ofp_oxm_handlers[i].oxm_set == NULL)
				continue;
			break;
		}
		supported++;
	}

	tp.tp_type = htons(tp_type);
	tp.tp_length = htons((sizeof(oxm) * supported) + sizeof(tp));

	if (m_copyback(m, *off, sizeof(tp), (caddr_t)&tp, M_NOWAIT))
		return (OFP_ERRREQ_MULTIPART_OVERFLOW);
	*off += sizeof(tp);

	for (i = 0 ; i < nitems(ofp_oxm_handlers); i++) {
		switch (tp_type) {
		case OFP_TABLE_FEATPROP_MATCH:
			if (ofp_oxm_handlers[i].oxm_match == NULL)
				continue;
			break;
		case OFP_TABLE_FEATPROP_WILDCARDS:
			if (ofp_oxm_handlers[i].oxm_match == NULL ||
			    !(ofp_oxm_handlers[i].oxm_flags &
			    SWOFP_MATCH_WILDCARD))
				continue;
			break;
		case OFP_TABLE_FEATPROP_APPLY_SETFIELD:
		case OFP_TABLE_FEATPROP_APPLY_SETFIELD_MISS:
		case OFP_TABLE_FEATPROP_WRITE_SETFIELD:
		case OFP_TABLE_FEATPROP_WRITE_SETFIELD_MISS:
			if (ofp_oxm_handlers[i].oxm_set == NULL)
				continue;
			break;
		}

		memset(&oxm, 0, sizeof(oxm));
		OFP_OXM_SET_FIELD(&oxm, ofp_oxm_handlers[i].oxm_field);
		if ((tp_type == OFP_TABLE_FEATPROP_MATCH) &&
		    (ofp_oxm_handlers[i].oxm_flags & SWOFP_MATCH_MASK))
			OFP_OXM_SET_HASMASK(&oxm);
		oxm.oxm_class = htons(OFP_OXM_C_OPENFLOW_BASIC);
		oxm.oxm_length = ofp_oxm_handlers[i].oxm_len;

		if (m_copyback(m, *off, sizeof(oxm), (caddr_t)&oxm, M_NOWAIT))
			return (OFP_ERRREQ_MULTIPART_OVERFLOW);
		*off += sizeof(oxm);
	}

	/*
	 * It's always 4 byte for padding becouse struct ofp_ox_mach and
	 * struct ofp_table_feature_property are 4 byte.
	 */
	if ((supported & 0x1) == 0) {
		if (m_copyback(m, *off, sizeof(padding),
		    (caddr_t)&padding, M_NOWAIT))
			return (OFP_ERRREQ_MULTIPART_OVERFLOW);
		*off += sizeof(padding);
	}

	return (0);
}

int
swofp_table_features_put_actions(struct mbuf *m, int *off, uint16_t tp_type)
{
	struct ofp_table_feature_property	 tp;
	struct ofp_action_header		 action;
	int					 i, supported = 0;
	int					 actionlen, padsize;
	uint8_t					 padding[8];

	for (i = 0 ; i < nitems(ofp_action_handlers); i++) {
		if (ofp_action_handlers[i].action)
			supported++;
	}

	actionlen = sizeof(action) - sizeof(action.ah_pad);
	tp.tp_type = htons(tp_type);
	tp.tp_length = (actionlen * supported) + sizeof(tp);

	padsize = OFP_ALIGN(tp.tp_length) - tp.tp_length;
	tp.tp_length = htons(tp.tp_length);

	if (m_copyback(m, *off, sizeof(tp), (caddr_t)&tp, M_NOWAIT))
		return (OFP_ERRREQ_MULTIPART_OVERFLOW);
	*off += sizeof(tp);

	for (i = 0 ; i < nitems(ofp_action_handlers); i++) {
		if (ofp_action_handlers[i].action == NULL)
			continue;

		memset(&action, 0, actionlen);
		action.ah_type = ntohs(ofp_action_handlers[i].action_type);
		/* XXX action length is different for experimenter type. */
		action.ah_len = ntohs(actionlen);

		if (m_copyback(m, *off, actionlen,
		    (caddr_t)&action, M_NOWAIT))
			return (OFP_ERRREQ_MULTIPART_OVERFLOW);
		*off += actionlen;
	}

	if (padsize) {
		memset(padding, 0, padsize);
		if (m_copyback(m, *off, padsize, &padding, M_NOWAIT))
			return (OFP_ERRREQ_MULTIPART_OVERFLOW);

		*off += padsize;
	}

	return (0);
}

int
swofp_table_features_put_instruction(struct mbuf *m, int *off, uint16_t tp_type)
{
	struct ofp_table_feature_property	 tp;
	struct ofp_instruction			 instructions[] =
	    {
		{
			htons(OFP_INSTRUCTION_T_GOTO_TABLE),
			htons(sizeof(struct ofp_instruction))
		},
		{
			htons(OFP_INSTRUCTION_T_WRITE_META),
			htons(sizeof(struct ofp_instruction))
		},
		{
			htons(OFP_INSTRUCTION_T_WRITE_ACTIONS),
			htons(sizeof(struct ofp_instruction))
		},
		{
			htons(OFP_INSTRUCTION_T_APPLY_ACTIONS),
			htons(sizeof(struct ofp_instruction))
		},
		{
			htons(OFP_INSTRUCTION_T_CLEAR_ACTIONS),
			htons(sizeof(struct ofp_instruction))
		},
	    };

	tp.tp_type = htons(tp_type);
	tp.tp_length = htons(sizeof(instructions) + sizeof(tp));

	if (m_copyback(m, *off, sizeof(tp), (caddr_t)&tp, M_NOWAIT))
		return (OFP_ERRREQ_MULTIPART_OVERFLOW);
	*off += sizeof(tp);

	if (m_copyback(m, *off, sizeof(instructions),
	    (caddr_t)instructions, M_NOWAIT))
		return (OFP_ERRREQ_MULTIPART_OVERFLOW);
	*off += sizeof(instructions);

	return (0);
}

int
swofp_mp_recv_table_features(struct switch_softc *sc, struct mbuf *m)
{
	struct swofp_ofs		*ofs = sc->sc_ofs;
	struct swofp_flow_table		*swft;
	struct ofp_table_features	*tblf;
	struct mbuf			*n;
	int				 off, error;
	struct swofp_mpmsg		 swmp;

	if ((error = swofp_mpmsg_reply_create(
	    mtod(m, struct ofp_multipart *), &swmp)))
		goto error;

	TAILQ_FOREACH(swft, &ofs->swofs_table_list, swft_table_next) {
		/* using mbuf becouse table featrues struct is variable length*/
		MGETHDR(n, M_DONTWAIT, MT_DATA);
		if (n == NULL)
			goto error;
		MCLGET(n, M_DONTWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			m_freem(n);
			goto error;
		}
		n->m_len = n->m_pkthdr.len = sizeof(*tblf);

		tblf = mtod(n, struct ofp_table_features *);
		memset(tblf, 0, sizeof(*tblf));
		tblf->tf_tableid = swft->swft_table_id;
		tblf->tf_metadata_match = UINT64_MAX;
		tblf->tf_metadata_write = UINT64_MAX;
		tblf->tf_config = 0;
		tblf->tf_max_entries = htonl(ofs->swofs_flow_max_entry);

		off = sizeof(*tblf);
		if ((error = swofp_table_features_put_instruction(n, &off,
		    OFP_TABLE_FEATPROP_INSTRUCTION)))
			goto error;

		if ((error = swofp_table_features_put_instruction(n, &off,
		    OFP_TABLE_FEATPROP_INSTRUCTION_MISS)))
			goto error;

		if ((error = swofp_table_features_put_actions(n, &off,
		    OFP_TABLE_FEATPROP_APPLY_ACTIONS)))
			goto error;

		if ((error = swofp_table_features_put_actions(n, &off,
		    OFP_TABLE_FEATPROP_APPLY_ACTIONS_MISS)))
			goto error;

		if ((error = swofp_table_features_put_actions(n, &off,
		    OFP_TABLE_FEATPROP_WRITE_ACTIONS)))
			goto error;

		if ((error = swofp_table_features_put_actions(n, &off,
		    OFP_TABLE_FEATPROP_WRITE_ACTIONS_MISS)))
			goto error;

		if ((error = swofp_table_features_put_oxm(n, &off,
		    OFP_TABLE_FEATPROP_MATCH)))
			goto error;

		if ((error = swofp_table_features_put_oxm(n, &off,
		    OFP_TABLE_FEATPROP_WILDCARDS)))
			goto error;

		if ((error = swofp_table_features_put_oxm(n, &off,
		    OFP_TABLE_FEATPROP_WRITE_SETFIELD)))
			goto error;

		if ((error = swofp_table_features_put_oxm(n, &off,
		    OFP_TABLE_FEATPROP_WRITE_SETFIELD_MISS)))
			goto error;

		if ((error = swofp_table_features_put_oxm(n, &off,
		    OFP_TABLE_FEATPROP_APPLY_SETFIELD)))
			goto error;

		if ((error = swofp_table_features_put_oxm(n, &off,
		    OFP_TABLE_FEATPROP_APPLY_SETFIELD_MISS)))
			goto error;

		tblf->tf_length = htons(n->m_pkthdr.len);

		if ((error = swofp_mpmsg_m_put(&swmp, n))) {
			m_freem(n);
			goto error;
		}
	}

	m_freem(m);
	return swofp_multipart_reply(sc, &swmp);

 error:
	m_freem(m);
	swofp_mpmsg_destroy(&swmp);
	return (error);
}

int
swofp_mp_recv_port_desc(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_switch_port	 swp;
	struct switch_port	*swpo;
	struct swofp_mpmsg	 swmp;
	struct ifnet		*ifs;
	int			 error;

	if ((error = swofp_mpmsg_reply_create(
	    mtod(m, struct ofp_multipart *), &swmp))) {
		m_freem(m);
		return (error);
	}

	TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
		memset(&swp, 0, sizeof(swp));
		ifs = if_get(swpo->swpo_ifindex);
		if (ifs == NULL)
			continue;

		if (swpo->swpo_flags & IFBIF_LOCAL)
			swp.swp_number = htonl(OFP_PORT_LOCAL);
		else
			swp.swp_number = htonl(swpo->swpo_port_no);

		memcpy(swp.swp_macaddr,
		    ((struct arpcom *)ifs)->ac_enaddr, ETHER_ADDR_LEN);
		strlcpy(swp.swp_name, ifs->if_xname,
		    sizeof(swp.swp_name));

		if (!ISSET(ifs->if_flags, IFF_UP))
			swp.swp_config |= OFP_PORTCONFIG_PORT_DOWN;
		if (!ISSET(swpo->swpo_flags, IFBIF_STP))
			swp.swp_config |= OFP_PORTCONFIG_NO_STP;
		swp.swp_config = htonl(swp.swp_config);

		if (!LINK_STATE_IS_UP(ifs->if_data.ifi_link_state))
			swp.swp_state |= OFP_PORTSTATE_LINK_DOWN;

		if_put(ifs);

		swp.swp_state = htonl(swp.swp_state);
		/* XXX how to get the if_media from ifp? ioctl? */
		swp.swp_cur = htonl(swp.swp_cur);
		swp.swp_advertised = htonl(swp.swp_advertised);
		swp.swp_supported = htonl(swp.swp_supported);
		swp.swp_peer = htonl(swp.swp_peer);

		if ((error = swofp_mpmsg_put(&swmp, (caddr_t)&swp,
		    sizeof(swp))))
			goto error;
	}

	m_freem(m);
	return swofp_multipart_reply(sc, &swmp);

 error:
	m_freem(m);
	swofp_mpmsg_destroy(&swmp);
	return (error);
}

int
swofp_mp_recv_group_desc(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_group_desc		 ogd;
	struct swofp_group_entry	*swge;
	struct swofp_mpmsg		 swmp;
	int				 error;

	if ((error = swofp_mpmsg_reply_create(
	    mtod(m, struct ofp_multipart *), &swmp)))
		goto failed;


	LIST_FOREACH(swge, &sc->sc_ofs->swofs_group_table, swge_next) {
		memset(&ogd, 0, sizeof(ogd));

		ogd.gd_length = htons(offsetof(struct ofp_group_desc,
		    gd_buckets) + swge->swge_buckets_len);
		ogd.gd_group_id = htonl(swge->swge_group_id);
		ogd.gd_type = swge->swge_type;

		/*
		 * Copy back GROUP DESC without buckets
		 */
		if ((error = swofp_mpmsg_put(&swmp, (caddr_t)&ogd,
		    sizeof(ogd))))
			goto failed;

		/*
		 * Copy back buckets on GROUP DESC
		 */
		if (swge->swge_buckets != NULL &&
		    (error = swofp_mpmsg_put(&swmp, (caddr_t)swge->swge_buckets,
		    swge->swge_buckets_len)))
			goto failed;
	}

	m_freem(m);
	return swofp_multipart_reply(sc, &swmp);

 failed:
	m_freem(m);
	swofp_mpmsg_destroy(&swmp);
	return (error);
}

int
swofp_barrier_req(struct switch_softc *sc, struct mbuf *m)
{
	swofp_barrier_reply(sc, m);
	return 0;
}

void
swofp_barrier_reply(struct switch_softc *sc, struct mbuf *m)
{
	struct ofp_header	*oh;

	oh = mtod(m, struct ofp_header *);
	oh->oh_type = OFP_T_BARRIER_REPLY;

	(void)swofp_output(sc, m);
}
