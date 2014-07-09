/*
 * $LynxId: LYLocal.c,v 1.127 2013/11/28 11:19:31 tom Exp $
 *
 *  Routines to manipulate the local filesystem.
 *  Written by: Rick Mallett, Carleton University
 *  Report problems to rmallett@ccs.carleton.ca
 *  Modified 18-Dec-95 David Trueman (david@cs.dal.ca):
 *	Added OK_PERMIT compilation option.
 *	Support replacement of compiled-in f)ull menu configuration via
 *	  DIRED_MENU definitions in lynx.cfg, so that more than one menu
 *	  can be driven by the same executable.
 *  Modified Oct-96 Klaus Weide (kweide@tezcat.com):
 *	Changed to use the library's HTList_* functions and macros for
 *	  managing the list of tagged file URLs.
 *	Keep track of proper level of URL escaping, so that unusual filenames
 *	  which contain #% etc. are handled properly (some HTUnEscapeSome()'s
 *	  left in to be conservative, and to document where superfluous
 *	  unescaping took place before).
 *	Dynamic memory instead of fixed length buffers in a few cases.
 *	Other minor changes to make things work as intended.
 *  Modified Jun-97 Klaus Weide (kweide@tezcat.com) & FM:
 *	Modified the code handling DIRED_MENU to do more careful
 *	  checking of the selected file.  In addition to "TAG", "FILE", and
 *	  "DIR", DIRED_MENU definitions in lynx.cfg now also recognize LINK as
 *	  a type.  DIRED_MENU definitions with a type field of "LINK" are only
 *	  used if the current selection is a symbolic link ("FILE" and "DIR"
 *	  definitions are not used in that case).  The default menu
 *	  definitions have been updated to reflect this change, and to avoid
 *	  the showing of menu items whose action would always fail - KW
 *	Cast all code into the Lynx programming style. - FM
 */

#include <HTUtils.h>
#include <HTAAProt.h>
#include <HTFile.h>
#include <HTAlert.h>
#include <HTParse.h>
#include <LYCurses.h>
#include <LYGlobalDefs.h>
#include <LYUtils.h>
#include <LYStrings.h>
#include <LYCharUtils.h>
#include <LYStructs.h>
#include <LYHistory.h>
#include <LYUpload.h>
#include <LYLocal.h>
#include <LYClean.h>
#include <www_wait.h>

#ifdef SUPPORT_CHDIR
#include <LYMainLoop.h>
#endif

#include <LYLeaks.h>

#undef USE_COMPRESS

#ifdef __DJGPP__
#define EXT_TAR_GZ ".tgz"
#define EXT_TAR_Z  ".taz"
#define EXT_Z      ".z"
#else
#define EXT_TAR_GZ ".tar.gz"
#define EXT_TAR_Z  ".tar.Z"
#define EXT_Z      ".Z"
#endif

#ifndef DIRED_MAXBUF
#define DIRED_MAXBUF 512
#endif

#ifdef DIRED_SUPPORT

#ifdef OK_INSTALL
#ifdef FNAMES_8_3
#define INSTALLDIRS_FILE "instdirs.htm"
#else
#define INSTALLDIRS_FILE ".installdirs.html"
#endif /* FNAMES_8_3 */
#endif /* OK_INSTALL */

static int get_filename(const char *prompt,
			bstring *buf);

#ifdef OK_PERMIT
static int permit_location(char *destpath,
			   char *srcpath,
			   char **newpath);
#endif /* OK_PERMIT */
/* *INDENT-OFF* */
static char *render_item ( const char *	s,
	const char *	path,
	const char *	dir,
	char *		buf,
	size_t		bufsize,
	int		url_syntax);

struct dired_menu {
    int cond;
#define DE_TAG     1
#define DE_DIR     2
#define DE_FILE    3
#define DE_SYMLINK 4
    char *sfx;
    const char *c_sfx;
    char *link;
    const char *c_link;
    char *rest;
    const char *c_rest;
    char *href;
    const char *c_href;
    struct dired_menu *next;
};

#define GetDiredSuffix(p) ((p)->sfx  ? (p)->sfx  : (p)->c_sfx)
#define GetDiredLink(p)   ((p)->link ? (p)->link : (p)->c_link)
#define GetDiredRest(p)   ((p)->rest ? (p)->rest : (p)->c_rest)
#define GetDiredHref(p)   ((p)->href ? (p)->href : (p)->c_href)

#undef DATA
#define DATA(cond, sfx, link, rest, href) { \
	cond, \
	NULL, sfx, \
	NULL, link, \
	NULL, rest, \
	NULL, href, \
	NULL }

static struct dired_menu *menu_head = NULL;
static struct dired_menu defmenu[] = {

/*
 * The following initializations determine the contents of the f)ull menu
 * selection when in dired mode.  If any menu entries are defined in the
 * configuration file via DIRED_MENU lines, then these default entries are
 * discarded entirely.
 */
#ifdef SUPPORT_CHDIR
DATA( 0,              "", "Change directory",
                      "", "LYNXDIRED://CHDIR"),
#endif
DATA( 0,              "", "New File",
"(in current directory)", "LYNXDIRED://NEW_FILE%d"),

DATA( 0,              "", "New Directory",
"(in current directory)", "LYNXDIRED://NEW_FOLDER%d"),

#ifdef OK_INSTALL
DATA( DE_FILE,        "", "Install",
"selected file to new location", "LYNXDIRED://INSTALL_SRC%p"),
/* The following (installing a directory) doesn't work for me, at least
   with the "install" from GNU fileutils 4.0.  I leave it in anyway, in
   case one compiles with INSTALL_PATH / INSTALL_ARGS defined to some
   other command for which it works (like a script, or maybe "cp -a"). - kw
*/
DATA( DE_DIR,         "", "Install",
"selected directory to new location", "LYNXDIRED://INSTALL_SRC%p"),
#endif /* OK_INSTALL */

DATA( DE_FILE,        "", "Modify File Name",
"(of current selection)", "LYNXDIRED://MODIFY_NAME%p"),
DATA( DE_DIR,         "", "Modify Directory Name",
"(of current selection)", "LYNXDIRED://MODIFY_NAME%p"),
#ifdef S_IFLNK
DATA( DE_SYMLINK,     "", "Modify Name",
"(of selected symbolic link)", "LYNXDIRED://MODIFY_NAME%p"),
#endif  /* S_IFLNK */

#ifdef OK_PERMIT
DATA( DE_FILE,        "", "Modify File Permissions",
"(of current selection)", "LYNXDIRED://PERMIT_SRC%p"),
DATA( DE_DIR,         "", "Modify Directory Permissions",
"(of current selection)", "LYNXDIRED://PERMIT_SRC%p"),
#endif /* OK_PERMIT */

DATA( DE_FILE,        "", "Change Location",
"(of selected file)"    , "LYNXDIRED://MODIFY_LOCATION%p"),
DATA( DE_DIR,         "", "Change Location",
"(of selected directory)", "LYNXDIRED://MODIFY_LOCATION%p"),
#ifdef S_IFLNK
DATA( DE_SYMLINK,     "", "Change Location",
"(of selected symbolic link)", "LYNXDIRED://MODIFY_LOCATION%p"),
#endif /* S_IFLNK */

DATA( DE_FILE,        "", "Remove File",
   "(current selection)", "LYNXDIRED://REMOVE_SINGLE%p"),
DATA( DE_DIR,         "", "Remove Directory",
   "(current selection)", "LYNXDIRED://REMOVE_SINGLE%p"),
#ifdef S_IFLNK
DATA( DE_SYMLINK,     "", "Remove Symbolic Link",
   "(current selection)", "LYNXDIRED://REMOVE_SINGLE%p"),
#endif /* S_IFLNK */

#if defined(OK_UUDECODE) && !defined(ARCHIVE_ONLY)
DATA( DE_FILE,        "", "UUDecode",
   "(current selection)", "LYNXDIRED://UUDECODE%p"),
#endif /* OK_UUDECODE && !ARCHIVE_ONLY */

#if defined(OK_TAR) && !defined(ARCHIVE_ONLY)
DATA( DE_FILE,        EXT_TAR_Z, "Expand",
   "(current selection)", "LYNXDIRED://UNTAR_Z%p"),
#endif /* OK_TAR && !ARCHIVE_ONLY */

#if defined(OK_TAR) && defined(OK_GZIP) && !defined(ARCHIVE_ONLY)
DATA( DE_FILE,        ".tar.gz", "Expand",
   "(current selection)", "LYNXDIRED://UNTAR_GZ%p"),

DATA( DE_FILE,        ".tgz", "Expand",
   "(current selection)", "LYNXDIRED://UNTAR_GZ%p"),
#endif /* OK_TAR && OK_GZIP && !ARCHIVE_ONLY */

#ifndef ARCHIVE_ONLY
DATA( DE_FILE,        EXT_Z, "Uncompress",
   "(current selection)", "LYNXDIRED://DECOMPRESS%p"),
#endif /* ARCHIVE_ONLY */

#if defined(OK_GZIP) && !defined(ARCHIVE_ONLY)
DATA( DE_FILE,        ".gz", "Uncompress",
   "(current selection)", "LYNXDIRED://UNGZIP%p"),
#endif /* OK_GZIP && !ARCHIVE_ONLY */

#if defined(OK_ZIP) && !defined(ARCHIVE_ONLY)
DATA( DE_FILE,        ".zip", "Uncompress",
   "(current selection)", "LYNXDIRED://UNZIP%p"),
#endif /* OK_ZIP && !ARCHIVE_ONLY */

#if defined(OK_TAR) && !defined(ARCHIVE_ONLY)
DATA( DE_FILE,        ".tar", "UnTar",
   "(current selection)", "LYNXDIRED://UNTAR%p"),
#endif /* OK_TAR && !ARCHIVE_ONLY */

#ifdef OK_TAR
DATA( DE_DIR,         "", "Tar",
   "(current selection)", "LYNXDIRED://TAR%p"),
#endif /* OK_TAR */

#if defined(OK_TAR) && defined(OK_GZIP)
DATA( DE_DIR,         "", "Tar and compress",
      "(using GNU gzip)", "LYNXDIRED://TAR_GZ%p"),
#endif /* OK_TAR && OK_GZIP */

#if defined(OK_TAR) && defined(USE_COMPRESS)
DATA( DE_DIR,         "", "Tar and compress",
      "(using compress)", "LYNXDIRED://TAR_Z%p"),
#endif /* OK_TAR && USE_COMPRESS */

#ifdef OK_ZIP
DATA( DE_DIR,         "", "Package and compress",
           "(using zip)", "LYNXDIRED://ZIP%p"),
#endif /* OK_ZIP */

DATA( DE_FILE,        "", "Compress",
 "(using Unix compress)", "LYNXDIRED://COMPRESS%p"),

#ifdef OK_GZIP
DATA( DE_FILE,        "", "Compress",
          "(using gzip)", "LYNXDIRED://GZIP%p"),
#endif /* OK_GZIP */

#ifdef OK_ZIP
DATA( DE_FILE,        "", "Compress",
           "(using zip)", "LYNXDIRED://ZIP%p"),
#endif /* OK_ZIP */

DATA( DE_TAG,         "", "Move all tagged items to another location.",
                      "", "LYNXDIRED://MOVE_TAGGED%d"),

#ifdef OK_INSTALL
DATA( DE_TAG,         "", "Install tagged files into another directory.",
                      "", "LYNXDIRED://INSTALL_SRC%00"),
#endif

DATA( DE_TAG,         "", "Remove all tagged files and directories.",
                      "", "LYNXDIRED://REMOVE_TAGGED"),

DATA( DE_TAG,         "", "Untag all tagged files and directories.",
                      "", "LYNXDIRED://CLEAR_TAGGED"),

DATA( 0,              NULL, NULL,
                      NULL, NULL),
};
#undef DATA
/* *INDENT-ON* */

