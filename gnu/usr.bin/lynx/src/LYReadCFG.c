#ifndef NO_RULES
#include <HTRules.h>
#else
#include <HTUtils.h>
#endif
#include <HTTP.h>  /* 'reloading' flag */
#include <HTFile.h>
#include <HTInit.h>
#include <UCMap.h>

#include <LYUtils.h>
#include <GridText.h>
#include <LYStrings.h>
#include <LYStructs.h>
#include <LYGlobalDefs.h>
#include <LYCharSets.h>
#include <LYCharUtils.h>
#include <LYKeymap.h>
#include <LYJump.h>
#include <LYGetFile.h>
#include <LYCgi.h>
#include <LYCurses.h>
#include <LYBookmark.h>
#include <LYCookie.h>
#include <LYReadCFG.h>
#include <HTAlert.h>
#include <LYHistory.h>
#include <LYPrettySrc.h>
#include <LYrcFile.h>

#ifdef DIRED_SUPPORT
#include <LYLocal.h>
#endif /* DIRED_SUPPORT */

#include <LYexit.h>
#include <LYLeaks.h>

#ifndef DISABLE_NEWS
#include <HTNews.h>
#endif

PUBLIC BOOLEAN have_read_cfg = FALSE;
PUBLIC BOOLEAN LYUseNoviceLineTwo = TRUE;

/*
 *  Translate a TRUE/FALSE field in a string buffer.
 */
PRIVATE BOOL is_true ARGS1(
	char *, string)
{
    if (!strncasecomp(string,"TRUE",4))
	return(TRUE);
    else
	return(FALSE);
}

/*
 *  Find an unescaped colon in a string buffer.
 */
PRIVATE char *find_colon ARGS1(
	char *, buffer)
{
    char ch, *buf = buffer;

    if (buf == NULL)
	return NULL;

    while ((ch = *buf) != 0) {
	if (ch == ':')
	    return buf;
	if (ch == '\\') {
	    buf++;
	    if (*buf == 0)
		break;
	}
	buf++;
    }
    return NULL;
}

PRIVATE void free_item_list ARGS1(
    lynx_list_item_type **,	ptr)
{
    lynx_list_item_type *cur = *ptr;
    lynx_list_item_type *next;

    while (cur) {
	next = cur->next;
	FREE(cur->name);
	FREE(cur->command);
	FREE(cur);
	cur = next;
    }
    *ptr = NULL;
}

/*
 *  Function for freeing the DOWNLOADER and UPLOADER menus list. - FM
 */
PRIVATE void free_all_item_lists NOARGS
{
    free_item_list(&printers);
    free_item_list(&downloaders);
#ifdef DIRED_SUPPORT
    free_item_list(&uploaders);
#endif /* DIRED_SUPPORT */

#ifdef USE_EXTERNALS
    free_item_list(&externals);
#endif /* USE_EXTERNALS */

    return;
}

/*
 *  Process string buffer fields for DOWNLOADER or UPLOADER menus.
 */
PRIVATE void add_item_to_list ARGS3(
	char *,			buffer,
	lynx_list_item_type **, list_ptr,
	int,			special)
{
    char *colon, *next_colon;
    lynx_list_item_type *cur_item, *prev_item;

    /*
     *	Make a linked list
     */
    if (*list_ptr == NULL) {
	/*
	 *  First item.
	 */
	cur_item = typecalloc(lynx_list_item_type);
	if (cur_item == NULL)
	    outofmem(__FILE__, "read_cfg");
	*list_ptr = cur_item;
#ifdef LY_FIND_LEAKS
	atexit(free_all_item_lists);
#endif
    } else {
	/*
	 *  Find the last item.
	 */
	for (prev_item = *list_ptr;
	     prev_item->next != NULL;
	     prev_item = prev_item->next)
	    ;  /* null body */
	cur_item = typecalloc(lynx_list_item_type);
	if (cur_item == NULL)
	    outofmem(__FILE__, "read_cfg");
	else
	    prev_item->next = cur_item;
    }
    cur_item->next = NULL;
    cur_item->name = NULL;
    cur_item->command = NULL;
    cur_item->always_enabled = FALSE;
    cur_item->override_primary_action = FALSE;
    cur_item->pagelen = 66;

    /*
     *	Find first unescaped colon and process fields
     */
    if ((colon = find_colon(buffer)) != NULL) {
	/*
	 *  Process name field
	 */
	cur_item->name = typecallocn(char, colon-buffer+1);
	if (cur_item->name == NULL)
	    outofmem(__FILE__, "read_cfg");
	LYstrncpy(cur_item->name, buffer, (int)(colon-buffer));
	remove_backslashes(cur_item->name);

	/*
	 *  Find end of command string and beginning of TRUE/FALSE option
	 *  field.  If we do not find a colon that ends the command string,
	 *  leave the always_enabled option flag as FALSE.  In any case,
	 *  we want the command string.
	 */
	if ((next_colon = find_colon(colon+1)) == NULL) {
	    next_colon = colon + strlen(colon);
	}
	if (next_colon - (colon+1) > 0) {
	    cur_item->command = typecallocn(char,next_colon-colon);
	    if (cur_item->command == NULL)
		outofmem(__FILE__, "read_cfg");
	    LYstrncpy(cur_item->command, colon+1, (int)(next_colon-(colon+1)));
	    remove_backslashes(cur_item->command);
	}
	if (*next_colon++) {
	    colon = next_colon;
	    if ((next_colon = strchr(colon, ':')) != 0)
		*next_colon++ = '\0';
	    cur_item->always_enabled = is_true(colon);
	    if (next_colon) {
		if (special) {
		    cur_item->pagelen = atoi(next_colon);
		} else {
		    cur_item->override_primary_action = is_true(next_colon);
		}
	    }
	}
    }
}

PUBLIC lynx_list_item_type *find_item_by_number ARGS2(
	lynx_list_item_type *,	list_ptr,
	char *,			number)
{
    int value = atoi(number);
    while (value-- >= 0 && list_ptr != 0) {
	list_ptr = list_ptr->next;
    }
    return list_ptr;
}

PUBLIC int match_item_by_name ARGS3(
    lynx_list_item_type *,	ptr,
    char *,			name,
    BOOLEAN,			only_overriders)
{
    return
	(ptr->command != 0
	&& !strncasecomp(ptr->name, name, strlen(ptr->name))
	&& (only_overriders ? ptr->override_primary_action : 1));
}

#if defined(USE_COLOR_STYLE) || defined(USE_COLOR_TABLE)

#ifndef COLOR_WHITE
#define COLOR_WHITE 7
#endif

#ifndef COLOR_BLACK
#define COLOR_BLACK 0
#endif

#ifdef USE_DEFAULT_COLORS
int default_fg = DEFAULT_COLOR;
int default_bg = DEFAULT_COLOR;
#else
int default_fg = COLOR_WHITE;
int default_bg = COLOR_BLACK;
#endif

PRIVATE CONST char *Color_Strings[16] =
{
    "black",
    "red",
    "green",
    "brown",
    "blue",
    "magenta",
    "cyan",
    "lightgray",
    "gray",
    "brightred",
    "brightgreen",
    "yellow",
    "brightblue",
    "brightmagenta",
    "brightcyan",
    "white"
};

#if defined(PDCURSES) && !defined(XCURSES)
/*
 * PDCurses (and possibly some other implementations) use a non-ANSI set of
 * codes for colors.
 */
PRIVATE int ColorCode ARGS1(
	int,	color)
{
	static int map[] = {
		0,  4,	2,  6, 1,  5,  3,  7,
		8, 12, 10, 14, 9, 13, 11, 15 };
	return map[color];
}
#else
#define ColorCode(color) (color)
#endif

BOOL default_color_reset = FALSE;

/*
 *  Validator for COLOR fields.
 */
PUBLIC int check_color ARGS2(
	char *, color,
	int,	the_default)
{
    int i;

    CTRACE2(TRACE_STYLE, (tfp, "check_color(%s,%d)\n", color, the_default));
    if (!strcasecomp(color, "default")) {
#ifdef USE_DEFAULT_COLORS
	if (!default_color_reset)
	    the_default = DEFAULT_COLOR;
#endif	/* USE_DEFAULT_COLORS */
	CTRACE2(TRACE_STYLE, (tfp, "=> default %d\n", the_default));
	return the_default;
    }
    if (!strcasecomp(color, "nocolor"))
	return NO_COLOR;

    for (i = 0; i < 16; i++) {
	if (!strcasecomp(color, Color_Strings[i])) {
	    int c = ColorCode(i);

	    CTRACE2(TRACE_STYLE, (tfp, "=> %d\n", c));
	    return c;
	}
    }
    CTRACE2(TRACE_STYLE, (tfp, "=> ERR_COLOR\n"));
    return ERR_COLOR;
}

PUBLIC CONST char *lookup_color ARGS1(
    int,	code)
{
    unsigned n;
    for (n = 0; n < 16; n++) {
	if ((int) ColorCode(n) == code)
	    return Color_Strings[n];
    }
    return "default";
}
#endif /* USE_COLOR_STYLE || USE_COLOR_TABLE */

#if defined(USE_COLOR_TABLE) || defined(EXP_ASSUMED_COLOR)

/*
 *  Exit routine for failed COLOR parsing.
 */
PRIVATE void exit_with_color_syntax ARGS1(
	char *,		error_line)
{
    unsigned int i;
    fprintf (stderr, gettext("\
Syntax Error parsing COLOR in configuration file:\n\
The line must be of the form:\n\
COLOR:INTEGER:FOREGROUND:BACKGROUND\n\
\n\
Here FOREGROUND and BACKGROUND must be one of:\n\
The special strings 'nocolor' or 'default', or\n")
	    );
    for (i = 0; i < 16; i += 4) {
	fprintf(stderr, "%16s %16s %16s %16s\n",
		Color_Strings[i], Color_Strings[i + 1],
		Color_Strings[i + 2], Color_Strings[i + 3]);
    }
    fprintf (stderr, "%s\n%s\n", gettext("Offending line:"), error_line);
    exit_immediately(EXIT_FAILURE);
}
#endif /* defined(USE_COLOR_TABLE) || defined(EXP_ASSUMED_COLOR) */

#if defined(USE_COLOR_TABLE)
/*
 *  Process string buffer fields for COLOR setting.
 */
PRIVATE void parse_color ARGS1(
	char *, buffer)
{
    int color;
    char *fg, *bg;
    char *temp = 0;

    StrAllocCopy(temp, buffer);	/* save a copy, for error messages */

    /*
     *	We are expecting a line of the form:
     *	  INTEGER:FOREGROUND:BACKGROUND
     */
    color = atoi(buffer);
    if (NULL == (fg = find_colon(buffer)))
	exit_with_color_syntax(temp);
    *fg++ = '\0';

    if (NULL == (bg = find_colon(fg)))
	exit_with_color_syntax(temp);
    *bg++ = '\0';

#if defined(USE_SLANG)
    if ((check_color(fg, default_fg) == ERR_COLOR) ||
	(check_color(bg, default_bg) == ERR_COLOR))
	exit_with_color_syntax(temp);

    SLtt_set_color(color, NULL, fg, bg);
#else
    if (lynx_chg_color(color,
	check_color(fg, default_fg),
	check_color(bg, default_bg)) < 0)
	exit_with_color_syntax(temp);
#endif
    FREE(temp);
}
#endif /* USE_COLOR_TABLE */

