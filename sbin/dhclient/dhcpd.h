/*	$OpenBSD: dhcpd.h,v 1.70 2010/06/02 09:57:16 phessler Exp $	*/

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
#include <sys/wait.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "dhcp.h"

#define	LOCAL_PORT	68
#define	REMOTE_PORT	67

struct option {
	char *name;
	char *format;
};

struct option_data {
	unsigned int	 len;
	u_int8_t	*data;
};

struct string_list {
	struct string_list	*next;
	char			string[1];	/* Actually bigger. */
};

struct iaddr {
	int len;
	unsigned char iabuf[16];
};

struct iaddrlist {
	struct iaddrlist *next;
	struct iaddr addr;
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
	time_t			 link_timeout;
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
	char			**scriptEnv;
	int			  scriptEnvsize;
};

struct interface_info {
	struct hardware		 hw_address;
	struct in_addr		 primary_address;
	char			 name[IFNAMSIZ];
	int			 rfdesc;
	int			 wfdesc;
	int			 ufdesc; /* unicast */
	unsigned char		*rbuf;
	size_t			 rbuf_max;
	size_t			 rbuf_offset;
	size_t			 rbuf_len;
	struct ifreq		*ifp;
	int			 noifmedia;
	int			 errors;
	u_int16_t		 index;
	int			 linkstat;
	int			 rdomain;
};

struct timeout {
	struct timeout	*next;
	time_t		 when;
	void		 (*func)(void);
};

#define	_PATH_DHCLIENT_CONF	"/etc/dhclient.conf"
#define	_PATH_DHCLIENT_DB	"/var/db/dhclient.leases"
#define	_PATH_DHCLIENT_SCRIPT	"/sbin/dhclient-script"
#define	DHCPD_LOG_FACILITY	LOG_DAEMON

/* External definitions... */

extern struct interface_info *ifi;
extern struct client_state *client;
extern struct client_config *config;

/* options.c */
int cons_options(unsigned char *, const int, struct option_data *);
char *pretty_print_option(unsigned int, unsigned char *, int, int, int);
void do_packet(int, unsigned int, struct iaddr, struct hardware *);

/* errwarn.c */
extern int warnings_occurred;
void error(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int warning(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int note(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
#ifdef DEBUG
int debug(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
#endif
int parse_warn(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));

/* conflex.c */
extern int lexline, lexchar;
extern char *token_line, *tlname;
void new_parse(char *);
int next_token(char **, FILE *);
int peek_token(char **, FILE *);

/* parse.c */
void skip_to_semi(FILE *);
int parse_semi(FILE *);
char *parse_string(FILE *);
int parse_ip_addr(FILE *, struct iaddr *);
void parse_hardware_param(FILE *, struct hardware *);
void parse_lease_time(FILE *, time_t *);
int parse_numeric_aggregate(FILE *, unsigned char *, int, int, int);
void convert_num(unsigned char *, char *, int, int);
time_t parse_date(FILE *);

/* bpf.c */
int if_register_bpf(void);
void if_register_send(void);
void if_register_receive(void);
ssize_t send_packet(struct in_addr, struct sockaddr_in *, struct hardware *);
ssize_t receive_packet(struct sockaddr_in *, struct hardware *);

/* dispatch.c */
void discover_interface(void);
void reinitialize_interface(void);
void dispatch(void);
void got_one(void);
void add_timeout(time_t, void (*)(void));
void cancel_timeout(void (*)(void));
int interface_link_status(char *);
int interface_status(char *);
int interface_link_forceup(char *);
void interface_link_forcedown(char *);
int get_rdomain(char *);

/* tables.c */
extern const struct option dhcp_options[256];

/* convert.c */
u_int32_t getULong(unsigned char *);
int32_t getLong(unsigned char *);
u_int16_t getUShort(unsigned char *);
int16_t getShort(unsigned char *);
void putULong(unsigned char *, u_int32_t);
void putLong(unsigned char *, int32_t);
void putUShort(unsigned char *, unsigned int);
void putShort(unsigned char *, int);

/* inet.c */
struct iaddr subnet_number(struct iaddr, struct iaddr);
struct iaddr broadcast_addr(struct iaddr, struct iaddr);
int addr_eq(struct iaddr, struct iaddr);
char *piaddr(struct iaddr);

/* dhclient.c */
extern char *path_dhclient_conf;
extern char *path_dhclient_db;
extern time_t cur_time;
extern int log_perror;
extern int routefd;

void dhcpoffer(struct iaddr, struct option_data *);
void dhcpack(struct iaddr, struct option_data *);
void dhcpnak(struct iaddr, struct option_data *);

void send_discover(void);
void send_request(void);
void send_decline(void);

void state_reboot(void);
void state_init(void);
void state_selecting(void);
void state_bound(void);
void state_panic(void);

void bind_lease(void);

void make_discover(struct client_lease *);
void make_request(struct client_lease *);
void make_decline(struct client_lease *);

void free_client_lease(struct client_lease *);
void rewrite_client_leases(void);
void write_client_lease(struct client_lease *, int);

void	 priv_script_init(char *, char *);
void	 priv_script_write_params(char *, struct client_lease *);
int	 priv_script_go(void);

void script_init(char *, struct string_list *);
void script_write_params(char *, struct client_lease *);
int script_go(void);
void script_set_env(const char *, const char *, const char *);
void script_flush_env(void);
int dhcp_option_ev_name(char *, size_t, const struct option *);

struct client_lease *packet_to_lease(struct iaddr, struct option_data *);
void go_daemon(void);

void routehandler(void);

/* packet.c */
void assemble_hw_header(unsigned char *, int *, struct hardware *);
void assemble_udp_ip_header(unsigned char *, int *, u_int32_t, u_int32_t,
    unsigned int, unsigned char *, int);
ssize_t decode_hw_header(unsigned char *, int, struct hardware *);
ssize_t decode_udp_ip_header(unsigned char *, int, struct sockaddr_in *,
    unsigned char *, int);

/* clparse.c */
int read_client_conf(void);
void read_client_leases(void);
void parse_client_statement(FILE *);
int parse_X(FILE *, u_int8_t *, int);
int parse_option_list(FILE *, u_int8_t *);
void parse_interface_declaration(FILE *);
void parse_client_lease_statement(FILE *, int);
void parse_client_lease_declaration(FILE *, struct client_lease *);
int parse_option_decl(FILE *, struct option_data *);
void parse_string_list(FILE *, struct string_list **, int);
void parse_reject_statement(FILE *);
