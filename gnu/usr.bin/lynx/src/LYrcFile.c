#include <HTUtils.h>
#include <HTFTP.h>
#include <LYUtils.h>
#include <LYrcFile.h>
#include <LYStrings.h>
#include <LYGlobalDefs.h>
#include <LYCharSets.h>
#include <LYBookmark.h>
#include <LYCookie.h>
#include <LYKeymap.h>

#include <LYLeaks.h>

#ifdef FNAMES_8_3
#define FNAME_LYNXRC "lynx.rc"
#else
#define FNAME_LYNXRC ".lynxrc"
#endif /* FNAMES_8_3 */

#define FIND_KEYWORD(cp, keyword) \
    ((cp = LYstrstr(line_buffer, keyword)) != NULL && \
     (cp - line_buffer) < number_sign)

PRIVATE char *SkipEquals ARGS1(char *, src)
{
    char *tmp;
    if ((tmp = (char *)strchr(src, '=')) != NULL)
	src = tmp + 1;
    return LYSkipBlanks(src);
}

PUBLIC void read_rc NOPARAMS
{
    char *line_buffer = NULL;
    char rcfile[LY_MAXPATH];
    FILE *fp;
    char *cp;
    int number_sign;
    char MBM_line[256];
    int  MBM_counter;
    char *MBM_cp2, *MBM_cp1;
    int  MBM_i2;

    /*
     *  Make an RC file name.
     */
    LYAddPathToHome(rcfile, sizeof(rcfile), FNAME_LYNXRC);

    /*
     *  Open the RC file for reading.
     */
    if ((fp = fopen(rcfile, "r")) == NULL) {
	return;
    }

    /*
     *  Process the entries.
     */
    while (LYSafeGets(&line_buffer, fp) != NULL) {
	/*
	 *  Remove any trailing white space.
	 */
	LYTrimTrailing(line_buffer);

	/*
	 *  Skip any comment or blank lines.
	 */
	if (line_buffer[0] == '\0' || line_buffer[0] == '#')
	    continue;

	/*
	 *  Find the line position of the number sign if there is one.
	 */
	if ((cp = (char *)strchr(line_buffer, '#')) == NULL)
	    number_sign = 999;
	else
	    number_sign = cp - line_buffer;

	/*
	 *  File editor.
	 */
	if (!system_editor && FIND_KEYWORD(cp, "file_editor")) {

	    cp = SkipEquals(cp);
	    StrAllocCopy(editor, cp);

	/*
	 *  Default bookmark file.
	 */
	} else if (FIND_KEYWORD(cp, "bookmark_file")) {

	    cp = SkipEquals(cp);

	    /*
	     *  Since this is the "Default Bookmark File", we save it
	     *  as a globals, and as the first MBM_A_subbookmark entry.
	     */
	    StrAllocCopy(bookmark_page, cp);
	    StrAllocCopy(BookmarkPage, cp);
	    StrAllocCopy(MBM_A_subbookmark[0], cp);
	    StrAllocCopy(MBM_A_subdescript[0], MULTIBOOKMARKS_DEFAULT);

	/*
	 *  Multiple (sub)bookmark support settings.
	 */
	} else if (FIND_KEYWORD(cp, "sub_bookmarks")) {

	   cp = SkipEquals(cp);
	   if (!strncasecomp(cp, "standard", 8)) {
	      LYMultiBookmarks = TRUE;
	      LYMBMAdvanced = FALSE;
	   } else if (!strncasecomp(cp, "advanced", 8)) {
	      LYMultiBookmarks = TRUE;
	      LYMBMAdvanced = TRUE;
	   } else {
	      LYMultiBookmarks = FALSE;
	   }

	/*
	 *  Multiple (sub)bookmark definitions and descriptions.
	 */
	} else if (FIND_KEYWORD(cp, "multi_bookmark")) {

	    /*
	     *  Found the root, now cycle through all the
	     *	possible spaces and match specific ones.
	     */
	    for (MBM_counter = 1;
		 MBM_counter <= MBM_V_MAXFILES; MBM_counter++) {
		sprintf(MBM_line, "multi_bookmark%c", (MBM_counter + 'A'));

		if (FIND_KEYWORD(cp, MBM_line)) {
		    if ((MBM_cp1 = (char *)strchr(cp, '=')) == NULL) {
			break;
		    } else {
			if ((MBM_cp2 = (char *)strchr(cp, ',')) == NULL) {
			    break;
			} else {
			    MBM_i2 = 0;
			    /*
			     *  skip over the '='.
			     */
			    MBM_cp1++;
			    while (MBM_cp1 && MBM_cp1 != MBM_cp2) {
				/*
				 *  Skip spaces.
				 */
				if (isspace(*MBM_cp1)) {
				    MBM_cp1++;
				    continue;
				} else {
				    MBM_line[MBM_i2++] = *MBM_cp1++;
				}
			    }
			    MBM_line[MBM_i2++] = '\0';

			    StrAllocCopy(MBM_A_subbookmark[MBM_counter],
					 MBM_line);

			    /*
			     *  Now get the description ',' and ->.
			     */
			    MBM_cp1 = (char *)strchr(cp, ',');

			    MBM_i2 = 0;
			    /*
			     *  Skip over the ','.
			     */
			    MBM_cp1++;
			    /*
			     *  Eat spaces in front of description.
			     */
			    MBM_cp1 = LYSkipBlanks(MBM_cp1);
			    while (*MBM_cp1)
				MBM_line[MBM_i2++] = *MBM_cp1++;
			    MBM_line[MBM_i2++] = '\0';

			    StrAllocCopy(MBM_A_subdescript[MBM_counter],
					 MBM_line);

			    break;
			}
		    }
		}
	    }

	/*
	 *  FTP/file sorting method.
	 */
	} else if (FIND_KEYWORD(cp, "file_sorting_method")) {

	   cp = SkipEquals(cp);
	   if (!strncasecomp(cp, "BY_FILENAME", 11))
		HTfileSortMethod = FILE_BY_NAME;
	   else if (!strncasecomp(cp, "BY_TYPE", 7))
		HTfileSortMethod = FILE_BY_TYPE;
	   else if (!strncasecomp(cp, "BY_SIZE", 7))
		HTfileSortMethod = FILE_BY_SIZE;
	   else if (!strncasecomp(cp, "BY_DATE", 7))
		HTfileSortMethod = FILE_BY_DATE;

	/*
	 *  Personal mail address.
	 */
	} else if (FIND_KEYWORD(cp, "personal_mail_address")) {

	    cp = SkipEquals(cp);
	    StrAllocCopy(personal_mail_address, cp);

	/*
	 *  Searching type.
	 */
	} else if (FIND_KEYWORD(cp, "case_sensitive_searching")) {

	    cp = SkipEquals(cp);
	    if (!strncasecomp(cp, "on", 2))
		case_sensitive = TRUE;
	    else
		case_sensitive = FALSE;

	/*
	 *  Character set.
	 */
	} else if (FIND_KEYWORD(cp, "character_set")) {

	    int i = 0;

	    cp = SkipEquals(cp);

	    i = UCGetLYhndl_byAnyName(cp); /* by MIME or full name */
	    if (i < 0)
		; /* do nothing here: so fallback to lynx.cfg */
	    else
		current_char_set = i;

	/*
	 *  Preferred language.
	 */
	} else if (FIND_KEYWORD(cp, "preferred_language")) {

	    cp = SkipEquals(cp);
	    StrAllocCopy(language, cp);

	/*
	 *  Preferred charset.
	 */
	} else if (FIND_KEYWORD(cp, "preferred_charset")) {

	    cp = SkipEquals(cp);
	    StrAllocCopy(pref_charset, cp);

	/*
	 *  VI keys.
	 */
	} else if (FIND_KEYWORD(cp, "vi_keys")) {

	    cp = SkipEquals(cp);
	    if (!strncasecomp(cp, "on", 2))
		vi_keys = TRUE;
	    else
		vi_keys = FALSE;

	/*
	 *  EMACS keys.
	 */
	} else if (FIND_KEYWORD(cp, "emacs_keys")) {

	    cp = SkipEquals(cp);
	    if (!strncasecomp(cp, "on", 2))
		emacs_keys = TRUE;
	    else
		emacs_keys=FALSE;

	/*
	 *  Show dot files.
	 */
	} else if (FIND_KEYWORD(cp, "show_dotfiles")) {

	    cp = SkipEquals(cp);
	    if (!strncasecomp(cp, "on", 2))
		show_dotfiles = TRUE;
	    else
		show_dotfiles = FALSE;

	/*
	 *  Show color.
	 */
	} else if (FIND_KEYWORD(cp, "show_color")) {

	    cp = SkipEquals(cp);
	    if (!strncasecomp(cp, "always", 6)) {
		LYrcShowColor = SHOW_COLOR_ALWAYS;
#if defined(USE_SLANG) || defined(COLOR_CURSES)
		if (LYShowColor != SHOW_COLOR_NEVER)
		    LYShowColor = SHOW_COLOR_ALWAYS;
#endif /* USE_SLANG || COLOR_CURSES */
	    } else if (!strncasecomp(cp, "never", 5)) {
		LYrcShowColor = SHOW_COLOR_NEVER;
#if defined(COLOR_CURSES)
		if (LYShowColor == SHOW_COLOR_ON)
		    LYShowColor = SHOW_COLOR_OFF;
#endif /* COLOR_CURSES */
	    }

	/*
	 *  Select popups.
	 */
	} else if (FIND_KEYWORD(cp, "select_popups")) {

	    cp = SkipEquals(cp);
	    if (!strncasecomp(cp, "off", 3))
		LYSelectPopups = FALSE;
	    else
		LYSelectPopups = TRUE;

	/*
	 *  Show cursor.
	 */
	} else if (FIND_KEYWORD(cp, "show_cursor")) {

	    cp = SkipEquals(cp);
	    if (!strncasecomp(cp, "off", 3))
		LYShowCursor = FALSE;
	    else
		LYShowCursor = TRUE;

	/*
	 *  Keypad mode.
	 */
	} else if (FIND_KEYWORD(cp, "keypad_mode")) {

	    cp = SkipEquals(cp);
	    if (LYstrstr(cp, "LINKS_ARE_NUMBERED"))
		keypad_mode = LINKS_ARE_NUMBERED;
	    else if (LYstrstr(cp, "LINKS_AND_FORM_FIELDS_ARE_NUMBERED"))
		keypad_mode = LINKS_AND_FORM_FIELDS_ARE_NUMBERED;
	    else
		keypad_mode = NUMBERS_AS_ARROWS;

	/*
	 *  Keyboard layout.
	 */
#ifdef EXP_KEYBOARD_LAYOUT
	} else if (FIND_KEYWORD(cp, "kblayout")) {

	    int i = 0;

	    cp = SkipEquals(cp);
	    for (; LYKbLayoutNames[i]; i++) {
		if (!strcmp(cp, LYKbLayoutNames[i])) {
		    current_layout = i;
		    break;
		}
	    }
#endif /* EXP_KEYBOARD_LAYOUT */

	/*
	 *  Line edit mode.
	 */
	} else if (FIND_KEYWORD(cp, "lineedit_mode")) {

	    int i = 0;

	    cp = SkipEquals(cp);
	    for (; LYLineeditNames[i]; i++) {
		if (!strncmp(cp, LYLineeditNames[i], strlen(cp))) {
		    current_lineedit = i;
		    break;
		}
	    }

#ifdef DIRED_SUPPORT
	/*
	 *  Directory list style.
	 */
	} else if (FIND_KEYWORD(cp, "dir_list_style")) {

	    cp = SkipEquals(cp);
	    if (LYstrstr(cp, "FILES_FIRST") != NULL) {
		dir_list_style = FILES_FIRST;
	    } else if (LYstrstr(cp,"DIRECTORIES_FIRST") != NULL) {
		dir_list_style = 0;
	    } else {
		dir_list_style = MIXED_STYLE;
	    }
#endif /* DIRED_SUPPORT */

	/*
	 *  Accept cookies from all domains?
	 */
	} else if (FIND_KEYWORD(cp, "accept_all_cookies")) {
	    cp = SkipEquals(cp);
	    if (LYstrstr(cp,"TRUE") != NULL) {
		LYAcceptAllCookies = TRUE;
	    } else {
		LYAcceptAllCookies = FALSE;
	    }


	/*
	 *  Accept all cookies from certain domains?
	 */
	} else if (FIND_KEYWORD(cp, "cookie_accept_domains")) {
	    cp = SkipEquals(cp);
	    cookie_domain_flag_set(cp, FLAG_ACCEPT_ALWAYS);
	    if(LYCookieAcceptDomains != NULL) {
		StrAllocCat(LYCookieAcceptDomains, ",");
	    }
	    StrAllocCat(LYCookieAcceptDomains, cp);


	/*
	 *  Reject all cookies from certain domains?
	 */
	} else if (FIND_KEYWORD(cp, "cookie_reject_domains")) {
	    cp = SkipEquals(cp);
	    cookie_domain_flag_set(cp, FLAG_REJECT_ALWAYS);
	    if(LYCookieRejectDomains != NULL) {
		StrAllocCat(LYCookieRejectDomains, ",");
	    }
	    StrAllocCat(LYCookieRejectDomains, cp);

	/*
	 *  Cookie domains to perform loose checks?
	 */
	} else if (FIND_KEYWORD(cp, "cookie_loose_invalid_domains")) {
	    cp = SkipEquals(cp);
	    StrAllocCopy(LYCookieLooseCheckDomains, cp);
	    cookie_domain_flag_set(cp, FLAG_INVCHECK_LOOSE);

	/*
	 *  Cookie domains to perform strict checks?
	 */
	} else if (FIND_KEYWORD(cp, "cookie_strict_invalid_domains")) {
	    cp = SkipEquals(cp);
	    StrAllocCopy(LYCookieStrictCheckDomains, cp);
	    cookie_domain_flag_set(cp, FLAG_INVCHECK_STRICT);

	/*
	 *  Cookie domains to query user over invalid cookies?
	 */
	} else if (FIND_KEYWORD(cp, "cookie_query_invalid_domains")) {
	    cp = SkipEquals(cp);
	    StrAllocCopy(LYCookieQueryCheckDomains, cp);
	    cookie_domain_flag_set(cp, FLAG_INVCHECK_QUERY);

#ifdef EXP_PERSISTENT_COOKIES
	/*
	 *  File in which to store persistent cookies.
	 */
	} else if (FIND_KEYWORD(cp, "cookie_file")) {
	    cp = SkipEquals(cp);
	    StrAllocCopy(LYCookieFile, cp);
#endif /* EXP_PERSISTENT_COOKIES */

	/*
	 *  User mode.
	 */
	} else if (FIND_KEYWORD(cp, "user_mode")) {

	    cp = SkipEquals(cp);
	    if (LYstrstr(cp, "ADVANCED") != NULL) {
		user_mode = ADVANCED_MODE;
	    } else if (LYstrstr(cp,"INTERMEDIATE") != NULL) {
		user_mode = INTERMEDIATE_MODE;
	    } else {
		user_mode = NOVICE_MODE;
	    }

#ifdef NOTUSED
#ifdef DISP_PARTIAL
	/*
	 *  Partial display logic--set the threshold # of lines before
	 *  Lynx redraws the screen
	 */
	} else if (FIND_KEYWORD(cp, "partial_thres")) {
	    cp = SkipEquals(cp);
	    if (atoi(cp) != 0)
		partial_threshold = atoi(cp);
#endif /* DISP_PARTIAL */
#endif /* NOTUSED */

#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
	/*
	 *  Local execution mode - all links.
	 */
	} else if (FIND_KEYWORD(cp, "run_all_execution_links")) {

	    cp = SkipEquals(cp);
	    if (!strncasecomp(cp, "on", 2))
		local_exec = TRUE;
	     else
		local_exec = FALSE;

	/*
	 *  Local execution mode - only links in local files.
	 */
	} else if (FIND_KEYWORD(cp, "run_execution_links_on_local_files")) {
	    cp = SkipEquals(cp);
	    if (!strncasecomp(cp, "on", 2))
		local_exec_on_local_files = TRUE;
	    else
		local_exec_on_local_files=FALSE;
#endif /* ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS */

	} else if (FIND_KEYWORD(cp, "verbose_images")) {
	   cp = SkipEquals(cp);
	   if (!strncasecomp(cp, "on", 2))
		verbose_img = 1;
	   else if (!strncasecomp(cp, "off", 3))
		verbose_img = 0;

	} /* end of if */

    } /* end of while */

    fclose(fp);
} /* big end */

