/*	$OpenBSD: tree.h,v 1.31 2023/03/08 04:43:09 guenther Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_SYS_TREE_H_
#define	_SYS_TREE_H_

#ifdef _KERNEL
#include <sys/stdint.h>
#else
#include <stdint.h>
#endif

#include <sys/_null.h>

/*
 * This file defines data structures for different types of trees:
 * splay trees and rank-balanced trees with weak AVL conditions.
 *
 * A splay tree is a self-organizing data structure.  Every operation
 * on the tree causes a splay to happen.  The splay moves the requested
 * node to the root of the tree and partly rebalances it.
 *
 * This has the benefit that request locality causes faster lookups as
 * the requested nodes move to the top of the tree.  On the other hand,
 * every lookup causes memory writes.
 *
 * The Balance Theorem bounds the total access time for m operations
 * and n inserts on an initially empty tree as O((m + n)lg n).  The
 * amortized cost for a sequence of m accesses to a splay tree is O(lg n);
 *
 * A rank-balanced tree is a binary search tree with an integer rank-difference
 * between itself and its children as an attribute of the node.
 * The sum of the rank-differences on any path from a node down to null is
 * the same, and defines the rank of that node.
 * Leaves have rank 0 and null nodes have rank -1.
 *
 * The following paper describes the rank balanced trees in detail:
 *   Haeupler, Sen and Tarjan, "Rank Balanced Trees",
 *   ACM Transactions on Algorithms Volume 11 Issue 4 June 2015
 *   Article No.: 30pp 1–26 https://doi.org/10.1145/2689412
 *
 * Weak AVL trees have the nice property that any insertion/deletion can cause
 * at most two rotations, which is an improvement over the standard AVL and
 * Red-Black trees.
 * As a comparison matrix:
 *   Tree type                      Weak AVL       AVL              Red-Black
 *   worst case height              2Log(N)        1.44Log(N)       2Log(N)
 *   height with only insertions    1.44Log(N)     1.44Log(N)       2Log(N)
 *   max rotations after insert     2              O(Log(N))        2
 *   max rotations after delete     2              2                3
 *
 * For each node we store the left, right and parent pointers.
 * We assume that a pointer to a struct is aligned to multiple of 4 bytes, which
 * leaves the last two bits free for our use. The last two bits of the parent
 * are used to store the rank difference between the node and its children.
 * The convention used here is that the rank difference = 1 + child bit.
 */

#define SPLAY_HEAD(name, type)						\
struct name {								\
	struct type *sph_root; /* root of the tree */			\
}

#define SPLAY_INITIALIZER(root)						\
	{ NULL }

#define SPLAY_INIT(root) do {						\
	(root)->sph_root = NULL;					\
} while (0)

#define SPLAY_ENTRY(type)						\
struct {								\
	struct type *spe_left; /* left element */			\
	struct type *spe_right; /* right element */			\
}

#define SPLAY_LEFT(elm, field)		(elm)->field.spe_left
#define SPLAY_RIGHT(elm, field)		(elm)->field.spe_right
#define SPLAY_ROOT(head)		(head)->sph_root
#define SPLAY_EMPTY(head)		(SPLAY_ROOT(head) == NULL)

/* SPLAY_ROTATE_{LEFT,RIGHT} expect that tmp hold SPLAY_{RIGHT,LEFT} */
#define SPLAY_ROTATE_RIGHT(head, tmp, field) do {			\
	SPLAY_LEFT((head)->sph_root, field) = SPLAY_RIGHT(tmp, field);	\
	SPLAY_RIGHT(tmp, field) = (head)->sph_root;			\
	(head)->sph_root = tmp;						\
} while (0)

#define SPLAY_ROTATE_LEFT(head, tmp, field) do {			\
	SPLAY_RIGHT((head)->sph_root, field) = SPLAY_LEFT(tmp, field);	\
	SPLAY_LEFT(tmp, field) = (head)->sph_root;			\
	(head)->sph_root = tmp;						\
} while (0)

#define SPLAY_LINKLEFT(head, tmp, field) do {				\
	SPLAY_LEFT(tmp, field) = (head)->sph_root;			\
	tmp = (head)->sph_root;						\
	(head)->sph_root = SPLAY_LEFT((head)->sph_root, field);		\
} while (0)

#define SPLAY_LINKRIGHT(head, tmp, field) do {				\
	SPLAY_RIGHT(tmp, field) = (head)->sph_root;			\
	tmp = (head)->sph_root;						\
	(head)->sph_root = SPLAY_RIGHT((head)->sph_root, field);	\
} while (0)

#define SPLAY_ASSEMBLE(head, node, left, right, field) do {		\
	SPLAY_RIGHT(left, field) = SPLAY_LEFT((head)->sph_root, field);	\
	SPLAY_LEFT(right, field) = SPLAY_RIGHT((head)->sph_root, field);\
	SPLAY_LEFT((head)->sph_root, field) = SPLAY_RIGHT(node, field);	\
	SPLAY_RIGHT((head)->sph_root, field) = SPLAY_LEFT(node, field);	\
} while (0)

/* Generates prototypes and inline functions */

#define SPLAY_PROTOTYPE(name, type, field, cmp)				\
void name##_SPLAY(struct name *, struct type *);			\
void name##_SPLAY_MINMAX(struct name *, int);				\
struct type *name##_SPLAY_INSERT(struct name *, struct type *);		\
struct type *name##_SPLAY_REMOVE(struct name *, struct type *);		\
									\
/* Finds the node with the same key as elm */				\
static __unused __inline struct type *					\
name##_SPLAY_FIND(struct name *head, struct type *elm)			\
{									\
	if (SPLAY_EMPTY(head))						\
		return(NULL);						\
	name##_SPLAY(head, elm);					\
	if ((cmp)(elm, (head)->sph_root) == 0)				\
		return (head->sph_root);				\
	return (NULL);							\
}									\
									\
static __unused __inline struct type *					\
name##_SPLAY_NEXT(struct name *head, struct type *elm)			\
{									\
	name##_SPLAY(head, elm);					\
	if (SPLAY_RIGHT(elm, field) != NULL) {				\
		elm = SPLAY_RIGHT(elm, field);				\
		while (SPLAY_LEFT(elm, field) != NULL) {		\
			elm = SPLAY_LEFT(elm, field);			\
		}							\
	} else								\
		elm = NULL;						\
	return (elm);							\
}									\
									\
static __unused __inline struct type *					\
name##_SPLAY_MIN_MAX(struct name *head, int val)			\
{									\
	name##_SPLAY_MINMAX(head, val);					\
        return (SPLAY_ROOT(head));					\
}

/* Main splay operation.
 * Moves node close to the key of elm to top
 */
