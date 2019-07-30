/*	$Id: fileproc.c,v 1.14 2017/01/24 13:32:55 jsing Exp $ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static int
serialise(const char *tmp, const char *real,
    const char *v, size_t vsz, const char *v2, size_t v2sz)
{
	int	 fd;

	/*
	 * Write into backup location, overwriting.
	 * Then atomically (?) do the rename.
	 */

	fd = open(tmp, O_WRONLY|O_CREAT|O_TRUNC, 0444);
	if (fd == -1) {
		warn("%s", tmp);
		return 0;
	} else if ((ssize_t)vsz != write(fd, v, vsz)) {
		warnx("%s", tmp);
		close(fd);
		return 0;
	} else if (v2 != NULL && write(fd, v2, v2sz) != (ssize_t)v2sz) {
		warnx("%s", tmp);
		close(fd);
		return 0;
	} else if (close(fd) == -1) {
		warn("%s", tmp);
		return 0;
	} else if (rename(tmp, real) == -1) {
		warn("%s", real);
		return 0;
	}

	return 1;
}

int
fileproc(int certsock, const char *certdir, const char *certfile, const char
    *chainfile, const char *fullchainfile)
{
	char		*csr = NULL, *ch = NULL;
	char		*certfile_bak = NULL, *chainfile_bak = NULL;
	char		*fullchainfile_bak = NULL;
	size_t		 chsz, csz;
	int		 rc = 0;
	long		 lval;
	enum fileop	 op;

	/* File-system and sandbox jailing. */

	if (chroot(certdir) == -1) {
		warn("chroot");
		goto out;
	}
	if (chdir("/") == -1) {
		warn("chdir");
		goto out;
	}

	/*
	 * rpath and cpath for rename, wpath and cpath for
	 * writing to the temporary.
	 */
	if (pledge("stdio cpath wpath rpath", NULL) == -1) {
		warn("pledge");
		goto out;
	}

	/* Read our operation. */

	op = FILE__MAX;
	if ((lval = readop(certsock, COMM_CHAIN_OP)) == 0)
		op = FILE_STOP;
	else if (lval == FILE_CREATE || lval == FILE_REMOVE)
		op = lval;

	if (FILE_STOP == op) {
		rc = 1;
		goto out;
	} else if (FILE__MAX == op) {
		warnx("unknown operation from certproc");
		goto out;
	}

	/*
	 * If revoking certificates, just unlink the files.
	 * We return the special error code of 2 to indicate that the
	 * certificates were removed.
	 */

	if (FILE_REMOVE == op) {
		if (certfile) {
			if (unlink(certfile) == -1 && errno != ENOENT) {
				warn("%s/%s", certdir, certfile);
				goto out;
			} else
				dodbg("%s/%s: unlinked", certdir, certfile);
		}

		if (chainfile) {
			if (unlink(chainfile) == -1 && errno != ENOENT) {
				warn("%s/%s", certdir, chainfile);
				goto out;
			} else
				dodbg("%s/%s: unlinked", certdir, chainfile);
		}

		if (fullchainfile) {
			if (unlink(fullchainfile) == -1 && errno != ENOENT) {
				warn("%s/%s", certdir, fullchainfile);
				goto out;
			} else
				dodbg("%s/%s: unlinked", certdir,
				    fullchainfile);
		}

		rc = 2;
		goto out;
	}

	/*
	 * Start by downloading the chain PEM as a buffer.
	 * This is not NUL-terminated, but we're just going to guess
	 * that it's well-formed and not actually touch the data.
	 * Once downloaded, dump it into CHAIN_BAK.
	 */

	if (certfile)
		if (asprintf(&certfile_bak, "%s~", certfile) == -1) {
			warn("asprintf");
			goto out;
		}

	if (chainfile)
		if (asprintf(&chainfile_bak, "%s~", chainfile) == -1) {
			warn("asprintf");
			goto out;
		}

	if (fullchainfile)
		if (asprintf(&fullchainfile_bak, "%s~", fullchainfile) == -1) {
			warn("asprintf");
			goto out;
		}

	if ((ch = readbuf(certsock, COMM_CHAIN, &chsz)) == NULL)
		goto out;

	if (chainfile) {
		if (!serialise(chainfile_bak, chainfile, ch, chsz, NULL, 0))
			goto out;

		dodbg("%s/%s: created", certdir, chainfile);
	}

	/*
	 * Next, wait until we receive the DER encoded (signed)
	 * certificate from the network process.
	 * This comes as a stream of bytes: we don't know how many, so
	 * just keep downloading.
	 */

	if ((csr = readbuf(certsock, COMM_CSR, &csz)) == NULL)
		goto out;

	if (certfile) {
		if (!serialise(certfile_bak, certfile, csr, csz, NULL, 0))
			goto out;

		dodbg("%s/%s: created", certdir, certfile);
	}

	/*
	 * Finally, create the full-chain file.
	 * This is just the concatenation of the certificate and chain.
	 * We return the special error code 2 to indicate that the
	 * on-file certificates were changed.
	 */
	if (fullchainfile) {
		if (!serialise(fullchainfile_bak, fullchainfile, csr, csz, ch,
		    chsz))
			goto out;

		dodbg("%s/%s: created", certdir, fullchainfile);
	}

	rc = 2;
out:
	close(certsock);
	free(csr);
	free(ch);
	free(certfile_bak);
	free(chainfile_bak);
	free(fullchainfile_bak);
	return rc;
}
