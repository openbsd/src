/*	$OpenBSD: rpc_hout.c,v 1.20 2011/04/06 11:36:26 miod Exp $	*/
/*	$NetBSD: rpc_hout.c,v 1.4 1995/06/11 21:49:55 pk Exp $	*/

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
 * rpc_hout.c, Header file outputter for the RPC protocol compiler
 */
#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "rpc_parse.h"
#include "rpc_util.h"

static void pconstdef(definition *);
static void pargdef(definition *);
static void pstructdef(definition *);
static void puniondef(definition *);
static void pprogramdef(definition *);
static void penumdef(definition *);
static void ptypedef(definition *);
static void pdefine(char *, char *);
static void puldefine(char *, char *);
static int define_printed(proc_list *, version_list *);
static int undefined2(char *, char *);
static void parglist(proc_list *, char *);
void pxdrfuncdecl(char *, int);
void pprocdef(proc_list *, version_list *, char *, int, int);
void pdeclaration(char *, declaration *, int, char *);

/*
 * Print the C-version of an xdr definition
 */
void
print_datadef(def)
	definition *def;
{

	if (def->def_kind == DEF_PROGRAM)  /* handle data only */
		return;

	if (def->def_kind != DEF_CONST)
		fprintf(fout, "\n");
	switch (def->def_kind) {
	case DEF_STRUCT:
		pstructdef(def);
		break;
	case DEF_UNION:
		puniondef(def);
		break;
	case DEF_ENUM:
		penumdef(def);
		break;
	case DEF_TYPEDEF:
		ptypedef(def);
		break;
	case DEF_PROGRAM:
		pprogramdef(def);
		break;
	case DEF_CONST:
		pconstdef(def);
		break;
	}
	if (def->def_kind != DEF_PROGRAM && def->def_kind != DEF_CONST) {
		pxdrfuncdecl(def->def_name,
		    def->def_kind != DEF_TYPEDEF ||
		    !isvectordef(def->def.ty.old_type, def->def.ty.rel));
	}
}


void
print_funcdef(def)
	definition *def;
{
	switch (def->def_kind) {
	case DEF_PROGRAM:
		fprintf(fout, "\n");
		pprogramdef(def);
		break;
	}
}

void
pxdrfuncdecl(name, pointerp)
	char *name;
	int pointerp;
{

	fprintf(fout,"#ifdef __cplusplus\n");
	fprintf(fout, "extern \"C\" bool_t xdr_%s(XDR *, %s %s);\n",
	    name, name, pointerp ? ("*") : "");
	fprintf(fout,"#elif defined(__STDC__)\n");
	fprintf(fout, "extern bool_t xdr_%s(XDR *, %s %s);\n",
	    name, name, pointerp ? ("*") : "");
	fprintf(fout,"#else /* Old Style C */\n");
	fprintf(fout, "bool_t xdr_%s();\n", name);
	fprintf(fout,"#endif /* Old Style C */\n\n");
}


static void
pconstdef(def)
	definition *def;
{
	pdefine(def->def_name, def->def.co);
}

/*
 * print out the definitions for the arguments of functions in the
 * header file
 */
static void
pargdef(def)
	definition *def;
{
	decl_list *l;
	version_list *vers;
	char *name;
	proc_list *plist;

	for (vers = def->def.pr.versions; vers != NULL; vers = vers->next) {
		for (plist = vers->procs; plist != NULL;
		    plist = plist->next) {
			if (!newstyle || plist->arg_num < 2) {
				continue; /* old style or single args */
			}
			name = plist->args.argname;
			fprintf(fout, "struct %s {\n", name);
			for (l = plist->args.decls;
			    l != NULL; l = l->next) {
				pdeclaration(name, &l->decl, 1, ";\n");
			}
			fprintf(fout, "};\n");
			fprintf(fout, "typedef struct %s %s;\n", name, name);
			pxdrfuncdecl(name, 0);
			fprintf(fout, "\n");
		}
	}
}

static void
pstructdef(def)
	definition *def;
{
	char *name = def->def_name;
	decl_list *l;

	fprintf(fout, "struct %s {\n", name);
	for (l = def->def.st.decls; l != NULL; l = l->next)
		pdeclaration(name, &l->decl, 1, ";\n");
	fprintf(fout, "};\n");
	fprintf(fout, "typedef struct %s %s;\n", name, name);
}

