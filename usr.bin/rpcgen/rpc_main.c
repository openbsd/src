/* $OpenBSD: rpc_main.c,v 1.29 2015/01/16 06:40:11 deraadt Exp $	 */
/* $NetBSD: rpc_main.c,v 1.9 1996/02/19 11:12:43 pk Exp $	 */

/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the "Oracle America, Inc." nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * rpc_main.c, Top level of the RPC protocol compiler.
 */

#define RPCGEN_VERSION	"199506"/* This program's version (year & month) */

#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>
#include "rpc_parse.h"
#include "rpc_util.h"
#include "rpc_scan.h"

#define EXTEND	1		/* alias for TRUE */
#define DONT_EXTEND	0	/* alias for FALSE */

#define SVR4_CPP "/usr/ccs/lib/cpp"
#define SUNOS_CPP "/lib/cpp"
static int      cppDefined = 0;	/* explicit path for C preprocessor */

struct commandline {
	int cflag;	/* xdr C routines */
	int hflag;	/* header file */
	int lflag;	/* client side stubs */
	int mflag;	/* server side stubs */
	int nflag;	/* netid flag */
	int sflag;	/* server stubs for the given transport */
	int tflag;	/* dispatch Table file */
	int Ssflag;	/* produce server sample code */
	int Scflag;	/* produce client sample code */
	char *infile;	/* input module name */
	char *outfile;/* output module name */
};

static char    *cmdname;

static char    *svcclosetime = "120";
static char    *CPP = "/usr/bin/cpp";
static char     CPPFLAGS[] = "-C";
static char     pathbuf[PATH_MAX];
static char    *allv[] = {
	"rpcgen", "-s", "udp", "-s", "tcp",
};
static int      allc = sizeof(allv) / sizeof(allv[0]);
static char    *allnv[] = {
	"rpcgen", "-s", "netpath",
};
static int      allnc = sizeof(allnv) / sizeof(allnv[0]);

#define ARGLISTLEN	20
#define FIXEDARGS         2

static char    *arglist[ARGLISTLEN];
static int      argcount = FIXEDARGS;


int             nonfatalerrors;	/* errors */
int             inetdflag /* = 1 */ ;	/* Support for inetd *//* is now the
					 * default */
int             pmflag;		/* Support for port monitors */
int             logflag;	/* Use syslog instead of fprintf for errors */
int             tblflag;	/* Support for dispatch table file */
int             callerflag;	/* Generate svc_caller() function */

#define INLINE 3
/* length at which to start doing an inline */

int doinline = INLINE;	/* length at which to start doing an
			 * inline. 3 = default if 0, no
			 * xdr_inline code */

int indefinitewait;	/* If started by port monitors, hang till it
			 * wants */
int exitnow;	/* If started by port monitors, exit after
		 * the call */
int timerflag;	/* TRUE if !indefinite && !exitnow */
int newstyle;	/* newstyle of passing arguments (by value) */
int Cflag = 0;	/* ANSI C syntax */
static int allfiles;	/* generate all files */
int tirpcflag = 0;	/* generating code for tirpc, by default */

static void c_output(char *, char *, int, char *);
static void h_output(char *, char *, int, char *);
static void s_output(int, char **, char *, char *, int, char *, int, int);
static void l_output(char *, char *, int, char *);
static void t_output(char *, char *, int, char *);
static void svc_output(char *, char *, int, char *);
static void clnt_output(char *, char *, int, char *);
static int do_registers(int, char **);
static void addarg(char *);
static void putarg(int, char *);
static void clear_args(void);
static void checkfiles(char *, char *);
static int parseargs(int, char **, struct commandline *);
static void usage(void);
void c_initialize(void);