static BOOLEAN cannot_stat(const char *name)
{
    char *tmpbuf = 0;

    HTSprintf0(&tmpbuf, gettext("Unable to get status of '%s'."), name);
    HTAlert(tmpbuf);
    FREE(tmpbuf);
    return FALSE;
}

#define OK_STAT(name, sb) (stat(name, sb) == 0)

static BOOLEAN ok_stat(const char *name, struct stat *sb)
{
    BOOLEAN rc = TRUE;

    CTRACE((tfp, "testing ok_stat(%s)\n", name));
    if (!OK_STAT(name, sb)) {
#ifdef DOSPATH
	size_t len = strlen(name);

	/*
	 * If a path ends with '\' or ':', we can guess that it may be
	 * a directory name.  Adding a '.' (after a '\') will produce a
	 * pathname that stat() will accept as a directory name.
	 */
	if (len != 0 && (name[len - 1] == '\\' || name[len - 1] == ':')) {
	    char *temp = malloc(len + 3);

	    if (temp != 0) {
		strcpy(temp, name);
		if (temp[len - 1] == '\\') {
		    strcpy(temp + len, ".");
		} else {
		    strcpy(temp + len, "\\.");
		}
		rc = OK_STAT(temp, sb);
		free(temp);
	    } else {
		rc = FALSE;
	    }
	} else
#endif
	    rc = FALSE;
    }

    if (rc == FALSE)
	rc = cannot_stat(name);

    return rc;
}

#ifdef HAVE_LSTAT
static BOOLEAN ok_lstat(char *name, struct stat *sb)
{
    CTRACE((tfp, "testing ok_lstat(%s)\n", name));
    if (lstat(name, sb) < 0) {
	return cannot_stat(name);
    }
    return TRUE;
}
#else
#define ok_lstat(name,sb) ok_stat(name,sb)
#endif

static BOOLEAN ok_file_or_dir(struct stat *sb)
{
    if (!S_ISDIR(sb->st_mode)
	&& !S_ISREG(sb->st_mode)) {
	HTAlert(gettext("The selected item is not a file or a directory!  Request ignored."));
	return FALSE;
    }
    return TRUE;
}

#ifdef OK_INSTALL		/* currently only used in local_install */
static BOOLEAN ok_localname(char *dst, const char *src)
{
    struct stat dir_info;

    if (!ok_stat(src, &dir_info)
	|| !ok_file_or_dir(&dir_info)) {
	return FALSE;
    }
    if (strlen(src) >= DIRED_MAXBUF) {
	CTRACE((tfp, "filename too long in ok_localname!\n"));
	return FALSE;
    }
    strcpy(dst, src);
    return TRUE;
}
#endif /* OK_INSTALL */

#define MAX_ARGC 10

static char **make_argv(const char *command,...)
{
    static char *result[MAX_ARGC];
    int argc = 0;
    char *value;
    va_list ap;

    va_start(ap, command);
    result[0] = 0;
    StrAllocCopy(result[argc++], command);
    do {
	result[argc] = 0;
	value = (char *) va_arg(ap, char *);

	if (value != 0)
	    StrAllocCopy(result[argc], value);
    } while (result[argc++] != 0);
    va_end(ap);

    return result;
}

static void free_argv(char **argv)
{
    int argc;

    for (argc = 0; argv[argc] != 0; ++argc) {
	free(argv[argc]);
    }
}

/*
 * Execute DIRED command, return -1 or 0 on failure, 1 success.
 */
static int LYExecv(const char *path,
		   char **argv,
		   char *msg)
{
    int rc = 0;

#if defined(VMS)
    CTRACE((tfp, "LYExecv:  Called inappropriately! (path=%s)\n", path));
#else
    int n;
    char *tmpbuf = 0;

#if defined(__DJGPP__) || defined(_WINDOWS)
    stop_curses();
    HTSprintf0(&tmpbuf, "%s", path);
    for (n = 1; argv[n] != 0; n++)
	HTSprintf(&tmpbuf, " %s", argv[n]);
    HTSprintf(&tmpbuf, "\n");
    rc = LYSystem(tmpbuf) ? 0 : 1;
#else
    int pid;

#ifdef HAVE_TYPE_UNIONWAIT
    union wait wstatus;

#else
    int wstatus;
#endif

    if (TRACE) {
	CTRACE((tfp, "LYExecv path='%s'\n", path));
	for (n = 0; argv[n] != 0; n++)
	    CTRACE((tfp, "argv[%d] = '%s'\n", n, argv[n]));
    }

    rc = 1;			/* It will work */
    stop_curses();
    pid = fork();		/* fork and execute command */

    switch (pid) {
    case -1:
	HTSprintf0(&tmpbuf, gettext("Unable to %s due to system error!"), msg);
	rc = 0;
	break;			/* don't fall thru! - KW */

    case 0:			/* child */
#ifdef USE_EXECVP
	execvp(path, argv);	/* this uses our $PATH */
#else
	execv(path, argv);
#endif
	exit(EXIT_FAILURE);	/* execv failed, give wait() something to look at */
	/*NOTREACHED */

    default:			/* parent */
#if !HAVE_WAITPID
	while (wait(&wstatus) != pid) ;		/* do nothing */
#else
	while (-1 == waitpid(pid, &wstatus, 0)) {	/* wait for child */
#ifdef EINTR
	    if (errno == EINTR)
		continue;
#endif /* EINTR */
#ifdef ERESTARTSYS
	    if (errno == ERESTARTSYS)
		continue;
#endif /* ERESTARTSYS */
	    break;
	}
#endif /* !HAVE_WAITPID */
	if ((WIFEXITED(wstatus)
	     && (WEXITSTATUS(wstatus) != 0))
	    || (WIFSIGNALED(wstatus)
		&& (WTERMSIG(wstatus) > 0))) {	/* error return */
	    HTSprintf0(&tmpbuf,
		       gettext("Probable failure to %s due to system error!"),
		       msg);
	    rc = 0;
	}
    }
#endif /* __DJGPP__ */

    if (rc == 0) {
	/*
	 * Screen may have message from the failed execv'd command.  Give user
	 * time to look at it before screen refresh.
	 */
	LYSleepAlert();
    }
    start_curses();
    if (tmpbuf != 0) {
	if (rc == 0)
	    HTAlert(tmpbuf);
	FREE(tmpbuf);
    }
#endif /* VMS || _WINDOWS */
    CTRACE((tfp, "LYexecv ->%d\n", rc));
    return (rc);
}

static int make_directory(char *path)
{
    int code;
    const char *program;

    if ((program = HTGetProgramPath(ppMKDIR)) != NULL) {
	char **args;
	char *msg = 0;

	HTSprintf0(&msg, "make directory %s", path);
	args = make_argv("mkdir",
			 path,
			 NULL);
	code = (LYExecv(program, args, msg) <= 0) ? -1 : 1;
	FREE(msg);
	free_argv(args);
    } else {
#ifdef _WINDOWS
	code = mkdir(path) ? -1 : 1;
#else
	code = mkdir(path, 0777) ? -1 : 1;
#endif
	CTRACE((tfp, "builtin mkdir ->%d\n\t%s\n", code, path));
    }
    return (code);
}

static int remove_file(char *path)
{
    int code;
    const char *program;

    if ((program = HTGetProgramPath(ppRM)) != NULL) {
	char **args;
	char *tmpbuf = NULL;

	args = make_argv("rm",
			 "-f",
			 path,
			 NULL);
	HTSprintf0(&tmpbuf, gettext("remove %s"), path);
	code = LYExecv(program, args, tmpbuf);
	FREE(tmpbuf);
	free_argv(args);
    } else {
	code = remove(path) ? -1 : 1;
	CTRACE((tfp, "builtin remove ->%d\n\t%s\n", code, path));
    }
    return (code);
}

static int remove_directory(char *path)
{
    int code;
    const char *program;

    if ((program = HTGetProgramPath(ppRMDIR)) != NULL) {
	char **args;
	char *tmpbuf = NULL;

	args = make_argv("rmdir",
			 path,
			 NULL);
	HTSprintf0(&tmpbuf, gettext("remove %s"), path);
	code = LYExecv(program, args, tmpbuf);
	FREE(tmpbuf);
	free_argv(args);
    } else {
	code = rmdir(path) ? -1 : 1;
	CTRACE((tfp, "builtin rmdir ->%d\n\t%s\n", code, path));
    }
    return (code);
}

static int touch_file(char *path)
{
    int code;
    const char *program;

    if ((program = HTGetProgramPath(ppTOUCH)) != NULL) {
	char **args;
	char *msg = NULL;

	HTSprintf0(&msg, gettext("touch %s"), path);
	args = make_argv("touch",
			 path,
			 NULL);
	code = (LYExecv(program, args, msg) <= 0) ? -1 : 1;
	FREE(msg);
	free_argv(args);
    } else {
	FILE *fp;

	if ((fp = fopen(path, BIN_W)) != 0) {
	    fclose(fp);
	    code = 1;
	} else {
	    code = -1;
	}
	CTRACE((tfp, "builtin touch ->%d\n\t%s\n", code, path));
    }
    return (code);
}

static int move_file(char *source, char *target)
{
    int code;
    const char *program;

    if ((program = HTGetProgramPath(ppMV)) != NULL) {
	char *msg = 0;
	char **args;

	HTSprintf0(&msg, gettext("move %s to %s"), source, target);
	args = make_argv("mv",
			 source,
			 target,
			 NULL);
	code = (LYExecv(program, args, msg) <= 0) ? -1 : 1;
	FREE(msg);
	free_argv(args);
    } else {
	struct stat sb;
	char *actual = 0;

	/* the caller sets up a target directory; we need a file path */
	if (stat(target, &sb) == 0
	    && S_ISDIR(sb.st_mode)) {
	    HTSprintf0(&actual, "%s/%s", target, LYPathLeaf(source));
	    CTRACE((tfp, "move_file source=%s, target=%s\n", source, target));
	    target = actual;
	}
	code = rename(source, target);
	CTRACE((tfp, "builtin move ->%d\n\tsource=%s\n\ttarget=%s\n",
		code, source, target));
	if (code != 0) {	/* it failed */
	    if ((code = LYCopyFile(source, target)) >= 0) {
		code = remove(source);
		CTRACE((tfp, "...remove source after copying ->%d\n", code));
	    }
	}
	if (code == 0)
	    code = 1;
	if (actual != target) {
	    FREE(actual);
	}
    }
    return code;
}

