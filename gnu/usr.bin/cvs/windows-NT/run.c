/* run.c --- routines for executing subprocesses under Windows NT.
   
   This file is part of GNU CVS.

   GNU CVS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <process.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>

static void run_add_arg PROTO((const char *s));
static void run_init_prog PROTO((void));

extern char *strtok ();

/*
 * To exec a program under CVS, first call run_setup() to setup any initial
 * arguments.  The options to run_setup are essentially like printf(). The
 * arguments will be parsed into whitespace separated words and added to the
 * global run_argv list.
 * 
 * Then, optionally call run_arg() for each additional argument that you'd like
 * to pass to the executed program.
 * 
 * Finally, call run_exec() to execute the program with the specified arguments.
 * The execvp() syscall will be used, so that the PATH is searched correctly.
 * File redirections can be performed in the call to run_exec().
 */
static char **run_argv;
static int run_argc;
static int run_argc_allocated;

void
run_setup (const char *prog)
{
    char *cp;
    int i;

    char *run_prog;

    /* clean out any malloc'ed values from run_argv */
    for (i = 0; i < run_argc; i++)
    {
	if (run_argv[i])
	{
	    free (run_argv[i]);
	    run_argv[i] = (char *) 0;
	}
    }
    run_argc = 0;

    run_prog = xstrdup (prog);

    /* put each word into run_argv, allocating it as we go */
    for (cp = strtok (run_prog, " \t"); cp; cp = strtok ((char *) NULL, " \t"))
	run_add_arg (cp);

    free (run_prog);
}

void
run_arg (s)
    const char *s;
{
    run_add_arg (s);
}

/* Return a malloc'd copy of s, with double quotes around it.  */
static char *
quote (const char *s)
{
    size_t s_len = 0;
    char *copy = NULL;
    char *scan = (char *) s;

    /* scan string for extra quotes ... */
    while (*scan)
	if ('"' == *scan++)
	    s_len += 2;   /* one extra for the quote character */
	else
	    s_len++;
    /* allocate length + byte for ending zero + for double quotes around */
    scan = copy = xmalloc(s_len + 3);
    *scan++ = '"';
    while (*s)
    {
	if ('"' == *s)
	    *scan++ = '\\';
	*scan++ = *s++;
    }
    /* ending quote and closing zero */
    *scan++ = '"';
    *scan++ = '\0';
    return copy;
}

static void
run_add_arg (s)
    const char *s;
{
    /* allocate more argv entries if we've run out */
    if (run_argc >= run_argc_allocated)
    {
	run_argc_allocated += 50;
	run_argv = (char **) xrealloc ((char *) run_argv,
				     run_argc_allocated * sizeof (char **));
    }

    if (s)
    {
	run_argv[run_argc] = (run_argc ? quote (s) : xstrdup (s));
	run_argc++;
    }
    else
	run_argv[run_argc] = (char *) 0;	/* not post-incremented on purpose! */
}