#ifdef USE_SOURCE_CACHE
static Config_Enum tbl_source_cache[] = {
    { "FILE",	SOURCE_CACHE_FILE },
    { "MEMORY",	SOURCE_CACHE_MEMORY },
    { "NONE",	SOURCE_CACHE_NONE },
    { NULL,		-1 },
};

static Config_Enum tbl_abort_source_cache[] = {
    { "KEEP",	SOURCE_CACHE_FOR_ABORTED_KEEP },
    { "DROP",	SOURCE_CACHE_FOR_ABORTED_DROP },
    { NULL,		-1 },
};
#endif

#define PARSE_ADD(n,v)   {n, CONF_ADD_ITEM,    UNION_ADD(v), 0}
#define PARSE_SET(n,v)   {n, CONF_BOOL,        UNION_SET(v), 0}
#define PARSE_ENU(n,v,t) {n, CONF_ENUM,        UNION_INT(v), t}
#define PARSE_INT(n,v)   {n, CONF_INT,         UNION_INT(v), 0}
#define PARSE_TIM(n,v)   {n, CONF_TIME,        UNION_INT(v), 0}
#define PARSE_STR(n,v)   {n, CONF_STR,         UNION_STR(v), 0}
#define PARSE_PRG(n,v)   {n, CONF_PRG,         UNION_DEF(v), 0}
#define PARSE_Env(n,v)   {n, CONF_ENV,         UNION_ENV(v), 0}
#define PARSE_ENV(n,v)   {n, CONF_ENV2,        UNION_ENV(v), 0}
#define PARSE_FUN(n,v)   {n, CONF_FUN,         UNION_FUN(v), 0}
#define PARSE_REQ(n,v)   {n, CONF_INCLUDE,     UNION_FUN(v), 0}
#define PARSE_DEF(n,v)   {n, CONF_ADD_TRUSTED, UNION_DEF(v), 0}
#define PARSE_NIL        {NULL,0,              UNION_DEF(0), 0}

typedef enum {
    CONF_UNSPECIFIED = 0
    ,CONF_BOOL			/* BOOLEAN type */
    ,CONF_FUN
    ,CONF_TIME
    ,CONF_ENUM
    ,CONF_INT
    ,CONF_STR
    ,CONF_PRG
    ,CONF_ENV			/* from environment variable */
    ,CONF_ENV2			/* from environment VARIABLE */
    ,CONF_INCLUDE		/* include file-- handle special */
    ,CONF_ADD_ITEM
    ,CONF_ADD_TRUSTED
} Conf_Types;

typedef struct
{
   CONST char *name;
   Conf_Types type;
   ParseData;
   Config_Enum *table;
}
Config_Type;

PRIVATE int assume_charset_fun ARGS1(
	char *,		value)
{
    UCLYhndl_for_unspec = safeUCGetLYhndl_byMIME(value);
    StrAllocCopy(UCAssume_MIMEcharset,
			LYCharSet_UC[UCLYhndl_for_unspec].MIMEname);
/*    this may be a memory for bogus typo -
    StrAllocCopy(UCAssume_MIMEcharset, value);
    LYLowerCase(UCAssume_MIMEcharset);    */

    return 0;
}

PRIVATE int assume_local_charset_fun ARGS1(
	char *,		value)
{
    UCLYhndl_HTFile_for_unspec = safeUCGetLYhndl_byMIME(value);
    return 0;
}

PRIVATE int assume_unrec_charset_fun ARGS1(
	char *,		value)
{
    UCLYhndl_for_unrec = safeUCGetLYhndl_byMIME(value);
    return 0;
}

PRIVATE int character_set_fun ARGS1(
	char *,		value)
{
    int i = UCGetLYhndl_byAnyName(value); /* by MIME or full name */

    if (i < 0) {
#ifdef CAN_AUTODETECT_DISPLAY_CHARSET
	if (auto_display_charset >= 0
	    && (!strnicmp(value,"AutoDetect ",11)
		|| !strnicmp(value,"AutoDetect-2 ",13)))
	    current_char_set = auto_display_charset;
#endif
	/* do nothing here: so fallback to userdefs.h */
    }
    else
	current_char_set = i;

    return 0;
}

PRIVATE int outgoing_mail_charset_fun ARGS1(
	char *,		value)
{
    outgoing_mail_charset = UCGetLYhndl_byMIME(value);
    /* -1 if NULL or not recognized value: no translation (compatibility) */

    return 0;
}

#ifdef EXP_ASSUMED_COLOR
/*
 *  Process string buffer fields for ASSUMED_COLOR setting.
 */
PRIVATE int assumed_color_fun ARGS1(
	char *, buffer)
{
    char *fg = buffer, *bg;
    char *temp = 0;

    StrAllocCopy(temp, buffer);	/* save a copy, for error messages */

    /*
     *	We are expecting a line of the form:
     *	  FOREGROUND:BACKGROUND
     */
    if (NULL == (bg = find_colon(fg)))
	exit_with_color_syntax(temp);
    *bg++ = '\0';

    default_fg = check_color(fg, default_fg);
    default_bg = check_color(bg, default_bg);

    if (default_fg == ERR_COLOR
     || default_bg == ERR_COLOR)
	exit_with_color_syntax(temp);
#ifdef USE_SLANG
    /*
     * Sorry - the order of initialization of slang precludes setting the
     * default colors from the lynx.cfg file, since slang is already
     * initialized before the file is read, and there is no interface defined
     * for setting it from the application (that's one of the problems with
     * using environment variables rather than a programmable interface) -TD
     */
#endif
    FREE(temp);
    return 0;
}
#endif /* EXP_ASSUMED_COLOR */

#ifdef USE_COLOR_TABLE
PRIVATE int color_fun ARGS1(
	char *,		value)
{
    parse_color (value);
    return 0;
}
#endif

PRIVATE int default_bookmark_file_fun ARGS1(
	char *,		value)
{
    set_default_bookmark_page(value);
    return 0;
}

PRIVATE int default_cache_size_fun ARGS1(
	char *,		value)
{
    HTCacheSize = atoi(value);
    if (HTCacheSize < 2) HTCacheSize = 2;
    return 0;
}

PRIVATE int default_editor_fun ARGS1(
	char *,		value)
{
    if (!system_editor) StrAllocCopy(editor, value);
    return 0;
}

PRIVATE int numbers_as_arrows_fun ARGS1(
	char *,		value)
{
    if (is_true(value))
	keypad_mode = NUMBERS_AS_ARROWS;
    else
	keypad_mode = LINKS_ARE_NUMBERED;

    return 0;
}

#ifdef DIRED_SUPPORT
PRIVATE int dired_menu_fun ARGS1(
	char *,		value)
{
    add_menu_item(value);
    return 0;
}
#endif

PRIVATE int jumpfile_fun ARGS1(
	char *,		value)
{
    char *buffer = NULL;

    HTSprintf0 (&buffer, "JUMPFILE:%s", value);
    if (!LYJumpInit(buffer))
	CTRACE((tfp, "Failed to register %s\n", buffer));
    FREE(buffer);

    return 0;
}

#ifdef EXP_KEYBOARD_LAYOUT
PRIVATE int keyboard_layout_fun ARGS1(
	char *,		key)
{
    if (!LYSetKbLayout(key))
	CTRACE((tfp, "Failed to set keyboard layout %s\n", key));
    return 0;
}
#endif /* EXP_KEYBOARD_LAYOUT */

PRIVATE int keymap_fun ARGS1(
	char *,		key)
{
    char *func, *efunc;

    if ((func = strchr(key, ':')) != NULL) {
	*func++ = '\0';
	efunc = strchr(func, ':');
	/* Allow comments on the ends of key remapping lines. - DT */
	/* Allow third field for line-editor action. - kw */
	if (efunc == func) {	/* have 3rd field, but 2nd field empty */
	    func = NULL;
	} else if (efunc && strncasecomp(efunc + 1, "DIRED", 5) == 0) {
	    if (!remap(key, strtok(func, " \t\n:#"), TRUE)) {
		fprintf(stderr,
			gettext("key remapping of %s to %s for %s failed\n"),
			key, func, efunc + 1);
	    } else if (func && !strcmp("TOGGLE_HELP", func)) {
		LYUseNoviceLineTwo = FALSE;
	    }
	    return 0;
	} else if (!remap(key, strtok(func, " \t\n:#"), FALSE)) {
	    fprintf(stderr, gettext("key remapping of %s to %s failed\n"),
		    key, func);
	} else {
	    if (func && !strcmp("TOGGLE_HELP", func))
		LYUseNoviceLineTwo = FALSE;
	}
	if (efunc) {
	    efunc++;
	    if (efunc == strtok((func ? NULL : efunc), " \t\n:#") && *efunc) {
		BOOLEAN success = FALSE;
		int lkc = lkcstring_to_lkc(key);
		int lec = -1;
		int select_edi = 0;
		char *sselect_edi = strtok(NULL, " \t\n:#");
		char **endp = &sselect_edi;
		if (sselect_edi) {
		    if (*sselect_edi)
			select_edi = strtol(sselect_edi, endp, 10);
		    if (**endp != '\0') {
			fprintf(stderr,
				gettext(
	"invalid line-editor selection %s for key %s, selecting all\n"),
				sselect_edi, key);
			select_edi = 0;
		    }
		}
		/*
		 *  PASS! tries to enter the key into the LYLineEditors
		 *  bindings in a different way from PASS, namely as
		 *  binding that maps to the specific lynx actioncode
		 *  (rather than to LYE_FORM_PASS).  That only works
		 *  for lynx keycodes with modifier bit set, and we
		 *  have no documented/official way to specify this
		 *  in the KEYMAP directive, although it can be made
		 *  to work e.g. by specifying a hex value that has the
		 *  modifier bit set.  But knowledge about the bit
		 *  pattern of modifiers should remain in internal
		 *  matter subject to change...  At any rate, if
		 *  PASS! fails try it the same way as for PASS. - kw
		 */
		if (!success && strcasecomp(efunc, "PASS!") == 0) {
		    if (func) {
			lec = LYE_FORM_LAC|lacname_to_lac(func);
			success = (BOOL) LYRemapEditBinding(lkc, lec, select_edi);
		    }
		    if (!success)
			fprintf(stderr,
				gettext(
   "setting of line-editor binding for key %s (0x%x) to 0x%x for %s failed\n"),
				key, lkc, lec, efunc);
		    else
			return 0;
		}
		if (!success) {
		    lec = lecname_to_lec(efunc);
		    success = (BOOL) LYRemapEditBinding(lkc, lec, select_edi);
		}
		if (!success) {
		    if (lec != -1) {
			fprintf(stderr,
				gettext(
   "setting of line-editor binding for key %s (0x%x) to 0x%x for %s failed\n"),
				key, lkc, lec, efunc);
		    } else {
			fprintf(stderr,
				gettext(
	   "setting of line-editor binding for key %s (0x%x) for %s failed\n"),
				key, lkc, efunc);
		    }
		}
	    }
	}
    }
    return 0;
}

