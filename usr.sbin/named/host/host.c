/*
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * Originally, this program came from Rutgers University, however it
 * is based on nslookup and other pieces of named tools, so it needs
 * that copyright notice.
 */

/*
 * Rewritten by Eric Wassenaar, Nikhef-H, <e07@nikhef.nl>
 *
 * The officially maintained source of this program is available
 * via anonymous ftp from machine 'ftp.nikhef.nl' [192.16.199.1]
 * in the directory '/pub/network' as 'host.tar.Z'
 *
 * You are kindly requested to report bugs and make suggestions
 * for improvements to the author at the given email address,
 * and to not re-distribute your own modifications to others.
 */

#ifndef lint
static char Version[] = "@(#)host.c	e07@nikhef.nl (Eric Wassenaar) 951231";
#endif

#if defined(apollo) && defined(lint)
#define __attribute(x)
#endif

#define justfun			/* this is only for fun */
#undef  obsolete		/* old code left as a reminder */
#undef  notyet			/* new code for possible future use */

/*
 *			New features
 *
 * - Major overhaul of the entire code.
 * - Very rigid error checking, with more verbose error messages.
 * - Zone listing section completely rewritten.
 * - It is now possible to do recursive listings into delegated zones.
 * - Maintain resource record statistics during zone listings.
 * - Maintain count of hosts during zone listings.
 * - Check for various extraneous conditions during zone listings.
 * - Check for illegal domain names containing invalid characters.
 * - Verify that certain domain names represent canonical host names.
 * - Perform ttl consistency checking during zone listings.
 * - Exploit multiple server addresses if available.
 * - Option to exploit only primary server for zone transfers.
 * - Option to exclude info from names that do not reside in a zone.
 * - Implement timeout handling during connect and read.
 * - Write resource record output to optional log file.
 * - Special MB tracing by recursively expanding MR and MG records.
 * - Special mode to check SOA records at each nameserver for a zone.
 * - Special mode to check reverse mappings of host addresses.
 * - Extended syntax allows multiple arguments on command line or stdin.
 * - Configurable default options in HOST_DEFAULTS environment variable.
 * - Implement new resource record types from RFC 1183 and 1348.
 * - Basic experimental NSAP support as defined in RFC 1637.
 * - Implement new resource record types from RFC 1664 and 1712.
 * - Code is extensively documented.
 */

/*
 *			Publication history
 *
 * This information has been moved to the RELEASE_NOTES file.
 */

/*
 *			Compilation options
 *
 * This program usually compiles without special compilation options,
 * but for some platforms you may have to define special settings.
 * See the Makefile and the header file port.h for details.
 */

/*
 *			Miscellaneous notes
 *
 * This program should be linked explicitly with the BIND resolver library
 * in case the default gethostbyname() or gethostbyaddr() routines use a
 * non-standard strategy for retrieving information. These functions in the
 * resolver library call on the nameserver, and fall back on the hosts file
 * only if no nameserver is running (ECONNREFUSED).
 *
 * You may also want to link this program with the BIND resolver library if
 * your default library has not been compiled with DEBUG printout enabled.
 *
 * The version of the resolver should be BIND 4.8.2 or later. The crucial
 * include files are <netdb.h>, (resolv.h>, <arpa/nameser.h>. These files
 * are assumed to be present in the /usr/include directory.
 *
 * The resolver code depends on the definition of the BSD pre-processor
 * variable. This variable is usually defined in the file <sys/param.h>.
 *
 * The definition of this variable determines the method how to handle
 * datagram connections. This may not work properly on all platforms.
 *
 * The hostent struct defined in <netdb.h> is assumed to handle multiple
 * addresses in h_addr_list[]. Usually this is true if BSD >= 43.
 *
 * Your nameserver may not handle queries about top-level zones properly
 * if the "domain" directive is present in the named.boot file. It will
 * append the default domain to single names for which no data is cached.
 *
 * The treatment of TXT records has changed from 4.8.2 to 4.8.3. Formerly,
 * the data consisted simply of the text string. Now, the text string is
 * preceded by the character count with a maximum of 255, and multiple
 * strings are embedded if the total character count exceeds 255.
 * We handle only the new situation in this program, assuming that nobody
 * uses TXT records before 4.8.3 (unfortunately this is not always true:
 * current vendor supplied software may sometimes be even pre-BIND 4.8.2).
 *
 * Note that in 4.8.3 PACKETSZ from nameser.h is still at 512, which is
 * the maximum possible packet size for datagrams, whereas MAXDATA from
 * db.h has increased from 256 to 2048. The resolver defines MAXPACKET
 * as 1024. The nameserver reads queries in a buffer of size BUFSIZ.
 *
 * The gethostbyname() routine in 4.8.3 interprets dotted quads (if not
 * terminated with a dot) and simulates a gethostbyaddr(), but we will
 * not rely on it, and handle dotted quads ourselves.
 *
 * On some systems a bug in the _doprnt() routine exists which prevents
 * printf("%.*s", n, string) to be printed correctly if n == 0.
 *
 * This program has not been optimized for speed. Especially the memory
 * management is simple and straightforward.
 */

/*
 *			Terminology used
 *
 * Gateway hosts.
 * These are hosts that have more than one address registered under
 * the same name. Obviously we cannot recognize a gateway host if it
 * has different names associated with its different addresses.
 *
 * Duplicate hosts.
 * These are non-gateway hosts of which the address was found earlier
 * but with a different name, possibly in a totally different zone.
 * Such hosts should not be counted again in the overall host count.
 * This situation notably occurs in e.g. the "ac.uk" domain which has
 * many names registered in both the long and the abbreviated form,
 * such as 'host.department.university.ac.uk' and 'host.dept.un.ac.uk'.
 * This is probably not an error per se. It is an error if some domain
 * has registered a foreign address under a name within its own domain.
 * To recognize duplicate hosts when traversing many zones, we have to
 * maintain a global list of host addresses. To simplify things, only
 * single-address hosts are handled as such.
 *
 * Extrazone hosts.
 * These are hosts which belong to a zone but which are not residing
 * directly within the zone under consideration and which are not
 * glue records for a delegated zone of the given zone. E.g. if we are
 * processing the zone 'bar' and find 'host.foo.bar' but 'foo.bar' is not
 * an NS registered delegated zone of 'bar' then it is considered to be
 * an extrazone host. This is not necessarily an error, but it could be.
 *
 * Lame delegations.
 * If we query the SOA record of a zone at a supposedly authoritative
 * nameserver for that zone (listed in the NS records for the zone),
 * the SOA record should be present and the answer authoritative.
 * If not, we flag a lame delegation of the zone to that nameserver.
 * This may need refinement in some special cases.
 * A lame delegation is also flagged if we discover that a nameserver
 * mentioned in an NS record does not exist when looking up its address.
 *
 * Primary nameserver.
 * This utility assumes that the first domain name in the RHS of the
 * SOA record for a zone contains the name of the primary nameserver
 * (or one of the primary nameservers) for that zone. Unfortunately,
 * this field has not been unambiguously defined. Nevertheless, many
 * hostmasters interpret the definitions given in RFC 1033 and 1035
 * as such, and therefore host will continue doing so. Interpretation
 * as the machine that holds the zone data disk file is pretty useless.
 */

/*
 *		Usage: host [options] name [server]
 *		Usage: host [options] -x [name ...]
 *		Usage: host [options] -X server [name ...]
 *
 * Regular command line options:
 * ----------------------------
 *
 * -t type	specify query type; default is T_A for normal mode
 * -a		specify query type T_ANY
 * -v		print verbose messages (-vv is very verbose)
 * -d		print debugging output (-dd prints even more)
 *
 * Special mode options.
 * --------------------
 *
 * -l		special mode to generate zone listing for a zone
 * -L level	do recursive zone listing/checking this level deep
 * -p		use primary nameserver of zone for zone transfers
 * -P server	give priority to preferred servers for zone transfers
 * -N zone	do not perform zone transfer for these explicit zones
 * -S		print zone resource record statistics
 * -H		special mode to count hosts residing in a zone
 * -G		same as -H but lists gateway hosts in addition
 * -E		same as -H but lists extrazone hosts in addition
 * -D		same as -H but lists duplicate hosts in addition
 * -C		special mode to check SOA records for a zone
 * -A		special mode to check reverse mappings of host addresses
 *
 * Miscellaneous options.
 * ---------------------
 *
 * -f filename	log resource record output also in given file
 * -F filename	same as -f, but exchange role of stdout and log file
 * -I chars	chars are not considered illegal in domain names
 * -i		generate reverse in-addr.arpa query for dotted quad
 * -n		generate reverse nsap.int query for dotted nsap address
 * -q		be quiet about some non-fatal errors
 * -T		print ttl value during non-verbose output
 * -Z		print selected RR output in full zone file format
 *
 * Seldom used options.
 * -------------------
 *
 * -c class	specify query class; default is C_IN
 * -e		exclude info from names that do not reside in the zone
 * -m		specify query type T_MAILB and trace MB records
 * -o		suppress resource record output to stdout
 * -r		do not use recursion when querying nameserver
 * -R		repeatedly add search domains to qualify queryname
 * -s secs	specify timeout value in seconds; default is 2 * 5
 * -u		use virtual circuit instead of datagram for queries
 * -w		wait until nameserver becomes available
 *
 * Undocumented options. (Experimental, subject to change)
 * --------------------
 *
 * -g length	only select names that are at least this long
 * -B		enforce full BIND behavior during DNSRCH
 * -M		special mode to list mailable delegated zones of zone
 * -W		special mode to list wildcard records in a zone
 * -z		special mode to list delegated zones in a zone
 */

static char Usage[] =
"\
Usage:      host [-v] [-a] [-t querytype] [options]  name  [server]\n\
Listing:    host [-v] [-a] [-t querytype] [options]  -l zone  [server]\n\
Hostcount:  host [-v] [options] -H [-D] [-E] [-G] zone\n\
Check soa:  host [-v] [options] -C zone\n\
Addrcheck:  host [-v] [options] -A host\n\
Listing options: [-L level] [-S] [-A] [-p] [-P prefserver] [-N skipzone]\n\
Common options:  [-d] [-f|-F filename] [-I chars] [-i|-n] [-q] [-T] [-Z]\n\
Other options:   [-c class] [-e] [-m] [-o] [-r] [-R] [-s secs] [-u] [-w]\n\
Extended usage:  [-x [name ...]] [-X server [name ...]]\
";

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>

#include <sys/types.h>		/* not always automatically included */
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>

#undef NOERROR			/* in <sys/streams.h> on solaris 2.x */
#include <arpa/nameser.h>
#include <resolv.h>

#include "port.h"		/* various portability definitions */
#include "conf.h"		/* various configuration definitions */
#include "type.h"		/* types should be in <arpa/nameser.h> */
#include "exit.h"		/* exit codes come from <sysexits.h> */

typedef int	bool;		/* boolean type */
#define TRUE	1
#define FALSE	0

#ifndef NO_DATA
#define NO_DATA	NO_ADDRESS	/* used here only in case authoritative */
#endif

#define NO_RREC	(NO_DATA + 1)	/* used for non-authoritative NO_DATA */
#define NO_HOST	(NO_DATA + 2)	/* used for non-authoritative HOST_NOT_FOUND */

#define QUERY_REFUSED  (NO_DATA + 3)	/* query was refused by server */
#define SERVER_FAILURE (NO_DATA + 4)	/* instead of TRY_AGAIN upon SERVFAIL */
#define HOST_NOT_CANON (NO_DATA + 5)	/* host name is not canonical */

#define T_NONE	0		/* yet unspecified resource record type */
#define T_FIRST	T_A		/* first possible type in resource record */
#define T_LAST	(T_AXFR - 1)	/* last  possible type in resource record */

#ifndef NOCHANGE
#define NOCHANGE 0xf		/* compatibility with older BIND versions */
#endif

#define NOT_DOTTED_QUAD	((ipaddr_t)-1)
#define BROADCAST_ADDR	((ipaddr_t)0xffffffff)
#define LOCALHOST_ADDR	((ipaddr_t)0x7f000001)

#if PACKETSZ > 1024
#define MAXPACKET PACKETSZ
#else
#define MAXPACKET 1024
#endif

typedef union {
	HEADER header;
	u_char packet[MAXPACKET];
} querybuf;

#ifndef HFIXEDSZ
#define HFIXEDSZ 12		/* actually sizeof(HEADER) */
#endif

#define MAXDLEN (MAXPACKET - HFIXEDSZ)	/* upper bound for dlen */

#include "rrec.h"		/* resource record structures */

#define input			/* read-only input parameter */
#define output			/* modified output parameter */

#define STDIN	0
#define STDOUT	1
#define STDERR	2

#ifdef lint
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN int errno;
EXTERN int h_errno;		/* defined in gethostnamadr.c */
EXTERN res_state_t _res;	/* defined in res_init.c */
extern char *dbprefix;		/* prefix for debug messages (send.c) */
extern char *version;		/* program version number (vers.c) */

char **optargv = NULL;		/* argument list including default options */
int optargc = 0;		/* number of arguments in new argument list */

int errorcount = 0;		/* global error count */

int record_stats[T_ANY+1];	/* count of resource records per type */

char cnamebuf[MAXDNAME+1];
char *cname = NULL;		/* name to which CNAME is aliased */

char mnamebuf[MAXDNAME+1];
char *mname = NULL;		/* name to which MR or MG is aliased */

char soanamebuf[MAXDNAME+1];
char *soaname = NULL;		/* domain name of SOA record */

char subnamebuf[MAXDNAME+1];
char *subname = NULL;		/* domain name of NS record */

char adrnamebuf[MAXDNAME+1];
char *adrname = NULL;		/* domain name of A record */

ipaddr_t address;		/* internet address of A record */

char *listhost = NULL;		/* actual host queried during zone listing */

char serverbuf[MAXDNAME+1];
char *server = NULL;		/* name of explicit server to query */

char realnamebuf[2*MAXDNAME+2];
char *realname = NULL;		/* the actual name that was queried */

FILE *logfile = NULL;		/* default is stdout only */
bool logexchange = FALSE;	/* exchange role of log file and stdout */

char *illegal = NULL;		/* give warning about illegal domain names */
char *skipzone = NULL;		/* zone(s) for which to skip zone transfer */
char *prefserver = NULL;	/* preferred server(s) for zone listing */

char *queryname = NULL;		/* the name about which to query */
int querytype = T_NONE;		/* the type of the query */
int queryclass = C_IN;		/* the class of the query */
ipaddr_t queryaddr;		/* set if name to query is dotted quad */

int debug = 0;			/* print resolver debugging output */
int verbose = 0;		/* verbose mode for extra output */

#ifdef justfun
int namelen = 0;		/* select records exceeding this length */
#endif

int recursive = 0;		/* recursive listmode maximum level */
int recursion_level = 0;	/* current recursion level */
int skip_level = 0;		/* level beyond which to skip checks */

bool quiet = FALSE;		/* suppress non-fatal warning messages */
bool reverse = FALSE;		/* generate reverse in-addr.arpa queries */
bool revnsap = FALSE;		/* generate reverse nsap.int queries */
bool primary = FALSE;		/* use primary server for zone transfers */
bool suppress = FALSE;		/* suppress resource record output */
bool dotprint = FALSE;		/* print trailing dot in non-listing mode */
bool ttlprint = FALSE;		/* print ttl value in non-verbose mode */
bool waitmode = FALSE;		/* wait until server becomes available */
bool mailmode = FALSE;		/* trace MG and MR into MB records */
bool addrmode = FALSE;		/* check reverse mappings of addresses */
bool listmode = FALSE;		/* generate zone listing of a zone */
bool hostmode = FALSE;		/* count real hosts residing within zone */
bool duplmode = FALSE;		/* list duplicate hosts within zone */
bool extrmode = FALSE;		/* list extrazone hosts within zone */
bool gatemode = FALSE;		/* list gateway hosts within zone */
bool checkmode = FALSE;		/* check SOA records at each nameserver */
bool mxdomains = FALSE;		/* list MX records for each delegated zone */
bool wildcards = FALSE;		/* list only wildcard records in a zone */
bool listzones = FALSE;		/* list only delegated zones in a zone */
bool exclusive = FALSE;		/* exclude records that are not in zone */
bool recurskip = FALSE;		/* skip certain checks during recursion */
bool statistics = FALSE;	/* print resource record statistics */
bool bindcompat = FALSE;	/* enforce full BIND DNSRCH compatibility */
bool classprint = FALSE;	/* print class value in non-verbose mode */

#include "defs.h"		/* declaration of functions */

#define lower(c)	(((c) >= 'A' && (c) <= 'Z') ? (c) + 'a' - 'A' : (c))
#define hexdigit(n)	(((n) < 10) ? '0' + (n) : 'A' + (n) - 10);

#define is_xdigit(c)	(isascii(c) && isxdigit(c))
#define is_space(c)	(isascii(c) && isspace(c))
#define is_alnum(c)	(isascii(c) && isalnum(c))
#define is_upper(c)	(isascii(c) && isupper(c))

#define bitset(a,b)	(((a) & (b)) != 0)
#define sameword(a,b)	(strcasecmp(a,b) == 0)
#define samepart(a,b)	(strncasecmp(a,b,strlen(b)) == 0)
#define samehead(a,b)	(strncasecmp(a,b,sizeof(b)-1) == 0)

#define fakename(a)	(samehead(a,"localhost.") || samehead(a,"loopback."))
#define nulladdr(a)	(((a) == 0) || ((a) == BROADCAST_ADDR))
#define fakeaddr(a)	(nulladdr(a) || ((a) == htonl(LOCALHOST_ADDR)))
#define incopy(a)	*((struct in_addr *)a)

#define newlist(a,n,t)	(t *)xalloc((ptr_t *)a, (siz_t)((n)*sizeof(t)))
#define newstring(s)	(char *)xalloc((ptr_t *)NULL, (siz_t)(strlen(s)+1))
#define newstr(s)	strcpy(newstring(s), s)
#define xfree(a)	(void) free((ptr_t *)a)

#define strlength(s)	(int)strlen(s)
#define in_string(s,c)	(index(s,c) != NULL)

#define plural(n)	(((n) == 1) ? "" : "s")
#define plurale(n)	(((n) == 1) ? "" : "es")

#ifdef DEBUG
#define assert(condition)\
{\
	if (!(condition))\
	{\
		(void) fprintf(stderr, "assertion botch: ");\
		(void) fprintf(stderr, "%s(%d): ", __FILE__, __LINE__);\
		(void) fprintf(stderr, "%s\n", "condition");\
		exit(EX_SOFTWARE);\
	}\
}
#else
#define assert(condition)
#endif

/*
** MAIN -- Start of program host
** -----------------------------
**
**	Exits:
**		EX_OK		Operation successfully completed
**		EX_UNAVAILABLE	Could not obtain requested information
**		EX_CANTCREAT	Could not create specified log file
**		EX_NOINPUT	No input arguments were found
**		EX_NOHOST	Could not lookup explicit server
**		EX_OSERR	Could not obtain resources
**		EX_USAGE	Improper parameter/option specified
**		EX_SOFTWARE	Assertion botch in DEBUG mode
*/

