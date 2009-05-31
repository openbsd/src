/*			Lynx Client-side Image MAP Support	       LYMap.c
 *			==================================
 *
 *	Author: FM	Foteos Macrides (macrides@sci.wfbr.edu)
 *
 */

#include <HTUtils.h>
#include <HTTP.h>
#include <HTAnchor.h>
#include <HTAccess.h>
#include <HTFormat.h>
#include <HTParse.h>
#include <HTAlert.h>
#include <LYUtils.h>
#include <LYMap.h>
#include <GridText.h>
#include <LYGlobalDefs.h>
#include <LYKeymap.h>
#include <LYCharUtils.h>
#include <LYCharSets.h>
#include <LYStrings.h>

#ifdef DIRED_SUPPORT
#include <LYUpload.h>
#include <LYLocal.h>
#endif

#include <LYexit.h>
#include <LYLeaks.h>

#define NO_MAP_TITLE "[USEMAP]"

typedef struct _LYMapElement {
    char *address;
    char *title;
#ifndef DONT_TRACK_INTERNAL_LINKS
    BOOL intern_flag;
#endif
} LYMapElement;

typedef struct _LYImageMap {
    char *address;
    char *title;
    HTList *elements;
} LYImageMap;

struct _HTStream {
    HTStreamClass *isa;
};

static HTList *LynxMaps = NULL;

BOOL LYMapsOnly = FALSE;

/*
 * Utility for freeing a list of MAPs.
 */
void ImageMapList_free(HTList *theList)
{
    LYImageMap *map;
    LYMapElement *element;
    HTList *cur = theList;
    HTList *current;

    if (!cur)
	return;

    while (NULL != (map = (LYImageMap *) HTList_nextObject(cur))) {
	FREE(map->address);
	FREE(map->title);
	if (map->elements) {
	    current = map->elements;
	    while (NULL !=
		   (element = (LYMapElement *) HTList_nextObject(current))) {
		FREE(element->address);
		FREE(element->title);
		FREE(element);
	    }
	    HTList_delete(map->elements);
	    map->elements = NULL;
	}
	FREE(map);
    }
    HTList_delete(theList);
    return;
}

#ifdef LY_FIND_LEAKS
/*
 * Utility for freeing the global list of MAPs.  - kw
 */
static void LYLynxMaps_free(void)
{
    ImageMapList_free(LynxMaps);
    LynxMaps = NULL;
    return;
}
#endif /* LY_FIND_LEAKS */

/*
 * We keep two kinds of lists:
 * - A global list (LynxMaps) shared by MAPs from all documents that
 *   do not have POST data.
 * - For each response to a POST which contains MAPs, a list specific
 *   to this combination of URL and post_data.  It is kept in the
 *   HTParentAnchor structure and is freed when the document is removed
 *   from memory, in the course of normal removal of anchors.
 *   MAPs from POST responses can only be accessed via internal links,
 *   i.e., from within the same document (with the same post_data).
 *   The notion of "same document" is extended, so that LYNXIMGMAP:
 *   and List Page screens are logically part of the document on which
 *   they are based. - kw
 *
 * If DONT_TRACK_INTERNAL_LINKS is defined, only the global list will be used
 * for all MAPs.
 *
 */

/*
 * Utility for creating an LYImageMap list, if it doesn't exist already, adding
 * LYImageMap entry structures if needed, and removing any LYMapElements in a
 * pre-existing LYImageMap entry so that it will have only those from AREA tags
 * for the current analysis of MAP element content.  - FM
 */
