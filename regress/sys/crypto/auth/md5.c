/*      $OpenBSD: md5.c,v 1.6 2004/07/22 15:11:35 miod Exp $  */

/*
 * Copyright (c) 2002 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <crypto/cryptodev.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MD5LEN 16

static char *
sysmd5(const char *s, size_t len)
{
	static char md[MD5LEN*2 + 1];
	unsigned char digest[MD5LEN];
	struct session_op session;
	struct crypt_op cryp;
	int cryptodev_fd = -1, fd = -1, i;

	if ((cryptodev_fd = open("/dev/crypto", O_RDWR, 0)) < 0) {
		warn("/dev/crypto");
		goto err;
	}
	if (ioctl(cryptodev_fd, CRIOGET, &fd) == -1) {
		warn("CRIOGET failed");
		goto err;
	}
	memset(&session, 0, sizeof(session));
	session.cipher = 0;
	session.mac = CRYPTO_MD5;
	session.mackeylen = 0;
	if (ioctl(fd, CIOCGSESSION, &session) == -1) {
		warn("CIOCGSESSION");
		goto err;
	}
	memset(&cryp, 0, sizeof(cryp));
	cryp.ses = session.ses;
	cryp.op = COP_ENCRYPT;			/*???*/
	cryp.flags = 0;
	cryp.src = (caddr_t) s;
	cryp.len = len;
	cryp.dst = 0;
	cryp.mac = (caddr_t) digest;
	cryp.iv = 0;
	if (ioctl(fd, CIOCCRYPT, &cryp) == -1) {
		warn("CIOCCRYPT");
		goto err;
	}
	if (ioctl(fd, CIOCFSESSION, &session.ses) == -1) {
		warn("CIOCFSESSION");
		goto err;
	}
	close(fd);
	close(cryptodev_fd);

	md[0] = '\0';
	for (i = 0; i < MD5LEN; i++)
		snprintf(md + 2*i, sizeof(md) - 2*i, "%2.2x", digest[i]);
	return (md);
err:
	if (fd != -1)
		close(fd);
	if (cryptodev_fd != -1)
		close(cryptodev_fd);
	return (NULL);
}

static int
getallowsoft(void)
{
	int mib[2], old;
	size_t olen;

	olen = sizeof(old);

	mib[0] = CTL_KERN;
	mib[1] = KERN_CRYPTODEVALLOWSOFT;
	if (sysctl(mib, 2, &old, &olen, NULL, 0) < 0)
		err(1, "sysctl failed");

	return old;
}

static void
setallowsoft(int new)
{
	int mib[2], old;
	size_t olen, nlen;

	olen = nlen = sizeof(new);

	mib[0] = CTL_KERN;
	mib[1] = KERN_CRYPTODEVALLOWSOFT;

	if (sysctl(mib, 2, &old, &olen, &new, nlen) < 0)
		err(1, "sysctl failed");
}

/* test vectors from RFC 1321 */
static struct {
	char *dat;
	char *md;
} test[] = {
	{ "",	 			"d41d8cd98f00b204e9800998ecf8427e" },
	{ "a",	 			"0cc175b9c0f1b6a831c399e269772661" },
	{ "abc",	 		"900150983cd24fb0d6963f7d28e17f72" },
	{ "message digest",	 	"f96b697d7cb7938d525a2f31aaf161d0" },
	{ "abcdefghijklmnopqrstuvwxyz",	"c3fcd3d76192e4007dfb496cca67e13b" },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
					"d174ab98d277d9f5a5611c2c9f419d9f" },
	{ "123456789012345678901234567890123456789012345678901234567890"
	  "12345678901234567890",	"57edf4a22be3c955ac49da2e2107b67a" },
	{ NULL, NULL },
};

int
main(int argc, char **argv)
{
	int allowed = 0, i, count, fail;
	char *md;

	if (geteuid() == 0) {
		allowed = getallowsoft();
		if (allowed == 0)
			setallowsoft(1);
	}
	for (count = 0, fail = 0, i = 0; test[i].dat; i++) {
		if ((md = sysmd5(test[i].dat, strlen(test[i].dat))) == NULL) {
			warn("md5 with /dev/crypto failed");
			continue;
		}
		if (strcmp(md, test[i].md) == 0) {
			printf("md5 ok for '%s'\n", test[i].dat);
			count++;
		} else {
			warnx("md5 failed for '%s': got '%s' expected '%s'",
			   test[i].dat, md, test[i].md);
			fail++;
		}
	}
	if (geteuid() == 0 && allowed == 0)
		setallowsoft(0);
	exit((fail > 0 || count == 0) ? 1 : 0);
}
