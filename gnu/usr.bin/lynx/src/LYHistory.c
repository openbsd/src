#include "HTUtils.h"
#include "tcp.h"
#include "HTTP.h"
#include "HTAlert.h"
#include "HText.h"
#include "LYGlobalDefs.h"
#include "LYUtils.h"
#include "LYHistory.h"
#include "LYPrint.h"
#include "LYDownload.h"
#include "LYKeymap.h"
#include "LYList.h"
#include "LYShowInfo.h"
#include "LYSignal.h"
#include "LYStrings.h"
#include "LYCharUtils.h"

#ifdef DIRED_SUPPORT
#include "LYUpload.h"
#include "LYLocal.h"
#endif /* DIRED_SUPPORT */

#include "LYexit.h"
#include "LYLeaks.h"
 
#define FREE(x) if (x) {free(x); x = NULL;}

PUBLIC  HTList * Visited_Links = NULL;  /* List of safe popped docs. */

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
     *  Exclude POST or HEAD replies, and bookmark, menu
     *  or list files. - FM
     */
    if (doc->post_data || doc->isHEAD || doc->bookmark ||
	!strcmp((doc->title ? doc->title : ""), HISTORY_PAGE_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), PRINT_OPTIONS_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), DOWNLOAD_OPTIONS_TITLE) ||
#ifdef DIRED_SUPPORT
	!strcmp((doc->title ? doc->title : ""), DIRED_MENU_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), UPLOAD_OPTIONS_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), PERMIT_OPTIONS_TITLE) ||
