#include <HTUtils.h>
#include <HTAlert.h>
#include <LYUtils.h>
#include <LYStrings.h>
#include <LYBookmark.h>
#include <LYGlobalDefs.h>
#include <LYClean.h>
#include <LYKeymap.h>
#include <LYCharUtils.h> /* need for META charset */
#include <UCAux.h>
#include <LYCharSets.h>  /* need for LYHaveCJKCharacterSet */
#include <LYCurses.h>
#include <GridText.h>

#ifdef VMS
#include <nam.h>
#endif /* VMS */

#include <LYLeaks.h>

PUBLIC char *MBM_A_subbookmark[MBM_V_MAXFILES+1];
PUBLIC char *MBM_A_subdescript[MBM_V_MAXFILES+1];

PRIVATE BOOLEAN is_mosaic_hotlist = FALSE;
PRIVATE char * convert_mosaic_bookmark_file PARAMS((char *filename_buffer));

PRIVATE void
show_bookmark_not_defined NOARGS
{
    char *string_buffer = 0;

    HTSprintf0(&string_buffer,
	    BOOKMARK_FILE_NOT_DEFINED,
	    key_for_func(LYK_OPTIONS));
    LYMBM_statusline(string_buffer);
    FREE(string_buffer);
}

/*
 *  Tries to open a bookmark file for reading, which may be
 *  the default, or based on offering the user a choice from
 *  the MBM_A_subbookmark[] array.  If successful the file is
 *  closed, and the filename in system path specs is returned,
 *  the URL is allocated into *URL, and the MBM_A_subbookmark[]
 *  filepath is allocated into the BookmarkPage global.  Returns
 *  a zero-length pointer to flag a cancel, or a space to flag
 *  an undefined selection, without allocating into *URL or
 *  BookmarkPage.  Returns NULL with allocating into BookmarkPage
 *  but not *URL is the selection is valid but the file doesn't
 *  yet exist. - FM
 */
PUBLIC char * get_bookmark_filename ARGS1(
	char **,	URL)
{
    static char filename_buffer[LY_MAXPATH];
    char *string_buffer = 0;
    FILE *fp;
    int MBM_tmp;

    /*
     *	Multi_Bookmarks support. - FMG & FM
     *	Let user select a bookmark file.
     */
    MBM_tmp = select_multi_bookmarks();
    if (MBM_tmp == -2)
	/*
	 *  Zero-length pointer flags a cancel. - FM
	 */
	return("");
    if (MBM_tmp == -1) {
	show_bookmark_not_defined();
	/*
	 *  Space flags an undefined selection. - FMG
	 */
	return(" ");
    } else {
	/*
	 *  Save the filepath as a global.  The system path will be
	 *  loaded into to the (static) filename_buffer as the return
	 *  value, the URL will be allocated into *URL, and we also
	 *  need the filepath available to calling functions.  This
	 *  is all pitifully non-reentrant, a la the original Lynx,
	 *  and should be redesigned someday. - FM
	 */
	StrAllocCopy(BookmarkPage, MBM_A_subbookmark[MBM_tmp]);
    }

    /*
     *	Seek it in the home path. - FM
     */
    LYAddPathToHome(filename_buffer,
		    sizeof(filename_buffer),
		    BookmarkPage);
    CTRACE(tfp, "\nget_bookmark_filename: SEEKING %s\n   AS %s\n\n",
		BookmarkPage, filename_buffer);
    if ((fp = fopen(filename_buffer,"r")) != NULL) {
	goto success;
    }

    /*
     *	Failure.
     */
    return(NULL);

success:
    /*
     *	We now have the file open.
     *	Check if it is a mosaic hotlist.
     */
    if (LYSafeGets(&string_buffer, fp) != 0
     && !strncmp(string_buffer, "ncsa-xmosaic-hotlist-format-1", 29)) {
	char *newname;
	/*
	 *  It is a mosaic hotlist file.
	 */
	is_mosaic_hotlist = TRUE;
	newname = convert_mosaic_bookmark_file(filename_buffer);
	LYLocalFileToURL(URL, newname);
    } else {
	is_mosaic_hotlist = FALSE;
	LYLocalFileToURL(URL, filename_buffer);
    }
    FREE(string_buffer);
    fclose(fp);

    return(filename_buffer);  /* bookmark file exists */

} /* big end */

