#include <HTUtils.h>
#include <HTFTP.h>
#include <HTTP.h>  /* 'reloading' flag */
#include <HTML.h>
#include <LYCurses.h>
#include <LYUtils.h>
#include <LYStrings.h>
#include <LYGlobalDefs.h>
#include <LYHistory.h>
#include <LYOptions.h>
#include <LYSignal.h>
#include <LYClean.h>
#include <LYCharSets.h>
#include <UCMap.h>
#include <UCAux.h>
#include <LYKeymap.h>
#include <LYrcFile.h>
#include <HTAlert.h>
#include <LYBookmark.h>
#include <GridText.h>
#include <LYGetFile.h>
#include <LYReadCFG.h>
#include <LYPrettySrc.h>

#include <LYLeaks.h>

BOOLEAN term_options;

PRIVATE void terminate_options	PARAMS((int sig));

#if !defined(NO_OPTION_MENU) || defined(NCURSES_MOUSE_VERSION)
#define COL_OPTION_VALUES 36  /* display column where option values start */
#endif

#ifndef NO_OPTION_MENU
PRIVATE int boolean_choice PARAMS((
	int		status,
	int		line,
	int		column,
	char **		choices));

#define MAXCHOICES 10

/*
 *  Values for the options menu. - FM
 *
 *  L_foo values are the Y coordinates for the menu item.
 *  B_foo values are the X coordinates for the item's prompt string.
 *  C_foo values are the X coordinates for the item's value string.
 */
#define L_EDITOR	 2
#define L_DISPLAY	 3

#define L_HOME		 4
#define C_MULTI		24
#define B_BOOK		34
#define C_DEFAULT	50

#define L_FTPSTYPE	 5
#define L_MAIL_ADDRESS	 6
#define L_SSEARCH	 7
#define L_LANGUAGE	 8
#define L_PREF_CHARSET	 9
#define L_ASSUME_CHARSET (L_PREF_CHARSET + 1)
#define L_CHARSET	10
#define L_RAWMODE	11

#define L_COLOR		L_RAWMODE
#define B_COLOR		44
#define C_COLOR		62

#define L_BOOL_A	12
#define B_VIKEYS	5
#define C_VIKEYS	15
#define B_EMACSKEYS	22
#define C_EMACSKEYS	36
#define B_SHOW_DOTFILES	44
#define C_SHOW_DOTFILES	62

#define L_BOOL_B	13
#define B_SELECT_POPUPS	5
#define C_SELECT_POPUPS	36
#define B_SHOW_CURSOR	44
#define C_SHOW_CURSOR	62

#define L_KEYPAD	14
#define L_LINEED	15
#define L_LAYOUT	16

#ifdef DIRED_SUPPORT
#define L_DIRED		17
#define L_USER_MODE	18
#define L_USER_AGENT	19
#define L_EXEC		20
#else
#define L_USER_MODE	17
#define L_USER_AGENT	18
#define L_EXEC		19
#endif /* DIRED_SUPPORT */

#define L_VERBOSE_IMAGES L_USER_MODE
#define B_VERBOSE_IMAGES 50
#define C_VERBOSE_IMAGES (B_VERBOSE_IMAGES + 21)

/* a kludge to add assume_charset only in ADVANCED mode... */
#define L_Bool_A (use_assume_charset ? L_BOOL_A + 1 : L_BOOL_A)
#define L_Bool_B (use_assume_charset ? L_BOOL_B + 1 : L_BOOL_B)
#define L_Exec (use_assume_charset ? L_EXEC + 1 : L_EXEC)
#define L_Rawmode (use_assume_charset ? L_RAWMODE + 1 : L_RAWMODE)
#define L_Charset (use_assume_charset ? L_CHARSET + 1 : L_CHARSET)
#define L_Color (use_assume_charset ? L_COLOR + 1 : L_COLOR)
#define L_Keypad (use_assume_charset ? L_KEYPAD + 1 : L_KEYPAD)
#define L_Lineed (use_assume_charset ? L_LINEED + 1 : L_LINEED)
#define L_Layout (use_assume_charset ? L_LAYOUT + 1 : L_LAYOUT)
#define L_Dired (use_assume_charset ? L_DIRED + 1 : L_DIRED)
#define L_User_Mode (use_assume_charset ? L_USER_MODE + 1 : L_USER_MODE)
#define L_User_Agent (use_assume_charset ? L_USER_AGENT + 1 : L_USER_AGENT)

#define LPAREN '('
#define RPAREN ')'

PRIVATE int add_it ARGS2(char *, text, int, len)
{
    if (len) {
	text[len] = '\0';
	addstr(text);
    }
    return 0;
}

/*
 * addlbl() is used instead of plain addstr() in old-style options menu
 * to show hot keys in bold.
 */
PRIVATE void addlbl ARGS1(CONST char *, text)
{
    char actual[80];
    int s, d;
    BOOL b = FALSE;

    for (s = d = 0; text[s]; s++) {
	actual[d++] = text[s];
	if (text[s] == LPAREN) {
	    d = add_it(actual, d-1);
	    start_bold();
	    b = TRUE;
	    actual[d++] = text[s];
	} else if (text[s] == RPAREN) {
	    d = add_it(actual, d);
	    stop_bold();
	    b = FALSE;
	}
    }
    add_it(actual, d);
    if (b)
	stop_bold();
}

PUBLIC void LYoptions NOARGS
{
#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
    int itmp;
#endif /* ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS */
    int response, ch;
    /*
     *	If the user changes the display we need memory to put it in.
     */
    char display_option[256];
    char *choices[MAXCHOICES];
    int CurrentCharSet = current_char_set;
    int CurrentAssumeCharSet = UCLYhndl_for_unspec;
    int CurrentShowColor = LYShowColor;
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
	HTAlert(OPTION_SCREEN_NEEDS_23);
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
    LYStatusLine = (LYlines - 1);	/* screen is otherwise too crowded */
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
    addlbl("(E)ditor                     : ");
    addstr((editor && *editor) ? editor : "NONE");

    move(L_DISPLAY, 5);
    addlbl("(D)ISPLAY variable           : ");
    addstr((x_display && *x_display) ? x_display : "NONE");

    move(L_HOME, 5);
    addlbl("mu(L)ti-bookmarks: ");
    addstr((LYMultiBookmarks ?
	      (LYMBMAdvanced ? "ADVANCED"
			     : "STANDARD")
			     : "OFF     "));
    move(L_HOME, B_BOOK);
    if (LYMultiBookmarks) {
	addlbl("review/edit (B)ookmarks files");
    } else {
	addlbl("(B)ookmark file: ");
	addstr((bookmark_page && *bookmark_page) ? bookmark_page : "NONE");
    }

    move(L_FTPSTYPE, 5);
    addlbl("(F)TP sort criteria          : ");
    addstr((HTfileSortMethod == FILE_BY_NAME ? "By Filename" :
	   (HTfileSortMethod == FILE_BY_SIZE ? "By Size    " :
	   (HTfileSortMethod == FILE_BY_TYPE ? "By Type    " :
					       "By Date    "))));

    move(L_MAIL_ADDRESS, 5);
    addlbl("(P)ersonal mail address      : ");
    addstr((personal_mail_address && *personal_mail_address) ?
				       personal_mail_address : "NONE");

    move(L_SSEARCH, 5);
    addlbl("(S)earching type             : ");
    addstr(case_sensitive ? "CASE SENSITIVE  " : "CASE INSENSITIVE");

    move(L_Charset, 5);
    addlbl("display (C)haracter set      : ");
    addstr((char *)LYchar_set_names[current_char_set]);

    move(L_LANGUAGE, 5);
    addlbl("preferred document lan(G)uage: ");
    addstr((language && *language) ? language : "NONE");

    move(L_PREF_CHARSET, 5);
    addlbl("preferred document c(H)arset : ");
    addstr((pref_charset && *pref_charset) ? pref_charset : "NONE");

    if (use_assume_charset) {
	move(L_ASSUME_CHARSET, 5);
	addlbl("(^A)ssume charset if unknown : ");
	if (UCAssume_MIMEcharset)
	    addstr(UCAssume_MIMEcharset);
	else
	    addstr((UCLYhndl_for_unspec >= 0) ?
		   (char *)LYCharSet_UC[UCLYhndl_for_unspec].MIMEname
					      : "NONE");
    }

    move(L_Rawmode, 5);
    addlbl("Raw 8-bit or CJK m(O)de      : ");
    addstr(LYRawMode ? "ON " : "OFF");

#if defined(USE_SLANG) || defined(COLOR_CURSES)
    move(L_Color, B_COLOR);
    addlbl("show color (&)  : ");
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
    addlbl("(V)I keys: ");
    addstr(vi_keys ? "ON " : "OFF");

    move(L_Bool_A, B_EMACSKEYS);
    addlbl("e(M)acs keys: ");
    addstr(emacs_keys ? "ON " : "OFF");

    move(L_Bool_A, B_SHOW_DOTFILES);
    addlbl("sho(W) dot files: ");
    addstr((!no_dotfiles && show_dotfiles) ? "ON " : "OFF");

    move(L_Bool_B, B_SELECT_POPUPS);
    addlbl("popups for selec(T) fields   : ");
    addstr(LYSelectPopups ? "ON " : "OFF");

    move(L_Bool_B, B_SHOW_CURSOR);
    addlbl("show cursor (@) : ");
    addstr(LYShowCursor ? "ON " : "OFF");

    move(L_Keypad, 5);
    addlbl("(K)eypad mode                : ");
    addstr((keypad_mode == NUMBERS_AS_ARROWS) ?
				"Numbers act as arrows             " :
	 ((keypad_mode == LINKS_ARE_NUMBERED) ?
				"Links are numbered                " :
				"Links and form fields are numbered"));

    move(L_Lineed, 5);
    addlbl("li(N)e edit style            : ");
    addstr(LYLineeditNames[current_lineedit]);

#ifdef EXP_KEYBOARD_LAYOUT
    move(L_Layout, 5);
    addlbl("Ke(Y)board layout            : ");
    addstr(LYKbLayoutNames[current_layout]);
#endif

#ifdef DIRED_SUPPORT
    move(L_Dired, 5);
    addlbl("l(I)st directory style       : ");
    addstr((dir_list_style == FILES_FIRST) ? "Files first      " :
	  ((dir_list_style == MIXED_STYLE) ? "Mixed style      " :
					     "Directories first"));
#endif /* DIRED_SUPPORT */

    move(L_User_Mode, 5);
    addlbl("(U)ser mode                  : ");
    addstr(  (user_mode == NOVICE_MODE) ? "Novice      " :
      ((user_mode == INTERMEDIATE_MODE) ? "Intermediate" :
					  "Advanced    "));

    addlbl("  verbose images (!) : ");
    addstr( verbose_img ? "ON " : "OFF" );

    move(L_User_Agent, 5);
    addlbl("user (A)gent                 : ");
    addstr((LYUserAgent && *LYUserAgent) ? LYUserAgent : "NONE");

#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
    move(L_Exec, 5);
    addlbl("local e(X)ecution links      : ");
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
	    _statusline(VALUE_ACCEPTED);
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
		    _statusline(EDIT_DISABLED);
		} else if (system_editor ) {
		    _statusline(EDITOR_LOCKED);
		} else {
		    if (editor && *editor)
			strcpy(display_option, editor);
		    else {  /* clear the NONE */
			move(L_EDITOR, COL_OPTION_VALUES);
			addstr("    ");
			*display_option = '\0';
		    }
		    _statusline(ACCEPT_DATA);
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
			HTInfoMsg(CANCELLED);
			HTInfoMsg("");
		    } else {
			_statusline(VALUE_ACCEPTED);
		    }
		}
		response = ' ';
		break;

	    case 'd':	/* Change the display. */
	    case 'D':
		if (x_display && *x_display) {
		    strcpy(display_option, x_display);
		} else {  /* clear the NONE */
		    move(L_DISPLAY, COL_OPTION_VALUES);
		    addstr("    ");
		    *display_option = '\0';
		}
		_statusline(ACCEPT_DATA);
		move(L_DISPLAY, COL_OPTION_VALUES);
		start_bold();
		ch = LYgetstr(display_option, VISIBLE,
			      sizeof(display_option), NORECALL);
		stop_bold();
		move(L_DISPLAY, COL_OPTION_VALUES);
		if ((term_options || ch == -1) ||
		    (x_display != NULL &&
#ifdef VMS
		     !strcasecomp(x_display, display_option)))
