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
 * CVS provides the most features when used in conjunction with the Version-5
 * release of RCS.  Thus, it is the default.  This also assumes that GNU diff
 * Version-1.15 is being used as well -- you will have to configure your RCS
 * V5 release separately to make this the case. If you do not have RCS V5 and
 * GNU diff V1.15, comment out this define. You should not try mixing and
 * matching other combinations of these tools.
 */
#ifndef HAVE_RCS5
#define	HAVE_RCS5
#endif

/*
 * If, before installing this version of CVS, you were running RCS V4 AND you
 * are installing this CVS and RCS V5 and GNU diff 1.15 all at the same time,
 * you should turn on the following define.  It only exists to try to do
 * reasonable things with your existing checked out files when you upgrade to
 * RCS V5, since the keyword expansion formats have changed with RCS V5.
 * 
 * If you already have been running with RCS5, or haven't been running with CVS
 * yet at all, or are sticking with RCS V4 for now, leave the commented out.
 */
#ifndef HAD_RCS4
/* #define	HAD_RCS4 */
#endif

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
 * The "diff" program to execute when creating patch output.  This "diff"
 * must support the "-c" option for context diffing.  Specify a full
 * pathname if your site wants to use a particular diff.  If you are
 * using the GNU version of diff (version 1.15 or later), this should
 * be "diff -a".
 * 
 * NOTE: this program is only used for the ``patch'' sub-command (and
 * for ``update'' if you are using the server).  The other commands
 * use rcsdiff which will use whatever version of diff was specified
 * when rcsdiff was built on your system.
 */

#ifndef DIFF
#define	DIFF	"@gdiff_path@"
#endif

/*
 * The "grep" program to execute when checking to see if a merged file had
 * any conflicts.  This "grep" must support the "-s" option and a standard
 * regular expression as an argument.  Specify a full pathname if your site
 * wants to use a particular grep.
 */

#ifndef GREP
#define GREP "@ggrep_path@"
#endif

/*
 * The "patch" program to run when using the CVS server and accepting
 * patches across the network.  Specify a full pathname if your site
 * wants to use a particular patch.
 */
#ifndef PATCH_PROGRAM
#define PATCH_PROGRAM	"patch"
#endif

/*
 * By default, RCS programs are executed with the shell or through execlp(),
 * so the user's PATH environment variable is searched.  If you'd like to
 * bind all RCS programs to a certain directory (perhaps one not in most
 * people's PATH) then set the default in RCSBIN_DFLT.  Note that setting
 * this here will cause all RCS programs to be executed from this directory,
 * unless the user overrides the default with the RCSBIN environment variable
 * or the "-b" option to CVS.
 * 
 * This define should be either the empty string ("") or a full pathname to the
 * directory containing all the installed programs from the RCS distribution.
 */
#ifndef RCSBIN_DFLT
#define	RCSBIN_DFLT	""
#endif

/*
 * The password-authenticating server creates a temporary checkout of
 * the affected files.  The variable TMPDIR_DFLT (or even better, the
 * command-line option "-T" in the line for CVS in /etc/inetd.conf)
 * can be used to specify the used directory.  This directory will
 * also be used for other temporary files.
 *
 * I have no idea what the right default for this is on the Mac.
 */
#ifndef TMPDIR_DFLT
#define	TMPDIR_DFLT	"/tmp"
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
 * The Repository file holds the path to the directory within the source
 * repository that contains the RCS ,v files for each CVS working directory.
 * This path is either a full-path or a path relative to CVSROOT.
 * 
 * The only advantage that I can see to having a relative path is that One can
 * change the physical location of the master source repository, change one's
 * CVSROOT environment variable, and CVS will work without problems.  I
 * recommend using full-paths.
 */
#ifndef RELATIVE_REPOS
/* #define	RELATIVE_REPOS	 */
#endif

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
 * The "cvs admin" command allows people to get around most of the logging
 * and info procedures within CVS.  For exmaple, "cvs tag tagname filename"
 * will perform some validity checks on the tag, while "cvs admin -Ntagname"
 * will not perform those checks.  For this reason, some sites may wish to
 * disable the admin function completely.
 *
 * To disable the admin function, uncomment the lines below.
 */
#ifndef CVS_NOADMIN
/* #define CVS_NOADMIN */
#endif

/*
 * The "cvs diff" command accepts all the single-character options that GNU
 * diff (1.15) accepts.  Except -D.  GNU diff uses -D as a way to put
 * cpp-style #define's around the output differences.  CVS, by default, uses
 * -D to specify a free-form date (like "cvs diff -D '1 week ago'").  If
 * you would prefer that the -D option of "cvs diff" work like the GNU diff
 * option, then comment out this define.
 */
#ifndef CVS_DIFFDATE
#define	CVS_DIFFDATE
#endif

/*
 * define this to enable the SETXID support (see FAQ 4D.13)
 * [ We have no such thing under OS/2, so far as I know. ]
 */
#undef SETXID_SUPPORT

/*
 * "cvs login" is under construction.  Don't define this unless you're
 * testing it, in which case you're me and you already know that.
 */
/* #define CVS_LOGIN */

/* End of CVS configuration section */

/*
 * Externs that are included in libc, but are used frequently enough to
 * warrant defining here.
 */
#ifndef STDC_HEADERS
extern void exit ();
#endif

#ifndef getwd
extern char *getwd ();
#endif

