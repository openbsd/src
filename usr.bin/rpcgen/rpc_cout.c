/*	$OpenBSD: rpc_cout.c,v 1.20 2009/10/27 23:59:42 deraadt Exp $	*/
/*	$NetBSD: rpc_cout.c,v 1.6 1996/10/01 04:13:53 cgd Exp $	*/
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
 * rpc_cout.c, XDR routine outputter for the RPC protocol compiler
 */
#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "rpc_parse.h"
#include "rpc_util.h"

static int findtype(definition *, char *);
static int undefined(char *);
static void print_generic_header(char *, int);
static void print_header(definition *);
static void print_prog_header(proc_list *);
static void print_trailer(void);
static void print_ifopen(int, char *);
static void print_ifarg(char *);
static void print_ifsizeof(char *, char *);
static void print_ifclose(int);
static void print_ifstat(int, char *, char *, relation, char *, char *, char *);
static void emit_program(definition *);
static void emit_enum(definition *);
static void emit_union(definition *);
static void emit_struct(definition *);
static void emit_typedef(definition *);
static void print_stat(int, declaration *);
void emit_inline(declaration *, int);
void emit_single_in_line(declaration *, int, relation);

/*
 * Emit the C-routine for the given definition
 */
void
emit(def)
	definition *def;
{
	if (def->def_kind == DEF_CONST) {
		return;
	}
	if (def->def_kind == DEF_PROGRAM) {
		emit_program(def);
		return;
	}
	if (def->def_kind == DEF_TYPEDEF) {
		/* now we need to handle declarations like struct typedef foo
		 * foo; since we dont want this to be expanded into 2 calls to
		 * xdr_foo */

		if (strcmp(def->def.ty.old_type, def->def_name) == 0)
			return;
	}

	print_header(def);
	switch (def->def_kind) {
	case DEF_UNION:
		emit_union(def);
		break;
	case DEF_ENUM:
		emit_enum(def);
		break;
	case DEF_STRUCT:
		emit_struct(def);
		break;
	case DEF_TYPEDEF:
		emit_typedef(def);
		break;
	}
	print_trailer();
}

static int
findtype(def, type)
	definition *def;
	char	*type;
{

	if (def->def_kind == DEF_PROGRAM || def->def_kind == DEF_CONST) {
		return (0);
	} else {
		return (streq(def->def_name, type));
	}
}

static int
undefined(type)
	char	*type;
{
	definition *def;

	def = (definition *) FINDVAL(defined, type, findtype);
	return (def == NULL);
}

static void
print_generic_header(procname, pointerp)
	char	*procname;
	int	pointerp;
{
	fprintf(fout, "\n");
	fprintf(fout, "bool_t\n");
	if (Cflag) {
		fprintf(fout, "xdr_%s(", procname);
		fprintf(fout, "XDR *xdrs, ");
		fprintf(fout, "%s ", procname);
		if (pointerp)
			fprintf(fout, "*");
		fprintf(fout, "objp)\n{\n");
	} else {
		fprintf(fout, "xdr_%s(xdrs, objp)\n", procname);
		fprintf(fout, "\tXDR *xdrs;\n");
		fprintf(fout, "\t%s ", procname);
		if (pointerp)
			fprintf(fout, "*");
		fprintf(fout, "objp;\n{\n");
	}
}

static void
print_header(def)
	definition *def;
{
	print_generic_header(def->def_name,
	    def->def_kind != DEF_TYPEDEF ||
	    !isvectordef(def->def.ty.old_type, def->def.ty.rel));

	/* Now add Inline support */

	if (doinline == 0)
		return;
}

static void
print_prog_header(plist)
	proc_list *plist;
{
	print_generic_header(plist->args.argname, 1);
}

static void
print_trailer()
{
	fprintf(fout, "\treturn (TRUE);\n");
	fprintf(fout, "}\n");
}

static void
print_ifopen(indent, name)
	int     indent;
	char   *name;
{
	tabify(fout, indent);
	fprintf(fout, "if (!xdr_%s(xdrs", name);
}

