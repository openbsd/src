/*	$OpenBSD: rpc_svcout.c,v 1.26 2010/09/01 14:43:34 millert Exp $	*/
/*	$NetBSD: rpc_svcout.c,v 1.7 1995/06/24 14:59:59 pk Exp $	*/

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
 * rpc_svcout.c, Server-skeleton outputter for the RPC protocol compiler
 */
#include <sys/cdefs.h>
#include <stdio.h>
#include <string.h>
#include "rpc_parse.h"
#include "rpc_util.h"

static char RQSTP[] = "rqstp";
static char TRANSP[] = "transp";
static char ARG[] = "argument";
static char RESULT[] = "result";
static char ROUTINE[] = "local";

char _errbuf[256];	/* For all messages */

void internal_proctype(proc_list *);
static void write_real_program(definition *);
static void write_program(definition *, char *);
static void printerr(char *, char *);
static void printif(char *, char *, char *, char *);
static void write_inetmost(char *);
static void print_return(char *);
static void print_pmapunset(char *);
static void print_err_message(char *);
static void write_timeout_func(void);
static void write_pm_most(char *, int);
static void write_caller_func(void);
static void write_rpc_svc_fg(char *, char *);
static void write_msg_out(void);
static void open_log_file(char *, char *);

static void
p_xdrfunc(char *rname, char *typename)
{
	if (Cflag)
		fprintf(fout, "\t\txdr_%s = (xdrproc_t) xdr_%s;\n",
		    rname, stringfix(typename));
	else
		fprintf(fout, "\t\txdr_%s = xdr_%s;\n", rname,
		    stringfix(typename));
}

void
internal_proctype(plist)
	proc_list *plist;
{
	fprintf(fout, "static ");
	ptype(plist->res_prefix, plist->res_type, 1);
	fprintf(fout, "*");
}

/*
 * write most of the service, that is, everything but the registrations.
 */
void
write_most(infile, netflag, nomain)
	char *infile;		/* our name */
	int netflag;
	int nomain;
{
	if (inetdflag || pmflag) {
		char *var_type;
		var_type = (nomain? "extern" : "static");
		fprintf(fout, "%s int _rpcpmstart;", var_type);
		fprintf(fout, "\t\t/* Started by a port monitor ? */\n");
		fprintf(fout, "%s int _rpcfdtype;", var_type);
		fprintf(fout, "\t\t/* Whether Stream or Datagram ? */\n");
		if (timerflag) {
			fprintf(fout, "%s int _rpcsvcdirty;", var_type);
			fprintf(fout, "\t/* Still serving ? */\n");
		}
		write_svc_aux(nomain);
	}
	/* write out dispatcher and stubs */
	write_programs(nomain? (char *)NULL : "static");

	if (nomain)
		return;

	fprintf(fout, "\nmain()\n");
	fprintf(fout, "{\n");
	if (inetdflag) {
		write_inetmost(infile); /* Includes call to write_rpc_svc_fg() */
	} else {
		if (tirpcflag) {
			if (netflag) {
				fprintf(fout, "\tSVCXPRT *%s;\n", TRANSP);
				fprintf(fout, "\tstruct netconfig *nconf = NULL;\n");
			}
			fprintf(fout, "\tpid_t pid;\n");
			fprintf(fout, "\tint i;\n");
			fprintf(fout, "\tchar mname[FMNAMESZ + 1];\n\n");
			write_pm_most(infile, netflag);
			fprintf(fout, "\telse {\n");
			write_rpc_svc_fg(infile, "\t\t");
			fprintf(fout, "\t}\n");
		} else {
			fprintf(fout, "\tSVCXPRT *%s;\n", TRANSP);
			fprintf(fout, "\n");
			print_pmapunset("\t");
		}
	}

	if (logflag && !inetdflag) {
		open_log_file(infile, "\t");
	}
}

/*
 * write a registration for the given transport
 */
