/*	$OpenBSD: rpc_sample.c,v 1.17 2010/09/01 14:43:34 millert Exp $	*/
/*	$NetBSD: rpc_sample.c,v 1.2 1995/06/11 21:50:01 pk Exp $	*/

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
 * rpc_sample.c, Sample client-server code outputter for the RPC protocol compiler
 */

#include <sys/cdefs.h>
#include <stdio.h>
#include <string.h>
#include "rpc_parse.h"
#include "rpc_util.h"

static char RQSTP[] = "rqstp";

static void write_sample_client(char *, version_list *);
static void write_sample_server(definition *);
static void return_type(proc_list *);

void
write_sample_svc(def)
	definition *def;
{

	if (def->def_kind != DEF_PROGRAM)
		return;
	write_sample_server(def);
}


int
write_sample_clnt(def)
	definition *def;
{
	version_list *vp;
	int count = 0;

	if (def->def_kind != DEF_PROGRAM)
		return(0);
	/* generate sample code for each version */
	for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
		write_sample_client(def->def_name, vp);
		++count;
	}
	return(count);
}


static void
write_sample_client(program_name, vp)
	char *program_name;
	version_list *vp;
{
	proc_list *proc;
	int i;
	decl_list *l;

	fprintf(fout, "\n\nvoid\n");
	pvname(program_name, vp->vers_num);
	if (Cflag)
		fprintf(fout,"(char *host)\n{\n");
	else
		fprintf(fout, "(host)\nchar *host;\n{\n");
	fprintf(fout, "\tCLIENT *clnt;\n");

	i = 0;
	for (proc = vp->procs; proc != NULL; proc = proc->next) {
		fprintf(fout, "\t");
		ptype(proc->res_prefix, proc->res_type, 1);
		fprintf(fout, " *result_%d;\n",++i);
		/* print out declarations for arguments */
		if (proc->arg_num < 2 && !newstyle) {
			fprintf(fout, "\t");
			if (!streq(proc->args.decls->decl.type, "void"))
				ptype(proc->args.decls->decl.prefix,
				    proc->args.decls->decl.type, 1);
			else
				fprintf(fout, "char *"); /* cannot have "void" type */
			fprintf(fout, " ");
			pvname(proc->proc_name, vp->vers_num);
			fprintf(fout, "_arg;\n");
		} else if (!streq(proc->args.decls->decl.type, "void")) {
			for (l = proc->args.decls; l != NULL; l = l->next) {
				fprintf(fout, "\t");
				ptype(l->decl.prefix, l->decl.type, 1);
				fprintf(fout, " ");
				pvname(proc->proc_name, vp->vers_num);
				fprintf(fout, "_%s;\n", l->decl.name);
		/*		pdeclaration(proc->args.argname, &l->decl, 1, ";\n");*/
			}
		}
	}

	/* generate creation of client handle */
	fprintf(fout, "\tclnt = clnt_create(host, %s, %s, \"%s\");\n",
	    program_name, vp->vers_name, tirpcflag? "netpath" : "udp");
	fprintf(fout, "\tif (clnt == NULL) {\n");
	fprintf(fout, "\t\tclnt_pcreateerror(host);\n");
	fprintf(fout, "\t\texit(1);\n\t}\n");

	/* generate calls to procedures */
	i = 0;
	for (proc = vp->procs; proc != NULL; proc = proc->next) {
		fprintf(fout, "\tresult_%d = ",++i);
		pvname(proc->proc_name, vp->vers_num);
		if (proc->arg_num < 2 && !newstyle) {
			fprintf(fout, "(");
			if (streq(proc->args.decls->decl.type, "void"))
				fprintf(fout, "(void*)");
			fprintf(fout, "&");
			pvname(proc->proc_name, vp->vers_num);
			fprintf(fout, "_arg, clnt);\n");
		} else if (streq(proc->args.decls->decl.type, "void")) {
			fprintf(fout, "(clnt);\n");
		} else {
			fprintf(fout, "(");
			for (l = proc->args.decls;	l != NULL; l = l->next) {
				pvname(proc->proc_name, vp->vers_num);
				fprintf(fout, "_%s, ", l->decl.name);
			}
			fprintf(fout, "clnt);\n");
		}
		fprintf(fout, "\tif (result_%d == NULL) {\n", i);
		fprintf(fout, "\t\tclnt_perror(clnt, \"call failed:\");\n");
		fprintf(fout, "\t}\n");
	}

	fprintf(fout, "\tclnt_destroy(clnt);\n");
	fprintf(fout, "}\n");
}

static void
write_sample_server(def)
	definition *def;
{
	version_list *vp;
	proc_list *proc;

	for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
		for (proc = vp->procs; proc != NULL; proc = proc->next) {
			fprintf(fout, "\n");
/*			if (Cflag)
				fprintf(fout, "extern \"C\"{\n");
*/
			return_type(proc);
			fprintf(fout, "* \n");
			pvname_svc(proc->proc_name, vp->vers_num);
			printarglist(proc, RQSTP, "struct svc_req *");

			fprintf(fout, "{\n");
			fprintf(fout, "\n\tstatic ");
			if (!streq(proc->res_type, "void"))
				return_type(proc);
			else
				fprintf(fout, "char*");  /* cannot have void type */
			fprintf(fout, " result;\n");
			fprintf(fout,
			    "\n\t/*\n\t * insert server code here\n\t */\n\n");
			if (!streq(proc->res_type, "void"))
				fprintf(fout, "\treturn(&result);\n}\n");
			else  /* cast back to void * */
				fprintf(fout, "\treturn((void*) &result);\n}\n");
/*			if (Cflag)
				fprintf(fout, "}\n");
*/
		}
	}
}

static void
return_type(plist)
	proc_list *plist;
{
	ptype(plist->res_prefix, plist->res_type, 1);
}

void
add_sample_msg(void)
{
	fprintf(fout, "/*\n");
	fprintf(fout, " * This is sample code generated by rpcgen.\n");
	fprintf(fout, " * These are only templates and you can use them\n");
	fprintf(fout, " * as a guideline for developing your own functions.\n");
	fprintf(fout, " */\n\n");
}

void
write_sample_clnt_main()
{
	list *l;
	definition *def;
	version_list *vp;

	fprintf(fout, "\n\n");
	if (Cflag)
		fprintf(fout,"main(int argc, char *argv[])\n{\n");
	else
		fprintf(fout, "main(argc, argv)\nint argc;\nchar *argv[];\n{\n");

	fprintf(fout, "\tchar *host;");
	fprintf(fout, "\n\n\tif (argc < 2) {");
	fprintf(fout, "\n\t\tprintf(\"usage: %%s server_host\\n\", argv[0]);\n");
	fprintf(fout, "\t\texit(1);\n\t}");
	fprintf(fout, "\n\thost = argv[1];\n");

	for (l = defined; l != NULL; l = l->next) {
		def = l->val;
		if (def->def_kind != DEF_PROGRAM)
			continue;
		for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
			fprintf(fout, "\t");
			pvname(def->def_name, vp->vers_num);
			fprintf(fout, "(host);\n");
		}
	}
	fprintf(fout, "}\n");
}
