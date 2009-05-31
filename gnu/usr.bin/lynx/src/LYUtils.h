#ifndef LYUTILS_H
#define LYUTILS_H

#include <LYCharVals.h>		/* S/390 -- gil -- 2149 */
#include <LYKeymap.h>

#ifndef HTLIST_H
#include <HTList.h>
#endif /* HTLIST_H */

#ifdef VMS
#include <HTFTP.h>
#include <HTVMSUtils.h>
#endif /* VMS */

#if defined(USE_DOS_DRIVES)
#include <HTDOS.h>
#endif

#if defined(SYSLOG_REQUESTED_URLS)
#include <syslog.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#ifdef VMS
#define HTSYS_name(path)   HTVMS_name("", path)
#define HTSYS_purge(path)  HTVMS_purge(path)
#define HTSYS_remove(path) HTVMS_remove(path)
#endif				/* VMS */
#if defined(USE_DOS_DRIVES)
#define HTSYS_name(path) HTDOS_name(path)
#endif
#ifndef HTSYS_name
#define HTSYS_name(path) path
#endif
#ifndef HTSYS_purge
#define HTSYS_purge(path)	/* nothing */
#endif
#ifndef HTSYS_remove
#define HTSYS_remove(path) remove(path)
#endif
#define LYIsPipeCommand(s) ((s)[0] == '|')
#ifdef VMS
#define TTY_DEVICE "tt:"
#define NUL_DEVICE "nl:"
#define LYIsNullDevice(s) (!strncasecomp(s, "nl:", 3) || !strncasecomp(s, "/nl/", 4))
#define LYSameFilename(a,b) (!strcasecomp(a,b))
#define LYSameHostname(a,b) (!strcasecomp(a,b))
#else
#if defined(DOSPATH) || defined(__EMX__)
#define TTY_DEVICE "con"
#define NUL_DEVICE "nul"
#define LYIsNullDevice(s) LYSameFilename(s,NUL_DEVICE)
#define LYSameFilename(a,b) (!strcasecomp(a,b))
#define LYSameHostname(a,b) (!strcasecomp(a,b))
#else
#if defined(__CYGWIN__)
#define TTY_DEVICE "/dev/tty"
#define NUL_DEVICE "/dev/null"
#define LYIsNullDevice(s) LYSameFilename(s,NUL_DEVICE)
#define LYSameFilename(a,b) (!strcasecomp(a,b))
#define LYSameHostname(a,b) (!strcasecomp(a,b))
#else
#define TTY_DEVICE "/dev/tty"
#define NUL_DEVICE "/dev/null"
#define LYIsNullDevice(s) LYSameFilename(s,NUL_DEVICE)
#define LYSameFilename(a,b) (!strcmp(a,b))
#define LYSameHostname(a,b) (!strcmp(a,b))
#endif				/* __CYGWIN__ */
#endif				/* DOSPATH */
#endif				/* VMS */
/* See definitions in src/LYCharVals.h.  The hardcoded values...
   This prohibits binding C-c and C-g.  Maybe it is better to remove this? */
#define LYCharIsINTERRUPT_HARD(ch)	\
  ((ch) == LYCharINTERRUPT1 || ch == LYCharINTERRUPT2)
#define LYCharIsINTERRUPT(ch)		\
  (LYCharIsINTERRUPT_HARD(ch) || LKC_TO_LAC(keymap,ch) == LYK_INTERRUPT)
#define LYCharIsINTERRUPT_NO_letter(ch)	\
  (LYCharIsINTERRUPT(ch) && !isprint(ch))
#if defined(USE_DOS_DRIVES)
#define PATHSEP_STR "\\"
#define LYIsPathSep(ch) ((ch) == '/' || (ch) == '\\')
#define LYIsDosDrive(s) (isalpha(UCH((s)[0])) && (s)[1] == ':')
#else
#define PATHSEP_STR "/"
#define LYIsPathSep(ch) ((ch) == '/')
#define LYIsDosDrive(s) FALSE	/* really nothing */
#endif
#ifdef EXP_ADDRLIST_PAGE
#define LYIsListpageTitle(name) \
    (!strcmp((name), LIST_PAGE_TITLE) || \
     !strcmp((name), ADDRLIST_PAGE_TITLE))
