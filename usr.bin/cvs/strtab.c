/*	$OpenBSD: strtab.c,v 1.1 2005/03/23 20:21:54 jfb Exp $	*/
/*
 * Copyright (c) 2005 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * The hash table code uses the 32-bit Fowler/Noll/Vo (FNV-1) hash algorithm
 * for indexing.
 *	http://www.isthe.com/chongo/tech/comp/fnv/
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "cvs.h"
#include "log.h"

#define CVS_STRTAB_HASHBITS       8
#define CVS_STRTAB_NBUCKETS      (1 << CVS_STRTAB_HASHBITS)
#define CVS_STRTAB_FNVPRIME      0x01000193
#define CVS_STRTAB_FNVINIT       0x811c9dc5

struct cvs_str {
	char *cs_str;
	int   cs_ref;
	SLIST_ENTRY(cvs_str) cs_link;
};

SLIST_HEAD(cvs_slist, cvs_str);

static struct cvs_slist cvs_strtab[CVS_STRTAB_NBUCKETS];


static struct cvs_str* cvs_strtab_insert (const char *);
static struct cvs_str* cvs_strtab_lookup (const char *);
static u_int32_t       cvs_strtab_hash   (const char *);
static void            cvs_strtab_free   (const char *);



/*
 * cvs_strdup()
 *
 * Simple interface to the string table.
 */
char*
cvs_strdup(const char *s)
{
	struct cvs_str *csp;

	if ((csp = cvs_strtab_lookup(s)) == NULL)
		csp = cvs_strtab_insert(s);
	else
		csp->cs_ref++;

	return (csp->cs_str);
}

/*
 * cvs_strfree()
 */
void
cvs_strfree(char *s)
{
	cvs_strtab_free(s);
}

/*
 *
 */
void
cvs_strtab_init(void)
{
	int i;

	for (i = 0; i < CVS_STRTAB_NBUCKETS; i++)
		SLIST_INIT(&cvs_strtab[i]);
}

/*
 * cvs_strtab_cleanup()
 *
 */
void
cvs_strtab_cleanup(void)
{
	int i, unfreed;
	struct cvs_str *sp;

	unfreed = 0;
	for (i = 0; i < CVS_STRTAB_NBUCKETS; i++) {
		SLIST_FOREACH(sp, &cvs_strtab[i], cs_link)
			unfreed++;
	}

	if (unfreed > 0)
		cvs_log(LP_WARN, "%d unfreed string references in string table",
		    unfreed);
}

/*
 * cvs_strtab_insert()
 *
 * Insert the string <str> in the string table.
 */
static struct cvs_str*
cvs_strtab_insert(const char *str)
{
	u_int32_t h;
	struct cvs_str *sp;

	if ((sp = (struct cvs_str *)malloc(sizeof(*sp))) == NULL) {
		cvs_log(LP_ERRNO, "failed to insert string `%s'", str);
		return (NULL);
	}

	if ((sp->cs_str = strdup(str)) == NULL) {
		free(sp);
		return (NULL);
	}

	h = cvs_strtab_hash(str);

	SLIST_INSERT_HEAD(&cvs_strtab[h], sp, cs_link);

	sp->cs_ref = 1;
	return (sp);
}

/*
 * cvs_strtab_hash()
 *
 * Generate a hash value from the string <str>.
 * The resulting hash value is bitwise AND'ed with the appropriate mask so
 * the returned index does not go out of the boundaries of the hash table.
 */
static u_int32_t
cvs_strtab_hash(const char *str)
{
	const char *np;
	u_int32_t h;

	h = CVS_STRTAB_FNVINIT;
	for (np = str; *np != '\0'; np++) {
		h *= CVS_STRTAB_FNVPRIME;
		h ^= (u_int32_t)*np;
	}

	return (h & (CVS_STRTAB_NBUCKETS - 1));
}

/*
 * cvs_strtab_lookup()
 *
 * Lookup the string <str> in the string table.  If the corresponding entry
 * is found, a pointer to the string is returned and the entry's reference
 * count is increased.
 */
static struct cvs_str*
cvs_strtab_lookup(const char *str)
{
	u_int32_t h;
	struct cvs_str *sp;

	h = cvs_strtab_hash(str);

	SLIST_FOREACH(sp, &(cvs_strtab[h]), cs_link)
		if (strcmp(str, sp->cs_str) == 0) {
			break;
		}

	return (sp);
}

/*
 * cvs_strtab_free()
 *
 * Release a reference to the string <str>.  If the reference count reaches 0,
 * the string is removed from the table and freed.
 */
static void
cvs_strtab_free(const char *str)
{
	u_int32_t h;
	struct cvs_str *sp;

	if ((sp = cvs_strtab_lookup(str)) == NULL) {
		cvs_log(LP_WARN, "attempt to free unregistered string `%s'",
		    str);
		return;
	}

	sp->cs_ref--;
	if (sp->cs_ref == 0) {
		/* no more references, free the file */
		h = cvs_file_hashname(sp->cs_str);

		SLIST_REMOVE(&(cvs_strtab[h]), sp, cvs_str, cs_link);
		free(sp->cs_str);
		free(sp);
	}
}