/*
 *  Converts a Mosaic hotlist file into an HTML
 *  file for handling as a Lynx bookmark file. - FM
 */
PRIVATE char * convert_mosaic_bookmark_file ARGS1(
	char *,		filename_buffer)
{
    static char newfile[LY_MAXPATH];
    FILE *fp, *nfp;
    char *buf = NULL;
    int line = -2;
    char *endline;

    LYRemoveTemp(newfile);
    if ((nfp = LYOpenTemp(newfile, HTML_SUFFIX, "w")) == NULL) {
	LYMBM_statusline(NO_TEMP_FOR_HOTLIST);
	sleep(AlertSecs);
	return ("");
    }

    if ((fp = fopen(filename_buffer, "r")) == NULL)
	return ("");  /* should always open */

    fprintf(nfp,"<head>\n<title>%s</title>\n</head>\n",MOSAIC_BOOKMARK_TITLE);
    fprintf(nfp, "%s\n\n<p>\n<ol>\n", gettext("\
     This file is an HTML representation of the X Mosaic hotlist file.\n\
     Outdated or invalid links may be removed by using the\n\
     remove bookmark command, it is usually the 'R' key but may have\n\
     been remapped by you or your system administrator."));

    while ((LYSafeGets(&buf, fp)) != NULL) {
	if(line >= 0) {
	    endline = &buf[strlen(buf)-1];
	    if(*endline == '\n')
		*endline = '\0';
	    if((line % 2) == 0) { /* even lines */
		if(*buf != '\0') {
		    strtok(buf," "); /* kill everything after the space */
		    fprintf(nfp,"<LI><a href=\"%s\">",buf); /* the URL */
		}
	    } else { /* odd lines */
		fprintf(nfp,"%s</a>\n",buf);  /* the title */
	    }
	}
	/* else - ignore the line (this gets rid of first two lines) */
	line++;
    }
    LYCloseTempFP(nfp);
    fclose(fp);
    return(newfile);
}

PRIVATE  BOOLEAN havevisible PARAMS((CONST char *Title));
PRIVATE  BOOLEAN have8bit PARAMS((CONST char *Title));
PRIVATE  char* title_convert8bit PARAMS((CONST char *Title));

/*
 *  Adds a link to a bookmark file, creating the file
 *  if it doesn't already exist, and making sure that
 *  no_cache is set for a pre-existing, cached file,
 *  so that the change will be evident on return to
 *  to that file. - FM
 */