static void
print_ifarg(arg)
	char   *arg;
{
	fprintf(fout, ", %s", arg);
}

static void
print_ifsizeof(prefix, type)
	char   *prefix;
	char   *type;
{
	if (streq(type, "bool")) {
		fprintf(fout, ", sizeof(bool_t), (xdrproc_t)xdr_bool");
	} else {
		fprintf(fout, ", sizeof(");
		if (undefined(type) && prefix) {
			fprintf(fout, "%s ", prefix);
		}
		fprintf(fout, "%s), (xdrproc_t)xdr_%s", type, type);
	}
}

static void
print_ifclose(indent)
	int     indent;
{
	fprintf(fout, "))\n");
	tabify(fout, indent);
	fprintf(fout, "\treturn (FALSE);\n");
}

static void
print_ifstat(indent, prefix, type, rel, amax, objname, name)
	int     indent;
	char   *prefix;
	char   *type;
	relation rel;
	char   *amax;
	char   *objname;
	char   *name;
{
	char   *alt = NULL;

	switch (rel) {
	case REL_POINTER:
		print_ifopen(indent, "pointer");
		print_ifarg("(char **)");
		fprintf(fout, "%s", objname);
		print_ifsizeof(prefix, type);
		break;
	case REL_VECTOR:
		if (streq(type, "string")) {
			alt = "string";
		} else
			if (streq(type, "opaque")) {
				alt = "opaque";
			}
		if (alt) {
			print_ifopen(indent, alt);
			print_ifarg(objname);
			print_ifarg(amax);
		} else {
			print_ifopen(indent, "vector");
			print_ifarg("(char *)");
			fprintf(fout, "%s,\n", objname);
			tabify(fout, indent);
			fprintf(fout, "    %s", amax);
		}
		if (!alt) {
			print_ifsizeof(prefix, type);
		}
		break;
	case REL_ARRAY:
		if (streq(type, "string")) {
			alt = "string";
		} else
			if (streq(type, "opaque")) {
				alt = "bytes";
			}
		if (streq(type, "string")) {
			print_ifopen(indent, alt);
			print_ifarg(objname);
			print_ifarg(amax);
		} else {
			if (alt) {
				print_ifopen(indent, alt);
			} else {
				print_ifopen(indent, "array");
			}
			print_ifarg("(char **)");
			if (*objname == '&') {
				fprintf(fout, "%s.%s_val,\n\t    (u_int *)%s.%s_len",
				    objname, name, objname, name);
			} else {
				fprintf(fout, "&%s->%s_val,\n\t    (u_int *)&%s->%s_len",
				    objname, name, objname, name);
			}
			fprintf(fout, ",\n\t    %s", amax);
		}
		if (!alt) {
			print_ifsizeof(prefix, type);
		}
		break;
	case REL_ALIAS:
		print_ifopen(indent, type);
		print_ifarg(objname);
		break;
	}
	print_ifclose(indent);
}

/* ARGSUSED */
static void
emit_enum(def)
	definition *def;
{
	fprintf(fout, "\n");

	print_ifopen(1, "enum");
	print_ifarg("(enum_t *)objp");
	print_ifclose(1);
}

static void
emit_program(def)
	definition *def;
{
	decl_list *dl;
	version_list *vlist;
	proc_list *plist;

	for (vlist = def->def.pr.versions; vlist != NULL; vlist = vlist->next)
		for (plist = vlist->procs; plist != NULL; plist = plist->next) {
			if (!newstyle || plist->arg_num < 2)
				continue;	/* old style, or single
						 * argument */
			print_prog_header(plist);
			for (dl = plist->args.decls; dl != NULL;
			    dl = dl->next)
				print_stat(1, &dl->decl);
			print_trailer();
		}
}

