/*
 * nsd.h -- nsd(8) definitions and prototypes
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
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
struct udb_base;
struct daemon_remote;

/* The NSD runtime states and NSD ipc command values */
#define	NSD_RUN	0
#define	NSD_RELOAD 1
#define	NSD_SHUTDOWN 2
#define	NSD_STATS 3
#define	NSD_REAP_CHILDREN 4
#define	NSD_QUIT 5
/*
 * PASS_TO_XFRD is followed by the u16(len in network order) and
 * then network packet contents.  packet is a notify(acl checked), or
 * xfr reply from a master(acl checked).
 * followed by u32(acl number that matched from notify/xfr acl).
 */
#define NSD_PASS_TO_XFRD 6
/*
 * RELOAD_REQ is sent when parent receives a SIGHUP and tells
 * xfrd that it wants to initiate a reload (and thus task swap).
 */
#define NSD_RELOAD_REQ 7
/*
 * RELOAD_DONE is sent at the end of a reload pass.
 * xfrd then knows that reload phase is over.
 */
#define NSD_RELOAD_DONE 8
/*
 * QUIT_SYNC is sent to signify a synchronisation of ipc
 * channel content during reload
 */
#define NSD_QUIT_SYNC 9
/*
 * QUIT_WITH_STATS is sent during a reload when BIND8_STATS is defined,
 * from parent to children.  The stats are transferred too from child to
 * parent with this commandvalue, when the child is exiting.
 */
#define NSD_QUIT_WITH_STATS 10
/*
 * QUIT_CHILD is sent at exit, to make sure the child has exited so that
 * port53 is free when all of nsd's processes have exited at shutdown time
 */
#define NSD_QUIT_CHILD 11

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
typedef	unsigned long stc_type;

#define	LASTELEM(arr)	(sizeof(arr) / sizeof(arr[0]) - 1)

#define	STATUP(nsd, stc) nsd->st.stc++
/* #define	STATUP2(nsd, stc, i)  ((i) <= (LASTELEM(nsd->st.stc) - 1)) ? nsd->st.stc[(i)]++ : \
				nsd->st.stc[LASTELEM(nsd->st.stc)]++ */

#define	STATUP2(nsd, stc, i) nsd->st.stc[(i) <= (LASTELEM(nsd->st.stc) - 1) ? i : LASTELEM(nsd->st.stc)]++
#else	/* BIND8_STATS */

#define	STATUP(nsd, stc) /* Nothing */
#define	STATUP2(nsd, stc, i) /* Nothing */

#endif /* BIND8_STATS */

#ifdef USE_ZONE_STATS
/* increment zone statistic, checks if zone-nonNULL and zone array bounds */
#define ZTATUP(nsd, zone, stc) ( \
	(zone && zone->zonestatid < nsd->zonestatsizenow) ? \
		nsd->zonestatnow[zone->zonestatid].stc++ \
		: 0)
#define	ZTATUP2(nsd, zone, stc, i) ( \
	(zone && zone->zonestatid < nsd->zonestatsizenow) ? \
		(nsd->zonestatnow[zone->zonestatid].stc[(i) <= (LASTELEM(nsd->zonestatnow[zone->zonestatid].stc) - 1) ? i : LASTELEM(nsd->zonestatnow[zone->zonestatid].stc)]++ ) \
		: 0)
#else /* USE_ZONE_STATS */
#define	ZTATUP(nsd, zone, stc) /* Nothing */
#define	ZTATUP2(nsd, zone, stc, i) /* Nothing */
#endif /* USE_ZONE_STATS */

struct nsd_socket
{
	struct addrinfo	*	addr;
	int			s;
	int			fam;
};

struct nsd_child
{
	 /* The type of child process (UDP or TCP handler). */
	int   kind;

	/* The child's process id.  */
	pid_t pid;

	/* child number in child array */
	int child_num;

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

	/*
	 * The handler for handling the commands from the child.
	 */
	struct netio_handler* handler;

#ifdef	BIND8_STATS
	stc_type query_count;
#endif
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
	volatile sig_atomic_t signal_hint_reload_hup;
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
	int	restart_children;
	int	reload_failed;

