/*	$OpenBSD: dba.c,v 1.5 2016/08/17 20:46:06 schwarze Exp $ */
/*
 * Copyright (c) 2016 Ingo Schwarze <schwarze@openbsd.org>
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
 *
 * Allocation-based version of the mandoc database, for read-write access.
 * The interface is defined in "dba.h".
 */
#include <sys/types.h>
#include <endian.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc_aux.h"
#include "mansearch.h"
#include "dba_write.h"
#include "dba_array.h"
#include "dba.h"

static void	*prepend(const char *, char);
static void	 dba_pages_write(struct dba_array *);
static int	 compare_names(const void *, const void *);
static int	 compare_strings(const void *, const void *);
static void	 dba_macros_write(struct dba_array *);
static void	 dba_macro_write(struct dba_array *);


/*** top-level functions **********************************************/

struct dba *
dba_new(int32_t npages)
{
	struct dba	*dba;
	int32_t		 im;

	dba = mandoc_malloc(sizeof(*dba));
	dba->pages = dba_array_new(npages, DBA_GROW);
	dba->macros = dba_array_new(MACRO_MAX, 0);
	for (im = 0; im < MACRO_MAX; im++)
		dba_array_set(dba->macros, im, dba_array_new(128, DBA_GROW));
	return dba;
}

void
dba_free(struct dba *dba)
{
	struct dba_array	*page, *macro, *entry;

	dba_array_FOREACH(dba->macros, macro) {
		dba_array_undel(macro);
		dba_array_FOREACH(macro, entry) {
			free(dba_array_get(entry, 0));
			dba_array_free(dba_array_get(entry, 1));
			dba_array_free(entry);
		}
		dba_array_free(macro);
	}
	dba_array_free(dba->macros);

	dba_array_undel(dba->pages);
	dba_array_FOREACH(dba->pages, page) {
		dba_array_free(dba_array_get(page, DBP_NAME));
		dba_array_free(dba_array_get(page, DBP_SECT));
		dba_array_free(dba_array_get(page, DBP_ARCH));
		free(dba_array_get(page, DBP_DESC));
		dba_array_free(dba_array_get(page, DBP_FILE));
		dba_array_free(page);
	}
	dba_array_free(dba->pages);

	free(dba);
}

/*
 * Write the complete mandoc database to disk; the format is:
 * - One integer each for magic and version.
 * - One pointer each to the macros table and to the final magic.
 * - The pages table.
 * - The macros table.
 * - And at the very end, the magic integer again.
 */
int
dba_write(const char *fname, struct dba *dba)
{
	int	 save_errno;
	int32_t	 pos_end, pos_macros, pos_macros_ptr;

	if (dba_open(fname) == -1)
		return -1;
	dba_int_write(MANDOCDB_MAGIC);
	dba_int_write(MANDOCDB_VERSION);
	pos_macros_ptr = dba_skip(1, 2);
	dba_pages_write(dba->pages);
	pos_macros = dba_tell();
	dba_macros_write(dba->macros);
	pos_end = dba_tell();
	dba_int_write(MANDOCDB_MAGIC);
	dba_seek(pos_macros_ptr);
	dba_int_write(pos_macros);
	dba_int_write(pos_end);
	if (dba_close() == -1) {
		save_errno = errno;
		unlink(fname);
		errno = save_errno;
		return -1;
	}
	return 0;
}


/*** functions for handling pages *************************************/

/*
 * Create a new page and append it to the pages table.
 */
struct dba_array *
dba_page_new(struct dba_array *pages, const char *arch,
    const char *desc, const char *file, enum form form)
{
	struct dba_array *page, *entry;

	page = dba_array_new(DBP_MAX, 0);
	entry = dba_array_new(1, DBA_STR | DBA_GROW);
	dba_array_add(page, entry);
	entry = dba_array_new(1, DBA_STR | DBA_GROW);
	dba_array_add(page, entry);
	if (arch != NULL && *arch != '\0') {
		entry = dba_array_new(1, DBA_STR | DBA_GROW);
		dba_array_add(entry, (void *)arch);
	} else
		entry = NULL;
	dba_array_add(page, entry);
	dba_array_add(page, mandoc_strdup(desc));
	entry = dba_array_new(1, DBA_STR | DBA_GROW);
	dba_array_add(entry, prepend(file, form));
	dba_array_add(page, entry);
	dba_array_add(pages, page);
	return page;
}

