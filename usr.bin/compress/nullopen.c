/*	$OpenBSD: nullopen.c,v 1.4 2011/09/22 10:41:04 deraadt Exp $	*/

/*
 * Copyright (c) 2003 Can Erkin Acar
 * Copyright (c) 1997 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "compress.h"

typedef struct {
	off_t 	  total_in;
	off_t 	  total_out;
	int   	  fd;
	int       gotmagic;
	char	  mode;
} null_stream;

char null_magic[2];


void *
null_open(int fd, const char *mode, char *name, int bits,
    u_int32_t mtime, int gotmagic)
{
	null_stream *s;

	if (fd < 0 || !mode)
		return NULL;

	if ((mode[0] != 'r' && mode[0] != 'w') || mode[1] != '\0') {
		errno = EINVAL;
		return NULL;
	}

	if ((s = (null_stream *) calloc(1, sizeof(null_stream))) == NULL)
		return NULL;

	s->fd = fd;
	s->gotmagic = gotmagic;
	s->total_in = s->total_out = 0;
	s->mode = mode[0];

	return s;
}

int
null_close(void *cookie, struct z_info *info, const char *name, struct stat *sb)
{
	null_stream *s = (null_stream*) cookie;
	int err = 0;

	if (s == NULL)
		return -1;


	if (info != NULL) {
		info->mtime = 0;
		info->crc =  (u_int32_t)-1;
		info->hlen = 0;
		info->total_in = (off_t) s->total_in;
		info->total_out = (off_t) s->total_out;
	}

	setfile(name, s->fd, sb);
	err = close(s->fd);

	free(s);

	return err;
}

int
null_flush(void *cookie, int flush)
{
	null_stream *s = (null_stream*)cookie;

	if (s == NULL || s->mode != 'w') {
		errno = EBADF;
		return (-1);
	}

	return 0;
}

int
null_read(void *cookie, char *buf, int len)
{
	null_stream *s = (null_stream*)cookie;

	if (s == NULL) {
		errno = EBADF;
		return (-1);
	}
	if (s->gotmagic) {
		if (len < 2) {
			errno = EBADF;
			return (-1);
		}

		buf[0] = null_magic[0];
		buf[1] = null_magic[1];
		s->gotmagic = 0;

		return (2);
	}

	return read(s->fd, buf, len);
}

int
null_write(void *cookie, const char *buf, int len)
{
	null_stream *s = (null_stream*)cookie;

	if (s == NULL) {
		errno = EBADF;
		return (-1);
	}

	return write(s->fd, buf, len);
}
