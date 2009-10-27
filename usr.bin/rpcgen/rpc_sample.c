/*	$OpenBSD: rpc_sample.c,v 1.16 2009/10/27 23:59:42 deraadt Exp $	*/
/*	$NetBSD: rpc_sample.c,v 1.2 1995/06/11 21:50:01 pk Exp $	*/
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
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
