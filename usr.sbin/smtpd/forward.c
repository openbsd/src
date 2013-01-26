/*	$OpenBSD: forward.c,v 1.33 2013/01/26 09:37:23 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
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

#include <ctype.h>
#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

#define	MAX_FORWARD_SIZE	(4 * 1024)
#define	MAX_EXPAND_NODES	(100)

int
forwards_get(int fd, struct expand *expand)
{
	FILE	       *fp = NULL;
	char	       *line = NULL;
	size_t		len;
	size_t		lineno;
	int		ret;
	struct stat	sb;

	ret = 0;
	if (fstat(fd, &sb) == -1)
		goto end;

	/* empty or over MAX_FORWARD_SIZE, temporarily fail */
	if (sb.st_size == 0) {
		log_info("info: forward file is empty");
		goto end;
	}
	if (sb.st_size >= MAX_FORWARD_SIZE) {
		log_info("info: forward file exceeds max size");
		goto end;
	}

	if ((fp = fdopen(fd, "r")) == NULL) {
		log_warn("warn: fdopen failure in forwards_get()");
		goto end;
	}

	while ((line = fparseln(fp, &len, &lineno, NULL, 0)) != NULL) {
		if (! expand_line(expand, line, 0)) {
			log_info("info: parse error in forward file");
			goto end;
		}
		if (expand->nb_nodes > MAX_EXPAND_NODES) {
			log_info("info: forward file expanded too many nodes");
			goto end;
		}
		free(line);
	}
	       
	ret = 1;

end:
	if (line)
		free(line);
	if (fp)
		fclose(fp);
	else
		close(fd);
	return ret;
}
