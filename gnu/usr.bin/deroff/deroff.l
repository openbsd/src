%{
/***************************************************************************
 * deroff.l                                                                *
 *                                                                         *
 * Removes roff constructs and preprocessors input                         *
 *                                                                         *
 * (c) 1996 David Frey, <david@eos.lugs.ch>                                *
 *                                                                         * 
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         * 
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         * 
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the                           *
 *     Free Software Foundation, Inc.,                                     *
 *     59 Temple Place - Suite 330,                                        *
 *     Boston, MA 02111, USA                                               *
 ***************************************************************************/

/*
 * $OpenBSD: deroff.l,v 1.1 1997/03/08 01:29:08 kstailey Exp $
 * $DebianId: deroff.l,v 1.1 1996/12/28 15:58:30 david Rel $
 */

/* Acknowledgments: The inclusion code is from the lex manpage. */

#include <stdio.h>
#include <stdlib.h>
#ifndef __OpenBSD__
# include <getopt.h>
#else
# include <errno.h>
#endif
#include <strings.h>
#include <ctype.h>
#include <locale.h>

#define VERSION "1.0"

#ifdef HAVE_GETOPT_LONG
struct option const long_options[] =
{
  {"help",     no_argument, 0, 'h'},
  {"help",     no_argument, 0, '?'},
  {"version",  no_argument, 0, 'V'},
  {"wordlist", no_argument, 0, 'w'},
  {(char *)0,  no_argument, 0, (char)0}
};
#endif

char *progname;
unsigned int skip=0;    /* we are in preprocessor material, skip it */
unsigned int line;      /* current line */
unsigned int word=0;    /* flag: output a word-by-word list */
char *yyname;           /* name of current file */

/* from the flex manpage: */
#define MAX_INCLUDE_DEPTH 10
YY_BUFFER_STATE include_stack[MAX_INCLUDE_DEPTH];
int include_stack_ptr = 0;
%}

/* the "so" state is used for picking up the name of an `include' file,
   the "nx" state is similarly used to pick up the name of a `next' file. */
%x nx so

D       [[:digit:]]
WS      [[:blank:]]

/* MC are 'macro characters', TC are 'troff characters' and
 * CC is the (leading) 'control character'. 
 *
 * Reference (for this whole section):
 *   Troff User's Manual
 *   Computing Sciences Technical Report No. 54
 *   AT&T Bell Laboratories, Murry Hill, New Jersey 07974.
 *   Revised November, 1992
 */

