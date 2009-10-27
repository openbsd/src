/*	$OpenBSD: skeleton.c,v 1.30 2009/10/27 23:59:50 deraadt Exp $	*/
/*	$NetBSD: skeleton.c,v 1.10 1996/03/25 00:36:18 mrg Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Paul Corbett.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "defs.h"

/*  The definition of yysccsid in the banner should be replaced with	*/
/*  a #pragma ident directive if the target C compiler supports		*/
/*  #pragma ident directives.						*/
/*									*/
/*  If the skeleton is changed, the banner should be changed so that	*/
/*  the altered version can be easily distinguished from the original.	*/
/*									*/
/*  The #defines included with the banner are there because they are	*/
/*  useful in subsequent code.  The macros #defined in the header or	*/
/*  the body either are not useful outside of semantic actions or	*/
/*  are conditional.							*/

char *banner[] =
{
    "#include <stdlib.h>",
    "#include <string.h>",
    "#define YYBYACC 1",
    "#define YYMAJOR 1",
    "#define YYMINOR 9",
    "#define YYLEX yylex()",
    "#define YYEMPTY -1",
    "#define yyclearin (yychar=(YYEMPTY))",
    "#define yyerrok (yyerrflag=0)",
    "#define YYRECOVERING() (yyerrflag!=0)",
    0
};


char *tables[] =
{
    "#if defined(__cplusplus) || defined(__STDC__)",
    "extern const short yylhs[];",
    "extern const short yylen[];",
    "extern const short yydefred[];",
    "extern const short yydgoto[];",
    "extern const short yysindex[];",
    "extern const short yyrindex[];",
    "extern const short yygindex[];",
    "extern const short yytable[];",
    "extern const short yycheck[];",
    "#if YYDEBUG",
    "extern const char *const yyname[];",
    "extern const char *const yyrule[];",
    "#endif",
    "#else /* !(defined(__cplusplus) || defined(__STDC__)) */",
    "extern short yylhs[];",
    "extern short yylen[];",
    "extern short yydefred[];",
    "extern short yydgoto[];",
    "extern short yysindex[];",
    "extern short yyrindex[];",
    "extern short yygindex[];",
    "extern short yytable[];",
    "extern short yycheck[];",
    "#if YYDEBUG",
    "extern char *yyname[];",
    "extern char *yyrule[];",
    "#endif /* YYDEBUG */",
    "#endif /* !(defined(__cplusplus) || defined(__STDC__)) */",
    0
};


char *header[] =
{
    "#ifdef YYSTACKSIZE",
    "#undef YYMAXDEPTH",
    "#define YYMAXDEPTH YYSTACKSIZE",
    "#else",
    "#ifdef YYMAXDEPTH",
    "#define YYSTACKSIZE YYMAXDEPTH",
    "#else",
    "#define YYSTACKSIZE 10000",
    "#define YYMAXDEPTH 10000",
    "#endif",
    "#endif",
    "#define YYINITSTACKSIZE 200",
    "/* LINTUSED */",
    "int yydebug;",
    "int yynerrs;",
    "int yyerrflag;",
    "int yychar;",
    "short *yyssp;",
    "YYSTYPE *yyvsp;",
    "YYSTYPE yyval;",
    "YYSTYPE yylval;",
    "short *yyss;",
    "short *yysslim;",
    "YYSTYPE *yyvs;",
    "int yystacksize;",
    0
};


