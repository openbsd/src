/*	$OpenBSD: shf.c,v 1.1.1.1 1996/08/14 06:19:11 downsj Exp $	*/

/*
 *  Shell file I/O routines
 */

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_limval.h"


/* flags to shf_emptybuf() */
#define EB_READSW	0x01	/* about to switch to reading */
#define EB_GROW		0x02	/* grow buffer if necessary (STRING+DYNAMIC) */

/*
 * Replacement stdio routines.  Stdio is too flakey on too many machines
 * to be useful when you have multiple processes using the same underlying
 * file descriptors.
 */

static int	shf_fillbuf	ARGS((struct shf *shf));
static int	shf_emptybuf	ARGS((struct shf *shf, int flags));

/* Open a file.  First three args are for open(), last arg is flags for
 * this package.  Returns NULL if file could not be opened, or if a dup
 * fails.
 */
struct shf *
shf_open(name, oflags, mode, sflags)
	const char *name;
	int oflags;
	int mode;
	int sflags;
{
	int fd;

	fd = open(name, oflags, mode);
	if (fd < 0)
		return NULL;
	if ((sflags & SHF_MAPHI) && fd < FDBASE) {
		int nfd;

		nfd = ksh_dupbase(fd, FDBASE);
		close(fd);
		if (nfd < 0)
			return NULL;
		fd = nfd;
	}
	sflags &= ~SHF_ACCMODE;
	sflags |= (oflags & O_ACCMODE) == O_RDONLY ? SHF_RD
		  : ((oflags & O_ACCMODE) == O_WRONLY ? SHF_WR
		     : SHF_RDWR);

	return shf_fdopen(fd, sflags, (struct shf *) 0);
}

/* Set up the shf structure for a file descriptor.  Doesn't fail. */
struct shf *
shf_fdopen(fd, sflags, shf)
	int fd;
	int sflags;
	struct shf *shf;
{
	int bsize = sflags & SHF_UNBUF ? (sflags & SHF_RD ? 1 : 0) : SHF_BSIZE;

	/* use fcntl() to figure out correct read/write flags */
	if (sflags & SHF_GETFL) {
		int flags = fcntl(fd, F_GETFL, 0);

		if (flags < 0)
			/* will get an error on first read/write */
			sflags |= SHF_RDWR;
		else
			switch (flags & O_ACCMODE) {
			case O_RDONLY: sflags |= SHF_RD; break;
			case O_WRONLY: sflags |= SHF_WR; break;
			case O_RDWR: sflags |= SHF_RDWR; break;
			}
	}

	if (!(sflags & (SHF_RD | SHF_WR)))
		internal_errorf(1, "shf_fdopen: missing read/write");

	if (shf) {
		if (bsize) {
			shf->buf = (unsigned char *) alloc(bsize, ATEMP);
			sflags |= SHF_ALLOCB;
		} else
			shf->buf = (unsigned char *) 0;
	} else {
		shf = (struct shf *) alloc(sizeof(struct shf) + bsize, ATEMP);
		shf->buf = (unsigned char *) &shf[1];
		sflags |= SHF_ALLOCS;
	}
	shf->areap = ATEMP;
	shf->fd = fd;
	shf->rp = shf->wp = shf->buf;
	shf->rnleft = 0;
	shf->rbsize = bsize;
	shf->wnleft = 0; /* force call to shf_emptybuf() */
	shf->wbsize = sflags & SHF_UNBUF ? 0 : bsize;
	shf->flags = sflags;
	shf->errno_ = 0;
	shf->bsize = bsize;
	if (sflags & SHF_CLEXEC)
		fd_clexec(fd);
	return shf;
}

/* Set up an existing shf (and buffer) to use the given fd */
struct shf *
shf_reopen(fd, sflags, shf)
	int fd;
	int sflags;
	struct shf *shf;
{
	int bsize = sflags & SHF_UNBUF ? (sflags & SHF_RD ? 1 : 0) : SHF_BSIZE;

	/* use fcntl() to figure out correct read/write flags */
	if (sflags & SHF_GETFL) {
		int flags = fcntl(fd, F_GETFL, 0);

		if (flags < 0)
			/* will get an error on first read/write */
			sflags |= SHF_RDWR;
		else
			switch (flags & O_ACCMODE) {
			case O_RDONLY: sflags |= SHF_RD; break;
			case O_WRONLY: sflags |= SHF_WR; break;
			case O_RDWR: sflags |= SHF_RDWR; break;
			}
	}

