#include "HTUtils.h"
#include "tcp.h"
#include "HTParse.h"
#include "HTList.h"
#include "HTAlert.h"
#include "LYCurses.h"
#include "LYUtils.h"
#include "LYGlobalDefs.h"
#include "LYSignal.h"
#include "LYStrings.h"
#include "LYClean.h"
#include "LYGetFile.h"
#include "LYDownload.h"
#include "LYSystem.h"
#ifdef VMS
#include "HTVMSUtils.h"
#endif /* VMS */
#ifdef DOSPATH
#include "HTDOS.h"
#endif

#include "LYexit.h"
#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

/*
 *  LYDownload takes a URL and downloads it using a user selected
 *  download program
 *
 *  It parses an incoming link that looks like
 *
 *  LYNXDOWNLOAD://Method=<#>/File=<STRING>/SugFile=<STRING>
 */
#ifdef VMS
#define COPY_COMMAND "copy/nolog/noconf %s %s"
PUBLIC BOOLEAN LYDidRename = FALSE;
#endif /* VMS */

PRIVATE char LYValidDownloadFile[256] = "\0";

PUBLIC void LYDownload ARGS1(
	char *, 	line)
{
    char *Line = NULL, *method, *file, *sug_file = NULL;
    int method_number;
    int count;
    char buffer[512];
    char command[512];
    char *cp, *cp1;
    lynx_html_item_type *download_command = 0;
    int c, len;
    FILE *fp;
    int ch, recall;
    int FnameTotal;
    int FnameNum;
    BOOLEAN FirstRecall = TRUE;
    BOOLEAN SecondS = FALSE;
#ifdef VMS
    extern BOOLEAN HadVMSInterrupt;
    LYDidRename = FALSE;
#endif /* VMS */

    /*
     *	Make sure we have a valid download
     *	file comparison string loaded via
     *	the download options menu. - FM
     */
    if (LYValidDownloadFile[0] == '\0') {
	goto failed;
    }

    /*
     *	Make a copy of the LYNXDOWNLOAD
     *	internal URL for parsing. - FM
     */
    StrAllocCopy(Line, line);

    /*
     *	Parse out the sug_file, Method and the File.
     */
    if ((sug_file = (char *)strstr(Line, "SugFile=")) != NULL) {
	*(sug_file-1) = '\0';
	/*
	 *  Go past "SugFile=".
	 */
	sug_file += 8;
    }

    if ((file = (char *)strstr(Line, "File=")) == NULL)
	goto failed;
    *(file-1) = '\0';
    /*
     *	Go past "File=".
     */
    file += 5;

    /*
     *	Make sure that the file string is the one from
     *	the last displayed download options menu. - FM
     */
    if (strcmp(file, LYValidDownloadFile)) {
	goto failed;
    }

#ifdef DIRED_SUPPORT
    if (!strncmp(file, "file://localhost", 16))
	file += 16;
    else if (!strncmp(file, "file:", 5))
	file += 5;
    HTUnEscape(file);
#endif /* DIRED_SUPPORT */

    if ((method = (char *)strstr(Line, "Method=")) == NULL)
	goto failed;
    /*
     *	Go past "Method=".
     */
    method += 7;
    method_number = atoi(method);

    /*
     *	Set up the sug_filenames recall buffer.
     */
    FnameTotal = (sug_filenames ? HTList_count(sug_filenames) : 0);
    recall = ((FnameTotal >= 1) ? RECALL : NORECALL);
    FnameNum = FnameTotal;

    if (method_number < 0) {
	/*
	 *  Write to local file.
	 */
	_statusline(FILENAME_PROMPT);
retry:
	if (sug_file)
	    LYstrncpy(buffer, sug_file, ((sizeof(buffer)/2) - 1));
	else
	    *buffer = '\0';
check_recall:
	if ((ch = LYgetstr(buffer,
			   VISIBLE, (sizeof(buffer)/2), recall)) < 0 ||
	    *buffer == '\0' || ch == UPARROW || ch == DNARROW) {
	    if (recall && ch == UPARROW) {
		if (FirstRecall) {
		    FirstRecall = FALSE;
		    /*
		     *	Use the last Fname in the list. - FM
		     */
		    FnameNum = 0;
		} else {
		    /*
		     *	Go back to the previous Fname in the list. - FM
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
		    strcpy(buffer, cp);
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
		     *	Use the first Fname in the list. - FM
		     */
		    FnameNum = FnameTotal - 1;
		} else {
		    /*
		     *	Advance to the next Fname in the list. - FM
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
		    strcpy(buffer, cp);
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
	    goto cancelled;
	}

	if (no_dotfiles || !show_dotfiles) {
	  if (*buffer == '.' ||
#ifdef VMS
	      ((cp = strrchr(buffer, ':')) && *(cp+1) == '.') ||
	      ((cp = strrchr(buffer, ']')) && *(cp+1) == '.') ||
#endif /* VMS */
	      ((cp = strrchr(buffer, '/')) && *(cp+1) == '.')) {
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
	if (!strncasecomp(buffer, "nl:", 3) ||
	    !strncasecomp(buffer, "/nl/", 4))
#else
	if (!strcmp(buffer, "/dev/null"))
#endif /* VMS */
	{
	    goto cancelled;
	}
	if ((cp = strchr(buffer, '~'))) {
	    *(cp++) = '\0';
	    strcpy(command, buffer);
	    if ((len = strlen(command)) > 0 && command[len-1] == '/')
		command[len-1] = '\0';
#ifdef DOSPATH
	    strcat(command, HTDOS_wwwName((char *)Home_Dir()));
#else
#ifdef VMS
	    strcat(command, HTVMS_wwwName((char *)Home_Dir()));
#else
	    strcat(command, Home_Dir());
#endif /* VMS */
#endif /* DOSPATH */
	    strcat(command, cp);
	    strcpy(buffer, command);
	}
#ifdef VMS
	if (strchr(buffer, '/') != NULL) {
	    strcpy(command, HTVMS_name("", buffer));
	    strcpy(buffer, command);
	}
	if (buffer[0] != '/' && strchr(buffer, ':') == NULL) {
	    strcpy(command, "sys$disk:");
	    if (strchr(buffer, ']') == NULL)
		strcat(command, "[]");
	    strcat(command, buffer);
	    strcpy(buffer, command);
	}
#else
	if (*buffer != '/')
	    cp = getenv("PWD");
	else
	    cp = NULL;
	if (cp) {
	    sprintf(command, "%s/%s", cp, buffer);
#ifdef DOSPATH
	    strcpy(buffer, HTDOS_name(command));
#else
	    strcpy(buffer, command);
#endif
	}
#endif /* VMS */

	/*
	 *  See if it already exists.
	 */
	if ((fp = fopen(buffer, "r")) != NULL) {
	    fclose(fp);

#ifdef VMS
	    _statusline(FILE_EXISTS_HPROMPT);
#else
	    _statusline(FILE_EXISTS_OPROMPT);
#endif /* VMS */
	    c = 0;
	    while(TOUPPER(c)!='Y' && TOUPPER(c)!='N' && c != 7 && c != 3)
		c = LYgetch();
#ifdef VMS
	    if (HadVMSInterrupt) {
		HadVMSInterrupt = FALSE;
		FREE(Line);
		return;
	    }
#endif /* VMS */

	    if (c == 7 || c == 3) { /* Control-G or Control-C */
		goto cancelled;
	    }

	    if (TOUPPER(c) == 'N') {
		_statusline(NEW_FILENAME_PROMPT);
		FirstRecall = TRUE;
		FnameNum = FnameTotal;
		goto retry;
	    }
	}

	/*
	 *  See if we can write to it.
	 */
	if ((fp = fopen(buffer, "w")) != NULL) {
	    fclose(fp);
	    remove(buffer);
	} else {
	    HTAlert(CANNOT_WRITE_TO_FILE);
	    _statusline(NEW_FILENAME_PROMPT);
	    FirstRecall = TRUE;
	    FnameNum = FnameTotal;
	    goto retry;
	}
	SecondS = TRUE;

	_statusline(SAVING);
	sleep(InfoSecs);
#ifdef VMS
	/*
	 *  Try rename() first. - FM
	 */
	if (TRACE)
	    fprintf(stderr, "command: rename(%s, %s)\n", file, buffer);
	if (rename(file, buffer)) {
	    /*
	     *	Failed.  Use spawned COPY_COMMAND. - FM
	     */
	    if (TRACE)
		fprintf(stderr, "         FAILED!\n");
	    sprintf(command, COPY_COMMAND, file, buffer);
	    if (TRACE)
		fprintf(stderr, "command: %s\n", command);
	    fflush(stderr);
	    fflush(stdout);
	    stop_curses();
	    system(command);
	    fflush(stdout);
	    fflush(stderr);
	    start_curses();
	} else {
	    /*
	     *	We don't have the temporary file (it was renamed to
	     *	a permanent file), so set a flag to pop out of the
	     *	download menu. - FM
	     */
	    LYDidRename = TRUE;
	}
	chmod(buffer, HIDE_CHMOD);
#else /* Unix: */
	/*
	 *  Prevent spoofing of the shell.
	 */
	cp = quote_pathname(file);
	cp1 = quote_pathname(buffer);
	sprintf(command, "%s %s %s", COPY_PATH, cp, cp1);
	FREE(cp);
	FREE(cp1);
	if (TRACE)
	    fprintf(stderr, "command: %s\n", command);
	fflush(stderr);
	fflush(stdout);
	stop_curses();
	system(command);
	fflush(stdout);
	fflush(stderr);
	start_curses();
#if defined(UNIX)
	LYRelaxFilePermissions(buffer);
#endif /* defined(UNIX) */
#endif /* VMS */

    } else {
	/*
	 *  Use configured download commands.
	 */
	buffer[0] = '\0';
	for (count = 0, download_command=downloaders;
	     count < method_number;
	     count++, download_command = download_command->next)
	    ; /* null body */

	/*
	 *  Commands have the form "command %s [etc]"
	 *  where %s is the filename.
	 */
	if (download_command->command != NULL) {
	    /*
	     *	Check for two '%s' and ask for the local filename if
	     *	there is.
	     */
	    char *first_s = strstr(download_command->command, "%s");
	    if (first_s && strstr(first_s+1, "%s")) {
		_statusline(FILENAME_PROMPT);
	again:	if (sug_file)
		    strcpy(buffer, sug_file);
		else
		    *buffer = '\0';
	check_again:
		if ((ch = LYgetstr(buffer, VISIBLE,
				   sizeof(buffer), recall)) < 0 ||
		    *buffer == '\0' || ch == UPARROW || ch == DNARROW) {
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
			    strcpy(buffer, cp);
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
			     *	Advance to the next Fname in the list. - FM
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
			    strcpy(buffer, cp);
			    if (FnameTotal == 1) {
				_statusline(EDIT_THE_PREV_FILENAME);
			    } else {
				_statusline(EDIT_A_PREV_FILENAME);
			    }
			    goto check_again;
			}
		    }

		    /*
		     * Download cancelled.
		     */
		    goto cancelled;
		}

		if (no_dotfiles || !show_dotfiles) {
		    if (*buffer == '.' ||
#ifdef VMS
		       ((cp = strrchr(buffer, ':')) && *(cp+1) == '.') ||
		       ((cp = strrchr(buffer, ']')) && *(cp+1) == '.') ||
#endif /* VMS */
		       ((cp = strrchr(buffer, '/')) && *(cp+1) == '.')) {
			HTAlert(FILENAME_CANNOT_BE_DOT);
			_statusline(NEW_FILENAME_PROMPT);
			goto again;
		    }
		}
		/*
		 *  Cancel if the user entered "/dev/null" on Unix,
		 *  or an "nl:" path (case-insensitive) on VMS. - FM
		 */
#ifdef VMS
		if (!strncasecomp(buffer, "nl:", 3) ||
		    !strncasecomp(buffer, "/nl/", 4))
#else
		if (!strcmp(buffer, "/dev/null"))
#endif /* VMS */
		{
		    goto cancelled;
		}
		SecondS = TRUE;
	    }

	    /*
	     *	The following is considered a bug by the community.
	     *	If the command only takes one argument on the command
	     *	line, then the suggested file name is not used.
	     *	It actually is not a bug at all and does as it should,
	     *	putting both names on the command line.
	     */
#ifdef VMS
	    sprintf(command, download_command->command, file, buffer,
			     "", "", "", "", "", "", "", "", "", "");
#else /* Unix: */
	    /*
	     *	Prevent spoofing of the shell.
	     */
	    cp = quote_pathname(file);
	    cp1 = quote_pathname(buffer);
	    sprintf(command, download_command->command, cp, cp1,
			     "", "", "", "", "", "", "", "", "", "");
	    FREE(cp);
	    FREE(cp1);
#endif /* VMS */

	} else {
	    _statusline(MISCONF_DOWNLOAD_COMMAND);
	    sleep(AlertSecs);
	    goto failed;
	}

	if (TRACE)
	    fprintf(stderr, "command: %s\n", command);
	stop_curses();
	fflush(stderr);
	fflush(stdout);
	system(command);
	fflush(stderr);
	fflush(stdout);
	start_curses();
	/* don't remove(file); */
    }

    if (SecondS == TRUE) {
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
    }
    FREE(Line);
    return;

failed:
    _statusline(CANNOT_DOWNLOAD_FILE);
    sleep(AlertSecs);
    FREE(Line);
    return;

cancelled:
    _statusline(CANCELLING);
    sleep(InfoSecs);
    FREE(Line);
    return;
}

/*
 *  LYdownload_options writes out the current download choices to
 *  a file so that the user can select printers in the same way that
 *  they select all other links.  Download links look like:
 *  LYNXDOWNLOAD://Method=<#>/File=<STRING>/SugFile=<STRING>
 */
PUBLIC int LYdownload_options ARGS2(
	char **,	newfile,
	char *, 	data_file)
{
    static char tempfile[256];
    static BOOLEAN first = TRUE;
    static char download_filename[256];
    char *sug_filename = NULL;
    FILE *fp0;
    lynx_html_item_type *cur_download;
    int count;

    if (first) {
	tempname(tempfile, NEW_FILE);
	first = FALSE;
#if defined (VMS) || defined (DOSPATH)
    sprintf(download_filename, "file://localhost/%s", tempfile);
#else
    sprintf(download_filename, "file://localhost%s", tempfile);
#endif /* VMS */
#ifdef VMS
    } else {
	remove(tempfile);   /* Remove duplicates on VMS. */
#endif /* VMS */
    }

    /*
     *	Get a suggested filename.
     */
    StrAllocCopy(sug_filename, *newfile);
    change_sug_filename(sug_filename);

    if ((fp0 = LYNewTxtFile(tempfile)) == NULL) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(-1);
    }

    LYstrncpy(LYValidDownloadFile,
	      data_file,
	      (sizeof(LYValidDownloadFile) - 1));
    StrAllocCopy(*newfile, download_filename);
    LYforce_no_cache = TRUE;  /* don't cache this doc */

    fprintf(fp0, "<head>\n<title>%s</title>\n</head>\n<body>\n",
		 DOWNLOAD_OPTIONS_TITLE);

    fprintf(fp0,"<h1>Download Options (%s Version %s)</h1><pre>\n",
				       LYNX_NAME, LYNX_VERSION);


    fprintf(fp0, "   You have the following download choices.\n");
    fprintf(fp0, "   Please select one:\n\n");

    if(!no_disk_save && !child_lynx)
#ifdef DIRED_SUPPORT
	/*
	 *  Disable save to disk option for local files.
	 */
	if (!lynx_edit_mode)
#endif /* DIRED_SUPPORT */
	    fprintf(fp0,"   \
<a href=\"LYNXDOWNLOAD://Method=-1/File=%s/SugFile=%s%s\">Save to disk</a>\n",
	   data_file, (lynx_save_space ? lynx_save_space : ""), sug_filename);
#ifdef DIRED_SUPPORT
	else {}
#endif /* DIRED_SUPPORT */
    else
	fprintf(fp0,"   Save to disk disabled.\n");

    if (downloaders != NULL) {
	for (count = 0, cur_download = downloaders; cur_download != NULL;
			cur_download = cur_download->next, count++) {
	    if (!no_download || cur_download->always_enabled) {
		fprintf(fp0,"   \
<a href=\"LYNXDOWNLOAD://Method=%d/File=%s/SugFile=%s\">",
				count,data_file, sug_filename);
		fprintf(fp0, (cur_download->name ?
				cur_download->name : "No Name Given"));
		fprintf(fp0,"</a>\n");
	    }
	}
    } else {
	fprintf(fp0, "\n\
No other download methods have been defined yet.  You may define\n\
an unlimited number of download methods using the lynx.cfg file.\n");
    }
    fprintf(fp0, "</pre>\n</body>\n");
    fclose(fp0);

    /*
     *	Free off temp copy.
     */
    FREE(sug_filename);

    return(0);
}
