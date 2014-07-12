/*
 * Generic hash table routines.
 * (c) Thomas Pornin 1998, 1999, 2000
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

#include <string.h>
#include "hash.h"
#include "mem.h"
#include "tune.h"

/*
 * hash_string() is a sample hash function for strings
 */
int hash_string(char *s)
{
#ifdef FAST_HASH
	unsigned h = 0, g;

	while (*s) {
		h = (h << 4) + *(unsigned char *)(s ++);
		if ((g = h & 0xF000U) != 0) h ^= (g >> 12);
		h &= ~g;
	}
	return (h ^ (h >> 9)) & 127U;
#else
	unsigned char h = 0;

	for (; *s; s ++) h ^= (unsigned char)(*s);
	return ((int)h);
#endif
}

/*
 * struct hash_item is the basic data type to internally handle hash tables
 */
struct hash_item {
	void *data;
	struct hash_item *next;
};

/*
 * This function adds an entry to the struct hash_item list
 */
static struct hash_item *add_entry(struct hash_item *blist, void *data)
{
	struct hash_item *t = getmem(sizeof(struct hash_item));

	t->data = data;
	t->next = blist;
	return t;
}

/*
 * This function finds a struct hash_item in a list, using the
 * comparison function provided as cmpdata (*cmpdata() returns
 * non-zero if the two parameters are to be considered identical).
 *
 * It returns 0 if the item is not found.
 */
static struct hash_item *get_entry(struct hash_item *blist, void *data,
	int (*cmpdata)(void *, void *))
{
	while (blist) {
		if ((*cmpdata)(data, blist->data)) return blist;
		blist = blist->next;
	}
	return 0;
}

/*
 * This function acts like get_entry but deletes the found item, using
 * the provided function deldata(); it returns 0 if the given data was
 * not found.
 */
static struct hash_item *del_entry(struct hash_item *blist, void *data,
	int (*cmpdata)(void *, void *), void (*deldata)(void *))
{
	struct hash_item *prev = 0, *save = blist;

	while (blist) {
		if ((*cmpdata)(data, blist->data)) {
			if (deldata) (*deldata)(blist->data);
			if (prev) prev->next = blist->next;
			if (save == blist) save = blist->next;
			freemem(blist);
			return save;
		}
		prev = blist;
		blist = blist->next;
	}
	return 0;
}

/*
 * This function creates a new hashtable, with the hashing and comparison
 * functions given as parameters
 */
struct HT *newHT(int n, int (*cmpdata)(void *, void *), int (*hash)(void *),
	void (*deldata)(void *))
{
	struct HT *t = getmem(sizeof(struct HT));
	int i;

	t->lists = getmem(n * sizeof(struct hash_item *));
	for (i = 0; i < n; i ++) t->lists[i] = 0;
	t->nb_lists = n;
	t->cmpdata = cmpdata;
	t->hash = hash;
	t->deldata = deldata;
	return t;
}

/*
 * This function adds a new entry in the hashtable ht; it returns 0
 * on success, or a pointer to the already present item otherwise.
 */
void *putHT(struct HT *ht, void *data)
{
	int h;
	struct hash_item *d;

	h = ((*(ht->hash))(data));
#ifndef FAST_HASH
	h %= ht->nb_lists;
#endif
	if ((d = get_entry(ht->lists[h], data, ht->cmpdata)))
		return d->data;
	ht->lists[h] = add_entry(ht->lists[h], data);
	return 0;
}

/*
 * This function adds a new entry in the hashtable ht, even if an equal
 * entry is already there. Exercise caution !
 * The new entry will "hide" the old one, which means that the new will be
 * found upon lookup/delete, not the old one.
 */
void *forceputHT(struct HT *ht, void *data)
{
	int h;

	h = ((*(ht->hash))(data));
#ifndef FAST_HASH
	h %= ht->nb_lists;
#endif
	ht->lists[h] = add_entry(ht->lists[h], data);
	return 0;
}

