/*	$OpenBSD: noexec.c,v 1.1 2002/08/31 22:56:01 mickey Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <err.h>

volatile sig_atomic_t fail;
int page_size;
char label[64] = "non-exec ";

#define PAD 64*1024
#define TEST 256	/* assuming the testfly() will fit */
u_int64_t data[PAD+TEST] = { 0 };
u_int64_t bss[PAD+TEST];

void
testfly()
{
}

void
sigsegv(int sig, siginfo_t *sip, void *scp)
{
	_exit(fail);
}

int
noexec(void *p, size_t size)
{

	fail = 0;
	printf("%s: execute\n", label);
	((void (*)(void))p)();

	return (1);
}

int
noexec_mprotect(void *p, size_t size)
{

	/* here we must fail on segv since we said it gets executable */
	fail = 1;
	mprotect(p, size, PROT_READ|PROT_WRITE|PROT_EXEC);
	printf("%s: execute\n", label);
	((void (*)(void))p)();

	/* here we are successfull on segv and fail if it still executes */
	fail = 0;
	mprotect(p, size, PROT_READ|PROT_WRITE);
	printf("%s: catch a signal\n", label);
	((void (*)(void))p)();

	return (1);
}

void *
getaddr(void *a)
{
	void *ret;

	if ((void *)&ret < a)
		ret = (void *)((u_long)&ret - 4 * page_size);
	else
		ret = (void *)((u_long)&ret + 4 * page_size);

	return (void *)((u_long)ret & ~(page_size - 1));
}

int
noexec_mmap(void *p, size_t size)
{
	void *addr;

	/* here we must fail on segv since we said it gets executable */
	fail = 1;
	if ((addr = mmap(p, size, PROT_READ|PROT_WRITE|PROT_EXEC,
	    MAP_ANON|MAP_FIXED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");
	printf("%s: execute\n", label);
	((void (*)(void))addr)();

	exit(0);
}

void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "Usage: %s [-s <size>] -[TDBHS] [-p] [-m]\n",
	    __progname);
	exit(2);
}

int
main(int argc, char *argv[])
{
	u_int64_t stack[256];	/* assuming the testfly() will fit */
	struct sigaction sa;
	int (*func)(void *, size_t);
	size_t size;
	char *ep;
	void *p;
	int ch;

	if ((page_size = sysconf(_SC_PAGESIZE)) < 0)
		err(1, "sysconf");

	p = NULL;
	func = &noexec;
	size = TEST;
	while ((ch = getopt(argc, argv, "TDBHSmps")) != -1)
		switch (ch) {
		case 'T':
			p = &testfly;
			strcat(label, "text");
			break;
		case 'D':
			p = &data[PAD];
			strcat(label, "data");
			break;
		case 'B':
			p = &bss[PAD];
			strcat(label, "bss");
			break;
		case 'H':
			p = malloc(TEST);	/* XXX align? */
			if (p == NULL)
				err(2, "malloc");
			strcat(label, "heap");
			break;
		case 'S':
			p = &stack;
			strcat(label, "stack");
			break;
		case 's':	/* only valid for stack */
			size = strtoul(optarg, &ep, 0);
			if (size > ULONG_MAX)
				errno = ERANGE;
			if (errno)
				err(1, "invalid size: %s", optarg);
			if (*ep)
				errx(1, "invalid size: %s", optarg);
			break;
		case 'm':
			func = &noexec_mmap;
			break;
		case 'p':
			func = &noexec_mprotect;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

	if (p == NULL)
		exit(2);

	sa.sa_sigaction = &sigsegv;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, NULL);

	memcpy(p, &testfly, TEST);

	exit((*func)(p, size));
}
