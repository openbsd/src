#include <HTUtils.h>
#include <HTFTP.h>
#include <HTTP.h>		/* 'reloading' flag */
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
#include <HTFile.h>

#include <LYLeaks.h>

BOOLEAN term_options;

#define TOP_LINK  "/"
#define MBM_LINK  "//MBM_MENU"

#define MARGIN_STR (no_margins ? "" : "&nbsp;&nbsp;")
#define MARGIN_LEN (no_margins ?  0 : 2)

static void terminate_options(int sig);

#define COL_OPTION_VALUES 36	/* display column where option values start */

#if defined(USE_SLANG) || defined(COLOR_CURSES)
static BOOLEAN can_do_colors = FALSE;
#endif

static int LYChosenShowColor = SHOW_COLOR_UNKNOWN;	/* whether to show and save */

BOOLEAN LYCheckUserAgent(void)
{
    if (non_empty(LYUserAgent)) {
	if (strstr(LYUserAgent, "Lynx") == 0
	    && strstr(LYUserAgent, "lynx") == 0
	    && strstr(LYUserAgent, "L_y_n_x") == 0
	    && strstr(LYUserAgent, "l_y_n_x") == 0) {
	    return FALSE;
	}
    }
    return TRUE;
}

static void validate_x_display(void)
{
    char *cp;

    if ((cp = LYgetXDisplay()) != NULL) {
	StrAllocCopy(x_display, cp);
    } else {
	FREE(x_display);
    }
}