PRIVATE int localhost_alias_fun ARGS1(
	char *,		value)
{
    LYAddLocalhostAlias(value);
    return 0;
}

#ifdef LYNXCGI_LINKS
PRIVATE int lynxcgi_environment_fun ARGS1(
	char *,		value)
{
    add_lynxcgi_environment(value);
    return 0;
}
#endif

PRIVATE int lynx_sig_file_fun ARGS1(
	char *,		value)
{
    char temp[LY_MAXPATH];
    LYstrncpy(temp, value, sizeof(temp)-1);
    if (LYPathOffHomeOK(temp, sizeof(temp))) {
	StrAllocCopy(LynxSigFile, temp);
	LYAddPathToHome(temp, sizeof(temp), LynxSigFile);
	StrAllocCopy(LynxSigFile, temp);
	CTRACE((tfp, "LYNX_SIG_FILE set to '%s'\n", LynxSigFile));
    } else {
	CTRACE((tfp, "LYNX_SIG_FILE '%s' is bad. Ignoring.\n", LYNX_SIG_FILE));
    }
    return 0;
}

#ifndef DISABLE_NEWS
PRIVATE int news_chunk_size_fun ARGS1(
	char *,		value)
{
    HTNewsChunkSize = atoi(value);
    /*
     * If the new HTNewsChunkSize exceeds the maximum,
     * increase HTNewsMaxChunk to this size. - FM
     */
    if (HTNewsChunkSize > HTNewsMaxChunk)
	HTNewsMaxChunk = HTNewsChunkSize;
    return 0;
}

PRIVATE int news_max_chunk_fun ARGS1(
	char *,		value)
{
    HTNewsMaxChunk = atoi(value);
    /*
     * If HTNewsChunkSize exceeds the new maximum,
     * reduce HTNewsChunkSize to this maximum. - FM
     */
    if (HTNewsChunkSize > HTNewsMaxChunk)
	HTNewsChunkSize = HTNewsMaxChunk;
    return 0;
}

PRIVATE int news_posting_fun ARGS1(
	char *,		value)
{
    LYNewsPosting = is_true(value);
    no_newspost = (BOOL) (LYNewsPosting == FALSE);
    return 0;
}
#endif /* DISABLE_NEWS */

#ifndef NO_RULES
PRIVATE int cern_rulesfile_fun ARGS1(
	char *,		value)
{
    char *rulesfile1 = NULL;
    char *rulesfile2 = NULL;
    if (HTLoadRules(value) >= 0) {
	return 0;
    }
    StrAllocCopy(rulesfile1, value);
    LYTrimLeading(value);
    LYTrimTrailing(value);
    if (!strncmp(value, "~/", 2)) {
	StrAllocCopy(rulesfile2, Home_Dir());
	StrAllocCat(rulesfile2, value+1);
    }
    else {
	StrAllocCopy(rulesfile2, value);
    }
    if (strcmp(rulesfile1, rulesfile2) &&
	HTLoadRules(rulesfile2) >= 0) {
	FREE(rulesfile1);
	FREE(rulesfile2);
	return 0;
    }
    fprintf(stderr,
	    gettext(
		"Lynx: cannot start, CERN rules file %s is not available\n"
		),
	    (rulesfile2 && *rulesfile2) ? rulesfile2 : gettext("(no name)"));
    exit_immediately(EXIT_FAILURE);
    return 0;			/* though redundant, for compiler-warnings */
}
#endif /* NO_RULES */

PRIVATE int printer_fun ARGS1(
	char *,		value)
{
    add_item_to_list(value, &printers, TRUE);
    return 0;
}

PRIVATE int referer_with_query_fun ARGS1(
	char *,		value)
{
    if (!strncasecomp(value, "SEND", 4))
	LYRefererWithQuery = 'S';
    else if (!strncasecomp(value, "PARTIAL", 7))
	LYRefererWithQuery = 'P';
    else
	LYRefererWithQuery = 'D';
    return 0;
}

PRIVATE int suffix_fun ARGS1(
	char *,		value)
{
    char *mime_type, *p;
    char *encoding = NULL, *sq = NULL, *description = NULL;
    double q = 1.0;

    if ((strlen (value) < 3)
    || (NULL == (mime_type = strchr (value, ':')))) {
	CTRACE((tfp, "Invalid SUFFIX:%s ignored.\n", value));
	return 0;
    }

    *mime_type++ = '\0';
    if (*mime_type) {
	if ((encoding = strchr(mime_type, ':')) != NULL) {
	    *encoding++ = '\0';
	    if ((sq = strchr(encoding, ':')) != NULL) {
		*sq++ = '\0';
		if ((description = strchr(sq, ':')) != NULL) {
		    *description++ = '\0';
		    if ((p = strchr(sq, ':')) != NULL)
			*p = '\0';
		    LYTrimTail(description);
		}
		LYRemoveBlanks(sq);
		if (!*sq)
		    sq = NULL;
	    }
	    LYRemoveBlanks(encoding);
	    LYLowerCase(encoding);
	    if (!*encoding)
		encoding = NULL;
	}
    }

    LYRemoveBlanks(mime_type);
    /*
     *  Not converted to lowercase on input, to make it possible to
     *  reproduce the equivalent of some of the HTInit.c defaults
     *  that use mixed case, although that is not recomended. - kw
     */ /*LYLowerCase(mime_type);*/

    if (!*mime_type) { /* that's ok now, with an encoding!  */
	CTRACE((tfp, "SUFFIX:%s without MIME type for %s\n", value,
	       encoding ? encoding : "what?"));
	mime_type = NULL; /* that's ok now, with an encoding!  */
	if (!encoding)
	    return 0;
    }

    if (!encoding) {
	if (strstr(mime_type, "tex") != NULL ||
	    strstr(mime_type, "postscript") != NULL ||
	    strstr(mime_type, "sh") != NULL ||
	    strstr(mime_type, "troff") != NULL ||
	    strstr(mime_type, "rtf") != NULL)
	    encoding = "8bit";
	else
	    encoding = "binary";
    }
    if (!sq) {
	q = 1.0;
    } else {
	double df = strtod(sq, &p);
	if (p == sq && df == 0.0) {
	    CTRACE((tfp, "Invalid q=%s for SUFFIX:%s, using -1.0\n",
		   sq, value));
	    q = -1.0;
	} else {
	    q = df;
	}
    }
    HTSetSuffix5(value, mime_type, encoding, description, q);

    return 0;
}

PRIVATE int suffix_order_fun ARGS1(
	char *,		value)
{
    char *p = value;
    char *optn;
    BOOLEAN want_file_init_now = FALSE;

    LYUseBuiltinSuffixes = TRUE;
    while ((optn = HTNextTok(&p, ", ", "", NULL)) != NULL) {
	if (!strcasecomp(optn, "NO_BUILTIN")) {
	    LYUseBuiltinSuffixes = FALSE;
	} else if (!strcasecomp(optn, "PRECEDENCE_HERE")) {
	    want_file_init_now = TRUE;
	} else if (!strcasecomp(optn, "PRECEDENCE_OTHER")) {
	    want_file_init_now = FALSE;
	} else {
	    CTRACE((tfp, "Invalid SUFFIX_ORDER:%s\n", optn));
	    break;
	}
    }

    if (want_file_init_now && !FileInitAlreadyDone) {
	HTFileInit();
	FileInitAlreadyDone = TRUE;
    }
    return 0;
}

PRIVATE int system_editor_fun ARGS1(
	char *,		value)
{
    StrAllocCopy(editor, value);
    system_editor = TRUE;
    return 0;
}

PRIVATE int viewer_fun ARGS1(
	char *,		value)
{
    char *mime_type;
    char *viewer;
    char *environment;

    mime_type = value;

    if ((strlen (value) < 3)
    || (NULL == (viewer = strchr (mime_type, ':'))))
	return 0;

    *viewer++ = '\0';

    LYRemoveBlanks(mime_type);
    LYLowerCase(mime_type);

    environment = strrchr(viewer, ':');
    if ((environment != NULL) &&
	(strlen(viewer) > 1) && *(environment-1) != '\\') {
	*environment++ = '\0';
	remove_backslashes(viewer);
	/*
	 * If environment equals xwindows then only assign the presentation if
	 * there is a $DISPLAY variable.
	 */
	if (!strcasecomp(environment,"XWINDOWS")) {
	    if (LYgetXDisplay() != NULL)
		HTSetPresentation(mime_type, viewer, 1.0, 3.0, 0.0, 0);
	} else if (!strcasecomp(environment,"NON_XWINDOWS")) {
	    if (LYgetXDisplay() == NULL)
		HTSetPresentation(mime_type, viewer, 1.0, 3.0, 0.0, 0);
	} else {
	    HTSetPresentation(mime_type, viewer, 1.0, 3.0, 0.0, 0);
	}
    } else {
	remove_backslashes(viewer);
	HTSetPresentation(mime_type, viewer, 1.0, 3.0, 0.0, 0);
    }

    return 0;
}

PRIVATE int nonrest_sigwinch_fun ARGS1(
	char *,		value)
{
    if (!strncasecomp(value, "XWINDOWS", 8)) {
	LYNonRestartingSIGWINCH = (BOOL) (LYgetXDisplay() != NULL);
    } else {
	LYNonRestartingSIGWINCH = is_true(value);
    }
    return 0;
}

#ifdef EXP_CHARSET_CHOICE
PRIVATE void matched_charset_choice ARGS2(
	BOOL,	display_charset,
	int,	i)
{
    int j;

    if (display_charset && !custom_display_charset) {
	for (custom_display_charset = TRUE, j = 0; j < LYNumCharsets; ++j)
	    charset_subsets[j].hide_display = TRUE;
    } else if (!display_charset && !custom_assumed_doc_charset) {
	for (custom_assumed_doc_charset = TRUE, j = 0; j < LYNumCharsets; ++j)
	    charset_subsets[j].hide_assumed = TRUE;
    }
    if (display_charset)
	charset_subsets[i].hide_display = FALSE;
    else
	charset_subsets[i].hide_assumed = FALSE;
}

