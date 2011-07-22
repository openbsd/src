/*
 * $LynxId: HTParse.h,v 1.19 2008/12/14 15:31:47 tom Exp $
 *				HTParse:  URL parsing in the WWW Library
 *				HTPARSE
 *
 *  This module of the WWW library contains code to parse URLs and various
 *  related things.
 *  Implemented by HTParse.c .
 */
#ifndef HTPARSE_H
#define HTPARSE_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
/*
 *  The following are flag bits which may be ORed together to form
 *  a number to give the 'wanted' argument to HTParse.
 */
#define PARSE_ACCESS            16
#define PARSE_HOST               8
#define PARSE_PATH               4
#define PARSE_ANCHOR             2
#define PARSE_PUNCTUATION        1
#define PARSE_ALL               31
#define PARSE_ALL_WITHOUT_ANCHOR  (PARSE_ALL ^ PARSE_ANCHOR)
/*
 *  Additional flag bits for more details on components already
 *  covered by the above.  The PARSE_PATH above doesn't really
 *  strictly refer to the path component in the sense of the URI
 *  specs only, but rather to that combined with a possible query
 *  component. - kw
 */
#define PARSE_STRICTPATH        32
#define PARSE_QUERY             64
/*
 *  The following are valid mask values.  The terms are the BNF names
 *  in the URL document.
 */
#define URL_XALPHAS     UCH(1)
#define URL_XPALPHAS    UCH(2)
#define URL_PATH        UCH(4)
/*	Strip white space off a string.				HTStrip()
 *	-------------------------------
 *
 * On exit,
 *	Return value points to first non-white character, or to 0 if none.
 *	All trailing white space is OVERWRITTEN with zero.
 */ extern char *HTStrip(char *s);

/*
 *	Parse a port number
 *	-------------------
 *
 * On entry,
 *	host            A pointer to hostname possibly followed by port
 *
 * On exit,
 *	returns         A pointer to the ":" before the port
 *	sets            the port number via the pointer portp.
 */
    extern char *HTParsePort(char *host, int *portp);

/*	Parse a Name relative to another name.			HTParse()
 *	--------------------------------------
 *
 *	This returns those parts of a name which are given (and requested)
 *	substituting bits from the related name where necessary.
 *
 * On entry,
 *	aName		A filename given
 *      relatedName     A name relative to which aName is to be parsed
 *      wanted          A mask for the bits which are wanted.
 *
 * On exit,
 *	returns		A pointer to a malloc'd string which MUST BE FREED
 */
    extern char *HTParse(const char *aName,
			 const char *relatedName,
			 int wanted);

/*	HTParseAnchor(), fast HTParse() specialization
 *	----------------------------------------------
 *
 * On exit,
 *	returns		A pointer within input string (probably to its end '\0')
 */
    extern const char *HTParseAnchor(const char *aName);

/*	Simplify a filename.				HTSimplify()
 *	--------------------
 *
 *  A unix-style file is allowed to contain the seqeunce xxx/../ which may
 *  be replaced by "" , and the seqeunce "/./" which may be replaced by "/".
 *  Simplification helps us recognize duplicate filenames.
 *
 *	Thus,	/etc/junk/../fred	becomes /etc/fred
 *		/etc/junk/./fred	becomes	/etc/junk/fred
 *
 *      but we should NOT change
 *		http://fred.xxx.edu/../..
 *
 *	or	../../albert.html
 */
    extern void HTSimplify(char *filename);

/*	Make Relative Name.					HTRelative()
 *	-------------------
 *
 * This function creates and returns a string which gives an expression of
 * one address as related to another.  Where there is no relation, an absolute
 * address is retured.
 *
 *  On entry,
 *	Both names must be absolute, fully qualified names of nodes
 *	(no anchor bits)
 *
 *  On exit,
 *	The return result points to a newly allocated name which, if
 *	parsed by HTParse relative to relatedName, will yield aName.
 *	The caller is responsible for freeing the resulting name later.
 *
 */
    extern char *HTRelative(const char *aName,
			    const char *relatedName);

/*		Escape undesirable characters using %		HTEscape()
 *		-------------------------------------
 *
 *	This function takes a pointer to a string in which
 *	some characters may be unacceptable are unescaped.
 *	It returns a string which has these characters
 *	represented by a '%' character followed by two hex digits.
 *
 *	Unlike HTUnEscape(), this routine returns a malloc'd string.
 */
    extern char *HTEscape(const char *str,
			  unsigned char mask);

/*		Escape unsafe characters using %		HTEscapeUnsafe()
 *		--------------------------------
 *
 *	This function takes a pointer to a string in which
 *	some characters may be that may be unsafe are unescaped.
 *	It returns a string which has these characters
 *	represented by a '%' character followed by two hex digits.
 *
 *	Unlike HTUnEscape(), this routine returns a malloc'd string.
 */
    extern char *HTEscapeUnsafe(const char *str);

/*	Escape undesirable characters using % but space to +.	HTEscapeSP()
 *	-----------------------------------------------------
 *
 *	This function takes a pointer to a string in which
 *	some characters may be unacceptable are unescaped.
 *	It returns a string which has these characters
 *	represented by a '%' character followed by two hex digits,
 *	except that spaces are converted to '+' instead of %2B.
 *
 *	Unlike HTUnEscape(), this routine returns a malloc'd string.
 */
    extern char *HTEscapeSP(const char *str,
			    unsigned char mask);

/*	Decode %xx escaped characters.				HTUnEscape()
 *	------------------------------
 *
 *	This function takes a pointer to a string in which some
 *	characters may have been encoded in %xy form, where xy is
 *	the acsii hex code for character 16x+y.
 *	The string is converted in place, as it will never grow.
 */
    extern char *HTUnEscape(char *str);

/*	Decode some %xx escaped characters.		      HTUnEscapeSome()
 *	-----------------------------------			Klaus Weide
 *							    (kweide@tezcat.com)
 *	This function takes a pointer to a string in which some
 *	characters may have been encoded in %xy form, where xy is
 *	the acsii hex code for character 16x+y, and a pointer to
 *	a second string containing one or more characters which
 *	should be unescaped if escaped in the first string.
 *	The first string is converted in place, as it will never grow.
 */
    extern char *HTUnEscapeSome(char *str,
				const char *do_trans);

/*
 *  Turn a string which is not a RFC 822 token into a quoted-string. - KW
 */
    extern void HTMake822Word(char **str,
			      int quoted);

#ifdef __cplusplus
}
#endif
#endif				/* HTPARSE_H */
