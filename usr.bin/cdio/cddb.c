/* $OpenBSD: cddb.c,v 1.11 2006/01/23 17:29:22 millert Exp $ */
/*
 * Copyright (c) 2002 Marc Espie.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/cdio.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include "extern.h"

unsigned long	cddb_sum(unsigned long);
void		send_hello(FILE *);
void		send_query(FILE *, int, struct cd_toc_entry *);
int		further_query(FILE *, char *);
int		connect_to(const char *, const char *);
int		parse_connect_to(const char *, const char *);
char *		get_line(FILE *);
char *		get_answer(FILE *);
void		verify_track_names(char **, int, struct cd_toc_entry *);
void		safe_copy(char **, const char *);

unsigned long
cddb_sum(unsigned long v)
{
	unsigned long sum = 0;

	while (v > 0) {
		sum += v % 10;
		v /= 10;
	}
	return (sum);
}

unsigned long
cddb_discid(int n, struct cd_toc_entry *e)
{
	unsigned long sum;
	int i;

	sum = 0;
	for (i =0; i < n; i++)
		sum += cddb_sum(entry2time(e+i));
	return (((sum % 0xff) << 24) |
	    ((entry2time(e+n) - entry2time(e)) << 8) | n);
}

void
send_hello(FILE *cout)
{
	char hostname[MAXHOSTNAMELEN];

	if (gethostname(hostname, sizeof(hostname)) == -1)
		strlcpy(hostname, "unknown", sizeof hostname);
	fprintf(cout, "CDDB HELLO %s %s cdio " VERSION "\r\n",
	    getlogin(), hostname);
	fflush(cout);
}

void
send_query(FILE *f, int n, struct cd_toc_entry *e)
{
	int i;

	fprintf(f, "cddb query %8lx %d", cddb_discid(n, e), n);
	for (i = 0; i < n; i++)
		fprintf(f, " %lu", entry2frames(e+i));
	fprintf(f, " %lu\r\n", (entry2frames(e+n)-entry2frames(e)) /75);
}

#define MAXSIZE 256
char copy_buffer[MAXSIZE];

void
safe_copy(char **p, const char *title)
{
	strnvis(copy_buffer, title, MAXSIZE-1, VIS_TAB|VIS_NL);
	if (*p == NULL)
		*p = strdup(copy_buffer);
	else {
		char *n;

		if (asprintf(&n, "%s%s", *p, copy_buffer) == -1)
			return;
		free(*p);
		*p = n;
	}
}

int
further_query(FILE *cout, char *line)
{
	char *key;
	char *title;

	key = strchr(line, ' ');
	if (!key)
		return 0;
	*key++ = 0;
	title = strchr(key, ' ');
	if (!title)
		return 0;
	*title++ = 0;
	strnvis(copy_buffer, title, MAXSIZE-1, VIS_TAB|VIS_NL);
	printf("%s", copy_buffer);
	strnvis(copy_buffer, line, MAXSIZE-1, VIS_TAB|VIS_NL);
	printf("(%s)\n", copy_buffer);
	fprintf(cout, "CDDB READ %s %s\r\n", line, key);
	fflush(cout);
	return 1;
}


int
connect_to(const char *host, const char *serv)
{
	int s = -1;
	struct addrinfo hints, *res0 = NULL, *res;
	int error;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	error = getaddrinfo(host, serv, &hints, &res0);
	if (error) {
		warnx("%s", gai_strerror(error));
		return -1;
	}

	for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);
		if (s == -1)
			continue;
		if (connect(s, res->ai_addr, res->ai_addrlen) == -1) {
			close(s);
			s = -1;
			continue;
		}
		break;
	}
	if (s == -1)
		warn("cddb");
	freeaddrinfo(res0);
	return s;
}

int
parse_connect_to(const char *host_port, const char *port)
{
	int s;
	char *last, *host;

	host = (char *)host_port;

	last = strrchr(host_port, ':');
	if (last != 0 && !(last != host && last[-1] == ':')) {
		port = last + 1;
		host = malloc(last - host_port + 1);
		if (!host)
			return -1;
		memcpy(host, host_port, last-host_port);
		host[last-host_port] = 0;
	}
	s = connect_to(host, port);
	if (host != host_port)
		free(host);
	return s;
}

char *
get_line(FILE *cin)
{
	char *line;
	size_t len;

	line = fgetln(cin, &len);
	if (!line)
		return NULL;
	if (len == 0)
		return NULL;
	if (line[len-1] == '\n')
		line[--len] = 0;
	if (len != 0 && line[len-1] == '\r')
		line[--len] = 0;
	return line;
}

char *
get_answer(FILE *cin)
{
	char *line;

	line = get_line(cin);
	if (!line || *line != '2')
		return NULL;
	else
		return line;
}

void
verify_track_names(char **names, int n, struct cd_toc_entry *e)
{
	int i;

	for (i = 0; i < n; i++) {
		if (names[i] == 0)
			names[i] = strdup(e->control & 4 ? "data" : "audio");
	}
}

char **
cddb(const char *host_port, int n, struct cd_toc_entry *e, char *arg)
{
	int s = -1;
	FILE *cin = NULL;
	FILE *cout = NULL;
	char *type;
	char *line;
	char **result = NULL;
	int i;

	s = parse_connect_to(host_port, "cddb");
	if (s == -1)
		goto end;
	cin = fdopen(s, "r");
	if (!cin) {
		warn("cddb: fdopen");
		goto end;
	}
	cout = fdopen(s, "w");
	s = -1;
	if (!cout) {
		warn("cddb: fdopen");
		goto end;
	}
	line = get_answer(cin);
	if (!line) {
		warnx("cddb: won't talk to us");
		goto end;
	}

	send_hello(cout);
	line = get_answer(cin);
	if (!line) {
		warnx("cddb: problem in hello");
		goto end;
	}

	send_query(cout, n, e);
	fflush(cout);
	line = get_answer(cin);
	if (!line) {
		warnx("cddb: problem in query");
		goto end;
	}
	type = strchr(line, ' ');
	if (!type)
		goto end;
	*type++ = 0;
	/* no match or other issue */
	if (strcmp(line, "202") == 0) {
		printf("cddb: No match in database\n");
		goto end;
	}
	if (strcmp(line, "211") == 0 || strcmp(line, "212") == 0) {
		int number = atoi(arg);
		if (number == 0) {
			if (strcmp(line, "211") == 0)
				printf("cddb: multiple matches\n");
			else {
				printf("cddb: inexact match\n");
				number = 1;
			}
		}
		if (number == 0) {
			for (i = 1;; i++) {
				line = get_line(cin);
				if (!line || strcmp(line, ".") == 0)
					goto end;
				printf("%d: %s\n", i, line);
			}
		} else {
			int ok = 0;

			for (i = 1;; i++) {
				line = get_line(cin);
				if (!line)
					break;
				if (strcmp(line, ".") == 0)
					break;
				if (i == number)
					ok = further_query(cout, line);
			}
			if (!ok)
				goto end;
		}
	} else if (strcmp(line, "200") != 0 || !further_query(cout, type))
		goto end;
	result = malloc(sizeof(char *) * (n + 1));
	if (!result)
		goto end;
	for (i = 0; i <= n; i++)
		result[i] = NULL;
	line = get_answer(cin);
	if (!line)
		goto end2;
	for (;;) {
		long k;
		char *end;

		line = get_line(cin);
		if (!line)
			goto end2;
		if (strcmp(line, ".") == 0)
			goto end;
		if (strncmp(line, "TTITLE", 6) != 0)
			continue;
		line += 6;
		k = strtol(line, &end, 10);
		if (*end++ != '=')
			continue;
		if (k >= n)
			continue;
		safe_copy(&result[k], end);
	}
	fprintf(cout, "QUIT\r\n");
	verify_track_names(result, n, e);
	goto end;
end2:
	free(result);
	result = NULL;
end:
	if (cout)
		fclose(cout);
	if (cin)
		fclose(cin);
	if (s != -1)
		close(s);
	return result;
}

void
free_names(char **names)
{
	int i;

	for (i = 0; names[i]; i++)
		free(names[i]);
	free(names);
}
