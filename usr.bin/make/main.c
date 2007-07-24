/*	$OpenPackages$ */
/*	$OpenBSD: main.c,v 1.73 2007/07/24 18:56:15 espie Exp $ */
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MAKE_BOOTSTRAP
#include <sys/utsname.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "defines.h"
#include "var.h"
#include "parse.h"
#include "parsevar.h"
#include "dir.h"
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

#ifndef PATH_MAX
# ifdef MAXPATHLEN
#  define PATH_MAX (MAXPATHLEN+1)
# else
#  define PATH_MAX	1024
# endif
#endif

#ifndef DEFMAXLOCAL
#define DEFMAXLOCAL DEFMAXJOBS
#endif	/* DEFMAXLOCAL */

#define MAKEFLAGS	".MAKEFLAGS"

static LIST		to_create; 	/* Targets to be made */
Lst create = &to_create;
GNode			*DEFAULT;	/* .DEFAULT node */
bool 		allPrecious;	/* .PRECIOUS given on line by itself */

static bool		noBuiltins;	/* -r flag */
static LIST		makefiles;	/* ordered list of makefiles to read */
static LIST		varstoprint;	/* list of variables to print */
int			maxJobs;	/* -j argument */
static int		maxLocal;	/* -L argument */
bool 		compatMake;	/* -B argument */
int 		debug;		/* -d flag */
bool 		noExecute;	/* -n flag */
bool 		keepgoing;	/* -k flag */
bool 		queryFlag;	/* -q flag */
bool 		touchFlag;	/* -t flag */
bool 		usePipes;	/* !-P flag */
bool 		ignoreErrors;	/* -i flag */
bool 		beSilent;	/* -s flag */

static void		MainParseArgs(int, char **);
static char *		chdir_verify_path(const char *);
static int		ReadMakefile(void *, void *);
static void		add_dirpath(Lst, const char *);
static void		usage(void);
static void		posixParseOptLetter(int);
static void		record_option(int, const char *);

static char *curdir;			/* startup directory */
static char *objdir;			/* where we chdir'ed to */


static void record_option(int c, const char *arg)
{
    char opt[3];

    opt[0] = '-';
    opt[1] = c;
    opt[2] = '\0';
    Var_Append(MAKEFLAGS, opt, VAR_GLOBAL);
    if (arg != NULL)
    	Var_Append(MAKEFLAGS, arg, VAR_GLOBAL);
}

