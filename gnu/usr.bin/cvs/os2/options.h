/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * This file holds (most of) the configuration tweaks that can be made to
 * customize CVS for your site.  CVS comes configured for a typical SunOS 4.x
 * environment.  The comments for each configurable item are intended to be
 * self-explanatory.  All #defines are tested first to see if an over-riding
 * option was specified on the "make" command line.
 * 
 * If special libraries are needed, you will have to edit the Makefile.in file
 * or the configure script directly.  Sorry.
 */

/*
 * For portability and heterogeneity reasons, CVS is shipped by default using
 * my own text-file version of the ndbm database library in the src/myndbm.c
 * file.  If you want better performance and are not concerned about
 * heterogeneous hosts accessing your modules file, turn this option off.
 */
#ifndef MY_NDBM
#define	MY_NDBM
#endif

/*
 * The "patch" program to run when using the CVS server and accepting
 * patches across the network.  Specify a full pathname if your site
 * wants to use a particular patch.
 *
 * We call this "cvspatch" because of reports of a native OS/2 "patch"
 * program that does not behave the way CVS expects.  So OS/2 users
 * should get a GNU patch and call it "cvspatch.exe".
 */
#ifndef PATCH_PROGRAM
#define PATCH_PROGRAM	"cvspatch"
#endif

/* Directory used for storing temporary files, if not overridden by
   environment variables or the -T global option.  There should be little
   need to change this (-T is a better mechanism if you need to use a
   different directory for temporary files).  */
#ifndef TMPDIR_DFLT
#define	TMPDIR_DFLT	"c:\\temp"
#endif

/*
 * The default editor to use, if one does not specify the "-e" option to cvs,
 * or does not have an EDITOR environment variable.  I set this to just "vi",
 * and use the shell to find where "vi" actually is.  This allows sites with
 * /usr/bin/vi or /usr/ucb/vi to work equally well (assuming that your PATH
 * is reasonable).
 *
 * The notepad program seems to be Windows NT's bare-bones text editor.
 */
#ifndef EDITOR_DFLT
#define	EDITOR_DFLT	"notepad"
#endif

/*
 * The default umask to use when creating or otherwise setting file or
 * directory permissions in the repository.  Must be a value in the
 * range of 0 through 0777.  For example, a value of 002 allows group
 * rwx access and world rx access; a value of 007 allows group rwx
 * access but no world access.  This value is overridden by the value
 * of the CVSUMASK environment variable, which is interpreted as an
 * octal number.
 */
#ifndef UMASK_DFLT
#define	UMASK_DFLT	002
#endif

/*
 * The cvs admin command is restricted to the members of the group
 * CVS_ADMIN_GROUP.  If this group does not exist, all users are
 * allowed to run cvs admin.  To disable the cvs admin for all users,
 * create an empty group CVS_ADMIN_GROUP.  To disable access control for
 * cvs admin, comment out the define below.
 *
 * Under Windows NT and OS/2, this must not be used because it tries
 * to include <grp.h>.
 */
#ifdef CVS_ADMIN_GROUP
/* #define CVS_ADMIN_GROUP "cvsadmin" */
#endif

/*
 * The Repository file holds the path to the directory within the
 * source repository that contains the RCS ,v files for each CVS
 * working directory.  This path is either a full-path or a path
 * relative to CVSROOT.
 * 
 * The big advantage that I can see to having a relative path is that
 * one can change the physical location of the master source
 * repository, change the contents of CVS/Root files in your
 * checked-out code, and CVS will work without problems.
 *
 * Therefore, RELATIVE_REPOS is now the default.  In the future, this
 * is likely to disappear entirely as a compile-time (or other) option,
 * so if you have other software which relies on absolute pathnames,
 * update them.
 */
#define RELATIVE_REPOS 1

/*
 * When committing or importing files, you must enter a log message.
 * Normally, you can do this either via the -m flag on the command line or an
 * editor will be started for you.  If you like to use logging templates (the
 * rcsinfo file within the $CVSROOT/CVSROOT directory), you might want to
 * force people to use the editor even if they specify a message with -m.
 * Enabling FORCE_USE_EDITOR will cause the -m message to be appended to the
 * temp file when the editor is started.
 */
#ifndef FORCE_USE_EDITOR
/* #define 	FORCE_USE_EDITOR */
#endif

/*
 * When locking the repository, some sites like to remove locks and assume
 * the program that created them went away if the lock has existed for a long
 * time.  This used to be the default for previous versions of CVS.  CVS now
 * attempts to be much more robust, so lock files should not be left around
 * by mistake. The new behaviour will never remove old locks (they must now
 * be removed by hand).  Enabling CVS_FUDGELOCKS will cause CVS to remove
 * locks that are older than CVSLCKAGE seconds.
 * Use of this option is NOT recommended.
 */
#ifndef CVS_FUDGELOCKS
/* #define CVS_FUDGELOCKS */
#endif

/*
 * When committing a permanent change, CVS and RCS make a log entry of
 * who committed the change.  If you are committing the change logged in
 * as "root" (not under "su" or other root-priv giving program), CVS/RCS
 * cannot determine who is actually making the change.
 *
 * As such, by default, CVS disallows changes to be committed by users
 * logged in as "root".  You can disable this option by commenting
 * out the lines below.
 *
 * Under Windows NT, privileges are associated with groups, not users,
 * so the case in which someone has logged in as root does not occur.
 * Thus, there is no need for this hack.
 *
 * todo: I don't know about OS/2 yet.  -kff
 */
#undef CVS_BADROOT

/*
 * define this to enable the SETXID support (see FAQ 4D.13)
 * [ We have no such thing under OS/2, so far as I know. ]
 */
#undef SETXID_SUPPORT

/*
 * Under OS/2, we build the authenticated client by default.
 */
#define AUTH_CLIENT_SUPPORT 1

/* End of CVS configuration section */

/*
 * Externs that are included in libc, but are used frequently enough to
 * warrant defining here.
 */
#ifndef STDC_HEADERS
extern void exit ();
#endif

#ifdef AUTH_CLIENT_SUPPORT
char *getpass (char *passbuf);
#endif /* AUTH_CLIENT_SUPPORT */
