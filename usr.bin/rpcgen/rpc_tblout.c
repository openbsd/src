/*	$OpenBSD: rpc_tblout.c,v 1.13 2010/09/01 14:43:34 millert Exp $	*/
/*	$NetBSD: rpc_tblout.c,v 1.3 1995/06/24 15:00:15 pk Exp $	*/

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
 * rpc_tblout.c, Dispatch table outputter for the RPC protocol compiler
 */
#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpc_parse.h"
#include "rpc_util.h"

#define TABSIZE		8
#define TABCOUNT	5
#define TABSTOP		(TABSIZE*TABCOUNT)

static char tabstr[TABCOUNT+1] = "\t\t\t\t\t";

static char tbl_hdr[] = "struct rpcgen_table %s_table[] = {\n";
static char tbl_end[] = "};\n";

static char null_entry[] = "\n\t(char *(*)())0,\n\
\t(xdrproc_t) xdr_void,\t\t\t0,\n\
\t(xdrproc_t) xdr_void,\t\t\t0,\n";

static char tbl_nproc[] = "int %s_nproc =\n\tsizeof(%s_table)/sizeof(%s_table[0]);\n\n";

static void write_table(definition *);
static void printit(char *, char *);

void
write_tables()
{
	definition *def;
	list *l;

	fprintf(fout, "\n");
	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind == DEF_PROGRAM)
			write_table(def);
	}
}

static void
write_table(def)
	definition *def;
{
	version_list *vp;
	proc_list *proc;
	int current;
	int expected;
	char progvers[100];
	int warning;

	for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
		warning = 0;
		snprintf(progvers, sizeof progvers, "%s_%s",
		    locase(def->def_name), vp->vers_num);
		/* print the table header */
		fprintf(fout, tbl_hdr, progvers);

		if (nullproc(vp->procs)) {
			expected = 0;
		} else {
			expected = 1;
			fprintf(fout, null_entry);
		}
		for (proc = vp->procs; proc != NULL; proc = proc->next) {
			current = atoi(proc->proc_num);
			if (current != expected++) {
				fprintf(fout,
				    "\n/*\n * WARNING: table out of order\n */\n");
				if (warning == 0) {
					fprintf(stderr,
					    "WARNING %s table is out of order\n",
					    progvers);
					warning = 1;
					nonfatalerrors = 1;
				}
				expected = current + 1;
			}
			fprintf(fout, "\n\t(char *(*)())RPCGEN_ACTION(");

			/* routine to invoke */
			if (!newstyle)
				pvname_svc(proc->proc_name, vp->vers_num);
			else {
				if (newstyle)
					fprintf(fout, "_");   /* calls internal func */
				pvname(proc->proc_name, vp->vers_num);
			}
			fprintf(fout, "),\n");

			/* argument info */
			if (proc->arg_num > 1)
				printit((char*) NULL, proc->args.argname);
			else
				/* do we have to do something special for newstyle */
				printit(proc->args.decls->decl.prefix,
				    proc->args.decls->decl.type);
			/* result info */
			printit(proc->res_prefix, proc->res_type);
		}

		/* print the table trailer */
		fprintf(fout, tbl_end);
		fprintf(fout, tbl_nproc, progvers, progvers, progvers);
	}
}

static void
printit(prefix, type)
	char *prefix;
	char *type;
{
	int len, tabs;

	len = fprintf(fout, "\txdr_%s,", stringfix(type));
	/* account for leading tab expansion */
	len += TABSIZE - 1;
	/* round up to tabs required */
	tabs = (TABSTOP - len + TABSIZE - 1)/TABSIZE;
	fprintf(fout, "%s", &tabstr[TABCOUNT-tabs]);

	if (streq(type, "void")) {
		fprintf(fout, "0");
	} else {
		fprintf(fout, "sizeof (");
		/* XXX: should "follow" be 1 ??? */
		ptype(prefix, type, 0);
		fprintf(fout, ")");
	}
	fprintf(fout, ",\n");
}