#else
		     !strcmp(x_display, display_option)))
#endif /* VMS */
		{
		    /*
		     *	Cancelled, or a non-NULL display string
		     *	wasn't changed. - FM
		     */
		    addstr((x_display && *x_display) ? x_display : "NONE");
		    clrtoeol();
		    if (ch == -1) {
			HTInfoMsg(CANCELLED);
			HTInfoMsg("");
		    } else {
			_statusline(VALUE_ACCEPTED);
		    }
		    response = ' ';
		    break;
		} else if (*display_option == '\0') {
		    if ((x_display == NULL) ||
			(x_display != NULL && *x_display == '\0')) {
			/*
			 *  NULL or zero-length display string
			 *  wasn't changed. - FM
			 */
			addstr("NONE");
			clrtoeol();
			_statusline(VALUE_ACCEPTED);
			response = ' ';
			break;
		    }
		}
		/*
		 *  Set the new DISPLAY variable. - FM
		 */
		LYsetXDisplay(display_option);
		if ((cp = LYgetXDisplay()) != NULL) {
		    StrAllocCopy(x_display, cp);
		} else {
		    FREE(x_display);
		}
		cp = NULL;
		addstr(x_display ? x_display : "NONE");
		clrtoeol();
		if ((x_display == NULL && *display_option == '\0') ||
		    (x_display != NULL &&
		     !strcmp(x_display, display_option))) {
		    if (x_display == NULL &&
			LYisConfiguredForX == TRUE) {
			_statusline(VALUE_ACCEPTED_WARNING_X);
		    } else if (x_display != NULL &&
			LYisConfiguredForX == FALSE) {
			_statusline(VALUE_ACCEPTED_WARNING_NONX);
		    } else {
			_statusline(VALUE_ACCEPTED);
		    }
		} else {
		    if (*display_option) {
			_statusline(FAILED_TO_SET_DISPLAY);
		    } else {
			_statusline(FAILED_CLEAR_SET_DISPLAY);
		    }
		}
		response = ' ';
		break;

	    case 'l':	/* Change multibookmarks option. */
	    case 'L':
		if (LYMBMBlocked) {
		    _statusline(MULTIBOOKMARKS_DISALLOWED);
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
			addstr(gettext("review/edit B)ookmarks files"));
		    } else {
			addstr(gettext("B)ookmark file: "));
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
		    _statusline(ACCEPT_DATA);
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
			_statusline(USE_PATH_OFF_HOME);
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
			HTInfoMsg(CANCELLED);
			HTInfoMsg("");
		    } else {
			_statusline(VALUE_ACCEPTED);
		    }
		} else { /* anonymous */
		    _statusline(BOOKMARK_CHANGE_DISALLOWED);
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
		_statusline(ACCEPT_DATA);
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
		    HTInfoMsg(CANCELLED);
		    HTInfoMsg("");
		} else {
		    _statusline(VALUE_ACCEPTED);
		}
		response = ' ';
		break;

	    case 's':	/* Change case sensitivity for searches. */
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
		     *	Set the raw 8-bit or CJK mode defaults and
		     *	character set if changed. - FM
		     */
		    if (CurrentAssumeCharSet != UCLYhndl_for_unspec ||
			UCLYhndl_for_unspec != curval) {
			if (UCLYhndl_for_unspec != CurrentAssumeCharSet) {
			    StrAllocCopy(UCAssume_MIMEcharset,
					 LYCharSet_UC[UCLYhndl_for_unspec].MIMEname);
			}
			LYRawMode = (UCLYhndl_for_unspec == current_char_set);
			HTMLSetUseDefaultRawMode(current_char_set, LYRawMode);
			HTMLSetCharacterHandling(current_char_set);
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
		    _statusline(NEED_ADVANCED_USER_MODE);
		    AddValueAccepted = FALSE;
		}
		break;

	    case 'c':	/* Change display charset setting. */
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
		_statusline(ACCEPT_DATA);
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
		    HTInfoMsg(CANCELLED);
		    HTInfoMsg("");
		} else {
		    _statusline(VALUE_ACCEPTED);
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
		_statusline(ACCEPT_DATA);
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
		    HTInfoMsg(CANCELLED);
		    HTInfoMsg("");
		} else {
		    _statusline(VALUE_ACCEPTED);
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
		    _statusline(DOTFILE_ACCESS_DISABLED);
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
			    HTUserMsg2(
				COLOR_TOGGLE_DISABLED_FOR_TERM,
				terminal);
			else
			    HTUserMsg(COLOR_TOGGLE_DISABLED);
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
				HTUserMsg2(
				    COLOR_TOGGLE_DISABLED_FOR_TERM,
				    terminal);
			    else
				HTUserMsg(COLOR_TOGGLE_DISABLED);
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

#ifdef EXP_KEYBOARD_LAYOUT
	    case 'y':	/* Change keyboard layout */
	    case 'Y':
		if (!LYSelectPopups) {
		    current_layout = boolean_choice(current_layout,
						      L_Layout, -1,
						      LYKbLayoutNames);
		} else {
		    current_layout = popup_choice(current_layout,
						    L_Layout, -1,
						    LYKbLayoutNames,
						    0, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		    move(L_Layout, COL_OPTION_VALUES);
		    clrtoeol();
		    addstr(LYKbLayoutNames[current_layout]);
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
#endif /* EXP_KEYBOARD_LAYOUT */

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

	    case '!':
		/*
		 *  Copy strings into choice array.
		 */
		choices[0] = NULL;
		StrAllocCopy(choices[0], "OFF");
		choices[1] = NULL;
		StrAllocCopy(choices[1], "ON ");
		choices[2] = NULL;
		if (!LYSelectPopups) {
		    verbose_img = boolean_choice(verbose_img,
						L_VERBOSE_IMAGES,
						C_VERBOSE_IMAGES,
						choices);
		} else {
		    verbose_img = popup_choice(verbose_img,
					     L_VERBOSE_IMAGES,
					     C_VERBOSE_IMAGES,
					     choices,
					     2, FALSE);
		}
		FREE(choices[0]);
		FREE(choices[1]);
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
		    _statusline(ACCEPT_DATA_OR_DEFAULT);
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
			HTInfoMsg(CANCELLED);
			HTInfoMsg("");
		    } else if (LYUserAgent && *LYUserAgent &&
			!strstr(LYUserAgent, "Lynx") &&
			!strstr(LYUserAgent, "lynx")) {
			_statusline(UA_COPYRIGHT_WARNING);
		    } else {
			_statusline(VALUE_ACCEPTED);
		    }
		} else { /* disallowed */
		    _statusline(UA_COPYRIGHT_WARNING);
		}
		response = ' ';
		break;

#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
	    case 'x':	/* Change local exec restriction. */
	    case 'X':
		if (exec_frozen && !LYSelectPopups) {
		    _statusline(CHANGE_OF_SETTING_DISALLOWED);
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
		    _statusline(SAVING_OPTIONS);
		    if (save_rc()) {
			LYrcShowColor = LYChosenShowColor;
			_statusline(OPTIONS_SAVED);
		    } else {
			HTAlert(OPTIONS_NOT_SAVED);
		    }
		} else {
		    _statusline(R_TO_RETURN_TO_LYNX);
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
		    _statusline(SAVE_OR_R_TO_RETURN_TO_LYNX);
		} else {
		    _statusline(R_TO_RETURN_TO_LYNX);
		}
	}  /* end switch */
    }  /* end while */

    term_options = FALSE;
    LYStatusLine = -1;		/* let user_mode have some of the screen */
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
    _statusline(ANY_KEY_CHANGE_RET_ACCEPT);

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
		case LYK_FASTBACKW_LINK:
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
		HTInfoMsg(CANCELLED);
		HTInfoMsg("");
	    } else {
		_statusline(VALUE_ACCEPTED);
	    }
	    return(cur_choice);
	}
    }
}
#endif /* !NO_OPTION_MENU */

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
		      a <= (MBM_current * MBM_V_MAXFILES/2 ); a++) {
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
		_statusline(SAVING_OPTIONS);
		if (save_rc())
		    _statusline(OPTIONS_SAVED);
		else
		    HTAlert(OPTIONS_NOT_SAVED);
	    } else {
		_statusline(R_TO_RETURN_TO_LYNX);
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
		_statusline(ACCEPT_DATA);

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

#if !defined(NO_OPTION_MENU) || defined(NCURSES_MOUSE_VERSION)
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
    _statusline(OPTION_CHOICE_NUMBER);

    /*
     *	Get the number, possibly with a suffix, from the user.
     */
    if (LYgetstr(temp, VISIBLE, sizeof(temp), NORECALL) < 0 ||
	*temp == 0 || term_options) {
	HTInfoMsg(CANCELLED);
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
PUBLIC int popup_choice ARGS6(
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
    int num_choices = 0, top, bottom, length = -1;
    unsigned width = 0;
    char ** Cptr = choices;
    int window_offset = 0;
    int DisplayLines = (LYlines - 2);
    char Cnum[64];
    int Lnum;
    int npages;
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
	HTAlert(POPUP_FAILED);
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
	_statusline(CHOICE_LIST_UNM_MSG);
    } else {
	_statusline(CHOICE_LIST_MESSAGE);
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
	c = LYgetch_for(FOR_CHOICE);
	if (term_options || c == 3 || c == 7) {
	    cmd = LYK_QUIT;
#ifndef USE_SLANG
	} else if (c == MOUSE_KEY) {
	    if ((cmd = fancy_mouse(form_window, i + 1 + window_offset, &cur_choice)) < 0)
		goto redraw;
	    if  (cmd == LYK_ACTIVATE)
		break;
#endif
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
			    HTUserMsg(ALREADY_AT_CHOICE_BEGIN);
			    if (disabled) {
				_statusline(CHOICE_LIST_UNM_MSG);
			    } else {
				_statusline(CHOICE_LIST_MESSAGE);
			    }
			    break;
			}
			window_offset = 0;
			cur_choice = 0;
			if (disabled) {
			    _statusline(CHOICE_LIST_UNM_MSG);
			} else {
			    _statusline(CHOICE_LIST_MESSAGE);
			}
			goto redraw;
		    }

		    /*
		     *	Treat a number equal to or greater than the
		     *	number of pages as the last page. - FM
		     */
		    if (number >= npages) {
			if (window_offset >= ((num_choices - length) + 1)) {
			    HTUserMsg(ALREADY_AT_CHOICE_END);
			    if (disabled) {
				_statusline(CHOICE_LIST_UNM_MSG);
			    } else {
				_statusline(CHOICE_LIST_MESSAGE);
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
			    _statusline(CHOICE_LIST_UNM_MSG);
			} else {
			    _statusline(CHOICE_LIST_MESSAGE);
			}
			goto redraw;
		    }

		    /*
		     *	We want an intermediate page. - FM
		     */
		    if (((number - 1) * length) == window_offset) {
			sprintf(buffer, ALREADY_AT_CHOICE_PAGE, number);
			HTUserMsg(buffer);
			if (disabled) {
			    _statusline(CHOICE_LIST_UNM_MSG);
			} else {
			    _statusline(CHOICE_LIST_MESSAGE);
			}
			break;
		    }
		    cur_choice = window_offset = ((number - 1) * length);
		    if (disabled) {
			_statusline(CHOICE_LIST_UNM_MSG);
		    } else {
			_statusline(CHOICE_LIST_MESSAGE);
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
			    HTUserMsg(buffer);
			    if (disabled) {
				_statusline(CHOICE_LIST_UNM_MSG);
			    } else {
				_statusline(CHOICE_LIST_MESSAGE);
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
				_statusline(CHOICE_LIST_UNM_MSG);
			    } else {
				_statusline(CHOICE_LIST_MESSAGE);
			    }
			    goto redraw;
			}

			/*
			 *  Not in range. - FM
			 */
			HTUserMsg(BAD_CHOICE_NUM_ENTERED);
		    }
		}

		/*
		 *  Restore the popup statusline. - FM
		 */
		if (disabled) {
		    _statusline(CHOICE_LIST_UNM_MSG);
		} else {
		    _statusline(CHOICE_LIST_MESSAGE);
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
		    _statusline(ENTER_WHEREIS_QUERY);
		    if ((ch = LYgetstr(prev_target, VISIBLE,
				       sizeof(prev_target_buffer),
				       recall)) < 0) {
			/*
			 *  User cancelled the search via ^G. - FM
			 */
			HTInfoMsg(CANCELLED);
			goto restore_popup_statusline;
		    }
		}

check_recall:
		if (*prev_target == '\0' &&
		    !(recall && (ch == UPARROW || ch == DNARROW))) {
		    /*
		     *	No entry.  Simply break.   - FM
		     */
		    HTInfoMsg(CANCELLED);
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
			    _statusline(EDIT_CURRENT_QUERY);
			} else if ((*prev_target_buffer && QueryTotal == 2) ||
				   (!(*prev_target_buffer) &&
				      QueryTotal == 1)) {
			    _statusline(EDIT_THE_PREV_QUERY);
			} else {
			    _statusline(EDIT_A_PREV_QUERY);
			}
			if ((ch = LYgetstr(prev_target, VISIBLE,
				sizeof(prev_target_buffer), recall)) < 0) {
			    /*
			     *	User cancelled the search via ^G. - FM
			     */
			    HTInfoMsg(CANCELLED);
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
			    _statusline(EDIT_CURRENT_QUERY);
			} else if ((*prev_target_buffer &&
				    QueryTotal == 2) ||
				   (!(*prev_target_buffer) &&
				    QueryTotal == 1)) {
			    _statusline(EDIT_THE_PREV_QUERY);
			} else {
			    _statusline(EDIT_A_PREV_QUERY);
			}
			if ((ch = LYgetstr(prev_target, VISIBLE,
					   sizeof(prev_target_buffer),
					   recall)) < 0) {
			    /*
			     * User cancelled the search via ^G. - FM
			     */
			    HTInfoMsg(CANCELLED);
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
		    HTUserMsg2(STRING_NOT_FOUND, prev_target_buffer);
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
		HTUserMsg2(STRING_NOT_FOUND, prev_target_buffer);

restore_popup_statusline:
		/*
		 *  Restore the popup statusline and
		 *  reset the search variables. - FM
		 */
		if (disabled)
		    _statusline(CHOICE_LIST_UNM_MSG);
		else
		    _statusline(CHOICE_LIST_MESSAGE);
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
		HTUserMsg(CANCELLED);
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
	_statusline("");
	return(orig_choice);
    } else {
	_statusline(VALUE_ACCEPTED);
	return(cur_choice);
    }
}

