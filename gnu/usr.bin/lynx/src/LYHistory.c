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
#include <LYCharSets.h>
#include <LYrcFile.h>
#ifdef DISP_PARTIAL
#include <LYMainLoop.h>
#endif

#ifdef DIRED_SUPPORT
#include <LYUpload.h>
#include <LYLocal.h>
#endif /* DIRED_SUPPORT */

#include <LYexit.h>
#include <LYLeaks.h>
#include <HTCJK.h>

PUBLIC HTList * Visited_Links = NULL;	/* List of safe popped docs. */
PUBLIC int Visited_Links_As = VISITED_LINKS_AS_LATEST | VISITED_LINKS_REVERSE;
PRIVATE VisitedLink *PrevVisitedLink = NULL;	    /* NULL on auxillary */
PRIVATE VisitedLink *PrevActiveVisitedLink = NULL;  /* Last non-auxillary */
PRIVATE VisitedLink Latest_first;
PRIVATE VisitedLink Latest_last;
PRIVATE VisitedLink *Latest_tree;
PRIVATE VisitedLink *First_tree;
PRIVATE VisitedLink *Last_by_first;

int nhist_extra;

#ifdef LY_FIND_LEAKS
PRIVATE int already_registered_free_messages_stack = 0;
PRIVATE int already_registered_clean_all_history = 0;
#endif

#ifdef LY_FIND_LEAKS
/*
 *  Utility for freeing the list of visited links. - FM
 */
PRIVATE void Visited_Links_free NOARGS
{
    VisitedLink *vl;
    HTList *cur = Visited_Links;

    PrevVisitedLink = NULL;
    PrevActiveVisitedLink = NULL;
    if (!cur)
	return;

    while (NULL != (vl = (VisitedLink *)HTList_nextObject(cur))) {
	FREE(vl->address);
	FREE(vl->title);
	FREE(vl);
    }
    HTList_delete(Visited_Links);
    Visited_Links = NULL;
    Latest_last.prev_latest = &Latest_first;
    Latest_first.next_latest = &Latest_last;
    Last_by_first = Latest_tree = First_tree = 0;
    return;
}
#endif /* LY_FIND_LEAKS */

#ifdef DEBUG
PRIVATE void trace_history ARGS1(
	CONST char *,	tag)
{
    if (TRACE) {
	CTRACE((tfp, "HISTORY %s %d/%d (%d extra)\n",
		     tag, nhist, MAXHIST, nhist_extra));
	CTRACE_FLUSH(tfp);
    }
}
#else
#define trace_history(tag) /* nothing */
#endif /* DEBUG */

/*
 *  Utility for listing visited links, making any repeated
 *  links the most current in the list. - FM
 */
