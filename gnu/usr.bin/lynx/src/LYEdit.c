#include <HTUtils.h>
#include <HTParse.h>
#include <HTAlert.h>
#include <LYCurses.h>
#include <LYUtils.h>
#include <LYGlobalDefs.h>
#include <LYEdit.h>
#ifdef VMS
#include <unixio.h>
#endif /* VMS */

#include <LYLeaks.h>

PUBLIC BOOLEAN editor_can_position NOARGS
{
#ifdef VMS
    return (strstr(editor, "sedt") || strstr(editor, "SEDT"));
#else
    return (strstr(editor, "emacs") || strstr(editor, "vi") ||
	strstr(editor, "pico")  || strstr(editor, "jove")   ||
 	strstr(editor, "jed")   || strstr(editor, "joe")    ||
 	strstr(editor, "jstar") || strstr(editor, "jmacs")  ||
 	strstr(editor, "rjoe")  || strstr(editor, "jpico"));
#endif
}

/*
 *  In edit mode invoke the given (or default) editor to display and edit the
 *  current file.  For editors listed in 'editor_can_position()', Lynx
 *  will open the file to the same line that the screen cursor is on (or
 *  close...) when editing is invoked.
 *
 *  Returns FALSE if file is uneditable.
 */
PUBLIC int edit_current_file ARGS3(
	char *,		newfile,
	int,		cur,
	int,		lineno)
{
    int result = FALSE;
    int params = 1;
    char *format = "%s %s";
    char *command = NULL;
    char *filename = NULL;
    char *colon, *number_sign;
    char position[80];
    FILE *fp;

    /*
     *  If its a remote file then we can't edit it.
     */
    if (!LYisLocalFile(newfile)) {
	HTUserMsg(CANNOT_EDIT_REMOTE_FILES);
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
#if !defined (VMS) && !defined (DOSPATH) && !defined (__EMX__)
    colon = strchr(newfile, ':');
    StrAllocCopy(filename, (colon + 1));
    HTUnEscape(filename);
    if ((fp = fopen(filename, "r")) == NULL) {
	FREE(filename);
#endif /* !VMS */
	filename = HTParse(newfile, "", PARSE_PATH+PARSE_PUNCTUATION);
	HTUnEscape(filename);
	StrAllocCopy(filename, HTSYS_name(filename));
	if ((fp = fopen(filename, "r")) == NULL)
	{
	    HTAlert(COULD_NOT_ACCESS_FILE);
	    CTRACE(tfp, "filename: '%s'\n", filename);
	    goto done;
	}
#if !defined (VMS) && !defined (DOSPATH) && !defined (__EMX__)
    }
#endif /* !VMS */
    fclose(fp);

#if defined(VMS) || defined(CANT_EDIT_UNWRITABLE_FILES)
    /*
     *  Don't allow editing if user lacks append access.
     */
    if ((fp = fopen(filename, "a")) == NULL)
    {
	HTUserMsg(NOAUTH_TO_EDIT_FILE);
	goto done;
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
    *position = 0;
#ifdef VMS
    lineno--;
#endif
    lineno += (nlinks ? links[cur].ly : 0);
    if (lineno > 0)
	sprintf(position, "%d", lineno);

    if (editor_can_position() && *position) {
#ifdef VMS
	format = "%s %s -%s";
	HTAddXpand(&command, format, params++, editor);
	HTAddParam(&command, format, params++, filename);
	HTAddParam(&command, format, params++, position);
	HTEndParam(&command, format, params);
#else
	format = "%s +%s %s";
	HTAddXpand(&command, format, params++, editor);
	HTAddParam(&command, format, params++, position);
	HTAddParam(&command, format, params++, filename);
	HTEndParam(&command, format, params);
#endif
    } else {
	HTAddXpand(&command, format, params++, editor);
	HTAddParam(&command, format, params++, filename);
	HTEndParam(&command, format, params);
    }

    CTRACE(tfp, "LYEdit: %s\n", command);
    CTRACE_SLEEP(MessageSecs);

    /*
     *  Invoke the editor. - FM
     */
    stop_curses();
    LYSystem(command);
    start_curses();

    result = TRUE;

done:
    /*
     *  Restore the fragment if there was one. - FM
     */
    if (number_sign)
	*number_sign = '#';

    FREE(command);
    FREE(filename);
    return (result);
}