#define SPLAY_GENERATE(name, type, field, cmp)				\
struct type *								\
name##_SPLAY_INSERT(struct name *head, struct type *elm)		\
{									\
    if (SPLAY_EMPTY(head)) {						\
	    SPLAY_LEFT(elm, field) = SPLAY_RIGHT(elm, field) = NULL;	\
    } else {								\
	    int __comp;							\
	    name##_SPLAY(head, elm);					\
	    __comp = (cmp)(elm, (head)->sph_root);			\
	    if(__comp < 0) {						\
		    SPLAY_LEFT(elm, field) = SPLAY_LEFT((head)->sph_root, field);\
		    SPLAY_RIGHT(elm, field) = (head)->sph_root;		\
		    SPLAY_LEFT((head)->sph_root, field) = NULL;		\
	    } else if (__comp > 0) {					\
		    SPLAY_RIGHT(elm, field) = SPLAY_RIGHT((head)->sph_root, field);\
		    SPLAY_LEFT(elm, field) = (head)->sph_root;		\
		    SPLAY_RIGHT((head)->sph_root, field) = NULL;	\
	    } else							\
		    return ((head)->sph_root);				\
    }									\
    (head)->sph_root = (elm);						\
    return (NULL);							\
}									\
									\
struct type *								\
name##_SPLAY_REMOVE(struct name *head, struct type *elm)		\
{									\
	struct type *__tmp;						\
	if (SPLAY_EMPTY(head))						\
		return (NULL);						\
	name##_SPLAY(head, elm);					\
	if ((cmp)(elm, (head)->sph_root) == 0) {			\
		if (SPLAY_LEFT((head)->sph_root, field) == NULL) {	\
			(head)->sph_root = SPLAY_RIGHT((head)->sph_root, field);\
		} else {						\
			__tmp = SPLAY_RIGHT((head)->sph_root, field);	\
			(head)->sph_root = SPLAY_LEFT((head)->sph_root, field);\
			name##_SPLAY(head, elm);			\
			SPLAY_RIGHT((head)->sph_root, field) = __tmp;	\
		}							\
		return (elm);						\
	}								\
	return (NULL);							\
}									\
									\
