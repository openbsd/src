/* This file was written by Jim Kingdon, and is hereby placed
   in the public domain.  */

#include <Wtypes.h>
#include <stdio.h>
#include <direct.h> /* For chdir */

#include "pubscc.h"

/* We get to put whatever we want here, and the caller will pass it
   to us, so we don't need any global variables.  This is the
   "void *context_arg" argument to most of the Scc* functions.  */
struct context {
    FILE *debuglog;
    /* Value of the CVSROOT we are currently working with (that is, the
       "open project" in SCC terminology), malloc'd, or NULL if there is
       no project currently open.  */
    char *root;
    /* Local directory (working directory in CVS parlance).  */
    char *local;
    SCC_outproc outproc;
};

/* In addition to context_arg, most of the Scc* functions take a
   "HWND window" argument.  This is so that we can put up dialogs.
   The window which is passed in is the IDE's window, which we
   should use as the parent of dialogs that we put up.  */

#include <windows.h>

/* Report a malloc error and return the SCC_return_* value which the
   caller should return to the IDE.  Probably this should be getting
   the window argument too, but for the moment we don't need it.
   Note that we only use this for errors which occur after the
   context->outproc is set up.  */
SCC_return
malloc_error (struct context *context)
{
    (*context->outproc) ("Out of memory\n", SCC_outproc_error);
    return SCC_return_non_specific_error;
}

/* Return the version of the SCC spec, major version in the high word,
   minor version in the low word.  */
LONG
SccGetVersion (void)
{
    /* We implement version 1.1 of the spec.  */
    return 0x10001;
}

SCC_return
SccInitialize (void **contextp, HWND window, LPSTR caller, LPSTR name,
               LPLONG caps, LPSTR path, LPDWORD co_comment_len,
               LPDWORD comment_len)
{
    struct context *context;
    FILE *fp;
    fp = fopen ("d:\\debug.scc", "w");
    if (fp == NULL)
        /* Do what?  Return some error value?  */
        abort ();
    context = malloc (sizeof (struct context));
    if (context == NULL)
    {
        fprintf (fp, "Out of memory\n");
        fclose (fp);
        /* Do what?  Return some error?  */
        abort ();
    }
    context->debuglog = fp;
    context->root = NULL;
    *contextp = context;
    fprintf (fp, "Made it into SccInitialize!\n");
    *caps = (SCC_cap_GetProjPath
	     | SCC_cap_AddFromScc
	     | SCC_cap_want_outproc);

    /* I think maybe this should have some more CVS-like
       name, like "CVS Root", if we decide that is what
       a SCC "project" is.  */
    strncpy (path, "CVS Project:", SCC_max_init_path);
    fprintf (fp, "Caller name is %s\n", caller);
    strncpy (name, "CVS", SCC_max_name);
    /* CVS has no limit on comment length.  But I suppose
       we need to return a value which is small enough for
       a caller to allocate a buffer this big.  Not that I
       would write a caller that way, but.....  */
    *co_comment_len = 8192;
    *comment_len = 8192;
    fflush (fp);
    return SCC_return_success;
}

SCC_return
SccUninitialize (void *context_arg)
{
    struct context *context = (struct context *)context_arg;
    if (ferror (context->debuglog))
	/* FIXME: return error value...  */
    if (fclose (context->debuglog) == EOF)
        /* FIXME: return error value, I think.  */
        ;
    free (context);
    return SCC_return_success;
}

SCC_return
SccOpenProject (void *context_arg, HWND window, LPSTR user,
		LPSTR project, LPSTR local_proj, LPSTR aux_proj,
		LPSTR comment, SCC_outproc outproc,
		LONG flags)
{
    struct context *context = (struct context *)context_arg;

    /* This can happen if the IDE opens a project which is not under
       CVS control.  I'm not sure whether checking for aux_proj
       being "" is the right way to detect this case, but it seems
       it should work because I think that the source code control
       system is what has control over the contents of aux_proj.  */
    if (aux_proj[0] == '\0')
	return SCC_return_unknown_project;

    context->root = malloc (strlen (aux_proj) + 5);
    if (context->root == NULL)
	return SCC_return_non_specific_error;
    strcpy (context->root, aux_proj);
    /* Since we don't yet support creating projects, we don't
       do anything with flags.  */

    if (outproc == 0)
    {
	/* This supposedly can happen if the IDE chooses not to implement
	   the outproc feature.  */
	fprintf (context->debuglog, "Uh oh.  outproc is a null pointer\n");
	context->root = NULL;
	fflush (context->debuglog);
	return SCC_return_non_specific_error;
    }
    context->outproc = outproc;

    fprintf (context->debuglog, "SccOpenProject (aux_proj=%s)\n", aux_proj);

    context->local = malloc (strlen (local_proj) + 5);
    if (context->local == NULL)
	return malloc_error (context);
    strcpy (context->local, local_proj);

    fflush (context->debuglog);
    return SCC_return_success;
}

