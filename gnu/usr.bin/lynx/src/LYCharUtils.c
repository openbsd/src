/*
**  Functions associated with LYCharSets.c and the Lynx version of HTML.c - FM
**  ==========================================================================
*/
#include <HTUtils.h>
#include <SGML.h>

#define Lynx_HTML_Handler
#include <HTChunk.h>
#include <HText.h>
#include <HTStyle.h>
#include <HTMIME.h>
#include <HTML.h>

#include <HTCJK.h>
#include <HTAtom.h>
#include <HTMLGen.h>
#include <HTParse.h>
#include <UCMap.h>
#include <UCDefs.h>
#include <UCAux.h>

#include <LYGlobalDefs.h>
#include <LYCharUtils.h>
#include <LYCharSets.h>

#include <HTAlert.h>
#include <HTFont.h>
#include <HTForms.h>
#include <HTNestedList.h>
#include <GridText.h>
#include <LYStrings.h>
#include <LYUtils.h>
#include <LYMap.h>
#include <LYBookmark.h>
#include <LYCurses.h>
#include <LYCookie.h>

#include <LYexit.h>
#include <LYLeaks.h>

extern BOOL HTPassEightBitRaw;
extern BOOL HTPassEightBitNum;
extern BOOL HTPassHighCtrlRaw;
extern BOOL HTPassHighCtrlNum;
extern HTkcode kanji_code;
extern HTCJKlang HTCJK;

/*
 *  Used for nested lists. - FM
 */
PUBLIC int OL_CONTINUE = -29999;     /* flag for whether CONTINUE is set */
PUBLIC int OL_VOID = -29998;	     /* flag for whether a count is set */


/*
**  This function converts any ampersands in allocated
**  strings to "&amp;".  If isTITLE is TRUE, it also
**  converts any angle-brackets to "&lt;" or "&gt;". - FM
*/
PUBLIC void LYEntify ARGS2(
	char **,	str,
	BOOLEAN,	isTITLE)
{
    char *p = *str;
    char *q = NULL, *cp = NULL;
    int amps = 0, lts = 0, gts = 0;

    if (p == NULL || *p == '\0')
	return;

    /*
     *	Count the ampersands. - FM
     */
    while ((*p != '\0') && (q = strchr(p, '&')) != NULL) {
	amps++;
	p = (q + 1);
    }

    /*
     *	Count the left-angle-brackets, if needed. - FM
     */
    if (isTITLE == TRUE) {
	p = *str;
	while ((*p != '\0') && (q = strchr(p, '<')) != NULL) {
	    lts++;
	    p = (q + 1);
	}
    }

    /*
     *	Count the right-angle-brackets, if needed. - FM
     */
    if (isTITLE == TRUE) {
	p = *str;
	while ((*p != '\0') && (q = strchr(p, '>')) != NULL) {
	    gts++;
	    p = (q + 1);
	}
    }

    /*
     *	Check whether we need to convert anything. - FM
     */
    if (amps == 0 && lts == 0 && gts == 0)
	return;

    /*
     *	Allocate space and convert. - FM
     */
    q = (char *)calloc(1,
		     (strlen(*str) + (4 * amps) + (3 * lts) + (3 * gts) + 1));
    if ((cp = q) == NULL)
	outofmem(__FILE__, "LYEntify");
    for (p = *str; *p; p++) {
	if (*p == '&') {
	    *q++ = '&';
	    *q++ = 'a';
	    *q++ = 'm';
	    *q++ = 'p';
	    *q++ = ';';
	} else if (isTITLE && *p == '<') {
	    *q++ = '&';
	    *q++ = 'l';
	    *q++ = 't';
	    *q++ = ';';
	} else if (isTITLE && *p == '>') {
	    *q++ = '&';
	    *q++ = 'g';
	    *q++ = 't';
	    *q++ = ';';
	} else {
	    *q++ = *p;
	}
    }
    StrAllocCopy(*str, cp);
    FREE(cp);
}

/*
**  This function trims characters <= that of a space (32),
**  including HT_NON_BREAK_SPACE (1) and HT_EN_SPACE (2),
**  but not ESC, from the heads of strings. - FM
*/
PUBLIC void LYTrimHead ARGS1(
	char *, str)
{
    int i = 0, j;

    if (!str || *str == '\0')
	return;

    while (str[i] != '\0' && WHITE(str[i]) && (unsigned char)str[i] != (unsigned char)CH_ESC)   /* S/390 -- gil -- 1669 */
	i++;
    if (i > 0) {
	for (j = 0; str[i] != '\0'; i++) {
	    str[j++] = str[i];
	}
	str[j] = '\0';
    }
}

/*
**  This function trims characters <= that of a space (32),
**  including HT_NON_BREAK_SPACE (1), HT_EN_SPACE (2), and
**  ESC from the tails of strings. - FM
*/
PUBLIC void LYTrimTail ARGS1(
	char *, str)
{
    int i;

    if (!str || *str == '\0')
	return;

    i = (strlen(str) - 1);
    while (i >= 0) {
	if (WHITE(str[i]))
	    str[i] = '\0';
	else
	    break;
	i--;
    }
}

/*
** This function should receive a pointer to the start
** of a comment.  It returns a pointer to the end ('>')
** character of comment, or it's best guess if the comment
** is invalid. - FM
*/
PUBLIC char *LYFindEndOfComment ARGS1(
	char *, str)
{
    char *cp, *cp1;
    enum comment_state { start1, start2, end1, end2 } state;

    if (str == NULL)
	/*
	 *  We got NULL, so return NULL. - FM
	 */
	return NULL;

    if (strncmp(str, "<!--", 4))
	/*
	 *  We don't have the start of a comment, so
	 *  return the beginning of the string. - FM
	 */
	return str;

    cp = (str + 4);
    if (*cp =='>')
	/*
	 * It's an invalid comment, so
	 * return this end character. - FM
	 */
	return cp;

    if ((cp1 = strchr(cp, '>')) == NULL)
	/*
	 *  We don't have an end character, so
	 *  return the beginning of the string. - FM
	 */
	return str;

    if (*cp == '-')
	/*
	 *  Ugh, it's a "decorative" series of dashes,
	 *  so return the next end character. - FM
	 */
	return cp1;

    /*
     *	OK, we're ready to start parsing. - FM
     */
    state = start2;
    while (*cp != '\0') {
	switch (state) {
	    case start1:
		if (*cp == '-')
		    state = start2;
		else
		    /*
		     *	Invalid comment, so return the first
		     *	'>' from the start of the string. - FM
		     */
		    return cp1;
		break;

	    case start2:
		if (*cp == '-')
		    state = end1;
		break;

	    case end1:
		if (*cp == '-')
		    state = end2;
		else
		    /*
		     *	Invalid comment, so return the first
		     *	'>' from the start of the string. - FM
		     */
		    return cp1;
		break;

	    case end2:
		if (*cp == '>')
		    /*
		     *	Valid comment, so return the end character. - FM
		     */
		    return cp;
		if (*cp == '-') {
		    state = start1;
		} else if (!(WHITE(*cp) && (unsigned char)*cp != (unsigned char)CH_ESC)) {  /* S/390 -- gil -- 1686 */
		    /*
		     *	Invalid comment, so return the first
		     *	'>' from the start of the string. - FM
		     */
		    return cp1;
		 }
		break;

	    default:
		break;
	}
	cp++;
    }

    /*
     *	Invalid comment, so return the first
     *	'>' from the start of the string. - FM
     */
    return cp1;
}

/*
**  If an HREF, itself or if resolved against a base,
**  represents a file URL, and the host is defaulted,
**  force in "//localhost".  We need this until
**  all the other Lynx code which performs security
**  checks based on the "localhost" string is changed
**  to assume "//localhost" when a host field is not
**  present in file URLs - FM
*/
PUBLIC void LYFillLocalFileURL ARGS2(
	char **,	href,
	CONST char *, 	base)
{
    char * temp = NULL;

    if (*href == NULL || *(*href) == '\0')
	return;

    if (!strcmp(*href, "//") || !strncmp(*href, "///", 3)) {
	if (base != NULL && !strncmp(base, "file:", 5)) {
	    StrAllocCopy(temp, "file:");
	    StrAllocCat(temp, *href);
	    StrAllocCopy(*href, temp);
	}
    }
    if (!strncmp(*href, "file:", 5)) {
	if (*(*href+5) == '\0') {
	    StrAllocCat(*href, "//localhost");
	} else if (!strcmp(*href, "file://")) {
	    StrAllocCat(*href, "localhost");
	} else if (!strncmp(*href, "file:///", 8)) {
	    StrAllocCopy(temp, (*href+7));
	    LYLocalFileToURL (href, temp);
	} else if (!strncmp(*href, "file:/", 6) && !LYIsHtmlSep(*(*href+6))) {
	    StrAllocCopy(temp, (*href+5));
	    LYLocalFileToURL (href, temp);
	}
    }

#if defined(DOSPATH) || defined(__EMX__)
    if (isalpha(*(*href)) && (*(*href+1) == ':'))  {
	/*
	 * If it's a local DOS path beginning with drive letter,
	 * add file://localhost/ prefix and go ahead.
	 */
	StrAllocCopy(temp, *href);
	LYLocalFileToURL (href, temp);
    }

    /* use below: strlen("file://localhost/") = 17 */
    if (!strncmp(*href, "file://localhost/", 17)
	  && (strlen(*href) == 19)
	  && isalpha(*(*href+17))
	  && (*(*href+18) == ':')) {
	/*
	 * Terminate DOS drive letter with a slash to surf root successfully.
	 * Here seems a proper place to do so.
	 */
	StrAllocCat(*href, "/");
    }
#endif /* DOSPATH */

    /*
     * No path in a file://localhost URL means a
     * directory listing for the current default. - FM
     */
    if (!strcmp(*href, "file://localhost")) {
	char *temp2;
#ifdef VMS
	temp2 = HTVMS_wwwName(getenv("PATH"));
#else
	char curdir[LY_MAXPATH];
	temp2 = wwwName(Current_Dir(curdir));
#endif /* VMS */
	LYAddHtmlSep(href);
	/*
	 *  Check for pathological cases - current dir has chars which
	 *  MUST BE URL-escaped - kw
	 */
	if (strchr(temp2, '%') != NULL || strchr(temp2, '#') != NULL) {
	    FREE(temp);
	    temp = HTEscape(temp2, URL_PATH);
	    StrAllocCat(*href, temp);
	} else {
	    StrAllocCat(*href, temp2);
	}
    }

#ifdef VMS
    /*
     * On VMS, a file://localhost/ URL means
     * a listing for the login directory. - FM
     */
    if (!strcmp(*href, "file://localhost/"))
	StrAllocCat(*href, (HTVMS_wwwName((char *)Home_Dir())+1));
#endif /* VMS */

    FREE(temp);
    return;
}

/*
**  This function writes a line with a META tag to an open file,
**  which will specify a charset parameter to use when the file is
**  read back in.  It is meant for temporary HTML files used by the
**  various special pages which may show titles of documents.  When those
**  files are created, the title strings normally have been translated and
**  expanded to the display character set, so we have to make sure they
**  don't get translated again.
**  If the user has changed the display character set during the lifetime
**  of the Lynx session (or, more exactly, during the time the title
**  strings to be written were generated), they may now have different
**  character encodings and there is currently no way to get it all right.
**  To change this, we would have to add a variable for each string which
**  keeps track of its character encoding.
**  But at least we can try to ensure that reading the file after future
**  display character set changes will give reasonable output.
**
**  The META tag is not written if the display character set (passed as
**  disp_chndl) already corresponds to the charset assumption that
**  would be made when the file is read. - KW
**
**  Currently this function is used for temporary files like "Lynx Info Page"
**  and for one permanent - bookmarks (so it may be a problem if you change
**  the display charset later: new bookmark entries may be mistranslated).
**								 - LP
*/
PUBLIC void LYAddMETAcharsetToFD ARGS2(
	FILE *, 	fd,
	int,		disp_chndl)
{
    if (disp_chndl == -1)
	/*
	 *  -1 means use current_char_set.
	 */
	disp_chndl = current_char_set;

    if (fd == NULL || disp_chndl < 0)
	/*
	 *  Should not happen.
	 */
	return;

    if (UCLYhndl_HTFile_for_unspec == disp_chndl)
	/*
	 *  Not need to do, so we don't.
	 */
	return;

    if (LYCharSet_UC[disp_chndl].enc == UCT_ENC_7BIT)
	/*
	 *  There shouldn't be any 8-bit characters in this case.
	 */
	return;

    /*
     *	In other cases we don't know because UCLYhndl_for_unspec may
     *	change during the lifetime of the file (by toggling raw mode
     *	or changing the display character set), so proceed.
     */
    fprintf(fd, "<META %s content=\"text/html;charset=%s\">\n",
		"http-equiv=\"content-type\"",
		LYCharSet_UC[disp_chndl].MIMEname);
}

/*
** This function returns OL TYPE="A" strings in
** the range of " A." (1) to "ZZZ." (18278). - FM
*/
PUBLIC char *LYUppercaseA_OL_String ARGS1(
	int, seqnum)
{
    static char OLstring[8];

    if (seqnum <= 1 ) {
	strcpy(OLstring, " A.");
	return OLstring;
    }
    if (seqnum < 27) {
	sprintf(OLstring, " %c.", (seqnum + 64));
	return OLstring;
    }
    if (seqnum < 703) {
	sprintf(OLstring, "%c%c.", ((seqnum-1)/26 + 64),
		(seqnum - ((seqnum-1)/26)*26 + 64));
	return OLstring;
    }
    if (seqnum < 18279) {
	sprintf(OLstring, "%c%c%c.", ((seqnum-27)/676 + 64),
		(((seqnum - ((seqnum-27)/676)*676)-1)/26 + 64),
		(seqnum - ((seqnum-1)/26)*26 + 64));
	return OLstring;
    }
    strcpy(OLstring, "ZZZ.");
    return OLstring;
}

/*
** This function returns OL TYPE="a" strings in
** the range of " a." (1) to "zzz." (18278). - FM
*/
PUBLIC char *LYLowercaseA_OL_String ARGS1(
	int, seqnum)
{
    static char OLstring[8];

    if (seqnum <= 1 ) {
	strcpy(OLstring, " a.");
	return OLstring;
    }
    if (seqnum < 27) {
	sprintf(OLstring, " %c.", (seqnum + 96));
	return OLstring;
    }
    if (seqnum < 703) {
	sprintf(OLstring, "%c%c.", ((seqnum-1)/26 + 96),
		(seqnum - ((seqnum-1)/26)*26 + 96));
	return OLstring;
    }
    if (seqnum < 18279) {
	sprintf(OLstring, "%c%c%c.", ((seqnum-27)/676 + 96),
		(((seqnum - ((seqnum-27)/676)*676)-1)/26 + 96),
		(seqnum - ((seqnum-1)/26)*26 + 96));
	return OLstring;
    }
    strcpy(OLstring, "zzz.");
    return OLstring;
}

