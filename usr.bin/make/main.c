/*	$OpenBSD: main.c,v 1.123 2019/04/22 18:32:09 espie Exp $ */
/*	$NetBSD: main.c,v 1.34 1997/03/24 20:56:36 gwr Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>	/* MACHINE MACHINE_ARCH */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "defines.h"
#include "var.h"
#include "lowparse.h"
#include "parse.h"
#include "parsevar.h"
#include "dir.h"
#include "direxpand.h"
#include "error.h"
#include "pathnames.h"
#include "init.h"
#include "job.h"
#include "compat.h"
#include "targ.h"
#include "suff.h"
#include "str.h"
#include "main.h"
#include "lst.h"
#include "memory.h"
#include "make.h"
#include "dump.h"

#define MAKEFLAGS	".MAKEFLAGS"

static LIST		to_create; 	/* Targets to be made */
Lst create = &to_create;
bool 		allPrecious;	/* .PRECIOUS given on line by itself */

static bool		noBuiltins;	/* -r flag */
static LIST		makefiles;	/* ordered list of makefiles to read */
static LIST		varstoprint;	/* list of variables to print */
int			maxJobs;	/* -j argument */
bool 		compatMake;	/* -B argument */
static bool		forceJobs = false;
int 		debug;		/* -d flag */
bool 		noExecute;	/* -n flag */
bool 		keepgoing;	/* -k flag */
bool 		queryFlag;	/* -q flag */
bool 		touchFlag;	/* -t flag */
bool 		ignoreErrors;	/* -i flag */
bool 		beSilent;	/* -s flag */
bool		dumpData;	/* -p flag */

struct dirs {
	char *current;
	char *object;
};

static void MainParseArgs(int, char **);
static void add_dirpath(Lst, const char *);
static void usage(void);
static void posixParseOptLetter(int);
static void record_option(int, const char *);

static char *figure_out_MACHINE(void);
static char *figure_out_MACHINE_ARCH(void);
static char *figure_out_MACHINE_CPU(void);

static char *chdir_verify_path(const char *, struct dirs *);
static char *figure_out_CURDIR(void);
static void setup_CURDIR_OBJDIR(struct dirs *);

static void setup_VPATH(void);

static void read_all_make_rules(bool, bool, Lst, struct dirs *);
static void read_makefile_list(Lst, struct dirs *);
static bool ReadMakefile(void *, void *);

static void
record_option(int c, const char *arg)
{
    	char opt[3];

	opt[0] = '-';
	opt[1] = c;
	opt[2] = '\0';
	Var_Append(MAKEFLAGS, opt);
	if (arg != NULL)
		Var_Append(MAKEFLAGS, arg);
}

static void
posixParseOptLetter(int c)
{
	switch(c) {
	case 'B':
		compatMake = true;
		return;	/* XXX don't pass to submakes. */
	case 'S':
		keepgoing = false;
		break;
	case 'e':
		Var_setCheckEnvFirst(true);
		break;
	case 'i':
		ignoreErrors = true;
		break;
	case 'k':
		keepgoing = true;
		break;
	case 'n':
		noExecute = true;
		break;
	case 'p':
		dumpData = true;
		break;
	case 'q':
		queryFlag = true;
		/* Kind of nonsensical, wot? */
		break;
	case 'r':
		noBuiltins = true;
		break;
	case 's':
		beSilent = true;
		break;
	case 't':
		touchFlag = true;
		break;
	default:
	case '?':
		usage();
	}
	record_option(c, NULL);
}

/*-
 * MainParseArgs --
 *	Parse a given argument vector. Called from main() and from
 *	Main_ParseArgLine() when the .MAKEFLAGS target is used.
 *
 *	XXX: Deal with command line overriding .MAKEFLAGS in makefile
 *
 * Side Effects:
 *	Various global and local flags will be set depending on the flags
 *	given
 */
