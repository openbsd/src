/*
 * Copyright (c) 2007-2008 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <sys/types.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */

#include "sudo.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: list.c,v 1.6 2008/11/09 14:13:12 millert Exp $";
#endif /* lint */

struct list_proto {
    struct list_proto *prev;
    struct list_proto *next;
};

struct list_head_proto {
    struct list_proto *first;
    struct list_proto *last;
};

/*
 * Pop the last element off the end of vh.
 * Returns the popped element.
 */
void *
tq_pop(vh)
    void *vh;
{
    struct list_head_proto *h = (struct list_head_proto *)vh;
    void *last = NULL;

    if (!tq_empty(h)) {
	last = (void *)h->last;
	if (h->first == h->last) {
	    h->first = NULL;
	    h->last = NULL;
	} else {
	    h->last = h->last->prev;
	    h->last->next = NULL;
	}
    }
    return (last);
}

/*
 * Convert from a semi-circle queue to normal doubly-linked list
 * with a head node.
 */
void
list2tq(vh, vl)
    void *vh;
    void *vl;
{
    struct list_head_proto *h = (struct list_head_proto *)vh;
    struct list_proto *l = (struct list_proto *)vl;

    if (l != NULL) {
#ifdef DEBUG
	if (l->prev == NULL) {
	    warningx("list2tq called with non-semicircular list");
	    abort();
	}
#endif
	h->first = l;
	h->last = l->prev;	/* l->prev points to the last member of l */
	l->prev = NULL;		/* zero last ptr now that we have a head */
    } else {
	h->first = NULL;
	h->last = NULL;
    }
}

/*
 * Append one queue (or single entry) to another using the
 * circular properties of the prev pointer to simplify the logic.
 */
void
list_append(vl1, vl2)
    void *vl1;
    void *vl2;
{
    struct list_proto *l1 = (struct list_proto *)vl1;
    struct list_proto *l2 = (struct list_proto *)vl2;
    void *tail = l2->prev;

    l1->prev->next = l2;
    l2->prev = l1->prev;
    l1->prev = tail;
}

/*
 * Append the list of entries to the head node and convert 
 * e from a semi-circle queue to normal doubly-linked list. 
 */
void
tq_append(vh, vl)
    void *vh;
    void *vl;
{
    struct list_head_proto *h = (struct list_head_proto *)vh;
    struct list_proto *l = (struct list_proto *)vl;
    void *tail = l->prev;

    if (h->first == NULL)
	h->first = l;
    else
	h->last->next = l;
    l->prev = h->last;
    h->last = tail;
}