#else
#define LYIsListpageTitle(name) \
    (!strcmp((name), LIST_PAGE_TITLE))
#endif
#define LYIsHtmlSep(ch) ((ch) == '/')
#define findPoundSelector(address) strchr(address, '#')
#define restorePoundSelector(pound) if ((pound) != NULL) *(pound) = '#'
    extern BOOL strn_dash_equ(const char *p1, const char *p2, int len);
    extern BOOLEAN LYAddSchemeForURL(char **AllocatedString, const char *default_scheme);
    extern BOOLEAN LYCachedTemp(char *result, char **cached);
    extern BOOLEAN LYCanDoHEAD(const char *address);
    extern BOOLEAN LYCanReadFile(const char *name);
    extern BOOLEAN LYCanWriteFile(const char *name);
    extern BOOLEAN LYCloseInput(FILE *fp);
    extern BOOLEAN LYCloseOutput(FILE *fp);
    extern BOOLEAN LYExpandHostForURL(char **AllocatedString, char
				      *prefix_list, char *suffix_list);
    extern BOOLEAN LYFixCursesOnForAccess(const char *addr, const char *physical);
    extern BOOLEAN LYPathOffHomeOK(char *fbuffer, size_t fbuffer_size);
    extern BOOLEAN LYValidateFilename(char *result, char *given);
    extern BOOLEAN LYisAbsPath(const char *path);
    extern BOOLEAN LYisLocalAlias(const char *filename);
    extern BOOLEAN LYisLocalFile(const char *filename);
    extern BOOLEAN LYisLocalHost(const char *filename);
    extern BOOLEAN LYisRootPath(const char *path);
    extern BOOLEAN inlocaldomain(void);
    extern FILE *InternalPageFP(char *filename, int reuse_flag);
    extern FILE *LYAppendToTxtFile(const char *name);
    extern FILE *LYNewBinFile(const char *name);
    extern FILE *LYNewTxtFile(const char *name);
    extern FILE *LYOpenScratch(char *result, const char *prefix);
    extern FILE *LYOpenTemp(char *result, const char *suffix, const char *mode);
    extern FILE *LYOpenTempRewrite(char *result, const char *suffix, const char *mode);
    extern FILE *LYReopenTemp(char *name);
    extern char *Current_Dir(char *pathname);
    extern char *LYAddPathToSave(char *fname);
    extern char *LYGetEnv(const char *name);
    extern char *LYLastPathSep(const char *path);
    extern char *LYPathLeaf(char *pathname);
    extern char *LYgetXDisplay(void);
    extern char *strip_trailing_slash(char *my_dirname);
    extern char *trimPoundSelector(char *address);
    extern const char *Home_Dir(void);
    extern const char *LYGetHiliteStr(int cur, int count);
    extern const char *LYSysShell(void);
    extern const char *index_to_restriction(int inx);
    extern const char *wwwName(const char *pathname);
    extern int HTCheckForInterrupt(void);
    extern int LYConsoleInputFD(BOOLEAN need_selectable);
    extern int LYCopyFile(char *src, char *dst);
    extern int LYGetHilitePos(int cur, int count);
    extern int LYRemoveTemp(char *name);
    extern int LYReopenInput(void);
    extern int LYSystem(char *command);
    extern int LYValidateOutput(char *filename);
    extern int find_restriction(const char *name, int len);
    extern int number2arrows(int number);
    extern size_t utf8_length(BOOL utf_flag, const char *data);
    extern time_t LYmktime(char *string, BOOL absolute);
    extern void BeginInternalPage(FILE *fp0, const char *Title, const char *HelpURL);
    extern void EndInternalPage(FILE *fp0);
    extern void HTAddSugFilename(char *fname);
    extern void HTSugFilenames_free(void);
    extern void LYAddHilite(int cur, char *text, int x);
    extern void LYAddHtmlSep(char **path);
    extern void LYAddHtmlSep0(char *path);
    extern void LYAddLocalhostAlias(char *alias);
    extern void LYAddPathSep(char **path);
    extern void LYAddPathSep0(char *path);
    extern void LYAddPathToHome(char *fbuffer, size_t fbuffer_size, const char *fname);
    extern void LYCheckBibHost(void);
    extern void LYCheckMail(void);
    extern void LYCleanupTemp(void);
    extern void LYCloseTemp(char *name);
    extern void LYCloseTempFP(FILE *fp);
    extern void LYConvertToURL(char **AllocatedString, int fixit);
    extern void LYDoCSI(char *url, const char *comment, char **csi);
    extern void LYEnsureAbsoluteURL(char **href, const char *name, int fixit);
    extern void LYFakeZap(BOOL set);
    extern void LYFixCursesOn(const char *reason);
    extern void LYFreeHilites(int first, int last);
    extern void LYFreeStringList(HTList *list);
    extern void LYLocalFileToURL(char **target, const char *source);
    extern void LYLocalhostAliases_free(void);
    extern void LYRenamedTemp(char *oldname, char *newname);
    extern void LYSetHilite(int cur, const char *text);
    extern void LYTrimHtmlSep(char *path);
    extern void LYTrimPathSep(char *path);
    extern void LYTrimRelFromAbsPath(char *path);
    extern void LYhighlight(int flag, int cur, const char *target);
    extern void LYmsec_delay(unsigned msec);
    extern void LYsetXDisplay(char *new_display);
    extern void WriteInternalTitle(FILE *fp0, const char *Title);
    extern void change_sug_filename(char *fname);
    extern void convert_to_spaces(char *string, BOOL condense);
    extern void free_and_clear(char **obj);
    extern void noviceline(int more_flag);
    extern void parse_restrictions(const char *s);
    extern void print_restrictions_to_fd(FILE *fp);
    extern void remove_backslashes(char *buf);
    extern void size_change(int sig);
    extern void statusline(const char *text);
    extern void toggle_novice_line(void);

