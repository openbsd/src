/*
 * nsd.c -- nsd(8)
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_GRP_H
#include <grp.h>
#endif /* HAVE_GRP_H */
#ifdef HAVE_SETUSERCONTEXT
#include <login_cap.h>
#endif /* HAVE_SETUSERCONTEXT */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "nsd.h"
#include "options.h"
#include "tsig.h"
#include "remote.h"
#include "xfrd-disk.h"
#ifdef USE_DNSTAP
#include "dnstap/dnstap_collector.h"
#endif

/* The server handler... */
struct nsd nsd;
static char hostname[MAXHOSTNAMELEN];
extern config_parser_state_type* cfg_parser;
static void version(void) ATTR_NORETURN;

/*
 * Print the help text.
 *
 */
static void
usage (void)
{
	fprintf(stderr, "Usage: nsd [OPTION]...\n");
	fprintf(stderr, "Name Server Daemon.\n\n");
	fprintf(stderr,
		"Supported options:\n"
		"  -4                   Only listen to IPv4 connections.\n"
		"  -6                   Only listen to IPv6 connections.\n"
		"  -a ip-address[@port] Listen to the specified incoming IP address (and port)\n"
		"                       May be specified multiple times).\n"
		"  -c configfile        Read specified configfile instead of %s.\n"
		"  -d                   do not fork as a daemon process.\n"
#ifndef NDEBUG
		"  -F facilities        Specify the debug facilities.\n"
#endif /* NDEBUG */
		"  -f database          Specify the database to load.\n"
		"  -h                   Print this help information.\n"
		, CONFIGFILE);
	fprintf(stderr,
		"  -i identity          Specify the identity when queried for id.server CHAOS TXT.\n"
		"  -I nsid              Specify the NSID. This must be a hex string.\n"
#ifndef NDEBUG
		"  -L level             Specify the debug level.\n"
#endif /* NDEBUG */
		"  -l filename          Specify the log file.\n"
		"  -N server-count      The number of servers to start.\n"
		"  -n tcp-count         The maximum number of TCP connections per server.\n"
		"  -P pidfile           Specify the PID file to write.\n"
		"  -p port              Specify the port to listen to.\n"
		"  -s seconds           Dump statistics every SECONDS seconds.\n"
		"  -t chrootdir         Change root to specified directory on startup.\n"
		);
	fprintf(stderr,
		"  -u user              Change effective uid to the specified user.\n"
		"  -V level             Specify verbosity level.\n"
		"  -v                   Print version information.\n"
		);
	fprintf(stderr, "Version %s. Report bugs to <%s>.\n",
		PACKAGE_VERSION, PACKAGE_BUGREPORT);
}

/*
 * Print the version exit.
 *
 */
