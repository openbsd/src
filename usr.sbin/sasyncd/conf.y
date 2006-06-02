/*	$OpenBSD: conf.y,v 1.11 2006/06/02 20:31:48 moritz Exp $	*/

/*
 * Copyright (c) 2005 Håkan Olsson.  All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Definitions */
%{
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "sasyncd.h"
#include "net.h"

/* Global configuration context.  */
struct cfgstate	cfgstate;

/* Local variables */
int	conflen = 0;
char	*confbuf, *confptr;

int	yyparse(void);
int	yylex(void);
void	yyerror(const char *);
%}

%union {
	char	*string;
	int	 val;
}

%token MODE INTERFACE INTERVAL LISTEN ON PORT PEER SHAREDKEY
%token Y_SLAVE Y_MASTER INET INET6 FLUSHMODE STARTUP NEVER SYNC
%token GROUP SKIPSLAVE
%token <string> STRING
%token <val>	VALUE
%type  <val>	af port mode flushmode

%%
/* Rules */

settings	: /* empty */
		| settings setting
		;

af		: /* empty */		{ $$ = AF_UNSPEC; }
		| INET			{ $$ = AF_INET; }
		| INET6			{ $$ = AF_INET6; }
		;

port		: /* empty */		{ $$ = SASYNCD_DEFAULT_PORT; }
		| PORT VALUE		{ $$ = $2; }
		;

mode		: Y_MASTER		{ $$ = MASTER; }
		| Y_SLAVE		{ $$ = SLAVE; }
		;

modes		: SKIPSLAVE
		{
			cfgstate.flags |= SKIP_LOCAL_SAS;
			log_msg(2, "config: not syncing SA to peers");
		}
		| mode
		{
			const char *m[] = CARPSTATES;
			cfgstate.lockedstate = $1;
			log_msg(2, "config: mode set to %s", m[$1]);
		}
		;

flushmode	: STARTUP		{ $$ = FM_STARTUP; }
		| NEVER			{ $$ = FM_NEVER; }
		| SYNC			{ $$ = FM_SYNC; }
		;

setting		: INTERFACE STRING
		{
			if (cfgstate.carp_ifname)
				free(cfgstate.carp_ifname);
			cfgstate.carp_ifname = $2;
			log_msg(2, "config: interface %s",
			    cfgstate.carp_ifname);
		}
		| GROUP STRING
		{
			if (cfgstate.carp_ifgroup)
				free(cfgstate.carp_ifgroup);
			cfgstate.carp_ifgroup = $2;
			log_msg(2, "config: group %s",
			    cfgstate.carp_ifgroup);
		}
		| FLUSHMODE flushmode
		{
			const char *fm[] = { "STARTUP", "NEVER", "SYNC" };
			cfgstate.flags |= $2;
			log_msg(2, "config: flush mode set to %s", fm[$2]);
		}
		| PEER STRING
		{
			struct syncpeer	*peer;
			int		 dup = 0;

			for (peer = LIST_FIRST(&cfgstate.peerlist); peer;
			     peer = LIST_NEXT(peer, link))
				if (strcmp($2, peer->name) == 0) {
					dup++;
					break;
				}
			if (dup)
				free($2);
			else {
				peer = (struct syncpeer *)calloc(1,
				    sizeof *peer);
				if (!peer) {
					log_err("config: calloc(1, %lu) "
					    "failed", sizeof *peer);
					free($2);
					YYERROR;
				}
				peer->name = $2;
			}
			LIST_INSERT_HEAD(&cfgstate.peerlist, peer, link);
			cfgstate.peercnt++;
			log_msg(2, "config: add peer %s", peer->name);
		}
		| LISTEN ON STRING af port
		{
			char pstr[20];

			if (cfgstate.listen_on)
				free(cfgstate.listen_on);
			cfgstate.listen_on = $3;
			cfgstate.listen_family = $4;
			cfgstate.listen_port = $5;
			if (cfgstate.listen_port < 1 ||
			    cfgstate.listen_port > 65534) {
				cfgstate.listen_port = SASYNCD_DEFAULT_PORT;
				log_msg(0, "config: bad port, listen-port "
				    "reset to %u", SASYNCD_DEFAULT_PORT);
			}
			if ($5 != SASYNCD_DEFAULT_PORT)
				snprintf(pstr, sizeof pstr, "port %d",$5);
			log_msg(2, "config: listen on %s %s%s",
			    cfgstate.listen_on, $4 == AF_INET6 ? "(IPv6) " :
			    ($4 == AF_INET ? "(IPv4) " : ""),
			    $5 != SASYNCD_DEFAULT_PORT ? pstr : "");
		}
		| MODE modes
		| SHAREDKEY STRING
		{
			if (cfgstate.sharedkey)
				free(cfgstate.sharedkey);
			cfgstate.sharedkey = $2;
			log_msg(2, "config: shared key set");
		}
		;

%%
/* Program */