int
main(argc, argv)
input int argc;
input char *argv[];
{
	register char *option;
	res_state_t new_res;		/* new resolver database */
	int result;			/* result status of action taken */
	char *program;			/* name that host was called with */
	char *servername = NULL;	/* name of explicit server */
	char *logfilename = NULL;	/* name of log file */
	bool extended = FALSE;		/* accept extended argument syntax */

	assert(sizeof(int) >= 4);	/* probably paranoid */
#ifdef obsolete
	assert(sizeof(u_short) == 2);	/* perhaps less paranoid */
	assert(sizeof(ipaddr_t) == 4);	/* but this is critical */
#endif

/*
 * Synchronize stdout and stderr in case output is redirected.
 */
	linebufmode(stdout);

/*
 * Initialize resolver, set new defaults. See show_res() for details.
 * The old defaults are (RES_RECURSE | RES_DEFNAMES | RES_DNSRCH)
 */
	(void) res_init();

	_res.options |=  RES_DEFNAMES;	/* qualify single names */
	_res.options &= ~RES_DNSRCH;	/* dotted names are qualified */

	_res.options |=  RES_RECURSE;	/* request nameserver recursion */
	_res.options &= ~RES_DEBUG;	/* turn off debug printout */
	_res.options &= ~RES_USEVC;	/* do not use virtual circuit */

	_res.retry = 2;		/* number  of retries, default = 4 */
	_res.retrans = 5;	/* timeout in seconds, default = 5 or 6 */

	/* initialize packet id */
	_res.id = getpid() & 0x7fff;

	/* save new defaults */
	new_res = _res;

/*
 * Check whether host was called with a different name.
 * Interpolate default options and parameters.
 */
	if (argc < 1 || argv[0] == NULL)
		fatal(Usage);

	option = getenv("HOST_DEFAULTS");
	if (option != NULL)
	{
		set_defaults(option, argc, argv);
		argc = optargc; argv = optargv;
	}

	program = rindex(argv[0], '/');
	if (program++ == NULL)
		program = argv[0];

	/* check for resource record names */
	querytype = parse_type(program);
	if (querytype < 0)
		querytype = T_NONE;

	/* check for zone listing abbreviation */
	if (sameword(program, "zone"))
		listmode = TRUE;

/*
 * Scan command line options and flags.
 */
	while (argc > 1 && argv[1] != NULL && argv[1][0] == '-')
	{
	    for (option = &argv[1][1]; *option != '\0'; option++)
	    {
		switch (*option)
		{
		    case 'w' :
			waitmode = TRUE;
			new_res.retry = 2;
			new_res.retrans = 5;
			break;

		    case 's' :
			if (argv[2] == NULL || argv[2][0] == '-')
				fatal("Missing timeout value");
			new_res.retry = 2;
			new_res.retrans = atoi(argv[2]);
			if (new_res.retrans <= 0)
				fatal("Invalid timeout value %s", argv[2]);
			argv++; argc--;
			break;

		    case 'r' :
			new_res.options &= ~RES_RECURSE;
			break;

		    case 'B' :
			bindcompat = TRUE;
			/*FALLTHROUGH*/

		    case 'R' :
			new_res.options |= RES_DNSRCH;
			break;

		    case 'u' :
			new_res.options |= RES_USEVC;
			break;

		    case 'd' :
			new_res.options |= RES_DEBUG;
			debug++;		/* increment debugging level */
			break;

		    case 'v' :
			verbose++;		/* increment verbosity level */
			break;

		    case 'q' :
			quiet = TRUE;
			break;

		    case 'i' :
			reverse = TRUE;
			break;

		    case 'n' :
			revnsap = TRUE;
			break;

		    case 'p' :
			primary = TRUE;
			break;

		    case 'o' :
			suppress = TRUE;
			break;

		    case 'e' :
			exclusive = TRUE;
			break;

		    case 'S' :
			statistics = TRUE;
			break;

		    case 'T' :
			ttlprint = TRUE;
			break;

		    case 'Z' :
			dotprint = TRUE;
			ttlprint = TRUE;
			classprint = TRUE;
			break;

		    case 'A' :
			addrmode = TRUE;
			break;

		    case 'D' :
		    case 'E' :
		    case 'G' :
		    case 'H' :
			if (*option == 'D')
				duplmode = TRUE;
			if (*option == 'E')
				extrmode = TRUE;
			if (*option == 'G')
				gatemode = TRUE;
			hostmode = TRUE;
			listmode = TRUE;
			if (querytype == T_NONE)
				querytype = -1;	/* suppress zone data output */
			break;

		    case 'C' :
			checkmode = TRUE;
			listmode = TRUE;
			if (querytype == T_NONE)
				querytype = -1;	/* suppress zone data output */
			break;

		    case 'z' :
			listzones = TRUE;
			listmode = TRUE;
			if (querytype == T_NONE)
				querytype = -1;	/* suppress zone data output */
			break;

		    case 'M' :
			mxdomains = TRUE;
			listmode = TRUE;
			if (querytype == T_NONE)
				querytype = -1;	/* suppress zone data output */
			break;

		    case 'W' :
			wildcards = TRUE;
			listmode = TRUE;
			if (querytype == T_NONE)
				querytype = T_MX;
			break;

		    case 'L' :
			if (argv[2] == NULL || argv[2][0] == '-')
				fatal("Missing recursion level");
			recursive = atoi(argv[2]);
			if (recursive <= 0)
				fatal("Invalid recursion level %s", argv[2]);
			argv++; argc--;
			/*FALLTHROUGH*/

		    case 'l' :
			listmode = TRUE;
			break;

		    case 'c' :
			if (argv[2] == NULL || argv[2][0] == '-')
				fatal("Missing query class");
			queryclass = parse_class(argv[2]);
			if (queryclass < 0)
				fatal("Invalid query class %s", argv[2]);
			argv++; argc--;
			break;

		    case 't' :
			if (argv[2] == NULL || argv[2][0] == '-')
				fatal("Missing query type");
			querytype = parse_type(argv[2]);
			if (querytype < 0)
				fatal("Invalid query type %s", argv[2]);
			argv++; argc--;
			break;

		    case 'a' :
			querytype = T_ANY;	/* filter anything available */
			break;

		    case 'm' :
			mailmode = TRUE;
			querytype = T_MAILB;	/* filter MINFO/MG/MR/MB data */
			break;

		    case 'I' :
			if (argv[2] == NULL || argv[2][0] == '-')
				fatal("Missing allowed chars");
			illegal = argv[2];
			argv++; argc--;
			break;

		    case 'P' :
			if (argv[2] == NULL || argv[2][0] == '-')
				fatal("Missing preferred server");
			prefserver = argv[2];
			argv++; argc--;
			break;

		    case 'N' :
			if (argv[2] == NULL || argv[2][0] == '-')
				fatal("Missing zone to be skipped");
			skipzone = argv[2];
			argv++; argc--;
			break;

		    case 'F' :
			logexchange = TRUE;
			/*FALLTHROUGH*/

		    case 'f' :
			if (argv[2] == NULL || argv[2][0] == '-')
				fatal("Missing log file name");
			logfilename = argv[2];
			argv++; argc--;
			break;

		    case 'X' :
			if (argv[2] == NULL || argv[2][0] == '-')
				fatal("Missing server name");
			servername = argv[2];
			argv++; argc--;
			/*FALLTHROUGH*/

		    case 'x' :
			extended = TRUE;
			break;
#ifdef justfun
		    case 'g' :
			if (argv[2] == NULL || argv[2][0] == '-')
				fatal("Missing minimum length");
			namelen = atoi(argv[2]);
			if (namelen <= 0)
				fatal("Invalid minimum length %s", argv[2]);
			argv++; argc--;
			break;
#endif
		    case 'V' :
			printf("Version %s\n", version);
			exit(EX_OK);

		    default:
			fatal(Usage);
		}
	    }

	    argv++; argc--;
	}

/*
 * Check the remaining arguments.
 */
	/* old syntax must have at least one argument */
	if (!extended && (argc < 2 || argv[1] == NULL || argc > 3))
		fatal(Usage);

	/* old syntax has explicit server as second argument */
	if (!extended && (argc > 2 && argv[2] != NULL))
		servername = argv[2];

/*
 * Open log file if requested.
 */
	if (logfilename != NULL)
		set_logfile(logfilename);

/*
 * Set default preferred server for zone listings, if not specified.
 */
	if (listmode && (prefserver == NULL))
		prefserver = myhostname();

/*
 * Check for possible alternative server. Use new resolver defaults.
 */
	if (servername != NULL)
		set_server(servername);

/*
 * Do final resolver initialization.
 * Show resolver parameters and special environment options.
 */
	/* set new resolver values changed by command options */
	_res.retry = new_res.retry;
	_res.retrans = new_res.retrans;
	_res.options = new_res.options;

	/* show the new resolver database */
	if (debug > 1 || verbose > 1)
		show_res();

	/* show customized default domain */
	option = getenv("LOCALDOMAIN");
	if (option != NULL && verbose > 1)
		printf("Explicit local domain %s\n\n", option);

/*
 * Process command line argument(s) depending on syntax.
 */
	if (!extended) /* only one argument */
		result = process_name(argv[1]);

	else if (argc < 2) /* no arguments */
		result = process_file(stdin);

	else /* multiple command line arguments */
		result = process_argv(argc, argv);

/*
 * Report result status of action taken.
 */
	exit(result);
	/*NOTREACHED*/
}

/*
** SET_DEFAULTS -- Interpolate default options and parameters in argv
** ------------------------------------------------------------------
**
**	The HOST_DEFAULTS environment variable gives customized options.
**
**	Returns:
**		None.
**
**	Outputs:
**		Creates ``optargv'' vector with ``optargc'' arguments.
*/

void
set_defaults(option, argc, argv)
input char *option;			/* option string */
input int argc;				/* original command line arg count */
input char *argv[];			/* original command line arguments */
{
	register char *p, *q;
	register int i;

/*
 * Allocate new argument vector.
 */
	optargv = newlist(NULL, 2, char *);
	optargv[0] = argv[0];
	optargc = 1;

/*
 * Construct argument list from option string.
 */
	for (q = "", p = newstr(option); *p != '\0'; p = q)
	{
		while (is_space(*p))
			p++;

		if (*p == '\0')
			break;

		for (q = p; *q != '\0' && !is_space(*q); q++)
			continue;

		if (*q != '\0')
			*q++ = '\0';

		optargv = newlist(optargv, optargc+2, char *);
		optargv[optargc] = p;
		optargc++;
	}

/*
 * Append command line arguments.
 */
	for (i = 1; i < argc && argv[i] != NULL; i++)
	{
		optargv = newlist(optargv, optargc+2, char *);
		optargv[optargc] = argv[i];
		optargc++;
	}

	/* and terminate */
	optargv[optargc] = NULL;
}

/*
** PROCESS_ARGV -- Process command line arguments
** ----------------------------------------------
**
**	Returns:
**		EX_OK if information was obtained successfully.
**		Appropriate exit code otherwise.
*/

int
process_argv(argc, argv)
input int argc;
input char *argv[];
{
	register int i;
	int result;			/* result status of action taken */
	int excode = EX_NOINPUT;	/* overall result status */

	for (i = 1; i < argc && argv[i] != NULL; i++)
	{
		/* process a single argument */
		result = process_name(argv[i]);

		/* maintain overall result */
		if (result != EX_OK || excode == EX_NOINPUT)
			excode = result;
	}

	/* return overall result */
	return(excode);
}

/*
** PROCESS_FILE -- Process arguments from input file
** -------------------------------------------------
**
**	Returns:
**		EX_OK if information was obtained successfully.
**		Appropriate exit code otherwise.
*/

int
process_file(fp)
input FILE *fp;				/* input file with query names */
{
	register char *p, *q;
	char buf[BUFSIZ];
	int result;			/* result status of action taken */
	int excode = EX_NOINPUT;	/* overall result status */

	while (fgets(buf, sizeof(buf), fp) != NULL)
	{
		p = index(buf, '\n');
		if (p != NULL)
			*p = '\0';

		/* extract names separated by whitespace */
		for (q = "", p = buf; *p != '\0'; p = q)
		{
			while (is_space(*p))
				p++;

			/* ignore comment lines */
			if (*p == '\0' || *p == '#' || *p == ';')
				break;

			for (q = p; *q != '\0' && !is_space(*q); q++)
				continue;

			if (*q != '\0')
				*q++ = '\0';

			/* process a single argument */
			result = process_name(p);

			/* maintain overall result */
			if (result != EX_OK || excode == EX_NOINPUT)
				excode = result;
		}
	}

	/* return overall result */
	return(excode);
}

/*
** PROCESS_NAME -- Process a single command line argument
** ------------------------------------------------------
**
**	Returns:
**		EX_OK if information was obtained successfully.
**		Appropriate exit code otherwise.
**
**	Wrapper for execute_name() to hide administrative tasks.
*/

int
process_name(name)
input char *name;			/* command line argument */
{
	int result;			/* result status of action taken */
	static int save_querytype;
	static bool save_reverse;
	static bool firstname = TRUE;

	/* separate subsequent pieces of output */
	if (!firstname && (verbose || debug || checkmode))
		printf("\n");

/*
 * Some global variables are redefined further on. Save their initial
 * values in the first pass, and restore them during subsequent passes.
 */
	if (firstname)
	{
		save_querytype = querytype;
		save_reverse = reverse;
		firstname = FALSE;
	}
	else
	{
		querytype = save_querytype;
		reverse = save_reverse;
	}

/*
 * Do the real work.
 */
	result = execute_name(name);
	return(result);
}

/*
** EXECUTE_NAME -- Process a single command line argument
** ------------------------------------------------------
**
**	Returns:
**		EX_OK if information was obtained successfully.
**		Appropriate exit code otherwise.
**
**	Outputs:
**		Defines ``queryname'' and ``queryaddr'' appropriately.
**
**	Side effects:
**		May redefine ``querytype'' and ``reverse'' if necessary.
*/

int
execute_name(name)
input char *name;			/* command line argument */
{
	bool result;			/* result status of action taken */

	/* check for nonsense input name */
	if (strlength(name) > MAXDNAME)
	{
		errmsg("Query name %s too long", name);
		return(EX_USAGE);
	}

/*
 * Analyze the name and type to be queried about.
 * The name can be an ordinary domain name, or an internet address
 * in dotted quad notation. If the -n option is given, the name is
 * supposed to be a dotted nsap address.
 */
	queryname = name;
	if (queryname[0] == '\0')
		queryname = ".";

	if (sameword(queryname, "."))
		queryaddr = NOT_DOTTED_QUAD;
	else
		queryaddr = inet_addr(queryname);

/*
 * Generate reverse in-addr.arpa query if so requested.
 * The input name must be a dotted quad, and be convertible.
 */
	if (reverse)
	{
		if (queryaddr == NOT_DOTTED_QUAD)
			name = queryname, queryname = NULL;
		else
			queryname = in_addr_arpa(queryname);

		if (queryname == NULL)
		{
			errmsg("Invalid dotted quad %s", name);
			return(EX_USAGE);
		}

		queryaddr = NOT_DOTTED_QUAD;
	}

/*
 * Generate reverse nsap.int query if so requested.
 * The input name must be a dotted nsap, and be convertible.
 */
	if (revnsap)
	{
		if (reverse)
			name = queryname, queryname = NULL;
		else
			queryname = nsap_int(queryname);

		if (queryname == NULL)
		{
			errmsg("Invalid nsap address %s", name);
			return(EX_USAGE);
		}

		queryaddr = NOT_DOTTED_QUAD;
		reverse = TRUE;
	}

/*
 * In regular mode, the querytype is used to formulate the nameserver
 * query, and any response is filtered out when processing the answer.
 * In listmode, the querytype is used to filter out the proper records.
 */
	/* set querytype for regular mode if unspecified */
	if ((querytype == T_NONE) && !listmode)
	{
		if ((queryaddr != NOT_DOTTED_QUAD) || reverse)
			querytype = T_PTR;
		else
			querytype = T_A;
	}

/*
 * Check for incompatible options.
 */
	/* cannot have dotted quad in listmode */
	if (listmode && (queryaddr != NOT_DOTTED_QUAD))
	{
		errmsg("Invalid query name %s", queryname);
		return(EX_USAGE);
	}

	/* must have regular name or dotted quad in addrmode */
	if (!listmode && addrmode && reverse)
	{
		errmsg("Invalid query name %s", queryname);
		return(EX_USAGE);
	}

	/* show what we are going to query about */
	if (verbose)
		show_types(queryname, querytype, queryclass);

/*
 * All set. Perform requested function.
 */
	result = execute(queryname, queryaddr);
	return(result ? EX_OK : EX_UNAVAILABLE);
}

/*
** EXECUTE -- Perform the requested function
** -----------------------------------------
**
**	Returns:
**		TRUE if information was obtained successfully.
**		FALSE otherwise.
**
**	The whole environment has been set up and checked.
*/

bool
execute(name, addr)
input char *name;			/* name to query about */
input ipaddr_t addr;			/* explicit address of query */
{
	bool result;			/* result status of action taken */

/*
 * Special mode to list contents of specified zone.
 */
	if (listmode)
	{
		result = list_zone(name);
		return(result);
	}

/*
 * Special mode to check reverse mappings of host addresses.
 */
	if (addrmode)
	{
		if (addr == NOT_DOTTED_QUAD)
			result = check_addr(name);
		else
			result = check_name(addr);
		return(result);
	}

/*
 * Regular mode to query about specified host.
 */
	result = host_query(name, addr);
	return(result);
}

/*
** HOST_QUERY -- Regular mode to query about specified host
** --------------------------------------------------------
**
**	Returns:
**		TRUE if information was obtained successfully.
**		FALSE otherwise.
*/

bool
host_query(name, addr)
input char *name;			/* name to query about */
input ipaddr_t addr;			/* explicit address of query */
{
	struct hostent *hp;
	struct in_addr inaddr;
	char newnamebuf[MAXDNAME+1];
	char *newname = NULL;		/* name to which CNAME is aliased */
	int ncnames = 0;		/* count of CNAMEs in chain */
	bool result;			/* result status of action taken */

	inaddr.s_addr = addr;

	result = FALSE;
	h_errno = TRY_AGAIN;

	/* retry until positive result or permanent failure */
	while (result == FALSE && h_errno == TRY_AGAIN)
	{
		/* reset before each query to avoid stale data */
		errno = 0;
		realname = NULL;

		if (addr == NOT_DOTTED_QUAD)
		{
			/* reset CNAME indicator */
			cname = NULL;

			/* lookup the name in question */
			if (newname == NULL)
				result = get_hostinfo(name, FALSE);
			else
				result = get_hostinfo(newname, TRUE);

			/* recurse on CNAMEs, but not too deep */
			if (cname && (querytype != T_CNAME))
			{
				newname = strcpy(newnamebuf, cname);

				if (++ncnames > 5)
				{
					errmsg("Possible CNAME loop");
					return(FALSE);
				}

				result = FALSE;
				h_errno = TRY_AGAIN;
				continue;
			}
		}
		else
		{
			hp = gethostbyaddr((char *)&inaddr, INADDRSZ, AF_INET);
			if (hp != NULL)
			{
				print_host("Name", hp);
				result = TRUE;
			}
		}

		/* only retry if so requested */
		if (!waitmode)
			break;
	}

	/* use actual name if available */
	if (realname != NULL)
		name = realname;

	/* explain the reason of a failure */
	if (result == FALSE)
		ns_error(name, querytype, queryclass, server);

	return(result);
}

/*
** MYHOSTNAME -- Determine our own fully qualified host name
** ---------------------------------------------------------
**
**	Returns:
**		Pointer to own host name.
**		Aborts if host name could not be determined.
*/

char *
myhostname()
{
	struct hostent *hp;
	static char mynamebuf[MAXDNAME+1];
	static char *myname = NULL;

	if (myname == NULL)
	{
		if (gethostname(mynamebuf, MAXDNAME) < 0)
		{
			perror("gethostname");
			exit(EX_OSERR);
		}
		mynamebuf[MAXDNAME] = '\0';

		hp = gethostbyname(mynamebuf);
		if (hp == NULL)
		{
			ns_error(mynamebuf, T_A, C_IN, server);
			errmsg("Error in looking up own name");
			exit(EX_NOHOST);
		}

		/* cache the result */
		myname = strcpy(mynamebuf, hp->h_name);
	}

	return(myname);
}

/*
** SET_SERVER -- Override default nameserver with explicit server
** --------------------------------------------------------------
**
**	Returns:
**		None.
**		Aborts the program if an unknown host was given.
**
**	Side effects:
**		The global variable ``server'' is set to indicate
**		that an explicit server is being used.
**
**	The default nameserver addresses in the resolver database
**	which are initialized by res_init() from /etc/resolv.conf
**	are replaced with the (possibly multiple) addresses of an
**	explicitly named server host. If a dotted quad is given,
**	only that single address will be used.
**
**	The answers from such server must be interpreted with some
**	care if we don't know beforehand whether it can be trusted.
*/

void
set_server(name)
input char *name;			/* name of server to be queried */
{
	register int i;
	struct hostent *hp;
	struct in_addr inaddr;
	ipaddr_t addr;			/* explicit address of server */

	/* check for nonsense input name */
	if (strlength(name) > MAXDNAME)
	{
		errmsg("Server name %s too long", name);
		exit(EX_USAGE);
	}

/*
 * Overrule the default nameserver addresses.
 */
	addr = inet_addr(name);
	inaddr.s_addr = addr;

	if (addr == NOT_DOTTED_QUAD)
	{
		/* lookup all of its addresses; this must not fail */
		hp = gethostbyname(name);
		if (hp == NULL)
		{
			ns_error(name, T_A, C_IN, server);
			errmsg("Error in looking up server name");
			exit(EX_NOHOST);
		}

		for (i = 0; i < MAXNS && hp->h_addr_list[i]; i++)
		{
			nslist(i).sin_family = AF_INET;
			nslist(i).sin_port = htons(NAMESERVER_PORT);
			nslist(i).sin_addr = incopy(hp->h_addr_list[i]);
		}
		_res.nscount = i;
	}
	else
	{
		/* lookup the name, but use only the given address */
		hp = gethostbyaddr((char *)&inaddr, INADDRSZ, AF_INET);

		nslist(0).sin_family = AF_INET;
		nslist(0).sin_port = htons(NAMESERVER_PORT);
		nslist(0).sin_addr = inaddr;
		_res.nscount = 1;
	}

/*
 * Indicate the use of an explicit server.
 */
	if (hp != NULL)
	{
		server = strcpy(serverbuf, hp->h_name);
		if (verbose)
			print_host("Server", hp);
	}
	else
	{
		server = strcpy(serverbuf, inet_ntoa(inaddr));
		if (verbose)
			printf("Server: %s\n\n", server);
	}
}

/*
** SET_LOGFILE -- Initialize optional log file
** -------------------------------------------
**
**	Returns:
**		None.
**		Aborts the program if the file could not be created.
**
**	Side effects:
**		The global variable ``logfile'' is set to indicate
**		that resource record output is to be written to it.
**
**	Swap ordinary stdout and log file output if so requested.
*/

void
set_logfile(filename)
input char *filename;			/* name of log file */
{
	if (logexchange)
	{
		logfile = fdopen(dup(STDOUT), "w");
		if (logfile == NULL)
		{
			perror("fdopen");
			exit(EX_OSERR);
		}

		if (freopen(filename, "w", stdout) == NULL)
		{
			perror(filename);
			exit(EX_CANTCREAT);
		}
	}
	else
	{
		logfile = fopen(filename, "w");
		if (logfile == NULL)
		{
			perror(filename);
			exit(EX_CANTCREAT);
		}
	}
}

/*
** FATAL -- Abort program when illegal option encountered
** ------------------------------------------------------
**
**	Returns:
**		Aborts after issuing error message.
*/

void /*VARARGS1*/
fatal(fmt, a, b, c, d)
input char *fmt;			/* format of message */
input char *a, *b, *c, *d;		/* optional arguments */
{
	(void) fprintf(stderr, fmt, a, b, c, d);
	(void) fprintf(stderr, "\n");
	exit(EX_USAGE);
}


/*
** ERRMSG -- Issue error message to error output
** ---------------------------------------------
**
**	Returns:
**		None.
**
**	Side effects:
**		Increments the global error count.
*/

void /*VARARGS1*/
errmsg(fmt, a, b, c, d)
input char *fmt;			/* format of message */
input char *a, *b, *c, *d;		/* optional arguments */
{
	(void) fprintf(stderr, fmt, a, b, c, d);
	(void) fprintf(stderr, "\n");

	/* flag an error */
	errorcount++;
}

/*
** GET_HOSTINFO -- Principal routine to query about given name
** -----------------------------------------------------------
**
**	Returns:
**		TRUE if requested info was obtained successfully.
**		FALSE otherwise.
**
**	This is the equivalent of the resolver module res_search().
**
**	In this program RES_DEFNAMES is always on, and RES_DNSRCH
**	is off by default. This means that single names without dot
**	are always, and only, tried within the own default domain,
**	and compound names are assumed to be already fully qualified.
**
**	The default BIND behavior can be simulated by turning on
**	RES_DNSRCH with -R. The given name, whether or not compound,
**	is then	first tried within the possible search domains.
**
**	Note. In the latter case, the search terminates in case the
**	specified name exists but does not have the desired type.
**	The BIND behavior is to continue the search. This can be
**	simulated with the undocumented option -B.
*/

bool
get_hostinfo(name, qualified)
input char *name;			/* name to query about */
input bool qualified;			/* assume fully qualified if set */
{
	register char **domain;
	register char *cp;
	int dot;			/* number of dots in query name */
	bool result;			/* result status of action taken */
	char oldnamebuf[2*MAXDNAME+2];
	char *oldname;			/* saved actual name when NO_DATA */
	int nodata = 0;			/* NO_DATA status during DNSRCH */
	int nquery = 0;			/* number of extra search queries */

/*
 * Single dot means root zone.
 */
	if (sameword(name, "."))
		qualified = TRUE;

/*
 * Names known to be fully qualified are just tried ``as is''.
 */
	if (qualified)
	{
		result = get_domaininfo(name, (char *)NULL);
		return(result);
	}

/*
 * Count number of dots. Move to the end of the name.
 */
	for (dot = 0, cp = name; *cp != '\0'; cp++)
		if (*cp == '.')
			dot++;

/*
 * Check for aliases of single name.
 * Note that the alias is supposed to be fully qualified.
 */
	if (dot == 0 && (cp = hostalias(name)) != NULL)
	{
		if (verbose)
			printf("Aliased to \"%s\"\n", cp);

		result = get_domaininfo(cp, (char *)NULL);
		return(result);
	}

/*
 * Trailing dot means absolute (fully qualified) address.
 */
	if (dot != 0 && cp[-1] == '.')
	{
		cp[-1] = '\0';
		result = get_domaininfo(name, (char *)NULL);
		cp[-1] = '.';
		return(result);
	}

/*
 * Append own default domain and other search domains if appropriate.
 */
	if ((dot == 0 && bitset(RES_DEFNAMES, _res.options)) ||
	    (dot != 0 && bitset(RES_DNSRCH, _res.options)))
	{
		for (domain = _res.dnsrch; *domain; domain++)
		{
			result = get_domaininfo(name, *domain);
			if (result)
				return(result);

			/* keep count of extra search queries */
			nquery++;

			/* in case nameserver not present */
			if (errno == ECONNREFUSED)
				return(FALSE);

			/* if no further search desired (single name) */
	    		if (!bitset(RES_DNSRCH, _res.options))
				break;

			/* if name exists but has not requested type */
			if (h_errno == NO_DATA || h_errno == NO_RREC)
			{
				if (bindcompat)
				{
					/* remember status and search up */
					oldname = strcpy(oldnamebuf, realname);
					nodata = h_errno;
					continue;
				}

				return(FALSE);
			}

			/* retry only if name does not exist at all */
			if (h_errno != HOST_NOT_FOUND && h_errno != NO_HOST)
				break;
		}
	}

/*
 * Single name lookup failed.
 */
	if (dot == 0)
	{
		/* unclear what actual name should be */
		if (nquery != 1)
			realname = NULL;

		/* restore nodata status from search */
		if (bindcompat && nodata)
		{
			realname = strcpy(realnamebuf, oldname);
			h_errno = nodata;
		}

		/* set status in case we never queried */
		if (!bitset(RES_DEFNAMES, _res.options))
			h_errno = HOST_NOT_FOUND;

		return(FALSE);
	}

/*
 * Rest means fully qualified.
 */
	result = get_domaininfo(name, (char *)NULL);

	/* restore nodata status from search */
	if (!result && bindcompat && nodata)
	{
		realname = strcpy(realnamebuf, oldname);
		h_errno = nodata;
	}

	return(result);
}

