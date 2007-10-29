/*	$OpenBSD: dhcpd.h,v 1.34 2007/10/29 16:51:02 krw Exp $ */

/*
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
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sockio.h>
#include <sys/time.h>

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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define ifr_netmask ifr_addr

#define HAVE_SA_LEN
#define HAVE_MKSTEMP

#include "dhcp.h"
#include "tree.h"

#define SERVER_PORT	67
#define CLIENT_PORT	68

struct iaddr {
	int len;
	unsigned char iabuf[16];
};

struct iaddrlist {
	struct iaddrlist *next;
	struct iaddr addr;
};

#define DEFAULT_HASH_SIZE	97

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

struct option_data {
	int len;
	u_int8_t *data;
};

struct string_list {
	struct string_list *next;
	char *string;
};

/* A name server, from /etc/resolv.conf. */
struct name_server {
	struct name_server *next;
	struct sockaddr_in addr;
	time_t rcdate;
};

/* A domain search list element. */
struct domain_search_list {
	struct domain_search_list *next;
	char *domain;
	time_t rcdate;
};

/* A dhcp packet and the pointers to its option values. */
struct packet {
	struct dhcp_packet *raw;
	int packet_length;
	int packet_type;
	int options_valid;
	int client_port;
	struct iaddr client_addr;
	struct interface_info *interface;	/* Interface on which packet
						   was received. */
	struct hardware *haddr;		/* Physical link address
					   of local sender (maybe gateway). */
	struct shared_network *shared_network;
	struct option_data options[256];
	int got_requested_address;	/* True if client sent the
					   dhcp-requested-address option. */
};

struct hardware {
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t haddr[16];
};

/* A dhcp lease declaration structure. */
struct lease {
	struct lease *next;
	struct lease *prev;
	struct lease *n_uid, *n_hw;
	struct lease *waitq_next;

	struct iaddr ip_addr;
	time_t starts, ends, timestamp;
	unsigned char *uid;
	int uid_len;
	int uid_max;
	unsigned char uid_buf[32];
	char *hostname;
	char *client_hostname;
	struct host_decl *host;
	struct subnet *subnet;
	struct shared_network *shared_network;
	struct hardware hardware_addr;

	int flags;
#define STATIC_LEASE		1
#define BOOTP_LEASE		2
#define DYNAMIC_BOOTP_OK	4
#define PERSISTENT_FLAGS	(DYNAMIC_BOOTP_OK)
#define EPHEMERAL_FLAGS		(BOOTP_LEASE)
#define MS_NULL_TERMINATION	8
#define ABANDONED_LEASE		16

	struct lease_state *state;
	u_int8_t releasing;
};

struct lease_state {
	struct lease_state *next;

	struct interface_info *ip;

	time_t offered_expiry;

	struct tree_cache *options[256];
	u_int32_t expiry, renewal, rebind;
	char filename[DHCP_FILE_LEN];
	char *server_name;

	struct iaddr from;

	int max_message_size;
	u_int8_t *prl;
	int prl_len;
	int got_requested_address;	/* True if client sent the
					   dhcp-requested-address option. */
	int got_server_identifier;	/* True if client sent the
					   dhcp-server-identifier option. */
	struct shared_network *shared_network;	/* Shared network of interface
						   on which request arrived. */

	u_int32_t xid;
	u_int16_t secs;
	u_int16_t bootp_flags;
	struct in_addr ciaddr;
	struct in_addr giaddr;
	u_int8_t hops;
	u_int8_t offer;
	struct hardware haddr;
};

#define	ROOT_GROUP	0
#define HOST_DECL	1
#define SHARED_NET_DECL	2
#define SUBNET_DECL	3
#define CLASS_DECL	4
#define	GROUP_DECL	5

/* Group of declarations that share common parameters. */
struct group {
	struct group *next;

	struct subnet *subnet;
	struct shared_network *shared_network;

	time_t default_lease_time;
	time_t max_lease_time;
	time_t bootp_lease_cutoff;
	time_t bootp_lease_length;

	char *filename;
	char *server_name;
	struct iaddr next_server;

	int boot_unknown_clients;
	int dynamic_bootp;
	int allow_bootp;
	int allow_booting;
	int get_lease_hostnames;
	int use_host_decl_names;
	int use_lease_addr_for_default_route;
	int authoritative;
	int always_reply_rfc1048;

