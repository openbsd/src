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
#include <HTMLDTD.h>

#include <LYLeaks.h>

#ifdef FNAMES_8_3
#define FNAME_LYNXRC "lynx.rc"
#else
#define FNAME_LYNXRC ".lynxrc"
#endif /* FNAMES_8_3 */

#define MSG_ENABLE_LYNXRC N_("Normally disabled.  See ENABLE_LYNXRC in lynx.cfg\n")
#define putBool(value) ((value) ? "on" : "off")

PUBLIC Config_Enum tbl_DTD_recovery[] = {
    { "true",		TRUE },
    { "false",		FALSE },
    { "on",		TRUE },
    { "off",		FALSE },
    { "sortasgml",	TRUE },
    { "tagsoup",	FALSE },
    { NULL,		-1 },
};

#ifdef DIRED_SUPPORT
PRIVATE Config_Enum tbl_dir_list_style[] = {
    { "FILES_FIRST",	FILES_FIRST },
    { "DIRECTORIES_FIRST", DIRS_FIRST },
    { "MIXED_STYLE",	MIXED_STYLE },
    { NULL,		MIXED_STYLE },
};
#ifdef LONG_LIST
PRIVATE Config_Enum tbl_dir_list_order[] = {
    { "ORDER_BY_NAME",	ORDER_BY_NAME },
    { "ORDER_BY_TYPE",	ORDER_BY_TYPE },
    { "ORDER_BY_SIZE",  ORDER_BY_SIZE },
    { "ORDER_BY_DATE",	ORDER_BY_DATE },
    { "ORDER_BY_MODE",	ORDER_BY_MODE },
#ifndef NO_GROUPS
    { "ORDER_BY_USER",	ORDER_BY_USER },
    { "ORDER_BY_GROUP",	ORDER_BY_GROUP },
#endif
    { NULL,		ORDER_BY_NAME },
};
#endif /* LONG_LIST */
#endif /* DIRED_SUPPORT */

PRIVATE Config_Enum tbl_file_sort[] = {
    { "BY_FILENAME",	FILE_BY_NAME },
    { "BY_TYPE",	FILE_BY_TYPE },
    { "BY_SIZE",	FILE_BY_SIZE },
    { "BY_DATE",	FILE_BY_DATE },
    { NULL,		-1 },
};

PUBLIC Config_Enum tbl_keypad_mode[] = {
    { "FIELDS_ARE_NUMBERED", FIELDS_ARE_NUMBERED },
    { "LINKS_AND_FIELDS_ARE_NUMBERED", LINKS_AND_FIELDS_ARE_NUMBERED },
    { "LINKS_ARE_NUMBERED", LINKS_ARE_NUMBERED },
    { "LINKS_ARE_NOT_NUMBERED", NUMBERS_AS_ARROWS },
    /* obsolete variations: */
    { "LINKS_AND_FORM_FIELDS_ARE_NUMBERED", LINKS_AND_FIELDS_ARE_NUMBERED },
    { "NUMBERS_AS_ARROWS", NUMBERS_AS_ARROWS },
    { NULL,		DEFAULT_KEYPAD_MODE }
};

PUBLIC Config_Enum tbl_multi_bookmarks[] = {
    { "OFF",		MBM_OFF },
    { "STANDARD",	MBM_STANDARD },
    { "ON",		MBM_STANDARD },
    { "ADVANCED",	MBM_ADVANCED },
    { NULL,		-1 }
};

PRIVATE Config_Enum tbl_show_colors[] = {
    { "default",	SHOW_COLOR_UNKNOWN },
    { "default",	SHOW_COLOR_OFF },
    { "default",	SHOW_COLOR_ON },
    { "on",		SHOW_COLOR_UNKNOWN },
    { "off",		SHOW_COLOR_UNKNOWN },
    { "never",		SHOW_COLOR_NEVER },
    { "always",		SHOW_COLOR_ALWAYS },
    { NULL,		SHOW_COLOR_UNKNOWN }
};

PUBLIC Config_Enum tbl_transfer_rate[] = {
    { "NONE",		rateOFF },
    { "KB",		rateKB },
    { "TRUE",		rateKB },
    { "BYTES",		rateBYTES },
    { "FALSE",		rateBYTES },
#ifdef USE_READPROGRESS
    { "KB,ETA",		rateEtaKB },
    { "BYTES,ETA",	rateEtaBYTES },
#endif
    { NULL,		-1 },
};