#endif /* DIRED_SUPPORT */
	!strcmp((doc->title ? doc->title : ""), CURRENT_KEYMAP_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), LIST_PAGE_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), SHOWINFO_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), COOKIE_JAR_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), VISITED_LINKS_TITLE) ||
	!strcmp((doc->title ? doc->title : ""), LYNX_TRACELOG_TITLE)) {
	return;
    }

    if ((new = (VisitedLink *)calloc(1, sizeof(*new))) == NULL)
    	outofmem(__FILE__, "HTAddVisitedLink");
    StrAllocCopy(new->address, doc->address);
    StrAllocCopy(new->title, (doc->title ? doc->title : "(no title)"));

    if (!Visited_Links) {
        Visited_Links = HTList_new();
	atexit(Visited_Links_free);
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
 *  Push the current filename, link and line number onto the history list.
 */
PUBLIC void LYpush ARGS2(
	document *,	doc,
	BOOLEAN,	force_push)
{
    /*
     *  Don't push NULL file names.
     */
    if (*doc->address == '\0')
	return;

    /*
     *  Check whether this is a document we
     *  don't push unless forced. - FM
     */
    if (!force_push) {
	/*
	 *  Don't push the history, printer, or download lists.
	 */
	if (!strcmp(doc->title, HISTORY_PAGE_TITLE) ||
	    !strcmp(doc->title, PRINT_OPTIONS_TITLE) ||
	    !strcmp(doc->title, DOWNLOAD_OPTIONS_TITLE)) {
	    if (!LYforce_no_cache)
		LYoverride_no_cache = TRUE;
	    return;
	}

#ifdef DIRED_SUPPORT
	/*
	 *  Don't push DIRED menu, upload or permit lists.
	 */
	if (!strcmp(doc->title, DIRED_MENU_TITLE) ||
	    !strcmp(doc->title, UPLOAD_OPTIONS_TITLE) ||
	    !strcmp(doc->title, PERMIT_OPTIONS_TITLE)) {
	    if (!LYforce_no_cache)
		LYoverride_no_cache = TRUE;
	    return;
	}
#endif /* DIRED_SUPPORT */
    }

    /*
     *  If file is identical to one before it, don't push it.
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
	    history[nhist-1].page = doc->line;
 	    return;
	}
    }

#ifdef NOTDEFINED
/*
**  The following segment not used any more - What's it good for,
**  anyway??  Doing a pop when a push is requested is confusing,
**  also to the user.  Moreover, the way it was done seems to cause
**  a memory leak. - KW
*/  /*
     *  If file is identical to one two before it, don't push it.
     */
    if (nhist > 2 &&
        STREQ(history[nhist-2].address, doc->address) &&
        !strcmp(history[nhist-2].post_data ?
		history[nhist-2].post_data : "",
                doc->post_data ?
		doc->post_data : "") &&
        !strcmp(history[nhist-2].bookmark ?
		history[nhist-2].bookmark : "",
                doc->bookmark ?
		doc->bookmark : "") &&
	history[nhist-2].isHEAD == doc->isHEAD) {
	/*
	 *  Pop one off the stack.
	 */
	nhist--;
        return;
    }
#endif /* NOTDEFINED */

    /*
     *  OK, push it if we have stack space.
     */
    if (nhist < MAXHIST)  {
	history[nhist].link = doc->link;
	history[nhist].page = doc->line;
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
		** that IF we have a HTMainText (i.e. it wasn't just
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
			if (TRACE) {
			    fprintf(stderr,
				"\nLYpush: pushed as internal link, OK\n");
			}
		    }
		}
	    }
	    if (!history[nhist].internal_link) {
		if (TRACE) {
		    fprintf(stderr,
			    "\nLYpush: push as internal link requested, %s\n",
			    "but didn't check out!");
		}
	    }
	}
	nhist++;
   	if (TRACE) {
    	    fprintf(stderr,
		    "\nLYpush: address:%s\n        title:%s\n",
		    doc->address, doc->title);
	}
    } else {
        if (LYCursesON) {
	    _statusline(MAXHIST_REACHED);
	    sleep(AlertSecs);
	} 
        if (TRACE) {
    	    fprintf(stderr,
     "\nLYpush: MAXHIST reached for:\n        address:%s\n        title:%s\n",
		    doc->address, doc->title);
	}
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
	doc->line = history[nhist].page;
	FREE(doc->title);
	doc->title = history[nhist].title;	 /* will be freed later */
	FREE(doc->address);
	doc->address = history[nhist].address;   /* will be freed later */
	FREE(doc->post_data);
	doc->post_data = history[nhist].post_data;
	FREE(doc->post_content_type);
	doc->post_content_type = history[nhist].post_content_type;
	FREE(doc->bookmark);
	doc->bookmark = history[nhist].bookmark; /* will be freed later */
	doc->isHEAD = history[nhist].isHEAD;
	doc->safe = history[nhist].safe;
	doc->internal_link = history[nhist].internal_link;
        if (TRACE) {
	    fprintf(stderr,
	    	    "LYpop: address:%s\n     title:%s\n",
		    doc->address, doc->title);
	}
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
	doc->line = history[number].page;
	StrAllocCopy(doc->title, history[number].title);
	StrAllocCopy(doc->address, history[number].address);
	StrAllocCopy(doc->post_data, history[number].post_data);
	StrAllocCopy(doc->post_content_type, history[number].post_content_type);
	StrAllocCopy(doc->bookmark, history[number].bookmark);
	doc->isHEAD = history[number].isHEAD;
	doc->safe = history[number].safe;
	doc->internal_link = history[number].internal_link; /* ?? */
    }
}

/*
 *  This procedure outputs the history buffer into a temporary file.
 */
