/*	Hypertext "Anchor" Object				HTAnchor.c
**	==========================
**
** An anchor represents a region of a hypertext document which is linked to
** another anchor in the same or a different document.
**
** History
**
**	   Nov 1990  Written in Objective-C for the NeXT browser (TBL)
**	24-Oct-1991 (JFG), written in C, browser-independant
**	21-Nov-1991 (JFG), first complete version
**
**	(c) Copyright CERN 1991 - See Copyright.html
*/

#define HASH_SIZE 101		/* Arbitrary prime. Memory/speed tradeoff */

#include "HTUtils.h"
#include "tcp.h"
#include <ctype.h>
#include "HTAnchor.h"
#include "HTParse.h"
#include "UCAux.h"
#include "UCMap.h"

#include "LYCharSets.h"
#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

#ifdef NOT_DEFINED
/*
 *	This is the hashing function used to determine which list in the
 *		adult_table a parent anchor should be put in.  This is a
 *		much simpler function than the original used.
 */
#define HASH_FUNCTION(cp_address) ((unsigned short int)strlen(cp_address) *\
	(unsigned short int)TOUPPER(*cp_address) % HASH_SIZE)
#endif /* NOT_DEFINED */
/*
 *	This is the original function.	We'll use it again. - FM
 */
PRIVATE int HASH_FUNCTION ARGS1(
	CONST char *,	cp_address)
{
    int hash;
    unsigned char *p;

    for (p = (unsigned char *)cp_address, hash = 0; *p; p++)
	hash = (int) (hash * 3 + (*(unsigned char *)p)) % HASH_SIZE;

    return hash;
}

typedef struct _HyperDoc Hyperdoc;
#ifdef VMS
struct _HyperDoc {
	int junk;	/* VMS cannot handle pointers to undefined structs */
};
#endif /* VMS */

PRIVATE HTList **adult_table = 0;  /* Point to table of lists of all parents */

/*				Creation Methods
**				================
**
**	Do not use "new" by itself outside this module. In order to enforce
**	consistency, we insist that you furnish more information about the
**	anchor you are creating : use newWithParent or newWithAddress.
*/
PRIVATE HTParentAnchor * HTParentAnchor_new NOARGS
{
    HTParentAnchor *newAnchor =
       (HTParentAnchor *)calloc(1, sizeof(HTParentAnchor));  /* zero-filled */
    newAnchor->parent = newAnchor;
    newAnchor->bookmark = NULL; 	/* Bookmark filename. - FM */
    newAnchor->isISMAPScript = FALSE;	/* Lynx appends ?0,0 if TRUE. - FM */
    newAnchor->isHEAD = FALSE;		/* HEAD request if TRUE. - FM */
    newAnchor->safe = FALSE;		/* Safe. - FM */
    newAnchor->FileCache = NULL;	/* Path to a disk-cached copy. - FM */
    newAnchor->SugFname = NULL; 	/* Suggested filename. - FM */
    newAnchor->RevTitle = NULL; 	/* TITLE for a LINK with REV. - FM */
    newAnchor->cache_control = NULL;	/* Cache-Control. - FM */
    newAnchor->no_cache = FALSE;	/* no-cache? - FM */
    newAnchor->content_type = NULL;	/* Content-Type. - FM */
    newAnchor->content_language = NULL; /* Content-Language. - FM */
    newAnchor->content_encoding = NULL; /* Compression algorith. - FM */
    newAnchor->content_base = NULL;	/* Content-Base. - FM */
    newAnchor->content_disposition = NULL; /* Content-Disposition. - FM */
    newAnchor->content_location = NULL; /* Content-Location. - FM */
    newAnchor->content_md5 = NULL;	/* Content-MD5. - FM */
    newAnchor->content_length = 0;	/* Content-Length. - FM */
    newAnchor->date = NULL;		/* Date. - FM */
    newAnchor->expires = NULL;		/* Expires. - FM */
    newAnchor->last_modified = NULL;	/* Last-Modified. - FM */
    newAnchor->server = NULL;		/* Server. - FM */
    return newAnchor;
}

PRIVATE HTChildAnchor * HTChildAnchor_new NOARGS
{
    return (HTChildAnchor *)calloc(1, sizeof(HTChildAnchor)); /* zero-filled */
}


#ifdef CASE_INSENSITIVE_ANCHORS
/*	Case insensitive string comparison
**	----------------------------------
** On entry,
**	s	Points to one string, null terminated
**	t	points to the other.
** On exit,
**	returns YES if the strings are equivalent ignoring case
**		NO if they differ in more than	their case.
*/
PRIVATE BOOL HTEquivalent ARGS2(
	CONST char *,	s,
	CONST char *,	t)
{
    if (s && t) {  /* Make sure they point to something */
	for (; *s && *t; s++, t++) {
	    if (TOUPPER(*s) != TOUPPER(*t)) {
		return NO;
	    }
	}
	return TOUPPER(*s) == TOUPPER(*t);
    } else {
	return s == t;	/* Two NULLs are equivalent, aren't they ? */
    }
}