#endif /* !NO_OPTION_MENU */

#ifndef NO_OPTION_FORMS

/*
 * I'm paranoid about mistyping strings.  Also, this way they get combined
 * so we don't have to worry about the intelligence of the compiler.
 * We don't need to burn memory like it's cheap.  We're better than that.
 */
#define SELECTED(flag) (flag) ? selected_string : ""
#define DISABLED(flag) (flag) ? disabled_string : ""
#define NOTEMPTY(text) (text && text[0]) ? text : ""

typedef struct {
    int value;
    CONST char *LongName;
    CONST char *HtmlName;
} OptValues;

typedef struct {
    char * tag;
    char * value;
} PostPair;

static CONST char selected_string[] = "selected";
static CONST char disabled_string[] = "disabled";
static CONST char on_string[]	    = "ON";
static CONST char off_string[]	    = "OFF";
static CONST char never_string[]    = "NEVER";
static CONST char always_string[]   = "ALWAYS";
static OptValues bool_values[] = {
	{ FALSE,	     "OFF",		  "OFF"		},
	{ TRUE,		     "ON",		  "ON"		},
	{ 0, 0, 0 }};

static char * secure_string		= "secure";
static char * secure_value		= NULL;
static char * save_options_string	= "save_options";

/*
 * Personal Preferences
 */