#if defined(MULTI_USER_UNIX)
    extern BOOL IsOurFile(const char *name);
#else
#define IsOurFile(name) TRUE
#endif

#ifdef EXP_ASCII_CTYPES
    extern int ascii_tolower(int i);
    extern int ascii_toupper(int i);
    extern int ascii_isupper(int i);
#endif

#ifdef __CYGWIN__
    extern int Cygwin_Shell(void);
#endif

#ifdef _WIN_CC
    extern int exec_command(char *cmd, int wait_flag);	/* xsystem.c */
    extern int xsystem(char *cmd);
#endif

/* Keeping track of User Interface Pages: */
    typedef enum {
	UIP_UNKNOWN = -1
	,UIP_HISTORY = 0
	,UIP_DOWNLOAD_OPTIONS
	,UIP_PRINT_OPTIONS
	,UIP_SHOWINFO
	,UIP_LIST_PAGE
	,UIP_VLINKS
	,UIP_LYNXCFG
	,UIP_OPTIONS_MENU
	,UIP_DIRED_MENU
	,UIP_PERMIT_OPTIONS
	,UIP_UPLOAD_OPTIONS
	,UIP_ADDRLIST_PAGE
	,UIP_CONFIG_DEF
	,UIP_TRACELOG
	,UIP_INSTALL
    } UIP_t;

#define UIP_P_FRAG 0x0001	/* flag: consider "url#frag" as matching "url" */

    extern BOOL LYIsUIPage3(const char *url, UIP_t type, int flagparam);

#define LYIsUIPage(url,type) LYIsUIPage3(url, type, UIP_P_FRAG)
    extern void LYRegisterUIPage(const char *url, UIP_t type);

#define LYUnRegisterUIPage(type) LYRegisterUIPage(NULL, type)
    extern void LYUIPages_free(void);