/*
** This function returns OL TYPE="I" strings in the
** range of " I." (1) to "MMM." (3000).- FM
*/
PUBLIC char *LYUppercaseI_OL_String ARGS1(
	int, seqnum)
{
    static char OLstring[8];
    int Arabic = seqnum;

    if (Arabic >= 3000) {
	strcpy(OLstring, "MMM.");
	return OLstring;
    }

    switch(Arabic) {
    case 1:
	strcpy(OLstring, " I.");
	return OLstring;
    case 5:
	strcpy(OLstring, " V.");
	return OLstring;
    case 10:
	strcpy(OLstring, " X.");
	return OLstring;
    case 50:
	strcpy(OLstring, " L.");
	return OLstring;
    case 100:
	strcpy(OLstring, " C.");
	return OLstring;
    case 500:
	strcpy(OLstring, " D.");
	return OLstring;
    case 1000:
	strcpy(OLstring, " M.");
	return OLstring;
    default:
	OLstring[0] = '\0';
	break;
    }

    while (Arabic >= 1000) {
	strcat(OLstring, "M");
	Arabic -= 1000;
    }

    if (Arabic >= 900) {
	strcat(OLstring, "CM");
	Arabic -= 900;
    }

    if (Arabic >= 500) {
	strcat(OLstring, "D");
	Arabic -= 500;
	while (Arabic >= 500) {
	    strcat(OLstring, "C");
	    Arabic -= 10;
	}
    }

    if (Arabic >= 400) {
	strcat(OLstring, "CD");
	Arabic -= 400;
    }

    while (Arabic >= 100) {
	strcat(OLstring, "C");
	Arabic -= 100;
    }

    if (Arabic >= 90) {
	strcat(OLstring, "XC");
	Arabic -= 90;
    }

    if (Arabic >= 50) {
	strcat(OLstring, "L");
	Arabic -= 50;
	while (Arabic >= 50) {
	    strcat(OLstring, "X");
	    Arabic -= 10;
	}
    }

    if (Arabic >= 40) {
	strcat(OLstring, "XL");
	Arabic -= 40;
    }

    while (Arabic > 10) {
	strcat(OLstring, "X");
	Arabic -= 10;
    }

    switch (Arabic) {
    case 1:
	strcat(OLstring, "I.");
	break;
    case 2:
	strcat(OLstring, "II.");
	break;
    case 3:
	strcat(OLstring, "III.");
	break;
    case 4:
	strcat(OLstring, "IV.");
	break;
    case 5:
	strcat(OLstring, "V.");
	break;
    case 6:
	strcat(OLstring, "VI.");
	break;
    case 7:
	strcat(OLstring, "VII.");
	break;
    case 8:
	strcat(OLstring, "VIII.");
	break;
    case 9:
	strcat(OLstring, "IX.");
	break;
    case 10:
	strcat(OLstring, "X.");
	break;
    default:
	strcat(OLstring, ".");
	break;
    }

    return OLstring;
}

/*
** This function returns OL TYPE="i" strings in
** range of " i." (1) to "mmm." (3000).- FM
*/
PUBLIC char *LYLowercaseI_OL_String ARGS1(
	int, seqnum)
{
    static char OLstring[8];
    int Arabic = seqnum;

    if (Arabic >= 3000) {
	strcpy(OLstring, "mmm.");
	return OLstring;
    }

    switch(Arabic) {
    case 1:
	strcpy(OLstring, " i.");
	return OLstring;
    case 5:
	strcpy(OLstring, " v.");
	return OLstring;
    case 10:
	strcpy(OLstring, " x.");
	return OLstring;
    case 50:
	strcpy(OLstring, " l.");
	return OLstring;
    case 100:
	strcpy(OLstring, " c.");
	return OLstring;
    case 500:
	strcpy(OLstring, " d.");
	return OLstring;
    case 1000:
	strcpy(OLstring, " m.");
	return OLstring;
    default:
	OLstring[0] = '\0';
	break;
    }

    while (Arabic >= 1000) {
	strcat(OLstring, "m");
	Arabic -= 1000;
    }

    if (Arabic >= 900) {
	strcat(OLstring, "cm");
	Arabic -= 900;
    }

    if (Arabic >= 500) {
	strcat(OLstring, "d");
	Arabic -= 500;
	while (Arabic >= 500) {
	    strcat(OLstring, "c");
	    Arabic -= 10;
	}
    }

    if (Arabic >= 400) {
	strcat(OLstring, "cd");
	Arabic -= 400;
    }

    while (Arabic >= 100) {
	strcat(OLstring, "c");
	Arabic -= 100;
    }

    if (Arabic >= 90) {
	strcat(OLstring, "xc");
	Arabic -= 90;
    }

    if (Arabic >= 50) {
	strcat(OLstring, "l");
	Arabic -= 50;
	while (Arabic >= 50) {
	    strcat(OLstring, "x");
	    Arabic -= 10;
	}
    }

    if (Arabic >= 40) {
	strcat(OLstring, "xl");
	Arabic -= 40;
    }

    while (Arabic > 10) {
	strcat(OLstring, "x");
	Arabic -= 10;
    }

    switch (Arabic) {
    case 1:
	strcat(OLstring, "i.");
	break;
    case 2:
	strcat(OLstring, "ii.");
	break;
    case 3:
	strcat(OLstring, "iii.");
	break;
    case 4:
	strcat(OLstring, "iv.");
	break;
    case 5:
	strcat(OLstring, "v.");
	break;
    case 6:
	strcat(OLstring, "vi.");
	break;
    case 7:
	strcat(OLstring, "vii.");
	break;
    case 8:
	strcat(OLstring, "viii.");
	break;
    case 9:
	strcat(OLstring, "ix.");
	break;
    case 10:
	strcat(OLstring, "x.");
	break;
    default:
	strcat(OLstring, ".");
	break;
    }

    return OLstring;
}

/*
**  This function initializes the Ordered List counter. - FM
*/
PUBLIC void LYZero_OL_Counter ARGS1(
	HTStructured *, 	me)
{
    int i;

    if (!me)
	return;

    for (i = 0; i < 12; i++) {
	me->OL_Counter[i] = OL_VOID;
	me->OL_Type[i] = '1';
    }

    me->Last_OL_Count = 0;
    me->Last_OL_Type = '1';

    return;
}

/*
**  This function is used by the HTML Structured object. - KW
*/
PUBLIC void LYGetChartransInfo ARGS1(
	HTStructured *, 	me)
{
    me->UCLYhndl = HTAnchor_getUCLYhndl(me->node_anchor,
					UCT_STAGE_STRUCTURED);
    if (me->UCLYhndl < 0) {
	int chndl = HTAnchor_getUCLYhndl(me->node_anchor, UCT_STAGE_HTEXT);

	if (chndl < 0) {
	    chndl = current_char_set;
	    HTAnchor_setUCInfoStage(me->node_anchor, chndl,
				    UCT_STAGE_HTEXT,
				    UCT_SETBY_STRUCTURED);
	}
	HTAnchor_setUCInfoStage(me->node_anchor, chndl,
				UCT_STAGE_STRUCTURED,
				UCT_SETBY_STRUCTURED);
	me->UCLYhndl = HTAnchor_getUCLYhndl(me->node_anchor,
					    UCT_STAGE_STRUCTURED);
    }
    me->UCI = HTAnchor_getUCInfoStage(me->node_anchor,
				      UCT_STAGE_STRUCTURED);
}