void									\
name##_SPLAY(struct name *head, struct type *elm)			\
{									\
	struct type __node, *__left, *__right, *__tmp;			\
	int __comp;							\
\
	SPLAY_LEFT(&__node, field) = SPLAY_RIGHT(&__node, field) = NULL;\
	__left = __right = &__node;					\
\
	while ((__comp = (cmp)(elm, (head)->sph_root))) {		\
		if (__comp < 0) {					\
			__tmp = SPLAY_LEFT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if ((cmp)(elm, __tmp) < 0){			\
				SPLAY_ROTATE_RIGHT(head, __tmp, field);	\
				if (SPLAY_LEFT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKLEFT(head, __right, field);		\
		} else if (__comp > 0) {				\
			__tmp = SPLAY_RIGHT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if ((cmp)(elm, __tmp) > 0){			\
				SPLAY_ROTATE_LEFT(head, __tmp, field);	\
				if (SPLAY_RIGHT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKRIGHT(head, __left, field);		\
		}							\
	}								\
	SPLAY_ASSEMBLE(head, &__node, __left, __right, field);		\
}									\
									\
/* Splay with either the minimum or the maximum element			\
 * Used to find minimum or maximum element in tree.			\
 */									\
void name##_SPLAY_MINMAX(struct name *head, int __comp) \
{									\
	struct type __node, *__left, *__right, *__tmp;			\
\
	SPLAY_LEFT(&__node, field) = SPLAY_RIGHT(&__node, field) = NULL;\
	__left = __right = &__node;					\
\
	while (1) {							\
		if (__comp < 0) {					\
			__tmp = SPLAY_LEFT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if (__comp < 0){				\
				SPLAY_ROTATE_RIGHT(head, __tmp, field);	\
				if (SPLAY_LEFT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKLEFT(head, __right, field);		\
		} else if (__comp > 0) {				\
			__tmp = SPLAY_RIGHT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if (__comp > 0) {				\
				SPLAY_ROTATE_LEFT(head, __tmp, field);	\
				if (SPLAY_RIGHT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKRIGHT(head, __left, field);		\
		}							\
	}								\
	SPLAY_ASSEMBLE(head, &__node, __left, __right, field);		\
}

#define SPLAY_NEGINF	-1
#define SPLAY_INF	1

#define SPLAY_INSERT(name, x, y)	name##_SPLAY_INSERT(x, y)
#define SPLAY_REMOVE(name, x, y)	name##_SPLAY_REMOVE(x, y)
#define SPLAY_FIND(name, x, y)		name##_SPLAY_FIND(x, y)
#define SPLAY_NEXT(name, x, y)		name##_SPLAY_NEXT(x, y)
#define SPLAY_MIN(name, x)		(SPLAY_EMPTY(x) ? NULL	\
					: name##_SPLAY_MIN_MAX(x, SPLAY_NEGINF))
#define SPLAY_MAX(name, x)		(SPLAY_EMPTY(x) ? NULL	\
					: name##_SPLAY_MIN_MAX(x, SPLAY_INF))

#define SPLAY_FOREACH(x, name, head)					\
	for ((x) = SPLAY_MIN(name, head);				\
	     (x) != NULL;						\
	     (x) = SPLAY_NEXT(name, head, x))

/* Macros that define a rank-balanced weak AVL tree */
/*
 * debug macros
 */
#ifdef DIAGNOSTIC
#ifdef _KERNEL
#define _RB_ASSERT(x)		KASSERT(x)
#else
#include <assert.h>
#define _RB_ASSERT(x)		assert(x)
#endif
#else
#define _RB_ASSERT(x)		do {} while (0)
#endif

/*
 * internal use macros
 */
#define _RB_LOWMASK					((uintptr_t)3U)
#define _RB_PTR(elm)					(__typeof(elm))((uintptr_t)(elm) & ~_RB_LOWMASK)

#define _RB_PDIR					((uintptr_t)0U)
#define _RB_LDIR					((uintptr_t)1U)
#define _RB_RDIR					((uintptr_t)2U)
#define _RB_ODIR(dir)					((dir) ^ 3U)

#define RB_ENTRY(type)					\
struct {						\
	/* parent, left, right */			\
	struct type *rbe_link[3];			\
}

#define RB_HEAD(name, type)				\
struct name {						\
	struct type *rbh_root;				\
}

#define RB_INITIALIZER(root)				\
{ NULL }


#define RB_INIT(head) do {				\
(head)->rbh_root = NULL;				\
} while (0)

#define RB_ROOT(head)					(head)->rbh_root
#define RB_EMPTY(head)					(RB_ROOT(head) == NULL)

/*
 * element macros
 */
#define _RB_GET_CHILD(elm, dir, field)			(elm)->field.rbe_link[dir]
#define _RB_SET_CHILD(elm, dir, celm, field) do {	\
_RB_GET_CHILD(elm, dir, field) = (celm);		\
} while (0)
#define _RB_UP(elm, field)				_RB_GET_CHILD(elm, _RB_PDIR, field)
#define _RB_REPLACE_PARENT(elm, pelm, field)		_RB_SET_CHILD(elm, _RB_PDIR, pelm, field)
#define _RB_COPY_PARENT(elm, nelm, field)		_RB_REPLACE_PARENT(nelm, _RB_UP(elm, field), field)


#define RB_SET_PARENT(elm, pelm, field) do {			\
_RB_UP(elm, field) = (__typeof(elm))(((uintptr_t)pelm) | 	\
	    (((uintptr_t)_RB_UP(elm, field)) & _RB_LOWMASK));	\
} while (0)
#define RB_PARENT(elm, field)				_RB_PTR(_RB_GET_CHILD(elm, _RB_PDIR, field))
#define RB_LEFT(elm, field)				_RB_GET_CHILD(elm, _RB_LDIR, field)
#define RB_RIGHT(elm, field)				_RB_GET_CHILD(elm, _RB_RDIR, field)


#define _RB_GET_RDIFF(elm, dir, field)			(((uintptr_t)_RB_UP(elm, field)) & dir)
#define _RB_GET_RDIFF2(elm, field)			(((uintptr_t)_RB_UP(elm, field)) & _RB_LOWMASK)
#define _RB_FLIP_RDIFF(elm, dir, field)			\
_RB_REPLACE_PARENT(elm, ((__typeof(elm))(((uintptr_t)_RB_UP(elm, field)) ^ dir)), field)
#define _RB_FLIP_RDIFF2(elm, field)			\
_RB_REPLACE_PARENT(elm, ((__typeof(elm))(((uintptr_t)_RB_UP(elm, field)) ^ _RB_LDIR ^ _RB_RDIR)), field)
#define _RB_SET_RDIFF0(elm, dir, field)			\
_RB_REPLACE_PARENT(elm, ((__typeof(elm))(((uintptr_t)_RB_UP(elm, field)) & ~dir)), field)
#define _RB_SET_RDIFF1(elm, dir, field)			\
_RB_REPLACE_PARENT(elm, ((__typeof(elm))(((uintptr_t)_RB_UP(elm, field)) | dir)), field)
#define _RB_SET_RDIFF11(elm, field)			\
_RB_REPLACE_PARENT(elm, ((__typeof(elm))(((uintptr_t)_RB_UP(elm, field)) | _RB_LDIR | _RB_RDIR)), field)
#define _RB_SET_RDIFF00(elm, field)			\
_RB_REPLACE_PARENT(elm, RB_PARENT(elm, field), field)


/*
 * RB_AUGMENT_CHECK should only return true when the update changes the node data,
 * so that updating can be stopped short of the root when it returns false.
 */
#ifndef RB_AUGMENT_CHECK
#ifndef RB_AUGMENT
#define RB_AUGMENT_CHECK(x) (0)
#else
#define RB_AUGMENT_CHECK(x) (RB_AUGMENT(x), 1)
#endif
#endif

#define _RB_AUGMENT_WALK(elm, match, field) do {	\
if (match == elm)					\
	match = NULL;					\
} while (RB_AUGMENT_CHECK(elm) &&			\
	(elm = RB_PARENT(elm, field)) != NULL)


/*
 *      elm            celm
 *      / \            / \
 *     c1  celm       elm gc2
 *         / \        / \
 *      gc1   gc2    c1 gc1
 */
#define _RB_ROTATE(elm, celm, dir, field) do {				\
if ((_RB_GET_CHILD(elm, _RB_ODIR(dir), field) = _RB_GET_CHILD(celm, dir, field)) != NULL)	\
	RB_SET_PARENT(_RB_GET_CHILD(celm, dir, field), elm, field);	\
_RB_SET_CHILD(celm, dir, elm, field);					\
RB_SET_PARENT(elm, celm, field);					\
} while (0)

#define _RB_SWAP_CHILD_OR_ROOT(head, elm, oelm, nelm, field) do {	\
if (elm == NULL)							\
	RB_ROOT(head) = nelm;						\
else									\
	_RB_SET_CHILD(elm, (RB_LEFT(elm, field) == (oelm) ? _RB_LDIR : _RB_RDIR), nelm, field);	\
} while (0)


#define _RB_GENERATE_RANK(name, type, field, cmp, attr)			\
attr int								\
name##_RB_RANK(const struct type *elm)					\
{									\
	int lrank, rrank;						\
	if (elm == NULL)						\
		return (-1);						\
	lrank =  name##_RB_RANK(RB_LEFT(elm, field));			\
	if (lrank == -2)						\
		return (-2);						\
	rrank =  name##_RB_RANK(RB_RIGHT(elm, field));			\
	if (rrank == -2)						\
		return (-2);						\
	lrank += (_RB_GET_RDIFF(elm, _RB_LDIR, field) == 0) ? 1 : 2;	\
	rrank += (_RB_GET_RDIFF(elm, _RB_RDIR, field) == 0) ? 1 : 2;	\
	if (lrank != rrank)						\
		return (-2);						\
	return (lrank);							\
}


#define _RB_GENERATE_MINMAX(name, type, field, cmp, attr)		\
									\
attr struct type *							\
name##_RB_MINMAX(struct name *head, int dir)				\
{									\
	struct type *tmp = RB_ROOT(head);				\
	struct type *parent = NULL;					\
	while (tmp) {							\
		parent = tmp;						\
		tmp = _RB_GET_CHILD(tmp, dir, field);			\
	}								\
	return (parent);						\
}


#define _RB_GENERATE_ITERATE(name, type, field, cmp, attr)		\
									\
attr struct type *							\
name##_RB_NEXT(struct type *elm)					\
{									\
	struct type *parent = NULL;					\
									\
	if (RB_RIGHT(elm, field)) {					\
		elm = RB_RIGHT(elm, field);				\
		while (RB_LEFT(elm, field))				\
			elm = RB_LEFT(elm, field);			\
	} else {							\
		parent = RB_PARENT(elm, field);				\
		while (parent && elm == RB_RIGHT(parent, field)) {	\
			elm = parent;					\
			parent = RB_PARENT(parent, field);		\
		}							\
		elm = parent;						\
	}								\
	return (elm);							\
}									\
									\
attr struct type *							\
name##_RB_PREV(struct type *elm)					\
{									\
	struct type *parent = NULL;					\
									\
	if (RB_LEFT(elm, field)) {					\
		elm = RB_LEFT(elm, field);				\
		while (RB_RIGHT(elm, field))				\
			elm = RB_RIGHT(elm, field);			\
	} else {							\
		parent = RB_PARENT(elm, field);				\
		while (parent && elm == RB_LEFT(parent, field)) {	\
			elm = parent;					\
			parent = RB_PARENT(parent, field);		\
		}							\
		elm = parent;						\
	}								\
	return (elm);							\
}


#define _RB_GENERATE_FIND(name, type, field, cmp, attr)			\
									\
attr struct type *							\
name##_RB_FIND(struct name *head, struct type *elm)			\
{									\
	struct type *tmp = RB_ROOT(head);				\
	__typeof(cmp(NULL, NULL)) comp;					\
	while (tmp) {							\
		comp = cmp(elm, tmp);					\
		if (comp < 0)						\
			tmp = RB_LEFT(tmp, field);			\
		else if (comp > 0)					\
			tmp = RB_RIGHT(tmp, field);			\
		else							\
			return (tmp);					\
	}								\
	return (NULL);							\
}									\
									\
attr struct type *							\
name##_RB_NFIND(struct name *head, struct type *elm)			\
{									\
	struct type *tmp = RB_ROOT(head);				\
	struct type *res = NULL;					\
	__typeof(cmp(NULL, NULL)) comp;					\
	while (tmp) {							\
		comp = cmp(elm, tmp);					\
		if (comp < 0) {						\
			res = tmp;					\
			tmp = RB_LEFT(tmp, field);			\
		}							\
		else if (comp > 0)					\
			tmp = RB_RIGHT(tmp, field);			\
		else							\
			return (tmp);					\
	}								\
	return (res);							\
}									\
									\
attr struct type *							\
name##_RB_PFIND(struct name *head, struct type *elm)			\
{									\
	struct type *tmp = RB_ROOT(head);				\
	struct type *res = NULL;					\
	__typeof(cmp(NULL, NULL)) comp;					\
	while (tmp) {							\
		comp = cmp(elm, tmp);					\
		if (comp > 0) {						\
			res = tmp;					\
			tmp = RB_RIGHT(tmp, field);			\
		}							\
		else if (comp < 0)					\
			tmp = RB_LEFT(tmp, field);			\
		else							\
			return (tmp);					\
	}								\
	return (res);							\
}


/*
 * When doing a balancing of the tree after a removal, lets check
 * when we are looking at the edge 'elm' to its 'parent'.
 * For now, assume that 'elm' has already been demoted.
 * Now there are two possibilities:
 * 1) if 'elm' has rank difference 2 with 'parent', or it is
 *    the root node, we are done.
 * 2) if 'elm' has rank difference 3 with 'parent', we have a
 *    few cases:
 *
 * 2.1) the sibling of 'elm' has rank difference 2 with 'parent'
 *      then we demote the 'parent'. And continue recursively from
 *      parent.
 *
 *                gpar                          gpar
 *                 /                             /
 *               1/2                           2/3
 *               /                             /
 *          parent           -->              /
 *          /    \                         parent
 *         3      2                       /      \ 1
 *        /        \                     2        \
 *       /         sibling              /        sibling
 *     elm            /\              elm           /\
 *      /\            --               /\           --
 *      --                             --
 *
 * 2.2) the sibling of 'elm' has rank difference 1 with 'parent', then
 *      we need to do rotations, based on the children of the
 *      sibling of elm.
 *
 * 2.2a) Both children of sibling have rdiff 2. Then we demote both
 *       parent and sibling and continue recursively from parent.
 *
 *                gpar                          gpar
 *                 /                             /
 *               1/2                           2/3
 *               /                             /
 *          parent           -->           parent
 *          /    \                         /    \
 *         3      1                       2      1
 *        /        \                     /        \
 *      elm         sibling            elm         sibling
 *      /\          2/   \2             /\         1/   \1
 *      --          c     d             --         c     d
 *
 *
 *  2.2b) the rdiff 1 child of 'sibling', 'c', is in the same
 *        direction as 'sibling' is wrt to 'parent'. We do a single
 *        rotation (with rank changes) and we are done.
 *
 *            gpar                    gpar                        gpar
 *             /                       /                           /
 *           1/2                     1/2      if                 1/2
 *           /                       /    parent->c == 2         /
 *      parent        -->       sibling      -->            sibling
 *      /    \                  1/    \                     2/    \
 *     3      1               parent   2                 parent    2
 *    /        \              2/   \    \                1/   \1    \
 *  elm         sibling      elm    c    d              elm    c     d
 *  /\           /   \1      /\                          /\
 *  --          c     d      --                          --
 *
 * 2.2c) the rdiff 1 child of 'sibling', 'c', is in the opposite
 *      direction as 'sibling' is wrt to 'parent'. We do a double
 *      rotation (with rank changes) and we are done.
 *
 *                gpar                          gpar
 *                 /                             /
 *               1/2                           1/2
 *               /                             /
 *          parent           -->              c
 *          /    \                          2/ \2
 *         3      1                     parent  sibling
 *        /        \                   1/   \    /   \1
 *      elm         sibling           elm   c1  c2    d
 *      /\          1/   \2           /\
 *      --          c     d           --
 *                 / \
 *                c1  c2
 *
 */
#define _RB_GENERATE_REMOVE(name, type, field, cmp, attr)		\
									\
attr struct type *							\
name##_RB_REMOVE_BALANCE(struct name *head, struct type *parent,	\
    struct type *elm)							\
{									\
	struct type *gpar, *sibling;					\
	uintptr_t elmdir, sibdir, ssdiff, sodiff;			\
									\
	gpar = NULL;							\
	sibling = NULL;							\
	if (RB_RIGHT(parent, field) == NULL && RB_LEFT(parent, field) == NULL) {	\
		_RB_SET_RDIFF00(parent, field);				\
		elm = parent;						\
		if ((parent = RB_PARENT(elm, field)) == NULL) {		\
			return (NULL);					\
		}							\
	}								\
	do {								\
		_RB_ASSERT(parent != NULL);				\
		gpar = RB_PARENT(parent, field);			\
		elmdir = RB_LEFT(parent, field) == elm ? _RB_LDIR : _RB_RDIR;	\
		if (_RB_GET_RDIFF(parent, elmdir, field) == 0) {	\
			/* case (1) */					\
			_RB_FLIP_RDIFF(parent, elmdir, field);		\
			return (NULL);					\
		}							\
		/* case 2 */						\
		sibdir = _RB_ODIR(elmdir);				\
		if (_RB_GET_RDIFF(parent, sibdir, field)) {		\
			/* case 2.1 */					\
			_RB_FLIP_RDIFF(parent, sibdir, field);		\
			continue;					\
		}							\
		/* case 2.2 */						\
		sibling = _RB_GET_CHILD(parent, sibdir, field);		\
		_RB_ASSERT(sibling != NULL);				\
		ssdiff = _RB_GET_RDIFF(sibling, elmdir, field);		\
		sodiff = _RB_GET_RDIFF(sibling, sibdir, field);		\
		_RB_FLIP_RDIFF(sibling, sibdir, field);			\
		if (ssdiff && sodiff) {					\
			/* case 2.2a */					\
			_RB_FLIP_RDIFF(sibling, elmdir, field);		\
			continue;					\
		}							\
		if (sodiff) {						\
			/* case 2.2c */					\
			elm = _RB_GET_CHILD(sibling, elmdir, field);	\
			_RB_ROTATE(sibling, elm, sibdir, field);	\
			_RB_FLIP_RDIFF(parent, elmdir, field);		\
			if (_RB_GET_RDIFF(elm, elmdir, field))		\
				_RB_FLIP_RDIFF(parent, sibdir, field);	\
			if (_RB_GET_RDIFF(elm, sibdir, field))		\
				_RB_FLIP_RDIFF(sibling, elmdir, field);	\
			_RB_SET_RDIFF11(elm, field);			\
		} else {						\
			/* case 2.2b */					\
			if (ssdiff) {					\
				_RB_SET_RDIFF11(sibling, field);	\
				_RB_SET_RDIFF00(parent, field);		\
			}						\
			elm = sibling;					\
		}							\
		_RB_ROTATE(parent, elm, elmdir, field);			\
		RB_SET_PARENT(elm, gpar, field);			\
		_RB_SWAP_CHILD_OR_ROOT(head, gpar, parent, elm, field);	\
		if (elm != sibling)					\
			(void)RB_AUGMENT_CHECK(sibling);		\
		return (elm);						\
	} while ((elm = parent, (parent = gpar) != NULL));		\
	return (elm);							\
}									\
									\
attr struct type *							\
name##_RB_REMOVE(struct name *head, struct type *elm)			\
{									\
	struct type *parent, *opar, *child, *rmin;			\
									\
	/* first find the element to swap with elm */			\
	child = RB_LEFT(elm, field);					\
	rmin = RB_RIGHT(elm, field);					\
	if (rmin == NULL || child == NULL) {				\
		rmin = child = (rmin == NULL ? child : rmin);		\
		parent = opar = RB_PARENT(elm, field);			\
	}								\
	else {								\
		parent = rmin;						\
		while (RB_LEFT(rmin, field)) {				\
			rmin = RB_LEFT(rmin, field);			\
		}							\
		RB_SET_PARENT(child, rmin, field);			\
		_RB_SET_CHILD(rmin, _RB_LDIR, child, field);		\
		child = RB_RIGHT(rmin, field);				\
		if (parent != rmin) {					\
			RB_SET_PARENT(parent, rmin, field);		\
			_RB_SET_CHILD(rmin, _RB_RDIR, parent, field);	\
			parent = RB_PARENT(rmin, field);		\
			_RB_SET_CHILD(parent, _RB_LDIR, child, field);	\
		}							\
		_RB_COPY_PARENT(elm, rmin, field);			\
		opar = RB_PARENT(elm, field);				\
	}								\
	_RB_SWAP_CHILD_OR_ROOT(head, opar, elm, rmin, field);		\
	if (child != NULL) {						\
		_RB_REPLACE_PARENT(child, parent, field);		\
	}								\
	if (parent != NULL) {						\
		opar = name##_RB_REMOVE_BALANCE(head, parent, child);	\
		if (parent == rmin && RB_LEFT(parent, field) == NULL) {	\
			opar = NULL;					\
			parent = RB_PARENT(parent, field);		\
		}							\
		_RB_AUGMENT_WALK(parent, opar, field);			\
		if (opar != NULL) {					\
			(void)RB_AUGMENT_CHECK(opar);			\
			(void)RB_AUGMENT_CHECK(RB_PARENT(opar, field));	\
		}							\
	}								\
	return (elm);							\
}


/*
 * When doing a balancing of the tree, lets check when we are looking
 * at the edge 'elm' to its 'parent'.
 * We assume that 'elm' has already been promoted.
 * Now there are two possibilities:
 * 1) if 'elm' has rank difference 1 with 'parent', or it is the root
 *    node, we are done.
 * 2) if 'elm' has rank difference 0 with 'parent', we have a few cases:
 *
 * 2.1) the sibling of 'elm' has rank difference 1 with 'parent' then
 *      we promote the 'parent'. And continue recursively from parent.
 *
 *                gpar                          gpar
 *                 /                             /(0/1)
 *               1/2                          parent
 *               /                            1/  \
 *   elm --0-- parent           -->         elm    2
 *    /\          \                          /\     \
 *    --           1                         --    sibling
 *                  \                                /\
 *                   sibling                         --
 *                   /\
 *                   --
 *
 * 2.2) the sibling of 'elm' has rank difference 2 with 'parent',
 *      then we need to do rotations based on the child of 'elm'
 *      that has rank difference 1 with 'elm'.
 *      there will always be one such child as 'elm' had to be promoted.
 *
 * 2.2a) the rdiff 1 child of 'elm', 'c', is in the same direction
 *       as 'elm' is wrt to 'parent'. We demote the parent and do
 *       a single rotation in the opposite direction and we are done.
 *
 *                       gpar                         gpar
 *                        /                            /
 *                      1/2                          1/2
 *                      /                            /
 *          elm --0-- parent           -->         elm
 *          / \          \                         / \
 *         1   2          2                       1   1
 *        /     \          \                     /     \
 *       c       d         sibling              c      parent
 *      /\       /\          /\                /\        / \
 *      --       --          --                --       1   1
 *                                                     /     \
 *                                                    d    sibling
 *                                                   /\      /\
 *                                                   --      --
 *
 *  2.2b) the rdiff 1 child of 'elm', 'c', is in the opposite
 *        direction as 'elm' is wrt to 'parent'. We do a double
 *        rotation (with rank changes) and we are done.
 *
 *                       gpar                         gpar
 *                        /                            /
 *                      1/2                          1/2
 *                      /                            /
 *          elm --0-- parent           -->          c
 *          / \          \                        1/ \1
 *         2   1          2                     elm   parent
 *        /     \          \                  1/  \      /  \1
 *       d       c         sibling            d    c1  c2  sibling
 *      /\      / \          /\              /\    /\  /\    /\
 *      --     c1 c2         --              --    --  --    --
 */
#define _RB_GENERATE_INSERT(name, type, field, cmp, attr)		\
									\
attr struct type *							\
name##_RB_INSERT_BALANCE(struct name *head, struct type *elm)		\
{									\
	struct type *child, *gpar, *parent;				\
	uintptr_t elmdir, sibdir;					\
									\
	child = NULL;							\
	gpar = NULL;							\
	parent = RB_PARENT(elm, field);					\
	if (!parent) {							\
		/* even though the parent check is already done by	\
		 * insert_finish, we need to keep this here as well for	\
		 * the linux compat layer, as it calls this function	\
		 */							\
		return (NULL);						\
	}								\
	do {								\
		/* elm has not been promoted yet */			\
		elmdir = RB_LEFT(parent, field) == elm ? _RB_LDIR : _RB_RDIR;	\
		if (_RB_GET_RDIFF(parent, elmdir, field)) {		\
			/* case (1) */					\
			_RB_FLIP_RDIFF(parent, elmdir, field);		\
			return (elm);					\
		}							\
		/* case (2) */						\
		gpar = RB_PARENT(parent, field);			\
		sibdir = _RB_ODIR(elmdir);				\
		_RB_FLIP_RDIFF(parent, sibdir, field);			\
		if (_RB_GET_RDIFF(parent, sibdir, field)) {		\
			/* case (2.1) */				\
			child = elm;					\
			elm = parent;					\
			continue;					\
		}							\
		/* we can only reach this point if we are in the	\
		 * second or greater iteration of the while loop	\
		 * which means that 'child' has been populated		\
		 */							\
		_RB_SET_RDIFF00(parent, field);				\
		/* case (2.2) */					\
		if (_RB_GET_RDIFF(elm, sibdir, field) == 0) {		\
			/* case (2.2b) */				\
			_RB_ROTATE(elm, child, elmdir, field);		\
			if (_RB_GET_RDIFF(child, sibdir, field))	\
				_RB_FLIP_RDIFF(parent, elmdir, field);	\
			if (_RB_GET_RDIFF(child, elmdir, field))	\
				_RB_FLIP_RDIFF2(elm, field);		\
			else						\
				_RB_FLIP_RDIFF(elm, elmdir, field);	\
			if (_RB_GET_RDIFF2(child, field) == 0)		\
				elm = child;				\
		} else {						\
			/* case (2.2a) */				\
			child = elm;					\
		}							\
		_RB_ROTATE(parent, child, sibdir, field);		\
		_RB_REPLACE_PARENT(child, gpar, field);			\
		_RB_SWAP_CHILD_OR_ROOT(head, gpar, parent, child, field);	\
		if (elm != child)					\
			(void)RB_AUGMENT_CHECK(elm);			\
		(void)RB_AUGMENT_CHECK(parent);				\
		return (child);						\
	} while ((parent = gpar) != NULL);				\
	return (NULL);							\
}									\
									\
/* Inserts a node into the RB tree */					\
attr struct type *							\
name##_RB_INSERT_FINISH(struct name *head, struct type *parent,		\
    struct type **tmpp, struct type *elm)				\
{									\
	struct type *tmp = elm;						\
	_RB_SET_CHILD(elm, _RB_LDIR, NULL, field);			\
	_RB_SET_CHILD(elm, _RB_RDIR, NULL, field);			\
	_RB_REPLACE_PARENT(elm, parent, field);				\
	*tmpp = elm;							\
	if (parent != NULL)						\
		tmp = name##_RB_INSERT_BALANCE(head, elm);		\
	_RB_AUGMENT_WALK(elm, tmp, field);				\
	if (tmp != NULL)						\
		(void)RB_AUGMENT_CHECK(tmp);				\
	return (NULL);							\
}									\
									\