static BOOLEAN not_already_exists(char *name)
{
    struct stat dir_info;

    if (!OK_STAT(name, &dir_info)) {
	if (errno != ENOENT) {
	    cannot_stat(name);
	} else {
	    return TRUE;
	}
    } else if (S_ISDIR(dir_info.st_mode)) {
	HTAlert(gettext("There is already a directory with that name!  Request ignored."));
    } else if (S_ISREG(dir_info.st_mode)) {
	HTAlert(gettext("There is already a file with that name!  Request ignored."));
    } else {
	HTAlert(gettext("The specified name is already in use!  Request ignored."));
    }
    return FALSE;
}

static BOOLEAN dir_has_same_owner(struct stat *dst_info,
				  struct stat *src_info)
{
    if (S_ISDIR(dst_info->st_mode)) {
	if (dst_info->st_uid == src_info->st_uid) {
	    return TRUE;
	} else {
	    HTAlert(gettext("Destination has different owner!  Request denied."));
	}
    } else {
	HTAlert(gettext("Destination is not a valid directory!  Request denied."));
    }
    return FALSE;
}

/*
 * Make sure the source and target are not the same location.
 */
static BOOLEAN same_location(struct stat *dst_info,
			     struct stat *src_info)
{
    BOOLEAN result = FALSE;

#ifdef UNIX
    if (src_info->st_dev == dst_info->st_dev &&
	src_info->st_ino == dst_info->st_ino) {
	HTAlert(gettext("Source and destination are the same location!  Request ignored!"));
	result = TRUE;
    }
#endif
    return result;
}

/*
 * Remove all tagged files and directories.
 */
static int remove_tagged(void)
{
    int ans;
    BOOL will_clear = TRUE;
    char *cp;
    char *tmpbuf = NULL;
    char *testpath = NULL;
    struct stat dir_info;
    int count;
    HTList *tag;

    if (HTList_isEmpty(tagged))	/* should never happen */
	return 0;

    ans = HTConfirm(gettext("Remove all tagged files and directories?"));

    count = 0;
    tag = tagged;
    while (ans == YES && (cp = (char *) HTList_nextObject(tag)) != NULL) {
	if (is_url(cp) == FILE_URL_TYPE) {	/* unnecessary check */
	    testpath = HTfullURL_toFile(cp);
	    LYTrimPathSep(testpath);
	    will_clear = TRUE;

	    /*
	     * Check the current status of the path to be deleted.
	     */
	    if (!ok_stat(testpath, &dir_info)) {
		will_clear = FALSE;
		break;
	    } else {
		if (remove_file(testpath) <= 0) {
		    if (count == 0)
			count = -1;
		    will_clear = FALSE;
		    break;
		}
		++count;
		FREE(testpath);
	    }
	}
    }
    FREE(testpath);
    FREE(tmpbuf);
    if (will_clear)
	clear_tags();
    return count;
}

static char *parse_directory(char *path)
{
    char *result;

    if (path) {
	path = strip_trailing_slash(path);
	path = HTParse(".", path, PARSE_PATH + PARSE_PUNCTUATION);
	result = HTURLPath_toFile(path, TRUE, FALSE);
	FREE(path);
    } else {			/* Last resort, should never happen. */
	result = HTURLPath_toFile(".", TRUE, FALSE);
    }
    return result;
}

/*
 * Move all tagged files and directories to a new location.
 *
 * The 'testpath' parameter is the current location, used for resolving
 * relative target specifications.
 */
static int modify_tagged(char *testpath)
{
    char *cp;
    bstring *given_target = NULL;
    char *dst_path = NULL;
    char *src_path = NULL;
    char *old_path = NULL;
    struct stat src_info;
    struct stat dst_info;
    int count = 0;
    HTList *tag;

    CTRACE((tfp, "modify_tagged(%s)\n", testpath));

    if (HTList_isEmpty(tagged))	/* should never happen */
	return 0;

    _statusline(gettext("Enter new location for tagged items: "));

    BStrCopy0(given_target, "");
    (void) LYgetBString(&given_target, FALSE, 0, NORECALL);
    if (!isBEmpty(given_target)) {
	/*
	 * Replace ~/ references to the home directory.
	 */
	if (LYIsTilde(given_target->str[0]) && LYIsPathSep(given_target->str[1])) {
	    char *cp1 = NULL;

	    StrAllocCopy(cp1, Home_Dir());
	    StrAllocCat(cp1, (given_target->str + 1));
	    BStrCopy0(given_target, cp1);
	    FREE(cp1);
	}

	/*
	 * If path is relative, prefix it with current location.
	 */
	if (!LYIsPathSep(given_target->str[0])) {
	    dst_path = HTLocalName(testpath);
	    LYAddPathSep(&dst_path);
	    StrAllocCat(dst_path, given_target->str);
	} else {
	    dst_path = HTLocalName(given_target->str);
	}

	if (!ok_stat(dst_path, &dst_info)) {
	    FREE(dst_path);
	    BStrFree(given_target);
	    return 0;
	}

	/*
	 * Determine the ownership of the current location, using the directory
	 * containing the file or subdir from each of the tagged files.
	 */
	for (tag = tagged; (cp = (char *) HTList_nextObject(tag)) != NULL;) {
	    src_path = parse_directory(cp);

	    if (isEmpty(old_path) || strcmp(old_path, src_path)) {
		if (!ok_stat(src_path, &src_info)
		    || same_location(&src_info, &dst_info)
		    || !dir_has_same_owner(&dst_info, &src_info)) {
		    FREE(src_path);
		    BStrFree(given_target);
		    return 0;
		}
	    }
	    StrAllocCopy(old_path, src_path);
	    FREE(src_path);
	}

	/*
	 * Move all tagged items to the target location.
	 */
	for (tag = tagged; (cp = (char *) HTList_nextObject(tag)) != NULL;) {
	    src_path = HTfullURL_toFile(cp);

	    if (move_file(src_path, dst_path) < 0) {
		if (count == 0)
		    count = -1;
		break;
	    }
	    FREE(src_path);
	    ++count;
	}
	clear_tags();
	FREE(src_path);
	FREE(dst_path);
    }
    BStrFree(given_target);
    return count;
}

/*
 * Modify the name of the specified item.
 */
static int modify_name(char *testpath)
{
    const char *cp;
    bstring *tmpbuf = NULL;
    char *newpath = NULL;
    struct stat dir_info;
    int code = 0;

    /*
     * Determine the status of the selected item.
     */
    testpath = strip_trailing_slash(testpath);

    if (ok_stat(testpath, &dir_info)) {

	/*
	 * Change the name of the file or directory.
	 */
	if (S_ISDIR(dir_info.st_mode)) {
	    cp = gettext("Enter new name for directory: ");
	} else if (S_ISREG(dir_info.st_mode)) {
	    cp = gettext("Enter new name for file: ");
	} else {
	    return ok_file_or_dir(&dir_info);
	}

	BStrCopy0(tmpbuf, LYPathLeaf(testpath));
	if (get_filename(cp, tmpbuf)) {

	    /*
	     * Do not allow the user to also change the location at this time.
	     */
	    if (LYLastPathSep(tmpbuf->str) != 0) {
		HTAlert(gettext("Illegal character (path-separator) found! Request ignored."));
	    } else if (strlen(tmpbuf->str)) {
		if ((cp = LYLastPathSep(testpath)) != NULL) {
		    HTSprintf0(&newpath, "%.*s%s",
			       (int) (cp - testpath + 1),
			       testpath, tmpbuf->str);
		} else {
		    StrAllocCopy(newpath, tmpbuf->str);
		}

		/*
		 * Make sure the destination does not already exist.
		 */
		if (not_already_exists(newpath)) {
		    code = move_file(testpath, newpath);
		}
		FREE(newpath);
	    }
	}
    }
    BStrFree(tmpbuf);
    return code;
}

/*
 * Change the location of a file or directory.
 */
static int modify_location(char *testpath)
{
    const char *cp;
    char *sp;
    bstring *tmpbuf = NULL;
    char *newpath = NULL;
    char *savepath = NULL;
    struct stat old_info;
    struct stat dir_info;
    int code = 0;

    /*
     * Determine the status of the selected item.
     */
    testpath = strip_trailing_slash(testpath);
    if (!ok_stat(testpath, &dir_info)) {
	return 0;
    }

    /*
     * Change the location of the file or directory.
     */
    if (S_ISDIR(dir_info.st_mode)) {
	cp = gettext("Enter new location for directory: ");
    } else if (S_ISREG(dir_info.st_mode)) {
	cp = gettext("Enter new location for file: ");
    } else {
	return ok_file_or_dir(&dir_info);
    }

    BStrCopy0(tmpbuf, testpath);
    *LYPathLeaf(tmpbuf->str) = '\0';
    if (get_filename(cp, tmpbuf)) {
	if (strlen(tmpbuf->str)) {
	    StrAllocCopy(savepath, testpath);
	    StrAllocCopy(newpath, testpath);

	    /*
	     * Allow ~/ references to the home directory.
	     */
	    if (LYIsTilde(tmpbuf->str[0])
		&& (tmpbuf->str[1] == '\0' || LYIsPathSep(tmpbuf->str[1]))) {
		StrAllocCopy(newpath, Home_Dir());
		StrAllocCat(newpath, (tmpbuf->str + 1));
		BStrCopy0(tmpbuf, newpath);
	    }
	    if (LYisAbsPath(tmpbuf->str)) {
		StrAllocCopy(newpath, tmpbuf->str);
	    } else if ((sp = LYLastPathSep(newpath)) != NULL) {
		*++sp = '\0';
		StrAllocCat(newpath, tmpbuf->str);
	    } else {
		HTAlert(gettext("Unexpected failure - unable to find trailing path separator"));
		FREE(newpath);
		FREE(savepath);
		BStrFree(tmpbuf);
		return 0;
	    }

	    /*
	     * Make sure the source and target have the same owner (uid).
	     */
	    old_info = dir_info;
	    if (!ok_stat(newpath, &dir_info)) {
		code = 0;
	    } else if (same_location(&old_info, &dir_info)) {
		code = 0;
	    } else if (dir_has_same_owner(&dir_info, &old_info)) {
		code = move_file(savepath, newpath);
	    }
	    FREE(newpath);
	    FREE(savepath);
	}
    }
    BStrFree(tmpbuf);
    return code;
}

/*
 * Modify name or location of a file or directory on localhost.
 */
