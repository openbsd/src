/*	$OpenBSD: compress_gzip.c,v 1.6 2013/01/26 09:37:23 gilles Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2012 Charles Longeau <chl@openbsd.org>
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
#include <err.h>
#include <fcntl.h>
#include <imsg.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zlib.h>

#include "smtpd.h"
#include "log.h"

static void*	compress_gzip_new(void);
static size_t	compress_gzip_chunk(void *, void *, size_t, void *, size_t);
static size_t	compress_gzip_finalize(void *, void *, size_t);
static void*	uncompress_gzip_new(void);
static size_t	uncompress_gzip_chunk(void *, void *, size_t, void *, size_t);
static size_t	uncompress_gzip_finalize(void *, void *, size_t);

struct compress_backend	compress_gzip = {
	compress_gzip_new,
	compress_gzip_chunk,
	compress_gzip_finalize,

	uncompress_gzip_new,
	uncompress_gzip_chunk,
	uncompress_gzip_finalize,
};

static void *
compress_gzip_new(void)
{
	return (NULL);
}

static size_t
compress_gzip_chunk(void *hdl, void *ib, size_t ibsz, void *ob, size_t obsz)
{
	return (-1);
}

static size_t
compress_gzip_finalize(void *hdl, void *ob, size_t obsz)
{
	return (-1);
}

static void *
uncompress_gzip_new(void)
{
	return (NULL);
}

static size_t
uncompress_gzip_chunk(void *hdl, void *ib, size_t ibsz, void *ob, size_t obsz)
{
	return (-1);
}

static size_t
uncompress_gzip_finalize(void *hdl, void *ob, size_t obsz)
{
	return (-1);
}
