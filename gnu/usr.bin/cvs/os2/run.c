/* run.c --- routines for executing subprocesses under OS/2.
   
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

#include "os2inc.h"

#include <process.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <io.h>

#define STDIN       0
#define STDOUT      1
#define STDERR      2

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
 * Finally, call run_exec() to execute the program with the specified
 * arguments. 
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

    free (run_prog)
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
    size_t s_len = strlen (s);
    char *copy = xmalloc (s_len + 3);
    char *scan = copy;

    *scan++ = '"';
    strcpy (scan, s);
    scan += s_len;
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
        /* not post-incremented on purpose! */
	run_argv[run_argc] = (char *) 0;
}

int
run_exec (stin, stout, sterr, flags)
    char *stin;
    char *stout;
    char *sterr;
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

    /* Recognize the return code for a failed subprocess.  */
    if (rval == -1)
        return 2;
    else
        return rval;		/* return child's exit status */

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
	FILE *result = popen (requoted, mode);
	free (requoted);
	return result;
    }
}


/* Running children with pipes connected to them.  */

/* Create a pipe.  Set READWRITE[0] to its reading end, and 
   READWRITE[1] to its writing end.  */

static int
my_pipe (int *readwrite)
{
    fprintf (stderr,
             "Error: my_pipe() is unimplemented.\n");
    exit (1);
}


/* Create a child process running COMMAND with IN as its standard input,
   and OUT as its standard output.  Return a handle to the child, or
   INVALID_HANDLE_VALUE.  */
static int
start_child (char *command, int in, int out)
{
    fprintf (stderr,
             "Error: start_child() is unimplemented.\n");
    exit (1);
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
	}
        len++;  /* for the space or the '\0'  */
    }

    {
        char *command = (char *) malloc (len);
	int i;
	char *p;

	if (! command)
	{
	    errno = ENOMEM;
	    return command;
	}

	p = command;
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
    fprintf (stderr,
             "Error: piped_child() is unimplemented.\n");
    exit (1);
}

/*
 * dir = 0 : main proc writes to new proc, which writes to oldfd
 * dir = 1 : main proc reads from new proc, which reads from oldfd
 *
 * If this returns at all, then it was successful and the return value
 * is a file descriptor; else it errors and exits.
 */
int
filter_stream_through_program (int oldfd, int dir,
			   char **prog, int *pidp)
{
	int newfd;  /* Gets set to one end of the pipe and returned. */
    HFILE from, to;
	HFILE Old0 = -1, Old1 = -1, Old2 = -1, Tmp;

    if (DosCreatePipe (&from, &to, 4096))
        return FALSE;

    /* Save std{in,out,err} */
    DosDupHandle (STDIN, &Old0);
    DosSetFHState (Old1, OPEN_FLAGS_NOINHERIT);
    DosDupHandle (STDOUT, &Old1);
    DosSetFHState (Old2, OPEN_FLAGS_NOINHERIT);
    DosDupHandle (STDERR, &Old2);
    DosSetFHState (Old2, OPEN_FLAGS_NOINHERIT);

    /* Redirect std{in,out,err} */
	if (dir)    /* Who goes where? */
	{
		Tmp = STDIN;
		DosDupHandle (oldfd, &Tmp);
		Tmp = STDOUT;
		DosDupHandle (to, &Tmp);
		Tmp = STDERR;
		DosDupHandle (to, &Tmp);

		newfd = from;
		_setmode (newfd, O_BINARY);

		DosClose (oldfd);
		DosClose (to);
		DosSetFHState (from, OPEN_FLAGS_NOINHERIT);
	}
	else
	{
		Tmp = STDIN;
		DosDupHandle (from, &Tmp);
		Tmp = STDOUT;
		DosDupHandle (oldfd, &Tmp);
		Tmp = STDERR;
		DosDupHandle (oldfd, &Tmp);

		newfd = to;
		_setmode (newfd, O_BINARY);

		DosClose (oldfd);
		DosClose (from);
		DosSetFHState (to, OPEN_FLAGS_NOINHERIT);
	}

    /* Spawn we now our hoary brood. */
	*pidp = spawnvp (P_NOWAIT, prog[0], prog);

    /* Restore std{in,out,err} */
    Tmp = STDIN;
    DosDupHandle (Old0, &Tmp);
    DosClose (Old0);
    Tmp = STDOUT;
    DosDupHandle (Old1, &Tmp);
    DosClose (Old1);
    Tmp = STDERR;
    DosDupHandle (Old2, &Tmp);
    DosClose (Old2);

    if(*pidp < 0) 
    {
        DosClose (from);
        DosClose (to);
        error (1, 0, "error spawning %s", prog[0]);
    }

    return newfd;
}


int
pipe (int *filedesc)
{
  /* todo: actually, we can use DosCreatePipe().  Fix this. */
  fprintf (stderr,
           "Error: pipe() should not have been called in client.\n");
  exit (1);
}


void
close_on_exec (int fd)
{
  /* Just does nothing for now... */

  /* Actually, we probably *can* implement this one.  Let's see... */
  /* Nope.  OS/2 has <fcntl.h>, but no fcntl() !  Wow. */
  /* Well, I'll leave this stuff in for future reference. */
}


/* Actually, we #define sleep() in config.h now. */
#ifndef sleep
unsigned int
sleep (unsigned int seconds)
{
  /* I don't want to interfere with alarm signals, so I'm going to do
     this the nasty way. */

  time_t base;
  time_t tick;
  int i;

  /* Init. */
  time (&base);
  time (&tick);

  /* Loop until time has passed. */
  while (difftime (tick, base) < seconds)
    {
      /* This might be more civilized than calling time over and over
         again. */
      for (i = 0; i < 10000; i++)
        ;
      time (&tick);
    }

  return 0;
}
#endif /* sleep */