PRIVATE int parse_charset_choice ARGS2(
	char *,	p,
	BOOL,	display_charset) /*if FALSE, then assumed doc charset*/
{
    int len, i;
    int matches = 0;

    /*only one charset choice is allowed per line!*/
    LYTrimHead(p);
    LYTrimTail(p);
    CTRACE((tfp, "parsing charset choice for %s:\"%s\"",
	(display_charset ? "display charset" : "assumed doc charset"), p));
    len = strlen(p);
    if (!len) {
	CTRACE((tfp," - EMPTY STRING\n"));
	return 1;
    }
    if (*p == '*' && len == 1) {
	if (display_charset)
	    for (custom_display_charset = TRUE, i = 0 ;i < LYNumCharsets; ++i)
		charset_subsets[i].hide_display = FALSE;
	else
	    for (custom_assumed_doc_charset = TRUE, i = 0; i < LYNumCharsets; ++i)
		charset_subsets[i].hide_assumed = FALSE;
	CTRACE((tfp," - all unhidden\n"));
	return 0;
    }
    if (p[len-1] == '*') {
	--len;
	for (i = 0 ;i < LYNumCharsets; ++i) {
	    if ((!strncasecomp(p, LYchar_set_names[i], len)) ||
		(!strncasecomp(p, LYCharSet_UC[i].MIMEname, len)) ) {
		++matches;
		matched_charset_choice(display_charset, i);
	    }
	}
	CTRACE((tfp," - %d matches\n", matches));
	return 0;
    } else {
	for (i = 0; i < LYNumCharsets; ++i) {
	    if ((!strcasecomp(p,LYchar_set_names[i])) ||
		(!strcasecomp(p,LYCharSet_UC[i].MIMEname)) ) {
		matched_charset_choice(display_charset, i);
		CTRACE((tfp," - OK\n"));
		++matches;
		return 0;
	    }
	}
	CTRACE((tfp," - NOT recognised\n"));
	return 1;
    }
}

PRIVATE int parse_display_charset_choice ARGS1(char*,p)
{
    return parse_charset_choice(p,1);
}

PRIVATE int parse_assumed_doc_charset_choice ARGS1(char*,p)
{
    return parse_charset_choice(p,0);
}

#endif /* EXP_CHARSET_CHOICE */

#ifdef USE_PRETTYSRC
PRIVATE void html_src_bad_syntax ARGS2(
	    char*, value,
	    char*, option_name)
{
    char *buf = 0;

    HTSprintf0(&buf,"HTMLSRC_%s", option_name);
    LYUpperCase(buf);
    fprintf(stderr,"Bad syntax in TAGSPEC %s:%s\n", buf, value);
    exit_immediately(EXIT_FAILURE);
}

PRIVATE int parse_html_src_spec ARGS3(
	    HTlexeme, lexeme_code,
	    char*, value,
	    char*, option_name)
{
   /* Now checking the value for being correct.  Since HTML_dtd is not
    * initialized completely (member tags points to non-initiailized data), we
    * use tags_old.  If the syntax is incorrect, then lynx will exit with error
    * message.
    */
    char* ts2;
    if (isEmpty(value)) return 0; /* silently ignoring*/

#define BS() html_src_bad_syntax(value,option_name)

    ts2 = strchr(value,':');
    if (!ts2)
	BS();
    *ts2 = '\0';

    CTRACE((tfp,"ReadCFG - parsing tagspec '%s:%s' for option '%s'\n",value,ts2,option_name));
    html_src_clean_item(lexeme_code);
    if ( html_src_parse_tagspec(value, lexeme_code, TRUE, TRUE)
	|| html_src_parse_tagspec(ts2, lexeme_code, TRUE, TRUE) )
    {
	*ts2 = ':';
	BS();
    }

    *ts2 = ':';
    StrAllocCopy(HTL_tagspecs[lexeme_code],value);
#undef BS
    return 0;
}

PRIVATE int psrcspec_fun ARGS1(char*,s)
{
    char* e;
    static Config_Enum lexemnames[] =
    {
	{ "comm",	HTL_comm	},
	{ "tag",	HTL_tag		},
	{ "attrib",	HTL_attrib	},
	{ "attrval",	HTL_attrval	},
	{ "abracket",	HTL_abracket	},
	{ "entity",	HTL_entity	},
	{ "href",	HTL_href	},
	{ "entire",	HTL_entire	},
	{ "badseq",	HTL_badseq	},
	{ "badtag",	HTL_badtag	},
	{ "badattr",	HTL_badattr	},
	{ "sgmlspecial", HTL_sgmlspecial },
	{ NULL,		-1		}
    };
    int found;

    e = strchr(s, ':');
    if (!e) {
	CTRACE((tfp,"bad format of PRETTYSRC_SPEC setting value, ignored %s\n",s));
	return 0;
    }
    *e = '\0';
    if (!LYgetEnum(lexemnames, s, &found)) {
	CTRACE((tfp,"bad format of PRETTYSRC_SPEC setting value, ignored %s:%s\n",s,e+1));
	return 0;
    }
    parse_html_src_spec(found, e+1, s);
    return 0;
}

PRIVATE int read_htmlsrc_attrname_xform ARGS1( char*,str)
{
    int val;
    if ( 1 == sscanf(str, "%d", &val) ) {
	if (val<0 || val >2) {
	    CTRACE((tfp,"bad value for htmlsrc_attrname_xform (ignored - must be one of 0,1,2): %d\n", val));
	} else
	    attrname_transform = val;
    } else {
	CTRACE((tfp,"bad value for htmlsrc_attrname_xform (ignored): %s\n",
		    str));
    }
    return 0;
}

PRIVATE int read_htmlsrc_tagname_xform ARGS1( char*,str)
{
    int val;
    if ( 1 == sscanf(str,"%d",&val) ) {
	if (val<0 || val >2) {
	    CTRACE((tfp,"bad value for htmlsrc_tagname_xform (ignored - must be one of 0,1,2): %d\n", val));
	} else
	    tagname_transform = val;
    } else {
	CTRACE((tfp,"bad value for htmlsrc_tagname_xform (ignored): %s\n",
		    str));
    }
    return 0;
}
#endif

#if defined(PDCURSES) && defined(PDC_BUILD) && PDC_BUILD >= 2401
PRIVATE int screen_size_fun ARGS1(
	char *,		value)
{
    char *cp;

    if ((cp = strchr(value, ',')) != 0) {
	*cp++ = '\0';       /* Terminate ID */
	scrsize_x = atoi(value);
	scrsize_y = atoi(cp);
	if ((scrsize_x <= 1) || (scrsize_y <= 1)) {
	    scrsize_x = scrsize_y = 0;
	}
	if ((scrsize_x > 0) && (scrsize_x < 80)) {
	    scrsize_x = 80;
	}
	if ((scrsize_y > 0) && (scrsize_y < 4)) {
	    scrsize_y = 4;
	}
	CTRACE((tfp, "scrsize: x=%d, y=%d\n", scrsize_x, scrsize_y));
    }
    return 0;
}
#endif

