/*	$OpenBSD: fbuf.c,v 1.2 1999/04/30 01:59:07 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

#include "arla_local.h"
RCSID("$KTH: fbuf.c,v 1.22 1998/12/25 20:25:58 assar Exp $") ;

#ifdef BROKEN_MMAP
#undef HAVE_MMAP
#endif

#ifdef HAVE_MMAP

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

    if (flags & FBUF_READ)
	ret = MAP_PRIVATE;
    if (flags & FBUF_WRITE)
	ret = MAP_SHARED;
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
	buf = mmap (0, len, PROT_READ | PROT_WRITE, mmap_flags(flags), fd, 0);
	if (buf == (void *) MAP_FAILED) {
	    close (fd);
	    return errno;
	}
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
    int ret;

    if (f->buf != NULL) {
	ret = munmap (f->buf, f->len);
	if (ret < 0) {
	    close (f->fd);
	    return errno;
	}
    }
    ret = ftruncate (f->fd, new_len);
    if (ret < 0) {
	close (f->fd);
	return errno;
    }
    return mmap_create (f, f->fd, new_len, f->flags);
}

/*
 * End using `f'.
 * Returns 0 or error.
 */

static int
mmap_end (fbuf *f)
{
    int ret;

    ret = munmap (f->buf, f->len);
    if (ret < 0)
	ret = errno;
    close (f->fd);
    return ret;
}

/*
 * Copy `len' bytes from the rx call `call' to the file `fd'.
 * Returns 0 or error
 */

static int 
mmap_copyrx2fd (struct rx_call *call, int fd, size_t len)
{
    void *buf;
    int r_len;
    int ret = 0;
	     
    if (len == 0)
	return 0;

    if(ftruncate (fd, len) < 0)
	return errno;
    buf = mmap (0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf == (void *) MAP_FAILED)
	return errno;
    r_len = rx_Read (call, buf, len);
    if (r_len != len)
	ret = rx_Error(call);
    munmap (buf, len);
    ftruncate (fd, len);
    return ret;
}

/*
 * Copy `len' bytes from `fd' to `call'.
 * Returns 0 or error.
 */

static int
mmap_copyfd2rx (int fd, struct rx_call *call, size_t len)
{
    void *buf;
    int r_write;
    int ret = 0;

    if (len == 0)
	return 0;

    buf = mmap (0, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == (void *) MAP_FAILED)
	return errno;
    r_write = rx_Write (call, buf, len);
    if (r_write != len)
	ret = rx_Error(call);
    munmap (buf, len);
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
    if (buf == NULL) {
	close(fd);
	return ENOMEM;
    }
    if (lseek(fd, 0, SEEK_SET) == -1 ||
	read (fd, buf, len) != len) {
	free(buf);
	close(fd);
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
    if (f->flags & FBUF_WRITE) {
	lseek (f->fd, SEEK_SET, 0);
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
    close (f->fd);
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
    close (f->fd);
    return ret;
}

/*
 * Copy `len' bytes from the rx call `call' to the file `fd'.
 * Returns 0 or error
 */

static int 
malloc_copyrx2fd (struct rx_call *call, int fd, size_t len)
{
    void *buf;
    struct stat statbuf;
    u_long bufsize;
    u_long nread;
    size_t reallen = len;
    int ret = 0;

    if (len == 0)
	return 0;

    if (fstat (fd, &statbuf)) {
	arla_warn (ADEBFBUF, errno, "fstat");
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
    if (ftruncate (fd, reallen) < 0)
	ret = errno;
    free (buf);
    return ret;
}

/*
 * Copy `len' bytes from `fd' to `call'.
 * Returns 0 or error.
 */

static int
malloc_copyfd2rx (int fd, struct rx_call *call, size_t len)
{
    void *buf;
    struct stat statbuf;
    u_long bufsize;
    u_long nread;
    int ret = 0;

    if (len == 0)
	return 0;

    if (fstat (fd, &statbuf)) {
	arla_warn (ADEBFBUF, errno, "fstat");
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
 */

int 
copyrx2fd (struct rx_call *call, int fd, size_t len)
{
#ifdef HAVE_MMAP
    return mmap_copyrx2fd (call, fd, len);
#else
    return malloc_copyrx2fd (call, fd, len);
#endif
}

/*
 * Copy from a file descriptor to a RX call.
 *
 * Returns: error number if failed
 */

int
copyfd2rx (int fd, struct rx_call *call, size_t len)
{
#ifdef HAVE_MMAP
    return mmap_copyfd2rx (fd, call, len);
#else
    return malloc_copyfd2rx (fd, call, len);
#endif
}
