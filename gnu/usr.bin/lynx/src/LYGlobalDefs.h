/* global variable definitions */

#ifndef LYGLOBALDEFS_H
#define LYGLOBALDEFS_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif /* HTUTILS_H */

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

/* Of the following definitions, currently unused are and could
   be removed (at least):
   CURRENT_KEYMAP_HELP
*/
#if defined(HAVE_CONFIG_H) && defined(HAVE_LYHELP_H)
#include <LYHelp.h>
#else
#define ALT_EDIT_HELP		"keystrokes/alt_edit_help.html"
#define BASHLIKE_EDIT_HELP	"keystrokes/bashlike_edit_help.html"
#define COOKIE_JAR_HELP		"Lynx_users_guide.html#Cookies"
#define CURRENT_KEYMAP_HELP	"keystrokes/keystroke_help.html"
#define DIRED_MENU_HELP		"keystrokes/dired_help.html"
#define DOWNLOAD_OPTIONS_HELP	"Lynx_users_guide.html#RemoteSource"
#define EDIT_HELP		"keystrokes/edit_help.html"
#define HISTORY_PAGE_HELP	"keystrokes/history_help.html"
#define LIST_PAGE_HELP		"keystrokes/follow_help.html"
#define LYNXCFG_HELP		"lynx.cfg"
#define OPTIONS_HELP		"keystrokes/option_help.html"
#define PRINT_OPTIONS_HELP	"keystrokes/print_help.html"
#define UPLOAD_OPTIONS_HELP	"Lynx_users_guide.html#DirEd"
#define VISITED_LINKS_HELP	"keystrokes/visited_help.html"
#endif /* LYHELP_H */

#ifdef USE_SOURCE_CACHE
#include <HTChunk.h>
#endif

#include <LYMail.h>		/* to get ifdef's for mail-variables */

#ifdef SOCKS
extern BOOLEAN socks_flag;
extern unsigned long socks_bind_remoteAddr;
#endif /* SOCKS */

#ifdef IGNORE_CTRL_C
extern BOOLEAN sigint;
#endif /* IGNORE_CTRL_C */

#if USE_VMS_MAILER
extern char *mail_adrs;
extern BOOLEAN UseFixedRecords; /* convert binary files to FIXED 512 records */
#endif /* VMS */

#ifndef VMS
extern char *list_format;
#endif /* !VMS */

#ifdef DIRED_SUPPORT

typedef enum {
    DIRS_FIRST = 0
    , FILES_FIRST
    , MIXED_STYLE
} enumDirListStyle;

typedef enum {
    ORDER_BY_NAME
    , ORDER_BY_SIZE
    , ORDER_BY_DATE
    , ORDER_BY_MODE
    , ORDER_BY_TYPE
    , ORDER_BY_USER
    , ORDER_BY_GROUP
} enumDirListOrder;

extern BOOLEAN lynx_edit_mode;
extern BOOLEAN no_dired_support;
extern HTList *tagged;
extern int LYAutoUncacheDirLists;
extern int dir_list_style;	/* enumDirListStyle */
extern int dir_list_order;	/* enumDirListOrder */

#ifdef OK_OVERRIDE
extern BOOLEAN prev_lynx_edit_mode;
#endif /* OK_OVERRIDE */

#ifdef OK_PERMIT
extern BOOLEAN no_change_exec_perms;
#endif /* OK_PERMIT */

#endif /* DIRED_SUPPORT */

extern int HTCacheSize;  /* the number of documents cached in memory */
#if defined(VMS) && defined(VAXC) && !defined(__DECC)
extern int HTVirtualMemorySize; /* bytes allocated and not yet freed  */
#endif /* VMS && VAXC && !__DECC */

#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
extern BOOLEAN local_exec;  /* TRUE to enable local program execution */
extern BOOLEAN local_exec_on_local_files; /* TRUE to enable local program  *
					   * execution in local files only */
#endif /* defined(EXEC_LINKS) || defined(EXEC_SCRIPTS) */

#if defined(LYNXCGI_LINKS) && !defined(VMS)  /* WebSter Mods -jkt */
extern char *LYCgiDocumentRoot;  /* DOCUMENT_ROOT in the lynxcgi env */
#endif /* LYNXCGI_LINKS */

