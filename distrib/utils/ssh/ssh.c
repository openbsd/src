/*	$OpenBSD: ssh.c,v 1.2 2000/03/01 22:10:12 todd Exp $	*/
/*	$NetBSD: ssh.c,v 1.1.1.1 1995/10/08 23:08:46 gwr Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon W. Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Small Shell - Nothing fancy.  Just runs programs.
 * The RAMDISK root uses this to save space.
 */

#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/wait.h>

/* XXX - SunOS hacks... */
#ifndef	WCOREDUMP
#define	WCOREDUMP(x) ((x) & 0200)
#endif

#ifndef	__P
#ifdef	__STDC__
#define __P(x) x
#else	/* STDC */
#define	__P(x) ()
#endif	/* STDC */
#endif	/* __P */

extern char *optarg;
extern int optind, opterr;

#define MAXLINE 256
#define MAXARGS 32

#define	MAXPATH 256
char cur_path[MAXPATH] = "PATH=/bin:/usr/bin";

char rc_name[] = ".sshrc";
char *prompt = "ssh: ";

int eflag;	/* exit on cmd failure */
int iflag;	/* interactive mode (catch interrupts) */
int sflag;	/* read from stdin (ignore file arg) */
int xflag;	/* execution trace */

/* Command file: name, line number, arg count, arg vector */
char *cf_name;
int cf_line;
int cf_argc;
char **cf_argv;

int def_omode = 0666;
int run_bg_pid;

jmp_buf next_cmd;

void catchsig __P((int sig));
void child_newfd __P((int setfd, char *file, int otype));
int find_in_path __P((char *cmd, char *filebuf));
void print_termsig __P((FILE *fp, int cstat));
int runfile __P((FILE *fp));


main(argc, argv)
	int argc;
	char **argv;
{
	struct sigaction sa;
	FILE *cfp;		/* command file ptr */
	int c, sig;
	int error = 0;

	while ((c = getopt(argc, argv, "eisx")) != -1) {
		switch (c) {
		case 'e':
			eflag++;
			break;
		case 'i':
			eflag++;
			break;
		case 's':
			sflag++;
			break;
		case 'x':
			xflag++;
			break;
		case '?':
			error++;
			break;
		}
	}
	if (error) {
		fprintf(stderr, "usage:  ssh [-eisx] [cmd_file [...]]\n");
		exit(1);
	}
	cf_argc = argc - optind;
	cf_argv = &argv[optind];

	/* If this is a login shell, run the rc file. */
	if (argv[0] && argv[0][0] == '-') {
		cf_line = 0;
		cf_name = rc_name;
		if ((cfp = fopen(cf_name, "r")) != NULL) {
			error = runfile(cfp);
			fclose(cfp);
		}
	}

	/* If no file names, read commands from stdin. */
	if (cf_argc == 0)
		sflag++;
	/* If stdin is a tty, be interactive. */
	if (sflag && isatty(fileno(stdin)))
		iflag++;

	/* Maybe run a command file... */
	if (!sflag && cf_argc) {
		cf_line = 0;
		cf_name = cf_argv[0];
		cfp = fopen(cf_name, "r");
		if (cfp == NULL) {
			perror(cf_name);
			exit(1);
		}
		error = runfile(cfp);
		fclose(cfp);
		exit(error);
	}

	/* Read commands from stdin. */
	cf_line = 0;
	cf_name = "(stdin)";
	if (iflag) {
		eflag = 0;	/* don't kill shell on error. */
		sig = setjmp(next_cmd);
		if (sig == 0) {
			/* Initialization... */
			sa.sa_handler = catchsig;
			sa.sa_flags = 0;
			sigemptyset(&sa.sa_mask);
			sigaction(SIGINT,  &sa, NULL);
			sigaction(SIGQUIT, &sa, NULL);
			sigaction(SIGTERM, &sa, NULL);
		} else {
			/* Got here via longjmp. */
			fprintf(stderr, " signal %d\n", sig);
			sigemptyset(&sa.sa_mask);
			sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);
		}
	}
	error = runfile(stdin);
	exit (error);
}

void
catchsig(sig)
	int sig;
{
	longjmp(next_cmd, sig);
}

/*
 * Run command from the passed stdio file pointer.
 * Returns exit status.
 */
