#if TEST___PROGNAME
int
main(void)
{
	extern char *__progname;

	return !__progname;
}
#endif /* TEST___PROGNAME */
#if TEST_ARC4RANDOM
#include <stdlib.h>

int
main(void)
{
	return (arc4random() + 1) ? 0 : 1;
}
#endif /* TEST_ARC4RANDOM */
#if TEST_B64_NTOP
#include <netinet/in.h>
#include <resolv.h>

int
main(void)
{
	const char *src = "hello world";
	char output[1024];

	return b64_ntop((const unsigned char *)src, 11, output, sizeof(output)) > 0 ? 0 : 1;
}
#endif /* TEST_B64_NTOP */
#if TEST_CAPSICUM
#include <sys/capsicum.h>

int
main(void)
{
	cap_enter();
	return(0);
}
#endif /* TEST_CAPSICUM */
#if TEST_ERR
/*
 * Copyright (c) 2015 Ingo Schwarze <schwarze@openbsd.org>
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

#include <err.h>

int
main(void)
{
	warnx("%d. warnx", 1);
	warn("%d. warn", 2);
	err(0, "%d. err", 3);
	/* NOTREACHED */
	return 1;
}
#endif /* TEST_ERR */
#if TEST_EXPLICIT_BZERO
#include <string.h>

int
main(void)
{
	char foo[10];

	explicit_bzero(foo, sizeof(foo));
	return(0);
}
#endif /* TEST_EXPLICIT_BZERO */
#if TEST_GETPROGNAME
#include <stdlib.h>

int
main(void)
{
	const char * progname;

	progname = getprogname();
	return progname == NULL;
}
#endif /* TEST_GETPROGNAME */
#if TEST_INFTIM
/*
 * Linux doesn't (always?) have this.
 */

#include <poll.h>
#include <stdio.h>

int
main(void)
{
	printf("INFTIM is defined to be %ld\n", (long)INFTIM);
	return 0;
}
#endif /* TEST_INFTIM */
#if TEST_MD5
#include <sys/types.h>
#include <md5.h>

int main(void)
{
	MD5_CTX ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, "abcd", 4);

	return 0;
}
#endif /* TEST_MD5 */
#if TEST_MEMMEM
#define _GNU_SOURCE
#include <string.h>

int
main(void)
{
	char *a = memmem("hello, world", strlen("hello, world"), "world", strlen("world"));
	return(NULL == a);
}
#endif /* TEST_MEMMEM */
#if TEST_MEMRCHR
#if defined(__linux__) || defined(__MINT__)
#define _GNU_SOURCE	/* See test-*.c what needs this. */
#endif
#include <string.h>

int
main(void)
{
	const char *buf = "abcdef";
	void *res;

	res = memrchr(buf, 'a', strlen(buf));
	return(NULL == res ? 1 : 0);
}
#endif /* TEST_MEMRCHR */
#if TEST_MEMSET_S
#include <string.h>

int main(void)
{
	char buf[10];
	memset_s(buf, 0, 'c', sizeof(buf));
	return 0;
}
#endif /* TEST_MEMSET_S */
#if TEST_PATH_MAX
/*
 * POSIX allows PATH_MAX to not be defined, see
 * http://pubs.opengroup.org/onlinepubs/9699919799/functions/sysconf.html;
 * the GNU Hurd is an example of a system not having it.
 *
 * Arguably, it would be better to test sysconf(_SC_PATH_MAX),
 * but since the individual *.c files include "config.h" before
 * <limits.h>, overriding an excessive value of PATH_MAX from
 * "config.h" is impossible anyway, so for now, the simplest
 * fix is to provide a value only on systems not having any.
 * So far, we encountered no system defining PATH_MAX to an
 * impractically large value, even though POSIX explicitly
 * allows that.
 *
 * The real fix would be to replace all static buffers of size
 * PATH_MAX by dynamically allocated buffers.  But that is
 * somewhat intrusive because it touches several files and
 * because it requires changing struct mlink in mandocdb.c.
 * So i'm postponing that for now.
 */

#include <limits.h>
#include <stdio.h>

int
main(void)
{
	printf("PATH_MAX is defined to be %ld\n", (long)PATH_MAX);
	return 0;
}
#endif /* TEST_PATH_MAX */
#if TEST_PLEDGE
#include <unistd.h>

int
main(void)
{
	return !!pledge("stdio", NULL);
}
#endif /* TEST_PLEDGE */
#if TEST_PROGRAM_INVOCATION_SHORT_NAME
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <errno.h>

