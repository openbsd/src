/*			Lynx Client-side Image MAP Support	       LYMap.c
**			==================================
**
**	Author: FM	Foteos Macrides (macrides@sci.wfbr.edu)
**
*/

#include "HTUtils.h"
#include "tcp.h"
#include "HTTP.h"
#include "HTAnchor.h"
#include "HTAccess.h"
#include "HTFormat.h"
#include "HTParse.h"
#include "HTAlert.h"
#include "LYUtils.h"
#include "LYMap.h"
#include "GridText.h"
#include "LYSignal.h"
#include "LYGlobalDefs.h"
#include "LYKeymap.h"
#include "LYCharUtils.h"

#ifdef DIRED_SUPPORT
#include "LYUpload.h"
#include "LYLocal.h"
#endif

#include "LYexit.h"
#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x=NULL;}

typedef struct _LYMapElement {
   char * address;
   char * title;
#ifndef DONT_TRACK_INTERNAL_LINKS
   BOOL   intern_flag;
#endif
} LYMapElement;

typedef struct _LYImageMap {
   char * address;
   char * title;
   HTList * elements;
} LYImageMap;

struct _HTStream
{
  HTStreamClass * isa;
};

PRIVATE HTList * LynxMaps = NULL;

PUBLIC BOOL LYMapsOnly = FALSE;

/*
 *  Utility for freeing a list of MAPs.
 */
