#ifndef LYUTILS_H
#define LYUTILS_H

#include <LYCharVals.h>  /* S/390 -- gil -- 2149 */
#include <LYKeymap.h>

#ifndef HTLIST_H
#include <HTList.h>
#endif /* HTLIST_H */

#ifdef VMS
#include <HTFTP.h>
#include <HTVMSUtils.h>
#define HTSYS_name(path)   HTVMS_name("", path)
#define HTSYS_purge(path)  HTVMS_purge(path)
#define HTSYS_remove(path) HTVMS_remove(path)
#endif /* VMS */

#if defined(USE_DOS_DRIVES)
#include <HTDOS.h>
#define HTSYS_name(path) HTDOS_name(path)
#endif

#ifndef HTSYS_name
#define HTSYS_name(path) path
#endif

#ifndef HTSYS_purge
#define HTSYS_purge(path) /*nothing*/
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
#endif /* __CYGWIN__ */
#endif /* DOSPATH */
#endif /* VMS */

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
#define LYIsDosDrive(s) FALSE /* really nothing */
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

extern BOOL strn_dash_equ PARAMS((CONST char* p1,CONST char* p2,int len));
extern BOOLEAN LYAddSchemeForURL PARAMS((char **AllocatedString, char *default_scheme));
extern BOOLEAN LYCachedTemp PARAMS((char *result, char **cached));
extern BOOLEAN LYCanDoHEAD PARAMS((CONST char *address));
extern BOOLEAN LYCanReadFile PARAMS((CONST char* name));
extern BOOLEAN LYCanWriteFile PARAMS((CONST char* name));
extern BOOLEAN LYCloseInput PARAMS((FILE * fp));
extern BOOLEAN LYCloseOutput PARAMS((FILE * fp));
extern BOOLEAN LYExpandHostForURL PARAMS((char **AllocatedString, char *prefix_list, char *suffix_list));
extern BOOLEAN LYFixCursesOnForAccess PARAMS((CONST char* addr, CONST char* physical));
extern BOOLEAN LYPathOffHomeOK PARAMS((char *fbuffer, size_t fbuffer_size));
extern BOOLEAN LYValidateFilename PARAMS((char * result, char * given));
extern BOOLEAN LYisAbsPath PARAMS((CONST char *path));
extern BOOLEAN LYisLocalAlias PARAMS((CONST char *filename));
extern BOOLEAN LYisLocalFile PARAMS((CONST char *filename));
extern BOOLEAN LYisLocalHost PARAMS((CONST char *filename));
extern BOOLEAN LYisRootPath PARAMS((CONST char *path));
extern BOOLEAN inlocaldomain NOPARAMS;
extern CONST char *Home_Dir NOPARAMS;
extern CONST char *index_to_restriction PARAMS((int inx));
extern CONST char *wwwName PARAMS((CONST char *pathname));
extern FILE *InternalPageFP PARAMS((char * filename, int reuse_flag));
extern FILE *LYAppendToTxtFile PARAMS((char * name));
extern FILE *LYNewBinFile PARAMS((char * name));
extern FILE *LYNewTxtFile PARAMS((char * name));
extern FILE *LYOpenScratch PARAMS((char *result, CONST char *prefix));
extern FILE *LYOpenTemp PARAMS((char *result, CONST char *suffix, CONST char *mode));
extern FILE *LYOpenTempRewrite PARAMS((char *result, CONST char *suffix, CONST char *mode));
extern FILE *LYReopenTemp PARAMS((char *name));
extern char *Current_Dir PARAMS((char * pathname));
extern char *LYAddPathToSave PARAMS((char *fname));
extern char *LYGetEnv PARAMS((CONST char * name));
extern char *LYGetHiliteStr PARAMS(( int cur, int count));
extern char *LYLastPathSep PARAMS((CONST char *path));
extern char *LYPathLeaf PARAMS((char * pathname));
extern char *LYSysShell NOPARAMS;
extern char *LYgetXDisplay NOPARAMS;
extern char *strip_trailing_slash PARAMS((char * my_dirname));
extern char *trimPoundSelector PARAMS((char * address));
extern int HTCheckForInterrupt NOPARAMS;
extern int LYCheckForProxyURL PARAMS((char *filename));
extern int LYConsoleInputFD PARAMS((BOOLEAN need_selectable));
extern int LYCopyFile PARAMS((char *src, char *dst));
extern int LYGetHilitePos PARAMS(( int cur, int count));
extern int LYRemoveTemp PARAMS((char *name));
extern int LYSystem PARAMS((char *command));
extern int LYValidateOutput PARAMS((char * filename));
extern int find_restriction PARAMS((CONST char * name, int len));
extern int is_url PARAMS((char *filename));
extern int number2arrows PARAMS((int number));
extern size_t utf8_length PARAMS((BOOL utf_flag, CONST char * data));
extern time_t LYmktime PARAMS((char *string, BOOL absolute));
extern void BeginInternalPage PARAMS((FILE *fp0, char *Title, char *HelpURL));
extern void EndInternalPage PARAMS((FILE *fp0));
extern void HTAddSugFilename PARAMS((char *fname));
extern void HTSugFilenames_free NOPARAMS;
extern void LYAddHilite PARAMS((int cur, char *text, int x));
extern void LYAddHtmlSep PARAMS((char **path));
extern void LYAddHtmlSep0 PARAMS((char *path));
extern void LYAddLocalhostAlias PARAMS((char *alias));
extern void LYAddPathSep PARAMS((char **path));
extern void LYAddPathSep0 PARAMS((char *path));
extern void LYAddPathToHome PARAMS((char *fbuffer, size_t fbuffer_size, char *fname));
extern void LYCheckBibHost NOPARAMS;
extern void LYCheckMail NOPARAMS;
extern void LYCleanupTemp NOPARAMS;
extern void LYCloseTemp PARAMS((char *name));
extern void LYCloseTempFP PARAMS((FILE *fp));
extern void LYConvertToURL PARAMS((char **AllocatedString, int fixit));
extern void LYDoCSI PARAMS((char *url, CONST char *comment, char **csi));
extern void LYEnsureAbsoluteURL PARAMS((char **href, CONST char *name, int fixit));
extern void LYFakeZap PARAMS((BOOL set));
extern void LYFixCursesOn PARAMS((CONST char* reason));
extern void LYLocalFileToURL PARAMS((char **target, CONST char *source));
extern void LYLocalhostAliases_free NOPARAMS;
extern void LYRenamedTemp PARAMS((char * oldname, char * newname));
extern void LYSetHilite PARAMS((int cur, char *text));
extern void LYTrimHtmlSep PARAMS((char *path));
extern void LYTrimPathSep PARAMS((char *path));
extern void LYTrimRelFromAbsPath PARAMS((char *path));
extern void LYhighlight PARAMS((int flag, int cur, char *target));
extern void LYsetXDisplay PARAMS((char *new_display));
extern void change_sug_filename PARAMS((char *fname));
extern void convert_to_spaces PARAMS((char *string, BOOL condense));
extern void free_and_clear PARAMS((char **obj));
extern void noviceline PARAMS((int more_flag));
extern void parse_restrictions PARAMS((CONST char *s));
extern void print_restrictions_to_fd PARAMS((FILE *fp));
extern void remove_backslashes PARAMS((char *buf));
extern void size_change PARAMS((int sig));
extern void statusline PARAMS((CONST char *text));
extern void toggle_novice_line NOPARAMS;

