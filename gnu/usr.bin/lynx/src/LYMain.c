#include <HTUtils.h>
#include <HTTP.h>
#include <HTParse.h>
#include <HTAccess.h>
#include <HTList.h>
#include <HTFile.h>
#include <UCMap.h>
#include <UCDefs.h>
#include <HTInit.h>
#include <HTAlert.h>
#include <LYCurses.h>
#include <LYStyle.h>
#include <HTML.h>
#include <LYUtils.h>
#include <LYGlobalDefs.h>
#include <LYOptions.h>
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
#include <LYShowInfo.h>

#ifdef VMS
#include <HTFTP.h>
#endif /* !DECNET */

#ifdef __DJGPP__
#include <dos.h>
#include <dpmi.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/exceptn.h>
#endif /* __DJGPP__ */

#ifdef __EMX__
#include <io.h>
#endif

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
#define TRACE_FILE "LY-TRACE.LOG"
#else
#define COOKIE_FILE ".lynx_cookies"
#define TRACE_FILE "Lynx.trace"
#endif /* FNAMES_8_3 */

/* ahhhhhhhhhh!! Global variables :-< */
#ifdef SOCKS
PUBLIC BOOLEAN socks_flag=TRUE;
#endif /* SOCKS */

#ifdef IGNORE_CTRL_C
PUBLIC BOOLEAN sigint = FALSE;
#endif /* IGNORE_CTRL_C */

#ifdef __DJGPP__
PRIVATE char init_ctrl_break[1];
#endif /* __DJGPP__ */

#if USE_VMS_MAILER
PUBLIC char *mail_adrs = NULL;	/* the mask for a VMS mail transport */
#endif

#ifdef VMS
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

#ifdef DIRED_SUPPORT
PUBLIC BOOLEAN lynx_edit_mode = FALSE;
PUBLIC BOOLEAN no_dired_support = FALSE;
PUBLIC HTList *tagged = NULL;
PUBLIC int LYAutoUncacheDirLists = 2; /* default dired uncaching behavior */
PUBLIC int dir_list_order = ORDER_BY_NAME;
PUBLIC int dir_list_style = MIXED_STYLE;

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
PUBLIC lynx_list_item_type *printers = NULL;
			    /* linked list of download options */
PUBLIC lynx_list_item_type *downloaders = NULL;
			    /* linked list of upload options */
#ifdef USE_EXTERNALS
PUBLIC lynx_list_item_type *externals = NULL;
			    /* linked list of external options */
#endif

PUBLIC lynx_list_item_type *uploaders = NULL;
PUBLIC int port_syntax = 1;
PUBLIC int LYShowColor = SHOW_COLOR_UNKNOWN; /* to show or not */
PUBLIC int LYrcShowColor = SHOW_COLOR_UNKNOWN; /* ... last used */

#if !defined(NO_OPTION_FORMS) && !defined(NO_OPTION_MENU)
PUBLIC BOOLEAN LYUseFormsOptions = TRUE; /* use forms-based options menu */
#endif

PUBLIC BOOLEAN LYJumpFileURL = FALSE;	 /* always FALSE the first time */
PUBLIC BOOLEAN LYPermitURL = FALSE;
PUBLIC BOOLEAN LYRestricted = FALSE; /* whether we have -anonymous option */
PUBLIC BOOLEAN LYShowCursor = SHOW_CURSOR; /* to show or not to show */
PUBLIC BOOLEAN LYUnderlineLinks = UNDERLINE_LINKS; /* Show the links underlined vs bold */
PUBLIC BOOLEAN LYUseDefShoCur = TRUE;	/* Command line -show_cursor toggle */
PUBLIC BOOLEAN LYUserSpecifiedURL = TRUE;/* always TRUE  the first time */
PUBLIC BOOLEAN LYValidate = FALSE;
PUBLIC BOOLEAN LYforce_no_cache = FALSE;
PUBLIC BOOLEAN LYinternal_flag = FALSE; /* override no-cache b/c internal link*/
PUBLIC BOOLEAN LYoverride_no_cache = FALSE;/*override no-cache b/c history etc*/
PUBLIC BOOLEAN LYresubmit_posts = ALWAYS_RESUBMIT_POSTS;
PUBLIC BOOLEAN LYtrimInputFields = FALSE;
PUBLIC BOOLEAN bold_H1 = FALSE;
PUBLIC BOOLEAN bold_headers = FALSE;
PUBLIC BOOLEAN bold_name_anchors = FALSE;
PUBLIC BOOLEAN case_sensitive = CASE_SENSITIVE_ALWAYS_ON;
PUBLIC BOOLEAN check_mail = CHECKMAIL;
PUBLIC BOOLEAN child_lynx = FALSE;
PUBLIC BOOLEAN dump_output_immediately = FALSE;
PUBLIC BOOLEAN emacs_keys = EMACS_KEYS_ALWAYS_ON;
PUBLIC BOOLEAN error_logging = MAIL_SYSTEM_ERROR_LOGGING;
PUBLIC BOOLEAN ftp_passive = FTP_PASSIVE; /* TRUE if doing ftp in passive mode */
PUBLIC BOOLEAN ftp_local_passive;
PUBLIC char *ftp_lasthost;
PUBLIC BOOLEAN goto_buffer = GOTOBUFFER; /* TRUE if offering default goto URL */
PUBLIC BOOLEAN historical_comments = FALSE;
PUBLIC BOOLEAN is_www_index = FALSE;
PUBLIC BOOLEAN jump_buffer = JUMPBUFFER; /* TRUE if offering default shortcut */
PUBLIC BOOLEAN lynx_mode = NORMAL_LYNX_MODE;
PUBLIC BOOLEAN minimal_comments = FALSE;
PUBLIC BOOLEAN nolist = FALSE;
PUBLIC BOOLEAN number_fields_on_left = TRUE;
PUBLIC BOOLEAN number_links_on_left = TRUE;
PUBLIC BOOLEAN recent_sizechange = FALSE;/* the window size changed recently? */
PUBLIC BOOLEAN soft_dquotes = FALSE;
PUBLIC BOOLEAN use_underscore = SUBSTITUTE_UNDERSCORES;
PUBLIC BOOLEAN verbose_img = VERBOSE_IMAGES;  /* show filenames or not */
PUBLIC BOOLEAN vi_keys = VI_KEYS_ALWAYS_ON;
PUBLIC int keypad_mode = DEFAULT_KEYPAD_MODE;
PUBLIC int user_mode = NOVICE_MODE;

PUBLIC BOOLEAN telnet_ok = TRUE;
#ifndef DISABLE_NEWS
PUBLIC BOOLEAN news_ok = TRUE;
#endif
PUBLIC BOOLEAN rlogin_ok = TRUE;
PUBLIC BOOLEAN long_url_ok = FALSE;
PUBLIC BOOLEAN ftp_ok = TRUE;
PUBLIC BOOLEAN system_editor = FALSE;

PUBLIC BOOLEAN had_restrictions_default = FALSE;
PUBLIC BOOLEAN had_restrictions_all = FALSE;

PUBLIC BOOLEAN exec_frozen = FALSE;
PUBLIC BOOLEAN no_bookmark = FALSE;
PUBLIC BOOLEAN no_bookmark_exec = FALSE;
PUBLIC BOOLEAN no_chdir = FALSE;
PUBLIC BOOLEAN no_disk_save = FALSE;
PUBLIC BOOLEAN no_dotfiles = NO_DOT_FILES;
PUBLIC BOOLEAN no_download = FALSE;
PUBLIC BOOLEAN no_editor = FALSE;
PUBLIC BOOLEAN no_exec = FALSE;
PUBLIC BOOLEAN no_file_url = FALSE;
PUBLIC BOOLEAN no_goto = FALSE;
PUBLIC BOOLEAN no_goto_configinfo = FALSE;
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
PUBLIC BOOLEAN no_goto_rlogin = FALSE;
PUBLIC BOOLEAN no_goto_telnet = FALSE;
PUBLIC BOOLEAN no_goto_tn3270 = FALSE;
PUBLIC BOOLEAN no_goto_wais = FALSE;
PUBLIC BOOLEAN no_inside_ftp = FALSE;
PUBLIC BOOLEAN no_inside_rlogin = FALSE;
PUBLIC BOOLEAN no_inside_telnet = FALSE;
PUBLIC BOOLEAN no_jump = FALSE;
PUBLIC BOOLEAN no_lynxcfg_info = FALSE;
PUBLIC BOOLEAN no_lynxcgi = FALSE;
PUBLIC BOOLEAN no_mail = FALSE;
PUBLIC BOOLEAN no_multibook = FALSE;
PUBLIC BOOLEAN no_option_save = FALSE;
PUBLIC BOOLEAN no_outside_ftp = FALSE;
PUBLIC BOOLEAN no_outside_rlogin = FALSE;
PUBLIC BOOLEAN no_outside_telnet = FALSE;
PUBLIC BOOLEAN no_print = FALSE;
PUBLIC BOOLEAN no_shell = FALSE;
PUBLIC BOOLEAN no_suspend = FALSE;
PUBLIC BOOLEAN no_telnet_port = FALSE;
PUBLIC BOOLEAN no_useragent = FALSE;

#ifndef DISABLE_NEWS
PUBLIC BOOLEAN no_goto_news = FALSE;
PUBLIC BOOLEAN no_goto_nntp = FALSE;
PUBLIC BOOLEAN no_goto_snews = FALSE;
PUBLIC BOOLEAN no_inside_news = FALSE;
PUBLIC BOOLEAN no_newspost = FALSE;
PUBLIC BOOLEAN no_outside_news = FALSE;
#endif

#ifdef USE_EXTERNALS
PUBLIC BOOLEAN no_externals = FALSE;
#endif

#ifndef NO_CONFIG_INFO
PUBLIC BOOLEAN no_lynxcfg_xinfo = FALSE;
#ifdef HAVE_CONFIG_H
PUBLIC BOOLEAN no_compileopts_info = FALSE;
#endif
#endif

PUBLIC BOOLEAN no_statusline = FALSE;
PUBLIC BOOLEAN no_filereferer = TRUE;
PUBLIC char LYRefererWithQuery = 'D';	/* 'D' for drop */
PUBLIC BOOLEAN local_host_only = FALSE;
PUBLIC BOOLEAN override_no_download = FALSE;
PUBLIC BOOLEAN show_dotfiles = FALSE;	/* From rcfile if no_dotfiles is false */
PUBLIC BOOLEAN LYforce_HTML_mode = FALSE;
PUBLIC BOOLEAN LYfind_leaks = TRUE;

#ifdef __DJGPP__
PUBLIC BOOLEAN watt_debug = FALSE;	/* WATT-32 debugging */
PUBLIC BOOLEAN dj_is_bash = FALSE;  /* Check for bash shell under DJGPP */
#endif /* __DJGPP__ */

#ifdef WIN_EX
PUBLIC BOOLEAN focus_window = FALSE;	/* 1998/10/05 (Mon) 17:18:42 */
PUBLIC char windows_drive[4];		/* 1998/01/13 (Tue) 21:13:24 */
#endif

#ifdef _WINDOWS
#define	TIMEOUT	180			/* 1998/03/30 (Mon) 14:50:44 */
PUBLIC int lynx_timeout = TIMEOUT;
PUBLIC CRITICAL_SECTION critSec_DNS;	/* 1998/09/03 (Thu) 22:01:56 */
PUBLIC CRITICAL_SECTION critSec_READ;	/* 1998/09/03 (Thu) 22:01:56 */
#endif /* _WINDOWS */

#if defined(WIN_EX)
PUBLIC BOOLEAN system_is_NT = FALSE;
#endif

#ifdef SH_EX
PUBLIC BOOLEAN show_cfg = FALSE;
#endif

PUBLIC BOOLEAN no_table_center = FALSE;	/* 1998/10/09 (Fri) 15:12:49 */

#if USE_BLAT_MAILER
PUBLIC BOOLEAN mail_is_blat = TRUE;
#endif

#ifdef USE_BLINK
#  ifdef __EMX__
PUBLIC BOOLEAN term_blink_is_boldbg = TRUE;
#  else
PUBLIC BOOLEAN term_blink_is_boldbg = FALSE;
#  endif
#endif