static void
puniondef(def)
	definition *def;
{
	case_list *l;
	char *name = def->def_name;
	declaration *decl;

	fprintf(fout, "struct %s {\n", name);
	decl = &def->def.un.enum_decl;
	if (streq(decl->type, "bool")) {
		fprintf(fout, "\tbool_t %s;\n", decl->name);
	} else {
		fprintf(fout, "\t%s %s;\n", decl->type, decl->name);
	}
	fprintf(fout, "\tunion {\n");
	for (l = def->def.un.cases; l != NULL; l = l->next) {
	  if (l->contflag == 0)
		pdeclaration(name, &l->case_decl, 2, ";\n");
	}
	decl = def->def.un.default_decl;
	if (decl && !streq(decl->type, "void")) {
		pdeclaration(name, decl, 2, ";\n");
	}
	fprintf(fout, "\t} %s_u;\n", name);
	fprintf(fout, "};\n");
	fprintf(fout, "typedef struct %s %s;\n", name, name);
}

static void
pdefine(name, num)
	char *name;
	char *num;
{
	fprintf(fout, "#define %s %s\n", name, num);
}

static void
puldefine(name, num)
	char *name;
	char *num;
{
	fprintf(fout, "#define %s ((u_long)%s)\n", name, num);
}

static int
define_printed(stop, start)
	proc_list *stop;
	version_list *start;
{
	version_list *vers;
	proc_list *proc;

	for (vers = start; vers != NULL; vers = vers->next) {
		for (proc = vers->procs; proc != NULL; proc = proc->next) {
			if (proc == stop) {
				return (0);
			} else if (streq(proc->proc_name, stop->proc_name)) {
				return (1);
			}
		}
	}
	abort();
	/* NOTREACHED */
}

static void
pprogramdef(def)
	definition *def;
{
	version_list *vers;
	proc_list *proc;
	int i;
	char *ext;

	pargdef(def);

	puldefine(def->def_name, def->def.pr.prog_num);
	for (vers = def->def.pr.versions; vers != NULL; vers = vers->next) {
		if (tblflag) {
			fprintf(fout, "extern struct rpcgen_table %s_%s_table[];\n",
			    locase(def->def_name), vers->vers_num);
			fprintf(fout, "extern %s_%s_nproc;\n",
			    locase(def->def_name), vers->vers_num);
		}
		puldefine(vers->vers_name, vers->vers_num);

		/*
		 * Print out 3 definitions, one for ANSI-C, another for C++,
		 * a third for old style C
		 */
		for (i=0; i<3; i++) {
			if (i==0) {
				fprintf(fout,"\n#ifdef __cplusplus\n");
				ext = "extern \"C\" ";
			} else if (i==1) {
				fprintf(fout,"\n#elif defined(__STDC__)\n");
				ext = "extern ";
			} else {
				fprintf(fout,"\n#else /* Old Style C */\n");
				ext = "extern ";
			}

			for (proc = vers->procs; proc != NULL; proc = proc->next) {
				if (!define_printed(proc, def->def.pr.versions))
					puldefine(proc->proc_name, proc->proc_num);
				fprintf(fout,"%s",ext);
				pprocdef(proc, vers, "CLIENT *", 0,i);
				fprintf(fout,"%s",ext);
				pprocdef(proc, vers, "struct svc_req *", 1,i);
			}
		}
		fprintf(fout,"#endif /* Old Style C */\n");
	}
}

void
pprocdef(proc, vp, addargtype, server_p,mode)
	proc_list *proc;
	version_list *vp;
	char *addargtype;
	int server_p;
	int mode;
{

	ptype(proc->res_prefix, proc->res_type, 1);
	fprintf(fout, "* ");
	if (server_p)
		pvname_svc(proc->proc_name, vp->vers_num);
	else
		pvname(proc->proc_name, vp->vers_num);

	/*
	 * mode  0 == cplusplus, mode  1 = ANSI-C, mode 2 = old style C
	 */
	if (mode == 0 || mode == 1)
		parglist(proc, addargtype);
	else
		fprintf(fout, "();\n");
}

/* print out argument list of procedure */
static void
parglist(proc, addargtype)
	proc_list *proc;
	char *addargtype;
{
	decl_list *dl;

	fprintf(fout,"(");

	if (proc->arg_num < 2 && newstyle &&
	   streq(proc->args.decls->decl.type, "void")) {
		/* 0 argument in new style:  do nothing */
	} else {
		for (dl = proc->args.decls; dl != NULL; dl = dl->next) {
			ptype(dl->decl.prefix, dl->decl.type, 1);
			if (!newstyle)
				fprintf(fout, "*"); /* old style passes by reference */
			fprintf(fout, ", ");
		}
	}
	fprintf(fout, "%s);\n", addargtype);
}

