#include <HTUtils.h>
#include <HTTP.h>
#include <HTParse.h>
#include <HTAccess.h>
#include <HTList.h>
#include <HTFile.h>
#include <UCMap.h>
#include <UCDefs.h>
#include <HTInit.h>
#include <LYCurses.h>
#include <LYStyle.h>
#include <HTML.h>
#include <LYUtils.h>
#include <LYGlobalDefs.h>
#include <LYSignal.h>
#include <LYGetFile.h>
#include <LYStrings.h>
#include <LYClean.h>
#include <LYCharSets.h>
#include <LYCharUtils.h>
#include <LYReadCFG.h>
#include <LYrcFile.h>
#include <LYKeymap.h>
#include <LYList.h>
#include <LYJump.h>
#include <LYMainLoop.h>
#include <LYBookmark.h>
#include <LYCookie.h>
#include <LYPrettySrc.h>

#ifdef __DJGPP__
#include <dos.h>
#include <dpmi.h>
#endif /* __DJGPP__ */

#ifdef __EMX__
#include <io.h>
#endif

#ifndef VMS
#ifdef SYSLOG_REQUESTED_URLS
#include <syslog.h>
#endif /* SYSLOG_REQUESTED_URLS */
#endif /* !VMS */

#ifdef LOCALE
#undef gettext		/* Solaris locale.h prototypes gettext() */
#include <locale.h>
#ifndef HAVE_GETTEXT
#define gettext(s) s
#endif
#endif /* LOCALE */

#include <LYexit.h>
#include <LYLeaks.h>

#ifdef FNAMES_8_3
#define COOKIE_FILE "cookies"
#else
#define COOKIE_FILE ".lynx_cookies"
#endif /* FNAMES_8_3 */

/* ahhhhhhhhhh!! Global variables :-< */
#ifdef SOCKS
PUBLIC BOOLEAN socks_flag=TRUE;
#endif /* SOCKS */

#ifdef IGNORE_CTRL_C
PUBLIC BOOLEAN sigint = FALSE;
#endif /* IGNORE_CTRL_C */

#ifdef __DJGPP__
char init_ctrl_break[1];
#endif /* __DJGPP__ */

#ifdef VMS
PUBLIC char *mail_adrs = NULL;	/* the mask for a VMS mail transport */
	       /* create FIXED 512 binaries */
PUBLIC BOOLEAN UseFixedRecords = USE_FIXED_RECORDS;
#endif /* VMS */

#ifndef VMS
PRIVATE char *lynx_version_putenv_command = NULL;
PUBLIC char *list_format = NULL;	/* LONG_LIST formatting mask */
#ifdef SYSLOG_REQUESTED_URLS
PUBLIC char *syslog_txt = NULL;		/* syslog arb text for session */
#endif /* SYSLOG_REQUESTED_URLS */
#endif /* !VMS */

#ifdef VMS
PUBLIC char *LYCSwingPath = NULL;
#endif /* VMS */

#ifdef DIRED_SUPPORT
PUBLIC BOOLEAN lynx_edit_mode = FALSE;
PUBLIC BOOLEAN no_dired_support = FALSE;
PUBLIC int dir_list_style = MIXED_STYLE;
PUBLIC HTList *tagged = NULL;
#ifdef OK_OVERRIDE
PUBLIC BOOLEAN prev_lynx_edit_mode = FALSE;
#endif /* OK_OVERRIDE */
#ifdef OK_PERMIT
#ifdef NO_CHANGE_EXECUTE_PERMS
PUBLIC BOOLEAN no_change_exec_perms = TRUE;
#else
PUBLIC BOOLEAN no_change_exec_perms = FALSE;
#endif /* NO_CHANGE_EXECUTE_PERMS */
#endif /* OK_PERMIT */
#endif /* DIRED_SUPPORT */

	   /* Number of docs cached in memory */
PUBLIC int HTCacheSize = DEFAULT_CACHE_SIZE;
#if defined(VMS) && defined(VAXC) && !defined(__DECC)
	   /* Don't dump doc cache unless this size is exceeded */
PUBLIC int HTVirtualMemorySize = DEFAULT_VIRTUAL_MEMORY_SIZE;
#endif /* VMS && VAXC && !_DECC */

#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
#ifndef NEVER_ALLOW_REMOTE_EXEC
PUBLIC BOOLEAN local_exec = LOCAL_EXECUTION_LINKS_ALWAYS_ON;
#else
PUBLIC BOOLEAN local_exec = FALSE;
#endif /* NEVER_ALLOW_REMOTE_EXEC */
PUBLIC BOOLEAN local_exec_on_local_files =
	       LOCAL_EXECUTION_LINKS_ON_BUT_NOT_REMOTE;
#endif /* EXEC_LINKS || EXEC_SCRIPTS */

#if defined(LYNXCGI_LINKS) && !defined(VMS)  /* WebSter Mods -jkt */
PUBLIC char *LYCgiDocumentRoot = NULL; /* DOCUMENT_ROOT in the lynxcgi env */
#endif /* LYNXCGI_LINKS */

#ifdef REVERSE_CLEAR_SCREEN_PROBLEM
PUBLIC BOOLEAN enable_scrollback=TRUE;
#else
PUBLIC BOOLEAN enable_scrollback=FALSE;
#endif /* REVERSE_CLEAR_SCREEN_PROBLEM */

PUBLIC char *empty_string = "\0";
PUBLIC int display_lines;  /* number of lines in display */
PUBLIC int www_search_result= -1;
			       /* linked list of printers */
PUBLIC lynx_printer_item_type *printers = NULL;
			    /* linked list of download options */
PUBLIC lynx_html_item_type *downloaders = NULL;
			    /* linked list of upload options */
#ifdef USE_EXTERNALS
PUBLIC lynx_html_item_type *externals = NULL;
			    /* linked list of external options */
#endif
PUBLIC lynx_html_item_type *uploaders = NULL;
PUBLIC int port_syntax = 1;
PUBLIC int LYShowColor = SHOW_COLOR_UNKNOWN; /* to show or not to show */
PUBLIC int LYChosenShowColor = SHOW_COLOR_UNKNOWN; /* whether to show and save */
PUBLIC int LYrcShowColor = SHOW_COLOR_UNKNOWN;	/* ... as last read or written */
#if !defined(NO_OPTION_FORMS) && !defined(NO_OPTION_MENU)
PUBLIC BOOLEAN LYUseFormsOptions = TRUE; /* use forms-based options menu */
#endif
PUBLIC BOOLEAN LYShowCursor = SHOW_CURSOR; /* to show or not to show */
PUBLIC BOOLEAN verbose_img = VERBOSE_IMAGES;  /* show filenames or not */
PUBLIC BOOLEAN LYUseDefShoCur = TRUE;	/* Command line -show_cursor toggle */
PUBLIC BOOLEAN LYforce_no_cache = FALSE;
PUBLIC BOOLEAN LYoverride_no_cache = FALSE;/*override no-cache b/c history etc*/
PUBLIC BOOLEAN LYinternal_flag = FALSE; /* override no-cache b/c internal link*/
PUBLIC BOOLEAN LYresubmit_posts = ALWAYS_RESUBMIT_POSTS;
PUBLIC BOOLEAN LYshow_kb_rate = TRUE;
PUBLIC BOOLEAN LYUserSpecifiedURL = TRUE;/* always TRUE  the first time */
PUBLIC BOOLEAN LYJumpFileURL = FALSE;	 /* always FALSE the first time */
PUBLIC BOOLEAN jump_buffer = JUMPBUFFER; /* TRUE if offering default shortcut */
PUBLIC BOOLEAN goto_buffer = GOTOBUFFER; /* TRUE if offering default goto URL */
PUBLIC BOOLEAN recent_sizechange = FALSE;/* the window size changed recently? */
PUBLIC int user_mode = NOVICE_MODE;
PUBLIC BOOLEAN dump_output_immediately = FALSE;
PUBLIC BOOLEAN is_www_index = FALSE;
PUBLIC BOOLEAN lynx_mode = NORMAL_LYNX_MODE;
PUBLIC BOOLEAN bold_headers = FALSE;
PUBLIC BOOLEAN bold_H1 = FALSE;
PUBLIC BOOLEAN bold_name_anchors = FALSE;
PUBLIC BOOLEAN use_underscore = SUBSTITUTE_UNDERSCORES;
PUBLIC BOOLEAN nolist = FALSE;
PUBLIC BOOLEAN historical_comments = FALSE;
PUBLIC BOOLEAN minimal_comments = FALSE;
PUBLIC BOOLEAN soft_dquotes = FALSE;
PUBLIC BOOLEAN LYRestricted = FALSE;
PUBLIC BOOLEAN LYValidate = FALSE;
PUBLIC BOOLEAN LYPermitURL = FALSE;
PUBLIC BOOLEAN child_lynx = FALSE;
PUBLIC BOOLEAN error_logging = MAIL_SYSTEM_ERROR_LOGGING;
PUBLIC BOOLEAN check_mail = CHECKMAIL;
PUBLIC BOOLEAN vi_keys = VI_KEYS_ALWAYS_ON;
PUBLIC BOOLEAN emacs_keys = EMACS_KEYS_ALWAYS_ON;
PUBLIC int keypad_mode = DEFAULT_KEYPAD_MODE;
PUBLIC BOOLEAN case_sensitive = CASE_SENSITIVE_ALWAYS_ON;
PUBLIC BOOLEAN telnet_ok = TRUE;
#ifndef DISABLE_NEWS
PUBLIC BOOLEAN news_ok = TRUE;
#endif
PUBLIC BOOLEAN rlogin_ok = TRUE;
PUBLIC BOOLEAN ftp_ok = TRUE;
PUBLIC BOOLEAN system_editor = FALSE;
#ifdef USE_EXTERNALS
PUBLIC BOOLEAN no_externals = FALSE;
#endif
PUBLIC BOOLEAN no_inside_telnet = FALSE;
PUBLIC BOOLEAN no_outside_telnet = FALSE;
PUBLIC BOOLEAN no_telnet_port = FALSE;
#ifndef DISABLE_NEWS
PUBLIC BOOLEAN no_inside_news = FALSE;
PUBLIC BOOLEAN no_outside_news = FALSE;
#endif
PUBLIC BOOLEAN no_inside_ftp = FALSE;
PUBLIC BOOLEAN no_outside_ftp = FALSE;
PUBLIC BOOLEAN no_inside_rlogin = FALSE;
PUBLIC BOOLEAN no_outside_rlogin = FALSE;
PUBLIC BOOLEAN no_suspend = FALSE;
PUBLIC BOOLEAN no_editor = FALSE;
PUBLIC BOOLEAN no_shell = FALSE;
PUBLIC BOOLEAN no_bookmark = FALSE;
PUBLIC BOOLEAN no_multibook = FALSE;
PUBLIC BOOLEAN no_bookmark_exec = FALSE;
PUBLIC BOOLEAN no_option_save = FALSE;
PUBLIC BOOLEAN no_print = FALSE;
PUBLIC BOOLEAN no_download = FALSE;
PUBLIC BOOLEAN no_disk_save = FALSE;
PUBLIC BOOLEAN no_exec = FALSE;
PUBLIC BOOLEAN no_lynxcgi = FALSE;
PUBLIC BOOLEAN exec_frozen = FALSE;
PUBLIC BOOLEAN no_goto = FALSE;
PUBLIC BOOLEAN no_goto_cso = FALSE;
PUBLIC BOOLEAN no_goto_file = FALSE;
PUBLIC BOOLEAN no_goto_finger = FALSE;
PUBLIC BOOLEAN no_goto_ftp = FALSE;
PUBLIC BOOLEAN no_goto_gopher = FALSE;
PUBLIC BOOLEAN no_goto_http = FALSE;
PUBLIC BOOLEAN no_goto_https = FALSE;
PUBLIC BOOLEAN no_goto_lynxcgi = FALSE;
PUBLIC BOOLEAN no_goto_lynxexec = FALSE;
PUBLIC BOOLEAN no_goto_lynxprog = FALSE;
PUBLIC BOOLEAN no_goto_mailto = FALSE;
#ifndef DISABLE_NEWS
PUBLIC BOOLEAN no_goto_news = FALSE;
PUBLIC BOOLEAN no_goto_nntp = FALSE;
#endif
PUBLIC BOOLEAN no_goto_rlogin = FALSE;
#ifndef DISABLE_NEWS
PUBLIC BOOLEAN no_goto_snews = FALSE;
#endif
PUBLIC BOOLEAN no_goto_telnet = FALSE;
PUBLIC BOOLEAN no_goto_tn3270 = FALSE;
PUBLIC BOOLEAN no_goto_wais = FALSE;
PUBLIC BOOLEAN no_jump = FALSE;
PUBLIC BOOLEAN no_file_url = FALSE;
#ifndef DISABLE_NEWS
PUBLIC BOOLEAN no_newspost = FALSE;
#endif
PUBLIC BOOLEAN no_mail = FALSE;
PUBLIC BOOLEAN no_dotfiles = NO_DOT_FILES;
PUBLIC BOOLEAN no_useragent = FALSE;
PUBLIC BOOLEAN no_statusline = FALSE;
PUBLIC BOOLEAN no_filereferer = FALSE;
PUBLIC BOOLEAN local_host_only = FALSE;
PUBLIC BOOLEAN override_no_download = FALSE;
PUBLIC BOOLEAN show_dotfiles = FALSE; /* From rcfile if no_dotfiles is false */
PUBLIC BOOLEAN LYforce_HTML_mode = FALSE;
PUBLIC char *editor = NULL;	/* the name of the current editor */
PUBLIC char *jumpfile = NULL;	/* the name of the default jumps file */
PUBLIC char *jumpprompt = NULL; /* the default jumps prompt */
PUBLIC char *bookmark_page = NULL; /* the name of the default bookmark page */
PUBLIC char *BookmarkPage = NULL;  /* the name of the current bookmark page */
PUBLIC char *LynxHome = NULL;	/* the default Home HREF. */
PUBLIC char *homepage = NULL;  /* home page or main screen */
PUBLIC char *original_dir = NULL; /* the original directory */
PUBLIC char *startfile = NULL;	/* the first file */
PUBLIC char *helpfile = NULL;	/* the main help file */
PUBLIC char *helpfilepath = NULL;   /* the path to the help file set */
PUBLIC char *lynxjumpfile = NULL;   /* the current jump file URL */
PUBLIC char *lynxlistfile = NULL;   /* the current list file URL */
PUBLIC char *lynxlinksfile = NULL;  /* the current visited links file URL */
PUBLIC char *startrealm = NULL;     /* the startfile realm */
PUBLIC char *indexfile = NULL;	    /* an index file if there is one */
PUBLIC int outgoing_mail_charset = -1;     /* translate mail to this charset */
PUBLIC char *personal_mail_address = NULL; /* the users mail address */
PUBLIC char *x_display = NULL;	    /* display environment variable */
PUBLIC char *personal_type_map = NULL;	   /* .mailcap */
PUBLIC char *global_type_map = NULL;	   /* global mailcap */
PUBLIC char *global_extension_map = NULL;  /* global mime.types */
PUBLIC char *personal_extension_map = NULL;/* .mime.types */
PUBLIC char *language = NULL;	    /* preferred language */
PUBLIC char *pref_charset = NULL;   /* preferred character set */
PUBLIC BOOLEAN LYNewsPosting = NEWS_POSTING; /* News posting supported? */
PUBLIC char *LynxSigFile = NULL;    /* Signature file, in or off home */
PUBLIC char *system_mail = NULL;	  /* The path for sending mail */
PUBLIC char *system_mail_flags = NULL;	  /* Flags for sending mail */
PUBLIC char *lynx_cfg_file = NULL;	  /* location of active lynx.cfg */
PUBLIC char *lynx_temp_space = NULL; /* The prefix for temporary file paths */
PUBLIC char *lynx_save_space = NULL; /* The prefix for save to disk paths */
PUBLIC char *LYHostName = NULL;		/* treat as a local host name */
PUBLIC char *LYLocalDomain = NULL;	/* treat as a local domain tail */
PUBLIC BOOLEAN clickable_images = MAKE_LINKS_FOR_ALL_IMAGES;
PUBLIC BOOLEAN pseudo_inline_alts = MAKE_PSEUDO_ALTS_FOR_INLINES;
PUBLIC BOOLEAN crawl = FALSE;	     /* Do crawl? */
PUBLIC BOOLEAN traversal = FALSE;    /* Do traversals? */
PUBLIC BOOLEAN check_realm = FALSE;  /* Restrict to the starting realm? */
	       /* Links beyond a displayed page with no links? */