int local_modify(DocInfo *doc, char **newpath)
{
    int ans;
    char *cp;
    bstring *testpath = NULL;
    int count;
    int code = 0;

    if (!HTList_isEmpty(tagged)) {
	cp = HTpartURL_toFile(doc->address);

	count = modify_tagged(cp);
	FREE(cp);

	if (doc->link > (nlinks - count - 1))
	    doc->link = (nlinks - count - 1);
	doc->link = ((doc->link < 0)
		     ? 0
		     : doc->link);

	return count;
    } else if (doc->link < 0 || doc->link > nlinks) {
	/*
	 * Added protection.
	 */
	return 0;
    }

    /*
     * Do not allow simultaneous change of name and location as in Unix.  This
     * reduces functionality but reduces difficulty for the novice.
     */
#ifdef OK_PERMIT
    _statusline(gettext("Modify name, location, or permission (n, l, or p): "));
#else
    _statusline(gettext("Modify name or location (n or l): "));
#endif /* OK_PERMIT */
    ans = LYgetch_single();

    if (StrChr("NLP", ans) != NULL) {
	cp = HTfullURL_toFile(links[doc->link].lname);
	if (strlen(cp) >= DIRED_MAXBUF) {
	    FREE(cp);
	    return 0;
	}
	BStrCopy0(testpath, cp);
	FREE(cp);

	if (ans == 'N') {
	    code = modify_name(testpath->str);
	} else if (ans == 'L') {
	    if (modify_location(testpath->str)) {
		if (doc->link == (nlinks - 1))
		    --doc->link;
		code = 1;
	    }
#ifdef OK_PERMIT
	} else if (ans == 'P') {
	    code = permit_location(NULL, testpath->str, newpath);
#endif /* OK_PERMIT */
	} else {
	    /*
	     * Code for changing ownership needed here.
	     */
	    HTAlert(gettext("This feature not yet implemented!"));
	}
    }
    BStrFree(testpath);
    return code;
}

#define BadChars() ((!no_dotfiles && show_dotfiles) \
		    ? "~/" \
		    : ".~/")

/*
 * Create a new empty file in the current directory.
 */
static int create_file(char *current_location)
{
    int code = FALSE;
    bstring *tmpbuf = NULL;
    char *testpath = NULL;

    BStrCopy0(tmpbuf, "");
    if (get_filename(gettext("Enter name of file to create: "), tmpbuf)) {

	if (strstr(tmpbuf->str, "//") != NULL) {
	    HTAlert(gettext("Illegal redirection \"//\" found! Request ignored."));
	} else if (strlen(tmpbuf->str) &&
		   StrChr(BadChars(), tmpbuf->str[0]) == NULL) {
	    StrAllocCopy(testpath, current_location);
	    LYAddPathSep(&testpath);

	    /*
	     * Append the target filename to the current location.
	     */
	    StrAllocCat(testpath, tmpbuf->str);

	    /*
	     * Make sure the target does not already exist
	     */
	    if (not_already_exists(testpath)) {
		code = touch_file(testpath);
	    }
	    FREE(testpath);
	}
    }
    BStrFree(tmpbuf);
    return code;
}

/*
 * Create a new directory in the current directory.
 */
static int create_directory(char *current_location)
{
    int code = FALSE;
    bstring *tmpbuf = NULL;
    char *testpath = NULL;

    BStrCopy0(tmpbuf, "");
    if (get_filename(gettext("Enter name for new directory: "), tmpbuf)) {

	if (strstr(tmpbuf->str, "//") != NULL) {
	    HTAlert(gettext("Illegal redirection \"//\" found! Request ignored."));
	} else if (strlen(tmpbuf->str) &&
		   StrChr(BadChars(), tmpbuf->str[0]) == NULL) {
	    StrAllocCopy(testpath, current_location);
	    LYAddPathSep(&testpath);

	    StrAllocCat(testpath, tmpbuf->str);

	    /*
	     * Make sure the target does not already exist.
	     */
	    if (not_already_exists(testpath)) {
		code = make_directory(testpath);
	    }
	    FREE(testpath);
	}
    }
    BStrFree(tmpbuf);
    return code;
}

/*
 * Create a file or a directory at the current location.
 */
int local_create(DocInfo *doc)
{
    int ans;
    char *cp;
    char testpath[DIRED_MAXBUF];

    cp = HTfullURL_toFile(doc->address);
    if (strlen(cp) >= DIRED_MAXBUF) {
	FREE(cp);
	return 0;
    }
    strcpy(testpath, cp);
    FREE(cp);

    _statusline(gettext("Create file or directory (f or d): "));
    ans = LYgetch_single();

    if (ans == 'F') {
	return (create_file(testpath));
    } else if (ans == 'D') {
	return (create_directory(testpath));
    } else {
	return 0;
    }
}

/*
 * Remove a single file or directory.
 */
static int remove_single(char *testpath)
{
    int code = 0;
    char *cp;
    char *tmpbuf = 0;
    struct stat dir_info;
    BOOL is_directory = FALSE;

    if (!ok_lstat(testpath, &dir_info)) {
	return 0;
    }

    /*
     * Locate the filename portion of the path.
     */
    if ((cp = LYLastPathSep(testpath)) != NULL) {
	++cp;
    } else {
	cp = testpath;
    }
    if (S_ISDIR(dir_info.st_mode)) {
	/*
	 * This strlen stuff will probably screw up intl translations.  Course,
	 * it's probably broken for screen sizes other 80, too -jes
	 */
	if (strlen(cp) < 37) {
	    HTSprintf0(&tmpbuf,
		       gettext("Remove directory '%s'?"), cp);
	} else {
	    HTSprintf0(&tmpbuf,
		       gettext("Remove directory?"));
	}
	is_directory = TRUE;
    } else if (S_ISREG(dir_info.st_mode)) {
	if (strlen(cp) < 60) {
	    HTSprintf0(&tmpbuf, gettext("Remove file '%s'?"), cp);
	} else {
	    HTSprintf0(&tmpbuf, gettext("Remove file?"));
	}
#ifdef S_IFLNK
    } else if (S_ISLNK(dir_info.st_mode)) {
	if (strlen(cp) < 50) {
	    HTSprintf0(&tmpbuf, gettext("Remove symbolic link '%s'?"), cp);
	} else {
	    HTSprintf0(&tmpbuf, gettext("Remove symbolic link?"));
	}
#endif
    } else {
	cannot_stat(testpath);
	FREE(tmpbuf);
	return 0;
    }

    if (HTConfirm(tmpbuf) == YES) {
	code = is_directory
	    ? remove_directory(testpath)
	    : remove_file(testpath);
    }
    FREE(tmpbuf);
    return code;
}

/*
 * Remove a file or a directory.
 */
int local_remove(DocInfo *doc)
{
    char *cp, *tp;
    char testpath[DIRED_MAXBUF];
    int count, i;

    if (!HTList_isEmpty(tagged)) {
	count = remove_tagged();
	if (doc->link > (nlinks - count - 1))
	    doc->link = (nlinks - count - 1);
	doc->link = ((doc->link < 0)
		     ? 0
		     : doc->link);
	return count;
    } else if (doc->link < 0 || doc->link > nlinks) {
	return 0;
    }
    cp = links[doc->link].lname;
    if (is_url(cp) == FILE_URL_TYPE) {
	tp = HTfullURL_toFile(cp);
	if (strlen(tp) >= DIRED_MAXBUF) {
	    FREE(tp);
	    return 0;
	}
	strcpy(testpath, tp);
	FREE(tp);

	if ((i = (int) strlen(testpath)) && testpath[i - 1] == '/')
	    testpath[(i - 1)] = '\0';

	if (remove_single(testpath)) {
	    if (doc->link == (nlinks - 1))
		--doc->link;
	    return 1;
	}
    }
    return 0;
}

#ifdef OK_PERMIT

static bstring *LYValidPermitFile = NULL;

static long permit_bits(char *string_mode)
{
    if (!strcmp(string_mode, "IRUSR"))
	return S_IRUSR;
    if (!strcmp(string_mode, "IWUSR"))
	return S_IWUSR;
    if (!strcmp(string_mode, "IXUSR"))
	return S_IXUSR;
    if (!strcmp(string_mode, "IRGRP"))
	return S_IRGRP;
    if (!strcmp(string_mode, "IWGRP"))
	return S_IWGRP;
    if (!strcmp(string_mode, "IXGRP"))
	return S_IXGRP;
    if (!strcmp(string_mode, "IROTH"))
	return S_IROTH;
    if (!strcmp(string_mode, "IWOTH"))
	return S_IWOTH;
    if (!strcmp(string_mode, "IXOTH"))
	return S_IXOTH;
    /* Don't include setuid and friends; use shell access for that. */
    return 0;
}

/*
 * Handle DIRED permissions.
 */