int
main(int argc, char *argv[])
{
	struct commandline cmd;

	(void) memset((char *) &cmd, 0, sizeof(struct commandline));
	clear_args();
	if (!parseargs(argc, argv, &cmd))
		usage();

	if (cmd.cflag || cmd.hflag || cmd.lflag || cmd.tflag || cmd.sflag ||
	    cmd.mflag || cmd.nflag || cmd.Ssflag || cmd.Scflag) {
		checkfiles(cmd.infile, cmd.outfile);
	} else
		checkfiles(cmd.infile, NULL);

	if (cmd.cflag) {
		c_output(cmd.infile, "-DRPC_XDR", DONT_EXTEND, cmd.outfile);
	} else if (cmd.hflag) {
		h_output(cmd.infile, "-DRPC_HDR", DONT_EXTEND, cmd.outfile);
	} else if (cmd.lflag) {
		l_output(cmd.infile, "-DRPC_CLNT", DONT_EXTEND, cmd.outfile);
	} else if (cmd.sflag || cmd.mflag || (cmd.nflag)) {
		s_output(argc, argv, cmd.infile, "-DRPC_SVC", DONT_EXTEND,
			 cmd.outfile, cmd.mflag, cmd.nflag);
	} else if (cmd.tflag) {
		t_output(cmd.infile, "-DRPC_TBL", DONT_EXTEND, cmd.outfile);
	} else if (cmd.Ssflag) {
		svc_output(cmd.infile, "-DRPC_SERVER", DONT_EXTEND, cmd.outfile);
	} else if (cmd.Scflag) {
		clnt_output(cmd.infile, "-DRPC_CLIENT", DONT_EXTEND, cmd.outfile);
	} else {
		/* the rescans are required, since cpp may effect input */
		c_output(cmd.infile, "-DRPC_XDR", EXTEND, "_xdr.c");
		reinitialize();
		h_output(cmd.infile, "-DRPC_HDR", EXTEND, ".h");
		reinitialize();
		l_output(cmd.infile, "-DRPC_CLNT", EXTEND, "_clnt.c");
		reinitialize();
		if (inetdflag || !tirpcflag)
			s_output(allc, allv, cmd.infile, "-DRPC_SVC", EXTEND,
			    "_svc.c", cmd.mflag, cmd.nflag);
		else
			s_output(allnc, allnv, cmd.infile, "-DRPC_SVC",
			    EXTEND, "_svc.c", cmd.mflag, cmd.nflag);
		if (tblflag) {
			reinitialize();
			t_output(cmd.infile, "-DRPC_TBL", EXTEND, "_tbl.i");
		}
		if (allfiles) {
			reinitialize();
			svc_output(cmd.infile, "-DRPC_SERVER", EXTEND, "_server.c");
		}
		if (allfiles) {
			reinitialize();
			clnt_output(cmd.infile, "-DRPC_CLIENT", EXTEND, "_client.c");
		}
	}
	exit(nonfatalerrors);
	/* NOTREACHED */
}

/*
 * add extension to filename
 */
static char *
extendfile(char *path, char *ext)
{
	char *file;
	char *res;
	char *p;
	size_t len;

	if ((file = strrchr(path, '/')) == NULL)
		file = path;
	else
		file++;

	len = strlen(file) + strlen(ext) + 1;
	res = alloc(len);
	if (res == NULL) {
		fprintf(stderr, "could not allocate memory\n");
		exit(1);
	}
	p = strrchr(file, '.');
	if (p == NULL)
		p = file + strlen(file);
	(void) strlcpy(res, file, len);
	(void) strlcpy(res + (p - file), ext, len - (p - file));
	return (res);
}

/*
 * Open output file with given extension
 */
static void
open_output(char *infile, char *outfile)
{

	if (outfile == NULL) {
		fout = stdout;
		return;
	}
	if (infile != NULL && streq(outfile, infile)) {
		fprintf(stderr, "%s: output would overwrite %s\n", cmdname,
		    infile);
		crash();
	}
	fout = fopen(outfile, "w");
	if (fout == NULL) {
		fprintf(stderr, "%s: unable to open ", cmdname);
		perror(outfile);
		crash();
	}
	record_open(outfile);

}

static void
add_warning(void)
{
	fprintf(fout, "/*\n");
	fprintf(fout, " * Please do not edit this file.\n");
	fprintf(fout, " * It was generated using rpcgen.\n");
	fprintf(fout, " */\n\n");
}

