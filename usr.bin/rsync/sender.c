/*	$OpenBSD: sender.c,v 1.30 2021/08/29 13:43:46 claudio Exp $ */
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

#include <openssl/md4.h>

#include "extern.h"

/*
 * A request from the receiver to download updated file data.
 */
struct	send_dl {
	int32_t			 idx; /* index in our file list */
	struct blkset		*blks; /* the sender's block information */
	TAILQ_ENTRY(send_dl)	 entries;
};

/*
 * The current file being "updated": sent from sender to receiver.
 * If there is no file being uploaded, "cur" is NULL.
 */
struct	send_up {
	struct send_dl	*cur; /* file being updated or NULL */
	struct blkstat	 stat; /* status of file being updated */
};

TAILQ_HEAD(send_dlq, send_dl);

/*
 * We have finished updating the receiver's file with sender data.
 * Deallocate and wipe clean all resources required for that.
 */
static void
send_up_reset(struct send_up *p)
{

	assert(p != NULL);

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
	p->stat.curst = BLKSTAT_NONE;
}

/*
 * This is the bulk of the sender work.
 * Here we tend to an output buffer that responds to receiver requests
 * for data.
 * This does not act upon the output descriptor itself so as to avoid
 * blocking, which otherwise would deadlock the protocol.
 * Returns zero on failure, non-zero on success.
 */