#ifdef CAN_CUT_AND_PASTE
    extern int put_clip(const char *szBuffer);

/* get_clip_grab() returns a pointer to the string in the system area.
   get_clip_release() should be called ASAP after this. */
    extern char *get_clip_grab(void);
    extern void get_clip_release(void);

#  ifdef WIN_EX
#    define size_clip()	8192
#  else
    extern int size_clip(void);

#  endif
#endif

#if defined(WIN_EX)		/* 1997/10/16 (Thu) 20:13:28 */
    extern char *HTDOS_short_name(char *path);
    extern char *w32_strerror(DWORD ercode);
#endif

#ifdef VMS
    extern void Define_VMSLogical(char *LogicalName, char *LogicalValue);
#endif				/* VMS */

#if ! HAVE_PUTENV
    extern int putenv(const char *string);
#endif				/* HAVE_PUTENV */

#if defined(MULTI_USER_UNIX)
    extern void LYRelaxFilePermissions(const char *name);

#else
#define LYRelaxFilePermissions(name)	/* nothing */
#endif

/*
 *  Whether or not the status line must be shown.
 */
    extern BOOLEAN mustshow;

#define _statusline(msg)	mustshow = TRUE, statusline(msg)

/*
 *  For is_url().
 *
 *  Universal document id types (see LYCheckForProxyURL)
 */
    typedef enum {
	NOT_A_URL_TYPE = 0,
	UNKNOWN_URL_TYPE = 1,	/* must be nonzero */

	HTTP_URL_TYPE,
	FILE_URL_TYPE,
	FTP_URL_TYPE,
	NCFTP_URL_TYPE,
	WAIS_URL_TYPE,
	NEWS_URL_TYPE,
	NNTP_URL_TYPE,
	TELNET_URL_TYPE,
	TN3270_URL_TYPE,
	RLOGIN_URL_TYPE,
	GOPHER_URL_TYPE,
	HTML_GOPHER_URL_TYPE,
	TELNET_GOPHER_URL_TYPE,
	INDEX_GOPHER_URL_TYPE,
	MAILTO_URL_TYPE,
	BIBP_URL_TYPE,
	FINGER_URL_TYPE,
	CSO_URL_TYPE,
	HTTPS_URL_TYPE,
	SNEWS_URL_TYPE,
	PROSPERO_URL_TYPE,
	AFS_URL_TYPE,

	DATA_URL_TYPE,

	LYNXEXEC_URL_TYPE,
	LYNXPROG_URL_TYPE,
	LYNXCGI_URL_TYPE,

	NEWSPOST_URL_TYPE,
	NEWSREPLY_URL_TYPE,
	SNEWSPOST_URL_TYPE,
	SNEWSREPLY_URL_TYPE,

	LYNXPRINT_URL_TYPE,
	LYNXHIST_URL_TYPE,
	LYNXDOWNLOAD_URL_TYPE,
	LYNXKEYMAP_URL_TYPE,
	LYNXIMGMAP_URL_TYPE,
	LYNXCOOKIE_URL_TYPE,
	LYNXDIRED_URL_TYPE,
	LYNXOPTIONS_URL_TYPE,
	LYNXCFG_URL_TYPE,
	LYNXCOMPILE_OPTS_URL_TYPE,
	LYNXMESSAGES_URL_TYPE,

	PROXY_URL_TYPE

    } UrlTypes;

    extern UrlTypes LYCheckForProxyURL(char *filename);
    extern UrlTypes is_url(char *filename);

/* common URLs */
#define STR_BIBP_URL         "bibp:"
#define LEN_BIBP_URL         5
#define isBIBP_URL(addr)     !strncasecomp(addr, STR_BIBP_URL, LEN_BIBP_URL)

#define STR_CSO_URL          "cso:"
#define LEN_CSO_URL          4
#define isCSO_URL(addr)      !strncasecomp(addr, STR_CSO_URL, LEN_CSO_URL)

