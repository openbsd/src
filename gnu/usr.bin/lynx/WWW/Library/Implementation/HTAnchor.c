/*	Hypertext "Anchor" Object				HTAnchor.c
**	==========================
**
** An anchor represents a region of a hypertext document which is linked to
** another anchor in the same or a different document.
**
** History
**
**	   Nov 1990  Written in Objective-C for the NeXT browser (TBL)
**	24-Oct-1991 (JFG), written in C, browser-independent
**	21-Nov-1991 (JFG), first complete version
**
**	(c) Copyright CERN 1991 - See Copyright.html
*/

#define HASH_SIZE 101		/* Arbitrary prime.  Memory/speed tradeoff */

#include <HTUtils.h>
#include <HTAnchor.h>
#include <HTParse.h>
#include <HTString.h>
#include <UCAux.h>
#include <UCMap.h>

#include <GridText.h>
#include <LYUtils.h>
#include <LYCharSets.h>
#include <LYLeaks.h>

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
    CONST unsigned char *p;

    for (p = (CONST unsigned char *)cp_address, hash = 0; *p; p++)
	hash = (int) (hash * 3 + (*(CONST unsigned char *)p)) % HASH_SIZE;

    return(hash);
}

typedef struct _HyperDoc Hyperdoc;
#ifdef VMS
struct _HyperDoc {
	int junk;	/* VMS cannot handle pointers to undefined structs */
};
#endif /* VMS */

/* Table of lists of all parents */
PRIVATE HTList adult_table[HASH_SIZE] = { {NULL, NULL} };


/*				Creation Methods
**				================
**
**	Do not use "new" by itself outside this module.  In order to enforce
**	consistency, we insist that you furnish more information about the
**	anchor you are creating : use newWithParent or newWithAddress.
*/
PRIVATE HTParentAnchor0 * HTParentAnchor0_new ARGS2(
	CONST char *,	address,
	short,		hash)
{
    HTParentAnchor0 *newAnchor = typecalloc(HTParentAnchor0);
    if (newAnchor == NULL)
	outofmem(__FILE__, "HTParentAnchor0_new");

    newAnchor->parent = newAnchor;	/* self */
    StrAllocCopy(newAnchor->address, address);
    newAnchor->adult_hash = hash;

    return(newAnchor);
}

PRIVATE HTParentAnchor * HTParentAnchor_new ARGS1(
	HTParentAnchor0 *,	parent)
{
    HTParentAnchor *newAnchor = typecalloc(HTParentAnchor);
    if (newAnchor == NULL)
	outofmem(__FILE__, "HTParentAnchor_new");

    newAnchor->parent = parent;		/* cross reference */
    parent->info = newAnchor;		/* cross reference */
    newAnchor->address = parent->address;  /* copy pointer */

    newAnchor->isISMAPScript = FALSE;	/* Lynx appends ?0,0 if TRUE. - FM */
    newAnchor->isHEAD = FALSE;		/* HEAD request if TRUE. - FM */
    newAnchor->safe = FALSE;		/* Safe. - FM */
    newAnchor->no_cache = FALSE;	/* no-cache? - FM */
    newAnchor->inBASE = FALSE;		/* duplicated from HTML.c/h */
    newAnchor->content_length = 0;	/* Content-Length. - FM */
    return(newAnchor);
}

PRIVATE HTChildAnchor * HTChildAnchor_new ARGS1(
	HTParentAnchor0 *,	parent)
{
    HTChildAnchor *p = typecalloc(HTChildAnchor);
    if (p == NULL)
	outofmem(__FILE__, "HTChildAnchor_new");

    p->parent = parent;		/* parent reference */
    return p;
}

PRIVATE HTChildAnchor * HText_pool_ChildAnchor_new ARGS1(
	HTParentAnchor *,	parent)
{
    HTChildAnchor *p = (HTChildAnchor *)HText_pool_calloc((HText*)(parent->document),
						sizeof(HTChildAnchor));
    if (p == NULL)
	outofmem(__FILE__, "HText_pool_ChildAnchor_new");

    p->parent = parent->parent;	/* parent reference */
    return p;
}

#ifdef CASE_INSENSITIVE_ANCHORS
/* Case insensitive string comparison */
#define HT_EQUIV(a,b) (TOUPPER(a) == TOUPPER(b))
#else
/* Case sensitive string comparison */
#define HT_EQUIV(a,b) ((a) == (b))
#endif