int
main(void)
{

	return !program_invocation_short_name;
}
#endif /* TEST_PROGRAM_INVOCATION_SHORT_NAME */
#if TEST_REALLOCARRAY
#include <stdlib.h>

int
main(void)
{
	return !reallocarray(NULL, 2, 2);
}
#endif /* TEST_REALLOCARRAY */
#if TEST_RECALLOCARRAY
#include <stdlib.h>

int
main(void)
{
	return !recallocarray(NULL, 0, 2, 2);
}
#endif /* TEST_RECALLOCARRAY */
#if TEST_SANDBOX_INIT
#include <sandbox.h>

int
main(void)
{
	char	*ep;
	int	 rc;

	rc = sandbox_init(kSBXProfileNoInternet, SANDBOX_NAMED, &ep);
	if (-1 == rc)
		sandbox_free_error(ep);
	return(-1 == rc);
}
#endif /* TEST_SANDBOX_INIT */
#if TEST_SECCOMP_FILTER
#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <errno.h>

int
main(void)
{

	prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, 0);
	return(EFAULT == errno ? 0 : 1);
}
#endif /* TEST_SECCOMP_FILTER */
#if TEST_SOCK_NONBLOCK
/*
 * Linux doesn't (always?) have this.
 */

#include <sys/socket.h>

int
main(void)
{
	int fd[2];
	socketpair(AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK, 0, fd);
	return 0;
}
#endif /* TEST_SOCK_NONBLOCK */
#if TEST_STRLCAT
#include <string.h>

int
main(void)
{
	char buf[3] = "a";
	return ! (strlcat(buf, "b", sizeof(buf)) == 2 &&
	    buf[0] == 'a' && buf[1] == 'b' && buf[2] == '\0');
}
#endif /* TEST_STRLCAT */
#if TEST_STRLCPY
#include <string.h>

int
main(void)
{
	char buf[2] = "";
	return ! (strlcpy(buf, "a", sizeof(buf)) == 1 &&
	    buf[0] == 'a' && buf[1] == '\0');
}
#endif /* TEST_STRLCPY */
#if TEST_STRNDUP
#include <string.h>

int
main(void)
{
	const char *foo = "bar";
	char *baz;

	baz = strndup(foo, 1);
	return(0 != strcmp(baz, "b"));
}
#endif /* TEST_STRNDUP */
#if TEST_STRNLEN
#include <string.h>

int
main(void)
{
	const char *foo = "bar";
	size_t sz;

	sz = strnlen(foo, 1);
	return(1 != sz);
}
#endif /* TEST_STRNLEN */
#if TEST_STRTONUM
/*
 * Copyright (c) 2015 Ingo Schwarze <schwarze@openbsd.org>
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

#include <stdlib.h>

int
main(void)
{
	const char *errstr;

	if (strtonum("1", 0, 2, &errstr) != 1)
		return 1;
	if (errstr != NULL)
		return 2;
	if (strtonum("1x", 0, 2, &errstr) != 0)
		return 3;
	if (errstr == NULL)
		return 4;
	if (strtonum("2", 0, 1, &errstr) != 0)
		return 5;
	if (errstr == NULL)
		return 6;
	if (strtonum("0", 1, 2, &errstr) != 0)
		return 7;
	if (errstr == NULL)
		return 8;
	return 0;
}
#endif /* TEST_STRTONUM */
#if TEST_SYS_QUEUE
#include <sys/queue.h>
#include <stddef.h>

struct foo {
	int bar;
	TAILQ_ENTRY(foo) entries;
};

TAILQ_HEAD(fooq, foo);

int
main(void)
{
	struct fooq foo_q;

	TAILQ_INIT(&foo_q);
	return 0;
}
#endif /* TEST_SYS_QUEUE */
#if TEST_SYSTRACE
#include <sys/param.h>
#include <dev/systrace.h>

#include <stdlib.h>

int
main(void)
{

	return(0);
}
#endif /* TEST_SYSTRACE */
#if TEST_UNVEIL
#include <unistd.h>

int
main(void)
{
	return -1 != unveil(NULL, NULL);
}
#endif /* TEST_UNVEIL */
#if TEST_ZLIB
#include <stddef.h>
#include <zlib.h>

int
main(void)
{
	gzFile		 gz;

	if (NULL == (gz = gzopen("/dev/null", "w")))
		return(1);
	gzputs(gz, "foo");
	gzclose(gz);
	return(0);
}
#endif /* TEST_ZLIB */
