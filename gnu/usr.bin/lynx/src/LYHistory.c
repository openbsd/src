#include <HTUtils.h>
#include <HTTP.h>
#include <GridText.h>
#include <HTAlert.h>
#include <HText.h>
#include <LYGlobalDefs.h>
#include <LYUtils.h>
#include <LYHistory.h>
#include <LYPrint.h>
#include <LYDownload.h>
#include <LYOptions.h>
#include <LYKeymap.h>
#include <LYList.h>
#include <LYShowInfo.h>
#include <LYStrings.h>
#include <LYCharUtils.h>
#include <LYGetFile.h>

#ifdef DIRED_SUPPORT
#include <LYUpload.h>
#include <LYLocal.h>
#endif /* DIRED_SUPPORT */

#include <LYexit.h>
#include <LYLeaks.h>

PUBLIC HTList * Visited_Links = NULL;	/* List of safe popped docs. */

#ifdef LY_FIND_LEAKS
/*
 *  Utility for freeing the list of visited links. - FM
 */
PRIVATE void Visited_Links_free NOARGS
{
    VisitedLink *vl;
    HTList *cur = Visited_Links;

    if (!cur)
	return;

    while (NULL != (vl = (VisitedLink *)HTList_nextObject(cur))) {
	FREE(vl->address);
	FREE(vl->title);
	FREE(vl);
    }
    HTList_delete(Visited_Links);
    Visited_Links = NULL;
    return;
}
#endif /* LY_FIND_LEAKS */

/*
 *  Utility for listing visited links, making any repeated
 *  links the most current in the list. - FM
 */
PUBLIC void LYAddVisitedLink ARGS1(
	document *,	doc)
{
    VisitedLink *new;
    VisitedLink *old;
    HTList *cur;

    if (!(doc->address && *doc->address))
	return;

    /*
     *	Exclude POST or HEAD replies, and bookmark, menu
     *	or list files. - FM
     */
    if (doc->post_data || doc->isHEAD || doc->bookmark ||
	(/* special url or a temp file */
	 (!strncmp(doc->address, "LYNX", 4) ||
	  !strncmp(doc->address, "file://localhost/", 17))
	 && (
	!strcmp((doc->title ? doc->title : ""), HISTORY_PAGE_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), PRINT_OPTIONS_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), DOWNLOAD_OPTIONS_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), OPTIONS_TITLE) ||
#ifdef DIRED_SUPPORT
	!strcmp((doc->title ? doc->title : ""), DIRED_MENU_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), UPLOAD_OPTIONS_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), PERMIT_OPTIONS_TITLE) ||
#endif /* DIRED_SUPPORT */
	!strcmp((doc->title ? doc->title : ""), CURRENT_KEYMAP_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), LIST_PAGE_TITLE) ||
#ifdef EXP_ADDRLIST_PAGE
	!strcmp((doc->title ? doc->title : ""), ADDRLIST_PAGE_TITLE) ||
#endif
	!strcmp((doc->title ? doc->title : ""), SHOWINFO_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), STATUSLINES_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), CONFIG_DEF_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), LYNXCFG_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), COOKIE_JAR_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), VISITED_LINKS_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), LYNX_TRACELOG_TITLE)))) {
	return;
    }

    if ((new = (VisitedLink *)calloc(1, sizeof(*new))) == NULL)
	outofmem(__FILE__, "LYAddVisitedLink");
    StrAllocCopy(new->address, doc->address);
    StrAllocCopy(new->title, (doc->title ? doc->title : NO_TITLE));

    if (!Visited_Links) {
	Visited_Links = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(Visited_Links_free);
#endif
	HTList_addObject(Visited_Links, new);
	return;
    }

    cur = Visited_Links;
    while (NULL != (old = (VisitedLink *)HTList_nextObject(cur))) {
	if (!strcmp((old->address ? old->address : ""),
		    (new->address ? new->address : "")) &&
	    !strcmp((old->title ? new->title : ""),
		    (new->title ? new->title : ""))) {
	    FREE(old->address);
	    FREE(old->title);
	    HTList_removeObject(Visited_Links, old);
	    FREE(old);
	    break;
	}
    }
    HTList_addObject(Visited_Links, new);

    return;
}