SCC_return
SccCloseProject (void *context_arg)
{
    struct context *context = (struct context *)context_arg;
    fprintf (context->debuglog, "SccCloseProject\n");
    fflush (context->debuglog);
    if (context->root != NULL)
	free (context->root);
    context->root = NULL;
    return SCC_return_success;
}

/* cvs get.  */
SCC_return
SccGet (void *context_arg, HWND window, LONG num_files,
        LPSTR *file_names,
	LONG options,
	void *prov_options)
{
    struct context *context = (struct context *)context_arg;
    int i;
    char *fname;

    fprintf (context->debuglog, "SccGet: %d; files:", num_files);
#if 1
    for (i = 0; i < num_files; ++i)
    {
	fprintf (context->debuglog, "%s ", file_names[i]);
    }
#endif
    fprintf (context->debuglog, "\n");
    if (options & SCC_cmdopt_dir)
	fprintf (context->debuglog, "  Get all\n");
    /* Should be using this flag to set -R vs. -l.  */
    if (options & SCC_cmdopt_recurse)
	fprintf (context->debuglog, "  recurse\n");

    for (i = 0; i < num_files; ++i)
    {
	/* As with all file names passed to us by the SCC, these
	   file names are absolute pathnames.  I think they will
	   tend to be paths within context->local, although I
	   don't know whether there are any exceptions to that.  */
	fname = file_names[i];
	fprintf (context->debuglog, "%s ", fname);
	/* Here we would write to the file named fname.  */
    }
    fprintf (context->debuglog, "\nExiting SccGet\n");
    fflush (context->debuglog);
    return SCC_return_success;
}

/* cvs edit.  */
SCC_return
SccCheckout (void *context_arg, HWND window, LONG num_files,
             LPSTR *file_names, LPSTR comment,
	     LONG options,
             void *prov_options)
{
    struct context *context = (struct context *)context_arg;
    fprintf (context->debuglog, "SccCheckout num_files=%ld\n", num_files);
    fflush (context->debuglog);
    /* For the moment we say that all files are not ours.  I'm not sure
       whether this is ever necessary; that is, whether the IDE will call
       us except where we have told the IDE that a file is under source
       control.  */
    /* I'm not sure what we would do if num_files > 1 and we wanted to
       return different statuses for different files.  */
    return SCC_return_non_scc_file;
}

/* cvs ci.  */
SCC_return
SccCheckin (void *context_arg, HWND window, LONG num_files,
            LPSTR *file_names, LPSTR comment,
	    LONG options,
            void *prov_options)
{
    return SCC_return_not_supported;
}

/* cvs unedit.  */
SCC_return
SccUncheckout (void *context_arg, HWND window, LONG num_files,
               LPSTR *file_names,
	       LONG options,
	       void *prov_options)
{
    return SCC_return_not_supported;
}

/* cvs add + cvs ci, more or less, I think (but see also
   the "keep checked out" flag in options).  */
SCC_return
SccAdd (void *context_arg, HWND window, LONG num_files,
        LPSTR *file_names, LPSTR comment,
	LONG *options,
        void *prov_options)
{
    return SCC_return_not_supported;
}

/* cvs rm -f + cvs ci, I think.  Should barf if SCC_REMOVE_KEEP
   (or maybe just put the file there, as if the user had removed
   it and then done a "copy <saved-file> <filename>".  */
SCC_return
SccRemove (void *context_arg, HWND window, LONG num_files,
           LPSTR *file_names, LPSTR comment,
	   LONG options,
           void *prov_options)
{
    return SCC_return_not_supported;
}

/* mv, cvs add, cvs rm, and cvs ci, I think.  */
SCC_return
SccRename (void *context_arg, HWND window, LPSTR old_name,
           LPSTR new_name)
{
    return SCC_return_not_supported;
}

/* If SCC_cmdopt_compare_files, SCC_cmdopt_consult_checksum, or
   SCC_cmdopt_consult_timestamp, then we are supposed to silently
   return a status, without providing any information directly to the
   user.  For no args or checksum (which we fall back to full compare)
   basically a call to No_Diff or ? in the client case.  For
   timestamp, just a Classify_File.  Now, if contents not set, then
   want to do a cvs diff, and preferably start up WinDiff or something
   (to be determined, for now perhaps could just return text via
   outproc).  */
SCC_return
SccDiff (void *context_arg, HWND window, LPSTR file_name,
         LONG options,
	 void *prov_options)
{
    return SCC_return_not_supported;
}

/* cvs log, I presume.  If we want to get fancier we could bring
   up a screen more analogous to the tkCVS log window, let the user
   do "cvs update -r", etc.  */
SCC_return
SccHistory (void *context_arg, HWND window, LONG num_files,
            LPSTR *file_names,
	    LONG options,
	    void *prov_options)
{
    return SCC_return_not_supported;
}

/* cvs status, presumably.  */
SCC_return
SccProperties (void *context_arg, HWND window, LPSTR file_name)
{
    return SCC_return_not_supported;
}

/* Not sure what this should do.  The most obvious thing is some
   kind of front-end to "cvs admin" but I'm not actually sure that
   is the most useful thing.  */
SCC_return
SccRunScc (void *context_arg, HWND window, LONG num_files,
           LPSTR *file_names)
{
    return SCC_return_not_supported;
}