/*	Null-terminated string comparison
**	---------------------------------
** On entry,
**	s	Points to one string, null terminated
**	t	points to the other.
** On exit,
**	returns YES if the strings are equivalent
**		NO if they differ.
*/
PRIVATE BOOL HTSEquivalent ARGS2(
	CONST char *,	s,
	CONST char *,	t)
{
    if (s && t) {  /* Make sure they point to something */
	for (; *s && *t; s++, t++) {
	    if (!HT_EQUIV(*s, *t)) {
		return(NO);
	    }
	}
	return(HT_EQUIV(*s, *t));
    } else {
	return(s == t);		/* Two NULLs are equivalent, aren't they ? */
    }
}

/*	Binary string comparison
**	------------------------
** On entry,
**	s	Points to one bstring
**	t	points to the other.
** On exit,
**	returns YES if the strings are equivalent
**		NO if they differ.
*/
PRIVATE BOOL HTBEquivalent ARGS2(
	CONST bstring *,	s,
	CONST bstring *,	t)
{
    if (s && t && BStrLen(s) == BStrLen(t)) {
	int j;
	int len = BStrLen(s);
	for (j = 0; j < len; ++j) {
	    if (!HT_EQUIV(BStrData(s)[j], BStrData(t)[j])) {
		return(NO);
	    }
	}
	return(YES);
    } else {
	return(s == t);		/* Two NULLs are equivalent, aren't they ? */
    }
}

/*
 *  Three-way compare function
 */
PRIVATE int compare_anchors ARGS2(
	void *, l,
	void *, r)
{
    CONST char* a = ((HTChildAnchor *)l)->tag;
    CONST char* b = ((HTChildAnchor *)r)->tag;
    /* both tags are not NULL */

#ifdef CASE_INSENSITIVE_ANCHORS
	return strcasecomp(a, b); /* Case insensitive */
#else
	return strcmp(a, b);      /* Case sensitive - FM */
#endif /* CASE_INSENSITIVE_ANCHORS */
}


/*	Create new or find old sub-anchor
**	---------------------------------
**
**	This one is for a named child.
**	The parent anchor must already exist.
*/
PRIVATE HTChildAnchor * HTAnchor_findNamedChild ARGS2(
	HTParentAnchor0 *,	parent,
	CONST char *,		tag)
{
    HTChildAnchor *child;

    if (parent && tag && *tag) {  /* TBL */
	if (parent->children) {
	    /*
	    **  Parent has children.  Search them.
	    */
	    HTChildAnchor sample;
	    sample.tag = (char*)tag;    /* for compare_anchors() only */

	    child = (HTChildAnchor *)HTBTree_search(parent->children, &sample);
	    if (child != NULL) {
		CTRACE((tfp, "Child anchor %p of parent %p with name `%s' already exists.\n",
				(void *)child, (void *)parent, tag));
		return(child);
	    }
	} else {  /* parent doesn't have any children yet : create family */
	    parent->children = HTBTree_new(compare_anchors);
	}

	child = HTChildAnchor_new(parent);
	CTRACE((tfp, "HTAnchor: New Anchor %p named `%s' is child of %p\n",
		(void *)child,
		NonNull(tag),
		(void *)child->parent));

	StrAllocCopy(child->tag, tag);   /* should be set before HTBTree_add */
	HTBTree_add(parent->children, child);
	return(child);

    } else {
	CTRACE((tfp, "HTAnchor_findNamedChild called with NULL parent.\n"));
	return(NULL);
    }

}

/*
**	This one is for a new unnamed child being edited into an existing
**	document.  The parent anchor and the document must already exist.
**	(Just add new unnamed child).
*/
PRIVATE HTChildAnchor * HTAnchor_addChild ARGS1(
	HTParentAnchor *,	parent)
{
    HTChildAnchor *child;

    if (!parent) {
	CTRACE((tfp, "HTAnchor_addChild called with NULL parent.\n"));
	return(NULL);
    }

    child = HText_pool_ChildAnchor_new(parent);
    CTRACE((tfp, "HTAnchor: New unnamed Anchor %p is child of %p\n",
		(void *)child,
		(void *)child->parent));

    child->tag = 0;
    HTList_linkObject(&parent->children_notag, child, &child->_add_children_notag);

    return(child);
}


PRIVATE HTParentAnchor0 * HTAnchor_findAddress_in_adult_table PARAMS((
	CONST DocAddress *	newdoc));

