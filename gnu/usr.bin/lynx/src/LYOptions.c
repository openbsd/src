#include "HTUtils.h"
#include "tcp.h"
#include "HTFTP.h"
#include "HTML.h"
#include "LYCurses.h"
#include "LYUtils.h"
#include "LYStrings.h"
#include "LYGlobalDefs.h"
#include "LYOptions.h"
#include "LYSignal.h"
#include "LYClean.h"
#include "LYCharSets.h"
#include "LYCharUtils.h"
#include "UCMap.h"
#include "UCAux.h"
#include "LYKeymap.h"
#include "LYrcFile.h"
#include "HTAlert.h"
#include "LYBookmark.h"
#include "GridText.h"

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

#ifdef VMS
#define DISPLAY "DECW$DISPLAY"
#else
#define DISPLAY "DISPLAY"
#endif /* VMS */

#define COL_OPTION_VALUES 36  /* display column where option values start */

BOOLEAN term_options;
PRIVATE void terminate_options	PARAMS((int sig));
PRIVATE int boolean_choice PARAMS((
	int		status,
	int		line,
	int		column,
	char ** 	choices));
PRIVATE int popup_choice PARAMS((
	int		cur_choice,
	int		line,
	int		column,
	char ** 	choices,
	int		i_length,
	int		disabled));

#define MAXCHOICES 10

#define L_Bool_A (use_assume_charset ? L_BOOL_A + 1 : L_BOOL_A)
#define L_Bool_B (use_assume_charset ? L_BOOL_B + 1 : L_BOOL_B)
#define L_Exec (use_assume_charset ? L_EXEC + 1 : L_EXEC)
#define L_Rawmode (use_assume_charset ? L_RAWMODE + 1 : L_RAWMODE)
#define L_Charset (use_assume_charset ? L_CHARSET + 1 : L_CHARSET)
#define L_Color (use_assume_charset ? L_COLOR + 1 : L_COLOR)
#define L_Keypad (use_assume_charset ? L_KEYPAD + 1 : L_KEYPAD)
#define L_Lineed (use_assume_charset ? L_LINEED + 1 : L_LINEED)
#define L_Dired (use_assume_charset ? L_DIRED + 1 : L_DIRED)
#define L_User_Mode (use_assume_charset ? L_USER_MODE + 1 : L_USER_MODE)
#define L_User_Agent (use_assume_charset ? L_USER_AGENT + 1 : L_USER_AGENT)

PRIVATE void option_statusline ARGS1(
	CONST char *,		text)
{
    /*
     *	Make sure we have a pointer to a string.
     */
    if (text == NULL)
	return;

    /*
     *	Don't print statusline messages if dumping to stdout.
     */
    if (dump_output_immediately)
	return;

    /*
     *	Use _statusline() set to output on the bottom line. - FM
     */
    LYStatusLine = (LYlines - 1);
    _statusline(text);
    LYStatusLine = -1;
}

PRIVATE void option_user_message ARGS2(
	CONST char *,		message,
	char *, 		argument)
{
    /*
     *	Make sure we have a pointer to a string.
     */
    if (message == NULL || argument == NULL)
	return;

    /*
     *	Don't print statusline messages if dumping to stdout.
     */
    if (dump_output_immediately)
	return;

    /*
     *	Use _user_message() set to output on the bottom line.
     */
    LYStatusLine = (LYlines - 1);
    _user_message(message, argument);
    LYStatusLine = -1;
}

PUBLIC void options NOARGS
{
#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
    int itmp;
#endif /* ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS */
    int response, ch;
    /*
     *	If the user changes the display we need memory to put it in.
     */
    char display_option[256];
#ifndef VMS
    static char putenv_command[142];
#endif /* !VMS */
    char *choices[MAXCHOICES];
    int CurrentCharSet = current_char_set;
    int CurrentShowColor = LYShowColor;
    int CurrentAssumeCharSet = UCLYhndl_for_unspec;
    BOOLEAN CurrentRawMode = LYRawMode;
    BOOLEAN AddValueAccepted = FALSE;
    char *cp = NULL;
    BOOL use_assume_charset, old_use_assume_charset;

#ifdef DIRED_SUPPORT
#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
    if (LYlines < 24) {
	HTAlert(OPTION_SCREEN_NEEDS_24);
	return;
    }
#else
    if (LYlines < 23) {
	HTAlert(OPTION_SCREEN_NEEDS_23);
	return;
    }
#endif /* ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS */
#else
#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
    if (LYlines < 23) {
	HTAlert(
	"Screen height must be at least 23 lines for the Options menu!");
	return;
    }
#else
    if (LYlines < 22) {
	HTAlert(OPTION_SCREEN_NEEDS_22);
	return;
    }
#endif /* ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS */
#endif /* DIRED_SUPPORT */

    term_options = FALSE;
    signal(SIGINT, terminate_options);
    if (no_option_save) {
	if (LYShowColor == SHOW_COLOR_NEVER) {
	    LYShowColor = SHOW_COLOR_OFF;
	} else if (LYShowColor == SHOW_COLOR_ALWAYS) {
	    LYShowColor = SHOW_COLOR_ON;
	}
#if defined(USE_SLANG) || defined(COLOR_CURSES)
    } else {
	if (LYChosenShowColor == SHOW_COLOR_UNKNOWN) {
	    switch (LYrcShowColor) {
	    case SHOW_COLOR_NEVER:
		LYChosenShowColor =
		    (LYShowColor >= SHOW_COLOR_ON) ?
				     SHOW_COLOR_ON :
				     SHOW_COLOR_NEVER;
		break;
	    case SHOW_COLOR_ALWAYS:
#if defined(COLOR_CURSES)
		if (!has_colors())
		    LYChosenShowColor = SHOW_COLOR_ALWAYS;
		else
#endif
		    LYChosenShowColor =
			(LYShowColor >= SHOW_COLOR_ON) ?
				     SHOW_COLOR_ALWAYS :
				     SHOW_COLOR_OFF;
		break;
	    default:
		LYChosenShowColor =
		    (LYShowColor >= SHOW_COLOR_ON) ?
				     SHOW_COLOR_ON :
				     SHOW_COLOR_OFF;
	    }
	}
#endif /* USE_SLANG || COLOR_CURSES */
    }

    old_use_assume_charset =
	use_assume_charset = (user_mode == ADVANCED_MODE);

draw_options:

    old_use_assume_charset = use_assume_charset;
    /*
     *	NOTE that printw() should be avoided for strings that
     *	might have non-ASCII or multibyte/CJK characters. - FM
     */
    response = 0;
#if defined(FANCY_CURSES) || defined (USE_SLANG)
    if (enable_scrollback) {
	clear();
    } else {
	erase();
    }
#else
    clear();
#endif /* FANCY_CURSES || USE_SLANG */
    move(0, 5);

    lynx_start_h1_color ();
    addstr("         Options Menu (");
    addstr(LYNX_NAME);
    addstr(" Version ");
    addstr(LYNX_VERSION);
    addch(')');
    lynx_stop_h1_color ();
    move(L_EDITOR, 5);
    addstr("E)ditor                      : ");
    addstr((editor && *editor) ? editor : "NONE");

    move(L_DISPLAY, 5);
    addstr("D)ISPLAY variable            : ");
    addstr((display && *display) ? display : "NONE");

    move(L_HOME, 5);
    addstr("mu(L)ti-bookmarks: ");
    addstr((LYMultiBookmarks ?
	      (LYMBMAdvanced ? "ADVANCED"
			     : "STANDARD")
			     : "OFF     "));
    move(L_HOME, B_BOOK);
    if (LYMultiBookmarks) {
	addstr("review/edit B)ookmarks files");
    } else {
	addstr("B)ookmark file: ");
	addstr((bookmark_page && *bookmark_page) ? bookmark_page : "NONE");
    }

    move(L_FTPSTYPE, 5);
    addstr("F)TP sort criteria           : ");
    addstr((HTfileSortMethod == FILE_BY_NAME ? "By Filename" :
	   (HTfileSortMethod == FILE_BY_SIZE ? "By Size    " :
	   (HTfileSortMethod == FILE_BY_TYPE ? "By Type    " :
					       "By Date    "))));

    move(L_MAIL_ADDRESS, 5);
    addstr("P)ersonal mail address       : ");
    addstr((personal_mail_address && *personal_mail_address) ?
				       personal_mail_address : "NONE");

    move(L_SSEARCH, 5);
    addstr("S)earching type              : ");
    addstr(case_sensitive ? "CASE SENSITIVE  " : "CASE INSENSITIVE");

    move(L_Charset, 5);
    addstr("display (C)haracter set      : ");
    addstr((char *)LYchar_set_names[current_char_set]);

    move(L_LANGUAGE, 5);
    addstr("preferred document lan(G)uage: ");
    addstr((language && *language) ? language : "NONE");

    move(L_PREF_CHARSET, 5);
    addstr("preferred document c(H)arset : ");
    addstr((pref_charset && *pref_charset) ? pref_charset : "NONE");

    if (use_assume_charset) {
	move(L_ASSUME_CHARSET, 5);
	addstr("^A)ssume charset if unknown  : ");
	if (UCAssume_MIMEcharset)
	    addstr(UCAssume_MIMEcharset);
	else
	    addstr((UCLYhndl_for_unspec >= 0) ?
		   (char *)LYCharSet_UC[UCLYhndl_for_unspec].MIMEname
					      : "NONE");
    }

    move(L_Rawmode, 5);
    addstr("Raw 8-bit or CJK m(O)de      : ");
    addstr(LYRawMode ? "ON " : "OFF");

#if defined(USE_SLANG) || defined(COLOR_CURSES)
    move(L_Color, B_COLOR);
    addstr("show color (&)  : ");
    if (no_option_save) {
	addstr((LYShowColor == SHOW_COLOR_OFF ? "OFF" :
						"ON "));
    } else {
	switch (LYChosenShowColor) {
	case SHOW_COLOR_NEVER:
                addstr("NEVER     ");
		break;
	case SHOW_COLOR_OFF:
		addstr("OFF");
		break;
	case SHOW_COLOR_ON:
		addstr("ON ");
		break;
	case SHOW_COLOR_ALWAYS:
#if defined(COLOR_CURSES)
		if (!has_colors())
		    addstr("Always try");
		else
#endif
		    addstr("ALWAYS    ");
	}
    }
#endif /* USE_SLANG || COLOR_CURSES */

    move(L_Bool_A, B_VIKEYS);
    addstr("V)I keys: ");
    addstr(vi_keys ? "ON " : "OFF");

    move(L_Bool_A, B_EMACSKEYS);
    addstr("e(M)acs keys: ");
    addstr(emacs_keys ? "ON " : "OFF");

    move(L_Bool_A, B_SHOW_DOTFILES);
    addstr("sho(W) dot files: ");
    addstr((!no_dotfiles && show_dotfiles) ? "ON " : "OFF");

    move(L_Bool_B, B_SELECT_POPUPS);
    addstr("popups for selec(T) fields   : ");
    addstr(LYSelectPopups ? "ON " : "OFF");

    move(L_Bool_B, B_SHOW_CURSOR);
    addstr("show cursor (@) : ");
    addstr(LYShowCursor ? "ON " : "OFF");

    move(L_Keypad, 5);
    addstr("K)eypad mode                 : ");
    addstr((keypad_mode == NUMBERS_AS_ARROWS) ?
				"Numbers act as arrows             " :
	 ((keypad_mode == LINKS_ARE_NUMBERED) ?
				"Links are numbered                " :
				"Links and form fields are numbered"));

    move(L_Lineed, 5);
    addstr("li(N)e edit style            : ");
    addstr(LYLineeditNames[current_lineedit]);

#ifdef DIRED_SUPPORT
    move(L_Dired, 5);
    addstr("l(I)st directory style       : ");
    addstr((dir_list_style == FILES_FIRST) ? "Files first      " :
	  ((dir_list_style == MIXED_STYLE) ? "Mixed style      " :
					     "Directories first"));
#endif /* DIRED_SUPPORT */

    move(L_User_Mode, 5);
    addstr("U)ser mode                   : ");
    addstr(  (user_mode == NOVICE_MODE) ? "Novice      " :
      ((user_mode == INTERMEDIATE_MODE) ? "Intermediate" :
					  "Advanced    "));

    move(L_User_Agent, 5);
    addstr("user (A)gent                 : ");
    addstr((LYUserAgent && *LYUserAgent) ? LYUserAgent : "NONE");

#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
    move(L_Exec, 5);
    addstr("local e(X)ecution links      : ");
#ifndef NEVER_ALLOW_REMOTE_EXEC
    addstr(               local_exec ? "ALWAYS ON           " :
	  (local_exec_on_local_files ? "FOR LOCAL FILES ONLY" :
				       "ALWAYS OFF          "));
#else
    addstr(local_exec_on_local_files ? "FOR LOCAL FILES ONLY" :
				       "ALWAYS OFF          ");
#endif /* !NEVER_ALLOW_REMOTE_EXEC */
#endif /* ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS */

    move(LYlines-3, 2);
    addstr(SELECT_SEGMENT);
    start_bold();
    addstr(CAP_LETT_SEGMENT);
    stop_bold();
    addstr(OF_OPT_LINE_SEGMENT);
    if (!no_option_save) {
	addstr(" '");
	start_bold();
	addstr(">");
	stop_bold();
	addstr("'");
	addstr(TO_SAVE_SEGMENT);
    }
    addstr(OR_SEGMENT);
    addstr("'");
    start_bold();
    addstr("r");
    stop_bold();
    addstr("'");
    addstr(TO_RETURN_SEGMENT);

    while (TOUPPER(response) != 'R' &&
	   !LYisNonAlnumKeyname(response, LYK_PREV_DOC) &&
	   response != '>' && !term_options &&
	   response != 7 &&  response != 3) {
	if (AddValueAccepted == TRUE) {
	    option_statusline(VALUE_ACCEPTED);
	    AddValueAccepted = FALSE;
	}
	move((LYlines - 2), 0);
	lynx_start_prompt_color ();
	addstr(COMMAND_PROMPT);
	lynx_stop_prompt_color ();

	refresh();
	response = LYgetch();
	if (term_options || response == 7 || response == 3)
	    response = 'R';
	if (LYisNonAlnumKeyname(response, LYK_REFRESH)) {
	    lynx_force_repaint();
	    goto draw_options;
	}
	switch (response) {
	    case 'e':	/* Change the editor. */
	    case 'E':
		if (no_editor) {
		    option_statusline(EDIT_DISABLED);
		} else if (system_editor ) {
		    option_statusline(EDITOR_LOCKED);
		} else {
		    if (editor && *editor)
			strcpy(display_option, editor);
		    else {  /* clear the NONE */
			move(L_EDITOR, COL_OPTION_VALUES);
			addstr("    ");
			*display_option = '\0';
		    }
		    option_statusline(ACCEPT_DATA);
		    move(L_EDITOR, COL_OPTION_VALUES);
		    start_bold();
		    ch = LYgetstr(display_option, VISIBLE,
				  sizeof(display_option), NORECALL);
		    stop_bold();
		    move(L_EDITOR, COL_OPTION_VALUES);
		    if (term_options || ch == -1) {
			addstr((editor && *editor) ?
					    editor : "NONE");
		    } else if (*display_option == '\0') {
			FREE(editor);
			addstr("NONE");
		    } else {
			StrAllocCopy(editor, display_option);
			addstr(display_option);
		    }
		    clrtoeol();
		    if (ch == -1) {
			option_statusline(CANCELLED);
			sleep(InfoSecs);
			option_statusline("");
		    } else {
			option_statusline(VALUE_ACCEPTED);
		    }
		}
		response = ' ';
		break;

	    case 'd':	/* Change the display. */
	    case 'D':
		if (display && *display) {
		    strcpy(display_option, display);
		} else {  /* clear the NONE */
		    move(L_DISPLAY, COL_OPTION_VALUES);
		    addstr("    ");
		    *display_option = '\0';
		}
		option_statusline(ACCEPT_DATA);
		move(L_DISPLAY, COL_OPTION_VALUES);
		start_bold();
		ch = LYgetstr(display_option, VISIBLE,
			      sizeof(display_option), NORECALL);
		stop_bold();
		move(L_DISPLAY, COL_OPTION_VALUES);
		if ((term_options || ch == -1) ||
		    (display != NULL &&
#ifdef VMS
		     !strcasecomp(display, display_option)))
#else
		     !strcmp(display, display_option)))
#endif /* VMS */
		{
		    /*
		     *	Cancelled, or a non-NULL display string
		     *	wasn't changed. - FM
		     */
		    addstr((display && *display) ? display : "NONE");
		    clrtoeol();
		    if (ch == -1) {
			option_statusline(CANCELLED);
			sleep(InfoSecs);
			option_statusline("");
		    } else {
			option_statusline(VALUE_ACCEPTED);
		    }
		    response = ' ';
		    break;
		} else if (*display_option == '\0') {
		    if ((display == NULL) ||
			(display != NULL && *display == '\0')) {
			/*
			 *  NULL or zero-length display string
			 *  wasn't changed. - FM
			 */
			addstr("NONE");
			clrtoeol();
			option_statusline(VALUE_ACCEPTED);
			response = ' ';
			break;
		    }
		}
		/*
		 *  Set the new DISPLAY variable. - FM
		 */
#ifdef VMS
		{
		    int i;
		    for (i = 0; display_option[i]; i++)
			display_option[i] = TOUPPER(display_option[i]);
		    Define_VMSLogical(DISPLAY, display_option);
		}
#else
		sprintf(putenv_command, "DISPLAY=%s", display_option);
		putenv(putenv_command);
#endif /* VMS */
		if ((cp = getenv(DISPLAY)) != NULL && *cp != '\0') {
		    StrAllocCopy(display, cp);
		} else {
		    FREE(display);
		}
		cp = NULL;
		addstr(display ? display : "NONE");
		clrtoeol();
		if ((display == NULL && *display_option == '\0') ||
		    (display != NULL &&
		     !strcmp(display, display_option))) {
		    if (display == NULL &&
			LYisConfiguredForX == TRUE) {
			option_statusline(VALUE_ACCEPTED_WARNING_X);
		    } else if (display != NULL &&
			LYisConfiguredForX == FALSE) {
			option_statusline(VALUE_ACCEPTED_WARNING_NONX);
		    } else {
			option_statusline(VALUE_ACCEPTED);
		    }
		} else {
		    if (*display_option) {
			option_statusline(FAILED_TO_SET_DISPLAY);
		    } else {
			option_statusline(FAILED_CLEAR_SET_DISPLAY);
		    }
		}
		response = ' ';
		break;

	    case 'l':	/* Change multibookmarks option. */
	    case 'L':
		if (LYMBMBlocked) {
		    option_statusline(MULTIBOOKMARKS_DISALLOWED);
		    response = ' ';
		    break;
		}
		choices[0] = NULL;
		StrAllocCopy(choices[0], "OFF     ");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "STANDARD");
		choices[2] = NULL;
		StrAllocCopy(choices[2], "ADVANCED");
		choices[3] = NULL;
		if (!LYSelectPopups) {
		    LYMultiBookmarks = boolean_choice((LYMultiBookmarks *
						       (1 + LYMBMAdvanced)),
						      L_HOME, C_MULTI,
						      choices);
		} else {
		    LYMultiBookmarks = popup_choice((LYMultiBookmarks *
						     (1 + LYMBMAdvanced)),
						    L_HOME, (C_MULTI - 1),
						    choices,
						    3, FALSE);
		}
		if (LYMultiBookmarks == 2) {
		    LYMultiBookmarks = TRUE;
		    LYMBMAdvanced = TRUE;
		} else {
		    LYMBMAdvanced = FALSE;
		}