#else

/*	Case sensitive string comparison
**	----------------------------------
** On entry,
**	s	Points to one string, null terminated
**	t	points to the other.
** On exit,
**	returns YES if the strings are identical or both NULL
**		NO if they differ.
*/
PRIVATE BOOL HTIdentical ARGS2(
	CONST char *,	s,
	CONST char *,	t)
{
    if (s && t) {  /* Make sure they point to something */
	for (; *s && *t; s++, t++) {
	    if (*s != *t) {
		return NO;
	    }
	}
	return *s == *t;
    } else {
	return s == t;	/* Two NULLs are identical, aren't they ? */
    }
}
#endif /* CASE_INSENSITIVE_ANCHORS */


/*	Create new or find old sub-anchor
**	---------------------------------
**
**	Me one is for a new anchor being edited into an existing
**	document. The parent anchor must already exist.
*/
PUBLIC HTChildAnchor * HTAnchor_findChild ARGS2(
	HTParentAnchor *,	parent,
	CONST char *,		tag)
{
    HTChildAnchor *child;
    HTList *kids;

    if (!parent) {
	if (TRACE)
	    fprintf(stderr, "HTAnchor_findChild called with NULL parent.\n");
	return NULL;
    }
    if ((kids = parent->children) != 0) {
	/*
	**  Parent has children.  Search them.
	*/
	if (tag && *tag) {		/* TBL */
	    while (NULL != (child=(HTChildAnchor *)HTList_nextObject(kids))) {
#ifdef CASE_INSENSITIVE_ANCHORS
		if (HTEquivalent(child->tag, tag)) { /* Case insensitive */
#else
		if (HTIdentical(child->tag, tag)) {  /* Case sensitive - FM */
#endif /* CASE_INSENSITIVE_ANCHORS */
		    if (TRACE)
			fprintf(stderr,
	      "Child anchor %p of parent %p with name `%s' already exists.\n",
				(void *)child, (void *)parent, tag);
		    return child;
		}
	    }
	}  /*  end if tag is void */
    } else {  /* parent doesn't have any children yet : create family */
	parent->children = HTList_new();
    }

    child = HTChildAnchor_new();
    if (TRACE)
	fprintf(stderr,
		"new Anchor %p named `%s' is child of %p\n",
		(void *)child,
		tag ? tag : (CONST char *)"",
		(void *)parent); /* int for apollo */
    HTList_addObject (parent->children, child);
    child->parent = parent;
    StrAllocCopy(child->tag, tag);
    return child;
}


/*	Create or find a child anchor with a possible link
**	--------------------------------------------------
**
**	Create new anchor with a given parent and possibly
**	a name, and possibly a link to a _relatively_ named anchor.
**	(Code originally in ParseHTML.h)
*/
PUBLIC HTChildAnchor * HTAnchor_findChildAndLink ARGS4(
	HTParentAnchor *,	parent, /* May not be 0   */
	CONST char *,		tag,	/* May be "" or 0 */
	CONST char *,		href,	/* May be "" or 0 */
	HTLinkType *,		ltype)	/* May be 0	  */
{
    HTChildAnchor * child = HTAnchor_findChild(parent, tag);

    if (TRACE)
	fprintf(stderr,"Entered HTAnchor_findChildAndLink\n");

    if (href && *href) {
	char *relative_to = HTAnchor_address((HTAnchor *)parent);
	DocAddress parsed_doc;
	HTAnchor * dest;

	parsed_doc.address = HTParse(href, relative_to, PARSE_ALL);
#ifndef DONT_TRACK_INTERNAL_LINKS
	if (ltype && parent->post_data && ltype == LINK_INTERNAL) {
	    /* for internal links, find a destination with the same
	       post data if the source of the link has post data. - kw */
	    parsed_doc.post_data = parent->post_data;
	    parsed_doc.post_content_type = parent->post_content_type;
	} else
#endif
	{
	    parsed_doc.post_data = NULL;
	    parsed_doc.post_content_type = NULL;
	}
	parsed_doc.bookmark = NULL;
	parsed_doc.isHEAD = FALSE;
	parsed_doc.safe = FALSE;
	dest = HTAnchor_findAddress(&parsed_doc);

	HTAnchor_link((HTAnchor *)child, dest, ltype);
	FREE(parsed_doc.address);
	FREE(relative_to);
    }
    return child;
}

/*
**  Function for freeing the adult hash table. - FM
*/
PRIVATE void free_adult_table NOARGS
{
    int i_counter;
    HTList * HTAp_freeme;
    HTParentAnchor * parent;
    /*
     *	Loop through all lists.
     */
    for (i_counter = 0; i_counter < HASH_SIZE; i_counter++) {
	/*
	**  Loop through the list.
	*/
	while (adult_table[i_counter] != NULL) {
	    /*
	    **	Free off items - FM
	    */
	    HTAp_freeme = adult_table[i_counter];
	    adult_table[i_counter] = HTAp_freeme->next;
	    if (HTAp_freeme->object) {
		parent = (HTParentAnchor *)HTAp_freeme->object;
		HTAnchor_delete(parent);
	    }
	    FREE(HTAp_freeme);
	}
    }
    FREE(adult_table);
}

/*	Create new or find old named anchor
**	-----------------------------------
**
**	Me one is for a reference which is found in a document, and might
**	not be already loaded.
**	Note: You are not guaranteed a new anchor -- you might get an old one,
**	like with fonts.
*/
PUBLIC HTAnchor * HTAnchor_findAddress ARGS1(
	CONST DocAddress *,	newdoc)
{
    /* Anchor tag specified ? */
    char *tag = HTParse(newdoc->address, "", PARSE_ANCHOR);

    if (TRACE)
	fprintf(stderr,"Entered HTAnchor_findAddress\n");

    /*
    **	If the address represents a sub-anchor, we recursively load its
    **	parent, then we create a child anchor within that document.
    */
    if (*tag) {
	DocAddress parsed_doc;
	HTParentAnchor * foundParent;
	HTChildAnchor * foundAnchor;

	parsed_doc.address = HTParse(newdoc->address, "",
		PARSE_ACCESS | PARSE_HOST | PARSE_PATH | PARSE_PUNCTUATION);
	parsed_doc.post_data = newdoc->post_data;
	parsed_doc.post_content_type = newdoc->post_content_type;
	parsed_doc.bookmark = newdoc->bookmark;
	parsed_doc.isHEAD = newdoc->isHEAD;
	parsed_doc.safe = newdoc->safe;

	foundParent = (HTParentAnchor *)HTAnchor_findAddress(&parsed_doc);
	foundAnchor = HTAnchor_findChild (foundParent, tag);
	FREE(parsed_doc.address);
	FREE(tag);
	return (HTAnchor *)foundAnchor;
    } else {
	/*
	**  If the address has no anchor tag,
	**  check whether we have this node.
	*/
	int hash;
	HTList * adults;
	HTList *grownups;
	HTParentAnchor * foundAnchor;

	FREE(tag);

	/*
	**  Select list from hash table,
	*/
	hash = HASH_FUNCTION(newdoc->address);
	if (!adult_table) {
	    adult_table = (HTList **)calloc(HASH_SIZE, sizeof(HTList *));
	    atexit(free_adult_table);
	}
	if (!adult_table[hash])
	    adult_table[hash] = HTList_new();
	adults = adult_table[hash];

	/*
	**  Search list for anchor.
	*/
	grownups = adults;
	while (NULL != (foundAnchor =
			(HTParentAnchor *)HTList_nextObject(grownups))) {
#ifdef CASE_INSENSITIVE_ANCHORS
	    if (HTEquivalent(foundAnchor->address, newdoc->address) &&
		HTEquivalent(foundAnchor->post_data, newdoc->post_data) &&
		foundAnchor->isHEAD == newdoc->isHEAD)
#else
	    if (HTIdentical(foundAnchor->address, newdoc->address) &&
		HTIdentical(foundAnchor->post_data, newdoc->post_data) &&
		foundAnchor->isHEAD == newdoc->isHEAD)
#endif /* CASE_INSENSITIVE_ANCHORS */
	    {
		if (TRACE)
		    fprintf(stderr,
			    "Anchor %p with address `%s' already exists.\n",
			    (void *)foundAnchor, newdoc->address);
		 return (HTAnchor *)foundAnchor;
	     }
	}

	/*
	**  Node not found: create new anchor.
	*/
	foundAnchor = HTParentAnchor_new();
	if (TRACE)
	    fprintf(stderr,
		    "New anchor %p has hash %d and address `%s'\n",
		    (void *)foundAnchor, hash, newdoc->address);
	StrAllocCopy(foundAnchor->address, newdoc->address);
	if (newdoc->post_data)
	    StrAllocCopy(foundAnchor->post_data, newdoc->post_data);
	if (newdoc->post_content_type)
	    StrAllocCopy(foundAnchor->post_content_type,
			 newdoc->post_content_type);
	if (newdoc->bookmark)
	    StrAllocCopy(foundAnchor->bookmark, newdoc->bookmark);
	foundAnchor->isHEAD = newdoc->isHEAD;
	foundAnchor->safe = newdoc->safe;
	HTList_addObject (adults, foundAnchor);
	return (HTAnchor *)foundAnchor;
    }
}


/*	Delete an anchor and possibly related things (auto garbage collection)
**	--------------------------------------------
**
**	The anchor is only deleted if the corresponding document is not loaded.
**	All outgoing links from parent and children are deleted, and this anchor
**	is removed from the sources list of all its targets.
**	We also try to delete the targets whose documents are not loaded.
**	If this anchor's source list is empty, we delete it and its children.
*/
PRIVATE void deleteLinks ARGS1(
	HTAnchor *,	me)
{
    /*
     *	Memory leaks fixed.
     *	05-27-94 Lynx 2-3-1 Garrett Arch Blythe
     */

    /*
     *	Anchor is NULL, do nothing.
     */
    if (!me) {
	return;
    }

    /*
     *	Unregister me with our mainLink destination anchor's parent.
     */
    if (me->mainLink.dest) {
	HTParentAnchor *parent = me->mainLink.dest->parent;

	/*
	 *  Remove me from the parent's sources so that the
	 *  parent knows one less anchor is it's dest.
	 */
	if (!HTList_isEmpty(parent->sources)) {
	    /*
	     *	Really should only need to deregister once.
	     */
	    HTList_removeObject(parent->sources, (void *)me);
	}

	/*
	 *  Test here to avoid calling overhead.
	 *  If the parent has no loaded document, then we should
	 *  tell it to attempt to delete itself.
	 *  Don't do this jass if the anchor passed in is the same
	 *  as the anchor to delete.
	 *  Also, don't do this if the destination parent is our
	 *  parent.
	 */
	if (!parent->document &&
	    parent != (HTParentAnchor *)me &&
	    me->parent != parent) {
	    HTAnchor_delete(parent);
	}

	/*
	 *  At this point, we haven't a mainLink.  Set it to be
	 *  so.
	 *  Leave the HTAtom pointed to by type up to other code to
	 *  handle (reusable, near static).
	 */
	me->mainLink.dest = NULL;
	me->mainLink.type = NULL;
    }

    /*
     *	Check for extra destinations in our links list.
     */
    if (!HTList_isEmpty(me->links)) {
	HTLink *target;
	HTParentAnchor *parent;

	/*
	 *  Take out our extra non mainLinks one by one, calling
	 *  their parents to know that they are no longer
	 *  the destination of me's anchor.
	 */
	while ((target = (HTLink *)HTList_removeLastObject(me->links)) != 0) {
	    parent = target->dest->parent;
	    if (!HTList_isEmpty(parent->sources)) {
		/*
		 *  Only need to tell destination parent
		 *  anchor once.
		 */
		HTList_removeObject(parent->sources, (void *)me);
	    }

	    /*
	     *	Avoid calling overhead.
	     *	If the parent hasn't a loaded document, then
	     *	   we will attempt to have the parent
	     *	   delete itself.
	     *	Don't call twice if this is the same anchor
	     *	   that we are trying to delete.
	     *	Also, don't do this if we are trying to delete
	     *	   our parent.
	     */
	    if (!parent->document &&
		(HTParentAnchor *)me != parent &&
		me->parent != parent) {
		HTAnchor_delete(parent);
	    }
	    /*
	     *	The link structure has to be deleted, too!
	     *	That was missing, but this code probably never
	     *	got exercised by Lynx.	- KW
	     */
	    FREE(target);
	}

	/*
	 *  At this point, me no longer has any destination in
	 *  the links list.  Get rid of it.
	 */
	if (me->links) {
	    HTList_delete(me->links);
	    me->links = NULL;
	}
    }

    /*
     *	Catch in case links list exists but nothing in it.
     */
    if (me->links) {
	HTList_delete(me->links);
	me->links = NULL;
    }
}

PUBLIC BOOL HTAnchor_delete ARGS1(
	HTParentAnchor *,	me)
{
    /*
     *	Memory leaks fixed.
     * 05-27-94 Lynx 2-3-1 Garrett Arch Blythe
     */
    HTList *cur;
    HTChildAnchor *child;

    /*
     *	Do nothing if nothing to do.
     */
    if (!me) {
	return(NO);
    }

    /*
     *	Don't delete if document is loaded or being loaded.
     */
    if (me->document || me->underway) {
	return(NO);
    }

    /*
     *	Recursively try to delete destination anchors of this parent.
     *	In any event, this will tell all destination anchors that we
     *	no longer consider them a destination.
     */
    deleteLinks((HTAnchor *)me);

    /*
     *	There are still incoming links to this one (we are the
     *	destination of another anchor).
     *	Don't actually delete this anchor, but children are OK to
     *	delete their links.
     */
    if (!HTList_isEmpty(me->sources)) {
	/*
	 *  Delete all outgoing links from children, do not
	 *  delete the children, though.
	 */
	if (!HTList_isEmpty(me->children)) {
	    cur = me->children;
	    while ((child = (HTChildAnchor *)HTList_nextObject(cur)) != 0) {
		if (child != NULL) {
		    deleteLinks((HTAnchor *)child);
		}
	    }
	}

	/*
	 *  Can't delete parent, still have sources.
	 */
	return(NO);
    }

    /*
     *	No more incoming links : kill everything
     *	First, recursively delete children and their links.
     */
    if (!HTList_isEmpty(me->children)) {
	while ((child = (HTChildAnchor *)HTList_removeLastObject(
							me->children)) != 0) {
	    if (child) {
		deleteLinks((HTAnchor *)child);
		if (child->tag) {
		    FREE(child->tag);
		}
		FREE(child);
	    }
	}
    }

    /*
     *	Delete our empty list of children.
     */
    if (me->children) {
	HTList_delete(me->children);
	me->children = NULL;
    }

    /*
     * Delete our empty list of sources.
     */
    if (me->sources) {
	HTList_delete(me->sources);
	me->sources = NULL;
    }

    /*
     *	Delete the methods list.
     */
    if (me->methods) {
	/*
	 *  Leave what the methods point to up in memory for
	 *  other code (near static stuff).
	 */
	HTList_delete(me->methods);
	me->methods = NULL;
    }

    /*
     *	Free up all allocated members.
     */
    FREE(me->charset);
    FREE(me->isIndexAction);
    FREE(me->isIndexPrompt);
    FREE(me->title);
    FREE(me->physical);
    FREE(me->post_data);
    FREE(me->post_content_type);
    FREE(me->bookmark);
    FREE(me->owner);
    FREE(me->RevTitle);
    if (me->FileCache) {
	FILE *fd;
	if ((fd = fopen(me->FileCache, "r")) != NULL) {
	    fclose(fd);
	    remove(me->FileCache);
	}
	FREE(me->FileCache);
    }
    FREE(me->SugFname);
    FREE(me->cache_control);
    FREE(me->content_type);
    FREE(me->content_language);
    FREE(me->content_encoding);
    FREE(me->content_base);
    FREE(me->content_disposition);
    FREE(me->content_location);
    FREE(me->content_md5);
    FREE(me->date);
    FREE(me->expires);
    FREE(me->last_modified);
    FREE(me->server);
#ifdef USE_HASH
    FREE(me->style);
#endif

    /*
     *	Remove ourselves from the hash table's list.
     */
    if (adult_table) {
	unsigned short int usi_hash = HASH_FUNCTION(me->address);

	if (adult_table[usi_hash])  {
	    HTList_removeObject(adult_table[usi_hash], (void *)me);
	}
    }

    /*
     *	Original code wanted a way to clean out the HTFormat if no
     *	longer needed (ref count?).  I'll leave it alone since
     *	those HTAtom objects are a little harder to know where
     *	they are being referenced all at one time. (near static)
     */

    /*
     *	Free the address.
     */
    FREE(me->address);

    FREE (me->UCStages);
    ImageMapList_free(me->imaps);


    /*
     *	Finally, kill the parent anchor passed in.
     */
    FREE(me);

    return(YES);
}


/*		Move an anchor to the head of the list of its siblings
**		------------------------------------------------------
**
**	This is to ensure that an anchor which might have already existed
**	is put in the correct order as we load the document.
*/
PUBLIC void HTAnchor_makeLastChild ARGS1(
	HTChildAnchor *,	me)
{
    if (me->parent != (HTParentAnchor *)me) {  /* Make sure it's a child */
	HTList * siblings = me->parent->children;
	HTList_removeObject (siblings, me);
	HTList_addObject (siblings, me);
    }
}

/*	Data access functions
**	---------------------
*/
PUBLIC HTParentAnchor * HTAnchor_parent ARGS1(
	HTAnchor *,	me)
{
    return me ? me->parent : NULL;
}

PUBLIC void HTAnchor_setDocument ARGS2(
	HTParentAnchor *,	me,
	HyperDoc *,		doc)
{
    if (me)
	me->document = doc;
}

PUBLIC HyperDoc * HTAnchor_document ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->document : NULL;
}