int
run_exec (stin, stout, sterr, flags)
    const char *stin;
    const char *stout;
    const char *sterr;
    int flags;
{
    int shin, shout, sherr;
    int sain, saout, saerr;	/* saved handles */
    int mode_out, mode_err;
    int status = -1;
    int rerrno = 0;
    int rval   = -1;
    void (*old_sigint) (int);

    if (trace)			/* if in trace mode */
    {
	(void) fprintf (stderr, "-> system(");
	run_print (stderr);
	(void) fprintf (stderr, ")\n");
    }

    /* Flush standard output and standard error, or otherwise we end
       up with strange interleavings of stuff called from CYGWIN
       vs. CMD.  */

    fflush (stderr);
    fflush (stdout);

    if (noexec && (flags & RUN_REALLY) == 0) /* if in noexec mode */
	return (0);

    /*
     * start the engine and take off
     */

    /* make sure that we are null terminated, since we didn't calloc */
    run_add_arg ((char *) 0);

    /* setup default file descriptor numbers */
    shin = 0;
    shout = 1;
    sherr = 2;

    /* set the file modes for stdout and stderr */
    mode_out = mode_err = O_WRONLY | O_CREAT;
    mode_out |= ((flags & RUN_STDOUT_APPEND) ? O_APPEND : O_TRUNC);
    mode_err |= ((flags & RUN_STDERR_APPEND) ? O_APPEND : O_TRUNC);

    /* open the files as required, shXX are shadows of stdin... */
    if (stin && (shin = open (stin, O_RDONLY)) == -1)
    {
	rerrno = errno;
	error (0, errno, "cannot open %s for reading (prog %s)",
	       stin, run_argv[0]);
	goto out0;
    }
    if (stout && (shout = open (stout, mode_out, 0666)) == -1)
    {
	rerrno = errno;
	error (0, errno, "cannot open %s for writing (prog %s)",
	       stout, run_argv[0]);
	goto out1;
    }
    if (sterr && (flags & RUN_COMBINED) == 0)
    {
	if ((sherr = open (sterr, mode_err, 0666)) == -1)
	{
	    rerrno = errno;
	    error (0, errno, "cannot open %s for writing (prog %s)",
		   sterr, run_argv[0]);
	    goto out2;
	}
    }
    /* now save the standard handles */
    sain = saout = saerr = -1;
    sain  = dup( 0); /* dup stdin  */
    saout = dup( 1); /* dup stdout */
    saerr = dup( 2); /* dup stderr */

    /* the new handles will be dup'd to the standard handles
     * for the spawn.
     */

    if (shin != 0)
      {
	(void) dup2 (shin, 0);
	(void) close (shin);
      }
    if (shout != 1)
      {
	(void) dup2 (shout, 1);
	(void) close (shout);
      }
    if (flags & RUN_COMBINED)
      (void) dup2 (1, 2);
    else if (sherr != 2)
      {
	(void) dup2 (sherr, 2);
	(void) close (sherr);
      }

    /* Ignore signals while we're running this.  */
    old_sigint = signal (SIGINT, SIG_IGN);

    /* dup'ing is done.  try to run it now */
    rval = spawnvp ( P_WAIT, run_argv[0], run_argv);

    /* Restore signal handling.  */
    signal (SIGINT, old_sigint);

    /* restore the original file handles   */
    if (sain  != -1) {
      (void) dup2( sain, 0);	/* re-connect stdin  */
      (void) close( sain);
    }
    if (saout != -1) {
      (void) dup2( saout, 1);	/* re-connect stdout */
      (void) close( saout);
    }
    if (saerr != -1) {
      (void) dup2( saerr, 2);	/* re-connect stderr */
      (void) close( saerr);
    }

    /* Flush standard output and standard error, or otherwise we end
       up with strange interleavings of stuff called from CYGWIN
       vs. CMD.  */

    fflush (stderr);
    fflush (stdout);

    /* Recognize the return code for an interrupted subprocess.  */
    if (rval == CONTROL_C_EXIT)
        return 2;
    else
        return rval;		/* end, if all went coorect */

    /* error cases */
    /* cleanup the open file descriptors */
  out2:
    if (stout)
	(void) close (shout);
  out1:
    if (stin)
	(void) close (shin);

  out0:
    if (rerrno)
	errno = rerrno;
    return (status);
}

void
run_print (fp)
    FILE *fp;
{
    int i;

    for (i = 0; i < run_argc; i++)
    {
	(void) fprintf (fp, "'%s'", run_argv[i]);
	if (i != run_argc - 1)
	    (void) fprintf (fp, " ");
    }
}

static char *
requote (const char *cmd)
{
    char *requoted = xmalloc (strlen (cmd) + 1);
    char *p = requoted;

    strcpy (requoted, cmd);
    while ((p = strchr (p, '\'')) != NULL)
    {
        *p++ = '"';
    }

    return requoted;
}

FILE *
run_popen (cmd, mode)
    const char *cmd;
    const char *mode;
{
    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> run_popen(%s,%s)\n",
			(server_active) ? 'S' : ' ', cmd, mode);