static void
version(void)
{
	fprintf(stderr, "%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	fprintf(stderr, "Written by NLnet Labs.\n\n");
	fprintf(stderr,
		"Copyright (C) 2001-2006 NLnet Labs.  This is free software.\n"
		"There is NO warranty; not even for MERCHANTABILITY or FITNESS\n"
		"FOR A PARTICULAR PURPOSE.\n");
	exit(0);
}

void
get_ip_port_frm_str(const char* arg, const char** hostname,
        const char** port)
{
	/* parse src[@port] option */
	char* delim = NULL;
	if (arg) {
		delim = strchr(arg, '@');
	}

	if (delim) {
		*delim = '\0';
		*port = delim+1;
	}
	*hostname = arg;
}

/* append interface to interface array (names, udp, tcp) */
void
add_interface(char*** nodes, struct nsd* nsd, char* ip)
{
	/* realloc the arrays */
	if(nsd->ifs == 0) {
		*nodes = xalloc_zero(sizeof(*nodes));
		nsd->udp = xalloc_zero(sizeof(*nsd->udp));
		nsd->tcp = xalloc_zero(sizeof(*nsd->udp));
	} else {
		region_remove_cleanup(nsd->region, free, *nodes);
		region_remove_cleanup(nsd->region, free, nsd->udp);
		region_remove_cleanup(nsd->region, free, nsd->tcp);
		*nodes = xrealloc(*nodes, (nsd->ifs+1)*sizeof(*nodes));
		nsd->udp = xrealloc(nsd->udp, (nsd->ifs+1)*sizeof(*nsd->udp));
		nsd->tcp = xrealloc(nsd->tcp, (nsd->ifs+1)*sizeof(*nsd->udp));
		(*nodes)[nsd->ifs] = NULL;
		memset(&nsd->udp[nsd->ifs], 0, sizeof(*nsd->udp));
		memset(&nsd->tcp[nsd->ifs], 0, sizeof(*nsd->tcp));
	}
	region_add_cleanup(nsd->region, free, *nodes);
	region_add_cleanup(nsd->region, free, nsd->udp);
	region_add_cleanup(nsd->region, free, nsd->tcp);

	/* add it */
	(*nodes)[nsd->ifs] = ip;
	++nsd->ifs;
}

/*
 * Fetch the nsd parent process id from the nsd pidfile
 *
 */
pid_t
readpid(const char *file)
{
	int fd;
	pid_t pid;
	char pidbuf[16];
	char *t;
	int l;

	if ((fd = open(file, O_RDONLY)) == -1) {
		return -1;
	}

	if (((l = read(fd, pidbuf, sizeof(pidbuf)))) == -1) {
		close(fd);
		return -1;
	}

	close(fd);

	/* Empty pidfile means no pidfile... */
	if (l == 0) {
		errno = ENOENT;
		return -1;
	}

	pid = (pid_t) strtol(pidbuf, &t, 10);

	if (*t && *t != '\n') {
		return -1;
	}
	return pid;
}

/*
 * Store the nsd parent process id in the nsd pidfile
 *
 */
int
writepid(struct nsd *nsd)
{
	FILE * fd;
	char pidbuf[32];

	snprintf(pidbuf, sizeof(pidbuf), "%lu\n", (unsigned long) nsd->pid);

	if ((fd = fopen(nsd->pidfile, "w")) ==  NULL ) {
		log_msg(LOG_ERR, "cannot open pidfile %s: %s",
			nsd->pidfile, strerror(errno));
		return -1;
	}

	if (!write_data(fd, pidbuf, strlen(pidbuf))) {
		log_msg(LOG_ERR, "cannot write pidfile %s: %s",
			nsd->pidfile, strerror(errno));
		fclose(fd);
		return -1;
	}
	fclose(fd);

	if (chown(nsd->pidfile, nsd->uid, nsd->gid) == -1) {
		log_msg(LOG_ERR, "cannot chown %u.%u %s: %s",
			(unsigned) nsd->uid, (unsigned) nsd->gid,
			nsd->pidfile, strerror(errno));
		return -1;
	}

	return 0;
}

void
unlinkpid(const char* file)
{
	int fd = -1;

	if (file) {
		/* truncate pidfile */
		fd = open(file, O_WRONLY | O_TRUNC, 0644);
		if (fd == -1) {
			/* Truncate the pid file.  */
			log_msg(LOG_ERR, "can not truncate the pid file %s: %s", file, strerror(errno));
		} else 
			close(fd);

		/* unlink pidfile */
		if (unlink(file) == -1)
			log_msg(LOG_WARNING, "failed to unlink pidfile %s: %s",
				file, strerror(errno));
	}
}

/*
 * Incoming signals, set appropriate actions.
 *
 */
void
sig_handler(int sig)
{
	/* To avoid race cond. We really don't want to use log_msg() in this handler */

	/* Are we a child server? */
	if (nsd.server_kind != NSD_SERVER_MAIN) {
		switch (sig) {
		case SIGCHLD:
			nsd.signal_hint_child = 1;
			break;
		case SIGALRM:
			break;
		case SIGINT:
		case SIGTERM:
			nsd.signal_hint_quit = 1;
			break;
		case SIGILL:
		case SIGUSR1:	/* Dump stats on SIGUSR1.  */
			nsd.signal_hint_statsusr = 1;
			break;
		default:
			break;
		}
		return;
	}

	/* We are the main process */
	switch (sig) {
	case SIGCHLD:
		nsd.signal_hint_child = 1;
		return;
	case SIGHUP:
		nsd.signal_hint_reload_hup = 1;
		return;
	case SIGALRM:
		nsd.signal_hint_stats = 1;
		break;
	case SIGILL:
		/*
		 * For backwards compatibility with BIND 8 and older
		 * versions of NSD.
		 */
		nsd.signal_hint_statsusr = 1;
		break;
	case SIGUSR1:
		/* Dump statistics.  */
		nsd.signal_hint_statsusr = 1;
		break;
	case SIGINT:
	case SIGTERM:
	default:
		nsd.signal_hint_shutdown = 1;
		break;
	}
}

/*
 * Statistic output...
 *
 */
#ifdef BIND8_STATS
void
bind8_stats (struct nsd *nsd)
{
	char buf[MAXSYSLOGMSGLEN];
	char *msg, *t;
	int i, len;

	/* Current time... */
	time_t now;
	if(!nsd->st.period)
		return;
	time(&now);

	/* NSTATS */
	t = msg = buf + snprintf(buf, MAXSYSLOGMSGLEN, "NSTATS %lld %lu",
				 (long long) now, (unsigned long) nsd->st.boot);
	for (i = 0; i <= 255; i++) {
		/* How much space left? */
		if ((len = buf + MAXSYSLOGMSGLEN - t) < 32) {
			log_msg(LOG_INFO, "%s", buf);
			t = msg;
			len = buf + MAXSYSLOGMSGLEN - t;
		}

		if (nsd->st.qtype[i] != 0) {
			t += snprintf(t, len, " %s=%lu", rrtype_to_string(i), nsd->st.qtype[i]);
		}
	}
	if (t > msg)
		log_msg(LOG_INFO, "%s", buf);

	/* XSTATS */
	/* Only print it if we're in the main daemon or have anything to report... */
	if (nsd->server_kind == NSD_SERVER_MAIN
	    || nsd->st.dropped || nsd->st.raxfr || (nsd->st.qudp + nsd->st.qudp6 - nsd->st.dropped)
	    || nsd->st.txerr || nsd->st.opcode[OPCODE_QUERY] || nsd->st.opcode[OPCODE_IQUERY]
	    || nsd->st.wrongzone || nsd->st.ctcp + nsd->st.ctcp6 || nsd->st.rcode[RCODE_SERVFAIL]
	    || nsd->st.rcode[RCODE_FORMAT] || nsd->st.nona || nsd->st.rcode[RCODE_NXDOMAIN]
	    || nsd->st.opcode[OPCODE_UPDATE]) {

		log_msg(LOG_INFO, "XSTATS %lld %lu"
			" RR=%lu RNXD=%lu RFwdR=%lu RDupR=%lu RFail=%lu RFErr=%lu RErr=%lu RAXFR=%lu"
			" RLame=%lu ROpts=%lu SSysQ=%lu SAns=%lu SFwdQ=%lu SDupQ=%lu SErr=%lu RQ=%lu"
			" RIQ=%lu RFwdQ=%lu RDupQ=%lu RTCP=%lu SFwdR=%lu SFail=%lu SFErr=%lu SNaAns=%lu"
			" SNXD=%lu RUQ=%lu RURQ=%lu RUXFR=%lu RUUpd=%lu",
			(long long) now, (unsigned long) nsd->st.boot,
			nsd->st.dropped, (unsigned long)0, (unsigned long)0, (unsigned long)0, (unsigned long)0,
			(unsigned long)0, (unsigned long)0, nsd->st.raxfr, (unsigned long)0, (unsigned long)0,
			(unsigned long)0, nsd->st.qudp + nsd->st.qudp6 - nsd->st.dropped, (unsigned long)0,
			(unsigned long)0, nsd->st.txerr,
			nsd->st.opcode[OPCODE_QUERY], nsd->st.opcode[OPCODE_IQUERY], nsd->st.wrongzone,
			(unsigned long)0, nsd->st.ctcp + nsd->st.ctcp6,
			(unsigned long)0, nsd->st.rcode[RCODE_SERVFAIL], nsd->st.rcode[RCODE_FORMAT],
			nsd->st.nona, nsd->st.rcode[RCODE_NXDOMAIN],
			(unsigned long)0, (unsigned long)0, (unsigned long)0, nsd->st.opcode[OPCODE_UPDATE]);
	}

}
#endif /* BIND8_STATS */

extern char *optarg;
extern int optind;

int
main(int argc, char *argv[])
{
	/* Scratch variables... */
	int c;
	pid_t	oldpid;
	size_t i;
	struct sigaction action;
#ifdef HAVE_GETPWNAM
	struct passwd *pwd = NULL;
#endif /* HAVE_GETPWNAM */

	struct addrinfo hints[2];
	int hints_in_use = 1;
	char** nodes = NULL; /* array of address strings, size nsd.ifs */
	const char *udp_port = 0;
	const char *tcp_port = 0;

	const char *configfile = CONFIGFILE;

	char* argv0 = (argv0 = strrchr(argv[0], '/')) ? argv0 + 1 : argv[0];

	log_init(argv0);

	/* Initialize the server handler... */
	memset(&nsd, 0, sizeof(struct nsd));
	nsd.region      = region_create(xalloc, free);
	nsd.dbfile	= 0;
	nsd.pidfile	= 0;
	nsd.server_kind = NSD_SERVER_MAIN;
	memset(&hints, 0, sizeof(*hints)*2);
	hints[0].ai_family = DEFAULT_AI_FAMILY;
	hints[0].ai_flags = AI_PASSIVE;
	hints[1].ai_family = DEFAULT_AI_FAMILY;
	hints[1].ai_flags = AI_PASSIVE;
	nsd.identity	= 0;
	nsd.version	= VERSION;
	nsd.username	= 0;
	nsd.chrootdir	= 0;
	nsd.nsid 	= NULL;
	nsd.nsid_len 	= 0;

	nsd.child_count = 0;
	nsd.maximum_tcp_count = 0;
	nsd.current_tcp_count = 0;
	nsd.grab_ip6_optional = 0;
	nsd.file_rotation_ok = 0;

	/* Set up our default identity to gethostname(2) */
	if (gethostname(hostname, MAXHOSTNAMELEN) == 0) {
		nsd.identity = hostname;
	} else {
		log_msg(LOG_ERR,
			"failed to get the host name: %s - using default identity",
			strerror(errno));
		nsd.identity = IDENTITY;
	}

	/* Parse the command line... */
	while ((c = getopt(argc, argv, "46a:c:df:hi:I:l:N:n:P:p:s:u:t:X:V:v"
#ifndef NDEBUG /* <mattthijs> only when configured with --enable-checking */
		"F:L:"
#endif /* NDEBUG */
		)) != -1) {
		switch (c) {
		case '4':
			hints[0].ai_family = AF_INET;
			break;
		case '6':
#ifdef INET6
			hints[0].ai_family = AF_INET6;
#else /* !INET6 */
			error("IPv6 support not enabled.");
#endif /* INET6 */
			break;
		case 'a':
			add_interface(&nodes, &nsd, optarg);
			break;
		case 'c':
			configfile = optarg;
			break;
		case 'd':
			nsd.debug = 1;
			break;
		case 'f':
			nsd.dbfile = optarg;
			break;
		case 'h':
			usage();
			exit(0);
		case 'i':
			nsd.identity = optarg;
			break;
		case 'I':
			if (nsd.nsid_len != 0) {
				/* can only be given once */
				break;
			}
			if (strncasecmp(optarg, "ascii_", 6) == 0) {
				nsd.nsid = xalloc(strlen(optarg+6));
				nsd.nsid_len = strlen(optarg+6);
				memmove(nsd.nsid, optarg+6, nsd.nsid_len);
			} else {
				if (strlen(optarg) % 2 != 0) {
					error("the NSID must be a hex string of an even length.");
				}
				nsd.nsid = xalloc(strlen(optarg) / 2);
				nsd.nsid_len = strlen(optarg) / 2;
				if (hex_pton(optarg, nsd.nsid, nsd.nsid_len) == -1) {
					error("hex string cannot be parsed '%s' in NSID.", optarg);
				}
			}
			break;
		case 'l':
			nsd.log_filename = optarg;
			break;
		case 'N':
			i = atoi(optarg);
			if (i <= 0) {
				error("number of child servers must be greater than zero.");
			} else {
				nsd.child_count = i;
			}
			break;
		case 'n':
			i = atoi(optarg);
			if (i <= 0) {
				error("number of concurrent TCP connections must greater than zero.");
			} else {
				nsd.maximum_tcp_count = i;
			}
			break;
		case 'P':
			nsd.pidfile = optarg;
			break;
		case 'p':
			if (atoi(optarg) == 0) {
				error("port argument must be numeric.");
			}
			tcp_port = optarg;
			udp_port = optarg;
			break;
		case 's':
#ifdef BIND8_STATS
			nsd.st.period = atoi(optarg);
#else /* !BIND8_STATS */
			error("BIND 8 statistics not enabled.");
#endif /* BIND8_STATS */
			break;
		case 't':
#ifdef HAVE_CHROOT
			nsd.chrootdir = optarg;
#else /* !HAVE_CHROOT */
			error("chroot not supported on this platform.");
#endif /* HAVE_CHROOT */
			break;
		case 'u':
			nsd.username = optarg;
			break;
		case 'V':
			verbosity = atoi(optarg);
			break;
		case 'v':
			version();
			/* version exits */
			break;
#ifndef NDEBUG
		case 'F':
			sscanf(optarg, "%x", &nsd_debug_facilities);
			break;
		case 'L':
			sscanf(optarg, "%d", &nsd_debug_level);
			break;
#endif /* NDEBUG */
		case '?':
		default:
			usage();
			exit(1);
		}
	}
	argc -= optind;
	/* argv += optind; */

	/* Commandline parse error */
	if (argc != 0) {
		usage();
		exit(1);
	}

	if (strlen(nsd.identity) > UCHAR_MAX) {
		error("server identity too long (%u characters)",
		      (unsigned) strlen(nsd.identity));
	}
	if(!tsig_init(nsd.region))
		error("init tsig failed");

	/* Read options */
	nsd.options = nsd_options_create(region_create_custom(xalloc, free,
		DEFAULT_CHUNK_SIZE, DEFAULT_LARGE_OBJECT_SIZE,
		DEFAULT_INITIAL_CLEANUP_SIZE, 1));
	if(!parse_options_file(nsd.options, configfile, NULL, NULL)) {
		error("could not read config: %s\n", configfile);
	}
	if(!parse_zone_list_file(nsd.options)) {
		error("could not read zonelist file %s\n",
			nsd.options->zonelistfile);
	}
	if(nsd.options->do_ip4 && !nsd.options->do_ip6) {
		hints[0].ai_family = AF_INET;
	}
#ifdef INET6
	if(nsd.options->do_ip6 && !nsd.options->do_ip4) {
		hints[0].ai_family = AF_INET6;
	}
#endif /* INET6 */
	if(nsd.options->ip_addresses)
	{
		ip_address_option_type* ip = nsd.options->ip_addresses;
		while(ip) {
			add_interface(&nodes, &nsd, ip->address);
			ip = ip->next;
		}
	}
	if (verbosity == 0)
		verbosity = nsd.options->verbosity;
#ifndef NDEBUG
	if (nsd_debug_level > 0 && verbosity == 0)
		verbosity = nsd_debug_level;
#endif /* NDEBUG */
	if(nsd.options->debug_mode) nsd.debug=1;
	if(!nsd.dbfile)
	{
		if(nsd.options->database)
			nsd.dbfile = nsd.options->database;
		else
			nsd.dbfile = DBFILE;
	}
	if(!nsd.pidfile)
	{
		if(nsd.options->pidfile)
			nsd.pidfile = nsd.options->pidfile;
		else
			nsd.pidfile = PIDFILE;
	}
	if(strcmp(nsd.identity, hostname)==0 || strcmp(nsd.identity,IDENTITY)==0)
	{
		if(nsd.options->identity)
			nsd.identity = nsd.options->identity;
	}
	if(nsd.options->version) {
		nsd.version = nsd.options->version;
	}
	if (nsd.options->logfile && !nsd.log_filename) {
		nsd.log_filename = nsd.options->logfile;
	}
	if(nsd.child_count == 0) {
		nsd.child_count = nsd.options->server_count;
	}
#ifdef SO_REUSEPORT
	if(nsd.options->reuseport && nsd.child_count > 1) {
		nsd.reuseport = nsd.child_count;
	}
#endif /* SO_REUSEPORT */
	if(nsd.maximum_tcp_count == 0) {
		nsd.maximum_tcp_count = nsd.options->tcp_count;
	}
	nsd.tcp_timeout = nsd.options->tcp_timeout;
	nsd.tcp_query_count = nsd.options->tcp_query_count;
	nsd.tcp_mss = nsd.options->tcp_mss;
	nsd.outgoing_tcp_mss = nsd.options->outgoing_tcp_mss;
	nsd.ipv4_edns_size = nsd.options->ipv4_edns_size;
	nsd.ipv6_edns_size = nsd.options->ipv6_edns_size;

	if(udp_port == 0)
	{
		if(nsd.options->port != 0) {
			udp_port = nsd.options->port;
			tcp_port = nsd.options->port;
		} else {
			udp_port = UDP_PORT;
			tcp_port = TCP_PORT;
		}
	}
#ifdef BIND8_STATS
	if(nsd.st.period == 0) {
		nsd.st.period = nsd.options->statistics;
	}
#endif /* BIND8_STATS */
#ifdef HAVE_CHROOT
	if(nsd.chrootdir == 0) nsd.chrootdir = nsd.options->chroot;
#ifdef CHROOTDIR
	/* if still no chrootdir, fallback to default */
	if(nsd.chrootdir == 0) nsd.chrootdir = CHROOTDIR;
#endif /* CHROOTDIR */
#endif /* HAVE_CHROOT */
	if(nsd.username == 0) {
		if(nsd.options->username) nsd.username = nsd.options->username;
		else nsd.username = USER;
	}
	if(nsd.options->zonesdir && nsd.options->zonesdir[0]) {
		if(chdir(nsd.options->zonesdir)) {
			error("cannot chdir to '%s': %s",
				nsd.options->zonesdir, strerror(errno));
		}
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "changed directory to %s",
			nsd.options->zonesdir));
	}

	/* EDNS0 */
	edns_init_data(&nsd.edns_ipv4, nsd.options->ipv4_edns_size);
