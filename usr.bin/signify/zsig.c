/* $OpenBSD: zsig.c,v 1.18 2019/12/22 06:37:25 espie Exp $ */
/*
 * Copyright (c) 2016 Marc Espie <espie@openbsd.org>
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

#ifndef VERIFYONLY
#include <stdint.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sha2.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include "signify.h"

struct gzheader {
	uint8_t flg;
	uint32_t mtime;
	uint8_t xflg;
	uint8_t os;
	uint8_t *name;
	uint8_t *comment;
	uint8_t *endcomment;
	unsigned long long headerlength;
	uint8_t *buffer;
};

#define FTEXT_FLAG 1
#define FHCRC_FLAG 2
#define FEXTRA_FLAG 4
#define FNAME_FLAG 8
#define FCOMMENT_FLAG 16

#define GZHEADERLENGTH 10
#define MYBUFSIZE 65536LU


static uint8_t fake[10] = { 0x1f, 0x8b, 8, FCOMMENT_FLAG, 0, 0, 0, 0, 0, 3 };

static uint8_t *
readgz_header(struct gzheader *h, int fd)
{
	size_t sz = 1023;
	uint8_t *p;
	size_t pos = 0;
	size_t len = 0;
	int state = 0;
	ssize_t n;
	uint8_t *buf;

	buf = xmalloc(sz);

	while (1) {
		if (len == sz) {
			sz *= 2;
			buf = realloc(buf, sz);
			if (!buf)
				err(1, "realloc");
		}
		n = read(fd, buf+len, sz-len);
		if (n == -1)
			err(1, "read");
		/* incomplete info */
		if (n == 0)
			errx(1, "gzheader truncated");
		len += n;
		h->comment = NULL;
		h->name = NULL;

		switch(state) {
		case 0: /* check header proper */
			/* need ten bytes */
			if (len < GZHEADERLENGTH)
				continue;
			h->flg = buf[3];
			h->mtime = buf[4] | (buf[5] << 8U) | (buf[6] << 16U) |
			    (buf[7] << 24U);
			h->xflg = buf[8];
			h->os = buf[9];
			/* magic gzip header */
			if (buf[0] != 0x1f || buf[1] != 0x8b || buf[2] != 8)
				err(1, "invalid magic in gzheader");
			/* XXX special code that only caters to our needs */
			if (h->flg & ~ (FCOMMENT_FLAG | FNAME_FLAG))
				err(1, "invalid flags in gzheader");
			pos = GZHEADERLENGTH;
			state++;
			/*FALLTHRU*/
		case 1:
			if (h->flg & FNAME_FLAG) {
				p = memchr(buf+pos, 0, len - pos);
				if (!p)
					continue;
				pos = (p - buf) + 1;
			}
			state++;
			/*FALLTHRU*/
		case 2:
			if (h->flg & FCOMMENT_FLAG) {
				p = memchr(buf+pos, 0, len - pos);
				if (!p)
					continue;
				h->comment = buf + pos;
				h->endcomment = p;
				pos = (p - buf) + 1;
			}
			if (h->flg & FNAME_FLAG)
				h->name = buf + GZHEADERLENGTH;
			h->headerlength = pos;
			h->buffer = buf;
			return buf + len;
		}

	}
}

static void
copy_blocks(int fdout, int fdin, const char *sha, const char *endsha,
    size_t bufsize, uint8_t *bufend)
{
	uint8_t *buffer;
	uint8_t *residual;
	uint8_t output[SHA512_256_DIGEST_STRING_LENGTH];

	buffer = xmalloc(bufsize);
	residual = (uint8_t *)endsha + 1;

	while (1) {
		/* get the next block */
		size_t n = 0;
		/* if we have residual data, we use it */
		if (residual != bufend) {
			/* how much can we copy */
			size_t len = bufend - residual;
			n = len >= bufsize ? bufsize : len;
			memcpy(buffer, residual, n);
			residual += n;
		}
		/* if we're not done yet, try to obtain more until EOF */
		while (n != bufsize) {
			ssize_t more = read(fdin, buffer+n, bufsize-n);
			if (more == -1)
				err(1, "read");
			n += more;
			if (more == 0)
				break;
		}
		SHA512_256Data(buffer, n, output);
		if (endsha - sha < SHA512_256_DIGEST_STRING_LENGTH-1)
			errx(4, "signature truncated");
		if (memcmp(output, sha, SHA512_256_DIGEST_STRING_LENGTH-1) != 0)
			errx(4, "signature mismatch");
		if (sha[SHA512_256_DIGEST_STRING_LENGTH-1] != '\n')
			errx(4, "signature mismatch");
		sha += SHA512_256_DIGEST_STRING_LENGTH;
		writeall(fdout, buffer, n, "stdout");
		if (n != bufsize)
			break;
	}
	free(buffer);
}

