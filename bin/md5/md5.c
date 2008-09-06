/*	$OpenBSD: md5.c,v 1.50 2008/09/06 12:01:34 djm Exp $	*/

/*
 * Copyright (c) 2001,2003,2005-2006 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <md4.h>
#include <md5.h>
#include <rmd160.h>
#include <sha1.h>
#include <sha2.h>
#include <crc.h>

#define STYLE_NORMAL	0
#define STYLE_REVERSE	1
#define STYLE_TERSE	2

#define MAX_DIGEST_LEN	128

enum program_mode {
	MODE_MD5,
	MODE_SHA1,
	MODE_RMD160,
	MODE_CKSUM,
	MODE_SUM
} pmode;

union ANY_CTX {
	CKSUM_CTX cksum;
	MD4_CTX md4;
	MD5_CTX md5;
	RMD160_CTX rmd160;
	SHA1_CTX sha1;
	SHA2_CTX sha2;
	SUM_CTX sum;
	SYSVSUM_CTX sysvsum;
};

/* Default print style for hash and chksum functions. */
int style_hash = STYLE_NORMAL;
int style_cksum = STYLE_REVERSE;

#define NHASHES	10
struct hash_function {
	const char *name;
	size_t digestlen;
	int *style;
	int base64;
	void *ctx;	/* XXX - only used by digest_file() */
	void (*init)(void *);
	void (*update)(void *, const unsigned char *, unsigned int);
	void (*final)(unsigned char *, void *);
	char * (*end)(void *, char *);
	TAILQ_ENTRY(hash_function) tailq;
} functions[NHASHES + 1] = {
	{
		"CKSUM",
		CKSUM_DIGEST_LENGTH,
		&style_cksum,
		-1,
		NULL,
		(void (*)(void *))CKSUM_Init,
		(void (*)(void *, const unsigned char *, unsigned int))CKSUM_Update,
		(void (*)(unsigned char *, void *))CKSUM_Final,
		(char *(*)(void *, char *))CKSUM_End
	}, {
		"SUM",
		SUM_DIGEST_LENGTH,
		&style_cksum,
		-1,
		NULL,
		(void (*)(void *))SUM_Init,
		(void (*)(void *, const unsigned char *, unsigned int))SUM_Update,
		(void (*)(unsigned char *, void *))SUM_Final,
		(char *(*)(void *, char *))SUM_End
	}, {
		"SYSVSUM",
		SYSVSUM_DIGEST_LENGTH,
		&style_cksum,
		-1,
		NULL,
		(void (*)(void *))SYSVSUM_Init,
		(void (*)(void *, const unsigned char *, unsigned int))SYSVSUM_Update,
		(void (*)(unsigned char *, void *))SYSVSUM_Final,
		(char *(*)(void *, char *))SYSVSUM_End
	}, {
		"MD4",
		MD4_DIGEST_LENGTH,
		&style_hash,
		0,
		NULL,
		(void (*)(void *))MD4Init,
		(void (*)(void *, const unsigned char *, unsigned int))MD4Update,
		(void (*)(unsigned char *, void *))MD4Final,
		(char *(*)(void *, char *))MD4End
	}, {
		"MD5",
		MD5_DIGEST_LENGTH,
		&style_hash,
		0,
		NULL,
		(void (*)(void *))MD5Init,
		(void (*)(void *, const unsigned char *, unsigned int))MD5Update,
		(void (*)(unsigned char *, void *))MD5Final,
		(char *(*)(void *, char *))MD5End
	}, {
		"RMD160",
		RMD160_DIGEST_LENGTH,
		&style_hash,
		0,
		NULL,
		(void (*)(void *))RMD160Init,
		(void (*)(void *, const unsigned char *, unsigned int))RMD160Update,
		(void (*)(unsigned char *, void *))RMD160Final,
		(char *(*)(void *, char *))RMD160End
	}, {
		"SHA1",
		SHA1_DIGEST_LENGTH,
		&style_hash,
		0,
		NULL,
		(void (*)(void *))SHA1Init,
		(void (*)(void *, const unsigned char *, unsigned int))SHA1Update,
		(void (*)(unsigned char *, void *))SHA1Final,
		(char *(*)(void *, char *))SHA1End
	}, {
		"SHA256",
		SHA256_DIGEST_LENGTH,
		&style_hash,
		0,
		NULL,
		(void (*)(void *))SHA256Init,
		(void (*)(void *, const unsigned char *, unsigned int))SHA256Update,
		(void (*)(unsigned char *, void *))SHA256Final,
		(char *(*)(void *, char *))SHA256End
	}, {
		"SHA384",
		SHA384_DIGEST_LENGTH,
		&style_hash,
		0,
		NULL,
		(void (*)(void *))SHA384Init,
		(void (*)(void *, const unsigned char *, unsigned int))SHA384Update,
		(void (*)(unsigned char *, void *))SHA384Final,
		(char *(*)(void *, char *))SHA384End
	}, {
		"SHA512",
		SHA512_DIGEST_LENGTH,
		&style_hash,
		0,
		NULL,
		(void (*)(void *))SHA512Init,
		(void (*)(void *, const unsigned char *, unsigned int))SHA512Update,
		(void (*)(unsigned char *, void *))SHA512Final,
		(char *(*)(void *, char *))SHA512End
	}, {
		NULL,
	}
};