PUBLIC BOOLEAN more_links = FALSE;
PUBLIC int     ccount = 0; /* Starting number for lnk#.dat files in crawls */
PUBLIC BOOLEAN LYCancelledFetch = FALSE; /* TRUE if cancelled binary fetch */
	       /* Include mime headers with source dump */
PUBLIC BOOLEAN keep_mime_headers = FALSE;
PUBLIC BOOLEAN no_url_redirection = FALSE; /* Don't follow URL redirections */
PUBLIC char *form_post_data = NULL;  /* User data for post form */
PUBLIC char *form_get_data = NULL;   /* User data for get form */
PUBLIC char *http_error_file = NULL; /* Place HTTP status code in this file */
	     /* Id:Password for protected documents */
PUBLIC char *authentication_info[2] = {NULL, NULL};
	     /* Id:Password for protected proxy servers */
PUBLIC char *proxyauth_info[2] = {NULL, NULL};
PUBLIC BOOLEAN HEAD_request = FALSE;
PUBLIC BOOLEAN scan_for_buried_news_references = TRUE;
PUBLIC BOOLEAN LYRawMode;
PUBLIC BOOLEAN LYDefaultRawMode;
PUBLIC BOOLEAN LYUseDefaultRawMode = TRUE;
PUBLIC char *UCAssume_MIMEcharset = NULL;
PUBLIC BOOLEAN UCSaveBookmarksInUnicode = FALSE;
PUBLIC BOOLEAN UCForce8bitTOUPPER = FALSE; /* override locale for case-conversion? */
PUBLIC int LYlines = 24;
PUBLIC int LYcols = 80;
PUBLIC int dump_output_width = 0;
PUBLIC linkstruct links[MAXLINKS];
PUBLIC histstruct history[MAXHIST];
PUBLIC int nlinks = 0;		/* number of links in memory */
PUBLIC int nhist = 0;		/* number of history entries */
PUBLIC int more = FALSE;	/* is there more text to display? */
PUBLIC int InfoSecs;	/* Seconds to sleep() for Information messages */
PUBLIC int MessageSecs; /* Seconds to sleep() for important Messages   */
PUBLIC int AlertSecs;	/* Seconds to sleep() for HTAlert() messages   */
PUBLIC BOOLEAN bookmark_start = FALSE;
PUBLIC char *LYUserAgent = NULL;	/* Lynx User-Agent header	   */
PUBLIC char *LYUserAgentDefault = NULL; /* Lynx default User-Agent header  */
PUBLIC BOOLEAN LYUseMouse = FALSE;
PUBLIC BOOLEAN LYNoRefererHeader=FALSE; /* Never send Referer header?	   */
PUBLIC BOOLEAN LYNoRefererForThis=FALSE;/* No Referer header for this URL? */
PUBLIC BOOLEAN LYNoFromHeader = TRUE;	/* Never send From header?	   */
PUBLIC BOOLEAN LYListNewsNumbers = LIST_NEWS_NUMBERS;
PUBLIC BOOLEAN LYListNewsDates = LIST_NEWS_DATES;
PUBLIC BOOLEAN LYisConfiguredForX = FALSE;
PUBLIC char *URLDomainPrefixes = NULL;
PUBLIC char *URLDomainSuffixes = NULL;
PUBLIC BOOLEAN startfile_ok = FALSE;
PUBLIC BOOLEAN LYSelectPopups = USE_SELECT_POPUPS;
PUBLIC BOOLEAN LYUseDefSelPop = TRUE;	/* Command line -popup toggle */
PUBLIC BOOLEAN LYMultiBookmarks = MULTI_BOOKMARK_SUPPORT;
PUBLIC BOOLEAN LYMBMBlocked = BLOCK_MULTI_BOOKMARKS;
PUBLIC BOOLEAN LYMBMAdvanced = TRUE;
PUBLIC int LYStatusLine = -1;		 /* Line for statusline() if > -1 */
PUBLIC BOOLEAN LYCollapseBRs = COLLAPSE_BR_TAGS;  /* Collapse serial BRs? */
PUBLIC BOOLEAN LYSetCookies = SET_COOKIES; /* Process Set-Cookie headers? */
PUBLIC BOOLEAN LYAcceptAllCookies = ACCEPT_ALL_COOKIES; /* take all cookies? */
PUBLIC char *LYCookieAcceptDomains = NULL; /* domains to accept all cookies */
PUBLIC char *LYCookieRejectDomains = NULL; /* domains to reject all cookies */
PUBLIC char *LYCookieStrictCheckDomains = NULL; /* check strictly  */
PUBLIC char *LYCookieLooseCheckDomains = NULL;  /* check loosely   */
PUBLIC char *LYCookieQueryCheckDomains = NULL;  /* check w/a query */
PUBLIC char *LYCookieSAcceptDomains = NULL; /* domains to accept all cookies */
PUBLIC char *LYCookieSRejectDomains = NULL; /* domains to reject all cookies */
PUBLIC char *LYCookieSStrictCheckDomains = NULL; /* check strictly  */
PUBLIC char *LYCookieSLooseCheckDomains = NULL;  /* check loosely   */
PUBLIC char *LYCookieSQueryCheckDomains = NULL;  /* check w/a query */
#ifdef EXP_PERSISTENT_COOKIES
BOOLEAN persistent_cookies = TRUE;
PUBLIC char *LYCookieFile = NULL;	   /* default cookie file */
#endif /* EXP_PERSISTENT_COOKIES */
PUBLIC char *XLoadImageCommand = NULL;	/* Default image viewer for X */
PUBLIC BOOLEAN LYNoISMAPifUSEMAP = FALSE; /* Omit ISMAP link if MAP present? */
PUBLIC int LYHiddenLinks = HIDDENLINKS_SEPARATE; /* Show hidden links? */

PUBLIC BOOL Old_DTD = NO;
PUBLIC FILE *LYTraceLogFP = NULL;		/* Pointer for TRACE log  */
PUBLIC char *LYTraceLogPath = NULL;		/* Path for TRACE log	   */
PUBLIC BOOLEAN LYUseTraceLog = USE_TRACE_LOG;	/* Use a TRACE log?	   */
PUBLIC BOOLEAN LYSeekFragMAPinCur = TRUE;
PUBLIC BOOLEAN LYSeekFragAREAinCur = TRUE;

PUBLIC BOOLEAN LYStripDotDotURLs = TRUE;	/* Try to fix ../ in some URLs? */
PUBLIC BOOLEAN LYForceSSLCookiesSecure = FALSE;
PUBLIC BOOLEAN LYNoCc = FALSE;
PUBLIC BOOLEAN LYPreparsedSource = FALSE;	/* Show source as preparsed?	 */
PUBLIC BOOLEAN LYPrependBaseToSource = TRUE;
PUBLIC BOOLEAN LYPrependCharsetToSource = TRUE;
PUBLIC BOOLEAN LYQuitDefaultYes = QUIT_DEFAULT_YES;

#ifdef DISP_PARTIAL
PUBLIC BOOLEAN display_partial_flag = TRUE; /* Display document during download */
PUBLIC BOOLEAN debug_display_partial = FALSE; /* Show with MessageSecs delay */
PUBLIC int partial_threshold = -1;  /* # of lines to be d/l'ed until we repaint */
#endif

/* These are declared in cutil.h for current freeWAIS libraries. - FM */
#ifdef DECLARE_WAIS_LOGFILES
PUBLIC char *log_file_name = NULL; /* for WAIS log file name	in libWWW */
PUBLIC FILE *logfile = NULL;	   /* for WAIS log file output	in libWWW */
#endif /* DECLARE_WAIS_LOGFILES */

#ifndef DISABLE_NEWS
extern int HTNewsChunkSize; /* Number of news articles per chunk (HTNews.c) */
extern int HTNewsMaxChunk;  /* Max news articles before chunking (HTNews.c) */
#endif

PRIVATE BOOLEAN stack_dump = FALSE;
PRIVATE char *terminal = NULL;
PRIVATE char *pgm;
PRIVATE BOOLEAN number_links = FALSE;
PRIVATE BOOLEAN LYPrependBase = FALSE;
PRIVATE HTList *LYStdinArgs = NULL;

#ifndef EXTENDED_OPTION_LOGIC
/* if set then '--' will be recognized as the end of options */
#define EXTENDED_OPTION_LOGIC 1
#endif

#if EXTENDED_OPTION_LOGIC
PRIVATE BOOLEAN no_options_further=FALSE; /* set to TRUE after '--' argument */
#endif


PRIVATE void parse_arg PARAMS((char **arg, int *i));
PRIVATE void print_help_and_exit PARAMS((int exit_status));

#ifndef VMS
PUBLIC BOOLEAN LYNoCore = NO_FORCED_CORE_DUMP;
PRIVATE void FatalProblem PARAMS((int sig));
#endif /* !VMS */

#if defined(USE_HASH)
    char *lynx_lss_file=NULL;
#endif

#ifdef __DJGPP__
PRIVATE int LY_set_ctrl_break(int setting)
{
    (void)signal(SIGINT, (setting ? SIG_DFL : SIG_IGN));
    setcbrk(setting);
}

PRIVATE int LY_get_ctrl_break(void)
{
    __dpmi_regs regs;
    regs.h.ah = 0x33;
    regs.h.al = 0x00;
    __dpmi_int (0x21, &regs);
    return ((int) regs.h.dl);
}

PRIVATE void reset_break(void)
{
    LY_set_ctrl_break(init_ctrl_break[0]);
}
#endif /* __DJGPP__ */

#ifdef LY_FIND_LEAKS
PRIVATE void free_lynx_globals NOARGS
{
    int i;

#ifndef VMS
    FREE(list_format);
#ifdef SYSLOG_REQUESTED_URLS
    FREE(syslog_txt);
#endif /* SYSLOG_REQUESTED_URLS */
#ifdef LYNXCGI_LINKS  /* WebSter Mods -jkt */
    FREE(LYCgiDocumentRoot);
#endif /* LYNXCGI_LINKS */
    free_lynx_cfg();
#endif /* !VMS */

#ifdef VMS
    Define_VMSLogical("LYNX_VERSION", "");
    FREE(mail_adrs);
    FREE(LYCSwingPath);
#endif /* VMS */

    FREE(LynxHome);
    FREE(homepage);
    FREE(original_dir);
    FREE(startfile);
    FREE(helpfile);
    FREE(helpfilepath);
    FREE(jumpprompt);
#ifdef JUMPFILE
    FREE(jumpfile);
#endif /* JUMPFILE */
    FREE(indexfile);
    FREE(x_display);
    FREE(global_type_map);
    FREE(personal_type_map);
    FREE(global_extension_map);
    FREE(personal_extension_map);
    FREE(language);
    FREE(pref_charset);
    FREE(LynxSigFile);
    FREE(system_mail);
    FREE(system_mail_flags);
#ifdef EXP_PERSISTENT_COOKIES
    FREE(LYCookieFile);
#endif
    FREE(LYCookieAcceptDomains);
    FREE(LYCookieRejectDomains);
    FREE(LYCookieLooseCheckDomains);
    FREE(LYCookieStrictCheckDomains);
    FREE(LYCookieQueryCheckDomains);
    FREE(LYUserAgent);
    FREE(LYUserAgentDefault);
    FREE(LYHostName);
    FREE(LYLocalDomain);
    FREE(lynx_save_space);
    FREE(bookmark_page);
    FREE(BookmarkPage);
    for (i = 0; i <= MBM_V_MAXFILES; i++) {
	FREE(MBM_A_subbookmark[i]);
	FREE(MBM_A_subdescript[i]);
    }
    FREE(editor);
    FREE(authentication_info[0]);
    FREE(authentication_info[1]);
    FREE(proxyauth_info[0]);
    FREE(proxyauth_info[1]);
    FREE(lynxjumpfile);
    FREE(startrealm);
    FREE(personal_mail_address);
    FREE(URLDomainPrefixes);
    FREE(URLDomainSuffixes);
    FREE(XLoadImageCommand);
#ifndef VMS
    FREE(lynx_version_putenv_command);
#endif
    FREE(lynx_temp_space);
    FREE(LYTraceLogPath);
    FREE(lynx_cfg_file);
#if defined(USE_HASH)
    FREE(lynx_lss_file);
#endif
    FREE(UCAssume_MIMEcharset);
    {
	char *p = LYlist_temp_url();
	if (p && *p) {
	    *p = '\0';
	    FREE(p);
	}
    }
    for (i = 0; i < nlinks; i++) {
	FREE(links[i].lname);
    }
    nlinks = 0;

    return;
}
#endif /* LY_FIND_LEAKS */

/*
 *  This function frees the LYStdinArgs list. - FM
 */
PRIVATE void LYStdinArgs_free NOARGS
{
    char *argument;
    HTList *cur = LYStdinArgs;

    if (cur == NULL)
	return;

    while (NULL != (argument = (char *)HTList_nextObject(cur))) {
	FREE(argument);
    }
    HTList_delete(LYStdinArgs);
    LYStdinArgs = NULL;
    return;
}

PUBLIC void exit_immediately ARGS1(
	int,		code)
{
#ifndef NOSIGHUP
    (void) signal(SIGHUP, SIG_DFL);
#endif /* NOSIGHUP */
    (void) signal(SIGTERM, SIG_DFL);
#ifndef VMS
    (void) signal(SIGINT, SIG_DFL);
#endif /* !VMS */
#ifdef SIGTSTP
    if (no_suspend)
	(void) signal(SIGTSTP, SIG_DFL);
#endif /* SIGTSTP */
    exit(code);
}

#ifdef  EBCDIC
      char un_IBM1047[ 256 ] = "";
