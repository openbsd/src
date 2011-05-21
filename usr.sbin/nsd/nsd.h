/*
 * nsd.h -- nsd(8) definitions and prototypes
 *
 * Copyright (c) 2001-2011, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef	_NSD_H_
#define	_NSD_H_

#include <signal.h>

#include "dns.h"
#include "edns.h"
struct netio_handler;
struct nsd_options;

/* The NSD runtime states and NSD ipc command values */
#define	NSD_RUN	0
#define	NSD_RELOAD 1
#define	NSD_SHUTDOWN 2
#define	NSD_STATS 3
#define	NSD_REAP_CHILDREN 4
#define	NSD_QUIT 5
/*
 * NSD_SOA_INFO is followed by u16(len in network byte order), dname,
 * and then nothing (no info) or soa info.
 */
#define NSD_SOA_INFO 6
/*
 * PASS_TO_XFRD is followed by the u16(len in network order) and
 * then network packet contents.  packet is a notify(acl checked), or
 * xfr reply from a master(acl checked).
 * followed by u32(acl number that matched from notify/xfr acl).
 */
#define NSD_PASS_TO_XFRD 7
/*
 * NSD_ZONE_STATE is followed by u16(len in network byte order),
 * octet 0: zone is expired, 1: zone ok. and dname of zone.
 */
#define NSD_ZONE_STATE 8
/*
 * SOA BEGIN is sent at the start of a reload SOA_INFO pass
 * xfrd will not send to the parent (deadlock prevention).
 */
#define NSD_SOA_BEGIN 9
/*
 * SOA END is sent at the end of a reload SOA_INFO pass.
 * xfrd then knows that reload phase is over.
 */
#define NSD_SOA_END 10
/*
 * QUIT_SYNC is sent to signify a synchronisation of ipc
 * channel content during reload
 */
#define NSD_QUIT_SYNC 11

#define NSD_SERVER_MAIN 0x0U
#define NSD_SERVER_UDP  0x1U
#define NSD_SERVER_TCP  0x2U
#define NSD_SERVER_BOTH (NSD_SERVER_UDP | NSD_SERVER_TCP)

#ifdef INET6
#define DEFAULT_AI_FAMILY AF_UNSPEC
#else
#define DEFAULT_AI_FAMILY AF_INET
#endif

#ifdef BIND8_STATS

/* Counter for statistics */
typedef	unsigned long stc_t;

#define	LASTELEM(arr)	(sizeof(arr) / sizeof(arr[0]) - 1)

#define	STATUP(nsd, stc) nsd->st.stc++
/* #define	STATUP2(nsd, stc, i)  ((i) <= (LASTELEM(nsd->st.stc) - 1)) ? nsd->st.stc[(i)]++ : \
				nsd->st.stc[LASTELEM(nsd->st.stc)]++ */

#define	STATUP2(nsd, stc, i) nsd->st.stc[(i) <= (LASTELEM(nsd->st.stc) - 1) ? i : LASTELEM(nsd->st.stc)]++
#else	/* BIND8_STATS */

#define	STATUP(nsd, stc) /* Nothing */
#define	STATUP2(nsd, stc, i) /* Nothing */

#endif /* BIND8_STATS */

struct nsd_socket
{
	struct addrinfo	*	addr;
	int			s;
};

struct nsd_child
{
	 /* The type of child process (UDP or TCP handler). */
	int   kind;

	/* The child's process id.  */
	pid_t pid;

	/*
	 * Socket used by the parent process to send commands and
	 * receive responses to/from this child process.
	 */
	int child_fd;

	/*
	 * Socket used by the child process to receive commands and
	 * send responses from/to the parent process.
	 */
	int parent_fd;

	/*
	 * IPC info, buffered for nonblocking writes to the child
	 */
	uint8_t need_to_send_STATS, need_to_send_QUIT;
	uint8_t need_to_exit, has_exited;
	stack_type* dirty_zones; /* stack of type zone_type* */

	/*
	 * The handler for handling the commands from the child.
	 */
	struct netio_handler* handler;
};

/* NSD configuration and run-time variables */
typedef struct nsd nsd_type;
struct	nsd
{
	/*
	 * Global region that is not deallocated until NSD shuts down.
	 */
	region_type    *region;

	/* Run-time variables */
	pid_t		pid;
	volatile sig_atomic_t mode;
	volatile sig_atomic_t signal_hint_reload;
	volatile sig_atomic_t signal_hint_child;
	volatile sig_atomic_t signal_hint_quit;
	volatile sig_atomic_t signal_hint_shutdown;
	volatile sig_atomic_t signal_hint_stats;
	volatile sig_atomic_t signal_hint_statsusr;
	volatile sig_atomic_t quit_sync_done;
	unsigned		server_kind;
	struct namedb	*db;
	int				debug;

	size_t            child_count;
	struct nsd_child *children;

	/* NULL if this is the parent process. */
	struct nsd_child *this_child;

	/* Configuration */
	const char		*dbfile;
	const char		*pidfile;
	const char		*log_filename;
	const char		*username;
	uid_t			uid;
	gid_t			gid;
	const char		*chrootdir;
	const char		*version;
	const char		*identity;
	uint16_t		nsid_len;
	unsigned char   *nsid;
	uint8_t 		file_rotation_ok;

	/* number of interfaces, ifs < MAX_INTERFACES */
	size_t	ifs;
	uint8_t grab_ip6_optional;

	/* TCP specific configuration */
	struct nsd_socket tcp[MAX_INTERFACES];

	/* UDP specific configuration */
	struct nsd_socket udp[MAX_INTERFACES];

	edns_data_type edns_ipv4;
#if defined(INET6)
	edns_data_type edns_ipv6;
#endif

	int maximum_tcp_count;
	int current_tcp_count;
	int tcp_query_count;
	int tcp_timeout;
	size_t ipv4_edns_size;
	size_t ipv6_edns_size;

#ifdef	BIND8_STATS

	struct nsdst {
		time_t	boot;
		int	period;		/* Produce statistics dump every st_period seconds */
		stc_t	qtype[257];	/* Counters per qtype */
		stc_t	qclass[4];	/* Class IN or Class CH or other */
		stc_t	qudp, qudp6;	/* Number of queries udp and udp6 */
		stc_t	ctcp, ctcp6;	/* Number of tcp and tcp6 connections */
		stc_t	rcode[17], opcode[6]; /* Rcodes & opcodes */
		/* Dropped, truncated, queries for nonconfigured zone, tx errors */
		stc_t	dropped, truncated, wrongzone, txerr, rxerr;
		stc_t 	edns, ednserr, raxfr, nona;
	} st;
#endif /* BIND8_STATS */

	struct nsd_options* options;
};

/* nsd.c */
pid_t readpid(const char *file);
int writepid(struct nsd *nsd);
void unlinkpid(const char* file);
void sig_handler(int sig);
void bind8_stats(struct nsd *nsd);

/* server.c */
int server_init(struct nsd *nsd);
int server_prepare(struct nsd *nsd);
void server_main(struct nsd *nsd);
void server_child(struct nsd *nsd);
/* extra domain numbers for temporary domains */
#define EXTRA_DOMAIN_NUMBERS 1024

#endif	/* _NSD_H_ */