static int
send_up_fsm(struct sess *sess, size_t *phase,
	struct send_up *up, void **wb, size_t *wbsz, size_t *wbmax,
	const struct flist *fl)
{
	size_t		 pos = 0, isz = sizeof(int32_t),
			 dsz = MD4_DIGEST_LENGTH;
	unsigned char	 fmd[MD4_DIGEST_LENGTH];
	off_t		 sz;
	char		 buf[20];

	switch (up->stat.curst) {
	case BLKSTAT_DATA:
		/*
		 * A data segment to be written: buffer both the length
		 * and the data.
		 * If we've finished the transfer, move on to the token;
		 * otherwise, keep sending data.
		 */

		sz = MINIMUM(MAX_CHUNK,
			up->stat.curlen - up->stat.curpos);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, isz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_int(sess, *wb, &pos, *wbsz, sz);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, sz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_buf(sess, *wb, &pos, *wbsz,
			up->stat.map + up->stat.curpos, sz);

		up->stat.curpos += sz;
		if (up->stat.curpos == up->stat.curlen)
			up->stat.curst = BLKSTAT_TOK;
		return 1;
	case BLKSTAT_TOK:
		/*
		 * The data token following (maybe) a data segment.
		 * These can also come standalone if, say, the file's
		 * being fully written.
		 * It's followed by a hash or another data segment,
		 * depending on the token.
		 */

		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, isz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_int(sess, *wb,
			&pos, *wbsz, up->stat.curtok);
		up->stat.curst = up->stat.curtok ?
			BLKSTAT_NEXT : BLKSTAT_HASH;
		return 1;
	case BLKSTAT_HASH:
		/*
		 * The hash following transmission of all file contents.
		 * This is always followed by the state that we're
		 * finished with the file.
		 */

		hash_file(up->stat.map, up->stat.mapsz, fmd, sess);
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, dsz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_buf(sess, *wb, &pos, *wbsz, fmd, dsz);
		up->stat.curst = BLKSTAT_DONE;
		return 1;
	case BLKSTAT_DONE:
		/*
		 * The data has been written.
		 * Clear our current send file and allow the block below
		 * to find another.
		 */

		if (!sess->opts->dry_run)
			LOG3("%s: flushed %jd KB total, %.2f%% uploaded",
			    fl[up->cur->idx].path,
			    (intmax_t)up->stat.total / 1024,
			    100.0 * up->stat.dirty / up->stat.total);
		send_up_reset(up);
		return 1;
	case BLKSTAT_PHASE:
		/*
		 * This is where we actually stop the algorithm: we're
		 * already at the second phase.
		 */

		send_up_reset(up);
		(*phase)++;
		return 1;
	case BLKSTAT_NEXT:
		/*
		 * Our last case: we need to find the
		 * next block (and token) to transmit to
		 * the receiver.
		 * These will drive the finite state
		 * machine in the first few conditional
		 * blocks of this set.
		 */

		assert(up->stat.fd != -1);
		blk_match(sess, up->cur->blks,
			fl[up->cur->idx].path, &up->stat);
		return 1;
	case BLKSTAT_NONE:
		break;
	}

	assert(BLKSTAT_NONE == up->stat.curst);

	/*
	 * We've either hit the phase change following the last file (or
	 * start, or prior phase change), or we need to prime the next
	 * file for transmission.
	 * We special-case dry-run mode.
	 */

	if (up->cur->idx < 0) {
		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, isz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_int(sess, *wb, &pos, *wbsz, -1);

		if (sess->opts->server && sess->rver > 27) {
			if (!io_lowbuffer_alloc(sess,
			    wb, wbsz, wbmax, isz)) {
				ERRX1("io_lowbuffer_alloc");
				return 0;
			}
			io_lowbuffer_int(sess, *wb, &pos, *wbsz, -1);
		}
		up->stat.curst = BLKSTAT_PHASE;
	} else if (sess->opts->dry_run) {
		if (!sess->opts->server)
			LOG1("%s", fl[up->cur->idx].wpath);

		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, isz)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		io_lowbuffer_int(sess, *wb, &pos, *wbsz, up->cur->idx);
		up->stat.curst = BLKSTAT_DONE;
	} else {
		assert(up->stat.fd != -1);

		/*
		 * FIXME: use the nice output of log_file() and so on in
		 * downloader.c, which means moving this into
		 * BLKSTAT_DONE instead of having it be here.
		 */

		if (!sess->opts->server)
			LOG1("%s", fl[up->cur->idx].wpath);

		if (!io_lowbuffer_alloc(sess, wb, wbsz, wbmax, 20)) {
			ERRX1("io_lowbuffer_alloc");
			return 0;
		}
		assert(sizeof(buf) == 20);
		blk_recv_ack(buf, up->cur->blks, up->cur->idx);
		io_lowbuffer_buf(sess, *wb, &pos, *wbsz, buf, 20);

		LOG3("%s: primed for %jd B total",
		    fl[up->cur->idx].path, (intmax_t)up->cur->blks->size);
		up->stat.curst = BLKSTAT_NEXT;
	}

	return 1;
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
			ERR("calloc");
			return 0;
		}
		s->idx = -1;
		s->blks = NULL;
		TAILQ_INSERT_TAIL(q, s, entries);
		return 1;
	}

	/* Validate the index. */

	if (idx < 0 || (uint32_t)idx >= flsz) {
		ERRX("file index out of bounds: invalid %d out of %zu",
		    idx, flsz);
		return 0;
	} else if (S_ISDIR(fl[idx].st.mode)) {
		ERRX("blocks requested for "
			"directory: %s", fl[idx].path);
		return 0;
	} else if (S_ISLNK(fl[idx].st.mode)) {
		ERRX("blocks requested for "
			"symlink: %s", fl[idx].path);
		return 0;
	} else if (!S_ISREG(fl[idx].st.mode)) {
		ERRX("blocks requested for "
			"special: %s", fl[idx].path);
		return 0;
	}

	if ((s = calloc(1, sizeof(struct send_dl))) == NULL) {
		ERR("callloc");
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
			ERRX1("blk_recv");
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
 * Pledges: stdio, getpw, rpath.
 */
int
rsync_sender(struct sess *sess, int fdin,
	int fdout, size_t argc, char **argv)
{
	struct flist	   *fl = NULL;
	const struct flist *f;
	size_t		    i, flsz = 0, phase = 0;
	int		    rc = 0, c;
	int32_t		    idx;
	struct pollfd	    pfd[3];
	struct send_dlq	    sdlq;
	struct send_dl	   *dl;
	struct send_up	    up;
	struct stat	    st;
	void		   *wbuf = NULL;
	size_t		    wbufpos = 0, wbufsz = 0, wbufmax = 0;
	ssize_t		    ssz;

	if (pledge("stdio getpw rpath", NULL) == -1) {
		ERR("pledge");
		return 0;
	}

	memset(&up, 0, sizeof(struct send_up));
	TAILQ_INIT(&sdlq);
	up.stat.fd = -1;
	up.stat.map = MAP_FAILED;
	up.stat.blktab = blkhash_alloc();

	/*
	 * Generate the list of files we want to send from our
	 * command-line input.
	 * This will also remove all invalid files.
	 */

	if (!flist_gen(sess, argc, argv, &fl, &flsz)) {
		ERRX1("flist_gen");
		goto out;
	}

	/* Client sends zero-length exclusions if deleting. */
	if (!sess->opts->server && sess->opts->del)
		send_rules(sess, fdout);

	/*
	 * Then the file list in any mode.
	 * Finally, the IO error (always zero for us).
	 */

	if (!flist_send(sess, fdin, fdout, fl, flsz)) {
		ERRX1("flist_send");
		goto out;
	} else if (!io_write_int(sess, fdout, 0)) {
		ERRX1("io_write_int");
		goto out;
	}

	/* Exit if we're the server with zero files. */

	if (flsz == 0 && sess->opts->server) {
		WARNX("sender has empty file list: exiting");
		rc = 1;
		goto out;
	} else if (!sess->opts->server)
		LOG1("Transfer starting: %zu files", flsz);

	/*
	 * If we're the server, read our exclusion list.
	 * This is always 0 for now.
	 */

	if (sess->opts->server)
		recv_rules(sess, fdin);

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

	for (;;) {
		assert(pfd[0].fd != -1);
		if ((c = poll(pfd, 3, poll_timeout)) == -1) {
			ERR("poll");
			goto out;
		} else if (c == 0) {
			ERRX("poll: timeout");
			goto out;
		}
		for (i = 0; i < 3; i++)
			if (pfd[i].revents & (POLLERR|POLLNVAL)) {
				ERRX("poll: bad fd");
				goto out;
			} else if (pfd[i].revents & POLLHUP) {
				ERRX("poll: hangup");
				goto out;
			}

		/*
		 * If we have a request coming down off the wire, pull
		 * it in as quickly as possible into our buffer.
		 * Start by seeing if we have a log message.
		 * If we do, pop it off, then see if we have anything
		 * left and hit it again if so (read priority).
		 */

		if (sess->mplex_reads && (pfd[0].revents & POLLIN)) {
			if (!io_read_flush(sess, fdin)) {
				ERRX1("io_read_flush");
				goto out;
			} else if (sess->mplex_read_remain == 0) {
				c = io_read_check(fdin);
				if (c < 0) {
					ERRX1("io_read_check");
					goto out;
				} else if (c > 0)
					continue;
				pfd[0].revents &= ~POLLIN;
			}
		}

		/*
		 * Now that we've handled the log messages, we're left
		 * here if we have any actual data coming down.
		 * Enqueue message requests, then loop again if we see
		 * more data (read priority).
		 */

		if (pfd[0].revents & POLLIN) {
			if (!io_read_int(sess, fdin, &idx)) {
				ERRX1("io_read_int");
				goto out;
			}
			if (!send_dl_enqueue(sess,
			    &sdlq, idx, fl, flsz, fdin)) {
				ERRX1("send_dl_enqueue");
				goto out;
			}
			c = io_read_check(fdin);
			if (c < 0) {
				ERRX1("io_read_check");
				goto out;
			} else if (c > 0)
				continue;
		}

		/*
		 * One of our local files has been opened in response
		 * to a receiver request and now we can map it.
		 * We'll respond to the event by looking at the map when
		 * the writer is available.
		 * Here we also enable the poll event for output.
		 */

		if (pfd[2].revents & POLLIN) {
			assert(up.cur != NULL);
			assert(up.stat.fd != -1);
			assert(up.stat.map == MAP_FAILED);
			assert(up.stat.mapsz == 0);
			f = &fl[up.cur->idx];

			if (fstat(up.stat.fd, &st) == -1) {
				ERR("%s: fstat", f->path);
				goto out;
			}

			/*
			 * If the file is zero-length, the map will
			 * fail, but either way we want to unset that
			 * we're waiting for the file to open and set
			 * that we're ready for the output channel.
			 */

			if ((up.stat.mapsz = st.st_size) > 0) {
				up.stat.map = mmap(NULL,
					up.stat.mapsz, PROT_READ,
					MAP_SHARED, up.stat.fd, 0);
				if (up.stat.map == MAP_FAILED) {
					ERR("%s: mmap", f->path);
					goto out;
				}
			}

			pfd[2].fd = -1;
			pfd[1].fd = fdout;
		}

		/*
		 * If we have buffers waiting to write, write them out
		 * as soon as we can in a non-blocking fashion.
		 * We must not be waiting for any local files.
		 * ALL WRITES MUST HAPPEN HERE.
		 * This keeps the sender deadlock-free.
		 */

		if ((pfd[1].revents & POLLOUT) && wbufsz > 0) {
			assert(pfd[2].fd == -1);
			assert(wbufsz - wbufpos);
			ssz = write(fdout, wbuf + wbufpos, wbufsz - wbufpos);
			if (ssz == -1) {
				ERR("write");
				goto out;
			}
			wbufpos += ssz;
			if (wbufpos == wbufsz)
				wbufpos = wbufsz = 0;
			pfd[1].revents &= ~POLLOUT;

			/* This is usually in io.c... */

			sess->total_write += ssz;
		}

		/*
		 * Engage the FSM for the current transfer.
		 * If our phase changes, stop processing.
		 */

		if (pfd[1].revents & POLLOUT && up.cur != NULL) {
			assert(pfd[2].fd == -1);
			assert(wbufpos == 0 && wbufsz == 0);
			if (!send_up_fsm(sess, &phase,
			    &up, &wbuf, &wbufsz, &wbufmax, fl)) {
				ERRX1("send_up_fsm");
				goto out;
			} else if (phase > 1)
				break;
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
			assert(wbufsz == 0 && wbufpos == 0);
			pfd[1].fd = -1;

			/*
			 * If there's nothing in the queue, then keep
			 * the output channel disabled and wait for
			 * whatever comes next from the reader.
			 */

			if ((up.cur = TAILQ_FIRST(&sdlq)) == NULL)
				continue;
			TAILQ_REMOVE(&sdlq, up.cur, entries);

			/* Hash our blocks. */

			blkhash_set(up.stat.blktab, up.cur->blks);

			/*
			 * End of phase: enable channel to receiver.
			 * We'll need our output buffer enabled in order
			 * to process this event.
			 */

			if (up.cur->idx == -1) {
				pfd[1].fd = fdout;
				continue;
			}

			/*
			 * Non-blocking open of file.
			 * This will be picked up in the state machine
			 * block of not being primed.
			 */

			up.stat.fd = open(fl[up.cur->idx].path,
				O_RDONLY|O_NONBLOCK, 0);
			if (up.stat.fd == -1) {
				ERR("%s: open", fl[up.cur->idx].path);
				goto out;
			}
			pfd[2].fd = up.stat.fd;
		}
	}

	if (!TAILQ_EMPTY(&sdlq)) {
		ERRX("phases complete with files still queued");
		goto out;
	}

	if (!sess_stats_send(sess, fdout)) {
		ERRX1("sess_stats_end");
		goto out;
	}

	/* Final "goodbye" message. */

	if (!io_read_int(sess, fdin, &idx)) {
		ERRX1("io_read_int");
		goto out;
	} else if (idx != -1) {
		ERRX("read incorrect update complete ack");
		goto out;
	}

	LOG2("sender finished updating");
	rc = 1;
out:
	send_up_reset(&up);
	while ((dl = TAILQ_FIRST(&sdlq)) != NULL) {
		TAILQ_REMOVE(&sdlq, dl, entries);
		free(dl->blks);
		free(dl);
	}
	flist_free(fl, flsz);
	free(wbuf);
	blkhash_free(up.stat.blktab);
	return rc;
}