unsigned char IBM1047[ 256 ] = /* ATOE OEMVS311 */
{
0x00,0x01,0x02,0x03,0x37,0x2d,0x2e,0x2f,0x16,0x05,0x15,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x3c,0x3d,0x32,0x26,0x18,0x19,0x3f,0x27,0x1c,0x1d,0x1e,0x1f,
0x40,0x5a,0x7f,0x7b,0x5b,0x6c,0x50,0x7d,0x4d,0x5d,0x5c,0x4e,0x6b,0x60,0x4b,0x61,
0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0x7a,0x5e,0x4c,0x7e,0x6e,0x6f,
0x7c,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,
0xd7,0xd8,0xd9,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xad,0xe0,0xbd,0x5f,0x6d,
0x79,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96,
0x97,0x98,0x99,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xc0,0x4f,0xd0,0xa1,0x07,
0x20,0x21,0x22,0x23,0x24,0x25,0x06,0x17,0x28,0x29,0x2a,0x2b,0x2c,0x09,0x0a,0x1b,
0x30,0x31,0x1a,0x33,0x34,0x35,0x36,0x08,0x38,0x39,0x3a,0x3b,0x04,0x14,0x3e,0xff,
0x41,0xaa,0x4a,0xb1,0x9f,0xb2,0x6a,0xb5,0xbb,0xb4,0x9a,0x8a,0xb0,0xca,0xaf,0xbc,
0x90,0x8f,0xea,0xfa,0xbe,0xa0,0xb6,0xb3,0x9d,0xda,0x9b,0x8b,0xb7,0xb8,0xb9,0xab,
0x64,0x65,0x62,0x66,0x63,0x67,0x9e,0x68,0x74,0x71,0x72,0x73,0x78,0x75,0x76,0x77,
0xac,0x69,0xed,0xee,0xeb,0xef,0xec,0xbf,0x80,0xfd,0xfe,0xfb,0xfc,0xba,0xae,0x59,
0x44,0x45,0x42,0x46,0x43,0x47,0x9c,0x48,0x54,0x51,0x52,0x53,0x58,0x55,0x56,0x57,
0x8c,0x49,0xcd,0xce,0xcb,0xcf,0xcc,0xe1,0x70,0xdd,0xde,0xdb,0xdc,0x8d,0x8e,0xdf
} ;

static void FixCharacters(void)
{
    int c;
    int work1[256],
	work2[256];

    for (c = 0; c < 256; c++) {
	un_IBM1047[IBM1047[c]] = c;
	work1[c] = keymap[c+1];
	work2[c] = key_override[c+1];
    }
    for (c = 0; c < 256; c++) {
	keymap      [IBM1047[c]+1] = work1[c];
	key_override[IBM1047[c]+1] = work2[c];
    }
}
#endif /* EBCDIC */

/* these are used for matching commandline options. */
PRIVATE int argcmp ARGS2(
	char*,		str,
	char*,		what)
{
    if (str[0] == '-' && str[1] == '-' ) ++str;
    return strcmp(str,what);
}

PRIVATE int argncmp ARGS2(
	char*,		str,
	char*,		what)
{
    if (str[0] == '-' && str[1] == '-' ) ++str;
    return strncmp(str, what, strlen(what));
}

/*
 * Wow!  Someone wants to start up Lynx.
 */
PUBLIC int main ARGS2(
	int,		argc,
	char **,	argv)
{
    int  i;		/* indexing variable */
    int status = 0;	/* exit status */
    char *temp = NULL;
    char *cp;
    FILE *fp;
    char filename[LY_MAXPATH];
    BOOL LYGetStdinArgs = FALSE;

#ifdef    NOT_ASCII
    FixCharacters();
#endif /* NOT_ASCII */

#ifdef _WINDOWS
    WSADATA WSAData;
    {
	int err;
	WORD wVerReq;

	wVerReq = MAKEWORD(1,1);

	err = WSAStartup(wVerReq, &WSAData);
	if (err != 0)
	{
	    printf(gettext("No Winsock found, sorry."));
	    sleep(5);
	    return;
	}
    }
#endif /* _WINDOWS */

#ifdef __DJGPP__
    if (LY_get_ctrl_break() == 0) {
	LY_set_ctrl_break(TRUE);
	init_ctrl_break[0] = 0;
    } else {
	init_ctrl_break[0] = 1;
    }
    atexit(reset_break);
    dbug_init();
    sock_init();
#endif

    /*
     * To prevent corrupting binary data on DOS, MS-WINDOWS or OS/2
     * we open files and stdout in BINARY mode by default.
     * Where necessary we should open and (close!) TEXT mode.
     * (use LYNewTxtFile/LYAppendToTxtFile to open text files for writing)
     */
    SetDefaultMode(O_BINARY);
    SetOutputMode(O_BINARY);

#ifdef DOSPATH
    if (getenv("TERM")==NULL) putenv("TERM=vt100");
#endif

    LYShowColor = (SHOW_COLOR ? SHOW_COLOR_ON : SHOW_COLOR_OFF);
    /*
     *	Set up the argument list.
     */
    pgm = argv[0];
    if ((cp = strrchr(pgm, '/')) != NULL) {
	pgm = cp + 1;
    }

    /*
     *	Act on -help NOW, so we only output the help and exit. - FM
     */
    for (i = 1; i < argc; i++) {
	if (argncmp(argv[i], "-help") == 0) {
	    parse_arg(&argv[i], &i);
	}
    }

#ifdef LY_FIND_LEAKS
    /*
     *	Register the final function to be executed when being exited.
     *	Will display memory leaks if LY_FIND_LEAKS is defined.
     */
    atexit(LYLeaks);
    /*
     *	Register the function which will free our allocated globals.
     */
    atexit(free_lynx_globals);
#endif /* LY_FIND_LEAKS */


#ifdef LOCALE
    /*
     *	LOCALE support for international characters.
     */
    setlocale(LC_ALL, "");
#endif /* LOCALE */
    /* Set the text message domain.  */
#ifdef HAVE_LIBINTL_H
#ifndef __DJGPP__
    bindtextdomain ("lynx", LOCALEDIR);
#endif /* !__DJGPP__ */
    textdomain ("lynx");
#endif /* HAVE_LIBINTL_H */

    /*
     *	Initialize our startup and global variables.
     */
#ifdef ULTRIX
    /*
     *	Need this for Ultrix.
     */
    terminal = getenv("TERM");
    if ((terminal == NULL) || !strncasecomp(terminal, "xterm", 5))
	terminal = "vt100";
#endif /* ULTRIX */
    /*
     *	Zero the links and history struct arrays.
     */
    memset((void *)links, 0, sizeof(linkstruct)*MAXLINKS);
    memset((void *)history, 0, sizeof(histstruct)*MAXHIST);
    /*
     *	Zero the MultiBookmark arrays.
     */
    memset((void *)MBM_A_subbookmark, 0, sizeof(char)*(MBM_V_MAXFILES+1));
    memset((void *)MBM_A_subdescript, 0, sizeof(char)*(MBM_V_MAXFILES+1));
#ifndef VMS
#ifdef SYSLOG_REQUESTED_URLS
    openlog("lynx", LOG_PID, LOG_LOCAL5);
#endif /* SYSLOG_REQUESTED_URLS */
    StrAllocCopy(list_format, LIST_FORMAT);
#endif /* !VMS */
    InfoSecs	= (int)INFOSECS;
    MessageSecs = (int)MESSAGESECS;
    AlertSecs	= (int)ALERTSECS;
    StrAllocCopy(helpfile, HELPFILE);
    StrAllocCopy(startfile, STARTFILE);
    LYTrimStartfile(startfile);
    StrAllocCopy(indexfile, DEFAULT_INDEX_FILE);
    StrAllocCopy(global_type_map, GLOBAL_MAILCAP);
    StrAllocCopy(personal_type_map, PERSONAL_MAILCAP);
    StrAllocCopy(global_extension_map, GLOBAL_EXTENSION_MAP);
    StrAllocCopy(personal_extension_map, PERSONAL_EXTENSION_MAP);
    StrAllocCopy(language, PREFERRED_LANGUAGE);
    StrAllocCopy(pref_charset, PREFERRED_CHARSET);
    StrAllocCopy(system_mail, SYSTEM_MAIL);
    StrAllocCopy(system_mail_flags, SYSTEM_MAIL_FLAGS);
    StrAllocCopy(LYUserAgent, LYNX_NAME);
    StrAllocCat(LYUserAgent, "/");
    StrAllocCat(LYUserAgent, LYNX_VERSION);
    if (HTLibraryVersion) {
	StrAllocCat(LYUserAgent, " libwww-FM/");
	StrAllocCat(LYUserAgent, HTLibraryVersion);
    }
    StrAllocCopy(LYUserAgentDefault, LYUserAgent);
#ifdef VMS
    Define_VMSLogical("LYNX_VERSION", LYNX_VERSION);
#else
    StrAllocCopy(lynx_version_putenv_command, "LYNX_VERSION=");
    StrAllocCat(lynx_version_putenv_command, LYNX_VERSION);
    (void) putenv(lynx_version_putenv_command);
    /* Note: you must not free the data passed to 'putenv()' until you give it
     * a new value for that variable.
     */
#endif /* VMS */

    if ((cp = getenv("LYNX_TEMP_SPACE")) != NULL)
	StrAllocCopy(lynx_temp_space, cp);
#if defined (UNIX)
    else if ((cp = getenv("TMPDIR")) != NULL)
	StrAllocCopy(lynx_temp_space, cp);
#endif
#if defined (DOSPATH) || defined (__EMX__)
    else if ((cp = getenv("TEMP")) != NULL)
	StrAllocCopy(lynx_temp_space, HTDOS_name(cp));
    else if ((cp = getenv("TMP")) != NULL)
	StrAllocCopy(lynx_temp_space, HTDOS_name(cp));
#endif
    else
#ifdef TEMP_SPACE
	StrAllocCopy(lynx_temp_space, TEMP_SPACE);
#else
    {
	printf(gettext("You MUST define a valid TMP or TEMP area!\n"));
	exit(-1);
    }
#endif

    if ((cp = strchr(lynx_temp_space, '~'))) {
	*(cp++) = '\0';
	StrAllocCopy(temp, lynx_temp_space);
	LYTrimPathSep(temp);
	StrAllocCat(temp, wwwName(Home_Dir()));
	StrAllocCat(temp, cp);
	StrAllocCopy(lynx_temp_space, temp);
	FREE(temp);
    }
    if ((cp = strstr(lynx_temp_space, "$USER")) != NULL) {
	char *cp1;

	if ((cp1 = (char *)getenv("USER")) != NULL) {
	    *cp = '\0';
	    StrAllocCopy(temp, lynx_temp_space);
	    *cp = '$';
	    StrAllocCat(temp, cp1);
	    cp += 5;
	    StrAllocCat(temp, cp);
	    StrAllocCopy(lynx_temp_space, temp);
	    FREE(temp);
	}
    }
#ifdef VMS
    LYLowerCase(lynx_temp_space);
    if (strchr(lynx_temp_space, '/') != NULL) {
	if (strlen(lynx_temp_space) == 1) {
	    StrAllocCopy(lynx_temp_space, "sys$scratch:");
	} else {
	    LYAddPathSep(&lynx_temp_space);
	    StrAllocCopy(temp, HTVMS_name("", lynx_temp_space));
	    StrAllocCopy(lynx_temp_space, temp);
	    FREE(temp);
	}
    }
    if (strchr(lynx_temp_space, ':') == NULL &&
	strchr(lynx_temp_space, ']') == NULL) {
	StrAllocCat(lynx_temp_space, ":");
    }
#else
    LYAddPathSep(&lynx_temp_space);
#endif /* VMS */
#ifdef VMS
    StrAllocCopy(mail_adrs, MAIL_ADRS);
#ifdef CSWING_PATH
    StrAllocCopy(LYCSwingPath, CSWING_PATH);
#endif /* CSWING_PATH */
#endif /* VMS */
#ifdef LYNX_HOST_NAME
    StrAllocCopy(LYHostName, LYNX_HOST_NAME);
#else
    StrAllocCopy(LYHostName, HTHostName());
#endif /* LYNX_HOST_NAME */
    StrAllocCopy(LYLocalDomain, LOCAL_DOMAIN);
    StrAllocCopy(URLDomainPrefixes, URL_DOMAIN_PREFIXES);
    StrAllocCopy(URLDomainSuffixes, URL_DOMAIN_SUFFIXES);
    StrAllocCopy(XLoadImageCommand, XLOADIMAGE_COMMAND);

    /*
     *	Disable news posting if the compilation-based
     *	LYNewsPosting value is FALSE.  This may be changed
     *	further down via lynx.cfg or the -restriction
     *	command line switch. - FM
     */
#ifndef DISABLE_NEWS
    no_newspost = (LYNewsPosting == FALSE);
#endif

    /*
     *	Set up trace, the anonymous account defaults,
     *	validate restrictions, and/or the nosocks flag,
     *	if requested, and an alternate configuration
     *	file, if specified, NOW.  Also, if we only want
     *	the help menu, output that and exit. - FM
     */
    for (i = 1; i < argc; i++) {
	if (argncmp(argv[i], "-trace") == 0) {
	    WWW_TraceFlag = TRUE;
	} else if (argncmp(argv[i], "-tlog") == 0) {
	    if (LYUseTraceLog) {
		LYUseTraceLog = FALSE;
	    } else {
		LYUseTraceLog = TRUE;
	    }
	} else if (argncmp(argv[i], "-anonymous") == 0) {
	    if (!LYValidate)
		parse_restrictions("default");
	    LYRestricted = TRUE;
	} else if (argcmp(argv[i], "-validate") == 0) {
	    /*
	     *	Follow only http URLs.
	     */
	    LYValidate = TRUE;
#ifdef SOCKS
	} else if (argncmp(argv[i], "-nosocks") == 0) {
	    socks_flag = FALSE;
#endif /* SOCKS */
	} else if (argncmp(argv[i], "-cfg") == 0) {
	    if ((cp=strchr(argv[i],'=')) != NULL)
		StrAllocCopy(lynx_cfg_file, cp+1);
	    else {
		StrAllocCopy(lynx_cfg_file, argv[i+1]);
		i++;
	    }

#if defined(USE_HASH)
	} else if (argncmp(argv[i], "-lss") == 0) {
	    if ((cp=strchr(argv[i],'=')) != NULL)
		StrAllocCopy(lynx_lss_file, cp+1);
	    else {
		StrAllocCopy(lynx_lss_file, argv[i+1]);
		i++;
	    }
	    CTRACE(tfp, "LYMain found -lss flag, lss file is %s\n",
		    lynx_lss_file ? lynx_lss_file : "<NONE>");
#endif
	}
    }

    /*
     *	If we have a lone "-" switch for getting arguments from stdin,
     *	get them NOW, and act on the relevant ones, saving the others
     *	into an HTList for handling after the other initializations.
     *	The primary purpose of this feature is to allow for the
     *	potentially very long command line that can be associated with
     *	post or get data.  The original implementation required that
     *	the lone "-" be the only command line argument, but that
     *	precluded its use when the lynx command is aliased with other
     *	arguments.  When interactive, the stdin input is terminated by
     *	by Control-D on Unix or Control-Z on VMS, and each argument
     *	is terminated by a RETURN.  When the argument is -get_data or
     *	-post_data, the data are terminate by a "___" string, alone
     *	on the line (also terminated by RETURN). - FM
     */
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-") == 0) {
	    LYGetStdinArgs = TRUE;
	}
    }
    if (LYGetStdinArgs == TRUE) {
	char *buf = NULL;

	while (LYSafeGets(&buf, stdin) != 0) {
	    int j;

	    for (j = strlen(buf) - 1; j > 0 &&
		(buf[j] == CR || buf[j] == LF); j--) {
		buf[j] = '\0';
	    }

	    if (argncmp(buf, "-trace") == 0) {
		WWW_TraceFlag = TRUE;
	    } else if (argncmp(buf, "-tlog") == 0) {
		if (LYUseTraceLog) {
		    LYUseTraceLog = FALSE;
		} else {
		    LYUseTraceLog = TRUE;
		}
	    } else if (argncmp(buf, "-anonymous") == 0) {
		if (!LYValidate && !LYRestricted)
		    parse_restrictions("default");
		LYRestricted = TRUE;
	    } else if (argcmp(buf, "-validate") == 0) {
		/*
		 *  Follow only http URLs.
		 */
		LYValidate = TRUE;
#ifdef SOCKS
	    } else if (argncmp(buf, "-nosocks") == 0) {
		socks_flag = FALSE;
#endif /* SOCKS */
	    } else if (argncmp(buf, "-cfg") == 0) {
		if ((cp = strchr(buf,'=')) != NULL) {
		    StrAllocCopy(lynx_cfg_file, cp+1);
		} else {
		    cp = LYSkipNonBlanks(buf);
		    cp = LYSkipBlanks(cp);
		    if (*cp)
			StrAllocCopy(lynx_cfg_file, cp);
		}
#if defined(USE_HASH)
	    } else if (argncmp(buf, "-lss") == 0) {
		if ((cp = strchr(buf,'=')) != NULL) {
		    StrAllocCopy(lynx_lss_file, cp+1);
		} else {
		    cp = LYSkipNonBlanks(buf);
		    cp = LYSkipBlanks(cp);
		    if (*cp)
			StrAllocCopy(lynx_lss_file, cp);
		}
		CTRACE(tfp, "LYMain found -lss flag, lss file is %s\n",
			lynx_lss_file ? lynx_lss_file : "<NONE>");
#endif
	    } else if (argcmp(buf, "-get_data") == 0) {
		/*
		 *  User data for GET form.
		 */
		char **get_data;

		/*
		 *  On Unix, conflicts with curses when interactive
		 *  so let's force a dump.  - CL
		 *
		 *  On VMS, mods have been made in LYCurses.c to deal
		 *  with potential conflicts, so don't force the dump
		 *  here. - FM
		 */
#ifndef VMS
		dump_output_immediately = TRUE;
		LYcols = 80;
#endif /* VMS */

		StrAllocCopy(form_get_data, "?");   /* Prime the pump */
		get_data = &form_get_data;

		/*
		 *  Build GET data for later.  Stop reading when we see
		 *  a line with "---" as its first three characters.
		 */
		while (LYSafeGets(&buf, stdin) != 0 &&
		       strncmp(buf, "---", 3) != 0) {
		    int j2;

		    /*
		     *	Strip line terminators.
		     */
		    for (j2 = strlen(buf) - 1; j2 >= 0 &&
			 (buf[j2] == CR || buf[j2] == LF); j2--) {
			buf[j2] = '\0';
		    }
		    StrAllocCat(*get_data, buf);
		}
	    } else if (argcmp(buf, "-post_data") == 0) {
		/*
		 *  User data for POST form.
		 */
		char **post_data;

		/*
		 *  On Unix, conflicts with curses when interactive
		 *  so let's force a dump.  - CL
		 *
		 *  On VMS, mods have been made in LYCurses.c to deal
		 *  with potential conflicts, so don't force a dump
		 *  here. - FM
		 */
#ifndef VMS
		dump_output_immediately = TRUE;
		LYcols = 80;
#endif /* VMS */

		post_data = &form_post_data;

		/*
		 *  Build post data for later.	Stop reading when we see
		 *  a line with "---" as its first three characters.
		 */
		while (LYSafeGets(&buf, stdin) != 0 &&
		       strncmp(buf, "---", 3) != 0) {
		    int j2;

		     /*
		      *  Strip line terminators.
		      */
		    for (j2 = strlen(buf) - 1; j2 >= 0 &&
			 (buf[j2] == CR || buf[j2] == LF); j2--) {
			buf[j2] = '\0';
		    }
		    StrAllocCat(*post_data, buf);
		}
	    } else if (buf[0] != '\0') {
		char *argument = NULL;

		if (LYStdinArgs == NULL) {
		    LYStdinArgs = HTList_new();
#ifdef LY_FIND_LEAKS
		    atexit(LYStdinArgs_free);
#endif
		}
		StrAllocCopy(argument, buf);
		HTList_appendObject(LYStdinArgs, argument);
	    }
	}
	FREE(buf);
    }

