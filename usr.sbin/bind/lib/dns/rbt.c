/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: rbt.c,v 1.115 2001/06/04 19:33:05 tale Exp $ */

/* Principal Authors: DCL */

#include <config.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/util.h>

/*
 * This define is so dns/name.h (included by dns/fixedname.h) uses more
 * efficient macro calls instead of functions for a few operations.
 */
#define DNS_NAME_USEINLINE 1

#include <dns/fixedname.h>
#include <dns/rbt.h>
#include <dns/result.h>

#define RBT_MAGIC		ISC_MAGIC('R', 'B', 'T', '+')
#define VALID_RBT(rbt)		ISC_MAGIC_VALID(rbt, RBT_MAGIC)

/*
 * XXXDCL Since parent pointers were added in again, I could remove all of the
 * chain junk, and replace with dns_rbt_firstnode, _previousnode, _nextnode,
 * _lastnode.  This would involve pretty major change to the API.
 */
#define CHAIN_MAGIC		ISC_MAGIC('0', '-', '0', '-')
#define VALID_CHAIN(chain)	ISC_MAGIC_VALID(chain, CHAIN_MAGIC)

#define RBT_HASH_SIZE		64

#ifdef RBT_MEM_TEST
#undef RBT_HASH_SIZE
#define RBT_HASH_SIZE 2	/* To give the reallocation code a workout. */
#endif

struct dns_rbt {
	unsigned int		magic;
	isc_mem_t *		mctx;
	dns_rbtnode_t *		root;
	void			(*data_deleter)(void *, void *);
	void *			deleter_arg;
	unsigned int		nodecount;
	unsigned int		hashsize;
	dns_rbtnode_t **	hashtable;
};

#define RED 0
#define BLACK 1

/*
 * Elements of the rbtnode structure.
 */
#define PARENT(node)		((node)->parent)
#define LEFT(node)	 	((node)->left)
#define RIGHT(node)		((node)->right)
#define DOWN(node)		((node)->down)
#define DATA(node)		((node)->data)
#define HASHNEXT(node)		((node)->hashnext)
#define HASHVAL(node)		((node)->hashval)
#define COLOR(node) 		((node)->color)
#define NAMELEN(node)		((node)->namelen)
#define OFFSETLEN(node)		((node)->offsetlen)
#define ATTRS(node)		((node)->attributes)
#define PADBYTES(node)		((node)->padbytes)
#define IS_ROOT(node)		ISC_TF((node)->is_root == 1)
#define FINDCALLBACK(node)	ISC_TF((node)->find_callback == 1)

/*
 * Structure elements from the rbtdb.c, not
 * used as part of the rbt.c algorithms.
 */
#define DIRTY(node)	((node)->dirty)
#define WILD(node)	((node)->wild)
#define LOCKNUM(node)	((node)->locknum)
#define REFS(node)	((node)->references)

/*
 * The variable length stuff stored after the node.
 */
#define NAME(node)	((unsigned char *)((node) + 1))
#define OFFSETS(node)	(NAME(node) + NAMELEN(node))

#define NODE_SIZE(node)	(sizeof(*node) + \
			 NAMELEN(node) + OFFSETLEN(node) + PADBYTES(node))

/*
 * Color management.
 */
#define IS_RED(node)		((node) != NULL && (node)->color == RED)
#define IS_BLACK(node)		((node) == NULL || (node)->color == BLACK)
#define MAKE_RED(node)		((node)->color = RED)
#define MAKE_BLACK(node)	((node)->color = BLACK)

/*
 * Chain management.
 *
 * The "ancestors" member of chains were removed, with their job now
 * being wholy handled by parent pointers (which didn't exist, because
 * of memory concerns, when chains were first implemented).
 */
#define ADD_LEVEL(chain, node) \
			(chain)->levels[(chain)->level_count++] = (node)

/*
 * The following macros directly access normally private name variables.
 * These macros are used to avoid a lot of function calls in the critical
 * path of the tree traversal code.
 */

#define NODENAME(node, name) \
do { \
	(name)->length = NAMELEN(node); \
	(name)->labels = OFFSETLEN(node); \
	(name)->ndata = NAME(node); \
	(name)->offsets = OFFSETS(node); \
	(name)->attributes = ATTRS(node); \
	(name)->attributes |= DNS_NAMEATTR_READONLY; \
} while (0)

#ifdef DEBUG
#define inline
/*
 * A little something to help out in GDB.
 */
dns_name_t Name(dns_rbtnode_t *node);
dns_name_t
Name(dns_rbtnode_t *node) {
	dns_name_t name;

	dns_name_init(&name, NULL);
	if (node != NULL)
		NODENAME(node, &name);

	return (name);
}

static void dns_rbt_printnodename(dns_rbtnode_t *node);
#endif

static inline dns_rbtnode_t *
find_up(dns_rbtnode_t *node) {
	dns_rbtnode_t *root;

	/*
	 * Return the node in the level above the argument node that points
	 * to the level the argument node is in.  If the argument node is in
	 * the top level, the return value is NULL.
	 */
	for (root = node; ! IS_ROOT(root); root = PARENT(root))
		; /* Nothing. */

	return(PARENT(root));
}

static inline unsigned int
name_hash(dns_name_t *name) {
	unsigned int nlabels;
	unsigned int hash;
	dns_name_t tname;

	if (dns_name_countlabels(name) == 1)
		return (dns_name_hash(name, ISC_FALSE));

	dns_name_init(&tname, NULL);
	nlabels = dns_name_countlabels(name);
	hash = 0;

	for ( ; nlabels > 0; nlabels--) {
		dns_name_getlabelsequence(name, nlabels - 1, 1, &tname);
		hash += dns_name_hash(&tname, ISC_FALSE);
	}

	return (hash);
}

#ifdef DNS_RBT_USEHASH
static inline void
compute_node_hash(dns_rbtnode_t *node) {
	unsigned int hash;
	dns_name_t name;
	dns_rbtnode_t *up_node;

	dns_name_init(&name, NULL);
	NODENAME(node, &name);
	hash = name_hash(&name);

	up_node = find_up(node);
	if (up_node != NULL)
		hash += HASHVAL(up_node);

	HASHVAL(node) = hash;
}
#endif

/*
 * Forward declarations.
 */
static isc_result_t
create_node(isc_mem_t *mctx, dns_name_t *name, dns_rbtnode_t **nodep);

#ifdef DNS_RBT_USEHASH
static inline isc_result_t
hash_node(dns_rbt_t *rbt, dns_rbtnode_t *node);
static inline void
unhash_node(dns_rbt_t *rbt, dns_rbtnode_t *node);
#else
#define hash_node(rbt, node) (ISC_R_SUCCESS)
#define unhash_node(rbt, node)
#endif

static inline void
rotate_left(dns_rbtnode_t *node, dns_rbtnode_t **rootp);
static inline void
rotate_right(dns_rbtnode_t *node, dns_rbtnode_t **rootp);

static void
dns_rbt_addonlevel(dns_rbtnode_t *node, dns_rbtnode_t *current, int order, 
		   dns_rbtnode_t **rootp);

static void
dns_rbt_deletefromlevel(dns_rbtnode_t *delete, dns_rbtnode_t **rootp);

static void
dns_rbt_deletetree(dns_rbt_t *rbt, dns_rbtnode_t *node);

/*
 * Initialize a red/black tree of trees.
 */
isc_result_t
dns_rbt_create(isc_mem_t *mctx, void (*deleter)(void *, void *),
	       void *deleter_arg, dns_rbt_t **rbtp)
{
	dns_rbt_t *rbt;

	REQUIRE(mctx != NULL);
	REQUIRE(rbtp != NULL && *rbtp == NULL);
	REQUIRE(deleter == NULL ? deleter_arg == NULL : 1);

	rbt = (dns_rbt_t *)isc_mem_get(mctx, sizeof(*rbt));
	if (rbt == NULL)
		return (ISC_R_NOMEMORY);

	rbt->mctx = mctx;
	rbt->data_deleter = deleter;
	rbt->deleter_arg = deleter_arg;
	rbt->root = NULL;
	rbt->nodecount = 0;
	rbt->hashtable = NULL;
	rbt->hashsize = 0;
	rbt->magic = RBT_MAGIC;

	*rbtp = rbt;

	return (ISC_R_SUCCESS);
}

/*
 * Deallocate a red/black tree of trees.
 */
void
dns_rbt_destroy(dns_rbt_t **rbtp) {
	dns_rbt_t *rbt;

	REQUIRE(rbtp != NULL && VALID_RBT(*rbtp));

	rbt = *rbtp;

	dns_rbt_deletetree(rbt, rbt->root);

	INSIST(rbt->nodecount == 0);

	if (rbt->hashtable != NULL)
		isc_mem_put(rbt->mctx, rbt->hashtable,
			    rbt->hashsize * sizeof(dns_rbtnode_t *));

	rbt->magic = 0;

	isc_mem_put(rbt->mctx, rbt, sizeof(*rbt));

	*rbtp = NULL;
}

unsigned int
dns_rbt_nodecount(dns_rbt_t *rbt) {
	REQUIRE(VALID_RBT(rbt));
	return (rbt->nodecount);
}

