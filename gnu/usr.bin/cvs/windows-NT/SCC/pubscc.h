/* This file was written by Jim Kingdon, and is hereby placed
   in the public domain.  */

/* Bits of the SCC interface.
   For paranoia's sake, I'm not using the same names as Microsoft.
   I don't imagine copying a few names could be a credible copyright
   case, but it seems safer to stick to only what is necessary for
   the interface to work.

   Note that some of the descriptions here have a certain amount of
   guesswork (for example, sometimes I have tried to translate to CVS
   terminology without actually verifying that the item means what I
   think it does).  If you find errors, please let us know according to
   the usual procedures for reporting CVS bugs.  */
typedef long SCC_return;
#define SCC_return_success 0
#define SCC_return_unknown_project -2
/* The file is not under SCC control.  */
#define SCC_return_non_scc_file -11
/* This operation is not supported.  I believe this status can only
   be returned from SccGet, SccAdd, SccRemove, SccHistory, or
   SccQueryInfo.  I'm not really sure what happens if it is returned
   from other calls.  */
#define SCC_return_not_supported -14
#define SCC_return_non_specific_error -15

enum SCC_command
{
	SCC_command_get,
	SCC_command_checkout,
	SCC_command_checkin,
	SCC_command_uncheckout,
	SCC_command_add,
	SCC_command_remove,
	SCC_command_diff,
	SCC_command_history,
	SCC_command_rename,
	SCC_command_properties,
	SCC_command_options
};

/* Outproc codes, for second argument to outproc.  */
#define SCC_outproc_info 1L
#define SCC_outproc_warning 2L
#define SCC_outproc_error 3L
/* Codes 4-7 relate to cancels and are only supported if the
   development environment said so with SccSetOption.  */
/* A status message, typically goes in something analogous to the emacs
   minibuffer.  For both this and SCC_outproc_nostatus, the development
   environment returns SCC_outproc_return_cancelled if the user has
   hit the cancel button.  */
#define SCC_outproc_status 4L
/* Like SCC_outproc_status, but there is no message to report.  */
#define SCC_outproc_nostatus 5L
/* Tell the development environment to offer a cancel button.  */
#define SCC_outproc_cancel_on 6L
/* Tell the development environment to not offer a cancel button.  */
#define SCC_outproc_cancel_off 7L

/* Return values from outproc.  */
#define SCC_outproc_return_success 0L
#define SCC_outproc_return_cancelled -1L
typedef long (*SCC_outproc) (char *, long);

typedef BOOL (*SCC_popul_proc) (LPVOID callerdat, BOOL add_keep,
                                LONG status, LPCSTR file);

/* Maximum sizes of various strings.  These are arbitrary limits
   which are imposed by the SCC.  */
/* Name argument to SccInitialize.  */
#define SCC_max_name 31
/* Path argument to SccInitialize.  */
#define SCC_max_init_path 31
/* Various paths many places in the interface.  */
#include <stdlib.h>
#define SCC_max_path _MAX_PATH

/* Status codes, as used by QueryInfo and GetEvents.  */
/* This means that we can't get status.  If the status is not
   SCC_status_error, then the status is a set of bit flags, as defined by
   the other SCC_status_* codes.  */
#define SCC_status_error -1L

/* The following status codes are things which the development environment
   is encouraged to check to determine things like whether to allow
   a checkin.  */
/* The current user has the file checked out (that is, under "cvs edit").
   It may or may not be in the directory where the development
   environment thinks it should be.  */
#define SCC_status_out_me 0x1000L
/* Should be set only if out_me is set.  The file is checked out where
   the development environment thinks it should be.  */
#define SCC_status_out_here 2L
/* Some other user has the file checked out.  */
#define SCC_status_out_someoneelse 4L
/* Reserved checkouts are in effect for the file.  */
#define SCC_status_reserved 8L
/* Reserved checkouts are not in effect for the file.  Multiple users
   can edit it.  Only one of SCC_status_reserved or SCC_status_nonreserved
   should be set.  I think maybe this flag should only be set if there
   actually is more than one copy currently checked out.  */