#ifdef SOCKS
    if (socks_flag)
	SOCKSinit(argv[0]);
#endif /* SOCKS */

    /*
     *	If we had -validate set all of the restrictions
     *	and disallow a TRACE log NOW. - FM
     */
    if (LYValidate == TRUE) {
	parse_restrictions("all");
	LYUseTraceLog = FALSE;
    }

    /*
     *	If we didn't get and act on a -validate or -anonymous
     *	switch, but can verify that this is the anonymous account,
     *	set the default restrictions for that account and disallow
     *	a TRACE log NOW. - FM
     */
    if (!LYValidate && !LYRestricted &&
	strlen((char *)ANONYMOUS_USER) > 0 &&
#if defined (VMS) || defined (NOUSERS)
	!strcasecomp(((char *)getenv("USER")==NULL ? " " : getenv("USER")),
		     ANONYMOUS_USER))
#else
#if HAVE_CUSERID
	STREQ((char *)cuserid((char *) NULL), ANONYMOUS_USER))
#else
	STREQ(((char *)getlogin()==NULL ? " " : getlogin()), ANONYMOUS_USER))
#endif /* HAVE_CUSERID */
#endif /* VMS */
    {
	parse_restrictions("default");
	LYRestricted = TRUE;
	LYUseTraceLog = FALSE;
    }

    /*
     *	Set up the TRACE log path, and logging if appropriate. - FM
     */
    LYAddPathToHome(LYTraceLogPath = malloc(LY_MAXPATH), LY_MAXPATH, "Lynx.trace");
    LYOpenTraceLog();

    /*
     *	Set up the default jump file stuff. - FM
     */
    StrAllocCopy(jumpprompt, JUMP_PROMPT);
#ifdef JUMPFILE
    StrAllocCopy(jumpfile, JUMPFILE);
    {
	temp = (char *)malloc(strlen(jumpfile) + 10);
	if (!temp) {
	    outofmem(__FILE__, "main");
	} else {
	    sprintf(temp, "JUMPFILE:%s", jumpfile);
	    if (!LYJumpInit(temp)) {
		CTRACE(tfp, "Failed to register %s\n", temp);
	    }
	    FREE(temp);
	}
    }
#endif /* JUMPFILE */

    /*
     *	If no alternate configuration file was specified on
     *	the command line, see if it's in the environment.
     */
    if (!lynx_cfg_file) {
	if (((cp=getenv("LYNX_CFG")) != NULL) ||
	    (cp=getenv("lynx_cfg")) != NULL)
	    StrAllocCopy(lynx_cfg_file, cp);
    }

    /*
     *	If we still don't have a configuration file,
     *	use the userdefs.h definition.
     */
    if (!lynx_cfg_file)
	StrAllocCopy(lynx_cfg_file, LYNX_CFG_FILE);

    /*
     *	Convert a '~' in the configuration file path to $HOME.
     */
#ifndef _WINDOWS /* avoid the whole ~ thing for now */
   /* I think this should only be performed if lynx_cfg_file starts with ~/ */
   if ((lynx_cfg_file[0] == '~') && LYIsPathSep(lynx_cfg_file[1]))
     {
#ifdef VMS
	StrAllocCopy(temp, HTVMS_wwwName((char *)Home_Dir()));
#else
	StrAllocCopy(temp, Home_Dir());
#endif /* VMS */
	StrAllocCat(temp, lynx_cfg_file + 1);
	StrAllocCopy(lynx_cfg_file, temp);
	FREE(temp);
     }
#endif

    /*
     *	If the configuration file is not available,
     *	inform the user and exit.
     */
    if ((fp = fopen(lynx_cfg_file, "r")) == NULL) {
	fprintf(stderr, gettext("\nConfiguration file %s is not available.\n\n"),
			lynx_cfg_file);
	exit(-1);
    }
    fclose(fp);

#if defined(USE_KEYMAPS) && defined(USE_SLANG)
    if (-1 == lynx_initialize_keymaps ())
	exit (-1);
#endif

    /*
     * Make sure we have the character sets declared.
     *	This will initialize the CHARTRANS handling. - KW
     */
    if (!LYCharSetsDeclared()) {
	fprintf(stderr, gettext("\nLynx character sets not declared.\n\n"));
	exit(-1);
    }
    /*
     *  (**) in Lynx, UCLYhndl_HTFile_for_unspec and UCLYhndl_for_unrec may be
     *  valid or not, but current_char_set and UCLYhndl_for_unspec SHOULD
     *  ALWAYS be a valid charset.  Initialized here and may be changed later
     *  from lynx.cfg/command_line/options_menu. - LP  (**)
     */
    /*
     *	Set up the compilation default character set. - FM
     */
    current_char_set = safeUCGetLYhndl_byMIME(CHARACTER_SET);
    /*
     *	Set up HTTP default for unlabeled charset (iso-8859-1).
     */
    UCLYhndl_for_unspec = LATIN1;
    StrAllocCopy(UCAssume_MIMEcharset,
			LYCharSet_UC[UCLYhndl_for_unspec].MIMEname);

    /*
     *	Make sure we have the edit map declared. - FM
     */
    if (!LYEditmapDeclared()) {
	fprintf(stderr, gettext("\nLynx edit map not declared.\n\n"));
	exit(-1);
    }

#if defined(USE_HASH)
    /*
     *	If no alternate lynx-style file was specified on
     *	the command line, see if it's in the environment.
     */
    if (!lynx_lss_file) {
	if (((cp=getenv("LYNX_LSS")) != NULL) ||
	    (cp=getenv("lynx_lss")) != NULL)
	    StrAllocCopy(lynx_lss_file, cp);
    }

    /*
     *	If we still don't have a lynx-style file,
     *	use the userdefs.h definition.
     */
    if (!lynx_lss_file)
	StrAllocCopy(lynx_lss_file, LYNX_LSS_FILE);

    /*
     *	Convert a '~' in the lynx-style file path to $HOME.
     */
    if ((cp = strchr(lynx_lss_file, '~'))) {
	*(cp++) = '\0';
	StrAllocCopy(temp, lynx_lss_file);
	LYTrimPathSep(temp);
#ifdef VMS
	StrAllocCat(temp, HTVMS_wwwName((char *)Home_Dir()));
#else
	StrAllocCat(temp, Home_Dir());
#endif /* VMS */
	StrAllocCat(temp, cp);
	StrAllocCopy(lynx_lss_file, temp);
	FREE(temp);
    }
    /*
     *	If the lynx-style file is not available,
     *	inform the user and exit.
     */
    if ((fp = fopen(lynx_lss_file, "r")) == NULL) {
	fprintf(stderr, gettext("\nLynx file %s is not available.\n\n"),
			lynx_lss_file);
    }
    else
    {
	fclose(fp);
	style_readFromFile(lynx_lss_file);
    }
#endif /* USE_HASH */

#if USE_COLOR_TABLE
    /*
     *	Set up default foreground and background colors.
     */
    lynx_setup_colors();
#endif /* USE_COLOR_TABLE */

    /*
     *  Set the original directory, used for default download
     */
    if (!strcmp(Current_Dir(filename), ".")) {
	if ((cp = getenv("PWD")) != 0)
	    StrAllocCopy(original_dir, cp);
    } else {
	StrAllocCopy(original_dir, filename);
    }

    /*
     *	Set the compilation default signature file. - FM
     */
    strcpy(filename, LYNX_SIG_FILE);
    if (LYPathOffHomeOK(filename, sizeof(filename))) {
	StrAllocCopy(LynxSigFile, filename);
	LYAddPathToHome(filename, sizeof(filename), LynxSigFile);
	StrAllocCopy(LynxSigFile, filename);
	CTRACE(tfp, "LYNX_SIG_FILE set to '%s'\n", LynxSigFile);
    } else {
	CTRACE(tfp, "LYNX_SIG_FILE '%s' is bad. Ignoring.\n", LYNX_SIG_FILE);
    }

#ifdef USE_PSRC
    /*this is required for checking the tagspecs when parsing cfg file by
       LYReadCFG.c:parse_html_src_spec -HV */
    HTSwitchDTD(TRUE);
#endif
    /*
     *	Process the configuration file.
     */
    read_cfg(lynx_cfg_file, "main program", 1, (FILE *)0);

    /*
     *	Process the RC file.
     */
    read_rc();

    /*
     * Get WWW_HOME environment variable if it exists.
     */
    if ((cp = getenv("WWW_HOME")) != NULL) {
	StrAllocCopy(startfile, cp);
	LYTrimStartfile(startfile);
    }

    /*
     * Set the LynxHome URL.  If it's a file URL and the
     * host is defaulted, force in "//localhost", and if
     * it's not an absolute URL, make it one. - FM
     */
    StrAllocCopy(LynxHome, startfile);
    LYFillLocalFileURL((char **)&LynxHome, "file://localhost");
    LYEnsureAbsoluteURL((char **)&LynxHome, "LynxHome", FALSE);

    /*
     *  Process any command line arguments not already handled. - FM
     */
    for (i = 1; i < argc; i++) {
	parse_arg(&argv[i], &i);
    }

    /*
     *  Process any stdin-derived arguments for a lone "-"  which we've
     *  loaded into LYStdinArgs. - FM
     */
    if (LYStdinArgs != NULL) {
	char *my_args[2];
	HTList *cur = LYStdinArgs;

	my_args[1] = NULL;
	while (NULL != (my_args[0] = (char *)HTList_nextObject(cur))) {
	     parse_arg(my_args, (int *)0);
	}
	LYStdinArgs_free();
    }

    /*
     *  Initialize other things based on the configuration read.
     */

#ifdef USE_PSRC
    if ( (!Old_DTD) != TRUE ) /* skip if they are already initialized -HV */
#endif
    HTSwitchDTD(!Old_DTD);

    /*
     * Set up the proper character set with the desired
     * startup raw 8-bit or CJK mode handling.  - FM
     */
    HTMLUseCharacterSet(current_char_set);

#ifdef EXP_PERSISTENT_COOKIES
    /*
     * Sod it, this looks like a reasonable place to load the
     * cookies file, probably.  - RP
     */
    if (persistent_cookies) {
	if(LYCookieFile == NULL) {
	   LYAddPathToHome(LYCookieFile = malloc(LY_MAXPATH), LY_MAXPATH, COOKIE_FILE);
	} else {
	    if ((cp = strchr(LYCookieFile, '~'))) {
		temp = NULL;
		*(cp++) = '\0';
		StrAllocCopy(temp, cp);
		LYTrimPathSep(temp);
		StrAllocCopy(LYCookieFile, wwwName(Home_Dir()));
		StrAllocCat(LYCookieFile, temp);
		FREE(temp);
	    }
	}
	LYLoadCookies(LYCookieFile);
    }