/*
 *  Returns true if this is a page that we would push onto the stack if not
 *  forced.
 */
PUBLIC BOOLEAN LYwouldPush ARGS1(
	char *,	title)
{
    return (!strcmp(title, HISTORY_PAGE_TITLE)
	 || !strcmp(title, PRINT_OPTIONS_TITLE)
	 || !strcmp(title, DOWNLOAD_OPTIONS_TITLE)
#ifdef DIRED_SUPPORT
	 || !strcmp(title, DIRED_MENU_TITLE)
	 || !strcmp(title, UPLOAD_OPTIONS_TITLE)
	 || !strcmp(title, PERMIT_OPTIONS_TITLE)
#endif /* DIRED_SUPPORT */
	 )
	 ? FALSE
	 : TRUE;
}

/*
 *  Push the current filename, link and line number onto the history list.
 */
PUBLIC void LYpush ARGS2(
	document *,	doc,
	BOOLEAN,	force_push)
{
    /*
     *	Don't push NULL file names.
     */
    if (*doc->address == '\0')
	return;

    /*
     *	Check whether this is a document we
     *	don't push unless forced. - FM
     */
    if (!force_push) {
	/*
	 *  Don't push the history, printer, or download lists.
	 */
	if (!LYwouldPush(doc->title)) {
	    if (!LYforce_no_cache)
		LYoverride_no_cache = TRUE;
	    return;
	}
    }

    /*
     *	If file is identical to one before it, don't push it.
     */
    if (nhist> 1 &&
	STREQ(history[nhist-1].address, doc->address) &&
	!strcmp(history[nhist-1].post_data ?
		history[nhist-1].post_data : "",
		doc->post_data ?
		doc->post_data : "") &&
	!strcmp(history[nhist-1].bookmark ?
		history[nhist-1].bookmark : "",
		doc->bookmark ?
		doc->bookmark : "") &&
	history[nhist-1].isHEAD == doc->isHEAD) {
	if (history[nhist-1].internal_link == doc->internal_link) {
	    /* But it is nice to have the last position remembered!
	       - kw */
	    history[nhist-1].link = doc->link;
	    history[nhist-1].line = doc->line;
	    return;
	}
    }
    /*
     *	OK, push it if we have stack space.
     */
    if (nhist < MAXHIST)  {
	history[nhist].link = doc->link;
	history[nhist].line = doc->line;
	history[nhist].title = NULL;
	StrAllocCopy(history[nhist].title, doc->title);
	history[nhist].address = NULL;
	StrAllocCopy(history[nhist].address, doc->address);
	history[nhist].post_data = NULL;
	StrAllocCopy(history[nhist].post_data, doc->post_data);
	history[nhist].post_content_type = NULL;
	StrAllocCopy(history[nhist].post_content_type, doc->post_content_type);
	history[nhist].bookmark = NULL;
	StrAllocCopy(history[nhist].bookmark, doc->bookmark);
	history[nhist].isHEAD = doc->isHEAD;
	history[nhist].safe = doc->safe;

	history[nhist].internal_link = FALSE; /* by default */
	history[nhist].intern_seq_start = -1; /* by default */
	if (doc->internal_link) {
	    /* Now some tricky stuff: if the caller thinks that the doc
	       to push was the result of following an internal
	       (fragment) link, we check whether we believe it.
	       It is only accepted as valid if the immediately preceding
	       item on the history stack is actually the same document
	       except for fragment and location info.  I.e. the Parent
	       Anchors are the same.
	       Also of course this requires that this is not the first
	       history item. - kw */
	    if (nhist > 0) {
		DocAddress WWWDoc;
		HTParentAnchor *thisparent, *thatparent = NULL;
		WWWDoc.address = doc->address;
		WWWDoc.post_data = doc->post_data;
		WWWDoc.post_content_type = doc->post_content_type;
		WWWDoc.bookmark = doc->bookmark;
		WWWDoc.isHEAD = doc->isHEAD;
		WWWDoc.safe = doc->safe;
		thisparent =
		    HTAnchor_parent(HTAnchor_findAddress(&WWWDoc));
		/* Now find the ParentAnchor for the previous history
		** item - kw
		*/
		if (thisparent) {
		    /* If the last-pushed item is a LYNXIMGMAP but THIS one
		    ** isn't, compare the physical URLs instead. - kw
		    */
		    if (0==strncmp(history[nhist-1].address,"LYNXIMGMAP:",11) &&
			0!=strncmp(doc->address,"LYNXIMGMAP:",11)) {
			WWWDoc.address = history[nhist-1].address + 11;
		    /*
		    ** If THIS item is a LYNXIMGMAP but the last-pushed one
		    ** isn't, fake it by using THIS item's address for
		    ** thatparent... - kw
		    */
		    } else if ((0==strncmp(doc->address,"LYNXIMGMAP:",11) &&
		       0!=strncmp(history[nhist-1].address,"LYNXIMGMAP:",11))) {
			char *temp = NULL;
			StrAllocCopy(temp, "LYNXIMGMAP:");
			StrAllocCat(temp, doc->address+11);
			WWWDoc.address = temp;
			WWWDoc.post_content_type = history[nhist-1].post_content_type;
			WWWDoc.bookmark = history[nhist-1].bookmark;
			WWWDoc.isHEAD = history[nhist-1].isHEAD;
			WWWDoc.safe = history[nhist-1].safe;
			thatparent =
			    HTAnchor_parent(HTAnchor_findAddress(&WWWDoc));
			FREE(temp);
		    } else {
			WWWDoc.address = history[nhist-1].address;
		    }
		    if (!thatparent) { /* if not yet done */
			WWWDoc.post_data = history[nhist-1].post_data;
			WWWDoc.post_content_type = history[nhist-1].post_content_type;
			WWWDoc.bookmark = history[nhist-1].bookmark;
			WWWDoc.isHEAD = history[nhist-1].isHEAD;
			WWWDoc.safe = history[nhist-1].safe;
			thatparent =
			    HTAnchor_parent(HTAnchor_findAddress(&WWWDoc));
		    }
		/* In addition to equality of the ParentAnchors, require
		** that IF we have a HTMainText (i.e., it wasn't just
		** HTuncache'd by mainloop), THEN it has to be consistent
		** with what we are trying to push.
		**   This may be overkill... - kw
		*/
		    if (thatparent == thisparent &&
			(!HTMainText || HTMainAnchor == thisparent)
			) {
			history[nhist].internal_link = TRUE;
			history[nhist].intern_seq_start =
			    history[nhist-1].intern_seq_start >= 0 ?
			    history[nhist-1].intern_seq_start : nhist-1;
			CTRACE(tfp, "\nLYpush: pushed as internal link, OK\n");
		    }
		}
	    }
	    if (!history[nhist].internal_link) {
		CTRACE(tfp, "\nLYpush: push as internal link requested, %s\n",
			    "but didn't check out!");
	    }
	}
	CTRACE(tfp, "\nLYpush[%d]: address:%s\n        title:%s\n",
		    nhist, doc->address, doc->title);
	nhist++;
    } else {
	if (LYCursesON) {
	    HTAlert(MAXHIST_REACHED);
	}
	CTRACE(tfp, "\nLYpush: MAXHIST reached for:\n        address:%s\n        title:%s\n",
		    doc->address, doc->title);
    }
}