/* Inserts a node into the RB tree */					\
attr struct type *							\
name##_RB_INSERT(struct name *head, struct type *elm)			\
{									\
	struct type *tmp;						\
	struct type *parent=NULL;					\
	struct type **tmpp=&RB_ROOT(head);				\
	__typeof(cmp(NULL, NULL)) comp;					\
									\
	while ((tmp = *tmpp) != NULL) {					\
		parent = tmp;						\
		comp = cmp(elm, tmp);					\
		if (comp < 0) {						\
			tmpp = &RB_LEFT(tmp, field);			\
		}							\
		else if (comp > 0) {					\
			tmpp = &RB_RIGHT(tmp, field);			\
		}							\
		else							\
			return (parent);				\
	}								\
	return (name##_RB_INSERT_FINISH(head, parent, tmpp, elm));	\
}									\
									\
attr struct type *							\
name##_RB_INSERT_NEXT(struct name *head, struct type *elm, struct type *next)	\
{									\
	struct type *tmp;						\
	struct type **tmpp;						\
	_RB_ASSERT((cmp)(elm, next) < 0);				\
	if (name##_RB_NEXT(elm) != NULL)				\
		_RB_ASSERT((cmp)(next, name##_RB_NEXT(elm)) < 0);	\
	tmpp = &RB_RIGHT(elm, field);					\
	while ((tmp = *tmpp) != NULL) {					\
		elm = tmp;						\
		tmpp = &RB_LEFT(tmp, field);				\
	}								\
	return name##_RB_INSERT_FINISH(head, elm, tmpp, next);		\
}									\
									\
attr struct type *							\
name##_RB_INSERT_PREV(struct name *head, struct type *elm, struct type *prev)	\
{									\
	struct type *tmp;						\
	struct type **tmpp;						\
	_RB_ASSERT((cmp)(elm, prev) > 0);				\
	if (name##_RB_PREV(elm) != NULL)				\
		_RB_ASSERT((cmp)(prev, name##_RB_PREV(elm)) > 0);	\
									\
	tmpp = &RB_LEFT(elm, field);					\
	while ((tmp = *tmpp) != NULL) {					\
		elm = tmp;						\
		tmpp = &RB_RIGHT(tmp, field);				\
	}								\
	return name##_RB_INSERT_FINISH(head, elm, tmpp, prev);		\
}


#define RB_GENERATE(name, type, field, cmp)				\
	_RB_GENERATE_INTERNAL(name, type, field, cmp,)

#define RB_GENERATE_STATIC(name, type, field, cmp)			\
	_RB_GENERATE_INTERNAL(name, type, field, cmp, __attribute__((__unused__)) static)

#define _RB_GENERATE_INTERNAL(name, type, field, cmp, attr)		\
	_RB_GENERATE_RANK(name, type, field, cmp, attr)			\
	_RB_GENERATE_MINMAX(name, type, field, cmp, attr)		\
	_RB_GENERATE_ITERATE(name, type, field, cmp, attr)		\
	_RB_GENERATE_FIND(name, type, field, cmp, attr)			\
	_RB_GENERATE_REMOVE(name, type, field, cmp, attr)		\
	_RB_GENERATE_INSERT(name, type, field, cmp, attr)


#define RB_PROTOTYPE(name, type, field, cmp)				\
	_RB_PROTOTYPE_INTERNAL(name, type, field, cmp,)

#define RB_PROTOTYPE_STATIC(name, type, field, cmp)			\
	_RB_PROTOTYPE_INTERNAL(name, type, field, cmp, __attribute__((__unused__)) static)

#define _RB_PROTOTYPE_INTERNAL(name, type, field, cmp, attr)		\
attr int		 name##_RB_RANK(const struct type *);		\
attr struct type	*name##_RB_MINMAX(struct name *, int);		\
attr struct type	*name##_RB_NEXT(struct type *);			\
attr struct type	*name##_RB_PREV(struct type *);			\
attr struct type	*name##_RB_FIND(struct name *, struct type *);	\
attr struct type	*name##_RB_NFIND(struct name *, struct type *);	\
attr struct type	*name##_RB_PFIND(struct name *, struct type *);	\
attr struct type 	*name##_RB_REMOVE_BALANCE(struct name *, struct type *, struct type *);	\
attr struct type	*name##_RB_REMOVE(struct name *, struct type *);	\
attr struct type	*name##_RB_INSERT_FINISH(struct name *, struct type *, struct type **, struct type *);	\
attr struct type	*name##_RB_INSERT_BALANCE(struct name *, struct type *);	\
attr struct type	*name##_RB_INSERT(struct name *, struct type *);	\
attr struct type	*name##_RB_INSERT_NEXT(struct name *, struct type *, struct type *);	\
attr struct type	*name##_RB_INSERT_PREV(struct name *, struct type *, struct type *);


#define RB_RANK(name, head)			name##_RB_RANK(head)
#define RB_MIN(name, head)			name##_RB_MINMAX(head, _RB_LDIR)
#define RB_MAX(name, head)			name##_RB_MINMAX(head, _RB_RDIR)
#define RB_NEXT(name, head, elm)		name##_RB_NEXT(elm)
#define RB_PREV(name, head, elm)		name##_RB_PREV(elm)
#define RB_FIND(name, head, elm)		name##_RB_FIND(head, elm)
#define RB_NFIND(name, head, elm)		name##_RB_NFIND(head, elm)
#define RB_PFIND(name, head, elm)		name##_RB_PFIND(head, elm)
#define RB_REMOVE(name, head, elm)		name##_RB_REMOVE(head, elm)
#define RB_INSERT(name, head, elm)		name##_RB_INSERT(head, elm)
#define RB_INSERT_NEXT(name, head, elm, next)	name##_RB_INSERT_NEXT(head, elm, next)
#define RB_INSERT_PREV(name, head, elm, prev)	name##_RB_INSERT_PREV(head, elm, prev)


#define RB_FOREACH(x, name, head)					\
	for ((x) = RB_MIN(name, head);					\
	     (x) != NULL;						\
	     (x) = name##_RB_NEXT(x))

#define RB_FOREACH_FROM(x, name, y)					\
	for ((x) = (y);							\
	    ((x) != NULL) && ((y) = name##_RB_NEXT(x), (x) != NULL);	\
	     (x) = (y))

#define RB_FOREACH_SAFE(x, name, head, y)				\
	for ((x) = RB_MIN(name, head);					\
	    ((x) != NULL) && ((y) = name##_RB_NEXT(x), (x) != NULL);	\
	     (x) = (y))

#define RB_FOREACH_REVERSE(x, name, head)				\
	for ((x) = RB_MAX(name, head);					\
	     (x) != NULL;						\
	     (x) = name##_RB_PREV(x))

#define RB_FOREACH_REVERSE_FROM(x, name, y)				\
	for ((x) = (y);							\
	    ((x) != NULL) && ((y) = name##_RB_PREV(x), (x) != NULL);	\
	     (x) = (y))

#define RB_FOREACH_REVERSE_SAFE(x, name, head, y)			\
	for ((x) = RB_MAX(name, head);					\
	    ((x) != NULL) && ((y) = name##_RB_PREV(x), (x) != NULL);	\
	     (x) = (y))

/*
 * Copyright (c) 2016 David Gwynne <dlg@openbsd.org>
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

#define RB_BLACK	0
#define RB_RED		1

struct rb_type {
	int		(*t_compare)(const void *, const void *);
	void		(*t_augment)(void *);
	unsigned int	  t_offset;	/* offset of rb_entry in type */
};

struct rb_tree {
	struct rb_entry	*rbt_root;
};

struct rb_entry {
	struct rb_entry	 *rbt_parent;
	struct rb_entry	 *rbt_left;
	struct rb_entry	 *rbt_right;
	unsigned int	  rbt_color;
};

#define RBT_HEAD(_name, _type)						\
struct _name {								\
	struct rb_tree rbh_root;					\
}

#define RBT_ENTRY(_type)	struct rb_entry

static inline void
_rb_init(struct rb_tree *rbt)
{
	rbt->rbt_root = NULL;
}

static inline int
_rb_empty(struct rb_tree *rbt)
{
	return (rbt->rbt_root == NULL);
}

void	*_rb_insert(const struct rb_type *, struct rb_tree *, void *);
void	*_rb_remove(const struct rb_type *, struct rb_tree *, void *);
void	*_rb_find(const struct rb_type *, struct rb_tree *, const void *);
void	*_rb_nfind(const struct rb_type *, struct rb_tree *, const void *);
void	*_rb_root(const struct rb_type *, struct rb_tree *);
void	*_rb_min(const struct rb_type *, struct rb_tree *);
void	*_rb_max(const struct rb_type *, struct rb_tree *);
void	*_rb_next(const struct rb_type *, void *);
void	*_rb_prev(const struct rb_type *, void *);
void	*_rb_left(const struct rb_type *, void *);
void	*_rb_right(const struct rb_type *, void *);
void	*_rb_parent(const struct rb_type *, void *);
void	 _rb_set_left(const struct rb_type *, void *, void *);
void	 _rb_set_right(const struct rb_type *, void *, void *);
void	 _rb_set_parent(const struct rb_type *, void *, void *);
void	 _rb_poison(const struct rb_type *, void *, unsigned long);
int	 _rb_check(const struct rb_type *, void *, unsigned long);

#define RBT_INITIALIZER(_head)	{ { NULL } }

#define RBT_PROTOTYPE(_name, _type, _field, _cmp)			\
extern const struct rb_type *const _name##_RBT_TYPE;			\
									\
__unused static inline void						\
_name##_RBT_INIT(struct _name *head)					\
{									\
	_rb_init(&head->rbh_root);					\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_INSERT(struct _name *head, struct _type *elm)		\
{									\
	return _rb_insert(_name##_RBT_TYPE, &head->rbh_root, elm);	\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_REMOVE(struct _name *head, struct _type *elm)		\
{									\
	return _rb_remove(_name##_RBT_TYPE, &head->rbh_root, elm);	\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_FIND(struct _name *head, const struct _type *key)		\
{									\
	return _rb_find(_name##_RBT_TYPE, &head->rbh_root, key);	\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_NFIND(struct _name *head, const struct _type *key)		\
{									\
	return _rb_nfind(_name##_RBT_TYPE, &head->rbh_root, key);	\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_ROOT(struct _name *head)					\
{									\
	return _rb_root(_name##_RBT_TYPE, &head->rbh_root);		\
}									\
									\
__unused static inline int						\
_name##_RBT_EMPTY(struct _name *head)					\
{									\
	return _rb_empty(&head->rbh_root);				\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_MIN(struct _name *head)					\
{									\
	return _rb_min(_name##_RBT_TYPE, &head->rbh_root);		\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_MAX(struct _name *head)					\
{									\
	return _rb_max(_name##_RBT_TYPE, &head->rbh_root);		\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_NEXT(struct _type *elm)					\
{									\
	return _rb_next(_name##_RBT_TYPE, elm);				\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_PREV(struct _type *elm)					\
{									\
	return _rb_prev(_name##_RBT_TYPE, elm);				\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_LEFT(struct _type *elm)					\
{									\
	return _rb_left(_name##_RBT_TYPE, elm);				\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_RIGHT(struct _type *elm)					\
{									\
	return _rb_right(_name##_RBT_TYPE, elm);			\
}									\
									\
__unused static inline struct _type *					\
_name##_RBT_PARENT(struct _type *elm)					\
{									\
	return _rb_parent(_name##_RBT_TYPE, elm);			\
}									\
									\
__unused static inline void						\
_name##_RBT_SET_LEFT(struct _type *elm, struct _type *left)		\
{									\
	_rb_set_left(_name##_RBT_TYPE, elm, left);			\
}									\
									\
__unused static inline void						\
_name##_RBT_SET_RIGHT(struct _type *elm, struct _type *right)		\
{									\
	_rb_set_right(_name##_RBT_TYPE, elm, right);			\
}									\
									\
__unused static inline void						\
_name##_RBT_SET_PARENT(struct _type *elm, struct _type *parent)		\
{									\
	_rb_set_parent(_name##_RBT_TYPE, elm, parent);			\
}									\
									\
__unused static inline void						\
_name##_RBT_POISON(struct _type *elm, unsigned long poison)		\
{									\
	_rb_poison(_name##_RBT_TYPE, elm, poison);			\
}									\
									\
__unused static inline int						\
_name##_RBT_CHECK(struct _type *elm, unsigned long poison)		\
{									\
	return _rb_check(_name##_RBT_TYPE, elm, poison);		\
}

#define RBT_GENERATE_INTERNAL(_name, _type, _field, _cmp, _aug)		\
static int								\
_name##_RBT_COMPARE(const void *lptr, const void *rptr)			\
{									\
	const struct _type *l = lptr, *r = rptr;			\
	return _cmp(l, r);						\
}									\
static const struct rb_type _name##_RBT_INFO = {			\
	_name##_RBT_COMPARE,						\
	_aug,								\
	offsetof(struct _type, _field),					\
};									\
const struct rb_type *const _name##_RBT_TYPE = &_name##_RBT_INFO

#define RBT_GENERATE_AUGMENT(_name, _type, _field, _cmp, _aug)		\
static void								\
_name##_RBT_AUGMENT(void *ptr)						\
{									\
	struct _type *p = ptr;						\
	return _aug(p);							\
}									\
RBT_GENERATE_INTERNAL(_name, _type, _field, _cmp, _name##_RBT_AUGMENT)

#define RBT_GENERATE(_name, _type, _field, _cmp)			\
    RBT_GENERATE_INTERNAL(_name, _type, _field, _cmp, NULL)

#define RBT_INIT(_name, _head)		_name##_RBT_INIT(_head)
#define RBT_INSERT(_name, _head, _elm)	_name##_RBT_INSERT(_head, _elm)
#define RBT_REMOVE(_name, _head, _elm)	_name##_RBT_REMOVE(_head, _elm)
#define RBT_FIND(_name, _head, _key)	_name##_RBT_FIND(_head, _key)
#define RBT_NFIND(_name, _head, _key)	_name##_RBT_NFIND(_head, _key)
#define RBT_ROOT(_name, _head)		_name##_RBT_ROOT(_head)
#define RBT_EMPTY(_name, _head)		_name##_RBT_EMPTY(_head)
#define RBT_MIN(_name, _head)		_name##_RBT_MIN(_head)
#define RBT_MAX(_name, _head)		_name##_RBT_MAX(_head)
#define RBT_NEXT(_name, _elm)		_name##_RBT_NEXT(_elm)
#define RBT_PREV(_name, _elm)		_name##_RBT_PREV(_elm)
#define RBT_LEFT(_name, _elm)		_name##_RBT_LEFT(_elm)
#define RBT_RIGHT(_name, _elm)		_name##_RBT_RIGHT(_elm)
#define RBT_PARENT(_name, _elm)		_name##_RBT_PARENT(_elm)
#define RBT_SET_LEFT(_name, _elm, _l)	_name##_RBT_SET_LEFT(_elm, _l)
#define RBT_SET_RIGHT(_name, _elm, _r)	_name##_RBT_SET_RIGHT(_elm, _r)
#define RBT_SET_PARENT(_name, _elm, _p)	_name##_RBT_SET_PARENT(_elm, _p)
#define RBT_POISON(_name, _elm, _p)	_name##_RBT_POISON(_elm, _p)
#define RBT_CHECK(_name, _elm, _p)	_name##_RBT_CHECK(_elm, _p)

#define RBT_FOREACH(_e, _name, _head)					\
	for ((_e) = RBT_MIN(_name, (_head));				\
	     (_e) != NULL;						\
	     (_e) = RBT_NEXT(_name, (_e)))

#define RBT_FOREACH_SAFE(_e, _name, _head, _n)				\
	for ((_e) = RBT_MIN(_name, (_head));				\
	     (_e) != NULL && ((_n) = RBT_NEXT(_name, (_e)), 1);	\
	     (_e) = (_n))

#define RBT_FOREACH_REVERSE(_e, _name, _head)				\
	for ((_e) = RBT_MAX(_name, (_head));				\
	     (_e) != NULL;						\
	     (_e) = RBT_PREV(_name, (_e)))

#define RBT_FOREACH_REVERSE_SAFE(_e, _name, _head, _n)			\
	for ((_e) = RBT_MAX(_name, (_head));				\
	     (_e) != NULL && ((_n) = RBT_PREV(_name, (_e)), 1);	\
	     (_e) = (_n))

#endif	/* _SYS_TREE_H_ */