PUBLIC void save_bookmark_link ARGS2(
	char *,		address,
	char *,		title)
{
    FILE *fp;
    BOOLEAN first_time = FALSE;
    char *filename;
    char *bookmark_URL = NULL;
    char filename_buffer[LY_MAXPATH];
    char string_buffer[BUFSIZ];
    char *Address = NULL;
    char *Title = NULL;
    int i, c;
    DocAddress WWWDoc;
    HTParentAnchor *tmpanchor;
    HText *text;

    /*
     *	Make sure we were passed something to save. - FM
     */
    if (!(address && *address)) {
	HTAlert(MALFORMED_ADDRESS);
	return;
    }

    /*
     *	Offer a choice of bookmark files,
     *	or get the default. - FMG
     */
    filename = get_bookmark_filename(&bookmark_URL);

    /*
     *	If filename is NULL, must create a new file.  If
     *	filename is a space, an invalid bookmark file was
     *	selected, or if zero-length, the user cancelled.
     *	Ignore request in both cases.  Otherwise, make
     *	a copy before anything might change the static
     *	get_bookmark_filename() buffer. - FM
     */
    if (filename == NULL) {
	first_time = TRUE;
	filename_buffer[0] = '\0';
    } else {
	if (*filename == '\0' || !strcmp(filename," ")) {
	    FREE(bookmark_URL);
	    return;
	}
	strcpy(filename_buffer, filename);
    }

    /*
     *	If BookmarkPage is NULL, something went
     *	wrong, so ignore the request. - FM
     */
    if (BookmarkPage == NULL) {
	FREE(bookmark_URL);
	return;
    }

    /*
     *	If the link will be added to the same
     *	bookmark file, get confirmation. - FM
     */
    if (LYMultiBookmarks == TRUE &&
	strstr(HTLoadedDocumentURL(),
	       (*BookmarkPage == '.' ?
		    (BookmarkPage+1) : BookmarkPage)) != NULL) {
	LYMBM_statusline(MULTIBOOKMARKS_SELF);
	c = LYgetch();
	if (TOUPPER(c) != 'L') {
	    FREE(bookmark_URL);
	    return;
	}
    }

    /*
     *	Allow user to change the title. - FM
     */
    do {
	LYstrncpy(string_buffer, title, sizeof(string_buffer)-1);
	convert_to_spaces(string_buffer, FALSE);
	LYMBM_statusline(TITLE_PROMPT);
	LYgetstr(string_buffer, VISIBLE, sizeof(string_buffer), NORECALL);
	if (*string_buffer == '\0') {
	    LYMBM_statusline(CANCELLED);
	    sleep(MessageSecs);
	    FREE(bookmark_URL);
	    return;
	}
    } while(!havevisible(string_buffer));

    /*
     *	Create the Title with any left-angle-brackets
     *	converted to &lt; entities and any ampersands
     *	converted to &amp; entities.  - FM
     *
     *  Convert 8-bit letters to &#xUUUU to avoid dependencies
     *  from display character set which may need changing.
     *  Do NOT convert any 8-bit chars if we have CJK display. - LP
     */
    StrAllocCopy(Title, string_buffer);
    LYEntify(&Title, TRUE);
    if (UCSaveBookmarksInUnicode &&
	have8bit(Title) && (!LYHaveCJKCharacterSet)) {
	char *p = title_convert8bit(Title);
	FREE(Title);
	Title = p;
    }

    /*
     *	Create the bookmark file, if it doesn't exist already,
     *	Otherwise, open the pre-existing bookmark file. - FM
     */
    SetDefaultMode(O_TEXT);
    if (first_time) {
	/*
	 *  Seek it in the home path. - FM
	 */
	LYAddPathToHome(filename_buffer,
			sizeof(filename_buffer),
			BookmarkPage);
    }
    CTRACE(tfp, "\nsave_bookmark_link: SEEKING %s\n   AS %s\n\n",
		BookmarkPage, filename_buffer);
    if ((fp = fopen(filename_buffer, (first_time ? "w" : "a+"))) == NULL) {
	LYMBM_statusline(BOOKMARK_OPEN_FAILED);
	sleep(AlertSecs);
	FREE(Title);
	FREE(bookmark_URL);
	return;
    }

    /*
     *	Convert all ampersands in the address to &amp; entities. - FM
     */
    StrAllocCopy(Address, address);
    LYEntify(&Address, FALSE);

    /*
     *	If we created a new bookmark file, write the headers. - FM
     *  Once and forever...
     */
    if (first_time) {
	fprintf(fp,"<head>\n");
	LYAddMETAcharsetToFD(fp, -1);
	fprintf(fp,"<title>%s</title>\n</head>\n", BOOKMARK_TITLE);
	fprintf(fp, "%s<br>\n%s\n\n<!--\n%s\n-->\n\n<p>\n<ol>",
		    gettext("\
     You can delete links using the remove bookmark command.  It is usually\n\
     the 'R' key but may have been remapped by you or your system\n\
     administrator."),
		    gettext("\
     This file also may be edited with a standard text editor to delete\n\
     outdated or invalid links, or to change their order."),
		    gettext("\
Note: if you edit this file manually\n\
      you should not change the format within the lines\n\
      or add other HTML markup.\n\
      Make sure any bookmark link is saved as a single line."));
    }

    /*
     *	Add the bookmark link, in Mosaic hotlist or Lynx format. - FM
     */
    if (is_mosaic_hotlist) {
	time_t NowTime = time(NULL);
	char *TimeString = (char *)ctime (&NowTime);
	/*
	 *  TimeString has a \n at the end.
	 */
	fprintf(fp,"%s %s%s\n", Address, TimeString, Title);
    } else {
	fprintf(fp,"<LI><a href=\"%s\">%s</a>\n", Address, Title);
    }
    fclose(fp);

    SetDefaultMode(O_BINARY);
    /*
     *	If this is a cached bookmark file, set nocache for
     *	it so we'll see the new bookmark link when that
     *	cache is retrieved. - FM
     */
    if (!first_time && nhist > 0 && bookmark_URL) {
	for (i = 0; i < nhist; i++) {
	    if (history[i].bookmark &&
		!strcmp(history[i].address, bookmark_URL)) {
		WWWDoc.address = history[i].address;
		WWWDoc.post_data = NULL;
		WWWDoc.post_content_type = NULL;
		WWWDoc.bookmark = history[i].bookmark;
		WWWDoc.isHEAD = FALSE;
		WWWDoc.safe = FALSE;
		if (((tmpanchor = HTAnchor_parent(
					HTAnchor_findAddress(&WWWDoc)
						 )) != NULL) &&
		    (text = (HText *)HTAnchor_document(tmpanchor)) != NULL) {
		    HText_setNoCache(text);
		}
		break;
	    }
	}
    }

    /*
     *	Clean up and report success.
     */
    FREE(Title);
    FREE(Address);
    FREE(bookmark_URL);
    LYMBM_statusline(OPERATION_DONE);
    sleep(MessageSecs);
}

