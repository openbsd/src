#include "HTUtils.h"
#include "tcp.h"
#include "HTParse.h"
#include "HTAccess.h"
#include "HTCJK.h"
#include "HTAlert.h"
#include "LYCurses.h"
#include "LYSignal.h"
#include "LYStructs.h"
#include "LYUtils.h"
#include "LYClean.h"
#include "LYStrings.h"
#include "LYGetFile.h"
#include "LYHistory.h"
#include "LYSystem.h"
#include "GridText.h"
#include "LYCharSets.h"
#include "LYNews.h"

#include "LYGlobalDefs.h"

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

/*
**  Global variable for async i/o.
*/
BOOLEAN term_message = FALSE;
PRIVATE void terminate_message  PARAMS((int sig));

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
    int c = 0;  /* user input */
    FILE *fd = NULL;
    char my_tempfile[256];
    FILE *fc = NULL;
    char CJKfile[256];
    char *postfile = NULL;
    char *NewsGroups = NULL;
    char *org = NULL;
    FILE *fp = NULL;

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
    tempname(my_tempfile, NEW_FILE);
    if ((fd = LYNewTxtFile(my_tempfile)) == NULL) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(postfile);
    }

    /*
     *  If we're using a Japanese display character set,
     *  open a temporary file for a conversion to JIS. - FM
     */
    CJKfile[0] = '\0';
    if (!strncmp(LYchar_set_names[current_char_set], "Japanese (EUC)", 14) ||
	!strncmp(LYchar_set_names[current_char_set], "Japanese (SJIS)", 15)) {
	tempname(CJKfile, NEW_FILE);
	if ((fc = LYNewTxtFile(CJKfile)) == NULL) {
	    HTAlert(CANNOT_OPEN_TEMP);
	    fclose(fd);
#ifdef VMS
	    while (remove(my_tempfile) == 0)
		; /* loop through all versions */
#else
	    remove(my_tempfile);
#endif /* VMS */
	    return(postfile);
	}
    }

    /*
     *  The newsgroups could be a comma-seperated list.
     *  It need not have spaces, but deal with any that
     *  may also have been hex escaped. - FM
     */
    StrAllocCopy(NewsGroups, newsgroups);
    HTUnEscape(NewsGroups);

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
    addstr("You will be posting to:");
    addstr("\n\t");
    addstr(NewsGroups);
    addch('\n');

    /*
     *  Get the mail address for the From header,
     *  offering personal_mail_address as default.
     */
    addstr("\n\n Please provide your mail address for the From: header\n");
    strcpy(user_input, "From: ");
    if (personal_mail_address)
	strcat(user_input, personal_mail_address);
    if (LYgetstr(user_input, VISIBLE,
		 sizeof(user_input), NORECALL) < 0 ||
	term_message) {
        _statusline(NEWS_POST_CANCELLED);
	sleep(InfoSecs);
	fclose(fd);		 /* Close the temp file. */
	scrollok(stdscr, FALSE); /* Stop scrolling.	 */
	goto cleanup;
    }
    fprintf(fd, "%s\n", user_input);

    /*
     *  Get the Subject header, offering the current
     *  document's title as the default if this is a
     *  followup rather than a new post. - FM
     */
    addstr("\n\n Please provide or edit the Subject: header\n");
    strcpy(user_input, "Subject: ");
    if ((followup == TRUE && nhist > 0) &&
        (cp = HText_getTitle()) != NULL) {
	/*
	 *  Add the default subject.
	 */
	while (isspace(*cp)) {
	    cp++;
	}
	if (strncasecomp(cp, "Re:", 3)) {
            strcat(user_input, "Re: ");
	}
        strcat(user_input, cp);
    }
    cp = NULL;
    if (LYgetstr(user_input, VISIBLE,
		 sizeof(user_input), NORECALL) < 0 ||
	term_message) {
        _statusline(NEWS_POST_CANCELLED);
        sleep(InfoSecs);
        fclose(fd);		 /* Close the temp file. */
	scrollok(stdscr, FALSE); /* Stop scrolling.	 */
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
	if (fgets(user_input, sizeof(user_input), fp) != NULL) {
	    if ((org = strchr(user_input, '\n')) != NULL) {
	        *org = '\0';
	    }
	    if (user_input[0] != '\0') {
	        StrAllocCat(cp, user_input);
	    }
	}
	fclose(fp);
#endif /* !VMS */
    }
    LYstrncpy(user_input, cp, (sizeof(user_input) - 16));
    FREE(cp); 
    addstr("\n\n Please provide or edit the Organization: header\n");
    if (LYgetstr(user_input, VISIBLE,
		 sizeof(user_input), NORECALL) < 0 ||
	term_message) {
        _statusline(NEWS_POST_CANCELLED);
        sleep(InfoSecs);
        fclose(fd);		 /* Close the temp file. */
	scrollok(stdscr, FALSE); /* Stop scrolling.	 */
        goto cleanup;
    }
    fprintf(fd, "%s\n", user_input);

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
	    _statusline(INC_ORIG_MSG_PROMPT);
	    c = 0;
	    while (TOUPPER(c) != 'Y' && TOUPPER(c) != 'N' &&
	    	   !term_message && c != 7 && c != 3)
	        c = LYgetch();
	    if (TOUPPER(c) == 'Y')
	        /*
		 *  The 1 will add the reply ">" in front of every line.
		 *  We're assuming that if the display character set is
		 *  Japanese and the document did not have a CJK charset,
		 *  any non-EUC or non-SJIS 8-bit characters in it where
		 *  converted to 7-bit equivalents. - FM
		 */
	        print_wwwfile_to_fd(fd, 1);
	}
	fclose(fd);		 /* Close the temp file. */
	scrollok(stdscr, FALSE); /* Stop scrolling.	 */
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
	if (system(user_input)) {
	    start_curses();
	    _statusline(ERROR_SPAWNING_EDITOR);
	    sleep(AlertSecs);
	} else {
	    start_curses();
	}
    } else {
        /*
	 *  Use the built in line editior.
	 */
	addstr("\n\n Please enter your message below.");
	addstr("\n When you are done, press enter and put a single period (.)");
	addstr("\n on a line and press enter again.");
	addstr("\n\n");
	refresh();
        *user_input = '\0';
	if (LYgetstr(user_input, VISIBLE,
	    	     sizeof(user_input), NORECALL) < 0 ||
	    term_message) {
	    _statusline(NEWS_POST_CANCELLED);
	    sleep(InfoSecs);
	    fclose(fd);			/* Close the temp file.	*/
	    scrollok(stdscr, FALSE);	/* Stop scrolling.	*/
	    goto cleanup;
	}
	while (!STREQ(user_input,".") && !term_message) { 
	    addch('\n');
	    fprintf(fd,"%s\n",user_input);
	    *user_input = '\0';
	    if (LYgetstr(user_input, VISIBLE,
	       		 sizeof(user_input), NORECALL) < 0) {
	        _statusline(NEWS_POST_CANCELLED);
	        sleep(InfoSecs);
	        fclose(fd);		 /* Close the temp file. */
		scrollok(stdscr, FALSE); /* Stop scrolling.	 */
	        goto cleanup;
	    }
 	}
	fprintf(fd, "\n");
	fclose(fd);		 /* Close the temp file. */
	scrollok(stdscr, FALSE); /* Stop scrolling.	 */
    }

    /*
     *  Confirm whether to post, and if so,
     *  whether to append the sig file. - FM
     */
    LYStatusLine = (LYlines - 1);
    _statusline(POST_MSG_PROMPT);
    c = 0;
    LYStatusLine = -1;
    while (TOUPPER(c) != 'Y' && TOUPPER(c) != 'N' &&
	   !term_message && c != 7   && c != 3)
	c = LYgetch();
    if (TOUPPER(c) != 'Y') {
        clear();  /* clear the screen */
	goto cleanup;
    }
    if ((LynxSigFile != NULL) &&
        (fp = fopen(LynxSigFile, "r")) != NULL) {
	LYStatusLine = (LYlines - 1);
	_user_message(APPEND_SIG_FILE, LynxSigFile);
	c = 0;
        LYStatusLine = -1;
	while (TOUPPER(c) != 'Y' && TOUPPER(c) != 'N' &&
	       !term_message && c != 7   && c != 3)
	    c = LYgetch();
	if (TOUPPER(c) == 'Y') {
	    if ((fd = fopen(my_tempfile, "a")) != NULL) {
	        fputs("-- \n", fd);
	        while (fgets(user_input, sizeof(user_input), fp) != NULL) {
		    fputs(user_input, fd);
		}
		fclose(fd);
	    }
	}
	fclose(fp);
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
	    while (fgets(user_input, sizeof(user_input), fd) != NULL) {
	        TO_JIS((unsigned char *)user_input,
		       (unsigned char *)CJKinput);
		fputs(CJKinput, fc);
	    }
	    fclose(fc);
	    StrAllocCopy(postfile, CJKfile);
	    fclose(fd);
#ifdef VMS
	    while (remove(my_tempfile) == 0)
		; /* loop through all versions */
#else
	    remove(my_tempfile);
#endif /* VMS */
	    fd = fc;
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
    statusline(POSTING_TO_NEWS);
    LYStatusLine = -1;
    sleep(MessageSecs);

    /*
     *  Come here to cleanup and exit.
     */
cleanup:
#ifndef VMS
    signal(SIGINT, cleanup_sig);
#endif /* !VMS */
    term_message = FALSE;
    if (!postfile) {
#ifdef VMS
        while (remove(my_tempfile) == 0)
	    ; /* loop through all versions */
#else
	remove(my_tempfile);
#endif /* VMS */
    }
    if (CJKfile[0] != '\0') {
#ifdef VMS
	fclose(fc);
        while (remove(CJKfile) == 0)
	    ; /* loop through all versions */
#else
	remove(CJKfile);
#endif /* VMS */
    }
    FREE(NewsGroups);

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