PRIVATE BOOL HTAnchor_link PARAMS((
	HTChildAnchor *		child,
	HTAnchor *		destination,
	HTLinkType *		type));


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
    HTChildAnchor * child;
    CTRACE((tfp,"Entered HTAnchor_findChildAndLink:  tag=`%s',%s href=`%s'\n",
	       NonNull(tag),
	       (ltype == HTInternalLink) ? " (internal link)" : "",
	       NonNull(href) ));

    if (tag && *tag) {
	child = HTAnchor_findNamedChild(parent->parent, tag);
    } else {
	child = HTAnchor_addChild(parent);
    }

    if (href && *href) {
	CONST char *fragment = NULL;
	HTParentAnchor0 * dest;

	if (ltype == HTInternalLink && *href == '#') {
	    dest = parent->parent;
	} else {
	    CONST char *relative_to = (parent->inBASE && *href != '#') ?
				parent->content_base : parent->address;
	    DocAddress parsed_doc;
	    parsed_doc.address = HTParse(href, relative_to,
					 PARSE_ALL_WITHOUT_ANCHOR);

	    parsed_doc.post_data = NULL;
	    parsed_doc.post_content_type = NULL;
	    if (ltype && parent->post_data && ltype == HTInternalLink) {
		/* for internal links, find a destination with the same
		   post data if the source of the link has post data. - kw
		   Example: LYNXIMGMAP: */
		parsed_doc.post_data = parent->post_data;
		parsed_doc.post_content_type = parent->post_content_type;
	    }
	    parsed_doc.bookmark = NULL;
	    parsed_doc.isHEAD = FALSE;
	    parsed_doc.safe = FALSE;

	    dest = HTAnchor_findAddress_in_adult_table(&parsed_doc);
	    FREE(parsed_doc.address);
	}

	/*
	** [from HTAnchor_findAddress()]
	** If the address represents a sub-anchor, we load its parent (above),
	** then we create a named child anchor within that parent.
	*/
	fragment = (*href == '#') ?  href+1 : HTParseAnchor(href);

	if (*fragment)
	    dest = (HTParentAnchor0 *)HTAnchor_findNamedChild(dest, fragment);


	if (tag && *tag) {
	    if (child->dest) { /* DUPLICATE_ANCHOR_NAME_WORKAROUND  - kw */
		CTRACE((tfp,
			   "*** Duplicate ChildAnchor %p named `%s'",
			   child, tag));
		if ((HTAnchor *)dest != child->dest || ltype != child->type) {
		    CTRACE((tfp,
			   ", different dest %p or type, creating unnamed child\n",
			   child->dest));
		    child = HTAnchor_addChild(parent);
		}
	    }
	}
	HTAnchor_link(child, (HTAnchor *)dest, ltype);
    }
    return child;
}


/*	Create new or find old parent anchor
**	------------------------------------
**
**	Me one is for a reference which is found in a document, and might
**	not be already loaded.
**	Note: You are not guaranteed a new anchor -- you might get an old one,
**	like with fonts.
*/
PUBLIC HTParentAnchor * HTAnchor_findAddress ARGS1(
	CONST DocAddress *,	newdoc)
{
    /* Anchor tag specified ? */
    CONST char *tag = HTParseAnchor(newdoc->address);

    CTRACE((tfp,"Entered HTAnchor_findAddress\n"));

    /*
    **	If the address represents a sub-anchor, we load its parent,
    **	then we create a named child anchor within that parent.
    */
    if (*tag) {
	DocAddress parsed_doc;
	HTParentAnchor0 * foundParent;
	HTChildAnchor * foundAnchor;

	parsed_doc.address = HTParse(newdoc->address, "",
				     PARSE_ALL_WITHOUT_ANCHOR);
	parsed_doc.post_data = newdoc->post_data;
	parsed_doc.post_content_type = newdoc->post_content_type;
	parsed_doc.bookmark = newdoc->bookmark;
	parsed_doc.isHEAD = newdoc->isHEAD;
	parsed_doc.safe = newdoc->safe;

	foundParent = HTAnchor_findAddress_in_adult_table(&parsed_doc);
	foundAnchor = HTAnchor_findNamedChild (foundParent, tag);
	FREE(parsed_doc.address);
	return HTAnchor_parent((HTAnchor *)foundParent);
    }
    return HTAnchor_parent((HTAnchor *)HTAnchor_findAddress_in_adult_table(newdoc));
}


/*  The address has no anchor tag, for sure.
 */
