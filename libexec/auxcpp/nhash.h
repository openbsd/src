/*
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

#ifndef UCPP__NHASH__
#define UCPP__NHASH__

/*
 * Each item stored in the hash table should be a structure beginning
 * with the following header.
 */
typedef struct hash_item_header_ {
	char *ident;
	struct hash_item_header_ *left, *right;
} hash_item_header;

/*
 * This macro takes as argument a pointer to a hash table item (a
 * structure beginning with `hash_item_header') and returns a pointer to
 * the item name. This name should be considered as read-only. The
 * retrieved pointer can become invalid whenever a new item is inserted
 * in or removed from the table.
 */
#define HASH_ITEM_NAME(s) (((hash_item_header *)(s))->ident + sizeof(unsigned))

/*
 * Number of lists for the primary hash step. Can be reduced to save more
 * memory, or increased to speed things up. It should be a power of 2
 * greater or equal than 2 and smaller than UINT_MAX.
 */
#define HTT_NUM_TREES   128

/*
 * Type for a hash table.
 */
typedef struct {
	void (*deldata)(void *);
	hash_item_header *tree[HTT_NUM_TREES];
} HTT;

/*
 * Type for a reduced version of HTT with only two binary trees. That
 * version has a lower initialization time and is suitable for situation
 * where only a limited number of elements will be stored, but new tables
 * need frequent initializations.
 */
typedef struct {
	void (*deldata)(void *);
	hash_item_header *tree[2];
} HTT2;

/*
 * Initialize a hash table. The `deldata' parameter should point to a
 * function which will be invoked on any item removed from the table;
 * that function should take care of the release of memory allocated for
 * that item (except the hash_item_header contents, which are handled
 * internally).
 */
void HTT_init(HTT *htt, void (*deldata)(void *));

/*
 * Link an item into the hash table under the given name. If another
 * item of identical name is already present in the table, a pointer to
 * that item is returned; otherwise, the new item is linked into the
 * table and NULL is returned. The object pointed to by `item' is
 * linked from the table, but not the string pointed to by `name'.
 */
void *HTT_put(HTT *htt, void *item, char *name);

/*
 * Retrieve an item by name from the hash table. NULL is returned if
 * the object is not found.
 */
void *HTT_get(HTT *htt, char *name);

/*
 * Remove an item from the hash table. 1 is returned if the item was
 * removed, 0 if it was not found.
 */
int HTT_del(HTT *htt, char *name);

/*
 * For all items stored within the hash table, invoke the provided
 * function with the item as parameter. The function may abort the
 * scan by performing a longjmp() to a context encapsulating the
 * call to that function.
 */
void HTT_scan(HTT *htt, void (*action)(void *));

/*
 * Release the whole table contents. After a call to this function,
 * the table is ready to accept new items.
 */
void HTT_kill(HTT *htt);

/*
 * The following functions are identical to the HTT_*() functions, except
 * that they operate on the reduced HTT2 tables.
 */
void HTT2_init(HTT2 *htt, void (*deldata)(void *));
void *HTT2_put(HTT2 *htt, void *item, char *name);
void *HTT2_get(HTT2 *htt, char *name);
int HTT2_del(HTT2 *htt, char *name);
void HTT2_scan(HTT2 *htt, void (*action)(void *));
void HTT2_kill(HTT2 *htt);

#endif
