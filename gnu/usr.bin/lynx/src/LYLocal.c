/*
**  Routines to manipulate the local filesystem.
**  Written by: Rick Mallett, Carleton University
**  Report problems to rmallett@ccs.carleton.ca
**  Modified 18-Dec-95 David Trueman (david@cs.dal.ca):
**	Added OK_PERMIT compilation option.
**	Support replacement of compiled-in f)ull menu configuration via
**	  DIRED_MENU definitions in lynx.cfg, so that more than one menu
**	  can be driven by the same executable.
**  Modified Oct-96 Klaus Weide (kweide@tezcat.com):
**	Changed to use the library's HTList_* functions and macros for
**	  managing the list of tagged file URLs.
**	Keep track of proper level of URL escaping, so that unusual filenames
**	  which contain #% etc. are handled properly (some HTUnEscapeSome()'s
**	  left in to be conservative, and to document where superfluous
**	  unescaping took place before).
**	Dynamic memory instead of fixed length buffers in a few cases.
**	Other minor changes to make things work as intended.
**  Modified Jun-97 Klaus Weide (kweide@tezcat.com) & FM:
**	Modified the code handling DIRED_MENU to do more careful
**	  checking of the selected file.  In addition to "TAG", "FILE", and
**	  "DIR", DIRED_MENU definitions in lynx.cfg now also recognize LINK as
**	  a type.  DIRED_MENU definitions with a type field of "LINK" are only
**	  used if the current selection is a symbolic link ("FILE" and "DIR"
**	  definitions are not used in that case).  The default menu
**	  definitions have been updated to reflect this change, and to avoid
**	  the showing of menu items whose action would always fail - KW
**	Cast all code into the Lynx programming style. - FM
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

#ifndef VMS
#ifndef _WINDOWS
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#endif /*_WINDOWS */
#endif /* VMS */

#ifndef WEXITSTATUS
# ifdef HAVE_TYPE_UNIONWAIT
#  define	WEXITSTATUS(status)	(status.w_retcode)
# else
#  define	WEXITSTATUS(status)	(((status) & 0xff00) >> 8)
# endif
#endif

#ifndef WTERMSIG
# ifdef HAVE_TYPE_UNIONWAIT
#  define	WTERMSIG(status)	(status.w_termsig)
# else
#  define	WTERMSIG(status)	((status) & 0x7f)
# endif
#endif

#include <LYLeaks.h>

PRIVATE int LYExecv PARAMS((
	char *		path,
	char ** 	argv,
	char *		msg));

#ifdef DIRED_SUPPORT
PUBLIC char LYPermitFileURL[LY_MAXPATH] = "\0";
PUBLIC char LYDiredFileURL[LY_MAXPATH] = "\0";

PRIVATE char *get_filename PARAMS((
	char *		prompt,
	char *		buf,
	size_t		bufsize));

#ifdef OK_PERMIT
PRIVATE BOOLEAN permit_location PARAMS((
	char *		destpath,
	char *		srcpath,
	char ** 	newpath));
#endif /* OK_PERMIT */

PRIVATE char *render_item PARAMS((
	CONST char *	s,
	char *		path,
	char *		dir,
	char *		buf,
	int		bufsize,
	BOOLEAN 	url_syntax));

PRIVATE struct dired_menu *menu_head = NULL;
struct dired_menu {
    int cond;
#define DE_TAG     1
#define DE_DIR     2
#define DE_FILE    3
#define DE_SYMLINK 4
    char *sfx;
    char *link;
    char *rest;
    char *href;
    struct dired_menu *next;
} defmenu[] = {
/*
 *  The following initializations determine the contents of the f)ull menu
 *  selection when in dired mode.  If any menu entries are defined in the
 *  configuration file via DIRED_MENU lines, then these default entries
 *  are discarded entirely.
 */
{ 0,		      "", "New File",
"(in current directory)", "LYNXDIRED://NEW_FILE%d",		NULL },

{ 0,		      "", "New Directory",
"(in current directory)", "LYNXDIRED://NEW_FOLDER%d",		NULL },

{ DE_FILE,	      "", "Install",
"(of current selection)", "LYNXDIRED://INSTALL_SRC%p",		NULL },
{ DE_DIR,	      "", "Install",
"(of current selection)", "LYNXDIRED://INSTALL_SRC%p",		NULL },

{ DE_FILE,	      "", "Modify File Name",
"(of current selection)", "LYNXDIRED://MODIFY_NAME%p",		NULL },
{ DE_DIR,	      "", "Modify Directory Name",
"(of current selection)", "LYNXDIRED://MODIFY_NAME%p",		NULL },
{ DE_SYMLINK,	      "", "Modify Name",
"(of selected symbolic link)", "LYNXDIRED://MODIFY_NAME%p",	NULL },

#ifdef OK_PERMIT
{ DE_FILE,	      "", "Modify File Permissions",
"(of current selection)", "LYNXDIRED://PERMIT_SRC%p",		NULL },
{ DE_DIR,	      "", "Modify Directory Permissions",
"(of current selection)", "LYNXDIRED://PERMIT_SRC%p",		NULL },
#endif /* OK_PERMIT */

{ DE_FILE,	      "", "Change Location",
"(of selected file)"	, "LYNXDIRED://MODIFY_LOCATION%p",	NULL },
{ DE_DIR,	      "", "Change Location",
"(of selected directory)", "LYNXDIRED://MODIFY_LOCATION%p",	NULL },
{ DE_SYMLINK,	      "", "Change Location",
"(of selected symbolic link)", "LYNXDIRED://MODIFY_LOCATION%p", NULL },

{ DE_FILE,	      "", "Remove File",
   "(current selection)", "LYNXDIRED://REMOVE_SINGLE%p",	NULL },
{ DE_DIR,	      "", "Remove Directory",
   "(current selection)", "LYNXDIRED://REMOVE_SINGLE%p",	NULL },
{ DE_SYMLINK,	      "", "Remove Symbolic Link",
   "(current selection)", "LYNXDIRED://REMOVE_SINGLE%p",	NULL },

#if defined(OK_UUDECODE) && !defined(ARCHIVE_ONLY)
{ DE_FILE,	      "", "UUDecode",
   "(current selection)", "LYNXDIRED://UUDECODE%p",		NULL },
#endif /* OK_UUDECODE && !ARCHIVE_ONLY */

#if defined(OK_TAR) && !defined(ARCHIVE_ONLY)
{ DE_FILE,	".tar.Z", "Expand",
   "(current selection)", "LYNXDIRED://UNTAR_Z%p",		NULL },
#endif /* OK_TAR && !ARCHIVE_ONLY */

#if defined(OK_TAR) && defined(OK_GZIP) && !defined(ARCHIVE_ONLY)
{ DE_FILE,     ".tar.gz", "Expand",
   "(current selection)", "LYNXDIRED://UNTAR_GZ%p",		NULL },

{ DE_FILE,	  ".tgz", "Expand",
   "(current selection)", "LYNXDIRED://UNTAR_GZ%p",		NULL },
#endif /* OK_TAR && OK_GZIP && !ARCHIVE_ONLY */

#ifndef ARCHIVE_ONLY
{ DE_FILE,	    ".Z", "Uncompress",
   "(current selection)", "LYNXDIRED://DECOMPRESS%p",		NULL },
#endif /* ARCHIVE_ONLY */

#if defined(OK_GZIP) && !defined(ARCHIVE_ONLY)
{ DE_FILE,	   ".gz", "Uncompress",
   "(current selection)", "LYNXDIRED://UNGZIP%p",		NULL },
#endif /* OK_GZIP && !ARCHIVE_ONLY */

#if defined(OK_ZIP) && !defined(ARCHIVE_ONLY)
{ DE_FILE,	  ".zip", "Uncompress",
   "(current selection)", "LYNXDIRED://UNZIP%p",		NULL },
#endif /* OK_ZIP && !ARCHIVE_ONLY */

#if defined(OK_TAR) && !defined(ARCHIVE_ONLY)
{ DE_FILE,	  ".tar", "UnTar",
   "(current selection)", "LYNXDIRED://UNTAR%p",		NULL },
#endif /* OK_TAR && !ARCHIVE_ONLY */

#ifdef OK_TAR
{ DE_DIR,	      "", "Tar",
   "(current selection)", "LYNXDIRED://TAR%p",			NULL },
#endif /* OK_TAR */

#if defined(OK_TAR) && defined(OK_GZIP)
{ DE_DIR,	      "", "Tar and compress",
      "(using GNU gzip)", "LYNXDIRED://TAR_GZ%p",		NULL },
#endif /* OK_TAR && OK_GZIP */

#ifdef OK_ZIP
{ DE_DIR,	      "", "Package and compress",
	   "(using zip)", "LYNXDIRED://ZIP%p",			NULL },
#endif /* OK_ZIP */

{ DE_FILE,	      "", "Compress",
 "(using Unix compress)", "LYNXDIRED://COMPRESS%p",		NULL },

#ifdef OK_GZIP
{ DE_FILE,	      "", "Compress",
	  "(using gzip)", "LYNXDIRED://GZIP%p", 		NULL },
#endif /* OK_GZIP */

#ifdef OK_ZIP
{ DE_FILE,	      "", "Compress",
	   "(using zip)", "LYNXDIRED://ZIP%p",			NULL },
#endif /* OK_ZIP */

{ DE_TAG,	      "", "Move all tagged items to another location.",
		      "", "LYNXDIRED://MOVE_TAGGED%d",		NULL },

{ DE_TAG,	      "", "Remove all tagged files and directories.",
		      "", "LYNXDIRED://REMOVE_TAGGED",		NULL },

{ DE_TAG,	      "", "Untag all tagged files and directories.",
		      "", "LYNXDIRED://CLEAR_TAGGED",		NULL },

{ 0,		    NULL, NULL,
		    NULL, NULL, 				NULL }
};