	if (!(sflags & (SHF_RD | SHF_WR)))
		internal_errorf(1, "shf_reopen: missing read/write");
	if (!shf || !shf->buf || shf->bsize < bsize)
		internal_errorf(1, "shf_reopen: bad shf/buf/bsize");

	/* assumes shf->buf and shf->bsize already set up */
	shf->fd = fd;
	shf->rp = shf->wp = shf->buf;
	shf->rnleft = 0;
	shf->rbsize = bsize;
	shf->wnleft = 0; /* force call to shf_emptybuf() */
	shf->wbsize = sflags & SHF_UNBUF ? 0 : bsize;
	shf->flags = (shf->flags & (SHF_ALLOCS | SHF_ALLOCB)) | sflags;
	shf->errno_ = 0;
	if (sflags & SHF_CLEXEC)
		fd_clexec(fd);
	return shf;
}

/* Open a string for reading or writing.  If reading, bsize is the number
 * of bytes that can be read.  If writing, bsize is the maximum number of
 * bytes that can be written.  If shf is not null, it is filled in and
 * returned, if it is null, shf is allocated.  If writing and buf is null
 * and SHF_DYNAMIC is set, the buffer is allocated (if bsize > 0, it is
 * used for the initial size).  Doesn't fail.
 * When writing, a byte is reserved for a trailing null - see shf_sclose().
 */
struct shf *
shf_sopen(buf, bsize, sflags, shf)
	char *buf;
	int bsize;
	int sflags;
	struct shf *shf;
{
	/* can't have a read+write string */
	if (!(sflags & (SHF_RD | SHF_WR))
	    || (sflags & (SHF_RD | SHF_WR)) == (SHF_RD | SHF_WR))
		internal_errorf(1, "shf_sopen: flags 0x%x", sflags);

	if (!shf) {
		shf = (struct shf *) alloc(sizeof(struct shf), ATEMP);
		sflags |= SHF_ALLOCS;
	}
	shf->areap = ATEMP;
	if (!buf && (sflags & SHF_WR) && (sflags & SHF_DYNAMIC)) {
		if (bsize <= 0)
			bsize = 64;
		sflags |= SHF_ALLOCB;
		buf = alloc(bsize, shf->areap);
	}
	shf->fd = -1;
	shf->buf = shf->rp = shf->wp = (unsigned char *) buf;
	shf->rnleft = bsize;
	shf->rbsize = bsize;
	shf->wnleft = bsize - 1;	/* space for a '\0' */
	shf->wbsize = bsize;
	shf->flags = sflags | SHF_STRING;
	shf->errno_ = 0;
	shf->bsize = bsize;

	return shf;
}

/* Flush and close file descriptor, free the shf structure */
int
shf_close(shf)
	struct shf *shf;
{
	int ret = 0;

	if (shf->fd >= 0) {
		ret = shf_flush(shf);
		if (close(shf->fd) < 0)
			ret = EOF;
	}
	if (shf->flags & SHF_ALLOCS)
		afree(shf, shf->areap);
	else if (shf->flags & SHF_ALLOCB)
		afree(shf->buf, shf->areap);

	return ret;
}

/* Flush and close file descriptor, don't free file structure */
int
shf_fdclose(shf)
	struct shf *shf;
{
	int ret = 0;

	if (shf->fd >= 0) {
		ret = shf_flush(shf);
		if (close(shf->fd) < 0)
			ret = EOF;
		shf->rnleft = 0;
		shf->rp = shf->buf;
		shf->wnleft = 0;
		shf->fd = -1;
	}

	return ret;
}

/* Close a string - if it was opened for writing, it is null terminated;
 * returns a pointer to the string and frees shf if it was allocated
 * (does not free string if it was allocated).
 */
char *
shf_sclose(shf)
	struct shf *shf;
{
	unsigned char *s = shf->buf;

	/* null terminate */
	if (shf->flags & SHF_WR) {
		shf->wnleft++;
		shf_putc('\0', shf);
	}
	if (shf->flags & SHF_ALLOCS)
		afree(shf, shf->areap);
	return (char *) s;
}

/* Flush and free file structure, don't close file descriptor */
int
shf_finish(shf)
	struct shf *shf;
{
	int ret = 0;

	if (shf->fd >= 0)
		ret = shf_flush(shf);
	if (shf->flags & SHF_ALLOCS)
		afree(shf, shf->areap);
	else if (shf->flags & SHF_ALLOCB)
		afree(shf->buf, shf->areap);

	return ret;
}

/* Un-read what has been read but not examined, or write what has been
 * buffered.  Returns 0 for success, EOF for (write) error.
 */
