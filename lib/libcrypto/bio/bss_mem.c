/* $OpenBSD: bss_mem.c,v 1.19 2022/02/18 17:30:13 jsing Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/buffer.h>

#include "bio_local.h"

static int mem_new(BIO *bio);
static int mem_free(BIO *bio);
static int mem_write(BIO *bio, const char *in, int in_len);
static int mem_read(BIO *bio, char *out, int out_len);
static int mem_puts(BIO *bio, const char *in);
static int mem_gets(BIO *bio, char *out, int out_len);
static long mem_ctrl(BIO *bio, int cmd, long arg1, void *arg2);

static const BIO_METHOD mem_method = {
	.type = BIO_TYPE_MEM,
	.name = "memory buffer",
	.bwrite = mem_write,
	.bread = mem_read,
	.bputs = mem_puts,
	.bgets = mem_gets,
	.ctrl = mem_ctrl,
	.create = mem_new,
	.destroy = mem_free
};

/*
 * bio->num is used to hold the value to return on 'empty', if it is
 * 0, should_retry is not set.
 */

const BIO_METHOD *
BIO_s_mem(void)
{
	return &mem_method;
}

BIO *
BIO_new_mem_buf(const void *buf, int buf_len)
{
	BIO *bio;
	BUF_MEM *b;

	if (buf == NULL) {
		BIOerror(BIO_R_NULL_PARAMETER);
		return NULL;
	}
	if (buf_len == -1)
		buf_len = strlen(buf);
	if (buf_len < 0) {
		BIOerror(BIO_R_INVALID_ARGUMENT);
		return NULL;
	}

	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		return NULL;

	b = bio->ptr;
	b->data = (void *)buf;	/* Trust in the BIO_FLAGS_MEM_RDONLY flag. */
	b->length = buf_len;
	b->max = buf_len;
	bio->flags |= BIO_FLAGS_MEM_RDONLY;
	/* Since this is static data retrying will not help. */
	bio->num = 0;

	return bio;
}

static int
mem_new(BIO *bio)
{
	BUF_MEM *b;

	if ((b = BUF_MEM_new()) == NULL)
		return 0;

	bio->shutdown = 1;
	bio->init = 1;
	bio->num = -1;
	bio->ptr = b;

	return 1;
}

static int
mem_free(BIO *bio)
{
	BUF_MEM *b;

	if (bio == NULL)
		return 0;
	if (!bio->shutdown || !bio->init || bio->ptr == NULL)
		return 1;

	b = bio->ptr;
	if (bio->flags & BIO_FLAGS_MEM_RDONLY)
		b->data = NULL;
	BUF_MEM_free(b);
	bio->ptr = NULL;

	return 1;
}

static int
mem_read(BIO *bio, char *out, int out_len)
{
	BUF_MEM *bm = bio->ptr;

	BIO_clear_retry_flags(bio);

	if (out == NULL || out_len <= 0)
		return 0;

	if ((size_t)out_len > bm->length)
		out_len = bm->length;

	if (out_len == 0) {
		if (bio->num != 0)
			BIO_set_retry_read(bio);
		return bio->num;
	}

	memcpy(out, bm->data, out_len);
	bm->length -= out_len;
	if (bio->flags & BIO_FLAGS_MEM_RDONLY) {
		bm->data += out_len;
	} else {
		memmove(&(bm->data[0]), &(bm->data[out_len]),
		    bm->length);
	}
	return out_len;
}

static int
mem_write(BIO *bio, const char *in, int in_len)
{
	BUF_MEM *bm = bio->ptr;
	size_t buf_len;

	BIO_clear_retry_flags(bio);

	if (in == NULL || in_len <= 0)
		return 0;

	if (bio->flags & BIO_FLAGS_MEM_RDONLY) {
		BIOerror(BIO_R_WRITE_TO_READ_ONLY_BIO);
		return -1;
	}

	/*
	 * Check for overflow and ensure we do not exceed an int, otherwise we
	 * cannot tell if BUF_MEM_grow_clean() succeeded.
	 */
	buf_len = bm->length + in_len;
	if (buf_len < bm->length || buf_len > INT_MAX)
		return -1;

	if (BUF_MEM_grow_clean(bm, buf_len) != buf_len)
		return -1;

	memcpy(&bm->data[buf_len - in_len], in, in_len);

	return in_len;
}

static long
mem_ctrl(BIO *bio, int cmd, long num, void *ptr)
{
	BUF_MEM *bm = bio->ptr;
	long ret = 1;
	char **pptr;

	switch (cmd) {
	case BIO_CTRL_RESET:
		if (bm->data != NULL) {
			/* For read only case reset to the start again */
			if (bio->flags & BIO_FLAGS_MEM_RDONLY) {
				bm->data -= bm->max - bm->length;
				bm->length = bm->max;
			} else {
				memset(bm->data, 0, bm->max);
				bm->length = 0;
			}
		}
		break;
	case BIO_CTRL_EOF:
		ret = (long)(bm->length == 0);
		break;
	case BIO_C_SET_BUF_MEM_EOF_RETURN:
		bio->num = (int)num;
		break;
	case BIO_CTRL_INFO:
		if (ptr != NULL) {
			pptr = (char **)ptr;
			*pptr = (char *)bm->data;
		}
		ret = (long)bm->length;
		break;
	case BIO_C_SET_BUF_MEM:
		mem_free(bio);
		bio->shutdown = (int)num;
		bio->ptr = ptr;
		break;
	case BIO_C_GET_BUF_MEM_PTR:
		if (ptr != NULL) {
			pptr = (char **)ptr;
			*pptr = (char *)bm;
		}
		break;
	case BIO_CTRL_GET_CLOSE:
		ret = (long)bio->shutdown;
		break;
	case BIO_CTRL_SET_CLOSE:
		bio->shutdown = (int)num;
		break;
	case BIO_CTRL_WPENDING:
		ret = 0L;
		break;
	case BIO_CTRL_PENDING:
		ret = (long)bm->length;
		break;
	case BIO_CTRL_DUP:
	case BIO_CTRL_FLUSH:
		ret = 1;
		break;
	case BIO_CTRL_PUSH:
	case BIO_CTRL_POP:
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int
mem_gets(BIO *bio, char *out, int out_len)
{
	BUF_MEM *bm = bio->ptr;
	int i, out_max;
	char *p;
	int ret = -1;

	BIO_clear_retry_flags(bio);

	out_max = bm->length;
	if (out_len - 1 < out_max)
		out_max = out_len - 1;
	if (out_max <= 0) {
		*out = '\0';
		return 0;
	}

	p = bm->data;
	for (i = 0; i < out_max; i++) {
		if (p[i] == '\n') {
			i++;
			break;
		}
	}

	/*
	 * i is now the max num of bytes to copy, either out_max or up to and
	 * including the first newline
	 */ 
	if ((ret = mem_read(bio, out, i)) > 0)
		out[ret] = '\0';

	return ret;
}

static int
mem_puts(BIO *bio, const char *in)
{
	return mem_write(bio, in, strlen(in));
}