/* Values to which keypad_mode can be set */
#define NUMBERS_AS_ARROWS 0
#define LINKS_ARE_NUMBERED 1
#define LINKS_AND_FIELDS_ARE_NUMBERED 2
#define FIELDS_ARE_NUMBERED 3

#define links_are_numbered() \
	    (keypad_mode == LINKS_ARE_NUMBERED || \
	     keypad_mode == LINKS_AND_FIELDS_ARE_NUMBERED)

#define fields_are_numbered() \
	    (keypad_mode == FIELDS_ARE_NUMBERED || \
	     keypad_mode == LINKS_AND_FIELDS_ARE_NUMBERED)

#define HIDDENLINKS_MERGE	0
#define HIDDENLINKS_SEPARATE	1
#define HIDDENLINKS_IGNORE	2

#define NOVICE_MODE 	  0
#define INTERMEDIATE_MODE 1
#define ADVANCED_MODE 	  2
extern BOOLEAN LYUseNoviceLineTwo;  /* True if TOGGLE_HELP is not mapped */

#define MAX_LINE 1024	/* Hope that no window is larger than this */
#define MAX_COLS 999	/* we don't expect wider than this */
#define DFT_COLS 80	/* ...and normally only this */
#define DFT_ROWS 24	/* ...corresponding nominal height */

extern char star_string[MAX_LINE + 1]; /* from GridText.c */
#define STARS(n) \
 ((n) >= MAX_LINE ? star_string : &star_string[(MAX_LINE-1)] - (n))

typedef enum {
    SHOW_COLOR_UNKNOWN = -1
    , SHOW_COLOR_NEVER = 0	/* positive numbers are index in LYOptions.c */
    , SHOW_COLOR_OFF
    , SHOW_COLOR_ON
    , SHOW_COLOR_ALWAYS
} enumShowColor;

extern int LYShowColor;		/* Show color or monochrome?	    */
extern int LYrcShowColor;	/* ... as read or last written	    */

typedef enum {
    MBM_OFF = 0
    , MBM_STANDARD
    , MBM_ADVANCED
} enumMultiBookmarks;

#if !defined(NO_OPTION_FORMS) && !defined(NO_OPTION_MENU)
extern BOOLEAN LYUseFormsOptions; /* use Forms-based options menu */
#else
#define LYUseFormsOptions FALSE	/* simplify ifdef'ing in LYMainLoop.c */
#endif

typedef enum {
    rateOFF = 0
    , rateBYTES = 1
    , rateKB
#ifdef USE_READPROGRESS
    , rateEtaBYTES
    , rateEtaKB
#endif
} TransferRate;

#ifdef USE_READPROGRESS
#  define rateEtaKB_maybe	rateEtaKB
#else
#  define rateEtaKB_maybe	rateKB
#endif

