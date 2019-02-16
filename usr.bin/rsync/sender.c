/*	$Id: sender.c,v 1.8 2019/02/16 16:57:17 florian Exp $ */
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
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * A request from the receiver to download updated file data.
 */
struct	send_dl {
	int32_t	 	     idx; /* index in our file list */
	struct blkset	    *blks; /* the sender's block information */
	TAILQ_ENTRY(send_dl) entries;
};

/*
 * The current file being "updated": sent from sender to receiver.
 * If there is no file being uploaded, "cur" is NULL.
 */
struct	send_up {
	struct send_dl	*cur; /* file being updated or NULL */
	struct blkstat	 stat; /* status of file being updated */
	int		 primed; /* blk_recv_ack() was called */
};

TAILQ_HEAD(send_dlq, send_dl);

/*
 * We have finished updating the receiver's file with sender data.
 * Deallocate and wipe clean all resources required for that.
 */
static void
send_up_reset(struct send_up *p)
{

	assert(NULL != p);

	/* Free the download request, if applicable. */

	if (p->cur != NULL) {
		free(p->cur->blks);
		free(p->cur);
		p->cur = NULL;
	}

	/* If we mapped a file for scanning, unmap it and close. */

	if (p->stat.map != MAP_FAILED)
		munmap(p->stat.map, p->stat.mapsz);

	p->stat.map = MAP_FAILED;
	p->stat.mapsz = 0;

	if (p->stat.fd != -1)
		close(p->stat.fd);

	p->stat.fd = -1;

	/* Now clear the in-transfer information. */

	p->stat.offs = 0;
	p->stat.hint = 0;
	p->primed = 0;
}

/*
 * Enqueue a download request, getting it off the read channel as
 * quickly a possible.
 * This frees up the read channel for further incoming requests.
 * We'll handle each element in turn, up to and including the last
 * request (phase change), which is always a -1 idx.
 * Returns zero on failure, non-zero on success.
 */
static int
send_dl_enqueue(struct sess *sess, struct send_dlq *q,
	int32_t idx, const struct flist *fl, size_t flsz, int fd)
{
	struct send_dl	*s;
	
	/* End-of-phase marker. */

	if (idx == -1) {
		if ((s = calloc(1, sizeof(struct send_dl))) == NULL) {
			ERR(sess, "calloc");
			return 0;
		}
		s->idx = -1;
		s->blks = NULL;
		TAILQ_INSERT_TAIL(q, s, entries);
		return 1;
	}

	/* Validate the index. */
		
	if (idx < 0 || (uint32_t)idx >= flsz) {
		ERRX(sess, "file index out of bounds: invalid %" 
			PRId32 " out of %zu", idx, flsz);
		return 0;
	} else if (S_ISDIR(fl[idx].st.mode)) {
		ERRX(sess, "blocks requested for "
			"directory: %s", fl[idx].path);
		return 0;
	} else if (S_ISLNK(fl[idx].st.mode)) {
		ERRX(sess, "blocks requested for "
			"symlink: %s", fl[idx].path);
		return 0;
	} else if (!S_ISREG(fl[idx].st.mode)) {
		ERRX(sess, "blocks requested for "
			"special: %s", fl[idx].path);
		return 0;
	}

	if ((s = calloc(1, sizeof(struct send_dl))) == NULL) {
		ERR(sess, "callloc");
		return 0;
	}
	s->idx = idx;
	s->blks = NULL;
	TAILQ_INSERT_TAIL(q, s, entries);

	/* 
	 * This blocks til the full blockset has been read.
	 * That's ok, because the most important thing is getting data
	 * off the wire.
	 */

	if (!sess->opts->dry_run) {
		s->blks = blk_recv(sess, fd, fl[idx].path);
		if (s->blks == NULL) {
			ERRX1(sess, "blk_recv");
			return 0;
		}
	}
	return 1;
}

/*
 * A client sender manages the read-only source files and sends data to
 * the receiver as requested.
 * First it sends its list of files, then it waits for the server to
 * request updates to individual files.
 * It queues requests for updates as soon as it receives them.
 * Returns zero on failure, non-zero on success.
 *
 * Pledges: stdio, rpath, unveil.
 */
