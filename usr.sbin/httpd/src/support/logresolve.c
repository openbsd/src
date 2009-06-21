/*	$OpenBSD: logresolve.c,v 1.15 2009/06/21 00:38:22 martynas Exp $	*/

/*
 * logresolve 1.1
 *
 * Tom Rathborne - tomr@aceldama.com - http://www.aceldama.com/~tomr/
 * UUNET Canada, April 16, 1995
 *
 * Rewritten by David Robinson. (drtr@ast.cam.ac.uk)
 *
 * usage: logresolve [-c] [-s filename] < access_log > access_log.new
 *
 * Arguments:
 *    -s filename     name of a file to record statistics
 *    -c              check the DNS for a matching A record for the host.
 *
 * Notes:
 *
 * To generate meaningful statistics from an HTTPD log file, it's good
 * to have the domain name of each machine that accessed your site, but
 * doing this on the fly can slow HTTPD down.
 *
 * Compiling NCSA HTTPD with the -DMINIMAL_DNS flag turns IP#->hostname
 * resolution off. Before running your stats program, just run your log
 * file through this program (logresolve) and all of your IP numbers will
 * be resolved into hostnames (where possible).
 *
 * logresolve takes an HTTPD access log (in the COMMON log file format,
 * or any other format that has the IP number/domain name as the first
 * field for that matter), and outputs the same file with all of the
 * domain names looked up. Where no domain name can be found, the IP
 * number is left in.
 *
 * To minimize impact on your nameserver, logresolve has its very own
 * internal hash-table cache. This means that each IP number will only
 * be looked up the first time it is found in the log file.
 *
 * The -c option causes logresolve to apply the same check as httpd
 * compiled with -DMAXIMUM_DNS; after finding the hostname from the IP
 * address, it looks up the IP addresses for the hostname and checks
 * that one of these matches the original address.
 */

#include "ap_config.h"
#include <sys/types.h>

#include <ctype.h>

#include <arpa/inet.h>

static void cgethost(struct sockaddr *sa, char *string, int check);
static int getline(char *s, int n);
static void stats(FILE *output);
static void usage(void);


/* maximum line length */
#define MAXLINE 1024

/* maximum length of a domain name */
#ifndef MAXDNAME
#define MAXDNAME 256
#endif

/* number of buckets in cache hash table */
#define BUCKETS 256

/*
 * struct nsrec - record of nameservice for cache linked list
 * 
 * ipnum - IP number hostname - hostname noname - nonzero if IP number has no
 * hostname, i.e. hostname=IP number
 */
struct nsrec {
	struct sockaddr_storage	 addr;
	char			*hostname;
	int			 noname;
	struct nsrec		*next;
} *nscache[BUCKETS];

/* statistics - obvious */

#if !defined(h_errno)
extern int h_errno; /* some machines don't have this in their headers */
#endif

/* largest value for h_errno */
#define MAX_ERR (NO_ADDRESS)
#define UNKNOWN_ERR (MAX_ERR+1)
#define NO_REVERSE  (MAX_ERR+2)

static int cachehits = 0;
static int cachesize = 0;
static int entries = 0;
static int resolves = 0;
static int withname = 0;
static int errors[MAX_ERR + 3];

/*
 * cgethost - gets hostname by IP address, caching, and adding unresolvable
 * IP numbers with their IP number as hostname, setting noname flag
 */
static void
cgethost(struct sockaddr *sa, char *string, int check)
{
	uint32_t hashval;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct nsrec **current, *new;
	char *name;
	char hostnamebuf[MAXHOSTNAMELEN];

	switch (sa->sa_family) {
	case AF_INET:
		hashval = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
		break;
	case AF_INET6:
		hashval = *(uint32_t *)&(
		    (struct sockaddr_in6 *)sa)->sin6_addr.s6_addr[12];
		break;
	default:
		hashval = 0;
		break;
	}

	current = &nscache[((hashval + (hashval >> 8) +
	    (hashval >> 16) + (hashval >> 24)) % BUCKETS)];

	while (*current) {
		if (sa->sa_len == (*current)->addr.ss_len
		    && memcmp(sa, &(*current)->addr, sa->sa_len) == 0)
			break;

		current = &(*current)->next;
	}

	if (*current == NULL) {
		cachesize++;
		new = (struct nsrec *)malloc(sizeof(struct nsrec));
		if (new == NULL) {
			perror("malloc");
			fprintf(stderr, "Insufficient memory\n");
			exit(1);
		}
		*current = new;
		new->next = NULL;

		memcpy(&new->addr, sa, sa->sa_len);

		new->noname = getnameinfo(sa, sa->sa_len, hostnamebuf,
		    sizeof(hostnamebuf), NULL, 0, 0);
		name = strdup(hostnamebuf);
		if (check) {
			struct addrinfo hints, *res;
			int error;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			error = getaddrinfo(hostnamebuf, NULL, &hints, &res);
			if (!error) {
				while (res) {
					if (sa->sa_len == res->ai_addrlen
					    && memcmp(sa, res->ai_addr,
					    sa->sa_len) == 0)
						break;

					res = res->ai_next;
				}
				if (!res)
					error++;
			}
			if (error) {
				getnameinfo(sa,	sa->sa_len, hostnamebuf,
				    sizeof(hostnamebuf), NULL, 0,
				    NI_NUMERICHOST);
				fprintf(stderr, "Bad host: %s != %s\n", name,
				    hostnamebuf);
				new->noname = NO_REVERSE;
				free(name);
				name = strdup(hostnamebuf);
			}
		}
		new->hostname = name;
		if (new->hostname == NULL) {
			perror("strdup");
			fprintf(stderr, "Insufficient memory\n");
			exit(1);
		}
	}
	else
		cachehits++;

	/* size of string == MAXDNAME +1 */
	strncpy(string, (*current)->hostname, MAXDNAME);
	string[MAXDNAME] = '\0';
}

