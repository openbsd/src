#include "HTUtils.h"
#include "tcp.h"
#include "HTParse.h"
#include "HTAlert.h"
#include "LYCurses.h"
#include "LYSignal.h"
#include "LYUtils.h"
#include "LYClean.h"
#include "LYGlobalDefs.h"
#include "LYEdit.h"
#include "LYStrings.h"
#include "LYSystem.h"
#ifdef VMS
#include <unixio.h>
#include "HTVMSUtils.h"
#endif /* VMS */
#ifdef DOSPATH
#include "HTDOS.h"
#endif

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

/*
 *  In edit mode invoke either emacs, vi, pico, jove, jed sedt or the
 *  default editor to display and edit the current file.
 *  For emacs, vi, pico, jove and jed, Lynx will open the file to the
 *  same line that the screen cursor is on when editing is invoked.
 *  Returns FALSE if file is uneditable.
 */
PUBLIC int edit_current_file ARGS3(
	char *,		newfile,
	int,		cur,
	int,		lineno)
{
    char command[512];
    char *filename = NULL;
    char *colon, *number_sign;
    FILE *fp;

    /*
     *  If its a remote file then we can't edit it.
     */
    if (!LYisLocalFile(newfile)) {
	_statusline(CANNOT_EDIT_REMOTE_FILES);
	sleep(MessageSecs);
	return FALSE;
    }

    /*
     *  If there's a fragment, trim it. - FM
     */
    number_sign = strchr(newfile, '#');
    if (number_sign)
	*number_sign = '\0';

    /*
     *  On Unix, first try to open it as a completely referenced file,
     *  then via the path alone.
     *
     * On VMS, only try the path.
     */
#if !defined (VMS) && !defined (DOSPATH)
    colon = strchr(newfile, ':');
    StrAllocCopy(filename, (colon + 1));
    HTUnEscape(filename);
    if ((fp = fopen(filename, "r")) == NULL) {
	FREE(filename);
#endif /* !VMS */
	filename = HTParse(newfile, "", PARSE_PATH+PARSE_PUNCTUATION);
	HTUnEscape(filename);
#ifdef DOSPATH
	if (strlen(filename)>1) filename++;
#endif
#ifdef DOSPATH
	if ((fp = fopen(HTDOS_name(filename),"r")) == NULL) {
#else
#ifdef VMS
	if ((fp = fopen(HTVMS_name("", filename), "r")) == NULL) {
#else
	if ((fp = fopen(filename, "r")) == NULL) {
#endif /* VMS */
#endif /* DOSPATH */
	    HTAlert(COULD_NOT_ACCESS_FILE);
	    FREE(filename);
	    goto failure;
	}
#if !defined (VMS) && !defined (DOSPATH)
    }
#endif /* !VMS */
    fclose(fp);

#if defined(VMS) || defined(CANT_EDIT_UNWRITABLE_FILES)
    /*
     *  Don't allow editing if user lacks append access.
     */
#ifdef DOSPATH
    if ((fp = fopen(HTDOS_name("", filename), "a")) == NULL) {
#else
#ifdef VMS
    if ((fp = fopen(HTVMS_name("", filename), "a")) == NULL) {
#else
    if ((fp = fopen(filename, "a")) == NULL) {
#endif /* VMS */
#endif /* DOSPATH */
	_statusline(NOAUTH_TO_EDIT_FILE);
	sleep(MessageSecs);
	goto failure;
    }
    fclose(fp);
#endif /* VMS || CANT_EDIT_UNWRITABLE_FILES */

    /*
     *  Make sure cur is at least zero. - FM
     */
    if (cur < 0) {
	cur = 0;
    }

    /*
     *  Set up the command for the editor. - FM
     */
#ifdef VMS
    if ((strstr(editor, "sedt") || strstr(editor, "SEDT")) &&
	((lineno - 1) + (nlinks ? links[cur].ly : 0)) > 0) {
	sprintf(command, "%s %s -%d",
			 editor,
			 HTVMS_name("", filename),
			 ((lineno - 1) + (nlinks ? links[cur].ly : 0)));
    } else {
	sprintf(command, "%s %s", editor, HTVMS_name("", filename));
    }
#else
    if (strstr(editor, "emacs") || strstr(editor, "vi") ||
	strstr(editor, "pico") || strstr(editor, "jove") ||
	strstr(editor, "jed"))
	sprintf(command, "%s +%d \"%s\"",
			 editor,
			 (lineno + (nlinks ? links[cur].ly : 0)),
#ifdef DOSPATH
			 HTDOS_name(filename));
#else
			 filename);
#endif /* DOSPATH */
    else
#ifdef __DJGPP__
	sprintf(command, "%s %s", editor, HTDOS_name(filename));
#else
	sprintf(command, "%s \"%s\"", editor,
#ifdef DOSPATH
				 HTDOS_name(filename));
#else
				 filename);
#endif /* DOSPATH */
#endif /* __DJGPP__ */
#endif /* VMS */
    if (TRACE) {
	fprintf(stderr, "LYEdit: %s\n", command);
	sleep(MessageSecs);
    }
    FREE(filename);

    /*
     *  Invoke the editor. - FM
     */
    fflush(stderr);
    fflush(stdout);
    stop_curses();
    system(command);
    fflush(stdout);
    fflush(stderr);
    start_curses();

    /*
     *  Restore the fragment if there was one. - FM
     */
    if (number_sign)
	*number_sign = '#';
    return TRUE;

failure:
    /*
     *  Restore the fragment if there was one. - FM
     */
    if (number_sign)
	*number_sign = '#';
    return FALSE;
}