PUBLIC Config_Enum tbl_user_mode[] = {
    { "ADVANCED",	ADVANCED_MODE },
    { "INTERMEDIATE",	INTERMEDIATE_MODE },
    { "NOVICE",		NOVICE_MODE },
    { NULL,		NOVICE_MODE }
};

PRIVATE Config_Enum tbl_visited_links[] = {
    { "FIRST_REVERSED",	VISITED_LINKS_AS_FIRST_V | VISITED_LINKS_REVERSE },
    { "FIRST",		VISITED_LINKS_AS_FIRST_V },
    { "TREE",		VISITED_LINKS_AS_TREE    },
    { "LAST_REVERSED",	VISITED_LINKS_AS_LATEST | VISITED_LINKS_REVERSE },
    { "LAST",		VISITED_LINKS_AS_LATEST  },
    { NULL,		DEFAULT_VISITED_LINKS }
};

PUBLIC Config_Enum tbl_force_prompt[] = {
    { "prompt",		FORCE_PROMPT_DFT	},
    { "yes",		FORCE_PROMPT_YES	},
    { "no",		FORCE_PROMPT_NO		},
    { NULL,		-1			}
};

PRIVATE BOOL getBool ARGS1(char *, src)
{
    return (BOOL) (!strncasecomp(src, "on", 2) || !strncasecomp(src, "true", 4));
}

PUBLIC CONST char *LYputEnum ARGS2(
    Config_Enum *,	table,
    int,		value)
{
    while (table->name != 0) {
	if (table->value == value) {
	    return table->name;
	}
	table++;
    }
    return "?";
}

PUBLIC BOOL LYgetEnum ARGS3(
    Config_Enum *,	table,
    char *,		name,
    int *,		result)
{
    Config_Enum *found = 0;
    unsigned len = strlen(name);
    int match = 0;

    if (len != 0) {
	while (table->name != 0) {
	    if (!strncasecomp(table->name, name, len)) {
		found = table;
		if (!strcasecomp(table->name, name)) {
		    match = 1;
		    break;
		}
		++match;
	    }
	    table++;
	}
	if (match == 1) {	/* if unambiguous */
	    *result = found->value;
	    return TRUE;
	}
    }
    return FALSE;		/* no match */
}

/* these are for data that are normally not read/written from .lynxrc */
#define PARSE_SET(n,v,h)   {n,    1, CONF_BOOL,  UNION_SET(v), 0, 0, 0, h}
#define PARSE_ARY(n,v,t,h) {n,    1, CONF_ARRAY, UNION_INT(v), t, 0, 0, h}
#define PARSE_ENU(n,v,t,h) {n,    1, CONF_ENUM,  UNION_INT(v), 0, t, 0, h}
#define PARSE_LIS(n,v,h)   {n,    1, CONF_LIS,   UNION_STR(v), 0, 0, 0, h}
#define PARSE_STR(n,v,h)   {n,    1, CONF_STR,   UNION_STR(v), 0, 0, 0, h}
#define PARSE_FUN(n,v,w,h) {n,    1, CONF_FUN,   UNION_FUN(v), 0, 0, w, h}
#define PARSE_MBM(n,h)     {n,    1, CONF_MBM,   UNION_DEF(0), 0, 0, 0, h}

/* these are for data that are optionally read/written from .lynxrc */
#define MAYBE_SET(n,v,h)   {n,    0, CONF_BOOL,  UNION_SET(v), 0, 0, 0, h}
#define MAYBE_ARY(n,v,t,h) {n,    0, CONF_ARRAY, UNION_INT(v), t, 0, 0, h}
#define MAYBE_ENU(n,v,t,h) {n,    0, CONF_ENUM,  UNION_INT(v), 0, t, 0, h}
#define MAYBE_LIS(n,v,h)   {n,    0, CONF_LIS,   UNION_STR(v), 0, 0, 0, h}
#define MAYBE_STR(n,v,h)   {n,    0, CONF_STR,   UNION_STR(v), 0, 0, 0, h}
#define MAYBE_FUN(n,v,w,h) {n,    0, CONF_FUN,   UNION_FUN(v), 0, 0, w, h}
#define MAYBE_MBM(n,h)     {n,    0, CONF_MBM,   UNION_DEF(0), 0, 0, 0, h}

