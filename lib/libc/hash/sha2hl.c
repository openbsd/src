/* sha2hl.c
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$OpenBSD: sha2hl.c,v 1.2 2003/05/09 16:46:31 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <sha2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char hex[]="0123456789abcdef";

/* ARGSUSED */
char *
SHA256_End(SHA256_CTX *ctx, char buf[SHA256_DIGEST_STRING_LENGTH])
{
    int i;
    u_int8_t digest[SHA256_DIGEST_LENGTH];

    if (buf == NULL && (buf = malloc(SHA256_DIGEST_STRING_LENGTH)) == NULL)
	return(NULL);

    SHA256_Final(digest, ctx);
    for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
	buf[i + i] = hex[digest[i] >> 4];
	buf[i + i + 1] = hex[digest[i] & 0x0f];
    }
    buf[i + i] = '\0';
    memset(digest, 0, sizeof(digest));
    return(buf);
}

char *
SHA256_File(char *filename, char buf[SHA256_DIGEST_STRING_LENGTH])
{
    u_int8_t buffer[BUFSIZ];
    SHA256_CTX ctx;
    int fd, num, oerrno;

    SHA256_Init(&ctx);

    if ((fd = open(filename, O_RDONLY)) < 0)
	return(0);

    while ((num = read(fd, buffer, sizeof(buffer))) > 0)
	SHA256_Update(&ctx, buffer, num);

    oerrno = errno;
    close(fd);
    errno = oerrno;
    return(num < 0 ? 0 : SHA256_End(&ctx, buf));
}

char *
SHA256_Data(const u_int8_t *data, size_t len, char buf[SHA256_DIGEST_STRING_LENGTH])
{
    SHA256_CTX ctx;

    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, len);
    return(SHA256_End(&ctx, buf));
}

/* ARGSUSED */
char *
SHA384_End(SHA384_CTX *ctx, char buf[SHA384_DIGEST_STRING_LENGTH])
{
    int i;
    u_int8_t digest[SHA384_DIGEST_LENGTH];

    if (buf == NULL && (buf = malloc(SHA384_DIGEST_STRING_LENGTH)) == NULL)
	return(NULL);

    SHA384_Final(digest, ctx);
    for (i = 0; i < SHA384_DIGEST_LENGTH; i++) {
	buf[i + i] = hex[digest[i] >> 4];
	buf[i + i + 1] = hex[digest[i] & 0x0f];
    }
    buf[i + i] = '\0';
    memset(digest, 0, sizeof(digest));
    return(buf);
}

char *
SHA384_File(char *filename, char buf[SHA384_DIGEST_STRING_LENGTH])
{
    u_int8_t buffer[BUFSIZ];
    SHA384_CTX ctx;
    int fd, num, oerrno;

    SHA384_Init(&ctx);

    if ((fd = open(filename, O_RDONLY)) < 0)
	return(0);

    while ((num = read(fd, buffer, sizeof(buffer))) > 0)
	SHA384_Update(&ctx, buffer, num);

    oerrno = errno;
    close(fd);
    errno = oerrno;
    return(num < 0 ? 0 : SHA384_End(&ctx, buf));
}

char *
SHA384_Data(const u_int8_t *data, size_t len, char buf[SHA384_DIGEST_STRING_LENGTH])
{
    SHA384_CTX ctx;

    SHA384_Init(&ctx);
    SHA384_Update(&ctx, data, len);
    return(SHA384_End(&ctx, buf));
}

/* ARGSUSED */
char *
SHA512_End(SHA512_CTX *ctx, char buf[SHA512_DIGEST_STRING_LENGTH])
{
    int i;
    u_int8_t digest[SHA512_DIGEST_LENGTH];

    if (buf == NULL && (buf = malloc(SHA512_DIGEST_STRING_LENGTH)) == NULL)
	return(NULL);

    SHA512_Final(digest, ctx);
    for (i = 0; i < SHA512_DIGEST_LENGTH; i++) {
	buf[i + i] = hex[digest[i] >> 4];
	buf[i + i + 1] = hex[digest[i] & 0x0f];
    }
    buf[i + i] = '\0';
    memset(digest, 0, sizeof(digest));
    return(buf);
}

char *
SHA512_File(char *filename, char buf[SHA512_DIGEST_STRING_LENGTH])
{
    u_int8_t buffer[BUFSIZ];
    SHA512_CTX ctx;
    int fd, num, oerrno;

    SHA512_Init(&ctx);

    if ((fd = open(filename, O_RDONLY)) < 0)
	return(0);

    while ((num = read(fd, buffer, sizeof(buffer))) > 0)
	SHA512_Update(&ctx, buffer, num);

    oerrno = errno;
    close(fd);
    errno = oerrno;
    return(num < 0 ? 0 : SHA512_End(&ctx, buf));
}

char *
SHA512_Data(const u_int8_t *data, size_t len, char buf[SHA512_DIGEST_STRING_LENGTH])
{
    SHA512_CTX ctx;

    SHA512_Init(&ctx);
    SHA512_Update(&ctx, data, len);
    return(SHA512_End(&ctx, buf));
}
