#include <HTUtils.h>
#include <HTParse.h>
#include <HTList.h>
#include <HTAlert.h>
#include <LYCurses.h>
#include <LYUtils.h>
#include <LYGlobalDefs.h>
#include <LYStrings.h>
#include <LYDownload.h>

#include <LYLeaks.h>

/*
 *  LYDownload takes a URL and downloads it using a user selected
 *  download program
 *
 *  It parses an incoming link that looks like
 *
 *  LYNXDOWNLOAD://Method=<#>/File=<STRING>/SugFile=<STRING>
 */
#ifdef VMS
PUBLIC BOOLEAN LYDidRename = FALSE;
#endif /* VMS */

PRIVATE char LYValidDownloadFile[LY_MAXPATH] = "\0";

PUBLIC void LYDownload ARGS1(
	char *, 	line)
{
    char *Line = NULL, *method, *file, *sug_file = NULL;
    int method_number;
    int count;
    char *the_command = 0;
    char buffer[LY_MAXPATH];
    char command[LY_MAXPATH];
    char *cp;
    lynx_html_item_type *download_command = 0;
    FILE *fp;
    int ch, recall;
    int FnameTotal;
    int FnameNum;
    BOOLEAN FirstRecall = TRUE;
    BOOLEAN SecondS = FALSE;
#ifdef VMS
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
	HTUnEscape(sug_file);
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
    /* FIXME: use HTLocalName */
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

	strcpy(command, buffer);
	if (!LYValidateFilename(buffer, command))
	    goto cancelled;
#if HAVE_POPEN
	else if (LYIsPipeCommand(buffer)) {
	    /* I don't know how to download to a pipe */
	    HTAlert(CANNOT_WRITE_TO_FILE);
	    _statusline(NEW_FILENAME_PROMPT);
	    FirstRecall = TRUE;
	    FnameNum = FnameTotal;
	    goto retry;
	}
#endif

	/*
	 *  See if it already exists.
	 */
	switch (LYValidateOutput(buffer)) {
	case 'Y':
	    break;
	case 'N':
	    _statusline(NEW_FILENAME_PROMPT);
	    FirstRecall = TRUE;
	    FnameNum = FnameTotal;
	    goto retry;
	default:
	    FREE(Line);
	    return;
	}

	/*
	 *  See if we can write to it.
	 */
	CTRACE(tfp, "LYDownload: filename is %s", buffer);

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

	HTInfoMsg(SAVING);
#ifdef VMS
	/*
	 *  Try rename() first. - FM
	 */
	CTRACE(tfp, "command: rename(%s, %s)\n", file, buffer);
	if (rename(file, buffer)) {
	    /*
	     *	Failed.  Use spawned COPY_COMMAND. - FM
	     */
	    CTRACE(tfp, "         FAILED!\n");
	    LYCopyFile(file, buffer);
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

	LYCopyFile(file, buffer);

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
	    if (HTCountCommandArgs(download_command->command) >= 2) {
		_statusline(FILENAME_PROMPT);
	again:	if (sug_file)
		    strncpy(buffer, sug_file, (sizeof(buffer)/2) - 1);
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
		    if (*LYPathLeaf(buffer) == '.') {
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
	    count = 1;
	    HTAddParam(&the_command, download_command->command, count, file);
	    if (HTCountCommandArgs(download_command->command) > 1)
		HTAddParam(&the_command, download_command->command, ++count, buffer);
	    HTEndParam(&the_command, download_command->command, count);

	} else {
	    HTAlert(MISCONF_DOWNLOAD_COMMAND);
	    goto failed;
	}

	CTRACE(tfp, "command: %s\n", the_command);
	stop_curses();
	LYSystem(the_command);
	FREE(the_command);
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
    HTAlert(CANNOT_DOWNLOAD_FILE);
    FREE(Line);
    return;

cancelled:
    HTInfoMsg(CANCELLING);
    FREE(Line);
    return;
}

/*
 *  LYdownload_options writes out the current download choices to
 *  a file so that the user can select downloaders in the same way that
 *  they select all other links.  Download links look like:
 *  LYNXDOWNLOAD://Method=<#>/File=<STRING>/SugFile=<STRING>
 */
PUBLIC int LYdownload_options ARGS2(
	char **,	newfile,
	char *, 	data_file)
{
    static char tempfile[LY_MAXPATH];
    char *downloaded_url = NULL;
    char *sug_filename = NULL;
    FILE *fp0;
    lynx_html_item_type *cur_download;
    int count;

    /*
     *	Get a suggested filename.
     */
    StrAllocCopy(sug_filename, *newfile);
    change_sug_filename(sug_filename);

    LYRemoveTemp(tempfile);
    if ((fp0 = LYOpenTemp(tempfile, HTML_SUFFIX, "w")) == NULL) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(-1);
    }
    StrAllocCopy(downloaded_url, *newfile);
    LYLocalFileToURL(newfile, tempfile);

    LYstrncpy(LYValidDownloadFile,
	      data_file,
	      (sizeof(LYValidDownloadFile) - 1));
    LYforce_no_cache = TRUE;  /* don't cache this doc */


    BeginInternalPage(fp0, DOWNLOAD_OPTIONS_TITLE, DOWNLOAD_OPTIONS_HELP);

    fprintf(fp0, "<pre>\n");
    fprintf(fp0, "<em>%s</em> %s\n",
    	    gettext("Downloaded link:"),
	    downloaded_url);
    FREE(downloaded_url);

    fprintf(fp0, "<em>%s</em> %s\n",
    	    gettext("Suggested file name:"),
	    sug_filename);

    fprintf(fp0, "\n%s\n",
	    (user_mode == NOVICE_MODE)
	    ? gettext("Standard download options:")
	    : gettext("Download options:"));

    if (!no_disk_save && !child_lynx) {
#ifdef DIRED_SUPPORT
	/*
	 *  Disable save to disk option for local files.
	 */
	if (!lynx_edit_mode)
#endif /* DIRED_SUPPORT */
	fprintf(fp0,
		"   <a href=\"LYNXDOWNLOAD://Method=-1/File=%s/SugFile=%s%s\">%s</a>\n",
		data_file,
		(lynx_save_space ? lynx_save_space : ""),
		sug_filename,
		gettext("Save to disk"));
    } else {
	fprintf(fp0, "   <em>%s</em>\n", gettext("Save to disk disabled."));
    }

    if (user_mode == NOVICE_MODE)
	fprintf(fp0, "\n%s\n", gettext("Local additions:"));

    if (downloaders != NULL) {
	for (count = 0, cur_download = downloaders; cur_download != NULL;
			cur_download = cur_download->next, count++) {
	    if (!no_download || cur_download->always_enabled) {
		fprintf(fp0, "   <a href=\"LYNXDOWNLOAD://Method=%d/File=%s/SugFile=%s\">",
			count,data_file, sug_filename);
		fprintf(fp0, "%s", (cur_download->name ?
			cur_download->name : gettext("No Name Given")));
		fprintf(fp0,"</a>\n");
	    }
	}
    }

    fprintf(fp0, "</pre>\n");
    EndInternalPage(fp0);
    LYCloseTempFP(fp0);

    /*
     *	Free off temp copy.
     */
    FREE(sug_filename);

    return(0);
}
