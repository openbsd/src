/*	$Id: io.c,v 1.12 2019/02/18 22:47:34 benno Exp $ */
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
#include <endian.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * A non-blocking check to see whether there's POLLIN data in fd.
 * Returns <0 on failure, 0 if there's no data, >0 if there is.
 */
int
io_read_check(struct sess *sess, int fd)
{
	struct pollfd	pfd;

	pfd.fd = fd;
	pfd.events = POLLIN;

	if (poll(&pfd, 1, 0) < 0) {
		ERR(sess, "poll");
		return -1;
	}
	return (pfd.revents & POLLIN);
}

/*
 * Write buffer to non-blocking descriptor.
 * Returns zero on failure, non-zero on success (zero or more bytes).
 * On success, fills in "sz" with the amount written.
 */
static int
io_write_nonblocking(struct sess *sess, int fd, const void *buf, size_t bsz,
    size_t *sz)
{
	struct pollfd	pfd;
	ssize_t		wsz;
	int		c;

	*sz = 0;

	if (bsz == 0)
		return 1;

	pfd.fd = fd;
	pfd.events = POLLOUT;

	/* Poll and check for all possible errors. */

	if ((c = poll(&pfd, 1, POLL_TIMEOUT)) == -1) {
		ERR(sess, "poll");
		return 0;
	} else if (c == 0) {
		ERRX(sess, "poll: timeout");
		return 0;
	} else if ((pfd.revents & (POLLERR|POLLNVAL))) {
		ERRX(sess, "poll: bad fd");
		return 0;
	} else if ((pfd.revents & POLLHUP)) {
		ERRX(sess, "poll: hangup");
		return 0;
	} else if (!(pfd.revents & POLLOUT)) {
		ERRX(sess, "poll: unknown event");
		return 0;
	}

	/* Now the non-blocking write. */

	if ((wsz = write(fd, buf, bsz)) < 0) {
		ERR(sess, "write");
		return 0;
	}

	*sz = wsz;
	return 1;
}

/*
 * Blocking write of the full size of the buffer.
 * Returns 0 on failure, non-zero on success (all bytes written).
 */
static int
io_write_blocking(struct sess *sess, int fd, const void *buf, size_t sz)
{
	size_t		wsz;
	int		c;

	while (sz > 0) {
		c = io_write_nonblocking(sess, fd, buf, sz, &wsz);
		if (!c) {
			ERRX1(sess, "io_write_nonblocking");
			return 0;
		} else if (wsz == 0) {
			ERRX(sess, "io_write_nonblocking: short write");
			return 0;
		}
		buf += wsz;
		sz -= wsz;
	}

	return 1;
}

/*
 * Write "buf" of size "sz" to non-blocking descriptor.
 * Returns zero on failure, non-zero on success (all bytes written to
 * the descriptor).
 */
int
io_write_buf(struct sess *sess, int fd, const void *buf, size_t sz)
{
	int32_t	 tag, tagbuf;
	size_t	 wsz;
	int	 c;

	if (!sess->mplex_writes) {
		c = io_write_blocking(sess, fd, buf, sz);
		sess->total_write += sz;
		return c;
	}

	while (sz > 0) {
		wsz = sz & 0xFFFFFF;
		tag = (7 << 24) + wsz;
		tagbuf = htole32(tag);
		if (!io_write_blocking(sess, fd, &tagbuf, sizeof(tagbuf))) {
			ERRX1(sess, "io_write_blocking");
			return 0;
		}
		if (!io_write_blocking(sess, fd, buf, wsz)) {
			ERRX1(sess, "io_write_blocking");
			return 0;
		}
		sess->total_write += wsz;
		sz -= wsz;
		buf += wsz;
	}

	return 1;
}

/*
 * Write "line" (NUL-terminated) followed by a newline.
 * Returns zero on failure, non-zero on succcess.
 */
int
io_write_line(struct sess *sess, int fd, const char *line)
{

	if (!io_write_buf(sess, fd, line, strlen(line)))
		ERRX1(sess, "io_write_buf");
	else if (!io_write_byte(sess, fd, '\n'))
		ERRX1(sess, "io_write_byte");
	else
		return 1;

	return 0;
}

/*
 * Read buffer from non-blocking descriptor.
 * Returns zero on failure, non-zero on success (zero or more bytes).
 */
