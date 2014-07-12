/*
 * Mixed hash table / binary tree code.
 * (c) Thomas Pornin 2002
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. The name of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "nhash.h"
#include "mem.h"

/*
 * Hash a string into an `unsigned' value. This function is derived
 * from the hash function used in the ELF binary object file format
 * hash tables. The result size is a 32-bit number if the `unsigned'
 * type is big enough to hold 32-bit arbitrary numbers, a 16-bit number
 * otherwise.
 */
static unsigned hash_string(char *name)
{
	unsigned h = 0;

	for (h = 0; *name; name ++) {
		unsigned g;

		h = (h << 4) + *(unsigned char *)name;
#if UINT_MAX >= 0xffffffffU
		g = h & 0xF0000000U;
		h ^= (g >> 24);
#else
		g = h & 0xF000U;
		h ^= (g >> 12);
#endif
		h &= ~g;
	}
	return h;
}

/*
 * Each item in the table is a structure beginning with a `hash_item_header'
 * structure. Those headers define binary trees such that all left-descendants
 * (respectively right-descendants) of a given tree node have an associated
 * hash value strictly smaller (respectively greater) than the hash value
 * associated with this node.
 *
 * The `ident' field points to an array of char. The `sizeof(unsigned)'
 * first `char' contain a copy of an `unsigned' value which is the hashed
 * string, except the least significant bit. When this bit is set to 0,
 * the node contains the unique item using that hash value. If the bit
 * is set to 1, then there are several items with that hash value.
 *
 * When several items share the same hash value, they are linked together
 * in a linked list by their `left' field. The node contains no data;
 * it is a "fake item".
 *
 * The `char' following the hash value encode the item name for true items.
 * For fake items, they contain the pointer to the first true item of the
 * corresponding link list (suitably aligned).
 *
 * There are HTT_NUM_TREES trees; the items are sorted among trees by the
 * lest significant bits of their hash value.
 */

static void internal_init(HTT *htt, void (*deldata)(void *), int reduced)
{
	htt->deldata = deldata;
	if (reduced) {
		HTT2 *htt2 = (HTT2 *)htt;

		htt2->tree[0] = htt2->tree[1] = NULL;
	} else {
		unsigned u;

		for (u = 0; u < HTT_NUM_TREES; u ++) htt->tree[u] = NULL;
	}
}

/* see nhash.h */
void HTT_init(HTT *htt, void (*deldata)(void *))
{
	internal_init(htt, deldata, 0);
}

/* see nhash.h */
void HTT2_init(HTT2 *htt, void (*deldata)(void *))
{
	internal_init((HTT *)htt, deldata, 1);
}

#define PTR_SHIFT    (sizeof(hash_item_header *) * \
                      ((sizeof(unsigned) + sizeof(hash_item_header *) - 1) / \
                       sizeof(hash_item_header *)))

#define TREE(u)    (*(reduced ? ((HTT2 *)htt)->tree + ((u) & 1) \
                              : htt->tree + ((u) & (HTT_NUM_TREES - 1))))

/*
 * Find a node for the given hash value. If `father' is not NULL, fill
 * `*father' with a pointer to the node's father.
 * If the return value is NULL, then no existing node was found; if `*father'
 * is also  NULL, the tree is empty. If the return value is not NULL but
 * `*father' is NULL, then the found node is the tree root.
 *
 * If `father' is not NULL, then `*leftson' is filled with 1 if the node
 * was looked for as the father left son, 0 otherwise.
 */
static hash_item_header *find_node(HTT *htt, unsigned u,
	hash_item_header **father, int *leftson, int reduced)
{
	hash_item_header *node = TREE(u);
	hash_item_header *nodef = NULL;
	int ls;

	u &= ~1U;
	while (node != NULL) {
		unsigned v = *(unsigned *)(node->ident);
		unsigned w = v & ~1U;

		if (u == w) break;
		nodef = node;
		if (u < w) {
			node = node->left;
			ls = 1;
		} else {
			node = node->right;
			ls = 0;
		}
	}
	if (father != NULL) {
		*father = nodef;
		*leftson = ls;
	}
	return node;
}

static void *internal_get(HTT *htt, char *name, int reduced)
{
	unsigned u = hash_string(name), v;
	hash_item_header *node = find_node(htt, u, NULL, NULL, reduced);

	if (node == NULL) return NULL;
	v = *(unsigned *)(node->ident);
	if ((v & 1U) == 0) {
		return (strcmp(HASH_ITEM_NAME(node), name) == 0) ? node : NULL;
	}
	node = *(hash_item_header **)(node->ident + PTR_SHIFT);
	while (node != NULL) {
		if (strcmp(HASH_ITEM_NAME(node), name) == 0) return node;
		node = node->left;
	}
	return NULL;
}