int
shf_flush(shf)
	struct shf *shf;
{
	if (shf->flags & SHF_STRING)
		return (shf->flags & SHF_WR) ? EOF : 0;

	if (shf->fd < 0)
		internal_errorf(1, "shf_flush: no fd");

	if (shf->flags & SHF_ERROR) {
		errno = shf->errno_;
		return EOF;
	}

	if (shf->flags & SHF_READING) {
		shf->flags &= ~(SHF_EOF | SHF_READING);
		if (shf->rnleft > 0) {
			lseek(shf->fd, (off_t) -shf->rnleft, 1);
			shf->rnleft = 0;
			shf->rp = shf->buf;
		}
		return 0;
	} else if (shf->flags & SHF_WRITING)
		return shf_emptybuf(shf, 0);

	return 0;
}

/* Write out any buffered data.  If currently reading, flushes the read
 * buffer.  Returns 0 for success, EOF for (write) error.
 */
static int
shf_emptybuf(shf, flags)
	struct shf *shf;
	int flags;
{
	int ret = 0;

	if (!(shf->flags & SHF_STRING) && shf->fd < 0)
		internal_errorf(1, "shf_emptybuf: no fd");

	if (shf->flags & SHF_ERROR) {
		errno = shf->errno_;
		return EOF;
	}

	if (shf->flags & SHF_READING) {
		if (flags & EB_READSW) /* doesn't happen */
			return 0;
		ret = shf_flush(shf);
		shf->flags &= ~SHF_READING;
	}
	if (shf->flags & SHF_STRING) {
		unsigned char	*nbuf;

		/* Note that we assume SHF_ALLOCS is not set if SHF_ALLOCB
		 * is set... (changing the shf pointer could cause problems)
		 */
		if (!(flags & EB_GROW) || !(shf->flags & SHF_DYNAMIC)
		    || !(shf->flags & SHF_ALLOCB))
			return EOF;
		/* allocate more space for buffer */
		nbuf = (unsigned char *) aresize(shf->buf, shf->wbsize * 2,
						shf->areap);
		shf->rp = nbuf + (shf->rp - shf->buf);
		shf->wp = nbuf + (shf->wp - shf->buf);
		shf->rbsize += shf->wbsize;
		shf->wbsize += shf->wbsize;
		shf->wnleft += shf->wbsize;
		shf->wbsize *= 2;
		shf->buf = nbuf;
	} else {
		if (shf->flags & SHF_WRITING) {
			int ntowrite = shf->wp - shf->buf;
			unsigned char *buf = shf->buf;
			int n;

			while (ntowrite > 0) {
				n = write(shf->fd, buf, ntowrite);
				if (n < 0) {
					if (errno == EINTR
					    && !(shf->flags & SHF_INTERRUPT))
						continue;
					shf->flags |= SHF_ERROR;
					shf->errno_ = errno;
					shf->wnleft = 0;
					if (buf != shf->buf) {
						/* allow a second flush
						 * to work */
						memmove(shf->buf, buf,
							ntowrite);
						shf->wp = shf->buf + ntowrite;
					}
					return EOF;
				}
				buf += n;
				ntowrite -= n;
			}
			if (flags & EB_READSW) {
				shf->wp = shf->buf;
				shf->wnleft = 0;
				shf->flags &= ~SHF_WRITING;
				return 0;
			}
		}
		shf->wp = shf->buf;
		shf->wnleft = shf->wbsize;
	}
	shf->flags |= SHF_WRITING;

	return ret;
}

/* Fill up a read buffer.  Returns EOF for a read error, 0 otherwise. */
static int
shf_fillbuf(shf)
	struct shf *shf;
{
	if (shf->flags & SHF_STRING)
		return 0;

	if (shf->fd < 0)
		internal_errorf(1, "shf_fillbuf: no fd");

	if (shf->flags & (SHF_EOF | SHF_ERROR)) {
		if (shf->flags & SHF_ERROR)
			errno = shf->errno_;
		return EOF;
	}

	if ((shf->flags & SHF_WRITING) && shf_emptybuf(shf, EB_READSW) == EOF)
		return EOF;

	shf->flags |= SHF_READING;

	shf->rp = shf->buf;
	while (1) {
		shf->rnleft = blocking_read(shf->fd, (char *) shf->buf,
					    shf->rbsize);
		if (shf->rnleft < 0 && errno == EINTR
		    && !(shf->flags & SHF_INTERRUPT))
			continue;
		break;
	}
	if (shf->rnleft <= 0) {
		if (shf->rnleft < 0) {
			shf->flags |= SHF_ERROR;
			shf->errno_ = errno;
			shf->rnleft = 0;
			shf->rp = shf->buf;
			return EOF;
		}
		shf->flags |= SHF_EOF;
	}
	return 0;
}

