/*	$OpenBSD: tsearch.c,v 1.4 2004/10/01 04:08:45 jsg Exp $	*/

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
#include <stdlib.h>

typedef struct node_t {
    char	  *key;
    struct node_t *left, *right;
} node;

/* find or insert datum into search tree */
void *
tsearch(vkey, vrootp, compar)
	const void 	*vkey;		/* key to be located */
	void		**vrootp;	/* address of tree root */
	int		(*compar)(const void *, const void *);
{
    register node *q;
    char *key = (char *)vkey;
    node **rootp = (node **)vrootp;

    if (rootp == (struct node_t **)0)
	return ((void *)0);
    while (*rootp != (struct node_t *)0) {	/* Knuth's T1: */
	int r;

	if ((r = (*compar)(key, (*rootp)->key)) == 0)	/* T2: */
	    return ((void *)*rootp);		/* we found it! */
	rootp = (r < 0) ?
	    &(*rootp)->left :		/* T3: follow left branch */
	    &(*rootp)->right;		/* T4: follow right branch */
    }
    q = (node *) malloc(sizeof(node));	/* T5: key not found */
    if (q != (struct node_t *)0) {	/* make new node */
	*rootp = q;			/* link new node to old */
	q->key = key;			/* initialize new node */
	q->left = q->right = (struct node_t *)0;
    }
    return ((void *)q);
}

/* delete node with given key */
void *
tdelete(vkey, vrootp, compar)
	const void	*vkey;		/* key to be deleted */
	void		**vrootp;	/* address of the root of tree */
	int		(*compar)(const void *, const void *);
{
    node **rootp = (node **)vrootp;
    char *key = (char *)vkey;
    node *p;
    register node *q;
    register node *r;
    int cmp;

    if (rootp == (struct node_t **)0 || (p = *rootp) == (struct node_t *)0)
	return ((struct node_t *)0);
    while ((cmp = (*compar)(key, (*rootp)->key)) != 0) {
	p = *rootp;
	rootp = (cmp < 0) ?
	    &(*rootp)->left :		/* follow left branch */
	    &(*rootp)->right;		/* follow right branch */
	if (*rootp == (struct node_t *)0)
	    return ((void *)0);		/* key not found */
    }
    r = (*rootp)->right;			/* D1: */
    if ((q = (*rootp)->left) == (struct node_t *)0)	/* Left (struct node_t *)0? */
	q = r;
    else if (r != (struct node_t *)0) {		/* Right link is null? */
	if (r->left == (struct node_t *)0) {	/* D2: Find successor */
	    r->left = q;
	    q = r;
	} else {			/* D3: Find (struct node_t *)0 link */
	    for (q = r->left; q->left != (struct node_t *)0; q = r->left)
		r = q;
	    r->left = q->right;
	    q->left = (*rootp)->left;
	    q->right = (*rootp)->right;
	}
    }
    free((struct node_t *) *rootp);	/* D4: Free node */
    *rootp = q;				/* link parent to new node */
    return(p);
}

/* Walk the nodes of a tree */
static void
trecurse(root, action, level)
	register node	*root;		/* Root of the tree to be walked */
	register void	(*action)();	/* Function to be called at each node */
	register int	level;
{
    if (root->left == (struct node_t *)0 && root->right == (struct node_t *)0)
	(*action)(root, leaf, level);
    else {
	(*action)(root, preorder, level);
	if (root->left != (struct node_t *)0)
	    trecurse(root->left, action, level + 1);
	(*action)(root, postorder, level);
	if (root->right != (struct node_t *)0)
	    trecurse(root->right, action, level + 1);
	(*action)(root, endorder, level);
    }
}

/* Walk the nodes of a tree */
void
twalk(vroot, action)
	const void	*vroot;		/* Root of the tree to be walked */
	void		(*action)(const void *, VISIT, int);
{
    node *root = (node *)vroot;

    if (root != (node *)0 && action != (void(*)())0)
	trecurse(root, action, 0);
}