static char * cookies_string		= "cookies";
static char * cookies_ignore_all_string = "ignore";
static char * cookies_up_to_user_string = "ask user";
static char * cookies_accept_all_string = "accept all";
static char * x_display_string		= "display";
static char * editor_string		= "editor";
static char * emacs_keys_string		= "emacs_keys";

#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
#define EXEC_ALWAYS 2
#define EXEC_LOCAL  1
#define EXEC_NEVER  0
static char * exec_links_string		= "exec_options";
static OptValues exec_links_values[]	= {
	{ EXEC_NEVER,	"ALWAYS OFF",		"ALWAYS OFF" },
	{ EXEC_LOCAL,	"FOR LOCAL FILES ONLY",	"FOR LOCAL FILES ONLY" },
#ifndef NEVER_ALLOW_REMOTE_EXEC
	{ EXEC_ALWAYS,	"ALWAYS ON",		"ALWAYS ON" },
#endif
	{ 0, 0, 0 }};
#endif /* ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS */

#ifdef EXP_KEYBOARD_LAYOUT
static char * kblayout_string		= "kblayout";
#endif
static char * keypad_mode_string	= "keypad_mode";
static OptValues keypad_mode_values[]	= {
	{ NUMBERS_AS_ARROWS,  "Numbers act as arrows", "number_arrows" },
	{ LINKS_ARE_NUMBERED, "Links are numbered",    "links_numbered" },
	{ LINKS_AND_FORM_FIELDS_ARE_NUMBERED,
			      "Links and form fields are numbered",
			      "links_and_forms" },
	{ 0, 0, 0 }};
static char * lineedit_style_string	= "lineedit_style";
static char * mail_address_string	= "mail_address";
static char * search_type_string	= "search_type";
static OptValues search_type_values[] = {
	{ FALSE,	    "Case insensitive",  "case_insensitive" },
	{ TRUE,		    "Case sensitive",	 "case_sensitive" },
	{ 0, 0, 0 }};
#if defined(USE_SLANG) || defined(COLOR_CURSES)
static char * show_color_string		= "show_color";
static OptValues show_color_values[] = {
	{ SHOW_COLOR_NEVER,	never_string,	never_string },
	{ SHOW_COLOR_OFF,	off_string,	off_string },
	{ SHOW_COLOR_ON,	on_string,	on_string },
	{ SHOW_COLOR_ALWAYS,	always_string,	always_string },
	{ 0, 0, 0 }};
#endif
static char * show_cursor_string	= "show_cursor";
static char * user_mode_string		= "user_mode";
static OptValues user_mode_values[] = {
	{ NOVICE_MODE,		"Novice",	"Novice" },
	{ INTERMEDIATE_MODE,	"Intermediate", "Intermediate" },
	{ ADVANCED_MODE,	"Advanced",	"Advanced" },
	{ 0, 0, 0 }};
static char * vi_keys_string		= "vi_keys";

/*
 * Document Layout
 */
static char * DTD_recovery_string      = "DTD";
static OptValues DTD_type_values[] = {
	/* Old_DTD variable */
	{ TRUE,		    "relaxed (TagSoup mode)",	 "tagsoup" },
	{ FALSE,	    "strict (SortaSGML mode)",	 "sortasgml" },
	{ 0, 0, 0 }};
static char * select_popups_string     = "select_popups";
static char * images_string            = "images";
static char * images_ignore_all_string  = "ignore";
static char * images_use_label_string   = "as labels";
static char * images_use_links_string   = "as links";
static char * verbose_images_string    = "verbose_images";
static OptValues verbose_images_type_values[] = {
	/* verbose_img variable */
	{ FALSE,	    "OFF",		 "OFF" },
	{ TRUE,		    "show filename",	 "ON" },
	{ 0, 0, 0 }};

/*
 * Bookmark Options
 */
static char * mbm_advanced_string	= "ADVANCED";
static char * mbm_off_string		= "OFF";
static char * mbm_standard_string	= "STANDARD";
static char * mbm_string		= "multi_bookmarks_mode";
static char * single_bookmark_string	= "single_bookmark_name";

/*
 * Character Set Options
 */
static char * assume_char_set_string	= "assume_char_set";
static char * display_char_set_string	= "display_char_set";
static char * raw_mode_string		= "raw_mode";

/*
 * File Management Options
 */
static char * show_dotfiles_string	= "show_dotfiles";
#ifdef DIRED_SUPPORT
static char * dired_sort_string		= "dired_sort";
static OptValues dired_values[] = {
	{ 0,			"Directories first",	"dired_dir" },
	{ FILES_FIRST,		"Files first",		"dired_files" },
	{ MIXED_STYLE,		"Mixed style",		"dired_mixed" },
	{ 0, 0, 0 }};
#endif /* DIRED_SUPPORT */
static char * ftp_sort_string = "ftp_sort";
static OptValues ftp_sort_values[] = {
	{ FILE_BY_NAME,		"By Name",		"ftp_by_name" },
	{ FILE_BY_TYPE,		"By Type",		"ftp_by_type" },
	{ FILE_BY_SIZE,		"By Size",		"ftp_by_size" },
	{ FILE_BY_DATE,		"By Date",		"ftp_by_date" },
	{ 0, 0, 0 }};

/*
 * Headers transferred to remote server
 */
static char * preferred_doc_char_string = "preferred_doc_char";
static char * preferred_doc_lang_string = "preferred_doc_lang";
static char * user_agent_string		= "user_agent";

#define PutLabel(fp, text) \
	fprintf(fp,"  %-33s: ", text)

#define PutTextInput(fp, Name, Value, Size, disable) \
	fprintf(fp,\
	"<input size=%d type=\"text\" name=\"%s\" value=\"%s\" %s>\n",\
		(int) Size, Name, Value, disable)

#define PutOption(fp, flag, html, name) \
	fprintf(fp,"<option value=\"%s\" %s>%s\n", html, SELECTED(flag), name)

#define BeginSelect(fp, text) \
	fprintf(fp,"<select name=\"%s\">\n", text)

#define MaybeSelect(fp, flag, text) \
	fprintf(fp,"<select name=\"%s\" %s>\n", text, DISABLED(flag))

#define EndSelect(fp)\
	fprintf(fp,"</select>\n")

PRIVATE void PutOptValues ARGS3(
	FILE *,		fp,
	int,		value,
	OptValues *,	table)
{
    while (table->LongName != 0) {
	PutOption(fp,
	    value == table->value,
	    table->HtmlName,
	    table->LongName);
	table++;
    }
}

PRIVATE BOOLEAN GetOptValues ARGS3(
	OptValues *,	table,
	char *,		value,
	int *,		result)
{
    while (table->LongName != 0) {
	if (!strcmp(value, table->HtmlName)) {
	    *result = table->value;
	    return TRUE;
	}
	table++;
    }
    return FALSE;
}

/*
 * Break cgi line into array of pairs of pointers.  Don't bother trying to
 * be efficient.  We're not called all that often.
 * We come in with a string looking like:
 * tag1=value1&tag2=value2&...&tagN=valueN
 * We leave with an array of post_pairs.  The last element in the array
 * will have a tag pointing to NULL.
 * Not pretty, but works.  Hey, if strings can be null terminate arrays...
 */

PRIVATE PostPair * break_data ARGS1(
    char *,	data)
{
    char * p = data;
    PostPair * q = NULL;
    int count = 0;

    if (p==NULL || p[0]=='\0')
	return NULL;

    q = calloc(sizeof(PostPair), 1);
    if (q==NULL)
	outofmem(__FILE__, "break_data(calloc)");

    do {
	/*
	 * First, break up on '&', sliding 'p' on down the line.
	 */
	q[count].value = LYstrsep(&p, "&");
	/*
	 * Then break up on '=', sliding value down, and setting tag.
	 */
	q[count].tag = LYstrsep(&(q[count].value), "=");

	/*
	 * Clean them up a bit, in case user entered a funky string.
	 */
	HTUnEscape(q[count].tag);

	/* In the value field we have '+' instead of ' '. So do a simple
	 * find&replace on the value field before UnEscaping() - SKY
	 */
	{
	   size_t i, len;
	   len = strlen(q[count].value);
	   for (i = 0; i < len; i++) {
		if (q[count].value[i] == '+') {
#ifdef UNIX
		    /*
		     * Allow for special case of options which begin with a "+" on
		     * Unix - TD
		     */
		    if (i > 0
		    && q[count].value[i+1] == '+'
		    && isalnum(q[count].value[i+2])) {
			q[count].value[i++] = ' ';
			i++;
			continue;
		    }

#endif
		    q[count].value[i] = ' ';
		}
	   }
	}
	HTUnEscape(q[count].value);

	count++;
	/*
	 * Like I said, screw efficiency.  Sides, realloc is fast on
	 * Linux ;->
	 */
	q = realloc(q, sizeof(PostPair)*(count+1));
	if (q==NULL)
	    outofmem(__FILE__, "break_data(realloc)");
	q[count].tag=NULL;
    } while (p!=NULL && p[0]!='\0');
    return q;
}