static int
io_read_nonblocking(struct sess *sess,
	int fd, void *buf, size_t bsz, size_t *sz)
{
	struct pollfd	pfd;
	ssize_t		rsz;
	int		c;

	*sz = 0;

	if (bsz == 0)
		return 1;

	pfd.fd = fd;
	pfd.events = POLLIN;

	/* Poll and check for all possible errors. */

	if ((c = poll(&pfd, 1, POLL_TIMEOUT)) == -1) {
		ERR(sess, "poll");
		return 0;
	} else if (c == 0) {
		ERRX(sess, "poll: timeout");
		return 0;
	} else if ((pfd.revents & (POLLERR|POLLNVAL))) {
		ERRX(sess, "poll: bad fd");
		return 0;
	} else if (!(pfd.revents & (POLLIN|POLLHUP))) {
		ERRX(sess, "poll: unknown event");
		return 0;
	}

	/* Now the non-blocking read, checking for EOF. */

	if ((rsz = read(fd, buf, bsz)) < 0) {
		ERR(sess, "read");
		return 0;
	} else if (rsz == 0) {
		ERRX(sess, "unexpected end of file");
		return 0;
	}

	*sz = rsz;
	return 1;
}

/*
 * Blocking read of the full size of the buffer.
 * This can be called from either the error type message or a regular
 * message---or for that matter, multiplexed or not.
 * Returns 0 on failure, non-zero on success (all bytes read).
 */
static int
io_read_blocking(struct sess *sess,
	int fd, void *buf, size_t sz)
{
	size_t	 rsz;
	int	 c;

	while (sz > 0) {
		c = io_read_nonblocking(sess, fd, buf, sz, &rsz);
		if (!c) {
			ERRX1(sess, "io_read_nonblocking");
			return 0;
		} else if (rsz == 0) {
			ERRX(sess, "io_read_nonblocking: short read");
			return 0;
		}
		buf += rsz;
		sz -= rsz;
	}

	return 1;
}

/*
 * When we do a lot of writes in a row (such as when the sender emits
 * the file list), the server might be sending us multiplexed log
 * messages.
 * If it sends too many, it clogs the socket.
 * This function looks into the read buffer and clears out any log
 * messages pending.
 * If called when there are valid data reads available, this function
 * does nothing.
 * Returns zero on failure, non-zero on success.
 */
int
io_read_flush(struct sess *sess, int fd)
{
	int32_t	 tagbuf, tag;
	char	 mpbuf[1024];

	if (sess->mplex_read_remain)
		return 1;

	/*
	 * First, read the 4-byte multiplex tag.
	 * The first byte is the tag identifier (7 for normal
	 * data, !7 for out-of-band data), the last three are
	 * for the remaining data size.
	 */

	if (!io_read_blocking(sess, fd, &tagbuf, sizeof(tagbuf))) {
		ERRX1(sess, "io_read_blocking");
		return 0;
	}
	tag = le32toh(tagbuf);
	sess->mplex_read_remain = tag & 0xFFFFFF;
	tag >>= 24;
	if (tag == 7)
		return 1;

	tag -= 7;

	if (sess->mplex_read_remain > sizeof(mpbuf)) {
		ERRX(sess, "multiplex buffer overflow");
		return 0;
	} else if (sess->mplex_read_remain == 0)
		return 1;

	if (!io_read_blocking(sess, fd, mpbuf, sess->mplex_read_remain)) {
		ERRX1(sess, "io_read_blocking");
		return 0;
	}
	if (mpbuf[sess->mplex_read_remain - 1] == '\n')
		mpbuf[--sess->mplex_read_remain] = '\0';

	/*
	 * Always print the server's messages, as the server
	 * will control its own log levelling.
	 */

	LOG0(sess, "%.*s", (int)sess->mplex_read_remain, mpbuf);
	sess->mplex_read_remain = 0;

	/*
	 * I only know that a tag of one means an error.
	 * This means that we should exit.
	 */

	if (tag == 1) {
		ERRX1(sess, "error from remote host");
		return 0;
	}
	return 1;
}

/*
 * Read buffer from non-blocking descriptor, possibly in multiplex read
 * mode.
 * Returns zero on failure, non-zero on success (all bytes read from
 * the descriptor).
 */
int
io_read_buf(struct sess *sess, int fd, void *buf, size_t sz)
{
	size_t	 rsz;
	int	 c;

	/* If we're not multiplexing, read directly. */

	if (!sess->mplex_reads) {
		assert(sess->mplex_read_remain == 0);
		c = io_read_blocking(sess, fd, buf, sz);
		sess->total_read += sz;
		return c;
	}

	while (sz > 0) {
		/*
		 * First, check to see if we have any regular data
		 * hanging around waiting to be read.
		 * If so, read the lesser of that data and whatever
		 * amount we currently want.
		 */

		if (sess->mplex_read_remain) {
			rsz = sess->mplex_read_remain < sz ?
				sess->mplex_read_remain : sz;
			if (!io_read_blocking(sess, fd, buf, rsz)) {
				ERRX1(sess, "io_read_blocking");
				return 0;
			}
			sz -= rsz;
			sess->mplex_read_remain -= rsz;
			buf += rsz;
			sess->total_read += rsz;
			continue;
		}

		assert(sess->mplex_read_remain == 0);
		if (!io_read_flush(sess, fd)) {
			ERRX1(sess, "io_read_flush");
			return 0;
		}
	}

	return 1;
}

