/*
 * Copyright (c) 2004-2005m, 2007-2009
 *	Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <sys/param.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "sudo.h"
#include "parse.h"
#include "redblack.h"
#include <gram.h>

/*
 * Globals
 */
struct rbtree *aliases;
unsigned int alias_seqno;

/*
 * Comparison function for the red-black tree.
 * Aliases are sorted by name with the type used as a tie-breaker.
 */
int
alias_compare(v1, v2)
    const void *v1, *v2;
{
    const struct alias *a1 = (const struct alias *)v1;
    const struct alias *a2 = (const struct alias *)v2;
    int res;

    if (v1 == NULL)
	res = -1;
    else if (v2 == NULL)
	res = 1;
    else if ((res = strcmp(a1->name, a2->name)) == 0)
	res = a1->type - a2->type;
    return(res);
}

/*
 * Search the tree for an alias with the specified name and type.
 * Returns a pointer to the alias structure or NULL if not found.
 */
struct alias *
alias_find(name, type)
    char *name;
    int type;
{
    struct alias key;
    struct rbnode *node;
    struct alias *a = NULL;

    key.name = name;
    key.type = type;
    if ((node = rbfind(aliases, &key)) != NULL) {
	    /*
	     * Compare the global sequence number with the one stored
	     * in the alias.  If they match then we've seen this alias
	     * before and found a loop.
	     */
	    a = node->data;
	    if (a->seqno == alias_seqno)
		return(NULL);
	    a->seqno = alias_seqno;
    }
    return(a);
}

/*
 * Add an alias to the aliases redblack tree.
 * Returns NULL on success and an error string on failure.
 */
char *
alias_add(name, type, members)
    char *name;
    int type;
    struct member *members;
{
    static char errbuf[512];
    struct alias *a;

    a = emalloc(sizeof(*a));
    a->name = name;
    a->type = type;
    a->seqno = 0;
    list2tq(&a->members, members);
    if (rbinsert(aliases, a)) {
	snprintf(errbuf, sizeof(errbuf), "Alias `%s' already defined", name);
	alias_free(a);
	return(errbuf);
    }
    return(NULL);
}

/*
 * Apply a function to each alias entry and pass in a cookie.
 */
void
alias_apply(func, cookie)
    int (*func) __P((void *, void *));
    void *cookie;
{
    rbapply(aliases, func, cookie, inorder);
}

/*
 * Returns TRUE if there are no aliases, else FALSE.
 */
int
no_aliases()
{
    return(rbisempty(aliases));
}

/*
 * Free memory used by an alias struct and its members.
 */
void
alias_free(v)
    void *v;
{
    struct alias *a = (struct alias *)v;
    struct member *m;
    struct sudo_command *c;
    void *next;

    efree(a->name);
    for (m = a->members.first; m != NULL; m = next) {
	next = m->next;
	if (m->type == COMMAND) {
		c = (struct sudo_command *) m->name;
		efree(c->cmnd);
		efree(c->args);
	}
	efree(m->name);
	efree(m);
    }
    efree(a);
}

/*
 * Find the named alias, remove it from the tree and return it.
 */
struct alias *
alias_remove(name, type)
    char *name;
    int type;
{
    struct rbnode *node;
    struct alias key, *a;

    key.name = name;
    key.type = type;
    if ((node = rbfind(aliases, &key)) == NULL)
	return(NULL);
    a = rbdelete(aliases, node);
    return(a);
}

void
init_aliases()
{
    if (aliases != NULL)
	rbdestroy(aliases, alias_free);
    aliases = rbcreate(alias_compare);
}
