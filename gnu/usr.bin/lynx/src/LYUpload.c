/*
**  Routines to upload files to the local filesystem.
**  Created by: Rick Mallett, Carleton University
**  Report problems to rmallett@ccs.carleton.ca
**  Modified 15-Dec-95 George Lindholm (lindholm@ucs.ubc.ca):
**	Reread the upload menu page every time, in case the "upload" directory
**	  has changed (make the current directory that for the upload process).
**	Prompt for the upload file name if there is no "%s" in the command
**	  string. Most protocols allow the user to specify the file name
**	  from the client side.  Xmodem appears to be the only that can't
**	  figure out the filename from the transfer data so it needs the
**	  information from lynx (or an upload script which prompts for it).
**	  On the other hand, zmodem aborts when you give it a filename on
**	  the command line (great way of bypassing the nodotfile code :=( ).
*/

#include "HTUtils.h"
#include "tcp.h"
#include "HTParse.h"
#include "HTAlert.h"
#include "LYCurses.h"
#include "LYUtils.h"
#include "LYGlobalDefs.h"
#include "LYSignal.h"
#include "LYStrings.h"
#include "LYClean.h"
#include "LYGetFile.h"
#include "LYUpload.h"
#include "LYSystem.h"
#include "LYLocal.h"

#include "LYexit.h"
#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

PUBLIC char LYUploadFileURL[256] = "\0";

/*
 *  LYUpload uploads a file to a given location using a 
 *  specified upload method.  It parses an incoming link
 *  that looks like:
 *	LYNXDIRED://UPLOAD=<#>/TO=<STRING>
 */
