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
extern int HTNewsMaxChunk;  /* Max news articles before chunking (HTNews.c) */
extern int HTNewsChunkSize; /* Number of news articles per chunk (HTNews.c) */
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
	    if ((next_colon = strchr(colon,':')) != 0)
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

#if USE_DEFAULT_COLORS
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
#if USE_DEFAULT_COLORS
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

#if defined(USE_COLOR_TABLE)

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

#ifdef SOURCE_CACHE
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

PRIVATE BOOLEAN LYgetEnum ARGS3(
    Config_Enum *,	table,
    CONST char *,	name,
    int *,		result)
{
    Config_Enum *found = 0;
    unsigned len = strlen(name);

    if (len != 0) {
	while (table->name != 0) {
	    if (!strncasecomp(table->name, name, len)) {
		if (found != 0)
		    return FALSE; /* ambiguous, don't use this */
		found = table;
	    }
	    table++;
	}
	if (found != 0) {
	    *result = found->value;
	    return TRUE;
	}
    }
    return FALSE;		/* no match */
}

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
#if USE_SLANG
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
    if ( !value || !*value) return 0; /* silently ignoring*/

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

    e = strchr(s,':');
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

/* This table is searched ignoring case */
PRIVATE Config_Type Config_Table [] =
{ 
     PARSE_SET("accept_all_cookies",   LYAcceptAllCookies),
     PARSE_TIM("alertsecs",            AlertSecs),
     PARSE_SET("always_resubmit_posts", LYresubmit_posts),
#ifdef EXEC_LINKS
     PARSE_DEF("always_trusted_exec",  ALWAYS_EXEC_PATH),
#endif
     PARSE_FUN("assume_charset",       assume_charset_fun),
     PARSE_FUN("assume_local_charset", assume_local_charset_fun),
     PARSE_FUN("assume_unrec_charset", assume_unrec_charset_fun),
#ifdef EXP_ASSUMED_COLOR
     PARSE_FUN("assumed_color",        assumed_color_fun),
#endif
#ifdef EXP_CHARSET_CHOICE
     PARSE_FUN("assumed_doc_charset_choice", parse_assumed_doc_charset_choice),
#endif
#ifdef DIRED_SUPPORT
     PARSE_INT("auto_uncache_dirlists", LYAutoUncacheDirLists),
#endif
#ifndef DISABLE_BIBP
     PARSE_STR("bibp_bibhost",         BibP_bibhost),
     PARSE_STR("bibp_globalserver",    BibP_globalserver),
#endif
     PARSE_SET("block_multi_bookmarks", LYMBMBlocked),
     PARSE_SET("bold_h1",              bold_H1),
     PARSE_SET("bold_headers",         bold_headers),
     PARSE_SET("bold_name_anchors",    bold_name_anchors),
     PARSE_SET("case_sensitive_always_on", case_sensitive),
     PARSE_FUN("character_set",        character_set_fun),
#ifdef CAN_SWITCH_DISPLAY_CHARSET
     PARSE_STR("charset_switch_rules", charset_switch_rules),
     PARSE_STR("charsets_directory",   charsets_directory),
#endif
     PARSE_SET("checkmail",            check_mail),
     PARSE_SET("collapse_br_tags",     LYCollapseBRs),
#ifdef USE_COLOR_TABLE
     PARSE_FUN("color",                color_fun),
#endif
#ifndef __DJGPP__
     PARSE_INT("connect_timeout",      connect_timeout),
#endif
     PARSE_STR("cookie_accept_domains", LYCookieSAcceptDomains),
#ifdef EXP_PERSISTENT_COOKIES
     PARSE_STR("cookie_file",          LYCookieFile),
#endif /* EXP_PERSISTENT_COOKIES */
     PARSE_STR("cookie_loose_invalid_domains", LYCookieSLooseCheckDomains),
     PARSE_STR("cookie_query_invalid_domains", LYCookieSQueryCheckDomains),
     PARSE_STR("cookie_reject_domains", LYCookieSRejectDomains),
#ifdef EXP_PERSISTENT_COOKIES
     PARSE_STR("cookie_save_file",     LYCookieSaveFile),
#endif /* EXP_PERSISTENT_COOKIES */
     PARSE_STR("cookie_strict_invalid_domains", LYCookieSStrictCheckDomains),
     PARSE_Env("cso_proxy", 0 ),
#ifdef VMS
     PARSE_STR("cswing_path",          LYCSwingPath),
#endif
     PARSE_FUN("default_bookmark_file", default_bookmark_file_fun),
     PARSE_FUN("default_cache_size",   default_cache_size_fun),
     PARSE_FUN("default_editor",       default_editor_fun),
     PARSE_STR("default_index_file",   indexfile),
     PARSE_ENU("default_keypad_mode",  keypad_mode, tbl_keypad_mode),
     PARSE_FUN("default_keypad_mode_is_numbers_as_arrows", numbers_as_arrows_fun),
     PARSE_ENU("default_user_mode",    user_mode, tbl_user_mode),
#if defined(VMS) && defined(VAXC) && !defined(__DECC)
     PARSE_INT("default_virtual_memory_size", HTVirtualMemorySize),
#endif
#ifdef DIRED_SUPPORT
     PARSE_FUN("dired_menu",           dired_menu_fun),
#endif
#ifdef EXP_CHARSET_CHOICE
     PARSE_FUN("display_charset_choice", parse_display_charset_choice),
#endif
     PARSE_ADD("downloader",           downloaders),
     PARSE_SET("emacs_keys_always_on", emacs_keys),
     PARSE_FUN("enable_lynxrc",        enable_lynxrc),
     PARSE_SET("enable_scrollback",    enable_scrollback),
#ifdef USE_EXTERNALS
     PARSE_ADD("external",             externals),
#endif
     PARSE_Env("finger_proxy",         0 ),
#if defined(_WINDOWS)	/* 1998/10/05 (Mon) 17:34:15 */
     PARSE_SET("focus_window",         focus_window),
#endif
     PARSE_SET("force_8bit_toupper",   UCForce8bitTOUPPER),
     PARSE_SET("force_empty_hrefless_a", force_empty_hrefless_a),
     PARSE_SET("force_ssl_cookies_secure", LYForceSSLCookiesSecure),
#if !defined(NO_OPTION_FORMS) && !defined(NO_OPTION_MENU)
     PARSE_SET("forms_options",        LYUseFormsOptions),
#endif
     PARSE_SET("ftp_passive",          ftp_passive),
     PARSE_Env("ftp_proxy",            0 ),
     PARSE_STR("global_extension_map", global_extension_map),
     PARSE_STR("global_mailcap",       global_type_map),
     PARSE_Env("gopher_proxy",         0 ),
     PARSE_SET("gotobuffer",           goto_buffer),
     PARSE_STR("helpfile",             helpfile),
#ifdef MARK_HIDDEN_LINKS
     PARSE_STR("hidden_link_marker",   hidden_link_marker),
#endif
     PARSE_SET("historical_comments",  historical_comments),
#ifdef USE_PRETTYSRC
     PARSE_FUN("htmlsrc_attrname_xform", read_htmlsrc_attrname_xform),
     PARSE_FUN("htmlsrc_tagname_xform", read_htmlsrc_tagname_xform),
#endif
     PARSE_Env("http_proxy",           0 ),
     PARSE_Env("https_proxy",          0 ),
     PARSE_REQ("include",              0),
     PARSE_TIM("infosecs",             InfoSecs),
     PARSE_STR("jump_prompt",          jumpprompt),
     PARSE_SET("jumpbuffer",           jump_buffer),
     PARSE_FUN("jumpfile",             jumpfile_fun),
#ifdef EXP_JUSTIFY_ELTS
     PARSE_SET("justify",              ok_justify),
     PARSE_INT("justify_max_void_percent", justify_max_void_percent),
#endif
#ifdef EXP_KEYBOARD_LAYOUT
     PARSE_FUN("keyboard_layout",      keyboard_layout_fun),
#endif
     PARSE_FUN("keymap",               keymap_fun),
     PARSE_SET("leftarrow_in_textfield_prompt", textfield_prompt_at_left_edge),
#ifndef VMS
     PARSE_STR("list_format",          list_format),
#endif
#ifndef DISABLE_NEWS
     PARSE_SET("list_news_dates",      LYListNewsDates),
     PARSE_SET("list_news_numbers",    LYListNewsNumbers),
#endif
     PARSE_STR("local_domain",         LYLocalDomain),
#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
     PARSE_SET("local_execution_links_always_on", local_exec),
     PARSE_SET("local_execution_links_on_but_not_remote", local_exec_on_local_files),
#endif
     PARSE_FUN("localhost_alias",      localhost_alias_fun),
     PARSE_STR("lynx_host_name",       LYHostName),
     PARSE_FUN("lynx_sig_file",        lynx_sig_file_fun),
#ifdef LYNXCGI_LINKS
#ifndef VMS
     PARSE_STR("lynxcgi_document_root", LYCgiDocumentRoot),
#endif
     PARSE_FUN("lynxcgi_environment",  lynxcgi_environment_fun),
#endif
#if USE_VMS_MAILER
     PARSE_STR("mail_adrs",            mail_adrs),
#endif
     PARSE_SET("mail_system_error_logging", error_logging),
     PARSE_SET("make_links_for_all_images", clickable_images),
     PARSE_SET("make_pseudo_alts_for_inlines", pseudo_inline_alts),
     PARSE_TIM("messagesecs",          MessageSecs),
     PARSE_SET("minimal_comments",     minimal_comments),
     PARSE_ENU("multi_bookmark_support", LYMultiBookmarks, tbl_multi_bookmarks),
     PARSE_SET("ncr_in_bookmarks",     UCSaveBookmarksInUnicode),
#ifndef DISABLE_NEWS
     PARSE_FUN("news_chunk_size",      news_chunk_size_fun),
     PARSE_FUN("news_max_chunk",       news_max_chunk_fun),
     PARSE_FUN("news_posting",         news_posting_fun),
     PARSE_Env("news_proxy",           0),
     PARSE_Env("newspost_proxy",       0),
     PARSE_Env("newsreply_proxy",      0),
     PARSE_Env("nntp_proxy",           0),
     PARSE_ENV("nntpserver",           0), /* actually NNTPSERVER */
#endif
     PARSE_SET("no_dot_files",         no_dotfiles),
     PARSE_SET("no_file_referer",      no_filereferer),
#ifndef VMS
     PARSE_SET("no_forced_core_dump",  LYNoCore),
#endif
     PARSE_SET("no_from_header",       LYNoFromHeader),
     PARSE_SET("no_ismap_if_usemap",   LYNoISMAPifUSEMAP),
     PARSE_Env("no_proxy",             0 ),
     PARSE_SET("no_referer_header",    LYNoRefererHeader),
#ifdef SH_EX
     PARSE_SET("no_table_center",      no_table_center),
#endif
     PARSE_FUN("nonrestarting_sigwinch", nonrest_sigwinch_fun),
     PARSE_FUN("outgoing_mail_charset", outgoing_mail_charset_fun),
#ifdef DISP_PARTIAL
     PARSE_SET("partial",              display_partial_flag),
     PARSE_INT("partial_thres",        partial_threshold),
#endif
#ifdef EXP_PERSISTENT_COOKIES
     PARSE_SET("persistent_cookies",   persistent_cookies),
#endif /* EXP_PERSISTENT_COOKIES */
     PARSE_STR("personal_extension_map", personal_extension_map),
     PARSE_STR("personal_mailcap",     personal_type_map),
     PARSE_STR("preferred_charset",    pref_charset),
     PARSE_STR("preferred_language",   language),
     PARSE_SET("prepend_base_to_source", LYPrependBaseToSource),
     PARSE_SET("prepend_charset_to_source", LYPrependCharsetToSource),
#ifdef USE_PRETTYSRC
     PARSE_SET("prettysrc",            LYpsrc),
     PARSE_FUN("prettysrc_spec",       psrcspec_fun),
     PARSE_SET("prettysrc_view_no_anchor_numbering", psrcview_no_anchor_numbering),
#endif
     PARSE_FUN("printer",              printer_fun),
     PARSE_SET("quit_default_yes",     LYQuitDefaultYes),
     PARSE_FUN("referer_with_query",   referer_with_query_fun),
     PARSE_SET("reuse_tempfiles",      LYReuseTempfiles),
#ifndef NO_RULES
     PARSE_FUN("rule",                 HTSetConfiguration),
     PARSE_FUN("rulesfile",            cern_rulesfile_fun),
#endif /* NO_RULES */
     PARSE_STR("save_space",           lynx_save_space),
     PARSE_SET("scan_for_buried_news_refs", scan_for_buried_news_references),
#ifdef USE_SCROLLBAR
     PARSE_SET("scrollbar",            LYsb),
     PARSE_SET("scrollbar_arrow",      LYsb_arrow),
#endif
     PARSE_SET("seek_frag_area_in_cur", LYSeekFragAREAinCur),
     PARSE_SET("seek_frag_map_in_cur", LYSeekFragMAPinCur),
     PARSE_SET("set_cookies",          LYSetCookies),
     PARSE_SET("show_cursor",          LYShowCursor),
     PARSE_ENU("show_kb_rate",         LYTransferRate, tbl_transfer_rate),
     PARSE_Env("snews_proxy",          0 ),
     PARSE_Env("snewspost_proxy",      0 ),
     PARSE_Env("snewsreply_proxy",     0 ),
     PARSE_SET("soft_dquotes",         soft_dquotes),
#ifdef SOURCE_CACHE
     PARSE_ENU("source_cache",         LYCacheSource, tbl_source_cache),
     PARSE_ENU("source_cache_for_aborted", LYCacheSourceForAborted, tbl_abort_source_cache),
#endif
     PARSE_STR("startfile",            startfile),
     PARSE_SET("strip_dotdot_urls",    LYStripDotDotURLs),
     PARSE_SET("substitute_underscores", use_underscore),
     PARSE_FUN("suffix",               suffix_fun),
     PARSE_FUN("suffix_order",         suffix_order_fun),
     PARSE_FUN("system_editor",        system_editor_fun),
     PARSE_STR("system_mail",          system_mail),
     PARSE_STR("system_mail_flags",    system_mail_flags),
     PARSE_ENU("tagsoup",              Old_DTD, tbl_DTD_recovery),
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
     PARSE_SET("textfields_need_activation", textfields_activation_option),
#endif
#if defined(_WINDOWS)
     PARSE_INT("timeout",              lynx_timeout),
#endif
     PARSE_SET("trim_input_fields",  LYtrimInputFields),
#ifdef EXEC_LINKS
     PARSE_DEF("trusted_exec",         EXEC_PATH),
#endif
#ifdef LYNXCGI_LINKS
     PARSE_DEF("trusted_lynxcgi",      CGI_PATH),
#endif
#ifdef DIRED_SUPPORT
     PARSE_ADD("uploader",             uploaders),
#endif
     PARSE_STR("url_domain_prefixes",  URLDomainPrefixes),
     PARSE_STR("url_domain_suffixes",  URLDomainSuffixes),
#ifdef VMS
     PARSE_SET("use_fixed_records",    UseFixedRecords),
#endif
#if defined(USE_MOUSE)
     PARSE_SET("use_mouse",            LYUseMouse),
#endif
     PARSE_SET("use_select_popups",    LYSelectPopups),
     PARSE_SET("verbose_images",       verbose_img),
     PARSE_SET("vi_keys_always_on",    vi_keys),
     PARSE_FUN("viewer",               viewer_fun),
     PARSE_Env("wais_proxy",           0 ),
     PARSE_STR("xloadimage_command",   XLoadImageCommand),

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
    static char *my_filename;

    if (my_filename != 0) {
	FREE(my_filename);
    }
    if (!LYisAbsPath(cfg_filename)
     && !(parent_filename == 0 && LYCanReadFile(cfg_filename))) {
	if (!strncmp(cfg_filename, "~/", 2)) {
	    HTSprintf0(&my_filename, "%s%s", Home_Dir(), cfg_filename+1);
	    cfg_filename = my_filename;
	} else {
	    if (parent_filename != 0) {
		StrAllocCopy(my_filename, parent_filename);
		*LYPathLeaf (my_filename) = '\0';
		StrAllocCat(my_filename, cfg_filename);
	    }
	    if (my_filename != 0 && LYCanReadFile(my_filename)) {
		cfg_filename = my_filename;
	    } else {
		StrAllocCopy(my_filename, dft_filename);
		*LYPathLeaf (my_filename) = '\0';
		StrAllocCat(my_filename, cfg_filename);
		if (LYCanReadFile(my_filename)) {
		    cfg_filename = my_filename;
		}
	    }
	}
    }
    return cfg_filename;
}

