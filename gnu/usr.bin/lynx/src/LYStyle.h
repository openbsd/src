#ifndef LYSTYLE_H
#define LYSTYLE_H

#ifdef USE_COLOR_STYLE

#include <AttrList.h>
#include <HTMLDTD.h>

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


extern void TrimColorClass PARAMS((
    CONST char *	tagname,
    char *		styleclassname,
    int *		phcode));


 /* this is an array of styles for tags that don't specify 'class' - the
    values from that array will be suggested by SGML.c by setting the
    following variable. Value of -1 means that style value should be calculated
    honestly. -HV */
extern int cached_tag_styles[HTML_ELEMENTS];

 /* the style for current tag is suggested in current_tag_style. If
    force_current_tag_style =TRUE, then no attempts to calculate the color style
    for current tag should be made - the value of 'current_tag_style' must
    be used.
 */
extern int current_tag_style;
extern BOOL force_current_tag_style;

extern BOOL force_classname;
extern char* forced_classname; /* if force_current_tag_style =TRUE, then here
    will be the classname (this is done to avoid copying the class name to the
    buffer class_name.  */

 /* This is called each time lss styles are read. It will fill
    each elt of 'cached_tag_styles' -HV */
extern void cache_tag_styles NOPARAMS;
 /* this is global var - it can be used for reading the end of string found
   during last invokation of TrimColorClass.
 */
extern void FastTrimColorClass PARAMS ((
	    CONST char*		 tag_name,
	    int			 name_len,
	    char*		 stylename,
	    char**		 pstylename_end,
	    int*		 hcode));

#endif /* USE_COLOR_STYLE */

#endif /* LYSTYLE_H */