	struct tree_cache *options[256];
};

/* A dhcp host declaration structure. */
struct host_decl {
	struct host_decl *n_ipaddr;
	char *name;
	struct hardware interface;
	struct tree_cache *fixed_addr;
	struct group *group;
};

struct shared_network {
	struct shared_network *next;
	char *name;
	struct subnet *subnets;
	struct interface_info *interface;
	struct lease *leases;
	struct lease *insertion_point;
	struct lease *last_lease;

	struct group *group;
};

struct subnet {
	struct subnet *next_subnet;
	struct subnet *next_sibling;
	struct shared_network *shared_network;
	struct interface_info *interface;
	struct iaddr interface_address;
	struct iaddr net;
	struct iaddr netmask;

	struct group *group;
};

struct class {
	char *name;

	struct group *group;
};

/* DHCP client lease structure... */
struct client_lease {
	struct client_lease *next;		/* Next lease in list. */
	time_t expiry, renewal, rebind;		/* Lease timeouts. */
	struct iaddr address;			/* Address being leased. */
	char *server_name;			/* Name of boot server. */
	char *filename;				/* File to boot. */
	struct string_list *medium;		/* Network medium. */

	unsigned int is_static : 1;	/* If set, lease is from config file. */
	unsigned int is_bootp: 1;	/* If set, lease was aquired with BOOTP. */

	struct option_data options[256];	/* Options supplied with lease. */
};

/* privsep message. fixed length for easy parsing */
struct pf_cmd {
	struct in_addr ip;
	u_int32_t type;
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

/* Configuration information from the config file... */
struct client_config {
	struct option_data defaults[256]; /* Default values for options. */
	enum {
		ACTION_DEFAULT,		/* Use server value if present,
					   otherwise default. */
		ACTION_SUPERSEDE,	/* Always use default. */
		ACTION_PREPEND,		/* Prepend default to server. */
		ACTION_APPEND		/* Append default to server. */
	} default_actions[256];

	struct option_data send_options[256]; /* Send these to server. */
	u_int8_t required_options[256]; /* Options server must supply. */
	u_int8_t requested_options[256]; /* Options to request from server. */
	int requested_option_count;	/* Number of requested options. */
	time_t timeout;			/* Start to panic if we don't get a
					   lease in this time period when
					   SELECTING. */
	time_t initial_interval;	/* All exponential backoff intervals
					   start here. */
	time_t retry_interval;		/* If the protocol failed to produce
					   an address before the timeout,
					   try the protocol again after this
					   many seconds. */
	time_t select_interval;		/* Wait this many seconds from the
					   first DHCPDISCOVER before
					   picking an offered lease. */
	time_t reboot_timeout;		/* When in INIT-REBOOT, wait this
					   long before giving up and going
					   to INIT. */
	time_t backoff_cutoff;		/* When doing exponential backoff,
					   never back off to an interval
					   longer than this amount. */
	struct string_list *media;	/* Possible network media values. */
	char *script_name;		/* Name of config script. */
	enum { IGNORE, ACCEPT, PREFER } bootp_policy;
					/* Ignore, accept or prefer BOOTP
					   responses. */
	struct string_list *medium;	/* Current network medium. */

	struct iaddrlist *reject_list;	/* Servers to reject. */
};

/* Per-interface state used in the dhcp client... */
struct client_state {
	struct client_lease *active;		  /* Currently active lease. */
	struct client_lease *new;			       /* New lease. */
	struct client_lease *offered_leases;	    /* Leases offered to us. */
	struct client_lease *leases;		/* Leases we currently hold. */
	struct client_lease *alias;			     /* Alias lease. */

	enum dhcp_state state;		/* Current state for this interface. */
	struct iaddr destination;		    /* Where to send packet. */
	u_int32_t xid;					  /* Transaction ID. */
	u_int16_t secs;			    /* secs value from DHCPDISCOVER. */
	time_t first_sending;			/* When was first copy sent? */
	time_t interval;	      /* What's the current resend interval? */
	struct string_list *medium;		   /* Last media type tried. */

	struct dhcp_packet packet;		    /* Outgoing DHCP packet. */
	int packet_length;	       /* Actual length of generated packet. */

	struct iaddr requested_address;	    /* Address we would like to get. */