PUBLIC void LYAddVisitedLink ARGS1(
	DocInfo *,	doc)
{
    VisitedLink *new;
    HTList *cur;
    char *title = (doc->title ? doc->title : NO_TITLE);

    if (!(doc->address && *doc->address)) {
	PrevVisitedLink = NULL;
	return;
    }

    /*
     *	Exclude POST or HEAD replies, and bookmark, menu
     *	or list files. - FM
     */
    if (doc->post_data || doc->isHEAD || doc->bookmark ||
	(/* special url or a temp file */
	 (!strncmp(doc->address, "LYNX", 4) ||
	  !strncmp(doc->address, "file://localhost/", 17)))) {
	int related = 1;	/* First approximation only */

	if (	LYIsUIPage(doc->address, UIP_HISTORY) ||
		LYIsUIPage(doc->address, UIP_VLINKS) ||
		LYIsUIPage(doc->address, UIP_SHOWINFO) ||
		isLYNXMESSAGES(doc->address) ||
			(related = 0)	||
#ifdef DIRED_SUPPORT
		LYIsUIPage(doc->address, UIP_DIRED_MENU) ||
		LYIsUIPage(doc->address, UIP_UPLOAD_OPTIONS) ||
		LYIsUIPage(doc->address, UIP_PERMIT_OPTIONS) ||
#endif /* DIRED_SUPPORT */
		LYIsUIPage(doc->address, UIP_PRINT_OPTIONS) ||
		LYIsUIPage(doc->address, UIP_DOWNLOAD_OPTIONS) ||
		LYIsUIPage(doc->address, UIP_OPTIONS_MENU) ||
		isLYNXKEYMAP(doc->address) ||
		LYIsUIPage(doc->address, UIP_LIST_PAGE) ||
#ifdef EXP_ADDRLIST_PAGE
		LYIsUIPage(doc->address, UIP_ADDRLIST_PAGE) ||
#endif
		LYIsUIPage(doc->address, UIP_CONFIG_DEF) ||
		LYIsUIPage(doc->address, UIP_LYNXCFG) ||
		isLYNXCOOKIE(doc->address) ||
		LYIsUIPage(doc->address, UIP_TRACELOG)	) {
	    if (!related)
		PrevVisitedLink = NULL;
	    return;
	}
    }

    if (!Visited_Links) {
	Visited_Links = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(Visited_Links_free);
#endif
	Latest_last.prev_latest = &Latest_first;
	Latest_first.next_latest = &Latest_last;
	Latest_last.next_latest = NULL;		/* Find bugs quick! */
	Latest_first.prev_latest = NULL;
	Last_by_first = Latest_tree = First_tree = NULL;
    }

    cur = Visited_Links;
    while (NULL != (new = (VisitedLink *)HTList_nextObject(cur))) {
	if (!strcmp(NonNull(new->address),
		    NonNull(doc->address))) {
	    PrevVisitedLink = PrevActiveVisitedLink = new;
	    /* Already visited.  Update the last-visited info. */
	    if (new->next_latest == &Latest_last)	/* optimization */
		return;

	    /* Remove from "latest" chain */
	    new->prev_latest->next_latest = new->next_latest;
	    new->next_latest->prev_latest = new->prev_latest;

	    /* Insert at the end of the "latest" chain */
	    Latest_last.prev_latest->next_latest = new;
	    new->prev_latest = Latest_last.prev_latest;
	    new->next_latest = &Latest_last;
	    Latest_last.prev_latest = new;
	    return;
	}
    }

    if ((new = typecalloc(VisitedLink)) == NULL)
	outofmem(__FILE__, "LYAddVisitedLink");
    StrAllocCopy(new->address, doc->address);
    LYformTitle(&(new->title), title);

    /* First-visited chain */
    HTList_appendObject(Visited_Links, new);	/* At end */
    new->prev_first = Last_by_first;
    Last_by_first = new;

    /* Tree structure */
    if (PrevVisitedLink) {
	VisitedLink *a = PrevVisitedLink;
	VisitedLink *b = a->next_tree;
	int l = PrevVisitedLink->level;

	/* Find last on the deeper levels */
	while (b && b->level > l)
	    a = b, b = b->next_tree;

	if (!b)			/* a == Latest_tree */
	    Latest_tree = new;
	new->next_tree = a->next_tree;
	a->next_tree = new;

	new->level = PrevVisitedLink->level + 1;
    } else {
	if (Latest_tree)
	    Latest_tree->next_tree = new;
	new->level = 0;
	new->next_tree = NULL;
	Latest_tree = new;
    }
    PrevVisitedLink = PrevActiveVisitedLink = new;
    if (!First_tree)
	First_tree = new;

    /* "latest" chain */
    Latest_last.prev_latest->next_latest = new;
    new->prev_latest = Latest_last.prev_latest;
    new->next_latest = &Latest_last;
    Latest_last.prev_latest = new;

    return;
}

/*
 *  Returns true if this is a page that we would push onto the stack if not
 *  forced.  If docurl is NULL, only the title is considered; otherwise
 *  also check the URL whether it is (likely to be) a generated special
 *  page.
 */