BOOL LYAddImageMap(char *address,
		   char *title,
		   HTParentAnchor *node_anchor)
{
    LYImageMap *tmp = NULL;
    LYImageMap *old = NULL;
    HTList *cur = NULL;
    HTList *theList = NULL;
    HTList *curele = NULL;
    LYMapElement *ele = NULL;

    if (isEmpty(address))
	return FALSE;
    if (!(node_anchor && node_anchor->address))
	return FALSE;

    /*
     * Set theList to either the global LynxMaps list or, if we are associated
     * with post data, the specific list.  The list is created if it doesn't
     * already exist.  - kw
     */
#ifndef DONT_TRACK_INTERNAL_LINKS
    if (node_anchor->post_data) {
	/*
	 * We are handling a MAP element found while parsing node_anchor's
	 * stream of data, and node_anchor has post_data associated and should
	 * therefore represent a POST response, so use the specific list.  - kw
	 */
	theList = node_anchor->imaps;
	if (!theList) {
	    theList = node_anchor->imaps = HTList_new();
	}
    } else
#endif
    {
	if (!LynxMaps) {
	    LynxMaps = HTList_new();
#ifdef LY_FIND_LEAKS
	    atexit(LYLynxMaps_free);
#endif
	}
	theList = LynxMaps;
    }

    if (theList) {
	cur = theList;
	while (NULL != (old = (LYImageMap *) HTList_nextObject(cur))) {
	    if (old->address == 0)	/* shouldn't happen */
		continue;
	    if (!strcmp(old->address, address)) {
		FREE(old->address);
		FREE(old->title);
		if (old->elements) {
		    curele = old->elements;
		    while (NULL !=
			   (ele = (LYMapElement *) HTList_nextObject(curele))) {
			FREE(ele->address);
			FREE(ele->title);
			FREE(ele);
		    }
		    HTList_delete(old->elements);
		    old->elements = NULL;
		}
		break;
	    }
	}
    }

    tmp = (old != NULL) ?
	old : typecalloc(LYImageMap);
    if (tmp == NULL) {
	outofmem(__FILE__, "LYAddImageMap");
	return FALSE;
    }
    StrAllocCopy(tmp->address, address);
    if (non_empty(title))
	StrAllocCopy(tmp->title, title);
    if (tmp != old)
	HTList_addObject(theList, tmp);
    return TRUE;
}

/*
 * Utility for adding LYMapElement's to LYImageMap's
 * in the appropriate list. - FM
 */
BOOL LYAddMapElement(char *map,
		     char *address,
		     char *title,
		     HTParentAnchor *node_anchor,
		     BOOL intern_flag GCC_UNUSED)
{
    LYMapElement *tmp = NULL;
    LYImageMap *theMap = NULL;
    HTList *theList = NULL;
    HTList *cur = NULL;

    if (isEmpty(map) || isEmpty(address))
	return FALSE;
    if (!(node_anchor && node_anchor->address))
	return FALSE;

    /*
     * Set theList to either the global LynxMaps list or, if we are associated
     * with post data, the specific list.  The list should already exist, since
     * this function is only called if the AREA tag we are handling was within
     * a MAP element in node_anchor's stream of data, so that LYAddImageMap has
     * been called.  - kw
     */
#ifndef DONT_TRACK_INTERNAL_LINKS
    if (node_anchor->post_data) {
	/*
	 * We are handling an AREA tag found while parsing node_anchor's stream
	 * of data, and node_anchor has post_data associated and should
	 * therefore represent a POST response, so use the specific list.  - kw
	 */
	theList = node_anchor->imaps;
	if (!theList) {
	    return FALSE;
	}
    } else
#endif
    {
	if (!LynxMaps)
	    LYAddImageMap(map, NULL, node_anchor);
	theList = LynxMaps;
    }

    cur = theList;
    while (NULL != (theMap = (LYImageMap *) HTList_nextObject(cur))) {
	if (!strcmp(theMap->address, map)) {
	    break;
	}
    }
    if (!theMap)
	return FALSE;
    if (!theMap->elements)
	theMap->elements = HTList_new();
    cur = theMap->elements;
    while (NULL != (tmp = (LYMapElement *) HTList_nextObject(cur))) {
	if (!strcmp(tmp->address, address)) {
	    FREE(tmp->address);
	    FREE(tmp->title);
	    HTList_removeObject(theMap->elements, tmp);
	    FREE(tmp);
	    break;
	}
    }

    tmp = typecalloc(LYMapElement);
    if (tmp == NULL) {
	perror("Out of memory in LYAddMapElement");
	return FALSE;
    }
    StrAllocCopy(tmp->address, address);
    if (non_empty(title))
	StrAllocCopy(tmp->title, title);
    else
	StrAllocCopy(tmp->title, address);
#ifndef DONT_TRACK_INTERNAL_LINKS
    tmp->intern_flag = intern_flag;
#endif
    HTList_appendObject(theMap->elements, tmp);

    CTRACE((tfp,
	    "LYAddMapElement\n\tmap     %s\n\taddress %s\n\ttitle   %s)\n",
	    NonNull(map), NonNull(address), NonNull(title)));

    return TRUE;
}

/*
 * Utility for checking whether an LYImageMap entry with a given address
 * already exists in the LynxMaps structure.  - FM
 */
BOOL LYHaveImageMap(char *address)
{
    LYImageMap *Map;
    HTList *cur = LynxMaps;

    if (!(cur && non_empty(address)))
	return FALSE;

    while (NULL != (Map = (LYImageMap *) HTList_nextObject(cur))) {
	if (!strcmp(Map->address, address)) {
	    return TRUE;
	}
    }

    return FALSE;
}

