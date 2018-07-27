/*	$OpenBSD: slaacd.h,v 1.20 2018/07/27 06:23:08 bket Exp $	*/

/*
 * Copyright (c) 2017 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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

#define	SLAACD_SOCKET		"/dev/slaacd.sock"
#define SLAACD_USER		"_slaacd"
#define SLAACD_RTA_LABEL	"slaacd"

#define SLAACD_SOIIKEY_LEN	16

/* MAXDNAME from arpa/namesr.h */
#define SLAACD_MAX_DNSSL	1025

static const char * const log_procnames[] = {
	"main",
	"engine",
	"frontend"
};

struct imsgev {
	struct imsgbuf	 ibuf;
	void		(*handler)(int, short, void *);
	struct event	 ev;
	short		 events;
};

enum imsg_type {
	IMSG_NONE,
#ifndef	SMALL
	IMSG_CTL_LOG_VERBOSE,
	IMSG_CTL_SHOW_INTERFACE_INFO,
	IMSG_CTL_SHOW_INTERFACE_INFO_RA,
	IMSG_CTL_SHOW_INTERFACE_INFO_RA_PREFIX,
	IMSG_CTL_SHOW_INTERFACE_INFO_RA_RDNS,
	IMSG_CTL_SHOW_INTERFACE_INFO_RA_DNSSL,
	IMSG_CTL_SHOW_INTERFACE_INFO_ADDR_PROPOSALS,
	IMSG_CTL_SHOW_INTERFACE_INFO_ADDR_PROPOSAL,
	IMSG_CTL_SHOW_INTERFACE_INFO_DFR_PROPOSALS,
	IMSG_CTL_SHOW_INTERFACE_INFO_DFR_PROPOSAL,
	IMSG_CTL_END,
	IMSG_UPDATE_ADDRESS,
	IMSG_UPDATE_LINK_STATE,
#endif	/* SMALL */
	IMSG_CTL_SEND_SOLICITATION,
	IMSG_SOCKET_IPC,
	IMSG_ICMP6SOCK,
	IMSG_ROUTESOCK,
	IMSG_CONTROLFD,
	IMSG_STARTUP,
	IMSG_STARTUP_DONE,
	IMSG_UPDATE_IF,
	IMSG_REMOVE_IF,
	IMSG_RA,
	IMSG_PROPOSAL,
	IMSG_PROPOSAL_ACK,
	IMSG_CONFIGURE_ADDRESS,
	IMSG_DEL_ADDRESS,
	IMSG_DEL_ROUTE,
	IMSG_FAKE_ACK,
	IMSG_CONFIGURE_DFR,
	IMSG_WITHDRAW_DFR,
	IMSG_DUP_ADDRESS,
};

enum {
	PROC_MAIN,
	PROC_ENGINE,
	PROC_FRONTEND
} slaacd_process;

enum rpref {
	LOW,
	MEDIUM,
	HIGH,
};

#ifndef	SMALL
struct ctl_engine_info {
	uint32_t		if_index;
	int			running;
	int			autoconfprivacy;
	int			soii;
	struct ether_addr	hw_address;
	struct sockaddr_in6	ll_address;
};

struct ctl_engine_info_ra {
	struct sockaddr_in6	 from;
	struct timespec		 when;
	struct timespec		 uptime;
	uint8_t			 curhoplimit;
	int			 managed;
	int			 other;
	char			 rpref[sizeof("MEDIUM")];
	uint16_t		 router_lifetime;	/* in seconds */
	uint32_t		 reachable_time;	/* in milliseconds */
	uint32_t		 retrans_time;		/* in milliseconds */
	uint32_t		 mtu;
};

struct ctl_engine_info_ra_prefix {
	struct in6_addr		prefix;
	uint8_t			prefix_len;
	int			onlink;
	int			autonomous;
	uint32_t		vltime;
	uint32_t		pltime;
};

struct ctl_engine_info_ra_rdns {
	uint32_t		lifetime;
	struct in6_addr		rdns;
};

struct ctl_engine_info_ra_dnssl {
	uint32_t		lifetime;
	char			dnssl[SLAACD_MAX_DNSSL];
};

struct ctl_engine_info_address_proposal {
	int64_t			 id;
	char			 state[sizeof("PROPOSAL_NEARLY_EXPIRED")];
	time_t			 next_timeout;
	int			 timeout_count;
	struct timespec		 when;
	struct timespec		 uptime;
	struct sockaddr_in6	 addr;
	struct in6_addr		 prefix;
	int			 privacy;
	uint8_t			 prefix_len;
	uint32_t		 vltime;
	uint32_t		 pltime;
};

struct ctl_engine_info_dfr_proposal {
	int64_t			 id;
	char			 state[sizeof("PROPOSAL_NEARLY_EXPIRED")];
	time_t			 next_timeout;
	int			 timeout_count;
	struct timespec		 when;
	struct timespec		 uptime;
	struct sockaddr_in6	 addr;
	uint32_t		 router_lifetime;
	char			 rpref[sizeof("MEDIUM")];
};

struct imsg_addrinfo {
	uint32_t		if_index;
	struct ether_addr	hw_address;
	struct sockaddr_in6	ll_address;
	struct sockaddr_in6	addr;
	struct in6_addr		mask;
	int			privacy;
	uint32_t		vltime;
	uint32_t		pltime;
};

struct imsg_link_state {
	uint32_t	if_index;
	int		link_state;
};
#endif	/* SMALL */

struct imsg_ifinfo {
	uint32_t		if_index;
	int			running;
	int			autoconfprivacy;
	int			soii;
	struct ether_addr	hw_address;
	struct sockaddr_in6	ll_address;
	uint8_t			soiikey[SLAACD_SOIIKEY_LEN];
};

struct imsg_del_addr {
	uint32_t		if_index;
	struct sockaddr_in6	addr;
};

struct imsg_del_route {
	uint32_t		if_index;
	struct sockaddr_in6	gw;
};

struct imsg_proposal_ack {
	int64_t		 id;
	pid_t		 pid;
	uint32_t	 if_index;
};

struct imsg_ra {
	uint32_t		if_index;
	struct sockaddr_in6	from;
	ssize_t			len;
	uint8_t			packet[1500];
};

struct imsg_dup_addr {
	uint32_t		if_index;
	struct sockaddr_in6	addr;
};

/* slaacd.c */
void		imsg_event_add(struct imsgev *);
int		imsg_compose_event(struct imsgev *, uint16_t, uint32_t, pid_t,
		    int, void *, uint16_t);
#ifndef	SMALL
const char	*sin6_to_str(struct sockaddr_in6 *);
#else
#define	sin6_to_str(x...)	""
#endif	/* SMALL */