/*
 *  Remove a link from a bookmark file.  The calling
 *  function is expected to have used get_filename_link(),
 *  pass us the link number as cur, the MBM_A_subbookmark[]
 *  string as cur_bookmark_page, and to have set up no_cache
 *  itself. - FM
 */
PUBLIC void remove_bookmark_link ARGS2(
	int,		cur,
	char *,		cur_bookmark_page)
{
    FILE *fp, *nfp;
    char *buf = NULL;
    int n;
#ifdef VMS
    char filename_buffer[NAM$C_MAXRSS+12];
    char newfile[NAM$C_MAXRSS+12];
#else
    char filename_buffer[LY_MAXPATH];
    char newfile[LY_MAXPATH];
    struct stat stat_buf;
    mode_t mode;
#endif /* VMS */
    char homepath[LY_MAXPATH];

    CTRACE(tfp, "remove_bookmark_link: deleting link number: %d\n", cur);

    if (!cur_bookmark_page)
	return;
    LYAddPathToHome(filename_buffer,
		    sizeof(filename_buffer),
		    cur_bookmark_page);
    CTRACE(tfp, "\nremove_bookmark_link: SEEKING %s\n   AS %s\n\n",
		cur_bookmark_page, filename_buffer);
    if ((fp = fopen(filename_buffer, "r")) == NULL) {
	HTAlert(BOOKMARK_OPEN_FAILED_FOR_DEL);
	return;
    }

    LYAddPathToHome(homepath, sizeof(homepath), "");
    if ((nfp = LYOpenScratch(newfile, homepath)) == 0) {
	fclose(fp);
	HTAlert(BOOKSCRA_OPEN_FAILED_FOR_DEL);
	return;
    }

#ifdef UNIX
    /*
     *	Explicitly preserve bookmark file mode on Unix. - DSL
     */
    if (stat(filename_buffer, &stat_buf) == 0) {
	mode = ((stat_buf.st_mode & 0777) | 0600); /* make it writable */
	(void) chmod(newfile, mode);
	if ((nfp = LYReopenTemp(newfile)) == NULL) {
	    (void) fclose(fp);
	    HTAlert(BOOKTEMP_REOPEN_FAIL_FOR_DEL);
	    return;
	}
    }
#endif /* UNIX */

    if (is_mosaic_hotlist) {
	int del_line = cur*2;  /* two lines per entry */
	n = -3;  /* skip past cookie and name lines */
	while (LYSafeGets(&buf, fp) != NULL) {
	    n++;
	    if (n == del_line || n == del_line+1)
		continue;  /* remove two lines */
	    if (fputs(buf, nfp) == EOF)
		goto failure;
	}

    } else {
	char *cp;
	BOOLEAN retain;
	int seen;

	n = -1;
	while (LYSafeGets(&buf, fp) != NULL) {
	    retain = TRUE;
	    seen = 0;
	    cp = buf;
	    while (n < cur && (cp = LYstrstr(cp, "<a href="))) {
		seen++;
		if (++n == cur) {
		    if (seen != 1 || !LYstrstr(buf, "</a>") ||
			LYstrstr((cp + 1), "<a href=")) {
			HTAlert(BOOKMARK_LINK_NOT_ONE_LINE);
			goto failure;
		    }
		    CTRACE(tfp, "remove_bookmark_link: skipping link %d\n", n);
		    retain = FALSE;
		}
		cp += 8;
	    }
	    if (retain && fputs(buf, nfp) == EOF)
		goto failure;
	}
    }

    CTRACE(tfp, "remove_bookmark_link: files: %s %s\n",
			newfile, filename_buffer);

    fclose(fp);
    fp = NULL;
    LYCloseTempFP(nfp);
    nfp = NULL;
#ifdef DOSPATH
    remove(filename_buffer);
#endif /* DOSPATH */

#ifdef UNIX
    /*
     *	By copying onto the bookmark file, rather than renaming it, we
     *	can preserve the original ownership of the file, provided that
     *	it is writable by the current process.
     *	Changed to copy  1998-04-26 -- gil
     */
    if (LYCopyFile(newfile, filename_buffer) == 0)
	return;
    HTAlert(BOOKTEMP_COPY_FAIL);
#else  /* !UNIX */
    if (rename(newfile, filename_buffer) != -1) {
	HTSYS_purge(filename_buffer);
	return;
    } else {
#ifndef VMS
	/*
	 *  Rename won't work across file systems.
	 *  Check if this is the case and do something appropriate.
	 *  Used to be ODD_RENAME
	 */
#ifdef _WINDOWS
	if (errno == ENOTSAM)
#else
	if (errno == EXDEV)
#endif /* WINDOWS */
	{
	    static CONST char MV_FMT[] = "%s %s %s";
	    char *buffer = 0;
	    HTAddParam(&buffer, MV_FMT, 1, MV_PATH);
	    HTAddParam(&buffer, MV_FMT, 2, newfile);
	    HTAddParam(&buffer, MV_FMT, 3, filename_buffer);
	    HTEndParam(&buffer, MV_FMT, 3);
	    LYSystem(buffer);
	    FREE(buffer);
	    return;
	}
#endif /* !VMS */

#ifdef VMS
	HTAlert(ERROR_RENAMING_SCRA);
#else
	HTAlert(ERROR_RENAMING_TEMP);
#endif /* VMS */
	if (TRACE)
	    perror("renaming the file");
    }
#endif /* UNIX */

failure:
    FREE(buf);
    HTAlert(BOOKMARK_DEL_FAILED);
    LYCloseTempFP(nfp);
    if (fp != NULL)
	fclose(fp);
    LYRemoveTemp(newfile);
}