static int permit_location(char *destpath,
			   char *srcpath,
			   char **newpath)
{
    int code = 0;

#ifndef UNIX
    HTAlert(gettext("Sorry, don't know how to permit non-UNIX files yet."));
#else
    static char tempfile[LY_MAXPATH] = "\0";
    char *cp;
    char tmpdst[LY_MAXPATH];
    struct stat dir_info;
    const char *program;

    if (srcpath) {
	/*
	 * Create form.
	 */
	FILE *fp0;
	char *user_filename;
	const char *group_name;

	srcpath = strip_trailing_slash(srcpath);

	/*
	 * A couple of sanity tests.
	 */
	if (!ok_lstat(srcpath, &dir_info)
	    || !ok_file_or_dir(&dir_info))
	    return code;

	user_filename = LYPathLeaf(srcpath);

	(void) LYRemoveTemp(tempfile);
	if ((fp0 = LYOpenTemp(tempfile, HTML_SUFFIX, "w")) == NULL) {
	    HTAlert(gettext("Unable to open permit options file"));
	    return (code);
	}

	/*
	 * Make the tempfile a URL.
	 */
	LYLocalFileToURL(newpath, tempfile);
	LYRegisterUIPage(*newpath, UIP_PERMIT_OPTIONS);

	group_name = HTAA_GidToName((int) dir_info.st_gid);
	BStrCopy0(LYValidPermitFile, srcpath);

	fprintf(fp0, "<Html><Head>\n<Title>%s</Title>\n</Head>\n<Body>\n",
		PERMIT_OPTIONS_TITLE);
	fprintf(fp0, "<H1>%s%s</H1>\n", PERMISSIONS_SEGMENT, user_filename);
	{
	    /*
	     * Prevent filenames which include '#' or '?' from messing it up.
	     */
	    char *srcpath_url = HTEscape(srcpath, URL_PATH);

	    fprintf(fp0, "<Form Action=\"%s//PERMIT_LOCATION%s\">\n",
		    STR_LYNXDIRED, srcpath_url);
	    FREE(srcpath_url);
	}

	fprintf(fp0, "<Ol><Li>%s<Br><Br>\n",
		gettext("Specify permissions below:"));
	fprintf(fp0, "%s:<Br>\n", gettext("Owner:"));
	fprintf(fp0,
		"<Input Type=\"checkbox\" Name=\"mode\" Value=\"IRUSR\" %s> Read<Br>\n",
		(dir_info.st_mode & S_IRUSR) ? "checked" : "");
	fprintf(fp0,
		"<Input Type=\"checkbox\" Name=\"mode\" Value=\"IWUSR\" %s> Write<Br>\n",
		(dir_info.st_mode & S_IWUSR) ? "checked" : "");
	/*
	 * If restricted, only change eXecute permissions on directories.
	 */
	if (!no_change_exec_perms || S_ISDIR(dir_info.st_mode))
	    fprintf(fp0,
		    "<Input Type=\"checkbox\" Name=\"mode\" Value=\"IXUSR\" %s> %s<Br>\n",
		    (dir_info.st_mode & S_IXUSR) ? "checked" : "",
		    S_ISDIR(dir_info.st_mode) ? "Search" : "Execute");

	fprintf(fp0, "%s %s:<Br>\n", gettext("Group"), group_name);
	fprintf(fp0,
		"<Input Type=\"checkbox\" Name=\"mode\" Value=\"IRGRP\" %s> Read<Br>\n",
		(dir_info.st_mode & S_IRGRP) ? "checked" : "");
	fprintf(fp0,
		"<Input Type=\"checkbox\" Name=\"mode\" Value=\"IWGRP\" %s> Write<Br>\n",
		(dir_info.st_mode & S_IWGRP) ? "checked" : "");
	/*
	 * If restricted, only change eXecute permissions on directories.
	 */
	if (!no_change_exec_perms || S_ISDIR(dir_info.st_mode))
	    fprintf(fp0,
		    "<Input Type=\"checkbox\" Name=\"mode\" Value=\"IXGRP\" %s> %s<Br>\n",
		    (dir_info.st_mode & S_IXGRP) ? "checked" : "",
		    S_ISDIR(dir_info.st_mode) ? "Search" : "Execute");

	fprintf(fp0, "%s<Br>\n", gettext("Others:"));
	fprintf(fp0,
		"<Input Type=\"checkbox\" Name=\"mode\" Value=\"IROTH\" %s> Read<Br>\n",
		(dir_info.st_mode & S_IROTH) ? "checked" : "");
	fprintf(fp0,
		"<Input Type=\"checkbox\" Name=\"mode\" Value=\"IWOTH\" %s> Write<Br>\n",
		(dir_info.st_mode & S_IWOTH) ? "checked" : "");
	/*
	 * If restricted, only change eXecute permissions on directories.
	 */
	if (!no_change_exec_perms || S_ISDIR(dir_info.st_mode))
	    fprintf(fp0,
		    "<Input Type=\"checkbox\" Name=\"mode\" Value=\"IXOTH\" %s> %s<Br>\n",
		    (dir_info.st_mode & S_IXOTH) ? "checked" : "",
		    S_ISDIR(dir_info.st_mode) ? "Search" : "Execute");

	fprintf(fp0,
		"<Br>\n<Li><Input Type=\"submit\" Value=\"Submit\">  %s %s %s.\n</Ol>\n</Form>\n",
		gettext("form to permit"),
		S_ISDIR(dir_info.st_mode) ? "directory" : "file",
		user_filename);
	fprintf(fp0, "</Body></Html>");
	LYCloseTempFP(fp0);

	LYforce_no_cache = TRUE;
	code = PERMIT_FORM_RESULT;	/* Special flag for LYMainLoop */

    } else {			/* The form being activated. */
	mode_t new_mode = 0;

	/*
	 * Make sure we have a valid set-permission file comparison string
	 * loaded via a previous call with srcpath != NULL.  - KW
	 */
	if (isBEmpty(LYValidPermitFile)) {
	    if (LYCursesON)
		HTAlert(INVALID_PERMIT_URL);
	    else
		fprintf(stderr, "%s\n", INVALID_PERMIT_URL);
	    CTRACE((tfp, "permit_location: called for <%s>.\n",
		    (destpath ?
		     destpath : "NULL URL pointer")));
	    return code;
	}
	cp = destpath;
	while (*cp != '\0' && *cp != '?') {	/* Find filename */
	    cp++;
	}
	if (*cp == '\0') {
	    return (code);	/* Nothing to permit. */
	}
	*cp++ = '\0';		/* Null terminate file name and
				   start working on the masks. */

	/* Will now operate only on filename part. */
	if ((destpath = HTURLPath_toFile(destpath, TRUE, FALSE)) == 0)
	    return (code);
	if (strlen(destpath) >= LY_MAXPATH) {
	    FREE(destpath);
	    return (code);
	}
	strcpy(tmpdst, destpath);
	FREE(destpath);
	destpath = tmpdst;

	/*
	 * Make sure that the file string is the one from the last displayed
	 * File Permissions menu.  - KW
	 */
	if (strcmp(destpath, LYValidPermitFile->str)) {
	    if (LYCursesON)
		HTAlert(INVALID_PERMIT_URL);
	    else
		fprintf(stderr, "%s\n", INVALID_PERMIT_URL);
	    CTRACE((tfp, "permit_location: called for file '%s'.\n",
		    destpath));
	    return code;
	}

	/*
	 * A couple of sanity tests.
	 */
	destpath = strip_trailing_slash(destpath);
	if (!ok_stat(destpath, &dir_info)
	    || !ok_file_or_dir(&dir_info)) {
	    return code;
	}

	/*
	 * Cycle over permission strings.
	 */
	while (*cp != '\0') {
	    char *cr = cp;

	    while (*cr != '\0' && *cr != '&') {		/* GET data split by '&'. */
		cr++;
	    }
	    if (*cr != '\0') {
		*cr++ = '\0';
	    }
	    if (StrNCmp(cp, "mode=", 5) == 0) {		/* Magic string. */
		long mask = permit_bits(cp + 5);

		if (mask != 0) {
		    /*
		     * If restricted, only change eXecute permissions on
		     * directories.
		     */
		    if (!no_change_exec_perms
			|| StrChr(cp + 5, 'X') == NULL
			|| S_ISDIR(dir_info.st_mode)) {
			new_mode |= (mode_t) mask;
		    }
		} else {
		    HTAlert(gettext("Invalid mode format."));
		    return code;
		}
	    } else {
		HTAlert(gettext("Invalid syntax format."));
		return code;
	    }

	    cp = cr;
	}

	/*
	 * Call chmod().
	 */
	code = 1;
	if ((program = HTGetProgramPath(ppCHMOD)) != NULL) {
	    char **args;
	    char amode[10];
	    char *tmpbuf = NULL;

	    HTSprintf0(&tmpbuf, "chmod %.4o %s", (unsigned) new_mode, destpath);
	    sprintf(amode, "%.4o", (unsigned) new_mode);
	    args = make_argv("chmod",
			     amode,
			     destpath,
			     NULL);
	    if (LYExecv(program, args, tmpbuf) <= 0) {
		code = -1;
	    }
	    FREE(tmpbuf);
	    free_argv(args);
	} else {
	    if (chmod(destpath, new_mode) < 0) {
		code = -1;
	    }
	    CTRACE((tfp, "builtin chmod %.4o ->%d\n\t%s\n",
		    (unsigned) new_mode, code, destpath));
	}
	if (code == 1)
	    LYforce_no_cache = TRUE;	/* Force update of dired listing. */
    }
#endif /* !UNIX */
    return code;
}
#endif /* OK_PERMIT */

/*
 * Display or remove a tag from a given link.
 */
void tagflag(int flag,
	     int cur)
{
    if (nlinks > 0) {
	LYmove(links[cur].ly, 2);
	lynx_stop_reverse();
	if (flag == TRUE) {
	    LYaddch('+');
	} else {
	    LYaddch(' ');
	}

#if defined(FANCY_CURSES) || defined(USE_SLANG)
	if (!LYShowCursor)
	    LYHideCursor();	/* get cursor out of the way */
	else
#endif /* FANCY CURSES || USE_SLANG */
	    /*
	     * Never hide the cursor if there's no FANCY CURSES.
	     */
	    LYmove(links[cur].ly, links[cur].lx);

	LYrefresh();
    }
}

/*
 * Handle DIRED tags.
 */
void showtags(HTList *t)
{
    int i;
    HTList *s;
    char *name;

    for (i = 0; i < nlinks; i++) {
	s = t;
	while ((name = (char *) HTList_nextObject(s)) != NULL) {
	    if (!strcmp(links[i].lname, name)) {
		tagflag(TRUE, i);
		break;
	    }
	}
    }
}

static char *DirectoryOf(char *pathname)
{
    char *result = 0;
    char *leaf;

    StrAllocCopy(result, pathname);
    leaf = LYPathLeaf(result);

    if (leaf != result) {
	const char *result1 = 0;

	*leaf = '\0';
	if (!LYisRootPath(result))
	    LYTrimPathSep(result);
	result1 = wwwName(result);
	StrAllocCopy(result, result1);
    }
    return result;
}

#ifdef __DJGPP__
/*
 * Convert filenames to acceptable 8+3 names when necessary.  Make a copy of
 * the parameter if we must modify it.
 */
static char *LYonedot(char *line)
{
    char *dot;
    static char line1[LY_MAXPATH];

    if (pathconf(line, _PC_NAME_MAX) <= 12) {
	LYStrNCpy(line1, line, sizeof(line1) - 1);
	for (;;) {
	    if ((dot = strrchr(line1, '.')) == 0
		|| LYLastPathSep(dot) != 0) {
		break;
	    } else if (strlen(dot) == 1) {
		*dot = 0;
	    } else {
		*dot = '_';
	    }
	}
	return (line1);
    }
    return (line);
}
#else
#define LYonedot(path) path
#endif /*  __DJGPP__ */

static char *match_op(const char *prefix,
		      char *data)
{
    size_t len = strlen(prefix);

    if (!StrNCmp("LYNXDIRED://", data, 12)
	&& !StrNCmp(prefix, data + 12, len)) {
	len += 12;
#if defined(USE_DOS_DRIVES)
	if (data[len] == '/') {	/* this is normal */
	    len++;
	}
#endif
	return data + len;
    }
    return 0;
}

/*
 * Construct the appropriate system command taking care to escape all path
 * references to avoid spoofing the shell.
 */