TAILQ_HEAD(hash_list, hash_function);

void digest_end(const struct hash_function *, void *, char *, size_t, int);
void digest_file(const char *, struct hash_list *, int);
int  digest_filelist(const char *, struct hash_function *);
void digest_print(const struct hash_function *, const char *, const char *);
void digest_printstr(const struct hash_function *, const char *, const char *);
void digest_string(char *, struct hash_list *);
void digest_test(struct hash_list *);
void digest_time(struct hash_list *, int);
void hash_insert(struct hash_list *, struct hash_function *, int);
void usage(void) __attribute__((__noreturn__));

extern char *__progname;
int qflag = 0;

int
main(int argc, char **argv)
{
	struct hash_function *hf, *hftmp;
	struct hash_list hl;
	size_t len;
	char *cp, *input_string;
	int fl, error, base64;
	int bflag, cflag, pflag, rflag, tflag, xflag;

	static const char *optstr[5] = {
		"bcpqrs:tx",
		"bcpqrs:tx",
		"bcpqrs:tx",
		"a:bco:pqrs:tx",
		"a:bco:pqrs:tx"
	};

	TAILQ_INIT(&hl);
	input_string = NULL;
	error = bflag = cflag = pflag = qflag = rflag = tflag = xflag = 0;

	pmode = MODE_MD5;
	if (strcmp(__progname, "md5") == 0)
		pmode = MODE_MD5;
	else if (strcmp(__progname, "sha1") == 0)
		pmode = MODE_SHA1;
	else if (strcmp(__progname, "rmd160") == 0)
		pmode = MODE_RMD160;
	else if (strcmp(__progname, "cksum") == 0)
		pmode = MODE_CKSUM;
	else if (strcmp(__progname, "sum") == 0)
		pmode = MODE_SUM;

	/* Check for -b option early since it changes behavior. */
	while ((fl = getopt(argc, argv, optstr[pmode])) != -1) {
		switch (fl) {
		case 'b':
			bflag = 1;
			break;
		case '?':
			usage();
		}
	}
	optind = 1;
	optreset = 1;
	while ((fl = getopt(argc, argv, optstr[pmode])) != -1) {
		switch (fl) {
		case 'a':
			while ((cp = strsep(&optarg, " \t,")) != NULL) {
				if (*cp == '\0')
					continue;
				base64 = -1;
				for (hf = functions; hf->name != NULL; hf++) {
					len = strlen(hf->name);
					if (strncasecmp(cp, hf->name, len) != 0)
						continue;
					if (cp[len] == '\0') {
						if (hf->base64 != -1)
							base64 = bflag;
						break;	/* exact match */
					}
					if (cp[len + 1] == '\0' &&
					    (cp[len] == 'b' || cp[len] == 'x')) {
						base64 =
						    cp[len] == 'b' ?  1 : 0;
						break;	/* match w/ suffix */
					}
				}
				if (hf->name == NULL) {
					warnx("unknown algorithm \"%s\"", cp);
					usage();
				}
				if (hf->base64 == -1 && base64 != -1) {
					warnx("%s doesn't support %s",
					    hf->name,
					    base64 ? "base64" : "hex");
					usage();
				}
				/* Check for dupes. */
				TAILQ_FOREACH(hftmp, &hl, tailq) {
					if (hftmp->base64 == base64 &&
					    strcmp(hf->name, hftmp->name) == 0)
						break;
				}
				if (hftmp == TAILQ_END(&hl))
					hash_insert(&hl, hf, base64);
			}
			break;
		case 'b':
			/* has already been parsed */
			break;
		case 'c':
			cflag = 1;
			break;
		case 'o':
			if (strcmp(optarg, "1") == 0)
				hf = &functions[1];
			else if (strcmp(optarg, "2") == 0)
				hf = &functions[2];
			else {
				warnx("illegal argument to -o option");
				usage();
			}
			/* Check for dupes. */
			TAILQ_FOREACH(hftmp, &hl, tailq) {
				if (strcmp(hf->name, hftmp->name) == 0)
					break;
			}
			if (hftmp == TAILQ_END(&hl))
				hash_insert(&hl, hf, 0);
			break;
		case 'p':
			pflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			input_string = optarg;
			break;
		case 't':
			tflag++;
			break;
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Most arguments are mutually exclusive */
	fl = pflag + (tflag ? 1 : 0) + xflag + cflag + (input_string != NULL);
	if (fl > 1 || (fl && argc && cflag == 0) || (rflag && qflag))
		usage();
	if (cflag != 0) {
		if (TAILQ_FIRST(&hl) != TAILQ_LAST(&hl, hash_list))
			errx(1, "only a single algorithm may be specified "
			    "in -c mode");
	}

	/* No algorithm specified, check the name we were called as. */
	if (TAILQ_EMPTY(&hl)) {
		for (hf = functions; hf->name != NULL; hf++) {
			if (strcasecmp(hf->name, __progname) == 0)
				break;
		}
		if (hf->name == NULL)
			hf = &functions[0];	/* default to cksum */
		hash_insert(&hl, hf, bflag);
	}

	if (rflag)
		style_hash = STYLE_REVERSE;
	if (qflag) {
		style_hash = STYLE_TERSE;
		style_cksum = STYLE_TERSE;
	}

	if (tflag)
		digest_time(&hl, tflag);
	else if (xflag)
		digest_test(&hl);
	else if (input_string)
		digest_string(input_string, &hl);
	else if (cflag) {
		if (argc == 0)
			error = digest_filelist("-", TAILQ_FIRST(&hl));
		else
			while (argc--)
				error += digest_filelist(*argv++,
				    TAILQ_FIRST(&hl));
	} else if (pflag || argc == 0)
		digest_file("-", &hl, pflag);
	else
		while (argc--)
			digest_file(*argv++, &hl, 0);

	return(error ? EXIT_FAILURE : EXIT_SUCCESS);
}

void
hash_insert(struct hash_list *hl, struct hash_function *hf, int base64)
{
	struct hash_function *hftmp;

	hftmp = malloc(sizeof(*hftmp));
	if (hftmp == NULL)
		err(1, NULL);
	*hftmp = *hf;
	hftmp->base64 = base64;
	TAILQ_INSERT_TAIL(hl, hftmp, tailq);
}

void
digest_end(const struct hash_function *hf, void *ctx, char *buf, size_t bsize,
    int base64)
{
	u_char *digest;

	if (base64 == 1) {
		if ((digest = malloc(hf->digestlen)) == NULL)
			err(1, NULL);
		hf->final(digest, ctx);
		if (b64_ntop(digest, hf->digestlen, buf, bsize) == -1)
			errx(1, "error encoding base64");
		memset(digest, 0, sizeof(digest));
	} else {
		hf->end(ctx, buf);
	}
}

void
digest_string(char *string, struct hash_list *hl)
{
	struct hash_function *hf;
	char digest[MAX_DIGEST_LEN + 1];
	union ANY_CTX context;

	TAILQ_FOREACH(hf, hl, tailq) {
		hf->init(&context);
		hf->update(&context, string, (unsigned int)strlen(string));
		digest_end(hf, &context, digest, sizeof(digest),
		    hf->base64);
		digest_printstr(hf, string, digest);
	}
}

void
digest_print(const struct hash_function *hf, const char *what,
    const char *digest)
{
	switch (*hf->style) {
	case STYLE_NORMAL:
		(void)printf("%s (%s) = %s\n", hf->name, what, digest);
		break;
	case STYLE_REVERSE:
		(void)printf("%s %s\n", digest, what);
		break;
	case STYLE_TERSE:
		(void)printf("%s\n", digest);
		break;
	}
}

void
digest_printstr(const struct hash_function *hf, const char *what,
    const char *digest)
{
	switch (*hf->style) {
	case STYLE_NORMAL:
		(void)printf("%s (\"%s\") = %s\n", hf->name, what, digest);
		break;
	case STYLE_REVERSE:
		(void)printf("%s %s\n", digest, what);
		break;
	case STYLE_TERSE:
		(void)printf("%s\n", digest);
		break;
	}
}

void
digest_file(const char *file, struct hash_list *hl, int echo)
{
	struct hash_function *hf;
	int fd;
	ssize_t nread;
	u_char data[BUFSIZ];
	char digest[MAX_DIGEST_LEN + 1];

	if (strcmp(file, "-") == 0)
		fd = STDIN_FILENO;
	else if ((fd = open(file, O_RDONLY, 0)) == -1) {
		warn("cannot open %s", file);
		return;
	}

	if (echo)
		fflush(stdout);

	TAILQ_FOREACH(hf, hl, tailq) {
		if ((hf->ctx = malloc(sizeof(union ANY_CTX))) == NULL)
			err(1, NULL);
		hf->init(hf->ctx);
	}
	while ((nread = read(fd, data, sizeof(data))) > 0) {
		if (echo)
			write(STDOUT_FILENO, data, (size_t)nread);
		TAILQ_FOREACH(hf, hl, tailq)
			hf->update(hf->ctx, data, (unsigned int)nread);
	}
	if (nread == -1) {
		warn("%s: read error", file);
		if (fd != STDIN_FILENO)
			close(fd);
		return;
	}
	if (fd != STDIN_FILENO)
		close(fd);
	TAILQ_FOREACH(hf, hl, tailq) {
		digest_end(hf, hf->ctx, digest, sizeof(digest), hf->base64);
		free(hf->ctx);
		hf->ctx = NULL;
		if (fd == STDIN_FILENO)
			(void)puts(digest);
		else
			digest_print(hf, file, digest);
	}
}

/*
 * Parse through the input file looking for valid lines.
 * If one is found, use this checksum and file as a reference and
 * generate a new checksum against the file on the filesystem.
 * Print out the result of each comparison.
 */
int
digest_filelist(const char *file, struct hash_function *defhash)
{
	int fd, found, base64, error, cmp;
	size_t algorithm_max, algorithm_min;
	const char *algorithm;
	char *filename, *checksum, *buf, *p;
	char digest[MAX_DIGEST_LEN + 1];
	char *lbuf = NULL;
	FILE *fp;
	ssize_t nread;
	size_t len;
	u_char data[BUFSIZ];
	union ANY_CTX context;
	struct hash_function *hf;

	if (strcmp(file, "-") == 0) {
		fp = stdin;
	} else if ((fp = fopen(file, "r")) == NULL) {
		warn("cannot open %s", file);
		return(1);
	}

	algorithm_max = algorithm_min = strlen(functions[0].name);
	for (hf = &functions[1]; hf->name != NULL; hf++) {
		len = strlen(hf->name);
		algorithm_max = MAX(algorithm_max, len);
		algorithm_min = MIN(algorithm_min, len);
	}

	error = found = 0;
	while ((buf = fgetln(fp, &len))) {
		base64 = 0;
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);

			(void)memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}
		while (isspace(*buf))
			buf++;

		/*
		 * Crack the line into an algorithm, filename, and checksum.
		 * Lines are of the form:
		 *  ALGORITHM (FILENAME) = CHECKSUM
		 *
		 * Fallback on GNU form:
		 *  CHECKSUM  FILENAME
		 */
		p = strchr(buf, ' ');
		if (p != NULL && *(p + 1) == '(') {
			/* BSD form */
			*p = '\0';
			algorithm = buf;
			len = strlen(algorithm);
			if (len > algorithm_max || len < algorithm_min)
				continue;

			filename = p + 2;
			p = strrchr(filename, ')');
			if (p == NULL || strncmp(p + 1, " = ", (size_t)3) != 0)
				continue;
			*p = '\0';

			checksum = p + 4;
			p = strpbrk(checksum, " \t\r");
			if (p != NULL)
				*p = '\0';

			/*
			 * Check that the algorithm is one we recognize.
			 */
			for (hf = functions; hf->name != NULL; hf++) {
				if (strcasecmp(algorithm, hf->name) == 0)
					break;
			}
			if (hf->name == NULL || *checksum == '\0')
				continue;
			/*
			 * Check the length to see if this could be
			 * a valid checksum.  If hex, it will be 2x the
			 * size of the binary data.  For base64, we have
			 * to check both with and without the '=' padding.
			 */
			len = strlen(checksum);
			if (len != hf->digestlen * 2) {
				size_t len2;

				if (checksum[len - 1] == '=') {
					/* use padding */
					len2 = 4 * ((hf->digestlen + 2) / 3);
				} else {
					/* no padding */
					len2 = (4 * hf->digestlen + 2) / 3;
				}
				if (len != len2)
					continue;
				base64 = 1;
			}
		} else {
			/* could be GNU form */
			if ((hf = defhash) == NULL)
				continue;
			algorithm = hf->name;
			checksum = buf;
			if ((p = strchr(checksum, ' ')) == NULL)
				continue;
			if (*hf->style & STYLE_REVERSE) {
				if ((p = strchr(p + 1, ' ')) == NULL)
					continue;
			}
			*p++ = '\0';
			while (isspace(*p))
				p++;
			if (*p == '\0')
				continue;
			filename = p;
			p = strpbrk(filename, "\t\r");
			if (p != NULL)
				*p = '\0';
		}
		found = 1;

		if ((fd = open(filename, O_RDONLY, 0)) == -1) {
			warn("cannot open %s", filename);
			(void)printf("(%s) %s: FAILED\n", algorithm, filename);
			error = 1;
			continue;
		}

		hf->init(&context);
		while ((nread = read(fd, data, sizeof(data))) > 0)
			hf->update(&context, data, (unsigned int)nread);
		if (nread == -1) {
			warn("%s: read error", file);
			error = 1;
			close(fd);
			continue;
		}
		close(fd);
		digest_end(hf, &context, digest, sizeof(digest), base64);

		if (base64)
			cmp = strncmp(checksum, digest, len);
		else
			cmp = strcasecmp(checksum, digest);
		if (cmp == 0) {
			if (qflag == 0)
				(void)printf("(%s) %s: OK\n", algorithm,
				    filename);
		} else {
			(void)printf("(%s) %s: FAILED\n", algorithm, filename);
			error = 1;
		}
	}
	if (fp != stdin)
		fclose(fp);
	if (!found)
		warnx("%s: no properly formatted checksum lines found", file);
	if (lbuf != NULL)
		free(lbuf);
	return(error || !found);
}