CC      [.']
MC      [A-Z][A-Za-z]*
TC      [a-z0-9.()*]

%%

{CC}?\\\"                       /* ignore troff comments */ ;
^{CC}ft.*                       /* ignore font  changing commands */ ;
\\f.                            /* ignore font  changing commands */ ;
\\s[+-]?{D}+                    /* ignore size  changing commands */ ;

"\\(f"[ifl]                     if (skip == 0) fprintf(yyout,"f%c",yytext[3]);
                                /* ligature */
"\\(F"[il]                      if (skip == 0) fprintf(yyout,"ff%c",yytext[3]); 
                                /* dito */
"\\("..                         /* ignore symbols */ ;
\\[ |^0!]                       if (skip == 0) fputc(' ',yyout); 
                                /* various blanks: full, half, quad, digit, 
                                   and transparent line. */
"\\"(-|"("(hy|mi|em))           if (skip == 0) fputc('-', yyout);
                                /* treat minus, hyphen and em-dash 
                                   (incorrectly) as - */
\\[e&%]                         /* ignore escape, zero-width, optional
                                   hyphenation character */ ;
"\\*"[^(]|"\\*("..              /* ignore interpolation of ..?
                                   (register variables in -ms)
                                   NOTE: This is not very wise, as we
                                         loose the accents! 
                                         \*['`^,:~] should be converted to
                                         acute,grave,circumflex,cedilla,
                                         umlaut, tilde accents. */ ;

\\${D}                          /* ignore interpolation of argument D */ ;
\\.                             /* drop the other various troff commands */ ;

^{CC}{WS}*nx                    BEGIN(nx);  /* read in next file */
<nx>[^[:space:]]+               { /* got the include file name */
                                  if (fclose(yyin) < 0) {
                                    fprintf(stderr, 
                                            "%s: cannot close '%s': %s!\n",
                                            progname, yyname, strerror(errno));
                                  } 
                                  if (yytext[0] == '\0') {
                                    fprintf(stderr, 
                                            "%s: .nx request without "\
                                            "filename!\n", progname);
                                    exit(1);
                                  }
                                  yyin=fopen(yytext, "r");
                                  if (yyin == NULL) {
                                    fprintf(stderr, 
                                            "%s: .nx request: "\
                                            "cannot open '%s': %s!\n",
                                            progname,yytext,strerror(errno));
                                     exit(1);
                                  }
                                  yy_switch_to_buffer(
                                    yy_create_buffer(yyin,YY_BUF_SIZE));
                                  BEGIN(INITIAL);
                                }
^{CC}{WS}*so			BEGIN(so);
<nx,so>{WS}*			/* eat the whitespace */
<so>[^[:space:]]+               { /* got the include file name */
                   		  if (include_stack_ptr >= MAX_INCLUDE_DEPTH) {
                                    fprintf(stderr, 
                                            "%s: .so-requests nested too "\
                                            "deeply!\n",
                                            progname );
                                    exit(1);
                                  }
                                  include_stack[include_stack_ptr++] =
                                    YY_CURRENT_BUFFER;
                                  yyin=fopen(yytext, "r" );
                                  if (yyin == NULL) {
                                    fprintf(stderr, 
                                            "%s: .so request: "\
                                             "cannot open '%s': %s!\n",
                                             progname,yytext,strerror(errno));
                                     exit(1);
                                  }
                                  yy_switch_to_buffer(
                                    yy_create_buffer(yyin,YY_BUF_SIZE));
                                  BEGIN(INITIAL);
                                }
^{CC}{WS}*de.*                  skip++; /* troff macro definition */
^{CC}("EQ"|[PTv]S|"G1")         skip++; 
^{CC}("EN"|[PTv]E|"G2")         skip--;
^{WS}*".."                      skip--; /* end of troff macro definition */
                                /* XXX: macros have not necesserally 
                                        to end with ".." */
^{CC}{WS}*[a-ce-mo-rt-z]{TC}.*  /* ignore troff commands including args,
				  .de, .nx and .so already handled above */ ;
^{CC}{WS}*(d[^e]|n[^x]|s[^o]).* /* ignore troff commands including args,
				  .de, .nx and .so already handled above */ ;
^{CC}{WS}*{MC}{WS}*[+-]?{D}*    /* assume that this are macros, and throw 
                                   numeric arguments away. */ ;
[[:punct:]]?[[:blank:]]+        { if (skip == 0) {
                                    if (!word) { ECHO; }
                                    else fputc('\n', yyout); 
                                  }
                                }
.                               { if (skip == 0) {
                                    if (!word) ECHO; 
                                    else {
                                      char c=yytext[0];
                                      if (isalnum(c) || (c == '\'')) {
                                        fputc(c, yyout);
                                      }
                                      else if (ispunct(c)) {
                                        fputc('\n', yyout);
                                      }
                                    }
                                  }
                                }
\n                              { line++; if (skip == 0) ECHO; }
<<EOF>>                         { /* at end-of-file, if it was a .so
                                     request, return to parent file */
                                  if (--include_stack_ptr < 0) yyterminate();
                                  else yy_delete_buffer(YY_CURRENT_BUFFER);
                                  yy_switch_to_buffer(
                                    include_stack[include_stack_ptr]);
                                }
%%

int yywrap()
{
  return 1;
}

void yyerror(char *s)
{
  fprintf(stderr,"%s: %s in line %d of %s\n", progname, s, line, yyname);
}

int usage(void)
{
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s [-w] {file(s)}\n", progname);
  fprintf(stderr, "  %s -h|-V\n\n", progname);
  fprintf(stderr, "options:\n");
  fprintf(stderr, "  -w  output a word-list.\n");
  fprintf(stderr, "  -h  output a this usage information.\n");
  fprintf(stderr, "  -V  output copyright and license information.\n");
  exit(1);
}

int warranty(void)
{
  const char warranty[]="\
Copyright 1996 David Frey.\n\
This is free software; see the GNU General Public Licence version 2 or later\n\
for copying conditions.  There is NO warranty.\n\n\
The code for .so and .nx inclusion was taken from the example code in the 
flex(1) manual page.";

  fprintf(stderr,"%s version %s.\n\n", progname, VERSION); 
  fprintf(stderr,"%s\n", warranty);
  exit(0);
}

int main(int argc, char *argv[])
{
  int c, i, res;

  progname=strrchr(argv[0],'/');
  if (progname==NULL) progname=argv[0]; 
  else progname++;

  yyout=stdout; /* just to clarify */
#ifdef HAVE_GETOPT_LONG
  while ((c = getopt_long(argc, argv, "h?Vw",
                          long_options, (int *) 0)) != EOF)
#else
  while ((c = getopt(argc, argv, "h?Vw")) != EOF)
#endif
  {
    switch (c) {
      case 'h': case '?': usage();    break; /* NEVER REACHED */
      case 'V':           warranty(); break; /* NEVER REACHED */
      case 'w':           word=1;     break;
      case 0  : default :             break;
    }
  }

  setlocale(LC_ALL, "");
  if (argc > optind) {
    res=0;
    for(i=optind; i<argc; i++) {
      yyname=argv[i]; yyin=fopen(yyname,"r");
      if (yyin == NULL) {
        fprintf(stderr, "%s: cannot open '%s': %s.\n",
                progname, yyname, strerror(errno));
      } 
      else {
        line=0; res += yylex(); 
        if (fclose(yyin) < 0) {
          fprintf(stderr, "%s: cannot close '%s': %s!\n",
                  progname, yyname, strerror(errno));
        }
      }
    }
  }
  else {
    yyin=stdin; yyout=stdout; yyname="stdin"; line=0; res=yylex();
  }
  return res;
}