#ifdef NOTUSED_FOTEMODS
/*
**  This function reallocates an allocated string and converts
**  characters for the current display character set.  It assumes
**  that invalid control characters have been dealt with by the
**  SGML (or other initial) parser of the document input stream
**  (i.e., are present only if elements or global flags have been
**  set to allow them), and that otherwise this is a copy of the
**  string with the charset of the input stream.  It handles Lynx
**  special characters based on the 'me' structure's element values
**  (the me->UsePlainSpace and me->HiddenValue elements, and its
**  chartrans related elements), and calls to other functions which
**  return structure element values.  HTChunk functions are used to
**  keep memory allocations at a minimum. - FM
*/
PUBLIC void LYExpandString ARGS2(
	HTStructured *, 	me,
	char **,		str)
{
    char *p = *str;
    HTChunk *s;
    BOOLEAN plain_space, hidden;
    char c;
    unsigned char c_unsign;
    char saved_char_in = '\0';
    BOOLEAN chk;
    UCode_t code, uck;
    char replace_buf [64];
    char utf_buf[8], utf_count = 0;
    char *utf_buf_p = utf_buf;
    UCode_t utf_char = 0, value;
    CONST char *name;
    int i, j, high, low, diff = 0;

    /*
    **	Don't do anything if we have no structure
    **	or string, or are in CJK mode. - FM
    */
    if (!me || !p || *p == '\0' ||
	HTCJK != NOCJK)
	return;

    /*
    **	Set "convenience copies" of me structure
    **	elements. - FM
    */
    plain_space = me->UsePlainSpace;
    hidden = me->HiddenValue;

    /*
    **	Check for special input charsets - FM
    */
    if (!strcmp(me->inUCI->MIMEname, "x-transparent")) {
	/*
	**  Conversions not intended. - FM
	*/
	return;
    }
    if (!strcmp(me->inUCI->MIMEname, "mnemonic") ||
	!strcmp(me->inUCI->MIMEname, "mnemonic+ascii+0")) {
	/*
	**  All ASCII representations of Unicode characters,
	**  and we have no reverse translation code for the
	**  multibyte characters, so punt. - FM
	*/
	return;
    }
    if (me->inUCLYhndl < 0 || me->outUCLYhndl < 0) {
	/*
	**  The chartrans procedure failed, so we don't
	**  do anything, and hope for the best. - FM
	*/
	CTRACE(tfp, "LYExpandString: Bad in (%d) or out (%d) handle(s).\n",
		    me->inUCLYhndl, me->outUCLYhndl);
	return;
    }

    /*
    **	Zero the UTF-8 multibytes buffer. - FM
    */
    utf_buf[0] = utf_buf[6] = utf_buf[7] = '\0';

    /*
    **	Set up an HTChunk for accumulating the expanded copy
    **	of the string, so that allocations are done in 128
    **	byte increments, only as required. - FM
    */
    s = HTChunkCreate(128);

    /*
    **	Check each character in the original string,
    **	and add the characters or substitutions to
    **	our clean copy. - FM
    */
    for (i = 0; p[i]; i++) {
	/*
	**  Make sure the character is handled as Unicode
	**  whenever that's appropriate.  - FM
	*/
	c = p[i];
	c_unsign = (unsigned char)c;
	code = (UCode_t)c_unsign;
	saved_char_in = '\0';
	/*
	**  Combine any UTF-8 multibytes into Unicode
	**  to check for special characters. - FM
	*/
	if (me->T.decode_utf8) {
	    /*
	    **	Our input charset is UTF-8, so check
	    **	for non-ASCII characters. - FM
	    */
	    if (TOASCII(c_unsign) > 127) {  /* S/390 -- gil -- 1703 */
		/*
		**  We have an octet from a multibyte character. - FM
		*/
		if (utf_count > 0 && (c & 0xc0) == 0x80) {
		    /*
		    **	Adjust the UCode_t value, add the octet
		    **	to the buffer, and decrement the byte
		    **	count. - FM
		    */
		    utf_char = (utf_char << 6) | (c & 0x3f);
		    utf_count--;
		    *utf_buf_p = c;
		    utf_buf_p++;
		    if (utf_count == 0) {
			/*
			**  We have all of the bytes, so terminate
			**  the buffer and set 'code' to the UCode_t
			**  value. - FM
			*/
			*utf_buf_p = '\0';
			code = utf_char;
			/*
			**  Set up the monobyte character
			**  values or non-character flags
			**  and fall through. - FM
			*/
			if (code > 0 && code < 256) {
			    c = ((char)(code & 0xff));
			    c_unsign = (unsigned char)c;
			}
		    } else {
			/*
			**  Get the next byte. - FM
			*/
			continue;
		    }
		} else {
		    /*
		    **	Start handling a new multibyte character. - FM
		    */
		    utf_buf[0] = c;
		    utf_buf_p = &utf_buf[1];
		    if ((c & 0xe0) == 0xc0) {
			utf_count = 1;
			utf_char = (c & 0x1f);
		    } else if ((c & 0xf0) == 0xe0) {
			utf_count = 2;
			utf_char = (c & 0x0f);
		    } else if ((c & 0xf8) == 0xf0) {
			utf_count = 3;
			utf_char = (c & 0x07);
		    } else if ((c & 0xfc) == 0xf8) {
			utf_count = 4;
			utf_char = (c & 0x03);
		    } else if ((c & 0xfe) == 0xfc) {
			utf_count = 5;
			utf_char = (c & 0x01);
		    } else {
			/*
			**  We got garbage, even though it should
			**  have been filtered out by the SGML or
			**  input stream parser, so we'll ignore
			**  it. - FM
			*/
			utf_count = 0;
			utf_buf[0] = '\0';
			utf_buf_p = utf_buf;
		    }
		    /*
		    **	Get the next byte. - FM
		    */
		    continue;
		}
	    } else if (utf_count > 0) {
		/*
		**  Got an ASCII character when expecting
		**  UTF-8 multibytes, so ignore the buffered
		**  multibyte characters and fall through with
		**  the current ASCII character. - FM
		*/
		utf_count = 0;
		utf_buf[0] = '\0';
		utf_buf_p = utf_buf;
		code = (UCode_t)c_unsign;
	    } else {
		/*
		**  Got a valid ASCII character, so fall
		**  through with it. - FM
		*/
		code = (UCode_t)c_unsign;
	    }
	}
	/*
	**  Convert characters from non-UTF-8 charsets
	**  to Unicode (if appropriate). - FM
	*/
	if (!(me->T.decode_utf8 &&
	      (unsigned char)p[i] > 127)) {
#ifdef NOTDEFINED
	    if (me->T.strip_raw_char_in)
		saved_char_in = c;
#endif /* NOTDEFINED */
	    if (me->T.trans_to_uni &&
		(code >= LYlowest_eightbit[me->inUCLYhndl] ||
		 (code < 32 && code != 0 &&
		  me->T.trans_C0_to_uni))) {
		/*
		**  Convert the octet to Unicode. - FM
		*/
		code = (UCode_t)UCTransToUni(c, me->inUCLYhndl);
		if (code > 0) {
		    saved_char_in = c;
		    if (code < 256) {
			c = ((char)(code & 0xff));
			c_unsign = (unsigned char)c;
		    }
		}
	    } else if (code < ' ' && code != 0 &&  /* S/390 -- gil -- 1720 */
		       me->T.trans_C0_to_uni) {
		/*
		**  Quote from SGML.c:
		**	"This else if may be too ugly to keep. - KW"
		*/
		if (me->T.trans_from_uni &&
		    (((code = UCTransToUni(c, me->inUCLYhndl)) >= ' ') ||  /* S/390 -- gil -- 1737 */
		     (me->T.transp &&
		      (code = UCTransToUni(c, me->inUCLYhndl)) > 0))) {
		    saved_char_in = c;
		    if (code < 256) {
			c = ((char)(code & 0xff));
			c_unsign = (unsigned char)c;
		    }
		} else {
		    uck = -1;
		    if (me->T.transp) {
			uck = UCTransCharStr(replace_buf, 60, c,
					     me->inUCLYhndl,
					     me->inUCLYhndl, NO);
		    }
		    if (!me->T.transp || uck < 0) {
			uck = UCTransCharStr(replace_buf, 60, c,
					     me->inUCLYhndl,
					     me->outUCLYhndl, YES);
		    }
		    if (uck == 0) {
			continue;
		    } else if (uck < 0) {
			utf_buf[0] = '\0';
			code = (unsigned char)c;
		    } else {
			c = replace_buf[0];
			if (c && replace_buf[1]) {
			    HTChunkPuts(s, replace_buf);
			    continue;
			}
		    }
		    utf_buf[0] = '\0';
		    code = (unsigned char)c;
		} /*  Next line end of ugly stuff for C0. - KW */
	    } else {
		utf_buf[0] = '\0';
		code = (unsigned char)c;
	    }
	}
	/*
	**  Ignore low ISO 646 7-bit control characters
	**  if they sneaked through (should have been
	**  filtered by the parser). - FM
	*/
	if (code < ' ' &&  /* S/390 -- gil -- 1754 */
	    c != 9 && c != 10 && c != 13) {
	    continue;
	}
	/*
	**  Ignore 127 if we don't have HTPassHighCtrlRaw
	**  and it sneaked through (should have been
	**  filtered by the parser). - FM
	*/
	if (TOASCII(c) == 127 &&  /* S/390 -- gil -- 1771 */
	    !(me->T.transp ||
	      code >= LYlowest_eightbit[me->inUCLYhndl])) {
	    continue;
	}
	/*
	**  Ignore 8-bit control characters 128 - 159 if we don't
	**  have HTPassHighCtrlRaw set and they sneaked through
	**  (should have been filtered by the parser). - FM
	*/
	if (TOASCII(code) > 127 && TOASCII(code) < 160 &&  /* S/390 -- gil -- 1788 */
	    !(me->T.transp ||
	      code >= LYlowest_eightbit[me->inUCLYhndl])) {
	    continue;
	}
	/*
	**  For 160 (nbsp), substitute Lynx special character
	**  (or a space if plain_space or hidden is set) if
	**  HTPassHighCtrlRaw is not set. - FM
	*/
	if (code == CH_NBSP) {  /* S/390 -- gil -- 1805 */
	    if (!me->T.pass_160_173_raw) {
		if (plain_space || hidden) {
		    HTChunkPutc(s, ' ');
		} else {
		    HTChunkPutc(s, HT_NON_BREAK_SPACE);
		}
	    } else if (!me->T.output_utf8) {
		HTChunkPutc(s, ((char)(code & 0xff)));
	    } else if (me->T.decode_utf8 && *utf_buf) {
		HTChunkPuts(s, utf_buf);
		utf_buf[0] == '\0';
		utf_buf_p = utf_buf;
	    } else {
		HTChunkPutUtf8Char(s, code);
	    }
	    continue;
	}
	/*
	**  For 173 (shy), substitute Lynx special character
	**  (or skip it if plain_space or hidden is set) if
	**  HTPassHighCtrlRaw is not set. - FM
	*/
	if (code == CH_SHY) {  /* S/390 -- gil -- 1822 */
	    if (!me->T.pass_160_173_raw) {
		if (!(plain_space || hidden)) {
		    HTChunkPutc(s, LY_SOFT_HYPHEN);
		}
	    } else if (!me->T.output_utf8) {
		HTChunkPutc(s, ((char)(code & 0xff)));
	    } else if (me->T.decode_utf8 && *utf_buf) {
		HTChunkPuts(s, utf_buf);
		utf_buf[0] == '\0';
		utf_buf_p = utf_buf;
	    } else {
		HTChunkPutUtf8Char(s, code);
	    }
	    continue;
	}
	/*
	**  For 8194 (ensp), 8195 (emsp), or 8201 (thinsp), use
	**  an ASCII space (32) if plain_space or hidden is TRUE,
	**  otherwise use the Lynx special character. - FM
	*/
	if (code == 8194 || code == 8195 || code == 8201) {
	    if (plain_space || hidden) {
		HTChunkPutc(s, ' ');
	    } else {
		HTChunkPutc(s, HT_EN_SPACE);
	    }
	    if (me->T.decode_utf8 && *utf_buf) {
		utf_buf[0] == '\0';
		utf_buf_p = utf_buf;
	    }
	    continue;
	}
	/*
	**  If we want the raw character, pass it now. - FM
	*/
	if (me->T.use_raw_char_in && saved_char_in) {
	    HTChunkPutc(s, saved_char_in);
	    continue;
	}
	/*
	**  Seek a translation from the chartrans tables.
	*/
	if ((chk = (me->T.trans_from_uni && code >= 160)) &&
	    (uck = UCTransUniChar(code, me->outUCLYhndl)) >= 32 &&
	    uck < 256 &&
	    (uck < 127 ||
	     uck >= LYlowest_eightbit[me->outUCLYhndl])) {
	    HTChunkPutc(s, ((char)(uck & 0xff)));
	    continue;
	} else if (chk &&
		   (uck == -4 ||
		    (me->T.repl_translated_C0 &&
		     uck > 0 && uck < ' ')) &&  /* S/390 -- gil -- 1839 */
		   /*
		   **  Not found; look for replacement string.
		   */
		   (uck = UCTransUniCharStr(replace_buf,
					    60, code,
					    me->outUCLYhndl,
					    0) >= 0)) {
	    /*
	    **	Got a replacement string.
	    */
	    HTChunkPuts(s, replace_buf);
	    continue;
	}
	/*
	**  If we want raw UTF-8, output that now. - FM
	*/
	if (me->T.output_utf8 &&
	    TOASCII(code) > 127 && code <= 0x7fffffffL) {  /* S/390 -- gil -- 1856 */
	    if (me->T.decode_utf8 && *utf_buf) {
		HTChunkPuts(s, utf_buf);
		utf_buf[0] == '\0';
		utf_buf_p = utf_buf;
	    } else {
		HTChunkPutUtf8Char(s, code);
	    }
	    continue;
	}
	/*
	**  If it's any other (> 160) 8-bit character
	**  and we have not set HTPassEightBitRaw
	**  nor have the "ISO Latin 1" character set selected,
	**  back translate for our character set. - FM
	*/
	if (code > 160 && code < 256 &&
	     me->outUCLYhndl != LATIN1 &&
	     (!(HTPassEightBitRaw ||
		(me->T.do_8bitraw && !me->T.trans_from_uni)))) {
	    value = (code - 160);
	    name = HTMLGetEntityName(value);
	    for (low = 0, high = HTML_dtd.number_of_entities;
		 high > low;
		 diff < 0 ? (low = j+1) : (high = j)) {
		/*
		**  Binary search.
		*/
		j = (low + (high-low)/2);
		diff = strcmp(HTML_dtd.entity_names[j], name);
		if (diff == 0) {
		    HTChunkPuts(s, LYCharSets[me->outUCLYhndl][j]);
		    break;
		}
	    }
	    if (diff == 0) {
		continue;
	    }
	}
	/*
	**  If it's ASCII at this point, use it. - FM
	*/
	if (TOASCII(code) < 127 && code > 0) {  /* S/390 -- gil -- 1873 */
	    HTChunkPutc(s, ((char)(code & 0xff)));
	    continue;
	}
	/*
	**  At this point, if we should have translated, the
	**  translation has failed.  We should have sent UTF-8
	**  output to the parser already, but what the heck,
	**  try again. - FM
	*/
	if (me->T.output_utf8 && *utf_buf) {
	    HTChunkPuts(s, utf_buf);
	    utf_buf[0] == '\0';
	    utf_buf_p = utf_buf;
	    continue;
	}
#ifdef NOTDEFINED
	/*
	**  Check for a strippable koi8-r 8-bit character. - FM
	*/
	if (me->T.strip_raw_char_in &&
	    (unsigned char)saved_char_in >= 192 &&
	    (unsigned char)saved_char_in < 255 &&
	    saved_char_in) {
	    /*
	    **	KOI8 special: strip high bit, gives (somewhat) readable
	    **	ASCII or KOI7 - it was constructed that way! - KW
	    */
	    HTChunkPutc(s, (saved_char_in & 0x7f));
	    continue;
	}
#endif /* NOTDEFINED */
	/*
	**  Ignore 8204 (zwnj), 8205 (zwj)
	**  8206 (lrm), and 8207 (rlm),
	**  if we get to here. - FM
	*/
	if (code == 8204 || code == 8205 ||
	    code == 8206 || code == 8207) {
	    CTRACE(tfp, "LYExpandString: Ignoring '%ld'.\n", code);
	    if (me->T.decode_utf8 && *utf_buf) {
		utf_buf[0] == '\0';
		utf_buf_p = utf_buf;
	    }
	    continue;
	}
	/*
	**  If we don't actually want the character,
	**  make it safe and output that now. - FM
	*/
	if ((c_unsign > 0 &&
	     c_unsign < LYlowest_eightbit[me->outUCLYhndl]) ||
	    (me->T.trans_from_uni && !HTPassEightBitRaw)) {
	    /*
	    **	If we do not have the "7-bit approximations" as our
	    **	output character set (in which case we did it already)
	    **	seek a translation for that.  Otherwise, or if the
	    **	translation fails, use UHHH notation. - FM
	    */
	    if ((chk = (me->outUCLYhndl !=
			UCGetLYhndl_byMIME("us-ascii"))) &&
		(uck = UCTransUniChar(code,
				      UCGetLYhndl_byMIME("us-ascii")))
				      >= ' ' && TOASCII(uck) < 127) {  /* S/390 -- gil -- 1890 */
		/*
		**  Got an ASCII character (yippey). - FM
		*/
		c = ((char)(uck & 0xff));
		HTChunkPutc(s, c);
		continue;
	    } else if ((uck == -4) &&
		       (uck = UCTransUniCharStr(replace_buf,
						60, code,
						UCGetLYhndl_byMIME("us-ascii"),
						0) >= 0)) {
		/*
		**  Got a replacement string (yippey). - FM
		*/
		HTChunkPuts(s, replace_buf);
		continue;
	    } else {
		/*
		**  Out of luck, so use the UHHH notation (ugh). - FM
		*/
		sprintf(replace_buf, "U%.2lX", TOASCII(code));  /* S/390 -- gil -- 1907 */
		HTChunkPuts(s, replace_buf);
		continue;
	    }
	}
	/*
	**  If we get to here and have a monobyte character,
	**  pass it. - FM
	*/
	if (c_unsign > 0 && c_unsign < 256) {
	    HTChunkPutc(s, c);
	}
    }

    /*
    **	Terminate the expanded string,
    **	replace the original, and free
    **	the chunk. - FM
    */
    HTChunkTerminate(s);
    StrAllocCopy(*str, s->data);
    HTChunkFree(s);
}
#endif /* NOTUSED_FOTEMODS */

/*
** Get UCS character code for one character from UTF-8 encoded string.
**
** On entry:
**	*ppuni should point to beginning of UTF-8 encoding character
** On exit:
**	*ppuni is advanced to point to the last byte of UTF-8 sequence,
**		if there was a valid one; otherwise unchanged.
** returns the UCS value
** returns negative value on error (invalid UTF-8 sequence)
*/
PRIVATE UCode_t UCGetUniFromUtf8String ARGS1(char **, ppuni)
{
    UCode_t uc_out = 0;
    char * p = *ppuni;
    int utf_count, i;
    if (!(**ppuni&0x80))
	return (UCode_t) **ppuni; /* ASCII range character */
    else if (!(**ppuni&0x40))
	return (-1);		/* not a valid UTF-8 start */
    if ((*p & 0xe0) == 0xc0) {
	utf_count = 1;
    } else if ((*p & 0xf0) == 0xe0) {
	utf_count = 2;
    } else if ((*p & 0xf8) == 0xf0) {
	utf_count = 3;
    } else if ((*p & 0xfc) == 0xf8) {
	utf_count = 4;
    } else if ((*p & 0xfe) == 0xfc) {
	utf_count = 5;
    } else { /* garbage */
	return (-1);
    }
    for (p = *ppuni, i = 0; i < utf_count ; i++) {
	if ((*(++p) & 0xc0) != 0x80)
	    return (-1);
    }
    p = *ppuni;
    switch (utf_count) {
    case 1:
	uc_out = (((*p&0x1f) << 6) | (*(p+1)&0x3f));
	break;
    case 2:
	uc_out = (((((*p&0x0f) << 6) | (*(p+1)&0x3f)) << 6) | (*(p+2)&0x3f));
	break;
    case 3:
	uc_out = (((((((*p&0x07) << 6) | (*(p+1)&0x3f)) << 6) | (*(p+2)&0x3f)) << 6)
	    | (*(p+3)&0x3f));
	break;
    case 4:
	uc_out = (((((((((*p&0x03) << 6) | (*(p+1)&0x3f)) << 6) | (*(p+2)&0x3f)) << 6)
		  | (*(p+3)&0x3f)) << 6) | (*(p+4)&0x3f));
	break;
    case 5:
	uc_out = (((((((((((*p&0x01) << 6) | (*(p+1)&0x3f)) << 6) | (*(p+2)&0x3f)) << 6)
		  | (*(p+3)&0x3f)) << 6) | (*(p+4)&0x3f)) << 6) | (*(p+5)&0x3f));
	break;
    }
    *ppuni = p + utf_count;
    return uc_out;
}

/*
 *  Given an UCS character code, will fill buffer passed in as q with
 *  the code's UTF-8 encoding.
 *  If terminate = YES, terminates string on success and returns pointer
 *			to beginning.
 *  If terminate = NO,	does not terminate string, and returns pointer
 *			next char after the UTF-8 put into buffer.
 *  On failure, including invalid code or 7-bit code, returns NULL.
 */
PRIVATE char * UCPutUtf8ToBuffer ARGS3(char *, q, UCode_t, code, BOOL, terminate)
{
    char *q_in = q;
    if (!q)
    return NULL;
    if (code > 127 && code < 0x7fffffffL) {
	if (code < 0x800L) {
	    *q++ = (char)(0xc0 | (code>>6));
	    *q++ = (char)(0x80 | (0x3f & (code)));
	} else if (code < 0x10000L) {
	    *q++ = (char)(0xe0 | (code>>12));
	    *q++ = (char)(0x80 | (0x3f & (code>>6)));
	    *q++ = (char)(0x80 | (0x3f & (code)));
	} else if (code < 0x200000L) {
	    *q++ = (char)(0xf0 | (code>>18));
	    *q++ = (char)(0x80 | (0x3f & (code>>12)));
	    *q++ = (char)(0x80 | (0x3f & (code>>6)));
	    *q++ = (char)(0x80 | (0x3f & (code)));
	} else if (code < 0x4000000L) {
	    *q++ = (char)(0xf8 | (code>>24));
	    *q++ = (char)(0x80 | (0x3f & (code>>18)));
	    *q++ = (char)(0x80 | (0x3f & (code>>12)));
	    *q++ = (char)(0x80 | (0x3f & (code>>6)));
	    *q++ = (char)(0x80 | (0x3f & (code)));
	} else {
	    *q++ = (char)(0xfc | (code>>30));
	    *q++ = (char)(0x80 | (0x3f & (code>>24)));
	    *q++ = (char)(0x80 | (0x3f & (code>>18)));
	    *q++ = (char)(0x80 | (0x3f & (code>>12)));
	    *q++ = (char)(0x80 | (0x3f & (code>>6)));
	    *q++ = (char)(0x80 | (0x3f & (code)));
	}
    } else {
	return NULL;
    }
    if (terminate) {
	*q = '\0';
	return q_in;
    } else {
	return q;
    }
}

	/* as in HTParse.c, saves some calls - kw */
PRIVATE CONST char *hex = "0123456789ABCDEF";

