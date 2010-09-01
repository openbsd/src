/*	$OpenBSD: rpc_clntout.c,v 1.15 2010/09/01 14:43:34 millert Exp $	*/
/*	$NetBSD: rpc_clntout.c,v 1.4 1995/06/11 21:49:52 pk Exp $	*/

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
 * rpc_clntout.c, Client-stub outputter for the RPC protocol compiler
 */
#include <stdio.h>
#include <string.h>
#include <rpc/types.h>
#include "rpc_parse.h"
#include "rpc_util.h"

static void write_program(definition *);
static void printbody(proc_list *);

#define DEFAULT_TIMEOUT 25	/* in seconds */
static char RESULT[] = "clnt_res";


void
write_stubs()
{
	list *l;
	definition *def;

	fprintf(fout,
		"\n/* Default timeout can be changed using clnt_control() */\n");
	fprintf(fout, "static struct timeval TIMEOUT = { %d, 0 };\n",
		DEFAULT_TIMEOUT);
	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind == DEF_PROGRAM) {
			write_program(def);
		}
	}
}

static void
write_program(def)
	definition *def;
{
	version_list *vp;
	proc_list *proc;

	for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
		for (proc = vp->procs; proc != NULL; proc = proc->next) {
			fprintf(fout, "\n");
			ptype(proc->res_prefix, proc->res_type, 1);
			fprintf(fout, "*\n");
			pvname(proc->proc_name, vp->vers_num);
			printarglist(proc, "clnt", "CLIENT *");
			fprintf(fout, "{\n");
			printbody(proc);
			fprintf(fout, "}\n");
		}
	}
}

/*
 * Writes out declarations of procedure's argument list.
 * In either ANSI C style, in one of old rpcgen style (pass by reference),
 * or new rpcgen style (multiple arguments, pass by value);
 */

/* sample addargname = "clnt"; sample addargtype = "CLIENT * " */

void printarglist(proc, addargname, addargtype)
	proc_list *proc;
	char *addargname, *addargtype;
{
	decl_list *l;

	if (!newstyle) {	/* old style: always pass argument by reference */
		if (Cflag) {			/* C++ style heading */
			fprintf(fout, "(");
			ptype(proc->args.decls->decl.prefix,
			    proc->args.decls->decl.type, 1);
			fprintf(fout, "*argp, %s%s)\n", addargtype, addargname);
		} else {
			fprintf(fout, "(argp, %s)\n", addargname);
			fprintf(fout, "\t");
			ptype(proc->args.decls->decl.prefix,
			    proc->args.decls->decl.type, 1);
			fprintf(fout, "*argp;\n");
		}
	} else if (streq(proc->args.decls->decl.type, "void")) {
		/* newstyle, 0 argument */
		if (Cflag)
			fprintf(fout, "(%s%s)\n", addargtype, addargname);
		else
			fprintf(fout, "(%s)\n", addargname);
	} else {
		/* new style, 1 or multiple arguments */
		if (!Cflag) {
			fprintf(fout, "(");
			for (l = proc->args.decls; l != NULL; l = l->next)
				fprintf(fout, "%s, ", l->decl.name);
			fprintf(fout, "%s)\n", addargname);
			for (l = proc->args.decls; l != NULL; l = l->next)
				pdeclaration(proc->args.argname, &l->decl, 1, ";\n");
		} else {	/* C++ style header */
			fprintf(fout, "(");
			for (l = proc->args.decls; l != NULL; l = l->next)
				pdeclaration(proc->args.argname, &l->decl, 0, ", ");
			fprintf(fout, " %s%s)\n", addargtype, addargname);
		}
	}

	if (!Cflag)
		fprintf(fout, "\t%s%s;\n", addargtype, addargname);
}

static char *
ampr(char *type)
{
	if (isvectordef(type, REL_ALIAS)) {
		return ("");
	} else {
		return ("&");
	}
}

static void
printbody(proc)
	proc_list *proc;
{
	decl_list *l;
	bool_t args2 = (proc->arg_num > 1);

	/*
	 * For new style with multiple arguments, need a structure in which
	 * to stuff the arguments.
	 */
	if (newstyle && args2) {
		fprintf(fout, "\t%s", proc->args.argname);
		fprintf(fout, " arg;\n");
	}
	fprintf(fout, "\tstatic ");
	if (streq(proc->res_type, "void")) {
		fprintf(fout, "char ");
	} else {
		ptype(proc->res_prefix, proc->res_type, 0);
	}
	fprintf(fout, "%s;\n",RESULT);
	fprintf(fout, "\n");
	fprintf(fout, "\tmemset((char *)%s%s, 0, sizeof(%s));\n",
	    ampr(proc->res_type), RESULT, RESULT);
	if (newstyle && !args2 && (streq(proc->args.decls->decl.type, "void"))) {
		/* newstyle, 0 arguments */
		fprintf(fout,
		    "\tif (clnt_call(clnt, %s, xdr_void", proc->proc_name);
		fprintf(fout,
		    ", NULL, xdr_%s, %s%s, TIMEOUT) != RPC_SUCCESS) {\n",
		    stringfix(proc->res_type), ampr(proc->res_type), RESULT);
	} else if (newstyle && args2) {
		/* newstyle, multiple arguments:  stuff arguments into structure */
		for (l = proc->args.decls;  l != NULL; l = l->next) {
			fprintf(fout, "\targ.%s = %s;\n",
			    l->decl.name, l->decl.name);
		}
		fprintf(fout,
		    "\tif (clnt_call(clnt, %s, xdr_%s", proc->proc_name,
		    proc->args.argname);
		fprintf(fout,
		    ", &arg, xdr_%s, %s%s, TIMEOUT) != RPC_SUCCESS) {\n",
		    stringfix(proc->res_type),
		    ampr(proc->res_type), RESULT);
	} else {  /* single argument, new or old style */
		fprintf(fout,
		    "\tif (clnt_call(clnt, %s, xdr_%s, %s%s, xdr_%s, "
		    "%s%s, TIMEOUT) != RPC_SUCCESS) {\n",
		    proc->proc_name,
		    stringfix(proc->args.decls->decl.type),
		    (newstyle ? "&" : ""),
		    (newstyle ? proc->args.decls->decl.name : "argp"),
		    stringfix(proc->res_type),
		    ampr(proc->res_type),RESULT);
	}
	fprintf(fout, "\t\treturn (NULL);\n");
	fprintf(fout, "\t}\n");
	if (streq(proc->res_type, "void")) {
		fprintf(fout, "\treturn ((void *)%s%s);\n",
		    ampr(proc->res_type),RESULT);
	} else {
		fprintf(fout, "\treturn (%s%s);\n", ampr(proc->res_type),RESULT);
	}
}

