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

#if defined(DOSPATH) || defined(__EMX__)
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

/* See definitions in src/LYCharVals.h.  The hardcoded values...
   This prohibits binding C-c and C-g.  Maybe it is better to remove this? */
#define LYCharIsINTERRUPT_HARD(ch)	\
  ((ch) == LYCharINTERRUPT1 || ch == LYCharINTERRUPT2)

#define LYCharIsINTERRUPT(ch)		\
  (LYCharIsINTERRUPT_HARD(ch) || LKC_TO_LAC(keymap,ch) == LYK_INTERRUPT)

#define LYCharIsINTERRUPT_NO_letter(ch)	\
  (LYCharIsINTERRUPT(ch) && !isprint(ch))

#if defined(DOSPATH) || defined(__EMX__)
#define LYIsPathSep(ch) ((ch) == '/' || (ch) == '\\')
#else
#define LYIsPathSep(ch) ((ch) == '/')
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
extern BOOLEAN LYisLocalAlias PARAMS((char *filename));
extern BOOLEAN LYisLocalFile PARAMS((char *filename));
extern BOOLEAN LYisLocalHost PARAMS((char *filename));
extern BOOLEAN LYisRootPath PARAMS((char *path));
extern BOOLEAN inlocaldomain NOPARAMS;
extern CONST char *Home_Dir NOPARAMS;
extern CONST char *index_to_restriction PARAMS(( int	inx));
extern FILE *LYAppendToTxtFile PARAMS((char * name));
extern FILE *LYNewBinFile PARAMS((char * name));
extern FILE *LYNewTxtFile PARAMS((char * name));
extern FILE *LYOpenScratch PARAMS((char *result, CONST char *prefix));
extern FILE *LYOpenTemp PARAMS((char *result, CONST char *suffix, CONST char *mode));
extern FILE *LYOpenTempRewrite PARAMS((char *result, CONST char *suffix, CONST char *mode));
extern FILE *LYReopenTemp PARAMS((char *name));
extern char *Current_Dir PARAMS((char * pathname));
extern char *LYLastPathSep PARAMS((CONST char *path));
extern char *LYPathLeaf PARAMS((char * pathname));
extern char *LYSysShell NOPARAMS;
extern char *LYgetXDisplay NOPARAMS;
extern char *strip_trailing_slash PARAMS((char * my_dirname));
extern char *wwwName PARAMS((CONST char *pathname));
extern int HTCheckForInterrupt NOPARAMS;
extern int LYCheckForProxyURL PARAMS((char *filename));
extern int LYConsoleInputFD PARAMS((BOOLEAN need_selectable));
extern int LYCopyFile PARAMS((char *src, char *dst));
extern int LYRemoveTemp PARAMS((char *name));
extern int LYSystem PARAMS((char *command));
extern int LYValidateOutput PARAMS((char * filename));
extern int find_restriction PARAMS((CONST char * name, int len));
extern int is_url PARAMS((char *filename));
extern int number2arrows PARAMS((int number));
extern time_t LYmktime PARAMS((char *string, BOOL absolute));
extern void BeginInternalPage PARAMS((FILE *fp0, char *Title, char *HelpURL));
extern void EndInternalPage PARAMS((FILE *fp0));
extern void HTAddSugFilename PARAMS((char *fname));
extern void HTSugFilenames_free NOPARAMS;
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
extern void LYTrimHtmlSep PARAMS((char *path));
extern void LYTrimPathSep PARAMS((char *path));
extern void LYTrimRelFromAbsPath PARAMS((char *path));
extern void LYsetXDisplay PARAMS((char *new_display));
extern void change_sug_filename PARAMS((char *fname));
extern void convert_to_spaces PARAMS((char *string, BOOL condense));
extern void free_and_clear PARAMS((char **obj));
extern void highlight PARAMS((int flag, int cur, char *target));
extern void noviceline PARAMS((int more_flag));
extern void parse_restrictions PARAMS((CONST char *s));
extern void print_restrictions_to_fd PARAMS((FILE *fp));
extern void remove_backslashes PARAMS((char *buf));
extern void size_change PARAMS((int sig));
extern void statusline PARAMS((CONST char *text));
extern void toggle_novice_line NOPARAMS;

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
extern int put_clip(char *szBuffer);
/* get_clip_grab() returns a pointer to the string in the system area.
   get_clip_release() should be called ASAP after this. */
extern char* get_clip_grab(void);
extern void  get_clip_release(void);
#  ifdef WIN_EX
#    define size_clip()	8192
#  else
extern int size_clip();
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

#ifdef UNIX
extern void LYRelaxFilePermissions PARAMS((CONST char * name));
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

/*
 *  For change_sug_filename().
 */
extern HTList *sug_filenames;

/*
 * syslog() facility
 */
#if !defined(VMS) && defined(SYSLOG_REQUESTED_URLS)
#ifdef WATT32
#include <sys/syslog.h>
#else
#include <syslog.h>
#endif

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

#if defined(DOSPATH) || defined(WIN_EX) || defined(__CYGWIN__)
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