/*
 * Add a section, architecture, or file name to an existing page.
 * Passing the NULL pointer for the architecture makes the page MI.
 * In that case, any earlier or later architectures are ignored.
 */
void
dba_page_add(struct dba_array *page, int32_t ie, const char *str)
{
	struct dba_array	*entries;
	char			*entry;

	entries = dba_array_get(page, ie);
	if (ie == DBP_ARCH) {
		if (entries == NULL)
			return;
		if (str == NULL || *str == '\0') {
			dba_array_free(entries);
			dba_array_set(page, DBP_ARCH, NULL);
			return;
		}
	}
	if (*str == '\0')
		return;
	dba_array_FOREACH(entries, entry) {
		if (ie == DBP_FILE && *entry < ' ')
			entry++;
		if (strcmp(entry, str) == 0)
			return;
	}
	dba_array_add(entries, (void *)str);
}

/*
 * Add an additional name to an existing page.
 */
void
dba_page_alias(struct dba_array *page, const char *name, uint64_t mask)
{
	struct dba_array	*entries;
	char			*entry;
	char			 maskbyte;

	if (*name == '\0')
		return;
	maskbyte = mask & NAME_MASK;
	entries = dba_array_get(page, DBP_NAME);
	dba_array_FOREACH(entries, entry) {
		if (strcmp(entry + 1, name) == 0) {
			*entry |= maskbyte;
			return;
		}
	}
	dba_array_add(entries, prepend(name, maskbyte));
}

/*
 * Return a pointer to a temporary copy of instr with inbyte prepended.
 */
static void *
prepend(const char *instr, char inbyte)
{
	static char	*outstr = NULL;
	static size_t	 outlen = 0;
	size_t		 newlen;

	newlen = strlen(instr) + 1;
	if (newlen > outlen) {
		outstr = mandoc_realloc(outstr, newlen + 1);
		outlen = newlen;
	}
	*outstr = inbyte;
	memcpy(outstr + 1, instr, newlen);
	return outstr;
}

/*
 * Write the pages table to disk; the format is:
 * - One integer containing the number of pages.
 * - For each page, five pointers to the names, sections,
 *   architectures, description, and file names of the page.
 *   MI pages write 0 instead of the architecture pointer.
 * - One list each for names, sections, architectures, descriptions and
 *   file names.  The description for each page ends with a NUL byte.
 *   For all the other lists, each string ends with a NUL byte,
 *   and the last string for a page ends with two NUL bytes.
 * - To assure alignment of following integers,
 *   the end is padded with NUL bytes up to a multiple of four bytes.
 */
static void
dba_pages_write(struct dba_array *pages)
{
	struct dba_array	*page, *entry;
	int32_t			 pos_pages, pos_end;

	pos_pages = dba_array_writelen(pages, 5);
	dba_array_FOREACH(pages, page) {
		dba_array_setpos(page, DBP_NAME, dba_tell());
		entry = dba_array_get(page, DBP_NAME);
		dba_array_sort(entry, compare_names);
		dba_array_writelst(entry);
	}
	dba_array_FOREACH(pages, page) {
		dba_array_setpos(page, DBP_SECT, dba_tell());
		entry = dba_array_get(page, DBP_SECT);
		dba_array_sort(entry, compare_strings);
		dba_array_writelst(entry);
	}
	dba_array_FOREACH(pages, page) {
		if ((entry = dba_array_get(page, DBP_ARCH)) != NULL) {
			dba_array_setpos(page, DBP_ARCH, dba_tell());
			dba_array_sort(entry, compare_strings);
			dba_array_writelst(entry);
		} else
			dba_array_setpos(page, DBP_ARCH, 0);
	}
	dba_array_FOREACH(pages, page) {
		dba_array_setpos(page, DBP_DESC, dba_tell());
		dba_str_write(dba_array_get(page, DBP_DESC));
	}
	dba_array_FOREACH(pages, page) {
		dba_array_setpos(page, DBP_FILE, dba_tell());
		dba_array_writelst(dba_array_get(page, DBP_FILE));
	}
	pos_end = dba_align();
	dba_seek(pos_pages);
	dba_array_FOREACH(pages, page)
		dba_array_writepos(page);
	dba_seek(pos_end);
}

static int
compare_names(const void *vp1, const void *vp2)
{
	const char	*cp1, *cp2;
	int		 diff;

	cp1 = *(char **)vp1;
	cp2 = *(char **)vp2;
	return (diff = *cp2 - *cp1) ? diff :
	    strcasecmp(cp1 + 1, cp2 + 1);
}

