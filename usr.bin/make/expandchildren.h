#ifndef EXPANDCHILDREN_H
#define EXPANDCHILDREN_H
/*	$OpenBSD: expandchildren.h,v 1.1 2020/01/13 14:05:21 espie Exp $ */

extern void LinkParent(GNode *, GNode *);

/* partial expansion of children. */
extern void expand_children_from(GNode *, LstNode);
/* expand_all_children(gn):
 *	figure out all variable/wildcards expansions in gn.
 *	TODO pretty sure this is independent from the main suff module.
 */
#define expand_all_children(gn)	\
    expand_children_from(gn, Lst_First(&(gn)->children))

#endif