PRIVATE HTParentAnchor0 * HTAnchor_findAddress_in_adult_table ARGS1(
	CONST DocAddress *,	newdoc)
{
    /*
    **  Check whether we have this node.
    */
    int hash;
    HTList * adults;
    HTList *grownups;
    HTParentAnchor0 * foundAnchor;
    BOOL need_extra_info = (newdoc->post_data || newdoc->post_content_type ||
		newdoc->bookmark || newdoc->isHEAD || newdoc->safe);

    /*
     *  We need not free adult_table[] atexit -
     *  it should be perfectly empty after free'ing all HText's.
     *  (There is an error if it is not empty at exit). -LP
     */

    /*
    **  Select list from hash table,
    */
    hash = HASH_FUNCTION(newdoc->address);
    adults = &(adult_table[hash]);

    /*
    **  Search list for anchor.
    */
    grownups = adults;
    while (NULL != (foundAnchor =
		    (HTParentAnchor0 *)HTList_nextObject(grownups))) {
	if (HTSEquivalent(foundAnchor->address, newdoc->address) &&

	    ((!foundAnchor->info && !need_extra_info) ||
	     (foundAnchor->info &&
	      HTBEquivalent(foundAnchor->info->post_data, newdoc->post_data) &&
	      foundAnchor->info->isHEAD == newdoc->isHEAD)))
	{
	    CTRACE((tfp, "Anchor %p with address `%s' already exists.\n",
			(void *)foundAnchor, newdoc->address));
	    return foundAnchor;
	}
    }

    /*
    **  Node not found: create new anchor.
    */
    foundAnchor = HTParentAnchor0_new(newdoc->address, hash);
    CTRACE((tfp, "New anchor %p has hash %d and address `%s'\n",
		(void *)foundAnchor, hash, newdoc->address));

    if (need_extra_info) {
	/* rare case, create a big structure */
	HTParentAnchor *p = HTParentAnchor_new(foundAnchor);

	if (newdoc->post_data)
	    BStrCopy(p->post_data, newdoc->post_data);
	if (newdoc->post_content_type)
	    StrAllocCopy(p->post_content_type,
		     newdoc->post_content_type);
	if (newdoc->bookmark)
	    StrAllocCopy(p->bookmark, newdoc->bookmark);
	p->isHEAD = newdoc->isHEAD;
	p->safe = newdoc->safe;
    }
    HTList_linkObject(adults, foundAnchor, &foundAnchor->_add_adult);

    return foundAnchor;
}

/*	Create new or find old named anchor - simple form
**	-------------------------------------------------
**
**     Like HTAnchor_findAddress, but simpler to use for simple cases.
**	No post data etc. can be supplied. - kw
*/
PUBLIC HTParentAnchor * HTAnchor_findSimpleAddress ARGS1(
	CONST char *,	url)
{
    DocAddress urldoc;

    urldoc.address = (char *)url; /* ignore warning, it IS treated like const - kw */
    urldoc.post_data = NULL;
    urldoc.post_content_type = NULL;
    urldoc.bookmark = NULL;
    urldoc.isHEAD = FALSE;
    urldoc.safe = FALSE;
    return HTAnchor_findAddress(&urldoc);
}


/*	Link me Anchor to another given one
**	-------------------------------------
*/
PRIVATE BOOL HTAnchor_link ARGS3(
	HTChildAnchor *,	child,
	HTAnchor *,		destination,
	HTLinkType *,		type)
{
    if (!(child && destination))
	return(NO);  /* Can't link to/from non-existing anchor */

    CTRACE((tfp, "Linking child %p to anchor %p\n", child, destination));
    if (child->dest) {
	CTRACE((tfp, "*** child anchor already has destination, exiting!\n"));
	return(NO);
    }

    child->dest = destination;
    child->type = type;

    if (child->parent != destination->parent)
	/* link only foreign children */
	HTList_linkObject(&destination->parent->sources, child, &child->_add_sources);

    return(YES);  /* Success */
}


/*	Delete an anchor and possibly related things (auto garbage collection)
**	--------------------------------------------
**
**	The anchor is only deleted if the corresponding document is not loaded.
**	All outgoing links from children are deleted, and children are
**	removed from the sources lists of theirs targets.
**	We also try to delete the targets whose documents are not loaded.
**	If this anchor's sources list is empty, we delete it and its children.
*/

/*
 *	Recursively try to delete destination anchor of this child.
 *	In any event, this will tell destination anchor that we
 *	no longer consider it a destination.
 */