#if defined(VMS) || defined(USE_SLANG)
		if (LYSelectPopups) {
		    move(L_HOME, C_MULTI);
		    clrtoeol();
		    addstr(choices[(LYMultiBookmarks * (1 + LYMBMAdvanced))]);
		}
#endif /* VMS || USE_SLANG */
		FREE(choices[0]);
		FREE(choices[1]);
		FREE(choices[2]);
#if !defined(VMS) && !defined(USE_SLANG)
		if (!LYSelectPopups)
#endif /* !VMS && !USE_SLANG */
		{
		    move(L_HOME, B_BOOK);
		    clrtoeol();
		    if (LYMultiBookmarks) {
			addstr("review/edit B)ookmarks files");
		    } else {
			addstr("B)ookmark file: ");
			addstr((bookmark_page && *bookmark_page) ?
						   bookmark_page : "NONE");
		    }
		}
		response = ' ';
		if (LYSelectPopups) {
#if !defined(VMS) || defined(USE_SLANG)
		    if (term_options) {
			term_options = FALSE;
		    } else {
			AddValueAccepted = TRUE;
		    }
		    goto draw_options;
#else
		    term_options = FALSE;
#endif /* !VMS || USE_SLANG */
		}
		break;

	    case 'b':	/* Change the bookmark page location. */
	    case 'B':
		/*
		 *  Anonymous users should not be allowed to
		 *  change the bookmark page.
		 */
		if (!no_bookmark) {
		    if (LYMultiBookmarks) {
			edit_bookmarks();
			signal(SIGINT, terminate_options);
			goto draw_options;
		    }
		    if (bookmark_page && *bookmark_page) {
			strcpy(display_option, bookmark_page);
		    } else {  /* clear the NONE */
			move(L_HOME, C_DEFAULT);
			clrtoeol();
			*display_option = '\0';
		    }
		    option_statusline(ACCEPT_DATA);
		    move(L_HOME, C_DEFAULT);
		    start_bold();
		    ch = LYgetstr(display_option, VISIBLE,
				  sizeof(display_option), NORECALL);
		    stop_bold();
		    move(L_HOME, C_DEFAULT);
		    if (term_options ||
			ch == -1 || *display_option == '\0') {
			addstr((bookmark_page && *bookmark_page) ?
						   bookmark_page : "NONE");
		    } else if (!LYPathOffHomeOK(display_option,
						sizeof(display_option))) {
			addstr((bookmark_page && *bookmark_page) ?
						   bookmark_page : "NONE");
			clrtoeol();
			option_statusline(USE_PATH_OFF_HOME);
			response = ' ';
			break;
		    } else {
			StrAllocCopy(bookmark_page, display_option);
			StrAllocCopy(MBM_A_subbookmark[0],
				     bookmark_page);
			addstr(bookmark_page);
		    }
		    clrtoeol();
		    if (ch == -1) {
			option_statusline(CANCELLED);
			sleep(InfoSecs);
			option_statusline("");
		    } else {
			option_statusline(VALUE_ACCEPTED);
		    }
		} else { /* anonymous */
		    option_statusline(BOOKMARK_CHANGE_DISALLOWED);
		}
		response = ' ';
		break;

	    case 'f':	/* Change ftp directory sorting. */
	    case 'F':	/*  (also local for non-DIRED)	 */
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "By Filename");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "By Type    ");
		choices[2] = NULL;
		StrAllocCopy(choices[2], "By Size    ");
		choices[3] = NULL;
		StrAllocCopy(choices[3], "By Date    ");
		choices[4] = NULL;
		if (!LYSelectPopups) {
		    HTfileSortMethod = boolean_choice(HTfileSortMethod,
						      L_FTPSTYPE, -1,
						      choices);
		} else {
		    HTfileSortMethod = popup_choice(HTfileSortMethod,
						    L_FTPSTYPE, -1,
						    choices,
						    4, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		    move(L_FTPSTYPE, COL_OPTION_VALUES);
		    clrtoeol();
		    addstr(choices[HTfileSortMethod]);
#endif /* VMS || USE_SLANG */
		}
		FREE(choices[0]);
		FREE(choices[1]);
		FREE(choices[2]);
		FREE(choices[3]);
		response = ' ';
		if (LYSelectPopups) {
#if !defined(VMS) || defined(USE_SLANG)
		    if (term_options) {
			term_options = FALSE;
		    } else {
			AddValueAccepted = TRUE;
		    }
		    goto draw_options;
#else
		    term_options = FALSE;
#endif /* !VMS || USE_SLANG */
		}
		break;

	    case 'p': /* Change personal mail address for From headers. */
	    case 'P':
		if (personal_mail_address && *personal_mail_address) {
		    strcpy(display_option, personal_mail_address);
		} else {  /* clear the NONE */
		    move(L_MAIL_ADDRESS, COL_OPTION_VALUES);
		    addstr("    ");
		    *display_option = '\0';
		}
		option_statusline(ACCEPT_DATA);
		move(L_MAIL_ADDRESS, COL_OPTION_VALUES);
		start_bold();
		ch = LYgetstr(display_option, VISIBLE,
			      sizeof(display_option), NORECALL);
		stop_bold();
		move(L_MAIL_ADDRESS, COL_OPTION_VALUES);
		if (term_options || ch == -1) {
		    addstr((personal_mail_address &&
			    *personal_mail_address) ?
			      personal_mail_address : "NONE");
		} else if (*display_option == '\0') {
		    FREE(personal_mail_address);
		    addstr("NONE");
		} else {
		    StrAllocCopy(personal_mail_address, display_option);
		    addstr(display_option);
		}
		clrtoeol();
		if (ch == -1) {
		    option_statusline(CANCELLED);
		    sleep(InfoSecs);
		    option_statusline("");
		} else {
		    option_statusline(VALUE_ACCEPTED);
		}
		response = ' ';
		break;

	    case 's':	/* Change case sentitivity for searches. */
	    case 'S':
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "CASE INSENSITIVE");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "CASE SENSITIVE  ");
		choices[2] = NULL;
		case_sensitive = boolean_choice(case_sensitive,
						L_SSEARCH, -1, choices);
		FREE(choices[0]);
		FREE(choices[1]);
		response = ' ';
		break;

	    case '\001':	/* Change assume_charset setting. */
		if (use_assume_charset) {
		    int i, curval;
		    char ** assume_list;
		    assume_list = (char **)calloc(LYNumCharsets + 1, sizeof(char *));
		    if (!assume_list) {
			outofmem(__FILE__, "options");
		    }
		    for (i = 0; i < LYNumCharsets; i++) {
			assume_list[i] = (char *)LYCharSet_UC[i].MIMEname;
		    }
		    curval = UCLYhndl_for_unspec;
		    if (curval == current_char_set && UCAssume_MIMEcharset) {
			curval = UCGetLYhndl_byMIME(UCAssume_MIMEcharset);
		    }
		    if (curval < 0)
			curval = LYRawMode ? current_char_set : 0;
		    if (!LYSelectPopups) {
			UCLYhndl_for_unspec = boolean_choice(curval,
							     L_ASSUME_CHARSET, -1,
							     assume_list);
		    } else {
			UCLYhndl_for_unspec = popup_choice(curval,
							   L_ASSUME_CHARSET, -1,
							   assume_list,
							   0, FALSE);
#if defined(VMS) || defined(USE_SLANG)
			move(L_ASSUME_CHARSET, COL_OPTION_VALUES);
			clrtoeol();
			if (UCLYhndl_for_unspec >= 0)
			    addstr((char *)
				   LYCharSet_UC[UCLYhndl_for_unspec].MIMEname);
#endif /* VMS || USE_SLANG */
		    }

		    /*
		 *  Set the raw 8-bit or CJK mode defaults and
		 *  character set if changed. - FM
		 */
		    if (CurrentAssumeCharSet != UCLYhndl_for_unspec ||
			UCLYhndl_for_unspec != curval) {
			if (UCLYhndl_for_unspec != CurrentAssumeCharSet) {
			    StrAllocCopy(UCAssume_MIMEcharset,
					 LYCharSet_UC[UCLYhndl_for_unspec].MIMEname);
			}
			LYRawMode = (UCLYhndl_for_unspec == current_char_set);
			HTMLSetUseDefaultRawMode(current_char_set, LYRawMode);
			HTMLUseCharacterSet(current_char_set);
			CurrentAssumeCharSet = UCLYhndl_for_unspec;
			CurrentRawMode = LYRawMode;
#if !defined(VMS) && !defined(USE_SLANG)
			if (!LYSelectPopups)
#endif /* !VMS && !USE_SLANG */
			{
			    move(L_Rawmode, COL_OPTION_VALUES);
			    clrtoeol();
			    addstr(LYRawMode ? "ON " : "OFF");
			}
		    }
		    FREE(assume_list);
		    response = ' ';
		    if (LYSelectPopups) {
#if !defined(VMS) || defined(USE_SLANG)
			if (term_options) {
			    term_options = FALSE;
			} else {
			    AddValueAccepted = TRUE;
			}
			goto draw_options;
#else
			term_options = FALSE;
#endif /* !VMS || USE_SLANG */
		    }
		} else {
		    option_statusline(NEED_ADVANCED_USER_MODE);
		    AddValueAccepted = FALSE;
		}
		break;

	    case 'c':	/* Change charset setting. */
	    case 'C':
		if (!LYSelectPopups) {
		    current_char_set = boolean_choice(current_char_set,
						      L_Charset, -1,
						      (char **)LYchar_set_names);
		} else {
		    current_char_set = popup_choice(current_char_set,
						    L_Charset, -1,
						    (char **)LYchar_set_names,
						    0, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		    move(L_Charset, COL_OPTION_VALUES);
		    clrtoeol();
		    addstr((char *)LYchar_set_names[current_char_set]);
#endif /* VMS || USE_SLANG */
		}
		/*
		 *  Set the raw 8-bit or CJK mode defaults and
		 *  character set if changed. - FM
		 */
		if (CurrentCharSet != current_char_set) {
		    HTMLSetRawModeDefault(current_char_set);
		    LYUseDefaultRawMode = TRUE;
		    HTMLUseCharacterSet(current_char_set);
		    CurrentCharSet = current_char_set;
		    CurrentRawMode = LYRawMode;
#if !defined(VMS) && !defined(USE_SLANG)
		    if (!LYSelectPopups)
#endif /* !VMS && !USE_SLANG */
		    {
			move(L_Rawmode, COL_OPTION_VALUES);
			clrtoeol();
			addstr(LYRawMode ? "ON " : "OFF");
		    }
		}
		response = ' ';
		if (LYSelectPopups) {
#if !defined(VMS) || defined(USE_SLANG)
		    if (term_options) {
			term_options = FALSE;
		    } else {
			AddValueAccepted = TRUE;
		    }
		    goto draw_options;
#else
		    term_options = FALSE;
#endif /* !VMS || USE_SLANG */
		}
		break;

	    case 'o':	/* Change raw mode setting. */
	    case 'O':
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "OFF");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "ON ");
		choices[2] = NULL;
		LYRawMode = boolean_choice(LYRawMode, L_Rawmode, -1, choices);
		/*
		 *  Set the LYUseDefaultRawMode value and character
		 *  handling if LYRawMode was changed. - FM
		 */
		if (CurrentRawMode != LYRawMode) {
		    HTMLSetUseDefaultRawMode(current_char_set, LYRawMode);
		    HTMLSetCharacterHandling(current_char_set);
		    CurrentRawMode = LYRawMode;
		}
		FREE(choices[0]);
		FREE(choices[1]);
		response = ' ';
		break;

	    case 'g':	/* Change language preference. */
	    case 'G':
		if (language && *language) {
		    strcpy(display_option, language);
		} else {  /* clear the NONE */
		    move(L_LANGUAGE, COL_OPTION_VALUES);
		    addstr("    ");
		    *display_option = '\0';
		}
		option_statusline(ACCEPT_DATA);
		move(L_LANGUAGE, COL_OPTION_VALUES);
		start_bold();
		ch = LYgetstr(display_option, VISIBLE,
			      sizeof(display_option), NORECALL);
		stop_bold();
		move(L_LANGUAGE, COL_OPTION_VALUES);
		if (term_options || ch == -1) {
		    addstr((language && *language) ?
					  language : "NONE");
		} else if (*display_option == '\0') {
		    FREE(language);
		    addstr("NONE");
		} else {
		    StrAllocCopy(language, display_option);
		    addstr(display_option);
		}
		clrtoeol();
		if (ch == -1) {
		    option_statusline(CANCELLED);
		    sleep(InfoSecs);
		    option_statusline("");
		} else {
		    option_statusline(VALUE_ACCEPTED);
		}
		response = ' ';
		break;

	    case 'h':	/* Change charset preference. */
	    case 'H':
		if (pref_charset && *pref_charset) {
		    strcpy(display_option, pref_charset);
		} else {  /* clear the NONE */
		    move(L_PREF_CHARSET, COL_OPTION_VALUES);
		    addstr("    ");
		    *display_option = '\0';
		}
		option_statusline(ACCEPT_DATA);
		move(L_PREF_CHARSET, COL_OPTION_VALUES);
		start_bold();
		ch = LYgetstr(display_option, VISIBLE,
			      sizeof(display_option), NORECALL);
		stop_bold();
		move(L_PREF_CHARSET, COL_OPTION_VALUES);
		if (term_options || ch == -1) {
		    addstr((pref_charset && *pref_charset) ?
			   pref_charset : "NONE");
		} else if (*display_option == '\0') {
		    FREE(pref_charset);
		    addstr("NONE");
		} else {
		    StrAllocCopy(pref_charset, display_option);
		    addstr(display_option);
		}
		clrtoeol();
		if (ch == -1) {
		    option_statusline(CANCELLED);
		    sleep(InfoSecs);
		    option_statusline("");
		} else {
		    option_statusline(VALUE_ACCEPTED);
		}
		response = ' ';
		break;

	    case 'v':	/* Change VI keys setting. */
	    case 'V':
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "OFF");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "ON ");
		choices[2] = NULL;
		vi_keys = boolean_choice(vi_keys,
					 L_Bool_A, C_VIKEYS,
					 choices);
		if (vi_keys) {
		    set_vi_keys();
		} else {
		    reset_vi_keys();
		}
		FREE(choices[0]);
		FREE(choices[1]);
		response = ' ';
		break;

	    case 'M':	/* Change emacs keys setting. */
	    case 'm':
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "OFF");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "ON ");
		choices[2] = NULL;
		emacs_keys = boolean_choice(emacs_keys,
					    L_Bool_A, C_EMACSKEYS,
					    choices);
		if (emacs_keys) {
		    set_emacs_keys();
		} else {
		    reset_emacs_keys();
		}
		FREE(choices[0]);
		FREE(choices[1]);
		response = ' ';
		break;

	    case 'W':	/* Change show dotfiles setting. */
	    case 'w':
		if (no_dotfiles) {
		    option_statusline(DOTFILE_ACCESS_DISABLED);
		} else {
		    /*
		     *	Copy strings into choice array.
		     */
		    choices[0] = NULL;
		    StrAllocCopy(choices[0], "OFF");
		    choices[1] = NULL;
		    StrAllocCopy(choices[1], "ON ");
		    choices[2] = NULL;
		    show_dotfiles = boolean_choice(show_dotfiles,
						   L_Bool_A,
						   C_SHOW_DOTFILES,
						   choices);
		    FREE(choices[0]);
		    FREE(choices[1]);
		}
		response = ' ';
		break;

	    case 't':	/* Change select popups setting. */
	    case 'T':
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "OFF");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "ON ");
		choices[2] = NULL;
		LYSelectPopups = boolean_choice(LYSelectPopups,
						L_Bool_B,
						C_SELECT_POPUPS,
						choices);
		FREE(choices[0]);
		FREE(choices[1]);
		response = ' ';
		break;