/* Lots of things that we could do here.  Options to get/update
   such as -r -D -k etc. just for starters.  Note that the terminology is
   a little confusing here.  This function relates to "provider options"
   (prov_options) which are a way for us to provide extra dialogs beyond
   the basic ones for a particular command.  It is unrelated to "command
   options" (SCC_cmdopt_*).  */
SCC_return
SccGetCommandOptions (void *context_arg, HWND window,
                      enum SCC_command command,
                      void **prov_optionsp)
{
    return SCC_return_not_supported;
}

/* Not existing CVS functionality, I don't think.
   Need to be able to tell user about what files
   are out there without actually getting them.  */
SCC_return
SccPopulateList (void *context_arg, enum SCC_command command,
                 LONG num_files,
                 LPSTR *file_names, SCC_popul_proc populate,
                 void *callerdat,
		 LONG options)
{
    return SCC_return_success;
}

/* cvs status, sort of.  */
SCC_return
SccQueryInfo (void *context_arg, LONG num_files, LPSTR *file_names,
              LPLONG status)
{
    return SCC_return_not_supported;
}

/* Like QueryInfo, but fast and for only a single file.  For example, the
   development environment might call this quite frequently to keep its
   screen display updated.  */
SCC_return
SccGetEvents (void *context_arg, LPSTR file_name,
	      LPLONG status,
              LPLONG events_remaining)
{
    /* They say this is supposed to only return cached status
       information, not go to disk or anything.  I assume that
       QueryInfo and probably the usual calls like Get would cause
       us to cache the status in the first place.  */
    return SCC_return_success;
}

/* This is where the user gives us the CVSROOT.  */
SCC_return
SccGetProjPath (void *context_arg, HWND window, LPSTR user,
                LPSTR proj_name, LPSTR local_proj, LPSTR aux_proj,
                BOOL allow_change, BOOL *new)
{
    /* For now we just hardcode the CVSROOT.  In the future we will
       of course prompt the user for it (simple implementation would
       have them supply a string; potentially better implementation
       would have menus or something for access methods and so on,
       although it might also have a way of bypassing that in case
       CVS supports new features that the GUI code doesn't
       understand).  We probably will also at some point want a
       "project" to encompass both a CVSROOT and a directory or
       module name within that CVSROOT, but we don't try to handle
       that yet either.  We also will want to be able to use "user"
       instead of having the username encoded in the aux_proj or
       proj_name, probably.  */

    struct context *context = (struct context *)context_arg;
    fprintf (context->debuglog, "SccGetProjPath called\n");

    /* At least for now we leave the proj_name alone, and just use
       the aux_proj.  */
    strncpy (proj_name, "zwork", SCC_max_path);
    strncpy (aux_proj, ":server:harvey:/home/kingdon/zwork/cvsroot",
	     SCC_max_path);
    if (local_proj[0] == '\0' && allow_change)
	strncpy (local_proj, "d:\\sccwork", SCC_max_path);
    /* I don't think I saw anything in the spec about this,
       but let's see if it helps.  */
    if (_chdir (local_proj) < 0)
	fprintf (context->debuglog, "Error in chdir: %s", strerror (errno));

    if (*new)
	/* It is OK for us to prompt the user for creating a new
	   project.  */
	/* We will say that the user said to create a new one.  */
	*new = 1;

    fflush (context->debuglog);
    return SCC_return_success;
}

/* Pretty much similar to SccPopulateList.  */
SCC_return
SccAddFromScc (void *context_arg, HWND window, LONG *files,
               char ***file_names)
{
    struct context *context = (struct context *)context_arg;

    /* For now we have hardcoded the notion that there are two files,
       foo.c and bar.c.  */
#define NUM_FILES 2
    if (files == NULL)
    {
	char **p;

	/* This means to free the memory that is allocated for
	   file_names.  */
	for (p = *file_names; *p != NULL; ++p)
	{
	    fprintf (context->debuglog, "Freeing %s\n", *p);
	    free (*p);
	}
    }
    else
    {
	*file_names = malloc ((NUM_FILES + 1) * sizeof (char **));
	if (*file_names == NULL)
	    return malloc_error (context);
	(*file_names)[0] = malloc (80);
	if ((*file_names)[0] == NULL)
	    return malloc_error (context);
	strcpy ((*file_names)[0], "foo.c");
	(*file_names)[1] = malloc (80);
	if ((*file_names)[1] == NULL)
	    return malloc_error (context);
	strcpy ((*file_names)[1], "bar.c");
	(*file_names)[2] = NULL;
	*files = 2;

	/* Are we supposed to also Get the files?  Or is the IDE
	   next going to call SccGet on each one?  The spec doesn't
	   say explicitly.  */
    }
    fprintf (context->debuglog, "Success in SccAddFromScc\n");
    fflush (context->debuglog);
    return SCC_return_success;
}

/* This changes several aspects of how we interact with the IDE.  */
SCC_return
SccSetOption (void *context_arg,
	      LONG option,
	      LONG val)
{
    return SCC_return_success;
}
