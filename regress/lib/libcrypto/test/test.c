/*	$OpenBSD: test.c,v 1.8 2026/09/22 20:21:46 joshua Exp $ */
/*
 * Copyright (c) 2025 Joshua Sing <joshua@joshuasing.dev>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test.h"

#define TEST_INDENT "    "
#define TEST_INDENT_LEN (sizeof(TEST_INDENT) - 1)

struct test_buf {
	char *data;
	size_t len;
	size_t cap;
};

struct test_chatty {
	char *last_name;
};

struct test {
	struct test *parent;
	struct test_chatty *chatty;
	char *name;
	struct test_buf output;
	struct test_buf partial;
	int skipped;
	int failed;
};

static void
test_buf_append(struct test_buf *b, const char *data, size_t len)
{
	size_t cap;
	char *p;

	if (len > SIZE_MAX - b->len)
		errx(1, "test output too large");

	/* Expand capacity to max(b->len + len, b->cap * 2) */
	if (b->cap - b->len < len) {
		cap = (b->len + len + 1) / 2;
		if (cap < b->cap)
			cap = b->cap;
		if ((p = reallocarray(b->data, 2, cap)) == NULL)
			err(1, "reallocarray");
		b->data = p;
		b->cap = cap * 2;
	}

	if (len > 0)
		memcpy(b->data + b->len, data, len);
	b->len += len;
}

static void
test_buf_append_indent(struct test_buf *b, const char *data, size_t len)
{
	const char *nl;
	size_t n;

	while (len > 0) {
		if ((nl = memchr(data, '\n', len)) != NULL)
			n = nl - data + 1;
		else
			n = len;

		test_buf_append(b, TEST_INDENT, TEST_INDENT_LEN);
		test_buf_append(b, data, n);
		data += n;
		len -= n;
	}
}

static void
test_fwrite(const char *data, size_t len)
{
	if (len == 0)
		return;
	if (fwrite(data, 1, len, stderr) != len)
		err(1, "fwrite");
}

static void
test_chatty_set_name(struct test_chatty *c, const char *name)
{
	if (c->last_name != NULL && strcmp(c->last_name, name) == 0)
		return;

	free(c->last_name);
	if ((c->last_name = strdup(name)) == NULL)
		err(1, "strdup");
}

static void
test_chatty_update(struct test_chatty *c, const char *name, const char *data,
    size_t len)
{
	test_chatty_set_name(c, name);
	test_fwrite(data, len);
}

static void
test_chatty_print(struct test_chatty *c, const char *name, const char *data,
    size_t len)
{
	if (c->last_name != NULL && strcmp(c->last_name, name) != 0) {
		if (fprintf(stderr, "=== NAME  %s\n", name) < 0)
			err(1, "fprintf");
	}
	test_chatty_set_name(c, name);
	test_fwrite(data, len);
}


static struct test *
test_new_root(void)
{
	struct test *t;

	if ((t = calloc(1, sizeof(*t))) == NULL)
		err(1, "calloc");

	return t;
}

static struct test *
test_new(struct test *parent, const char *name)
{
	struct test *t;

	if (parent == NULL || name == NULL)
		err(1, "parent and name must not be NULL");

	if ((t = calloc(1, sizeof(*t))) == NULL)
		err(1, "calloc");

	t->parent = parent;
	t->chatty = parent->chatty;

	if (parent->name == NULL) {
		if ((t->name = strdup(name)) == NULL)
			err(1, "strdup");
	} else {
		/* Add prefix of parent test name. */
		if (asprintf(&t->name, "%s/%s", parent->name, name) < 0)
			err(1, "asprintf");
	}

	return t;
}

struct test *
test_init(void)
{
	struct test *t;
	char *v;

	t = test_new_root();

	if (((v = getenv("TEST_VERBOSE")) != NULL) && strcmp(v, "0") != 0) {
		if ((t->chatty = calloc(1, sizeof(*t->chatty))) == NULL)
			err(1, "calloc");
	}

	return t;
}

static void
test_cleanup(struct test *t)
{
	free(t->name);
	free(t->output.data);
	free(t->partial.data);
	free(t);
}

static void
test_write_line(struct test *t, const char *line, size_t len)
{
	/* The root test never reports, so its output is written directly. */
	if (t->parent == NULL) {
		test_fwrite(TEST_INDENT, TEST_INDENT_LEN);
		test_fwrite(line, len);
		return;
	}

	if (t->chatty == NULL) {
		test_buf_append(&t->output, TEST_INDENT, TEST_INDENT_LEN);
		test_buf_append(&t->output, line, len);
		return;
	}

	test_chatty_print(t->chatty, t->name, TEST_INDENT, TEST_INDENT_LEN);
	test_fwrite(line, len);
}

static void
test_output_write(struct test *t, const char *data, size_t len)
{
	const char *nl;
	size_t n;

	while ((nl = memchr(data, '\n', len)) != NULL) {
		n = nl - data + 1;
		if (t->partial.len > 0) {
			test_buf_append(&t->partial, data, n);
			test_write_line(t, t->partial.data, t->partial.len);
			t->partial.len = 0;
		} else
			test_write_line(t, data, n);
		data += n;
		len -= n;
	}

	test_buf_append(&t->partial, data, len);
}

