
/*	Specialities of GridText as subclass of HText
*/
#ifndef LYGRIDTEXT_H
#define LYGRIDTEXT_H

#include "HText.h"		/* Superclass */

#ifndef HTFORMS_H
#include "HTForms.h"
#endif /* HTFORMS_H */

#define LY_UNDERLINE_START_CHAR	'\003'
#define LY_UNDERLINE_END_CHAR	'\004'
#define LY_BOLD_START_CHAR	'\005'
#define LY_BOLD_END_CHAR	'\006'
#ifndef LY_SOFT_HYPHEN
#define LY_SOFT_HYPHEN		((char)7)
#endif /* !LY_SOFT_HYPHEN */
#define IsSpecialAttrChar(a)  ((a > '\002') && (a < '\010'))

extern int HTCurSelectGroupType;
extern char * HTCurSelectGroupSize;
extern HText * HTMainText;		/* Equivalent of main window */
extern HTParentAnchor * HTMainAnchor;	/* Anchor for HTMainText */

#ifdef SHORT_NAMES
#define HText_childNumber		HTGTChNu
#define HText_canScrollUp		HTGTCaUp
#define HText_canScrollDown		HTGTCaDo
#define HText_scrollUp			HTGTScUp
#define HText_scrollDown		HTGTScDo
#define HText_scrollTop			HTGTScTo
#define HText_scrollBottom		HTGTScBo
#define HText_sourceAnchors		HTGTSoAn
#define HText_setStale			HTGTStal
#define HText_refresh			HTGTRefr
#endif /* SHORT_NAMES */

extern int WWW_TraceFlag;
extern int HTCacheSize;

extern BOOLEAN mustshow;

#if defined(VMS) && defined(VAXC) && !defined(__DECC)
extern int HTVirtualMemorySize;
#endif /* VMS && VAXC && !__DECC */
extern HTChildAnchor * HText_childNumber PARAMS((int n));
extern void HText_FormDescNumber PARAMS((int n, char **desc));

/*	Is there any file left?
*/
extern BOOL HText_canScrollUp PARAMS((HText * text));
extern BOOL HText_canScrollDown NOPARAMS;

/*	Move display within window
*/
extern void HText_scrollUp PARAMS((HText * text));	/* One page */
extern void HText_scrollDown PARAMS((HText * text));	/* One page */
extern void HText_scrollTop PARAMS((HText * text));
extern void HText_scrollBottom PARAMS((HText * text));
extern void HText_pageDisplay PARAMS((int line_num, char *target));
extern BOOL HText_pageHasPrevTarget NOPARAMS;

extern int HText_LinksInLines PARAMS((HText *text, int line_num, int Lines));

extern void HText_setLastChar PARAMS((HText *text, char ch));
extern char HText_getLastChar PARAMS((HText *text));
extern void HText_setIgnoreExcess PARAMS((HText *text, BOOL ignore));

extern int HText_sourceAnchors PARAMS((HText * text));
extern void HText_setStale PARAMS((HText * text));
extern void HText_refresh PARAMS((HText * text));
extern char * HText_getTitle NOPARAMS;
extern char * HText_getSugFname NOPARAMS;
extern void HTCheckFnameForCompression PARAMS((
	char **			fname,
	HTParentAnchor *	anchor,
	BOOLEAN			strip_ok));
extern char * HText_getLastModified NOPARAMS;
extern char * HText_getDate NOPARAMS;
extern char * HText_getServer NOPARAMS;
extern char * HText_getOwner NOPARAMS;
extern char * HText_getContentBase NOPARAMS;
extern char * HText_getContentLocation NOPARAMS;
#ifdef USE_HASH
extern char * HText_getStyle NOPARAMS;
#endif
extern void HText_setMainTextOwner PARAMS((CONST char * owner));
extern char * HText_getRevTitle NOPARAMS;
extern void print_wwwfile_to_fd PARAMS((FILE * fp, int is_reply));
extern BOOL HText_select PARAMS((HText *text));
extern BOOL HText_POSTReplyLoaded PARAMS((document *doc));
extern BOOL HTFindPoundSelector PARAMS((char *selector));
extern int HTGetLinkInfo PARAMS((
	int		number,
	int		want_go,
	int *		go_line,
	int *		linknum,
	char **		hightext,
	char **		lname));
