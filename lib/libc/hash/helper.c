/*	$OpenBSD: helper.c,v 1.5 2004/05/03 17:30:14 millert Exp $	*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$OpenBSD: helper.c,v 1.5 2004/05/03 17:30:14 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <hashinc>

/* ARGSUSED */
char *
HASHEnd(HASH_CTX *ctx, char *buf)
{
	int i;
	u_int8_t digest[HASH_DIGEST_LENGTH];
	static const char hex[] = "0123456789abcdef";

	if (buf == NULL && (buf = malloc(HASH_DIGEST_STRING_LENGTH)) == NULL)
		return (NULL);

	HASHFinal(digest, ctx);
	for (i = 0; i < HASH_DIGEST_LENGTH; i++) {
		buf[i + i] = hex[digest[i] >> 4];
		buf[i + i + 1] = hex[digest[i] & 0x0f];
	}
	buf[i + i] = '\0';
	memset(digest, 0, sizeof(digest));
	return (buf);
}

char *
HASHFileChunk(char *filename, char *buf, off_t off, off_t len)
{
	u_char buffer[BUFSIZ];
	HASH_CTX ctx;
	int fd, save_errno;
	ssize_t nr;

	HASHInit(&ctx);

	if ((fd = open(filename, O_RDONLY)) < 0)
		return (NULL);
	if (off > 0 && lseek(fd, off, SEEK_SET) < 0)
		return (NULL);

	while ((nr = read(fd, buffer, MIN(sizeof(buffer), len))) > 0) {
		HASHUpdate(&ctx, buffer, (size_t)nr);
		if (len > 0 && (len -= nr) == 0)
			break;
	}

	save_errno = errno;
	close(fd);
	errno = save_errno;
	return (nr < 0 ? NULL : HASHEnd(&ctx, buf));
}

char *
HASHFile(char *filename, char *buf)
{
	return (HASHFileChunk(filename, buf, (off_t)0, (off_t)0));
}

char *
HASHData(const u_char *data, size_t len, char *buf)
{
	HASH_CTX ctx;

	HASHInit(&ctx);
	HASHUpdate(&ctx, data, len);
	return (HASHEnd(&ctx, buf));
}