void
write_netid_register(transp)
	char *transp;
{
	list *l;
	definition *def;
	version_list *vp;
	char *sp;
	char tmpbuf[32];

	sp = "";
	fprintf(fout, "\n");
	fprintf(fout, "%s\tnconf = getnetconfigent(\"%s\");\n", sp, transp);
	fprintf(fout, "%s\tif (nconf == NULL) {\n", sp);
	(void) snprintf(_errbuf, sizeof _errbuf, "cannot find %s netid.", transp);
	snprintf(tmpbuf, sizeof tmpbuf, "%s\t\t", sp);
	print_err_message(tmpbuf);
	fprintf(fout, "%s\t\texit(1);\n", sp);
	fprintf(fout, "%s\t}\n", sp);
	fprintf(fout, "%s\t%s = svc_tli_create(RPC_ANYFD, nconf, 0, 0, 0);\n",
	    sp, TRANSP);
	fprintf(fout, "%s\tif (%s == NULL) {\n", sp, TRANSP);
	(void) snprintf(_errbuf, sizeof _errbuf, "cannot create %s service.", transp);
	print_err_message(tmpbuf);
	fprintf(fout, "%s\t\texit(1);\n", sp);
	fprintf(fout, "%s\t}\n", sp);

	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind != DEF_PROGRAM)
			continue;
		for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
			fprintf(fout,
			    "%s\t(void) rpcb_unset(%s, %s, nconf);\n",
			    sp, def->def_name, vp->vers_name);
			fprintf(fout,
			    "%s\tif (!svc_reg(%s, %s, %s, ",
			    sp, TRANSP, def->def_name, vp->vers_name);
			pvname(def->def_name, vp->vers_num);
			fprintf(fout, ", nconf)) {\n");
			(void) snprintf(_errbuf, sizeof _errbuf,
			    "unable to register (%s, %s, %s).",
			    def->def_name, vp->vers_name, transp);
			print_err_message(tmpbuf);
			fprintf(fout, "%s\t\texit(1);\n", sp);
			fprintf(fout, "%s\t}\n", sp);
		}
	}
	fprintf(fout, "%s\tfreenetconfigent(nconf);\n", sp);
}

/*
 * write a registration for the given transport for TLI
 */
void
write_nettype_register(transp)
	char *transp;
{
	list *l;
	definition *def;
	version_list *vp;

	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind != DEF_PROGRAM)
			continue;
		for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
			fprintf(fout, "\tif (!svc_create(");
			pvname(def->def_name, vp->vers_num);
			fprintf(fout, ", %s, %s, \"%s\")) {\n",
			    def->def_name, vp->vers_name, transp);
			(void) snprintf(_errbuf, sizeof _errbuf,
			    "unable to create (%s, %s) for %s.",
			    def->def_name, vp->vers_name, transp);
			print_err_message("\t\t");
			fprintf(fout, "\t\texit(1);\n");
			fprintf(fout, "\t}\n");
		}
	}
}

/*
 * write the rest of the service
 */
void
write_rest()
{
	fprintf(fout, "\n");
	if (inetdflag) {
		fprintf(fout, "\tif (%s == (SVCXPRT *)NULL) {\n", TRANSP);
		(void) snprintf(_errbuf, sizeof _errbuf, "could not create a handle");
		print_err_message("\t\t");
		fprintf(fout, "\t\texit(1);\n");
		fprintf(fout, "\t}\n");
		if (timerflag) {
			fprintf(fout, "\tif (_rpcpmstart) {\n");
			fprintf(fout,
			    "\t\t(void) signal(SIGALRM, %s closedown);\n",
			    Cflag? "(SIG_PF)" : "(void(*)())");
			fprintf(fout, "\t\t(void) alarm(_RPCSVC_CLOSEDOWN);\n");
			fprintf(fout, "\t}\n");
		}
	}
	fprintf(fout, "\tsvc_run();\n");
	(void) snprintf(_errbuf, sizeof _errbuf, "svc_run returned");
	print_err_message("\t");
	fprintf(fout, "\texit(1);\n");
	fprintf(fout, "\t/* NOTREACHED */\n");
	fprintf(fout, "}\n");
}

void
write_programs(storage)
	char *storage;
{
	definition *def;
	list *l;

	/* write out stubs for procedure  definitions */
	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind == DEF_PROGRAM)
			write_real_program(def);
	}

	/* write out dispatcher for each program */
	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind == DEF_PROGRAM)
			write_program(def, storage);
	}
}

/* write out definition of internal function (e.g. _printmsg_1(...))
   which calls server's definition of actual function (e.g. printmsg_1(...)).
   Unpacks single user argument of printmsg_1 to call-by-value format
   expected by printmsg_1. */