PUBLIC BOOLEAN HEAD_request = FALSE;
PUBLIC BOOLEAN LYAcceptAllCookies = ACCEPT_ALL_COOKIES; /* take all cookies? */
PUBLIC BOOLEAN LYCancelledFetch = FALSE;/* TRUE if cancelled binary fetch */
PUBLIC BOOLEAN LYCollapseBRs = COLLAPSE_BR_TAGS;  /* Collapse serial BRs? */
PUBLIC BOOLEAN LYDefaultRawMode;
PUBLIC BOOLEAN LYListNewsDates = LIST_NEWS_DATES;
PUBLIC BOOLEAN LYListNewsNumbers = LIST_NEWS_NUMBERS;
PUBLIC BOOLEAN LYMBMBlocked = BLOCK_MULTI_BOOKMARKS;
PUBLIC BOOLEAN LYNewsPosting = NEWS_POSTING; /* News posting supported? */
PUBLIC BOOLEAN LYNoFromHeader = TRUE;	/* Never send From header?	   */
PUBLIC BOOLEAN LYNoRefererForThis=FALSE;/* No Referer header for this URL? */
PUBLIC BOOLEAN LYNoRefererHeader=FALSE; /* Never send Referer header?	   */
PUBLIC BOOLEAN LYRawMode;
PUBLIC BOOLEAN LYSelectPopups = USE_SELECT_POPUPS;
PUBLIC BOOLEAN LYSetCookies = SET_COOKIES; /* Process Set-Cookie headers? */
PUBLIC BOOLEAN LYUseDefSelPop = TRUE;	/* Command line -popup toggle */
PUBLIC BOOLEAN LYUseDefaultRawMode = TRUE;
PUBLIC BOOLEAN LYUseMouse = FALSE;
PUBLIC BOOLEAN LYisConfiguredForX = FALSE;
PUBLIC BOOLEAN UCForce8bitTOUPPER = FALSE; /* override locale for case-conversion? */
PUBLIC BOOLEAN UCSaveBookmarksInUnicode = FALSE;
PUBLIC BOOLEAN bookmark_start = FALSE;
PUBLIC BOOLEAN check_realm = FALSE;  /* Restrict to the starting realm? */
PUBLIC BOOLEAN clickable_images = MAKE_LINKS_FOR_ALL_IMAGES;
PUBLIC BOOLEAN crawl = FALSE;		/* Do crawl? */
PUBLIC BOOLEAN keep_mime_headers = FALSE; /* Include mime headers with source dump */
PUBLIC BOOLEAN more = FALSE;		/* is there more text to display? */
PUBLIC BOOLEAN more_links = FALSE;	/* Links beyond a displayed page with no links? */
PUBLIC BOOLEAN no_url_redirection = FALSE; /* Don't follow URL redirections */
PUBLIC BOOLEAN pseudo_inline_alts = MAKE_PSEUDO_ALTS_FOR_INLINES;
PUBLIC BOOLEAN scan_for_buried_news_references = TRUE;
PUBLIC BOOLEAN startfile_ok = FALSE;
PUBLIC BOOLEAN startfile_stdin = FALSE;
PUBLIC BOOLEAN traversal = FALSE;	/* Do traversals? */
PUBLIC char *BookmarkPage = NULL;	/* the name of the current bookmark page */
PUBLIC char *LYCookieAcceptDomains = NULL; /* domains to accept all cookies */
PUBLIC char *LYCookieLooseCheckDomains = NULL;  /* check loosely   */
PUBLIC char *LYCookieQueryCheckDomains = NULL;  /* check w/a query */
PUBLIC char *LYCookieRejectDomains = NULL; /* domains to reject all cookies */
PUBLIC char *LYCookieSAcceptDomains = NULL; /* domains to accept all cookies */
PUBLIC char *LYCookieSLooseCheckDomains = NULL;  /* check loosely   */
PUBLIC char *LYCookieSQueryCheckDomains = NULL;  /* check w/a query */
PUBLIC char *LYCookieSRejectDomains = NULL; /* domains to reject all cookies */
PUBLIC char *LYCookieSStrictCheckDomains = NULL; /* check strictly  */
PUBLIC char *LYCookieStrictCheckDomains = NULL; /* check strictly  */
PUBLIC char *LYHostName = NULL;		/* treat as a local host name */
PUBLIC char *LYLocalDomain = NULL;	/* treat as a local domain tail */
PUBLIC char *LYUserAgent = NULL;	/* Lynx User-Agent header	   */
PUBLIC char *LYUserAgentDefault = NULL; /* Lynx default User-Agent header  */
PUBLIC char *LynxHome = NULL;		/* the default Home HREF. */
PUBLIC char *LynxSigFile = NULL;	/* Signature file, in or off home */
PUBLIC char *UCAssume_MIMEcharset = NULL;
PUBLIC char *URLDomainPrefixes = NULL;
PUBLIC char *URLDomainSuffixes = NULL;
PUBLIC char *authentication_info[2] = {NULL, NULL}; /* Id:Password for protected documents */
PUBLIC char *bookmark_page = NULL;	/* the name of the default bookmark page */
PUBLIC char *editor = NULL;		/* the name of the current editor */
PUBLIC char *form_get_data = NULL;	/* User data for get form */
PUBLIC char *form_post_data = NULL;	/* User data for post form */
PUBLIC char *global_extension_map = NULL;  /* global mime.types */
PUBLIC char *global_type_map = NULL;	/* global mailcap */
PUBLIC char *helpfile = NULL;		/* the main help file */
PUBLIC char *helpfilepath = NULL;	/* the path to the help file set */
PUBLIC char *homepage = NULL;		/* home page or main screen */
PUBLIC char *http_error_file = NULL;	/* Place HTTP status code in this file */
PUBLIC char *indexfile = NULL;		/* an index file if there is one */
PUBLIC char *jumpfile = NULL;		/* the name of the default jumps file */
PUBLIC char *jumpprompt = NULL;		/* the default jumps prompt */
PUBLIC char *language = NULL;		/* preferred language */
PUBLIC char *lynx_cfg_file = NULL;	/* location of active lynx.cfg */
PUBLIC char *lynx_cmd_logfile;		/* file to write keystroke commands, if any */
PUBLIC char *lynx_cmd_script;		/* file to read keystroke commands, if any */
PUBLIC char *lynx_save_space = NULL;	/* The prefix for save to disk paths */
PUBLIC char *lynx_temp_space = NULL;	/* The prefix for temporary file paths */
PUBLIC char *lynxjumpfile = NULL;	/* the current jump file URL */
PUBLIC char *lynxlinksfile = NULL;	/* the current visited links file URL */
PUBLIC char *lynxlistfile = NULL;	/* the current list file URL */
PUBLIC char *original_dir = NULL;	/* the original directory */
PUBLIC char *personal_extension_map = NULL;/* .mime.types */
PUBLIC char *personal_mail_address = NULL; /* the users mail address */
PUBLIC char *personal_type_map = NULL;	   /* .mailcap */
PUBLIC char *pref_charset = NULL;	/* preferred character set */
PUBLIC char *proxyauth_info[2] = {NULL, NULL}; /* Id:Password for protected proxy servers */
PUBLIC char *startfile = NULL;		/* the first file */
PUBLIC char *startrealm = NULL;		/* the startfile realm */
PUBLIC char *system_mail = NULL;	/* The path for sending mail */
PUBLIC char *system_mail_flags = NULL;	/* Flags for sending mail */
PUBLIC char *x_display = NULL;		/* display environment variable */
PUBLIC HistInfo history[MAXHIST];
PUBLIC int AlertSecs;			/* time-delay for HTAlert() messages   */
PUBLIC int DebugSecs;			/* time-delay for HTProgress messages */
PUBLIC int InfoSecs;			/* time-delay for Information messages */
PUBLIC int LYMultiBookmarks = MULTI_BOOKMARK_SUPPORT;
PUBLIC int LYStatusLine = -1;		/* Line for statusline() if > -1 */
PUBLIC int LYcols = DFT_COLS;
PUBLIC int LYlines = DFT_ROWS;
PUBLIC int MessageSecs;			/* time-delay for important Messages   */
PUBLIC int ReplaySecs;			/* time-delay for command-scripts */
PUBLIC int ccount = 0;			/* Starting number for lnk#.dat files in crawls */
PUBLIC int dump_output_width = 0;
PUBLIC int lynx_temp_subspace = 0;	/* > 0 if we made temp-directory */
PUBLIC int nhist = 0;			/* number of history entries */
PUBLIC int nlinks = 0;			/* number of links in memory */
PUBLIC int outgoing_mail_charset = -1;	/* translate mail to this charset */
PUBLIC LinkInfo links[MAXLINKS];

#ifndef DISABLE_BIBP
PUBLIC BOOLEAN BibP_bibhost_available = FALSE;  /* until check succeeds  */
PUBLIC BOOLEAN BibP_bibhost_checked = FALSE;  /*  until LYCheckBibHost   */
PUBLIC BOOLEAN no_goto_bibp = FALSE;
PUBLIC char *BibP_bibhost = NULL;	 /* local server for bibp: links  */
PUBLIC char *BibP_globalserver = NULL;   /* global server for bibp: links */
#endif

#ifdef USE_PERSISTENT_COOKIES
PUBLIC BOOLEAN persistent_cookies = FALSE; /* disabled by default! */
PUBLIC char *LYCookieFile = NULL;	/* cookie read file */
PUBLIC char *LYCookieSaveFile = NULL;	/* cookie save file */
#endif /* USE_PERSISTENT_COOKIES */

#ifdef EXP_NESTED_TABLES
PUBLIC BOOLEAN nested_tables =
#if defined(USE_COLOR_STYLE)
    TRUE
#else
    FALSE				/* see 2001-08-15  */
#endif
    ;
#endif

PUBLIC BOOLEAN LYShowTransferRate = TRUE;
PUBLIC int LYTransferRate = rateKB;
PUBLIC char * LYTransferName = NULL;

PUBLIC char *XLoadImageCommand = NULL;	/* Default image viewer for X */
PUBLIC BOOLEAN LYNoISMAPifUSEMAP = FALSE; /* Omit ISMAP link if MAP present? */
PUBLIC int LYHiddenLinks = HIDDENLINKS_SEPARATE; /* Show hidden links? */

PUBLIC int Old_DTD = NO;
PRIVATE BOOL DTD_recovery = NO;

#ifndef NO_LYNX_TRACE
PUBLIC FILE *LYTraceLogFP = NULL;		/* Pointer for TRACE log  */
#endif
PUBLIC char *LYTraceLogPath = NULL;		/* Path for TRACE log	   */
PUBLIC BOOLEAN LYUseTraceLog = USE_TRACE_LOG;	/* Use a TRACE log?	   */

PUBLIC BOOLEAN LYSeekFragMAPinCur = TRUE;
PUBLIC BOOLEAN LYSeekFragAREAinCur = TRUE;
PUBLIC BOOLEAN LYStripDotDotURLs = TRUE;	/* Try to fix ../ in some URLs? */
PUBLIC BOOLEAN LYForceSSLCookiesSecure = FALSE;
PUBLIC BOOLEAN LYNoCc = FALSE;
PUBLIC BOOLEAN LYPreparsedSource = FALSE;	/* Show source as preparsed? */
PUBLIC BOOLEAN LYPrependBaseToSource = TRUE;
PUBLIC BOOLEAN LYPrependCharsetToSource = TRUE;
PUBLIC BOOLEAN LYQuitDefaultYes = QUIT_DEFAULT_YES;
PUBLIC BOOLEAN dont_wrap_pre = FALSE;

PUBLIC int cookie_noprompt;

#ifdef USE_SSL
PUBLIC int ssl_noprompt = FORCE_PROMPT_DFT;
#endif

PUBLIC int connect_timeout = 18000; /*=180000*0.1 - used in HTDoConnect.*/

#ifdef EXP_JUSTIFY_ELTS
PUBLIC BOOL ok_justify = TRUE;
PUBLIC int justify_max_void_percent = 35;
#endif

#ifdef EXP_LOCALE_CHARSET
PUBLIC BOOLEAN LYLocaleCharset = FALSE;
#endif

#ifndef NO_DUMP_WITH_BACKSPACES
PUBLIC BOOLEAN with_backspaces = FALSE;
#endif

#if defined(PDCURSES) && defined(PDC_BUILD) && PDC_BUILD >= 2401
PUBLIC int scrsize_x = 0;
PUBLIC int scrsize_y = 0;
#endif

PUBLIC BOOL force_empty_hrefless_a = FALSE;

#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
PUBLIC BOOL textfields_need_activation = FALSE;
PUBLIC BOOL textfields_activation_option = FALSE;
#endif

PUBLIC BOOLEAN textfield_prompt_at_left_edge = FALSE;

#ifdef MARK_HIDDEN_LINKS
PUBLIC char* hidden_link_marker = NULL;
#endif

#ifdef DISP_PARTIAL
PUBLIC BOOLEAN display_partial_flag = TRUE; /* Display document during download */
PUBLIC BOOLEAN debug_display_partial = FALSE; /* Show with MessageSecs delay */
PUBLIC int partial_threshold = -1;  /* # of lines to be d/l'ed until we repaint */
#endif

PUBLIC BOOLEAN LYNonRestartingSIGWINCH = FALSE;
PUBLIC BOOLEAN LYReuseTempfiles = FALSE;
PUBLIC BOOLEAN LYUseBuiltinSuffixes = TRUE;

#ifdef MISC_EXP
PUBLIC int LYNoZapKey = 0; /* 0: off (do z checking), 1: full, 2: initially */
#endif

#ifndef DISABLE_NEWS
#include <HTNews.h>
#endif

PUBLIC BOOLEAN FileInitAlreadyDone = FALSE;

PRIVATE BOOLEAN stack_dump = FALSE;
PRIVATE char *terminal = NULL;
PRIVATE char *pgm;
PRIVATE BOOLEAN number_links = FALSE;
PRIVATE BOOLEAN number_fields = FALSE;
PRIVATE BOOLEAN LYPrependBase = FALSE;
PRIVATE HTList *LYStdinArgs = NULL;

#ifndef EXTENDED_OPTION_LOGIC
/* if set then '--' will be recognized as the end of options */
#define EXTENDED_OPTION_LOGIC 1
#endif

#ifndef EXTENDED_STARTFILE_RECALL
/* if set then additional non-option args (before the last one) will be
   made available for 'g'oto recall - kw */
#define EXTENDED_STARTFILE_RECALL 1
#endif

#ifndef OPTNAME_ALLOW_DASHES
/* if set, then will allow dashes and underscores to be used interchangeable
   in commandline option's names - VH */
#define OPTNAME_ALLOW_DASHES 1
#endif

#if EXTENDED_OPTION_LOGIC
PRIVATE BOOLEAN no_options_further=FALSE; /* set to TRUE after '--' argument */
#endif

PRIVATE BOOL parse_arg PARAMS((char **arg, unsigned mask, int *i));
PRIVATE void print_help_and_exit PARAMS((int exit_status)) GCC_NORETURN;
PRIVATE void print_help_strings PARAMS((CONST char * name, CONST char * help, CONST char * value, BOOLEAN option));

#ifndef VMS
PUBLIC BOOLEAN LYNoCore = NO_FORCED_CORE_DUMP;
PUBLIC BOOLEAN restore_sigpipe_for_children = FALSE;
PRIVATE void FatalProblem PARAMS((int sig));
#endif /* !VMS */

#if defined(USE_COLOR_STYLE)
PUBLIC char *lynx_lss_file = NULL;
#endif

#ifdef __DJGPP__
PRIVATE void LY_set_ctrl_break(int setting)
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

#if defined(WIN_EX)
PRIVATE int is_windows_nt(void)
{
    DWORD version;

    version = GetVersion();
    if ((version & 0x80000000) == 0)
	return 1;
    else
	return 0;
}
#endif


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
#endif /* VMS */
#ifndef VMS
    FREE(lynx_version_putenv_command);
#endif

#if USE_VMS_MAILER
    FREE(mail_adrs);
#endif

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
#ifndef DISABLE_BIBP
    FREE(BibP_bibhost);
    FREE(BibP_globalserver);
#endif
#ifdef USE_PERSISTENT_COOKIES
    FREE(LYCookieFile);
    FREE(LYCookieSaveFile);
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
    FREE(lynx_temp_space);
    FREE(LYTransferName);
    FREE(LYTraceLogPath);
    FREE(lynx_cfg_file);
#if defined(USE_COLOR_STYLE)
    FREE(lynx_lss_file);
#endif
    FREE(UCAssume_MIMEcharset);
    LYUIPages_free();
    for (i = 0; i < nlinks; i++) {
	FREE(links[i].lname);
    }
    nlinks = 0;
    HTList_delete(LYcommandList());

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

PRIVATE void FixCharacters(void)
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

PRIVATE int argncmp ARGS2(
	char*,		str,
	char*,		what)
{
    if (str[0] == '-' && str[1] == '-' ) ++str;
#if OPTNAME_ALLOW_DASHES
    return strncmp(str, what, strlen(what));
#else
    ++str; ++what; /*skip leading dash in both strings*/
    return !strn_dash_equ(str, what, strlen(what));
#endif
}

PRIVATE void tildeExpand ARGS2(
	char **,	pathname,
	BOOLEAN,	embedded)
{
    char *temp = *pathname;

    if (embedded) {
	if (temp != NULL) {
	    temp = strstr(*pathname, "/~");
	    if (temp != 0)
		temp++;
	    else
		temp = *pathname;
	}
    }

    if (temp != NULL
     && temp[0] == '~') {
	if (temp[1] == '/'
	 && temp[2] != '\0') {
	    temp = NULL;
	    StrAllocCopy(temp, *pathname + 2);
	    StrAllocCopy(*pathname, wwwName(Home_Dir()));
	    LYAddPathSep(pathname);
	    StrAllocCat(*pathname, temp);
	    FREE(temp);
	} else if (temp[1] == '\0') {
	    StrAllocCopy(*pathname, wwwName(Home_Dir()));
	}
    }
}

PRIVATE BOOL GetStdin ARGS2(
	char **,	buf,
	BOOL,		marker)
{
    if (LYSafeGets(buf, stdin) != 0
     && (!marker || strncmp(*buf, "---", 3) != 0)) {
	LYTrimTrailing(*buf);
	CTRACE((tfp, "...data: %s\n", *buf));
	return TRUE;
    }
    CTRACE((tfp, "...mark: %s\n", *buf ? *buf : ""));
    return FALSE;
}

#ifdef WIN32
PRIVATE BOOL cleanup_win32(DWORD fdwCtrlType)
{
    switch (fdwCtrlType) {
    case CTRL_CLOSE_EVENT:
	cleanup_sig(-1);
	return TRUE;
    default:
	return FALSE;
    }
}
#endif