int
runfile(cfp)
	FILE *cfp;
{
	char ibuf[MAXLINE];
	char *argv[MAXARGS];
	char *p;
	int i, argc, exitcode, cpid, cstat;

	/* The command loop. */
	exitcode = 0;
	for (;;) {
		if (iflag) {
			fprintf(stderr, prompt);
			fflush(stderr);
		}

		if ((fgets(ibuf, sizeof(ibuf), cfp)) == NULL)
			break;
		cf_line++;

		argc = 0;
		p = ibuf;

		while (argc < MAXARGS-1) {
			/* skip blanks or tabs */
			while ((*p == ' ') || (*p == '\t')) {
			next_token:
				*p++ = '\0';
			}
			/* end of line? */
			if ((*p == '\n') || (*p == '#')) {
				*p = '\0';
				break;	/* to eol */
			}
			if (*p == '\0')
				break;
			/* save start of token */
			argv[argc++] = p;
			/* find end of token */
			while (*p) {
				if ((*p == '\n') || (*p == '#')) {
					*p = '\0';
					goto eol;
				}
				if ((*p == ' ') || (*p == '\t'))
					goto next_token;
				p++;
			}
		}
	eol:

		if (argc > 0) {
			if (xflag) {
				fprintf(stderr, "x");
				for (i = 0; i < argc; i++) {
					fprintf(stderr, " %s", argv[i]);
				}
				fprintf(stderr, "\n");
			}
			argv[argc] = NULL;
			exitcode = cmd_eval(argc, argv);
		}

		/* Collect children. */
		while ((cpid = waitpid(0, &cstat, WNOHANG)) > 0) {
			if (iflag) {
				fprintf(stderr, "[%d] ", cpid);
				if (WTERMSIG(cstat)) {
					print_termsig(stderr, cstat);
				} else {
					fprintf(stderr, "Exited, status %d\n",
							WEXITSTATUS(cstat));
				}
			}
		}

		if (exitcode && eflag)
			break;
	}
	/* return status of last command */
	return (exitcode);
}


/****************************************************************
 *  Table of buildin commands
 *  for cmd_eval() to search...
 ****************************************************************/

struct cmd {
	char *name;
	int (*func)();
	char *help;
};
struct cmd cmd_table[];

/*
 * Evaluate a command named as argv[0]
 * with arguments argv[1],argv[2]...
 * Returns exit status.
 */
int
cmd_eval(argc, argv)
	int argc;
	char **argv;
{
	struct cmd *cp;

	/*
	 * Do linear search for a builtin command.
	 * Performance does not matter here.
	 */
	for (cp = cmd_table; cp->name; cp++) {
		if (!strcmp(cp->name, argv[0])) {
			/* Pass only args to builtin. */
			--argc; argv++;
			return (cp->func(argc, argv));
		}
	}

	/*
	 * If no matching builtin, let "run ..."
	 * have a chance to try an external.
	 */
	return (cmd_run(argc, argv));
}

/*****************************************************************
 *  Here are the actual commands.  For these,
 *  the command name has been skipped, so
 *  argv[0] is the first arg (if any args).
 *  All return an exit status.
 ****************************************************************/

char help_cd[] = "cd [dir]";

int
cmd_cd(argc, argv)
	int argc;
	char **argv;
{
	char *dir;
	int err;

	if (argc > 0)
		dir = argv[0];
	else {
		dir = getenv("HOME");
		if (dir == NULL)
			dir = "/";
	}
	if (chdir(dir)) {
		perror(dir);
		return (1);
	}
	return(0);
}

char help_exit[] = "exit [n]";

int
cmd_exit(argc, argv)
	int argc;
	char **argv;
{
	int val = 0;

	if (argc > 0)
		val = atoi(argv[0]);
	exit(val);
}

char help_help[] = "help [command]";

int
cmd_help(argc, argv)
	int argc;
	char **argv;
{
	struct cmd *cp;

	if (argc > 0) {
		for (cp = cmd_table; cp->name; cp++) {
			if (!strcmp(cp->name, argv[0])) {
				printf("usage:  %s\n", cp->help);
				return (0);
			}
		}
		printf("%s: no such command\n", argv[0]);
	}

	printf("Builtin commands: ");
	for (cp = cmd_table; cp->name; cp++) {
		printf(" %s", cp->name);
	}
	printf("\nFor specific usage:  help [command]\n");
	return (0);
}

char help_path[] = "path [dir1:dir2:...]";

int
cmd_path(argc, argv)
	int argc;
	char **argv;
{
	int i;

	if (argc <= 0) {
		printf("%s\n", cur_path);
		return(0);
	}

	strncpy(cur_path+5, argv[0], MAXPATH-6);
	putenv(cur_path);

	return (0);
}

/*****************************************************************
 *  The "run" command is the big one.
 *  Does fork/exec/wait, redirection...
 *  Returns exit status of child
 *  (or zero for a background job)
 ****************************************************************/

char help_run[] = "\
run [-bg] [-i ifile] [-o ofile] [-e efile] program [args...]\n\
or simply:  program [args...]";