static void
posixParseOptLetter(int c)
{
	switch(c) {
	case 'B':
		compatMake = true;
		return;	/* XXX don't pass to submakes. */
	case 'P':
		usePipes = false;
		break;
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
	int forceJobs = 0;

#define OPTFLAGS "BD:I:PSV:d:ef:ij:km:nqrst"
#define OPTLETTERS "BPSiknqrst"

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
		case 'D':
			Var_Set(optarg, "1", VAR_GLOBAL);
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
				case 'j':
					debug |= DEBUG_JOB;
					break;
				case 'l':
					debug |= DEBUG_LOUD;
					break;
				case 'm':
					debug |= DEBUG_MAKE;
					break;
				case 's':
					debug |= DEBUG_SUFF;
					break;
				case 't':
					debug |= DEBUG_TARG;
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
		   char *endptr;

			forceJobs = true;
			maxJobs = strtol(optarg, &endptr, 0);
			if (endptr == optarg) {
				fprintf(stderr,
					"make: illegal argument to -j option -- %s -- not a number\n",
					optarg);
				usage();
			}
			maxLocal = maxJobs;
			record_option(c, optarg);
			break;
		}
		case 'm':
			Dir_AddDir(sysIncPath, optarg);
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

	/*
	 * Be compatible if user did not specify -j and did not explicitly
	 * turn compatibility on
	 */
	if (!compatMake && !forceJobs)
		compatMake = true;
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

char *
chdir_verify_path(const char *path)
{
    struct stat sb;

    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
	if (chdir(path)) {
	    (void)fprintf(stderr, "make warning: %s: %s.\n",
		  path, strerror(errno));
	    return NULL;
	} else {
	    if (path[0] != '/')
	    	return Str_concat(curdir, path, '/');
	    else
		return estrdup(path);
	}
    }

    return NULL;
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
	struct stat sb, sa;
	char *p, *path, *pathp, *pwd;
	char *mdpath;
	char *machine = getenv("MACHINE");
	char *machine_arch = getenv("MACHINE_ARCH");
	const char *syspath = _PATH_DEFSYSPATH;

#ifdef RLIMIT_NOFILE
	/*
	 * get rid of resource limit on file descriptors
	 */
	{
		struct rlimit rl;
		if (getrlimit(RLIMIT_NOFILE, &rl) != -1 &&
		    rl.rlim_cur != rl.rlim_max) {
			rl.rlim_cur = rl.rlim_max;
			(void)setrlimit(RLIMIT_NOFILE, &rl);
		}
	}
#endif
	/*
	 * Find where we are and take care of PWD for the automounter...
	 * All this code is so that we know where we are when we start up
	 * on a different machine with pmake.
	 */
	if ((curdir = dogetcwd()) == NULL) {
		(void)fprintf(stderr, "make: %s.\n", strerror(errno));
		exit(2);
	}

	if (stat(curdir, &sa) == -1) {
	    (void)fprintf(stderr, "make: %s: %s.\n",
			  curdir, strerror(errno));
	    exit(2);
	}

	if ((pwd = getenv("PWD")) != NULL) {
	    if (stat(pwd, &sb) == 0 && sa.st_ino == sb.st_ino &&
		sa.st_dev == sb.st_dev) {
		    free(curdir);
		    curdir = estrdup(pwd);
	    }
	}

	/*
	 * Get the name of this type of MACHINE from utsname
	 * so we can share an executable for similar machines.
	 * (i.e. m68k: amiga hp300, mac68k, sun3, ...)
	 *
	 * Note that both MACHINE and MACHINE_ARCH are decided at
	 * run-time.
	 */
	if (!machine) {
#ifndef MAKE_BOOTSTRAP
	    static struct utsname utsname;

	    if (uname(&utsname) == -1) {
		    perror("make: uname");
		    exit(2);
	    }
	    machine = utsname.machine;
#else
	    machine = MACHINE;
#endif
	}

	if (!machine_arch) {
#ifndef MACHINE_ARCH
	    machine_arch = "unknown";	/* XXX: no uname -p yet */
#else
	    machine_arch = MACHINE_ARCH;
#endif
	}

	/*
	 * If the MAKEOBJDIR (or by default, the _PATH_OBJDIR) directory
	 * exists, change into it and build there.  (If a .${MACHINE} suffix
	 * exists, use that directory instead).
	 * Otherwise check MAKEOBJDIRPREFIX`cwd` (or by default,
	 * _PATH_OBJDIRPREFIX`cwd`) and build there if it exists.
	 * If all fails, use the current directory to build.
	 *
	 * Once things are initted,
	 * have to add the original directory to the search path,
	 * and modify the paths for the Makefiles appropriately.  The
	 * current directory is also placed as a variable for make scripts.
	 */
	mdpath = NULL;
	if (!(pathp = getenv("MAKEOBJDIRPREFIX"))) {
		if (!(path = getenv("MAKEOBJDIR"))) {
			path = _PATH_OBJDIR;
			pathp = _PATH_OBJDIRPREFIX;
			mdpath = Str_concat(path, machine, '.');
			if (!(objdir = chdir_verify_path(mdpath)))
				if (!(objdir=chdir_verify_path(path))) {
					free(mdpath);
					mdpath = Str_concat(pathp, curdir, 0);
					if (!(objdir=chdir_verify_path(mdpath)))
						objdir = curdir;
				}
		}
		else if (!(objdir = chdir_verify_path(path)))
			objdir = curdir;
	}
	else {
		mdpath = Str_concat(pathp, curdir, 0);
		if (!(objdir = chdir_verify_path(mdpath)))
			objdir = curdir;
	}
	free(mdpath);

	esetenv("PWD", objdir);
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
	usePipes = true;		/* Catch child output in pipes */
	debug = 0;			/* No debug verbosity, please. */

	maxLocal = DEFMAXLOCAL; 	/* Set default local max concurrency */
	maxJobs = maxLocal;
	compatMake = false;		/* No compat mode */


	/*
	 * Initialize all external modules.
	 */
	Init();

	if (objdir != curdir)
		Dir_AddDir(dirSearchPath, curdir);
	Var_Set(".CURDIR", curdir, VAR_GLOBAL);
	Var_Set(".OBJDIR", objdir, VAR_GLOBAL);

	/*
	 * Initialize various variables.
	 *	MAKE also gets this name, for compatibility
	 *	.MAKEFLAGS gets set to the empty string just in case.
	 *	MFLAGS also gets initialized empty, for compatibility.
	 */
	Var_Set("MAKE", argv[0], VAR_GLOBAL);
	Var_Set(".MAKE", argv[0], VAR_GLOBAL);
	Var_Set(MAKEFLAGS, "", VAR_GLOBAL);
	Var_Set("MFLAGS", "", VAR_GLOBAL);
	Var_Set("MACHINE", machine, VAR_GLOBAL);
	Var_Set("MACHINE_ARCH", machine_arch, VAR_GLOBAL);

	/*
	 * First snag any flags out of the MAKEFLAGS environment variable.
	 */
	Main_ParseArgLine(getenv("MAKEFLAGS"));

	MainParseArgs(argc, argv);

	/* And set up everything for sub-makes */
	Var_AddCmdline(MAKEFLAGS);


	DEFAULT = NULL;

	/*
	 * Set up the .TARGETS variable to contain the list of targets to be
	 * created. If none specified, make the variable empty -- the parser
	 * will fill the thing in with the default or .MAIN target.
	 */
	if (!Lst_IsEmpty(create)) {
		LstNode ln;

		for (ln = Lst_First(create); ln != NULL; ln = Lst_Adv(ln)) {
			char *name = (char *)Lst_Datum(ln);

			Var_Append(".TARGETS", name, VAR_GLOBAL);
		}
	} else
		Var_Set(".TARGETS", "", VAR_GLOBAL);


	/*
	 * If no user-supplied system path was given (through the -m option)
	 * add the directories from the DEFSYSPATH (more than one may be given
	 * as dir1:...:dirn) to the system include path.
	 */
	if (Lst_IsEmpty(sysIncPath))
	    add_dirpath(sysIncPath, syspath);

	/*
	 * Read in the built-in rules first, followed by the specified
	 * makefile(s), or the default BSDmakefile, Makefile or
	 * makefile, in that order.
	 */
	if (!noBuiltins) {
		LstNode ln;
		LIST sysMkPath; 		/* Path of sys.mk */

		Lst_Init(&sysMkPath);
		Dir_Expand(_PATH_DEFSYSMK, sysIncPath, &sysMkPath);
		if (Lst_IsEmpty(&sysMkPath))
			Fatal("make: no system rules (%s).", _PATH_DEFSYSMK);
		ln = Lst_Find(&sysMkPath, ReadMakefile, NULL);
		if (ln != NULL)
			Fatal("make: cannot open %s.", (char *)Lst_Datum(ln));
#ifdef CLEANUP
		Lst_Destroy(&sysMkPath, (SimpleProc)free);
#endif
	}

	if (!Lst_IsEmpty(&makefiles)) {
		LstNode ln;

		ln = Lst_Find(&makefiles, ReadMakefile, NULL);
		if (ln != NULL)
			Fatal("make: cannot open %s.", (char *)Lst_Datum(ln));
	} else if (!ReadMakefile("BSDmakefile", NULL))
		if (!ReadMakefile("makefile", NULL))
			(void)ReadMakefile("Makefile", NULL);

	/* Always read a .depend file, if it exists. */
	(void)ReadMakefile(".depend", NULL);

	Var_Append("MFLAGS", Var_Value(MAKEFLAGS),
	    VAR_GLOBAL);

	/* Install all the flags into the MAKEFLAGS env variable. */
	if (((p = Var_Value(MAKEFLAGS)) != NULL) && *p)
		esetenv("MAKEFLAGS", p);

	/*
	 * For compatibility, look at the directories in the VPATH variable
	 * and add them to the search path, if the variable is defined. The
	 * variable's value is in the same format as the PATH envariable, i.e.
	 * <directory>:<directory>:<directory>...
	 */
	if (Var_Value("VPATH") != NULL) {
	    char *vpath;

	    vpath = Var_Subst("${VPATH}", NULL, false);
	    add_dirpath(dirSearchPath, vpath);
	    (void)free(vpath);
	}

	/* Now that all search paths have been read for suffixes et al, it's
	 * time to add the default search path to their lists...  */
	Suff_DoPaths();

	/* Print the initial graph, if the user requested it.  */
	if (DEBUG(GRAPH1))
		Targ_PrintGraph(1);

	/* Print the values of any variables requested by the user.  */
	if (!Lst_IsEmpty(&varstoprint)) {
	    LstNode ln;

	    for (ln = Lst_First(&varstoprint); ln != NULL; ln = Lst_Adv(ln)) {
		    char *value = Var_Value((char *)Lst_Datum(ln));

		    printf("%s\n", value ? value : "");
	    }
	} else {
	    /* Have now read the entire graph and need to make a list of targets
	     * to create. If none was given on the command line, we consult the
	     * parsing module to find the main target(s) to create.  */
	    if (Lst_IsEmpty(create))
		Parse_MainName(&targs);
	    else
		Targ_FindList(&targs, create);

	    if (compatMake)
		/* Compat_Init will take care of creating all the targets as
		 * well as initializing the module.  */
	    	Compat_Run(&targs);
	    else {
		/* Initialize job module before traversing the graph, now that
		 * any .BEGIN and .END targets have been read.	This is done
		 * only if the -q flag wasn't given (to prevent the .BEGIN from
		 * being executed should it exist).  */
		if (!queryFlag) {
			if (maxLocal == -1)
				maxLocal = maxJobs;
			Job_Init(maxJobs, maxLocal);
		}

		/* Traverse the graph, checking on all the targets.  */
		outOfDate = Make_Run(&targs);
	    }
	}

#ifdef CLEANUP
	Lst_Destroy(&targs, NOFREE);
	Lst_Destroy(&varstoprint, NOFREE);
	Lst_Destroy(&makefiles, NOFREE);
	Lst_Destroy(create, (SimpleProc)free);
#endif

	/* print the graph now it's been processed if the user requested it */
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);

