/*	$OpenBSD: compress.c,v 1.6 2003/03/03 22:24:08 ian Exp $	*/

/*
 * compress routines:
 *	zmagic() - returns 0 if not recognized, uncompresses and prints
 *		   information if recognized
 *	uncompress(method, old, n, newch) - uncompress old into new, 
 *					    using method, return sizeof new
 */
#include "file.h"
#include <stdlib.h>
#ifdef	HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#ifdef	HAVE_LIBZ
#include <zlib.h>
#endif

static struct {
   const char *magic;
   int   maglen;
   const char *const argv[3];
   int	 silent;
} compr[] = {
    { "\037\235", 2, { "uncompress", "-c", NULL }, 0 },	/* compressed */
    { "\037\213", 2, { "gzip", "-cdq", NULL }, 1 },	/* gzipped */
    { "\037\236", 2, { "gzip", "-cdq", NULL }, 1 },	/* frozen */
    { "\037\240", 2, { "gzip", "-cdq", NULL }, 1 },	/* SCO LZH */
    /* the standard pack utilities do not accept standard input */
    { "\037\036", 2, { "gzip", "-cdq", NULL }, 0 },	/* packed */
};

static int ncompr = sizeof(compr) / sizeof(compr[0]);


static int swrite(int, const void *, size_t);
static int sread(int, void *, size_t);
static int uncompress(int, const unsigned char *, unsigned char **, int);

int
zmagic(buf, nbytes)
unsigned char *buf;
int nbytes;
{
	unsigned char *newbuf;
	int newsize;
	int i;

	for (i = 0; i < ncompr; i++) {
		if (nbytes < compr[i].maglen)
			continue;
		if (memcmp(buf, compr[i].magic,  compr[i].maglen) == 0)
			break;
	}

	if (i == ncompr)
		return 0;

	if ((newsize = uncompress(i, buf, &newbuf, nbytes)) != 0) {
		tryit(newbuf, newsize, 1);
		free(newbuf);
		printf(" (");
		tryit(buf, nbytes, 0);
		printf(")");
	}
	return 1;
}

/*
 * `safe' write for sockets and pipes.
 */
static int
swrite(int fd, const void *buf, size_t n)
{
	int rv;
	size_t rn = n;

	do
		switch (rv = write(fd, buf, n)) {
		case -1:
			if (errno == EINTR)
				continue;
			return -1;
		default:
			n -= rv;
			buf = ((const char *)buf) + rv;
			break;
		}
	while (n > 0);
	return rn;
}

/*
 * `safe' read for sockets and pipes.
 */
static int
sread(int fd, void *buf, size_t n)
{
	int rv;
	size_t rn = n;

	do
		switch (rv = read(fd, buf, n)) {
		case -1:
			if (errno == EINTR)
				continue;
			return -1;
		case 0:
			return rn - n;
		default:
			n -= rv;
			buf = ((char *)buf) + rv;
			break;
		}
	while (n > 0);
	return rn;
}

int
pipe2file(int fd, void *startbuf, size_t nbytes)
{
	char buf[4096];
	int r, tfd;

	(void)strcpy(buf, "/tmp/file.XXXXXX");
#ifndef HAVE_MKSTEMP
	{
		char *ptr = mktemp(buf);
		tfd = open(ptr, O_RDWR|O_TRUNC|O_EXCL|O_CREAT, 0600);
		r = errno;
		(void)unlink(ptr);
		errno = r;
	}
#else
	tfd = mkstemp(buf);
	r = errno;
	(void)unlink(buf);
	errno = r;
#endif
	if (tfd == -1) {
		error("Can't create temporary file for pipe copy (%s)\n",
		    strerror(errno));
		/*NOTREACHED*/
	}

	if (swrite(tfd, startbuf, nbytes) != nbytes)
		r = 1;
	else {
		while ((r = sread(fd, buf, sizeof(buf))) > 0)
			if (swrite(tfd, buf, r) != r)
				break;
	}

	switch (r) {
	case -1:
		error("Error copying from pipe to temp file (%s)\n",
		    strerror(errno));
		/*NOTREACHED*/
	case 0:
		break;
	default:
		error("Error while writing to temp file (%s)\n",
		    strerror(errno));
		/*NOTREACHED*/
	}

	/*
	 * We duplicate the file descriptor, because fclose on a
	 * tmpfile will delete the file, but any open descriptors
	 * can still access the phantom inode.
	 */
	if ((fd = dup2(tfd, fd)) == -1) {
		error("Couldn't dup destcriptor for temp file(%s)\n",
		    strerror(errno));
		/*NOTREACHED*/
	}
	(void)close(tfd);
	if (lseek(fd, (off_t)0, SEEK_SET) == (off_t)-1) {
		error("Couldn't seek on temp file (%s)\n", strerror(errno));
		/*NOTREACHED*/
	}
	return fd;
}

static int
uncompress(method, old, newch, n)
int method;
const unsigned char *old;
unsigned char **newch;
int n;
{
	int fdin[2], fdout[2];

	if (pipe(fdin) == -1 || pipe(fdout) == -1) {
		err(1, "cannot create pipe");	
		/*NOTREACHED*/
	}
	switch (fork()) {
	case 0:	/* child */
		(void) close(0);
		(void) dup(fdin[0]);
		(void) close(fdin[0]);
		(void) close(fdin[1]);

		(void) close(1);
		(void) dup(fdout[1]);
		(void) close(fdout[0]);
		(void) close(fdout[1]);
		if (compr[method].silent)
		    (void) close(2);

		execvp(compr[method].argv[0], (char *const *)compr[method].argv);
		err(1, "could not execute `%s'", compr[method].argv[0]);
		/*NOTREACHED*/
	case -1:
		err(1, "could not fork");
		/*NOTREACHED*/

	default: /* parent */
		(void) close(fdin[0]);
		(void) close(fdout[1]);
		if (write(fdin[1], old, n) != n) {
			err(1, "write failed");
			/*NOTREACHED*/
		}
		(void) close(fdin[1]);
		if ((*newch = (unsigned char *) malloc(n)) == NULL) {
			err(1, "malloc");
			/*NOTREACHED*/
		}
		if ((n = read(fdout[0], *newch, n)) <= 0) {
			free(*newch);
			err(1, "read failed");
			/*NOTREACHED*/
		}
		(void) close(fdout[0]);
		(void) wait(NULL);
		return n;
	}
}