/* clear list of arguments */
static void
clear_args(void)
{
	int             i;
	for (i = FIXEDARGS; i < ARGLISTLEN; i++)
		arglist[i] = NULL;
	argcount = FIXEDARGS;
}

/* make sure that a CPP exists */
static void
find_cpp(void)
{
	struct stat     buf;

	/* SVR4 or explicit cpp does not exist */
	if (stat(CPP, &buf) < 0) {
		if (cppDefined) {
			fprintf(stderr, "cannot find C preprocessor: %s \n", CPP);
			crash();
		} else {
			/* try the other one */
			CPP = SUNOS_CPP;
			if (stat(CPP, &buf) < 0) {	/* can't find any cpp */
				fprintf(stderr,
				    "cannot find any C preprocessor: %s\n", CPP);
				crash();
			}
		}
	}
}

/*
 * Open input file with given define for C-preprocessor
 */
static void
open_input(char *infile, char *define)
{
	int             pd[2];

	infilename = (infile == NULL) ? "<stdin>" : infile;
	(void) pipe(pd);
	switch (fork()) {
	case 0:
		find_cpp();
		putarg(0, CPP);
		putarg(1, CPPFLAGS);
		addarg(define);
		addarg(infile);
		addarg((char *) NULL);
		(void) close(1);
		(void) dup2(pd[1], 1);
		(void) close(pd[0]);
		execv(arglist[0], arglist);
		perror("execv");
		exit(1);
	case -1:
		perror("fork");
		exit(1);
	}
	(void) close(pd[1]);
	fin = fdopen(pd[0], "r");
	if (fin == NULL) {
		fprintf(stderr, "%s: ", cmdname);
		perror(infilename);
		crash();
	}
}

/* valid tirpc nettypes */
static char    *valid_ti_nettypes[] = {
	"netpath",
	"visible",
	"circuit_v",
	"datagram_v",
	"circuit_n",
	"datagram_n",
	"udp",
	"tcp",
	"raw",
	NULL
};

/* valid inetd nettypes */
static char    *valid_i_nettypes[] = {
	"udp",
	"tcp",
	NULL
};

static int
check_nettype(char *name, char *list_to_check[])
{
	int             i;
	for (i = 0; list_to_check[i] != NULL; i++) {
		if (strcmp(name, list_to_check[i]) == 0)
			return 1;
	}
	fprintf(stderr, "illegal nettype :\'%s\'\n", name);
	return 0;
}

/*
 * Compile into an XDR routine output file
 */

static void
c_output(infile, define, extend, outfile)
	char           *infile;
	char           *define;
	int             extend;
	char           *outfile;
{
	definition     *def;
	char           *include;
	char           *outfilename;
	long            tell;

	c_initialize();
	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	if (infile && (include = extendfile(infile, ".h"))) {
		fprintf(fout, "#include \"%s\"\n", include);
		free(include);
		/* .h file already contains rpc/rpc.h */
	} else
		fprintf(fout, "#include <rpc/rpc.h>\n");
	tell = ftell(fout);
	while ((def = get_definition())) {
		emit(def);
	}
	if (extend && tell == ftell(fout)) {
		(void) unlink(outfilename);
	}
}


void
c_initialize(void)
{

	/* add all the starting basic types */

	add_type(1, "int");
	add_type(1, "long");
	add_type(1, "short");
	add_type(1, "bool");

	add_type(1, "u_int");
	add_type(1, "u_long");
	add_type(1, "u_short");

}

static const char rpcgen_table_dcl[] = "struct rpcgen_table {\n\
	char	*(*proc)();\n\
	xdrproc_t	xdr_arg;\n\
	unsigned int	len_arg;\n\
	xdrproc_t	xdr_res;\n\
	unsigned int	len_res;\n\
};\n";