#ifdef CLEANUP
	if (objdir != curdir)
	    free(objdir);
	free(curdir);
	End();
#endif
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
ReadMakefile(void *p, void *q UNUSED)
{
	char *fname = (char *)p;	/* makefile to read */
	FILE *stream;
	char *name;

	if (!strcmp(fname, "-")) {
		Var_Set("MAKEFILE", "", VAR_GLOBAL);
		Parse_File(estrdup("(stdin)"), stdin);
	} else {
		if ((stream = fopen(fname, "r")) != NULL)
			goto found;
		/* if we've chdir'd, rebuild the path name */
		if (curdir != objdir && *fname != '/') {
			char *path;

			path = Str_concat(curdir, fname, '/');
			if ((stream = fopen(path, "r")) == NULL)
			    free(path);
			else {
			    fname = path;
			    goto found;
			}
		}
		/* look in -I and system include directories. */
		name = Dir_FindFile(fname, parseIncPath);
		if (!name)
			name = Dir_FindFile(fname, sysIncPath);
		if (!name || !(stream = fopen(name, "r")))
			return false;
		fname = name;
		/*
		 * set the MAKEFILE variable desired by System V fans -- the
		 * placement of the setting here means it gets set to the last
		 * makefile specified, as it is set by SysV make.
		 */
found:		Var_Set("MAKEFILE", fname, VAR_GLOBAL);
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
"usage: make [-BeiknPqrSst] [-D variable] [-d flags] [-f makefile]\n\
	    [-I directory] [-j max_jobs] [-m directory] [-V variable]\n\
	    [NAME=value] [target ...]\n");
	exit(2);
}