/* Seek to a new position in the file.  If writing, flushes the buffer
 * first.  If reading, optimizes small relative seeks that stay inside the
 * buffer.  Returns 0 for success, EOF otherwise.
 */
int
shf_seek(shf, where, from)
	struct shf *shf;
	off_t where;
	int from;
{
	if (shf->fd < 0) {
		errno = EINVAL;
		return EOF;
	}

	if (shf->flags & SHF_ERROR) {
		errno = shf->errno_;
		return EOF;
	}

	if ((shf->flags & SHF_WRITING) && shf_emptybuf(shf, EB_READSW) == EOF)
		return EOF;

	if (shf->flags & SHF_READING) {
		if (from == SEEK_CUR &&
				(where < 0 ?
					-where >= shf->rbsize - shf->rnleft :
					where < shf->rnleft)) {
			shf->rnleft -= where;
			shf->rp += where;
			return 0;
		}
		shf->rnleft = 0;
		shf->rp = shf->buf;
	}

	shf->flags &= ~(SHF_EOF | SHF_READING | SHF_WRITING);

	if (lseek(shf->fd, where, from) < 0) {
		shf->errno_ = errno;
		shf->flags |= SHF_ERROR;
		return EOF;
	}

	return 0;
}


/* Read a buffer from shf.  Returns the number of bytes read into buf,
 * if no bytes were read, returns 0 if end of file was seen, EOF if
 * a read error occurred.
 */
int
shf_read(buf, bsize, shf)
	char *buf;
	int bsize;
	struct shf *shf;
{
	int orig_bsize = bsize;
	int ncopy;

	if (!(shf->flags & SHF_RD))
		internal_errorf(1, "shf_read: flags %x", shf->flags);

	if (bsize <= 0)
		internal_errorf(1, "shf_read: bsize %d", bsize);

	while (bsize > 0) {
		if (shf->rnleft == 0
		    && (shf_fillbuf(shf) == EOF || shf->rnleft == 0))
			break;
		ncopy = shf->rnleft;
		if (ncopy > bsize)
			ncopy = bsize;
		memcpy(buf, shf->rp, ncopy);
		buf += ncopy;
		bsize -= ncopy;
		shf->rp += ncopy;
		shf->rnleft -= ncopy;
	}
	/* Note: fread(3S) returns 0 for errors - this doesn't */
	return orig_bsize == bsize ? (shf_error(shf) ? EOF : 0)
				   : orig_bsize - bsize;
}

/* Read up to a newline or EOF.  The newline is put in buf; buf is always
 * null terminated.  Returns NULL on read error or if nothing was read before
 * end of file, returns a pointer to the null byte in buf otherwise.
 */
char *
shf_getse(buf, bsize, shf)
	char *buf;
	int bsize;
	struct shf *shf;
{
	unsigned char *end;
	int ncopy;
	char *orig_buf = buf;

	if (!(shf->flags & SHF_RD))
		internal_errorf(1, "shf_getse: flags %x", shf->flags);

	if (bsize <= 0)
		return (char *) 0;

	--bsize;	/* save room for null */
	do {
		if (shf->rnleft == 0) {
			if (shf_fillbuf(shf) == EOF)
				return NULL;
			if (shf->rnleft == 0) {
				*buf = '\0';
				return buf == orig_buf ? NULL : buf;
			}
		}
		end = (unsigned char *) memchr((char *) shf->rp, '\n',
					     shf->rnleft);
		ncopy = end ? end - shf->rp + 1 : shf->rnleft;
		if (ncopy > bsize)
			ncopy = bsize;
		memcpy(buf, (char *) shf->rp, ncopy);
		shf->rp += ncopy;
		shf->rnleft -= ncopy;
		buf += ncopy;
		bsize -= ncopy;
	} while (!end && bsize);
	*buf = '\0';
	return buf;
}

/* Returns the char read.  Returns EOF for error and end of file. */
int
shf_getchar(shf)
	struct shf *shf;
{
	if (!(shf->flags & SHF_RD))
		internal_errorf(1, "shf_getchar: flags %x", shf->flags);

	if (shf->rnleft == 0 && (shf_fillbuf(shf) == EOF || shf->rnleft == 0))
		return EOF;
	--shf->rnleft;
	return *shf->rp++;
}