static char *build_command(char *line,
			   char *dirName,
			   char *arg)
{
    char *buffer = NULL;
    const char *program;
    const char *tar_path = HTGetProgramPath(ppTAR);

    if ((arg = match_op("DECOMPRESS", line)) != 0) {
#define FMT_UNCOMPRESS "%s %s"
	if ((program = HTGetProgramPath(ppUNCOMPRESS)) != NULL) {
	    HTAddParam(&buffer, FMT_UNCOMPRESS, 1, program);
	    HTAddParam(&buffer, FMT_UNCOMPRESS, 2, arg);
	    HTEndParam(&buffer, FMT_UNCOMPRESS, 2);
	}
	return buffer;
    }
#if defined(OK_UUDECODE) && !defined(ARCHIVE_ONLY)
    if ((arg = match_op("UUDECODE", line)) != 0) {
#define FMT_UUDECODE "%s %s"
	if ((program = HTGetProgramPath(ppUUDECODE)) != NULL) {
	    HTAddParam(&buffer, FMT_UUDECODE, 1, program);
	    HTAddParam(&buffer, FMT_UUDECODE, 2, arg);
	    HTEndParam(&buffer, FMT_UUDECODE, 2);
	    HTAlert(gettext("Warning!  UUDecoded file will exist in the directory you started Lynx."));
	}
	return buffer;
    }
#endif /* OK_UUDECODE && !ARCHIVE_ONLY */

#ifdef OK_TAR
    if (tar_path != NULL) {
# ifndef ARCHIVE_ONLY
#  ifdef OK_GZIP
	if ((arg = match_op("UNTAR_GZ", line)) != 0) {
#define FMT_UNTAR_GZ "cd %s; %s -qdc %s |  %s %s %s"
	    if ((program = HTGetProgramPath(ppGZIP)) != NULL) {
		dirName = DirectoryOf(arg);
		HTAddParam(&buffer, FMT_UNTAR_GZ, 1, dirName);
		HTAddParam(&buffer, FMT_UNTAR_GZ, 2, program);
		HTAddParam(&buffer, FMT_UNTAR_GZ, 3, arg);
		HTAddParam(&buffer, FMT_UNTAR_GZ, 4, tar_path);
		HTAddToCmd(&buffer, FMT_UNTAR_GZ, 5, TAR_DOWN_OPTIONS);
		HTAddToCmd(&buffer, FMT_UNTAR_GZ, 6, TAR_PIPE_OPTIONS);
		HTEndParam(&buffer, FMT_UNTAR_GZ, 6);
	    }
	    return buffer;
	}
#  endif /* OK_GZIP */
	if ((arg = match_op("UNTAR_Z", line)) != 0) {
#define FMT_UNTAR_Z "cd %s; %s %s |  %s %s %s"
	    if ((program = HTGetProgramPath(ppZCAT)) != NULL) {
		dirName = DirectoryOf(arg);
		HTAddParam(&buffer, FMT_UNTAR_Z, 1, dirName);
		HTAddParam(&buffer, FMT_UNTAR_Z, 2, program);
		HTAddParam(&buffer, FMT_UNTAR_Z, 3, arg);
		HTAddParam(&buffer, FMT_UNTAR_Z, 4, tar_path);
		HTAddToCmd(&buffer, FMT_UNTAR_Z, 5, TAR_DOWN_OPTIONS);
		HTAddToCmd(&buffer, FMT_UNTAR_Z, 6, TAR_PIPE_OPTIONS);
		HTEndParam(&buffer, FMT_UNTAR_Z, 6);
	    }
	    return buffer;
	}
	if ((arg = match_op("UNTAR", line)) != 0) {
#define FMT_UNTAR "cd %s; %s %s %s"
	    dirName = DirectoryOf(arg);
	    HTAddParam(&buffer, FMT_UNTAR, 1, dirName);
	    HTAddParam(&buffer, FMT_UNTAR, 2, tar_path);
	    HTAddToCmd(&buffer, FMT_UNTAR, 3, TAR_DOWN_OPTIONS);
	    HTAddParam(&buffer, FMT_UNTAR, 4, arg);
	    HTEndParam(&buffer, FMT_UNTAR, 4);
	    return buffer;
	}
# endif	/* !ARCHIVE_ONLY */

# ifdef OK_GZIP
	if ((arg = match_op("TAR_GZ", line)) != 0) {
#define FMT_TAR_GZ "cd %s; %s %s %s %s | %s -qc >%s%s"
	    if ((program = HTGetProgramPath(ppGZIP)) != NULL) {
		dirName = DirectoryOf(arg);
		HTAddParam(&buffer, FMT_TAR_GZ, 1, dirName);
		HTAddParam(&buffer, FMT_TAR_GZ, 2, tar_path);
		HTAddToCmd(&buffer, FMT_TAR_GZ, 3, TAR_UP_OPTIONS);
		HTAddToCmd(&buffer, FMT_TAR_GZ, 4, TAR_PIPE_OPTIONS);
		HTAddParam(&buffer, FMT_TAR_GZ, 5, LYPathLeaf(arg));
		HTAddParam(&buffer, FMT_TAR_GZ, 6, program);
		HTAddParam(&buffer, FMT_TAR_GZ, 7, LYonedot(LYPathLeaf(arg)));
		HTAddParam(&buffer, FMT_TAR_GZ, 8, EXT_TAR_GZ);
		HTEndParam(&buffer, FMT_TAR_GZ, 8);
	    }
	    return buffer;
	}
# endif	/* OK_GZIP */

	if ((arg = match_op("TAR_Z", line)) != 0) {
#define FMT_TAR_Z "cd %s; %s %s %s %s | %s >%s%s"
	    if ((program = HTGetProgramPath(ppCOMPRESS)) != NULL) {
		dirName = DirectoryOf(arg);
		HTAddParam(&buffer, FMT_TAR_Z, 1, dirName);
		HTAddParam(&buffer, FMT_TAR_Z, 2, tar_path);
		HTAddToCmd(&buffer, FMT_TAR_Z, 3, TAR_UP_OPTIONS);
		HTAddToCmd(&buffer, FMT_TAR_Z, 4, TAR_PIPE_OPTIONS);
		HTAddParam(&buffer, FMT_TAR_Z, 5, LYPathLeaf(arg));
		HTAddParam(&buffer, FMT_TAR_Z, 6, program);
		HTAddParam(&buffer, FMT_TAR_Z, 7, LYonedot(LYPathLeaf(arg)));
		HTAddParam(&buffer, FMT_TAR_Z, 8, EXT_TAR_Z);
		HTEndParam(&buffer, FMT_TAR_Z, 8);
	    }
	    return buffer;
	}

	if ((arg = match_op("TAR", line)) != 0) {
#define FMT_TAR "cd %s; %s %s %s %s.tar %s"
	    dirName = DirectoryOf(arg);
	    HTAddParam(&buffer, FMT_TAR, 1, dirName);
	    HTAddParam(&buffer, FMT_TAR, 2, tar_path);
	    HTAddToCmd(&buffer, FMT_TAR, 3, TAR_UP_OPTIONS);
	    HTAddToCmd(&buffer, FMT_TAR, 4, TAR_FILE_OPTIONS);
	    HTAddParam(&buffer, FMT_TAR, 5, LYonedot(LYPathLeaf(arg)));
	    HTAddParam(&buffer, FMT_TAR, 6, LYPathLeaf(arg));
	    HTEndParam(&buffer, FMT_TAR, 6);
	    return buffer;
	}
    }
#endif /* OK_TAR */

#ifdef OK_GZIP
    if ((arg = match_op("GZIP", line)) != 0) {
#define FMT_GZIP "%s -q %s"
	if ((program = HTGetProgramPath(ppGZIP)) != NULL) {
	    HTAddParam(&buffer, FMT_GZIP, 1, program);
	    HTAddParam(&buffer, FMT_GZIP, 2, arg);
	    HTEndParam(&buffer, FMT_GZIP, 2);
	}
	return buffer;
    }
#ifndef ARCHIVE_ONLY
    if ((arg = match_op("UNGZIP", line)) != 0) {
#define FMT_UNGZIP "%s -d %s"
	if ((program = HTGetProgramPath(ppGZIP)) != NULL) {
	    HTAddParam(&buffer, FMT_UNGZIP, 1, program);
	    HTAddParam(&buffer, FMT_UNGZIP, 2, arg);
	    HTEndParam(&buffer, FMT_UNGZIP, 2);
	}
	return buffer;
    }
#endif /* !ARCHIVE_ONLY */
#endif /* OK_GZIP */

#ifdef OK_ZIP
    if ((arg = match_op("ZIP", line)) != 0) {
#define FMT_ZIP "cd %s; %s -rq %s.zip %s"
	if ((program = HTGetProgramPath(ppZIP)) != NULL) {
	    dirName = DirectoryOf(arg);
	    HTAddParam(&buffer, FMT_ZIP, 1, dirName);
	    HTAddParam(&buffer, FMT_ZIP, 2, program);
	    HTAddParam(&buffer, FMT_ZIP, 3, LYonedot(LYPathLeaf(arg)));
	    HTAddParam(&buffer, FMT_ZIP, 4, LYPathLeaf(arg));
	    HTEndParam(&buffer, FMT_ZIP, 4);
	}
	return buffer;
    }
#if !defined(ARCHIVE_ONLY)
    if ((arg = match_op("UNZIP", line)) != 0) {
#define FMT_UNZIP "cd %s; %s -q %s"
	if ((program = HTGetProgramPath(ppUNZIP)) != NULL) {
	    dirName = DirectoryOf(arg);
	    HTAddParam(&buffer, FMT_UNZIP, 1, dirName);
	    HTAddParam(&buffer, FMT_UNZIP, 2, program);
	    HTAddParam(&buffer, FMT_UNZIP, 3, arg);
	    HTEndParam(&buffer, FMT_UNZIP, 3);
	}
	return buffer;
    }
# endif	/* !ARCHIVE_ONLY */
#endif /* OK_ZIP */

    if ((arg = match_op("COMPRESS", line)) != 0) {
#define FMT_COMPRESS "%s %s"
	if ((program = HTGetProgramPath(ppCOMPRESS)) != NULL) {
	    HTAddParam(&buffer, FMT_COMPRESS, 1, program);
	    HTAddParam(&buffer, FMT_COMPRESS, 2, arg);
	    HTEndParam(&buffer, FMT_COMPRESS, 2);
	}
	return buffer;
    }

    return NULL;
}

/*
 * Perform file management operations for LYNXDIRED URL's.  Attempt to be
 * consistent.  These are (pseudo) URLs - i.e., they should be in URL syntax: 
 * some bytes will be URL-escaped with '%'.  This is necessary because these
 * (pseudo) URLs will go through some of the same kinds of interpretations and
 * mutilations as real ones:  HTParse, stripping off #fragments etc.  (Some
 * access schemes currently have special rules about not escaping parsing '#'
 * "the URL way" built into HTParse, but that doesn't look like a clean way.)
 */