/*
 *	  Any raw 8-bit or multibyte characters already have been
 *	  handled in relation to the display character set
 *	  in SGML_character(), including named and numeric entities.
 *
**  This function used for translations HTML special fields inside tags
**  (ALT=, VALUE=, etc.) from charset `cs_from' to charset `cs_to'.
**  It also unescapes non-ASCII characters from URL (#fragments !)
**  if st_URL is active.
**
**  If `do_ent' is YES, it converts named entities
**  and numeric character references (NCRs) to their `cs_to' replacements.
**
**  Named entities converted to unicodes.  NCRs (unicodes) converted
**  by UCdomap.c chartrans functions.
**  ???NCRs with values in the ISO-8859-1 range 160-255 may be converted
**  to their HTML entity names (via old-style entities) and then translated
**  according to the LYCharSets.c array for `cs_out'???.
**
**  Some characters (see descriptions in `put_special_unicodes' from SGML.c)
**  translated in relation with the state of boolean variables
**  `use_lynx_specials', `plain_space' and `hidden'.  It is not clear yet:
**
**  If plain_space is TRUE, nbsp (160) will be treated as an ASCII
**  space (32).  If hidden is TRUE, entities will be translated
**  (if `do_ent' is YES) but escape sequences will be passed unaltered.
**  If `hidden' is FALSE, some characters are converted to Lynx special
**  codes (see `put_special_unicodes') or ASCII space if `plain_space'
**  applies).  @@ is `use_lynx_specials' needed, does it have any effect? @@
**  If `use_lynx_specials' is YES, translate byte values 160 and 173
**  meaning U+00A0 and U+00AD given as or converted from raw char input
**  are converted to HT_NON_BREAK_SPACE and LY_SOFT_HYPHEN, respectively
**  (unless input and output charset are both iso-8859-1, for compatibility
**  with previous usage in HTML.c) even if `hidden' or `plain_space' is set.
**
**  If `Back' is YES, the reverse is done instead i.e., Lynx special codes
**  in the input are translated back to character values.
**
**  If `Back' is YES, an attempt is made to use UCReverseTransChar() for
**  back translation which may be more efficient. (?)
**
**  If `stype' is st_URL, non-ASCII characters are URL-encoded instead.
**  The sequence of bytes being URL-encoded is the raw input character if
**  we couldn't translate it from `cs_in' (CJK etc.); otherwise it is the
**  UTF-8 representation if either `cs_to' requires this or if the
**  character's Unicode value is > 255, otherwise it should be the iso-8859-1
**  representation.
**  No general URL-encoding occurs for displayable ASCII characters and
**  spaces and some C0 controls valid in HTML (LF, TAB), it is expected
**  that other functions will take care of that as appropriate.
**
**  Escape characters (0x1B, '\033') are
**  - URL-encoded	if `stype'  is st_URL,	 otherwise
**  - dropped		if `stype'  is st_other, otherwise (i.e., st_HTML)
**  - passed		if `hidden' is TRUE or HTCJK is set, otherwise
**  - dropped.
**
**  (If `stype' is st_URL or st_other most of the parameters really predefined:
**  cs_from=cs_to, use_lynx_specials=plain_space=NO, and hidden=YES)
**
**
**  Returns pointer to the char** passed in
**		 if string translated or translation unnecessary,
**	    NULL otherwise
**		 (in which case something probably went wrong.)
**
**
**  In general, this somehow ugly function (KW)
**  cover three functions from v.2.7.2 (FM):
**		    extern void LYExpandString PARAMS((
**		       HTStructured *	       me,
**		       char **		       str));
**		    extern void LYUnEscapeEntities PARAMS((
**		       HTStructured *	       me,
**		       char **		       str));
**		    extern void LYUnEscapeToLatinOne PARAMS((
**		       HTStructured *	       me,
**		       char **		       str,
**		       BOOLEAN		       isURL));
*/

PRIVATE char ** LYUCFullyTranslateString_1 ARGS9(
	char **,	str,
	int,		cs_from,
	int,		cs_to,
	BOOLEAN,	do_ent,
	BOOL,		use_lynx_specials,
	BOOLEAN,	plain_space,
	BOOLEAN,	hidden,
	BOOL,		Back,
	CharUtil_st,	stype)
{
    char * p;
    char *q, *qs;
    HTChunk *chunk = NULL;
    char * cp = 0;
    char cpe = 0;
    char *esc = NULL;
    char replace_buf [64];
    int uck;
    int lowest_8;
    UCode_t code = 0;
    long int lcode;
    BOOL output_utf8 = 0, repl_translated_C0 = 0;
    size_t len;
    CONST char * name = NULL;
    BOOLEAN no_bytetrans;
    UCTransParams T;
    BOOL from_is_utf8 = FALSE;
    char * puni;
    enum _state
	{ S_text, S_esc, S_dollar, S_paren, S_nonascii_text, S_dollar_paren,
	S_trans_byte, S_check_ent, S_ncr, S_check_uni, S_named, S_check_name,
	S_recover,
	S_got_oututf8, S_got_outstring, S_put_urlstring,
	S_got_outchar, S_put_urlchar, S_next_char, S_done} state = S_text;
    enum _parsing_what
	{ P_text, P_utf8, P_hex, P_decimal, P_named
	} what = P_text;

    /*
    **	Make sure we have a non-empty string. - FM
    */
    if (!str || *str == NULL || **str == '\0')
	return str;

    /*
     * FIXME: something's wrong with the limit checks here (clearing the
     * buffer helps).
     */
    memset(replace_buf, 0, sizeof(replace_buf));

    /*
    **	Don't do byte translation
    **	if original AND target character sets
    **	are both iso-8859-1,
    **	or if we are in CJK mode.
    */
    no_bytetrans = ((cs_to <= 0 && cs_from == cs_to) ||
		    HTCJK != NOCJK);

    /* No need to translate or examine the string any further */
    if (!no_bytetrans)
	no_bytetrans = (!use_lynx_specials && !Back &&
			UCNeedNotTranslate(cs_from, cs_to));

    /*
    **	Save malloc/calloc overhead in simple case - kw
    */
    if (do_ent && hidden && (stype != st_URL) && (strchr(*str, '&') == NULL))
	do_ent = FALSE;

    /* Can't do, caller should figure out what to do... */
    if (!UCCanTranslateFromTo(cs_from, cs_to)) {
	if (cs_to < 0)
	    return NULL;
	if (!do_ent && no_bytetrans)
	    return NULL;
	no_bytetrans = TRUE;
    } else if (cs_to < 0) {
	do_ent = FALSE;
    }

    if (!do_ent && no_bytetrans)
	return str;
    p = *str;

    if (!no_bytetrans) {
	UCTransParams_clear(&T);
	UCSetTransParams(&T, cs_from, &LYCharSet_UC[cs_from],
			 cs_to, &LYCharSet_UC[cs_to]);
	from_is_utf8 = (LYCharSet_UC[cs_from].enc == UCT_ENC_UTF8);
	output_utf8 = T.output_utf8;
	repl_translated_C0 = T.repl_translated_C0;
	puni = p;
    } else if (do_ent) {
	output_utf8 = (LYCharSet_UC[cs_to].enc == UCT_ENC_UTF8 ||
		       HText_hasUTF8OutputSet(HTMainText));
	repl_translated_C0 = (LYCharSet_UC[cs_to].enc == UCT_ENC_8BIT_C0);
    }

    lowest_8 = LYlowest_eightbit[cs_to];

    /*
    **	Create a buffer string seven times the length of the original,
    **	so we have plenty of room for expansions. - FM
    */
    len = strlen(p) + 16;
    q = p;

    qs = q;

/*  Create the HTChunk only if we need it */
#define CHUNK (chunk ? chunk : (chunk = HTChunkCreate2(128, len+1)))

#define REPLACE_STRING(s) \
		if (q != qs) HTChunkPutb(CHUNK, qs, q-qs); \
		HTChunkPuts(CHUNK, s); \
		qs = q = *str

#define REPLACE_CHAR(c) if (q > p) { \
		HTChunkPutb(CHUNK, qs, q-qs); \
		qs = q = *str; \
		*q++ = c; \
	    } else \
		*q++ = c

    /*
    *  Loop through string, making conversions as needed.
    *
    *  The while() checks for a non-'\0' char only for the normal
    *  text states since other states may temporarily modify p or *p
    *  (which should be restored before S_done!) - kw
    */

    while (*p || (state != S_text && state != S_nonascii_text)) {
	switch(state) {
	case S_text:
	    code = (unsigned char)(*p);
	    if (*p == '\033') {
		if ((HTCJK != NOCJK && !hidden) || stype != st_HTML) {
		    state = S_esc;
		    if (stype == st_URL) {
			REPLACE_STRING("%1B");
			p++;
			continue;
		    } else if (stype != st_HTML) {
			p++;
			continue;
		    } else {
			*q++ = *p++;
			continue;
		    }
		} else if (!hidden) {
		    /*
		    **	CJK handling not on, and not a hidden INPUT,
		    **	so block escape. - FM
		    */
		    state = S_next_char;
		} else {
		    state = S_trans_byte;
		}
	    } else {
		state = (do_ent ? S_check_ent : S_trans_byte);
	    }
	    break;

	case S_esc:
	    if (*p == '$') {
		state = S_dollar;
		*q++ = *p++;
		continue;
	    } else if (*p == '(') {
		state = S_paren;
		*q++ = *p++;
		continue;
	    } else {
		state = S_text;
	    }

	case S_dollar:
	    if (*p == '@' || *p == 'B' || *p == 'A') {
		state = S_nonascii_text;
		*q++ = *p++;
		continue;
	    } else if (*p == '(') {
		state = S_dollar_paren;
		*q++ = *p++;
		continue;
	    } else {
		state = S_text;
	    }
	    break;

	case S_dollar_paren:
	    if (*p == 'C') {
		state = S_nonascii_text;
		*q++ = *p++;
		continue;
	    } else {
		state = S_text;
	    }
	    break;

	case S_paren:
	    if (*p == 'B' || *p == 'J' || *p == 'T')  {
		state = S_text;
		*q++ = *p++;
		continue;
	    } else if (*p == 'I') {
		state = S_nonascii_text;
		*q++ = *p++;
		continue;
	    } else {
		state = S_text;
	    }
	    break;

	case S_nonascii_text:
	    if (*p == '\033') {
		if ((HTCJK != NOCJK && !hidden) || stype != st_HTML) {
		    state = S_esc;
		    if (stype == st_URL) {
			REPLACE_STRING("%1B");
			p++;
			continue;
		    } else if (stype != st_HTML) {
			p++;
			continue;
		    }
		}
	    }
	    *q++ = *p++;
	    continue;

	case S_trans_byte:
	    /*	character translation goes here  */
	    /*
	    **	Don't do anything if we have no string,
	    **	or if original AND target character sets
	    **	are both iso-8859-1,
	    **	or if we are in CJK mode.
	    */
	    if (*p == '\0' || no_bytetrans) {
		state = S_got_outchar;
		break;
	    }

	    if (Back) {
		int rev_c;
		if ((*p) == HT_NON_BREAK_SPACE ||
		    (*p) == HT_EN_SPACE) {
		    if (plain_space) {
			code = *p = ' ';
			state = S_got_outchar;
			break;
		    } else {
			*p = 160;
			code = 160;
			if (LYCharSet_UC[cs_to].enc == UCT_ENC_8859 ||
			    (LYCharSet_UC[cs_to].like8859 & UCT_R_8859SPECL)) {
			    state = S_got_outchar;
			    break;
			}
		    }
		} else if ((*p) == LY_SOFT_HYPHEN) {
		    *p = 173;
		    code = 173;
		    if (LYCharSet_UC[cs_to].enc == UCT_ENC_8859 ||
			(LYCharSet_UC[cs_to].like8859 & UCT_R_8859SPECL)) {
			state = S_got_outchar;
			break;
		    }
		} else if (code < 127 || T.transp) {
		    state = S_got_outchar;
		    break;
		}
		rev_c = UCReverseTransChar(*p, cs_to, cs_from);
		if (rev_c > 127) {
		    *p = rev_c;
		    code = rev_c;
		    state = S_got_outchar;
		    break;
		}
	    } else if (code < 127) {
		state = S_got_outchar;
		break;
	    }

	    if (from_is_utf8) {
		if (((*p)&0xc0)==0xc0) {
		    puni = p;
		    code = UCGetUniFromUtf8String(&puni);
		    if (code <= 0) {
			code = (unsigned char)(*p);
		    } else {
			what = P_utf8;
		    }
		}
	    } else if (use_lynx_specials && !Back &&
		       (code == 160 || code == 173) &&
		       (LYCharSet_UC[cs_from].enc == UCT_ENC_8859 ||
			(LYCharSet_UC[cs_from].like8859 & UCT_R_8859SPECL))) {
		if (code == 160)
		    code = *p = HT_NON_BREAK_SPACE;
		else if (code == 173)
		    code = *p = LY_SOFT_HYPHEN;
		state = S_got_outchar;
		break;
	    } else if (T.trans_to_uni) {
		code = UCTransToUni(*p, cs_from);
		if (code <= 0) {
		    /* What else can we do? */
		    code = (unsigned char)(*p);
		}
#ifdef NOTUSED_FOTEMODS
	    } else if (T.strip_raw_char_in &&
		       (unsigned char)(*p) >= 0xc0 &&
		       (unsigned char)(*p) < 255) {
		code = ((*p & 0x7f));
		state = S_got_outchar;
		break;
#endif /* NOTUSED_FOTEMODS */
	    } else if (!T.trans_from_uni) {
		state = S_got_outchar;
		break;
	    }
	    /*
		    **	Substitute Lynx special character for
		    **	160 (nbsp) if use_lynx_specials is set.
		    */
	    if (use_lynx_specials && !Back &&
		(code == 160 || code == 173)) {
		code = ((code==160 ? HT_NON_BREAK_SPACE : LY_SOFT_HYPHEN));
		state = S_got_outchar;
		break;
	    }

	    state = S_check_uni;
	    break;

	case S_check_ent:
	    if (*p == '&') {
		char * pp = p + 1;
		len = strlen(pp);
		/*
		**  Check for a numeric entity. - FM
		*/
		if (*pp == '#' && len > 2 &&
		    (*(pp+1) == 'x' || *(pp+1) == 'X') &&
		    (unsigned char)*(pp+2) < 127 &&
		    isxdigit((unsigned char)*(pp+2))) {
		    what = P_hex;
		    state = S_ncr;
		} else if (*pp == '#' && len > 2 &&
			   (unsigned char)*(pp+1) < 127 &&
			   isdigit((unsigned char)*(pp+1))) {
		    what = P_decimal;
		    state = S_ncr;
		} else if ((unsigned char)*pp < 127 &&
			   isalpha((unsigned char)*pp)) {
		    what = P_named;
		    state = S_named;
		} else {
		    state = S_trans_byte;
		}
	    } else {
		state = S_trans_byte;
	    }
	    break;

	case S_ncr:
		if (what == P_hex) {
		    p += 3;
		} else {	/* P_decimal */
		    p += 2;
		}
		cp = p;
		while (*p && (unsigned char)*p < 127 &&
		       (what == P_hex ? isxdigit((unsigned char)*p) :
					isdigit((unsigned char)*p))) {
		    p++;
		}
		/*
		**  Save the terminator and isolate the digit(s). - FM
		*/
		cpe = *p;
		if (*p)
		    *p++ = '\0';
		/*
		** Show the numeric entity if the value:
		**  (1) Is greater than 255 and unhandled Unicode.
		**  (2) Is less than 32, and not valid and we don't
		**	have HTCJK set.
		**  (3) Is 127 and we don't have HTPassHighCtrlRaw
		**	or HTCJK set.
		**  (4) Is 128 - 159 and we don't have HTPassHighCtrlNum set.
		*/
		if ((((what == P_hex) ? sscanf(cp, "%lx", &lcode) :
					sscanf(cp, "%ld", &lcode)) != 1) ||
		    lcode > 0x7fffffffL || lcode < 0) {
		    state = S_recover;
		    break;
		} else {
		    code = lcode;
		    if ((code == 1) ||
		       (code > 127 && code < 156)) {
			/*
			** Assume these are Microsoft code points, inflicted on
			** us by FrontPage.  - FM
			**
			** MS FrontPage uses syntax like &#153; in 128-159
			** range and doesn't follow Unicode standards for this
			** area.  Windows-1252 codepoints are assumed here.
			*/
			switch (code) {
			case 1:
			    /*
			    **	WHITE SMILING FACE
			    */
			    code = 0x263a;
			    break;
			case 128:
			    /*
			    **	EURO currency sign
			    */
			    code = 0x20ac;
			    break;
			case 130:
			    /*
			    **	SINGLE LOW-9 QUOTATION MARK (sbquo)
			    */
			    code = 0x201a;
			    break;
			case 132:
			    /*
			    **	DOUBLE LOW-9 QUOTATION MARK (bdquo)
			    */
			    code = 0x201e;
			    break;
			case 133:
			    /*
			    **	HORIZONTAL ELLIPSIS (hellip)
			    */
			    code = 0x2026;
			    break;
			case 134:
			    /*
			    **	DAGGER (dagger)
			    */
			    code = 0x2020;
			    break;
			case 135:
			    /*
			    **	DOUBLE DAGGER (Dagger)
			    */
			    code = 0x2021;
			    break;
			case 137:
			    /*
			    **	PER MILLE SIGN (permil)
			    */
			    code = 0x2030;
			    break;
			case 139:
			    /*
			    **	SINGLE LEFT-POINTING ANGLE QUOTATION MARK
			    **	(lsaquo)
			    */
			    code = 0x2039;
			    break;
			case 145:
			    /*
			    **	LEFT SINGLE QUOTATION MARK (lsquo)
			    */
			    code = 0x2018;
			    break;
			case 146:
			    /*
			    **	RIGHT SINGLE QUOTATION MARK (rsquo)
			    */
			    code = 0x2019;
			    break;
			case 147:
			    /*
			    **	LEFT DOUBLE QUOTATION MARK (ldquo)
			    */
			    code = 0x201c;
			    break;
			case 148:
			    /*
			    **	RIGHT DOUBLE QUOTATION MARK (rdquo)
			    */
			    code = 0x201d;
			    break;
			case 149:
			    /*
			    **	BULLET (bull)
			    */
			    code = 0x2022;
			    break;
			case 150:
			    /*
			    **	EN DASH (ndash)
			    */
			    code = 0x2013;
			    break;
			case 151:
			    /*
			    **	EM DASH (mdash)
			    */
			    code = 0x2014;
			    break;
			case 152:
			    /*
			    **	SMALL TILDE (tilde)
			    */
			    code = 0x02dc;
			    break;
			case 153:
			    /*
			    **	TRADE MARK SIGN (trade)
			    */
			    code = 0x2122;
			    break;
			case 155:
			    /*
			    **	SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
			    **	(rsaquo)
			    */
			    code = 0x203a;
			    break;
			default:
			    /*
			    **	Do not attempt a conversion
			    **	to valid Unicode values.
			    */
			    break;
		       }
		    }
		    state = S_check_uni;
		}
		break;

	case S_check_uni:
		/*
		** Show the numeric entity if the value:
		**  (2) Is less than 32, and not valid and we don't
		**	have HTCJK set.
		**  (3) Is 127 and we don't have HTPassHighCtrlRaw
		**	or HTCJK set.
		**  (4) Is 128 - 159 and we don't have HTPassHighCtrlNum set.
		*/
		 if ((code < 32 &&
		     code != 9 && code != 10 && code != 13 &&
		     HTCJK == NOCJK) ||
		    (code == 127 &&
		     !(HTPassHighCtrlRaw || HTCJK != NOCJK)) ||
		    (code > 127 && code < 160 &&
		     !HTPassHighCtrlNum)) {
		     state = S_recover;
		     break;
		 }
		/*
		**  Convert the value as an unsigned char,
		**  hex escaped if isURL is set and it's
		**  8-bit, and then recycle the terminator
		**  if it is not a semicolon. - FM
		*/
		if (code > 159 && stype == st_URL) {
		    state = S_got_oututf8;
		    break;
		}
		    /*
		    **	For 160 (nbsp), use that value if it's
		    **	a hidden INPUT, otherwise use an ASCII
		    **	space (32) if plain_space is TRUE,
		    **	otherwise use the Lynx special character. - FM
		    */
		if (code == 160) {
		    if (hidden) {
			;
		    } else if (plain_space) {
			code = ' ';
		    } else {
			code = HT_NON_BREAK_SPACE;
		    }
		    state = S_got_outchar;
		    break;
		}
		/*
		    **	For 173 (shy), use that value if it's
		    **	a hidden INPUT, otherwise ignore it
		    **	if plain_space is TRUE, otherwise use
		    **	the Lynx special character. - FM
		    */
		if (code == 173) {
		    if (plain_space) {
			replace_buf[0] = '\0';
			state = S_got_outstring;
			break;
		    } else if (Back &&
			       !(LYCharSet_UC[cs_to].enc == UCT_ENC_8859 ||
				 (LYCharSet_UC[cs_to].like8859 &
					       UCT_R_8859SPECL))) {
			;	/* nothing, may be translated later */
		    } else if (hidden || Back) {
			state = S_got_outchar;
			break;
		    } else {
			code = LY_SOFT_HYPHEN;
			state = S_got_outchar;
			break;
		    }
		}
		/*
		**  Seek a translation from the chartrans tables.
		*/
		if ((uck = UCTransUniChar(code,
					  cs_to)) >= 32 &&
		    uck < 256 &&
		    (uck < 127 || uck >= lowest_8)) {
		    code = uck;
		    state = S_got_outchar;
		    break;
		} else if ((uck == -4 ||
			    (repl_translated_C0 &&
			     uck > 0 && uck < 32)) &&
			   /*
			   **  Not found; look for replacement string.
			   */
			   (uck = UCTransUniCharStr(replace_buf,
						    60, code,
						    cs_to,
						    0) >= 0)) {
		    state = S_got_outstring;
		    break;
		}
		if (output_utf8 &&
		    code > 127 && code < 0x7fffffffL) {
		    state = S_got_oututf8;
		    break;
		}
		/*
		**  For 8194 (ensp), 8195 (emsp), or 8201 (thinsp),
		**  use the character reference if it's a hidden INPUT,
		**  otherwise use an ASCII space (32) if plain_space is
		**  TRUE, otherwise use the Lynx special character. - FM
		*/
		if (code == 8194 || code == 8195 || code == 8201) {
		    if (hidden) {
			state = S_recover;
		    } else if (plain_space) {
			code = ' ';
			state = S_got_outchar;
		    } else {
			code = HT_EN_SPACE;
			state = S_got_outchar;
		    }
		    break;
		    /*
		    **	Ignore 8204 (zwnj), 8205 (zwj)
		    **	8206 (lrm), and 8207 (rlm),
		    **	for now, if we got this far without
		    **	finding a representation for them.
		    */
		} else if (code == 8204 || code == 8205 ||
			   code == 8206 || code == 8207) {
		    CTRACE(tfp, "LYUCFullyTranslateString: Ignoring '%ld'.\n", code);
		    replace_buf[0] = '\0';
		    state = S_got_outstring;
		    break;
		    /*
		    **	Show the numeric entity if the value:
		    **	(1) Is greater than 255 and unhandled Unicode.
		    */
		} else if (code > 255) {
		    /*
			**  Illegal or not yet handled value.
			**  Return "&#" verbatim and continue
			**  from there. - FM
			*/
		    state = S_recover;
		    break;
		/*
		**  If it's ASCII, or is 8-bit but HTPassEightBitNum
		**  is set or the character set is "ISO Latin 1",
		**  use it's value. - FM
		*/
		} else if (code < 161 ||
			   (code < 256 &&
			    (HTPassEightBitNum || cs_to == LATIN1))) {
		    /*
		    **	No conversion needed.
		    */
		    state = S_got_outchar;
		    break;
		    /*
		    **	If we get to here, convert and handle
		    **	the character as a named entity. - FM
		    */
		} else {
		    name = HTMLGetEntityName(code - 160);
		    state = S_check_name;
		    break;
		}

	case S_recover:
	    if (what == P_decimal || what == P_hex) {
		    /*
		    **	Illegal or not yet handled value.
		    **	Return "&#" verbatim and continue
		    **	from there. - FM
		    */
		    *q++ = '&';
		    *q++ = '#';
		    if (what == P_hex)
			*q++ = 'x';
		    if (cpe != '\0')
			*(p-1) = cpe;
		    p = cp;
		    state = S_done;
	    } else if (what == P_named) {
		*cp = cpe;
		*q++ = '&';
		state = S_done;
#ifdef NOTUSED_FOTEMODS
	    } else if (T.strip_raw_char_in &&
		(unsigned char)(*p) >= 0xc0 &&
		(unsigned char)(*p) < 255) {
		code = (((*p) & 0x7f));
		state = S_got_outchar;
#endif /* NOTUSED_FOTEMODS */
	    } else if (!T.output_utf8 && stype == st_HTML && !hidden &&
		!(HTPassEightBitRaw &&
		 (unsigned char)(*p) >= lowest_8)) {
		sprintf(replace_buf, "U%.2lX", code);
		state = S_got_outstring;
	    } else {
		puni = p;
		code = (unsigned char)(*p);
		state = S_got_outchar;
	    }
	    break;

	case S_named:
	    cp = ++p;
	    while (*cp && (unsigned char)*cp < 127 &&
		   isalnum((unsigned char)*cp))
		cp++;
	    cpe = *cp;
	    *cp = '\0';
/*	    ppuni = cp - 1; */
	    name = p;
	    state = S_check_name;
	    break;

	case S_check_name:
	    /*
	    **	Seek the Unicode value for the named entity.
	    **
	    **	!!!! We manually recover the case of '=' terminator which
	    **	is commonly found on query to CGI-scripts
	    **	enclosed as href= URLs like  "somepath/?x=1&yz=2"
	    **	Without this dirty fix, submission of such URLs was broken
	    **	if &yz string happened to be a recognized entity name. - LP
	    */
	   if ( ((code = HTMLGetEntityUCValue(name)) > 0) &&
		!((cpe == '=') && (stype == st_URL)) ) {
		state = S_check_uni;
		break;
	    }
	    /*
	    **	Didn't find the entity.
	    **	Return verbatim.
	    */
	    state = S_recover;
	    break;

				/* * * O U T P U T   S T A T E S * * */

	case S_got_oututf8:
	    if (code > 255 ||
		(code >= 128 && LYCharSet_UC[cs_to].enc == UCT_ENC_UTF8)) {
		UCPutUtf8ToBuffer(replace_buf, code, YES);
		state = S_got_outstring;
	    } else {
		state = S_got_outchar;
	    }
	    break;
	case S_got_outstring:
	    if (what == P_decimal || what == P_hex) {
		if (cpe != ';' && cpe != '\0')
		    *(--p) = cpe;
		p--;
	    } else if (what == P_named) {
		*cp = cpe;
		p = (*cp != ';') ? (cp - 1) : cp;
	    } else if (what == P_utf8) {
		p = puni;
	    }
	    if (replace_buf[0] == '\0') {
		state = S_next_char;
		break;
	    }
	    if (stype == st_URL) {
		code = replace_buf[0]; /* assume string OK if first char is */
		if (code >= 127 ||
		    (code < 32 && (code != 9 && code != 10 && code != 0))) {
		    state = S_put_urlstring;
		    break;
		}
	    }
	    REPLACE_STRING(replace_buf);
	    state = S_next_char;
	    break;
	case S_put_urlstring:
	    esc = HTEscape(replace_buf, URL_XALPHAS);
	    REPLACE_STRING(esc);
	    FREE(esc);
	    state = S_next_char;
	    break;
	case S_got_outchar:
	    if (what == P_decimal || what == P_hex) {
		if (cpe != ';' && cpe != '\0')
		    *(--p) = cpe;
		p--;
	    } else if (what == P_named) {
		*cp = cpe;
		p = (*cp != ';') ? (cp - 1) : cp;
	    } else if (what == P_utf8) {
		p = puni;
	    }
	    if (stype == st_URL &&
	    /*	Not a full HTEscape, only for 8bit and ctrl chars */
		(TOASCII(code) >= 127 ||  /* S/390 -- gil -- 1925 */
		 (code < ' ' && (code != '\t' && code != '\n')))) {
		    state = S_put_urlchar;
		    break;
	    } else if (!hidden && code == 10 && *p == 10
		       && q != qs && *(q-1) == 13) {
		/*
		**  If this is not a hidden string, and the current char is
		**  the LF ('\n') of a CRLF pair, drop the CR ('\r'). - KW
		*/
		*(q-1) = *p++;
		state = S_done;
		break;
	    }
	    *q++ = (char)code;
	    state = S_next_char;
	    break;
	case S_put_urlchar:
	    *q++ = '%';
	    REPLACE_CHAR(hex[(TOASCII(code) >> 4) & 15]);  /* S/390 -- gil -- 1944 */
	    REPLACE_CHAR(hex[(TOASCII(code) & 15)]);
				/* fall through */
	case S_next_char:
	    p++;		/* fall through */
	case S_done:
	    state = S_text;
	    what = P_text;
				/* for next round */
	}
    }

    *q = '\0';
    if (chunk) {
	HTChunkPutb(CHUNK, qs, q-qs + 1); /* also terminates */
	if (stype == st_URL) {
	    LYTrimHead(chunk->data);
	    LYTrimTail(chunk->data);
	}
	StrAllocCopy(*str, chunk->data);
	HTChunkFree(chunk);
    } else {
	if (stype == st_URL) {
	    LYTrimHead(qs);
	    LYTrimTail(qs);
	}
    }
    return str;
}

