/*	$OpenBSD: md5.c,v 1.28 2004/04/28 23:58:41 millert Exp $	*/

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

#include <md5.h>
#include <sha1.h>
#include <rmd160.h>

#define MAX_DIGEST_LEN	40

union ANY_CTX {
	MD5_CTX md5;
	RMD160_CTX rmd160;
	SHA1_CTX sha1;
};

struct hash_functions {
	char *name;
	int digestlen;
	void (*init)(void *);
	void (*update)(void *, const unsigned char *, unsigned int);
	char * (*end)(void *, char *);
	char * (*file)(char *, char *);
	char * (*data)(const unsigned char *, unsigned int, char *);
};

struct hash_functions functions[] = {
	{
		"MD5",
		MD5_DIGEST_LENGTH * 2,
		(void (*)(void *))MD5Init,
		(void (*)(void *, const unsigned char *, unsigned int))MD5Update,
		(char *(*)(void *, char *))MD5End,
		(char *(*)(char *, char *))MD5File,
		(char *(*)(const unsigned char *, unsigned int, char *))MD5Data
	}, {
		"RMD160",
		RMD160_DIGEST_LENGTH * 2,
		(void (*)(void *))RMD160Init,
		(void (*)(void *, const unsigned char *, unsigned int))RMD160Update,
		(char *(*)(void *, char *))RMD160End,
		(char *(*)(char *, char *))RMD160File,
		(char *(*)(const unsigned char *, unsigned int, char *))RMD160Data
	}, {
		"SHA1",
		SHA1_DIGEST_LENGTH * 2,
		(void (*)(void *))SHA1Init,
		(void (*)(void *, const unsigned char *, unsigned int))SHA1Update,
		(char *(*)(void *, char *))SHA1End,
		(char *(*)(char *, char *))SHA1File,
		(char *(*)(const unsigned char *, unsigned int, char *))SHA1Data
	}, {
		NULL,
	},
};

extern char *__progname;
static void usage(void);
static void digest_file(char *, struct hash_functions *, int);
static int digest_filelist(int, char *);
static void digest_string(char *, struct hash_functions *);
static void digest_test(struct hash_functions *);
static void digest_time(struct hash_functions *);

int
main(int argc, char **argv)
{
	int fl, digest_type, error;
	int cflag, pflag, tflag, xflag;
	char *input_string;

	/* Set digest type based on program name, defaults to MD5. */
	for (digest_type = 0; functions[digest_type].name != NULL;
	    digest_type++) {
		if (strcasecmp(functions[digest_type].name, __progname) == 0)
			break;
	}
	if (functions[digest_type].name == NULL)
		digest_type = 0;

	input_string = NULL;
	error = cflag = pflag = tflag = xflag = 0;
	while ((fl = getopt(argc, argv, "pctxs:")) != -1) {
		switch (fl) {
		case 'c':
			cflag = 1;
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

	/* All arguments are mutually exclusive */
	fl = pflag + tflag + xflag + cflag + (input_string != NULL);
	if (fl > 1 || (fl && argc && cflag == 0))
		usage();

	if (tflag)
		digest_time(&functions[digest_type]);
	else if (xflag)
		digest_test(&functions[digest_type]);
	else if (input_string)
		digest_string(input_string, &functions[digest_type]);
	else if (cflag)
		if (argc == 0)
			error = digest_filelist(digest_type, "-");
		else
			while (argc--)
				error += digest_filelist(digest_type, *argv++);
	else if (pflag || argc == 0)
		digest_file("-", &functions[digest_type], pflag);
	else
		while (argc--)
			digest_file(*argv++, &functions[digest_type], 0);

	return(error ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
digest_string(char *string, struct hash_functions *hf)
{
	char digest[MAX_DIGEST_LEN + 1];

	(void)hf->data((unsigned char *)string, (unsigned int)strlen(string),
	    digest);
	(void)printf("%s (\"%s\") = %s\n", hf->name, string, digest);
}

static void
digest_file(char *file, struct hash_functions *hf, int echo)
{
	int fd;
	ssize_t nread;
	u_char data[BUFSIZ];
	char digest[MAX_DIGEST_LEN + 1];
	union ANY_CTX context;

	if (strcmp(file, "-") == 0)
		fd = STDIN_FILENO;
	else if ((fd = open(file, O_RDONLY, 0)) == -1) {
		warn("cannot open %s", file);
		return;
	}

	if (echo)
		fflush(stdout);

	hf->init(&context);
	while ((nread = read(fd, data, sizeof(data))) > 0) {
		if (echo)
			write(STDOUT_FILENO, data, (size_t)nread);
		hf->update(&context, data, (unsigned int)nread);
	}
	if (nread == -1) {
		warn("%s: read error", file);
		if (fd != STDIN_FILENO)
			close(fd);
		return;
	}
	(void)hf->end(&context, digest);

	if (fd == STDIN_FILENO) {
		(void)puts(digest);
	} else {
		close(fd);
		(void)printf("%s (%s) = %s\n", hf->name, file, digest);
	}
}

/*
 * Parse through the input file looking for valid lines.
 * If one is found, use this checksum and file as a reference and
 * generate a new checksum against the file on the filesystem.
 * Print out the result of each comparison.
 */
static int
digest_filelist(int algorithm_def, char *file)
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
			hf = &functions[algorithm_def];
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
			p = strpbrk(filename, " \t\r");
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

static void
digest_time(struct hash_functions *hf)
{
	struct timeval start, stop, res;
	union ANY_CTX context;
	u_int i;
	u_char data[TEST_BLOCK_LEN];
	char digest[MAX_DIGEST_LEN + 1];
	double elapsed;

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

static void
digest_test(struct hash_functions *hf)
{
	union ANY_CTX context;
	int i;
	char digest[MAX_DIGEST_LEN + 1];
	unsigned char buf[1000];
	char *test_strings[] = {
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

	(void)printf("%s test suite:\n", hf->name);

	for (i = 0; i < 8; i++) {
		hf->init(&context);
		hf->update((void *)&context, (unsigned char *)test_strings[i],
		    (unsigned int)strlen(test_strings[i]));
		(void)hf->end(&context, digest);
		(void)printf("%s (\"%s\") = %s\n", hf->name, test_strings[i],
		    digest);
	}

	/* Now simulate a string of a million 'a' characters. */
	memset(buf, 'a', sizeof(buf));
	hf->init(&context);
	for (i = 0; i < 1000; i++)
		hf->update(&context, (unsigned char *)buf,
		    (unsigned int)sizeof(buf));
	(void)hf->end(&context, digest);
	(void)printf("%s (one million 'a' characters) = %s\n",
	    hf->name, digest);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-p | -t | -x | -c [ checksum_file ... ]",
	    __progname);
	fprintf(stderr, " | -s string | file ...]\n");
	exit(EXIT_FAILURE);
}
