#include <HTUtils.h>
#include <HTAccess.h>
#include <HTList.h>
#include <HTML.h>
#include <HTFTP.h>
#include <HTFile.h>
#include <HTTP.h>
#include <HTAABrow.h>
#include <HTNews.h>
#include <LYCurses.h>
#include <LYStyle.h>
#include <LYGlobalDefs.h>
#include <HTAlert.h>
#include <LYUtils.h>
#include <GridText.h>
#include <LYStrings.h>
#include <LYOptions.h>
#include <LYSignal.h>
#include <LYGetFile.h>
#include <HTForms.h>
#include <LYSearch.h>
#include <LYClean.h>
#include <LYHistory.h>
#include <LYPrint.h>
#include <LYMail.h>
#include <LYEdit.h>
#include <LYShowInfo.h>
#include <LYBookmark.h>
#include <LYKeymap.h>
#include <LYJump.h>
#include <LYDownload.h>
#include <LYList.h>
#include <LYMap.h>
#include <LYTraversal.h>
#include <LYCharSets.h>
#include <LYCharUtils.h>
#include <LYCookie.h>
#include <LYMainLoop.h>
#include <LYPrettySrc.h>

#ifdef USE_EXTERNALS
#include <LYExtern.h>
#endif

#ifdef __EMX__
#include <io.h>
#endif

#ifdef DIRED_SUPPORT
#include <LYLocal.h>
#include <LYUpload.h>
#endif /* DIRED_SUPPORT */

#include <LYexit.h>
#include <LYLeaks.h>


PRIVATE void exit_immediately_with_error_message PARAMS((int state, BOOLEAN first_file));
PRIVATE void print_status_message PARAMS((CONST linkstruct curlink, char **cp));
PRIVATE BOOL confirm_post_resub PARAMS((
    CONST char*		address,
    CONST char*		title,
    int			if_imgmap,
    int			if_file));
PRIVATE int are_different PARAMS((document *doc1, document *doc2));
PUBLIC void HTGotoURLs_free NOPARAMS;
PUBLIC void HTAddGotoURL PARAMS((char *url));

#ifndef DONT_TRACK_INTERNAL_LINKS
PRIVATE int are_phys_different PARAMS((document *doc1, document *doc2));
#endif

#define FASTTAB

PRIVATE int sametext ARGS2(
	char *,		een,
	char *,		twee)
{
    if (een && twee)
	return (strcmp(een, twee) == 0);
    return TRUE;
}

PUBLIC	HTList * Goto_URLs = NULL;  /* List of Goto URLs */

PUBLIC char * LYRequestTitle = NULL; /* newdoc.title in calls to getfile() */

#ifdef DISP_PARTIAL
PUBLIC int Newline_partial = 0;     /* required for display_partial mode */
PUBLIC int NumOfLines_partial = -1; /* initialize to -1 the very first time */
PUBLIC BOOLEAN display_partial = FALSE;
PUBLIC int Newline = 0;
#else
PRIVATE int Newline = 0;
#endif

PRIVATE document newdoc;
PRIVATE document curdoc;
PRIVATE char *traversal_host = NULL;
PRIVATE char *traversal_link_to_add = NULL;

#ifdef LY_FIND_LEAKS
/*
 *  Function for freeing allocated mainloop() variables. - FM
 */
PRIVATE void free_mainloop_variables NOARGS
{
    FREE(newdoc.title);
    FREE(newdoc.address);
    FREE(newdoc.post_data);
    FREE(newdoc.post_content_type);
    FREE(newdoc.bookmark);
    FREE(curdoc.title);
    FREE(curdoc.address);
    FREE(curdoc.post_data);
    FREE(curdoc.post_content_type);
    FREE(curdoc.bookmark);
#ifdef USE_HASH
    FREE(curdoc.style);
    FREE(newdoc.style);
#endif
    FREE(traversal_host);
    FREE(traversal_link_to_add);
#ifdef DIRED_SUPPORT
    clear_tags();
#endif /* DIRED_SUPPORT */

    return;
}
#endif /* LY_FIND_LEAKS */

PUBLIC FILE *TraceFP NOARGS
{
    if (LYTraceLogFP != 0) {
	return LYTraceLogFP;
    }
    return stderr;
}

PRIVATE void TracelogOpenFailed NOARGS
{
    WWW_TraceFlag = FALSE;
    if (LYCursesON) {
	HTUserMsg(TRACELOG_OPEN_FAILED);
    } else {
	fprintf(stderr, "%s\n", TRACELOG_OPEN_FAILED);
	exit(-1);
    }
}

PUBLIC BOOLEAN LYOpenTraceLog NOARGS
{
    if (TRACE && LYUseTraceLog && LYTraceLogFP == NULL) {
	/*
	 * If we can't open it for writing, give up.  Otherwise, on VMS close
	 * it, delete it and any versions from previous sessions so they don't
	 * accumulate, and open it again.  - FM
	 */
	if ((LYTraceLogFP = LYNewTxtFile(LYTraceLogPath)) == NULL) {
	    TracelogOpenFailed();
	    return FALSE;
	}
#ifdef VMS
	LYCloseTracelog();
	HTSYS_remove(LYTraceLogPath);
	if ((LYTraceLogFP = LYNewTxtFile(LYTraceLogPath)) == NULL) {
	    TracelogOpenFailed();
	    return FALSE;
	}
#endif /* VMS */
	fflush(stdout);
	fflush(stderr);
	fprintf(tfp, "\t\t%s (%s)\n\n", LYNX_TRACELOG_TITLE, LYNX_VERSION);
	/*
	 *  If TRACE is on, indicate whether the
	 *  anonymous restrictions are set. - FM
	 */
	if (LYRestricted) {
	    CTRACE(tfp, "Anonymous restrictions are set.\n");
	}
    }
    return TRUE;
}

PUBLIC void LYCloseTracelog NOARGS
{
    if (LYTraceLogFP != 0) {
	fflush(stdout);
	fflush(stderr);
	fclose(LYTraceLogFP);
	LYTraceLogFP = 0;
    }
}

PRIVATE BOOLEAN LYReopenTracelog ARGS1(BOOLEAN *, trace_flag_ptr)
{
    CTRACE(tfp, "\nTurning off TRACE for fetch of log.\n");
    LYCloseTracelog();
    if ((LYTraceLogFP = LYAppendToTxtFile(LYTraceLogPath)) == NULL) {
	TracelogOpenFailed();
	return FALSE;
    }
    if (TRACE) {
	WWW_TraceFlag = FALSE;
	*trace_flag_ptr = TRUE;
    }
    return TRUE;
}

PRIVATE void do_change_link ARGS1(
	char *,		prev_target)
{
    /* Is there a mouse-clicked link waiting? */
    int mouse_tmp = get_mouse_link();
    /* If yes, use it as the link */
    if (mouse_tmp != -1) {
	highlight(OFF, curdoc.link, prev_target);
	curdoc.link = mouse_tmp;
    }
}

/*
 *  Here's where we do all the work.
 *  mainloop is basically just a big switch dependent on the users input.
 *  I have tried to offload most of the work done here to procedures to
 *  make it more modular, but this procedure still does a lot of variable
 *  manipulation.  This needs some work to make it neater. - Lou Moutilli
 *					(memoir from the original Lynx - FM)
 */