#if defined(INET6)
#if defined(IPV6_USE_MIN_MTU) || defined(IPV6_MTU)
	edns_init_data(&nsd.edns_ipv6, nsd.options->ipv6_edns_size);
#else /* no way to set IPV6 MTU, send no bigger than that. */
	if (nsd.options->ipv6_edns_size < IPV6_MIN_MTU)
		edns_init_data(&nsd.edns_ipv6, nsd.options->ipv6_edns_size);
	else
		edns_init_data(&nsd.edns_ipv6, IPV6_MIN_MTU);
#endif /* IPV6 MTU) */
#endif /* defined(INET6) */

	if (nsd.nsid_len == 0 && nsd.options->nsid) {
		if (strlen(nsd.options->nsid) % 2 != 0) {
			error("the NSID must be a hex string of an even length.");
		}
		nsd.nsid = xalloc(strlen(nsd.options->nsid) / 2);
		nsd.nsid_len = strlen(nsd.options->nsid) / 2;
		if (hex_pton(nsd.options->nsid, nsd.nsid, nsd.nsid_len) == -1) {
			error("hex string cannot be parsed '%s' in NSID.", nsd.options->nsid);
		}
	}
	edns_init_nsid(&nsd.edns_ipv4, nsd.nsid_len);
#if defined(INET6)
	edns_init_nsid(&nsd.edns_ipv6, nsd.nsid_len);