#endif

    /*
     * Set up our help and about file base paths. - FM
     */
    StrAllocCopy(helpfilepath, helpfile);
    if ((cp = LYPathLeaf(helpfilepath)) != helpfilepath)
	*cp = '\0';
    LYAddHtmlSep(&helpfilepath);

    /*
     *	Check for a save space path in the environment.
     *	If one was set in the configuration file, that
     *	one will be overridden. - FM
     */
    if ((cp=getenv("LYNX_SAVE_SPACE")) != NULL)
	StrAllocCopy(lynx_save_space, cp);

    /*
     *	We have a save space path, make sure it's valid. - FM
     */
    if (lynx_save_space && *lynx_save_space == '\0') {
	FREE(lynx_save_space);
    }
    if (lynx_save_space) {
	if ((cp = strchr(lynx_save_space, '~')) != NULL) {
	    *(cp++) = '\0';
	    StrAllocCopy(temp, lynx_save_space);
	    LYTrimPathSep(temp);
	    StrAllocCat(temp, wwwName(Home_Dir()));
	    StrAllocCat(temp, cp);
	    StrAllocCopy(lynx_save_space, temp);
	    FREE(temp);
	}
#ifdef VMS
	LYLowerCase(lynx_save_space);
	if (strchr(lynx_save_space, '/') != NULL) {
	    if (strlen(lynx_save_space) == 1) {
		StrAllocCopy(lynx_save_space, "sys$login:");
	    } else {
		LYAddPathSep(&lynx_save_space);
		StrAllocCopy(temp, HTVMS_name("", lynx_save_space));
		StrAllocCopy(lynx_save_space, temp);
		FREE(temp);
	    }
	}
	if (strchr(lynx_save_space, ':') == NULL &&
	    strchr(lynx_save_space, ']') == NULL) {
	    StrAllocCat(lynx_save_space, ":");
	}
#else
	LYAddPathSep(&lynx_save_space);
#endif /* VMS */
    }

    /*
     *	Set up the file extension and mime type maps from
     *	src/HTInit.c and the global and personal mime.types
     *	and mailcap files.  These will override any SUFFIX
     *	or VIEWER maps in userdefs.h or the configuration
     *	file, if they overlap.
     */
    HTFormatInit();
    HTFileInit();

#ifndef VMS
#ifdef SYSLOG_REQUESTED_URLS
    if (syslog_txt) {
	syslog(LOG_INFO, "Session start:%s", syslog_txt);
    } else {
	syslog(LOG_INFO, "Session start");
    }
#endif /* SYSLOG_REQUESTED_URLS */
#endif /* !VMS */

#ifdef USE_SLANG
    if (LYShowColor >= SHOW_COLOR_ON &&
	!(Lynx_Color_Flags & SL_LYNX_USE_COLOR)) {
	Lynx_Color_Flags |= SL_LYNX_USE_COLOR;
    } else if ((Lynx_Color_Flags & SL_LYNX_USE_COLOR) ||
	       getenv("COLORTERM") != NULL) {
	if (LYShowColor != SHOW_COLOR_NEVER &&
	    LYShowColor != SHOW_COLOR_ALWAYS) {
	    LYShowColor = SHOW_COLOR_ON;
	}
    }
#endif /* USE_SLANG */

    if (LYPreparsedSource) {
	HTPreparsedFormatInit();
    }

#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
#ifdef NEVER_ALLOW_REMOTE_EXEC
    if (local_exec) {
	local_exec = FALSE;
	local_exec_on_local_files = TRUE;
    }
#endif /* NEVER_ALLOW_REMOTE_EXEC */
#endif /* EXEC_LINKS || EXEC_SCRIPTS */

    if (emacs_keys)
	set_emacs_keys();

    if (vi_keys)
	set_vi_keys();

    if (number_links && keypad_mode == NUMBERS_AS_ARROWS)
	keypad_mode = LINKS_ARE_NUMBERED;
    if (keypad_mode == NUMBERS_AS_ARROWS)
	set_numbers_as_arrows();

    /*
     *	Check the -popup command line toggle. - FM
     */
    if (LYUseDefSelPop == FALSE) {
	if (LYSelectPopups == TRUE)
	    LYSelectPopups = FALSE;
	else
	    LYSelectPopups = TRUE;
    }

    /*
     *	Check the -show_cursor command line toggle. - FM
     */
    if (LYUseDefShoCur == FALSE) {
	if (LYShowCursor == TRUE)
	    LYShowCursor = FALSE;
	else
	    LYShowCursor = TRUE;
    }

    /*
     *	Check the -base command line switch with -source. - FM
     */
    if (LYPrependBase && HTOutputFormat == HTAtom_for("www/download")) {
	LYPrependBaseToSource = TRUE;
    }

    /*
     *	Disable multiple bookmark support if not interactive,
     *	so it doesn't crash on curses functions, or if the
     *	support was blocked via userdefs.h and/or lynx.cfg,
     *	or via command line restrictions. - FM
     */
    if (no_multibook)
	LYMBMBlocked = TRUE;
    if (dump_output_immediately || LYMBMBlocked || no_multibook) {
	LYMultiBookmarks = FALSE;
	LYMBMBlocked = TRUE;
	no_multibook = TRUE;
    }

#ifdef SOURCE_CACHE
    /*
     * Disable source caching if not interactive.
     */
    if (dump_output_immediately)
	LYCacheSource = SOURCE_CACHE_NONE;
#endif
#ifdef DISP_PARTIAL
    /*
     * Disable partial mode if not interactive.
     */
    if (dump_output_immediately)
	display_partial_flag = FALSE;
#endif

#ifdef VMS
    set_vms_keys();
#endif /* VMS */

    /* trap interrupts */
    if (!dump_output_immediately)
#ifndef NOSIGHUP
	(void) signal(SIGHUP, cleanup_sig);
#endif /* NOSIGHUP */
    (void) signal(SIGTERM, cleanup_sig);
#ifdef SIGWINCH
    (void) signal(SIGWINCH, size_change);
#endif /* SIGWINCH */
#ifndef VMS
    if (!TRACE && !dump_output_immediately && !stack_dump) {
	(void) signal(SIGINT, cleanup_sig);
#ifndef __linux__
#ifndef DOSPATH
	(void) signal(SIGBUS, FatalProblem);
#endif /* DOSPATH */
#endif /* !__linux__ */
	(void) signal(SIGSEGV, FatalProblem);
	(void) signal(SIGILL, FatalProblem);
	/*
	 *  Since we're doing lots of TCP, just ignore SIGPIPE altogether.
	 *
	 *  HTTCP.c should deal with a broken pipe for servers.
	 *  Rick Mallet's check after c = GetChar() in LYStrings.c should
	 *   deal with a disconnected terminal.
	 *  So the runaway CPU time problem on Unix should not occur any
	 *   more.
	 */
#ifndef DOSPATH
	(void) signal(SIGPIPE, SIG_IGN);
#endif /* DOSPATH */
    }
#endif /* !VMS */

#ifdef SIGTSTP
    /*
     *	Block Control-Z suspending if requested. - FM
     */
    if (no_suspend)
	(void) signal(SIGTSTP,SIG_IGN);
#endif /* SIGTSTP */

    /*
     *	Check for a valid HEAD request. - FM
     */
    if (HEAD_request && LYCanDoHEAD(startfile) != TRUE) {
	fprintf(stderr,
 "The '-head' switch is for http HEAD requests and cannot be used for\n'%s'.\n",
		startfile);
	exit_immediately(-1);
    }

    /*
     *	Check for a valid MIME headers request. - FM
     */
    if (keep_mime_headers && LYCanDoHEAD(startfile) != TRUE) {
	fprintf(stderr,
 "The '-mime_header' switch is for http URLs and cannot be used for\n'%s'.\n",
		startfile);
	exit_immediately(-1);
    }

    /*
     *	Check for a valid traversal request. - FM
     */
    if (traversal && strncmp(startfile, "http", 4)) {
	fprintf(stderr,
 "The '-traversal' switch is for http URLs and cannot be used for\n'%s'.\n",
		startfile);
	exit_immediately(-1);
    }

    /*
     *  Finish setting up for an INTERACTIVE session.
     *  Done here so that URL guessing in LYEnsureAbsoluteURL() can be
     *  interruptible (terminal is in raw mode, select() works).  -BL
     */
#ifdef USE_PSRC
    if (!dump_output_immediately)
	HTMLSRC_init_caches(); /* do it before terminal is initialized*/
#endif

    if (!dump_output_immediately) {
	setup(terminal);
    }
    /*
     *	If startfile is a file URL and the host is defaulted,
     *	force in "//localhost", and if it's not an absolute URL,
     *	make it one. - FM
     */
    LYFillLocalFileURL((char **)&startfile, "file://localhost");
    LYEnsureAbsoluteURL((char **)&startfile, "STARTFILE", FALSE);

    /*
     *	If homepage was specified and is a file URL with the
     *	host defaulted, force in "//localhost", and if it's
     *	not an absolute URL, make it one. - FM
     */
    if (homepage) {
	LYFillLocalFileURL((char **)&homepage, "file://localhost");
	LYEnsureAbsoluteURL((char **)&homepage, "HOMEPAGE", FALSE);
    }

    /*
     *	If we don't have a homepage specified,
     *	set it to startfile.  Otherwise, reset
     *	LynxHome. - FM
     */
    if (!(homepage && *homepage)) {
	StrAllocCopy(homepage, startfile);
    } else {
	StrAllocCopy(LynxHome, homepage);
    }

    /*
     *	Set up the inside/outside domain restriction flags. - FM
     */
    if (inlocaldomain()) {
#if !defined(HAVE_UTMP) || defined(VMS) /* not selective */
	telnet_ok = !no_inside_telnet && !no_outside_telnet && telnet_ok;
#ifndef DISABLE_NEWS
	news_ok = !no_inside_news && !no_outside_news && news_ok;
#endif
	ftp_ok = !no_inside_ftp && !no_outside_ftp && ftp_ok;
	rlogin_ok = !no_inside_rlogin && !no_outside_rlogin && rlogin_ok;
#else
	CTRACE(tfp,"LYMain.c: User in Local domain\n");
	telnet_ok = !no_inside_telnet && telnet_ok;
#ifndef DISABLE_NEWS
	news_ok = !no_inside_news && news_ok;
#endif
	ftp_ok = !no_inside_ftp && ftp_ok;
	rlogin_ok = !no_inside_rlogin && rlogin_ok;
#endif /* !HAVE_UTMP || VMS */
    } else {
	CTRACE(tfp,"LYMain.c: User in REMOTE domain\n");
	telnet_ok = !no_outside_telnet && telnet_ok;
#ifndef DISABLE_NEWS
	news_ok = !no_outside_news && news_ok;
#endif
	ftp_ok = !no_outside_ftp && ftp_ok;
	rlogin_ok = !no_outside_rlogin && rlogin_ok;
    }

    /*
     *	Make sure our bookmark default strings
     *	are all allocated and synchronized. - FM
     */
    if (!bookmark_page || *bookmark_page == '\0') {
	StrAllocCopy(bookmark_page, "lynx_bookmarks");
	StrAllocCat(bookmark_page, HTML_SUFFIX);
	StrAllocCopy(BookmarkPage, bookmark_page);
	StrAllocCopy(MBM_A_subbookmark[0], bookmark_page);
	StrAllocCopy(MBM_A_subdescript[0], "Default");
    }
    if (!BookmarkPage || *BookmarkPage == '\0') {
	StrAllocCopy(BookmarkPage, bookmark_page);
	StrAllocCopy(MBM_A_subbookmark[0], bookmark_page);
	StrAllocCopy(MBM_A_subdescript[0], MULTIBOOKMARKS_DEFAULT);
    }

    /*
     *	Here's where we do all the work.
     */
    if (dump_output_immediately) {
	/*
	 *  Finish setting up and start a
	 *  NON-INTERACTIVE session. - FM
	 */
	if (crawl && !number_links) {
	    keypad_mode = NUMBERS_AS_ARROWS;
	} else if (!nolist) {
	    if (keypad_mode == NUMBERS_AS_ARROWS) {
		keypad_mode = LINKS_ARE_NUMBERED;
	    }
	}
	if (x_display != NULL && *x_display != '\0') {
	    LYisConfiguredForX = TRUE;
	}
	if (dump_output_width > 0) {
	    LYcols = dump_output_width;
	}
	status = mainloop();
	if (!nolist &&
	    (keypad_mode == LINKS_ARE_NUMBERED ||
	     keypad_mode == LINKS_AND_FORM_FIELDS_ARE_NUMBERED))
	    printlist(stdout,FALSE);
#ifdef EXP_PERSISTENT_COOKIES
	/*
	 *  We want to save cookies picked up when in immediate dump
	 *  mode.  Instead of calling cleanup() here, let's only call
	 *  this one. - BJP
	 */
	if (persistent_cookies)
	    LYStoreCookies(LYCookieFile);
#endif /* EXP_PERSISTENT_COOKIES */
	cleanup_files();
	exit_immediately(status);
    } else {
	/*
	 *  Start an INTERACTIVE session. - FM
	 */
	if (x_display != NULL && *x_display != '\0') {
	    LYisConfiguredForX = TRUE;
	}
#ifdef USE_COLOR_STYLE
	cache_tag_styles();
#endif

	ena_csi((LYlowest_eightbit[current_char_set] > 155));
	LYOpenCloset();
	status = mainloop();
	LYCloseCloset();
	cleanup();
	exit(status);
    }

    return(status);	/* though redundant, for compiler-warnings */
}

/*
 *  Called by HTAccessInit to register any protocols supported by lynx.
 *  Protocols added by lynx:
 *    LYNXKEYMAP, lynxcgi, LYNXIMGMAP, LYNXCOOKIE
 */
#ifdef GLOBALREF_IS_MACRO
extern GLOBALREF (HTProtocol, LYLynxKeymap);
extern GLOBALREF (HTProtocol, LYLynxCGI);
extern GLOBALREF (HTProtocol, LYLynxIMGmap);
extern GLOBALREF (HTProtocol, LYLynxCookies);
#else
GLOBALREF  HTProtocol LYLynxKeymap;
GLOBALREF  HTProtocol LYLynxCGI;
GLOBALREF  HTProtocol LYLynxIMGmap;
GLOBALREF  HTProtocol LYLynxCookies;
#endif /* GLOBALREF_IS_MACRO */

PUBLIC void LYRegisterLynxProtocols NOARGS
{
    HTRegisterProtocol(&LYLynxKeymap);
    HTRegisterProtocol(&LYLynxCGI);
    HTRegisterProtocol(&LYLynxIMGmap);
    HTRegisterProtocol(&LYLynxCookies);
}

#ifndef NO_CONFIG_INFO
/*
 *  Some stuff to reload lynx.cfg without restarting new lynx session,
 *  also load options menu items and command-line options
 *  to make things consistent.  Not implemented yet.
 *  Warning: experimental, more main() reorganization required.
 *
 *  Called by user of interactive session by LYNXCFG://reload/ link.
 */
