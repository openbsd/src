/* global variable definitions */

#ifndef LYGLOBALDEFS_H
#define LYGLOBALDEFS_H

#ifndef USERDEFS_H
#include "userdefs.h"
#endif /* USERDEFS_H */

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */

#ifndef LYSTRUCTS_H
#include "LYStructs.h"
#endif /* LYSTRUCTS_H */

#ifdef SOCKS
extern BOOLEAN socks_flag;
#endif /* SOCKS */

#ifdef IGNORE_CTRL_C
extern BOOLEAN sigint;
#endif /* IGNORE_CTRL_C */

#ifdef VMS
extern char *mail_adrs;
extern BOOLEAN UseFixedRecords; /* convert binary files to FIXED 512 records */
#endif /* VMS */

#ifndef VMS
extern char *NNTPSERVER_putenv_cmd;
extern char *http_proxy_putenv_cmd;
extern char *https_proxy_putenv_cmd;
extern char *ftp_proxy_putenv_cmd;
extern char *gopher_proxy_putenv_cmd;
extern char *cso_proxy_putenv_cmd;
extern char *news_proxy_putenv_cmd;
extern char *newspost_proxy_putenv_cmd;
extern char *newsreply_proxy_putenv_cmd;
extern char *snews_proxy_putenv_cmd;
extern char *snewspost_proxy_putenv_cmd;
extern char *snewsreply_proxy_putenv_cmd;
extern char *nntp_proxy_putenv_cmd;
extern char *wais_proxy_putenv_cmd;
extern char *finger_proxy_putenv_cmd;
extern char *no_proxy_putenv_cmd;
extern char *list_format;
#endif /* !VMS */

#ifdef VMS
extern char *LYCSwingPath;
#endif /* VMS */

#ifdef DIRED_SUPPORT
extern BOOLEAN lynx_edit_mode;
extern BOOLEAN no_dired_support;
extern BOOLEAN dir_list_style;
extern HTList *tagged;
#define FILES_FIRST 1
#define MIXED_STYLE 2
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
#define LINKS_AND_FORM_FIELDS_ARE_NUMBERED 2

#define HIDDENLINKS_MERGE	0
#define HIDDENLINKS_SEPARATE	1
#define HIDDENLINKS_IGNORE	2

#define NOVICE_MODE 	  0
#define INTERMEDIATE_MODE 1
#define ADVANCED_MODE 	  2
extern BOOLEAN LYUseNoviceLineTwo;  /* True if TOGGLE_HELP is not mapped */

#define MAX_LINE 1024	/* Hope that no window is larger than this */
extern char star_string[MAX_LINE + 1]; /* from GridText.c */
#define STARS(n) \
 ((n) >= MAX_LINE ? star_string : &star_string[(MAX_LINE-1)] - (n))
#define DIRNAMESIZE 256

#define SHOW_COLOR_UNKNOWN	(-1)
#define SHOW_COLOR_NEVER  0
#define SHOW_COLOR_OFF	  1
#define SHOW_COLOR_ON	  2
#define SHOW_COLOR_ALWAYS 3
extern int LYShowColor;		/* Show color or monochrome?	    */
extern int LYChosenShowColor;	/* extended color/monochrome choice */
extern int LYrcShowColor;	/* ... as read or last written	    */

