/*	$OpenBSD: run.c,v 1.5 1998/06/03 16:20:35 deraadt Exp $	*/

/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/*  run, runv, runp, runvp --  execute process and wait for it to exit
 *
 *  Usage:
 *	i = run (file, arg1, arg2, ..., argn, 0);
 *	i = runv (file, arglist);
 *	i = runp (file, arg1, arg2, ..., argn, 0);
 *	i = runvp (file, arglist);
 *	i = runio (argv, in, out, err);
 *
 *  Run, runv, runp and runvp have argument lists exactly like the
 *  corresponding routines, execl, execv, execlp, execvp.  The run
 *  routines perform a fork, then:
 *  IN THE NEW PROCESS, an execl[p] or execv[p] is performed with the
 *  specified arguments.  The process returns with a -1 code if the
 *  exec was not successful.
 *  IN THE PARENT PROCESS, the signals SIGQUIT and SIGINT are disabled,
 *  the process waits until the newly forked process exits, the
 *  signals are restored to their original status, and the return
 *  status of the process is analyzed.
 *  All run routines return:  -1 if the exec failed or if the child was
 *  terminated abnormally; otherwise, the exit code of the child is
 *  returned.
 *
 **********************************************************************
 * HISTORY
 * 
 * 23-Sep-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Merged old runv and runvp modules.
 *
 * 22-Nov-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added check and kill if child process was stopped.
 *
 * 30-Apr-85  Steven Shafer (sas) at Carnegie-Mellon University
 *	Adapted for 4.2 BSD UNIX:  Conforms to new signals and wait.
 *
 * 15-July-82 Mike Accetta (mja) and Neal Friedman (naf)
 *				  at Carnegie-Mellon University
 *	Added a return(-1) if vfork fails.  This should only happen
 *	if there are no more processes available.
 *
 * 28-Jan-80  Steven Shafer (sas) at Carnegie-Mellon University
 *	Added setuid and setgid for system programs' use.
 *
 * 21-Jan-80  Steven Shafer (sas) at Carnegie-Mellon University
 *	Changed fork to vfork.
 *
 * 20-Nov-79  Steven Shafer (sas) at Carnegie-Mellon University
 *	Created for VAX.  The proper way to fork-and-execute a system
 *	program is now by "runvp" or "runp", with the program name
 *	(rather than an absolute pathname) as the first argument;
 *	that way, the "PATH" variable in the environment does the right
 *	thing.  Too bad execvp and execlp (hence runvp and runp) don't
 *	accept a pathlist as an explicit argument.
 *
 **********************************************************************
 */

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "supcdefs.h"
#include "supextern.h"

#ifndef __STDC__
#ifndef const
#define const
#endif
#endif

static int dorun __P((char *, char **, int));

int
#ifdef __STDC__
run(char *name, ...)
#else
run(va_alist)
va_dcl
#endif
{
	int val;
	va_list ap;
	char **argv;
#ifdef __STDC__
	va_start(ap, name);
#else
	char *name;

	va_start(ap);
	name = va_arg(ap, char *);
#endif

	argv = va_arg(ap, char **);
	val = runv (name, argv);
	va_end(ap);
	return(val);
}

int runv (name,argv)
char *name,**argv;
{
	return (dorun (name, argv, 0));
}

int
#ifdef __STDC__
runp(char *name, ...)
#else
runp (va_alist)
va_dcl
#endif
{
	int val;
	va_list ap;
	char **argv;
#ifdef __STDC__
	va_start(ap, name);
#else
	char *name;

	va_start(ap);
	name = va_arg(ap, char *);
#endif

	argv = va_arg(ap, char **);
	val = runvp (name, argv);
	va_end(ap);
	return (val);
}

int runvp (name,argv)
char *name,**argv;
{
	return (dorun (name, argv, 1));
}

static
int dorun (name,argv,usepath)
char *name,**argv;
int usepath;
{
	int wpid;
	register int pid;
	struct sigaction ignoresig,intsig,quitsig;
	int status;

	if ((pid = vfork()) == -1)
		return(-1);	/* no more process's, so exit with error */

	if (pid == 0) {			/* child process */
		setegid (getgid());
		setgid (getgid());
		seteuid (getuid());
		setuid (getuid());
		if (usepath)
		    execvp(name,argv);
		else
		    execv(name,argv);
		fprintf (stderr,"run: can't exec %s\n",name);
		_exit (0377);
	}

	memset(&ignoresig, 0, sizeof ignoresig);
	ignoresig.sa_handler = SIG_IGN;	/* ignore INT and QUIT signals */
	sigemptyset(&ignoresig.sa_mask);
	ignoresig.sa_flags = 0;
	sigaction (SIGINT,&ignoresig,&intsig);
	sigaction (SIGQUIT,&ignoresig,&quitsig);
	do {
		wpid = wait3 (&status, WUNTRACED, 0);
		if (WIFSTOPPED (status)) {
		    kill (0,SIGTSTP);
		    wpid = 0;
		}
	} while (wpid != pid && wpid != -1);
	sigaction (SIGINT,&intsig,0);	/* restore signals */
	sigaction (SIGQUIT,&quitsig,0);

	if (WIFSIGNALED (status) || WEXITSTATUS(status) == 0377)
		return (-1);

	return (WEXITSTATUS(status));
}

/*
 * Like system(3), but with an argument list and explicit redirections
 * that does not use the shell
 */
int
runio(argv, infile, outfile, errfile)
	char *const argv[];
	const char *infile;
	const char *outfile;
	const char *errfile;
{
	int	fd;
	pid_t	pid;
	int	status;

	switch ((pid = fork())) {
	case -1:
		return -1;

	case 0:
		if (infile) {
			(void) close(0);
			if ((fd = open(infile, O_RDONLY)) == -1)
				exit(1);
			if (fd != 0)
				(void) dup2(fd, 0);
		}

		if (outfile) {
			(void) close(1);
			if ((fd = open(outfile, O_RDWR|O_CREAT|O_TRUNC,
				       0666)) == -1)
				exit(1);
			if (fd != 1)
				(void) dup2(fd, 1);
		}

		if (errfile) {
			(void) close(2);
			if ((fd = open(errfile, O_RDWR|O_CREAT|O_TRUNC,
				       0666)) == -1)
				exit(1);
			if (fd != 2)
				(void) dup2(fd, 2);
		}

		execvp(argv[0], argv);
		exit(1);
		/*NOTREACHED*/
		return 0;
	
	default:
		if (waitpid(pid, &status, 0) == -1)
			return -1;
		return status;
	}
}