PUBLIC void ImageMapList_free ARGS1(
    HTList *,		theList)
{
    LYImageMap *map;
    LYMapElement *element;
    HTList *cur = theList;
    HTList *current;

    if (!cur)
	return;

    while (NULL != (map = (LYImageMap *)HTList_nextObject(cur))) {
	FREE(map->address);
	FREE(map->title);
	if (map->elements) {
	    current = map->elements;
	    while (NULL !=
		   (element = (LYMapElement *)HTList_nextObject(current))) {
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

/*
 *  Utility for freeing the global list of MAPs. - kw
 */
PRIVATE void LYLynxMaps_free NOARGS
{
    ImageMapList_free(LynxMaps);
    LynxMaps = NULL;
    return;
}

/*
 *  We keep two kinds of lists:
 *  - A global list (LynxMaps) shared by MAPs from all documents that
 *    do not have POST data.
 *  - For each response to a POST which contains MAPs, a list specific
 *    to this combination of URL and post_data.  It is kept in the
 *    HTParentAnchor structure and is freed when the document is removed
 *    from memory, in the course of normal removal of anchors.
 *    MAPs from POST responses can only be accessed via internal links,
 *    i.e. from within the same document (with the same post_data).
 *    The notion of "same document" is extended, so that LYNXIMGMAP:
 *    and List Page screens are logically part of the document on which
 *    they are based. - kw
 *
 *  If DONT_TRACK_INTERNAL_LINKS is defined, only the global list will
 *  be used for all MAPs.
 *
 */

/*
 *  Utility for creating an LYImageMap list, if it doesn't
 *  exist already, adding LYImageMap entry structures if needed, and
 *  removing any LYMapElements in a pre-existing LYImageMap entry so that
 *  it will have only those from AREA tags for the current analysis of
 *  MAP element content. - FM
 */
PUBLIC BOOL LYAddImageMap ARGS3(
	char *, 	address,
	char *, 	title,
	HTParentAnchor *, node_anchor)
{
    LYImageMap *new = NULL;
    LYImageMap *old = NULL;
    HTList *cur = NULL;
    HTList *theList = NULL;
    HTList *curele = NULL;
    LYMapElement *ele = NULL;

    if (!(address && *address))
	return FALSE;
    if (!(node_anchor && node_anchor->address))
	return FALSE;

    /*
     *	Set theList to either the global LynxMaps list or, if we
     *	are associated with post data, the specific list.  The
     *	list is created if it doesn't already exist. - kw
     */
#ifndef DONT_TRACK_INTERNAL_LINKS
    if (node_anchor->post_data) {
	/*
	 *  We are handling a MAP element found while parsing
	 *  node_anchor's stream of data, and node_anchor has
	 *  post_data associated and should therefore represent
	 *  a POST response, so use the specific list. - kw
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
	    atexit(LYLynxMaps_free);
	}
	theList = LynxMaps;
    }

    if (theList) {
	cur = theList;
	while (NULL != (old = (LYImageMap *)HTList_nextObject(cur))) {
	    if (!strcmp(old->address, address)) {
		FREE(old->address);
		FREE(old->title);
		if (old->elements) {
		    curele = old->elements;
		    while (NULL !=
			   (ele = (LYMapElement *)HTList_nextObject(curele))) {
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

    new = (old != NULL) ?
		    old : (LYImageMap *)calloc(1, sizeof(LYImageMap));
    if (new == NULL) {
	perror("Out of memory in LYAddImageMap");
	return FALSE;
    }
    StrAllocCopy(new->address, address);
    if (title && *title)
	StrAllocCopy(new->title, title);
    if (new != old)
	HTList_addObject(theList, new);
    return TRUE;
}

/*
 * Utility for adding LYMapElements to LYImageMaps
 * in the appropriate list. - FM
 */
PUBLIC BOOL LYAddMapElement ARGS5(
	char *, 	map,
	char *, 	address,
	char *, 	title,
	HTParentAnchor *, node_anchor,
	BOOL,		intern_flag)
{
    LYMapElement *new = NULL;
    LYImageMap *theMap = NULL;
    HTList *theList = NULL;
    HTList *cur = NULL;

    if (!(map && *map && address && *address))
	return FALSE;
    if (!(node_anchor && node_anchor->address))
	return FALSE;

    /*
     *	Set theList to either the global LynxMaps list or, if we
     *	are associated with post data, the specific list.  The
     *	list should already exist, since this function is only called
     *	if the AREA tag we are handling was within a MAP element
     *	in node_anchor's stream of data, so that LYAddImageMap has
     *	been called. - kw
     */
#ifndef DONT_TRACK_INTERNAL_LINKS
    if (node_anchor->post_data) {
	/*
	 *  We are handling an AREA tag found while parsing
	 *  node_anchor's stream of data, and node_anchor has
	 *  post_data associated and should therefore represent
	 *  a POST response, so use the specific list. - kw
	 */
	theList = node_anchor->imaps;
	if (!theList)
	    return FALSE;
    } else
#endif
    {
	if (!LynxMaps)
	    LYAddImageMap(map, NULL, node_anchor);
	theList = LynxMaps;
    }

    cur = theList;
    while (NULL != (theMap = (LYImageMap *)HTList_nextObject(cur))) {
	if (!strcmp(theMap->address, map)) {
	    break;
	}
    }
    if (!theMap)
	return FALSE;
    if (!theMap->elements)
	theMap->elements = HTList_new();
    cur = theMap->elements;
    while (NULL != (new = (LYMapElement *)HTList_nextObject(cur))) {
	if (!strcmp(new->address, address)) {
	    FREE(new->address);
	    FREE(new->title);
	    HTList_removeObject(theMap->elements, new);
	    FREE(new);
	    break;
	}
    }

    new = (LYMapElement *)calloc(1, sizeof(LYMapElement));
    if (new == NULL) {
	perror("Out of memory in LYAddMapElement");
	return FALSE;
    }
    StrAllocCopy(new->address, address);
    if (title && *title)
	StrAllocCopy(new->title, title);
    else
	StrAllocCopy(new->title, address);
#ifndef DONT_TRACK_INTERNAL_LINKS
    new->intern_flag = intern_flag;
#endif
    HTList_appendObject(theMap->elements, new);
    return TRUE;
}

/*
 *  Utility for checking whether an LYImageMap entry
 *  with a given address already exists in the LynxMaps
 *  structure. - FM
 */
PUBLIC BOOL LYHaveImageMap ARGS1(
	char *, 	address)
{
    LYImageMap *Map;
    HTList *cur = LynxMaps;

    if (!(cur && address && *address != '\0'))
	return FALSE;

    while (NULL != (Map = (LYImageMap *)HTList_nextObject(cur))) {
	if (!strcmp(Map->address, address)) {
	    return TRUE;
	}
    }

    return FALSE;
}

/*
 *  Fills in a DocAddress structure for getting the HTParentAnchor of
 *  the underlying resource.  ALso returns a pointer to that anchor in
 *  *punderlying if we are dealing with POST data. - kw
 *
 *  address  is the address of the underlying resource, i.e. the one
 *	     containing the MAP element, the MAP's name appended as
 *	     fragment is ignored.
 *  anAnchor is the LYNXIMGMAP: anchor; if it is associated with POST
 *	     data, we want the specific list, otherwise the global list.
 */
PRIVATE void fill_DocAddress ARGS4(
    DocAddress *,	wwwdoc,
    char *,		address,
    HTParentAnchor *,	anAnchor,
    HTParentAnchor **,	punderlying)
{
    HTParentAnchor * underlying;
    if (anAnchor && anAnchor->post_data) {
	wwwdoc->address = address;
	wwwdoc->post_data = anAnchor->post_data;
	wwwdoc->post_content_type = anAnchor->post_content_type;
	wwwdoc->bookmark = NULL;
	wwwdoc->isHEAD = FALSE;
	wwwdoc->safe = FALSE;
	underlying = HTAnchor_parent(HTAnchor_findAddress(wwwdoc));
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
 *  Get the appropriate list for creating a LYNXIMGMAP: pseudo-
 *  document: either the global list (LynxMaps), or the specific
 *  list if a List Page for a POST response is requested.  Also
 *  fill in the DocAddress structure etc. by calling fill_DocAddress().
 *
 *  address is the address of the underlying resource, i.e. the one
 *	    containing the MAP element, the MAP's name appended as
 *	    fragment is ignored.
 *  anchor  is the LYNXIMGMAP: anchor for which LYLoadIMGmap() is
 *	    requested; if it is associated with POST data, we want the
 *	    specific list for this combination of address+post_data.
 *
 * if DONT_TRACK_INTERNAL_LINKS is defined, the Anchor passed to
 * LYLoadIMGmap() will never have post_data, so that the global list
 * will be used. - kw
 */
PRIVATE HTList * get_the_list ARGS4(
    DocAddress *,	wwwdoc,
    char *,		address,
    HTParentAnchor *,	anchor,
    HTParentAnchor **,	punderlying)
{
    if (anchor && anchor->post_data) {
	fill_DocAddress(wwwdoc, address, anchor, punderlying);
	if (punderlying && *punderlying)
	    return (*punderlying)->imaps;
	return anchor->imaps;
    } else {
	fill_DocAddress(wwwdoc, address, NULL, punderlying);
	return LynxMaps;
    }
}

/*	LYLoadIMGmap - F.Macrides (macrides@sci.wfeb.edu)
**	------------
**	Create a text/html stream with a list of links
**	for HyperText References in AREAs of a MAP.
*/

PRIVATE int LYLoadIMGmap ARGS4 (
	CONST char *,		arg,
	HTParentAnchor *,	anAnchor,
	HTFormat,		format_out,
	HTStream*,		sink)
{
    HTFormat format_in = WWW_HTML;
    HTStream *target = NULL;
    char buf[1024];
    LYMapElement *new = NULL;
    LYImageMap *theMap = NULL;
    char *MapTitle = NULL;
    char *MapAddress = NULL;
    HTList *theList;
    HTList *cur = NULL;
    char *address = NULL;
    char *cp = NULL;
    DocAddress WWWDoc;
    HTParentAnchor * underlying;
    BOOL old_cache_setting = LYforce_no_cache;
    BOOL old_reloading = reloading;
    HTFormat old_format_out = HTOutputFormat;

    if (!strncasecomp(arg, "LYNXIMGMAP:", 11)) {
	address = (char * )(arg + 11);
    }
    if (!(address && strchr(address, ':'))) {
	HTAlert(MISDIRECTED_MAP_REQUEST);
	return(HT_NOT_LOADED);
    }

    theList = get_the_list(&WWWDoc, address, anAnchor, &underlying);
    if (WWWDoc.safe)
	anAnchor->safe = TRUE;

    if (!theList) {
	if (anAnchor->post_data && !WWWDoc.safe &&
	    ((underlying && underlying->document && !LYforce_no_cache) ||
	     HTConfirm(
#if __STDC__
		"LYNXIMGMAP: "
#endif
		CONFIRM_POST_RESUBMISSION) != TRUE)) {
	    HTAlert("Image map from POST response not available!");
	    return(HT_NOT_LOADED);
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
	    return(HT_NOT_LOADED);
	}
	LYforce_no_cache = old_cache_setting;
	reloading = old_reloading;
	HTOutputFormat = old_format_out;
	LYMapsOnly = FALSE;
	theList = get_the_list(&WWWDoc, address, anAnchor, &underlying);
    }

    if (!theList) {
	HTAlert(MAPS_NOT_AVAILABLE);
	return(HT_NOT_LOADED);
    }

    cur = theList;
    while (NULL != (theMap = (LYImageMap *)HTList_nextObject(cur))) {
	if (!strcmp(theMap->address, address)) {
	    break;
	}
    }
    if (!(theMap && theMap->elements)) {
	if (anAnchor->post_data && !WWWDoc.safe &&
	    ((underlying && underlying->document && !LYforce_no_cache) ||
	    HTConfirm(
#if __STDC__
		"LYNXIMGMAP: "
#endif
		CONFIRM_POST_RESUBMISSION) != TRUE)) {
	    HTAlert("Image map from POST response not available!");
	    return(HT_NOT_LOADED);
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
	    return(HT_NOT_LOADED);
	}
	LYforce_no_cache = old_cache_setting;
	reloading = old_reloading;
	HTOutputFormat = old_format_out;
	LYMapsOnly = FALSE;
	cur = get_the_list(&WWWDoc, address, anAnchor, &underlying);
	while (NULL != (theMap = (LYImageMap *)HTList_nextObject(cur))) {
	    if (!strcmp(theMap->address, address)) {
		break;
	    }
	}
	if (!(theMap && theMap->elements)) {
	    HTAlert(MAP_NOT_AVAILABLE);
	    return(HT_NOT_LOADED);
	}
    }

#ifdef DONT_TRACK_INTERNAL_LINKS
    anAnchor->no_cache = TRUE;
#endif

    target = HTStreamStack(format_in,
			   format_out,
			   sink, anAnchor);

    if (!target || target == NULL) {
	sprintf(buf, CANNOT_CONVERT_I_TO_O,
		HTAtom_name(format_in), HTAtom_name(format_out));
	HTAlert(buf);
	return(HT_NOT_LOADED);
    }

    if (theMap->title && *theMap->title) {
	StrAllocCopy(MapTitle, theMap->title);
    } else if (anAnchor->title && *anAnchor->title) {
	StrAllocCopy(MapTitle, anAnchor->title);
    } else if (LYRequestTitle && *LYRequestTitle &&
	       strcasecomp(LYRequestTitle, "[USEMAP]")) {
	StrAllocCopy(MapTitle, LYRequestTitle);
    } else if ((cp=strrchr(address, '#')) != NULL) {
	StrAllocCopy(MapTitle, (cp+1));
    }
    if (!(MapTitle && *MapTitle)) {
	StrAllocCopy(MapTitle, "[USEMAP]");
    } else {
	LYEntify(&MapTitle, TRUE);
    }

    sprintf(buf,"<head>\n<title>%s</title>\n</head>\n<body>\n", MapTitle);
    (*target->isa->put_block)(target, buf, strlen(buf));

    sprintf(buf,"<h1><em>%s</em></h1>\n", MapTitle);
    (*target->isa->put_block)(target, buf, strlen(buf));

    StrAllocCopy(MapAddress, address);
    LYEntify(&MapAddress, FALSE);
    sprintf(buf,"<h2><em>MAP:</em>&nbsp;%s</h2>\n", MapAddress);
    (*target->isa->put_block)(target, buf, strlen(buf));

    sprintf(buf, "<%s compact>\n", ((keypad_mode == NUMBERS_AS_ARROWS) ?
				    "ol" : "ul"));
    (*target->isa->put_block)(target, buf, strlen(buf));
    cur = theMap->elements;
    while (NULL != (new=(LYMapElement *)HTList_nextObject(cur))) {
	StrAllocCopy(MapAddress, new->address);
	LYEntify(&MapAddress, FALSE);
	(*target->isa->put_block)(target, "<li><a href=\"", 13);
	(*target->isa->put_block)(target, MapAddress, strlen(MapAddress));
#ifndef DONT_TRACK_INTERNAL_LINKS
	if (new->intern_flag)
	    (*target->isa->put_block)(target, "\" TYPE=\"internal link\"\n>",24);
	else
#endif
	    (*target->isa->put_block)(target, "\"\n>", 3);
	StrAllocCopy(MapTitle, new->title);
	LYEntify(&MapTitle, TRUE);
	(*target->isa->put_block)(target, MapTitle, strlen(MapTitle));
	(*target->isa->put_block)(target, "</a>\n", 5);
    }
    sprintf(buf,"</%s>\n</body>\n", ((keypad_mode == NUMBERS_AS_ARROWS) ?
				     "ol" : "ul"));
    (*target->isa->put_block)(target, buf, strlen(buf));

    (*target->isa->_free)(target);
    FREE(MapAddress);
    FREE(MapTitle);
    return(HT_LOADED);
}

#ifdef GLOBALDEF_IS_MACRO
#define _LYIMGMAP_C_GLOBALDEF_1_INIT { "LYNXIMGMAP", LYLoadIMGmap, 0}
GLOBALDEF (HTProtocol,LYLynxIMGmap,_LYIMGMAP_C_GLOBALDEF_1_INIT);
#else
GLOBALDEF PUBLIC HTProtocol LYLynxIMGmap = {"LYNXIMGMAP", LYLoadIMGmap, 0};
#endif /* GLOBALDEF_IS_MACRO */