/* Put a character back in the input stream.  Returns the character if
 * successful, EOF if there is no room.
 */
int
shf_ungetc(c, shf)
	int c;
	struct shf *shf;
{
	if (!(shf->flags & SHF_RD))
		internal_errorf(1, "shf_ungetc: flags %x", shf->flags);

	if ((shf->flags & SHF_ERROR) || c == EOF
	    || (shf->rp == shf->buf && shf->rnleft))
		return EOF;

	if ((shf->flags & SHF_WRITING) && shf_emptybuf(shf, EB_READSW) == EOF)
		return EOF;

	if (shf->rp == shf->buf)
		shf->rp = shf->buf + shf->rbsize;
	if (shf->flags & SHF_STRING) {
		/* Can unget what was read, but not something different - we
		 * don't want to modify a string.
		 */
		if (shf->rp[-1] != c)
			return EOF;
		shf->flags &= ~SHF_EOF;
		shf->rp--;
		shf->rnleft++;
		return c;
	}
	shf->flags &= ~SHF_EOF;
	*--(shf->rp) = c;
	shf->rnleft++;
	return c;
}

/* Write a character.  Returns the character if successful, EOF if
 * the char could not be written.
 */
int
shf_putchar(c, shf)
	int c;
	struct shf *shf;
{
	if (!(shf->flags & SHF_WR))
		internal_errorf(1, "shf_putchar: flags %x", shf->flags);

	if (c == EOF)
		return EOF;

	if (shf->flags & SHF_UNBUF) {
		char cc = c;
		int n;

		if (shf->fd < 0)
			internal_errorf(1, "shf_putchar: no fd");
		if (shf->flags & SHF_ERROR) {
			errno = shf->errno_;
			return EOF;
		}
		while ((n = write(shf->fd, &cc, 1)) != 1)
			if (n < 0) {
				if (errno == EINTR
				    && !(shf->flags & SHF_INTERRUPT))
					continue;
				shf->flags |= SHF_ERROR;
				shf->errno_ = errno;
				return EOF;
			}
	} else {
		/* Flush deals with strings and sticky errors */
		if (shf->wnleft == 0 && shf_emptybuf(shf, EB_GROW) == EOF)
			return EOF;
		shf->wnleft--;
		*shf->wp++ = c;
	}

	return c;
}

/* Write a string.  Returns the length of the string if successful, EOF if
 * the string could not be written.
 */
int
shf_puts(s, shf)
	const char *s;
	struct shf *shf;
{
	if (!s)
		return EOF;

	return shf_write(s, strlen(s), shf);
}

/* Write a buffer.  Returns nbytes if successful, EOF if there is an error. */
int
shf_write(buf, nbytes, shf)
	const char *buf;
	int nbytes;
	struct shf *shf;
{
	int orig_nbytes = nbytes;
	int n;
	int ncopy;

	if (!(shf->flags & SHF_WR))
		internal_errorf(1, "shf_write: flags %x", shf->flags);

	if (nbytes < 0)
		internal_errorf(1, "shf_write: nbytes %d", nbytes);

	if ((ncopy = shf->wnleft)) {
		if (ncopy > nbytes)
			ncopy = nbytes;
		memcpy(shf->wp, buf, ncopy);
		nbytes -= ncopy;
		buf += ncopy;
		shf->wp += ncopy;
		shf->wnleft -= ncopy;
	}
	if (nbytes > 0) {
		/* Flush deals with strings and sticky errors */
		if (shf_emptybuf(shf, EB_GROW) == EOF)
			return EOF;
		if (nbytes > shf->wbsize) {
			ncopy = nbytes;
			if (shf->wbsize)
				ncopy -= nbytes % shf->wbsize;
			nbytes -= ncopy;
			while (ncopy > 0) {
				n = write(shf->fd, buf, ncopy);
				if (n < 0) {
					if (errno == EINTR
					    && !(shf->flags & SHF_INTERRUPT))
						continue;
					shf->flags |= SHF_ERROR;
					shf->errno_ = errno;
					shf->wnleft = 0;
					/* Note: fwrite(3S) returns 0 for
					 * errors - this doesn't */
					return EOF;
				}
				buf += n;
				ncopy -= n;
			}
		}
		if (nbytes > 0) {
			memcpy(shf->wp, buf, nbytes);
			shf->wp += nbytes;
			shf->wnleft -= nbytes;
		}
	}

	return orig_nbytes;
}

int
#ifdef HAVE_PROTOTYPES
shf_fprintf(struct shf *shf, const char *fmt, ...)
#else
shf_fprintf(shf, fmt, va_alist)
	struct shf *shf;
	const char *fmt;
	va_dcl