static void
MainParseArgs(int argc, char **argv)
{
	int c, optend;

#define OPTFLAGS "BC:D:I:SV:d:ef:ij:km:npqrst"
#define OPTLETTERS "BSiknpqrst"

	if (pledge("stdio rpath wpath cpath fattr proc exec", NULL) == -1)
		err(2, "pledge");

	optind = 1;	/* since we're called more than once */
	optreset = 1;
	optend = 0;
	while (optind < argc) {
		if (!optend && argv[optind][0] == '-') {
			if (argv[optind][1] == '\0')
				optind++;	/* ignore "-" */
			else if (argv[optind][1] == '-' &&
			    argv[optind][2] == '\0') {
				optind++;	/* ignore "--" */
				optend++;	/* "--" denotes end of flags */
			}
		}
		c = optend ? -1 : getopt(argc, argv, OPTFLAGS);
		switch (c) {
		case 'C':
			break;
		case 'D':
			Var_Set(optarg, "1");
			record_option(c, optarg);
			break;
		case 'I':
			Parse_AddIncludeDir(optarg);
			record_option(c, optarg);
			break;
		case 'V':
			Lst_AtEnd(&varstoprint, optarg);
			record_option(c, optarg);
			break;
		case 'd': {
			char *modules = optarg;

			for (; *modules; ++modules)
				switch (*modules) {
				case 'A':
					debug = ~0;
					break;
				case 'a':
					debug |= DEBUG_ARCH;
					break;
				case 'c':
					debug |= DEBUG_COND;
					break;
				case 'd':
					debug |= DEBUG_DIR;
					break;
				case 'D':
					debug |= DEBUG_DOUBLE;
					break;
				case 'e':
					debug |= DEBUG_EXPENSIVE;
					break;
				case 'f':
					debug |= DEBUG_FOR;
					break;
				case 'g':
					if (modules[1] == '1') {
						debug |= DEBUG_GRAPH1;
						++modules;
					}
					else if (modules[1] == '2') {
						debug |= DEBUG_GRAPH2;
						++modules;
					}
					break;
				case 'h':
					debug |= DEBUG_HELDJOBS;
					break;
				case 'j':
					debug |= DEBUG_JOB | DEBUG_KILL;
					break;
				case 'J':
					/* ignore */
					break;
				case 'k':
					debug |= DEBUG_KILL;
					break;
				case 'l':
					debug |= DEBUG_LOUD;
					break;
				case 'm':
					debug |= DEBUG_MAKE;
					break;
				case 'n':
					debug |= DEBUG_NAME_MATCHING;
					break;
				case 'p':
					debug |= DEBUG_PARALLEL;
					break;
				case 'q':
					debug |= DEBUG_QUICKDEATH;
					break;
				case 's':
					debug |= DEBUG_SUFF;
					break;
				case 't':
					debug |= DEBUG_TARG;
					break;
				case 'T':
					debug |= DEBUG_TARGGROUP;
					break;
				case 'v':
					debug |= DEBUG_VAR;
					break;
				default:
					(void)fprintf(stderr,
				"make: illegal argument to -d option -- %c\n",
					    *modules);
					usage();
				}
			record_option(c, optarg);
			break;
		}
		case 'f':
			Lst_AtEnd(&makefiles, optarg);
			break;
		case 'j': {
			const char *errstr;

			forceJobs = true;
			maxJobs = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr != NULL) {
				fprintf(stderr,
				    "make: illegal argument to -j option"
				    " -- %s -- %s\n", optarg, errstr);
				usage();
			}
			record_option(c, optarg);
			break;
		}
		case 'm':
			Dir_AddDir(systemIncludePath, optarg);
			record_option(c, optarg);
			break;
		case -1:
			/* Check for variable assignments and targets. */
			if (argv[optind] != NULL &&
			    !Parse_CmdlineVar(argv[optind])) {
				if (!*argv[optind])
					Punt("illegal (null) argument.");
				Lst_AtEnd(create, estrdup(argv[optind]));
			}
			optind++;	/* skip over non-option */
			break;
		default:
			posixParseOptLetter(c);
		}
	}
}