#undef REPLACE_CHAR
#undef REPLACE_STRING

PUBLIC BOOL LYUCFullyTranslateString ARGS7(
	char **, str,
	int,	cs_from,
	int,	cs_to,
	BOOL,	use_lynx_specials,
	BOOLEAN,	plain_space,
	BOOLEAN,	hidden,
	CharUtil_st,	stype)
{
    BOOL ret = YES;
    /* May reallocate *str even if cs_to == 0 */
    if (!LYUCFullyTranslateString_1(str, cs_from, cs_to, TRUE,
				    use_lynx_specials, plain_space, hidden,
				    NO, stype)) {
	ret = NO;
    }
    return ret;
}

PUBLIC BOOL LYUCTranslateBackFormData ARGS4(
	char **, str,
	int,	cs_from,
	int,	cs_to,
	BOOLEAN,	plain_space)
{
    char ** ret;
    /* May reallocate *str */
    ret = (LYUCFullyTranslateString_1(str, cs_from, cs_to, FALSE,
				       NO, plain_space, YES,
				       YES, st_HTML));
    return (ret != NULL);
}

/*
**  This function processes META tags in HTML streams. - FM
*/
PUBLIC void LYHandleMETA ARGS4(
	HTStructured *, 	me,
	CONST BOOL*,		present,
	CONST char **,		value,
	char **,		include GCC_UNUSED)
{
    char *http_equiv = NULL, *name = NULL, *content = NULL;
    char *href = NULL, *id_string = NULL, *temp = NULL;
    char *cp, *cp0, *cp1 = NULL;
    int url_type = 0;

    if (!me || !present)
	return;

    /*
     *	Load the attributes for possible use by Lynx. - FM
     */
    if (present[HTML_META_HTTP_EQUIV] &&
	value[HTML_META_HTTP_EQUIV] && *value[HTML_META_HTTP_EQUIV]) {
	StrAllocCopy(http_equiv, value[HTML_META_HTTP_EQUIV]);
	convert_to_spaces(http_equiv, TRUE);
	LYUCFullyTranslateString(&http_equiv, me->tag_charset, me->tag_charset,
				 NO, NO, YES, st_other);
	LYTrimHead(http_equiv);
	LYTrimTail(http_equiv);
	if (*http_equiv == '\0') {
	    FREE(http_equiv);
	}
    }
    if (present[HTML_META_NAME] &&
	value[HTML_META_NAME] && *value[HTML_META_NAME]) {
	StrAllocCopy(name, value[HTML_META_NAME]);
	convert_to_spaces(name, TRUE);
	LYUCFullyTranslateString(&name, me->tag_charset, me->tag_charset,
				 NO, NO, YES, st_other);
	LYTrimHead(name);
	LYTrimTail(name);
	if (*name == '\0') {
	    FREE(name);
	}
    }
    if (present[HTML_META_CONTENT] &&
	value[HTML_META_CONTENT] && *value[HTML_META_CONTENT]) {
	/*
	 *  Technically, we should be creating a comma-separated
	 *  list, but META tags come one at a time, and we'll
	 *  handle (or ignore) them as each is received.  Also,
	 *  at this point, we only trim leading and trailing
	 *  blanks from the CONTENT value, without translating
	 *  any named entities or numeric character references,
	 *  because how we should do that depends on what type
	 *  of information it contains, and whether or not any
	 *  of it might be sent to the screen. - FM
	 */
	StrAllocCopy(content, value[HTML_META_CONTENT]);
	convert_to_spaces(content, FALSE);
	LYTrimHead(content);
	LYTrimTail(content);
	if (*content == '\0') {
	    FREE(content);
	}
    }
    CTRACE(tfp, "LYHandleMETA: HTTP-EQUIV=\"%s\" NAME=\"%s\" CONTENT=\"%s\"\n",
		(http_equiv ? http_equiv : "NULL"),
		(name ? name : "NULL"),
		(content ? content : "NULL"));

    /*
     *	Make sure we have META name/value pairs to handle. - FM
     */
    if (!(http_equiv || name) || !content)
	goto free_META_copies;

    /*
     * Check for a no-cache Pragma
     * or Cache-Control directive. - FM
     */
    if (!strcasecomp((http_equiv ? http_equiv : ""), "Pragma") ||
	!strcasecomp((http_equiv ? http_equiv : ""), "Cache-Control")) {
	LYUCFullyTranslateString(&content, me->tag_charset, me->tag_charset,
				 NO, NO, YES, st_other);
	LYTrimHead(content);
	LYTrimTail(content);
	if (!strcasecomp(content, "no-cache")) {
	    me->node_anchor->no_cache = TRUE;
	    HText_setNoCache(me->text);
	}

	/*
	 *  If we didn't get a Cache-Control MIME header,
	 *  and the META has one, convert to lowercase,
	 *  store it in the anchor element, and if we
	 *  haven't yet set no_cache, check whether we
	 *  should. - FM
	 */
	if ((!me->node_anchor->cache_control) &&
	    !strcasecomp((http_equiv ? http_equiv : ""), "Cache-Control")) {
	    LYLowerCase(content);
	    StrAllocCopy(me->node_anchor->cache_control, content);
	    if (me->node_anchor->no_cache == FALSE) {
		cp0 = content;
		while ((cp = strstr(cp0, "no-cache")) != NULL) {
		    cp += 8;
		    while (*cp != '\0' && WHITE(*cp))
			cp++;
		    if (*cp == '\0' || *cp == ';') {
			me->node_anchor->no_cache = TRUE;
			HText_setNoCache(me->text);
			break;
		    }
		    cp0 = cp;
		}
		if (me->node_anchor->no_cache == TRUE)
		    goto free_META_copies;
		cp0 = content;
		while ((cp = strstr(cp0, "max-age")) != NULL) {
		    cp += 7;
		    while (*cp != '\0' && WHITE(*cp))
			cp++;
		    if (*cp == '=') {
			cp++;
			while (*cp != '\0' && WHITE(*cp))
			    cp++;
			if (isdigit((unsigned char)*cp)) {
			    cp0 = cp;
			    while (isdigit((unsigned char)*cp))
				cp++;
			    if (*cp0 == '0' && cp == (cp0 + 1)) {
				me->node_anchor->no_cache = TRUE;
				HText_setNoCache(me->text);
				break;
			    }
			}
		    }
		    cp0 = cp;
		}
	    }
	}

    /*
     * Check for an Expires directive. - FM
     */
    } else if (!strcasecomp((http_equiv ? http_equiv : ""), "Expires")) {
	/*
	 *  If we didn't get an Expires MIME header,
	 *  store it in the anchor element, and if we
	 *  haven't yet set no_cache, check whether we
	 *  should.  Note that we don't accept a Date
	 *  header via META tags, because it's likely
	 *  to be untrustworthy, but do check for a
	 *  Date header from a server when making the
	 *  comparison. - FM
	 */
	LYUCFullyTranslateString(&content, me->tag_charset, me->tag_charset,
				 NO, NO, YES, st_other);
	LYTrimHead(content);
	LYTrimTail(content);
	StrAllocCopy(me->node_anchor->expires, content);
	if (me->node_anchor->no_cache == FALSE) {
	    if (!strcmp(content, "0")) {
		/*
		 *  The value is zero, which we treat as
		 *  an absolute no-cache directive. - FM
		 */
		me->node_anchor->no_cache = TRUE;
		HText_setNoCache(me->text);
	    } else if (me->node_anchor->date != NULL) {
		/*
		 *  We have a Date header, so check if
		 *  the value is less than or equal to
		 *  that. - FM
		 */
		if (LYmktime(content, TRUE) <=
		    LYmktime(me->node_anchor->date, TRUE)) {
		    me->node_anchor->no_cache = TRUE;
		    HText_setNoCache(me->text);
		}
	    } else if (LYmktime(content, FALSE) <= 0) {
		/*
		 *  We don't have a Date header, and
		 *  the value is in past for us. - FM
		 */
		me->node_anchor->no_cache = TRUE;
		HText_setNoCache(me->text);
	    }
	}

    /*
     *	Check for a text/html Content-Type with a
     *	charset directive, if we didn't already set
     *	the charset via a server's header. - AAC & FM
     */
    } else if (!(me->node_anchor->charset && *me->node_anchor->charset) &&
	       !strcasecomp((http_equiv ? http_equiv : ""), "Content-Type")) {
	LYUCcharset * p_in = NULL;
	LYUCcharset * p_out = NULL;
	LYUCFullyTranslateString(&content, me->tag_charset, me->tag_charset,
				 NO, NO, YES, st_other);
	LYTrimHead(content);
	LYTrimTail(content);
	LYLowerCase(content);

	if ((cp = strstr(content, "text/html;")) != NULL &&
	    (cp1 = strstr(content, "charset")) != NULL &&
	    cp1 > cp) {
	    BOOL chartrans_ok = NO;
	    char *cp3 = NULL, *cp4;
	    int chndl;

	    cp1 += 7;
	    while (*cp1 == ' ' || *cp1 == '=' || *cp1 == '"')
		cp1++;

	    StrAllocCopy(cp3, cp1); /* copy to mutilate more */
	    for (cp4 = cp3; (*cp4 != '\0' && *cp4 != '"' &&
			     *cp4 != ';'  && *cp4 != ':' &&
			     !WHITE(*cp4)); cp4++) {
		; /* do nothing */
	    }
	    *cp4 = '\0';
	    cp4 = cp3;
	    chndl = UCGetLYhndl_byMIME(cp3);
	    if (UCCanTranslateFromTo(chndl, current_char_set)) {
		chartrans_ok = YES;
		StrAllocCopy(me->node_anchor->charset, cp4);
		HTAnchor_setUCInfoStage(me->node_anchor, chndl,
					UCT_STAGE_PARSER,
					UCT_SETBY_STRUCTURED);
	    } else if (chndl < 0) {
		/*
		 *  Got something but we don't recognize it.
		 */
		chndl = UCLYhndl_for_unrec;
		if (chndl < 0) /* UCLYhndl_for_unrec not defined :-( */
		     chndl = UCLYhndl_for_unspec; /* always >= 0 */
		if (UCCanTranslateFromTo(chndl, current_char_set)) {
		    chartrans_ok = YES;
		    HTAnchor_setUCInfoStage(me->node_anchor, chndl,
					    UCT_STAGE_PARSER,
					    UCT_SETBY_STRUCTURED);
		}
	    }
	    if (chartrans_ok) {
		p_in = HTAnchor_getUCInfoStage(me->node_anchor,
					       UCT_STAGE_PARSER);
		p_out = HTAnchor_setUCInfoStage(me->node_anchor,
						current_char_set,
						UCT_STAGE_HTEXT,
						UCT_SETBY_DEFAULT);
		if (!p_out) {
		    /*
		     *	Try again.
		     */
		    p_out = HTAnchor_getUCInfoStage(me->node_anchor,
						    UCT_STAGE_HTEXT);
		}
		if (!strcmp(p_in->MIMEname, "x-transparent")) {
		    HTPassEightBitRaw = TRUE;
		    HTAnchor_setUCInfoStage(me->node_anchor,
				HTAnchor_getUCLYhndl(me->node_anchor,
						     UCT_STAGE_HTEXT),
						     UCT_STAGE_PARSER,
						     UCT_SETBY_DEFAULT);
		}
		if (!strcmp(p_out->MIMEname, "x-transparent")) {
		    HTPassEightBitRaw = TRUE;
		    HTAnchor_setUCInfoStage(me->node_anchor,
				HTAnchor_getUCLYhndl(me->node_anchor,
						     UCT_STAGE_PARSER),
					    UCT_STAGE_HTEXT,
					    UCT_SETBY_DEFAULT);
		}
		if (p_in->enc != UCT_ENC_CJK) {
		    HTCJK = NOCJK;
		    if (!(p_in->codepoints &
			  UCT_CP_SUBSETOF_LAT1) &&
			chndl == current_char_set) {
			HTPassEightBitRaw = TRUE;
		    }
		} else if (p_out->enc == UCT_ENC_CJK) {
		    Set_HTCJK(p_in->MIMEname, p_out->MIMEname);
		}
		LYGetChartransInfo(me);
		/*
		**  Update the chartrans info homologously to
		**  a Content-Type MIME header with a charset
		**  parameter. - FM
		*/
		if (me->UCLYhndl != chndl) {
		    HTAnchor_setUCInfoStage(me->node_anchor, chndl,
					    UCT_STAGE_MIME,
					    UCT_SETBY_STRUCTURED);
		    HTAnchor_setUCInfoStage(me->node_anchor, chndl,
					    UCT_STAGE_PARSER,
					    UCT_SETBY_STRUCTURED);
		    me->inUCLYhndl = HTAnchor_getUCLYhndl(me->node_anchor,
							  UCT_STAGE_PARSER);
		    me->inUCI = HTAnchor_getUCInfoStage(me->node_anchor,
							UCT_STAGE_PARSER);
		}
		UCSetTransParams(&me->T,
				 me->inUCLYhndl, me->inUCI,
				 me->outUCLYhndl, me->outUCI);
	    } else {
		/*
		 *  Cannot translate.
		 *  If according to some heuristic the given
		 *  charset and the current display character
		 *  both are likely to be like ISO-8859 in
		 *  structure, pretend we have some kind
		 *  of match.
		 */
		BOOL given_is_8859
		    = (!strncmp(cp4, "iso-8859-", 9) &&
		       isdigit((unsigned char)cp4[9]));
		BOOL given_is_8859like
		    = (given_is_8859 || !strncmp(cp4, "windows-", 8) ||
			!strncmp(cp4, "cp12", 4) ||
			!strncmp(cp4, "cp-12", 5));
		BOOL given_and_display_8859like
		    = (given_is_8859like &&
		       (strstr(LYchar_set_names[current_char_set],
			       "ISO-8859") ||
			strstr(LYchar_set_names[current_char_set],
			       "windows-")));

		if (given_is_8859) {
		    cp1 = &cp4[10];
		    while (*cp1 &&
			   isdigit((unsigned char)(*cp1)))
			cp1++;
		    *cp1 = '\0';
		}
		if (given_and_display_8859like) {
		    StrAllocCopy(me->node_anchor->charset, cp4);
		    HTPassEightBitRaw = TRUE;
		}
		HTAlert(*cp4 ? cp4 : me->node_anchor->charset);

	    }
	    FREE(cp3);

	    if (me->node_anchor->charset) {
		CTRACE(tfp,
			"LYHandleMETA: New charset: %s\n",
			me->node_anchor->charset);
	    }
	}
	/*
	 *  Set the kcode element based on the charset. - FM
	 */
	HText_setKcode(me->text, me->node_anchor->charset, p_in);

    /*
     *	Check for a Refresh directive. - FM
     */
    } else if (!strcasecomp((http_equiv ? http_equiv : ""), "Refresh")) {
	char *Seconds = NULL;

	/*
	 *  Look for the Seconds field. - FM
	 */
	cp = LYSkipBlanks(content);
	if (*cp && isdigit(*cp)) {
	    cp1 = cp;
	    while (*cp1 && isdigit(*cp1))
		cp1++;
	    if (*cp1)
		*cp1++ = '\0';
	    StrAllocCopy(Seconds, cp);
	}
	if (Seconds) {
	    /*
	     *	We have the seconds field.
	     *	Now look for a URL field - FM
	     */
	    while (*cp1) {
		if (!strncasecomp(cp1, "URL", 3)) {
		    cp = (cp1 + 3);
		    while (*cp && (*cp == '=' || isspace((unsigned char)*cp)))
			cp++;
		    cp1 = cp;
		    while (*cp1 && !isspace((unsigned char)*cp1))
			cp1++;
		    *cp1 = '\0';
		    if (*cp)
			StrAllocCopy(href, cp);
		    break;
		}
		cp1++;
	    }
	    if (href) {
		/*
		 *  We found a URL field, so check it out. - FM
		 */
		if (!(url_type = LYLegitimizeHREF(me, (char**)&href,
						  TRUE, FALSE))) {
		    /*
		     *	The specs require a complete URL,
		     *	but this is a Netscapism, so don't
		     *	expect the author to know that. - FM
		     */
		    HTUserMsg(REFRESH_URL_NOT_ABSOLUTE);
		    /*
		     *	Use the document's address
		     *	as the base. - FM
		     */
		    if (*href != '\0') {
			temp = HTParse(href,
				       me->node_anchor->address, PARSE_ALL);
			StrAllocCopy(href, temp);
			FREE(temp);
		    } else {
			StrAllocCopy(href, me->node_anchor->address);
			HText_setNoCache(me->text);
		    }
		}
		/*
		 *  Check whether to fill in localhost. - FM
		 */
		LYFillLocalFileURL((char **)&href,
				   (me->inBASE ?
				 me->base_href : me->node_anchor->address));
		/*
		 *  Set the no_cache flag if the Refresh URL
		 *  is the same as the document's address. - FM
		 */
		if (!strcmp(href, me->node_anchor->address)) {
		    HText_setNoCache(me->text);
		}
	    } else {
		/*
		 *  We didn't find a URL field, so use
		 *  the document's own address and set
		 *  the no_cache flag. - FM
		 */
		StrAllocCopy(href, me->node_anchor->address);
		HText_setNoCache(me->text);
	    }
	    /*
	     *	Check for an anchor in http or https URLs. - FM
	     */
	    if ((strncmp(href, "http", 4) == 0) &&
		(cp = strrchr(href, '#')) != NULL) {
		StrAllocCopy(id_string, cp);
		*cp = '\0';
	    }
	    if (me->inA) {
		/*
		 *  Ugh!  The META tag, which is a HEAD element,
		 *  is in an Anchor, which is BODY element.  All
		 *  we can do is close the Anchor and cross our
		 *  fingers. - FM
		 */
		if (me->inBoldA == TRUE && me->inBoldH == FALSE)
		    HText_appendCharacter(me->text, LY_BOLD_END_CHAR);
		me->inBoldA = FALSE;
		HText_endAnchor(me->text, me->CurrentANum);
		me->inA = FALSE;
		me->CurrentANum = 0;
	    }
	    me->CurrentA = HTAnchor_findChildAndLink(
				me->node_anchor,	/* Parent */
				id_string,		/* Tag */
				href,			/* Addresss */
				(void *)0);		/* Type */
	    if (id_string)
		*cp = '#';
	    FREE(id_string);
	    LYEnsureSingleSpace(me);
	    if (me->inUnderline == FALSE)
		HText_appendCharacter(me->text, LY_UNDERLINE_START_CHAR);
	    HTML_put_string(me, "REFRESH(");
	    HTML_put_string(me, Seconds);
	    HTML_put_string(me, " sec):");
	    FREE(Seconds);
	    if (me->inUnderline == FALSE)
		HText_appendCharacter(me->text, LY_UNDERLINE_END_CHAR);
	    HTML_put_character(me, ' ');
	    me->in_word = NO;
	    HText_beginAnchor(me->text, me->inUnderline, me->CurrentA);
	    if (me->inBoldH == FALSE)
		HText_appendCharacter(me->text, LY_BOLD_START_CHAR);
	    HTML_put_string(me, href);
	    FREE(href);
	    if (me->inBoldH == FALSE)
		HText_appendCharacter(me->text, LY_BOLD_END_CHAR);
	    HText_endAnchor(me->text, 0);
	    LYEnsureSingleSpace(me);
	}

    /*
     *	Check for a suggested filename via a Content-Disposition with
     *	a filename=name.suffix in it, if we don't already have it
     *	via a server header. - FM
     */
    } else if (!(me->node_anchor->SugFname && *me->node_anchor->SugFname) &&
	       !strcasecomp((http_equiv ?
			     http_equiv : ""), "Content-Disposition")) {
	cp = content;
	while (*cp != '\0' && strncasecomp(cp, "filename", 8))
	    cp++;
	if (*cp != '\0') {
	    cp += 8;
	    while ((*cp != '\0') && (WHITE(*cp) || *cp == '='))
		cp++;
	    while (*cp != '\0' && WHITE(*cp))
		cp++;
	    if (*cp != '\0') {
		StrAllocCopy(me->node_anchor->SugFname, cp);
		if (*me->node_anchor->SugFname == '\"') {
		    if ((cp = strchr((me->node_anchor->SugFname + 1),
				     '\"')) != NULL) {
			*(cp + 1) = '\0';
			HTMIME_TrimDoubleQuotes(me->node_anchor->SugFname);
		    } else {
			FREE(me->node_anchor->SugFname);
		    }
		    if (me->node_anchor->SugFname != NULL &&
			*me->node_anchor->SugFname == '\0') {
			FREE(me->node_anchor->SugFname);
		    }
		}
		if ((cp = me->node_anchor->SugFname) != NULL) {
		    while (*cp != '\0' && !WHITE(*cp))
			cp++;
		    *cp = '\0';
		    if (*me->node_anchor->SugFname == '\0')
			FREE(me->node_anchor->SugFname);
		}
	    }
	}
    /*
     *	Check for a Set-Cookie directive. - AK
     */
    } else if (!strcasecomp((http_equiv ? http_equiv : ""), "Set-Cookie")) {
	/*
	 *  This will need to be updated when Set-Cookie/Set-Cookie2
	 *  handling is finalized.  For now, we'll still assume
	 *  "historical" cookies in META directives. - FM
	 */
	url_type = is_url(me->inBASE ?
		       me->base_href : me->node_anchor->address);
	if (url_type == HTTP_URL_TYPE || url_type == HTTPS_URL_TYPE) {
	    LYSetCookie(content,
			NULL,
			(me->inBASE ?
		      me->base_href : me->node_anchor->address));
	}
    }

    /*
     *	Free the copies. - FM
     */
free_META_copies:
    FREE(http_equiv);
    FREE(name);
    FREE(content);
}