/*
 *  Allows user to select sub-bookmarks files. - FMG & FM
 */
PUBLIC int select_multi_bookmarks NOARGS
{
    int c;

    /*
     *	If not enabled, pick the "default" (0).
     */
    if (LYMultiBookmarks == FALSE || LYHaveSubBookmarks() == FALSE) {
	if (MBM_A_subbookmark[0]) /* If it exists! */
	    return(0);
	else
	    return(-1);
    }

    /*
     *	For ADVANCED users, we can just mess with the status line to save
     *	the 2 redraws of the screen, if LYMBMAdvnced is TRUE.  '=' will
     *	still show the screen and let them do it the "long" way.
     */
    if (LYMBMAdvanced && user_mode == ADVANCED_MODE) {
	LYMBM_statusline(MULTIBOOKMARKS_SELECT);
get_advanced_choice:
	c = LYgetch();
#ifdef VMS
	if (HadVMSInterrupt) {
	    HadVMSInterrupt = FALSE;
	    c = 7;
	}
#endif /* VMS */
	if (LYisNonAlnumKeyname(c, LYK_PREV_DOC) ||
	    c == 7 || c == 3) {
	    /*
	     *	Treat left-arrow, ^G, or ^C as cancel.
	     */
	    return(-2);
	}
	if (LYisNonAlnumKeyname(c, LYK_REFRESH)) {
	    /*
	     *	Refresh the screen.
	     */
	    lynx_force_repaint();
	    refresh();
	    goto get_advanced_choice;
	}
	if (LYisNonAlnumKeyname(c, LYK_ACTIVATE)) {
	    /*
	     *	Assume default bookmark file on ENTER or right-arrow.
	     */
	    return (MBM_A_subbookmark[0] ? 0 : -1);
	}
	switch (c) {
	    case '=':
		/*
		 *  Get the choice via the menu.
		 */
		return(select_menu_multi_bookmarks());

	    default:
		/*
		 *  Convert to an array index, act on it if valid.
		 *  Otherwise, get another keystroke.
		 */
		c = TOUPPER(c) - 'A';
		if (c < 0 || c > MBM_V_MAXFILES) {
		    goto get_advanced_choice;
		}
	}
	/*
	 *  See if we have a bookmark like that.
	 */
	return (MBM_A_subbookmark[c] ? c : -1);
    } else {
	/*
	 *  Get the choice via the menu.
	 */
	return(select_menu_multi_bookmarks());
    }
}