static void
write_real_program(def)
	definition *def;
{
	version_list *vp;
	proc_list *proc;
	decl_list *l;

	if (!newstyle) return;  /* not needed for old style */
	for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
		for (proc = vp->procs; proc != NULL; proc = proc->next) {
			fprintf(fout, "\n");
			internal_proctype(proc);
			fprintf(fout, "\n_");
			pvname(proc->proc_name, vp->vers_num);
			if (Cflag) {
				fprintf(fout, "(");
				/* arg name */
				if (proc->arg_num > 1)
					fprintf(fout, "%s", proc->args.argname);
				else
					ptype(proc->args.decls->decl.prefix,
					    proc->args.decls->decl.type, 0);
				fprintf(fout, " *argp, struct svc_req *%s)\n",
				    RQSTP);
			} else {
				fprintf(fout, "(argp, %s)\n", RQSTP);
				/* arg name */
				if (proc->arg_num > 1)
					fprintf(fout, "\t%s *argp;\n",
					    proc->args.argname);
				else {
					fprintf(fout, "\t");
					ptype(proc->args.decls->decl.prefix,
					    proc->args.decls->decl.type, 0);
					fprintf(fout, " *argp;\n");
				}
				fprintf(fout, "	struct svc_req *%s;\n", RQSTP);
			}

			fprintf(fout, "{\n");
			fprintf(fout, "\treturn(");
			pvname_svc(proc->proc_name, vp->vers_num);
			fprintf(fout, "(");
			if (proc->arg_num < 2) { /* single argument */
				if (!streq(proc->args.decls->decl.type, "void"))
					fprintf(fout, "*argp, ");  /* non-void */
			} else {
				for (l = proc->args.decls;  l != NULL; l = l->next)
					fprintf(fout, "argp->%s, ", l->decl.name);
			}
			fprintf(fout, "%s));\n}\n", RQSTP);
		}
	}
}

