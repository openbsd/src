/*	$OpenBSD: ifstated.c,v 1.1 2004/01/23 21:34:30 mcbride Exp $	*/

/*
 * Copyright (c) 2004 Marco Pfatschbacher <mpf@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * ifstated listens to link_state transitions on interfaces
 * and executes predefined commands.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <err.h>
#include <util.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>


#define CMD_LENGTH 100
#define CONF_LINES 50

struct conf_t {
	u_short dev;			/* if_index  */
	u_char state;			/* if_link_state */
	char cmd[CMD_LENGTH];
} conf_table[CONF_LINES];

char	*state_name[] = { "UNKNOWN", "DOWN", "UP" };
int	opt_debug = 0;
int	opt_inhibit = 0; 		/* don't run scripts on startup */
char 	*configfile = "/etc/ifstated.conf";

#define MAX_IFINDEX 64
int 	prev_states[MAX_IFINDEX]; 	/* -1 to trigger init */

volatile sig_atomic_t got_sighup;

void	loop(void);
void	eval_rtmsg(struct rt_msghdr *, int);
void	scan_table(int, int);
void	fetch_state(void);
void	usage(void);
void	doconfig(const char*);
void	sighup_handler(int);
#define LOG(s,a) if (opt_debug) \
	printf("ifstated: " s , a ); \
	else \
	syslog(LOG_DAEMON, s , a);


void
usage(void)
{
	fprintf(stderr, "usage: ifstated [-hdi] [-f config]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int ch;
	struct sigaction sact;

	while ((ch = getopt(argc, argv, "hdif:")) != -1) {
		switch (ch) {
		case 'h':
			usage();
			break;
		case 'd':
			opt_debug = 1;
			break;
		case 'i':
			opt_inhibit = 1;
			break;
		case 'f':
			configfile = optarg;
			break;
		default:
			usage();
		}
	}

	doconfig(configfile);

	bzero((char *)&sact, sizeof sact);
	sigemptyset(&sact.sa_mask);
	sact.sa_flags = 0;
	/* sact.sa_flags |= SA_RESTART; */
	sact.sa_handler = sighup_handler;
	(void) sigaction(SIGHUP, &sact, NULL);

	if (!opt_debug) {
		daemon(0, 0);
		setproctitle(NULL);
	}

	LOG("%s\n", "started");

	loop();

	LOG("%s\n", "dropped out of main loop. exiting");
	return (0);
}

void
doconfig(const char *conf)
{
	FILE *fconfig = NULL;
	char *line = NULL;
	size_t len, lineno = 0;
	int device = -1;
	int state = -1;
	int nrofstates = sizeof(state_name) / sizeof(char *);
	int cf_line = 0;

	fconfig = fopen(conf, "r");

	if (fconfig == NULL)
		errx(1, "could not open config: %s\n", conf);

	bzero(conf_table, sizeof(conf_table));
	bzero(prev_states, sizeof(prev_states));

	while ((line = fparseln(fconfig, &len, &lineno, 
	    NULL, FPARSELN_UNESCALL)) != NULL) {

		char statename[20];
		char devname[IF_NAMESIZE];
		int i;
		char *cp;
		
#define WS      " \t\n"
		cp = line;

		cp += strspn(cp, WS);
		if (cp[0] == '\0') {
			/* empty line */
			free(line);
			continue;
		}

		if (cf_line >= CONF_LINES)
			errx(1, "too much lines in config\n");

		/* commands */
		if (line[0] == ' ' || line[0] == '\t') {
			cp = line;
			while (*cp == ' ' || *cp == '\t')
				cp++;

			if (state == -1)
				errx(1, "no context for config line: %lu\n",
				    (u_long) lineno);

			conf_table[cf_line].dev = device;
			conf_table[cf_line].state = state;
			snprintf(conf_table[cf_line].cmd, CMD_LENGTH, "%s", cp);
			/* read state via fetch_state() at startup */
			prev_states[device] = -1;

			cf_line++;
			free(line);
			continue;
		}

		/* context */
		state = -1;

		if (sscanf(line, "%[^.].%19[^:]\n", devname, statename) != 2)
			errx(1, "bad state definition at: %d\n", lineno);

		if ((device = if_nametoindex(devname)) == 0)
			errx(1, "no such device %s at: %d\n", devname, lineno);

		for (i = nrofstates - 1; i >= 0; i--)
			if (strncmp(state_name[i], statename, 
			    strlen(state_name[i])) == 0)
				state = i;

		if (state == -1)
			errx(1, "bad state definition at: %d\n", lineno);

		if (device >= MAX_IFINDEX)
			errx(1, "ifindex exceeded at: %d\n", lineno);

		free(line);
	}
	fclose(fconfig);
}

void
loop(void)
{
	int sockfd;
	int n;
	char msg[2048];

	if ((sockfd = socket(PF_ROUTE, SOCK_RAW, 0)) < 0)
		errx(1, "no routing socket");

	if (!opt_inhibit)
		fetch_state();

	for (;;) {
		n = read(sockfd, msg, sizeof(msg)); 

		/* reread config, run commands for current states */
		if (got_sighup) {
			got_sighup = 0;
			LOG("%s\n", "restart");
			doconfig(configfile);
			if (!opt_inhibit)
				fetch_state();
		}

		eval_rtmsg((struct rt_msghdr *) msg, n);
	}
}

void
sighup_handler(int x)
{
	got_sighup = 1;
}

void
eval_rtmsg(struct rt_msghdr *rtm, int msglen)
{
	struct if_msghdr *ifm;

	/* XXX ignore errors? */
	if (msglen < sizeof(struct rt_msghdr))
		return;

	if (rtm->rtm_version != RTM_VERSION)
		return;

	if (rtm->rtm_type != RTM_IFINFO)
		return;

	ifm = (struct if_msghdr *)rtm;
	scan_table(ifm->ifm_index, ifm->ifm_data.ifi_link_state);
}


/*
 * Scan conf_table for commands to run.
 * Ignore, if there was no change to the previous state.
 */
void
scan_table(int if_index, int state)
{
	int i = 0;

	while (conf_table[i].cmd[0] != '\0') {
		if (conf_table[i].dev == if_index &&
		    conf_table[i].state == state &&
		    if_index <= MAX_IFINDEX &&
		    prev_states[if_index] != state) {

			LOG("%s\n", conf_table[i].cmd);
			system(conf_table[i].cmd);
		}
		i++;
	}
	prev_states[if_index] = state;
}

/* Fetch the current link states. */
void
fetch_state(void)
{
	int i;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	
	for (i = 0; i < MAX_IFINDEX; i++) {
		struct ifreq ifr;
		struct if_data  ifrdat;

		char if_name[IF_NAMESIZE];

		if (prev_states[i] != -1)
			continue;

		if_indextoname(i, if_name);

		strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
		ifr.ifr_data = (caddr_t)&ifrdat;

		if (ioctl(sock, SIOCGIFDATA, (caddr_t)&ifr) == -1)
			continue;

		scan_table(i, ifrdat.ifi_link_state);

	}
	close(sock);
}