/*
**  This function handles P elements in HTML streams.
**  If start is TRUE it handles a start tag, and if
**  FALSE, an end tag.	We presently handle start
**  and end tags identically, but this can lead to
**  a different number of blank lines between the
**  current paragraph and subsequent text when a P
**  end tag is present or not in the markup. - FM
*/
PUBLIC void LYHandleP ARGS5(
	HTStructured *, 	me,
	CONST BOOL*,		present,
	CONST char **,		value,
	char **,		include GCC_UNUSED,
	BOOL,			start)
{
    if (TRUE) {
	/*
	 *  FIG content should be a true block, which like P inherits
	 *  the current style.	APPLET is like character elements or
	 *  an ALT attribute, unless it content contains a block element.
	 *  If we encounter a P in either's content, we set flags to treat
	 *  the content as a block.  - FM
	 */
	if (start) {
	    if (me->inFIG)
		me->inFIGwithP = TRUE;

	    if (me->inAPPLET)
		me->inAPPLETwithP = TRUE;
	}

	UPDATE_STYLE;
	if (me->List_Nesting_Level >= 0) {
	    /*
	     *	We're in a list.  Treat P as an instruction to
	     *	create one blank line, if not already present,
	     *	then fall through to handle attributes, with
	     *	the "second line" margins. - FM
	     */
	    if (me->inP) {
		if (me->inFIG || me->inAPPLET ||
		    me->inCAPTION || me->inCREDIT ||
		    me->sp->style->spaceAfter > 0 ||
		    (start && me->sp->style->spaceBefore > 0)) {
		    LYEnsureDoubleSpace(me);
		} else {
		    LYEnsureSingleSpace(me);
		}
	    }
	} else if (me->sp[0].tag_number == HTML_ADDRESS) {
	    /*
	     *	We're in an ADDRESS.  Treat P as an instruction
	     *	to start a newline, if needed, then fall through
	     *	to handle attributes. - FM
	     */
	    if (HText_LastLineSize(me->text, FALSE)) {
		HText_setLastChar(me->text, ' ');  /* absorb white space */
		HText_appendCharacter(me->text, '\r');
	    }
	} else {
	    if (start) {
		if (!(me->inLABEL && !me->inP)) {
		    HText_appendParagraph(me->text);
		}
	    } else if (me->sp->style->spaceAfter > 0) {
		LYEnsureDoubleSpace(me);
	    } else {
		LYEnsureSingleSpace(me);
	    }
	    me->inLABEL = FALSE;
	}
	me->in_word = NO;

	if (LYoverride_default_alignment(me)) {
	    me->sp->style->alignment = LYstyles(me->sp[0].tag_number)->alignment;
	} else if (me->List_Nesting_Level >= 0 ||
		   ((me->Division_Level < 0) &&
		    (!strcmp(me->sp->style->name, "Normal") ||
		     !strcmp(me->sp->style->name, "Preformatted")))) {
		me->sp->style->alignment = HT_LEFT;
	} else {
	    me->sp->style->alignment = me->current_default_alignment;
	}

	if (start) {
	    if (present && present[HTML_P_ALIGN] && value[HTML_P_ALIGN]) {
		if (!strcasecomp(value[HTML_P_ALIGN], "center") &&
		    !(me->List_Nesting_Level >= 0 && !me->inP))
		    me->sp->style->alignment = HT_CENTER;
		else if (!strcasecomp(value[HTML_P_ALIGN], "right") &&
		    !(me->List_Nesting_Level >= 0 && !me->inP))
		    me->sp->style->alignment = HT_RIGHT;
		else if (!strcasecomp(value[HTML_P_ALIGN], "left") ||
			 !strcasecomp(value[HTML_P_ALIGN], "justify"))
		    me->sp->style->alignment = HT_LEFT;
	    }

	    CHECK_ID(HTML_P_ID);
	}

	/*
	 *  Mark that we are starting a new paragraph
	 *  and don't have any of it's text yet. - FM
	 *
	 */
	me->inP = FALSE;
    }

    return;
}