/*
 * Like io_write_buf(), but for a long (which is a composite type).
 * Returns zero on failure, non-zero on success.
 */
int
io_write_long(struct sess *sess, int fd, int64_t val)
{
	int64_t	nv;

	/* Short-circuit: send as an integer if possible. */

	if (val <= INT32_MAX && val >= 0) {
		if (!io_write_int(sess, fd, (int32_t)val)) {
			ERRX1(sess, "io_write_int");
			return 0;
		}
		return 1;
	}

	/* Otherwise, pad with max integer, then send 64-bit. */

	nv = htole64(val);

	if (!io_write_int(sess, fd, INT32_MAX))
		ERRX1(sess, "io_write_int");
	else if (!io_write_buf(sess, fd, &nv, sizeof(int64_t)))
		ERRX1(sess, "io_write_buf");
	else
		return 1;

	return 0;
}

/*
 * Like io_write_buf(), but for an integer.
 * Returns zero on failure, non-zero on success.
 */
int
io_write_int(struct sess *sess, int fd, int32_t val)
{
	int32_t	nv;

	nv = htole32(val);

	if (!io_write_buf(sess, fd, &nv, sizeof(int32_t))) {
		ERRX1(sess, "io_write_buf");
		return 0;
	}
	return 1;
}

/*
 * A simple assertion-protected memory copy from th einput "val" or size
 * "valsz" into our buffer "buf", full size "buflen", position "bufpos".
 * Increases our "bufpos" appropriately.
 * This has no return value, but will assert() if the size of the buffer
 * is insufficient for the new data.
 */
void
io_buffer_buf(struct sess *sess, void *buf,
	size_t *bufpos, size_t buflen, const void *val, size_t valsz)
{

	assert(*bufpos + valsz <= buflen);
	memcpy(buf + *bufpos, val, valsz);
	*bufpos += valsz;
}

/*
 * Like io_buffer_buf(), but also accomodating for multiplexing codes.
 * This should NEVER be passed to io_write_buf(), but instead passed
 * directly to a write operation.
 */
void
io_lowbuffer_buf(struct sess *sess, void *buf,
	size_t *bufpos, size_t buflen, const void *val, size_t valsz)
{
	int32_t	tagbuf;

	if (valsz == 0)
		return;

	if (!sess->mplex_writes) {
		io_buffer_buf(sess, buf, bufpos, buflen, val, valsz);
		return;
	}

	assert(*bufpos + valsz + sizeof(int32_t) <= buflen);
	assert(valsz == (valsz & 0xFFFFFF));
	tagbuf = htole32((7 << 24) + valsz);

	io_buffer_int(sess, buf, bufpos, buflen, tagbuf);
	io_buffer_buf(sess, buf, bufpos, buflen, val, valsz);
}

/*
 * Allocate the space needed for io_lowbuffer_buf() and friends.
 * This should be called for *each* lowbuffer operation, so:
 *   io_lowbuffer_alloc(... sizeof(int32_t));
 *   io_lowbuffer_int(...);
 *   io_lowbuffer_alloc(... sizeof(int32_t));
 *   io_lowbuffer_int(...);
 * And not sizeof(int32_t) * 2 or whatnot.
 * Returns zero on failure, non-zero on succes.
 */
int
io_lowbuffer_alloc(struct sess *sess, void **buf,
	size_t *bufsz, size_t *bufmax, size_t sz)
{
	void	*pp;
	size_t	 extra;

	extra = sess->mplex_writes ? sizeof(int32_t) : 0;

	if (*bufsz + sz + extra > *bufmax) {
		pp = realloc(*buf, *bufsz + sz + extra);
		if (pp == NULL) {
			ERR(sess, "realloc");
			return 0;
		}
		*buf = pp;
		*bufmax = *bufsz + sz + extra;
	}
	*bufsz += sz + extra;
	return 1;
}

/*
 * Like io_lowbuffer_buf(), but for a single integer.
 */
void
io_lowbuffer_int(struct sess *sess, void *buf,
	size_t *bufpos, size_t buflen, int32_t val)
{
	int32_t	nv = htole32(val);

	io_lowbuffer_buf(sess, buf, bufpos, buflen, &nv, sizeof(int32_t));
}

/*
 * Like io_buffer_buf(), but for a single integer.
 */
