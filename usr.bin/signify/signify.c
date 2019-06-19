/* $OpenBSD: signify.c,v 1.131 2019/03/23 07:10:06 tedu Exp $ */
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

#include <limits.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <ohash.h>
#include <err.h>
#include <unistd.h>
#include <readpassphrase.h>
#include <util.h>
#include <sha2.h>

#include "crypto_api.h"
#include "signify.h"

#define SIGBYTES crypto_sign_ed25519_BYTES
#define SECRETBYTES crypto_sign_ed25519_SECRETKEYBYTES
#define PUBLICBYTES crypto_sign_ed25519_PUBLICKEYBYTES

#define PKALG "Ed"
#define KDFALG "BK"
#define KEYNUMLEN 8

#define COMMENTHDR "untrusted comment: "
#define COMMENTHDRLEN 19
#define COMMENTMAXLEN 1024
#define VERIFYWITH "verify with "

struct enckey {
	uint8_t pkalg[2];
	uint8_t kdfalg[2];
	uint32_t kdfrounds;
	uint8_t salt[16];
	uint8_t checksum[8];
	uint8_t keynum[KEYNUMLEN];
	uint8_t seckey[SECRETBYTES];
};

struct pubkey {
	uint8_t pkalg[2];
	uint8_t keynum[KEYNUMLEN];
	uint8_t pubkey[PUBLICBYTES];
};

struct sig {
	uint8_t pkalg[2];
	uint8_t keynum[KEYNUMLEN];
	uint8_t sig[SIGBYTES];
};

static void __dead
usage(const char *error)
{
	if (error)
		fprintf(stderr, "%s\n", error);
	fprintf(stderr, "usage:"
#ifndef VERIFYONLY
	    "\t%1$s -C [-q] -p pubkey -x sigfile [file ...]\n"
	    "\t%1$s -G [-n] [-c comment] -p pubkey -s seckey\n"
	    "\t%1$s -S [-enz] [-x sigfile] -s seckey -m message\n"
#endif
	    "\t%1$s -V [-eqz] [-p pubkey] [-t keytype] [-x sigfile] -m message\n",
	    getprogname());
	exit(1);
}

int
xopen(const char *fname, int oflags, mode_t mode)
{
	struct stat sb;
	int fd;

	if (strcmp(fname, "-") == 0) {
		if ((oflags & O_WRONLY))
			fd = dup(STDOUT_FILENO);
		else
			fd = dup(STDIN_FILENO);
		if (fd == -1)
			err(1, "dup failed");
	} else {
		fd = open(fname, oflags, mode);
		if (fd == -1)
			err(1, "can't open %s for %s", fname,
			    (oflags & O_WRONLY) ? "writing" : "reading");
	}
	if (fstat(fd, &sb) == -1 || S_ISDIR(sb.st_mode))
		errx(1, "not a valid file: %s", fname);
	return fd;
}

void *
xmalloc(size_t len)
{
	void *p;

	if (!(p = malloc(len)))
		err(1, "malloc %zu", len);
	return p;
}

static size_t
parseb64file(const char *filename, char *b64, void *buf, size_t buflen,
    char *comment)
{
	char *commentend, *b64end;

	commentend = strchr(b64, '\n');
	if (!commentend || commentend - b64 <= COMMENTHDRLEN ||
	    memcmp(b64, COMMENTHDR, COMMENTHDRLEN) != 0)
		errx(1, "invalid comment in %s; must start with '%s'",
		    filename, COMMENTHDR);
	*commentend = '\0';
	if (comment) {
		if (strlcpy(comment, b64 + COMMENTHDRLEN,
		    COMMENTMAXLEN) >= COMMENTMAXLEN)
			errx(1, "comment too long");
	}
	if (!(b64end = strchr(commentend + 1, '\n')))
		errx(1, "missing new line after base64 in %s", filename);
	*b64end = '\0';
	if (b64_pton(commentend + 1, buf, buflen) != buflen)
		errx(1, "unable to parse %s", filename);
	if (memcmp(buf, PKALG, 2) != 0)
		errx(1, "unsupported file %s", filename);
	return b64end - b64 + 1;
}