PRIVATE int gen_options PARAMS((char **newfile));
/*
 * Handle options from the pseudo-post.  I think we really only need
 * post_data here, but bring along everything just in case.  It's only a
 * pointer.  MRC
 *
 * Options are processed in order according to gen_options(), we should not
 * depend on it and add boolean flags where the order is essential (save,
 * character sets...)
 *
 * Security:  some options are disabled in gen_options() under certain
 * conditions.  We *should* duplicate the same conditions here in postoptions()
 * to prevent user with a limited access from editing HTML options code
 * manually (e.g., doing 'e'dit in 'o'ptions) and submit it to access the
 * restricted items.  Prevent spoofing attempts from index overrun. - LP
 *
 * Exit status: NULLFILE (reload) or NORMAL (use HText cache).
 *
 * On exit, got the document which was current before the Options menu:
 *
 *   (from cache) nothing changed or no visual effect supposed:
 *             editor name, e-mail, etc.
 *
 *   (reload locally) to see the effect of certain changes:
 *             display_char_set, assume_charset, etc.
 *             (use 'need_reload' flag where necessary).
 *
 *   (reload from remote server and uncache on a proxy)
 *             few options changes should be transferred to remote server:
 *             preferred language, fake browser name, etc.
 *             (use 'need_end_reload' flag).
 */