#define PARSE_NIL          {NULL, 1, 0,          UNION_DEF(0), 0, 0, 0, 0}

typedef enum {
    CONF_UNSPECIFIED = 0
    ,CONF_ARRAY
    ,CONF_BOOL
    ,CONF_FUN
    ,CONF_INT
    ,CONF_ENUM
    ,CONF_LIS
    ,CONF_MBM
    ,CONF_STR
} Conf_Types;

typedef struct config_type
{
    CONST char *name;
    int enabled;		/* see lynx.cfg ENABLE_LYNXRC "off" lines */
    Conf_Types type;
    ParseData;
    char **strings;
    Config_Enum *table;
    void (*write_it) PARAMS((FILE * fp, struct config_type *));
    char *note;
} Config_Type;

PRIVATE int get_assume_charset ARGS1(char *, value)
{
    int i;

    for (i = 0; i < LYNumCharsets; ++i) {
    	if (!strcasecomp(value, LYCharSet_UC[i].MIMEname)) {
	    UCLYhndl_for_unspec = i;
	    break;
	}
    }
    return 0;
}

PRIVATE void put_assume_charset ARGS2(FILE *, fp, struct config_type *, tbl)
{
    int i;

    for (i = 0; i < LYNumCharsets; ++i)
	fprintf(fp, "#    %s\n", LYCharSet_UC[i].MIMEname);
    fprintf(fp, "%s=%s\n\n", tbl->name, LYCharSet_UC[UCLYhndl_for_unspec].MIMEname);
}

PRIVATE int get_display_charset ARGS1(char *, value)
{
    int i = 0;

    i = UCGetLYhndl_byAnyName(value); /* by MIME or full name */
    if (i >= 0)
	current_char_set = i;
    return 0;
}

PRIVATE void put_display_charset ARGS2(FILE *, fp, struct config_type *, tbl)
{
    int i;

    for (i = 0; LYchar_set_names[i]; i++)
	fprintf(fp, "#    %s\n", LYchar_set_names[i]);
    fprintf(fp, "%s=%s\n\n", tbl->name, LYchar_set_names[current_char_set]);
}

PRIVATE int get_editor ARGS1(char *, value)
{
    if (!system_editor)
	StrAllocCopy(editor, value);
    return 0;
}

PRIVATE void put_editor ARGS2(FILE *, fp, struct config_type *, tbl)
{
    fprintf(fp, "%s=%s\n\n", tbl->name, NonNull(editor));
}

PUBLIC int get_tagsoup ARGS1(char *, value)
{
    int found = Old_DTD;

    if (LYgetEnum(tbl_DTD_recovery, value, &found)
     && Old_DTD != found) {
	Old_DTD = found;
	HTSwitchDTD(!Old_DTD);
    }
    return 0;
}

PRIVATE void put_tagsoup ARGS2(FILE *, fp, struct config_type *, tbl)
{
    fprintf(fp, "%s=%s\n\n", tbl->name, LYputEnum(tbl_DTD_recovery, Old_DTD));
}

