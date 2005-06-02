/*	$OpenBSD: script.c,v 1.1 2005/06/02 20:09:39 tholo Exp $	*/
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
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
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "pathnames.h"
#include "acpi.h"

void
run_script(const char *script)
{
	char path[MAXPATHLEN];
	int status;
	pid_t pid;

	strlcpy(path, _PATH_ETC_ACPI, sizeof(path));
	strlcat(path, "/", sizeof(path));
	strlcat(path, script, sizeof(path));

	if (access(path, X_OK)) {
		strlcpy(path, _PATH_ETC_ACPI, sizeof(path));
		strlcat(path, "/default", sizeof(path));

		if (access(path, X_OK))
			return;
	}

	switch (pid = fork()) {
	case -1:
		return;
	case 0:
		execl(path, script, (char *)NULL);
		break;
	default:
		wait4(pid, &status, 0, NULL);
		break;
	}
}