int mainloop NOARGS
{
    int c = 0, real_c = 0, old_c = 0;
    int cmd = LYK_DO_NOTHING, real_cmd = LYK_DO_NOTHING;
    int getresult;
    int arrowup = FALSE, show_help = FALSE;
    char prev_target[512];
    char user_input_buffer[1024];
    char *owner_address = NULL;  /* Holds the responsible owner's address     */
    char *ownerS_address = NULL; /* Holds owner's address during source fetch */
    BOOLEAN first_file = TRUE;
    BOOLEAN popped_doc = FALSE;
    BOOLEAN refresh_screen = FALSE;
    BOOLEAN force_load = FALSE;
    BOOLEAN try_internal = FALSE;
    BOOLEAN crawl_ok = FALSE;
    BOOLEAN rlink_exists;
    BOOLEAN rlink_allowed;
    BOOLEAN vi_keys_flag = vi_keys;
    BOOLEAN emacs_keys_flag = emacs_keys;
    BOOLEAN trace_mode_flag = FALSE;
    BOOLEAN forced_HTML_mode = LYforce_HTML_mode;
    char cfile[128];
    FILE *cfp;
    char *cp, *toolbar;
    int ch, recall;
    int URLTotal;
    int URLNum;
    BOOLEAN FirstURLRecall = TRUE;
    char *temp = NULL;
    BOOLEAN ForcePush = FALSE;
    BOOLEAN override_LYresubmit_posts = FALSE;
    unsigned int len;
    int i;
    int n;

#ifdef DIRED_SUPPORT
    char *tp = NULL;
    struct stat dir_info;
#endif /* DIRED_SUPPORT */

/*
 *  curdoc.address contains the name of the file that is currently open.
 *  newdoc.address contains the name of the file that will soon be
 *		     opened if it exits.
 *  prev_target    contains the last search string the user searched for.
 *  newdoc.title   contains the link name that the user last chose to get
 *		     into the current link (file).
 */
    /* initialize some variables*/
    newdoc.address = NULL;
    newdoc.title = NULL;
    newdoc.post_data = NULL;
    newdoc.post_content_type = NULL;
    newdoc.bookmark = NULL;
    curdoc.address = NULL;
    curdoc.title = NULL;
    curdoc.post_data = NULL;
    curdoc.post_content_type = NULL;
    curdoc.bookmark = NULL;
#ifdef USE_HASH
    curdoc.style = NULL;
    newdoc.style = NULL;
#endif
    nhist = 0;
    user_input_buffer[(sizeof(user_input_buffer) - 1)] = '\0';
    *prev_target = '\0';
    *user_input_buffer = '\0';
#ifdef LY_FIND_LEAKS
    atexit(free_mainloop_variables);
#endif
initialize:
    StrAllocCopy(newdoc.address, startfile);
    StrAllocCopy(startrealm, startfile);
    StrAllocCopy(newdoc.title, gettext("Entry into main screen"));
    newdoc.isHEAD = FALSE;
    newdoc.safe = FALSE;
    newdoc.internal_link = FALSE;
    newdoc.line = 1;
    newdoc.link = 0;

#ifdef USE_SLANG
    if (TRACE && LYCursesON) {
	addstr("\n");
	refresh();
    }
#endif /* USE_SLANG */
    CTRACE(tfp,"Entering mainloop, startfile=%s\n",startfile);

    if (form_post_data) {
	StrAllocCopy(newdoc.post_data, form_post_data);
	StrAllocCopy(newdoc.post_content_type,
		     "application/x-www-form-urlencoded");
    } else if (form_get_data) {
	StrAllocCat(newdoc.address, form_get_data);
    }

    if (bookmark_start) {
	if (LYValidate) {
	    HTAlert(BOOKMARKS_DISABLED);
	    bookmark_start = FALSE;
	    goto initialize;
	} else if (traversal) {
	    HTAlert(BOOKMARKS_NOT_TRAVERSED);
	    traversal = FALSE;
	    crawl = FALSE;
	    bookmark_start = FALSE;
	    goto initialize;
	} else {
	    /*
	     *	See if a bookmark page exists.	If it does,
	     *	replace newdoc.address with it's name
	     */
	    if ((cp = get_bookmark_filename(&newdoc.address)) != NULL &&
		 *cp != '\0' && strcmp(cp, " ")) {
		StrAllocCopy(newdoc.title, BOOKMARK_TITLE);
		StrAllocCopy(newdoc.bookmark, BookmarkPage);
		StrAllocCopy(startrealm, newdoc.address);
		FREE(newdoc.post_data);
		FREE(newdoc.post_content_type);
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		CTRACE(tfp, "Using bookmarks=%s\n", newdoc.address);
	    } else {
		HTUserMsg(BOOKMARKS_NOT_OPEN);
		bookmark_start = FALSE;
		goto initialize;
	    }
	}
    }

    FREE(form_post_data);
    FREE(form_get_data);

    if (user_mode == NOVICE_MODE)
	display_lines = LYlines-4;
    else
	display_lines = LYlines-2;

    while (TRUE) {
#ifdef USE_HASH
	if (curdoc.style != NULL) force_load = TRUE;
#endif
	/*
	 *  If newdoc.address is different then curdoc.address then
	 *  we need to go out and find and load newdoc.address.
	 */
	if (LYforce_no_cache || force_load ||
	    are_different(&curdoc, &newdoc)) {

		force_load = FALSE;  /* done */
		if (TRACE && LYCursesON) {
		    move(LYlines-1, LYcols-1);	/* make sure cursor is down */
#ifdef USE_SLANG
		    addstr("\n");
#endif /* USE_SLANG */
		    refresh();
		}
try_again:
		/*
		 *  Push the old file onto the history stack if we
		 *  have a current doc and a new address. - FM
		 */
		if (curdoc.address && newdoc.address) {
		    /*
		     *	Don't actually push if this is a LYNXDOWNLOAD
		     *	URL, because that returns NORMAL even if it
		     *	fails due to a spoof attempt or file access
		     *	problem, and we set the newdoc structure
		     *	elements to the curdoc structure elements
		     *	under case NORMAL.  - FM
		     */
		    if (strncmp(newdoc.address, "LYNXDOWNLOAD:", 13))
		    {
			LYpush(&curdoc, ForcePush);
		    }
		} else if (!newdoc.address) {
		    /*
		     *	If newdoc.address is empty then pop a file
		     *	and load it.  - FM
		     */
		    LYpop(&newdoc);
		    popped_doc = TRUE;


#ifndef DONT_TRACK_INTERNAL_LINKS
#define NO_INTERNAL_OR_DIFFERENT(c,n) TRUE
#define NONINTERNAL_OR_PHYS_DIFFERENT(p,n) (!curdoc.internal_link || \
			   are_phys_different(p,n))
#else /* TRACK_INTERNAL_LINKS */
#define NO_INTERNAL_OR_DIFFERENT(c,n) are_different(c,n)
#define NONINTERNAL_OR_PHYS_DIFFERENT(p,n) are_different(p,n)
#endif /* TRACK_INTERNAL_LINKS */


#ifndef DONT_TRACK_INTERNAL_LINKS
		    /*
		    ** If curdoc had been reached via an internal
		    ** (fragment) link from what we now have just
		    ** popped into newdoc, then override non-caching in
		    ** all cases. - kw
		    */
		    if (curdoc.internal_link &&
			!are_phys_different(&curdoc, &newdoc)) {
			LYinternal_flag = TRUE;
			LYoverride_no_cache = TRUE;
			LYforce_no_cache = FALSE;
			try_internal = TRUE;
		    } else
#endif /* TRACK_INTERNAL_LINKS */
			/*
			 * Force a no_cache override unless
			 *  it's a bookmark file, or it has POST content
			 *  and LYresubmit_posts is set without safe also
			 *  set, and we are not going to another position
			 *  in the current document or restoring the previous
			 *  document due to a NOT_FOUND or NULLFILE return
			 *  value from getfile(). - FM
			 */
			if ((newdoc.bookmark != NULL) ||
			(newdoc.post_data != NULL &&
			 !newdoc.safe &&
			 LYresubmit_posts &&
			 !override_LYresubmit_posts &&
			    NO_INTERNAL_OR_DIFFERENT(&curdoc, &newdoc))) {
			LYoverride_no_cache = FALSE;
		    } else {
			LYoverride_no_cache = TRUE;
		    }
		}
		override_LYresubmit_posts = FALSE;

		if (HEAD_request) {
		    /*
		     *	Make SURE this is an appropriate request. - FM
		     */
		    if (newdoc.address) {
			if (LYCanDoHEAD(newdoc.address) == TRUE) {
			    newdoc.isHEAD = TRUE;
			} else if (!strncmp(newdoc.address, "LYNXIMGMAP:", 11)) {
			    if (LYCanDoHEAD(newdoc.address + 11) == TRUE) {
				StrAllocCopy(temp, newdoc.address + 11);
				FREE(newdoc.address);
				newdoc.address = temp;
				newdoc.isHEAD = TRUE;
				temp = NULL;
			    }
			}
		    }
		    try_internal = FALSE;
		    HEAD_request = FALSE;
		}

		/*
		 *  If we're getting the TRACE log and it's not new,
		 *  check whether its HText structure has been dumped,
		 *  and if so, fflush() and fclose() it to ensure it's
		 *  fully updated, and then fopen() it again. - FM
		 */
		if (LYUseTraceLog == TRUE &&
		    trace_mode_flag == FALSE &&
		    LYTraceLogFP != NULL &&
		    !strcmp((newdoc.title ? newdoc.title : ""),
			     LYNX_TRACELOG_TITLE)) {
		    DocAddress WWWDoc;
		    HTParentAnchor *tmpanchor;

		    WWWDoc.address = newdoc.address;
		    WWWDoc.post_data = newdoc.post_data;
		    WWWDoc.post_content_type = newdoc.post_content_type;
		    WWWDoc.bookmark = newdoc.bookmark;
		    WWWDoc.isHEAD = newdoc.isHEAD;
		    WWWDoc.safe = newdoc.safe;
		    tmpanchor = HTAnchor_parent(HTAnchor_findAddress(&WWWDoc));
		    if ((HText *)HTAnchor_document(tmpanchor) == NULL) {
			if (!LYReopenTracelog(&trace_mode_flag)) {
			    old_c = 0;
			    cmd = LYK_PREV_DOC;
			    goto new_cmd;
			}
		    }
		}

		LYRequestTitle = newdoc.title;
		if (newdoc.bookmark)
		    LYforce_HTML_mode = TRUE;
		if (LYValidate &&
		    startfile_ok &&
		    newdoc.address && startfile && homepage &&
		    (!strcmp(newdoc.address, startfile) ||
		     !strcmp(newdoc.address, homepage))) {
		    LYPermitURL = TRUE;
		}

		Newline = newdoc.line; /* bypass for partial mode */
#ifdef DISP_PARTIAL
		display_partial = display_partial_flag; /* restore */
		Newline_partial = Newline;  /* initialize */
		/*
		 * Disable display_partial if requested URL has #fragment
		 * and we are not popped from the history stack
		 * so can't calculate correct newline position for fragment.
		 * Otherwise user got the new document from the first page
		 * and be moved to #fragment later after download
		 * completed, but only if s/he did not mess screen up by
		 * scrolling before...  So fall down to old behavior here.
		 */
		if (Newline_partial == 1 && strchr(newdoc.address, '#'))
		    display_partial = FALSE;
#endif /* DISP_PARTIAL */
#ifdef USE_PSRC
		psrc_first_tag = TRUE;
#endif
#ifndef DONT_TRACK_INTERNAL_LINKS
		if (try_internal) {
		    if (newdoc.address &&
			0==strncmp(newdoc.address, "LYNXIMGMAP:", 11)) {
			try_internal = FALSE;
		    } else if (curdoc.address &&
			0==strncmp(curdoc.address, "LYNXIMGMAP:", 11)) {
			try_internal = FALSE;
		    }
		}
		if (try_internal) {
		    char *hashp = strchr(newdoc.address,'#');
		    if (hashp) {
			HTFindPoundSelector(hashp+1);
		    }
		    getresult = (HTMainText != NULL) ? NORMAL : NOT_FOUND;
		    try_internal = FALSE; /* done */
		    /* fix up newdoc.address which may have been fragment-only */
		    if (getresult == NORMAL && (!hashp || hashp == newdoc.address)) {
			if (!hashp) {
			    StrAllocCopy(newdoc.address, HTLoadedDocumentURL());
			} else {
			    StrAllocCopy(temp, HTLoadedDocumentURL());
			    StrAllocCat(temp, hashp); /* append fragment */
			    StrAllocCopy(newdoc.address, temp);
			    FREE(temp);
			}
		    }
		} else {
		    if (newdoc.internal_link && newdoc.address &&
			*newdoc.address == '#' && nhist > 0) {
			char *cp0;
			if (0==strncmp(history[nhist-1].address, "LYNXIMGMAP:", 11))
			    cp0 = history[nhist-1].address + 11;
			else
			    cp0 = history[nhist-1].address;
			StrAllocCopy(temp, cp0);
			cp0 = strchr(temp, '#');
			if (cp0)
			    *cp0 = '\0';
			StrAllocCat(temp, newdoc.address);
			FREE(newdoc.address);
			newdoc.address = temp;
			temp = NULL;
		    }
		    getresult = getfile(&newdoc);
		}
#else  /* TRACK_INTERNAL_LINKS */
		getresult = getfile(&newdoc);
#endif /* TRACK_INTERNAL_LINKS */

		switch(getresult) {

		case NOT_FOUND:
		    /*
		     *	OK! can't find the file, so it must not be around now.
		     *	Do any error logging, if appropriate.
		     */
		    LYoverride_no_cache = FALSE; /* Was TRUE if popped. - FM */
		    popped_doc = FALSE;		 /* Was TRUE if popped. - FM */
		    LYinternal_flag = FALSE;	 /* Reset to default. - kw */
		    if (trace_mode_flag == TRUE) {
			WWW_TraceFlag = TRUE;
			trace_mode_flag = FALSE;
			fprintf(tfp, "Turning TRACE back on.\n\n");
		    }
		    if (error_logging &&
			first_file && owner_address && !LYCancelledFetch) {
			/*
			 *  Email a bad link message to the owner of
			 *  the document (but NOT to lynx-dev). - FM
			 */
			if (strncasecomp(owner_address, "mailto:", 7)) {
			    if (strncasecomp((owner_address + 7),
					     "lynx-dev@", 9)) {
				mailmsg(curdoc.link,
					(owner_address+7),
					history[nhist-1].address,
					history[nhist-1].title);
			    }
			}
		    }
		    if (traversal && !first_file && !LYCancelledFetch) {
			FILE *ofp;

			if ((ofp = LYAppendToTxtFile(TRAVERSE_ERRORS)) == NULL) {
			    if ((ofp = LYNewTxtFile(TRAVERSE_ERRORS)) == NULL) {
				perror(NOOPEN_TRAV_ERR_FILE);
				exit_immediately(-1);
			    }
			}
			fprintf(ofp, "%s %s\tin %s\n",
				     links[curdoc.link].lname,
				     links[curdoc.link].target,
				     history[nhist-1].address);
			fclose(ofp);
		    }

		    /*
		     *	Fall through to do the NULL stuff and reload the
		     *	old file, unless the first file wasn't found or
		     *	has gone missing.
		     */
		    if (!nhist) {
			/*
			 *  If nhist = 0 then it must be the first file.
			 */
			exit_immediately_with_error_message(NOT_FOUND, first_file);
			return(-1);
		    }

		case NULLFILE:
		    /*
		     *	Not supposed to return any file.
		     */
		    LYoverride_no_cache = FALSE; /* Was TRUE if popped. - FM */
		    popped_doc = FALSE;		 /* Was TRUE if popped. - FM */
		    LYinternal_flag = FALSE;	 /* Reset to default. - kw */
		    if (trace_mode_flag == TRUE) {
			WWW_TraceFlag = TRUE;
			trace_mode_flag = FALSE;
			fprintf(tfp, "Turning TRACE back on.\n\n");
		    }
		    FREE(newdoc.address); /* to pop last doc */
		    FREE(newdoc.bookmark);
		    LYJumpFileURL = FALSE;
		    reloading = FALSE;
		    LYPermitURL = FALSE;
		    LYCancelledFetch = FALSE;
		    ForcePush = FALSE;
		    LYforce_HTML_mode = FALSE;
		    if (traversal) {
			crawl_ok = FALSE;
			if (traversal_link_to_add) {
			    /*
			     *	It's a binary file, or the fetch attempt
			     *	failed.  Add it to TRAVERSE_REJECT_FILE
			     *	so we don't try again in this run.
			     */
			    if (!lookup_reject(traversal_link_to_add)) {
				add_to_reject_list(traversal_link_to_add);
			    }
			    FREE(traversal_link_to_add);
			}
		    }
		    /*
		     *  Make sure the first file was found and
		     *  has not gone missing.
		     */
		    if (!nhist) {
			/*
			 *  If nhist = 0 then it must be the first file.
			 */
			if (first_file && homepage &&
#ifdef VMS
			   strcasecomp(homepage, startfile) != 0
#else
			   strcmp(homepage, startfile) != 0
#endif /* VMS */
			   ) {
			   /*
			    *  Couldn't return to the first file but there is a
			    *  homepage we can use instead. Useful for when the
			    *  first URL causes a program to be invoked. - GL
			    *
			    *  But first make sure homepage is different from
			    *  startfile (above), then make it the same (below)
			    *  so we don't enter an infinite getfile() loop on
			    *  on failures to find the files. - FM
			    */
			   StrAllocCopy(newdoc.address, homepage);
			   FREE(newdoc.post_data);
			   FREE(newdoc.post_content_type);
			   FREE(newdoc.bookmark);
			   StrAllocCopy(startfile, homepage);
			   newdoc.isHEAD = FALSE;
			   newdoc.safe = FALSE;
			   newdoc.internal_link = FALSE;
			   goto try_again;
			} else {
			   exit_immediately_with_error_message(NULLFILE, first_file);
			   return(-1);
			}
		    }

		   /*
		    *  Retrieval of a newdoc just failed, and just
		    *  going to try_again would pop the next doc
		    *  from history and try to get it without further
		    *  questions.  This may not be the right thing to do if
		    *  we have POST data, so fake a PREV_DOC key if it seems
		    *  that some prompting should be done.  Dunno about the
		    *  traversal logic, so I leave that case alone.
		    */
		   if (history[nhist - 1].post_data &&
		       !history[nhist - 1].safe) {
		       if (HText_POSTReplyLoaded((document *)&history[(nhist - 1)])) {
			   override_LYresubmit_posts = TRUE;
			   goto try_again;
		       }
		       /*  Set newdoc fields, just in case the PREV_DOC
			*  gets cancelled. - kw */
		       if (!curdoc.address) {
			   StrAllocCopy(newdoc.address, HTLoadedDocumentURL());
			   StrAllocCopy(newdoc.title, HTLoadedDocumentTitle());
			   if (HTMainAnchor && HTMainAnchor->post_data) {
			       StrAllocCopy(newdoc.post_data,
					    HTMainAnchor->post_data);
			       StrAllocCopy(newdoc.post_content_type,
					    HTMainAnchor->post_content_type);
			   } else {
			       FREE(newdoc.post_data);
			   }
			   newdoc.isHEAD = HTLoadedDocumentIsHEAD();
			   newdoc.safe = HTLoadedDocumentIsSafe();
			   newdoc.internal_link = FALSE;
		       } else {
			   StrAllocCopy(newdoc.address, curdoc.address);
			   StrAllocCopy(newdoc.title, curdoc.title);
			   StrAllocCopy(newdoc.post_data, curdoc.post_data);
			   StrAllocCopy(newdoc.post_content_type,
					curdoc.post_content_type);
			   newdoc.isHEAD = curdoc.isHEAD;
			   newdoc.safe = curdoc.safe;
			   newdoc.internal_link = curdoc.internal_link;
			   newdoc.line = curdoc.line;
			   newdoc.link = curdoc.link;
		       }
		       cmd = LYK_PREV_DOC;
		       goto new_cmd;
		    }
		    override_LYresubmit_posts = TRUE;
		    goto try_again;

		case NORMAL:
		    /*
		     *	Marvelously, we got the document!
		     */
		    LYoverride_no_cache = FALSE; /* Was TRUE if popped. - FM */
		    LYinternal_flag = FALSE;	 /* Reset to default. - kw */
		    if (trace_mode_flag == TRUE) {
			WWW_TraceFlag = TRUE;
			trace_mode_flag = FALSE;
			fprintf(tfp, "Turning TRACE back on.\n\n");
		    }
		    *prev_target = '\0';    /* Reset for this document. - FM */

		    /*
		     *	If it's the first file and we're interactive,
		     *	check whether it's a bookmark file which was
		     *	not accessed via the -book switch. - FM
		     */
		    if (((first_file == TRUE) &&
			 (dump_output_immediately == FALSE) &&
			 !(newdoc.bookmark && *newdoc.bookmark)) &&
			((LYisLocalFile(newdoc.address) == TRUE) &&
			 !(strcmp((HText_getTitle() ? HText_getTitle() : ""),
				  BOOKMARK_TITLE))) &&
			(temp = HTParse(newdoc.address, "",
				     PARSE_PATH+PARSE_PUNCTUATION)) != NULL) {
			cp = wwwName(Home_Dir());
			len = strlen(cp);
#ifdef VMS
			if (!strncasecomp(temp, cp, len) &&
			    strlen(temp) > len)
#else
			if (!strncmp(temp, cp, len) &&
			    strlen(temp) > len)
#endif /* VMS */
			{
			    /*
			     *	We're interactive and this might be a
			     *	bookmark file entered as a startfile
			     *	rather than invoked via -book.	Check
			     *	if it's in our bookmark file list, and
			     *	if so, reload if with the relevant
			     *	bookmark elements set. - FM
			     */
			    if ((cp = (char *)calloc(1,
				  (strlen((char *)&temp[len]) + 2))) == NULL)
				outofmem(__FILE__, "mainloop");
			    if (temp[len] == '/') {
				if (strchr((char *)&temp[(len + 1)], '/')) {
				    sprintf(cp, ".%s", (char *)&temp[len]);
				} else {
				    strcpy(cp, (char *)&temp[(len + 1)]);
				}
			    } else {
				strcpy(cp, (char *)&temp[len]);
			    }
			    for (i = 0; i <= MBM_V_MAXFILES; i++) {
				if (MBM_A_subbookmark[i] &&
#ifdef VMS
				    !strcasecomp(cp, MBM_A_subbookmark[i])
#else
				    !strcmp(cp, MBM_A_subbookmark[i])
#endif /* VMS */
				    ) {
				    StrAllocCopy(BookmarkPage,
						 MBM_A_subbookmark[i]);
				    break;
				}
			    }
			    FREE(cp);
			    if (i <= MBM_V_MAXFILES) {
				FREE(temp);
				if (LYValidate) {
				    HTAlert(BOOKMARKS_DISABLED);
				    return(-1);
				}
				if ((temp = HTParse(newdoc.address, "",
				 PARSE_ACCESS+PARSE_HOST+PARSE_PUNCTUATION))) {
				    StrAllocCopy(newdoc.address, temp);
				    HTuncache_current_document();
				    FREE(curdoc.address);
				    StrAllocCat(newdoc.address,
					    wwwName(Home_Dir()));
				    StrAllocCat(newdoc.address, "/");
				    StrAllocCat(newdoc.address,
					(strncmp(BookmarkPage, "./", 2) ?
							   BookmarkPage :
							(BookmarkPage + 2)));
				    StrAllocCopy(newdoc.title, BOOKMARK_TITLE);
				    StrAllocCopy(newdoc.bookmark, BookmarkPage);
#ifdef USE_HASH
				    if (curdoc.style)
					StrAllocCopy(newdoc.style, curdoc.style);
#endif
				    StrAllocCopy(startrealm, newdoc.address);
				    FREE(newdoc.post_data);
				    FREE(newdoc.post_content_type);
				    newdoc.isHEAD = FALSE;
				    newdoc.safe = FALSE;
				    FREE(temp);
				    if (!strcmp(homepage, startfile))
					StrAllocCopy(homepage, newdoc.address);
				    StrAllocCopy(startfile, newdoc.address);
				    CTRACE(tfp, "Reloading as bookmarks=%s\n",
						newdoc.address);
				    goto try_again;
				}
			    }
			}
			cp = NULL;
		    }
		    FREE(temp);

		    if (traversal) {
			/*
			 *  During traversal build up lists of all links
			 *  traversed.	Traversal mode is a special
			 *  feature for traversing http links in the web.
			 */
			if (traversal_link_to_add) {
			    /*
			     *	Add the address we sought to TRAVERSE_FILE.
			     */
			    if (!lookup(traversal_link_to_add))
				add_to_table(traversal_link_to_add);
			    FREE(traversal_link_to_add);
			}
			if (curdoc.address && curdoc.title &&
			    strncmp(curdoc.address, "LYNXIMGMAP:", 11))
			    /*
			     *	Add the address we got to TRAVERSE_FOUND_FILE.
			     */
			    add_to_traverse_list(curdoc.address, curdoc.title);
		    }

		    /*
		     *	If this was a LYNXDOWNLOAD, we still have curdoc,
		     *	not a newdoc, so reset the address, title and
		     *	positioning elements. - FM
		     */
		    if (newdoc.address && curdoc.address &&
			!strncmp(newdoc.address, "LYNXDOWNLOAD:", 13)) {
			StrAllocCopy(newdoc.address, curdoc.address);
			StrAllocCopy(newdoc.title, (curdoc.title ?
						    curdoc.title : ""));
			StrAllocCopy(newdoc.bookmark, curdoc.bookmark);
			newdoc.line = curdoc.line;
			newdoc.link = curdoc.link;
			newdoc.internal_link = FALSE; /* can't be true. - kw */
		    }

		    /*
		     *	Set Newline to the saved line.	It contains the
		     *	line the user was on if s/he has been in the file
		     *	before, or it is 1 if this is a new file.
		     */
#ifdef DISP_PARTIAL
		    /* Newline = newdoc.line; */
		    /* - alreary set and probably updated in partial mode */
		    /* incremental rendering stage already closed (but see below) */

		    if (Newline != Newline_partial || display_partial == TRUE) {
		       /* This is the case when we came from the history stack
			* _and_ cached HText was used instead of HT*Copy() call.
			* Set Newline and close partial mode here.
			*/
		       Newline = Newline_partial;
		       display_partial = FALSE;
		    }
#else
		    Newline = newdoc.line; /* now here, no partial mode */
#endif

		    /*
		     *	If we are going to a target line or
		     *	the first page of a popped document,
		     *	override any www_search line result.
		     */
		    if (Newline > 1 || popped_doc == TRUE)
			 www_search_result = -1;

		    /*
		     *	Make sure curdoc.line will not be equal
		     *	to Newline, so we get a redraw.
		     */
		    curdoc.line = -1;
#ifdef USE_PSRC
		    if (psrc_view)
			HTMark_asSource(); /* this flag is not set, since when
			displaying source, psrc_view is temporary unset when
			writing the HTML header - and HTMainText is created
			at that time.*/
#endif
		    break;
		}  /* end switch */

	    if (TRACE) {
		if (!LYTraceLogFP || trace_mode_flag) {
		    sleep(AlertSecs); /* allow me to look at the results */
		}
	    }

	    /*
	     *	Set the files the same.
	     */
	    StrAllocCopy(curdoc.address, newdoc.address);
	    StrAllocCopy(curdoc.post_data, newdoc.post_data);
	    StrAllocCopy(curdoc.post_content_type, newdoc.post_content_type);
	    StrAllocCopy(curdoc.bookmark, newdoc.bookmark);
#ifdef USE_HASH
	    StrAllocCopy(curdoc.style, HText_getStyle());
	    if (curdoc.style != NULL)
		style_readFromFile (curdoc.style);
#endif
	    curdoc.isHEAD = newdoc.isHEAD;
	    curdoc.internal_link = newdoc.internal_link;

	    /*
	     *	Set the remaining document elements and add to
	     *	the visited links list. - FM
	     */
	    if (ownerS_address != NULL) {
#ifndef USE_PSRC
		if (HTOutputFormat == WWW_SOURCE && !HText_getOwner())
#else
		if ( (LYpsrc ? psrc_view : HTOutputFormat == WWW_SOURCE)
			&& !HText_getOwner() )
#endif
		    HText_setMainTextOwner(ownerS_address);
		FREE(ownerS_address);
	    }
	    if (HText_getTitle()) {
		StrAllocCopy(curdoc.title, HText_getTitle());
	    } else if (!dump_output_immediately) {
		StrAllocCopy(curdoc.title, newdoc.title);
	    }
	    StrAllocCopy(owner_address, HText_getOwner());
	    curdoc.safe = HTLoadedDocumentIsSafe();
	    if (!dump_output_immediately) {
		LYAddVisitedLink(&curdoc);
	    }


	   /*
	    *  Reset WWW present mode so that if we were getting
	    *  the source, we get rendered HTML from now on.
	    */
	   HTOutputFormat = WWW_PRESENT;
#ifdef USE_PSRC
	   psrc_view = FALSE;
#endif

	   HTMLSetCharacterHandling(current_char_set);	/* restore, for sure? */

	   /*
	    *  Reset all of the other relevant flags. - FM
	    */
	   LYUserSpecifiedURL = FALSE;	/* only set for goto's and jumps's */
	   LYJumpFileURL = FALSE;	/* only set for jump's */
	   LYNoRefererForThis = FALSE;	/* always reset on return here */
	   reloading = FALSE;		/* only set for RELOAD and RESUBMIT */
	   HEAD_request = FALSE;	/* only set for HEAD requests */
	   LYPermitURL = FALSE;		/* only set for LYValidate */
	   ForcePush = FALSE;		/* only set for some PRINT requests. */
	   LYforce_HTML_mode = FALSE;
	   popped_doc = FALSE;

	} /* end if (LYforce_no_cache || force_load || are_different(...)) */

	if (dump_output_immediately) {
	    if (crawl) {
		print_crawl_to_fd(stdout, curdoc.address, curdoc.title);
	    } else {
		print_wwwfile_to_fd(stdout,0);
	    }
	    return(0);
	}

	/*
	 *  If the recent_sizechange variable is set to TRUE
	 *  then the window size changed recently.
	 */
	if (recent_sizechange) {
		stop_curses();
		start_curses();
		clear();
		refresh_screen = TRUE; /* to force a redraw */
		if (HTMainText)	/* to REALLY force it... - kw */
		    HText_setStale(HTMainText);
		recent_sizechange = FALSE;
		if (user_mode == NOVICE_MODE) {
		    display_lines = LYlines-4;
		} else {
		    display_lines = LYlines-2;
		}
	}

	if (www_search_result != -1) {
	     /*
	      *  This was a WWW search, set the line
	      *  to the result of the search.
	      */
	     Newline = www_search_result;
	     www_search_result = -1;  /* reset */
	}

	if (first_file == TRUE) {
	    /*
	     *	We can never again have the first file.
	     */
	    first_file = FALSE;

	    /*
	     *	Set the startrealm, and deal as best we can
	     *	with preserving forced HTML mode for a local
	     *	startfile. - FM
	     */
	    temp = HTParse(curdoc.address, "",
			   PARSE_ACCESS+PARSE_HOST+PARSE_PUNCTUATION);
	    if (!temp || *temp == '\0') {
		StrAllocCopy(startrealm, NO_NOTHING);
	    } else {
		StrAllocCopy(startrealm, temp);
		FREE(temp);
		if (!(temp = HTParse(curdoc.address, "",
				     PARSE_PATH+PARSE_PUNCTUATION))) {
		    LYAddHtmlSep(&startrealm);
		} else {
		    if (forced_HTML_mode &&
			!dump_output_immediately &&
			!curdoc.bookmark &&
			!strncasecomp(curdoc.address, "file:", 5) &&
			strlen(temp) > 1) {
			/*
			 *  We forced HTML for a local startfile which
			 *  is not a bookmark file and has a path of at
			 *  least two letters.	If it doesn't have a
			 *  suffix mapped to text/html, we'll set the
			 *  entire path (including the lead slash) as a
			 *  "suffix" mapped to text/html to ensure it is
			 *  always treated as an HTML source file.  We
			 *  are counting on a tail match to this full path
			 *  for some other URL fetched during the session
			 *  having too low a probability to worry about,
			 *  but it could happen. - FM
			 */
			HTAtom *encoding;

			if (HTFileFormat(temp, &encoding, NULL) != WWW_HTML) {
			    HTSetSuffix(temp, "text/html", "8bit", 1.0);
			}
		    }
		    if ((cp = strrchr(temp, '/')) != NULL) {
			*(cp+1) = '\0';
			StrAllocCat(startrealm, temp);
		    }
		}
	    }
	    FREE(temp);
	    CTRACE(tfp, "Starting realm is '%s'\n\n", startrealm);
	    if (traversal) {
		/*
		 *  Set up the crawl output stuff.
		 */
		if (curdoc.address && !lookup(curdoc.address)) {
		    if (strncmp(curdoc.address, "LYNXIMGMAP:", 11))
			crawl_ok = TRUE;
		    add_to_table(curdoc.address);
		}
		/*
		 *  Set up the traversal_host comparison string.
		 */
		if (strncmp((curdoc.address ? curdoc.address : "NULL"),
			    "http", 4)) {
		    StrAllocCopy(traversal_host, NO_NOTHING);
		} else if (check_realm) {
		    StrAllocCopy(traversal_host, startrealm);
		} else {
		    temp = HTParse(curdoc.address, "",
				   PARSE_ACCESS+PARSE_HOST+PARSE_PUNCTUATION);
		    if (!temp || *temp == '\0') {
			StrAllocCopy(traversal_host, NO_NOTHING);
		    } else {
			StrAllocCopy(traversal_host, temp);
			LYAddHtmlSep(&traversal_host);
		    }
		    FREE(temp);
		}
		CTRACE(tfp, "Traversal host is '%s'\n\n", traversal_host);
	    }
	    if (startfile) {
		/*
		 *  If homepage was not equated to startfile,
		 *  make the homepage URL the first goto
		 *  entry. - FM
		 */
		if (homepage && strcmp(startfile, homepage))
		    HTAddGotoURL(homepage);
		/*
		 *  If we are not starting up with startfile
		 *  (e.g., had -book), or if we are using the
		 *  startfile and it has no POST content, make
		 *  the startfile URL a goto entry. - FM
		 */
		if (strcmp(startfile, newdoc.address) ||
		    newdoc.post_data == NULL)
		    HTAddGotoURL(startfile);
	    }
	    if (TRACE) {
		refresh_screen = TRUE;
		if (!LYTraceLogFP || trace_mode_flag) {
		    sleep(AlertSecs);
		}
	    }
	}

#ifdef SOURCE_CACHE
	/*
	 * If the parse settings have changed since this HText was
	 * generated, we need to reparse and redraw it.  -dsb
	 *
	 * Should be configured to avoid shock for experienced lynx users.
	 * Currently enabled for cached sources only.
	 */
	if (HTdocument_settings_changed()) {
	   if (HTcan_reparse_document()) {
	       HTUserMsg(gettext("Reparsing document under current settings..."));
	       if (HTreparse_document()) {}
	   } else {
		/*
		 * Urk.  I have no idea how to recover from a failure here.
		 * At a guess, I'll try reloading.  -dsb
		 */
			/*  currently disabled ***
		HTUserMsg(gettext("Reparsing document under current settings..."));
		cmd = LYK_RELOAD;
		goto new_cmd;
			*/
	    }
	}

	/*
	 *  Trying to accomodate HTreparse_document() logic
	 *  with mainloop events.  Working out of force_load cycle
	 *  set all the necessary flags here, from case NORMAL
	 *  (see also LYK_SOURCE, some staff implemented directly there).
	 */
	if (from_source_cache) {
	    from_source_cache = FALSE; /* done */

	    /*
	     *	Make sure curdoc.line will not be equal
	     *	to Newline, so we get a redraw.
	     */
	    curdoc.line = -1;

	}
#endif


	/*
	 *  If the curdoc.line is different than Newline then there must
	 *  have been a change since last update.  Run HText_pageDisplay()
	 *  create a fresh screen of text out.
	 *  All display_partial calls ends here for final redraw.
	 */
	if (curdoc.line != Newline) {

	    refresh_screen = FALSE;

	    HText_pageDisplay(Newline, prev_target);

#ifdef DIRED_SUPPORT
	    if (lynx_edit_mode && nlinks > 0 && !HTList_isEmpty(tagged))
	      showtags(tagged);
#endif /* DIRED_SUPPORT */

	    /*
	     *  If more equals TRUE, then there is more info below this page.
	     */
	    more = HText_canScrollDown();
	    curdoc.line = Newline = HText_getTopOfScreen()+1;

	    if (curdoc.title == NULL) {
		/*
		 *  If we don't yet have a title, try to get it,
		 *  or set to that for newdoc.title. - FM
		 */
		if (HText_getTitle()) {
		    StrAllocCopy(curdoc.title, HText_getTitle());
		} else {
		    StrAllocCopy(curdoc.title, newdoc.title);
		}
	    }

	    if (arrowup) {
		/*
		 *  arrowup is set if we just came up from
		 *  a page below.
		 */
		curdoc.link = nlinks - 1;
		arrowup = FALSE;
	    } else {
		curdoc.link = newdoc.link;
		if (curdoc.link >= nlinks) {
		    curdoc.link = nlinks - 1;
		} else if (curdoc.link < 0 && nlinks > 0) {
		    /*
		     *	We may have popped a doc (possibly in local_dired)
		     *	which didn't have any links when it was pushed, but
		     *	does have links now (e.g., a file was created) - KW
		     */
		    curdoc.link = 0;
		}
	    }

	    show_help = FALSE; /* reset */
	    newdoc.line = 1;
	    newdoc.link = 0;
	    curdoc.line = Newline; /* set */
	}

	/*
	 *  Refresh the screen if necessary.
	 */
	if (refresh_screen) {
#if defined(FANCY_CURSES) || defined (USE_SLANG)
	    if (enable_scrollback) {
		clear();
	    } else {
		erase();
	    }
#else
	    clear();
#endif /* FANCY_CURSES || USE_SLANG */
	    HText_pageDisplay(Newline, prev_target);

#ifdef DIRED_SUPPORT
	    if (lynx_edit_mode && nlinks > 0 && !HTList_isEmpty(tagged))
		showtags(tagged);
#endif /* DIRED_SUPPORT */

	    /*
	     *  If more equals TRUE, then there is more info below this page.
	     */
	    more = HText_canScrollDown();

	    if (user_mode == NOVICE_MODE)
		noviceline(more);  /* print help message */
	    refresh_screen = FALSE;

	}

	/*
	 *  Report unread or new mail, if appropriate.
	 */
	if (check_mail && !no_mail)
	    LYCheckMail();

	/*
	 *  If help is not on the screen,
	 *  then put a message on the screen
	 *  to tell the user other misc info.
	 */
	if (!show_help) {
	   print_status_message(links[curdoc.link], &cp);
	} else {
	   show_help = FALSE;
	}

	if (!(nlinks > 0 &&
	      links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
	      (links[curdoc.link].form->type == F_TEXT_TYPE ||
	       links[curdoc.link].form->type == F_TEXTAREA_TYPE)))
	     /*
	      *  Highlight current link.
	      */
	    highlight(ON, curdoc.link, prev_target);

	if (traversal) {
	    /*
	     *	Don't go interactively into forms,
	     *	or accept keystrokes from the user
	     */
	    if (crawl && crawl_ok) {
		crawl_ok = FALSE;
#ifdef FNAMES_8_3
		sprintf(cfile,"lnk%05d.dat",ccount);
#else
		sprintf(cfile,"lnk%08d.dat",ccount);
#endif /* FNAMES_8_3 */
		ccount = ccount + 1;
		if ((cfp = LYNewTxtFile(cfile))  != NULL) {
		    print_crawl_to_fd(cfp,curdoc.address,curdoc.title);
		    fclose(cfp);
		} else {
		    if (!dump_output_immediately)
			cleanup();
#ifdef UNIX
		    if (dump_output_immediately)
			fprintf(stderr,
			gettext("Fatal error - could not open output file %s\n"),cfile);
		    else
#endif
			printf(
			gettext("Fatal error - could not open output file %s\n"),cfile);
		    if (!dump_output_immediately) {
			exit_immediately(-1);
		    }
		    return(-1);
		}
	    }
	} else {
	    /*
	     *	Normal, non-traversal handling.
	     */
	    if (nlinks > 0 &&
		links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
		(links[curdoc.link].form->type == F_TEXT_TYPE ||
		 links[curdoc.link].form->type == F_TEXT_SUBMIT_TYPE ||
		 links[curdoc.link].form->type == F_PASSWORD_TYPE ||
		 links[curdoc.link].form->type == F_TEXTAREA_TYPE)) {
		/*
		 *  Replace novice lines if in NOVICE_MODE.
		 */
		if (user_mode==NOVICE_MODE) {
		    move(LYlines-2,0); clrtoeol();
		    addstr(FORM_NOVICELINE_ONE);
		    move(LYlines-1,0); clrtoeol();
		    addstr(FORM_NOVICELINE_TWO);
		}
		c = change_form_link(&links[curdoc.link],
				     &newdoc, &refresh_screen,
				     links[curdoc.link].form->name,
				     links[curdoc.link].form->value);

		if (c == '\n' || c == '\r') {
#ifdef AUTOGROW
		    /*
		     *  If on the bottom line of a TEXTAREA, and the user hit
		     *  the ENTER key, we add a new line/anchor automatically,
		     *  positioning the cursor on it.
		     *
		     *  If at the bottom of the screen, we effectively perform
		     *  an LYK_DOWN_HALF-like operation, then move down to the
		     *  new line we just added.  --KED  02/14/99
		     *
		     *  [There is some redundancy and non-standard indentation
		     *   in the monster-if() below.  This is intentional ... to
		     *   try and improve the "readability" (such as it is).
		     *   Caveat emptor to anyone trying to change it.]
		     */
		    if ((links[curdoc.link].type       == WWW_FORM_LINK_TYPE &&
			 links[curdoc.link].form->type == F_TEXTAREA_TYPE)
			&&
			 ((curdoc.link == nlinks-1)
			 ||
			 ((curdoc.link <  nlinks-1) &&
			  !(links[curdoc.link+1].type == WWW_FORM_LINK_TYPE  &&
			    links[curdoc.link+1].form->type == F_TEXTAREA_TYPE))
			 ||
			 ((curdoc.link <  nlinks-1) &&
			  ((links[curdoc.link+1].type == WWW_FORM_LINK_TYPE  &&
			    links[curdoc.link+1].form->type == F_TEXTAREA_TYPE)
			    &&
			    ((links[curdoc.link].form->number	       !=
				      links[curdoc.link+1].form->number)     ||
			     (strcmp (links[curdoc.link].form->name,
				      links[curdoc.link+1].form->name) != 0)))))) {

			HText_ExpandTextarea (&links[curdoc.link], 1);

			if (links[curdoc.link].ly < display_lines) {
			    refresh_screen = TRUE;

			} else {

			    Newline += (display_lines/2);
			    if (nlinks > 0 && curdoc.link > -1 &&
				links[curdoc.link].ly > display_lines/2) {
				    newdoc.link = curdoc.link;
				    for (i = 0; links[i].ly <= (display_lines/2); i++)
					--newdoc.link;
				    newdoc.link++;
			    }
			}
		   }
#endif /* AUTOGROW */

#ifdef FASTTAB
		    /*
		     *	Make return act like down-arrow.
		     */
		    c = DNARROW;
#else
		    /*
		     *	Make return act like tab.
		     */
		    c = '\t';
#endif /* FASTTAB */
		}
	    } else {
		/*
		 *  Get a keystroke from the user.
		 *  Save the last keystroke to avoid
		 *  redundant error reporting.
		 */
		real_c = c = LYgetch(); /* get user input */

#ifndef VMS
		if (c == 3) {		/* ^C */
		    /*
		     *	This shouldn't happen.	We'll try to
		     *	deal with whatever bug caused it. - FM
		     */
		    signal(SIGINT, cleanup_sig);
		    old_c = 0;
		    cmd = LYK_QUIT;
		    goto new_cmd;
		}
#endif /* !VMS */
		if (old_c != real_c) {
		    old_c = 0;
		}
	    }
	}

#ifdef VMS
	if (HadVMSInterrupt) {
	    HadVMSInterrupt = FALSE;
	    c = DO_NOTHING;
	}
#else
	if (recent_sizechange) {
	    if (c <= 0)
		c = DO_NOTHING;
	}
#endif /* VMS */

new_keyboard_input:
	/*
	 *  A goto point for new input without going
	 *  back through the getch() loop.
	 */
	if (traversal) {
	    /*
	     *	This is a special feature to traverse every http link
	     *	derived from startfile and check for errors or create
	     *	crawl output files.  Only URL's that begin with
	     *	"traversal_host" are searched - this keeps the search
	     *	from crossing to other servers (a feature, not a bug!).
	     */
	    rlink_exists = (nlinks > 0 && links[curdoc.link].lname != NULL);
	    if (rlink_exists) {
		rlink_allowed =
		    (!lookup_reject(links[curdoc.link].lname) &&
		     traversal_host && links[curdoc.link].lname &&
		     !strncmp(traversal_host,
			      (strncmp(links[curdoc.link].lname,
				       "LYNXIMGMAP:", 11)
					 ?
		links[curdoc.link].lname : (links[curdoc.link].lname + 11)),
			      strlen(traversal_host)));
	    } else {
		rlink_allowed = FALSE;
	    }
	    if (rlink_exists && rlink_allowed) {
		if (lookup(links[curdoc.link].lname)) {
		    if (more_links ||
			(curdoc.link > -1 && curdoc.link < nlinks -1))
			 c= DNARROW;
		    else {
			if (STREQ(curdoc.title,"Entry into main screen") ||
			    (nhist <= 0 )) {
			    if (!dump_output_immediately) {
				cleanup();
				exit_immediately(-1);
			    }
			    return(-1);
			}
			c = LTARROW;
		    }
		} else {
		    StrAllocCopy(traversal_link_to_add,
				 links[curdoc.link].lname);
		    if (strncmp(traversal_link_to_add, "LYNXIMGMAP:", 11))
			crawl_ok = TRUE;
		    c = RTARROW;
		}
	    } else { /* no good right link, so only down and left arrow ok*/
		if (rlink_exists)
		    add_to_reject_list(links[curdoc.link].lname);
		if (more_links ||
		    (curdoc.link > -1 && curdoc.link < nlinks-1))
		    c = DNARROW;
		else {
		    /*
		     *	curdoc.title doesn't always work, so
		     *	bail out if the history list is empty.
		     */
		    if (STREQ(curdoc.title,"Entry into main screen") ||
			(nhist <= 0 )) {
			if (!dump_output_immediately) {
			    cleanup();
			    exit_immediately(-1);
			}
			return(-1);
		    }
		    c = LTARROW;
		}
	    } /* right link not NULL or link to another site*/
	} /* traversal */

	cmd = keymap[c+1];  /* add 1 to map EOF to 0 */

#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
	if (lynx_edit_mode && key_override[c+1] && !no_dired_support)
	  cmd = key_override[c+1];
#endif /* DIRED_SUPPORT && OK_OVERRIDE */

	real_cmd = cmd;

new_cmd:  /*
	   *  A goto point for new input without going
	   *  back through the getch() loop.
	   */

	CTRACE_FLUSH(tfp);

	switch(cmd) {
	case 0: /* unmapped character */
	default:
	    if (more)
		_statusline(MOREHELP);
	    else
		_statusline(HELP);
	    show_help = TRUE;

	    if (TRACE) {
		sprintf(cfile, "%d", c);
		addstr(cfile);	/* show the user input */
		cfile[0] = '\0';
	    }
	    break;

	case LYK_INTERRUPT:
	    /*
	     *	No network transmission to interrupt - 'til we multithread.
	     */
	    break;

	case LYK_F_LINK_NUM:
	     c = '\0';
	case LYK_1:
	case LYK_2:
	case LYK_3:
	case LYK_4:
	case LYK_5:
	case LYK_6:
	case LYK_7:
	case LYK_8:
	case LYK_9:
	{
	    /*
	     *	Get a number from the user and follow that link number.
	     */
	    int lindx = ((nlinks > 0) ? curdoc.link : 0);
	    int number;

	    /* pass cur line num for use in follow_link_number()
	     * Note: Current line may not equal links[cur].line
	     */
	    number = curdoc.line;
	    switch (follow_link_number(c, lindx, &newdoc, &number)) {
	    case DO_LINK_STUFF:
		/*
		 *  Follow a normal link.
		 */
		StrAllocCopy(newdoc.address, links[lindx].lname);
		StrAllocCopy(newdoc.title, links[lindx].hightext);
#ifndef DONT_TRACK_INTERNAL_LINKS
		/*
		 *  For internal links, retain POST content if present.
		 *  If we are on the List Page, prevent pushing it on
		 *  the history stack.	Otherwise set try_internal to
		 *  signal that the top of the loop should attempt to
		 *  reposition directly, without calling getfile. - kw
		 */
		if (links[lindx].type == WWW_INTERN_LINK_TYPE) {
		    LYinternal_flag = TRUE;
		    newdoc.internal_link = TRUE;
		    if (LYIsListpageTitle(curdoc.title ? curdoc.title : "") &&
			0==strcmp(HTLoadedDocumentURL(), LYlist_temp_url())) {
			if (!curdoc.post_data ||
			    /*
			     *	Normal case - List Page is not associated
			     *	with post data. - kw
			     */
			    (!LYresubmit_posts && curdoc.post_data &&
			     history[nhist - 1].post_data &&
			     !strcmp(curdoc.post_data,
				     history[nhist - 1].post_data) &&
			     HText_getContentBase() &&
			     !strncmp(HText_getContentBase(),
				      strncmp(history[nhist - 1].address,
					      "LYNXIMGMAP:", 11) ?
				      history[nhist - 1].address :
				      history[nhist - 1].address + 11,
				      strlen(HText_getContentBase())))) {
			    /*
			     *	Normal case - as best as we can check, the
			     *	document at the top of the history stack
			     *	seems to be the document the List Page is
			     *	about (or a LYNXIMGMAP derived from it),
			     *	and LYresubmit_posts is not set, so don't
			     *	prompt here.  If we actually have to repeat
			     *	a POST because, against expectations, the
			     *	underlying document isn't cached any more,
			     *	HTAccess will prompt for confirmation,
			     *	unless we had LYK_NOCACHE. - kw
			     */
			    LYinternal_flag = TRUE;
			} else {
			    HTLastConfirmCancelled(); /* reset flag */
			    if (!confirm_post_resub(newdoc.address,
						    newdoc.title,
						    (LYresubmit_posts &&
						     HText_POSTReplyLoaded(&newdoc)) ? 1 : 2,
						    2)) {
				if (HTLastConfirmCancelled() ||
				    (LYresubmit_posts &&
				     !HText_POSTReplyLoaded(&newdoc))) {
				    /* cancel the whole thing */
				    LYforce_no_cache = FALSE;
				    reloading = FALSE;
				    StrAllocCopy(newdoc.address, curdoc.address);
				    StrAllocCopy(newdoc.title, curdoc.title);
				    newdoc.internal_link = curdoc.internal_link;
				    HTInfoMsg(CANCELLED);
				    if (nlinks > 0)
					HText_pageDisplay(curdoc.line, prev_target);
				    break;
				} else if (LYresubmit_posts) {
				    /* If LYresubmit_posts is set, and the
				       answer was No, and we have a cached
				       copy, then use it. - kw */
				    LYforce_no_cache = FALSE;
				} else {
				    /* if No, but not ^C or ^G, drop
				     * the post data.  Maybe the link
				     * wasn't meant to be internal after
				     * all, here we can recover from that
				     * assumption. - kw */
				    FREE(newdoc.post_data);
				    FREE(newdoc.post_content_type);
				    newdoc.internal_link = FALSE;
				    HTAlert(DISCARDING_POST_DATA);
				}
			    }
			}
			/*
			 *  Don't push the List Page if we follow an
			 *  internal link given by it. - kw
			 */
			FREE(curdoc.address);
		    } else
			try_internal = TRUE;
		    if (!(LYresubmit_posts && newdoc.post_data))
			LYinternal_flag = TRUE;
		    force_load = TRUE;
		    break;
		} else {
		    /*
		     *	Free POST content if not an internal link. - kw
		     */
		    FREE(newdoc.post_data);
		    FREE(newdoc.post_content_type);
		}
#endif /* DONT_TRACK_INTERNAL_LINKS */
		/*
		 *  Might be an anchor in the same doc from a POST
		 *  form.  If so, don't free the content. -- FM
		 */
		if (are_different(&curdoc, &newdoc)) {
		    FREE(newdoc.post_data);
		    FREE(newdoc.post_content_type);
		    FREE(newdoc.bookmark);
		    newdoc.isHEAD = FALSE;
		    newdoc.safe = FALSE;
		}
		newdoc.internal_link = FALSE;
		force_load = TRUE;  /* force MainLoop to reload */
		break;

	    case DO_GOTOLINK_STUFF:
		/*
		 *  Position on a normal link, don't follow it. - KW
		 */
		Newline = newdoc.line;
		newdoc.line = 1;
		if (Newline == curdoc.line) {
		    /*
		     *	It's a link in the current page. - FM
		     */
		    if (nlinks > 0 && curdoc.link > -1) {
			if (curdoc.link == newdoc.link) {
			    /*
			     *	It's the current link, and presumably
			     *	reflects a typo in the statusline entry,
			     *	so issue a statusline message for the
			     *	typo-prone users (like me 8-). - FM
			     */
			    StrAllocCopy(temp, user_input_buffer);
			    sprintf(user_input_buffer,
				    LINK_ALREADY_CURRENT, number);
			    HTUserMsg(user_input_buffer);
			    strcpy(user_input_buffer, temp);
			    FREE(temp);
			} else {
			    /*
			     *	It's a different link on this page,
			     *	so turn the highlighting off, set the
			     *	current link to the new link value from
			     *	follow_link_number(), and re-initialize
			     *	the new link value. - FM
			     */
			    highlight(OFF, curdoc.link, prev_target);
			    curdoc.link = newdoc.link;
			    newdoc.link = 0;
			}
		    }
		}
		break;		/* nothing more to do */

	    case DO_GOTOPAGE_STUFF:
		/*
		 *  Position on a page in this document. - FM
		 */
		Newline = newdoc.line;
		newdoc.line = 1;
		if (Newline == curdoc.line) {
		    /*
		     *	It's the current page, so issue a
		     *	statusline message for the typo-prone
		     *	users (like me 8-). - FM
		     */
		    if (Newline <= 1) {
			HTInfoMsg(ALREADY_AT_BEGIN);
		    } else if (!more) {
			HTInfoMsg(ALREADY_AT_END);
		    } else {
			StrAllocCopy(temp, user_input_buffer);
			sprintf(user_input_buffer,
				ALREADY_AT_PAGE, number);
			HTUserMsg(user_input_buffer);
			strcpy(user_input_buffer, temp);
			FREE(temp);
		    }
		}
		break;

	    case PRINT_ERROR:
		old_c = real_c;
		HTUserMsg(BAD_LINK_NUM_ENTERED);
		break;
	    }
	    break;
	}

	case LYK_SOURCE:  /* toggle view source mode */
	    /*
	     *	Check if this is a reply from a POST, and if so,
	     *	seek confirmation if the safe element is not set. - FM
	     */
	    if ((curdoc.post_data != NULL &&
		 curdoc.safe != TRUE) &&
		confirm_post_resub(curdoc.address, curdoc.title,
				   1, 1) == FALSE) {
		HTInfoMsg(CANCELLED);
		break;
	    }

	    if (HTisDocumentSource()) {
		HTOutputFormat = WWW_PRESENT;
#ifdef USE_PSRC
		psrc_view = FALSE;
#endif
	    } else {
		if (HText_getOwner())
		    StrAllocCopy(ownerS_address, HText_getOwner());
		LYUCPushAssumed(HTMainAnchor);
#ifdef USE_PSRC
		if (LYpsrc)
		    psrc_view = TRUE;
		else
		    HTOutputFormat = WWW_SOURCE;
#else
		HTOutputFormat = WWW_SOURCE;
#endif
	    }

#ifdef SOURCE_CACHE
	    if (HTreparse_document()) {
			/*
			 * These normally get cleaned up after getfile() returns;
			 * since we're not calling getfile(), we have to clean them
			 * up ourselves.  -dsb
			 */
			HTOutputFormat = WWW_PRESENT;
#ifdef USE_PSRC
			if (psrc_view)
				HTMark_asSource();
			psrc_view = FALSE;
#endif
			FREE(ownerS_address);   /* not used with source_cache */
			LYUCPopAssumed();  		/* probably a right place here */
			HTMLSetCharacterHandling(current_char_set);  /* restore now */

			break;
	    }
#endif

	    FREE(curdoc.address); /* so it doesn't get pushed */
	    LYforce_no_cache = TRUE;
	    break;

	case LYK_RELOAD:  /* control-R to reload and refresh */
	    /*
	     *	Check if this is a reply from a POST, and if so,
	     *	seek confirmation if the safe element is not set. - FM
	     */
	    if ((curdoc.post_data != NULL &&
		 curdoc.safe != TRUE) &&
		HTConfirm(CONFIRM_POST_RESUBMISSION) == FALSE) {
		HTInfoMsg(CANCELLED);
		break;
	    }

	    /*
	     *	Check to see if should reload source, or load html
	     */

	    if (HTisDocumentSource()) {
		force_old_UCLYhndl_on_reload = TRUE;
		forced_UCLYhdnl = HTMainText_Get_UCLYhndl();
#ifndef USE_PSRC
		HTOutputFormat = WWW_SOURCE;
#else
		if (LYpsrc)
		    psrc_view = TRUE;
		else
		    HTOutputFormat = WWW_SOURCE;
#endif
	    }

	    HEAD_request = HTLoadedDocumentIsHEAD();
	    HTuncache_current_document();
#ifdef NO_ASSUME_SAME_DOC
	    /*
	     *	Don't assume the reloaded document will be the same. - FM
	     */
	    newdoc.line = 1;
	    newdoc.link = 0;
#else
	    /*
	     *	Do assume the reloaded document will be the same. - FM
	     *	(I don't remember all the reasons why we couldn't assume
	     *	 this.	As the problems show up, we'll try to fix them,
	     *	 or add warnings.  - FM)
	     */
	    newdoc.line = curdoc.line;
	    newdoc.link = curdoc.link;
#endif /* NO_ASSUME_SAME_DOC */
	    FREE(curdoc.address); /* so it doesn't get pushed */
#ifdef VMS
	    lynx_force_repaint();
#endif /* VMS */
	    /*
	     *	Reload should force a cache refresh on a proxy.
	     *	      -- Ari L. <luotonen@dxcern.cern.ch>
	     *
	     *	-- but only if this was really a reload requested by
	     *	the user, not if we jumped here to handle reloading for
	     *	INLINE_TOGGLE, IMAGE_TOGGLE, RAW_TOGGLE, etc. - KW
	     */
	    if (real_cmd == LYK_RELOAD)
		reloading = TRUE;
	    break;

	case LYK_HISTORICAL:	/* toggle 'historical' comments parsing */
#ifdef SOURCE_CACHE
	    if (!HTcan_reparse_document()) {
#endif
	    /*
	     *	Check if this is a reply from a POST, and if so,
	     *	seek confirmation of reload if the safe element
	     *	is not set. - FM
	     */
	    if ((curdoc.post_data != NULL &&
		 curdoc.safe != TRUE) &&
		confirm_post_resub(curdoc.address, NULL,
				   0, 0) == FALSE) {
		HTInfoMsg(WILL_NOT_RELOAD_DOC);
	    } else {
		HTuncache_current_document();
		StrAllocCopy(newdoc.address, curdoc.address);
		FREE(curdoc.address);
		newdoc.line = curdoc.line;
		newdoc.link = curdoc.link;
	    }
#ifdef SOURCE_CACHE
	    } /* end if no bypass */
#endif
	    if (historical_comments)
		historical_comments = FALSE;
	    else
		historical_comments = TRUE;
	    if (minimal_comments) {
		HTAlert(historical_comments ?
			HISTORICAL_ON_MINIMAL_OFF : HISTORICAL_OFF_MINIMAL_ON);
	    } else {
		HTAlert(historical_comments ?
			HISTORICAL_ON_VALID_OFF : HISTORICAL_OFF_VALID_ON);
	    }
#ifdef SOURCE_CACHE
	    if (HTreparse_document()) {
		break; /* OK */
	    }
#endif
	    break;

	case LYK_MINIMAL:	/* toggle 'minimal' comments parsing */
	    if (!historical_comments) {
#ifdef SOURCE_CACHE
	    if (!HTcan_reparse_document()) {
#endif
		/*
		 *  Check if this is a reply from a POST, and if so,
		 *  seek confirmation of reload if the safe element
		 *  is not set. - FM
		 */
		if ((curdoc.post_data != NULL &&
		     curdoc.safe != TRUE) &&
		    confirm_post_resub(curdoc.address, NULL,
				       0, 0) == FALSE) {
		    HTInfoMsg(WILL_NOT_RELOAD_DOC);
		} else {
		    HTuncache_current_document();
		    StrAllocCopy(newdoc.address, curdoc.address);
		    FREE(curdoc.address);
		    newdoc.line = curdoc.line;
		    newdoc.link = curdoc.link;
		}
	    }
#ifdef SOURCE_CACHE
	    } /* end if no bypass */
#endif
	    if (minimal_comments)
		minimal_comments = FALSE;
	    else
		minimal_comments = TRUE;
	    if (!historical_comments) {
		HTAlert(minimal_comments ?
			MINIMAL_ON_IN_EFFECT : MINIMAL_OFF_VALID_ON);
	    } else {
		HTAlert(minimal_comments ?
			MINIMAL_ON_BUT_HISTORICAL : MINIMAL_OFF_HISTORICAL_ON);
	    }
#ifdef SOURCE_CACHE
	    if (HTreparse_document()) {
		break; /* OK */
	    }
#endif
	    break;

	case LYK_SOFT_DQUOTES:
#ifdef SOURCE_CACHE
	    if (!HTcan_reparse_document()) {
#endif
	    /*
	     *	Check if this is a reply from a POST, and if so,
	     *	seek confirmation of reload if the safe element
	     *	is not set. - FM
	     */
	    if ((curdoc.post_data != NULL &&
		 curdoc.safe != TRUE) &&
		confirm_post_resub(curdoc.address, NULL,
				   1, 1) == FALSE) {
		HTInfoMsg(WILL_NOT_RELOAD_DOC);
	    } else {
		HTuncache_current_document();
		StrAllocCopy(newdoc.address, curdoc.address);
		FREE(curdoc.address);
		newdoc.line = curdoc.line;
		newdoc.link = curdoc.link;
	    }
#ifdef SOURCE_CACHE
	    } /* end if no bypass */
#endif
	    if (soft_dquotes)
		soft_dquotes = FALSE;
	    else
		soft_dquotes = TRUE;
	    HTUserMsg(soft_dquotes ?
		      SOFT_DOUBLE_QUOTE_ON : SOFT_DOUBLE_QUOTE_OFF);
#ifdef SOURCE_CACHE
	    if (HTreparse_document()) {
		break; /* OK */
	    }
#endif
	    break;

	case LYK_SWITCH_DTD:
#ifdef SOURCE_CACHE
	    if (!HTcan_reparse_document()) {
#endif
	    /*
	     *	Check if this is a reply from a POST, and if so,
	     *	seek confirmation of reload if the safe element
	     *	is not set. - FM, kw
	     */
	    if ((curdoc.post_data != NULL &&
		 curdoc.safe != TRUE) &&
		confirm_post_resub(curdoc.address, NULL,
				   1, 1) == FALSE) {
		HTInfoMsg(WILL_NOT_RELOAD_DOC);
	    } else {
		/*
		 *  If currently viewing preparsed source, switching
		 *  to the other DTD parsing may show source differences,
		 *  so stay in source view - kw
		 */

		/* NOTE: this conditional can be considered incorrect -
		   current behaviour - when viewing source and
		   LYPreparsedSource==TRUE, pressing ^V will toggle parser mode
		   AND switch back from the source view to presentation view.-HV
		*/
		if (HTisDocumentSource() && LYPreparsedSource) {
#ifdef USE_PSRC
		    if (LYpsrc)
			psrc_view = TRUE;
		    else
#endif
		    HTOutputFormat = WWW_SOURCE;
		}
		HTuncache_current_document();
		StrAllocCopy(newdoc.address, curdoc.address);
		FREE(curdoc.address);
#ifdef NO_ASSUME_SAME_DOC
		newdoc.line = 1;
		newdoc.link = 0;
#else
		newdoc.line = curdoc.line;
		newdoc.link = curdoc.link;
#endif /* NO_ASSUME_SAME_DOC */
	    }
#ifdef SOURCE_CACHE
	    } /* end if no bypass */
#endif
	    Old_DTD = !Old_DTD;
	    HTSwitchDTD(!Old_DTD);
	    HTUserMsg(Old_DTD ? USING_DTD_0 : USING_DTD_1);
#ifdef SOURCE_CACHE
	    if (HTcan_reparse_document()) {
	    if (HTisDocumentSource() && LYPreparsedSource) {
#ifdef USE_PSRC
		if (LYpsrc)
		    psrc_view = TRUE;
		else
#endif
		HTOutputFormat = WWW_SOURCE;
	    }
	    if (HTreparse_document()) {
		break;
	    }
	    } /* end if no bypass */
#endif
	    break;

#ifdef NOT_DONE_YET
	case LYK_PIPE:
	    /* ignore for now */
	    break;
#endif /* NOT_DONE_YET */

	case LYK_QUIT:	/* quit */
	    if (LYQuitDefaultYes == TRUE) {
		c = HTConfirmDefault(REALLY_QUIT_Y, YES);
	    } else {
		c = HTConfirmDefault(REALLY_QUIT_N, NO);
	    }
	    if (LYQuitDefaultYes == TRUE) {
		if (c != NO) {
		    return(0);
		} else {
		    HTInfoMsg(NO_CANCEL);
		}
	    } else if (c == YES) {
		return(0);
	    } else {
		HTInfoMsg(NO_CANCEL);
	    }
	    break;

	case LYK_ABORT:		/* don't ask the user about quitting */
	    return(0);

	case LYK_NEXT_PAGE:	/* next page */
	    if (more) {
		Newline += display_lines;
	    } else if (curdoc.link < nlinks-1) {
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link = nlinks-1;  /* put on last link */
	    } else if (old_c != real_c) {
		   old_c = real_c;
		   HTInfoMsg(ALREADY_AT_END);
	    }
	    break;

	case LYK_PREV_PAGE:  /* page up */
	    if (Newline > 1) {
		Newline -= display_lines;
	    } else if (curdoc.link > 0) {
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link = 0;  /* put on first link */
	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(ALREADY_AT_BEGIN);
	    }
	    break;

	case  LYK_UP_TWO:
	    if (Newline > 1) {
		int scrollamount = (Newline > 2 ? 2 : 1);
		Newline -= scrollamount;
		if (nlinks > 0 && curdoc.link > -1) {
		    if (links[curdoc.link].ly + scrollamount <= display_lines) {
			newdoc.link = curdoc.link +
				      HText_LinksInLines(HTMainText,
							 Newline, scrollamount);
		    } else {
			arrowup = TRUE;
		    }
		}
	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(ALREADY_AT_BEGIN);
	    }
	    break;

	case  LYK_DOWN_TWO:
	    if (more) {
		Newline += 2;
		if (nlinks > 0 && curdoc.link > -1 &&
		    links[curdoc.link].ly > 2) {
		    newdoc.link = curdoc.link;
		    for (i = 0; links[i].ly <= 2; i++)
			--newdoc.link;
		}
	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(ALREADY_AT_END);
	    }
	    break;

	case  LYK_UP_HALF:
	    if (Newline > 1) {
		int scrollamount = display_lines/2;
		if (Newline - scrollamount < 1)
		    scrollamount = Newline - 1;
		Newline -= scrollamount;
		if (nlinks > 0 && curdoc.link > -1) {
		    if (links[curdoc.link].ly + scrollamount <= display_lines) {
			newdoc.link = curdoc.link +
				      HText_LinksInLines(HTMainText,
							 Newline,
							 scrollamount);
		    } else {
			arrowup = TRUE;
		    }
		}
	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(ALREADY_AT_BEGIN);
	    }
	    break;

	case  LYK_DOWN_HALF:
	    if (more) {
		Newline += (display_lines/2);
		if (nlinks > 0 && curdoc.link > -1 &&
		    links[curdoc.link].ly > display_lines/2) {
		    newdoc.link = curdoc.link;
		    for (i = 0; links[i].ly <= (display_lines/2); i++)
			--newdoc.link;
		}
	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(ALREADY_AT_END);
	    }
	    break;

	case LYK_REFRESH:
	   refresh_screen = TRUE;
	   lynx_force_repaint();
	   break;

	case LYK_HOME:
	    if (curdoc.line > 1)
		Newline = 1;
	    else {
		cmd = LYK_PREV_PAGE;
		goto new_cmd;
	    }
	    break;

	case LYK_END:
	    if (more) {
	       Newline = HText_getNumOfLines() - display_lines + 3;  /* go to end of file */
	       arrowup = TRUE;	 /* position on last link */
	    } else {
		cmd = LYK_NEXT_PAGE;
		goto new_cmd;
	    }
	    break;

	case LYK_PREV_LINK:
	    if (curdoc.link > 0) {	     /* previous link */
		/*
		 *  Unhighlight current link.
		 */
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link--;

	    } else if (!more &&
		       curdoc.link==0 && Newline==1) { /* at the top of list */
		/*
		 *  If there is only one page of data and the user
		 *  goes off the top, then unhighlight the current
		 *  link and just move the cursor to last link on
		 *  the page.
		 */
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link = nlinks-1;  /* the last link */

	    } else if (curdoc.line > 1) {	/* previous page */
		/*
		 *  Go back to the previous page.
		 */
		int scrollamount = (Newline > display_lines ?
					      display_lines : Newline - 1);
		Newline -= scrollamount;
		if (scrollamount < display_lines &&
		    nlinks > 0 && curdoc.link == 0 &&
		    links[0].ly - 1 + scrollamount <= display_lines) {
			newdoc.link = HText_LinksInLines(HTMainText,
							 1,
							 scrollamount) - 1;
		} else {
		    arrowup = TRUE;
		}

	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(ALREADY_AT_BEGIN);
	    }
	    break;

	case LYK_NEXT_LINK:
	    if (curdoc.link < nlinks-1) {	/* next link */
		highlight(OFF, curdoc.link, prev_target);
#ifdef FASTTAB
		/*
		 *  Move to different textarea if TAB in textarea.
		 */
		if (links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
		    links[curdoc.link].form->type == F_TEXTAREA_TYPE &&
		    c=='\t') {
		    int thisgroup = links[curdoc.link].form->number;
		    char *thisname = links[curdoc.link].form->name;

		    do curdoc.link++;
		    while ((curdoc.link < nlinks-1) &&
			   links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
			   links[curdoc.link].form->type == F_TEXTAREA_TYPE &&
			   links[curdoc.link].form->number == thisgroup &&
			   sametext(links[curdoc.link].form->name, thisname));
		} else {
		    curdoc.link++;
		}
#else
		curdoc.link++;
#endif /* FASTTAB */
	    /*
	     *	At the bottom of list and there is only one page.
	     *	Move to the top link on the page.
	     */
	    } else if (!more && Newline == 1 && curdoc.link == nlinks-1) {
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link = 0;

	    } else if (more) {	/* next page */
		 Newline += (display_lines);

	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(ALREADY_AT_END);
	    }
	    break;

	case LYK_FASTFORW_LINK:
	{
	    int samepage = 0, nextlink = curdoc.link;
	    if (nlinks > 1) {

		/*
		 *  If in textarea, move to first link or field
		 *  after it if there is one on this screen. - kw
		 */
		if (links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
		    links[curdoc.link].form->type == F_TEXTAREA_TYPE) {
		    int thisgroup = links[curdoc.link].form->number;
		    char *thisname = links[curdoc.link].form->name;

		    if (curdoc.link < nlinks-1 &&
			!(links[nlinks-1].type == WWW_FORM_LINK_TYPE &&
			  links[nlinks-1].form->type == F_TEXTAREA_TYPE &&
			  links[nlinks-1].form->number == thisgroup &&
			  sametext(links[nlinks-1].form->name, thisname))) {
			do nextlink++;
			while
			    (links[nextlink].type == WWW_FORM_LINK_TYPE &&
			     links[nextlink].form->type == F_TEXTAREA_TYPE &&
			     links[nextlink].form->number == thisgroup &&
			     sametext(links[nextlink].form->name, thisname));
			samepage = 1;
		    } else if (!more && Newline == 1 && curdoc.link > 0) {
			nextlink = 0;
			samepage = 1;
		    }
		} else if (curdoc.link < nlinks-1) {
		    nextlink++;
		    samepage = 1;
		} else if (!more && Newline == 1 && curdoc.link > 0) {
		    nextlink = 0;
		    samepage = 1;
		}
	    }
	    if (samepage) {
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link = nextlink;
		break;		/* and we are done. */

	    /*
	     *	At the bottom of list and there is only one page.
	     *	Move to the top link on the page.
	     */
	    } else if (!more && Newline == 1 && curdoc.link == nlinks-1) {
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link = 0;

	    } else if (more &&	/* need a later page */
		       HTGetLinkOrFieldStart(curdoc.link,
					     &Newline, &newdoc.link,
					     1, TRUE) != NO) {
		Newline++;	/* our line counting starts with 1 not 0 */
		/* nothing more to do here */

	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(NO_LINKS_BELOW);
	    }
	    break;
	}
	case LYK_FASTBACKW_LINK:
	{
	    int samepage = 0, nextlink = curdoc.link;
	    int res;
	    if (nlinks > 1) {

		/*
		 *  If in textarea, move to first link or textarea group
		 *  before it if there is one on this screen. - kw
		 */
		if (links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
		    links[curdoc.link].form->type == F_TEXTAREA_TYPE) {
		    int thisgroup = links[curdoc.link].form->number;
		    char *thisname = links[curdoc.link].form->name;

		    if (curdoc.link > 0 &&
			!(links[0].type == WWW_FORM_LINK_TYPE &&
			  links[0].form->type == F_TEXTAREA_TYPE &&
			  links[0].form->number == thisgroup &&
			  sametext(links[0].form->name, thisname))) {
			do nextlink--;
			while
			    (links[nextlink].type == WWW_FORM_LINK_TYPE &&
			     links[nextlink].form->type == F_TEXTAREA_TYPE &&
			     links[nextlink].form->number == thisgroup &&
			     sametext(links[nextlink].form->name, thisname));
			samepage = 1;

		    } else if (!more && Newline == 1 &&
			       (links[0].type == WWW_FORM_LINK_TYPE &&
				links[0].form->type == F_TEXTAREA_TYPE &&
				links[0].form->number == thisgroup &&
				sametext(links[0].form->name, thisname)) &&
			       !(links[nlinks-1].type == WWW_FORM_LINK_TYPE &&
				 links[nlinks-1].form->type == F_TEXTAREA_TYPE &&
				 links[nlinks-1].form->number == thisgroup &&
				 sametext(links[nlinks-1].form->name, thisname))) {
			nextlink = nlinks - 1;
			samepage = 1;

		    } else if (!more && Newline == 1 && curdoc.link > 0) {
			nextlink = 0;
			samepage = 1;
		    }
		} else if (curdoc.link > 0) {
		    nextlink--;
		    samepage = 1;
		} else if (!more && Newline == 1) {
		    nextlink = nlinks - 1;
		    samepage = 1;
		}
	    }
	    if (samepage) {
		/*
		 *  If the link as determined so far is part of a
		 *  group of textarea fields, try to use the first
		 *  of them that's on the screen instead. - kw
		 */
		if (nextlink > 0 &&
		    links[nextlink].type == WWW_FORM_LINK_TYPE &&
		    links[nextlink].form->type == F_TEXTAREA_TYPE) {
		    int thisgroup = links[nextlink].form->number;
		    char *thisname = links[nextlink].form->name;
		    if (links[0].type == WWW_FORM_LINK_TYPE &&
			links[0].form->type == F_TEXTAREA_TYPE &&
			links[0].form->number == thisgroup &&
			sametext(links[0].form->name, thisname)) {
			nextlink = 0;
		    } else
			while
			    (nextlink > 1 &&
			     links[nextlink-1].type == WWW_FORM_LINK_TYPE &&
			     links[nextlink-1].form->type == F_TEXTAREA_TYPE &&
			     links[nextlink-1].form->number == thisgroup &&
			     sametext(links[nextlink-1].form->name, thisname)) {
			    nextlink--;
			}
		}
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link = nextlink;
		break;		/* and we are done. */

	    } else if (Newline > 1 &&	/* need a previous page */
		       (res = HTGetLinkOrFieldStart(curdoc.link,
						    &Newline, &newdoc.link,
						    -1, TRUE)) != NO) {
		if (res == LINK_DO_ARROWUP) {
		    /*
		     *  It says we should use the normal PREV_LINK
		     *  mechanism, so we'll do that. - kw
		     */
		    if (nlinks > 0)
			curdoc.link = 0;
		    cmd = LYK_PREV_LINK;
		    goto new_cmd;
		}
		Newline++;	/* our line counting starts with 1 not 0 */
		/* nothing more to do here */

	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(NO_LINKS_ABOVE);
	    }
	    break;
	}

	case LYK_UP_LINK:
	    if (curdoc.link > 0 &&
		(links[0].ly != links[curdoc.link].ly ||
		 !HText_LinksInLines(HTMainText, 1, Newline - 1))) {
		/* more links before this on screen, and first of them on
		   a different line or no previous links before this screen? */
		int newlink = -1;
		for (i = curdoc.link; i >= 0; i--) {
		    if (links[i].ly < links[curdoc.link].ly) {
			newlink = i;
			break;
		    }
		}
		if (newlink > -1) {
		    highlight(OFF, curdoc.link, prev_target);
		    curdoc.link = newlink;
#ifdef NOTDEFINED
		} else if (!more && Newline == 1 && curdoc.link == 0) {
		    highlight(OFF, curdoc.link, prev_target);
		    curdoc.link = (nlinks-1);
		} else if (more) {  /* next page */
			Newline += (display_lines);
#else
		} else if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(NO_LINKS_ABOVE);
#endif /* NOTDEFINED */
		}

#ifdef NOTDEFINED
	    /*
	     *	At the bottom of list and there is only one page.
	     *	Move to the top link on the page.
	     */
	    } else if (!more && Newline == 1 && curdoc.link == (nlinks-1)) {
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link = 0;
#endif /* NOTDEFINED */

	    } else if (curdoc.line > 1 && Newline > 1) {  /* previous page */
		int scrollamount = (Newline > display_lines ?
					      display_lines : Newline - 1);
		Newline -= scrollamount;
		if (scrollamount < display_lines &&
		    nlinks > 0 && curdoc.link > -1 &&
		    links[0].ly -1 + scrollamount <= display_lines) {
			newdoc.link = HText_LinksInLines(HTMainText,
							 1,
							 scrollamount) - 1;
		} else {
		    arrowup = TRUE;
		}

	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(ALREADY_AT_BEGIN);
	    }
	    break;

	case LYK_DOWN_LINK:
	    if (curdoc.link < (nlinks-1)) {	/* more links? */
		int newlink = -1;
		for (i = curdoc.link; i < nlinks; i++)
		   if (links[i].ly > links[curdoc.link].ly) {
			newlink = i;
			break;
		   }

		if (newlink > -1) {
		    highlight(OFF, curdoc.link, prev_target);
		    curdoc.link = newlink;
#ifdef NOTDEFINED
		} else if (!more &&
			   Newline == 1 && curdoc.link == (nlinks-1)) {
		    highlight(OFF, curdoc.link, prev_target);
		    curdoc.link = 0;
#endif /* NOTDEFINED */
		} else if (more) {  /* next page */
			Newline += (display_lines);
		} else if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(NO_LINKS_BELOW);
		    break;
		}
#ifdef NOTDEFINED
	    /*
	     *	At the bottom of list and there is only one page.
	     *	Move to the top link on the page.
	     */
	    } else if (!more && Newline == 1 && curdoc.link == (nlinks-1)) {
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link = 0;
#endif /* NOTDEFINED */
	    } else if (more) {	/* next page */
		    Newline += (display_lines);

	    } else if (old_c != real_c) {
		old_c = real_c;
		HTInfoMsg(ALREADY_AT_END);
	    }
	    break;

	case LYK_CHANGE_LINK:
	    do_change_link(prev_target);
	    break;

	case LYK_RIGHT_LINK:
	    if (curdoc.link<nlinks-1 &&
			links[curdoc.link].ly == links[curdoc.link+1].ly) {
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link++;
	    }
	    break;

	case LYK_LEFT_LINK:
	    if (curdoc.link>0 &&
			links[curdoc.link].ly == links[curdoc.link-1].ly) {
		highlight(OFF, curdoc.link, prev_target);
		curdoc.link--;
	    }
	    break;

	case LYK_COOKIE_JAR:	   /* show the cookie jar */
	    /*
	     *	Don't do if already viewing the cookie jar.
	     */
	    if (strcmp((curdoc.title ? curdoc.title : ""),
		       COOKIE_JAR_TITLE)) {
		StrAllocCopy(newdoc.address, "LYNXCOOKIE:/");
		FREE(newdoc.post_data);
		FREE(newdoc.post_content_type);
		FREE(newdoc.bookmark);
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		newdoc.internal_link = FALSE;
		LYforce_no_cache = TRUE;
		if (LYValidate || check_realm) {
		    LYPermitURL = TRUE;
		}
	    } else {
		/*
		 *  If already in the cookie jar, get out.
		 */
		cmd = LYK_PREV_DOC;
		goto new_cmd;
	    }
	    break;

	case LYK_HISTORY:	/* show the history page */
	    if (curdoc.title && strcmp(curdoc.title, HISTORY_PAGE_TITLE)) {
		/*
		 *  Don't do this if already viewing history page.
		 *
		 *  Push the current file so that the history list
		 *  contains the current file for printing purposes.
		 *  Pop the file afterwards to prevent multiple copies.
		 */
		if (TRACE && !LYUseTraceLog && LYCursesON) {
		    move(LYlines-1, LYcols-1);	/* make sure cursor is down */
#ifdef USE_SLANG
		    addstr("\n");
#endif /* USE_SLANG */
		    refresh();
		}
		LYpush(&curdoc, ForcePush);

		/*
		 *  Print history options to file.
		 */
		if (showhistory(&newdoc.address) < 0) {
		    LYpop(&curdoc);
		    break;
		}
		StrAllocCopy(newdoc.title, HISTORY_PAGE_TITLE);
		FREE(newdoc.post_data);
		FREE(newdoc.post_content_type);
		FREE(newdoc.bookmark);
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		newdoc.internal_link = FALSE;
		newdoc.link = 1; /*@@@ bypass "recent statusline messages" link */
		FREE(curdoc.address);  /* so it doesn't get pushed */

		refresh_screen = TRUE;
		if (LYValidate || check_realm) {
		    LYPermitURL = TRUE;
		}
		break;
	    } /* end if strncmp */
	    /*
	     *	Don't put break here so that if the backspace key
	     *	is pressed in the history page, we fall though,
	     *	i.e., it acts like a left arrow.
	     */

	case LYK_PREV_DOC:			 /* back up a level */
	    if (nhist > 0) {  /* if there is anything to go back to */
		/*
		 *  Check if the previous document is a reply from a POST,
		 *  and if so, seek confirmation of resubmission if the safe
		 *  element is not set and the document is not still in the
		 *  cache or LYresubmit_posts is set.  If not confirmed and
		 *  it is not the startfile, pop it so we go to the yet
		 *  previous document, until we're OK or reach the startfile.
		 *  If we reach the startfile and its not OK or we don't get
		 *  confirmation, cancel. - FM
		 */
		DocAddress WWWDoc;
		HTParentAnchor *tmpanchor;
		HText *text;
		BOOLEAN conf = FALSE, first = TRUE;

		HTLastConfirmCancelled(); /* reset flag */
		while (nhist > 0) {
		    conf = FALSE;
		    if (history[(nhist - 1)].post_data == NULL) {
			break;
		    }
		    WWWDoc.address = history[(nhist - 1)].address;
		    WWWDoc.post_data = history[(nhist - 1)].post_data;
		    WWWDoc.post_content_type =
				       history[(nhist - 1)].post_content_type;
		    WWWDoc.bookmark = history[(nhist - 1)].bookmark;
		    WWWDoc.isHEAD = history[(nhist - 1)].isHEAD;
		    WWWDoc.safe = history[(nhist - 1)].safe;
		    tmpanchor = HTAnchor_parent(HTAnchor_findAddress(&WWWDoc));
		    if (HTAnchor_safe(tmpanchor)) {
			break;
		    }
		    if (((text =
			  (HText *)HTAnchor_document(tmpanchor)) == NULL &&
			 (!strncmp(WWWDoc.address, "LYNXIMGMAP:", 11) ||
			 (conf = confirm_post_resub(WWWDoc.address,
						    history[(nhist - 1)].title,
						    0, 0))
			  == FALSE)) ||
			((LYresubmit_posts && !conf &&
			  (NONINTERNAL_OR_PHYS_DIFFERENT(
			      (document *)&history[(nhist - 1)],
			      &curdoc) ||
			   NONINTERNAL_OR_PHYS_DIFFERENT(
			       (document *)&history[(nhist - 1)],
			       &newdoc))) &&
			 !confirm_post_resub(WWWDoc.address,
					     history[(nhist - 1)].title,
					     2, 2))) {
			if (HTLastConfirmCancelled()) {
			    if (!first && curdoc.internal_link)
				FREE(curdoc.address);
			    cmd = LYK_DO_NOTHING;
			    goto new_cmd;
			}
			if (nhist == 1) {
			    HTInfoMsg(CANCELLED);
			    old_c = 0;
			    cmd = LYK_DO_NOTHING;
			    goto new_cmd;
			} else {
			    HTUserMsg2(WWW_SKIP_MESSAGE, WWWDoc.address);
			    do {
				LYpop(&curdoc);
			    } while (nhist > 1 && !are_different(
				(document *)&history[(nhist - 1)],
				&curdoc));
			    first = FALSE; /* have popped at least one */
			    continue;
			}
		    } else {
			/*
			 *  Break from loop; if user just confirmed to
			 *  load again because document wasn't in cache,
			 *  set LYforce_no_cache to avoid unnecessary
			 *  repeat question down the road. - kw
			 */
			if (conf)
			    LYforce_no_cache = TRUE;
			break;
		    }
		}

		if (!first)
		    curdoc.internal_link = FALSE;

		/*
		 *  Set newdoc.address to empty to pop a file.
		 */
		FREE(newdoc.address);
#ifdef DIRED_SUPPORT
		if (lynx_edit_mode)
		    HTuncache_current_document();
#endif /* DIRED_SUPPORT */
	    } else if (child_lynx == TRUE) {
		return(0); /* exit on left arrow in main screen */

	    } else if (old_c != real_c) {
		old_c = real_c;
		HTUserMsg(ALREADY_AT_FIRST);
	    }
	    break;

	case LYK_NOCACHE: /* Force submission of form or link with no-cache */
	    if (nlinks > 0) {
		if (links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
		    links[curdoc.link].form->type != F_SUBMIT_TYPE &&
		    links[curdoc.link].form->type != F_IMAGE_SUBMIT_TYPE) {
		    if (old_c == real_c)
			break;
		    old_c = real_c;
		    HTUserMsg(NOT_ON_SUBMIT_OR_LINK);
		    break;
		} else {
		    LYforce_no_cache = TRUE;
		    reloading = TRUE;
		}
	    } /* fall through to LYK_ACTIVATE */

	case LYK_ACTIVATE:			/* follow a link */
	    {
	     /* Is there a mouse-clicked link waiting? */
	     int mouse_tmp = get_mouse_link();
	     /* If yes, use it as the link */
	     if (mouse_tmp != -1) curdoc.link = mouse_tmp;
	    }
	     if (nlinks > 0) {
		if (links[curdoc.link].type == WWW_FORM_LINK_TYPE) {
		    /*
		     *	Don't try to submit forms with bad actions. - FM
		     */
		    if (links[curdoc.link].form->type == F_SUBMIT_TYPE ||
			links[curdoc.link].form->type == F_IMAGE_SUBMIT_TYPE ||
			links[curdoc.link].form->type ==
						    F_TEXT_SUBMIT_TYPE) {
			/*
			 *  Do nothing if it's disabled. - FM
			 */
			if (links[curdoc.link].form->disabled == YES) {
			    HTOutputFormat = WWW_PRESENT;
			    LYforce_no_cache = FALSE;
			    reloading = FALSE;
			    break;
			}
			/*
			 *  Make sure we have an action. - FM
			 */
			if (!links[curdoc.link].form->submit_action ||
			    *links[curdoc.link].form->submit_action
								== '\0') {
			    HTUserMsg(NO_FORM_ACTION);
			    HTOutputFormat = WWW_PRESENT;
			    LYforce_no_cache = FALSE;
			    reloading = FALSE;
			    break;
			}
			/*
			 *  Check for no_mail if the form action
			 *  is a mailto URL. - FM
			 */
			if (links[curdoc.link].form->submit_method
				     == URL_MAIL_METHOD && no_mail) {
			    HTAlert(FORM_MAILTO_DISALLOWED);
			    HTOutputFormat = WWW_PRESENT;
			    LYforce_no_cache = FALSE;
			    reloading = FALSE;
			    break;
			}
			/*
			 *  Make sure this isn't a spoof in an account
			 *  with restrictions on file URLs. - FM
			 */
			if (no_file_url &&
			    !strncasecomp(
				    links[curdoc.link].form->submit_action,
					  "file:", 5)) {
			    HTAlert(FILE_ACTIONS_DISALLOWED);
			    HTOutputFormat = WWW_PRESENT;
			    LYforce_no_cache = FALSE;
			    reloading = FALSE;
			    break;
			}
			/*
			 *  Make sure this isn't a spoof attempt
			 *  via an internal URL. - FM
			 */
			if (!strncasecomp(
				    links[curdoc.link].form->submit_action,
					  "LYNXCOOKIE:", 11) ||
#ifdef DIRED_SUPPORT
#ifdef OK_PERMIT
			    (!(strncasecomp(
				    links[curdoc.link].form->submit_action,
					   "LYNXDIRED:", 10)) &&
			     (no_dired_support ||
			      strncasecomp(
				(links[curdoc.link].form->submit_action + 10),
					   "//PERMIT_LOCATION", 17) ||
			      strcmp(curdoc.address, LYPermitFileURL) ||
			      strcmp((curdoc.title ? curdoc.title : ""),
				     PERMIT_OPTIONS_TITLE)))  ||
#else
			    !strncasecomp(
				    links[curdoc.link].form->submit_action,
					  "LYNXDIRED:", 10) ||
#endif /* OK_PERMIT */
#endif /* DIRED_SUPPORT */
			    !strncasecomp(
				    links[curdoc.link].form->submit_action,
					  "LYNXDOWNLOAD:", 13) ||
			    !strncasecomp(
				    links[curdoc.link].form->submit_action,
					  "LYNXHIST:", 9) ||
			    !strncasecomp(
				    links[curdoc.link].form->submit_action,
					  "LYNXKEYMAP:", 11) ||
			    !strncasecomp(
				    links[curdoc.link].form->submit_action,
					  "LYNXIMGMAP:", 11) ||
			    !strncasecomp(
				    links[curdoc.link].form->submit_action,
					  "LYNXPRINT:", 10) ||
			    !strncasecomp(
				    links[curdoc.link].form->submit_action,
					  "lynxexec:", 9) ||
			    !strncasecomp(
				    links[curdoc.link].form->submit_action,
					  "lynxprog:", 9)) {
			    HTAlert(SPECIAL_ACTION_DISALLOWED);
			    CTRACE(tfp, "LYMainLoop: Rejected '%s'\n",
					links[curdoc.link].form->submit_action);
			    HTOutputFormat = WWW_PRESENT;
			    LYforce_no_cache = FALSE;
			    reloading = FALSE;
			    break;
			}
#ifdef NOTDEFINED /* We're disabling form inputs instead of using this. - FM */
			/*
			 *  Check for enctype and let user know we
			 *  don't yet support multipart/form-data - FM
			 */
			if (links[curdoc.link].form->submit_enctype) {
			    if (!strcmp(
				     links[curdoc.link].form->submit_enctype,
					"multipart/form-data")) {
				HTAlert(
	gettext("Enctype multipart/form-data not yet supported!  Cannot submit."));
				HTOutputFormat = WWW_PRESENT;
				LYforce_no_cache = FALSE;
				reloading = FALSE;
				break;
			    }
			}
#endif /* NOTDEFINED */
			if (check_realm) {
			    LYPermitURL = TRUE;
			}
			if (no_filereferer == TRUE &&
			    !strncmp(curdoc.address, "file:", 5)) {
			    LYNoRefererForThis = TRUE;
			}
			StrAllocCopy(newdoc.title,
				     links[curdoc.link].hightext);
		    }
		    c = change_form_link(&links[curdoc.link],
					 &newdoc, &refresh_screen,
					 links[curdoc.link].form->name,
					 links[curdoc.link].form->value);
		    if (HTOutputFormat == HTAtom_for("www/download") &&
			newdoc.post_data != NULL &&
			newdoc.safe == FALSE) {
			if ((HText_POSTReplyLoaded(&newdoc) == TRUE) &&
			    HTConfirm(CONFIRM_POST_RESUBMISSION) == FALSE) {
			    HTInfoMsg(CANCELLED);
			    HTOutputFormat = WWW_PRESENT;
			    LYforce_no_cache = FALSE;
			    StrAllocCopy(newdoc.address, curdoc.address);
			    StrAllocCopy(newdoc.title, curdoc.title);
			    StrAllocCopy(newdoc.post_data, curdoc.post_data);
			    StrAllocCopy(newdoc.post_content_type,
					 curdoc.post_content_type);
			    StrAllocCopy(newdoc.bookmark, curdoc.bookmark);
			    newdoc.isHEAD = curdoc.isHEAD;
			    newdoc.safe = curdoc.safe;
			    newdoc.internal_link = curdoc.internal_link;
			    break;
			}
		    }
		    if (c == 23) {
			c = DO_NOTHING;
			refresh_screen = TRUE;
		    }
		    goto new_keyboard_input;
		} else {
		    /*
		     *	Not a forms link.
		     *
		     *	Make sure this isn't a spoof in an account
		     *	with restrictions on file URLs. - FM
		     */
		    if (no_file_url &&
			!strncmp(links[curdoc.link].lname, "file:", 5)) {
			if (strncmp(curdoc.address, "file:", 5)) {
			    HTAlert(FILE_SERVED_LINKS_DISALLOWED);
			    reloading = FALSE;
			    break;
			} else if (curdoc.bookmark != NULL) {
			    HTAlert(FILE_BOOKMARKS_DISALLOWED);
			    reloading = FALSE;
			    break;
			}
		    }
		    /*
		     *	Make sure this isn't a spoof attempt
		     *	via an internal URL in a non-internal
		     *	document. - FM
		     */
		    if ((!strncmp(links[curdoc.link].lname,
				  "LYNXCOOKIE:", 11) &&
			 strcmp((curdoc.title ? curdoc.title : ""),
				COOKIE_JAR_TITLE)) ||
#ifdef DIRED_SUPPORT
			(!strncmp(links[curdoc.link].lname,
				  "LYNXDIRED:", 10) &&
			 (strcmp(curdoc.address, LYDiredFileURL) ||
			  strcmp((curdoc.title ? curdoc.title : ""),
				DIRED_MENU_TITLE)) &&
			 (strcmp(curdoc.address, LYPermitFileURL) ||
			  strcmp((curdoc.title ? curdoc.title : ""),
				PERMIT_OPTIONS_TITLE)) &&
			 (strcmp(curdoc.address, LYUploadFileURL) ||
			  strcmp((curdoc.title ? curdoc.title : ""),
				UPLOAD_OPTIONS_TITLE))) ||
#endif /* DIRED_SUPPORT */
			(!strncmp(links[curdoc.link].lname,
				 "LYNXDOWNLOAD:", 13) &&
			 strcmp((curdoc.title ? curdoc.title : ""),
				DOWNLOAD_OPTIONS_TITLE)) ||
			(!strncmp(links[curdoc.link].lname,
				  "LYNXHIST:", 9) &&
			 strcmp((curdoc.title ? curdoc.title : ""),
				HISTORY_PAGE_TITLE) &&
			 strcmp(curdoc.address, LYlist_temp_url())) ||
			(!strncmp(links[curdoc.link].lname,
				  "LYNXPRINT:", 10) &&
			 strcmp((curdoc.title ? curdoc.title : ""),
				PRINT_OPTIONS_TITLE))) {
			    HTAlert(SPECIAL_VIA_EXTERNAL_DISALLOWED);
			    HTOutputFormat = WWW_PRESENT;
			    LYforce_no_cache = FALSE;
			    reloading = FALSE;
			    break;
			}
		    /*
		     *	Follow a normal link or anchor.
		     */
		    StrAllocCopy(newdoc.address, links[curdoc.link].lname);
		    StrAllocCopy(newdoc.title, links[curdoc.link].hightext);
#ifndef DONT_TRACK_INTERNAL_LINKS
		/*
		 *  For internal links, retain POST content if present.
		 *  If we are on the List Page, prevent pushing it on
		 *  the history stack.	Otherwise set try_internal to
		 *  signal that the top of the loop should attempt to
		 *  reposition directly, without calling getfile. - kw
		 */
		    /*
		     *	Might be an internal link anchor in the same doc.
		     *	If so, take the try_internal shortcut if we didn't
		     *	fall through from LYK_NOCACHE. - kw
		     */
		    newdoc.internal_link =
			(links[curdoc.link].type == WWW_INTERN_LINK_TYPE);
		    if (newdoc.internal_link) {
			/*
			 *  Special case of List Page document with an
			 *  internal link indication, which may really stand
			 *  for an internal link within the document the
			 *  List Page is about. - kw
			 */
			if ( 0==strcmp(curdoc.address, LYlist_temp_url()) &&
			    (LYIsListpageTitle(curdoc.title ? curdoc.title : ""))) {
			    if (!curdoc.post_data ||
				/*
				 *  Normal case - List Page is not associated
				 *  with post data. - kw
				 */
				(!LYresubmit_posts && curdoc.post_data &&
				history[nhist - 1].post_data &&
				!strcmp(curdoc.post_data,
					 history[nhist - 1].post_data) &&
				HText_getContentBase() &&
				!strncmp(HText_getContentBase(),
					 strncmp(history[nhist - 1].address,
						 "LYNXIMGMAP:", 11) ?
					 history[nhist - 1].address :
					 history[nhist - 1].address + 11,
					 strlen(HText_getContentBase())))) {
				/*
				 *  Normal case - as best as we can check, the
				 *  document at the top of the history stack
				 *  seems to be the document the List Page is
				 *  about (or a LYNXIMGMAP derived from it),
				 *  and LYresubmit_posts is not set, so don't
				 *  prompt here.  If we actually have to repeat
				 *  a POST because, against expectations, the
				 *  underlying document isn't cached any more,
				 *  HTAccess will prompt for confirmation,
				 *  unless we had LYK_NOCACHE. - kw
				 */
				LYinternal_flag = TRUE;
			    } else {
				HTLastConfirmCancelled(); /* reset flag */
				if (!confirm_post_resub(newdoc.address,
							newdoc.title,
						(LYresubmit_posts &&
				       HText_POSTReplyLoaded(&newdoc)) ? 1 : 2,
							2)) {
				    if (HTLastConfirmCancelled() ||
					(LYresubmit_posts &&
					 cmd != LYK_NOCACHE &&
					 !HText_POSTReplyLoaded(&newdoc))) {
					/* cancel the whole thing */
					LYforce_no_cache = FALSE;
					reloading = FALSE;
					StrAllocCopy(newdoc.address, curdoc.address);
					StrAllocCopy(newdoc.title, curdoc.title);
					newdoc.internal_link = curdoc.internal_link;
					HTInfoMsg(CANCELLED);
					break;
				    } else if (LYresubmit_posts &&
					       cmd != LYK_NOCACHE) {
					/* If LYresubmit_posts is set, and the
					   answer was No, and the key wasn't
					   NOCACHE, and we have a cached copy,
					   then use it. - kw */
					LYforce_no_cache = FALSE;
				    } else {
					/* if No, but not ^C or ^G, drop
					 * the post data.  Maybe the link
					 * wasn't meant to be internal after
					 * all, here we can recover from that
					 * assumption. - kw */
					FREE(newdoc.post_data);
					FREE(newdoc.post_content_type);
					newdoc.internal_link = FALSE;
					HTAlert(DISCARDING_POST_DATA);
				    }
				}
			    }
			    /*
			     *	Don't push the List Page if we follow an
			     *	internal link given by it. - kw
			     */
			    FREE(curdoc.address);
			} else if (cmd != LYK_NOCACHE) {
			    try_internal = TRUE;
			}
			if (!(LYresubmit_posts && newdoc.post_data))
			    LYinternal_flag = TRUE;
			/* We still set force_load so that history pushing
			** etc. will be done.  - kw */
			force_load = TRUE;
			break;
		    } else {
			/*
			 *  Free POST content if not an internal link. - kw
			 */
			FREE(newdoc.post_data);
			FREE(newdoc.post_content_type);
		    }
#endif /* TRACK_INTERNAL_LINKS */
		    /*
		     *	Might be an anchor in the same doc from a POST
		     *	form.  If so, dont't free the content. -- FM
		     */
		    if (are_different(&curdoc, &newdoc)) {
			FREE(newdoc.post_data);
			FREE(newdoc.post_content_type);
			FREE(newdoc.bookmark);
		    }
		    if (!no_jump && lynxjumpfile && curdoc.address &&
			!strcmp(lynxjumpfile, curdoc.address)) {
			LYJumpFileURL = TRUE;
			LYUserSpecifiedURL = TRUE;
		    } else if ((curdoc.title &&
			       !strcmp(curdoc.title, HISTORY_PAGE_TITLE)) ||
			       curdoc.bookmark != NULL ||
			       (lynxjumpfile &&
				!strcmp(lynxjumpfile, curdoc.address))) {
			LYUserSpecifiedURL = TRUE;
		    } else if (no_filereferer == TRUE &&
			       !strncmp(curdoc.address, "file:", 5)) {
			LYNoRefererForThis = TRUE;
		    }
		    newdoc.link = 0;
		    force_load = TRUE;	/* force MainLoop to reload */
#ifdef USE_PSRC
		    psrc_view = FALSE;	/* we get here if link is not internal */
#endif

#ifdef DIRED_SUPPORT
		    if (lynx_edit_mode) {
			  HTuncache_current_document();
			  /*
			   *  Unescaping any slash chars in the URL,
			   *  but avoid double unescaping and too-early
			   *  unescaping of other chars. - KW
			   */
			  HTUnEscapeSome(newdoc.address,"/");
			  strip_trailing_slash(newdoc.address);
		    }
#endif /* DIRED_SUPPORT */
		    if (!strncmp(curdoc.address, "LYNXCOOKIE:", 11)) {
			HTuncache_current_document();
		    }
		}
	    }
	    break;

	case LYK_ELGOTO:   /* edit URL of current link and go to it  */
	    if (no_goto && !LYValidate) {
		/*
		 *  Go to not allowed. - FM
		 */
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(GOTO_DISALLOWED);
		}
		break;
	    }
	    if (!(nlinks > 0 && curdoc.link > -1) ||
		(links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
		 links[curdoc.link].form->type != F_SUBMIT_TYPE &&
		 links[curdoc.link].form->type != F_IMAGE_SUBMIT_TYPE &&
		 links[curdoc.link].form->type != F_TEXT_SUBMIT_TYPE)) {
		/*
		 *  No links on page, or not a normal link
		 *  or form submit button. - FM
		 */
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(NOT_ON_SUBMIT_OR_LINK);
		}
		break;
	    }
	    if ((links[curdoc.link].type == WWW_FORM_LINK_TYPE) &&
		(!links[curdoc.link].form->submit_action ||
		 *links[curdoc.link].form->submit_action == '\0')) {
		/*
		 *  Form submit button with no ACTION defined. - FM
		 */
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(NO_FORM_ACTION);
		}
		break;
	    }
