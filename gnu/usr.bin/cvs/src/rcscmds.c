/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * The functions in this file provide an interface for performing 
 * operations directly on RCS files. 
 */

#include "cvs.h"
#include <assert.h>

/* This file, rcs.h, and rcs.c, are intended to define our interface
   to RCS files.  As of July, 1996, there are still a few places that
   still exec RCS commands directly.  The intended long-term direction
   is to have CVS access RCS files via an RCS library (rcs.c can be
   considered a start at one), for performance, cleanliness (CVS has
   some awful hacks to work around RCS behaviors which don't make
   sense for CVS), installation hassles, ease of implementing the CVS
   server (I don't think that the output-out-of-order bug can be
   completely fixed as long as CVS calls RCS), and perhaps other
   reasons.

   Whether there will also be a version of RCS which uses this
   library, or whether the library will be packaged for uses beyond
   CVS or RCS (many people would like such a thing) is an open
   question.  Some considerations:

   1.  An RCS library for CVS must have the capabilities of the
   existing CVS code which accesses RCS files.  In particular, simple
   approaches will often be slow.

   2.  An RCS library should not use the code from the current RCS
   (5.7 and its ancestors).  The code has many problems.  Too few
   comments, too many layers of abstraction, too many global variables
   (the correct number for a library is zero), too much intricately
   interwoven functionality, and too many clever hacks.  Paul Eggert,
   the current RCS maintainer, agrees.

   3.  More work needs to be done in terms of separating out the RCS
   library from the rest of CVS (for example, cvs_output should be
   replaced by a callback, and the declarations should be centralized
   into rcs.h, and probably other such cleanups).

   4.  To be useful for RCS and perhaps for other uses, the library
   may need features beyond those needed by CVS.

   5.  Any changes to the RCS file format *must* be compatible.  Many,
   many tools (not just CVS and RCS) can at least import this format.
   RCS and CVS must preserve the current ability to import/export it
   (preferably improved--magic branches are currently a roadblock).
   TODO: improve rcsfile.5 in the RCS distribution so that it more
   completely documents this format.

   On somewhat related notes:

   1.  A library for diff is an obvious idea.  The one thing which I'm
   not so sure about is that I think CVS probably wants the ability to
   allow arbitrarily-bizarre (and possibly customized for particular
   file formats) external diff programs.

   2.  A library for patch is another such idea.  CVS's needs are
   smaller than the functionality of the standalone patch program (it
   only calls patch in the client, and only needs to be able to patch
   unmodified versions, which is something that RCS_deltas already
   does in a different context).  But it is silly for CVS to be making
   people install patch as well as CVS for such a simple purpose.  */

/* For RCS file PATH, make symbolic tag TAG point to revision REV.
   This validates that TAG is OK for a user to use.  Return value is
   -1 for error (and errno is set to indicate the error), positive for
   error (and an error message has been printed), or zero for success.  */

int
RCS_exec_settag(path, tag, rev)
    const char *path;
    const char *tag;
    const char *rev;
{
    run_setup ("%s%s -x,v/ -q -N%s:%s", Rcsbin, RCS, tag, rev);
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
}

/* NOERR is 1 to suppress errors--FIXME it would
   be better to avoid the errors or some cleaner solution.  */
int
RCS_exec_deltag(path, tag, noerr)
    const char *path;
    const char *tag;
    int noerr;
{
    run_setup ("%s%s -x,v/ -q -N%s", Rcsbin, RCS, tag);
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, noerr ? DEVNULL : RUN_TTY, RUN_NORMAL);
}

/* set RCS branch to REV */
int
RCS_exec_setbranch(path, rev)
    const char *path;
    const char *rev;
{
    run_setup ("%s%s -x,v/ -q -b%s", Rcsbin, RCS, rev ? rev : "");
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
}

/* Lock revision REV.  NOERR is 1 to suppress errors--FIXME it would
   be better to avoid the errors or some cleaner solution.  */