#endif /* defined(INET6) */

	/* Number of child servers to fork.  */
	nsd.children = (struct nsd_child *) region_alloc_array(
		nsd.region, nsd.child_count, sizeof(struct nsd_child));
	for (i = 0; i < nsd.child_count; ++i) {
		nsd.children[i].kind = NSD_SERVER_BOTH;
		nsd.children[i].pid = -1;
		nsd.children[i].child_fd = -1;
		nsd.children[i].parent_fd = -1;
		nsd.children[i].handler = NULL;
		nsd.children[i].need_to_send_STATS = 0;
		nsd.children[i].need_to_send_QUIT = 0;
		nsd.children[i].need_to_exit = 0;
		nsd.children[i].has_exited = 0;
#ifdef  BIND8_STATS
		nsd.children[i].query_count = 0;
#endif
	}

	nsd.this_child = NULL;

	/* We need at least one active interface */
	if (nsd.ifs == 0) {
		add_interface(&nodes, &nsd, NULL);

		/*
		 * With IPv6 we'd like to open two separate sockets,
		 * one for IPv4 and one for IPv6, both listening to
		 * the wildcard address (unless the -4 or -6 flags are
		 * specified).
		 *
		 * However, this is only supported on platforms where
		 * we can turn the socket option IPV6_V6ONLY _on_.
		 * Otherwise we just listen to a single IPv6 socket
		 * and any incoming IPv4 connections will be
		 * automatically mapped to our IPv6 socket.
		 */
#ifdef INET6
		if (hints[0].ai_family == AF_UNSPEC) {
#ifdef IPV6_V6ONLY
			add_interface(&nodes, &nsd, NULL);
			hints[0].ai_family = AF_INET6;
			hints[1].ai_family = AF_INET;
			hints_in_use = 2;
			nsd.grab_ip6_optional = 1;
#else /* !IPV6_V6ONLY */
			hints[0].ai_family = AF_INET6;
#endif	/* IPV6_V6ONLY */
		}
#endif /* INET6 */
	}

	/* Set up the address info structures with real interface/port data */
	assert(nodes);
	for (i = 0; i < nsd.ifs; ++i) {
		int r;
		const char* node = NULL;
		const char* service = NULL;
		int h = ((hints_in_use == 1)?0:i%hints_in_use);

		/* We don't perform name-lookups */
		if (nodes[i] != NULL)
			hints[h].ai_flags |= AI_NUMERICHOST;
		get_ip_port_frm_str(nodes[i], &node, &service);

		hints[h].ai_socktype = SOCK_DGRAM;
		if ((r=getaddrinfo(node, (service?service:udp_port), &hints[h], &nsd.udp[i].addr)) != 0) {
#ifdef INET6
			if(nsd.grab_ip6_optional && hints[0].ai_family == AF_INET6) {
				log_msg(LOG_WARNING, "No IPv6, fallback to IPv4. getaddrinfo: %s",
				r==EAI_SYSTEM?strerror(errno):gai_strerror(r));
				continue;
			}
#endif
			error("cannot parse address '%s': getaddrinfo: %s %s",
				nodes[i]?nodes[i]:"(null)",
				gai_strerror(r),
				r==EAI_SYSTEM?strerror(errno):"");
		}

		hints[h].ai_socktype = SOCK_STREAM;
		if ((r=getaddrinfo(node, (service?service:tcp_port), &hints[h], &nsd.tcp[i].addr)) != 0) {
			error("cannot parse address '%s': getaddrinfo: %s %s",
				nodes[i]?nodes[i]:"(null)",
				gai_strerror(r),
				r==EAI_SYSTEM?strerror(errno):"");
		}
	}

	/* Parse the username into uid and gid */
	nsd.gid = getgid();
	nsd.uid = getuid();
