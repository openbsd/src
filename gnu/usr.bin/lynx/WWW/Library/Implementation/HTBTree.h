/*                  /Net/dxcern/userd/timbl/hypertext/WWW/Library/Implementation/HTBTree.html
                         BALANCED BINARY TREE FOR SORTING THINGS

   Tree creation, traversal and freeing.  User-supplied comparison routine.

   Author: Arthur Secret, CERN. Public domain.  Please mail bugs and changes to
   www-request@info.cern.ch

   part of libWWW

 */
#ifndef HTBTREE_H
#define HTBTREE_H 1

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
/*

Data structures

 */ typedef struct _HTBTree_element {
	void *object;		/* User object */
	struct _HTBTree_element *up;
	struct _HTBTree_element *left;
	int left_depth;
	struct _HTBTree_element *right;
	int right_depth;
    } HTBTElement;

    typedef int (*HTComparer) (void *a, void *b);

    typedef struct _HTBTree_top {
	HTComparer compare;
	struct _HTBTree_element *top;
    } HTBTree;

/*

Create a binary tree given its discrimination routine

 */
    extern HTBTree *HTBTree_new(HTComparer comp);

/*

Free storage of the tree but not of the objects

 */
    extern void HTBTree_free(HTBTree *tree);

/*

Free storage of the tree and of the objects

 */
    extern void HTBTreeAndObject_free(HTBTree *tree);

/*

Add an object to a binary tree

 */

    extern void HTBTree_add(HTBTree *tree, void *object);

/*

Search an object in a binary tree

  returns          Pointer to equivalent object in a tree or NULL if none.
 */

    extern void *HTBTree_search(HTBTree *tree, void *object);

/*

Find user object for element

 */
#define HTBTree_object(element)  ((element)->object)

/*

Find next element in depth-first order

  ON ENTRY,

  ele                    if NULL, start with leftmost element. if != 0 give next object to
                         the right.

  returns                Pointer to element or NULL if none left.

 */
    extern HTBTElement *HTBTree_next(HTBTree *tree, HTBTElement *ele);

#ifdef __cplusplus
}
#endif
#endif				/* HTBTREE_H */