static void
emit_union(def)
	definition *def;
{
	declaration *dflt;
	case_list *cl;
	declaration *cs;
	char   *object;
	char   *vecformat = "objp->%s_u.%s";
	char   *format = "&objp->%s_u.%s";

	fprintf(fout, "\n");
	print_stat(1, &def->def.un.enum_decl);
	fprintf(fout, "\tswitch (objp->%s) {\n", def->def.un.enum_decl.name);
	for (cl = def->def.un.cases; cl != NULL; cl = cl->next) {
		fprintf(fout, "\tcase %s:\n", cl->case_name);
		if (cl->contflag == 1)	/* a continued case statement */
			continue;
		cs = &cl->case_decl;
		if (!streq(cs->type, "void")) {
			int len = strlen(def->def_name) + strlen(format) +
			    strlen(cs->name) + 1;

			object = alloc(len);
			if (object == NULL) {
				fprintf(stderr, "Fatal error: no memory\n");
				crash();
			}
			if (isvectordef(cs->type, cs->rel)) {
				snprintf(object, len, vecformat, def->def_name,
				    cs->name);
			} else {
				snprintf(object, len, format, def->def_name,
				    cs->name);
			}
			print_ifstat(2, cs->prefix, cs->type, cs->rel, cs->array_max,
			    object, cs->name);
			free(object);
		}
		fprintf(fout, "\t\tbreak;\n");
	}
	dflt = def->def.un.default_decl;
	if (dflt != NULL) {
		if (!streq(dflt->type, "void")) {
			int len = strlen(def->def_name) + strlen(format) +
			    strlen(dflt->name) + 1;

			fprintf(fout, "\tdefault:\n");
			object = alloc(len);
			if (object == NULL) {
				fprintf(stderr, "Fatal error: no memory\n");
				crash();
			}
			if (isvectordef(dflt->type, dflt->rel)) {
				snprintf(object, len, vecformat, def->def_name,
				    dflt->name);
			} else {
				snprintf(object, len, format, def->def_name,
				    dflt->name);
			}

			print_ifstat(2, dflt->prefix, dflt->type, dflt->rel,
			    dflt->array_max, object, dflt->name);
			free(object);
			fprintf(fout, "\t\tbreak;\n");
		}
	} else {
		fprintf(fout, "\tdefault:\n");
		fprintf(fout, "\t\treturn (FALSE);\n");
	}

	fprintf(fout, "\t}\n");
}