/* This table is searched ignoring case */
PRIVATE Config_Type Config_Table [] =
{
     PARSE_SET(RC_ACCEPT_ALL_COOKIES,   LYAcceptAllCookies),
     PARSE_TIM(RC_ALERTSECS,            AlertSecs),
     PARSE_SET(RC_ALWAYS_RESUBMIT_POSTS, LYresubmit_posts),
#ifdef EXEC_LINKS
     PARSE_DEF(RC_ALWAYS_TRUSTED_EXEC,  ALWAYS_EXEC_PATH),
#endif
     PARSE_FUN(RC_ASSUME_CHARSET,       assume_charset_fun),
     PARSE_FUN(RC_ASSUME_LOCAL_CHARSET, assume_local_charset_fun),
     PARSE_FUN(RC_ASSUME_UNREC_CHARSET, assume_unrec_charset_fun),
#ifdef EXP_ASSUMED_COLOR
     PARSE_FUN(RC_ASSUMED_COLOR,        assumed_color_fun),
#endif
#ifdef EXP_CHARSET_CHOICE
     PARSE_FUN(RC_ASSUMED_DOC_CHARSET_CHOICE, parse_assumed_doc_charset_choice),
#endif
#ifdef DIRED_SUPPORT
     PARSE_INT(RC_AUTO_UNCACHE_DIRLISTS, LYAutoUncacheDirLists),
#endif
#ifndef DISABLE_BIBP
     PARSE_STR(RC_BIBP_BIBHOST,         BibP_bibhost),
     PARSE_STR(RC_BIBP_GLOBALSERVER,    BibP_globalserver),
#endif
     PARSE_SET(RC_BLOCK_MULTI_BOOKMARKS, LYMBMBlocked),
     PARSE_SET(RC_BOLD_H1,              bold_H1),
     PARSE_SET(RC_BOLD_HEADERS,         bold_headers),
     PARSE_SET(RC_BOLD_NAME_ANCHORS,    bold_name_anchors),
     PARSE_PRG(RC_BZIP2_PATH,           ppBZIP2),
     PARSE_SET(RC_CASE_SENSITIVE_ALWAYS_ON, case_sensitive),
     PARSE_FUN(RC_CHARACTER_SET,        character_set_fun),
#ifdef CAN_SWITCH_DISPLAY_CHARSET
     PARSE_STR(RC_CHARSET_SWITCH_RULES, charset_switch_rules),
     PARSE_STR(RC_CHARSETS_DIRECTORY,   charsets_directory),
#endif
     PARSE_SET(RC_CHECKMAIL,            check_mail),
     PARSE_PRG(RC_CHMOD_PATH,           ppCHMOD),
     PARSE_SET(RC_COLLAPSE_BR_TAGS,     LYCollapseBRs),
#ifdef USE_COLOR_TABLE
     PARSE_FUN(RC_COLOR,                color_fun),
#endif
     PARSE_PRG(RC_COMPRESS_PATH,        ppCOMPRESS),
     PARSE_PRG(RC_COPY_PATH,            ppCOPY),
     PARSE_INT(RC_CONNECT_TIMEOUT,      connect_timeout),
     PARSE_STR(RC_COOKIE_ACCEPT_DOMAINS, LYCookieSAcceptDomains),
#ifdef USE_PERSISTENT_COOKIES
     PARSE_STR(RC_COOKIE_FILE,          LYCookieFile),
#endif /* USE_PERSISTENT_COOKIES */
     PARSE_STR(RC_COOKIE_LOOSE_INVALID_DOMAINS, LYCookieSLooseCheckDomains),
     PARSE_STR(RC_COOKIE_QUERY_INVALID_DOMAINS, LYCookieSQueryCheckDomains),
     PARSE_STR(RC_COOKIE_REJECT_DOMAINS, LYCookieSRejectDomains),
#ifdef USE_PERSISTENT_COOKIES
     PARSE_STR(RC_COOKIE_SAVE_FILE,     LYCookieSaveFile),
#endif /* USE_PERSISTENT_COOKIES */
     PARSE_STR(RC_COOKIE_STRICT_INVALID_DOMAIN, LYCookieSStrictCheckDomains),
     PARSE_Env(RC_CSO_PROXY,		0),
#ifdef VMS
     PARSE_PRG(RC_CSWING_PATH,          ppCSWING),
#endif
     PARSE_TIM(RC_DELAYSECS,            DebugSecs),
     PARSE_FUN(RC_DEFAULT_BOOKMARK_FILE, default_bookmark_file_fun),
     PARSE_FUN(RC_DEFAULT_CACHE_SIZE,   default_cache_size_fun),
     PARSE_FUN(RC_DEFAULT_EDITOR,       default_editor_fun),
     PARSE_STR(RC_DEFAULT_INDEX_FILE,   indexfile),
     PARSE_ENU(RC_DEFAULT_KEYPAD_MODE,  keypad_mode, tbl_keypad_mode),
     PARSE_FUN(RC_DEFAULT_KEYPAD_MODE_NUMARO, numbers_as_arrows_fun),
     PARSE_ENU(RC_DEFAULT_USER_MODE,    user_mode, tbl_user_mode),
#if defined(VMS) && defined(VAXC) && !defined(__DECC)
     PARSE_INT(RC_DEFAULT_VIRTUAL_MEMORY_SIZE, HTVirtualMemorySize),
#endif
#ifdef DIRED_SUPPORT
     PARSE_FUN(RC_DIRED_MENU,           dired_menu_fun),
#endif
#ifdef EXP_CHARSET_CHOICE
     PARSE_FUN(RC_DISPLAY_CHARSET_CHOICE, parse_display_charset_choice),
#endif
     PARSE_ADD(RC_DOWNLOADER,           downloaders),
     PARSE_SET(RC_EMACS_KEYS_ALWAYS_ON, emacs_keys),
     PARSE_FUN(RC_ENABLE_LYNXRC,        enable_lynxrc),
     PARSE_SET(RC_ENABLE_SCROLLBACK,    enable_scrollback),
#ifdef USE_EXTERNALS
     PARSE_ADD(RC_EXTERNAL,             externals),
#endif
     PARSE_Env(RC_FINGER_PROXY,         0),
#if defined(_WINDOWS)	/* 1998/10/05 (Mon) 17:34:15 */
     PARSE_SET(RC_FOCUS_WINDOW,         focus_window),
#endif
     PARSE_SET(RC_FORCE_8BIT_TOUPPER,   UCForce8bitTOUPPER),
     PARSE_ENU(RC_FORCE_COOKIE_PROMPT,  cookie_noprompt, tbl_force_prompt),
     PARSE_SET(RC_FORCE_EMPTY_HREFLESS_A, force_empty_hrefless_a),
     PARSE_SET(RC_FORCE_SSL_COOKIES_SECURE, LYForceSSLCookiesSecure),
#ifdef USE_SSL
     PARSE_ENU(RC_FORCE_SSL_PROMPT,     ssl_noprompt, tbl_force_prompt),
#endif
#if !defined(NO_OPTION_FORMS) && !defined(NO_OPTION_MENU)
     PARSE_SET(RC_FORMS_OPTIONS,        LYUseFormsOptions),
#endif
     PARSE_SET(RC_FTP_PASSIVE,          ftp_passive),
     PARSE_Env(RC_FTP_PROXY,            0),
     PARSE_STR(RC_GLOBAL_EXTENSION_MAP, global_extension_map),
     PARSE_STR(RC_GLOBAL_MAILCAP,       global_type_map),
     PARSE_Env(RC_GOPHER_PROXY,         0),
     PARSE_SET(RC_GOTOBUFFER,           goto_buffer),
     PARSE_PRG(RC_GZIP_PATH,            ppGZIP),
     PARSE_STR(RC_HELPFILE,             helpfile),
#ifdef MARK_HIDDEN_LINKS
     PARSE_STR(RC_HIDDEN_LINK_MARKER,   hidden_link_marker),
#endif
     PARSE_SET(RC_HISTORICAL_COMMENTS,  historical_comments),
#ifdef USE_PRETTYSRC
     PARSE_FUN(RC_HTMLSRC_ATTRNAME_XFORM, read_htmlsrc_attrname_xform),
     PARSE_FUN(RC_HTMLSRC_TAGNAME_XFORM, read_htmlsrc_tagname_xform),
#endif
     PARSE_Env(RC_HTTP_PROXY,           0),
     PARSE_Env(RC_HTTPS_PROXY,          0),
     PARSE_REQ(RC_INCLUDE,              0),
     PARSE_TIM(RC_INFOSECS,             InfoSecs),
     PARSE_PRG(RC_INSTALL_PATH,         ppINSTALL),
     PARSE_STR(RC_JUMP_PROMPT,          jumpprompt),
     PARSE_SET(RC_JUMPBUFFER,           jump_buffer),
     PARSE_FUN(RC_JUMPFILE,             jumpfile_fun),
#ifdef EXP_JUSTIFY_ELTS
     PARSE_SET(RC_JUSTIFY,              ok_justify),
     PARSE_INT(RC_JUSTIFY_MAX_VOID_PERCENT, justify_max_void_percent),
#endif
#ifdef EXP_KEYBOARD_LAYOUT
     PARSE_FUN(RC_KEYBOARD_LAYOUT,      keyboard_layout_fun),
#endif
     PARSE_FUN(RC_KEYMAP,               keymap_fun),
     PARSE_SET(RC_LEFTARROW_IN_TEXTFLD_PROMPT, textfield_prompt_at_left_edge),
#ifndef VMS
     PARSE_STR(RC_LIST_FORMAT,          list_format),
#endif
#ifndef DISABLE_NEWS
     PARSE_SET(RC_LIST_NEWS_DATES,      LYListNewsDates),
     PARSE_SET(RC_LIST_NEWS_NUMBERS,    LYListNewsNumbers),
#endif
#ifdef EXP_LOCALE_CHARSET
     PARSE_SET(RC_LOCALE_CHARSET,       LYLocaleCharset),
#endif
     PARSE_STR(RC_LOCAL_DOMAIN,         LYLocalDomain),
     PARSE_FUN(RC_LOCALHOST_ALIAS,      localhost_alias_fun),
#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
     PARSE_SET(RC_LOCAL_EXECUTION_LINKS_ALWAYS, local_exec),
     PARSE_SET(RC_LOCAL_EXECUTION_LINKS_LOCAL, local_exec_on_local_files),
#endif
     PARSE_STR(RC_LYNX_HOST_NAME,       LYHostName),
     PARSE_FUN(RC_LYNX_SIG_FILE,        lynx_sig_file_fun),
#ifdef LYNXCGI_LINKS
#ifndef VMS
     PARSE_STR(RC_LYNXCGI_DOCUMENT_ROOT, LYCgiDocumentRoot),
#endif
     PARSE_FUN(RC_LYNXCGI_ENVIRONMENT,  lynxcgi_environment_fun),
#endif
#if USE_VMS_MAILER
     PARSE_STR(RC_MAIL_ADRS,            mail_adrs),
#endif
     PARSE_SET(RC_MAIL_SYSTEM_ERROR_LOGGING, error_logging),
     PARSE_SET(RC_MAKE_LINKS_FOR_ALL_IMAGES, clickable_images),
     PARSE_SET(RC_MAKE_PSEUDO_ALTS_FOR_INLINES, pseudo_inline_alts),
     PARSE_TIM(RC_MESSAGESECS,          MessageSecs),
     PARSE_SET(RC_MINIMAL_COMMENTS,     minimal_comments),
     PARSE_PRG(RC_MKDIR_PATH,           ppMKDIR),
     PARSE_ENU(RC_MULTI_BOOKMARK_SUPPORT, LYMultiBookmarks, tbl_multi_bookmarks),
     PARSE_PRG(RC_MV_PATH,              ppMV),
     PARSE_SET(RC_NCR_IN_BOOKMARKS,     UCSaveBookmarksInUnicode),
#ifndef DISABLE_NEWS
     PARSE_FUN(RC_NEWS_CHUNK_SIZE,      news_chunk_size_fun),
     PARSE_FUN(RC_NEWS_MAX_CHUNK,       news_max_chunk_fun),
     PARSE_FUN(RC_NEWS_POSTING,         news_posting_fun),
     PARSE_Env(RC_NEWS_PROXY,           0),
     PARSE_Env(RC_NEWSPOST_PROXY,       0),
     PARSE_Env(RC_NEWSREPLY_PROXY,      0),
     PARSE_Env(RC_NNTP_PROXY,           0),
     PARSE_ENV(RC_NNTPSERVER,           0), /* actually NNTPSERVER */
#endif
     PARSE_SET(RC_NUMBER_FIELDS_ON_LEFT,number_fields_on_left),
     PARSE_SET(RC_NUMBER_LINKS_ON_LEFT, number_links_on_left),
     PARSE_SET(RC_NO_DOT_FILES,         no_dotfiles),
     PARSE_SET(RC_NO_FILE_REFERER,      no_filereferer),
#ifndef VMS
     PARSE_SET(RC_NO_FORCED_CORE_DUMP,  LYNoCore),
#endif
     PARSE_SET(RC_NO_FROM_HEADER,       LYNoFromHeader),
     PARSE_SET(RC_NO_ISMAP_IF_USEMAP,   LYNoISMAPifUSEMAP),
     PARSE_Env(RC_NO_PROXY,             0),
     PARSE_SET(RC_NO_REFERER_HEADER,    LYNoRefererHeader),
     PARSE_SET(RC_NO_TABLE_CENTER,      no_table_center),
     PARSE_FUN(RC_NONRESTARTING_SIGWINCH, nonrest_sigwinch_fun),
     PARSE_FUN(RC_OUTGOING_MAIL_CHARSET, outgoing_mail_charset_fun),
#ifdef DISP_PARTIAL
     PARSE_SET(RC_PARTIAL,              display_partial_flag),
     PARSE_INT(RC_PARTIAL_THRES,        partial_threshold),
#endif
#ifdef USE_PERSISTENT_COOKIES
     PARSE_SET(RC_PERSISTENT_COOKIES,   persistent_cookies),
#endif /* USE_PERSISTENT_COOKIES */
     PARSE_STR(RC_PERSONAL_EXTENSION_MAP, personal_extension_map),
     PARSE_STR(RC_PERSONAL_MAILCAP,     personal_type_map),
     PARSE_STR(RC_PREFERRED_CHARSET,    pref_charset),
     PARSE_STR(RC_PREFERRED_LANGUAGE,   language),
     PARSE_SET(RC_PREPEND_BASE_TO_SOURCE, LYPrependBaseToSource),
     PARSE_SET(RC_PREPEND_CHARSET_TO_SOURCE, LYPrependCharsetToSource),
#ifdef USE_PRETTYSRC
     PARSE_SET(RC_PRETTYSRC,            LYpsrc),
     PARSE_FUN(RC_PRETTYSRC_SPEC,       psrcspec_fun),
     PARSE_SET(RC_PRETTYSRC_VIEW_NO_ANCHOR_NUM, psrcview_no_anchor_numbering),
#endif
     PARSE_FUN(RC_PRINTER,              printer_fun),
     PARSE_SET(RC_QUIT_DEFAULT_YES,     LYQuitDefaultYes),
     PARSE_FUN(RC_REFERER_WITH_QUERY,   referer_with_query_fun),
#ifdef EXP_CMD_LOGGING
     PARSE_TIM(RC_REPLAYSECS,           ReplaySecs),
#endif
     PARSE_SET(RC_REUSE_TEMPFILES,      LYReuseTempfiles),
     PARSE_PRG(RC_RLOGIN_PATH,          ppRLOGIN),
     PARSE_PRG(RC_RM_PATH,              ppRM),
#ifndef NO_RULES
     PARSE_FUN(RC_RULE,                 HTSetConfiguration),
     PARSE_FUN(RC_RULESFILE,            cern_rulesfile_fun),
#endif /* NO_RULES */
     PARSE_STR(RC_SAVE_SPACE,           lynx_save_space),
     PARSE_SET(RC_SCAN_FOR_BURIED_NEWS_REFS, scan_for_buried_news_references),
#if defined(PDCURSES) && defined(PDC_BUILD) && PDC_BUILD >= 2401
     PARSE_FUN(RC_SCREEN_SIZE,          screen_size_fun),
#endif
#ifdef USE_SCROLLBAR
     PARSE_SET(RC_SCROLLBAR,            LYShowScrollbar),
     PARSE_SET(RC_SCROLLBAR_ARROW,      LYsb_arrow),
#endif
     PARSE_SET(RC_SEEK_FRAG_AREA_IN_CUR, LYSeekFragAREAinCur),
     PARSE_SET(RC_SEEK_FRAG_MAP_IN_CUR, LYSeekFragMAPinCur),
     PARSE_SET(RC_SET_COOKIES,          LYSetCookies),
     PARSE_SET(RC_SHOW_CURSOR,          LYShowCursor),
     PARSE_STR(RC_SHOW_KB_NAME,         LYTransferName),
     PARSE_ENU(RC_SHOW_KB_RATE,         LYTransferRate, tbl_transfer_rate),
     PARSE_Env(RC_SNEWS_PROXY,          0),
     PARSE_Env(RC_SNEWSPOST_PROXY,      0),
     PARSE_Env(RC_SNEWSREPLY_PROXY,     0),
     PARSE_SET(RC_SOFT_DQUOTES,         soft_dquotes),
#ifdef USE_SOURCE_CACHE
     PARSE_ENU(RC_SOURCE_CACHE,         LYCacheSource, tbl_source_cache),
     PARSE_ENU(RC_SOURCE_CACHE_FOR_ABORTED, LYCacheSourceForAborted, tbl_abort_source_cache),
#endif
     PARSE_STR(RC_STARTFILE,            startfile),
     PARSE_SET(RC_STRIP_DOTDOT_URLS,    LYStripDotDotURLs),
     PARSE_SET(RC_SUBSTITUTE_UNDERSCORES, use_underscore),
     PARSE_FUN(RC_SUFFIX,               suffix_fun),
     PARSE_FUN(RC_SUFFIX_ORDER,         suffix_order_fun),
     PARSE_FUN(RC_SYSTEM_EDITOR,        system_editor_fun),
     PARSE_STR(RC_SYSTEM_MAIL,          system_mail),
     PARSE_STR(RC_SYSTEM_MAIL_FLAGS,    system_mail_flags),
     PARSE_FUN(RC_TAGSOUP,              get_tagsoup),
     PARSE_PRG(RC_TAR_PATH,             ppTAR),
     PARSE_PRG(RC_TELNET_PATH,          ppTELNET),
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
     PARSE_SET(RC_TEXTFIELDS_NEED_ACTIVATION, textfields_activation_option),
#endif
     PARSE_PRG(RC_TN3270_PATH,          ppTN3270),
#if defined(_WINDOWS)
     PARSE_INT(RC_TIMEOUT,              lynx_timeout),
#endif
     PARSE_PRG(RC_TOUCH_PATH,           ppTOUCH),
     PARSE_SET(RC_TRIM_INPUT_FIELDS,    LYtrimInputFields),
#ifdef EXEC_LINKS
     PARSE_DEF(RC_TRUSTED_EXEC,         EXEC_PATH),
#endif
#ifdef LYNXCGI_LINKS
     PARSE_DEF(RC_TRUSTED_LYNXCGI,      CGI_PATH),
#endif
     PARSE_PRG(RC_UNCOMPRESS_PATH,      ppUNCOMPRESS),
     PARSE_SET(RC_UNDERLINE_LINKS,      LYUnderlineLinks),
     PARSE_PRG(RC_UNZIP_PATH,           ppUNZIP),
#ifdef DIRED_SUPPORT
     PARSE_ADD(RC_UPLOADER,             uploaders),
#endif
     PARSE_STR(RC_URL_DOMAIN_PREFIXES,  URLDomainPrefixes),
     PARSE_STR(RC_URL_DOMAIN_SUFFIXES,  URLDomainSuffixes),
#ifdef VMS
     PARSE_SET(RC_USE_FIXED_RECORDS,    UseFixedRecords),
#endif
#if defined(USE_MOUSE)
     PARSE_SET(RC_USE_MOUSE,            LYUseMouse),
#endif
     PARSE_SET(RC_USE_SELECT_POPUPS,    LYSelectPopups),
     PARSE_PRG(RC_UUDECODE_PATH,        ppUUDECODE),
     PARSE_SET(RC_VERBOSE_IMAGES,       verbose_img),
     PARSE_SET(RC_VI_KEYS_ALWAYS_ON,    vi_keys),
     PARSE_FUN(RC_VIEWER,               viewer_fun),
     PARSE_Env(RC_WAIS_PROXY,           0),
     PARSE_STR(RC_XLOADIMAGE_COMMAND,   XLoadImageCommand),
     PARSE_PRG(RC_ZCAT_PATH,            ppZCAT),
     PARSE_PRG(RC_ZIP_PATH,             ppZIP),

     PARSE_NIL
};

