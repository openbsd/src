/*
 * $LynxId: LYCharUtils.h,v 1.28 2012/02/10 18:36:39 tom Exp $
 */
#ifndef LYCHARUTILS_H
#define LYCHARUTILS_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif /* HTUTILS_H */

#ifndef HTSTREAM_H
#include <HTStream.h>
#endif /* HTSTREAM_H */

#ifdef __cplusplus
extern "C" {
#endif
#define CHECK_ID(code) LYCheckForID(me, present, value, (int)code)
    typedef enum {
	st_HTML = 0,		/* attributes and content found in HTML, probably meant for display */
	st_URL,			/* URLs, fragments, NAME and ID */
	st_other
    } CharUtil_st;

    extern char **LYUCFullyTranslateString(char **str,
					   int cs_from,
					   int cs_to,
					   int do_ent,
					   int use_lynx_specials,
					   int plain_space,
					   int hidden,
					   int Back,
					   CharUtil_st stype);
    extern BOOL LYUCTranslateHTMLString(char **str,
					int cs_from,
					int cs_to,
					int use_lynx_specials,
					int plain_space,
					int hidden,
					CharUtil_st stype);
    extern BOOL LYUCTranslateBackFormData(char **str,
					  int cs_from,
					  int cs_to,
					  int plain_space);
    extern void LYEntify(char **str,
			 int isTITLE);
    extern const char *LYEntifyTitle(char **target, const char *source);
    extern const char *LYEntifyValue(char **target, const char *source);
    extern void LYTrimHead(char *str);
    extern void LYTrimTail(char *str);
    extern char *LYFindEndOfComment(char *str);
    extern void LYFillLocalFileURL(char **href,
				   const char *base);
    extern void LYAddMETAcharsetToFD(FILE *fd,
				     int disp_chndl);
    extern void LYAddMETAcharsetToStream(HTStream *target,
					 int disp_chndl);
    extern void LYformTitle(char **dst,
			    const char *src);
    extern char *LYParseTagParam(char *from,
				 const char *name);
    extern void LYParseRefreshURL(char *content,
				  char **p_seconds,
				  char **p_address);

#ifdef Lynx_HTML_Handler
    extern int OL_CONTINUE;	/* flag for whether CONTINUE is set */
    extern int OL_VOID;		/* flag for whether a count is set */
    extern void LYZero_OL_Counter(HTStructured * me);
    extern char *LYUppercaseA_OL_String(int seqnum);
    extern char *LYLowercaseA_OL_String(int seqnum);
    extern char *LYUppercaseI_OL_String(int seqnum);
    extern char *LYLowercaseI_OL_String(int seqnum);
    extern void LYGetChartransInfo(HTStructured * me);
    extern void LYHandleMETA(HTStructured * me, const BOOL *present,
			     STRING2PTR value,
			     char **include);
    extern void LYHandlePlike(HTStructured * me, const BOOL *present,
			      STRING2PTR value,
			      char **include,
			      int align_idx,
			      int start);
    extern void LYHandleSELECT(HTStructured * me, const BOOL *present,
			       STRING2PTR value,
			       char **include,
			       int start);
    extern int LYLegitimizeHREF(HTStructured * me, char **href,
				int force_slash,
				int strip_dots);
    extern void LYCheckForContentBase(HTStructured * me);
    extern void LYCheckForID(HTStructured * me, const BOOL *present,
			     STRING2PTR value,
			     int attribute);
    extern void LYHandleID(HTStructured * me, const char *id);
    extern BOOLEAN LYoverride_default_alignment(HTStructured * me);
    extern void LYEnsureDoubleSpace(HTStructured * me);
    extern void LYEnsureSingleSpace(HTStructured * me);
    extern void LYResetParagraphAlignment(HTStructured * me);
    extern BOOLEAN LYCheckForCSI(HTParentAnchor *anchor,
				 char **url);

#endif				/* Lynx_HTML_Handler */

#define LYUCTranslateBackHeaderText LYUCTranslateBackFormData

#ifdef __cplusplus
}
#endif
#endif				/* LYCHARUTILS_H */
