/*
 * udbradtree -- radix tree for binary strings for in udb file.
 *
 * Copyright (c) 2011, NLnet Labs.  See LICENSE for license.
 */
#ifndef UDB_RADTREE_H
#define UDB_RADTREE_H
#include "udb.h"
struct udb_radnode;

/** length of the binary string */
typedef uint16_t udb_radstrlen_t;

/**
 * The radix tree
 *
 * The elements are stored based on binary strings(0-255) of a given length.
 * They are sorted, a prefix is sorted before its suffixes.
 * If you want to know the key string, you should store it yourself, the
 * tree stores it in the parts necessary for lookup.
 * For binary strings for domain names see the radname routines.
 *
 * This is the tree on disk representation.  It has _d suffix in the name
 * to help delineate disk structures from normal structures.
 */
struct udb_radtree_d {
	/** root node in tree, to udb_radnode_d */
	struct udb_rel_ptr root;
	/** count of number of elements */
	uint64_t count;
};

/**
 * A radix tree lookup node.  It is stored on disk, and the lookup array
 * is allocated.
 */
struct udb_radnode_d {
	/** data element associated with the binary string up to this node */
	struct udb_rel_ptr elem;
	/** parent node (NULL for the root), to udb_radnode_d */
	struct udb_rel_ptr parent;
	/** the  array structure, for lookup by [byte-offset]. udb_radarray_d */
	struct udb_rel_ptr lookup;
	/** index in the parent lookup array */
	uint8_t pidx;
	/** offset of the lookup array, add to [i] for lookups */
	uint8_t offset;
};

/**
 * radix select edge in array
 * The string for this element is the Nth string in the stringarray.
 */
struct udb_radsel_d {
	/** length of the additional string for this edge,
	 * additional string after the selection-byte for this edge.*/
	udb_radstrlen_t len;
	/** padding for non64bit compilers to 64bit boundaries, to make
	 * the udb file more portable, without this the file would work
	 * on the system it is created on (which is what we promise), but
	 * with this, you have a chance of it working on other platforms */
	uint16_t padding16;
	uint32_t padding32;
	/** node that deals with byte+str, to udb_radnode_d */
	struct udb_rel_ptr node;
};

/**
 * Array of radsel elements.
 * This is the header, the array is allocated contiguously behind it.
 * The strings (often very short) are allocated behind the array.
 * All strings are given the same amount of space (str_cap),
 * so there is capacity*str_cap bytes at the end.
 */
struct udb_radarray_d {
	/** length of the lookup array */
	uint16_t len;
	/** capacity of the lookup array (can be larger than length) */
	uint16_t capacity;
	/** space capacity of for every string */
	udb_radstrlen_t str_cap;
	/** padding to 64bit alignment, just in case compiler goes mad */
	uint16_t padding;
	/** the elements (allocated contiguously after this structure) */
	struct udb_radsel_d array[0];
};

/**
 * Create new radix tree on udb storage
 * @param udb: the udb to allocate space on.
 * @param ptr: ptr to the udbradtree is returned here.  Pass uninitialised.
 * 	type is udb_radtree_d.
 * @return 0 on alloc failure.
 */
int udb_radix_tree_create(udb_base* udb, udb_ptr* ptr);

/**
 * Delete intermediate nodes from radix tree
 * @param udb: the udb.
 * @param rt: radix tree to be cleared. type udb_radtree_d.
 */
void udb_radix_tree_clear(udb_base* udb, udb_ptr* rt);

/**
 * Delete radix tree.
 * You must have deleted the elements, this deletes the nodes.
 * @param udb: the udb.
 * @param rt: radix tree to be deleted. type udb_radtree_d.
 */
void udb_radix_tree_delete(udb_base* udb, udb_ptr* rt);

/**
 * Insert element into radix tree.
 * @param udb: the udb.
 * @param rt: the radix tree, type udb_radtree_d.
 * @param key: key string.
 * @param len: length of key.
 * @param elem: pointer to element data, on the udb store.
 * @param result: the inserted node is set to this value.  Pass uninitialised.
	Not set if the routine fails.
 * @return NULL on failure - out of memory.
 * 	NULL on failure - duplicate entry.
 * 	On success the new radix node for this element (udb_radnode_d).
 */
udb_void udb_radix_insert(udb_base* udb, udb_ptr* rt, uint8_t* k,
	udb_radstrlen_t len, udb_ptr* elem, udb_ptr* result);

/**
 * Delete element from radix tree.
 * @param udb: the udb.
 * @param rt: the radix tree. type udb_radtree_d
 * @param n: radix node for that element. type udb_radnode_d
 * 	if NULL, nothing is deleted.
 */