PUBLIC int postoptions ARGS1(
    document *,		newdoc)
{
    PostPair *data = 0;
    DocAddress WWWDoc;  /* need on exit */
    int i;
    int code;
    BOOLEAN save_all = FALSE;
    int display_char_set_old = current_char_set;
    BOOLEAN raw_mode_old = LYRawMode;
    BOOLEAN assume_char_set_changed = FALSE;
    BOOLEAN need_reload = FALSE;
    BOOLEAN need_end_reload = FALSE;
#if defined(USE_SLANG) || defined(COLOR_CURSES)
    int CurrentShowColor = LYShowColor;
#endif

    /*-------------------------------------------------
     * kludge a link from mbm_menu, the URL was:
     * "<a href=\"LYNXOPTIONS://MBM_MENU\">Goto multi-bookmark menu</a>\n"
     *--------------------------------------------------*/

    if (strstr(newdoc->address, "LYNXOPTIONS://MBM_MENU")) {
	FREE(newdoc->post_data);
	if (!no_bookmark)
	   edit_bookmarks();
	else /* anonymous */
	   HTAlert(BOOKMARK_CHANGE_DISALLOWED);
	return(NULLFILE);
    }


    /*-------------------------------------------------
     * kludge gen_options() call:
     *--------------------------------------------------*/

    if (strstr(newdoc->address, "LYNXOPTIONS:/") && !newdoc->post_data) {
	int status = gen_options(&newdoc->address);
	if (status == NOT_FOUND)
	    return(NOT_FOUND);

	/* exit to getfile() cyrcle */
	WWWDoc.address = newdoc->address;
	WWWDoc.post_data = newdoc->post_data;
	WWWDoc.post_content_type = newdoc->post_content_type;
	WWWDoc.bookmark = newdoc->bookmark;
	WWWDoc.isHEAD = newdoc->isHEAD;
	WWWDoc.safe = newdoc->safe;

	if (!HTLoadAbsolute(&WWWDoc))
	    return(NOT_FOUND);
	return(NORMAL);
    }


    data = break_data(newdoc->post_data);

    for (i = 0; data[i].tag != NULL; i++) {
	/*
	 * Paranoid security.
	 */
	if (!strcmp(data[i].tag, secure_string)) {
	    if (!secure_value || strcmp(data[i].value, secure_value)) {
		/*
		 * FIXME: We've been spoofed message here.
		 */
		FREE(data);
		return(NULLFILE);
	    }
	    FREE(secure_value);
	}

	/* Save options */
	if (!strcmp(data[i].tag, save_options_string) && (!no_option_save)) {
	    save_all = TRUE;
	}

	/* Cookies: SELECT */
	if (!strcmp(data[i].tag, cookies_string)) {
	    if (!strcmp(data[i].value, cookies_ignore_all_string)) {
		LYSetCookies = FALSE;
	    } else if (!strcmp(data[i].value, cookies_up_to_user_string)) {
		LYSetCookies = TRUE;
		LYAcceptAllCookies = FALSE;
	    } else if (!strcmp(data[i].value, cookies_accept_all_string)) {
		LYSetCookies = TRUE;
		LYAcceptAllCookies = TRUE;
	    }
	}

	/* X Display: INPUT */
	if (!strcmp(data[i].tag, x_display_string)) {
	    LYsetXDisplay(data[i].value);
	}

	/* Editor: INPUT */
	if (!strcmp(data[i].tag, editor_string)) {
	    FREE(editor);
	    StrAllocCopy(editor, data[i].value);
	}

	/* Emacs keys: ON/OFF */
	if (!strcmp(data[i].tag, emacs_keys_string)
	 && GetOptValues(bool_values, data[i].value, &code)) {
	    if ((emacs_keys = code) != FALSE) {
		set_emacs_keys();
	    } else {
		reset_emacs_keys();
	    }
	}

	/* Execution links: SELECT */
#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
	if (!strcmp(data[i].tag, exec_links_string)
	 && GetOptValues(exec_links_values, data[i].value, &code)) {
#ifndef NEVER_ALLOW_REMOTE_EXEC
	    local_exec = (code == EXEC_ALWAYS);
#endif /* !NEVER_ALLOW_REMOTE_EXEC */
	    local_exec_on_local_files = (code == EXEC_LOCAL);
	}
#endif /* ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS */

	/* Keypad Mode: SELECT */
	if (!strcmp(data[i].tag, keypad_mode_string)) {
	    int newval;
	    if (GetOptValues(keypad_mode_values, data[i].value, &newval)
		 && keypad_mode != newval) {
		keypad_mode = newval;
		need_reload = TRUE;
	    }
	}

	/* Line edit style: SELECT */
	if (!strcmp(data[i].tag, lineedit_style_string)) {
	    int newval = atoi(data[i].value);
	    int j;
	    /* prevent spoofing attempt */
	    for (j = 0; LYLineeditNames[j]; j++) {
	       if (j==newval)  current_lineedit = newval;
	    }
	}

#ifdef EXP_KEYBOARD_LAYOUT
	/* Keyboard layout: SELECT */
	if (!strcmp(data[i].tag, kblayout_string)) {
	    int newval = atoi(data[i].value);
	    int j;
	    /* prevent spoofing attempt */
	    for (j = 0; LYKbLayoutNames[j]; j++) {
	       if (j==newval)  current_layout = newval;
	    }
	}
#endif /* EXP_KEYBOARD_LAYOUT */

	/* Mail Address: INPUT */
	if (!strcmp(data[i].tag, mail_address_string)) {
	    FREE(personal_mail_address);
	    StrAllocCopy(personal_mail_address, data[i].value);
	}

	/* Search Type: SELECT */
	if (!strcmp(data[i].tag, search_type_string)
	 && GetOptValues(search_type_values, data[i].value, &code)) {
	    case_sensitive = code;
	}

	/* HTML error tolerance: SELECT */
	if (!strcmp(data[i].tag, DTD_recovery_string)
	 && GetOptValues(DTD_type_values, data[i].value, &code)) {
	    if (Old_DTD != code) {
		Old_DTD = code;
		HTSwitchDTD(!Old_DTD);
		need_reload = TRUE;
	    }
	}

	/* Select Popups: ON/OFF */
	if (!strcmp(data[i].tag, select_popups_string)
	 && GetOptValues(bool_values, data[i].value, &code)) {
	    LYSelectPopups = code;
	}

#if defined(USE_SLANG) || defined(COLOR_CURSES)
	/* Show Color: SELECT */
	if (!strcmp(data[i].tag, show_color_string)
	 && GetOptValues(show_color_values, data[i].value, &LYShowColor)) {
	    LYChosenShowColor = LYShowColor;
	    if (CurrentShowColor != LYShowColor) {
		lynx_force_repaint();
	    }
	    CurrentShowColor = LYShowColor;
#ifdef USE_SLANG
	    SLtt_Use_Ansi_Colors = (LYShowColor > 1 ? 1 : 0);
#endif
	}
#endif /* USE_SLANG || COLOR_CURSES */

	/* Show Cursor: ON/OFF */
	if (!strcmp(data[i].tag, show_cursor_string)
	 && GetOptValues(bool_values, data[i].value, &code)) {
	    LYShowCursor = code;
	}

	/* User Mode: SELECT */
	if (!strcmp(data[i].tag, user_mode_string)
	 && GetOptValues(user_mode_values, data[i].value, &user_mode)) {
	    if (user_mode == NOVICE_MODE) {
		display_lines = (LYlines - 4);
	    } else {
		display_lines = LYlines-2;
	    }
	}

	/* Show Images: SELECT */
	if (!strcmp(data[i].tag, images_string)) {
	    if (!strcmp(data[i].value, images_ignore_all_string)
			&& !(pseudo_inline_alts == FALSE && clickable_images == FALSE)) {
		 pseudo_inline_alts = FALSE;
		 clickable_images = FALSE;
		need_reload = TRUE;
	    } else if (!strcmp(data[i].value, images_use_label_string)
			&& !(pseudo_inline_alts == TRUE && clickable_images == FALSE)) {
		 pseudo_inline_alts = TRUE;
		 clickable_images = FALSE;
		need_reload = TRUE;
	    } else if (!strcmp(data[i].value, images_use_links_string)
			&& !(clickable_images == TRUE)) {
		clickable_images = TRUE;
		need_reload = TRUE;
	    }
	}

	/* Verbose Images: ON/OFF */
	if (!strcmp(data[i].tag, verbose_images_string)
	 && GetOptValues(verbose_images_type_values, data[i].value, &code)) {
	    if (verbose_img != code) {
		verbose_img = code;
		need_reload = TRUE;
	    }
	}

	/* VI Keys: ON/OFF */
	if (!strcmp(data[i].tag, vi_keys_string)
	 && GetOptValues(bool_values, data[i].value, &code)) {
	    if ((vi_keys = code) != FALSE) {
		set_vi_keys();
	    } else {
		reset_vi_keys();
	    }
	}

	/* Bookmarks File Menu: SELECT */
	if (!strcmp(data[i].tag, mbm_string) && (!LYMBMBlocked)) {
	    if (!strcmp(data[i].value, mbm_off_string)) {
		LYMultiBookmarks = FALSE;
	    } else if (!strcmp(data[i].value, mbm_standard_string)) {
		LYMultiBookmarks = TRUE;
		LYMBMAdvanced = FALSE;
	    } else if (!strcmp(data[i].value, mbm_advanced_string)) {
		LYMultiBookmarks = TRUE;
		LYMBMAdvanced = TRUE;
	    }
	}

	/* Default Bookmarks filename: INPUT */
	if (!strcmp(data[i].tag, single_bookmark_string) && (!no_bookmark)) {
	    if (strcmp(data[i].value, "")) {
		FREE(bookmark_page);
		StrAllocCopy(bookmark_page, data[i].value);
	    }
	}

	/* Assume Character Set: SELECT */
	if (!strcmp(data[i].tag, assume_char_set_string)) {
	    int newval = UCGetLYhndl_byMIME(data[i].value);

	    if (newval >= 0
	     && ((raw_mode_old &&
		     newval != safeUCGetLYhndl_byMIME(UCAssume_MIMEcharset))
	      || (!raw_mode_old &&
		     newval != UCLYhndl_for_unspec)
		)) {

		UCLYhndl_for_unspec = newval;
		StrAllocCopy(UCAssume_MIMEcharset, data[i].value);
		assume_char_set_changed = TRUE;
	    }
	}

	/* Display Character Set: SELECT */
	if (!strcmp(data[i].tag, display_char_set_string)) {
	    int newval = atoi(data[i].value);
	    int j;
	    /* prevent spoofing attempt */
	    for (j = 0; LYchar_set_names[j]; j++) {
		if (j==newval)	current_char_set = newval;
	    }
	}

	/* Raw Mode: ON/OFF */
	if (!strcmp(data[i].tag, raw_mode_string)
	 && GetOptValues(bool_values, data[i].value, &code)) {
	    LYRawMode = code;
	}

	/*
	 * ftp sort: SELECT
	 */
	if (!strcmp(data[i].tag, ftp_sort_string)) {
	    GetOptValues(ftp_sort_values, data[i].value, &HTfileSortMethod);
	}

#ifdef DIRED_SUPPORT
	/* Local Directory Sort: SELECT */
	if (!strcmp(data[i].tag, dired_sort_string)) {
	    GetOptValues(dired_values, data[i].value, &dir_list_style);
	}
#endif /* DIRED_SUPPORT */

	/* Show dot files: ON/OFF */
	if (!strcmp(data[i].tag, show_dotfiles_string) && (!no_dotfiles)
	 && GetOptValues(bool_values, data[i].value, &code)) {
	    show_dotfiles = code;
	}

	/* Preferred Document Character Set: INPUT */
	if (!strcmp(data[i].tag, preferred_doc_char_string)) {
	    if (strcmp(pref_charset, data[i].value)) {
		FREE(pref_charset);
		StrAllocCopy(pref_charset, data[i].value);
		need_end_reload = TRUE;
	   }
	}

	/* Preferred Document Language: INPUT */
	if (!strcmp(data[i].tag, preferred_doc_lang_string)) {
	    if (strcmp(language, data[i].value)) {
		FREE(language);
		StrAllocCopy(language, data[i].value);
		need_end_reload = TRUE;
	    }
	}

	/* User Agent: INPUT */
	if (!strcmp(data[i].tag, user_agent_string) && (!no_useragent)) {
	    if (strcmp(LYUserAgent, data[i].value)) {
		need_end_reload = TRUE;
		FREE(LYUserAgent);
		/* ignore Copyright warning ? */
		StrAllocCopy(LYUserAgent,
		   *(data[i].value)
		   ? data[i].value
		   : LYUserAgentDefault);
		if (LYUserAgent && *LYUserAgent &&
		   !strstr(LYUserAgent, "Lynx") &&
		   !strstr(LYUserAgent, "lynx")) {
		    HTAlert(UA_COPYRIGHT_WARNING);
		}
	    }
	}
    } /* end of loop */

    /*
     * Process the flags:
     */
     if ( display_char_set_old != current_char_set ||
	       raw_mode_old != LYRawMode ||
	       assume_char_set_changed ) {
	/*
	 * charset settings: the order is essential here.
	 */
       if (display_char_set_old != current_char_set) {
		/*
		 *  Set the LYUseDefaultRawMode value and character
		 *  handling if LYRawMode was changed. - FM
		 */
		LYUseDefaultRawMode = TRUE;
		HTMLUseCharacterSet(current_char_set);
	    }
	if (assume_char_set_changed) {
		LYRawMode = (UCLYhndl_for_unspec == current_char_set);
	    }
       if (raw_mode_old != LYRawMode || assume_char_set_changed) {
		/*
		 *  Set the raw 8-bit or CJK mode defaults and
		 *  character set if changed. - FM
		 */
		HTMLSetUseDefaultRawMode(current_char_set, LYRawMode);
		HTMLSetCharacterHandling(current_char_set);
	    }
	need_reload = TRUE;
     } /* end of charset settings */


    /*
     * FIXME: Golly gee, we need to write all of this out now, don't we?
     */
    FREE(newdoc->post_data);
    FREE(data);
    if (save_all) {
	_statusline(SAVING_OPTIONS);
	if (save_rc()) {
	    _statusline(OPTIONS_SAVED);
	} else {
	    HTAlert(OPTIONS_NOT_SAVED);
	}
    }

    /*
     *  Exit: working around the previous document.
     *  Being out of mainloop()/getfile() cycle, do things manually.
     */
    CTRACE(tfp, "\nLYOptions.c/postoptions(): exiting...\n");
    CTRACE(tfp, "                            need_reload = %s\n",
                    need_reload ? "TRUE" : "FALSE");
    CTRACE(tfp, "                            need_end_reload = %s\n",
                    need_end_reload ? "TRUE" : "FALSE");

    /*  Options menu was pushed before postoptions(), so pop-up. */
    LYpop(newdoc);
    WWWDoc.address = newdoc->address;
    WWWDoc.post_data = newdoc->post_data;
    WWWDoc.post_content_type = newdoc->post_content_type;
    WWWDoc.bookmark = newdoc->bookmark;
    WWWDoc.isHEAD = newdoc->isHEAD;
    WWWDoc.safe = newdoc->safe;
    LYforce_no_cache = FALSE;   /* ! */
    LYoverride_no_cache = TRUE; /* ! */
    /*
     * Working out of getfile() cycle we reset *no_cache manually here so
     * HTLoadAbsolute() will return "Document already in memory":  it was
     * forced reloading Options Menu again without this (overhead).
     *
     * Probably *no_cache was set in a wrong position because of
     * the internal page...
     */
    if (!HTLoadAbsolute(&WWWDoc))
	return(NOT_FOUND);

    /* comment out to avoid warning when removing forms content... */
    /* HTuncache_current_document(); */ /* will never use again */

    /*
     *  Return to previous doc, not to options menu!
     *  Reload the document we had before the options menu
     *  but uncache only when necessary (Hurrah, user!):
     */
    LYpop(newdoc);
    WWWDoc.address = newdoc->address;
    WWWDoc.post_data = newdoc->post_data;
    WWWDoc.post_content_type = newdoc->post_content_type;
    WWWDoc.bookmark = newdoc->bookmark;
    WWWDoc.isHEAD = newdoc->isHEAD;
    WWWDoc.safe = newdoc->safe;
    LYforce_no_cache = FALSE;   /* see below */
    LYoverride_no_cache = TRUE; /* see below */
    /*
     * Re-setting of *no_cache is probably not required here but this is a
     * guarantee against _double_ reloading over the net in case prev document
     * has its own "no cache" attribute and options menu set "need_reload"
     * also.  Force this HTLoadAbsolute() to return "Document already in
     * memory".
     */
    if (!HTLoadAbsolute(&WWWDoc))
       return(NOT_FOUND);

    /*
     * Now most interesting part: reload document when necessary.
     * **********************************************************
     */

    reloading = FALSE;  /* set manually */
    /*  force end-to-end reload from remote server if change LYUserAgent
     *  or language or pref_charset (marked by need_end_reload flag above),
     *  from old-style LYK_OPTIONS (mainloop):
     */
    if ((need_end_reload == TRUE &&
	 (strncmp(newdoc->address, "http", 4) == 0 ||
	  strncmp(newdoc->address, "lynxcgi:", 8) == 0))) {
	/*
	 *  An option has changed which may influence
	 *  content negotiation, and the resource is from
	 *  a http or https or lynxcgi URL (the only protocols
	 *  which currently do anything with this information).
	 *  Set reloading = TRUE so that proxy caches will be
	 *  flushed, which is necessary until the time when
	 *  all proxies understand HTTP 1.1 Vary: and all
	 *  Servers properly use it...  Treat like
	 *  case LYK_RELOAD (see comments there). - KW
	 */
	reloading = TRUE;  /* global flag */
	need_reload = TRUE;  /* this was probably already TRUE, don't worry */
    }

    if (need_reload == FALSE) {
	/*  no uncache, already loaded */
	CTRACE(tfp, "LYOptions.c/postoptions(): now really exit.\n\n");
	return(NORMAL);
    } else {
	/*  update HText cache */

	/*
	 *  see LYK_RELOAD & LYK_OPTIONS in mainloop for details...
	 */
	if (HTisDocumentSource()) {
#ifndef USE_PSRC
	    HTOutputFormat = WWW_SOURCE;
#else
	    if (LYpsrc)
		psrc_view = TRUE;
	    else
		HTOutputFormat = WWW_SOURCE;
#endif
	}
#ifdef SOURCE_CACHE
	if (reloading == FALSE) {
	    /* one more attempt to be smart enough: */
	    if (HTreparse_document()) {
		CTRACE(tfp, "LYOptions.c/postoptions(): now really exit.\n\n");
		return(NORMAL);
	    }
	}
#endif
	HEAD_request = HTLoadedDocumentIsHEAD();
	/*  uncache and load again */
	HTuncache_current_document();
	LYpush(newdoc, FALSE);
	CTRACE(tfp, "LYOptions.c/postoptions(): now really exit.\n\n");
	return(NULLFILE);
    }

    /******** Done! **************************************************/
}