extern BOOLEAN LYShowCursor;	/* Show the cursor or hide it?	    */
extern BOOLEAN LYUseDefShoCur;	/* Command line -show_cursor toggle */
extern BOOLEAN LYCursesON;  /* start_curses()->TRUE, stop_curses()->FALSE */
extern BOOLEAN LYUserSpecifiedURL;  /* URL from a goto or document? */
extern BOOLEAN LYJumpFileURL;   /* URL from the jump file shortcuts? */
extern BOOLEAN jump_buffer;     /* TRUE if offering default shortcut */
extern BOOLEAN goto_buffer;     /* TRUE if offering default goto URL */
extern char *LYRequestTitle;    /* newdoc.title in calls to getfile() */
extern char *jumpprompt;        /* The default jump statusline prompt */
extern int more;  /* is there more document to display? */
extern int display_lines; /* number of lines in the display */
extern int www_search_result;
extern char *checked_box;  /* form boxes */
extern char *unchecked_box;  /* form boxes */
extern char *checked_radio;  /* form radio buttons */
extern char *unchecked_radio;  /* form radio buttons */
extern char *empty_string;
extern char *LynxHome;
extern char *startfile;
extern char *helpfile;
extern char *helpfilepath;
extern char *lynxjumpfile;
extern char *lynxlistfile;
extern char *lynxlinksfile;
extern char *display;
extern char *language;
extern char *pref_charset;	/* Lynx's preferred character set - MM */
extern BOOLEAN LYNewsPosting;	/* News posting supported if TRUE */
extern char *LynxSigFile;	/* Signature file, in or off home */
extern char *system_mail;
extern char *system_mail_flags;
extern char *lynx_temp_space;
extern char *lynx_save_space;
extern BOOLEAN LYforce_HTML_mode;
extern BOOLEAN LYforce_no_cache;
extern BOOLEAN LYoverride_no_cache;  /* don't need fresh copy, from history */
extern BOOLEAN LYinternal_flag; /* don't need fresh copy, was internal link */
extern BOOLEAN LYresubmit_posts;
extern BOOLEAN user_mode; /* novice or advanced */
extern BOOLEAN is_www_index;
extern BOOLEAN dump_output_immediately;
extern int dump_output_width;
extern BOOLEAN lynx_mode;
extern BOOLEAN bold_headers;
extern BOOLEAN bold_H1;
extern BOOLEAN bold_name_anchors;
extern BOOLEAN recent_sizechange;
extern BOOLEAN telnet_ok;
extern BOOLEAN news_ok;
extern BOOLEAN ftp_ok;
extern BOOLEAN rlogin_ok;
extern BOOLEAN no_print;    /* TRUE to disable printing */
extern BOOLEAN system_editor; /* True if locked-down editor */
extern BOOLEAN child_lynx;	  /* TRUE to exit with an arrow */
extern BOOLEAN error_logging;     /* TRUE to mail error messages */
extern BOOLEAN check_mail;        /* TRUE to report unread/new mail messages */
extern BOOLEAN vi_keys;           /* TRUE to turn on vi-like key movement */
extern BOOLEAN emacs_keys;        /* TRUE to turn on emacs-like key movement */
extern BOOLEAN keypad_mode;       /* is set to either NUMBERS_AS_ARROWS *
				   * or LINKS_ARE_NUMBERED 		*/
extern BOOLEAN case_sensitive;    /* TRUE to turn on case sensitive search */
extern BOOLEAN no_inside_telnet;  /* this and following are restrictions */
extern BOOLEAN no_outside_telnet;
extern BOOLEAN no_telnet_port;
extern BOOLEAN no_inside_news;  
extern BOOLEAN no_outside_news;
extern BOOLEAN no_inside_ftp;
extern BOOLEAN no_outside_ftp;
extern BOOLEAN no_inside_rlogin;
extern BOOLEAN no_outside_rlogin;
extern BOOLEAN no_suspend;
extern BOOLEAN no_editor;
extern BOOLEAN no_shell;
extern BOOLEAN no_bookmark;
extern BOOLEAN no_multibook;
extern BOOLEAN no_bookmark_exec;
extern BOOLEAN no_option_save;
extern BOOLEAN no_print;
extern BOOLEAN no_download;
extern BOOLEAN no_disk_save;
extern BOOLEAN no_exec;
extern BOOLEAN no_lynxcgi;
extern BOOLEAN exec_frozen;
extern BOOLEAN no_goto;
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
extern BOOLEAN no_jump;
extern BOOLEAN no_file_url;
extern BOOLEAN no_newspost;
extern BOOLEAN no_mail;
extern BOOLEAN no_dotfiles;
extern BOOLEAN no_useragent;
extern BOOLEAN no_statusline;
extern BOOLEAN no_filereferer;
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
extern BOOLEAN LYCancelDownload;
extern BOOLEAN LYValidate;
extern BOOLEAN LYPermitURL;
extern BOOLEAN enable_scrollback; /* Clear screen before displaying new page */
extern BOOLEAN keep_mime_headers; /* Include mime headers and *
				   * force source dump	      */