static void
MainParseChdir(int argc, char **argv)
{
	int c, optend, oldopterr;

	optind = 1;	/* since we're called more than once */
	optreset = 1;
	optend = 0;
	oldopterr = opterr;
	opterr = 0;
	while (optind < argc) {
		if (!optend && argv[optind][0] == '-') {
			if (argv[optind][1] == '\0')
				optind++;	/* ignore "-" */
			else if (argv[optind][1] == '-' &&
			    argv[optind][2] == '\0') {
				optind++;	/* ignore "--" */
				optend++;	/* "--" denotes end of flags */
			}
		}
		c = optend ? -1 : getopt(argc, argv, OPTFLAGS);
		switch (c) {
		case 'C':
			if (chdir(optarg) == -1)
				err(2, "chdir(%s)", optarg);
			break;
		case -1:
			optind++;	/* skip over non-option */
			break;
		default:
			break;
		}
	}
	opterr = oldopterr;
}

/*-
 * Main_ParseArgLine --
 *	Used by the parse module when a .MFLAGS or .MAKEFLAGS target
 *	is encountered and by main() when reading the .MAKEFLAGS envariable.
 *	Takes a line of arguments and breaks it into its
 *	component words and passes those words and the number of them to the
 *	MainParseArgs function.
 *	The line should have all its leading whitespace removed.
 *
 * Side Effects:
 *	Only those that come from the various arguments.
 */
void
Main_ParseArgLine(const char *line) 	/* Line to fracture */
{
	char **argv;			/* Manufactured argument vector */
	int argc;			/* Number of arguments in argv */
	char *args;			/* Space used by the args */
	char *buf;
	char *argv0;
	const char *s;
	size_t len;


	if (line == NULL)
		return;
	for (; *line == ' '; ++line)
		continue;
	if (!*line)
		return;

	/* POSIX rule: MAKEFLAGS can hold a set of option letters without
	 * any blanks or dashes. */
	for (s = line;; s++) {
		if (*s == '\0') {
			while (line != s)
				posixParseOptLetter(*line++);
			return;
		}
		if (strchr(OPTLETTERS, *s) == NULL)
			break;
	}
	argv0 = Var_Value(".MAKE");
	len = strlen(line) + strlen(argv0) + 2;
	buf = emalloc(len);
	(void)snprintf(buf, len, "%s %s", argv0, line);

	argv = brk_string(buf, &argc, &args);
	free(buf);
	MainParseArgs(argc, argv);

	free(args);
	free(argv);
}

/* Add a :-separated path to a Lst of directories.  */
static void
add_dirpath(Lst l, const char *n)
{
	const char *start;
	const char *cp;

	for (start = n;;) {
		for (cp = start; *cp != '\0' && *cp != ':';)
			cp++;
		Dir_AddDiri(l, start, cp);
		if (*cp == '\0')
			break;
		else
			start= cp+1;
	}
}

/*
 * Get the name of this type of MACHINE from utsname so we can share an
 * executable for similar machines. (i.e. m68k: amiga hp300, mac68k, sun3, ...)
 *
 * Note that MACHINE, MACHINE_ARCH and MACHINE_CPU are decided at
 * run-time.
 */
static char *
figure_out_MACHINE()
{
	char *r = getenv("MACHINE");
	if (r == NULL) {
		static struct utsname utsname;

		if (uname(&utsname) == -1)
			err(2, "uname");
		r = utsname.machine;
	}
	return r;
}

static char *
figure_out_MACHINE_ARCH()
{
	char *r = getenv("MACHINE_ARCH");
	if (r == NULL) {
#ifndef MACHINE_ARCH
		r = "unknown";	/* XXX: no uname -p yet */
#else
		r = MACHINE_ARCH;
#endif
	}
	return r;
}
static char *
figure_out_MACHINE_CPU()
{
	char *r = getenv("MACHINE_CPU");
	if (r == NULL) {
#if !defined(MACHINE_CPU) && ! defined(MACHINE_ARCH)
		r = "unknown";	/* XXX: no uname -p yet */
#else
#if defined(MACHINE_CPU)
		r = MACHINE_CPU;
#else
		r = MACHINE_ARCH;
#endif
#endif
	}
	return r;
}

