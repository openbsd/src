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

#include "HTUtils.h"
#include "tcp.h"
#include "HTAlert.h"
#include "HTParse.h"
#include "LYCurses.h"
#include "LYGlobalDefs.h"
#include "LYUtils.h"
#include "LYStrings.h"
#include "LYCharUtils.h"
#include "LYStructs.h"
#include "LYGetFile.h"
#include "LYHistory.h"
#include "LYUpload.h"
#include "LYLocal.h"
#include "LYSystem.h"

#ifndef VMS
#ifndef _WINDOWS
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <errno.h>
#include <grp.h>
#endif /*_WINDOWS */
#endif /* VMS */

#ifndef WEXITSTATUS
# if HAVE_TYPE_UNIONWAIT
#  define	WEXITSTATUS(status)	(status.w_retcode)
# else
#  define	WEXITSTATUS(status)	(((status) & 0xff00) >> 8)
# endif
#endif

#ifndef WTERMSIG
# if HAVE_TYPE_UNIONWAIT
#  define	WTERMSIG(status)	(status.w_termsig)
# else
#  define	WTERMSIG(status)	((status) & 0x7f)
# endif
#endif

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}


PRIVATE int LYExecv PARAMS((
	char *		path,
	char ** 	argv,
	char *		msg));

#ifdef DIRED_SUPPORT
PUBLIC char LYPermitFileURL[256] = "\0";
PUBLIC char LYDiredFileURL[256] = "\0";