/*
** GET_DOMAININFO -- Fetch and print desired info about name in domain
** -------------------------------------------------------------------
**
**	Returns:
**		TRUE if requested info was obtained successfully.
**		FALSE otherwise.
**
**	Side effects:
**		Sets global variable ``realname'' to actual name queried.
**
**	This is the equivalent of the resolver module res_querydomain().
**
**	Things get a little complicated in case RES_DNSRCH is on.
**	If we get an answer but the data is corrupted, an error will be
**	returned and NO_RECOVERY will be set. This will terminate the
**	extra search loop, but a compound name will still be tried as-is.
**	The same holds if the query times out or we have a server failure,
**	in which case an error will be returned and TRY_AGAIN will be set.
**	For now we take this for granted. Normally RES_DNSRCH is disabled.
**	In this default case we do only one query and we have no problem.
*/

bool
get_domaininfo(name, domain)
input char *name;			/* name to query about */
input char *domain;			/* domain to which name is relative */
{
	char namebuf[2*MAXDNAME+2];	/* buffer to store full domain name */
	querybuf answer;
	register int n;
	bool result;			/* result status of action taken */

/*
 * Show what we are about to query.
 */
	if (verbose)
	{
		if (domain == NULL || domain[0] == '\0')
			printf("Trying %s", name);
		else
			printf("Trying %s within %s", name, domain);

		if (server && (verbose > 1))
			printf(" at server %s", server);

		printf(" ...\n");
	}

/*
 * Construct the actual domain name.
 * A null domain means the given name is already fully qualified.
 * If the composite name is too long, res_mkquery() will fail.
 */
	if (domain == NULL || domain[0] == '\0')
		(void) sprintf(namebuf, "%.*s", MAXDNAME, name);
	else
		(void) sprintf(namebuf, "%.*s.%.*s",
				MAXDNAME, name, MAXDNAME, domain);
	name = namebuf;

/*
 * Fetch the desired info.
 */
	n = get_info(&answer, name, querytype, queryclass);
	result = (n < 0) ? FALSE : TRUE;

/*
 * Print the relevant data.
 * If we got a positive answer, the data may still be corrupted.
 */
	if (result)
		result = print_info(&answer, n, name, querytype, FALSE);

/*
 * Remember the actual name that was queried.
 * Must be at the end to avoid clobbering during recursive calls.
 */
	realname = strcpy(realnamebuf, name);

	return(result);
}

/*
** GET_INFO -- Basic routine to issue a nameserver query
** -----------------------------------------------------
**
**	Returns:
**		Length of nameserver answer buffer, if obtained.
**		-1 if an error occurred (h_errno is set appropriately).
**
**	This is the equivalent of the resolver module res_query().
*/

int
get_info(answerbuf, name, type, class)
output querybuf *answerbuf;		/* location of buffer to store answer */
input char *name;			/* full name to query about */
input int type;				/* specific resource record type */
input int class;			/* specific resource record class */
{
	querybuf query;
	HEADER *bp;
	int ancount;
	register int n;

/*
 * Construct query, and send it to the nameserver.
 * res_send() will fail if no nameserver responded. In the BIND version the
 * possible values for errno are ECONNREFUSED and ETIMEDOUT. If we did get
 * an answer, errno should be reset, since res_send() may have left an errno
 * in case it has used datagrams. Our private version of res_send() will leave
 * also other error statuses, and will clear errno if an answer was obtained.
 */
	errno = 0;	/* reset before querying nameserver */

	n = res_mkquery(QUERY, name, class, type, (qbuf_t *)NULL, 0,
			(rrec_t *)NULL, (qbuf_t *)&query, sizeof(querybuf));
	if (n < 0)
	{
		if (debug)
			printf("%sres_mkquery failed\n", dbprefix);
		h_errno = NO_RECOVERY;
		return(-1);
	}

	n = res_send((qbuf_t *)&query, n, (qbuf_t *)answerbuf, sizeof(querybuf));
	if (n < 0)
	{
		if (debug)
			printf("%sres_send failed\n", dbprefix);
		h_errno = TRY_AGAIN;
		return(-1);
	}

	errno = 0;	/* reset after we got an answer */

	if (n < HFIXEDSZ)
	{
		pr_error("answer length %s too short after %s query for %s",
			itoa(n), pr_type(type), name);
		h_errno = NO_RECOVERY;
		return(-1);
	}

/*
 * Analyze the status of the answer from the nameserver.
 */
	if (debug || verbose)
		print_status(answerbuf);

	bp = (HEADER *)answerbuf;
	ancount = ntohs(bp->ancount);

	if (bp->rcode != NOERROR || ancount == 0)
	{
		switch (bp->rcode)
		{
		    case NXDOMAIN:
			/* distinguish between authoritative or not */
			h_errno = bp->aa ? HOST_NOT_FOUND : NO_HOST;
			break;

		    case NOERROR:
			/* distinguish between authoritative or not */
			h_errno = bp->aa ? NO_DATA : NO_RREC;
			break;

		    case SERVFAIL:
			h_errno = SERVER_FAILURE; /* instead of TRY_AGAIN */
			break;

		    case REFUSED:
			h_errno = QUERY_REFUSED; /* instead of NO_RECOVERY */
			break;

		    default:
			h_errno = NO_RECOVERY; /* FORMERR NOTIMP NOCHANGE */
			break;
		}
		return(-1);
	}

	h_errno = 0;
	return(n);
}

/*
** PRINT_INFO -- Check resource records in answer and print relevant data
** ----------------------------------------------------------------------
**
**	Returns:
**		TRUE if answer buffer was processed successfully.
**		FALSE otherwise.
**
**	Side effects:
**		Will recurse on MAILB records if appropriate.
**		See also side effects of the print_rrec() routine.
*/

bool
print_info(answerbuf, answerlen, name, type, listing)
input querybuf *answerbuf;		/* location of answer buffer */
input int answerlen;			/* length of answer buffer */
input char *name;			/* full name we are querying about */
input int type;				/* record type we are querying about */
input bool listing;			/* set if this is a zone listing */
{
	HEADER *bp;
	int qdcount, ancount, nscount, arcount;
	u_char *msg, *eom;
	register u_char *cp;

	bp = (HEADER *)answerbuf;
	qdcount = ntohs(bp->qdcount);
	ancount = ntohs(bp->ancount);
	nscount = ntohs(bp->nscount);
	arcount = ntohs(bp->arcount);

	msg = (u_char *)answerbuf;
	eom = (u_char *)answerbuf + answerlen;
	cp  = (u_char *)answerbuf + HFIXEDSZ;

/*
 * Skip the query section in the response (present only in normal queries).
 */
	if (qdcount)
	{
		while (qdcount > 0 && cp < eom)
		{
			/* cp += dn_skipname(cp, eom) + QFIXEDSZ; */
			cp = skip_qrec(name, cp, msg, eom);
			if (cp == NULL)
				return(FALSE);
			qdcount--;
		}

		if (qdcount)
		{
			pr_error("invalid qdcount after %s query for %s",
				pr_type(type), name);
			h_errno = NO_RECOVERY;
			return(FALSE);
		}
	}

/*
 * Process the actual answer section in the response.
 * During zone transfers, this is the only section available.
 */
	if (ancount)
	{
		if (!listing && verbose && !bp->aa)
			printf("The following answer is not authoritative:\n");

		while (ancount > 0 && cp < eom)
		{
			cp = print_rrec(name, cp, msg, eom, listing);
			if (cp == NULL)
				return(FALSE);
			ancount--;

		/*
		 * When we ask for address and there is a CNAME, it returns
		 * both the CNAME and the address.  Since we trace down the
		 * CNAME chain ourselves, we don't really want to print the
		 * address at this point.
		 */
			if (!listmode && !verbose && cname)
				return(TRUE);

		/*
		 * Recursively expand MR or MG records into MB records.
		 */
			if (!listmode && mailmode && mname)
			{
				char newnamebuf[MAXDNAME+1];
				char *newname;

				newname = strcpy(newnamebuf, mname);
				mname = NULL;

				(void) get_recursive(newname);
			}
		}

		if (ancount)
		{
			pr_error("invalid ancount after %s query for %s",
				pr_type(type), name);
			h_errno = NO_RECOVERY;
			return(FALSE);
		}
	}

/*
 * The nameserver and additional info section are normally not processed.
 * Both sections shouldn't exist in zone transfers.
 */
	if (!verbose || exclusive)
		return(TRUE);

	if (nscount)
	{
		printf("Authoritative nameservers:\n");

		while (nscount > 0 && cp < eom)
		{
			cp = print_rrec(name, cp, msg, eom, FALSE);
			if (cp == NULL)
				return(FALSE);
			nscount--;
		}

		if (nscount)
		{
			pr_error("invalid nscount after %s query for %s",
				pr_type(type), name);
			h_errno = NO_RECOVERY;
			return(FALSE);
		}
	}

	if (arcount)
	{
		printf("Additional information:\n");

		while (arcount > 0 && cp < eom)
		{
			cp = print_rrec(name, cp, msg, eom, FALSE);
			if (cp == NULL)
				return(FALSE);
			arcount--;
		}

		if (arcount)
		{
			pr_error("invalid arcount after %s query for %s",
				pr_type(type), name);
			h_errno = NO_RECOVERY;
			return(FALSE);
		}
	}

	return(TRUE);
}

/*
** PRINT_DATA -- Output resource record data if this record is wanted
** ------------------------------------------------------------------
**
**	Returns:
**		None.
**
**	Inputs:
**		The global variable ``doprint'' is set by print_rrec()
**		if we need to print the data.
*/

static bool doprint;		/* indicates whether or not to print */

void /*VARARGS1*/
print_data(fmt, a, b, c, d)
input char *fmt;			/* format of message */
input char *a, *b, *c, *d;		/* optional arguments */
{
	/* if (doprint) */
	{
		if (!suppress)
			printf(fmt, a, b, c, d);

		if (logfile != NULL)
			(void) fprintf(logfile, fmt, a, b, c, d);
	}
}

#define doprintf(x)\
{\
	if (doprint)\
	{\
		print_data x ;\
	}\
}

/*
** PRINT_RREC -- Decode single resource record and output relevant data
** --------------------------------------------------------------------
**
**	Returns:
**		Pointer to position in answer buffer after current record.
**		NULL if there was a format error in the current record.
**
**	Outputs:
**		The global variable ``doprint'' is set appropriately
**		for use by print_data().
**
**	Side effects:
**		Updates resource record statistics in record_stats[].
**		Sets ``soaname'' if this is an SOA record.
**		Sets ``subname'' if this is an NS record.
**		Sets ``adrname'' if this is an A record.
**		Sets ``address'' if this is an A record.
**		Sets ``cname'' if this is a valid CNAME record.
**		Sets ``mname'' if this is a valid MAILB record.
**		These variables must have been cleared before calling
**		print_info() and may be checked afterwards.
*/

/* print domain names after certain conversions */
#define pr_name(x)	pr_domain(x, listing)

/* check the LHS record name of these records for invalid characters */
#define test_valid(t)	((t == T_A && !reverse) || t == T_MX || t == T_AAAA)

/* check the RHS domain name of these records for canonical host names */
#define test_canon(t)	(t == T_NS || t == T_MX)

u_char *
print_rrec(name, cp, msg, eom, listing)
input char *name;			/* full name we are querying about */
register u_char *cp;			/* current position in answer buf */
input u_char *msg, *eom;		/* begin and end of answer buf */
input bool listing;			/* set if this is a zone listing */
{
	char rname[MAXDNAME+1];		/* record name in LHS */
	char dname[MAXDNAME+1];		/* domain name in RHS */
	int type, class, ttl, dlen;	/* fixed values in every record */
	u_char *eor;			/* predicted position of next record */
	bool classmatch;		/* set if we want to see this class */
	char *host = listhost;		/* contacted host for zone listings */
	register int n, c;
	struct in_addr inaddr;
	struct protoent *protocol;
	struct servent *service;

/*
 * Pickup the standard values present in each resource record.
 */
	n = expand_name(name, T_NONE, cp, msg, eom, rname);
	if (n < 0)
		return(NULL);
	cp += n;

	n = 3*INT16SZ + INT32SZ;
	if (check_size(rname, T_NONE, cp, msg, eom, n) < 0)
		return(NULL);

	type = _getshort(cp);
	cp += INT16SZ;

	class = _getshort(cp);
	cp += INT16SZ;

	ttl = _getlong(cp);
	cp += INT32SZ;

	dlen = _getshort(cp);
	cp += INT16SZ;

	eor = cp + dlen;

/*
 * Decide whether or not to print this resource record.
 */
	if (listing)
	{
		classmatch = want_class(class, queryclass);
		doprint = classmatch && want_type(type, querytype);
	}
	else
	{
		classmatch = want_class(class, C_ANY);
		doprint = classmatch && want_type(type, T_ANY);
	}

#ifdef obsolete
	if (doprint && exclusive && !samedomain(rname, name, TRUE))
		doprint = FALSE;
#endif
	if (doprint && exclusive && !indomain(rname, name, TRUE))
		doprint = FALSE;

	if (doprint && exclusive && fakename(rname))
		doprint = FALSE;

	if (doprint && wildcards && !in_string(rname, '*'))
		doprint = FALSE;
#ifdef justfun
	if (namelen && (strlength(rname) < namelen))
		doprint = FALSE;
#endif

/*
 * Print name and common values, if appropriate.
 */
	doprintf(("%-20s", pr_name(rname)))

	if (verbose || ttlprint)
		doprintf(("\t%s", itoa(ttl)))

	if (verbose || classprint || (class != queryclass))
		doprintf(("\t%s", pr_class(class)))

	doprintf(("\t%s", pr_type(type)))

/*
 * Update resource record statistics for zone listing.
 */
	if (listing && classmatch)
	{
		if (type >= T_FIRST && type <= T_LAST)
			record_stats[type]++;
	}

/*
 * Save the domain name of an SOA or NS or A record for zone listing.
 */
	if (listing && classmatch)
	{
		if (type == T_A)
			adrname = strcpy(adrnamebuf, rname);

		else if (type == T_NS)
			subname = strcpy(subnamebuf, rname);

		else if (type == T_SOA)
			soaname = strcpy(soanamebuf, rname);
	}

/*
 * Print type specific data, if appropriate.
 */
	switch (type)
	{
	    case T_A:
		if (class == C_IN || class == C_HS)
		{
			if (dlen == INADDRSZ)
			{
				bcopy((char *)cp, (char *)&inaddr, INADDRSZ);
				address = inaddr.s_addr;
				doprintf(("\t%s", inet_ntoa(inaddr)))
				cp += INADDRSZ;
				break;
			}

			if (dlen == INADDRSZ + 1 + INT16SZ)
			{
				bcopy((char *)cp, (char *)&inaddr, INADDRSZ);
				address = inaddr.s_addr;
				doprintf(("\t%s", inet_ntoa(inaddr)))
				cp += INADDRSZ;

				n = *cp++;
				doprintf((" ; proto = %s", itoa(n)))

				n = _getshort(cp);
				doprintf((", port = %s", itoa(n)))
				cp += INT16SZ;
				break;
			}

			address = 0;
			break;
		}

		address = 0;
		cp += dlen;
		break;

	    case T_MX:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", itoa(n)))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_NS:
	    case T_PTR:
	    case T_CNAME:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;
		break;

	    case T_HINFO:
		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t\"%s\"", stoa(cp, n)))
		cp += n;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t\"%s\"", stoa(cp, n)))
		cp += n;
		break;

	    case T_SOA:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;

		n = 5*INT32SZ;
		if (check_size(rname, type, cp, msg, eor, n) < 0)
			break;
		doprintf((" ("))

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", utoa(n)))
		doprintf(("\t;serial (version)"))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", itoa(n)))
		doprintf(("\t;refresh period (%s)", pr_time(n, FALSE)))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", itoa(n)))
		doprintf(("\t;retry interval (%s)", pr_time(n, FALSE)))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", itoa(n)))
		doprintf(("\t;expire time (%s)", pr_time(n, FALSE)))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", itoa(n)))
		doprintf(("\t;default ttl (%s)", pr_time(n, FALSE)))
		cp += INT32SZ;

		doprintf(("\n\t\t\t)"))
		break;

	    case T_WKS:
		if (check_size(rname, type, cp, msg, eor, INADDRSZ) < 0)
			break;
		bcopy((char *)cp, (char *)&inaddr, INADDRSZ);
		doprintf(("\t%s", inet_ntoa(inaddr)))
		cp += INADDRSZ;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		protocol = getprotobynumber(n);
		if (protocol != NULL)
			doprintf((" %s", protocol->p_name))
		else
			doprintf((" %s", itoa(n)))

		doprintf((" ("))
		n = 0;
		while (cp < eor)
		{
		    c = *cp++;
		    do
		    {
 			if (c & 0200)
			{
			    int port;

			    port = htons(n);
			    if (protocol != NULL)
				    service = getservbyport(port, protocol->p_name);
			    else
				    service = NULL;

			    if (service != NULL)
				    doprintf((" %s", service->s_name))
			    else
				    doprintf((" %s", itoa(n)))
			}
 			c <<= 1;
		    } while (++n & 07);
		}
		doprintf((" )"))
		break;

#ifdef obsolete
	    case T_TXT:
		if (dlen > 0)
		{
			doprintf(("\t\"%s\"", stoa(cp, dlen)))
			cp += dlen;
		}
		break;
#endif

	    case T_TXT:
		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t\"%s", stoa(cp, n)))
		cp += n;

		while (cp < eor)
		{
			if (check_size(rname, type, cp, msg, eor, 1) < 0)
				break;
			n = *cp++;
			doprintf(("%s", stoa(cp, n)))
			cp += n;
		}
		doprintf(("\""))
		break;

	    case T_MINFO:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_MB:
	    case T_MG:
	    case T_MR:
	    case T_MD:
	    case T_MF:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;
		break;

	    case T_UID:
	    case T_GID:
		if (dlen == INT32SZ)
		{
			n = _getlong(cp);
			doprintf(("\t%s", itoa(n)))
			cp += INT32SZ;
		}
		break;

	    case T_UINFO:
		doprintf(("\t\"%s\"", stoa(cp, dlen)))
		cp += dlen;
		break;

	    case T_RP:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_RT:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", itoa(n)))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_AFSDB:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", itoa(n)))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_X25:
		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t%s", stoa(cp, n)))
		cp += n;
		break;

	    case T_ISDN:
		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t%s", stoa(cp, n)))
		cp += n;

		if (cp < eor)
		{
			if (check_size(rname, type, cp, msg, eor, 1) < 0)
				break;
			n = *cp++;
			doprintf((" %s", stoa(cp, n)))
			cp += n;
		}
		break;

	    case T_NSAP:
		doprintf(("\t0x%s", nsap_ntoa(cp, dlen)))
		cp += dlen;
		break;

	    case T_NSAPPTR:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;
		break;

	    case T_PX:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", itoa(n)))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_GPOS:
		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t%s", stoa(cp, n)))
		cp += n;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t%s", stoa(cp, n)))
		cp += n;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t%s", stoa(cp, n)))
		cp += n;
		break;

	    case T_LOC:
		if ((n = *cp) != T_LOC_VERSION)
		{
			pr_error("invalid version %s in %s record for %s",
				itoa(n), pr_type(type), rname);
			cp += dlen;
			break;
		}

		n = INT32SZ + 3*INT32SZ;
		if (check_size(rname, type, cp, msg, eor, n) < 0)
			break;
		c = _getlong(cp);
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\t%s ", pr_spherical(n, "N", "S")))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf((" %s ", pr_spherical(n, "E", "W")))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf((" %sm ", pr_vertical(n, "", "-")))
		cp += INT32SZ;

		doprintf((" %sm", pr_precision((c >> 16) & 0xff)))
		doprintf((" %sm", pr_precision((c >>  8) & 0xff)))
		doprintf((" %sm", pr_precision((c >>  0) & 0xff)))
		break;

	    case T_UNSPEC:
	    case T_NULL:
		cp += dlen;
		break;

	    case T_SIG:
	    case T_KEY:
	    case T_AAAA:
		doprintf(("\t(not yet implemented)"))
		cp += dlen;
		break;

	    default:
		doprintf(("\t???"))
		cp += dlen;
		break;
	}

/*
 * Terminate resource record printout.
 */
	doprintf(("\n"))

/*
 * Check whether we have reached the exact end of this resource record.
 * If not, we cannot be sure that the record has been decoded correctly,
 * and therefore the subsequent tests will be skipped.
 */
	if (cp != eor)
	{
		pr_error("size error in %s record for %s, off by %s",
			pr_type(type), rname, itoa(cp - eor));

		/* we believe value of dlen; should perhaps return(NULL) */
		return(eor);
	}

/*
 * Save the CNAME alias for cname chain tracing.
 * Save the MR or MG alias for MB chain tracing.
 * These features can be enabled only in normal mode.
 */
	if (!listmode && classmatch)
	{
		if (type == T_CNAME)
			cname = strcpy(cnamebuf, dname);

		else if (type == T_MR || type == T_MG)
			mname = strcpy(mnamebuf, dname);
	}

/*
 * Suppress the subsequent checks in quiet mode.
 * This can safely be done as there are no side effects.
 * It may speedup things, and nothing would be printed anyway.
 */
	if (quiet)
		return(cp);

/*
 * In zone listings, resource records with the same name/type/class
 * must have the same ttl value. Maintain and check list of record info.
 * This is done on a per-zone basis.
 */
	if (listing && !check_ttl(rname, type, class, ttl))
	{
		pr_warning("%s %s records have different ttl within %s from %s",
			rname, pr_type(type), name, host);
	}

/*
 * Check validity of 'host' related domain names in certain resource records.
 * These include LHS record names and RHS domain names of selected records.
 * Currently underscores are not reported during deep recursive listings.
 */
	if (test_valid(type) && !valid_name(rname, TRUE, FALSE, recurskip))
	{
		pr_warning("%s %s record has illegal name",
			rname, pr_type(type));
	}

	if (test_canon(type) && !valid_name(dname, FALSE, FALSE, recurskip))
	{
		pr_warning("%s %s host %s has illegal name",
			rname, pr_type(type), dname);
	}

/*
 * The RHS of various resource records should refer to a canonical host name,
 * i.e. it should exist and have an A record and not be a CNAME.
 * Currently this test is suppressed during deep recursive zone listings.
 */
	if (!recurskip && test_canon(type) && (n = check_canon(dname)) != 0)
	{
		/* only report definitive target host failures */
		if (n == HOST_NOT_FOUND)
			pr_warning("%s %s host %s does not exist",
				rname, pr_type(type), dname);
		else if (n == NO_DATA)
			pr_warning("%s %s host %s has no A record",
				rname, pr_type(type), dname);
		else if (n == HOST_NOT_CANON)
			pr_warning("%s %s host %s is not canonical",
				rname, pr_type(type), dname);

		/* authoritative failure to find nameserver target host */
		if (type == T_NS && (n == NO_DATA || n == HOST_NOT_FOUND))
		{
			if (server == NULL)
				errmsg("%s has lame delegation to %s",
					rname, dname);
		}
	}

