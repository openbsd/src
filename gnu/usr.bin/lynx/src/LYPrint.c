#include "HTUtils.h"
#include "tcp.h"
#include "HTAccess.h"
#include "HTList.h"
#include "HTAlert.h"
#include "HTFile.h"
#include "LYCurses.h"
#include "GridText.h"
#include "LYUtils.h"
#include "LYPrint.h"
#include "LYGlobalDefs.h"
#include "LYSignal.h"
#include "LYStrings.h"
#include "LYClean.h"
#include "LYGetFile.h"
#include "LYHistory.h"
#include "LYSystem.h"
#include "LYList.h"
#include "LYCharSets.h"  /* To get current charset for mail header. */
#ifdef VMS
#include "HTVMSUtils.h"
#endif /* VMS */
#ifdef DOSPATH
#include "HTDOS.h"
#endif

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

/*
 *  printfile prints out the current file minus the links and targets
 *  to a veriaty of places
 */

/* it parses an incoming link that looks like
 *
 *  LYNXPRINT://LOCAL_FILE/lines=##
 *  LYNXPRINT://MAIL_FILE/lines=##
 *  LYNXPRINT://TO_SCREEN/lines=##
 *  LYNXPRINT://PRINTER/lines=##/number=#
 */

#define TO_FILE   1
#define TO_SCREEN 2
#define MAIL	  3
#define PRINTER   4

#ifdef VMS
PRIVATE int remove_quotes PARAMS((char *string));
#endif /* VMS */