/*
 *  Pop the previous filename, link and line number from the history list.
 */
PUBLIC void LYpop ARGS1(
	document *,	doc)
{
    if (nhist > 0) {
	nhist--;
	doc->link = history[nhist].link;
	doc->line = history[nhist].line;
	FREE(doc->title);
	doc->title = history[nhist].title;	 /* will be freed later */
	FREE(doc->address);
	doc->address = history[nhist].address;	 /* will be freed later */
	FREE(doc->post_data);
	doc->post_data = history[nhist].post_data;
	FREE(doc->post_content_type);
	doc->post_content_type = history[nhist].post_content_type;
	FREE(doc->bookmark);
	doc->bookmark = history[nhist].bookmark; /* will be freed later */
	doc->isHEAD = history[nhist].isHEAD;
	doc->safe = history[nhist].safe;
	doc->internal_link = history[nhist].internal_link;
#ifdef DISP_PARTIAL
	/* assume we pop the 'doc' to show it soon... */
	Newline_partial = doc->line;	/* reinitialize */
#endif /* DISP_PARTIAL */
	CTRACE(tfp, "LYpop[%d]: address:%s\n     title:%s\n",
		    nhist, doc->address, doc->title);
    }
}

/*
 *  Pop the specified hist entry, link and line number from the history
 *  list but don't actually remove the entry, just return it.
 *  (This procedure is badly named :)
 */
