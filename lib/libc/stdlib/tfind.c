/*	$OpenBSD: tfind.c,v 1.4 2004/10/01 04:08:45 jsg Exp $	*/

/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 */
/*LINTLIBRARY*/
#include <search.h>

typedef struct node_t
{
    char	  *key;
    struct node_t *llink, *rlink;
} node;

/* find a node, or return 0 */
void *
tfind(vkey, vrootp, compar)
	const void	*vkey;		/* key to be found */
	void		*const *vrootp;	/* address of the tree root */
	int		(*compar)(const void *, const void *);
{
    char *key = (char *)vkey;
    node **rootp = (node **)vrootp;

    if (rootp == (struct node_t **)0)
	return ((struct node_t *)0);
    while (*rootp != (struct node_t *)0) {	/* T1: */
	int r;
	if ((r = (*compar)(key, (*rootp)->key)) == 0)	/* T2: */
	    return (*rootp);		/* key found */
	rootp = (r < 0) ?
	    &(*rootp)->llink :		/* T3: follow left branch */
	    &(*rootp)->rlink;		/* T4: follow right branch */
    }
    return (node *)0;
}