PRIVATE BOOLEAN cannot_stat ARGS1(char *, name)
{
    char *tmpbuf = 0;
    HTSprintf(&tmpbuf, gettext("Unable to get status of '%s'."), name);
    HTAlert(tmpbuf);
    FREE(tmpbuf);
    return FALSE;
}

PRIVATE BOOLEAN ok_stat ARGS2(char *, name, struct stat*, sb)
{
    CTRACE(tfp, "testing ok_stat(%s)\n", name);
    if (stat(name, sb) < 0) {
	return cannot_stat(name);
    }
    return TRUE;
}

#ifdef HAVE_LSTAT
PRIVATE BOOLEAN ok_lstat ARGS2(char *, name, struct stat*, sb)
{
    CTRACE(tfp, "testing ok_lstat(%s)\n", name);
    if (lstat(name, sb) < 0) {
	return cannot_stat(name);
    }
    return TRUE;
}
#else
#define ok_lstat(name,sb) ok_stat(name,sb)
#endif

PRIVATE BOOLEAN ok_file_or_dir ARGS1(struct stat*, sb)
{
    if (!S_ISDIR(sb->st_mode)
     && !S_ISREG(sb->st_mode)) {
	HTAlert(gettext("The selected item is not a file or a directory!  Request ignored."));
	return FALSE;
    }
    return TRUE;
}

PRIVATE BOOLEAN ok_localname ARGS2(char*, dst, char*, src)
{
    char *s = HTfullURL_toFile(strip_trailing_slash(src));
    struct stat dir_info;

    if (!ok_stat(s, &dir_info)
     || !ok_file_or_dir(&dir_info)) {
	FREE(s);
	return FALSE;
    }
    strcpy(dst, s);
    FREE(s);
    return TRUE;
}

PRIVATE int move_file ARGS2(char *, source, char *, target)
{
    int code;
    char *msg = 0;
    char *args[5];

    HTSprintf(&msg, gettext("move %s to %s"), source, target);
    args[0] = "mv";
    args[1] = source;
    args[2] = target;
    args[3] = (char *) 0;
    code = (LYExecv(MV_PATH, args, msg) <= 0) ? -1 : 1;
    FREE(msg);
    return code;
}