	struct client_config *config;	    /* Information from config file. */

	char **scriptEnv;  /* Client script env */
	int scriptEnvsize; /* size of the env table */

	struct string_list *env;	       /* Client script environment. */
	int envc;			/* Number of entries in environment. */
};

/* Information about each network interface. */

struct interface_info {
	struct interface_info *next;	/* Next interface in list... */
	struct shared_network *shared_network;
				/* Networks connected to this interface. */
	struct hardware hw_address;	/* Its physical address. */
	struct in_addr primary_address;	/* Primary interface address. */
	char name[IFNAMSIZ];		/* Its name... */
	int rfdesc;			/* Its read file descriptor. */
	int wfdesc;			/* Its write file descriptor, if
					   different. */
	unsigned char *rbuf;		/* Read buffer, if required. */
	size_t rbuf_max;		/* Size of read buffer. */
	size_t rbuf_offset;		/* Current offset into buffer. */
	size_t rbuf_len;		/* Length of data in buffer. */

	struct ifreq *ifp;		/* Pointer to ifreq struct. */

	/* Only used by DHCP client code. */
	struct client_state *client;
	int noifmedia;
	int errors;
	int dead;
	u_int16_t	index;
};

struct hardware_link {
	struct hardware_link *next;
	char name[IFNAMSIZ];
	struct hardware address;
};

struct dhcpd_timeout {
	struct dhcpd_timeout *next;
	time_t when;
	void (*func)(void *);
	void *what;
};

struct protocol {
	struct protocol *next;
	int fd;
	void (*handler)(struct protocol *);
	void *local;
};

/* Bitmask of dhcp option codes. */
typedef unsigned char option_mask[16];

/* DHCP Option mask manipulation macros... */
#define OPTION_ZERO(mask)	(memset (mask, 0, 16))
#define OPTION_SET(mask, bit)	(mask[bit >> 8] |= (1 << (bit & 7)))
#define OPTION_CLR(mask, bit)	(mask[bit >> 8] &= ~(1 << (bit & 7)))
#define OPTION_ISSET(mask, bit)	(mask[bit >> 8] & (1 << (bit & 7)))
#define OPTION_ISCLR(mask, bit)	(!OPTION_ISSET (mask, bit))

/* An option occupies its length plus two header bytes (code and
    length) for every 255 bytes that must be stored. */
#define OPTION_SPACE(x)		((x) + 2 * ((x) / 255 + 1))

#define _PATH_DHCPD_CONF	"/etc/dhcpd.conf"
#define _PATH_DHCPD_DB		"/var/db/dhcpd.leases"
#define _PATH_DEV_PF		"/dev/pf"
#define DHCPD_LOG_FACILITY	LOG_DAEMON

#define MAX_TIME 0x7fffffff
#define MIN_TIME 0

/* External definitions... */

/* options.c */
void	 parse_options(struct packet *);
void	 parse_option_buffer(struct packet *, unsigned char *, int);
int	 cons_options(struct packet *, struct dhcp_packet *, int,
	    struct tree_cache **, int, int, int, u_int8_t *, int);
int	 store_options(unsigned char *, int, struct tree_cache **,
	    unsigned char *, int, int, int);
char	*pretty_print_option(unsigned int, unsigned char *, int, int, int);
void	 do_packet(struct interface_info *, struct dhcp_packet *, int,
	    unsigned int, struct iaddr, struct hardware *);

/* errwarn.c */
extern int warnings_occurred;
void	error(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int	warning(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int	note(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int	debug(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int	parse_warn(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));

/* dhcpd.c */
extern time_t		cur_time;
extern struct group	root_group;

extern u_int16_t	server_port;
extern u_int16_t	client_port;
extern int		log_priority;
extern int		log_perror;

extern char		*path_dhcpd_conf;
extern char		*path_dhcpd_db;

int	main(int, char *[]);
void	cleanup(void);
void	lease_pinged(struct iaddr, u_int8_t *, int);
void	lease_ping_timeout(void *);
void	periodic_scan(void *);

/* conflex.c */
extern int	 lexline, lexchar;
extern char	*token_line, *tlname;
extern char	 comments[4096];
extern int	 comment_index;
extern int	 eol_token;

void	new_parse(char *);
int	next_token(char **, FILE *);
int	peek_token(char **, FILE *);

/* confpars.c */
int	 readconf(void);
void	 read_leases(void);
int	 parse_statement(FILE *, struct group *, int, struct host_decl *, int);
void	 parse_allow_deny(FILE *, struct group *, int);
void	 skip_to_semi(FILE *);
int	 parse_boolean(FILE *);
int	 parse_semi(FILE *);
int	 parse_lbrace(FILE *);
void	 parse_host_declaration(FILE *, struct group *);
char	*parse_host_name(FILE *);
void	 parse_class_declaration(FILE *, struct group *, int);
void	 parse_lease_time(FILE *, time_t *);
void	 parse_shared_net_declaration(FILE *, struct group *);
void	 parse_subnet_declaration(FILE *, struct shared_network *);
void	 parse_group_declaration(FILE *, struct group *);
void	 parse_hardware_param(FILE *, struct hardware *);
char	*parse_string(FILE *);

struct tree		*parse_ip_addr_or_hostname(FILE *, int);
struct tree_cache	*parse_fixed_addr_param(FILE *);
void			 parse_option_param(FILE *, struct group *);
time_t			 parse_timestamp(FILE *);
struct lease		*parse_lease_declaration(FILE *);
void			 parse_address_range(FILE *, struct subnet *);
time_t			 parse_date(FILE *);
unsigned char		*parse_numeric_aggregate(FILE *, unsigned char *, int *,
			    int, int, int);
void			 convert_num(unsigned char *, char *, int, int);

/* tree.c */
pair			 cons(caddr_t, pair);
struct tree_cache	*tree_cache(struct tree *);
struct tree		*tree_host_lookup(char *);
struct dns_host_entry	*enter_dns_host(char *);
struct tree		*tree_const(unsigned char *, int);
struct tree		*tree_concat(struct tree *, struct tree *);
struct tree		*tree_limit(struct tree *, int);
int			 tree_evaluate(struct tree_cache *);

/* dhcp.c */
extern int	outstanding_pings;

void dhcp(struct packet *);
void dhcpdiscover(struct packet *);
void dhcprequest(struct packet *);
void dhcprelease(struct packet *);
void dhcpdecline(struct packet *);
void dhcpinform(struct packet *);
void nak_lease(struct packet *, struct iaddr *cip);
void ack_lease(struct packet *, struct lease *, unsigned int, time_t);
void dhcp_reply(struct lease *);
struct lease *find_lease(struct packet *, struct shared_network *, int *);
struct lease *mockup_lease(struct packet *, struct shared_network *,
    struct host_decl *);

/* bootp.c */
void bootp(struct packet *);

/* memory.c */
void enter_host(struct host_decl *);
struct host_decl *find_hosts_by_haddr(int, unsigned char *, int);
struct host_decl *find_hosts_by_uid(unsigned char *, int);
struct subnet *find_host_for_network(struct host_decl **, struct iaddr *,
    struct shared_network *);
void new_address_range(struct iaddr, struct iaddr, struct subnet *, int);
extern struct subnet *find_grouped_subnet(struct shared_network *,
    struct iaddr);
extern struct subnet *find_subnet(struct iaddr);
void enter_shared_network(struct shared_network *);
int subnet_inner_than(struct subnet *, struct subnet *, int);
void enter_subnet(struct subnet *);
void enter_lease(struct lease *);
int supersede_lease(struct lease *, struct lease *, int);
void release_lease(struct lease *);
void abandon_lease(struct lease *, char *);
struct lease *find_lease_by_uid(unsigned char *, int);
struct lease *find_lease_by_hw_addr(unsigned char *, int);
struct lease *find_lease_by_ip_addr(struct iaddr);
void uid_hash_add(struct lease *);
void uid_hash_delete(struct lease *);
void hw_hash_add(struct lease *);
void hw_hash_delete(struct lease *);
struct class *add_class(int, char *);
struct class *find_class(int, unsigned char *, int);
struct group *clone_group(struct group *, char *);
void write_leases(void);

/* alloc.c */
void * dmalloc(int, char *);
void dfree(void *, char *);
struct packet *new_packet(char *);
struct dhcp_packet *new_dhcp_packet(char *);
struct tree *new_tree(char *);
struct tree_cache *new_tree_cache(char *);
struct hash_table *new_hash_table(int, char *);
struct hash_bucket *new_hash_bucket(char *);
struct lease *new_lease(char *);
struct lease *new_leases(int, char *);
struct subnet *new_subnet(char *);
struct class *new_class(char *);
struct shared_network *new_shared_network(char *);
struct group *new_group(char *);
struct protocol *new_protocol(char *);
struct lease_state *new_lease_state(char *);
struct domain_search_list *new_domain_search_list(char *);
struct name_server *new_name_server(char *);
void free_name_server(struct name_server *, char *);
void free_domain_search_list(struct domain_search_list *, char *);
void free_lease_state(struct lease_state *, char *);
void free_protocol(struct protocol *, char *);
void free_group(struct group *, char *);
void free_shared_network(struct shared_network *, char *);
void free_class(struct class *, char *);
void free_subnet(struct subnet *, char *);
void free_lease(struct lease *, char *);
void free_hash_bucket(struct hash_bucket *, char *);
void free_tree_cache(struct tree_cache *);
void free_tree(struct tree *, char *);

/* print.c */
char *print_hw_addr(int, int, unsigned char *);

/* bpf.c */
int if_register_bpf(struct interface_info *);
void if_register_send(struct interface_info *);
ssize_t send_packet(struct interface_info *, struct dhcp_packet *, size_t,
    struct in_addr, struct sockaddr_in *, struct hardware *);
void if_register_receive(struct interface_info *);
ssize_t receive_packet(struct interface_info *, unsigned char *, size_t,
    struct sockaddr_in *, struct hardware *);

/* dispatch.c */
extern struct interface_info *interfaces;
extern struct protocol *protocols;
extern void (*bootp_packet_handler)(struct interface_info *,
    struct dhcp_packet *, int, unsigned int, struct iaddr, struct hardware *);
extern struct dhcpd_timeout *timeouts;
void discover_interfaces(void);
void dispatch(void);
int locate_network(struct packet *);
void got_one(struct protocol *);
void add_timeout(time_t, void (*)(void *), void *);
void cancel_timeout(void (*)(void *), void *);
void add_protocol (char *, int, void (*)(struct protocol *), void *);
void remove_protocol(struct protocol *);

/* hash.c */
struct hash_table *new_hash(void);
void add_hash(struct hash_table *, unsigned char *, int, unsigned char *);
void delete_hash_entry(struct hash_table *, unsigned char *, int);
unsigned char *hash_lookup(struct hash_table *, unsigned char *, int);

/* tables.c */
extern struct option dhcp_options[256];
extern unsigned char dhcp_option_default_priority_list[256];
extern char *hardware_types[256];
extern struct hash_table universe_hash;
extern struct universe dhcp_universe;
void initialize_universes(void);

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
struct iaddr ip_addr(struct iaddr, struct iaddr, u_int32_t);
u_int32_t host_addr(struct iaddr, struct iaddr);
int addr_eq(struct iaddr, struct iaddr);
char *piaddr(struct iaddr);

/* db.c */
int write_lease(struct lease *);
int commit_leases(void);
void db_startup(void);
void new_lease_file(void);

/* packet.c */
void assemble_hw_header(struct interface_info *, unsigned char *,
    int *, struct hardware *);
void assemble_udp_ip_header(struct interface_info *, unsigned char *,
    int *, u_int32_t, u_int32_t, unsigned int, unsigned char *, int);
ssize_t decode_hw_header(struct interface_info *, unsigned char *,
    int, struct hardware *);
ssize_t decode_udp_ip_header(struct interface_info *, unsigned char *,
    int, struct sockaddr_in *, unsigned char *, int);
u_int32_t	checksum(unsigned char *, unsigned, u_int32_t);
u_int32_t	wrapsum(u_int32_t);

/* icmp.c */
void icmp_startup(int, void (*)(struct iaddr, u_int8_t *, int));
int icmp_echorequest(struct iaddr *);
void icmp_echoreply(struct protocol *);

/* pfutils.c */
__dead void pftable_handler(void);
void pf_change_table(int, int, struct in_addr, char *);
void pf_kill_state(int, struct in_addr);
size_t atomicio(ssize_t (*)(int, void *, size_t), int, void *, size_t);
#define vwrite (ssize_t (*)(int, void *, size_t))write
void pfmsg(char, struct lease *);
