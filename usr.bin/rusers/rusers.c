/*	$OpenBSD: rusers.c,v 1.15 2001/11/02 17:16:22 millert Exp $	*/

/*
 * Copyright (c) 2001 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 *  Copyright (c) 1993 John Brezak
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] = "$OpenBSD: rusers.c,v 1.15 2001/11/02 17:16:22 millert Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <arpa/inet.h>
#include <rpcsvc/rusers.h>
#include <rpcsvc/rnusers.h>	/* Old protocol version */
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Preferred formatting */
#define HOST_WIDTH 17
#define LINE_WIDTH 8
#define NAME_WIDTH 8

int search_host(struct in_addr);
void remember_host(char **);
void fmt_idle(int, char *, size_t);
void print_longline(int, u_int, char *, char *, char *, char *, int);
void onehost(char *);
void allhosts(void);
bool_t rusers_reply(char *, struct sockaddr_in *);
bool_t rusers_reply_3(char *, struct sockaddr_in *);
__dead void usage(void);

int longopt;
int allopt;
long termwidth;
extern char *__progname;

struct host_list {
	struct host_list *next;
	struct in_addr addr;
} *hosts;

int
main(int argc, char **argv)
{
	struct winsize win;
	char *cp, *ep;
	int ch;
	
	while ((ch = getopt(argc, argv, "al")) != -1)
		switch (ch) {
		case 'a':
			allopt++;
			break;
		case 'l':
			longopt++;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}

	if (isatty(STDOUT_FILENO)) {
		if ((cp = getenv("COLUMNS")) != NULL && *cp != '\0') {
			termwidth = strtol(cp, &ep, 10);
			if (*ep != '\0' || termwidth >= INT_MAX ||
			    termwidth < 0)
				termwidth = 0;
		}
		if (termwidth == 0 &&
		    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == 0 &&
		    win.ws_col > 0)
			termwidth = win.ws_col;
		else
			termwidth = 80;
	}
	setlinebuf(stdout);
	if (argc == optind)
		allhosts();
	else {
		for (; optind < argc; optind++)
			(void) onehost(argv[optind]);
	}
	exit(0);
}

int
search_host(struct in_addr addr)
{
	struct host_list *hp;
	
	if (!hosts)
		return(0);

	for (hp = hosts; hp != NULL; hp = hp->next) {
		if (hp->addr.s_addr == addr.s_addr)
			return(1);
	}
	return(0);
}

void
remember_host(char **ap)
{
	struct host_list *hp;

	for (; *ap; ap++) {
		if (!(hp = malloc(sizeof(struct host_list))))
			err(1, NULL);
		hp->addr.s_addr = *(in_addr_t *)*ap;
		hp->next = hosts;
		hosts = hp;
	}
}

void
fmt_idle(int idle, char *idle_time, size_t idle_time_len)
{
	int days, hours, minutes, seconds;

	switch (idle) {
	case 0:
		*idle_time = '\0';
		break;
	case INT_MAX:
		strlcpy(idle_time, "??", idle_time_len);
		break;
	default:
		seconds = idle;
		days = seconds / (60*60*24);
		seconds %= (60*60*24);
		hours = seconds / (60*60);
		seconds %= (60*60);
		minutes = seconds / 60;
		seconds %= 60;
		if (idle >= (24*60*60))
			snprintf(idle_time, idle_time_len,
			    "%d days, %d:%02d:%02d",
			    days, hours, minutes, seconds);
		else if (idle >= (60*60))
			snprintf(idle_time, idle_time_len, "%2d:%02d:%02d",
			    hours, minutes, seconds);
		else if (idle > 60)
			snprintf(idle_time, idle_time_len, "%2d:%02d",
			    minutes, seconds);
		else
			snprintf(idle_time, idle_time_len, "   :%02d", idle);
		break;
	}
}