static inline isc_result_t
chain_name(dns_rbtnodechain_t *chain, dns_name_t *name,
	   isc_boolean_t include_chain_end)
{
	dns_name_t nodename;
	isc_result_t result = ISC_R_SUCCESS;
	int i;

	dns_name_init(&nodename, NULL);

	if (include_chain_end && chain->end != NULL) {
		NODENAME(chain->end, &nodename);
		result = dns_name_copy(&nodename, name, NULL);
		if (result != ISC_R_SUCCESS)
			return (result);
	} else
		dns_name_reset(name);

	for (i = (int)chain->level_count - 1; i >= 0; i--) {
		NODENAME(chain->levels[i], &nodename);
		result = dns_name_concatenate(name, &nodename, name, NULL);

		if (result != ISC_R_SUCCESS)
			return (result);
	}
	return (result);
}

static inline isc_result_t
move_chain_to_last(dns_rbtnodechain_t *chain, dns_rbtnode_t *node) {
	do {
		/*
		 * Go as far right and then down as much as possible,
		 * as long as the rightmost node has a down pointer.
		 */
		while (RIGHT(node) != NULL)
			node = RIGHT(node);

		if (DOWN(node) == NULL)
			break;

		ADD_LEVEL(chain, node);
		node = DOWN(node);
	} while (1);

	chain->end = node;

	return (ISC_R_SUCCESS);
}

/*
 * Add 'name' to tree, initializing its data pointer with 'data'.
 */

isc_result_t
dns_rbt_addnode(dns_rbt_t *rbt, dns_name_t *name, dns_rbtnode_t **nodep) {
	/*
	 * Does this thing have too many variables or what?
	 */
	dns_rbtnode_t **root, *parent, *child, *current, *new_current;
	dns_name_t *add_name, current_name, *prefix, *suffix;
	dns_fixedname_t fixedcopy, fixedprefix, fixedsuffix;
	dns_offsets_t current_offsets;
	dns_namereln_t compared;
	isc_result_t result = ISC_R_SUCCESS;
	dns_rbtnodechain_t chain;
	unsigned int common_labels, common_bits, add_bits;
	int order;

	REQUIRE(VALID_RBT(rbt));
	REQUIRE(dns_name_isabsolute(name));
	REQUIRE(nodep != NULL && *nodep == NULL);

	/*
	 * Create a copy of the name so the original name structure is
	 * not modified.  The name data needs to be modifiable when
	 * a node is split on a bitstring label.
	 */
	dns_fixedname_init(&fixedcopy);
	add_name = dns_fixedname_name(&fixedcopy);
	dns_name_clone(name, add_name);

	if (rbt->root == NULL) {
		result = create_node(rbt->mctx, add_name, &new_current);
		if (result == ISC_R_SUCCESS) {
			rbt->nodecount++;
			new_current->is_root = 1;
			rbt->root = new_current;
			*nodep = new_current;
			result = hash_node(rbt, new_current);
		}
		return (result);
	}

	dns_rbtnodechain_init(&chain, rbt->mctx);

	dns_fixedname_init(&fixedprefix);
	dns_fixedname_init(&fixedsuffix);
	prefix = dns_fixedname_name(&fixedprefix);
	suffix = dns_fixedname_name(&fixedsuffix);

	root = &rbt->root;
	INSIST(IS_ROOT(*root));
	parent = NULL;
	current = NULL;
	child = *root;
	dns_name_init(&current_name, current_offsets);

	do {
		current = child;

		NODENAME(current, &current_name);
		compared = dns_name_fullcompare(add_name, &current_name,
						&order,
						&common_labels, &common_bits);

		if (compared == dns_namereln_equal) {
			*nodep = current;
			result = ISC_R_EXISTS;
			break;

		}

		if (compared == dns_namereln_none) {

			if (order < 0) {
				parent = current;
				child = LEFT(current);

			} else if (order > 0) {
				parent = current;
				child = RIGHT(current);

			}

		} else {
			/*
			 * This name has some suffix in common with the
			 * name at the current node.  If the name at
			 * the current node is shorter, that means the
			 * new name should be in a subtree.  If the
			 * name at the current node is longer, that means
			 * the down pointer to this tree should point
			 * to a new tree that has the common suffix, and
			 * the non-common parts of these two names should
			 * start a new tree.
			 */
			if (compared == dns_namereln_subdomain) {
				/*
				 * All of the existing labels are in common,
				 * so the new name is in a subtree.
				 * Whack off the common labels for the
				 * not-in-common part to be searched for
				 * in the next level.
				 */
				result = dns_name_split(add_name,
							common_labels,
							common_bits,
							add_name, NULL);

				if (result != ISC_R_SUCCESS)
					break;

				/*
				 * Follow the down pointer (possibly NULL).
				 */
				root = &DOWN(current);

				INSIST(*root == NULL ||
				       (IS_ROOT(*root) &&
					PARENT(*root) == current));

				parent = NULL;
				child = DOWN(current);
				ADD_LEVEL(&chain, current);

			} else {
				/*
				 * The number of labels in common is fewer
				 * than the number of labels at the current
				 * node, so the current node must be adjusted
				 * to have just the common suffix, and a down
				 * pointer made to a new tree.
				 */

				INSIST(compared == dns_namereln_commonancestor
				       || compared == dns_namereln_contains);

				/*
				 * Ensure the number of levels in the tree
				 * does not exceed the number of logical
				 * levels allowed by DNSSEC.
				 *
				 * XXXDCL need a better error result?
				 *
				 * XXXDCL Since chain ancestors were removed,
				 * no longer used by dns_rbt_addonlevel(),
				 * this is the only real use of chains in the
				 * function.  It could be done instead with
				 * a simple integer variable, but I am pressed
				 * for time.
				 */
				if (chain.level_count ==
				    (sizeof(chain.levels) /
				     sizeof(*chain.levels))) {
					result = ISC_R_NOSPACE;
					break;
				}

				/*
				 * Split the name into two parts, a prefix
				 * which is the not-in-common parts of the
				 * two names and a suffix that is the common
				 * parts of them.
				 */
				result = dns_name_split(&current_name,
							common_labels,
							common_bits,
							prefix, suffix);

				if (result == ISC_R_SUCCESS)
					result = create_node(rbt->mctx, suffix,
							     &new_current);

				if (result != ISC_R_SUCCESS)
					break;

				/*
				 * Reproduce the tree attributes of the
				 * current node.
				 */
				new_current->is_root = current->is_root;
				PARENT(new_current)  = PARENT(current);
				LEFT(new_current)    = LEFT(current);
				RIGHT(new_current)   = RIGHT(current);
				COLOR(new_current)   = COLOR(current);

				/*
				 * Fix pointers that were to the current node.
				 */
				if (parent != NULL) {
					if (LEFT(parent) == current)
						LEFT(parent) = new_current;
					else
						RIGHT(parent) = new_current;
				}
				if (LEFT(new_current) != NULL)
					PARENT(LEFT(new_current)) =
						new_current;
				if (RIGHT(new_current) != NULL)
					PARENT(RIGHT(new_current)) =
						new_current;
				if (*root == current)
					*root = new_current;

				/*
				 * Now make the new root of the subtree
				 * as the not-in-common labels of the current
				 * node, keeping the same memory location so
				 * as not to break any external references to
				 * the node.  The down pointer and name data
				 * are preserved, while left and right
				 * pointers are nullified when the node is
				 * established as the start of the next level.
				 *
				 * The name stored at the node is effectively
				 * truncated in place by setting the shorter
				 * name length, moving the offsets to the
				 * end of the truncated name, and then
				 * updating PADBYTES to reflect the truncation.
				 *
				 * When bitstring labels are involved, things
				 * are just a tad more complicated (aren't
				 * they always?) because the splitting
				 * has shifted the bits that this name needs
				 * from the end of the label they were in
				 * to either the beginning of the label or
				 * even to the previous (lesser significance)
				 * label if the split was done in a maximally
				 * sized bitstring label.  The bit count has
				 * been adjusted too, so there are convolutions
				 * to deal with all the bit movement.  Yay,
				 * I *love* bit labels.  Grumble grumble.
				 */
				if (common_bits > 0) {
					unsigned char *p;
					unsigned int skip_width;
					unsigned int start_label =
					    dns_name_countlabels(&current_name)
						- common_labels;

					/*
					 * If it is not the first label which
					 * was split, also copy the label
					 * before it -- which will essentially
					 * be a NO-OP unless the preceding
					 * label is a bitstring and the split
					 * label was 256 bits.  Testing for
					 * that case is probably roughly
					 * as expensive as just unconditionally
					 * copying the preceding label.
					 */
					if (start_label > 0)
						start_label--;

					skip_width =
						prefix->offsets[start_label];

					memcpy(NAME(current) + skip_width,
					       prefix->ndata + skip_width,
					       prefix->length - skip_width);

					/*
					 * Now add_bits is set to the total
					 * number of bits in the split label of
					 * the name being added, and used later
					 * to determine if the job was
					 * completed by pushing the
					 * not-in-common bits down one level.
					 */
					start_label =
						dns_name_countlabels(add_name)
						- common_labels;

					p = add_name->ndata +
						add_name->offsets[start_label];
					INSIST(*p == DNS_LABELTYPE_BITSTRING);

					add_bits = *(p + 1);

					/*
					 * A bitstring that was split would not
					 * result in a part of maximal length.
					 */
					INSIST(add_bits != 0);
				} else
					add_bits = 0;

				NAMELEN(current) = prefix->length;
				OFFSETLEN(current) = prefix->labels;
				memcpy(OFFSETS(current), prefix->offsets,
				       prefix->labels);
				PADBYTES(current) +=
				       (current_name.length - prefix->length) +
				       (current_name.labels - prefix->labels);

				/*
				 * Set up the new root of the next level.
				 * By definition it will not be the top
				 * level tree, so clear DNS_NAMEATTR_ABSOLUTE.
				 */
				current->is_root = 1;
				PARENT(current) = new_current;
				DOWN(new_current) = current;
				root = &DOWN(new_current);

				ADD_LEVEL(&chain, new_current);

				LEFT(current) = NULL;
				RIGHT(current) = NULL;

				MAKE_BLACK(current);
				ATTRS(current) &= ~DNS_NAMEATTR_ABSOLUTE;

				rbt->nodecount++;
				result = hash_node(rbt, new_current);
				if (result != ISC_R_SUCCESS)
					break;

				if (common_labels ==
				    dns_name_countlabels(add_name) &&
				    common_bits == add_bits) {
					/*
					 * The name has been added by pushing
					 * the not-in-common parts down to
					 * a new level.
					 */
					*nodep = new_current;
					return (ISC_R_SUCCESS);

				} else {
					/*
					 * The current node has no data,
					 * because it is just a placeholder.
					 * Its data pointer is already NULL
					 * from create_node()), so there's
					 * nothing more to do to it.
					 */

					/*
					 * The not-in-common parts of the new
					 * name will be inserted into the new
					 * level following this loop (unless
					 * result != ISC_R_SUCCESS, which
					 * is tested after the loop ends).
					 */
					result = dns_name_split(add_name,
								common_labels,
								common_bits,
								add_name,
								NULL);


					break;
				}

			}

		}

	} while (child != NULL);

	if (result == ISC_R_SUCCESS)
		result = create_node(rbt->mctx, add_name, &new_current);

	if (result == ISC_R_SUCCESS) {
		dns_rbt_addonlevel(new_current, current, order, root);
		rbt->nodecount++;
		*nodep = new_current;
		result = hash_node(rbt, new_current);
		/*
		 * XXXDCL Ugh.  If hash_node failed, it was because
		 * there is not enough memory.  The node is now unfindable,
		 * and ideally should be removed.  This is kind of tricky,
		 * and all hell is probably going to break loose throughout
		 * the rest of the library because of the lack of memory,
		 * so fixing up the tree as though no addition had been
		 * made is skipped.  (Actually, this hash_node failing is
		 * not the only situation in this file where an unexpected
		 * error can leave things in an incorrect state.)
		 */
	}

	return (result);
}