/*
 * Write a set of comments.  Doing it this way avoids preprocessor problems
 * with the leading '#', makes it simpler to use gettext.
 */
PRIVATE void write_list ARGS2(
    	FILE *,		fp,
	char *,		list)
{
    int first = TRUE;
    while (*list != 0) {
	int ch = *list++;
	if (ch == '\n') {
	    first = TRUE;
	} else {
	    if (first) {
		fputs("# ", fp);
		first = FALSE;
	    }
	}
	fputc(ch, fp);
    }
}

PUBLIC int save_rc NOPARAMS
{
    char rcfile[LY_MAXPATH];
    FILE *fp;
    int i;
    int MBM_c;

    /*
     *  Make a name.
     */
    LYAddPathToHome(rcfile, sizeof(rcfile), FNAME_LYNXRC);

    /*
     *  Open the file for write.
     */
    if ((fp = LYNewTxtFile(rcfile)) == NULL) {
	return FALSE;
    }

    /*
     *  Header.
     */
    write_list(fp, gettext("\
Lynx User Defaults File\n\
\n\
This file contains options saved from the Lynx Options Screen (normally\n\
with the '>' key).  There is normally no need to edit this file manually,\n\
since the defaults here can be controlled from the Options Screen, and the\n\
next time options are saved from the Options Screen this file will be\n\
completely rewritten.  You have been warned...\n\
If you are looking for the general configuration file - it is normally\n\
called lynx.cfg, and it has different content and a different format.\n\
It is not this file.\n\
"));
    fprintf(fp, "\n");

    /*
     *  File editor
     */
    write_list(fp, gettext("\
file_editor specifies the editor to be invoked when editing local files\n\
or sending mail.  If no editor is specified, then file editing is disabled\n\
unless it is activated from the command line, and the built-in line editor\n\
will be used for sending mail.\n\
"));
    fprintf(fp, "file_editor=%s\n\n", (editor ? editor : ""));

    /*
     *  Default bookmark file.
     */
    write_list(fp, gettext("\
bookmark_file specifies the name and location of the default bookmark\n\
file into which the user can paste links for easy access at a later\n\
date.\n\
"));
    fprintf(fp, "bookmark_file=%s\n\n", (bookmark_page ? bookmark_page : ""));

    /*
     *  Multiple (sub)bookmark support settings.
     */
    write_list(fp, gettext("\
If sub_bookmarks is not turned \"off\", and multiple bookmarks have\n\
been defined (see below), then all bookmark operations will first\n\
prompt the user to select an active sub-bookmark file.  If the default\n\
Lynx bookmark_file is defined (see above), it will be used as the\n\
default selection.  When this option is set to \"advanced\", and the\n\
user mode is advanced, the 'v'iew bookmark command will invoke a\n\
statusline prompt instead of the menu seen in novice and intermediate\n\
user modes.  When this option is set to \"standard\", the menu will be\n\
presented regardless of user mode.\n\
"));
    fprintf(fp, "sub_bookmarks=%s\n\n", (LYMultiBookmarks ?
					   (LYMBMAdvanced ?
					       "advanced" : "standard")
							  : "off"));

    /*
     *  Multiple (sub)bookmark definitions and descriptions.
     */
    write_list(fp, gettext("\
The following allow you to define sub-bookmark files and descriptions.\n\
The format is multi_bookmark<capital_letter>=<filename>,<description>\n\
Up to 26 bookmark files (for the English capital letters) are allowed.\n\
We start with \"multi_bookmarkB\" since 'A' is the default (see above).\n\
"));
    for (MBM_c = 1; MBM_c <= MBM_V_MAXFILES; MBM_c++)
       fprintf(fp, "multi_bookmark%c=%s%s%s\n",
		   (MBM_c + 'A'),
		   (MBM_A_subbookmark[MBM_c] ?
		    MBM_A_subbookmark[MBM_c] : ""),
		   (MBM_A_subbookmark[MBM_c] ?
					 "," : ""),
		   (MBM_A_subdescript[MBM_c] ?
		    MBM_A_subdescript[MBM_c] : ""));
    fprintf(fp, "\n");

    /*
     *  FTP/file sorting method.
     */
    write_list(fp, gettext("\
The file_sorting_method specifies which value to sort on when viewing\n\
file lists such as FTP directories.  The options are:\n\
   BY_FILENAME -- sorts on the name of the file\n\
   BY_TYPE     -- sorts on the type of the file\n\
   BY_SIZE     -- sorts on the size of the file\n\
   BY_DATE     -- sorts on the date of the file\n\
"));
    fprintf(fp, "file_sorting_method=%s\n\n",
		(HTfileSortMethod == FILE_BY_NAME ? "BY_FILENAME"
						  :
		(HTfileSortMethod == FILE_BY_SIZE ? "BY_SIZE"
						  :
		(HTfileSortMethod == FILE_BY_TYPE ? "BY_TYPE"
						  : "BY_DATE"))));

    /*
     *  Personal mail address.
     */
    write_list(fp, gettext("\
personal_mail_address specifies your personal mail address.  The\n\
address will be sent during HTTP file transfers for authorization and\n\
logging purposes, and for mailed comments.\n\
If you do not want this information given out, set the NO_FROM_HEADER\n\
to TRUE in lynx.cfg, or use the -nofrom command line switch.  You also\n\
could leave this field blank, but then you won't have it included in\n\
your mailed comments.\n\
"));
    fprintf(fp, "personal_mail_address=%s\n\n",
		(personal_mail_address ? personal_mail_address : ""));

    /*
     *  Searching type.
     */
    write_list(fp, gettext("\
If case_sensitive_searching is \"on\" then when the user invokes a search\n\
using the 's' or '/' keys, the search performed will be case sensitive\n\
instead of case INsensitive.  The default is usually \"off\".\n\
"));
    fprintf(fp, "case_sensitive_searching=%s\n\n",
		(case_sensitive ? "on" : "off"));

    /*
     *  Character set.
     */
    write_list(fp, gettext("\
The character_set definition controls the representation of 8 bit\n\
characters for your terminal.  If 8 bit characters do not show up\n\
correctly on your screen you may try changing to a different 8 bit\n\
set or using the 7 bit character approximations.\n\
Current valid characters sets are:\n\
"));
    for (i = 0; LYchar_set_names[i]; i++)
	fprintf(fp, "#    %s\n", LYchar_set_names[i]);
    fprintf(fp, "character_set=%s\n\n", LYchar_set_names[current_char_set]);


    /*
     *  Preferred language.
     */
    write_list(fp, gettext("\
preferred_language specifies the language in MIME notation (e.g., en,\n\
fr, may be a comma-separated list in decreasing preference)\n\
which Lynx will indicate you prefer in requests to http servers.\n\
If a file in that language is available, the server will send it.\n\
Otherwise, the server will send the file in it's default language.\n\
"));
    fprintf(fp, "preferred_language=%s\n\n", (language ? language : ""));

    /*
     *  Preferred charset.
     */
    write_list(fp, gettext("\
preferred_charset specifies the character set in MIME notation (e.g.,\n\
ISO-8859-2, ISO-8859-5) which Lynx will indicate you prefer in requests\n\
to http servers using an Accept-Charset header.  The value should NOT\n\
include ISO-8859-1 or US-ASCII, since those values are always assumed\n\
by default.  May be a comma-separated list.\n\
If a file in that character set is available, the server will send it.\n\
If no Accept-Charset header is present, the default is that any\n\
character set is acceptable.  If an Accept-Charset header is present,\n\
and if the server cannot send a response which is acceptable\n\
according to the Accept-Charset header, then the server SHOULD send\n\
an error response, though the sending of an unacceptable response\n\
is also allowed.\n\
"));
    fprintf(fp, "preferred_charset=%s\n\n",
		(pref_charset ? pref_charset : ""));

    /*
     *  Show color.
     */
    if (LYChosenShowColor != SHOW_COLOR_UNKNOWN) {
	write_list(fp, gettext("\
show_color specifies how to set the color mode at startup.  A value of\n\
\"never\" will force color mode off (treat the terminal as monochrome)\n\
at startup even if the terminal appears to be color capable.  A value of\n\
\"always\" will force color mode on even if the terminal appears to be\n\
monochrome, if this is supported by the library used to build lynx.\n\
A value of \"default\" will yield the behavior of assuming\n\
a monochrome terminal unless color capability is inferred at startup\n\
based on the terminal type, or the -color command line switch is used, or\n\
the COLORTERM environment variable is set.  The default behavior always is\n\
used in anonymous accounts or if the \"option_save\" restriction is set.\n\
The effect of the saved value can be overridden via\n\
the -color and -nocolor command line switches.\n\
The mode set at startup can be changed via the \"show color\" option in\n\
the 'o'ptions menu.  If the option settings are saved, the \"on\" and\n\
\"off\" \"show color\" settings will be treated as \"default\".\n\
"));
     fprintf(fp, "show_color=%s\n\n",
	     ((LYChosenShowColor == SHOW_COLOR_NEVER  ? "never"  :
	       (LYChosenShowColor == SHOW_COLOR_ALWAYS ? "always" :
						      "default"))));
    }

    /*
     *  VI keys.
     */
    write_list(fp, gettext("\
If vi_keys is set to \"on\", then the normal VI movement keys:\n\
  j = down    k = up\n\
  h = left    l = right\n\
will be enabled.  These keys are only lower case.\n\
Capital 'H', 'J' and 'K will still activate help, jump shortcuts,\n\
and the keymap display, respectively.\n\
"));
     fprintf(fp, "vi_keys=%s\n\n", (vi_keys ? "on" : "off"));

    /*
     *  EMACS keys.
     */
    write_list(fp, gettext("\
If emacs_keys is to \"on\" then the normal EMACS movement keys:\n\
  ^N = down    ^P = up\n\
  ^B = left    ^F = right\n\
will be enabled.\n\
"));
    fprintf(fp, "emacs_keys=%s\n\n", (emacs_keys ? "on" : "off"));

    /*
     *  Show dot files.
     */
    write_list(fp, gettext("\
show_dotfiles specifies that the directory listing should include\n\
\"hidden\" (dot) files/directories.  If set \"on\", this will be\n\
honored only if enabled via userdefs.h and/or lynx.cfg, and not\n\
restricted via a command line switch.  If display of hidden files\n\
is disabled, creation of such files via Lynx also is disabled.\n\
"));
    fprintf(fp, "show_dotfiles=%s\n\n", (show_dotfiles ? "on" : "off"));

    /*
     *  Select popups.
     */
    write_list(fp, gettext("\
select_popups specifies whether the OPTIONs in a SELECT block which\n\
lacks a MULTIPLE attribute are presented as a vertical list of radio\n\
buttons or via a popup menu.  Note that if the MULTIPLE attribute is\n\
present in the SELECT start tag, Lynx always will create a vertical list\n\
of checkboxes for the OPTIONs.  A value of \"on\" will set popup menus\n\
as the default while a value of \"off\" will set use of radio boxes.\n\
The default can be overridden via the -popup command line toggle.\n\
"));
    fprintf(fp, "select_popups=%s\n\n", (LYSelectPopups ? "on" : "off"));

    /*
     *  Show cursor.
     */
    write_list(fp, gettext("\
show_cursor specifies whether to 'hide' the cursor to the right (and\n\
bottom, if possible) of the screen, or to place it to the left of the\n\
current link in documents, or current option in select popup windows.\n\
Positioning the cursor to the left of the current link or option is\n\
helpful for speech or braille interfaces, and when the terminal is\n\
one which does not distinguish the current link based on highlighting\n\
or color.  A value of \"on\" will set positioning to the left as the\n\
default while a value of \"off\" will set 'hiding' of the cursor.\n\
The default can be overridden via the -show_cursor command line toggle.\n\
"));
    fprintf(fp, "show_cursor=%s\n\n", (LYShowCursor ? "on" : "off"));

    /*
     *  Keypad mode.
     */
    write_list(fp, gettext("\
If keypad_mode is set to \"NUMBERS_AS_ARROWS\", then the numbers on\n\
your keypad when the numlock is on will act as arrow keys:\n\
            8 = Up Arrow\n\
  4 = Left Arrow    6 = Right Arrow\n\
            2 = Down Arrow\n\
and the corresponding keyboard numbers will act as arrow keys,\n\
regardless of whether numlock is on.\n\
"));
    write_list(fp, gettext("\
If keypad_mode is set to \"LINKS_ARE_NUMBERED\", then numbers will\n\
appear next to each link and numbers are used to select links.\n\
"));
    write_list(fp, gettext("\
If keypad_mode is set to \"LINKS_AND_FORM_FIELDS_ARE_NUMBERED\", then\n\
numbers will appear next to each link and visible form input field.\n\
Numbers are used to select links, or to move the \"current link\" to a\n\
form input field or button.  In addition, options in popup menus are\n\
indexed so that the user may type an option number to select an option in\n\
a popup menu, even if the option isn't visible on the screen.  Reference\n\
lists and output from the list command also enumerate form inputs.\n\
"));
    write_list(fp, gettext("\
NOTE: Some fixed format documents may look disfigured when\n\
\"LINKS_ARE_NUMBERED\" or \"LINKS_AND_FORM_FIELDS_ARE_NUMBERED\" are\n\
enabled.\n\
"));
    fprintf(fp, "keypad_mode=%s\n\n",
		((keypad_mode == NUMBERS_AS_ARROWS) ?  "NUMBERS_AS_ARROWS" :
	       ((keypad_mode == LINKS_ARE_NUMBERED) ? "LINKS_ARE_NUMBERED" :
				      "LINKS_AND_FORM_FIELDS_ARE_NUMBERED")));

#ifdef NOTUSED
#ifdef DISP_PARTIAL
    /*
     *  Partial display threshold
     */
    write_list(fp, gettext("\
partial_thres specifies the number of lines Lynx should download and render\n\
before we redraw the screen in Partial Display logic\n\
e.g., partial_thres=2\n\
would have Lynx redraw every 2 lines that it renders\n\
partial_thres=-1 would use the entire screensize\n\
"));
    fprintf(fp, "partial_thres=%d\n\n", partial_threshold);
#endif /* DISP_PARTIAL */
#endif /* NOTUSED */

    /*
     *  Line edit mode.
     */
    write_list(fp, gettext("\
lineedit_mode specifies the key binding used for inputting strings in\n\
prompts and forms.  If lineedit_mode is set to \"Default Binding\" then\n\
the following control characters are used for moving and deleting:\n\
\n\
             Prev  Next       Enter = Accept input\n\
   Move char: <-    ->        ^G    = Cancel input\n\
   Move word: ^P    ^N        ^U    = Erase line\n\
 Delete char: ^H    ^R        ^A    = Beginning of line\n\
 Delete word: ^B    ^F        ^E    = End of line\n\
\n\
Current lineedit modes are:\n\
"));
    {
	char **bindings = LYLineeditNames;
	while (*bindings) {
	    fprintf(fp, "#    %s\n", *bindings);
	    bindings++;
	}
    }
    fprintf(fp, "lineedit_mode=%s\n\n", LYLineeditNames[current_lineedit]);
#ifdef EXP_KEYBOARD_LAYOUT
    fprintf(fp, "kblayout=%s\n\n", LYKbLayoutNames[current_layout]);
#endif

#ifdef DIRED_SUPPORT
    /*
     *  Directory list style.
     */
    write_list(fp, gettext("\
dir_list_styles specifies the directory list style under DIRED_SUPPORT\n\
(if implemented).  The default is \"MIXED_STYLE\", which sorts both\n\
files and directories together.  \"FILES_FIRST\" lists files first and\n\
\"DIRECTORIES_FIRST\" lists directories first.\n\
"));
    fprintf(fp, "dir_list_style=%s\n\n",
		(dir_list_style==FILES_FIRST ? "FILES_FIRST"
					     :
		(dir_list_style==MIXED_STYLE ? "MIXED_STYLE"
					     : "DIRECTORIES_FIRST")));
#endif /* DIRED_SUPPORT */

    /*
     *  User mode.
     */
    write_list(fp, gettext("\
user_mode specifies the users level of knowledge with Lynx.  The\n\
default is \"NOVICE\" which displays two extra lines of help at the\n\
bottom of the screen to aid the user in learning the basic Lynx\n\
commands.  Set user_mode to \"INTERMEDIATE\" to turn off the extra info.\n\
Use \"ADVANCED\" to see the URL of the currently selected link at the\n\
bottom of the screen.\n\
"));
    fprintf(fp, "user_mode=%s\n\n",
		(user_mode == NOVICE_MODE ? "NOVICE" :
			 (user_mode == ADVANCED_MODE ?
					  "ADVANCED" : "INTERMEDIATE")));

    /*
     *  Cookie options
     */
    write_list(fp, gettext("\
accept_all_cookies allows the user to tell Lynx to automatically\n\
accept all cookies if desired.  The default is \"FALSE\" which will\n\
prompt for each cookie.  Set accept_all_cookies to \"TRUE\" to accept\n\
all cookies.\n\
"));
    fprintf(fp, "accept_all_cookies=%s\n\n",
		(LYAcceptAllCookies == FALSE ? "FALSE" : "TRUE"));

    write_list(fp, gettext("\
cookie_accept_domains and cookie_reject_domains are comma-delimited\n\
lists of domains from which Lynx should automatically accept or reject\n\
all cookies.  If a domain is specified in both options, rejection will\n\
take precedence.  The accept_all_cookies parameter will override any\n\
settings made here.\n\
"));
    fprintf(fp, "cookie_accept_domains=%s\n",
		    (LYCookieAcceptDomains == NULL ? ""
		    : LYCookieAcceptDomains));
    fprintf(fp, "cookie_reject_domains=%s\n\n",
		    (LYCookieRejectDomains == NULL ? ""
		    : LYCookieRejectDomains));


    write_list(fp, gettext("\
cookie_loose_invalid_domains, cookie_strict_invalid_domains, and\n\
cookie_query_invalid_domains are comma-delimited lists of which domains\n\
should be subjected to varying degrees of validity checking.  If a\n\
domain is set to strict checking, strict conformance to RFC2109 will\n\
be applied.  A domain with loose checking will be allowed to set cookies\n\
with an invalid path or domain attribute.  All domains will default to\n\
querying the user for an invalid path or domain.\n\
"));
    fprintf(fp, "cookie_loose_invalid_domains=%s\n",
	    (LYCookieLooseCheckDomains == NULL) ? ""
		    : LYCookieLooseCheckDomains);
    fprintf(fp, "cookie_strict_invalid_domains=%s\n",
	    (LYCookieStrictCheckDomains == NULL) ? ""
		    : LYCookieStrictCheckDomains);
    fprintf(fp, "cookie_query_invalid_domains=%s\n\n",
	    (LYCookieQueryCheckDomains == NULL) ? ""
		    : LYCookieQueryCheckDomains);


#ifdef EXP_PERSISTENT_COOKIES
    /*
     *  Cookie file.
     */
    write_list(fp, gettext("\
cookie_file specifies the file in which to store persistent cookies.\n\
The default is ~/.lynx_cookies.\n\
"));
    fprintf(fp, "cookie_file=%s\n\n",
		(LYCookieFile == NULL ? "~/.lynx_cookies" : LYCookieFile));
#endif /* EXP_PERSISTENT_COOKIES */



#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
    /*
     *  Local execution mode - all links.
     */
    write_list(fp, gettext("\
If run_all_execution_links is set \"on\" then all local execution links\n\
will be executed when they are selected.\n\
\n\
WARNING - This is potentially VERY dangerous.  Since you may view\n\
          information that is written by unknown and untrusted sources\n\
          there exists the possibility that Trojan horse links could be\n\
          written.  Trojan horse links could be written to erase files\n\
          or compromise security.  This should only be set to \"on\" if\n\
          you are viewing trusted source information.\n\
"));
    fprintf(fp, "run_all_execution_links=%s\n\n",
		(local_exec ? "on" : "off"));

    /*
     *  Local execution mode - only links in local files.
     */
    write_list(fp, gettext("\
If run_execution_links_on_local_files is set \"on\" then all local\n\
execution links that are found in LOCAL files will be executed when they\n\
are selected.  This is different from run_all_execution_links in that\n\
only files that reside on the local system will have execution link\n\
permissions.\n\
\n\
WARNING - This is potentially dangerous.  Since you may view\n\
          information that is written by unknown and untrusted sources\n\
          there exists the possibility that Trojan horse links could be\n\
          written.  Trojan horse links could be written to erase files\n\
          or compromise security.  This should only be set to \"on\" if\n\
          you are viewing trusted source information.\n\
"));
    fprintf(fp, "run_execution_links_on_local_files=%s\n\n",
		(local_exec_on_local_files ? "on" : "off"));
#endif /* defined(EXEC_LINKS) || defined(EXEC_SCRIPTS) */

    write_list(fp, gettext("\
If verbose_images is \"on\", lynx will print the name of the image\n\
source file in place of [INLINE], [LINK] or [IMAGE]\n\
See also VERBOSE_IMAGES in lynx.cfg\n\
"));
    fprintf(fp, "verbose_images=%s\n\n",
		verbose_img ? "on" : "off");

    /*
     *  Close the RC file.
     */
    fclose(fp);

    HTSYS_purge(rcfile);

    return TRUE;
}