char *body[] =
{
    "/* allocate initial stack or double stack size, up to YYMAXDEPTH */",
    "#if defined(__cplusplus) || defined(__STDC__)",
    "static int yygrowstack(void)",
    "#else",
    "static int yygrowstack()",
    "#endif",
    "{",
    "    int newsize, i;",
    "    short *newss;",
    "    YYSTYPE *newvs;",
    "",
    "    if ((newsize = yystacksize) == 0)",
    "        newsize = YYINITSTACKSIZE;",
    "    else if (newsize >= YYMAXDEPTH)",
    "        return -1;",
    "    else if ((newsize *= 2) > YYMAXDEPTH)",
    "        newsize = YYMAXDEPTH;",
    "    i = yyssp - yyss;",
    "#ifdef SIZE_MAX",
    "#define YY_SIZE_MAX SIZE_MAX",
    "#else",
    "#define YY_SIZE_MAX 0xffffffffU",
    "#endif",
    "    if (newsize && YY_SIZE_MAX / newsize < sizeof *newss)",
    "        goto bail;",
    "    newss = yyss ? (short *)realloc(yyss, newsize * sizeof *newss) :",
    "      (short *)malloc(newsize * sizeof *newss); /* overflow check above */",
    "    if (newss == NULL)",
    "        goto bail;",
    "    yyss = newss;",
    "    yyssp = newss + i;",
    "    if (newsize && YY_SIZE_MAX / newsize < sizeof *newvs)",
    "        goto bail;",
    "    newvs = yyvs ? (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs) :",
    "      (YYSTYPE *)malloc(newsize * sizeof *newvs); /* overflow check above */",
    "    if (newvs == NULL)",
    "        goto bail;",
    "    yyvs = newvs;",
    "    yyvsp = newvs + i;",
    "    yystacksize = newsize;",
    "    yysslim = yyss + newsize - 1;",
    "    return 0;",
    "bail:",
    "    if (yyss)",
    "            free(yyss);",
    "    if (yyvs)",
    "            free(yyvs);",
    "    yyss = yyssp = NULL;",
    "    yyvs = yyvsp = NULL;",
    "    yystacksize = 0;",
    "    return -1;",
    "}",
    "",
    "#define YYABORT goto yyabort",
    "#define YYREJECT goto yyabort",
    "#define YYACCEPT goto yyaccept",
    "#define YYERROR goto yyerrlab",
    "int",
    "#if defined(__cplusplus) || defined(__STDC__)",
    "yyparse(void)",
    "#else",
    "yyparse()",
    "#endif",
    "{",
    "    int yym, yyn, yystate;",
    "#if YYDEBUG",
    "#if defined(__cplusplus) || defined(__STDC__)",
    "    const char *yys;",
    "#else /* !(defined(__cplusplus) || defined(__STDC__)) */",
    "    char *yys;",
    "#endif /* !(defined(__cplusplus) || defined(__STDC__)) */",
    "",
    "    if ((yys = getenv(\"YYDEBUG\")))",
    "    {",
    "        yyn = *yys;",
    "        if (yyn >= '0' && yyn <= '9')",
    "            yydebug = yyn - '0';",
    "    }",
    "#endif /* YYDEBUG */",
    "",
    "    yynerrs = 0;",
    "    yyerrflag = 0;",
    "    yychar = (-1);",
    "",
    "    if (yyss == NULL && yygrowstack()) goto yyoverflow;",
    "    yyssp = yyss;",
    "    yyvsp = yyvs;",
    "    *yyssp = yystate = 0;",
    "",
    "yyloop:",
    "    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;",
    "    if (yychar < 0)",
    "    {",
    "        if ((yychar = yylex()) < 0) yychar = 0;",
    "#if YYDEBUG",
    "        if (yydebug)",
    "        {",
    "            yys = 0;",
    "            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];",
    "            if (!yys) yys = \"illegal-symbol\";",
    "            printf(\"%sdebug: state %d, reading %d (%s)\\n\",",
    "                    YYPREFIX, yystate, yychar, yys);",
    "        }",
    "#endif",
    "    }",
    "    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&",
    "            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)",
    "    {",
    "#if YYDEBUG",
    "        if (yydebug)",
    "            printf(\"%sdebug: state %d, shifting to state %d\\n\",",
    "                    YYPREFIX, yystate, yytable[yyn]);",
    "#endif",
    "        if (yyssp >= yysslim && yygrowstack())",
    "        {",
    "            goto yyoverflow;",
    "        }",
    "        *++yyssp = yystate = yytable[yyn];",
    "        *++yyvsp = yylval;",
    "        yychar = (-1);",
    "        if (yyerrflag > 0)  --yyerrflag;",
    "        goto yyloop;",
    "    }",
    "    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&",
    "            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)",
    "    {",
    "        yyn = yytable[yyn];",
    "        goto yyreduce;",
    "    }",
    "    if (yyerrflag) goto yyinrecovery;",
    "#if defined(lint) || defined(__GNUC__)",
    "    goto yynewerror;",
    "#endif",
    "yynewerror:",
    "    yyerror(\"syntax error\");",
    "#if defined(lint) || defined(__GNUC__)",
    "    goto yyerrlab;",
    "#endif",
    "yyerrlab:",
    "    ++yynerrs;",
    "yyinrecovery:",
    "    if (yyerrflag < 3)",
    "    {",
    "        yyerrflag = 3;",
    "        for (;;)",
    "        {",
    "            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&",
    "                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)",
    "            {",
    "#if YYDEBUG",
    "                if (yydebug)",
    "                    printf(\"%sdebug: state %d, error recovery shifting\\",
    " to state %d\\n\", YYPREFIX, *yyssp, yytable[yyn]);",
    "#endif",
    "                if (yyssp >= yysslim && yygrowstack())",
    "                {",
    "                    goto yyoverflow;",
    "                }",
    "                *++yyssp = yystate = yytable[yyn];",
    "                *++yyvsp = yylval;",
    "                goto yyloop;",
    "            }",
    "            else",
    "            {",
    "#if YYDEBUG",
    "                if (yydebug)",
    "                    printf(\"%sdebug: error recovery discarding state %d\
\\n\",",
    "                            YYPREFIX, *yyssp);",
    "#endif",
    "                if (yyssp <= yyss) goto yyabort;",
    "                --yyssp;",
    "                --yyvsp;",
    "            }",
    "        }",
    "    }",
    "    else",
    "    {",
    "        if (yychar == 0) goto yyabort;",
    "#if YYDEBUG",
    "        if (yydebug)",
    "        {",
    "            yys = 0;",
    "            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];",
    "            if (!yys) yys = \"illegal-symbol\";",
    "            printf(\"%sdebug: state %d, error recovery discards token %d\
 (%s)\\n\",",
    "                    YYPREFIX, yystate, yychar, yys);",
    "        }",
    "#endif",
    "        yychar = (-1);",
    "        goto yyloop;",
    "    }",
    "yyreduce:",
    "#if YYDEBUG",
    "    if (yydebug)",
    "        printf(\"%sdebug: state %d, reducing by rule %d (%s)\\n\",",
    "                YYPREFIX, yystate, yyn, yyrule[yyn]);",
    "#endif",
    "    yym = yylen[yyn];",
    "    if (yym)",
    "        yyval = yyvsp[1-yym];",
    "    else",
    "        memset(&yyval, 0, sizeof yyval);",
    "    switch (yyn)",
    "    {",
    0
};


