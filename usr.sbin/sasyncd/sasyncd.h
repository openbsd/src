/*	$OpenBSD: sasyncd.h,v 1.1 2005/03/30 18:44:49 ho Exp $	*/

/*
 * Copyright (c) 2005 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * This code was written under funding by Multicom Security AB.
 */


#include <sys/queue.h>

enum RUNSTATE		{ INIT = 0, SLAVE, MASTER, FAIL };
#define CARPSTATES	{ "INIT", "SLAVE", "MASTER", "FAIL" }

struct syncpeer;
struct timeval;

struct cfgstate {
	enum RUNSTATE	 runstate;
	enum RUNSTATE	 lockedstate;
	int		 debug;
	int		 verboselevel;

	char		*carp_ifname;
	int		 carp_check_interval;

	char		*cafile;
	char		*certfile;
	char		*privkeyfile;

	int		 pfkey_socket;

	char		*listen_on;
	in_port_t	 listen_port;

	LIST_HEAD(, syncpeer) peerlist;
};

extern struct cfgstate	cfgstate;
extern char		*__progname;

#define SASYNCD_USER	"_isakmpd"
#define SASYNCD_CFGFILE	"/etc/sasyncd.conf"

#define CARP_DEFAULT_INTERVAL	10

#define SASYNCD_DEFAULT_PORT	501
#define SASYNCD_CAFILE		"/etc/ssl/ca.crt"
#define SASYNCD_CERTFILE	"/etc/ssl/sasyncd.crt"
#define SASYNCD_PRIVKEY		"/etc/ssl/private/sasyncd.key"

/*
 * sasyncd "protocol" definition
 *
 * Message format:
 *   u_int32_t	type
 *   u_int32_t	len
 *   raw        data
 */

/* sasyncd protocol message types */
#define MSG_SYNCCTL	0
#define MSG_PFKEYDATA	1
#define MSG_MAXTYPE	1	/* Increase when new types are added. */

/* conf.c */
int	conf_init(int, char **);

/* carp.c */
void	carp_check_state(void);
int	carp_init(void);

/* log.c */
void	log_init(char *);
void	log_msg(int, const char *, ...);
void	log_err(const char *, ...);

/* net.c */
void	net_ctl_update_state(void);
int	net_init(void);
void	net_handle_messages(fd_set *);
int	net_queue(struct syncpeer *, u_int32_t, u_int8_t *, u_int32_t,
    u_int32_t);
void	net_send_messages(fd_set *);
int	net_set_rfds(fd_set *);
int	net_set_pending_wfds(fd_set *);
void	net_shutdown(void);

/* pfkey.c */
int	pfkey_init(int);
int	pfkey_queue_message(u_int8_t *, u_int32_t);
void	pfkey_read_message(fd_set *);
void	pfkey_send_message(fd_set *);
void	pfkey_set_rfd(fd_set *);
void	pfkey_set_pending_wfd(fd_set *);
int	pfkey_set_promisc(void);
void	pfkey_shutdown(void);
void	pfkey_snapshot(void *);

/* timer.c */
void	timer_init(void);
void	timer_next_event(struct timeval *);
void	timer_run(void);
int	timer_add(char *, u_int32_t, void (*)(void *), void *);

#if defined (GC_DEBUG)
/* Boehms GC */
void    *GC_debug_malloc(size_t, char *, int);
void    *GC_debug_realloc(void *, size_t, char *, int);
void     GC_debug_free(void *);
char    *gc_strdup(const char *);

#define malloc(x)       GC_debug_malloc ((x), __FILE__, __LINE__)
#define realloc(x,y)    GC_debug_realloc ((x), (y), __FILE__, __LINE__)
#define free(x)         GC_debug_free (x)
#define calloc(x,y)     malloc((x) * (y))
#define strdup(x)       gc_strdup((x))

#endif /* WITH_BOEHM_GC */