/* prints various statistics to output */
static void
stats(FILE *output)
{
	int i;
	char *ipstring;
	struct nsrec *current;
	char *errstring[MAX_ERR + 3];
	char hostnamebuf[MAXHOSTNAMELEN];

	for (i = 0; i < MAX_ERR + 3; i++)
		errstring[i] = "Unknown error";
	errstring[HOST_NOT_FOUND] = "Host not found";
	errstring[TRY_AGAIN] = "Try again";
	errstring[NO_RECOVERY] = "Non recoverable error";
	errstring[NO_DATA] = "No data record";
	errstring[NO_ADDRESS] = "No address";
	errstring[NO_REVERSE] = "No reverse entry";

	fprintf(output, "logresolve Statistics:\n");

	fprintf(output, "Entries: %d\n", entries);
	fprintf(output, "    With name   : %d\n", withname);
	fprintf(output, "    Resolves    : %d\n", resolves);
	if (errors[HOST_NOT_FOUND])
		fprintf(output, "    - Not found : %d\n",
		    errors[HOST_NOT_FOUND]);
	if (errors[TRY_AGAIN])
		fprintf(output, "    - Try again : %d\n", errors[TRY_AGAIN]);
	if (errors[NO_DATA])
		fprintf(output, "    - No data   : %d\n", errors[NO_DATA]);
	if (errors[NO_ADDRESS])
		fprintf(output, "    - No address: %d\n", errors[NO_ADDRESS]);
	if (errors[NO_REVERSE])
		fprintf(output, "    - No reverse: %d\n", errors[NO_REVERSE]);
	fprintf(output, "Cache hits      : %d\n", cachehits);
	fprintf(output, "Cache size      : %d\n", cachesize);
	fprintf(output, "Cache buckets   :     IP number * hostname\n");

	for (i = 0; i < BUCKETS; i++)
		for (current = nscache[i]; current != NULL;
		    current = current->next) {
			getnameinfo((struct sockaddr *)&current->addr,
			    current->addr.ss_len, hostnamebuf,
			    sizeof(hostnamebuf), NULL, 0, NI_NUMERICHOST);
			ipstring = hostnamebuf;
			if (current->noname == 0)
				fprintf(output, "  %3d  %15s - %s\n", i,
				    ipstring, current->hostname);
			else {
				if (current->noname > MAX_ERR + 2)
					fprintf(output, "  %3d  %15s : Unknown "
					    "error\n", i, ipstring);
				else
					fprintf(output, "  %3d  %15s : %s\n",
					    i, ipstring,
				errstring[current->noname]);
			}
		}
}


/*gets a line from stdin */
static int
getline(char *s, int n)
{
	if (!fgets(s, n, stdin))
		return (0);
	s[strcspn(s, "\n")] = '\0';
	return (1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: logresolve [-c] [-s filename] < access_log "
	    "> access_log.new\n");
	exit(1);
}

int main
(int argc, char *argv[])
{
	char *bar, hoststring[MAXDNAME + 1], line[MAXLINE], *statfile;
	int i, check;
	struct addrinfo hints, *res;
	int error;
	int ch;

	check = 0;
	statfile = NULL;
	while ((ch = getopt(argc, argv, "s:c")) != -1) {
		switch (ch) {
		case 'c':
			check = 1;
			break;
		case 's':
			statfile = optarg;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

	for (i = 0; i < BUCKETS; i++)
		nscache[i] = NULL;
	for (i = 0; i < MAX_ERR + 2; i++)
		errors[i] = 0;

	while (getline(line, MAXLINE)) {
		if (line[0] == '\0')
			continue;
		entries++;
		if (!isdigit((int)line[0])) {	/* short cut */
			puts(line);
			withname++;
			continue;
		}
		bar = strchr(line, ' ');
		if (bar != NULL)
			*bar = '\0';
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
		error = getaddrinfo(line, NULL, &hints, &res);
		if (error) {
			if (bar != NULL)
			*bar = ' ';
			puts(line);
			withname++;
			continue;
		}

		resolves++;

		cgethost(res->ai_addr, hoststring, check);
		if (bar != NULL)
			printf("%s %s\n", hoststring, bar + 1);
		else
			puts(hoststring);
		freeaddrinfo(res);
	}

	if (statfile != NULL) {
		FILE *fp;
		fp = fopen(statfile, "w");
		if (fp == NULL) {
			fprintf(stderr, "logresolve: could not open statistics "
			    "file '%s'\n", statfile);
			exit(1);
		}
		stats(fp);
		fclose(fp);
	}

	return (0);
}
