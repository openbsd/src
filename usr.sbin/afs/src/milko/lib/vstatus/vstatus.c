/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This module contains the code that parse the file /vice.+/vol\d{8}
 * file. The file is have the following fields:
 *	see vstat.xg file.
 */

#include <config.h>

RCSID("$KTH: vstatus.c,v 1.5 2000/10/03 00:20:35 lha Exp $");

#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <unistd.h>

#include <assert.h>

#include <err.h>

#include <vstatus.h>

/*
 *  Read in the volume status from `fd' and return it in `vs'
 */

int
vstatus_read (int fd, vstatus *v)
{
    char *ptr = NULL;
    char *ret_ptr;
    size_t len;
    int ret;

    assert (v);

    ptr = malloc (VSTATUS_SIZE);
    if (ptr == NULL) {
	ret = errno;
	goto err_out;
    }
    len = VSTATUS_SIZE;

    ret = lseek (fd, 0, SEEK_SET);
    if (ret) {
	ret = errno;
	goto err_out;
    }

    ret = read (fd, ptr, VSTATUS_SIZE);
    if (ret != VSTATUS_SIZE) {
	ret = errno;
	goto err_out;
    }

    ret_ptr = ydr_decode_vstatus (v, ptr, &len);
    if (len != 0) {
	ret = errno;
	goto err_out;
    }
    free (ptr);

    if (v->magic != VSTATUS_MAGIC ||
	v->version != VSTATUS_VERSION)
	return EINVAL;

    return 0;

 err_out:
    if (ptr)
	free (ptr);

    return ret;
}

/*
 * Write `vs' to `fd'
 */

int
vstatus_write (int fd, vstatus *v)
{
    char *ptr = NULL;
    char *ret_ptr;
    size_t len;
    int ret;

    assert (v);

    v->magic = VSTATUS_MAGIC;
    v->version = VSTATUS_VERSION;

    ptr = malloc (VSTATUS_SIZE);
    if (ptr == NULL) {
	ret = errno;
	goto err_out;
    }
    len = VSTATUS_SIZE;

    ret = lseek (fd, 0, SEEK_SET);
    if (ret) {
	ret = errno;
	goto err_out;
    }

    ret_ptr = ydr_encode_vstatus (v, ptr, &len);
    if (len != 0 || ret_ptr == NULL) {
	ret = errno;
	goto err_out;
    }

    ret = write (fd, ptr, VSTATUS_SIZE);
    if (ret != VSTATUS_SIZE) {
	ret = errno;
	goto err_out;
    }
    free (ptr);

    return 0;

 err_out:
    if (ptr)
	free (ptr);

    return ret;
}

void
vstatus_print_onode (FILE *out, onode_opaque *o)
{
    int i;
    assert (o);

    fprintf (out, "%d:0x", o->size);
    for (i = 0; i < o->size; i++) {
	fprintf (out, "%02x", (int) o->data[i] & 0xff);
    }
    fprintf (out, "\n");

}