#if defined(USE_SLANG) || defined(COLOR_CURSES)
	    case '&':	/* Change show color setting. */
		if (no_option_save) {
#if defined(COLOR_CURSES)
		    if (!has_colors()) {
			char * terminal = getenv("TERM");
			if (terminal)
			    option_user_message(
				COLOR_TOGGLE_DISABLED_FOR_TERM,
				terminal);
			else
			    option_statusline(COLOR_TOGGLE_DISABLED);
			sleep(AlertSecs);
		    }
#endif
		/*
		 *  Copy strings into choice array.
		 */
		    choices[0] = NULL;
		    StrAllocCopy(choices[0], "OFF");
		    choices[1] = NULL;
		    StrAllocCopy(choices[1], "ON ");
		    choices[2] = NULL;
		    LYShowColor = boolean_choice((LYShowColor - 1),
						 L_Color,
						 C_COLOR,
						 choices);
		    if (LYShowColor == 0) {
			LYShowColor = SHOW_COLOR_OFF;
		    } else {
			LYShowColor = SHOW_COLOR_ON;
		    }
		} else {		/* !no_option_save */
		    BOOLEAN again = FALSE;
		    int chosen;
		/*
		 *  Copy strings into choice array.
		 */
		    choices[0] = NULL;
		    StrAllocCopy(choices[0], "NEVER     ");
		    choices[1] = NULL;
		    StrAllocCopy(choices[1], "OFF       ");
		    choices[2] = NULL;
		    StrAllocCopy(choices[2], "ON        ");
		    choices[3] = NULL;
#if defined(COLOR_CURSES)
		    if (!has_colors())
			StrAllocCopy(choices[3], "Always try");
		    else
#endif
			StrAllocCopy(choices[3], "ALWAYS    ");
		    choices[4] = NULL;
		    do {
			if (!LYSelectPopups) {
			    chosen = boolean_choice(LYChosenShowColor,
						    L_Color,
						    C_COLOR,
						    choices);
			} else {
			    chosen = popup_choice(LYChosenShowColor,
						  L_Color,
						  C_COLOR,
						  choices, 4, FALSE);
			}
#if defined(COLOR_CURSES)
			again = (chosen == 2 && !has_colors());
			if (again) {
			    char * terminal = getenv("TERM");
			    if (terminal)
				option_user_message(
				    COLOR_TOGGLE_DISABLED_FOR_TERM,
				    terminal);
			    else
				option_statusline(COLOR_TOGGLE_DISABLED);
			    sleep(AlertSecs);
			}
#endif
		    } while (again);
		    LYChosenShowColor = chosen;
#if defined(VMS)
		    if (LYSelectPopups) {
			move(L_Color, C_COLOR);
			clrtoeol();
			addstr(choices[LYChosenShowColor]);
		    }
#endif /* VMS */
#if defined(COLOR_CURSES)
		    if (has_colors())
#endif
			LYShowColor = chosen;
		    FREE(choices[2]);
		    FREE(choices[3]);
		}
		FREE(choices[0]);
		FREE(choices[1]);
		if (CurrentShowColor != LYShowColor) {
		    lynx_force_repaint();
		}
		CurrentShowColor = LYShowColor;
