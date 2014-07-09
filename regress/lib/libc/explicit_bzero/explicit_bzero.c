/*	$OpenBSD: explicit_bzero.c,v 1.2 2014/07/09 14:26:59 bcook Exp $	*/
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

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_NE(a, b) assert((a) != (b))
#define ASSERT_GE(a, b) assert((a) >= (b))

static void
call_on_stack(void (*fn)(int), void *stack, size_t stacklen)
{
	/*
	 * This is a bit more complicated than strictly necessary, but
	 * it ensures we don't have any flaky test failures due to
	 * inherited signal masks/actions/etc.
	 *
	 * On systems where SA_ONSTACK is not supported, this could
	 * alternatively be implemented using makecontext() or
	 * pthread_attr_setstack().
	 */

	const struct sigaction sigact = {
		.sa_handler = fn,
		.sa_flags = SA_ONSTACK,
	};
	const stack_t sigstk = {
		.ss_sp = stack,
		.ss_size = stacklen,
	};
	struct sigaction oldsigact;
	stack_t oldsigstk;
	sigset_t sigset, oldsigset;

	/* First, block all signals. */
	ASSERT_EQ(0, sigemptyset(&sigset));
	ASSERT_EQ(0, sigfillset(&sigset));
	ASSERT_EQ(0, sigprocmask(SIG_BLOCK, &sigset, &oldsigset));

	/* Next setup the signal stack and handler for SIGUSR1. */
	ASSERT_EQ(0, sigaltstack(&sigstk, &oldsigstk));
	ASSERT_EQ(0, sigaction(SIGUSR1, &sigact, &oldsigact));

	/* Raise SIGUSR1 and momentarily unblock it to run the handler. */
	ASSERT_EQ(0, raise(SIGUSR1));
	ASSERT_EQ(0, sigdelset(&sigset, SIGUSR1));
	ASSERT_EQ(-1, sigsuspend(&sigset));
	ASSERT_EQ(EINTR, errno);

	/* Restore the original signal action, stack, and mask. */
	ASSERT_EQ(0, sigaction(SIGUSR1, &oldsigact, NULL));
	if (oldsigstk.ss_flags & SA_ONSTACK)
		ASSERT_EQ(0, sigaltstack(&oldsigstk, NULL));
	ASSERT_EQ(0, sigprocmask(SIG_SETMASK, &oldsigset, NULL));
}

/* 128 bits of random data. */
static const char secret[16] = {
	0xa0, 0x6c, 0x0c, 0x81, 0xba, 0xd8, 0x5b, 0x0c,
	0xb0, 0xd6, 0xd4, 0xe3, 0xeb, 0x52, 0x5f, 0x96,
};

enum {
	SECRETCOUNT = 16,
	SECRETBYTES = SECRETCOUNT * sizeof(secret)
};

static void
populate_secret(char *buf, size_t len)
{
	int i, fds[2];
	ASSERT_EQ(0, pipe(fds));

	for (i = 0; i < SECRETCOUNT; i++)
		ASSERT_EQ(sizeof(secret), write(fds[1], secret, sizeof(secret)));
	ASSERT_EQ(0, close(fds[1]));

	ASSERT_EQ(len, read(fds[0], buf, len));
	ASSERT_EQ(0, close(fds[0]));
}

static void
test_without_bzero(int signo)
{
	char buf[SECRETBYTES];
	populate_secret(buf, sizeof(buf));
}

static void
test_with_bzero(int signo)
{
	char buf[SECRETBYTES];
	populate_secret(buf, sizeof(buf));
	explicit_bzero(buf, sizeof(buf));
}

static char altstack[SIGSTKSZ];

int
main()
{
	/*
	 * First, test that if we *don't* call explicit_bzero, that we
	 * *are* able to find the secret data on the stack.  This
	 * sanity checks that call_on_stack() and populare_secret()
	 * work as intended.
	 */
	memset(altstack, 0, sizeof(altstack));
	call_on_stack(test_without_bzero, altstack, sizeof(altstack));
	ASSERT_NE(NULL, memmem(altstack, sizeof(altstack), secret, sizeof(secret)));

	/*
	 * Now test with a call to explicit_bzero() and check that we
	 * *don't* find the secret data.
	 */
	memset(altstack, 0, sizeof(altstack));
	call_on_stack(test_with_bzero, altstack, sizeof(altstack));
	ASSERT_EQ(NULL, memmem(altstack, sizeof(altstack), secret, sizeof(secret)));

	return (0);
}