/*
 * On request, reverse map the address of an A record, and verify that
 * it is registered and maps back to the name of the A record.
 * Currently this option has effect here only during zone listings.
 */
	if (addrmode && (type == T_A && !reverse) && !fakeaddr(address))
	{
		host = mapreverse(rname, inaddr);
		if (host == NULL)
			pr_warning("%s address %s is not registered",
				rname, inet_ntoa(inaddr));
		else if (host != rname)
			pr_warning("%s address %s maps to %s",
				rname, inet_ntoa(inaddr), host);
	}

/*
 * This record was processed successfully.
 */
	return(cp);
}

/*
** SKIP_QREC -- Skip the query record in the nameserver answer buffer
** ------------------------------------------------------------------
**
**	Returns:
**		Pointer to position in answer buffer after current record.
**		NULL if there was a format error in the current record.
*/

u_char *
skip_qrec(name, cp, msg, eom)
input char *name;			/* full name we are querying about */
register u_char *cp;			/* current position in answer buf */
input u_char *msg, *eom;		/* begin and end of answer buf */
{
	char rname[MAXDNAME+1];		/* record name in LHS */
	int type, class;		/* fixed values in query record */
	register int n;

	n = expand_name(name, T_NONE, cp, msg, eom, rname);
	if (n < 0)
		return(NULL);
	cp += n;

	n = 2*INT16SZ;
	if (check_size(rname, T_NONE, cp, msg, eom, n) < 0)
		return(NULL);

	type = _getshort(cp);
	cp += INT16SZ;

	class = _getshort(cp);
	cp += INT16SZ;

#ifdef lint
	if (verbose)
		printf("%-20s\t%s\t%s\n",
			rname, pr_class(class), pr_type(type));
#endif
	return(cp);
}

/*
** GET_RECURSIVE -- Wrapper for get_hostinfo() during recursion
** ------------------------------------------------------------
**
**	Returns:
**		TRUE if requested info was obtained successfully.
**		FALSE otherwise.
*/

bool
get_recursive(name)
input char *name;			/* name to query about */
{
	static int level = 0;		/* recursion level */
	bool result;			/* result status of action taken */
	int save_errno;
	int save_herrno;

	if (level > 5)
	{
		errmsg("Recursion too deep");
		return(FALSE);
	}

	save_errno = errno;
	save_herrno = h_errno;

	level++;
	result = get_hostinfo(name, TRUE);
	level--;

	errno = save_errno;
	h_errno = save_herrno;

	return(result);
}


/*
 * Nameserver information.
 * Stores names and addresses of all servers that are to be queried
 * for a zone transfer of the desired zone. Normally these are the
 * authoritative primary and/or secondary nameservers for the zone.
 */

char nsname[MAXNSNAME][MAXDNAME+1];		/* nameserver host name */
struct in_addr ipaddr[MAXNSNAME][MAXIPADDR];	/* nameserver addresses */
int naddrs[MAXNSNAME];				/* count of addresses */
int nservers = 0;				/* count of nameservers */

#ifdef notyet
typedef struct srvr_data {
	char nsname[MAXDNAME+1];		/* nameserver host name */
	struct in_addr ipaddr[MAXIPADDR];	/* nameserver addresses */
	int naddrs;				/* count of addresses */
} srvr_data_t;

srvr_data_t nsinfo[MAXNSNAME];	/* nameserver info */
#endif

bool authserver;		/* server is supposed to be authoritative */
bool lameserver;		/* server could not provide SOA service */

/*
 * Host information.
 * Stores names and (single) addresses encountered during the zone listing
 * of all A records that belong to the zone. Non-authoritative glue records
 * that do not belong to the zone are not stored. Glue records that belong
 * to a delegated zone will be filtered out later during the host count scan.
 * The host names are allocated dynamically.
#ifdef notyet
 * The host data should have been allocated dynamically to avoid static
 * limits, but this is less important since it is not saved across calls.
 * In case the static limit is reached, increase MAXHOSTS and recompile.
#endif
 */

char *hostname[MAXHOSTS];	/* host name of host in zone */
ipaddr_t hostaddr[MAXHOSTS];	/* first host address */
bool multaddr[MAXHOSTS];	/* set if this is a multiple address host */
int hostcount = 0;		/* count of hosts in zone */

#ifdef notyet
typedef struct host_data {
	char *hostname;		/* host name of host in zone */
	ipaddr_t hostaddr;	/* first host address */
	bool multaddr;		/* set if this is a multiple address host */
} host_data_t;

host_data_t hostlist[MAXHOSTS];	/* info on hosts in zone */
#endif

/*
 * Delegated zone information.
 * Stores the names of the delegated zones encountered during the zone
 * listing. The names and the list itself are allocated dynamically.
 */

char **zonename = NULL;		/* names of delegated zones within zone */
int zonecount = 0;		/* count of delegated zones within zone */

/*
 * Address information.
 * Stores the (single) addresses of hosts found in all zones traversed.
 * Used to search for duplicate hosts (same address but different name).
 * The list of addresses is allocated dynamically, and remains allocated.
 * This has now been implemented as a hashed list, using the low-order
 * address bits as the hash key.
 */

#ifdef obsolete
ipaddr_t *addrlist = NULL;	/* global list of addresses */
int addrcount = 0;		/* count of global addresses */
#endif

/*
 * SOA record information.
 */

soa_data_t soa;			/* buffer to store soa data */

/*
 * Nameserver preference.
 * As per BIND 4.9.* resource records may be returned after round-robin
 * reshuffling each time they are retrieved. For NS records, this may
 * lead to an unfavorable order for doing zone transfers.
 * We apply some heuristic to sort the NS records according to their
 * preference with respect to a given list of preferred server domains.
 */

int nsrank[MAXNSNAME];		/* nameserver ranking after sorting */
int nspref[MAXNSNAME];		/* nameserver preference value */

/*
** LIST_ZONE -- Basic routine to do complete zone listing and checking
** -------------------------------------------------------------------
**
**	Returns:
**		TRUE if the requested info was processed successfully.
**		FALSE otherwise.
*/

int total_calls = 0;		/* number of calls for zone processing */
int total_check = 0;		/* number of zones successfully processed */
int total_tries = 0;		/* number of zone transfer attempts */
int total_zones = 0;		/* number of successful zone transfers */
int total_hosts = 0;		/* number of hosts in all traversed zones */
int total_dupls = 0;		/* number of duplicates in all zones */

#ifdef justfun
char longname[MAXDNAME+1];	/* longest host name found */
int longsize = 0;		/* size of longest host name */
#endif

bool
list_zone(name)
input char *name;			/* name of zone to process */
{
	register int n;
	register int i;
	int nzones;			/* count of delegated zones */
	int nhosts;			/* count of real host names */
	int ndupls;			/* count of duplicate hosts */
	int nextrs;			/* count of extrazone hosts */
	int ngates;			/* count of gateway hosts */

	total_calls += 1;		/* update zone processing calls */

/*
 * Normalize to not have trailing dot, unless it is the root zone.
 */
	n = strlength(name);
	if (n > 1 && name[n-1] == '.')
		name[n-1] = '\0';

/*
 * Indicate whether we are processing an in-addr.arpa reverse zone.
 * In this case we will suppress accumulating host count statistics.
 */
	reverse = indomain(name, ARPA_ROOT, FALSE);

/*
 * Suppress various checks if working beyond the recursion skip level.
 * This affects processing in print_rrec(). It may need refinement.
 */
	recurskip = (recursion_level > skip_level) ? TRUE : FALSE;

/*
 * Find the nameservers for the given zone.
 */
	(void) find_servers(name);

	if (nservers < 1)
	{
		errmsg("No nameservers for %s found", name);
		return(FALSE);
	}

/*
 * Make sure we have an address for at least one nameserver.
 */
	for (n = 0; n < nservers; n++)
		if (naddrs[n] > 0)
			break;

	if (n >= nservers)
	{
		errmsg("No addresses of nameservers for %s found", name);
		return(FALSE);
	}

/*
 * Without an explicit server on the command line, the servers we
 * have looked up are supposed to be authoritative for the zone.
 */
	authserver = server ? FALSE : TRUE;

/*
 * Check SOA records at each of the nameservers if so requested.
 */
	if (checkmode)
	{
		do_check(name);

		total_check += 1;	/* update zones processed */

		/* all done if maximum recursion level reached */
		if (!recursive || (recursion_level >= recursive))
			return((errorcount == 0) ? TRUE : FALSE);
	}

/*
 * The zone transfer for certain zones can be skipped.
 * Currently this must be indicated on the command line.
 */
	if (skip_transfer(name))
	{
		if (verbose || statistics || checkmode || hostmode)
			printf("Skipping zone transfer for %s\n", name);
		return(FALSE);
	}

/*
 * Ask zone transfer to the nameservers, until one responds.
 */
	total_tries += 1;		/* update zone transfer attempts */

	if (!do_transfer(name))
		return(FALSE);

	total_zones += 1;		/* update successful zone transfers */

/*
 * Print resource record statistics if so requested.
 */
	if (statistics)
		print_statistics(name, querytype, queryclass);

/*
 * Accumulate host count statistics for this zone.
 * Do this only in modes in which such output would be printed.
 */
	nzones = zonecount;

	nhosts = 0, ndupls = 0, nextrs = 0, ngates = 0;

	i = (verbose || statistics || hostmode) ? 0 : hostcount;

	for (n = i; n < hostcount; n++)
	{
		/* skip fake hosts using a very rudimentary test */
		if (fakename(hostname[n]) || fakeaddr(hostaddr[n]))
			continue;
#ifdef justfun
		/* save longest host name encountered so far */
		if (verbose && ((i = strlength(hostname[n])) > longsize))
		{
			longsize = i;
			(void) strcpy(longname, hostname[n]);
		}
#endif
		/* skip apparent glue records */
		if (gluerecord(hostname[n], name, zonename, nzones))
		{
			if (verbose > 1)
				printf("%s is glue record\n", hostname[n]);
			continue;
		}

		/* otherwise count as host */
		nhosts++;

	/*
	 * Mark hosts not residing directly in the zone as extrazone host.
	 */
		if (!samedomain(hostname[n], name, TRUE))
		{
			nextrs++;
			if (extrmode || (verbose > 1))
				printf("%s is extrazone host\n", hostname[n]);
		}

	/*
	 * Mark hosts with more than one address as gateway host.
	 * These are not checked for duplicate addresses.
	 */
		if (multaddr[n])
		{
			ngates++;
			if (gatemode || (verbose > 1))
				printf("%s is gateway host\n", hostname[n]);
		}
		
	/*
	 * Compare single address hosts against global list of addresses.
	 * Multiple address hosts are too complicated to handle this way.
	 */
		else if (check_dupl(hostaddr[n]))
		{
			struct in_addr inaddr;
			inaddr.s_addr = hostaddr[n];

			ndupls++;
			if (duplmode || (verbose > 1))
				printf("%s is duplicate host with address %s\n",
					hostname[n], inet_ntoa(inaddr));
		}
	}

/*
 * Print statistics for this zone.
 */
	if (verbose || statistics || hostmode)
	{
		printf("Found %d host%s within %s\n",
			nhosts, plural(nhosts), name);

	    if ((ndupls > 0) || duplmode || (verbose > 1))
		printf("Found %d duplicate host%s within %s\n",
			ndupls, plural(ndupls), name);

	    if ((nextrs > 0) || extrmode || (verbose > 1))
		printf("Found %d extrazone host%s within %s\n",
			nextrs, plural(nextrs), name);

	    if ((ngates > 0) || gatemode || (verbose > 1))
		printf("Found %d gateway host%s within %s\n",
			ngates, plural(ngates), name);
	}

	total_hosts += nhosts;		/* update total number of hosts */
	total_dupls += ndupls;		/* update total number of duplicates */

	if (!checkmode)
		total_check += 1;	/* update zones processed */

	if (verbose || statistics)
		printf("Found %d delegated zone%s within %s\n",
			nzones, plural(nzones), name);

/*
 * Sort the encountered delegated zones alphabetically.
 * Note that this precludes further use of the zone_index() function.
 */
	if ((nzones > 1) && (recursive || listzones || mxdomains))
		qsort((char *)zonename, nzones, sizeof(char *), compare_name);

/*
 * The names of the hosts were allocated dynamically.
 */
	for (n = 0; n < hostcount; n++)
		xfree(hostname[n]);

/*
 * Check for mailable delegated zones within this zone.
 * This is based on ordinary MX lookup, and not on the MX info
 * which may be present in the zone listing, to reduce zone transfers.
 */
	if (mxdomains)
	{
		if (recursion_level == 0)
		{
			if (verbose)
				printf("\n");

			if (!get_mxrec(name))
				ns_error(name, T_MX, queryclass, server);
		}

		for (n = 0; n < nzones; n++)
		{
			if (verbose)
				printf("\n");

			if (!get_mxrec(zonename[n]))
				ns_error(zonename[n], T_MX, queryclass, server);
		}
	}

/*
 * Do recursion on delegated zones if requested and any were found.
 * Temporarily save zonename list, and force allocation of new list.
 */
	if (recursive && (recursion_level < recursive))
	{
		for (n = 0; n < nzones; n++)
		{
			char **newzone;		/* local copy of list */

			newzone = zonename;
			zonename = NULL;	/* allocate new list */

			if (verbose || statistics || checkmode || hostmode)
				printf("\n");

			if (listzones)
			{
				for (i = 0; i <= recursion_level; i++)
					printf("%s", (i == 0) ? "\t" : "  ");
				printf("%s\n", newzone[n]);
			}

			if (verbose)
				printf("Entering zone %s\n", newzone[n]);

			recursion_level++;
			(void) list_zone(newzone[n]);
			recursion_level--;

			zonename = newzone;	/* restore */
		}
	}
	else if (listzones)
	{
		for (n = 0; n < nzones; n++)
		{
			for (i = 0; i <= recursion_level; i++)
				printf("%s", (i == 0) ? "\t" : "  ");
			printf("%s\n", zonename[n]);
		}
	}

/*
 * The names of the delegated zones were allocated dynamically.
 * The list of delegated zone names was also allocated dynamically.
 */
	for (n = 0; n < nzones; n++)
		xfree(zonename[n]);

	if (zonename != NULL)
		xfree(zonename);

	zonename = NULL;

/*
 * Print final overall statistics.
 */
	if (recursive && (recursion_level == 0))
	{
		if (verbose || statistics || checkmode || hostmode)
			printf("\n");

		if (verbose || statistics || hostmode)
			printf("Encountered %d host%s in %d zone%s within %s\n",
				total_hosts, plural(total_hosts),
				total_zones, plural(total_zones),
				name);

		if (verbose || statistics || hostmode)
			printf("Encountered %d duplicate host%s in %d zone%s within %s\n",
				total_dupls, plural(total_dupls),
				total_zones, plural(total_zones),
				name);

		if (verbose || statistics || checkmode)
			printf("Transferred %d zone%s out of %d attempt%s\n",
				total_zones, plural(total_zones),
				total_tries, plural(total_tries));

		if (verbose || statistics || checkmode)
			printf("Processed %d zone%s out of %d request%s\n",
				total_check, plural(total_check),
				total_calls, plural(total_calls));
#ifdef justfun
		if (verbose && (longsize > 0))
			printf("Longest hostname %s\t%d\n",
				longname, longsize);
#endif
	}

	/* indicate whether any errors were encountered */
	return((errorcount == 0) ? TRUE : FALSE);
}

/*
** FIND_SERVERS -- Fetch names and addresses of authoritative servers
** ------------------------------------------------------------------
**
**	Returns:
**		TRUE if servers could be determined successfully.
**		FALSE otherwise.
**
**	Inputs:
**		The global variable ``server'', if set, contains the
**		name of the explicit server to be contacted.
**		The global variable ``primary'', if set, indicates
**		that we must use the primary nameserver for the zone.
**		If both are set simultaneously, the explicit server
**		is contacted to retrieve the desired servers.
**
**	Outputs:
**		The count of nameservers is stored in ``nservers''.
**		Names are stored in the nsname[] database.
**		Addresses are stored in the ipaddr[] database.
**		Address counts are stored in the naddrs[] database.
*/

bool
find_servers(name)
input char *name;			/* name of zone to find servers for */
{
	struct hostent *hp;
	register int n;
	register int i;

/*
 * Use the explicit server if given on the command line.
 * Its addresses are stored in the resolver state struct.
 * This server may not be authoritative for the given zone.
 */
	if (server && !primary)
	{
		(void) strcpy(nsname[0], server);
		for (i = 0; i < MAXIPADDR && i < _res.nscount; i++)
			ipaddr[0][i] = nslist(i).sin_addr;
		naddrs[0] = i;

		nservers = 1;
		return(TRUE);
	}

/*
 * Fetch primary nameserver info if so requested.
 * Get its name from the SOA record for the zone, and do a regular
 * host lookup to fetch its addresses. We are assuming here that the
 * SOA record is a proper one. This is not necessarily true.
 * Obviously this server should be authoritative.
 */
	if (primary && !server)
	{
		char *primaryname;

		primaryname = get_primary(name);
		if (primaryname == NULL)
		{
			ns_error(name, T_SOA, queryclass, server);
			nservers = 0;
			return(FALSE);
		}

		hp = gethostbyname(primaryname);
		if (hp == NULL)
		{
			ns_error(primaryname, T_A, C_IN, server);
			nservers = 0;
			return(FALSE);
		}

		(void) strcpy(nsname[0], hp->h_name);
		for (i = 0; i < MAXIPADDR && hp->h_addr_list[i]; i++)
			ipaddr[0][i] = incopy(hp->h_addr_list[i]);
		naddrs[0] = i;

		if (verbose)
			printf("Found %d address%-2s for %s\n",
				naddrs[0], plurale(naddrs[0]), nsname[0]);

		nservers = 1;
		return(TRUE);
	}

/*
 * Otherwise we have to find the nameservers for the zone.
 * These are supposed to be authoritative, but sometimes we
 * encounter lame delegations, perhaps due to misconfiguration.
 * Retrieve the NS records for this zone.
 */
	if (!get_servers(name))
	{
		ns_error(name, T_NS, queryclass, server);
		nservers = 0;
		return(FALSE);
	}

/*
 * Usually we'll get addresses for all the servers in the additional
 * info section.  But in case we don't, look up their addresses.
 * Addresses could be missing because there is no room in the answer.
 * No address is present if the name of a server is not canonical.
 * If we get no addresses by extra query, and this is authoritative,
 * we flag a lame delegation to that server.
 */
	for (n = 0; n < nservers; n++)
	{
	    if (naddrs[n] == 0)
	    {
		hp = gethostbyname(nsname[n]);
		if (hp != NULL)
		{
			for (i = 0; i < MAXIPADDR && hp->h_addr_list[i]; i++)
				ipaddr[n][i] = incopy(hp->h_addr_list[i]);
			naddrs[n] = i;
		}

		if (verbose)
			printf("Found %d address%-2s for %s by extra query\n",
				naddrs[n], plurale(naddrs[n]), nsname[n]);

		if (hp == NULL)
		{
			/* server name lookup failed */
			ns_error(nsname[n], T_A, C_IN, server);

			/* authoritative denial: probably misconfiguration */
			if (h_errno == NO_DATA || h_errno == HOST_NOT_FOUND)
			{
				errmsg("%s has lame delegation to %s",
					name, nsname[n]);
			}
		}

		if ((hp != NULL) && !sameword(hp->h_name, nsname[n]))
			pr_warning("%s nameserver %s is not canonical (%s)",
				name, nsname[n], hp->h_name);
	    }
	    else
	    {
		if (verbose)
			printf("Found %d address%-2s for %s\n",
				naddrs[n], plurale(naddrs[n]), nsname[n]);
	    }
	}

/*
 * Issue warning if only one server has been discovered.
 * This is not an error per se, but not much redundancy in that case.
 */
	if (nservers == 1)
		pr_warning("%s has only one nameserver %s",
			name, nsname[0]);

	return((nservers > 0) ? TRUE : FALSE);
}

/*
** GET_SERVERS -- Fetch names and addresses of authoritative servers
** -----------------------------------------------------------------
**
**	Returns:
**		TRUE if servers could be determined successfully.
**		FALSE otherwise.
**
**	Side effects:
**		The count of nameservers is stored in ``nservers''.
**		Names are stored in the nsname[] database.
**		Addresses are stored in the ipaddr[] database.
**		Address counts are stored in the naddrs[] database.
*/

bool
get_servers(name)
input char *name;			/* name of zone to find servers for */
{
	querybuf answer;
	register int n;
	bool result;			/* result status of action taken */

	if (verbose)
		printf("Finding nameservers for %s ...\n", name);

	n = get_info(&answer, name, T_NS, queryclass);
	if (n < 0)
		return(FALSE);

	if (verbose > 1)
		(void) print_info(&answer, n, name, T_NS, FALSE);

	result = get_nsinfo(&answer, n, name);
	return(result);
}

/*
** GET_NSINFO -- Extract nameserver data from nameserver answer buffer
** -------------------------------------------------------------------
**
**	Returns:
**		TRUE if servers could be determined successfully.
**		FALSE otherwise.
**
**	Outputs:
**		The count of nameservers is stored in ``nservers''.
**		Names are stored in the nsname[] database.
**		Addresses are stored in the ipaddr[] database.
**		Address counts are stored in the naddrs[] database.
*/

bool
get_nsinfo(answerbuf, answerlen, name)
input querybuf *answerbuf;		/* location of answer buffer */
input int answerlen;			/* length of answer buffer */
input char *name;			/* name of zone to find servers for */
{
	HEADER *bp;
	int qdcount, ancount, nscount, arcount, rrcount;
	u_char *msg, *eom;
	register u_char *cp;
	register int i;

	nservers = 0;			/* count of nameservers */

	bp = (HEADER *)answerbuf;
	qdcount = ntohs(bp->qdcount);
	ancount = ntohs(bp->ancount);
	nscount = ntohs(bp->nscount);
	arcount = ntohs(bp->arcount);

	msg = (u_char *)answerbuf;
	eom = (u_char *)answerbuf + answerlen;
	cp  = (u_char *)answerbuf + HFIXEDSZ;

	while (qdcount > 0 && cp < eom)
	{
		cp = skip_qrec(name, cp, msg, eom);
		if (cp == NULL)
			return(FALSE);
		qdcount--;
	}

	if (qdcount)
	{
		pr_error("invalid qdcount after %s query for %s",
			pr_type(T_NS), name);
		h_errno = NO_RECOVERY;
		return(FALSE);
	}

/*
 * If the answer is authoritative, the names are found in the
 * answer section, and the nameserver section is empty.
 * If not, there may be duplicate names in both sections.
 * Addresses are found in the additional info section both cases.
 */
	rrcount = ancount + nscount + arcount;
	while (rrcount > 0 && cp < eom)
	{
		char rname[MAXDNAME+1];
		char dname[MAXDNAME+1];
		int type, class, ttl, dlen;
		u_char *eor;
		register int n;
		struct in_addr inaddr;

		n = expand_name(name, T_NONE, cp, msg, eom, rname);
		if (n < 0)
			return(FALSE);
		cp += n;

		n = 3*INT16SZ + INT32SZ;
		if (check_size(rname, T_NONE, cp, msg, eom, n) < 0)
			return(FALSE);

		type = _getshort(cp);
		cp += INT16SZ;

		class = _getshort(cp);
		cp += INT16SZ;

		ttl = _getlong(cp);
		cp += INT32SZ;

		dlen = _getshort(cp);
		cp += INT16SZ;

		eor = cp + dlen;
#ifdef lint
		if (verbose)
			printf("%-20s\t%d\t%s\t%s\n",
				rname, ttl, pr_class(class), pr_type(type));
#endif
		if ((type == T_NS) && sameword(rname, name))
		{
			n = expand_name(rname, type, cp, msg, eom, dname);
			if (n < 0)
				return(FALSE);
			cp += n;

			for (i = 0; i < nservers; i++)
				if (sameword(nsname[i], dname))
					break;	/* duplicate */

			if (i >= nservers && nservers < MAXNSNAME)
			{
				(void) strcpy(nsname[nservers], dname);
				naddrs[nservers] = 0;
				nservers++;
			}
		}
		else if ((type == T_A) && dlen == INADDRSZ)
		{
			for (i = 0; i < nservers; i++)
				if (sameword(nsname[i], rname))
					break;	/* found */

			if (i < nservers && naddrs[i] < MAXIPADDR)
			{
				bcopy((char *)cp, (char *)&inaddr, INADDRSZ);
				ipaddr[i][naddrs[i]] = inaddr;
				naddrs[i]++;
			}

			cp += dlen;
		}
		else
			cp += dlen;

		if (cp != eor)
		{
			pr_error("size error in %s record for %s, off by %s",
				pr_type(type), rname, itoa(cp - eor));
			return(FALSE);
		}

		rrcount--;
	}

	if (rrcount)
	{
		pr_error("invalid rrcount after %s query for %s",
			pr_type(T_NS), name);
		h_errno = NO_RECOVERY;
		return(FALSE);
	}

	return(TRUE);
}

