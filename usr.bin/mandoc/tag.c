/*      $OpenBSD: tag.c,v 1.6 2015/07/28 18:38:05 schwarze Exp $    */
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
static struct tag_files	 tag_files;


/*
 * Prepare for using a pager.
 * Not all pagers are capable of using a tag file,
 * but for simplicity, create it anyway.
 */
struct tag_files *
tag_init(void)
{
	struct ohash_info	 tag_info;
	int			 ofd;

	ofd = -1;
	tag_files.tfd = -1;

	/* Save the original standard output for use by the pager. */

	if ((tag_files.ofd = dup(STDOUT_FILENO)) == -1)
		goto fail;

	/* Create both temporary output files. */

	(void)strlcpy(tag_files.ofn, "/tmp/man.XXXXXXXXXX",
	    sizeof(tag_files.ofn));
	(void)strlcpy(tag_files.tfn, "/tmp/man.XXXXXXXXXX",
	    sizeof(tag_files.tfn));
	signal(SIGHUP, tag_signal);
	signal(SIGINT, tag_signal);
	signal(SIGTERM, tag_signal);
	if ((ofd = mkstemp(tag_files.ofn)) == -1)
		goto fail;
	if ((tag_files.tfd = mkstemp(tag_files.tfn)) == -1)
		goto fail;
	if (dup2(ofd, STDOUT_FILENO) == -1)
		goto fail;
	close(ofd);

	/*
	 * Set up the ohash table to collect output line numbers
	 * where various marked-up terms are documented.
	 */

	tag_info.alloc = tag_alloc;
	tag_info.calloc = tag_calloc;
	tag_info.free = tag_free;
	tag_info.key_offset = offsetof(struct tag_entry, s);
	tag_info.data = NULL;
	ohash_init(&tag_data, 4, &tag_info);
	return(&tag_files);

fail:
	tag_unlink();
	if (ofd != -1)
		close(ofd);
	if (tag_files.ofd != -1)
		close(tag_files.ofd);
	if (tag_files.tfd != -1)
		close(tag_files.tfd);
	*tag_files.ofn = '\0';
	*tag_files.tfn = '\0';
	tag_files.ofd = -1;
	tag_files.tfd = -1;
	return(NULL);
}

/*
 * Set the line number where a term is defined,
 * unless it is already defined at a higher priority.
 */
void
tag_put(const char *s, int prio, size_t line)
{
	struct tag_entry	*entry;
	size_t			 len;
	unsigned int		 slot;

	if (tag_files.tfd <= 0)
		return;
	slot = ohash_qlookup(&tag_data, s);
	entry = ohash_find(&tag_data, slot);
	if (entry == NULL) {
		len = strlen(s) + 1;
		entry = mandoc_malloc(sizeof(*entry) + len);
		memcpy(entry->s, s, len);
		ohash_insert(&tag_data, slot, entry);
	} else if (entry->prio <= prio)
		return;
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

	if (tag_files.tfd <= 0)
		return;
	stream = fdopen(tag_files.tfd, "w");
	entry = ohash_first(&tag_data, &slot);
	while (entry != NULL) {
		if (stream != NULL)
			fprintf(stream, "%s %s %zu\n",
			    entry->s, tag_files.ofn, entry->line);
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

	if (*tag_files.ofn != '\0')
		unlink(tag_files.ofn);
	if (*tag_files.tfn != '\0')
		unlink(tag_files.tfn);
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
