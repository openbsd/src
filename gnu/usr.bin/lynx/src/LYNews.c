#include <HTUtils.h>
#ifndef DISABLE_NEWS
#include <HTParse.h>
#include <HTAccess.h>
#include <HTCJK.h>
#include <HTAlert.h>
#include <LYCurses.h>
#include <LYSignal.h>
#include <LYStructs.h>
#include <LYUtils.h>
#include <LYClean.h>
#include <LYStrings.h>
#include <LYHistory.h>
#include <GridText.h>
#include <LYCharSets.h>
#include <LYNews.h>
#include <LYEdit.h>

#include <LYGlobalDefs.h>

#include <LYLeaks.h>

/*
 *  Global variable for async i/o.
 */
BOOLEAN term_message = FALSE;
static void terminate_message(int sig);

static BOOLEAN message_has_content(const char *filename,
				   BOOLEAN *nonspaces)
{
    FILE *fp;
    char *buffer = NULL;
    BOOLEAN in_headers = TRUE;

    *nonspaces = FALSE;

    if (!filename || (fp = fopen(filename, "r")) == NULL) {
	CTRACE((tfp, "Failed to open file %s for reading!\n",
		NONNULL(filename)));
	return FALSE;
    }
    while (LYSafeGets(&buffer, fp) != NULL) {
	char *cp = buffer;
	char firstnonblank = '\0';

	LYTrimNewline(cp);
	for (; *cp; cp++) {
	    if (!firstnonblank && isgraph(UCH(*cp))) {
		firstnonblank = *cp;
	    } else if (!isspace(UCH(*cp))) {
		*nonspaces = TRUE;
	    }
	}
	if (firstnonblank && firstnonblank != '>') {
	    if (!in_headers) {
		LYCloseInput(fp);
		FREE(buffer);
		return TRUE;
	    }
	}
	if (!firstnonblank) {
	    in_headers = FALSE;
	}
    }
    FREE(buffer);
    LYCloseInput(fp);
    return FALSE;
}

/*
 *  This function is called from HTLoadNews() to have the user
 *  create a file with news headers and a body for posting of
 *  a new message (based on a newspost://nntp_host/newsgroups
 *  or snewspost://secure_nntp_host/newsgroups URL), or to post
 *  a followup (based on a newsreply://nntp_host/newsgroups or
 *  snewsreply://secure_nntp_host/newsgroups URL). The group
 *  or comma-separated list of newsgroups is passed without
 *  a lead slash, and followup is TRUE for newsreply or
 *  snewsreply URLs.  - FM
 */