#endif
{
	va_list args;
	int n;

	SH_VA_START(args, fmt);
	n = shf_vfprintf(shf, fmt, args);
	va_end(args);

	return n;
}

int
#ifdef HAVE_PROTOTYPES
shf_snprintf(char *buf, int bsize, const char *fmt, ...)
#else
shf_snprintf(buf, bsize, fmt, va_alist)
	char *buf;
	int bsize;
	const char *fmt;
	va_dcl
#endif
{
	struct shf shf;
	va_list args;
	int n;

	if (!buf || bsize <= 0)
		internal_errorf(1, "shf_snprintf: buf %lx, bsize %d",
			(long) buf, bsize);

	shf_sopen(buf, bsize, SHF_WR, &shf);
	SH_VA_START(args, fmt);
	n = shf_vfprintf(&shf, fmt, args);
	va_end(args);
	shf_sclose(&shf); /* null terminates */
	return n;
}

char *
#ifdef HAVE_PROTOTYPES
shf_smprintf(const char *fmt, ...)
#else
shf_smprintf(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	struct shf shf;
	va_list args;

	shf_sopen((char *) 0, 0, SHF_WR|SHF_DYNAMIC, &shf);
	SH_VA_START(args, fmt);
	shf_vfprintf(&shf, fmt, args);
	va_end(args);
	return shf_sclose(&shf); /* null terminates */
}

#undef FP  			/* if you want floating point stuff */

#define BUF_SIZE	128
#define FPBUF_SIZE	(DMAXEXP+16)/* this must be >
				 *	MAX(DMAXEXP, log10(pow(2, DSIGNIF)))
				 *    + ceil(log10(DMAXEXP)) + 8 (I think).
				 * Since this is hard to express as a
				 * constant, just use a large buffer.
				 */

/*
 *	What kinda of machine we on?  Hopefully the C compiler will optimize
 *  this out...
 *
 *	For shorts, we want sign extend for %d but not for %[oxu] - on 16 bit
 *  machines it don't matter.  Assmumes C compiler has converted shorts to
 *  ints before pushing them.
 */
#define POP_INT(f, s, a) (((f) & FL_LONG) ?				\
				va_arg((a), unsigned long)		\
			    :						\
				(sizeof(int) < sizeof(long) ?		\
					((s) ?				\
						(long) va_arg((a), int)	\
					    :				\
						va_arg((a), unsigned))	\
				    :					\
					va_arg((a), unsigned)))

#define ABIGNUM		32000	/* big numer that will fit in a short */
#define LOG2_10		3.321928094887362347870319429	/* log base 2 of 10 */

#define	FL_HASH		0x001	/* `#' seen */
#define FL_PLUS		0x002	/* `+' seen */
#define FL_RIGHT	0x004	/* `-' seen */
#define FL_BLANK	0x008	/* ` ' seen */
#define FL_SHORT	0x010	/* `h' seen */
#define FL_LONG		0x020	/* `l' seen */
#define FL_ZERO		0x040	/* `0' seen */
#define FL_DOT		0x080	/* '.' seen */
#define FL_UPPER	0x100	/* format character was uppercase */
#define FL_NUMBER	0x200	/* a number was formated %[douxefg] */


#ifdef FP
#include <math.h>

static double
my_ceil(d)
	double	d;
{
	double		i;

	return d - modf(d, &i) + (d < 0 ? -1 : 1);
}
#endif /* FP */