/*
 * Append the SSL version to lynx version or user-agent string.
 */
#ifdef USE_SSL
PRIVATE void append_ssl_version ARGS2(
	char **,	target,
	char *,		separator)
{
    char SSLLibraryVersion[256];
    char *SSLcp;

    HTSprintf(target, " SSL-MM%s1.4.1", separator);

#undef LYNX_SSL_VERSION

#if defined(SSLEAY_VERSION)
#define LYNX_SSL_VERSION SSLeay_version(SSLEAY_VERSION)
#else
#if defined(OPENSSL_VERSION_TEXT)
#define LYNX_SSL_VERSION OPENSSL_VERSION_TEXT
#else
#if defined(GNUTLS_VERSION)
#define LYNX_SSL_VERSION GNUTLS_VERSION
#endif /* GNUTLS_VERSION */
#endif /* OPENSSL_VERSION_TEXT */
#endif

#ifdef LYNX_SSL_VERSION
    if (*separator == ' ')
	StrAllocCat(*target, ",");
    LYstrncpy(SSLLibraryVersion, LYNX_SSL_VERSION, sizeof(SSLLibraryVersion)-1);
    if ((SSLcp = strchr(SSLLibraryVersion, ' ')) != NULL) {
	*SSLcp++ = *separator;
	if ((SSLcp = strchr(SSLcp, ' ')) != NULL) {
	    *SSLcp = '\0';
	    StrAllocCat(*target, " ");
	    StrAllocCat(*target, SSLLibraryVersion);
	}
    }
#endif /* LYNX_SSL_VERSION */
}
#endif /* USE_SSL */

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
    struct stat dir_info;
    char filename[LY_MAXPATH];
    BOOL LYGetStdinArgs = FALSE;
#ifdef _WINDOWS
    WSADATA WSAData;
#endif /* _WINDOWS */

    /*
     * Just in case someone has the idea to install lynx set-uid, let's try
     * to discourage it.
     */
#if defined(GETUID) && defined(SETUID)
    setuid(getuid());
#endif

#ifdef    NOT_ASCII
    FixCharacters();
#endif /* NOT_ASCII */

#ifndef DISABLE_FTP
    /* malloc a sizeof(char) so 1st strcmp() won't dump in HTLoadFile() */
    ftp_lasthost = calloc(1,sizeof(char));
#endif

#ifdef EXP_CHARSET_CHOICE
    memset((char*)charset_subsets, 0, sizeof(charset_subset_t)*MAXCHARSETS);
#endif

#ifdef _WINDOWS
    {
	int err;
	WORD wVerReq;

	wVerReq = MAKEWORD(1, 1);

	err = WSAStartup(wVerReq, &WSAData);
	if (err != 0)
	{
	    printf(gettext("No Winsock found, sorry."));
	    sleep(5);
	    return 1;
	}
    }

    /* 1998/09/03 (Thu) 22:02:32 */
    InitializeCriticalSection(&critSec_DNS);
    InitializeCriticalSection(&critSec_READ);

#endif /* _WINDOWS */

#if 0 /* defined(__CYGWIN__) - does not work with screen */
    if (strcmp(ttyname(fileno(stdout)), "/dev/conout") != 0) {
	printf("please \"$CYGWIN=notty\"\n");
	exit(EXIT_SUCCESS);
    }
#endif

#if defined(WIN_EX)
    /* 1997/10/19 (Sun) 21:40:54 */
    system_is_NT = (BOOL) is_windows_nt();

    /* 1998/01/13 (Tue) 21:13:47 */
    GetWindowsDirectory(filename, sizeof filename);
    windows_drive[0] = filename[0];
    windows_drive[1] = filename[1];
    windows_drive[2] = '\0';
#endif


#ifdef __DJGPP__
    if (LY_get_ctrl_break() == 0) {
	LY_set_ctrl_break(TRUE);
	init_ctrl_break[0] = 0;
    } else {
	init_ctrl_break[0] = 1;
    }
    __djgpp_set_sigquit_key(0x082D); /* Bind ALT-X to SIGQUIT */
    signal(SIGQUIT, cleanup_sig);
    atexit(reset_break);

    if (((cp = LYGetEnv("SHELL")) != NULL)
      && (strstr(LYPathLeaf(cp), "sh") != NULL))
	dj_is_bash = TRUE;
#endif /* __DJGPP__ */

    /*
     * To prevent corrupting binary data on DOS, MS-WINDOWS or OS/2
     * we open files and stdout in BINARY mode by default.
     * Where necessary we should open and (close!) TEXT mode.
     * (use LYNewTxtFile/LYAppendToTxtFile to open text files for writing)
     */
    SetDefaultMode(O_BINARY);
    SetOutputMode(O_BINARY);

#ifdef DOSPATH
    if (LYGetEnv("TERM")==NULL) putenv("TERM=vt100");