PUBLIC BOOLEAN LYwouldPush ARGS2(
	CONST char *,	title,
	CONST char *,	docurl)
{
    BOOLEAN rc = FALSE;

    /*
     *  All non-pushable generated pages have URLs that begin with
     *  "file://localhost/" and end with HTML_SUFFIX. - kw
     */
    if (docurl) {
	size_t ulen;
	if (strncmp(docurl, "file://localhost/", 17) != 0 ||
	    (ulen = strlen(docurl)) <= strlen(HTML_SUFFIX) ||
	    strcmp(docurl + ulen - strlen(HTML_SUFFIX), HTML_SUFFIX) != 0) {
	    /*
	     *  If it is not a local HTML file, it may be a Web page that
	     *  accidentally has the same title.  So return TRUE now. - kw
	     */
	    return TRUE;
	}
    }

    if (docurl) {
	rc = (BOOLEAN)
		! (LYIsUIPage(docurl, UIP_HISTORY)
		|| LYIsUIPage(docurl, UIP_PRINT_OPTIONS)
#ifdef DIRED_SUPPORT
		|| LYIsUIPage(docurl, UIP_DIRED_MENU)
		|| LYIsUIPage(docurl, UIP_UPLOAD_OPTIONS)
		|| LYIsUIPage(docurl, UIP_PERMIT_OPTIONS)
#endif /* DIRED_SUPPORT */
	    );
    } else {
	rc = (BOOLEAN)
		! (!strcmp(title, HISTORY_PAGE_TITLE)
		|| !strcmp(title, PRINT_OPTIONS_TITLE)
#ifdef DIRED_SUPPORT
		|| !strcmp(title, DIRED_MENU_TITLE)
		|| !strcmp(title, UPLOAD_OPTIONS_TITLE)
		|| !strcmp(title, PERMIT_OPTIONS_TITLE)
#endif /* DIRED_SUPPORT */
	    );
    }
    return rc;
}

/*
 * Free post-data for 'DocInfo'
 */
PUBLIC void LYFreePostData ARGS1(
    DocInfo *,		doc)
{
    BStrFree(doc->post_data);
    FREE(doc->post_content_type);
}

/*
 * Free strings associated with a 'DocInfo' struct.
 */
PUBLIC void LYFreeDocInfo ARGS1(
    DocInfo *,		doc)
{
    FREE(doc->title);
    FREE(doc->address);
    FREE(doc->bookmark);
    LYFreePostData(doc);
}

/*
 *  Free the information in the last history entry.
 */
PRIVATE void clean_extra_history NOARGS
{
    trace_history("clean_extra_history");
    nhist += nhist_extra;
    while (nhist_extra > 0) {
	nhist--;
	LYFreeDocInfo(&HDOC(nhist));
	nhist_extra--;
    }
    trace_history("...clean_extra_history");
}

/*
 * Free the entire history stack, for auditing memory leaks.
 */
#ifdef LY_FIND_LEAKS
PRIVATE void clean_all_history NOARGS
{
    trace_history("clean_all_history");
    clean_extra_history();
    while (nhist > 0) {
	nhist--;
	LYFreeDocInfo(&HDOC(nhist));
    }
    trace_history("...clean_all_history");
}
#endif

/* FIXME What is the relationship to are_different() from the mainloop?! */
PRIVATE int are_identical ARGS2(
	HistInfo *,	doc,
	DocInfo *,	doc1)
{
     return (	STREQ(doc1->address, doc->hdoc.address)
		&& BINEQ(doc1->post_data, doc->hdoc.post_data)
		&& !strcmp(NonNull(doc1->bookmark),
			   NonNull(doc->hdoc.bookmark))
		&& doc1->isHEAD == doc->hdoc.isHEAD );
}

/*
 *  Push the current filename, link and line number onto the history list.
 */