/*
 *  Allows user to select sub-bookmarks files. - FMG & FM
 */
PUBLIC int select_menu_multi_bookmarks NOARGS
{
    int c, MBM_tmp_count, MBM_allow;
    int MBM_screens, MBM_from, MBM_to, MBM_current;

    /*
     *	If not enabled, pick the "default" (0).
     */
    if (LYMultiBookmarks == FALSE)
	return(0);

    /*
     *	Filip M. Gieszczykiewicz (filipg@paranoia.com) & FM
     *	---------------------------------------------------
     *	LYMultiBookmarks - TRUE when multi_support enabled.
     *
     *	MBM_A_subbookmark[n] - Hold values of the respective
     *	"multi_bookmarkn" in the lynxrc file.
     *
     *	MBM_A_subdescript[n] - Hold description entries in the
     *	lynxrc file.
     *
     *	Note: MBM_A_subbookmark[0] is defined to be same value as
     *	      "bookmark_file" in the lynxrc file and/or the startup
     *	      "bookmark_page".
     *
     *	We make the display of bookmarks depend on rows we have
     *	available.
     *
     *	We load BookmarkPage with the valid MBM_A_subbookmark[n]
     *	via get_bookmark_filename().  Otherwise, that function
     *	returns a zero-length string to indicate a cancel, a
     *	single space to indicate an invalid choice, or NULL to
     *	indicate an inaccessible file.
     */
    MBM_allow=(LYlines-7);	/* We need 7 for header and footer */
    /*
     *	Screen big enough?
     */
    if (MBM_allow <= 0) {
	/*
	 *  Too small.
	 */
	HTAlert(MULTIBOOKMARKS_SMALL);
	return (-2);
    }

    MBM_screens = (MBM_V_MAXFILES/MBM_allow)+1; /* int rounds off low. */

    MBM_current = 1; /* Gotta start somewhere :-) */

draw_bookmark_choices:
    MBM_from = MBM_allow * MBM_current - MBM_allow;
    if (MBM_from < 0)
	MBM_from = 0; /* 0 is default bookmark... */
    if (MBM_current != 1)
	MBM_from++;

    MBM_to = (MBM_allow * MBM_current);
    if (MBM_to > MBM_V_MAXFILES)
	MBM_to = MBM_V_MAXFILES;

    /*
     *	Display menu of bookmarks.  NOTE that we avoid printw()'s
     *	to increase the chances that any non-ASCII or multibyte/CJK
     *	characters will be handled properly. - FM
     */
    clear();
    move(1, 5);
    lynx_start_h1_color ();
    if (MBM_screens > 1) {
	char *shead_buffer = 0;
	HTSprintf0(&shead_buffer,
		MULTIBOOKMARKS_SHEAD_MASK, MBM_current, MBM_screens);
	addstr(shead_buffer);
	FREE(shead_buffer);
    } else {
	addstr(MULTIBOOKMARKS_SHEAD);
    }

    lynx_stop_h1_color ();

    MBM_tmp_count = 0;
    for (c = MBM_from; c <= MBM_to; c++) {
	move(3+MBM_tmp_count, 5);
	addch((unsigned char)(c + 'A'));
	addstr(" : ");
	if (MBM_A_subdescript[c])
	    addstr(MBM_A_subdescript[c]);
	move(3+MBM_tmp_count,36);
	addch('(');
	if (MBM_A_subbookmark[c])
	    addstr(MBM_A_subbookmark[c]);
	addch(')');
	MBM_tmp_count++;
    }

    /*
     *	Don't need to show it if it all fits on one screen!
     */
    if (MBM_screens > 1) {
	move(LYlines-2, 0);
	addstr("'");
	standout();
	addstr("[");
	standend();
	addstr("' ");
	addstr(PREVIOUS);
	addstr(", '");
	standout();
	addstr("]");
	standend();
	addstr("' ");
	addstr(NEXT_SCREEN);
    }

    LYMBM_statusline(MULTIBOOKMARKS_SAVE);
get_bookmark_choice:
    c = LYgetch();
#ifdef VMS
    if (HadVMSInterrupt) {
	HadVMSInterrupt = FALSE;
	c = 7;
    }
#endif /* VMS */

    if (LYisNonAlnumKeyname(c, LYK_PREV_DOC) ||
	c == 7 || c == 3) {
	/*
	 *  Treat left-arrow, ^G, or ^C as cancel.
	 */
	return(-2);
    }

    if (LYisNonAlnumKeyname(c, LYK_REFRESH)) {
	/*
	 *  Refresh the screen.
	 */
	lynx_force_repaint();
	refresh();
	goto get_bookmark_choice;
    }

    if (LYisNonAlnumKeyname(c, LYK_ACTIVATE)) {
	/*
	 *  Assume default bookmark file on ENTER or right-arrow.
	 */
	return(MBM_A_subbookmark[0] ? 0 : -1);
    }

    /*
     *	Next range, if available.
     */
    if ((c == ']' ||  LYisNonAlnumKeyname(c, LYK_NEXT_PAGE)) &&
	MBM_screens > 1) {
	if (++MBM_current > MBM_screens)
	    MBM_current = 1;
	goto draw_bookmark_choices;
    }

    /*
     *	Previous range, if available.
     */
    if ((c == '[' ||  LYisNonAlnumKeyname(c, LYK_PREV_PAGE)) &&
	MBM_screens > 1) {
	if (--MBM_current <= 0)
	    MBM_current = MBM_screens;
	goto draw_bookmark_choices;
    }

    c = TOUPPER(c) - 'A';
    /*
     *	See if we have a bookmark like that.
     */
    if (c < 0 || c > MBM_V_MAXFILES) {
	goto get_bookmark_choice;
    } else if (!MBM_A_subbookmark[c]) {
	show_bookmark_not_defined();
	LYMBM_statusline(MULTIBOOKMARKS_SAVE);
	goto get_bookmark_choice;
    } else {
	return(c);
    }
}