static void
write_program(def, storage)
	definition *def;
	char *storage;
{
	version_list *vp;
	proc_list *proc;
	int filled;

	for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
		fprintf(fout, "\n");
		if (storage != NULL)
			fprintf(fout, "%s ", storage);
		fprintf(fout, "void\t");
		pvname(def->def_name, vp->vers_num);

		if (Cflag) {
			fprintf(fout, "(struct svc_req *%s, ", RQSTP);
			fprintf(fout, "SVCXPRT *%s);\n", TRANSP);
		} else {
			fprintf(fout, "();\n");
		}
		fprintf(fout, "\n");

		if (storage != NULL)
			fprintf(fout, "%s ", storage);
		fprintf(fout, "void\n");
		pvname(def->def_name, vp->vers_num);

		if (Cflag) {
			fprintf(fout, "(struct svc_req *%s, ", RQSTP);
			fprintf(fout, "SVCXPRT *%s)\n", TRANSP);
		} else {
			fprintf(fout, "(%s, %s)\n", RQSTP, TRANSP);
			fprintf(fout, "    struct svc_req *%s;\n", RQSTP);
			fprintf(fout, "    SVCXPRT *%s;\n", TRANSP);
		}
		fprintf(fout, "{\n");

		filled = 0;
		fprintf(fout, "\tunion {\n");
		for (proc = vp->procs; proc != NULL; proc = proc->next) {
			if (proc->arg_num < 2) { /* single argument */
				if (streq(proc->args.decls->decl.type,
				    "void"))
					continue;
				filled = 1;
				fprintf(fout, "\t\t");
				ptype(proc->args.decls->decl.prefix,
				    proc->args.decls->decl.type, 0);
				pvname(proc->proc_name, vp->vers_num);
				fprintf(fout, "_arg;\n");

			} else {
				filled = 1;
				fprintf(fout, "\t\t%s", proc->args.argname);
				fprintf(fout, " ");
				pvname(proc->proc_name, vp->vers_num);
				fprintf(fout, "_arg;\n");
			}
		}
		if (!filled)
			fprintf(fout, "\t\tint fill;\n");
		fprintf(fout, "\t} %s;\n", ARG);
		fprintf(fout, "\tchar *%s;\n", RESULT);

		if (Cflag) {
			fprintf(fout, "\txdrproc_t xdr_%s, xdr_%s;\n", ARG, RESULT);
			fprintf(fout,
			    "\tchar *(*%s)(char *, struct svc_req *);\n",
			    ROUTINE);
		} else {
			fprintf(fout, "\tbool_t (*xdr_%s)(), (*xdr_%s)();\n",
			    ARG, RESULT);
			fprintf(fout, "\tchar *(*%s)();\n", ROUTINE);
		}
		fprintf(fout, "\n");

		if (callerflag)
			fprintf(fout, "\tcaller = transp;\n"); /*EVAS*/
		if (timerflag)
			fprintf(fout, "\t_rpcsvcdirty = 1;\n");
		fprintf(fout, "\tswitch (%s->rq_proc) {\n", RQSTP);
		if (!nullproc(vp->procs)) {
			fprintf(fout, "\tcase NULLPROC:\n");
			fprintf(fout,
			    Cflag
			    ? "\t\t(void) svc_sendreply(%s, (xdrproc_t) xdr_void, (char *)NULL);\n"
			    : "\t\t(void) svc_sendreply(%s, xdr_void, (char *)NULL);\n",
			    TRANSP);
			print_return("\t\t");
			fprintf(fout, "\n");
		}
		for (proc = vp->procs; proc != NULL; proc = proc->next) {
			fprintf(fout, "\tcase %s:\n", proc->proc_name);
			if (proc->arg_num < 2) { /* single argument */
				p_xdrfunc(ARG, proc->args.decls->decl.type);
			} else {
				p_xdrfunc(ARG, proc->args.argname);
			}
			p_xdrfunc(RESULT, proc->res_type);
			if (Cflag)
				fprintf(fout,
				    "\t\t%s = (char *(*)(char *, struct svc_req *)) ",
				    ROUTINE);
			else
				fprintf(fout, "\t\t%s = (char *(*)()) ", ROUTINE);

			if (newstyle) { /* new style: calls internal routine */
				fprintf(fout,"_");
			}
			if (!newstyle)
				pvname_svc(proc->proc_name, vp->vers_num);
			else
				pvname(proc->proc_name, vp->vers_num);
			fprintf(fout, ";\n");
			fprintf(fout, "\t\tbreak;\n\n");
		}
		fprintf(fout, "\tdefault:\n");
		printerr("noproc", TRANSP);
		print_return("\t\t");
		fprintf(fout, "\t}\n");

		fprintf(fout, "\t(void) memset((char *)&%s, 0, sizeof (%s));\n", ARG, ARG);
		printif ("getargs", TRANSP, "(caddr_t) &", ARG);
		printerr("decode", TRANSP);
		print_return("\t\t");
		fprintf(fout, "\t}\n");

		if (Cflag)
			fprintf(fout, "\t%s = (*%s)((char *)&%s, %s);\n",
			    RESULT, ROUTINE, ARG, RQSTP);
		else
			fprintf(fout, "\t%s = (*%s)(&%s, %s);\n",
			    RESULT, ROUTINE, ARG, RQSTP);
		fprintf(fout,
		    "\tif (%s != NULL && !svc_sendreply(%s, xdr_%s, %s)) {\n",
		    RESULT, TRANSP, RESULT, RESULT);
		printerr("systemerr", TRANSP);
		fprintf(fout, "\t}\n");

		printif ("freeargs", TRANSP, "(caddr_t) &", ARG);
		(void) snprintf(_errbuf, sizeof _errbuf, "unable to free arguments");
		print_err_message("\t\t");
		fprintf(fout, "\t\texit(1);\n");
		fprintf(fout, "\t}\n");
		print_return("\t");
		fprintf(fout, "}\n");
	}
}

static void
printerr(err, transp)
	char *err;
	char *transp;
{
	fprintf(fout, "\t\tsvcerr_%s(%s);\n", err, transp);
}

static void
printif(proc, transp, prefix, arg)
	char *proc;
	char *transp;
	char *prefix;
	char *arg;
{
	fprintf(fout, "\tif (!svc_%s(%s, xdr_%s, %s%s)) {\n",
	    proc, transp, arg, prefix, arg);
}

int
nullproc(proc)
	proc_list *proc;
{
	for (; proc != NULL; proc = proc->next) {
		if (streq(proc->proc_num, "0"))
			return (1);
	}
	return (0);
}