PUBLIC void LYpop_num ARGS2(
	int,		number,
	document *,	doc)
{
    if (number >= 0 && nhist > number) {
	doc->link = history[number].link;
	doc->line = history[number].line;
	StrAllocCopy(doc->title, history[number].title);
	StrAllocCopy(doc->address, history[number].address);
	StrAllocCopy(doc->post_data, history[number].post_data);
	StrAllocCopy(doc->post_content_type, history[number].post_content_type);
	StrAllocCopy(doc->bookmark, history[number].bookmark);
	doc->isHEAD = history[number].isHEAD;
	doc->safe = history[number].safe;
	doc->internal_link = history[number].internal_link; /* ?? */
#ifdef DISP_PARTIAL
	/* assume we pop the 'doc' to show it soon... */
	Newline_partial = doc->line;	/* reinitialize */
#endif /* DISP_PARTIAL */
    }
}

/*
 *  This procedure outputs the history buffer into a temporary file.
 */
PUBLIC int showhistory ARGS1(
	char **,	newfile)
{
    static char tempfile[LY_MAXPATH];
    char *Title = NULL;
    int x = 0;
    FILE *fp0;

    LYRemoveTemp(tempfile);
    if ((fp0 = LYOpenTemp(tempfile, HTML_SUFFIX, "w")) == NULL) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(-1);
    }

    LYLocalFileToURL(newfile, tempfile);

    LYforce_HTML_mode = TRUE;	/* force this file to be HTML */
    LYforce_no_cache = TRUE;	/* force this file to be new */

    BeginInternalPage(fp0, HISTORY_PAGE_TITLE, HISTORY_PAGE_HELP);

    fprintf(fp0, "<tr align=right> <a href=\"LYNXMESSAGES:\">[%s]</a> </tr>\n",
		 STATUSLINES_TITLE);

    fprintf(fp0, "<pre>\n");

    fprintf(fp0, "<em>%s</em>\n", gettext("You selected:"));
    for (x = nhist-1; x >= 0; x--) {
	/*
	 *  The number of the document in the hist stack,
	 *  its title in a link, and its address. - FM
	 */
	if (history[x].title != NULL) {
	    StrAllocCopy(Title, history[x].title);
	    LYEntify(&Title, TRUE);
	    LYTrimLeading(Title);
	    LYTrimTrailing(Title);
	    if (*Title == '\0')
		StrAllocCopy(Title , NO_TITLE);
	} else {
	    StrAllocCopy(Title, NO_TITLE);
	}
	fprintf(fp0,
		"%s<em>%d</em>. <tab id=t%d><a href=\"LYNXHIST:%d\">%s</a>\n",
		(x > 99 ? "" : x < 10 ? "  " : " "),
		x, x, x, Title);
	if (history[x].address != NULL) {
	    StrAllocCopy(Title, history[x].address);
	    LYEntify(&Title, TRUE);
	} else {
	    StrAllocCopy(Title, gettext("(no address)"));
	}
	if (history[x].internal_link) {
	    if (history[x].intern_seq_start == history[nhist-1].intern_seq_start)
		StrAllocCat(Title, gettext(" (internal)"));
	    else
		StrAllocCat(Title, gettext(" (was internal)"));
	}
	fprintf(fp0, "<tab to=t%d>%s\n", x, Title);
    }
    fprintf(fp0,"</pre>\n");
    EndInternalPage(fp0);

    LYCloseTempFP(fp0);
    FREE(Title);
    return(0);
}