static void
emit_struct(def)
	definition *def;
{
	decl_list *dl;
	int     i, j, size, flag;
	decl_list *cur, *psav;
	bas_type *ptr;
	char   *sizestr, *plus;
	char    ptemp[256];
	int     can_inline;

	if (doinline == 0) {
		for (dl = def->def.st.decls; dl != NULL; dl = dl->next)
			print_stat(1, &dl->decl);
		return;
	}
	for (dl = def->def.st.decls; dl != NULL; dl = dl->next)
		if (dl->decl.rel == REL_VECTOR) {
			fprintf(fout, "\tint i;\n");
			break;
		}
	fprintf(fout, "\n");

	size = 0;
	can_inline = 0;
	for (dl = def->def.st.decls; dl != NULL; dl = dl->next)
		if (dl->decl.prefix == NULL &&
		    (ptr = find_type(dl->decl.type)) != NULL &&
		    (dl->decl.rel == REL_ALIAS || dl->decl.rel == REL_VECTOR)) {
			if (dl->decl.rel == REL_ALIAS)
				size += ptr->length;
			else {
				can_inline = 1;
				break;	/* can be inlined */
			}
		} else {
			if (size >= doinline) {
				can_inline = 1;
				break;	/* can be inlined */
			}
			size = 0;
		}
	if (size > doinline)
		can_inline = 1;

	if (can_inline == 0) {	/* can not inline, drop back to old mode */
		fprintf(fout, "\n");
		for (dl = def->def.st.decls; dl != NULL; dl = dl->next)
			print_stat(1, &dl->decl);
		return;
	}

	/* May cause lint to complain. but  ... */
	fprintf(fout, "\tint32_t *buf;\n");

	flag = PUT;
	for (j = 0; j < 2; j++) {
		if (flag == PUT)
			fprintf(fout, "\n\tif (xdrs->x_op == XDR_ENCODE) {\n");
		else
			fprintf(fout, "\t\treturn (TRUE);\n\t} else if (xdrs->x_op == XDR_DECODE) {\n");

		i = 0;
		size = 0;
		sizestr = NULL;
		for (dl = def->def.st.decls; dl != NULL; dl = dl->next) {	/* xxx */

			/* now walk down the list and check for basic types */
			if (dl->decl.prefix == NULL &&
			    (ptr = find_type(dl->decl.type)) != NULL &&
			    (dl->decl.rel == REL_ALIAS || dl->decl.rel == REL_VECTOR)) {
				if (i == 0)
					cur = dl;
				i++;

				if (dl->decl.rel == REL_ALIAS)
					size += ptr->length;
				else {
					/* this is required to handle arrays */

					if (sizestr == NULL)
						plus = "";
					else
						plus = "+";

					if (ptr->length != 1)
						snprintf(ptemp, sizeof ptemp,
						    "%s%s* %d", plus,
						    dl->decl.array_max,
						    ptr->length);
					else
						snprintf(ptemp, sizeof ptemp,
						    "%s%s", plus,
						    dl->decl.array_max);

					/* now concatenate to sizestr !!!! */
					if (sizestr == NULL) {
						sizestr = strdup(ptemp);
						if (sizestr == NULL) {
							fprintf(stderr,
							    "Fatal error: no memory\n");
							crash();
						}
					} else {
						size_t len;

						len = strlen(sizestr) +
						    strlen(ptemp) + 1;
						sizestr = (char *)realloc(sizestr, len);
						if (sizestr == NULL) {
							fprintf(stderr,
							    "Fatal error: no memory\n");
							crash();
						}
						/* build up length of array */
						strlcat(sizestr, ptemp, len);
					}
				}

			} else {
				if (i > 0) {
					if (sizestr == NULL && size < doinline) {
						/* don't expand into inline
						 * code if size < doinline */
						while (cur != dl) {
							print_stat(2, &cur->decl);
							cur = cur->next;
						}
					} else {
						/* were already looking at a
						 * xdr_inlineable structure */
						if (sizestr == NULL)
							fprintf(fout,
							    "\t\tbuf = (int32_t *)XDR_INLINE(xdrs,\n\t\t    %d * BYTES_PER_XDR_UNIT);", size);
						else if (size == 0)
							fprintf(fout,
							    "\t\tbuf = (int32_t *)XDR_INLINE(xdrs,\n\t\t    %s * BYTES_PER_XDR_UNIT);",
								    sizestr);
						else
							fprintf(fout,
							    "\t\tbuf = (int32_t *)XDR_INLINE(xdrs,\n\t\t    (%d + %s) * BYTES_PER_XDR_UNIT);", size, sizestr);

						fprintf(fout,
						    "\n\t\tif (buf == NULL) {\n");

						psav = cur;
						while (cur != dl) {
							print_stat(3, &cur->decl);
							cur = cur->next;
						}

						fprintf(fout, "\t\t} else {\n");

						cur = psav;
						while (cur != dl) {
							emit_inline(&cur->decl, flag);
							cur = cur->next;
						}
						fprintf(fout, "\t\t}\n");
					}
				}
				size = 0;
				i = 0;
				sizestr = NULL;
				print_stat(2, &dl->decl);
			}
		}
		if (i > 0) {
			if (sizestr == NULL && size < doinline) {
				/* don't expand into inline code if size <
				 * doinline */
				while (cur != dl) {
					print_stat(2, &cur->decl);
					cur = cur->next;
				}
			} else {
				/* were already looking at a xdr_inlineable
				 * structure */
				if (sizestr == NULL)
					fprintf(fout, "\t\tbuf = (int32_t *)XDR_INLINE(xdrs,\n\t\t    %d * BYTES_PER_XDR_UNIT);",
					    size);
				else
					if (size == 0)
						fprintf(fout,
						    "\t\tbuf = (int32_t *)XDR_INLINE(xdrs,\n\t\t    %s * BYTES_PER_XDR_UNIT);",
						    sizestr);
					else
						fprintf(fout,
						    "\t\tbuf = (int32_t *)XDR_INLINE(xdrs,\n\t\t    (%d + %s) * BYTES_PER_XDR_UNIT);",
						    size, sizestr);

				fprintf(fout, "\n\t\tif (buf == NULL) {\n");

				psav = cur;
				while (cur != NULL) {
					print_stat(3, &cur->decl);
					cur = cur->next;
				}
				fprintf(fout, "\t\t} else {\n");

				cur = psav;
				while (cur != dl) {
					emit_inline(&cur->decl, flag);
					cur = cur->next;
				}

				fprintf(fout, "\t\t}\n");

			}
		}
		flag = GET;
	}
	fprintf(fout, "\t\treturn (TRUE);\n\t}\n\n");

	/* now take care of XDR_FREE case */

	for (dl = def->def.st.decls; dl != NULL; dl = dl->next)
		print_stat(1, &dl->decl);
}

