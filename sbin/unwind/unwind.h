/*	$OpenBSD: unwind.h,v 1.56 2021/08/31 20:18:03 kn Exp $	*/

/*
 * Copyright (c) 2018 Florian Obser <florian@openbsd.org>
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

#include <sys/types.h>
#include <sys/tree.h>
#include <netinet/in.h>	/* INET6_ADDRSTRLEN */
#include <event.h>
#include <imsg.h>
#include <netdb.h>	/* NI_MAXHOST */
#include <stdint.h>

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define _PATH_CONF_FILE		"/etc/unwind.conf"
#define	_PATH_UNWIND_SOCKET	"/dev/unwind.sock"
#define UNWIND_USER		"_unwind"

#define OPT_VERBOSE	0x00000001
#define OPT_VERBOSE2	0x00000002
#define OPT_VERBOSE3	0x00000004
#define OPT_NOACTION	0x00000008

#define	ROOT_DNSKEY_TTL	172800	/* TTL from authority */
#define	KSK2017		".	172800	IN	DNSKEY	257 3 8 AwEAAaz/tAm8yTn4Mfeh5eyI96WSVexTBAvkMgJzkKTOiW1vkIbzxeF3+/4RgWOq7HrxRixHlFlExOLAJr5emLvN7SWXgnLh4+B5xQlNVz8Og8kvArMtNROxVQuCaSnIDdD5LKyWbRd2n9WGe2R8PzgCmr3EgVLrjyBxWezF0jLHwVN8efS3rCj/EWgvIWgb9tarpVUDK/b58Da+sqqls3eNbuv7pr+eoZG+SrDK6nWeL3c6H5Apxz7LjVc1uTIdsIXxuOLYA4/ilBmSVIzuDWfdRUfhHdY6+cn8HFRm+2hM8AnXGXws9555KrUB5qihylGa8subX2Nn6UwNR1AkUTV74bU="

#define	IMSG_DATA_SIZE(imsg)	((imsg).hdr.len - IMSG_HEADER_SIZE)

enum uw_resolver_type {
	UW_RES_RECURSOR,
	UW_RES_AUTOCONF,
	UW_RES_ODOT_AUTOCONF,
	UW_RES_ASR,
	UW_RES_FORWARDER,
	UW_RES_ODOT_FORWARDER,
	UW_RES_DOT,
	UW_RES_NONE
};

static const char * const	uw_resolver_type_str[] = {
	"recursor",
	"autoconf",
	"oDoT-autoconf",
	"stub",
	"forwarder",
	"oDoT-forwarder",
	"DoT"
};

static const char * const	uw_resolver_type_short[] = {
	"rec",
	"auto",
	"auto*",
	"stub",
	"forw",
	"forw*",
	"DoT"
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
	IMSG_CTL_RELOAD,
	IMSG_CTL_STATUS,
	IMSG_CTL_AUTOCONF,
	IMSG_CTL_MEM,
	IMSG_RECONF_CONF,
	IMSG_RECONF_BLOCKLIST_FILE,
	IMSG_RECONF_FORWARDER,
	IMSG_RECONF_DOT_FORWARDER,
	IMSG_RECONF_FORCE,
	IMSG_RECONF_END,
	IMSG_UDP4SOCK,
	IMSG_UDP6SOCK,
	IMSG_TCP4SOCK,
	IMSG_TCP6SOCK,
	IMSG_ROUTESOCK,
	IMSG_CONTROLFD,
	IMSG_STARTUP,
	IMSG_STARTUP_DONE,
	IMSG_SOCKET_IPC_FRONTEND,
	IMSG_SOCKET_IPC_RESOLVER,
	IMSG_QUERY,
	IMSG_ANSWER,
	IMSG_CTL_RESOLVER_INFO,
	IMSG_CTL_AUTOCONF_RESOLVER_INFO,
	IMSG_CTL_MEM_INFO,
	IMSG_CTL_END,
	IMSG_HTTPSOCK,
	IMSG_TAFD,
	IMSG_NEW_TA,
	IMSG_NEW_TAS_ABORT,
	IMSG_NEW_TAS_DONE,
	IMSG_NETWORK_CHANGED,
	IMSG_BLFD,
	IMSG_REPLACE_DNS,
	IMSG_NEW_DNS64_PREFIXES_START,
	IMSG_NEW_DNS64_PREFIX,
	IMSG_NEW_DNS64_PREFIXES_DONE,
	IMSG_CHANGE_AFS,
};

struct uw_forwarder {
	TAILQ_ENTRY(uw_forwarder)		 entry;
	char					 ip[INET6_ADDRSTRLEN];
	char					 auth_name[NI_MAXHOST];
	uint16_t				 port;
	uint32_t				 if_index;
	int					 src;
};

struct force_tree_entry {
	RB_ENTRY(force_tree_entry)	 entry;
	char				 domain[NI_MAXHOST];
	enum uw_resolver_type		 type;
	int				 acceptbogus;
};

RB_HEAD(force_tree, force_tree_entry);

struct resolver_preference {
	enum uw_resolver_type			 types[UW_RES_NONE];
	int					 len;
};

TAILQ_HEAD(uw_forwarder_head, uw_forwarder);
struct uw_conf {
	struct uw_forwarder_head	 uw_forwarder_list;
	struct uw_forwarder_head	 uw_dot_forwarder_list;
	struct force_tree		 force;
	struct resolver_preference	 res_pref;
	int				 enabled_resolvers[UW_RES_NONE];
	char				*blocklist_file;
	int				 blocklist_log;
};

struct query_imsg {
	uint64_t	 id;
	char		 qname[NI_MAXHOST];
	int		 t;
	int		 c;
	struct timespec	 tp;
};

struct answer_header {
	uint64_t id;
	int	 srvfail;
	int	 bogus;
	int	 answer_len;
};

extern uint32_t	 cmd_opts;

/* unwind.c */
void	main_imsg_compose_frontend(int, pid_t, void *, uint16_t);
void	main_imsg_compose_frontend_fd(int, pid_t, int);
void	main_imsg_compose_resolver(int, pid_t, void *, uint16_t);
void	merge_config(struct uw_conf *, struct uw_conf *);
void	imsg_event_add(struct imsgev *);
int	imsg_compose_event(struct imsgev *, uint16_t, uint32_t, pid_t,
	    int, void *, uint16_t);
void	imsg_receive_config(struct imsg *, struct uw_conf **);

struct uw_conf	*config_new_empty(void);
void		 config_clear(struct uw_conf *);

/* printconf.c */
void	print_config(struct uw_conf *);

/* parse.y */
struct uw_conf	*parse_config(char *);
int		 cmdline_symset(char *);

RB_PROTOTYPE(force_tree, force_tree_entry, entry, force_tree_cmp);