/*
 *  This function makes the history page seem like any other type of
 *  file since more info is needed than can be provided by the normal
 *  link structure.  We saved out the history number to a special URL.
 *  The info looks like:  LYNXHIST:#
 */
PUBLIC BOOLEAN historytarget ARGS1(
	document *,	newdoc)
{
    int number;
    DocAddress WWWDoc;
    HTParentAnchor *tmpanchor;
    HText *text;
    BOOLEAN treat_as_intern = FALSE;

    if ((!newdoc || !newdoc->address) ||
	strlen(newdoc->address) < 10 || !isdigit(*(newdoc->address+9)))
	return(FALSE);

    if ((number = atoi(newdoc->address+9)) > nhist || number < 0)
	return(FALSE);

    /*
     * Optimization: assume we came from the History Page,
     * so never return back - always a new version next time.
     */
    HTuncache_current_document();  /* don't waste the cache */

    LYpop_num(number, newdoc);
    if (((newdoc->internal_link &&
	  history[number].intern_seq_start == history[nhist-1].intern_seq_start) ||
	 (number < nhist-1 &&
	  history[nhist-1].internal_link &&
	  number == history[nhist-1].intern_seq_start))
	&& !(LYforce_no_cache == TRUE && LYoverride_no_cache == FALSE)) {
#ifndef DONT_TRACK_INTERNAL_LINKS
	LYforce_no_cache = FALSE;
	LYinternal_flag = TRUE;
	newdoc->internal_link = TRUE;
	treat_as_intern = TRUE;
#endif
    } else {
	newdoc->internal_link = FALSE;
    }
    /*
     *	If we have POST content, and have LYresubmit_posts set
     *	or have no_cache set or do not still have the text cached,
     *	ask the user whether to resubmit the form. - FM
     */
    if (newdoc->post_data != NULL) {
	WWWDoc.address = newdoc->address;
	WWWDoc.post_data = newdoc->post_data;
	WWWDoc.post_content_type = newdoc->post_content_type;
	WWWDoc.bookmark = newdoc->bookmark;
	WWWDoc.isHEAD = newdoc->isHEAD;
	WWWDoc.safe = newdoc->safe;
	tmpanchor = HTAnchor_parent(HTAnchor_findAddress(&WWWDoc));
	text = (HText *)HTAnchor_document(tmpanchor);
	if (((((LYresubmit_posts == TRUE) ||
	       (LYforce_no_cache == TRUE &&
		LYoverride_no_cache == FALSE)) &&
	      !(treat_as_intern && !reloading)) ||
	     text == NULL) &&
	    (!strncmp(newdoc->address, "LYNXIMGMAP:", 11) ||
	     HTConfirm(CONFIRM_POST_RESUBMISSION) == TRUE)) {
	    LYforce_no_cache = TRUE;
	    LYoverride_no_cache = FALSE;
	} else if (text != NULL) {
	    LYforce_no_cache = FALSE;
	    LYoverride_no_cache = TRUE;
	} else {
	    HTInfoMsg(CANCELLED);
	    return(FALSE);
	}
    }

    if (number != 0)
	StrAllocCat(newdoc->title, gettext(" (From History)"));
    return(TRUE);
}