/*  We don't want code to change an address after anchor creation... yet ?
PUBLIC void HTAnchor_setAddress ARGS2(
	HTAnchor *,	me,
	char *, 	addr)
{
    if (me)
	StrAllocCopy (me->parent->address, addr);
}
*/

PUBLIC char * HTAnchor_address ARGS1(
	HTAnchor *,	me)
{
    char *addr = NULL;

    if (me) {
	if (((HTParentAnchor *)me == me->parent) ||
	    !((HTChildAnchor *)me)->tag) {  /* it's an adult or no tag */
	    StrAllocCopy(addr, me->parent->address);
	} else {  /* it's a named child */
	    addr = malloc(2 +
			  strlen(me->parent->address) +
			  strlen(((HTChildAnchor *)me)->tag));
	    if (addr == NULL)
		outofmem(__FILE__, "HTAnchor_address");
	    sprintf(addr, "%s#%s",
			   me->parent->address, ((HTChildAnchor *)me)->tag);
	}
    }
    return addr;
}

PUBLIC void HTAnchor_setFormat ARGS2(
	HTParentAnchor *,	me,
	HTFormat,		form)
{
    if (me)
	me->format = form;
}

PUBLIC HTFormat HTAnchor_format ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->format : NULL;
}

PUBLIC void HTAnchor_setIndex ARGS2(
	HTParentAnchor *,	me,
	char *, 		address)
{
    if (me) {
	me->isIndex = YES;
	StrAllocCopy(me->isIndexAction, address);
    }
}