static void
readb64file(const char *filename, void *buf, size_t buflen, char *comment)
{
	char b64[2048];
	int rv, fd;

	fd = xopen(filename, O_RDONLY | O_NOFOLLOW, 0);
	if ((rv = read(fd, b64, sizeof(b64) - 1)) == -1)
		err(1, "read from %s", filename);
	b64[rv] = '\0';
	parseb64file(filename, b64, buf, buflen, comment);
	explicit_bzero(b64, sizeof(b64));
	close(fd);
}

static uint8_t *
readmsg(const char *filename, unsigned long long *msglenp)
{
	unsigned long long msglen = 0;
	uint8_t *msg = NULL;
	struct stat sb;
	ssize_t x, space;
	int fd;
	const unsigned long long maxmsgsize = 1UL << 30;

	fd = xopen(filename, O_RDONLY | O_NOFOLLOW, 0);
	if (fstat(fd, &sb) == 0 && S_ISREG(sb.st_mode)) {
		if (sb.st_size > maxmsgsize)
			errx(1, "msg too large in %s", filename);
		space = sb.st_size + 1;
	} else {
		space = 64 * 1024 - 1;
	}

	msg = xmalloc(space + 1);
	while (1) {
		if (space == 0) {
			if (msglen * 2 > maxmsgsize)
				errx(1, "msg too large in %s", filename);
			space = msglen;
			if (!(msg = realloc(msg, msglen + space + 1)))
				err(1, "realloc");
		}
		if ((x = read(fd, msg + msglen, space)) == -1)
			err(1, "read from %s", filename);
		if (x == 0)
			break;
		space -= x;
		msglen += x;
	}

	msg[msglen] = '\0';
	close(fd);

	*msglenp = msglen;
	return msg;
}

void
writeall(int fd, const void *buf, size_t buflen, const char *filename)
{
	ssize_t x;

	while (buflen != 0) {
		if ((x = write(fd, buf, buflen)) == -1)
			err(1, "write to %s", filename);
		buflen -= x;
		buf = (char *)buf + x;
	}
}

#ifndef VERIFYONLY
static char *
createheader(const char *comment, const void *buf, size_t buflen)
{
	char *header;
	char b64[1024];

	if (b64_ntop(buf, buflen, b64, sizeof(b64)) == -1)
		errx(1, "base64 encode failed");
	if (asprintf(&header, "%s%s\n%s\n", COMMENTHDR, comment, b64) == -1)
		err(1, "asprintf failed");
	explicit_bzero(b64, sizeof(b64));
	return header;
}

static void
writekeyfile(const char *filename, const char *comment, const void *buf,
    size_t buflen, int oflags, mode_t mode)
{
	char *header;
	int fd;

	fd = xopen(filename, O_CREAT|oflags|O_NOFOLLOW|O_WRONLY, mode);
	header = createheader(comment, buf, buflen);
	writeall(fd, header, strlen(header), filename);
	freezero(header, strlen(header));
	close(fd);
}

