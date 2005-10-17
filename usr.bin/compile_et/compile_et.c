/*	$OpenBSD: compile_et.c,v 1.13 2005/10/17 07:35:29 biorn Exp $	*/
/*
 * Copyright (c) 1998-2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#undef ROKEN_RENAME
#include "compile_et.h"

/* RCSID("$KTH: compile_et.c,v 1.17 2005/05/16 08:42:45 lha Exp $"); */

#include <err.h>
#include "error_table.h"

int numerror;
extern FILE *yyin;

extern void yyparse(void);

long base;
int number;
char *prefix;
char *id_str;

char name[128];
char Basename[128];

#ifdef YYDEBUG
extern int yydebug = 1;
#endif

char *filename;
char hfn[128];
char cfn[128];

struct error_code *codes = NULL;

static int
generate_c(void)
{
	int n;
	struct error_code *ec;
	FILE *c_file = fopen(cfn, "w");

	if (c_file == NULL)
		return 1;

	fprintf(c_file, "/* Generated from %s */\n", filename);
	if (id_str) 
		fprintf(c_file, "/* %s */\n", id_str);

	fprintf(c_file, "\n");
	fprintf(c_file, "#include <stddef.h>\n");
	fprintf(c_file, "#include <com_err.h>\n");
	fprintf(c_file, "#include \"%s\"\n", hfn);
	fprintf(c_file, "\n");

	fprintf(c_file, "static const char *%s_error_strings[] = {\n", name);

	for (ec = codes, n = 0; ec; ec = ec->next, n++) {
		while (n < ec->number) {
			fprintf(c_file,
			    "\t/* %03d */ \"Reserved %s error (%d)\",\n",
			    n, name, n);
			n++;
		}
		fprintf(c_file,
		    "\t/* %03d */ \"%s\",\n", ec->number, ec->string);
	}

	fprintf(c_file, "\tNULL\n");
	fprintf(c_file, "};\n");
	fprintf(c_file, "\n");
	fprintf(c_file, "#define num_errors %d\n", number);
	fprintf(c_file, "\n");
	fprintf(c_file, 
	    "void initialize_%s_error_table_r(struct et_list **list)\n", name);
	fprintf(c_file, "{\n");
	fprintf(c_file, 
	    "    initialize_error_table_r(list, %s_error_strings, "
	    "num_errors, ERROR_TABLE_BASE_%s);\n", name, name);
	fprintf(c_file, "}\n");
	fprintf(c_file, "\n");
	fprintf(c_file, "void initialize_%s_error_table(void)\n", name);
	fprintf(c_file, "{\n");
	fprintf(c_file,
	    "    init_error_table(%s_error_strings, ERROR_TABLE_BASE_%s, "
	    "num_errors);\n", name, name);
	fprintf(c_file, "}\n");

	fclose(c_file);
	return 0;
}

static int
generate_h(void)
{
	struct error_code *ec;
	char fn[128], *p;
	FILE *h_file = fopen(hfn, "w");

	if (h_file == NULL)
		return 1;

	snprintf(fn, sizeof(fn), "__%s__", hfn);
	for (p = fn; *p; p++) {
		if(!isalnum((unsigned char)*p))
			*p = '_';
	}

	fprintf(h_file, "/* Generated from %s */\n", filename);
	if (id_str) 
		fprintf(h_file, "/* %s */\n", id_str);

	fprintf(h_file, "\n");
	fprintf(h_file, "#ifndef %s\n", fn);
	fprintf(h_file, "#define %s\n", fn);
	fprintf(h_file, "\n");
	fprintf(h_file, "struct et_list;\n");
	fprintf(h_file, "\n");
	fprintf(h_file, 
	    "void initialize_%s_error_table_r(struct et_list **);\n",
	    name);
	fprintf(h_file, "\n");
	fprintf(h_file, "void initialize_%s_error_table(void);\n", name);
	fprintf(h_file, "#define init_%s_err_tbl initialize_%s_error_table\n", 
	    name, name);
	fprintf(h_file, "\n");
	fprintf(h_file, "typedef enum %s_error_number{\n", name);

	for (ec = codes; ec; ec = ec->next)
		fprintf(h_file, "\t%s = %ld%s\n", ec->name, base + ec->number,
		    (ec->next != NULL) ? "," : "");

	fprintf(h_file, "} %s_error_number;\n", name);
	fprintf(h_file, "\n");
	fprintf(h_file, "#define ERROR_TABLE_BASE_%s %ld\n", name, base);
	fprintf(h_file, "\n");
	fprintf(h_file, "#endif /* %s */\n", fn);

	fclose(h_file);
	return 0;
}

static int
generate(void)
{
	return generate_c() || generate_h();
}

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s file\n", __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	char *p;

	if (argc != 2)
		usage();

	filename = argv[1];
	yyin = fopen(filename, "r");
	if (yyin == NULL)
		err(1, "%s", filename);
	
	p = strrchr(filename, '/');
	if (p)
		p++;
	else
		p = filename;

	strlcpy(Basename, p, sizeof(Basename));
	Basename[strcspn(Basename, ".")] = '\0';

	snprintf(hfn, sizeof(hfn), "%s.h", Basename);
	snprintf(cfn, sizeof(cfn), "%s.c", Basename);

	yyparse();
	if (numerror)
		return 1;

	return generate();
}
