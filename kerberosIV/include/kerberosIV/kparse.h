/*	$Id: kparse.h,v 1.1.1.1 1995/12/14 06:52:34 tholo Exp $	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

/*
 * Include file for kparse routines.
 */

#ifndef KPARSE_DEFS
#define KPARSE_DEFS

/*
 * values returned by fGetParameterSet() 
 */

#define PS_BAD_KEYWORD	  -2	/* unknown or duplicate keyword */
#define PS_SYNTAX	  -1	/* syntax error */
#define PS_OKAY		   0	/* got a complete parameter set */
#define PS_EOF		   1	/* nothing more in the file */

/*
 * values returned by fGetKeywordValue() 
 */

#define KV_SYNTAX	 -2	/* syntax error */
#define KV_EOF		 -1	/* nothing more in the file */
#define KV_OKAY		  0	/* got a keyword/value pair */
#define KV_EOL		  1	/* nothing more on this line */

/*
 * values returned by fGetToken() 
 */

#define GTOK_BAD_QSTRING -1	/* newline found in quoted string */
#define GTOK_EOF	  0	/* end of file encountered */
#define GTOK_QSTRING	  1	/* quoted string */
#define GTOK_STRING	  2	/* unquoted string */
#define GTOK_NUMBER	  3	/* one or more digits */
#define GTOK_PUNK	  4	/* punks are punctuation, newline,
				 * etc. */
#define GTOK_WHITE	  5	/* one or more whitespace chars */

/*
 * extended character classification macros 
 */

#define ISOCTAL(CH) 	( (CH>='0')  && (CH<='7') )
#define ISQUOTE(CH) 	( (CH=='\"') || (CH=='\'') || (CH=='`') )
#define ISWHITESPACE(C) ( (C==' ')   || (C=='\t') )
#define ISLINEFEED(C) 	( (C=='\n')  || (C=='\r')  || (C=='\f') )

/*
 * tokens consist of any printable charcacter except comma, equal, or
 * whitespace 
 */

#define ISTOKENCHAR(C) ((C>040) && (C<0177) && (C != ',') && (C != '='))

/*
 * the parameter table defines the keywords that will be recognized by
 * fGetParameterSet, and their default values if not specified. 
 */

typedef struct {
    char *keyword;
    char *defvalue;
    char *value;
}       parmtable;

#define PARMCOUNT(P) (sizeof(P)/sizeof(P[0]))

extern int LineNbr;		/* current line # in parameter file */

extern char ErrorMsg[];		/*
				 * meaningful only when KV_SYNTAX,
 				 * PS_SYNTAX,  or PS_BAD_KEYWORD is
				 * returned by fGetKeywordValue or
				 * fGetParameterSet
				 */

#include <stdio.h>

int fGetParameterSet __P((FILE *fp, parmtable *parm, int parmcount));
int ParmCompare __P((parmtable *parm, int parmcount, char *keyword, char *value));
void FreeParameterSet __P((parmtable *parm, int parmcount));
int fGetKeywordValue __P((FILE *fp, char *keyword, int klen, char *value, int vlen));
int fGetToken __P((FILE *fp, char *dest, int maxlen));
int fGetLiteral __P((FILE *fp));
int fUngetChar __P((int ch, FILE *fp));
int fGetChar __P((FILE *fp));
char * strsave __P((char *p));
char * strutol __P((char *start));

#endif /* KPARSE_DEFS */
