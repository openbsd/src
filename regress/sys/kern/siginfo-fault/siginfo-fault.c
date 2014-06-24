/*	$OpenBSD: siginfo-fault.c,v 1.2 2014/06/24 19:05:42 matthew Exp $	*/
/*
 * Copyright (c) 2014 Google Inc.
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

#include <sys/mman.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK_EQ(a, b) assert((a) == (b))
#define CHECK_NE(a, b) assert((a) != (b))
#define CHECK_GE(a, b) assert((a) >= (b))
#define FAIL() assert(0)

static jmp_buf env;
static volatile int gotsigno;
static volatile siginfo_t gotsi;

static void
sigsegv(int signo, siginfo_t *si, void *ctx)
{
	gotsigno = signo;
	gotsi = *si;
        siglongjmp(env, 1);
}

static void
checksig(int expsigno, int expcode, volatile char *expaddr)
{
	int fail = 0;
	if (expsigno != gotsigno) {
		printf("signo: expect %d (%s)", expsigno, strsignal(expsigno));
		printf(", actual %d (%s)\n", gotsigno, strsignal(gotsigno));
		++fail;
	}
	if (expsigno != gotsi.si_signo) {
		printf("signo: expect %d (%s)", expsigno, strsignal(expsigno));
		printf(", actual %d (%s)\n", gotsi.si_signo, strsignal(gotsi.si_signo));
		++fail;
	}
	if (expcode != gotsi.si_code) {
		printf("si_code: expect %d, actual %d\n", expcode, gotsi.si_code);
		++fail;
	}
	if (expaddr != gotsi.si_addr) {
		printf("si_addr: expect %p, actual %p\n", expaddr, gotsi.si_addr);
		++fail;
	}
	CHECK_EQ(0, fail);
}

int
main()
{
        long pagesize = sysconf(_SC_PAGESIZE);
        CHECK_NE(-1, pagesize);

        const struct sigaction sa = {
                .sa_sigaction = sigsegv,
                .sa_flags = SA_SIGINFO,
        };
        CHECK_EQ(0, sigaction(SIGSEGV, &sa, NULL));
        CHECK_EQ(0, sigaction(SIGBUS, &sa, NULL));

        volatile char *p = mmap(NULL, pagesize, PROT_READ,
	    MAP_PRIVATE|MAP_ANON, -1, 0);
        CHECK_NE(MAP_FAILED, p);

        if (sigsetjmp(env, 1) == 0) {
		p[0] = 1;
                FAIL();
        }
	checksig(SIGSEGV, SEGV_ACCERR, p);

	CHECK_EQ(0, mprotect((void *)p, pagesize, PROT_NONE));
        if (sigsetjmp(env, 1) == 0) {
		(void)p[1];
                FAIL();
        }
	checksig(SIGSEGV, SEGV_ACCERR, p + 1);

	CHECK_EQ(0, munmap((void *)p, pagesize));
        if (sigsetjmp(env, 1) == 0) {
		(void)p[2];
                FAIL();
        }
	checksig(SIGSEGV, SEGV_MAPERR, p + 2);

	char filename[] = "/tmp/siginfo-fault.XXXXXXXX";
	int fd = mkstemp(filename);
	CHECK_GE(fd, 0);
	CHECK_EQ(0, unlink(filename));
	CHECK_EQ(0, ftruncate(fd, 0));  /* just in case */
	p = mmap(NULL, pagesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	CHECK_NE(MAP_FAILED, p);
	CHECK_EQ(0, close(fd));

	if (sigsetjmp(env, 1) == 0) {
		p[3] = 1;
		FAIL();
	}
	checksig(SIGBUS, BUS_ADRERR, p + 3);

        return (0);
}
