#include "HTUtils.h"
#include "tcp.h"
#include "HTFile.h"
#include "UCMap.h"

#include "LYUtils.h"
#include "LYStrings.h"
#include "LYStructs.h"
#include "LYGlobalDefs.h"
#include "LYCharSets.h"
#include "LYKeymap.h"
#include "LYJump.h"
#include "LYGetFile.h"
#include "LYCgi.h"
#include "LYCurses.h"
#include "LYSignal.h"
#include "LYBookmark.h"
#include "LYReadCFG.h"

#ifdef DIRED_SUPPORT
#include "LYLocal.h"
#endif /* DIRED_SUPPORT */

#include "LYexit.h"
#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

extern int HTNewsMaxChunk;  /* Max news articles before chunking (HTNews.c) */
extern int HTNewsChunkSize; /* Number of news articles per chunk (HTNews.c) */

PUBLIC BOOLEAN have_read_cfg=FALSE;
PUBLIC BOOLEAN LYUseNoviceLineTwo=TRUE;

#ifdef VMS
#define DISPLAY "DECW$DISPLAY"
#else
#define DISPLAY "DISPLAY"
#endif /* VMS */

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
	char *, 		buffer,
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
	    perror("Out of memory in read_cfg");
	*list_ptr = cur_item;
	atexit(free_item_list);
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
	    perror("Out of memory in read_cfg");
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
	    perror("Out of memory in read_cfg");
	LYstrncpy(cur_item->name, buffer, (int)(colon-buffer));
	remove_backslashes(cur_item->name);

	/*
	 *  Process TRUE/FALSE field
	 */
	if ((next_colon = find_colon(colon+1)) != NULL) {
	    cur_item->command = (char *)calloc(next_colon-colon, sizeof(char));
	    if (cur_item->command == NULL)
		perror("Out of memory in read_cfg");
	    LYstrncpy(cur_item->command, colon+1, (int)(next_colon-(colon+1)));
	    remove_backslashes(cur_item->command);
	    cur_item->always_enabled = is_true(next_colon+1);
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
	char *, 			buffer,
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
	    perror("Out of memory in read_cfg");
	*list_ptr = cur_item;
	atexit(free_printer_item_list);
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
	    perror("Out of memory in read_cfg");
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
	    perror("Out of memory in read_cfg");
	LYstrncpy(cur_item->name, buffer, (int)(colon-buffer));
	remove_backslashes(cur_item->name);

	/*
	 *  Process TRUE/FALSE field.
	 */
	if ((next_colon = find_colon(colon+1)) != NULL) {
	    cur_item->command = (char *)calloc(next_colon-colon, sizeof(char));
	    if (cur_item->command == NULL)
		perror("Out of memory in read_cfg");
	    LYstrncpy(cur_item->command, colon+1, (int)(next_colon-(colon+1)));
	    remove_backslashes(cur_item->command);
	    cur_item->always_enabled = is_true(next_colon+1);
	}

	/*
	 *  Process pagelen field.
	 */
	if ((next_colon = find_colon(next_colon+1)) != NULL) {
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

static char *Color_Strings[16] =
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
	char *, 	error_line)
{
    unsigned int i;
    fprintf (stderr, "\
Syntax Error parsing COLOR in configuration file:\n\
The line must be of the form:\n\
COLOR:INTEGER:FOREGROUND:BACKGROUND\n\
\n\
Here FOREGROUND and BACKGROUND must be one of:\n\
The special strings 'nocolor' or 'default', or\n"
	    );
    for (i = 0; i < 16; i += 4) {
	fprintf(stderr, "%16s %16s %16s %16s\n",
		Color_Strings[i], Color_Strings[i + 1],
		Color_Strings[i + 2], Color_Strings[i + 3]);
    }
    fprintf (stderr, "Offending line:\n%s\n",error_line);

#ifndef NOSIGHUP
    (void) signal(SIGHUP, SIG_DFL);
#endif /* NOSIGHUP */
    (void) signal(SIGTERM, SIG_DFL);
#ifndef VMS
    (void) signal(SIGINT, SIG_DFL);
#endif /* !VMS */
#ifdef SIGTSTP
    if (no_suspend)
	(void) signal(SIGTSTP,SIG_DFL);
#endif /* SIGTSTP */
    exit(-1);
}

/*
 *  Process string buffer fields for COLOR setting.
 */
PRIVATE void parse_color ARGS1(
	char *, buffer)
{
    int color;
    char *fg, *bg;
    char temp[501];

    if (strlen(buffer) < sizeof(temp))
	strcpy(temp, buffer);
    else
	strcpy(temp, "Color config line too long");

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
}
#endif /* USE_COLOR_TABLE */

/*
 * Process the configuration file (lynx.cfg).
 */
PUBLIC void read_cfg ARGS1(
	char *, cfg_filename)
{
    FILE *fp;
    char buffer[501];
    char temp[501];
    char *cp, *cp1;
    int i, j, len;

    /*
     *	Locate and open the file.
     */
    if (!cfg_filename || strlen(cfg_filename) == 0) {
	if (TRACE)
	    fprintf(stderr,"No filename following -cfg switch!\n");
	return;
    }
    if ((fp = fopen(cfg_filename,"r")) == NULL) {
	if (TRACE)
	    fprintf(stderr,"lynx.cfg file not found as %s\n",cfg_filename);
	return;
    }
    have_read_cfg=TRUE;

    /*
     *	Process each line in the file.
     */
    while (fgets(buffer, 500, fp) != NULL) {
	/*
	 *  Strip off \n at the end.
	 */
	if ((cp = (char *)strchr(buffer, '\n')) != NULL)
	    *cp = '\0';

	/*
	 *  Trim off any trailing comments.
	 */
	if ((cp = (char *)strrchr(buffer, ':')) != NULL) {
	    if ((cp1 = (char *)strchr(cp, '#')) != NULL) {
		cp1--;
		if (isspace((unsigned char)*cp1)) {
		    *cp1 = '\0';
		}
	    }
	}

	/*
	 *  Strip off trailing white space.
	 */
	len = strlen(buffer);
	while (len && isspace(buffer[len-1])) {
	    len--;
	    buffer[len] = '\0';
	}

	/*
	 *  Skip any blank or purely comment lines.
	 */
	if (buffer[0] == '\0' || buffer[0] == '#')
	    continue;

	/*
	 * Process the string buffer.
	 */
	switch (TOUPPER(buffer[0])) {

	case 'A':
	if (!strncasecomp(buffer, "ALERTSECS:", 10)) {
	    strcpy(temp, buffer+10);
	    for (i = 0; temp[i]; i++) {
		if (!isdigit(temp[i])) {
		    temp[i] = '\0';
		    break;
		}
	    }
	    if (temp[0])
		AlertSecs = atoi(temp);

	} else if (!strncasecomp(buffer, "ALWAYS_RESUBMIT_POSTS:", 22)) {
	    LYresubmit_posts = is_true(buffer+22);

#ifdef EXEC_LINKS
	} else if (!strncasecomp(buffer, "ALWAYS_TRUSTED_EXEC:", 20)) {
	    add_trusted(&buffer[20], ALWAYS_EXEC_PATH); /* Add exec path */
#endif /* EXEC_LINKS */

	} else if (!strncasecomp(buffer, "ASSUME_CHARSET:", 15)) {
	    StrAllocCopy(UCAssume_MIMEcharset, buffer+15);
	    for (i = 0; UCAssume_MIMEcharset[i]; i++)
		UCAssume_MIMEcharset[i] = TOLOWER(UCAssume_MIMEcharset[i]);
	    UCLYhndl_for_unspec =
		UCGetLYhndl_byMIME(UCAssume_MIMEcharset);

	} else if (!strncasecomp(buffer, "ASSUME_LOCAL_CHARSET:", 21)) {
	    StrAllocCopy(UCAssume_localMIMEcharset, buffer+21);
	    for (i = 0; UCAssume_localMIMEcharset[i]; i++)
		UCAssume_localMIMEcharset[i] =
			TOLOWER(UCAssume_localMIMEcharset[i]);
	    UCLYhndl_HTFile_for_unspec =
		UCGetLYhndl_byMIME(UCAssume_localMIMEcharset);

	} else if (!strncasecomp(buffer, "ASSUME_UNREC_CHARSET:", 21)) {
	    StrAllocCopy(UCAssume_unrecMIMEcharset, buffer+21);
	    for (i = 0; UCAssume_unrecMIMEcharset[i]; i++)
		UCAssume_unrecMIMEcharset[i] =
			TOLOWER(UCAssume_unrecMIMEcharset[i]);
	    UCLYhndl_for_unrec =
		UCGetLYhndl_byMIME(UCAssume_unrecMIMEcharset);
	}
	break;

	case 'B':
	if (!strncasecomp(buffer, "BLOCK_MULTI_BOOKMARKS:", 22)) {
	    LYMBMBlocked = is_true(buffer+22);

	} else if (!strncasecomp(buffer, "BOLD_H1:", 8)) {
	    bold_H1 = is_true(buffer+8);

	} else if (!strncasecomp(buffer, "BOLD_HEADERS:", 13)) {
	    bold_headers = is_true(buffer+13);

	} else if (!strncasecomp(buffer, "BOLD_NAME_ANCHORS:", 18)) {
	    bold_name_anchors = is_true(buffer+18);
	}
	break;

	case 'C':
	if (!strncasecomp(buffer, "CASE_SENSITIVE_ALWAYS_ON:", 25)) {
	     case_sensitive = is_true(buffer+25);

	} else if (!strncasecomp(buffer, "CHARACTER_SET:", 14)) {
	    for (i = 0; LYchar_set_names[i]; i++) {
		if (!strncmp(buffer+14,
			     LYchar_set_names[i], strlen(buffer+14))) {
		    current_char_set=i;
		    HTMLSetRawModeDefault(i);
		    break;
		}
	    }

	} else if (!strncasecomp(buffer, "CHECKMAIL:", 10)) {
	    check_mail = is_true(buffer+10);

	} else if (!strncasecomp(buffer, "COLLAPSE_BR_TAGS:", 17)) {
		LYCollapseBRs = is_true(buffer+17);

#if USE_COLOR_TABLE
	} else if (!strncasecomp(buffer, "COLOR:", 6)) {
		parse_color(buffer + 6);
#endif /* USE_COLOR_TABLE */

	} else if (!strncasecomp(buffer, "cso_proxy:", 10)) {
	    if (getenv("cso_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "cso_proxy");
		Define_VMSLogical(temp, (char *)&buffer[10]);
#else
		strcpy(temp, "cso_proxy=");
		StrAllocCopy(cso_proxy_putenv_cmd, temp);
		StrAllocCat(cso_proxy_putenv_cmd, (char *)&buffer[10]);
		putenv(cso_proxy_putenv_cmd);
#endif /* VMS */
	    }

#ifdef VMS
	} else if (!strncasecomp(buffer, "CSWING_PATH:", 12)) {
	    StrAllocCopy(LYCSwingPath, buffer+12);
#endif /* VMS */
	}
	break;

	case 'D':
	if (!strncasecomp(buffer, "DEFAULT_BOOKMARK_FILE:", 22)) {
	    StrAllocCopy(bookmark_page, buffer+22);
	    StrAllocCopy(BookmarkPage, bookmark_page);
	    StrAllocCopy(MBM_A_subbookmark[0], bookmark_page);
	    StrAllocCopy(MBM_A_subdescript[0], MULTIBOOKMARKS_DEFAULT);

	} else if (!strncasecomp(buffer, "DEFAULT_CACHE_SIZE:", 19)) {
		HTCacheSize = atoi(buffer+19);

	} else if (!system_editor &&
		   !strncasecomp(buffer, "DEFAULT_EDITOR:", 15)) {
	    StrAllocCopy(editor, buffer+15);

	} else if (!strncasecomp(buffer, "DEFAULT_INDEX_FILE:", 19)) {
	    StrAllocCopy(indexfile, buffer+19);

	} else if (!strncasecomp(buffer,
			"DEFAULT_KEYPAD_MODE_IS_NUMBERS_AS_ARROWS:", 41)) {
	    if (is_true(buffer+41))
		keypad_mode = NUMBERS_AS_ARROWS;
	    else
		keypad_mode = LINKS_ARE_NUMBERED;

	} else if (!strncasecomp(buffer, "DEFAULT_USER_MODE:", 18)) {
	    if (!strncasecomp(buffer+18, "NOVICE", 6))
		user_mode = NOVICE_MODE;
	    else if (!strncasecomp(buffer+18, "INTER", 5))
		user_mode = INTERMEDIATE_MODE;
	    else if (!strncasecomp(buffer+18, "ADVANCE", 7))
		user_mode = ADVANCED_MODE;

#if defined(VMS) && defined(VAXC) && !defined(__DECC)
	} else if (!strncasecomp(buffer,
				 "DEFAULT_VIRTUAL_MEMORY_SIZE:", 28)) {
		HTVirtualMemorySize = atoi(buffer+28);
#endif /* VMS && VAXC && !__DECC */

#ifdef DIRED_SUPPORT
	} else if (!strncasecomp(buffer, "DIRED_MENU:", 11)) {
	    add_menu_item(&buffer[11]);
#endif /* DIRED_SUPPORT */

	} else if (!strncasecomp(buffer, "DOWNLOADER:", 11)) {
	    add_item_to_list(&buffer[11], &downloaders);
	}
	break;

	case 'E':
	if (!strncasecomp(buffer, "EMACS_KEYS_ALWAYS_ON:", 21)) {
	    emacs_keys = is_true(buffer+21);

	} else if (!strncasecomp(buffer, "ENABLE_SCROLLBACK:", 18)) {
	    enable_scrollback = is_true(buffer+18);
	}
#ifdef USE_EXTERNALS
	else if(!strncasecomp(buffer,"EXTERNAL:",9)) {
	    add_item_to_list(&buffer[9],&externals);
	}
#endif
	break;

	case 'F':
	if (!strncasecomp(buffer, "finger_proxy:", 13)) {
	    if (getenv("finger_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "finger_proxy");
		Define_VMSLogical(temp, (char *)&buffer[13]);
#else
		strcpy(temp, "finger_proxy=");
		StrAllocCopy(finger_proxy_putenv_cmd, temp);
		StrAllocCat(finger_proxy_putenv_cmd, (char *)&buffer[13]);
		putenv(finger_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "FORCE_SSL_COOKIES_SECURE:", 25)) {
	    LYForceSSLCookiesSecure = is_true(buffer+25);

	} else if (!strncasecomp(buffer, "ftp_proxy:", 10)) {
	    if (getenv("ftp_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "ftp_proxy");
		Define_VMSLogical(temp, (char *)&buffer[10]);
#else
		strcpy(temp, "ftp_proxy=");
		StrAllocCopy(ftp_proxy_putenv_cmd, temp);
		StrAllocCat(ftp_proxy_putenv_cmd, (char *)&buffer[10]);
		putenv(ftp_proxy_putenv_cmd);
#endif /* VMS */
	    }
	}
	break;

	case 'G':
	if (!strncasecomp(buffer, "GLOBAL_EXTENSION_MAP:", 21)) {
	    StrAllocCopy(global_extension_map, buffer+21);

	} else if (!strncasecomp(buffer, "GLOBAL_MAILCAP:", 15)) {
	    StrAllocCopy(global_type_map, buffer+15);

	} else if (!strncasecomp(buffer, "gopher_proxy:", 13)) {
	    if (getenv("gopher_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "gopher_proxy");
		Define_VMSLogical(temp, (char *)&buffer[13]);
#else
		strcpy(temp, "gopher_proxy=");
		StrAllocCopy(gopher_proxy_putenv_cmd, temp);
		StrAllocCat(gopher_proxy_putenv_cmd, (char *)&buffer[13]);
		putenv(gopher_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "GOTOBUFFER:", 11)) {
	    goto_buffer = is_true(buffer+11);
	}
	break;

	case 'H':
	if (!strncasecomp(buffer, "HELPFILE:", 9)) {
	    StrAllocCopy(helpfile, buffer+9);

	} else if (!strncasecomp(buffer, "HISTORICAL_COMMENTS:", 20)) {
	    historical_comments = is_true(buffer+20);

	} else if (!strncasecomp(buffer, "http_proxy:", 11)) {
	    if (getenv("http_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "http_proxy");
		Define_VMSLogical(temp, (char *)&buffer[11]);
#else
		strcpy(temp, "http_proxy=");
		StrAllocCopy(http_proxy_putenv_cmd, temp);
		StrAllocCat(http_proxy_putenv_cmd, (char *)&buffer[11]);
		putenv(http_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "https_proxy:", 12)) {
	    if (getenv("https_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "https_proxy");
		Define_VMSLogical(temp, (char *)&buffer[12]);
#else
		strcpy(temp, "https_proxy=");
		StrAllocCopy(https_proxy_putenv_cmd, temp);
		StrAllocCat(https_proxy_putenv_cmd, (char *)&buffer[12]);
		putenv(https_proxy_putenv_cmd);
#endif /* VMS */
	    }
	}
	break;

	case 'I':
	if (!strncasecomp(buffer, "INFOSECS:", 9)) {
	    strcpy(temp, buffer+9);
	    for (i = 0; temp[i]; i++) {
		if (!isdigit(temp[i])) {
		    temp[i] = '\0';
		    break;
		}
	    }
	    if (temp[0])
		InfoSecs = atoi(temp);
	}
	break;

	case 'J':
	if (!strncasecomp(buffer, "JUMP_PROMPT:", 12)) {
	    StrAllocCopy(jumpprompt, buffer+12);

	} else if (!strncasecomp(buffer, "JUMPBUFFER:", 11)) {
	    jump_buffer = is_true(buffer+11);

	} else if (!strncasecomp(buffer, "JUMPFILE:", 9)) {
	    if (!LYJumpInit(buffer)) {
		if (TRACE)
		    fprintf(stderr, "Failed to register %s\n", buffer);
	    }
	}
	break;

	case 'K':
	if (!strncasecomp(buffer, "KEYMAP:", 7)) {
	    char *key;
	    char *func;

	    key = buffer + 7;
	    if ((func = strchr(key, ':')) != NULL)	{
		*func++ = '\0';
		/* Allow comments on the ends of key remapping lines. - DT */
		if (!remap(key, strtok(func, " \t\n#")))
		    fprintf(stderr,
			    "key remapping of %s to %s failed\n",key,func);
		else if (!strcmp("TOGGLE_HELP", strtok(func, " \t\n#")))
		    LYUseNoviceLineTwo = FALSE;
	    }
	}
	break;

	case 'L':
	if (!strncasecomp(buffer, "LIST_NEWS_NUMBERS:", 18)) {
		LYListNewsNumbers = is_true(buffer+18);

	} else if (!strncasecomp(buffer, "LIST_NEWS_DATES:", 16)) {
		LYListNewsDates = is_true(buffer+16);

#ifndef VMS
	} else if (!strncasecomp(buffer, "LIST_FORMAT:", 12)) {
	    StrAllocCopy(list_format, buffer+12);
#endif /* !VMS */

	} else if (!strncasecomp(buffer, "LOCALHOST_ALIAS:", 16)) {
	    LYAddLocalhostAlias(buffer+16);

	} else if (!strncasecomp(buffer, "LOCAL_DOMAIN:", 13)) {
	    StrAllocCopy(LYLocalDomain, buffer+13);

#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
	} else if (!strncasecomp(buffer,
			 "LOCAL_EXECUTION_LINKS_ALWAYS_ON:", 32)) {
	    local_exec = is_true(buffer+32);

	} else if (!strncasecomp(buffer,
			 "LOCAL_EXECUTION_LINKS_ON_BUT_NOT_REMOTE:", 40)) {
	    local_exec_on_local_files = is_true(buffer+40);
#endif /* defined(EXEC_LINKS) || defined(EXEC_SCRIPTS) */

#ifdef LYNXCGI_LINKS
	} else if (!strncasecomp(buffer, "LYNXCGI_ENVIRONMENT:", 20)) {
	    add_lynxcgi_environment(buffer+20);

#ifndef VMS
	/* WebSter Mods - Next 2 Lines -jkt */
	} else if (!strncasecomp(buffer, "LYNXCGI_DOCUMENT_ROOT:", 22)) {
	    StrAllocCopy(LYCgiDocumentRoot, buffer+22);
#endif /* !VMS */
#endif /* LYNXCGI_LINKS */

	} else if (!strncasecomp(buffer, "LYNX_HOST_NAME:", 15)) {
	    StrAllocCopy(LYHostName, buffer+15);

	} else if (!strncasecomp(buffer, "LYNX_SIG_FILE:", 14)) {
	    strcpy(temp, (buffer+14));
	    if (LYPathOffHomeOK(temp, 256)) {
		StrAllocCopy(LynxSigFile, temp);
		LYAddPathToHome(temp, 256, LynxSigFile);
		StrAllocCopy(LynxSigFile, temp);
		if (TRACE)
		    fprintf(stderr,
			    "LYNX_SIG_FILE set to '%s'\n", LynxSigFile);
	    } else {
		if (TRACE)
		    fprintf(stderr, "LYNX_SIG_FILE '%s' is bad. Ignoring.\n",
				    LYNX_SIG_FILE);
	    }
	}
	break;

	case 'M':
	if (!strncasecomp(buffer, "MAIL_SYSTEM_ERROR_LOGGING:", 26)) {
	    error_logging = is_true(buffer+26);

#ifdef VMS
	} else if (!strncasecomp(buffer, "MAIL_ADRS:", 10)) {
	    StrAllocCopy(mail_adrs, buffer+10);
#endif /* VMS */

	} else if (!strncasecomp(buffer, "MAKE_LINKS_FOR_ALL_IMAGES:", 26)) {
	    clickable_images = is_true(buffer+26);

	} else if (!strncasecomp(buffer,
				 "MAKE_PSEUDO_ALTS_FOR_INLINES:", 29)) {
	    pseudo_inline_alts = is_true(buffer+29);

	} else if (!strncasecomp(buffer, "MESSAGESECS:", 12)) {
	    strcpy(temp, buffer+12);
	    for (i = 0; temp[i]; i++) {
		if (!isdigit(temp[i])) {
		    temp[i] = '\0';
		    break;
		}
	    }
	    if (temp[0])
		MessageSecs = atoi(temp);

	} else if (!strncasecomp(buffer, "MINIMAL_COMMENTS:", 17)) {
	    minimal_comments = is_true(buffer+17);

	} else if (!strncasecomp(buffer, "MULTI_BOOKMARK_SUPPORT:", 23)) {
	    LYMultiBookmarks = is_true(buffer+23);
	}
	break;

	case 'N':
	if (!strncasecomp(buffer, "NEWS_CHUNK_SIZE:", 16)) {
		HTNewsChunkSize = atoi(buffer+16);
		/*
		 * If the new HTNewsChunkSize exceeds the maximum,
		 * increase HTNewsMaxChunk to this size. - FM
		 */
		if (HTNewsChunkSize > HTNewsMaxChunk) {
		    HTNewsMaxChunk = HTNewsChunkSize;
		}

	} else if (!strncasecomp(buffer, "NEWS_MAX_CHUNK:", 15)) {
		HTNewsMaxChunk = atoi(buffer+15);
		/*
		 * If HTNewsChunkSize exceeds the new maximum,
		 * reduce HTNewsChunkSize to this maximum. - FM
		 */
		if (HTNewsChunkSize > HTNewsMaxChunk) {
		    HTNewsChunkSize = HTNewsMaxChunk;
		}

	} else if (!strncasecomp(buffer, "NEWS_POSTING:", 13)) {
	    LYNewsPosting = is_true(buffer+13);
	    no_newspost = (LYNewsPosting == FALSE);

	} else if (!strncasecomp(buffer, "news_proxy:", 11)) {
	    if (getenv("news_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "news_proxy");
		Define_VMSLogical(temp, (char *)&buffer[11]);
#else
		strcpy(temp, "news_proxy=");
		StrAllocCopy(news_proxy_putenv_cmd, temp);
		StrAllocCat(news_proxy_putenv_cmd, (char *)&buffer[11]);
		putenv(news_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "newspost_proxy:", 15)) {
	    if (getenv("newspost_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "newspost_proxy");
		Define_VMSLogical(temp, (char *)&buffer[15]);
#else
		strcpy(temp, "newspost_proxy=");
		StrAllocCopy(newspost_proxy_putenv_cmd, temp);
		StrAllocCat(newspost_proxy_putenv_cmd, (char *)&buffer[15]);
		putenv(newspost_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "newsreply_proxy:", 16)) {
	    if (getenv("newsreply_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "newsreply_proxy");
		Define_VMSLogical(temp, (char *)&buffer[16]);
#else
		strcpy(temp, "newsreply_proxy=");
		StrAllocCopy(newsreply_proxy_putenv_cmd, temp);
		StrAllocCat(newsreply_proxy_putenv_cmd, (char *)&buffer[16]);
		putenv(newsreply_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "nntp_proxy:", 11)) {
	    if (getenv("nntp_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "nntp_proxy");
		Define_VMSLogical(temp, (char *)&buffer[11]);
#else
		strcpy(temp, "nntp_proxy=");
		StrAllocCopy(nntp_proxy_putenv_cmd, temp);
		StrAllocCat(nntp_proxy_putenv_cmd, (char *)&buffer[11]);
		putenv(nntp_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "NNTPSERVER:", 11)) {
	    if (getenv("NNTPSERVER") == NULL) {
#ifdef VMS
		strcpy(temp, "NNTPSERVER");
		Define_VMSLogical(temp, (char *)&buffer[11]);
#else
		strcpy(temp, "NNTPSERVER=");
		StrAllocCopy(NNTPSERVER_putenv_cmd, temp);
		StrAllocCat(NNTPSERVER_putenv_cmd, (char *)&buffer[11]);
		putenv(NNTPSERVER_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "NO_DOT_FILES:", 13)) {
	    no_dotfiles = is_true(buffer+13);

	} else if (!strncasecomp(buffer, "NO_FILE_REFERER:", 16)) {
	    no_filereferer = is_true(buffer+16);

#ifndef VMS
	} else if (!strncasecomp(buffer, "NO_FORCED_CORE_DUMP:", 20)) {
	    LYNoCore = is_true(buffer+20);
#endif /* !VMS */

	} else if (!strncasecomp(buffer, "NO_FROM_HEADER:", 15)) {
	    LYNoFromHeader = is_true(buffer+15);

	} else if (!strncasecomp(buffer, "NO_ISMAP_IF_USEMAP:", 19)) {
	    LYNoISMAPifUSEMAP = is_true(buffer+19);

	} else if (!strncasecomp(buffer, "no_proxy:", 9)) {
	    if (getenv("no_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "no_proxy");
		Define_VMSLogical(temp, (char *)&buffer[9]);
#else
		strcpy(temp, "no_proxy=");
		StrAllocCopy(no_proxy_putenv_cmd, temp);
		StrAllocCat(no_proxy_putenv_cmd, (char *)&buffer[9]);
		putenv(no_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "NO_REFERER_HEADER:", 18)) {
	    LYNoRefererHeader = is_true(buffer+18);
	}
	break;

	case 'P':
	if (!strncasecomp(buffer, "PERSONAL_MAILCAP:", 17)) {
	    StrAllocCopy(personal_type_map, buffer+17);

	} else if (!strncasecomp(buffer, "PERSONAL_EXTENSION_MAP:", 23)) {
	    StrAllocCopy(personal_extension_map, buffer+23);

	} else if (!strncasecomp(buffer, "PREFERRED_CHARSET:", 18)) {
	    StrAllocCopy(pref_charset, buffer+18);

	} else if (!strncasecomp(buffer, "PREFERRED_LANGUAGE:", 19)) {
	    StrAllocCopy(language, buffer+19);

	} else if (!strncasecomp(buffer, "PREPEND_BASE_TO_SOURCE:", 23)) {
	    LYPrependBaseToSource = is_true(buffer+23);

	} else if (!strncasecomp(buffer, "PREPEND_CHARSET_TO_SOURCE:", 26)) {
	    LYPrependCharsetToSource = is_true(buffer+26);

	} else if (!strncasecomp(buffer, "PRINTER:", 8)) {
	    add_printer_to_list(&buffer[8], &printers);
	}
	break;

#ifdef RAWDOSKEYHACK
	case 'R':
	if (!strncasecomp(buffer, "RAW_DOS_KEY_HACK:", 17)) {
	    raw_dos_key_hack = is_true(buffer+17);
	}
	break;
#endif /* RAWDOSKEYHACK */

	case 'Q':
	if (!strncasecomp(buffer, "QUIT_DEFAULT_YES:", 17)) {
	    LYQuitDefaultYes = is_true(buffer+17);
	}
	break;

	case 'S':
	if (!strncasecomp(buffer, "SAVE_SPACE:", 11)) {
	    StrAllocCopy(lynx_save_space, buffer+11);

	} else if (!strncasecomp(buffer, "SCAN_FOR_BURIED_NEWS_REFS:", 26)) {
	    scan_for_buried_news_references = is_true(buffer+26);

	} else if (!strncasecomp(buffer, "SEEK_FRAG_AREA_IN_CUR:", 22)) {
	    LYSeekFragAREAinCur = is_true(buffer+22);

	} else if (!strncasecomp(buffer, "SEEK_FRAG_MAP_IN_CUR:", 21)) {
	    LYSeekFragMAPinCur = is_true(buffer+21);

	} else if (!strncasecomp(buffer, "SET_COOKIES:", 12)) {
	    LYSetCookies = is_true(buffer+12);

	} else if (!strncasecomp(buffer, "SHOW_CURSOR:", 12)) {
	    LYShowCursor = is_true(buffer+12);

	} else if (!strncasecomp(buffer, "snews_proxy:", 12)) {
	    if (getenv("snews_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "snews_proxy");
		Define_VMSLogical(temp, (char *)&buffer[12]);
#else
		strcpy(temp, "snews_proxy=");
		StrAllocCopy(snews_proxy_putenv_cmd, temp);
		StrAllocCat(snews_proxy_putenv_cmd, (char *)&buffer[12]);
		putenv(snews_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "snewspost_proxy:", 16)) {
	    if (getenv("snewspost_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "snewspost_proxy");
		Define_VMSLogical(temp, (char *)&buffer[16]);
#else
		strcpy(temp, "snewspost_proxy=");
		StrAllocCopy(snewspost_proxy_putenv_cmd, temp);
		StrAllocCat(snewspost_proxy_putenv_cmd, (char *)&buffer[16]);
		putenv(snewspost_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "snewsreply_proxy:", 17)) {
	    if (getenv("snewsreply_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "snewsreply_proxy");
		Define_VMSLogical(temp, (char *)&buffer[17]);
#else
		strcpy(temp, "snewsreply_proxy=");
		StrAllocCopy(snewsreply_proxy_putenv_cmd, temp);
		StrAllocCat(snewsreply_proxy_putenv_cmd, (char *)&buffer[17]);
		putenv(snewsreply_proxy_putenv_cmd);
#endif /* VMS */
	    }

	} else if (!strncasecomp(buffer, "SOFT_DQUOTES:", 13)) {
	    soft_dquotes = is_true(buffer+13);

	} else if (!strncasecomp(buffer, "STARTFILE:", 10)) {
	    StrAllocCopy(startfile, buffer+10);

	} else if (!strncasecomp(buffer, "STRIP_DOTDOT_URLS:", 18)) {
	    LYStripDotDotURLs = is_true(buffer+18);

	} else if (!strncasecomp(buffer, "SUBSTITUTE_UNDERSCORES:", 23)) {
	    use_underscore = is_true(buffer+23);

	} else if (!strncasecomp(buffer, "SUFFIX:", 7)) {
	    char *extention;
	    char *mime_type;

	    if (strlen(buffer) > 9) {
		extention = buffer + 7;
		if ((mime_type = strchr(extention, ':')) != NULL) {
		    *mime_type++ = '\0';
		    for (i = 0, j = 0; mime_type[i]; i++) {
			if (mime_type[i] != ' ') {
			    mime_type[j++] = TOLOWER(mime_type[i]);
			}
		    }
		    mime_type[j] = '\0';
		    if (strstr(mime_type, "tex") != NULL ||
			strstr(mime_type, "postscript") != NULL ||
			strstr(mime_type, "sh") != NULL ||
			strstr(mime_type, "troff") != NULL ||
			strstr(mime_type, "rtf") != NULL)
			HTSetSuffix(extention, mime_type, "8bit", 1.0);
		    else
			HTSetSuffix(extention, mime_type, "binary", 1.0);
		}
	    }

	} else if (!strncasecomp(buffer, "SYSTEM_EDITOR:", 14)) {
	    StrAllocCopy(editor, buffer+14);
	    system_editor = TRUE;

	} else if (!strncasecomp(buffer, "SYSTEM_MAIL:", 12)) {
	    StrAllocCopy(system_mail, buffer+12);

	} else if (!strncasecomp(buffer, "SYSTEM_MAIL_FLAGS:", 18)) {
	    StrAllocCopy(system_mail_flags, buffer+18);
	}
	break;

	case 'T':
#ifdef EXEC_LINKS
	if (!strncasecomp(buffer, "TRUSTED_EXEC:", 13)) {
	    add_trusted(&buffer[13], EXEC_PATH); /* Add exec path */
	}
#endif /* EXEC_LINKS */

#ifdef LYNXCGI_LINKS
	if (!strncasecomp(buffer, "TRUSTED_LYNXCGI:", 16)) {
	    add_trusted(&buffer[16], CGI_PATH); /* Add CGI path */
	}
#endif /* LYNXCGI_LINKS */
	break;

	case 'U':
	if (!strncasecomp(buffer, "URL_DOMAIN_PREFIXES:", 20)) {
	    StrAllocCopy(URLDomainPrefixes, buffer+20);

	} else if (!strncasecomp(buffer, "URL_DOMAIN_SUFFIXES:", 20)) {
	    StrAllocCopy(URLDomainSuffixes, buffer+20);

#ifdef DIRED_SUPPORT
	} else if (!strncasecomp(buffer, "UPLOADER:", 9)) {
	    add_item_to_list(&buffer[9], &uploaders);
#endif /* DIRED_SUPPORT */

#ifdef VMS
	} else if (!strncasecomp(buffer, "USE_FIXED_RECORDS:", 18)) {
	    UseFixedRecords = is_true(buffer+18);
#endif /* VMS */

#if defined(NCURSES_MOUSE_VERSION) || defined(USE_SLANG_MOUSE)
	} else if(!strncasecomp(buffer, "USE_MOUSE:",10)) {
		LYUseMouse = is_true(buffer+10);
#endif

	} else if (!strncasecomp(buffer, "USE_SELECT_POPUPS:", 18)) {
	    LYSelectPopups = is_true(buffer+18);
	}
	break;

	case 'V':
	if (!strncasecomp(buffer, "VI_KEYS_ALWAYS_ON:", 18)) {
	    vi_keys = is_true(buffer+18);

	} else if (!strncasecomp(buffer, "VIEWER:", 7)) {
	    char *mime_type;
	    char *viewer;
	    char *environment;

	    if (strlen(buffer) > 9) {
		mime_type = buffer + 7;
		if ((viewer = strchr(mime_type, ':')) != NULL) {
		    *viewer++ = '\0';
		    for (i = 0, j = 0; mime_type[i]; i++) {
			if (mime_type[i] != ' ') {
			    mime_type[j++] = TOLOWER(mime_type[i]);
			}
		    }
		    mime_type[j] = '\0';
		    environment = strrchr(viewer, ':');
		    if ((environment != NULL) &&
			(strlen(viewer) > 1) && *(environment-1) != '\\') {
			*environment++ = '\0';
			remove_backslashes(viewer);
			/*
			 *  If environment equals xwindows then only
			 *  assign the presentation if there is a display
			 *  variable.
			 */
			if (!strcasecomp(environment,"XWINDOWS")) {
			    if ((cp = getenv(DISPLAY)) != NULL &&
				*cp != '\0')
				HTSetPresentation(mime_type, viewer,
						  1.0, 3.0, 0.0, 0);
			} else if (!strcasecomp(environment,"NON_XWINDOWS")) {
			    if ((cp = getenv(DISPLAY)) == NULL ||
				*cp == '\0')
				HTSetPresentation(mime_type, viewer,
						  1.0, 3.0, 0.0, 0);
			} else {
			    HTSetPresentation(mime_type, viewer,
						  1.0, 3.0, 0.0, 0);
			}
		    } else {
			remove_backslashes(viewer);
			HTSetPresentation(mime_type, viewer,
						  1.0, 3.0, 0.0, 0);
		    }
		}
	    }
	}
	break;

	case 'W':
	if (!strncasecomp(buffer, "wais_proxy:", 11)) {
	    if (getenv("wais_proxy") == NULL) {
#ifdef VMS
		strcpy(temp, "wais_proxy");
		Define_VMSLogical(temp, (char *)&buffer[11]);
#else
		strcpy(temp, "wais_proxy=");
		StrAllocCopy(wais_proxy_putenv_cmd, temp);
		StrAllocCat(wais_proxy_putenv_cmd, (char *)&buffer[11]);
		putenv(wais_proxy_putenv_cmd);
#endif /* VMS */
	    }
	}
	break;

	case 'X':
	if (!strncasecomp(buffer, "XLOADIMAGE_COMMAND:", 19)) {
	    StrAllocCopy(XLoadImageCommand, (char *)&buffer[19]);
	}
	break;

	default:
	break;

	}  /* end of Huge switch */
    } /* end of while */
    fclose(fp);

    /*
     *	If any DOWNLOADER: commands have always_enabled set (:TRUE),
     *	make override_no_download TRUE, so that other restriction
     *	settings will not block presentation of a download menu
     *	with those always_enabled options still available. - FM
     */
    if (downloaders != NULL) {
	int count;
	lynx_html_item_type *cur_download;

	for (count = 0, cur_download = downloaders;
	     cur_download != NULL;
	     cur_download = cur_download->next, count++) {
	    if (cur_download->always_enabled) {
		override_no_download = TRUE;
		break;
	    }
	}
    }
}