static char *
figure_out_CURDIR()
{
	char *dir, *cwd;
	struct stat sa, sb;

	/* curdir is cwd... */
	cwd = dogetcwd();
	if (cwd == NULL)
		err(2, "getcwd");

	if (stat(cwd, &sa) == -1)
		err(2, "%s", cwd);

	/* ...but we can use the alias $PWD if we can prove it is the same
	 * directory */
	if ((dir = getenv("PWD")) != NULL) {
		if (stat(dir, &sb) == 0 && sa.st_ino == sb.st_ino &&
		    sa.st_dev == sb.st_dev) {
		    	free(cwd);
			return estrdup(dir);
		}
	}

	return cwd;
}

static char *
chdir_verify_path(const char *path, struct dirs *d)
{
	if (chdir(path) == 0) {
		if (path[0] != '/')
			return Str_concat(d->current, path, '/');
		else
			return estrdup(path);
	}
	return NULL;
}

static void
setup_CURDIR_OBJDIR(struct dirs *d)
{
	char *path;

	d->current = figure_out_CURDIR();
	/*
	 * If the MAKEOBJDIR (or by default, the _PATH_OBJDIR) directory
	 * exists, change into it and build there.  
	 *
	 * Once things are initted,
	 * have to add the original directory to the search path,
	 * and modify the paths for the Makefiles appropriately.  The
	 * current directory is also placed as a variable for make scripts.
	 */
	if ((path = getenv("MAKEOBJDIR")) == NULL) {
		path = _PATH_OBJDIR;
	} 
	d->object = chdir_verify_path(path, d);
	if (d->object == NULL)
		d->object = d->current;
}

/*
 * if the VPATH variable is defined, add its contents to the search path.
 * Uses the same format as the PATH env variable, i.e.,
 * <directory>:<directory>:<directory>...
 */
static void
setup_VPATH()
{
	if (Var_Value("VPATH") != NULL) {
		char *vpath;

		vpath = Var_Subst("${VPATH}", NULL, false);
		add_dirpath(defaultPath, vpath);
		(void)free(vpath);
	}
}

static void
read_makefile_list(Lst mk, struct dirs *d)
{
	LstNode ln;
	ln = Lst_Find(mk, ReadMakefile, d);
	if (ln != NULL)
		Fatal("make: cannot open %s.", (char *)Lst_Datum(ln));
}

static void
read_all_make_rules(bool noBuiltins, bool read_depend,
    Lst makefiles, struct dirs *d)
{
	/*
	 * Read in the built-in rules first, followed by the specified
	 * makefile(s), or the default Makefile or makefile, in that order.
	 */
	if (!noBuiltins) {
		LIST sysMkPath; 		/* Path of sys.mk */

		Lst_Init(&sysMkPath);
		Dir_Expand(_PATH_DEFSYSMK, systemIncludePath, &sysMkPath);
		if (Lst_IsEmpty(&sysMkPath))
			Fatal("make: no system rules (%s).", _PATH_DEFSYSMK);

		read_makefile_list(&sysMkPath, d);
	}

	if (!Lst_IsEmpty(makefiles)) {
		read_makefile_list(makefiles, d);
	} else if (!ReadMakefile("makefile", d))
		(void)ReadMakefile("Makefile", d);

	/* read a .depend file, if it exists, and we're not building depend */

	if (read_depend)
		(void)ReadMakefile(".depend", d);
	Parse_End();
}