PUBLIC int printfile ARGS1(
	document *,	newdoc)
{
    static char tempfile[256];
    static BOOLEAN first = TRUE;
    char buffer[LINESIZE];
    char filename[LINESIZE];
    char user_response[256];
    int lines_in_file = 0;
    int printer_number = 0;
    int pages = 0;
    int type = 0, c, len;
    FILE *outfile_fp;
    char *cp = NULL;
    lynx_printer_item_type *cur_printer;
    char *sug_filename = NULL;
    char *link_info = NULL;
    DocAddress WWWDoc;
    int pagelen = 0;
    int ch, recall;
    int FnameTotal;
    int FnameNum;
    BOOLEAN FirstRecall = TRUE;
    char *content_base = NULL, *content_location = NULL;
    HTFormat format;
    HTAtom *encoding;
    BOOL use_mime, use_cte, use_type;
    CONST char *disp_charset;
    static BOOLEAN first_mail_preparsed = TRUE;
    char *envbuffer = NULL;
#ifdef VMS
    BOOLEAN isPMDF = FALSE;
    char hdrfile[256];
    FILE *hfd;

    if (!strncasecomp(system_mail, "PMDF SEND", 9)) {
	isPMDF = TRUE;
    }
#endif /* VMS */

    /*
     *	Extract useful info from URL.
     */
    StrAllocCopy(link_info, newdoc->address+12);

    /*
     *	Reload the file we want to print into memory.
     */
    LYpop(newdoc);
    WWWDoc.address = newdoc->address;
    WWWDoc.post_data = newdoc->post_data;
    WWWDoc.post_content_type = newdoc->post_content_type;
    WWWDoc.bookmark = newdoc->bookmark;
    WWWDoc.isHEAD = newdoc->isHEAD;
    WWWDoc.safe = newdoc->safe;
    if (!HTLoadAbsolute(&WWWDoc))
	return(NOT_FOUND);

    /*
     *	If we have an explicit content-base, we may use it even
     *	if not in source mode. - kw
     */
    if (HText_getContentBase()) {
	StrAllocCopy(content_base, HText_getContentBase());
	collapse_spaces(content_base);
	if (!(content_base && *content_base)) {
	    FREE(content_base);
	}
    }
    /*
     *	If document is source, load the content_base
     *	and content_location strings. - FM
     */
    if (HTisDocumentSource()) {
	if (HText_getContentLocation()) {
	    StrAllocCopy(content_location, HText_getContentLocation());
	    collapse_spaces(content_location);
	    if (!(content_location && *content_location)) {
		FREE(content_location);
	    }
	}
	if (!content_base) {
	    if ((content_location) && is_url(content_location)) {
		StrAllocCopy(content_base, content_location);
	    } else {
		StrAllocCopy(content_base, newdoc->address);
	    }
	}
	if (!content_location) {
	    StrAllocCopy(content_location, newdoc->address);
	}
    }

    /*
     *	Load the suggested filename string. - FM
     */
    if (HText_getSugFname() != NULL)
	StrAllocCopy(sug_filename, HText_getSugFname()); /* must be freed */
    else
	StrAllocCopy(sug_filename, newdoc->address); /* must be freed */
    /*
     *	Strip any gzip or compress suffix, if present. - FM
     */
    cp = NULL;
    if (strlen(sug_filename) > 3) {
	cp = (char *)&sug_filename[(strlen(sug_filename) - 3)];
	if ((*cp == '.' || *cp == '-' || *cp == '_') &&
	    !strcasecomp((cp + 1), "gz")) {
	    *cp = '\0';
	} else {
	    cp = NULL;
	}
    }
    if ((cp == NULL) && strlen(sug_filename) > 2) {
	cp = (char *)&sug_filename[(strlen(sug_filename) - 2)];
	if ((*cp == '.' || *cp == '-' || *cp == '_') &&
	    !strcasecomp((cp + 1), "Z")) {
	    *cp = '\0';
	}
    }
    cp = NULL;

    /*
     *	Get the number of lines in the file.
     */
    if ((cp = (char *)strstr(link_info, "lines=")) != NULL) {
	/*
	 *  Terminate prev string here.
	 */
	*cp = '\0';
	/*
	 *  Number of characters in "lines=".
	 */
	cp += 6;

	lines_in_file = atoi(cp);
	pages = lines_in_file/66;
    }

    /*
     *	Determine the type.
     */
    if (strstr(link_info, "LOCAL_FILE")) {
	type = TO_FILE;
    } else if (strstr(link_info, "TO_SCREEN")) {
	type = TO_SCREEN;
    } else if (strstr(link_info, "MAIL_FILE")) {
	type = MAIL;
    } else if (strstr(link_info, "PRINTER")) {
	type = PRINTER;

	if ((cp = (char *)strstr(link_info, "number=")) != NULL) {
	    /* number of characters in "number=" */
	    cp += 7;
	    printer_number = atoi(cp);
	}
	if ((cp = (char *)strstr(link_info, "pagelen=")) != NULL) {
	    /* number of characters in "pagelen=" */
	    cp += 8;
	    pagelen = atoi(cp);
	} else {
	    /* default to 66 lines */
	    pagelen = 66;
	}
    }

    /*
     *	Set up the sug_filenames recall buffer.
     */
    FnameTotal = (sug_filenames ? HTList_count(sug_filenames) : 0);
    recall = ((FnameTotal >= 1) ? RECALL : NORECALL);
    FnameNum = FnameTotal;

    /*
     *	Act on the request. - FM
     */
    switch (type) {
	case TO_FILE:
#if defined(__DJGPP__) || defined(_WINDOWS)
		_fmode = O_TEXT;
#endif /* __DJGPP__  or _WINDOWS */
		_statusline(FILENAME_PROMPT);
	retry:	strcpy(filename, sug_filename);  /* add suggestion info */
		/* make the sug_filename conform to system specs */
		change_sug_filename(filename);
		if (!(HTisDocumentSource()) &&
		    (cp = strrchr(filename, '.')) != NULL) {
		    format = HTFileFormat(filename, &encoding, NULL);
		    if (!strcasecomp(format->name, "text/html") ||
			!IsUnityEnc(encoding)) {
			*cp = '\0';
			strcat(filename, ".txt");
		    }
		}
		if (lynx_save_space && *lynx_save_space) {
		    strcpy(buffer, lynx_save_space);
		    strcat(buffer, filename);
		    strcpy(filename, buffer);
		}
	check_recall:
		if ((ch = LYgetstr(filename, VISIBLE,
				   sizeof(filename), recall)) < 0 ||
		    *filename == '\0' || ch == UPARROW || ch == DNARROW) {
		    if (recall && ch == UPARROW) {
			if (FirstRecall) {
			    FirstRecall = FALSE;
			    /*
			     *	Use the last Fname in the list. - FM
			     */
			    FnameNum = 0;
			} else {
			    /*
			     *	Go back to the previous Fname
			     *	in the list. - FM
			     */
			    FnameNum++;
			}
			if (FnameNum >= FnameTotal) {
			    /*
			     *	Reset the FirstRecall flag,
			     *	and use sug_file or a blank. - FM
			     */
			    FirstRecall = TRUE;
			    FnameNum = FnameTotal;
			    _statusline(FILENAME_PROMPT);
			    goto retry;
			} else if ((cp = (char *)HTList_objectAt(
							sug_filenames,
							FnameNum)) != NULL) {
			    strcpy(filename, cp);
			    if (FnameTotal == 1) {
				_statusline(EDIT_THE_PREV_FILENAME);
			    } else {
				_statusline(EDIT_A_PREV_FILENAME);
			    }
			    goto check_recall;
			}
		    } else if (recall && ch == DNARROW) {
			if (FirstRecall) {
			    FirstRecall = FALSE;
			    /*
			     * Use the first Fname in the list. - FM
			     */
			    FnameNum = FnameTotal - 1;
			} else {
			    /*
			     * Advance to the next Fname in the list. - FM
			     */
			    FnameNum--;
			}
			if (FnameNum < 0) {
			    /*
			     *	Set the FirstRecall flag,
			     *	and use sug_file or a blank. - FM
			     */
			    FirstRecall = TRUE;
			    FnameNum = FnameTotal;
			    _statusline(FILENAME_PROMPT);
			    goto retry;
			} else if ((cp = (char *)HTList_objectAt(
							sug_filenames,
							FnameNum)) != NULL) {
			    strcpy(filename, cp);
			    if (FnameTotal == 1) {
				_statusline(EDIT_THE_PREV_FILENAME);
			    } else {
				_statusline(EDIT_A_PREV_FILENAME);
			    }
			    goto check_recall;
			}
		    }

		    /*
		     *	Save cancelled.
		     */
		    _statusline(SAVE_REQUEST_CANCELLED);
		    sleep(InfoSecs);
		    break;
		}

		if (no_dotfiles || !show_dotfiles) {
		    if (*filename == '.' ||
#ifdef VMS
			((cp = strrchr(filename, ':')) && *(cp+1) == '.') ||
			((cp = strrchr(filename, ']')) && *(cp+1) == '.') ||
#endif /* VMS */
			((cp = strrchr(filename, '/')) && *(cp+1) == '.')) {
			HTAlert(FILENAME_CANNOT_BE_DOT);
			_statusline(NEW_FILENAME_PROMPT);
			FirstRecall = TRUE;
			FnameNum = FnameTotal;
			goto retry;
		    }
		}
		/*
		 *  Cancel if the user entered "/dev/null" on Unix,
		 *  or an "nl:" path (case-insensitive) on VMS. - FM
		 */
#ifdef VMS
		if (!strncasecomp(filename, "nl:", 3) ||
		    !strncasecomp(filename, "/nl/", 4))
#else
		if (!strcmp(filename, "/dev/null"))
#endif /* VMS */
		{
		    _statusline(SAVE_REQUEST_CANCELLED);
		    sleep(InfoSecs);
		    break;
		}
		if ((cp = strchr(filename, '~'))) {
		    *(cp++) = '\0';
		    strcpy(buffer, filename);
		    if ((len=strlen(buffer)) > 0 && buffer[len-1] == '/')
			buffer[len-1] = '\0';
#ifdef DOSPATH
		    strcat(buffer, HTDOS_wwwName((char *)Home_Dir()));
#else
#ifdef VMS
		    strcat(buffer, HTVMS_wwwName((char *)Home_Dir()));
#else
		    strcat(buffer, Home_Dir());
#endif /* VMS */
#endif /* DOSPATH */
		    strcat(buffer, cp);
		    strcpy(filename, buffer);
		}
#ifdef VMS
		if (strchr(filename, '/') != NULL) {
		    strcpy(buffer, HTVMS_name("", filename));
		    strcpy(filename, buffer);
		}
		if (filename[0] != '/' && strchr(filename, ':') == NULL) {
		    strcpy(buffer, "sys$disk:");
		    if (strchr(filename, ']') == NULL)
		    strcat(buffer, "[]");
		    strcat(buffer, filename);
		} else {
		    strcpy(buffer, filename);
		}
#else
		if (*filename != '/')
		    cp = getenv("PWD");
		else
		    cp = NULL;
		if (cp)
#ifdef DOSPATH
		    sprintf(buffer, "%s/%s", cp, HTDOS_name(filename));
#else
		    sprintf(buffer, "%s/%s", cp, filename);
#endif
		else
#ifdef DOSPATH
		    strcpy(buffer, HTDOS_name(filename));
#else
		    strcpy(buffer, filename);
#endif
#endif /* VMS */

		/*
		 *  See if it already exists.
		 */
		if ((outfile_fp = fopen(buffer, "r")) != NULL) {
		    fclose(outfile_fp);
#ifdef VMS
		    _statusline(FILE_EXISTS_HPROMPT);
#else
		    _statusline(FILE_EXISTS_OPROMPT);
#endif /* VMS */
		    c = 0;
		    while (TOUPPER(c)!='Y' && TOUPPER(c)!='N' &&
			   c != 7 && c != 3)
			c = LYgetch();
#ifdef VMS
		    if (HadVMSInterrupt) {
			HadVMSInterrupt = FALSE;
			_statusline(SAVE_REQUEST_CANCELLED);
			sleep(InfoSecs);
			break;
		    }
#endif /* VMS */
		    if (c == 7 || c == 3) { /* Control-G or Control-C */
			_statusline(SAVE_REQUEST_CANCELLED);
			sleep(InfoSecs);
			break;
		    }
		    if (TOUPPER(c) == 'N') {
			_statusline(NEW_FILENAME_PROMPT);
			FirstRecall = TRUE;
			FnameNum = FnameTotal;
			goto retry;
		    }
		}

		if ((outfile_fp = LYNewTxtFile(buffer)) == NULL) {
		    HTAlert(CANNOT_WRITE_TO_FILE);
		    _statusline(NEW_FILENAME_PROMPT);
		    FirstRecall = TRUE;
		    FnameNum = FnameTotal;
		    goto retry;
		}

		if (LYPrependBaseToSource && HTisDocumentSource()) {
		    /*
		     *	Added the document's base as a BASE tag
		     *	to the top of the file.  May create
		     *	technically invalid HTML, but will help
		     *	get any partial or relative URLs resolved
		     *	properly if no BASE tag is present to
		     *	replace it. - FM
		     */
		    fprintf(outfile_fp,
			    "<!-- X-URL: %s -->\n<BASE HREF=\"%s\">\n",
			    newdoc->address, content_base);
		}

		if (LYPrependCharsetToSource && HTisDocumentSource()) {
		    /*
		     *	Added the document's charset as a META CHARSET tag
		     *	to the top of the file.  May create
		     *	technically invalid HTML, but will help to resolve
		     *	properly the document converted via chartrans:
		     *	printed document correspond to a display charset
		     *	and we *should* override both assume_local_charset
		     *	and original document's META CHARSET (if any).
		     *
		     *	Currently, if several META CHARSETs are found Lynx uses
		     *	the first only, and it is opposite to BASE where the
		     *	original BASE in the <HEAD> overrides ones from the
		     *	top.
		     *
		     *	As in print-to-email we write charset only if the
		     *	document has 8-bit characters, and we have no CJK or an
		     *	unofficial "x-" charset.
		     */
		     use_cte = HTLoadedDocumentEightbit();
		     disp_charset = LYCharSet_UC[current_char_set].MIMEname;
		     if (!use_cte || LYHaveCJKCharacterSet ||
			  strncasecomp(disp_charset, "x-", 2) == 0) {
		     } else {
			fprintf(outfile_fp,
				"<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=%s\">\n\n",
				disp_charset);
			}
		}

		print_wwwfile_to_fd(outfile_fp,0);
		if (keypad_mode)
		    printlist(outfile_fp,FALSE);

		fclose(outfile_fp);
#ifdef VMS
		if (0 == strncasecomp(buffer, "sys$disk:", 9)) {
		    if (0 == strncmp((buffer+9), "[]", 2)) {
			HTAddSugFilename(buffer+11);
		    } else {
			HTAddSugFilename(buffer+9);
		    }
		} else {
		    HTAddSugFilename(buffer);
		}
#else
		HTAddSugFilename(buffer);
#endif /* VMS */
#if defined(__DJGPP__) || defined(_WINDOWS)
		_fmode = O_BINARY;
#endif /* __DJGPP__ or _WINDOWS */
		break;

	case MAIL:
	    if (LYPreparsedSource && first_mail_preparsed &&
		HTisDocumentSource()) {
		_statusline(CONFIRM_MAIL_SOURCE_PREPARSED);
		c = 0;
		while (TOUPPER(c)!='Y' && TOUPPER(c)!='N' &&
		       c != 7 && c != 3)
		    c = LYgetch();
#ifdef VMS
		if (HadVMSInterrupt) {
		    HadVMSInterrupt = FALSE;
		    _statusline(MAIL_REQUEST_CANCELLED);
		    sleep(InfoSecs);
		    break;
		}
#endif /* VMS */
		if (c == RTARROW || c == 'y' || c== 'Y'
		    || c == '\n' || c == '\r') {
		    addstr("   Ok...");
		    first_mail_preparsed = FALSE;
		} else	{
		    _statusline(MAIL_REQUEST_CANCELLED);
		    sleep(InfoSecs);
		    break;
		}
	    }

		_statusline(MAIL_ADDRESS_PROMPT);
		strcpy(user_response, (personal_mail_address ?
				       personal_mail_address : ""));
		if (LYgetstr(user_response, VISIBLE,
			     sizeof(user_response), NORECALL) < 0 ||
		    *user_response == '\0') {
		    _statusline(MAIL_REQUEST_CANCELLED);
		    sleep(InfoSecs);
		    break;
		}

		/*
		 *  Determine which mail headers should be sent.
		 *  Use Content-Type and MIME-Version headers only
		 *  if needed.	We need them if we are mailing HTML
		 *  source, or if we have 8-bit characters and will
		 *  be sending Content-Transfer-Encoding to indicate
		 *  this.  We will append a charset parameter to the
		 *  Content-Type if we do not have an "x-" charset,
		 *  and we will include the Content-Transfer-Encoding
		 *  only if we are appending the charset parameter,
		 *  because indicating an 8-bit transfer without also
		 *  indicating the charset can cause problems with
		 *  many mailers. - FM & KW
		 */
		disp_charset = LYCharSet_UC[current_char_set].MIMEname;
		use_cte = HTLoadedDocumentEightbit();
		if (!(use_cte && strncasecomp(disp_charset, "x-", 2))) {
		    disp_charset = NULL;
		    use_cte = FALSE;
		}
		use_type =  (disp_charset || HTisDocumentSource());

		change_sug_filename(sug_filename);
#ifdef VMS
		if (strchr(user_response,'@') && !strchr(user_response,':') &&
		   !strchr(user_response,'%') && !strchr(user_response,'"')) {
		    sprintf(filename, mail_adrs, user_response);
		    strcpy(user_response, filename);
		}

		if (first) {
		    tempname(tempfile, NEW_FILE);
		    if (isPMDF) {
			tempname(hdrfile, NEW_FILE);
			if ((len = strlen(hdrfile)) > 4) {
			    len -= 5;
			    if (!strcasecomp((hdrfile + len), ".html")) {
				hdrfile[len] = '\0';
				strcat(hdrfile, ".txt");
			    }
			}
		    }
		    first = FALSE;
		} else {
		    remove(tempfile);	/* remove duplicates */
		}
		if (HTisDocumentSource()) {
		    if ((len = strlen(tempfile)) > 3) {
			len -= 4;
			if (!strcasecomp((tempfile + len), ".txt")) {
			    tempfile[len] = '\0';
			    strcat(tempfile, ".html");
			}
		    }
		} else if ((len = strlen(tempfile)) > 4) {
		    len -= 5;
		    if (!strcasecomp((tempfile + len), ".html")) {
			tempfile[len] = '\0';
			strcat(tempfile, ".txt");
		    }
		}
		if ((outfile_fp = LYNewTxtFile(tempfile)) == NULL) {
		    HTAlert(UNABLE_TO_OPEN_TEMPFILE);
		    break;
		}

		if (isPMDF) {
		    if ((hfd = LYNewTxtFile(hdrfile)) == NULL) {
			HTAlert(UNABLE_TO_OPEN_TEMPFILE);
			break;
		    }
		    if (use_type) {
			fprintf(hfd, "Mime-Version: 1.0\n");
			if (use_cte) {
			    fprintf(hfd,
				    "Content-Transfer-Encoding: 8bit\n");
			}
		    }
		    if (HTisDocumentSource()) {
			/*
			 *  Add Content-Type, Content-Location, and
			 *  Content-Base headers for HTML source. - FM
			 */
			fprintf(hfd, "Content-Type: text/html");
			if (disp_charset != NULL) {
			    fprintf(hfd,
				    "; charset=%s\n",
				    disp_charset);
			} else {
			    fprintf(hfd, "\n");
			}
			fprintf(hfd,
				"Content-Base: %s\n",
				content_base);
			fprintf(hfd,
				"Content-Location: %s\n",
				content_location);
		    } else {
			/*
			 *  Add Content-Type: text/plain if we have 8-bit
			 *  characters and a valid charset for non-source
			 *  documents. - FM
			 */
			if (disp_charset != NULL) {
			    fprintf(hfd,
				    "Content-Type: text/plain; charset=%s\n",
				    disp_charset);
			}
		    }
		    /*
		     *	X-URL header. - FM
		     */
		    fprintf(hfd, "X-URL: %s\n", newdoc->address);
		}

		/*
		 *  Write the contents to a temp file.
		 */
		if (LYPrependBaseToSource && HTisDocumentSource()) {
		    /*
		     *	Added the document's base as a BASE tag to
		     *	the top of the message body.  May create
		     *	technically invalid HTML, but will help
		     *	get any partial or relative URLs resolved
		     *	properly if no BASE tag is present to
		     *	replace it. - FM
		     */
		    fprintf(outfile_fp,
			    "<!-- X-URL: %s -->\n<BASE HREF=\"%s\">\n\n",
			    newdoc->address, content_base);
		} else if (!isPMDF) {
		    fprintf(outfile_fp, "X-URL: %s\n\n", newdoc->address);
		}
		print_wwwfile_to_fd(outfile_fp, 0);
		if (keypad_mode)
		    printlist(outfile_fp, FALSE);
		fclose(outfile_fp);

		if (isPMDF) {
		    /*
		     *	For PMDF, put the subject in the
		     *	header file and close it. - FM
		     */
		    fprintf(hfd, "Subject: %.70s\n\n", sug_filename);
		    fclose(hfd);
		    /*
		     *	Now set up the command. - FM
		     */
		    sprintf(buffer,
			    "%s %s %s,%s %s",
			    system_mail,
			    system_mail_flags,
			    hdrfile,
			    tempfile,
			    user_response);
		} else {
		    /*
		     *	For "generic" VMS MAIL, include
		     *	the subject in the command. - FM
		     */
		    remove_quotes(sug_filename);
		    sprintf(buffer,
			    "%s %s/subject=\"%.70s\" %s %s",
			    system_mail,
			    system_mail_flags,
			    sug_filename,
			    tempfile,
			    user_response);
		}

		stop_curses();
		printf(MAILING_FILE);
		fflush(stdout);
		system(buffer);
		fflush(stdout);
		sleep(AlertSecs);
		start_curses();
		if (isPMDF) {
		    /*
		     *	Delete the header file. - FM
		     */
		    remove(hdrfile);
		}
#else /* Unix: */
		sprintf(buffer, "%s %s", system_mail, system_mail_flags);

#ifdef DOSPATH
		sprintf(tempfile, "%s%s", lynx_temp_space, "temp_mail.txt");
		if ((outfile_fp = LYNewTxtFile(tempfile)) == NULL) {
			_statusline(MAIL_REQUEST_FAILED);
			sleep(AlertSecs);
			return;
		}
#else
		if ((outfile_fp = popen(buffer, "w")) == NULL) {
			_statusline(MAIL_REQUEST_FAILED);
			sleep(AlertSecs);
			break;
		}
#endif

		/*
		 *  Determine which mail headers should be sent.
		 *  Use Content-Type and MIME-Version headers only
		 *  if needed.	We need them if we are mailing HTML
		 *  source, or if we have 8-bit characters and will
		 *  be sending Content-Transfer-Encoding to indicate
		 *  this.
		 *
		 *  Send Content-Transfer-Encoding only if the document
		 *  has 8-bit characters.  Send a charset parameter only
		 *  if the document has 8-bit characters and we we seem
		 *  to have a valid charset.  - kw
		 */
		use_cte = HTLoadedDocumentEightbit();
		disp_charset = LYCharSet_UC[current_char_set].MIMEname;
		/*
		 *  Don't send a charset if we have a CJK character set
		 *  selected, since it may not be appropriate for mail...
		 *  Also don't use an inofficial "x-" charset. - kw
		 */
		if (!use_cte || LYHaveCJKCharacterSet ||
		    strncasecomp(disp_charset, "x-", 2) == 0) {
		    disp_charset = NULL;
		}
#ifdef NOTDEFINED
		/*  Enable this if indicating an 8-bit transfer without
		 *  also indicating the charset causes problems. - kw */
		if (use_cte && !disp_charse)
		    use_cte = FALSE;
#endif /* NOTDEFINED */
		use_type =  (disp_charset || HTisDocumentSource());
		use_mime = (use_cte || use_type);

		if (use_mime) {
		    fprintf(outfile_fp, "Mime-Version: 1.0\n");
		    if (use_cte) {
			fprintf(outfile_fp,
				"Content-Transfer-Encoding: 8bit\n");
		    }
		}

		if (HTisDocumentSource()) {
		    /*
		     *	Add Content-Type, Content-Location, and
		     *	Content-Base headers for HTML source. - FM
		     */
		    fprintf(outfile_fp, "Content-Type: text/html");
		    if (disp_charset != NULL) {
			fprintf(outfile_fp, "; charset=%s\n",
					    disp_charset);
		    } else {
			fprintf(outfile_fp, "\n");
		    }
		} else {
		    /*
		     *	Add Content-Type: text/plain if we have 8-bit
		     *	characters and a valid charset for non-source
		     *	documents. - KW
		     */
		    if (disp_charset != NULL) {
			fprintf(outfile_fp,
				"Content-Type: text/plain; charset=%s\n",
				disp_charset);
		    }
		}
		/*
		 *  If we are using MIME headers, add content-base and
		 *  content-location if we have them.  This will always
		 *  be the case if the document is source. - kw
		 */
		if (use_mime) {
		    if (content_base)
			fprintf(outfile_fp, "Content-Base: %s\n",
				content_base);
		    if (content_location)
			fprintf(outfile_fp, "Content-Location: %s\n",
				content_location);
		}

		/*
		 *  Add the To, Subject, and X-URL headers. - FM
		 */
		fprintf(outfile_fp, "To: %s\nSubject: %s\n",
				     user_response, sug_filename);
		fprintf(outfile_fp, "X-URL: %s\n\n", newdoc->address);
		if (LYPrependBaseToSource && HTisDocumentSource()) {
		    /*
		     *	Added the document's base as a BASE tag to
		     *	the top of the message body.  May create
		     *	technically invalid HTML, but will help
		     *	get any partial or relative URLs resolved
		     *	properly if no BASE tag is present to
		     *	replace it. - FM
		     */
		    fprintf(outfile_fp,
			    "<!-- X-URL: %s -->\n<BASE HREF=\"%s\">\n\n",
			    newdoc->address, content_base);
		}
		print_wwwfile_to_fd(outfile_fp, 0);
		if (keypad_mode)
		    printlist(outfile_fp, FALSE);

#ifdef DOSPATH
		sprintf(buffer, "%s -t \"%s\" -F %s", system_mail, user_response, tempfile);
		fclose(outfile_fp);	/* Close the tmpfile. */
		stop_curses();
		printf("Sending \n\n$ %s\n\nPlease wait...", buffer);
		system(buffer);
		sleep(MessageSecs);
		start_curses();
		remove(tempfile);	/* Delete the tmpfile. */
#else
		pclose(outfile_fp);
#endif
#endif /* VMS */
		break;

	case TO_SCREEN:
		pages = lines_in_file/(LYlines+1);
		/* count fractional pages ! */
		if ((lines_in_file % (LYlines+1)) > 0)
		    pages++;
		if (pages > 4) {
		    sprintf(filename, CONFIRM_LONG_SCREEN_PRINT, pages);
		    _statusline(filename);
		    c=LYgetch();
#ifdef VMS
		    if (HadVMSInterrupt) {
			HadVMSInterrupt = FALSE;
			_statusline(PRINT_REQUEST_CANCELLED);
			sleep(InfoSecs);
			break;
		    }
#endif /* VMS */
		    if (c == RTARROW || c == 'y' || c== 'Y'
			 || c == '\n' || c == '\r') {
			addstr("   Ok...");
		    } else {
			_statusline(PRINT_REQUEST_CANCELLED);
			sleep(InfoSecs);
			break;
		    }
		}

		_statusline(PRESS_RETURN_TO_BEGIN);
		*filename = '\0';
		if (LYgetstr(filename, VISIBLE,
			     sizeof(filename), NORECALL) < 0) {
		      _statusline(PRINT_REQUEST_CANCELLED);
		      sleep(InfoSecs);
		      break;
		}

		outfile_fp = stdout;

		stop_curses();
#ifndef VMS
		signal(SIGINT, SIG_IGN);
#endif /* !VMS */

		if (LYPrependBaseToSource && HTisDocumentSource()) {
		    /*
		     *	Added the document's base as a BASE tag
		     *	to the top of the file.  May create
		     *	technically invalid HTML, but will help
		     *	get any partial or relative URLs resolved
		     *	properly if no BASE tag is present to
		     *	replace it. - FM
		     */
		    fprintf(outfile_fp,
			    "<!-- X-URL: %s -->\n<BASE HREF=\"%s\">\n\n",
			    newdoc->address, content_base);
		}
		print_wwwfile_to_fd(outfile_fp, 0);
		if (keypad_mode)
		    printlist(outfile_fp, FALSE);

#ifdef VMS
		if (HadVMSInterrupt) {
		     HadVMSInterrupt = FALSE;
		     start_curses();
		     break;
		}
#endif /* VMS */
		fprintf(stdout,"\n\n%s", PRESS_RETURN_TO_FINISH);

		fflush(stdout);  /* refresh to screen */
		LYgetch();  /* grab some user input to pause */
#ifdef VMS
		HadVMSInterrupt = FALSE;
#endif /* VMS */
		start_curses();
		break;

	case PRINTER:
		pages = lines_in_file/pagelen;
		/* count fractional pages ! */
		if ((lines_in_file % pagelen) > 0)
		    pages++;
		if (pages > 4) {
		    sprintf(filename, CONFIRM_LONG_PAGE_PRINT, pages);
		    _statusline(filename);
		    c=LYgetch();
#ifdef VMS
		    if (HadVMSInterrupt) {
			HadVMSInterrupt = FALSE;
			_statusline(PRINT_REQUEST_CANCELLED);
			sleep(InfoSecs);
			break;
		    }
#endif /* VMS */
		    if (c == RTARROW || c == 'y' || c== 'Y'
			 || c == '\n' || c == '\r') {
			addstr("   Ok...");
		    } else  {
			_statusline(PRINT_REQUEST_CANCELLED);
			sleep(InfoSecs);
			break;
		    }
		}

		if (first) {
		    tempname(tempfile, NEW_FILE);
		    first = FALSE;
		} else {
		    remove(tempfile);	/* Remove previous tempfile. */
		}
		if (((cp = strrchr(tempfile, '.')) != NULL) &&
#ifdef VMS
		    NULL == strchr(cp, ']') &&
#endif /* VMS */
		    NULL == strchr(cp, '/')) {
		    if (HTisDocumentSource() &&
			strcasecomp(cp, HTML_SUFFIX)) {
			*cp = '\0';
			strcat(tempfile, HTML_SUFFIX);
		    } else if (!HTisDocumentSource() &&
			       strcasecomp(cp, ".txt")) {
			*cp = '\0';
			strcat(tempfile, ".txt");
		    }
		}
		if ((outfile_fp = LYNewTxtFile(tempfile)) == NULL) {
		    HTAlert(FILE_ALLOC_FAILED);
		    break;
		}

		if (LYPrependBaseToSource && HTisDocumentSource()) {
		    /*
		     *	Added the document's base as a BASE tag
		     *	to the top of the file.  May create
		     *	technically invalid HTML, but will help
		     *	get any partial or relative URLs resolved
		     *	properly if no BASE tag is present to
		     *	replace it. - FM
		     */
		    fprintf(outfile_fp,
			    "<!-- X-URL: %s -->\n<BASE HREF=\"%s\">\n\n",
			    newdoc->address, content_base);
		}
		print_wwwfile_to_fd(outfile_fp, 0);
		if (keypad_mode)
		    printlist(outfile_fp, FALSE);

		fclose(outfile_fp);

		/* find the right printer number */
		{
		    int count=0;
		    for (cur_printer = printers;
			 count < printer_number;
			 count++, cur_printer = cur_printer->next)
			; /* null body */
		}

		/*
		 *  Commands have the form "command %s [%s] [etc]"
		 *  where %s is the filename and the second optional
		 *  %s is the suggested filename.
		 */
		if (cur_printer->command != NULL) {
		    /*
		     *	Check for two '%s' and ask for the second filename
		     *	argument if there is.
		     */
		    char *first_s = strstr(cur_printer->command, "%s");
		    if (first_s && strstr(first_s+1, "%s")) {
			_statusline(FILENAME_PROMPT);
		again:	strcpy(filename, sug_filename);
			change_sug_filename(filename);
			if (!(HTisDocumentSource()) &&
			    (cp = strrchr(filename, '.')) != NULL) {
			    format = HTFileFormat(filename, &encoding, NULL);
			    if (!strcasecomp(format->name, "text/html") ||
				!IsUnityEnc(encoding)) {
				*cp = '\0';
				strcat(filename, ".txt");
			    }
			}
		check_again:
			if ((ch = LYgetstr(filename, VISIBLE,
					   sizeof(filename), recall)) < 0 ||
			    *filename == '\0' ||
			    ch == UPARROW || ch == DNARROW) {
			    if (recall && ch == UPARROW) {
				if (FirstRecall) {
				    FirstRecall = FALSE;
				    /*
				     *	Use the last Fname in the list. - FM
				     */
				    FnameNum = 0;
				} else {
				    /*
				     *	Go back to the previous Fname
				     *	in the list. - FM
				     */
				    FnameNum++;
				}
				if (FnameNum >= FnameTotal) {
				    /*
				     *	Reset the FirstRecall flag,
				     *	and use sug_file or a blank. - FM
				     */
				    FirstRecall = TRUE;
				    FnameNum = FnameTotal;
				    _statusline(FILENAME_PROMPT);
				    goto again;
				} else if ((cp = (char *)HTList_objectAt(
							sug_filenames,
							FnameNum)) != NULL) {
				    strcpy(filename, cp);
				    if (FnameTotal == 1) {
					_statusline(EDIT_THE_PREV_FILENAME);
				    } else {
					_statusline(EDIT_A_PREV_FILENAME);
				    }
				    goto check_again;
				}
			    } else if (recall && ch == DNARROW) {
				if (FirstRecall) {
				    FirstRecall = FALSE;
				    /*
				     *	Use the first Fname in the list. - FM
				     */
				    FnameNum = FnameTotal - 1;
				} else {
				    /*
				     *	Advance to the next Fname
				     *	in the list. - FM
				     */
				    FnameNum--;
				}
				if (FnameNum < 0) {
				    /*
				     *	Set the FirstRecall flag,
				     *	and use sug_file or a blank. - FM
				     */
				    FirstRecall = TRUE;
				    FnameNum = FnameTotal;
				    _statusline(FILENAME_PROMPT);
				    goto again;
				} else if ((cp = (char *)HTList_objectAt(
							sug_filenames,
							FnameNum)) != NULL) {
				    strcpy(filename, cp);
				    if (FnameTotal == 1) {
					_statusline(EDIT_THE_PREV_FILENAME);
				    } else {
					_statusline(EDIT_A_PREV_FILENAME);
				    }
				    goto check_again;
				}
			    }

			    /*
			     *	Printer cancelled.
			     */
			    _statusline(PRINT_REQUEST_CANCELLED);
			    sleep(InfoSecs);
			    break;
			}

			if (no_dotfiles || !show_dotfiles) {
			    if (*filename == '.' ||
#ifdef VMS
			       ((cp = strrchr(filename, ':')) &&
						*(cp+1) == '.') ||
			       ((cp = strrchr(filename, ']')) &&
						*(cp+1) == '.') ||
#endif /* VMS */
			       ((cp = strrchr(filename, '/')) &&
						*(cp+1) == '.')) {
				HTAlert(FILENAME_CANNOT_BE_DOT);
				_statusline(NEW_FILENAME_PROMPT);
				FirstRecall = TRUE;
				FnameNum = FnameTotal;
				goto again;
			    }
			}
			/*
			 *  Cancel if the user entered "/dev/null" on Unix,
			 *  or an "nl:" path (case-insensitive) on VMS. - FM
			 */
#ifdef VMS
			if (!strncasecomp(filename, "nl:", 3) ||
			    !strncasecomp(filename, "/nl/", 4))
#else
			if (!strcmp(filename, "/dev/null"))
#endif /* VMS */
			{
			    _statusline(PRINT_REQUEST_CANCELLED);
			    sleep(InfoSecs);
			    break;
			}
			HTAddSugFilename(filename);
		    }

#ifdef VMS
		    sprintf(buffer, cur_printer->command, tempfile, filename,
				    "", "", "", "", "", "", "", "", "", "");
#else /* Unix: */
		    /*
		     *	Prevent spoofing of the shell.
		     */
		    cp = quote_pathname(filename);
		    sprintf(buffer, cur_printer->command, tempfile, cp,
				    "", "", "", "", "", "", "", "", "", "");
		    FREE(cp);
#endif /* !VMS */

		} else {
		    HTAlert(PRINTER_MISCONF_ERROR);
		    break;
		}

		/*
		 *  Move the cursor to the top of the screen so that
		 *  output from system'd commands don't scroll up
		 *  the screen.
		 */
		move(1,1);

		stop_curses();
		if (TRACE)
		    fprintf(stderr, "command: %s\n", buffer);
		printf(PRINTING_FILE);
#ifdef VMS
		/*
		 *  Set document's title as a VMS logical. -  FM
		 */
		StrAllocCopy(envbuffer, HText_getTitle());
		if (!(envbuffer && *envbuffer))
		    StrAllocCopy(envbuffer, "No Title");
		Define_VMSLogical("LYNX_PRINT_TITLE", envbuffer);
#else
		/*
		 *  Set document's title as an environment variable. - JKT
		 */
		StrAllocCopy(envbuffer, "LYNX_PRINT_TITLE=");
		StrAllocCat(envbuffer, HText_getTitle());
		putenv(envbuffer);
#endif /* VMS */
		fflush(stdout);
		system(buffer);
#ifdef VMS
		/*
		 *  Remove LYNX_PRINT_TITLE logical. - FM
		 */
		Define_VMSLogical("LYNX_PRINT_TITLE", "");
#else
		/*
		 *  Remove LYNX_PRINT_TITLE value from environment. - KW
		 */
		envbuffer[17] = '\0'; /* truncate after '=' */
		putenv(envbuffer);
#endif /* VMS */
		FREE(envbuffer);
		fflush(stdout);
#ifndef VMS
		signal(SIGINT, cleanup_sig);
#endif /* !VMS */
		sleep(MessageSecs);
		start_curses();
		/* don't remove(tempfile); */
    } /* end switch */

    FREE(link_info);
    FREE(sug_filename);
    FREE(content_base);
    FREE(content_location);
    return(NORMAL);
}