/*
 *  This procedure outputs the Visited Links
 *  list into a temporary file. - FM
 */
PUBLIC int LYShowVisitedLinks ARGS1(
	char **,	newfile)
{
    static char tempfile[LY_MAXPATH];
    char *Title = NULL;
    char *Address = NULL;
    int x;
    FILE *fp0;
    VisitedLink *vl;
    HTList *cur = Visited_Links;

    if (!cur)
	return(-1);

    LYRemoveTemp(tempfile);
    if ((fp0 = LYOpenTemp(tempfile, HTML_SUFFIX, "w")) == NULL) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(-1);
    }

    LYLocalFileToURL(newfile, tempfile);

    LYforce_HTML_mode = TRUE;	/* force this file to be HTML */
    LYforce_no_cache = TRUE;	/* force this file to be new */

    BeginInternalPage(fp0, VISITED_LINKS_TITLE, VISITED_LINKS_HELP);

    fprintf(fp0, "<pre>\n");
    fprintf(fp0, "<em>%s</em>\n",
	    gettext("You visited (POSTs, bookmark, menu and list files excluded):"));
    x = HTList_count(Visited_Links);
    while (NULL != (vl = (VisitedLink *)HTList_nextObject(cur))) {
	/*
	 *  The number of the document (most recent highest),
	 *  its title in a link, and its address. - FM
	 */
	x--;
	if (vl->title != NULL && *vl->title != '\0') {
	    StrAllocCopy(Title, vl->title);
	    LYEntify(&Title, TRUE);
	    LYTrimLeading(Title);
	    LYTrimTrailing(Title);
	    if (*Title == '\0')
		StrAllocCopy(Title , NO_TITLE);
	} else {
	    StrAllocCopy(Title , NO_TITLE);
	}
	if (vl->address != NULL && *vl->address != '\0') {
	    StrAllocCopy(Address, vl->address);
	    LYEntify(&Address, FALSE);
	    fprintf(fp0,
		    "%s<em>%d</em>. <tab id=t%d><a href=\"%s\">%s</a>\n",
		    (x > 99 ? "" : x < 10 ? "  " : " "),
		    x, x, Address, Title);
	} else {
	    fprintf(fp0,
		    "%s<em>%d</em>. <tab id=t%d><em>%s</em>\n",
		    (x > 99 ? "" : x < 10 ? "  " : " "),
		    x, x, Title);
	}
	if (Address != NULL) {
	    StrAllocCopy(Address, vl->address);
	    LYEntify(&Address, TRUE);
	}
	fprintf(fp0, "<tab to=t%d>%s\n", x,
		     ((Address != NULL) ? Address : gettext("(no address)")));
    }
    fprintf(fp0,"</pre>\n");
    EndInternalPage(fp0);

    LYCloseTempFP(fp0);
    FREE(Title);
    FREE(Address);
    return(0);
}


/*
 *  Keep cycled buffer for statusline messages.
 */
#define STATUSBUFSIZE   40
PRIVATE char * buffstack[STATUSBUFSIZE];
PRIVATE int topOfStack = 0;
#ifdef LY_FIND_LEAKS
PRIVATE int already_registered_free_messages_stack = 0;
#endif

#ifdef LY_FIND_LEAKS
PRIVATE void free_messages_stack NOARGS
{
    topOfStack = STATUSBUFSIZE;

    while (--topOfStack >= 0) {
	FREE(buffstack[topOfStack]);
    }
}
#endif