PUBLIC void reload_read_cfg NOARGS
{
    if (LYRestricted) return;  /* for sure */

    /* save .lynxrc file in case we change something from Options Menu */
    if (!save_rc()) return;    /* can not write the very own file :( */

    {
	/* set few safe flags: */
#ifdef PERSISTENT_COOKIES
	BOOLEAN persistent_cookies_flag = persistent_cookies;
	char * LYCookieFile_flag = LYCookieFile;
#endif

	free_lynx_cfg(); /* free downloaders, printers, not always environments */
	/*
	 *  Process the configuration file.
	 */
	read_cfg(lynx_cfg_file, "main program", 1, (FILE *)0);

	/*
	 *  Process the RC file.
	 */
	read_rc();


	/* We are not interested in startfile here */
	/* but other things may be lost: */

	/*
	 *  Process any command line arguments not already handled.
	 */
		/* Not implemented yet here */

	/*
	 *  Process any stdin-derived arguments for a lone "-"  which we've
	 *  loaded into LYStdinArgs.
	 */
		/* Not implemented yet here */

	/*
	 *  Initialize other things based on the configuration read.
	 */
		/* Not implemented yet here,
		 * a major problem: file paths
		 * like lynx_save_space, LYCookieFile etc.
		 */
#ifdef PERSISTENT_COOKIES
	/* restore old settings */
	 if (persistent_cookies != persistent_cookies_flag) {
	     persistent_cookies = persistent_cookies_flag;
	     HTAlert(gettext("persistent cookies state will be changed in next session only."));
	 }
	 if (strcmp(LYCookieFile, LYCookieFile_flag)) {
	     StrAllocCopy(LYCookieFile, LYCookieFile_flag);
	     CTRACE(tfp, "cookies file can be changed in next session only, restored.\n")
	 }
#endif

    }
}
#endif /* !NO_CONFIG_INFO */


/* There are different ways of setting arguments on the command line, and
 * there are different types of arguments.  These include:
 *
 *   -set_some_variable		 ==> some_variable  = TRUE
 *   -toggle_some_variable	 ==> some_variable = !some_variable
 *   -some_variable=value	 ==> some_variable = value
 *
 * Others are complicated and require a function call.
 */

struct parse_args_type;
typedef int (*ParseFunc) PARAMS((char *));

typedef union {
	BOOLEAN * set_value;
	int *     int_value;
	char **   str_value;
	ParseFunc fun_value;
} ParseUnion;

/*
 * Storing the four types of data in separate fields costs about 1K of data.
 * However, this provides usable type-checking.  The initial version of the
 * parse_args_type used 'long' for all types, and dumped core when processing
 * "lynx -help".  (The compiler was unable to detect some minor errors).
 */
#ifdef  PARSE_DEBUG
#define ParseData BOOLEAN *set_value; int *int_value; char **str_value; ParseFunc fun_value
#define PARSE_SET(n,t,v,h) {n,t,    v,  0,  0,  0,    h}
#define PARSE_INT(n,t,v,h) {n,t,    0,  v,  0,  0,    h}
#define PARSE_STR(n,t,v,h) {n,t,    0,  0,  v,  0,    h}
#define PARSE_FUN(n,t,v,h) {n,t,    0,  0,  0,  v,    h}
#else
#define ParseData long value
#define PARSE_SET(n,t,v,h) {n,t,   (long) (v),        h}
#define PARSE_INT(n,t,v,h) {n,t,   (long) (v),        h}
#define PARSE_STR(n,t,v,h) {n,t,   (long) (v),        h}
#define PARSE_FUN(n,t,v,h) {n,t,   (long) (v),        h}
#endif

typedef struct parse_args_type
{
   CONST char *name;
   int type;
#define IGNORE_ARG		0x000
#define TOGGLE_ARG		0x001
#define SET_ARG			0x002
#define UNSET_ARG		0x003
#define FUNCTION_ARG		0x004
#define LYSTRING_ARG		0x005
#define INT_ARG			0x006
#define STRING_ARG		0x007
#define ARG_TYPE_MASK		0x0FF
#define NEED_NEXT_ARG		0x100

#define NEED_INT_ARG		(NEED_NEXT_ARG | INT_ARG)
#define NEED_LYSTRING_ARG	(NEED_NEXT_ARG | LYSTRING_ARG)
#define NEED_STRING_ARG		(NEED_NEXT_ARG | STRING_ARG)
#define NEED_FUNCTION_ARG	(NEED_NEXT_ARG | FUNCTION_ARG)

   /* If the NEED_NEXT_ARG flags is set, and the option was not specified
    * with an '=' character, then use the next argument in the argv list.
    */

   ParseData;
   CONST char *help_string;
}
Parse_Args_Type;

/* -auth, -pauth */
static int parse_authentication ARGS2(
	char *,			next_arg,
	char **,		result)
{
    /*
     *  Authentication information for protected documents.
     */
    char *auth_info = 0;

    if (next_arg != 0) {
	StrAllocCopy(auth_info, next_arg);
	memset(next_arg, ' ', strlen(next_arg));  /* Let's not show too much */
    }

    if (auth_info != 0) {
	char *cp;

	if ((cp = strchr(auth_info, ':')) != 0) {		/* Pw */
	    *cp++ = '\0';	/* Terminate ID */
	    HTUnEscape(cp);
	    StrAllocCopy(result[1], cp);
	}
	if (*auth_info) {					/* Id */
	    HTUnEscape(auth_info);
	    StrAllocCopy(result[0], auth_info);
	}
	FREE(auth_info);
    }
    return 0;
}

/* -anonymous */
static int anonymous_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
   /*
    *  Should already have been set, so we don't
    *  override or replace any additional
    *  restrictions from the command line. - FM
    */
   if (!LYRestricted)
      parse_restrictions("default");
   LYRestricted = TRUE;
   return 0;
}

/* -assume_charset */
static int assume_charset_fun ARGS1(
	char *,			next_arg)
{
    UCLYhndl_for_unspec = safeUCGetLYhndl_byMIME(next_arg);
    StrAllocCopy(UCAssume_MIMEcharset,
		 LYCharSet_UC[UCLYhndl_for_unspec].MIMEname);
/*	   this may be a memory for bogus typo -
    StrAllocCopy(UCAssume_MIMEcharset, next_arg);
    LYLowerCase(UCAssume_MIMEcharset);   */

    return 0;
}

/* -assume_local_charset */
static int assume_local_charset_fun ARGS1(
	char *,			next_arg)
{
    UCLYhndl_HTFile_for_unspec = safeUCGetLYhndl_byMIME(next_arg);
    return 0;
}

/* -assume_unrec_charset */
static int assume_unrec_charset_fun ARGS1(
	char *,			next_arg)
{
    UCLYhndl_for_unrec = safeUCGetLYhndl_byMIME(next_arg);
    return 0;
}

/* -auth */
static int auth_fun ARGS1(
	char *,			next_arg)
{
    parse_authentication(next_arg, authentication_info);
    return 0;
}

/* -base */
static int base_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    /*
     *  Treat -source equivalently to an interactive download with
     *  LYPrefixBaseToSource configured to TRUE, so that a BASE tag is
     *  prepended for text/html content types.  We normally treat the
     *  module-wide global LYPrefixBaseToSource flag as FALSE with
     *  -source, but force it TRUE, later, if LYPrependBase is set
     *  TRUE here. - FM
     */
    LYPrependBase = TRUE;
    if (HTOutputFormat == HTAtom_for("www/dump"))
	HTOutputFormat = HTAtom_for("www/download");

    return 0;
}

#ifdef USE_SLANG
/* -blink */
static int blink_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    Lynx_Color_Flags |= SL_LYNX_USE_BLINK;
    return 0;
}
#endif

/* -cache */
static int cache_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0)
	HTCacheSize = atoi(next_arg);
    /*
     *  Limit size.
     */
    if (HTCacheSize < 2) HTCacheSize = 2;

    return 0;
}

/* -child */
static int child_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    child_lynx = TRUE;
    no_disk_save = TRUE;
    return 0;
}

#ifdef USE_SLANG
/* -color */
static int color_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    Lynx_Color_Flags |= SL_LYNX_USE_COLOR;

    if (LYShowColor != SHOW_COLOR_ALWAYS)
	LYShowColor = SHOW_COLOR_ON;

    return 0;
}
#endif

/* -crawl */
static int crawl_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    crawl = TRUE;
    LYcols = 80;
    return 0;
}

/* -display */
static int display_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0) {
	LYsetXDisplay(next_arg);
	if ((next_arg = LYgetXDisplay()) != 0)
	    StrAllocCopy(x_display, next_arg);
    }

    return 0;
}

/* -dump */
static int dump_output_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    dump_output_immediately = TRUE;
    LYcols = 80;
    return 0;
}

/* -editor */
static int editor_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0)
	StrAllocCopy(editor, next_arg);
    system_editor = TRUE;
    return 0;
}

/* -error_file */
static int error_file_fun ARGS1(
	char *,			next_arg)
{
    /*
     *  Output return (success/failure) code
     *  of an HTTP transaction.
     */
    if (next_arg != 0)
	http_error_file = next_arg;
    return 0;
}

#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
/* -exec */
static int exec_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
#ifndef NEVER_ALLOW_REMOTE_EXEC
    local_exec = TRUE;
#else
    local_exec_on_local_files = TRUE;
#endif /* NEVER_ALLOW_REMOTE_EXEC */
    return 0;
}
#endif

/* -get_data */
static int get_data_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    /*
     *  User data for GET form.
     */
    char **get_data;
    char *buf = NULL;

    /*
     *  On Unix, conflicts with curses when interactive
     *  so let's force a dump.  - CL
     *
     *  On VMS, mods have been made in LYCurses.c to deal with
     *  potential conflicts, so don't force the dump here. - FM
     */
#ifndef VMS
    dump_output_immediately = TRUE;
    LYcols = 80;
#endif /* VMS */

    StrAllocCopy(form_get_data, "?");   /* Prime the pump */
    get_data = &form_get_data;

    /*
     *  Build GET data for later.  Stop reading when we see a line
     *  with "---" as its first three characters.
     */
    while (LYSafeGets(&buf, stdin) != 0 &&
	  strncmp(buf, "---", 3) != 0) {
	int j;

	for (j = strlen(buf) - 1; j >= 0 && /* Strip line terminators */
	    (buf[j] == CR || buf[j] == LF); j--)
	    buf[j] = '\0';

	StrAllocCat(*get_data, buf);
    }

    return 0;
}

/* -help */
static int help_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    print_help_and_exit (0);
    return 0;
}

/* -hiddenlinks */
static int hiddenlinks_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0) {
	if (strncasecomp(next_arg, "merge", 1) == 0)
	    LYHiddenLinks = HIDDENLINKS_MERGE;
	else if (strncasecomp(next_arg, "listonly", 1) == 0)
	    LYHiddenLinks = HIDDENLINKS_SEPARATE;
	else if (strncasecomp(next_arg, "ignore", 1) == 0)
	    LYHiddenLinks = HIDDENLINKS_IGNORE;
	else
	    print_help_and_exit (-1);
    } else {
	LYHiddenLinks = HIDDENLINKS_MERGE;
    }

    return 0;
}

/* -homepage */
static int homepage_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0) {
	StrAllocCopy(homepage, next_arg);
	LYTrimStartfile(homepage);
    }
    return 0;
}

/* -mime_header */
static int mime_header_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    /*
     *  Include mime headers and force source dump.
     */
    keep_mime_headers = TRUE;
    dump_output_immediately = TRUE;
    HTOutputFormat = (LYPrependBase ?
		      HTAtom_for("www/download") : HTAtom_for("www/dump"));
    LYcols = 999;
    return 0;
}

#ifndef DISABLE_NEWS
/* -newschunksize */
static int newschunksize_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0) {
	HTNewsChunkSize = atoi(next_arg);
	/*
	 * If the new HTNewsChunkSize exceeds the maximum,
	 * increase HTNewsMaxChunk to this size. - FM
	 */
	if (HTNewsChunkSize > HTNewsMaxChunk)
	    HTNewsMaxChunk = HTNewsChunkSize;
    }
    return 0;
}

/* -newsmaxchunk */
static int newsmaxchunk_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg) {
	HTNewsMaxChunk = atoi(next_arg);
	/*
	 * If HTNewsChunkSize exceeds the new maximum,
	 * reduce HTNewsChunkSize to this maximum. - FM
	 */
	if (HTNewsChunkSize > HTNewsMaxChunk)
	    HTNewsChunkSize = HTNewsMaxChunk;
    }
    return 0;
}
#endif /* not DISABLE_NEWS */

/* -nobrowse */
static int nobrowse_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
   HTDirAccess = HT_DIR_FORBID;
   return 0;
}

/* -nocolor */
static int nocolor_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    LYShowColor = SHOW_COLOR_NEVER;
#ifdef USE_SLANG
    Lynx_Color_Flags &= ~SL_LYNX_USE_COLOR;
    Lynx_Color_Flags |= SL_LYNX_OVERRIDE_COLOR;
#endif
    return 0;
}

/* -nopause */
static int nopause_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    InfoSecs = 0;
    MessageSecs = 0;
    AlertSecs = 0;
    return 0;
}

/* -pauth */
static int pauth_fun ARGS1(
	char *,			next_arg)
{
    parse_authentication(next_arg, proxyauth_info);
    return 0;
}

/* -post_data */
static int post_data_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    /*
     *  User data for POST form.
     */
    char **post_data;
    char *buf = NULL;

    /*
     * On Unix, conflicts with curses when interactive so let's force a dump.
     * - CL
     *
     * On VMS, mods have been made in LYCurses.c to deal with potential
     * conflicts, so don't force a dump here.  - FM
     */
#ifndef VMS
    dump_output_immediately = TRUE;
    LYcols = 80;
#endif /* VMS */

    post_data = &form_post_data;

    /*
     * Build post data for later.  Stop reading when we see a line with "---"
     * as its first three characters.
     */
    while (LYSafeGets(&buf, stdin) != 0 &&
	  strncmp(buf, "---", 3) != 0) {
	int j;

	for (j = strlen(buf) - 1; j >= 0 && /* Strip line terminators */
	    (buf[j] == CR || buf[j] == LF); j--) {
	    buf[j] = '\0';
	}
	StrAllocCat(*post_data, buf);
    }
    return 0;
}