#ifdef USE_SLANG
		SLtt_Use_Ansi_Colors = (LYShowColor > 1 ? 1 : 0);
#endif
		response = ' ';
		if (LYSelectPopups && !no_option_save) {
#if !defined(VMS) || defined(USE_SLANG)
		    if (term_options) {
			term_options = FALSE;
		    } else {
			AddValueAccepted = TRUE;
		    }
		    goto draw_options;
#else
		    term_options = FALSE;
#endif /* !VMS || USE_SLANG */
		}
		break;
#endif /* USE_SLANG or COLOR_CURSES */

	    case '@':	/* Change show cursor setting. */
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "OFF");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "ON ");
		choices[2] = NULL;
		LYShowCursor = boolean_choice(LYShowCursor,
					      L_Bool_B,
					      C_SHOW_CURSOR,
					      choices);
		FREE(choices[0]);
		FREE(choices[1]);
		response = ' ';
		break;

	    case 'k':	/* Change keypad mode. */
	    case 'K':
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0],
			     "Numbers act as arrows             ");
		choices[1] = NULL;
		StrAllocCopy(choices[1],
			     "Links are numbered                ");
		choices[2] = NULL;
		StrAllocCopy(choices[2],
			     "Links and form fields are numbered");
		choices[3] = NULL;
		if (!LYSelectPopups) {
		    keypad_mode = boolean_choice(keypad_mode,
						 L_Keypad, -1,
						 choices);
		} else {
		    keypad_mode = popup_choice(keypad_mode,
					       L_Keypad, -1,
					       choices,
					       3, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		    move(L_Keypad, COL_OPTION_VALUES);
		    clrtoeol();
		    addstr(choices[keypad_mode]);
#endif /* VMS || USE_SLANG */
		}
		if (keypad_mode == NUMBERS_AS_ARROWS) {
		    set_numbers_as_arrows();
		} else {
		    reset_numbers_as_arrows();
		}
		FREE(choices[0]);
		FREE(choices[1]);
		FREE(choices[2]);
		response = ' ';
		if (LYSelectPopups) {
#if !defined(VMS) || defined(USE_SLANG)
		    if (term_options) {
			term_options = FALSE;
		    } else {
			AddValueAccepted = TRUE;
		    }
		    goto draw_options;
#else
		    term_options = FALSE;
#endif /* !VMS || USE_SLANG */
		}
		break;

	    case 'n':	/* Change line editor key bindings. */
	    case 'N':
		if (!LYSelectPopups) {
		    current_lineedit = boolean_choice(current_lineedit,
						      L_Lineed, -1,
						      LYLineeditNames);
		} else {
		    current_lineedit = popup_choice(current_lineedit,
						    L_Lineed, -1,
						    LYLineeditNames,
						    0, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		    move(L_Lineed, COL_OPTION_VALUES);
		    clrtoeol();
		    addstr(LYLineeditNames[current_lineedit]);
#endif /* VMS || USE_SLANG */
		}
		response = ' ';
		if (LYSelectPopups) {
#if !defined(VMS) || defined(USE_SLANG)
		    if (term_options) {
			term_options = FALSE;
		    } else {
			AddValueAccepted = TRUE;
		    }
		    goto draw_options;
#else
		    term_options = FALSE;
#endif /* !VMS || USE_SLANG */
		}
		break;

#ifdef DIRED_SUPPORT
	    case 'i':	/* Change local directory sorting. */
	    case 'I':
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "Directories first");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "Files first      ");
		choices[2] = NULL;
		StrAllocCopy(choices[2], "Mixed style      ");
		choices[3] = NULL;
		if (!LYSelectPopups) {
		    dir_list_style = boolean_choice(dir_list_style,
						    L_Dired, -1,
						    choices);
		} else {
		    dir_list_style = popup_choice(dir_list_style,
						  L_Dired, -1,
						  choices,
						  3, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		    move(L_Dired, COL_OPTION_VALUES);
		    clrtoeol();
		    addstr(choices[dir_list_style]);
#endif /* VMS || USE_SLANG */
		}
		FREE(choices[0]);
		FREE(choices[1]);
		FREE(choices[2]);
		response = ' ';
		if (LYSelectPopups) {
#if !defined(VMS) || defined(USE_SLANG)
		    if (term_options) {
			term_options = FALSE;
		    } else {
			AddValueAccepted = TRUE;
		    }
		    goto draw_options;
#else
		    term_options = FALSE;
#endif /* !VMS || USE_SLANG */
		}
		break;
#endif /* DIRED_SUPPORT */

	    case 'u':	/* Change user mode. */
	    case 'U':
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "Novice      ");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "Intermediate");
		choices[2] = NULL;
		StrAllocCopy(choices[2], "Advanced    ");
		choices[3] = NULL;
		if (!LYSelectPopups) {
		    user_mode = boolean_choice(user_mode,
					       L_User_Mode, -1,
					       choices);
		    use_assume_charset = (user_mode >= 2);
		} else {
		    user_mode = popup_choice(user_mode,
					     L_User_Mode, -1,
					     choices,
					     3, FALSE);
		    use_assume_charset = (user_mode >= 2);
#if defined(VMS) || defined(USE_SLANG)
		    if (use_assume_charset == old_use_assume_charset) {
			move(L_User_Mode, COL_OPTION_VALUES);
			clrtoeol();
			addstr(choices[user_mode]);
		    }
#endif /* VMS || USE_SLANG */
		}
		FREE(choices[0]);
		FREE(choices[1]);
		FREE(choices[2]);
		if (user_mode == NOVICE_MODE) {
		    display_lines = (LYlines - 4);
		} else {
		    display_lines = LYlines-2;
		}
		response = ' ';
		if (LYSelectPopups) {
#if !defined(VMS) || defined(USE_SLANG)
		    if (term_options) {
			term_options = FALSE;
		    } else {
			AddValueAccepted = TRUE;
		    }
		    goto draw_options;
#else
		    term_options = FALSE;
		    if (use_assume_charset != old_use_assume_charset)
			goto draw_options;
#endif /* !VMS || USE_SLANG */
		}
		break;

	    case 'a':	/* Change user agent string. */
	    case 'A':
		if (!no_useragent) {
		    if (LYUserAgent && *LYUserAgent) {
			strcpy(display_option, LYUserAgent);
		    } else {  /* clear the NONE */
			move(L_HOME, COL_OPTION_VALUES);
			addstr("    ");
			*display_option = '\0';
		    }
		    option_statusline(ACCEPT_DATA_OR_DEFAULT);
		    move(L_User_Agent, COL_OPTION_VALUES);
		    start_bold();
		    ch = LYgetstr(display_option, VISIBLE,
				  sizeof(display_option), NORECALL);
		    stop_bold();
		    move(L_User_Agent, COL_OPTION_VALUES);
		    if (term_options || ch == -1) {
			addstr((LYUserAgent &&
				*LYUserAgent) ?
				  LYUserAgent : "NONE");
		    } else if (*display_option == '\0') {
			StrAllocCopy(LYUserAgent, LYUserAgentDefault);
			addstr((LYUserAgent &&
				*LYUserAgent) ?
				  LYUserAgent : "NONE");
		    } else {
			StrAllocCopy(LYUserAgent, display_option);
			addstr(display_option);
		    }
		    clrtoeol();
		    if (ch == -1) {
			option_statusline(CANCELLED);
			sleep(InfoSecs);
			option_statusline("");
		    } else if (LYUserAgent && *LYUserAgent &&
			!strstr(LYUserAgent, "Lynx") &&
			!strstr(LYUserAgent, "lynx")) {
			option_statusline(UA_COPYRIGHT_WARNING);
		    } else {
			option_statusline(VALUE_ACCEPTED);
		    }
		} else { /* disallowed */
		    option_statusline(UA_COPYRIGHT_WARNING);
		}
		response = ' ';
		break;

#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
	    case 'x':	/* Change local exec restriction. */
	    case 'X':
		if (exec_frozen && !LYSelectPopups) {
		    option_statusline(CHANGE_OF_SETTING_DISALLOWED);
		    response = ' ';
		    break;
		}
#ifndef NEVER_ALLOW_REMOTE_EXEC
		if (local_exec) {
		    itmp = 2;
		} else
#endif /* !NEVER_ALLOW_REMOTE_EXEC */
		{
		    if (local_exec_on_local_files) {
			itmp= 1;
		    } else {
			itmp = 0;
		    }
		}
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "ALWAYS OFF          ");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "FOR LOCAL FILES ONLY");
		choices[2] = NULL;