static void
test_flush_partial(struct test *t)
{
	if (t->partial.len > 0)
		test_output_write(t, "\n", 1);
}

static void
test_write_parent(struct test *t, const char *data, size_t len)
{
	if (t->parent->parent == NULL)
		test_fwrite(data, len);
	else
		test_buf_append_indent(&t->parent->output, data, len);
}

/*
 * test_report writes the result of a test, followed by its buffered output.
 * Passing and skipped tests are only reported in verbose mode.
 */
static void
test_report(struct test *t)
{
	const char *status;
	char *header;
	int len;

	if (t->failed)
		status = "FAIL";
	else if (t->chatty != NULL && t->skipped)
		status = "SKIP";
	else if (t->chatty != NULL)
		status = "PASS";
	else
		return;

	if ((len = asprintf(&header, "--- %s: %s\n", status, t->name)) < 0)
		err(1, "asprintf");

	/*
	 * Buffered output always ends with a newline, so writing the header
	 * and output separately gives the same indentation as a single write.
	 */
	if (t->chatty != NULL && t->parent->parent == NULL) {
		test_chatty_update(t->chatty, t->name, header, len);
		test_fwrite(t->output.data, t->output.len);
	} else {
		test_write_parent(t, header, len);
		test_write_parent(t, t->output.data, t->output.len);
	}
	t->output.len = 0;

	free(header);
}

int
test_result(struct test *t)
{
	int failed = t->failed;

	test_flush_partial(t);

	if (fputs(failed ? "FAIL\n" : "PASS\n", stderr) == EOF)
		err(1, "fputs");

	if (t->chatty != NULL) {
		free(t->chatty->last_name);
		free(t->chatty);
	}
	test_cleanup(t);

	return failed;
}

void
test_fail(struct test *t)
{
	t->failed = 1;

	/* Also fail parent. */
	if (t->parent != NULL)
		test_fail(t->parent);
}

void
test_skipnow(struct test *t)
{
	t->skipped = 1;
}

void
test_printf(struct test *t, const char *fmt, ...)
{
	va_list ap;
	char *msg;
	int len;

	va_start(ap, fmt);
	len = vasprintf(&msg, fmt, ap);
	va_end(ap);
	if (len == -1)
		err(1, "vasprintf");

	test_output_write(t, msg, len);

	free(msg);
}

static void
test_vlogf_internal(struct test *t, const char *label, const char *func,
    const char *file, int line, const char *fmt, va_list ap)
{
	struct test_buf buf;
	const char *filename, *msg, *nl;
	char *prefix, *s;
	char *l = ": ";
	size_t len, n;
	int ret;

	if (label == NULL) {
		label = "";
		l = "";
	}

	memset(&buf, 0, sizeof(buf));

	if ((ret = vasprintf(&s, fmt, ap)) < 0)
		err(1, "vasprintf");
	msg = s;
	len = ret;

	/* Remove a single trailing newline, one is always added below. */
	if (len > 0 && msg[len - 1] == '\n')
		len--;

	if ((filename = strrchr(file, '/')) != NULL)
		filename++;
	else
		filename = file;

	/* Prefix with the call location. */
	if ((ret = asprintf(&prefix, "%s [%s:%d]%s%s: ", func, filename, line,
	    l, label)) == -1)
		err(1, "asprintf");
	test_buf_append(&buf, prefix, ret);

	/* Second and subsequent lines are indented with an additional level. */
	while ((nl = memchr(msg, '\n', len)) != NULL) {
		n = nl - msg + 1;
		test_buf_append(&buf, msg, n);
		test_buf_append(&buf, TEST_INDENT, TEST_INDENT_LEN);
		msg += n;
		len -= n;
	}
	test_buf_append(&buf, msg, len);
	test_buf_append(&buf, "\n", 1);

	test_flush_partial(t);
	test_output_write(t, buf.data, buf.len);

	free(buf.data);
	free(prefix);
	free(s);
}

void
test_logf_internal(struct test *t, const char *label, const char *func,
    const char *file, int line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	test_vlogf_internal(t, label, func, file, line, fmt, ap);
	va_end(ap);
}

void
test_run(struct test *pt, const char *name, test_run_func *fn, const void *arg)
{
	struct test *t, *p;
	char *msg;
	int len;

	t = test_new(pt, name);

	if (t->chatty != NULL) {
		if ((len = asprintf(&msg, "=== RUN   %s\n", t->name)) < 0)
			err(1, "asprintf");
		test_chatty_update(t->chatty, t->name, msg, len);
		free(msg);
	}

	fn(t, arg);

	/* Flush partial lines for this test and all parents. */
	for (p = t; p->parent != NULL; p = p->parent)
		test_flush_partial(p);

	test_report(t);
	test_cleanup(t);
}