void
print_longline(int ut_time, u_int idle, char *host, char *user, char *line,
	       char *remhost, int remhostmax)
{
	char date[32], idle_time[64];
	char remote[RUSERS_MAXHOSTLEN + 1];
	int len;

	strftime(date, sizeof(date), "%h %d %R", localtime((time_t *)&ut_time));
	date[sizeof(date) - 1] = '\0';
	fmt_idle(idle, idle_time, sizeof(idle_time));
	len = termwidth -
	    (MAX(strlen(user), NAME_WIDTH) + 1 + HOST_WIDTH + 1 + LINE_WIDTH +
	    1 + strlen(date) + 1 + MAX(8, strlen(idle_time)) + 1 + 2);
	if (len > 0 && *remhost != '\0')
		snprintf(remote, sizeof(remote), "(%.*s)",
		    MIN(len, remhostmax), remhost);
	else
		remote[0] = '\0';
	len = HOST_WIDTH - MIN(HOST_WIDTH, strlen(host)) +
	    LINE_WIDTH - MIN(LINE_WIDTH, strlen(line));
	printf("%-*s %.*s:%.*s%-*s %-12s %8s %s\n",
	    NAME_WIDTH, user, HOST_WIDTH, host, LINE_WIDTH, line,
	    len, "", date, idle_time, remote);
}

bool_t
rusers_reply(char *replyp, struct sockaddr_in *raddrp)
{
	char user[RNUSERS_MAXUSERLEN + 1];
	char utline[RNUSERS_MAXLINELEN + 1];
	utmpidlearr *up = (utmpidlearr *)replyp;
	struct hostent *hp;
	char *host, *taddrs[2];
	int i;
	
	if (search_host(raddrp->sin_addr))
		return(0);

	if (!allopt && !up->uia_cnt)
		return(0);
	
	hp = gethostbyaddr((char *)&raddrp->sin_addr,
	    sizeof(struct in_addr), AF_INET);
	if (hp) {
		host = hp->h_name;
		remember_host(hp->h_addr_list);
	} else {
		host = inet_ntoa(raddrp->sin_addr);
		taddrs[0] = (char *)&raddrp->sin_addr;
		taddrs[1] = NULL;
		remember_host(taddrs);
	}
	
	if (!longopt)
		printf("%-*.*s ", HOST_WIDTH, HOST_WIDTH, host);
	
	for (i = 0; i < up->uia_cnt; i++) {
		/* NOTE: strncpy() used below for non-terminated strings. */
		strncpy(user, up->uia_arr[i]->ui_utmp.ut_name,
		    sizeof(user) - 1);
		user[sizeof(user) - 1] = '\0';
		if (longopt) {
			strncpy(utline, up->uia_arr[i]->ui_utmp.ut_line,
			    sizeof(utline) - 1);
			utline[sizeof(utline) - 1] = '\0';
			print_longline(up->uia_arr[i]->ui_utmp.ut_time,
			    up->uia_arr[i]->ui_idle, host, user, utline,
			    up->uia_arr[i]->ui_utmp.ut_host, RNUSERS_MAXHOSTLEN);
		} else {
			fputs(user, stdout);
			putchar(' ');
		}
	}
	if (!longopt)
		putchar('\n');
	
	return(0);
}

bool_t
rusers_reply_3(char *replyp, struct sockaddr_in *raddrp)
{
	char user[RUSERS_MAXUSERLEN + 1];
	char utline[RUSERS_MAXLINELEN + 1];
	utmp_array *up3 = (utmp_array *)replyp;
	struct hostent *hp;
	char *host, *taddrs[2];
	int i;
	
	if (search_host(raddrp->sin_addr))
		return(0);

	if (!allopt && !up3->utmp_array_len)
		return(0);
	
	hp = gethostbyaddr((char *)&raddrp->sin_addr,
	    sizeof(struct in_addr), AF_INET);
	if (hp) {
		host = hp->h_name;
		remember_host(hp->h_addr_list);
	} else {
		host = inet_ntoa(raddrp->sin_addr);
		taddrs[0] = (char *)&raddrp->sin_addr;
		taddrs[1] = NULL;
		remember_host(taddrs);
	}
	
	if (!longopt)
		printf("%-*.*s ", HOST_WIDTH, HOST_WIDTH, host);
	
	for (i = 0; i < up3->utmp_array_len; i++) {
		/* NOTE: strncpy() used below for non-terminated strings. */
		strncpy(user, up3->utmp_array_val[i].ut_user,
		    sizeof(user) - 1);
		user[sizeof(user) - 1] = '\0';
		if (longopt) {
			strncpy(utline, up3->utmp_array_val[i].ut_line,
			    sizeof(utline) - 1);
			utline[sizeof(utline) - 1] = '\0';
			print_longline(up3->utmp_array_val[i].ut_time,
			    up3->utmp_array_val[i].ut_idle, host, user, utline,
			    up3->utmp_array_val[i].ut_host, RUSERS_MAXHOSTLEN);
		} else {
			fputs(user, stdout);
			putchar(' ');
		}
	}
	if (!longopt)
		putchar('\n');
	
	return(0);
}

