/*	$OpenBSD: rmatch_test.c,v 1.2 2024/02/20 10:37:35 claudio Exp $	*/

/*
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
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


#include <stdio.h>
#include <string.h>

int	rmatch(const char *, const char *, int);

struct tv {
	const char *pattern;
	const char *string;
	int result;
	const char *reason;
} tvs[] = {
	{ "*abc", "aaaabc", 0 },
	{ "abc", "abc", 0 },
	{ "*aaa", "bbbcaa", 1 },
	{ "aaa", "aabb", 1 },
	{ "[a]??", "aaa", 0 },
	{ "*[a]??", "abcabc", 0 },
	{ "[b]??", "abcabc", 1 },
	{ "*[b]??*", "abcabc", 0 },
	{ "[a-z]", "a", 0 },
	{ "*[a-z]", "1234a", 0 },
	{ "[a-z]", "A", 1 },
	{ "*[a-z]", "1234A", 1 },
	{ "[[:lower:]]", "a", 0 },
	{ "*[[:lower:]]", "1234a", 0 },
	{ "[[:lower:]]", "A", 1 },
	{ "*[[:lower:]]", "1234A", 1 },
	{ "?", "a", 0 },
	{ "*?", "1234a", 0 },
	{ "\\a", "a", 0 },
	{ "*\\a", "1234a", 0 },
	{ "/", "/", 0 },
	{ "*/", "1234/", 0 },
	{ "?", "/", 1 },
	{ "*?", "1234/", 1 },
	{ "\\/", "/", 0 },
	{ "*\\/", "1234/", 0 },
	{ "*abc/", "abc/", 0 },
	{ "*abc/", "ababc/", 0 },
	{ "*abc/*", "abc/xyz", 0 },
	{ "*abc/", "xyz/abc", 1 },
	{ "*abc/", "ab/", 1 },
	{ "*abc/", "abc", 1 },
	{ "*abc/", "abc.", 1 },
	{ "*abc/", "abcab/", 1 },
	{ "*abc/", "abcd/abc", 1 },
	{ "**??""?/", "a/aa/aaa/", 0 },
	{ "**??""?/", "a/aa/aa/", 1 },
	{ "**??""?/", "a/aa/aaaxxxx/", 0 },
	{ "**abc/", "a/aa/xxabc/", 0 },

	/* rysnc wildtest.txt */

	{ "foo", "foo", 0 },
	{ "bar", "foo", 1 },
	{ "", "", 0 },
	{ "???", "foo", 0 },
	{ "??", "foo", 1 },
	{ "*", "foo", 0 },
	{ "f*", "foo", 0 },
	{ "*f", "foo", 1 },
	{ "*foo*", "foo", 0 },
	{ "*ob*a*r*", "foobar", 0 },
	{ "*ab", "aaaaaaabababab", 0 },
	{ "foo\\*", "foo*", 0 },
	{ "foo\\*bar", "foobar", 1 },
	{ "f\\\\oo", "f\\oo", 0 },
	{ "*[al]?", "ball", 0 },
	{ "[ten]", "ten", 1 },
	{ "**[!te]", "ten", 0 },
	{ "**[!ten]", "ten", 1 },
	{ "t[a-g]n", "ten", 0 },
	{ "t[!a-g]n", "ten", 1 },
	{ "t[!a-g]n", "ton", 0 },
	{ "t[^a-g]n", "ton", 0 },
	{ "a[]]b", "a]b", 0 },
	{ "a[]-]b", "a-b", 0 },
	{ "a[]-]b", "a]b", 0 },
	{ "a[]-]b", "aab", 1 },
	{ "a[]a-]b", "aab", 0 },
	{ "]", "]", 0 },
	{ "foo*bar", "foo/baz/bar", 1 },
	{ "foo**bar", "foo/baz/bar", 0 },
	{ "foo?bar", "foo/bar", 1 },
	{ "foo[/]bar", "foo/bar", 1 },
	{ "f[^eiu][^eiu][^eiu][^eiu][^eiu]r", "foo/bar", 1 },
	{ "f[^eiu][^eiu][^eiu][^eiu][^eiu]r", "foo-bar", 0 },
	{ "**/foo", "foo", 1 },
	{ "**/foo", "/foo", 0 },
	{ "**/foo", "bar/baz/foo", 0 },
	{ "*/foo", "bar/baz/foo", 1 },
	{ "**/bar*", "foo/bar/baz", 1 },
	{ "**/bar/*", "deep/foo/bar/baz", 0 },
	{ "**/bar/*", "deep/foo/bar/baz/", 1 },
	{ "**/bar/**", "deep/foo/bar/baz/", 0 },
	{ "**/bar/*", "deep/foo/bar", 1 },
	{ "**/bar/**", "deep/foo/bar/", 0 },
	{ "**/bar**", "foo/bar/baz", 0 },
	{ "*/bar/**", "foo/bar/baz/x", 0 },
	{ "*/bar/**", "deep/foo/bar/baz/x", 1 },
	{ "**/bar/*/*", "deep/foo/bar/baz/x", 0 },
	{ "a[c-c]st", "acrt", 1 },
	{ "a[c-c]rt", "acrt", 0 },
	{ "[!]-]", "]", 1 },
	{ "[!]-]", "a", 0 },
	{ "\\", "", 1 },
	{ "\\", "\\", 1, "backslash at end is taken literally" },
	{ "*/\\", "/\\", 1, "backslash at end is taken literally" },
	{ "*/\\\\", "/\\", 0 },
	{ "foo", "foo", 0 },
	{ "@foo", "@foo", 0 },
	{ "@foo", "foo", 1 },
	{ "\\[ab]", "[ab]", 0 },
	{ "[[]ab]", "[ab]", 0 },
	{ "[[:]ab]", "[ab]", 0 },
	{ "[[::]ab]", "[ab]", 1, "bad char class taken literally" },
	{ "[[:digit]ab]", "[ab]", 0 },
	{ "[\\[:]ab]", "[ab]", 0 },
	{ "\\??\\?b", "?a?b", 0 },
	{ "\\a\\b\\c", "abc", 0 },
	{ "", "foo", 1 },
	{ "**/t[o]", "foo/bar/baz/to", 0 },
	{ "[[:alpha:]][[:digit:]][[:upper:]]", "a1B", 0 },
	{ "[[:digit:][:upper:][:space:]]", "a", 1 },
	{ "[[:digit:][:upper:][:space:]]", "A", 0 },
	{ "[[:digit:][:upper:][:space:]]", "1", 0 },
	{ "[[:digit:][:upper:][:spaci:]]", "1", 1 },
	{ "[[:digit:][:upper:][:space:]]", " ", 0 },
	{ "[[:digit:][:upper:][:space:]]", ".", 1 },
	{ "[[:digit:][:punct:][:space:]]", ".", 0 },
	{ "[[:xdigit:]]", "5", 0 },
	{ "[[:xdigit:]]", "f", 0 },
	{ "[[:xdigit:]]", "D", 0 },
	{ "[[:alnum:][:alpha:][:blank:][:cntrl:][:digit:][:graph:][:lower:][:print:][:punct:][:space:][:upper:][:xdigit:]]", "_", 0 },
	{ "[^[:alnum:][:alpha:][:blank:][:digit:][:graph:][:lower:][:print:][:punct:][:space:][:upper:][:xdigit:]]", "", 0 },
	{ "[^[:alnum:][:alpha:][:blank:][:cntrl:][:digit:][:lower:][:space:][:upper:][:xdigit:]]", ".", 0 },
	{ "[a-c[:digit:]x-z]", "5", 0 },
	{ "[a-c[:digit:]x-z]", "b", 0 },
	{ "[a-c[:digit:]x-z]", "y", 0 },
	{ "[a-c[:digit:]x-z]", "q", 1 },
	{ "[\\\\-^]", "]", 0 },
	{ "[\\\\-^]", "[", 1 },
	{ "[\\-_]", "-", 0 },
	{ "[\\]]", "]", 0 },
	{ "[\\]]", "\\]", 1 },
	{ "[\\]]", "\\", 1 },
	{ "a[]b", "ab", 1 },
	{ "a[]b", "a[]b", 1, "empty [] is taken literally" },
	{ "ab[", "ab[", 1, "single [ char taken literally" },
	{ "[!", "ab", 1 },
	{ "[-", "ab", 1 },
	{ "[-]", "-", 0 },
	{ "[a-", "-", 1 },
	{ "[!a-", "-", 1 },
	{ "[--A]", "-", 0 },
	{ "[--A]", "5", 0 },
	{ "[ --]", " ", 0 },
	{ "[ --]", "$", 0 },
	{ "[ --]", "-", 0 },
	{ "[ --]", "0", 1 },
	{ "[---]", "-", 0 },
	{ "[------]", "-", 0 },
	{ "[a-e-n]", "j", 1 },
	{ "[a-e-n]", "-", 0 },
	{ "[!------]", "a", 0 },
	{ "[]-a]", "[", 1 },
	{ "[]-a]", "^", 0 },
	{ "[!]-a]", "^", 1 },
	{ "[!]-a]", "[", 0 },
	{ "[a^bc]", "^", 0 },
	{ "[a-]b]", "-b]", 0 },
	{ "[\\]", "\\", 1 },
	{ "[\\\\]", "\\", 0 },
	{ "[!\\\\]", "\\", 1 },
	{ "[A-\\\\]", "G", 0 },
	{ "b*a", "aaabbb", 1 },
	{ "*ba*", "aabcaa", 1 },
	{ "[,]", ",", 0 },
	{ "[\\\\,]", ",", 0 },
	{ "[\\\\,]", "\\", 0 },
	{ "[,-.]", "-", 0 },
	{ "[,-.]", "+", 1 },
	{ "[,-.]", "-.]", 1 },
	{ "[\\1-\\3]", "2", 0 },
	{ "[\\1-\\3]", "3", 0 },
	{ "[\\1-\\3]", "4", 1 },
	{ "[[-\\]]", "\\", 0 },
	{ "[[-\\]]", "[", 0 },
	{ "[[-\\]]", "]", 0 },
	{ "[[-\\]]", "-", 1 },
	{ "-*-*-*-*-*-*-12-*-*-*-m-*-*-*", "-adobe-courier-bold-o-normal--12-120-75-75-m-70-iso8859-1", 0 },
	{ "-*-*-*-*-*-*-12-*-*-*-m-*-*-*", "-adobe-courier-bold-o-normal--12-120-75-75-X-70-iso8859-1", 1 },
	{ "-*-*-*-*-*-*-12-*-*-*-m-*-*-*", "-adobe-courier-bold-o-normal--12-120-75-75-/-70-iso8859-1", 1 },
	{ "/*/*/*/*/*/*/12/*/*/*/m/*/*/*", "/adobe/courier/bold/o/normal//12/120/75/75/m/70/iso8859/1", 0 },
	{ "/*/*/*/*/*/*/12/*/*/*/m/*/*/*", "/adobe/courier/bold/o/normal//12/120/75/75/X/70/iso8859/1", 1 },
	{ "**/*a*b*g*n*t", "abcd/abcdefg/abcdefghijk/abcdefghijklmnop.txt", 0 },
	{ "**/*a*b*g*n*t", "abcd/abcdefg/abcdefghijk/abcdefghijklmnop.txtz", 1 },

	/* infinte loop test from Kyle Evans */
	{ "dir/*.o", "dir1/a.o", 1 },

	{ NULL },
};

int
main (int argc, char **argv)
{
	const char *p, *s;
	size_t i;
	int rv = 0;

	for (i = 0; tvs[i].pattern != NULL; i++) {
		p = tvs[i].pattern;
		s = tvs[i].string;
		if (rmatch(p, s, 0) == tvs[i].result) {
			printf("string %s pattern %s does %s\n",
			    tvs[i].string, tvs[i].pattern,
			    tvs[i].result ? "not match" : "match");
		} else if (tvs[i].reason) {
			printf("string %s pattern %s SHOULD %s but %s\n",
			    tvs[i].string, tvs[i].pattern,
			    tvs[i].result ? "not match" : "match",
			    tvs[i].reason);
		} else {
			printf("FAILED: string %s pattern %s failed to %s\n",
			    tvs[i].string, tvs[i].pattern,
			    tvs[i].result ? "not match" : "match");
			rv = 1;
		}
	}

	return rv;
}