#ifdef DIRED_SUPPORT
	    if (!strncmp(links[curdoc.link].lname,
			 "LYNXDIRED:", 10) ||
		!strcmp(curdoc.address, LYDiredFileURL) ||
		!strcmp((curdoc.title ? curdoc.title : ""),
			DIRED_MENU_TITLE) ||
		!strcmp(curdoc.address, LYPermitFileURL) ||
		!strcmp((curdoc.title ? curdoc.title : ""),
			PERMIT_OPTIONS_TITLE) ||
		!strcmp(curdoc.address, LYUploadFileURL) ||
		!strcmp((curdoc.title ? curdoc.title : ""),
			UPLOAD_OPTIONS_TITLE)) {
		/*
		 *  Disallow editing of File Management URLs. - FM
		 */
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(EDIT_FM_MENU_URLS_DISALLOWED);
		}
		break;
	    }
#endif /* DIRED_SUPPORT */

	    /*
	     *	Save the current user_input_buffer string,
	     *	and load the current link's address. - FM
	     */
	    StrAllocCopy(temp, user_input_buffer);
	    LYstrncpy(user_input_buffer,
		      ((links[curdoc.link].type == WWW_FORM_LINK_TYPE)
						?
	    links[curdoc.link].form->submit_action : links[curdoc.link].lname),
		      (sizeof(user_input_buffer) - 1));

	    /*
	     *	Offer the current link's URL for editing. - FM
	     */
	    _statusline(EDIT_CURLINK_URL);
	    if (((ch = LYgetstr(user_input_buffer, VISIBLE,
				sizeof(user_input_buffer), RECALL)) >= 0) &&
		user_input_buffer[0] != '\0' &&
		strcmp(user_input_buffer,
		       ((links[curdoc.link].type == WWW_FORM_LINK_TYPE)
				? links[curdoc.link].form->submit_action
				: links[curdoc.link].lname))) {
		LYTrimAllStartfile(user_input_buffer); 
		if (user_input_buffer[0] != '\0') {
		    goto check_goto_URL;
		}
	    }
	    /*
	     *	User cancelled via ^G, a full deletion,
	     *	or not modifying the URL. - FM
	     */
	    HTInfoMsg(CANCELLED);
	    strcpy(user_input_buffer, temp);
	    FREE(temp);
	    break;

	case LYK_ECGOTO:   /* edit current URL and go to to it	*/
	    if (no_goto && !LYValidate) {
		/*
		 *  Go to not allowed. - FM
		 */
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(GOTO_DISALLOWED);
		}
		break;
	    }