char *LYNewsPost(char *newsgroups,
		 BOOLEAN followup)
{
    char user_input[1024];
    char CJKinput[1024];
    char *cp = NULL;
    const char *kp = NULL;
    int c = 0;			/* user input */
    int len;
    FILE *fd = NULL;
    char my_tempfile[LY_MAXPATH];
    FILE *fc = NULL;
    char CJKfile[LY_MAXPATH];
    char *postfile = NULL;
    char *NewsGroups = NULL;
    char *References = NULL;
    char *org = NULL;
    FILE *fp = NULL;
    BOOLEAN nonempty = FALSE;
    BOOLEAN nonspaces = FALSE;

    /*
     * Make sure a non-zero length newspost, newsreply, snewspost or snewsreply
     * path was sent to us.  - FM
     */
    if (isEmpty(newsgroups))
	return (postfile);

    /*
     * Return immediately if we do get called, maybe by some quirk of HTNews.c,
     * when we shouldn't.  - kw
     */
    if (no_newspost)
	return (postfile);

    /*
     * Open a temporary file for the headers and message body.  - FM
     */
#ifdef __DJGPP__
    if ((fd = LYOpenTemp(my_tempfile, HTML_SUFFIX, BIN_W)) == NULL)
#else
    if ((fd = LYOpenTemp(my_tempfile, HTML_SUFFIX, "w")) == NULL)
#endif /* __DJGPP__ */
    {
	HTAlert(CANNOT_OPEN_TEMP);
	return (postfile);
    }

    /*
     * If we're using a Japanese display character set, open a temporary file
     * for a conversion to JIS.  - FM
     */
    CJKfile[0] = '\0';
    if (current_char_set == UCGetLYhndl_byMIME("euc-jp") ||
	current_char_set == UCGetLYhndl_byMIME("shift_jis")) {
	if ((fc = LYOpenTemp(CJKfile, HTML_SUFFIX, "w")) == NULL) {
	    HTAlert(CANNOT_OPEN_TEMP);
	    LYRemoveTemp(my_tempfile);
	    return (postfile);
	}
    }

    /*
     * The newsgroups could be a comma-seperated list.  It need not have
     * spaces, but deal with any that may also have been hex escaped.  - FM
     */
    StrAllocCopy(NewsGroups, newsgroups);
    if ((cp = strstr(NewsGroups, ";ref="))) {
	*cp = '\0';
	cp += 5;
	if (*cp == '<') {
	    StrAllocCopy(References, cp);
	} else {
	    StrAllocCopy(References, "<");
	    StrAllocCat(References, cp);
	    StrAllocCat(References, ">");
	}
	HTUnEscape(References);
	if (!((cp = strchr(References, '@')) && cp > References + 1 &&
	      isalnum(UCH(cp[1])))) {
	    FREE(References);
	}
    }
    HTUnEscape(NewsGroups);
    if (!*NewsGroups) {
	LYCloseTempFP(fd);	/* Close the temp file. */
	goto cleanup;
    }

    /*
     * Allow ^C to cancel the posting, i.e., don't let SIGINTs exit Lynx.
     */
    signal(SIGINT, terminate_message);
    term_message = FALSE;

    /*
     * Show the list of newsgroups.  - FM
     */
    LYclear();
    LYmove(2, 0);
    scrollok(LYwin, TRUE);	/* Enable scrolling. */
    LYaddstr(gettext("You will be posting to:"));
    LYaddstr("\n\t");
    LYaddstr(NewsGroups);
    LYaddch('\n');

    /*
     * Get the mail address for the From header, offering personal_mail_address
     * as default.
     */
    LYaddstr(gettext("\n\n Please provide your mail address for the From: header\n"));
    sprintf(user_input, "From: %.*s", (int) sizeof(user_input) - 8,
	    NonNull(personal_mail_address));
    if (LYgetstr(user_input, VISIBLE,
		 sizeof(user_input), NORECALL) < 0 ||
	term_message) {
	HTInfoMsg(NEWS_POST_CANCELLED);
	LYCloseTempFP(fd);	/* Close the temp file. */
	scrollok(LYwin, FALSE);	/* Stop scrolling.      */
	goto cleanup;
    }
    fprintf(fd, "%s\n", user_input);

    /*
     * Get the Subject header, offering the current document's title as the
     * default if this is a followup rather than a new post.  - FM
     */
    LYaddstr(gettext("\n\n Please provide or edit the Subject: header\n"));
    strcpy(user_input, "Subject: ");
    if ((followup == TRUE && nhist > 0) &&
	(kp = HText_getTitle()) != NULL) {
	/*
	 * Add the default subject.
	 */
	kp = LYSkipCBlanks(kp);
#ifdef CJK_EX			/* 1998/05/15 (Fri) 09:10:38 */
	if (HTCJK == JAPANESE) {
	    CJKinput[0] = '\0';
	    switch (kanji_code) {
	    case EUC:
		TO_EUC((const unsigned char *) kp, (unsigned char *) CJKinput);
		kp = CJKinput;
		break;
	    case SJIS:
		TO_SJIS((const unsigned char *) kp, (unsigned char *) CJKinput);
		kp = CJKinput;
		break;
	    default:
		break;
	    }
	}
#endif
	if (strncasecomp(kp, "Re:", 3)) {
	    strcat(user_input, "Re: ");
	}
	len = strlen(user_input);
	LYstrncpy(user_input + len, kp, sizeof(user_input) - len - 1);
    }
    cp = NULL;
    if (LYgetstr(user_input, VISIBLE,
		 sizeof(user_input), NORECALL) < 0 ||
	term_message) {
	HTInfoMsg(NEWS_POST_CANCELLED);
	LYCloseTempFP(fd);	/* Close the temp file. */
	scrollok(LYwin, FALSE);	/* Stop scrolling.      */
	goto cleanup;
    }
    fprintf(fd, "%s\n", user_input);

    /*
     * Add Organization:  header.
     */
    StrAllocCopy(cp, "Organization: ");
    if ((org = LYGetEnv("ORGANIZATION")) != NULL) {
	StrAllocCat(cp, org);
    } else if ((org = LYGetEnv("NEWS_ORGANIZATION")) != NULL) {
	StrAllocCat(cp, org);
    }
#ifdef UNIX
    else if ((fp = fopen("/etc/organization", TXT_R)) != NULL) {
	char *buffer = 0;

	if (LYSafeGets(&buffer, fp) != NULL) {
	    if (user_input[0] != '\0') {
		LYTrimNewline(buffer);
		StrAllocCat(cp, buffer);
	    }
	}
	FREE(buffer);
	LYCloseInput(fp);
    }
#else
#ifdef _WINDOWS			/* 1998/05/14 (Thu) 17:47:01 */
    else {
	char *p, fname[LY_MAXPATH];

	strcpy(fname, LynxSigFile);
	p = strrchr(fname, '/');
	if (p != 0 && (p - fname) < sizeof(fname) - 15) {
	    strcpy(p + 1, "LYNX_ETC.TXT");
	    if ((fp = fopen(fname, TXT_R)) != NULL) {
		if (fgets(user_input, sizeof(user_input), fp) != NULL) {
		    if ((org = strchr(user_input, '\n')) != NULL) {
			*org = '\0';
		    }
		    if (user_input[0] != '\0') {
			StrAllocCat(cp, user_input);
		    }
		}
		LYCloseInput(fp);
	    }
	}
    }
#endif /* _WINDOWS */
#endif /* !UNIX */
    LYstrncpy(user_input, cp, (sizeof(user_input) - 16));
    FREE(cp);
    LYaddstr(gettext("\n\n Please provide or edit the Organization: header\n"));
    if (LYgetstr(user_input, VISIBLE,
		 sizeof(user_input), NORECALL) < 0 ||
	term_message) {
	HTInfoMsg(NEWS_POST_CANCELLED);
	LYCloseTempFP(fd);	/* Close the temp file. */
	scrollok(LYwin, FALSE);	/* Stop scrolling.      */
	goto cleanup;
    }
    fprintf(fd, "%s\n", user_input);

    if (References) {
	fprintf(fd, "References: %s\n", References);
    }
    /*
     * Add Newsgroups Summary and Keywords headers.
     */
    fprintf(fd, "Newsgroups: %s\nSummary: \nKeywords: \n\n", NewsGroups);

    /*
     * Have the user create the message body.
     */
    if (!no_editor && non_empty(editor)) {

	if (followup && nhist > 0) {
	    /*
	     * Ask if the user wants to include the original message.
	     */
	    if (term_message) {
		_statusline(INC_ORIG_MSG_PROMPT);
	    } else if (HTConfirm(INC_ORIG_MSG_PROMPT) == YES) {
		/*
		 * The 'TRUE' will add the reply ">" in front of every line. 
		 * We're assuming that if the display character set is Japanese
		 * and the document did not have a CJK charset, any non-EUC or
		 * non-SJIS 8-bit characters in it where converted to 7-bit
		 * equivalents.  - FM
		 */
		print_wwwfile_to_fd(fd, FALSE, TRUE);
	    }
	}
	LYCloseTempFP(fd);	/* Close the temp file. */
	scrollok(LYwin, FALSE);	/* Stop scrolling.      */
	if (term_message || LYCharIsINTERRUPT(c))
	    goto cleanup;

	/*
	 * Spawn the user's editor on the news file.
	 */
	edit_temporary_file(my_tempfile, "", SPAWNING_EDITOR_FOR_NEWS);

	nonempty = message_has_content(my_tempfile, &nonspaces);

    } else {
	/*
	 * Use the built in line editior.
	 */
	LYaddstr(gettext("\n\n Please enter your message below."));
	LYaddstr(gettext("\n When you are done, press enter and put a single period (.)"));
	LYaddstr(gettext("\n on a line and press enter again."));
	LYaddstr("\n\n");
	LYrefresh();
	*user_input = '\0';
	if (LYgetstr(user_input, VISIBLE,
		     sizeof(user_input), NORECALL) < 0 ||
	    term_message) {
	    HTInfoMsg(NEWS_POST_CANCELLED);
	    LYCloseTempFP(fd);	/* Close the temp file. */
	    scrollok(LYwin, FALSE);	/* Stop scrolling.      */
	    goto cleanup;
	}
	while (!STREQ(user_input, ".") && !term_message) {
	    LYaddch('\n');
	    fprintf(fd, "%s\n", user_input);
	    if (!nonempty && strlen(user_input))
		nonempty = TRUE;
	    *user_input = '\0';
	    if (LYgetstr(user_input, VISIBLE,
			 sizeof(user_input), NORECALL) < 0) {
		HTInfoMsg(NEWS_POST_CANCELLED);
		LYCloseTempFP(fd);	/* Close the temp file. */
		scrollok(LYwin, FALSE);		/* Stop scrolling.      */
		goto cleanup;
	    }
	}
	fprintf(fd, "\n");
	LYCloseTempFP(fd);	/* Close the temp file. */
	scrollok(LYwin, FALSE);	/* Stop scrolling.      */
    }

    if (nonempty) {
	/*
	 * Confirm whether to post, and if so, whether to append the sig file. 
	 * - FM
	 */
	LYStatusLine = (LYlines - 1);
	c = HTConfirm(POST_MSG_PROMPT);
	LYStatusLine = -1;
	if (c != YES) {
	    LYclear();		/* clear the screen */
	    goto cleanup;
	}
    } else {
	HTAlert(gettext("Message has no original text!"));
	if (!nonspaces
	    || HTConfirmDefault(POST_MSG_PROMPT, NO) != YES)
	    goto cleanup;
    }
    if ((LynxSigFile != NULL) && (fp = fopen(LynxSigFile, TXT_R)) != NULL) {
	char *msg = NULL;

	HTSprintf0(&msg, APPEND_SIG_FILE, LynxSigFile);

	LYStatusLine = (LYlines - 1);
	if (term_message) {
	    _user_message(APPEND_SIG_FILE, LynxSigFile);
	} else if (HTConfirm(msg) == YES) {
	    if ((fd = LYAppendToTxtFile(my_tempfile)) != NULL) {
		char *buffer = NULL;

		fputs("-- \n", fd);
		while (LYSafeGets(&buffer, fp) != NULL) {
		    fputs(buffer, fd);
		}
		LYCloseOutput(fd);
	    }
	}
	LYCloseInput(fp);
	FREE(msg);
	LYStatusLine = -1;
    }
    LYclear();			/* clear the screen */

    /*
     * If we are using a Japanese display character set, convert the contents
     * of the temp file to JIS (nothing should change if it does not, in fact,
     * contain EUC or SJIS di-bytes).  Otherwise, use the temp file as is.  -
     * FM
     */
    if (CJKfile[0] != '\0') {
	if ((fd = fopen(my_tempfile, TXT_R)) != NULL) {
	    char *buffer = NULL;

	    while (LYSafeGets(&buffer, fd) != NULL) {
		TO_JIS((unsigned char *) buffer,
		       (unsigned char *) CJKinput);
		fputs(CJKinput, fc);
	    }
	    LYCloseTempFP(fc);
	    StrAllocCopy(postfile, CJKfile);
	    LYCloseInput(fd);
	    LYRemoveTemp(my_tempfile);
	    strcpy(my_tempfile, CJKfile);
	    CJKfile[0] = '\0';
	} else {
	    StrAllocCopy(postfile, my_tempfile);
	}
    } else {
	StrAllocCopy(postfile, my_tempfile);
    }
    if (!followup) {
	/*
	 * If it's not a followup, the current document most likely is the
	 * group listing, so force a to have the article show up in the list
	 * after the posting.  Note, that if it's a followup via a link in a
	 * news article, the user must do a reload manually on returning to the
	 * group listing.  - FM
	 */
	LYforce_no_cache = TRUE;
    }
    LYStatusLine = (LYlines - 1);
    HTUserMsg(POSTING_TO_NEWS);
    LYStatusLine = -1;

    /*
     * Come here to cleanup and exit.
     */
  cleanup:
#ifndef VMS
    signal(SIGINT, cleanup_sig);
#endif /* !VMS */
    term_message = FALSE;
    if (!postfile)
	LYRemoveTemp(my_tempfile);
    LYRemoveTemp(CJKfile);
    FREE(NewsGroups);
    FREE(References);

    return (postfile);
}

static void terminate_message(int sig GCC_UNUSED)
{
    term_message = TRUE;
    /*
     * Reassert the AST.
     */
    signal(SIGINT, terminate_message);
#ifdef VMS
    /*
     * Refresh the screen to get rid of the "interrupt" message.
     */
    lynx_force_repaint();
    LYrefresh();
#endif /* VMS */
}

#endif /* not DISABLE_NEWS */
