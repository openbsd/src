
#ifndef LYCHARUTILS_H
#define LYCHARUTILS_H

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */

#define CHECK_ID(code) LYCheckForID(me, present, value, (int)code)

typedef enum {
    st_HTML	= 0,	/* attributes and content found in HTML, probably meant for display */
    st_URL,		/* URLs, fragments, NAME and ID */
    st_other
} CharUtil_st;

extern BOOL LYUCFullyTranslateString PARAMS((
	char ** 	str,
	int		cs_from,
	int		cs_to,
	BOOL		use_lynx_specials,
	BOOLEAN 	plain_space,
	BOOLEAN 	hidden,
	CharUtil_st	stype));
extern BOOL LYUCTranslateBackFormData PARAMS((
	char ** 	str,
	int		cs_from,
	int		cs_to,
	BOOLEAN 	plain_space));
extern void LYEntify PARAMS((
	char ** 	str,
	BOOLEAN 	isTITLE));
extern void LYTrimHead PARAMS((
	char *		str));
extern void LYTrimTail PARAMS((
	char *		str));
extern char *LYFindEndOfComment PARAMS((
	char *		str));
extern void LYFillLocalFileURL PARAMS((
	char ** 	href,
	char *		base));
extern void LYAddMETAcharsetToFD PARAMS((
	FILE *			fd,
	int			disp_chndl));

#ifdef Lynx_HTML_Handler
extern int OL_CONTINUE; 	/* flag for whether CONTINUE is set */
extern int OL_VOID;		/* flag for whether a count is set */
extern void LYZero_OL_Counter PARAMS((
	HTStructured *		me));
extern char *LYUppercaseA_OL_String PARAMS((
	int			seqnum));
extern char *LYLowercaseA_OL_String PARAMS((
	int			seqnum));
extern char *LYUppercaseI_OL_String PARAMS((
	int			seqnum));
extern char *LYLowercaseI_OL_String PARAMS((
	int			seqnum));
extern void LYGetChartransInfo PARAMS((
	HTStructured *		me));
extern void LYHandleMETA PARAMS((
	HTStructured *		me,
	CONST BOOL*		present,
	CONST char **		value,
	char ** 		include));
extern void LYHandleP PARAMS((
	HTStructured *		me,
	CONST BOOL*		present,
	CONST char **		value,
	char ** 		include,
	BOOL			start));
extern void LYHandleSELECT PARAMS((
	HTStructured *		me,
	CONST BOOL*		present,
	CONST char **		value,
	char ** 		include,
	BOOL			start));
extern int LYLegitimizeHREF PARAMS((
	HTStructured *		me,
	char ** 		href,
	BOOL			force_slash,
	BOOL			strip_dots));
extern void LYCheckForContentBase PARAMS((
	HTStructured *		me));
extern void LYCheckForID PARAMS((
	HTStructured *		me,
	CONST BOOL *		present,
	CONST char **		value,
	int			attribute));
extern void LYHandleID PARAMS((
	HTStructured *		me,
	char *			id));
extern BOOLEAN LYoverride_default_alignment PARAMS((
	HTStructured *		me));
extern void LYEnsureDoubleSpace PARAMS((
	HTStructured *		me));
extern void LYEnsureSingleSpace PARAMS((
	HTStructured *		me));
extern void LYResetParagraphAlignment PARAMS((
	HTStructured *		me));
extern BOOLEAN LYCheckForCSI PARAMS((
	HTParentAnchor *	anchor,
	char ** 		url));
#endif /* Lynx_HTML_Handler */

#endif /* LYCHARUTILS_H */