PRIVATE void deleteLinks ARGS1(
	HTChildAnchor *,	me)
{
    /*
     *	Unregister me with our destination anchor's parent.
     */
    if (me->dest) {
	HTParentAnchor0 *parent = me->dest->parent;

	/*
	 *  Start.  Set the dest pointer to zero.
	 */
	 me->dest = NULL;

	/*
	 *  Remove me from the parent's sources so that the
	 *  parent knows one less anchor is its dest.
	 */
	if ((me->parent != parent) && !HTList_isEmpty(&parent->sources)) {
	    /*
	     *	Really should only need to deregister once.
	     */
	    HTList_unlinkObject(&parent->sources, (void *)me);
	}

	/*
	 *  Recursive call.
	 *  Test here to avoid calling overhead.
	 *  Don't delete if document is loaded or being loaded.
	 */
	if ((me->parent != parent) && !parent->underway &&
	    (!parent->info || !parent->info->document)) {
	    HTAnchor_delete(parent);
	}

	/*
	 *  At this point, we haven't a destination.  Set it to be
	 *  so.
	 *  Leave the HTAtom pointed to by type up to other code to
	 *  handle (reusable, near static).
	 */
	me->type = NULL;
    }
}


PRIVATE void HTParentAnchor_free PARAMS((
	HTParentAnchor *	me));

PUBLIC BOOL HTAnchor_delete ARGS1(
	HTParentAnchor0 *,	me)
{
    /*
     *	Memory leaks fixed.
     * 05-27-94 Lynx 2-3-1 Garrett Arch Blythe
     */
    HTBTElement *ele;
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
    if (me->underway || (me->info && me->info->document)) {
	return(NO);
    }

    /*
     *  Mark ourselves busy, so that recursive calls of this function
     *  on this HTParentAnchor0 will not free it from under our feet. - kw
     */
    me->underway = TRUE;

    {
	/*
	 *  Delete all outgoing links from named children.
	 *  Do not delete named children itself (may have incoming links).
	 */
	if (me->children) {
	    ele = HTBTree_next(me->children, NULL);
	    while (ele != NULL) {
		child = (HTChildAnchor *)HTBTree_object(ele);
		if (child->dest)
		    deleteLinks(child);
		ele = HTBTree_next(me->children, ele);
	    }
	}
    }
    me->underway = FALSE;


    /*
     * There are still incoming links to this one (we are the
     * destination of another anchor).
     */
    if (!HTList_isEmpty(&me->sources)) {
	/*
	 *  Can't delete parent, still have sources.
	 */
	return(NO);
    }

    /*
     *	No more incoming and outgoing links : kill everything
     *	First, delete named children.
     */
    if (me->children) {
	ele = HTBTree_next(me->children, NULL);
	while (ele != NULL) {
	    child = (HTChildAnchor *)HTBTree_object(ele);
	    FREE(child->tag);
	    FREE(child);
	    ele = HTBTree_next(me->children, ele);
	}
	HTBTree_free(me->children);
    }

    /*
     *  Delete the ParentAnchor, if any. (Document was already deleted).
     */
    if (me->info) {
	HTParentAnchor_free(me->info);
	FREE(me->info);
    }

    /*
     *	Remove ourselves from the hash table's list.
     */
    HTList_unlinkObject(&(adult_table[me->adult_hash]), (void *)me);

    /*
     *	Free the address.
     */
    FREE(me->address);

    /*
     *	Finally, kill the parent anchor passed in.
     */
    FREE(me);

    return(YES);
}

/*
 *  Unnamed children (children_notag) have no sence without HText -
 *  delete them and their links if we are about to free HText.
 *  Document currently exists. Called within HText_free().
 */
PUBLIC void HTAnchor_delete_links ARGS1(
	HTParentAnchor *,	me)
{
    HTList *cur;
    HTChildAnchor *child;

    /*
     *	Do nothing if nothing to do.
     */
    if (!me || !me->document) {
	return;
    }

    /*
     *  Mark ourselves busy, so that recursive calls
     *  on this HTParentAnchor0 will not free it from under our feet. - kw
     */
    me->parent->underway = TRUE;

    /*
     *  Delete all outgoing links from unnamed children.
     */
    if (!HTList_isEmpty(&me->children_notag)) {
	cur = &me->children_notag;
	while ((child =
		(HTChildAnchor *)HTList_unlinkLastObject(cur)) != 0) {
	    deleteLinks(child);
	    /* child allocated in HText pool, HText_free() will free it later*/
	}
    }
    me->parent->underway = FALSE;
}