#define STR_FILE_URL         "file:"
#define LEN_FILE_URL         5
#define isFILE_URL(addr)     ((*addr == 'f' || *addr == 'F') &&\
                             !strncasecomp(addr, STR_FILE_URL, LEN_FILE_URL))

#define STR_FINGER_URL       "finger:"
#define LEN_FINGER_URL       7
#define isFINGER_URL(addr)   !strncasecomp(addr, STR_FINGER_URL, LEN_FINGER_URL)

#define STR_FTP_URL          "ftp:"
#define LEN_FTP_URL          4
#define isFTP_URL(addr)      !strncasecomp(addr, STR_FTP_URL, LEN_FTP_URL)

#define STR_GOPHER_URL       "gopher:"
#define LEN_GOPHER_URL       7
#define isGOPHER_URL(addr)   !strncasecomp(addr, STR_GOPHER_URL, LEN_GOPHER_URL)

#define STR_HTTP_URL         "http:"
#define LEN_HTTP_URL         5
#define isHTTP_URL(addr)     !strncasecomp(addr, STR_HTTP_URL, LEN_HTTP_URL)

#define STR_HTTPS_URL        "https:"
#define LEN_HTTPS_URL        6
#define isHTTPS_URL(addr)    !strncasecomp(addr, STR_HTTPS_URL, LEN_HTTPS_URL)

#define STR_MAILTO_URL       "mailto:"
#define LEN_MAILTO_URL       7
#define isMAILTO_URL(addr)   !strncasecomp(addr, STR_MAILTO_URL, LEN_MAILTO_URL)

#define STR_NEWS_URL         "news:"
#define LEN_NEWS_URL         5
#define isNEWS_URL(addr)     !strncasecomp(addr, STR_NEWS_URL, LEN_NEWS_URL)

#define STR_NNTP_URL         "nntp:"
#define LEN_NNTP_URL         5
#define isNNTP_URL(addr)     !strncasecomp(addr, STR_NNTP_URL, LEN_NNTP_URL)

#define STR_RLOGIN_URL       "rlogin:"
#define LEN_RLOGIN_URL       7
#define isRLOGIN_URL(addr)   !strncasecomp(addr, STR_RLOGIN_URL, LEN_RLOGIN_URL)

#define STR_SNEWS_URL        "snews:"
#define LEN_SNEWS_URL        6
#define isSNEWS_URL(addr)    !strncasecomp(addr, STR_SNEWS_URL, LEN_SNEWS_URL)

#define STR_TELNET_URL       "telnet:"
#define LEN_TELNET_URL       7
#define isTELNET_URL(addr)   !strncasecomp(addr, STR_TELNET_URL, LEN_TELNET_URL)

#define STR_TN3270_URL       "tn3270:"
#define LEN_TN3270_URL       7
#define isTN3270_URL(addr)   !strncasecomp(addr, STR_TN3270_URL, LEN_TN3270_URL)

#define STR_WAIS_URL         "wais:"
#define LEN_WAIS_URL         5
#define isWAIS_URL(addr)     !strncasecomp(addr, STR_WAIS_URL, LEN_WAIS_URL)

/* internal URLs */
#define STR_LYNXCFG          "LYNXCFG:"
#define LEN_LYNXCFG          8
#define isLYNXCFG(addr)      !strncasecomp(addr, STR_LYNXCFG, LEN_LYNXCFG)

#define STR_LYNXCFLAGS       "LYNXCOMPILEOPTS:"
#define LEN_LYNXCFLAGS       16
#define isLYNXCFLAGS(addr)   !strncasecomp(addr, STR_LYNXCFLAGS, LEN_LYNXCFLAGS)

#define STR_LYNXCGI          "lynxcgi:"
#define LEN_LYNXCGI          8
#define isLYNXCGI(addr)      ((*addr == 'l' || *addr == 'L') &&\
                             !strncasecomp(addr, STR_LYNXCGI, LEN_LYNXCGI))

#define STR_LYNXCOOKIE       "LYNXCOOKIE:"
#define LEN_LYNXCOOKIE       11
#define isLYNXCOOKIE(addr)   !strncasecomp(addr, STR_LYNXCOOKIE, LEN_LYNXCOOKIE)