#endif

    LYShowColor = (SHOW_COLOR ? SHOW_COLOR_ON : SHOW_COLOR_OFF);
    /*
     *	Set up the argument list.
     */
    pgm = argv[0];
    cp = NULL;
    if ((cp = LYLastPathSep(pgm)) != NULL) {
	pgm = cp + 1;
    }

    /*
     *	Act on -help NOW, so we only output the help and exit. - FM
     */
    for (i = 1; i < argc; i++) {
	if (argncmp(argv[i], "-help") == 0) {
	    parse_arg(&argv[i], 1, &i);
	}
#ifdef SH_EX
	if (strncmp(argv[i], "-show_cfg", 9) == 0) {
	    show_cfg = TRUE;
	}
#endif
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
#if defined(HAVE_LIBINTL_H) || defined(HAVE_LIBGETTEXT_H)
#ifndef __DJGPP__
    if ((cp = LYGetEnv("LYNX_LOCALEDIR")) == 0)
	cp = LOCALEDIR;
    bindtextdomain ("lynx", cp);
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
    terminal = LYGetEnv("TERM");
    if ((terminal == NULL) || !strncasecomp(terminal, "xterm", 5))
	terminal = "vt100";
#endif /* ULTRIX */
    /*
     *	Zero the links and history struct arrays.
     */
    memset((void *)links, 0, sizeof(LinkInfo)*MAXLINKS);
    memset((void *)history, 0, sizeof(HistInfo)*MAXHIST);
    /*
     *	Zero the MultiBookmark arrays.
     */
    memset((void *)MBM_A_subbookmark, 0, sizeof(char)*(MBM_V_MAXFILES+1));
    memset((void *)MBM_A_subdescript, 0, sizeof(char)*(MBM_V_MAXFILES+1));
#ifndef VMS
    StrAllocCopy(list_format, LIST_FORMAT);
#endif /* !VMS */

    AlertSecs	= SECS2Secs(ALERTSECS);
    DebugSecs	= SECS2Secs(DEBUGSECS);
    InfoSecs	= SECS2Secs(INFOSECS);
    MessageSecs	= SECS2Secs(MESSAGESECS);
    ReplaySecs	= SECS2Secs(REPLAYSECS);

    StrAllocCopy(LYTransferName, "KiB");
    StrAllocCopy(helpfile, HELPFILE);
    StrAllocCopy(startfile, STARTFILE);
    LYEscapeStartfile(&startfile);
    StrAllocCopy(indexfile, DEFAULT_INDEX_FILE);
    StrAllocCopy(global_type_map, GLOBAL_MAILCAP);
    StrAllocCopy(personal_type_map, PERSONAL_MAILCAP);
    StrAllocCopy(global_extension_map, GLOBAL_EXTENSION_MAP);
    StrAllocCopy(personal_extension_map, PERSONAL_EXTENSION_MAP);
    StrAllocCopy(language, PREFERRED_LANGUAGE);
    StrAllocCopy(pref_charset, PREFERRED_CHARSET);
    StrAllocCopy(system_mail, SYSTEM_MAIL);
#ifdef SYSTEM_MAIL_FLAGS
    StrAllocCopy(system_mail_flags, SYSTEM_MAIL_FLAGS);
#else
    StrAllocCopy(system_mail_flags, "");
#endif

    StrAllocCopy(LYUserAgent, LYNX_NAME);
    StrAllocCat(LYUserAgent, "/");
    StrAllocCat(LYUserAgent, LYNX_VERSION);
    if (HTLibraryVersion) {
	StrAllocCat(LYUserAgent, " libwww-FM/");
	StrAllocCat(LYUserAgent, HTLibraryVersion);
    }
#ifdef USE_SSL
    append_ssl_version(&LYUserAgent, "/");
#endif /* USE_SSL */
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

    if ((cp = LYGetEnv("LYNX_TEMP_SPACE")) != NULL)
	StrAllocCopy(lynx_temp_space, cp);
#if defined (UNIX) || defined (__DJGPP__)
    else if ((cp = LYGetEnv("TMPDIR")) != NULL)
	StrAllocCopy(lynx_temp_space, cp);
#endif
#if defined (DOSPATH) || defined (__EMX__)
    else if ((cp = LYGetEnv("TEMP")) != NULL)
	StrAllocCopy(lynx_temp_space, cp);
    else if ((cp = LYGetEnv("TMP")) != NULL)
	StrAllocCopy(lynx_temp_space, cp);
#endif
    else
#ifdef TEMP_SPACE
	StrAllocCopy(lynx_temp_space, TEMP_SPACE);
#else
    {
	printf(gettext("You MUST define a valid TMP or TEMP area!\n"));
	exit(EXIT_FAILURE);
    }
#endif

#ifdef WIN_EX	/* for Windows 2000 ... 1999/08/23 (Mon) 08:24:35 */
    if (access(lynx_temp_space, 0) != 0)
#endif
	tildeExpand(&lynx_temp_space, TRUE);

    if ((cp = strstr(lynx_temp_space, "$USER")) != NULL) {
	char *cp1;

	if ((cp1 = LYGetEnv("USER")) != NULL) {
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
    StrAllocCat(lynx_temp_space, "/lynx-XXXXXXXXXX");
    lynx_temp_space = mkdtemp(lynx_temp_space);
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
    StrAllocCopy(lynx_temp_space, HTSYS_name(lynx_temp_space));
#endif /* VMS */

    if ((HTStat(lynx_temp_space, &dir_info) < 0
#if defined(MULTI_USER_UNIX)
	&& mkdir(lynx_temp_space, 0700) < 0
#endif
	)
     || !S_ISDIR(dir_info.st_mode)) {
	fprintf(stderr, "%s: %s\n",
		lynx_temp_space,
		gettext("No such directory"));
	exit_immediately(EXIT_FAILURE);
    }

#if USE_VMS_MAILER
#ifndef MAIL_ADRS
#define MAIL_ADRS "\"IN%%\"\"%s\"\"\""
#endif
    StrAllocCopy(mail_adrs, MAIL_ADRS);
#endif

#ifdef LYNX_HOST_NAME
    StrAllocCopy(LYHostName, LYNX_HOST_NAME);
#else
    StrAllocCopy(LYHostName, HTHostName());
#endif /* LYNX_HOST_NAME */

    StrAllocCopy(LYLocalDomain, LOCAL_DOMAIN);
    StrAllocCopy(URLDomainPrefixes, URL_DOMAIN_PREFIXES);
    StrAllocCopy(URLDomainSuffixes, URL_DOMAIN_SUFFIXES);
    StrAllocCopy(XLoadImageCommand, XLOADIMAGE_COMMAND);

#ifndef DISABLE_BIBP
    StrAllocCopy(BibP_globalserver, BIBP_GLOBAL_SERVER);
    StrAllocCopy(BibP_bibhost, "http://bibhost/");  /* protocol specified. */
#endif

    /*
     *	Disable news posting if the compilation-based
     *	LYNewsPosting value is FALSE.  This may be changed
     *	further down via lynx.cfg or the -restriction
     *	command line switch. - FM
     */
#ifndef DISABLE_NEWS
    no_newspost = (BOOL) (LYNewsPosting == FALSE);
#endif

    /*
     *	Set up trace, the anonymous account defaults,
     *	validate restrictions, and/or the nosocks flag,
     *	if requested, and an alternate configuration
     *	file, if specified, NOW.  Also, if we only want
     *	the help menu, output that and exit. - FM
     */
#ifndef NO_LYNX_TRACE
    if (LYGetEnv("LYNX_TRACE") != 0) {
	WWW_TraceFlag = TRUE;
    }
#endif
    for (i = 1; i < argc; i++) {
	parse_arg(&argv[i], 2, &i);
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
     *	-post_data, the data are terminated by a "---" string, alone
     *	on the line (also terminated by RETURN). - FM
     */
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-") == 0) {
	    LYGetStdinArgs = TRUE;
	    break;
	}
    }
    if (LYGetStdinArgs == TRUE) {
	char *buf = NULL;

	CTRACE((tfp, "processing stdin arguments\n"));
	while (GetStdin(&buf, TRUE)) {
	    char *noargv[2];

	    noargv[0] = buf;
	    noargv[1] = NULL;
	    LYTrimTrailing(buf);

	    if (parse_arg(&noargv[0], 2, (int *)0) == FALSE
	     &&  buf[0] != '\0') {
		char *argument = NULL;

		if (LYStdinArgs == NULL) {
		    LYStdinArgs = HTList_new();
#ifdef LY_FIND_LEAKS
		    atexit(LYStdinArgs_free);
#endif
		}
		StrAllocCopy(argument, buf);
		HTList_appendObject(LYStdinArgs, argument);
		CTRACE((tfp, "...StdinArg:%s\n", argument));
	    } else {
		CTRACE((tfp, "...complete:%s\n", buf));
	    }
	}
	CTRACE((tfp, "...done with stdin arguments\n"));
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
	strlen(ANONYMOUS_USER) > 0 &&
#if defined (VMS) || defined (NOUSERS)
	!strcasecomp((LYGetEnv("USER")==NULL ? " " : LYGetEnv("USER")),
		     ANONYMOUS_USER)
#else
#ifdef HAVE_CUSERID
	STREQ((char *)cuserid((char *) NULL), ANONYMOUS_USER)
#else
	STREQ(((char *)getlogin()==NULL ? " " : getlogin()), ANONYMOUS_USER)
#endif /* HAVE_CUSERID */
#endif /* VMS */
	) {
	parse_restrictions("default");
	LYRestricted = TRUE;
	LYUseTraceLog = FALSE;
    }

    /*
     *	Set up the TRACE log path, and logging if appropriate. - FM
     */
    if ((cp = LYGetEnv("LYNX_TRACE_FILE")) == 0)
	cp = TRACE_FILE;
    LYAddPathToHome(LYTraceLogPath = malloc(LY_MAXPATH), LY_MAXPATH, cp);

    LYOpenTraceLog();

#ifdef EXP_CMD_LOGGING
    /*
     *	Open command-script, if specified
     */
    if (lynx_cmd_script != 0) {
	tildeExpand(&lynx_cmd_script, TRUE);
	LYOpenCmdScript();
    }
    /*
     *	Open command-logging, if specified
     */
    if (lynx_cmd_logfile != 0) {
	tildeExpand(&lynx_cmd_logfile, TRUE);
	LYOpenCmdLogfile(argc, argv);
    }
#endif

    /*
     *	Set up the default jump file stuff. - FM
     */
    StrAllocCopy(jumpprompt, JUMP_PROMPT);
#ifdef JUMPFILE
    StrAllocCopy(jumpfile, JUMPFILE);
    {
	temp = NULL;
	HTSprintf0(&temp, "JUMPFILE:%s", jumpfile);
	if (!LYJumpInit(temp)) {
	    CTRACE((tfp, "Failed to register %s\n", temp));
	}
	FREE(temp);
    }
#endif /* JUMPFILE */

    /*
     *	If no alternate configuration file was specified on
     *	the command line, see if it's in the environment.
     */
    if (!lynx_cfg_file) {
	if (((cp=LYGetEnv("LYNX_CFG")) != NULL) ||
	    (cp=LYGetEnv("lynx_cfg")) != NULL)
	    StrAllocCopy(lynx_cfg_file, cp);
    }

    /*
     *	If we still don't have a configuration file,
     *	use the userdefs.h definition.
     */
    if (!lynx_cfg_file)
	StrAllocCopy(lynx_cfg_file, LYNX_CFG_FILE);

#ifndef _WINDOWS /* avoid the whole ~ thing for now */
    tildeExpand(&lynx_cfg_file, FALSE);
#endif

    /*
     *	If the configuration file is not available,
     *	inform the user and exit.
     */
    if (!LYCanReadFile(lynx_cfg_file)) {
	fprintf(stderr, gettext("\nConfiguration file %s is not available.\n\n"),
			lynx_cfg_file);
	exit(EXIT_FAILURE);
    }

    /*
     * Make sure we have the character sets declared.
     *	This will initialize the CHARTRANS handling. - KW
     */
    if (!LYCharSetsDeclared()) {
	fprintf(stderr, gettext("\nLynx character sets not declared.\n\n"));
	exit(EXIT_FAILURE);
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
#ifdef CAN_AUTODETECT_DISPLAY_CHARSET
    if (auto_display_charset >= 0)
	current_char_set = auto_display_charset;
    else
#endif
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
	exit(EXIT_FAILURE);
    }

#if defined(USE_COLOR_STYLE)
    /*
     *	If no alternate lynx-style file was specified on
     *	the command line, see if it's in the environment.
     */
    if (!lynx_lss_file) {
	if (((cp=LYGetEnv("LYNX_LSS")) != NULL) ||
	    (cp=LYGetEnv("lynx_lss")) != NULL)
	    StrAllocCopy(lynx_lss_file, cp);
    }

    /*
     *	If we still don't have a lynx-style file,
     *	use the userdefs.h definition.
     */
    if (!lynx_lss_file)
	StrAllocCopy(lynx_lss_file, LYNX_LSS_FILE);

    tildeExpand(&lynx_lss_file, TRUE);

    /*
     *	If the lynx-style file is not available,
     *	inform the user and exit.
     */
    if (!LYCanReadFile(lynx_lss_file)) {
	fprintf(stderr, gettext("\nLynx file %s is not available.\n\n"),
			lynx_lss_file);
    }
    else
    {
	style_readFromFile(lynx_lss_file);
    }
#endif /* USE_COLOR_STYLE */

#ifdef USE_COLOR_TABLE
    /*
     *	Set up default foreground and background colors.
     */
    lynx_setup_colors();
#endif /* USE_COLOR_TABLE */

    /*
     *  Set the original directory, used for default download
     */
    if (!strcmp(Current_Dir(filename), ".")) {
	if ((cp = LYGetEnv("PWD")) != 0)
	    StrAllocCopy(original_dir, cp);
    } else {
	StrAllocCopy(original_dir, filename);
    }

    /*
     *	Set the compilation default signature file. - FM
     */
    LYstrncpy(filename, LYNX_SIG_FILE, sizeof(filename)-1);
    if (LYPathOffHomeOK(filename, sizeof(filename))) {
	StrAllocCopy(LynxSigFile, filename);
	LYAddPathToHome(filename, sizeof(filename), LynxSigFile);
	StrAllocCopy(LynxSigFile, filename);
	CTRACE((tfp, "LYNX_SIG_FILE set to '%s'\n", LynxSigFile));
    } else {
	CTRACE((tfp, "LYNX_SIG_FILE '%s' is bad. Ignoring.\n", LYNX_SIG_FILE));
    }

#ifdef USE_PRETTYSRC
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
    read_rc(NULL);

#ifdef EXP_LOCALE_CHARSET
    LYFindLocaleCharset();
#endif

    /*
     * Get WWW_HOME environment variable if it exists.
     */
    if ((cp = LYGetEnv("WWW_HOME")) != NULL) {
	StrAllocCopy(startfile, cp);
	LYEscapeStartfile(&startfile);
    }

    /*
     * Set the LynxHome URL.  If it's a file URL and the
     * host is defaulted, force in "//localhost", and if
     * it's not an absolute URL, make it one. - FM
     */
    StrAllocCopy(LynxHome, startfile);
    LYEnsureAbsoluteURL((char **)&LynxHome, "LynxHome", FALSE);

    /*
     *  Process any command line arguments not already handled. - FM
     */
    for (i = 1; i < argc; i++) {
	parse_arg(&argv[i], 4, &i);
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
	    parse_arg(my_args, 4, (int *)0);
	}
	LYStdinArgs_free();
    }
#ifdef CAN_SWITCH_DISPLAY_CHARSET
    if (current_char_set == auto_display_charset) /* Better: explicit option */
	switch_display_charsets = 1;
#endif

#if defined (TTY_DEVICE) || defined(HAVE_TTYNAME)
    /*
     *	If we are told to read the startfile from standard input, do it now,
     *	after we have read all of the option data from standard input.
     */
    if (startfile_stdin) {
	char result[LY_MAXPATH];
	char *buf = NULL;
	char *tty = NULL;
# ifdef HAVE_TTYNAME
	tty = ttyname(fileno(stderr));
# endif
	if (tty == NULL)
	    tty = isatty(fileno(stdin)) ? TTY_DEVICE : NUL_DEVICE;

	CTRACE((tfp, "processing stdin startfile, tty=%s\n", tty));
	if ((fp = LYOpenTemp (result, HTML_SUFFIX, "w")) != 0) {
	    StrAllocCopy(startfile, result);
	    while (GetStdin(&buf, FALSE)) {
		fputs(buf, fp);
		fputc('\n', fp);
	    }
	    FREE(buf);
	    LYCloseTempFP(fp);
	}
	CTRACE((tfp, "...done stdin startfile\n"));
	if ((freopen(tty, "r", stdin)) == 0) {
	    CTRACE((tfp, "cannot open a terminal (%s)\n", tty));
	    if (!dump_output_immediately) {
		fprintf(stderr, "cannot open a terminal (%s)\n", tty);
		exit(1);
	    }
	}
    }
#endif

    /*
     *  Initialize other things based on the configuration read.
     */

#ifdef USE_PRETTYSRC
    if ( (!Old_DTD) != TRUE ) /* skip if they are already initialized -HV */
#endif
    HTSwitchDTD(!Old_DTD);

    /*
     * Set up the proper character set with the desired
     * startup raw 8-bit or CJK mode handling.  - FM
     */
    HTMLUseCharacterSet(current_char_set);

#ifdef USE_PERSISTENT_COOKIES
    /*
     * Sod it, this looks like a reasonable place to load the
     * cookies file, probably.  - RP
     *
     * And to set LYCookieSaveFile. - BJP
     */
    if (persistent_cookies) {
	if(LYCookieFile == NULL) {
	    LYAddPathToHome(LYCookieFile = malloc(LY_MAXPATH), LY_MAXPATH, COOKIE_FILE);
	} else {
	    tildeExpand(&LYCookieFile, FALSE);
	}
	LYLoadCookies(LYCookieFile);
    }

    /* tilde-expand LYCookieSaveFile */
    if (LYCookieSaveFile != NULL) {
	tildeExpand(&LYCookieSaveFile, FALSE);
    }

    /*
     * In dump_output_immediately mode, LYCookieSaveFile defaults to
     * /dev/null, otherwise it defaults to LYCookieFile.
     */

    if (LYCookieSaveFile == NULL) {
	if (dump_output_immediately) {
	    StrAllocCopy(LYCookieSaveFile, "/dev/null");
	} else {
	    StrAllocCopy(LYCookieSaveFile, LYCookieFile);
	}
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
    if ((cp = LYGetEnv("LYNX_SAVE_SPACE")) != NULL)
	StrAllocCopy(lynx_save_space, cp);

    /*
     *	We have a save space path, make sure it's valid. - FM
     */
    if (lynx_save_space && *lynx_save_space == '\0') {
	FREE(lynx_save_space);
    }
    if (lynx_save_space) {
	tildeExpand(&lynx_save_space, TRUE);
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
    if (!FileInitAlreadyDone)
	HTFileInit();

    if (!LYCheckUserAgent()) {
	HTAlwaysAlert(gettext("Warning:"), UA_NO_LYNX_WARNING);
    }
#ifdef SH_EX
    if (show_cfg) {
	cleanup();
	exit(EXIT_SUCCESS);
    }
#endif

#ifdef USE_SLANG
    if (LYShowColor >= SHOW_COLOR_ON &&
	!(Lynx_Color_Flags & SL_LYNX_USE_COLOR)) {
	Lynx_Color_Flags |= SL_LYNX_USE_COLOR;
    } else if ((Lynx_Color_Flags & SL_LYNX_USE_COLOR) ||
	       LYGetEnv("COLORTERM") != NULL) {
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

    if (crawl) {
	/* No numbered links by default, as documented
	   in CRAWL.announce. - kw */
	if (!number_links) {
	    keypad_mode = NUMBERS_AS_ARROWS;
	}
    }

    if (!links_are_numbered()) {
	if (number_fields)
	    keypad_mode = LINKS_AND_FIELDS_ARE_NUMBERED;
	if (number_links)
	    keypad_mode = LINKS_ARE_NUMBERED;
	set_numbers_as_arrows();
    }

    /*
     *	Check the -popup command line toggle. - FM
     */
    if (LYUseDefSelPop == FALSE) {
	LYSelectPopups = !LYSelectPopups;
    }

    /*
     *	Check the -show_cursor command line toggle. - FM
     */
    if (LYUseDefShoCur == FALSE) {
	LYShowCursor = !LYShowCursor;
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
	LYMultiBookmarks = MBM_OFF;
	LYMBMBlocked = TRUE;
	no_multibook = TRUE;
    }

#ifdef USE_SOURCE_CACHE
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

#if defined (__DJGPP__)
    if (watt_debug)
	dbug_init();
    sock_init();

    __system_flags =
	__system_emulate_chdir	      | /* handle `cd' internally */
	__system_handle_null_commands | /* ignore cmds with no effect */
	__system_allow_long_cmds      | /* handle commands > 126 chars	 */
	__system_use_shell	      | /* use $SHELL if set */
	__system_allow_multiple_cmds  | /* allow `cmd1; cmd2; ...' */
	__system_redirect;		/* redirect internally */

    /* This speeds up stat() tremendously */
    _djstat_flags |= _STAT_INODE | _STAT_EXEC_MAGIC |_STAT_DIRSIZE;
#endif  /* __DJGPP__ */

    /* trap interrupts */
#ifdef WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE) cleanup_win32, TRUE);
#endif

#ifndef NOSIGHUP
    if (!dump_output_immediately)
	(void) signal(SIGHUP, cleanup_sig);
#endif /* NOSIGHUP */

    (void) signal(SIGTERM, cleanup_sig);
#ifdef SIGWINCH
    LYExtSignal(SIGWINCH, size_change);
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
	if (signal(SIGPIPE, SIG_IGN) != SIG_IGN)
	     restore_sigpipe_for_children = TRUE;
#endif /* DOSPATH */
    }
#endif /* !VMS */

#ifdef SIGTSTP
    /*
     *	Block Control-Z suspending if requested. - FM
     */
    if (no_suspend)
	(void) signal(SIGTSTP, SIG_IGN);
#endif /* SIGTSTP */

    /*
     *	Check for a valid HEAD request. - FM
     */
    if (HEAD_request && LYCanDoHEAD(startfile) != TRUE) {
	fprintf(stderr,
 "The '-head' switch is for http HEAD requests and cannot be used for\n'%s'.\n",
		startfile);
	exit_immediately(EXIT_FAILURE);
    }

    /*
     *	Check for a valid MIME headers request. - FM
     */
    if (keep_mime_headers && LYCanDoHEAD(startfile) != TRUE) {
	fprintf(stderr,
 "The '-mime_header' switch is for http URLs and cannot be used for\n'%s'.\n",
		startfile);
	exit_immediately(EXIT_FAILURE);
    }

    /*
     *	Check for a valid traversal request. - FM
     */
    if (traversal && strncmp(startfile, "http", 4)) {
	fprintf(stderr,
 "The '-traversal' switch is for http URLs and cannot be used for\n'%s'.\n",
		startfile);
	exit_immediately(EXIT_FAILURE);
    }

    /*
     *  Finish setting up for an INTERACTIVE session.
     *  Done here so that URL guessing in LYEnsureAbsoluteURL() can be
     *  interruptible (terminal is in raw mode, select() works).  -BL
     */
#ifdef USE_PRETTYSRC
    if (!dump_output_immediately) {
	HTMLSRC_init_caches(FALSE); /* do it before terminal is initialized*/
#ifdef LY_FIND_LEAKS
	atexit(html_src_clean_data);
#endif
    }
#endif

    if (!dump_output_immediately) {
	setup(terminal);
    }
    /*
     *	If startfile is a file URL and the host is defaulted,
     *	force in "//localhost", and if it's not an absolute URL,
     *	make it one. - FM
     */
    LYEnsureAbsoluteURL((char **)&startfile, "STARTFILE", FALSE);

    /*
     *	If homepage was specified and is a file URL with the
     *	host defaulted, force in "//localhost", and if it's
     *	not an absolute URL, make it one. - FM
     */
    if (homepage) {
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
	telnet_ok = (BOOL)(!no_inside_telnet && !no_outside_telnet && telnet_ok);
#ifndef DISABLE_NEWS
	news_ok = (BOOL)(!no_inside_news && !no_outside_news && news_ok);
#endif
	ftp_ok = (BOOL)(!no_inside_ftp && !no_outside_ftp && ftp_ok);
	rlogin_ok = (BOOL)(!no_inside_rlogin && !no_outside_rlogin && rlogin_ok);
#else
	CTRACE((tfp, "LYMain: User in Local domain\n"));
	telnet_ok = (BOOL)(!no_inside_telnet && telnet_ok);
#ifndef DISABLE_NEWS
	news_ok = (BOOL)(!no_inside_news && news_ok);
#endif
	ftp_ok = (BOOL)(!no_inside_ftp && ftp_ok);
	rlogin_ok = (BOOL)(!no_inside_rlogin && rlogin_ok);
#endif /* !HAVE_UTMP || VMS */
    } else {
	CTRACE((tfp, "LYMain: User in REMOTE domain\n"));
	telnet_ok = (BOOL)(!no_outside_telnet && telnet_ok);
#ifndef DISABLE_NEWS
	news_ok = (BOOL)(!no_outside_news && news_ok);
#endif
	ftp_ok = (BOOL)(!no_outside_ftp && ftp_ok);
	rlogin_ok = (BOOL)(!no_outside_rlogin && rlogin_ok);
    }

    /*
     *	Make sure our bookmark default strings
     *	are all allocated and synchronized. - FM
     */
    if (!bookmark_page || *bookmark_page == '\0') {
	temp = NULL;
	HTSprintf0(&temp, "lynx_bookmarks%s", HTML_SUFFIX);
	set_default_bookmark_page(temp);
	FREE(temp);
    }
    if (!BookmarkPage || *BookmarkPage == '\0') {
	set_default_bookmark_page(bookmark_page);
    }

#if !defined(VMS) && defined(SYSLOG_REQUESTED_URLS)
    LYOpenlog (syslog_txt);
#endif

    /*
     *	Here's where we do all the work.
     */
    if (dump_output_immediately) {
	/*
	 *  Finish setting up and start a
	 *  NON-INTERACTIVE session. - FM
	 */
	if (crawl && !number_links && !number_fields) {
	    keypad_mode = NUMBERS_AS_ARROWS;
	} else if (!nolist) {
	    if (!links_are_numbered()) {
		if (number_fields)
		    keypad_mode = LINKS_AND_FIELDS_ARE_NUMBERED;
		else
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
	    !crawl &&		/* For -crawl it has already been done! */
	    links_are_numbered())
	    printlist(stdout, FALSE);
#ifdef USE_PERSISTENT_COOKIES
	/*
	 *  We want to save cookies picked up when in immediate dump
	 *  mode.  Instead of calling cleanup() here, let's only call
	 *  this one. - BJP
	 */
	if (persistent_cookies)
	    LYStoreCookies(LYCookieSaveFile);
#endif /* USE_PERSISTENT_COOKIES */
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

#ifndef NO_DUMP_WITH_BACKSPACES
	if (with_backspaces) {
	    /* we should warn about this somehow (nop for now) -VH */
	    with_backspaces = FALSE;
	}
#endif

#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN
	init_charset_subsets();
#endif

	ena_csi((BOOLEAN)(LYlowest_eightbit[current_char_set] > 155));
	status = mainloop();
	LYCloseCloset(RECALL_URL);
	LYCloseCloset(RECALL_MAIL);
#if defined(PDCURSES) && defined(PDC_BUILD) && PDC_BUILD >= 2401
	if (! isendwin()) {
	    extern int saved_scrsize_x;
	    extern int saved_scrsize_y;
	    if ((saved_scrsize_x != 0) && (saved_scrsize_y != 0)) {
	        resize_term(saved_scrsize_y, saved_scrsize_x);
	    }
	}
#endif
	cleanup();
	exit(status);
    }

    return(status);	/* though redundant, for compiler-warnings */
}

/*
 *  Called by HTAccessInit to register any protocols supported by lynx.
 *  Protocols added by lynx:
 *    LYNXKEYMAP, lynxcgi, LYNXIMGMAP, LYNXCOOKIE, LYNXMESSAGES
 */
#ifdef GLOBALREF_IS_MACRO
extern GLOBALREF (HTProtocol, LYLynxKeymap);
extern GLOBALREF (HTProtocol, LYLynxCGI);
extern GLOBALREF (HTProtocol, LYLynxIMGmap);
extern GLOBALREF (HTProtocol, LYLynxCookies);
extern GLOBALREF (HTProtocol, LYLynxStatusMessages);
#else
GLOBALREF  HTProtocol LYLynxKeymap;
GLOBALREF  HTProtocol LYLynxCGI;
GLOBALREF  HTProtocol LYLynxIMGmap;
GLOBALREF  HTProtocol LYLynxCookies;
GLOBALREF  HTProtocol LYLynxStatusMessages;
#endif /* GLOBALREF_IS_MACRO */

PUBLIC void LYRegisterLynxProtocols NOARGS
{
    HTRegisterProtocol(&LYLynxKeymap);
    HTRegisterProtocol(&LYLynxCGI);
    HTRegisterProtocol(&LYLynxIMGmap);
    HTRegisterProtocol(&LYLynxCookies);
    HTRegisterProtocol(&LYLynxStatusMessages);
}

#ifndef NO_CONFIG_INFO
/*
 *  Some stuff to reload lynx.cfg without restarting new lynx session,
 *  also load options menu items and command-line options
 *  to make things consistent.
 *
 *  Called by user of interactive session by LYNXCFG://reload/ link.
 *
 *  Warning: experimental, more main() reorganization required.
 *	*Known* exceptions: persistent cookies, cookie files.
 *
 *	Some aspects of COLOR (with slang?).
 *	Viewer stuff, mailcap files
 *	SUFFIX, mime.types files
 *	RULESFILE/RULE
 *
 *	All work "somewhat", but not exactly as the first time.
 */
PUBLIC void reload_read_cfg NOARGS
{
    char *tempfile;
    FILE *rcfp;
    /*
     *  no_option_save is always set for -anonymous and -validate.
     *  It is better to check for one or several specific restriction
     *  flags than for 'LYRestricted', which doesn't get set for
     *  individual restrictions or for -validate!
     *  However, no_option_save may not be the appropriate one to
     *  check - in that case, a new no_something should be added
     *  that gets automatically set for -anonymous and -validate
     *  (and whether it applies for -anonymous can be made installer-
     *  configurable in the usual way at the bottom of userdefs.h). - kw
     *
     */
    if (no_option_save) {
	/* current logic requires(?) that saving user preferences is
	   possible.  Additional applicable restrictions are already
	   checked by caller. - kw */
	return;
    }

    /*
     *  Current user preferences are saved in a temporary file, to be
     *  read in again after lynx.cfg has been read.  This avoids
     *  accidental changing of the preferences file.  The regular
     *  preferences file doesn't even need to exist, and won't be
     *  created as a side effect of this function.  Honoring the
     *  no_option_save restriction may thus be unnecessarily restrictive,
     *  but the check is currently still left in place. - kw
     */
    tempfile = typecallocn(char, LY_MAXPATH);
    if (!tempfile) {
	HTAlwaysAlert(NULL, NOT_ENOUGH_MEMORY);
	return;
    }
    rcfp = LYOpenTemp(tempfile, ".rc" , "w");
    if (rcfp == NULL) {
	FREE(tempfile);
	HTAlwaysAlert(NULL, CANNOT_OPEN_TEMP);
	return;
    }
    if (!save_rc(rcfp)) {
	HTAlwaysAlert(NULL, OPTIONS_NOT_SAVED);
	LYRemoveTemp(tempfile);
	FREE(tempfile);
	return;    /* can not write the very own file :( */
    }

    {
	/* set few safe flags: */
#ifdef USE_PERSISTENT_COOKIES
	BOOLEAN persistent_cookies_flag = persistent_cookies;
	char * LYCookieFile_flag = NULL;
	char * LYCookieSaveFile_flag = NULL;
	if (persistent_cookies) {
	    StrAllocCopy(LYCookieFile_flag, LYCookieFile);
	    StrAllocCopy(LYCookieSaveFile_flag, LYCookieSaveFile);
	}
#endif

#ifdef EXP_CHARSET_CHOICE
	custom_assumed_doc_charset = FALSE;
	custom_display_charset = FALSE;
	memset((char*)charset_subsets, 0, sizeof(charset_subset_t)*MAXCHARSETS);
#endif

#ifdef USE_PRETTYSRC
	html_src_on_lynxcfg_reload();
#endif
	/* free downloaders, printers, environments, dired menu */
	free_lynx_cfg();
#ifdef USE_SOURCE_CACHE
	source_cache_file_error = FALSE; /* reset flag */
#endif

	/*
	 *  Process the configuration file.
	 */
	read_cfg(lynx_cfg_file, "main program", 1, (FILE *)0);

	/*
	 *  Process the temporary RC file.
	 */
	rcfp = fopen(tempfile, "r");
	read_rc(rcfp);
	LYRemoveTemp(tempfile);
	FREE(tempfile);		/* done with it - kw */

#ifdef EXP_CHARSET_CHOICE
	init_charset_subsets();
#endif

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
	if (user_mode == NOVICE_MODE) {
	    display_lines = LYlines - 4;
	} else {
	    display_lines = LYlines - 2;
	}
		/* Not implemented yet here,
		 * a major problem: file paths
		 * like lynx_save_space, LYCookieFile etc.
		 */
#ifdef USE_PERSISTENT_COOKIES
	/* restore old settings */
	if (persistent_cookies != persistent_cookies_flag) {
	    persistent_cookies = persistent_cookies_flag;
	    HTAlert(gettext("persistent cookies state will be changed in next session only."));
	}
	if (persistent_cookies) {
	    if (strcmp(LYCookieFile, LYCookieFile_flag)) {
		StrAllocCopy(LYCookieFile, LYCookieFile_flag);
		CTRACE((tfp, "cookie file can be changed in next session only, restored.\n"));
	    }
	    if (strcmp(LYCookieSaveFile, LYCookieSaveFile_flag)) {
		StrAllocCopy(LYCookieSaveFile, LYCookieSaveFile_flag);
		CTRACE((tfp, "cookie save file can be changed in next session only, restored.\n"));
	    }
	    FREE(LYCookieFile_flag);
	    FREE(LYCookieSaveFile_flag);
	}
#endif

    }
}
#endif /* !NO_CONFIG_INFO */

PRIVATE void disable_pausing NOARGS
{
    AlertSecs = 0;
    DebugSecs = 0;
    InfoSecs = 0;
    MessageSecs = 0;
    ReplaySecs = 0;
}

PRIVATE void force_dump_mode NOARGS
{
    dump_output_immediately = TRUE;
    disable_pausing();
    LYcols = DFT_COLS;
}

/* There are different ways of setting arguments on the command line, and
 * there are different types of arguments.  These include:
 *
 *   -set_some_variable		 ==> some_variable  = TRUE
 *   -toggle_some_variable	 ==> some_variable = !some_variable
 *   -some_variable=value	 ==> some_variable = value
 *
 * Others are complicated and require a function call.
 */

#define PARSE_SET(n,t,v,h) {n,    t, UNION_SET(v), h}
#define PARSE_INT(n,t,v,h) {n,    t, UNION_INT(v), h}
#define PARSE_STR(n,t,v,h) {n,    t, UNION_STR(v), h}
#define PARSE_FUN(n,t,v,h) {n,    t, UNION_FUN(v), h}
#define PARSE_NIL          {NULL, 0, UNION_DEF(0), NULL}

typedef struct parse_args_type
{
   CONST char *name;
   int type;

#define TOGGLE_ARG		0x0010
#define SET_ARG			0x0020
#define UNSET_ARG		0x0030
#define FUNCTION_ARG		0x0040
#define LYSTRING_ARG		0x0050
#define INT_ARG			0x0060
#define STRING_ARG		0x0070
#define TIME_ARG		0x0080
#define ARG_TYPE_MASK		0x0FF0
#define NEED_NEXT_ARG		0x1000

#define NEED_INT_ARG		(NEED_NEXT_ARG | INT_ARG)
#define NEED_TIME_ARG		(NEED_NEXT_ARG | TIME_ARG)
#define NEED_LYSTRING_ARG	(NEED_NEXT_ARG | LYSTRING_ARG)
#define NEED_STRING_ARG		(NEED_NEXT_ARG | STRING_ARG)
#define NEED_FUNCTION_ARG	(NEED_NEXT_ARG | FUNCTION_ARG)

   /* If the NEED_NEXT_ARG flags is set, and the option was not specified
    * with an '=' character, then use the next argument in the argv list.
    */

   ParseData;
   CONST char *help_string;
}
Config_Type;

/* -auth, -pauth */
PRIVATE int parse_authentication ARGS2(
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
PRIVATE int anonymous_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    if (!LYValidate && !LYRestricted)
	parse_restrictions("default");
    LYRestricted = TRUE;
    return 0;
}

/* -assume_charset */
PRIVATE int assume_charset_fun ARGS1(
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
PRIVATE int assume_local_charset_fun ARGS1(
	char *,			next_arg)
{
    UCLYhndl_HTFile_for_unspec = safeUCGetLYhndl_byMIME(next_arg);
    return 0;
}

/* -assume_unrec_charset */
PRIVATE int assume_unrec_charset_fun ARGS1(
	char *,			next_arg)
{
    UCLYhndl_for_unrec = safeUCGetLYhndl_byMIME(next_arg);
    return 0;
}

/* -auth */
PRIVATE int auth_fun ARGS1(
	char *,			next_arg)
{
    parse_authentication(next_arg, authentication_info);
    return 0;
}

/* -base */
PRIVATE int base_fun ARGS1(
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

/* -cache */
PRIVATE int cache_fun ARGS1(
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
PRIVATE int child_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    child_lynx = TRUE;
    no_disk_save = TRUE;
    return 0;
}

#ifdef USE_SLANG
/* -color */
PRIVATE int color_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    Lynx_Color_Flags |= SL_LYNX_USE_COLOR;

    if (LYShowColor != SHOW_COLOR_ALWAYS)
	LYShowColor = SHOW_COLOR_ON;

    return 0;
}
#endif

#ifdef MISC_EXP
/* -convert_to */
PRIVATE int convert_to_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0) {
	char *outformat = NULL;
	char *cp1, *cp2, *cp4;
	int chndl;
	StrAllocCopy(outformat, next_arg);
	/* not lowercased, to allow for experimentation - kw */
	/*LYLowerCase(outformat);*/
	if ((cp1 = strchr(outformat, ';')) != NULL) {
	    if ((cp2 = LYstrstr(cp1, "charset")) != NULL) {
		cp2 += 7;
		while (*cp2 == ' ' || *cp2 == '=' || *cp2 == '\"')
		    cp2++;
		for (cp4 = cp2; (*cp4 != '\0' && *cp4 != '\"' &&
				 *cp4 != ';'  &&
				 !WHITE(*cp4));	cp4++)
		    ; /* do nothing */
		*cp4 = '\0';
		/* This is intentionally not the "safe" version,
		   to allow for experimentation. */
		chndl = UCGetLYhndl_byMIME(cp2);
		if (chndl < 0) chndl = UCLYhndl_for_unrec;
		if (chndl < 0) {
		    fprintf(stderr,
		    gettext("Lynx: ignoring unrecognized charset=%s\n"), cp2);
		} else {
		    current_char_set = chndl;
		}
		*cp1 = '\0';	/* truncate outformat */
	    }
	}
	HTOutputFormat = HTAtom_for(outformat);
	FREE(outformat);
    } else {
	HTOutputFormat = NULL;
    }
    return 0;
}
#endif

/* -crawl */
PRIVATE int crawl_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    crawl = TRUE;
    LYcols = DFT_COLS;
    return 0;
}

/* -display */
PRIVATE int display_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0) {
	LYsetXDisplay(next_arg);
    }

    return 0;
}