void
io_buffer_int(struct sess *sess, void *buf,
	size_t *bufpos, size_t buflen, int32_t val)
{
	int32_t	nv = htole32(val);

	io_buffer_buf(sess, buf, bufpos, buflen, &nv, sizeof(int32_t));
}

/*
 * Like io_read_buf(), but for a long >=0.
 * Returns zero on failure, non-zero on success.
 */
int
io_read_ulong(struct sess *sess, int fd, uint64_t *val)
{
	int64_t	oval;

	if (!io_read_long(sess, fd, &oval)) {
		ERRX1(sess, "io_read_long");
		return 0;
	} else if (oval < 0) {
		ERRX(sess, "io_read_size: negative value");
		return 1;
	}

	*val = oval;
	return 1;
}

/*
 * Like io_read_buf(), but for a long.
 * Returns zero on failure, non-zero on success.
 */
int
io_read_long(struct sess *sess, int fd, int64_t *val)
{
	int64_t	oval;
	int32_t sval;

	/* Start with the short-circuit: read as an int. */

	if (!io_read_int(sess, fd, &sval)) {
		ERRX1(sess, "io_read_int");
		return 0;
	} else if (sval != INT32_MAX) {
		*val = sval;
		return 1;
	}

	/* If the int is maximal, read as 64 bits. */

	if (!io_read_buf(sess, fd, &oval, sizeof(int64_t))) {
		ERRX1(sess, "io_read_buf");
		return 0;
	}

	*val = le64toh(oval);
	return 1;
}

/*
 * One thing we often need to do is read a size_t.
 * These are transmitted as int32_t, so make sure that the value
 * transmitted is not out of range.
 * FIXME: I assume that size_t can handle int32_t's max.
 * Returns zero on failure, non-zero on success.
 */
int
io_read_size(struct sess *sess, int fd, size_t *val)
{
	int32_t	oval;

	if (!io_read_int(sess, fd, &oval)) {
		ERRX1(sess, "io_read_int");
		return 0;
	} else if (oval < 0) {
		ERRX(sess, "io_read_size: negative value");
		return 0;
	}

	*val = oval;
	return 1;
}

/*
 * Like io_read_buf(), but for an integer.
 * Returns zero on failure, non-zero on success.
 */
int
io_read_int(struct sess *sess, int fd, int32_t *val)
{
	int32_t	oval;

	if (!io_read_buf(sess, fd, &oval, sizeof(int32_t))) {
		ERRX1(sess, "io_read_buf");
		return 0;
	}

	*val = le32toh(oval);
	return 1;
}

/*
 * Copies "valsz" from "buf", full size "bufsz" at position" bufpos",
 * into "val".
 * Calls assert() if the source doesn't have enough data.
 * Increases "bufpos" to the new position.
 */
void
io_unbuffer_buf(struct sess *sess, const void *buf,
	size_t *bufpos, size_t bufsz, void *val, size_t valsz)
{

	assert(*bufpos + valsz <= bufsz);
	memcpy(val, buf + *bufpos, valsz);
	*bufpos += valsz;
}

/*
 * Calls io_unbuffer_buf() and converts.
 */
void
io_unbuffer_int(struct sess *sess, const void *buf,
	size_t *bufpos, size_t bufsz, int32_t *val)
{
	int32_t	oval;

	io_unbuffer_buf(sess, buf, bufpos, bufsz, &oval, sizeof(int32_t));
	*val = le32toh(oval);
}

/*
 * Calls io_unbuffer_buf() and converts.
 */
int
io_unbuffer_size(struct sess *sess, const void *buf,
	size_t *bufpos, size_t bufsz, size_t *val)
{
	int32_t	oval;

	io_unbuffer_int(sess, buf, bufpos, bufsz, &oval);
	if (oval < 0) {
		ERRX(sess, "io_unbuffer_size: negative value");
		return 0;
	}
	*val = oval;
	return 1;
}

/*
 * Like io_read_buf(), but for a single byte >=0.
 * Returns zero on failure, non-zero on success.
 */
int
io_read_byte(struct sess *sess, int fd, uint8_t *val)
{

	if (!io_read_buf(sess, fd, val, sizeof(uint8_t))) {
		ERRX1(sess, "io_read_buf");
		return 0;
	}
	return 1;
}

/*
 * Like io_write_buf(), but for a single byte.
 * Returns zero on failure, non-zero on success.
 */
int
io_write_byte(struct sess *sess, int fd, uint8_t val)
{

	if (!io_write_buf(sess, fd, &val, sizeof(uint8_t))) {
		ERRX1(sess, "io_write_buf");
		return 0;
	}
	return 1;
}