static void summarize_x_display(char *display_option)
{
    if ((x_display == NULL && *display_option == '\0') ||
	(x_display != NULL && !strcmp(x_display, display_option))) {
	if (x_display == NULL && LYisConfiguredForX == TRUE) {
	    _statusline(VALUE_ACCEPTED_WARNING_X);
	} else if (x_display != NULL && LYisConfiguredForX == FALSE) {
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
}

static void SetupChosenShowColor(void)
{
#if defined(USE_SLANG) || defined(COLOR_CURSES)
    can_do_colors = TRUE;
#if defined(COLOR_CURSES)
    if (LYCursesON)		/* could crash if called before initialization */
	can_do_colors = (has_colors()
			 ? TRUE
			 : FALSE);
#endif
    if (!no_option_save) {
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
    }
#endif /* USE_SLANG || COLOR_CURSES */
}

#ifndef NO_OPTION_MENU
static int boolean_choice(int status,
			  int line,
			  int column,
			  const char **choices);

#define LYChooseBoolean(status, line, column, choices) \
	boolean_choice(status, line, column, (const char **)choices)

#define MAXCHOICES 10

/*
 * Values for the options menu.  - FM
 *
 * L_foo values are the Y coordinates for the menu item.
 * B_foo values are the X coordinates for the item's prompt string.
 * C_foo values are the X coordinates for the item's value string.
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
#define L_Bool_A     (use_assume_charset ? L_BOOL_A     + 1 : L_BOOL_A)
#define L_Bool_B     (use_assume_charset ? L_BOOL_B     + 1 : L_BOOL_B)
#define L_Exec       (use_assume_charset ? L_EXEC       + 1 : L_EXEC)
#define L_Rawmode    (use_assume_charset ? L_RAWMODE    + 1 : L_RAWMODE)
#define L_Charset    (use_assume_charset ? L_CHARSET    + 1 : L_CHARSET)
#define L_Color      (use_assume_charset ? L_COLOR      + 1 : L_COLOR)
#define L_Keypad     (use_assume_charset ? L_KEYPAD     + 1 : L_KEYPAD)
#define L_Lineed     (use_assume_charset ? L_LINEED     + 1 : L_LINEED)
#define L_Layout     (use_assume_charset ? L_LAYOUT     + 1 : L_LAYOUT)
#define L_Dired      (use_assume_charset ? L_DIRED      + 1 : L_DIRED)
#define L_User_Mode  (use_assume_charset ? L_USER_MODE  + 1 : L_USER_MODE)
#define L_User_Agent (use_assume_charset ? L_USER_AGENT + 1 : L_USER_AGENT)

#define LPAREN '('
#define RPAREN ')'

static int add_it(char *text, int len)
{
    if (len) {
	text[len] = '\0';
	LYaddstr(text);
    }
    return 0;
}

/*
 * addlbl() is used instead of plain LYaddstr() in old-style options menu
 * to show hot keys in bold.
 */
static void addlbl(const char *text)
{
    char actual[80];
    int s, d;
    BOOL b = FALSE;

    for (s = d = 0; text[s]; s++) {
	actual[d++] = text[s];
	if (text[s] == LPAREN) {
	    d = add_it(actual, d - 1);
	    lynx_start_bold();
	    b = TRUE;
	    actual[d++] = text[s];
	} else if (text[s] == RPAREN) {
	    d = add_it(actual, d);
	    lynx_stop_bold();
	    b = FALSE;
	}
    }
    add_it(actual, d);
    if (b)
	lynx_stop_bold();
}

#if !defined(VMS) || defined(USE_SLANG)
#define HANDLE_LYOPTIONS \
		    if (term_options) { \
			term_options = FALSE; \
		    } else { \
			AddValueAccepted = TRUE; \
		    } \
		    goto draw_options
#else
#define HANDLE_LYOPTIONS \
		    term_options = FALSE; \
		    if (use_assume_charset != old_use_assume_charset) \
			goto draw_options
#endif /* !VMS || USE_SLANG */

void LYoptions(void)
{
#define ShowBool(value) LYaddstr((value) ? "ON " : "OFF")
    static const char *bool_choices[] =
    {
	"OFF",
	"ON",
	NULL
    };
    static const char *caseless_choices[] =
    {
	"CASE INSENSITIVE",
	"CASE SENSITIVE",
	NULL
    };
    static const char *dirList_choices[] =
    {
	"Directories first",
	"Files first",
	"Mixed style",
	NULL
    };

#if defined(ENABLE_OPTS_CHANGE_EXEC) && (defined(EXEC_LINKS) || defined(EXEC_SCRIPTS))
    static const char *exec_choices[] =
    {
	"ALWAYS OFF",
	"FOR LOCAL FILES ONLY",
#ifndef NEVER_ALLOW_REMOTE_EXEC
	"ALWAYS ON",
#endif				/* !NEVER_ALLOW_REMOTE_EXEC */
	NULL
    };
#endif
    static const char *fileSort_choices[] =
    {
	"By Filename",
	"By Type",
	"By Size",
	"By Date",
	NULL
    };
    static const char *keypad_choices[] =
    {
	"Numbers act as arrows",
	"Links are numbered",
	"Links and form fields are numbered",
	NULL
    };
    static const char *mbm_choices[] =
    {
	"OFF     ",
	"STANDARD",
	"ADVANCED",
	NULL
    };
    static const char *userMode_choices[] =
    {
	"Novice",
	"Intermediate",
	"Advanced",
	NULL
    };

#if defined(ENABLE_OPTS_CHANGE_EXEC) && (defined(EXEC_LINKS) || defined(EXEC_SCRIPTS))
    int itmp;
#endif /* ENABLE_OPTS_CHANGE_EXEC */
    int response, ch;

    /*
     * If the user changes the display we need memory to put it in.
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
#ifdef ENABLE_OPTS_CHANGE_EXEC
    if (LYlines < 24) {
	HTAlert(OPTION_SCREEN_NEEDS_24);
	return;
    }
#else
    if (LYlines < 23) {
	HTAlert(OPTION_SCREEN_NEEDS_23);
	return;
    }
#endif /* ENABLE_OPTS_CHANGE_EXEC */
#else
#ifdef ENABLE_OPTS_CHANGE_EXEC
    if (LYlines < 23) {
	HTAlert(OPTION_SCREEN_NEEDS_23);
	return;
    }
#else
    if (LYlines < 22) {
	HTAlert(OPTION_SCREEN_NEEDS_22);
	return;
    }
#endif /* ENABLE_OPTS_CHANGE_EXEC */
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
	SetupChosenShowColor();
#endif /* USE_SLANG || COLOR_CURSES */
    }

    old_use_assume_charset =
	use_assume_charset = (BOOL) (user_mode == ADVANCED_MODE);

  draw_options:

    old_use_assume_charset = use_assume_charset;
    /*
     * NOTE that printw() should be avoided for strings that might have
     * non-ASCII or multibyte/CJK characters.  - FM
     */
#if defined(FANCY_CURSES) || defined (USE_SLANG)
    if (enable_scrollback) {
	LYclear();
    } else {
	LYerase();
    }
#else
    LYclear();
#endif /* FANCY_CURSES || USE_SLANG */
    LYmove(0, 5);

    lynx_start_h1_color();
    LYaddstr("         Options Menu (");
    LYaddstr(LYNX_NAME);
    LYaddstr(" Version ");
    LYaddstr(LYNX_VERSION);
    LYaddch(')');
    lynx_stop_h1_color();
    LYmove(L_EDITOR, 5);
    addlbl("(E)ditor                     : ");
    LYaddstr(non_empty(editor) ? editor : "NONE");

    LYmove(L_DISPLAY, 5);
    addlbl("(D)ISPLAY variable           : ");
    LYaddstr(non_empty(x_display) ? x_display : "NONE");

    LYmove(L_HOME, 5);
    addlbl("mu(L)ti-bookmarks: ");
    LYaddstr(mbm_choices[LYMultiBookmarks]);
    LYmove(L_HOME, B_BOOK);
    if (LYMultiBookmarks != MBM_OFF) {
	addlbl("review/edit (B)ookmarks files");
    } else {
	addlbl("(B)ookmark file: ");
	LYaddstr(non_empty(bookmark_page) ? bookmark_page : "NONE");
    }

    LYmove(L_FTPSTYPE, 5);
    addlbl("(F)TP sort criteria          : ");
    LYaddstr((HTfileSortMethod == FILE_BY_NAME ? "By Filename" :
	      (HTfileSortMethod == FILE_BY_SIZE ? "By Size    " :
	       (HTfileSortMethod == FILE_BY_TYPE ? "By Type    " :
		"By Date    "))));

    LYmove(L_MAIL_ADDRESS, 5);
    addlbl("(P)ersonal mail address      : ");
    LYaddstr(non_empty(personal_mail_address) ?
	     personal_mail_address : "NONE");

    LYmove(L_SSEARCH, 5);
    addlbl("(S)earching type             : ");
    LYaddstr(case_sensitive ? "CASE SENSITIVE  " : "CASE INSENSITIVE");

    LYmove(L_Charset, 5);
    addlbl("display (C)haracter set      : ");
    LYaddstr(LYchar_set_names[current_char_set]);

    LYmove(L_LANGUAGE, 5);
    addlbl("preferred document lan(G)uage: ");
    LYaddstr(non_empty(language) ? language : "NONE");

    LYmove(L_PREF_CHARSET, 5);
    addlbl("preferred document c(H)arset : ");
    LYaddstr(non_empty(pref_charset) ? pref_charset : "NONE");

    if (use_assume_charset) {
	LYmove(L_ASSUME_CHARSET, 5);
	addlbl("(^A)ssume charset if unknown : ");
	if (UCAssume_MIMEcharset)
	    LYaddstr(UCAssume_MIMEcharset);
	else
	    LYaddstr((UCLYhndl_for_unspec >= 0) ?
		     LYCharSet_UC[UCLYhndl_for_unspec].MIMEname
		     : "NONE");
    }

    LYmove(L_Rawmode, 5);
    addlbl("Raw 8-bit or CJK m(O)de      : ");
    ShowBool(LYRawMode);

#if defined(USE_SLANG) || defined(COLOR_CURSES)
    LYmove(L_Color, B_COLOR);
    addlbl("show color (&)  : ");
    if (no_option_save) {
	ShowBool(LYShowColor == SHOW_COLOR_OFF);
    } else {
	switch (LYChosenShowColor) {
	case SHOW_COLOR_NEVER:
	    LYaddstr("NEVER     ");
	    break;
	case SHOW_COLOR_OFF:
	    LYaddstr("OFF");
	    break;
	case SHOW_COLOR_ON:
	    LYaddstr("ON ");
	    break;
	case SHOW_COLOR_ALWAYS:
#if defined(COLOR_CURSES)
	    if (!has_colors())
		LYaddstr("Always try");
	    else
#endif
		LYaddstr("ALWAYS    ");
	}
    }
#endif /* USE_SLANG || COLOR_CURSES */

    LYmove(L_Bool_A, B_VIKEYS);
    addlbl("(V)I keys: ");
    ShowBool(vi_keys);

    LYmove(L_Bool_A, B_EMACSKEYS);
    addlbl("e(M)acs keys: ");
    ShowBool(emacs_keys);

    LYmove(L_Bool_A, B_SHOW_DOTFILES);
    addlbl("sho(W) dot files: ");
    ShowBool(!no_dotfiles && show_dotfiles);

    LYmove(L_Bool_B, B_SELECT_POPUPS);
    addlbl("popups for selec(T) fields   : ");
    ShowBool(LYSelectPopups);

    LYmove(L_Bool_B, B_SHOW_CURSOR);
    addlbl("show cursor (@) : ");
    ShowBool(LYShowCursor);

    LYmove(L_Keypad, 5);
    addlbl("(K)eypad mode                : ");
    LYaddstr(fields_are_numbered() && links_are_numbered()
	     ? "Links and form fields are numbered"
	     : links_are_numbered()
	     ? "Links are numbered                "
	     : fields_are_numbered()
	     ? "Form fields are numbered          "
	     : "Numbers act as arrows             ");

    LYmove(L_Lineed, 5);
    addlbl("li(N)e edit style            : ");
    LYaddstr(LYLineeditNames[current_lineedit]);

#ifdef EXP_KEYBOARD_LAYOUT
    LYmove(L_Layout, 5);
    addlbl("Ke(Y)board layout            : ");
    LYaddstr(LYKbLayoutNames[current_layout]);
#endif

#ifdef DIRED_SUPPORT
    LYmove(L_Dired, 5);
    addlbl("l(I)st directory style       : ");
    LYaddstr((dir_list_style == FILES_FIRST) ? "Files first      " :
	     ((dir_list_style == MIXED_STYLE) ? "Mixed style      " :
	      "Directories first"));
#endif /* DIRED_SUPPORT */

    LYmove(L_User_Mode, 5);
    addlbl("(U)ser mode                  : ");
    LYaddstr((user_mode == NOVICE_MODE) ? "Novice      " :
	     ((user_mode == INTERMEDIATE_MODE) ? "Intermediate" :
	      "Advanced    "));

    addlbl("  verbose images (!) : ");
    ShowBool(verbose_img);

    LYmove(L_User_Agent, 5);
    addlbl("user (A)gent                 : ");
    LYaddstr(non_empty(LYUserAgent) ? LYUserAgent : "NONE");

#if defined(ENABLE_OPTS_CHANGE_EXEC) && (defined(EXEC_LINKS) || defined(EXEC_SCRIPTS))
    LYmove(L_Exec, 5);
    addlbl("local e(X)ecution links      : ");
#ifndef NEVER_ALLOW_REMOTE_EXEC
    LYaddstr(local_exec ? "ALWAYS ON           " :
	     (local_exec_on_local_files ? "FOR LOCAL FILES ONLY" :
	      "ALWAYS OFF          "));
#else
    LYaddstr(local_exec_on_local_files ? "FOR LOCAL FILES ONLY" :
	     "ALWAYS OFF          ");
#endif /* !NEVER_ALLOW_REMOTE_EXEC */
#endif /* ENABLE_OPTS_CHANGE_EXEC */

    LYmove(LYlines - 3, 2);
    LYaddstr(SELECT_SEGMENT);
    lynx_start_bold();
    LYaddstr(CAP_LETT_SEGMENT);
    lynx_stop_bold();
    LYaddstr(OF_OPT_LINE_SEGMENT);
    if (!no_option_save) {
	LYaddstr(" '");
	lynx_start_bold();
	LYaddstr(">");
	lynx_stop_bold();
	LYaddstr("'");
	LYaddstr(TO_SAVE_SEGMENT);
    }
    LYaddstr(OR_SEGMENT);
    LYaddstr("'");
    lynx_start_bold();
    LYaddstr("r");
    lynx_stop_bold();
    LYaddstr("'");
    LYaddstr(TO_RETURN_SEGMENT);

    response = 0;
    while (response != 'R' &&
	   !LYisNonAlnumKeyname(response, LYK_PREV_DOC) &&
	   response != '>' && !term_options &&
	   !LYCharIsINTERRUPT_NO_letter(response)) {
	if (AddValueAccepted == TRUE) {
	    _statusline(VALUE_ACCEPTED);
	    AddValueAccepted = FALSE;
	}
	LYmove((LYlines - 2), 0);
	lynx_start_prompt_color();
	LYaddstr(COMMAND_PROMPT);
	lynx_stop_prompt_color();

	LYrefresh();
	response = LYgetch_single();
	if (term_options || LYCharIsINTERRUPT_NO_letter(response))
	    response = 'R';
	if (LYisNonAlnumKeyname(response, LYK_REFRESH)) {
	    lynx_force_repaint();
	    goto draw_options;
	}
	switch (response) {
	case 'E':		/* Change the editor. */
	    if (no_editor) {
		_statusline(EDIT_DISABLED);
	    } else if (system_editor) {
		_statusline(EDITOR_LOCKED);
	    } else {
		if (non_empty(editor))
		    LYstrncpy(display_option, editor, sizeof(display_option) - 1);
		else {		/* clear the NONE */
		    LYmove(L_EDITOR, COL_OPTION_VALUES);
		    LYaddstr("    ");
		    *display_option = '\0';
		}
		_statusline(ACCEPT_DATA);
		LYmove(L_EDITOR, COL_OPTION_VALUES);
		lynx_start_bold();
		ch = LYgetstr(display_option, VISIBLE,
			      sizeof(display_option), NORECALL);
		lynx_stop_bold();
		LYmove(L_EDITOR, COL_OPTION_VALUES);
		if (term_options || ch == -1) {
		    LYaddstr(non_empty(editor) ?
			     editor : "NONE");
		} else if (*display_option == '\0') {
		    FREE(editor);
		    LYaddstr("NONE");
		} else {
		    StrAllocCopy(editor, display_option);
		    LYaddstr(display_option);
		}
		LYclrtoeol();
		if (ch == -1) {
		    HTInfoMsg(CANCELLED);
		    HTInfoMsg("");
		} else {
		    _statusline(VALUE_ACCEPTED);
		}
	    }
	    response = ' ';
	    break;

	case 'D':		/* Change the display. */
	    if (non_empty(x_display)) {
		LYstrncpy(display_option, x_display, sizeof(display_option) - 1);
	    } else {		/* clear the NONE */
		LYmove(L_DISPLAY, COL_OPTION_VALUES);
		LYaddstr("    ");
		*display_option = '\0';
	    }
	    _statusline(ACCEPT_DATA);
	    LYmove(L_DISPLAY, COL_OPTION_VALUES);
	    lynx_start_bold();
	    ch = LYgetstr(display_option, VISIBLE,
			  sizeof(display_option), NORECALL);
	    lynx_stop_bold();
	    LYmove(L_DISPLAY, COL_OPTION_VALUES);

#ifdef VMS
#define CompareEnvVars(a,b) strcasecomp(a, b)
#else
#define CompareEnvVars(a,b) strcmp(a, b)
#endif /* VMS */

	    if ((term_options || ch == -1) ||
		(x_display != NULL &&
		 !CompareEnvVars(x_display, display_option))) {
		/*
		 * Cancelled, or a non-NULL display string wasn't changed.  -
		 * FM
		 */
		LYaddstr(non_empty(x_display) ? x_display : "NONE");
		LYclrtoeol();
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
		     * NULL or zero-length display string wasn't changed.  - FM
		     */
		    LYaddstr("NONE");
		    LYclrtoeol();
		    _statusline(VALUE_ACCEPTED);
		    response = ' ';
		    break;
		}
	    }
	    /*
	     * Set the new DISPLAY variable.  - FM
	     */
	    LYsetXDisplay(display_option);
	    validate_x_display();
	    cp = NULL;
	    LYaddstr(x_display ? x_display : "NONE");
	    LYclrtoeol();
	    summarize_x_display(display_option);
	    response = ' ';
	    break;

	case 'L':		/* Change multibookmarks option. */
	    if (LYMBMBlocked) {
		_statusline(MULTIBOOKMARKS_DISALLOWED);
		response = ' ';
		break;
	    }
	    if (!LYSelectPopups) {
		LYMultiBookmarks = LYChooseBoolean(LYMultiBookmarks,
						   L_HOME, C_MULTI,
						   mbm_choices);
	    } else {
		LYMultiBookmarks = LYChoosePopup(LYMultiBookmarks,
						 L_HOME, (C_MULTI - 1),
						 mbm_choices,
						 3, FALSE, FALSE);
	    }
#if defined(VMS) || defined(USE_SLANG)
	    if (LYSelectPopups) {
		LYmove(L_HOME, C_MULTI);
		LYclrtoeol();
		LYaddstr(mbm_choices[LYMultiBookmarks]);
	    }
#endif /* VMS || USE_SLANG */
#if !defined(VMS) && !defined(USE_SLANG)
	    if (!LYSelectPopups)
#endif /* !VMS && !USE_SLANG */
	    {
		LYmove(L_HOME, B_BOOK);
		LYclrtoeol();
		if (LYMultiBookmarks != MBM_OFF) {
		    LYaddstr(gettext("review/edit B)ookmarks files"));
		} else {
		    LYaddstr(gettext("B)ookmark file: "));
		    LYaddstr(non_empty(bookmark_page) ?
			     bookmark_page : "NONE");
		}
	    }
	    response = ' ';
	    if (LYSelectPopups) {
		HANDLE_LYOPTIONS;
	    }
	    break;

	case 'B':		/* Change the bookmark page location. */
	    /*
	     * Anonymous users should not be allowed to change the bookmark
	     * page.
	     */
	    if (!no_bookmark) {
		if (LYMultiBookmarks != MBM_OFF) {
		    edit_bookmarks();
		    signal(SIGINT, terminate_options);
		    goto draw_options;
		}
		if (non_empty(bookmark_page)) {
		    LYstrncpy(display_option,
			      bookmark_page,
			      sizeof(display_option) - 1);
		} else {	/* clear the NONE */
		    LYmove(L_HOME, C_DEFAULT);
		    LYclrtoeol();
		    *display_option = '\0';
		}
		_statusline(ACCEPT_DATA);
		LYmove(L_HOME, C_DEFAULT);
		lynx_start_bold();
		ch = LYgetstr(display_option, VISIBLE,
			      sizeof(display_option), NORECALL);
		lynx_stop_bold();
		LYmove(L_HOME, C_DEFAULT);
		if (term_options ||
		    ch == -1 || *display_option == '\0') {
		    LYaddstr(non_empty(bookmark_page) ?
			     bookmark_page : "NONE");
		} else if (!LYPathOffHomeOK(display_option,
					    sizeof(display_option))) {
		    LYaddstr(non_empty(bookmark_page) ?
			     bookmark_page : "NONE");
		    LYclrtoeol();
		    _statusline(USE_PATH_OFF_HOME);
		    response = ' ';
		    break;
		} else {
		    StrAllocCopy(bookmark_page, display_option);
		    StrAllocCopy(MBM_A_subbookmark[0], bookmark_page);
		    LYaddstr(bookmark_page);
		}
		LYclrtoeol();
		if (ch == -1) {
		    HTInfoMsg(CANCELLED);
		    HTInfoMsg("");
		} else {
		    _statusline(VALUE_ACCEPTED);
		}
	    } else {		/* anonymous */
		_statusline(BOOKMARK_CHANGE_DISALLOWED);
	    }
	    response = ' ';
	    break;

	case 'F':		/* Change ftp directory sorting. */
	    if (!LYSelectPopups) {
		HTfileSortMethod = LYChooseBoolean(HTfileSortMethod,
						   L_FTPSTYPE, -1,
						   fileSort_choices);
	    } else {
		HTfileSortMethod = LYChoosePopup(HTfileSortMethod,
						 L_FTPSTYPE, -1,
						 fileSort_choices,
						 4, FALSE, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		LYmove(L_FTPSTYPE, COL_OPTION_VALUES);
		LYclrtoeol();
		LYaddstr(fileSort_choices[HTfileSortMethod]);
#endif /* VMS || USE_SLANG */
	    }
	    response = ' ';
	    if (LYSelectPopups) {
		HANDLE_LYOPTIONS;
	    }
	    break;

	case 'P':		/* Change personal mail address for From headers. */
	    if (non_empty(personal_mail_address)) {
		LYstrncpy(display_option,
			  personal_mail_address,
			  sizeof(display_option) - 1);
	    } else {		/* clear the NONE */
		LYmove(L_MAIL_ADDRESS, COL_OPTION_VALUES);
		LYaddstr("    ");
		*display_option = '\0';
	    }
	    _statusline(ACCEPT_DATA);
	    LYmove(L_MAIL_ADDRESS, COL_OPTION_VALUES);
	    lynx_start_bold();
	    ch = LYgetstr(display_option, VISIBLE,
			  sizeof(display_option), NORECALL);
	    lynx_stop_bold();
	    LYmove(L_MAIL_ADDRESS, COL_OPTION_VALUES);
	    if (term_options || ch == -1) {
		LYaddstr((personal_mail_address &&
			  *personal_mail_address) ?
			 personal_mail_address : "NONE");
	    } else if (*display_option == '\0') {
		FREE(personal_mail_address);
		LYaddstr("NONE");
	    } else {
		StrAllocCopy(personal_mail_address, display_option);
		LYaddstr(display_option);
	    }
	    LYclrtoeol();
	    if (ch == -1) {
		HTInfoMsg(CANCELLED);
		HTInfoMsg("");
	    } else {
		_statusline(VALUE_ACCEPTED);
	    }
	    response = ' ';
	    break;

	case 'S':		/* Change case sensitivity for searches. */
	    case_sensitive = LYChooseBoolean(case_sensitive,
					     L_SSEARCH, -1,
					     caseless_choices);
	    response = ' ';
	    break;

	case '\001':		/* Change assume_charset setting. */
	    if (use_assume_charset) {
		int i, curval;
		const char **assume_list;
		assume_list = typecallocn(const char *, (LYNumCharsets + 1));

		if (!assume_list) {
		    outofmem(__FILE__, "options");
		}
		for (i = 0; i < LYNumCharsets; i++) {
		    assume_list[i] = LYCharSet_UC[i].MIMEname;
		}
		curval = UCLYhndl_for_unspec;
		if (curval == current_char_set && UCAssume_MIMEcharset) {
		    curval = UCGetLYhndl_byMIME(UCAssume_MIMEcharset);
		}
		if (curval < 0)
		    curval = LYRawMode ? current_char_set : 0;
		if (!LYSelectPopups) {
#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN
		    UCLYhndl_for_unspec =
			assumed_doc_charset_map[(LYChooseBoolean(charset_subsets[curval].assumed_idx,
								 L_ASSUME_CHARSET, -1,
								 assumed_charset_choices)
						 ? 1
						 : 0)];
#else
		    UCLYhndl_for_unspec =
			LYChooseBoolean(curval,
					L_ASSUME_CHARSET, -1,
					assume_list);
#endif
		} else {
#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN
		    UCLYhndl_for_unspec =
			assumed_doc_charset_map[(LYChoosePopup(charset_subsets[curval].assumed_idx,
							       L_ASSUME_CHARSET, -1,
							       assumed_charset_choices,
							       0,
							       FALSE,
							       FALSE)
						 ? 1
						 : 0)];
#else
		    UCLYhndl_for_unspec =
			LYChoosePopup(curval,
				      L_ASSUME_CHARSET, -1,
				      assume_list,
				      0, FALSE, FALSE);
#endif
#if defined(VMS) || defined(USE_SLANG)
		    LYmove(L_ASSUME_CHARSET, COL_OPTION_VALUES);
		    LYclrtoeol();
		    if (UCLYhndl_for_unspec >= 0)
			LYaddstr(LYCharSet_UC[UCLYhndl_for_unspec].MIMEname);
#endif /* VMS || USE_SLANG */
		}

		/*
		 * Set the raw 8-bit or CJK mode defaults and character set if
		 * changed.  - FM
		 */
		if (CurrentAssumeCharSet != UCLYhndl_for_unspec ||
		    UCLYhndl_for_unspec != curval) {
		    if (UCLYhndl_for_unspec != CurrentAssumeCharSet) {
			StrAllocCopy(UCAssume_MIMEcharset,
				     LYCharSet_UC[UCLYhndl_for_unspec].MIMEname);
		    }
		    if (HTCJK != JAPANESE)
			LYRawMode = (BOOL) (UCLYhndl_for_unspec == current_char_set);
		    HTMLSetUseDefaultRawMode(current_char_set, LYRawMode);
		    HTMLSetCharacterHandling(current_char_set);
		    CurrentAssumeCharSet = UCLYhndl_for_unspec;
		    CurrentRawMode = LYRawMode;
#if !defined(VMS) && !defined(USE_SLANG)
		    if (!LYSelectPopups)
#endif /* !VMS && !USE_SLANG */
		    {
			LYmove(L_Rawmode, COL_OPTION_VALUES);
			LYclrtoeol();
			ShowBool(LYRawMode);
		    }
		}
		FREE(assume_list);
		response = ' ';
		if (LYSelectPopups) {
		    HANDLE_LYOPTIONS;
		}
	    } else {
		_statusline(NEED_ADVANCED_USER_MODE);
		AddValueAccepted = FALSE;
	    }
	    break;

	case 'C':		/* Change display charset setting. */
	    if (!LYSelectPopups) {
#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN
		displayed_display_charset_idx = LYChooseBoolean(displayed_display_charset_idx,
								L_Charset, -1,
								display_charset_choices);
		current_char_set = display_charset_map[displayed_display_charset_idx];
#else
		current_char_set = LYChooseBoolean(current_char_set,
						   L_Charset, -1,
						   LYchar_set_names);
#endif
	    } else {
#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN
		displayed_display_charset_idx = LYChoosePopup(displayed_display_charset_idx,
							      L_Charset, -1,
							      display_charset_choices,
							      0, FALSE, FALSE);
		current_char_set = display_charset_map[displayed_display_charset_idx];
#else
		current_char_set = LYChoosePopup(current_char_set,
						 L_Charset, -1,
						 LYchar_set_names,
						 0, FALSE, FALSE);
#endif

#if defined(VMS) || defined(USE_SLANG)
		LYmove(L_Charset, COL_OPTION_VALUES);
		LYclrtoeol();
		LYaddstr(LYchar_set_names[current_char_set]);
#endif /* VMS || USE_SLANG */
	    }
	    /*
	     * Set the raw 8-bit or CJK mode defaults and character set if
	     * changed.  - FM
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
		    LYmove(L_Rawmode, COL_OPTION_VALUES);
		    LYclrtoeol();
		    ShowBool(LYRawMode);
		}
#ifdef CAN_SWITCH_DISPLAY_CHARSET
		/* Deduce whether the user wants autoswitch: */
		switch_display_charsets =
		    (current_char_set == auto_display_charset
		     || current_char_set == auto_other_display_charset);
#endif
	    }
	    response = ' ';
	    if (LYSelectPopups) {
		HANDLE_LYOPTIONS;
	    }
	    break;

	case 'O':		/* Change raw mode setting. */
	    LYRawMode = LYChooseBoolean(LYRawMode, L_Rawmode, -1, bool_choices);
	    /*
	     * Set the LYUseDefaultRawMode value and character handling if
	     * LYRawMode was changed.  - FM
	     */
	    if (CurrentRawMode != LYRawMode) {
		HTMLSetUseDefaultRawMode(current_char_set, LYRawMode);
		HTMLSetCharacterHandling(current_char_set);
		CurrentRawMode = LYRawMode;
	    }
	    response = ' ';
	    break;

	case 'G':		/* Change language preference. */
	    if (non_empty(language)) {
		LYstrncpy(display_option, language, sizeof(display_option) - 1);
	    } else {		/* clear the NONE */
		LYmove(L_LANGUAGE, COL_OPTION_VALUES);
		LYaddstr("    ");
		*display_option = '\0';
	    }
	    _statusline(ACCEPT_DATA);
	    LYmove(L_LANGUAGE, COL_OPTION_VALUES);
	    lynx_start_bold();
	    ch = LYgetstr(display_option, VISIBLE,
			  sizeof(display_option), NORECALL);
	    lynx_stop_bold();
	    LYmove(L_LANGUAGE, COL_OPTION_VALUES);
	    if (term_options || ch == -1) {
		LYaddstr(non_empty(language) ?
			 language : "NONE");
	    } else if (*display_option == '\0') {
		FREE(language);
		LYaddstr("NONE");
	    } else {
		StrAllocCopy(language, display_option);
		LYaddstr(display_option);
	    }
	    LYclrtoeol();
	    if (ch == -1) {
		HTInfoMsg(CANCELLED);
		HTInfoMsg("");
	    } else {
		_statusline(VALUE_ACCEPTED);
	    }
	    response = ' ';
	    break;

	case 'H':		/* Change charset preference. */
	    if (non_empty(pref_charset)) {
		LYstrncpy(display_option,
			  pref_charset,
			  sizeof(display_option) - 1);
	    } else {		/* clear the NONE */
		LYmove(L_PREF_CHARSET, COL_OPTION_VALUES);
		LYaddstr("    ");
		*display_option = '\0';
	    }
	    _statusline(ACCEPT_DATA);
	    LYmove(L_PREF_CHARSET, COL_OPTION_VALUES);
	    lynx_start_bold();
	    ch = LYgetstr(display_option, VISIBLE,
			  sizeof(display_option), NORECALL);
	    lynx_stop_bold();
	    LYmove(L_PREF_CHARSET, COL_OPTION_VALUES);
	    if (term_options || ch == -1) {
		LYaddstr(non_empty(pref_charset) ?
			 pref_charset : "NONE");
	    } else if (*display_option == '\0') {
		FREE(pref_charset);
		LYaddstr("NONE");
	    } else {
		StrAllocCopy(pref_charset, display_option);
		LYaddstr(display_option);
	    }
	    LYclrtoeol();
	    if (ch == -1) {
		HTInfoMsg(CANCELLED);
		HTInfoMsg("");
	    } else {
		_statusline(VALUE_ACCEPTED);
	    }
	    response = ' ';
	    break;

	case 'V':		/* Change VI keys setting. */
	    vi_keys = LYChooseBoolean(vi_keys,
				      L_Bool_A, C_VIKEYS,
				      bool_choices);
	    if (vi_keys) {
		set_vi_keys();
	    } else {
		reset_vi_keys();
	    }
	    response = ' ';
	    break;

	case 'M':		/* Change emacs keys setting. */
	    emacs_keys = LYChooseBoolean(emacs_keys,
					 L_Bool_A, C_EMACSKEYS,
					 bool_choices);
	    if (emacs_keys) {
		set_emacs_keys();
	    } else {
		reset_emacs_keys();
	    }
	    response = ' ';
	    break;

	case 'W':		/* Change show dotfiles setting. */
	    if (no_dotfiles) {
		_statusline(DOTFILE_ACCESS_DISABLED);
	    } else {
		show_dotfiles = LYChooseBoolean(show_dotfiles,
						L_Bool_A,
						C_SHOW_DOTFILES,
						bool_choices);
	    }
	    response = ' ';
	    break;

	case 'T':		/* Change select popups setting. */
	    LYSelectPopups = LYChooseBoolean(LYSelectPopups,
					     L_Bool_B,
					     C_SELECT_POPUPS,
					     bool_choices);
	    response = ' ';
	    break;

#if defined(USE_SLANG) || defined(COLOR_CURSES)
	case '&':		/* Change show color setting. */
	    if (no_option_save) {
#if defined(COLOR_CURSES)
		if (!has_colors()) {
		    char *terminal = LYGetEnv("TERM");

		    if (terminal)
			HTUserMsg2(COLOR_TOGGLE_DISABLED_FOR_TERM,
				   terminal);
		    else
			HTUserMsg(COLOR_TOGGLE_DISABLED);
		    break;
		}
#endif
		LYShowColor = LYChooseBoolean((LYShowColor - 1),
					      L_Color,
					      C_COLOR,
					      bool_choices);
		if (LYShowColor == 0) {
		    LYShowColor = SHOW_COLOR_OFF;
		} else {
		    LYShowColor = SHOW_COLOR_ON;
		}
	    } else {		/* !no_option_save */
		BOOLEAN again = FALSE;
		int chosen;

		/*
		 * Copy strings into choice array.
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
			chosen = LYChooseBoolean(LYChosenShowColor,
						 L_Color,
						 C_COLOR,
						 choices);
		    } else {
			chosen = LYChoosePopup(LYChosenShowColor,
					       L_Color,
					       C_COLOR,
					       choices, 4, FALSE, FALSE);
		    }
#if defined(COLOR_CURSES)
		    again = (BOOL) (chosen == SHOW_COLOR_ON && !has_colors());
		    if (again) {
			char *terminal = LYGetEnv("TERM");

			if (terminal)
			    HTUserMsg2(COLOR_TOGGLE_DISABLED_FOR_TERM,
				       terminal);
			else
			    HTUserMsg(COLOR_TOGGLE_DISABLED);
		    }
#endif
		} while (again);
		LYChosenShowColor = chosen;
#if defined(VMS)
		if (LYSelectPopups) {
		    LYmove(L_Color, C_COLOR);
		    LYclrtoeol();
		    LYaddstr(choices[LYChosenShowColor]);
		}
#endif /* VMS */
#if defined(COLOR_CURSES)
		if (has_colors())
#endif
		    LYShowColor = chosen;
		FREE(choices[0]);
		FREE(choices[1]);
		FREE(choices[2]);
		FREE(choices[3]);
	    }
	    if (CurrentShowColor != LYShowColor) {
		lynx_force_repaint();
	    }
	    CurrentShowColor = LYShowColor;
#ifdef USE_SLANG
	    SLtt_Use_Ansi_Colors = (LYShowColor > SHOW_COLOR_OFF ? TRUE : FALSE);
#endif
	    response = ' ';
	    if (LYSelectPopups && !no_option_save) {
		HANDLE_LYOPTIONS;
	    }
	    break;
#endif /* USE_SLANG or COLOR_CURSES */

	case '@':		/* Change show cursor setting. */
	    LYShowCursor = LYChooseBoolean(LYShowCursor,
					   L_Bool_B,
					   C_SHOW_CURSOR,
					   bool_choices);
	    response = ' ';
	    break;

	case 'K':		/* Change keypad mode. */
	    if (!LYSelectPopups) {
		keypad_mode = LYChooseBoolean(keypad_mode,
					      L_Keypad, -1,
					      keypad_choices);
	    } else {
		keypad_mode = LYChoosePopup(keypad_mode,
					    L_Keypad, -1,
					    keypad_choices,
					    3, FALSE, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		LYmove(L_Keypad, COL_OPTION_VALUES);
		LYclrtoeol();
		LYaddstr(keypad_choices[keypad_mode]);
#endif /* VMS || USE_SLANG */
	    }
	    if (keypad_mode == NUMBERS_AS_ARROWS) {
		set_numbers_as_arrows();
	    } else {
		reset_numbers_as_arrows();
	    }
	    response = ' ';
	    if (LYSelectPopups) {
		HANDLE_LYOPTIONS;
	    }
	    break;

	case 'N':		/* Change line editor key bindings. */
	    if (!LYSelectPopups) {
		current_lineedit = LYChooseBoolean(current_lineedit,
						   L_Lineed, -1,
						   LYLineeditNames);
	    } else {
		current_lineedit = LYChoosePopup(current_lineedit,
						 L_Lineed, -1,
						 LYLineeditNames,
						 0, FALSE, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		LYmove(L_Lineed, COL_OPTION_VALUES);
		LYclrtoeol();
		LYaddstr(LYLineeditNames[current_lineedit]);
#endif /* VMS || USE_SLANG */
	    }
	    response = ' ';
	    if (LYSelectPopups) {
		HANDLE_LYOPTIONS;
	    }
	    break;

#ifdef EXP_KEYBOARD_LAYOUT
	case 'Y':		/* Change keyboard layout */
	    if (!LYSelectPopups) {
		current_layout = LYChooseBoolean(current_layout,
						 L_Layout, -1,
						 LYKbLayoutNames);
	    } else {
		current_layout = LYChoosePopup(current_layout,
					       L_Layout, -1,
					       LYKbLayoutNames,
					       0, FALSE, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		LYmove(L_Layout, COL_OPTION_VALUES);
		LYclrtoeol();
		LYaddstr(LYKbLayoutNames[current_layout]);
#endif /* VMS || USE_SLANG */
	    }
	    response = ' ';
	    if (LYSelectPopups) {
		HANDLE_LYOPTIONS;
	    }
	    break;
#endif /* EXP_KEYBOARD_LAYOUT */

#ifdef DIRED_SUPPORT
	case 'I':		/* Change local directory sorting. */
	    if (!LYSelectPopups) {
		dir_list_style = LYChooseBoolean(dir_list_style,
						 L_Dired, -1,
						 dirList_choices);
	    } else {
		dir_list_style = LYChoosePopup(dir_list_style,
					       L_Dired, -1,
					       dirList_choices,
					       3, FALSE, FALSE);
#if defined(VMS) || defined(USE_SLANG)
		LYmove(L_Dired, COL_OPTION_VALUES);
		LYclrtoeol();
		LYaddstr(dirList_choices[dir_list_style]);
#endif /* VMS || USE_SLANG */
	    }
	    response = ' ';
	    if (LYSelectPopups) {
		HANDLE_LYOPTIONS;
	    }
	    break;
#endif /* DIRED_SUPPORT */

	case 'U':		/* Change user mode. */
	    if (!LYSelectPopups) {
		user_mode = LYChooseBoolean(user_mode,
					    L_User_Mode, -1,
					    userMode_choices);
		use_assume_charset = (BOOL) (user_mode >= 2);
	    } else {
		user_mode = LYChoosePopup(user_mode,
					  L_User_Mode, -1,
					  userMode_choices,
					  3, FALSE, FALSE);
		use_assume_charset = (BOOL) (user_mode >= 2);
#if defined(VMS) || defined(USE_SLANG)
		if (use_assume_charset == old_use_assume_charset) {
		    LYmove(L_User_Mode, COL_OPTION_VALUES);
		    LYclrtoeol();
		    LYaddstr(userMode_choices[user_mode]);
		}
#endif /* VMS || USE_SLANG */
	    }
	    if (user_mode == NOVICE_MODE) {
		display_lines = (LYlines - 4);
	    } else {
		display_lines = LYlines - 2;
	    }
	    response = ' ';
	    if (LYSelectPopups) {
		HANDLE_LYOPTIONS;
	    }
	    break;

	case '!':
	    if (!LYSelectPopups) {
		verbose_img = LYChooseBoolean(verbose_img,
					      L_VERBOSE_IMAGES,
					      C_VERBOSE_IMAGES,
					      bool_choices);
	    } else {
		verbose_img = LYChoosePopup(verbose_img,
					    L_VERBOSE_IMAGES,
					    C_VERBOSE_IMAGES,
					    bool_choices,
					    2, FALSE, FALSE);
	    }
	    response = ' ';
	    if (LYSelectPopups) {
		HANDLE_LYOPTIONS;
	    }
	    break;

	case 'A':		/* Change user agent string. */
	    if (!no_useragent) {
		if (non_empty(LYUserAgent)) {
		    LYstrncpy(display_option,
			      LYUserAgent,
			      sizeof(display_option) - 1);
		} else {	/* clear the NONE */
		    LYmove(L_HOME, COL_OPTION_VALUES);
		    LYaddstr("    ");
		    *display_option = '\0';
		}
		_statusline(ACCEPT_DATA_OR_DEFAULT);
		LYmove(L_User_Agent, COL_OPTION_VALUES);
		lynx_start_bold();
		ch = LYgetstr(display_option, VISIBLE,
			      sizeof(display_option), NORECALL);
		lynx_stop_bold();
		LYmove(L_User_Agent, COL_OPTION_VALUES);
		if (term_options || ch == -1) {
		    LYaddstr((LYUserAgent &&
			      *LYUserAgent) ?
			     LYUserAgent : "NONE");
		} else if (*display_option == '\0') {
		    StrAllocCopy(LYUserAgent, LYUserAgentDefault);
		    LYaddstr((LYUserAgent &&
			      *LYUserAgent) ?
			     LYUserAgent : "NONE");
		} else {
		    StrAllocCopy(LYUserAgent, display_option);
		    LYaddstr(display_option);
		}
		LYclrtoeol();
		if (ch == -1) {
		    HTInfoMsg(CANCELLED);
		    HTInfoMsg("");
		} else if (!LYCheckUserAgent()) {
		    _statusline(UA_PLEASE_USE_LYNX);
		} else {
		    _statusline(VALUE_ACCEPTED);
		}
	    } else {		/* disallowed */
		_statusline(UA_CHANGE_DISABLED);
	    }
	    response = ' ';
	    break;

#if defined(ENABLE_OPTS_CHANGE_EXEC) && (defined(EXEC_LINKS) || defined(EXEC_SCRIPTS))
	case 'X':		/* Change local exec restriction. */
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
		    itmp = 1;
		} else {
		    itmp = 0;
		}
	    }
	    if (!LYSelectPopups) {
		itmp = LYChooseBoolean(itmp,
				       L_Exec, -1,
				       exec_choices);
	    } else {
		itmp = LYChoosePopup(itmp,
				     L_Exec, -1,
				     exec_choices,
				     0, (exec_frozen ? TRUE : FALSE),
				     FALSE);
#if defined(VMS) || defined(USE_SLANG)
		LYmove(L_Exec, COL_OPTION_VALUES);
		LYclrtoeol();
		LYaddstr(exec_choices[itmp]);
#endif /* VMS || USE_SLANG */
	    }
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
		}		/* end switch */
	    }
	    response = ' ';
	    if (LYSelectPopups) {
		HANDLE_LYOPTIONS;
	    }
	    break;
#endif /* ENABLE_OPTS_CHANGE_EXEC */

	case '>':		/* Save current options to RC file. */
	    if (!no_option_save) {
		HTInfoMsg(SAVING_OPTIONS);
		LYrcShowColor = LYChosenShowColor;
		if (save_rc(NULL)) {
		    HTInfoMsg(OPTIONS_SAVED);
		} else {
		    HTAlert(OPTIONS_NOT_SAVED);
		}
	    } else {
		HTInfoMsg(R_TO_RETURN_TO_LYNX);
		/*
		 * Change response so that we don't exit the options menu.
		 */
		response = ' ';
	    }
	    break;

	case 'R':		/* Return to document (quit options menu). */
	    break;

	default:
	    if (!no_option_save) {
		HTInfoMsg(SAVE_OR_R_TO_RETURN_TO_LYNX);
	    } else {
		HTInfoMsg(R_TO_RETURN_TO_LYNX);
	    }
	}			/* end switch */
    }				/* end while */

    term_options = FALSE;
    LYStatusLine = -1;		/* let user_mode have some of the screen */
    signal(SIGINT, cleanup_sig);
}