#define SCC_status_nonreserved 0x10L

/* The following flags are intended for the development environment to
   display the status of a file.  We are allowed to support them or not
   as we choose.  */
/* The file in the working directory is not the latest version in the
   repository.  Like when "cvs status" says "Needs Checkout".  */
#define SCC_status_needs_update 0x20L
/* The file is no longer in the project.  I think this is the case where
   cvs update prints "file xxx is no longer pertinent" (but I don't know,
   there are other statuses involved with removed files).  */
#define SCC_status_not_pertinent 0x40L
/* No checkins are permitted for this file.  No real CVS analogue, because
   this sort of thing would be done by commitinfo, &c.  */
#define SCC_status_no_checkins 0x80L
/* There was a merge, but the user hasn't yet dealt with it.  I think this
   probably should be used both if there were conflicts on the merge and
   if there were not (not sure, though).  */
#define SCC_status_had_conflicts 0x100L
/* This indicates something has happened to the file.  I suspect it mainly
   is intended for cases in which we detect that something happened to the
   file behind our backs.  I suppose CVS might use this for cases in which
   sanity checks on the CVSADM files fail, or in which the file has been
   made read/write without a "cvs edit", or that sort of thing.

   Or maybe it should be set if the file has been edited in the
   normal fashion.  I'm not sure.  */
#define SCC_status_munged 0x800L
/* The file exists in several projects.  In CVS I would suppose the
   equivalent probably would be that several modules (using features
   like -d) are capable of checking out a given file in the repository
   in several locations.  CVS has no current ability to give a different
   status when that has happened, but it might be cool.  */
#define SCC_status_several_projects 0x200L
/* There is a sticky tag or date in effect.  */
#define SCC_status_stuck 0x400L

/* Bits to set in the caps used by SccInitialize.  Most of these are
   relatively straightforward, for example SCC_cap_QueryInfo is set to
   indicate that the SccQueryInfo function is supported.  */
/* CodeWright 5.00b and 5.00c seem to call SccQueryInfo regardless of whether
   this bit is set in caps.  */
#define SCC_cap_QueryInfo 0x80L
#define SCC_cap_GetProjPath 0x200L
#define SCC_cap_AddFromScc 0x400L
#define SCC_cap_want_outproc 0x8000L

/* These are command options.  Some of them are specific to a particular
   command, some of them are good for more than one command.  Because many
   values are reused for different commands, look at the listed commands
   to see what a particular value means for a particular command.  */
/* Recurse into directories.  SccGet.  */
#define SCC_cmdopt_recurse 2L
/* This means to get all the files in a directory.  SccGet.  */
#define SCC_cmdopt_dir 1L
/* Without this flag, after a checkin, files are normally not checked
   out.  This flag disables that handling, and if it is set files will
   still be checked out after the checkin completes.  SccCheckin, SccAdd.  */
#define SCC_cmdopt_no_unedit 0x1000L
/* File is text.  SccAdd.  */
#define SCC_cmdopt_text 1L
/* File is binary.  SccAdd.  */
#define SCC_cmdopt_binary 2L
/* We are supposed to decide whether it is text or binary.  We can use the
   CVS wrappers stuff to decide based on the file name.  Obviously, this
   constant is just another way of saying that neither SCC_cmdopt_text nor
   SCC_cmdopt_binary are set.  SccAdd.  */
#define SCC_cmdopt_auto 0L
/* Maintain only a head revision for the file, no history.  SccAdd.  */
#define SCC_cmdopt_only_one 4L
/* In addition to removing the file from the repository, also delete it
   from the working directory.  My guess is that development environments
   would generally tend to pass this flag by default.  SccRemove.  */
#define SCC_cmdopt_retain_local 1L
/* Compare files in a case-insensitive fashion.  SccDiff.  */
#define SCC_cmdopt_case_insensitive 2L
/* Ignore whitespace in comparing files.  SccDiff.  */
#define SCC_cmdopt_ignore_all_space 4L
/* Instead of generating diffs, just say whether files are equal, based on
   the file contents.  SccDiff.  */