#ifdef HAVE_GETPWNAM
	/* Parse the username into uid and gid */
	if (*nsd.username) {
		if (isdigit((unsigned char)*nsd.username)) {
			char *t;
			nsd.uid = strtol(nsd.username, &t, 10);
			if (*t != 0) {
				if (*t != '.' || !isdigit((unsigned char)*++t)) {
					error("-u user or -u uid or -u uid.gid");
				}
				nsd.gid = strtol(t, &t, 10);
			} else {
				/* Lookup the group id in /etc/passwd */
				if ((pwd = getpwuid(nsd.uid)) == NULL) {
					error("user id %u does not exist.", (unsigned) nsd.uid);
				} else {
					nsd.gid = pwd->pw_gid;
				}
			}
		} else {
			/* Lookup the user id in /etc/passwd */
			if ((pwd = getpwnam(nsd.username)) == NULL) {
				error("user '%s' does not exist.", nsd.username);
			} else {
				nsd.uid = pwd->pw_uid;
				nsd.gid = pwd->pw_gid;
			}
		}
	}
	/* endpwent(); */
#endif /* HAVE_GETPWNAM */

#if defined(HAVE_SSL)
	key_options_tsig_add(nsd.options);
#endif

	append_trailing_slash(&nsd.options->xfrdir, nsd.options->region);
	/* Check relativity of pathnames to chroot */
	if (nsd.chrootdir && nsd.chrootdir[0]) {
		/* existing chrootdir: append trailing slash for strncmp checking */
		append_trailing_slash(&nsd.chrootdir, nsd.region);
		append_trailing_slash(&nsd.options->zonesdir, nsd.options->region);

		/* zonesdir must be absolute and within chroot,
		 * all other pathnames may be relative to zonesdir */
		if (strncmp(nsd.options->zonesdir, nsd.chrootdir, strlen(nsd.chrootdir)) != 0) {
			error("zonesdir %s has to be an absolute path that starts with the chroot path %s",
				nsd.options->zonesdir, nsd.chrootdir);
		} else if (!file_inside_chroot(nsd.pidfile, nsd.chrootdir)) {
			error("pidfile %s is not relative to %s: chroot not possible",
				nsd.pidfile, nsd.chrootdir);
		} else if (!file_inside_chroot(nsd.dbfile, nsd.chrootdir)) {
			error("database %s is not relative to %s: chroot not possible",
				nsd.dbfile, nsd.chrootdir);
		} else if (!file_inside_chroot(nsd.options->xfrdfile, nsd.chrootdir)) {
			error("xfrdfile %s is not relative to %s: chroot not possible",
				nsd.options->xfrdfile, nsd.chrootdir);
		} else if (!file_inside_chroot(nsd.options->zonelistfile, nsd.chrootdir)) {
			error("zonelistfile %s is not relative to %s: chroot not possible",
				nsd.options->zonelistfile, nsd.chrootdir);
		} else if (!file_inside_chroot(nsd.options->xfrdir, nsd.chrootdir)) {
			error("xfrdir %s is not relative to %s: chroot not possible",
				nsd.options->xfrdir, nsd.chrootdir);
		}
	}

	/* Set up the logging */
	log_open(LOG_PID, FACILITY, nsd.log_filename);
	if (!nsd.log_filename)
		log_set_log_function(log_syslog);
	else if (nsd.uid && nsd.gid) {
		if(chown(nsd.log_filename, nsd.uid, nsd.gid) != 0)
			VERBOSITY(2, (LOG_WARNING, "chown %s failed: %s",
				nsd.log_filename, strerror(errno)));
	}
	log_msg(LOG_NOTICE, "%s starting (%s)", argv0, PACKAGE_STRING);

	/* Do we have a running nsd? */
	if ((oldpid = readpid(nsd.pidfile)) == -1) {
		if (errno != ENOENT) {
			log_msg(LOG_ERR, "can't read pidfile %s: %s",
				nsd.pidfile, strerror(errno));
		}
	} else {
		if (kill(oldpid, 0) == 0 || errno == EPERM) {
			log_msg(LOG_WARNING,
				"%s is already running as %u, continuing",
				argv0, (unsigned) oldpid);
		} else {
			log_msg(LOG_ERR,
				"...stale pid file from process %u",
				(unsigned) oldpid);
		}
	}

	/* Setup the signal handling... */
	action.sa_handler = sig_handler;
	sigfillset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGHUP, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGILL, &action, NULL);
	sigaction(SIGUSR1, &action, NULL);
	sigaction(SIGALRM, &action, NULL);
	sigaction(SIGCHLD, &action, NULL);
	action.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &action, NULL);

	/* Initialize... */
	nsd.mode = NSD_RUN;
	nsd.signal_hint_child = 0;
	nsd.signal_hint_reload = 0;
	nsd.signal_hint_reload_hup = 0;
	nsd.signal_hint_quit = 0;
	nsd.signal_hint_shutdown = 0;
	nsd.signal_hint_stats = 0;
	nsd.signal_hint_statsusr = 0;
	nsd.quit_sync_done = 0;

	/* Initialize the server... */
	if (server_init(&nsd) != 0) {
		error("server initialization failed, %s could "
			"not be started", argv0);
	}