/*
 * Add a name to the tree of trees, associating it with some data.
 */
isc_result_t
dns_rbt_addname(dns_rbt_t *rbt, dns_name_t *name, void *data) {
	isc_result_t result;
	dns_rbtnode_t *node;

	REQUIRE(VALID_RBT(rbt));
	REQUIRE(dns_name_isabsolute(name));

	node = NULL;

	result = dns_rbt_addnode(rbt, name, &node);

	/*
	 * dns_rbt_addnode will report the node exists even when
	 * it does not have data associated with it, but the
	 * dns_rbt_*name functions all behave depending on whether
	 * there is data associated with a node.
	 */
	if (result == ISC_R_SUCCESS ||
	    (result == ISC_R_EXISTS && DATA(node) == NULL)) {
		DATA(node) = data;
		result = ISC_R_SUCCESS;
	}

	return (result);
}

/*
 * Find the node for "name" in the tree of trees.
 */
isc_result_t
dns_rbt_findnode(dns_rbt_t *rbt, dns_name_t *name, dns_name_t *foundname,
		 dns_rbtnode_t **node, dns_rbtnodechain_t *chain,
		 unsigned int options, dns_rbtfindcallback_t callback,
		 void *callback_arg)
{
	dns_rbtnode_t *current, *last_compared, *current_root;
	dns_rbtnodechain_t localchain;
	dns_name_t *search_name, current_name, *callback_name;
	dns_fixedname_t fixedcallbackname, fixedsearchname;
	dns_namereln_t compared;
	isc_result_t result, saved_result;
	unsigned int common_labels, common_bits;
	int order;

	REQUIRE(VALID_RBT(rbt));
	REQUIRE(dns_name_isabsolute(name));
	REQUIRE(node != NULL && *node == NULL);
	REQUIRE((options & (DNS_RBTFIND_NOEXACT | DNS_RBTFIND_NOPREDECESSOR))
		!=         (DNS_RBTFIND_NOEXACT | DNS_RBTFIND_NOPREDECESSOR));

	/*
	 * If there is a chain it needs to appear to be in a sane state,
	 * otherwise a chain is still needed to generate foundname and
	 * callback_name.
	 */
	if (chain == NULL) {
		options |= DNS_RBTFIND_NOPREDECESSOR;
		chain = &localchain;
		dns_rbtnodechain_init(chain, rbt->mctx);
	} else
		dns_rbtnodechain_reset(chain);

	if (rbt->root == NULL)
		return (ISC_R_NOTFOUND);
	else {
		/*
		 * Appease GCC about variables it incorrectly thinks are
		 * possibly used unitialized.
		 */
		compared = dns_namereln_none;
		last_compared = NULL;
	}

	dns_fixedname_init(&fixedcallbackname);
	callback_name = dns_fixedname_name(&fixedcallbackname);

	/*
	 * search_name is the name segment being sought in each tree level.
	 * By using a fixedname, the search_name will definitely have offsets
	 * and a buffer for use by any splitting that happens in the middle
	 * of a bitstring label.  By using dns_name_clone, no name data is
	 * copied unless a bitstring split occurs.
	 */
	dns_fixedname_init(&fixedsearchname);
	search_name = dns_fixedname_name(&fixedsearchname);
	dns_name_clone(name, search_name);

	dns_name_init(&current_name, NULL);

	saved_result = ISC_R_SUCCESS;
	current = rbt->root;
	current_root = rbt->root;

	while (current != NULL) {
		NODENAME(current, &current_name);
		compared = dns_name_fullcompare(search_name, &current_name,
						&order,
						&common_labels, &common_bits);
		last_compared = current;

		if (compared == dns_namereln_equal)
			break;

		if (compared == dns_namereln_none) {
#ifdef DNS_RBT_USEHASH
			dns_name_t hash_name;
			dns_rbtnode_t *hnode;
			dns_rbtnode_t *up_current;
			unsigned int nlabels;
			unsigned int tlabels = 1;
			unsigned int hash;
			isc_boolean_t has_bitstring = ISC_FALSE;

			/*
			 * If there is no hash table, hashing can't be done.
			 * Similarly, when current != current_root, that
			 * means a left or right pointer was followed, which
			 * only happens when the algorithm fell through to
			 * the traditional binary search because of a
			 * bitstring label, so that traditional search
			 * should be continued.
			 */
			if (rbt->hashtable == NULL ||
			    current != current_root)
				goto nohash;

			nlabels = dns_name_countlabels(search_name);

			/*
			 * current_root is the root of the current level, so
			 * it's parent is the same as it's "up" pointer.
			 */
			up_current = PARENT(current_root);
			dns_name_init(&hash_name, NULL);

		hashagain:
			dns_name_getlabelsequence(search_name,
						  nlabels - tlabels,
						  tlabels, &hash_name);
			hash = HASHVAL(up_current) + name_hash(&hash_name);

			for (hnode = rbt->hashtable[hash % rbt->hashsize];
			     hnode != NULL;
			     hnode = hnode->hashnext)
			{
				dns_name_t hnode_name;

				if (hash != HASHVAL(hnode))
					continue;
				if (find_up(hnode) != up_current)
					continue;
				dns_name_init(&hnode_name, NULL);
				NODENAME(hnode, &hnode_name);
				if (dns_name_equal(&hnode_name, &hash_name))
					break;
			}

			if (hnode != NULL) {
				current = hnode;
				/*
				 * This is an optimization.  If hashing found
				 * the right node, the next call to
				 * dns_name_fullcompare() would obviously
				 * return _equal or _subdomain.  Determine
				 * which of those would be the case by
				 * checking if the full name was hashed.  Then
				 * make it look like dns_name_fullcompare
				 * was called and jump to the right place.
				 */
				if (tlabels == nlabels) {
					compared = dns_namereln_equal;
					break;
				} else {
					common_labels = tlabels;
					common_bits = 0;
					compared = dns_namereln_subdomain;
					goto subdomain;
				}
			}

			/*
			 * XXXDCL Bitstring labels complicate things, as usual.
			 * Checking for the situation could be done up by the
			 * dns_name_getlabelsequence so that they could still
			 * use the hashing code, but it would be messy to
			 * repeatedly try various bitstring lengths.  Instead
			 * just notice when a bitstring label is involved and
			 * then punt to the traditional binary search if no
			 * hash node is found after all of the labels are
			 * tried.
			 */
			if (has_bitstring == ISC_FALSE &&
			    hash_name.ndata[0] ==
			    DNS_LABELTYPE_BITSTRING)
				has_bitstring = ISC_TRUE;

			if (tlabels++ < nlabels)
				goto hashagain;

			/*
			 * All of the labels have been tried against the hash
			 * table.  If there wasn't a bitstring label involved,
			 * the name isn't in the table.  If there was, fall
			 * through to the traditional search algorithm.
			 */
			if (! has_bitstring) {
				/*
				 * Done with the search.
				 */
				current = NULL;
				continue;
			}
			    
			/* FALLTHROUGH */
		nohash:
#endif /* DNS_RBT_USEHASH */
			/*
			 * Standard binary search tree movement.
			 */
			if (order < 0)
				current = LEFT(current);
			else
				current = RIGHT(current);

		} else {
			/*
			 * The names have some common suffix labels.
			 *
			 * If the number in common are equal in length to
			 * the current node's name length, then follow the
			 * down pointer and search in the new tree.
			 */
			if (compared == dns_namereln_subdomain) {
		subdomain:
				/*
				 * Whack off the current node's common parts
				 * for the name to search in the next level.
				 */
				result = dns_name_split(search_name,
							common_labels,
							common_bits,
							search_name, NULL);
				if (result != ISC_R_SUCCESS) {
					dns_rbtnodechain_reset(chain);
					return (result);
				}

				/*
				 * This might be the closest enclosing name.
				 */
				if (DATA(current) != NULL ||
				    (options & DNS_RBTFIND_EMPTYDATA) != 0)
					*node = current;

				/*
				 * Point the chain to the next level.   This
				 * needs to be done before 'current' is pointed
				 * there because the callback in the next
				 * block of code needs the current 'current',
				 * but in the event the callback requests that
				 * the search be stopped then the
				 * DNS_R_PARTIALMATCH code at the end of this
				 * function needs the chain pointed to the
				 * next level.
				 */
				ADD_LEVEL(chain, current);

				/*
				 * The caller may want to interrupt the
				 * downward search when certain special nodes
				 * are traversed.  If this is a special node,
				 * the callback is used to learn what the
				 * caller wants to do.
				 */
				if (callback != NULL &&
				    FINDCALLBACK(current)) {
					result = chain_name(chain,
							    callback_name,
							    ISC_FALSE);
					if (result != ISC_R_SUCCESS) {
						dns_rbtnodechain_reset(chain);
						return (result);
					}

					result = (callback)(current,
							    callback_name,
							    callback_arg);
					if (result != DNS_R_CONTINUE) {
						saved_result = result;
						/*
						 * Treat this node as if it
						 * had no down pointer.
						 */
						current = NULL;
						break;
					}
				}

				/*
				 * Finally, head to the next tree level.
				 */
				current = DOWN(current);
				current_root = current;

			} else {
				/*
				 * Though there are labels in common, the
				 * entire name at this node is not common
				 * with the search name so the search
				 * name does not exist in the tree.
				 */
				INSIST(compared == dns_namereln_commonancestor
				       || compared == dns_namereln_contains);

				current = NULL;
			}
		}
	}

	/*
	 * If current is not NULL, NOEXACT is not disallowing exact matches,
	 * and either the node has data or an empty node is ok, return
	 * ISC_R_SUCCESS to indicate an exact match.
	 */
	if (current != NULL && (options & DNS_RBTFIND_NOEXACT) == 0 &&
	    (DATA(current) != NULL ||
	     (options & DNS_RBTFIND_EMPTYDATA) != 0)) {
		/*
		 * Found an exact match.
		 */
		chain->end = current;
		chain->level_matches = chain->level_count;

		if (foundname != NULL)
			result = chain_name(chain, foundname, ISC_TRUE);
		else
			result = ISC_R_SUCCESS;

		if (result == ISC_R_SUCCESS) {
			*node = current;
			result = saved_result;
		} else
			*node = NULL;
	} else {
		/*
		 * Did not find an exact match (or did not want one).
		 */
		if (*node != NULL) {
			/*
			 * ... but found a partially matching superdomain.
			 * Unwind the chain to the partial match node
			 * to set level_matches to the level above the node,
			 * and then to derive the name.
			 *
			 * chain->level_count is guaranteed to be at least 1
			 * here because by definition of finding a superdomain,
			 * the chain is pointed to at least the first subtree.
			 */
			chain->level_matches = chain->level_count - 1;

			while (chain->levels[chain->level_matches] != *node) {
				INSIST(chain->level_matches > 0);
				chain->level_matches--;
			}

			if (foundname != NULL) {
				unsigned int saved_count = chain->level_count;

				chain->level_count = chain->level_matches + 1;

				result = chain_name(chain, foundname,
						    ISC_FALSE);

				chain->level_count = saved_count;
			} else
				result = ISC_R_SUCCESS;

			if (result == ISC_R_SUCCESS)
				result = DNS_R_PARTIALMATCH;

		} else
			result = ISC_R_NOTFOUND;

		if (current != NULL) {
			/*
			 * There was an exact match but either
			 * DNS_RBTFIND_NOEXACT was set, or
			 * DNS_RBTFIND_EMPTYDATA was set and the node had no
			 * data.  A policy decision was made to set the
			 * chain to the exact match, but this is subject
			 * to change if it becomes apparent that something
			 * else would be more useful.  It is important that
			 * this case is handled here, because the predecessor
			 * setting code below assumes the match was not exact.
			 */
			INSIST(((options & DNS_RBTFIND_NOEXACT) != 0) ||
			       ((options & DNS_RBTFIND_EMPTYDATA) == 0 &&
				DATA(current) == NULL));
			chain->end = current;

		} else if ((options & DNS_RBTFIND_NOPREDECESSOR) != 0) {
			/*
			 * Ensure the chain points nowhere.
			 */
			chain->end = NULL;

		} else {
			/*
			 * Since there was no exact match, the chain argument
			 * needs to be pointed at the DNSSEC predecessor of
			 * the search name.
			 */
			if (compared == dns_namereln_subdomain) {
				/*
				 * Attempted to follow a down pointer that was
				 * NULL, which means the searched for name was
				 * a subdomain of a terminal name in the tree.
				 * Since there are no existing subdomains to
				 * order against, the terminal name is the
				 * predecessor.
				 */
				INSIST(chain->level_count > 0);
				INSIST(chain->level_matches <
				       chain->level_count);
				chain->end =
					chain->levels[--chain->level_count];

			} else {
				isc_result_t result2;

				/*
				 * Point current to the node that stopped
				 * the search.
				 *
				 * With the hashing modification that has been
				 * added to the algorithm, the stop node of a
				 * standard binary search is not known.  So it
				 * has to be found.  There is probably a more
				 * clever way of doing this.
				 *
				 * The assignment of current to NULL when
				 * the relationship is *not* dns_namereln_none,
				 * even though it later gets set to the same
				 * last_compared anyway, is simply to not push
				 * the while loop in one more level of
				 * indentation.
				 */
				if (compared == dns_namereln_none)
					current = last_compared;
				else
					current = NULL;

				while (current != NULL) {
					NODENAME(current, &current_name);
					compared = dns_name_fullcompare(
								search_name,
								&current_name,
								&order,
								&common_labels,
								&common_bits);

					last_compared = current;

					/*
					 * Standard binary search movement.
					 */
					if (order < 0)
						current = LEFT(current);
					else
						current = RIGHT(current);

				}

				current = last_compared;

				/*
				 * Reached a point within a level tree that
				 * positively indicates the name is not
				 * present, but the stop node could be either
				 * less than the desired name (order > 0) or
				 * greater than the desired name (order < 0).
				 *
				 * If the stop node is less, it is not
				 * necessarily the predecessor.  If the stop
				 * node has a down pointer, then the real
				 * predecessor is at the end of a level below
				 * (not necessarily the next level).
				 * Move down levels until the rightmost node
				 * does not have a down pointer.
				 *
				 * When the stop node is greater, it is
				 * the successor.  All the logic for finding
				 * the predecessor is handily encapsulated
				 * in dns_rbtnodechain_prev.  In the event
				 * that the search name is less than anything
				 * else in the tree, the chain is reset.
				 * XXX DCL What is the best way for the caller
				 *         to know that the search name has
				 *         no predecessor?
				 */


				if (order > 0) {
					if (DOWN(current) != NULL) {
						ADD_LEVEL(chain, current);

						result2 =
						      move_chain_to_last(chain,
								DOWN(current));

						if (result2 != ISC_R_SUCCESS)
							result = result2;
					} else
						/*
						 * Ah, the pure and simple
						 * case.  The stop node is the
						 * predecessor.
						 */
						chain->end = current;

				} else {
					INSIST(order < 0);

					chain->end = current;

					result2 = dns_rbtnodechain_prev(chain,
									NULL,
									NULL);
					if (result2 == ISC_R_SUCCESS ||
					    result2 == DNS_R_NEWORIGIN)
						; 	/* Nothing. */
					else if (result2 == ISC_R_NOMORE)
						/*
						 * There is no predecessor.
						 */
						dns_rbtnodechain_reset(chain);
					else
						result = result2;
				}

			}
		}
	}

	return (result);
}

