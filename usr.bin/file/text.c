/* $OpenBSD: text.c,v 1.1 2015/04/24 16:24:11 nicm Exp $ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <ctype.h>
#include <string.h>

#include "file.h"
#include "magic.h"
#include "xmalloc.h"

static const char *text_words[][3] = {
	{ "msgid", "PO (gettext message catalogue)", "text/x-po" },
	{ "dnl", "M4 macro language pre-processor", "text/x-m4" },
	{ "import", "Java program", "text/x-java" },
	{ "\"libhdr\"", "BCPL program", "text/x-bcpl" },
	{ "\"LIBHDR\"", "BCPL program", "text/x-bcpl" },
	{ "//", "C++ program", "text/x-c++" },
	{ "virtual", "C++ program", "text/x-c++" },
	{ "class", "C++ program", "text/x-c++" },
	{ "public:", "C++ program", "text/x-c++" },
	{ "private:", "C++ program", "text/x-c++" },
	{ "/*", "C program", "text/x-c" },
	{ "#include", "C program", "text/x-c" },
	{ "char", "C program", "text/x-c" },
	{ "The", "English", "text/plain" },
	{ "the", "English", "text/plain" },
	{ "double", "C program", "text/x-c" },
	{ "extern", "C program", "text/x-c" },
	{ "float", "C program", "text/x-c" },
	{ "struct", "C program", "text/x-c" },
	{ "union", "C program", "text/x-c" },
	{ "CFLAGS", "make commands", "text/x-makefile" },
	{ "LDFLAGS", "make commands", "text/x-makefile" },
	{ "all:", "make commands", "text/x-makefile" },
	{ ".PRECIOUS", "make commands", "text/x-makefile" },
	{ ".ascii", "assembler program", "text/x-asm" },
	{ ".asciiz", "assembler program", "text/x-asm" },
	{ ".byte", "assembler program", "text/x-asm" },
	{ ".even", "assembler program", "text/x-asm" },
	{ ".globl", "assembler program", "text/x-asm" },
	{ ".text", "assembler program", "text/x-asm" },
	{ "clr", "assembler program", "text/x-asm" },
	{ "(input", "Pascal program", "text/x-pascal" },
	{ "program", "Pascal program", "text/x-pascal" },
	{ "record", "Pascal program", "text/x-pascal" },
	{ "dcl", "PL/1 program", "text/x-pl1" },
	{ "Received:", "mail", "text/x-mail" },
	{ ">From", "mail", "text/x-mail" },
	{ "Return-Path:", "mail", "text/x-mail" },
	{ "Cc:", "mail", "text/x-mail" },
	{ "Newsgroups:", "news", "text/x-news" },
	{ "Path:", "news", "text/x-news" },
	{ "Organization:", "news", "text/x-news" },
	{ "href=", "HTML document", "text/html" },
	{ "HREF=", "HTML document", "text/html" },
	{ "<body", "HTML document", "text/html" },
	{ "<BODY", "HTML document", "text/html" },
	{ "<html", "HTML document", "text/html" },
	{ "<HTML", "HTML document", "text/html" },
	{ "<!--", "HTML document", "text/html" },
	{ NULL, NULL, NULL }
};

static int
text_is_ascii(u_char c)
{
	const char	cc[] = "\007\010\011\012\014\015\033";

	if (c == '\0')
		return (0);
	if (strchr(cc, c) != NULL)
		return (1);
	return (c > 31 && c < 127);
}

static int
text_is_latin1(u_char c)
{
	if (c >= 160)
		return (1);
	return (text_is_ascii(c));
}

static int
text_is_extended(u_char c)
{
	if (c >= 128)
		return (1);
	return (text_is_ascii(c));
}

static int
text_try_test(const void *base, size_t size, int (*f)(u_char))
{
	const u_char	*data = base;
	size_t		 offset;

	for (offset = 0; offset < size; offset++) {
		if (!f(data[offset]))
			return (0);
	}
	return (1);
}

const char *
text_get_type(const void *base, size_t size)
{
	if (text_try_test(base, size, text_is_ascii))
		return ("ASCII");
	if (text_try_test(base, size, text_is_latin1))
		return ("ISO-8859");
	if (text_try_test(base, size, text_is_extended))
		return ("Non-ISO extended-ASCII");
	return (NULL);
}

const char *
text_try_words(const void *base, size_t size, int flags)
{
	const char	*cp, *end, *next, *word;
	size_t		 wordlen;
	u_int		 i;

	end = (char*)base + size;
	for (cp = base; cp != end; /* nothing */) {
		while (cp != end && isspace((u_char)*cp))
			cp++;

		next = cp;
		while (next != end && !isspace((u_char)*next))
			next++;

		for (i = 0; /* nothing */; i++) {
			word = text_words[i][0];
			if (word == NULL)
				break;
			wordlen = strlen(word);

			if ((size_t)(next - cp) != wordlen)
				continue;
			if (memcmp(cp, word, wordlen) != 0)
				continue;
			if (flags & MAGIC_TEST_MIME)
				return (text_words[i][2]);
			return (text_words[i][1]);
		}

		cp = next;
	}
	return (NULL);
}
