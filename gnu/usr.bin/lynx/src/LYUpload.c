/*
**  Routines to upload files to the local filesystem.
**  Created by: Rick Mallett, Carleton University
**  Report problems to rmallett@ccs.carleton.ca
**  Modified 15-Dec-95 George Lindholm (lindholm@ucs.ubc.ca):
**	Reread the upload menu page every time, in case the "upload" directory
**	  has changed (make the current directory that for the upload process).
**	Prompt for the upload file name if there is no "%s" in the command
**	  string.  Most protocols allow the user to specify the file name
**	  from the client side.  Xmodem appears to be the only that can't
**	  figure out the filename from the transfer data so it needs the
**	  information from lynx (or an upload script which prompts for it).
**	  On the other hand, zmodem aborts when you give it a filename on
**	  the command line (great way of bypassing the nodotfile code :=( ).
*/

#include <HTUtils.h>
#include <HTFile.h>
#include <HTParse.h>
#include <HTAlert.h>
#include <LYCurses.h>
#include <LYUtils.h>
#include <LYGlobalDefs.h>
#include <LYStrings.h>
#include <LYClean.h>
#include <LYGetFile.h>
#include <LYUpload.h>
#include <LYLocal.h>

#include <LYexit.h>
#include <LYLeaks.h>

#define SUBDIR_COMMAND "cd %s ; "

/*
 *  LYUpload uploads a file to a given location using a
 *  specified upload method.  It parses an incoming link
 *  that looks like:
 *	LYNXDIRED://UPLOAD=<#>/TO=<STRING>
 */
PUBLIC int LYUpload ARGS1(
	char *, 	line)
{
    char *method, *directory;
    int method_number;
    int count;
    char *the_upload = 0;
    char tmpbuf[LY_MAXPATH];
    char *filename = NULL;
    lynx_list_item_type *upload_command = 0;
    char *the_command = 0;

    /*
     *	Use configured upload commands.
     */
    if((directory = strstr(line, "TO=")) == NULL)
	goto failed;
    *(directory - 1) = '\0';
    /* go past "Directory=" */
    directory += 3;

    if((method = strstr(line, "UPLOAD=")) == NULL)
	goto failed;
    /*
     *	Go past "Method=".
     */
    method += 7;
    method_number = atoi(method);

    for (count = 0, upload_command = uploaders; count < method_number;
	count++, upload_command = upload_command->next)
      ; /* null body */

    /*
     *	Parsed out the Method and the Location?
     */
    if (upload_command->command == NULL) {
	HTAlert(gettext("ERROR! - upload command is misconfigured"));
	goto failed;
    }

    /*
     *	Care about the local name?
     */
    if (HTCountCommandArgs (upload_command->command)) {
	/*
	 *  Commands have the form "command %s [etc]"
	 *  where %s is the filename.
	 */
	_statusline(FILENAME_PROMPT);
retry:
	*tmpbuf = '\0';
	if (LYgetstr(tmpbuf, VISIBLE, sizeof(tmpbuf), NORECALL) < 0)
	    goto cancelled;

	if (*tmpbuf == '\0')
	    goto cancelled;

	if (strstr(tmpbuf, "../") != NULL) {
	    HTAlert(gettext("Illegal redirection \"../\" found! Request ignored."));
	    goto cancelled;
	} else if (strchr(tmpbuf, '/') != NULL) {
	    HTAlert(gettext("Illegal character \"/\" found! Request ignored."));
	    goto cancelled;
	} else if (tmpbuf[0] == '~') {
	    HTAlert(gettext("Illegal redirection using \"~\" found! Request ignored."));
	    goto cancelled;
	}
	HTSprintf0(&filename, "%s/%s", directory, tmpbuf);

#ifdef HAVE_POPEN
	if (LYIsPipeCommand(filename)) {
	    HTAlert(CANNOT_WRITE_TO_FILE);
	    _statusline(NEW_FILENAME_PROMPT);
	    goto retry;
	}
#endif
	switch (LYValidateOutput(filename)) {
	case 'Y':
	    break;
	case 'N':
	    goto retry;
	default:
	    goto cancelled;
	}

	/*
	 *  See if we can write to it.
	 */
	CTRACE((tfp, "LYUpload: filename is %s", filename));

	if (! LYCanWriteFile(filename)) {
	    goto retry;
	}

	HTAddParam(&the_upload, upload_command->command, 1, filename);
	HTEndParam(&the_upload, upload_command->command, 1);
    } else {			/* No substitution, no changes */
	StrAllocCopy(the_upload, upload_command->command);
    }

    HTAddParam(&the_command, SUBDIR_COMMAND, 1, directory);
    HTEndParam(&the_command, SUBDIR_COMMAND, 1);
    StrAllocCat(the_command, the_upload);

    CTRACE((tfp, "command: %s\n", the_command));

    stop_curses();
    LYSystem(the_command);
    start_curses();

    FREE(the_command);
    FREE(the_upload);
#if defined(MULTI_USER_UNIX)
    if (filename != 0)
	chmod(filename, HIDE_CHMOD);
#endif /* UNIX */
    FREE(filename);

    return 1;

failed:
    HTAlert(gettext("Unable to upload file."));
    return 0;

cancelled:
    HTInfoMsg(CANCELLING);
    return 0;
}

/*
 *  LYUpload_options writes out the current upload choices to a
 *  file so that the user can select printers in the same way that
 *  they select all other links.  Upload links look like:
 *	LYNXDIRED://UPLOAD=<#>/TO=<STRING>
 */
PUBLIC int LYUpload_options ARGS2(
	char **,	newfile,
	char *, 	directory)
{
    static char tempfile[LY_MAXPATH];
    FILE *fp0;
    lynx_list_item_type *cur_upload;
    int count;
    static char curloc[LY_MAXPATH];
    char *cp;

    if ((fp0 = InternalPageFP(tempfile, TRUE)) == 0)
	return(-1);

#ifdef VMS
    strcpy(curloc, "/sys$login");
#else
    cp = HTfullURL_toFile(directory);
    strcpy(curloc,cp);
    LYTrimPathSep(curloc);
    FREE(cp);
#endif /* VMS */

    LYLocalFileToURL(newfile, tempfile);
    LYRegisterUIPage(*newfile, UIP_UPLOAD_OPTIONS);

    BeginInternalPage(fp0, UPLOAD_OPTIONS_TITLE, UPLOAD_OPTIONS_HELP);

    fprintf(fp0, "<pre>\n");
    fprintf(fp0, "   <em>%s</em> %s\n", gettext("Upload To:"), curloc);
    fprintf(fp0, "\n%s\n", gettext("Upload options:"));

    if (uploaders != NULL) {
	for (count = 0, cur_upload = uploaders;
	     cur_upload != NULL;
	     cur_upload = cur_upload->next, count++) {
	    fprintf(fp0, "   <a href=\"LYNXDIRED://UPLOAD=%d/TO=%s\">",
			 count, curloc);
	    fprintf(fp0, (cur_upload->name ?
			  cur_upload->name : gettext("No Name Given")));
	    fprintf(fp0, "</a>\n");
	}
    } else {
	fprintf(fp0, "   &lt;NONE&gt;\n");
    }

    fprintf(fp0, "</pre>\n");
    EndInternalPage(fp0);
    LYCloseTempFP(fp0);

    LYforce_no_cache = TRUE;

    return(0);
}