#else
	(void) fprintf (stderr, "-> run_popen(%s,%s)\n", cmd, mode);
#endif
    if (noexec)
	return (NULL);

    /* If the command string uses single quotes, turn them into
       double quotes.  */
    {
        char *requoted = requote (cmd);
	/* Save and restore our file descriptors to work around
	   apparent bugs in _popen.  We are perhaps better off using
	   the win32 functions instead of _popen.  */
	int old_stdin = dup (STDIN_FILENO);
	int old_stdout = dup (STDOUT_FILENO);
	int old_stderr = dup (STDERR_FILENO);

	FILE *result = popen (requoted, mode);

	dup2 (old_stdin, STDIN_FILENO);
	dup2 (old_stdout, STDOUT_FILENO);
	dup2 (old_stderr, STDERR_FILENO);
	close (old_stdin);
	close (old_stdout);
	close (old_stderr);

	free (requoted);
	return result;
    }
}


/* Running children with pipes connected to them.  */

/* It's kind of ridiculous the hoops we're jumping through to get
   this working.  _pipe and dup2 and _spawnmumble work just fine, except
   that the child inherits a file descriptor for the writing end of the
   pipe, and thus will never receive end-of-file on it.  If you know of
   a better way to implement the piped_child function, please let me know. 
   
   You can apparently specify _O_NOINHERIT when you open a file, but there's
   apparently no fcntl function, so you can't change that bit on an existing
   file descriptor.  */

/* Given a handle, make an inheritable duplicate of it, and close
   the original.  */
static HANDLE
inheritable (HANDLE in)
{
    HANDLE copy;
    HANDLE self = GetCurrentProcess ();

    if (! DuplicateHandle (self, in, self, &copy, 
			   0, 1 /* fInherit */,
			   DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE))
        return INVALID_HANDLE_VALUE;

    return copy;
}


/* Initialize the SECURITY_ATTRIBUTES structure *LPSA.  Set its
   bInheritHandle flag according to INHERIT.  */
static void
init_sa (LPSECURITY_ATTRIBUTES lpsa, BOOL inherit)
{
  lpsa->nLength = sizeof(*lpsa);
  lpsa->bInheritHandle = inherit;
  lpsa->lpSecurityDescriptor = NULL;
}


enum inherit_pipe { inherit_reading, inherit_writing };

/* Create a pipe.  Set READWRITE[0] to its reading end, and 
   READWRITE[1] to its writing end.  If END is inherit_reading,
   make the only the handle for the pipe's reading end inheritable.
   If END is inherit_writing, make only the handle for the pipe's
   writing end inheritable.  Return 0 if we succeed, -1 if we fail.

   Why does inheritability matter?  Consider the case of a
   pipe carrying data from the parent process to the child
   process.  The child wants to read data from the parent until
   it reaches the EOF.  Now, the only way to send an EOF on a pipe
   is to close all the handles to its writing end.  Obviously, the 
   parent has a handle to the writing end when it creates the child.
   If the child inherits this handle, then it will never close it
   (the child has no idea it's inherited it), and will thus never
   receive an EOF on the pipe because it's holding a handle
   to it.
   
   In Unix, the child process closes the pipe ends before it execs.
   In Windows NT, you create the pipe with uninheritable handles, and then use
   DuplicateHandle to make the appropriate ends inheritable.  */

static int
my_pipe (HANDLE *readwrite, enum inherit_pipe end)
{
    HANDLE read, write;
    SECURITY_ATTRIBUTES sa;

    init_sa (&sa, 0);
    if (! CreatePipe (&read, &write, &sa, 1 << 13))
    {
        errno = EMFILE;
        return -1;
    }
    if (end == inherit_reading)
        read = inheritable (read);
    else
        write = inheritable (write);

    if (read == INVALID_HANDLE_VALUE
        || write == INVALID_HANDLE_VALUE)
    {
        CloseHandle (read);
	CloseHandle (write);
	errno = EMFILE;
	return -1;
    }

    readwrite[0] = read;
    readwrite[1] = write;

    return 0;
}


