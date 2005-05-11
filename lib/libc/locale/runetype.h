/* $OpenBSD: runetype.h,v 1.2 2005/05/11 18:44:12 espie Exp $ */

#ifndef _NB_RUNETYPE_H_
#define _NB_RUNETYPE_H_

#include <sys/cdefs.h>
#include <sys/types.h>

typedef uint32_t		__nbrune_t;

typedef uint32_t _RuneType;

/*
 * wctype stuffs.
 */
typedef struct _WCTypeEntry {
	char		*te_name;
	_RuneType	te_mask;
} _WCTypeEntry;
#define _WCTYPE_INDEX_ALNUM	0
#define _WCTYPE_INDEX_ALPHA	1
#define _WCTYPE_INDEX_BLANK	2
#define _WCTYPE_INDEX_CNTRL	3
#define _WCTYPE_INDEX_DIGIT	4
#define _WCTYPE_INDEX_GRAPH	5
#define _WCTYPE_INDEX_LOWER	6
#define _WCTYPE_INDEX_PRINT	7
#define _WCTYPE_INDEX_PUNCT	8
#define _WCTYPE_INDEX_SPACE	9
#define _WCTYPE_INDEX_UPPER	10
#define _WCTYPE_INDEX_XDIGIT	11
#define _WCTYPE_NINDEXES	12

#endif