int
cmd_run(argc, argv)
	int argc;
	char **argv;
{
	struct sigaction sa;
	int pid, err, cstat, fd;
	char file[MAXPATHLEN];
	int background;
	char *opt, *ifile, *ofile, *efile;
	extern char **environ;

	/*
	 * Parse options:
	 * -b  : background
	 * -i  : input file
	 * -o  : output file
	 * -e  : error file
	 */
	background = 0;
	ifile = ofile = efile = NULL;
	while ((argc > 0) && (argv[0][0] == '-')) {
		opt = argv[0];
		--argc; argv++;
		switch (opt[1]) {
		case 'b':
			background++;
			break;
		case 'i':
			ifile = argv[0];
			goto shift;
		case 'o':
			ofile = argv[0];
			goto shift;
		case 'e':
			efile = argv[0];
			goto shift;
		default:
			fprintf(stderr, "run %s: bad option\n", opt);
			return (1);
		shift:
			--argc; argv++;
		}
	}

	if (argc <= 0) {
		fprintf(stderr, "%s:%d run: missing command\n",
				cf_name, cf_line);
		return (1);
	}

	/* Commands containing '/' get no path search. */
	if (strchr(argv[0], '/')) {
		strncpy(file, argv[0], sizeof(file)-1);
		if (access(file, X_OK)) {
			perror(file);
			return (1);
		}
	} else {
		if (find_in_path(argv[0], file)) {
			fprintf(stderr, "%s: command not found\n", argv[0]);
			return (1);
		}
	}

	pid = fork();
	if (pid == 0) {
		/* child runs this */
		/* handle redirection options... */
		if (ifile)
			child_newfd(0, ifile, O_RDONLY);
		if (ofile)
			child_newfd(1, ofile, O_WRONLY|O_CREAT);
		if (efile)
			child_newfd(2, efile, O_WRONLY|O_CREAT);
		if (background) {
			/* Ignore SIGINT, SIGQUIT */
			sa.sa_handler = SIG_IGN;
			sa.sa_flags = 0;
			sigemptyset(&sa.sa_mask);
			sigaction(SIGINT,  &sa, NULL);
			sigaction(SIGQUIT, &sa, NULL);
		}
		err = execve(file, argv, environ);
		perror(argv[0]);
		return (1);
	}
	/* parent */
	/* Handle background option... */
	if (background) {
		fprintf(stderr, "[%d]\n", pid);
		run_bg_pid = pid;
		return (0);
	}
	if (waitpid(pid, &cstat, 0) < 0) {
		perror("waitpid");
		return (1);
	}
	if (WTERMSIG(cstat)) {
		print_termsig(stderr, cstat);
	}
	return (WEXITSTATUS(cstat));
}

/*****************************************************************
 *  table of builtin commands
 ****************************************************************/
struct cmd cmd_table[] = {
	{ "cd",   cmd_cd,   help_cd },
	{ "exit", cmd_exit, help_exit },
	{ "help", cmd_help, help_help },
	{ "path", cmd_path, help_path },
	{ "run",  cmd_run,  help_run },
	{ 0 },
};

/*****************************************************************
 *  helper functions for the "run" command
 ****************************************************************/

int
find_in_path(cmd, filebuf)
	char *cmd;
	char *filebuf;
{
	char *dirp, *endp, *bufp;	/* dir, end */

	dirp = cur_path + 5;
	while (*dirp) {
		endp = dirp;
		bufp = filebuf;
		while (*endp && (*endp != ':'))
			*bufp++ = *endp++;
		*bufp++ = '/';
		strcpy(bufp, cmd);
		if (access(filebuf, X_OK) == 0)
			return (0);
		if (*endp == ':')
			endp++;
		dirp = endp;	/* next dir */
	}
	return (-1);
}

/*
 * Set the file descriptor SETFD to FILE,
 * which was opened with OTYPE and MODE.
 */
void
child_newfd(setfd, file, otype)
	int setfd;	/* what to set (i.e. 0,1,2) */
	char *file;
	int otype;	/* O_RDONLY, etc. */
{
	int newfd;

	close(setfd);
	if ((newfd = open(file, otype, def_omode)) < 0) {
		perror(file);
		exit(1);
	}
	if (newfd != setfd) {
		dup2(newfd, setfd);
		close(newfd);
	}
}

void
print_termsig(fp, cstat)
	FILE *fp;
	int cstat;
{
	fprintf(fp, "Terminated, signal %d",
			WTERMSIG(cstat));
	if (WCOREDUMP(cstat))
		fprintf(fp, " (core dumped)");
	fprintf(fp, "\n");
}
