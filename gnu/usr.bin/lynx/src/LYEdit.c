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
    static CONST char *table[] = {
#ifdef VMS
	"sedt",
	"SEDT"
#else
	"emacs",
	"jed",
	"jmacs",
	"joe",
	"jove",
	"jpico",
	"jstar",
	"pico",
	"rjoe",
	"vi"
#endif
    };
    unsigned n;
    for (n = 0; n < TABLESIZE(table); n++) {
	if (strstr(editor, table[n]) != 0) {
	    return TRUE;
	}
    }
    return FALSE;
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
    char *filename = NULL;
#if !(defined(VMS) || defined(USE_DOS_DRIVES))
    char *colon;
#endif
    char *number_sign;
    char position[80];
#if defined(VMS) || defined(CANT_EDIT_UNWRITABLE_FILES)
    FILE *fp;
#endif

    CTRACE((tfp, "edit_current_file(newfile=%s, cur=%d, lineno=%d)\n",
		 newfile, cur, lineno));

    /*
     *  If it's a remote file then we can't edit it.
     */
    if (!LYisLocalFile(newfile)) {
	HTUserMsg(CANNOT_EDIT_REMOTE_FILES);
	return FALSE;
    }

    /*
     *  If there's a fragment, trim it. - FM
     */
    number_sign = trimPoundSelector(newfile);

    /*
     *  On Unix, first try to open it as a completely referenced file,
     *  then via the path alone.
     *
     * On VMS, only try the path.
     */
#if defined (VMS) || defined (USE_DOS_DRIVES)
    filename = HTParse(newfile, "", PARSE_PATH+PARSE_PUNCTUATION);
    HTUnEscape(filename);
    StrAllocCopy(filename, HTSYS_name(filename));
    if (!LYCanReadFile(filename)) {
#ifdef SH_EX
	HTUserMsg2(COULD_NOT_EDIT_FILE, filename);
#else
	HTAlert(COULD_NOT_ACCESS_FILE);
#endif
	CTRACE((tfp, "filename: '%s'\n", filename));
	goto done;
    }
#else	/* something like UNIX */
    if (strncmp(newfile, "file://localhost/", 16) == 0)
	colon = newfile + 16;
    else
	colon = strchr(newfile, ':');
    StrAllocCopy(filename, (colon + 1));
    HTUnEscape(filename);
    if (!LYCanReadFile(filename)) {
	FREE(filename);
	filename = HTParse(newfile, "", PARSE_PATH+PARSE_PUNCTUATION);
	HTUnEscape(filename);
	if (!LYCanReadFile(HTSYS_name(filename))) {
	    HTAlert(COULD_NOT_ACCESS_FILE);
	    goto done;
	}
    }
#endif

#if defined(VMS) || defined(CANT_EDIT_UNWRITABLE_FILES)
    /*
     *  Don't allow editing if user lacks append access.
     */
    if ((fp = fopen(filename, TXT_A)) == NULL)
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

    edit_temporary_file(filename, position, NULL);
    result = TRUE;

done:
    /*
     *  Restore the fragment if there was one. - FM
     */
    restorePoundSelector(number_sign);

    FREE(filename);
    CTRACE((tfp, "edit_current_file returns %d\n", result));
    return (result);
}

PUBLIC void edit_temporary_file ARGS3(
	char *,		filename,
	char *,		position,
	char *,		message)
{
#ifdef UNIX
    struct stat stat_info;
#endif
    char *format = "%s %s";
    char *command = NULL;
    char *editor_arg = "";
    int params = 1;
    int rv;

    if (strstr(editor, "pico")) {
	editor_arg = " -t"; /* No prompt for filename to use */
    }
    if (editor_can_position() && *position) {
#ifdef VMS
	format = "%s %s -%s%s";
	HTAddXpand(&command, format, params++, editor);
	HTAddParam(&command, format, params++, filename);
	HTAddParam(&command, format, params++, position);
	HTAddParam(&command, format, params++, editor_arg);
	HTEndParam(&command, format, params);
#else
	format = "%s +%s%s %s";
	HTAddXpand(&command, format, params++, editor);
	HTAddParam(&command, format, params++, position);
	HTAddParam(&command, format, params++, editor_arg);
	HTAddParam(&command, format, params++, filename);
	HTEndParam(&command, format, params);
#endif
    }
#ifdef DOSPATH
    else if (strncmp(editor, "VZ", 2)==0) {
	/* for Vz editor */
	format = "%s %s -%s";
	HTAddXpand(&command, format, params++, editor);
	HTAddParam(&command, format, params++, HTDOS_short_name(filename));
	HTAddParam(&command, format, params++, position);
	HTEndParam(&command, format, params);
    } else if (strncmp(editor, "edit", 4)==0) {
	/* for standard editor */
	HTAddXpand(&command, format, params++, editor);
	HTAddParam(&command, format, params++, HTDOS_short_name(filename));
	HTEndParam(&command, format, params);
    }
#endif
    else {
#ifdef _WINDOWS
	if (strchr(editor, ' '))
	    HTAddXpand(&command, format, params++, HTDOS_short_name(editor));
	else
	    HTAddXpand(&command, format, params++, editor);
#else
	HTAddXpand(&command, format, params++, editor);
#endif
	HTAddParam(&command, format, params++, filename);
	HTEndParam(&command, format, params);
    }
    if (message != NULL) {
	_statusline(message);
    }

    CTRACE((tfp, "LYEdit: %s\n", command));
    CTRACE_SLEEP(MessageSecs);

    stop_curses();

#ifdef UNIX
    set_errno(0);
#endif
    if ((rv = LYSystem(command)) != 0) {	/* Spawn Editor */
	start_curses();
	/*
	 *  If something went wrong, we should probably return soon;
	 *  currently we don't, but at least put out a message. - kw
	 */
	{
#ifdef UNIX
	    int rvhi = (rv >> 8);
	    CTRACE((tfp, "ExtEditForm: system() returned %d (0x%x), %s\n",
		   rv, rv, errno ? LYStrerror(errno) : "reason unknown"));
	    LYFixCursesOn("show error warning:");
	    if (rv != -1 && (rv && 0xff) && !rvhi) {
		HTAlwaysAlert(NULL, gettext("Editor killed by signal"));
	    } else if (!(rv == -1 || (rvhi == 127 && errno))) {
		HTUserMsg2(gettext("Editor returned with error status, %s"),
			   errno ? LYStrerror(errno) : gettext("reason unknown."));
	    } else
#endif
		HTAlwaysAlert(NULL, ERROR_SPAWNING_EDITOR);
	}
    } else {
	start_curses();
    }
#ifdef UNIX
    /*
     *  Delete backup file, if that's your style.
     */
    HTSprintf0 (&command, "%s~", filename);
    if (stat (command, &stat_info) == 0)
	remove (command);
#endif
    FREE(command);
}
