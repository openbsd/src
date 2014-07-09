/* $LynxId: LYStyle.h,v 1.19 2013/10/22 00:42:18 tom Exp $ */
#ifndef LYSTYLE_H
#define LYSTYLE_H

#include <HTUtils.h>

#ifdef USE_COLOR_STYLE

#include <AttrList.h>
#include <HTMLDTD.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
	char *given;
	char *actual;
    } LSS_NAMES;

    /* list of elements */ extern const SGML_dtd HTML_dtd;

    /* array of currently set styles */
    extern HTCharStyle displayStyles[DSTYLE_ELEMENTS];

    /* Set all the buckets in the hash table to be empty */
    extern void parse_userstyles(void);

    extern void style_defaultStyleSheet(void);

    extern int style_readFromFile(char *file);

    extern void TrimColorClass(const char *tagname,
			       char *styleclassname,
			       int *phcode);

    /* This is an array of styles for tags that don't specify 'class' - the
     * values from that array will be suggested by SGML.c by setting the
     * following variable.  A value of -1 means that the style value should be
     * calculated honestly -HV
     */
    extern int cached_tag_styles[HTML_ELEMENTS];

    /* The style for current tag is suggested in current_tag_style.  If
     * force_current_tag_style =TRUE, then no attempts to calculate the color
     * style for current tag should be made - the value of 'current_tag_style'
     * must be used.
     */
    extern int current_tag_style;
    extern BOOL force_current_tag_style;

    extern BOOL force_classname;

    /* If force_current_tag_style =TRUE, then here will be the classname (this
     * is done to avoid copying the class name to the buffer class_name.
     */
    extern char *forced_classname;

    /* This is called each time lss styles are read.  It will fill each element
     * of 'cached_tag_styles' -HV
     */
    extern void cache_tag_styles(void);

    /* use this for reading the end of string found during last invokation of
     * TrimColorClass.
     */
    extern void FastTrimColorClass(const char *tag_name,
				   unsigned name_len,
				   char *stylename,
				   char **pstylename_end,
				   int *hcode);

    /*
     * Functions for cached-styles
     */
    extern BOOL ValidCachedStyle(int y, int x);
    extern unsigned GetCachedStyle(int y, int x);
    extern void FreeCachedStyles(void);
    extern void ResetCachedStyles(void);
    extern void SetCachedStyle(int y, int x, unsigned value);

#ifdef __cplusplus
}
#endif
#endif				/* USE_COLOR_STYLE */
extern int lynx_has_color;

#endif /* LYSTYLE_H */
