/*
 * Copyright (c) 1995 - 2003 Kungliga Tekniska Högskolan
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

#include <config.h>

RCSID("$arla: fbuf.c,v 1.22 2003/01/23 10:34:55 tol Exp $") ;

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <unistd.h>

#include <fs_errors.h>

#include <roken.h>

#include <fbuf.h>

#ifdef HAVE_MMAP

/*
 * mmap version of the fbuf interface, the mmap_copy{rx2fd,fd2rx} are
 * little complicated to support reading/writing on non page
 * bonderies.
 */

#if !defined(MAP_FAILED)
#define MAP_FAILED ((void *)(-1))
#endif

/*
 * Map fbuf_flags in `flags' to mmap dito
 */

static int
mmap_flags (fbuf_flags flags)
{
    int ret = 0;

    assert ((flags & (FBUF_PRIVATE | FBUF_SHARED)) == FBUF_PRIVATE
	|| (flags & (FBUF_PRIVATE | FBUF_SHARED)) == FBUF_SHARED);

    if (flags & FBUF_PRIVATE)
	ret = MAP_PRIVATE;
    else if (flags & FBUF_SHARED)
	ret = MAP_SHARED;
    return ret;
}

/*
 * Map fbuf_flags in `flags' to mmap prot
 */

static int
mmap_prot (fbuf_flags flags)
{
    int ret = 0;

    if (flags & FBUF_READ)
	ret |= PROT_READ;
    if (flags & FBUF_WRITE)
	ret |= PROT_WRITE;
    return ret;
}

/*
 * Create a fbuf with (fd, len, flags).
 * Returns 0 or error.
 */

static int
mmap_create (fbuf *f, int fd, size_t len, fbuf_flags flags)
{
    void *buf;

    if (len != 0) {
	buf = mmap (0, len, mmap_prot(flags), mmap_flags(flags), fd, 0);
	if (buf == (void *) MAP_FAILED)
	    return errno;
    } else
	buf = NULL;

    f->buf   = buf;
    f->fd    = fd;
    f->len   = len;
    f->flags = flags;
    return 0;
}

/*
 * Change the size of the underlying file and the fbuf to `new_len'
 * bytes.
 * Returns 0 or error.
 */

static int
mmap_truncate (fbuf *f, size_t new_len)
{
    int ret = 0;

    if (f->buf != NULL) {
	if (msync(f->buf, f->len, MS_ASYNC))
	    ret = errno;
	if (munmap (f->buf, f->len))
	    ret = errno;
	if (ret)
	    return ret;
    }
    ret = ftruncate (f->fd, new_len);
    if (ret < 0)
	return errno;
    return mmap_create (f, f->fd, new_len, f->flags);
}

/*
 * End using `f'.
 * Returns 0 or error.
 */

static int
mmap_end (fbuf *f)
{
    int ret = 0;

    if (msync (f->buf, f->len, MS_ASYNC))
	ret = errno;
    if (munmap (f->buf, f->len))
	ret = errno;
    return ret;
}

/*
 *
 */

static size_t mmap_max_size = 10 * 1024 * 1024;

/*
 * Copy `len' bytes from the rx call `call' to the file `fd'.
 * Returns 0 or error
 */

static int 
mmap_copyrx2fd (struct rx_call *call, int fd, off_t off, size_t len)
{
    void *buf;
    int r_len;
    int ret = 0;
    off_t adjust_off, adjust_len;
    size_t size;

    if (len == 0)
	return 0;

    /* padding */
    adjust_off = off % getpagesize();

    while (len > 0) {

	size = min(len + adjust_off, mmap_max_size);
	adjust_len = getpagesize() - (size % getpagesize());

	buf = mmap (0, size + adjust_len, 
		    PROT_READ | PROT_WRITE, MAP_SHARED, 
		    fd, off - adjust_off);
	if (buf == (void *) MAP_FAILED)
	    return errno;
	r_len = rx_Read (call, ((char *) buf) + adjust_off, size - adjust_off);
	if (r_len != size - adjust_off)
	    ret = conv_to_arla_errno(rx_GetCallError(call));

	len -= r_len;
	off += r_len;
	adjust_off = 0;

	if (msync (buf, size + adjust_len, MS_ASYNC))
	    ret = errno;
	if (munmap (buf, size + adjust_len))
	    ret = errno;

	if (ret)
	    break;
    }

    return ret;
}

/*
 * Copy `len' bytes from `fd' to `call'.
 * Returns 0 or error.
 */

static int
mmap_copyfd2rx (int fd, struct rx_call *call, off_t off, size_t len)
{
    void *buf;
    int w_len;
    int ret = 0;
    off_t adjust_off, adjust_len;
    size_t size;

    if (len == 0)
	return 0;

    adjust_off = off % getpagesize();

    while (len > 0) {

	size = min(len + adjust_off, mmap_max_size);
	adjust_len = size % getpagesize();

	buf = mmap (0, size + adjust_len, 
		    PROT_READ, MAP_PRIVATE, fd, off - adjust_off);
	if (buf == (void *) MAP_FAILED)
	    return errno;
	w_len = rx_Write (call, (char *)buf + adjust_off, size - adjust_off);
	if (w_len != size - adjust_off)
	    ret = conv_to_arla_errno(rx_GetCallError(call));

	len -= w_len;
	off += w_len;
	adjust_off = 0;

	if (munmap (buf, size + adjust_len))
	    ret = errno;
	if (ret)
	    break;
    }

    return ret;
}

#else /* !HAVE_MMAP */

/*
 * Create a fbuf with (fd, len, flags).
 * Returns 0 or error.
 */