/*
 * Get the data pointer associated with 'name'.
 */
isc_result_t
dns_rbt_findname(dns_rbt_t *rbt, dns_name_t *name, unsigned int options,
		 dns_name_t *foundname, void **data) {
	dns_rbtnode_t *node = NULL;
	isc_result_t result;

	REQUIRE(data != NULL && *data == NULL);

	result = dns_rbt_findnode(rbt, name, foundname, &node, NULL,
				  options, NULL, NULL);

	if (node != NULL &&
	    (DATA(node) != NULL || (options & DNS_RBTFIND_EMPTYDATA) != 0))
		*data = DATA(node);
	else
		result = ISC_R_NOTFOUND;

	return (result);
}

/*
 * Delete a name from the tree of trees.
 */
isc_result_t
dns_rbt_deletename(dns_rbt_t *rbt, dns_name_t *name, isc_boolean_t recurse) {
	dns_rbtnode_t *node = NULL;
	isc_result_t result;

	REQUIRE(VALID_RBT(rbt));
	REQUIRE(dns_name_isabsolute(name));

	/*
	 * First, find the node.
	 *
	 * When searching, the name might not have an exact match:
	 * consider a.b.a.com, b.b.a.com and c.b.a.com as the only
	 * elements of a tree, which would make layer 1 a single
	 * node tree of "b.a.com" and layer 2 a three node tree of
	 * a, b, and c.  Deleting a.com would find only a partial depth
	 * match in the first layer.  Should it be a requirement that
	 * that the name to be deleted have data?  For now, it is.
	 *
	 * ->dirty, ->locknum and ->references are ignored; they are
	 * solely the province of rbtdb.c.
	 */
	result = dns_rbt_findnode(rbt, name, NULL, &node, NULL,
				  DNS_RBTFIND_NOOPTIONS, NULL, NULL);

	if (result == ISC_R_SUCCESS) {
		if (DATA(node) != NULL)
			result = dns_rbt_deletenode(rbt, node, recurse);
		else
			result = ISC_R_NOTFOUND;

	} else if (result == DNS_R_PARTIALMATCH)
		result = ISC_R_NOTFOUND;

	return (result);
}

