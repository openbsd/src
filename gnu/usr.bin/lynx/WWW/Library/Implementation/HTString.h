/*                                                   String handling for libwww
                                         STRINGS
                                             
   Case-independent string comparison and allocations with copies etc
   
 */
#ifndef HTSTRING_H
#define HTSTRING_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif /* HTUTILS_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern const char *HTLibraryVersion;	/* String for help screen etc */

/*
    EBCDIC string comparison using ASCII collating sequence
*/
#ifdef    NOT_ASCII
    extern int AS_casecomp(const char *a, const char *b);
    extern int AS_ncmp(const char *a, const char *b, unsigned int n);

#define    AS_cmp( a, b )  ( AS_ncmp( ( a ), ( b ), -1 ) )

#else
#define AS_casecomp( a, b ) ( strcasecomp( ( a ), ( b ) ) )
#define AS_ncmp( a, b, c )  ( strncmp( ( a ), ( b ), ( c ) ) )
#define AS_cmp strcmp

#endif				/* NOT_ASCII */

/*

Case-insensitive string comparison

   The usual routines (comp instead of cmp) had some problem.
   
 */
    extern int strcasecomp(const char *a, const char *b);
    extern int strncasecomp(const char *a, const char *b, int n);

    extern int strcasecomp8(const char *a, const char *b);
    extern int strncasecomp8(const char *a, const char *b, int n);

    extern int strcasecomp_asterisk(const char *a, const char *b);

    /*
     * strcasecomp8 and strncasecomp8 are variants of strcasecomp and
     * strncasecomp, but use 8bit upper/lower case information from the
     * current display charset
     */

/*

Malloced string manipulation

 */
#define StrAllocCopy(dest, src) HTSACopy (&(dest), src)
#define StrAllocCat(dest, src)  HTSACat  (&(dest), src)
    extern char *HTSACopy(char **dest, const char *src);
    extern char *HTSACat(char **dest, const char *src);

/*
optimized for heavily realloc'd strings in temp objects
*/
#define StrAllocCopy_extra(dest, src) HTSACopy_extra (&(dest), src)
#define FREE_extra(x)   {if (x != NULL) {HTSAFree_extra(x); x = NULL;}}
#define Clear_extra(x)  {if (x != NULL) {*x = '\0';}}
    extern char *HTSACopy_extra(char **dest, const char *src);
    extern void HTSAFree_extra(char *s);

/*

Next word or quoted string

 */
    extern char *HTNextField(char **pstr);

/* A more general parser - kw */
    extern char *HTNextTok(char **pstr,
			   const char *delims, const char *bracks, char *found);

    extern char *HTSprintf(char **pstr, const char *fmt,...) GCC_PRINTFLIKE(2,3);
    extern char *HTSprintf0(char **pstr, const char *fmt,...) GCC_PRINTFLIKE(2,3);

#if defined(LY_FIND_LEAKS)	/* private otherwise */
    extern char *StrAllocVsprintf(char **pstr,
				  size_t len,
				  const char *fmt,
				  va_list * ap);
#endif

#if (defined(VMS) || defined(DOSPATH) || defined(__EMX__)) && !defined(__CYGWIN__)
#define USE_QUOTED_PARAMETER 0
#else
#define USE_QUOTED_PARAMETER 1
#endif

#if USE_QUOTED_PARAMETER
    extern char *HTQuoteParameter(const char *parameter);
    extern void HTAddXpand(char **result, const char *command, int number, const char *parameter);

#else
#define HTQuoteParameter(parameter) parameter	/* simplify ifdef'ing */
#define HTAddXpand(result,command,number,parameter)  HTAddParam(result,command,number,parameter)
#endif

    extern int HTCountCommandArgs(const char *command);
    extern void HTAddToCmd(char **result, const char *command, int number, const char *string);
    extern void HTAddParam(char **result, const char *command, int number, const char *parameter);
    extern void HTEndParam(char **result, const char *command, int number);

/* Force an option, with leading blanks, to be appended without quoting them */
#define HTOptParam(result, command, number, parameter) HTSACat(result, parameter)

/* Binary copy and concat */
    typedef struct {
	char *str;
	int len;
    } bstring;

    extern void HTSABCopy(bstring **dest, const char *src, int len);
    extern void HTSABCopy0(bstring **dest, const char *src);
    extern void HTSABCat(bstring **dest, const char *src, int len);
    extern void HTSABCat0(bstring **dest, const char *src);
    extern BOOL HTSABEql(bstring *a, bstring *b);
    extern void HTSABFree(bstring **ptr);

#define BStrLen(s)    (((s) != 0) ? (s)->len : 0)
#define BStrData(s)   (((s) != 0) ? (s)->str : 0)

#define BINEQ(a,b)    (HTSABEql(a,b))	/* like STREQ() */

#define isBEmpty(p)   ((p) == 0 || BStrLen(p) == 0)

#define BStrCopy(d,s)  HTSABCopy(  &(d), BStrData(s), BStrLen(s))
#define BStrCopy0(d,s) HTSABCopy0( &(d), s)
#define BStrCat(d,s)   HTSABCat(   &(d), BStrData(s), BStrLen(s))
#define BStrCat0(d,s)  HTSABCat0(  &(d), s)
#define BStrFree(d)    HTSABFree(  &(d))

    extern bstring *HTBprintf(bstring **pstr, const char *fmt,...) GCC_PRINTFLIKE(2,3);

    extern void trace_bstring(bstring *data);
    extern void trace_bstring2(const char *text, int size);

#ifdef __cplusplus
}
#endif
#endif				/* HTSTRING_H */