/*
** SORT_SERVERS -- Sort set of nameservers according to preference
** ---------------------------------------------------------------
**
**	Returns:
**		None.
**
**	Inputs:
**		Set of nameservers as determined by find_servers().
**		The global variable ``prefserver'', if set, contains
**		a list of preferred server domains to compare against.
**
**	Outputs:
**		Stores the preferred nameserver order in nsrank[].
*/

void
sort_servers()
{
	register int i, j;
	register int n, pref;
	register char *p, *q;

/*
 * Initialize the default ranking.
 */
	for (n = 0; n < nservers; n++)
	{
		nsrank[n] = n;
		nspref[n] = 0;
	}

/*
 * Determine the nameserver preference.
 * Compare against a list of comma-separated preferred server domains.
 * Use the maximum value of all comparisons.
 */
	for (q = NULL, p = prefserver; p != NULL; p = q)
	{
		q = index(p, ',');
		if (q != NULL)
			*q = '\0';

		for (n = 0; n < nservers; n++)
		{
			pref = matchlabels(nsname[n], p);
			if (pref > nspref[n])
				nspref[n] = pref;
		}

		if (q != NULL)
			*q++ = ',';
	}

/*
 * Sort the set according to preference.
 * Keep the rest as much as possible in original order.
 */
	for (i = 0; i < nservers; i++)
	{
		for (j = i + 1; j < nservers; j++)
		{
			if (nspref[j] > nspref[i])
			{
				pref = nspref[j];
				/* nspref[j] = nspref[i]; */
				for (n = j; n > i; n--)
					nspref[n] = nspref[n-1];
				nspref[i] = pref;

				pref = nsrank[j];
				/* nsrank[j] = nsrank[i]; */
				for (n = j; n > i; n--)
					nsrank[n] = nsrank[n-1];
				nsrank[i] = pref;
			}
		}
	}
}

/*
** SKIP_TRANSFER -- Check whether a zone transfer should be skipped
** ----------------------------------------------------------------
**
**	Returns:
**		TRUE if a transfer for this zone should be skipped.
**		FALSE if the zone transfer should proceed.
**
**	Inputs:
**		The global variable ``skipzone'', if set, contains
**		a list of zone names to be skipped.
**
**	Certain zones are known to contain bogus information, and
**	can be requested to be excluded from further processing.
**	The zone transfer for such zones and its delegated zones
**	will be skipped.
*/

bool
skip_transfer(name)
input char *name;			/* name of zone to process */
{
	register char *p, *q;
	bool skip = FALSE;

	for (q = NULL, p = skipzone; p != NULL; p = q)
	{
		q = index(p, ',');
		if (q != NULL)
			*q = '\0';

		if (sameword(name, p))
			skip = TRUE;

		if (q != NULL)
			*q++ = ',';
	}

	return(skip);
}

/*
** DO_CHECK -- Check SOA records at each of the nameservers
** --------------------------------------------------------
**
**	Returns:
**		None.
**
**	Inputs:
**		The count of nameservers is stored in ``nservers''.
**		Names are stored in the nsname[] database.
**		Addresses are stored in the ipaddr[] database.
**		Address counts are stored in the naddrs[] database.
**
**	The SOA record of the zone is checked at each nameserver.
**	Nameserver recursion is turned off to make sure that the
**	answer is authoritative.
*/

void
do_check(name)
input char *name;			/* name of zone to process */
{
	res_state_t save_res;		/* saved copy of resolver database */
	char *save_server;		/* saved copy of server name */
	register int n;
	register int i;

	/* save resolver database */
	save_res = _res;
	save_server = server;

	/* turn off nameserver recursion */
	_res.options &= ~RES_RECURSE;

	for (n = 0; n < nservers; n++)
	{
		if (naddrs[n] < 1)
			continue;	/* shortcut */

		server = nsname[n];
		for (i = 0; i < MAXNS && i < naddrs[n]; i++)
		{
			nslist(i).sin_family = AF_INET;
			nslist(i).sin_port = htons(NAMESERVER_PORT);
			nslist(i).sin_addr = ipaddr[n][i];
		}
		_res.nscount = i;

		/* retrieve and check SOA */
		if (check_zone(name))
			continue;

		/* SOA query failed */
		ns_error(name, T_SOA, queryclass, server);

		/* explicit server failure: possibly data expired */
		lameserver = (h_errno == SERVER_FAILURE) ? TRUE : FALSE;

		/* non-authoritative denial: assume lame delegation */
		if (h_errno == NO_RREC || h_errno == NO_HOST)
			lameserver = TRUE;

		/* authoritative denial: probably misconfiguration */
		if (h_errno == NO_DATA || h_errno == HOST_NOT_FOUND)
			lameserver = TRUE;

		/* flag an error if server should not have failed */
		if (lameserver && authserver)
			errmsg("%s has lame delegation to %s",
				name, server);
	}

	/* restore resolver database */
	_res = save_res;
	server = save_server;
}

/*
** DO_TRANSFER -- Perform a zone transfer from any of its nameservers
** ------------------------------------------------------------------
**
**	Returns:
**		TRUE if the zone data have been retrieved successfully.
**		FALSE if none of the servers responded.
**
**	Inputs:
**		The count of nameservers is stored in ``nservers''.
**		Names are stored in the nsname[] database.
**		Addresses are stored in the ipaddr[] database.
**		Address counts are stored in the naddrs[] database.
**
**	Ask zone transfer to the nameservers, until one responds.
**	The list of nameservers is sorted according to preference.
**	An authoritative server should always respond positively.
**	If it responds with an error, we may have a lame delegation.
**	Always retry with the next server to avoid missing entire zones.
*/

bool
do_transfer(name)
input char *name;			/* name of zone to do zone xfer for */
{
	register int n, ns;
	register int i;

	for (sort_servers(), ns = 0; ns < nservers; ns++)
	{
	    for (n = nsrank[ns], i = 0; i < naddrs[n]; i++)
	    {
		if (verbose)
			printf("Trying server %s (%s) ...\n",
				inet_ntoa(ipaddr[n][i]), nsname[n]);

		if (transfer_zone(name, queryclass, ipaddr[n][i], nsname[n]))
			goto done;	/* double break */

		/* zone transfer failed */
		if ((h_errno != TRY_AGAIN) || verbose)
			ns_error(name, T_AXFR, queryclass, nsname[n]);

		/* zone transfer request was explicitly refused */
		if (h_errno == QUERY_REFUSED)
			break;

		/* explicit server failure: possibly data expired */
		lameserver = (h_errno == SERVER_FAILURE) ? TRUE : FALSE;

		/* non-authoritative denial: assume lame delegation */
		if (h_errno == NO_RREC || h_errno == NO_HOST)
			lameserver = TRUE;

		/* authoritative denial: probably misconfiguration */
		if (h_errno == NO_DATA || h_errno == HOST_NOT_FOUND)
			lameserver = TRUE;

		/* flag an error if server should not have failed */
		if (lameserver && authserver)
			errmsg("%s has lame delegation to %s",
				name, nsname[n]);

		/* try next server if this one is sick */
		if (lameserver)
			break;

		/* terminate on irrecoverable errors */
		if (h_errno != TRY_AGAIN)
			return(FALSE);

		/* in case nameserver not present */
		if (errno == ECONNREFUSED)
			break;
	    }
	}
done:
	if (ns >= nservers)
	{
		if ((h_errno == TRY_AGAIN) && !verbose)
			ns_error(name, T_AXFR, queryclass, (char *)NULL);
		errmsg("No nameservers for %s responded", name);
		return(FALSE);
	}

	return(TRUE);
}

/*
** TRANSFER_ZONE -- Wrapper for get_zone() to hide administrative tasks
** --------------------------------------------------------------------
**
**	Returns:
**		See get_zone() for details.
**
**	Side effects:
**		See get_zone() for details.
**
**	This routine may be called repeatedly with different server
**	addresses, until one of the servers responds. Various items
**	must be reset on every try to continue with a clean slate.
*/

bool
transfer_zone(name, class, inaddr, host)
input char *name;			/* name of zone to do zone xfer for */
input int class;			/* specific resource record class */
input struct in_addr inaddr;		/* address of server to be queried */
input char *host;			/* name of server to be queried */
{
	register int n;

/*
 * Reset the resource record statistics before each try.
 */
	clear_statistics();

/*
 * Reset the hash tables of saved resource record information.
 * These tables are used only during the zone transfer itself.
 */
	clear_ttltab();
	clear_hosttab();
	clear_zonetab();

/*
 * Perform the actual zone transfer.
 */
	if (get_zone(name, class, inaddr, host))
		return(TRUE);

/*
 * Failure to get the zone. Free any memory that may have been allocated.
 * On success it is the responsibility of the caller to free the memory.
 * The information gathered is used by list_zone() after the zone transfer.
 */
	for (n = 0; n < hostcount; n++)
		xfree(hostname[n]);

	for (n = 0; n < zonecount; n++)
		xfree(zonename[n]);

	if (zonename != NULL)
		xfree(zonename);

	zonename = NULL;

	return(FALSE);
}

/*
** GET_ZONE -- Perform a zone transfer from server at specific address
** -------------------------------------------------------------------
**
**	Returns:
**		TRUE if the zone data have been retrieved successfully.
**		FALSE if an error occurred (h_errno is set appropriately).
**		Set TRY_AGAIN wherever possible to try the next server.
**
**	Side effects:
**		Stores list of delegated zones found in zonename[],
**		and the count of delegated zones in ``zonecount''.
**		Stores list of host names found in hostname[],
**		and the count of host names in ``hostcount''.
**		Updates resource record statistics in record_stats[].
**		This array must have been cleared before.
*/

bool
get_zone(name, class, inaddr, host)
input char *name;			/* name of zone to do zone xfer for */
input int class;			/* specific resource record class */
input struct in_addr inaddr;		/* address of server to be queried */
input char *host;			/* name of server to be queried */
{
	querybuf query;
	querybuf answer;
	HEADER *bp;
	int ancount;
	int sock;
	struct sockaddr_in sin;
	register int n;
	register int i;
	int nrecords = 0;		/* number of records processed */
	int soacount = 0;		/* count of SOA records */

	zonecount = 0;			/* count of delegated zones */
	hostcount = 0;			/* count of host names */

/*
 * Construct query, and connect to the given server.
 */
	errno = 0;	/* reset before querying nameserver */

	n = res_mkquery(QUERY, name, class, T_AXFR, (qbuf_t *)NULL, 0,
			(rrec_t *)NULL, (qbuf_t *)&query, sizeof(querybuf));
	if (n < 0)
	{
		if (debug)
			printf("%sres_mkquery failed\n", dbprefix);
		h_errno = NO_RECOVERY;
		return(FALSE);
	}

	if (debug)
	{
		printf("%sget_zone()\n", dbprefix);
		pr_query((qbuf_t *)&query, n, stdout);
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(NAMESERVER_PORT);
	sin.sin_addr = inaddr;

	/* add name and address to error messages */
	/* _res_setaddr(&sin, host); */

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		_res_perror(&sin, host, "socket");
		h_errno = TRY_AGAIN;
		return(FALSE);
	}

	if (_res_connect(sock, &sin, sizeof(sin)) < 0)
	{
		if (debug || verbose)
			_res_perror(&sin, host, "connect");
		(void) close(sock);
		h_errno = TRY_AGAIN;
		return(FALSE);
	}

	if (verbose)
		printf("Asking zone transfer for %s ...\n", name);

/*
 * Send the query buffer.
 */
	if (_res_write(sock, &sin, host, (char *)&query, n) < 0)
	{
		(void) close(sock);
		h_errno = TRY_AGAIN;
		return(FALSE);
	}

/*
 * Process all incoming records, each record in a separate packet.
 */
	while ((n = _res_read(sock, &sin, host, (char *)&answer, sizeof(querybuf))) != 0)
	{
		if (n < 0)
		{
			(void) close(sock);
			h_errno = TRY_AGAIN;
			return(FALSE);
		}

		errno = 0;	/* reset after we got an answer */

		if (n < HFIXEDSZ)
		{
			pr_error("answer length %s too short during %s for %s from %s",
				itoa(n), pr_type(T_AXFR), name, host);
			(void) close(sock);
			h_errno = TRY_AGAIN;
			return(FALSE);
		}

		if (debug > 1)
		{
			printf("%sgot answer, %d bytes:\n", dbprefix, n);
			pr_query((qbuf_t *)&answer, n, stdout);
		}

	/*
	 * Analyze the contents of the answer and check for errors.
	 * An error can be expected only in the very first packet.
	 * The query section should be empty except in the first packet.
	 * Note the special error status codes for specific failures.
	 */
		bp = (HEADER *)&answer;
		ancount = ntohs(bp->ancount);

		if (bp->rcode != NOERROR || ancount == 0)
		{
			if (debug || verbose)
				print_status(&answer);

			switch (bp->rcode)
			{
			    case NXDOMAIN:
				/* distinguish between authoritative or not */
				h_errno = bp->aa ? HOST_NOT_FOUND : NO_HOST;
				break;

			    case NOERROR:
				/* distinguish between authoritative or not */
				h_errno = bp->aa ? NO_DATA : NO_RREC;
				break;

			    case REFUSED:
				/* special status if zone transfer refused */
				h_errno = QUERY_REFUSED;
				break;

			    case SERVFAIL:
				/* special status upon explicit failure */
				h_errno = SERVER_FAILURE;
				break;

			    default:
				/* all other errors will cause a retry */
				h_errno = TRY_AGAIN;
				break;
			}

			if (nrecords != 0)
				pr_error("unexpected error during %s for %s from %s",
					pr_type(T_AXFR), name, host);

			(void) close(sock);
			return(FALSE);
		}

		h_errno = 0;

	/*
	 * The nameserver and additional info section should be empty,
	 * and there should be a single answer in the answer section.
	 */
		if (ancount != 1)
			pr_error("multiple answers during %s for %s from %s",
				pr_type(T_AXFR), name, host);

		i = ntohs(bp->nscount);
		if (i != 0)
			pr_error("nonzero nscount during %s for %s from %s",
				pr_type(T_AXFR), name, host);

		i = ntohs(bp->arcount);
		if (i != 0)
			pr_error("nonzero arcount during %s for %s from %s",
				pr_type(T_AXFR), name, host);

	/*
	 * Valid packet received. Print contents if appropriate.
	 */
		nrecords++;

		soaname = NULL, subname = NULL, adrname = NULL;
		listhost = host;
		(void) print_info(&answer, n, name, T_AXFR, TRUE);

	/*
	 * Terminate upon the second SOA record for this zone.
	 */
		if (soaname && sameword(soaname, name) && soacount++)
			break;

		/* the nameserver balks on this one */
		if (soaname && !sameword(soaname, name))
			pr_warning("extraneous SOA record for %s within %s from %s",
				soaname, name, host);

	/*
	 * Save encountered delegated zone name for recursive listing.
	 */
		if (subname && indomain(subname, name, FALSE))
		{
			i = zone_index(subname, TRUE);
#ifdef obsolete
			for (i = 0; i < zonecount; i++)
				if (sameword(zonename[i], subname))
					break;	/* duplicate */
#endif
			if (i >= zonecount)
			{
				zonename = newlist(zonename, zonecount+1, char *);
				zonename[zonecount] = newstr(subname);
				zonecount++;
			}
		}
		/* warn about strange delegated zones */
		else if (subname && !indomain(subname, name, TRUE))
			pr_warning("extraneous NS record for %s within %s from %s",
				subname, name, host);

	/*
	 * Save encountered name of A record for host name count.
	 */
		if (adrname && indomain(adrname, name, FALSE) && !reverse)
		{
			i = host_index(adrname, (hostcount < MAXHOSTS));
#ifdef obsolete
			for (i = 0; i < hostcount; i++)
				if (sameword(hostname[i], adrname))
					break;	/* duplicate */
#endif
			if (i < hostcount && address != hostaddr[i])
				multaddr[i] = TRUE;

			if (i >= hostcount && hostcount < MAXHOSTS)
			{
				hostname[hostcount] = newstr(adrname);
				hostaddr[hostcount] = address;
				multaddr[hostcount] = FALSE;
				hostcount++;

				if (hostcount == MAXHOSTS)
					pr_error("maximum %s hosts reached within %s from %s",
						itoa(hostcount), name, host);
			}
		}
		/* check for unauthoritative glue records */
		else if (adrname && !indomain(adrname, name, TRUE))
			pr_warning("extraneous glue record for %s within %s from %s",
				adrname, name, host);
	}

/*
 * End of zone transfer at second SOA record or zero length read.
 */
	(void) close(sock);

/*
 * Check for the anomaly that the whole transfer consisted of the
 * SOA records only. Could occur if we queried the victim of a lame
 * delegation which happened to have the SOA record present.
 */
	if (nrecords <= soacount)
	{
		pr_error("empty zone transfer for %s from %s",
			name, host);
		h_errno = NO_RREC;
		return(FALSE);
	}

/*
 * Do an extra check for delegated zones that also have an A record.
 * Those may have been defined in the child zone, and crept in the
 * parent zone, or may have been defined as glue records.
 * This is not necessarily an error, but the host count may be wrong.
 * Note that an A record for the current zone has been ignored above.
 */
	for (n = 0; n < zonecount; n++)
	{
		i = host_index(zonename[n], FALSE);
#ifdef obsolete
		for (i = 0; i < hostcount; i++)
			if (sameword(hostname[i], zonename[n]))
				break;	/* found */
#endif
		if (i < hostcount)
			pr_warning("%s has both NS and A records within %s from %s",
				zonename[n], name, host);
	}

/*
 * The zone transfer has been successful.
 */
	if (verbose)
		printf("Transfer complete, %d records received for %s\n",
			nrecords, name);

	return(TRUE);
}

/*
** GET_MXREC -- Fetch MX records of a domain
** -----------------------------------------
**
**	Returns:
**		TRUE if MX records were found.
**		FALSE otherwise.
*/

bool
get_mxrec(name)
input char *name;			/* domain name to get mx for */
{
	querybuf answer;
	register int n;

	if (verbose)
		printf("Finding MX records for %s ...\n", name);

	n = get_info(&answer, name, T_MX, queryclass);
	if (n < 0)
		return(FALSE);

	(void) print_info(&answer, n, name, T_MX, FALSE);

	return(TRUE);
}

/*
** GET_PRIMARY -- Fetch name of primary nameserver for a zone
** ----------------------------------------------------------
**
**	Returns:
**		Pointer to the name of the primary server, if found.
**		NULL if the server could not be determined.
*/

char *
get_primary(name)
input char *name;			/* name of zone to get soa for */
{
	querybuf answer;
	register int n;

	if (verbose)
		printf("Finding primary nameserver for %s ...\n", name);

	n = get_info(&answer, name, T_SOA, queryclass);
	if (n < 0)
		return(NULL);

	if (verbose > 1)
		(void) print_info(&answer, n, name, T_SOA, FALSE);

	soaname = NULL;
	(void) get_soainfo(&answer, n, name);
	if (soaname == NULL)
		return(NULL);

	return(soa.primary);
}

/*
** CHECK_ZONE -- Fetch and analyze SOA record of a zone
** ----------------------------------------------------
**
**	Returns:
**		TRUE if the SOA record was found at the given server.
**		FALSE otherwise.
**
**	Inputs:
**		The global variable ``server'' must contain the name
**		of the server that was queried.
*/

bool
check_zone(name)
input char *name;			/* name of zone to get soa for */
{
	querybuf answer;
	register int n;

	if (verbose)
		printf("Checking SOA for %s at server %s ...\n", name, server);
	else if (authserver)
		printf("%-20s\tNS\t%s\n", name, server);
	else
		printf("%s\t(%s)\n", name, server);

	n = get_info(&answer, name, T_SOA, queryclass);
	if (n < 0)
		return(FALSE);

	if (verbose > 1)
		(void) print_info(&answer, n, name, T_SOA, FALSE);

	soaname = NULL;
	(void) get_soainfo(&answer, n, name);
	if (soaname == NULL)
		return(FALSE);

	check_soa(&answer, name);

	return(TRUE);
}

/*
** GET_SOAINFO -- Extract SOA data from nameserver answer buffer
** -------------------------------------------------------------
**
**	Returns:
**		TRUE if the SOA record was found successfully.
**		FALSE otherwise.
**
**	Outputs:
**		The global struct ``soa'' is filled with the soa data.
**
**	Side effects:
**		Sets ``soaname'' if this is a valid SOA record.
**		This variable must have been cleared before calling
**		get_soainfo() and may be checked afterwards.
*/

