#ifndef NO_RULES
#include <HTRules.h>
#else
#include <HTUtils.h>
#endif
#include <HTFile.h>
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
PRIVATE int is_true ARGS1(
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

/*
 *  Function for freeing the DOWNLOADER and UPLOADER menus list. - FM
 */
PRIVATE void free_item_list NOARGS
{
    lynx_html_item_type *cur;
    lynx_html_item_type *next;

    cur = downloaders;
    while (cur) {
	next = cur->next;
	FREE(cur->name);
	FREE(cur->command);
	FREE(cur);
	cur = next;
    }
    downloaders = NULL;

#ifdef DIRED_SUPPORT
    cur = uploaders;
    while (cur) {
	next = cur->next;
	FREE(cur->name);
	FREE(cur->command);
	FREE(cur);
	cur = next;
    }
    uploaders = NULL;
#endif /* DIRED_SUPPORT */

#ifdef USE_EXTERNALS
    cur = externals;
    while (cur) {
	next = cur->next;
	FREE(cur->name);
	FREE(cur->command);
	FREE(cur);
	cur = next;
    }
    externals = NULL;
#endif /* USE_EXTERNALS */

    return;
}

/*
 *  Process string buffer fields for DOWNLOADER or UPLOADER menus.
 */
PRIVATE void add_item_to_list ARGS2(
	char *,			buffer,
	lynx_html_item_type **, list_ptr)
{
    char *colon, *next_colon;
    lynx_html_item_type *cur_item, *prev_item;

    /*
     *	Make a linked list
     */
    if (*list_ptr == NULL) {
	/*
	 *  First item.
	 */
	cur_item = (lynx_html_item_type *)calloc(sizeof(lynx_html_item_type),1);
	if (cur_item == NULL)
	    outofmem(__FILE__, "read_cfg");
	*list_ptr = cur_item;
#ifdef LY_FIND_LEAKS
	atexit(free_item_list);
#endif
    } else {
	/*
	 *  Find the last item.
	 */
	for (prev_item = *list_ptr;
	     prev_item->next != NULL;
	     prev_item = prev_item->next)
	    ;  /* null body */
	cur_item = (lynx_html_item_type *)calloc(sizeof(lynx_html_item_type),1);
	if (cur_item == NULL)
	    outofmem(__FILE__, "read_cfg");
	else
	    prev_item->next = cur_item;
    }
    cur_item->next = NULL;
    cur_item->name = NULL;
    cur_item->command = NULL;
    cur_item->always_enabled = FALSE;

    /*
     *	Find first unescaped colon and process fields
     */
    if ((colon = find_colon(buffer)) != NULL) {
	/*
	 *  Process name field
	 */
	cur_item->name = (char *)calloc((colon-buffer+1),sizeof(char));
	if (cur_item->name == NULL)
	    outofmem(__FILE__, "read_cfg");
	LYstrncpy(cur_item->name, buffer, (int)(colon-buffer));
	remove_backslashes(cur_item->name);

	/*
	 *  Process TRUE/FALSE field.  If we do not find one, assume it is
	 *  true.  In any case, we want the command string.
	 */
	if ((next_colon = find_colon(colon+1)) == NULL) {
	    next_colon = colon + strlen(colon);
	}
	if (next_colon - (colon+1) > 0) {
	    cur_item->command = (char *)calloc(next_colon-colon, sizeof(char));
	    if (cur_item->command == NULL)
		outofmem(__FILE__, "read_cfg");
	    LYstrncpy(cur_item->command, colon+1, (int)(next_colon-(colon+1)));
	    remove_backslashes(cur_item->command);
	    cur_item->always_enabled = TRUE;
	}
	if (*next_colon++) {
	    cur_item->always_enabled = is_true(next_colon);
	}
    }
}


/*
 *  Function for freeing the PRINTER menus list. - FM
 */
PRIVATE void free_printer_item_list NOARGS
{
    lynx_printer_item_type *cur = printers;
    lynx_printer_item_type *next;

    while (cur) {
	next = cur->next;
	FREE(cur->name);
	FREE(cur->command);
	FREE(cur);
	cur = next;
    }
    printers = NULL;

    return;
}

/*
 *  Process string buffer fields for PRINTER menus.
 */
PRIVATE void add_printer_to_list ARGS2(
	char *,				buffer,
	lynx_printer_item_type **,	list_ptr)
{
    char *colon, *next_colon;
    lynx_printer_item_type *cur_item, *prev_item;

    /*
     *	Make a linked list.
     */
    if (*list_ptr == NULL) {
	/*
	 *  First item.
	 */
	cur_item = (lynx_printer_item_type *)calloc(sizeof(lynx_printer_item_type),1);
	if (cur_item == NULL)
	    outofmem(__FILE__, "read_cfg");
	*list_ptr = cur_item;
#ifdef LY_FIND_LEAKS
	atexit(free_printer_item_list);
#endif
    } else {
	/*
	 *  Find the last item.
	 */
	for (prev_item = *list_ptr;
	     prev_item->next != NULL;
	     prev_item = prev_item->next)
	    ;  /* null body */

	cur_item = (lynx_printer_item_type *)calloc(sizeof(lynx_printer_item_type),1);
	if (cur_item == NULL)
	    outofmem(__FILE__, "read_cfg");
	else
	    prev_item->next = cur_item;
    }
    cur_item->next = NULL;
    cur_item->name = NULL;
    cur_item->command = NULL;
    cur_item->always_enabled = FALSE;

    /*
     *	Find first unescaped colon and process fields.
     */
    if ((colon = find_colon(buffer)) != NULL) {
	/*
	 *  Process name field.
	 */
	cur_item->name = (char *)calloc((colon-buffer+1), sizeof(char));
	if (cur_item->name == NULL)
	    outofmem(__FILE__, "read_cfg");
	LYstrncpy(cur_item->name, buffer, (int)(colon-buffer));
	remove_backslashes(cur_item->name);

	/*
	 *  Process TRUE/FALSE field.
	 */
	if ((next_colon = find_colon(colon+1)) != NULL) {
	    cur_item->command = (char *)calloc(next_colon-colon, sizeof(char));
	    if (cur_item->command == NULL)
		outofmem(__FILE__, "read_cfg");
	    LYstrncpy(cur_item->command, colon+1, (int)(next_colon-(colon+1)));
	    remove_backslashes(cur_item->command);
	    cur_item->always_enabled = is_true(next_colon+1);
	}

	/*
	 *  Process pagelen field.
	 */
	if (next_colon != NULL
	 && (next_colon = find_colon(next_colon+1)) != NULL) {
	    cur_item->pagelen = atoi(next_colon+1);
	} else {
	    /* default to 66 lines */
	    cur_item->pagelen = 66;
	}
    }
}

#if defined(USE_COLOR_STYLE) || defined(USE_COLOR_TABLE)

#ifdef USE_SLANG
#define COLOR_WHITE 7
#define COLOR_BLACK 0
#endif

int default_fg = COLOR_WHITE;
int default_bg = COLOR_BLACK;

static CONST char *Color_Strings[16] =
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

#ifdef DOSPATH
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

/*
 *  Validator for COLOR fields.
 */
PUBLIC int check_color ARGS2(
	char *, color,
	int,	the_default)
{
    int i;

    if (!strcasecomp(color, "default"))
	return the_default;
    if (!strcasecomp(color, "nocolor"))
	return NO_COLOR;

    for (i = 0; i < 16; i++) {
	if (!strcasecomp(color, Color_Strings[i]))
	    return ColorCode(i);
    }
    return ERR_COLOR;
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
    exit_immediately(-1);
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
    if ((check_color(fg, default_fg) < 0) ||
	(check_color(bg, default_bg) < 0))
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

typedef int (*ParseFunc) PARAMS((char *));

typedef union {
	lynx_html_item_type ** add_value;
	BOOLEAN * set_value;
	int *	  int_value;
	char **   str_value;
	ParseFunc fun_value;
	long	  def_value;
} ConfigUnion;

#ifdef	PARSE_DEBUG
#define ParseData \
	lynx_html_item_type** add_value; \
	BOOLEAN *set_value; \
	int *int_value; \
	char **str_value; \
	ParseFunc fun_value; \
	long def_value
#define PARSE_ADD(n,t,v) {n,t,	 &v,  0,  0,  0,  0,  0}
#define PARSE_SET(n,t,v) {n,t,	  0,  v,  0,  0,  0,  0}
#define PARSE_INT(n,t,v) {n,t,	  0,  0,  v,  0,  0,  0}
#define PARSE_STR(n,t,v) {n,t,	  0,  0,  0,  v,  0,  0}
#define PARSE_ENV(n,t,v) {n,t,	  0,  0,  0,  v,  0,  0}
#define PARSE_FUN(n,t,v) {n,t,	  0,  0,  0,  0,  v,  0}
#define PARSE_DEF(n,t,v) {n,t,	  0,  0,  0,  0,  0,  v}
#else
#define ParseData long value
#define PARSE_ADD(n,t,v) {n,t,	 (long)&(v)}
#define PARSE_SET(n,t,v) {n,t,	 (long) (v)}
#define PARSE_INT(n,t,v) {n,t,	 (long) (v)}
#define PARSE_STR(n,t,v) {n,t,	 (long) (v)}
#define PARSE_ENV(n,t,v) {n,t,	 (long) (v)}
#define PARSE_FUN(n,t,v) {n,t,	 (long) (v)}
#define PARSE_DEF(n,t,v) {n,t,	 (long) (v)}
#endif

typedef struct
{
   CONST char *name;
   int type;
#define CONF_UNSPECIFIED	0
#define CONF_BOOL		1      /* BOOLEAN type */
#define CONF_FUN		2
#define CONF_INT		3
#define CONF_STR		4
#define CONF_ENV		5      /* from environment variable */
#define CONF_ENV2		6      /* from environment VARIABLE */
#define CONF_INCLUDE		7      /* include file-- handle special */
#define CONF_ADD_ITEM		8
#define CONF_ADD_TRUSTED	9

   ParseData;
}
Config_Type;

static int assume_charset_fun ARGS1(
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

static int assume_local_charset_fun ARGS1(
	char *,		value)
{
    UCLYhndl_HTFile_for_unspec = safeUCGetLYhndl_byMIME(value);
    return 0;
}

static int assume_unrec_charset_fun ARGS1(
	char *,		value)
{
    UCLYhndl_for_unrec = safeUCGetLYhndl_byMIME(value);
    return 0;
}

static int character_set_fun ARGS1(
	char *,		value)
{
    int i = UCGetLYhndl_byAnyName(value); /* by MIME or full name */
    if (i < 0)
	; /* do nothing here: so fallback to userdefs.h */
    else
	current_char_set = i;

    return 0;
}

static int outgoing_mail_charset_fun ARGS1(
	char *,		value)
{
    outgoing_mail_charset = UCGetLYhndl_byMIME(value);
    /* -1 if NULL or not recognized value: no translation (compatibility) */

    return 0;
}


#ifdef USE_COLOR_TABLE
static int color_fun ARGS1(
	char *,		value)
{
    parse_color (value);
    return 0;
}
#endif

static int default_bookmark_file_fun ARGS1(
	char *,		value)
{
    StrAllocCopy(bookmark_page, value);
    StrAllocCopy(BookmarkPage, bookmark_page);
    StrAllocCopy(MBM_A_subbookmark[0], bookmark_page);
    StrAllocCopy(MBM_A_subdescript[0], MULTIBOOKMARKS_DEFAULT);
    return 0;
}

static int default_cache_size_fun ARGS1(
	char *,		value)
{
    HTCacheSize = atoi(value);
    if (HTCacheSize < 2) HTCacheSize = 2;
    return 0;
}

static int default_editor_fun ARGS1(
	char *,		value)
{
    if (!system_editor) StrAllocCopy(editor, value);
    return 0;
}

static int numbers_as_arrows_fun ARGS1(
	char *,		value)
{
    if (is_true(value))
	keypad_mode = NUMBERS_AS_ARROWS;
    else
	keypad_mode = LINKS_ARE_NUMBERED;

    return 0;
}

static int default_user_mode_fun ARGS1(
	char *,		value)
{
    if (!strncasecomp(value, "NOVICE", 6))
	user_mode = NOVICE_MODE;
    else if (!strncasecomp(value, "INTER", 5))
	user_mode = INTERMEDIATE_MODE;
    else if (!strncasecomp(value, "ADVANCE", 7))
	user_mode = ADVANCED_MODE;

   return 0;
}

#ifdef DIRED_SUPPORT
static int dired_menu_fun ARGS1(
	char *,		value)
{
    add_menu_item(value);
    return 0;
}
#endif

static int jumpfile_fun ARGS1(
	char *,		value)
{
    char *buffer = NULL;

    HTSprintf0 (&buffer, "JUMPFILE:%s", value);
    if (!LYJumpInit(buffer))
	CTRACE(tfp, "Failed to register %s\n", buffer);
    FREE(buffer);

    return 0;
}

#ifdef EXP_KEYBOARD_LAYOUT
static int keyboard_layout_fun ARGS1(
	char *,		key)
{
    if (!LYSetKbLayout(key))
	CTRACE(tfp, "Failed to set keyboard layout %s\n", key);
    return 0;
}
#endif /* EXP_KEYBOARD_LAYOUT */

static int keymap_fun ARGS1(
	char *,		key)
{
    char *func;

    if ((func = strchr(key, ':')) != NULL)	{
	*func++ = '\0';
	/* Allow comments on the ends of key remapping lines. - DT */
	if (!remap(key, strtok(func, " \t\n#")))
	    fprintf(stderr, "%s%s%s%s%s\n",
		    gettext("key remapping of "),
		    key, TO_SEGMENT, func,
		    gettext(" failed"));
	else if (!strcmp("TOGGLE_HELP", strtok(func, " \t\n#")))
	    LYUseNoviceLineTwo = FALSE;
    }
    return 0;
}

static int localhost_alias_fun ARGS1(
	char *,		value)
{
    LYAddLocalhostAlias(value);
    return 0;
}

#ifdef LYNXCGI_LINKS
static int lynxcgi_environment_fun ARGS1(
	char *,		value)
{
    add_lynxcgi_environment(value);
    return 0;
}
#endif

static int lynx_sig_file_fun ARGS1(
	char *,		value)
{
    char temp[LY_MAXPATH];
    LYstrncpy(temp, value, sizeof(temp)-1);
    if (LYPathOffHomeOK(temp, sizeof(temp))) {
	StrAllocCopy(LynxSigFile, temp);
	LYAddPathToHome(temp, sizeof(temp), LynxSigFile);
	StrAllocCopy(LynxSigFile, temp);
	CTRACE(tfp, "LYNX_SIG_FILE set to '%s'\n", LynxSigFile);
    } else {
	CTRACE(tfp, "LYNX_SIG_FILE '%s' is bad. Ignoring.\n", LYNX_SIG_FILE);
    }
    return 0;
}

#ifndef DISABLE_NEWS
static int news_chunk_size_fun ARGS1(
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

static int news_max_chunk_fun ARGS1(
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

static int news_posting_fun ARGS1(
	char *,		value)
{
    LYNewsPosting = is_true(value);
    no_newspost = (LYNewsPosting == FALSE);
    return 0;
}
#endif /* DISABLE_NEWS */

#ifndef NO_RULES
static int cern_rulesfile_fun ARGS1(
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
    exit_immediately(69);	/* EX_UNAVAILABLE in sysexits.h */
    return 0;			/* though redundant, for compiler-warnings */
}
#endif /* NO_RULES */

static int printer_fun ARGS1(
	char *,		value)
{
    add_printer_to_list(value, &printers);
    return 0;
}

#ifdef SOURCE_CACHE
static int source_cache_fun ARGS1(
	char *,		value)
{
    if (!strncasecomp(value, "FILE", 4))
	LYCacheSource = SOURCE_CACHE_FILE;
    else if (!strncasecomp(value, "MEM", 3))
	LYCacheSource = SOURCE_CACHE_MEMORY;
    else if (!strncasecomp(value, "NONE", 4))
	LYCacheSource = SOURCE_CACHE_NONE;

    return 0;
}
#endif

static int suffix_fun ARGS1(
	char *,		value)
{
    char *mime_type;

    if ((strlen (value) < 3)
    || (NULL == (mime_type = strchr (value, ':'))))
	return 0;

    *mime_type++ = '\0';

    LYRemoveBlanks(mime_type);
    LYLowerCase(mime_type);

    if (strstr(mime_type, "tex") != NULL ||
	strstr(mime_type, "postscript") != NULL ||
	strstr(mime_type, "sh") != NULL ||
	strstr(mime_type, "troff") != NULL ||
	strstr(mime_type, "rtf") != NULL)
	HTSetSuffix(value, mime_type, "8bit", 1.0);
    else
	HTSetSuffix(value, mime_type, "binary", 1.0);

    return 0;
}

static int system_editor_fun ARGS1(
	char *,		value)
{
    StrAllocCopy(editor, value);
    system_editor = TRUE;
    return 0;
}

static int viewer_fun ARGS1(
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


#ifdef USE_PSRC

static void html_src_bad_syntax ARGS2(
	    char*, value,
	    char*, option_name)
{
    char *buf = 0;

    HTSprintf0(&buf,"HTMLSRC_%s", option_name);
    LYUpperCase(buf);
    fprintf(stderr,"Bad syntax in TAGSPEC %s:%s\n", buf, value);
    exit_immediately(-1);
}


static int parse_html_src_spec ARGS3(
	    HTlexem, lexem_code,
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

    if ( html_src_parse_tagspec(value, lexem_code, TRUE, TRUE)
	|| html_src_parse_tagspec(ts2, lexem_code, TRUE, TRUE) )
    {
	*ts2 = ':';
	BS();
    }

    *ts2 = ':';
    HTL_tagspecs[lexem_code] = NULL;
    StrAllocCopy(HTL_tagspecs[lexem_code],value);
#undef BS
    return 0;
}

#define defHTSRC_parse_fun(x) static int html_src_set_##x ARGS1( char*,str) \
 { parse_html_src_spec(HTL_##x,str,#x); return 0; }

defHTSRC_parse_fun(comm)
defHTSRC_parse_fun(tag)
defHTSRC_parse_fun(attrib)
defHTSRC_parse_fun(attrval)
defHTSRC_parse_fun(abracket)
defHTSRC_parse_fun(entity)
defHTSRC_parse_fun(href)
defHTSRC_parse_fun(entire)
defHTSRC_parse_fun(badseq)
defHTSRC_parse_fun(badtag)
defHTSRC_parse_fun(badattr)
defHTSRC_parse_fun(sgmlspecial)

#undef defHTSRC_parse_fun

static int read_htmlsrc_attrname_xform ARGS1( char*,str)
{
    int val;
    if ( 1 == sscanf(str, "%d", &val) ) {
	if (val<0 || val >2) {
	    CTRACE(tfp,"bad value for htmlsrc_attrname_xform (ignored - must be one of 0,1,2): %d\n", val);
	} else
	    attrname_transform = val;
    } else {
	CTRACE(tfp,"bad value for htmlsrc_attrname_xform (ignored): %s\n",
		    str);
    }
    return 0;
}

static int read_htmlsrc_tagname_xform ARGS1( char*,str)
{
    int val;
    if ( 1 == sscanf(str,"%d",&val) ) {
	if (val<0 || val >2) {
	    CTRACE(tfp,"bad value for htmlsrc_tagname_xform (ignored - must be one of 0,1,2): %d\n", val);
	} else
	    tagname_transform = val;
    } else {
	CTRACE(tfp,"bad value for htmlsrc_tagname_xform (ignored): %s\n",
		    str);
    }
    return 0;
}


#define defHTSRC_option(x) \
    PARSE_FUN( "htmlsrc_" #x ,CONF_FUN, html_src_set_##x),

#endif


/* This table should be sorted alphabetically */
static Config_Type Config_Table [] =
{
     PARSE_SET("accept_all_cookies", CONF_BOOL, &LYAcceptAllCookies),
     PARSE_INT("alertsecs", CONF_INT, &AlertSecs),
     PARSE_SET("always_resubmit_posts", CONF_BOOL, &LYresubmit_posts),
#ifdef EXEC_LINKS
     PARSE_DEF("always_trusted_exec", CONF_ADD_TRUSTED, ALWAYS_EXEC_PATH),
#endif
     PARSE_FUN("assume_charset", CONF_FUN, assume_charset_fun),
     PARSE_FUN("assume_local_charset", CONF_FUN, assume_local_charset_fun),
     PARSE_FUN("assume_unrec_charset", CONF_FUN, assume_unrec_charset_fun),
     PARSE_SET("block_multi_bookmarks", CONF_BOOL, &LYMBMBlocked),
     PARSE_SET("bold_h1", CONF_BOOL, &bold_H1),
     PARSE_SET("bold_headers", CONF_BOOL, &bold_headers),
     PARSE_SET("bold_name_anchors", CONF_BOOL, &bold_name_anchors),
     PARSE_SET("case_sensitive_always_on", CONF_BOOL, &case_sensitive),
     PARSE_FUN("character_set", CONF_FUN, character_set_fun),
     PARSE_SET("checkmail", CONF_BOOL, &check_mail),
     PARSE_SET("collapse_br_tags", CONF_BOOL, &LYCollapseBRs),
#ifdef USE_COLOR_TABLE
     PARSE_FUN("color", CONF_FUN, color_fun),
#endif
     PARSE_STR("cookie_accept_domains", CONF_STR, &LYCookieSAcceptDomains),
#ifdef EXP_PERSISTENT_COOKIES
     PARSE_STR("cookie_file", CONF_STR, &LYCookieFile),
#endif /* EXP_PERSISTENT_COOKIES */
     PARSE_STR("cookie_loose_invalid_domains", CONF_STR, &LYCookieSLooseCheckDomains),
     PARSE_STR("cookie_query_invalid_domains", CONF_STR, &LYCookieSQueryCheckDomains),
     PARSE_STR("cookie_reject_domains", CONF_STR, &LYCookieSRejectDomains),
     PARSE_STR("cookie_strict_invalid_domains", CONF_STR, &LYCookieSStrictCheckDomains),
     PARSE_ENV("cso_proxy", CONF_ENV, 0 ),
#ifdef VMS
     PARSE_STR("CSWING_PATH", CONF_STR, &LYCSwingPath),
#endif
     PARSE_FUN("default_bookmark_file", CONF_FUN, default_bookmark_file_fun),
     PARSE_FUN("default_cache_size", CONF_FUN, default_cache_size_fun),
     PARSE_FUN("default_editor", CONF_FUN, default_editor_fun),
     PARSE_STR("default_index_file", CONF_STR, &indexfile),
     PARSE_FUN("default_keypad_mode_is_numbers_as_arrows", CONF_FUN, numbers_as_arrows_fun),
     PARSE_FUN("default_user_mode", CONF_FUN, default_user_mode_fun),
#if defined(VMS) && defined(VAXC) && !defined(__DECC)
     PARSE_INT("default_virtual_memory_size", CONF_INT, &HTVirtualMemorySize),
#endif
#ifdef DIRED_SUPPORT
     PARSE_FUN("dired_menu", CONF_FUN, dired_menu_fun),
#endif
     PARSE_ADD("downloader", CONF_ADD_ITEM, downloaders),
     PARSE_SET("emacs_keys_always_on", CONF_BOOL, &emacs_keys),
     PARSE_SET("enable_scrollback", CONF_BOOL, &enable_scrollback),
#ifdef USE_EXTERNALS
     PARSE_ADD("external", CONF_ADD_ITEM, externals),
#endif
     PARSE_ENV("finger_proxy", CONF_ENV, 0 ),
     PARSE_SET("force_8bit_toupper", CONF_BOOL, &UCForce8bitTOUPPER),
     PARSE_SET("force_ssl_cookies_secure", CONF_BOOL, &LYForceSSLCookiesSecure),
#if !defined(NO_OPTION_FORMS) && !defined(NO_OPTION_MENU)
     PARSE_SET("forms_options", CONF_BOOL, &LYUseFormsOptions),
#endif
     PARSE_ENV("ftp_proxy", CONF_ENV, 0 ),
     PARSE_STR("global_extension_map", CONF_STR, &global_extension_map),
     PARSE_STR("global_mailcap", CONF_STR, &global_type_map),
     PARSE_ENV("gopher_proxy", CONF_ENV, 0 ),
     PARSE_SET("gotobuffer", CONF_BOOL, &goto_buffer),
     PARSE_STR("helpfile", CONF_STR, &helpfile),
     PARSE_SET("historical_comments", CONF_BOOL, &historical_comments),

#ifdef USE_PSRC

     defHTSRC_option(abracket)
     defHTSRC_option(attrib)

     PARSE_FUN("htmlsrc_attrname_xform", CONF_FUN, read_htmlsrc_attrname_xform),

     defHTSRC_option(attrval)
     defHTSRC_option(badattr)
     defHTSRC_option(badseq)
     defHTSRC_option(badtag)
     defHTSRC_option(comm)
     defHTSRC_option(entire)
     defHTSRC_option(entity)
     defHTSRC_option(href)
     defHTSRC_option(sgmlspecial)
     defHTSRC_option(tag)

     PARSE_FUN("htmlsrc_tagname_xform", CONF_FUN, read_htmlsrc_tagname_xform),


# undef defHTSRC_option
#endif

     PARSE_ENV("http_proxy", CONF_ENV, 0 ),
     PARSE_ENV("https_proxy", CONF_ENV, 0 ),
     PARSE_FUN("include", CONF_INCLUDE, 0),
     PARSE_INT("infosecs", CONF_INT, &InfoSecs),
     PARSE_STR("jump_prompt", CONF_STR, &jumpprompt),
     PARSE_SET("jumpbuffer", CONF_BOOL, &jump_buffer),
     PARSE_FUN("jumpfile", CONF_FUN, jumpfile_fun),
#ifdef EXP_KEYBOARD_LAYOUT
     PARSE_FUN("keyboard_layout", CONF_FUN, keyboard_layout_fun),
#endif
     PARSE_FUN("keymap", CONF_FUN, keymap_fun),
#ifndef DISABLE_NEWS
     PARSE_SET("list_news_numbers", CONF_BOOL, &LYListNewsNumbers),
     PARSE_SET("list_news_dates", CONF_BOOL, &LYListNewsDates),
#endif
#ifndef VMS
     PARSE_STR("list_format", CONF_STR, &list_format),
#endif
     PARSE_FUN("localhost_alias", CONF_FUN, localhost_alias_fun),
     PARSE_STR("local_domain", CONF_STR, &LYLocalDomain),
#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
     PARSE_SET("local_execution_links_always_on", CONF_BOOL, &local_exec),
     PARSE_SET("local_execution_links_on_but_not_remote", CONF_BOOL, &local_exec_on_local_files),
#endif
#ifdef LYNXCGI_LINKS
     PARSE_FUN("lynxcgi_environment", CONF_FUN, lynxcgi_environment_fun),
#ifndef VMS
     PARSE_STR("lynxcgi_document_root", CONF_STR, &LYCgiDocumentRoot),
#endif
#endif
     PARSE_STR("lynx_host_name", CONF_STR, &LYHostName),
     PARSE_FUN("lynx_sig_file", CONF_FUN, lynx_sig_file_fun),
     PARSE_SET("mail_system_error_logging", CONF_BOOL, &error_logging),
#ifdef VMS
     PARSE_STR("mail_adrs", CONF_STR, &mail_adrs),
#endif
     PARSE_SET("make_links_for_all_images", CONF_BOOL, &clickable_images),
     PARSE_SET("make_pseudo_alts_for_inlines", CONF_BOOL, &pseudo_inline_alts),
     PARSE_INT("messagesecs", CONF_INT, &MessageSecs),
     PARSE_SET("minimal_comments", CONF_BOOL, &minimal_comments),
     PARSE_SET("multi_bookmark_support", CONF_BOOL, &LYMultiBookmarks),
     PARSE_SET("ncr_in_bookmarks", CONF_BOOL, &UCSaveBookmarksInUnicode),
#ifndef DISABLE_NEWS
     PARSE_FUN("news_chunk_size", CONF_FUN, news_chunk_size_fun),
     PARSE_FUN("news_max_chunk", CONF_FUN, news_max_chunk_fun),
     PARSE_FUN("news_posting", CONF_FUN, news_posting_fun),
     PARSE_ENV("news_proxy", CONF_ENV, 0),
     PARSE_ENV("newspost_proxy", CONF_ENV, 0),
     PARSE_ENV("newsreply_proxy", CONF_ENV, 0),
     PARSE_ENV("nntp_proxy", CONF_ENV, 0),
     PARSE_ENV("nntpserver", CONF_ENV2, 0), /* actually NNTPSERVER */
#endif
     PARSE_SET("no_dot_files", CONF_BOOL, &no_dotfiles),
     PARSE_SET("no_file_referer", CONF_BOOL, &no_filereferer),
#ifndef VMS
     PARSE_SET("no_forced_core_dump", CONF_BOOL, &LYNoCore),
#endif
     PARSE_SET("no_from_header", CONF_BOOL, &LYNoFromHeader),
     PARSE_SET("no_ismap_if_usemap", CONF_BOOL, &LYNoISMAPifUSEMAP),
     PARSE_ENV("no_proxy", CONF_ENV, 0 ),
     PARSE_SET("no_referer_header", CONF_BOOL, &LYNoRefererHeader),
     PARSE_FUN("outgoing_mail_charset", CONF_FUN, outgoing_mail_charset_fun),
#ifdef DISP_PARTIAL
     PARSE_SET("partial", CONF_BOOL, &display_partial_flag),
     PARSE_INT("partial_thres", CONF_INT, &partial_threshold),
#endif
#ifdef EXP_PERSISTENT_COOKIES
     PARSE_SET("persistent_cookies", CONF_BOOL, &persistent_cookies),
#endif /* EXP_PERSISTENT_COOKIES */
     PARSE_STR("personal_mailcap", CONF_STR, &personal_type_map),
     PARSE_STR("personal_extension_map", CONF_STR, &personal_extension_map),
     PARSE_STR("preferred_charset", CONF_STR, &pref_charset),
     PARSE_STR("preferred_language", CONF_STR, &language),
     PARSE_SET("prepend_base_to_source", CONF_BOOL, &LYPrependBaseToSource),
     PARSE_SET("prepend_charset_to_source", CONF_BOOL, &LYPrependCharsetToSource),
     PARSE_FUN("printer", CONF_FUN, printer_fun),
     PARSE_SET("quit_default_yes", CONF_BOOL, &LYQuitDefaultYes),
#ifndef NO_RULES
     PARSE_FUN("rule", CONF_FUN, HTSetConfiguration),
     PARSE_FUN("rulesfile", CONF_FUN, cern_rulesfile_fun),
#endif /* NO_RULES */
     PARSE_STR("save_space", CONF_STR, &lynx_save_space),
     PARSE_SET("scan_for_buried_news_refs", CONF_BOOL, &scan_for_buried_news_references),
     PARSE_SET("seek_frag_area_in_cur", CONF_BOOL, &LYSeekFragAREAinCur),
     PARSE_SET("seek_frag_map_in_cur", CONF_BOOL, &LYSeekFragMAPinCur),
     PARSE_SET("set_cookies", CONF_BOOL, &LYSetCookies),
     PARSE_SET("show_cursor", CONF_BOOL, &LYShowCursor),
     PARSE_SET("show_kb_rate", CONF_BOOL, &LYshow_kb_rate),
     PARSE_ENV("snews_proxy", CONF_ENV, 0 ),
     PARSE_ENV("snewspost_proxy", CONF_ENV, 0 ),
     PARSE_ENV("snewsreply_proxy", CONF_ENV, 0 ),
     PARSE_SET("soft_dquotes", CONF_BOOL, &soft_dquotes),
#ifdef SOURCE_CACHE
     PARSE_SET("source_cache", CONF_FUN, source_cache_fun),
#endif
     PARSE_STR("startfile", CONF_STR, &startfile),
     PARSE_SET("strip_dotdot_urls", CONF_BOOL, &LYStripDotDotURLs),
     PARSE_SET("substitute_underscores", CONF_BOOL, &use_underscore),
     PARSE_FUN("suffix", CONF_FUN, suffix_fun),
     PARSE_FUN("system_editor", CONF_FUN, system_editor_fun),
     PARSE_STR("system_mail", CONF_STR, &system_mail),
     PARSE_STR("system_mail_flags", CONF_STR, &system_mail_flags),
     PARSE_SET("tagsoup", CONF_BOOL, &Old_DTD),
#ifdef EXEC_LINKS
     PARSE_DEF("trusted_exec", CONF_ADD_TRUSTED, EXEC_PATH),
#endif
#ifdef LYNXCGI_LINKS
     PARSE_DEF("trusted_lynxcgi", CONF_ADD_TRUSTED, CGI_PATH),
#endif
     PARSE_STR("url_domain_prefixes", CONF_STR, &URLDomainPrefixes),
     PARSE_STR("url_domain_suffixes", CONF_STR, &URLDomainSuffixes),
#ifdef DIRED_SUPPORT
     PARSE_ADD("uploader", CONF_ADD_ITEM, uploaders),
#endif
#ifdef VMS
     PARSE_SET("use_fixed_records", CONF_BOOL, &UseFixedRecords),
#endif
#if defined(NCURSES_MOUSE_VERSION) || defined(USE_SLANG_MOUSE)
     PARSE_SET("use_mouse", CONF_BOOL, &LYUseMouse),
#endif
     PARSE_SET("use_select_popups", CONF_BOOL, &LYSelectPopups),
     PARSE_SET("verbose_images", CONF_BOOL, &verbose_img),
     PARSE_SET("vi_keys_always_on", CONF_BOOL, &vi_keys),
     PARSE_FUN("viewer", CONF_FUN, viewer_fun),
     PARSE_ENV("wais_proxy", CONF_ENV, 0 ),
     PARSE_STR("xloadimage_command", CONF_STR, &XLoadImageCommand),

     {0, 0, 0}
};

/*
 * Free memory allocated in 'read_cfg()'
 */
PUBLIC void free_lynx_cfg NOARGS
{
    Config_Type *tbl;

    for (tbl = Config_Table; tbl->name != 0; tbl++) {
#ifdef PARSE_DEBUG
	Config_Type *q = tbl;
#else
	ConfigUnion *q = (ConfigUnion *)(&(tbl->value));
#endif
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
    free_item_list();
    free_printer_item_list();
}

PRIVATE Config_Type *lookup_config ARGS1(
	char *,		name)
{
    Config_Type *tbl = Config_Table;
    char ch = TOUPPER(*name);

    while (tbl->name != 0) {
	char ch1 = tbl->name[0];

	if ((ch == TOUPPER(ch1))
	    && (0 == strcasecomp (name, tbl->name)))
	    break;

	tbl++;
    }
    return tbl;
}

#define NOPTS_ ( (sizeof Config_Table)/(sizeof Config_Table[0]) - 1 )
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
    char mypath[LY_MAXPATH];
    char *buffer = 0;

    CTRACE(tfp, "Loading cfg file '%s'.\n", cfg_filename);

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
	exit(-1);
    }
    /*
     *	Locate and open the file.
     */
    if (!cfg_filename || strlen(cfg_filename) == 0) {
	CTRACE(tfp,"No filename following -cfg switch!\n");
	return;
    }
    if (!strncmp(cfg_filename, "~/", 2)) {
	strcpy(mypath, Home_Dir());
	strcat(mypath, cfg_filename+1);
	cfg_filename = mypath;
    }
    if ((fp = fopen(cfg_filename,"r")) == 0) {
	CTRACE(tfp,"lynx.cfg file not found as %s\n",cfg_filename);
	return;
    }
    have_read_cfg = TRUE;

    /*
     *	Process each line in the file.
     */
    while (LYSafeGets(&buffer, fp) != 0) {
	char *name, *value;
	char *cp;
	Config_Type *tbl;
#ifdef PARSE_DEBUG
	Config_Type *q;
#else
	ConfigUnion *q;
#endif

	/* Most lines in the config file are comment lines.  Weed them out
	 * now.  Also, leading whitespace is ok, so trim it.
	 */
	name = LYSkipBlanks(buffer);

	if (*name == '#')
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
	    if (isspace ((unsigned char) *cp))
		*cp = 0;
	}

	tbl = lookup_config(name);
	if (tbl->name == 0) {
	    /* lynx ignores unknown keywords */
	    continue;
	}

	if ( allowed && (*allowed)[ tbl-Config_Table ] ) {
	    if (fp0 == NULL)
		fprintf (stderr, "%s is not allowed in the %s\n",
		    name,cfg_filename);
	    /*FIXME: we can do something wiser if we are generating
	    the html representation of lynx.cfg - say include this line
	    in bold, or something...*/

	    continue;
	}

#ifdef PARSE_DEBUG
	q = tbl;
#else
	q = (ConfigUnion *)(&(tbl->value));
#endif
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

	case CONF_INT:
	    if (q->int_value != 0) {
		int ival;
		/* Apparently, if an integer value is not present, then the
		 * value is not changed.  So, use the sscanf function to make
		 * this determination.
		 */
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
			q->str_value = calloc(1, sizeof(char *));
		HTSprintf0 (q->str_value, "%s=%s", name, value);
		putenv (*(q->str_value));
#endif
	    }
	    break;

	case CONF_INCLUDE: {
	    /* include another file */
	    optidx_set_t cur_set,anded_set;
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
	    if (fp0 != 0  &&  !LYRestricted) {
		LYLocalFileToURL(&url, value);
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
	    if (fp0 != 0  &&  !LYRestricted && resultant_set) {
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
	    if (fp0 != 0  &&  !LYRestricted) {
		fprintf(fp0, "    #&lt;end of %s&gt;\n\n", cp1);
		FREE(url);
		FREE(cp1);
	    }
#endif
	    }
	    break;

	case CONF_ADD_ITEM:
	    if (q->add_value != 0)
		add_item_to_list (value, q->add_value);
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

    fclose (fp);

    /*
     *	If any DOWNLOADER: commands have always_enabled set (:TRUE),
     *	make override_no_download TRUE, so that other restriction
     *	settings will not block presentation of a download menu
     *	with those always_enabled options still available. - FM
     */
    if (downloaders != 0) {
	lynx_html_item_type *cur_download;

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

    if (LYCookieSAcceptDomains != NULL) {
	cookie_domain_flag_set(LYCookieSAcceptDomains, FLAG_ACCEPT_ALWAYS);
	FREE(LYCookieSAcceptDomains);
    }

    if (LYCookieSRejectDomains != NULL) {
	cookie_domain_flag_set(LYCookieSRejectDomains, FLAG_REJECT_ALWAYS);
	FREE(LYCookieSRejectDomains);
    }

    if (LYCookieSStrictCheckDomains != NULL) {
	cookie_domain_flag_set(LYCookieSStrictCheckDomains, FLAG_INVCHECK_STRICT);
	FREE(LYCookieSStrictCheckDomains);
    }

    if (LYCookieSLooseCheckDomains != NULL) {
	cookie_domain_flag_set(LYCookieSLooseCheckDomains, FLAG_INVCHECK_LOOSE);
	FREE(LYCookieSLooseCheckDomains);
    }

    if (LYCookieSQueryCheckDomains != NULL) {
	cookie_domain_flag_set(LYCookieSQueryCheckDomains, FLAG_INVCHECK_QUERY);
	FREE(LYCookieSQueryCheckDomains);
    }

    if (LYCookieAcceptDomains != NULL) {
	cookie_domain_flag_set(LYCookieAcceptDomains, FLAG_ACCEPT_ALWAYS);
    }

    if (LYCookieRejectDomains != NULL) {
	cookie_domain_flag_set(LYCookieRejectDomains, FLAG_REJECT_ALWAYS);
    }

    if (LYCookieStrictCheckDomains != NULL) {
	cookie_domain_flag_set(LYCookieStrictCheckDomains, FLAG_INVCHECK_STRICT);
    }

    if (LYCookieLooseCheckDomains != NULL) {
	cookie_domain_flag_set(LYCookieLooseCheckDomains, FLAG_INVCHECK_LOOSE);
    }

    if (LYCookieQueryCheckDomains != NULL) {
	cookie_domain_flag_set(LYCookieQueryCheckDomains, FLAG_INVCHECK_QUERY);
    }

}
/* this is a public interface to do_read_cfg */
PUBLIC void read_cfg ARGS4(
	char *, cfg_filename,
	char *, parent_filename,
	int,	nesting_level,
	FILE *,	fp0)
{
    do_read_cfg(cfg_filename,parent_filename,nesting_level,fp0,
	NULL);
}


/*
 *  Show rendered lynx.cfg data without comments, LYNXCFG:/ internal page.
 *  Called from getfile() cyrcle:
 *  we create and load the page just in place and return to mainloop().
 */
PUBLIC int lynx_cfg_infopage ARGS1(
    document *,		       newdoc)
{
    static char tempfile[LY_MAXPATH];
    static char *local_url;  /* static! */
    DocAddress WWWDoc;  /* need on exit */
    char *temp = 0;
    char *cp1 = NULL;
    FILE *fp0;


#ifndef NO_CONFIG_INFO
    /*-------------------------------------------------
     * kludge a link from LYNXCFG:/, the URL was:
     * "  <a href=\"LYNXCFG://reload\">RELOAD THE CHANGES</a>\n"
     *--------------------------------------------------*/

    if ((strstr(newdoc->address, "LYNXCFG://reload")) && !LYRestricted) {
	/*
	 *  Some staff to reload read_cfg(),
	 *  but also load options menu items and command-line options
	 *  to make things consistent.	Implemented in LYMain.c
	 */
	reload_read_cfg();

	/*
	 *  now pop-up and return to updated LYNXCFG:/ page,
	 *  remind postoptions() but much simpler:
	 */

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

	/*  now set up the flag and fall down to create a new LYNXCFG:/ page */
	local_url = 0;	/* see below */
    }
#endif /* !NO_CONFIG_INFO */


    if (local_url == 0) {

	LYRemoveTemp(tempfile);
	if ((fp0 = LYOpenTemp(tempfile, HTML_SUFFIX, "w")) == NULL) {
	    HTAlert(CANNOT_OPEN_TEMP);
	    return(NOT_FOUND);
	}
	LYLocalFileToURL(&local_url, tempfile);

	LYforce_no_cache = TRUE;  /* don't cache this doc */

	BeginInternalPage (fp0, LYNXCFG_TITLE, NULL);
	fprintf(fp0, "<pre>\n");


#ifndef NO_CONFIG_INFO
	if (!LYRestricted) {
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
    }

    /* return to getfile() cyrcle */
    StrAllocCopy(newdoc->address, local_url);
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


#if defined(HAVE_CONFIG_H) && !defined(NO_CONFIG_INFO)
/*
 *  Compile-time definitions info, LYNXCOMPILEOPTS:/ internal page,
 *  from getfile() cyrcle.
 */
PUBLIC int lynx_compile_opts ARGS1(
    document *,		       newdoc)
{
    char tempfile[LY_MAXPATH];
#define PutDefs(table, N) fprintf(fp0, "%-35s %s\n", table[N].name, table[N].value)
#include <cfg_defs.h>
    unsigned n;
    static char *info_url;  /* static! */
    DocAddress WWWDoc;  /* need on exit */
    FILE *fp0;

    /* create the page only once - compile-time data will not change... */

    if (info_url == 0) {
	if ((fp0 = LYOpenTemp (tempfile, HTML_SUFFIX, "w")) == 0) {
	    HTAlert(CANNOT_OPEN_TEMP);
	    return(NOT_FOUND);
	}
	LYLocalFileToURL(&info_url, tempfile);

	BeginInternalPage (fp0, CONFIG_DEF_TITLE, NULL);
	fprintf(fp0, "<pre>\n");

	fprintf(fp0, "%s %s<a href=\"LYNXCFG:\"> lynx.cfg</a> %s\n\n",
		     SEE_ALSO,
		     YOUR_SEGMENT,
		     RUNTIME_OPT_SEGMENT);

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
    }

    /* exit to getfile() cyrcle */
    StrAllocCopy(newdoc->address, info_url);
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
#endif /* !NO_CONFIG_INFO */
