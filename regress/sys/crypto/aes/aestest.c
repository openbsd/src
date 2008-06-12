/*      $OpenBSD: aestest.c,v 1.1 2008/06/12 19:42:48 djm Exp $  */

/*
 * Copyright (c) 2002 Markus Friedl.  All rights reserved.
 * Copyright (c) 2008 Damien Miller.  All rights reserved.
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

/*
 * Test crypto(4) AES with test vectors provided by Dr Brian Gladman:
 * http://fp.gladman.plus.com/AES/
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
#include <ctype.h>

static int
syscrypt(const unsigned char *key, size_t klen, const unsigned char *in,
    unsigned char *out, size_t len, int do_encrypt)
{
	struct session_op session;
	struct crypt_op cryp;
	int cryptodev_fd = -1, fd = -1;
	u_char iv[32];

	/*
	 * Kludge; the kernel doesn't support ECB encryption so we
	 * use a all-zero IV and encrypt a single block only, so the
	 * result should be the same.
	 */
	bzero(iv, sizeof(iv));

	if ((cryptodev_fd = open("/dev/crypto", O_RDWR, 0)) < 0) {
		warn("/dev/crypto");
		goto err;
	}
	if (ioctl(cryptodev_fd, CRIOGET, &fd) == -1) {
		warn("CRIOGET failed");
		goto err;
	}
	memset(&session, 0, sizeof(session));
	session.cipher = CRYPTO_AES_CBC;
	session.key = (caddr_t) key;
	session.keylen = klen;
	if (ioctl(fd, CIOCGSESSION, &session) == -1) {
		warn("CIOCGSESSION");
		goto err;
	}
	memset(&cryp, 0, sizeof(cryp));
	cryp.ses = session.ses;
	cryp.op = do_encrypt ? COP_ENCRYPT : COP_DECRYPT;
	cryp.flags = 0;
	cryp.len = len;
	cryp.src = (caddr_t) in;
	cryp.dst = (caddr_t) out;
	cryp.iv = (caddr_t) iv;
	cryp.mac = 0;
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
	return (0);

err:
	if (fd != -1)
		close(fd);
	if (cryptodev_fd != -1)
		close(cryptodev_fd);
	return (-1);
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

static int
match(unsigned char *a, unsigned char *b, size_t len)
{
	size_t i;

	if (memcmp(a, b, len) == 0)
		return (1);

	warnx("decrypt/plaintext mismatch");

	for (i = 0; i < len; i++)
		printf("%2.2x", a[i]);
	printf("\n");
	for (i = 0; i < len; i++)
		printf("%2.2x", b[i]);
	printf("\n");

	return (0);
}

/*
 * Match expected substring at start of line. If sequence is match, return
 * a pointer to the first character in the string past the sequence and and
 * following whitespace.
 * returns NULL is the start of the line does not match.
 */
static const char *
startswith(const char *line, const char *startswith)
{
	size_t len = strlen(startswith);

	if (strncmp(line, startswith, len) != 0)
		return NULL;
	line = line + len;
	while (isspace(*line))
		line++;
	return line;
}

/* Read a hex string and convert to bytes */
static void
parsehex(const char *hex, u_char **s, u_int *lenp)
{
	u_char *ret, v;
	u_int i, len;
	char c;

	len = i = 0;
	ret = NULL;
	v = 0;
	while ((c = *(hex++)) != '\0') {
		if (strchr(" \t\r\n", c) != NULL)
			continue;
		if (c >= '0' && c <= '9')
			v |= c - '0';
		else if (c >= 'a' && c <= 'f')
			v |= 10 + (c - 'a');
		else if (c >= 'A' && c <= 'F')
			v |= 10 + c - 'A';
		else
			errx(1, "%s: invalid character \"%c\" in hex string",
			    __func__, c);
		switch (++i) {
		case 1:
			v <<= 4;
			break;
		case 2:
			if ((ret = realloc(ret, ++len)) == NULL)
				errx(1, "realloc(%u)", len);
			ret[len - 1] = v;
			v = 0;
			i = 0;
		}
	}
	if (i != 0)
		errx(1, "%s: odd number of characters in hex string", __func__);
	*lenp = len;
	*s = ret;
}

static int
do_tests(const char *filename, int test_num, u_char *key, u_int keylen,
    u_char *plaintext, u_char *ciphertext, u_int textlen)
{
	char result[32];
	int fail = 0;

	/* Encrypt test */
	if (syscrypt(key, keylen, plaintext, result, textlen, 1) < 0) {
		warnx("encrypt with /dev/crypto failed");
		fail++;
	} else if (!match(result, ciphertext, textlen)) {
		fail++;
	} else
		printf("OK encrypt test vector %s %u\n", filename, test_num);

	/* Decrypt test */
	if (syscrypt(key, keylen, ciphertext, result, textlen, 0) < 0) {
		warnx("decrypt with /dev/crypto failed");
		fail++;
	} else if (!match(result, plaintext, textlen)) {
		fail++;
	} else
		printf("OK decrypt test vector %s %u\n", filename, test_num);

	return fail;
}