static int widest_choice(const char **choices)
{
    int n, width = 0;

    for (n = 0; choices[n] != NULL; ++n) {
	int len = strlen(choices[n]);

	if (width < len)
	    width = len;
    }
    return width;
}

static void show_choice(const char *choice,
			int width)
{
    int len = strlen(choice);

    LYaddstr(choice);
    while (len++ < width)
	LYaddch(' ');
}

/*
 * Take a status code, prompt the user for a new status, and return it.
 */
static int boolean_choice(int cur_choice,
			  int line,
			  int column,
			  const char **choices)
{
    int response = 0;
    int cmd = 0;
    int number = 0;
    int col = (column >= 0 ? column : COL_OPTION_VALUES);
    int orig_choice = cur_choice;
    int width = widest_choice(choices);

    /*
     * Get the number of choices and then make number zero-based.
     */
    for (number = 0; choices[number] != NULL; number++) ;	/* empty loop body */
    number--;

    /*
     * Update the statusline.
     */
    _statusline(ANY_KEY_CHANGE_RET_ACCEPT);

    /*
     * Highlight the current choice.
     */
    LYmove(line, col);
    lynx_start_reverse();
    show_choice(choices[cur_choice], width);
    if (LYShowCursor)
	LYmove(line, (col - 1));
    LYrefresh();

    /*
     * Get the keyboard entry, and leave the cursor at the choice, to indicate
     * that it can be changed, until the user accepts the current choice.
     */
    term_options = FALSE;
    while (1) {
	LYmove(line, col);
	if (term_options == FALSE) {
	    response = LYgetch_single();
	}
	if (term_options || LYCharIsINTERRUPT_NO_letter(response)) {
	    /*
	     * Control-C or Control-G.
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
	    (cmd = LKC_TO_LAC(keymap, response)) != LYK_ACTIVATE) {
	    switch (cmd) {
	    case LYK_HOME:
		cur_choice = 0;
		break;

	    case LYK_END:
		cur_choice = number;
		break;

	    case LYK_REFRESH:
		lynx_force_repaint();
		LYrefresh();
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
	    case LYK_LPOS_PREV_LINK:
	    case LYK_FASTBACKW_LINK:
	    case LYK_UP_LINK:
	    case LYK_LEFT_LINK:
		if (cur_choice == 0)
		    cur_choice = number;	/* go back to end */
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
		if ((cmd - LYK_1 + 1) <= number) {
		    cur_choice = cmd - LYK_1 + 1;
		    break;
		}		/* else fall through! */
	    default:
		if (cur_choice == number)
		    cur_choice = 0;	/* go over the top and around */
		else
		    cur_choice++;
	    }			/* end of switch */
	    show_choice(choices[cur_choice], width);
	    if (LYShowCursor)
		LYmove(line, (col - 1));
	    LYrefresh();
	} else {
	    /*
	     * Unhighlight choice.
	     */
	    LYmove(line, col);
	    lynx_stop_reverse();
	    show_choice(choices[cur_choice], width);

	    if (term_options) {
		term_options = FALSE;
		HTInfoMsg(CANCELLED);
		HTInfoMsg("");
	    } else {
		_statusline(VALUE_ACCEPTED);
	    }
	    return cur_choice;
	}
    }
}
#endif /* !NO_OPTION_MENU */

