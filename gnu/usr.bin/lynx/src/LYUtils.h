
#ifndef LYUTILS_H
#define LYUTILS_H

#include <stdio.h>

#ifndef HTLIST_H
#include "HTList.h"
#endif /* HTLIST_H */

extern void highlight PARAMS((int flag, int cur, char *target));
extern void free_and_clear PARAMS((char **obj));
extern void collapse_spaces PARAMS((char *string));
extern void convert_to_spaces PARAMS((char *string, BOOL condense));
extern char * strip_trailing_slash PARAMS((char * dirname));
extern void statusline PARAMS((CONST char *text));
extern void toggle_novice_line NOPARAMS;
extern void noviceline PARAMS((int more_flag));
extern void LYFakeZap PARAMS((BOOL set));
extern int HTCheckForInterrupt NOPARAMS;
extern BOOLEAN LYisLocalFile PARAMS((char *filename));
extern BOOLEAN LYisLocalHost PARAMS((char *filename));
extern void LYLocalhostAliases_free NOPARAMS;
extern void LYAddLocalhostAlias PARAMS((char *alias));
extern BOOLEAN LYisLocalAlias PARAMS((char *filename));
extern int LYCheckForProxyURL PARAMS((char *filename));
extern int is_url PARAMS((char *filename));
extern BOOLEAN LYCanDoHEAD PARAMS((CONST char *address));
extern void remove_backslashes PARAMS((char *buf));
extern char *quote_pathname PARAMS((char *pathname));
extern BOOLEAN inlocaldomain NOPARAMS;
extern void size_change PARAMS((int sig));
extern void HTSugFilenames_free NOPARAMS;
extern void HTAddSugFilename PARAMS((char *fname));
extern void change_sug_filename PARAMS((char *fname));
extern void tempname PARAMS((char *namebuffer, int action));
extern int number2arrows PARAMS((int number));
extern void parse_restrictions PARAMS((char *s));
extern void checkmail NOPARAMS;
extern int LYCheckMail NOPARAMS;
extern void LYEnsureAbsoluteURL PARAMS((char **href, char *name));
extern void LYConvertToURL PARAMS((char **AllocatedString));
extern BOOLEAN LYExpandHostForURL PARAMS((
	char **AllocatedString, char *prefix_list, char *suffix_list));
extern BOOLEAN LYAddSchemeForURL PARAMS((
	char **AllocatedString, char *default_scheme));
extern void LYTrimRelFromAbsPath PARAMS((char *path));
extern void LYDoCSI PARAMS((char *url, CONST char *comment, char **csi));
#ifdef VMS
extern void Define_VMSLogical PARAMS((
	char *LogicalName, char *LogicalValue));
#endif /* VMS */
extern CONST char *Home_Dir NOPARAMS;
extern BOOLEAN LYPathOffHomeOK PARAMS((char *fbuffer, size_t fbuffer_size));
extern void LYAddPathToHome PARAMS((
	char *fbuffer, size_t fbuffer_size, char *fname));
extern time_t LYmktime PARAMS((char *string, BOOL absolute));
#if ! HAVE_PUTENV
extern int putenv PARAMS((CONST char *string));
#endif /* HAVE_PUTENV */

FILE *LYNewBinFile PARAMS((char * name));
FILE *LYNewTxtFile PARAMS((char * name));
FILE *LYAppendToTxtFile PARAMS((char * name));
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
 *  Universal document id types.
 */
#define HTTP_URL_TYPE		 1
#define FILE_URL_TYPE		 2
#define FTP_URL_TYPE		 3
#define WAIS_URL_TYPE		 4
#define NEWS_URL_TYPE		 5
#define NNTP_URL_TYPE		 6
#define TELNET_URL_TYPE		 7
#define TN3270_URL_TYPE		 8
#define RLOGIN_URL_TYPE		 9
#define GOPHER_URL_TYPE		10
#define HTML_GOPHER_URL_TYPE	11
#define TELNET_GOPHER_URL_TYPE	12
#define INDEX_GOPHER_URL_TYPE	13
#define MAILTO_URL_TYPE		14
#define FINGER_URL_TYPE		15
#define CSO_URL_TYPE		16
#define HTTPS_URL_TYPE		17
#define SNEWS_URL_TYPE		18
#define PROSPERO_URL_TYPE	19
#define AFS_URL_TYPE		20

#define DATA_URL_TYPE		21

#define LYNXEXEC_URL_TYPE	22
#define LYNXPROG_URL_TYPE	23
#define LYNXCGI_URL_TYPE	24

#define NEWSPOST_URL_TYPE	25
#define NEWSREPLY_URL_TYPE	26
#define SNEWSPOST_URL_TYPE	27
#define SNEWSREPLY_URL_TYPE	28

#define LYNXPRINT_URL_TYPE	29
#define LYNXHIST_URL_TYPE	30
#define LYNXDOWNLOAD_URL_TYPE	31
#define LYNXKEYMAP_URL_TYPE	32
#define LYNXIMGMAP_URL_TYPE	33
#define LYNXCOOKIE_URL_TYPE	34
#define LYNXDIRED_URL_TYPE	35

#define PROXY_URL_TYPE		36

#define UNKNOWN_URL_TYPE	37

/*
 *  For change_sug_filename().
 */
extern HTList *sug_filenames;

/*
 *  For tempname().
 */
#define NEW_FILE     0
#define REMOVE_FILES 1

/*
 *  Miscellaneous.
 */
#define ON      1
#define OFF     0
#define STREQ(a,b) (strcmp(a,b) == 0)
#define STRNEQ(a,b,c) (strncmp(a,b,c) == 0)

#define HIDE_CHMOD 0600
#define HIDE_UMASK 0077

#endif /* LYUTILS_H */
