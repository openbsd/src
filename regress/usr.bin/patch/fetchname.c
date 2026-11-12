/*	$OpenBSD: fetchname.c,v 1.5 2026/11/12 14:39:28 op Exp $	*/

/*
 * Copyright (c) 2026 Kirill A. Korinsky <kirill@korins.ky>
 * Copyright (c) 2026 Omar Polo <op@openbsd.org>
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


#include <sys/stat.h>

#include <err.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define strnEQ(s1,s2,l) (!strncmp(s1, s2, l))
#define _PATH_DEVNULL "/dev/null"

int out_of_mem;

static char *
savestr(const char *s)
{
	char *t;

	if ((t = strdup(s)) == NULL)
		err(1, "strdup");
	return t;
}

#include "util.c"

struct example {
	const char *input;
	int strip;
	const char *expected;
};

static const struct example examples[] = {
	{ "dir/file.c", 0, "dir/file.c" },
	{ "dir/my file.c\n", 0, "dir/my" },
	{ "dir/my file.c\tDATE\n", 0, "dir/my file.c" },
	{ "dir/a\nb", 0, "dir/a" },
	{ "dir/a\nb\tDATE", 0, "dir/a\nb" },
	{ "dir/a\rb", 0, "dir/a" },
	{ "dir/a\rb\tDATE", 0, "dir/a\rb" },
	{ "dir/a\vb", 0, "dir/a" },
	{ "dir/a\vb\tDATE", 0, "dir/a\vb" },
	{ "dir/a\fb", 0, "dir/a" },
	{ "dir/a\fb\tDATE", 0, "dir/a\fb" },
	{ "dir/a\tb.c", 0, "dir/a" },
	{ "dir/a\tb\tDATE", 0, "dir/a" },
	{ "dir/file.c\r\n", 0, "dir/file.c" },
	{ "dir/file.c \tDATE\n", 0, "dir/file.c " },
	{ " \t\n\r\v\fdir/file.c", 0, "dir/file.c" },
	{ "\tdir/my file.c\n", 0, "dir/my" },
	{ " \t\n\r\v\f", 0, "" },
	{ "", 0, NULL },
	{ NULL, 0, NULL },
	{ "/dev/null\tDATE\n", 0, NULL },
	{ "/dev/null-extra", 0, NULL },
	{ " \t/dev/null\tDATE", 0, NULL },
	{ "a/b/c", 0, "a/b/c" },
	{ "a/b/c", 1, "b/c" },
	{ "a/b/c", 2, "c" },
	{ "a/b/c", 9, "c" },
	{ "file.c", 9, "file.c" },
	{ "a///b/c", 1, "b/c" },
	{ "a///b/c", 0, "a///b/c" },
	{ "/a/b", 1, "a/b" },
	{ "/a/b", 0, "/a/b" },
	{ "../a/b", 0, "../a/b" },
	{ "../a/b", 1, "a/b" },
	{ "a/", 1, "a/" },
	{ "a/\tDATE", 1, "a/" },
	{ "a/ DATE", 1, "a/" },
	{ "x/dev/null", 1, "dev/null" },
	{ ".", 0, "." },
	{ "./test.c", 123, "test.c" },
	{ "test.c/child.c", 123, "child.c" },
	{ "/a/b", 123, "b" },
};

int
main(void)
{
	const struct example *example;
	char *actual;
	size_t i, count, failures = 0;
	bool exists, passed;

	count = sizeof(examples) / sizeof(examples[0]);
	for (i = 0; i < count; i++) {
		example = &examples[i];
		exists = true;
		actual = fetchname(example->input, &exists, example->strip);
		if (example->expected == NULL)
			passed = actual == NULL && exists;
		else
			passed = actual != NULL &&
			    strcmp(actual, example->expected) == 0;
		if (!passed) {
			printf("%zu, strip=%d '%s' => wanted='%s' got='%s'\n",
			    i, example->strip, example->input,
			    example->expected, actual);
			failures++;
		}

		free(actual);
	}
	return failures;
}