static void terminate_options(int sig GCC_UNUSED)
{
    term_options = TRUE;
    /*
     * Reassert the AST.
     */
    signal(SIGINT, terminate_options);
#ifdef VMS
    /*
     * Refresh the screen to get rid of the "interrupt" message.
     */
    if (!dump_output_immediately) {
	lynx_force_repaint();
	LYrefresh();
    }
#endif /* VMS */
}

/*
 * Multi-Bookmark On-Line editing support.  - FMG & FM
 */
void edit_bookmarks(void)
{
    int response = 0, def_response = 0, ch;
    int MBM_current = 1;

#define MULTI_OFFSET 8
    int a;			/* misc counter */
    char MBM_tmp_line[256];	/* buffer for LYgetstr */
    char ehead_buffer[265];

    /*
     * We need (MBM_V_MAXFILES + MULTI_OFFSET) lines to display the whole list
     * at once.  Otherwise break it up into two segments.  We know it won't be
     * less than that because 'o'ptions needs 23-24 at LEAST.
     */
    term_options = FALSE;
    signal(SIGINT, terminate_options);

  draw_bookmark_list:
    /*
     * Display menu of bookmarks.  NOTE that we avoid printw()'s to increase
     * the chances that any non-ASCII or multibyte/CJK characters will be
     * handled properly.  - FM
     */
#if defined(FANCY_CURSES) || defined (USE_SLANG)
    if (enable_scrollback) {
	LYclear();
    } else {
	LYerase();
    }
#else
    LYclear();
#endif /* FANCY_CURSES || USE_SLANG */
    LYmove(0, 5);
    lynx_start_h1_color();
    if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET)) {
	sprintf(ehead_buffer, MULTIBOOKMARKS_EHEAD_MASK, MBM_current);
	LYaddstr(ehead_buffer);
    } else {
	LYaddstr(MULTIBOOKMARKS_EHEAD);
    }
    lynx_stop_h1_color();

    if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET)) {
	for (a = ((MBM_V_MAXFILES / 2 + 1) * (MBM_current - 1));
	     a <= (MBM_current * MBM_V_MAXFILES / 2); a++) {
	    LYmove((3 + a) - ((MBM_V_MAXFILES / 2 + 1) * (MBM_current - 1)), 5);
	    LYaddch(UCH(LYindex2MBM(a)));
	    LYaddstr(" : ");
	    if (MBM_A_subdescript[a])
		LYaddstr(MBM_A_subdescript[a]);
	    LYmove((3 + a) - ((MBM_V_MAXFILES / 2 + 1) * (MBM_current - 1)), 35);
	    LYaddstr("| ");
	    if (MBM_A_subbookmark[a]) {
		LYaddstr(MBM_A_subbookmark[a]);
	    }
	}
    } else {
	for (a = 0; a <= MBM_V_MAXFILES; a++) {
	    LYmove(3 + a, 5);
	    LYaddch(UCH(LYindex2MBM(a)));
	    LYaddstr(" : ");
	    if (MBM_A_subdescript[a])
		LYaddstr(MBM_A_subdescript[a]);
	    LYmove(3 + a, 35);
	    LYaddstr("| ");
	    if (MBM_A_subbookmark[a]) {
		LYaddstr(MBM_A_subbookmark[a]);
	    }
	}
    }

    /*
     * Only needed when we have 2 screens.
     */
    if (LYlines < MBM_V_MAXFILES + MULTI_OFFSET) {
	LYmove((LYlines - 4), 0);
	LYaddstr("'");
	lynx_start_bold();
	LYaddstr("[");
	lynx_stop_bold();
	LYaddstr("' ");
	LYaddstr(PREVIOUS);
	LYaddstr(", '");
	lynx_start_bold();
	LYaddstr("]");
	lynx_stop_bold();
	LYaddstr("' ");
	LYaddstr(NEXT_SCREEN);
    }

    LYmove((LYlines - 3), 0);
    if (!no_option_save) {
	LYaddstr("'");
	lynx_start_bold();
	LYaddstr(">");
	lynx_stop_bold();
	LYaddstr("'");
	LYaddstr(TO_SAVE_SEGMENT);
    }
    LYaddstr(OR_SEGMENT);
    LYaddstr("'");
    lynx_start_bold();
    LYaddstr("^G");
    lynx_stop_bold();
    LYaddstr("'");
    LYaddstr(TO_RETURN_SEGMENT);

    while (!term_options &&
	   !LYisNonAlnumKeyname(response, LYK_PREV_DOC) &&
	   !LYCharIsINTERRUPT_NO_letter(response) && response != '>') {

	LYmove((LYlines - 2), 0);
	lynx_start_prompt_color();
	LYaddstr(MULTIBOOKMARKS_LETTER);
	lynx_stop_prompt_color();

	LYrefresh();
	response = (def_response ? def_response : LYgetch_single());
	def_response = 0;

	/*
	 * Check for a cancel.
	 */
	if (term_options || LYCharIsINTERRUPT_NO_letter(response) ||
	    LYisNonAlnumKeyname(response, LYK_PREV_DOC))
	    continue;

	/*
	 * Check for a save.
	 */
	if (response == '>') {
	    if (!no_option_save) {
		HTInfoMsg(SAVING_OPTIONS);
		if (save_rc(NULL))
		    HTInfoMsg(OPTIONS_SAVED);
		else
		    HTAlert(OPTIONS_NOT_SAVED);
	    } else {
		HTInfoMsg(R_TO_RETURN_TO_LYNX);
		/*
		 * Change response so that we don't exit the options menu.
		 */
		response = ' ';
	    }
	    continue;
	}

	/*
	 * Check for a refresh.
	 */
	if (LYisNonAlnumKeyname(response, LYK_REFRESH)) {
	    lynx_force_repaint();
	    continue;
	}

	/*
	 * Move between the screens - if we can't show it all at once.
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
	 * Instead of using 26 case statements, we set up a scan through the
	 * letters and edit the lines that way.
	 */
	for (a = 0; a <= MBM_V_MAXFILES; a++) {
	    if (LYMBM2index(response) == a) {
		if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET)) {
		    if (MBM_current == 1 && a > (MBM_V_MAXFILES / 2)) {
			MBM_current = 2;
			def_response = response;
			goto draw_bookmark_list;
		    }
		    if (MBM_current == 2 && a < (MBM_V_MAXFILES / 2)) {
			MBM_current = 1;
			def_response = response;
			goto draw_bookmark_list;
		    }
		}
		_statusline(ACCEPT_DATA);

		if (a > 0) {
		    lynx_start_bold();
		    if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET))
			LYmove((3 + a)
			       - ((MBM_V_MAXFILES / 2 + 1) * (MBM_current - 1)),
			       9);
		    else
			LYmove((3 + a), 9);
		    LYstrncpy(MBM_tmp_line,
			      (!MBM_A_subdescript[a] ?
			       "" : MBM_A_subdescript[a]),
			      sizeof(MBM_tmp_line) - 1);
		    ch = LYgetstr(MBM_tmp_line, VISIBLE,
				  sizeof(MBM_tmp_line), NORECALL);
		    lynx_stop_bold();

		    if (strlen(MBM_tmp_line) < 1) {
			FREE(MBM_A_subdescript[a]);
		    } else {
			StrAllocCopy(MBM_A_subdescript[a], MBM_tmp_line);
		    }
		    if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET))
			LYmove((3 + a)
			       - ((MBM_V_MAXFILES / 2 + 1)
				  * (MBM_current - 1)),
			       5);
		    else
			LYmove((3 + a), 5);
		    LYaddch(UCH(LYindex2MBM(a)));
		    LYaddstr(" : ");
		    if (MBM_A_subdescript[a])
			LYaddstr(MBM_A_subdescript[a]);
		    LYclrtoeol();
		    LYrefresh();
		}

		if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET))
		    LYmove((3 + a)
			   - ((MBM_V_MAXFILES / 2 + 1)
			      * (MBM_current - 1)),
			   35);
		else
		    LYmove((3 + a), 35);
		LYaddstr("| ");

		lynx_start_bold();
		LYstrncpy(MBM_tmp_line,
			  NonNull(MBM_A_subbookmark[a]),
			  sizeof(MBM_tmp_line) - 1);
		ch = LYgetstr(MBM_tmp_line, VISIBLE,
			      sizeof(MBM_tmp_line), NORECALL);
		lynx_stop_bold();

		if (*MBM_tmp_line == '\0') {
		    if (a == 0)
			StrAllocCopy(MBM_A_subbookmark[a], bookmark_page);
		    else
			FREE(MBM_A_subbookmark[a]);
		} else if (!LYPathOffHomeOK(MBM_tmp_line,
					    sizeof(MBM_tmp_line))) {
		    LYMBM_statusline(USE_PATH_OFF_HOME);
		    LYSleepAlert();
		} else {
		    StrAllocCopy(MBM_A_subbookmark[a], MBM_tmp_line);
		    if (a == 0) {
			StrAllocCopy(bookmark_page, MBM_A_subbookmark[a]);
		    }
		}
		if (LYlines < (MBM_V_MAXFILES + MULTI_OFFSET))
		    LYmove((3 + a)
			   - ((MBM_V_MAXFILES / 2 + 1)
			      * (MBM_current - 1)),
			   35);
		else
		    LYmove((3 + a), 35);
		LYaddstr("| ");
		if (MBM_A_subbookmark[a])
		    LYaddstr(MBM_A_subbookmark[a]);
		LYclrtoeol();
		LYmove(LYlines - 1, 0);
		LYclrtoeol();
		break;
	    }
	}			/* end for */
    }				/* end while */

    term_options = FALSE;
    signal(SIGINT, cleanup_sig);
}