#define SCC_cmdopt_compare_files 0x10L
/* Instead of generating diffs, just say whether files are equal.  This may
   use a checksum if we want, or if not, it can be the same as
   SCC_cmdopt_compare_files.  */
#define SCC_cmdopt_consult_checksum 0x20L
/* Instead of generating diffs, just say whether files are equal.  This may
   use a timestamp if we want, or if not, it can be the same as either
   SCC_cmdopt_consult_checksum or SCC_cmdopt_compare_files.  */
#define SCC_cmdopt_consult_timestamp 0x40L

/* Values for the flags argument to OpenProject.  */
/* If this is set, and the development environment tries to open a project
   which doesn't exist, then create it.  */
#define SCC_open_autocreate 1L
/* If autocreate is not set, and the development environment tries to
   open a project which doesn't exist, and may_prompt is set, we are
   allowed to prompt the user to create a new project.  If may_prompt
   is not set, we should just return SCC_return_unknown_project and
   not open the project.  */
#define SCC_open_may_prompt 2L

/* Constants for SccSetOption.  */
#define SCC_option_background 1L
/* If option is SCC_option_background, then val turns background
   processing on or off.  If it is off, we can, if we need to, queue
   up events or something which won't disturb the development
   environment.  */
#  define SCC_option_background_yes 1L
#  define SCC_option_background_no 0L
#define SCC_option_cancel 3L
/* If option is SCC_option_cancel, then val says whether the development
   environment supports the SCC_outproc_* codes related to having the
   development environment handle a cancel button.  If this is not set,
   we are allowed/encouraged to implement a cancel button ourselves.  */
#  define SCC_option_cancel_on 1L
#  define SCC_option_cancel_off 0L
/* A SCC_option_* value of 10 has also been observed (I think from
   CodeWright 5.00).  I have no idea what it means; it isn't documented
   by the SCC API from Microsoft (version 0.99.0823).  */

/* The "void *context_arg" argument to most of the Scc* functions
   stores a pointer to a structure that the version control system
   gets to allocate, so it doesn't need any global variables.  */

/* In addition to context_arg, most of the Scc* functions take a
   "HWND window" argument.  This is so that we can put up dialogs.
   The window which is passed in is the IDE's window, which we
   should use as the parent of dialogs that we put up.  */

#include <windows.h>

/* Return the version of the SCC spec, major version in the high word,
   minor version in the low word.  Recommended value is 0x10001 for
   version 1.1 of the spec.  */
extern LONG SccGetVersion (void);

/* Set up the version control system.  This should be called before any
   other SCC calls other than SccGetVersion.  */
extern SCC_return SccInitialize
  (/* The version control system should allocate the context argument
      in SccInitialize and store a pointer to it in *contextp.  */
   void **contextp,

   HWND window, LPSTR caller,
   /* Version control system should copy in the
      name of the version control system here,
      up to SCC_max_name bytes.  */
   LPSTR name,

   /* Version control system should set *caps to indicate what it
      supports, using bits from SCC_cap_*.  */
   LPLONG caps,

   /* Version control system should copy in a string here, that the
      development environment can put places like a makefile to
      distinguish this version control system from others.  Up to
      SCC_max_init_path bytes.  */
   LPSTR path,

   /* Version control system should set these to the maximum size for
      checkout comments and comments.  I'm not sure whether existing
      development environments tend to allocate fixed size arrays
      based on the return length (I would recommend that a development
      environment not do so, but that is a different question).  */
   LPDWORD co_comment_len,
   LPDWORD comment_len);

/* The version control system should free any resources it has allocated,
   including the context structure itself.  */
extern SCC_return SccUninitialize (void *context_arg);

extern SCC_return SccOpenProject
  (void *context_arg, HWND window, LPSTR user,
   LPSTR project, LPSTR local_proj,
   LPSTR aux_proj,
   LPSTR comment,

   /* This is the function which the version control system can call
      to ask the development environment to display output, or
      (SCC_outproc)0 if the development environment doesn't support
      the outproc feature.  */
   SCC_outproc outproc,

   /* One or more of the SCC_open_* settings.  */
   LONG flags);