PUBLIC void HTAnchor_setPrompt ARGS2(
	HTParentAnchor *,	me,
	char *, 		prompt)
{
    if (me) {
	StrAllocCopy(me->isIndexPrompt, prompt);
    }
}

PUBLIC BOOL HTAnchor_isIndex ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->isIndex : NO;
}

/*	Whether Anchor has been designated as an ISMAP link
**	(normally by presence of an ISMAP attribute on A or IMG) - KW
*/
PUBLIC BOOL HTAnchor_isISMAPScript ARGS1(
	HTAnchor *,	me)
{
    return me ? me->parent->isISMAPScript : NO;
}

PUBLIC BOOL HTAnchor_hasChildren ARGS1(
	HTParentAnchor *,	me)
{
    return me ? ! HTList_isEmpty(me->children) : NO;
}

#if defined(USE_HASH)
/*	Style handling.
*/
PUBLIC CONST char * HTAnchor_style ARGS1(
	HTParentAnchor *,	me)
{
	return me ? me->style : NULL;
}

PUBLIC void HTAnchor_setStyle ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		style)
{
    if (me) {
	StrAllocCopy(me->style, style);
    }
}
#endif


/*	Title handling.
*/
PUBLIC CONST char * HTAnchor_title ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->title : NULL;
}