/* -restrictions */
static int restrictions_fun ARGS1(
	char *,			next_arg)
{
    static CONST char *Usage[] = {
 ""
,"   USAGE: lynx -restrictions=[option][,option][,option]"
,"   List of Options:"
,"   all             restricts all options."
,"   bookmark        disallow changing the location of the bookmark file."
,"   bookmark_exec   disallow execution links via the bookmark file"
#if defined(DIRED_SUPPORT) && defined(OK_PERMIT)
,"   change_exec_perms  disallow changing the eXecute permission on files"
,"                   (but still allow it for directories) when local file"
,"                   management is enabled."
#endif /* DIRED_SUPPORT && OK_PERMIT */
,"   default         same as commandline option -anonymous.  Disables"
,"                   default services for anonymous users.  Currently set to,"
,"                   all restricted except for: inside_telnet, outside_telnet,"
,"                   inside_news, inside_ftp, outside_ftp, inside_rlogin,"
,"                   outside_rlogin, goto, jump and mail.  Defaults"
,"                   are settable within userdefs.h"
#ifdef DIRED_SUPPORT
,"   dired_support   disallow local file management"
#endif /* DIRED_SUPPORT */
,"   disk_save       disallow saving to disk in the download and print menus"
,"   dotfiles        disallow access to, or creation of, hidden (dot) files"
,"   download        disallow downloaders in the download menu"
,"   editor          disallow editing"
,"   exec            disable execution scripts"
,"   exec_frozen     disallow the user from changing the execution link"
#ifdef USE_EXTERNALS
,"   externals       disable passing URLs to external programs"
#endif
,"   file_url        disallow using G)oto, served links or bookmarks for"
,"                   file: URL's"
,"   goto            disable the 'g' (goto) command"
#if !defined(HAVE_UTMP) || defined(VMS) /* not selective */
,"   inside_ftp      disallow ftps for people coming from inside your"
,"                   domain (utmp required for selectivity)"
,"   inside_news     disallow USENET news posting for people coming from"
,"                   inside your domain (utmp required for selectivity)"
,"   inside_rlogin   disallow rlogins for people coming from inside your"
,"                   domain (utmp required for selectivity)"
,"   inside_telnet   disallow telnets for people coming from inside your"
,"                   domain (utmp required for selectivity)"
#else
,"   inside_ftp      disallow ftps for people coming from inside your domain"
,"   inside_news     disallow USENET news posting for people coming from inside"
,"                   your domain"
,"   inside_rlogin   disallow rlogins for people coming from inside your domain"
,"   inside_telnet   disallow telnets for people coming from inside your domain"
#endif /* HAVE_UTMP || VMS */
,"   jump            disable the 'j' (jump) command"
,"   mail            disallow mail"
,"   multibook       disallow multiple bookmark files"
,"   news_post       disallow USENET News posting."
,"   option_save     disallow saving options in .lynxrc"
#if !defined(HAVE_UTMP) || defined(VMS) /* not selective */
,"   outside_ftp     disallow ftps for people coming from outside your"
,"                   domain (utmp required for selectivity)"
,"   outside_news    disallow USENET news posting for people coming from"
,"                   outside your domain (utmp required for selectivity)"
,"   outside_rlogin  disallow rlogins for people coming from outside your"
,"                   domain (utmp required for selectivity)"
,"   outside_telnet  disallow telnets for people coming from outside your"
,"                   domain (utmp required for selectivity)"
#else
,"   outside_ftp     disallow ftps for people coming from outside your domain"
,"   outside_news    disallow USENET news posting for people coming from outside"
,"                   your domain"
,"   outside_rlogin  disallow rlogins for people coming from outside your domain"
,"   outside_telnet  disallow telnets for people coming from outside your domain"
#endif /* !HAVE_UTMP || VMS */
,"   print           disallow most print options"
,"   shell           disallow shell escapes, and lynxexec, lynxprog or lynxcgi"
,"                   G)oto's"
,"   suspend         disallow Control-Z suspends with escape to shell"
,"   telnet_port     disallow specifying a port in telnet G)oto's"
,"   useragent       disallow modifications of the User-Agent header"
    };
    size_t n;

    if (next_arg != 0) {
	parse_restrictions(next_arg);
    } else {
	SetOutputMode( O_TEXT );
	for (n = 0; n < sizeof(Usage)/sizeof(Usage[0]); n++)
	    printf("%s\n", Usage[n]);
	SetOutputMode( O_BINARY );
	exit(0);
    }
    return 0;
}

/* -selective */
static int selective_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
   HTDirAccess = HT_DIR_SELECTIVE;
   return 0;
}

/* -source */
static int source_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    dump_output_immediately = TRUE;
    HTOutputFormat = (LYPrependBase ?
		      HTAtom_for("www/download") : HTAtom_for("www/dump"));
    LYcols = 999;
    return 0;
}

/* -traversal */
static int traversal_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    traversal = TRUE;
#ifdef USE_SLANG
    LYcols = 80;
#else
    LYcols = 999;
#endif /* USE_SLANG */

    return 0;
}

/* -version */
static int version_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    SetOutputMode( O_TEXT );

    printf("\n");
    printf(gettext("\n%s Version %s (%.*s)\n"),
	  LYNX_NAME, LYNX_VERSION,
	  LYNX_DATE_LEN,
	  LYNX_RELEASE ? LYNX_RELEASE_DATE : &LYNX_DATE[LYNX_DATE_OFF]
	  );
#ifdef SYSTEM_NAME
#ifndef __DATE__
#define __DATE__ ""
#endif
#ifndef __TIME__
#define __TIME__ ""
#endif
    printf(gettext("Built on %s %s %s\n"), SYSTEM_NAME, __DATE__, __TIME__);
#endif
    printf("\n");
    printf(gettext(
	  "Copyrights held by the University of Kansas, CERN, and other contributors.\n"
	  ));
    printf(gettext("Distributed under the GNU General Public License.\n"));
    printf(gettext(
	  "See http://lynx.browser.org/ and the online help for more information.\n\n"
	  ));

    SetOutputMode( O_BINARY );

    exit(0);
    /* NOT REACHED */
    return 0;
}

/* -width */
static int width_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0) {
	int w = atoi(next_arg);
	if (w > 0)
	    dump_output_width = ((w < 999) ? w : 999);
    }

    return 0;
}

/* NOTE: This table is sorted by name; the lookup relies on that. */
static Parse_Args_Type Arg_Table [] =
{
   PARSE_SET(
      "accept_all_cookies", SET_ARG,		&LYAcceptAllCookies,
      "\naccepts all cookies"
   ),
   PARSE_FUN(
      "anonymous",	FUNCTION_ARG,	anonymous_fun,
      "used to specify the anonymous account"
   ),
   PARSE_FUN(
      "assume_charset", NEED_FUNCTION_ARG, assume_charset_fun,
      "=MIMEname\ncharset for documents that don't specify it"
   ),
   PARSE_FUN(
      "assume_local_charset",	NEED_FUNCTION_ARG,assume_local_charset_fun,
      "=MIMEname\ncharset assumed for local files"
   ),
   PARSE_FUN(
      "assume_unrec_charset",	NEED_FUNCTION_ARG,assume_unrec_charset_fun,
      "=MIMEname\nuse this instead of unrecognized charsets"
   ),
   PARSE_FUN(
      "auth",		NEED_FUNCTION_ARG,	auth_fun,
      "=id:pw\nauthentication information for protected documents"
   ),
   PARSE_FUN(
      "base",		FUNCTION_ARG,		base_fun,
      "prepend a request URL comment and BASE tag to text/html\noutputs for -source dumps"
   ),
#ifdef USE_SLANG
   PARSE_FUN(
      "blink",		FUNCTION_ARG,		blink_fun,
      "force high intensity bg colors in color mode"
   ),
#endif
   PARSE_SET(
      "book",		SET_ARG,		&bookmark_start,
      "use the bookmark page as the startfile"
   ),
   PARSE_SET(
      "buried_news",	TOGGLE_ARG,		&scan_for_buried_news_references,
      "toggles scanning of news articles for buried references"
   ),
   PARSE_FUN(
      "cache",		NEED_FUNCTION_ARG,	cache_fun,
      "=NUMBER\nNUMBER of documents cached in memory"
   ),
   PARSE_SET(
      "case",		SET_ARG,		&case_sensitive,
      "enable case sensitive user searching"
   ),
   PARSE_STR(
      "cfg",		IGNORE_ARG|NEED_NEXT_ARG,	0,
      "=FILENAME\nspecifies a lynx.cfg file other than the default"
   ),
   PARSE_FUN(
      "child",		FUNCTION_ARG,		child_fun,
      "exit on left-arrow in startfile, and disable save to disk"
   ),
#ifdef USE_SLANG
   PARSE_FUN(
      "color",		FUNCTION_ARG,		color_fun,
      "force color mode on with standard bg colors"
   ),
#endif
   PARSE_SET(
      "cookies",	TOGGLE_ARG,		&LYSetCookies,
      "toggles handling of Set-Cookie headers"
   ),
#ifdef EXP_PERSISTENT_COOKIES
   PARSE_STR(
      "cookie_file",	LYSTRING_ARG,		&LYCookieFile,
      "=FILENAME\nspecifies a file to use to store cookies"
   ),
#endif /* EXP_PERSISTENT_COOKIES */
#ifndef VMS
   PARSE_SET(
      "core",		TOGGLE_ARG,		&LYNoCore,
      "toggles forced core dumps on fatal errors"
   ),
#endif
   PARSE_FUN(
      "crawl",		FUNCTION_ARG,		crawl_fun,
      "with -traversal, output each page to a file\n\
with -dump, format output as with -traversal, but to stdout"
   ),
#ifdef DISP_PARTIAL
   PARSE_SET(
      "debug_partial",	TOGGLE_ARG,		&debug_display_partial,
      "incremental display stages with MessageSecs delay"
   ),
#endif
   PARSE_FUN(
      "display",	NEED_FUNCTION_ARG,	display_fun,
      "=DISPLAY\nset the display variable for X exec'ed programs"
   ),
   PARSE_FUN(
      "dump",		FUNCTION_ARG,		dump_output_fun,
      "dump the first file to stdout and exit"
   ),
   PARSE_FUN(
      "editor",		NEED_FUNCTION_ARG,	editor_fun,
      "=EDITOR\nenable edit mode with specified editor"
   ),
   PARSE_SET(
      "emacskeys",	SET_ARG,		&emacs_keys,
      "enable emacs-like key movement"
   ),
   PARSE_SET(
      "enable_scrollback", TOGGLE_ARG,		&enable_scrollback,
      "\ntoggles compatibility with comm programs' scrollback\n\
keys (may be incompatible with some curses packages)"
   ),
   PARSE_FUN(
      "error_file",	NEED_FUNCTION_ARG,	error_file_fun,
      "=FILE\nwrite the HTTP status code here"
   ),
#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
#ifndef NEVER_ALLOW_REMOTE_EXEC
   PARSE_FUN(
      "exec",		FUNCTION_ARG,		exec_fun,
      "enable local program execution"
   ),
#endif
   PARSE_SET(
      "locexec",	SET_ARG,		&local_exec_on_local_files,
      "enable local program execution from local files only"
   ),
   PARSE_SET(
      "noexec",		UNSET_ARG,		&local_exec,
      "disable local program execution (DEFAULT)"
   ),
#endif /* EXEC_LINKS || EXEC_SCRIPTS */
#ifdef VMS
   PARSE_SET(
      "fileversions",	SET_ARG,		&HTVMSFileVersions,
      "include all versions of files in local VMS directory\nlistings"
   ),
#endif
   PARSE_SET(
      "force_html",	SET_ARG,		&LYforce_HTML_mode,
      "forces the first document to be interpreted as HTML"
   ),
   PARSE_SET(
      "force_secure",	TOGGLE_ARG,		&LYForceSSLCookiesSecure,
      "toggles forcing of the secure flag for SSL cookies"
   ),
   PARSE_SET(
      "from",		TOGGLE_ARG,		&LYNoFromHeader,
      "toggle transmissions of From headers"
   ),
#if !defined(NO_OPTION_FORMS) && !defined(NO_OPTION_MENU)
   PARSE_SET(
      "forms_options",	TOGGLE_ARG,		&LYUseFormsOptions,
      "toggles forms-based vs old-style options menu"
   ),
#endif
   PARSE_SET(
      "ftp",		UNSET_ARG,		&ftp_ok,
      "disable ftp access"
   ),
   PARSE_FUN(
      "get_data",	FUNCTION_ARG,		get_data_fun,
      "user data for get forms, read from stdin,\nterminated by '---' on a line"
   ),
   PARSE_SET(
      "head",		SET_ARG,		&HEAD_request,
      "send a HEAD request"
   ),
   PARSE_FUN(
      "help",		FUNCTION_ARG,		help_fun,
      "print this usage message"
   ),
   PARSE_FUN(
      "hiddenlinks",	NEED_FUNCTION_ARG,	hiddenlinks_fun,
      "=[option]\nhidden links: options are merge, listonly, or ignore"
   ),
   PARSE_SET(
      "historical",	TOGGLE_ARG,		&historical_comments,
      "toggles use of '>' or '-->' as a terminator for comments"
   ),
   PARSE_FUN(
      "homepage",	NEED_FUNCTION_ARG,	homepage_fun,
      "=URL\nset homepage separate from start page"
   ),
   PARSE_SET(
      "image_links",	TOGGLE_ARG,		&clickable_images,
      "toggles inclusion of links for all images"
   ),
   PARSE_STR(
      "index",		NEED_LYSTRING_ARG,	&indexfile,
      "=URL\nset the default index file to URL"
   ),
   PARSE_SET(
      "ismap",		TOGGLE_ARG,		&LYNoISMAPifUSEMAP,
      "toggles inclusion of ISMAP links when client-side\nMAPs are present"
   ),
   PARSE_INT(
      "link",		NEED_INT_ARG,		&ccount,
      "=NUMBER\nstarting count for lnk#.dat files produced by -crawl"
   ),
   PARSE_SET(
      "localhost",	SET_ARG,		&local_host_only,
      "disable URLs that point to remote hosts"
   ),
#if defined(USE_HASH)
   PARSE_STR(
      "lss",		IGNORE_ARG|NEED_NEXT_ARG,	0,
      "=FILENAME\nspecifies a lynx.lss file other than the default"
   ),
#endif
   PARSE_FUN(
      "mime_header",	FUNCTION_ARG,		mime_header_fun,
      "include mime headers and force source dump"
   ),
   PARSE_SET(
      "minimal",	TOGGLE_ARG,		&minimal_comments,
      "toggles minimal versus valid comment parsing"
   ),
#ifndef DISABLE_NEWS
   PARSE_FUN(
      "newschunksize",	NEED_FUNCTION_ARG,	newschunksize_fun,
      "=NUMBER\nnumber of articles in chunked news listings"
   ),
   PARSE_FUN(
      "newsmaxchunk",	NEED_FUNCTION_ARG,	newsmaxchunk_fun,
      "=NUMBER\nmaximum news articles in listings before chunking"
   ),
#endif
   PARSE_FUN(
      "nobrowse",	FUNCTION_ARG,		nobrowse_fun,
      "disable directory browsing"
   ),
   PARSE_SET(
      "nocc",		SET_ARG,		&LYNoCc,
      "disable Cc: prompts for self copies of mailings"
   ),
   PARSE_FUN(
      "nocolor",	FUNCTION_ARG,		nocolor_fun,
      "turn off color support"
   ),
   PARSE_SET(
      "nofilereferer",	SET_ARG,		&no_filereferer,
      "disable transmissions of Referer headers for file URLs"
   ),
   PARSE_SET(
      "nolist",		SET_ARG,		&nolist,
      "disable the link list feature in dumps"
   ),
   PARSE_SET(
      "nolog",		UNSET_ARG,		&error_logging,
      "disable mailing of error messages to document owners"
   ),
   PARSE_FUN(
      "nopause",	FUNCTION_ARG,		nopause_fun,
      "disable forced pauses for statusline messages"
   ),
   PARSE_SET(
      "noprint",	SET_ARG,		&no_print,
      "disable print functions"
   ),
   PARSE_SET(
      "noredir",	SET_ARG,		&no_url_redirection,
      "don't follow Location: redirection"
   ),
   PARSE_SET(
      "noreferer",	SET_ARG,		&LYNoRefererHeader,
      "disable transmissions of Referer headers"
   ),
#ifdef SOCKS
   PARSE_SET(
      "nosocks",	UNSET_ARG,		&socks_flag,
      "don't use SOCKS proxy for this session"
   ),
#endif
   PARSE_SET(
      "nostatus",	SET_ARG,		&no_statusline,
      "disable the miscellaneous information messages"
   ),
   PARSE_SET(
      "number_links",	SET_ARG,		&number_links,
      "force numbering of links"
   ),
#ifdef DISP_PARTIAL
   PARSE_SET(
      "partial",	TOGGLE_ARG,		&display_partial_flag,
      "toggles display partial pages while downloading"
   ),
   PARSE_INT(
      "partial_thres",  NEED_INT_ARG,          &partial_threshold,
      "[=NUMBER]\nnumber of lines to render before repainting display\n\
with partial-display logic"
   ),
#endif
   PARSE_FUN(
      "pauth",		NEED_FUNCTION_ARG,	pauth_fun,
      "=id:pw\nauthentication information for protected proxy server"
   ),
   PARSE_SET(
      "popup",		UNSET_ARG,		&LYUseDefSelPop,
      "toggles handling of single-choice SELECT options via\npopup windows or as lists of radio buttons"
   ),
   PARSE_FUN(
      "post_data",	FUNCTION_ARG,		post_data_fun,
      "user data for post forms, read from stdin,\nterminated by '---' on a line"
   ),
   PARSE_SET(
      "preparsed",	SET_ARG,		&LYPreparsedSource,
      "show parsed text/html with -source and in source view\n\
to visualize how lynx behaves with invalid HTML"
   ),
#ifdef USE_PSRC
   PARSE_SET(
      "prettysrc",	SET_ARG,		&LYpsrc,
      "do syntax highlighting and hyperlink handling in source view"
   ),
#endif
   PARSE_SET(
      "print",		UNSET_ARG,		&no_print,
      "enable print functions (DEFAULT)"
   ),
   PARSE_SET(
      "pseudo_inlines", TOGGLE_ARG,		&pseudo_inline_alts,
      "toggles pseudo-ALTs for inlines with no ALT string"
   ),
   PARSE_SET(
      "raw",		UNSET_ARG,		&LYUseDefaultRawMode,
      "toggles default setting of 8-bit character translations\nor CJK mode for the startup character set"
   ),
   PARSE_SET(
      "realm",		SET_ARG,		&check_realm,
      "restricts access to URLs in the starting realm"
   ),
   PARSE_SET(
      "reload",		SET_ARG,		&reloading,
      "flushes the cache on a proxy server\n(only the first document affected)"
   ),
   PARSE_FUN(
      "restrictions",	FUNCTION_ARG,		restrictions_fun,
      "=[options]\nuse -restrictions to see list"
   ),
   PARSE_SET(
      "resubmit_posts", TOGGLE_ARG,		&LYresubmit_posts,
      "toggles forced resubmissions (no-cache) of forms with\n\
method POST when the documents they returned are sought\n\
with the PREV_DOC command or from the History List"
   ),
   PARSE_SET(
      "rlogin",		UNSET_ARG,		&rlogin_ok,
      "disable rlogins"
   ),
   PARSE_FUN(
      "selective",	FUNCTION_ARG,		selective_fun,
      "require .www_browsable files to browse directories"
   ),
   PARSE_SET(
      "show_cursor",	TOGGLE_ARG,		&LYUseDefShoCur,
      "toggles hiding of the cursor in the lower right corner"
   ),
   PARSE_SET(
      "soft_dquotes",	TOGGLE_ARG,		&soft_dquotes,
      "toggles emulation of the old Netscape and Mosaic bug which\n\
treated '>' as a co-terminator for double-quotes and tags"
   ),
   PARSE_FUN(
      "source",		FUNCTION_ARG,		source_fun,
      "dump the source of the first file to stdout and exit"
   ),
   PARSE_SET(
      "stack_dump",	SET_ARG,		&stack_dump,
      "disable SIGINT cleanup handler"
   ),
   PARSE_SET(
      "startfile_ok",	SET_ARG,		&startfile_ok,
      "allow non-http startfile and homepage with -validate"
   ),
#ifndef VMS
#ifdef SYSLOG_REQUESTED_URLS
   PARSE_STR(
      "syslog",		NEED_LYSTRING_ARG,	&syslog_txt,
      "=text\ninformation for syslog call"
   ),
#endif
#endif
   PARSE_SET(
      "tagsoup",	SET_ARG,		&Old_DTD,
      "use TagSoup rather than SortaSGML parser"
   ),
   PARSE_SET(
      "telnet",		UNSET_ARG,		&telnet_ok,
      "disable telnets"
   ),
   PARSE_STR(
      "term",		NEED_STRING_ARG,	&terminal,
      "=TERM\nset terminal type to TERM"
   ),
   PARSE_SET(
      "tlog",		IGNORE_ARG,		0,
      "toggles use of a Lynx Trace Log for the current session"
   ),
   PARSE_SET(
      "trace",		IGNORE_ARG,		0,
      "turns on Lynx trace mode"
   ),
   PARSE_FUN(
      "traversal",	FUNCTION_ARG,		traversal_fun,
      "traverse all http links derived from startfile"
   ),
   PARSE_SET(
      "underscore",	TOGGLE_ARG,		&use_underscore,
      "toggles use of _underline_ format in dumps"
   ),
#if defined(NCURSES_MOUSE_VERSION) || defined(USE_SLANG_MOUSE)
   PARSE_SET(
      "use_mouse",	SET_ARG,		&LYUseMouse,
      "turn on xterm mouse support"
   ),
#endif
   PARSE_STR(
      "useragent",	NEED_LYSTRING_ARG,	&LYUserAgent,
      "=Name\nset alternate Lynx User-Agent header"
   ),
   PARSE_SET(
      "validate",	IGNORE_ARG,		0,
      "accept only http URLs (for validation)"
   ),
   PARSE_SET(
      "verbose",	TOGGLE_ARG,		&verbose_img,
      "toggles [LINK], [IMAGE] and [INLINE] comments \nwith filenames of these images"
   ),
   PARSE_FUN(
      "version",	FUNCTION_ARG,		version_fun,
      "print Lynx version information"
   ),
   PARSE_SET(
      "vikeys",		SET_ARG,		&vi_keys,
      "enable vi-like key movement"
   ),
   PARSE_FUN(
      "width",		NEED_FUNCTION_ARG,	width_fun,
      "=NUMBER\nscreen width for formatting of dumps (default is 80)"
   ),
   {NULL, 0, 0, NULL}
};