PUBLIC FILE *LYOpenCFG ARGS3(
    char *,	cfg_filename,
    char *,	parent_filename,
    char *,	dft_filename)
{
    cfg_filename = actual_filename(cfg_filename, parent_filename, dft_filename);
    CTRACE((tfp, "opening config file %s\n", cfg_filename));
    return fopen(cfg_filename, TXT_R);
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

	tbl = lookup_config(name);
	if (tbl->name == 0) {
	    /* lynx ignores unknown keywords */
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
#ifdef HAVE_NAPMS
		    ival *= 1000;
#endif
		    *(q->int_value) = (int) ival;
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

	    if (getenv (name) == 0) {
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

	case CONF_INCLUDE: {
	    /* include another file */
	    optidx_set_t cur_set, anded_set;
	    optidx_set_t* resultant_set = NULL;
	    char* p1, *p2, savechar;
	    BOOL any_optname_found = FALSE;

	    char *url = NULL;
	    char *cp1 = NULL;
	    char *sep = NULL;

	    if ( (p1 = strstr(value, sep=" for ")) != 0
#if defined(UNIX) && !defined(__EMX__)
		|| (p1 = strstr(value, sep=":")) != 0
#endif
	    ) {
		*p1 = '\0';
		p1 += strlen(sep);
	    }

#ifndef NO_CONFIG_INFO
	    if (fp0 != 0  &&  !no_lynxcfg_xinfo) {
		LYLocalFileToURL(&url, actual_filename(value, cfg_filename, LYNX_CFG_FILE));
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

	case CONF_ADD_ITEM:
	    if (q->add_value != 0)
		add_item_to_list (value, q->add_value, FALSE);
	    break;

#if defined(EXEC_LINKS) || defined(LYNXCGI_LINKS)
	case CONF_ADD_TRUSTED:
	    add_trusted (value, q->def_value);
	    break;
#endif
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
    do_read_cfg(cfg_filename, parent_filename, nesting_level, fp0, NULL);
}


/*
 *  Show rendered lynx.cfg data without comments, LYNXCFG:/ internal page.
 *  Called from getfile() cycle:
 *  we create and load the page just in place and return to mainloop().
 */
PUBLIC int lynx_cfg_infopage ARGS1(
    document *,		       newdoc)
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
	    !strcmp(HTLoadedDocumentURL(), history[nhist-1].address) &&
	    LYIsUIPage(history[nhist-1].address, UIP_LYNXCFG) &&
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

	if (LYReuseTempfiles) {
	    fp0 = LYOpenTempRewrite(tempfile, HTML_SUFFIX, "w");
	} else {
	    if (tempfile[0])
		LYRemoveTemp(tempfile);
	    fp0 = LYOpenTemp(tempfile, HTML_SUFFIX, "w");
	}
	if (fp0 == NULL) {
	    HTAlert(CANNOT_OPEN_TEMP);
	    return(NOT_FOUND);
	}
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

#if defined(HAVE_CONFIG_H) && !defined(NO_CONFIG_INFO)
	    if (!no_compileopts_info) {
		fprintf(fp0, "%s <a href=\"LYNXCOMPILEOPTS:\">%s</a>\n\n",
			SEE_ALSO,
			COMPILE_OPT_SEGMENT);
	    }
#endif

	    /** a new experimental link ... **/
	    if (user_mode == ADVANCED_MODE)
		fprintf(fp0, "  <a href=\"LYNXCFG://reload\">%s</a>\n",
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
    document *,		       newdoc)
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
	if (LYReuseTempfiles) {
	    fp0 = LYOpenTempRewrite(tempfile, HTML_SUFFIX, "w");
	} else {
	    LYRemoveTemp(tempfile);
	    fp0 = LYOpenTemp(tempfile, HTML_SUFFIX, "w");
	}
	if (fp0 == NULL) {
	    HTAlert(CANNOT_OPEN_TEMP);
	    return(NOT_FOUND);
	}
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
