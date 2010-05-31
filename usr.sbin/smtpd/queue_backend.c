/*	$OpenBSD: queue_backend.c,v 1.1 2010/05/31 23:38:56 jacekm Exp $	*/

/*
 * Copyright (c) 2010 Jacek Masiulaniec <jacekm@dobremiasto.net>
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
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "queue_backend.h"

static char path[PATH_MAX];

static char *idchars = "ABCDEFGHIJKLMNOPQRSTUVWYZabcdefghijklmnopqrstuvwxyz0123456789";

int
queue_be_content_create(u_int64_t *content_id)
{
	int c, fd;

	c = idchars[arc4random_uniform(61)];
	snprintf(path, sizeof path, "content/%c/%cXXXXXXX", c, c);
	fd = mkstemp(path);
	if (fd < 0) {
		if (errno != ENOENT)
			return -1;
		if (mkdir(dirname(path), 0700) < 0)
			return -1;
		fd = mkstemp(path);
		if (fd < 0)
			return -1;
	}
	close(fd);
	*content_id = queue_be_encode(path + 10);
	return 0;
}

int
queue_be_content_open(u_int64_t content_id, int wr)
{
	char *id;

	id = queue_be_decode(content_id);
	snprintf(path, sizeof path, "content/%c/%s", id[0], id);
	return open(path, wr ? O_RDWR|O_APPEND|O_EXLOCK : O_RDONLY|O_SHLOCK);
}

void
queue_be_content_delete(u_int64_t content_id)
{
	char *id;

	id = queue_be_decode(content_id);
	snprintf(path, sizeof path, "content/%c/%s", id[0], id);
	unlink(path);
}

int
queue_be_action_new(u_int64_t content_id, u_int64_t *action_id, char *aux)
{
	FILE *fp;
	char *id;
	int fd;

	id = queue_be_decode(content_id);
	snprintf(path, sizeof path, "action/%c/%s,XXXXXXXX", id[0], id);
	fd = mkstemp(path);
	if (fd < 0) {
		if (errno != ENOENT)
			return -1;
		if (mkdir(dirname(path), 0700) < 0)
			return -1;
		fd = mkstemp(path);
		if (fd < 0)
			return -1;
	}
	fp = fdopen(fd, "w+");
	if (fp == NULL) {
		unlink(path);
		return -1;
	}
	fprintf(fp, "%s\n", aux);
	if (fclose(fp) == EOF) {
		unlink(path);
		return -1;
	}
	*action_id = queue_be_encode(path + 18);
	return 0;
}

int
queue_be_action_read(struct action_be *a, u_int64_t content_id, u_int64_t action_id)
{
	static char status[2048];
	static char aux[2048];
	struct stat sb_status, sb_content;
	char *id;
	FILE *fp;

	bzero(a, sizeof *a);
	a->content_id = content_id;
	a->action_id = action_id;

	/*
	 * Auxillary params for mta and mda.
	 */
	id = queue_be_decode(content_id);
	snprintf(path, sizeof path, "action/%c/%s,", id[0], id);
	strlcat(path, queue_be_decode(action_id), sizeof path);
	fp = fopen(path, "r");
	if (fp == NULL)
		return -1;
	if (fgets(aux, sizeof aux, fp) == NULL) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	aux[strcspn(aux, "\n")] = '\0';
	a->aux = aux;

	/*
	 * Error status message.
	 */
	id = queue_be_decode(content_id);
	snprintf(path, sizeof path, "status/%c/%s,", id[0], id);
	strlcat(path, queue_be_decode(action_id), sizeof path);
	fp = fopen(path, "r");
	if (fp) {
		if (fgets(status, sizeof status, fp) != NULL)
			status[strcspn(status, "\n")] = '\0';
		else
			status[0] = '\0';
		if (fstat(fileno(fp), &sb_status) < 0) {
			fclose(fp);
			return -1;
		}
		fclose(fp);
	} else
		status[0] = '\0';
	a->status = status;

	/*
	 * Message birth time.
	 *
	 * For bounces, use mtime of the status file.
	 * For non-bounces, use mtime of the content file.
	 */
	id = queue_be_decode(content_id);
	snprintf(path, sizeof path, "content/%c/%s", id[0], id);
	if (stat(path, &sb_content) < 0)
		return -1;
	if (sb_content.st_mode & S_IWUSR)
		a->birth = 0;
	else if (status[0] == '5' || status[0] == '6')
		a->birth = sb_status.st_mtime;
	else
		a->birth = sb_content.st_mtime;

	return 0;
}