int
shf_vfprintf(shf, fmt, args)
	struct shf *shf;
	const char *fmt;
	va_list args;
{
	char		c, *s;
	int		UNINITIALIZED(tmp);
	int		field, precision;
	int		len;
	int		flags;
	unsigned long	lnum;
					/* %#o produces the longest output */
	char		numbuf[(BITS(long) + 2) / 3 + 1];
	/* this stuff for dealing with the buffer */
	int		nwritten = 0;
#ifdef FP
	/* should be in <math.h>
	 *  extern double frexp();
	 */
	extern char *ecvt();

	double		fpnum;
	int		expo, decpt;
	char		style;
	char		fpbuf[FPBUF_SIZE];
#endif /* FP */

	if (!fmt)
		return 0;

	while ((c = *fmt++)) {
		if (c != '%') {
			shf_putc(c, shf);
			nwritten++;
			continue;
		}
		/*
		 *	This will accept flags/fields in any order - not
		 *  just the order specified in printf(3), but this is
		 *  the way _doprnt() seems to work (on bsd and sysV).
		 *  The only resriction is that the format character must
		 *  come last :-).
		 */
		flags = field = precision = 0;
		for ( ; (c = *fmt++) ; ) {
			switch (c) {
			case '#':
				flags |= FL_HASH;
				continue;

			case '+':
				flags |= FL_PLUS;
				continue;

			case '-':
				flags |= FL_RIGHT;
				continue;

			case ' ':
				flags |= FL_BLANK;
				continue;

			case '0':
				if (!(flags & FL_DOT))
					flags |= FL_ZERO;
				continue;

			case '.':
				flags |= FL_DOT;
				precision = 0;
				continue;

			case '*':
				tmp = va_arg(args, int);
				if (flags & FL_DOT)
					precision = tmp;
				else if ((field = tmp) < 0) {
					field = -field;
					flags |= FL_RIGHT;
				}
				continue;

			case 'l':
				flags |= FL_LONG;
				continue;

			case 'h':
				flags |= FL_SHORT;
				continue;
			}
			if (digit(c)) {
				tmp = c - '0';
				while (c = *fmt++, digit(c))
					tmp = tmp * 10 + c - '0';
				--fmt;
				if (tmp < 0)		/* overflow? */
					tmp = 0;
				if (flags & FL_DOT)
					precision = tmp;
				else
					field = tmp;
				continue;
			}
			break;
		}

		if (precision < 0)
			precision = 0;

		if (!c)		/* nasty format */
			break;

		if (c >= 'A' && c <= 'Z') {
			flags |= FL_UPPER;
			c = c - 'A' + 'a';
		}

		switch (c) {
		case 'p': /* pointer */
			flags &= ~(FL_LONG | FL_SHORT);
			if (sizeof(char *) > sizeof(int))
				flags |= FL_LONG; /* hope it fits.. */
			/* aaahhh... */
		case 'd':
		case 'i':
		case 'o':
		case 'u':
		case 'x':
			flags |= FL_NUMBER;
			s = &numbuf[sizeof(numbuf)];
			lnum = POP_INT(flags, c == 'd', args);
			switch (c) {
			case 'd':
			case 'i':
				if (0 > (long) lnum)
					lnum = - (long) lnum, tmp = 1;
				else
					tmp = 0;
				/* aaahhhh..... */

			case 'u':
				do {
					*--s = lnum % 10 + '0';
					lnum /= 10;
				} while (lnum);

				if (c != 'u') {
					if (tmp)
						*--s = '-';
					else if (flags & FL_PLUS)
						*--s = '+';
					else if (flags & FL_BLANK)
						*--s = ' ';
				}
				break;

			case 'o':
				do {
					*--s = (lnum & 0x7) + '0';
					lnum >>= 3;
				} while (lnum);

				if ((flags & FL_HASH) && *s != '0')
					*--s = '0';
				break;

			case 'p':
			case 'x':
			    {
				const char *digits = (flags & FL_UPPER) ?
						  "0123456789ABCDEF"
						: "0123456789abcdef";
				do {
					*--s = digits[lnum & 0xf];
					lnum >>= 4;
				} while (lnum);

				if (flags & FL_HASH) {
					*--s = (flags & FL_UPPER) ? 'X' : 'x';
					*--s = '0';
				}
			    }
			}
			len = &numbuf[sizeof(numbuf)] - s;
			if (flags & FL_DOT) {
				if (precision > len) {
					field = precision;
					flags |= FL_ZERO;
				} else
					precision = len; /* no loss */
			}
			break;

#ifdef FP
		case 'e':
		case 'g':
		case 'f':
		    {
			char *p;

			/*
			 *	This could proabably be done better,
			 *  but it seems to work.  Note that gcvt()
			 *  is not used, as you cannot tell it to
			 *  not strip the zeros.
			 */
			flags |= FL_NUMBER;
			if (!(flags & FL_DOT))
				precision = 6;	/* default */
			/*
			 *	Assumes doubles are pushed on
			 *  the stack.  If this is not so, then
			 *  FL_LONG/FL_SHORT should be checked.
			 */
			fpnum = va_arg(args, double);
			s = fpbuf;
			style = c;
			/*
			 *  This is the same as
			 *	expo = ceil(log10(fpnum))
			 *  but doesn't need -lm.  This is an
			 *  aproximation as expo is rounded up.
			 */
			(void) frexp(fpnum, &expo);
			expo = my_ceil(expo / LOG2_10);

			if (expo < 0)
				expo = 0;

			p = ecvt(fpnum, precision + 1 + expo,
				 &decpt, &tmp);
			if (c == 'g') {
				if (decpt < -4 || decpt > precision)
					style = 'e';
				else
					style = 'f';
				if (decpt > 0 && (precision -= decpt) < 0)
					precision = 0;
			}
			if (tmp)
				*--s = '-';
			else if (flags & FL_PLUS)
				*--s = '+';
			else if (flags & FL_BLANK)
				*--s = ' ';

			if (style == 'e')
				*s++ = *p++;
			else {
				if (decpt > 0) {
					/* Overflow check - should
					 * never have this problem.
					 */
					if (decpt >
						&fpbuf[sizeof(fpbuf)]
							- s - 8)
						decpt =
						 &fpbuf[sizeof(fpbuf)]
							- s - 8;
					(void) memcpy(s, p, decpt);
					s += decpt;
					p += decpt;
				} else
					*s++ = '0';
			}

			/* print the fraction? */
			if (precision > 0) {
				*s++ = '.';
				/* Overflow check - should
				 * never have this problem.
				 */
				if (precision > &fpbuf[sizeof(fpbuf)]
							- s - 7)
					precision =
						&fpbuf[sizeof(fpbuf)]
						- s - 7;
				for (tmp = decpt;  tmp++ < 0 &&
					    precision > 0 ; precision--)
					*s++ = '0';
				tmp = strlen(p);
				if (precision > tmp)
					precision = tmp;
				/* Overflow check - should
				 * never have this problem.
				 */
				if (precision > &fpbuf[sizeof(fpbuf)]
							- s - 7)
					precision =
						&fpbuf[sizeof(fpbuf)]
						- s - 7;
				(void) memcpy(s, p, precision);
				s += precision;
				/*
				 *	`g' format strips trailing
				 *  zeros after the decimal.
				 */
				if (c == 'g' && !(flags & FL_HASH)) {
					while (*--s == '0')
						;
					if (*s != '.')
						s++;
				}
			} else if (flags & FL_HASH)
				*s++ = '.';

			if (style == 'e') {
				*s++ = (flags & FL_UPPER) ? 'E' : 'e';
				if (--decpt >= 0)
					*s++ = '+';
				else {
					*s++ = '-';
					decpt = -decpt;
				}
				p = &numbuf[sizeof(numbuf)];
				for (tmp = 0; tmp < 2 || decpt ; tmp++) {
					*--p = '0' + decpt % 10;
					decpt /= 10;
				}
				tmp = &numbuf[sizeof(numbuf)] - p;
				(void) memcpy(s, p, tmp);
				s += tmp;
			}

			len = s - fpbuf;
			s = fpbuf;
			precision = len;
			break;
		    }
#endif /* FP */

		case 's':
			if (!(s = va_arg(args, char *)))
				s = "(null %s)";
			len = strlen(s);
			break;

		case 'c':
			flags &= ~FL_DOT;
			numbuf[0] = va_arg(args, int);
			s = numbuf;
			len = 1;
			break;

		case '%':
		default:
			numbuf[0] = c;
			s = numbuf;
			len = 1;
			break;
		}

		/*
		 *	At this point s should point to a string that is
		 *  to be formatted, and len should be the length of the
		 *  string.
		 */
		if (!(flags & FL_DOT) || len < precision)
			precision = len;
		if (field > precision) {
			field -= precision;
			if (!(flags & FL_RIGHT)) {
				field = -field;
				/* skip past sign or 0x when padding with 0 */
				if ((flags & FL_ZERO) && (flags & FL_NUMBER)) {
					if (*s == '+' || *s == '-' || *s ==' ')
					{
						shf_putc(*s, shf);
						s++;
						precision--;
						nwritten++;
					} else if (*s == '0') {
						shf_putc(*s, shf);
						s++;
						nwritten++;
						if (--precision > 0 &&
							(*s | 0x20) == 'x')
						{
							shf_putc(*s, shf);
							s++;
							precision--;
							nwritten++;
						}
					}
					c = '0';
				} else
					c = flags & FL_ZERO ? '0' : ' ';
				if (field < 0) {
					nwritten += -field;
					for ( ; field < 0 ; field++)
						shf_putc(c, shf);
				}
			} else
				c = ' ';
		} else
			field = 0;

		if (precision > 0) {
			nwritten += precision;
			for ( ; precision-- > 0 ; s++)
				shf_putc(*s, shf);
		}
		if (field > 0) {
			nwritten += field;
			for ( ; field > 0 ; --field)
				shf_putc(c, shf);
		}
	}

	return shf_error(shf) ? EOF : nwritten;
}