#if defined(HAVE_SSL)
	if(nsd.options->control_enable) {
		/* read ssl keys while superuser and outside chroot */
		if(!(nsd.rc = daemon_remote_create(nsd.options)))
			error("could not perform remote control setup");
	}
#endif /* HAVE_SSL */

	/* Unless we're debugging, fork... */
	if (!nsd.debug) {
		int fd;

		/* Take off... */
		switch ((nsd.pid = fork())) {
		case 0:
			/* Child */
			break;
		case -1:
			error("fork() failed: %s", strerror(errno));
			break;
		default:
			/* Parent is done */
			server_close_all_sockets(nsd.udp, nsd.ifs);
			server_close_all_sockets(nsd.tcp, nsd.ifs);
			exit(0);
		}

		/* Detach ourselves... */
		if (setsid() == -1) {
			error("setsid() failed: %s", strerror(errno));
		}

		if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
			(void)dup2(fd, STDIN_FILENO);
			(void)dup2(fd, STDOUT_FILENO);
			(void)dup2(fd, STDERR_FILENO);
			if (fd > 2)
				(void)close(fd);
		}
	}

	/* Get our process id */
	nsd.pid = getpid();

	/* Set user context */
#ifdef HAVE_GETPWNAM
	if (*nsd.username) {
#ifdef HAVE_SETUSERCONTEXT
		/* setusercontext does initgroups, setuid, setgid, and
		 * also resource limits from login config, but we
		 * still call setresuid, setresgid to be sure to set all uid */
		if (setusercontext(NULL, pwd, nsd.uid,
			LOGIN_SETALL & ~LOGIN_SETUSER & ~LOGIN_SETGROUP) != 0)
			log_msg(LOG_WARNING, "unable to setusercontext %s: %s",
				nsd.username, strerror(errno));
#endif /* HAVE_SETUSERCONTEXT */
	}