int
queue_be_action_status(u_int64_t content_id, u_int64_t action_id, char *status)
{
	FILE *fp;
	char *id;

	id = queue_be_decode(content_id);
	snprintf(path, sizeof path, "status/%c/%s,", id[0], id);
	strlcat(path, queue_be_decode(action_id), sizeof path);
	fp = fopen(path, "w+");
	if (fp == NULL) {
		if (errno != ENOENT)
			return -1;
		mkdir(dirname(path), 0700);
		fp = fopen(path, "w+");
		if (fp == NULL)
			return -1;
	}
	if (fprintf(fp, "%s\n", status) == -1) {
		fclose(fp);
		return -1;
	}
	if (fclose(fp) == EOF)
		return -1;
	return 0;
}

void
queue_be_action_delete(u_int64_t content_id, u_int64_t action_id)
{
	char *id, *dir[] = { "action", "status" };
	u_int i;

	for (i = 0; i < 2; i++) {
		id = queue_be_decode(content_id);
		snprintf(path, sizeof path, "%s/%c/%s,", dir[i], id[0], id);
		id = queue_be_decode(action_id);
		strlcat(path, id, sizeof path);
		unlink(path);
	}
}

int
queue_be_commit(u_int64_t content_id)
{
	char *id;

	id = queue_be_decode(content_id);
	snprintf(path, sizeof path, "content/%c/%s", id[0], id);
	if (utimes(path, NULL) < 0 || chmod(path, 0400) < 0)
		return -1;
	return 0;
}

int
queue_be_getnext(struct action_be *a)
{
	static FTS	*fts;
	static FTSENT	*fe;
	char		*dir[] = { "action", NULL };

	if (fts == NULL) {
		fts = fts_open(dir, FTS_PHYSICAL|FTS_NOCHDIR, NULL);
		if (fts == NULL)
			return -1;
	}

	for (;;) {
		fe = fts_read(fts);
		if (fe == NULL) {
			if (errno) {
				fts_close(fts);
				return -1;
			} else {
				if (fts_close(fts) < 0)
					return -1;
				a->content_id = 0;
				a->action_id = 0;
				return 0;
			}
		}
		switch (fe->fts_info) {
		case FTS_F:
			break;
		case FTS_D:
		case FTS_DP:
			continue;
		default:
			fts_close(fts);
			return -1;
		}
		break;
	}

	if (fe->fts_namelen != 17 || fe->fts_name[8] != ',') {
		fts_close(fts);
		return -1;
	}
	a->content_id = queue_be_encode(fe->fts_name);
	a->action_id = queue_be_encode(fe->fts_name + 9);
	if (queue_be_action_read(a, a->content_id, a->action_id) < 0)
		return -2;

	return 0;
}

char *
queue_be_decode(u_int64_t id)
{
        static char txt[9];

	memcpy(txt, &id, sizeof id);
	txt[8] = '\0';
	return txt;
}

u_int64_t
queue_be_encode(const char *txt)
{
        u_int64_t id;

	if (strlen(txt) < sizeof id)
		id = INVALID_ID;
	else
		memcpy(&id, txt, sizeof id);

        return id;
}

int
queue_be_init(char *prefix, uid_t uid, gid_t gid)
{
	char *dir[] = { "action", "content", "status" };
	int i;

	for (i = 0; i < 3; i++) {
		snprintf(path, sizeof path, "%s/%s", prefix, dir[i]);
		if (mkdir(path, 0700) < 0 && errno != EEXIST)
			return -1;
		if (chmod(path, 0700) < 0)
			return -1;
		if (chown(path, uid, gid) < 0)
			return -1;
	}
	return 0;
}
