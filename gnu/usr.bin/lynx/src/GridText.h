
/*	Specialities of GridText as subclass of HText
*/
#ifndef LYGRIDTEXT_H
#define LYGRIDTEXT_H

#include <HText.h>		/* Superclass */

#ifndef HTFORMS_H
#include <HTForms.h>
#endif /* HTFORMS_H */

#ifndef HT_NON_BREAK_SPACE
#define HT_NON_BREAK_SPACE	((char)1)	/* remember it */
#endif /* !HT_NON_BREAK_SPACE */
#ifndef HT_EN_SPACE
#define HT_EN_SPACE		((char)2)	/* remember it */
#endif /* !HT_EN_SPACE */
#define LY_UNDERLINE_START_CHAR	'\003'
#define LY_UNDERLINE_END_CHAR	'\004'

/* Turn about is fair play ASCII platforms use EBCDIC tab;
   EBCDIC platforms use ASCII tab for LY_BOLD_START_CHAR.
*/
#ifdef EBCDIC
#define LY_BOLD_START_CHAR	'\011'
#else
#define LY_BOLD_START_CHAR	'\005'
#endif

#define LY_BOLD_END_CHAR	'\006'
#ifndef LY_SOFT_HYPHEN
#define LY_SOFT_HYPHEN		((char)7)
#endif /* !LY_SOFT_HYPHEN */
#define LY_SOFT_NEWLINE 	((char)8)

#ifdef EBCDIC
#define IsSpecialAttrChar(a)  (((a) > '\002') && ((a) <= '\011') && ((a)!='\t'))
#else
#define IsSpecialAttrChar(a)  (((a) > '\002') && ((a) <= '\010'))
#endif

#define TABSTOP 8
#define SPACES  "        "  /* must be at least TABSTOP spaces long */
#define SPLAT   '.'

#define NOCHOP 0
#define CHOP   1

/* just for information:
US-ASCII control characters <32 which are not defined in Unicode standard
=00	U+0000	NULL
=01	U+0001	START OF HEADING
=02	U+0002	START OF TEXT
=03	U+0003	END OF TEXT
=04	U+0004	END OF TRANSMISSION
=05	U+0005	ENQUIRY
=06	U+0006	ACKNOWLEDGE
=07	U+0007	BELL
=08	U+0008	BACKSPACE
=09	U+0009	HORIZONTAL TABULATION
=0A	U+000A	LINE FEED
=0B	U+000B	VERTICAL TABULATION
=0C	U+000C	FORM FEED
=0D	U+000D	CARRIAGE RETURN
=0E	U+000E	SHIFT OUT
=0F	U+000F	SHIFT IN
=10	U+0010	DATA LINK ESCAPE
=11	U+0011	DEVICE CONTROL ONE
=12	U+0012	DEVICE CONTROL TWO
=13	U+0013	DEVICE CONTROL THREE
=14	U+0014	DEVICE CONTROL FOUR
=15	U+0015	NEGATIVE ACKNOWLEDGE
=16	U+0016	SYNCHRONOUS IDLE
=17	U+0017	END OF TRANSMISSION BLOCK
=18	U+0018	CANCEL
=19	U+0019	END OF MEDIUM
=1A	U+001A	SUBSTITUTE
=1B	U+001B	ESCAPE
=1C	U+001C	FILE SEPARATOR
=1D	U+001D	GROUP SEPARATOR
=1E	U+001E	RECORD SEPARATOR
=1F	U+001F	UNIT SEPARATOR
=7F	U+007F	DELETE
*/

extern int HTCurSelectGroupType;
extern char * HTCurSelectGroupSize;
extern HText * HTMainText;		/* Equivalent of main window */
extern HTParentAnchor * HTMainAnchor;	/* Anchor for HTMainText */

extern int WWW_TraceFlag;

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
extern CONST char * HText_getTitle NOPARAMS;
extern CONST char * HText_getSugFname NOPARAMS;
extern void HTCheckFnameForCompression PARAMS((
	char **			fname,
	HTParentAnchor *	anchor,
	BOOLEAN			strip_ok));
extern CONST char * HText_getLastModified NOPARAMS;
extern CONST char * HText_getDate NOPARAMS;
extern CONST char * HText_getServer NOPARAMS;
extern CONST char * HText_getOwner NOPARAMS;
extern CONST char * HText_getContentBase NOPARAMS;
extern CONST char * HText_getContentLocation NOPARAMS;
extern CONST char * HText_getMessageID NOPARAMS;
extern CONST char * HText_getRevTitle NOPARAMS;
#ifdef USE_HASH
extern CONST char * HText_getStyle NOPARAMS;
#endif
extern void HText_setMainTextOwner PARAMS((CONST char * owner));
extern void print_wwwfile_to_fd PARAMS((FILE * fp, int is_reply));
extern BOOL HText_select PARAMS((HText *text));
extern BOOL HText_POSTReplyLoaded PARAMS((document *doc));
extern BOOL HTFindPoundSelector PARAMS((char *selector));
extern int HTGetRelLinkNum PARAMS((int num, int rel, int cur));
extern int HTGetLinkInfo PARAMS((
	int		number,
	int		want_go,
	int *		go_line,
	int *		linknum,
	char **		hightext,
	char **		lname));
extern int HTGetLinkOrFieldStart PARAMS((
	int		curlink,
	int *		go_line,
	int *		linknum,
	int		direction,
	BOOLEAN		ta_skip));
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
#ifdef SOURCE_CACHE
extern BOOLEAN HTreparse_document NOPARAMS;
extern BOOLEAN HTcan_reparse_document NOPARAMS;
extern BOOLEAN HTdocument_settings_changed NOPARAMS;
#endif
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
extern BOOL HText_inLineOne PARAMS((HText *text));
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
extern void HText_trimHightext PARAMS((
	HText *		text,
	BOOLEAN 	final));
extern void HText_SubmitForm PARAMS((
	FormInfo *	submit_item,
	document *	doc,
	char *		link_name,
	char *		link_value));
extern void HText_DisableCurrentForm NOPARAMS;
extern void HText_ResetForm PARAMS((FormInfo *form));
extern void HText_activateRadioButton PARAMS((FormInfo *form));
extern BOOLEAN HText_HaveUserChangedForms NOPARAMS;

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

extern int HText_ExtEditForm PARAMS((
	struct link *	form_link));
extern void HText_ExpandTextarea PARAMS((
	struct link *	form_link,
	int             newlines));
extern int HText_InsertFile PARAMS((
	struct link *	form_link));

extern void redraw_lines_of_link PARAMS((int cur));

#ifdef USE_PSRC
extern void HTMark_asSource NOPARAMS;
#endif

extern int HTMainText_Get_UCLYhndl NOPARAMS;

#endif /* LYGRIDTEXT_H */
