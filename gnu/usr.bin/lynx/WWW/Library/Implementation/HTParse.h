/*                                   HTParse:  URL parsing in the WWW Library
**				HTPARSE
**
**  This module of the WWW library contains code to parse URLs and various
**  related things.
**  Implemented by HTParse.c .
*/
#ifndef HTPARSE_H
#define HTPARSE_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

/*
**  The following are flag bits which may be ORed together to form
**  a number to give the 'wanted' argument to HTParse.
*/
#define PARSE_ACCESS            16
#define PARSE_HOST               8
#define PARSE_PATH               4
#define PARSE_ANCHOR             2
#define PARSE_PUNCTUATION        1
#define PARSE_ALL               31
/*
**  Additional flag bits for more details on components already
**  covered by the above.  The PARSE_PATH above doesn't really
**  strictly refer to the path component in the sense of the URI
**  specs only, but rather to that combined with a possible query
**  component. - kw
*/
#define PARSE_STRICTPATH        32
#define PARSE_QUERY             64

/*
**  The following are valid mask values.  The terms are the BNF names
**  in the URL document.
*/
#define URL_XALPHAS     (unsigned char) 1
#define URL_XPALPHAS    (unsigned char) 2
#define URL_PATH        (unsigned char) 4


/*	Strip white space off a string.				HTStrip()
**	-------------------------------
**
** On exit,
**	Return value points to first non-white character, or to 0 if none.
**	All trailing white space is OVERWRITTEN with zero.
*/
extern char * HTStrip PARAMS((
	char *		s));

/*	Parse a Name relative to another name.			HTParse()
**	--------------------------------------
**
**	This returns those parts of a name which are given (and requested)
**	substituting bits from the related name where necessary.
**
** On entry,
**	aName		A filename given
**      relatedName     A name relative to which aName is to be parsed
**      wanted          A mask for the bits which are wanted.
**
** On exit,
**	returns		A pointer to a malloc'd string which MUST BE FREED
*/
extern char * HTParse PARAMS((
	CONST char *	aName,
	CONST char *	relatedName,
	int		wanted));

/*	Simplify a filename.				HTSimplify()
**	--------------------
**
**  A unix-style file is allowed to contain the seqeunce xxx/../ which may
**  be replaced by "" , and the seqeunce "/./" which may be replaced by "/".
**  Simplification helps us recognize duplicate filenames.
**
**	Thus, 	/etc/junk/../fred 	becomes	/etc/fred
**		/etc/junk/./fred	becomes	/etc/junk/fred
**
**      but we should NOT change
**		http://fred.xxx.edu/../..
**
**	or	../../albert.html
*/
extern void HTSimplify PARAMS((
	char *		filename));

/*	Make Relative Name.					HTRelative()
**	-------------------
**
** This function creates and returns a string which gives an expression of
** one address as related to another.  Where there is no relation, an absolute
** address is retured.
**
**  On entry,
**	Both names must be absolute, fully qualified names of nodes
**	(no anchor bits)
**
**  On exit,
**	The return result points to a newly allocated name which, if
**	parsed by HTParse relative to relatedName, will yield aName.
**	The caller is responsible for freeing the resulting name later.
**
*/
extern char * HTRelative PARAMS((
	CONST char *	aName,
	CONST char *	relatedName));

/*		Escape undesirable characters using %		HTEscape()
**		-------------------------------------
**
**	This function takes a pointer to a string in which
**	some characters may be unacceptable are unescaped.
**	It returns a string which has these characters
**	represented by a '%' character followed by two hex digits.
**
**	Unlike HTUnEscape(), this routine returns a malloc'd string.
*/
extern char * HTEscape PARAMS((
	CONST char *	str,
	unsigned char	mask));

/*		Escape unsafe characters using %                HTEscapeUnsafe()
**		--------------------------------
**
**	This function takes a pointer to a string in which
**	some characters may be that may be unsafe are unescaped.
**	It returns a string which has these characters
**	represented by a '%' character followed by two hex digits.
**
**	Unlike HTUnEscape(), this routine returns a malloc'd string.
*/
extern char * HTEscapeUnsafe PARAMS((
       CONST char *    str));

/*	Escape undesirable characters using % but space to +.	HTEscapeSP()
**	-----------------------------------------------------
**
**	This function takes a pointer to a string in which
**	some characters may be unacceptable are unescaped.
**	It returns a string which has these characters
**	represented by a '%' character followed by two hex digits,
**	except that spaces are converted to '+' instead of %2B.
**
**	Unlike HTUnEscape(), this routine returns a malloc'd string.
*/
extern char * HTEscapeSP PARAMS((
	CONST char *	str,
	unsigned char	mask));

/*	Decode %xx escaped characters.				HTUnEscape()
**	------------------------------
**
**	This function takes a pointer to a string in which some
**	characters may have been encoded in %xy form, where xy is
**	the acsii hex code for character 16x+y.
**	The string is converted in place, as it will never grow.
*/
extern char * HTUnEscape PARAMS((
	char *		str));

/*	Decode some %xx escaped characters.		      HTUnEscapeSome()
**	-----------------------------------			Klaus Weide
**							    (kweide@tezcat.com)
**	This function takes a pointer to a string in which some
**	characters may have been encoded in %xy form, where xy is
**	the acsii hex code for character 16x+y, and a pointer to
**	a second string containing one or more characters which
**	should be unescaped if escaped in the first string.
**	The first string is converted in place, as it will never grow.
*/
extern char * HTUnEscapeSome PARAMS((
	char *		str,
	CONST char *	do_trans));

/*
**  Turn a string which is not a RFC 822 token into a quoted-string. - KW
*/
extern void HTMake822Word PARAMS((
	char **		str));

#endif  /* HTPARSE_H */

/*
   end of HTParse
    */