#ifdef DIRED_SUPPORT
	    if (!strcmp(curdoc.address, LYDiredFileURL) ||
		!strcmp((curdoc.title ? curdoc.title : ""),
			DIRED_MENU_TITLE) ||
		!strcmp(curdoc.address, LYPermitFileURL) ||
		!strcmp((curdoc.title ? curdoc.title : ""),
			PERMIT_OPTIONS_TITLE) ||
		!strcmp(curdoc.address, LYUploadFileURL) ||
		!strcmp((curdoc.title ? curdoc.title : ""),
			UPLOAD_OPTIONS_TITLE)) {
		/*
		 *  Disallow editing of File Management URLs. - FM
		 */
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(EDIT_FM_MENU_URLS_DISALLOWED);
		}
		break;
	    }
#endif /* DIRED_SUPPORT */

	    /*
	     *	Save the current user_input_buffer string,
	     *	and load the current document's address.
	     */
	    StrAllocCopy(temp, user_input_buffer);
	    LYstrncpy(user_input_buffer,
		      curdoc.address,
		      (sizeof(user_input_buffer) - 1));

	    /*
	     *	Warn the user if the current document has POST
	     *	data associated with it. - FM
	     */
	    if (curdoc.post_data)
		HTAlert(CURRENT_DOC_HAS_POST_DATA);

	    /*
	     *	Offer the current document's URL for editing. - FM
	     */
	    _statusline(EDIT_CURDOC_URL);
	    if (((ch = LYgetstr(user_input_buffer, VISIBLE,
				sizeof(user_input_buffer), RECALL)) >= 0) &&
		user_input_buffer[0] != '\0' &&
		strcmp(user_input_buffer, curdoc.address)) {
		LYTrimAllStartfile(user_input_buffer); 
		if (user_input_buffer[0] != '\0') {
		    goto check_goto_URL;
		}
	    }
	    /*
	     *	User cancelled via ^G, a full deletion,
	     *	or not modifying the URL. - FM
	     */
	    HTInfoMsg(CANCELLED);
	    strcpy(user_input_buffer, temp);
	    FREE(temp);
	    break;

	case LYK_GOTO:	 /* 'g' to goto a random URL  */
	    if (no_goto && !LYValidate) {
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(GOTO_DISALLOWED);
		}
		break;
	    }

	    StrAllocCopy(temp, user_input_buffer);
	    if (!goto_buffer)
		*user_input_buffer = '\0';

	    URLTotal = (Goto_URLs ? HTList_count(Goto_URLs) : 0);
	    if (goto_buffer && *user_input_buffer) {
		recall = ((URLTotal > 1) ? RECALL : NORECALL);
		URLNum = 0;
		FirstURLRecall = FALSE;
	    } else {
		recall = ((URLTotal >= 1) ? RECALL : NORECALL);
		URLNum = URLTotal;
		FirstURLRecall = TRUE;
	    }

	    /*
	     *	Ask the user.
	     */
	    _statusline(URL_TO_OPEN);
	    if ((ch = LYgetstr(user_input_buffer, VISIBLE,
			       sizeof(user_input_buffer), recall)) < 0 ) {
		/*
		 *  User cancelled the Goto via ^G.
		 *  Restore user_input_buffer and break. - FM
		 */
		strcpy(user_input_buffer, temp);
		FREE(temp);
		HTInfoMsg(CANCELLED);
		break;
	    }