/*
 * Fills in a DocAddress structure for getting the HTParentAnchor of the
 * underlying resource.  ALso returns a pointer to that anchor in
 * *punderlying if we are dealing with POST data.  - kw
 *
 * address  is the address of the underlying resource, i.e., the one
 *	    containing the MAP element, the MAP's name appended as
 *	    fragment is ignored.
 * anAnchor is the LYNXIMGMAP: anchor; if it is associated with POST
 *	    data, we want the specific list, otherwise the global list.
 */
static void fill_DocAddress(DocAddress *wwwdoc, char *address,
			    HTParentAnchor *anAnchor,
			    HTParentAnchor **punderlying)
{
    HTParentAnchor *underlying;

    if (anAnchor && anAnchor->post_data) {
	wwwdoc->address = address;
	wwwdoc->post_data = anAnchor->post_data;
	wwwdoc->post_content_type = anAnchor->post_content_type;
	wwwdoc->bookmark = NULL;
	wwwdoc->isHEAD = FALSE;
	wwwdoc->safe = FALSE;
	underlying = HTAnchor_findAddress(wwwdoc);
	if (underlying->safe)
	    wwwdoc->safe = TRUE;
	if (punderlying)
	    *punderlying = underlying;
    } else {
	wwwdoc->address = address;
	wwwdoc->post_data = NULL;
	wwwdoc->post_content_type = NULL;
	wwwdoc->bookmark = NULL;
	wwwdoc->isHEAD = FALSE;
	wwwdoc->safe = FALSE;
	if (punderlying)
	    *punderlying = NULL;
    }
}

/*
 * Get the appropriate list for creating a LYNXIMGMAP:  pseudo- document: 
 * either the global list (LynxMaps), or the specific list if a List Page for a
 * POST response is requested.  Also fill in the DocAddress structure etc.  by
 * calling fill_DocAddress().
 *
 * address is the address of the underlying resource, i.e., the one
 *	   containing the MAP element, the MAP's name appended as
 *	   fragment is ignored.
 * anchor  is the LYNXIMGMAP: anchor for which LYLoadIMGmap() is
 *	   requested; if it is associated with POST data, we want the
 *	   specific list for this combination of address+post_data.
 *
 * if DONT_TRACK_INTERNAL_LINKS is defined, the Anchor passed to
 * LYLoadIMGmap() will never have post_data, so that the global list
 * will be used. - kw
 */
static HTList *get_the_list(DocAddress *wwwdoc, char *address,
			    HTParentAnchor *anchor,
			    HTParentAnchor **punderlying)
{
    if (anchor && anchor->post_data) {
	fill_DocAddress(wwwdoc, address, anchor, punderlying);
	if (non_empty(punderlying))
	    return (*punderlying)->imaps;
	return anchor->imaps;
    } else {
	fill_DocAddress(wwwdoc, address, NULL, punderlying);
	return LynxMaps;
    }
}

/*	LYLoadIMGmap - F.Macrides (macrides@sci.wfeb.edu)
 *	------------
 *	Create a text/html stream with a list of links
 *	for HyperText References in AREAs of a MAP.
 */

