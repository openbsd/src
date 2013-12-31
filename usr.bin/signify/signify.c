/* $OpenBSD: signify.c,v 1.1 2013/12/31 03:03:32 tedu Exp $ */
/*
 * Copyright (c) 2013 Ted Unangst <tedu@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/stat.h>

#include <netinet/in.h>
#include <resolv.h>

#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <readpassphrase.h>
#include <util.h>
#include <sha2.h>

#include "crypto_api.h"

#define streq(a, b) (strcmp(a, b) == 0)

#define SIGBYTES crypto_sign_ed25519_BYTES
#define SECRETBYTES crypto_sign_ed25519_SECRETKEYBYTES
#define PUBLICBYTES crypto_sign_ed25519_PUBLICKEYBYTES

#define PKALG "Ed"
#define KDFALG "BK"

struct enckey {
	uint8_t pkalg[2];
	uint8_t kdfalg[2];
	uint32_t kdfrounds;
	uint8_t salt[16];
	uint8_t checksum[8];
	uint8_t seckey[SECRETBYTES];
};

struct pubkey {
	uint8_t pkalg[2];
	uint8_t pubkey[PUBLICBYTES];
};

struct sig {
	uint8_t pkalg[2];
	uint8_t sig[SIGBYTES];
};

extern char *__progname;

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-P] [-i input] [-p pubkey] [-s seckey] "
	    "generate|sign|verify", __progname);
	exit(1);
}

static int
xopen(const char *fname, int flags, mode_t mode)
{
	int fd;

	fd = open(fname, flags, mode);
	if (fd == -1)
		err(1, "open %s", fname);
	return fd;
}

static void *
xmalloc(size_t len)
{
	void *p;

	p = malloc(len);
	if (!p)
		err(1, "malloc %zu", len);
	return p;
}

static void
readall(int fd, void *buf, size_t len)
{
	if (read(fd, buf, len) != len)
		err(1, "read");
}

static void
readb64file(const char *filename, void *buf, size_t len)
{
	char b64[2048];
	int i, rv, fd;

	fd = xopen(filename, O_RDONLY | O_NOFOLLOW, 0);
	memset(b64, 0, sizeof(b64));
	rv = read(fd, b64, sizeof(b64) - 1);
	if (rv == -1)
		err(1, "read in %s", filename);
	for (i = 0; i < rv; i++)
		if (b64[i] == '\n')
			break;
	if (i == rv)
		errx(1, "no newline in %s", filename);
	rv = b64_pton(b64 + i, buf, len);
	if (rv != len)
		errx(1, "invalid b64 encoding in %s", filename);
	memset(b64, 0, sizeof(b64));
	close(fd);
	if (memcmp(buf, PKALG, 2))
		errx(1, "unsupported file %s", filename);
}

uint8_t *
readmsg(const char *filename, unsigned long long *msglenp)
{
	unsigned long long msglen;
	uint8_t *msg;
	struct stat sb;
	int fd;

	fd = xopen(filename, O_RDONLY | O_NOFOLLOW, 0);
	fstat(fd, &sb);
	msglen = sb.st_size;
	if (msglen > (1UL << 30))
		errx(1, "msg too large in %s", filename);
	msg = xmalloc(msglen);
	readall(fd, msg, msglen);
	close(fd);

	*msglenp = msglen;
	return msg;
}

static void
writeall(int fd, const void *buf, size_t len)
{
	if (write(fd, buf, len) != len)
		err(1, "write");
}

static void
writeb64file(const char *filename, const char *comment, const void *buf,
    size_t len, mode_t mode)
{
	char header[1024];
	char b64[1024];
	int fd, rv;

	fd = xopen(filename, O_CREAT|O_EXCL|O_NOFOLLOW|O_RDWR, mode);
	snprintf(header, sizeof(header), "signify -- %s\n", comment);
	writeall(fd, header, strlen(header));
	if ((rv = b64_ntop(buf, len, b64, sizeof(b64))) == -1)
		errx(1, "b64 encode failed");
	writeall(fd, b64, rv);
	memset(b64, 0, sizeof(b64));
	close(fd);
}

static void
kdf(uint8_t *salt, size_t saltlen, int rounds, uint8_t *key, size_t keylen)
{
	char pass[1024];

	if (rounds == 0) {
		memset(key, 0, keylen);
		return;
	}

	if (!readpassphrase("passphrase: ", pass, sizeof(pass), 0))
		errx(1, "readpassphrase");
	if (bcrypt_pbkdf(pass, strlen(pass), salt, saltlen, key,
	    keylen, rounds) == -1)
		errx(1, "bcrypt pbkdf");
	memset(pass, 0, sizeof(pass));
}

static void
signmsg(uint8_t *seckey, uint8_t *msg, unsigned long long msglen,
    uint8_t *sig)
{
	unsigned long long siglen;
	uint8_t *sigbuf;

	sigbuf = xmalloc(msglen + SIGBYTES);
	crypto_sign_ed25519(sigbuf, &siglen, msg, msglen, seckey);
	memcpy(sig, sigbuf, SIGBYTES);
	free(sigbuf);
}

static void
verifymsg(uint8_t *pubkey, uint8_t *msg, unsigned long long msglen,
    uint8_t *sig)
{
	uint8_t *sigbuf, *dummybuf;
	unsigned long long siglen, dummylen;

	siglen = SIGBYTES + msglen;
	sigbuf = xmalloc(siglen);
	dummybuf = xmalloc(siglen);
	memcpy(sigbuf, sig, SIGBYTES);
	memcpy(sigbuf + SIGBYTES, msg, msglen);
	if (crypto_sign_ed25519_open(dummybuf, &dummylen, sigbuf, siglen,
	    pubkey) == -1)
		errx(1, "signature failed");
	free(sigbuf);
	free(dummybuf);
}

static void
generate(const char *pubkeyfile, const char *seckeyfile, int rounds)
{
	uint8_t digest[SHA512_DIGEST_LENGTH];
	struct pubkey pubkey;
	struct enckey enckey;
	uint8_t xorkey[sizeof(enckey.seckey)];
	SHA2_CTX ctx;
	int i;

	crypto_sign_ed25519_keypair(pubkey.pubkey, enckey.seckey);

	SHA512Init(&ctx);
	SHA512Update(&ctx, enckey.seckey, sizeof(enckey.seckey));
	SHA512Final(digest, &ctx);

	memcpy(enckey.pkalg, PKALG, 2);
	memcpy(enckey.kdfalg, KDFALG, 2);
	enckey.kdfrounds = htonl(rounds);
	arc4random_buf(enckey.salt, sizeof(enckey.salt));
	kdf(enckey.salt, sizeof(enckey.salt), rounds, xorkey, sizeof(xorkey));
	memcpy(enckey.checksum, digest, sizeof(enckey.checksum));
	for (i = 0; i < sizeof(enckey.seckey); i++)
		enckey.seckey[i] ^= xorkey[i];
	memset(digest, 0, sizeof(digest));
	memset(xorkey, 0, sizeof(xorkey));

	writeb64file(seckeyfile, "secret key", &enckey,
	    sizeof(enckey), 0600);
	memset(&enckey, 0, sizeof(enckey));

	memcpy(pubkey.pkalg, PKALG, 2);
	writeb64file(pubkeyfile, "public key", &pubkey,
	    sizeof(pubkey), 0666);
}

static void
sign(const char *seckeyfile, const char *inputfile, const char *sigfile)
{
	struct sig sig;
	uint8_t digest[SHA512_DIGEST_LENGTH];
	struct enckey enckey;
	uint8_t xorkey[sizeof(enckey.seckey)];
	uint8_t *msg;
	unsigned long long msglen;
	int i, rounds;
	SHA2_CTX ctx;

	readb64file(seckeyfile, &enckey, sizeof(enckey));

	if (memcmp(enckey.kdfalg, KDFALG, 2))
		errx(1, "unsupported KDF");
	rounds = ntohl(enckey.kdfrounds);
	kdf(enckey.salt, sizeof(enckey.salt), rounds, xorkey, sizeof(xorkey));
	for (i = 0; i < sizeof(enckey.seckey); i++)
		enckey.seckey[i] ^= xorkey[i];
	memset(xorkey, 0, sizeof(xorkey));
	SHA512Init(&ctx);
	SHA512Update(&ctx, enckey.seckey, sizeof(enckey.seckey));
	SHA512Final(digest, &ctx);
	if (memcmp(enckey.checksum, digest, sizeof(enckey.checksum)))
	    errx(1, "incorrect passphrase");
	memset(digest, 0, sizeof(digest));

	msg = readmsg(inputfile, &msglen);

	signmsg(enckey.seckey, msg, msglen, sig.sig);
	memset(&enckey, 0, sizeof(enckey));

	memcpy(sig.pkalg, PKALG, 2);
	writeb64file(sigfile, "signature", &sig, sizeof(sig), 0666);

	free(msg);
}

static void
verify(const char *pubkeyfile, const char *inputfile, const char *sigfile)
{
	struct sig sig;
	struct pubkey pubkey;
	unsigned long long msglen;
	uint8_t *msg;

	readb64file(pubkeyfile, &pubkey, sizeof(pubkey));
	readb64file(sigfile, &sig, sizeof(sig));

	msg = readmsg(inputfile, &msglen);

	verifymsg(pubkey.pubkey, msg, msglen, sig.sig);
	printf("verified\n");

	free(msg);
}

int
main(int argc, char **argv)
{
	const char *verb = NULL;
	const char *pubkeyfile = NULL, *seckeyfile = NULL, *inputfile = NULL,
	    *sigfile = NULL;
	char sigfilebuf[1024];
	int ch, rounds;

	rounds = 42;

	while ((ch = getopt(argc, argv, "I:NO:P:S:")) != -1) {
		switch (ch) {
		case 'I':
			inputfile = optarg;
			break;
		case 'N':
			rounds = 0;
			break;
		case 'O':
			sigfile = optarg;
			break;
		case 'P':
			pubkeyfile = optarg;
			break;
		case 'S':
			seckeyfile = optarg;
			break;
		case 'V':
			verb = optarg;
			break;
		default:
			usage();
			break;
		}
	}
	if (argc != 0)
		usage();

	if (inputfile && !sigfile) {
		if (snprintf(sigfilebuf, sizeof(sigfilebuf), "%s.sig",
		    inputfile) >= sizeof(sigfile))
			errx(1, "path too long");
		sigfile = sigfilebuf;
	}

	if (streq(verb, "generate")) {
		if (!pubkeyfile || !seckeyfile)
			usage();
		generate(pubkeyfile, seckeyfile, rounds);
	} else if (streq(verb, "sign")) {
		if (!seckeyfile || !inputfile)
			usage();
		sign(seckeyfile, inputfile, sigfile);
	} else if (streq(verb, "verify")) {
		if (!pubkeyfile || !inputfile)
			usage();
		verify(pubkeyfile, inputfile, sigfile);
	} else {
		usage();
	}
	return 0;
}
