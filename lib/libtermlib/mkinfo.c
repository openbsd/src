/*	$OpenBSD: mkinfo.c,v 1.1.1.1 1996/05/31 05:40:02 tholo Exp $	*/

/*
 * Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by SigmaSoft, Th.  Lockert.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: mkinfo.c,v 1.1.1.1 1996/05/31 05:40:02 tholo Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "term.private.h"

int main __P((int, char *[]));
int caporder __P((const void *, const void *));
int infoorder __P((const void *, const void *));

struct caps {
    int		 type;
    int		 index;
    char	*name;
    char	*info;
    char	*cap;
};

/*
 * Create sorted capability lists and other data files needed
 * for the terminfo library
 */
int
main(argc, argv)
     int argc;
     char *argv[];
{
    char *line, *type, *name, *info, *cap;
    int numbool, numnum, numstr;
    int binbool, binnum, binstr;
    int i, maxcaps, numcaps;
    struct caps *cp;
    size_t len;
    FILE *fp;

    if (argc != 2)
	errx(1, "Usage: mkinfo Capabilities");
    if ((fp = fopen(argv[1], "r")) == NULL)
	err(1, "Could not open '%s' for read", argv[1]);
    numbool = numnum = numstr = numcaps = 0;
    binbool = binnum = binstr = -1;
    cp = calloc(sizeof(struct caps), maxcaps = 32);
    while ((line = fgetln(fp, &len)) != NULL && len > 0) {
	if (line[len - 1] != '\n') {
	    warnx("Last input line bad");
	    continue;
	}
	line[len - 1] = '\0';
	if (strncmp(line, "#endbinary", 10) == 0) {
	    binbool = numbool;
	    binnum = numnum;
	    binstr = numstr;
	}
	if (line[0] == '\0' || line[0] == '#')
	    continue;
	if ((type = strtok(line, " \t")) == NULL) {
	    warnx("Bad input line");
	    continue;
	}
	if ((name = strtok(NULL, " \t")) == NULL) {
	    warnx("Bad input line");
	    continue;
	}
	if ((info = strtok(NULL, " \t")) == NULL) {
	    warnx("Bad input line");
	    continue;
	}
	if ((cap = strtok(NULL, " \t")) == NULL) {
	    warnx("Bad input line");
	    continue;
	}
	if (strcmp(type, "bool") == 0) {
	    cp[numcaps].type = TYPE_BOOL;
	    cp[numcaps].index = numbool++;
	}
	else if (strcmp(type, "num") == 0) {
	    cp[numcaps].type = TYPE_NUM;
	    cp[numcaps].index = numnum++;
	}
	else if (strcmp(type, "str") == 0) {
	    cp[numcaps].type = TYPE_STR;
	    cp[numcaps].index = numstr++;
	}
	else {
	    warnx("Bad type '%s' encountered", type);
	    continue;
	}
	cp[numcaps].name = strdup(name);
	cp[numcaps].info = strdup(info);
	cp[numcaps++].cap = strdup(cap);
	if (numcaps == maxcaps) {
	    maxcaps <<= 1;
	    if ((cp = realloc(cp, maxcaps * sizeof(struct caps))) == NULL)
		errx(1, "Out of memory");
	}
    }
    fclose(fp);

    if ((fp = fopen("boolnames.c", "w")) == NULL)
	err(1, "Could not open 'boolnames.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "const char * const boolnames[] = {\n");
    for (i = 0; i < numcaps; i++)
	if (cp[i].type == TYPE_BOOL)
	    fprintf(fp, "\t\"%s\",\n", cp[i].name);
    fprintf(fp, "};\n");
    fclose(fp);
    if ((fp = fopen("boolcodes.c", "w")) == NULL)
	err(1, "Could not open 'boolcodes.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "const char * const boolcodes[] = {\n");
    for (i = 0; i < numcaps; i++)
	if (cp[i].type == TYPE_BOOL)
	    fprintf(fp, "\t\"%s\",\n", cp[i].info);
    fprintf(fp, "};\n");
    fclose(fp);
    if ((fp = fopen("boolfnames.c", "w")) == NULL)
	err(1, "Could not open 'boolfnames.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "const char * const boolfnames[] = {\n");
    for (i = 0; i < numcaps; i++)
	if (cp[i].type == TYPE_BOOL)
	    fprintf(fp, "\t\"%s\",\n", cp[i].cap);
    fprintf(fp, "};\n");
    fclose(fp);

    if ((fp = fopen("numnames.c", "w")) == NULL)
	err(1, "Could not open 'numnames.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "const char * const numnames[] = {\n");
    for (i = 0; i < numcaps; i++)
	if (cp[i].type == TYPE_NUM)
	    fprintf(fp, "\t\"%s\",\n", cp[i].name);
    fprintf(fp, "};\n");
    fclose(fp);
    if ((fp = fopen("numcodes.c", "w")) == NULL)
	err(1, "Could not open 'numcodes.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "const char * const numcodes[] = {\n");
    for (i = 0; i < numcaps; i++)
	if (cp[i].type == TYPE_NUM)
	    fprintf(fp, "\t\"%s\",\n", cp[i].info);
    fprintf(fp, "};\n");
    fclose(fp);
    if ((fp = fopen("numfnames.c", "w")) == NULL)
	err(1, "Could not open 'numfnames.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "const char * const numfnames[] = {\n");
    for (i = 0; i < numcaps; i++)
	if (cp[i].type == TYPE_NUM)
	    fprintf(fp, "\t\"%s\",\n", cp[i].cap);
    fprintf(fp, "};\n");
    fclose(fp);

    if ((fp = fopen("strnames.c", "w")) == NULL)
	err(1, "Could not open 'strnames.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "const char * const strnames[] = {\n");
    for (i = 0; i < numcaps; i++)
	if (cp[i].type == TYPE_STR)
	    fprintf(fp, "\t\"%s\",\n", cp[i].name);
    fprintf(fp, "};\n");
    fclose(fp);
    if ((fp = fopen("strcodes.c", "w")) == NULL)
	err(1, "Could not open 'strcodes.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "const char * const strcodes[] = {\n");
    for (i = 0; i < numcaps; i++)
	if (cp[i].type == TYPE_STR)
	    fprintf(fp, "\t\"%s\",\n", cp[i].info);
    fprintf(fp, "};\n");
    fclose(fp);
    if ((fp = fopen("strfnames.c", "w")) == NULL)
	err(1, "Could not open 'strfnames.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "const char * const strfnames[] = {\n");
    for (i = 0; i < numcaps; i++)
	if (cp[i].type == TYPE_STR)
	    fprintf(fp, "\t\"%s\",\n", cp[i].cap);
    fprintf(fp, "};\n");
    fclose(fp);

    qsort(cp, numcaps, sizeof(struct caps), caporder);
    if ((fp = fopen("captoidx.c", "w")) == NULL)
	err(1, "Could not open 'captoidx.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "#include <stdlib.h>\n\n");
    fprintf(fp, "#include \"term.private.h\"\n\n");
    fprintf(fp, "const struct xtoidx _ti_captoidx[] = {\n");
    for (i = 0; i < numcaps; i++)
	fprintf(fp, "\t{ \"%s\",\t%d,\t%d },\n", cp[i].cap, cp[i].index, cp[i].type);
    fprintf(fp, "};\n\n");
    fprintf(fp, "size_t _ti_numcaps = %d;\n", numcaps);
    fclose(fp);

    qsort(cp, numcaps, sizeof(struct caps), infoorder);
    if ((fp = fopen("infotoidx.c", "w")) == NULL)
	err(1, "Could not open 'infotoidx.c' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "#include <stdlib.h>\n\n");
    fprintf(fp, "#include \"term.private.h\"\n\n");
    fprintf(fp, "const struct xtoidx _ti_infotoidx[] = {\n");
    for (i = 0; i < numcaps; i++)
	fprintf(fp, "\t{ \"%s\",\t%d,\t%d },\n", cp[i].info, cp[i].index, cp[i].type);
    fprintf(fp, "};\n\n");
    fprintf(fp, "size_t _ti_numinfos = %d;\n", numcaps);
    fclose(fp);

    if ((fp = fopen("binaries.h", "w")) == NULL)
	err(1, "Could not open 'binaries.h' for writing");
    fprintf(fp, "/* This file automatically generated.  Do not edit. */\n\n");
    fprintf(fp, "#define BIN_BOOL_CNT\t%d\n", binbool);
    fprintf(fp, "#define BIN_NUM_CNT\t%d\n", binnum);
    fprintf(fp, "#define BIN_STR_CNT\t%d\n", binstr);
    fclose(fp);
    exit(0);
}

int
caporder(p1, p2)
     const void *p1;
     const void *p2;
{
    return strcmp(((struct caps *)p1)->cap, ((struct caps *)p2)->cap);
}

int
infoorder(p1, p2)
     const void *p1;
     const void *p2;
{
    return strcmp(((struct caps *)p1)->info, ((struct caps *)p2)->info);
}