PRIVATE char *lynxcfginfo_url = NULL;	/* static */
#if defined(HAVE_CONFIG_H) && !defined(NO_CONFIG_INFO)
PRIVATE char *configinfo_url = NULL;	/* static */
#endif

/*
 * Free memory allocated in 'read_cfg()'
 */
PUBLIC void free_lynx_cfg NOARGS
{
    Config_Type *tbl;

    for (tbl = Config_Table; tbl->name != 0; tbl++) {
	ParseUnionPtr q = ParseUnionOf(tbl);

	switch (tbl->type) {
	case CONF_ENV:
	    if (q->str_value != 0) {
		char *name = *(q->str_value);
		char *eqls = strchr(name, '=');
		if (eqls != 0) {
		    *eqls = 0;
#ifdef VMS
		    Define_VMSLogical(name, NULL);
#else
# ifdef HAVE_UNSETENV
		    unsetenv(name);
# else
		    if (putenv(name))
			break;
# endif
#endif
		}
		FREE(*(q->str_value));
		FREE(q->str_value);
		/* is it enough for reload_read_cfg() to clean up
		 * the result of putenv()?  No for certain platforms.
		 */
	    }
	    break;
	default:
	    break;
	}
    }
    free_all_item_lists();
#ifdef DIRED_SUPPORT
    reset_dired_menu();		/* frees and resets dired menu items - kw */
#endif
    FREE(lynxcfginfo_url);
#if defined(HAVE_CONFIG_H) && !defined(NO_CONFIG_INFO)
    FREE(configinfo_url);
#endif
}

PRIVATE Config_Type *lookup_config ARGS1(
	char *,		name)
{
    Config_Type *tbl = Config_Table;
    char ch = (char) TOUPPER(*name);

    while (tbl->name != 0) {
	char ch1 = tbl->name[0];

	if ((ch == TOUPPER(ch1))
	    && (0 == strcasecomp (name, tbl->name)))
	    break;

	tbl++;
    }
    return tbl;
}

/*
 * If the given value is an absolute path (by syntax), or we can read it, use
 * the value as given.  Otherwise, assume it must be in the same place we read
 * the parent configuration file from.
 *
 * Note:  only read files from the current directory if there's no parent
 * filename, otherwise it leads to user surprise.
 */
PRIVATE char *actual_filename ARGS3(
    char *,	cfg_filename,
    char *,	parent_filename,
    char *,	dft_filename)
{
    char *my_filename = NULL;

    if (!LYisAbsPath(cfg_filename)
     && !(parent_filename == 0 && LYCanReadFile(cfg_filename))) {
	if (!strncmp(cfg_filename, "~/", 2)) {
	    HTSprintf0(&my_filename, "%s%s", Home_Dir(), cfg_filename+1);
	} else {
	    if (parent_filename != 0) {
		StrAllocCopy(my_filename, parent_filename);
		*LYPathLeaf (my_filename) = '\0';
		StrAllocCat(my_filename, cfg_filename);
	    }
	    if (my_filename == 0 || !LYCanReadFile(my_filename)) {
		StrAllocCopy(my_filename, dft_filename);
		*LYPathLeaf (my_filename) = '\0';
		StrAllocCat(my_filename, cfg_filename);
		if (!LYCanReadFile(my_filename)) {
		    StrAllocCopy(my_filename, cfg_filename);
		}
	    }
	}
    } else {
	StrAllocCopy(my_filename, cfg_filename);
    }
    return my_filename;
}

PUBLIC FILE *LYOpenCFG ARGS3(
    char *,	cfg_filename,
    char *,	parent_filename,
    char *,	dft_filename)
{
    char *my_file = actual_filename(cfg_filename, parent_filename, dft_filename);
    FILE *result;

    CTRACE((tfp, "opening config file %s\n", my_file));
    result = fopen(my_file, TXT_R);
    FREE(my_file);

    return result;
}

#define NOPTS_ ( TABLESIZE(Config_Table) - 1 )
typedef BOOL (optidx_set_t) [ NOPTS_ ];
 /* if element is FALSE, then it's allowed in the current file*/

#define optidx_set_AND(r,a,b) \
    {\
	unsigned i1;\
	for (i1 = 0; i1 < NOPTS_; ++i1) \
	    (r)[i1]= (a)[i1] || (b)[i1]; \
    }

/*
 * For simple (boolean, string, integer, time) values, set the corresponding
 * configuration variable.
 */
PUBLIC void LYSetConfigValue ARGS2(
    char *,	name,
    char *,	value)
{
    Config_Type *tbl = lookup_config(name);
    ParseUnionPtr q = ParseUnionOf(tbl);
    char *temp;

    switch (tbl->type) {
    case CONF_BOOL:
	if (q->set_value != 0)
	    *(q->set_value) = is_true (value);
	break;

    case CONF_FUN:
	if (q->fun_value != 0)
	    (*(q->fun_value)) (value);
	break;

    case CONF_TIME:
	if (q->int_value != 0) {
	    float ival;
	    if (1 == sscanf (value, "%f", &ival)) {
		*(q->int_value) = (int) SECS2Secs(ival);
	    }
	}
	break;

    case CONF_ENUM:
	if (tbl->table != 0)
	    LYgetEnum(tbl->table, value, q->int_value);
	break;

    case CONF_INT:
	if (q->int_value != 0) {
	    int ival;
	    if (1 == sscanf (value, "%d", &ival))
		*(q->int_value) = ival;
	}
	break;

    case CONF_STR:
	if (q->str_value != 0)
	    StrAllocCopy(*(q->str_value), value);
	break;

    case CONF_ENV:
    case CONF_ENV2:

	if (tbl->type == CONF_ENV)
	    LYLowerCase(name);
	else
	    LYUpperCase(name);

	if (LYGetEnv (name) == 0) {
#ifdef VMS
	    Define_VMSLogical(name, value);
#else
	    if (q->str_value == 0)
		q->str_value = typecalloc(char *);
	    HTSprintf0 (q->str_value, "%s=%s", name, value);
	    putenv (*(q->str_value));
#endif
	}
	break;
    case CONF_ADD_ITEM:
	if (q->add_value != 0)
	    add_item_to_list (value, q->add_value, FALSE);
	break;

#if defined(EXEC_LINKS) || defined(LYNXCGI_LINKS)
    case CONF_ADD_TRUSTED:
	add_trusted (value, q->def_value);
	break;
#endif

    case CONF_PRG:
	if (StrAllocCopy(temp, value))
	    HTSetProgramPath(q->def_value, temp);
	break;

    default:
	break;
    }
}

