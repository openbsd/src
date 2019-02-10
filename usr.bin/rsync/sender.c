/*	$Id: sender.c,v 1.3 2019/02/10 23:43:31 benno Exp $ */
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
#include <sys/stat.h>

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * A client sender manages the read-only source files and sends data to
 * the receiver as requested.
 * First it sends its list of files, then it waits for the server to
 * request updates to individual files.
 * Returns zero on failure, non-zero on success.
 *
 * Pledges: stdio, rpath, unveil.
 */
int
rsync_sender(struct sess *sess, int fdin,
	int fdout, size_t argc, char **argv)
{
	struct flist	*fl = NULL;
	size_t		 flsz = 0, phase = 0, excl;
	int		 rc = 0, c;
	int32_t		 idx;
	struct blkset	*blks = NULL;

	if (-1 == pledge("stdio rpath unveil", NULL)) {
		ERR(sess, "pledge");
		return 0;
	}

	/*
	 * Generate the list of files we want to send from our
	 * command-line input.
	 * This will also remove all invalid files.
	 */

	if ( ! flist_gen(sess, argc, argv, &fl, &flsz)) {
		ERRX1(sess, "flist_gen");
		goto out;
	}

	/* Client sends zero-length exclusions if deleting. */

	if ( ! sess->opts->server && sess->opts->del &&
	     ! io_write_int(sess, fdout, 0)) {
		ERRX1(sess, "io_write_int");
		goto out;
	}

	/*
	 * Then the file list in any mode.
	 * Finally, the IO error (always zero for us).
	 */

	if ( ! flist_send(sess, fdin, fdout, fl, flsz)) {
		ERRX1(sess, "flist_send");
		goto out;
	} else if ( ! io_write_int(sess, fdout, 0)) {
		ERRX1(sess, "io_write_int");
		goto out;
	}

	/* Exit if we're the server with zero files. */

	if (0 == flsz && sess->opts->server) {
		WARNX(sess, "sender has empty file list: exiting");
		rc = 1;
		goto out;
	} else if ( ! sess->opts->server)
		LOG1(sess, "Transfer starting: %zu files", flsz);

	/*
	 * If we're the server, read our exclusion list.
	 * This is always 0 for now.
	 */

	if (sess->opts->server) {
		if ( ! io_read_size(sess, fdin, &excl)) {
			ERRX1(sess, "io_read_size");
			goto out;
		} else if (0 != excl) {
			ERRX1(sess, "exclusion list is non-empty");
			goto out;
		}
	}

	/*
	 * We have two phases: the first has a two-byte checksum, the
	 * second has a full 16-byte checksum.
	 */

	LOG2(sess, "sender transmitting phase 1 data");

	for (;;) {
		if ( ! io_read_int(sess, fdin, &idx)) {
			ERRX1(sess, "io_read_int");
			goto out;
		}

		/*
		 * If we receive an invalid index (-1), then we're
		 * either promoted to the second phase or it's time to
		 * exit, depending upon which phase we're in.
		 */

		if (-1 == idx) {
			if ( ! io_write_int(sess, fdout, idx)) {
				ERRX1(sess, "io_write_int");
				goto out;
			}

			/* FIXME: I don't understand this ack. */

			if (sess->opts->server && sess->rver > 27)
				if ( ! io_write_int(sess, fdout, idx)) {
					ERRX1(sess, "io_write_int");
					goto out;
				}

			if (phase++)
				break;
			LOG2(sess, "sender transmitting phase 2 data");
			continue;
		}

		/* Validate index and file type. */

		if (idx < 0 || (uint32_t)idx >= flsz) {
			ERRX(sess, "file index out of bounds: "
				"invalid %" PRId32 " out of %zu",
				idx, flsz);
			goto out;
		} else if (S_ISDIR(fl[idx].st.mode)) {
			ERRX(sess, "blocks requested for "
				"directory: %s", fl[idx].path);
			goto out;
		} else if (S_ISLNK(fl[idx].st.mode)) {
			ERRX(sess, "blocks requested for "
				"symlink: %s", fl[idx].path);
			goto out;
		} else if ( ! S_ISREG(fl[idx].st.mode)) {
			ERRX(sess, "blocks requested for "
				"special: %s", fl[idx].path);
			goto out;
		}

		if ( ! sess->opts->server)
			LOG1(sess, "%s", fl[idx].wpath);

		/* Dry-run doesn't do anything. */

		if (sess->opts->dry_run) {
			if ( ! io_write_int(sess, fdout, idx)) {
				ERRX1(sess, "io_write_int");
				goto out;
			}
			continue;
		}

		/*
		 * The server will now send us its view of the file.
		 * It does so by cutting a file into a series of blocks
		 * and checksumming each block.
		 * We can then compare the blocks in our file and those
		 * in theirs, and send them blocks they're missing or
		 * don't have.
		 */

		blks = blk_recv(sess, fdin, fl[idx].path);
		if (NULL == blks) {
			ERRX1(sess, "blk_recv");
			goto out;
		} else if ( ! blk_recv_ack(sess, fdout, blks, idx)) {
			ERRX1(sess, "blk_recv_ack");
			goto out;
		}

		c = blk_match(sess, fdout, blks, fl[idx].path);
		blkset_free(blks);

		if ( ! c) {
			ERRX1(sess, "blk_match");
			goto out;
		}
	}

	if ( ! sess_stats_send(sess, fdout)) {
		ERRX1(sess, "sess_stats_end");
		goto out;
	}

	/* Final "goodbye" message. */

	if ( ! io_read_int(sess, fdin, &idx)) {
		ERRX1(sess, "io_read_int");
		goto out;
	} else if (-1 != idx) {
		ERRX(sess, "read incorrect update complete ack");
		goto out;
	}

	LOG2(sess, "sender finished updating");
	rc = 1;
out:
	flist_free(fl, flsz);
	return rc;
}