static void
penumdef(def)
	definition *def;
{
	char *name = def->def_name;
	enumval_list *l;
	char *last = NULL;
	int count = 0;

	fprintf(fout, "enum %s {\n", name);
	for (l = def->def.en.vals; l != NULL; l = l->next) {
		fprintf(fout, "\t%s", l->name);
		if (l->assignment) {
			fprintf(fout, " = %s", l->assignment);
			last = l->assignment;
			count = 1;
		} else {
			if (last == NULL) {
				fprintf(fout, " = %d", count++);
			} else {
				fprintf(fout, " = %s + %d", last, count++);
			}
		}
		if (l->next)
			fprintf(fout, ",\n");
		else
			fprintf(fout, "\n");
	}
	fprintf(fout, "};\n");
	fprintf(fout, "typedef enum %s %s;\n", name, name);
}

static void
ptypedef(def)
	definition *def;
{
	char *name = def->def_name;
	char *old = def->def.ty.old_type;
	char prefix[8];	/* enough to contain "struct ", including NUL */
	relation rel = def->def.ty.rel;

	if (!streq(name, old)) {
		if (streq(old, "string")) {
			old = "char";
			rel = REL_POINTER;
		} else if (streq(old, "opaque")) {
			old = "char";
		} else if (streq(old, "bool")) {
			old = "bool_t";
		}
		if (undefined2(old, name) && def->def.ty.old_prefix) {
			snprintf(prefix, sizeof prefix, "%s ", def->def.ty.old_prefix);
		} else {
			prefix[0] = 0;
		}
		fprintf(fout, "typedef ");
		switch (rel) {
		case REL_ARRAY:
			fprintf(fout, "struct {\n");
			fprintf(fout, "\tu_int %s_len;\n", name);
			fprintf(fout, "\t%s%s *%s_val;\n", prefix, old, name);
			fprintf(fout, "} %s", name);
			break;
		case REL_POINTER:
			fprintf(fout, "%s%s *%s", prefix, old, name);
			break;
		case REL_VECTOR:
			fprintf(fout, "%s%s %s[%s]", prefix, old, name,
				def->def.ty.array_max);
			break;
		case REL_ALIAS:
			fprintf(fout, "%s%s %s", prefix, old, name);
			break;
		}
		fprintf(fout, ";\n");
	}
}

void
pdeclaration(name, dec, tab, separator)
	char *name;
	declaration *dec;
	int tab;
	char *separator;
{
	char buf[8];	/* enough to hold "struct ", include NUL */
	char *prefix;
	char *type;

	if (streq(dec->type, "void"))
		return;
	tabify(fout, tab);
	if (streq(dec->type, name) && !dec->prefix) {
		fprintf(fout, "struct ");
	}
	if (streq(dec->type, "string")) {
		fprintf(fout, "char *%s", dec->name);
	} else {
		prefix = "";
		if (streq(dec->type, "bool")) {
			type = "bool_t";
		} else if (streq(dec->type, "opaque")) {
			type = "char";
		} else {
			if (dec->prefix) {
				snprintf(buf, sizeof buf, "%s ", dec->prefix);
				prefix = buf;
			}
			type = dec->type;
		}
		switch (dec->rel) {
		case REL_ALIAS:
			fprintf(fout, "%s%s %s", prefix, type, dec->name);
			break;
		case REL_VECTOR:
			fprintf(fout, "%s%s %s[%s]", prefix, type, dec->name,
				dec->array_max);
			break;
		case REL_POINTER:
			fprintf(fout, "%s%s *%s", prefix, type, dec->name);
			break;
		case REL_ARRAY:
			fprintf(fout, "struct {\n");
			tabify(fout, tab);
			fprintf(fout, "\tu_int %s_len;\n", dec->name);
			tabify(fout, tab);
			fprintf(fout, "\t%s%s *%s_val;\n", prefix, type, dec->name);
			tabify(fout, tab);
			fprintf(fout, "} %s", dec->name);
			break;
		}
	}
	fprintf(fout, "%s", separator);
}

static int
undefined2(type, stop)
	char *type;
	char *stop;
{
	list *l;
	definition *def;

	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind != DEF_PROGRAM) {
			if (streq(def->def_name, stop)) {
				return (1);
			} else if (streq(def->def_name, type)) {
				return (0);
			}
		}
	}
	return (1);
}