static int LYLoadIMGmap(const char *arg,
			HTParentAnchor *anAnchor,
			HTFormat format_out,
			HTStream *sink)
{
    HTFormat format_in = WWW_HTML;
    HTStream *target = NULL;
    char *buf = NULL;
    LYMapElement *tmp = NULL;
    LYImageMap *theMap = NULL;
    char *MapTitle = NULL;
    char *MapAddress = NULL;
    HTList *theList;
    HTList *cur = NULL;
    char *address = NULL;
    char *cp = NULL;
    DocAddress WWWDoc;
    HTParentAnchor *underlying;
    BOOL old_cache_setting = LYforce_no_cache;
    BOOL old_reloading = reloading;
    HTFormat old_format_out = HTOutputFormat;

    if (isLYNXIMGMAP(arg)) {
	address = (char *) (arg + LEN_LYNXIMGMAP);
    }
    if (!(address && strchr(address, ':'))) {
	HTAlert(MISDIRECTED_MAP_REQUEST);
	return (HT_NOT_LOADED);
    }

    theList = get_the_list(&WWWDoc, address, anAnchor, &underlying);
    if (WWWDoc.safe)
	anAnchor->safe = TRUE;

    if (!theList) {
	if (anAnchor->post_data && !WWWDoc.safe &&
	    ((underlying && underlying->document && !LYforce_no_cache) ||
	     HTConfirm(CONFIRM_POST_RESUBMISSION) != TRUE)) {
	    HTAlert(FAILED_MAP_POST_REQUEST);
	    return (HT_NOT_LOADED);
	}
	LYforce_no_cache = TRUE;
	reloading = TRUE;
	HTOutputFormat = WWW_PRESENT;
	LYMapsOnly = TRUE;
	if (!HTLoadAbsolute(&WWWDoc)) {
	    LYforce_no_cache = old_cache_setting;
	    reloading = old_reloading;
	    HTOutputFormat = old_format_out;
	    LYMapsOnly = FALSE;
	    HTAlert(MAP_NOT_ACCESSIBLE);
	    return (HT_NOT_LOADED);
	}
	LYforce_no_cache = old_cache_setting;
	reloading = old_reloading;
	HTOutputFormat = old_format_out;
	LYMapsOnly = FALSE;
	theList = get_the_list(&WWWDoc, address, anAnchor, &underlying);
    }

    if (!theList) {
	HTAlert(MAPS_NOT_AVAILABLE);
	return (HT_NOT_LOADED);
    }

    cur = theList;
    while (NULL != (theMap = (LYImageMap *) HTList_nextObject(cur))) {
	if (!strcmp(theMap->address, address)) {
	    break;
	}
    }
    if (theMap && HTList_count(theMap->elements) == 0) {
	/*
	 * We found a MAP without any usable AREA.  Fake a redirection to the
	 * address with fragment.  We do this even for post data (internal link
	 * within a document with post data) if it will not result in an
	 * unwanted network request.  - kw
	 */
	if (!anAnchor->post_data) {
	    StrAllocCopy(redirecting_url, address);
	    return (HT_REDIRECTING);
	} else if (WWWDoc.safe ||
		   (underlying->document && !anAnchor->document &&
		    (LYinternal_flag || LYoverride_no_cache))) {
	    StrAllocCopy(redirecting_url, address);
	    redirect_post_content = TRUE;
	    return (HT_REDIRECTING);
	}
    }
    if (!(theMap && theMap->elements)) {
	if (anAnchor->post_data && !WWWDoc.safe &&
	    ((underlying && underlying->document && !LYforce_no_cache) ||
	     HTConfirm(CONFIRM_POST_RESUBMISSION) != TRUE)) {
	    HTAlert(FAILED_MAP_POST_REQUEST);
	    return (HT_NOT_LOADED);
	}
	LYforce_no_cache = TRUE;
	reloading = TRUE;
	HTOutputFormat = WWW_PRESENT;
	LYMapsOnly = TRUE;
	if (!HTLoadAbsolute(&WWWDoc)) {
	    LYforce_no_cache = old_cache_setting;
	    reloading = old_reloading;
	    HTOutputFormat = old_format_out;
	    LYMapsOnly = FALSE;
	    HTAlert(MAP_NOT_ACCESSIBLE);
	    return (HT_NOT_LOADED);
	}
	LYforce_no_cache = old_cache_setting;
	reloading = old_reloading;
	HTOutputFormat = old_format_out;
	LYMapsOnly = FALSE;
	cur = get_the_list(&WWWDoc, address, anAnchor, &underlying);
	while (NULL != (theMap = (LYImageMap *) HTList_nextObject(cur))) {
	    if (!strcmp(theMap->address, address)) {
		break;
	    }
	}
	if (!(theMap && theMap->elements)) {
	    HTAlert(MAP_NOT_AVAILABLE);
	    return (HT_NOT_LOADED);
	}
    }
#ifdef DONT_TRACK_INTERNAL_LINKS
    anAnchor->no_cache = TRUE;
#endif

    target = HTStreamStack(format_in,
			   format_out,
			   sink, anAnchor);

    if (!target || target == NULL) {
	HTSprintf0(&buf, CANNOT_CONVERT_I_TO_O,
		   HTAtom_name(format_in), HTAtom_name(format_out));
	HTAlert(buf);
	FREE(buf);
	return (HT_NOT_LOADED);
    }

    if (non_empty(theMap->title)) {
	StrAllocCopy(MapTitle, theMap->title);
    } else if (non_empty(anAnchor->title)) {
	StrAllocCopy(MapTitle, anAnchor->title);
    } else if (non_empty(LYRequestTitle) &&
	       strcasecomp(LYRequestTitle, NO_MAP_TITLE)) {
	StrAllocCopy(MapTitle, LYRequestTitle);
    } else if ((cp = strchr(address, '#')) != NULL) {
	StrAllocCopy(MapTitle, (cp + 1));
    }
    if (isEmpty(MapTitle)) {
	StrAllocCopy(MapTitle, NO_MAP_TITLE);
    } else {
	LYEntify(&MapTitle, TRUE);
    }

#define PUTS(buf)    (*target->isa->put_block)(target, buf, strlen(buf))

    HTSprintf0(&buf, "<html>\n<head>\n");
    PUTS(buf);
    HTSprintf0(&buf, "<META %s content=\"text/html;charset=%s\">\n",
	       "http-equiv=\"content-type\"",
	       LYCharSet_UC[current_char_set].MIMEname);
    PUTS(buf);
    /*
     * This page is a list of titles and anchors for them.  Since titles
     * already passed SGML/HTML stage they are converted to current_char_set. 
     * That is why we insist on META charset for this page.
     */
    HTSprintf0(&buf, "<title>%s</title>\n", MapTitle);
    PUTS(buf);
    HTSprintf0(&buf, "</head>\n<body>\n");
    PUTS(buf);

    HTSprintf0(&buf, "<h1><em>%s</em></h1>\n", MapTitle);
    PUTS(buf);

    StrAllocCopy(MapAddress, address);
    LYEntify(&MapAddress, FALSE);
    HTSprintf0(&buf, "<h2><em>MAP:</em>&nbsp;%s</h2>\n", MapAddress);
    PUTS(buf);

    HTSprintf0(&buf, "<%s compact>\n", ((keypad_mode == NUMBERS_AS_ARROWS) ?
					"ol" : "ul"));
    PUTS(buf);
    cur = theMap->elements;
    while (NULL != (tmp = (LYMapElement *) HTList_nextObject(cur))) {
	StrAllocCopy(MapAddress, tmp->address);
	LYEntify(&MapAddress, FALSE);
	PUTS("<li><a href=\"");
	PUTS(MapAddress);
	PUTS("\"");
#ifndef DONT_TRACK_INTERNAL_LINKS
	if (tmp->intern_flag)
	    PUTS(" TYPE=\"internal link\"");
#endif
	PUTS("\n>");
	LYformTitle(&MapTitle, tmp->title);
	LYEntify(&MapTitle, TRUE);
	PUTS(MapTitle);
	PUTS("</a>\n");
    }
    HTSprintf0(&buf, "</%s>\n</body>\n</html>\n",
	       ((keypad_mode == NUMBERS_AS_ARROWS)
		? "ol"
		: "ul"));
    PUTS(buf);

    (*target->isa->_free) (target);
    FREE(MapAddress);
    FREE(MapTitle);
    FREE(buf);
    return (HT_LOADED);
}