#if defined(USE_CURSES_PADS) || !defined(NO_OPTION_MENU) || (defined(USE_MOUSE) && (defined(NCURSES) || defined(PDCURSES)))

/*
 * This function offers the choices for values of an option via a popup window
 * which functions like that for selection of options in a form.  - FM
 *
 * Also used for mouse popups with ncurses; this is indicated by for_mouse.
 */
int popup_choice(int cur_choice,
		 int line,
		 int column,
		 const char **choices,
		 int i_length,
		 int disabled,
		 BOOLEAN for_mouse)
{
    if (column < 0)
	column = (COL_OPTION_VALUES - 1);

    term_options = FALSE;
    cur_choice = LYhandlePopupList(cur_choice,
				   line,
				   column,
				   (const char **) choices,
				   -1,
				   i_length,
				   disabled,
				   for_mouse,
				   TRUE);
    switch (cur_choice) {
    case LYK_QUIT:
    case LYK_ABORT:
    case LYK_PREV_DOC:
	term_options = TRUE;
	if (!for_mouse) {
	    HTUserMsg(CANCELLED);
	}
	break;
    }

    if (disabled || term_options) {
	_statusline("");
    } else if (!for_mouse) {
	_statusline(VALUE_ACCEPTED);
    }
    return (cur_choice);
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

typedef struct {
    int value;
    const char *LongName;
    const char *HtmlName;
} OptValues;

typedef struct {
    char *tag;
    char *value;
} PostPair;

static const char selected_string[] = "selected";
static const char disabled_string[] = "disabled";
static const char on_string[] = N_("ON");
static const char off_string[] = N_("OFF");
static const char never_string[] = N_("NEVER");
static const char always_string[] = N_("ALWAYS");
static OptValues bool_values[] =
{
    {FALSE, N_("OFF"), "OFF"},
    {TRUE, N_("ON"), "ON"},
    {0, 0, 0}
};

static const char *secure_string = "secure";
static char *secure_value = NULL;
static const char *save_options_string = "save_options";

/*
 * Personal Preferences
 */
static const char *cookies_string = RC_SET_COOKIES;
static const char *cookies_ignore_all_string = N_("ignore");
static const char *cookies_up_to_user_string = N_("ask user");
static const char *cookies_accept_all_string = N_("accept all");
static const char *x_display_string = RC_DISPLAY;
static const char *editor_string = RC_FILE_EDITOR;
static const char *emacs_keys_string = RC_EMACS_KEYS;

#if defined(ENABLE_OPTS_CHANGE_EXEC) && (defined(EXEC_LINKS) || defined(EXEC_SCRIPTS))
#define EXEC_ALWAYS 2
#define EXEC_LOCAL  1
#define EXEC_NEVER  0
static const char *exec_links_string = RC_RUN_ALL_EXECUTION_LINKS;
static OptValues exec_links_values[] =
{
    {EXEC_NEVER, N_("ALWAYS OFF"), "ALWAYS OFF"},
    {EXEC_LOCAL, N_("FOR LOCAL FILES ONLY"), "FOR LOCAL FILES ONLY"},
#ifndef NEVER_ALLOW_REMOTE_EXEC
    {EXEC_ALWAYS, N_("ALWAYS ON"), "ALWAYS ON"},
#endif
    {0, 0, 0}
};
#endif /* ENABLE_OPTS_CHANGE_EXEC */

#ifdef EXP_KEYBOARD_LAYOUT
static const char *kblayout_string = RC_KBLAYOUT;
#endif
static const char *keypad_mode_string = RC_KEYPAD_MODE;
static OptValues keypad_mode_values[] =
{
    {NUMBERS_AS_ARROWS, N_("Numbers act as arrows"),
     "number_arrows"},
    {LINKS_ARE_NUMBERED, N_("Links are numbered"),
     "links_numbered"},
    {LINKS_AND_FIELDS_ARE_NUMBERED,
     N_("Links and form fields are numbered"),
     "links_and_forms"},
    {FIELDS_ARE_NUMBERED,
     N_("Form fields are numbered"),
     "forms_numbered"},
    {0, 0, 0}
};
static const char *lineedit_mode_string = RC_LINEEDIT_MODE;
static const char *mail_address_string = RC_PERSONAL_MAIL_ADDRESS;
static const char *anonftp_password_string = RC_ANONFTP_PASSWORD;
static const char *search_type_string = RC_CASE_SENSITIVE_SEARCHING;
static OptValues search_type_values[] =
{
    {FALSE, N_("Case insensitive"), "case_insensitive"},
    {TRUE, N_("Case sensitive"), "case_sensitive"},
    {0, 0, 0}
};

#if defined(USE_SLANG) || defined(COLOR_CURSES)
static const char *show_color_string = RC_SHOW_COLOR;
static OptValues show_color_values[] =
{
    {SHOW_COLOR_NEVER, never_string, never_string},
    {SHOW_COLOR_OFF, off_string, off_string},
    {SHOW_COLOR_ON, on_string, on_string},
    {SHOW_COLOR_ALWAYS, always_string, always_string},
    {0, 0, 0}
};
#endif

static const char *show_cursor_string = RC_SHOW_CURSOR;

static const char *underline_links_string = RC_UNDERLINE_LINKS;

#ifdef USE_SCROLLBAR
static const char *show_scrollbar_string = RC_SCROLLBAR;
#endif

static const char prompt_dft_string[] = N_("prompt normally");
static const char prompt_yes_string[] = N_("force yes-response");
static const char prompt_no_string[] = N_("force no-response");
static OptValues prompt_values[] =
{
    {FORCE_PROMPT_DFT, prompt_dft_string, prompt_dft_string},
    {FORCE_PROMPT_YES, prompt_yes_string, prompt_yes_string},
    {FORCE_PROMPT_NO, prompt_no_string, prompt_no_string},
    {0, 0, 0}
};

static const char *cookie_prompt_string = RC_FORCE_COOKIE_PROMPT;

#ifdef USE_SSL
static const char *ssl_prompt_string = RC_FORCE_SSL_PROMPT;
#endif

static const char *user_mode_string = RC_USER_MODE;
static OptValues user_mode_values[] =
{
    {NOVICE_MODE, N_("Novice"), "Novice"},
    {INTERMEDIATE_MODE, N_("Intermediate"), "Intermediate"},
    {ADVANCED_MODE, N_("Advanced"), "Advanced"},
    {0, 0, 0}
};

static const char *vi_keys_string = RC_VI_KEYS;

static const char *visited_links_string = RC_VISITED_LINKS;
static OptValues visited_links_values[] =
{
    {VISITED_LINKS_AS_FIRST_V, N_("By First Visit"), "first_visited"},
    {VISITED_LINKS_AS_FIRST_V | VISITED_LINKS_REVERSE,
     N_("By First Visit Reversed"), "first_visited_reversed"},
    {VISITED_LINKS_AS_TREE, N_("As Visit Tree"), "visit_tree"},
    {VISITED_LINKS_AS_LATEST, N_("By Last Visit"), "last_visited"},
    {VISITED_LINKS_AS_LATEST | VISITED_LINKS_REVERSE,
     N_("By Last Visit Reversed"), "last_visited_reversed"},
    {0, 0, 0}
};

/*
 * Document Layout
 */
static const char *DTD_recovery_string = RC_TAGSOUP;
static OptValues DTD_type_values[] =
{
	/* Old_DTD variable */
    {TRUE, N_("relaxed (TagSoup mode)"), "tagsoup"},
    {FALSE, N_("strict (SortaSGML mode)"), "sortasgml"},
    {0, 0, 0}
};

static const char *select_popups_string = RC_SELECT_POPUPS;
static const char *images_string = "images";
static const char *images_ignore_all_string = N_("ignore");
static const char *images_use_label_string = N_("as labels");
static const char *images_use_links_string = N_("as links");

static const char *verbose_images_string = RC_VERBOSE_IMAGES;
static OptValues verbose_images_type_values[] =
{
	/* verbose_img variable */
    {FALSE, N_("OFF"), "OFF"},
    {TRUE, N_("show filename"), "ON"},
    {0, 0, 0}
};

/*
 * Bookmark Options
 */
static const char *mbm_string = RC_MULTI_BOOKMARK;
static OptValues mbm_values[] =
{
    {MBM_OFF, N_("OFF"), "OFF"},
    {MBM_STANDARD, N_("STANDARD"), "STANDARD"},
    {MBM_ADVANCED, N_("ADVANCED"), "ADVANCED"},
    {0, 0, 0}
};

static const char *single_bookmark_string = RC_BOOKMARK_FILE;

/*
 * Character Set Options
 */
static const char *assume_char_set_string = RC_ASSUME_CHARSET;
static const char *display_char_set_string = RC_CHARACTER_SET;
static const char *raw_mode_string = RC_RAW_MODE;

#ifdef EXP_LOCALE_CHARSET
static const char *locale_charset_string = RC_LOCALE_CHARSET;
#endif

/*
 * File Management Options
 */
static const char *show_dotfiles_string = RC_SHOW_DOTFILES;

#ifdef DIRED_SUPPORT
static const char *dired_list_string = RC_DIR_LIST_STYLE;
static OptValues dired_list_values[] =
{
    {DIRS_FIRST, N_("Directories first"), "dired_dir"},
    {FILES_FIRST, N_("Files first"), "dired_files"},
    {MIXED_STYLE, N_("Mixed style"), "dired_mixed"},
    {0, 0, 0}
};

#ifdef LONG_LIST
static const char *dired_sort_string = RC_DIR_LIST_ORDER;
static OptValues dired_sort_values[] =
{
    {ORDER_BY_NAME, N_("By Name"), "dired_by_name"},
    {ORDER_BY_TYPE, N_("By Type"), "dired_by_type"},
    {ORDER_BY_SIZE, N_("By Size"), "dired_by_size"},
    {ORDER_BY_DATE, N_("By Date"), "dired_by_date"},
    {ORDER_BY_MODE, N_("By Mode"), "dired_by_mode"},
#ifndef NO_GROUPS
    {ORDER_BY_USER, N_("By User"), "dired_by_user"},
    {ORDER_BY_GROUP, N_("By Group"), "dired_by_group"},
#endif
    {0, 0, 0}
};
#endif /* LONG_LIST */
#endif /* DIRED_SUPPORT */

static const char *ftp_sort_string = RC_FILE_SORTING_METHOD;
static OptValues ftp_sort_values[] =
{
    {FILE_BY_NAME, N_("By Name"), "ftp_by_name"},
    {FILE_BY_TYPE, N_("By Type"), "ftp_by_type"},
    {FILE_BY_SIZE, N_("By Size"), "ftp_by_size"},
    {FILE_BY_DATE, N_("By Date"), "ftp_by_date"},
    {0, 0, 0}
};

#ifdef USE_READPROGRESS
static const char *show_rate_string = RC_SHOW_KB_RATE;
static OptValues rate_values[] =
{
    {rateOFF, N_("Do not show rate"), "rate_off"},
    {rateBYTES, N_("Show %s/sec rate"), "rate_bytes"},
    {rateKB, N_("Show %s/sec rate"), "rate_kb"},
#ifdef USE_READPROGRESS
    {rateEtaBYTES, N_("Show %s/sec, ETA"), "rate_eta_bytes"},
    {rateEtaKB, N_("Show %s/sec, ETA"), "rate_eta_kb"},
#endif
    {0, 0, 0}
};
#endif /* USE_READPROGRESS */

/*
 * Presentation (MIME) types used in "Accept".
 */
static const char *preferred_media_string = RC_PREFERRED_MEDIA_TYPES;
static OptValues media_values[] =
{
    {mediaOpt1, N_("Accept lynx's internal types"), "media_opt1"},
    {mediaOpt2, N_("Also accept lynx.cfg's types"), "media_opt2"},
    {mediaOpt3, N_("Also accept user's types"), "media_opt3"},
    {mediaOpt4, N_("Also accept system's types"), "media_opt4"},
    {mediaALL, N_("Accept all types"), "media_all"},
    {0, 0, 0}
};

static const char *preferred_encoding_string = RC_PREFERRED_ENCODING;
static OptValues encoding_values[] =
{
    {encodingNONE, N_("None"), "encoding_none"},
#if defined(USE_ZLIB) || defined(GZIP_PATH)
    {encodingGZIP, N_("gzip"), "encoding_gzip"},
    {encodingDEFLATE, N_("deflate"), "encoding_deflate"},
#endif
#if defined(USE_ZLIB) || defined(COMPRESS_PATH)
    {encodingCOMPRESS, N_("compress"), "encoding_compress"},
#endif
#if defined(USE_BZLIB) || defined(BZIP2_PATH)
    {encodingBZIP2, N_("bzip2"), "encoding_bzip2"},
#endif
    {encodingALL, N_("All"), "encoding_all"},
    {0, 0, 0}
};

/*
 * Headers transferred to remote server
 */
static const char *preferred_doc_char_string = RC_PREFERRED_CHARSET;
static const char *preferred_doc_lang_string = RC_PREFERRED_LANGUAGE;
static const char *user_agent_string = RC_USERAGENT;

#define PutHeader(fp, Name) \
	fprintf(fp, "\n%s<em>%s</em>\n", MARGIN_STR, Name);

#define PutTextInput(fp, Name, Value, Size, disable) \
	fprintf(fp,\
	"<input size=%d type=\"text\" name=\"%s\" value=\"%s\" %s>\n",\
		(int) Size, Name, Value, disable_all?disabled_string:disable)

#define PutOption(fp, flag, html, name) \
	fprintf(fp,"<option value=\"%s\" %s>%s\n", html, SELECTED(flag), gettext(name))

#define BeginSelect(fp, text) \
	fprintf(fp,"<select name=\"%s\" %s>\n", text, disable_all?disabled_string:"")

#define MaybeSelect(fp, flag, text) \
	fprintf(fp,"<select name=\"%s\" %s>\n", text, disable_all?disabled_string:DISABLED(flag))

#define EndSelect(fp)\
	fprintf(fp,"</select>\n")

static void PutOptValues(FILE *fp, int value,
			 OptValues * table)
{
    while (table->LongName != 0) {
	if (table->HtmlName) {
	    PutOption(fp,
		      value == table->value,
		      table->HtmlName,
		      table->LongName);
	}
	table++;
    }
}

static BOOLEAN GetOptValues(OptValues * table, char *value,
			    int *result)
{
    while (table->LongName != 0) {
	if (table->HtmlName && !strcmp(value, table->HtmlName)) {
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
static PostPair *break_data(bstring *data)
{
    char *p;
    PostPair *q = NULL;
    int count = 0;

    if (isBEmpty(data))
	return NULL;

    p = BStrData(data);
    CTRACE((tfp, "break_data %s\n", p));

    q = typecalloc(PostPair);
    if (q == NULL)
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
			&& q[count].value[i + 1] == '+'
			&& isalnum(UCH(q[count].value[i + 2]))) {
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
	CTRACE((tfp, "...item[%d] tag=%s, value=%s\n",
		count, q[count].tag, q[count].value));

	count++;
	/*
	 * Like I said, screw efficiency.  Sides, realloc is fast on
	 * Linux ;->
	 */
	q = typeRealloc(PostPair, q, count + 1);
	if (q == NULL)
	    outofmem(__FILE__, "break_data(realloc)");
	q[count].tag = NULL;
    } while (p != NULL && p[0] != '\0');
    return q;
}

static BOOL isLynxOptionsPage(const char *address, const char *portion)
{
    BOOL result = FALSE;

    if (!strncasecomp(address, STR_LYNXOPTIONS, LEN_LYNXOPTIONS)) {
	unsigned len = strlen(portion);

	address += LEN_LYNXOPTIONS;
	if (!strncasecomp(address, portion, len)
	    && (address[len] == '\0' || LYIsHtmlSep(address[len]))) {
	    result = TRUE;
	}
    }
    return result;
}

static int gen_options(char **newfile);

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

int postoptions(DocInfo *newdoc)
{
    PostPair *data = 0;
    DocAddress WWWDoc;		/* need on exit */
    int i;
    int code = 0;
    BOOLEAN save_all = FALSE;
    int display_char_set_old = current_char_set;
    int old_media_value = LYAcceptMedia;
    BOOLEAN raw_mode_old = LYRawMode;
    BOOLEAN assume_char_set_changed = FALSE;
    BOOLEAN need_reload = FALSE;
    BOOLEAN need_end_reload = FALSE;

#if defined(USE_SLANG) || defined(COLOR_CURSES)
    int CurrentShowColor = LYShowColor;
#endif

    /*-------------------------------------------------
     * kludge a link from mbm_menu, the URL was:
     * "<a href=\"" LYNXOPTIONS_PAGE(MBM_MENU) "\">Goto multi-bookmark menu</a>\n"
     *--------------------------------------------------*/

    if (isLynxOptionsPage(newdoc->address, MBM_LINK)) {
	FREE(newdoc->post_data);
	if (no_bookmark) {
	    HTAlert(BOOKMARK_CHANGE_DISALLOWED);	/* anonymous */
	    return (NULLFILE);
	} else if (dump_output_immediately) {
	    return (NOT_FOUND);
	} else {
	    edit_bookmarks();
	    return (NULLFILE);
	}
    } else if (!isLynxOptionsPage(newdoc->address, "/")) {
	HTAlert(RANDOM_URL_DISALLOWED);
	return NULLFILE;
    }

    data = break_data(newdoc->post_data);

    if (!data) {
	int status;

	/*-------------------------------------------------
	 * kludge gen_options() call:
	 *--------------------------------------------------*/
	status = gen_options(&newdoc->address);
	if (status != NORMAL) {
	    HTAlwaysAlert("Unexpected way of accessing", newdoc->address);
	    FREE(newdoc->address);
	    return (status);
	}

	/* exit to getfile() cycle */
	WWWDoc.address = newdoc->address;
	WWWDoc.post_data = newdoc->post_data;
	WWWDoc.post_content_type = newdoc->post_content_type;
	WWWDoc.bookmark = newdoc->bookmark;
	WWWDoc.isHEAD = newdoc->isHEAD;
	WWWDoc.safe = newdoc->safe;

	if (!HTLoadAbsolute(&WWWDoc))
	    return (NOT_FOUND);
	LYRegisterUIPage(newdoc->address, UIP_OPTIONS_MENU);
#ifdef DIRED_SUPPORT
	lynx_edit_mode = FALSE;
#endif /* DIRED_SUPPORT */
	return (NORMAL);
    }

    if (!LYIsUIPage3(HTLoadedDocumentURL(), UIP_OPTIONS_MENU, 0) &&
	!LYIsUIPage3(HTLoadedDocumentURL(), UIP_VLINKS, 0)) {
	char *buf = NULL;

	/*  We may have been spoofed? */
	HTSprintf0(&buf,
		   gettext("Use %s to invoke the Options menu!"),
		   key_for_func_ext(LYK_OPTIONS, FOR_PANEL));
	HTAlert(buf);
	FREE(buf);
	FREE(data);
	return (NOT_FOUND);
    }

    for (i = 0; data[i].tag != NULL; i++) {
	/*
	 * This isn't really for security, but rather for avoiding that the
	 * user may revisit an older instance from the history stack and submit
	 * stuff which accidentally undoes changes that had been done from a
	 * newer instance.  - kw
	 */
	if (!strcmp(data[i].tag, secure_string)) {
	    if (!secure_value || strcmp(data[i].value, secure_value)) {
		char *buf = NULL;

		/*
		 * We probably came from an older instance of the Options
		 * page that had been on the history stack. - kw
		 */
		HTSprintf0(&buf,
			   gettext("Use %s to invoke the Options menu!"),
			   key_for_func_ext(LYK_OPTIONS, FOR_PANEL));
		HTAlert(buf);
		FREE(data);
		return (NULLFILE);
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
	    validate_x_display();
	    summarize_x_display(data[i].value);
	}

	/* Editor: INPUT */
	if (!strcmp(data[i].tag, editor_string)) {
	    FREE(editor);
	    StrAllocCopy(editor, data[i].value);
	}

	/* Emacs keys: ON/OFF */
	if (!strcmp(data[i].tag, emacs_keys_string)
	    && GetOptValues(bool_values, data[i].value, &code)) {
	    if ((emacs_keys = (BOOL) code) != FALSE) {
		set_emacs_keys();
	    } else {
		reset_emacs_keys();
	    }
	}

	/* Execution links: SELECT */
#if defined(ENABLE_OPTS_CHANGE_EXEC) && (defined(EXEC_LINKS) || defined(EXEC_SCRIPTS))
	if (!strcmp(data[i].tag, exec_links_string)
	    && GetOptValues(exec_links_values, data[i].value, &code)) {
#ifndef NEVER_ALLOW_REMOTE_EXEC
	    local_exec = (code == EXEC_ALWAYS);
#endif /* !NEVER_ALLOW_REMOTE_EXEC */
	    local_exec_on_local_files = (code == EXEC_LOCAL);
	}
#endif /* ENABLE_OPTS_CHANGE_EXEC */

	/* Keypad Mode: SELECT */
	if (!strcmp(data[i].tag, keypad_mode_string)) {
	    int newval = 0;

	    if (GetOptValues(keypad_mode_values, data[i].value, &newval)
		&& keypad_mode != newval) {
		keypad_mode = newval;
		need_reload = TRUE;
		if (keypad_mode == NUMBERS_AS_ARROWS) {
		    set_numbers_as_arrows();
		} else {
		    reset_numbers_as_arrows();
		}
	    }
	}

	/* Line edit style: SELECT */
	if (!strcmp(data[i].tag, lineedit_mode_string)) {
	    int newval = atoi(data[i].value);
	    int j;

	    /* prevent spoofing attempt */
	    for (j = 0; LYLineeditNames[j]; j++) {
		if (j == newval)
		    current_lineedit = newval;
	    }
	}
#ifdef EXP_KEYBOARD_LAYOUT
	/* Keyboard layout: SELECT */
	if (!strcmp(data[i].tag, kblayout_string)) {
	    int newval = atoi(data[i].value);
	    int j;

	    /* prevent spoofing attempt */
	    for (j = 0; LYKbLayoutNames[j]; j++) {
		if (j == newval)
		    current_layout = newval;
	    }
	}
#endif /* EXP_KEYBOARD_LAYOUT */

	/* Mail Address: INPUT */
	if (!strcmp(data[i].tag, mail_address_string)) {
	    FREE(personal_mail_address);
	    StrAllocCopy(personal_mail_address, data[i].value);
	}

	/* Anonymous FTP Password: INPUT */
	if (!strcmp(data[i].tag, anonftp_password_string)) {
	    FREE(anonftp_password);
	    StrAllocCopy(anonftp_password, data[i].value);
	}

	/* Search Type: SELECT */
	if (!strcmp(data[i].tag, search_type_string)
	    && GetOptValues(search_type_values, data[i].value, &code)) {
	    case_sensitive = (BOOL) code;
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
	    LYSelectPopups = (BOOL) code;
	}
#if defined(USE_SLANG) || defined(COLOR_CURSES)
	/* Show Color: SELECT */
	if (!strcmp(data[i].tag, show_color_string)
	    && GetOptValues(show_color_values, data[i].value,
			    &LYChosenShowColor)) {
	    if (can_do_colors)
		LYShowColor = LYChosenShowColor;
	    if (CurrentShowColor != LYShowColor) {
		lynx_force_repaint();
	    }
	    CurrentShowColor = LYShowColor;
#ifdef USE_SLANG
	    SLtt_Use_Ansi_Colors = (LYShowColor > SHOW_COLOR_OFF ? TRUE : FALSE);
#endif
	}
#endif /* USE_SLANG || COLOR_CURSES */

	/* Show Cursor: ON/OFF */
	if (!strcmp(data[i].tag, show_cursor_string)
	    && GetOptValues(bool_values, data[i].value, &code)) {
	    LYShowCursor = (BOOL) code;
	}

	/* Underline links: ON/OFF */
	if (!strcmp(data[i].tag, underline_links_string)
	    && GetOptValues(bool_values, data[i].value, &code)) {
	    LYUnderlineLinks = (BOOL) code;
	}
#ifdef USE_SCROLLBAR
	/* Show Scrollbar: ON/OFF */
	if (!strcmp(data[i].tag, show_scrollbar_string)
	    && GetOptValues(bool_values, data[i].value, &code)) {
	    LYShowScrollbar = (BOOL) code;
	    need_reload = TRUE;
	}
#endif

	/* Cookie Prompting: SELECT */
	if (!strcmp(data[i].tag, cookie_prompt_string))
	    GetOptValues(prompt_values, data[i].value, &cookie_noprompt);

#ifdef USE_SSL
	/* SSL Prompting: SELECT */
	if (!strcmp(data[i].tag, ssl_prompt_string))
	    GetOptValues(prompt_values, data[i].value, &ssl_noprompt);
#endif

	/* User Mode: SELECT */
	if (!strcmp(data[i].tag, user_mode_string)
	    && GetOptValues(user_mode_values, data[i].value, &user_mode)) {
	    if (user_mode == NOVICE_MODE) {
		display_lines = (LYlines - 4);
	    } else {
		display_lines = LYlines - 2;
	    }
	}

	/* Type of visited pages page: SELECT */
	if (!strcmp(data[i].tag, visited_links_string))
	    GetOptValues(visited_links_values, data[i].value, &Visited_Links_As);

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
		verbose_img = (BOOL) code;
		need_reload = TRUE;
	    }
	}

	/* VI Keys: ON/OFF */
	if (!strcmp(data[i].tag, vi_keys_string)
	    && GetOptValues(bool_values, data[i].value, &code)) {
	    if ((vi_keys = (BOOL) code) != FALSE) {
		set_vi_keys();
	    } else {
		reset_vi_keys();
	    }
	}

	/* Bookmarks File Menu: SELECT */
	if (!strcmp(data[i].tag, mbm_string) && (!LYMBMBlocked)) {
	    GetOptValues(mbm_values, data[i].value, &LYMultiBookmarks);
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
#ifdef EXP_LOCALE_CHARSET
	/* Use locale-based character set: ON/OFF */
	if (!strcmp(data[i].tag, locale_charset_string)
	    && GetOptValues(bool_values, data[i].value, &code)) {
	    LYLocaleCharset = (BOOL) code;
	}
#endif

	/* Display Character Set: SELECT */
	if (!strcmp(data[i].tag, display_char_set_string)) {
	    int newval = atoi(data[i].value);
	    int j;

	    /* prevent spoofing attempt */
	    for (j = 0; LYchar_set_names[j]; j++) {
		if (j == newval)
		    current_char_set = newval;
	    }
	}

	/* Raw Mode: ON/OFF */
	if (!strcmp(data[i].tag, raw_mode_string)
	    && GetOptValues(bool_values, data[i].value, &code)) {
	    LYRawMode = (BOOL) code;
	}

	/*
	 * ftp sort: SELECT
	 */
	if (!strcmp(data[i].tag, ftp_sort_string)) {
	    GetOptValues(ftp_sort_values, data[i].value, &HTfileSortMethod);
	}
#ifdef DIRED_SUPPORT
	/* Local Directory Style: SELECT */
	if (!strcmp(data[i].tag, dired_list_string)) {
	    GetOptValues(dired_list_values, data[i].value, &dir_list_style);
	}
#ifdef LONG_LIST
	/* Local Directory Order: SELECT */
	if (!strcmp(data[i].tag, dired_sort_string)) {
	    GetOptValues(dired_sort_values, data[i].value, &dir_list_order);
	}
#endif /* LONG_LIST */
#endif /* DIRED_SUPPORT */

	/* Show dot files: ON/OFF */
	if (!strcmp(data[i].tag, show_dotfiles_string) && (!no_dotfiles)
	    && GetOptValues(bool_values, data[i].value, &code)) {
	    show_dotfiles = (BOOL) code;
	}
#ifdef USE_READPROGRESS
	/* Show Transfer Rate: enumerated value */
	if (!strcmp(data[i].tag, show_rate_string)
	    && GetOptValues(rate_values, data[i].value, &code)) {
	    LYTransferRate = code;
	}
#endif /* USE_READPROGRESS */

	/* Preferred Media Type: SELECT */
	if (!strcmp(data[i].tag, preferred_media_string)) {
	    GetOptValues(media_values, data[i].value, &LYAcceptMedia);
	}

	/* Preferred Encoding: SELECT */
	if (!strcmp(data[i].tag, preferred_encoding_string)) {
	    GetOptValues(encoding_values, data[i].value, &LYAcceptEncoding);
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
		if (!LYCheckUserAgent()) {
		    HTAlert(UA_PLEASE_USE_LYNX);
		}
	    }
	}
    }				/* end of loop */

    /*
     * Process the flags:
     */
#ifdef EXP_LOCALE_CHARSET
    LYFindLocaleCharset();
#endif

    if (old_media_value != LYAcceptMedia)
	HTFilterPresentations();

    if (display_char_set_old != current_char_set ||
	raw_mode_old != LYRawMode ||
	assume_char_set_changed) {
	/*
	 * charset settings: the order is essential here.
	 */
	if (display_char_set_old != current_char_set) {
	    /*
	     * Set the LYUseDefaultRawMode value and character handling if
	     * LYRawMode was changed.  - FM
	     */
	    LYUseDefaultRawMode = TRUE;
	    HTMLUseCharacterSet(current_char_set);
#ifdef CAN_SWITCH_DISPLAY_CHARSET
	    /* Deduce whether the user wants autoswitch: */
	    switch_display_charsets =
		(current_char_set == auto_display_charset
		 || current_char_set == auto_other_display_charset);
#endif
	}
	if (assume_char_set_changed && HTCJK != JAPANESE) {
	    LYRawMode = (BOOL) (UCLYhndl_for_unspec == current_char_set);
	}
	if (raw_mode_old != LYRawMode || assume_char_set_changed) {
	    /*
	     * Set the raw 8-bit or CJK mode defaults and character set if
	     * changed.  - FM
	     */
	    HTMLSetUseDefaultRawMode(current_char_set, LYRawMode);
	    HTMLSetCharacterHandling(current_char_set);
	}
	need_reload = TRUE;
    }
    /* end of charset settings */
    /*
     * FIXME: Golly gee, we need to write all of this out now, don't we?
     */
    BStrFree(newdoc->post_data);
    FREE(data);
    if (save_all) {
	HTInfoMsg(SAVING_OPTIONS);
	LYrcShowColor = LYChosenShowColor;
	if (save_rc(NULL)) {
	    HTInfoMsg(OPTIONS_SAVED);
	} else {
	    HTAlert(OPTIONS_NOT_SAVED);
	}
    }

    /*
     * Exit:  working around the previous document.  Being out of
     * mainloop()/getfile() cycle, do things manually.
     */
    CTRACE((tfp, "\nLYOptions.c/postoptions(): exiting...\n"));
    CTRACE((tfp, "                            need_reload = %s\n",
	    need_reload ? "TRUE" : "FALSE"));
    CTRACE((tfp, "                            need_end_reload = %s\n",
	    need_end_reload ? "TRUE" : "FALSE"));

    /*  Options menu was pushed before postoptions(), so pop-up. */
    LYpop(newdoc);
    WWWDoc.address = newdoc->address;
    WWWDoc.post_data = newdoc->post_data;
    WWWDoc.post_content_type = newdoc->post_content_type;
    WWWDoc.bookmark = newdoc->bookmark;
    WWWDoc.isHEAD = newdoc->isHEAD;
    WWWDoc.safe = newdoc->safe;
    LYforce_no_cache = FALSE;	/* ! */
    LYoverride_no_cache = TRUE;	/* ! */
    /*
     * Working out of getfile() cycle we reset *no_cache manually here so
     * HTLoadAbsolute() will return "Document already in memory":  it was
     * forced reloading Options Menu again without this (overhead).
     *
     * Probably *no_cache was set in a wrong position because of
     * the internal page...
     */
    if (!HTLoadAbsolute(&WWWDoc))
	return (NOT_FOUND);

    HTuncache_current_document();	/* will never use again */

    /*
     * Return to previous doc, not to options menu!  Reload the document we had
     * before the options menu but uncache only when necessary (Hurrah, user!):
     */
    LYpop(newdoc);
    WWWDoc.address = newdoc->address;
    WWWDoc.post_data = newdoc->post_data;
    WWWDoc.post_content_type = newdoc->post_content_type;
    WWWDoc.bookmark = newdoc->bookmark;
    WWWDoc.isHEAD = newdoc->isHEAD;
    WWWDoc.safe = newdoc->safe;
    LYforce_no_cache = FALSE;	/* see below */
    LYoverride_no_cache = TRUE;	/* see below */
    /*
     * Re-setting of *no_cache is probably not required here but this is a
     * guarantee against _double_ reloading over the net in case prev document
     * has its own "no cache" attribute and options menu set "need_reload"
     * also.  Force this HTLoadAbsolute() to return "Document already in
     * memory".
     */
    if (!HTLoadAbsolute(&WWWDoc))
	return (NOT_FOUND);

    /*
     * Now most interesting part: reload document when necessary.
     * **********************************************************
     */

    reloading = FALSE;		/* set manually */
    /* force end-to-end reload from remote server if change LYUserAgent or
     * language or pref_charset (marked by need_end_reload flag above), from
     * old-style LYK_OPTIONS (mainloop):
     */
    if ((need_end_reload == TRUE &&
	 (strncmp(newdoc->address, "http", 4) == 0 ||
	  isLYNXCGI(newdoc->address) == 0))) {
	/*
	 * An option has changed which may influence content negotiation, and
	 * the resource is from a http or https or lynxcgi URL (the only
	 * protocols which currently do anything with this information).  Set
	 * reloading = TRUE so that proxy caches will be flushed, which is
	 * necessary until the time when all proxies understand HTTP 1.1 Vary: 
	 * and all Servers properly use it...  Treat like case LYK_RELOAD (see
	 * comments there).  - KW
	 */
	reloading = TRUE;	/* global flag */
	need_reload = TRUE;	/* this was probably already TRUE, don't worry */
    }

    if (need_reload == FALSE) {
	/*  no uncache, already loaded */
	CTRACE((tfp, "LYOptions.c/postoptions(): now really exit.\n\n"));
	return (NORMAL);
    } else {
	/*  update HText cache */

	/*
	 * see LYK_RELOAD & LYK_OPTIONS in mainloop for details...
	 */
	if (HTisDocumentSource()) {
	    srcmode_for_next_retrieval(1);
	}
#ifdef USE_SOURCE_CACHE
	if (reloading == FALSE) {
	    /* one more attempt to be smart enough: */
	    if (HTcan_reparse_document()) {
		if (!HTreparse_document())
		    srcmode_for_next_retrieval(0);
		CTRACE((tfp, "LYOptions.c/postoptions(): now really exit.\n\n"));
		return (NORMAL);
	    }
	}
#endif
	if (newdoc->post_data != NULL && !newdoc->safe &&
	    confirm_post_resub(newdoc->address, newdoc->title, 2, 1) == FALSE) {
	    HTInfoMsg(WILL_NOT_RELOAD_DOC);
	    if (HTisDocumentSource()) {
		srcmode_for_next_retrieval(0);
	    }
	    return (NORMAL);
	}

	HEAD_request = HTLoadedDocumentIsHEAD();
	/*  uncache and load again */
	HTuncache_current_document();
	LYpush(newdoc, FALSE);
	CTRACE((tfp, "LYOptions.c/postoptions(): now really exit.\n\n"));
	return (NULLFILE);
    }

    /******** Done! **************************************************/
}

static char *NewSecureValue(void)
{
    static char oops[] = "?";

    FREE(secure_value);
    if ((secure_value = typeMallocn(char, 80)) != 0) {
#if defined(RAND_MAX)
	long key = lynx_rand();

#else
	long key = (long) secure_value + (long) time(0);
#endif
	sprintf(secure_value, "%ld", key);
	return secure_value;
    }
    return oops;
}

#define LABEL_LEN 33

/*
 * Note: the 'value' we are passing here is a local copy of the "same" string
 * as is used in LYrcFile.c to index the savable options.
 */
static void PutLabel(FILE *fp, const char *name,
		     const char *value)
{
    int have = strlen(name);
    int want = LABEL_LEN;
    int need = LYstrExtent(name, have, want);

    fprintf(fp, "%s%s", MARGIN_STR, name);

    if (will_save_rc(value) && !no_option_save) {
	while (need++ < want)
	    fprintf(fp, "&nbsp;");
    } else {
	want -= 3;
	if (need < want) {
	    fprintf(fp, "&nbsp;");
	    ++need;
	}
	fprintf(fp, "(!)");
	while (need++ < want) {
	    fprintf(fp, "&nbsp;");
	}
    }
    fprintf(fp, ": ");
}

/*
 * For given a list of the .lynxrc names for boolean flags that make up a
 * composite setting, check if any are not writable for the .lynxrc file.  If
 * so, return that name, so the subsequence will_save_rc() check in PutLabel()
 * will flag the composite as not-saved.
 */
static const char *check_if_write_lynxrc(const char **table)
{
    int n;
    const char *result = NULL;

    for (n = 0; table[n] != 0; ++n) {
	result = table[n];
	if (!will_save_rc(result))
	    break;
    }
    return result;
}

/*
 * The options menu treats "Cookies" as a single enumeration, but it is read
 * from lynx.cfg (and perhaps .lynxrc) as a set of booleans.  Check if any are
 * not writable to .lynxrc, so we can show the user. 
 */
static const char *will_save_cookies(void)
{
    static const char *table[] =
    {
	RC_SET_COOKIES,		/* LYSetCookies */
	RC_ACCEPT_ALL_COOKIES,	/* LYAcceptAllCookies */
	NULL
    };

    return check_if_write_lynxrc(table);
}

/*
 * The options menu treats "Show images" as a single enumeration, but it is
 * read from lynx.cfg (and perhaps .lynxrc) as a set of booleans.  Check if any
 * are not writable to .lynxrc, so we can show the user. 
 */
static const char *will_save_images(void)
{
    static const char *table[] =
    {
	RC_MAKE_PSEUDO_ALTS_FOR_INLINES,	/* pseudo_inline_alts */
	RC_MAKE_LINKS_FOR_ALL_IMAGES,	/* clickable_images */
	NULL
    };

    return check_if_write_lynxrc(table);
}

/*
 * The visited-links menu is used from the visited-links page as well as the
 * options page.
 */
void LYMenuVisitedLinks(FILE *fp0, int disable_all)
{
    BeginSelect(fp0, visited_links_string);
    PutOptValues(fp0, Visited_Links_As, visited_links_values);
    EndSelect(fp0);
}

/*
 * Okay, someone wants to change options.  So, let's gen up a form for them
 * and pass it around.  Gor, this is ugly.  Be a lot easier in Bourne with
 * "here" documents.  :->
 * Basic Strategy:  For each option, throw up the appropriate type of
 * control, giving defaults as appropriate.  If nothing else, we're
 * probably going to test every control there is.  MRC
 *
 * This function is synchronized with postoptions().  Read the comments in
 * postoptions() header if you change something in gen_options().
 */
static int gen_options(char **newfile)
{
    int i;
    static char tempfile[LY_MAXPATH] = "\0";
    BOOLEAN disable_all = FALSE;
    FILE *fp0;
    size_t cset_len = 0;
    size_t text_len = ((LYcolLimit > 45)
		       ? LYcolLimit - (LABEL_LEN + 2 + MARGIN_LEN)
		       : 7);	/* cf: PutLabel */

    if ((fp0 = InternalPageFP(tempfile, TRUE)) == 0)
	return (NOT_FOUND);

    LYLocalFileToURL(newfile, tempfile);

    /* This should not be needed if we regenerate the temp file every time with
     * a new name, which just happened above in the case
     * LYReuseTempfiles==FALSE.  Even for LYReuseTempfiles=TRUE, code at the
     * end of postoptions() may remove an older cached version from memory if
     * that version of the page was left by submitting changes.  - kw
     * 1999-11-27
     * If access to the actual file via getfile() later fails (maybe because of
     * some restrictions), mainloop may leave this flag on after popping the
     * previous doc which is then unnecessarily reloaded.  But I changed
     * mainloop to reset the flag.  - kw 1999-05-24
     */
    LYforce_no_cache = TRUE;

    /*
     * Without LYUseFormsOptions set we should maybe not even get here.
     * However, it's possible we do; disable the form in that case. - kw
     */
#ifndef NO_OPTION_MENU
    if (!LYUseFormsOptions)
	disable_all = TRUE;
#endif

    BeginInternalPage(fp0, OPTIONS_TITLE, NULL);	/* help link below */

    /*
     * I do C, not HTML.  Feel free to pretty this up.
     */
    fprintf(fp0, "<form action=\"%s\" method=\"post\">\n", STR_LYNXOPTIONS);
    /*
     * use following with some sort of one shot secret key akin to NCSA
     * (or was it CUTE?) telnet one shot password to allow ftp to self.
     * to prevent spoofing.
     */
    fprintf(fp0, "<input name=\"%s\" type=\"hidden\" value=\"%s\">\n",
	    secure_string, NewSecureValue());

    /*
     * visible text begins here
     */

    /* Submit/Reset/Help */
    fprintf(fp0, "<p align=center>\n");
    if (!disable_all) {
	fprintf(fp0, "<input type=\"submit\" value=\"%s\"> - \n", ACCEPT_CHANGES);
	fprintf(fp0, "<input type=\"reset\" value=\"%s\"> - \n", RESET_CHANGES);
	fprintf(fp0, "%s - \n", CANCEL_CHANGES);
    }
    fprintf(fp0, "<a href=\"%s%s\">%s</a>\n",
	    helpfilepath, OPTIONS_HELP, TO_HELP);

    /* Save options */
    if (!no_option_save) {
	if (!disable_all) {
	    fprintf(fp0, "<p align=center>%s: ", SAVE_OPTIONS);
	    fprintf(fp0, "<input type=\"checkbox\" name=\"%s\">\n",
		    save_options_string);
	}
	fprintf(fp0, "<br>%s\n",
		gettext("(options marked with (!) will not be saved)"));
    }

    /*
     * preformatted text follows
     */
    fprintf(fp0, "<pre>\n");

    PutHeader(fp0, gettext("General Preferences"));
    /*****************************************************************/

    /* User Mode: SELECT */
    PutLabel(fp0, gettext("User mode"), user_mode_string);
    BeginSelect(fp0, user_mode_string);
    PutOptValues(fp0, user_mode, user_mode_values);
    EndSelect(fp0);

    /* Editor: INPUT */
    PutLabel(fp0, gettext("Editor"), editor_string);
    PutTextInput(fp0, editor_string, NonNull(editor), text_len,
		 DISABLED(no_editor || system_editor));

    /* Search Type: SELECT */
    PutLabel(fp0, gettext("Type of Search"), search_type_string);
    BeginSelect(fp0, search_type_string);
    PutOptValues(fp0, case_sensitive, search_type_values);
    EndSelect(fp0);

    PutHeader(fp0, gettext("Security and Privacy"));
    /*****************************************************************/

    /* Cookies: SELECT */
    PutLabel(fp0, gettext("Cookies"), will_save_cookies());
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

    /* Cookie Prompting: SELECT */
    PutLabel(fp0, gettext("Invalid-Cookie Prompting"), cookie_prompt_string);
    BeginSelect(fp0, cookie_prompt_string);
    PutOptValues(fp0, cookie_noprompt, prompt_values);
    EndSelect(fp0);

#ifdef USE_SSL
    /* SSL Prompting: SELECT */
    PutLabel(fp0, gettext("SSL Prompting"), ssl_prompt_string);
    BeginSelect(fp0, ssl_prompt_string);
    PutOptValues(fp0, ssl_noprompt, prompt_values);
    EndSelect(fp0);
#endif

    PutHeader(fp0, gettext("Keyboard Input"));
    /*****************************************************************/

    /* Keypad Mode: SELECT */
    PutLabel(fp0, gettext("Keypad mode"), keypad_mode_string);
    BeginSelect(fp0, keypad_mode_string);
    PutOptValues(fp0, keypad_mode, keypad_mode_values);
    EndSelect(fp0);

    /* Emacs keys: ON/OFF */
    PutLabel(fp0, gettext("Emacs keys"), emacs_keys_string);
    BeginSelect(fp0, emacs_keys_string);
    PutOptValues(fp0, emacs_keys, bool_values);
    EndSelect(fp0);

    /* VI Keys: ON/OFF */
    PutLabel(fp0, gettext("VI keys"), vi_keys_string);
    BeginSelect(fp0, vi_keys_string);
    PutOptValues(fp0, vi_keys, bool_values);
    EndSelect(fp0);

    /* Line edit style: SELECT */
    if (LYLineeditNames[1]) {	/* well, at least 2 line edit styles available */
	PutLabel(fp0, gettext("Line edit style"), lineedit_mode_string);
	BeginSelect(fp0, lineedit_mode_string);
	for (i = 0; LYLineeditNames[i]; i++) {
	    char temp[16];

	    sprintf(temp, "%d", i);
	    PutOption(fp0, i == current_lineedit, temp, LYLineeditNames[i]);
	}
	EndSelect(fp0);
    }
#ifdef EXP_KEYBOARD_LAYOUT
    /* Keyboard layout: SELECT */
    PutLabel(fp0, gettext("Keyboard layout"), kblayout_string);
    BeginSelect(fp0, kblayout_string);
    for (i = 0; LYKbLayoutNames[i]; i++) {
	char temp[16];

	sprintf(temp, "%d", i);
	PutOption(fp0, i == current_layout, temp, LYKbLayoutNames[i]);
    }
    EndSelect(fp0);
#endif /* EXP_KEYBOARD_LAYOUT */

    /*
     * Display and Character Set
     */
    PutHeader(fp0, gettext("Display and Character Set"));
    /*****************************************************************/

#ifdef EXP_LOCALE_CHARSET
    /* Use locale-based character set: ON/OFF */
    PutLabel(fp0, gettext("Use locale-based character set"), locale_charset_string);
    BeginSelect(fp0, locale_charset_string);
    PutOptValues(fp0, LYLocaleCharset, bool_values);
    EndSelect(fp0);
#else
#define LYLocaleCharset FALSE
#endif

    /* Display Character Set: SELECT */
    PutLabel(fp0, gettext("Display character set"), display_char_set_string);
    MaybeSelect(fp0, LYLocaleCharset, display_char_set_string);
    for (i = 0; LYchar_set_names[i]; i++) {
	char temp[10];
	size_t len = strlen(LYchar_set_names[i]);

	if (len > cset_len)
	    cset_len = len;
	sprintf(temp, "%d", i);
#ifdef EXP_CHARSET_CHOICE
	if (!charset_subsets[i].hide_display)
#endif
	    PutOption(fp0, i == current_char_set, temp, LYchar_set_names[i]);
    }
    EndSelect(fp0);

    /* Assume Character Set: SELECT */
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
	PutLabel(fp0, gettext("Assumed document character set"), assume_char_set_string);
	BeginSelect(fp0, assume_char_set_string);
	for (i = 0; i < LYNumCharsets; i++) {
#ifdef EXP_CHARSET_CHOICE
	    if (!charset_subsets[i].hide_assumed)
#endif
		PutOption(fp0, i == curval,
			  LYCharSet_UC[i].MIMEname,
			  LYCharSet_UC[i].MIMEname);
	}
	EndSelect(fp0);
    }

    /* Raw Mode: ON/OFF */
    if (LYHaveCJKCharacterSet) {
	/*
	 * Since CJK people hardly mixed with other world
	 * we split the header to make it more readable:
	 * "CJK mode" for CJK display charsets, and "Raw 8-bit" for others.
	 */
	PutLabel(fp0, gettext("CJK mode"), raw_mode_string);
    } else {
	PutLabel(fp0, gettext("Raw 8-bit"), raw_mode_string);
    }

    BeginSelect(fp0, raw_mode_string);
    PutOptValues(fp0, LYRawMode, bool_values);
    EndSelect(fp0);

    /* X Display: INPUT */
    PutLabel(fp0, gettext("X Display"), x_display_string);
    PutTextInput(fp0, x_display_string, NonNull(x_display), text_len, "");

    /*
     * Document Appearance
     */
    PutHeader(fp0, gettext("Document Appearance"));
    /*****************************************************************/

    /* Show Color: SELECT */
#if defined(USE_SLANG) || defined(COLOR_CURSES)
    SetupChosenShowColor();
    PutLabel(fp0, gettext("Show color"), show_color_string);
    if (no_option_save) {
	MaybeSelect(fp0, !can_do_colors, show_color_string);
	if (LYShowColor == SHOW_COLOR_NEVER) {
	    LYShowColor = SHOW_COLOR_OFF;
	} else if (LYShowColor == SHOW_COLOR_ALWAYS) {
	    LYShowColor = SHOW_COLOR_ON;
	}
	PutOptValues(fp0, LYShowColor - SHOW_COLOR_OFF, bool_values);
    } else {
	BeginSelect(fp0, show_color_string);
	if (can_do_colors) {
	    show_color_values[2].HtmlName = on_string;
	    show_color_values[3].LongName = always_string;
	} else {
	    show_color_values[2].HtmlName = NULL;	/* suppress "ON" - kw */
	    show_color_values[3].LongName = "Always try";
	}
	PutOptValues(fp0, LYChosenShowColor, show_color_values);
    }
    EndSelect(fp0);
#endif /* USE_SLANG || COLOR_CURSES */

    /* Show cursor: ON/OFF */
    PutLabel(fp0, gettext("Show cursor"), show_cursor_string);
    BeginSelect(fp0, show_cursor_string);
    PutOptValues(fp0, LYShowCursor, bool_values);
    EndSelect(fp0);

    /* Underline links: ON/OFF */
    PutLabel(fp0, gettext("Underline links"), underline_links_string);
    BeginSelect(fp0, underline_links_string);
    PutOptValues(fp0, LYUnderlineLinks, bool_values);
    EndSelect(fp0);

#ifdef USE_SCROLLBAR
    /* Show scrollbar: ON/OFF */
    PutLabel(fp0, gettext("Show scrollbar"), show_scrollbar_string);
    BeginSelect(fp0, show_scrollbar_string);
    PutOptValues(fp0, LYShowScrollbar, bool_values);
    EndSelect(fp0);
#endif

    /* Select Popups: ON/OFF */
    PutLabel(fp0, gettext("Popups for select fields"), select_popups_string);
    BeginSelect(fp0, select_popups_string);
    PutOptValues(fp0, LYSelectPopups, bool_values);
    EndSelect(fp0);

    /* HTML error recovery: SELECT */
    PutLabel(fp0, gettext("HTML error recovery"), DTD_recovery_string);
    BeginSelect(fp0, DTD_recovery_string);
    PutOptValues(fp0, Old_DTD, DTD_type_values);
    EndSelect(fp0);

    /* Show Images: SELECT */
    PutLabel(fp0, gettext("Show images"), will_save_images());
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
    PutLabel(fp0, gettext("Verbose images"), verbose_images_string);
    BeginSelect(fp0, verbose_images_string);
    PutOptValues(fp0, verbose_img, verbose_images_type_values);
    EndSelect(fp0);

    /*
     * Headers Transferred to Remote Servers
     */
    PutHeader(fp0, gettext("Headers Transferred to Remote Servers"));
    /*****************************************************************/

    /* Mail Address: INPUT */
    PutLabel(fp0, gettext("Personal mail address"), mail_address_string);
    PutTextInput(fp0, mail_address_string,
		 NonNull(personal_mail_address), text_len, "");

    /* Mail Address: INPUT */
    PutLabel(fp0, gettext("Password for anonymous ftp"), mail_address_string);
    PutTextInput(fp0, anonftp_password_string,
		 NonNull(anonftp_password), text_len, "");

    /* Preferred media type: SELECT */
    PutLabel(fp0, gettext("Preferred media type"), preferred_media_string);
    BeginSelect(fp0, preferred_media_string);
    PutOptValues(fp0, LYAcceptMedia, media_values);
    EndSelect(fp0);

    /* Preferred encoding: SELECT */
    PutLabel(fp0, gettext("Preferred encoding"), preferred_encoding_string);
    BeginSelect(fp0, preferred_encoding_string);
    PutOptValues(fp0, LYAcceptEncoding, encoding_values);
    EndSelect(fp0);

    /* Preferred Document Character Set: INPUT */
    PutLabel(fp0, gettext("Preferred document character set"), preferred_doc_char_string);
    PutTextInput(fp0, preferred_doc_char_string,
		 NonNull(pref_charset), cset_len + 2, "");

    /* Preferred Document Language: INPUT */
    PutLabel(fp0, gettext("Preferred document language"), preferred_doc_lang_string);
    PutTextInput(fp0, preferred_doc_lang_string,
		 NonNull(language), cset_len + 2, "");

    /* User Agent: INPUT */
    if (!no_useragent) {
	PutLabel(fp0, gettext("User-Agent header"), user_agent_string);
	PutTextInput(fp0, user_agent_string,
		     NonNull(LYUserAgent), text_len, "");
    }

    /*
     * Listing and Accessing Files
     */
    PutHeader(fp0, gettext("Listing and Accessing Files"));
    /*****************************************************************/

    /* FTP sort: SELECT */
    PutLabel(fp0, gettext("FTP sort criteria"), ftp_sort_string);
    BeginSelect(fp0, ftp_sort_string);
    PutOptValues(fp0, HTfileSortMethod, ftp_sort_values);
    EndSelect(fp0);

#ifdef DIRED_SUPPORT
    /* Local Directory Sort: SELECT */
    PutLabel(fp0, gettext("Local directory sort criteria"), dired_list_string);
    BeginSelect(fp0, dired_list_string);
    PutOptValues(fp0, dir_list_style, dired_list_values);
    EndSelect(fp0);
#ifdef LONG_LIST
    /* Local Directory Order: SELECT */
    PutLabel(fp0, gettext("Local directory sort order"), dired_sort_string);
    BeginSelect(fp0, dired_sort_string);
    PutOptValues(fp0, dir_list_order, dired_sort_values);
    EndSelect(fp0);
#endif /* LONG_LIST */
#endif /* DIRED_SUPPORT */

    /* Show dot files: ON/OFF */
    if (!no_dotfiles) {
	PutLabel(fp0, gettext("Show dot files"), show_dotfiles_string);
	BeginSelect(fp0, show_dotfiles_string);
	PutOptValues(fp0, show_dotfiles, bool_values);
	EndSelect(fp0);
    }

    /* Execution links: SELECT */
#if defined(ENABLE_OPTS_CHANGE_EXEC) && (defined(EXEC_LINKS) || defined(EXEC_SCRIPTS))
    PutLabel(fp0, gettext("Execution links"), exec_links_string);
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
#endif /* ENABLE_OPTS_CHANGE_EXEC */

#ifdef USE_READPROGRESS
    /* Show transfer rate: SELECT */
    PutLabel(fp0, gettext("Show transfer rate"), show_rate_string);
    BeginSelect(fp0, show_rate_string);
    for (i = 0; rate_values[i].LongName != 0; ++i) {
	char *message = NULL;

	HTSprintf0(&message,
		   rate_values[i].LongName,
		   HTProgressUnits(rate_values[i].value));
	PutOption(fp0,
		  LYTransferRate == rate_values[i].value,
		  rate_values[i].HtmlName,
		  message);
	FREE(message);
    }
    EndSelect(fp0);
#endif /* USE_READPROGRESS */

    /*
     * Special Files and Screens
     */
    PutHeader(fp0, gettext("Special Files and Screens"));
    /*****************************************************************/

    /* Multi-Bookmark Mode: SELECT */
    if (!LYMBMBlocked) {
	PutLabel(fp0, gettext("Multi-bookmarks"), mbm_string);
	BeginSelect(fp0, mbm_string);
	PutOptValues(fp0, LYMultiBookmarks, mbm_values);
	EndSelect(fp0);
    }

    /* Bookmarks File Menu: LINK/INPUT */
    if (LYMultiBookmarks) {
	PutLabel(fp0, gettext("Review/edit Bookmarks files"), mbm_string);
	fprintf(fp0, "<a href=\"%s\">%s</a>\n",
		LYNXOPTIONS_PAGE(MBM_LINK), gettext("Goto multi-bookmark menu"));
    } else {
	PutLabel(fp0, gettext("Bookmarks file"), single_bookmark_string);
	PutTextInput(fp0, single_bookmark_string,
		     NonNull(bookmark_page), text_len, "");
    }

    /* Visited Pages: SELECT */
    PutLabel(fp0, gettext("Visited Pages"), visited_links_string);
    LYMenuVisitedLinks(fp0, disable_all);

    if (!no_lynxcfg_info) {
	fprintf(fp0, "\n  %s<a href=\"%s\">lynx.cfg</a>.\n",
		gettext("View the file "),
		STR_LYNXCFG);
    }

    fprintf(fp0, "\n</pre>\n");

    /* Submit/Reset */
    if (!disable_all) {
	fprintf(fp0, "<p align=center>\n");
	fprintf(fp0, "<input type=\"submit\" value=\"%s\"> - \n", ACCEPT_CHANGES);
	fprintf(fp0, "<input type=\"reset\" value=\"%s\"> - \n", RESET_CHANGES);
	fprintf(fp0, "%s\n", CANCEL_CHANGES);
    }

    /*
     * close HTML
     */
    fprintf(fp0, "</form>\n");
    EndInternalPage(fp0);

    LYCloseTempFP(fp0);
    return (NORMAL);
}
#endif /* !NO_OPTION_FORMS */