/*
 * Process the configuration file (lynx.cfg).
 *
 * 'allowed' is a pointer to HTList of allowed options.  Since the included
 * file can also include other files with a list of acceptable options, these
 * lists are ANDed.
 */
PRIVATE void do_read_cfg ARGS5(
	char *, cfg_filename,
	char *, parent_filename,
	int,	nesting_level,
	FILE *,	fp0,
	optidx_set_t*, allowed)
{
    FILE *fp;
    char *buffer = 0;

    CTRACE((tfp, "Loading cfg file '%s'.\n", cfg_filename));

    /*
     *	Don't get hung up by an include file loop.  Arbitrary max depth
     *	of 10.	- BL
     */
    if (nesting_level > 10) {
	fprintf(stderr,
		gettext("More than %d nested lynx.cfg includes -- perhaps there is a loop?!?\n"),
		nesting_level - 1);
	fprintf(stderr,gettext("Last attempted include was '%s',\n"), cfg_filename);
	fprintf(stderr,gettext("included from '%s'.\n"), parent_filename);
	exit(EXIT_FAILURE);
    }
    /*
     *	Locate and open the file.
     */
    if (!cfg_filename || strlen(cfg_filename) == 0) {
	CTRACE((tfp,"No filename following -cfg switch!\n"));
	return;
    }
    if ((fp = LYOpenCFG(cfg_filename, parent_filename, LYNX_CFG_FILE)) == 0) {
	CTRACE((tfp, "lynx.cfg file not found as '%s'\n", cfg_filename));
	return;
    }
    have_read_cfg = TRUE;

    /*
     *	Process each line in the file.
     */
#ifdef SH_EX
    if (show_cfg) {
	time_t t;
	time(&t);
	printf("### %s %s, at %s", LYNX_NAME, LYNX_VERSION, ctime(&t));
    }
#endif
    while (LYSafeGets(&buffer, fp) != 0) {
	char *name, *value;
	char *cp;
	Config_Type *tbl;
	ParseUnionPtr q;

	/* Most lines in the config file are comment lines.  Weed them out
	 * now.  Also, leading whitespace is ok, so trim it.
	 */
	name = LYSkipBlanks(buffer);

	if (ispunct(UCH(*name)))
	    continue;

	LYTrimTrailing(name);

	if (*name == 0) continue;

	/* Significant lines are of the form KEYWORD:WHATEVER */
	if ((value = strchr (name, ':')) == 0) {
	    /* fprintf (stderr, "Bad line-- no :\n"); */
	    CTRACE((tfp, "LYReadCFG: missing ':' %s\n", name));
	    continue;
	}

	/* skip past colon, but replace ':' with 0 to make name meaningful */
	*value++ = 0;

	/*
	 *  Trim off any trailing comments.
	 *
	 *  (Apparently, the original code considers a trailing comment
	 *   valid only if preceded by a space character but is not followed
	 *   by a colon.  -- JED)
	 */
	if ((cp = strrchr (value, ':')) == 0)
	    cp = value;
	if ((cp = strchr (cp, '#')) != 0) {
	    cp--;
	    if (isspace(UCH(*cp)))
		*cp = 0;
	}

	CTRACE2(TRACE_CFG, (tfp, "LYReadCFG %s:%s\n", name, value));
	tbl = lookup_config(name);
	if (tbl->name == 0) {
	    /* lynx ignores unknown keywords */
	    CTRACE((tfp, "LYReadCFG: ignored %s:%s\n", name, value));
	    continue;
	}
#ifdef SH_EX
	if (show_cfg)
	    printf("%s:%s\n", name, value);
#endif

	if ( allowed && (*allowed)[ tbl-Config_Table ] ) {
	    if (fp0 == NULL)
		fprintf (stderr, "%s is not allowed in the %s\n",
		    name,cfg_filename);
	    /*FIXME: we can do something wiser if we are generating
	    the html representation of lynx.cfg - say include this line
	    in bold, or something...*/

	    continue;
	}

	q = ParseUnionOf(tbl);
	switch ((fp0 != 0 && tbl->type != CONF_INCLUDE)
		? CONF_UNSPECIFIED
		: tbl->type) {
	case CONF_BOOL:
	case CONF_FUN:
	case CONF_TIME:
	case CONF_ENUM:
	case CONF_INT:
	case CONF_STR:
	case CONF_ENV:
	case CONF_ENV2:
	case CONF_ADD_ITEM:
	case CONF_ADD_TRUSTED:
	    LYSetConfigValue(name, value);
	    break;

	case CONF_INCLUDE: {
	    /* include another file */
	    optidx_set_t cur_set, anded_set;
	    optidx_set_t* resultant_set = NULL;
	    char* p1, *p2, savechar;
	    BOOL any_optname_found = FALSE;

	    char *url = NULL;
	    char *cp1 = NULL;
	    char *sep = NULL;

	    if ( (p1 = strstr(value, sep = " for ")) != 0
#if defined(UNIX) && !defined(USE_DOS_DRIVES)
		|| (p1 = strstr(value, sep = ":")) != 0
#endif
	    ) {
		*p1 = '\0';
		p1 += strlen(sep);
	    }

#ifndef NO_CONFIG_INFO
	    if (fp0 != 0  &&  !no_lynxcfg_xinfo) {
		char *my_file = actual_filename(value, cfg_filename, LYNX_CFG_FILE);

		LYLocalFileToURL(&url, my_file);
		FREE(my_file);
		StrAllocCopy(cp1, value);
		if (strchr(value, '&') || strchr(value, '<')) {
		    LYEntify(&cp1, TRUE);
		}

		fprintf(fp0, "%s:<a href=\"%s\">%s</a>\n\n", name, url, cp1);
		fprintf(fp0, "    #&lt;begin  %s&gt;\n", cp1);
	    }
#endif

	    if (p1) {
		while (*(p1 = LYSkipBlanks(p1)) != 0) {
		    Config_Type *tbl2;

		    p2 = LYSkipNonBlanks(p1);
		    savechar = *p2;
		    *p2 = 0;

		    tbl2 = lookup_config(p1);
		    if (tbl2->name == 0) {
			if (fp0 == NULL)
			    fprintf (stderr, "unknown option name %s in %s\n",
				     p1, cfg_filename);
		    } else {
			unsigned i;
			if (!any_optname_found) {
			    any_optname_found = TRUE;
			    for (i = 0; i < NOPTS_; ++i)
				cur_set[i] = TRUE;
			}
			cur_set[tbl2 - Config_Table] = FALSE;
		    }
		    if (savechar && p2[1])
			p1 = p2 + 1;
		    else
			break;
		}
	    }
	    if (!allowed) {
		if (!any_optname_found)
		    resultant_set = NULL;
		else
		    resultant_set = &cur_set;
	    } else {
		if (!any_optname_found)
		    resultant_set = allowed;
		else {
		    optidx_set_AND(anded_set, *allowed, cur_set);
		    resultant_set = &anded_set;
		}
	    }

#ifndef NO_CONFIG_INFO
	    /*
	     * Now list the opts that are allowed in included file.  If all
	     * opts are allowed, then emit nothing, else emit an effective set
	     * of allowed options in <ul>.  Option names will be uppercased.
	     * FIXME:  uppercasing option names can be considered redundant.
	     */
	    if (fp0 != 0  &&  !no_lynxcfg_xinfo && resultant_set) {
		char *buf = NULL;
		unsigned i;

		fprintf(fp0,"     Options allowed in this file:\n");
		for (i = 0; i < NOPTS_; ++i) {
		    if ((*resultant_set)[i])
			continue;
		    StrAllocCopy(buf, Config_Table[i].name);
		    LYUpperCase(buf);
		    fprintf(fp0,"         * %s\n", buf);
		}
		FREE(buf);
	    }
#endif
	    do_read_cfg (value, cfg_filename, nesting_level + 1, fp0,resultant_set);

#ifndef NO_CONFIG_INFO
	    if (fp0 != 0  &&  !no_lynxcfg_xinfo) {
		fprintf(fp0, "    #&lt;end of %s&gt;\n\n", cp1);
		FREE(url);
		FREE(cp1);
	    }
#endif
	    }
	    break;

	default:
	    if (fp0 != 0) {
		if (strchr(value, '&') || strchr(value, '<')) {
		    char *cp1 = NULL;
		    StrAllocCopy(cp1, value);
		    LYEntify(&cp1, TRUE);
		    fprintf(fp0, "%s:%s\n", name, cp1);
		    FREE(cp1);
		} else {
		    fprintf(fp0, "%s:%s\n", name, value);
		}
	    }
	    break;
	}
    }

    LYCloseInput (fp);

    /*
     *	If any DOWNLOADER: commands have always_enabled set (:TRUE),
     *	make override_no_download TRUE, so that other restriction
     *	settings will not block presentation of a download menu
     *	with those always_enabled options still available. - FM
     */
    if (downloaders != 0) {
	lynx_list_item_type *cur_download;

	cur_download = downloaders;
	while (cur_download != 0) {
	    if (cur_download->always_enabled) {
		override_no_download = TRUE;
		break;
	    }
	    cur_download = cur_download->next;
	}
    }

    /*
     * If any COOKIE_{ACCEPT,REJECT}_DOMAINS have been defined,
     * process them.  These are comma delimited lists of
     * domains. - BJP
     *
     * And for query/strict/loose invalid cookie checking. - BJP
     */
    LYConfigCookies();
}

/* this is a public interface to do_read_cfg */
PUBLIC void read_cfg ARGS4(
	char *, cfg_filename,
	char *, parent_filename,
	int,	nesting_level,
	FILE *,	fp0)
{
    HTInitProgramPaths();
    do_read_cfg(cfg_filename, parent_filename, nesting_level, fp0, NULL);
}

#ifndef NO_CONFIG_INFO
PRIVATE void extra_cfg_link ARGS3(
	FILE *,	fp,
	char *,	href,
	char *,	name)
{
    fprintf(fp, "<a href=\"%s\">%s</a>",
	    href, name);
}
#endif /* NO_CONFIG_INFO */

/*
 *  Show rendered lynx.cfg data without comments, LYNXCFG:/ internal page.
 *  Called from getfile() cycle:
 *  we create and load the page just in place and return to mainloop().
 */
