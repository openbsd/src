$*	$OpenBSD: disknames.c,v 1.3 2007/02/14 23:02:03 krw Exp $	*/

/*
 * Copyright (c) 2007 Kenneth R. Westerback <krw@openbsd.org>
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
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <unistd.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	struct iovec iov[2];
	int mib[2];
	size_t len;
	char *p;

	mib[0] = CTL_HW;
	mib[1] = HW_DISKNAMES;

	if (sysctl(mib, 2, NULL, &len, NULL, 0) != -1)
		if ((p = malloc(len)) != NULL)
			if (sysctl(mib, 2, p, &len, NULL, 0) != -1) {
				iov[0].iov_base = p;
				iov[0].iov_len = len;
				iov[1].iov_base = "\n";
				iov[1].iov_len = 1;
				writev(STDOUT_FILENO, iov, 2);
				exit(0);
			}
	exit(1);
}	
