/* mdXhl.c
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: mdXhl.c,v 1.10 2002/12/23 04:33:31 millert Exp $";
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
MDXEnd(MDX_CTX *ctx, char *buf)
{
    int i;
    unsigned char digest[16];
    static const char hex[]="0123456789abcdef";

    if (buf == NULL && (buf = malloc(33)) == NULL)
	return(NULL);
    MDXFinal(digest,ctx);
    for (i=0;i<16;i++) {
	buf[i+i] = hex[digest[i] >> 4];
	buf[i+i+1] = hex[digest[i] & 0x0f];
    }
    buf[i+i] = '\0';
    return(buf);
}

char *
MDXFile(char *filename, char *buf)
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
    return(MDXEnd(&ctx, buf));
}

char *
MDXData(const u_char *data, size_t len, char *buf)
{
    MDX_CTX ctx;

    MDXInit(&ctx);
    MDXUpdate(&ctx,data,len);
    return(MDXEnd(&ctx, buf));
}