static void
write_inetmost(infile)
	char *infile;
{
	fprintf(fout, "\tSVCXPRT *%s;\n", TRANSP);
	fprintf(fout, "\tint sock;\n");
	fprintf(fout, "\tint proto;\n");
	fprintf(fout, "\tstruct sockaddr_in saddr;\n");
	fprintf(fout, "\tint asize = sizeof (saddr);\n");
	fprintf(fout, "\n");
	fprintf(fout,
	"\tif (getsockname(0, (struct sockaddr *)&saddr, &asize) == 0) {\n");
	fprintf(fout, "\t\tint ssize = sizeof (int);\n\n");
	fprintf(fout, "\t\tif (saddr.sin_family != AF_INET)\n");
	fprintf(fout, "\t\t\texit(1);\n");
	fprintf(fout, "\t\tif (getsockopt(0, SOL_SOCKET, SO_TYPE,\n");
	fprintf(fout, "\t\t    (char *)&_rpcfdtype, &ssize) == -1)\n");
	fprintf(fout, "\t\t\texit(1);\n");
	fprintf(fout, "\t\tsock = 0;\n");
	fprintf(fout, "\t\t_rpcpmstart = 1;\n");
	fprintf(fout, "\t\tproto = 0;\n");
	open_log_file(infile, "\t\t");
	fprintf(fout, "\t} else {\n");
	write_rpc_svc_fg(infile, "\t\t");
	fprintf(fout, "\t\tsock = RPC_ANYSOCK;\n");
	print_pmapunset("\t\t");
	fprintf(fout, "\t}\n");
}

static void
print_return(space)
	char *space;
{
	if (exitnow)
		fprintf(fout, "%sexit(0);\n", space);
	else {
		if (timerflag)
			fprintf(fout, "%s_rpcsvcdirty = 0;\n", space);
		fprintf(fout, "%sreturn;\n", space);
	}
}

static void
print_pmapunset(space)
	char *space;
{
	version_list *vp;
	definition *def;
	list *l;

	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind == DEF_PROGRAM) {
			for (vp = def->def.pr.versions; vp != NULL;
			    vp = vp->next) {
				fprintf(fout, "%s(void) pmap_unset(%s, %s);\n",
				    space, def->def_name, vp->vers_name);
			}
		}
	}
}

static void
print_err_message(space)
	char *space;
{
	if (logflag)
		fprintf(fout, "%ssyslog(LOG_ERR, \"%%s\", \"%s\");\n", space, _errbuf);
	else if (inetdflag || pmflag)
		fprintf(fout, "%s_msgout(\"%s\");\n", space, _errbuf);
	else
		fprintf(fout, "%sfprintf(stderr, \"%s\");\n", space, _errbuf);
}

/*
 * Write the server auxiliary function (_msgout, timeout)
 */
void
write_svc_aux(nomain)
	int nomain;
{
	if (!logflag)
		write_msg_out();
	if (!nomain)
		write_timeout_func();
	if (callerflag)			/*EVAS*/
		write_caller_func();	/*EVAS*/
}

/*
 * Write the _msgout function
 */

void
write_msg_out()
{
	fprintf(fout, "\n");
	fprintf(fout, "static\n");
	if (!Cflag) {
		fprintf(fout, "void _msgout(msg)\n");
		fprintf(fout, "\tchar *msg;\n");
	} else {
		fprintf(fout, "void _msgout(char *msg)\n");
	}
	fprintf(fout, "{\n");
	fprintf(fout, "#ifdef RPC_SVC_FG\n");
	if (inetdflag || pmflag)
		fprintf(fout, "\tif (_rpcpmstart)\n");
	fprintf(fout, "\t\tsyslog(LOG_ERR, \"%%s\", msg);\n");
	fprintf(fout, "\telse {\n");
	fprintf(fout, "\t\t(void) write(STDERR_FILENO, msg, strlen(msg));\n");
	fprintf(fout, "\t\t(void) write(STDERR_FILENO, \"\\n\", 1);\n");
	fprintf(fout, "\t}\n#else\n");
	fprintf(fout, "\tsyslog(LOG_ERR, \"%%s\", msg);\n");
	fprintf(fout, "#endif\n");
	fprintf(fout, "}\n");
}

/*
 * Write the timeout function
 */