int local_dired(DocInfo *doc)
{
    char *line_url;		/* will point to doc's address, which is a URL */
    char *line = NULL;		/* same as line_url, but HTUnEscaped, will be alloced */
    char *arg = NULL;		/* ...will point into line[] */
    char *tp = NULL;
    char *tmpbuf = NULL;
    char *buffer = NULL;
    char *dirName = NULL;
    BOOL do_pop_doc = TRUE;

    line_url = doc->address;
    CTRACE((tfp, "local_dired: called for <%s>.\n",
	    (line_url
	     ? line_url
	     : gettext("NULL URL pointer"))));
    HTUnEscapeSome(line_url, "/");	/* don't mess too much with *doc */

    StrAllocCopy(line, line_url);
    HTUnEscape(line);		/* _file_ (not URL) syntax, for those functions
				   that need it.  Don't forget to FREE it. */
    if (match_op("CHDIR", line) != 0) {
#ifdef SUPPORT_CHDIR
	handle_LYK_CHDIR();
	do_pop_doc = FALSE;
#endif
	arg = 0;		/* do something to avoid cc's complaints */
    } else if ((arg = match_op("NEW_FILE", line)) != 0) {
	if (create_file(arg) > 0)
	    LYforce_no_cache = TRUE;
    } else if ((arg = match_op("NEW_FOLDER", line)) != 0) {
	if (create_directory(arg) > 0)
	    LYforce_no_cache = TRUE;
#ifdef OK_INSTALL
    } else if ((arg = match_op("INSTALL_SRC", line)) != 0) {
	local_install(NULL, arg, &tp);
	if (tp) {
	    FREE(doc->address);
	    doc->address = tp;
	}
	FREE(line);
	return 0;
    } else if ((arg = match_op("INSTALL_DEST", line)) != 0) {
	local_install(arg, NULL, &tp);
	LYpop(doc);
#endif /* OK_INSTALL */
    } else if ((arg = match_op("MODIFY_NAME", line)) != 0) {
	if (modify_name(arg) > 0)
	    LYforce_no_cache = TRUE;
    } else if ((arg = match_op("MODIFY_LOCATION", line)) != 0) {
	if (modify_location(arg) > 0)
	    LYforce_no_cache = TRUE;
    } else if ((arg = match_op("MOVE_TAGGED", line_url)) != 0) {
	if (modify_tagged(arg) > 0)
	    LYforce_no_cache = TRUE;
#ifdef OK_PERMIT
    } else if ((arg = match_op("PERMIT_SRC", line)) != 0) {
	permit_location(NULL, arg, &tp);
	if (tp) {
	    /*
	     * One of the checks may have failed.
	     */
	    FREE(doc->address);
	    doc->address = tp;
	}
	FREE(line);
	return 0;
    } else if ((arg = match_op("PERMIT_LOCATION", line_url)) != 0) {
	permit_location(arg, NULL, &tp);
#endif /* OK_PERMIT */
    } else if ((arg = match_op("REMOVE_SINGLE", line)) != 0) {
	if (remove_single(arg) > 0)
	    LYforce_no_cache = TRUE;
    } else if (match_op("REMOVE_TAGGED", line) != 0) {
	if (remove_tagged())
	    LYforce_no_cache = TRUE;
    } else if (match_op("CLEAR_TAGGED", line) != 0) {
	clear_tags();
    } else if ((arg = match_op("UPLOAD", line)) != 0) {
	/*
	 * They're written by LYUpload_options() HTUnEscaped; don't want to
	 * change that for now...  so pass through without more unescaping. 
	 * Directory names containing '#' will probably fail.
	 */
	if (LYUpload(line_url))
	    LYforce_no_cache = TRUE;
    } else {
	LYTrimPathSep(line);
	if (LYLastPathSep(line) == NULL) {
	    FREE(line);
	    return 0;
	}

	buffer = build_command(line, dirName, arg);

	if (buffer != 0) {
	    if ((int) strlen(buffer) < LYcolLimit - 14) {
		HTSprintf0(&tmpbuf, gettext("Executing %s "), buffer);
	    } else {
		HTSprintf0(&tmpbuf,
			   gettext("Executing system command. This might take a while."));
	    }
	    _statusline(tmpbuf);
	    stop_curses();
	    printf("%s\r\n", tmpbuf);
	    LYSystem(buffer);
#ifdef VMS
	    HadVMSInterrupt = FALSE;
#endif /* VMS */
	    start_curses();
	    LYforce_no_cache = TRUE;
	}
    }

    FREE(dirName);
    FREE(tmpbuf);
    FREE(buffer);
    FREE(line);
    FREE(tp);
    if (do_pop_doc)
	LYpop(doc);
    return 0;
}

/*
 * Provide a menu of file management options.
 */
int dired_options(DocInfo *doc, char **newfile)
{
    static char tempfile[LY_MAXPATH];
    const char *my_suffix;
    char *path = NULL;
    char *dir;
    lynx_list_item_type *nxt;
    struct stat dir_info;
    FILE *fp0;
    char *dir_url;
    char *path_url;
    BOOLEAN nothing_tagged;
    int count;
    struct dired_menu *mp;
    char buf[2048];

    if ((fp0 = InternalPageFP(tempfile, FALSE)) == 0)
	return (0);

    /*
     * Make the tempfile a URL.
     */
    LYLocalFileToURL(newfile, tempfile);
    LYRegisterUIPage(*newfile, UIP_DIRED_MENU);

    if (doc->link > -1 && doc->link < (nlinks + 1)) {
	path = HTfullURL_toFile(links[doc->link].lname);
	LYTrimPathSep(path);

	if (!ok_lstat(path, &dir_info)) {
	    LYCloseTempFP(fp0);
	    FREE(path);
	    return 0;
	}

    } else {
	StrAllocCopy(path, "");
	memset(&dir_info, 0, sizeof(dir_info));
    }

    dir = HTfullURL_toFile(doc->address);
    LYTrimPathSep(dir);

    nothing_tagged = (BOOL) (HTList_isEmpty(tagged));

    BeginInternalPage(fp0, DIRED_MENU_TITLE, DIRED_MENU_HELP);

    fprintf(fp0, "<em>%s</em> %s<br>\n", gettext("Current directory:"), dir);

    if (nothing_tagged) {
	fprintf(fp0, "<em>%s</em> ", gettext("Current selection:"));
	if (strlen(path)) {
	    fprintf(fp0, "%s<p>\n", path);
	} else {
	    fprintf(fp0, "%s.<p>\n", gettext("Nothing currently selected."));
	}
    } else {
	/*
	 * Write out number of tagged items, and names of first few of them
	 * relative to current (in the DIRED sense) directory.
	 */
	int n = HTList_count(tagged);
	char *cp1 = NULL;
	char *cd = NULL;
	int i, m;

#define NUM_TAGS_TO_WRITE 10
	fprintf(fp0, "<em>%s</em> %d %s",
		gettext("Current selection:"),
		n, ((n == 1)
		    ? gettext("tagged item:")
		    : gettext("tagged items:")));
	StrAllocCopy(cd, doc->address);
	HTUnEscapeSome(cd, "/");
	LYAddHtmlSep(&cd);
	m = (n < NUM_TAGS_TO_WRITE) ? n : NUM_TAGS_TO_WRITE;
	for (i = 1; i <= m; i++) {
	    cp1 = HTRelative((char *) HTList_objectAt(tagged, i - 1),
			     (*cd ? cd : "file://localhost"));
	    HTUnEscape(cp1);
	    LYEntify(&cp1, TRUE);	/* _should_ do this everywhere... */
	    fprintf(fp0, "%s<br>\n&nbsp;&nbsp;&nbsp;%s",
		    (i == 1 ? "" : " ,"), cp1);
	    FREE(cp1);
	}
	if (n > m) {
	    fprintf(fp0, " , ...");
	}
	fprintf(fp0, "<p>\n");
	FREE(cd);
    }

    /*
     * If menu_head is NULL then use defaults and link them together now.
     */
    if (menu_head == NULL) {
	for (mp = defmenu; GetDiredHref(mp) != NULL; mp++)
	    mp->next = (mp + 1);
	(--mp)->next = NULL;
	menu_head = defmenu;
    }

    for (mp = menu_head; mp != NULL; mp = mp->next) {
	if (mp->cond != DE_TAG && !nothing_tagged)
	    continue;
	if (mp->cond == DE_TAG && nothing_tagged)
	    continue;
	if (mp->cond == DE_DIR &&
	    (!*path || !S_ISDIR(dir_info.st_mode)))
	    continue;
	if (mp->cond == DE_FILE &&
	    (!*path || !S_ISREG(dir_info.st_mode)))
	    continue;
#ifdef S_IFLNK
	if (mp->cond == DE_SYMLINK &&
	    (!*path || !S_ISLNK(dir_info.st_mode)))
	    continue;
#endif
	my_suffix = GetDiredSuffix(mp);
	if (non_empty(my_suffix) &&
	    (strlen(path) < strlen(my_suffix) ||
	     strcmp(my_suffix, &path[(strlen(path) - strlen(my_suffix))]) != 0))
	    continue;
	dir_url = HTEscape(dir, URL_PATH);
	path_url = HTEscape(path, URL_PATH);
	fprintf(fp0, "<a href=\"%s",
		render_item(GetDiredHref(mp),
			    path_url, dir_url, buf, sizeof(buf), YES));
	fprintf(fp0, "\">%s</a> ",
		render_item(GetDiredLink(mp),
			    path, dir, buf, sizeof(buf), NO));
	fprintf(fp0, "%s<br>\n",
		render_item(GetDiredRest(mp),
			    path, dir, buf, sizeof(buf), NO));
	FREE(dir_url);
	FREE(path_url);
    }
    FREE(path);

    if (uploaders != NULL) {
	fprintf(fp0, "<p>Upload to current directory:<p>\n");
	for (count = 0, nxt = uploaders;
	     nxt != NULL;
	     nxt = nxt->next, count++) {
	    fprintf(fp0,
		    "<a href=\"LYNXDIRED://UPLOAD=%d/TO=%s\"> %s </a><br>\n",
		    count, dir, nxt->name);
	}
    }
    FREE(dir);

    EndInternalPage(fp0);
    LYCloseTempFP(fp0);

    LYforce_no_cache = TRUE;

    return (0);
}

/*
 * Check DIRED filename, return true on success
 */
static int get_filename(const char *prompt,
			bstring *buf)
{
    char *cp;

    _statusline(prompt);

    (void) LYgetBString(&buf, FALSE, 0, NORECALL);
    if (strstr(buf->str, "../") != NULL) {
	HTAlert(gettext("Illegal filename; request ignored."));
	return FALSE;
    } else if (no_dotfiles || !show_dotfiles) {
	cp = LYLastPathSep(buf->str);	/* find last slash */
	if (cp)
	    cp += 1;
	else
	    cp = buf->str;
	if (*cp == '.') {
	    HTAlert(gettext("Illegal filename; request ignored."));
	    return FALSE;
	}
    }
    return !isBEmpty(buf);
}

#ifdef OK_INSTALL

#define LYEXECV_MAX_ARGC 15
/* these are quasi-constant once they have been allocated: */
static char **install_argp = NULL;	/* args for execv install */
static char *install_path = NULL;	/* auxiliary */

#ifdef LY_FIND_LEAKS
static void clear_install_path(void)
{
    FREE(install_argp);
    FREE(install_path);
}
#endif /* LY_FIND_LEAKS */

/*
 * Fill in args array for execv (or execvp etc.) call, after first allocating
 * it if necessary.  No fancy parsing, cmd_args is just split at spaces.  Leave
 * room for reserve additional args to be added by caller.
 *
 * On success *argvp points to new args vector, *pathp is auxiliary.  On
 * success returns index of next argument, else -1.  This is generic enough
 * that it could be used for other calls than install, except the atexit call. 
 * Go through this trouble for install because INSTALL_ARGS may be significant,
 * and someone may configure it with more than one significant flags.  - kw
 */
static int fill_argv_for_execv(char ***argvp,
			       char **pathp,
			       char *cmd_path,
			       const char *cmd_args,
			       int reserve)
{
    int n = 0;

    char **args;
    char *cp;

    if (*argvp == NULL) {
	*argvp = typecallocn(char *, LYEXECV_MAX_ARGC + 1);

	if (!*argvp)
	    return (-1);
#ifdef LY_FIND_LEAKS
	atexit(clear_install_path);
#endif
    }
    args = *argvp;
    args[n++] = cmd_path;
    if (cmd_args) {
	StrAllocCopy(*pathp, cmd_args);
	cp = strtok(*pathp, " ");
	if (cp) {
	    while (cp && (n < LYEXECV_MAX_ARGC - reserve)) {
		args[n++] = cp;
		cp = strtok(NULL, " ");
	    }
	    if (cp && (n >= LYEXECV_MAX_ARGC - reserve)) {
		CTRACE((tfp, "Too many args for '%s' in '%s'!\n",
			NONNULL(cmd_path), cmd_args));
		return (-1);
	    }
	} else {
	    args[n++] = *pathp;
	}
    }
    args[n] = (char *) 0;
    return (n);
}