#ifndef NEVER_ALLOW_REMOTE_EXEC
		StrAllocCopy(choices[2], "ALWAYS ON           ");
		choices[3] = NULL;
#endif /* !NEVER_ALLOW_REMOTE_EXEC */
		if (!LYSelectPopups) {
		    itmp = boolean_choice(itmp,
					  L_Exec, -1,
					  choices);
		} else {
		    itmp = popup_choice(itmp,
					L_Exec, -1,
					choices,
					0, (exec_frozen ? TRUE : FALSE));
#if defined(VMS) || defined(USE_SLANG)
		    move(L_Exec, COL_OPTION_VALUES);
		    clrtoeol();
		    addstr(choices[itmp]);
#endif /* VMS || USE_SLANG */
		}
		FREE(choices[0]);
		FREE(choices[1]);
#ifndef NEVER_ALLOW_REMOTE_EXEC
		FREE(choices[2]);
#endif /* !NEVER_ALLOW_REMOTE_EXEC */
		if (!exec_frozen) {
		    switch (itmp) {
			case 0:
			    local_exec = FALSE;
			    local_exec_on_local_files = FALSE;
			    break;
			case 1:
			    local_exec = FALSE;
			    local_exec_on_local_files = TRUE;
			    break;
#ifndef NEVER_ALLOW_REMOTE_EXEC
			case 2:
			    local_exec = TRUE;
			    local_exec_on_local_files = FALSE;
			    break;
#endif /* !NEVER_ALLOW_REMOTE_EXEC */
		    } /* end switch */
		}
		response = ' ';
		if (LYSelectPopups) {
#if !defined(VMS) || defined(USE_SLANG)
		    if (exec_frozen || term_options) {
			term_options = FALSE;
		    } else {
			AddValueAccepted = TRUE;
		    }
		    goto draw_options;
#else
		    term_options = FALSE;
#endif /* !VMS || USE_SLANG */
		}
		break;
#endif /* ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS */

	    case '>':	/* Save current options to RC file. */
		if (!no_option_save) {
		    option_statusline(SAVING_OPTIONS);
		    if (save_rc()) {
			LYrcShowColor = LYChosenShowColor;
			option_statusline(OPTIONS_SAVED);
		    } else {
			HTAlert(OPTIONS_NOT_SAVED);
		    }
		} else {
		    option_statusline(R_TO_RETURN_TO_LYNX);
		    /*
		     *	Change response so that we don't exit
		     *	the options menu.
		     */
		    response = ' ';
		}
		break;

	    case 'r':	/* Return to document (quit options menu). */
	    case 'R':
		break;

	    default:
		if (!no_option_save) {
		    option_statusline(SAVE_OR_R_TO_RETURN_TO_LYNX);
		} else {
		    option_statusline(R_TO_RETURN_TO_LYNX);
		}
	}  /* end switch */
    }  /* end while */

    term_options = FALSE;
    signal(SIGINT, cleanup_sig);
}

/*
 *  Take a boolean status,prompt the user for a new status,
 *  and return it.
 */
PRIVATE int boolean_choice ARGS4(
	int,		cur_choice,
	int,		line,
	int,		column,
	char **,	choices)
{
    int response = 0;
    int cmd = 0;
    int number = 0;
    int col = (column >= 0 ? column : COL_OPTION_VALUES);
    int orig_choice = cur_choice;
#ifdef VMS
    extern BOOLEAN HadVMSInterrupt; /* Flag from cleanup_sig() AST */
#endif /* VMS */

    /*
     *	Get the number of choices and then make
     *	number zero-based.
     */
    for (number = 0; choices[number] != NULL; number++)
	;  /* empty loop body */
    number--;

    /*
     *	Update the statusline.
     */
    option_statusline(ANY_KEY_CHANGE_RET_ACCEPT);

    /*
     *	Highlight the current choice.
     */
    move(line, col);
    start_reverse();
    addstr(choices[cur_choice]);
    if (LYShowCursor)
	move(line, (col - 1));
    refresh();

    /*
     *	Get the keyboard entry, and leave the
     *	cursor at the choice, to indicate that
     *	it can be changed, until the user accepts
     *	the current choice.
     */
    term_options = FALSE;
    while (1) {
	move(line, col);
	if (term_options == FALSE) {
	    response = LYgetch();
	}
	if (term_options || response == 7 || response == 3) {
	     /*
	      *  Control-C or Control-G.
	      */
	    response = '\n';
	    term_options = TRUE;
	    cur_choice = orig_choice;
	}
#ifdef VMS
	if (HadVMSInterrupt) {
	    HadVMSInterrupt = FALSE;
	    response = '\n';
	    term_options = TRUE;
	    cur_choice = orig_choice;
	}
#endif /* VMS */
	if ((response != '\n' && response != '\r') &&
	    (cmd = keymap[response+1]) != LYK_ACTIVATE) {
	    switch (cmd) {
		case LYK_HOME:
		    cur_choice = 0;
		    break;

		case LYK_END:
		    cur_choice = number;
		    break;

		case LYK_REFRESH:
		    lynx_force_repaint();
		    refresh();
		    break;

		case LYK_QUIT:
		case LYK_ABORT:
		case LYK_PREV_DOC:
		    cur_choice = orig_choice;
		    term_options = TRUE;
		    break;

		case LYK_PREV_PAGE:
		case LYK_UP_HALF:
		case LYK_UP_TWO:
		case LYK_PREV_LINK:
		case LYK_UP_LINK:
		case LYK_LEFT_LINK:
		    if (cur_choice == 0)
			cur_choice = number;  /* go back to end */
		    else
			cur_choice--;
		    break;

		case LYK_1:
		case LYK_2:
		case LYK_3:
		case LYK_4:
		case LYK_5:
		case LYK_6:
		case LYK_7:
		case LYK_8:
		case LYK_9:
		    if((cmd - LYK_1 + 1) <= number) {
			cur_choice = cmd -LYK_1 + 1;
			break;
		    }  /* else fall through! */
		default:
		    if (cur_choice == number)
			cur_choice = 0;  /* go over the top and around */
		    else
			cur_choice++;
	    }  /* end of switch */
	    addstr(choices[cur_choice]);
	    if (LYShowCursor)
		move(line, (col - 1));
	    refresh();
	} else {
	    /*
	     *	Unhighlight choice.
	     */
	    move(line, col);
	    stop_reverse();
	    addstr(choices[cur_choice]);

	    if (term_options) {
		term_options = FALSE;
		option_statusline(CANCELLED);
		sleep(InfoSecs);
		option_statusline("");
	    } else {
		option_statusline(VALUE_ACCEPTED);
	    }
	    return(cur_choice);
	}
    }
}

PRIVATE void terminate_options ARGS1(
	int,		sig GCC_UNUSED)
{
    term_options = TRUE;
    /*
     *	Reassert the AST.
     */
    signal(SIGINT, terminate_options);
#ifdef VMS
    /*
     *	Refresh the screen to get rid of the "interrupt" message.
     */
    if (!dump_output_immediately) {
	lynx_force_repaint();
	refresh();
    }
#endif /* VMS */
}

/*
 *  Multi-Bookmark On-Line editing support. - FMG & FM
 */
PUBLIC void edit_bookmarks NOARGS
{
    int response = 0, def_response = 0, ch;
    int MBM_current = 1;
#define MULTI_OFFSET 8
    int a; /* misc counter */
    char MBM_tmp_line[256]; /* buffer for LYgetstr */
    char ehead_buffer[265];

    /*
     *	We need (MBM_V_MAXFILES + MULTI_OFFSET) lines to display
     *	the whole list at once.  Otherwise break it up into two
     *	segments.  We know it won't be less than that because
     *	'o'ptions needs 23-24 at LEAST.
     */
    term_options = FALSE;
    signal(SIGINT, terminate_options);

draw_bookmark_list:
    /*
     *	Display menu of bookmarks.  NOTE that we avoid printw()'s
     *	to increase the chances that any non-ASCII or multibyte/CJK
     *	characters will be handled properly. - FM
     */
#if defined(FANCY_CURSES) || defined (USE_SLANG)
    if (enable_scrollback) {
	clear();
    } else {
	erase();
    }
#else
    clear();
#endif /* FANCY_CURSES || USE_SLANG */
    move(0, 5);
    lynx_start_h1_color ();
    if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET)) {
	sprintf(ehead_buffer, MULTIBOOKMARKS_EHEAD_MASK, MBM_current);
	addstr(ehead_buffer);
    } else {
	addstr(MULTIBOOKMARKS_EHEAD);
    }
    lynx_stop_h1_color ();

    if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET)) {
	for (a = ((MBM_V_MAXFILES/2 + 1) * (MBM_current - 1));
		      a <= ((float)MBM_V_MAXFILES/2 * MBM_current); a++) {
	    move((3 + a) - ((MBM_V_MAXFILES/2 + 1)*(MBM_current - 1)), 5);
	    addch((unsigned char)(a + 'A'));
	    addstr(" : ");
	    if (MBM_A_subdescript[a])
		addstr(MBM_A_subdescript[a]);
	    move((3 + a) - ((MBM_V_MAXFILES/2 + 1)*(MBM_current - 1)), 35);
	    addstr("| ");
	    if (MBM_A_subbookmark[a]) {
		addstr(MBM_A_subbookmark[a]);
	    }
	}
    } else {
	for (a = 0; a <= MBM_V_MAXFILES; a++) {
	    move(3 + a, 5);
	    addch((unsigned char)(a + 'A'));
	    addstr(" : ");
	    if (MBM_A_subdescript[a])
		addstr(MBM_A_subdescript[a]);
	    move(3 + a, 35);
	    addstr("| ");
	    if (MBM_A_subbookmark[a]) {
		addstr(MBM_A_subbookmark[a]);
	    }
	}
    }

    /*
     *	Only needed when we have 2 screens.
     */
    if (LYlines < MBM_V_MAXFILES + MULTI_OFFSET) {
	move((LYlines - 4), 0);
	addstr("'");
	start_bold();
	addstr("[");
	stop_bold();
	addstr("' ");
	addstr(PREVIOUS);
	addstr(", '");
	start_bold();
	addstr("]");
	stop_bold();
	addstr("' ");
	addstr(NEXT_SCREEN);
    }

    move((LYlines - 3), 0);
    if (!no_option_save) {
	addstr("'");
	start_bold();
	addstr(">");
	stop_bold();
	addstr("'");
	addstr(TO_SAVE_SEGMENT);
    }
    addstr(OR_SEGMENT);
    addstr("'");
    start_bold();
    addstr("^G");
    stop_bold();
    addstr("'");
    addstr(TO_RETURN_SEGMENT);

    while (!term_options &&
	   !LYisNonAlnumKeyname(response, LYK_PREV_DOC) &&
	   response != 7 && response != 3 &&
	   response != '>') {

	move((LYlines - 2), 0);
	lynx_start_prompt_color ();
	addstr(MULTIBOOKMARKS_LETTER);
	lynx_stop_prompt_color ();

	refresh();
	response = (def_response ? def_response : LYgetch());
	def_response = 0;

	/*
	 *  Check for a cancel.
	 */
	if (term_options ||
	    response == 7 || response == 3 ||
	    LYisNonAlnumKeyname(response, LYK_PREV_DOC))
	    continue;

	/*
	 *  Check for a save.
	 */
	if (response == '>') {
	    if (!no_option_save) {
		option_statusline(SAVING_OPTIONS);
		if (save_rc())
		    option_statusline(OPTIONS_SAVED);
		else
		    HTAlert(OPTIONS_NOT_SAVED);
	    } else {
		option_statusline(R_TO_RETURN_TO_LYNX);
		/*
		 *  Change response so that we don't exit
		 *  the options menu.
		 */
		response = ' ';
	    }
	    continue;
	}

	/*
	 *  Check for a refresh.
	 */
	if (LYisNonAlnumKeyname(response, LYK_REFRESH)) {
	    lynx_force_repaint();
	    continue;
	}

	/*
	 *  Move between the screens - if we can't show it all at once.
	 */
	if ((response == ']' ||
	     LYisNonAlnumKeyname(response, LYK_NEXT_PAGE)) &&
	    LYlines < (MBM_V_MAXFILES + MULTI_OFFSET)) {
	    MBM_current++;
	    if (MBM_current >= 3)
		MBM_current = 1;
	    goto draw_bookmark_list;
	}
	if ((response == '[' ||
	     LYisNonAlnumKeyname(response, LYK_PREV_PAGE)) &&
	    LYlines < (MBM_V_MAXFILES + MULTI_OFFSET)) {
	    MBM_current--;
	    if (MBM_current <= 0)
		MBM_current = 2;
	    goto draw_bookmark_list;
	}

	/*
	 *  Instead of using 26 case statements, we set up
	 *  a scan through the letters and edit the lines
	 *  that way.
	 */
	for (a = 0; a <= MBM_V_MAXFILES; a++) {
	    if ((TOUPPER(response) - 'A') == a) {
		if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET)) {
		    if (MBM_current == 1 && a > (MBM_V_MAXFILES/2)) {
			MBM_current = 2;
			def_response = response;
			goto draw_bookmark_list;
		    }
		    if (MBM_current == 2 && a < (MBM_V_MAXFILES/2)) {
			MBM_current = 1;
			def_response = response;
			goto draw_bookmark_list;
		    }
		}
		option_statusline(ACCEPT_DATA);

		if (a > 0) {
		    start_bold();
		    if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET))
			move(
			 (3 + a) - ((MBM_V_MAXFILES/2 + 1)*(MBM_current - 1)),
			     9);
		    else
			move((3 + a), 9);
		    strcpy(MBM_tmp_line,
			   (!MBM_A_subdescript[a] ?
					       "" : MBM_A_subdescript[a]));
		    ch = LYgetstr(MBM_tmp_line, VISIBLE,
				  sizeof(MBM_tmp_line), NORECALL);
		    stop_bold();

		    if (strlen(MBM_tmp_line) < 1) {
			FREE(MBM_A_subdescript[a]);
		    } else {
			StrAllocCopy(MBM_A_subdescript[a], MBM_tmp_line);
		    }
		    if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET))
			move(
			 (3 + a) - ((MBM_V_MAXFILES/2 + 1)*(MBM_current - 1)),
			     5);
		    else
			move((3 + a), 5);
		    addch((unsigned char)(a + 'A'));
		    addstr(" : ");
		    if (MBM_A_subdescript[a])
			addstr(MBM_A_subdescript[a]);
		    clrtoeol();
		    refresh();
		}

		if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET))
		    move((3 + a) - ((MBM_V_MAXFILES/2 + 1)*(MBM_current - 1)),
			 35);
		else
		    move((3 + a), 35);
		addstr("| ");

		start_bold();
		strcpy(MBM_tmp_line,
		       (!MBM_A_subbookmark[a] ? "" : MBM_A_subbookmark[a]));
		ch = LYgetstr(MBM_tmp_line, VISIBLE,
			      sizeof(MBM_tmp_line), NORECALL);
		stop_bold();

		if (*MBM_tmp_line == '\0') {
		    if (a == 0)
			StrAllocCopy(MBM_A_subbookmark[a], bookmark_page);
		    else
			FREE(MBM_A_subbookmark[a]);
		} else if (!LYPathOffHomeOK(MBM_tmp_line,
					    sizeof(MBM_tmp_line))) {
			LYMBM_statusline(USE_PATH_OFF_HOME);
			sleep(AlertSecs);
		} else {
		    StrAllocCopy(MBM_A_subbookmark[a], MBM_tmp_line);
		    if (a == 0) {
			StrAllocCopy(bookmark_page, MBM_A_subbookmark[a]);
		    }
		}
		if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET))
		    move((3 + a) - ((MBM_V_MAXFILES/2 + 1)*(MBM_current-1)),
			 35);
		else
		    move((3 + a), 35);
		addstr("| ");
		if (MBM_A_subbookmark[a])
		    addstr(MBM_A_subbookmark[a]);
		clrtoeol();
		move(LYlines-1, 0);
		clrtoeol();
		break;
	    }
	}  /* end for */
    } /* end while */

    term_options = FALSE;
    signal(SIGINT, cleanup_sig);
}