/* -display_charset */
PRIVATE int display_charset_fun ARGS1(
	char *,			next_arg)
{
    int i = UCGetLYhndl_byMIME(next_arg);

#ifdef CAN_AUTODETECT_DISPLAY_CHARSET
    if (i < 0 && !strcasecomp(next_arg, "auto"))
	i = auto_display_charset;
#endif
    if (i < 0) {	/* do nothing here: so fallback to lynx.cfg */
	fprintf(stderr,
		gettext("Lynx: ignoring unrecognized charset=%s\n"), next_arg);
    }
    else
	current_char_set = i;
    return 0;
}

/* -dump */
PRIVATE int dump_output_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    force_dump_mode();
    return 0;
}

/* -editor */
PRIVATE int editor_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0)
	StrAllocCopy(editor, next_arg);
    system_editor = TRUE;
    return 0;
}

/* -error_file */
PRIVATE int error_file_fun ARGS1(
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
PRIVATE int exec_fun ARGS1(
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
PRIVATE int get_data_fun ARGS1(
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
    force_dump_mode();
#endif /* VMS */

    StrAllocCopy(form_get_data, "?");   /* Prime the pump */
    get_data = &form_get_data;

    /*
     *  Build GET data for later.  Stop reading when we see a line
     *  with "---" as its first three characters.
     */
    while (GetStdin(&buf, TRUE)) {
	StrAllocCat(*get_data, buf);
    }

    return 0;
}

/* -help */
PRIVATE int help_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    print_help_and_exit (0);
    return 0;
}

/* -hiddenlinks */
PRIVATE int hiddenlinks_fun ARGS1(
	char *,			next_arg)
{
    static Config_Enum table[] = {
	{ "merge",	HIDDENLINKS_MERGE },
	{ "listonly",	HIDDENLINKS_SEPARATE },
	{ "ignore",	HIDDENLINKS_IGNORE },
	{ NULL,		-1 },
    };

    if (next_arg != 0) {
	if (!LYgetEnum(table, next_arg, &LYHiddenLinks))
	    print_help_and_exit (-1);
    } else {
	LYHiddenLinks = HIDDENLINKS_MERGE;
    }

    return 0;
}

/* -homepage */
PRIVATE int homepage_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0) {
	StrAllocCopy(homepage, next_arg);
	LYEscapeStartfile(&homepage);
    }
    return 0;
}