static void
emit_typedef(def)
	definition *def;
{
	char   *prefix = def->def.ty.old_prefix;
	char   *type = def->def.ty.old_type;
	char   *amax = def->def.ty.array_max;
	relation rel = def->def.ty.rel;

	fprintf(fout, "\n");
	print_ifstat(1, prefix, type, rel, amax, "objp", def->def_name);
}

static void
print_stat(indent, dec)
	declaration *dec;
	int     indent;
{
	char   *prefix = dec->prefix;
	char   *type = dec->type;
	char   *amax = dec->array_max;
	relation rel = dec->rel;
	char    name[256];

	if (isvectordef(type, rel)) {
		snprintf(name, sizeof name, "objp->%s", dec->name);
	} else {
		snprintf(name, sizeof name, "&objp->%s", dec->name);
	}
	print_ifstat(indent, prefix, type, rel, amax, name, dec->name);
}

char   *upcase(char *);

void
emit_inline(decl, flag)
	declaration *decl;
	int     flag;
{
	/*check whether an array or not */

	switch (decl->rel) {
	case REL_ALIAS:
		fprintf(fout, "\t");
		emit_single_in_line(decl, flag, REL_ALIAS);
		break;
	case REL_VECTOR:
		fprintf(fout, "\t\t\t{\n\t\t\t\t%s *genp;\n\n", decl->type);
		fprintf(fout, "\t\t\t\tfor (i = 0, genp = objp->%s;\n\t\t\t\t    i < %s; i++) {\n\t\t\t",
		    decl->name, decl->array_max);
		emit_single_in_line(decl, flag, REL_VECTOR);
		fprintf(fout, "\t\t\t\t}\n\t\t\t}\n");

	}
}

void
emit_single_in_line(decl, flag, rel)
	declaration *decl;
	int     flag;
	relation rel;
{
	char   *upp_case;
	int     freed = 0;

	if (flag == PUT)
		fprintf(fout, "\t\tIXDR_PUT_");
	else
		if (rel == REL_ALIAS)
			fprintf(fout, "\t\tobjp->%s = IXDR_GET_", decl->name);
		else
			fprintf(fout, "\t\t*genp++ = IXDR_GET_");

	upp_case = upcase(decl->type);

	/* hack  - XX */
	if (strcmp(upp_case, "INT") == 0) {
		free(upp_case);
		freed = 1;
		upp_case = "LONG";
	}
	if (strcmp(upp_case, "U_INT") == 0) {
		free(upp_case);
		freed = 1;
		upp_case = "U_LONG";
	}
	if (flag == PUT)
		if (rel == REL_ALIAS)
			fprintf(fout, "%s(buf, objp->%s);\n", upp_case, decl->name);
		else
			fprintf(fout, "%s(buf, *genp++);\n", upp_case);

	else
		fprintf(fout, "%s(buf);\n", upp_case);
	if (!freed)
		free(upp_case);
}

char *
upcase(str)
	char   *str;
{
	char   *ptr, *hptr;

	ptr = (char *) malloc(strlen(str)+1);
	if (ptr == (char *) NULL) {
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	hptr = ptr;
	while (*str != '\0')
		*ptr++ = toupper(*str++);

	*ptr = '\0';
	return (hptr);
}