static char *
generate_guard(char *pathname)
{
	char           *filename, *guard, *tmp, *tmp2;

	filename = strrchr(pathname, '/');	/* find last component */
	filename = ((filename == 0) ? pathname : filename + 1);
	guard = strdup(filename);
	if (guard == NULL) {
		fprintf(stderr, "out of memory while processing %s\n", filename);
		crash();
	}

	/* convert to upper case */
	tmp = guard;
	while (*tmp) {
		if (islower((unsigned char)*tmp))
			*tmp = toupper((unsigned char)*tmp);
		tmp++;
	}

	tmp2 = extendfile(guard, "_H_RPCGEN");
	free(guard);
	guard = tmp2;

	return (guard);
}

/*
 * Compile into an XDR header file
 */

static void
h_output(infile, define, extend, outfile)
	char           *infile;
	char           *define;
	int             extend;
	char           *outfile;
{
	definition     *def;
	char           *outfilename;
	long            tell;
	char           *guard;
	list           *l;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	guard = generate_guard(outfilename ? outfilename : infile);

	fprintf(fout, "#ifndef _%s\n#define _%s\n\n", guard,
		guard);

	fprintf(fout, "#define RPCGEN_VERSION\t%s\n\n", RPCGEN_VERSION);
	fprintf(fout, "#include <rpc/rpc.h>\n\n");

	tell = ftell(fout);
	/* print data definitions */
	while ((def = get_definition())) {
		print_datadef(def);
	}

	/*
	 * print function declarations. Do this after data definitions
	 * because they might be used as arguments for functions
	 */
	for (l = defined; l != NULL; l = l->next) {
		print_funcdef(l->val);
	}
	if (extend && tell == ftell(fout)) {
		(void) unlink(outfilename);
	} else if (tblflag) {
		fprintf(fout, rpcgen_table_dcl);
	}
	fprintf(fout, "\n#endif /* !_%s */\n", guard);

	free(guard);
}

/*
 * Compile into an RPC service
 */
static void
s_output(argc, argv, infile, define, extend, outfile, nomain, netflag)
	int             argc;
	char           *argv[];
	char           *infile;
	char           *define;
	int             extend;
	char           *outfile;
	int             nomain;
	int             netflag;
{
	char           *include;
	definition     *def;
	int             foundprogram = 0;
	char           *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	if (infile && (include = extendfile(infile, ".h"))) {
		fprintf(fout, "#include \"%s\"\n", include);
		free(include);
	} else
		fprintf(fout, "#include <rpc/rpc.h>\n");

	fprintf(fout, "#include <unistd.h>\n");
	fprintf(fout, "#include <stdio.h>\n");
	fprintf(fout, "#include <stdlib.h>/* getenv, exit */\n");
	if (Cflag) {
		fprintf(fout,
			"#include <rpc/pmap_clnt.h> /* for pmap_unset */\n");
		fprintf(fout, "#include <string.h> /* strcmp */ \n");
	}
	fprintf(fout, "#include <netdb.h>\n");	/* evas */
	if (strcmp(svcclosetime, "-1") == 0)
		indefinitewait = 1;
	else if (strcmp(svcclosetime, "0") == 0)
		exitnow = 1;
	else if (inetdflag || pmflag) {
		fprintf(fout, "#include <signal.h>\n");
		timerflag = 1;
	}
	if (!tirpcflag && inetdflag)
		fprintf(fout, "#include <sys/ttycom.h>/* TIOCNOTTY */\n");
	if (Cflag && (inetdflag || pmflag)) {
		fprintf(fout, "#ifdef __cplusplus\n");
		fprintf(fout, "#include <sysent.h> /* getdtablesize, open */\n");
		fprintf(fout, "#endif /* __cplusplus */\n");

		if (tirpcflag)
			fprintf(fout, "#include <unistd.h> /* setsid */\n");
	}
	if (tirpcflag)
		fprintf(fout, "#include <sys/types.h>\n");

	fprintf(fout, "#include <memory.h>\n");
	if (tirpcflag)
		fprintf(fout, "#include <stropts.h>\n");

	if (inetdflag || !tirpcflag) {
		fprintf(fout, "#include <sys/socket.h>\n");
		fprintf(fout, "#include <netinet/in.h>\n");
	}
	if ((netflag || pmflag) && tirpcflag) {
		fprintf(fout, "#include <netconfig.h>\n");
	}
	if (/* timerflag && */ tirpcflag)
		fprintf(fout, "#include <sys/resource.h> /* rlimit */\n");
	if (logflag || inetdflag || pmflag) {
		fprintf(fout, "#include <syslog.h>\n");
		fprintf(fout, "#include <errno.h>\n");
	}
	/* for ANSI-C */
	fprintf(fout, "\n#ifdef __STDC__\n#define SIG_PF void(*)(int)\n#endif\n");

	fprintf(fout, "\n#ifdef DEBUG\n#define RPC_SVC_FG\n#endif\n");
	if (timerflag)
		fprintf(fout, "\n#define _RPCSVC_CLOSEDOWN %s\n", svcclosetime);
	while ((def = get_definition())) {
		foundprogram |= (def->def_kind == DEF_PROGRAM);
	}
	if (extend && !foundprogram) {
		(void) unlink(outfilename);
		return;
	}
	if (callerflag)		/* EVAS */
		fprintf(fout, "\nstatic SVCXPRT *caller;\n");	/* EVAS */
	write_most(infile, netflag, nomain);
	if (!nomain) {
		if (!do_registers(argc, argv)) {
			if (outfilename)
				(void) unlink(outfilename);
			usage();
		}
		write_rest();
	}
}