static int
run_file(const char *filename)
{
	FILE *tv;
	char buf[1024], *eol;
	const char *cp, *errstr;
	int lnum = 0, fail = 0;
	u_char *key, *plaintext, *ciphertext;
	u_int keylen, textlen, tmp;
	int blocksize, keysize, test;

	if ((tv = fopen(filename, "r")) == NULL)
		err(1, "fopen(\"%s\")", filename);

	keylen = textlen = tmp = 0;
	key = ciphertext = plaintext = NULL;
	blocksize = keysize = test = -1;
	while ((fgets(buf, sizeof(buf), tv)) != NULL) {
		lnum++;
		eol = buf + strlen(buf) - 1;
		if (*eol != '\n')
			errx(1, "line %d: too long", lnum);
		if (eol > buf && *(eol - 1) == '\r')
			eol--;
		*eol = '\0';
		if ((cp = startswith(buf, "BLOCKSIZE=")) != NULL) {
			if (blocksize != -1)
				errx(1, "line %d: blocksize already set", lnum);
			blocksize = (int)strtonum(cp, 128, 128, &errstr);
			if (errstr)
				errx(1, "line %d: blocksize is %s: \"%s\"",
				    lnum, errstr, cp);
		} else if ((cp = startswith(buf, "KEYSIZE=")) != NULL) {
			if (keysize != -1)
				errx(1, "line %d: keysize already set", lnum);
			keysize = (int)strtonum(cp, 128, 256, &errstr);
			if (errstr)
				errx(1, "line %d: keysize is %s: \"%s\"",
				    lnum, errstr, cp);
			if (keysize != 128 && keysize != 256)
				errx(1, "line %d: XXX only 128 or 256 "
				    "bit keys for now (keysize = %d)",
				    lnum, keysize);
		} else if ((cp = startswith(buf, "PT=")) != NULL) {
			if (plaintext != NULL)
				free(plaintext);
			parsehex(cp, &plaintext, &tmp);
			if (tmp * 8 != (u_int)blocksize)
				errx(1, "line %d: plaintext len %u != "
				    "blocklen %d", lnum, tmp, blocksize);
			if (textlen != 0) {
				if (textlen != tmp)
					errx(1, "line %d: plaintext len %u != "
					    "ciphertext len %d", lnum, tmp,
					    textlen);
			} else
				textlen = tmp;
		} else if ((cp = startswith(buf, "CT=")) != NULL) {
			if (ciphertext != NULL)
				free(ciphertext);
			parsehex(cp, &ciphertext, &tmp);
			if (tmp * 8 != (u_int)blocksize)
				errx(1, "line %d: ciphertext len %u != "
				    "blocklen %d", lnum, tmp, blocksize);
			if (textlen != 0) {
				if (textlen != tmp)
					errx(1, "line %d: ciphertext len %u != "
					    "plaintext len %d", lnum, tmp,
					    textlen);
			} else
				textlen = tmp;
		} else if ((cp = startswith(buf, "KEY=")) != NULL) {
			if (key != NULL)
				free(key);
			parsehex(cp, &key, &keylen);
			if (keylen * 8 != (u_int)keysize)
				errx(1, "line %d: ciphertext len %u != "
				    "blocklen %d", lnum, tmp, textlen);
		} else if ((cp = startswith(buf, "TEST=")) != NULL) {
			if (plaintext == NULL || ciphertext == NULL ||
			    key == NULL || blocksize == -1 || keysize == -1) {
				if (test != -1)
					errx(1, "line %d: new test before "
					    "parameters", lnum);
				goto parsetest;
			}
			/* do the tests */
			fail += do_tests(filename, test, key, keylen,
			    plaintext, ciphertext, textlen);
 parsetest:
			test = (int)strtonum(cp, 0, 65536, &errstr);
			if (errstr)
				errx(1, "line %d: test is %s: \"%s\"",
				    lnum, errstr, cp);
		} else {
			/* don't care */
			continue;
		}
	}
	fclose(tv);

	return fail;
}

int
main(int argc, char **argv)
{
	int allowed = 0, fail = 0, i;

	if (argc < 2)
		errx(1, "usage: aestest [test-vector-file]");

	if (geteuid() == 0) {
		allowed = getallowsoft();
		if (allowed == 0)
			setallowsoft(1);
	}

	for (i = 1; i < argc; i++)
		fail += run_file(argv[1]);

	if (geteuid() == 0 && allowed == 0)
		setallowsoft(0);

	return fail > 0 ? 1 : 0;
}