static int
malloc_create (fbuf *f, int fd, size_t len, fbuf_flags flags)
{
    void *buf;

    buf = malloc (len);
    if (buf == NULL)
	return ENOMEM;
    if (lseek(fd, 0, SEEK_SET) == -1 ||
	read (fd, buf, len) != len) {
	free(buf);
	return errno;
    }
    f->buf   = buf;
    f->fd    = fd;
    f->len   = len;
    f->flags = flags;
    return 0;
}

/*
 * Write out the data of `f' to the file.
 * Returns 0 or error.
 */

static int
malloc_flush (fbuf *f)
{
    if ((f->flags & FBUF_WRITE) && (f->flags & FBUF_SHARED)) {
	lseek (f->fd, 0, SEEK_SET);
	if (write (f->fd, f->buf, f->len) != f->len)
	    return errno;
    }
    return 0;
}

/*
 * End using `f'.
 * Returns 0 or error.
 */

static int
malloc_end (fbuf *f)
{
    int ret;

    ret = malloc_flush (f);
    free (f->buf);
    return ret;
}

/*
 * Change the size of the underlying file and the fbuf to `new_len'
 * bytes.
 * Returns 0 or error.
 */

static int
malloc_truncate (fbuf *f, size_t new_len)
{
    void *buf;
    int ret;

    ret = malloc_flush (f);
    if (ret)
	goto fail;

    ret = ftruncate (f->fd, new_len);
    if (ret < 0) {
	ret = errno;
	goto fail;
    }

    buf = realloc (f->buf, new_len);
    if (buf == NULL) {
	ret = ENOMEM;
	goto fail;
    }

    f->buf = buf;
    f->len = new_len;
    return 0;

fail:
    free (f->buf);
    f->len = 0;
    return ret;
}

/*
 * Copy `len' bytes from the rx call `call' to the file `fd'.
 * Returns 0 or error
 */

static int 
malloc_copyrx2fd (struct rx_call *call, int fd, off_t off, size_t len)
{
    void *buf;
    struct stat statbuf;
    u_long bufsize;
    u_long nread;
    int ret = 0;

    if (len == 0)
	return 0;

    if (lseek (fd, off, SEEK_SET) == -1)
	return errno;

    if (fstat (fd, &statbuf)) {
/*	arla_warn (ADEBFBUF, errno, "fstat"); */
	bufsize = 1024;
    } else
	bufsize = statbuf.st_blksize;

    buf = malloc (bufsize);
    if (buf == NULL)
	return ENOMEM;

    while ( len && (nread = rx_Read (call, buf, min(bufsize,
						    len))) > 0) {
	if (write (fd, buf, nread) != nread) {
	    ret = errno;
	    break;
	}
	len -= nread;
    }
    free (buf);
    return ret;
}

/*
 * Copy `len' bytes from at offset `off' in `fd' to `call'.
 * Returns 0 or error.
 */

static int
malloc_copyfd2rx (int fd, struct rx_call *call, off_t off, size_t len)
{
    void *buf;
    struct stat statbuf;
    u_long bufsize;
    u_long nread;
    int ret = 0;

    if (len == 0)
	return 0;

    if (lseek (fd, off, SEEK_SET) < 0)
	return errno;

    if (fstat (fd, &statbuf)) {
/*	arla_warn (ADEBFBUF, errno, "fstat"); */
	bufsize = 1024;
    } else
	bufsize = statbuf.st_blksize;

    buf = malloc (bufsize);
    if (buf == NULL)
	return ENOMEM;

    while (len
	   && (nread = read (fd, buf, min(bufsize, len))) > 0) {
	if (rx_Write (call, buf, nread) != nread) {
	    ret = errno;
	    break;
	}
	len -= nread;
    }
    free (buf);
    return ret;
}
#endif /* !HAVE_MMAP */

/*
 * Accessor functions.
 */

size_t
fbuf_len (fbuf *f)
{
    return f->len;
}

/*
 * 
 */

void *
fbuf_buf (fbuf *f)
{
    return f->buf;
}

/*
 * Return a pointer to a copy of this file contents. If we have mmap,
 * we use that, otherwise we have to allocate some memory and read it.
 */

int
fbuf_create (fbuf *f, int fd, size_t len, fbuf_flags flags)
{
#ifdef HAVE_MMAP
    return mmap_create (f, fd, len, flags);
#else
    return malloc_create (f, fd, len, flags);
#endif
}

/*
 * Undo everything we did in fbuf_create.
 */

int
fbuf_end (fbuf *f)
{
#ifdef HAVE_MMAP
    return mmap_end (f);
#else
    return malloc_end (f);
#endif
}

/*
 * Make the file (and the buffer) be `new_len' bytes.
 */

int
fbuf_truncate (fbuf *f, size_t new_len)
{
#ifdef HAVE_MMAP
    return mmap_truncate (f, new_len);
#else
    return malloc_truncate (f, new_len);
#endif
}

/*
 * Copy from a RX_call to a fd.
 * The area between offset and len + offset should be present in the file.
 */

int 
copyrx2fd (struct rx_call *call, int fd, off_t off, size_t len)
{
#ifdef HAVE_MMAP
    return mmap_copyrx2fd (call, fd, off, len);
#else
    return malloc_copyrx2fd (call, fd, off, len);
#endif
}

/*
 * Copy from a file descriptor to a RX call.
 *
 * Returns: error number if failed
 */

int
copyfd2rx (int fd, struct rx_call *call, off_t off, size_t len)
{
#ifdef HAVE_MMAP
    return mmap_copyfd2rx (fd, call, off, len);
#else
    return malloc_copyfd2rx (fd, call, off, len);
#endif
}
