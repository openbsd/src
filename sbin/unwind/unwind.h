/*	$OpenBSD: unwind.h,v 1.6 2019/02/03 12:02:30 florian Exp $	*/

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


#include <event.h>
#include <imsg.h>
#include <stdint.h>

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define CONF_FILE		"/etc/unwind.conf"
#define	UNWIND_SOCKET		"/var/run/unwind.sock"
#define UNWIND_USER		"_unwind"

#define OPT_VERBOSE	0x00000001
#define OPT_VERBOSE2	0x00000002
#define OPT_NOACTION	0x00000004

enum {
	PROC_MAIN,
	PROC_RESOLVER,
	PROC_FRONTEND,
	PROC_CAPTIVEPORTAL,
} unwind_process;

static const char * const log_procnames[] = {
	"main",
	"resolver",
	"frontend",
	"captive portal",
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
	IMSG_CTL_CAPTIVEPORTAL_INFO,
	IMSG_RECONF_CONF,
	IMSG_RECONF_CAPTIVE_PORTAL_HOST,
	IMSG_RECONF_CAPTIVE_PORTAL_PATH,
	IMSG_RECONF_CAPTIVE_PORTAL_EXPECTED_RESPONSE,
	IMSG_RECONF_FORWARDER,
	IMSG_RECONF_DOT_FORWARDER,
	IMSG_RECONF_END,
	IMSG_UDP4SOCK,
	IMSG_UDP6SOCK,
	IMSG_ROUTESOCK,
	IMSG_CONTROLFD,
	IMSG_STARTUP,
	IMSG_STARTUP_DONE,
	IMSG_SOCKET_IPC_FRONTEND,
	IMSG_SOCKET_IPC_RESOLVER,
	IMSG_SOCKET_IPC_CAPTIVEPORTAL,
	IMSG_QUERY,
	IMSG_ANSWER_HEADER,
	IMSG_ANSWER,
	IMSG_OPEN_DHCP_LEASE,
	IMSG_LEASEFD,
	IMSG_FORWARDER,
	IMSG_RESOLVER_DOWN,
	IMSG_RESOLVER_UP,
	IMSG_OPEN_PORTS,
	IMSG_CTL_RESOLVER_INFO,
	IMSG_CTL_RESOLVER_WHY_BOGUS,
	IMSG_CTL_RESOLVER_HISTOGRAM,
	IMSG_CTL_END,
	IMSG_CTL_RECHECK_CAPTIVEPORTAL,
	IMSG_OPEN_HTTP_PORT,
	IMSG_HTTPSOCK,
	IMSG_CAPTIVEPORTAL_STATE
};

struct unwind_forwarder {
	SIMPLEQ_ENTRY(unwind_forwarder)		 entry;
	char					 name[1024]; /* XXX */
};

struct unwind_conf {
	SIMPLEQ_HEAD(unwind_forwarder_head, unwind_forwarder)	 unwind_forwarder_list;
	struct unwind_forwarder_head	 unwind_dot_forwarder_list;
	int				 unwind_options;
	char				*captive_portal_host;
	char				*captive_portal_path;
	char				*captive_portal_expected_response;
	int				 captive_portal_expected_status;
	int				 captive_portal_auto;
};

struct query_imsg {
	uint64_t	 id;
	char		 qname[255];
	int		 t;
	int		 c;
	int		 err;
	int		 bogus;
	int		 async_id;
	void		*resolver;
	struct timespec	 tp;
};

extern uint32_t	 cmd_opts;

/* unwind.c */
void	main_imsg_compose_frontend(int, pid_t, void *, uint16_t);
void	main_imsg_compose_frontend_fd(int, pid_t, int);
void	main_imsg_compose_resolver(int, pid_t, void *, uint16_t);
void	main_imsg_compose_captiveportal(int, pid_t, void *, uint16_t);
void	main_imsg_compose_captiveportal_fd(int, pid_t, int);
void	merge_config(struct unwind_conf *, struct unwind_conf *);
void	imsg_event_add(struct imsgev *);
int	imsg_compose_event(struct imsgev *, uint16_t, uint32_t, pid_t,
	    int, void *, uint16_t);

struct unwind_conf	*config_new_empty(void);
void			 config_clear(struct unwind_conf *);

/* printconf.c */
void	print_config(struct unwind_conf *);

/* parse.y */
struct unwind_conf	*parse_config(char *);
int			 cmdline_symset(char *);
