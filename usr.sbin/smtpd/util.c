/*	$OpenBSD: util.c,v 1.12 2009/02/22 11:44:29 form Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

int
bsnprintf(char *str, size_t size, const char *format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = vsnprintf(str, size, format, ap);
	va_end(ap);
	if (ret == -1 || ret >= (int)size)
		return 0;

	return 1;
}

/* Close file, signifying temporary error condition (if any) to the caller. */
int
safe_fclose(FILE *fp)
{
	if (ferror(fp)) {
		fclose(fp);
		return 0;
	}
	if (fflush(fp)) {
		fclose(fp);
		if (errno == ENOSPC)
			return 0;
		fatal("safe_fclose: fflush");
	}
	if (fsync(fileno(fp)))
		fatal("safe_fclose: fsync");
	if (fclose(fp))
		fatal("safe_fclose: fclose");

	return 1;
}

struct passwd *
safe_getpwnam(const char *name)
{
	struct passwd *ret;

	ret = getpwnam(name);
	endpwent();

	return ret;
}

struct passwd *
safe_getpwuid(uid_t uid)
{
	struct passwd *ret;

	ret = getpwuid(uid);
	endpwent();

	return ret;
}

int
hostname_match(char *hostname, char *pattern)
{
	while (*pattern != '\0' && *hostname != '\0') {
		if (*pattern == '*') {
			while (*pattern == '*')
				pattern++;
			while (*hostname != '\0' &&
			    tolower(*hostname) != tolower(*pattern))
				hostname++;
			continue;
		}

		if (tolower(*pattern) != tolower(*hostname))
			return 0;
		pattern++;
		hostname++;
	}

	return (*hostname == '\0' && *pattern == '\0');
}

int
recipient_to_path(struct path *path, char *recipient)
{
	char *username;
	char *hostname;

	username = recipient;
	hostname = strrchr(username, '@');

	if (username[0] == '\0') {
		*path->user = '\0';
		*path->domain = '\0';
		return 1;
	}

	if (hostname == NULL) {
		if (strcasecmp(username, "postmaster") != 0)
			return 0;
		hostname = "localhost";
	} else {
		*hostname++ = '\0';
	}

	if (strlcpy(path->user, username, sizeof(path->user))
	    >= sizeof(path->user))
		return 0;

	if (strlcpy(path->domain, hostname, sizeof(path->domain))
	    >= sizeof(path->domain))
		return 0;

	return 1;
}

int
valid_localpart(char *s)
{
#define IS_ATEXT(c)     (isalnum(c) || strchr("!#$%&'*+-/=?^_`{|}~", (c)))
nextatom:
        if (! IS_ATEXT(*s) || *s == '\0')
                return 0;
        while (*(++s) != '\0') {
                if (*s == '.')
                        break;
                if (IS_ATEXT(*s))
                        continue;
                return 0;
        }
        if (*s == '.') {
                s++;
                goto nextatom;
        }
        return 1;
}

int
valid_domainpart(char *s)
{
nextsub:
        if (!isalnum(*s))
                return 0;
        while (*(++s) != '\0') {
                if (*s == '.')
                        break;
                if (isalnum(*s) || *s == '-')
                        continue;
                return 0;
        }
        if (s[-1] == '-')
                return 0;
        if (*s == '.') {
		s++;
                goto nextsub;
	}
        return 1;
}