extern BOOLEAN LYCursesON;  	/* start_curses()->TRUE, stop_curses()->FALSE */
extern BOOLEAN LYJumpFileURL;   /* URL from the jump file shortcuts? */
extern BOOLEAN LYNewsPosting;	/* News posting supported if TRUE */
extern BOOLEAN LYShowCursor;	/* Show the cursor or hide it?	    */
extern BOOLEAN LYShowTransferRate;
extern BOOLEAN LYUnderlineLinks; /* Show the links underlined vs bold */
extern BOOLEAN LYUseDefShoCur;	/* Command line -show_cursor toggle */
extern BOOLEAN LYUserSpecifiedURL;  /* URL from a goto or document? */
extern BOOLEAN LYfind_leaks;
extern BOOLEAN LYforce_HTML_mode;
extern BOOLEAN LYforce_no_cache;
extern BOOLEAN LYinternal_flag; /* don't need fresh copy, was internal link */
extern BOOLEAN LYoverride_no_cache;  /* don't need fresh copy, from history */
extern BOOLEAN LYresubmit_posts;
extern BOOLEAN LYtrimInputFields;
extern BOOLEAN bold_H1;
extern BOOLEAN bold_headers;
extern BOOLEAN bold_name_anchors;
extern BOOLEAN case_sensitive;    /* TRUE to turn on case sensitive search */
extern BOOLEAN check_mail;        /* TRUE to report unread/new mail messages */
extern BOOLEAN child_lynx;        /* TRUE to exit with an arrow */
extern BOOLEAN dump_output_immediately;
extern BOOLEAN emacs_keys;        /* TRUE to turn on emacs-like key movement */
extern BOOLEAN error_logging;     /* TRUE to mail error messages */
extern BOOLEAN ftp_ok;
extern BOOLEAN ftp_passive;	/* TRUE if we want to use passive mode ftp */
extern BOOLEAN ftp_local_passive;
extern char *ftp_lasthost;
extern BOOLEAN goto_buffer;     /* TRUE if offering default goto URL */
extern BOOLEAN is_www_index;
extern BOOLEAN jump_buffer;     /* TRUE if offering default shortcut */
extern BOOLEAN long_url_ok;
extern BOOLEAN lynx_mode;
extern BOOLEAN more;		/* is there more document to display? */
extern BOOLEAN news_ok;
extern BOOLEAN number_fields_on_left;
extern BOOLEAN number_links_on_left;
extern BOOLEAN recent_sizechange;
extern BOOLEAN rlogin_ok;
extern BOOLEAN system_editor;	  /* True if locked-down editor */
extern BOOLEAN telnet_ok;
extern BOOLEAN verbose_img;	/* display filenames of images?     */
extern BOOLEAN vi_keys;		/* TRUE to turn on vi-like key movement */
extern char *LYRequestReferer;	/* Referer, may be set in getfile() */
extern char *LYRequestTitle;	/* newdoc.title in calls to getfile() */
extern char *LYTransferName;	/* abbreviation for Kilobytes */
extern char *LynxHome;
extern char *LynxSigFile;	/* Signature file, in or off home */
extern char *checked_box;	/* form boxes */
extern char *checked_radio;	/* form radio buttons */
extern char *empty_string;
extern char *helpfile;
extern char *helpfilepath;
extern char *jumpprompt;	/* The default jump statusline prompt */
extern char *language;
extern char *lynx_cfg_file;	/* location of active lynx.cfg file */
extern char *lynx_cmd_logfile;	/* file to write keystroke commands, if any */
extern char *lynx_cmd_script;	/* file to read keystroke commands, if any */
extern char *lynx_save_space;
extern char *lynx_temp_space;
extern char *lynxjumpfile;
extern char *lynxlinksfile;
extern char *lynxlistfile;
extern char *original_dir;
extern char *pref_charset;	/* Lynx's preferred character set - MM */
extern char *startfile;
extern char *system_mail;
extern char *system_mail_flags;
extern char *unchecked_box;	/* form boxes */
extern char *unchecked_radio;	/* form radio buttons */
extern char *x_display;
extern int LYTransferRate;	/* see enum TransferRate */
extern int display_lines;	/* number of lines in the display */
extern int dump_output_width;
extern int keypad_mode;		/* NUMBERS_AS_ARROWS or LINKS_ARE_NUMBERED */
extern int lynx_temp_subspace;
extern int user_mode;		/* novice or advanced */
extern int www_search_result;

extern BOOLEAN exec_frozen;
extern BOOLEAN had_restrictions_all;     /* parsed these restriction options */
extern BOOLEAN had_restrictions_default; /* flags to note whether we have... */
extern BOOLEAN no_bookmark;
extern BOOLEAN no_bookmark_exec;
extern BOOLEAN no_chdir;
extern BOOLEAN no_compileopts_info;
extern BOOLEAN no_disk_save;
extern BOOLEAN no_dotfiles;
extern BOOLEAN no_download;
extern BOOLEAN no_editor;
extern BOOLEAN no_exec;
extern BOOLEAN no_file_url;
extern BOOLEAN no_goto;
extern BOOLEAN no_goto_configinfo;
extern BOOLEAN no_goto_cso;
extern BOOLEAN no_goto_file;
extern BOOLEAN no_goto_finger;
extern BOOLEAN no_goto_ftp;
extern BOOLEAN no_goto_gopher;
extern BOOLEAN no_goto_http;
extern BOOLEAN no_goto_https;
extern BOOLEAN no_goto_lynxcgi;
extern BOOLEAN no_goto_lynxexec;
extern BOOLEAN no_goto_lynxprog;
extern BOOLEAN no_goto_mailto;
extern BOOLEAN no_goto_news;
extern BOOLEAN no_goto_nntp;
extern BOOLEAN no_goto_rlogin;
extern BOOLEAN no_goto_snews;
extern BOOLEAN no_goto_telnet;
extern BOOLEAN no_goto_tn3270;
extern BOOLEAN no_goto_wais;
extern BOOLEAN no_inside_ftp;
extern BOOLEAN no_inside_news;
extern BOOLEAN no_inside_rlogin;
extern BOOLEAN no_inside_telnet;  /* this and following are restrictions */
extern BOOLEAN no_jump;
extern BOOLEAN no_lynxcfg_info;
extern BOOLEAN no_lynxcfg_xinfo;
extern BOOLEAN no_lynxcgi;
extern BOOLEAN no_mail;
extern BOOLEAN no_multibook;
extern BOOLEAN no_newspost;
extern BOOLEAN no_option_save;
extern BOOLEAN no_outside_ftp;
extern BOOLEAN no_outside_news;
extern BOOLEAN no_outside_rlogin;
extern BOOLEAN no_outside_telnet;
extern BOOLEAN no_print;          /* TRUE to disable printing */
extern BOOLEAN no_shell;
extern BOOLEAN no_suspend;
extern BOOLEAN no_telnet_port;
extern BOOLEAN no_useragent;