void LYPrintImgMaps(FILE *fp)
{
    const char *only = HTLoadedDocumentURL();
    int only_len = strlen(only);
    HTList *outer = LynxMaps;
    HTList *inner;
    LYImageMap *map;
    LYMapElement *elt;
    int count;

    if (HTList_count(outer) > 0) {
	while (NULL != (map = (LYImageMap *) HTList_nextObject(outer))) {
	    if (only_len != 0) {
		if (strncmp(only, map->address, only_len)
		    || (map->address[only_len] != '\0'
			&& map->address[only_len] != '#')) {
		    continue;
		}
	    }
	    fprintf(fp, "\n%s\n", isEmpty(map->title) ? NO_MAP_TITLE : map->title);
	    fprintf(fp, "%s\n", map->address);
	    inner = map->elements;
	    count = 0;
	    while (NULL != (elt = (LYMapElement *) HTList_nextObject(inner))) {
		fprintf(fp, "%4d. %s", ++count, elt->address);
#ifndef DONT_TRACK_INTERNAL_LINKS
		if (elt->intern_flag)
		    fprintf(fp, " TYPE=\"internal link\"");
#endif
		fprintf(fp, "\n");
	    }
	}
    }
}

#ifdef GLOBALDEF_IS_MACRO
#define _LYIMGMAP_C_GLOBALDEF_1_INIT { "LYNXIMGMAP", LYLoadIMGmap, 0}
GLOBALDEF(HTProtocol, LYLynxIMGmap, _LYIMGMAP_C_GLOBALDEF_1_INIT);
#else
GLOBALDEF HTProtocol LYLynxIMGmap =
{"LYNXIMGMAP", LYLoadIMGmap, 0};
#endif /* GLOBALDEF_IS_MACRO */
