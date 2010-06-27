/*	$OpenBSD: noexec.c,v 1.11 2010/06/27 17:42:23 art Exp $	*/

/*
 * Copyright (c) 2002,2003 Michael Shalayeff
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

#include <sys/param.h>
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
#define	MAXPAGESIZE 8192
#define TESTSZ 256	/* assuming the testfly() will fit */
u_int64_t data[(PAD + TESTSZ + PAD + MAXPAGESIZE) / 8] = { 0 };
u_int64_t bss[(PAD + TESTSZ + PAD + MAXPAGESIZE) / 8];

void testfly(void);

static void
fdcache(void *p, size_t size)
{
#ifdef __hppa__
	__asm __volatile(	/* XXX this hardcodes the TESTSZ */
	    "fdc,m	%1(%0)\n\t"
	    "fdc,m	%1(%0)\n\t"
	    "fdc,m	%1(%0)\n\t"
	    "fdc,m	%1(%0)\n\t"
	    "fdc,m	%1(%0)\n\t"
	    "fdc,m	%1(%0)\n\t"
	    "fdc,m	%1(%0)\n\t"
	    "fdc,m	%1(%0)"
	    : "+r" (p) : "r" (32));
#endif
}

static void
sigsegv(int sig, siginfo_t *sip, void *scp)
{
	_exit(fail);
}

static int
noexec(void *p, size_t size)
{
	fail = 0;
	printf("%s: execute\n", label);
	fflush(stdout);
	((void (*)(void))p)();

	return (1);
}

static int
noexec_mprotect(void *p, size_t size)
{

	/* here we must fail on segv since we said it gets executable */
	fail = 1;
	if (mprotect(p, size, PROT_READ|PROT_EXEC) < 0)
		err(1, "mprotect 1");
	printf("%s: execute\n", label);
	fflush(stdout);
	((void (*)(void))p)();

	/* here we are successful on segv and fail if it still executes */
	fail = 0;
	if (mprotect(p, size, PROT_READ) < 0)
		err(1, "mprotect 2");
	printf("%s: catch a signal\n", label);
	fflush(stdout);
	((void (*)(void))p)();

	return (1);
}

static void *
getaddr(void *a)
{
	void *ret;

	if ((void *)&ret < a)
		ret = (void *)((u_long)&ret - 4 * page_size);
	else
		ret = (void *)((u_long)&ret + 4 * page_size);

	return (void *)((u_long)ret & ~(page_size - 1));
}

static int
noexec_mmap(void *p, size_t size)
{
	memcpy(p + page_size * 1, p, page_size);
	memcpy(p + page_size * 2, p, page_size);
	fdcache(p + page_size * 1, TESTSZ);
	fdcache(p + page_size * 2, TESTSZ);

	/* here we must fail on segv since we said it gets executable */
	fail = 1;

	printf("%s: execute #1\n", label);
	fflush(stdout);
	((void (*)(void))p)();

	/* unmap the first page to see that the higher page is still exec */
	if (munmap(p, page_size) < 0)
		err(1, "munmap");

	p += page_size;
	printf("%s: execute #2\n", label);
	fflush(stdout);
	((void (*)(void))p)();

	/* unmap the last page to see that the lower page is still exec */
	if (munmap(p + page_size, page_size) < 0)
		err(1, "munmap");

	printf("%s: execute #3\n", label);
	fflush(stdout);
	((void (*)(void))p)();

	return (0);
}

static void
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
	u_int64_t stack[TESTSZ/8];	/* assuming the testfly() will fit */
	struct sigaction sa;
	int (*func)(void *, size_t);
	size_t size;
	char *ep;
	void *p, *ptr;
	int ch;

	if ((page_size = sysconf(_SC_PAGESIZE)) < 0)
		err(1, "sysconf");

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	p = NULL;
	func = &noexec;
	size = TESTSZ;
	while ((ch = getopt(argc, argv, "TDBHSmps:")) != -1) {
		if (p == NULL) {
			switch (ch) {
			case 'T':
				p = &testfly;
				(void) strlcat(label, "text", sizeof(label));
				continue;
			case 'D':
				p = &data[(PAD + page_size) / 8];
				p = (void *)((long)p & ~(page_size - 1));
				(void) strlcat(label, "data", sizeof(label));
				continue;
			case 'B':
				p = &bss[(PAD + page_size) / 8];
				p = (void *)((long)p & ~(page_size - 1));
				(void) strlcat(label, "bss", sizeof(label));
				continue;
			case 'H':
				p = malloc(size + 2 * page_size);
				if (p == NULL)
					err(2, "malloc");
				p += page_size;
				p = (void *)((long)p & ~(page_size - 1));
				(void) strlcat(label, "heap", sizeof(label));
				continue;
			case 'S':
				p = getaddr(&stack);
				(void) strlcat(label, "stack", sizeof(label));
				continue;
			case 's':	/* only valid for heap and size */
				size = strtoul(optarg, &ep, 0);
				if (size > ULONG_MAX)
					errno = ERANGE;
				if (errno)
					err(1, "invalid size: %s", optarg);
				if (*ep)
					errx(1, "invalid size: %s", optarg);
				continue;
			}
		}
		switch (ch) {
		case 'm':
			if (p) {
				if ((ptr = mmap(p, size + 2 * page_size,
				    PROT_READ|PROT_WRITE,
				    MAP_ANON|MAP_FIXED, -1, 0)) == MAP_FAILED)
					err(1, "mmap");
				(void) strlcat(label, "-mmap", sizeof(label));
			} else {
				if ((ptr = mmap(p, size + 2 * page_size,
				    PROT_READ|PROT_WRITE|PROT_EXEC,
				    MAP_ANON, -1, 0)) == MAP_FAILED)
					err(1, "mmap");
				func = &noexec_mmap;
				(void) strlcat(label, "mmap", sizeof(label));
			}
			p = ptr;
			break;
		case 'p':
			func = &noexec_mprotect;
			(void) strlcat(label, "-mprotect", sizeof(label));
			break;
		default:
			usage();
		}
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

	if (p != &testfly) {
		memcpy(p, &testfly, TESTSZ);
		fdcache(p, size);
	}

	exit((*func)(p, size));
}

__asm (".space 8192; .globl  testfly; .type   testfly, @function; testfly: ret ;.space 8192");
