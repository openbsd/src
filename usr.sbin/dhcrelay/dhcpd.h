/*	$OpenBSD: dhcpd.h,v 1.2 2004/04/13 01:22:30 henning Exp $	*/

/*
 * Copyright (c) 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.    All rights reserved.
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
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include <sys/types.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "dhcp.h"
#include "tree.h"

#define	LOCAL_PORT	68
#define	REMOTE_PORT	67

struct option_data {
	int		 len;
	u_int8_t	*data;
};

struct string_list {
	struct string_list	*next;
	char			*string;
};

struct iaddr {
	int len;
	unsigned char iabuf[16];
};

struct iaddrlist {
	struct iaddrlist *next;
	struct iaddr addr;
};

struct packet {
	struct dhcp_packet	*raw;
	int			 packet_length;
	int			 packet_type;
	int			 options_valid;
	int			 client_port;
	struct iaddr		 client_addr;
	struct interface_info	*interface;
	struct hardware		*haddr;
	struct option_data	 options[256];
	int			 got_requested_address;
};

struct hardware {
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t haddr[16];
};

struct client_lease {
	struct client_lease	*next;
	time_t			 expiry, renewal, rebind;
	struct iaddr		 address;
	char			*server_name;
	char			*filename;
	struct string_list	*medium;
	unsigned int		 is_static : 1;
	unsigned int		 is_bootp : 1;
	struct option_data	 options[256];
};

/* Possible states in which the client can be. */
enum dhcp_state {
	S_REBOOTING,
	S_INIT,
	S_SELECTING,
	S_REQUESTING,
	S_BOUND,
	S_RENEWING,
	S_REBINDING
};

struct client_config {
	struct option_data	defaults[256];
	enum {
		ACTION_DEFAULT,
		ACTION_SUPERSEDE,
		ACTION_PREPEND,
		ACTION_APPEND
	} default_actions[256];

	struct option_data	 send_options[256];
	u_int8_t		 required_options[256];
	u_int8_t		 requested_options[256];
	int			 requested_option_count;
	time_t			 timeout;
	time_t			 initial_interval;
	time_t			 retry_interval;
	time_t			 select_interval;
	time_t			 reboot_timeout;
	time_t			 backoff_cutoff;
	struct string_list	*media;
	char			*script_name;
	enum { IGNORE, ACCEPT, PREFER }
				 bootp_policy;
	struct string_list	*medium;
	struct iaddrlist	*reject_list;
};

struct client_state {
	struct client_lease	 *active;
	struct client_lease	 *new;
	struct client_lease	 *offered_leases;
	struct client_lease	 *leases;
	struct client_lease	 *alias;
	enum dhcp_state		  state;
	struct iaddr		  destination;
	u_int32_t		  xid;
	u_int16_t		  secs;
	time_t			  first_sending;
	time_t			  interval;
	struct string_list	 *medium;
	struct dhcp_packet	  packet;
	int			  packet_length;
	struct iaddr		  requested_address;
	struct client_config	 *config;
	char			**scriptEnv;
	int			  scriptEnvsize;
	struct string_list	 *env;
	int			  envc;
};

struct interface_info {
	struct interface_info	*next;
	struct hardware		 hw_address;
	struct in_addr		 primary_address;
	char			 name[IFNAMSIZ];
	int			 rfdesc;
	int			 wfdesc;
	unsigned char		*rbuf;
	size_t			 rbuf_max;
	size_t			 rbuf_offset;
	size_t			 rbuf_len;
	struct ifreq		*ifp;
	struct client_state	*client;
	int			 noifmedia;
	int			 errors;
	int			 dead;
	u_int16_t		 index;
};

struct timeout {
	struct timeout	*next;
	time_t		 when;
	void		 (*func)(void *);
	void		*what;
};

struct protocol {
	struct protocol	*next;
	int fd;
	void (*handler)(struct protocol *);
	void *local;
};

#define DEFAULT_HASH_SIZE 97

struct hash_bucket {
	struct hash_bucket *next;
	unsigned char *name;
	int len;
	unsigned char *value;
};

struct hash_table {
	int hash_count;
	struct hash_bucket *buckets[DEFAULT_HASH_SIZE];
};

/* Default path to dhcpd config file. */
#define	DHCPD_LOG_FACILITY	LOG_DAEMON

#define	MAX_TIME 0x7fffffff
#define	MIN_TIME 0

/* External definitions... */

/* errwarn.c */
extern int warnings_occurred;
void error(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int warn(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int note(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int debug(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));

/* bpf.c */
int if_register_bpf(struct interface_info *);
void if_register_send(struct interface_info *);
void if_register_receive(struct interface_info *);
ssize_t send_packet(struct interface_info *,
    struct packet *, struct dhcp_packet *, size_t, struct in_addr,
    struct sockaddr_in *, struct hardware *);
ssize_t receive_packet(struct interface_info *, unsigned char *, size_t,
    struct sockaddr_in *, struct hardware *);

/* dispatch.c */
extern void (*bootp_packet_handler)(struct interface_info *,
    struct dhcp_packet *, int, unsigned int, struct iaddr, struct hardware *);
void discover_interfaces(struct interface_info *);
void reinitialize_interfaces(void);
void dispatch(void);
void got_one(struct protocol *);
void add_timeout(time_t, void (*)(void *), void *);
void cancel_timeout(void (*)(void *), void *);
void add_protocol(char *, int, void (*)(struct protocol *), void *);
void remove_protocol(struct protocol *);
int interface_link_status(char *);

/* packet.c */
void assemble_hw_header(struct interface_info *, unsigned char *,
    int *, struct hardware *);
void assemble_udp_ip_header(struct interface_info *, unsigned char *,
    int *, u_int32_t, u_int32_t, unsigned int, unsigned char *, int);
ssize_t decode_hw_header(struct interface_info *, unsigned char *,
    int, struct hardware *);
ssize_t decode_udp_ip_header(struct interface_info *, unsigned char *,
    int, struct sockaddr_in *, unsigned char *, int);


/* crap */
extern time_t cur_time;
extern int log_priority;
extern int log_perror;