void udb_radix_delete(udb_base* udb, udb_ptr* rt, udb_ptr* n);

/**
 * Find radix element in tree.
 * @param rt: the radix tree, type udb_radtree_d.
 * @param key: key string.
 * @param len: length of key.
 * @return the radix node or NULL if not found. type udb_radnode_d
 */
udb_void udb_radix_search(udb_ptr* rt, uint8_t* k,
	udb_radstrlen_t len);

/**
 * Find radix element in tree, and if not found, find the closest smaller or
 * equal element in the tree.
 * @param udb: the udb.
 * @param rt: the radix tree, type udb_radtree_d.
 * @param key: key string.
 * @param len: length of key.
 * @param result: returns the radix node or closest match (NULL if key is
 * 	smaller than the smallest key in the tree). type udb_radnode_d.
 * 	you can pass an uninitialized ptr, an unlinked or a zeroed one.
 * @return true if exact match, false if no match.
 */
int udb_radix_find_less_equal(udb_base* udb, udb_ptr* rt, uint8_t* k,
	udb_radstrlen_t len, udb_ptr* result);

/**
 * Return the first (smallest) element in the tree.
 * @param udb: the udb.
 * @param rt: the radix tree, type udb_radtree_d.
 * @param p: set to the first node in the tree, or NULL if none.
 * 	type udb_radnode_d.
 * 	pass uninitialised, zero or unlinked udb_ptr.
 */
void udb_radix_first(udb_base* udb, udb_ptr* rt, udb_ptr* p);

/**
 * Return the last (largest) element in the tree.
 * @param udb: the udb.
 * @param rt: the radix tree, type udb_radtree_d.
 * @param p: last node or NULL if none, type udb_radnode_d.
 * 	pass uninitialised, zero or unlinked udb_ptr.
 */
void udb_radix_last(udb_base* udb, udb_ptr* rt, udb_ptr* p);

/**
 * Return the next element.
 * @param udb: the udb.
 * @param n: adjusted to the next element, or NULL if none. type udb_radnode_d.
 */
void udb_radix_next(udb_base* udb, udb_ptr* n);

/**
 * Return the previous element.
 * @param udb: the udb.
 * @param n: adjusted to the prev node or NULL if none. type udb_radnode_d.
 */
void udb_radix_prev(udb_base* udb, udb_ptr* n);

/*
 * Perform a walk through all elements of the tree.
 * node: variable of type struct radnode*.
 * tree: pointer to the tree.
 * for(udb_radix_first(tree, node); node->data; udb_radix_next(node))
*/

/** for use in udb-walkfunc, walks relptrs in udb_chunk_type_radtree */
void udb_radix_tree_walk_chunk(void* base, void* d, uint64_t s,
	udb_walk_relptr_cb* cb, void* arg);

/** for use in udb-walkfunc, walks relptrs in udb_chunk_type_radnode */
void udb_radix_node_walk_chunk(void* base, void* d, uint64_t s,
	udb_walk_relptr_cb* cb, void* arg);

/** for use in udb-walkfunc, walks relptrs in udb_chunk_type_radarray */
void udb_radix_array_walk_chunk(void* base, void* d, uint64_t s,
	udb_walk_relptr_cb* cb, void* arg);

/** get the memory used by the lookup structure for a radnode */
size_t size_of_lookup_ext(udb_ptr* node);

/** insert radtree element, key is a domain name
 * @param udb: udb.
 * @param rt: the tree.
 * @param dname: domain name in uncompressed wireformat.
 * @param dlen: length of k.
 * @param elem: element to store
 * @param result: the inserted node is set to this value.  Pass uninitialised.
	Not set if the routine fails.
 * @return 0 on failure
 */
udb_void udb_radname_insert(udb_base* udb, udb_ptr* rt, const uint8_t* dname,
	size_t dlen, udb_ptr* elem, udb_ptr* result);

/** search for a radname element, key is a domain name.
 * @param udb: udb
 * @param rt: the tree
 * @param dname: domain name in uncompressed wireformat.
 * @param dlen: length of k.
 * @param result: result ptr to store the node into.
 *    may be uninitialized.
 * @return 0 if not found.
 */
int udb_radname_search(udb_base* udb, udb_ptr* rt, const uint8_t* dname,
	size_t dlen, udb_ptr* result);

#define RADNODE(ptr) ((struct udb_radnode_d*)UDB_PTR(ptr))
#define RADTREE(ptr) ((struct udb_radtree_d*)UDB_PTR(ptr))

#endif /* UDB_RADTREE_H */