/*
 * generate client side stubs
 */
static void
l_output(infile, define, extend, outfile)
	char           *infile;
	char           *define;
	int             extend;
	char           *outfile;
{
	char           *include;
	definition     *def;
	int             foundprogram = 0;
	char           *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	if (Cflag)
		fprintf(fout, "#include <memory.h> /* for memset */\n");
	if (infile && (include = extendfile(infile, ".h"))) {
		fprintf(fout, "#include \"%s\"\n", include);
		free(include);
	} else
		fprintf(fout, "#include <rpc/rpc.h>\n");
	while ((def = get_definition()))
		foundprogram |= (def->def_kind == DEF_PROGRAM);

	if (extend && !foundprogram) {
		(void) unlink(outfilename);
		return;
	}
	write_stubs();
}

/*
 * generate the dispatch table
 */
static void
t_output(infile, define, extend, outfile)
	char           *infile;
	char           *define;
	int             extend;
	char           *outfile;
{
	definition     *def;
	int             foundprogram = 0;
	char           *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	while ((def = get_definition()))
		foundprogram |= (def->def_kind == DEF_PROGRAM);

	if (extend && !foundprogram) {
		(void) unlink(outfilename);
		return;
	}
	write_tables();
}

/* sample routine for the server template */
static void
svc_output(infile, define, extend, outfile)
	char           *infile;
	char           *define;
	int             extend;
	char           *outfile;
{
	definition     *def;
	char           *include;
	char           *outfilename;
	long            tell;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	checkfiles(infile, outfilename);	/* check if outfile already
						 * exists. if so, print an
						 * error message and exit */
	open_output(infile, outfilename);
	add_sample_msg();

	if (infile && (include = extendfile(infile, ".h"))) {
		fprintf(fout, "#include \"%s\"\n", include);
		free(include);
	} else
		fprintf(fout, "#include <rpc/rpc.h>\n");

	tell = ftell(fout);
	while ((def = get_definition()))
		write_sample_svc(def);

	if (extend && tell == ftell(fout))
		(void) unlink(outfilename);
}


/* sample main routine for client */
static void
clnt_output(infile, define, extend, outfile)
	char           *infile;
	char           *define;
	int             extend;
	char           *outfile;
{
	definition *def;
	char *include, *outfilename;
	long tell;
	int has_program = 0;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;

	/*
	 * check if outfile already exists. if so,
	 * print an error message and exit
	 */
	checkfiles(infile, outfilename);

	open_output(infile, outfilename);
	add_sample_msg();
	if (infile && (include = extendfile(infile, ".h"))) {
		fprintf(fout, "#include \"%s\"\n", include);
		free(include);
	} else
		fprintf(fout, "#include <rpc/rpc.h>\n");
	tell = ftell(fout);
	while ((def = get_definition()))
		has_program += write_sample_clnt(def);

	if (has_program)
		write_sample_clnt_main();

	if (extend && tell == ftell(fout))
		(void) unlink(outfilename);
}