PRIVATE void to_stack ARGS1(char *, str)
{
    /*
     *  Cycle buffer:
     */
    if (topOfStack == STATUSBUFSIZE) {
	topOfStack = 0;
    }

    /*
     *  Register string.
     */
    FREE(buffstack[topOfStack]);
    buffstack[topOfStack] = str;
    topOfStack++;
#ifdef LY_FIND_LEAKS
    if(!already_registered_free_messages_stack) {
	already_registered_free_messages_stack = 1;
	atexit(free_messages_stack);
    }
#endif
}


/*
 *  Status line messages list, LYNXMESSAGES:/ internal page,
 *  called from getfile() cyrcle.
 */
PUBLIC int LYshow_statusline_messages ARGS1(
    document *,			      newdoc)
{
    static char tempfile[LY_MAXPATH];
    static char *info_url;
    DocAddress WWWDoc;  /* need on exit */
    FILE *fp0;
    int i;

    LYRemoveTemp(tempfile);
    if ((fp0 = LYOpenTemp (tempfile, HTML_SUFFIX, "w")) == 0) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(NOT_FOUND);
    }
    LYLocalFileToURL(&info_url, tempfile);

    LYforce_no_cache = TRUE;  /* don't cache this doc */

    BeginInternalPage (fp0, STATUSLINES_TITLE, NULL);
    fprintf(fp0, "<pre>\n");
    fprintf(fp0, "<ol>\n");

    /* print messages in reverse order: */
    i = topOfStack;
    while (--i >= 0) {
	if (buffstack[i] != NULL)
	    fprintf(fp0,  "<li> <em>%s</em>\n",  buffstack[i]);
    }
    i = STATUSBUFSIZE;
    while (--i >= topOfStack) {
	if (buffstack[i] != NULL)
	fprintf(fp0,  "<li> <em>%s</em>\n",  buffstack[i]);
    }

    fprintf(fp0, "</ol>\n");
    fprintf(fp0, "</pre>\n");
    EndInternalPage(fp0);
    LYCloseTempFP(fp0);


    /* exit to getfile() cyrcle */
    StrAllocCopy(newdoc->address, info_url);
    WWWDoc.address = newdoc->address;
    WWWDoc.post_data = newdoc->post_data;
    WWWDoc.post_content_type = newdoc->post_content_type;
    WWWDoc.bookmark = newdoc->bookmark;
    WWWDoc.isHEAD = newdoc->isHEAD;
    WWWDoc.safe = newdoc->safe;

    if (!HTLoadAbsolute(&WWWDoc))
	return(NOT_FOUND);
    return(NORMAL);
}

/*
 * Dump statusline messages into the buffer.
 * Called from mainloop() when exit immediately with an error:
 * can not access startfile (first_file) so a couple of alert messages
 * will be very useful on exit.
 * (Don't expect everyone will look a trace log in case of difficulties:))
 */
PUBLIC void LYstatusline_messages_on_exit ARGS1(
	char **,	buf)
{
    int i;

    StrAllocCat(*buf, "\n");
    /* print messages in chronological order:
     * probably a single message but let's do it.
     */
    i = topOfStack - 1;
    while (++i <= STATUSBUFSIZE) {
	if (buffstack[i] != NULL) {
	    StrAllocCat(*buf, buffstack[i]);
	    StrAllocCat(*buf, "\n");
	}
    }
    i = -1;
    while (++i < topOfStack) {
	if (buffstack[i] != NULL) {
	    StrAllocCat(*buf, buffstack[i]);
	    StrAllocCat(*buf, "\n");
	}
    }
}


PUBLIC void LYstore_message2 ARGS2(
	CONST char *,	message,
	CONST char *,	argument)
{

    if (message != NULL) {
	char *temp = NULL;
	HTSprintf(&temp, message, (argument == 0) ? "" : argument);
	to_stack(temp);
    }
}

PUBLIC void LYstore_message ARGS1(
	CONST char *,	message)
{
    if (message != NULL) {
	char *temp = NULL;
	StrAllocCopy(temp, message);
	to_stack(temp);
    }
}