extern SCC_return SccCloseProject (void *context_arg);

/* cvs get.  */
extern SCC_return SccGet
  (void *context_arg, HWND window,

   /* Files to get, where file_names is an array
      of num_files names.  */
   /* As with all file names passed to us by the SCC, these file names
      are absolute pathnames.  I think they will tend to be paths
      within the local directory specified by the local_proj argument
      to SccOpenProject, although I don't know whether there are any
      exceptions to that.  */
   LONG num_files,
   LPSTR *file_names,

   /* Command options.  */
   LONG options,

   void *prov_options);

/* cvs edit.  */
extern SCC_return SccCheckout
  (void *context_arg, HWND window, 

   /* Files to operate on, where file_names is an array of num_files
      names.  */
   LONG num_files,
   LPSTR *file_names,

   LPSTR comment,

   /* Command options.  I'm not sure what command options, if any, are
      defined for SccCheckout.  */
   LONG options,

   void *prov_options);

/* cvs ci.  */
extern SCC_return SccCheckin
  (void *context_arg, HWND window,

   /* Files to operate on, where file_names is an array of num_files
      names.  */
   LONG num_files,
   LPSTR *file_names,

   LPSTR comment,

   /* Command options.  */
   LONG options,

   void *prov_options);

/* cvs unedit.  */
extern SCC_return SccUncheckout
  (void *context_arg, HWND window,

   /* Files to operate on, where file_names is an array of num_files
      names.  */
   LONG num_files,
   LPSTR *file_names,

   /* Command options.  I'm not sure what command options, if any, are
      defined for SccUncheckout.  */
   LONG options,

   void *prov_options);

/* cvs add + cvs ci, more or less, I think (but see also
   the "keep checked out" flag in options).  */
extern SCC_return SccAdd
  (void *context_arg, HWND window,

   /* Files to operate on, where file_names is an array of num_files
      names.  */
   LONG num_files,
   LPSTR *file_names,

   LPSTR comment,

   /* Array of num_files command options, one for each file.  */
   LONG *options,

   void *prov_options);

/* cvs rm -f + cvs ci, I think.  Should barf if SCC_REMOVE_KEEP
   (or maybe just put the file there, as if the user had removed
   it and then done a "copy <saved-file> <filename>".  */
extern SCC_return SccRemove
  (void *context_arg, HWND window,

   /* Files to operate on, where file_names is an array of num_files
      names.  */
   LONG num_files,
   LPSTR *file_names,

   LPSTR comment,

   /* Command options.  */
   LONG options,

   void *prov_options);

/* mv, cvs add, cvs rm, and cvs ci, I think.  */
extern SCC_return SccRename
  (void *context_arg, HWND window, LPSTR old_name,
   LPSTR new_name);

/* If SCC_cmdopt_compare_files, SCC_cmdopt_consult_checksum, or
   SCC_cmdopt_consult_timestamp, then we are supposed to silently
   return a status, without providing any information directly to the
   user.  For no args or checksum (which we fall back to full compare)
   basically a call to No_Diff or ? in the client case.  For
   timestamp, just a Classify_File.  Now, if contents not set, then
   want to do a cvs diff, and preferably start up WinDiff or something
   (to be determined, for now perhaps could just return text via
   outproc).  */
extern SCC_return SccDiff
  (void *context_arg, HWND window, LPSTR file_name,

   /* Command options.  */
   LONG options,

   void *prov_options);

/* cvs log, I presume.  If we want to get fancier we could bring
   up a screen more analogous to the tkCVS log window, let the user
   do "cvs update -r", etc.  */
extern SCC_return SccHistory
  (void *context_arg, HWND window,

   /* Files to operate on, where file_names is an array of num_files
      names.  */
   LONG num_files,
   LPSTR *file_names,

   /* Command options.  I'm not sure what command options,
      if any, are defined for SccHistory.  */
   LONG options,

   void *prov_options);

