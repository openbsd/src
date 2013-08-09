/*
 * Copyright (c) Sylvestre Gallon <ccna.syl@gmail.com>
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

#include <string.h>
#include <fuse_opt.h>

const struct fuse_opt nullopts[] = {
	FUSE_OPT_END
};

const int nullresults[] = {
	0, 0, 0, 0, 0, 0
};

const struct fuse_opt badopts[] = {
	FUSE_OPT_KEY("-p ", 0),
	FUSE_OPT_KEY("-C", 1),
	FUSE_OPT_KEY("-V", 3),
	FUSE_OPT_KEY("--version", 3),
	FUSE_OPT_KEY("-h", 2),
	FUSE_OPT_END
};

static int
match_opts(const struct fuse_opt *opts, const int *results)
{
	if (fuse_opt_match(opts, NULL) != 0)
		return (1);
	if (fuse_opt_match(opts, "") != 0)
		return (1);

	if (fuse_opt_match(opts, "bar=") != results[0])
		return (1);
	if (fuse_opt_match(opts, "--foo=") != results[1])
		return (1);
	if (fuse_opt_match(opts, "bar=%s") != results[2])
		return (1);
	if (fuse_opt_match(opts, "--foo=%lu") != results[3])
		return (1);
	if (fuse_opt_match(opts, "-x ") != results[4])
		return (1);
	if (fuse_opt_match(opts, "-x %s") != results[5])
		return (1);

	return (0);
}

int
main(int ac, char **av)
{
	if (match_opts(nullopts, nullresults) != 0)
		return (1);
	if (match_opts(badopts, nullresults) != 0)
		return (1);
	return (0);
}