#endif /* HAVE_GETPWNAM */

	/* Chroot */
#ifdef HAVE_CHROOT
	if (nsd.chrootdir && nsd.chrootdir[0]) {
		int l = strlen(nsd.chrootdir)-1; /* ends in trailing slash */

		if (file_inside_chroot(nsd.log_filename, nsd.chrootdir))
			nsd.file_rotation_ok = 1;

		/* strip chroot from pathnames if they're absolute */
		nsd.options->zonesdir += l;
		if (nsd.log_filename){
			if (nsd.log_filename[0] == '/')
				nsd.log_filename += l;
		}
		if (nsd.pidfile[0] == '/')
			nsd.pidfile += l;
		if (nsd.dbfile[0] == '/')
			nsd.dbfile += l;
		if (nsd.options->xfrdfile[0] == '/')
			nsd.options->xfrdfile += l;
		if (nsd.options->zonelistfile[0] == '/')
			nsd.options->zonelistfile += l;
		if (nsd.options->xfrdir[0] == '/')
			nsd.options->xfrdir += l;

		/* strip chroot from pathnames of "include:" statements
		 * on subsequent repattern commands */
		cfg_parser->chroot = nsd.chrootdir;

#ifdef HAVE_TZSET
		/* set timezone whilst not yet in chroot */
		tzset();
#endif
		if (chroot(nsd.chrootdir)) {
			error("unable to chroot: %s", strerror(errno));
		}
		if (chdir("/")) {
			error("unable to chdir to chroot: %s", strerror(errno));
		}
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "changed root directory to %s",
			nsd.chrootdir));
		/* chdir to zonesdir again after chroot */
		if(nsd.options->zonesdir && nsd.options->zonesdir[0]) {
			if(chdir(nsd.options->zonesdir)) {
				error("unable to chdir to '%s': %s",
					nsd.options->zonesdir, strerror(errno));
			}
			DEBUG(DEBUG_IPC,1, (LOG_INFO, "changed directory to %s",
				nsd.options->zonesdir));
		}
	}
	else