static void
kdf(uint8_t *salt, size_t saltlen, int rounds, int allowstdin, int confirm,
    uint8_t *key, size_t keylen)
{
	char pass[1024];
	int rppflags = RPP_ECHO_OFF;
	const char *errstr = NULL;

	if (rounds == 0) {
		memset(key, 0, keylen);
		return;
	}

	if (allowstdin && !isatty(STDIN_FILENO))
		rppflags |= RPP_STDIN;
	if (!readpassphrase("passphrase: ", pass, sizeof(pass), rppflags))
		errx(1, "unable to read passphrase");
	if (strlen(pass) == 0)
		errx(1, "please provide a password");
	if (confirm && !(rppflags & RPP_STDIN)) {
		char pass2[1024];
		if (!readpassphrase("confirm passphrase: ", pass2,
		    sizeof(pass2), rppflags))
			errstr = "unable to read passphrase";
		if (!errstr && strcmp(pass, pass2) != 0)
			errstr = "passwords don't match";
		explicit_bzero(pass2, sizeof(pass2));
	}
	if (!errstr && bcrypt_pbkdf(pass, strlen(pass), salt, saltlen, key,
	    keylen, rounds) == -1)
		errstr = "bcrypt pbkdf";
	explicit_bzero(pass, sizeof(pass));
	if (errstr)
		errx(1, "%s", errstr);
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
generate(const char *pubkeyfile, const char *seckeyfile, int rounds,
    const char *comment)
{
	uint8_t digest[SHA512_DIGEST_LENGTH];
	struct pubkey pubkey;
	struct enckey enckey;
	uint8_t xorkey[sizeof(enckey.seckey)];
	uint8_t keynum[KEYNUMLEN];
	char commentbuf[COMMENTMAXLEN];
	SHA2_CTX ctx;
	int i, nr;

	crypto_sign_ed25519_keypair(pubkey.pubkey, enckey.seckey);
	arc4random_buf(keynum, sizeof(keynum));

	SHA512Init(&ctx);
	SHA512Update(&ctx, enckey.seckey, sizeof(enckey.seckey));
	SHA512Final(digest, &ctx);

	memcpy(enckey.pkalg, PKALG, 2);
	memcpy(enckey.kdfalg, KDFALG, 2);
	enckey.kdfrounds = htonl(rounds);
	memcpy(enckey.keynum, keynum, KEYNUMLEN);
	arc4random_buf(enckey.salt, sizeof(enckey.salt));
	kdf(enckey.salt, sizeof(enckey.salt), rounds, 1, 1, xorkey, sizeof(xorkey));
	memcpy(enckey.checksum, digest, sizeof(enckey.checksum));
	for (i = 0; i < sizeof(enckey.seckey); i++)
		enckey.seckey[i] ^= xorkey[i];
	explicit_bzero(digest, sizeof(digest));
	explicit_bzero(xorkey, sizeof(xorkey));

	nr = snprintf(commentbuf, sizeof(commentbuf), "%s secret key", comment);
	if (nr == -1 || nr >= sizeof(commentbuf))
		errx(1, "comment too long");
	writekeyfile(seckeyfile, commentbuf, &enckey,
	    sizeof(enckey), O_EXCL, 0600);
	explicit_bzero(&enckey, sizeof(enckey));

	memcpy(pubkey.pkalg, PKALG, 2);
	memcpy(pubkey.keynum, keynum, KEYNUMLEN);
	nr = snprintf(commentbuf, sizeof(commentbuf), "%s public key", comment);
	if (nr == -1 || nr >= sizeof(commentbuf))
		errx(1, "comment too long");
	writekeyfile(pubkeyfile, commentbuf, &pubkey,
	    sizeof(pubkey), O_EXCL, 0666);
}

static const char *
check_keyname_compliance(const char *pubkeyfile, const char *seckeyfile)
{
	const char *pos;
	size_t len;

	/* basename may or may not modify input */
	pos = strrchr(seckeyfile, '/');
	if (pos != NULL)
		seckeyfile = pos + 1;

	len = strlen(seckeyfile);
	if (len < 5) /* ?.key */
		goto bad;
	if (strcmp(seckeyfile + len - 4, ".sec") != 0)
		goto bad;
	if (pubkeyfile != NULL) {
		pos = strrchr(pubkeyfile, '/');
		if (pos != NULL)
			pubkeyfile = pos + 1;

		if (strlen(pubkeyfile) != len)
			goto bad;
		if (strcmp(pubkeyfile + len - 4, ".pub") != 0)
			goto bad;
		if (strncmp(pubkeyfile, seckeyfile, len - 4) != 0)
			goto bad;
	}

	return seckeyfile;
bad:
	errx(1, "please use naming scheme of keyname.pub and keyname.sec");
}

uint8_t *
createsig(const char *seckeyfile, const char *msgfile, uint8_t *msg,
    unsigned long long msglen)
{
	struct enckey enckey;
	uint8_t xorkey[sizeof(enckey.seckey)];
	struct sig sig;
	char *sighdr;
	uint8_t digest[SHA512_DIGEST_LENGTH];
	int i, nr, rounds;
	SHA2_CTX ctx;
	char comment[COMMENTMAXLEN], sigcomment[COMMENTMAXLEN];

	readb64file(seckeyfile, &enckey, sizeof(enckey), comment);

	if (strcmp(seckeyfile, "-") == 0) {
 		nr = snprintf(sigcomment, sizeof(sigcomment),
		    "signature from %s", comment);
	} else {
		const char *keyname = check_keyname_compliance(NULL, 
		    seckeyfile);
		nr = snprintf(sigcomment, sizeof(sigcomment),
		    VERIFYWITH "%.*s.pub", (int)strlen(keyname) - 4, keyname);
	}
	if (nr == -1 || nr >= sizeof(sigcomment))
		errx(1, "comment too long");

	if (memcmp(enckey.kdfalg, KDFALG, 2) != 0)
		errx(1, "unsupported KDF");
	rounds = ntohl(enckey.kdfrounds);
	kdf(enckey.salt, sizeof(enckey.salt), rounds, strcmp(msgfile, "-") != 0,
	    0, xorkey, sizeof(xorkey));
	for (i = 0; i < sizeof(enckey.seckey); i++)
		enckey.seckey[i] ^= xorkey[i];
	explicit_bzero(xorkey, sizeof(xorkey));
	SHA512Init(&ctx);
	SHA512Update(&ctx, enckey.seckey, sizeof(enckey.seckey));
	SHA512Final(digest, &ctx);
	if (memcmp(enckey.checksum, digest, sizeof(enckey.checksum)) != 0)
		errx(1, "incorrect passphrase");
	explicit_bzero(digest, sizeof(digest));

	signmsg(enckey.seckey, msg, msglen, sig.sig);
	memcpy(sig.keynum, enckey.keynum, KEYNUMLEN);
	explicit_bzero(&enckey, sizeof(enckey));

	memcpy(sig.pkalg, PKALG, 2);

	sighdr = createheader(sigcomment, &sig, sizeof(sig));
	return sighdr;
}

static void
sign(const char *seckeyfile, const char *msgfile, const char *sigfile,
    int embedded)
{
	uint8_t *msg;
	char *sighdr;
	int fd;
	unsigned long long msglen;

	msg = readmsg(msgfile, &msglen);

	sighdr = createsig(seckeyfile, msgfile, msg, msglen);

	fd = xopen(sigfile, O_CREAT|O_TRUNC|O_NOFOLLOW|O_WRONLY, 0666);
	writeall(fd, sighdr, strlen(sighdr), sigfile);
	free(sighdr);
	if (embedded)
		writeall(fd, msg, msglen, sigfile);
	close(fd);

	free(msg);
}
#endif

static void
verifymsg(struct pubkey *pubkey, uint8_t *msg, unsigned long long msglen,
    struct sig *sig, int quiet)
{
	uint8_t *sigbuf, *dummybuf;
	unsigned long long siglen, dummylen;

	if (memcmp(pubkey->keynum, sig->keynum, KEYNUMLEN) != 0)
		errx(1, "verification failed: checked against wrong key");

	siglen = SIGBYTES + msglen;
	sigbuf = xmalloc(siglen);
	dummybuf = xmalloc(siglen);
	memcpy(sigbuf, sig->sig, SIGBYTES);
	memcpy(sigbuf + SIGBYTES, msg, msglen);
	if (crypto_sign_ed25519_open(dummybuf, &dummylen, sigbuf, siglen,
	    pubkey->pubkey) == -1)
		errx(1, "signature verification failed");
	if (!quiet)
		printf("Signature Verified\n");
	free(sigbuf);
	free(dummybuf);
}

static void
check_keytype(const char *pubkeyfile, const char *keytype)
{
	const char *p;
	size_t typelen;

	if (!(p = strrchr(pubkeyfile, '-')))
		goto bad;
	p++;
	typelen = strlen(keytype);
	if (strncmp(p, keytype, typelen) != 0)
		goto bad;
	if (strcmp(p + typelen, ".pub") != 0)
		goto bad;
	return;

bad:
	errx(1, "incorrect keytype: %s is not %s", pubkeyfile, keytype);
}

static void
readpubkey(const char *pubkeyfile, struct pubkey *pubkey,
    const char *sigcomment, const char *keytype)
{
	const char *safepath = "/etc/signify";
	char keypath[1024];

	if (!pubkeyfile) {
		pubkeyfile = strstr(sigcomment, VERIFYWITH);
		if (pubkeyfile && strchr(pubkeyfile, '/') == NULL) {
			pubkeyfile += strlen(VERIFYWITH);
			if (keytype)
				check_keytype(pubkeyfile, keytype);
			if (snprintf(keypath, sizeof(keypath), "%s/%s",
			    safepath, pubkeyfile) >= sizeof(keypath))
				errx(1, "name too long %s", pubkeyfile);
			pubkeyfile = keypath;
		} else
			usage("must specify pubkey");
	}
	readb64file(pubkeyfile, pubkey, sizeof(*pubkey), NULL);
}

static void
verifysimple(const char *pubkeyfile, const char *msgfile, const char *sigfile,
    int quiet, const char *keytype)
{
	char sigcomment[COMMENTMAXLEN];
	struct sig sig;
	struct pubkey pubkey;
	unsigned long long msglen;
	uint8_t *msg;

	msg = readmsg(msgfile, &msglen);

	readb64file(sigfile, &sig, sizeof(sig), sigcomment);
	readpubkey(pubkeyfile, &pubkey, sigcomment, keytype);

	verifymsg(&pubkey, msg, msglen, &sig, quiet);

	free(msg);
}

static uint8_t *
verifyembedded(const char *pubkeyfile, const char *sigfile,
    int quiet, unsigned long long *msglenp, const char *keytype)
{
	char sigcomment[COMMENTMAXLEN];
	struct sig sig;
	struct pubkey pubkey;
	unsigned long long msglen, siglen;
	uint8_t *msg;

	msg = readmsg(sigfile, &msglen);

	siglen = parseb64file(sigfile, msg, &sig, sizeof(sig), sigcomment);
	readpubkey(pubkeyfile, &pubkey, sigcomment, keytype);

	msglen -= siglen;
	memmove(msg, msg + siglen, msglen);
	msg[msglen] = 0;

	verifymsg(&pubkey, msg, msglen, &sig, quiet);

	*msglenp = msglen;
	return msg;
}

static void
verify(const char *pubkeyfile, const char *msgfile, const char *sigfile,
    int embedded, int quiet, const char *keytype)
{
	unsigned long long msglen;
	uint8_t *msg;
	int fd;

	if (embedded) {
		msg = verifyembedded(pubkeyfile, sigfile, quiet, &msglen,
		    keytype);
		fd = xopen(msgfile, O_CREAT|O_TRUNC|O_NOFOLLOW|O_WRONLY, 0666);
		writeall(fd, msg, msglen, msgfile);
		free(msg);
		close(fd);
	} else {
		verifysimple(pubkeyfile, msgfile, sigfile, quiet, keytype);
	}
}

#ifndef VERIFYONLY
#define HASHBUFSIZE 224
struct checksum {
	char file[PATH_MAX];
	char hash[HASHBUFSIZE];
	char algo[32];
};

static void *
ecalloc(size_t s1, size_t s2, void *data)
{
	void *p;

	if (!(p = calloc(s1, s2)))
		err(1, "calloc");
	return p;
}

static void
efree(void *p, void *data)
{
	free(p);
}

static void
recodehash(char *hash, size_t len)
{
	uint8_t data[HASHBUFSIZE / 2];
	int i, rv;

	if (strlen(hash) == len)
		return;
	if ((rv = b64_pton(hash, data, sizeof(data))) == -1)
		errx(1, "invalid base64 encoding");
	for (i = 0; i < rv; i++)
		snprintf(hash + i * 2, HASHBUFSIZE - i * 2, "%2.2x", data[i]);
}

static int
verifychecksum(struct checksum *c, int quiet)
{
	char buf[HASHBUFSIZE];

	if (strcmp(c->algo, "SHA256") == 0) {
		recodehash(c->hash, SHA256_DIGEST_STRING_LENGTH-1);
		if (!SHA256File(c->file, buf))
			return 0;
	} else if (strcmp(c->algo, "SHA512") == 0) {
		recodehash(c->hash, SHA512_DIGEST_STRING_LENGTH-1);
		if (!SHA512File(c->file, buf))
			return 0;
	} else {
		errx(1, "can't handle algorithm %s", c->algo);
	}
	if (strcmp(c->hash, buf) != 0)
		return 0;
	if (!quiet)
		printf("%s: OK\n", c->file);
	return 1;
}

static void
verifychecksums(char *msg, int argc, char **argv, int quiet)
{
	struct ohash_info info = { 0, NULL, ecalloc, efree, NULL };
	struct ohash myh;
	struct checksum c;
	char *e, *line, *endline;
	int hasfailed = 0;
	int i, rv;
	unsigned int slot;

	ohash_init(&myh, 6, &info);
	if (argc) {
		for (i = 0; i < argc; i++) {
			slot = ohash_qlookup(&myh, argv[i]);
			e = ohash_find(&myh, slot);
			if (e == NULL)
				ohash_insert(&myh, slot, argv[i]);
		}
	}

	line = msg;
	while (line && *line) {
		if ((endline = strchr(line, '\n')))
			*endline++ = '\0';
#if PATH_MAX < 1024 || HASHBUFSIZE < 224
#error sizes are wrong
#endif
		rv = sscanf(line, "%31s (%1023[^)]) = %223s",
		    c.algo, c.file, c.hash);
		if (rv != 3)
			errx(1, "unable to parse checksum line %s", line);
		line = endline;
		if (argc) {
			slot = ohash_qlookup(&myh, c.file);
			e = ohash_find(&myh, slot);
			if (e != NULL) {
				if (verifychecksum(&c, quiet) != 0)
					ohash_remove(&myh, slot);
			}
		} else {
			if (verifychecksum(&c, quiet) == 0) {
				slot = ohash_qlookup(&myh, c.file);
				e = ohash_find(&myh, slot);
				if (e == NULL) {
					if (!(e = strdup(c.file)))
						err(1, "strdup");
					ohash_insert(&myh, slot, e);
				}
			}
		}
	}

	for (e = ohash_first(&myh, &slot); e != NULL; e = ohash_next(&myh, &slot)) {
		fprintf(stderr, "%s: FAIL\n", e);
		hasfailed = 1;
		if (argc == 0)
			free(e);
	}
	ohash_delete(&myh);
	if (hasfailed)
		exit(1);
}

static void
check(const char *pubkeyfile, const char *sigfile, int quiet, int argc,
    char **argv)
{
	unsigned long long msglen;
	uint8_t *msg;

	msg = verifyembedded(pubkeyfile, sigfile, quiet, &msglen, NULL);
	verifychecksums((char *)msg, argc, argv, quiet);

	free(msg);
}

void *
verifyzdata(uint8_t *zdata, unsigned long long zdatalen,
    const char *filename, const char *pubkeyfile, const char *keytype)
{
	struct sig sig;
	char sigcomment[COMMENTMAXLEN];
	unsigned long long siglen;
	struct pubkey pubkey;

	if (zdatalen < sizeof(sig))
		errx(1, "signature too short in %s", filename);
	siglen = parseb64file(filename, zdata, &sig, sizeof(sig),
	    sigcomment);
	readpubkey(pubkeyfile, &pubkey, sigcomment, keytype);
	zdata += siglen;
	zdatalen -= siglen;
	verifymsg(&pubkey, zdata, zdatalen, &sig, 1);
	return zdata;
}
#endif

int
main(int argc, char **argv)
{
	const char *pubkeyfile = NULL, *seckeyfile = NULL, *msgfile = NULL,
	    *sigfile = NULL;
	char sigfilebuf[PATH_MAX];
	const char *comment = "signify";
	char *keytype = NULL;
	int ch;
	int none = 0;
	int embedded = 0;
	int quiet = 0;
	int gzip = 0;
	enum {
		NONE,
		CHECK,
		GENERATE,
		SIGN,
		VERIFY
	} verb = NONE;

	if (pledge("stdio rpath wpath cpath tty", NULL) == -1)
		err(1, "pledge");

	while ((ch = getopt(argc, argv, "CGSVzc:em:np:qs:t:x:")) != -1) {
		switch (ch) {
#ifndef VERIFYONLY
		case 'C':
			if (verb)
				usage(NULL);
			verb = CHECK;
			break;
		case 'G':
			if (verb)
				usage(NULL);
			verb = GENERATE;
			break;
		case 'S':
			if (verb)
				usage(NULL);
			verb = SIGN;
			break;
		case 'z':
			gzip = 1;
			break;
#endif
		case 'V':
			if (verb)
				usage(NULL);
			verb = VERIFY;
			break;
		case 'c':
			comment = optarg;
			break;
		case 'e':
			embedded = 1;
			break;
		case 'm':
			msgfile = optarg;
			break;
		case 'n':
			none = 1;
			break;
		case 'p':
			pubkeyfile = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		case 's':
			seckeyfile = optarg;
			break;
		case 't':
			keytype = optarg;
			break;
		case 'x':
			sigfile = optarg;
			break;
		default:
			usage(NULL);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (embedded && gzip)
		errx(1, "can't combine -e and -z options");

	if (setvbuf(stdout, NULL, _IOLBF, 0) != 0)
		err(1, "setvbuf");

#ifndef VERIFYONLY
	if (verb == CHECK) {
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
		if (!sigfile)
			usage("must specify sigfile");
		check(pubkeyfile, sigfile, quiet, argc, argv);
		return 0;
	}
#endif

	if (argc != 0)
		usage(NULL);

	if (!sigfile && msgfile) {
		int nr;
		if (strcmp(msgfile, "-") == 0)
			usage("must specify sigfile with - message");
		nr = snprintf(sigfilebuf, sizeof(sigfilebuf),
		    "%s.sig", msgfile);
		if (nr == -1 || nr >= sizeof(sigfilebuf))
			errx(1, "path too long");
		sigfile = sigfilebuf;
	}

	switch (verb) {
#ifndef VERIFYONLY
	case GENERATE:
		/* no pledge */
		if (!pubkeyfile || !seckeyfile)
			usage("must specify pubkey and seckey");
		check_keyname_compliance(pubkeyfile, seckeyfile);
		generate(pubkeyfile, seckeyfile, none ? 0 : 42, comment);
		break;
	case SIGN:
		/* no pledge */
		if (gzip) {
			if (!msgfile || !seckeyfile || !sigfile)
				usage("must specify message sigfile seckey");
			zsign(seckeyfile, msgfile, sigfile, none);
		} else {
			if (!msgfile || !seckeyfile)
				usage("must specify message and seckey");
			sign(seckeyfile, msgfile, sigfile, embedded);
		}
		break;
#endif
	case VERIFY:
		if ((embedded || gzip) &&
		    (msgfile && strcmp(msgfile, "-") != 0)) {
			/* will need to create output file */
			if (pledge("stdio rpath wpath cpath", NULL) == -1)
				err(1, "pledge");
		} else {
			if (pledge("stdio rpath", NULL) == -1)
				err(1, "pledge");
		}
		if (gzip) {
			zverify(pubkeyfile, msgfile, sigfile, keytype);
		} else {
			if (!msgfile)
				usage("must specify message");
			verify(pubkeyfile, msgfile, sigfile, embedded,
			    quiet, keytype);
		}
		break;
	default:
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");
		usage(NULL);
		break;
	}

	return 0;
}