/*
 *  This function returns TRUE if we have sub-bookmarks defined.
 *  Otherwise (i.e., only the default bookmark file is defined),
 *  it returns FALSE. - FM
 */
PUBLIC BOOLEAN LYHaveSubBookmarks NOARGS
{
    int i;

    for (i = 1; i < MBM_V_MAXFILES; i++) {
	if (MBM_A_subbookmark[i] != NULL && *MBM_A_subbookmark[i] != '\0')
	    return(TRUE);
    }

    return(FALSE);
}

/*
 *  This function passes a string to _statusline(), making
 *  sure it is at the bottom of the screen if LYMultiBookmarks
 *  is TRUE, otherwise, letting it go to the normal statusline
 *  position based on the current user mode.  We want to use
 *  _statusline() so that any multibyte/CJK characters in the
 *  string will be handled properly. - FM
 */
PUBLIC void LYMBM_statusline  ARGS1(
	char *,		text)
{
    if (LYMultiBookmarks == TRUE && user_mode == NOVICE_MODE) {
	LYStatusLine = (LYlines - 1);
	_statusline(text);
	LYStatusLine = -1;
    } else {
	_statusline(text);
    }
}

/*
 * Check whether we have any visible (non-blank) chars.
 */
PRIVATE  BOOLEAN havevisible ARGS1(CONST char *, Title)
{
    CONST char *p = Title;
    unsigned char c;
    long unicode;

    for ( ; *p; p++) {
	c = (unsigned char)(TOASCII(*p));
	if (c > 32 && c < 127)
	    return(TRUE);
	if (c <= 32 || c == 127)
	    continue;
	if (LYHaveCJKCharacterSet || !UCCanUniTranslateFrom(current_char_set))
	    return(TRUE);
	unicode = UCTransToUni(*p, current_char_set);
	if (unicode > 32 && unicode < 127)
	    return(TRUE);
	if (unicode <= 32 || unicode == 0xa0 || unicode == 0xad)
	    continue;
	if (unicode >= 0x2000 && unicode < 0x200f)
	    continue;
	return(TRUE);
    }
    return(FALSE); /* if we came here */
}