/*
**  This function prompts for a choice or page number.
**  If a 'g' or 'p' suffix is included, that will be
**  loaded into c.  Otherwise, c is zeroed. - FM
*/
PRIVATE int get_popup_choice_number ARGS1(
	int *,		c)
{
    char temp[120];

    /*
     *	Load the c argument into the prompt buffer.
     */
    temp[0] = *c;
    temp[1] = '\0';
    option_statusline(OPTION_CHOICE_NUMBER);

    /*
     *	Get the number, possibly with a suffix, from the user.
     */
    if (LYgetstr(temp, VISIBLE, sizeof(temp), NORECALL) < 0 ||
	*temp == 0 || term_options) {
	option_statusline(CANCELLED);
	sleep(InfoSecs);
	*c = '\0';
	term_options = FALSE;
	return(0);
    }

    /*
     *	If we had a 'g' or 'p' suffix, load it into c.
     *	Otherwise, zero c.  Then return the number.
     */
    if (strchr(temp, 'g') != NULL || strchr(temp, 'G') != NULL) {
	*c = 'g';
    } else if (strchr(temp, 'p') != NULL || strchr(temp, 'P') != NULL) {
	*c = 'p';
    } else {
	*c = '\0';
    }
    return(atoi(temp));
}

/*
 *  This function offers the choices for values of an
 *  option via a popup window which functions like
 *  that for selection of options in a form. - FM
 */
