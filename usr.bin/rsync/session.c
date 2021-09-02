/*	$OpenBSD: session.c,v 1.9 2021/09/02 21:06:06 deraadt Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "extern.h"

/*
 * Accept how much we've read, written, and file-size, and print them in
 * a human-readable fashion (with GB, MB, etc. prefixes).
 * This only prints as the client.
 */
static void
stats_log(struct sess *sess,
	uint64_t tread, uint64_t twrite, uint64_t tsize)
{
	double		 tr, tw, ts;
	const char	*tru = "B", *twu = "B", *tsu = "B";
	int		 trsz = 0, twsz = 0, tssz = 0;

	assert(verbose);
	if (sess->opts->server)
		return;

	if (tread >= 1024 * 1024 * 1024) {
		tr = tread / (1024.0 * 1024.0 * 1024.0);
		tru = "GB";
		trsz = 3;
	} else if (tread >= 1024 * 1024) {
		tr = tread / (1024.0 * 1024.0);
		tru = "MB";
		trsz = 2;
	} else if (tread >= 1024) {
		tr = tread / 1024.0;
		tru = "KB";
		trsz = 1;
	} else
		tr = tread;

	if (twrite >= 1024 * 1024 * 1024) {
		tw = twrite / (1024.0 * 1024.0 * 1024.0);
		twu = "GB";
		twsz = 3;
	} else if (twrite >= 1024 * 1024) {
		tw = twrite / (1024.0 * 1024.0);
		twu = "MB";
		twsz = 2;
	} else if (twrite >= 1024) {
		tw = twrite / 1024.0;
		twu = "KB";
		twsz = 1;
	} else
		tw = twrite;

	if (tsize >= 1024 * 1024 * 1024) {
		ts = tsize / (1024.0 * 1024.0 * 1024.0);
		tsu = "GB";
		tssz = 3;
	} else if (tsize >= 1024 * 1024) {
		ts = tsize / (1024.0 * 1024.0);
		tsu = "MB";
		tssz = 2;
	} else if (tsize >= 1024) {
		ts = tsize / 1024.0;
		tsu = "KB";
		tssz = 1;
	} else
		ts = tsize;

	LOG1("Transfer complete: "
	    "%.*lf %s sent, %.*lf %s read, %.*lf %s file size",
	    trsz, tr, tru,
	    twsz, tw, twu,
	    tssz, ts, tsu);
}

/*
 * At the end of transmission, we write our statistics if we're the
 * server, then log only if we're not the server.
 * Either way, only do this if we're in verbose mode.
 * Returns zero on failure, non-zero on success.
 */
int
sess_stats_send(struct sess *sess, int fd)
{
	uint64_t tw, tr, ts;

	if (verbose == 0)
		return 1;

	tw = sess->total_write;
	tr = sess->total_read;
	ts = sess->total_size;

	if (sess->opts->server) {
		if (!io_write_ulong(sess, fd, tr)) {
			ERRX1("io_write_ulong");
			return 0;
		} else if (!io_write_ulong(sess, fd, tw)) {
			ERRX1("io_write_ulong");
			return 0;
		} else if (!io_write_ulong(sess, fd, ts)) {
			ERRX1("io_write_ulong");
			return 0;
		}
	}

	stats_log(sess, tr, tw, ts);
	return 1;
}

/*
 * At the end of the transmission, we have some statistics to read.
 * Only do this (1) if we're in verbose mode and (2) if we're the
 * server.
 * Then log the findings.
 * Return zero on failure, non-zero on success.
 */
int
sess_stats_recv(struct sess *sess, int fd)
{
	uint64_t tr, tw, ts;

	if (sess->opts->server || verbose == 0)
		return 1;

	if (!io_read_ulong(sess, fd, &tw)) {
		ERRX1("io_read_ulong");
		return 0;
	} else if (!io_read_ulong(sess, fd, &tr)) {
		ERRX1("io_read_ulong");
		return 0;
	} else if (!io_read_ulong(sess, fd, &ts)) {
		ERRX1("io_read_ulong");
		return 0;
	}

	stats_log(sess, tr, tw, ts);
	return 1;
}