check_recall:
	    /*
	     *	Get rid of leading spaces (and any other spaces).
	     */
	    LYTrimAllStartfile(user_input_buffer); 
	    if (*user_input_buffer == '\0' &&
		!(recall && (ch == UPARROW || ch == DNARROW))) {
		strcpy(user_input_buffer, temp);
		FREE(temp);
		HTInfoMsg(CANCELLED);
		break;
	    }
	    if (recall && ch == UPARROW) {
		if (FirstURLRecall) {
		    /*
		     *	Use last URL in the list. - FM
		     */
		    FirstURLRecall = FALSE;
		    URLNum = 0;
		} else {
		    /*
		     *	Go back to the previous URL in the list. - FM
		     */
		    URLNum++;
		}
		if (URLNum >= URLTotal)
		    /*
		     *	Roll around to the last URL in the list. - FM
		     */
		    URLNum = 0;
		if ((cp = (char *)HTList_objectAt(Goto_URLs,
						  URLNum)) != NULL) {
		    strcpy(user_input_buffer, cp);
		    if (goto_buffer && *temp &&
			!strcmp(temp, user_input_buffer)) {
			_statusline(EDIT_CURRENT_GOTO);
		    } else if ((goto_buffer && URLTotal == 2) ||
			       (!goto_buffer && URLTotal == 1)) {
			_statusline(EDIT_THE_PREV_GOTO);
		    } else {
			_statusline(EDIT_A_PREV_GOTO);
		    }
		    if ((ch = LYgetstr(user_input_buffer, VISIBLE,
				      sizeof(user_input_buffer),
				      recall)) < 0) {
			/*
			 *  User cancelled the Goto via ^G.
			 *  Restore user_input_buffer and break. - FM
			 */
			strcpy(user_input_buffer, temp);
			FREE(temp);
			HTInfoMsg(CANCELLED);
			break;
		    }
		    goto check_recall;
		}
	    } else if (recall && ch == DNARROW) {
		if (FirstURLRecall) {
		    /*
		     *	Use the first URL in the list. - FM
		     */
		    FirstURLRecall = FALSE;
		    URLNum = URLTotal - 1;
		} else {
		    /*
		     *	Advance to the next URL in the list. - FM
		     */
		    URLNum--;
		}
		if (URLNum < 0)
		    /*
		     *	Roll around to the first URL in the list. - FM
		     */
		    URLNum = URLTotal - 1;
		if ((cp=(char *)HTList_objectAt(Goto_URLs,
						    URLNum)) != NULL) {
		    strcpy(user_input_buffer, cp);
		    if (goto_buffer && *temp &&
			!strcmp(temp, user_input_buffer)) {
			_statusline(EDIT_CURRENT_GOTO);
		    } else if ((goto_buffer && URLTotal == 2) ||
			       (!goto_buffer && URLTotal == 1)) {
			_statusline(EDIT_THE_PREV_GOTO);
		    } else {
			_statusline(EDIT_A_PREV_GOTO);
		    }
		    if ((ch = LYgetstr(user_input_buffer, VISIBLE,
				       sizeof(user_input_buffer),
				       recall)) < 0) {
			/*
			 *  User cancelled the Goto via ^G.
			 *  Restore user_input_buffer and break. - FM
			 */
			strcpy(user_input_buffer, temp);
			FREE(temp);
			HTInfoMsg(CANCELLED);
			break;
		    }
		    goto check_recall;
		}
	    }

check_goto_URL:
	    /* allow going to anchors*/
	    if (*user_input_buffer == '#' ) {
		if ( user_input_buffer[1] &&
		     HTFindPoundSelector(user_input_buffer+1) ) {
		     /* HTFindPoundSelector will initialize www_search_result,
			so we do nothing else. */
		    HTAddGotoURL(user_input_buffer);
		}
		break;
	    }
	    /*
	     *	If its not a URL then make it one.
	     */
	    StrAllocCopy(temp, user_input_buffer);
	    LYFillLocalFileURL((char **)&temp, "file://localhost");
	    LYEnsureAbsoluteURL((char **)&temp, "", TRUE);
	    sprintf(user_input_buffer, "%.*s",
		    (int)(sizeof(user_input_buffer) - 1), temp);
	    FREE(temp);
	    if ((no_file_url || no_goto_file) &&
		!strncmp(user_input_buffer,"file:",5)) {
		HTUserMsg(GOTO_FILE_DISALLOWED);

	    } else if ((no_shell || no_goto_lynxexec
#ifdef EXEC_LINKS
			|| local_exec_on_local_files
#endif /* EXEC_LINKS */
			) &&
		       !strncmp(user_input_buffer, "lynxexec:",9)) {
		HTUserMsg(GOTO_EXEC_DISALLOWED);

	    } else if ((no_shell || no_goto_lynxprog
#ifdef EXEC_LINKS
			|| local_exec_on_local_files
#endif /* EXEC_LINKS */
			) &&
		       !strncmp(user_input_buffer, "lynxprog:",9)) {
		HTUserMsg(GOTO_PROG_DISALLOWED);

	    } else if ((no_shell || no_goto_lynxcgi) &&
		       !strncmp(user_input_buffer, "lynxcgi:", 8)) {
		HTUserMsg(GOTO_CGI_DISALLOWED);

	    } else if (LYValidate &&
		       strncmp(user_input_buffer, "http:", 5) &&
		       strncmp(user_input_buffer, "https:", 6)) {
		HTUserMsg(GOTO_NON_HTTP_DISALLOWED);

	    } else if (no_goto_cso &&
		       !strncmp(user_input_buffer, "cso:", 4)) {
		HTUserMsg(GOTO_CSO_DISALLOWED);

	    } else if (no_goto_finger &&
		       !strncmp(user_input_buffer, "finger:", 7)) {
		HTUserMsg(GOTO_FINGER_DISALLOWED);

	    } else if (no_goto_ftp &&
		       !strncmp(user_input_buffer, "ftp:", 4)) {
		HTUserMsg(GOTO_FTP_DISALLOWED);

	    } else if (no_goto_gopher &&
		       !strncmp(user_input_buffer, "gopher:", 7)) {
		HTUserMsg(GOTO_GOPHER_DISALLOWED);

	    } else if (no_goto_http &&
		       !strncmp(user_input_buffer, "http:", 5)) {
		HTUserMsg(GOTO_HTTP_DISALLOWED);

	    } else if (no_goto_https &&
		       !strncmp(user_input_buffer, "https:", 6)) {
		HTUserMsg(GOTO_HTTPS_DISALLOWED);

	    } else if (no_goto_mailto &&
		       !strncmp(user_input_buffer, "mailto:", 7)) {
		HTUserMsg(GOTO_MAILTO_DISALLOWED);

#ifndef DISABLE_NEWS
	    } else if (no_goto_news &&
		       !strncmp(user_input_buffer, "news:", 5)) {
		HTUserMsg(GOTO_NEWS_DISALLOWED);

	    } else if (no_goto_nntp &&
		       !strncmp(user_input_buffer, "nntp:", 5)) {
		HTUserMsg(GOTO_NNTP_DISALLOWED);
#endif

	    } else if (no_goto_rlogin &&
		       !strncmp(user_input_buffer, "rlogin:", 7)) {
		HTUserMsg(GOTO_RLOGIN_DISALLOWED);

#ifndef DISABLE_NEWS
	    } else if (no_goto_snews &&
		       !strncmp(user_input_buffer, "snews:", 6)) {
		HTUserMsg(GOTO_SNEWS_DISALLOWED);
#endif

	    } else if (no_goto_telnet &&
		       !strncmp(user_input_buffer, "telnet:", 7)) {
		HTUserMsg(GOTO_TELNET_DISALLOWED);

	    } else if (no_goto_tn3270 &&
		       !strncmp(user_input_buffer, "tn3270:", 7)) {
		HTUserMsg(GOTO_TN3270_DISALLOWED);

	    } else if (no_goto_wais &&
		       !strncmp(user_input_buffer, "wais:", 5)) {
		HTUserMsg(GOTO_WAIS_DISALLOWED);

	    } else if (!strncmp(user_input_buffer, "LYNXCOOKIE:", 11) ||
		       !strncmp(user_input_buffer, "LYNXDIRED:", 10) ||
		       !strncmp(user_input_buffer, "LYNXDOWNLOAD:", 13) ||
		       !strncmp(user_input_buffer, "LYNXOPTIONS:", 12) ||
		       !strncmp(user_input_buffer, "LYNXCFG:", 8) ||
		       !strncmp(user_input_buffer, "LYNXCOMPILEOPTS:", 16) ||
		       !strncmp(user_input_buffer, "LYNXPRINT:", 10)) {
		HTUserMsg(GOTO_SPECIAL_DISALLOWED);

	   } else {
		StrAllocCopy(newdoc.address, user_input_buffer);
		newdoc.isHEAD = FALSE;
		/*
		 *  Might be an anchor in the same doc from a POST
		 *  form.  If so, dont't free the content. -- FM
		 */
		if (are_different(&curdoc, &newdoc)) {
		    /*
		     *	Make a name for this new URL.
		     */
		    StrAllocCopy(newdoc.title, gettext("A URL specified by the user"));
		    FREE(newdoc.post_data);
		    FREE(newdoc.post_content_type);
		    FREE(newdoc.bookmark);
		    newdoc.safe = FALSE;
		    newdoc.internal_link = FALSE;
		    force_load = TRUE;
#ifdef DIRED_SUPPORT
		    if (lynx_edit_mode)
			HTuncache_current_document();
#endif /* DIRED_SUPPORT */
		}
		LYUserSpecifiedURL = TRUE;
		HTAddGotoURL(newdoc.address);
	    }
	    break;

	case LYK_HELP:			/* show help file */
	    if (!STREQ(curdoc.address, helpfile)) {
		/*
		 *  Set the filename.
		 */
		StrAllocCopy(newdoc.address, helpfile);
		/*
		 *  Make a name for this help file.
		 */
		StrAllocCopy(newdoc.title, gettext("Help Screen"));
		FREE(newdoc.post_data);
		FREE(newdoc.post_content_type);
		FREE(newdoc.bookmark);
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		newdoc.internal_link = FALSE;
	    }
	    break;

	case LYK_INDEX:  /* index file */
	    /*
	     *	Make sure we are not in the index already.
	     */
	    if (!STREQ(curdoc.address, indexfile)) {

		if (indexfile[0]=='\0') { /* no defined index */
			if (old_c != real_c)	{
			    old_c = real_c;
			    HTUserMsg(NO_INDEX_FILE);
			}

		} else {
		    StrAllocCopy(newdoc.address, indexfile);
		    StrAllocCopy(newdoc.title, gettext("System Index")); /* name it */
		    FREE(newdoc.post_data);
		    FREE(newdoc.post_content_type);
		    FREE(newdoc.bookmark);
		    newdoc.isHEAD = FALSE;
		    newdoc.safe = FALSE;
		    newdoc.internal_link = FALSE;
		} /* end else */
	    }  /* end if */
	    break;

	case LYK_MAIN_MENU:	/* return to main screen */
	    /*
	     *	If its already the homepage then don't reload it.
	     */
	    if (!STREQ(curdoc.address,homepage)) {

		if (HTConfirmDefault(CONFIRM_MAIN_SCREEN, NO) == YES) {
		    StrAllocCopy(newdoc.address, homepage);
		    StrAllocCopy(newdoc.title, gettext("Entry into main screen"));
		    FREE(newdoc.post_data);
		    FREE(newdoc.post_content_type);
		    FREE(newdoc.bookmark);
		    newdoc.isHEAD = FALSE;
		    newdoc.safe = FALSE;
		    newdoc.internal_link = FALSE;
		    highlight(OFF, curdoc.link, prev_target);
#ifdef DIRED_SUPPORT
		    if (lynx_edit_mode)
		      HTuncache_current_document();
#endif /* DIRED_SUPPORT */
		}
	    } else {
		if (old_c != real_c)	{
			old_c = real_c;
			HTUserMsg(IN_MAIN_SCREEN);
		}
	    }
	    break;

	case LYK_OPTIONS:     /* options screen */
#ifdef DIRED_SUPPORT
	    c = dir_list_style;
#endif /* DIRED_SUPPORT */
#ifndef NO_OPTION_MENU
if (!LYUseFormsOptions) {
	    BOOLEAN LYUseDefaultRawMode_flag = LYUseDefaultRawMode;
	    BOOLEAN LYSelectPopups_flag = LYSelectPopups;
	    BOOLEAN verbose_img_flag = verbose_img;
	    BOOLEAN keypad_mode_flag = keypad_mode;
	    BOOLEAN show_dotfiles_flag = show_dotfiles;
	    BOOLEAN user_mode_flag = user_mode;
	    int CurrentAssumeCharSet_flag = UCLYhndl_for_unspec;
	    int CurrentCharSet_flag = current_char_set;
	    int HTfileSortMethod_flag = HTfileSortMethod;
	    char *CurrentUserAgent = NULL;
	    char *CurrentNegoLanguage = NULL;
	    char *CurrentNegoCharset = NULL;
	    StrAllocCopy(CurrentUserAgent, (LYUserAgent ?
					    LYUserAgent : ""));
	    StrAllocCopy(CurrentNegoLanguage, (language ?
					       language : ""));
	    StrAllocCopy(CurrentNegoCharset, (pref_charset ?
					      pref_charset : ""));

	    LYoptions(); /** do the old-style options stuff **/

	    if (keypad_mode_flag != keypad_mode ||
		(user_mode_flag != user_mode &&
		 (user_mode_flag == NOVICE_MODE ||
		  user_mode == NOVICE_MODE)) ||
		(((HTfileSortMethod_flag != HTfileSortMethod) ||
#ifdef DIRED_SUPPORT
		  (c != dir_list_style) ||
#endif /* DIRED_SUPPORT */
		  (show_dotfiles_flag != show_dotfiles)) &&
		 (!strncmp(curdoc.address, "file:", 5) ||
		  !strncmp(curdoc.address, "ftp:", 4))) ||
		CurrentCharSet_flag != current_char_set ||
		CurrentAssumeCharSet_flag != UCLYhndl_for_unspec ||
		verbose_img_flag != verbose_img ||
		LYUseDefaultRawMode_flag != LYUseDefaultRawMode ||
		LYSelectPopups_flag != LYSelectPopups ||
		((strcmp(CurrentUserAgent, (LYUserAgent ?
					    LYUserAgent : "")) ||
		  strcmp(CurrentNegoLanguage, (language ?
					       language : "")) ||
		  strcmp(CurrentNegoCharset, (pref_charset ?
					      pref_charset : ""))) &&
		 (!strncmp(curdoc.address, "http", 4) ||
		  !strncmp(curdoc.address, "lynxcgi:", 8)))) {
		/*
		 *  Check if this is a reply from a POST, and if so,
		 *  seek confirmation of reload if the safe element
		 *  is not set. - FM
		 */
		if ((curdoc.post_data != NULL &&
		     curdoc.safe != TRUE) &&
		    confirm_post_resub(curdoc.address, curdoc.title,
				       2, 1) == FALSE) {
		    HTInfoMsg(WILL_NOT_RELOAD_DOC);
		} else {
		    StrAllocCopy(newdoc.address, curdoc.address);
		    if (((strcmp(CurrentUserAgent, (LYUserAgent ?
					    LYUserAgent : "")) ||
			  strcmp(CurrentNegoLanguage,
				 (language ? language : "")) ||
			  strcmp(CurrentNegoCharset,
				 (pref_charset ? pref_charset : ""))) &&
			 (strncmp(curdoc.address, "http", 4) == 0 ||
			  strncmp(curdoc.address, "lynxcgi:", 8) == 0))) {
			/*
			 *  An option has changed which may influence
			 *  content negotiation, and the resource is from
			 *  a http or https or lynxcgi URL (the only protocols
			 *  which currently do anything with this information).
			 *  Set reloading = TRUE so that proxy caches will be
			 *  flushed, which is necessary until the time when
			 *  all proxies understand HTTP 1.1 Vary: and all
			 *  Servers properly use it...	Treat like
			 *  case LYK_RELOAD (see comments there). - KW
			 */
			reloading = TRUE;
		    }
		    if (HTisDocumentSource()) {
#ifndef USE_PSRC
			HTOutputFormat = WWW_SOURCE;
#else
			if (LYpsrc)
			    psrc_view = TRUE;
			else
			    HTOutputFormat = WWW_SOURCE;
#endif
		    }
#ifdef SOURCE_CACHE
		    if (reloading == FALSE) {
			/* one more attempt to be smart enough: */
			if (HTreparse_document()) {
			    FREE(CurrentUserAgent);
			    FREE(CurrentNegoLanguage);
			    FREE(CurrentNegoCharset);
			    break;
			}
		    }
#endif
		    HEAD_request = HTLoadedDocumentIsHEAD();
		    HTuncache_current_document();
#ifdef NO_ASSUME_SAME_DOC
		    newdoc.line = 1;
		    newdoc.link = 0;
#else
		    newdoc.line = curdoc.line;
		    newdoc.link = curdoc.link;
#endif /* NO_ASSUME_SAME_DOC */
		    LYforce_no_cache = TRUE;
		    FREE(curdoc.address); /* So it doesn't get pushed. */
		}
	    }
	    FREE(CurrentUserAgent);
	    FREE(CurrentNegoLanguage);
	    FREE(CurrentNegoCharset);
	    refresh_screen = TRUE; /* to repaint screen */
	    break;
} /* end if !LYUseFormsOptions */
#endif /* !NO_OPTION_MENU */
#ifndef NO_OPTION_FORMS
	    /*
	     * Generally stolen from LYK_COOKIE_JAR.  Options menu handling is
	     * done in postoptions(), called from getfile() currently.
	     *
	     * postoptions() is also responsible for reloading the document
	     * before the 'options menu' but only when (a few) important
	     * options were changed.
	     *
	     * It is critical that post_data is freed here since the
	     * submission of changed options is done via the same protocol as
	     * LYNXOPTIONS:
	     */
	    /*
	     *	Don't do if already viewing options page.
	     */
	    if (strcmp((curdoc.title ? curdoc.title : ""), OPTIONS_TITLE)) {

		StrAllocCopy(newdoc.address, "LYNXOPTIONS:/");
		FREE(newdoc.post_data);
		FREE(newdoc.post_content_type);
		FREE(newdoc.bookmark);
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		newdoc.internal_link = FALSE;
		LYforce_no_cache = TRUE;
		if (LYValidate || check_realm) {
		    LYPermitURL = TRUE;
		}
	   } else {
		/*
		 *  If already in the options menu, get out.
		 */
		cmd = LYK_PREV_DOC;
		goto new_cmd;
	    }