PRIVATE int popup_choice ARGS6(
	int,		cur_choice,
	int,		line,
	int,		column,
	char **,	choices,
	int,		i_length,
	int,		disabled)
{
    int ly = line;
    int lx = (column >= 0 ? column : (COL_OPTION_VALUES - 1));
    int c = 0, cmd = 0, i = 0, j = 0;
    int orig_choice = cur_choice;
#ifndef USE_SLANG
    WINDOW * form_window;
#endif /* !USE_SLANG */
    int num_choices = 0, top, bottom, length = -1, width = 0;
    char ** Cptr = choices;
    int window_offset = 0;
    int DisplayLines = (LYlines - 2);
    char Cnum[64];
    int Lnum;
    int npages;
#ifdef VMS
    extern BOOLEAN HadVMSInterrupt; /* Flag from cleanup_sig() AST */
#endif /* VMS */
    static char prev_target[512];		/* Search string buffer */
    static char prev_target_buffer[512];	/* Next search buffer */
    static BOOL first = TRUE;
    char *cp;
    int ch = 0, recall;
    int QueryTotal;
    int QueryNum;
    BOOLEAN FirstRecall = TRUE;
    BOOLEAN ReDraw = FALSE;
    int number;
    char buffer[512];

    /*
     * Initialize the search string buffer. - FM
     */
    if (first) {
	*prev_target_buffer = '\0';
	first = FALSE;
    }
    *prev_target = '\0';
    QueryTotal = (search_queries ? HTList_count(search_queries) : 0);
    recall = ((QueryTotal >= 1) ? RECALL : NORECALL);
    QueryNum = QueryTotal;

    /*
     *	Count the number of choices to be displayed, where
     *	num_choices ranges from 0 to n, and set width to the
     *	longest choice string length.  Also set Lnum to the
     *	length for the highest choice number, then decrement
     *	num_choices so as to be zero-based.  The window width
     *	will be based on the sum of width and Lnum. - FM
     */
    for (num_choices = 0; Cptr[num_choices] != NULL; num_choices++) {
	if (strlen(Cptr[num_choices]) > width) {
	    width = strlen(Cptr[num_choices]);
	}
    }
    sprintf(Cnum, "%d: ", num_choices);
    Lnum = strlen(Cnum);
    num_choices--;

    /*
     *	Let's assume for the sake of sanity that ly is the number
     *	 corresponding to the line the option is on.
     *	Let's also assume that cur_choice is the number of the
     *	 choice that should be initially selected, with 0 being
     *	 the first choice.
     *	So what we have, is the top equal to the current screen line
     *	 subtracting the cur_choice + 1 (the one must be for the top
     *	 line we will draw in a box).  If the top goes under 0, then
     *	 consider it 0.
     */
    top = ly - (cur_choice + 1);
    if (top < 0)
	top = 0;

    /*
     *	Check and see if we need to put the i_length parameter up to
     *	the number of real choices.
     */
    if (i_length < 1) {
	i_length = num_choices;
    } else {
	/*
	 *  Otherwise, it is really one number too high.
	 */
	i_length--;
    }

    /*
     *	The bottom is the value of the top plus the number of choices
     *	to view plus 3 (one for the top line, one for the bottom line,
     *	and one to offset the 0 counted in the num_choices).
     */
    bottom = top + i_length + 3;

    /*
     *	Hmm...	If the bottom goes beyond the number of lines available,
     */
    if (bottom > DisplayLines) {
	/*
	 *  Position the window at the top if we have more
	 *  choices than will fit in the window.
	 */
	if ((i_length + 3) > DisplayLines) {
	    top = 0;
	    bottom = (top + (i_length + 3));
	    if (bottom > DisplayLines)
		bottom = (DisplayLines + 1);
	} else {
	    /*
	     *	Try to position the window so that the selected choice will
	     *	  appear where the choice box currently is positioned.
	     *	It could end up too high, at this point, but we'll move it
	     *	  down latter, if that has happened.
	     */
	    top = (DisplayLines + 1) - (i_length + 3);
	    bottom = (DisplayLines + 1);
	}
    }

    /*
     *	This is really fun, when the length is 4, it means 0 to 4, or 5.
     */
    length = (bottom - top) - 2;

    /*
     *	Move the window down if it's too high.
     */
    if (bottom < ly + 2) {
	bottom = ly + 2;
	if (bottom > DisplayLines + 1)
	    bottom = DisplayLines + 1;
	top = bottom - length - 2;
    }

    /*
     *	Set up the overall window, including the boxing characters ('*'),
     *	if it all fits.  Otherwise, set up the widest window possible. - FM
     */
#ifdef USE_SLANG
    SLsmg_fill_region(top, lx - 1, bottom - top, (Lnum + width + 4), ' ');
#else
    if (!(form_window = newwin(bottom - top, (Lnum + width + 4),
			       top, (lx - 1))) &&
	!(form_window = newwin(bottom - top, 0, top, 0))) {
	option_statusline(POPUP_FAILED);
	return(orig_choice);
    }
    scrollok(form_window, TRUE);
#ifdef PDCURSES
    keypad(form_window, TRUE);
#endif /* PDCURSES */
#ifdef NCURSES
    LYsubwindow(form_window);
#endif
#if defined(HAVE_GETBKGD) /* not defined in ncurses 1.8.7 */
    wbkgd(form_window, getbkgd(stdscr));
    wbkgdset(form_window, getbkgd(stdscr));
#endif
#endif /* USE_SLANG */

    /*
     *	Clear the command line and write
     *	the popup statusline. - FM
     */
    move((LYlines - 2), 0);
    clrtoeol();
    if (disabled) {
	option_statusline(CHOICE_LIST_UNM_MSG);
    } else {
	option_statusline(CHOICE_LIST_MESSAGE);
    }

    /*
     *	Set up the window_offset for choices.
     *	 cur_choice ranges from 0...n
     *	 length ranges from 0...m
     */
    if (cur_choice >= length) {
	window_offset = cur_choice - length + 1;
    }

    /*
     *	Compute the number of popup window pages. - FM
     */
    npages = ((num_choices + 1) > length) ?
		(((num_choices + 1) + (length - 1))/(length))
					  : 1;
/*
 *  OH!  I LOVE GOTOs! hack hack hack
 */
redraw:
    Cptr = choices;

    /*
     *	Display the boxed choices.
     */
    for (i = 0; i <= num_choices; i++) {
	if (i >= window_offset && i - window_offset < length) {
	    sprintf(Cnum, "%s%d: ",
			   ((num_choices > 8 && i < 9) ?
						   " " : ""),
			   (i + 1));
#ifdef USE_SLANG
	    SLsmg_gotorc(top + ((i + 1) - window_offset), (lx - 1 + 2));
	    addstr(Cnum);
	    SLsmg_write_nstring(Cptr[i], width);
#else
	    wmove(form_window, ((i + 1) - window_offset), 2);
	    wclrtoeol(form_window);
	    waddstr(form_window, Cnum);
	    waddstr(form_window, Cptr[i]);
#endif /* USE_SLANG */
	}
    }
#ifdef USE_SLANG
    SLsmg_draw_box(top, (lx - 1), (bottom - top), (Lnum + width + 4));
#else
#ifdef VMS
    VMSbox(form_window, (bottom - top), (Lnum + width + 4));
#else
    LYbox(form_window, FALSE);
#endif /* VMS */
    wrefresh(form_window);
#endif /* USE_SLANG */
    Cptr = NULL;

    /*
     *	Loop on user input.
     */
    while (cmd != LYK_ACTIVATE) {
	/*
	 *  Unreverse cur choice.
	 */
	if (Cptr != NULL) {
	    sprintf(Cnum, "%s%d: ",
			  ((num_choices > 8 && i < 9) ?
						  " " : ""),
			  (i + 1));
#ifdef USE_SLANG
	    SLsmg_gotorc((top + ((i + 1) - window_offset)), (lx - 1 + 2));
	    addstr(Cnum);
	    SLsmg_write_nstring(Cptr[i], width);
#else
	    wmove(form_window, ((i + 1) - window_offset), 2);
	    waddstr(form_window, Cnum);
	    waddstr(form_window, Cptr[i]);
#endif /* USE_SLANG */
	}
	Cptr = choices;
	i = cur_choice;
	sprintf(Cnum, "%s%d: ",
		      ((num_choices > 8 && i < 9) ?
					      " " : ""),
		      (i + 1));
#ifdef USE_SLANG
	SLsmg_gotorc((top + ((i + 1) - window_offset)), (lx - 1 + 2));
	addstr(Cnum);
	SLsmg_set_color(2);
	SLsmg_write_nstring(Cptr[i], width);
	SLsmg_set_color(0);
	/*
	 *  If LYShowCursor is ON, move the cursor to the left
	 *  of the current choice, so that blind users, who are
	 *  most likely to have LYShowCursor ON, will have it's
	 *  string spoken or passed to the braille interface as
	 *  each choice is made current.  Otherwise, move it to
	 *  the bottom, right column of the screen, to "hide"
	 *  the cursor as for the main document, and let sighted
	 *  users rely on the current choice's highlighting or
	 *  color without the distraction of a blinking cursor
	 *  in the window. - FM
	 */
	if (LYShowCursor)
	    SLsmg_gotorc((top + ((i + 1) - window_offset)), (lx - 1 + 1));
	else
	    SLsmg_gotorc((LYlines - 1), (LYcols - 1));
	SLsmg_refresh();
#else
	wmove(form_window, ((i + 1) - window_offset), 2);
	waddstr(form_window, Cnum);
	wstart_reverse(form_window);
	waddstr(form_window, Cptr[i]);
	wstop_reverse(form_window);
	/*
	 *  If LYShowCursor is ON, move the cursor to the left
	 *  of the current choice, so that blind users, who are
	 *  most likely to have LYShowCursor ON, will have it's
	 *  string spoken or passed to the braille interface as
	 *  each choice is made current.  Otherwise, leave it to
	 *  the right of the current choice, since we can't move
	 *  it out of the window, and let sighted users rely on
	 *  the highlighting of the current choice without the
	 *  distraction of a blinking cursor preceding it. - FM
	 */
	if (LYShowCursor)
	    wmove(form_window, ((i + 1) - window_offset), 1);
	wrefresh(form_window);
#endif /* USE_SLANG  */

	term_options = FALSE;
	c = LYgetch();
	if (term_options || c == 3 || c == 7) {
	     /*
	      *  Control-C or Control-G
	      */
	    cmd = LYK_QUIT;
	} else {
	    cmd = keymap[c+1];
	}
#ifdef VMS
	if (HadVMSInterrupt) {
	    HadVMSInterrupt = FALSE;
	    cmd = LYK_QUIT;
	}
#endif /* VMS */

	switch(cmd) {
	    case LYK_F_LINK_NUM:
		c = '\0';
	    case LYK_1:
	    case LYK_2:
	    case LYK_3:
	    case LYK_4:
	    case LYK_5:
	    case LYK_6:
	    case LYK_7:
	    case LYK_8:
	    case LYK_9:
		/*
		 *  Get a number from the user, possibly with
		 *  a 'g' or 'p' suffix (which will be loaded
		 *  into c). - FM & LE
		 */
		number = get_popup_choice_number((int *)&c);

		/*
		 *  Check for a 'p' suffix. - FM
		 */
		if (c == 'p') {
		    /*
		     *	Treat 1 or less as the first page. - FM
		     */
		    if (number <= 1) {
			if (window_offset == 0) {
			    option_statusline(ALREADY_AT_CHOICE_BEGIN);
			    sleep(MessageSecs);
			    if (disabled) {
				option_statusline(CHOICE_LIST_UNM_MSG);
			    } else {
				option_statusline(CHOICE_LIST_MESSAGE);
			    }
			    break;
			}
			window_offset = 0;
			cur_choice = 0;
			if (disabled) {
			    option_statusline(CHOICE_LIST_UNM_MSG);
			} else {
			    option_statusline(CHOICE_LIST_MESSAGE);
			}
			goto redraw;
		    }

		    /*
		     *	Treat a number equal to or greater than the
		     *	number of pages as the last page. - FM
		     */
		    if (number >= npages) {
			if (window_offset >= ((num_choices - length) + 1)) {
			    option_statusline(ALREADY_AT_CHOICE_END);
			    sleep(MessageSecs);
			    if (disabled) {
				option_statusline(CHOICE_LIST_UNM_MSG);
			    } else {
				option_statusline(CHOICE_LIST_MESSAGE);
			    }
			    break;
			}
			window_offset = ((npages - 1) * length);
			if (window_offset > (num_choices - length)) {
			    window_offset = (num_choices - length + 1);
			}
			if (cur_choice < window_offset)
			    cur_choice = window_offset;
			if (disabled) {
			    option_statusline(CHOICE_LIST_UNM_MSG);
			} else {
			    option_statusline(CHOICE_LIST_MESSAGE);
			}
			goto redraw;
		    }

		    /*
		     *	We want an intermediate page. - FM
		     */
		    if (((number - 1) * length) == window_offset) {
			sprintf(buffer, ALREADY_AT_CHOICE_PAGE, number);
			option_statusline(buffer);
			sleep(MessageSecs);
			if (disabled) {
			    option_statusline(CHOICE_LIST_UNM_MSG);
			} else {
			    option_statusline(CHOICE_LIST_MESSAGE);
			}
			break;
		    }
		    cur_choice = window_offset = ((number - 1) * length);
		    if (disabled) {
			option_statusline(CHOICE_LIST_UNM_MSG);
		    } else {
			option_statusline(CHOICE_LIST_MESSAGE);
		    }
		    goto redraw;

		}

		/*
		 *  Check for a positive number, which signifies
		 *  that a choice should be sought. - FM
		 */
		if (number > 0) {
		    /*
		     *	Decrement the number so as to correspond
		     *	with our cur_choice values. - FM
		     */
		    number--;

		    /*
		     *	If the number is in range and had no legal
		     *	suffix, select the indicated choice. - FM
		     */
		    if (number <= num_choices && c == '\0') {
			cur_choice = number;
			cmd = LYK_ACTIVATE;
			break;
		    }

		    /*
		     *	Verify that we had a 'g' suffix,
		     *	and act on the number. - FM
		     */
		    if (c == 'g') {
			if (cur_choice == number) {
			    /*
			     *	The choice already is current. - FM
			     */
			    sprintf(buffer,
				    CHOICE_ALREADY_CURRENT, (number + 1));
			    option_statusline(buffer);
			    sleep(MessageSecs);
			    if (disabled) {
				option_statusline(CHOICE_LIST_UNM_MSG);
			    } else {
				option_statusline(CHOICE_LIST_MESSAGE);
			    }
			    break;
			}

			if (number <= num_choices) {
			    /*
			     *	The number is in range and had a 'g'
			     *	suffix, so make it the current choice,
			     *	scrolling if needed. - FM
			     */
			    j = (number - cur_choice);
			    cur_choice = number;
			    if ((j > 0) &&
				(cur_choice - window_offset) >= length) {
				window_offset += j;
				if (window_offset > (num_choices - length + 1))
				    window_offset = (num_choices - length + 1);
			    } else if ((cur_choice - window_offset) < 0) {
				window_offset -= abs(j);
				if (window_offset < 0)
				    window_offset = 0;
			    }
			    if (disabled) {
				option_statusline(CHOICE_LIST_UNM_MSG);
			    } else {
				option_statusline(CHOICE_LIST_MESSAGE);
			    }
			    goto redraw;
			}

			/*
			 *  Not in range. - FM
			 */
			option_statusline(BAD_CHOICE_NUM_ENTERED);
			sleep(MessageSecs);
		    }
		}

		/*
		 *  Restore the popup statusline. - FM
		 */
		if (disabled) {
		    option_statusline(CHOICE_LIST_UNM_MSG);
		} else {
		    option_statusline(CHOICE_LIST_MESSAGE);
		}
		break;

	    case LYK_PREV_LINK:
	    case LYK_UP_LINK:

		if (cur_choice > 0)
		    cur_choice--;

		/*
		 *  Scroll the window up if necessary.
		 */
		if ((cur_choice - window_offset) < 0) {
		    window_offset--;
		    goto redraw;
		}
		break;

	    case LYK_NEXT_LINK:
	    case LYK_DOWN_LINK:
		if (cur_choice < num_choices)
		    cur_choice++;

		/*
		 *  Scroll the window down if necessary
		 */
		if ((cur_choice - window_offset) >= length) {
		    window_offset++;
		    goto redraw;
		}
		break;

	    case LYK_NEXT_PAGE:
		/*
		 *  Okay, are we on the last page of the choices list?
		 *  If not then,
		 */
		if (window_offset != (num_choices - length + 1)) {
		    /*
		     *	Modify the current choice to not be a
		     *	coordinate in the list, but a coordinate
		     *	on the item selected in the window.
		     */
		    cur_choice -= window_offset;

		    /*
		     *	Page down the proper length for the list.
		     *	If simply to far, back up.
		     */
		    window_offset += length;
		    if (window_offset > (num_choices - length)) {
			window_offset = (num_choices - length + 1);
		    }

		    /*
		     *	Readjust the current choice to be a choice
		     *	list coordinate rather than window.
		     *	Redraw this thing.
		     */
		    cur_choice += window_offset;
		    goto redraw;
		}
		else if (cur_choice < num_choices) {
		    /*
		     *	Already on last page of the choice list so
		     *	just redraw it with the last item selected.
		     */
		    cur_choice = num_choices;
		}
		break;

	    case LYK_PREV_PAGE:
		/*
		 *  Are we on the first page of the choice list?
		 *  If not then,
		 */
		if (window_offset != 0) {
		    /*
		     *	Modify the current choice to not be a choice
		     *	list coordinate, but a window coordinate.
		     */
		    cur_choice -= window_offset;

		    /*
		     *	Page up the proper length.
		     *	If too far, back up.
		     */
		    window_offset -= length;
		    if (window_offset < 0) {
			window_offset = 0;
		    }

		    /*
		     *	Readjust the current choice.
		     */
		    cur_choice += window_offset;
		    goto redraw;
		} else if (cur_choice > 0) {
		    /*
		     *	Already on the first page so just
		     *	back up to the first item.
		     */
		    cur_choice = 0;
		}
		break;

	    case LYK_HOME:
		cur_choice = 0;
		if (window_offset > 0) {
		    window_offset = 0;
		    goto redraw;
		}
		break;

	    case LYK_END:
		cur_choice = num_choices;
		if (window_offset != (num_choices - length + 1)) {
		    window_offset = (num_choices - length + 1);
		    goto redraw;
		}
		break;

	    case LYK_DOWN_TWO:
		cur_choice += 2;
		if (cur_choice > num_choices)
		    cur_choice = num_choices;

		/*
		 *  Scroll the window down if necessary.
		 */
		if ((cur_choice - window_offset) >= length) {
		    window_offset += 2;
		    if (window_offset > (num_choices - length + 1))
			window_offset = (num_choices - length + 1);
		    goto redraw;
		}
		break;

	    case LYK_UP_TWO:
		cur_choice -= 2;
		if (cur_choice < 0)
		    cur_choice = 0;

		/*
		 *  Scroll the window up if necessary.
		 */
		if ((cur_choice - window_offset) < 0) {
		    window_offset -= 2;
		    if (window_offset < 0)
			window_offset = 0;
		    goto redraw;
		}
		break;

	    case LYK_DOWN_HALF:
		cur_choice += (length/2);
		if (cur_choice > num_choices)
		    cur_choice = num_choices;

		/*
		 *  Scroll the window down if necessary.
		 */
		if ((cur_choice - window_offset) >= length) {
		    window_offset += (length/2);
		    if (window_offset > (num_choices - length + 1))
			window_offset = (num_choices - length + 1);
		    goto redraw;
		}
		break;

	    case LYK_UP_HALF:
		cur_choice -= (length/2);
		if (cur_choice < 0)
		    cur_choice = 0;

		/*
		 *  Scroll the window up if necessary.
		 */
		if ((cur_choice - window_offset) < 0) {
		    window_offset -= (length/2);
		    if (window_offset < 0)
			window_offset = 0;
		    goto redraw;
		}
		break;

	    case LYK_REFRESH:
		lynx_force_repaint();
		refresh();
		break;

	    case LYK_NEXT:
		if (recall && *prev_target_buffer == '\0') {
		    /*
		     *	We got a 'n'ext command with no prior query
		     *	specified within the popup window.  See if
		     *	one was entered when the popup was retracted,
		     *	and if so, assume that's what's wanted.  Note
		     *	that it will become the default within popups,
		     *	unless another is entered within a popup.  If
		     *	the within popup default is to be changed at
		     *	that point, use WHEREIS ('/') and enter it,
		     *	or the up- or down-arrow keys to seek any of
		     *	the previously entered queries, regardless of
		     *	whether they were entered within or outside
		     *	of a popup window. - FM
		     */
		    if ((cp = (char *)HTList_objectAt(search_queries,
						      0)) != NULL) {
			strcpy(prev_target_buffer, cp);
			QueryNum = 0;
			FirstRecall = FALSE;
		    }
		}
		strcpy(prev_target, prev_target_buffer);
	    case LYK_WHEREIS:
		if (*prev_target == '\0' ) {
		    option_statusline(ENTER_WHEREIS_QUERY);
		    if ((ch = LYgetstr(prev_target, VISIBLE,
				       sizeof(prev_target_buffer),
				       recall)) < 0) {
			/*
			 *  User cancelled the search via ^G. - FM
			 */
			option_statusline(CANCELLED);
			sleep(InfoSecs);
			goto restore_popup_statusline;
		    }
		}

check_recall:
		if (*prev_target == '\0' &&
		    !(recall && (ch == UPARROW || ch == DNARROW))) {
		    /*
		     *	No entry.  Simply break.   - FM
		     */
		    option_statusline(CANCELLED);
		    sleep(InfoSecs);
		    goto restore_popup_statusline;
		}

		if (recall && ch == UPARROW) {
		    if (FirstRecall) {
			/*
			 *  Use the current string or
			 *  last query in the list. - FM
			 */
			FirstRecall = FALSE;
			if (*prev_target_buffer) {
			    for (QueryNum = (QueryTotal - 1);
				 QueryNum > 0; QueryNum--) {
				if ((cp = (char *)HTList_objectAt(
							search_queries,
							QueryNum)) != NULL &&
				    !strcmp(prev_target_buffer, cp)) {
				    break;
				}
			    }
			} else {
			    QueryNum = 0;
			}
		    } else {
			/*
			 *  Go back to the previous query in the list. - FM
			 */
			QueryNum++;
		    }
		    if (QueryNum >= QueryTotal)
			/*
			 *  Roll around to the last query in the list. - FM
			 */
			QueryNum = 0;
		    if ((cp = (char *)HTList_objectAt(search_queries,
						      QueryNum)) != NULL) {
			strcpy(prev_target, cp);
			if (*prev_target_buffer &&
			    !strcmp(prev_target_buffer, prev_target)) {
			    option_statusline(EDIT_CURRENT_QUERY);
			} else if ((*prev_target_buffer && QueryTotal == 2) ||
				   (!(*prev_target_buffer) &&
				      QueryTotal == 1)) {
			    option_statusline(EDIT_THE_PREV_QUERY);
			} else {
			    option_statusline(EDIT_A_PREV_QUERY);
			}
			if ((ch = LYgetstr(prev_target, VISIBLE,
				sizeof(prev_target_buffer), recall)) < 0) {
			    /*
			     *	User cancelled the search via ^G. - FM
			     */
			    option_statusline(CANCELLED);
			    sleep(InfoSecs);
			    goto restore_popup_statusline;
			}
			goto check_recall;
		    }
		} else if (recall && ch == DNARROW) {
		    if (FirstRecall) {
		    /*
		     *	Use the current string or
		     *	first query in the list. - FM
		     */
		    FirstRecall = FALSE;
		    if (*prev_target_buffer) {
			for (QueryNum = 0;
			     QueryNum < (QueryTotal - 1); QueryNum++) {
			    if ((cp = (char *)HTList_objectAt(
							search_queries,
							QueryNum)) != NULL &&
				!strcmp(prev_target_buffer, cp)) {
				    break;
			    }
			}
		    } else {
			QueryNum = (QueryTotal - 1);
		    }
		} else {
		    /*
		     *	Advance to the next query in the list. - FM
		     */
		    QueryNum--;
		}
		if (QueryNum < 0)
		    /*
		     *	Roll around to the first query in the list. - FM
		     */
		    QueryNum = (QueryTotal - 1);
		    if ((cp = (char *)HTList_objectAt(search_queries,
						      QueryNum)) != NULL) {
			strcpy(prev_target, cp);
			if (*prev_target_buffer &&
			    !strcmp(prev_target_buffer, prev_target)) {
			    option_statusline(EDIT_CURRENT_QUERY);
			} else if ((*prev_target_buffer &&
				    QueryTotal == 2) ||
				   (!(*prev_target_buffer) &&
				    QueryTotal == 1)) {
			    option_statusline(EDIT_THE_PREV_QUERY);
			} else {
			    option_statusline(EDIT_A_PREV_QUERY);
			}
			if ((ch = LYgetstr(prev_target, VISIBLE,
					   sizeof(prev_target_buffer),
					   recall)) < 0) {
			    /*
			     * User cancelled the search via ^G. - FM
			     */
			    option_statusline(CANCELLED);
			    sleep(InfoSecs);
			    goto restore_popup_statusline;
			}
			goto check_recall;
		    }
		}
		/*
		 *  Replace the search string buffer with the new target. - FM
		 */
		strcpy(prev_target_buffer, prev_target);
		HTAddSearchQuery(prev_target_buffer);

		/*
		 *  Start search at the next choice. - FM
		 */
		for (j = 1; Cptr[i+j] != NULL; j++) {
		    sprintf(buffer, "%s%d: %s",
				    ((num_choices > 8 && (j + i) < 9) ?
								  " " : ""),
				    (i + j + 1),
				    Cptr[i+j]);
		    if (case_sensitive) {
			if (strstr(buffer, prev_target_buffer) != NULL)
			    break;
		    } else {
			if (LYstrstr(buffer, prev_target_buffer) != NULL)
			    break;
		    }
		}
		if (Cptr[i+j] != NULL) {
		    /*
		     *	We have a hit, so make that choice the current. - FM
		     */
		    cur_choice += j;
		    /*
		     *	Scroll the window down if necessary.
		     */
		    if ((cur_choice - window_offset) >= length) {
			window_offset += j;
			if (window_offset > (num_choices - length + 1))
			    window_offset = (num_choices - length + 1);
			ReDraw = TRUE;
		    }
		    goto restore_popup_statusline;
		}

		/*
		 *  If we started at the beginning, it can't be present. - FM
		 */
		if (cur_choice == 0) {
		    option_user_message(STRING_NOT_FOUND, prev_target_buffer);
		    sleep(MessageSecs);
		    goto restore_popup_statusline;
		}

		/*
		 *  Search from the beginning to the current choice. - FM
		 */
		for (j = 0; j < cur_choice; j++) {
		    sprintf(buffer, "%s%d: %s",
				    ((num_choices > 8 && j < 9) ?
							    " " : ""),
				    (j + 1),
				    Cptr[j]);
		    if (case_sensitive) {
			if (strstr(buffer, prev_target_buffer) != NULL)
			    break;
		    } else {
			if (LYstrstr(buffer, prev_target_buffer) != NULL)
			    break;
		    }
		}
		if (j < cur_choice) {
		    /*
		     *	We have a hit, so make that choice the current. - FM
		     */
		    j = (cur_choice - j);
		    cur_choice -= j;
		    /*
		     *	Scroll the window up if necessary.
		     */
		    if ((cur_choice - window_offset) < 0) {
			window_offset -= j;
			if (window_offset < 0)
			    window_offset = 0;
			ReDraw = TRUE;
		    }
		    goto restore_popup_statusline;
		}

		/*
		 *  Didn't find it in the preceding choices either. - FM
		 */
		option_user_message(STRING_NOT_FOUND, prev_target_buffer);
		sleep(MessageSecs);

restore_popup_statusline:
		/*
		 *  Restore the popup statusline and
		 *  reset the search variables. - FM
		 */
		if (disabled)
		    option_statusline(CHOICE_LIST_UNM_MSG);
		else
		    option_statusline(CHOICE_LIST_MESSAGE);
		*prev_target = '\0';
		QueryTotal = (search_queries ? HTList_count(search_queries)
					     : 0);
		recall = ((QueryTotal >= 1) ? RECALL : NORECALL);
		QueryNum = QueryTotal;
		if (ReDraw == TRUE) {
		    ReDraw = FALSE;
		    goto redraw;
		}
		break;

	    case LYK_QUIT:
	    case LYK_ABORT:
	    case LYK_PREV_DOC:
		cur_choice = orig_choice;
		term_options = TRUE;
		option_statusline(CANCELLED);
		sleep(MessageSecs);
		cmd = LYK_ACTIVATE; /* to exit */
		break;
	}
    }
#ifndef USE_SLANG
    delwin(form_window);
#ifdef NCURSES
    LYsubwindow(0);
#endif
#endif /* !USE_SLANG */

    if (disabled || term_options) {
	option_statusline("");
	return(orig_choice);
    } else {
	option_statusline(VALUE_ACCEPTED);
	return(cur_choice);
    }
}
