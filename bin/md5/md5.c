/*	$OpenBSD: md5.c,v 1.32 2004/12/29 17:32:44 millert Exp $	*/

/*
 * Copyright (c) 2001, 2003 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
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

#define MAX_DIGEST_LEN	128

union ANY_CTX {
	CKSUM_CTX cksum;
	MD4_CTX md4;
	MD5_CTX md5;
	RMD160_CTX rmd160;
	SHA1_CTX sha1;
	SHA256_CTX sha256;
	SHA384_CTX sha384;
	SHA512_CTX sha512;
	SUM_CTX sum;
	SYSVSUM_CTX sysvsum;
};

void digest_print(const char *, const char *, const char *);
void digest_print_short(const char *, const char *, const char *);
void digest_print_string(const char *, const char *, const char *);

#define NHASHES	10
struct hash_functions {
	char *name;
	int digestlen;
	void *ctx;	/* XXX - only used by digest_file() */
	void (*init)(void *);
	void (*update)(void *, const unsigned char *, unsigned int);
	char * (*end)(void *, char *);
	void (*print)(const char *, const char *, const char *);
	void (*printstr)(const char *, const char *, const char *);
} functions[NHASHES + 1] = {
	{
		"CKSUM",
		CKSUM_DIGEST_LENGTH * 2,
		NULL,
		(void (*)(void *))CKSUM_Init,
		(void (*)(void *, const unsigned char *, unsigned int))CKSUM_Update,
		(char *(*)(void *, char *))CKSUM_End,
		digest_print_short,
		digest_print_short
	}, {
		"SUM",
		SUM_DIGEST_LENGTH * 2,
		NULL,
		(void (*)(void *))SUM_Init,
		(void (*)(void *, const unsigned char *, unsigned int))SUM_Update,
		(char *(*)(void *, char *))SUM_End,
		digest_print_short,
		digest_print_short
	}, {
		"SYSVSUM",
		SYSVSUM_DIGEST_LENGTH * 2,
		NULL,
		(void (*)(void *))SYSVSUM_Init,
		(void (*)(void *, const unsigned char *, unsigned int))SYSVSUM_Update,
		(char *(*)(void *, char *))SYSVSUM_End,
		digest_print_short,
		digest_print_short
	}, {
		"MD4",
		MD5_DIGEST_LENGTH * 2,
		NULL,
		(void (*)(void *))MD4Init,
		(void (*)(void *, const unsigned char *, unsigned int))MD4Update,
		(char *(*)(void *, char *))MD4End,
		digest_print,
		digest_print_string
	}, {
		"MD5",
		MD5_DIGEST_LENGTH * 2,
		NULL,
		(void (*)(void *))MD5Init,
		(void (*)(void *, const unsigned char *, unsigned int))MD5Update,
		(char *(*)(void *, char *))MD5End,
		digest_print,
		digest_print_string
	}, {
		"RMD160",
		RMD160_DIGEST_LENGTH * 2,
		NULL,
		(void (*)(void *))RMD160Init,
		(void (*)(void *, const unsigned char *, unsigned int))RMD160Update,
		(char *(*)(void *, char *))RMD160End,
		digest_print,
		digest_print_string
	}, {
		"SHA1",
		SHA1_DIGEST_LENGTH * 2,
		NULL,
		(void (*)(void *))SHA1Init,
		(void (*)(void *, const unsigned char *, unsigned int))SHA1Update,
		(char *(*)(void *, char *))SHA1End,
		digest_print,
		digest_print_string
	}, {
		"SHA256",
		SHA256_DIGEST_LENGTH * 2,
		NULL,
		(void (*)(void *))SHA256_Init,
		(void (*)(void *, const unsigned char *, unsigned int))SHA256_Update,
		(char *(*)(void *, char *))SHA256_End,
		digest_print,
		digest_print_string
	}, {
		"SHA384",
		SHA384_DIGEST_LENGTH * 2,
		NULL,
		(void (*)(void *))SHA384_Init,
		(void (*)(void *, const unsigned char *, unsigned int))SHA384_Update,
		(char *(*)(void *, char *))SHA384_End,
		digest_print,
		digest_print_string
	}, {
		"SHA512",
		SHA512_DIGEST_LENGTH * 2,
		NULL,
		(void (*)(void *))SHA512_Init,
		(void (*)(void *, const unsigned char *, unsigned int))SHA512_Update,
		(char *(*)(void *, char *))SHA512_End,
		digest_print,
		digest_print_string
	}, {
		NULL,
	},
};

void usage(void) __attribute__((__noreturn__));
void digest_file(const char *, struct hash_functions **, int);
int digest_filelist(const char *, struct hash_functions *);
void digest_string(char *, struct hash_functions **);
void digest_test(struct hash_functions **);
void digest_time(struct hash_functions **);