PUBLIC int LYpush ARGS2(
	DocInfo *,	doc,
	BOOLEAN,	force_push)
{
    /*
     *	Don't push NULL file names.
     */
    if (*doc->address == '\0')
	return 0;

    /*
     *	Check whether this is a document we
     *	don't push unless forced. - FM
     */
    if (!force_push) {
	/*
	 *  Don't push the history, printer, or download lists.
	 */
	if (!LYwouldPush(doc->title, doc->address)) {
	    if (!LYforce_no_cache)
		LYoverride_no_cache = TRUE;
	    return 0;
	}
    }

    /*
     *	If file is identical to one before it, don't push it.
     */
    if ( nhist > 1 && are_identical(&(history[nhist-1]), doc)) {
	if (HDOC(nhist-1).internal_link == doc->internal_link) {
	    /* But it is nice to have the last position remembered!
	       - kw */
	    HDOC(nhist-1).link = doc->link;
	    HDOC(nhist-1).line = doc->line;
	    return 0;
	}
    }

    /*
     *	If file is identical to the current document, just move the pointer.
     */
    if ( nhist_extra >= 1 && are_identical(&(history[nhist]), doc)) {
	HDOC(nhist).link = doc->link;
	HDOC(nhist).line = doc->line;
	nhist_extra--;
	nhist++;
	trace_history("LYpush: just move the cursor");
	return 1;
    }

    clean_extra_history();
#ifdef LY_FIND_LEAKS
    if (!already_registered_clean_all_history) {
	already_registered_clean_all_history = 1;
	atexit(clean_all_history);
    }
#endif

    /*
     *	OK, push it if we have stack space.
     */
    if (nhist < MAXHIST)  {
	HDOC(nhist).link = doc->link;
	HDOC(nhist).line = doc->line;

	HDOC(nhist).title = NULL;
	LYformTitle(&(HDOC(nhist).title), doc->title);

	HDOC(nhist).address = NULL;
	StrAllocCopy(HDOC(nhist).address, doc->address);

	HDOC(nhist).post_data = NULL;
	BStrCopy(HDOC(nhist).post_data, doc->post_data);

	HDOC(nhist).post_content_type = NULL;
	StrAllocCopy(HDOC(nhist).post_content_type, doc->post_content_type);

	HDOC(nhist).bookmark = NULL;
	StrAllocCopy(HDOC(nhist).bookmark, doc->bookmark);

	HDOC(nhist).isHEAD = doc->isHEAD;
	HDOC(nhist).safe = doc->safe;

	HDOC(nhist).internal_link = FALSE; /* by default */
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
		    HTAnchor_findAddress(&WWWDoc);
		/* Now find the ParentAnchor for the previous history
		** item - kw
		*/
		if (thisparent) {
		    /* If the last-pushed item is a LYNXIMGMAP but THIS one
		    ** isn't, compare the physical URLs instead. - kw
		    */
		    if (isLYNXIMGMAP(HDOC(nhist-1).address) &&
			!isLYNXIMGMAP(doc->address)) {
			WWWDoc.address = HDOC(nhist-1).address + LEN_LYNXIMGMAP;
		    /*
		    ** If THIS item is a LYNXIMGMAP but the last-pushed one
		    ** isn't, fake it by using THIS item's address for
		    ** thatparent... - kw
		    */
		    } else if (isLYNXIMGMAP(doc->address) &&
		       !isLYNXIMGMAP(HDOC(nhist-1).address)) {
			char *temp = NULL;
			StrAllocCopy(temp, STR_LYNXIMGMAP);
			StrAllocCat(temp, doc->address + LEN_LYNXIMGMAP);
			WWWDoc.address = temp;
			WWWDoc.post_content_type = HDOC(nhist-1).post_content_type;
			WWWDoc.bookmark = HDOC(nhist-1).bookmark;
			WWWDoc.isHEAD = HDOC(nhist-1).isHEAD;
			WWWDoc.safe = HDOC(nhist-1).safe;
			thatparent =
			    HTAnchor_findAddress(&WWWDoc);
			FREE(temp);
		    } else {
			WWWDoc.address = HDOC(nhist-1).address;
		    }
		    if (!thatparent) { /* if not yet done */
			WWWDoc.post_data = HDOC(nhist-1).post_data;
			WWWDoc.post_content_type = HDOC(nhist-1).post_content_type;
			WWWDoc.bookmark = HDOC(nhist-1).bookmark;
			WWWDoc.isHEAD = HDOC(nhist-1).isHEAD;
			WWWDoc.safe = HDOC(nhist-1).safe;
			thatparent =
			    HTAnchor_findAddress(&WWWDoc);
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
			HDOC(nhist).internal_link = TRUE;
			history[nhist].intern_seq_start =
			    history[nhist-1].intern_seq_start >= 0 ?
			    history[nhist-1].intern_seq_start : nhist-1;
			CTRACE((tfp, "\nLYpush: pushed as internal link, OK\n"));
		    }
		}
	    }
	    if (!HDOC(nhist).internal_link) {
		CTRACE((tfp, "\nLYpush: push as internal link requested, %s\n",
			    "but didn't check out!"));
	    }
	}
	CTRACE((tfp, "\nLYpush[%d]: address:%s\n        title:%s\n",
		    nhist, doc->address, doc->title));
	nhist++;
    } else {
	if (LYCursesON) {
	    HTAlert(MAXHIST_REACHED);
	}
	CTRACE((tfp, "\nLYpush: MAXHIST reached for:\n        address:%s\n        title:%s\n",
		    doc->address, doc->title));
    }
    return 1;
}