extern BOOLEAN no_statusline;
extern BOOLEAN no_filereferer;
extern char LYRefererWithQuery;	/* 'S', 'P', or 'D' */
extern BOOLEAN local_host_only;
extern BOOLEAN override_no_download;
extern BOOLEAN show_dotfiles;	/* From rcfile if no_dotfiles is false */
extern char *indexfile;
extern char *personal_mail_address;
extern char *homepage;	      /* startfile or command line argument */
extern char *editor;          /* if non empty it enables edit mode with
			       * the editor that is named */
extern char *jumpfile;
extern char *bookmark_page;
extern char *BookmarkPage;
extern char *personal_type_map;
extern char *global_type_map;
extern char *global_extension_map;
extern char *personal_extension_map;
extern char *LYHostName;
extern char *LYLocalDomain;
extern BOOLEAN use_underscore;
extern BOOLEAN nolist;
extern BOOLEAN historical_comments;
extern BOOLEAN minimal_comments;
extern BOOLEAN soft_dquotes;

#ifdef USE_SOURCE_CACHE
extern BOOLEAN source_cache_file_error;
extern int LYCacheSource;
#define SOURCE_CACHE_NONE	0
#define SOURCE_CACHE_FILE	1
#define SOURCE_CACHE_MEMORY	2

extern int LYCacheSourceForAborted;
#define SOURCE_CACHE_FOR_ABORTED_KEEP 1
#define SOURCE_CACHE_FOR_ABORTED_DROP 0
#endif

extern BOOLEAN LYCancelDownload;
extern BOOLEAN LYRestricted;	/* whether we had -anonymous option */
extern BOOLEAN LYValidate;
extern BOOLEAN LYPermitURL;
extern BOOLEAN enable_scrollback; /* Clear screen before displaying new page */
extern BOOLEAN keep_mime_headers; /* Include mime headers and *
				   * force source dump	      */
extern BOOLEAN no_url_redirection;   /* Don't follow URL redirections */
#ifdef DISP_PARTIAL
extern BOOLEAN display_partial;      /* Display document while loading */
extern int NumOfLines_partial;       /* -//- "current" number of lines */
extern int partial_threshold;
extern BOOLEAN debug_display_partial;  /* show with MessageSecs delay */
extern BOOLEAN display_partial_flag; /* permanent flag, not mutable */
#endif
extern char *form_post_data;         /* User data for post form */
extern char *form_get_data;          /* User data for get form */
extern char *http_error_file;        /* Place HTTP status code in this file */
extern char *authentication_info[2]; /* Id:Password for protected documents */
extern char *proxyauth_info[2];      /* Id:Password for protected proxy server */
extern BOOLEAN HEAD_request;         /* Do a HEAD request */
extern BOOLEAN scan_for_buried_news_references;
extern BOOLEAN bookmark_start;       /* Use bookmarks as startfile */
extern BOOLEAN clickable_images;
extern BOOLEAN nested_tables;
extern BOOLEAN pseudo_inline_alts;
extern BOOLEAN crawl;
extern BOOLEAN traversal;
extern BOOLEAN check_realm;
extern char * startrealm;
extern BOOLEAN more_links;
extern int     ccount;
extern BOOLEAN LYCancelledFetch;
extern char * LYToolbarName;

extern int AlertSecs;
extern int InfoSecs;
extern int MessageSecs;
extern int DebugSecs;
extern int ReplaySecs;