/*
 * Remove a node from the tree of trees.
 *
 * NOTE WELL: deletion is *not* symmetric with addition; that is, reversing
 * a sequence of additions to be deletions will not generally get the
 * tree back to the state it started in.  For example, if the addition
 * of "b.c" caused the node "a.b.c" to be split, pushing "a" to its own level,
 * then the subsequent deletion of "b.c" will not cause "a" to be pulled up,
 * restoring "a.b.c".  The RBT *used* to do this kind of rejoining, but it
 * turned out to be a bad idea because it could corrupt an active nodechain
 * that had "b.c" as one of its levels -- and the RBT has no idea what
 * nodechains are in use by callers, so it can't even *try* to helpfully
 * fix them up (which would probably be doomed to failure anyway).
 *
 * Similarly, it is possible to leave the tree in a state where a supposedly
 * deleted node still exists.  The first case of this is obvious; take
 * the tree which has "b.c" on one level, pointing to "a".  Now deleted "b.c".
 * It was just established in the previous paragraph why we can't pull "a"
 * back up to its parent level.  But what happens when "a" then gets deleted?
 * "b.c" is left hanging around without data or children.  This condition
 * is actually pretty easy to detect, but ... should it really be removed?
 * Is a chain pointing to it?  An iterator?  Who knows!  (Note that the
 * references structure member cannot be looked at because it is private to
 * rbtdb.)  This is ugly and makes me unhappy, but after hours of trying to
 * make it more aesthetically proper and getting nowhere, this is the way it
 * is going to stay until such time as it proves to be a *real* problem.
 *
 * Finally, for reference, note that the original routine that did node
 * joining was called join_nodes().  It has been excised, living now only
 * in the CVS history, but comments have been left behind that point to it just
 * in case someone wants to muck with this some more.
 *
 * The one positive aspect of all of this is that joining used to have a
 * case where it might fail.  Without trying to join, now this function always
 * succeeds. It still returns isc_result_t, though, so the API wouldn't change.  */
isc_result_t
dns_rbt_deletenode(dns_rbt_t *rbt, dns_rbtnode_t *node, isc_boolean_t recurse)
{
	dns_rbtnode_t *parent;

	REQUIRE(VALID_RBT(rbt));
	REQUIRE(node != NULL);

	if (DOWN(node) != NULL) {
		if (recurse)
			dns_rbt_deletetree(rbt, DOWN(node));

		else {
			if (DATA(node) != NULL && rbt->data_deleter != NULL)
				rbt->data_deleter(DATA(node),
						  rbt->deleter_arg);
			DATA(node) = NULL;

			/*
			 * Since there is at least one node below this one and
			 * no recursion was requested, the deletion is
			 * complete.  The down node from this node might be all
			 * by itself on a single level, so join_nodes() could
			 * be used to collapse the tree (with all the caveats
			 * of the comment at the start of this function).
			 */
			return (ISC_R_SUCCESS);
		}
	}

	/*
	 * Note the node that points to the level of the node that is being
	 * deleted.  If the deleted node is the top level, parent will be set
	 * to NULL.
	 */
	parent = find_up(node);

	/*
	 * This node now has no down pointer (either because it didn't
	 * have one to start, or because it was recursively removed).
	 * So now the node needs to be removed from this level.
	 */
	dns_rbt_deletefromlevel(node, parent == NULL ? &rbt->root :
						       &DOWN(parent));

	if (DATA(node) != NULL && rbt->data_deleter != NULL)
		rbt->data_deleter(DATA(node), rbt->deleter_arg);

	unhash_node(rbt, node);
	isc_mem_put(rbt->mctx, node, NODE_SIZE(node));
	rbt->nodecount--;

	/*
	 * There are now two special cases that can exist that would
	 * not have existed if the tree had been created using only
	 * the names that now exist in it.  (This is all related to
	 * join_nodes() as described in this function's introductory comment.)
	 * Both cases exist when the deleted node's parent (the node
	 * that pointed to the deleted node's level) is not null but
	 * it has no data:  parent != NULL && DATA(parent) == NULL.
	 *
	 * The first case is that the deleted node was the last on its level:
	 * DOWN(parent) == NULL.  This case can only exist if the parent was
	 * previously deleted -- and so now, apparently, the parent should go
	 * away.  That can't be done though because there might be external
	 * references to it, such as through a nodechain.
	 *
	 * The other case also involves a parent with no data, but with the
	 * deleted node being the next-to-last node instead of the last:
	 * LEFT(DOWN(parent)) == NULL && RIGHT(DOWN(parent)) == NULL.
	 * Presumably now the remaining node on the level should be joined
	 * with the parent, but it's already been described why that can't be
	 * done.
	 */

	/*
	 * This function never fails.
	 */
	return (ISC_R_SUCCESS);
}

void
dns_rbt_namefromnode(dns_rbtnode_t *node, dns_name_t *name) {

	REQUIRE(node != NULL);
	REQUIRE(name != NULL);
	REQUIRE(name->offsets == NULL);

	NODENAME(node, name);
}

isc_result_t
dns_rbt_fullnamefromnode(dns_rbtnode_t *node, dns_name_t *name) {
	dns_name_t current;
	isc_result_t result;

	REQUIRE(node != NULL);
	REQUIRE(name != NULL);
	REQUIRE(name->buffer != NULL);

	dns_name_init(&current, NULL);
	dns_name_reset(name);

	do {
		INSIST(node != NULL);

		NODENAME(node, &current);

		result = dns_name_concatenate(name, &current, name, NULL);
		if (result != ISC_R_SUCCESS)
			break;
		
		node = find_up(node);
	} while (! dns_name_isabsolute(name));

	return (result);
}

char *
dns_rbt_formatnodename(dns_rbtnode_t *node, char *printname, unsigned int size)
{
	dns_fixedname_t fixedname;
	dns_name_t *name;
	isc_result_t result;

	REQUIRE(node != NULL);
	REQUIRE(printname != NULL);

	dns_fixedname_init(&fixedname);
	name = dns_fixedname_name(&fixedname);
	result = dns_rbt_fullnamefromnode(node, name);
	if (result == ISC_R_SUCCESS)
		dns_name_format(name, printname, size);
	else
		snprintf(printname, sizeof(printname),
			 "<error building name: %s>",
			 dns_result_totext(result));

	return (printname);
}

static isc_result_t
create_node(isc_mem_t *mctx, dns_name_t *name, dns_rbtnode_t **nodep) {
	dns_rbtnode_t *node;
	isc_region_t region;
	unsigned int labels;

	REQUIRE(name->offsets != NULL);

	dns_name_toregion(name, &region);
	labels = dns_name_countlabels(name);
	ENSURE(labels > 0);

	/*
	 * Allocate space for the node structure, the name, and the offsets.
	 */
	node = (dns_rbtnode_t *)isc_mem_get(mctx, sizeof(*node) +
					    region.length + labels);

	if (node == NULL)
		return (ISC_R_NOMEMORY);

	node->is_root = 0;
	PARENT(node) = NULL;
	RIGHT(node) = NULL;
	LEFT(node) = NULL;
	DOWN(node) = NULL;
	DATA(node) = NULL;
#ifdef DNS_RBT_USEHASH
	HASHNEXT(node) = NULL;
	HASHVAL(node) = 0;
#endif

	LOCKNUM(node) = 0;
	REFS(node) = 0;
	WILD(node) = 0;
	DIRTY(node) = 0;
	node->find_callback = 0;

	MAKE_BLACK(node);

	/*
	 * The following is stored to make reconstructing a name from the
	 * stored value in the node easy:  the length of the name, the number
	 * of labels, whether the name is absolute or not, the name itself,
	 * and the name's offsets table.
	 *
	 * XXX RTH
	 *	The offsets table could be made smaller by eliminating the
	 *	first offset, which is always 0.  This requires changes to
	 * 	lib/dns/name.c.
	 */
	NAMELEN(node) = region.length;
	PADBYTES(node) = 0;
	OFFSETLEN(node) = labels;
	ATTRS(node) = name->attributes;

	memcpy(NAME(node), region.base, region.length);
	memcpy(OFFSETS(node), name->offsets, labels);

	*nodep = node;

	return (ISC_R_SUCCESS);
}