static void
write_timeout_func()
{
	if (!timerflag)
		return;
	fprintf(fout, "\n");
	fprintf(fout, "static void\n");
	fprintf(fout, "closedown()\n");
	fprintf(fout, "{\n");
	fprintf(fout, "\tint save_errno = errno;\n\n");
	fprintf(fout, "\tif (_rpcsvcdirty == 0) {\n");
	fprintf(fout, "\t\textern fd_set *__svc_fdset;\n");
	fprintf(fout, "\t\textern int __svc_fdsetsize;\n");
	fprintf(fout, "\t\tint i, openfd;\n");
	if (tirpcflag && pmflag) {
		fprintf(fout, "\t\tstruct t_info tinfo;\n\n");
		fprintf(fout, "\t\tif (!t_getinfo(0, &tinfo) && (tinfo.servtype == T_CLTS))\n");
	} else {
		fprintf(fout, "\n\t\tif (_rpcfdtype == SOCK_DGRAM)\n");
	}
	fprintf(fout, "\t\t\t_exit(0);\n");
	fprintf(fout, "\t\tfor (i = 0, openfd = 0; i < __svc_fdsetsize && openfd < 2; i++)\n");
	fprintf(fout, "\t\t\tif (FD_ISSET(i, __svc_fdset))\n");
	fprintf(fout, "\t\t\t\topenfd++;\n");
	fprintf(fout, "\t\tif (openfd <= (_rpcpmstart?0:1))\n");
	fprintf(fout, "\t\t\t_exit(0);\n");
	fprintf(fout, "\t}\n");
	fprintf(fout, "\t(void) alarm(_RPCSVC_CLOSEDOWN);\n");
	fprintf(fout, "\terrno = save_errno;\n");
	fprintf(fout, "}\n");
}

static void
write_caller_func()			/*EVAS*/
{
#define	P(s)	fprintf(fout, s);

P("\n");
P("char *svc_caller()\n");
P("{\n");
P("	struct sockaddr_in actual;\n");
P("	struct hostent *hp;\n");
P("	static struct in_addr prev;\n");
P("	static char cname[256];\n\n");

P("	actual = *svc_getcaller(caller);\n\n");

P("	if (memcmp((char *)&actual.sin_addr, (char *)&prev,\n");
P("		 sizeof(struct in_addr)) == 0)\n");
P("		return (cname);\n\n");

P("	prev = actual.sin_addr;\n\n");

P("	hp = gethostbyaddr((char *) &actual.sin_addr, sizeof(actual.sin_addr), AF_INET);\n");
P("	if (hp == NULL) {                       /* dummy one up */\n");
P("		extern char *inet_ntoa();\n");
P("		strlcpy(cname, inet_ntoa(actual.sin_addr), sizeof cname);\n");
P("	} else {\n");
P("		strlcpy(cname, hp->h_name, sizeof cname);\n");
P("	}\n\n");

P("	return (cname);\n");
P("}\n");

#undef P
}

/*
 * Write the most of port monitor support
 */
