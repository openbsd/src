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

#include <LYGlobalDefs.h>

#include <LYLeaks.h>

/*
**  Global variable for async i/o.
*/
BOOLEAN term_message = FALSE;
PRIVATE void terminate_message  PARAMS((int sig));

PRIVATE BOOLEAN message_has_content ARGS1(
    CONST char *,		filename)
{
    FILE *fp;
    char *buffer = NULL;
    BOOLEAN in_headers = TRUE;

    if (!filename || (fp = fopen(filename, "r")) == NULL) {
	CTRACE(tfp, "Failed to open file %s for reading!\n",
	       filename ? filename : "(<null>)");
	return FALSE;
    }
    while (LYSafeGets(&buffer, fp) != NULL) {
	char *cp = buffer;
	char firstnonblank = '\0';
	if (*cp == '\0') {
	    break;
	}
	for (; *cp; cp++) {
	    if (*cp == '\n') {
		break;
	    } else if (*cp != ' ') {
		if (!firstnonblank && isgraph((unsigned char)*cp)) {
		    firstnonblank = *cp;
		}
	    }
	}
	if (*cp != '\n') {
	    int c;
	    while ((c = getc(fp)) != EOF && c != (int)(unsigned char)'\n') {
		if (!firstnonblank && isgraph((unsigned char)c))
		    firstnonblank = (char)c;
	    }
	}
	if (firstnonblank && firstnonblank != '>') {
	    if (!in_headers) {
		fclose(fp);
		FREE(buffer);
		return TRUE;
	    }
	}
	if (!firstnonblank) {
	    in_headers = FALSE;
	}
    }
    FREE(buffer);
    fclose(fp);
    return FALSE;
}