/* This table is searched ignoring case */
static Config_Type Config_Table [] =
{
    PARSE_SET(RC_ACCEPT_ALL_COOKIES,    LYAcceptAllCookies, N_("\
accept_all_cookies allows the user to tell Lynx to automatically\n\
accept all cookies if desired.  The default is \"FALSE\" which will\n\
prompt for each cookie.  Set accept_all_cookies to \"TRUE\" to accept\n\
all cookies.\n\
")),
    MAYBE_FUN(RC_ASSUME_CHARSET,        get_assume_charset, put_assume_charset, MSG_ENABLE_LYNXRC),
    PARSE_STR(RC_BOOKMARK_FILE,         bookmark_page,     N_("\
bookmark_file specifies the name and location of the default bookmark\n\
file into which the user can paste links for easy access at a later\n\
date.\n\
")),
    PARSE_SET(RC_CASE_SENSITIVE_SEARCHING, case_sensitive, N_("\
If case_sensitive_searching is \"on\" then when the user invokes a search\n\
using the 's' or '/' keys, the search performed will be case sensitive\n\
instead of case INsensitive.  The default is usually \"off\".\n\
")),
    PARSE_FUN(RC_CHARACTER_SET,         get_display_charset, put_display_charset, N_("\
The character_set definition controls the representation of 8 bit\n\
characters for your terminal.  If 8 bit characters do not show up\n\
correctly on your screen you may try changing to a different 8 bit\n\
set or using the 7 bit character approximations.\n\
Current valid characters sets are:\n\
")),
    PARSE_LIS(RC_COOKIE_ACCEPT_DOMAINS, LYCookieAcceptDomains, N_("\
cookie_accept_domains and cookie_reject_domains are comma-delimited\n\
lists of domains from which Lynx should automatically accept or reject\n\
all cookies.  If a domain is specified in both options, rejection will\n\
take precedence.  The accept_all_cookies parameter will override any\n\
settings made here.\n\
")),
#ifdef USE_PERSISTENT_COOKIES
    PARSE_STR(RC_COOKIE_FILE,	        LYCookieFile, N_("\
cookie_file specifies the file from which to read persistent cookies.\n\
The default is ~/.lynx_cookies.\n\
")),
#endif
    PARSE_STR(RC_COOKIE_LOOSE_INVALID_DOMAINS, LYCookieLooseCheckDomains, N_("\
cookie_loose_invalid_domains, cookie_strict_invalid_domains, and\n\
cookie_query_invalid_domains are comma-delimited lists of which domains\n\
should be subjected to varying degrees of validity checking.  If a\n\
domain is set to strict checking, strict conformance to RFC2109 will\n\
be applied.  A domain with loose checking will be allowed to set cookies\n\
with an invalid path or domain attribute.  All domains will default to\n\
querying the user for an invalid path or domain.\n\
")),
    PARSE_STR(RC_COOKIE_QUERY_INVALID_DOMAINS, LYCookieQueryCheckDomains, NULL),
    PARSE_LIS(RC_COOKIE_REJECT_DOMAINS, LYCookieRejectDomains, NULL),
    PARSE_STR(RC_COOKIE_STRICT_INVALID_DOMAIN, LYCookieStrictCheckDomains, NULL),
#ifdef DIRED_SUPPORT
#ifdef LONG_LIST
    PARSE_ENU(RC_DIR_LIST_ORDER,        dir_list_order,     tbl_dir_list_order, N_("\
dir_list_order specifies the directory list order under DIRED_SUPPORT\n\
(if implemented).  The default is \"ORDER_BY_NAME\"\n\
")),
#endif
    PARSE_ENU(RC_DIR_LIST_STYLE,        dir_list_style,     tbl_dir_list_style, N_("\
dir_list_styles specifies the directory list style under DIRED_SUPPORT\n\
(if implemented).  The default is \"MIXED_STYLE\", which sorts both\n\
files and directories together.  \"FILES_FIRST\" lists files first and\n\
\"DIRECTORIES_FIRST\" lists directories first.\n\
")),
#endif
    MAYBE_STR(RC_DISPLAY,               x_display,          MSG_ENABLE_LYNXRC),
    PARSE_SET(RC_EMACS_KEYS,            emacs_keys, N_("\
If emacs_keys is to \"on\" then the normal EMACS movement keys:\n\
  ^N = down    ^P = up\n\
  ^B = left    ^F = right\n\
will be enabled.\n\
")),
    PARSE_FUN(RC_FILE_EDITOR,           get_editor,         put_editor, N_("\
file_editor specifies the editor to be invoked when editing local files\n\
or sending mail.  If no editor is specified, then file editing is disabled\n\
unless it is activated from the command line, and the built-in line editor\n\
will be used for sending mail.\n\
")),
    PARSE_ENU(RC_FILE_SORTING_METHOD,   HTfileSortMethod,   tbl_file_sort, N_("\
The file_sorting_method specifies which value to sort on when viewing\n\
file lists such as FTP directories.  The options are:\n\
   BY_FILENAME -- sorts on the name of the file\n\
   BY_TYPE     -- sorts on the type of the file\n\
   BY_SIZE     -- sorts on the size of the file\n\
   BY_DATE     -- sorts on the date of the file\n\
")),
    MAYBE_ENU(RC_FORCE_COOKIE_PROMPT,   cookie_noprompt,    tbl_force_prompt,
	      MSG_ENABLE_LYNXRC),
#ifdef USE_SSL
    MAYBE_ENU(RC_FORCE_SSL_PROMPT,      ssl_noprompt,       tbl_force_prompt,
	      MSG_ENABLE_LYNXRC),
#endif
#ifdef EXP_KEYBOARD_LAYOUT
    PARSE_ARY(RC_KBLAYOUT,              current_layout,     LYKbLayoutNames, NULL),
#endif
    PARSE_ENU(RC_KEYPAD_MODE,           keypad_mode,        tbl_keypad_mode, NULL),
    PARSE_ARY(RC_LINEEDIT_MODE,         current_lineedit,   LYLineeditNames, N_("\
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
")),
#ifdef EXP_LOCALE_CHARSET
    MAYBE_SET(RC_LOCALE_CHARSET,      LYLocaleCharset,        MSG_ENABLE_LYNXRC),
#endif
    MAYBE_SET(RC_MAKE_PSEUDO_ALTS_FOR_INLINES, pseudo_inline_alts, MSG_ENABLE_LYNXRC),
    MAYBE_SET(RC_MAKE_LINKS_FOR_ALL_IMAGES, clickable_images, MSG_ENABLE_LYNXRC),
    PARSE_MBM(RC_MULTI_BOOKMARK, N_("\
The following allow you to define sub-bookmark files and descriptions.\n\
The format is multi_bookmark<capital_letter>=<filename>,<description>\n\
Up to 26 bookmark files (for the English capital letters) are allowed.\n\
We start with \"multi_bookmarkB\" since 'A' is the default (see above).\n\
")),
    PARSE_STR(RC_PERSONAL_MAIL_ADDRESS, personal_mail_address, N_("\
personal_mail_address specifies your personal mail address.  The\n\
address will be sent during HTTP file transfers for authorization and\n\
logging purposes, and for mailed comments.\n\
If you do not want this information given out, set the NO_FROM_HEADER\n\
to TRUE in lynx.cfg, or use the -nofrom command line switch.  You also\n\
could leave this field blank, but then you won't have it included in\n\
your mailed comments.\n\
")),
    PARSE_STR(RC_PREFERRED_CHARSET,     pref_charset, N_("\
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
")),
    PARSE_STR(RC_PREFERRED_LANGUAGE,    language, N_("\
preferred_language specifies the language in MIME notation (e.g., en,\n\
fr, may be a comma-separated list in decreasing preference)\n\
which Lynx will indicate you prefer in requests to http servers.\n\
If a file in that language is available, the server will send it.\n\
Otherwise, the server will send the file in its default language.\n\
")),
    MAYBE_SET(RC_RAW_MODE,              LYRawMode,          MSG_ENABLE_LYNXRC),
#if defined(ENABLE_OPTS_CHANGE_EXEC) && (defined(EXEC_LINKS) || defined(EXEC_SCRIPTS))
    PARSE_SET(RC_RUN_ALL_EXECUTION_LINKS, local_exec, N_("\
If run_all_execution_links is set \"on\" then all local execution links\n\
will be executed when they are selected.\n\
\n\
WARNING - This is potentially VERY dangerous.  Since you may view\n\
          information that is written by unknown and untrusted sources\n\
          there exists the possibility that Trojan horse links could be\n\
          written.  Trojan horse links could be written to erase files\n\
          or compromise security.  This should only be set to \"on\" if\n\
          you are viewing trusted source information.\n\
")),
    PARSE_SET(RC_RUN_EXECUTION_LINKS_LOCAL, local_exec_on_local_files, N_("\
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
")),
#endif
#ifdef USE_SCROLLBAR
    MAYBE_SET(RC_SCROLLBAR,             LYShowScrollbar, MSG_ENABLE_LYNXRC),
#endif
    PARSE_SET(RC_SELECT_POPUPS,         LYSelectPopups, N_("\
select_popups specifies whether the OPTIONs in a SELECT block which\n\
lacks a MULTIPLE attribute are presented as a vertical list of radio\n\
buttons or via a popup menu.  Note that if the MULTIPLE attribute is\n\
present in the SELECT start tag, Lynx always will create a vertical list\n\
of checkboxes for the OPTIONs.  A value of \"on\" will set popup menus\n\
as the default while a value of \"off\" will set use of radio boxes.\n\
The default can be overridden via the -popup command line toggle.\n\
")),
    MAYBE_SET(RC_SET_COOKIES,           LYSetCookies,      MSG_ENABLE_LYNXRC),
    PARSE_ENU(RC_SHOW_COLOR,            LYrcShowColor,     tbl_show_colors, N_("\
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
")),
    PARSE_SET(RC_SHOW_CURSOR,           LYShowCursor, N_("\
show_cursor specifies whether to 'hide' the cursor to the right (and\n\
bottom, if possible) of the screen, or to place it to the left of the\n\
current link in documents, or current option in select popup windows.\n\
Positioning the cursor to the left of the current link or option is\n\
helpful for speech or braille interfaces, and when the terminal is\n\
one which does not distinguish the current link based on highlighting\n\
or color.  A value of \"on\" will set positioning to the left as the\n\
default while a value of \"off\" will set 'hiding' of the cursor.\n\
The default can be overridden via the -show_cursor command line toggle.\n\
")),
    PARSE_SET(RC_SHOW_DOTFILES,         show_dotfiles, N_("\
show_dotfiles specifies that the directory listing should include\n\
\"hidden\" (dot) files/directories.  If set \"on\", this will be\n\
honored only if enabled via userdefs.h and/or lynx.cfg, and not\n\
restricted via a command line switch.  If display of hidden files\n\
is disabled, creation of such files via Lynx also is disabled.\n\
")),
#ifdef USE_READPROGRESS
    MAYBE_ENU(RC_SHOW_KB_RATE,          LYTransferRate,    tbl_transfer_rate,
	      MSG_ENABLE_LYNXRC),
#endif
    PARSE_ENU(RC_SUB_BOOKMARKS,         LYMultiBookmarks,  tbl_multi_bookmarks, N_("\
If sub_bookmarks is not turned \"off\", and multiple bookmarks have\n\
been defined (see below), then all bookmark operations will first\n\
prompt the user to select an active sub-bookmark file.  If the default\n\
Lynx bookmark_file is defined (see above), it will be used as the\n\
default selection.  When this option is set to \"advanced\", and the\n\
user mode is advanced, the 'v'iew bookmark command will invoke a\n\
statusline prompt instead of the menu seen in novice and intermediate\n\
user modes.  When this option is set to \"standard\", the menu will be\n\
presented regardless of user mode.\n\
")),
    MAYBE_FUN(RC_TAGSOUP,               get_tagsoup,        put_tagsoup,
              MSG_ENABLE_LYNXRC),
    MAYBE_SET(RC_UNDERLINE_LINKS,       LYUnderlineLinks,   MSG_ENABLE_LYNXRC),
    PARSE_ENU(RC_USER_MODE,             user_mode,          tbl_user_mode, N_("\
user_mode specifies the users level of knowledge with Lynx.  The\n\
default is \"NOVICE\" which displays two extra lines of help at the\n\
bottom of the screen to aid the user in learning the basic Lynx\n\
commands.  Set user_mode to \"INTERMEDIATE\" to turn off the extra info.\n\
Use \"ADVANCED\" to see the URL of the currently selected link at the\n\
bottom of the screen.\n\
")),
    MAYBE_STR(RC_USERAGENT,             LYUserAgent,        MSG_ENABLE_LYNXRC),
    PARSE_SET(RC_VERBOSE_IMAGES,        verbose_img, N_("\
If verbose_images is \"on\", lynx will print the name of the image\n\
source file in place of [INLINE], [LINK] or [IMAGE]\n\
See also VERBOSE_IMAGES in lynx.cfg\n\
")),
    PARSE_SET(RC_VI_KEYS,               vi_keys, N_("\
If vi_keys is set to \"on\", then the normal VI movement keys:\n\
  j = down    k = up\n\
  h = left    l = right\n\
will be enabled.  These keys are only lower case.\n\
Capital 'H', 'J' and 'K will still activate help, jump shortcuts,\n\
and the keymap display, respectively.\n\
")),
    PARSE_ENU(RC_VISITED_LINKS,         Visited_Links_As,   tbl_visited_links, N_("\
The visited_links setting controls how Lynx organizes the information\n\
in the Visited Links Page.\n\
")),

    PARSE_NIL
};

PRIVATE Config_Type *lookup_config ARGS1(
	char *,		name)
{
    Config_Type *tbl = Config_Table;
    char ch = (char) TOUPPER(*name);

    while (tbl->name != 0) {
	if (tbl->enabled) {
	    char ch1 = tbl->name[0];

	    if ((ch == TOUPPER(ch1))
		&& (0 == strcasecomp (name, tbl->name)))
		break;
	}

	tbl++;
    }
    return tbl;
}

/*  Read and process user options.
 *  If the passed-in fp is NULL, open the regular user defaults file
 *  for reading, otherwise use fp which has to be a file open for
 *  reading. - kw
 */
PUBLIC void read_rc ARGS1(FILE *, fp)
{
    char *buffer = NULL;
    char rcfile[LY_MAXPATH];
    char MBM_line[256];
    int  n;

    if (!fp) {
	/*
	 *  Make an RC file name, open it for reading.
	 */
	LYAddPathToHome(rcfile, sizeof(rcfile), FNAME_LYNXRC);
	if ((fp = fopen(rcfile, TXT_R)) == NULL) {
	    return;
	}
	CTRACE((tfp, "read_rc opened %s\n", rcfile));
    } else {
	CTRACE((tfp, "read_rc used passed-in stream\n"));
    }

    /*
     *  Process the entries.
     */
    while (LYSafeGets(&buffer, fp) != NULL) {
	char *name, *value, *notes;
	Config_Type *tbl;
	ParseUnionPtr q;

	/* Most lines in the config file are comment lines.  Weed them out
	 * now.  Also, leading whitespace is ok, so trim it.
	 */
	LYTrimTrailing(buffer);
	name = LYSkipBlanks(buffer);
	if (ispunct(UCH(*name)) || *name == '\0')
	    continue;

	/*
	 * Parse the "name=value" strings.
	 */
	if ((value = strchr(name, '=')) == 0) {
	    CTRACE((tfp, "LYrcFile: missing '=' %s\n", name));
	    continue;
	}
	*value++ = '\0';
	LYTrimTrailing(name);
	value = LYSkipBlanks(value);
	CTRACE2(TRACE_CFG, (tfp, "LYrcFile %s:%s\n", name, value));

	tbl = lookup_config(name);
	if (tbl->name == 0) {
	    char *special = RC_MULTI_BOOKMARK;
	    if (!strncasecomp(name, special, strlen(special))) {
		tbl = lookup_config(special);
	    }
	    /* lynx ignores unknown keywords */
	    if (tbl->name == 0) {
		CTRACE((tfp, "LYrcFile: ignored %s=%s\n", name, value));
		continue;
	    }
	}

	q = ParseUnionOf(tbl);
	switch (tbl->type) {
	case CONF_BOOL:
	    if (q->set_value != 0)
		*(q->set_value) = getBool (value);
	    break;

	case CONF_FUN:
	    if (q->fun_value != 0)
		(*(q->fun_value)) (value);
	    break;

	case CONF_ARRAY:
	    for (n = 0; tbl->strings[n] != 0; ++n) {
		if (!strcasecomp(value, tbl->strings[n])) {
		    *(q->int_value) = n;
		    break;
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

	case CONF_LIS:
	    if (q->str_value != 0) {
		if (*(q->str_value) != NULL)
		    StrAllocCat(*(q->str_value), ",");
		StrAllocCat(*(q->str_value), value);
	    }
	    break;

	case CONF_MBM:
	    for (n = 1; n <= MBM_V_MAXFILES; n++) {
		sprintf(MBM_line, "multi_bookmark%c", LYindex2MBM(n));

		if (!strcasecomp(name, MBM_line)) {
		    if ((notes = strchr(value, ',')) != 0) {
			*notes++ = '\0';
			LYTrimTrailing(value);
			notes = LYSkipBlanks(notes);
		    } else {
			notes = value + strlen(value);
		    }
		    StrAllocCopy(MBM_A_subbookmark[n], value);
		    StrAllocCopy(MBM_A_subdescript[n], notes);
		    break;
		}
	    }
	    break;

	case CONF_STR:
	    if (q->str_value != 0)
		StrAllocCopy(*(q->str_value), value);
	    break;

	case CONF_UNSPECIFIED:
	    break;
	}
    }

    LYCloseInput(fp);
    LYConfigCookies();	/* update cookie settings, if any */

#if defined(USE_SLANG) || defined(COLOR_CURSES)
    /*
     * We may override the commandline "-color" option with the .lynxrc file
     */
    switch (LYrcShowColor) {
    case SHOW_COLOR_ALWAYS:
	if (LYShowColor != SHOW_COLOR_NEVER)
	    LYShowColor = SHOW_COLOR_ALWAYS;
	break;
    case SHOW_COLOR_NEVER:
	if (LYShowColor == SHOW_COLOR_ON)
	    LYShowColor = SHOW_COLOR_OFF;
	break;
    default:
	/* don't override */
	break;
    }
#endif
    set_default_bookmark_page(bookmark_page);
}

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

/*
 * This is too long for some compilers.
 */
PRIVATE void explain_keypad_mode ARGS1(FILE *, fp)
{
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
}

/*  Save user options.
 *  If the passed-in fp is NULL, open the regular user defaults file
 *  for writing, otherwise use fp which has to be a temp file open for
 *  writing. - kw
 */
PUBLIC int save_rc ARGS1(FILE *, fp)
{
    Config_Type *tbl = Config_Table;
    char rcfile[LY_MAXPATH];
    BOOLEAN is_tempfile = (BOOL) (fp != NULL);
    int n;

    if (!fp) {
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
    }

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

    while (tbl->name != 0) {
	ParseUnionPtr q = ParseUnionOf(tbl);

	if (!tbl->enabled) {
	    tbl++;
	    continue;
	} if (tbl->note != NULL) {
	    write_list(fp, gettext(tbl->note));
	} else if (tbl->table == tbl_keypad_mode) {
	    explain_keypad_mode(fp);
	}

	switch (tbl->type) {
	case CONF_BOOL:
	    fprintf(fp, "%s=%s\n\n", tbl->name, putBool(*(q->set_value)));
	    break;

	case CONF_FUN:
	    if (tbl->write_it != 0)
		tbl->write_it(fp, tbl);
	    break;

	case CONF_ARRAY:
	    for (n = 0; tbl->strings[n] != 0; ++n)
		fprintf(fp, "#    %s\n", tbl->strings[n]);
	    fprintf(fp, "%s=%s\n\n", tbl->name,
		    tbl->strings[*(q->int_value)]);
	    break;

	case CONF_ENUM:
	    fprintf(fp, "%s=%s\n\n", tbl->name,
		    LYputEnum(tbl->table, *(q->int_value)));
	    break;

	case CONF_INT:
	    fprintf(fp, "%s=%d\n\n", tbl->name, *(q->int_value));
	    break;

	case CONF_MBM:
	    for (n = 1; n <= MBM_V_MAXFILES; n++) {
		fprintf(fp, "multi_bookmark%c=", LYindex2MBM(n));

		fprintf(fp, "%s", NonNull(MBM_A_subbookmark[n]));
		if (MBM_A_subdescript[n] != 0
		 && *MBM_A_subdescript[n] != 0)
		    fprintf(fp, ",%s", MBM_A_subdescript[n]);
		fprintf(fp, "\n");
	    }
	    fprintf(fp, "\n");
	    break;

	case CONF_LIS:
	    /* FALLTHRU */
	case CONF_STR:
	    fprintf(fp, "%s=%s\n\n", tbl->name,
			(q->str_value != 0 && *(q->str_value) != 0)
			    ? *(q->str_value)
			    : "");
	    break;

	case CONF_UNSPECIFIED:
	    break;
	}
	tbl++;
    }

    /*
     *  Close the RC file.
     */
    if (is_tempfile) {
	LYCloseTempFP(fp);
    } else {
	LYCloseOutput(fp);
	HTSYS_purge(rcfile);
    }

    return TRUE;
}

/*
 * Returns true if the given name would be saved in .lynxrc
 */
PUBLIC BOOL will_save_rc ARGS1(char *, name)
{
    Config_Type *tbl = lookup_config(name);
    return tbl->name != 0;
}

PUBLIC int enable_lynxrc ARGS1(
	char *,		value)
{
    Config_Type *tbl;
    char *colon = strchr(value, ':');

    if (colon != 0) {
	*colon++ = 0;
	LYTrimLeading(value);
	LYTrimTrailing(value);

	for (tbl = Config_Table; tbl->name != 0; tbl++) {
	    if (!strcasecomp(value, tbl->name)) {
		tbl->enabled = getBool(colon);
		CTRACE((tfp, "enable_lynxrc(%s) %s\n", value, putBool(tbl->enabled)));
		break;
	    }
	}
    }
    return 0;
}