PUBLIC int showhistory ARGS1(
	char **,	newfile)
{
    static char tempfile[256];
    static BOOLEAN first = TRUE;
    static char hist_filename[256];
    char *Title = NULL;
    int x = 0;
    FILE *fp0;

    if (first) {
	tempname(tempfile, NEW_FILE);
	/*
	 *  Make the file a URL now.
	 */
#if defined (VMS) || defined (DOSPATH)
	sprintf(hist_filename,"file://localhost/%s", tempfile);
#else
	sprintf(hist_filename,"file://localhost%s", tempfile);
#endif /* VMS */
	first = FALSE;
#ifdef VMS
    } else {
	remove(tempfile);  /* Remove duplicates on VMS. */
#endif /* VMS */
    }

    if ((fp0 = LYNewTxtFile(tempfile)) == NULL) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(-1);
    }

    StrAllocCopy(*newfile, hist_filename);
    LYforce_HTML_mode = TRUE;	/* force this file to be HTML */
    LYforce_no_cache = TRUE;	/* force this file to be new */

    fprintf(fp0, "<head>\n");
    LYAddMETAcharsetToFD(fp0, -1);
    fprintf(fp0, "<title>%s</title>\n</head>\n<body>\n",
		 HISTORY_PAGE_TITLE);
    fprintf(fp0, "<h1>You have reached the History Page</h1>\n");
    fprintf(fp0, "<h2>%s Version %s</h2>\n<pre>", LYNX_NAME, LYNX_VERSION);
    fprintf(fp0, "<em>You selected:</em>\n");
    for (x = nhist-1; x >= 0; x--) {
	/*
	 *  The number of the document in the hist stack,
	 *  its title in a link, and its address. - FM
	 */
	if (history[x].title != NULL) {
	    StrAllocCopy(Title, history[x].title);
	    LYEntify(&Title, TRUE);
	} else {
	    StrAllocCopy(Title, "(no title)");
	}
	fprintf(fp0,
		"%s<em>%d</em>. <tab id=t%d><a href=\"LYNXHIST:%d\">%s</a>\n",
		(x > 99 ? "" : x < 10 ? "  " : " "),  
		x, x, x, Title);
	if (history[x].address != NULL) {
	    StrAllocCopy(Title, history[x].address);
	    LYEntify(&Title, TRUE);
	} else {
	    StrAllocCopy(Title, "(no address)");
	}
	if (history[x].internal_link) {
	    if (history[x].intern_seq_start == history[nhist-1].intern_seq_start)
		StrAllocCat(Title, " (internal)");
	    else
		StrAllocCat(Title, " (was internal)");
	}
	fprintf(fp0, "<tab to=t%d>%s\n", x, Title);
    }

    fprintf(fp0,"</pre>\n</body>\n");

    fclose(fp0);
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
     *  If we have POST content, and have LYresubmit_posts set
     *  or have no_cache set or do not still have the text cached,
     *  ask the user whether to resubmit the form. - FM
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
	    _statusline(CANCELLED);
	    sleep(InfoSecs);
	    return(FALSE);
	}
    }

    if (number != 0)
	StrAllocCat(newdoc->title," (From History)");
    return(TRUE);
}

/*
 *  This procedure outputs the Visited Links
 *  list into a temporary file. - FM
 */
PUBLIC int LYShowVisitedLinks ARGS1(
	char **,	newfile)
{
    static char tempfile[256];
    static BOOLEAN first = TRUE;
    static char vl_filename[256];
    char *Title = NULL;
    char *Address = NULL;
    int x;
    FILE *fp0;
    VisitedLink *vl;
    HTList *cur = Visited_Links;

    if (!cur)
        return(-1);

    if (first) {
	tempname(tempfile, NEW_FILE);
	/*
	 *  Make the file a URL now.
	 */
#if defined (VMS) || defined (DOSPATH)
	sprintf(vl_filename,"file://localhost/%s", tempfile);
#else
	sprintf(vl_filename,"file://localhost%s", tempfile);
#endif /* VMS */
	first = FALSE;
#ifdef VMS
    } else {
	remove(tempfile);  /* Remove duplicates on VMS. */
#endif /* VMS */
    }

    if ((fp0 = LYNewTxtFile(tempfile)) == NULL) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(-1);
    }

    StrAllocCopy(*newfile, vl_filename);
    LYforce_HTML_mode = TRUE;	/* force this file to be HTML */
    LYforce_no_cache = TRUE;	/* force this file to be new */

    fprintf(fp0, "<head>\n");
    LYAddMETAcharsetToFD(fp0, -1);
    fprintf(fp0, "<title>%s</title>\n</head>\n<body>\n",
		 VISITED_LINKS_TITLE);
    fprintf(fp0, "<h1>You have reached the Visited Links Page</h1>\n");
    fprintf(fp0, "<h2>%s Version %s</h2>\n<pre>", LYNX_NAME, LYNX_VERSION);
    fprintf(fp0, 
  "<em>You visited (POSTs, bookmark, menu and list files excluded):</em>\n");
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
	} else {
	    StrAllocCopy(Title , "(no title)");
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
		     ((Address != NULL) ? Address : "(no address)"));
    }

    fprintf(fp0,"</pre>\n</body>\n");

    fclose(fp0);
    FREE(Title);
    FREE(Address);
    return(0);
}