/*
 *  Pop the previous filename, link and line number from the history list.
 */
PUBLIC void LYpop ARGS1(
	DocInfo *,	doc)
{
    if (nhist > 0) {
	clean_extra_history();
	nhist--;

	LYFreeDocInfo(doc);

	*doc = HDOC(nhist);

#ifdef DISP_PARTIAL
	/* assume we pop the 'doc' to show it soon... */
	LYSetNewline(doc->line);	/* reinitialize */
#endif /* DISP_PARTIAL */
	CTRACE((tfp, "LYpop[%d]: address:%s\n     title:%s\n",
		    nhist, doc->address, doc->title));
    }
}

/*
 *  Move to the previous filename, link and line number from the history list.
 */
PUBLIC void LYhist_prev ARGS1(
	DocInfo *,	doc)
{
    trace_history("LYhist_prev");
    if (nhist > 0 && (nhist_extra || nhist < MAXHIST)) {
	nhist--;
	nhist_extra++;
	LYpop_num(nhist, doc);
	trace_history("...LYhist_prev");
    }
}

/*
 *  Called before calling LYhist_prev().
 */
PUBLIC void LYhist_prev_register ARGS1(
	DocInfo *,	doc)
{
    trace_history("LYhist_prev_register");
    if (nhist > 1) {
	if (nhist_extra) {	/* Make something to return back */
	    /* Store the new position */
	    HDOC(nhist).link = doc->link;
	    HDOC(nhist).line = doc->line;
	} else if (nhist < MAXHIST) { /* push will fail */
	    if (LYpush(doc, 0)) {
		nhist--;
		nhist_extra++;
	    }
	}
	trace_history("...LYhist_prev_register");
    }
}

/*
 *  Move to the next filename, link and line number from the history.
 */
PUBLIC int LYhist_next ARGS2(
	DocInfo *,	doc,
	DocInfo *,	newdoc)
{
    if (nhist_extra <= 1)	/* == 1 when we are the last one */
	return 0;
    /* Store the new position */
    HDOC(nhist).link = doc->link;
    HDOC(nhist).line = doc->line;
    nhist++;
    nhist_extra--;
    LYpop_num(nhist, newdoc);
    return 1;
}

/*
 *  Pop the specified hist entry, link and line number from the history
 *  list but don't actually remove the entry, just return it.
 *  (This procedure is badly named :)
 */
PUBLIC void LYpop_num ARGS2(
	int,		number,
	DocInfo *,	doc)
{
    if (number >= 0 && nhist + nhist_extra > number) {
	doc->link = HDOC(number).link;
	doc->line = HDOC(number).line;
	StrAllocCopy(doc->title, HDOC(number).title);
	StrAllocCopy(doc->address, HDOC(number).address);
	BStrCopy(doc->post_data, HDOC(number).post_data);
	StrAllocCopy(doc->post_content_type, HDOC(number).post_content_type);
	StrAllocCopy(doc->bookmark, HDOC(number).bookmark);
	doc->isHEAD = HDOC(number).isHEAD;
	doc->safe = HDOC(number).safe;
	doc->internal_link = HDOC(number).internal_link; /* ?? */
#ifdef DISP_PARTIAL
	/* assume we pop the 'doc' to show it soon... */
	LYSetNewline(doc->line);	/* reinitialize */
#endif /* DISP_PARTIAL */
    }
}

/*
 *  This procedure outputs the history buffer into a temporary file.
 */