int
rsync_sender(struct sess *sess, int fdin,
	int fdout, size_t argc, char **argv)
{
	struct flist	*fl = NULL;
	size_t		 i, flsz = 0, phase = 0, excl;
	int		 rc = 0, c;
	int32_t		 idx;
	struct pollfd	 pfd[3];
	struct send_dlq	 sdlq;
	struct send_up	 up;
	struct stat	 st;

	if (pledge("stdio getpw rpath unveil", NULL) == -1) {
		ERR(sess, "pledge");
		return 0;
	}

	memset(&up, 0, sizeof(struct send_up));
	TAILQ_INIT(&sdlq);
	up.stat.fd = -1;
	up.stat.map = MAP_FAILED;

	/*
	 * Generate the list of files we want to send from our
	 * command-line input.
	 * This will also remove all invalid files.
	 */

	if (!flist_gen(sess, argc, argv, &fl, &flsz)) {
		ERRX1(sess, "flist_gen");
		goto out;
	}

	/* Client sends zero-length exclusions if deleting. */

	if (!sess->opts->server && sess->opts->del &&
	    !io_write_int(sess, fdout, 0)) {
		ERRX1(sess, "io_write_int");
		goto out;
	}

	/*
	 * Then the file list in any mode.
	 * Finally, the IO error (always zero for us).
	 */

	if (!flist_send(sess, fdin, fdout, fl, flsz)) {
		ERRX1(sess, "flist_send");
		goto out;
	} else if (!io_write_int(sess, fdout, 0)) {
		ERRX1(sess, "io_write_int");
		goto out;
	}

	/* Exit if we're the server with zero files. */

	if (flsz == 0 && sess->opts->server) {
		WARNX(sess, "sender has empty file list: exiting");
		rc = 1;
		goto out;
	} else if (!sess->opts->server)
		LOG1(sess, "Transfer starting: %zu files", flsz);

	/*
	 * If we're the server, read our exclusion list.
	 * This is always 0 for now.
	 */

	if (sess->opts->server) {
		if (!io_read_size(sess, fdin, &excl)) {
			ERRX1(sess, "io_read_size");
			goto out;
		} else if (excl != 0) {
			ERRX1(sess, "exclusion list is non-empty");
			goto out;
		}
	}

	/* 
	 * Set up our poll events.
	 * We start by polling only in receiver requests, enabling other
	 * poll events on demand.
	 */

	pfd[0].fd = fdin; /* from receiver */
	pfd[0].events = POLLIN;
	pfd[1].fd = -1; /* to receiver */
	pfd[1].events = POLLOUT;
	pfd[2].fd = -1; /* from local file */
	pfd[2].events = POLLIN;

	/* The main sender loop runs into phase == 2. */

	for (;;) {
		assert(pfd[0].fd != -1);
		if ((c = poll(pfd, 3, POLL_TIMEOUT)) == -1) {
			ERR(sess, "poll");
			goto out;
		} else if (c == 0) {
			ERRX(sess, "poll: timeout");
			goto out;
		}

		for (i = 0; i < 3; i++)
			if (pfd[i].revents & (POLLERR|POLLNVAL)) {
				ERRX(sess, "poll: bad fd");
				goto out;
			} else if (pfd[i].revents & POLLHUP) {
				ERRX(sess, "poll: hangup");
				goto out;
			}

		/* 
		 * Flush out multiplexed messages.
		 * These might otherwise clog the reader.
		 */

		if (sess->mplex_reads &&
		    (POLLIN & pfd[0].revents)) {
			if (!io_read_flush(sess, fdin)) {
				ERRX1(sess, "io_read_flush");
				goto out;
			} else if (sess->mplex_read_remain == 0)
				pfd[0].revents &= ~POLLIN;
		}

		/*
		 * If we have a request coming down off the wire, pull
		 * it in as quickly as possible into our buffer.
		 * This unclogs the socket buffers so the data can flow.
		 */

		if (pfd[0].revents & POLLIN)
			for (;;) {
				if (!io_read_int(sess, fdin, &idx)) {
					ERRX1(sess, "io_read_int");
					goto out;
				} 
				if (!send_dl_enqueue(sess,
				    &sdlq, idx, fl, flsz, fdin)) {
					ERRX1(sess, "send_dl_enqueue");
					goto out;
				}
				c = io_read_check(sess, fdin);
				if (c < 0) {
					ERRX1(sess, "io_read_check");
					goto out;
				} else if (c == 0)
					break;
			}

		/*
		 * One of our local files has been opened (in response
		 * to a receiver request) and now we can map it.
		 * We'll respond to the event by looking at the map when
		 * the writer is available.
		 * Here we also enable the poll event for output.
		 */

		if (pfd[2].revents & POLLIN) {
			assert(up.cur != NULL);
			assert(up.stat.fd != -1);
			assert(up.stat.map == MAP_FAILED);
			assert(up.stat.mapsz == 0);

			if (fstat(up.stat.fd, &st) == -1) {
				ERR(sess, "%s: fstat", fl[up.cur->idx].path);
				goto out;
			}

			/*
			 * If the file is zero-length, the map will
			 * fail, but either way we want to unset that
			 * we're waiting for the file to open.
			 * We'll close the descriptor after processing.
			 */

			if ((up.stat.mapsz = st.st_size) > 0) {
				up.stat.map = mmap(NULL, up.stat.mapsz, 
					PROT_READ, MAP_SHARED, up.stat.fd, 0);
				if (up.stat.map == MAP_FAILED) {
					ERR(sess, "%s: mmap", fl[up.cur->idx].path);
					goto out;
				}
			} 
			pfd[2].fd = -1;
			pfd[1].fd = fdout;
		}

		/*
		 * Our outbound is ready to process the current event.
		 * This means we've already opened the file and possibly
		 * mapped it, and we're ready to send blocks.
		 * Do this one block at a time lest we block the channel
		 * while read events are coming in.
		 */

		if (pfd[1].revents & POLLOUT) {
			assert(up.cur != NULL);
			assert(pfd[2].fd == -1);

			/*
			 * If we receive an invalid index (-1), then we're
			 * either promoted to the second phase or it's time to
			 * exit, depending upon which phase we're in.
			 * Otherwise, we either start a transfer
			 * sequence (if not primed) or continue one.
			 */

			if (up.cur->idx < 0) {
				pfd[1].fd = -1;
				send_up_reset(&up);
				if (!io_write_int(sess, fdout, -1)) {
					ERRX1(sess, "io_write_int");
					goto out;
				}

				/* Send superfluous ack. */

				if (sess->opts->server && sess->rver > 27 &&
				    !io_write_int(sess, fdout, -1)) {
					ERRX1(sess, "io_write_int");
					goto out;
				}

				if (phase++)
					break;
			} else if (0 == up.primed) {
				if (!sess->opts->server)
					LOG1(sess, "%s", fl[up.cur->idx].wpath);

				/* Dry-running does nothing but a response. */

				if (sess->opts->dry_run &&
				    !io_write_int(sess, fdout, up.cur->idx)) {
					ERRX1(sess, "io_write_int");
					goto out;
				}

				/* Actually perform the block send. */

				assert(up.stat.fd != -1);
				if (!blk_recv_ack(sess, fdout,
				    up.cur->blks, up.cur->idx)) {
					ERRX1(sess, "blk_recv_ack");
					goto out;
				} 
				up.primed = 1;
			} else {
				assert(up.stat.fd != -1);
				c = blk_match(sess, fdout, up.cur->blks, 
					fl[up.cur->idx].path, &up.stat);
				if (c < 0) {
					ERRX1(sess, "blk_match");
					goto out;
				} else if (c > 0) {
					send_up_reset(&up);
					pfd[1].fd = -1;
				}
			}
		}

		/*
		 * Incoming queue management.
		 * If we have no queue component that we're waiting on,
		 * then pull off the receiver-request queue and start
		 * processing the request.
		 */

		if (up.cur == NULL) {
			assert(pfd[2].fd == -1);
			assert(up.stat.fd == -1);
			assert(up.stat.map == MAP_FAILED);
			assert(up.stat.mapsz == 0);

			if ((up.cur = TAILQ_FIRST(&sdlq)) == NULL)
				continue;
			TAILQ_REMOVE(&sdlq, up.cur, entries);

			/* End of phase: enable channel to receiver. */

			if (up.cur->idx == -1) {
				pfd[1].fd = fdout;
				continue;
			}
			
			/* Non-blocking open of file. */

			up.stat.fd = open(fl[up.cur->idx].path, 
				O_RDONLY|O_NONBLOCK, 0);
			if (up.stat.fd == -1) {
				ERR(sess, "%s: open", fl[up.cur->idx].path);
				goto out;
			}
			pfd[2].fd = up.stat.fd;
		}
	}

	if (!sess_stats_send(sess, fdout)) {
		ERRX1(sess, "sess_stats_end");
		goto out;
	}

	/* Final "goodbye" message. */

	if (!io_read_int(sess, fdin, &idx)) {
		ERRX1(sess, "io_read_int");
		goto out;
	} else if (idx != -1) {
		ERRX(sess, "read incorrect update complete ack");
		goto out;
	}

	LOG2(sess, "sender finished updating");
	rc = 1;
out:
	flist_free(fl, flsz);
	return rc;
}