#define STR_LYNXDIRED        "LYNXDIRED:"
#define LEN_LYNXDIRED        10
#define isLYNXDIRED(addr)    !strncasecomp(addr, STR_LYNXDIRED, LEN_LYNXDIRED)

#define STR_LYNXEXEC         "lynxexec:"
#define LEN_LYNXEXEC         9
#define isLYNXEXEC(addr)     ((*addr == 'l' || *addr == 'L') &&\
                             !strncasecomp(addr, STR_LYNXEXEC, LEN_LYNXEXEC))

#define STR_LYNXDOWNLOAD     "LYNXDOWNLOAD:"
#define LEN_LYNXDOWNLOAD     13
#define isLYNXDOWNLOAD(addr) !strncasecomp(addr, STR_LYNXDOWNLOAD, LEN_LYNXDOWNLOAD)

#define STR_LYNXHIST         "LYNXHIST:"
#define LEN_LYNXHIST         9
#define isLYNXHIST(addr)     !strncasecomp(addr, STR_LYNXHIST, LEN_LYNXHIST)

#define STR_LYNXKEYMAP       "LYNXKEYMAP:"
#define LEN_LYNXKEYMAP       11
#define isLYNXKEYMAP(addr)   !strncasecomp(addr, STR_LYNXKEYMAP, LEN_LYNXKEYMAP)

#define STR_LYNXIMGMAP       "LYNXIMGMAP:"
#define LEN_LYNXIMGMAP       11
#define isLYNXIMGMAP(addr)   !strncasecomp(addr, STR_LYNXIMGMAP, LEN_LYNXIMGMAP)

#define STR_LYNXMESSAGES     "LYNXMESSAGES:"
#define LEN_LYNXMESSAGES     13
#define isLYNXMESSAGES(addr) !strncasecomp(addr, STR_LYNXMESSAGES, LEN_LYNXMESSAGES)

#define STR_LYNXOPTIONS      "LYNXOPTIONS:"
#define LEN_LYNXOPTIONS      12
#define isLYNXOPTIONS(addr)  !strncasecomp(addr, STR_LYNXOPTIONS, LEN_LYNXOPTIONS)

#define STR_LYNXPRINT        "LYNXPRINT:"
#define LEN_LYNXPRINT        10
#define isLYNXPRINT(addr)    !strncasecomp(addr, STR_LYNXPRINT, LEN_LYNXPRINT)

#define STR_LYNXPROG         "lynxprog:"
#define LEN_LYNXPROG         9
#define isLYNXPROG(addr)     ((*addr == 'l' || *addr == 'L') &&\
                             !strncasecomp(addr, STR_LYNXPROG, LEN_LYNXPROG))

#define LYNXOPTIONS_PAGE(s)  STR_LYNXOPTIONS s
/*
 *  For change_sug_filename().
 */
    extern HTList *sug_filenames;

/*
 * syslog() facility
 */
#if defined(SYSLOG_REQUESTED_URLS)
    extern void LYOpenlog(const char *banner);
    extern void LYSyslog(char *arg);
    extern void LYCloselog(void);
#endif				/* SYSLOG_REQUESTED_URLS */

/*
 *  Miscellaneous.
 */
#define ON      1
#define OFF     0
#define STREQ(a,b) (strcmp(a,b) == 0)
#define STRNEQ(a,b,c) (strncmp(a,b,c) == 0)

#define HIDE_CHMOD 0600
#define HIDE_UMASK 0077

#if defined(DOSPATH) || defined(__CYGWIN__)
#define TXT_R	"rt"
#define TXT_W	"wt"
#define TXT_A	"at+"
#else
#define TXT_R	"r"
#define TXT_W	"w"
#define TXT_A	"a+"
#endif

#define BIN_R	"rb"
#define BIN_W	"wb"
#define BIN_A	"ab+"

#ifdef __cplusplus
}
#endif
#endif				/* LYUTILS_H */
