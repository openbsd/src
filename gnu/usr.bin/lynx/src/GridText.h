/*	Specialities of GridText as subclass of HText
*/
#ifndef LYGRIDTEXT_H
#define LYGRIDTEXT_H

#include <HText.h>		/* Superclass */

#ifndef HTFORMS_H
#include <HTForms.h>
#endif /* HTFORMS_H */

#include <HTFont.h>

#include <HTCJK.h>

#ifdef __cplusplus
extern "C" {
#endif
#define TABSTOP 8
#define SPACES  "        "	/* must be at least TABSTOP spaces long */
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
*/ extern int HTCurSelectGroupType;
    extern char *HTCurSelectGroupSize;

#if defined(VMS) && defined(VAXC) && !defined(__DECC)
    extern int HTVirtualMemorySize;
#endif				/* VMS && VAXC && !__DECC */

    extern HTChildAnchor *HText_childNextNumber(int n, void **prev);
    extern void HText_FormDescNumber(int n, const char **desc);

/*	Is there any file left?
*/
    extern BOOL HText_canScrollUp(HText *text);
    extern BOOL HText_canScrollDown(void);

/*	Move display within window
*/
    extern void HText_scrollUp(HText *text);	/* One page */
    extern void HText_scrollDown(HText *text);	/* One page */
    extern void HText_scrollTop(HText *text);
    extern void HText_scrollBottom(HText *text);
    extern void HText_pageDisplay(int line_num, char *target);
    extern BOOL HText_pageHasPrevTarget(void);

    extern int HText_LinksInLines(HText *text, int line_num, int Lines);

    extern int HText_getAbsLineNumber(HText *text, int anchor_number);
    extern int HText_closestAnchor(HText *text, int offset);
    extern int HText_locateAnchor(HText *text, int anchor_number);
    extern int HText_anchorRelativeTo(HText *text, int top_lineno, int anchor_num);

    extern void HText_setLastChar(HText *text, char ch);
    extern char HText_getLastChar(HText *text);
    extern void HText_setIgnoreExcess(HText *text, BOOL ignore);

    extern int HText_sourceAnchors(HText *text);
    extern void HText_setStale(HText *text);
    extern void HText_refresh(HText *text);
    extern const char *HText_getTitle(void);
    extern const char *HText_getSugFname(void);
    extern void HTCheckFnameForCompression(char **fname,
					   HTParentAnchor *anchor,
					   BOOLEAN strip_ok);
    extern const char *HText_getLastModified(void);
    extern const char *HText_getDate(void);
    extern const char *HText_getHttpHeaders(void);
    extern const char *HText_getServer(void);
    extern const char *HText_getOwner(void);
    extern const char *HText_getContentBase(void);
    extern const char *HText_getContentLocation(void);
    extern const char *HText_getMessageID(void);
    extern const char *HText_getRevTitle(void);

#ifdef USE_COLOR_STYLE
    extern const char *HText_getStyle(void);
#endif
    extern void HText_setMainTextOwner(const char *owner);
    extern void print_wwwfile_to_fd(FILE *fp, BOOLEAN is_email, BOOLEAN is_reply);
    extern BOOL HText_select(HText *text);
    extern BOOL HText_POSTReplyLoaded(DocInfo *doc);
    extern BOOL HTFindPoundSelector(const char *selector);
    extern int HTGetRelLinkNum(int num, int rel, int cur);
    extern int HTGetLinkInfo(int number,
			     int want_go,
			     int *go_line,
			     int *linknum,
			     char **hightext,
			     char **lname);
    extern BOOL HText_TAHasMoreLines(int curlink,
				     int direction);
    extern int HTGetLinkOrFieldStart(int curlink,
				     int *go_line,
				     int *linknum,
				     int direction,
				     BOOLEAN ta_skip);
    extern BOOL HText_getFirstTargetInLine(HText *text,
					   int line_num,
					   BOOL utf_flag,
					   int *offset,
					   int *tLen,
					   char **data,
					   const char *target);
    extern int HTisDocumentSource(void);
    extern void HTuncache_current_document(void);

#ifdef USE_SOURCE_CACHE
    extern BOOLEAN HTreparse_document(void);
    extern BOOLEAN HTcan_reparse_document(void);
    extern BOOLEAN HTdocument_settings_changed(void);
#endif

    extern BOOL HTLoadedDocumentEightbit(void);
    extern BOOL HText_LastLineEmpty(HText *me, BOOL IgnoreSpaces);
    extern BOOL HText_PreviousLineEmpty(HText *me, BOOL IgnoreSpaces);
    extern BOOL HText_inLineOne(HText *text);
    extern BOOLEAN HTLoadedDocumentIsHEAD(void);
    extern BOOLEAN HTLoadedDocumentIsSafe(void);
    extern bstring *HTLoadedDocumentPost_data(void);
    extern const char *HTLoadedDocumentBookmark(void);
    extern const char *HTLoadedDocumentCharset(void);
    extern const char *HTLoadedDocumentTitle(void);
    extern const char *HTLoadedDocumentURL(void);
    extern const char *HText_HiddenLinkAt(HText *text, int number);
    extern int HText_HiddenLinkCount(HText *text);
    extern int HText_LastLineOffset(HText *me);
    extern int HText_LastLineSize(HText *me, BOOL IgnoreSpaces);
    extern int HText_PreviousLineSize(HText *me, BOOL IgnoreSpaces);
    extern int HText_getCurrentColumn(HText *text);
    extern int HText_getLines(HText *text);
    extern int HText_getMaximumColumn(HText *text);
    extern int HText_getNumOfBytes(void);
    extern int HText_getNumOfLines(void);
    extern int HText_getPreferredTopLine(HText *text, int line_number);
    extern int HText_getTabIDColumn(HText *text, const char *name);
    extern int HText_getTopOfScreen(void);
    extern int do_www_search(DocInfo *doc);
    extern void HText_NegateLineOne(HText *text);
    extern void HText_RemovePreviousLine(HText *text);
    extern void HText_setNodeAnchorBookmark(const char *bookmark);
    extern void HText_setTabID(HText *text, const char *name);
    extern void *HText_pool_calloc(HText *text, unsigned size);

/* "simple table" stuff */
    extern int HText_endStblTABLE(HText *);
    extern int HText_trimCellLines(HText *text);
    extern void HText_cancelStbl(HText *);
    extern void HText_endStblCOLGROUP(HText *);
    extern void HText_endStblTD(HText *);
    extern void HText_endStblTR(HText *);
    extern void HText_startStblCOL(HText *, int, short, BOOL);
    extern void HText_startStblRowGroup(HText *, short);
    extern void HText_startStblTABLE(HText *, short);
    extern void HText_startStblTD(HText *, int, int, short, BOOL);
    extern void HText_startStblTR(HText *, short);

/* forms stuff */
    extern void HText_beginForm(char *action,
				char *method,
				char *enctype,
				char *title,
				const char *accept_cs);
    extern void HText_endForm(HText *text);
    extern void HText_beginSelect(char *name,
				  int name_cs,
				  BOOLEAN multiple,
				  char *len);
    extern int HText_getOptionNum(HText *text);
    extern char *HText_setLastOptionValue(HText *text,
					  char *value,
					  char *submit_value,
					  int order,
					  BOOLEAN checked,
					  int val_cs,
					  int submit_val_cs);
    extern int HText_beginInput(HText *text,
				BOOL underline,
				InputFieldData * I);
    extern void HText_endInput(HText *text);
    extern int HText_SubmitForm(FormInfo * submit_item, DocInfo *doc,
				char *link_name,
				char *link_value);
    extern void HText_DisableCurrentForm(void);
    extern void HText_ResetForm(FormInfo * form);
    extern void HText_activateRadioButton(FormInfo * form);
    extern BOOLEAN HText_HaveUserChangedForms(HText *text);

    extern HTList *search_queries;	/* Previous isindex and whereis queries */
    extern void HTSearchQueries_free(void);
    extern void HTAddSearchQuery(char *query);

    extern void user_message(const char *message,
			     const char *argument);

#define _user_message(msg, arg)	mustshow = TRUE, user_message(msg, arg)

    extern void www_user_search(int start_line,
				DocInfo *doc,
				char *target,
				int direction);

    extern void print_crawl_to_fd(FILE *fp,
				  char *thelink,
				  char *thetitle);
    extern char *stub_HTAnchor_address(HTAnchor * me);

    extern void HText_setToolbar(HText *text);
    extern BOOL HText_hasToolbar(HText *text);

    extern void HText_setNoCache(HText *text);
    extern BOOL HText_hasNoCacheSet(HText *text);

    extern BOOL HText_hasUTF8OutputSet(HText *text);
    extern void HText_setKcode(HText *text,
			       const char *charset,
			       LYUCcharset *p_in);

    extern void HText_setBreakPoint(HText *text);

    extern BOOL HText_AreDifferent(HTParentAnchor *anchor,
				   const char *full_address);

    extern int HText_ExtEditForm(LinkInfo * form_link);
    extern void HText_ExpandTextarea(LinkInfo * form_link, int newlines);
    extern int HText_InsertFile(LinkInfo * form_link);

    extern void redraw_lines_of_link(int cur);
    extern void LYMoveToLink(int cur,
			     const char *target,
			     const char *hightext,
			     int flag,
			     BOOL inU,
			     BOOL utf_flag);

#ifdef USE_PRETTYSRC
    extern void HTMark_asSource(void);
#endif

    extern int HTMainText_Get_UCLYhndl(void);

#ifdef KANJI_CODE_OVERRIDE
    extern HTkcode last_kcode;
#endif

    extern HTkcode HText_getKcode(HText *text);
    extern void HText_updateKcode(HText *text, HTkcode kcode);
    extern HTkcode HText_getSpecifiedKcode(HText *text);
    extern void HText_updateSpecifiedKcode(HText *text, HTkcode kcode);

#ifdef __cplusplus
}
#endif
#endif				/* LYGRIDTEXT_H */
