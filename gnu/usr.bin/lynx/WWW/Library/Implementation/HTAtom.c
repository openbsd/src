/*			Atoms: Names to numbers			HTAtom.c
 *			=======================
 *
 *	Atoms are names which are given representative pointer values
 *	so that they can be stored more efficiently, and comparisons
 *	for equality done more efficiently.
 *
 *	Atoms are kept in a hash table consisting of an array of linked lists.
 *
 * Authors:
 *	TBL	Tim Berners-Lee, WorldWideWeb project, CERN
 *	(c) Copyright CERN 1991 - See Copyright.html
 *
 */

#include <HTUtils.h>

#define HASH_SIZE	101	/* Tunable */
#include <HTAtom.h>

#include <HTList.h>

#include <LYexit.h>
#include <LYLeaks.h>

static HTAtom *hash_table[HASH_SIZE];
static BOOL initialised = NO;

/*
 *	To free off all atoms.
 */
#ifdef LY_FIND_LEAKS
static void free_atoms(void);
#endif

/*
 *	Alternate hashing function.
 */
#define HASH_FUNCTION(cp_hash) ((strlen(cp_hash) * UCH(*cp_hash)) % HASH_SIZE)

HTAtom *HTAtom_for(const char *string)
{
    int hash;
    HTAtom *a;

    /* First time around, clear hash table
     */
    /*
     * Memory leak fixed.
     * 05-29-94 Lynx 2-3-1 Garrett Arch Blythe
     */
    if (!initialised) {
	int i;

	for (i = 0; i < HASH_SIZE; i++)
	    hash_table[i] = (HTAtom *) 0;
	initialised = YES;
#ifdef LY_FIND_LEAKS
	atexit(free_atoms);
#endif
    }

    /*          Generate hash function
     */
    hash = HASH_FUNCTION(string);

    /*          Search for the string in the list
     */
    for (a = hash_table[hash]; a; a = a->next) {
	if (0 == strcasecomp(a->name, string)) {
	    /* CTRACE((tfp, "HTAtom: Old atom %p for `%s'\n", a, string)); */
	    return a;		/* Found: return it */
	}
    }

    /*          Generate a new entry
     */
    a = (HTAtom *) malloc(sizeof(*a));
    if (a == NULL)
	outofmem(__FILE__, "HTAtom_for");
    a->name = (char *) malloc(strlen(string) + 1);
    if (a->name == NULL)
	outofmem(__FILE__, "HTAtom_for");
    strcpy(a->name, string);
    a->next = hash_table[hash];	/* Put onto the head of list */
    hash_table[hash] = a;
#ifdef NOT_DEFINED
    CTRACE((tfp, "HTAtom: New atom %p for `%s'\n", a, string));
#endif /* NOT_DEFINED */
    return a;
}

#ifdef LY_FIND_LEAKS
/*
 *	Purpose:	Free off all atoms.
 *	Arguments:	void
 *	Return Value:	void
 *	Remarks/Portability/Dependencies/Restrictions:
 *		To be used at program exit.
 *	Revision History:
 *		05-29-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
static void free_atoms(void)
{
    auto int i_counter;
    HTAtom *HTAp_freeme;

    /*
     * Loop through all lists of atoms.
     */
    for (i_counter = 0; i_counter < HASH_SIZE; i_counter++) {
	/*
	 * Loop through the list.
	 */
	while (hash_table[i_counter] != NULL) {
	    /*
	     * Free off atoms and any members.
	     */
	    HTAp_freeme = hash_table[i_counter];
	    hash_table[i_counter] = HTAp_freeme->next;
	    FREE(HTAp_freeme->name);
	    FREE(HTAp_freeme);
	}
    }
}
#endif /* LY_FIND_LEAKS */

static BOOL mime_match(const char *name,
		       const char *templ)
{
    if (name && templ) {
	static char *n1 = NULL;
	static char *t1 = NULL;
	char *n2;
	char *t2;

	StrAllocCopy(n1, name);	/* These also free the ones */
	StrAllocCopy(t1, templ);	/* from previous call.  */

	if (!(n2 = strchr(n1, '/')) || !(t2 = strchr(t1, '/')))
	    return NO;

	*(n2++) = (char) 0;
	*(t2++) = (char) 0;

	if ((0 == strcmp(t1, "*") || 0 == strcmp(t1, n1)) &&
	    (0 == strcmp(t2, "*") || 0 == strcmp(t2, n2)))
	    return YES;
    }
    return NO;
}

HTList *HTAtom_templateMatches(const char *templ)
{
    HTList *matches = HTList_new();

    if (initialised && templ) {
	int i;
	HTAtom *cur;

	for (i = 0; i < HASH_SIZE; i++) {
	    for (cur = hash_table[i]; cur; cur = cur->next) {
		if (mime_match(cur->name, templ))
		    HTList_addObject(matches, (void *) cur);
	    }
	}
    }
    return matches;
}