/*
 * Check whether string have 8 bit chars.
 */
PRIVATE  BOOLEAN have8bit ARGS1(CONST char *, Title)
{
    CONST char *p = Title;

    for ( ; *p; p++) {
	if ((unsigned char)*p > 127)
	return(TRUE);
    }
    return(FALSE); /* if we came here */
}

/*
 *  Ok, title have 8-bit characters and they are in display charset.
 *  Bookmarks is a permanent file.  To avoid dependencies from display
 *  character set which may be changed with time
 *  we store 8-bit characters as numeric character reference (NCR),
 *  so where the character encoded as unicode number in form of &#xUUUU;
 *
 *  To make bookmarks more readable for human (&#xUUUU certainly not)
 *  we add a comment with '7-bit approximation' from the converted string.
 *  This is a valid HTML and bookmarks code.
 *
 *  We do not want use META charset tag in bookmarks file:
 *  it will never be changed later :-(
 *  NCR's translation is part of I18N and HTML4.0
 *  supported starting with Lynx 2.7.2,
 *  Netscape 4.0 and MSIE 4.0.
 *  Older versions fail.
 *
 */
PRIVATE  char* title_convert8bit ARGS1(CONST char *, Title)
{
    CONST char *p = Title;
    char *p0;
    char *q;
    char *comment = NULL;
    char *ncr     = NULL;
    char *buf = NULL;
    int charset_in  = current_char_set;
    int charset_out = UCGetLYhndl_byMIME("us-ascii");

    for ( ; *p; p++) {
	char temp[2];
	LYstrncpy(temp, p, sizeof(temp)-1);
	if ((unsigned char)*temp <= 127) {
	    StrAllocCat(comment, temp);
	    StrAllocCat(ncr, temp);
	} else {
	    long unicode;
	    char replace_buf [32];

	    if (UCTransCharStr(replace_buf, sizeof(replace_buf), *temp,
				  charset_in, charset_out, YES) > 0)
		StrAllocCat(comment, replace_buf);

	    unicode = UCTransToUni( *temp, charset_in);

	    StrAllocCat(ncr, "&#");
	    sprintf(replace_buf, "%ld", unicode);
	    StrAllocCat(ncr, replace_buf);
	    StrAllocCat(ncr, ";");
	}
    }

    /*
     *  Cleanup comment, collapse multiple dashes into one dash,
     *  skip '>'.
     */
    for (q = p0 = comment; *p0; p0++) {
	if ((unsigned char)(TOASCII(*p0)) >= 32 &&
	    *p0 != '>' &&
	    (q == comment || *p0 != '-' || *(q-1) != '-')) {
	    *q++ = *p0;
	}
    }
    *q = '\0';

    /*
     * valid bookmark should be a single line (no linebreaks!).
     */
    StrAllocCat(buf, "<!-- ");
    StrAllocCat(buf, comment);
    StrAllocCat(buf, " -->");
    StrAllocCat(buf, ncr);

    FREE(comment);
    FREE(ncr);
    return(buf);
}