bool
get_soainfo(answerbuf, answerlen, name)
input querybuf *answerbuf;		/* location of answer buffer */
input int answerlen;			/* length of answer buffer */
input char *name;			/* name of zone to get soa for */
{
	HEADER *bp;
	int qdcount, ancount;
	u_char *msg, *eom;
	register u_char *cp;

	bp = (HEADER *)answerbuf;
	qdcount = ntohs(bp->qdcount);
	ancount = ntohs(bp->ancount);

	msg = (u_char *)answerbuf;
	eom = (u_char *)answerbuf + answerlen;
	cp  = (u_char *)answerbuf + HFIXEDSZ;

	while (qdcount > 0 && cp < eom)
	{
		cp = skip_qrec(name, cp, msg, eom);
		if (cp == NULL)
			return(FALSE);
		qdcount--;
	}

	if (qdcount)
	{
		pr_error("invalid qdcount after %s query for %s",
			pr_type(T_SOA), name);
		h_errno = NO_RECOVERY;
		return(FALSE);
	}

/*
 * Check answer section only.
 * The nameserver section may contain the nameservers for the zone,
 * and the additional section their addresses, but not guaranteed.
 * Those sections are usually empty for authoritative answers.
 */
	while (ancount > 0 && cp < eom)
	{
		char rname[MAXDNAME+1];
		int type, class, ttl, dlen;
		u_char *eor;
		register int n;

		n = expand_name(name, T_NONE, cp, msg, eom, rname);
		if (n < 0)
			return(FALSE);
		cp += n;

		n = 3*INT16SZ + INT32SZ;
		if (check_size(rname, T_NONE, cp, msg, eom, n) < 0)
			return(FALSE);

		type = _getshort(cp);
		cp += INT16SZ;

		class = _getshort(cp);
		cp += INT16SZ;

		ttl = _getlong(cp);
		cp += INT32SZ;

		dlen = _getshort(cp);
		cp += INT16SZ;

		eor = cp + dlen;
#ifdef lint
		if (verbose)
			printf("%-20s\t%d\t%s\t%s\n",
				rname, ttl, pr_class(class), pr_type(type));
#endif
		switch (type)
		{
		    case T_SOA:
			n = expand_name(rname, type, cp, msg, eom, soa.primary);
			if (n < 0)
				return(FALSE);
			cp += n;

			n = expand_name(rname, type, cp, msg, eom, soa.hostmaster);
			if (n < 0)
				return(FALSE);
			cp += n;

			n = 5*INT32SZ;
			if (check_size(rname, type, cp, msg, eor, n) < 0)
				return(FALSE);
			soa.serial = _getlong(cp);
			cp += INT32SZ;
			soa.refresh = _getlong(cp);
			cp += INT32SZ;
			soa.retry = _getlong(cp);
			cp += INT32SZ;
			soa.expire = _getlong(cp);
			cp += INT32SZ;
			soa.defttl = _getlong(cp);
			cp += INT32SZ;

			/* valid complete soa record found */
			soaname = strcpy(soanamebuf, rname);
			break;

		    default:
			cp += dlen;
			break;
		}

		if (cp != eor)
		{
			pr_error("size error in %s record for %s, off by %s",
				pr_type(type), rname, itoa(cp - eor));
			return(FALSE);
		}

		ancount--;
	}

	if (ancount)
	{
		pr_error("invalid ancount after %s query for %s",
			pr_type(T_SOA), name);
		h_errno = NO_RECOVERY;
		return(FALSE);
	}

	return(TRUE);
}

/*
** CHECK_SOA -- Analyze retrieved SOA records of a zone
** ----------------------------------------------------
**
**	Returns:
**		None.
**
**	Inputs:
**		The global variable ``server'' must contain the
**		name of the server that was queried.
**		The global struct ``soa'' must contain the soa data.
*/

void
check_soa(answerbuf, name)
input querybuf *answerbuf;		/* location of answer buffer */
input char *name;			/* name of zone to check soa for */
{
	static char oldnamebuf[MAXDNAME+1];
	static char *oldname = NULL;	/* previous name of zone */
	static char *oldserver = NULL;	/* previous name of server */
	static soa_data_t oldsoa;	/* previous soa data */
	register int n;
	HEADER *bp;

/*
 * Print the various SOA fields in abbreviated form.
 * Values are actually unsigned, but we print them as signed integers,
 * apart from the serial which really becomes that big sometimes.
 * In the latter case we print a warning below.
 */
	printf("%s\t%s\t(%u %d %d %d %d)\n",
		soa.primary, soa.hostmaster, (unsigned)soa.serial,
		soa.refresh, soa.retry, soa.expire, soa.defttl);

/*
 * We are supposed to have queried an authoritative nameserver, and since
 * nameserver recursion has been turned off, answer must be authoritative.
 */
	bp = (HEADER *)answerbuf;
	if (!bp->aa)
	{
		if (authserver)
			pr_error("%s SOA record at %s is not authoritative",
				name, server);
		else
			pr_warning("%s SOA record at %s is not authoritative",
				name, server);

		if (authserver)
			errmsg("%s has lame delegation to %s",
				name, server);
	}

/*
 * Check whether we are switching to a new zone.
 * The old name must have been saved in static storage.
 */
	if ((oldname != NULL) && !sameword(name, oldname))
		oldname = NULL;

/*
 * Make few timer consistency checks only for the first one in a series.
 * Compare the primary field against the list of authoritative servers.
 * Explicitly check the hostmaster field for illegal characters ('@').
 * Yell if the serial has the high bit set (not always intentional).
 */
	if (oldname == NULL)
	{
		for (n = 0; n < nservers; n++)
			if (sameword(soa.primary, nsname[n]))
				break;	/* found */

		if ((n >= nservers) && authserver)
			pr_warning("%s SOA primary %s is not advertised via NS",
				name, soa.primary);

		if (!valid_name(soa.primary, FALSE, FALSE, FALSE))
			pr_warning("%s SOA primary %s has illegal name",
				name, soa.primary);

		if (!valid_name(soa.hostmaster, FALSE, TRUE, FALSE))
			pr_warning("%s SOA hostmaster %s has illegal mailbox",
				name, soa.hostmaster);

		if (bitset(0x80000000, soa.serial))
			pr_warning("%s SOA serial has high bit set",
				name);

		if (soa.retry > soa.refresh)
			pr_warning("%s SOA retry exceeds refresh",
				name);

		if (soa.refresh + soa.retry > soa.expire)
			pr_warning("%s SOA refresh+retry exceeds expire",
				name);
	}

/*
 * Compare various fields with those of the previous query, if any.
 * Different serial numbers may be present if secondaries have not yet
 * refreshed the data from the primary. Issue only a warning in that case.
 */
	if (oldname != NULL)
	{
		if (!sameword(soa.primary, oldsoa.primary))
			pr_error("%s and %s have different primary for %s",
				server, oldserver, name);

		if (!sameword(soa.hostmaster, oldsoa.hostmaster))
			pr_error("%s and %s have different hostmaster for %s",
				server, oldserver, name);

		if (soa.serial != oldsoa.serial)
			pr_warning("%s and %s have different serial for %s",
				server, oldserver, name);

		if (soa.refresh != oldsoa.refresh)
			pr_error("%s and %s have different refresh for %s",
				server, oldserver, name);

		if (soa.retry != oldsoa.retry)
			pr_error("%s and %s have different retry for %s",
				server, oldserver, name);

		if (soa.expire != oldsoa.expire)
			pr_error("%s and %s have different expire for %s",
				server, oldserver, name);

		if (soa.defttl != oldsoa.defttl)
			pr_error("%s and %s have different defttl for %s",
				server, oldserver, name);
	}

/*
 * Save the current information.
 */
	oldname = strcpy(oldnamebuf, name);
	oldserver = server;
	oldsoa = soa;
}

/*
** CHECK_DUPL -- Check global address list for duplicates
** ------------------------------------------------------
**
**	Returns:
**		TRUE if the given host address already exists.
**		FALSE otherwise.
**
**	Side effects:
**		Adds the host address to the list if not present.
**
**	The information in this table is global, and is not cleared.
*/

#define AHASHSIZE	0x2000
#define AHASHMASK	0x1fff

typedef struct addr_tab {
	ipaddr_t *addrlist;		/* global list of addresses */
	int addrcount;			/* count of global addresses */
} addr_tab_t;

addr_tab_t addrtab[AHASHSIZE];		/* hash list of global addresses */

bool
check_dupl(addr)
input ipaddr_t addr;			/* address of host to check */
{
 	register int i;
	register addr_tab_t *s;

	s = &addrtab[ntohl(addr) & AHASHMASK];

	for (i = 0; i < s->addrcount; i++)
		if (s->addrlist[i] == addr)
			return(TRUE);	/* duplicate */

	s->addrlist = newlist(s->addrlist, s->addrcount+1, ipaddr_t);
	s->addrlist[s->addrcount] = addr;
	s->addrcount++;
	return(FALSE);
}

/*
** CHECK_TTL -- Check list of records for different ttl values
** -----------------------------------------------------------
**
**	Returns:
**		TRUE if the ttl value matches the first record
**		already listed with the same name/type/class.
**		FALSE only when the first discrepancy is found.
**
**	Side effects:
**		Adds the record data to the list if not present.
*/

#define THASHSIZE	2003

typedef struct ttl_tab {
	struct ttl_tab *next;		/* next entry in chain */
	char *name;			/* name of resource record */
	int type;			/* resource record type */
	int class;			/* resource record class */
	int ttl;			/* time_to_live value */
	int count;			/* count of different ttl values */
} ttl_tab_t;

ttl_tab_t *ttltab[THASHSIZE];		/* hash list of record info */

bool
check_ttl(name, type, class, ttl)
input char *name;			/* resource record name */
input int type, class, ttl;		/* resource record fixed values */
{
	register ttl_tab_t *s;
	register ttl_tab_t **ps;
	register unsigned int hfunc;
	register char *p;
	register char c;

/*
 * Compute the hash function for this resource record.
 * Look it up in the appropriate hash chain.
 */
	for (hfunc = type, p = name; (c = *p) != '\0'; p++)
	{
		hfunc = ((hfunc << 1) ^ (lower(c) & 0377)) % THASHSIZE;
	}

	for (ps = &ttltab[hfunc]; (s = *ps) != NULL; ps = &s->next)
	{
		if (s->type != type || s->class != class)
			continue;
		if (sameword(s->name, name))
			break;
	}

/*
 * Allocate new entry if not found.
 */
	if (s == NULL)
	{
		/* ps = &ttltab[hfunc]; */
		s = newlist(NULL, 1, ttl_tab_t);

		/* initialize new entry */
		s->name = newstr(name);
		s->type = type;
		s->class = class;
		s->ttl = ttl;
		s->count = 0;

		/* link it in */
		s->next = *ps;
		*ps = s;
	}

/*
 * Check whether the ttl value matches the first recorded one.
 * If not, signal only the first discrepancy encountered, so
 * only one warning message will be printed.
 */
	if (s->ttl == ttl)
		return(TRUE);

	s->count += 1;
	return((s->count == 1) ? FALSE : TRUE);
}

/*
** CLEAR_TTLTAB -- Clear resource record list for ttl checking
** -----------------------------------------------------------
**
**	Returns:
**		None.
**
**	An entry on the hash list, and the host name in each
**	entry, have been allocated in dynamic memory.
**
**	The information in this table is on a per-zone basis.
**	It must be cleared before any subsequent zone transfers.
*/

void
clear_ttltab()
{
	register int i;
	register ttl_tab_t *s, *t;

	for (i = 0; i < THASHSIZE; i++)
	{
		if (ttltab[i] != NULL)
		{
			/* free chain of entries */
			for (t = NULL, s = ttltab[i]; s != NULL; s = t)
			{
				t = s->next;
				xfree(s->name);
				xfree(s);
			}

			/* reset hash chain */
			ttltab[i] = NULL;
		}
	}
}

/*
** HOST_INDEX -- Check list of host names for name being present
** -------------------------------------------------------------
**
**	Returns:
**		Index into hostname[] table, if found.
**		Current ``hostcount'' value, if not found.
**
**	Side effects:
**		May add an entry to the hash list if not present.
**
**	A linear search through the master table becomes very
**	costly for zones with more than a few thousand hosts.
**	Maintain a hash list with indexes into the master table.
**	Caller should update the master table after this call.
*/

#define HHASHSIZE	2003

typedef struct host_tab {
	struct host_tab *next;		/* next entry in chain */
	int slot;			/* slot in host name table */
} host_tab_t;

host_tab_t *hosttab[HHASHSIZE];		/* hash list of host name info */

int
host_index(name, enter)
input char *name;			/* the host name to check */
input bool enter;			/* add to table if not found */
{
	register host_tab_t *s;
	register host_tab_t **ps;
	register unsigned int hfunc;
	register char *p;
	register char c;

/*
 * Compute the hash function for this host name.
 * Look it up in the appropriate hash chain.
 */
	for (hfunc = 0, p = name; (c = *p) != '\0'; p++)
	{
		hfunc = ((hfunc << 1) ^ (lower(c) & 0377)) % HHASHSIZE;
	}

	for (ps = &hosttab[hfunc]; (s = *ps) != NULL; ps = &s->next)
	{
		if (s->slot >= hostcount)
			continue;
		if (sameword(hostname[s->slot], name))
			break;
	}

/*
 * Allocate new entry if not found.
 */
	if ((s == NULL) && enter)
	{
		/* ps = &hosttab[hfunc]; */
		s = newlist(NULL, 1, host_tab_t);

		/* initialize new entry */
		s->slot = hostcount;

		/* link it in */
		s->next = *ps;
		*ps = s;
	}

	return((s != NULL) ? s->slot : hostcount);
}

/*
** CLEAR_HOSTTAB -- Clear hash list for host name checking
** -------------------------------------------------------
**
**	Returns:
**		None.
**
**	A hash list entry has been allocated in dynamic memory.
**
**	The information in this table is on a per-zone basis.
**	It must be cleared before any subsequent zone transfers.
*/

void
clear_hosttab()
{
	register int i;
	register host_tab_t *s, *t;

	for (i = 0; i < HHASHSIZE; i++)
	{
		if (hosttab[i] != NULL)
		{
			/* free chain of entries */
			for (t = NULL, s = hosttab[i]; s != NULL; s = t)
			{
				t = s->next;
				xfree(s);
			}

			/* reset hash chain */
			hosttab[i] = NULL;
		}
	}
}

/*
** ZONE_INDEX -- Check list of zone names for name being present
** -------------------------------------------------------------
**
**	Returns:
**		Index into zonename[] table, if found.
**		Current ``zonecount'' value, if not found.
**
**	Side effects:
**		May add an entry to the hash list if not present.
**
**	A linear search through the master table becomes very
**	costly for more than a few thousand delegated zones.
**	Maintain a hash list with indexes into the master table.
**	Caller should update the master table after this call.
*/

#define ZHASHSIZE	2003

typedef struct zone_tab {
	struct zone_tab *next;		/* next entry in chain */
	int slot;			/* slot in zone name table */
} zone_tab_t;

zone_tab_t *zonetab[ZHASHSIZE];		/* hash list of zone name info */

int
zone_index(name, enter)
input char *name;			/* the zone name to check */
input bool enter;			/* add to table if not found */
{
	register zone_tab_t *s;
	register zone_tab_t **ps;
	register unsigned int hfunc;
	register char *p;
	register char c;

/*
 * Compute the hash function for this zone name.
 * Look it up in the appropriate hash chain.
 */
	for (hfunc = 0, p = name; (c = *p) != '\0'; p++)
	{
		hfunc = ((hfunc << 1) ^ (lower(c) & 0377)) % ZHASHSIZE;
	}

	for (ps = &zonetab[hfunc]; (s = *ps) != NULL; ps = &s->next)
	{
		if (s->slot >= zonecount)
			continue;
		if (sameword(zonename[s->slot], name))
			break;
	}

/*
 * Allocate new entry if not found.
 */
	if ((s == NULL) && enter)
	{
		/* ps = &zonetab[hfunc]; */
		s = newlist(NULL, 1, zone_tab_t);

		/* initialize new entry */
		s->slot = zonecount;

		/* link it in */
		s->next = *ps;
		*ps = s;
	}

	return((s != NULL) ? s->slot : zonecount);
}

/*
** CLEAR_ZONETAB -- Clear hash list for zone name checking
** -------------------------------------------------------
**
**	Returns:
**		None.
**
**	A hash list entry has been allocated in dynamic memory.
**
**	The information in this table is on a per-zone basis.
**	It must be cleared before any subsequent zone transfers.
*/

void
clear_zonetab()
{
	register int i;
	register zone_tab_t *s, *t;

	for (i = 0; i < ZHASHSIZE; i++)
	{
		if (zonetab[i] != NULL)
		{
			/* free chain of entries */
			for (t = NULL, s = zonetab[i]; s != NULL; s = t)
			{
				t = s->next;
				xfree(s);
			}

			/* reset hash chain */
			zonetab[i] = NULL;
		}
	}
}

/*
** CHECK_CANON -- Check list of domain names for name being canonical
** ------------------------------------------------------------------
**
**	Returns:
**		Nonzero if the name is definitely not canonical.
**		0 if it is canonical, or if it remains undecided.
**
**	Side effects:
**		Adds the domain name to the list if not present.
**
**	The information in this table is global, and is not cleared
**	(which may be necessary if the checking algorithm changes).
*/

#define CHASHSIZE	2003

typedef struct canon_tab {
	struct canon_tab *next;		/* next entry in chain */
	char *name;			/* domain name */
	int status;			/* nonzero if not canonical */
} canon_tab_t;

canon_tab_t *canontab[CHASHSIZE];	/* hash list of domain name info */

int
check_canon(name)
input char *name;			/* the domain name to check */
{
	register canon_tab_t *s;
	register canon_tab_t **ps;
	register unsigned int hfunc;
	register char *p;
	register char c;

/*
 * Compute the hash function for this domain name.
 * Look it up in the appropriate hash chain.
 */
	for (hfunc = 0, p = name; (c = *p) != '\0'; p++)
	{
		hfunc = ((hfunc << 1) ^ (lower(c) & 0377)) % CHASHSIZE;
	}

	for (ps = &canontab[hfunc]; (s = *ps) != NULL; ps = &s->next)
	{
		if (sameword(s->name, name))
			break;
	}

/*
 * Allocate new entry if not found.
 * Only then is the actual check carried out.
 */
	if (s == NULL)
	{
		/* ps = &canontab[hfunc]; */
		s = newlist(NULL, 1, canon_tab_t);

		/* initialize new entry */
		s->name = newstr(name);
		s->status = canonical(name);

		/* link it in */
		s->next = *ps;
		*ps = s;
	}

	return(s->status);
}

/*
** CHECK_ADDR -- Check whether reverse address mappings revert to host
** -------------------------------------------------------------------
**
**	Returns:
**		TRUE if all addresses of host map back to host.
**		FALSE otherwise.
*/

bool
check_addr(name)
input char *name;			/* host name to check addresses for */
{
	struct hostent *hp;
	register int i;
	struct in_addr inaddr[MAXADDRS];
	int naddr;
	char hnamebuf[MAXDNAME+1];
	char *hname;
	char inamebuf[MAXDNAME+1];
	char *iname;
	int matched;

/*
 * Look up the specified host to fetch its addresses.
 */
	hp = gethostbyname(name);
	if (hp == NULL)
	{
		ns_error(name, T_A, C_IN, server);
		return(FALSE);
	}

	hname = strcpy(hnamebuf, hp->h_name);

	for (i = 0; i < MAXADDRS && hp->h_addr_list[i]; i++)
		inaddr[i] = incopy(hp->h_addr_list[i]);
	naddr = i;

	if (verbose)
		printf("Found %d address%s for %s\n",
			naddr, plurale(naddr), hname);

/*
 * Map back the addresses found, and check whether they revert to host.
 */
	for (matched = 0, i = 0; i < naddr; i++)
	{
		iname = strcpy(inamebuf, inet_ntoa(inaddr[i]));

		if (verbose)
			printf("Checking %s address %s\n", hname, iname);

		hp = gethostbyaddr((char *)&inaddr[i], INADDRSZ, AF_INET);
		if (hp == NULL)
			ns_error(iname, T_PTR, C_IN, server);
		else if (!sameword(hp->h_name, hname))
			pr_warning("%s address %s maps to %s",
				hname, iname, hp->h_name);
		else
			matched++;
	}

	return((matched == naddr) ? TRUE : FALSE);
}

/*
** CHECK_NAME -- Check whether address belongs to host addresses
** -------------------------------------------------------------
**
**	Returns:
**		TRUE if given address was found among host addresses.
**		FALSE otherwise.
*/

bool
check_name(addr)
input ipaddr_t addr;			/* address of host to check */
{
	struct hostent *hp;
	register int i;
	struct in_addr inaddr;
	char hnamebuf[MAXDNAME+1];
	char *hname;
	char inamebuf[MAXDNAME+1];
	char *iname;
	int matched;

/*
 * Check whether the address is registered by fetching its host name.
 */
	inaddr.s_addr = addr;
	iname = strcpy(inamebuf, inet_ntoa(inaddr));

	hp = gethostbyaddr((char *)&inaddr, INADDRSZ, AF_INET);
	if (hp == NULL)
	{
		ns_error(iname, T_PTR, C_IN, server);
		return(FALSE);
	}

	hname = strcpy(hnamebuf, hp->h_name);

	if (verbose)
		printf("Address %s maps to %s\n", iname, hname);

/*
 * Lookup the host name found to fetch its addresses.
 * Verify whether the mapped host name is canonical.
 */
	hp = gethostbyname(hname);
	if (hp == NULL)
	{
		ns_error(hname, T_A, C_IN, server);
		return(FALSE);
	}

	if (!sameword(hp->h_name, hname))
		pr_warning("%s host %s is not canonical (%s)",
			iname, hname, hp->h_name);

/*
 * Check whether the given address is listed among the known addresses.
 */
	for (matched = 0, i = 0; hp->h_addr_list[i]; i++)
	{
		inaddr = incopy(hp->h_addr_list[i]);

		if (verbose)
			printf("Checking %s address %s\n",
				hname, inet_ntoa(inaddr));

		if (inaddr.s_addr == addr)
			matched++;
	}

	if (!matched)
		pr_error("address %s does not belong to %s",
			iname, hname);

	return(matched ? TRUE : FALSE);
}

/*
** PARSE_TYPE -- Decode rr type from input string
** ----------------------------------------------
**
**	Returns:
**		Value of resource record type.
**		-1 if specified record name is invalid.
**
**	Note.	T_MD, T_MF, T_MAILA are obsolete, but recognized.
**		T_AXFR is not allowed to be specified as query type.
*/

int
parse_type(str)
input char *str;			/* input string with record type */
{
	register int type;

	if (sameword(str, "A"))		return(T_A);
	if (sameword(str, "NS"))	return(T_NS);
	if (sameword(str, "MD"))	return(T_MD);		/* obsolete */
	if (sameword(str, "MF"))	return(T_MF);		/* obsolete */
	if (sameword(str, "CNAME"))	return(T_CNAME);
	if (sameword(str, "SOA"))	return(T_SOA);
	if (sameword(str, "MB"))	return(T_MB);
	if (sameword(str, "MG"))	return(T_MG);
	if (sameword(str, "MR"))	return(T_MR);
	if (sameword(str, "NULL"))	return(T_NULL);
	if (sameword(str, "WKS"))	return(T_WKS);
	if (sameword(str, "PTR"))	return(T_PTR);
	if (sameword(str, "HINFO"))	return(T_HINFO);
	if (sameword(str, "MINFO"))	return(T_MINFO);
	if (sameword(str, "MX"))	return(T_MX);
	if (sameword(str, "TXT"))	return(T_TXT);

	if (sameword(str, "RP"))	return(T_RP);
	if (sameword(str, "AFSDB"))	return(T_AFSDB);
	if (sameword(str, "X25"))	return(T_X25);
	if (sameword(str, "ISDN"))	return(T_ISDN);
	if (sameword(str, "RT"))	return(T_RT);
	if (sameword(str, "NSAP"))	return(T_NSAP);
	if (sameword(str, "NSAP-PTR"))	return(T_NSAPPTR);
	if (sameword(str, "SIG"))	return(T_SIG);
	if (sameword(str, "KEY"))	return(T_KEY);
	if (sameword(str, "PX"))	return(T_PX);
	if (sameword(str, "GPOS"))	return(T_GPOS);
	if (sameword(str, "AAAA"))	return(T_AAAA);
	if (sameword(str, "LOC"))	return(T_LOC);

	if (sameword(str, "UINFO"))	return(T_UINFO);
	if (sameword(str, "UID"))	return(T_UID);
	if (sameword(str, "GID"))	return(T_GID);
	if (sameword(str, "UNSPEC"))	return(T_UNSPEC);

	if (sameword(str, "AXFR"))	return(-1);		/* illegal */
	if (sameword(str, "MAILB"))	return(T_MAILB);
	if (sameword(str, "MAILA"))	return(T_MAILA);	/* obsolete */
	if (sameword(str, "ANY"))	return(T_ANY);
	if (sameword(str, "*"))		return(T_ANY);

	type = atoi(str);
	if (type >= T_FIRST && type <= T_LAST)
		return(type);

	return(-1);
}