#define TEST_BLOCK_LEN 10000
#define TEST_BLOCK_COUNT 10000

void
digest_time(struct hash_list *hl, int times)
{
	struct hash_function *hf;
	struct timeval start, stop, res;
	union ANY_CTX context;
	u_int i;
	u_char data[TEST_BLOCK_LEN];
	char digest[MAX_DIGEST_LEN + 1];
	double elapsed;
	int count = TEST_BLOCK_COUNT;
	while (--times > 0 && count < INT_MAX / 10)
		count *= 10;

	TAILQ_FOREACH(hf, hl, tailq) {
		(void)printf("%s time trial.  Processing %d %d-byte blocks...",
		    hf->name, count, TEST_BLOCK_LEN);
		fflush(stdout);

		/* Initialize data based on block number. */
		for (i = 0; i < TEST_BLOCK_LEN; i++)
			data[i] = (u_char)(i & 0xff);

		gettimeofday(&start, NULL);
		hf->init(&context);
		for (i = 0; i < count; i++)
			hf->update(&context, data, TEST_BLOCK_LEN);
		digest_end(hf, &context, digest, sizeof(digest), hf->base64);
		gettimeofday(&stop, NULL);
		timersub(&stop, &start, &res);
		elapsed = res.tv_sec + res.tv_usec / 1000000.0;

		(void)printf("\nDigest = %s\n", digest);
		(void)printf("Time   = %f seconds\n", elapsed);
		(void)printf("Speed  = %f bytes/second\n",
		    (double)TEST_BLOCK_LEN * count / elapsed);
	}
}