char *trailer[] =
{
    "    }",
    "    yyssp -= yym;",
    "    yystate = *yyssp;",
    "    yyvsp -= yym;",
    "    yym = yylhs[yyn];",
    "    if (yystate == 0 && yym == 0)",
    "    {",
    "#if YYDEBUG",
    "        if (yydebug)",
    "            printf(\"%sdebug: after reduction, shifting from state 0 to\\",
    " state %d\\n\", YYPREFIX, YYFINAL);",
    "#endif",
    "        yystate = YYFINAL;",
    "        *++yyssp = YYFINAL;",
    "        *++yyvsp = yyval;",
    "        if (yychar < 0)",
    "        {",
    "            if ((yychar = yylex()) < 0) yychar = 0;",
    "#if YYDEBUG",
    "            if (yydebug)",
    "            {",
    "                yys = 0;",
    "                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];",
    "                if (!yys) yys = \"illegal-symbol\";",
    "                printf(\"%sdebug: state %d, reading %d (%s)\\n\",",
    "                        YYPREFIX, YYFINAL, yychar, yys);",
    "            }",
    "#endif",
    "        }",
    "        if (yychar == 0) goto yyaccept;",
    "        goto yyloop;",
    "    }",
    "    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&",
    "            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)",
    "        yystate = yytable[yyn];",
    "    else",
    "        yystate = yydgoto[yym];",
    "#if YYDEBUG",
    "    if (yydebug)",
    "        printf(\"%sdebug: after reduction, shifting from state %d \\",
    "to state %d\\n\", YYPREFIX, *yyssp, yystate);",
    "#endif",
    "    if (yyssp >= yysslim && yygrowstack())",
    "    {",
    "        goto yyoverflow;",
    "    }",
    "    *++yyssp = yystate;",
    "    *++yyvsp = yyval;",
    "    goto yyloop;",
    "yyoverflow:",
    "    yyerror(\"yacc stack overflow\");",
    "yyabort:",
    "    if (yyss)",
    "            free(yyss);",
    "    if (yyvs)",
    "            free(yyvs);",
    "    yyss = yyssp = NULL;",
    "    yyvs = yyvsp = NULL;",
    "    yystacksize = 0;",
    "    return (1);",
    "yyaccept:",
    "    if (yyss)",
    "            free(yyss);",
    "    if (yyvs)",
    "            free(yyvs);",
    "    yyss = yyssp = NULL;",
    "    yyvs = yyvsp = NULL;",
    "    yystacksize = 0;",
    "    return (0);",
    "}",
    0
};


void
write_section(char *section[])
{
    int c;
    int i;
    char *s;
    FILE *f;

    f = code_file;
    for (i = 0; (s = section[i]); ++i)
    {
	++outline;
	while ((c = *s))
	{
	    putc(c, f);
	    ++s;
	}
	putc('\n', f);
    }
}