/* -mime_header */
PRIVATE int mime_header_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    /*
     *  Include mime headers and force source dump.
     */
    keep_mime_headers = TRUE;
    force_dump_mode();
    HTOutputFormat = (LYPrependBase ?
		      HTAtom_for("www/download") : HTAtom_for("www/dump"));
    LYcols = MAX_COLS;
    return 0;
}

#ifndef DISABLE_NEWS
/* -newschunksize */
PRIVATE int newschunksize_fun ARGS1(
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
PRIVATE int newsmaxchunk_fun ARGS1(
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

/* -nobold */
PRIVATE int nobold_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
   LYnoVideo(1);
   return 0;
}

/* -nobrowse */
PRIVATE int nobrowse_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
   HTDirAccess = HT_DIR_FORBID;
   return 0;
}

/* -nocolor */
PRIVATE int nocolor_fun ARGS1(
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
PRIVATE int nopause_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    disable_pausing();
    return 0;
}

/* -noreverse */
PRIVATE int noreverse_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
   LYnoVideo(2);
   return 0;
}

/* -nounderline */
PRIVATE int nounderline_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
   LYnoVideo(4);
   return 0;
}

#ifdef MISC_EXP
/* -nozap */
PRIVATE int nozap_fun ARGS1(
	char *,			next_arg)
{
    LYNoZapKey = 1; /* everything but "initially" treated as "full" - kw */
    if (next_arg != 0) {
	if (strcasecomp(next_arg, "initially") == 0)
	    LYNoZapKey = 2;

    }
   return 0;
}
#endif /* MISC_EXP */

/* -pauth */
PRIVATE int pauth_fun ARGS1(
	char *,			next_arg)
{
    parse_authentication(next_arg, proxyauth_info);
    return 0;
}

/* -post_data */
PRIVATE int post_data_fun ARGS1(
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
    force_dump_mode();
#endif /* VMS */

    post_data = &form_post_data;

    /*
     * Build post data for later.  Stop reading when we see a line with "---"
     * as its first three characters.
     */
    while (GetStdin(&buf, TRUE)) {
	StrAllocCat(*post_data, buf);
    }
    return 0;
}

PRIVATE char *show_restriction ARGS1(
	CONST char *,		name)
{
    char *value = 0;

    switch (find_restriction(name, -1)) {
    case TRUE:
	value = "on";
	break;
    case FALSE:
	value = "off";
	break;
    default:
	value = "?";
	break;
    }
    return value;
}

/* -restrictions */
PRIVATE int restrictions_fun ARGS1(
	char *,			next_arg)
{
    static CONST struct {
	CONST char *name;
	CONST char *help;
    } table[] = {
	{ "all", "restricts all options." },
	{ "bookmark", "disallow changing the location of the bookmark file" },
	{ "bookmark_exec", "disallow execution links via the bookmark file" },
#if defined(DIRED_SUPPORT) && defined(OK_PERMIT)
	{ "change_exec_perms", "\
disallow changing the eXecute permission on files\n\
(but still allow it for directories) when local file\n\
management is enabled." },
#endif /* DIRED_SUPPORT && OK_PERMIT */
#ifdef SUPPORT_CHDIR
	{ "chdir", "\
disallow changing the working directory of lynx, e.g.,\n\
to affect the behavior of download command" },
#endif
#if defined(HAVE_CONFIG_H) && !defined(NO_CONFIG_INFO)
	{ "compileopts_info", "\
disable info on options used to compile the binary" },
#endif
	{ "default", "\
same as commandline option -anonymous.  Sets the\n\
default service restrictions for anonymous users.  Set to\n\
all restricted, except for: inside_telnet, outside_telnet,\n\
inside_ftp, outside_ftp, inside_rlogin, outside_rlogin,\n\
inside_news, outside_news, telnet_port, jump, mail, print,\n\
exec, and goto.  The settings for these, as well as\n\
additional goto restrictions for specific URL schemes\n\
that are also applied, are derived from definitions\n\
within userdefs.h." },
#ifdef DIRED_SUPPORT
	{ "dired_support", "disallow local file management" },
#endif /* DIRED_SUPPORT */
	{ "disk_save", "disallow saving to disk in the download and print menus" },
	{ "dotfiles", "disallow access to, or creation of, hidden (dot) files" },
	{ "download", "disallow some downloaders in the download menu" },
	{ "editor", "disallow editing" },
	{ "exec", "disable execution scripts" },
	{ "exec_frozen", "disallow the user from changing the execution link option" },
#ifdef USE_EXTERNALS
	{ "externals", "disable passing URLs to some external programs" },
#endif
	{ "file_url", "\
disallow using G)oto, served links or bookmarks for\n\
file: URL's" },
	{ "goto", "disable the 'g' (goto) command" },
#if !defined(HAVE_UTMP) || defined(VMS) /* not selective */
	{ "inside_ftp", "\
disallow ftps coming from inside your\n\
domain (utmp required for selectivity)" },
	{ "inside_news", "\
disallow USENET news reading and posting coming\n\
from inside your domain (utmp required for selectivity)" },
	{ "inside_rlogin", "\
disallow rlogins coming from inside your\n\
domain (utmp required for selectivity)" },
	{ "inside_telnet", "\
disallow telnets coming from inside your\n\
domain (utmp required for selectivity)" },
#else
	{ "inside_ftp", "\
disallow ftps coming from inside your domain" },
	{ "inside_news", "\
disallow USENET news reading and posting coming\n\
from inside your domain" },
	{ "inside_rlogin", "\
disallow rlogins coming from inside your domain" },
	{ "inside_telnet", "\
disallow telnets coming from inside your domain" },
#endif /* HAVE_UTMP || VMS */
	{ "jump", "disable the 'j' (jump) command" },
	{ "lynxcfg_info", "\
disable viewing of lynx.cfg configuration file info" },
#ifndef NO_CONFIG_INFO
	{ "lynxcfg_xinfo", "\
disable extended lynx.cfg viewing and reloading" },
#endif
	{ "lynxcgi", "\
disallow execution of Lynx CGI URLs" },
	{ "mail", "disallow mail" },
	{ "multibook", "disallow multiple bookmark files" },
	{ "news_post", "disallow USENET News posting." },
	{ "option_save", "disallow saving options in .lynxrc" },
#if !defined(HAVE_UTMP) || defined(VMS) /* not selective */
	{ "outside_ftp", "\
disallow ftps coming from outside your\n\
domain (utmp required for selectivity)" },
	{ "outside_news", "\
disallow USENET news reading and posting coming\n\
from outside your domain (utmp required for selectivity)" },
	{ "outside_rlogin", "\
disallow rlogins coming from outside your\n\
domain (utmp required for selectivity)" },
	{ "outside_telnet", "\
disallow telnets coming from outside your\n\
domain (utmp required for selectivity)" },
#else
	{ "outside_ftp", "\
disallow ftp coming from outside your domain" },
	{ "outside_news", "\
disallow USENET news reading and posting coming\n\
from outside your domain" },
	{ "outside_rlogin", "\
disallow rlogins coming from outside your domain" },
	{ "outside_telnet", "\
disallow telnets coming from outside your domain" },
#endif /* !HAVE_UTMP || VMS */
	{ "print", "disallow most print options" },
	{ "shell", "\
disallow shell escapes, and lynxexec, lynxprog or lynxcgi\n\
G)oto's" },
	{ "suspend", "disallow Control-Z suspends with escape to shell" },
	{ "telnet_port", "disallow specifying a port in telnet G)oto's" },
	{ "useragent", "disallow modifications of the User-Agent header" },
    };
    static CONST char *Usage[] = {
 ""
,"USAGE: lynx -restrictions=[option][,option][,option]"
,"List of Options:"
,"  ?                 when used alone, list restrictions in effect."

    };
    unsigned j, k, column = 0;
    CONST char *name;
    CONST char *value;
    BOOLEAN found, first;

    if (isEmpty(next_arg)) {
	SetOutputMode( O_TEXT );
	for (j = 0; j < TABLESIZE(Usage); j++) {
	    printf("%s\n", Usage[j]);
	}
	for (j = 0; j < TABLESIZE(table); j++) {
	    if (!strcmp(table[j].name, "all")
	     || !strcmp(table[j].name, "default")) {
		value = NULL;
	    } else {
		value = show_restriction(table[j].name);
	    }
	    print_help_strings (
		table[j].name, table[j].help, value, FALSE);
	}
	first = TRUE;
	for (j = 0; ; j++) {
	    found = FALSE;
	    if ((name = index_to_restriction(j)) == 0) {
		break;
	    }
	    for (k = 0; k < TABLESIZE(table); k++) {
		if (!strcmp(name, table[k].name)) {
		    found = TRUE;
		}
	    }
	    if (!found) {
		if (first) {
		    printf("Other restrictions (see the user's guide):\n");
		}
		value = show_restriction(table[j].name);
		printf("%s%s (%s)", column ? ", " : "  ", name, value);
		column += 5 + strlen(name) + strlen(value);
		if (column > 50) {
		    column = 0;
		    printf("\n");
		}
		first = FALSE;
	    }
	}
	if (column)
	    printf("\n");
	SetOutputMode( O_BINARY );
	exit(EXIT_SUCCESS);
    } else if (*next_arg == '?') {
	SetOutputMode( O_TEXT );
	print_restrictions_to_fd(stdout);
	SetOutputMode( O_BINARY );
	exit(EXIT_SUCCESS);
    } else {
	parse_restrictions(next_arg);
    }
    return 0;
}

/* -selective */
PRIVATE int selective_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
   HTDirAccess = HT_DIR_SELECTIVE;
   return 0;
}

/* -source */
PRIVATE int source_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    force_dump_mode();
    HTOutputFormat = (LYPrependBase ?
		      HTAtom_for("www/download") : HTAtom_for("www/dump"));
    LYcols = MAX_COLS;
    return 0;
}

/* -traversal */
PRIVATE int traversal_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    traversal = TRUE;
#ifdef USE_SLANG
    LYcols = DFT_COLS;
#else
    LYcols = MAX_COLS;
#endif /* USE_SLANG */

    return 0;
}

/* -version */
PRIVATE int version_fun ARGS1(
	char *,			next_arg GCC_UNUSED)
{
    char *result = NULL;

    SetOutputMode( O_TEXT );

    HTSprintf0(&result, gettext("%s Version %s (%s)"),
	       LYNX_NAME, LYNX_VERSION,
	       LYVersionDate());
#ifdef USE_SSL
    StrAllocCat(result, "\n");
    HTSprintf(&result, "libwww-FM %s,", HTLibraryVersion);
    append_ssl_version(&result, " ");
#endif /* USE_SSL */
    printf("%s\n", result);
    free(result);

#ifndef __DATE__
#define __DATE__ ""
#endif
#ifndef __TIME__
#define __TIME__ ""
#endif

/*
 * SYSTEM_NAME is set by the configure script.  Show build date/time for other
 * systems, according to predefined compiler symbols.
 */
#ifdef __CYGWIN__
    printf("Compiled by CYGWIN (%s %s).\n", __DATE__, __TIME__);
#else
#ifdef __BORLANDC__
    printf("Compiled by Borland C++ (%s %s).\n", __DATE__, __TIME__);
#else
#ifdef _MSC_VER
    printf("Compiled by Microsoft Visual C++ (%s %s).\n", __DATE__, __TIME__);
#else
#ifdef __DJGPP__
    printf("Compiled by DJGPP (%s %s).\n", __DATE__, __TIME__);
#endif /* __DJGPP__ */
#endif /* _MSC_VER */
#endif /* __BORLANDC__ */
#endif /* __CYGWIN__ */

    printf("\n");
    printf(gettext(
	  "Copyrights held by the University of Kansas, CERN, and other contributors.\n"
	  ));
    printf(gettext("Distributed under the GNU General Public License.\n"));
    printf(gettext(
	  "See http://lynx.isc.org/ and the online help for more information.\n\n"
	  ));
#ifdef USE_SSL
    printf("See http://www.moxienet.com/lynx/ for information about SSL for Lynx.\n");
#ifdef OPENSSL_VERSION_TEXT
    printf("See http://www.openssl.org/ for information about OpenSSL.\n");
#endif /* OPENSSL_VERSION_TEXT */
    printf("\n");
#endif /* USE_SSL */

    SetOutputMode( O_BINARY );

    exit(EXIT_SUCCESS);
    /* NOT REACHED */
    return 0;
}

/* -width */
PRIVATE int width_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0) {
	int w = atoi(next_arg);
	if (w > 0)
	    dump_output_width = ((w < MAX_COLS) ? w : MAX_COLS);
    }

    return 0;
}

