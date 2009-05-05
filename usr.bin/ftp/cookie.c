/*	$OpenBSD: cookie.c,v 1.5 2009/05/05 19:35:30 martynas Exp $	*/

/*
 * Copyright (c) 2007 Pierre-Yves Ritschard <pyr@openbsd.org>
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

#ifndef SMALL

#include <sys/types.h>
#include <sys/queue.h>

#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "ftp_var.h"

struct cookie {
	TAILQ_ENTRY(cookie)	 entry;
	TAILQ_ENTRY(cookie)	 tempentry;
	u_int8_t		 flags;
#define F_SECURE		 0x01
#define F_TAILMATCH		 0x02
#define F_NOEXPIRY		 0x04
#define F_MATCHPATH		 0x08
	time_t			 expires;
	char			*domain;
	char			*path;
	char			*key;
	char			*val;
};
TAILQ_HEAD(cookiejar, cookie);

typedef enum {
	DOMAIN = 0, TAILMATCH = 1, PATH = 2, SECURE = 3,
	EXPIRES = 4, NAME = 5, VALUE = 6, DONE = 7 
} field_t;

static struct cookiejar jar;

void
cookie_load(void)
{
	field_t		 field;
	size_t		 len;
	time_t		 date;
	char		*line;
	char		*lbuf;
	char		*param;
	const char	*estr;
	FILE		*fp;
	struct cookie	*ck;

	if (cookiefile == NULL)
		return;

	TAILQ_INIT(&jar);
	fp = fopen(cookiefile, "r");
	if (fp == NULL)
		err(1, "cannot open cookie file %s", cookiefile);
	date = time(NULL);
	lbuf = NULL;
	while ((line = fgetln(fp, &len)) != NULL) {
		if (line[len - 1] == '\n') {
			line[len - 1] = '\0';
			--len;
		} else {
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, line, len);
			lbuf[len] = '\0';
			line = lbuf;
		}
		line[strcspn(line, "\r")] = '\0';

		line += strspn(line, " \t");
		if ((*line == '#') || (*line == '\0')) {
			continue;
		}
		field = DOMAIN;
		ck = calloc(1, sizeof(*ck));
		if (ck == NULL)
			err(1, NULL);
		while ((param = strsep(&line, "\t")) != NULL) {
			switch (field) {
			case DOMAIN:
				if (*param == '.') {
					if (asprintf(&ck->domain,
					    "*%s", param) == -1)
						err(1, NULL);
				} else {
					ck->domain = strdup(param);
					if (ck->domain == NULL)
						err(1, NULL);
				}
				break;
			case TAILMATCH:
				if (strcasecmp(param, "TRUE") == 0) {
					ck->flags |= F_TAILMATCH;
				} else if (strcasecmp(param, "FALSE") != 0) {
					errx(1, "invalid cookie file");
				}
				break;
			case PATH:
				if (strcmp(param, "/") != 0) {
					ck->flags |= F_MATCHPATH;
					if (asprintf(&ck->path,
					    "%s*", param) == -1)
						err(1, NULL);
				}
				break;
			case SECURE:
				if (strcasecmp(param, "TRUE") == 0) {
					ck->flags |= F_SECURE;
				} else if (strcasecmp(param, "FALSE") != 0) {
					errx(1, "invalid cookie file");
				}
				break;
			case EXPIRES:
				/*
				 * rely on sizeof(time_t) being 4
				 */
				ck->expires = strtonum(param, 0,
				    INT_MAX, &estr);
				if (estr) {
					if (errno == ERANGE)
						ck->flags |= F_NOEXPIRY;
					else
						errx(1, "invalid cookie file");
				}
				break;
			case NAME:
				ck->key = strdup(param);
				if (ck->key == NULL)
					err(1, NULL);
				break;
			case VALUE:
				ck->val = strdup(param);
				if (ck->val == NULL)
					err(1, NULL);
				break;
			case DONE:
				errx(1, "invalid cookie file");
				break;
			}
			field++;
		}
		if (field != DONE)
			errx(1, "invalid cookie file");
		if (ck->expires < date && !(ck->flags & F_NOEXPIRY)) {
			free(ck->val);
			free(ck->key);
			free(ck->path);
			free(ck->domain);
			free(ck);
		} else
			TAILQ_INSERT_TAIL(&jar, ck, entry);
	}	
	free(lbuf);
	fclose(fp);
}

void
cookie_get(const char *domain, const char *path, int secure, char **pstr)
{
	size_t		 len;
	size_t		 headlen;
	char		*head;
	char		*str;
	struct cookie	*ck;
	struct cookiejar tempjar;

	*pstr = NULL;

	if (cookiefile == NULL)
		return;

	TAILQ_INIT(&tempjar);
	len = strlen("Cookie\r\n");

	TAILQ_FOREACH(ck, &jar, entry) {
		if (fnmatch(ck->domain, domain, 0) == 0 &&
		    (secure || !(ck->flags & F_SECURE))) {

			if (ck->flags & F_MATCHPATH &&
			    fnmatch(ck->path, path, 0) != 0)
				continue;

			len += strlen(ck->key) + strlen(ck->val) +
			    strlen("; =");
			TAILQ_INSERT_TAIL(&tempjar, ck, tempentry);
		}
	}
	if (TAILQ_EMPTY(&tempjar))
		return;
	len += 1;
	str = malloc(len);
	if (str == NULL)
		err(1, NULL);

	(void)strlcpy(str, "Cookie:", len);
	TAILQ_FOREACH(ck, &tempjar, tempentry) {
		head = str + strlen(str);
		headlen = len - strlen(str);

		snprintf(head, headlen, "%s %s=%s",
		    (ck == TAILQ_FIRST(&tempjar))? "" : ";", ck->key, ck->val);
	}
	if (strlcat(str, "\r\n", len) >= len)
		errx(1, "cookie header truncated");
	*pstr = str;
}

#endif /* !SMALL */