void
zverify(const char *pubkeyfile, const char *msgfile, const char *sigfile,
    const char *keytype)
{
	struct gzheader h;
	size_t bufsize, len;
	char *p;
	uint8_t *bufend;
	int fdin, fdout;

	/* by default, verification will love pipes */
	if (!sigfile)
		sigfile = "-";
	if (!msgfile)
		msgfile = "-";

	fdin = xopen(sigfile, O_RDONLY | O_NOFOLLOW, 0);

	bufend = readgz_header(&h, fdin);
	if (!(h.flg & FCOMMENT_FLAG))
		errx(1, "unsigned gzip archive");
	fake[8] = h.xflg;
	len = h.endcomment-h.comment;

	p = verifyzdata(h.comment, len, sigfile,
	    pubkeyfile, keytype);

	bufsize = MYBUFSIZE;

#define BEGINS_WITH(x, y) memcmp((x), (y), sizeof(y)-1) == 0

	while (BEGINS_WITH(p, "algorithm=SHA512/256") ||
	    BEGINS_WITH(p, "date=") ||
	    BEGINS_WITH(p, "key=") ||
	    sscanf(p, "blocksize=%zu\n", &bufsize) > 0) {
		while (*(p++) != '\n')
			continue;
	}

	if (*p != '\n')
		errx(1, "invalid signature");

	fdout = xopen(msgfile, O_CREAT|O_TRUNC|O_NOFOLLOW|O_WRONLY, 0666);
	writeall(fdout, fake, sizeof fake, msgfile);
	writeall(fdout, h.comment, len+1, msgfile);
	*(p++) = 0;
	copy_blocks(fdout, fdin, p, h.endcomment, bufsize, bufend);
	free(h.buffer);
	close(fdout);
	close(fdin);
}

void
zsign(const char *seckeyfile, const char *msgfile, const char *sigfile,
    int skipdate)
{
	size_t bufsize = MYBUFSIZE;
	int fdin, fdout;
	struct gzheader h;
	struct stat sb;
	size_t space;
	char *msg;
	char *p;
	uint8_t *buffer;
	uint8_t *sighdr;
	char date[80];
	time_t clock;

	fdin = xopen(msgfile, O_RDONLY, 0);
	if (fstat(fdin, &sb) == -1 || !S_ISREG(sb.st_mode))
		errx(1, "Sorry can only sign regular files");

	readgz_header(&h, fdin);
	/* we don't care about the header, actually */
	free(h.buffer);

	if (lseek(fdin, h.headerlength, SEEK_SET) == -1)
		err(1, "seek in %s", msgfile);

	space = (sb.st_size / MYBUFSIZE+1) * SHA512_256_DIGEST_STRING_LENGTH +
		1024; /* long enough for extra header information */

	msg = xmalloc(space);
	buffer = xmalloc(bufsize);
	if (skipdate) {
		clock = 0;
	} else {
		time(&clock);
	}
	strftime(date, sizeof date, "%Y-%m-%dT%H:%M:%SZ", gmtime(&clock));
	snprintf(msg, space,
	    "date=%s\n"
	    "key=%s\n"
	    "algorithm=SHA512/256\n"
	    "blocksize=%zu\n\n",
	    date, seckeyfile, bufsize);
	p = strchr(msg, 0);

	while (1) {
		size_t n = read(fdin, buffer, bufsize);
		if (n == -1)
			err(1, "read from %s", msgfile);
		if (n == 0)
			break;
		SHA512_256Data(buffer, n, p);
		p += SHA512_256_DIGEST_STRING_LENGTH;
		p[-1] = '\n';
		if (msg + space < p)
			errx(1, "file too long %s", msgfile);
	}
	*p = 0;

	fdout = xopen(sigfile, O_CREAT|O_TRUNC|O_NOFOLLOW|O_WRONLY, 0666);
	sighdr = createsig(seckeyfile, msgfile, msg, p-msg);
	fake[8] = h.xflg;

	writeall(fdout, fake, sizeof fake, sigfile);
	writeall(fdout, sighdr, strlen(sighdr), sigfile);
	free(sighdr);
	/* need the 0 ! */
	writeall(fdout, msg, p - msg + 1, sigfile);
	free(msg);

	if (lseek(fdin, h.headerlength, SEEK_SET) == -1)
		err(1, "seek in %s", msgfile);

	while (1) {
		size_t n = read(fdin, buffer, bufsize);
		if (n == -1)
			err(1, "read from %s", msgfile);
		if (n == 0)
			break;
		writeall(fdout, buffer, n, sigfile);
	}
	free(buffer);
	close(fdout);
}
#endif