PRIVATE char *filename PARAMS((
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
	char *		s,
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
"(of selected symbolic link)", "LYNXDIRED://MODIFY_NAME%p",		NULL },

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

/*
 *  Remove all tagged files and directories.
 */
PRIVATE BOOLEAN remove_tagged NOARGS
{
    int c, ans;
    char *cp, *tp;
    char tmpbuf[1024];
    char *testpath = NULL;
    struct stat dir_info;
    int count, i;
    HTList *tag;
    char *args[5];

    if (HTList_isEmpty(tagged))  /* should never happen */
	return 0;

    _statusline("Remove all tagged files and directories (y or n): ");
    c = LYgetch();
    ans = TOUPPER(c);

    count = 0;
    tag = tagged;
    while (ans == 'Y' && (cp = (char *)HTList_nextObject(tag)) != NULL) {
	if (is_url(cp) == FILE_URL_TYPE) { /* unecessary check */
	    tp = cp;
	    if (!strncmp(tp, "file://localhost", 16)) {
		tp += 16;
	    } else if (!strncmp(tp, "file:", 5)) {
		tp += 5;
	    }
	    StrAllocCopy(testpath, tp);
	    HTUnEscape(testpath);
	    if ((i = strlen(testpath)) && testpath[i-1] == '/')
		testpath[(i - 1)] = '\0';

	    /*
	     *	Check the current status of the path to be deleted.
	     */
	    if (stat(testpath,&dir_info) == -1) {
		sprintf(tmpbuf,
			"System error - failed to get status of '%s'.",
			testpath);
		_statusline(tmpbuf);
		sleep(AlertSecs);
		return count;
	    } else {
		args[0] = "rm";
		args[1] = "-rf";
		args[2] = testpath;
		args[3] = (char *) 0;
		sprintf(tmpbuf, "remove %s", testpath);
		if (LYExecv(RM_PATH, args, tmpbuf) <= 0) {
		    FREE(testpath);
		    return ((count == 0) ? -1 : count);
		}
		++count;
	    }
	}
    }
    FREE(testpath);
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
    char *args[5];
    int count = 0;
    HTList *tag;

    if (HTList_isEmpty(tagged))  /* should never happen */
	return 0;

    _statusline("Enter new location for tagged items: ");

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
	if (!strncmp(cp, "file://localhost", 16)) {
	    cp += 16;
	} else if (!strncmp(cp, "file:", 5)) {
	    cp += 5;
	}
	if (testpath == NULL) {
	    /*
	     *	Get the directory containing the file or subdir.
	     */
	    cp = strip_trailing_slash(cp);
	    savepath = HTParse(".", cp, PARSE_PATH+PARSE_PUNCTUATION);
	} else {
	    StrAllocCopy(savepath, cp);
	}
	HTUnEscape(savepath);
	if (stat(savepath, &dir_info) == -1) {
	    sprintf(tmpbuf, "Unable to get status of '%s'.", savepath);
	    _statusline(tmpbuf);
	    sleep(AlertSecs);
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
		sprintf(tmpbuf, "%s", "Path too long");
		_statusline(tmpbuf);
		sleep(AlertSecs);
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
	if (tmpbuf[0] != '/') {
	    if (savepath[(strlen(savepath) - 1)] != '/')
		StrAllocCat(savepath,"/");
	    StrAllocCat(savepath,tmpbuf);
	} else {
	    StrAllocCopy(savepath,tmpbuf);
	}

	/*
	 *  stat() the target location to determine type and ownership.
	 */
	if (stat(savepath, &dir_info) == -1) {
	    sprintf(tmpbuf,"Unable to get status of '%s'.",savepath);
	    _statusline(tmpbuf);
	    sleep(AlertSecs);
	    FREE(savepath);
	    return 0;
	}

	/*
	 *  Make sure the source and target locations are not the same place.
	 */
	if (dev == dir_info.st_dev && inode == dir_info.st_ino) {
	    _statusline(
	   "Source and destination are the same location - request ignored!");
	    sleep(AlertSecs);
	    FREE(savepath);
	    return 0;
	}

	/*
	 *  Make sure the target location is a directory which is owned
	 * by the same uid as the owner of the current location.
	 */
	if ((dir_info.st_mode & S_IFMT) == S_IFDIR) {
	    if (dir_info.st_uid == owner) {
		count = 0;
		tag = tagged;

		/*
		 *  Move all tagged items to the target location.
		 */
		while ((cp = (char *)HTList_nextObject(tag)) != NULL) {
		    if (!strncmp(cp, "file://localhost", 16)) {
			cp += 16;
		    } else if (!strncmp(cp, "file:", 5)) {
			cp += 5;
		    }
		    StrAllocCopy(srcpath, cp);
		    HTUnEscape(srcpath);

		    sprintf(tmpbuf, "move %s to %s", srcpath, savepath);
		    args[0] = "mv";
		    args[1] = srcpath;
		    args[2] = savepath;
		    args[3] = (char *) 0;
		    if (LYExecv(MV_PATH, args, tmpbuf) <= 0) {
			if (count == 0)
			    count = -1;
			break;
		    }
		    ++count;
		}
		FREE(srcpath);
		FREE(savepath);
		clear_tags();
		return count;
	    } else {
		_statusline(
			"Destination has different owner! Request denied.");
		sleep(AlertSecs);
		FREE(srcpath);
		FREE(savepath);
		return 0;
	    }
	} else {
	    _statusline(
		   "Destination is not a valid directory! Request denied.");
	    sleep(AlertSecs);
	    FREE(savepath);
	    return 0;
	}
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
    char *args[5];

    /*
     *	Determine the status of the selected item.
     */
    testpath = strip_trailing_slash(testpath);

    if (stat(testpath, &dir_info) == -1) {
	sprintf(tmpbuf, "Unable to get status of '%s'.", testpath);
	_statusline(tmpbuf);
	sleep(AlertSecs);
    } else {
	/*
	 *  Change the name of the file or directory.
	 */
	if ((dir_info.st_mode & S_IFMT) == S_IFDIR) {
	     cp = "Enter new name for directory: ";
	} else if ((dir_info.st_mode & S_IFMT) == S_IFREG) {
	     cp = "Enter new name for file: ";
	} else {
	     _statusline(
	 "The selected item is not a file or a directory! Request ignored.");
	     sleep(AlertSecs);
	     return 0;
	}
	if (filename(cp, tmpbuf, sizeof(tmpbuf)) == NULL)
	    return 0;

	/*
	 *  Do not allow the user to also change the location at this time.
	 */
	if (strchr(tmpbuf, '/') != NULL) {
	    _statusline("Illegal character \"/\" found! Request ignored.");
	    sleep(AlertSecs);
	} else if (strlen(tmpbuf) &&
		   (cp = strrchr(testpath, '/')) != NULL) {
	    strcpy(savepath,testpath);
	    *(++cp) = '\0';
	    strcpy(newpath,testpath);
	    strcat(newpath,tmpbuf);

	    /*
	     *	Make sure the destination does not already exist.
	     */
	    if (stat(newpath, &dir_info) == -1) {
		if (errno != ENOENT) {
		    sprintf(tmpbuf,
			    "Unable to determine status of '%s'.", newpath);
		    _statusline(tmpbuf);
		    sleep(AlertSecs);
		} else {
		    sprintf(tmpbuf, "move %s to %s", savepath, newpath);
		    args[0] = "mv";
		    args[1] = savepath;
		    args[2] = newpath;
		    args[3] = (char *) 0;
		    if (LYExecv(MV_PATH, args, tmpbuf) <= 0)
			return (-1);
		    return 1;
		}
	    } else if ((dir_info.st_mode & S_IFMT) == S_IFDIR) {
		_statusline(
	    "There is already a directory with that name! Request ignored.");
		sleep(AlertSecs);
	    } else if ((dir_info.st_mode & S_IFMT) == S_IFREG) {
		_statusline(
		 "There is already a file with that name! Request ignored.");
		sleep(AlertSecs);
	    } else {
		_statusline(
		   "The specified name is already in use! Request ignored.");
		sleep(AlertSecs);
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
    int mode;
    char *cp;
    dev_t dev;
    ino_t inode;
    uid_t owner;
    char tmpbuf[1024];
    char newpath[512];
    char savepath[512];
    struct stat dir_info;
    char *args[5];

    /*
     *	Determine the status of the selected item.
     */
    testpath = strip_trailing_slash(testpath);

    if (stat(testpath, &dir_info) == -1) {
	sprintf(tmpbuf, "Unable to get status of '%s'.", testpath);
	_statusline(tmpbuf);
	sleep(AlertSecs);
	return 0;
    }

    /*
     *	Change the location of the file or directory.
     */
    if ((dir_info.st_mode & S_IFMT) == S_IFDIR) {
	cp = "Enter new location for directory: ";
    } else if ((dir_info.st_mode & S_IFMT) == S_IFREG) {
	cp = "Enter new location for file: ";
    } else {
	_statusline(
	"The specified item is not a file or a directory - request ignored.");
	sleep(AlertSecs);
	return 0;
    }
    if (filename(cp, tmpbuf, sizeof(tmpbuf)) == NULL)
	return 0;
    if (strlen(tmpbuf)) {
	strcpy(savepath, testpath);
	strcpy(newpath, testpath);

	/*
	 *  Allow ~/ references to the home directory.
	 */
	if (!strncmp(tmpbuf,"~/",2)) {
	    strcpy(newpath, Home_Dir());
	    strcat(newpath, (tmpbuf + 1));
	    strcpy(tmpbuf, newpath);
	}
	if (tmpbuf[0] != '/') {
	    if ((cp = strrchr(newpath,'/')) != NULL) {
		*++cp = '\0';
		strcat(newpath,tmpbuf);
	    } else {
	    _statusline("Unexpected failure - unable to find trailing \"/\"");
		sleep(AlertSecs);
		return 0;
	    }
	} else {
	    strcpy(newpath,tmpbuf);
	}

	/*
	 *  Make sure the source and target have the same owner (uid).
	 */
	dev = dir_info.st_dev;
	mode = dir_info.st_mode;
	inode = dir_info.st_ino;
	owner = dir_info.st_uid;
	if (stat(newpath, &dir_info) == -1) {
	    sprintf(tmpbuf,"Unable to get status of '%s'.",newpath);
	    _statusline(tmpbuf);
	    sleep(AlertSecs);
	    return 0;
	}
	if ((dir_info.st_mode & S_IFMT) != S_IFDIR) {
	    _statusline(
		"Destination is not a valid directory! Request denied.");
	    sleep(AlertSecs);
	    return 0;
	}

	/*
	 *  Make sure the source and target are not the same location.
	 */
	if (dev == dir_info.st_dev && inode == dir_info.st_ino) {
	    _statusline(
	   "Source and destination are the same location! Request ignored!");
	    sleep(AlertSecs);
	    return 0;
	}
	if (dir_info.st_uid == owner) {
	    sprintf(tmpbuf,"move %s to %s",savepath,newpath);
	    args[0] = "mv";
	    args[1] = savepath;
	    args[2] = newpath;
	    args[3] = (char *) 0;
	    if (LYExecv(MV_PATH, args, tmpbuf) <= 0)
		return (-1);
	    return 1;
	} else {
	 _statusline("Destination has different owner! Request denied.");
	    sleep(AlertSecs);
	    return 0;
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
	cp = doc->address;
	if (!strncmp(cp, "file://localhost", 16)) {
	    cp += 16;
	} else if (!strncmp(cp, "file:", 5)) {
	    cp += 5;
	}
	strcpy(testpath, cp);
	HTUnEscapeSome(testpath, "/");
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
    _statusline("Modify name, location, or permission (n, l, or p): ");
#else
    _statusline("Modify name, or location (n or l): ");
#endif /* OK_PERMIT */
    c = LYgetch();
    ans = TOUPPER(c);

    if (strchr("NLP", ans) != NULL) {
	cp = links[doc->link].lname;
	if (!strncmp(cp, "file://localhost", 16)) {
	    cp += 16;
	} else if(!strncmp(cp, "file:", 5)) {
	    cp += 5;
	}
	strcpy(testpath, cp);
	HTUnEscape(testpath);

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
	     _statusline("This feature not yet implemented!");
	    sleep(AlertSecs);
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
    char tmpbuf[512];
    char testpath[512];
    struct stat dir_info;
    char *args[5];
    char *bad_chars = ".~/";

    if (filename("Enter name of file to create: ",
		 tmpbuf, sizeof(tmpbuf)) == NULL) {
	return 0;
    }

    if (!no_dotfiles && show_dotfiles) {
	bad_chars = "~/";
    }

    if (strstr(tmpbuf, "//") != NULL) {
	_statusline("Illegal redirection \"//\" found! Request ignored.");
	sleep(AlertSecs);
    } else if (strlen(tmpbuf) && strchr(bad_chars, tmpbuf[0]) == NULL) {
	strcpy(testpath,current_location);
	if (testpath[(strlen(testpath) - 1)] != '/') {
	    strcat(testpath,"/");
	}

	/*
	 *  Append the target filename to the current location.
	 */
	strcat(testpath, tmpbuf);

	/*
	 *  Make sure the target does not already exist
	 */
	if (stat(testpath, &dir_info) == -1) {
	    if (errno != ENOENT) {
		sprintf(tmpbuf,
			"Unable to determine status of '%s'.", testpath);
		_statusline(tmpbuf);
		sleep(AlertSecs);
		return 0;
	    }
	    sprintf(tmpbuf,"create %s",testpath);
	    args[0] = "touch";
	    args[1] = testpath;
	    args[2] = (char *) 0;
	    if (LYExecv(TOUCH_PATH, args, tmpbuf) <= 0)
		return (-1);
	    return 1;
	} else if ((dir_info.st_mode & S_IFMT) == S_IFDIR) {
	    _statusline(
	   "There is already a directory with that name! Request ignored.");
	    sleep(AlertSecs);
	} else if ((dir_info.st_mode & S_IFMT) == S_IFREG) {
	    _statusline(
		"There is already a file with that name! Request ignored.");
	    sleep(AlertSecs);
	} else {
	    _statusline(
		  "The specified name is already in use! Request ignored.");
	    sleep(AlertSecs);
	}
    }
    return 0;
}

/*
 *  Create a new directory in the current directory.
 */
PRIVATE BOOLEAN create_directory ARGS1(
	char *, 	current_location)
{
    char tmpbuf[512];
    char testpath[512];
    struct stat dir_info;
    char *args[5];
    char *bad_chars = ".~/";

    if (filename("Enter name for new directory: ",
		 tmpbuf, sizeof(tmpbuf)) == NULL) {
	return 0;
    }

    if (!no_dotfiles && show_dotfiles) {
	bad_chars = "~/";
    }

    if (strstr(tmpbuf, "//") != NULL) {
	_statusline("Illegal redirection \"//\" found! Request ignored.");
	sleep(AlertSecs);
    } else if (strlen(tmpbuf) && strchr(bad_chars, tmpbuf[0]) == NULL) {
	strcpy(testpath,current_location);
	if (testpath[(strlen(testpath) - 1)] != '/') {
	    strcat(testpath,"/");
	}
	strcat(testpath, tmpbuf);

	/*
	 *  Make sure the target does not already exist.
	 */
	if (stat(testpath, &dir_info) == -1) {
	    if (errno != ENOENT) {
		sprintf(tmpbuf,
			"Unable to determine status of '%s'.", testpath);
		_statusline(tmpbuf);
		sleep(AlertSecs);
		return 0;
	    }
	    sprintf(tmpbuf,"make directory %s",testpath);
	    args[0] = "mkdir";
	    args[1] = testpath;
	    args[2] = (char *) 0;
	    if (LYExecv(MKDIR_PATH, args, tmpbuf) <= 0)
		return (-1);
	    return 1;
	} else if ((dir_info.st_mode & S_IFMT) == S_IFDIR) {
	    _statusline(
	   "There is already a directory with that name! Request ignored.");
	    sleep(AlertSecs);
	} else if ((dir_info.st_mode & S_IFMT) == S_IFREG) {
	    _statusline(
		"There is already a file with that name! Request ignored.");
	    sleep(AlertSecs);
	} else {
	    _statusline(
		  "The specified name is already in use! Request ignored.");
	    sleep(AlertSecs);
	}
    }
    return 0;
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

    _statusline("Create file or directory (f or d): ");
    c = LYgetch();
    ans = TOUPPER(c);

    cp = doc->address;
    if (!strncmp(cp, "file://localhost", 16)) {
	cp += 16;
    } else if (!strncmp(cp, "file:", 5)) {
	cp += 5;
    }
    strcpy(testpath,cp);
    HTUnEscape(testpath);

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
    int c;
    char *cp;
    char tmpbuf[1024];
    struct stat dir_info;
    char *args[5];

    /*
     *	lstat() first in case its a symbolic link.
     */
    if (lstat(testpath, &dir_info) == -1 &&
	stat(testpath, &dir_info) == -1) {
	sprintf(tmpbuf,
		"System error - failed to get status of '%s'.", testpath);
	_statusline(tmpbuf);
	sleep(AlertSecs);
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
    if ((dir_info.st_mode & S_IFMT) == S_IFDIR) {
	if (strlen(cp) < 37) {
	    sprintf(tmpbuf,
		    "Remove '%s' and all of its contents (y or n): ", cp);
	} else {
	    sprintf(tmpbuf,
		    "Remove directory and all of its contents (y or n): ");
	}
    } else if ((dir_info.st_mode & S_IFMT) == S_IFREG) {
	if (strlen(cp) < 60) {
	    sprintf(tmpbuf, "Remove file '%s' (y or n): ", cp);
	} else {
	    sprintf(tmpbuf, "Remove file (y or n): ");
	}
#ifdef S_IFLNK
    } else if ((dir_info.st_mode & S_IFMT) == S_IFLNK) {
	if (strlen(cp) < 50) {
	    sprintf(tmpbuf, "Remove symbolic link '%s' (y or n): ", cp);
	} else {
	    sprintf(tmpbuf, "Remove symbolic link (y or n): ");
	}
#endif
    } else {
	sprintf(tmpbuf, "Unable to determine status of '%s'.", testpath);
	_statusline(tmpbuf);
	sleep(AlertSecs);
	return 0;
    }
    _statusline(tmpbuf);

    c = LYgetch();
    if (TOUPPER(c) == 'Y') {
	sprintf(tmpbuf,"remove %s",testpath);
	args[0] = "rm";
	args[1] = "-rf";
	args[2] = testpath;
	args[3] = (char *) 0;
	if (LYExecv(RM_PATH, args, tmpbuf) <= 0)
	    return (-1);
	return 1;
    }
    return 0;
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
	tp = cp;
	if (!strncmp(tp, "file://localhost", 16)) {
	    tp += 16;
	} else if (!strncmp(tp, "file:", 5)) {
	    tp += 5;
	}
	strcpy(testpath, tp);
	HTUnEscape(testpath);
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
    char *string_mode;	/* Key for  value below */
    long permit_bits;	/* Value for chmod/whatever */
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

#ifndef S_ISDIR
#define S_ISDIR(mode)   ((mode&0xF000) == 0x4000)
#endif /* !S_ISDIR */

PRIVATE char LYValidPermitFile[256] = "\0";

/*
 *  Handle DIRED permissions.
 */
PRIVATE BOOLEAN permit_location ARGS3(
	char *, 	destpath,
	char *, 	srcpath,
	char **,	newpath)
{
#ifndef UNIX
    _statusline("Sorry, don't know how to permit non-UNIX files yet.");
    sleep(AlertSecs);
    return(0);
#else
    static char tempfile[256] = "\0";
    static BOOLEAN first = TRUE;
    char *cp;
    char tmpbuf[LINESIZE];
    struct stat dir_info;

    if (srcpath) {
	/*
	 *  Create form.
	 */
	FILE *fp0;
	char * user_filename;
	struct group * grp;
	char * group_name;

	/*
	 *  A couple of sanity tests.
	 */
	srcpath = strip_trailing_slash(srcpath);
	if (strncmp(srcpath, "file://localhost", 16) == 0)
	    srcpath += 16;
	if (lstat(srcpath, &dir_info) == -1) {
	    sprintf(tmpbuf, "Unable to get status of '%s'.", srcpath);
	    _statusline(tmpbuf);
	    sleep(AlertSecs);
	    return 0;
	} else if ((dir_info.st_mode & S_IFMT) != S_IFDIR &&
	    (dir_info.st_mode & S_IFMT) != S_IFREG) {
	    _statusline(
	"The specified item is not a file nor a directory - request ignored.");
	    sleep(AlertSecs);
	    return(0);
	}

	user_filename = srcpath;
	cp = strrchr(srcpath, '/');
	if (cp != NULL) {
	    user_filename = (cp + 1);
	}

	if (first) {
	    /*
	     *	Get an unused tempfile name. - FM
	     */
	    tempname(tempfile, NEW_FILE);
	}

	/*
	 *  Open the tempfile for writing and set its
	 *  protection in case this wasn't done via an
	 *  external umask. - FM
	 */
	if ((fp0 = LYNewTxtFile(tempfile)) == NULL) {
	    _statusline("Unable to open permit options file");
	    sleep(AlertSecs);
	    return(0);
	}

	if (first) {
	    /*
	     *	Make the tempfile a URL.
	     */
	    strcpy(LYPermitFileURL, "file://localhost");
	    strcat(LYPermitFileURL, tempfile);
	    first = FALSE;
	}
	StrAllocCopy(*newpath, LYPermitFileURL);

	grp = getgrgid(dir_info.st_gid);
	if (grp == NULL) {
	    group_name = "";
	} else {
	    group_name = grp->gr_name;
	}

	LYstrncpy(LYValidPermitFile,
		  srcpath,
		  (sizeof(LYValidPermitFile) - 1));

	fprintf(fp0, "<Html><Head>\n<Title>%s</Title>\n</Head>\n<Body>\n",
		PERMIT_OPTIONS_TITLE);
	fprintf(fp0,"<H1>Permissions for %s</H1>\n", user_filename);
	{   /*
	     *	Prevent filenames which include '#' or '?' from messing it up.
	     */
	    char * srcpath_url = HTEscape(srcpath, URL_PATH);
	    fprintf(fp0, "<Form Action=\"LYNXDIRED://PERMIT_LOCATION%s\">\n",
		    srcpath_url);
	    FREE(srcpath_url);
	}

	fprintf(fp0, "<Ol><Li>Specify permissions below:<Br><Br>\n");
	fprintf(fp0, "Owner:<Br>\n");
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

	fprintf(fp0, "Group %s:<Br>\n", group_name);
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

	fprintf(fp0, "Others:<Br>\n");
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
"<Br>\n<Li><Input Type=\"submit\" Value=\"Submit\"> \
form to permit %s %s.\n</Ol>\n</Form>\n",
		(dir_info.st_mode & S_IFMT) == S_IFDIR ? "directory" : "file",
		user_filename);
	fprintf(fp0, "</Body></Html>");
	fclose(fp0);

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
	    if (TRACE)
		fprintf(stderr, "permit_location: called for <%s>.\n",
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

	HTUnEscape(destpath);	/* Will now operate only on filename part. */

	/*
	 *  Make sure that the file string is the one from
	 *  the last displayed File Permissions menu. - KW
	 */
	if (strcmp(destpath, LYValidPermitFile)) {
	    if (LYCursesON)
		HTAlert(INVALID_PERMIT_URL);
	    else
		fprintf(stderr, "%s\n", INVALID_PERMIT_URL);
	    if (TRACE)
		fprintf(stderr, "permit_location: called for file '%s'.\n",
			destpath);
	    return 0;
	}

	/*
	 *  A couple of sanity tests.
	 */
	destpath = strip_trailing_slash(destpath);
	if (stat(destpath, &dir_info) == -1) {
	    sprintf(tmpbuf, "Unable to get status of '%s'.", destpath);
	    _statusline(tmpbuf);
	    sleep(AlertSecs);
	    return 0;
	} else if ((dir_info.st_mode & S_IFMT) != S_IFDIR &&
	    (dir_info.st_mode & S_IFMT) != S_IFREG) {
	    _statusline(
	"The specified item is not a file nor a directory - request ignored.");
	    sleep(AlertSecs);
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
		    _statusline("Invalid mode format.");
		    sleep(AlertSecs);
		    return 0;
		}
	    } else {
		_statusline("Invalid syntax format.");
		sleep(AlertSecs);
		return 0;
	    }

	    cp = cr;
	}

#ifdef UNIX
	/*
	 *  Call chmod().
	 */
	sprintf(tmpbuf, "chmod %.4o %s", (unsigned int)new_mode, destpath);
	sprintf(amode, "%.4o", (unsigned int)new_mode);
	args[0] = "chmod";
	args[1] = amode;
	args[2] = destpath;
	args[3] = (char *) 0;
	if (LYExecv(CHMOD_PATH, args, tmpbuf) <= 0) {
	    return (-1);
	}
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

/*
 *  Perform file management operations for LYNXDIRED URL's.
 *  Attempt to be consistent.  These are (pseudo) URLs - i.e. they should
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
    char *cp, *tp, *bp;
    char tmpbuf[256];
    char buffer[512];

    line_url = doc->address;
    if (TRACE)
	fprintf(stderr, "local_dired: called for <%s>.\n",
		(line_url ?
		 line_url : "NULL URL pointer"));
    HTUnEscapeSome(line_url, "/");	/* don't mess too much with *doc */

    StrAllocCopy(line, line_url);
    HTUnEscape(line);	/* _file_ (not URL) syntax, for those functions
			   that need it.  Don't forget to FREE it. */

    tp = NULL;
    if (!strncmp(line, "LYNXDIRED://NEW_FILE", 20)) {
	if (create_file(&line[20]) > 0)
	    LYforce_no_cache = TRUE;
    } else if (!strncmp(line, "LYNXDIRED://NEW_FOLDER", 22)) {
	if (create_directory(&line[22]) > 0)
	    LYforce_no_cache = TRUE;
    } else if (!strncmp(line, "LYNXDIRED://INSTALL_SRC", 23)) {
	local_install(NULL, &line[23], &tp);
	StrAllocCopy(doc->address, tp);
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
	if (line[(strlen(line) - 1)] == '/')
	    line[strlen(line)-1] = '\0';
	if ((cp = strrchr(line, '/')) == NULL) {
	    FREE(line);
	    return 0;
	}

	/*
	 *  Construct the appropriate system command taking care to
	 *  escape all path references to avoid spoofing the shell.
	 */
	*buffer = '\0';
	if (!strncmp(line, "LYNXDIRED://DECOMPRESS", 22)) {
	    tp = quote_pathname(line + 22);
	    sprintf(buffer,"%s %s", UNCOMPRESS_PATH, tp);
	    FREE(tp);

#if defined(OK_UUDECODE) && !defined(ARCHIVE_ONLY)
	} else if (!strncmp(line, "LYNXDIRED://UUDECODE", 20)) {
	    tp = quote_pathname(line + 20);
	    sprintf(buffer,"%s %s", UUDECODE_PATH, tp);
	    _statusline(
      "Warning! UUDecoded file will exist in the directory you started Lynx.");
	    sleep(AlertSecs);
	    FREE(tp);
#endif /* OK_UUDECODE && !ARCHIVE_ONLY */

#ifdef OK_TAR
# ifndef ARCHIVE_ONLY
#  ifdef OK_GZIP
	} else if (!strncmp(line, "LYNXDIRED://UNTAR_GZ", 20)) {
	    tp = quote_pathname(line+20);
	    *cp++ = '\0';
	    cp = quote_pathname(line + 20);
	    sprintf(buffer, "%s -qdc %s | (cd %s; %s -xf -)",
			    GZIP_PATH, tp, cp, TAR_PATH);
	    FREE(cp);
	    FREE(tp);
#  endif /* OK_GZIP */

	} else if (!strncmp(line, "LYNXDIRED://UNTAR_Z", 19)) {
	    tp = quote_pathname(line + 19);
	    *cp++ = '\0';
	    cp = quote_pathname(line + 19);
	    sprintf(buffer, "%s %s | (cd %s; %s -xf -)",
			    ZCAT_PATH, tp, cp, TAR_PATH);
	    FREE(cp);
	    FREE(tp);

	} else if (!strncmp(line, "LYNXDIRED://UNTAR", 17)) {
	    tp = quote_pathname(line + 17);
	    *cp++ = '\0';
	    cp = quote_pathname(line + 17);
	    sprintf(buffer, "cd %s; %s -xf %s", cp, TAR_PATH, tp);
	    FREE(cp);
	    FREE(tp);
# endif /* !ARCHIVE_ONLY */

# ifdef OK_GZIP
	} else if (!strncmp(line, "LYNXDIRED://TAR_GZ", 18)) {
	    *cp++ = '\0';
	    cp = quote_pathname(cp);
	    tp = quote_pathname(line + 18);
	    sprintf(buffer, "(cd %s; %s -cf - %s) | %s -qc >%s/%s.tar.gz",
			    tp, TAR_PATH, cp, GZIP_PATH, tp, cp);
	    FREE(cp);
	    FREE(tp);
# endif /* OK_GZIP */

	} else if (!strncmp(line, "LYNXDIRED://TAR_Z", 17)) {
	    *cp++ = '\0';
	    cp = quote_pathname(cp);
	    tp = quote_pathname(line + 17);
	    sprintf(buffer, "(cd %s; %s -cf - %s) | %s >%s/%s.tar.Z",
			    tp, TAR_PATH, cp, COMPRESS_PATH, tp, cp);
	    FREE(cp);
	    FREE(tp);

	} else if (!strncmp(line, "LYNXDIRED://TAR", 15)) {
	    *cp++ = '\0';
	    cp = quote_pathname(cp);
	    tp = quote_pathname(line + 15);
	    sprintf(buffer, "(cd %s; %s -cf %s.tar %s)",
			    tp, TAR_PATH, cp, cp);
	    FREE(cp);
	    FREE(tp);
#endif /* OK_TAR */

#ifdef OK_GZIP
	} else if (!strncmp(line, "LYNXDIRED://GZIP", 16)) {
	    tp = quote_pathname(line + 16);
	    sprintf(buffer, "%s -q %s", GZIP_PATH, tp);
	    FREE(tp);
#ifndef ARCHIVE_ONLY
	} else if (!strncmp(line, "LYNXDIRED://UNGZIP", 18)) {
	    tp = quote_pathname(line + 18);
	    sprintf(buffer, "%s -d %s", GZIP_PATH, tp);
	    FREE(tp);
#endif /* !ARCHIVE_ONLY */
#endif /* OK_GZIP */

#ifdef OK_ZIP
	} else if (!strncmp(line, "LYNXDIRED://ZIP", 15)) {
	    tp = quote_pathname(line + 15);
	    *cp++ = '\0';
	    bp = quote_pathname(cp);
	    cp = quote_pathname(line + 15);
	    sprintf(buffer, "cd %s; %s -rq %s.zip %s", cp, ZIP_PATH, tp, bp);
	    FREE(cp);
	    FREE(bp);
	    FREE(tp);
#ifndef ARCHIVE_ONLY
	} else if (!strncmp(line, "LYNXDIRED://UNZIP", 17)) {
	    tp = quote_pathname(line + 17);
	    *cp = '\0';
	    cp = quote_pathname(line + 17);
	    sprintf(buffer, "cd %s; %s -q %s", cp, UNZIP_PATH, tp);
	    FREE(cp);
	    FREE(tp);
# endif /* !ARCHIVE_ONLY */
#endif /* OK_ZIP */

	} else if (!strncmp(line, "LYNXDIRED://COMPRESS", 20)) {
	    tp = quote_pathname(line + 20);
	    sprintf(buffer, "%s %s", COMPRESS_PATH, tp);
	    FREE(tp);
	}

	if (strlen(buffer)) {
	    if (strlen(buffer) < 60) {
		sprintf(tmpbuf, "Executing %s ", buffer);
	    } else {
		sprintf(tmpbuf,
			"Executing system command. This might take a while.");
	    }
	    _statusline(tmpbuf);
	    stop_curses();
	    printf("%s\n", tmpbuf);
	    fflush(stdout);
	    system(buffer);
#ifdef VMS
	    extern BOOLEAN HadVMSInterrupt
	    HadVMSInterrupt = FALSE;
#endif /* VMS */
	    start_curses();
	    LYforce_no_cache = TRUE;
	}
    }

    FREE(line);
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
    static char tempfile[256];
    static BOOLEAN first = TRUE;
    char path[512], dir[512]; /* much too large */
    char tmpbuf[LINESIZE];
    lynx_html_item_type *nxt;
    struct stat dir_info;
    FILE *fp0;
    char *cp = NULL;
    char *dir_url = NULL;	/* Will hold URL-escaped path of
				   directory from where DIRED_MENU was
				   invoked (NOT its full URL). */
    char *path_url = NULL;	/* Will hold URL-escaped path of file
				   (or directory) which was selected
				   when DIRED_MENU was invoked (NOT
				   its full URL). */
    BOOLEAN nothing_tagged;
    int count;
    struct dired_menu *mp;
    char buf[2048];


    if (first) {
	/*
	 *  Get an unused tempfile name. - FM
	 */
	tempname(tempfile, NEW_FILE);
    }

    /*
     *	Open the tempfile for writing and set its
     *	protection in case this wasn't done via an
     *	external umask. - FM
     */
    if ((fp0 = LYNewTxtFile(tempfile)) == NULL) {
	_statusline("Unable to open file management menu file.");
	sleep(AlertSecs);
	return(0);
    }

    if (first) {
	/*
	 *  Make the tempfile a URL.
	 */
	strcpy(LYDiredFileURL, "file://localhost");
	strcat(LYDiredFileURL, tempfile);
	first = FALSE;
    }
    StrAllocCopy(*newfile, LYDiredFileURL);

    cp = doc->address;
    if (!strncmp(cp, "file://localhost", 16)) {
	cp += 16;
    } else if (!strncmp(cp, "file:", 5)) {
	cp += 5;
    }
    strcpy(dir, cp);
    StrAllocCopy(dir_url, cp);
    if (dir_url[(strlen(dir_url) - 1)] == '/')
	dir_url[(strlen(dir_url) - 1)] = '\0';
    HTUnEscape(dir);
    if (dir[(strlen(dir) - 1)] == '/')
	dir[(strlen(dir) - 1)] = '\0';

    if (doc->link > -1 && doc->link < (nlinks+1)) {
	cp = links[doc->link].lname;
	if (!strncmp(cp, "file://localhost", 16)) {
	    cp += 16;
	} else if (!strncmp(cp, "file:", 5)) {
	    cp += 5;
	}
	strcpy(path, cp);
	StrAllocCopy(path_url, cp);
	if (*path_url && path_url[1] && path_url[(strlen(path_url) - 1)] == '/')
	    path_url[(strlen(path_url) - 1)] = '\0';
	HTUnEscape(path);
	if (*path && path[1] && path[(strlen(path) - 1)] == '/')
	    path[(strlen(path) - 1)] = '\0';

	if (lstat(path, &dir_info) == -1 && stat(path, &dir_info) == -1) {
	    sprintf(tmpbuf, "Unable to get status of '%s'.", path);
	    _statusline(tmpbuf);
	    sleep(AlertSecs);
	    FREE(dir_url);
	    FREE(path_url);
	    return 0;
	}

    } else {
	path[0] = '\0';
	StrAllocCopy(path_url, path);
    }

    nothing_tagged = (HTList_isEmpty(tagged));

    fprintf(fp0,
	    "<head>\n<title>%s</title></head>\n<body>\n", DIRED_MENU_TITLE);

    fprintf(fp0,
	    "\n<h1>File Management Options (%s Version %s)</h1>",
	    LYNX_NAME, LYNX_VERSION);

    fprintf(fp0, "Current directory is %s<br>\n", dir);

    if (nothing_tagged) {
	if (strlen(path)) {
	    fprintf(fp0, "Current selection is %s<p>\n", path);
	} else {
	    fprintf(fp0, "Nothing currently selected.<p>\n");
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
	fprintf(fp0, "Current selection is %d tagged item%s",
		     n, ((n == 1) ? ":" : "s:"));
	StrAllocCopy(cd, doc->address);
	HTUnEscapeSome(cd, "/");
	if (*cd && cd[(strlen(cd) - 1)] != '/')
	    StrAllocCat(cd, "/");
	m = (n < NUM_TAGS_TO_WRITE) ? n : NUM_TAGS_TO_WRITE;
	for (i = 1; i <= m; i++) {
	    cp1 = HTRelative(HTList_objectAt(tagged, i-1),
			     (*cd ? cd : "file://localhost"));
	    HTUnEscape(cp1);
	    LYEntify(&cp1, TRUE); /* _should_ do this everywhere... */
	    fprintf(fp0, "%s <br>\n &nbsp;&nbsp;&nbsp;%s",
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
	    (!*path || (dir_info.st_mode & S_IFMT) != S_IFDIR))
	    continue;
	if (mp->cond == DE_FILE &&
	    (!*path || (dir_info.st_mode & S_IFMT) != S_IFREG))
	    continue;
#ifdef S_IFLNK
	if (mp->cond == DE_SYMLINK &&
	    (!*path || (dir_info.st_mode & S_IFMT) != S_IFLNK))
	    continue;
#endif
	if (*mp->sfx &&
	    (strlen(path) < strlen(mp->sfx) ||
	     strcmp(mp->sfx, &path[(strlen(path) - strlen(mp->sfx))]) != 0))
	    continue;
	fprintf(fp0, "<a href=\"%s",
		render_item(mp->href, path_url, dir_url, buf,2048, YES));
	fprintf(fp0, "\">%s</a> ",
		render_item(mp->link, path, dir, buf,2048, NO));
	fprintf(fp0, "%s<br>\n",
		render_item(mp->rest, path, dir, buf,2048, NO));
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

    fprintf(fp0, "</body>\n");
    fclose(fp0);

    FREE(dir_url);
    FREE(path_url);

    LYforce_no_cache = TRUE;

    return(0);
}

/*
 *  Check DIRED filename.
 */
PRIVATE char *filename ARGS3(
	char *, 	prompt,
	char *, 	buf,
	size_t,		bufsize)
{
    char *cp;

    _statusline(prompt);

    *buf = '\0';
    LYgetstr(buf, VISIBLE, bufsize, NORECALL);
    if (strstr(buf, "../") != NULL) {
	_statusline("Illegal filename; request ignored.");
	sleep(AlertSecs);
	return NULL;
    }

    if (no_dotfiles || !show_dotfiles) {
	cp = strrchr(buf, '/'); /* find last slash */
	if (cp)
	    cp += 1;
	else
	    cp = buf;
	if (*cp == '.') {
	    _statusline("Illegal filename; request ignored.");
	    sleep(AlertSecs);
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
    char tmpbuf[512];
    static char savepath[512]; /* This will be the link that
				  is to be installed. */
    struct stat dir_info;
    char *args[6];
    HTList *tag;
    int count = 0;
    int n = 0, src;	/* indices into 'args[]' */

    /*
     *	Determine the status of the selected item.
     */
    if (srcpath) {
	srcpath = strip_trailing_slash(srcpath);
	if (strncmp(srcpath, "file://localhost", 16) == 0)
	    srcpath += 16;
	if (stat(srcpath, &dir_info) == -1) {
	    sprintf(tmpbuf, "Unable to get status of '%s'.", srcpath);
	    _statusline(tmpbuf);
	    sleep(AlertSecs);
	    return 0;
	} else if ((dir_info.st_mode & S_IFMT) != S_IFDIR &&
		   (dir_info.st_mode & S_IFMT) != S_IFREG) {
	    _statusline(
	  "The selected item is not a file or a directory! Request ignored.");
	    sleep(AlertSecs);
	    return 0;
	}
	strcpy(savepath, srcpath);
	LYforce_no_cache = TRUE;
	strcpy(tmpbuf, "file://localhost");
	strcat(tmpbuf, Home_Dir());
	strcat(tmpbuf, "/.installdirs.html");
	StrAllocCopy(*newpath, tmpbuf);
	return 0;
    }

    destpath = strip_trailing_slash(destpath);

    if (stat(destpath,&dir_info) == -1) {
	sprintf(tmpbuf,"Unable to get status of '%s'.",destpath);
	_statusline(tmpbuf);
	sleep(AlertSecs);
	return 0;
    } else if ((dir_info.st_mode & S_IFMT) != S_IFDIR) {
	_statusline(
		"The selected item is not a directory! Request ignored.");
	sleep(AlertSecs);
	return 0;
    } else if (0 /*directory not writeable*/) {
	_statusline("Install in the selected directory not permitted.");
	sleep(AlertSecs);
	return 0;
    }

    statusline("Just a moment, ...");
    args[n++] = "install";
#ifdef INSTALL_ARGS
    args[n++] = INSTALL_ARGS;
#endif /* INSTALL_ARGS */
    src = n++;
    args[n++] = destpath;
    args[n] = (char *)0;
    sprintf(tmpbuf, "install %s", destpath);
    tag = tagged;

    if (HTList_isEmpty(tagged)) {
	args[src] = savepath;
	if (LYExecv(INSTALL_PATH, args, tmpbuf) <= 0)
	    return (-1);
	count++;
    } else {
	char *name;
	while ((name = (char *)HTList_nextObject(tag))) {
	    args[src] = name;
	    if (strncmp("file://localhost", args[src], 16) == 0)
		 args[src] = (name + 16);

	    if (LYExecv(INSTALL_PATH, args, tmpbuf) <= 0)
		return ((count == 0) ? -1 : count);
	    count++;
	}
	clear_tags();
    }
    statusline("Installation complete");
    sleep(InfoSecs);
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
	char *, 	s,
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
#ifdef NOTDEFINED
		    /*
		     *	These chars come from lynx.cfg or the default, let's
		     *	just assume there won't be any improper %'s there that
		     *	would need escaping.
		     */
		    if(url_syntax) {
			*BP_INC = '2';
			*BP_INC = '5';
		    }
#endif /* NOTDEFINED */
		    break;
		case 'p':
		    cp = path;
		    while (*cp)
			*BP_INC = *cp++;
		    break;
		case 'd':
		    cp = dir;
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
#ifdef NOTDEFINED
		    if (url_syntax) {
			*BP_INC = '2';
			*BP_INC = '5';
		    }
#endif /* NOTDEFINED */
		    *BP_INC =*s;
		    break;
	    }
	} else {
	    /*
	     *	Other chars come from the lynx.cfg or
	     *	the default. Let's assume there isn't
	     *	anything weird there that needs escaping.
	     */
	    *BP_INC =*s;
	}
	s++;
    }
    if (overrun & url_syntax) {
	sprintf(buf,"Temporary URL or list would be too long.");
	_statusline(buf);
	sleep(AlertSecs);
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
    if (TRACE) {
	fprintf(stderr, "LYExecv:  Called inappropriately!\n");
    }
    return(0);
#else
    int rc;
    char tmpbuf[512];
    pid_t pid;
#if HAVE_TYPE_UNIONWAIT
    union wait wstatus;
#else
    int wstatus;
#endif

    rc = 1;		/* It will work */
    tmpbuf[0] = '\0';	/* empty buffer for alert messages */
    stop_curses();
    pid = fork();	/* fork and execute rm */
    switch (pid) {
	case -1:
	    sprintf(tmpbuf, "Unable to %s due to system error!", msg);
	    rc = 0;
	    break;	/* don't fall thru! - KW */
	case 0:  /* child */
	    execv(path, argv);
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
		sprintf(tmpbuf, "Probable failure to %s due to system error!",
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
    if (tmpbuf[0]) {
	_statusline(tmpbuf);
	sleep(AlertSecs);
    }

    return(rc);
#endif /* VMS */
}
