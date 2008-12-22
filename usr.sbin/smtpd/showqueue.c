/*	$OpenBSD: showqueue.c,v 1.3 2008/12/22 13:28:10 jacekm Exp $	*/

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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"

void		show_queue(int);
void		show_runqueue(int);
void		list_bucket(char *, int);
void		list_message(char *, char *, int);
void		display_envelope(struct message *, int);

void
show_queue(int flags)
{
	DIR *dirp;
	struct dirent *dp;
	u_int16_t bucket;
	const char *errstr;

	dirp = opendir(PATH_SPOOL PATH_QUEUE);
	if (dirp == NULL)
		err(1, "%s", PATH_SPOOL PATH_QUEUE);

	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		bucket = strtonum(dp->d_name, 0, DIRHASH_BUCKETS - 1, &errstr);
		if (errstr) {
			warnx("warning: invalid bucket \"%s\" in queue.",
			    dp->d_name);
			continue;
		}

		list_bucket(dp->d_name, flags);

	}

	closedir(dirp);
}

void
list_bucket(char *bucket, int flags)
{
	DIR *dirp;
	struct dirent *dp;
	char pathname[MAXPATHLEN];

	snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_SPOOL PATH_QUEUE, bucket);

	dirp = opendir(pathname);
	if (dirp == NULL) {
		if (errno == ENOENT)
			return;
		err(1, "%s", pathname);
	}

	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		list_message(bucket, dp->d_name, flags);
	}

	closedir(dirp);
}

void
list_message(char *bucket, char *message, int flags)
{
	DIR *dirp;
	struct dirent *dp;
	char pathname[MAXPATHLEN];
	FILE *fp;
	struct message envelope;

	snprintf(pathname, MAXPATHLEN, "%s/%s/%s%s", PATH_SPOOL PATH_QUEUE,
	    bucket, message, PATH_ENVELOPES);

	dirp = opendir(pathname);
	if (dirp == NULL) {
		if (errno == ENOENT)
			return;
		err(1, "%s", pathname);
	}

	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		snprintf(pathname, MAXPATHLEN, "%s/%s/%s%s/%s", PATH_SPOOL PATH_QUEUE,
		    bucket, message, PATH_ENVELOPES, dp->d_name);

		fp = fopen(pathname, "r");
		if (fp == NULL) {
			if (errno == ENOENT)
				continue;
			err(1, "%s", pathname);
		}

		if (fread(&envelope, sizeof(struct message), 1, fp) != 1)
			err(1, "%s", pathname);

		fclose(fp);

		display_envelope(&envelope, flags);
	}

	closedir(dirp);
}

void
show_runqueue(int flags)
{
	DIR *dirp;
	struct dirent *dp;
	char pathname[MAXPATHLEN];
	FILE *fp;
	struct message envelope;

	dirp = opendir(PATH_SPOOL PATH_RUNQUEUE);
	if (dirp == NULL)
		err(1, "%s", PATH_SPOOL PATH_RUNQUEUE);

	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_SPOOL PATH_RUNQUEUE,
		    dp->d_name);

		fp = fopen(pathname, "r");
		if (fp == NULL) {
			if (errno == ENOENT)
				continue;
			err(1, "%s", pathname);
		}

		if (fread(&envelope, sizeof(struct message), 1, fp) != 1)
			err(1, "%s", pathname);

		fclose(fp);

		display_envelope(&envelope, flags);

	}

	closedir(dirp);
}

void
display_envelope(struct message *envelope, int flags)
{
	switch (envelope->type) {
	case T_MDA_MESSAGE:
		printf("MDA");
		break;
	case T_MTA_MESSAGE:
		printf("MTA");
		break;
	case T_MDA_MESSAGE|T_DAEMON_MESSAGE:
		printf("MDA-DAEMON");
		break;
	case T_MTA_MESSAGE|T_DAEMON_MESSAGE:
		printf("MTA-DAEMON");
		break;
	default:
		printf("UNKNOWN");
	}

	printf("|%s|%s@%s|%s@%s|%d|%u\n",
	    envelope->message_uid,
	    envelope->sender.user, envelope->sender.domain,
	    envelope->recipient.user, envelope->recipient.domain,
	    envelope->lasttry,
	    envelope->retry);
}