#ifdef VMS
PRIVATE int remove_quotes ARGS1(
	char *, 	string)
{
   int i;

   for(i = 0; string[i] != '\0'; i++)
	if(string[i] == '"')
	   string[i] = ' ';
	else if(string[i] == '&')
	   string[i] = ' ';
	else if(string[i] == '|')
	   string[i] = ' ';

   return(0);
}
#endif /* VMS */

/*
 * print_options writes out the current printer choices to a file
 * so that the user can select printers in the same way that
 * they select all other links
 * printer links look like
 *  LYNXPRINT://LOCAL_FILE/lines=#	     print to a local file
 *  LYNXPRINT://TO_SCREEN/lines=#	     print to the screen
 *  LYNXPRINT://MAIL_FILE/lines=#	     mail the file
 *  LYNXPRINT://PRINTER/lines=#/number=#   print to printer number #
 */
PUBLIC int print_options ARGS2(
	char **,	newfile,
	int,		lines_in_file)
{
    static char tempfile[256];
    static BOOLEAN first = TRUE;
    static char print_filename[256];
    char buffer[LINESIZE];
    int count;
    int pages;
    FILE *fp0;
    lynx_printer_item_type *cur_printer;

    pages = lines_in_file/66 + 1;

    if (first) {
	tempname(tempfile, NEW_FILE);
#if defined (VMS) || defined (DOSPATH)
	sprintf(print_filename, "file://localhost/%s", tempfile);
#else
	sprintf(print_filename, "file://localhost%s", tempfile);
#endif /* VMS */
	first = FALSE;
#ifdef VMS
    } else {
	remove(tempfile);   /* Remove duplicates on VMS. */
#endif /* !VMS */
    }

    if ((fp0 = LYNewTxtFile(tempfile)) == NULL) {
	HTAlert(UNABLE_TO_OPEN_PRINTOP_FILE);
	return(-1);
    }

    StrAllocCopy(*newfile, print_filename);
    LYforce_no_cache = TRUE;

    fprintf(fp0, "<head>\n<title>%s</title>\n</head>\n<body>\n",
		 PRINT_OPTIONS_TITLE);

    fprintf(fp0,"<h1>Printing Options (%s Version %s)</h1><pre>\n",
				       LYNX_NAME, LYNX_VERSION);

    pages = (lines_in_file+65)/66;
    sprintf(buffer,
	   "   There are %d lines, or approximately %d page%s, to print.\n",
	    lines_in_file, pages, (pages > 1 ? "s" : ""));
    fputs(buffer,fp0);

    if (no_print || no_disk_save || child_lynx || no_mail)
	fprintf(fp0, "   Some print functions have been disabled!!!\n");

    fprintf(fp0, "   You have the following print choices.\n");
    fprintf(fp0, "   Please select one:\n\n");

    if (child_lynx == FALSE && no_disk_save == FALSE && no_print == FALSE)
	fprintf(fp0,
   "   <a href=\"LYNXPRINT://LOCAL_FILE/lines=%d\">Save to a local file</a>\n",
		lines_in_file);
    else
	fprintf(fp0,"   Save to disk disabled.\n");
    if (child_lynx == FALSE && no_mail == FALSE)
	 fprintf(fp0,
   "   <a href=\"LYNXPRINT://MAIL_FILE/lines=%d\">Mail the file</a>\n",
		lines_in_file);
    fprintf(fp0,
   "   <a href=\"LYNXPRINT://TO_SCREEN/lines=%d\">Print to the screen</a>\n",
		lines_in_file);

    for (count = 0, cur_printer = printers; cur_printer != NULL;
	cur_printer = cur_printer->next, count++)
    if (no_print == FALSE || cur_printer->always_enabled) {
	fprintf(fp0,
   "   <a href=\"LYNXPRINT://PRINTER/number=%d/pagelen=%d/lines=%d\">",
		count, cur_printer->pagelen, lines_in_file);
	fprintf(fp0, (cur_printer->name ?
		      cur_printer->name : "No Name Given"));
	fprintf(fp0, "</a>\n");
    }
    fprintf(fp0, "</pre>\n</body>\n");
    fclose(fp0);

    LYforce_no_cache = TRUE;
    return(0);
}