int
RCS_exec_lock(path, rev, noerr)
    const char *path;
    const char *rev;
    int noerr;
{
    run_setup ("%s%s -x,v/ -q -l%s", Rcsbin, RCS, rev ? rev : "");
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, noerr ? DEVNULL : RUN_TTY, RUN_NORMAL);
}

/* Unlock revision REV.  NOERR is 1 to suppress errors--FIXME it would
   be better to avoid the errors or some cleaner solution.  */
int
RCS_exec_unlock(path, rev, noerr)
    const char *path;
    const char *rev;
    int noerr;
{
    run_setup ("%s%s -x,v/ -q -u%s", Rcsbin, RCS, rev ? rev : "");
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, noerr ? DEVNULL : RUN_TTY, RUN_NORMAL);
}

/* Merge revisions REV1 and REV2. */
int
RCS_merge(path, options, rev1, rev2)
     const char *path;
     const char *options;
     const char *rev1;
     const char *rev2;
{
    int status;

    /* XXX - Do merge by hand instead of using rcsmerge, due to -k handling */

    run_setup ("%s%s -x,v/ %s -r%s -r%s %s", Rcsbin, RCS_RCSMERGE,
	       options, rev1, rev2, path);
    status = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
#ifndef HAVE_RCS5
    if (status == 0) 
    {
	/* Run GREP to see if there appear to be conflicts in the file */
	run_setup ("%s", GREP);
	run_arg (RCS_MERGE_PAT);
	run_arg (path);
	status = (run_exec (RUN_TTY, DEVNULL, RUN_TTY, RUN_NORMAL) == 0);

    }
#endif
    return status;
}

/* Check out a revision from RCSFILE into WORKFILE, or to standard output
   if WORKFILE is NULL.  TAG is the tag to check out, or NULL if one
   should check out the head of the default branch.  OPTIONS is a string
   such as -kb or -kkv, for keyword expansion options, or NULL if there
   are none.  If WORKFILE is NULL, run regardless of noexec; if non-NULL,
   noexec inhibits execution.  SOUT is what to do with standard output
   (typically RUN_TTY).  */
int
RCS_exec_checkout (rcsfile, workfile, tag, options, sout)
    char *rcsfile;
    char *workfile;
    char *tag;
    char *options;
    char *sout;
{
    run_setup ("%s%s -x,v/ -q %s%s", Rcsbin, RCS_CO,
               tag ? "-r" : "", tag ? tag : "");
    if (options != NULL && options[0] != '\0')
	run_arg (options);
    if (workfile == NULL)
	run_arg ("-p");
    run_arg (rcsfile);
    if (workfile != NULL)
	run_arg (workfile);
    return run_exec (RUN_TTY, sout, RUN_TTY,
                     workfile == NULL ? (RUN_NORMAL | RUN_REALLY) : RUN_NORMAL);
}

/* Check in to RCSFILE with revision REV (which must be greater than the
   largest revision) and message MESSAGE (which is checked for legality).
   If FLAGS & RCS_FLAGS_DEAD, check in a dead revision.  If FLAGS &
   RCS_FLAGS_QUIET, tell ci to be quiet.  If FLAGS & RCS_FLAGS_MODTIME,
   use the working file's modification time for the checkin time.
   WORKFILE is the working file to check in from, or NULL to use the usual
   RCS rules for deriving it from the RCSFILE.  */
int
RCS_checkin (rcsfile, workfile, message, rev, flags)
    char *rcsfile;
    char *workfile;
    char *message;
    char *rev;
    int flags;
{
    run_setup ("%s%s -x,v/ -f %s%s", Rcsbin, RCS_CI,
	       rev ? "-r" : "", rev ? rev : "");
    if (flags & RCS_FLAGS_DEAD)
	run_arg ("-sdead");
    if (flags & RCS_FLAGS_QUIET)
	run_arg ("-q");
    if (flags & RCS_FLAGS_MODTIME)
	run_arg ("-d");
    run_args ("-m%s", make_message_rcslegal (message));
    if (workfile != NULL)
	run_arg (workfile);
    run_arg (rcsfile);
    return run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
}