/*
 * Install the specified file or directory.
 */
BOOLEAN local_install(char *destpath,
		      char *srcpath,
		      char **newpath)
{
    char *tmpbuf = NULL;
    static char savepath[DIRED_MAXBUF];		/* This will be the link that

						   is to be installed. */
    struct stat dir_info;
    char **args;
    HTList *tag;
    char *cp = NULL;
    char *tmpdest = NULL;
    int count = 0;
    int n = 0;			/* indices into 'args[]' */
    static int src = -1;
    const char *program;

    if ((program = HTGetProgramPath(ppINSTALL)) == NULL) {
	HTAlert(gettext("Install in the selected directory not permitted."));
	return 0;
    }

    /*
     * Determine the status of the selected item.
     */
    if (srcpath) {
	srcpath = strip_trailing_slash(srcpath);
	if (is_url(srcpath)) {
	    char *local_src = HTfullURL_toFile(srcpath);

	    if (!ok_localname(savepath, local_src)) {
		FREE(local_src);
		return 0;
	    }
	    FREE(local_src);
	} else if (!HTList_isEmpty(tagged) &&
		   srcpath[0] == '\0') {
	    savepath[0] = '\0';	/* will always use tagged list - kw */
	} else if (!ok_localname(savepath, srcpath)) {
	    return 0;
	}
	LYforce_no_cache = TRUE;
	LYLocalFileToURL(newpath, Home_Dir());
	LYAddHtmlSep(newpath);
	StrAllocCat(*newpath, INSTALLDIRS_FILE);
	LYRegisterUIPage(*newpath, UIP_INSTALL);
	return 0;
    }

    /* deal with ~/ or /~/ at the beginning - kw */
    if (LYIsTilde(destpath[0]) &&
	(LYIsPathSep(destpath[1]) || destpath[1] == '\0')) {
	cp = &destpath[1];
    } else if (LYIsPathSep(destpath[0]) && LYIsTilde(destpath[1]) &&
	       (LYIsPathSep(destpath[2]) || destpath[2] == '\0')) {
	cp = &destpath[2];
    }
    if (cp) {
	/* If found, allocate new string, make destpath point to it - kw */
	StrAllocCopy(tmpdest, Home_Dir());
	if (cp[0] && cp[1]) {
	    LYAddPathSep(&tmpdest);
	    StrAllocCat(tmpdest, cp + 1);
	}
	destpath = tmpdest;
    }

    destpath = strip_trailing_slash(destpath);

    if (!ok_stat(destpath, &dir_info)) {
	FREE(tmpdest);
	return 0;
    } else if (!S_ISDIR(dir_info.st_mode)) {
	HTAlert(gettext("The selected item is not a directory!  Request ignored."));
	FREE(tmpdest);
	return 0;
    } else if (0 /*directory not writable */ ) {
	HTAlert(gettext("Install in the selected directory not permitted."));
	FREE(tmpdest);
	return 0;
    }

    statusline(gettext("Just a moment, ..."));

    /* fill in the fixed args, if not already done - kw */
    if (src > 0 && install_argp) {
	n = src;
	n++;
    } else {
	n = fill_argv_for_execv(&install_argp, &install_path,
				"install",
#ifdef INSTALL_ARGS
				INSTALL_ARGS,
#else
				NULL,
#endif /* INSTALL_ARGS */
				2);
	if (n <= 0) {
	    src = 0;
	    HTAlert(gettext("Error building install args"));
	    FREE(tmpdest);
	    return 0;
	}
	src = n++;
    }
    args = install_argp;

    args[n++] = destpath;
    args[n] = (char *) 0;
    tag = tagged;

    if (HTList_isEmpty(tagged)) {
	/* simplistic detection of identical src and dest - kw */
	if (!strcmp(savepath, destpath)) {
	    HTUserMsg2(gettext("Source and target are the same: %s"),
		       savepath);
	    FREE(tmpdest);
	    return (-1);	/* don't do it */
	} else if (!StrNCmp(savepath, destpath, strlen(destpath)) &&
		   LYIsPathSep(savepath[strlen(destpath)]) &&
		   LYLastPathSep(savepath + strlen(destpath) + 1) == 0) {
	    HTUserMsg2(gettext("Already in target directory: %s"),
		       savepath);
	    FREE(tmpdest);
	    return 0;		/* don't do it */
	}
	args[src] = savepath;
	HTSprintf0(&tmpbuf, "install %s in %s", savepath, destpath);
	if (LYExecv(program, args, tmpbuf) <= 0) {
	    FREE(tmpbuf);
	    FREE(tmpdest);
	    return (-1);
	}
	count++;
    } else {
	char *name;

	HTSprintf0(&tmpbuf, "install in %s", destpath);
	while ((name = (char *) HTList_nextObject(tag))) {
	    int err;

	    args[src] = HTfullURL_toFile(name);

	    /* simplistic detection of identical src and dest - kw */
	    if (!strcmp(args[src], destpath)) {
		HTUserMsg2(gettext("Source and target are the same: %s"),
			   args[src]);
		FREE(args[src]);
		continue;	/* skip this source file */
	    } else if (!StrNCmp(args[src], destpath, strlen(destpath)) &&
		       LYIsPathSep(args[src][strlen(destpath)]) &&
		       LYLastPathSep(args[src] + strlen(destpath) + 1) == 0) {
		HTUserMsg2(gettext("Already in target directory: %s"),
			   args[src]);
		FREE(args[src]);
		continue;	/* skip this source file */
	    }
	    err = (LYExecv(program, args, tmpbuf) <= 0);
	    FREE(args[src]);
	    if (err) {
		FREE(tmpbuf);
		FREE(tmpdest);
		return ((count == 0) ? -1 : count);
	    }
	    count++;
	}
	clear_tags();
    }
    FREE(tmpbuf);
    FREE(tmpdest);
    HTInfoMsg(gettext("Installation complete"));
    return count;
}
#endif /* OK_INSTALL */

/*
 * Clear DIRED tags.
 */
void clear_tags(void)
{
    char *cp = NULL;

    while ((cp = (char *) HTList_removeLastObject(tagged)) != NULL) {
	FREE(cp);
    }
    if (HTList_isEmpty(tagged))
	FREE(tagged);
}

/*
 * Handle DIRED menu item.
 */
void add_menu_item(char *str)
{
    struct dired_menu *tmp, *mp;
    char *cp;
    BOOL used = FALSE;

    /*
     * First custom menu definition causes entire default menu to be discarded.
     */
    if (menu_head == defmenu)
	menu_head = NULL;

    tmp = typecalloc(struct dired_menu);

    if (tmp == NULL)
	outofmem(__FILE__, "add_menu_item");

    assert(tmp != NULL);

    /*
     * Conditional on tagged != NULL ?
     */
    if ((cp = StrChr(str, ':')) != 0) {
	*cp++ = '\0';
	if (strcasecomp(str, "tag") == 0) {
	    tmp->cond = DE_TAG;
	} else if (strcasecomp(str, "dir") == 0) {
	    tmp->cond = DE_DIR;
	} else if (strcasecomp(str, "file") == 0) {
	    tmp->cond = DE_FILE;
#ifdef S_IFLNK
	} else if (strcasecomp(str, "link") == 0) {
	    tmp->cond = DE_SYMLINK;
#endif /* S_IFLNK */
	}

	/*
	 * Conditional on matching suffix.
	 */
	str = cp;
	if ((cp = StrChr(str, ':')) != 0) {
	    *cp++ = '\0';
	    StrAllocCopy(tmp->sfx, str);

	    str = cp;
	    if ((cp = StrChr(str, ':')) != 0) {
		*cp++ = '\0';
		StrAllocCopy(tmp->link, str);

		str = cp;
		if ((cp = StrChr(str, ':')) != 0) {
		    *cp++ = '\0';
		    StrAllocCopy(tmp->rest, str);

		    StrAllocCopy(tmp->href, cp);

		    if (menu_head) {
			for (mp = menu_head;
			     mp && mp->next != NULL;
			     mp = mp->next) {
			    ;
			}
			if (mp != NULL) {
			    mp->next = tmp;
			    used = TRUE;
			}
		    } else {
			menu_head = tmp;
			used = TRUE;
		    }
		}
	    }
	}
    }
    if (!used)
	FREE(tmp);
}

void reset_dired_menu(void)
{
    if (menu_head != defmenu) {
	struct dired_menu *mp, *mp_next = NULL;

	for (mp = menu_head; mp != NULL; mp = mp_next) {
	    FREE(mp->sfx);
	    FREE(mp->link);
	    FREE(mp->rest);
	    FREE(mp->href);
	    mp_next = mp->next;
	    FREE(mp);
	}
	menu_head = NULL;
    }
}

/*
 * Create URL for DIRED HREF value.
 */
static char *render_item(const char *s,
			 const char *path,
			 const char *dir,
			 char *buf,
			 size_t bufsize,
			 int url_syntax)
{
    const char *cp;
    char *bp;
    char overrun = '\0';
    char *taglist = NULL;

#define BP_INC (bp>buf+bufsize-2 ?  &overrun : bp++)
    /* Buffer overrun could happen for very long
       tag list, if %l or %t are used */
    bp = buf;
    while (*s && !overrun) {
	if (*s == '%') {
	    s++;
	    switch (*s) {
	    case '%':
		*BP_INC = '%';
		break;
	    case 'p':
		cp = path;
		if (!LYIsHtmlSep(*cp))
		    *BP_INC = '/';
		while (*cp)
		    *BP_INC = *cp++;
		break;
	    case 'd':
		cp = dir;
		if (!LYIsHtmlSep(*cp))
		    *BP_INC = '/';
		while (*cp)
		    *BP_INC = *cp++;
		break;
	    case 'f':
		cp = LYLastPathSep(path);
		if (cp)
		    cp++;
		else
		    cp = path;
		while (*cp)
		    *BP_INC = *cp++;
		break;
	    case 'l':
	    case 't':
		if (!HTList_isEmpty(tagged)) {
		    HTList *cur = tagged;
		    char *name;

		    while (!overrun &&
			   (name = (char *) HTList_nextObject(cur)) != NULL) {
			if (*s == 'l' && (cp = strrchr(name, '/')))
			    cp++;
			else
			    cp = name;
			StrAllocCat(taglist, cp);
			StrAllocCat(taglist, " ");	/* should this be %20? */
		    }
		}
		if (taglist) {
		    /* could HTUnescape here... */
		    cp = taglist;
		    while (*cp)
			*BP_INC = *cp++;
		    FREE(taglist);
		}
		break;
	    default:
		*BP_INC = '%';
		*BP_INC = *s;
		break;
	    }
	} else {
	    /*
	     * Other chars come from the lynx.cfg or the default.  Let's assume
	     * there isn't anything weird there that needs escaping.
	     */
	    *BP_INC = *s;
	}
	s++;
    }
    if (overrun & url_syntax) {
	HTAlert(gettext("Temporary URL or list would be too long."));
	bp = buf;		/* set to start, will return empty string as URL */
    }
    *bp = '\0';
    return buf;
}

#endif /* DIRED_SUPPORT */