PUBLIC void HTAnchor_setTitle ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		title)
{
    int i;

    if (me) {
	StrAllocCopy(me->title, title);
	for (i = 0; me->title[i]; i++) {
	    if ((unsigned char)me->title[i] == 1 ||
		(unsigned char)me->title[i] == 2) {
		me->title[i] = ' ';
	    }
	}
    }
}

PUBLIC void HTAnchor_appendTitle ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		title)
{
    int i;

    if (me) {
	StrAllocCat(me->title, title);
	for (i = 0; me->title[i]; i++) {
	    if ((unsigned char)me->title[i] == 1 ||
		(unsigned char)me->title[i] == 2) {
		me->title[i] = ' ';
	    }
	}
    }
}

/*	Bookmark handling.
*/
PUBLIC CONST char * HTAnchor_bookmark ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->bookmark : NULL;
}

PUBLIC void HTAnchor_setBookmark ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		bookmark)
{
    if (me)
	StrAllocCopy(me->bookmark, bookmark);
}

/*	Owner handling.
*/
PUBLIC CONST char * HTAnchor_owner ARGS1(
	HTParentAnchor *,	me)
{
    return (me ? me->owner : NULL);
}

PUBLIC void HTAnchor_setOwner ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		owner)
{
    if (me) {
	StrAllocCopy(me->owner, owner);
    }
}