	/* NULL if this is the parent process. */
	struct nsd_child *this_child;

	/* mmaps with data exchange from xfrd and reload */
	struct udb_base* task[2];
	int mytask;
	/* the base used by this (child)process */
	struct event_base* event_base;
	/* the server_region used by this (child)process */
	region_type* server_region;
	struct netio_handler* xfrd_listener;
	struct daemon_remote* rc;

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

	/* number of interfaces */
	size_t	ifs;
	uint8_t grab_ip6_optional;
	/* non0 if so_reuseport is in use, if so, tcp, udp array increased */
	int reuseport;

	/* TCP specific configuration (array size ifs) */
	struct nsd_socket* tcp;

	/* UDP specific configuration (array size ifs) */
	struct nsd_socket* udp;

	edns_data_type edns_ipv4;
#if defined(INET6)
	edns_data_type edns_ipv6;
#endif

	int maximum_tcp_count;
	int current_tcp_count;
	int tcp_query_count;
	int tcp_timeout;
	int tcp_mss;
	int outgoing_tcp_mss;
	size_t ipv4_edns_size;
	size_t ipv6_edns_size;

#ifdef	BIND8_STATS

	struct nsdst {
		time_t	boot;
		int	period;		/* Produce statistics dump every st_period seconds */
		stc_type qtype[257];	/* Counters per qtype */
		stc_type qclass[4];	/* Class IN or Class CH or other */
		stc_type qudp, qudp6;	/* Number of queries udp and udp6 */
		stc_type ctcp, ctcp6;	/* Number of tcp and tcp6 connections */
		stc_type rcode[17], opcode[6]; /* Rcodes & opcodes */
		/* Dropped, truncated, queries for nonconfigured zone, tx errors */
		stc_type dropped, truncated, wrongzone, txerr, rxerr;
		stc_type edns, ednserr, raxfr, nona;
		uint64_t db_disk, db_mem;
	} st;
	/* per zone stats, each an array per zone-stat-idx, stats per zone is
	 * add of [0][zoneidx] and [1][zoneidx]. */
	struct nsdst* zonestat[2];
	/* fd for zonestat mapping (otherwise mmaps cannot be shared between
	 * processes and resized) */
	int zonestatfd[2];
	/* filenames */
	char* zonestatfname[2];
	/* size of the mmapped zone stat array (number of array entries) */
	size_t zonestatsize[2], zonestatdesired, zonestatsizenow;
	/* current zonestat array to use */
	struct nsdst* zonestatnow;
#endif /* BIND8_STATS */
	/* ratelimit for errors, time value */
	time_t err_limit_time;
	/* ratelimit for errors, packet count */
	unsigned int err_limit_count;

	struct nsd_options* options;
};

extern struct nsd nsd;

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
void server_shutdown(struct nsd *nsd);
void server_close_all_sockets(struct nsd_socket sockets[], size_t n);
struct event_base* nsd_child_event_base(void);
/* extra domain numbers for temporary domains */
#define EXTRA_DOMAIN_NUMBERS 1024
#define SLOW_ACCEPT_TIMEOUT 2 /* in seconds */
/* ratelimit for error responses */
#define ERROR_RATELIMIT 100 /* qps */
/* allocate zonestat structures */
void server_zonestat_alloc(struct nsd* nsd);
/* remap the mmaps for zonestat isx, to bytesize sz.  Caller has to set
 * the zonestatsize */
void zonestat_remap(struct nsd* nsd, int idx, size_t sz);
/* allocate and init xfrd variables */
void server_prepare_xfrd(struct nsd *nsd);
/* start xfrdaemon (again) */
void server_start_xfrd(struct nsd *nsd, int del_db, int reload_active);
/* send SOA serial numbers to xfrd */
void server_send_soa_xfrd(struct nsd *nsd, int shortsoa);
ssize_t block_read(struct nsd* nsd, int s, void* p, ssize_t sz, int timeout);

#endif	/* _NSD_H_ */