#endif /* !NO_OPTION_FORMS */
	    break;

	case LYK_INDEX_SEARCH: /* search for a user string */
	    if (is_www_index) {
		/*
		 *  Perform a database search.
		 *
		 *  do_www_search will try to go out and get the document.
		 *  If it returns TRUE, a new document was returned and is
		 *  named in the newdoc.address.
		 */
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		if (do_www_search(&newdoc) == NORMAL) {
		    /*
		     *	Yah, the search succeeded.
		     */
		    if (TRACE && !LYUseTraceLog && LYCursesON) {
			/*
			 *  Make sure cursor is down.
			 */
			move(LYlines-1, LYcols-1);
#ifdef USE_SLANG
			addstr("\n");
#endif /* USE_SLANG */
			refresh();
		    }
		    LYpush(&curdoc, ForcePush);
		    /*
		     *	Make the curdoc.address the newdoc.address so that
		     *	getfile doesn't try to get the newdoc.address.
		     *	Since we have already gotten it.
		     */
		    StrAllocCopy(curdoc.address, newdoc.address);
		    StrAllocCopy(newdoc.post_data, curdoc.post_data);
		    newdoc.internal_link = FALSE;
		    curdoc.line = -1;
		    Newline = 0;
		    refresh_screen = TRUE; /* redisplay it */
		} else if (use_this_url_instead != NULL) {
		    /*
		     *	Got back a redirecting URL.  Check it out.
		     */
		    _user_message("Using %s", use_this_url_instead);
		    /*
		     *	Make a name for this URL.
		     */
		    StrAllocCopy(newdoc.title,
				 "A URL specified by redirection");
		    StrAllocCopy(newdoc.address, use_this_url_instead);
		    FREE(newdoc.post_data);
		    FREE(newdoc.post_content_type);
		    FREE(newdoc.bookmark);
		    newdoc.isHEAD = FALSE;
		    newdoc.safe = FALSE;
		    newdoc.internal_link = FALSE;
		    FREE(use_this_url_instead);
		    force_load = TRUE;
		    break;
		} else {
		    /*
		     *	Yuk, the search failed.  Restore the old file.
		     */
		    StrAllocCopy(newdoc.address, curdoc.address);
		    StrAllocCopy(newdoc.post_data, curdoc.post_data);
		    StrAllocCopy(newdoc.post_content_type,
				 curdoc.post_content_type);
		    StrAllocCopy(newdoc.bookmark, curdoc.bookmark);
		    newdoc.isHEAD = curdoc.isHEAD;
		    newdoc.safe = curdoc.safe;
		    newdoc.internal_link = curdoc.internal_link;
		}
	    } else if (old_c != real_c) {
		old_c = real_c;
		HTUserMsg(NOT_ISINDEX);
	    }
	    break;

	case LYK_WHEREIS: /* search within the document */
	case LYK_NEXT:	  /* search for the next occurrence in the document */
	    /* user search */
	{
	    BOOLEAN have_target_onscreen = (*prev_target != '\0' &&
					    HText_pageHasPrevTarget());
	    BOOL found;
	    int oldcur = curdoc.link; /* temporarily remember */
	    char *remember_old_target = NULL;
	    if (have_target_onscreen)
		StrAllocCopy(remember_old_target, prev_target);
	    else
		StrAllocCopy(remember_old_target, "");

	    if (cmd != LYK_NEXT) {
		/*
		 *  Reset prev_target to force prompting
		 *  for a new search string and to turn
		 *  off highlighting in no search string
		 *  is entered by the user.
		 */
		*prev_target = '\0';
		found = textsearch(&curdoc, prev_target, FALSE);
	    } else {
		/*
		 *  When the third argument is TRUE, the previous
		 *  search string, if any, will be recalled from
		 *  a buffer, loaded into prev_target, and used
		 *  for the search without prompting for a new
		 *  search string.  This allows the LYK_NEXT
		 *  command to repeat a search in a new document,
		 *  after prev_target was reset on fetch of that
		 *  document.
		 */
		found = textsearch(&curdoc, prev_target, TRUE);
	    }

	    /*
	     *	Force a redraw to ensure highlighting of hits
	     *	even when found on the same page, or clearing
	     *	of highlighting is the default search string
	     *	was erased without replacement. - FM
	     */
	    /*
	    ** Well let's try to avoid it at least in a few cases
	    ** where it is not needed. - kw
	    */
	    if (www_search_result >= 0 && www_search_result != curdoc.line) {
		refresh_screen = TRUE; /* doesn't really matter */
	    } else if (!found) {
		refresh_screen = have_target_onscreen;
	    } else if (!have_target_onscreen && found) {
		refresh_screen = TRUE;
	    } else if (www_search_result == curdoc.line &&
		       curdoc.link == oldcur &&
		       curdoc.link >= 0 && nlinks > 0 &&
		       links[curdoc.link].ly >= (display_lines/3)) {
		refresh_screen = TRUE;
	    } else if ((case_sensitive && 0!=strcmp(prev_target,
						    remember_old_target)) ||
		      (!case_sensitive && 0!=strcasecomp8(prev_target,
						    remember_old_target))) {
		refresh_screen = TRUE;
	    }
	    FREE(remember_old_target);
	}
	    break;

	case LYK_COMMENT:  /* reply by mail */
	    if (!owner_address &&
		strncasecomp(curdoc.address, "http", 4)) {
		if (old_c != real_c)	{
		    old_c = real_c;
		    HTUserMsg(NO_OWNER);
		}
	    } else if (no_mail) {
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(MAIL_DISALLOWED);
		}
	    } else {
		if (HTConfirmDefault(CONFIRM_COMMENT, NO)) {
		    if (!owner_address) {
			/*
			 *  No owner defined, so make a guess and
			 *  and offer it to the user. - FM
			 */
			char *address = NULL;
			temp = HTParse(curdoc.address, "", PARSE_PATH);

			if (temp != NULL) {
			    HTUnEscape(temp);
			    if (*temp == '~' && strlen(temp) > 1) {
				/*
				 *  It's a ~user URL so guess user@host. - FM
				 */
				if ((cp = strchr((temp+1), '/')) != NULL)
				    *cp = '\0';
				StrAllocCopy(address, "mailto:");
				StrAllocCat(address, (temp+1));
				StrAllocCat(address, "@");
			    }
			    FREE(temp);
			}
			if (address == NULL)
			    /*
			     *	Wasn't a ~user URL so guess WebMaster@host. - FM
			     */
			    StrAllocCopy(address, "mailto:WebMaster@");
			temp = HTParse(curdoc.address, "", PARSE_HOST);
			StrAllocCat(address, temp);
			HTSprintf0(&temp, NO_OWNER_USE, address);
			c = HTConfirmDefault(temp, NO);
			FREE(temp);
			if (c == YES) {
			    StrAllocCopy(owner_address, address);
			    FREE(address);
			} else {
			    FREE(address);
			    break;
			}
		    }
		    if (is_url(owner_address) != MAILTO_URL_TYPE) {
			/*
			 *  The address is a URL.  Just follow the link.
			 */
			StrAllocCopy(newdoc.address, owner_address);
			newdoc.internal_link = FALSE;
		    } else {
			/*
			 *  The owner_address is a mailto: URL.
			 */
			CONST char *kp = HText_getRevTitle();
			CONST char *id = HText_getMessageID();
			char *tmptitle = NULL;
			if (!kp && HTMainAnchor) {
			    kp = HTAnchor_subject(HTMainAnchor);
			    if (kp && *kp) {
				if (strncasecomp(kp, "Re: ", 4)) {
				    StrAllocCopy(tmptitle, "Re: ");
				    StrAllocCat(tmptitle, kp);
				    kp = tmptitle;
				}
			    }
			}

			if (strchr(owner_address,':')!=NULL)
			     /*
			      *  Send a reply.	The address is after the colon.
			      */
			     reply_by_mail(strchr(owner_address,':')+1,
					   curdoc.address,
					   (kp ? kp : ""), id);
			else
			    reply_by_mail(owner_address, curdoc.address,
					  (kp ? kp : ""), id);

			FREE(tmptitle);
			refresh_screen = TRUE;	/* to force a showpage */
		    }
		}
	    }
	    break;

#ifdef DIRED_SUPPORT
	case LYK_TAG_LINK:	/* tag or untag the current link */
	    if (lynx_edit_mode && nlinks > 0 && !no_dired_support) {
		if (!strcmp(links[curdoc.link].hightext, ".."))
		    break;	/* Never tag the parent directory */
		if (dir_list_style == MIXED_STYLE) {
		    if (!strcmp(links[curdoc.link].hightext, "../"))
			break;
		} else if (!strncmp(links[curdoc.link].hightext, "Up to ", 6))
		    break;
		{
		    /*
		     *	HTList-based management of tag list, see LYLocal.c - KW
		     */
		    HTList * t1 = tagged;
		    char * tagname = NULL;
		    BOOLEAN found = FALSE;

		    while ((tagname = (char *)HTList_nextObject(t1)) != NULL) {
			if (!strcmp(links[curdoc.link].lname, tagname)) {
			    found = TRUE;
			    HTList_removeObject(tagged, tagname);
			    FREE(tagname);
			    tagflag(OFF,curdoc.link);
			    break;
			}
		    }
		    if (!found) {
			if (tagged == NULL)
			    tagged = HTList_new();
			tagname = NULL;
			StrAllocCopy(tagname,links[curdoc.link].lname);
			HTList_addObject(tagged,tagname);
			tagflag(ON,curdoc.link);
		    }
		}
		if (curdoc.link < nlinks-1) {
		    highlight(OFF, curdoc.link, prev_target);
		    curdoc.link++;
		} else if (!more && Newline == 1 && curdoc.link == nlinks-1) {
		    highlight(OFF, curdoc.link, prev_target);
		    curdoc.link = 0;
		} else if (more) {  /* next page */
		    Newline += (display_lines);
		}
	    }
	    break;

	case LYK_MODIFY:  /* rename a file or directory */
	    if (lynx_edit_mode && nlinks > 0 && !no_dired_support) {
		int ret;

		ret = local_modify(&curdoc, &newdoc.address);
		if (ret == PERMIT_FORM_RESULT) { /* Permit form thrown up */
		    refresh_screen = TRUE;
		} else if (ret) {
		    HTuncache_current_document();
		    StrAllocCopy(newdoc.address, curdoc.address);
		    FREE(curdoc.address);
		    FREE(newdoc.post_data);
		    FREE(newdoc.post_content_type);
		    FREE(newdoc.bookmark);
		    newdoc.isHEAD = FALSE;
		    newdoc.safe = FALSE;
		    newdoc.internal_link = FALSE;
		    newdoc.line = curdoc.line;
		    newdoc.link = curdoc.link;
		    clear();
		}
	    }
	    break;

	case LYK_CREATE:  /* create a new file or directory */
	    if (lynx_edit_mode && !no_dired_support) {
		if (local_create(&curdoc)) {
		    HTuncache_current_document();
		    StrAllocCopy(newdoc.address, curdoc.address);
		    FREE(curdoc.address);
		    FREE(newdoc.post_data);
		    FREE(newdoc.post_content_type);
		    FREE(newdoc.bookmark);
		    newdoc.isHEAD = FALSE;
		    newdoc.safe = FALSE;
		    newdoc.line = curdoc.line;
		    newdoc.link = curdoc.link > -1 ? curdoc.link : 0;
		    clear();
		}
	    }
	    break;
#endif /* DIRED_SUPPORT */

	case LYK_EDIT:	/* edit */
	    if (no_editor) {
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(EDIT_DISABLED);
		}
		break;
	    }
#ifdef AUTOEXTEDIT
	    /*
	     *  If we're in a forms TEXTAREA, invoke the editor on *its*
	     *  contents, rather than attempting to edit the html source
	     *  document.  KED
	     */
	    if (links[curdoc.link].type       == WWW_FORM_LINK_TYPE &&
		links[curdoc.link].form->type == F_TEXTAREA_TYPE)   {
		cmd = LYK_EDIT_TEXTAREA;
		goto new_cmd;
	    }

	    /*
	     *  If we're in a forms TEXT type, tell user the request
	     *  is bogus (though in reality, without this trap, if the
	     *  document with the TEXT field is local, the editor *would*
	     *  be invoked on the source .html file; eg, the o(ptions)
	     *  form tempfile).
	     *
	     *  [This is done to avoid possible user confusion, due to
	     *   auto invocation of the editor on the TEXTAREA's contents
	     *   via the above if() statement.]
	     */
	    if (links[curdoc.link].type       == WWW_FORM_LINK_TYPE &&
		links[curdoc.link].form->type == F_TEXT_TYPE)       {
		HTUserMsg (CANNOT_EDIT_FIELD);
		break;
	    }
#endif /* AUTOEXTEDIT */

#ifdef DIRED_SUPPORT
	    /*
	     *	Allow the user to edit the link rather
	     *	than curdoc in edit mode.
	     */
	    if (lynx_edit_mode &&
		editor && *editor != '\0' && !no_dired_support) {
		if (nlinks > 0) {
		    cp = links[curdoc.link].lname;
		    if (is_url(cp) == FILE_URL_TYPE) {
			cp = HTfullURL_toFile(cp);
			StrAllocCopy(tp, cp);
			FREE(cp);

			if (stat(tp, &dir_info) == -1) {
			    HTAlert(NO_STATUS);
			} else {
			    if (S_ISREG(dir_info.st_mode)) {
				StrAllocCopy(tp, links[curdoc.link].lname);
				HTUnEscapeSome(tp, "/");
				if (edit_current_file(tp,
						      curdoc.link, Newline)) {
				    HTuncache_current_document();
				    StrAllocCopy(newdoc.address,
						 curdoc.address);
				    FREE(curdoc.address);
#ifdef NO_SEEK_OLD_POSITION
				    /*
				     *	Go to top of file.
				     */
				    newdoc.line = 1;
				    newdoc.link = 0;
#else
				    /*
				     *	Seek old position,
				     *	which probably changed.
				     */
				    newdoc.line = curdoc.line;
				    newdoc.link = curdoc.link;
#endif /* NO_SEEK_OLD_POSITION */
				    clear();  /* clear the screen */
				}
			    }
			}
			FREE(tp);
		    }
		}
	    } else
#endif /* DIRED_SUPPORT */
	    if (editor && *editor != '\0') {
		if (edit_current_file(newdoc.address, curdoc.link, Newline)) {
		    HTuncache_current_document();
		    LYforce_no_cache = TRUE;  /*force reload of document */
		    FREE(curdoc.address); /* so it doesn't get pushed */
#ifdef NO_SEEK_OLD_POSITION
		    /*
		     *	Go to top of file.
		     */
		    newdoc.line = 1;
		    newdoc.link = 0;
#else
		    /*
		     *	Seek old position, which probably changed.
		     */
		    newdoc.line = curdoc.line;
		    newdoc.link = curdoc.link;
#endif /* NO_SEEK_OLD_POSITION */
		    clear();  /* clear the screen */
		}

	    } else {
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(NO_EDITOR);
		}
	    }
	    break;

	case LYK_DEL_BOOKMARK:	/* remove a bookmark file link */
#ifdef DIRED_SUPPORT
	case LYK_REMOVE:	/* remove files and directories */
	    c = NO;
	    if (lynx_edit_mode && nlinks > 0 && !no_dired_support) {
		local_remove(&curdoc);
		c = YES;
	    } else
#endif /* DIRED_SUPPORT */
	    if (curdoc.bookmark != NULL) {
		if ((c = HTConfirmDefault(CONFIRM_BOOKMARK_DELETE,NO)) != YES)
		    break;
		remove_bookmark_link(links[curdoc.link].anchor_number-1,
				     curdoc.bookmark);
	    } else {	/* behave like REFRESH for backward compatibility */
		refresh_screen = TRUE;
		if (old_c != real_c) {
		    old_c = real_c;
		    lynx_force_repaint();
		}
		break;
	    }
	    if (c == YES) {
		HTuncache_current_document();
		StrAllocCopy(newdoc.address, curdoc.address);
		FREE(curdoc.address);
		newdoc.line = curdoc.line;
		if (curdoc.link == nlinks-1) {
		    /*
		     *	We deleted the last link on the page. - FM
		     */
		    newdoc.link = curdoc.link-1;
		} else {
		    newdoc.link = curdoc.link;
		}
	    }
	    break;

#ifdef DIRED_SUPPORT
	case LYK_INSTALL:  /* install a file into system area */
	    if (lynx_edit_mode && nlinks > 0 && !no_dired_support)
		local_install(NULL, links[curdoc.link].lname, &newdoc.address);
	    break;
#endif /* DIRED_SUPPORT */

	case LYK_INFO:	/* show document info */
	    /*
	     *	Don't do if already viewing info page.
	     */
	    if (strcmp((curdoc.title ? curdoc.title : ""),
		       SHOWINFO_TITLE)) {
		if (showinfo(&curdoc, HText_getNumOfLines(),
			     &newdoc, owner_address) < 0)
		    break;
		StrAllocCopy(newdoc.title, SHOWINFO_TITLE);
		FREE(newdoc.post_data);
		FREE(newdoc.post_content_type);
		FREE(newdoc.bookmark);
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		newdoc.internal_link = FALSE;
		LYforce_no_cache = TRUE;
		if (LYValidate || check_realm)
		    LYPermitURL = TRUE;
	    } else {
		/*
		 *  If already in info page, get out.
		 */
		cmd = LYK_PREV_DOC;
		goto new_cmd;
	    }
	    break;

	case LYK_EDIT_TEXTAREA: /* use external editor on a TEXTAREA - KED */
	    if (no_editor) {
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(EDIT_DISABLED);
		}
		break;
	    }

	    /*
	     *  See if the current link is in a form TEXTAREA.
	     */
	    if (links[curdoc.link].type       == WWW_FORM_LINK_TYPE &&
		links[curdoc.link].form->type == F_TEXTAREA_TYPE)   {

		/* stop screen */
		stop_curses();

		n = HText_ExtEditForm (&links[curdoc.link]);

		/*
		 *  TODO: Move cursor "n" lines from the current line to
		 *	  position it on the 1st trailing blank line in
		 *	  the now edited TEXTAREA.  If the target line/
		 *	  anchor requires us to scroll up/down, position
		 *	  the target in the approximate center of the
		 *	  screen.
		 */

		/* curdoc.link += n;*/	/* works, except for page crossing, */
					/* damnit; why is nothing ever easy */

		/* start screen */
		start_curses();
		refresh_screen = TRUE;

	    } else {

		HTInfoMsg (NOT_IN_TEXTAREA);
	    }
	    break;

	case LYK_GROW_TEXTAREA: /* add new lines to bottom of TEXTAREA - KED */
	    /*
	     *  See if the current link is in a form TEXTAREA.
	     */
	    if (links[curdoc.link].type       == WWW_FORM_LINK_TYPE &&
		links[curdoc.link].form->type == F_TEXTAREA_TYPE)   {

		HText_ExpandTextarea (&links[curdoc.link], TEXTAREA_EXPAND_SIZE);

		refresh_screen = TRUE;

	    } else {

		HTInfoMsg (NOT_IN_TEXTAREA);
	    }
	    break;

	case LYK_INSERT_FILE: /* insert file in TEXTAREA, above cursor - KED */
	    /*
	     *  See if the current link is in a form TEXTAREA.
	     */
	    if (links[curdoc.link].type       == WWW_FORM_LINK_TYPE &&
		links[curdoc.link].form->type == F_TEXTAREA_TYPE)   {

		n = HText_InsertFile (&links[curdoc.link]);

		/*
		 *  TODO: Move cursor "n" lines from the current line to
		 *	  position it on the 1st line following the text
		 *	  that was inserted.  If the target line/anchor
		 *	  requires us to scroll up/down, position the
		 *	  target in the approximate center of the screen.
		 *
		 *  [Current behavior leaves cursor on the same line relative
		 *   to the start of the TEXTAREA that it was on before the
		 *   insertion.  This is the same behavior that occurs with
		 *   (my) editor, so this TODO will stay unimplemented.]
		 */

		refresh_screen = TRUE;

	    } else {

		HTInfoMsg (NOT_IN_TEXTAREA);
	    }
	    break;

	case LYK_PRINT:  /* print the file */
	    if (LYValidate) {
		if (old_c != real_c)	{
		    old_c = real_c;
		    HTUserMsg(PRINT_DISABLED);
		}
		break;
	    }

	    /*
	     *	Don't do if already viewing print options page.
	     */
	    if (strcmp((curdoc.title ? curdoc.title : ""),
		       PRINT_OPTIONS_TITLE)) {

		if (print_options(&newdoc.address,
				  &curdoc.address, HText_getNumOfLines()) < 0)
		    break;
		StrAllocCopy(newdoc.title, PRINT_OPTIONS_TITLE);
		FREE(newdoc.post_data);
		FREE(newdoc.post_content_type);
		FREE(newdoc.bookmark);
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		ForcePush = TRUE;  /* see LYpush() and print_options() */
		if (check_realm)
		    LYPermitURL = TRUE;
		refresh_screen = TRUE;	/* redisplay */
	    }
	    break;

	case LYK_LIST:	/* list links in the current document */
	    /*
	     *	Don't do if already viewing list page.
	     */
	    if (!strcmp((curdoc.title ? curdoc.title : ""),
			LIST_PAGE_TITLE)) {
		/*
		 *  Already viewing list page, so get out.
		 */
		cmd = LYK_PREV_DOC;
		goto new_cmd;
	    }

	    /*
	     *	Print list page to file.
	     */
	    if (showlist(&newdoc, TRUE) < 0)
		break;
	    StrAllocCopy(newdoc.title, LIST_PAGE_TITLE);
	    /*
	     *	showlist will set newdoc's other fields.  It may leave
	     *	post_data intact so the list can be used to follow
	     *	internal links in the current document even if it is
	     *	a POST response. - kw
	     */

	    refresh_screen = TRUE;  /* redisplay */
	    if (LYValidate || check_realm) {
		LYPermitURL = TRUE;
		StrAllocCopy(lynxlistfile, newdoc.address);
	    }
	    break;

#ifdef EXP_ADDRLIST_PAGE
	case LYK_ADDRLIST:   /* always list URL's (only) */
	    /*
	     *	Don't do if already viewing list addresses page.
	     */
	    if (!strcmp((curdoc.title ? curdoc.title : ""),
			ADDRLIST_PAGE_TITLE)) {
		/*
		 *  Already viewing list page, so get out.
		 */
		cmd = LYK_PREV_DOC;
		goto new_cmd;
	    }

	    /*
	     *	Print address list page to file.
	     */
	    if (showlist(&newdoc, FALSE) < 0)
		break;
	    StrAllocCopy(newdoc.title, ADDRLIST_PAGE_TITLE);
	    /*
	     *	showlist will set newdoc's other fields.  It may leave
	     *	post_data intact so the list can be used to follow
	     *	internal links in the current document even if it is
	     *	a POST response. - kw
	     */

	    refresh_screen = TRUE;  /* redisplay */
	    if (LYValidate || check_realm) {
		LYPermitURL = TRUE;
		StrAllocCopy(lynxlistfile, newdoc.address);
	    }
	    break;