PUBLIC int lynx_cfg_infopage ARGS1(
    DocInfo *,		       newdoc)
{
    static char tempfile[LY_MAXPATH] = "\0";
    DocAddress WWWDoc;  /* need on exit */
    char *temp = 0;
    char *cp1 = NULL;
    FILE *fp0;


#ifndef NO_CONFIG_INFO
    /*-------------------------------------------------
     * kludge a link from LYNXCFG:/, the URL was:
     * "  <a href=\"LYNXCFG://reload\">RELOAD THE CHANGES</a>\n"
     *--------------------------------------------------*/

    if (!no_lynxcfg_xinfo && (strstr(newdoc->address, "LYNXCFG://reload"))) {
	/*
	 *  Some stuff to reload read_cfg(),
	 *  but also load options menu items and command-line options
	 *  to make things consistent.	Implemented in LYMain.c
	 */
	reload_read_cfg();

	/*
	 *  now pop-up and return to updated LYNXCFG:/ page,
	 *  remind postoptions() but much simpler:
	 */
	/*
	 *  But check whether the top history document is really
	 *  the expected LYNXCFG: page. - kw
	 */
	if (HTMainText && nhist > 0 &&
	    !strcmp(HTLoadedDocumentTitle(), LYNXCFG_TITLE) &&
	    !strcmp(HTLoadedDocumentURL(), HDOC(nhist-1).address) &&
	    LYIsUIPage(HDOC(nhist-1).address, UIP_LYNXCFG) &&
	    (!lynxcfginfo_url ||
	     strcmp(HTLoadedDocumentURL(), lynxcfginfo_url))) {
	    /*  the page was pushed, so pop-up. */
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
	     * forced reloading obsolete file again without this (overhead).
	     *
	     * Probably *no_cache was set in a wrong position because of
	     * the internal page...
	     */
	    if (!HTLoadAbsolute(&WWWDoc))
		return(NOT_FOUND);

	    HTuncache_current_document();  /* will never use again */
	    LYUnRegisterUIPage(UIP_LYNXCFG);
	}

	/*  now set up the flag and fall down to create a new LYNXCFG:/ page */
	FREE(lynxcfginfo_url);	/* see below */
    }
#endif /* !NO_CONFIG_INFO */

    /*
     * We regenerate the file if reloading has been requested (with
     * LYK_NOCACHE key).  If we did not regenerate, there would be no
     * way to recover in a session from a situation where the file is
     * corrupted (for example truncated because the file system was full
     * when it was first created - lynx doesn't check for write errors
     * below), short of manual complete removal or perhaps forcing
     * regeneration with LYNXCFG://reload.  Similarly, there would be no
     * simple way to get a different page if user_mode has changed to
     * Advanced after the file was first generated in a non-Advanced mode
     * (the difference being in whether the page includes the link to
     * LYNXCFG://reload or not).
     * We also try to regenerate the file if lynxcfginfo_url is set,
     * indicating that tempfile is valid, but the file has disappeared anyway.
     * This can happen to a long-lived lynx process if for example some system
     * script periodically cleans up old files in the temp file space. - kw
     */

    if (LYforce_no_cache && reloading) {
	FREE(lynxcfginfo_url); /* flag to code below to regenerate - kw */
    } else if (lynxcfginfo_url != NULL) {
	if (!LYCanReadFile(tempfile)) { /* check existence */
	    FREE(lynxcfginfo_url); /* flag to code below to try again - kw */
	}
    }
    if (lynxcfginfo_url == 0) {

	if ((fp0 = InternalPageFP(tempfile, TRUE)) == 0)
	    return(NOT_FOUND);

	LYLocalFileToURL(&lynxcfginfo_url, tempfile);

	LYforce_no_cache = TRUE;  /* don't cache this doc */

	BeginInternalPage (fp0, LYNXCFG_TITLE, NULL);
	fprintf(fp0, "<pre>\n");

#ifndef NO_CONFIG_INFO
	if (!no_lynxcfg_xinfo) {
#if defined(HAVE_CONFIG_H) || defined(VMS)
	    if (strcmp(lynx_cfg_file, LYNX_CFG_FILE)) {
		fprintf(fp0, "<em>%s\n%s",
			     gettext("The following is read from your lynx.cfg file."),
			     gettext("Please read the distribution"));
		LYLocalFileToURL(&temp, LYNX_CFG_FILE);
		fprintf(fp0, " <a href=\"%s\">lynx.cfg</a> ",
			     temp);
		FREE(temp);
		fprintf(fp0, "%s</em>\n\n",
			     gettext("for more comments."));
	    } else
#endif /* HAVE_CONFIG_H */
	    {
	    /* no absolute path... for lynx.cfg on DOS/Win32 */
		fprintf(fp0, "<em>%s\n%s",
			     gettext("The following is read from your lynx.cfg file."),
			     gettext("Please read the distribution"));
		fprintf(fp0, " </em>lynx.cfg<em> ");
		fprintf(fp0, "%s</em>\n",
			     gettext("for more comments."));
	    }

#ifndef NO_CONFIG_INFO
#if defined(HAVE_CONFIG_H) && defined(USE_COLOR_STYLE)
	    if (!no_compileopts_info && !no_lynxcfg_xinfo) {
		fprintf(fp0, "%s</pre><ul><li>", SEE_ALSO);
		extra_cfg_link(fp0, STR_LYNXCFLAGS, COMPILE_OPT_SEGMENT);

		fprintf(fp0, "<li>");
		LYLocalFileToURL(&temp, lynx_lss_file);
		extra_cfg_link(fp0, temp, COLOR_STYLE_SEGMENT);
		fprintf(fp0, "</ul><pre>\n");
	    } else
#endif
	    {
		fprintf(fp0, "%s ", SEE_ALSO);
#if defined(HAVE_CONFIG_H)
		if (!no_compileopts_info) {
		    extra_cfg_link(fp0, STR_LYNXCFLAGS, COMPILE_OPT_SEGMENT);
		}
#endif
#if defined(USE_COLOR_STYLE)
		if (!no_lynxcfg_xinfo) {
		    LYLocalFileToURL(&temp, lynx_lss_file);
		    extra_cfg_link(fp0, temp, COLOR_STYLE_SEGMENT);
		}
#endif
		fprintf(fp0, "\n\n");
	    }
#endif /* NO_CONFIG_INFO */

	    /** a new experimental link ... **/
	    if (user_mode == ADVANCED_MODE)
		fprintf(fp0, "  <a href=\"%s//reload\">%s</a>\n",
			     STR_LYNXCFG,
			     gettext("RELOAD THE CHANGES"));


	    LYLocalFileToURL(&temp, lynx_cfg_file);
	    StrAllocCopy(cp1, lynx_cfg_file);
	    if (strchr(lynx_cfg_file, '&') || strchr(lynx_cfg_file, '<')) {
		LYEntify(&cp1, TRUE);
	    }
	    fprintf(fp0, "\n    #<em>%s <a href=\"%s\">%s</a></em>\n",
			gettext("Your primary configuration"),
			temp,
			cp1);
	    FREE(temp);
	    FREE(cp1);

	} else
#endif /* !NO_CONFIG_INFO */

	fprintf(fp0, "<em>%s</em>\n\n", gettext("The following is read from your lynx.cfg file."));

	/*
	 *  Process the configuration file.
	 */
	read_cfg(lynx_cfg_file, "main program", 1, fp0);

	fprintf(fp0, "</pre>\n");
	EndInternalPage(fp0);
	LYCloseTempFP(fp0);
	LYRegisterUIPage(lynxcfginfo_url, UIP_LYNXCFG);
    }

    /* return to getfile() cycle */
    StrAllocCopy(newdoc->address, lynxcfginfo_url);
    WWWDoc.address = newdoc->address;
    WWWDoc.post_data = newdoc->post_data;
    WWWDoc.post_content_type = newdoc->post_content_type;
    WWWDoc.bookmark = newdoc->bookmark;
    WWWDoc.isHEAD = newdoc->isHEAD;
    WWWDoc.safe = newdoc->safe;

    if (!HTLoadAbsolute(&WWWDoc))
	return(NOT_FOUND);
#ifdef DIRED_SUPPORT
    lynx_edit_mode = FALSE;
#endif /* DIRED_SUPPORT */
    return(NORMAL);
}


#if defined(HAVE_CONFIG_H) && !defined(NO_CONFIG_INFO)
/*
 *  Compile-time definitions info, LYNXCOMPILEOPTS:/ internal page,
 *  from getfile() cycle.
 */
PUBLIC int lynx_compile_opts ARGS1(
    DocInfo *,		       newdoc)
{
    static char tempfile[LY_MAXPATH] = "\0";
#define PutDefs(table, N) fprintf(fp0, "%-35s %s\n", table[N].name, table[N].value)
#include <cfg_defs.h>
    unsigned n;
    DocAddress WWWDoc;  /* need on exit */
    FILE *fp0;

    /* In general, create the page only once - compile-time data will not
     * change...  But we will regenerate the file anyway, in two situations:
     * (a) configinfo_url has been FREEd - this can happen if free_lynx_cfg()
     * was called as part of a LYNXCFG://reload action.
     * (b) reloading has been requested (with LYK_NOCACHE key).  If we did
     * not regenerate, there would be no way to recover in a session from
     * a situation where the file is corrupted (for example truncated because
     * the file system was full when it was first created - lynx doesn't
     * check for write errors below), short of manual complete removal or
     * forcing regeneration with LYNXCFG://reload.
     * (c) configinfo_url is set, indicating that tempfile is valid, but
     * the file has disappeared anyway.  This can happen to a long-lived lynx
     * process if for example some system script periodically cleans up old
     * files in the temp file space. - kw
     */

    if (LYforce_no_cache && reloading) {
	FREE(configinfo_url); /* flag to code below to regenerate - kw */
    } else if (configinfo_url != NULL) {
	if (!LYCanReadFile(tempfile)) { /* check existence */
	    FREE(configinfo_url); /* flag to code below to try again - kw */
	}
    }
    if (configinfo_url == NULL) {
	if ((fp0 = InternalPageFP(tempfile, TRUE)) == 0)
	    return(NOT_FOUND);

	LYLocalFileToURL(&configinfo_url, tempfile);

	BeginInternalPage (fp0, CONFIG_DEF_TITLE, NULL);
	fprintf(fp0, "<pre>\n");

	fprintf(fp0, "\n%s<br>\n<em>config.cache</em>\n", AUTOCONF_CONFIG_CACHE);
	for (n = 0; n < TABLESIZE(config_cache); n++) {
	    PutDefs(config_cache, n);
	}
	fprintf(fp0, "\n%s<br>\n<em>lynx_cfg.h</em>\n", AUTOCONF_LYNXCFG_H);
	for (n = 0; n < TABLESIZE(config_defines); n++) {
	    PutDefs(config_defines, n);
	}
	fprintf(fp0, "</pre>\n");
	EndInternalPage(fp0);
	LYCloseTempFP(fp0);
	LYRegisterUIPage(configinfo_url, UIP_CONFIG_DEF);
    }

    /* exit to getfile() cycle */
    StrAllocCopy(newdoc->address, configinfo_url);
    WWWDoc.address = newdoc->address;
    WWWDoc.post_data = newdoc->post_data;
    WWWDoc.post_content_type = newdoc->post_content_type;
    WWWDoc.bookmark = newdoc->bookmark;
    WWWDoc.isHEAD = newdoc->isHEAD;
    WWWDoc.safe = newdoc->safe;

    if (!HTLoadAbsolute(&WWWDoc))
	return(NOT_FOUND);
#ifdef DIRED_SUPPORT
    lynx_edit_mode = FALSE;
#endif /* DIRED_SUPPORT */
    return(NORMAL);
}
#endif /* !NO_CONFIG_INFO */