PRIVATE void HTParentAnchor_free ARGS1(
	HTParentAnchor *,	me)
{
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
    BStrFree(me->post_data);
    FREE(me->post_content_type);
    FREE(me->bookmark);
    FREE(me->owner);
    FREE(me->RevTitle);
    FREE(me->citehost);
#ifdef USE_SOURCE_CACHE
    HTAnchor_clearSourceCache(me);
#endif
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
    FREE(me->message_id);
    FREE(me->subject);
    FREE(me->date);
    FREE(me->expires);

    FREE(me->last_modified);
    FREE(me->ETag);
    FREE(me->server);
#ifdef USE_COLOR_STYLE
    FREE(me->style);
#endif

    /*
     *	Original code wanted a way to clean out the HTFormat if no
     *	longer needed (ref count?).  I'll leave it alone since
     *	those HTAtom objects are a little harder to know where
     *	they are being referenced all at one time. (near static)
     */

    FREE(me->UCStages);
    ImageMapList_free(me->imaps);
}

#ifdef USE_SOURCE_CACHE
PUBLIC void HTAnchor_clearSourceCache ARGS1(
	HTParentAnchor *,	me)
{
    /*
     * Clean up the source cache, if any.
     */
    if (me->source_cache_file) {
	CTRACE((tfp, "SourceCache: Removing file %s\n",
	       me->source_cache_file));
	LYRemoveTemp(me->source_cache_file);
	FREE(me->source_cache_file);
    }
    if (me->source_cache_chunk) {
	CTRACE((tfp, "SourceCache: Removing memory chunk %p\n",
	       (void *)me->source_cache_chunk));
	HTChunkFree(me->source_cache_chunk);
	me->source_cache_chunk = NULL;
    }
}
#endif /* USE_SOURCE_CACHE */

/*	Data access functions
**	---------------------
*/
PUBLIC HTParentAnchor * HTAnchor_parent ARGS1(
	HTAnchor *,	me)
{
    if (!me)
	return NULL;

    if (me->parent->info)
	return me->parent->info;

    /* else: create a new structure */
    return HTParentAnchor_new(me->parent);
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
    return( me ? me->document : NULL);
}


PUBLIC char * HTAnchor_address ARGS1(
	HTAnchor *,	me)
{
    char *addr = NULL;

    if (me) {
	if (((HTParentAnchor0 *)me == me->parent) ||
	    ((HTParentAnchor *)me == me->parent->info) ||
	    !((HTChildAnchor *)me)->tag) {  /* it's an adult or no tag */
	    StrAllocCopy(addr, me->parent->address);
	} else {  /* it's a named child */
	    HTSprintf0(&addr, "%s#%s",
		       me->parent->address, ((HTChildAnchor *)me)->tag);
	}
    }
    return(addr);
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
    return( me ? me->format : NULL);
}

PUBLIC void HTAnchor_setIndex ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		address)
{
    if (me) {
	me->isIndex = YES;
	StrAllocCopy(me->isIndexAction, address);
    }
}

PUBLIC void HTAnchor_setPrompt ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		prompt)
{
    if (me) {
	StrAllocCopy(me->isIndexPrompt, prompt);
    }
}

PUBLIC BOOL HTAnchor_isIndex ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->isIndex : NO);
}

/*	Whether Anchor has been designated as an ISMAP link
**	(normally by presence of an ISMAP attribute on A or IMG) - KW
*/
PUBLIC BOOL HTAnchor_isISMAPScript ARGS1(
	HTAnchor *,	me)
{
    return( (me && me->parent->info) ? me->parent->info->isISMAPScript : NO);
}

#if defined(USE_COLOR_STYLE)
/*	Style handling.
*/
PUBLIC CONST char * HTAnchor_style ARGS1(
	HTParentAnchor *,	me)
{
	return( me ? me->style : NULL);
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
    return( me ? me->title : NULL);
}

