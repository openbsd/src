/* mdXhl.c
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: mdXhl.c,v 1.9 1999/08/17 09:13:12 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <mdX.h>

/* ARGSUSED */
char *
MDXEnd(ctx, buf)
    MDX_CTX *ctx;
    char *buf;
{
    int i;
    char *p = buf;
    unsigned char digest[16];
    static const char hex[]="0123456789abcdef";

    if (!p)
	p = malloc(33);
    if (!p)
	return 0;
    MDXFinal(digest,ctx);
    for (i=0;i<16;i++) {
	p[i+i] = hex[digest[i] >> 4];
	p[i+i+1] = hex[digest[i] & 0x0f];
    }
    p[i+i] = '\0';
    return p;
}

char *
MDXFile (filename, buf)
    char *filename;
    char *buf;
{
    unsigned char buffer[BUFSIZ];
    MDX_CTX ctx;
    int f,i,j;

    MDXInit(&ctx);
    f = open(filename, O_RDONLY);
    if (f < 0) return 0;
    while ((i = read(f,buffer,sizeof buffer)) > 0) {
	MDXUpdate(&ctx,buffer,i);
    }
    j = errno;
    close(f);
    errno = j;
    if (i < 0) return 0;
    return MDXEnd(&ctx, buf);
}

char *
MDXData (data, len, buf)
    const unsigned char *data;
    size_t len;
    char *buf;
{
    MDX_CTX ctx;

    MDXInit(&ctx);
    MDXUpdate(&ctx,data,len);
    return MDXEnd(&ctx, buf);
}