/*
** PARSE_CLASS -- Decode rr class from input string
** ------------------------------------------------
**
**	Returns:
**		Value of resource class.
**		-1 if specified class name is invalid.
**
**	Note.	C_CSNET is obsolete, but recognized.
*/

int
parse_class(str)
input char *str;			/* input string with resource class */
{
	register int class;

	if (sameword(str, "IN"))	return(C_IN);
	if (sameword(str, "INTERNET"))	return(C_IN);
	if (sameword(str, "CS"))	return(C_CSNET);	/* obsolete */
	if (sameword(str, "CSNET"))	return(C_CSNET);	/* obsolete */
	if (sameword(str, "CH"))	return(C_CHAOS);
	if (sameword(str, "CHAOS"))	return(C_CHAOS);
	if (sameword(str, "HS"))	return(C_HS);
	if (sameword(str, "HESIOD"))	return(C_HS);

	if (sameword(str, "ANY"))	return(C_ANY);
	if (sameword(str, "*"))		return(C_ANY);

	class = atoi(str);
	if (class > 0)
		return(class);

	return(-1);
}

/*
** IN_ADDR_ARPA -- Convert dotted quad string to reverse in-addr.arpa
** ------------------------------------------------------------------
**
**	Returns:
**		Pointer to appropriate reverse in-addr.arpa name
**		with trailing dot to force absolute domain name.
**		NULL in case of invalid dotted quad input string.
*/

char *
in_addr_arpa(dottedquad)
input char *dottedquad;			/* input string with dotted quad */
{
	static char addrbuf[4*4 + sizeof(ARPA_ROOT) + 2];
	unsigned int a[4];
	register int n;

	n = sscanf(dottedquad, "%u.%u.%u.%u", &a[0], &a[1], &a[2], &a[3]);
	switch (n)
	{
	    case 4:
		(void) sprintf(addrbuf, "%u.%u.%u.%u.%s.",
			a[3]&0xff, a[2]&0xff, a[1]&0xff, a[0]&0xff, ARPA_ROOT);
		break;

	    case 3:
		(void) sprintf(addrbuf, "%u.%u.%u.%s.",
			a[2]&0xff, a[1]&0xff, a[0]&0xff, ARPA_ROOT);
		break;

	    case 2:
		(void) sprintf(addrbuf, "%u.%u.%s.",
			a[1]&0xff, a[0]&0xff, ARPA_ROOT);
		break;

	    case 1:
		(void) sprintf(addrbuf, "%u.%s.",
			a[0]&0xff, ARPA_ROOT);
		break;

	    default:
		return(NULL);
	}

	while (--n >= 0)
		if (a[n] > 255)
			return(NULL);

	return(addrbuf);
}

/*
** NSAP_INT -- Convert dotted nsap address string to reverse nsap.int
** ------------------------------------------------------------------
**
**	Returns:
**		Pointer to appropriate reverse nsap.int name
**		with trailing dot to force absolute domain name.
**		NULL in case of invalid nsap address input string.
*/

char *
nsap_int(name)
input char *name;			/* input string with dotted nsap */
{
	static char addrbuf[4*MAXNSAP + sizeof(NSAP_ROOT) + 2];
	register int n;
	register int i;

	/* skip optional leading hex indicator */
	if (samehead(name, "0x"))
		name += 2;

	for (n = 0, i = strlength(name)-1; i >= 0; --i)
	{
		/* skip optional interspersed separators */
		if (name[i] == '.' || name[i] == '+' || name[i] == '/')
			continue;

		/* must consist of hex digits only */
		if (!is_xdigit(name[i]))
			return(NULL);

		/* but not too many */
		if (n >= 4*MAXNSAP)
			return(NULL);

		addrbuf[n++] = name[i];
		addrbuf[n++] = '.';
	}

	/* must have an even number of hex digits */ 
	if (n == 0 || (n % 4) != 0)
		return(NULL);

	(void) sprintf(&addrbuf[n], "%s.", NSAP_ROOT);
	return(addrbuf);
}

/*
** PRINT_HOST -- Print host name and address of hostent struct
** -----------------------------------------------------------
**
**	Returns:
**		None.
*/

void
print_host(heading, hp)
input char *heading;			/* header string */
input struct hostent *hp;		/* location of hostent struct */
{
	register char **ap;

	printf("%s: %s", heading, hp->h_name);

	for (ap = hp->h_addr_list; ap && *ap; ap++)
	{
		if (ap == hp->h_addr_list)
			printf("\nAddress:");

		printf(" %s", inet_ntoa(incopy(*ap)));
	}

	for (ap = hp->h_aliases; ap && *ap && **ap; ap++)
	{
		if (ap == hp->h_aliases)
			printf("\nAliases:");

		printf(" %s", *ap);
	}

	printf("\n\n");
}

/*
** SHOW_RES -- Show resolver database information
** ----------------------------------------------
**
**	Returns:
**		None.
**
**	Inputs:
**		The resolver database _res is localized in the resolver.
*/

void
show_res()
{
	register int i;
	register char **domain;

/*
 * The default domain is defined by the "domain" entry in /etc/resolv.conf
 * if not overridden by the environment variable "LOCALDOMAIN".
 * If still not defined, gethostname() may yield a fully qualified host name.
 */
	printf("Default domain:");
	if (_res.defdname[0] != '\0')
		printf(" %s", _res.defdname);
	printf("\n");

/*
 * The search domains are extracted from the default domain components,
 * but may be overridden by "search" directives in /etc/resolv.conf
 * since 4.8.3.
 */
	printf("Search domains:");
	for (domain = _res.dnsrch; *domain; domain++)
		printf(" %s", *domain);
	printf("\n");

/*
 * The routine res_send() will do _res.retry tries to contact each of the
 * _res.nscount nameserver addresses before giving up when using datagrams.
 * The first try will timeout after _res.retrans seconds. Each following
 * try will timeout after ((_res.retrans << try) / _res.nscount) seconds.
 * Note. When we contact an explicit server the addresses will be replaced
 * by the multiple addresses of the same server.
 * When doing a zone transfer _res.retrans is used for the connect timeout.
 */
	printf("Timeout per retry: %d secs\n", _res.retrans);
	printf("Number of retries: %d\n", _res.retry);

	printf("Number of addresses: %d\n", _res.nscount);
	for (i = 0; i < _res.nscount; i++)
		printf("%s\n", inet_ntoa(nslist(i).sin_addr));

/*
 * The resolver options are initialized by res_init() to contain the
 * defaults settings (RES_RECURSE | RES_DEFNAMES | RES_DNSRCH)
 * The various options have the following meaning:
 *
 *	RES_INIT	set after res_init() has been called
 *	RES_DEBUG	let the resolver modules print debugging info
 *	RES_AAONLY	want authoritative answers only (not implemented)
 *	RES_USEVC	use tcp virtual circuit instead of udp datagrams
 *	RES_PRIMARY	use primary nameserver only (not implemented)
 *	RES_IGNTC	ignore datagram truncation; don't switch to tcp
 *	RES_RECURSE	forward query if answer not locally available
 *	RES_DEFNAMES	add default domain to queryname without dot
 *	RES_STAYOPEN	keep tcp socket open for subsequent queries
 *	RES_DNSRCH	append search domains even to queryname with dot
 */
	printf("Options set:");
	if (bitset(RES_INIT,      _res.options)) printf(" INIT");
	if (bitset(RES_DEBUG,     _res.options)) printf(" DEBUG");
	if (bitset(RES_AAONLY,    _res.options)) printf(" AAONLY");
	if (bitset(RES_USEVC,     _res.options)) printf(" USEVC");
	if (bitset(RES_PRIMARY,   _res.options)) printf(" PRIMARY");
	if (bitset(RES_IGNTC,     _res.options)) printf(" IGNTC");
	if (bitset(RES_RECURSE,   _res.options)) printf(" RECURSE");
	if (bitset(RES_DEFNAMES,  _res.options)) printf(" DEFNAMES");
	if (bitset(RES_STAYOPEN,  _res.options)) printf(" STAYOPEN");
	if (bitset(RES_DNSRCH,    _res.options)) printf(" DNSRCH");
	printf("\n");

	printf("Options clr:");
	if (!bitset(RES_INIT,     _res.options)) printf(" INIT");
	if (!bitset(RES_DEBUG,    _res.options)) printf(" DEBUG");
	if (!bitset(RES_AAONLY,   _res.options)) printf(" AAONLY");
	if (!bitset(RES_USEVC,    _res.options)) printf(" USEVC");
	if (!bitset(RES_PRIMARY,  _res.options)) printf(" PRIMARY");
	if (!bitset(RES_IGNTC,    _res.options)) printf(" IGNTC");
	if (!bitset(RES_RECURSE,  _res.options)) printf(" RECURSE");
	if (!bitset(RES_DEFNAMES, _res.options)) printf(" DEFNAMES");
	if (!bitset(RES_STAYOPEN, _res.options)) printf(" STAYOPEN");
	if (!bitset(RES_DNSRCH,   _res.options)) printf(" DNSRCH");
	printf("\n");

/*
 * The new BIND 4.9.3 has additional features which are not (yet) used.
 */
	printf("\n");
}

/*
** PRINT_STATISTICS -- Print resource record statistics
** ----------------------------------------------------
**
**	Returns:
**		None.
**
**	Inputs:
**		The record_stats[] counts have been updated by print_rrec().
*/

void
print_statistics(name, filter, class)
input char *name;			/* name of zone we are listing */
input int filter;			/* type of records we want to see */
input int class;			/* class of records we want to see */
{
	register int type;
	int nrecords;

	for (type = T_FIRST; type <= T_LAST; type++)
	{
		nrecords = record_stats[type];
		if (nrecords > 0 || ((filter != T_ANY) && want_type(type, filter)))
		{
			printf("Found %4d %-5s record%-1s", nrecords,
				pr_type(type), plural(nrecords));

			if (class != C_IN)
				printf(" in class %s", pr_class(class));

			printf(" within %s\n", name);
		}
	}
}


/*
** CLEAR_STATISTICS -- Clear resource record statistics
** ----------------------------------------------------
**
**	Returns:
**		None.
*/

void
clear_statistics()
{
	bzero((char *)record_stats, sizeof(record_stats));
}

/*
** SHOW_TYPES -- Show resource record types wanted
** -----------------------------------------------
**
**	Returns:
**		None.
*/

void
show_types(name, filter, class)
input char *name;			/* name we want to query about */
input int filter;			/* type of records we want to see */
input int class;			/* class of records we want to see */
{
	register int type;

	if (filter >= T_NONE)
	{
		printf("Query about %s for record types", name);

		if (filter == T_ANY)
			printf(" %s", pr_type(T_ANY));
		else
			for (type = T_FIRST; type <= T_LAST; type++)
				if (want_type(type, filter))
					printf(" %s", pr_type(type));

		if (class != C_IN)
			printf(" in class %s", pr_class(class));

		printf("\n");
	}
}

/*
** NS_ERROR -- Print error message from errno and h_errno
** ------------------------------------------------------
**
**	Returns:
**		None.
**
** If BIND res_send() fails, it will leave errno in either of the first
** two following states when using datagrams. Note that this depends on
** the proper handling of connected datagram sockets, which is usually
** true if BSD >= 43 (see res_send.c for details; it may need a patch).
** Note. If the 4.8 version succeeds, it may leave errno as EAFNOSUPPORT
** if it has disconnected a previously connected datagram socket, since
** the dummy address used to disconnect does not have a proper family set.
** Always clear errno after getting a reply, or patch res_send().
** Our private version of res_send() will leave also other error statuses.
*/