PUBLIC void HTAnchor_setTitle ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		title)
{
    int i;

    if (me) {
	if (title) {
	    StrAllocCopy(me->title, title);
	    for (i = 0; me->title[i]; i++) {
		if (UCH(me->title[i]) == 1 ||
		    UCH(me->title[i]) == 2) {
		    me->title[i] = ' ';
		}
	    }
	} else {
	    CTRACE((tfp,"HTAnchor_setTitle: New title is NULL! "));
	    if (me->title) {
		CTRACE((tfp,"Old title was \"%s\".\n", me->title));
		FREE(me->title);
	    } else {
		CTRACE((tfp,"Old title was NULL.\n"));
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
	    if (UCH(me->title[i]) == 1 ||
		UCH(me->title[i]) == 2) {
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
    return( me ? me->bookmark : NULL);
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
    return( me ? me->owner : NULL);
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
    return( me ? me->RevTitle : NULL);
}

PUBLIC void HTAnchor_setRevTitle ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		title)
{
    int i;

    if (me) {
	StrAllocCopy(me->RevTitle, title);
	for (i = 0; me->RevTitle[i]; i++) {
	    if (UCH(me->RevTitle[i]) == 1 ||
		UCH(me->RevTitle[i]) == 2) {
		me->RevTitle[i] = ' ';
	    }
	}
    }
}

#ifndef DISABLE_BIBP
/*	Citehost for bibp links from LINKs with REL="citehost". - RDC
*/
PUBLIC CONST char * HTAnchor_citehost ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->citehost : NULL);
}

PUBLIC void HTAnchor_setCitehost ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		citehost)
{
    if (me) {
	StrAllocCopy(me->citehost, citehost);
    }
}
#endif /* !DISABLE_BIBP */

/*	Suggested filename handling. - FM
**	(will be loaded if we had a Content-Disposition
**	 header or META element with filename=name.suffix)
*/
PUBLIC CONST char * HTAnchor_SugFname ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->SugFname : NULL);
}

/*	Content-Encoding handling. - FM
**	(will be loaded if we had a Content-Encoding
**	 header.)
*/
PUBLIC CONST char * HTAnchor_content_encoding ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->content_encoding : NULL);
}

/*	Content-Type handling. - FM
*/
PUBLIC CONST char * HTAnchor_content_type ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->content_type : NULL);
}

/*	Last-Modified header handling. - FM
*/
PUBLIC CONST char * HTAnchor_last_modified ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->last_modified : NULL);
}

/*	Date header handling. - FM
*/
PUBLIC CONST char * HTAnchor_date ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->date : NULL);
}

/*	Server header handling. - FM
*/
PUBLIC CONST char * HTAnchor_server ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->server : NULL);
}

/*	Safe header handling. - FM
*/
PUBLIC BOOL HTAnchor_safe ARGS1(
	HTParentAnchor *,	me)
{
    return (BOOL) ( me ? me->safe : FALSE);
}

/*	Content-Base header handling. - FM
*/
PUBLIC CONST char * HTAnchor_content_base ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->content_base : NULL);
}

/*	Content-Location header handling. - FM
*/
PUBLIC CONST char * HTAnchor_content_location ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->content_location : NULL);
}

/*	Message-ID, used for mail replies - kw
*/
PUBLIC CONST char * HTAnchor_messageID ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->message_id : NULL);
}

PUBLIC BOOL HTAnchor_setMessageID ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		messageid)
{
    if (!(me && messageid && *messageid)) {
	return FALSE;
    }
    StrAllocCopy(me->message_id, messageid);
    return TRUE;
}

/*	Subject, used for mail replies - kw
*/
PUBLIC CONST char * HTAnchor_subject ARGS1(
	HTParentAnchor *,	me)
{
    return( me ? me->subject : NULL);
}

PUBLIC BOOL HTAnchor_setSubject ARGS2(
	HTParentAnchor *,	me,
	CONST char *,		subject)
{
    if (!(me && subject && *subject)) {
	return FALSE;
    }
    StrAllocCopy(me->subject, subject);
    return TRUE;
}

/*	Manipulation of links
**	---------------------
*/
PUBLIC HTAnchor * HTAnchor_followLink ARGS1(
	HTChildAnchor *,	me)
{
    return( me->dest);
}

PUBLIC HTAnchor * HTAnchor_followTypedLink ARGS2(
	HTChildAnchor *,	me,
	HTLinkType *,		type)
{
    if (me->type == type)
	return( me->dest);
    return(NULL);  /* No link of me type */
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
    return( me->methods);
}

/*	Protocol
**	--------
*/
PUBLIC void * HTAnchor_protocol ARGS1(
	HTParentAnchor *,	me)
{
    return( me->protocol);
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
    return( me->physical);
}

