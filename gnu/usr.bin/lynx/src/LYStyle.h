#ifndef LYSTYLE_H
#define LYSTYLE_H

#ifdef USE_COLOR_STYLE

#include "AttrList.h"

/* list of elements */
extern CONST SGML_dtd HTML_dtd;

/* array of currently set styles */
extern HTCharStyle displayStyles[DSTYLE_ELEMENTS];

/* Can we do colour? - RP */
extern int lynx_has_color;

/* Set all the buckets in the hash table to be empty */
extern void style_initialiseHashTable NOPARAMS;

extern void parse_userstyles NOPARAMS;

extern void HStyle_addStyle PARAMS((char* buffer));

extern void style_deleteStyleList NOPARAMS;

extern void style_defaultStyleSheet NOPARAMS;

extern int style_readFromFile PARAMS((char* file));

#endif /* USE_COLOR_STYLE */

#endif /* LYSTYLE_H */