PRIVATE char *NewSecureValue NOARGS
{
    FREE(secure_value);
    if ((secure_value = malloc(80)) != 0) {
	sprintf(secure_value, "%ld", (long)secure_value + (long)time(0));
	return secure_value;
    }
    return "?";
}

/*
 * Okay, someone wants to change options.  So, lets gen up a form for them
 * and pass it around.  Gor, this is ugly.  Be a lot easier in Bourne with
 * "here" documents.  :->
 * Basic Strategy:  For each option, throw up the appropriate type of
 * control, giving defaults as appropriate.  If nothing else, we're
 * probably going to test every control there is.  MRC
 *
 * This function is synchronized with postoptions().  Read the comments in
 * postoptions() header if you change something in gen_options().
 */
PRIVATE int gen_options ARGS1(
	char **,	newfile)
{
    int i;
#if defined(USE_SLANG) || defined(COLOR_CURSES)
    BOOLEAN can_do_colors;
#endif
    static char tempfile[LY_MAXPATH];
    FILE *fp0;
    size_t cset_len = 0;
    size_t text_len = COLS - 38;	/* cf: PutLabel */

    LYRemoveTemp(tempfile);
    fp0 = LYOpenTemp(tempfile, HTML_SUFFIX, "w");
    if (fp0 == NULL) {
	HTAlert(UNABLE_TO_OPEN_TEMPFILE);
	return(NOT_FOUND);
    }

    LYLocalFileToURL(newfile, tempfile);

    LYforce_no_cache = TRUE;

    BeginInternalPage(fp0, OPTIONS_TITLE, NULL); /* help link below */

    /*
     * I do C, not HTML.  Feel free to pretty this up.
     */
    fprintf(fp0,"<form action=\"LYNXOPTIONS:\" method=\"post\">\n");
    /*
     * use following with some sort of one shot secret key akin to NCSA
     * (or was it CUTE?) telnet one shot password to allow ftp to self.
     * to prevent spoofing.
     */
    fprintf(fp0,"<input name=\"%s\" type=\"hidden\" value=\"%s\">\n",
	    secure_string, NewSecureValue());

    /*
     * visible text begins here
     */

    /* Submit/Reset/Help */
    fprintf(fp0,"<p align=center>\n");
    fprintf(fp0,"<input type=\"submit\" value=\"%s\"> - \n", ACCEPT_CHANGES);
    fprintf(fp0,"<input type=\"reset\" value=\"%s\">\n", RESET_CHANGES);
    fprintf(fp0,"%s\n", CANCEL_CHANGES);
    fprintf(fp0, "<a href=\"%s%s\">%s</a>\n",
		 helpfilepath, OPTIONS_HELP, TO_HELP);

    /* Save options */
    if (!no_option_save) {
	fprintf(fp0, "<p align=center>%s: ", SAVE_OPTIONS);
	fprintf(fp0, "<input type=\"checkbox\" name=\"%s\">\n",
		save_options_string);
    }

    /*
     * preformatted text follows
     */
    fprintf(fp0,"<pre>\n");
    fprintf(fp0,"\n  <em>%s</em>\n", gettext("Personal Preferences"));

    /* Cookies: SELECT */
    PutLabel(fp0, gettext("Cookies"));
    BeginSelect(fp0, cookies_string);
    PutOption(fp0, !LYSetCookies,
	      cookies_ignore_all_string,
	      cookies_ignore_all_string);
    PutOption(fp0, LYSetCookies && !LYAcceptAllCookies,
	      cookies_up_to_user_string,
	      cookies_up_to_user_string);
    PutOption(fp0, LYSetCookies && LYAcceptAllCookies,
	      cookies_accept_all_string,
	      cookies_accept_all_string);
    EndSelect(fp0);

    /* Editor: INPUT */
    PutLabel(fp0, gettext("Editor"));
    PutTextInput(fp0, editor_string, NOTEMPTY(editor), text_len,
		DISABLED(no_editor || system_editor));

    /* Emacs keys: ON/OFF */
    PutLabel(fp0, gettext("Emacs keys"));
    BeginSelect(fp0, emacs_keys_string);
    PutOptValues(fp0, emacs_keys, bool_values);
    EndSelect(fp0);

    /* Keypad Mode: SELECT */
    PutLabel(fp0, gettext("Keypad mode"));
    BeginSelect(fp0, keypad_mode_string);
    PutOptValues(fp0, keypad_mode, keypad_mode_values);
    EndSelect(fp0);

    /* Line edit style: SELECT */
    if (LYLineeditNames[1]) { /* well, at least 2 line edit styles available */
	PutLabel(fp0, "Line edit style");
	BeginSelect(fp0, lineedit_style_string);
	for (i = 0; LYLineeditNames[i]; i++) {
	    char temp[16];
	    sprintf(temp, "%d", i);
	    PutOption(fp0, i==current_lineedit, temp, LYLineeditNames[i]);
	}
	EndSelect(fp0);
    }

#ifdef EXP_KEYBOARD_LAYOUT
    /* Keyboard layout: SELECT */
    PutLabel(fp0, "Keyboard layout");
    BeginSelect(fp0, kblayout_string);
    for (i = 0; LYKbLayoutNames[i]; i++) {
	char temp[16];
	sprintf(temp, "%d", i);
	PutOption(fp0, i==current_layout, temp, LYKbLayoutNames[i]);
    }
    EndSelect(fp0);
#endif /* EXP_KEYBOARD_LAYOUT */

    /* Mail Address: INPUT */
    PutLabel(fp0, gettext("Personal mail address"));
    PutTextInput(fp0, mail_address_string,
		NOTEMPTY(personal_mail_address), text_len, "");

    /* Search Type: SELECT */
    PutLabel(fp0, gettext("Searching type"));
    BeginSelect(fp0, search_type_string);
    PutOptValues(fp0, case_sensitive, search_type_values);
    EndSelect(fp0);

    /* Show Color: SELECT */
#if defined(USE_SLANG) || defined(COLOR_CURSES)
    can_do_colors = 1;
#if defined(COLOR_CURSES)
    can_do_colors = has_colors();
#endif
    PutLabel(fp0, gettext("Show color"));
    MaybeSelect(fp0, !can_do_colors, show_color_string);
    if (no_option_save) {
	if (LYShowColor == SHOW_COLOR_NEVER) {
	    LYShowColor = SHOW_COLOR_OFF;
	} else if (LYShowColor == SHOW_COLOR_ALWAYS) {
	    LYShowColor = SHOW_COLOR_ON;
	}
	PutOptValues(fp0, LYShowColor, bool_values);
    } else {
	if (LYChosenShowColor == SHOW_COLOR_UNKNOWN) {
	    switch (LYrcShowColor) {
	    case SHOW_COLOR_NEVER:
		LYChosenShowColor =
		    (LYShowColor >= SHOW_COLOR_ON) ?
			SHOW_COLOR_ON : SHOW_COLOR_NEVER;
		break;
	    case SHOW_COLOR_ALWAYS:
		if (!can_do_colors)
		    LYChosenShowColor = SHOW_COLOR_ALWAYS;
		else
		    LYChosenShowColor =
			(LYShowColor >= SHOW_COLOR_ON) ?
				SHOW_COLOR_ALWAYS : SHOW_COLOR_OFF;
		break;
	    default:
		LYChosenShowColor =
		    (LYShowColor >= SHOW_COLOR_ON) ?
			SHOW_COLOR_ON : SHOW_COLOR_OFF;
	    }
	}
	show_color_values[3].LongName = (can_do_colors) ? always_string
							: "Always try";
	PutOptValues(fp0, LYChosenShowColor, show_color_values);
    }
    EndSelect(fp0);
#endif /* USE_SLANG || COLOR_CURSES */

    /* Show cursor: ON/OFF */
    PutLabel(fp0, gettext("Show cursor"));
    BeginSelect(fp0, show_cursor_string);
    PutOptValues(fp0, LYShowCursor, bool_values);
    EndSelect(fp0);

    /* User Mode: SELECT */
    PutLabel(fp0, gettext("User mode"));
    BeginSelect(fp0, user_mode_string);
    PutOptValues(fp0, user_mode, user_mode_values);
    EndSelect(fp0);

    /* VI Keys: ON/OFF */
    PutLabel(fp0, gettext("VI keys"));
    BeginSelect(fp0, vi_keys_string);
    PutOptValues(fp0, vi_keys, bool_values);
    EndSelect(fp0);

    /* Display Character Set: SELECT */
    PutLabel(fp0, gettext("Display character set"));
    BeginSelect(fp0, display_char_set_string);
    for (i = 0; LYchar_set_names[i]; i++) {
	char temp[10];
	size_t len = strlen(LYchar_set_names[i]);
	if (len > cset_len)
	   cset_len = len;
	sprintf(temp, "%d", i);
	PutOption(fp0, i==current_char_set, temp, LYchar_set_names[i]);
    }
    EndSelect(fp0);

    /* X Display: INPUT */
    PutLabel(fp0, gettext("X Display"));
    PutTextInput(fp0, x_display_string, NOTEMPTY(x_display), text_len, "");

    /*
     * Document Layout
     */
    fprintf(fp0,"\n  <em>%s</em>\n", gettext("Document Layout"));

    /* Assume Character Set: SELECT */
    /* if (user_mode==ADVANCED_MODE) */
    {
	int curval;
	curval = UCLYhndl_for_unspec;

	/*
	 * FIXME: If bogus value in lynx.cfg, then in old way, that is the
	 * string that was displayed.  Now, user will never see that.  Good
	 * or bad?  I don't know.  MRC
	 */
	if (curval == current_char_set) {
	    /* ok, LYRawMode, so use UCAssume_MIMEcharset */
	    curval = safeUCGetLYhndl_byMIME(UCAssume_MIMEcharset);
	}
	PutLabel(fp0, gettext("Assumed document character set"));
	BeginSelect(fp0, assume_char_set_string);
	for (i = 0; i < LYNumCharsets; i++) {
	    PutOption(fp0, i==curval,
		      LYCharSet_UC[i].MIMEname,
		      LYCharSet_UC[i].MIMEname);
	}
	EndSelect(fp0);
    }

    /* Raw Mode: ON/OFF */
    if (LYHaveCJKCharacterSet)
	/*
	 * Since CJK people hardly mixed with other world
	 * we split the header to make it more readable:
	 * "CJK mode" for CJK display charsets, and "Raw 8-bit" for others.
	 */
	PutLabel(fp0, gettext("CJK mode"));
    else
	PutLabel(fp0, gettext("Raw 8-bit"));

    BeginSelect(fp0, raw_mode_string);
    PutOptValues(fp0, LYRawMode, bool_values);
    EndSelect(fp0);

    /* HTML error recovery: SELECT */
    PutLabel(fp0, gettext("HTML error recovery"));
    BeginSelect(fp0, DTD_recovery_string);
    PutOptValues(fp0, Old_DTD, DTD_type_values);
    EndSelect(fp0);

    /* Select Popups: ON/OFF */
    PutLabel(fp0, gettext("Popups for select fields"));
    BeginSelect(fp0, select_popups_string);
    PutOptValues(fp0, LYSelectPopups, bool_values);
    EndSelect(fp0);

    /* Show Images: SELECT */
    PutLabel(fp0, gettext("Show images"));
    BeginSelect(fp0, images_string);
    PutOption(fp0, !pseudo_inline_alts && !clickable_images,
       images_ignore_all_string,
       images_ignore_all_string);
    PutOption(fp0, pseudo_inline_alts && !clickable_images,
       images_use_label_string,
       images_use_label_string);
    PutOption(fp0, clickable_images,
       images_use_links_string,
       images_use_links_string);
    EndSelect(fp0);

    /* Verbose Images: ON/OFF */
    PutLabel(fp0, gettext("Verbose images"));
    BeginSelect(fp0, verbose_images_string);
    PutOptValues(fp0, verbose_img, verbose_images_type_values);
    EndSelect(fp0);

    /*
     * Bookmark Options
     */
    fprintf(fp0,"\n  <em>%s</em>\n", gettext("Bookmark Options"));

    /* Multi-Bookmark Mode: SELECT */
    if (!LYMBMBlocked) {
       PutLabel(fp0, gettext("Multi-bookmarks"));
       BeginSelect(fp0, mbm_string);
       PutOption(fp0, !LYMultiBookmarks,
	   mbm_off_string,
	   mbm_off_string);
       PutOption(fp0, LYMultiBookmarks && !LYMBMAdvanced,
	   mbm_standard_string,
	   mbm_standard_string);
       PutOption(fp0, LYMultiBookmarks && LYMBMAdvanced,
	   mbm_advanced_string,
	   mbm_advanced_string);
       EndSelect(fp0);
    }

    /* Bookmarks File Menu: LINK/INPUT */
    if (LYMultiBookmarks) {

	PutLabel(fp0, gettext("Review/edit Bookmarks files"));
	fprintf(fp0, "<a href=\"LYNXOPTIONS://MBM_MENU\">%s</a>\n",
		gettext("Goto multi-bookmark menu"));

    } else {
	PutLabel(fp0, gettext("Bookmarks file"));
	PutTextInput(fp0, single_bookmark_string,
		NOTEMPTY(bookmark_page), text_len, "");
    }

    /*
     * File Management Options
     */
    fprintf(fp0,"\n  <em>%s</em>\n", DIRED_MENU_TITLE);

    /* FTP sort: SELECT */
    PutLabel(fp0, gettext("FTP sort criteria"));
    BeginSelect(fp0, ftp_sort_string);
    PutOptValues(fp0, HTfileSortMethod, ftp_sort_values);
    EndSelect(fp0);

#ifdef DIRED_SUPPORT
    /* Local Directory Sort: SELECT */
    PutLabel(fp0, gettext("Local directory sort criteria"));
    BeginSelect(fp0, dired_sort_string);
    PutOptValues(fp0, dir_list_style, dired_values);
    EndSelect(fp0);
#endif /* DIRED_SUPPORT */

    /* Show dot files: ON/OFF */
    if (!no_dotfiles) {
	PutLabel(fp0, gettext("Show dot files"));
	BeginSelect(fp0, show_dotfiles_string);
	PutOptValues(fp0, show_dotfiles, bool_values);
	EndSelect(fp0);
    }

    /* Execution links: SELECT */
#ifdef ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS
    PutLabel(fp0, gettext("Execution links"));
    BeginSelect(fp0, exec_links_string);
#ifndef NEVER_ALLOW_REMOTE_EXEC
    PutOptValues(fp0, local_exec
		      ? EXEC_ALWAYS
		      : (local_exec_on_local_files
			  ? EXEC_LOCAL
			  : EXEC_NEVER),
	       exec_links_values);
#else
    PutOptValues(fp0, local_exec_on_local_files
		      ? EXEC_LOCAL
		      : EXEC_NEVER,
	       exec_links_values);
#endif /* !NEVER_ALLOW_REMOTE_EXEC */
    EndSelect(fp0);
#endif /* ALLOW_USERS_TO_CHANGE_EXEC_WITHIN_OPTIONS */

    /*
     * Headers transferred to remote server
     */
    fprintf(fp0,"\n  <em>%s</em>\n", gettext("Headers transferred to remote server"));

    /* Preferred Document Character Set: INPUT */
    PutLabel(fp0, gettext("Preferred document character set"));
    PutTextInput(fp0, preferred_doc_char_string,
	    NOTEMPTY(pref_charset), cset_len+2, "");

    /* Preferred Document Language: INPUT */
    PutLabel(fp0, gettext("Preferred document language"));
    PutTextInput(fp0, preferred_doc_lang_string,
	    NOTEMPTY(language), cset_len+2, "");

    /* User Agent: INPUT */
    if (!no_useragent) {
	PutLabel(fp0, gettext("User-Agent header"));
	PutTextInput(fp0, user_agent_string,
		     NOTEMPTY(LYUserAgent), text_len, "");
    }

    if (!LYRestricted) {
	fprintf(fp0,
		"\n  Check your <a href=\"LYNXCFG:\">lynx.cfg</a> here\n");
    }

    fprintf(fp0,"\n</pre>\n");

    /* Submit/Reset */
    fprintf(fp0,"<p align=center>\n");
    fprintf(fp0,"<input type=\"submit\" value=\"%s\">\n - ", ACCEPT_CHANGES);
    fprintf(fp0,"<input type=\"reset\" value=\"%s\">\n", RESET_CHANGES);
    fprintf(fp0,"%s\n", CANCEL_CHANGES);

    /*
     * close HTML
     */
    fprintf(fp0,"</form>\n");
    EndInternalPage(fp0);

    LYCloseTempFP(fp0);
    return(NORMAL);
}
#endif /* !NO_OPTION_FORMS */