/*
 * This function finds the entry corresponding to *data in the
 * hashtable ht (using the comparison function given as argument
 * to newHT)
 */
void *getHT(struct HT *ht, void *data)
{
	int h;
	struct hash_item *t;

	h = ((*(ht->hash))(data));
#ifndef FAST_HASH
	h %= ht->nb_lists;
#endif
	if ((t = get_entry(ht->lists[h], data, ht->cmpdata)) == 0)
		return 0;
	return (t->data);
}

/*
 * This function finds and delete the entry corresponding to *data
 * in the hashtable ht (using the comparison function given as
 * argument to newHT).
 */

int delHT(struct HT *ht, void *data)
{
	int h;

	h = ((*(ht->hash))(data));
#ifndef FAST_HASH
	h %= ht->nb_lists;
#endif
	ht->lists[h] = del_entry(ht->lists[h], data, ht->cmpdata, ht->deldata);
	return 1;
}

/*
 * This function completely eradicates from memory a given hash table,
 * releasing all objects
 */
void killHT(struct HT *ht)
{
	int i;
	struct hash_item *t, *n;
	void (*dd)(void *) = ht->deldata;

	for (i = 0; i < ht->nb_lists; i ++) for (t = ht->lists[i]; t;) {
		n = t->next;
		if (dd) (*dd)(t->data);
		freemem(t);
		t = n;
	}
	freemem(ht->lists);
	freemem(ht);
}

/*
 * This function stores a backup of the hash table, for context stacking.
 */
void saveHT(struct HT *ht, void **buffer)
{
	struct hash_item **b = (struct hash_item **)buffer;

	mmv(b, ht->lists, ht->nb_lists * sizeof(struct hash_item *));
}

/*
 * This function restores the saved state of the hash table.
 * Do NOT use if some of the entries that were present before the backup
 * have been removed (even temporarily).
 */
void restoreHT(struct HT *ht, void **buffer)
{
	struct hash_item **b = (struct hash_item **)buffer;
	int i;

	for (i = 0; i < ht->nb_lists; i ++) {
		struct hash_item *t = ht->lists[i], *n;

		while (t != b[i]) {
			n = t->next;
			(*(ht->deldata))(t->data);
			freemem(t);
			t = n;
		}
		ht->lists[i] = b[i];
	}
}

/*
 * This function is evil. It inserts a new item in a saved hash table,
 * tweaking the save buffer and the hash table in order to keep things
 * stable. There are no checks.
 */
void tweakHT(struct HT *ht, void **buffer, void *data)
{
	int h;
	struct hash_item *d, *e;

	h = ((*(ht->hash))(data));
#ifndef FAST_HASH
	h %= ht->nb_lists;
#endif
	for (d = ht->lists[h]; d != buffer[h]; d = d->next);
	d = add_entry(buffer[h], data);
	if (buffer[h] == ht->lists[h]) {
		buffer[h] = ht->lists[h] = d;
		return;
	}
	for (e = ht->lists[h]; e->next != buffer[h]; e = e->next);
	e->next = d;
	buffer[h] = d;
}

/*
 * This function scans the whole table and calls the given function on
 * each entry.
 */
void scanHT(struct HT *ht, void (*action)(void *))
{
	int i;

	for (i = 0; i < ht->nb_lists; i ++) {
		struct hash_item *t = ht->lists[i];

		while (t) {
			(*action)(t->data);
			t = t->next;
		}
	}
}

/*
 * The two following fonctions are generic for storing structures
 * uniquely identified by their name, which must be the first
 * field of the structure.
 */
int hash_struct(void *m)
{
	char *n = *(char **)m;

#ifdef FAST_HASH
	return hash_string(n);
#else
	return hash_string(n) & 127;
#endif
}

int cmp_struct(void *m1, void *m2)
{
	char *n1 = *(char **)m1, *n2 = *(char **)m2;

	return !strcmp(n1, n2);
}