/*
**  This function handles SELECT elements in HTML streams.
**  If start is TRUE it handles a start tag, and if FALSE,
**  an end tag. - FM
*/
PUBLIC void LYHandleSELECT ARGS5(
	HTStructured *, 	me,
	CONST BOOL*,		present,
	CONST char **,		value,
	char **,		include GCC_UNUSED,
	BOOL,			start)
{
    int i;

    if (start == TRUE) {
	char *name = NULL;
	BOOLEAN multiple = NO;
	char *size = NULL;

	/*
	 *  Initialize the disable attribute.
	 */
	me->select_disabled = FALSE;

	/*
	 *  Make sure we're in a form.
	 */
	if (!me->inFORM) {
	    if (TRACE) {
		fprintf(tfp,
			"Bad HTML: SELECT start tag not within FORM tag\n");
	    } else if (!me->inBadHTML) {
		HTUserMsg(BAD_HTML_USE_TRACE);
		me->inBadHTML = TRUE;
	    }

	    /*
	     *	We should have covered all crash possibilities with the
	     *	current TagSoup parser, so we'll allow it because some
	     *	people with other browsers use SELECT for "information"
	     *	popups, outside of FORM blocks, though no Lynx user
	     *	would do anything that awful, right? - FM
	     *//***
	    return;
		***/
	}

	/*
	 *  Check for unclosed TEXTAREA.
	 */
	if (me->inTEXTAREA) {
	    if (TRACE) {
		fprintf(tfp, "Bad HTML: Missing TEXTAREA end tag\n");
	    } else if (!me->inBadHTML) {
		HTUserMsg(BAD_HTML_USE_TRACE);
		me->inBadHTML = TRUE;
	    }
	}

	/*
	 *  Set to know we are in a select tag.
	 */
	me->inSELECT = TRUE;

	if (!(present && present[HTML_SELECT_NAME] &&
	      value[HTML_SELECT_NAME]  && *value[HTML_SELECT_NAME])) {
	    StrAllocCopy(name, "");
	} else if (strchr(value[HTML_SELECT_NAME], '&') == NULL) {
	    StrAllocCopy(name, value[HTML_SELECT_NAME]);
	} else {
	    StrAllocCopy(name, value[HTML_SELECT_NAME]);
	    UNESCAPE_FIELDNAME_TO_STD(&name);
	}
	if (present && present[HTML_SELECT_MULTIPLE])
	    multiple=YES;
	if (present && present[HTML_SELECT_DISABLED])
	    me->select_disabled = TRUE;
	if (present && present[HTML_SELECT_SIZE] &&
	    value[HTML_SELECT_SIZE] && *value[HTML_SELECT_SIZE]) {
#ifdef NOTDEFINED
	    StrAllocCopy(size, value[HTML_SELECT_SIZE]);
#else
	    /*
	     *	Let the size be determined by the number of OPTIONs. - FM
	     */
	    CTRACE(tfp, "LYHandleSELECT: Ignoring SIZE=\"%s\" for SELECT.\n",
			value[HTML_SELECT_SIZE]);
#endif /* NOTDEFINED */
	}

	if (me->inBoldH == TRUE &&
	    (multiple == NO || LYSelectPopups == FALSE)) {
	    HText_appendCharacter(me->text, LY_BOLD_END_CHAR);
	    me->inBoldH = FALSE;
	    me->needBoldH = TRUE;
	}
	if (me->inUnderline == TRUE &&
	    (multiple == NO || LYSelectPopups == FALSE)) {
	    HText_appendCharacter(me->text, LY_UNDERLINE_END_CHAR);
	    me->inUnderline = FALSE;
	}

	if ((multiple == NO && LYSelectPopups == TRUE) &&
	    (me->sp[0].tag_number == HTML_PRE || me->inPRE == TRUE ||
	     !me->sp->style->freeFormat) &&
	    HText_LastLineSize(me->text, FALSE) > (LYcols - 8)) {
	    /*
	     *	Force a newline when we're using a popup in
	     *	a PRE block and are within 7 columns from the
	     *	right margin.  This will allow for the '['
	     *	popup designator and help avoid a wrap in the
	     *	underscore placeholder for the retracted popup
	     *	entry in the HText structure. - FM
	     */
	    HTML_put_character(me, '\n');
	    me->in_word = NO;
	}

	LYCheckForID(me, present, value, (int)HTML_SELECT_ID);

	HText_beginSelect(name, ATTR_CS_IN, multiple, size);
	FREE(name);
	FREE(size);

	me->first_option = TRUE;
    } else {
	/*
	 *  Handle end tag.
	 */
	char *ptr;

	/*
	 *  Make sure we had a select start tag.
	 */
	if (!me->inSELECT) {
	    if (TRACE) {
		fprintf(tfp, "Bad HTML: Unmatched SELECT end tag\n");
	    } else if (!me->inBadHTML) {
		HTUserMsg(BAD_HTML_USE_TRACE);
		me->inBadHTML = TRUE;
	    }
	    return;
	}

	/*
	 *  Set to know that we are no longer in a select tag.
	 */
	me->inSELECT = FALSE;

	/*
	 *  Clear the disable attribute.
	 */
	me->select_disabled = FALSE;

	/*
	 *  Finish the data off.
	 */
	HTChunkTerminate(&me->option);
	/*
	 *  Finish the previous option.
	 */
	ptr = HText_setLastOptionValue(me->text,
				       me->option.data,
				       me->LastOptionValue,
				       LAST_ORDER,
				       me->LastOptionChecked,
				       me->UCLYhndl,
				       ATTR_CS_IN);
	FREE(me->LastOptionValue);

	me->LastOptionChecked = FALSE;

	if (HTCurSelectGroupType == F_CHECKBOX_TYPE ||
	    LYSelectPopups == FALSE) {
	    /*
	     *	Start a newline after the last checkbox/button option.
	     */
	    LYEnsureSingleSpace(me);
	} else {
	    /*
	     *	Output popup box with the default option to screen,
	     *	but use non-breaking spaces for output.
	     */
	    if (ptr &&
		me->sp[0].tag_number == HTML_PRE && strlen(ptr) > 6) {
		/*
		 *  The code inadequately handles OPTION fields in PRE tags.
		 *  We'll put up a minimum of 6 characters, and if any
		 *  more would exceed the wrap column, we'll ignore them.
		 */
		for (i = 0; i < 6; i++) {
		    if (*ptr == ' ')
			HText_appendCharacter(me->text, HT_NON_BREAK_SPACE);
		    else
			HText_appendCharacter(me->text, *ptr);
		    ptr++;
		}
		HText_setIgnoreExcess(me->text, TRUE);
	    }
	    for (; ptr && *ptr != '\0'; ptr++) {
		if (*ptr == ' ')
		    HText_appendCharacter(me->text, HT_NON_BREAK_SPACE);
		else
		    HText_appendCharacter(me->text, *ptr);
	    }
	    /*
	     *	Add end option character.
	     */
	    if (!me->first_option) {
		HText_appendCharacter(me->text, ']');
		HText_setLastChar(me->text, ']');
		me->in_word = YES;
	    }
	    HText_setIgnoreExcess(me->text, FALSE);
	}
	HTChunkClear(&me->option);

	if (me->Underline_Level > 0 && me->inUnderline == FALSE) {
	    HText_appendCharacter(me->text, LY_UNDERLINE_START_CHAR);
	    me->inUnderline = TRUE;
	}
	if (me->needBoldH == TRUE && me->inBoldH == FALSE) {
	    HText_appendCharacter(me->text, LY_BOLD_START_CHAR);
	    me->inBoldH = TRUE;
	    me->needBoldH = FALSE;
	}
    }
}