#endif /* EXP_ADDRLIST_PAGE */

	case LYK_VLINKS:  /* list links visited during the current session */
	    if (!strcmp((curdoc.title ? curdoc.title : ""),
			VISITED_LINKS_TITLE)) {
		/*
		 *  Already viewing visited links page, so get out.
		 */
		cmd = LYK_PREV_DOC;
		goto new_cmd;
	    }

	    /*
	     *	Print visited links page to file.
	     */
	    if (LYShowVisitedLinks(&newdoc.address) < 0) {
		HTUserMsg(VISITED_LINKS_EMPTY);
		break;
	    }
	    StrAllocCopy(newdoc.title, VISITED_LINKS_TITLE);
	    FREE(newdoc.post_data);
	    FREE(newdoc.post_content_type);
	    FREE(newdoc.bookmark);
	    newdoc.isHEAD = FALSE;
	    newdoc.safe = FALSE;
	    newdoc.internal_link = FALSE;
	    refresh_screen = TRUE;
	    if (LYValidate || check_realm) {
		LYPermitURL = TRUE;
		StrAllocCopy(lynxlinksfile, newdoc.address);
	    }
	    break;

	case LYK_TOOLBAR:  /* go to Toolbar or Banner in current document */
	    if (!HText_hasToolbar(HTMainText)) {
		if (old_c != real_c) {
		    old_c = real_c;
		    HTUserMsg(NO_TOOLBAR);
		}
	    } else if (old_c != real_c) {
		old_c = real_c;
		if ((cp = strchr(curdoc.address, '#')) != NULL)
		    *cp = '\0';
		toolbar = (char *)malloc(strlen(curdoc.address) +
					 strlen(LYToolbarName) + 2);
		if (!toolbar)
		    outofmem(__FILE__, "mainloop");

		sprintf(toolbar, "%s#%s", curdoc.address, LYToolbarName);
		if (cp)
		    *cp = '#';
		StrAllocCopy(newdoc.address, toolbar);
		FREE(toolbar);
		try_internal = TRUE;
		force_load = TRUE;  /* force MainLoop to reload */
	    }
	    break;

#if defined(DIRED_SUPPORT) || defined(VMS)
	case LYK_DIRED_MENU:  /* provide full file management menu */
#ifdef VMS
	    /*
	     *	Check if the CSwing Directory/File Manager is available.
	     *	Will be disabled if LYCSwingPath is NULL, zero-length,
	     *	or "none" (case insensitive), if no_file_url was set via
	     *	the file_url restriction, if no_goto_file was set for
	     *	the anonymous account, or if HTDirAccess was set to
	     *	HT_DIR_FORBID or HT_DIR_SELECTIVE via the -nobrowse
	     *	or -selective switches. - FM
	     */
	    if (!(LYCSwingPath && *LYCSwingPath) ||
		!strcasecomp(LYCSwingPath, "none") ||
		no_file_url || no_goto_file ||
		HTDirAccess == HT_DIR_FORBID ||
		HTDirAccess == HT_DIR_SELECTIVE) {
		if (old_c != real_c)	{
		    old_c = real_c;
		    HTUserMsg(DFM_NOT_AVAILABLE);
		}
		break;
	    }

	    /*
	     *	If we are viewing a local directory listing or a
	     *	local file which is not temporary, invoke CSwing
	     *	with the URL's directory converted to VMS path specs
	     *	and passed as the argument, so we start up CSwing
	     *	positioned on that node of the directory tree.
	     *	Otherwise, pass the current default directory as
	     *	the argument. - FM
	     */
	    if (LYisLocalFile(curdoc.address) &&
		strncasecomp(curdoc.address,
			     lynx_temp_space, strlen(lynx_temp_space))) {
		/*
		 *  We are viewing a local directory or a local file
		 *  which is not temporary. - FM
		 */
		struct stat stat_info;

		cp = HTParse(curdoc.address, "", PARSE_PATH|PARSE_PUNCTUATION);
		HTUnEscape(cp);
		if (HTStat(cp, &stat_info) == -1) {
		    CTRACE(tfp, "mainloop: Can't stat %s\n", cp);
		    FREE(cp);
		    temp = (char *)calloc(1, (strlen(LYCSwingPath) + 4));
		    if (temp == NULL)
			outofmem(__FILE__, "mainloop");
		    sprintf(temp, "%s []", LYCSwingPath);
		    refresh_screen = TRUE;  /* redisplay */
		} else {
		    char *VMSdir = NULL;

		    if (S_ISDIR(stat_info.st_mode)) {
			/*
			 *  We're viewing a local directory.  Make
			 *  that the CSwing argument. - FM
			 */
			LYAddPathSep(&cp);
			StrAllocCopy(VMSdir, HTVMS_name("", cp));
			FREE(cp);
		    } else {
			/*
			 *  We're viewing a local file.  Make it's
			 *  directory the CSwing argument. - FM
			 */
			StrAllocCopy(VMSdir, HTVMS_name("", cp));
			FREE(cp);
			if ((cp = strrchr(VMSdir, ']')) != NULL) {
			    *(cp + 1) = '\0';
			    cp == NULL;
			} else if ((cp = strrchr(VMSdir, ':')) != NULL) {
			    *(cp + 1) = '\0';
			    cp == NULL;
			}
		    }
		    temp = (char *)calloc(1,
					  (strlen(LYCSwingPath) +
					   strlen(VMSdir) +
					   2));
		    if (temp == NULL)
			outofmem(__FILE__, "mainloop");
		    sprintf(temp, "%s %s", LYCSwingPath, VMSdir);
		    FREE(VMSdir);
		    /*
		     *	Uncache the current document in case we
		     *	change, move, or delete it during the
		     *	CSwing session. - FM
		     */
		    HTuncache_current_document();
		    StrAllocCopy(newdoc.address, curdoc.address);
		    StrAllocCopy(newdoc.title,
				 curdoc.title ? curdoc.title : "");
		    StrAllocCopy(newdoc.bookmark, curdoc.bookmark);
		    FREE(curdoc.address);
		    newdoc.line = curdoc.line;
		    newdoc.link = curdoc.link;
		}
	    } else {
		/*
		 *  We're not viewing a local directory or file.
		 *  Pass CSwing the current default directory as
		 *  an argument and don't uncache the current
		 *  document. - FM
		 */
		temp = (char *)calloc(1, (strlen(LYCSwingPath) + 4));
		if (temp == NULL)
		    outofmem(__FILE__, "mainloop");
		sprintf(temp, "%s []", LYCSwingPath);
		refresh_screen = TRUE;	/* redisplay */
	    }
	    stop_curses();
	    LYSystem(temp);
	    start_curses();
	    FREE(temp);
	    break;
#else
	    /*
	     *	Don't do if not allowed or already viewing the menu.
	     */
	    if (lynx_edit_mode && !no_dired_support &&
		strcmp(curdoc.address, LYDiredFileURL) &&
		strcmp((curdoc.title ? curdoc.title : ""),
		       DIRED_MENU_TITLE)) {
		dired_options(&curdoc,&newdoc.address);
		refresh_screen = TRUE;	/* redisplay */
	    }
	    break;
#endif /* VMS */
#endif /* DIRED_SUPPORT || VMS*/

#ifdef USE_EXTERNALS
	case LYK_EXTERN:  /* use external program on url */
	    if  ((nlinks > 0) && (links[curdoc.link].lname != NULL))
	    {
		run_external(links[curdoc.link].lname);
		refresh_screen = TRUE;
	    }
	    break;
#endif /* USE_EXTERNALS */

	case LYK_ADD_BOOKMARK:	/* add link to bookmark file */
	    if (LYValidate) {
		if (old_c != real_c)	{
		    old_c = real_c;
		    HTUserMsg(BOOKMARKS_DISABLED);
		}
		break;
	    }

	    if (strcmp((curdoc.title ? curdoc.title : ""),
		       HISTORY_PAGE_TITLE) &&
		strcmp((curdoc.title ? curdoc.title : ""),
		       SHOWINFO_TITLE) &&
		strcmp((curdoc.title ? curdoc.title : ""),
		       PRINT_OPTIONS_TITLE) &&
#ifdef DIRED_SUPPORT
		strcmp(curdoc.address, LYDiredFileURL) &&
		strcmp((curdoc.title ? curdoc.title : ""),
		       DIRED_MENU_TITLE) &&
		strcmp(curdoc.address, LYPermitFileURL) &&
		strcmp((curdoc.title ? curdoc.title : ""),
		       PERMIT_OPTIONS_TITLE) &&
		strcmp(curdoc.address, LYUploadFileURL) &&
		strcmp((curdoc.title ? curdoc.title : ""),
		       UPLOAD_OPTIONS_TITLE) &&
#endif /* DIRED_SUPPORT */
		strcmp((curdoc.title ? curdoc.title : ""),
		       DOWNLOAD_OPTIONS_TITLE) &&
		strcmp((curdoc.title ? curdoc.title : ""),
		       COOKIE_JAR_TITLE) &&
		strcmp((curdoc.title ? curdoc.title : ""),
		       OPTIONS_TITLE) &&
		((nlinks <= 0) ||
		 (links[curdoc.link].lname != NULL &&
		  strncmp(links[curdoc.link].lname,
			 "LYNXHIST:", 9) &&
		  strncmp(links[curdoc.link].lname,
			 "LYNXPRINT:", 10) &&
		  strncmp(links[curdoc.link].lname,
			 "LYNXDIRED:", 10) &&
		  strncmp(links[curdoc.link].lname,
			 "LYNXDOWNLOAD:", 13) &&
		  strncmp(links[curdoc.link].lname,
			 "LYNXCOOKIE:", 11) &&
		  strncmp(links[curdoc.link].lname,
			 "LYNXOPTIONS:", 12) &&
		  strncmp(links[curdoc.link].lname,
			 "LYNXLIST:", 9)))) {
		if (nlinks > 0) {
		    if (curdoc.post_data == NULL &&
			curdoc.bookmark == NULL &&
			!LYIsListpageTitle(curdoc.title ? curdoc.title : "") &&
			strcmp((curdoc.title ? curdoc.title : ""),
			       VISITED_LINKS_TITLE)) {
			/*
			 *  The document doesn't have POST content,
			 *  and is not a bookmark file, nor is the
			 *  list or visited links page, so we can
			 *  save either that or the link. - FM
			 */
			_statusline(BOOK_D_L_OR_CANCEL);
			c = LYgetch();
			if (TOUPPER(c) == 'D') {
			    save_bookmark_link(curdoc.address, curdoc.title);
			    refresh_screen = TRUE; /* MultiBookmark support */
			    goto check_add_bookmark_to_self;
			}
		    } else {
			if (LYMultiBookmarks == FALSE &&
			    curdoc.bookmark != NULL &&
			    strstr(curdoc.address,
				   (*bookmark_page == '.'
						  ?
				(bookmark_page+1) : bookmark_page)) != NULL) {
			    /*
			     *	If multiple bookmarks are disabled, offer
			     *	the L)ink or C)ancel, but with wording
			     *	which indicates that the link already
			     *	exists in this bookmark file. - FM
			     */
			    _statusline(MULTIBOOKMARKS_SELF);
			} else if (curdoc.post_data != NULL &&
				   links[curdoc.link].type == WWW_INTERN_LINK_TYPE) {
			    /*
			     *	Internal link, and document has POST content.
			     */
			    HTUserMsg(NOBOOK_POST_FORM);
			    break;
			} else {
			    /*
			     *	Only offer the link in a document with
			     *	POST content, or if the current document
			     *	is a bookmark file and multiple bookmarks
			     *	are enabled. - FM
			     */
			    _statusline(BOOK_L_OR_CANCEL);
			}
			c = LYgetch();
		    }
		    if (TOUPPER(c) == 'L') {
			if (curdoc.post_data != NULL &&
			    links[curdoc.link].type == WWW_INTERN_LINK_TYPE) {
			    /*
			     *	Internal link, and document has POST content.
			     */
			    HTUserMsg(NOBOOK_POST_FORM);
			    break;
			}
			/*
			 *  User does want to save the link. - FM
			 */
			if (links[curdoc.link].type != WWW_FORM_LINK_TYPE) {
			    save_bookmark_link(links[curdoc.link].lname,
					       links[curdoc.link].hightext);
			    refresh_screen = TRUE; /* MultiBookmark support */
			} else {
			    HTUserMsg(NOBOOK_FORM_FIELD);
			    break;
			}
		    } else {
			break;
		    }
		} else if (curdoc.post_data != NULL) {
		    /*
		     *	No links, and document has POST content. - FM
		     */
		    HTUserMsg(NOBOOK_POST_FORM);
		    break;
		} else if (curdoc.bookmark != NULL) {
		    /*
		     *	It's a bookmark file from which all
		     *	of the links were deleted. - FM
		     */
		    HTUserMsg(BOOKMARKS_NOLINKS);
		    break;
		} else {
		    _statusline(BOOK_D_OR_CANCEL);
		    c = LYgetch();
		    if (TOUPPER(c) == 'D') {
			save_bookmark_link(curdoc.address, curdoc.title);
			refresh_screen = TRUE; /* MultiBookmark support */
		    } else {
			break;
		    }
		}
check_add_bookmark_to_self:
		if (curdoc.bookmark && BookmarkPage &&
		    !strcmp(curdoc.bookmark, BookmarkPage)) {
		    HTuncache_current_document();
		    StrAllocCopy(newdoc.address, curdoc.address);
		    StrAllocCopy(newdoc.bookmark, curdoc.bookmark);
		    FREE(curdoc.address);
		    newdoc.line = curdoc.line;
		    newdoc.link = curdoc.link;
		    newdoc.internal_link = FALSE;
		}
		FREE(temp);
	    } else {
		if (old_c != real_c)	{
			old_c = real_c;
			HTUserMsg(NOBOOK_HSML);
		}
	    }
	    break;

	case LYK_VIEW_BOOKMARK:   /* v to view home page */
	    if (LYValidate) {
		if (old_c != real_c)	{
		    old_c = real_c;
		    HTUserMsg(BOOKMARKS_DISABLED);
		}
		break;
	    }

	    /*
	     *	See if a bookmark exists.
	     *	If it does replace newdoc.address with it's name.
	     */
	    if ((cp = get_bookmark_filename(&newdoc.address)) != NULL) {
		if (*cp == '\0' || !strcmp(cp, " ") ||
		    !strcmp(curdoc.address, newdoc.address)) {
		    if (LYMultiBookmarks == TRUE)
			refresh_screen = TRUE;
		    break;
		}
		LYforce_no_cache = TRUE;  /*force the document to be reloaded*/
		StrAllocCopy(newdoc.title, BOOKMARK_TITLE);
		StrAllocCopy(newdoc.bookmark, BookmarkPage);
		FREE(newdoc.post_data);
		FREE(newdoc.post_content_type);
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		newdoc.internal_link = FALSE;
	    } else {
		if (old_c != real_c) {
		    old_c = real_c;
		    LYMBM_statusline(BOOKMARKS_NOT_OPEN);
		    sleep(AlertSecs);
		    if (LYMultiBookmarks == TRUE) {
			refresh_screen = TRUE;
		    }
		}
	    }
	    break;

	case LYK_SHELL:  /* shell escape */
	    if (!no_shell) {
		stop_curses();
		printf("%s\r\n", SPAWNING_MSG);
		LYSystem(LYSysShell());
		start_curses();
		refresh_screen = TRUE;	/* for an HText_pageDisplay() */
	    } else {
		if (old_c != real_c)	{
			old_c = real_c;
			HTUserMsg(SPAWNING_DISABLED);
		}
	    }
	    break;

	case LYK_DOWNLOAD:
	    /*
	     *	Don't do if both download and disk_save are restricted.
	     */
	    if (LYValidate ||
		(no_download && !override_no_download && no_disk_save)) {
		if (old_c != real_c)	{
		    old_c = real_c;
		    HTUserMsg(DOWNLOAD_DISABLED);
		}
		break;
	    }

	    /*
	     *	Don't do if already viewing download options page.
	     */
	    if (!strcmp((curdoc.title ? curdoc.title : ""),
			DOWNLOAD_OPTIONS_TITLE))
		break;

	    if (nlinks > 0) {
		if (links[curdoc.link].type == WWW_FORM_LINK_TYPE) {
		    if (links[curdoc.link].form->type == F_SUBMIT_TYPE ||
			links[curdoc.link].form->type == F_IMAGE_SUBMIT_TYPE) {
			if (links[curdoc.link].form->submit_method ==
				 URL_MAIL_METHOD) {
			    if (old_c != real_c) {
				old_c = real_c;
				HTUserMsg(NO_DOWNLOAD_MAILTO_ACTION);
			    }
			    break;
			}
			if (!strncmp(links[curdoc.link].form->submit_action,
				"LYNXOPTIONS:", 12)) {
			    if (old_c != real_c) {
				old_c = real_c;
				HTUserMsg(NO_DOWNLOAD_SPECIAL);
			    }
			    break;
			}
			HTOutputFormat = HTAtom_for("www/download");
			LYforce_no_cache = TRUE;
			cmd = LYK_ACTIVATE;
			goto new_cmd;
		    }
		    if (old_c != real_c) {
			old_c = real_c;
			HTUserMsg(NO_DOWNLOAD_INPUT);
		    }

		} else if (!strcmp((curdoc.title ? curdoc.title : ""),
				   COOKIE_JAR_TITLE)) {
		    if (old_c != real_c)	{
			old_c = real_c;
			HTUserMsg(NO_DOWNLOAD_COOKIES);
		    }

		} else if (!strcmp((curdoc.title ? curdoc.title : ""),
				   PRINT_OPTIONS_TITLE)) {
		    if (old_c != real_c)	{
			old_c = real_c;
			HTUserMsg(NO_DOWNLOAD_PRINT_OP);
		    }

#ifdef DIRED_SUPPORT
		} else if (!strcmp(curdoc.address, LYUploadFileURL) ||
			   !strcmp((curdoc.title ? curdoc.title : ""),
				   UPLOAD_OPTIONS_TITLE)) {
		    if (old_c != real_c)	{
			old_c = real_c;
			HTUserMsg(NO_DOWNLOAD_UPLOAD_OP);
		    }

		} else if (!strcmp(curdoc.address, LYPermitFileURL) ||
			   !strcmp((curdoc.title ? curdoc.title : ""),
				   PERMIT_OPTIONS_TITLE)) {
		    if (old_c != real_c)	{
			old_c = real_c;
			HTUserMsg(NO_DOWNLOAD_PERMIT_OP);
		    }

		} else if (lynx_edit_mode && !no_dired_support) {
		    /*
		     *	Don't bother making a /tmp copy of the local file.
		     */
		    StrAllocCopy(temp, newdoc.address);
		    StrAllocCopy(newdoc.address, links[curdoc.link].lname);
		    if (LYdownload_options(&newdoc.address,
					   links[curdoc.link].lname) < 0)
			StrAllocCopy(newdoc.address, temp);
		    else
			newdoc.internal_link = FALSE;
		    FREE(temp);
#endif /* DIRED_SUPPORT */

		} else if (!strcmp((curdoc.title ? curdoc.title : ""),
				   HISTORY_PAGE_TITLE) &&
		    !strncmp(links[curdoc.link].lname, "LYNXHIST:", 9)) {
		    int number = atoi(links[curdoc.link].lname+9);
		    if ((history[number].post_data != NULL &&
			 history[number].safe != TRUE) &&
			HTConfirm(CONFIRM_POST_RESUBMISSION) == FALSE) {
			HTInfoMsg(CANCELLED);
			break;
		    }
		    /*
		     *  OK, we download from history page, restore URL from stack.
		     */
		    StrAllocCopy(newdoc.address, history[number].address);
		    StrAllocCopy(newdoc.title, links[curdoc.link].hightext);
		    StrAllocCopy(newdoc.bookmark, history[number].bookmark);
		    FREE(newdoc.post_data);
		    FREE(newdoc.post_content_type);
		    if (history[number].post_data)
			StrAllocCopy(newdoc.post_data,
				     history[number].post_data);
		    if (history[number].post_content_type)
			StrAllocCopy(newdoc.post_content_type,
				     history[number].post_content_type);
		    newdoc.isHEAD = history[number].isHEAD;
		    newdoc.safe = history[number].safe;
		    newdoc.internal_link = FALSE;
		    newdoc.link = 0;
		    HTOutputFormat = HTAtom_for("www/download");
		    LYUserSpecifiedURL = TRUE;
		    /*
		     *	Force the document to be reloaded.
		     */
		    LYforce_no_cache = TRUE;

		} else if (!strncmp(links[curdoc.link].lname, "data:", 5)) {
		    if (old_c != real_c) {
			old_c = real_c;
			HTAlert(UNSUPPORTED_DATA_URL);
		    }

		} else if (!strncmp(links[curdoc.link].lname,
				    "LYNXCOOKIE:", 11) ||
			   !strncmp(links[curdoc.link].lname,
				    "LYNXDIRED:", 10) ||
			   !strncmp(links[curdoc.link].lname,
				    "LYNXDOWNLOAD:", 13) ||
			   !strncmp(links[curdoc.link].lname,
				    "LYNXPRINT:", 10) ||
			   !strncmp(links[curdoc.link].lname,
				    "LYNXOPTIONS:", 12) ||
			   !strncmp(links[curdoc.link].lname,
				    "LYNXCFG:", 8) ||
			   !strncmp(links[curdoc.link].lname,
				    "LYNXCOMPILEOPTS:", 16) ||
			   !strncmp(links[curdoc.link].lname,
				    "lynxexec:", 9) ||
			   !strncmp(links[curdoc.link].lname,
				    "lynxprog:", 9)) {
		    HTUserMsg(NO_DOWNLOAD_SPECIAL);

		} else if (!strncmp(links[curdoc.link].lname,
				    "mailto:", 7)) {
		    HTUserMsg(NO_DOWNLOAD_MAILTO_LINK);

		/*
		 *  From here on we could have a remote host, so check if
		 *  that's allowed.
		 *
		 *  We copy all these checks from getfile() to LYK_DOWNLOAD
		 *  here because LYNXDOWNLOAD:// will NOT be pushing the
		 *  previous document into the history stack so preserve
		 *  getfile() from returning a wrong status (NULLFILE).
		 */
		} else if (local_host_only &&
			   !(LYisLocalHost(links[curdoc.link].lname) ||
			     LYisLocalAlias(links[curdoc.link].lname))) {
		    HTUserMsg(ACCESS_ONLY_LOCALHOST);
		} else {   /* Not a forms, options or history link */
		    /*
		     *	Follow a normal link or anchor.  Note that
		     *	if it's an anchor within the same document,
		     *	entire document will be downloaded.
		     */
		    StrAllocCopy(newdoc.address, links[curdoc.link].lname);
		    StrAllocCopy(newdoc.title, links[curdoc.link].hightext);
#ifndef DONT_TRACK_INTERNAL_LINKS
		    /*
		     *	Might be an internal link in the same doc from a
		     *	POST form.  If so, don't free the content. - kw
		     */
		    if (links[curdoc.link].type != WWW_INTERN_LINK_TYPE)
#else
		    /*
		     *	Might be an anchor in the same doc from a POST
		     *	form.  If so, don't free the content. -- FM
		     */
		    if (are_different(&curdoc, &newdoc))
#endif /* TRACK_INTERNAL_LINKS */
		    {
			FREE(newdoc.post_data);
			FREE(newdoc.post_content_type);
			FREE(newdoc.bookmark);
			newdoc.isHEAD = FALSE;
			newdoc.safe = FALSE;
		    }
		    newdoc.internal_link = FALSE;
		    newdoc.link = 0;
		    HTOutputFormat = HTAtom_for("www/download");
		    /*
		     *	Force the document to be reloaded.
		     */
		    LYforce_no_cache = TRUE;
		}
	    } else if (old_c != real_c) {
		old_c = real_c;
		HTUserMsg(NO_DOWNLOAD_CHOICE);
	    }
	    break;

#ifdef DIRED_SUPPORT
	  case LYK_UPLOAD:
	    /*
	     *	Don't do if already viewing upload options page.
	     */
	    if (!strcmp(curdoc.address, LYUploadFileURL) ||
		!strcmp((curdoc.title ? curdoc.title : ""),
			UPLOAD_OPTIONS_TITLE))
		break;

	    if (lynx_edit_mode && !no_dired_support) {
		LYUpload_options((char **)&newdoc.address,
				 (char *)curdoc.address);
		StrAllocCopy(newdoc.title, UPLOAD_OPTIONS_TITLE);
		FREE(newdoc.post_data);
		FREE(newdoc.post_content_type);
		FREE(newdoc.bookmark);
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		newdoc.internal_link = FALSE;
		/*
		 *  Uncache the current listing so that it will
		 *  be updated to included the uploaded file if
		 *  placed in the current directory. - FM
		 */
		HTuncache_current_document();
	     }
	    break;
#endif /* DIRED_SUPPORT */

	case LYK_TRACE_TOGGLE:	/*  Toggle TRACE mode. */
	    WWW_TraceFlag = ! WWW_TraceFlag;
	    if (LYOpenTraceLog())
		HTUserMsg(WWW_TraceFlag ? TRACE_ON : TRACE_OFF);
	    break;

	case LYK_TRACE_LOG:	/*  View TRACE log. */
	    /*
	     *	Check whether we've started a TRACE log
	     *	in this session. - FM
	     */
	    if (LYTraceLogFP == NULL) {
		HTUserMsg(NO_TRACELOG_STARTED);
		break;
	    }

	    /*
	     *	Don't do if already viewing the TRACE log. - FM
	     */
	    if (!strcmp((curdoc.title ? curdoc.title : ""),
			LYNX_TRACELOG_TITLE))
		break;

	    /*
	     *	If TRACE mode is on, turn it off during this fetch of the
	     *	TRACE log, so we don't enter stuff about this fetch, and
	     *	set a flag for turning it back on when we return to this
	     *	loop.  Note that we'll miss any messages about memory
	     *	exhaustion if it should occur.	It seems unlikely that
	     *	anything else bad might happen, but if it does, we'll
	     *	miss messages about that too.  We also fflush(), close,
	     *	and open it again, to make sure all stderr messages thus
	     *	far will be in the log. - FM
	     */
	    if (!LYReopenTracelog(&trace_mode_flag))
		break;

	    LYLocalFileToURL (&(newdoc.address), LYTraceLogPath);
	    StrAllocCopy(newdoc.title, LYNX_TRACELOG_TITLE);
	    FREE(newdoc.post_data);
	    FREE(newdoc.post_content_type);
	    FREE(newdoc.bookmark);
	    newdoc.isHEAD = FALSE;
	    newdoc.safe = FALSE;
	    newdoc.internal_link = FALSE;
	    if (LYValidate || check_realm) {
		LYPermitURL = TRUE;
	    }
	    LYforce_no_cache = TRUE;
	    break;

	case LYK_IMAGE_TOGGLE:
	    if (clickable_images)
		clickable_images = FALSE;
	    else
		clickable_images = TRUE;

	    HTUserMsg(clickable_images ?
		     CLICKABLE_IMAGES_ON : CLICKABLE_IMAGES_OFF);
#ifdef SOURCE_CACHE
	    if (HTreparse_document()) {
		break;
	    }