extern char * LYUserAgent;		/* Lynx User-Agent header */
extern char * LYUserAgentDefault;	/* Lynx default User-Agent header */
extern BOOLEAN LYNoRefererHeader;	/* Never send Referer header? */
extern BOOLEAN LYNoRefererForThis;	/* No Referer header for this URL? */
extern BOOLEAN LYNoFromHeader;		/* Never send From header?    */
extern BOOLEAN LYListNewsNumbers;
extern BOOLEAN LYUseMouse;
extern BOOLEAN LYListNewsDates;

extern BOOLEAN LYRawMode;
extern BOOLEAN LYDefaultRawMode;
extern BOOLEAN LYUseDefaultRawMode;
extern char *UCAssume_MIMEcharset;
extern BOOLEAN UCSaveBookmarksInUnicode; /* in titles,  chars >127 save as &#xUUUU */
extern BOOLEAN UCForce8bitTOUPPER; /* disable locale case-conversion for >127 */
extern int outgoing_mail_charset; /* translate outgoing mail to this charset */

extern BOOLEAN LYisConfiguredForX;
extern char *URLDomainPrefixes;
extern char *URLDomainSuffixes;
extern BOOLEAN startfile_ok;
extern BOOLEAN LYSelectPopups;		/* Cast popups to radio buttons? */
extern BOOLEAN LYUseDefSelPop;		/* Command line -popup toggle    */
extern int LYMultiBookmarks;		/* Multi bookmark support on?	 */
extern BOOLEAN LYMBMBlocked;		/* Force MBM support off?	 */
extern int LYStatusLine;		/* Line for statusline() or -1   */
extern BOOLEAN LYCollapseBRs;		/* Collapse serial BRs?		 */
extern BOOLEAN LYSetCookies;		/* Process Set-Cookie headers?	 */
extern BOOLEAN LYAcceptAllCookies;      /* accept ALL cookies?           */
extern char *LYCookieAcceptDomains;     /* domains to accept all cookies */
extern char *LYCookieRejectDomains;     /* domains to reject all cookies */
extern char *LYCookieStrictCheckDomains; /* domains to check strictly    */
extern char *LYCookieLooseCheckDomains; /* domains to check loosely      */
extern char *LYCookieQueryCheckDomains; /* domains to check w/a query    */
extern char *LYCookieSAcceptDomains;    /* domains to accept all cookies */
extern char *LYCookieSRejectDomains;    /* domains to reject all cookies */
extern char *LYCookieSStrictCheckDomains;/* domains to check strictly    */
extern char *LYCookieSLooseCheckDomains;/* domains to check loosely      */
extern char *LYCookieSQueryCheckDomains;/* domains to check w/a query    */

#ifndef DISABLE_BIBP
extern BOOLEAN no_goto_bibp;
extern char *BibP_globalserver;         /* global server for bibp: links */
extern char *BibP_bibhost;              /* local server for bibp: links  */
extern BOOLEAN BibP_bibhost_checked;    /* bibhost has been checked      */
extern BOOLEAN BibP_bibhost_available;  /* bibhost is responding         */
#endif

#ifdef USE_PERSISTENT_COOKIES
extern BOOLEAN persistent_cookies;
extern char *LYCookieFile;              /* cookie read file              */
extern char *LYCookieSaveFile;          /* cookie save file              */
#endif /* USE_PERSISTENT_COOKIES */

extern char *XLoadImageCommand;		/* Default image viewer for X	 */

#ifdef USE_EXTERNALS
extern BOOLEAN no_externals; 		/* don't allow the use of externals */
#endif

extern BOOLEAN LYNoISMAPifUSEMAP;	/* Omit ISMAP link if MAP present? */
extern int LYHiddenLinks;

extern int Old_DTD;

#define MBM_V_MAXFILES  25		/* Max number of sub-bookmark files */

/*
 *  Arrays that holds the names of sub-bookmark files
 *  and their descriptions.
 */
extern char *MBM_A_subbookmark[MBM_V_MAXFILES+1];
extern char *MBM_A_subdescript[MBM_V_MAXFILES+1];