/* cvs status, presumably.  */
extern SCC_return SccProperties
  (void *context_arg, HWND window, LPSTR file_name);

/* Not sure what this should do.  The most obvious thing is some
   kind of front-end to "cvs admin" but I'm not actually sure that
   is the most useful thing.  */
extern SCC_return SccRunScc
  (void *context_arg, HWND window,

   LONG num_files,
   LPSTR *file_names);

/* If the user invokes version-control-system-defined behavior
   (typically by clicking an Advanced button in a dialog, e.g. the Get
   dialog), and the user clicks on that button, then the development
   environment calls SccGetCommandOptions.  The version control system
   interacts with the user and then sets *PROV_OPTIONSP to whatever it
   wants.  The development environment doesn't do anything with it,
   but does pass it to the various commands as prov_options.  If it
   calls SccGetCommandOptions again, it will pass the same value (so
   user choices from the previous "Advanced" click can serve as
   defaults).

   Note that "provider options" (prov_options) are unrelated to
   "command options" (SCC_cmdopt_*).  */

extern SCC_return SccGetCommandOptions
  (void *context_arg, HWND window,
   enum SCC_command command,
   void **prov_optionsp);

/* Not existing CVS functionality, I don't think.
   Need to be able to tell user about what files
   are out there without actually getting them.  */
extern SCC_return SccPopulateList
  (void *context_arg, enum SCC_command command,

   LONG num_files,
   LPSTR *file_names,

   SCC_popul_proc populate,
   void *callerdat,

   /* Command options.  I'm not sure what command options,
      if any, are defined for SccPopulateList.  */
   LONG options);

/* cvs status, sort of.  */
extern SCC_return SccQueryInfo
  (void *context_arg,

   LONG num_files, LPSTR *file_names,

   /* This is an array of NUM_FILES entries.  In each one
      we store a SCC_status_* code.  */
   LPLONG status);

/* Like QueryInfo, but fast and for only a single file.  For example, the
   development environment might call this quite frequently to keep its
   screen display updated.  Supposed to only return cached status
   information, not go to disk or anything.  I assume that
   QueryInfo and probably the usual calls like Get would cause
   the version control system to cache the status in the first place.  */
extern SCC_return SccGetEvents
  (void *context_arg, LPSTR file_name,

   /* Here the version control system stores the SCC_status_* code.  */
   LPLONG status,

   LPLONG events_remaining);

/* This is where the user gives us the CVSROOT.  */
extern SCC_return SccGetProjPath
  (void *context_arg, HWND window, LPSTR user,

   /* Version control system copies in the project name
      here, up to SCC_max_path bytes.  */
   LPSTR proj_name,

   /* If allow_change, the version control system may copy
      into this field, up to SCC_max_path bytes.  */
   LPSTR local_proj,

   /* Version control system copies into this field, up to
      SCC_max_path bytes.  */
   LPSTR aux_proj,

   BOOL allow_change, BOOL *new);

/* Pretty much similar to SccPopulateList.  Not sure whether this also
   involves getting the files, or whether the development environment will
   typically call SccGet after this function.  */
extern SCC_return SccAddFromScc
  (void *context_arg, HWND window,

   /* Version control system sets *files to the number of files and
      *file_names to an array each element of which and contains the
      name of one of the files.  The names can be relative pathnames
      (e.g. "foo.c").  If files is NULL, that means something different;
      the version control system should free the memory that it allocated
      for file_names.  */
   LONG *files,
   char ***file_names);

/* This changes several aspects of how we interact with the IDE.  */
extern SCC_return SccSetOption
  (void *context_arg,
   /* One of the SCC_option_* codes.  */
   LONG option,
   /* Meaning of this will depend on the value of option.  */
   LONG val);

/* New functions with CodeWright 5.00c: SccAddRef, SccRelease,
   SccDiffToRev, SccLabel, SccLock and SccMerge.  I don't have any
   details on them.  */