/* see nhash.h */
void *HTT_get(HTT *htt, char *name)
{
	return internal_get(htt, name, 0);
}

/* see nhash.h */
void *HTT2_get(HTT2 *htt, char *name)
{
	return internal_get((HTT *)htt, name, 1);
}

/*
 * Make an item identifier from its name and its hash value.
 */
static char *make_ident(char *name, unsigned u)
{
	size_t n = strlen(name) + 1;
	char *ident = getmem(n + sizeof(unsigned));

	*(unsigned *)ident = u & ~1U;
	memcpy(ident + sizeof(unsigned), name, n);
	return ident;
}

/*
 * Make an identifier for a fake item, pointing to a true item.
 */
static char *make_fake_ident(unsigned u, hash_item_header *next)
{
	char *ident = getmem(PTR_SHIFT + sizeof(hash_item_header *));

	*(unsigned *)ident = u | 1U;
	*(hash_item_header **)(ident + PTR_SHIFT) = next;
	return ident;
}

/*
 * Adding an item is straightforward:
 *  1. look for its emplacement
 *  2. if no node is found, use the item as a new node and link it to the tree
 *  3. if a node is found:
 *     3.1. if the node is real, check for name inequality, then create a
 *          fake node and assemble the two-element linked list
 *     3.2. if the node is fake, look for the name in the list; if not found,
 *          add the node at the list end
 */
static void *internal_put(HTT *htt, void *item, char *name, int reduced)
{
	unsigned u = hash_string(name), v;
	int ls;
	hash_item_header *father;
	hash_item_header *node = find_node(htt, u, &father, &ls, reduced);
	hash_item_header *itemg = item, *pnode;

	if (node == NULL) {
		itemg->left = itemg->right = NULL;
		itemg->ident = make_ident(name, u);
		if (father == NULL) {
			TREE(u) = itemg;
		} else if (ls) {
			father->left = itemg;
		} else {
			father->right = itemg;
		}
		return NULL;
	}
	v = *(unsigned *)(node->ident);
	if ((v & 1U) == 0) {
		if (strcmp(HASH_ITEM_NAME(node), name) == 0)
			return node;
		pnode = getmem(sizeof *pnode);
		pnode->left = node->left;
		pnode->right = node->right;
		pnode->ident = make_fake_ident(u, node);
		node->left = itemg;
		node->right = NULL;
		itemg->left = itemg->right = NULL;
		itemg->ident = make_ident(name, u);
		if (father == NULL) {
			TREE(u) = pnode;
		} else if (ls) {
			father->left = pnode;
		} else {
			father->right = pnode;
		}
		return NULL;
	}
	node = *(hash_item_header **)(node->ident + PTR_SHIFT);
	while (node != NULL) {
		if (strcmp(HASH_ITEM_NAME(node), name) == 0) return node;
		pnode = node;
		node = node->left;
	}
	itemg->left = itemg->right = NULL;
	itemg->ident = make_ident(name, u);
	pnode->left = itemg;
	return NULL;
}

/* see nhash.h */
void *HTT_put(HTT *htt, void *item, char *name)
{
	return internal_put(htt, item, name, 0);
}

/* see nhash.h */
void *HTT2_put(HTT2 *htt, void *item, char *name)
{
	return internal_put((HTT *)htt, item, name, 1);
}

/*
 * A fake node subnode list has shrunk to one item only; make the
 * node real again.
 *   fnode    the fake node
 *   node     the last remaining node
 *   father   the fake node father (NULL if the fake node is root)
 *   leftson  1 if the fake node is a left son, 0 otehrwise
 *   u        the hash value for this node
 */
static void shrink_node(HTT *htt, hash_item_header *fnode,
	hash_item_header *node, hash_item_header *father, int leftson,
	unsigned u, int reduced)
{
	node->left = fnode->left;
	node->right = fnode->right;
	if (father == NULL) {
		TREE(u) = node;
	} else if (leftson) {
		father->left = node;
	} else {
		father->right = node;
	}
	freemem(fnode->ident);
	freemem(fnode);
}

/*
 * Deletion algorithm:
 *  1. look for the node; if not found, exit
 *  2. if the node is real:
 *     2.1. check for equality; exit otherwise
 *     2.2. delete the node
 *     2.3. promote the leftest of right descendants or rightest of left
 *          descendants
 *  3. if the node is fake:
 *     3.1. check the list items for equality; exit otherwise
 *     3.2. delete the correct item
 *     3.3. if there remains only one item, supress the fake node
 */