void
digest_test(struct hash_list *hl)
{
	struct hash_function *hf;
	union ANY_CTX context;
	int i;
	char digest[MAX_DIGEST_LEN + 1];
	unsigned char buf[1000];
	unsigned const char *test_strings[] = {
		"",
		"a",
		"abc",
		"message digest",
		"abcdefghijklmnopqrstuvwxyz",
		"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
		    "0123456789",
		"12345678901234567890123456789012345678901234567890123456789"
		    "012345678901234567890",
	};

	TAILQ_FOREACH(hf, hl, tailq) {
		(void)printf("%s test suite:\n", hf->name);

		for (i = 0; i < 8; i++) {
			hf->init(&context);
			hf->update((void *)&context, test_strings[i],
			    (unsigned int)strlen(test_strings[i]));
			digest_end(hf, &context, digest, sizeof(digest),
			    hf->base64);
			digest_printstr(hf, test_strings[i], digest);
		}

		/* Now simulate a string of a million 'a' characters. */
		memset(buf, 'a', sizeof(buf));
		hf->init(&context);
		for (i = 0; i < 1000; i++)
			hf->update(&context, buf,
			    (unsigned int)sizeof(buf));
		digest_end(hf, &context, digest, sizeof(digest), hf->base64);
		digest_print(hf, "one million 'a' characters",
		    digest);
	}
}

void
usage(void)
{
	switch (pmode) {
	case MODE_MD5:
	case MODE_SHA1:
	case MODE_RMD160:
		fprintf(stderr, "usage: %s [-bpqrtx] [-c [checklist ...]] "
		    "[-s string] [file ...]\n", __progname);
		break;
	case MODE_CKSUM:
	case MODE_SUM:
		fprintf(stderr, "usage: %s [-bpqrtx] [-a algorithms] "
		    "[-c [checklist ...]] [-o 1 | 2]\n"
		    "       %*s [-s string] [file ...]\n",
		    __progname, (int)strlen(__progname), "");
		break;
	}

	exit(EXIT_FAILURE);
}
