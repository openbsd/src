/*	$OpenBSD: md5.c,v 1.16 2003/01/14 17:15:53 millert Exp $	*/

/*
 * Copyright (c) 2001 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
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

#define DIGEST_MD5	0
#define DIGEST_SHA1	1
#define DIGEST_RMD160	2

union ANY_CTX {
	MD5_CTX md5;
	SHA1_CTX sha1;
	RMD160_CTX rmd160;
};

struct hash_functions {
	char *name;
	void (*init)();
	void (*update)();
	char * (*end)();
	char * (*file)();
	char * (*data)();
};
struct hash_functions functions[] = {
	{
		"MD5",
		MD5Init, MD5Update, MD5End, MD5File, MD5Data
	}, {
		"SHA1",
		SHA1Init, SHA1Update, SHA1End, SHA1File, SHA1Data
	}, {
		"RMD160",
		RMD160Init, RMD160Update, RMD160End, RMD160File, RMD160Data
	},
};

extern char *__progname;
static void usage(void);
static void digest_file(char *, struct hash_functions *, int);
static void digest_string(char *, struct hash_functions *);
static void digest_test(struct hash_functions *);
static void digest_time(struct hash_functions *);

int
main(int argc, char **argv)
{
	int fl, digest_type;
	int pflag, tflag, xflag;
	char *input_string;

	/* Set digest type based on program name, defaults to MD5. */
	if (strcmp(__progname, "rmd160") == 0)
		digest_type = DIGEST_RMD160;
	else if (strcmp(__progname, "sha1") == 0)
		digest_type = DIGEST_SHA1;
	else
		digest_type = DIGEST_MD5;

	input_string = NULL;
	pflag = tflag = xflag = 0;
	while ((fl = getopt(argc, argv, "ps:tx")) != -1) {
		switch (fl) {
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
	fl = pflag + tflag + xflag + (input_string != NULL);
	if (fl > 1 || (fl && argc))
		usage();

	if (tflag)
		digest_time(&functions[digest_type]);
	else if (xflag)
		digest_test(&functions[digest_type]);
	else if (input_string)
		digest_string(input_string, &functions[digest_type]);
	else if (pflag || argc == 0)
		digest_file("-", &functions[digest_type], pflag);
	else
		while (argc--)
			digest_file(*argv++, &functions[digest_type], 0);

	exit(0);
}

static void
digest_string(char *string, struct hash_functions *hf)
{
	char *digest;

	digest = hf->data(string, strlen(string), NULL);
	(void)printf("%s (\"%s\") = %s\n", hf->name, string, digest);
	free(digest);
}

static void
digest_file(char *file, struct hash_functions *hf, int echo)
{
	int fd;
	ssize_t nread;
	u_char data[BUFSIZ];
	char *digest;
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
		hf->update(&context, data, nread);
	}
	if (nread == -1) {
		warn("%s: read error", file);
		if (fd != STDIN_FILENO)
			close(fd);
		return;
	}
	digest = hf->end(&context, NULL);

	if (fd == STDIN_FILENO) {
		(void)puts(digest);
	} else {
		close(fd);
		(void)printf("%s (%s) = %s\n", hf->name, file, digest);
	}
	free(digest);
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
	char *digest;
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
	digest = hf->end(&context, NULL);
	gettimeofday(&stop, NULL);
	timersub(&stop, &start, &res);
	elapsed = res.tv_sec + res.tv_usec / 1000000.0;

	(void)printf("\nDigest = %s\n", digest);
	(void)printf("Time   = %f seconds\n", elapsed);
	(void)printf("Speed  = %f bytes/second\n",
	    TEST_BLOCK_LEN * TEST_BLOCK_COUNT / elapsed);
	free(digest);
}

static void
digest_test(struct hash_functions *hf)
{
	union ANY_CTX context;
	int i;
	char *digest, buf[1000];
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
		hf->update(&context, test_strings[i], strlen(test_strings[i]));
		digest = hf->end(&context, NULL);
		(void)printf("%s (\"%s\") = %s\n", hf->name, test_strings[i],
		    digest);
		free(digest);
	}

	/* Now simulate a string of a million 'a' characters. */
	memset(buf, 'a', sizeof(buf));
	hf->init(&context);
	for (i = 0; i < 1000; i++)
		hf->update(&context, buf, sizeof(buf));
	digest = hf->end(&context, NULL);
	(void)printf("%s (one million 'a' characters) = %s\n",
	    hf->name, digest);
	free(digest);

}

static void
usage()
{
	fprintf(stderr, "usage: %s [-p | -t | -x | -s string | file ...]\n",
	    __progname);
	exit(1);
}