#ifdef DNS_RBT_USEHASH
static inline void
hash_add_node(dns_rbt_t *rbt, dns_rbtnode_t *node) {
	unsigned int hash;

	compute_node_hash(node);

	hash = HASHVAL(node) % rbt->hashsize;
	HASHNEXT(node) = rbt->hashtable[hash];

	rbt->hashtable[hash] = node;
}

static isc_result_t
inithash(dns_rbt_t *rbt) {
	unsigned int bytes;

	rbt->hashsize = RBT_HASH_SIZE;
	bytes = rbt->hashsize * sizeof(dns_rbtnode_t *);
	rbt->hashtable = isc_mem_get(rbt->mctx, bytes);

	if (rbt->hashtable == NULL)
		return (ISC_R_NOMEMORY);

	memset(rbt->hashtable, 0, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
rehash(dns_rbt_t *rbt) {
	unsigned int oldsize;
	dns_rbtnode_t **oldtable;
	dns_rbtnode_t *node;
	unsigned int hash;
	unsigned int i;

	oldsize = rbt->hashsize;
	rbt->hashsize *= 2;
	oldtable = rbt->hashtable;
	rbt->hashtable = isc_mem_get(rbt->mctx,
				     rbt->hashsize * sizeof(dns_rbtnode_t *));
	if (rbt->hashtable == NULL)
		return (ISC_R_NOMEMORY);

	for (i = 0; i < rbt->hashsize; i++)
		rbt->hashtable[i] = NULL;

	for (i = 0; i < oldsize; i++) {
		node = oldtable[i];
		while (node != NULL) {
			hash = HASHVAL(node) % rbt->hashsize;
			oldtable[i] = HASHNEXT(node);
			HASHNEXT(node) = rbt->hashtable[hash];
			rbt->hashtable[hash] = node;
			node = oldtable[i];
		}
	}

	isc_mem_put(rbt->mctx, oldtable,
		    rbt->hashsize * sizeof(dns_rbtnode_t *) / 2);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
hash_node(dns_rbt_t *rbt, dns_rbtnode_t *node) {
	isc_result_t result = ISC_R_SUCCESS;

	if (rbt->hashtable == NULL)
		result = inithash(rbt);
	else if (rbt->nodecount >= rbt->hashsize)
		result = rehash(rbt);

	if (result == ISC_R_SUCCESS)
		hash_add_node(rbt, node);

	return (result);
}

static inline void
unhash_node(dns_rbt_t *rbt, dns_rbtnode_t *node) {
	unsigned int bucket;
	dns_rbtnode_t *bucket_node;

	if (rbt->hashtable != NULL) {
		bucket = HASHVAL(node) % rbt->hashsize;
		bucket_node = rbt->hashtable[bucket];

		if (bucket_node == node)
			rbt->hashtable[bucket] = HASHNEXT(node);
		else {
			while (HASHNEXT(bucket_node) != node) {
				INSIST(HASHNEXT(bucket_node) != NULL);
				bucket_node = HASHNEXT(bucket_node);
			}
			HASHNEXT(bucket_node) = HASHNEXT(node);
		}
	}
}
#endif /* DNS_RBT_USEHASH */

static inline void
rotate_left(dns_rbtnode_t *node, dns_rbtnode_t **rootp) {
	dns_rbtnode_t *child;

	REQUIRE(node != NULL);
	REQUIRE(rootp != NULL);

	child = RIGHT(node);
	INSIST(child != NULL);

	RIGHT(node) = LEFT(child);
	if (LEFT(child) != NULL)
		PARENT(LEFT(child)) = node;
	LEFT(child) = node;

	if (child != NULL)
		PARENT(child) = PARENT(node);

	if (IS_ROOT(node)) {
		*rootp = child;
		child->is_root = 1;
		node->is_root = 0;

	} else {
		if (LEFT(PARENT(node)) == node)
			LEFT(PARENT(node)) = child;
		else
			RIGHT(PARENT(node)) = child;
	}

	PARENT(node) = child;
}

static inline void
rotate_right(dns_rbtnode_t *node, dns_rbtnode_t **rootp) {
	dns_rbtnode_t *child;

	REQUIRE(node != NULL);
	REQUIRE(rootp != NULL);

	child = LEFT(node);
	INSIST(child != NULL);

	LEFT(node) = RIGHT(child);
	if (RIGHT(child) != NULL)
		PARENT(RIGHT(child)) = node;
	RIGHT(child) = node;

	if (child != NULL)
		PARENT(child) = PARENT(node);

	if (IS_ROOT(node)) {
		*rootp = child;
		child->is_root = 1;
		node->is_root = 0;

	} else {
		if (LEFT(PARENT(node)) == node)
			LEFT(PARENT(node)) = child;
		else
			RIGHT(PARENT(node)) = child;
	}

	PARENT(node) = child;
}

/*
 * This is the real workhorse of the insertion code, because it does the
 * true red/black tree on a single level.
 */
static void
dns_rbt_addonlevel(dns_rbtnode_t *node, dns_rbtnode_t *current, int order, 
		   dns_rbtnode_t **rootp)
{
	dns_rbtnode_t *child, *root, *parent, *grandparent;
	dns_name_t add_name, current_name;
	dns_offsets_t add_offsets, current_offsets;

	REQUIRE(rootp != NULL);
	REQUIRE(node != NULL && LEFT(node) == NULL && RIGHT(node) == NULL);
	REQUIRE(current != NULL);

	root = *rootp;
	if (root == NULL) {
		/*
		 * First node of a level.
		 */
		MAKE_BLACK(node);
		node->is_root = 1;
		PARENT(node) = current;
		*rootp = node;
		return;
	}

	child = root;

	dns_name_init(&add_name, add_offsets);
	NODENAME(node, &add_name);

	dns_name_init(&current_name, current_offsets);
	NODENAME(current, &current_name);

	if (order < 0) {
		INSIST(LEFT(current) == NULL);
		LEFT(current) = node;
	} else {
		INSIST(RIGHT(current) == NULL);
		RIGHT(current) = node;
	}

	INSIST(PARENT(node) == NULL);
	PARENT(node) = current;

	MAKE_RED(node);

	while (node != root && IS_RED(PARENT(node))) {
		/*
		 * XXXDCL could do away with separate parent and grandparent
		 * variables.  They are vestiges of the days before parent
		 * pointers.  However, they make the code a little clearer.
		 */

		parent = PARENT(node);
		grandparent = PARENT(parent);

		if (parent == LEFT(grandparent)) {
			child = RIGHT(grandparent);
			if (child != NULL && IS_RED(child)) {
				MAKE_BLACK(parent);
				MAKE_BLACK(child);
				MAKE_RED(grandparent);
				node = grandparent;
			} else {
				if (node == RIGHT(parent)) {
					rotate_left(parent, &root);
					node = parent;
					parent = PARENT(node);
					grandparent = PARENT(parent);
				}
				MAKE_BLACK(parent);
				MAKE_RED(grandparent);
				rotate_right(grandparent, &root);
			}
		} else {
			child = LEFT(grandparent);
			if (child != NULL && IS_RED(child)) {
				MAKE_BLACK(parent);
				MAKE_BLACK(child);
				MAKE_RED(grandparent);
				node = grandparent;
			} else {
				if (node == LEFT(parent)) {
					rotate_right(parent, &root);
					node = parent;
					parent = PARENT(node);
					grandparent = PARENT(parent);
				}
				MAKE_BLACK(parent);
				MAKE_RED(grandparent);
				rotate_left(grandparent, &root);
			}
		}
	}

	MAKE_BLACK(root);
	ENSURE(IS_ROOT(root));
	*rootp = root;

	return;
}

/*
 * This is the real workhorse of the deletion code, because it does the
 * true red/black tree on a single level.
 */
static void
dns_rbt_deletefromlevel(dns_rbtnode_t *delete, dns_rbtnode_t **rootp) {
	dns_rbtnode_t *child, *sibling, *parent;
	dns_rbtnode_t *successor;

	REQUIRE(delete != NULL);

	/*
	 * Verify that the parent history is (apparently) correct.
	 */
	INSIST((IS_ROOT(delete) && *rootp == delete) ||
	       (! IS_ROOT(delete) &&
		(LEFT(PARENT(delete)) == delete ||
		 RIGHT(PARENT(delete)) == delete)));

	child = NULL;

	if (LEFT(delete) == NULL) {
		if (RIGHT(delete) == NULL) {
			if (IS_ROOT(delete)) {
				/*
				 * This is the only item in the tree.
				 */
				*rootp = NULL;
				return;
			}
		} else
			/*
			 * This node has one child, on the right.
			 */
			child = RIGHT(delete);

	} else if (RIGHT(delete) == NULL)
		/*
		 * This node has one child, on the left.
		 */
		child = LEFT(delete);

	else {
		dns_rbtnode_t holder, *tmp = &holder;

		/*
		 * This node has two children, so it cannot be directly
		 * deleted.  Find its immediate in-order successor and
		 * move it to this location, then do the deletion at the
		 * old site of the successor.
		 */
		successor = RIGHT(delete);
		while (LEFT(successor) != NULL)
			successor = LEFT(successor);

		/*
		 * The successor cannot possibly have a left child;
		 * if there is any child, it is on the right.
		 */
		if (RIGHT(successor) != NULL)
			child = RIGHT(successor);

		/* Swap the two nodes; it would be simpler to just replace
		 * the value being deleted with that of the successor,
		 * but this rigamarole is done so the caller has complete
		 * control over the pointers (and memory allocation) of
		 * all of nodes.  If just the key value were removed from
		 * the tree, the pointer to the node would be unchanged.
		 */

		/*
		 * First, put the successor in the tree location of the
		 * node to be deleted.  Save its existing tree pointer
		 * information, which will be needed when linking up
		 * delete to the successor's old location.
		 */
		memcpy(tmp, successor, sizeof(dns_rbtnode_t));

		if (IS_ROOT(delete)) {
			*rootp = successor;
			successor->is_root = ISC_TRUE;
			delete->is_root = ISC_FALSE;

		} else
			if (LEFT(PARENT(delete)) == delete)
				LEFT(PARENT(delete)) = successor;
			else
				RIGHT(PARENT(delete)) = successor;

		PARENT(successor) = PARENT(delete);
		LEFT(successor)   = LEFT(delete);
		RIGHT(successor)  = RIGHT(delete);
		COLOR(successor)  = COLOR(delete);

		if (LEFT(successor) != NULL)
			PARENT(LEFT(successor)) = successor;
		if (RIGHT(successor) != successor)
			PARENT(RIGHT(successor)) = successor;

		/*
		 * Now relink the node to be deleted into the
		 * successor's previous tree location.  PARENT(tmp)
		 * is the successor's original parent.
		 */
		INSIST(! IS_ROOT(delete));

		if (PARENT(tmp) == delete) {
			/*
			 * Node being deleted was successor's parent.
			 */
			RIGHT(successor) = delete;
			PARENT(delete) = successor;

		} else {
			LEFT(PARENT(tmp)) = delete;
			PARENT(delete) = PARENT(tmp);
		}

		/*
		 * Original location of successor node has no left.
		 */
		LEFT(delete)   = NULL;
		RIGHT(delete)  = RIGHT(tmp);
		COLOR(delete)  = COLOR(tmp);
	}

	/*
	 * Remove the node by removing the links from its parent.
	 */
	if (! IS_ROOT(delete)) {
		if (LEFT(PARENT(delete)) == delete)
			LEFT(PARENT(delete)) = child;
		else
			RIGHT(PARENT(delete)) = child;

		if (child != NULL)
			PARENT(child) = PARENT(delete);

	} else {
		/*
		 * This is the root being deleted, and at this point
		 * it is known to have just one child.
		 */
		*rootp = child;
		child->is_root = 1;
		PARENT(child) = PARENT(delete);
	}

	/*
	 * Fix color violations.
	 */
	if (IS_BLACK(delete)) {
		parent = PARENT(delete);

		while (child != *rootp && IS_BLACK(child)) {
			INSIST(child == NULL || ! IS_ROOT(child));

			if (LEFT(parent) == child) {
				sibling = RIGHT(parent);

				if (IS_RED(sibling)) {
					MAKE_BLACK(sibling);
					MAKE_RED(parent);
					rotate_left(parent, rootp);
					sibling = RIGHT(parent);
				}

				if (IS_BLACK(LEFT(sibling)) &&
				    IS_BLACK(RIGHT(sibling))) {
					MAKE_RED(sibling);
					child = parent;

				} else {

					if (IS_BLACK(RIGHT(sibling))) {
						MAKE_BLACK(LEFT(sibling));
						MAKE_RED(sibling);
						rotate_right(sibling, rootp);
						sibling = RIGHT(parent);
					}

					COLOR(sibling) = COLOR(parent);
					MAKE_BLACK(parent);
					MAKE_BLACK(RIGHT(sibling));
					rotate_left(parent, rootp);
					child = *rootp;
				}

			} else {
				/*
				 * Child is parent's right child.
				 * Everything is doen the same as above,
				 * except mirrored.
				 */
				sibling = LEFT(parent);

				if (IS_RED(sibling)) {
					MAKE_BLACK(sibling);
					MAKE_RED(parent);
					rotate_right(parent, rootp);
					sibling = LEFT(parent);
				}

				if (IS_BLACK(LEFT(sibling)) &&
				    IS_BLACK(RIGHT(sibling))) {
					MAKE_RED(sibling);
					child = parent;

				} else {
					if (IS_BLACK(LEFT(sibling))) {
						MAKE_BLACK(RIGHT(sibling));
						MAKE_RED(sibling);
						rotate_left(sibling, rootp);
						sibling = LEFT(parent);
					}

					COLOR(sibling) = COLOR(parent);
					MAKE_BLACK(parent);
					MAKE_BLACK(LEFT(sibling));
					rotate_right(parent, rootp);
					child = *rootp;
				}
			}

			parent = PARENT(child);
		}

		if (IS_RED(child))
			MAKE_BLACK(child);
	}
}

/*
 * This should only be used on the root of a tree, because no color fixup
 * is done at all.
 *
 * NOTE: No root pointer maintenance is done, because the function is only
 * used for two cases:
 * + deleting everything DOWN from a node that is itself being deleted, and
 * + deleting the entire tree of trees from dns_rbt_destroy.
 * In each case, the root pointer is no longer relevant, so there
 * is no need for a root parameter to this function.
 *
 * If the function is ever intended to be used to delete something where
 * a pointer needs to be told that this tree no longer exists,
 * this function would need to adjusted accordingly.
 */
static void
dns_rbt_deletetree(dns_rbt_t *rbt, dns_rbtnode_t *node) {
	REQUIRE(VALID_RBT(rbt));

	if (node == NULL)
		return;

	if (LEFT(node) != NULL)
		dns_rbt_deletetree(rbt, LEFT(node));
	if (RIGHT(node) != NULL)
		dns_rbt_deletetree(rbt, RIGHT(node));
	if (DOWN(node) != NULL)
		dns_rbt_deletetree(rbt, DOWN(node));

	if (DATA(node) != NULL && rbt->data_deleter != NULL)
		rbt->data_deleter(DATA(node), rbt->deleter_arg);

	unhash_node(rbt, node);
	isc_mem_put(rbt->mctx, node, NODE_SIZE(node));
	rbt->nodecount--;
}

static void
dns_rbt_indent(int depth) {
	int i;

	for (i = 0; i < depth; i++)
		putchar('\t');
}

static void
dns_rbt_printnodename(dns_rbtnode_t *node) {
	isc_buffer_t target;
	isc_region_t r;
	dns_name_t name;
	char buffer[1024];
	dns_offsets_t offsets;

	r.length = NAMELEN(node);
	r.base = NAME(node);

	dns_name_init(&name, offsets);
	dns_name_fromregion(&name, &r);

	isc_buffer_init(&target, buffer, 255);

	/*
	 * ISC_FALSE means absolute names have the final dot added.
	 */
	dns_name_totext(&name, ISC_FALSE, &target);

	printf("%.*s", (int)target.used, (char *)target.base);
}

static void
dns_rbt_printtree(dns_rbtnode_t *root, dns_rbtnode_t *parent, int depth) {
	dns_rbt_indent(depth);

	if (root != NULL) {
		dns_rbt_printnodename(root);
		printf(" (%s", IS_RED(root) ? "RED" : "black");
		if (parent) {
			printf(" from ");
			dns_rbt_printnodename(parent);
		}

		if ((! IS_ROOT(root) && PARENT(root) != parent) ||
		    (  IS_ROOT(root) && depth > 0 &&
		       DOWN(PARENT(root)) != root)) {

			printf(" (BAD parent pointer! -> ");
			if (PARENT(root) != NULL)
				dns_rbt_printnodename(PARENT(root));
			else
				printf("NULL");
			printf(")");
		}

		printf(")\n");


		depth++;

		if (DOWN(root)) {
			dns_rbt_indent(depth);
			printf("++ BEG down from ");
			dns_rbt_printnodename(root);
			printf("\n");
			dns_rbt_printtree(DOWN(root), NULL, depth);
			dns_rbt_indent(depth);
			printf("-- END down from ");
			dns_rbt_printnodename(root);
			printf("\n");
		}

		if (IS_RED(root) && IS_RED(LEFT(root)))
		    printf("** Red/Red color violation on left\n");
		dns_rbt_printtree(LEFT(root), root, depth);

		if (IS_RED(root) && IS_RED(RIGHT(root)))
		    printf("** Red/Red color violation on right\n");
		dns_rbt_printtree(RIGHT(root), root, depth);

	} else
		printf("NULL\n");
}

void
dns_rbt_printall(dns_rbt_t *rbt) {
	REQUIRE(VALID_RBT(rbt));

	dns_rbt_printtree(rbt->root, NULL, 0);
}

/*
 * Chain Functions
 */

void
dns_rbtnodechain_init(dns_rbtnodechain_t *chain, isc_mem_t *mctx) {
	/*
	 * Initialize 'chain'.
	 */

	REQUIRE(chain != NULL);

	chain->mctx = mctx;
	chain->end = NULL;
	chain->level_count = 0;
	chain->level_matches = 0;

	chain->magic = CHAIN_MAGIC;
}

isc_result_t
dns_rbtnodechain_current(dns_rbtnodechain_t *chain, dns_name_t *name,
			 dns_name_t *origin, dns_rbtnode_t **node)
{
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(VALID_CHAIN(chain));

	if (node != NULL)
		*node = chain->end;

	if (chain->end == NULL)
		return (ISC_R_NOTFOUND);

	if (name != NULL) {
		NODENAME(chain->end, name);

		if (chain->level_count == 0) {
			/*
			 * Names in the top level tree are all absolute.
			 * Always make 'name' relative.
			 */
			INSIST(dns_name_isabsolute(name));

			/*
			 * This is cheaper than dns_name_getlabelsequence().
			 */
			name->labels--;
			name->length--;
			name->attributes &= ~DNS_NAMEATTR_ABSOLUTE;
		}
	}

	if (origin != NULL) {
		if (chain->level_count > 0)
			result = chain_name(chain, origin, ISC_FALSE);
		else
			result = dns_name_copy(dns_rootname, origin, NULL);
	}

	return (result);
}

isc_result_t
dns_rbtnodechain_prev(dns_rbtnodechain_t *chain, dns_name_t *name,
		      dns_name_t *origin)
{
	dns_rbtnode_t *current, *previous, *predecessor;
	isc_result_t result = ISC_R_SUCCESS;
	isc_boolean_t new_origin = ISC_FALSE;

	REQUIRE(VALID_CHAIN(chain) && chain->end != NULL);

	predecessor = NULL;

	current = chain->end;

	if (LEFT(current) != NULL) {
		/*
		 * Moving left one then right as far as possible is the
		 * previous node, at least for this level.
		 */
		current = LEFT(current);

		while (RIGHT(current) != NULL)
			current = RIGHT(current);

		predecessor = current;

	} else {
		/*
		 * No left links, so move toward the root.  If at any point on
		 * the way there the link from parent to child is a right
		 * link, then the parent is the previous node, at least
		 * for this level.
		 */
		while (! IS_ROOT(current)) {
			previous = current;
			current = PARENT(current);

			if (RIGHT(current) == previous) {
				predecessor = current;
				break;
			}
		}
	}

	if (predecessor != NULL) {
		/*
		 * Found a predecessor node in this level.  It might not
		 * really be the predecessor, however.
		 */
		if (DOWN(predecessor) != NULL) {
			/*
			 * The predecessor is really down at least one level.
			 * Go down and as far right as possible, and repeat
			 * as long as the rightmost node has a down pointer.
			 */
			do {
				/*
				 * XXX DCL Need to do something about origins
				 * here. See whether to go down, and if so
				 * whether it is truly what Bob calls a
				 * new origin.
				 */
				ADD_LEVEL(chain, predecessor);
				predecessor = DOWN(predecessor);

				/* XXX DCL duplicated from above; clever
				 * way to unduplicate? */

				while (RIGHT(predecessor) != NULL)
					predecessor = RIGHT(predecessor);
			} while (DOWN(predecessor) != NULL);

			/* XXX DCL probably needs work on the concept */
			if (origin != NULL)
				new_origin = ISC_TRUE;
		}

	} else if (chain->level_count > 0) {
		/*
		 * Dang, didn't find a predecessor in this level.
		 * Got to the root of this level without having traversed
		 * any right links.  Ascend the tree one level; the
		 * node that points to this tree is the predecessor.
		 */
		INSIST(chain->level_count > 0 && IS_ROOT(current));
		predecessor = chain->levels[--chain->level_count];

		/* XXX DCL probably needs work on the concept */
		/*
		 * Don't declare an origin change when the new origin is "."
		 * at the top level tree, because "." is declared as the origin
		 * for the second level tree.
		 */
		if (origin != NULL &&
		    (chain->level_count > 0 || OFFSETLEN(predecessor) > 1))
			new_origin = ISC_TRUE;
	}

	if (predecessor != NULL) {
		chain->end = predecessor;

		if (new_origin) {
			result = dns_rbtnodechain_current(chain, name, origin,
							  NULL);
			if (result == ISC_R_SUCCESS)
				result = DNS_R_NEWORIGIN;

		} else
			result = dns_rbtnodechain_current(chain, name, NULL,
							  NULL);

	} else
		result = ISC_R_NOMORE;

	return (result);
}

isc_result_t
dns_rbtnodechain_next(dns_rbtnodechain_t *chain, dns_name_t *name,
		      dns_name_t *origin)
{
	dns_rbtnode_t *current, *previous, *successor;
	isc_result_t result = ISC_R_SUCCESS;
	isc_boolean_t new_origin = ISC_FALSE;

	REQUIRE(VALID_CHAIN(chain) && chain->end != NULL);

	successor = NULL;

	current = chain->end;

	/*
	 * If there is a level below this node, the next node is the leftmost
	 * node of the next level.
	 */
	if (DOWN(current) != NULL) {
		/*
		 * Don't declare an origin change when the new origin is "."
		 * at the second level tree, because "." is already declared
		 * as the origin for the top level tree.
		 */
		if (chain->level_count > 0 ||
		    OFFSETLEN(current) > 1)
			new_origin = ISC_TRUE;

		ADD_LEVEL(chain, current);
		current = DOWN(current);

		while (LEFT(current) != NULL)
			current = LEFT(current);

		successor = current;

	} else if (RIGHT(current) == NULL) {
		/*
		 * The successor is up, either in this level or a previous one.
		 * Head back toward the root of the tree, looking for any path
		 * that was via a left link; the successor is the node that has
		 * that left link.  In the event the root of the level is
		 * reached without having traversed any left links, ascend one
		 * level and look for either a right link off the point of
		 * ascent, or search for a left link upward again, repeating
		 * ascents until either case is true.
		 */
		do {
			while (! IS_ROOT(current)) {
				previous = current;
				current = PARENT(current);

				if (LEFT(current) == previous) {
					successor = current;
					break;
				}
			}

			if (successor == NULL) {
				/*
				 * Reached the root without having traversed
				 * any left pointers, so this level is done.
				 */
				if (chain->level_count == 0)
					break;

				current = chain->levels[--chain->level_count];
				new_origin = ISC_TRUE;

				if (RIGHT(current) != NULL)
					break;
			}
		} while (successor == NULL);
	}

	if (successor == NULL && RIGHT(current) != NULL) {
		current = RIGHT(current);

		while (LEFT(current) != NULL)
			current = LEFT(current);

		successor = current;
	}

	if (successor != NULL) {
		chain->end = successor;

		/*
		 * It is not necessary to use dns_rbtnodechain_current like
		 * the other functions because this function will never
		 * find a node in the topmost level.  This is because the
		 * root level will never be more than one name, and everything
		 * in the megatree is a successor to that node, down at
		 * the second level or below.
		 */

		if (name != NULL)
			NODENAME(chain->end, name);

		if (new_origin) {
			if (origin != NULL)
				result = chain_name(chain, origin, ISC_FALSE);

			if (result == ISC_R_SUCCESS)
				result = DNS_R_NEWORIGIN;

		} else
			result = ISC_R_SUCCESS;

	} else
		result = ISC_R_NOMORE;

	return (result);
}

isc_result_t
dns_rbtnodechain_first(dns_rbtnodechain_t *chain, dns_rbt_t *rbt,
		       dns_name_t *name, dns_name_t *origin)

{
	isc_result_t result;

	REQUIRE(VALID_RBT(rbt));
	REQUIRE(VALID_CHAIN(chain));

	dns_rbtnodechain_reset(chain);

	chain->end = rbt->root;

	result = dns_rbtnodechain_current(chain, name, origin, NULL);

	if (result == ISC_R_SUCCESS)
		result = DNS_R_NEWORIGIN;

	return (result);
}

isc_result_t
dns_rbtnodechain_last(dns_rbtnodechain_t *chain, dns_rbt_t *rbt,
		       dns_name_t *name, dns_name_t *origin)

{
	isc_result_t result;

	REQUIRE(VALID_RBT(rbt));
	REQUIRE(VALID_CHAIN(chain));

	dns_rbtnodechain_reset(chain);

	result = move_chain_to_last(chain, rbt->root);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_rbtnodechain_current(chain, name, origin, NULL);

	if (result == ISC_R_SUCCESS)
		result = DNS_R_NEWORIGIN;

	return (result);
}


void
dns_rbtnodechain_reset(dns_rbtnodechain_t *chain) {
	/*
	 * Free any dynamic storage associated with 'chain', and then
	 * reinitialize 'chain'.
	 */

	REQUIRE(VALID_CHAIN(chain));

	chain->end = NULL;
	chain->level_count = 0;
	chain->level_matches = 0;
}

void
dns_rbtnodechain_invalidate(dns_rbtnodechain_t *chain) {
	/*
	 * Free any dynamic storage associated with 'chain', and then
	 * invalidate 'chain'.
	 */

	dns_rbtnodechain_reset(chain);

	chain->magic = 0;
}