extern BOOL HText_getFirstTargetInLine PARAMS((
	HText *		text,
	int		line_num,
	BOOL		utf_flag,
	int *		offset,
	int *		tLen,
	char **		data,
	char *		target));
extern int HTisDocumentSource NOPARAMS;
extern void HTuncache_current_document NOPARAMS;
extern int HText_getTopOfScreen NOPARAMS;
extern int HText_getLines PARAMS((HText * text));
extern int HText_getNumOfLines NOPARAMS;
extern int do_www_search PARAMS((document *doc));
extern char * HTLoadedDocumentURL NOPARAMS;
extern char * HTLoadedDocumentPost_data NOPARAMS;
extern char * HTLoadedDocumentTitle NOPARAMS;
extern BOOLEAN HTLoadedDocumentIsHEAD NOPARAMS;
extern BOOLEAN HTLoadedDocumentIsSafe NOPARAMS;
extern char * HTLoadedDocumentCharset NOPARAMS;
extern BOOL HTLoadedDocumentEightbit NOPARAMS;
extern void HText_setNodeAnchorBookmark PARAMS((CONST char *bookmark));
extern char * HTLoadedDocumentBookmark NOPARAMS;
extern int HText_LastLineSize PARAMS((HText *me, BOOL IgnoreSpaces));
extern int HText_PreviousLineSize PARAMS((HText *me, BOOL IgnoreSpaces));
extern void HText_NegateLineOne PARAMS((HText *text));
extern void HText_RemovePreviousLine PARAMS((HText *text));
extern int HText_getCurrentColumn PARAMS((HText *text));
extern int HText_getMaximumColumn PARAMS((HText *text));
extern void HText_setTabID PARAMS((HText *text, CONST char *name));
extern int HText_getTabIDColumn PARAMS((HText *text, CONST char *name));
extern int HText_HiddenLinkCount PARAMS((HText *text));
extern char * HText_HiddenLinkAt PARAMS((HText *text, int number));

/* forms stuff */
extern void HText_beginForm PARAMS((
	char *		action,
	char *		method,
	char *		enctype,
	char *		title,
	CONST char *	accept_cs));
extern void HText_endForm PARAMS((HText *text));
extern void HText_beginSelect PARAMS((char *name,
				      int name_cs,
				      BOOLEAN multiple,
				      char *len));
extern int HText_getOptionNum PARAMS((HText *text));
extern char * HText_setLastOptionValue PARAMS((
	HText *		text,
	char *		value,
	char *		submit_value,
	int 		order,
	BOOLEAN		checked,
	int 		val_cs,
	int 		submit_val_cs));
extern int HText_beginInput PARAMS((
	HText *		text,
	BOOL		underline,
	InputFieldData *I));
extern void HText_SubmitForm PARAMS((
	FormInfo *	submit_item,
	document *	doc,
	char *		link_name,
	char *		link_value));
extern void HText_DisableCurrentForm NOPARAMS;
extern void HText_ResetForm PARAMS((FormInfo *form));
extern void HText_activateRadioButton PARAMS((FormInfo *form));

extern HTList * search_queries; /* Previous isindex and whereis queries */
extern void HTSearchQueries_free NOPARAMS;
extern void HTAddSearchQuery PARAMS((char *query));

extern void user_message PARAMS((
	CONST char *	message,
	CONST char *	argument));

#define _user_message(msg, arg)	mustshow = TRUE, user_message(msg, arg)

extern void www_user_search PARAMS((int start_line, document *doc, char *target));

extern void print_crawl_to_fd PARAMS((
	FILE *		fp,
	char *		thelink,
	char *		thetitle));
extern char * stub_HTAnchor_address PARAMS((HTAnchor *me));

extern void HText_setToolbar PARAMS((HText *text));
extern BOOL HText_hasToolbar PARAMS((HText *text));

extern void HText_setNoCache PARAMS((HText *text));
extern BOOL HText_hasNoCacheSet PARAMS((HText *text));

extern BOOL HText_hasUTF8OutputSet PARAMS((HText *text));
extern void HText_setKcode PARAMS((
	HText *		text,
	CONST char *	charset,
	LYUCcharset *	p_in));

extern void HText_setBreakPoint PARAMS((HText *text));

extern BOOL HText_AreDifferent PARAMS((
	HTParentAnchor *	anchor,
	CONST char *		full_address));

#endif /* LYGRIDTEXT_H */