extern BOOLEAN no_url_redirection;   /* Don't follow URL redirections */
extern char *form_post_data;         /* User data for post form */
extern char *form_get_data;          /* User data for get form */
extern char *http_error_file;        /* Place HTTP status code in this file */
extern char *authentication_info[2]; /* Id:Password for protected documents */
extern char *proxyauth_info[2];	  /* Id:Password for protected proxy server */
extern BOOLEAN HEAD_request;	     /* Do a HEAD request */
extern BOOLEAN scan_for_buried_news_references;
extern BOOLEAN bookmark_start;	  /* Use bookmarks as startfile */
extern BOOLEAN clickable_images;
extern BOOLEAN pseudo_inline_alts;
extern BOOLEAN crawl;
extern BOOLEAN traversal;
extern BOOLEAN check_realm;
extern char * startrealm;
extern BOOLEAN more_links;
extern int     ccount;
extern BOOLEAN LYCancelledFetch;
extern char * LYToolbarName;
extern int InfoSecs;
extern int MessageSecs;
extern int AlertSecs;
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
extern char *UCAssume_localMIMEcharset;
extern char *UCAssume_unrecMIMEcharset;
extern BOOLEAN LYisConfiguredForX;
extern char *URLDomainPrefixes;
extern char *URLDomainSuffixes;
extern BOOLEAN startfile_ok;
extern BOOLEAN LYSelectPopups;		/* Cast popups to radio buttons? */
extern BOOLEAN LYUseDefSelPop;		/* Command line -popup toggle    */
extern int LYMultiBookmarks;    	/* Multi bookmark support on?	 */
extern BOOLEAN LYMBMBlocked;		/* Force MBM support off?	 */
extern BOOLEAN LYMBMAdvanced;		/* MBM statusline for ADVANCED?	 */
extern int LYStatusLine;		/* Line for statusline() or -1   */
extern BOOLEAN LYCollapseBRs;		/* Collapse serial BRs?		 */
extern BOOLEAN LYSetCookies;		/* Process Set-Cookie headers?	 */
extern char *XLoadImageCommand;		/* Default image viewer for X	 */
#ifdef USE_EXTERNALS
extern BOOLEAN no_externals; /* don't allow the use of externals */
#endif
extern BOOLEAN LYNoISMAPifUSEMAP;	/* Omit ISMAP link if MAP present? */
extern int LYHiddenLinks;

extern BOOL New_DTD; 

#define BOOKMARK_TITLE "Bookmark file"
#define MOSAIC_BOOKMARK_TITLE "Converted Mosaic Hotlist"
#define MBM_V_MAXFILES  25	/* Max number of sub-bookmark files */
/*
 *  Arrays that holds the names of sub-bookmark files
 *  and their descriptions.
 */
extern char *MBM_A_subbookmark[MBM_V_MAXFILES+1];
extern char *MBM_A_subdescript[MBM_V_MAXFILES+1];
extern FILE *LYTraceLogFP;		/* Pointer for TRACE log	 */
extern char *LYTraceLogPath;		/* Path for TRACE log		 */
extern BOOLEAN LYUseTraceLog;		/* Use a TRACE log?		 */
extern FILE LYOrigStderr;		/* Original stderr pointer	 */
extern BOOLEAN LYSeekFragMAPinCur;
extern BOOLEAN LYSeekFragAREAinCur;
extern BOOLEAN LYStripDotDotURLs;	/* Try to fix ../ in some URLs?  */
extern BOOLEAN LYForceSSLCookiesSecure;
extern BOOLEAN LYNoCc;
extern BOOLEAN LYPreparsedSource;	/* Show source as preparsed?	 */
extern BOOLEAN LYPrependBaseToSource;
extern BOOLEAN LYPrependCharsetToSource;
extern BOOLEAN LYQuitDefaultYes;

#ifdef RAWDOSKEYHACK
extern BOOLEAN raw_dos_key_hack; /* Turn on and off DOS key hack */
#endif /* RAWDOSKEYHACK */

#ifndef VMS
extern BOOLEAN LYNoCore;
#endif /* !VMS */

#endif /* LYGLOBALDEFS_H */