struct keyword {
	char *name;
	int   value;
};

static int
match_cmp(const void *a, const void *b)
{
	return strcmp(a, ((const struct keyword *)b)->name);
}

static int
match(char *token)
{
	/* Sorted */
	static const struct keyword keywords[] = {
		{ "flushmode", FLUSHMODE },
		{ "group", GROUP },
		{ "inet", INET },
		{ "inet6", INET6 },
		{ "interface", INTERFACE },
		{ "listen", LISTEN },
		{ "master", Y_MASTER },
		{ "mode", MODE },
		{ "never", NEVER },
		{ "on", ON },
		{ "peer", PEER },
		{ "port", PORT },
		{ "sharedkey", SHAREDKEY },
		{ "skipslave", SKIPSLAVE },
		{ "slave", Y_SLAVE },
		{ "startup", STARTUP },
		{ "sync", SYNC },
	};
	const struct keyword *k;

	k = bsearch(token, keywords, sizeof keywords / sizeof keywords[0],
	    sizeof keywords[0], match_cmp);

	return k ? k->value : STRING;
}

int
yylex(void)
{
	char *p;
	int v;

	/* Locate next token */
	if (!confptr)
		confptr = confbuf;
	else {
		for (p = confptr; *p && p < confbuf + conflen; p++)
			;
		p++;
		if (!*p)
			return 0;
		confptr = p;
	}

	/* Numerical token? */
	if (isdigit(*confptr)) {
		for (p = confptr; *p; p++)
			if (*p == '.') /* IP address, or bad input */
				goto is_string;
		v = (int)strtol(confptr, (char **)NULL, 10);
		yylval.val = v;
		return VALUE;
	}

  is_string:
	v = match(confptr);
	if (v == STRING) {
		yylval.string = strdup(confptr);
		if (!yylval.string) {
			log_err("yylex: strdup()");
			exit(1);
		}
	}
	return v;
}

static int
conf_parse_file(char *cfgfile)
{
	struct stat	st;
	int		fd, r;
	char		*buf, *s, *d;
	struct passwd	*pw = getpwnam(SASYNCD_USER);

	if (stat(cfgfile, &st) != 0)
		goto bad;

	/* Valid file? */
	if ((st.st_uid && st.st_uid != pw->pw_uid) ||
	    ((st.st_mode & S_IFMT) != S_IFREG) ||
	    ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0)) {
		log_msg(0, "configuration file has bad owner, type or mode");
		goto bad;
	}

	fd = open(cfgfile, O_RDONLY, 0);
	if (fd < 0)
		goto bad;

	conflen = st.st_size;
	buf = (char *)malloc(conflen + 1);
	if (!buf) {
		log_err("malloc(%d) failed", conflen + 1);
		close(fd);
		return 1;
	}

	if (read(fd, buf, conflen) != conflen) {
		log_err("read() failed");
		free(buf);
		close(fd);
		return 1;
	}
	close(fd);

	/* Prepare the buffer somewhat in the way of strsep() */
	buf[conflen] = (char)0;
	for (s = buf, d = s; *s && s < buf + conflen; s++) {
		if (isspace(*s) && isspace(*(s+1)))
			continue;
		if (*s == '#') {
			while (*s != '\n' && s < buf + conflen)
				s++;
			continue;
		}
		if (d == buf && isspace(*s))
			continue;
		*d++ = *s;
	}
	*d = (char)0;
	for (s = buf; s <= d; s++)
		if (isspace(*s))
			*s = (char)0;

	confbuf = buf;
	confptr = NULL;
	r = yyparse();
	free(buf);

	if (!cfgstate.carp_ifgroup)
		cfgstate.carp_ifgroup = strdup("carp");

	return r;

  bad:
	log_msg(0, "failed to open \"%s\"", cfgfile);
	return 1;
}

int
conf_init(int argc, char **argv)
{
	char	*cfgfile = 0;
	int	 ch;

	memset(&cfgstate, 0, sizeof cfgstate);
	cfgstate.runstate = INIT;
	LIST_INIT(&cfgstate.peerlist);

	cfgstate.listen_port = SASYNCD_DEFAULT_PORT;

	while ((ch = getopt(argc, argv, "c:dv")) != -1) {
		switch (ch) {
		case 'c':
			if (cfgfile)
				return 2;
			cfgfile = optarg;
			break;
		case 'd':
			cfgstate.debug++;
			break;
		case 'v':
			cfgstate.verboselevel++;
			break;
		default:
			return 2;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		return 2;

	if (!cfgfile)
		cfgfile = SASYNCD_CFGFILE;

	if (conf_parse_file(cfgfile) == 0) {
		if (!cfgstate.sharedkey) {
			fprintf(stderr, "config: "
			    "no shared key specified, cannot continue");
			return 1;
		}
		return 0;
	}

	return 1;
}

void
yyerror(const char *s)
{
	fprintf(stderr, "config: %s\n", s);
}