/* Initialize the STARTUPINFO structure *LPSI.  */
static void
init_si (LPSTARTUPINFO lpsi)
{
  memset (lpsi, 0, sizeof (*lpsi));
  lpsi->cb = sizeof(*lpsi);
  lpsi->lpReserved = NULL;
  lpsi->lpTitle = NULL;
  lpsi->lpReserved2 = NULL;
  lpsi->cbReserved2 = 0;
  lpsi->lpDesktop = NULL;
  lpsi->dwFlags = 0;
}


/* Create a child process running COMMAND with IN as its standard input,
   and OUT as its standard output.  Return a handle to the child, or
   INVALID_HANDLE_VALUE.  */
static int
start_child (char *command, HANDLE in, HANDLE out)
{
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  BOOL status;

  /* The STARTUPINFO structure can specify handles to pass to the
     child as its standard input, output, and error.  */
  init_si (&si);
  si.hStdInput = in;
  si.hStdOutput = out;
  si.hStdError  = (HANDLE) _get_osfhandle (2);
  si.dwFlags = STARTF_USESTDHANDLES;

  status = CreateProcess ((LPCTSTR) NULL,
                          (LPTSTR) command,
		          (LPSECURITY_ATTRIBUTES) NULL, /* lpsaProcess */
		          (LPSECURITY_ATTRIBUTES) NULL, /* lpsaThread */
		          TRUE, /* fInheritHandles */
		          0,    /* fdwCreate */
		          (LPVOID) 0, /* lpvEnvironment */
		          (LPCTSTR) 0, /* lpszCurDir */
		          &si,  /* lpsiStartInfo */
		          &pi); /* lppiProcInfo */

  if (! status)
  {
      DWORD error_code = GetLastError ();
      switch (error_code)
      {
      case ERROR_NOT_ENOUGH_MEMORY:
      case ERROR_OUTOFMEMORY:
          errno = ENOMEM; break;
      case ERROR_BAD_EXE_FORMAT:
          errno = ENOEXEC; break;
      case ERROR_ACCESS_DENIED:
          errno = EACCES; break;
      case ERROR_NOT_READY:
      case ERROR_FILE_NOT_FOUND:
      case ERROR_PATH_NOT_FOUND:
      default:
          errno = ENOENT; break;
      }
      return (int) INVALID_HANDLE_VALUE;
  }

  /* The _spawn and _cwait functions in the C runtime library
     seem to operate on raw NT handles, not PID's.  Odd, but we'll
     deal.  */
  return (int) pi.hProcess;
}


/* Given an array of arguments that one might pass to spawnv,
   construct a command line that one might pass to CreateProcess.
   Try to quote things appropriately.  */
static char *
build_command (char **argv)
{
    int len;

    /* Compute the total length the command will have.  */
    {
        int i;

	len = 0;
        for (i = 0; argv[i]; i++)
	{
	    char *p;

	    len += 2;  /* for the double quotes */

	    for (p = argv[i]; *p; p++)
	    {
	        if (*p == '"')
		    len += 2;
		else
		    len++;
	    }
	    len++;  /* for the space or the '\0'  */
	}
    }

    {
	/* The + 10 is in case len is 0.  */
        char *command = (char *) malloc (len + 10);
	int i;
	char *p;

	if (! command)
	{
	    errno = ENOMEM;
	    return command;
	}

	p = command;
        *p = '\0';
	/* copy each element of argv to command, putting each command
	   in double quotes, and backslashing any quotes that appear
	   within an argument.  */
	for (i = 0; argv[i]; i++)
	{
	    char *a;
	    *p++ = '"';
	    for (a = argv[i]; *a; a++)
	    {
	        if (*a == '"')
		    *p++ = '\\', *p++ = '"';
		else
		    *p++ = *a;
	    }
	    *p++ = '"';
	    *p++ = ' ';
	}
	if (p > command)
	    p[-1] = '\0';

        return command;
    }
}


