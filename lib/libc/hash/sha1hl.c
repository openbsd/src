/* sha1hl.c
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: sha1hl.c,v 1.2 1999/08/17 09:13:12 millert Exp $";
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
SHA1End(ctx, buf)
    SHA1_CTX *ctx;
    char *buf;
{
    int i;
    char *p = buf;
    u_char digest[20];
    static const char hex[]="0123456789abcdef";

    if (p == NULL && (p = malloc(41)) == NULL)
	return 0;

    SHA1Final(digest,ctx);
    for (i = 0; i < 20; i++) {
	p[i + i] = hex[digest[i] >> 4];
	p[i + i + 1] = hex[digest[i] & 0x0f];
    }
    p[i + i] = '\0';
    return(p);
}

char *
SHA1File (filename, buf)
    char *filename;
    char *buf;
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
SHA1Data (data, len, buf)
    const u_char *data;
    size_t len;
    char *buf;
{
    SHA1_CTX ctx;

    SHA1Init(&ctx);
    SHA1Update(&ctx, data, len);
    return(SHA1End(&ctx, buf));
}