static int internal_del(HTT *htt, char *name, int reduced)
{
	unsigned u = hash_string(name), v;
	int ls;
	hash_item_header *father;
	hash_item_header *node = find_node(htt, u, &father, &ls, reduced);
	hash_item_header *pnode, *fnode, *znode;
	char *tmp;

	if (node == NULL) return 0;
	v = *(unsigned *)(node->ident);
	if ((v & 1U) != 0) {
		fnode = node;
		node = znode = *(hash_item_header **)(node->ident + PTR_SHIFT);
		pnode = NULL;
		while (node != NULL) {
			if (strcmp(HASH_ITEM_NAME(node), name) == 0) break;
			pnode = node;
			node = node->left;
		}
		if (node == NULL) return 0;
		if (pnode == NULL) {
			/*
			 * We supress the first item in the list.
			 */
			*(hash_item_header **)(fnode->ident + PTR_SHIFT) =
				node->left;
			if (node->left->left == NULL) {
				shrink_node(htt, fnode, node->left,
					father, ls, u, reduced);
			}
		} else {
			pnode->left = node->left;
			if (pnode->left == NULL && znode == pnode) {
				shrink_node(htt, fnode, pnode,
					father, ls, u, reduced);
			}
		}
	} else {
		if (strcmp(HASH_ITEM_NAME(node), name) != 0) return 0;
		if (node->left != NULL) {
			for (znode = node, pnode = node->left; pnode->right;
				znode = pnode, pnode = pnode->right);
			if (znode != node) {
				znode->right = pnode->left;
				pnode->left = node->left;
			}
			pnode->right = node->right;
		} else if (node->right != NULL) {
			for (znode = node, pnode = node->right; pnode->left;
				znode = pnode, pnode = pnode->left);
			if (znode != node) {
				znode->left = pnode->right;
				pnode->right = node->right;
			}
			pnode->left = node->left;
		} else pnode = NULL;
		if (father == NULL) {
			TREE(u) = pnode;
		} else if (ls) {
			father->left = pnode;
		} else {
			father->right = pnode;
		}
	}
	tmp = node->ident;
	htt->deldata(node);
	freemem(tmp);
	return 1;
}

/* see nhash.h */
int HTT_del(HTT *htt, char *name)
{
	return internal_del(htt, name, 0);
}

/* see nhash.h */
int HTT2_del(HTT2 *htt, char *name)
{
	return internal_del((HTT *)htt, name, 1);
}

/*
 * Apply `action()' on all nodes of the tree whose root is given as
 * parameter `node'. If `wipe' is non-zero, the nodes are removed
 * from memory.
 */
static void scan_node(hash_item_header *node, void (*action)(void *), int wipe)
{
	unsigned v;

	if (node == NULL) return;
	scan_node(node->left, action, wipe);
	scan_node(node->right, action, wipe);
	v = *(unsigned *)(node->ident);
	if ((v & 1U) != 0) {
		hash_item_header *pnode, *nnode;

		for (pnode = *(hash_item_header **)(node->ident + PTR_SHIFT);
			pnode != NULL; pnode = nnode) {
			char *tmp = pnode->ident;

			nnode = pnode->left;
			action(pnode);
			if (wipe) freemem(tmp);
		}
		if (wipe) {
			freemem(node->ident);
			freemem(node);
		}
	} else {
		char *tmp = node->ident;

		action(node);
		if (wipe) freemem(tmp);
	}
}

/* see nhash.h */
void HTT_scan(HTT *htt, void (*action)(void *))
{
	unsigned u;

	for (u = 0; u < HTT_NUM_TREES; u ++) {
		scan_node(htt->tree[u], action, 0);
	}
}

/* see nhash.h */
void HTT2_scan(HTT2 *htt, void (*action)(void *))
{
	scan_node(htt->tree[0], action, 0);
	scan_node(htt->tree[1], action, 0);
}

/* see nhash.h */
void HTT_kill(HTT *htt)
{
	unsigned u;

	for (u = 0; u < HTT_NUM_TREES; u ++) {
		scan_node(htt->tree[u], htt->deldata, 1);
	}
}

/* see nhash.h */
void HTT2_kill(HTT2 *htt)
{
	scan_node(htt->tree[0], htt->deldata, 1);
	scan_node(htt->tree[1], htt->deldata, 1);
}