PUBLIC int showhistory ARGS1(
	char **,	newfile)
{
    static char tempfile[LY_MAXPATH] = "\0";
    char *Title = NULL;
    int x = 0;
    FILE *fp0;

    if ((fp0 = InternalPageFP(tempfile, TRUE)) == 0)
	return(-1);

    LYLocalFileToURL(newfile, tempfile);

    LYforce_HTML_mode = TRUE;	/* force this file to be HTML */
    LYforce_no_cache = TRUE;	/* force this file to be new */

    BeginInternalPage(fp0, HISTORY_PAGE_TITLE, HISTORY_PAGE_HELP);

    fprintf(fp0, "<p align=right> <a href=\"%s\">[%s]</a>\n",
		 STR_LYNXMESSAGES, STATUSLINES_TITLE);

    fprintf(fp0, "<pre>\n");

    fprintf(fp0, "<em>%s</em>\n", gettext("You selected:"));
    for (x = nhist + nhist_extra - 1; x >= 0; x--) {
	/*
	 *  The number of the document in the hist stack,
	 *  its title in a link, and its address. - FM
	 */
	if (HDOC(x).title != NULL) {
	    StrAllocCopy(Title, HDOC(x).title);
	    LYEntify(&Title, TRUE);
	    LYTrimLeading(Title);
	    LYTrimTrailing(Title);
	    if (*Title == '\0')
		StrAllocCopy(Title , NO_TITLE);
	} else {
	    StrAllocCopy(Title, NO_TITLE);
	}
	fprintf(fp0,
		"%s<em>%d</em>. <tab id=t%d><a href=\"%s%d\">%s</a>\n",
		(x > 99 ? "" : x < 10 ? "  " : " "),
		x, x, STR_LYNXHIST, x, Title);
	if (HDOC(x).address != NULL) {
	    StrAllocCopy(Title, HDOC(x).address);
	    LYEntify(&Title, TRUE);
	} else {
	    StrAllocCopy(Title, gettext("(no address)"));
	}
	if (HDOC(x).internal_link) {
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
	DocInfo *,	newdoc)
{
    int number;
    DocAddress WWWDoc;
    HTParentAnchor *tmpanchor;
    HText *text;
    BOOLEAN treat_as_intern = FALSE;

    if ((!newdoc || !newdoc->address) ||
	strlen(newdoc->address) < 10 || !isdigit(UCH(*(newdoc->address+9))))
	return(FALSE);

    if ((number = atoi(newdoc->address+9)) > nhist + nhist_extra || number < 0)
	return(FALSE);

    /*
     * Optimization: assume we came from the History Page,
     * so never return back - always a new version next time.
     * But check first whether HTMainText is really the History
     * Page document - in some obscure situations this may not be
     * the case.  If HTMainText seems to be a History Page document,
     * also check that it really hasn't been pushed. - LP, kw
     */
    if (HTMainText && nhist > 0 &&
	!strcmp(HTLoadedDocumentTitle(), HISTORY_PAGE_TITLE) &&
	LYIsUIPage3(HTLoadedDocumentURL(), UIP_HISTORY, 0) &&
	strcmp(HTLoadedDocumentURL(), HDOC(nhist-1).address)) {
	HTuncache_current_document();  /* don't waste the cache */
    }

    LYpop_num(number, newdoc);
    if (((newdoc->internal_link &&
	  history[number].intern_seq_start == history[nhist-1].intern_seq_start) ||
	 (number < nhist-1 &&
	  HDOC(nhist-1).internal_link &&
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
	tmpanchor = HTAnchor_findAddress(&WWWDoc);
	text = (HText *)HTAnchor_document(tmpanchor);
	if (((((LYresubmit_posts == TRUE) ||
	       (LYforce_no_cache == TRUE &&
		LYoverride_no_cache == FALSE)) &&
	      !(treat_as_intern && !reloading)) ||
	     text == NULL) &&
	    (isLYNXIMGMAP(newdoc->address) ||
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
 *  This procedure outputs the Visited Links list into a temporary file. - FM
 *  Returns links's number to make active (1-based), or 0 if not required.
 */
PUBLIC int LYShowVisitedLinks ARGS1(
	char **,	newfile)
{
    static char tempfile[LY_MAXPATH] = "\0";
    char *Title = NULL;
    char *Address = NULL;
    int x, tot;
    FILE *fp0;
    VisitedLink *vl;
    HTList *cur = Visited_Links;
    int offset;
    int ret = 0;
    char *arrow, *post_arrow;

    if (!cur)
	return(-1);

    if ((fp0 = InternalPageFP(tempfile, TRUE)) == 0)
	return(-1);

    LYLocalFileToURL(newfile, tempfile);
    LYRegisterUIPage(*newfile, UIP_VLINKS);

    LYforce_HTML_mode = TRUE;	/* force this file to be HTML */
    LYforce_no_cache = TRUE;	/* force this file to be new */

    BeginInternalPage(fp0, VISITED_LINKS_TITLE, VISITED_LINKS_HELP);

#ifndef NO_OPTION_FORMS
    fprintf(fp0, "<form action=\"%s\" method=\"post\">\n", STR_LYNXOPTIONS);
    LYMenuVisitedLinks (fp0, FALSE);
    fprintf(fp0, "<input type=\"submit\" value=\"Accept Changes\">\n");
    fprintf(fp0, "</form>\n");
    fprintf(fp0, "<P>\n");
#endif

    fprintf(fp0, "<pre>\n");
    fprintf(fp0, "<em>%s</em>\n",
	    gettext("You visited (POSTs, bookmark, menu and list files excluded):"));
    if (Visited_Links_As & VISITED_LINKS_REVERSE)
	tot = x = HTList_count(Visited_Links);
    else
	tot = x = -1;

    if (Visited_Links_As & VISITED_LINKS_AS_TREE) {
	vl = First_tree;
    } else if (Visited_Links_As & VISITED_LINKS_AS_LATEST) {
	if (Visited_Links_As & VISITED_LINKS_REVERSE)
	    vl = Latest_last.prev_latest;
	else
	    vl = Latest_first.next_latest;
	if (vl == &Latest_last || vl == &Latest_first)
	    vl = NULL;
    } else {
	if (Visited_Links_As & VISITED_LINKS_REVERSE)
	    vl = Last_by_first;
	else
	    vl = (VisitedLink *)HTList_nextObject(cur);
    }
    while (NULL != vl) {
	/*
	 *  The number of the document (most recent highest),
	 *  its title in a link, and its address. - FM
	 */
	post_arrow = arrow = "";
	if (Visited_Links_As & VISITED_LINKS_REVERSE)
	    x--;
	else
	    x++;
	if (vl == PrevActiveVisitedLink) {
	    if (Visited_Links_As & VISITED_LINKS_REVERSE)
		ret = tot - x + 2;
	    else
		ret = x + 3;
	}
	if (vl == PrevActiveVisitedLink) {
	    post_arrow = "<A NAME=current></A>";
	    /* Otherwise levels 0 and 1 look the same when with arrow: */
	    arrow = (vl->level && (Visited_Links_As & VISITED_LINKS_AS_TREE))
			 ? "==>" : "=>";
	    StrAllocCat(*newfile, "#current");
	}
	if (Visited_Links_As & VISITED_LINKS_AS_TREE) {
	    offset = 2 * vl->level;
	    if (offset > 24)
		offset = (offset + 24)/2;
	    if (offset > LYcols * 3/4)
		offset = LYcols * 3/4;
	} else
	    offset = (x > 99 ? 0 : x < 10 ? 2 : 1);
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
		    "%-*s%s<em>%d</em>. <tab id=t%d><a href=\"%s\">%s</a>\n",
		    offset, arrow, post_arrow,
		    x, x, Address, Title);
	} else {
	    fprintf(fp0,
		    "%-*s%s<em>%d</em>. <tab id=t%d><em>%s</em>\n",
		    offset, arrow, post_arrow,
		    x, x, Title);
	}
	if (Address != NULL) {
	    StrAllocCopy(Address, vl->address);
	    LYEntify(&Address, TRUE);
	}
	fprintf(fp0, "<tab to=t%d>%s\n", x,
		     ((Address != NULL) ? Address : gettext("(no address)")));
	if (Visited_Links_As & VISITED_LINKS_AS_TREE)
	    vl = vl->next_tree;
	else if (Visited_Links_As & VISITED_LINKS_AS_LATEST) {
	    if (Visited_Links_As & VISITED_LINKS_REVERSE)
		vl = vl->prev_latest;
	    else
		vl = vl->next_latest;
	    if (vl == &Latest_last || vl == &Latest_first)
		vl = NULL;
	} else {
	    if (Visited_Links_As & VISITED_LINKS_REVERSE)
		vl = vl->prev_first;
	    else
		vl = (VisitedLink *)HTList_nextObject(cur);
	}
    }
    fprintf(fp0,"</pre>\n");
    EndInternalPage(fp0);

    LYCloseTempFP(fp0);
    FREE(Title);
    FREE(Address);
    return(ret);
}


/*
 *  Keep cycled buffer for statusline messages.
 */
#define STATUSBUFSIZE   40
PRIVATE char * buffstack[STATUSBUFSIZE];
PRIVATE int topOfStack = 0;

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
    if (topOfStack >= STATUSBUFSIZE) {
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
    if (topOfStack >= STATUSBUFSIZE) {
	topOfStack = 0;
    }
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
    while (++i < STATUSBUFSIZE) {
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
	HTSprintf0(&temp, message, NonNull(argument));
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

/*     LYLoadMESSAGES
**     --------------
**     Create a text/html stream with a list of recent statusline messages.
**     LYNXMESSAGES:/ internal page.
**     [implementation based on LYLoadKeymap()].
*/

struct _HTStream
{
    HTStreamClass * isa;
};

PRIVATE int LYLoadMESSAGES ARGS4 (
	CONST char *,		arg GCC_UNUSED,
	HTParentAnchor *,	anAnchor,
	HTFormat,		format_out,
	HTStream*,		sink)
{
    HTFormat format_in = WWW_HTML;
    HTStream *target = NULL;
    char *buf = NULL;
    int nummsg = 0;

    int i;
    char *temp = NULL;

    i = STATUSBUFSIZE;
    while (--i >= 0) {
	if (buffstack[i] != NULL)
	    nummsg++;
    }

    /*
     *  Set up the stream. - FM
     */
    target = HTStreamStack(format_in, format_out, sink, anAnchor);

    if (!target || target == NULL) {
	HTSprintf0(&buf, CANNOT_CONVERT_I_TO_O,
			 HTAtom_name(format_in), HTAtom_name(format_out));
	HTAlert(buf);
	FREE(buf);
	return(HT_NOT_LOADED);
    }
    anAnchor->no_cache = TRUE;

#define PUTS(buf)    (*target->isa->put_block)(target, buf, strlen(buf))

    HTSprintf0(&buf, "<html>\n<head>\n");
    PUTS(buf);
	/*
	 *  This page is a list of messages in display character set.
	 */
    HTSprintf0(&buf, "<META %s content=\"text/html;charset=%s\">\n",
	       "http-equiv=\"content-type\"",
	       LYCharSet_UC[current_char_set].MIMEname);
    PUTS(buf);
    HTSprintf0(&buf, "<title>%s</title>\n</head>\n<body>\n",
	       STATUSLINES_TITLE);
    PUTS(buf);

    if (nummsg != 0) {
	HTSprintf0(&buf, "<ol>\n");
	PUTS(buf);
	/* print messages in reverse order: */
	i = topOfStack;
	while (--i >= 0) {
	    if (buffstack[i] != NULL) {
		StrAllocCopy(temp, buffstack[i]);
		LYEntify(&temp, TRUE);
		HTSprintf0(&buf, "<li value=%d> <em>%s</em>\n", nummsg, temp);
		nummsg--;
		PUTS(buf);
	    }
	}
	i = STATUSBUFSIZE;
	while (--i >= topOfStack) {
	    if (buffstack[i] != NULL) {
		StrAllocCopy(temp, buffstack[i]);
		LYEntify(&temp, TRUE);
		HTSprintf0(&buf, "<li value=%d> <em>%s</em>\n", nummsg, temp);
		nummsg--;
		PUTS(buf);
	    }
	}
	FREE(temp);
	HTSprintf0(&buf, "</ol>\n</body>\n</html>\n");
    } else {
	HTSprintf0(&buf, "<p>%s\n</body>\n</html>\n",
		   gettext("(No messages yet)"));
    }
    PUTS(buf);

    (*target->isa->_free)(target);
    FREE(buf);
    return(HT_LOADED);
}

#ifdef GLOBALDEF_IS_MACRO
#define _LYMESSAGES_C_GLOBALDEF_1_INIT { "LYNXMESSAGES", LYLoadMESSAGES, 0}
GLOBALDEF (HTProtocol,LYLynxStatusMessages,_LYMESSAGES_C_GLOBALDEF_1_INIT);
#else
GLOBALDEF PUBLIC HTProtocol LYLynxStatusMessages = {"LYNXMESSAGES", LYLoadMESSAGES, 0};
#endif /* GLOBALDEF_IS_MACRO */