int main(int, char **);
/*-
 * main --
 *	The main function, for obvious reasons. Initializes variables
 *	and a few modules, then parses the arguments give it in the
 *	environment and on the command line. Reads the system makefile
 *	followed by either Makefile, makefile or the file given by the
 *	-f argument. Sets the .MAKEFLAGS PMake variable based on all the
 *	flags it has received by then uses either the Make or the Compat
 *	module to create the initial list of targets.
 *
 * Results:
 *	If -q was given, exits -1 if anything was out-of-date. Else it exits
 *	0.
 *
 * Side Effects:
 *	The program exits when done. Targets are created. etc. etc. etc.
 */
int
main(int argc, char **argv)
{
	static LIST targs;	/* target nodes to create */
	bool outOfDate = true;	/* false if all targets up to date */
	char *machine = figure_out_MACHINE();
	char *machine_arch = figure_out_MACHINE_ARCH();
	char *machine_cpu = figure_out_MACHINE_CPU();
	const char *syspath = _PATH_DEFSYSPATH;
	char *p;
	static struct dirs d;
	bool read_depend = true;/* false if we don't want to read .depend */

	MainParseChdir(argc, argv);
	setup_CURDIR_OBJDIR(&d);

	esetenv("PWD", d.object);
	unsetenv("CDPATH");

	Static_Lst_Init(create);
	Static_Lst_Init(&makefiles);
	Static_Lst_Init(&varstoprint);
	Static_Lst_Init(&targs);

	beSilent = false;		/* Print commands as executed */
	ignoreErrors = false;		/* Pay attention to non-zero returns */
	noExecute = false;		/* Execute all commands */
	keepgoing = false;		/* Stop on error */
	allPrecious = false;		/* Remove targets when interrupted */
	queryFlag = false;		/* This is not just a check-run */
	noBuiltins = false;		/* Read the built-in rules */
	touchFlag = false;		/* Actually update targets */
	debug = 0;			/* No debug verbosity, please. */

	maxJobs = DEFMAXJOBS;
	compatMake = false;		/* No compat mode */


	/*
	 * Initialize all external modules.
	 */
	Init();

	if (d.object != d.current)
		Dir_AddDir(defaultPath, d.current);
	Var_Set(".CURDIR", d.current);
	Var_Set(".OBJDIR", d.object);
	Parse_setcurdir(d.current);
	Targ_setdirs(d.current, d.object);

	/*
	 * Initialize various variables.
	 *	MAKE also gets this name, for compatibility
	 *	.MAKEFLAGS gets set to the empty string just in case.
	 *	MFLAGS also gets initialized empty, for compatibility.
	 */
	Var_Set("MAKE", argv[0]);
	Var_Set(".MAKE", argv[0]);
	Var_Set(MAKEFLAGS, "");
	Var_Set("MFLAGS", "");
	Var_Set("MACHINE", machine);
	Var_Set("MACHINE_ARCH", machine_arch);
	Var_Set("MACHINE_CPU", machine_cpu);

	/*
	 * First snag any flags out of the MAKEFLAGS environment variable.
	 */
	Main_ParseArgLine(getenv("MAKEFLAGS"));
	
	basedirectory = getenv("MAKEBASEDIRECTORY");
	if (basedirectory == NULL)
		setenv("MAKEBASEDIRECTORY", d.current, 0);

	MainParseArgs(argc, argv);

	/*
	 * Be compatible if user did not specify -j
	 */
	if (!forceJobs)
		compatMake = true;

	/* And set up everything for sub-makes */
	Var_AddCmdline(MAKEFLAGS);


	/*
	 * Set up the .TARGETS variable to contain the list of targets to be
	 * created. If none specified, make the variable empty -- the parser
	 * will fill the thing in with the default or .MAIN target.
	 */
	if (!Lst_IsEmpty(create)) {
		LstNode ln;

		for (ln = Lst_First(create); ln != NULL; ln = Lst_Adv(ln)) {
			char *name = Lst_Datum(ln);

			if (strcmp(name, "depend") == 0)
				read_depend = false;

			Var_Append(".TARGETS", name);
		}
	} else
		Var_Set(".TARGETS", "");


	/*
	 * If no user-supplied system path was given (through the -m option)
	 * add the directories from the DEFSYSPATH (more than one may be given
	 * as dir1:...:dirn) to the system include path.
	 */
	if (Lst_IsEmpty(systemIncludePath))
	    add_dirpath(systemIncludePath, syspath);

	read_all_make_rules(noBuiltins, read_depend, &makefiles, &d);

	Var_Append("MFLAGS", Var_Value(MAKEFLAGS));

	/* Install all the flags into the MAKEFLAGS env variable. */
	if (((p = Var_Value(MAKEFLAGS)) != NULL) && *p)
		esetenv("MAKEFLAGS", p);

	setup_VPATH();

	process_suffixes_after_makefile_is_read();

	if (dumpData) {
		dump_data();
		exit(0);
	}

	/* Print the initial graph, if the user requested it.  */
	if (DEBUG(GRAPH1))
		dump_data();

	/* Print the values of any variables requested by the user.  */
	if (!Lst_IsEmpty(&varstoprint)) {
		LstNode ln;

		for (ln = Lst_First(&varstoprint); ln != NULL;
		    ln = Lst_Adv(ln)) {
			char *value = Var_Value(Lst_Datum(ln));

			printf("%s\n", value ? value : "");
		}
	} else {
		/* Have now read the entire graph and need to make a list
		 * of targets to create. If none was given on the command
		 * line, we consult the parsing module to find the main
		 * target(s) to create.  */
		if (Lst_IsEmpty(create))
			Parse_MainName(&targs);
		else
			Targ_FindList(&targs, create);

		Job_Init(maxJobs);
		/* If the user has defined a .BEGIN target, execute the commands
		 * attached to it.  */
		if (!queryFlag)
			Job_Begin();
		if (compatMake)
			/* Compat_Init will take care of creating all the
			 * targets as well as initializing the module.  */
			Compat_Run(&targs);
		else {
			/* Traverse the graph, checking on all the targets.  */
			outOfDate = Make_Run(&targs);
		}
	}

	/* print the graph now it's been processed if the user requested it */
	if (DEBUG(GRAPH2))
		post_mortem();

	if (queryFlag && outOfDate)
		return 1;
	else
		return 0;
}