void
ns_error(name, type, class, host) 
input char *name;			/* full name we queried about */
input int type;				/* record type we queried about */
input int class;			/* record class we queried about */
input char *host;			/* set if explicit server was used */
{
	static char *auth = "Authoritative answer";

/*
 * Print the message associated with the network related errno values.
 */
	switch (errno)
	{
	    case ECONNREFUSED:
		/*
		 * The contacted host does not have a nameserver running.
		 * The standard res_send() also returns this if none of
		 * the intended hosts could be reached via datagrams.
		 */
		if (host != NULL)
			errmsg("Nameserver %s not running", host);
		else
			errmsg("Nameserver not running");
		break;

	    case ETIMEDOUT:
		/*
		 * The contacted server did not give any reply at all
		 * within the specified time frame.
		 */
		if (host != NULL)
			errmsg("Nameserver %s not responding", host);
		else
			errmsg("Nameserver not responding");
		break;

	    case ENETDOWN:
	    case ENETUNREACH:
	    case EHOSTDOWN:
	    case EHOSTUNREACH:
		/*
		 * The host to be contacted or its network can not be reached.
		 * Our private res_send() also returns this using datagrams.
		 */
		if (host != NULL)
			errmsg("Nameserver %s not reachable", host);
		else
			errmsg("Nameserver not reachable");
		break;
	}

/*
 * Print the message associated with the particular nameserver error.
 */
	switch (h_errno)
	{
	    case HOST_NOT_FOUND:
		/*
		 * The specified name does definitely not exist at all.
		 * In this case the answer is always authoritative.
		 * Nameserver status: NXDOMAIN
		 */
		if (class != C_IN)
			errmsg("%s does not exist in class %s (%s)",
				name, pr_class(class), auth);
		else if (host != NULL)
			errmsg("%s does not exist at %s (%s)",
				name, host, auth);
		else
			errmsg("%s does not exist (%s)",
				name, auth);
		break;

	    case NO_HOST:
		/*
		 * The specified name does not exist, but the answer
		 * was not authoritative, so it is still undecided.
		 * Nameserver status: NXDOMAIN
		 */
		if (class != C_IN)
			errmsg("%s does not exist in class %s, try again",
				name, pr_class(class));
		else if (host != NULL)
			errmsg("%s does not exist at %s, try again",
				name, host);
		else
			errmsg("%s does not exist, try again",
				name);
		break;

	    case NO_DATA:
		/*
		 * The name is valid, but the specified type does not exist.
		 * This status is here returned only in case authoritative.
		 * Nameserver status: NOERROR
		 */
		if (class != C_IN)
			errmsg("%s has no %s record in class %s (%s)",
				name, pr_type(type), pr_class(class), auth);
		else if (host != NULL)
			errmsg("%s has no %s record at %s (%s)",
				name, pr_type(type), host, auth);
		else
			errmsg("%s has no %s record (%s)",
				name, pr_type(type), auth);
		break;

	    case NO_RREC:
		/*
		 * The specified type does not exist, but we don't know whether
		 * the name is valid or not. The answer was not authoritative.
		 * Perhaps recursion was off, and no data was cached locally.
		 * Nameserver status: NOERROR
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s currently not present",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record currently not present at %s",
				name, pr_type(type), host);
		else
			errmsg("%s %s record currently not present",
				name, pr_type(type));
		break;

	    case TRY_AGAIN:
		/*
		 * Some intermediate failure, e.g. connect timeout,
		 * or some local operating system transient errors.
		 * General failure to reach any appropriate servers.
		 * The status SERVFAIL now yields a separate error code.
		 * Nameserver status: (SERVFAIL)
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s not found, try again",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record not found at %s, try again",
				name, pr_type(type), host);
		else
			errmsg("%s %s record not found, try again",
				name, pr_type(type));
		break;

	    case SERVER_FAILURE:
		/*
		 * Explicit server failure status. This will be returned upon
		 * some internal server errors, forwarding failures, or when
		 * the server is not authoritative for a specific class.
		 * Also if the zone data has expired at a secondary server.
		 * Nameserver status: SERVFAIL
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s not found, server failure",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record not found at %s, server failure",
				name, pr_type(type), host);
		else
			errmsg("%s %s record not found, server failure",
				name, pr_type(type));
		break;

	    case NO_RECOVERY:
		/*
		 * Some irrecoverable format error, or server refusal.
		 * The status REFUSED now yields a separate error code.
		 * Nameserver status: (REFUSED) FORMERR NOTIMP NOCHANGE
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s not found, no recovery",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record not found at %s, no recovery",
				name, pr_type(type), host);
		else
			errmsg("%s %s record not found, no recovery",
				name, pr_type(type));
		break;

	    case QUERY_REFUSED:
		/*
		 * The server explicitly refused to answer the query.
		 * Servers can be configured to disallow zone transfers.
		 * Nameserver status: REFUSED
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s query refused",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record query refused by %s",
				name, pr_type(type), host);
		else
			errmsg("%s %s record query refused",
				name, pr_type(type));
		break;

	    default:
		/*
		 * Unknown cause for server failure.
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s not found",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record not found at %s",
				name, pr_type(type), host);
		else
			errmsg("%s %s record not found",
				name, pr_type(type));
		break;
	}
}

/*
** DECODE_ERROR -- Convert nameserver error code to error message
** --------------------------------------------------------------
**
**	Returns:
**		Pointer to appropriate error message.
*/

char *
decode_error(rcode)
input int rcode;			/* error code from bp->rcode */
{
	switch (rcode)
	{
	    case NOERROR: 	return("no error");
	    case FORMERR:	return("format error");
	    case SERVFAIL:	return("server failure");
	    case NXDOMAIN:	return("non-existent domain");
	    case NOTIMP:	return("not implemented");
	    case REFUSED:	return("query refused");
	    case NOCHANGE:	return("no change");
	}

	return("unknown error");
}

/*
** PRINT_STATUS -- Print result status after nameserver query
** ----------------------------------------------------------
**
**	Returns:
**		None.
**
**	Conditions:
**		The size of the answer buffer must have been
**		checked before to be of sufficient length,
**		i.e. to contain at least the buffer header.
*/

void
print_status(answerbuf)
input querybuf *answerbuf;		/* location of answer buffer */
{
	HEADER *bp;
	int ancount;
	bool failed;

	bp = (HEADER *)answerbuf;
	ancount = ntohs(bp->ancount);
	failed = (bp->rcode != NOERROR || ancount == 0);

	printf("%sQuery %s, %d answer%s%s, %sstatus: %s\n",
		verbose ? "" : dbprefix,
		failed ? "failed" : "done",
		ancount, plural(ancount),
		bp->tc ? " (truncated)" : "",
		bp->aa ? "authoritative " : "",
		decode_error((int)bp->rcode));
}

/*
** PR_ERROR -- Print error message about encountered inconsistencies
** -----------------------------------------------------------------
**
**	We are supposed to have an error condition which is fatal
**	for normal continuation, and the message is always printed.
**
**	Returns:
**		None.
**
**	Side effects:
**		Increments the global error count.
*/

void /*VARARGS1*/
pr_error(fmt, a, b, c, d)
input char *fmt;			/* format of message */
input char *a, *b, *c, *d;		/* optional arguments */
{
	(void) fprintf(stderr, " *** ");
	(void) fprintf(stderr, fmt, a, b, c, d);
	(void) fprintf(stderr, "\n");

	/* flag an error */
	errorcount++;
}


/*
** PR_WARNING -- Print warning message about encountered inconsistencies
** ---------------------------------------------------------------------
**
**	We are supposed to have an error condition which is non-fatal
**	for normal continuation, and the message is suppressed in case
**	quiet mode has been selected.
**
**	Returns:
**		None.
*/

void /*VARARGS1*/
pr_warning(fmt, a, b, c, d)
input char *fmt;			/* format of message */
input char *a, *b, *c, *d;		/* optional arguments */
{
	if (!quiet)
	{
		(void) fprintf(stderr, " !!! ");
		(void) fprintf(stderr, fmt, a, b, c, d);
		(void) fprintf(stderr, "\n");
	}
}

/*
** WANT_TYPE -- Indicate whether the rr type matches the desired filter
** --------------------------------------------------------------------
**
**	Returns:
**		TRUE if the resource record type matches the filter.
**		FALSE otherwise.
**
**	In regular mode, the querytype is used to formulate the query,
**	and the filter is set to T_ANY to filter out any response.
**	In listmode, we get everything, so the filter is set to the
**	querytype to filter out the proper responses.
**	Note that T_NONE is the default querytype in listmode.
*/

bool
want_type(type, filter)
input int type;				/* resource record type */
input int filter;			/* type of records we want to see */
{
	if (type == filter)
		return(TRUE);

	if (filter == T_ANY)
		return(TRUE);

	if (filter == T_NONE &&
	   (type == T_A || type == T_NS || type == T_PTR))
		return(TRUE);

	if (filter == T_MAILB &&
	   (type == T_MB || type == T_MR || type == T_MG || type == T_MINFO))
		return(TRUE);

	if (filter == T_MAILA &&
	   (type == T_MD || type == T_MF))
		return(TRUE);

	return(FALSE);
}

/*
** WANT_CLASS -- Indicate whether the rr class matches the desired filter
** ----------------------------------------------------------------------
**
**	Returns:
**		TRUE if the resource record class matches the filter.
**		FALSE otherwise.
**
**	In regular mode, the queryclass is used to formulate the query,
**	and the filter is set to C_ANY to filter out any response.
**	In listmode, we get everything, so the filter is set to the
**	queryclass to filter out the proper responses.
**	Note that C_IN is the default queryclass in listmode.
*/

bool
want_class(class, filter)
input int class;			/* resource record class */
input int filter;			/* class of records we want to see */
{
	if (class == filter)
		return(TRUE);

	if (filter == C_ANY)
		return(TRUE);

	return(FALSE);
}

/*
** INDOMAIN -- Check whether a name belongs to a zone
** --------------------------------------------------
**
**	Returns:
**		TRUE if the given name lies anywhere in the zone, or
**		if the given name is the same as the zone and may be so.
**		FALSE otherwise.
*/

bool
indomain(name, domain, equal)
input char *name;			/* the name under consideration */
input char *domain;			/* the name of the zone */
input bool equal;			/* set if name may be same as zone */
{
	register char *dot;

	if (sameword(name, domain))
		return(equal);

	if (sameword(domain, "."))
		return(TRUE);

	dot = index(name, '.');
	while (dot != NULL)
	{
		if (sameword(dot+1, domain))
			return(TRUE);

		dot = index(dot+1, '.');
	}

	return(FALSE);
}

/*
** SAMEDOMAIN -- Check whether a name belongs to a zone
** ----------------------------------------------------
**
**	Returns:
**		TRUE if the given name lies directly in the zone, or
**		if the given name is the same as the zone and may be so.
**		FALSE otherwise.
*/

bool
samedomain(name, domain, equal)
input char *name;			/* the name under consideration */
input char *domain;			/* the name of the zone */
input bool equal;			/* set if name may be same as zone */
{
	register char *dot;

	if (sameword(name, domain))
		return(equal);

	dot = index(name, '.');
	if (dot == NULL)
		return(sameword(domain, "."));

	if (sameword(dot+1, domain))
		return(TRUE);

	return(FALSE);
}

/*
** GLUERECORD -- Check whether a name is a glue record
** ---------------------------------------------------
**
**	Returns:
**		TRUE is this is a glue record.
**		FALSE otherwise.
**
**	The name is supposed to be the name of an address record.
**	If it lies directly in the given zone, it is considered
**	an ordinary host within that zone, and not a glue record.
**	If it does not belong to the given zone at all, is it
**	here considered to be a glue record.
**	If it lies in the given zone, but not directly, it is
**	considered a glue record if it belongs to any of the known
**	delegated zones of the given zone.
**	In the root zone itself are no hosts, only glue records.
*/

bool
gluerecord(name, domain, zone, nzones)
input char *name;			/* the name under consideration */
input char *domain;			/* name of zone being processed */
input char *zone[];			/* list of known delegated zones */
input int nzones;			/* number of known delegated zones */
{
	register int n;

	if (sameword(domain, "."))
		return(TRUE);

	if (samedomain(name, domain, TRUE))
		return(FALSE);

	if (!indomain(name, domain, TRUE))
		return(TRUE);

	for (n = 0; n < nzones; n++)
		if (indomain(name, zone[n], TRUE))
			return(TRUE);

	return(FALSE);
}

/*
** MATCHLABELS -- Determine number of matching domain name labels
** --------------------------------------------------------------
**
**	Returns:
**		Number of shared trailing components in both names.
*/

int
matchlabels(name, domain)
input char *name;			/* domain name to check */
input char *domain;			/* domain name to compare against */
{
	register int i, j;
	int matched = 0;

	i = strlength(name);
	j = strlength(domain);

	while (--i >= 0 && --j >= 0)
	{
		if (lower(name[i]) != lower(domain[j]))
			break;
		if (domain[j] == '.')
			matched++;
		else if (j == 0 && (i == 0 || name[i-1] == '.'))
			matched++;
	}

	return(matched);
}

/*
** PR_DOMAIN -- Convert domain name according to printing options
** --------------------------------------------------------------
**
**	Returns:
**		Pointer to new domain name, if conversion was done.
**		Pointer to original name, if no conversion necessary.
*/

char *
pr_domain(name, listing)
input char *name;			/* domain name to be printed */
input bool listing;			/* set if this is a zone listing */
{
	char *newname;			/* converted domain name */

/*
 * Print reverse nsap.int name in forward notation, unless prohibited.
 */
	if (revnsap && !dotprint)
	{
		newname = pr_nsap(name);
		if (newname != name)
			return(newname);
	}

/*
 * Print domain names with trailing dot if necessary.
 */
	if (listing || dotprint)
	{
		newname = pr_dotname(name);
		if (newname != name)
			return(newname);
	}

/*
 * No conversion was required, use original name.
 */
	return(name);
}

/*
** PR_DOTNAME -- Return domain name with trailing dot
** --------------------------------------------------
**
**	Returns:
**		Pointer to new domain name, if dot was added.
**		Pointer to original name, if dot was already present.
*/

char *
pr_dotname(name)
input char *name;			/* domain name to append to */
{
	static char buf[MAXDNAME+2];	/* buffer to store new domain name */
	register int n;

	n = strlength(name);
	if (n > 0 && name[n-1] == '.')
		return(name);

	if (n > MAXDNAME)
		n = MAXDNAME;

#ifdef obsolete
	(void) sprintf(buf, "%.*s.", MAXDNAME, name);
#endif
	bcopy(name, buf, n);
	buf[n] = '.';
	buf[n+1] = '\0';
	return(buf);
}

/*
** PR_NSAP -- Convert reverse nsap.int to dotted forward notation
** --------------------------------------------------------------
**
**	Returns:
**		Pointer to new dotted nsap, if converted.
**		Pointer to original name otherwise.
*/

char *
pr_nsap(name)
input char *name;			/* potential reverse nsap.int name */
{
	static char buf[3*MAXNSAP+1];
	register char *p;
	register int n;
	register int i;

	/* must begin with single hex digits separated by dots */
	for (i = 0; is_xdigit(name[i]) && name[i+1] == '.'; i += 2)
		continue;

	/* must have an even number of hex digits */ 
	if (i == 0 || (i % 4) != 0)
		return(name);

	/* but not too many */
	if (i > 4*MAXNSAP)
		return(name);

	/* must end in the appropriate root domain */
	if (!sameword(&name[i], NSAP_ROOT))
		return(name);

	for (p = buf, n = 0; i >= 4; i -= 4, n++)
	{
		*p++ = name[i-2];
		*p++ = name[i-4];

		/* add dots for readability */
		if ((n % 2) == 0 && (i - 4) > 0)
			*p++ = '.';
	}
	*p = '\0';

	return(buf);
}

/*
** PR_TYPE -- Return name of resource record type
** ----------------------------------------------
**
**	Returns:
**		Pointer to name of resource record type.
**
**	Note.	All possible (even obsolete) types are recognized.
*/

char *
pr_type(type)
input int type;				/* resource record type */
{
	static char buf[30];

	switch (type)
	{
	    case T_A:       return("A");	/* internet address */
	    case T_NS:      return("NS");	/* authoritative server */
	    case T_MD:      return("MD");	/* mail destination */
	    case T_MF:      return("MF");	/* mail forwarder */
	    case T_CNAME:   return("CNAME");	/* canonical name */
	    case T_SOA:     return("SOA");	/* start of auth zone */
	    case T_MB:      return("MB");	/* mailbox domain name */
	    case T_MG:      return("MG");	/* mail group member */
	    case T_MR:      return("MR");	/* mail rename name */
	    case T_NULL:    return("NULL");	/* null resource record */
	    case T_WKS:     return("WKS");	/* well known service */
	    case T_PTR:     return("PTR");	/* domain name pointer */
	    case T_HINFO:   return("HINFO");	/* host information */
	    case T_MINFO:   return("MINFO");	/* mailbox information */
	    case T_MX:      return("MX");	/* mail routing info */
	    case T_TXT:     return("TXT");	/* descriptive text */

	    case T_RP:      return("RP");	/* responsible person */
	    case T_AFSDB:   return("AFSDB");	/* afs database location */
	    case T_X25:     return("X25");	/* x25 address */
	    case T_ISDN:    return("ISDN");	/* isdn address */
	    case T_RT:      return("RT");	/* route through host */
	    case T_NSAP:    return("NSAP");	/* nsap address */
	    case T_NSAPPTR: return("NSAP-PTR");	/* nsap pointer */
	    case T_SIG:     return("SIG");	/* security signature */
	    case T_KEY:     return("KEY");	/* security key */
	    case T_PX:      return("PX");	/* rfc822 - x400 mapping */
	    case T_GPOS:    return("GPOS");	/* geographical position */
	    case T_AAAA:    return("AAAA");	/* ip v6 address */
	    case T_LOC:     return("LOC");	/* geographical location */

	    case T_UINFO:   return("UINFO");	/* user information */
	    case T_UID:     return("UID");	/* user ident */
	    case T_GID:     return("GID");	/* group ident */
	    case T_UNSPEC:  return("UNSPEC");	/* unspecified binary data */

	    case T_AXFR:    return("AXFR");	/* zone transfer */
	    case T_MAILB:   return("MAILB");	/* matches MB/MR/MG/MINFO */
	    case T_MAILA:   return("MAILA");	/* matches MD/MF */
	    case T_ANY:     return("ANY");	/* matches any type */

	    case T_NONE:    return("resource");	/* not yet determined */
	}

	(void) sprintf(buf, "%d", type);
	return(buf);
}

/*
** PR_CLASS -- Return name of resource record class
** ------------------------------------------------
**
**	Returns:
**		Pointer to name of resource record class.
*/

char *
pr_class(class)
input int class;			/* resource record class */
{
	static char buf[30];

	switch (class)
	{
	    case C_IN:      return("IN");	/* internet */
	    case C_CSNET:   return("CS");	/* csnet */
	    case C_CHAOS:   return("CH");	/* chaosnet */
	    case C_HS:      return("HS");	/* hesiod */
	    case C_ANY:     return("ANY");	/* any class */
	}

	(void) sprintf(buf, "%d", class);
	return(buf);
}

/*
** EXPAND_NAME -- Expand compressed domain name in a recource record
** -----------------------------------------------------------------
**
**	Returns:
**		Number of bytes advanced in answer buffer.
**		-1 if there was a format error.
**
**	It is assumed that the specified buffer is of sufficient size.
*/

int
expand_name(name, type, cp, msg, eom, namebuf)
input char *name;			/* name of resource record */
input int type;				/* type of resource record */
input u_char *cp;			/* current position in answer buf */
input u_char *msg, *eom;		/* begin and end of answer buf */
output char *namebuf;			/* location of buf to expand name in */
{
	register int n;

	n = dn_expand(msg, eom, cp, (nbuf_t *)namebuf, MAXDNAME);
	if (n < 0)
	{
		pr_error("expand error in %s record for %s, offset %s",
			pr_type(type), name, itoa(cp - msg));
		h_errno = NO_RECOVERY;
		return(-1);
	}

	/* change root to single dot */
	if (namebuf[0] == '\0')
	{
		namebuf[0] = '.';
		namebuf[1] = '\0';
	}

	return(n);
}

/*
** CHECK_SIZE -- Check whether resource record is of sufficient length
** -------------------------------------------------------------------
**
**	Returns:
**		Requested size if current record is long enough.
**		-1 if current record does not have this many bytes.
**
**	Note that HINFO records are very often incomplete since only
**	one of the two data fields has been filled in and the second
**	field is missing. So we generate only a warning message.
*/

int
check_size(name, type, cp, msg, eor, size)
input char *name;			/* name of resource record */
input int type;				/* type of resource record */
input u_char *cp;			/* current position in answer buf */
input u_char *msg;			/* begin of answer buf */
input u_char *eor;			/* predicted position of next record */
input int size;				/* required record size remaining */
{
	if (cp + size > eor)
	{
		if (type != T_HINFO)
			pr_error("incomplete %s record for %s, offset %s",
				pr_type(type), name, itoa(cp - msg));
		else
			pr_warning("incomplete %s record for %s",
				pr_type(type), name);
		h_errno = NO_RECOVERY;
		return(-1);
	}

	return(size);
}

/*
** VALID_NAME -- Check whether domain name contains invalid characters
** -------------------------------------------------------------------
**
**	Returns:
**		TRUE if the name is valid.
**		FALSE otherwise.
**
**	The total size of a compound name should not exceed MAXDNAME.
**	We assume that this is true. Its individual components between
**	dots should not be longer than 64. This is not checked here.
**
**	Only alphanumeric characters and dash '-' may be used (dash
**	only in the middle). We only check the individual characters.
**	Strictly speaking, this restriction is only for ``host names''.
**	The underscore is illegal, at least not recommended, but is
**	so abundant that is requires special processing.
**
**	If the domain name represents a mailbox specification, the
**	first label up to the first (unquoted) dot is the local part
**	of a mail address, which should adhere to the RFC 822 specs.
**	This first dot takes the place of the RFC 822 '@' sign.
**
**	The label '*' can in principle be used anywhere to indicate
**	wildcarding. It is valid only in the LHS resource record name,
**	in definitions in zone files only as the first component.
**	Used primarily in wildcard MX record definitions.
*/

char *specials = ".()<>@,;:\\\"[]";	/* RFC 822 specials */

bool
valid_name(name, wildcard, localpart, underscore)
input char *name;			/* domain name to check */
input bool wildcard;			/* set if wildcard is allowed */
input bool localpart;			/* set if this is a mailbox spec */
input bool underscore;			/* set if underscores are allowed */
{
	bool backslash = FALSE;
	bool quoting = FALSE;
	register char *p;
	register char c;

	for (p = name; (c = *p) != '\0'; p++)
	{
		/* special check for local part in mailbox */
		if (localpart)
		{
			if (backslash)
				backslash = FALSE;	/* escape this char */
			else if (c == '\\')
				backslash = TRUE;	/* escape next char */
			else if (c == '"')
				quoting = !quoting;	/* start/stop quoting */
			else if (quoting)
				continue;		/* allow quoted chars */
			else if (c == '.')
				localpart = FALSE;	/* instead of '@' */
			else if (c == '@')
				return(FALSE);		/* should be '.' */
			else if (in_string(specials, c))
				return(FALSE);		/* must be escaped */
			else if (is_space(c))
				return(FALSE);		/* must be escaped */
			continue;
		}

		/* basic character set */
		if (is_alnum(c) || (c == '-'))
			continue;

		/* start of a new component */
		if (c == '.')
			continue;

		/* allow '*' for use in wildcard names */
		if ((c == '*') && wildcard)
			continue;

		/* ignore underscore in certain circumstances */
		if ((c == '_') && underscore && !illegal)
			continue;

		/* silently allowed widespread exceptions */
		if (illegal && in_string(illegal, c))
			continue;

		return(FALSE);
	}

	/* must be beyond the local part in a mailbox */
	if (localpart)
		return(FALSE);

	return(TRUE);
}

/* 
** CANONICAL -- Check whether domain name is a canonical host name
** ---------------------------------------------------------------
**
**	Returns:
**		Nonzero if the name is definitely not canonical.
**		0 if it is canonical, or if it remains undecided.
*/

int
canonical(name)
input char *name;			/* the domain name to check */
{
	struct hostent *hp;
	int status;
	int save_errno;
	int save_herrno;
	
/*
 * Preserve state when querying, to avoid clobbering current values.
 */
	save_errno = errno;
	save_herrno = h_errno;

	hp = gethostbyname(name);
	status = h_errno;

	errno = save_errno;
	h_errno = save_herrno;

/*
 * Indicate negative result only after definitive lookup failures.
 */
	if (hp == NULL)
	{
		/* authoritative denial -- not existing or no A record */
		if (status == NO_DATA || status == HOST_NOT_FOUND)
			return(status);

		/* nameserver failure -- still undecided, assume ok */
		return(0);
	}

/*
 * The given name exists and there is an associated A record.
 * The name of this A record should be the name we queried about.
 * If this is not the case we probably supplied a CNAME.
 */
	status = sameword(hp->h_name, name) ? 0 : HOST_NOT_CANON;
	return(status);
}

/* 
** MAPREVERSE -- Check whether address maps back to given domain
** -------------------------------------------------------------
**
**	Returns:
**		NULL if address could definitively not be mapped.
**		Given name if the address maps back properly, or
**		in case of transient nameserver failures.
**		Reverse name if it differs from the given name.
*/

char *
mapreverse(name, inaddr)
input char *name;			/* domain name of A record */
input struct in_addr inaddr;		/* address of A record to check */
{
	struct hostent *hp;
	int status;
	int save_errno;
	int save_herrno;
	
/*
 * Preserve state when querying, to avoid clobbering current values.
 */
	save_errno = errno;
	save_herrno = h_errno;

	hp = gethostbyaddr((char *)&inaddr, INADDRSZ, AF_INET);
	status = h_errno;

	errno = save_errno;
	h_errno = save_herrno;

/*
 * Indicate negative result only after definitive lookup failures.
 */
	if (hp == NULL)
	{
		/* authoritative denial -- not existing or no PTR record */
		if (status == NO_DATA || status == HOST_NOT_FOUND)
			return(NULL);

		/* nameserver failure -- still undecided, assume ok */
		return(name);
	}

/*
 * Indicate whether the reverse mapping yields the given name.
 */
	return(sameword(hp->h_name, name) ? name : hp->h_name);
}

/* 
** COMPARE_NAME -- Compare two names wrt alphabetical order
** --------------------------------------------------------
**
**	Returns:
**		Value of case-insensitive comparison.
*/

int
compare_name(a, b)
input char **a;				/* first name */
input char **b;				/* second name */
{
	return(strcasecmp(*a, *b));
}

/*
** XALLOC -- Allocate or reallocate additional memory
** --------------------------------------------------
**
**	Returns:
**		Pointer to (re)allocated buffer space.
**		Aborts if the requested memory could not be obtained.
*/

ptr_t *
xalloc(buf, size)
register ptr_t *buf;			/* current start of buffer space */
input siz_t size;			/* number of bytes to allocate */
{
	if (buf == NULL)
		buf = malloc(size);
	else
		buf = realloc(buf, size);

	if (buf == NULL)
	{
		errmsg("Out of memory");
		exit(EX_OSERR);
	}

	return(buf);
}

/*
** ITOA -- Convert integer value to ascii string
** ---------------------------------------------
**
**	Returns:
**		Pointer to string.
*/

char *
itoa(n)
input int n;				/* value to convert */
{
	static char buf[30];

	(void) sprintf(buf, "%d", n);
	return(buf);
}


/*
** UTOA -- Convert unsigned integer value to ascii string
** ------------------------------------------------------
**
**	Returns:
**		Pointer to string.
*/

char *
utoa(n)
input int n;				/* value to convert */
{
	static char buf[30];

	(void) sprintf(buf, "%u", (unsigned)n);
	return(buf);
}

/*
** STOA -- Extract partial ascii string
** ------------------------------------
**
**	Returns:
**		Pointer to string.
*/

char *
stoa(cp, size)
input u_char *cp;			/* current position in answer buf */
input int size;				/* number of bytes to extract */
{
	static char buf[MAXDLEN+1];

	if (size > MAXDLEN)
		size = MAXDLEN;

#ifdef obsolete
	if (size > 0)
		(void) sprintf(buf, "%.*s", size, (char *)cp);
	else
		(void) sprintf(buf, "%s", "");
#endif
	bcopy((char *)cp, buf, size);
	buf[size] = '\0';
	return(buf);
}

/*
** NSAP_NTOA -- Convert binary nsap address to ascii
** -------------------------------------------------
**
**	Returns:
**		Pointer to string.
**
**	As per RFC 1637 an nsap address is encoded in binary form
**	in the resource record. It was unclear from RFC 1348 how
**	the encoding should be. RFC 1629 defines an upper bound
**	of 20 bytes to the size of a binary nsap address.
*/

char *
nsap_ntoa(cp, size)
input u_char *cp;			/* current position in answer buf */
input int size;				/* number of bytes to extract */
{
	static char buf[3*MAXNSAP+1];
	register char *p;
	register int n;
	register int i;

	if (size > MAXNSAP)
		size = MAXNSAP;

	for (p = buf, i = 0; i < size; i++, cp++)
	{
		n = ((int)(*cp) >> 4) & 0x0f;
		*p++ = hexdigit(n);
		n = ((int)(*cp) >> 0) & 0x0f;
		*p++ = hexdigit(n);

		/* add dots for readability */
		if ((i % 2) == 0 && (i + 1) < size)
			*p++ = '.';
	}
	*p = '\0';

	return(buf);
}

/*
** PR_TIME -- Produce printable version of a time interval
** -------------------------------------------------------
**
**	Returns:
**		Pointer to a string version of interval.
**
**	The value is a time interval expressed in seconds.
*/

char *
pr_time(value, brief)
input int value;			/* the interval to be converted */
input bool brief;			/* use brief format if set */
{
	static char buf[256];
	register char *p = buf;
	int week, days, hour, mins, secs;

	/* special cases */
	if (value < 0)
		return("negative");
	if ((value == 0) && !brief)
		return("zero seconds");

/*
 * Decode the components.
 */
	secs = value % 60; value /= 60;
	mins = value % 60; value /= 60;
	hour = value % 24; value /= 24;
	days = value;

	if (!brief)
	{
		days = value % 7; value /= 7;
		week = value;
	}

/*
 * Now turn it into a sexy form.
 */
	if (brief)
	{
		if (days > 0)
		{
			(void) sprintf(p, "%d+", days);
			p += strlength(p);
		}

		(void) sprintf(p, "%02d:%02d:%02d", hour, mins, secs);
		return(buf);
	}

	if (week > 0)
	{
		(void) sprintf(p, ", %d week%s", week, plural(week));
		p += strlength(p);
	}

	if (days > 0)
	{
		(void) sprintf(p, ", %d day%s", days, plural(days));
		p += strlength(p);
	}

	if (hour > 0)
	{
		(void) sprintf(p, ", %d hour%s", hour, plural(hour));
		p += strlength(p);
	}

	if (mins > 0)
	{
		(void) sprintf(p, ", %d minute%s", mins, plural(mins));
		p += strlength(p);
	}

	if (secs > 0)
	{
		(void) sprintf(p, ", %d second%s", secs, plural(secs));
		/* p += strlength(p); */
	}

	return(buf + 2);
}

/*
** PR_SPHERICAL -- Produce printable version of a spherical location
** -----------------------------------------------------------------
**
**	Returns:
**		Pointer to a string version of location.
**
**	The value is a spherical location (latitude or longitude)
**	expressed in thousandths of a second of arc.
**	The value 2^31 represents zero (equator or prime meridian).
*/

char *
pr_spherical(value, pos, neg)
input int value;			/* the location to be converted */
input char *pos;			/* suffix if value positive */
input char *neg;			/* suffix if value negative */
{
	static char buf[256];
	register char *p = buf;
	char *direction;
	int degrees, minutes, seconds, fracsec;

/*
 * Normalize.
 */
	value -= (1 << 31);

	direction = pos;
	if (value < 0)
	{
		direction = neg;
		value = -value;
	}

/*
 * Decode the components.
 */
	fracsec = value % 1000; value /= 1000;
	seconds = value % 60;   value /= 60;
	minutes = value % 60;   value /= 60;
	degrees = value;

/*
 * Construct output string.
 */
	(void) sprintf(p, "%d", degrees);
	p += strlength(p);

	if (minutes > 0 || seconds > 0 || fracsec > 0)
	{
		(void) sprintf(p, " %02d", minutes);
		p += strlength(p);
	}

	if (seconds > 0 || fracsec > 0)
	{
		(void) sprintf(p, " %02d", seconds);
		p += strlength(p);
	}

	if (fracsec > 0)
	{
		(void) sprintf(p, ".%03d", fracsec);
		p += strlength(p);
	}

	(void) sprintf(p, " %s", direction);

#ifdef obsolete
	(void) sprintf(buf, "%d %02d %02d.%03d %s",
		degrees, minutes, seconds, fracsec, direction);
#endif
	return(buf);
}

/*
** PR_VERTICAL -- Produce printable version of a vertical location
** ---------------------------------------------------------------
**
**	Returns:
**		Pointer to a string version of location.
**
**	The value is an altitude expressed in centimeters, starting
**	from a base 100000 meters below the GPS reference spheroid.
**	This allows for the actual range [-10000000 .. 4293967296].
*/

char *
pr_vertical(value, pos, neg)
input int value;			/* the location to be converted */
input char *pos;			/* prefix if value positive */
input char *neg;			/* prefix if value negative */
{
	static char buf[256];
	register char *p = buf;
	char *direction;
	int meters, centimeters;
	unsigned int altitude;
	unsigned int reference;

/*
 * Normalize.
 */
	altitude = value;
	reference = 100000*100;

	if (altitude < reference)
	{
		direction = neg;
		altitude = reference - altitude;
	}
	else
	{
		direction = pos;
		altitude = altitude - reference;
	}

/*
 * Decode the components.
 */
	centimeters = altitude % 100; altitude /= 100;
	meters = altitude;

/*
 * Construct output string.
 */
	(void) sprintf(p, "%s%d", direction, meters);
	p += strlength(p);

	if (centimeters > 0)
		(void) sprintf(p, ".%02d", centimeters);

#ifdef obsolete
	(void) sprintf(buf, "%s%d.%02d", direction, meters, centimeters);
#endif
	return(buf);
}

/*
** PR_PRECISION -- Produce printable version of a location precision
** -----------------------------------------------------------------
**
**	Returns:
**		Pointer to a string version of precision.
**
**	The value is a precision expressed in centimeters, encoded
**	as 4-bit mantissa and 4-bit power of 10 (each ranging 0-9).
*/

unsigned int poweroften[10] =
{1,10,100,1000,10000,100000,1000000,10000000,100000000,1000000000};

char *
pr_precision(value)
input int value;			/* the precision to be converted */
{
	static char buf[256];
	register char *p = buf;
	int meters, centimeters;
	unsigned int precision;
	register int mantissa;
	register int exponent;

/*
 * Normalize.
 */
	mantissa = ((value >> 4) & 0x0f) % 10;
	exponent = ((value >> 0) & 0x0f) % 10;
	precision = mantissa * poweroften[exponent];

/*
 * Decode the components.
 */
	centimeters = precision % 100; precision /= 100;
	meters = precision;

/*
 * Construct output string.
 */
	(void) sprintf(p, "%d", meters);
	p += strlength(p);

	if (centimeters > 0)
		(void) sprintf(p, ".%02d", centimeters);

#ifdef obsolete
	(void) sprintf(buf, "%d.%02d", meters, centimeters);
#endif
	return(buf);
}