extern BOOLEAN LYForceSSLCookiesSecure;
extern BOOLEAN LYNoCc;
extern BOOLEAN LYNonRestartingSIGWINCH;
extern BOOLEAN LYPreparsedSource;	/* Show source as preparsed?	 */
extern BOOLEAN LYPrependBaseToSource;
extern BOOLEAN LYPrependCharsetToSource;
extern BOOLEAN LYQuitDefaultYes;
extern BOOLEAN LYReuseTempfiles;
extern BOOLEAN LYSeekFragAREAinCur;
extern BOOLEAN LYSeekFragMAPinCur;
extern BOOLEAN LYStripDotDotURLs;	/* Try to fix ../ in some URLs?  */
extern BOOLEAN LYUseBuiltinSuffixes;
extern BOOLEAN dont_wrap_pre;

extern int cookie_noprompt;

typedef enum {
    FORCE_PROMPT_DFT		/* force a prompt, use the result */
    ,FORCE_PROMPT_YES		/* assume "yes" where a prompt would be used */
    ,FORCE_PROMPT_NO		/* assume "no" where a prompt would be used */
} FORCE_PROMPT;

#ifdef USE_SSL
extern int ssl_noprompt;
#endif

#ifdef MISC_EXP
extern int LYNoZapKey;  /* 0: off (do 'z' checking), 1: full, 2: initially */
#endif

#ifdef EXP_JUSTIFY_ELTS
extern BOOL ok_justify;
extern int justify_max_void_percent;
#endif

#ifdef EXP_LOCALE_CHARSET
extern BOOLEAN LYLocaleCharset;
#endif

#ifndef NO_DUMP_WITH_BACKSPACES
extern BOOLEAN with_backspaces;
#endif

#if defined(PDCURSES) && defined(PDC_BUILD) && PDC_BUILD >= 2401
extern int scrsize_x;
extern int scrsize_y;
#endif

#ifndef NO_LYNX_TRACE
extern FILE *LYTraceLogFP;		/* Pointer for TRACE log	 */
extern char *LYTraceLogPath;		/* Path for TRACE log		 */
#endif
extern BOOLEAN LYUseTraceLog;		/* Use a TRACE log?		 */

extern BOOL force_empty_hrefless_a;
extern int connect_timeout;

#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
extern BOOL textfields_need_activation;
extern BOOL textfields_activation_option;
#ifdef INACTIVE_INPUT_STYLE_VH
extern BOOL textinput_redrawn;
#endif
#else
#define textfields_need_activation FALSE
#endif /* TEXTFIELDS_MAY_NEED_ACTIVATION */

extern BOOLEAN textfield_prompt_at_left_edge;


#ifndef VMS
extern BOOLEAN LYNoCore;
extern BOOLEAN restore_sigpipe_for_children;
#endif /* !VMS */

#if defined(USE_COLOR_STYLE)
extern char *lynx_lss_file;
#endif

extern int HTNoDataOK;		/* HT_NO_DATA-is-ok hack */
extern BOOLEAN FileInitAlreadyDone;

#ifdef __DJGPP__
extern BOOLEAN watt_debug;
extern BOOLEAN dj_is_bash;
#endif /* __DJGPP__ */

#ifdef WIN_EX
/* LYMain.c */
extern BOOLEAN focus_window;
extern BOOLEAN system_is_NT;
extern char windows_drive[4];
extern int lynx_timeout;
#endif /* _WINDOWS */

#ifdef SH_EX
extern BOOLEAN show_cfg;
#endif

extern BOOLEAN no_table_center;

#if USE_BLAT_MAILER
extern BOOLEAN mail_is_blat;
#endif

#if defined(__CYGWIN__)
extern void cygwin_conv_to_full_win32_path(char *posix, char *dos);
extern void cygwin_conv_to_full_posix_path(char *dos, char *posix);
extern int setmode(int handle, int amode);
#endif

#if !defined(__CYGWIN__) && defined(__CYGWIN32__)
#define __CYGWIN__

#define	cygwin_conv_to_full_win32_path(p, q) \
	cygwin32_conv_to_full_win32_path(p, q)

#define	cygwin_conv_to_full_posix_path(p, q) \
	cygwin32_conv_to_full_posix_path(p, q)
#endif

#ifdef USE_SCROLLBAR
/* GridText.c */
extern BOOLEAN LYShowScrollbar;
extern BOOLEAN LYsb_arrow;
extern int LYsb_begin;
extern int LYsb_end;
#endif

#ifdef MARK_HIDDEN_LINKS
extern char* hidden_link_marker;
#endif

#ifdef USE_BLINK
extern BOOLEAN term_blink_is_boldbg;
#endif

#endif /* LYGLOBALDEFS_H */
