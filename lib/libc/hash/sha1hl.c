/* sha1hl.c
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: sha1hl.c,v 1.3 2002/12/23 04:33:31 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sha1.h>

/* ARGSUSED */
char *
SHA1End(SHA1_CTX *ctx, char *buf)
{
    int i;
    u_char digest[20];
    static const char hex[]="0123456789abcdef";

    if (buf == NULL && (buf = malloc(41)) == NULL)
	return(NULL);

    SHA1Final(digest,ctx);
    for (i = 0; i < 20; i++) {
	buf[i + i] = hex[digest[i] >> 4];
	buf[i + i + 1] = hex[digest[i] & 0x0f];
    }
    buf[i + i] = '\0';
    return(buf);
}

char *
SHA1File (char *filename, char *buf)
{
    u_char buffer[BUFSIZ];
    SHA1_CTX ctx;
    int fd, num, oerrno;

    SHA1Init(&ctx);

    if ((fd = open(filename, O_RDONLY)) < 0)
	return(0);

    while ((num = read(fd, buffer, sizeof(buffer))) > 0)
	SHA1Update(&ctx, buffer, num);

    oerrno = errno;
    close(fd);
    errno = oerrno;
    return(num < 0 ? 0 : SHA1End(&ctx, buf));
}

char *
SHA1Data (const u_char *data, size_t len, char *buf)
{
    SHA1_CTX ctx;

    SHA1Init(&ctx);
    SHA1Update(&ctx, data, len);
    return(SHA1End(&ctx, buf));
}