#endif
	    cmd = LYK_RELOAD;
	    goto new_cmd;

	case LYK_INLINE_TOGGLE:
	    if (pseudo_inline_alts)
		pseudo_inline_alts = FALSE;
	    else
		pseudo_inline_alts = TRUE;

	    HTUserMsg(pseudo_inline_alts ?
		      PSEUDO_INLINE_ALTS_ON : PSEUDO_INLINE_ALTS_OFF);
#ifdef SOURCE_CACHE
	    if (HTreparse_document()) {
		break;
	    }
#endif
	    cmd = LYK_RELOAD;
	    goto new_cmd;

	case LYK_RAW_TOGGLE:
	    if (HTLoadedDocumentCharset()) {
		HTUserMsg(gettext("charset for this document specified explicitely, sorry..."));
		break;
	    } else {
		LYUseDefaultRawMode = !LYUseDefaultRawMode;
		HTUserMsg(LYRawMode ? RAWMODE_OFF : RAWMODE_ON);
		HTMLSetCharacterHandling(current_char_set);
#ifdef SOURCE_CACHE
		if (HTreparse_document()) {
		    break;
		}
#endif
		cmd = LYK_RELOAD;
		goto new_cmd;
	    }

	case LYK_HEAD:
	    if (nlinks > 0 &&
		(links[curdoc.link].type != WWW_FORM_LINK_TYPE ||
		 links[curdoc.link].form->type == F_SUBMIT_TYPE ||
		 links[curdoc.link].form->type == F_IMAGE_SUBMIT_TYPE)) {
		/*
		 *  We have links, and the current link is a
		 *  normal link or a form's submit button. - FM
		 */
		_statusline(HEAD_D_L_OR_CANCEL);
		c = LYgetch();
		if (TOUPPER(c) == 'D') {
		    char *scheme = strncmp(curdoc.address, "LYNXIMGMAP:", 11) ?
			curdoc.address : curdoc.address + 11;
		    if (LYCanDoHEAD(scheme) != TRUE) {
			HTUserMsg(DOC_NOT_HTTP_URL);
		    } else {
			/*
			 *  Check if this is a reply from a POST,
			 *  and if so, seek confirmation if the
			 *  safe element is not set. - FM
			 */
			if ((curdoc.post_data != NULL &&
			     curdoc.safe != TRUE) &&
			    HTConfirm(CONFIRM_POST_DOC_HEAD) == FALSE) {
			    HTInfoMsg(CANCELLED);
			    break;
			}
			HEAD_request = TRUE;
			LYforce_no_cache = TRUE;
			StrAllocCopy(newdoc.title, curdoc.title);
			if (HTLoadedDocumentIsHEAD()) {
			    HTuncache_current_document();
			    FREE(curdoc.address);
			} else {
			    StrAllocCat(newdoc.title, " - HEAD");
			}
		    }
		    break;
		} else if (TOUPPER(c) == 'L') {
		    if (links[curdoc.link].type != WWW_FORM_LINK_TYPE &&
			strncmp(links[curdoc.link].lname, "http", 4) &&
			strncmp(links[curdoc.link].lname,
				"LYNXIMGMAP:http", 15) &&
			LYCanDoHEAD(links[curdoc.link].lname) != TRUE &&
			(links[curdoc.link].type != WWW_INTERN_LINK_TYPE ||
			 !curdoc.address ||
			 strncmp(curdoc.address, "http", 4))) {
			HTUserMsg(LINK_NOT_HTTP_URL);
		    } else if (links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
			       links[curdoc.link].form->disabled) {
			HTUserMsg(FORM_ACTION_DISABLED);
		    } else if (links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
			       strncmp(links[curdoc.link].form->submit_action,
							      "lynxcgi:", 8) &&
			       strncmp(links[curdoc.link].form->submit_action,
								 "http", 4)) {
			HTUserMsg(FORM_ACTION_NOT_HTTP_URL);
		    } else if (links[curdoc.link].type == WWW_FORM_LINK_TYPE &&
			       links[curdoc.link].form->submit_method ==
							  URL_POST_METHOD &&
			       HTConfirm(CONFIRM_POST_LINK_HEAD) == FALSE) {
			HTInfoMsg(CANCELLED);
		    } else {
			HEAD_request = TRUE;
			LYforce_no_cache = TRUE;
			cmd = LYK_ACTIVATE;
			goto new_cmd;
		    }
		    break;
		}
		break;
	    } else {
		/*
		 *  We can offer only this document for a HEAD request.
		 *  Check if this is a reply from a POST, and if so,
		 *  seek confirmation if the safe element is not set. - FM
		 */
		if ((curdoc.post_data != NULL &&
		     curdoc.safe != TRUE) &&
		    HTConfirm(CONFIRM_POST_DOC_HEAD) == FALSE) {
		    HTInfoMsg(CANCELLED);
		    break;
		} else if (nlinks > 0) {
		    /*
		     *	The current link is a non-submittable form
		     *	link, so prompt the user to make it clear
		     *	that the HEAD request would be for the
		     *	current document, not the form link. - FM
		     */
		    _statusline(HEAD_D_OR_CANCEL);
		    c = LYgetch();
		} else {
		    /*
		     *	No links, so we can just assume that
		     *	the user wants a HEAD request for the
		     *	current document. - FM
		     */
		    c = 'D';
		}
		if (TOUPPER(c) == 'D') {
		    char *scheme = strncmp(curdoc.address, "LYNXIMGMAP:", 11) ?
			curdoc.address : curdoc.address + 11;
		    /*
		     *	The user didn't cancel, so check if
		     *	a HEAD request is appropriate for the
		     *	current document. - FM
		     */
		    if (LYCanDoHEAD(scheme) != TRUE) {
			HTUserMsg(DOC_NOT_HTTP_URL);
		    } else {
			HEAD_request = TRUE;
			LYforce_no_cache = TRUE;
			StrAllocCopy(newdoc.title, curdoc.title);
			if (HTLoadedDocumentIsHEAD()) {
			    HTuncache_current_document();
			    FREE(curdoc.address);
			} else {
			    StrAllocCat(newdoc.title, " - HEAD");
			}
		    }
		}
	    }
	    break;

	case LYK_TOGGLE_HELP:
	    if (user_mode == NOVICE_MODE) {
		toggle_novice_line();
		noviceline(more);
	    }
	    break;

	case LYK_KEYMAP:
	    if (old_c != real_c) {
		old_c = real_c;
		StrAllocCopy(newdoc.address, "LYNXKEYMAP:");
		StrAllocCopy(newdoc.title, CURRENT_KEYMAP_TITLE);
		FREE(newdoc.post_data);
		FREE(newdoc.post_content_type);
		FREE(newdoc.bookmark);
		newdoc.isHEAD = FALSE;
		newdoc.safe = FALSE;
		newdoc.internal_link = FALSE;
		/*
		 *  If vi_keys changed, the keymap did too,
		 *  so force no cache, and reset the flag. - FM
		 */
		if (vi_keys_flag != vi_keys ||
		    emacs_keys_flag != emacs_keys) {
		    LYforce_no_cache = TRUE;
		    vi_keys_flag = vi_keys;
		    emacs_keys_flag = emacs_keys;
		}
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
		/*
		 *  Remember whether we are in dired menu
		 *  so we can display the right keymap.
		 */
		if (!no_dired_support) {
		    prev_lynx_edit_mode = lynx_edit_mode;
		}
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
		LYforce_no_cache = TRUE;
	    }
	    break;

	case LYK_JUMP:
	    {
		char *ret;

		if (no_jump || JThead == NULL) {
		    if (old_c != real_c) {
			old_c = real_c;
			if (no_jump)
			    HTUserMsg(JUMP_DISALLOWED);
			else
			    HTUserMsg(NO_JUMPFILE);
		    }
		} else {
		    LYJumpFileURL = TRUE;
		    if ((ret = LYJump(c)) != NULL) {
#ifdef PERMIT_GOTO_FROM_JUMP
			if (!strncasecomp(ret, "Go ", 3)) {
			    LYJumpFileURL = FALSE;
			    StrAllocCopy(temp, user_input_buffer);
			    URLTotal = (Goto_URLs ?
			  HTList_count(Goto_URLs) : 0);
			    recall = ((URLTotal >= 1) ? RECALL : NORECALL);
			    URLNum = URLTotal;
			    FirstURLRecall = TRUE;
			    if (!strcasecomp(ret, "Go :")) {
				if (recall) {
				    ch = UPARROW;
				    goto check_recall;
				}
				FREE(temp);
				HTUserMsg(NO_RANDOM_URLS_YET);
				break;
			    }
			    ret = HTParse((ret+3), startfile, PARSE_ALL);
			    strcpy(user_input_buffer, ret);
			    FREE(ret);
			    goto check_recall;
			}
#endif /* PERMIT_GOTO_FROM_JUMP */
			ret = HTParse(ret, startfile, PARSE_ALL);
			if (!LYTrimStartfile(ret)) {
			    LYRemoveBlanks(user_input_buffer);
			}
			StrAllocCopy(newdoc.address, ret);
			StrAllocCopy(lynxjumpfile, ret);
			FREE(newdoc.post_data);
			FREE(newdoc.post_content_type);
			FREE(newdoc.bookmark);
			newdoc.isHEAD = FALSE;
			newdoc.safe = FALSE;
			newdoc.internal_link = FALSE;
			FREE(ret);
			LYUserSpecifiedURL = TRUE;
		    } else {
			LYJumpFileURL = FALSE;
		    }
		}
	    }
	    break;

	case LYK_CLEAR_AUTH:
	    if (old_c != real_c) {
		old_c = real_c;
		if (HTConfirm(CLEAR_ALL_AUTH_INFO)) {
		    FREE(authentication_info[0]);
		    FREE(authentication_info[1]);
		    FREE(proxyauth_info[0]);
		    FREE(proxyauth_info[1]);
		    HTClearHTTPAuthInfo();
#ifndef DISABLE_NEWS
		    HTClearNNTPAuthInfo();
#endif
#ifndef DISABLE_FTP
		    HTClearFTPPassword();
#endif
		    HTUserMsg(AUTH_INFO_CLEARED);
		} else {
		    HTUserMsg(CANCELLED);
		}
	    }
	    break;

	case LYK_DO_NOTHING:	/* pretty self explanatory */
	    break;

	} /* end of BIG switch */
    }
}

/*
 *  Ask a post resubmission prompt with some indication of what would
 *  be resubmitted, useful especially for going backward in history.
 *  Try to use parts of the address or, if given, the title, depending
 *  on how much fits on the statusline.
 *  if_imgmap and if_file indicate how to handle an address that is
 *  a "LYNXIMGMAP:", or a "file:" URL (presumably the List Page file),
 *  respectively: 0: auto-deny, 1: auto-confirm, 2: prompt.
 *  - kw
 */

PRIVATE BOOL confirm_post_resub ARGS4(
    CONST char*,	address,
    CONST char*,	title,
    int,		if_imgmap,
    int,		if_file)
{
    size_t len1;
    CONST char *msg = CONFIRM_POST_RESUBMISSION_TO;
    char buf[240];
    char *temp = NULL;
    BOOL res;
    size_t maxlen = LYcols - 6;
    if (!address) {
	return(NO);
    } else if (!strncmp(address, "LYNXIMGMAP:", 11)) {
	if (if_imgmap <= 0)
	    return(NO);
	else if (if_imgmap == 1)
	    return(YES);
	else
	    msg = CONFIRM_POST_LIST_RELOAD;
    } else if (!strncmp(address, "file:", 5)) {
	if (if_file <= 0)
	    return(NO);
	else if (if_file == 1)
	    return(YES);
	else
	    msg = CONFIRM_POST_LIST_RELOAD;
    } else if (dump_output_immediately) {
	return(NO);
    }
    if (maxlen >= sizeof(buf))
	maxlen = sizeof(buf) - 1;
    if ((len1 = strlen(msg)) +
	strlen(address) <= maxlen) {
	sprintf(buf, msg, address);
	return HTConfirm(buf);
    }
    if (len1 + strlen(temp = HTParse(address, "",
				     PARSE_ACCESS+PARSE_HOST+PARSE_PATH
				     +PARSE_PUNCTUATION)) <= maxlen) {
	sprintf(buf, msg, temp);
	res = HTConfirm(buf);
	FREE(temp);
	return(res);
    }
    FREE(temp);
    if (title && (len1 + strlen(title) <= maxlen)) {
	sprintf(buf, msg, title);
	return HTConfirm(buf);
    }
    if (len1 + strlen(temp = HTParse(address, "",
				     PARSE_ACCESS+PARSE_HOST
				     +PARSE_PUNCTUATION)) <= maxlen) {
	sprintf(buf, msg, temp);
	res = HTConfirm(buf);
	FREE(temp);
	return(res);
    }
    FREE(temp);
    if ((temp = HTParse(address, "", PARSE_HOST)) && *temp &&
	len1 + strlen(temp) <= maxlen) {
	sprintf(buf, msg, temp);
	res = HTConfirm(buf);
	FREE(temp);
	return(res);
    }
    FREE(temp);
    return HTConfirm(CONFIRM_POST_RESUBMISSION);
}

PRIVATE int are_different ARGS2(
	document *,	doc1,
	document *,	doc2)
{
    char *cp1, *cp2;

    /*
     *	Do we have two addresses?
     */
    if (!doc1->address || !doc2->address)
	return (TRUE);

    /*
     *	Do they differ in the type of request?
     */
    if (doc1->isHEAD != doc2->isHEAD)
	return (TRUE);

    /*
     *	See if the addresses are different, making sure
     *	we're not tripped up by multiple anchors in the
     *	the same document from a POST form. -- FM
     */
    if ((cp1 = strchr(doc1->address, '#')) != NULL)
	*cp1 = '\0';
    if ((cp2 = strchr(doc2->address, '#')) != NULL)
	*cp2 = '\0';
    /*
     *	Are the base addresses different?
     */
    if (strcmp(doc1->address, doc2->address))
      {
	if (cp1)
	    *cp1 = '#';
	if (cp2)
	    *cp2 = '#';
	return(TRUE);
      }
    if (cp1)
	*cp1 = '#';
    if (cp2)
	*cp2 = '#';

    /*
     *	Do the docs have different contents?
     */
    if (doc1->post_data)
      {
	if (doc2->post_data)
	  {
	    if (strcmp(doc1->post_data, doc2->post_data))
		return(TRUE);
	  }
	else
	    return(TRUE);
      }
    else
	if (doc2->post_data)
	    return(TRUE);

    /*
     *	We'll assume the two documents in fact are the same.
     */
    return(FALSE);
}

/* This determines whether two docs are _physically_ different,
 * meaning they are "from different files". - kw
 */
#ifndef DONT_TRACK_INTERNAL_LINKS
PRIVATE int are_phys_different ARGS2(
	document *,	doc1,
	document *,	doc2)
{
    char *cp1, *cp2, *ap1 = doc1->address, *ap2 = doc2->address;

    /*
     *	Do we have two addresses?
     */
    if (!doc1->address || !doc2->address)
	return (TRUE);

    /*
     *	Do they differ in the type of request?
     */
    if (doc1->isHEAD != doc2->isHEAD)
	return (TRUE);

    /*
     * Skip over possible LYNXIMGMAP parts. - kw
     */
    if (0==strncmp(doc1->address, "LYNXIMGMAP:", 11))
	ap1 += 11;
    if (0==strncmp(doc2->address, "LYNXIMGMAP:", 11))
	ap2 += 11;
    /*
     * If there isn't any real URL in doc2->address, but maybe just
     * a fragment, doc2 is assumed to be an internal reference in
     * the same physical document, so return FALSE. - kw
     */
    if (*ap2 == '\0' || *ap2 == '#')
	return(FALSE);

    /*
     *	See if the addresses are different, making sure
     *	we're not tripped up by multiple anchors in the
     *	the same document from a POST form. -- FM
     */
    if ((cp1 = strchr(doc1->address, '#')) != NULL)
	*cp1 = '\0';
    if ((cp2 = strchr(doc2->address, '#')) != NULL)
	*cp2 = '\0';
    /*
     *	Are the base addresses different?
     */
    if (strcmp(ap1, ap2))
      {
	if (cp1)
	    *cp1 = '#';
	if (cp2)
	    *cp2 = '#';
	return(TRUE);
      }
    if (cp1)
	*cp1 = '#';
    if (cp2)
	*cp2 = '#';

    /*
     *	Do the docs have different contents?
     */
    if (doc1->post_data)
      {
	if (doc2->post_data)
	  {
	    if (strcmp(doc1->post_data, doc2->post_data))
		return(TRUE);
	  }
	else
	    return(TRUE);
      }
    else
	if (doc2->post_data)
	    return(TRUE);

    /*
     *	We'll assume the two documents in fact are the same.
     */
    return(FALSE);
}
#endif

/*
 *  Utility for freeing the list of goto URLs. - FM
 */
PUBLIC void HTGotoURLs_free NOARGS
{
    char *url;
    HTList *cur = Goto_URLs;

    if (!cur)
	return;

    while (NULL != (url = (char *)HTList_nextObject(cur))) {
	FREE(url);
    }
    HTList_delete(Goto_URLs);
    Goto_URLs = NULL;
    return;
}

/*
 *  Utility for listing Goto URLs, making any
 *  repeated URLs the most current in the list. - FM
 */
PUBLIC void HTAddGotoURL ARGS1(
	char *,		url)
{
    char *new;
    char *old;
    HTList *cur;

    if (!(url && *url))
	return;

    if ((new = (char *)calloc(1, (strlen(url) + 1))) == NULL)
	outofmem(__FILE__, "HTAddGotoURL");
    strcpy(new, url);

    if (!Goto_URLs) {
	Goto_URLs = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(HTGotoURLs_free);
#endif
	HTList_addObject(Goto_URLs, new);
	return;
    }

    cur = Goto_URLs;
    while (NULL != (old = (char *)HTList_nextObject(cur))) {
	if (!strcmp(old, new)) {
	    HTList_removeObject(Goto_URLs, old);
	    FREE(old);
	    break;
	}
    }
    HTList_addObject(Goto_URLs, new);

    return;
}

/*
 *  When help is not on the screen,
 *  put a message on the screen
 *  to tell the user other misc info.
 */
PRIVATE void print_status_message ARGS2(
	CONST linkstruct,	curlink,
	char **,		cp)
{
    /*
     *	Make sure form novice lines are replaced.
     */
    if (user_mode == NOVICE_MODE) {
	noviceline(more);
    }

    /*
     *	If we are in forms mode then explicitly
     *	tell the user what each kind of link is.
     */
    if (HTisDocumentSource()) {
	/*
	 *  Currently displaying HTML source.
	 */
	_statusline(SOURCE_HELP);

#ifdef INDICATE_FORMS_MODE_FOR_ALL_LINKS_ON_PAGE
    } else if (lynx_mode == FORMS_LYNX_MODE && nlinks > 0) {
#else
#ifdef NORMAL_NON_FORM_LINK_STATUSLINES_FOR_ALL_USER_MODES
    } else if (lynx_mode == FORMS_LYNX_MODE && nlinks > 0 &&
	       !(curlink.type & WWW_LINK_TYPE)) {
#else
    } else if (lynx_mode == FORMS_LYNX_MODE && nlinks > 0 &&
	       !(user_mode == ADVANCED_MODE &&
		 (curlink.type & WWW_LINK_TYPE))) {
#endif /* NORMAL_NON_FORM_LINK_STATUSLINES_FOR_ALL_USER_MODES */
#endif /* INDICATE_FORMS_MODE_FOR_ALL_LINKS_ON_PAGE */
	if (curlink.type == WWW_FORM_LINK_TYPE) {
	    switch(curlink.form->type) {
	    case F_PASSWORD_TYPE:
		if (curlink.form->disabled == YES)
		    statusline(FORM_LINK_PASSWORD_UNM_MSG);
		else
		    statusline(FORM_LINK_PASSWORD_MESSAGE);
		break;
	    case F_OPTION_LIST_TYPE:
		if (curlink.form->disabled == YES)
		    statusline(FORM_LINK_OPTION_LIST_UNM_MSG);
		else
		    statusline(FORM_LINK_OPTION_LIST_MESSAGE);
		break;
	    case F_CHECKBOX_TYPE:
		if (curlink.form->disabled == YES)
		    statusline(FORM_LINK_CHECKBOX_UNM_MSG);
		else
		    statusline(FORM_LINK_CHECKBOX_MESSAGE);
		break;
	    case F_RADIO_TYPE:
		if (curlink.form->disabled == YES)
		    statusline(FORM_LINK_RADIO_UNM_MSG);
		else
		    statusline(FORM_LINK_RADIO_MESSAGE);
		break;
	    case F_TEXT_SUBMIT_TYPE:
		if (curlink.form->disabled == YES) {
		    statusline(FORM_LINK_TEXT_SUBMIT_UNM_MSG);
		} else if (curlink.form->submit_method ==
			 URL_MAIL_METHOD) {
		    if (no_mail)
			statusline(FORM_LINK_TEXT_SUBMIT_MAILTO_DIS_MSG);
		    else
			statusline(FORM_LINK_TEXT_SUBMIT_MAILTO_MSG);
		} else if (curlink.form->no_cache) {
		    statusline(FORM_LINK_TEXT_RESUBMIT_MESSAGE);
		} else {
		    statusline(FORM_LINK_TEXT_SUBMIT_MESSAGE);
		}
		break;
	    case F_SUBMIT_TYPE:
	    case F_IMAGE_SUBMIT_TYPE:
		if (curlink.form->disabled == YES) {
		    statusline(FORM_LINK_SUBMIT_DIS_MSG);
		} else if (curlink.form->submit_method ==
			 URL_MAIL_METHOD) {
		    if (no_mail) {
			statusline(FORM_LINK_SUBMIT_MAILTO_DIS_MSG);
		    } else {
			if(user_mode == ADVANCED_MODE) {
			    char *submit_str = NULL;

			    StrAllocCopy(submit_str, FORM_LINK_SUBMIT_MAILTO_PREFIX);
			    StrAllocCat(submit_str, curlink.form->submit_action);
			    statusline(submit_str);
			    FREE(submit_str);
			} else {
			    statusline(FORM_LINK_SUBMIT_MAILTO_MSG);
			}
		    }
		} else if (curlink.form->no_cache) {
		    if(user_mode == ADVANCED_MODE) {
			char *submit_str = NULL;

			StrAllocCopy(submit_str, FORM_LINK_RESUBMIT_PREFIX);
			StrAllocCat(submit_str, curlink.form->submit_action);
			statusline(submit_str);
			FREE(submit_str);
		    } else {
			statusline(FORM_LINK_RESUBMIT_MESSAGE);
		    }
		} else {
		    if(user_mode == ADVANCED_MODE) {
			char *submit_str = NULL;

			StrAllocCopy(submit_str, FORM_LINK_SUBMIT_PREFIX);
			StrAllocCat(submit_str, curlink.form->submit_action);
			statusline(submit_str);
			FREE(submit_str);
		    } else {
			statusline(FORM_LINK_SUBMIT_MESSAGE);
		    }
		}
		break;
	    case F_RESET_TYPE:
		if (curlink.form->disabled == YES)
		    statusline(FORM_LINK_RESET_DIS_MSG);
		else
		    statusline(FORM_LINK_RESET_MESSAGE);
		break;
	    case F_TEXT_TYPE:
		if (curlink.form->disabled == YES)
		    statusline(FORM_LINK_TEXT_UNM_MSG);
		else
		    statusline(FORM_LINK_TEXT_MESSAGE);
		break;
	    case F_TEXTAREA_TYPE:
		if (curlink.form->disabled == YES)
		    statusline(FORM_LINK_TEXT_UNM_MSG);
		else
		    statusline(FORM_LINK_TEXTAREA_MESSAGE);
		break;
	    }
	} else {
	    statusline(NORMAL_LINK_MESSAGE);
	}

	/*
	 *  Let them know if it's an index -- very rare.
	 */
	if (is_www_index) {
	    move(LYlines-1, LYcols-8);
	    start_reverse();
	    addstr("-index-");
	    stop_reverse();
	}

    } else if (user_mode == ADVANCED_MODE && nlinks > 0) {
	/*
	 *  Show the URL or, for some internal links, the fragment
	 */
	*cp = NULL;
	if (curlink.type == WWW_INTERN_LINK_TYPE &&
	    strncmp(curlink.lname, "LYNXIMGMAP:", 11)) {
	    *cp = strchr(curlink.lname, '#');
	}
	if (!(*cp))
	    *cp = curlink.lname;
	if (more) {
	    if (is_www_index)
		_user_message("-more- -index- %s",
					 *cp);
	    else
		_user_message("-more- %s",*cp);
	} else {
	    if (is_www_index)
		_user_message("-index- %s",*cp);
	    else
		statusline(*cp);
	}
    } else if (is_www_index && more) {
	char buf[128];

	sprintf(buf, WWW_INDEX_MORE_MESSAGE, key_for_func(LYK_INDEX_SEARCH));
	_statusline(buf);
    } else if (is_www_index) {
	char buf[128];

	sprintf(buf, WWW_INDEX_MESSAGE, key_for_func(LYK_INDEX_SEARCH));
	_statusline(buf);
    } else if (more) {
	if (user_mode == NOVICE_MODE)
		_statusline(MORE);
	else
		_statusline(MOREHELP);
    } else {
	_statusline(HELP);
    }
    /* turn off cursor since now it's probably on statusline -HV */
    move((LYlines - 1), (LYcols - 1));
}


PRIVATE void exit_immediately_with_error_message ARGS2(
	int,		state,
	BOOLEAN,	first_file)
{
    char *buf = 0;
    char *buf2 = 0;

    if (first_file) {
	/* print statusline messages as a hint, if any */
	LYstatusline_messages_on_exit(&buf2);
    }

    if (state == NOT_FOUND)
    {
	HTSprintf0(&buf, "%s\n%s %s\n",
		   buf2,
		   gettext("lynx: Can't access startfile"),
		   /*
		    * hack: if we fail in HTAccess.c
		    * avoid duplicating URL, oh.
		    */
		   strstr(buf2, gettext("Can't Access")) ? "" : startfile);
    }

    if (state == NULLFILE)
    {
	HTSprintf0(&buf, "%s\n%s\n%s\n",
		   buf2,
		   gettext("lynx: Start file could not be found or is not text/html or text/plain"),
		   gettext("      Exiting..."));
    }

    FREE(buf2);

    if (!dump_output_immediately)
	cleanup();

#ifdef UNIX
    if (dump_output_immediately) {
	fprintf(stderr, buf);
    } else
#endif /* UNIX */
    {
	SetOutputMode( O_TEXT );
	printf(buf);
	SetOutputMode( O_BINARY );
    }

    FREE(buf);

    if (!dump_output_immediately) {
	exit_immediately(-1);
    }
    /* else: return(-1) in mainloop */
}