/*
 * Perform registrations for service output
 * Return 0 if failed; 1 otherwise.
 */
static int
do_registers(argc, argv)
	int             argc;
	char           *argv[];
{
	int             i;

	if (inetdflag || !tirpcflag) {
		for (i = 1; i < argc; i++) {
			if (streq(argv[i], "-s")) {
				if (!check_nettype(argv[i + 1], valid_i_nettypes))
					return 0;
				write_inetd_register(argv[i + 1]);
				i++;
			}
		}
	} else {
		for (i = 1; i < argc; i++)
			if (streq(argv[i], "-s")) {
				if (!check_nettype(argv[i + 1], valid_ti_nettypes))
					return 0;
				write_nettype_register(argv[i + 1]);
				i++;
			} else if (streq(argv[i], "-n")) {
				write_netid_register(argv[i + 1]);
				i++;
			}
	}
	return 1;
}

/*
 * Add another argument to the arg list
 */
static void
addarg(cp)
	char           *cp;
{
	if (argcount >= ARGLISTLEN) {
		fprintf(stderr, "rpcgen: too many defines\n");
		crash();
		/* NOTREACHED */
	}
	arglist[argcount++] = cp;

}

static void
putarg(where, cp)
	char           *cp;
	int             where;
{
	if (where >= ARGLISTLEN) {
		fprintf(stderr, "rpcgen: arglist coding error\n");
		crash();
		/* NOTREACHED */
	}
	arglist[where] = cp;
}

/*
 * if input file is stdin and an output file is specified then complain
 * if the file already exists. Otherwise the file may get overwritten
 * If input file does not exist, exit with an error
 */
static void
checkfiles(infile, outfile)
	char           *infile;
	char           *outfile;
{
	struct stat     buf;

	if (infile)		/* infile ! = NULL */
		if (stat(infile, &buf) < 0) {
			perror(infile);
			crash();
		}
#if 0
	if (outfile) {
		if (stat(outfile, &buf) < 0)
			return;	/* file does not exist */
		else {
			fprintf(stderr,
			    "file '%s' already exists and may be overwritten\n",
			    outfile);
			crash();
		}
	}
#endif
}

/*
 * Parse command line arguments
 */
static int
parseargs(argc, argv, cmd)
	int argc;
	char *argv[];
	struct commandline *cmd;
{
	int i, j, nflags;
	char c, flag[(1 << 8 * sizeof(char))];

	cmdname = argv[0];
	cmd->infile = cmd->outfile = NULL;
	if (argc < 2)
		return (0);