#endif /* HAVE_CHROOT */
		nsd.file_rotation_ok = 1;

	DEBUG(DEBUG_IPC,1, (LOG_INFO, "file rotation on %s %sabled",
		nsd.log_filename, nsd.file_rotation_ok?"en":"dis"));

	/* Write pidfile */
	if (writepid(&nsd) == -1) {
		log_msg(LOG_ERR, "cannot overwrite the pidfile %s: %s",
			nsd.pidfile, strerror(errno));
	}

	/* Drop the permissions */
#ifdef HAVE_GETPWNAM
	if (*nsd.username) {
#ifdef HAVE_INITGROUPS
		if(initgroups(nsd.username, nsd.gid) != 0)
			log_msg(LOG_WARNING, "unable to initgroups %s: %s",
				nsd.username, strerror(errno));
#endif /* HAVE_INITGROUPS */
		endpwent();

#ifdef HAVE_SETRESGID
		if(setresgid(nsd.gid,nsd.gid,nsd.gid) != 0)
#elif defined(HAVE_SETREGID) && !defined(DARWIN_BROKEN_SETREUID)
			if(setregid(nsd.gid,nsd.gid) != 0)
#else /* use setgid */
				if(setgid(nsd.gid) != 0)
#endif /* HAVE_SETRESGID */
					error("unable to set group id of %s: %s",
						nsd.username, strerror(errno));

#ifdef HAVE_SETRESUID
		if(setresuid(nsd.uid,nsd.uid,nsd.uid) != 0)
#elif defined(HAVE_SETREUID) && !defined(DARWIN_BROKEN_SETREUID)
			if(setreuid(nsd.uid,nsd.uid) != 0)
#else /* use setuid */
				if(setuid(nsd.uid) != 0)
#endif /* HAVE_SETRESUID */
					error("unable to set user id of %s: %s",
						nsd.username, strerror(errno));

		DEBUG(DEBUG_IPC,1, (LOG_INFO, "dropped user privileges, run as %s",
			nsd.username));
	}
#endif /* HAVE_GETPWNAM */

	if (pledge("stdio rpath wpath cpath dns inet proc", NULL) == -1)
		error("pledge");

	xfrd_make_tempdir(&nsd);
#ifdef USE_ZONE_STATS
	options_zonestatnames_create(nsd.options);
	server_zonestat_alloc(&nsd);
#endif /* USE_ZONE_STATS */
#ifdef USE_DNSTAP
	if(nsd.options->dnstap_enable) {
		nsd.dt_collector = dt_collector_create(&nsd);
		dt_collector_start(nsd.dt_collector, &nsd);
	}
#endif /* USE_DNSTAP */

	if(nsd.server_kind == NSD_SERVER_MAIN) {
		server_prepare_xfrd(&nsd);
		/* xfrd forks this before reading database, so it does not get
		 * the memory size of the database */
		server_start_xfrd(&nsd, 0, 0);
		/* close zonelistfile in non-xfrd processes */
		zone_list_close(nsd.options);
	}
	if (server_prepare(&nsd) != 0) {
		unlinkpid(nsd.pidfile);
		error("server preparation failed, %s could "
			"not be started", argv0);
	}
	if(nsd.server_kind == NSD_SERVER_MAIN) {
		server_send_soa_xfrd(&nsd, 0);
	}

	/* Really take off */
	log_msg(LOG_NOTICE, "%s started (%s), pid %d",
		argv0, PACKAGE_STRING, (int) nsd.pid);

	if (nsd.server_kind == NSD_SERVER_MAIN) {
		server_main(&nsd);
	} else {
		server_child(&nsd);
	}

	/* NOTREACH */
	exit(0);
}