PRIVATE BOOLEAN not_already_exists ARGS1(char *, name)
{
    struct stat dir_info;

    if (stat(name, &dir_info) == -1) {
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

PRIVATE BOOLEAN dir_has_same_owner ARGS2(struct stat *, info, uid_t, owner)
{
    if (S_ISDIR(info->st_mode)) {
	if (info->st_uid == owner) {
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
 *  Remove all tagged files and directories.
 */
PRIVATE BOOLEAN remove_tagged NOARGS
{
    int ans;
    BOOL will_clear = TRUE;
    char *cp;
    char *tmpbuf = NULL;
    char *testpath = NULL;
    struct stat dir_info;
    int count;
    HTList *tag;
    char *args[5];

    if (HTList_isEmpty(tagged))  /* should never happen */
	return 0;

    ans = HTConfirm(gettext("Remove all tagged files and directories "));

    count = 0;
    tag = tagged;
    while (ans == YES && (cp = (char *)HTList_nextObject(tag)) != NULL) {
	if (is_url(cp) == FILE_URL_TYPE) { /* unnecessary check */
	    testpath = HTfullURL_toFile(cp);
	    LYTrimPathSep(testpath);
	    will_clear = TRUE;

	    /*
	     *	Check the current status of the path to be deleted.
	     */
	    if (!ok_stat(testpath, &dir_info)) {
		will_clear = FALSE;
		break;
	    } else {
		args[0] = "rm";
		args[1] = "-rf";
		args[2] = testpath;
		args[3] = (char *) 0;
		HTSprintf0(&tmpbuf, gettext("remove %s"), testpath);
		if (LYExecv(RM_PATH, args, tmpbuf) <= 0) {
		    if (count == 0) count = -1;
		    will_clear = FALSE;
		    break;
		}
		++count;
	    }
	}
    }
    FREE(testpath);
    FREE(tmpbuf);
    if (will_clear)
	clear_tags();
    return count;
}

/*
 *  Move all tagged files and directories to a new location.
 *  Input is current directory.
 *  The tests in this function can, at best, prevent some user mistakes -
 *   anybody who relies on them for security is seriously misguided.
 *  If a user has enough permissions to move a file somewhere, the same
 *   uid with Lynx & dired can do the same thing.
 */
PRIVATE BOOLEAN modify_tagged ARGS1(
	char *, 	testpath)
{
    char *cp;
    dev_t dev;
    ino_t inode;
    uid_t owner;
    char tmpbuf[1024];
    char *savepath = NULL;
    char *srcpath = NULL;
    struct stat dir_info;
    int count = 0;
    HTList *tag;

    if (HTList_isEmpty(tagged))  /* should never happen */
	return 0;

    _statusline(gettext("Enter new location for tagged items: "));

    tmpbuf[0] = '\0';
    LYgetstr(tmpbuf, VISIBLE, sizeof(tmpbuf), NORECALL);
    if (strlen(tmpbuf)) {
    /*
     *	Determine the ownership of the current location.
     */
	/*
	 *  This test used to always fail from the dired menu...
	 *  changed to something that hopefully makes more sense - KW
	 */
	if (testpath && *testpath && 0!=strcmp(testpath,"/")) {
	    /*
	     *	testpath passed in and is not empty and not a single "/"
	     *	(which would probably be bogus) - use it.
	     */
	    cp = testpath;
	} else {
	    /*
	     *	Prepare to get directory path from one of the tagged files.
	     */
	    cp = HTList_lastObject(tagged);
	    testpath = NULL;	/* Won't be needed any more in this function,
				   set to NULL as a flag. */
	    if (!cp)	/* Last resort, should never happen. */
		cp = "/";
	}

	if (testpath == NULL) {
	    /*
	     *	Get the directory containing the file or subdir.
	     */
	    cp = HTfullURL_toFile(strip_trailing_slash(cp));
	    savepath = HTParse(".", cp, PARSE_PATH+PARSE_PUNCTUATION);
	} else {
	    cp = HTfullURL_toFile(cp);
	    StrAllocCopy(savepath, cp);
	}
	FREE(cp);

	if (!ok_stat(savepath, &dir_info)) {
	    FREE(savepath);
	    return 0;
	}

	/*
	 *  Save the owner of the current location for later use.
	 *  Also save the device and inode for location checking/
	 */
	dev = dir_info.st_dev;
	inode = dir_info.st_ino;
	owner = dir_info.st_uid;

	/*
	 *  Replace ~/ references to the home directory.
	 */
	if (!strncmp(tmpbuf, "~/", 2)) {
	    char *cp1 = NULL;
	    StrAllocCopy(cp1, Home_Dir());
	    StrAllocCat(cp1, (tmpbuf + 1));
	    if (strlen(cp1) > (sizeof(tmpbuf) - 1)) {
		HTAlert(gettext("Path too long"));
		FREE(savepath);
		FREE(cp1);
		return 0;
	    }
	    strcpy(tmpbuf, cp1);
	    FREE(cp1);
	}

	/*
	 *  If path is relative, prefix it with current location.
	 */
	if (!LYIsPathSep(tmpbuf[0])) {
	    LYAddPathSep(&savepath);
	    StrAllocCat(savepath,tmpbuf);
	} else {
	    StrAllocCopy(savepath,tmpbuf);
	}

	/*
	 *  stat() the target location to determine type and ownership.
	 */
	if (!ok_stat(savepath, &dir_info)) {
	    FREE(savepath);
	    return 0;
	}

	/*
	 *  Make sure the source and target locations are not the same place.
	 */
	if (dev == dir_info.st_dev && inode == dir_info.st_ino) {
	    HTAlert(gettext("Source and destination are the same location - request ignored!"));
	    FREE(savepath);
	    return 0;
	}

	/*
	 *  Make sure the target location is a directory which is owned
	 * by the same uid as the owner of the current location.
	 */
	if (dir_has_same_owner(&dir_info, owner)) {
	    count = 0;
	    tag = tagged;

	    /*
	     *  Move all tagged items to the target location.
	     */
	    while ((cp = (char *)HTList_nextObject(tag)) != NULL) {
		cp = HTfullURL_toFile(cp);
		StrAllocCopy(srcpath, cp);

		if (move_file(srcpath, savepath) < 0) {
		    FREE(cp);
		    if (count == 0)
			count = -1;
		    break;
		}
		FREE(cp);
		++count;
	    }
	    clear_tags();
	    FREE(srcpath);
	}
	FREE(savepath);
	return count;
    }
    return 0;
}

/*
 *  Modify the name of the specified item.
 */
PRIVATE BOOLEAN modify_name ARGS1(
	char *, 	testpath)
{
    char *cp;
    char tmpbuf[512];
    char newpath[512];
    char savepath[512];
    struct stat dir_info;

    /*
     *	Determine the status of the selected item.
     */
    testpath = strip_trailing_slash(testpath);

    if (ok_stat(testpath, &dir_info)) {
	/*
	 *  Change the name of the file or directory.
	 */
	if (S_ISDIR(dir_info.st_mode)) {
	     cp = gettext("Enter new name for directory: ");
	} else if (S_ISREG(dir_info.st_mode)) {
	     cp = gettext("Enter new name for file: ");
	} else {
	     return ok_file_or_dir(&dir_info);
	}
	if (get_filename(cp, tmpbuf, sizeof(tmpbuf)) == NULL)
	    return 0;

	/*
	 *  Do not allow the user to also change the location at this time.
	 */
	if (strchr(tmpbuf, '/') != NULL) {
	    HTAlert(gettext("Illegal character \"/\" found! Request ignored."));
	} else if (strlen(tmpbuf) &&
		   (cp = strrchr(testpath, '/')) != NULL) {
	    strcpy(savepath,testpath);
	    *(++cp) = '\0';
	    strcpy(newpath,testpath);
	    strcat(newpath,tmpbuf);

	    /*
	     *	Make sure the destination does not already exist.
	     */
	    if (not_already_exists(newpath)) {
		return move_file(savepath, newpath);
	    }
	}
    }
    return 0;
}

/*
 *  Change the location of a file or directory.
 */
PRIVATE BOOLEAN modify_location ARGS1(
	char *, 	testpath)
{
    char *cp;
    dev_t dev;
    ino_t inode;
    uid_t owner;
    char tmpbuf[1024];
    char newpath[512];
    char savepath[512];
    struct stat dir_info;

    /*
     *	Determine the status of the selected item.
     */
    testpath = strip_trailing_slash(testpath);

    if (!ok_stat(testpath, &dir_info)) {
	return 0;
    }

    /*
     *	Change the location of the file or directory.
     */
    if (S_ISDIR(dir_info.st_mode)) {
	cp = gettext("Enter new location for directory: ");
    } else if (S_ISREG(dir_info.st_mode)) {
	cp = gettext("Enter new location for file: ");
    } else {
	return ok_file_or_dir(&dir_info);
    }
    if (get_filename(cp, tmpbuf, sizeof(tmpbuf)) == NULL)
	return 0;
    if (strlen(tmpbuf)) {
	strcpy(savepath, testpath);
	strcpy(newpath, testpath);

	/*
	 *  Allow ~/ references to the home directory.
	 */
	if (!strncmp(tmpbuf, "~/", 2)
	 || !strcmp(tmpbuf,"~")) {
	    strcpy(newpath, Home_Dir());
	    strcat(newpath, (tmpbuf + 1));
	    strcpy(tmpbuf, newpath);
	}
	if (!LYIsPathSep(tmpbuf[0])) {
	    if ((cp = strrchr(newpath,'/')) != NULL) {
		*++cp = '\0';
		strcat(newpath,tmpbuf);
	    } else {
		HTAlert(gettext("Unexpected failure - unable to find trailing \"/\""));
		return 0;
	    }
	} else {
	    strcpy(newpath,tmpbuf);
	}

	/*
	 *  Make sure the source and target have the same owner (uid).
	 */
	dev = dir_info.st_dev;
	inode = dir_info.st_ino;
	owner = dir_info.st_uid;
	if (!ok_stat(newpath, &dir_info)) {
	    return 0;
	}

	/*
	 *  Make sure the source and target are not the same location.
	 */
	if (dev == dir_info.st_dev && inode == dir_info.st_ino) {
	    HTAlert(gettext("Source and destination are the same location!  Request ignored!"));
	    return 0;
	}
	if (dir_has_same_owner(&dir_info, owner)) {
	    return move_file(savepath,newpath);
	}
    }
    return 0;
}

/*
 *  Modify name or location of a file or directory on localhost.
 */
PUBLIC BOOLEAN local_modify ARGS2(
	document *,	doc,
	char **,	newpath)
{
    int c, ans;
    char *cp;
    char testpath[512]; /* a bit ridiculous */
    int count;

    if (!HTList_isEmpty(tagged)) {
	cp = HTpartURL_toFile(doc->address);
	strcpy(testpath, cp);
	FREE(cp);

	count = modify_tagged(testpath);

	if (doc->link > (nlinks-count - 1))
	    doc->link = (nlinks-count - 1);
	doc->link = (doc->link < 0) ?
				  0 : doc->link;

	return count;
    } else if (doc->link < 0 || doc->link > nlinks) {
	/*
	 *  Added protection.
	 */
	return 0;
    }

    /*
     *	Do not allow simultaneous change of name and location as in Unix.
     *	This reduces functionality but reduces difficulty for the novice.
     */
#ifdef OK_PERMIT
    _statusline(gettext("Modify name, location, or permission (n, l, or p): "));
#else
    _statusline(gettext("Modify name, or location (n or l): "));
#endif /* OK_PERMIT */
    c = LYgetch();
    ans = TOUPPER(c);

    if (strchr("NLP", ans) != NULL) {
	cp = HTfullURL_toFile(links[doc->link].lname);
	strcpy(testpath, cp);
	FREE(cp);

	if (ans == 'N') {
	    return(modify_name(testpath));
	} else if (ans == 'L') {
	    if (modify_location(testpath)) {
		if (doc->link == (nlinks-1))
		    --doc->link;
		return 1;
	    }
#ifdef OK_PERMIT
	} else if (ans == 'P') {
	    return(permit_location(NULL, testpath, newpath));
#endif /* OK_PERMIT */
	} else {
	    /*
	     *	Code for changing ownership needed here.
	     */
	    HTAlert(gettext("This feature not yet implemented!"));
	}
    }
    return 0;
}

/*
 *  Create a new empty file in the current directory.
 */
PRIVATE BOOLEAN create_file ARGS1(
	char *, 	current_location)
{
    int code = FALSE;
    char tmpbuf[512];
    char testpath[512];
    char *args[5];
    char *bad_chars = ".~/";

    if (get_filename(gettext("Enter name of file to create: "),
		     tmpbuf, sizeof(tmpbuf)) == NULL) {
	return code;
    }

    if (!no_dotfiles && show_dotfiles) {
	bad_chars = "~/";
    }

    if (strstr(tmpbuf, "//") != NULL) {
	HTAlert(gettext("Illegal redirection \"//\" found! Request ignored."));
    } else if (strlen(tmpbuf) && strchr(bad_chars, tmpbuf[0]) == NULL) {
	strcpy(testpath,current_location);
	LYAddPathSep0(testpath);

	/*
	 *  Append the target filename to the current location.
	 */
	strcat(testpath, tmpbuf);

	/*
	 *  Make sure the target does not already exist
	 */
	if (not_already_exists(testpath)) {
	    char *msg = 0;
	    HTSprintf(&msg,gettext("create %s"),testpath);
	    args[0] = "touch";
	    args[1] = testpath;
	    args[2] = (char *) 0;
	    code = (LYExecv(TOUCH_PATH, args, msg) <= 0) ? -1 : 1;
	    FREE(msg);
	}
    }
    return code;
}

/*
 *  Create a new directory in the current directory.
 */
PRIVATE BOOLEAN create_directory ARGS1(
	char *, 	current_location)
{
    int code = FALSE;
    char tmpbuf[512];
    char testpath[512];
    char *args[5];
    char *bad_chars = ".~/";

    if (get_filename(gettext("Enter name for new directory: "),
		     tmpbuf, sizeof(tmpbuf)) == NULL) {
	return code;
    }

    if (!no_dotfiles && show_dotfiles) {
	bad_chars = "~/";
    }

    if (strstr(tmpbuf, "//") != NULL) {
	HTAlert(gettext("Illegal redirection \"//\" found! Request ignored."));
    } else if (strlen(tmpbuf) && strchr(bad_chars, tmpbuf[0]) == NULL) {
	strcpy(testpath,current_location);
	LYAddPathSep0(testpath);

	strcat(testpath, tmpbuf);

	/*
	 *  Make sure the target does not already exist.
	 */
	if (not_already_exists(testpath)) {
	    char *msg = 0;
	    HTSprintf(&msg,"make directory %s",testpath);
	    args[0] = "mkdir";
	    args[1] = testpath;
	    args[2] = (char *) 0;
	    code = (LYExecv(MKDIR_PATH, args, msg) <= 0) ? -1 : 1;
	    FREE(msg);
	}
    }
    return code;
}

/*
 *  Create a file or a directory at the current location.
 */
PUBLIC BOOLEAN local_create ARGS1(
	document *,	doc)
{
    int c, ans;
    char *cp;
    char testpath[512];

    _statusline(gettext("Create file or directory (f or d): "));
    c = LYgetch();
    ans = TOUPPER(c);

    cp = HTfullURL_toFile(doc->address);
    strcpy(testpath,cp);
    FREE(cp);

    if (ans == 'F') {
	return(create_file(testpath));
    } else if (ans == 'D') {
	return(create_directory(testpath));
    } else {
	return 0;
    }
}

/*
 *  Remove a single file or directory.
 */
PRIVATE BOOLEAN remove_single ARGS1(
	char *, 	testpath)
{
    int code = 0;
    char *cp;
    char *tmpbuf = 0;
    struct stat dir_info;
    char *args[5];

    if (!ok_lstat(testpath, &dir_info)) {
	return 0;
    }

    /*
     *	Locate the filename portion of the path.
     */
    if ((cp = strrchr(testpath, '/')) != NULL) {
	++cp;
    } else {
	cp = testpath;
    }
    if (S_ISDIR(dir_info.st_mode)) {
	/*** This strlen stuff will probably screw up intl translations /jes ***/
	/*** Course, it's probably broken for screen sizes other 80, too     ***/
	if (strlen(cp) < 37) {
	    HTSprintf0(&tmpbuf,
		       gettext("Remove '%s' and all of its contents: "), cp);
	} else {
	    HTSprintf0(&tmpbuf,
		       gettext("Remove directory and all of its contents: "));
	}
    } else if (S_ISREG(dir_info.st_mode)) {
	if (strlen(cp) < 60) {
	    HTSprintf0(&tmpbuf, gettext("Remove file '%s': "), cp);
	} else {
	    HTSprintf0(&tmpbuf, gettext("Remove file: "));
	}
#ifdef S_IFLNK
    } else if (S_ISLNK(dir_info.st_mode)) {
	if (strlen(cp) < 50) {
	    HTSprintf0(&tmpbuf, gettext("Remove symbolic link '%s': "), cp);
	} else {
	    HTSprintf0(&tmpbuf, gettext("Remove symbolic link: "));
	}
#endif
    } else {
	cannot_stat(testpath);
	FREE(tmpbuf);
	return 0;
    }

    if (HTConfirm(tmpbuf) == YES) {
	HTSprintf0(&tmpbuf,"remove %s",testpath);
	args[0] = "rm";
	args[1] = "-rf";
	args[2] = testpath;
	args[3] = (char *) 0;
	code = (LYExecv(RM_PATH, args, tmpbuf) <= 0) ? -1 : 1;
    }
    FREE(tmpbuf);
    return code;
}

/*
 *  Remove a file or a directory.
 */
PUBLIC BOOLEAN local_remove ARGS1(
	document *,	doc)
{
    char *cp, *tp;
    char testpath[512];
    int count, i;

    if (!HTList_isEmpty(tagged)) {
	count = remove_tagged();
	if (doc->link > (nlinks-count - 1))
	    doc->link = (nlinks-count - 1);
	doc->link = (doc->link < 0) ?
				  0 : doc->link;
	return count;
    } else if (doc->link < 0 || doc->link > nlinks) {
	return 0;
    }
    cp = links[doc->link].lname;
    if (is_url(cp) == FILE_URL_TYPE) {
	tp = HTfullURL_toFile(cp);
	strcpy(testpath, tp);
	FREE(tp);

	if ((i = strlen(testpath)) && testpath[i - 1] == '/')
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
/*
 *  Table of permission strings and chmod values.
 *  Makes the code a bit cleaner.
 */
static struct {
    CONST char *string_mode;	/* Key for  value below */
    long permit_bits;		/* Value for chmod/whatever */
} permissions[] = {
    {"IRUSR", S_IRUSR},
    {"IWUSR", S_IWUSR},
    {"IXUSR", S_IXUSR},
    {"IRGRP", S_IRGRP},
    {"IWGRP", S_IWGRP},
    {"IXGRP", S_IXGRP},
    {"IROTH", S_IROTH},
    {"IWOTH", S_IWOTH},
    {"IXOTH", S_IXOTH},
    {NULL, 0}			/* Don't include setuid and friends;
				   use shell access for that. */
};

PRIVATE char LYValidPermitFile[LY_MAXPATH] = "\0";

/*
 *  Handle DIRED permissions.
 */
PRIVATE BOOLEAN permit_location ARGS3(
	char *, 	destpath,
	char *, 	srcpath,
	char **,	newpath)
{
#ifndef UNIX
    HTAlert(gettext("Sorry, don't know how to permit non-UNIX files yet."));
    return(0);
#else
    static char tempfile[LY_MAXPATH] = "\0";
    char *cp;
    char *tmpbuf = NULL;
    char tmpdst[LY_MAXPATH];
    struct stat dir_info;

    if (srcpath) {
	/*
	 *  Create form.
	 */
	FILE *fp0;
	char local_src[LY_MAXPATH];
	char * user_filename;
	char * group_name;

	cp = HTfullURL_toFile(strip_trailing_slash(srcpath));
	strcpy(local_src, cp);
	FREE(cp);

	/*
	 *  A couple of sanity tests.
	 */
	if (!ok_lstat(local_src, &dir_info)
	 || !ok_file_or_dir(&dir_info))
	    return 0;

	user_filename = LYPathLeaf(local_src);

	LYRemoveTemp(tempfile);
	if ((fp0 = LYOpenTemp(tempfile, HTML_SUFFIX, "w")) == NULL) {
	    HTAlert(gettext("Unable to open permit options file"));
	    return(0);
	}

	/*
	 * Make the tempfile a URL.
	 */
	LYLocalFileToURL(newpath, tempfile);
	strcpy(LYPermitFileURL, *newpath);

	group_name = HTAA_GidToName (dir_info.st_gid);
	LYstrncpy(LYValidPermitFile,
		  local_src,
		  (sizeof(LYValidPermitFile) - 1));

	fprintf(fp0, "<Html><Head>\n<Title>%s</Title>\n</Head>\n<Body>\n",
		PERMIT_OPTIONS_TITLE);
      fprintf(fp0,"<H1>%s%s</H1>\n", PERMISSIONS_SEGMENT, user_filename);
	{   /*
	     *	Prevent filenames which include '#' or '?' from messing it up.
	     */
	    char * srcpath_url = HTEscape(srcpath, URL_PATH);
	    fprintf(fp0, "<Form Action=\"LYNXDIRED://PERMIT_LOCATION%s\">\n",
		    srcpath_url);
	    FREE(srcpath_url);
	}

	fprintf(fp0, "<Ol><Li>%s<Br><Br>\n", gettext("Specify permissions below:"));
	fprintf(fp0, "%s:<Br>\n", gettext("Owner:"));
	fprintf(fp0,
     "<Input Type=\"checkbox\" Name=\"mode\" Value=\"IRUSR\" %s> Read<Br>\n",
		(dir_info.st_mode & S_IRUSR) ? "checked" : "");
	fprintf(fp0,
    "<Input Type=\"checkbox\" Name=\"mode\" Value=\"IWUSR\" %s> Write<Br>\n",
		(dir_info.st_mode & S_IWUSR) ? "checked" : "");
	/*
	 *  If restricted, only change eXecute permissions on directories.
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
	 *  If restricted, only change eXecute permissions on directories.
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
	 *  If restricted, only change eXecute permissions on directories.
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
	return(PERMIT_FORM_RESULT);	 /* Special flag for LYMainLoop */

    } else {				 /* The form being activated. */
	mode_t new_mode = 0;
	char *args[5];
	char amode[10];

	/*
	 *  Make sure we have a valid set-permission
	 *  file comparison string loaded via a previous
	 *  call with srcpath != NULL. - KW
	 */
	if (LYValidPermitFile[0] == '\0') {
	    if (LYCursesON)
		HTAlert(INVALID_PERMIT_URL);
	    else
		fprintf(stderr, "%s\n", INVALID_PERMIT_URL);
	    CTRACE(tfp, "permit_location: called for <%s>.\n",
			(destpath ?
			 destpath : "NULL URL pointer"));
	    return 0;
	}
	cp = destpath;
	while (*cp != '\0' && *cp != '?') { /* Find filename */
	    cp++;
	}
	if (*cp == '\0') {
	    return(0);	/* Nothing to permit. */
	}
	*cp++ = '\0';	/* Null terminate file name and
			   start working on the masks. */

	if ((destpath = HTfullURL_toFile(destpath)) == 0)
		return(0);

	strcpy(tmpdst, destpath);	/* operate only on filename */
	FREE(destpath);
	destpath = tmpdst;

	/*
	 *  Make sure that the file string is the one from
	 *  the last displayed File Permissions menu. - KW
	 */
	if (strcmp(destpath, LYValidPermitFile)) {
	    if (LYCursesON)
		HTAlert(INVALID_PERMIT_URL);
	    else
		fprintf(stderr, "%s\n", INVALID_PERMIT_URL);
	    CTRACE(tfp, "permit_location: called for file '%s'.\n",
			destpath);
	    return 0;
	}

	/*
	 *  A couple of sanity tests.
	 */
	destpath = strip_trailing_slash(destpath);
	if (!ok_stat(destpath, &dir_info)
	 || !ok_file_or_dir(&dir_info)) {
	    return 0;
	}

	/*
	 *  Cycle over permission strings.
	 */
	while(*cp != '\0') {
	    char *cr = cp;

	    while(*cr != '\0' && *cr != '&') { /* GET data split by '&'. */
		cr++;
	    }
	    if (*cr != '\0') {
		*cr++ = '\0';
	    }
	    if (strncmp(cp, "mode=", 5) == 0) { /* Magic string. */
		int i;

		for(i = 0; permissions[i].string_mode != NULL; i++) {
		    if (strcmp(permissions[i].string_mode, cp+5) == 0) {
			/*
			 *  If restricted, only change eXecute
			 *  permissions on directories.
			 */
			if (!no_change_exec_perms ||
			    strchr(cp+5,'X') == NULL ||
			    S_ISDIR(dir_info.st_mode))
			    new_mode |= permissions[i].permit_bits;
			break;
		    }
		}
		if (permissions[i].string_mode == NULL) {
		    HTAlert(gettext("Invalid mode format."));
		    return 0;
		}
	    } else {
		HTAlert(gettext("Invalid syntax format."));
		return 0;
	    }

	    cp = cr;
	}

#ifdef UNIX
	/*
	 *  Call chmod().
	 */
	HTSprintf(&tmpbuf, "chmod %.4o %s", (unsigned int)new_mode, destpath);
	sprintf(amode, "%.4o", (unsigned int)new_mode);
	args[0] = "chmod";
	args[1] = amode;
	args[2] = destpath;
	args[3] = (char *) 0;
	if (LYExecv(CHMOD_PATH, args, tmpbuf) <= 0) {
	    FREE(tmpbuf);
	    return (-1);
	}
	FREE(tmpbuf);
#endif /* UNIX */
	LYforce_no_cache = TRUE;	/* Force update of dired listing. */
	return 1;
    }
#endif /* !UNIX */
}
#endif /* OK_PERMIT */

/*
 *  Display or remove a tag from a given link.
 */
PUBLIC void tagflag ARGS2(
	int,		flag,
	int,		cur)
{
    if (nlinks > 0) {
	move(links[cur].ly, 2);
	stop_reverse();
	if (flag == ON) {
	    addch('+');
	} else {
	    addch(' ');
	}

#if defined(FANCY_CURSES) || defined(USE_SLANG)
	if (!LYShowCursor)
	    move((LYlines - 1), (LYcols - 1)); /* get cursor out of the way */
	else
#endif /* FANCY CURSES || USE_SLANG */
	    /*
	     *	Never hide the cursor if there's no FANCY CURSES.
	     */
	    move(links[cur].ly, links[cur].lx);

	refresh();
    }
}

/*
 *  Handle DIRED tags.
 */
PUBLIC void showtags ARGS1(
	HTList *,	t)
{
    int i;
    HTList *s;
    char *name;

    for (i = 0; i < nlinks; i++) {
	s = t;
	while ((name = HTList_nextObject(s)) != NULL) {
	    if (!strcmp(links[i].lname, name)) {
		tagflag(ON, i);
		break;
	    }
	}
    }
}

PRIVATE char * DirectoryOf ARGS1(
	char *,		pathname)
{
    char *result = 0;
    char *leaf;

    StrAllocCopy(result, pathname);
    leaf = LYPathLeaf(result);
    if (leaf != result) {
	*leaf = '\0';
	LYTrimPathSep(result);
    }
    return result;
}

/*
 *  Perform file management operations for LYNXDIRED URL's.
 *  Attempt to be consistent.  These are (pseudo) URLs - i.e., they should
 *  be in URL syntax: some bytes will be URL-escaped with '%'.	This is
 *  necessary because these (pseudo) URLs will go through some of the same
 *  kinds of interpretations and mutilations as real ones: HTParse, stripping
 *  off #fragments etc.  (Some access schemes currently have special rules
 *  about not escaping parsing '#' "the URL way" built into HTParse, but that
 *  doesn't look like a clean way.)
 */
PUBLIC int local_dired ARGS1(
	document *,	doc)
{
    char *line_url;    /* will point to doc's address, which is a URL */
    char *line = NULL; /* same as line_url, but HTUnEscaped, will be alloced */
    char *tp = NULL;
    char *tmpbuf = NULL;
    char *buffer = NULL;
    char *dirname = NULL;

    line_url = doc->address;
    CTRACE(tfp, "local_dired: called for <%s>.\n",
		(line_url ?
		 line_url : gettext("NULL URL pointer")));
    HTUnEscapeSome(line_url, "/");	/* don't mess too much with *doc */

    StrAllocCopy(line, line_url);
    HTUnEscape(line);	/* _file_ (not URL) syntax, for those functions
			   that need it.  Don't forget to FREE it. */

    if (!strncmp(line, "LYNXDIRED://NEW_FILE", 20)) {
	if (create_file(&line[20]) > 0)
	    LYforce_no_cache = TRUE;
    } else if (!strncmp(line, "LYNXDIRED://NEW_FOLDER", 22)) {
	if (create_directory(&line[22]) > 0)
	    LYforce_no_cache = TRUE;
    } else if (!strncmp(line, "LYNXDIRED://INSTALL_SRC", 23)) {
	local_install(NULL, &line[23], &tp);
	StrAllocCopy(doc->address, tp);
	FREE(tp);
	FREE(line);
	return 0;
    } else if (!strncmp(line, "LYNXDIRED://INSTALL_DEST", 24)) {
	local_install(&line[24], NULL, &tp);
	LYpop(doc);
    } else if (!strncmp(line, "LYNXDIRED://MODIFY_NAME", 23)) {
	if (modify_name(&line[23]) > 0)
	LYforce_no_cache = TRUE;
    } else if (!strncmp(line, "LYNXDIRED://MODIFY_LOCATION", 27)) {
	if (modify_location(&line[27]) > 0)
	    LYforce_no_cache = TRUE;
    } else if (!strncmp(line, "LYNXDIRED://MOVE_TAGGED", 23)) {
	if (modify_tagged(&line_url[23]) > 0)
	    LYforce_no_cache = TRUE;
#ifdef OK_PERMIT
    } else if (!strncmp(line, "LYNXDIRED://PERMIT_SRC", 22)) {
	permit_location(NULL, &line[22], &tp);
	if (tp)
	    /*
	     *	One of the checks may have failed.
	     */
	    StrAllocCopy(doc->address, tp);
	FREE(line);
	FREE(tp);
	return 0;
    } else if (!strncmp(line, "LYNXDIRED://PERMIT_LOCATION", 27)) {
	permit_location(&line_url[27], NULL, &tp);
#endif /* OK_PERMIT */
    } else if (!strncmp(line, "LYNXDIRED://REMOVE_SINGLE", 25)) {
	if (remove_single(&line[25]) > 0)
	    LYforce_no_cache = TRUE;
    } else if (!strncmp(line, "LYNXDIRED://REMOVE_TAGGED", 25)) {
	if (remove_tagged())
	    LYforce_no_cache = TRUE;
    } else if (!strncmp(line, "LYNXDIRED://CLEAR_TAGGED", 24)) {
	clear_tags();
    } else if (!strncmp(line, "LYNXDIRED://UPLOAD", 18)) {
	/*
	 *  They're written by LYUpload_options() HTUnEscaped;
	 *  don't want to change that for now... so pass through
	 *  without more unescaping.  Directory names containing
	 *  '#' will probably fail.
	 */
	if (LYUpload(line_url))
	    LYforce_no_cache = TRUE;
    } else {
	LYTrimPathSep(line);
	if (strrchr(line, '/') == NULL) {
	    FREE(line);
	    return 0;
	}

	/*
	 *  Construct the appropriate system command taking care to
	 *  escape all path references to avoid spoofing the shell.
	 */
	if (!strncmp(line, "LYNXDIRED://DECOMPRESS", 22)) {
#define FMT_UNCOMPRESS "%s %s"
	    HTAddParam(&buffer, FMT_UNCOMPRESS, 1, UNCOMPRESS_PATH);
	    HTAddParam(&buffer, FMT_UNCOMPRESS, 2, line+22);
	    HTEndParam(&buffer, FMT_UNCOMPRESS, 2);

#if defined(OK_UUDECODE) && !defined(ARCHIVE_ONLY)
	} else if (!strncmp(line, "LYNXDIRED://UUDECODE", 20)) {
#define FMT_UUDECODE "%s %s"
	    HTAddParam(&buffer, FMT_UUDECODE, 1, UUDECODE_PATH);
	    HTAddParam(&buffer, FMT_UUDECODE, 2, line+20);
	    HTEndParam(&buffer, FMT_UUDECODE, 2);
	    HTAlert(gettext("Warning!  UUDecoded file will exist in the directory you started Lynx."));
#endif /* OK_UUDECODE && !ARCHIVE_ONLY */

#ifdef OK_TAR
# ifndef ARCHIVE_ONLY
#  ifdef OK_GZIP
	} else if (!strncmp(line, "LYNXDIRED://UNTAR_GZ", 20)) {
#define FMT_UNTAR_GZ "%s -qdc %s | (cd %s; %s -xf -)"
	    dirname = DirectoryOf(line+20);
	    HTAddParam(&buffer, FMT_UNTAR_GZ, 1, GZIP_PATH);
	    HTAddParam(&buffer, FMT_UNTAR_GZ, 2, line+20);
	    HTAddParam(&buffer, FMT_UNTAR_GZ, 3, dirname);
	    HTAddParam(&buffer, FMT_UNTAR_GZ, 4, TAR_PATH);
	    HTEndParam(&buffer, FMT_UNTAR_GZ, 4);
#  endif /* OK_GZIP */

	} else if (!strncmp(line, "LYNXDIRED://UNTAR_Z", 19)) {
#define FMT_UNTAR_Z "%s %s | (cd %s; %s -xf -)"
	    dirname = DirectoryOf(line+19);
	    HTAddParam(&buffer, FMT_UNTAR_Z, 1, ZCAT_PATH);
	    HTAddParam(&buffer, FMT_UNTAR_Z, 2, line+19);
	    HTAddParam(&buffer, FMT_UNTAR_Z, 3, dirname);
	    HTAddParam(&buffer, FMT_UNTAR_Z, 4, TAR_PATH);
	    HTEndParam(&buffer, FMT_UNTAR_Z, 4);

	} else if (!strncmp(line, "LYNXDIRED://UNTAR", 17)) {
#define FMT_UNTAR "cd %s; %s -xf %s"
	    dirname = DirectoryOf(line+17);
	    HTAddParam(&buffer, FMT_UNTAR, 1, dirname);
	    HTAddParam(&buffer, FMT_UNTAR, 2, TAR_PATH);
	    HTAddParam(&buffer, FMT_UNTAR, 3, line+17);
	    HTEndParam(&buffer, FMT_UNTAR, 3);
# endif /* !ARCHIVE_ONLY */

# ifdef OK_GZIP
	} else if (!strncmp(line, "LYNXDIRED://TAR_GZ", 18)) {
#define FMT_TAR_GZ "(cd %s; %s -cf - %s) | %s -qc >%s/%s.tar.gz"
	    dirname = DirectoryOf(line+18);
	    HTAddParam(&buffer, FMT_TAR_GZ, 1, dirname);
	    HTAddParam(&buffer, FMT_TAR_GZ, 2, TAR_PATH);
	    HTAddParam(&buffer, FMT_TAR_GZ, 3, LYPathLeaf(line+18));
	    HTAddParam(&buffer, FMT_TAR_GZ, 4, GZIP_PATH);
	    HTAddParam(&buffer, FMT_TAR_GZ, 5, dirname);
	    HTAddParam(&buffer, FMT_TAR_GZ, 6, LYPathLeaf(line+18));
	    HTEndParam(&buffer, FMT_TAR_GZ, 6);
# endif /* OK_GZIP */

	} else if (!strncmp(line, "LYNXDIRED://TAR_Z", 17)) {
#define FMT_TAR_Z "(cd %s; %s -cf - %s) | %s >%s/%s.tar.Z"
	    dirname = DirectoryOf(line+17);
	    HTAddParam(&buffer, FMT_TAR_Z, 1, dirname);
	    HTAddParam(&buffer, FMT_TAR_Z, 2, TAR_PATH);
	    HTAddParam(&buffer, FMT_TAR_Z, 3, LYPathLeaf(line+17));
	    HTAddParam(&buffer, FMT_TAR_Z, 4, COMPRESS_PATH);
	    HTAddParam(&buffer, FMT_TAR_Z, 5, dirname);
	    HTAddParam(&buffer, FMT_TAR_Z, 6, LYPathLeaf(line+17));
	    HTEndParam(&buffer, FMT_TAR_Z, 6);

	} else if (!strncmp(line, "LYNXDIRED://TAR", 15)) {
#define FMT_TAR "(cd %s; %s -cf %s.tar %s)"
	    dirname = DirectoryOf(line+15);
	    HTAddParam(&buffer, FMT_TAR, 1, dirname);
	    HTAddParam(&buffer, FMT_TAR, 2, TAR_PATH);
	    HTAddParam(&buffer, FMT_TAR, 3, LYPathLeaf(line+15));
	    HTAddParam(&buffer, FMT_TAR, 4, LYPathLeaf(line+15));
	    HTEndParam(&buffer, FMT_TAR, 4);
#endif /* OK_TAR */

#ifdef OK_GZIP
	} else if (!strncmp(line, "LYNXDIRED://GZIP", 16)) {
#define FMT_GZIP "%s -q %s"
	    HTAddParam(&buffer, FMT_GZIP, 1, GZIP_PATH);
	    HTAddParam(&buffer, FMT_GZIP, 2, line+16);
	    HTEndParam(&buffer, FMT_GZIP, 2);
#ifndef ARCHIVE_ONLY
	} else if (!strncmp(line, "LYNXDIRED://UNGZIP", 18)) {
#define FMT_UNGZIP "%s -d %s"
	    HTAddParam(&buffer, FMT_UNGZIP, 1, GZIP_PATH);
	    HTAddParam(&buffer, FMT_UNGZIP, 2, line+18);
	    HTEndParam(&buffer, FMT_UNGZIP, 2);
#endif /* !ARCHIVE_ONLY */
#endif /* OK_GZIP */

#ifdef OK_ZIP
	} else if (!strncmp(line, "LYNXDIRED://ZIP", 15)) {
#define FMT_ZIP "cd %s; %s -rq %s.zip %s"
	    dirname = DirectoryOf(line+15);
	    HTAddParam(&buffer, FMT_ZIP, 1, dirname);
	    HTAddParam(&buffer, FMT_ZIP, 2, ZIP_PATH);
	    HTAddParam(&buffer, FMT_ZIP, 3, line+15);
	    HTAddParam(&buffer, FMT_ZIP, 4, LYPathLeaf(line+15));
	    HTEndParam(&buffer, FMT_ZIP, 4);
#ifndef ARCHIVE_ONLY
	} else if (!strncmp(line, "LYNXDIRED://UNZIP", 17)) {
#define FMT_UNZIP "cd %s; %s -q %s"
	    dirname = DirectoryOf(line+17);
	    HTAddParam(&buffer, FMT_UNZIP, 1, dirname);
	    HTAddParam(&buffer, FMT_UNZIP, 2, UNZIP_PATH);
	    HTAddParam(&buffer, FMT_UNZIP, 3, line+17);
	    HTEndParam(&buffer, FMT_UNZIP, 3);
# endif /* !ARCHIVE_ONLY */
#endif /* OK_ZIP */

	} else if (!strncmp(line, "LYNXDIRED://COMPRESS", 20)) {
#define FMT_COMPRESS "%s %s"
	    HTAddParam(&buffer, FMT_COMPRESS, 1, COMPRESS_PATH);
	    HTAddParam(&buffer, FMT_COMPRESS, 2, line+20);
	    HTEndParam(&buffer, FMT_COMPRESS, 2);
	}

	if (buffer != 0) {
	    if (strlen(buffer) < 60) {
		HTSprintf0(&tmpbuf, gettext("Executing %s "), buffer);
	    } else {
		HTSprintf0(&tmpbuf,
			   gettext("Executing system command. This might take a while."));
	    }
	    _statusline(tmpbuf);
	    stop_curses();
	    printf("%s\n", tmpbuf);
	    LYSystem(buffer);
#ifdef VMS
	    HadVMSInterrupt = FALSE;
#endif /* VMS */
	    start_curses();
	    LYforce_no_cache = TRUE;
	}
    }

    FREE(dirname);
    FREE(tmpbuf);
    FREE(buffer);
    FREE(line);
    FREE(tp);
    LYpop(doc);
    return 0;
}

/*
 *  Provide a menu of file management options.
 */
PUBLIC int dired_options ARGS2(
	document *,	doc,
	char **,	newfile)
{
    static char tempfile[LY_MAXPATH];
    char path[512], dir[512]; /* much too large */
    lynx_html_item_type *nxt;
    struct stat dir_info;
    FILE *fp0;
    char *cp = NULL;
    char *dir_url;
    char *path_url;
    BOOLEAN nothing_tagged;
    int count;
    struct dired_menu *mp;
    char buf[2048];

    LYRemoveTemp(tempfile);
    if ((fp0 = LYOpenTemp(tempfile, HTML_SUFFIX, "w")) == NULL) {
	HTAlert(gettext("Unable to open file management menu file."));
	return(0);
    }

    /*
     *  Make the tempfile a URL.
     */
    LYLocalFileToURL(newfile, tempfile);
    strcpy(LYDiredFileURL, *newfile);

    cp = HTpartURL_toFile(doc->address);
    strcpy(dir, cp);
    LYTrimPathSep(dir);
    FREE(cp);

    if (doc->link > -1 && doc->link < (nlinks+1)) {
	cp = HTfullURL_toFile(links[doc->link].lname);
	strcpy(path, cp);
	LYTrimPathSep(path);
	FREE(cp);

	if (!ok_lstat(path, &dir_info)) {
	    LYCloseTempFP(fp0);
	    return 0;
	}

    } else {
	path[0] = '\0';
    }

    nothing_tagged = (HTList_isEmpty(tagged));

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
	 *  Write out number of tagged items, and names of first
	 *  few of them relative to current (in the DIRED sense)
	 *  directory.
	 */
	int n = HTList_count(tagged);
	char *cp1 = NULL;
	char *cd = NULL;
	int i, m;
#define NUM_TAGS_TO_WRITE 10
	fprintf(fp0, "<em>%s</em> %d %s",
		gettext("Current selection:"),
		n, ((n == 1) ? gettext("tagged item:") : gettext("tagged items:")));
	StrAllocCopy(cd, doc->address);
	HTUnEscapeSome(cd, "/");
	LYAddHtmlSep(&cd);
	m = (n < NUM_TAGS_TO_WRITE) ? n : NUM_TAGS_TO_WRITE;
	for (i = 1; i <= m; i++) {
	    cp1 = HTRelative(HTList_objectAt(tagged, i-1),
			     (*cd ? cd : "file://localhost"));
	    HTUnEscape(cp1);
	    LYEntify(&cp1, TRUE); /* _should_ do this everywhere... */
	    fprintf(fp0, "%s<br>\n&nbsp;&nbsp;&nbsp;%s",
			 (i == 1 ? "" : " ,"), cp1);
	    FREE(cp1);
	}
	if (n > m) {
	    fprintf(fp0," , ...");
	}
	fprintf(fp0, "<p>\n");
	FREE(cd);
    }

    /*
     *	If menu_head is NULL then use defaults and link them together now.
     */
    if (menu_head == NULL) {
	for (mp = defmenu; mp->href != NULL; mp++)
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
	if (*mp->sfx &&
	    (strlen(path) < strlen(mp->sfx) ||
	     strcmp(mp->sfx, &path[(strlen(path) - strlen(mp->sfx))]) != 0))
	    continue;
	dir_url  = HTEscape(dir, URL_PATH);
	path_url = HTEscape(path, URL_PATH);
	fprintf(fp0, "<a href=\"%s",
		render_item(mp->href, path_url, dir_url, buf,sizeof(buf), YES));
	fprintf(fp0, "\">%s</a> ",
		render_item(mp->link, path, dir, buf,sizeof(buf), NO));
	fprintf(fp0, "%s<br>\n",
		render_item(mp->rest, path, dir, buf,sizeof(buf), NO));
	FREE(dir_url);
	FREE(path_url);
    }

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

    EndInternalPage(fp0);
    LYCloseTempFP(fp0);

    LYforce_no_cache = TRUE;

    return(0);
}

/*
 *  Check DIRED filename.
 */
PRIVATE char *get_filename ARGS3(
	char *, 	prompt,
	char *, 	buf,
	size_t, 	bufsize)
{
    char *cp;

    _statusline(prompt);

    *buf = '\0';
    LYgetstr(buf, VISIBLE, bufsize, NORECALL);
    if (strstr(buf, "../") != NULL) {
	HTAlert(gettext("Illegal filename; request ignored."));
	return NULL;
    }

    if (no_dotfiles || !show_dotfiles) {
	cp = strrchr(buf, '/'); /* find last slash */
	if (cp)
	    cp += 1;
	else
	    cp = buf;
	if (*cp == '.') {
	    HTAlert(gettext("Illegal filename; request ignored."));
	    return NULL;
	}
    }
    return buf;
}

/*
 *  Install the specified file or directory.
 */
PUBLIC BOOLEAN local_install ARGS3(
	char *, 	destpath,
	char *, 	srcpath,
	char **,	newpath)
{
    char *tmpbuf = NULL;
    char savepath[512]; /* This will be the link that is to be installed. */
    struct stat dir_info;
    char *args[6];
    HTList *tag;
    int count = 0;
    int n = 0, src;	/* indices into 'args[]' */

    /*
     *	Determine the status of the selected item.
     */
    if (srcpath) {
	if (!ok_localname(savepath, srcpath))
	    return 0;

	LYforce_no_cache = TRUE;
	LYLocalFileToURL(newpath, Home_Dir());
	StrAllocCat(*newpath, "/.installdirs.html");
	return 0;
    }

    destpath = strip_trailing_slash(destpath);

    if (!ok_stat(destpath, &dir_info)) {
	return 0;
    } else if (!S_ISDIR(dir_info.st_mode)) {
	HTAlert(gettext("The selected item is not a directory!  Request ignored."));
	return 0;
    } else if (0 /*directory not writable*/) {
	HTAlert(gettext("Install in the selected directory not permitted."));
	return 0;
    }

    statusline(gettext("Just a moment, ..."));
    args[n++] = "install";
#ifdef INSTALL_ARGS
    args[n++] = INSTALL_ARGS;
#endif /* INSTALL_ARGS */
    src = n++;
    args[n++] = destpath;
    args[n] = (char *)0;
    HTSprintf(&tmpbuf, "install %s", destpath);
    tag = tagged;

    if (HTList_isEmpty(tagged)) {
	args[src] = savepath;
	if (LYExecv(INSTALL_PATH, args, tmpbuf) <= 0)
	    return (-1);
	count++;
    } else {
	char *name;
	while ((name = (char *)HTList_nextObject(tag))) {
	    int err;
	    args[src] = HTfullURL_toFile(name);
	    err = (LYExecv(INSTALL_PATH, args, tmpbuf) <= 0);
	    FREE(args[src]);
	    if (err)
		return ((count == 0) ? -1 : count);
	    count++;
	}
	clear_tags();
    }
    FREE(tmpbuf);
    HTInfoMsg(gettext("Installation complete"));
    return count;
}

/*
 *  Clear DIRED tags.
 */
PUBLIC void clear_tags NOARGS
{
    char *cp = NULL;

    while ((cp = HTList_removeLastObject(tagged)) != NULL) {
	FREE(cp);
    }
    if (HTList_isEmpty(tagged))
	FREE(tagged);
}

/*
 *  Handle DIRED menu item.
 */
PUBLIC void add_menu_item ARGS1(
	char *, 	str)
{
    struct dired_menu *new, *mp;
    char *cp;

    /*
     *	First custom menu definition causes entire default menu to be
     *	discarded.
     */
    if (menu_head == defmenu)
	menu_head = NULL;

    new = (struct dired_menu *)calloc(1, sizeof(*new));
    if (new == NULL)
	outofmem(__FILE__, "add_menu_item");

    /*
     *	Conditional on tagged != NULL ?
     */
    cp = strchr(str, ':');
    *cp++ = '\0';
    if (strcasecomp(str, "tag") == 0) {
	new->cond = DE_TAG;
    } else if (strcasecomp(str, "dir") == 0) {
	new->cond = DE_DIR;
    } else if (strcasecomp(str, "file") == 0) {
	new->cond = DE_FILE;
    } else if (strcasecomp(str, "link") == 0) {
	new->cond = DE_SYMLINK;
    }

    /*
     *	Conditional on matching suffix.
     */
    str = cp;
    cp = strchr(str, ':');
    *cp++ = '\0';
    StrAllocCopy(new->sfx, str);

    str = cp;
    cp = strchr(str, ':');
    *cp++ = '\0';
    StrAllocCopy(new->link, str);

    str = cp;
    cp = strchr(str, ':');
    *cp++ = '\0';
    StrAllocCopy(new->rest, str);

    StrAllocCopy(new->href, cp);

    if (menu_head) {
	for (mp = menu_head; mp && mp->next != NULL; mp = mp->next)
	    ;
	mp->next = new;
    } else
	menu_head = new;
}

/*
 *  Create URL for DIRED HREF value.
 */
PRIVATE char * render_item ARGS6(
	CONST char *, 	s,
	char *, 	path,
	char *, 	dir,
	char *, 	buf,
	int,		bufsize,
	BOOLEAN,	url_syntax)
{
    char *cp;
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
		    cp = strrchr(path, '/');
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
			       (name = (char *)HTList_nextObject(cur))!=NULL) {
			    if (*s == 'l' && (cp = strrchr(name, '/')))
				cp++;
			    else
				cp = name;
			    StrAllocCat(taglist, cp);
			    StrAllocCat(taglist, " "); /* should this be %20?*/
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
		    *BP_INC =*s;
		    break;
	    }
	} else {
	    /*
	     *	Other chars come from the lynx.cfg or
	     *	the default.  Let's assume there isn't
	     *	anything weird there that needs escaping.
	     */
	    *BP_INC =*s;
	}
	s++;
    }
    if (overrun & url_syntax) {
	strcpy(buf,gettext("Temporary URL or list would be too long."));
	HTAlert(buf);
	bp = buf;	/* set to start, will return empty string as URL */
    }
    *bp = '\0';
    return buf;
}
#endif /* DIRED_SUPPORT */

/*
 *  Execute DIRED command.
 */
PRIVATE int LYExecv ARGS3(
	char *, 	path,
	char **,	argv,
	char *, 	msg)
{
#if defined(VMS) || defined(_WINDOWS)
    CTRACE(tfp, "LYExecv:  Called inappropriately!\n");
    return(0);
#else
    int rc;
    char *tmpbuf = 0;
    pid_t pid;
#ifdef HAVE_TYPE_UNIONWAIT
    union wait wstatus;
#else
    int wstatus;
#endif

    if (TRACE) {
	int n;
	CTRACE(tfp, "LYExecv path='%s'\n", path);
	for (n = 0; argv[n] != 0; n++)
	    CTRACE(tfp, "argv[%d] = '%s'\n", n, argv[n]);
    }

    rc = 1;		/* It will work */
    stop_curses();
    pid = fork();	/* fork and execute rm */
    switch (pid) {
	case -1:
	    HTSprintf(&tmpbuf, gettext("Unable to %s due to system error!"), msg);
	    rc = 0;
	    break;	/* don't fall thru! - KW */
	case 0:  /* child */
#ifdef USE_EXECVP
	    execvp(path, argv);	/* this uses our $PATH */
#else
	    execv(path, argv);
#endif
	    exit(-1);	/* execv failed, give wait() something to look at */
	default:  /* parent */
#if !HAVE_WAITPID
	    while (wait(&wstatus) != pid)
		; /* do nothing */
#else
	    while (-1 == waitpid(pid, &wstatus, 0)) { /* wait for child */
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
	    if (WEXITSTATUS(wstatus) != 0 ||
		WTERMSIG(wstatus) > 0)	{ /* error return */
		HTSprintf(&tmpbuf, gettext("Probable failure to %s due to system error!"),
				   msg);
		rc = 0;
	    }
    }

    if (rc == 0) {
	/*
	 *  Screen may have message from the failed execv'd command.
	 *  Give user time to look at it before screen refresh.
	 */
	sleep(AlertSecs);
    }
    start_curses();
    if (tmpbuf != 0) {
	HTAlert(tmpbuf);
	FREE(tmpbuf);
    }

    return(rc);
#endif /* VMS */
}
