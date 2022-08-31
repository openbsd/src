/*	$OpenBSD: json.c,v 1.3 2022/08/31 12:13:59 claudio Exp $ */

/*
 * Copyright (c) 2020 Claudio Jeker <claudio@openbsd.org>
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
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "json.h"

#define JSON_MAX_STACK	16

enum json_type {
	NONE,
	START,
	ARRAY,
	OBJECT
};

struct json_stack {
	const char	*name;
	unsigned int	count;
	enum json_type	type;
} stack[JSON_MAX_STACK];

char indent[JSON_MAX_STACK + 1];
int level;

static void
do_comma_indent(void)
{
	if (stack[level].count++ > 0)
		printf(",\n");
	printf("\t%.*s", level, indent);
}

static void
do_name(const char *name)
{
	if (stack[level].type == ARRAY)
		return;
	printf("\"%s\": ", name);
}

static int
do_find(enum json_type type, const char *name)
{
	int i;

	for (i = level; i > 0; i--)
		if (type == stack[i].type &&
		    strcmp(name, stack[i].name) == 0)
			return i;

	/* not found */
	return -1;
}

void
json_do_start(void)
{
	memset(indent, '\t', JSON_MAX_STACK);
	memset(stack, 0, sizeof(stack));
	level = 0;
	stack[level].type = START;

	printf("{\n");
}

void
json_do_finish(void)
{
	while (level > 0)
		json_do_end();
	printf("\n}\n");
}

void
json_do_array(const char *name)
{
	int i, l;

	if ((l = do_find(ARRAY, name)) > 0) {
		/* array already in use, close element and move on */
		for (i = level - l; i > 0; i--)
			json_do_end();
		return;
	}
	/* Do not stack arrays, while allowed this is not needed */
	if (stack[level].type == ARRAY)
		json_do_end();

	do_comma_indent();
	do_name(name);
	printf("[\n");

	if (++level >= JSON_MAX_STACK)
		errx(1, "json stack too deep");
	
	stack[level].name = name;
	stack[level].type = ARRAY;
	stack[level].count = 0;
}

void
json_do_object(const char *name)
{
	int i, l;

	if ((l = do_find(OBJECT, name)) > 0) {
		/* roll back to that object and close it */
		for (i = level - l; i >= 0; i--)
			json_do_end();
	}

	do_comma_indent();
	do_name(name);
	printf("{\n");

	if (++level >= JSON_MAX_STACK)
		errx(1, "json stack too deep");
	
	stack[level].name = name;
	stack[level].type = OBJECT;
	stack[level].count = 0;
}

void
json_do_end(void)
{
	if (stack[level].type == ARRAY)
		printf("\n%.*s]", level, indent);
	else if (stack[level].type == OBJECT)
		printf("\n%.*s}", level, indent);
	else
		errx(1, "json bad stack state");

	stack[level].name = NULL;
	stack[level].type = NONE;
	stack[level].count = 0;

	if (level-- <= 0)
		errx(1, "json stack underflow");

	stack[level].count++;
}

void
json_do_printf(const char *name, const char *fmt, ...)
{
	va_list ap;

	do_comma_indent();

	do_name(name);
	printf("\"");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\"");
}

void
json_do_hexdump(const char *name, void *buf, size_t len)
{
	uint8_t *data = buf;
	size_t i;

	do_comma_indent();
	do_name(name);
	printf("\"");
	for (i=0; i < len; i++)
		printf("%02x", *(data+i));
	printf("\"");
}

void
json_do_bool(const char *name, int v)
{
	do_comma_indent();
	do_name(name);
	if (v)
		printf("true");
	else
		printf("false");
}

void
json_do_uint(const char *name, unsigned long long v)
{
	do_comma_indent();
	do_name(name);
	printf("%llu", v);
}

void
json_do_int(const char *name, long long v)
{
	do_comma_indent();
	do_name(name);
	printf("%lld", v);
}

void
json_do_double(const char *name, double v)
{
	do_comma_indent();
	do_name(name);
	printf("%f", v);
}