static void print_help_strings ARGS3(
	CONST char *,	name,
	CONST char *,	help,
	CONST char *,	value)
{
    int pad;
    int c;
    int first;
    int field_width = 20;

    pad = field_width - (4 + (int) strlen (name));

    fprintf (stdout, "   -%s", name);

    if (*help != '=') {
	pad--;
	while (pad > 0) {
	    fputc (' ', stdout);
	    pad--;
	}
	fputc (' ', stdout);	  /* at least one space */
	first = 0;
    } else {
	first = pad;
    }

    if (strchr (help, '\n') == 0) {
	fprintf (stdout, "%s", help);
    } else {
	while ((c = *help) != 0) {
	    if (c == '\n') {
		if ((pad = --first) < 0) {
		    pad = field_width;
		} else {
		    c = ' ';
		}
		fputc (c, stdout);
		while (pad--)
		    fputc (' ', stdout);
	    } else {
		fputc (c, stdout);
	    }
	    help++;
	    first--;
	}
    }
    if (value)
	printf(" (%s)", value);
    fputc ('\n', stdout);
}

static void print_help_and_exit ARGS1(int, exit_status)
{
    Parse_Args_Type *p;

    if (pgm == NULL) pgm = "lynx";

    SetOutputMode( O_TEXT );

    fprintf (stdout, gettext("USAGE: %s [options] [file]\n"), pgm);
    fprintf (stdout, gettext("Options are:\n"));
    print_help_strings("",
"receive the arguments from stdin (enclose\n\
in double-quotes (\"-\") on VMS)", NULL);

    for (p = Arg_Table; p->name != 0; p++) {
	char temp[LINESIZE], *value = temp;
#ifdef PARSE_DEBUG
	Parse_Args_Type * q = p;
#else
	ParseUnion *q = (ParseUnion *)(&(p->value));
#endif
	switch (p->type & ARG_TYPE_MASK) {
	    case TOGGLE_ARG:
	    case SET_ARG:
		sprintf(temp, "%s", *(q->set_value) ? "on" : "off");
		break;
	    case UNSET_ARG:
		sprintf(temp, "%s", *(q->set_value) ? "off" : "on");
		break;
	    case INT_ARG:
		sprintf(temp, "%d", *(q->int_value));
		break;
	    case STRING_ARG:
		if ((value = *(q->str_value)) != 0
		 && !*value)
		    value = 0;
		break;
	    default:
		value = 0;
		break;
	}
	print_help_strings(p->name, p->help_string, value);
    }

    SetOutputMode( O_BINARY );

    exit (exit_status);
}

/*
 * This function performs a string comparison on two strings a and b.  a is
 * assumed to be an ordinary null terminated string, but b may be terminated
 * by an '=', '+' or '-' character.  If terminated by '=', *c will be pointed
 * to the character following the '='.  If terminated by '+' or '-', *c will
 * be pointed to that character.  (+/- added for toggle processing - BL.)
 * If a and b match, it returns 1.  Otherwise 0 is returned.
 */
static int arg_eqs_parse ARGS3(
	CONST char *,	a,
	char *,		b,
	char **,	c)
{
    *c = NULL;
    while (1) {
	if ((*a != *b)
	 || (*a == 0)
	 || (*b == 0)) {
	    if (*a == 0) {
		switch (*b) {
		case '=':
		    *c = b + 1;
		    return 1;
		case '-':	/* FALLTHRU */
		case '+':
		    *c = b;
		    return 1;
		case 0:
		    return 1;
		default:
		    return 0;
		}
	    } else {
		return 0;
	    }
	}
	a++;
	b++;
     }
}

#define is_true(s)  (*s == '1' || *s == '+' || !strcmp(s, "on"))
#define is_false(s) (*s == '0' || *s == '-' || !strcmp(s, "off"))

PRIVATE void parse_arg ARGS2(
	char **,	argv,
	int *,		i)
{
    Parse_Args_Type *p;
    char *arg_name;

    arg_name = argv[0];

    /*
     *	Check for a command line startfile. - FM
     */
#if !EXTENDED_OPTION_LOGIC
    if (*arg_name != '-')
#else
    if (*arg_name != '-' || no_options_further == TRUE )
#endif
    {
	StrAllocCopy(startfile, arg_name);
	LYTrimStartfile(startfile);
	return;
    }
#if EXTENDED_OPTION_LOGIC
    if (strcmp(arg_name,"--") == 0) {
	no_options_further = TRUE;
	return;
    }
#endif

    /* lose the first '-' character */
    arg_name++;

    /*
     *	Skip any lone "-" arguments, because we've loaded
     *	the stdin input into an HTList structure for
     *	special handling. - FM
     */
    if (*arg_name == 0)
	return;

    /* allow GNU-style options with -- prefix*/
    if (*arg_name == '-') ++arg_name;


    p = Arg_Table;
    while (p->name != 0) {
#ifdef PARSE_DEBUG
	Parse_Args_Type *q = p;
#else
	ParseUnion *q = (ParseUnion *)(&(p->value));
#endif
	ParseFunc fun;
	char *next_arg = NULL;

	if ((p->name[0] != *arg_name)
	    || (0 == arg_eqs_parse (p->name, arg_name, &next_arg))) {
	    p++;
	    continue;
	}

	if ((p->type & NEED_NEXT_ARG) && (next_arg == 0)) {
	    next_arg = argv[1];
	    if ((i != 0) && (next_arg != 0))
		(*i)++;
	}

	switch (p->type & ARG_TYPE_MASK) {
	case TOGGLE_ARG:	/* FALLTHRU */
	case SET_ARG:		/* FALLTHRU */
	case UNSET_ARG:
	     if (q->set_value != 0) {
		 if (next_arg == 0) {
		    switch (p->type & ARG_TYPE_MASK) {
		    case TOGGLE_ARG:
			 *(q->set_value) = !(*(q->set_value));
			 break;
		    case SET_ARG:
			 *(q->set_value) = TRUE;
			 break;
		    case UNSET_ARG:
			 *(q->set_value) = FALSE;
			 break;
		    }
		 } else if (is_true(next_arg)) {
		     *(q->set_value) = TRUE;
		 } else if (is_false(next_arg)) {
		     *(q->set_value) = FALSE;
		 }
		 /* deliberately ignore anything else - BL */
	     }
	     break;

	case FUNCTION_ARG:
	     fun = q->fun_value;
	     if (0 != fun) {
		 if (-1 == (*fun) (next_arg)) {
		 }
	     }
	     break;

	case LYSTRING_ARG:
	     if ((q->str_value != 0) && (next_arg != 0))
		 StrAllocCopy(*(q->str_value), next_arg);
	     break;

	case INT_ARG:
	     if ((q->int_value != 0) && (next_arg != 0))
		 *(q->int_value) = atoi (next_arg);
	     break;

	case STRING_ARG:
	     if ((q->str_value != 0) && (next_arg != 0))
		*(q->str_value) = next_arg;
	     break;

	case IGNORE_ARG:
	     break;
	}

	return;
    }

    if (pgm == 0) pgm = "LYNX";

    fprintf (stderr, gettext("%s: Invalid Option: %s\n"), pgm, argv[0]);
    print_help_and_exit (-1);
}

#ifndef VMS
PRIVATE void FatalProblem ARGS1(
	int,		sig)
{
    /*
     *	Ignore further interrupts. - mhc: 11/2/91
     */
#ifndef NOSIGHUP
    (void) signal(SIGHUP, SIG_IGN);
#endif /* NOSIGHUP */
    (void) signal (SIGTERM, SIG_IGN);
    (void) signal (SIGINT, SIG_IGN);
#ifndef __linux__
#ifndef DOSPATH
    (void) signal(SIGBUS, SIG_IGN);
#endif /* ! DOSPATH */
#endif /* !__linux__ */
    (void) signal(SIGSEGV, SIG_IGN);
    (void) signal(SIGILL, SIG_IGN);

    /*
     *	Flush all messages. - FM
     */
    fflush(stderr);
    fflush(stdout);

    /*
     *	Deal with curses, if on, and clean up. - FM
     */
    if (LYOutOfMemory && LYCursesON) {
	sleep(AlertSecs);
    }
    cleanup_sig(0);
#ifndef __linux__
#ifndef DOSPATH
    signal(SIGBUS, SIG_DFL);
#endif /* DOSPATH */
#endif /* !__linux__ */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGILL, SIG_DFL);

    /*
     *	Issue appropriate messages and abort or exit. - FM
     */
    if (LYOutOfMemory == FALSE) {
	fprintf (stderr, "\r\n\
A Fatal error has occurred in %s Ver. %s\r\n", LYNX_NAME, LYNX_VERSION);

	fprintf(stderr, "\r\n\
Please notify your system administrator to confirm a bug, and\r\n\
if confirmed, to notify the lynx-dev list.  Bug reports should\r\n\
have concise descriptions of the command and/or URL which causes\r\n\
the problem, the operating system name with version number, the\r\n\
TCPIP implementation, and any other relevant information.\r\n");

	fprintf(stderr, "\r\n\
Do NOT mail the core file if one was generated.\r\n");

	fprintf(stderr, "\r\n\
Lynx now exiting with signal:  %d\r\n\r\n", sig);

	/*
	 *  Exit and possibly dump core.
	 */
	if (LYNoCore) {
	    exit(-1);
	}
	abort();

    } else {
	LYOutOfMemory = FALSE;
	printf("\r\n%s\r\n\r\n", MEMORY_EXHAUSTED_ABORT);
	fflush(stdout);

	/*
	 *  Exit without dumping core.
	 */
	exit(0);
    }
}
#endif /* !VMS */
