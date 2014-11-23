/*	$OpenBSD: dhcpd.h,v 1.144 2014/11/23 18:22:45 krw Exp $	*/

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
#include <sys/queue.h>

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
#define	INTERNALSIG	INT_MAX
#define DB_TIMEFMT	"%w %Y/%m/%d %T UTC"
#define BAD_DB_TIMEFMT	"%u %Y/%m/%d %T"
#define OLD_DB_TIMEFMT	"%w %Y/%m/%d %T"

struct option {
	char *name;
	char *format;
};

struct option_data {
	unsigned int	 len;
	u_int8_t	*data;
};

struct reject_elem {
	TAILQ_ENTRY(reject_elem) next;
	struct in_addr		 addr;
};

struct client_lease {
	TAILQ_ENTRY(client_lease) next;
	time_t			 expiry, renewal, rebind;
	struct in_addr		 address;
	struct in_addr		 next_server;
	char			*server_name;
	char			*filename;
	char			*resolv_conf;
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

	struct in_addr		 address;
	struct in_addr		 next_server;
	struct option_data	 send_options[256];
	u_int8_t		 required_options[256];
	u_int8_t		 requested_options[256];
	u_int8_t		 ignored_options[256];
	int			 requested_option_count;
	int			 required_option_count;
	int			 ignored_option_count;
	time_t			 timeout;
	time_t			 initial_interval;
	time_t			 link_timeout;
	time_t			 retry_interval;
	time_t			 select_interval;
	time_t			 reboot_timeout;
	time_t			 backoff_cutoff;
	enum { IGNORE, ACCEPT, PREFER }
				 bootp_policy;
	TAILQ_HEAD(, reject_elem) reject_list;
	char			*resolv_tail;
	char			*filename;
	char			*server_name;
};

struct client_state {
	struct client_lease	*active;
	struct client_lease	*new;
	TAILQ_HEAD(, client_lease) offered_leases;
	TAILQ_HEAD(_leases, client_lease) leases;
	enum dhcp_state		 state;
	struct in_addr		 destination;
	int			 flags;
#define IS_RESPONSIBLE	0x1
	u_int32_t		 xid;
	u_int16_t		 secs;
	time_t			 first_sending;
	time_t			 interval;
	struct dhcp_packet	 packet;
	struct dhcp_packet	 bootrequest_packet;
	int			 bootrequest_packet_length;
	struct in_addr		 requested_address;
};

struct interface_info {
	struct ether_addr	hw_address;
	char		 name[IFNAMSIZ];
	int		 bfdesc; /* bpf - reading & broadcast writing*/
	int		 ufdesc; /* udp - unicast writing */
	unsigned char	*rbuf;
	size_t		 rbuf_max;
	size_t		 rbuf_offset;
	size_t		 rbuf_len;
	int		 errors;
	u_int16_t	 index;
	int		 linkstat;
	int		 rdomain;
	int		 flags;
#define IFI_NEW_LLADDR	0x00000001
#define IFI_HUP		0x00000002
#define IFI_NOMEDIA	0x00000004
};

struct dhcp_timeout {
	time_t	 when;
	void	 (*func)(void);
};

#define	_PATH_DHCLIENT_CONF	"/etc/dhclient.conf"
#define	_PATH_DHCLIENT_DB	"/var/db/dhclient.leases"

/* External definitions. */

extern struct interface_info *ifi;
extern struct client_state *client;
extern struct client_config *config;
extern struct imsgbuf *unpriv_ibuf;
extern struct in_addr deleting;
extern struct in_addr adding;
extern struct in_addr active_addr;
extern volatile sig_atomic_t quit;

/* options.c */
int cons_options(struct option_data *);
char *pretty_print_option(unsigned int, struct option_data *, int);
int pretty_print_string(unsigned char *, size_t, unsigned char *, size_t, int);
void do_packet(unsigned int, struct in_addr, struct ether_addr *);

/* errwarn.c */
extern int warnings_occurred;
void error(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
void warning(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
void note(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
#ifdef DEBUG
void debug(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
#endif
void parse_warn(char *);

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
int parse_ip_addr(FILE *, struct in_addr *);
int parse_cidr(FILE *, unsigned char *);
void parse_ethernet(FILE *, struct ether_addr *);
void parse_lease_time(FILE *, time_t *);
int parse_decimal(FILE *, unsigned char *, char);
int parse_hex(FILE *, unsigned char *);
time_t parse_date(FILE *);

/* bpf.c */
void if_register_send(void);
void if_register_receive(void);
ssize_t send_packet(struct in_addr, struct in_addr);
ssize_t receive_packet(struct sockaddr_in *, struct ether_addr *);

/* dispatch.c */
void dispatch(void);
void set_timeout(time_t, void (*)(void));
void set_timeout_interval(time_t, void (*)(void));
void cancel_timeout(void);
void interface_link_forceup(char *);
int interface_status(char *);
int get_rdomain(char *);
void get_hw_address(void);
int subnet_exists(struct client_lease *);

/* tables.c */
extern const struct option dhcp_options[256];

/* dhclient.c */
extern char *path_dhclient_conf;
extern char *path_dhclient_db;
extern int log_perror;
extern int routefd;

void dhcpoffer(struct in_addr, struct option_data *, char *);
void dhcpack(struct in_addr, struct option_data *, char *);
void dhcpnak(struct in_addr, struct option_data *, char *);

void free_client_lease(struct client_lease *);

void routehandler(void);

/* packet.c */
void assemble_eh_header(struct ether_header *);
ssize_t decode_hw_header(unsigned char *, int, struct ether_addr *);
ssize_t decode_udp_ip_header(unsigned char *, int, struct sockaddr_in *,
    int);
u_int32_t checksum(unsigned char *, unsigned, u_int32_t);
u_int32_t wrapsum(u_int32_t);

/* clparse.c */
void read_client_conf(void);
void read_client_leases(void);

/* kroute.c */
void delete_addresses(char *, int);
void delete_address(char *, int, struct in_addr);

void add_address(char *, int, struct in_addr, struct in_addr);

void flush_routes(char *, int);

void add_route(int, struct in_addr, struct in_addr, struct in_addr, int, int);

void sendhup(struct client_lease *);

int resolv_conf_priority(int);

void flush_unpriv_ibuf(const char *);
