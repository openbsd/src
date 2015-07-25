/*      $OpenBSD: tag.c,v 1.4 2015/07/25 14:01:39 schwarze Exp $    */
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
#include <sys/types.h>

#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ohash.h>

#include "mandoc_aux.h"
#include "tag.h"

struct tag_entry {
	size_t	 line;
	int	 prio;
	char	 s[];
};

static	void	 tag_signal(int);
static	void	*tag_alloc(size_t, void *);
static	void	 tag_free(void *, void *);
static	void	*tag_calloc(size_t, size_t, void *);

static struct ohash	 tag_data;
static char		*tag_fn = NULL;
static int		 tag_fd = -1;


/*
 * Set up the ohash table to collect output line numbers
 * where various marked-up terms are documented and create
 * the temporary tags file, saving the name for the pager.
 */
char *
tag_init(void)
{
	struct ohash_info	 tag_info;

	tag_fn = mandoc_strdup("/tmp/man.XXXXXXXXXX");
	signal(SIGHUP, tag_signal);
	signal(SIGINT, tag_signal);
	signal(SIGTERM, tag_signal);
	if ((tag_fd = mkstemp(tag_fn)) == -1) {
		free(tag_fn);
		tag_fn = NULL;
		return(NULL);
	}

	tag_info.alloc = tag_alloc;
	tag_info.calloc = tag_calloc;
	tag_info.free = tag_free;
	tag_info.key_offset = offsetof(struct tag_entry, s);
	tag_info.data = NULL;
	ohash_init(&tag_data, 4, &tag_info);
	return(tag_fn);
}

/*
 * Return the line number where a term is defined,
 * or 0 if the term is unknown.
 */
size_t
tag_get(const char *s, size_t len, int prio)
{
	struct tag_entry	*entry;
	const char		*end;
	unsigned int		 slot;

	if (tag_fd == -1)
		return(0);
	if (len == 0)
		len = strlen(s);
	end = s + len;
	slot = ohash_qlookupi(&tag_data, s, &end);
	entry = ohash_find(&tag_data, slot);
	return((entry == NULL || prio < entry->prio) ? 0 : entry->line);
}

/*
 * Set the line number where a term is defined.
 */
void
tag_put(const char *s, size_t len, int prio, size_t line)
{
	struct tag_entry	*entry;
	const char		*end;
	unsigned int		 slot;

	if (tag_fd == -1)
		return;
	if (len == 0)
		len = strlen(s);
	end = s + len;
	slot = ohash_qlookupi(&tag_data, s, &end);
	entry = ohash_find(&tag_data, slot);
	if (entry == NULL) {
		entry = mandoc_malloc(sizeof(*entry) + len + 1);
		memcpy(entry->s, s, len);
		entry->s[len] = '\0';
		ohash_insert(&tag_data, slot, entry);
	}
	entry->line = line;
	entry->prio = prio;
}

/*
 * Write out the tags file using the previously collected
 * information and clear the ohash table while going along.
 */
void
tag_write(void)
{
	FILE			*stream;
	struct tag_entry	*entry;
	unsigned int		 slot;

	if (tag_fd == -1)
		return;
	stream = fdopen(tag_fd, "w");
	entry = ohash_first(&tag_data, &slot);
	while (entry != NULL) {
		if (stream != NULL)
			fprintf(stream, "%s - %zu\n", entry->s, entry->line);
		free(entry);
		entry = ohash_next(&tag_data, &slot);
	}
	ohash_delete(&tag_data);
	if (stream != NULL)
		fclose(stream);
}

void
tag_unlink(void)
{

	if (tag_fn != NULL)
		unlink(tag_fn);
}

static void
tag_signal(int signum)
{

	tag_unlink();
	signal(signum, SIG_DFL);
	kill(getpid(), signum);
	/* NOTREACHED */
	_exit(1);
}

/*
 * Memory management callback functions for ohash.
 */
static void *
tag_alloc(size_t sz, void *arg)
{

	return(mandoc_malloc(sz));
}

static void *
tag_calloc(size_t nmemb, size_t sz, void *arg)
{

	return(mandoc_calloc(nmemb, sz));
}

static void
tag_free(void *p, void *arg)
{

	free(p);
}