extern char *__progname;

int
main(int argc, char **argv)
{
	struct hash_functions *hf, *hashes[NHASHES + 1];
	int fl, i, error;
	int cflag, pflag, tflag, xflag;
	char *cp, *input_string;

	input_string = NULL;
	error = cflag = pflag = tflag = xflag = 0;
	memset(hashes, 0, sizeof(hashes));
	while ((fl = getopt(argc, argv, "a:co:ps:tx")) != -1) {
		switch (fl) {
		case 'a':
			while ((cp = strsep(&optarg, " \t,")) != NULL) {
				if (*cp == '\0')
					continue;
				for (hf = functions; hf->name != NULL; hf++)
					if (strcasecmp(hf->name, cp) == 0)
						break;
				if (hf->name == NULL) {
					warnx("unknown algorithm \"%s\"", cp);
					usage();
				}
				for (i = 0; i < NHASHES && hashes[i] != hf; i++)
					if (hashes[i] == NULL) {
						hashes[i] = hf;
						break;
					}
			}
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
			for (i = 0; i < NHASHES && hashes[i] != hf; i++) {
				if (hashes[i] == NULL) {
					hashes[i] = hf;
					break;
				}
			}
			break;
		case 'p':
			pflag = 1;
			break;
		case 's':
			input_string = optarg;
			break;
		case 't':
			tflag = 1;
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
	fl = pflag + tflag + xflag + cflag + (input_string != NULL);
	if (fl > 1 || (fl && argc && cflag == 0))
		usage();
	if (cflag != 0 && hashes[1] != NULL)
		errx(1, "only a single algorithm may be specified in -c mode");

	/* No algorithm specified, check the name we were called as. */
	if (hashes[0] == NULL) {
		for (hf = functions; hf->name != NULL; hf++) {
			if (strcasecmp(hf->name, __progname) == 0) {
				hashes[0] = hf;
				break;
			}
		}
		if (hashes[0] == NULL)
			hashes[0] = &functions[0];	/* default to cksum */
	}

	if (tflag)
		digest_time(hashes);
	else if (xflag)
		digest_test(hashes);
	else if (input_string)
		digest_string(input_string, hashes);
	else if (cflag) {
		if (argc == 0)
			error = digest_filelist("-", hashes[0]);
		else
			while (argc--)
				error += digest_filelist(*argv++, hashes[0]);
	} else if (pflag || argc == 0)
		digest_file("-", hashes, pflag);
	else
		while (argc--)
			digest_file(*argv++, hashes, 0);

	return(error ? EXIT_FAILURE : EXIT_SUCCESS);
}

void
digest_string(char *string, struct hash_functions **hashes)
{
	struct hash_functions *hf;
	char digest[MAX_DIGEST_LEN + 1];
	union ANY_CTX context;

	while (*hashes != NULL) {
		hf = *hashes++;
		hf->init(&context);
		hf->update(&context, string, (unsigned int)strlen(string));
		(void)hf->end(&context, digest);
		hf->printstr(hf->name, string, digest);
	}
}

void
digest_print(const char *name, const char *what, const char *digest)
{
	(void)printf("%s (%s) = %s\n", name, what, digest);
}

void
digest_print_string(const char *name, const char *what, const char *digest)
{
	(void)printf("%s (\"%s\") = %s\n", name, what, digest);
}

void
digest_print_short(const char *name, const char *what, const char *digest)
{
	(void)printf("%s %s\n", digest, what);
}

void
digest_file(const char *file, struct hash_functions **hashes, int echo)
{
	struct hash_functions **hfp;
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

	for (hfp = hashes; *hfp != NULL; hfp++) {
		if (((*hfp)->ctx = malloc(sizeof(union ANY_CTX))) == NULL)
			err(1, NULL);
		(*hfp)->init((*hfp)->ctx);
	}
	while ((nread = read(fd, data, sizeof(data))) > 0) {
		if (echo)
			write(STDOUT_FILENO, data, (size_t)nread);
		for (hfp = hashes; *hfp != NULL; hfp++)
			(*hfp)->update((*hfp)->ctx, data, (unsigned int)nread);
	}
	if (nread == -1) {
		warn("%s: read error", file);
		if (fd != STDIN_FILENO)
			close(fd);
		return;
	}
	if (fd != STDIN_FILENO)
		close(fd);
	for (hfp = hashes; *hfp != NULL; hfp++) {
		(void)(*hfp)->end((*hfp)->ctx, digest);
		free((*hfp)->ctx);
		if (fd == STDIN_FILENO)
			(void)puts(digest);
		else
			(*hfp)->print((*hfp)->name, file, digest);
	}
}

/*
 * Parse through the input file looking for valid lines.
 * If one is found, use this checksum and file as a reference and
 * generate a new checksum against the file on the filesystem.
 * Print out the result of each comparison.
 */
int
digest_filelist(const char *file, struct hash_functions *defhash)
{
	int fd, found, error;
	int algorithm_max, algorithm_min;
	char *algorithm, *filename, *checksum, *buf, *p;
	char digest[MAX_DIGEST_LEN + 1];
	char *lbuf = NULL;
	FILE *fp;
	ssize_t nread;
	size_t len;
	u_char data[BUFSIZ];
	union ANY_CTX context;
	struct hash_functions *hf;

	if (strcmp(file, "-") == 0) {
		fp = stdin;
	} else if ((fp = fopen(file, "r")) == NULL) {
		warn("cannot open %s", file);
		return(1);
	}

	if (defhash < &functions[3])
		defhash = NULL;	/* No GNU format for sum, cksum, sysvsum */

	algorithm_max = algorithm_min = strlen(functions[0].name);
	for (hf = &functions[1]; hf->name != NULL; hf++) {
		len = strlen(hf->name);
		algorithm_max = MAX(algorithm_max, len);
		algorithm_min = MIN(algorithm_min, len);
	}

	error = found = 0;
	while ((buf = fgetln(fp, &len))) {
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
				if (strcmp(algorithm, hf->name) == 0)
					break;
			}
			if (hf->name == NULL ||
			    strlen(checksum) != hf->digestlen)
				continue;
		} else {
			/* could be GNU form */
			if ((hf = defhash) == NULL)
				continue;
			algorithm = hf->name;
			checksum = buf;
			if ((p = strchr(checksum, ' ')) == NULL)
				continue;
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

		if ((fd = open(filename, O_RDONLY, 0)) == -1) {
			warn("cannot open %s", filename);
			(void)printf("(%s) %s: FAILED\n", algorithm, filename);
			error = 1;
			continue;
		}

		found = 1;
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
		(void)hf->end(&context, digest);

		if (strcmp(checksum, digest) == 0)
			(void)printf("(%s) %s: OK\n", algorithm, filename);
		else
			(void)printf("(%s) %s: FAILED\n", algorithm, filename);
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
digest_time(struct hash_functions **hashes)
{
	struct hash_functions *hf;
	struct timeval start, stop, res;
	union ANY_CTX context;
	u_int i;
	u_char data[TEST_BLOCK_LEN];
	char digest[MAX_DIGEST_LEN + 1];
	double elapsed;

	while (*hashes != NULL) {
		hf = *hashes++;
		(void)printf("%s time trial.  Processing %d %d-byte blocks...",
		    hf->name, TEST_BLOCK_COUNT, TEST_BLOCK_LEN);
		fflush(stdout);

		/* Initialize data based on block number. */
		for (i = 0; i < TEST_BLOCK_LEN; i++)
			data[i] = (u_char)(i & 0xff);

		gettimeofday(&start, NULL);
		hf->init(&context);
		for (i = 0; i < TEST_BLOCK_COUNT; i++)
			hf->update(&context, data, TEST_BLOCK_LEN);
		(void)hf->end(&context, digest);
		gettimeofday(&stop, NULL);
		timersub(&stop, &start, &res);
		elapsed = res.tv_sec + res.tv_usec / 1000000.0;

		(void)printf("\nDigest = %s\n", digest);
		(void)printf("Time   = %f seconds\n", elapsed);
		(void)printf("Speed  = %f bytes/second\n",
		    TEST_BLOCK_LEN * TEST_BLOCK_COUNT / elapsed);
	}
}

void
digest_test(struct hash_functions **hashes)
{
	struct hash_functions *hf;
	union ANY_CTX context;
	int i;
	char digest[MAX_DIGEST_LEN + 1];
	unsigned char buf[1000];
	unsigned char *test_strings[] = {
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

	while (*hashes != NULL) {
		hf = *hashes++;
		(void)printf("%s test suite:\n", hf->name);

		for (i = 0; i < 8; i++) {
			hf->init(&context);
			hf->update((void *)&context, test_strings[i],
			    (unsigned int)strlen(test_strings[i]));
			(void)hf->end(&context, digest);
			hf->printstr(hf->name, test_strings[i], digest);
		}

		/* Now simulate a string of a million 'a' characters. */
		memset(buf, 'a', sizeof(buf));
		hf->init(&context);
		for (i = 0; i < 1000; i++)
			hf->update(&context, buf,
			    (unsigned int)sizeof(buf));
		(void)hf->end(&context, digest);
		hf->print(hf->name, "one million 'a' characters",
		    digest);
	}
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-p | -t | -x | -c [checklist ...] | "
	    "-s string | file ...]\n", __progname);
	if (strcmp(__progname, "cksum") == 0)
		fprintf(stderr, "             [-a algorithms]] [-o 1 | 2]\n");

	exit(EXIT_FAILURE);
}