/*	TITLE handling in LINKs with REV="made" or REV="owner". - FM
*/
PUBLIC CONST char * HTAnchor_RevTitle ARGS1(
	HTParentAnchor *,	me)
{
    return (me ? me->RevTitle : NULL);
}

PUBLIC void HTAnchor_setRevTitle ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		title)
{
    int i;

    if (me) {
	StrAllocCopy(me->RevTitle, title);
	for (i = 0; me->RevTitle[i]; i++) {
	    if ((unsigned char)me->RevTitle[i] == 1 ||
		(unsigned char)me->RevTitle[i] == 2) {
		me->RevTitle[i] = ' ';
	    }
	}
    }
}

/*	Suggested filename handling. - FM
**	(will be loaded if we had a Content-Disposition
**	 header or META element with filename=name.suffix)
*/
PUBLIC CONST char * HTAnchor_SugFname ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->SugFname : NULL;
}

/*	Content-Encoding handling. - FM
**	(will be loaded if we had a Content-Encoding
**	 header.)
*/
PUBLIC CONST char * HTAnchor_content_encoding ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->content_encoding : NULL;
}

/*	Content-Type handling. - FM
*/
PUBLIC CONST char * HTAnchor_content_type ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->content_type : NULL;
}

/*	Last-Modified header handling. - FM
*/
PUBLIC CONST char * HTAnchor_last_modified ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->last_modified : NULL;
}

/*	Date header handling. - FM
*/
PUBLIC CONST char * HTAnchor_date ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->date : NULL;
}

/*	Server header handling. - FM
*/
PUBLIC CONST char * HTAnchor_server ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->server : NULL;
}

/*	Safe header handling. - FM
*/
PUBLIC BOOL HTAnchor_safe ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->safe : FALSE;
}