#ifdef EXP_ASCII_CTYPES
extern int ascii_tolower PARAMS((int i));
extern int ascii_toupper PARAMS((int i));
extern int ascii_isupper PARAMS((int i));
#endif

#ifdef __CYGWIN__
extern int Cygwin_Shell PARAMS((void));
#endif

#ifdef _WIN_CC
extern int exec_command(char * cmd, int wait_flag); /* xsystem.c */
extern int xsystem(char *cmd);
#endif

/* Keeping track of User Interface Pages: */
typedef enum {
    UIP_UNKNOWN=-1
  , UIP_HISTORY=0
  , UIP_DOWNLOAD_OPTIONS
  , UIP_PRINT_OPTIONS
  , UIP_SHOWINFO
  , UIP_LIST_PAGE
  , UIP_VLINKS
  , UIP_LYNXCFG
  , UIP_OPTIONS_MENU
  , UIP_DIRED_MENU
  , UIP_PERMIT_OPTIONS
  , UIP_UPLOAD_OPTIONS
  , UIP_ADDRLIST_PAGE
  , UIP_CONFIG_DEF
  , UIP_TRACELOG
  , UIP_INSTALL
} UIP_t;

#define UIP_P_FRAG 0x0001   /* flag: consider "url#frag" as matching "url" */

extern BOOL LYIsUIPage3 PARAMS((CONST char * url, UIP_t type, int flagparam));
#define LYIsUIPage(url,type) LYIsUIPage3(url, type, UIP_P_FRAG)
extern void LYRegisterUIPage PARAMS((CONST char * url, UIP_t type));
#define LYUnRegisterUIPage(type) LYRegisterUIPage(NULL, type)
extern void LYUIPages_free NOPARAMS;

#ifdef CAN_CUT_AND_PASTE
extern int put_clip PARAMS((char *szBuffer));
/* get_clip_grab() returns a pointer to the string in the system area.
   get_clip_release() should be called ASAP after this. */
extern char* get_clip_grab NOPARAMS;
extern void  get_clip_release NOPARAMS;
#  ifdef WIN_EX
#    define size_clip()	8192
#  else
extern int size_clip NOPARAMS;
#  endif
#endif

#if defined(WIN_EX)	/* 1997/10/16 (Thu) 20:13:28 */
extern char *HTDOS_short_name(char *path);
extern char *w32_strerror(DWORD ercode);
#endif

#ifdef VMS
extern void Define_VMSLogical PARAMS((char *LogicalName, char *LogicalValue));
#endif /* VMS */

#if ! HAVE_PUTENV
extern int putenv PARAMS((CONST char *string));
#endif /* HAVE_PUTENV */

#if defined(MULTI_USER_UNIX)
extern void LYRelaxFilePermissions PARAMS((CONST char * name));
#else
#define LYRelaxFilePermissions(name) /* nothing */
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

/*
 *  For change_sug_filename().
 */
extern HTList *sug_filenames;

/*
 * syslog() facility
 */
#if !defined(VMS) && defined(SYSLOG_REQUESTED_URLS)
#include <syslog.h>

extern void LYOpenlog  PARAMS((CONST char *banner));
extern void LYSyslog   PARAMS((char *arg));
extern void LYCloselog NOPARAMS;

#endif /* !VMS && SYSLOG_REQUESTED_URLS */

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

#endif /* LYUTILS_H */