static void
write_pm_most(infile, netflag)
	char *infile;
	int netflag;
{
	list *l;
	definition *def;
	version_list *vp;

	fprintf(fout, "\tif (!ioctl(0, I_LOOK, mname) &&\n");
	fprintf(fout, "\t\t(!strcmp(mname, \"sockmod\") ||");
	fprintf(fout, " !strcmp(mname, \"timod\"))) {\n");
	fprintf(fout, "\t\tchar *netid;\n");
	if (!netflag) {	/* Not included by -n option */
		fprintf(fout, "\t\tstruct netconfig *nconf = NULL;\n");
		fprintf(fout, "\t\tSVCXPRT *%s;\n", TRANSP);
	}
	if (timerflag)
		fprintf(fout, "\t\tint pmclose;\n");
/* not necessary, defined in /usr/include/stdlib */
/*	fprintf(fout, "\t\textern char *getenv();\n");*/
	fprintf(fout, "\n");
	fprintf(fout, "\t\t_rpcpmstart = 1;\n");
	if (logflag)
		open_log_file(infile, "\t\t");
	fprintf(fout, "\t\tif ((netid = getenv(\"NLSPROVIDER\")) == NULL) {\n");
	snprintf(_errbuf, sizeof _errbuf, "cannot get transport name");
	print_err_message("\t\t\t");
	fprintf(fout, "\t\t} else if ((nconf = getnetconfigent(netid)) == NULL) {\n");
	snprintf(_errbuf, sizeof _errbuf, "cannot get transport info");
	print_err_message("\t\t\t");
	fprintf(fout, "\t\t}\n");
	/*
	 * A kludgy support for inetd services. Inetd only works with
	 * sockmod, and RPC works only with timod, hence all this jugglery
	 */
	fprintf(fout, "\t\tif (strcmp(mname, \"sockmod\") == 0) {\n");
	fprintf(fout, "\t\t\tif (ioctl(0, I_POP, 0) || ioctl(0, I_PUSH, \"timod\")) {\n");
	snprintf(_errbuf, sizeof _errbuf, "could not get the right module");
	print_err_message("\t\t\t\t");
	fprintf(fout, "\t\t\t\texit(1);\n");
	fprintf(fout, "\t\t\t}\n");
	fprintf(fout, "\t\t}\n");
	if (timerflag)
		fprintf(fout, "\t\tpmclose = (t_getstate(0) != T_DATAXFER);\n");
	fprintf(fout, "\t\tif ((%s = svc_tli_create(0, nconf, NULL, 0, 0)) == NULL) {\n",
			TRANSP);
	snprintf(_errbuf, sizeof _errbuf, "cannot create server handle");
	print_err_message("\t\t\t");
	fprintf(fout, "\t\t\texit(1);\n");
	fprintf(fout, "\t\t}\n");
	fprintf(fout, "\t\tif (nconf)\n");
	fprintf(fout, "\t\t\tfreenetconfigent(nconf);\n");
	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind != DEF_PROGRAM) {
			continue;
		}
		for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
			fprintf(fout,
				"\t\tif (!svc_reg(%s, %s, %s, ",
				TRANSP, def->def_name, vp->vers_name);
			pvname(def->def_name, vp->vers_num);
			fprintf(fout, ", 0)) {\n");
			(void) snprintf(_errbuf, sizeof _errbuf, "unable to register (%s, %s).",
					def->def_name, vp->vers_name);
			print_err_message("\t\t\t");
			fprintf(fout, "\t\t\texit(1);\n");
			fprintf(fout, "\t\t}\n");
		}
	}
	if (timerflag) {
		fprintf(fout, "\t\tif (pmclose) {\n");
		fprintf(fout, "\t\t\t(void) signal(SIGALRM, %s closedown);\n",
				Cflag? "(SIG_PF)" : "(void(*)())");
		fprintf(fout, "\t\t\t(void) alarm(_RPCSVC_CLOSEDOWN);\n");
		fprintf(fout, "\t\t}\n");
	}
	fprintf(fout, "\t\tsvc_run();\n");
	fprintf(fout, "\t\texit(1);\n");
	fprintf(fout, "\t\t/* NOTREACHED */\n");
	fprintf(fout, "\t}\n");
}

/*
 * Support for backgrounding the server if self started.
 */
static void
write_rpc_svc_fg(infile, sp)
	char *infile;
	char *sp;
{
	fprintf(fout, "#ifndef RPC_SVC_FG\n");
	fprintf(fout, "%sint size;\n", sp);
	if (tirpcflag)
		fprintf(fout, "%sstruct rlimit rl;\n", sp);
	if (inetdflag) {
		fprintf(fout, "%sint i;\n\n", sp);
		fprintf(fout, "%spid_t pid;\n\n", sp);
	}
	fprintf(fout, "%spid = fork();\n", sp);
	fprintf(fout, "%sif (pid < 0) {\n", sp);
	fprintf(fout, "%s\tperror(\"cannot fork\");\n", sp);
	fprintf(fout, "%s\texit(1);\n", sp);
	fprintf(fout, "%s}\n", sp);
	fprintf(fout, "%sif (pid)\n", sp);
	fprintf(fout, "%s\texit(0);\n", sp);
	/* get number of file descriptors */
	if (tirpcflag) {
		fprintf(fout, "%srl.rlim_max = 0;\n", sp);
		fprintf(fout, "%sgetrlimit(RLIMIT_NOFILE, &rl);\n", sp);
		fprintf(fout, "%sif ((size = rl.rlim_max) == 0)\n", sp);
		fprintf(fout, "%s\texit(1);\n", sp);
	} else {
		fprintf(fout, "%ssize = getdtablesize();\n", sp);
	}

	fprintf(fout, "%sfor (i = 0; i < size; i++)\n", sp);
	fprintf(fout, "%s\t(void) close(i);\n", sp);
	/* Redirect stderr and stdout to console */
	fprintf(fout, "%si = open(\"/dev/console\", 2);\n", sp);
	fprintf(fout, "%s(void) dup2(i, 1);\n", sp);
	fprintf(fout, "%s(void) dup2(i, 2);\n", sp);
	/* This removes control of the controlling terminal */
	if (tirpcflag)
		fprintf(fout, "%ssetsid();\n", sp);
	else {
		fprintf(fout, "%si = open(\"/dev/tty\", 2);\n", sp);
		fprintf(fout, "%sif (i >= 0) {\n", sp);
		fprintf(fout, "%s\t(void) ioctl(i, TIOCNOTTY, (char *)NULL);\n", sp);
		fprintf(fout, "%s\t(void) close(i);\n", sp);
		fprintf(fout, "%s}\n", sp);
	}
	if (!logflag)
		open_log_file(infile, sp);
	fprintf(fout, "#endif\n");
	if (logflag)
		open_log_file(infile, sp);
}