/*
**  This function is called from HTLoadNews() to have the user
**  create a file with news headers and a body for posting of
**  a new message (based on a newspost://nntp_host/newsgroups
**  or snewspost://secure_nntp_host/newsgroups URL), or to post
**  a followup (based on a newsreply://nntp_host/newsgroups or
**  snewsreply://secure_nntp_host/newsgroups URL). The group
**  or comma-separated list of newsgroups is passed without
**  a lead slash, and followup is TRUE for newsreply or
**  snewsreply URLs.  - FM
*/
PUBLIC char *LYNewsPost ARGS2(
	char *,		newsgroups,
	BOOLEAN,	followup)
{
    char user_input[1024];
    char CJKinput[1024];
    char *cp = NULL;
    CONST char *kp = NULL;
    int c = 0;  /* user input */
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

    /*
     *  Make sure a non-zero length newspost, newsreply,
     *  snewspost or snewsreply path was sent to us. - FM
     */
    if (!(newsgroups && *newsgroups))
	return(postfile);

    /*
     *  Open a temporary file for the headers
     *  and message body. - FM
     */
    if ((fd = LYOpenTemp(my_tempfile, HTML_SUFFIX, "w")) == NULL) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(postfile);
    }

    /*
     *  If we're using a Japanese display character set,
     *  open a temporary file for a conversion to JIS. - FM
     */
    CJKfile[0] = '\0';
    if (current_char_set == UCGetLYhndl_byMIME("euc-jp") ||
	current_char_set == UCGetLYhndl_byMIME("shift_jis")) {
	if ((fc = LYOpenTemp(CJKfile, HTML_SUFFIX, "w")) == NULL) {
	    HTAlert(CANNOT_OPEN_TEMP);
	    LYRemoveTemp(my_tempfile);
	    return(postfile);
	}
    }

    /*
     *  The newsgroups could be a comma-seperated list.
     *  It need not have spaces, but deal with any that
     *  may also have been hex escaped. - FM
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
	      isalnum(cp[1]))) {
	    FREE(References);
	}
    }
    HTUnEscape(NewsGroups);
    if (!*NewsGroups) {
	LYCloseTempFP(fd);		/* Close the temp file.	*/
	goto cleanup;
    }

    /*
     *  Allow ^C to cancel the posting,
     *  i.e., don't let SIGINTs exit Lynx.
     */
    signal(SIGINT, terminate_message);
    term_message = FALSE;

    /*
     *  Show the list of newsgroups. - FM
     */
    clear();
    move(2,0);
    scrollok(stdscr, TRUE);	/* Enable scrolling. */
    addstr(gettext("You will be posting to:"));
    addstr("\n\t");
    addstr(NewsGroups);
    addch('\n');

    /*
     *  Get the mail address for the From header,
     *  offering personal_mail_address as default.
     */
    addstr(gettext("\n\n Please provide your mail address for the From: header\n"));
    strcpy(user_input, "From: ");
    if (personal_mail_address)
	strcat(user_input, personal_mail_address);
    if (LYgetstr(user_input, VISIBLE,
		 sizeof(user_input), NORECALL) < 0 ||
	term_message) {
	HTInfoMsg(NEWS_POST_CANCELLED);
	LYCloseTempFP(fd);		/* Close the temp file.	*/
	scrollok(stdscr, FALSE);	/* Stop scrolling.	*/
	goto cleanup;
    }
    fprintf(fd, "%s\n", user_input);

    /*
     *  Get the Subject header, offering the current
     *  document's title as the default if this is a
     *  followup rather than a new post. - FM
     */
    addstr(gettext("\n\n Please provide or edit the Subject: header\n"));
    strcpy(user_input, "Subject: ");
    if ((followup == TRUE && nhist > 0) &&
	(kp = HText_getTitle()) != NULL) {
	/*
	 *  Add the default subject.
	 */
	kp = LYSkipCBlanks(kp);
	if (strncasecomp(kp, "Re:", 3)) {
	    strcat(user_input, "Re: ");
	}
	strcat(user_input, kp);
    }
    cp = NULL;
    if (LYgetstr(user_input, VISIBLE,
		 sizeof(user_input), NORECALL) < 0 ||
	term_message) {
	HTInfoMsg(NEWS_POST_CANCELLED);
	LYCloseTempFP(fd);		/* Close the temp file. */
	scrollok(stdscr, FALSE);	/* Stop scrolling.	*/
	goto cleanup;
    }
    fprintf(fd,"%s\n",user_input);

    /*
     *  Add Organization: header.
     */
    StrAllocCopy(cp, "Organization: ");
    if (((org = getenv("ORGANIZATION")) != NULL) && *org != '\0') {
	StrAllocCat(cp, org);
    } else if (((org = getenv("NEWS_ORGANIZATION")) != NULL) &&
	       *org != '\0') {
	StrAllocCat(cp, org);
#ifndef VMS
    } else if ((fp = fopen("/etc/organization", "r")) != NULL) {
	char *buffer = 0;
	if (LYSafeGets(&buffer, fp) != NULL) {
	    if ((org = strchr(buffer, '\n')) != NULL) {
		*org = '\0';
	    }
	    if (user_input[0] != '\0') {
		StrAllocCat(cp, buffer);
	    }
	}
	FREE(buffer);
	fclose(fp);
#endif /* !VMS */
    }
    LYstrncpy(user_input, cp, (sizeof(user_input) - 16));
    FREE(cp);
    addstr(gettext("\n\n Please provide or edit the Organization: header\n"));
    if (LYgetstr(user_input, VISIBLE,
		 sizeof(user_input), NORECALL) < 0 ||
	term_message) {
	HTInfoMsg(NEWS_POST_CANCELLED);
	LYCloseTempFP(fd);		/* Close the temp file. */
	scrollok(stdscr, FALSE);	/* Stop scrolling.	*/
	goto cleanup;
    }
    fprintf(fd, "%s\n", user_input);

    if (References) {
	fprintf(fd, "References: %s\n", References);
    }
    /*
     *  Add Newsgroups Summary and Keywords headers.
     */
    fprintf(fd, "Newsgroups: %s\nSummary: \nKeywords: \n\n", NewsGroups);

    /*
     *  Have the user create the message body.
     */
    if (!no_editor && editor && *editor != '\0') {
	/*
	 *  Use an external editor.
	 */
	char *editor_arg = "";

	if (followup && nhist > 0) {
	    /*
	     *  Ask if the user wants to include the original message.
	     */
	    if (term_message) {
		_statusline(INC_ORIG_MSG_PROMPT);
	    } else if (HTConfirm(INC_ORIG_MSG_PROMPT) == YES) {
		/*
		 *  The 1 will add the reply ">" in front of every line.
		 *  We're assuming that if the display character set is
		 *  Japanese and the document did not have a CJK charset,
		 *  any non-EUC or non-SJIS 8-bit characters in it where
		 *  converted to 7-bit equivalents. - FM
		 */
		print_wwwfile_to_fd(fd, 1);
	    }
	}
	LYCloseTempFP(fd);		/* Close the temp file. */
	scrollok(stdscr, FALSE);	/* Stop scrolling.	*/
	if (term_message || c == 7 || c == 3)
	    goto cleanup;

	/*
	 *  Spawn the user's editor on the news file.
	 */
	if (strstr(editor, "pico")) {
	    editor_arg = " -t"; /* No prompt for filename to use */
	}
	sprintf(user_input,"%s%s %s", editor, editor_arg, my_tempfile);
	_statusline(SPAWNING_EDITOR_FOR_NEWS);
	stop_curses();
	if (LYSystem(user_input)) {
	    start_curses();
	    HTAlert(ERROR_SPAWNING_EDITOR);
	} else {
	    start_curses();
	}

	nonempty = message_has_content(my_tempfile);

    } else {
	/*
	 *  Use the built in line editior.
	 */
	addstr(gettext("\n\n Please enter your message below."));
	addstr(gettext("\n When you are done, press enter and put a single period (.)"));
	addstr(gettext("\n on a line and press enter again."));
	addstr("\n\n");
	refresh();
	*user_input = '\0';
	if (LYgetstr(user_input, VISIBLE,
		     sizeof(user_input), NORECALL) < 0 ||
	    term_message) {
	    HTInfoMsg(NEWS_POST_CANCELLED);
	    LYCloseTempFP(fd);		/* Close the temp file.	*/
	    scrollok(stdscr, FALSE);	/* Stop scrolling.	*/
	    goto cleanup;
	}
	while (!STREQ(user_input,".") && !term_message) {
	    addch('\n');
	    fprintf(fd,"%s\n",user_input);
	    if (!nonempty && strlen(user_input))
		nonempty = TRUE;
	    *user_input = '\0';
	    if (LYgetstr(user_input, VISIBLE,
			 sizeof(user_input), NORECALL) < 0) {
		HTInfoMsg(NEWS_POST_CANCELLED);
		LYCloseTempFP(fd);		/* Close the temp file. */
		scrollok(stdscr, FALSE);	/* Stop scrolling.	*/
		goto cleanup;
	    }
	}
	fprintf(fd, "\n");
	LYCloseTempFP(fd);		/* Close the temp file. */
	scrollok(stdscr, FALSE);	/* Stop scrolling.	*/
    }

    if (!nonempty) {
	HTAlert(gettext("Message has no original text!"));
	goto cleanup;
    }
    /*
     *  Confirm whether to post, and if so,
     *  whether to append the sig file. - FM
     */
    LYStatusLine = (LYlines - 1);
    c = HTConfirm(POST_MSG_PROMPT);
    LYStatusLine = -1;
    if (c != YES) {
	clear();  /* clear the screen */
	goto cleanup;
    }
    if ((LynxSigFile != NULL) && (fp = fopen(LynxSigFile, "r")) != NULL) {
	char *msg = NULL;
	HTSprintf0(&msg, APPEND_SIG_FILE, LynxSigFile);

	LYStatusLine = (LYlines - 1);
	if (term_message) {
	    _user_message(APPEND_SIG_FILE, LynxSigFile);
	} else if (HTConfirm(msg) == YES) {
	    if ((fd = LYAppendToTxtFile (my_tempfile)) != NULL) {
		char *buffer = NULL;
		fputs("-- \n", fd);
		while (LYSafeGets(&buffer, fp) != NULL) {
		    fputs(buffer, fd);
		}
		fclose(fd);
	    }
	}
	fclose(fp);
	FREE(msg);
	LYStatusLine = -1;
    }
    clear();  /* clear the screen */

    /*
     *  If we are using a Japanese display character
     *  set, convert the contents of the temp file to
     *  JIS (nothing should change if it does not, in
     *  fact, contain EUC or SJIS di-bytes).  Otherwise,
     *  use the temp file as is. - FM
     */
    if (CJKfile[0] != '\0') {
	if ((fd = fopen(my_tempfile, "r")) != NULL) {
	    char *buffer = NULL;
	    while (LYSafeGets(&buffer, fd) != NULL) {
		TO_JIS((unsigned char *)buffer,
		       (unsigned char *)CJKinput);
		fputs(CJKinput, fc);
	    }
	    LYCloseTempFP(fc);
	    StrAllocCopy(postfile, CJKfile);
	    fclose(fd);
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
	 *  If it's not a followup, the current document
	 *  most likely is the group listing, so force a
	 *  to have the article show up in the list after
	 *  the posting.  Note, that if it's a followup
	 *  via a link in a news article, the user must
	 *  do a reload manually on returning to the
	 *  group listing. - FM
	 */
	LYforce_no_cache = TRUE;
    }
    LYStatusLine = (LYlines - 1);
    HTUserMsg(POSTING_TO_NEWS);
    LYStatusLine = -1;

    /*
     *  Come here to cleanup and exit.
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

    return(postfile);
}

PRIVATE void terminate_message ARGS1(
	int,	sig GCC_UNUSED)
{
    term_message = TRUE;
    /*
     *  Reassert the AST.
     */
    signal(SIGINT, terminate_message);
#ifdef VMS
    /*
     *  Refresh the screen to get rid of the "interrupt" message.
     */
    lynx_force_repaint();
    refresh();
#endif /* VMS */
}

#endif /* not DISABLE_NEWS */