PUBLIC int LYUpload ARGS1(
	char *,		line) 
{
    char *method, *directory, *dir;
    int method_number;
    int count;
    char tmpbuf[256];
    char buffer[256];
    lynx_html_item_type *upload_command = 0;
    int c;
    char *cp;
    FILE *fp;
    char cmd[512];
#ifdef VMS
    extern BOOLEAN HadVMSInterrupt;
#endif /* VMS */

    /*
     *  Use configured upload commands.
     */
    if((directory = (char *)strstr(line, "TO=")) == NULL)
	goto failed;
    *(directory - 1) = '\0';
    /* go past "Directory=" */
    directory+=3;

    if((method = (char *)strstr(line, "UPLOAD=")) == NULL)
	goto failed;
    /*
     *  Go past "Method=".
     */
    method += 7;
    method_number = atoi(method);

    for (count = 0, upload_command = uploaders; count < method_number;
	count++, upload_command = upload_command->next)
      ; /* null body */

    /*
     *  Parsed out the Method and the Location?
     */
    if (upload_command->command == NULL) {
	_statusline("ERROR! - upload command is misconfigured");
	sleep(AlertSecs);
	goto failed;
    }

    /*
     *  Care about the local name?
     */
    if (strstr(upload_command->command, "%s")) {
	/*
	 *  Commands have the form "command %s [etc]"
	 *  where %s is the filename.
	 */
	_statusline("Enter a filename: ");
retry:
	*tmpbuf = '\0';
	if (LYgetstr(tmpbuf, VISIBLE, sizeof(tmpbuf), NORECALL) < 0)
	    goto cancelled;

	if (*tmpbuf == '\0')
	    goto cancelled;

	if (strstr(tmpbuf, "../") != NULL) {
	    _statusline(
		    "Illegal redirection \"../\" found! Request ignored.");
	    sleep(AlertSecs);
	    goto cancelled;
	} else if (strchr(tmpbuf, '/') != NULL) {
	    _statusline("Illegal character \"/\" found! Request ignored.");
	    sleep(AlertSecs);
	    goto cancelled;
	} else if (tmpbuf[0] == '~') {
	    _statusline(
		"Illegal redirection using \"~\" found! Request ignored.");
	    sleep(AlertSecs);
	    goto cancelled;
	}
	sprintf(buffer, "%s/%s", directory, tmpbuf);

	if (no_dotfiles || !show_dotfiles) {
	    if (*buffer == '.' ||
#ifdef VMS
		((cp = strrchr(buffer, ':')) && *(cp+1) == '.') ||
		((cp = strrchr(buffer, ']')) && *(cp+1) == '.') ||
#endif /* VMS */
		((cp = strrchr(buffer, '/')) && *(cp+1) == '.')) {
		_statusline(
		  "File name may not begin with dot. Enter a new filename: ");
		goto retry;
	    }
	}

	/*
	 *  See if it already exists.
	 */
	if ((fp = fopen(buffer, "r")) != NULL) {
	    fclose(fp);

#ifdef VMS
	    _statusline("File exists. Create higher version? (y/n)");
#else
	    _statusline("File exists. Overwrite? (y/n)");
#endif /* VMS */
	    c = 0;
	    while (TOUPPER(c) != 'Y' && TOUPPER(c) != 'N' && c != 7 && c != 3)
		c = LYgetch();
#ifdef VMS
	    if (HadVMSInterrupt) {
		HadVMSInterrupt = FALSE;
		goto cancelled;
	    }
#endif /* VMS */

	    if (c == 7 || c == 3) { /* Control-G or Control-C */
		goto cancelled;
	    }

	    if (TOUPPER(c) == 'N') {
		_statusline("Enter a filename: ");
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
	    _statusline("Cannot write to file. Enter a new filename: ");
	    goto retry;
	}

#ifdef VMS
	sprintf(tmpbuf, upload_command->command, buffer, "", "", "", "", "");
#else
	cp = quote_pathname(buffer); /* to prevent spoofing of the shell */
	sprintf(tmpbuf, upload_command->command, cp, "", "", "", "", "");
	FREE(cp);
#endif /* VMS */
    } else {			/* No substitution, no changes */
	strcpy(tmpbuf, upload_command->command);
    }

    dir = quote_pathname(directory);
    sprintf(cmd, "cd %s ; %s", dir, tmpbuf);
    FREE(dir);
    stop_curses();
    if (TRACE)
	fprintf(stderr, "command: %s\n", cmd);
    system(cmd);
    fflush(stdout);
    start_curses();
#ifdef UNIX 
    chmod(buffer, HIDE_CHMOD);
#endif /* UNIX */ 
    /* don't remove(file); */

    return 1;

failed:
    _statusline("Unable to upload file.");
    sleep(AlertSecs);
    return 0;

cancelled:
    _statusline("Cancelling.");
    sleep(InfoSecs);
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
	char *,		directory)
{
    static char tempfile[256];
    static BOOLEAN first = TRUE;
    FILE *fp0;
    lynx_html_item_type *cur_upload;
    int count;
    static char curloc[256];
    char *cp;

    if (first) {
	/*
	 *  Get an unused tempfile name. - FM
	 */
        tempname(tempfile, NEW_FILE);
#ifdef VMS
    } else {
        remove(tempfile);   /* Remove duplicates on VMS. */
#endif /* VMS */
    }

    /*
     *  Open the tempfile for writing and set it's
     *  protection in case this wasn't done via an
     *  external umask. - FM
     */
    if ((fp0 = LYNewTxtFile(tempfile)) == NULL) {
	HTAlert(CANNOT_OPEN_TEMP);
	return(-1);
    }

#ifdef VMS
    strcpy(curloc, "/sys$login");
#else
    cp = directory;
    if (!strncmp(cp, "file://localhost", 16))
        cp += 16;
    else if (!strncmp(cp, "file:", 5))
        cp += 5;
    strcpy(curloc,cp);
    HTUnEscape(curloc);
    if (curloc[strlen(curloc) - 1] == '/')
        curloc[strlen(curloc) - 1] = '\0';
#endif /* VMS */

    if (first) {
	/*
	 *  Make the tempfile a URL.
 	 */
#if defined (VMS) || defined (DOSPATH)
	sprintf(LYUploadFileURL, "file://localhost/%s", tempfile);
#else
	sprintf(LYUploadFileURL, "file://localhost%s", tempfile);
#endif /* VMS */
	first = FALSE;
    }
    StrAllocCopy(*newfile, LYUploadFileURL);

    fprintf(fp0, "<head>\n<title>%s</title>\n</head>\n<body>\n",
    		 UPLOAD_OPTIONS_TITLE);

    fprintf(fp0, "<h1>Upload Options (%s Version %s)</h1>\n",
    				      LYNX_NAME, LYNX_VERSION);

    fputs("You have the following upload choices.<br>\n", fp0);
    fputs("Please select one:<br>\n<pre>\n", fp0);

    if (uploaders != NULL) {
	for (count = 0, cur_upload = uploaders;
	     cur_upload != NULL; 
	     cur_upload = cur_upload->next, count++) {
	    fprintf(fp0, "   <a href=\"LYNXDIRED://UPLOAD=%d/TO=%s\">",
			 count, curloc);
	    fprintf(fp0, (cur_upload->name ? 
			  cur_upload->name : "No Name Given"));
	    fprintf(fp0, "</a>\n");
	}
    } else {
	fprintf(fp0, "\n   \
No other upload methods have been defined yet.  You may define\n   \
an unlimited number of upload methods using the lynx.cfg file.\n");

    }
    fprintf(fp0, "</pre>\n</body>\n");
    fclose(fp0);

    LYforce_no_cache = TRUE;

    return(0);
}