PUBLIC void HTAnchor_setPhysical ARGS2(
	HTParentAnchor *,	me,
	char *,			physical)
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
	int chndl = UCLYhndl_for_unspec;  /* always >= 0 */
	UCAnchorInfo * stages = typecalloc(UCAnchorInfo);
	if (stages == NULL)
	    outofmem(__FILE__, "HTAnchor_getUCInfoStage");
	for (i = 0; i < UCT_STAGEMAX; i++) {
	    stages->s[i].C.MIMEname = "";
	    stages->s[i].LYhndl = -1;
	}
	if (me->charset) {
	    chndl = UCGetLYhndl_byMIME(me->charset);
	    if (chndl < 0)
		chndl = UCLYhndl_for_unrec;
	    if (chndl < 0)
		/*
		**  UCLYhndl_for_unrec not defined :-(
		**  fallback to UCLYhndl_for_unspec which always valid.
		*/
		chndl = UCLYhndl_for_unspec;  /* always >= 0 */
	}
	memcpy(&stages->s[UCT_STAGE_MIME].C, &LYCharSet_UC[chndl],
	       sizeof(LYUCcharset));
	stages->s[UCT_STAGE_MIME].lock = UCT_SETBY_DEFAULT;
	stages->s[UCT_STAGE_MIME].LYhndl = chndl;
	me->UCStages = stages;
    }
    if (me) {
	return( &me->UCStages->s[which_stage].C);
    }
    return(NULL);
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
	    return( me->UCStages->s[which_stage].LYhndl);
	}
    }
    return( -1);
}

#ifdef CAN_SWITCH_DISPLAY_CHARSET
PRIVATE void setup_switch_display_charset ARGS2(HTParentAnchor *, me, int, h)
{
    if (!Switch_Display_Charset(h,SWITCH_DISPLAY_CHARSET_MAYBE))
	return;
    HTAnchor_setUCInfoStage(me, current_char_set,
			    UCT_STAGE_HTEXT, UCT_SETBY_MIME); /* highest priorty! */
    HTAnchor_setUCInfoStage(me, current_char_set,
			    UCT_STAGE_STRUCTURED, UCT_SETBY_MIME); /* highest priorty! */
    CTRACE((tfp, "changing UCInfoStage: HTEXT/STRUCTURED stages charset='%s'.\n",
	    LYCharSet_UC[current_char_set].MIMEname));
}
#endif

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
#ifdef CAN_SWITCH_DISPLAY_CHARSET
	    int ohandle = me->UCStages->s[which_stage].LYhndl;
#endif
	    me->UCStages->s[which_stage].lock = set_by;
	    me->UCStages->s[which_stage].LYhndl = LYhndl;
	    if (LYhndl >= 0) {
		memcpy(p, &LYCharSet_UC[LYhndl], sizeof(LYUCcharset));
#ifdef CAN_SWITCH_DISPLAY_CHARSET
		/* Allow a switch to a more suitable display charset */
		if ( LYhndl != ohandle && which_stage == UCT_STAGE_PARSER )
		    setup_switch_display_charset(me, LYhndl);
#endif
	    }
	    else {
		p->UChndl = -1;
	    }
	    return(p);
	}
    }
    return(NULL);
}

PUBLIC LYUCcharset * HTAnchor_resetUCInfoStage ARGS4(
	HTParentAnchor *,	me,
	int,			LYhndl,
	int,			which_stage,
	int,			set_by)
{
    int ohandle;

    if (!me || !me->UCStages)
	return(NULL);
    me->UCStages->s[which_stage].lock = set_by;
    ohandle = me->UCStages->s[which_stage].LYhndl;
    me->UCStages->s[which_stage].LYhndl = LYhndl;
#ifdef CAN_SWITCH_DISPLAY_CHARSET
    /* Allow a switch to a more suitable display charset */
    if (LYhndl >= 0 && LYhndl != ohandle && which_stage == UCT_STAGE_PARSER)
	setup_switch_display_charset(me, LYhndl);
#endif
    return( &me->UCStages->s[which_stage].C);
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
#ifdef CAN_SWITCH_DISPLAY_CHARSET
	    int ohandle = me->UCStages->s[to_stage].LYhndl;
#endif
	    me->UCStages->s[to_stage].lock = set_by;
	    me->UCStages->s[to_stage].LYhndl =
		me->UCStages->s[from_stage].LYhndl;
#ifdef CAN_SWITCH_DISPLAY_CHARSET
	    /* Allow a switch to a more suitable display charset */
	    if ( me->UCStages->s[to_stage].LYhndl >= 0
		 && me->UCStages->s[to_stage].LYhndl != ohandle
		 && to_stage == UCT_STAGE_PARSER )
		setup_switch_display_charset(me,
					     me->UCStages->s[to_stage].LYhndl);
#endif
	    if (p_to != p_from)
		memcpy(p_to, p_from, sizeof(LYUCcharset));
	    return(p_to);
	}
    }
    return(NULL);
}