static void
open_log_file(infile, sp)
	char *infile;
	char *sp;
{
	char *s;

	s = strrchr(infile, '.');
	if (s)
		*s = '\0';
	fprintf(fout,"%sopenlog(\"%s\", LOG_PID, LOG_DAEMON);\n", sp, infile);
	if (s)
		*s = '.';
}

/*
 * write a registration for the given transport for Inetd
 */
void
write_inetd_register(transp)
	char *transp;
{
	list *l;
	definition *def;
	version_list *vp;
	char *sp;
	int isudp;
	char tmpbuf[32];

	if (inetdflag)
		sp = "\t";
	else
		sp = "";
	if (streq(transp, "udp"))
		isudp = 1;
	else
		isudp = 0;
	fprintf(fout, "\n");
	if (inetdflag) {
		fprintf(fout, "\tif (_rpcfdtype == 0 || _rpcfdtype == %s) {\n",
				isudp ? "SOCK_DGRAM" : "SOCK_STREAM");
	}
	if (inetdflag && streq(transp, "tcp")) {
		fprintf(fout, "%s\tif (_rpcpmstart)\n", sp);

		fprintf(fout, "%s\t\t%s = svc%s_create(%s",
			sp, TRANSP, "fd", inetdflag? "sock": "RPC_ANYSOCK");
		if (!isudp)
			fprintf(fout, ", 0, 0");
		fprintf(fout, ");\n");

		fprintf(fout, "%s\telse\n", sp);

		fprintf(fout, "%s\t\t%s = svc%s_create(%s",
			sp, TRANSP, transp, inetdflag? "sock": "RPC_ANYSOCK");
		if (!isudp)
			fprintf(fout, ", 0, 0");
		fprintf(fout, ");\n");

	} else {
		fprintf(fout, "%s\t%s = svc%s_create(%s",
			sp, TRANSP, transp, inetdflag? "sock": "RPC_ANYSOCK");
		if (!isudp)
			fprintf(fout, ", 0, 0");
		fprintf(fout, ");\n");
	}
	fprintf(fout, "%s\tif (%s == NULL) {\n", sp, TRANSP);
	(void) snprintf(_errbuf, sizeof _errbuf, "cannot create %s service.", transp);
	(void) snprintf(tmpbuf, sizeof tmpbuf, "%s\t\t", sp);
	print_err_message(tmpbuf);
	fprintf(fout, "%s\t\texit(1);\n", sp);
	fprintf(fout, "%s\t}\n", sp);

	if (inetdflag) {
		fprintf(fout, "%s\tif (!_rpcpmstart)\n\t", sp);
		fprintf(fout, "%s\tproto = IPPROTO_%s;\n",
				sp, isudp ? "UDP": "TCP");
	}
	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind != DEF_PROGRAM) {
			continue;
		}
		for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
			fprintf(fout, "%s\tif (!svc_register(%s, %s, %s, ",
				sp, TRANSP, def->def_name, vp->vers_name);
			pvname(def->def_name, vp->vers_num);
			if (inetdflag)
				fprintf(fout, ", proto)) {\n");
			else
				fprintf(fout, ", IPPROTO_%s)) {\n",
					isudp ? "UDP": "TCP");
			(void) snprintf(_errbuf, sizeof _errbuf, "unable to register (%s, %s, %s).",
					def->def_name, vp->vers_name, transp);
			print_err_message(tmpbuf);
			fprintf(fout, "%s\t\texit(1);\n", sp);
			fprintf(fout, "%s\t}\n", sp);
		}
	}
	if (inetdflag)
		fprintf(fout, "\t}\n");
}