#if defined(PDCURSES) && defined(PDC_BUILD) && PDC_BUILD >= 2401
/* -scrsize */
PRIVATE int scrsize_fun ARGS1(
	char *,			next_arg)
{
    if (next_arg != 0) {
	char *cp;

	if ((cp = strchr(next_arg, ',')) != 0) {
	    *cp++ = '\0';	/* Terminate ID */
	    scrsize_x = atoi(next_arg);
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
    }
    return 0;
}
#endif

/* NOTE: This table is sorted by name to make the help message useful */
PRIVATE Config_Type Arg_Table [] =
{
   PARSE_SET(
      "accept_all_cookies", 4|SET_ARG,		LYAcceptAllCookies,
      "\naccept cookies without prompting if Set-Cookie handling\nis on"
   ),
   PARSE_FUN(
      "anonymous",	2|FUNCTION_ARG,		anonymous_fun,
      "apply restrictions for anonymous account,\nsee also -restrictions"
   ),
   PARSE_FUN(
      "assume_charset", 4|NEED_FUNCTION_ARG,	assume_charset_fun,
      "=MIMEname\ncharset for documents that don't specify it"
   ),
   PARSE_FUN(
      "assume_local_charset",	4|NEED_FUNCTION_ARG,assume_local_charset_fun,
      "=MIMEname\ncharset assumed for local files"
   ),
   PARSE_FUN(
      "assume_unrec_charset",	4|NEED_FUNCTION_ARG,assume_unrec_charset_fun,
      "=MIMEname\nuse this instead of unrecognized charsets"
   ),
   PARSE_FUN(
      "auth",		4|NEED_FUNCTION_ARG,	auth_fun,
      "=id:pw\nauthentication information for protected documents"
   ),
   PARSE_FUN(
      "base",		4|FUNCTION_ARG,		base_fun,
      "prepend a request URL comment and BASE tag to text/html\noutputs for -source dumps"
   ),
#ifndef DISABLE_BIBP
   PARSE_STR(
      "bibhost",	4|NEED_LYSTRING_ARG,	BibP_bibhost,
      "=URL\nlocal bibp server (default http://bibhost/)"
   ),
#endif
#ifdef USE_BLINK
   PARSE_SET(
      "blink",		4|SET_ARG,		term_blink_is_boldbg,
      "enable bright background via the BLINK terminal attribute"
   ),
#endif
   PARSE_SET(
      "book",		4|SET_ARG,		bookmark_start,
      "use the bookmark page as the startfile"
   ),
   PARSE_SET(
      "buried_news",	4|TOGGLE_ARG,		scan_for_buried_news_references,
      "toggles scanning of news articles for buried references"
   ),
   PARSE_FUN(
      "cache",		4|NEED_FUNCTION_ARG,	cache_fun,
      "=NUMBER\nNUMBER of documents cached in memory"
   ),
   PARSE_SET(
      "case",		4|SET_ARG,		case_sensitive,
      "enable case sensitive user searching"
   ),
   PARSE_SET(
      "center",		4|TOGGLE_ARG,		no_table_center,
      "toggle center alignment in HTML TABLE"
   ),
   PARSE_STR(
      "cfg",		2|NEED_LYSTRING_ARG,	lynx_cfg_file,
      "=FILENAME\nspecifies a lynx.cfg file other than the default"
   ),
   PARSE_FUN(
      "child",		4|FUNCTION_ARG,		child_fun,
      "exit on left-arrow in startfile, and disable save to disk"
   ),
#ifdef EXP_CMD_LOGGING
   PARSE_STR(
       "cmd_log",	2|NEED_LYSTRING_ARG,	lynx_cmd_logfile,
       "=FILENAME\nlog keystroke commands to the given file"
   ),
   PARSE_STR(
       "cmd_script",	2|NEED_LYSTRING_ARG,	lynx_cmd_script,
       "=FILENAME\nread keystroke commands from the given file\n(see -cmd_log)"
   ),
#endif
#ifdef USE_SLANG
   PARSE_FUN(
      "color",		4|FUNCTION_ARG,		color_fun,
      "force color mode on with standard bg colors"
   ),
#endif
   PARSE_INT(
      "connect_timeout", 4|NEED_INT_ARG,	connect_timeout,
      "=N\nset the N-second connection timeout"
   ),
#ifdef MISC_EXP
   PARSE_FUN(
      "convert_to",	4|FUNCTION_ARG,		convert_to_fun,
      "=FORMAT\nconvert input, FORMAT is in MIME type notation\n(experimental)"
   ),
#endif
#ifdef USE_PERSISTENT_COOKIES
   PARSE_STR(
      "cookie_file",	4|LYSTRING_ARG,		LYCookieFile,
      "=FILENAME\nspecifies a file to use to read cookies"
   ),
   PARSE_STR(
      "cookie_save_file",	4|LYSTRING_ARG,	LYCookieSaveFile,
      "=FILENAME\nspecifies a file to use to store cookies"
   ),
#endif /* USE_PERSISTENT_COOKIES */
   PARSE_SET(
      "cookies",	4|TOGGLE_ARG,		LYSetCookies,
      "toggles handling of Set-Cookie headers"
   ),
#ifndef VMS
   PARSE_SET(
      "core",		4|TOGGLE_ARG,		LYNoCore,
      "toggles forced core dumps on fatal errors"
   ),
#endif
   PARSE_FUN(
      "crawl",		4|FUNCTION_ARG,		crawl_fun,
      "with -traversal, output each page to a file\n\
with -dump, format output as with -traversal, but to stdout"
   ),
#ifdef USE_CURSES_PADS
   PARSE_SET(
      "curses_pads",	4|TOGGLE_ARG,		LYuseCursesPads,
      "uses curses pad feature to support left/right shifting"
   ),
#endif
#ifdef DISP_PARTIAL
   PARSE_SET(
      "debug_partial",	4|TOGGLE_ARG,		debug_display_partial,
      "incremental display stages with MessageSecs delay"
   ),
#endif
   PARSE_INT(
      "delay",		4|NEED_TIME_ARG,	DebugSecs,
      "=NNN\nset NNN-second delay at statusline message"
   ),
   PARSE_FUN(
      "display",	4|NEED_FUNCTION_ARG,	display_fun,
      "=DISPLAY\nset the display variable for X exec'ed programs"
   ),
   PARSE_FUN(
      "display_charset", 4|NEED_FUNCTION_ARG,	display_charset_fun,
      "=MIMEname\ncharset for the terminal output"
   ),
   PARSE_SET(
      "dont_wrap_pre",	4|SET_ARG,		dont_wrap_pre,
      "inhibit wrapping of text in <pre> when -dump'ing and\n\
-crawl'ing, mark wrapped lines in interactive session"
   ),
   PARSE_FUN(
      "dump",		4|FUNCTION_ARG,		dump_output_fun,
      "dump the first file to stdout and exit"
   ),
   PARSE_FUN(
      "editor",		4|NEED_FUNCTION_ARG,	editor_fun,
      "=EDITOR\nenable edit mode with specified editor"
   ),
   PARSE_SET(
      "emacskeys",	4|SET_ARG,		emacs_keys,
      "enable emacs-like key movement"
   ),
   PARSE_SET(
      "enable_scrollback", 4|TOGGLE_ARG,	enable_scrollback,
      "\ntoggles compatibility with comm programs' scrollback\n\
keys (may be incompatible with some curses packages)"
   ),
   PARSE_FUN(
      "error_file",	4|NEED_FUNCTION_ARG,	error_file_fun,
      "=FILE\nwrite the HTTP status code here"
   ),
#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
#ifndef NEVER_ALLOW_REMOTE_EXEC
   PARSE_FUN(
      "exec",		4|FUNCTION_ARG,		exec_fun,
      "enable local program execution"
   ),
#endif
#endif /* EXEC_LINKS || EXEC_SCRIPTS */
#ifdef VMS
   PARSE_SET(
      "fileversions",	4|SET_ARG,		HTVMSFileVersions,
      "include all versions of files in local VMS directory\nlistings"
   ),
#endif
#ifdef LY_FIND_LEAKS
   PARSE_SET(
      "find_leaks",	4|TOGGLE_ARG,		LYfind_leaks,
      "toggles memory-leak checking"
   ),
#endif
   PARSE_SET(
      "force_empty_hrefless_a",	4|SET_ARG,	force_empty_hrefless_a,
      "\nforce HREF-less 'A' elements to be empty (close them as\nsoon as they are seen)"
   ),
   PARSE_SET(
      "force_html",	4|SET_ARG,		LYforce_HTML_mode,
      "forces the first document to be interpreted as HTML"
   ),
   PARSE_SET(
      "force_secure",	4|TOGGLE_ARG,		LYForceSSLCookiesSecure,
      "toggles forcing of the secure flag for SSL cookies"
   ),
#if !defined(NO_OPTION_FORMS) && !defined(NO_OPTION_MENU)
   PARSE_SET(
      "forms_options",	4|TOGGLE_ARG,		LYUseFormsOptions,
      "toggles forms-based vs old-style options menu"
   ),
#endif
   PARSE_SET(
      "from",		4|TOGGLE_ARG,		LYNoFromHeader,
      "toggle transmission of From headers"
   ),
   PARSE_SET(
      "ftp",		4|UNSET_ARG,		ftp_ok,
      "disable ftp access"
   ),
   PARSE_FUN(
      "get_data",	6|FUNCTION_ARG,		get_data_fun,
      "user data for get forms, read from stdin,\nterminated by '---' on a line"
   ),
   PARSE_SET(
      "head",		4|SET_ARG,		HEAD_request,
      "send a HEAD request"
   ),
   PARSE_FUN(
      "help",		5|FUNCTION_ARG,		help_fun,
      "print this usage message"
   ),
   PARSE_FUN(
      "hiddenlinks",	4|NEED_FUNCTION_ARG,	hiddenlinks_fun,
      "=[option]\nhidden links: options are merge, listonly, or ignore"
   ),
   PARSE_SET(
      "historical",	4|TOGGLE_ARG,		historical_comments,
      "toggles use of '>' or '-->' as terminator for comments"
   ),
   PARSE_FUN(
      "homepage",	4|NEED_FUNCTION_ARG,	homepage_fun,
      "=URL\nset homepage separate from start page"
   ),
   PARSE_SET(
      "image_links",	4|TOGGLE_ARG,		clickable_images,
      "toggles inclusion of links for all images"
   ),
   PARSE_STR(
      "index",		4|NEED_LYSTRING_ARG,	indexfile,
      "=URL\nset the default index file to URL"
   ),
   PARSE_SET(
      "ismap",		4|TOGGLE_ARG,		LYNoISMAPifUSEMAP,
      "toggles inclusion of ISMAP links when client-side\nMAPs are present"
   ),
#ifdef EXP_JUSTIFY_ELTS
   PARSE_SET(
      "justify",	4|SET_ARG,		ok_justify,
      "do justification of text"
   ),
#endif
   PARSE_INT(
      "link",		4|NEED_INT_ARG,		ccount,
      "=NUMBER\nstarting count for lnk#.dat files produced by -crawl"
   ),
   PARSE_SET(
      "localhost",	4|SET_ARG,		local_host_only,
      "disable URLs that point to remote hosts"
   ),
#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
   PARSE_SET(
      "locexec",	4|SET_ARG,		local_exec_on_local_files,
      "enable local program execution from local files only"
   ),
#endif /* EXEC_LINKS || EXEC_SCRIPTS */
#if defined(USE_COLOR_STYLE)
   PARSE_STR(
      "lss",		2|NEED_LYSTRING_ARG,	lynx_lss_file,
      "=FILENAME\nspecifies a lynx.lss file other than the default"
   ),
#endif
   PARSE_FUN(
      "mime_header",	4|FUNCTION_ARG,		mime_header_fun,
      "include mime headers and force source dump"
   ),
   PARSE_SET(
      "minimal",	4|TOGGLE_ARG,		minimal_comments,
      "toggles minimal versus valid comment parsing"
   ),
#ifdef EXP_NESTED_TABLES
   PARSE_SET(
      "nested_tables",	4|TOGGLE_ARG,		nested_tables,
      "toggles nested-tables logic"
   ),
#endif
#ifndef DISABLE_NEWS
   PARSE_FUN(
      "newschunksize",	4|NEED_FUNCTION_ARG,	newschunksize_fun,
      "=NUMBER\nnumber of articles in chunked news listings"
   ),
   PARSE_FUN(
      "newsmaxchunk",	4|NEED_FUNCTION_ARG,	newsmaxchunk_fun,
      "=NUMBER\nmaximum news articles in listings before chunking"
   ),
#endif
#if USE_BLAT_MAILER
   PARSE_SET(
      "noblat",		4|TOGGLE_ARG,		mail_is_blat,
      "select mail tool (`BLAT' ==> `sendmail')"
   ),
#endif
   PARSE_FUN(
      "nobold",		4|FUNCTION_ARG,		nobold_fun,
      "disable bold video-attribute"
   ),
   PARSE_FUN(
      "nobrowse",	4|FUNCTION_ARG,		nobrowse_fun,
      "disable directory browsing"
   ),
   PARSE_SET(
      "nocc",		4|SET_ARG,		LYNoCc,
      "disable Cc: prompts for self copies of mailings"
   ),
   PARSE_FUN(
      "nocolor",	4|FUNCTION_ARG,		nocolor_fun,
      "turn off color support"
   ),
#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
   PARSE_SET(
      "noexec",		4|UNSET_ARG,		local_exec,
      "disable local program execution (DEFAULT)"
   ),
#endif /* EXEC_LINKS || EXEC_SCRIPTS */
   PARSE_SET(
      "nofilereferer",	4|SET_ARG,		no_filereferer,
      "disable transmission of Referer headers for file URLs"
   ),
   PARSE_SET(
      "nolist",		4|SET_ARG,		nolist,
      "disable the link list feature in dumps"
   ),
   PARSE_SET(
      "nolog",		4|UNSET_ARG,		error_logging,
      "disable mailing of error messages to document owners"
   ),
#if defined(HAVE_SIGACTION) && defined(SIGWINCH)
   PARSE_SET(
      "nonrestarting_sigwinch", 4|SET_ARG,	LYNonRestartingSIGWINCH,
      "\nmake window size change handler non-restarting"
   ),
#endif /* HAVE_SIGACTION */
   PARSE_FUN(
      "nopause",	4|FUNCTION_ARG,		nopause_fun,
      "disable forced pauses for statusline messages"
   ),
   PARSE_SET(
      "noprint",	4|SET_ARG,		no_print,
      "disable some print functions, like -restrictions=print"
   ),
   PARSE_SET(
      "noredir",	4|SET_ARG,		no_url_redirection,
      "don't follow Location: redirection"
   ),
   PARSE_SET(
      "noreferer",	4|SET_ARG,		LYNoRefererHeader,
      "disable transmission of Referer headers"
   ),
   PARSE_FUN(
      "noreverse",	4|FUNCTION_ARG,		noreverse_fun,
      "disable reverse video-attribute"
   ),
#ifdef SOCKS
   PARSE_SET(
      "nosocks",	6|UNSET_ARG,		socks_flag,
      "don't use SOCKS proxy for this session"
   ),
#endif
   PARSE_SET(
      "nostatus",	4|SET_ARG,		no_statusline,
      "disable the miscellaneous information messages"
   ),
   PARSE_FUN(
      "nounderline",	4|FUNCTION_ARG,		nounderline_fun,
      "disable underline video-attribute"
   ),
#ifdef MISC_EXP
   PARSE_FUN(
      "nozap",		4|FUNCTION_ARG,		nozap_fun,
      "=DURATION (\"initially\" or \"full\") disable checks for 'z' key"
   ),
#endif
   PARSE_SET(
      "number_fields",	4|SET_ARG,		number_fields,
      "force numbering of links as well as form input fields"
   ),
   PARSE_SET(
      "number_links",	4|SET_ARG,		number_links,
      "force numbering of links"
   ),
#ifdef DISP_PARTIAL
   PARSE_SET(
      "partial",	4|TOGGLE_ARG,		display_partial_flag,
      "toggles display partial pages while downloading"
   ),
   PARSE_INT(
      "partial_thres",	4|NEED_INT_ARG,		partial_threshold,
      "[=NUMBER]\nnumber of lines to render before repainting display\n\
with partial-display logic"
   ),
#endif
   PARSE_FUN(
      "pauth",		4|NEED_FUNCTION_ARG,	pauth_fun,
      "=id:pw\nauthentication information for protected proxy server"
   ),
   PARSE_SET(
      "popup",		4|UNSET_ARG,		LYUseDefSelPop,
      "toggles handling of single-choice SELECT options via\npopup windows or as lists of radio buttons"
   ),
   PARSE_FUN(
      "post_data",	6|FUNCTION_ARG,		post_data_fun,
      "user data for post forms, read from stdin,\nterminated by '---' on a line"
   ),
   PARSE_SET(
      "preparsed",	4|SET_ARG,		LYPreparsedSource,
      "show parsed text/html with -source and in source view\n\
to visualize how lynx behaves with invalid HTML"
   ),
#ifdef USE_PRETTYSRC
   PARSE_SET(
      "prettysrc",	4|SET_ARG,		LYpsrc,
      "do syntax highlighting and hyperlink handling in source\nview"
   ),
#endif
   PARSE_SET(
      "print",		4|UNSET_ARG,		no_print,
      "enable print functions (DEFAULT), opposite of -noprint"
   ),
   PARSE_SET(
      "pseudo_inlines", 4|TOGGLE_ARG,		pseudo_inline_alts,
      "toggles pseudo-ALTs for inlines with no ALT string"
   ),
   PARSE_SET(
      "raw",		4|UNSET_ARG,		LYUseDefaultRawMode,
      "toggles default setting of 8-bit character translations\n\
or CJK mode for the startup character set"
   ),
   PARSE_SET(
      "realm",		4|SET_ARG,		check_realm,
      "restricts access to URLs in the starting realm"
   ),
   PARSE_SET(
      "reload",		4|SET_ARG,		reloading,
      "flushes the cache on a proxy server\n(only the first document affected)"
   ),
   PARSE_FUN(
      "restrictions",	4|FUNCTION_ARG,		restrictions_fun,
      "=[options]\nuse -restrictions to see list"
   ),
   PARSE_SET(
      "resubmit_posts", 4|TOGGLE_ARG,		LYresubmit_posts,
      "toggles forced resubmissions (no-cache) of forms with\n\
method POST when the documents they returned are sought\n\
with the PREV_DOC command or from the History List"
   ),
   PARSE_SET(
      "rlogin",		4|UNSET_ARG,		rlogin_ok,
      "disable rlogins"
   ),
#if defined(PDCURSES) && defined(PDC_BUILD) && PDC_BUILD >= 2401
   PARSE_FUN(
      "scrsize",	4|NEED_FUNCTION_ARG,	scrsize_fun,
      "=width,height\nsize of window"
   ),
#endif
#ifdef USE_SCROLLBAR
   PARSE_SET(
      "scrollbar",	4|TOGGLE_ARG,		LYShowScrollbar,
      "toggles showing scrollbar"
   ),
   PARSE_SET(
      "scrollbar_arrow", 4|TOGGLE_ARG,		LYsb_arrow,
      "toggles showing arrows at ends of the scrollbar"
   ),
#endif
   PARSE_FUN(
      "selective",	4|FUNCTION_ARG,		selective_fun,
      "require .www_browsable files to browse directories"
   ),
   PARSE_SET(
      "short_url",	4|SET_ARG,		long_url_ok,
      "enables examination of beginning and end of long URL in\nstatus line"
   ),
#ifdef SH_EX
   PARSE_SET(
      "show_cfg",	4|SET_ARG,		show_cfg,
      "Show `LYNX.CFG' setting"
   ),
#endif
   PARSE_SET(
      "show_cursor",	4|TOGGLE_ARG,		LYUseDefShoCur,
      "toggles hiding of the cursor in the lower right corner"
   ),
#ifdef USE_READPROGRESS
   PARSE_SET(
      "show_rate",	4|TOGGLE_ARG,		LYShowTransferRate,
      "toggles display of transfer rate"
   ),
#endif
   PARSE_SET(
      "soft_dquotes",	4|TOGGLE_ARG,		soft_dquotes,
      "toggles emulation of the old Netscape and Mosaic bug which\n\
treated '>' as a co-terminator for double-quotes and tags"
   ),
   PARSE_FUN(
      "source",		4|FUNCTION_ARG,		source_fun,
      "dump the source of the first file to stdout and exit"
   ),
   PARSE_SET(
      "stack_dump",	4|SET_ARG,		stack_dump,
      "disable SIGINT cleanup handler"
   ),
   PARSE_SET(
      "startfile_ok",	4|SET_ARG,		startfile_ok,
      "allow non-http startfile and homepage with -validate"
   ),
   PARSE_SET(
      "stdin",		4|SET_ARG,		startfile_stdin,
      "read startfile from standard input"
   ),
#ifndef VMS
#ifdef SYSLOG_REQUESTED_URLS
   PARSE_STR(
      "syslog",		4|NEED_LYSTRING_ARG,	syslog_txt,
      "=text\ninformation for syslog call"
   ),
#endif
#endif
   PARSE_SET(
      "tagsoup",	4|SET_ARG,		DTD_recovery,
      "use TagSoup rather than SortaSGML parser"
   ),
   PARSE_SET(
      "telnet",		4|UNSET_ARG,		telnet_ok,
      "disable telnets"
   ),
   PARSE_STR(
      "term",		4|NEED_STRING_ARG,	terminal,
      "=TERM\nset terminal type to TERM"
   ),
#ifdef _WINDOWS
   PARSE_INT(
      "timeout",	4|INT_ARG,		lynx_timeout,
      "=NUMBER\nset TCP/IP timeout"
   ),
#endif
   PARSE_SET(
      "tlog",		2|TOGGLE_ARG,		LYUseTraceLog,
      "toggles use of a Lynx Trace Log for the current session"
   ),
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
   PARSE_SET(
      "tna",		4|SET_ARG,		textfields_activation_option,
      "turn on \"Textfields Need Activation\" mode"
   ),
#endif
#ifndef NO_LYNX_TRACE
   PARSE_SET(
      "trace",		2|SET_ARG,		WWW_TraceFlag,
      "turns on Lynx trace mode"
   ),
   PARSE_INT(
      "trace_mask",	2|INT_ARG,		WWW_TraceMask,
      "customize Lynx trace mode"
   ),
#endif
   PARSE_FUN(
      "traversal",	4|FUNCTION_ARG,		traversal_fun,
      "traverse all http links derived from startfile"
   ),
   PARSE_SET(
      "trim_input_fields", 2|SET_ARG,		LYtrimInputFields,
      "trim input text/textarea fields in forms"
   ),
   PARSE_SET(
      "underline_links",4|TOGGLE_ARG,		LYUnderlineLinks,
      "toggles use of underline/bold attribute for links"
   ),
   PARSE_SET(
      "underscore",	4|TOGGLE_ARG,		use_underscore,
      "toggles use of _underline_ format in dumps"
   ),
#if defined(USE_MOUSE)
   PARSE_SET(
      "use_mouse",	4|SET_ARG,		LYUseMouse,
      "turn on mouse support"
   ),
#endif
   PARSE_STR(
      "useragent",	4|NEED_LYSTRING_ARG,	LYUserAgent,
      "=Name\nset alternate Lynx User-Agent header"
   ),
   PARSE_SET(
      "validate",	2|SET_ARG,		LYValidate,
      "accept only http URLs (meant for validation)\nimplies more restrictions than -anonymous, but\ngoto is allowed for http and https"
   ),
   PARSE_SET(
      "verbose",	4|TOGGLE_ARG,		verbose_img,
      "toggles [LINK], [IMAGE] and [INLINE] comments \nwith filenames of these images"
   ),
   PARSE_FUN(
      "version",	4|FUNCTION_ARG,		version_fun,
      "print Lynx version information"
   ),
   PARSE_SET(
      "vikeys",		4|SET_ARG,		vi_keys,
      "enable vi-like key movement"
   ),
#ifdef __DJGPP__
   PARSE_SET(
      "wdebug",		4|TOGGLE_ARG,		watt_debug,
      "enables Waterloo tcp/ip packet debug. Prints to watt\ndebugfile"
  ),
#endif /* __DJGPP__ */
   PARSE_FUN(
      "width",		4|NEED_FUNCTION_ARG,	width_fun,
      "=NUMBER\nscreen width for formatting of dumps (default is 80)"
   ),
#ifndef NO_DUMP_WITH_BACKSPACES
   PARSE_SET(
      "with_backspaces", 4|SET_ARG,		with_backspaces,
      "emit backspaces in output if -dumping or -crawling\n(like 'man' does)"
   ),
#endif
   PARSE_NIL
};

PRIVATE void print_help_strings ARGS4(
	CONST char *,	name,
	CONST char *,	help,
	CONST char *,	value,
	BOOLEAN,	option)
{
    int pad;
    int c;
    int first;
    int field_width = 20;

    pad = field_width - (2 + option + (int) strlen (name));

    fprintf (stdout, "  %s%s", option ? "-" : "", name);

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

PRIVATE void print_help_and_exit ARGS1(int, exit_status)
{
    Config_Type *p;

    if (pgm == NULL) pgm = "lynx";

    SetOutputMode( O_TEXT );

    fprintf (stdout, gettext("USAGE: %s [options] [file]\n"), pgm);
    fprintf (stdout, gettext("Options are:\n"));
#ifdef VMS
    print_help_strings("",
"receive the arguments from stdin (enclose\n\
in double-quotes (\"-\") on VMS)", NULL, TRUE);
#else
    print_help_strings("", "receive options and arguments from stdin", NULL, TRUE);
#endif /* VMS */

    for (p = Arg_Table; p->name != 0; p++) {
	char temp[LINESIZE], *value = temp;
	ParseUnionPtr q = ParseUnionOf(p);

	switch (p->type & ARG_TYPE_MASK) {
	    case TOGGLE_ARG:
	    case SET_ARG:
		strcpy(temp, *(q->set_value) ? "on" : "off");
		break;
	    case UNSET_ARG:
		strcpy(temp, *(q->set_value) ? "off" : "on");
		break;
	    case INT_ARG:
		sprintf(temp, "%d", *(q->int_value));
		break;
	    case TIME_ARG:
		sprintf(temp, SECS_FMT, (double) Secs2SECS(*(q->int_value)));
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
	print_help_strings(p->name, p->help_string, value, TRUE);
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
PRIVATE int arg_eqs_parse ARGS3(
	CONST char *,	a,
	char *,		b,
	char **,	c)
{
    int result = -1;

    *c = NULL;
    while (result < 0) {
	if ((*a != *b)
	 || (*a == 0)
	 || (*b == 0)) {
	    if (*a == 0) {
		switch (*b) {
		case '\t':	/* embedded blank when reading stdin */
		case ' ':
		    *c = LYSkipBlanks(b);
		    result = 1;
		    break;
		case '=':
		case ':':
		    *c = b + 1;
		    result = 1;
		    break;
		case '-':
#if OPTNAME_ALLOW_DASHES
		    if (isalpha(UCH(b[1]))) {
			result = 0;
			break;
		    }
#endif
		    /* FALLTHRU */
		case '+':
		    *c = b;
		    result = 1;
		    break;
		case 0:
		    result = 1;
		    break;
		default:
		    result = 0;
		    break;
		}
	    } else {
#if OPTNAME_ALLOW_DASHES
		if (!(*a == '_' && *b == '-'))
#endif
		result = 0;
	    }
	}
	a++;
	b++;
    }
    return result;
}

#define is_true(s)  (*s == '1' || *s == '+' || !strcmp(s, "on"))
#define is_false(s) (*s == '0' || *s == '-' || !strcmp(s, "off"))

PRIVATE BOOL parse_arg ARGS3(
	char **,	argv,
	unsigned,	mask,
	int *,		i)
{
    Config_Type *p;
    char *arg_name;
#if EXTENDED_STARTFILE_RECALL
    static BOOLEAN had_nonoption = FALSE;
#endif

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
#if EXTENDED_STARTFILE_RECALL
	if (had_nonoption && !dump_output_immediately) {
	    HTAddGotoURL(startfile); /* startfile was set by a previous arg */
	}
	had_nonoption = TRUE;
#endif
	StrAllocCopy(startfile, arg_name);
	LYEscapeStartfile(&startfile);
#ifdef _WINDOWS	/* 1998/01/14 (Wed) 20:11:17 */
	HTUnEscape(startfile);
	{
	    char *p;

	    p = startfile;
	    while (*p++) {
		if (*p == '|')
		    *p = ':';
	    }
	}
#endif
	return (BOOL)(i != 0);
    }
#if EXTENDED_OPTION_LOGIC
    if (strcmp(arg_name,"--") == 0) {
	no_options_further = TRUE;
	return TRUE;
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
	return TRUE;

    /* allow GNU-style options with -- prefix*/
    if (*arg_name == '-') ++arg_name;

    CTRACE((tfp, "parse_arg(%s)\n", arg_name));

    p = Arg_Table;
    while (p->name != 0) {
	ParseUnionPtr q = ParseUnionOf(p);
	ParseFunc fun;
	char *next_arg = NULL;
	char *temp_ptr = NULL;

	if ((p->name[0] != *arg_name)
	    || (0 == arg_eqs_parse (p->name, arg_name, &next_arg))) {
	    p++;
	    continue;
	}

	if (p->type & NEED_NEXT_ARG) {
	    if (next_arg == 0) {
		next_arg = argv[1];
		if ((i != 0) && (next_arg != 0))
		    (*i)++;
	    }
	    CTRACE((tfp, "...arg:%s\n", next_arg != 0 ? next_arg : "<null>"));
	}

	/* ignore option if it's not our turn */
	if ((p->type & mask) == 0) {
	    CTRACE((tfp, "...skip (mask %d/%d)\n", mask, p->type & 7));
	    return FALSE;
	}

	switch (p->type & ARG_TYPE_MASK) {
	case TOGGLE_ARG:	/* FALLTHRU */
	case SET_ARG:		/* FALLTHRU */
	case UNSET_ARG:
	     if (q->set_value != 0) {
		 if (next_arg == 0) {
		    switch (p->type & ARG_TYPE_MASK) {
		    case TOGGLE_ARG:
			 *(q->set_value) = (BOOL) !(*(q->set_value));
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
		 *(q->int_value) = strtol (next_arg, &temp_ptr, 0);
	     break;

	case TIME_ARG:
	    if ((q->int_value != 0) && (next_arg != 0)) {
		float ival;
		if (1 == sscanf (next_arg, "%f", &ival)) {
		    *(q->int_value) = (int) SECS2Secs(ival);
		}
	    }
	    break;

	case STRING_ARG:
	    if ((q->str_value != 0) && (next_arg != 0))
		*(q->str_value) = next_arg;
	    break;
	}

	Old_DTD = DTD_recovery;	/* BOOL != int */
	return TRUE;
    }

    if (pgm == 0) pgm = "LYNX";

    fprintf (stderr, gettext("%s: Invalid Option: %s\n"), pgm, argv[0]);
    print_help_and_exit (-1);
    return FALSE;
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
	LYSleepAlert();
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

	if (!(sig == 0 && LYNoCore)) {
	    fprintf(stderr, "\r\n\
Do NOT mail the core file if one was generated.\r\n");
	}
	if (sig != 0) {
	    fprintf(stderr, "\r\n\
Lynx now exiting with signal:  %d\r\n\r\n", sig);
#ifdef WIN_EX	/* 1998/08/09 (Sun) 09:58:25 */
	{
	    char *msg;
	    switch (sig) {
	    case SIGABRT:	msg = "SIGABRT";	break;
	    case SIGFPE:	msg = "SIGFPE";		break;
	    case SIGILL:	msg = "SIGILL";		break;
	    case SIGSEGV:	msg = "SIGSEGV";	break;
	    default:		msg = "Not-def";	break;
	    }
	    fprintf(stderr, "signal code = %s\n", msg);
	}
#endif
	}

	/*
	 *  Exit and possibly dump core.
	 */
	if (LYNoCore) {
	    exit(EXIT_FAILURE);
	}
	abort();

    } else {
	LYOutOfMemory = FALSE;
	printf("\r\n%s\r\n\r\n", MEMORY_EXHAUSTED_ABORT);
	fflush(stdout);

	/*
	 *  Exit without dumping core.
	 */
	exit(EXIT_SUCCESS);
    }
}
#endif /* !VMS */