void
onehost(char *host)
{
	utmpidlearr up;
	utmp_array up3;
	CLIENT *rusers_clnt;
	struct sockaddr_in addr;
	struct hostent *hp;
	struct timeval tv = { 25, 0 };
	int error;
	
	hp = gethostbyname(host);
	if (hp == NULL)
		errx(1, "unknown host \"%s\"", host);

	/* try version 3 first */
	rusers_clnt = clnt_create(host, RUSERSPROG, RUSERSVERS_3, "udp");
	if (rusers_clnt == NULL) {
		clnt_pcreateerror(__progname);
		exit(1);
	}

	memset(&up3, 0, sizeof(up3));
	error = clnt_call(rusers_clnt, RUSERSPROC_NAMES, xdr_void, NULL,
	    xdr_utmp_array, &up3, tv);
	switch (error) {
	case RPC_SUCCESS:
		addr.sin_addr.s_addr = *(int *)hp->h_addr;
		rusers_reply_3((char *)&up3, &addr);
		clnt_destroy(rusers_clnt);
		return;
	case RPC_PROGVERSMISMATCH:
		clnt_destroy(rusers_clnt);
		break;
	default:
		clnt_perror(rusers_clnt, __progname);
		clnt_destroy(rusers_clnt);
		exit(1);
	}

	/* fall back to version 2 */
	rusers_clnt = clnt_create(host, RUSERSPROG, RUSERSVERS_IDLE, "udp");
	if (rusers_clnt == NULL) {
		clnt_pcreateerror(__progname);
		exit(1);
	}

	memset(&up, 0, sizeof(up));
	error = clnt_call(rusers_clnt, RUSERSPROC_NAMES, xdr_void, NULL,
	    xdr_utmpidlearr, &up, tv);
	if (error != RPC_SUCCESS) {
		clnt_perror(rusers_clnt, __progname);
		clnt_destroy(rusers_clnt);
		exit(1);
	}
	addr.sin_addr.s_addr = *(int *)hp->h_addr;
	rusers_reply((char *)&up, &addr);
	clnt_destroy(rusers_clnt);
}

void
allhosts(void)
{
	utmpidlearr up;
	utmp_array up3;
	enum clnt_stat clnt_stat;

	puts("Sending broadcast for rusersd protocol version 3...");
	memset(&up3, 0, sizeof(up3));
	clnt_stat = clnt_broadcast(RUSERSPROG, RUSERSVERS_3,
	    RUSERSPROC_NAMES, xdr_void, NULL, xdr_utmp_array,
	    (char *)&up3, rusers_reply_3);
	if (clnt_stat != RPC_SUCCESS && clnt_stat != RPC_TIMEDOUT)
		errx(1, "%s", clnt_sperrno(clnt_stat));

	puts("Sending broadcast for rusersd protocol version 2...");
	memset(&up, 0, sizeof(up));
	clnt_stat = clnt_broadcast(RUSERSPROG, RUSERSVERS_IDLE,
	    RUSERSPROC_NAMES, xdr_void, NULL, xdr_utmpidlearr,
	    (char *)&up, rusers_reply);
	if (clnt_stat != RPC_SUCCESS && clnt_stat != RPC_TIMEDOUT)
		errx(1, "%s", clnt_sperrno(clnt_stat));
}

void
usage(void)
{

	fprintf(stderr, "usage: %s [-la] [hosts ...]\n", __progname);
	exit(1);
}
