/*
 * Copyright (c) 2002 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

/*
 * Simple getopt_long() and getopt_long_only() excerciser.
 * ENVIRONMENT:
 *	LONG_ONLY	: use getopt_long_only() (default is getopt_long())
 *	POSIXLY_CORRECT	: don't permute args
 */

int
main(int argc, char **argv)
{
	int ch, idx, goggles;
	int (*gl)(int, char * const *, const char *, const struct option *, int *);
	struct option longopts[] = {
		{ "force", no_argument, 0, 0 },
		{ "fast", no_argument, 0, '1' },
		{ "best", no_argument, 0, '9' },
		{ "input", required_argument, 0, 'i' },
		{ "illiterate", no_argument, 0, 0 },
		{ "drinking", required_argument, &goggles, 42 },
		{ "help", no_argument, 0, 'h' },
		{ 0, 0, 0, 0 },
	};

	if (getenv("LONG_ONLY")) {
		gl = getopt_long_only;
		printf("getopt_long_only");
	} else {
		gl = getopt_long;
		printf("getopt_long");
	}
	if (getenv("POSIXLY_CORRECT"))
		printf(" (POSIXLY_CORRECT)");
	printf(": ");
	for (idx = 1; idx < argc; idx++)
		printf("%s ", argv[idx]);
	printf("\n");

	goggles = 0;
	for (;;) {
		idx = -1;
		ch = gl(argc, argv, "19bf:i:h", longopts, &idx);
		if (ch == -1)
			break;
		switch (ch) {
		case 0:
		case '1':
		case '9':
		case 'h':
			if (idx != -1) {
				if (goggles == 42)
					printf("option %s, arg %s\n",
					    longopts[idx].name, optarg);
				else
					printf("option %s\n",
					    longopts[idx].name);
			} else
				printf("option %c\n", ch);
			break;
		case 'f':
		case 'i':
			if (idx != -1)
				printf("option %s, arg %s\n",
				    longopts[idx].name, optarg);
			else
				printf("option %c, arg %s\n", ch, optarg);
			break;

		case '?':
			break;

		default:
			printf("unexpected return value: %c\n", ch);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		printf("remaining ARGV: ");
		while (argc--)
			printf("%s ", *argv++);
		printf("\n");
	}
	printf("\n");

	exit (0);
}