/*	Content-Base header handling. - FM
*/
PUBLIC CONST char * HTAnchor_content_base ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->content_base : NULL;
}

/*	Content-Location header handling. - FM
*/
PUBLIC CONST char * HTAnchor_content_location ARGS1(
	HTParentAnchor *,	me)
{
    return me ? me->content_location : NULL;
}

/*	Link me Anchor to another given one
**	-------------------------------------
*/
PUBLIC BOOL HTAnchor_link ARGS3(
	HTAnchor *,	source,
	HTAnchor *,	destination,
	HTLinkType *,	type)
{
    if (!(source && destination))
	return NO;  /* Can't link to/from non-existing anchor */
    if (TRACE)
	fprintf(stderr,
		"Linking anchor %p to anchor %p\n", source, destination);
    if (!source->mainLink.dest) {
	source->mainLink.dest = destination;
	source->mainLink.type = type;
    } else {
	HTLink * newLink = (HTLink *)calloc (1, sizeof (HTLink));
	if (newLink == NULL)
	    outofmem(__FILE__, "HTAnchor_link");
	newLink->dest = destination;
	newLink->type = type;
	if (!source->links)
	    source->links = HTList_new();
	HTList_addObject (source->links, newLink);
    }
    if (!destination->parent->sources)
	destination->parent->sources = HTList_new();
    HTList_addObject (destination->parent->sources, source);
    return YES;  /* Success */
}


/*	Manipulation of links
**	---------------------
*/
PUBLIC HTAnchor * HTAnchor_followMainLink ARGS1(
	HTAnchor *,	me)
{
    return me->mainLink.dest;
}

PUBLIC HTAnchor * HTAnchor_followTypedLink ARGS2(
	HTAnchor *,	me,
	HTLinkType *,	type)
{
    if (me->mainLink.type == type)
	return me->mainLink.dest;
    if (me->links) {
	HTList *links = me->links;
	HTLink *the_link;
	while (NULL != (the_link=(HTLink *)HTList_nextObject(links))) {
	    if (the_link->type == type) {
		return the_link->dest;
	    }
	}
    }
    return NULL;  /* No link of me type */
}


/*	Make main link
*/
PUBLIC BOOL HTAnchor_makeMainLink ARGS2(
	HTAnchor *,	me,
	HTLink *,	movingLink)
{
    /* Check that everything's OK */
    if (!(me && HTList_removeObject (me->links, movingLink))) {
	return NO;  /* link not found or NULL anchor */
    } else {
	/* First push current main link onto top of links list */
	HTLink *newLink = (HTLink *)calloc (1, sizeof (HTLink));
	if (newLink == NULL)
	    outofmem(__FILE__, "HTAnchor_makeMainLink");
	memcpy((void *)newLink,
	       (CONST char *)&me->mainLink, sizeof (HTLink));
	HTList_addObject (me->links, newLink);

	/* Now make movingLink the new main link, and free it */
	memcpy((void *)&me->mainLink,
	       (CONST void *)movingLink, sizeof (HTLink));
	FREE(movingLink);
	return YES;
    }
}


/*	Methods List
**	------------
*/
PUBLIC HTList * HTAnchor_methods ARGS1(
	HTParentAnchor *,	me)
{
    if (!me->methods) {
	me->methods = HTList_new();
    }
    return me->methods;
}

/*	Protocol
**	--------
*/
PUBLIC void * HTAnchor_protocol ARGS1(
	HTParentAnchor *,	me)
{
    return me->protocol;
}

PUBLIC void HTAnchor_setProtocol ARGS2(
	HTParentAnchor *,	me,
	void*,			protocol)
{
    me->protocol = protocol;
}

/*	Physical Address
**	----------------
*/
PUBLIC char * HTAnchor_physical ARGS1(
	HTParentAnchor *,	me)
{
    return me->physical;
}

PUBLIC void HTAnchor_setPhysical ARGS2(
	HTParentAnchor *,	me,
	char *, 		physical)
{
    if (me) {
	StrAllocCopy(me->physical, physical);
    }
}

