/*
 * $LynxId: HTAssoc.c,v 1.10 2010/04/29 09:34:03 tom Exp $
 *
 * MODULE							HTAssoc.c
 *	    ASSOCIATION LIST FOR STORING NAME-VALUE PAIRS.
 *	    NAMES NOT CASE SENSITIVE, AND ONLY COMMON LENGTH
 *	    IS CHECKED (allows abbreviations; well, length is
 *	    taken from lookup-up name, so if table contains
 *	    a shorter abbrev it is not found).
 * AUTHORS:
 *	AL	Ari Luotonen	luotonen@dxcern.cern.ch
 *
 * HISTORY:
 *
 *
 * BUGS:
 *
 *
 */

#include <HTUtils.h>

#include <HTAssoc.h>

#include <LYLeaks.h>

HTAssocList *HTAssocList_new(void)
{
    return HTList_new();
}

void HTAssocList_delete(HTAssocList *alist)
{
    if (alist) {
	HTAssocList *cur = alist;
	HTAssoc *assoc;

	while (NULL != (assoc = (HTAssoc *) HTList_nextObject(cur))) {
	    FREE(assoc->name);
	    FREE(assoc->value);
	    FREE(assoc);
	}
	HTList_delete(alist);
	alist = NULL;
    }
}

void HTAssocList_add(HTAssocList *alist,
		     const char *name,
		     const char *value)
{
    HTAssoc *assoc;

    if (alist) {
	if (!(assoc = (HTAssoc *) malloc(sizeof(HTAssoc))))
	      outofmem(__FILE__, "HTAssoc_add");

	assert(assoc != NULL);

	assoc->name = NULL;
	assoc->value = NULL;

	if (name)
	    StrAllocCopy(assoc->name, name);
	if (value)
	    StrAllocCopy(assoc->value, value);
	HTList_addObject(alist, (void *) assoc);
    } else {
	CTRACE((tfp, "HTAssoc_add: ERROR: assoc list NULL!!\n"));
    }
}

char *HTAssocList_lookup(HTAssocList *alist,
			 const char *name)
{
    HTAssocList *cur = alist;
    HTAssoc *assoc;

    while (NULL != (assoc = (HTAssoc *) HTList_nextObject(cur))) {
	if (!strncasecomp(assoc->name, name, (int) strlen(name)))
	    return assoc->value;
    }
    return NULL;
}