static int
compare_strings(const void *vp1, const void *vp2)
{
	const char	*cp1, *cp2;

	cp1 = *(char **)vp1;
	cp2 = *(char **)vp2;
	return strcmp(cp1, cp2);
}

/*** functions for handling macros ************************************/

/*
 * Create a new macro entry and append it to one of the macro tables.
 */
void
dba_macro_new(struct dba *dba, int32_t im, const char *value,
    const int32_t *pp)
{
	struct dba_array	*entry, *pages;
	const int32_t		*ip;
	int32_t			 np;

	np = 0;
	for (ip = pp; *ip; ip++)
		np++;
	pages = dba_array_new(np, DBA_GROW);
	for (ip = pp; *ip; ip++)
		dba_array_add(pages, dba_array_get(dba->pages,
		    be32toh(*ip) / 5 / sizeof(*ip) - 1));

	entry = dba_array_new(2, 0);
	dba_array_add(entry, mandoc_strdup(value));
	dba_array_add(entry, pages);

	dba_array_add(dba_array_get(dba->macros, im), entry);
}

/*
 * Look up a macro entry by value and add a reference to a new page to it.
 * If the value does not yet exist, create a new macro entry
 * and add it to the macro table in question.
 */
void
dba_macro_add(struct dba_array *macros, int32_t im, const char *value,
    struct dba_array *page)
{
	struct dba_array	*macro, *entry, *pages;

	if (*value == '\0')
		return;
	macro = dba_array_get(macros, im);
	dba_array_FOREACH(macro, entry)
		if (strcmp(value, dba_array_get(entry, 0)) == 0)
			break;
	if (entry == NULL) {
		entry = dba_array_new(2, 0);
		dba_array_add(entry, mandoc_strdup(value));
		pages = dba_array_new(1, DBA_GROW);
		dba_array_add(entry, pages);
		dba_array_add(macro, entry);
	} else
		pages = dba_array_get(entry, 1);
	dba_array_add(pages, page);
}

/*
 * Write the macros table to disk; the format is:
 * - The number of macro tables (actually, MACRO_MAX).
 * - That number of pointers to the individual macro tables.
 * - The individual macro tables.
 */
static void
dba_macros_write(struct dba_array *macros)
{
	struct dba_array	*macro;
	int32_t			 im, pos_macros, pos_end;

	pos_macros = dba_array_writelen(macros, 1);
	im = 0;
	dba_array_FOREACH(macros, macro) {
		dba_array_setpos(macros, im++, dba_tell());
		dba_macro_write(macro);
	}
	pos_end = dba_tell();
	dba_seek(pos_macros);
	dba_array_writepos(macros);
	dba_seek(pos_end);
}

/*
 * Write one individual macro table to disk; the format is:
 * - The number of entries in the table.
 * - For each entry, two pointers, the first one to the value
 *   and the second one to the list of pages.
 * - A list of values, each ending in a NUL byte.
 * - To assure alignment of following integers,
 *   padding with NUL bytes up to a multiple of four bytes.
 * - A list of pointers to pages, each list ending in a 0 integer.
 */
static void
dba_macro_write(struct dba_array *macro)
{
	struct dba_array	*entry, *pages, *page;
	int			 empty;
	int32_t			 addr, pos_macro, pos_end;

	dba_array_FOREACH(macro, entry) {
		pages = dba_array_get(entry, 1);
		empty = 1;
		dba_array_FOREACH(pages, page)
			if (dba_array_getpos(page))
				empty = 0;
		if (empty)
			dba_array_del(macro);
	}
	pos_macro = dba_array_writelen(macro, 2);
	dba_array_FOREACH(macro, entry) {
		dba_array_setpos(entry, 0, dba_tell());
		dba_str_write(dba_array_get(entry, 0));
	}
	dba_align();
	dba_array_FOREACH(macro, entry) {
		dba_array_setpos(entry, 1, dba_tell());
		pages = dba_array_get(entry, 1);
		dba_array_FOREACH(pages, page)
			if ((addr = dba_array_getpos(page)))
				dba_int_write(addr);
		dba_int_write(0);
	}
	pos_end = dba_tell();
	dba_seek(pos_macro);
	dba_array_FOREACH(macro, entry)
		dba_array_writepos(entry);
	dba_seek(pos_end);
}