/*
**  We store charset info in the HTParentAnchor object, for several
**  "stages".  (See UCDefs.h)
**  A stream method is supposed to know what stage in the model it is.
**
**  General model	MIME	 ->  parser  ->  structured  ->  HText
**  e.g., text/html
**	from HTTP:	HTMIME.c ->  SGML.c  ->  HTML.c      ->  GridText.c
**     text/plain
**	from file:	HTFile.c ->  HTPlain.c		     ->  GridText.c
**
**  The lock/set_by is used to lock e.g. a charset set by an explicit
**  HTTP MIME header against overriding by a HTML META tag - the MIME
**  header has higher priority.  Defaults (from -assume_.. options etc.)
**  will not override charset explicitly given by server.
**
**  Some advantages of keeping this in the HTAnchor:
**  - Global variables are bad.
**  - Can remember a charset given by META tag when toggling to SOURCE view.
**  - Can remember a charset given by <A CHARSET=...> href in another doc.
**
**  We don't modify the HTParentAnchor's charset element
**  here, that one will only be set when explicitly given.
*/
PUBLIC LYUCcharset * HTAnchor_getUCInfoStage ARGS2(
	HTParentAnchor *,	me,
	int,			which_stage)
{
    if (me && !me->UCStages) {
	int i;
	int chndl = UCLYhndl_for_unspec;
	UCAnchorInfo * stages = (UCAnchorInfo*)calloc(1,
						      sizeof(UCAnchorInfo));
	if (stages == NULL)
	    outofmem(__FILE__, "HTAnchor_getUCInfoStage");
	for (i = 0; i < UCT_STAGEMAX; i++) {
	    stages->s[i].C.MIMEname = "";
	    stages->s[i].LYhndl = -1;
	}
	if (me->charset) {
	    chndl = UCGetLYhndl_byMIME(me->charset);
	    if (chndl < 0) {
		chndl = UCLYhndl_for_unrec;
	    }
	}
	if (chndl >= 0) {
	    memcpy(&stages->s[UCT_STAGE_MIME].C, &LYCharSet_UC[chndl],
		   sizeof(LYUCcharset));
	    stages->s[UCT_STAGE_MIME].lock = UCT_SETBY_DEFAULT;
	} else {
	    /*
	     *	Should not happen...
	     */
	    stages->s[UCT_STAGE_MIME].C.UChndl = -1;
	    stages->s[UCT_STAGE_MIME].lock = UCT_SETBY_NONE;
	}
	stages->s[UCT_STAGE_MIME].LYhndl = chndl;
	me->UCStages = stages;
    }
    if (me) {
	return &me->UCStages->s[which_stage].C;
    }
    return NULL;
}

PUBLIC int HTAnchor_getUCLYhndl ARGS2(
	HTParentAnchor *,	me,
	int,			which_stage)
{
    if (me) {
	if (!me->UCStages) {
	    /*
	     *	This will allocate and initialize, if not yet done.
	     */
	    (void) HTAnchor_getUCInfoStage(me, which_stage);
	}
	if (me->UCStages->s[which_stage].lock > UCT_SETBY_NONE) {
	    return me->UCStages->s[which_stage].LYhndl;
	}
    }
    return -1;
}

PUBLIC LYUCcharset * HTAnchor_setUCInfoStage ARGS4(
	HTParentAnchor *,	me,
	int,			LYhndl,
	int,			which_stage,
	int,			set_by)
{
    if (me) {
	/*
	 *  This will allocate and initialize, if not yet done.
	 */
	LYUCcharset * p = HTAnchor_getUCInfoStage(me, which_stage);
	/*
	 *  Can we override?
	 */
	if (set_by >= me->UCStages->s[which_stage].lock) {
	    me->UCStages->s[which_stage].lock = set_by;
	    me->UCStages->s[which_stage].LYhndl = LYhndl;
	    if (LYhndl >= 0) {
		memcpy(p, &LYCharSet_UC[LYhndl], sizeof(LYUCcharset));
	    }
	    else {
		p->UChndl = -1;
	    }
	    return p;
	}
    }
    return NULL;
}

PUBLIC LYUCcharset * HTAnchor_resetUCInfoStage ARGS4(
	HTParentAnchor *,	me,
	int,			LYhndl,
	int,			which_stage,
	int,			set_by)
{
    if (!me || !me->UCStages)
	return NULL;
    me->UCStages->s[which_stage].lock = set_by;
    me->UCStages->s[which_stage].LYhndl = LYhndl;
    return &me->UCStages->s[which_stage].C;
}

/*
**  A set_by of (-1) means use the lock value from the from_stage.
*/
PUBLIC LYUCcharset * HTAnchor_copyUCInfoStage ARGS4(
	HTParentAnchor *,	me,
	int,			to_stage,
	int,			from_stage,
	int,			set_by)
{
    if (me) {
	/*
	 *  This will allocate and initialize, if not yet done.
	 */
	LYUCcharset * p_from = HTAnchor_getUCInfoStage(me, from_stage);
	LYUCcharset * p_to = HTAnchor_getUCInfoStage(me, to_stage);
	/*
	 *  Can we override?
	 */
	if (set_by == -1)
	    set_by = me->UCStages->s[from_stage].lock;
	if (set_by == UCT_SETBY_NONE)
	    set_by = UCT_SETBY_DEFAULT;
	if (set_by >= me->UCStages->s[to_stage].lock) {
	    me->UCStages->s[to_stage].lock = set_by;
	    me->UCStages->s[to_stage].LYhndl =
		me->UCStages->s[from_stage].LYhndl;
	    if (p_to != p_from)
		memcpy(p_to, p_from, sizeof(LYUCcharset));
	    return p_to;
	}
    }
    return NULL;
}