/*
**  This function strips white characters and
**  generally fixes up attribute values that
**  were received from the SGML parser and
**  are to be treated as partial or absolute
**  URLs. - FM
*/
PUBLIC int LYLegitimizeHREF ARGS4(
	HTStructured *, 	me,
	char **,		href,
	BOOL,			force_slash,
	BOOL,			strip_dots)
{
    int url_type = 0;
    char *pound = NULL;
    char *fragment = NULL;

    if (!me || !href || *href == NULL || *(*href) == '\0')
	return(url_type);

    if (!LYTrimStartfile(*href)) {
	/*
	 *  Collapse spaces in the actual URL, but just
	 *  protect against tabs or newlines in the
	 *  fragment, if present.  This seeks to cope
	 *  with atrocities inflicted on the Web by
	 *  authoring tools such as Frontpage. - FM
	 */
	if ((pound = strchr(*href, '#')) != NULL) {
	    StrAllocCopy(fragment, pound);
	    *pound = '\0';
	    convert_to_spaces(fragment, FALSE);
	}
	LYRemoveBlanks(*href);
	if (fragment != NULL) {
	    StrAllocCat(*href, fragment);
	    FREE(fragment);
	}
    }
    if (*(*href) == '\0')
	return(url_type);
    LYUCFullyTranslateString(href, me->tag_charset, me->tag_charset,
			     NO, NO, YES, st_URL);
    url_type = is_url(*href);
    if (!url_type && force_slash &&
	(!strcmp(*href, ".") || !strcmp(*href, "..")) &&
	 strncmp((me->inBASE ?
	       me->base_href : me->node_anchor->address),
		 "file:", 5)) {
	/*
	 *  The Fielding RFC/ID for resolving partial HREFs says
	 *  that a slash should be on the end of the preceding
	 *  symbolic element for "." and "..", but all tested
	 *  browsers only do that for an explicit "./" or "../",
	 *  so we'll respect the RFC/ID only if force_slash was
	 *  TRUE and it's not a file URL. - FM
	 */
	StrAllocCat(*href, "/");
    }
    if ((!url_type && LYStripDotDotURLs && strip_dots && *(*href) == '.') &&
	 !strncasecomp((me->inBASE ?
		     me->base_href : me->node_anchor->address),
		       "http", 4)) {
	/*
	 *  We will be resolving a partial reference versus an http
	 *  or https URL, and it has lead dots, which may be retained
	 *  when resolving via HTParse(), but the request would fail
	 *  if the first element of the resultant path is two dots,
	 *  because no http or https server accepts such paths, and
	 *  the current URL draft, likely to become an RFC, says that
	 *  it's optional for the UA to strip them as a form of error
	 *  recovery.  So we will, recursively, for http/https URLs,
	 *  like the "major market browsers" which made this problem
	 *  so common on the Web, but we'll also issue a message about
	 *  it, such that the bad partial reference might get corrected
	 *  by the document provider. - FM
	 */
	char *temp = NULL, *path = NULL, *cp;
	CONST char *str = "";

	if (((temp = HTParse(*href,
			     (me->inBASE ?
			   me->base_href : me->node_anchor->address),
			     PARSE_ALL)) != NULL && temp[0] != '\0') &&
	    (path = HTParse(temp, "",
			    PARSE_PATH+PARSE_PUNCTUATION)) != NULL &&
	    !strncmp(path, "/..", 3)) {
	    cp = (path + 3);
	    if (LYIsHtmlSep(*cp) || *cp == '\0') {
		if ((me->inBASE ?
	       me->base_href[4] : me->node_anchor->address[4]) == 's') {
		    str = "s";
		}
		if (TRACE) {
		    fprintf(tfp,
			 "LYLegitimizeHREF: Bad value '%s' for http%s URL.\n",
			   *href, str);
		    fprintf(tfp,
			 "                  Stripping lead dots.\n");
		} else if (!me->inBadHREF) {
		    HTUserMsg(BAD_PARTIAL_REFERENCE);
		    me->inBadHREF = TRUE;
		}
	    }
	    if (*cp == '\0') {
		StrAllocCopy(*href, "/");
	    } else if (LYIsHtmlSep(*cp)) {
		while (!strncmp(cp, "/..", 3)) {
		    if (*(cp + 3) == '/') {
			cp += 3;
			continue;
		    } else if (*(cp + 3) == '\0') {
			*(cp + 1) = '\0';
			*(cp + 2) = '\0';
		    }
		    break;
		}
		StrAllocCopy(*href, cp);
	    }
	}
	FREE(temp);
	FREE(path);
    }
    return(url_type);
}

/*
**  This function checks for a Content-Base header,
**  and if not present, a Content-Location header
**  which is an absolute URL, and sets the BASE
**  accordingly.  If set, it will be replaced by
**  any BASE tag in the HTML stream, itself. - FM
*/
PUBLIC void LYCheckForContentBase ARGS1(
	HTStructured *, 	me)
{
    char *cp = NULL;
    BOOL present[HTML_BASE_ATTRIBUTES];
    CONST char *value[HTML_BASE_ATTRIBUTES];
    int i;

    if (!(me && me->node_anchor))
	return;

    if (me->node_anchor->content_base != NULL) {
	/*
	 *  We have a Content-Base value.  Use it
	 *  if it's non-zero length. - FM
	 */
	if (*me->node_anchor->content_base == '\0')
	    return;
	StrAllocCopy(cp, me->node_anchor->content_base);
	LYRemoveBlanks(cp);
    } else if (me->node_anchor->content_location != NULL) {
	/*
	 *  We didn't have a Content-Base value, but do
	 *  have a Content-Location value.  Use it if
	 *  it's an absolute URL. - FM
	 */
	if (*me->node_anchor->content_location == '\0')
	    return;
	StrAllocCopy(cp, me->node_anchor->content_location);
	LYRemoveBlanks(cp);
	if (!is_url(cp)) {
	    FREE(cp);
	    return;
	}
    } else {
	/*
	 *  We had neither a Content-Base nor
	 *  Content-Location value. - FM
	 */
	return;
    }

    /*
     *	If we collapsed to a zero-length value,
     *	ignore it. - FM
     */
    if (*cp == '\0') {
	FREE(cp);
	return;
    }

    /*
     *	Pass the value to HTML_start_element as
     *	the HREF of a BASE tag. - FM
     */
    for (i = 0; i < HTML_BASE_ATTRIBUTES; i++)
	 present[i] = NO;
    present[HTML_BASE_HREF] = YES;
    value[HTML_BASE_HREF] = (CONST char *)cp;
    (*me->isa->start_element)(me, HTML_BASE, present, value,
			      0, 0);
    FREE(cp);
}

/*
**  This function creates NAMEd Anchors if a non-zero-length NAME
**  or ID attribute was present in the tag. - FM
*/
PUBLIC void LYCheckForID ARGS4(
	HTStructured *, 	me,
	CONST BOOL *,		present,
	CONST char **,		value,
	int,			attribute)
{
    HTChildAnchor *ID_A = NULL;
    char *temp = NULL;

    if (!(me && me->text))
	return;

    if (present && present[attribute]
	&& value[attribute] && *value[attribute]) {
	/*
	 *  Translate any named or numeric character references. - FM
	 */
	StrAllocCopy(temp, value[attribute]);
	LYUCFullyTranslateString(&temp, me->tag_charset, me->tag_charset,
				 NO, NO, YES, st_URL);

	/*
	 *  Create the link if we still have a non-zero-length string. - FM
	 */
	if ((temp[0] != '\0') &&
	    (ID_A = HTAnchor_findChildAndLink(
				me->node_anchor,	/* Parent */
				temp,			/* Tag */
				NULL,			/* Addresss */
				(void *)0))) {		/* Type */
	    HText_beginAnchor(me->text, me->inUnderline, ID_A);
	    HText_endAnchor(me->text, 0);
	}
	FREE(temp);
    }
}

/*
**  This function creates a NAMEd Anchor for the ID string
**  passed to it directly as an argument.  It assumes the
**  does not need checking for character references. - FM
*/
PUBLIC void LYHandleID ARGS2(
	HTStructured *, 	me,
	char *, 		id)
{
    HTChildAnchor *ID_A = NULL;

    if (!(me && me->text) ||
	!(id && *id))
	return;

    /*
     *	Create the link if we still have a non-zero-length string. - FM
     */
    if ((ID_A = HTAnchor_findChildAndLink(
				me->node_anchor,	/* Parent */
				id,			/* Tag */
				NULL,			/* Addresss */
				(void *)0)) != NULL) {	/* Type */
	HText_beginAnchor(me->text, me->inUnderline, ID_A);
	HText_endAnchor(me->text, 0);
    }
}

/*
**  This function checks whether we want to override
**  the current default alignment for paragraphs and
**  instead use that specified in the element's style
**  sheet. - FM
*/
PUBLIC BOOLEAN LYoverride_default_alignment ARGS1(
	HTStructured *, me)
{
    if (!me)
	return NO;

    switch(me->sp[0].tag_number) {
	case HTML_BLOCKQUOTE:
	case HTML_BQ:
	case HTML_NOTE:
	case HTML_FN:
	case HTML_ADDRESS:
	    me->sp->style->alignment = HT_LEFT;
	    return YES;

	default:
	    break;
    }
    return NO;
}

/*
**  This function inserts newlines if needed to create double spacing,
**  and sets the left margin for subsequent text to the second line
**  indentation of the current style. - FM
*/
PUBLIC void LYEnsureDoubleSpace ARGS1(
	HTStructured *, me)
{
    if (!me || !me->text)
	return;

    if (HText_LastLineSize(me->text, FALSE)) {
	HText_setLastChar(me->text, ' ');  /* absorb white space */
	HText_appendCharacter(me->text, '\r');
	HText_appendCharacter(me->text, '\r');
    } else if (HText_PreviousLineSize(me->text, FALSE)) {
	HText_setLastChar(me->text, ' ');  /* absorb white space */
	HText_appendCharacter(me->text, '\r');
    } else if (me->List_Nesting_Level >= 0) {
	HText_NegateLineOne(me->text);
    }
    me->in_word = NO;
    return;
}

/*
**  This function inserts a newline if needed to create single spacing,
**  and sets the left margin for subsequent text to the second line
**  indentation of the current style. - FM
*/
PUBLIC void LYEnsureSingleSpace ARGS1(
	HTStructured *, me)
{
    if (!me || !me->text)
	return;

    if (HText_LastLineSize(me->text, FALSE)) {
	HText_setLastChar(me->text, ' ');  /* absorb white space */
	HText_appendCharacter(me->text, '\r');
    } else if (me->List_Nesting_Level >= 0) {
	HText_NegateLineOne(me->text);
    }
    me->in_word = NO;
    return;
}

/*
**  This function resets paragraph alignments for block
**  elements which do not have a defined style sheet. - FM
*/
PUBLIC void LYResetParagraphAlignment ARGS1(
	HTStructured *, me)
{
    if (!me)
	return;

    if (me->List_Nesting_Level >= 0 ||
	((me->Division_Level < 0) &&
	 (!strcmp(me->sp->style->name, "Normal") ||
	  !strcmp(me->sp->style->name, "Preformatted")))) {
	me->sp->style->alignment = HT_LEFT;
    } else {
	me->sp->style->alignment = me->current_default_alignment;
    }
    return;
}

/*
**  This example function checks whether the given anchor has
**  an address with a file scheme, and if so, loads it into the
**  the SGML parser's context->url element, which was passed as
**  the second argument.  The handle_comment() calling function in
**  SGML.c then calls LYDoCSI() in LYUtils.c to insert HTML markup
**  into the corresponding stream, homologously to an SSI by an
**  HTTP server. - FM
**
**  For functions similar to this but which depend on details of
**  the HTML handler's internal data, the calling interface should
**  be changed, and functions in SGML.c would have to make sure not
**  to call such functions inappropriately (e.g., calling a function
**  specific to the Lynx_HTML_Handler when SGML.c output goes to
**  some other HTStructured object like in HTMLGen.c), or the new
**  functions could be added to the SGML.h interface.
*/
PUBLIC BOOLEAN LYCheckForCSI ARGS2(
	HTParentAnchor *,	anchor,
	char **,		url)
{
    if (!(anchor && anchor->address))
	return FALSE;

    if (strncasecomp(anchor->address, "file:", 5))
	return FALSE;

    if (!LYisLocalHost(anchor->address))
	return FALSE;

    StrAllocCopy(*url, anchor->address);
    return TRUE;
}

/*
**  This function is called from the SGML parser to look at comments
**  and see whether we should collect some info from them.  Currently
**  it only looks for comments with Message-Id and Subject info, in the
**  exact form generated by MHonArc for archived mailing list.  If found,
**  the info is stored in the document's HTParentAnchor.  It can later be
**  used for generating a mail response.
**
**  We are extra picky here because there isn't any official definition
**  for these kinds of comments - we might (and still can) misinterpret
**  arbitrary comments as something they aren't.
**
**  If something doesn't look right, for example invalid characters, the
**  strings are not stored.  Mail responses will use something else as
**  the subject, probably the document URL, and will not have an
**  In-Reply-To header.
**
**  All this is a hack - to do this the right way, mailing list archivers
**  would have to agree on some better mechanism to make this kind of info
**  from original mail headers available, for example using LINK.  - kw
*/
PUBLIC BOOLEAN LYCommentHacks ARGS2(
	HTParentAnchor *,	anchor,
	CONST char *,		comment)
{
    CONST char *cp = comment;
    size_t len;

    if (comment == NULL)
	return FALSE;

    if (!(anchor && anchor->address))
	return FALSE;

    if (strncmp(comment, "!--X-Message-Id: ", 17) == 0) {
	char *messageid = NULL;
	char *p;
	for (cp = comment+17; *cp; cp++) {
	    if ((unsigned char)*cp >= 127 || !isgraph((unsigned char)*cp)) {
		break;
	    }
	}
	if (strcmp(cp, " --")) {
	    return FALSE;
	}
	cp = comment + 17;
	StrAllocCopy(messageid, cp);
	/* This should be ok - message-id should only contain 7-bit ASCII */
	if (!LYUCFullyTranslateString(&messageid, 0, 0, NO, NO, YES, st_URL))
	    return FALSE;
	for (p = messageid; *p; p++) {
	    if ((unsigned char)*p >= 127 || !isgraph((unsigned char)*p)) {
		break;
	    }
	}
	if (strcmp(p, " --")) {
	    FREE(messageid);
	    return FALSE;
	}
	if ((p = strchr(messageid, '@')) == NULL || p[1] == '\0') {
	    FREE(messageid);
	    return FALSE;
	}
	p = messageid;
	if ((len = strlen(p)) >= 8 && !strcmp(&p[len-3], " --")) {
	    p[len-3] = '\0';
	} else {
	    FREE(messageid);
	    return FALSE;
	}
	if (HTAnchor_setMessageID(anchor, messageid)) {
	    FREE(messageid);
	    return TRUE;
	} else {
	    FREE(messageid);
	    return FALSE;
	}
    }
    if (strncmp(comment, "!--X-Subject: ", 14) == 0) {
	char *subject = NULL;
	char *p;
	for (cp = comment+14; *cp; cp++) {
	    if ((unsigned char)*cp >= 127 || !isprint((unsigned char)*cp)) {
		return FALSE;
	    }
	}
	cp = comment + 14;
	StrAllocCopy(subject, cp);
	/* @@@
	 * This may not be the right thing for the subject - but mail
	 * subjects shouldn't contain 8-bit characters in raw form anyway.
	 * We have to unescape character entities, since that's what MHonArc
	 * seems to generate.  But if after that there are 8-bit characters
	 * the string is rejected.  We would probably not know correctly
	 * what charset to assume anyway - the mail sender's can differ from
	 * the archive's.  And the code for sending mail cannot deal well
	 * with 8-bit characters - we should not put them in the Subject
	 * header in raw form, but don't have MIME encoding implemented.
	 * Someone may want to do more about this...  - kw
	 */
	if (!LYUCFullyTranslateString(&subject, 0, 0, NO, YES, NO, st_HTML))
	    return FALSE;
	for (p = subject; *p; p++) {
	    if ((unsigned char)*p >= 127 || !isprint((unsigned char)*p)) {
		FREE(subject);
		return FALSE;
	    }
	}
	p = subject;
	if ((len = strlen(p)) >= 4 && !strcmp(&p[len-3], " --")) {
	    p[len-3] = '\0';
	} else {
	    FREE(subject);
	    return FALSE;
	}
	if (HTAnchor_setSubject(anchor, subject)) {
	    FREE(subject);
	    return TRUE;
	} else {
	    FREE(subject);
	    return FALSE;
	}
    }

    return FALSE;
}