	allfiles = 0;
	flag['c'] = 0;
	flag['h'] = 0;
	flag['l'] = 0;
	flag['m'] = 0;
	flag['o'] = 0;
	flag['s'] = 0;
	flag['n'] = 0;
	flag['t'] = 0;
	flag['S'] = 0;
	flag['C'] = 0;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			if (cmd->infile) {
				fprintf(stderr,
				    "Cannot specify more than one input file!\n");
				return (0);
			}
			cmd->infile = argv[i];
		} else {
			for (j = 1; argv[i][j] != 0; j++) {
				c = argv[i][j];
				switch (c) {
				case 'A':
					callerflag = 1;
					break;
				case 'a':
					allfiles = 1;
					break;
				case 'c':
				case 'h':
				case 'l':
				case 'm':
				case 't':
					if (flag[(unsigned char)c])
						return (0);
					flag[(unsigned char)c] = 1;
					break;
				case 'S':
					/*
					 * sample flag: Ss or Sc. Ss means
					 * set flag['S']; Sc means set
					 * flag['C'];
					 */
					c = argv[i][++j];	/* get next char */
					if (c == 's')
						c = 'S';
					else if (c == 'c')
						c = 'C';
					else
						return (0);

					if (flag[(unsigned char)c])
						return (0);
					flag[(unsigned char)c] = 1;
					break;
				case 'C':	/* ANSI C syntax */
					Cflag = 1;
					break;

				case 'b':
					/*
					 * turn TIRPC flag off for
					 * generating backward compatible
					 */
					tirpcflag = 0;
					break;

				case 'I':
					inetdflag = 1;
					break;
				case 'N':
					newstyle = 1;
					break;
				case 'L':
					logflag = 1;
					break;
				case 'K':
					if (++i == argc)
						return (0);
					svcclosetime = argv[i];
					goto nextarg;
				case 'T':
					tblflag = 1;
					break;
				case 'i':
					if (++i == argc)
						return (0);
					doinline = atoi(argv[i]);
					goto nextarg;
				case 'n':
				case 'o':
				case 's':
					if (argv[i][j - 1] != '-' ||
					    argv[i][j + 1] != 0)
						return (0);
					flag[(unsigned char)c] = 1;
					if (++i == argc)
						return (0);
					if (c == 's') {
						if (!streq(argv[i], "udp") &&
						    !streq(argv[i], "tcp"))
							return (0);
					} else if (c == 'o') {
						if (cmd->outfile)
							return (0);
						cmd->outfile = argv[i];
					}
					goto nextarg;
				case 'D':
					if (argv[i][j - 1] != '-')
						return (0);
					(void) addarg(argv[i]);
					goto nextarg;
				case 'Y':
					if (++i == argc)
						return (0);
					if (snprintf(pathbuf, sizeof pathbuf,
					    "%s/cpp", argv[i]) >= sizeof pathbuf)
						usage();
					CPP = pathbuf;
					cppDefined = 1;
					goto nextarg;
				default:
					return (0);
				}
			}
	nextarg:
			;
		}
	}

	cmd->cflag = flag['c'];
	cmd->hflag = flag['h'];
	cmd->lflag = flag['l'];
	cmd->mflag = flag['m'];
	cmd->nflag = flag['n'];
	cmd->sflag = flag['s'];
	cmd->tflag = flag['t'];
	cmd->Ssflag = flag['S'];
	cmd->Scflag = flag['C'];

	if (tirpcflag) {
		pmflag = inetdflag ? 0 : 1;	/* pmflag or inetdflag is
						 * always TRUE */
		if (inetdflag && cmd->nflag) {
			/* netid not allowed with inetdflag */
			fprintf(stderr, "Cannot use netid flag with inetd flag!\n");
			return (0);
		}
	} else {
		/* 4.1 mode */
		pmflag = 0;	/* set pmflag only in tirpcmode */
		inetdflag = 1;	/* inetdflag is TRUE by default */
		if (cmd->nflag) {
			/* netid needs TIRPC */
			fprintf(stderr, "Cannot use netid flag without TIRPC!\n");
			return (0);
		}
	}

	if (newstyle && (tblflag || cmd->tflag)) {
		fprintf(stderr, "Cannot use table flags with newstyle!\n");
		return (0);
	}
	/* check no conflicts with file generation flags */
	nflags = cmd->cflag + cmd->hflag + cmd->lflag + cmd->mflag +
	    cmd->sflag + cmd->nflag + cmd->tflag + cmd->Ssflag + cmd->Scflag;

	if (nflags == 0) {
		if (cmd->outfile != NULL || cmd->infile == NULL)
			return (0);
	} else if (nflags > 1) {
		fprintf(stderr, "Cannot have more than one file generation flag!\n");
		return (0);
	}
	return (1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-abACILNT] [-Dname[=value]] [-i lines] "
	    "[-K seconds] infile\n", cmdname);
	fprintf(stderr, "       %s [-c | -h | -l | -m | -t | -Sc | -Ss] "
	    "[-o outfile] [infile]\n", cmdname);
	fprintf(stderr, "       %s [-s nettype]* [-o outfile] [infile]\n", cmdname);
	exit(1);
}
