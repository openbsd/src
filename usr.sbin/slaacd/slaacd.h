/*	$OpenBSD: slaacd.h,v 1.2 2017/03/18 21:22:08 florian Exp $	*/

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

#define	SLAACD_SOCKET		"/var/run/slaacd.sock"
#define SLAACD_USER		"_slaacd"

#define OPT_VERBOSE	0x00000001
#define OPT_VERBOSE2	0x00000002

#define SLAACD_MAXTEXT		256
#define SLAACD_MAXGROUPNAME	16

#if 0
#define DEBUG_IMSG(fmt, arg...) log_debug(fmt, ##arg)
#else
#define DEBUG_IMSG(fmt, arg...)
#endif
/* MAXDNAME from arpa/namesr.h */
#define SLAACD_MAX_DNSSL	1025

static const char * const log_procnames[] = {
	"main",
	"frontend",
	"engine"
};

struct imsgev {
	struct imsgbuf	 ibuf;
	void		(*handler)(int, short, void *);
	struct event	 ev;
	short		 events;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_LOG_VERBOSE,
	IMSG_CTL_SHOW_INTERFACE_INFO,
	IMSG_CTL_SHOW_INTERFACE_INFO_RA,
	IMSG_CTL_SHOW_INTERFACE_INFO_RA_PREFIX,
	IMSG_CTL_SHOW_INTERFACE_INFO_RA_RDNS,
	IMSG_CTL_SHOW_INTERFACE_INFO_RA_DNSSL,
	IMSG_CTL_END,
	IMSG_SOCKET_IPC,
	IMSG_STARTUP,
	IMSG_UPDATE_IF,
	IMSG_REMOVE_IF,
	IMSG_RA,
	IMSG_CTL_SEND_SOLICITATION,
};

extern const char* imsg_type_name[];

enum {
	PROC_MAIN,
	PROC_ENGINE,
	PROC_FRONTEND
} slaacd_process;

struct ctl_engine_info {
	uint32_t		if_index;
	int			running;
	int			autoconfprivacy;
	struct ether_addr	hw_address;
};

enum rpref {
	LOW,
	MEDIUM,
	HIGH,
};

struct ctl_engine_info_ra {
	struct sockaddr_in6	 from;
	struct timespec		 when;
	struct timespec		 uptime;
	uint8_t			 curhoplimit;
	int			 managed;
	int			 other;
	enum rpref		 rpref;
	uint16_t		 router_lifetime;	/* in seconds */
	uint32_t		 reachable_time;	/* in milliseconds */
	uint32_t		 retrans_time;		/* in milliseconds */
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

struct imsg_ifinfo {
	uint32_t		if_index;
	int			running;
	int			autoconfprivacy;
	struct ether_addr	hw_address;
};

struct imsg_ra {
	uint32_t		if_index;
	struct sockaddr_in6	from;
	ssize_t			len;
	uint8_t			packet[1500];
};

extern uint32_t	 cmd_opts;
extern char	*csock;

/* slaacd.c */
int	main_imsg_compose_frontend(int, pid_t, void *, uint16_t);
int	main_imsg_compose_engine(int, pid_t, void *, uint16_t);
void	imsg_event_add(struct imsgev *);
int	imsg_compose_event(struct imsgev *, uint16_t, uint32_t, pid_t,
	    int, void *, uint16_t);