/* Create an asynchronous child process executing ARGV,
   with its standard input and output connected to the 
   parent with pipes.  Set *TO to the file descriptor on
   which one writes data for the child; set *FROM to
   the file descriptor from which one reads data from the child.
   Return the handle of the child process (this is what
   _cwait and waitpid expect).  */
int
piped_child (char **argv, int *to, int *from)
{
  int child;
  HANDLE pipein[2], pipeout[2];
  char *command;

  /* Turn argv into a form acceptable to CreateProcess.  */
  command = build_command (argv);
  if (! command)
      return -1;

  /* Create pipes for communicating with child.  Arrange for
     the child not to inherit the ends it won't use.  */
  if (my_pipe (pipein, inherit_reading) == -1
      || my_pipe (pipeout, inherit_writing) == -1)
      return -1;  

  child = start_child (command, pipein[0], pipeout[1]);
  free (command);
  if (child == (int) INVALID_HANDLE_VALUE)
      return -1;

  /* Close the pipe ends the parent doesn't use.  */
  CloseHandle (pipein[0]);
  CloseHandle (pipeout[1]);

  /* Given the pipe handles, turn them into file descriptors for
     use by the caller.  */
  if ((*to      = _open_osfhandle ((long) pipein[1],  _O_BINARY)) == -1
      || (*from = _open_osfhandle ((long) pipeout[0], _O_BINARY)) == -1)
      return -1;

  return child;
}

/*
 * dir = 0 : main proc writes to new proc, which writes to oldfd
 * dir = 1 : main proc reads from new proc, which reads from oldfd
 *
 * Returns: a file descriptor.  On failure (e.g., the exec fails),
 * then filter_stream_through_program() complains and dies.
 */

int
filter_stream_through_program (oldfd, dir, prog, pidp)
     int oldfd, dir;
     char **prog;
     pid_t *pidp;
{
    HANDLE pipe[2];
    char *command;
    int child;
    HANDLE oldfd_handle;
    HANDLE newfd_handle;
    int newfd;

    /* Get the OS handle associated with oldfd, to be passed to the child.  */
    if ((oldfd_handle = (HANDLE) _get_osfhandle (oldfd)) < 0)
	error (1, errno, "cannot _get_osfhandle");

    if (dir)
    {
        /* insert child before parent, pipe goes child->parent.  */
	if (my_pipe (pipe, inherit_writing) == -1)
	    error (1, errno, "cannot my_pipe");
	if ((command = build_command (prog)) == NULL)
	    error (1, errno, "cannot build_command");
	child = start_child (command, oldfd_handle, pipe[1]);
	free (command);
	if (child == (int) INVALID_HANDLE_VALUE)
	    error (1, errno, "cannot start_child");
	close (oldfd);
	CloseHandle (pipe[1]);
	newfd_handle = pipe[0];
    }
    else
    {
        /* insert child after parent, pipe goes parent->child.  */
	if (my_pipe (pipe, inherit_reading) == -1)
	    error (1, errno, "cannot my_pipe");
	if ((command = build_command (prog)) == NULL)
	    error (1, errno, "cannot build_command");
	child = start_child (command, pipe[0], oldfd_handle);
	free (command);
	if (child == (int) INVALID_HANDLE_VALUE)
	    error (1, errno, "cannot start_child");
	close (oldfd);
	CloseHandle (pipe[0]);
	newfd_handle = pipe[1];
    }

    if ((newfd = _open_osfhandle ((long) newfd_handle, _O_BINARY)) == -1)
        error (1, errno, "cannot _open_osfhandle");

    if (pidp)
	*pidp = child;
    return newfd;    
}


/* Arrange for the file descriptor FD to not be inherited by child
   processes.  At the moment, CVS uses this function only on pipes
   returned by piped_child, and our implementation of piped_child
   takes care of setting the file handles' inheritability, so this
   can be a no-op.  */
void
close_on_exec (int fd)
{
}