/*-
 * ReadMakefile  --
 *	Open and parse the given makefile.
 *
 * Results:
 *	true if ok. false if couldn't open file.
 *
 * Side Effects:
 *	lots
 */
static bool
ReadMakefile(void *p, void *q)
{
	const char *fname = p;	/* makefile to read */
	struct dirs *d = q;
	FILE *stream;
	char *name;

	if (!strcmp(fname, "-")) {
		Var_Set("MAKEFILE", "");
		Parse_File(estrdup("(stdin)"), stdin);
	} else {
		if ((stream = fopen(fname, "r")) != NULL)
			goto found;
		/* if we've chdir'd, rebuild the path name */
		if (d->current != d->object && *fname != '/') {
			char *path;

			path = Str_concat(d->current, fname, '/');
			if ((stream = fopen(path, "r")) == NULL)
				free(path);
			else {
				fname = path;
				goto found;
			}
		}
		/* look in -I and system include directories. */
		name = Dir_FindFile(fname, userIncludePath);
		if (!name)
			name = Dir_FindFile(fname, systemIncludePath);
		if (!name || !(stream = fopen(name, "r")))
			return false;
		fname = name;
		/*
		 * set the MAKEFILE variable desired by System V fans -- the
		 * placement of the setting here means it gets set to the last
		 * makefile specified, as it is set by SysV make.
		 */
found:		Var_Set("MAKEFILE", fname);
		Parse_File(fname, stream);
	}
	return true;
}


/*
 * usage --
 *	exit with usage message
 */
static void
usage()
{
	(void)fprintf(stderr,
"usage: make [-BeiknpqrSst] [-C directory] [-D variable] [-d flags] [-f mk]\n\
	    [-I directory] [-j max_processes] [-m directory] [-V variable]\n\
	    [NAME=value] [target ...]\n");
	exit(2);
}


