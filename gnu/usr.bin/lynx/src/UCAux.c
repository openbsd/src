#include <HTUtils.h>

#include <HTCJK.h>
#include <UCMap.h>
#include <UCDefs.h>
#include <HTStream.h>
#include <UCAux.h>
#include <LYCharSets.h>

PUBLIC BOOL UCCanUniTranslateFrom ARGS1(
	int,		from)
{
    if (from < 0)
	return NO;
    if (LYCharSet_UC[from].enc == UCT_ENC_CJK)
	return NO;
    if (!strcmp(LYCharSet_UC[from].MIMEname, "x-transparent"))
	return NO;

    /* others YES */
    return YES;
}

PUBLIC BOOL UCCanTranslateUniTo ARGS1(
	int,		to)
{
    if (to < 0)
	return NO;
/*???
    if (!strcmp(LYCharSet_UC[to].MIMEname, "x-transparent"))
       return NO;
*/

    return YES;			/* well at least some characters... */
}

PUBLIC BOOL UCCanTranslateFromTo ARGS2(
	int,		from,
	int,		to)
{
    if (from == to)
	return YES;
    if (from < 0 || to < 0)
	return NO;
    if (from == LATIN1)
	return UCCanTranslateUniTo(to);
    if (to == LATIN1 || LYCharSet_UC[to].enc == UCT_ENC_UTF8)
	return UCCanUniTranslateFrom(from);
    {
	CONST char * fromname = LYCharSet_UC[from].MIMEname;
	CONST char * toname = LYCharSet_UC[to].MIMEname;
	if (!strcmp(fromname, "x-transparent") ||
	    !strcmp(toname, "x-transparent")) {
	    return YES; /* ??? */
	} else if (!strcmp(fromname, "us-ascii")) {
	    return YES;
	}
	if (LYCharSet_UC[from].enc == UCT_ENC_CJK) {
	    /*
	    **  CJK mode may be off (i.e., HTCJK == NOCJK) because
	    **  the current document is not CJK, but the check may
	    **  be for capability in relation to another document,
	    **  for which CJK mode might be turned on when retrieved.
	    **  Thus, when the from charset is CJK, check if the to
	    **  charset is CJK, and return NO or YES in relation to
	    **  that. - FM
	    */
	    if (LYCharSet_UC[to].enc != UCT_ENC_CJK)
		return NO;
	    if ((!strcmp(toname, "euc-jp") ||
		 !strcmp(toname, "shift_jis")) &&
		(!strcmp(fromname, "euc-jp") ||
		 !strcmp(fromname, "shift_jis")))
		return YES;
	    /*
	    **  The euc-cn and euc-kr charsets were handled
	    **  by the (from == to) above, so we need not
	    **  check those. - FM
	    **/
	    return NO;
	}
    }
    return YES;    /* others YES */
}

/*
**  Returns YES if no translation necessary (because
**  charsets are equal, are equivalent, etc.).
*/
PUBLIC BOOL UCNeedNotTranslate ARGS2(
	int,		from,
	int,		to)
{
    CONST char *fromname;
    CONST char *toname;
    if (from == to)
	return YES;
    if (from < 0)
	return NO;		/* ??? */
    if (LYCharSet_UC[from].enc == UCT_ENC_7BIT) {
	return YES;		/* Only 7bit chars. */
    }
    fromname = LYCharSet_UC[from].MIMEname;
    if (!strcmp(fromname, "x-transparent") ||
	!strcmp(fromname, "us-ascii")) {
	    return YES;
    }
    if (to < 0)
	return NO;		/* ??? */
    if (to == LATIN1) {
	if (LYCharSet_UC[from].codepoints & (UCT_CP_SUBSETOF_LAT1))
	    return YES;
    }
    toname = LYCharSet_UC[to].MIMEname;
    if (!strcmp(toname, "x-transparent")) {
	return YES;
    }
    if (LYCharSet_UC[to].enc == UCT_ENC_UTF8) {
	return NO;
    }
    if (from == LATIN1) {
	if (LYCharSet_UC[from].codepoints & (UCT_CP_SUPERSETOF_LAT1))
	    return YES;
    }
    if (LYCharSet_UC[from].enc == UCT_ENC_CJK) {
	if (HTCJK == NOCJK)	/* Use that global flag, for now. */
	    return NO;
	if (HTCJK == JAPANESE &&
	    (!strcmp(fromname, "euc-jp") ||
	     !strcmp(fromname, "shift_jis")))
	    return YES;	/* translate internally by lynx, no unicode */
	return NO;	/* If not handled by (from == to) above. */
    }
    return NO;
}

/*
**  The idea here is that any stage of the stream pipe which is interested
**  in some charset dependent processing will call this function.
**  Given input and output charsets, this function will set various flags
**  in a UCTransParams structure that _suggest_ to the caller what to do.
**
**  Should be called once when a stage starts processing text (and the
**  input and output charsets are known), or whenever one of input or
**  output charsets has changed (e.g., by SGML.c stage after HTML.c stage
**  has processed a META tag).
**  The global flags (LYRawMode, HTPassEightBitRaw etc.) are currently
**  not taken into account here (except for HTCJK, somewhat), it's still
**  up to the caller to do something about them. - KW
*/
PUBLIC void UCSetTransParams ARGS5(
    UCTransParams *,	pT,
    int,		cs_in,
    CONST LYUCcharset*,	p_in,
    int,		cs_out,
    CONST LYUCcharset*,	p_out)
{
    CTRACE((tfp, "UCSetTransParams: from %s(%d) to %s(%d)\n",
	   p_in->MIMEname,  UCGetLYhndl_byMIME(p_in->MIMEname),
	   p_out->MIMEname, UCGetLYhndl_byMIME(p_out->MIMEname)));

    /*
    **  Initialize this element to FALSE, and set it TRUE
    **  below if we're dealing with VISCII. - FM
    */
    pT->trans_C0_to_uni = FALSE;

    /*
    **  The "transparent" display character set is a
    **  "super raw mode". - FM
    */
    pT->transp = (BOOL) (!strcmp(p_in->MIMEname, "x-transparent") ||
		  !strcmp(p_out->MIMEname, "x-transparent"));

    if (pT->transp) {
	/*
	**  Set up the structure for "transparent". - FM
	*/
	pT->do_cjk = FALSE;
	pT->decode_utf8 = FALSE;
	pT->output_utf8 = FALSE;  /* We may, but won't know about it. - KW */
	pT->do_8bitraw = TRUE;
	pT->use_raw_char_in = TRUE;
	pT->strip_raw_char_in = FALSE;
	pT->pass_160_173_raw = TRUE;
	pT->repl_translated_C0 = (BOOL) (p_out->enc == UCT_ENC_8BIT_C0);
	pT->trans_C0_to_uni = (BOOL) (p_in->enc == UCT_ENC_8BIT_C0 ||
			       p_out->enc == UCT_ENC_8BIT_C0);
    } else {
	/*
	**  Initialize local flags. - FM
	*/
	BOOL intm_ucs = FALSE;
	BOOL use_ucs = FALSE;
	/*
	**  Set this element if we want to treat
	**  the input as CJK. - FM
	*/
	pT->do_cjk = (BOOL) ((p_in->enc == UCT_ENC_CJK) && (HTCJK != NOCJK));
	/*
	**  Set these elements based on whether
	**  we are dealing with UTF-8. - FM
	*/
	pT->decode_utf8 = (BOOL) (p_in->enc == UCT_ENC_UTF8);
	pT->output_utf8 = (BOOL) (p_out->enc == UCT_ENC_UTF8);
	if (pT->do_cjk) {
	    /*
	    **  Set up the structure for a CJK input with
	    **  a CJK output (HTCJK != NOCJK). - FM
	    */
	    intm_ucs = FALSE;
	    pT->trans_to_uni = FALSE;
	    use_ucs = FALSE;
	    pT->do_8bitraw = FALSE;
	    pT->pass_160_173_raw = TRUE;
	    pT->use_raw_char_in = FALSE; /* Not used for CJK. - KW */
	    pT->repl_translated_C0 = FALSE;
	    pT->trans_from_uni = FALSE;	 /* Not used for CJK. - KW */
	} else {
	    /*
	    **  Set up for all other charset combinations.
	    **  The intm_ucs flag is set TRUE if the input
	    **  charset is iso-8859-1 or UTF-8, or largely
	    **  equivalent to them, i.e., if we have UCS without
	    **  having to do a table translation.
	    */
	    intm_ucs = (BOOL) (cs_in == LATIN1 || pT->decode_utf8 ||
			(p_in->codepoints &
			 (UCT_CP_SUBSETOF_LAT1|UCT_CP_SUBSETOF_UCS2)));
	    /*
	    **  pT->trans_to_uni is set TRUE if we do not have that as
	    **  input already, and we can translate to Unicode.  Note
	    **  that UTF-8 always is converted to Unicode in functions
	    **  that use the transformation structure, so it is
	    **  treated as already Unicode here.
	    */
	    pT->trans_to_uni = (BOOL) (!intm_ucs &&
				UCCanUniTranslateFrom(cs_in));
	    /*
	    **  We set this if we are translating to Unicode and
	    **  what normally are low value control characters in
	    **  fact are encoding octets for the input charset
	    **  (presently, this applies to VISCII). - FM
	    */
	    pT->trans_C0_to_uni = (BOOL) (pT->trans_to_uni &&
				   p_in->enc == UCT_ENC_8BIT_C0);
	    /*
	    **  We set this, presently, for VISCII. - FM
	    */
	    pT->repl_translated_C0 = (BOOL) (p_out->enc == UCT_ENC_8BIT_C0);
	    /*
	    **  Currently unused for any charset combination.
	    **  Should always be FALSE
	    */
	    pT->strip_raw_char_in = FALSE;
	    /*
	    **  use_ucs should be set TRUE if we have or will create
	    **  Unicode values for input octets or UTF multibytes. - FM
	    */
	    use_ucs = (BOOL) (intm_ucs || pT->trans_to_uni);
	    /*
	    **  This is set TRUE if use_ucs was set FALSE.  It is
	    **  complementary to the HTPassEightBitRaw flag, which
	    **  is set TRUE or FALSE elsewhere based on the raw mode
	    **  setting in relation to the current Display Character
	    **  Set. - FM
	    */
	    pT->do_8bitraw = (BOOL) (!use_ucs);
	    /*
	    **  This is set TRUE when 160 and 173 should not be
	    **  treated as nbsp and shy, respectively. - FM
	    */
	    pT->pass_160_173_raw = (BOOL) (!use_ucs &&
				    !(p_in->like8859 & UCT_R_8859SPECL));
	    /*
	    **  This is set when the input and output charsets match,
	    **  and they are not ones which should go through a Unicode
	    **  translation process anyway. - FM
	    */
	    pT->use_raw_char_in = (BOOL) (!pT->output_utf8 &&
				   cs_in == cs_out &&
				   !pT->trans_C0_to_uni);
	    /*
	    **  This should be set TRUE when we expect to have
	    **  done translation to Unicode or had the equivalent
	    **  as input, can translate it to our output charset,
	    **  and normally want to do so.  The latter depends on
	    **  the pT->do_8bitraw and pT->use_raw_char_in values set
	    **  above, but also on HTPassEightBitRaw in any functions
	    **  which use the transformation structure.. - FM
	    */
	    pT->trans_from_uni = (BOOL) (use_ucs && !pT->do_8bitraw &&
				  !pT->use_raw_char_in &&
				  UCCanTranslateUniTo(cs_out));
	}
    }
}

/*
**  This function initializes the transformation
**  structure by setting all its elements to
**  FALSE. - KW
*/
PUBLIC void UCTransParams_clear ARGS1(
    UCTransParams *,    pT)
{
    pT->transp = FALSE;
    pT->do_cjk = FALSE;
    pT->decode_utf8 = FALSE;
    pT->output_utf8 = FALSE;
    pT->do_8bitraw = FALSE;
    pT->use_raw_char_in = FALSE;
    pT->strip_raw_char_in = FALSE;
    pT->pass_160_173_raw = FALSE;
    pT->trans_to_uni = FALSE;
    pT->trans_C0_to_uni = FALSE;
    pT->repl_translated_C0 = FALSE;
    pT->trans_from_uni = FALSE;
}

/*
**  If terminal is in UTF-8 mode, it probably cannot understand
**  box drawing chars as (n)curses handles them.  (This may also
**  be true for other display character sets, but isn't currently
**  checked.)  In that case set the chars for hori and vert drawing
**  chars to displayable ASCII chars if '0' was requested.  They'll
**  stay as they are otherwise. - kw
*/
PUBLIC void UCSetBoxChars ARGS5(
    int,	cset,
    int *,	pvert_out,
    int *,	phori_out,
    int,	vert_in,
    int,	hori_in)
{
    if (cset >= -1 && LYCharSet_UC[cset].enc == UCT_ENC_UTF8) {
	*pvert_out = (vert_in ? vert_in : '|');
	*phori_out = (hori_in ? hori_in : '-');
    } else {
	*pvert_out = vert_in;
	*phori_out = hori_in;
    }
}

/*
**  Given an output target HTStream* (can also be a HTStructured* via
**  typecast), the target stream's put_character method, and a Unicode
**  character,  CPutUtf8_charstring() will either output the UTF8
**  encoding of the Unicode and return YES, or do nothing and return
**  NO (if conversion would be unnecessary or the Unicode character is
**  considered invalid).
**
**  [Could be used more generally, but is currently only used for &#nnnnn
**  stuff - generation of UTF8 from 8-bit encoded charsets not yet done
**  by SGML.c etc.]
*/
#define PUTC(ch) ((*myPutc)(target, (char)(ch)))
#define PUTC2(ch) ((*myPutc)(target,(char)(0x80|(0x3f &(ch)))))

PUBLIC BOOL UCPutUtf8_charstring ARGS3(
	HTStream *,	target,
	putc_func_t *,	myPutc,
	long,		code)
{
    if (code < 128)
	return NO;		/* indicate to caller we didn't handle it */
    else if   (code < 0x800L) {
	PUTC(0xc0 | (code>>6));
	PUTC2(code);
    } else if (code < 0x10000L) {
	PUTC(0xe0 | (code>>12));
	PUTC2(code>>6);
	PUTC2(code);
    } else if (code < 0x200000L) {
	PUTC(0xf0 | (code>>18));
	PUTC2(code>>12);
	PUTC2(code>>6);
	PUTC2(code);
    } else if (code < 0x4000000L) {
	PUTC(0xf8 | (code>>24));
	PUTC2(code>>18);
	PUTC2(code>>12);
	PUTC2(code>>6);
	PUTC2(code);
    } else if (code <= 0x7fffffffL) {
	PUTC(0xfc | (code>>30));
	PUTC2(code>>24);
	PUTC2(code>>18);
	PUTC2(code>>12);
	PUTC2(code>>6);
	PUTC2(code);
    } else
	return NO;
    return YES;
}

/*
**  This function converts a Unicode (UCode_t) value
**  to a multibyte UTF-8 character, which is loaded
**  into the buffer received as an argument.  The
**  buffer should be large enough to hold at least
**  seven characters (but should be declared as 8
**  to minimize byte alignment problems with some
**  compilers). - FM
*/
PUBLIC BOOL UCConvertUniToUtf8 ARGS2(
	UCode_t,	code,
	char *,		buffer)
{
    char *ch = buffer;

    if (!ch)
	return NO;

    if (code <= 0 || code > 0x7fffffffL) {
	*ch = '\0';
	return NO;
    }

    if (code < 0x800L) {
	*ch++ = (char)(0xc0 | (code>>6));
	*ch++ = (char)(0x80 | (0x3f & (code)));
	*ch = '\0';
    } else if (code < 0x10000L) {
	*ch++ = (char)(0xe0 | (code>>12));
	*ch++ = (char)(0x80 | (0x3f & (code>>6)));
	*ch++ = (char)(0x80 | (0x3f & (code)));
	*ch = '\0';
    } else if (code < 0x200000L) {
	*ch++ = (char)(0xf0 | (code>>18));
	*ch++ = (char)(0x80 | (0x3f & (code>>12)));
	*ch++ = (char)(0x80 | (0x3f & (code>>6)));
	*ch++ = (char)(0x80 | (0x3f & (code)));
	*ch = '\0';
    } else if (code < 0x4000000L) {
	*ch++ = (char)(0xf8 | (code>>24));
	*ch++ = (char)(0x80 | (0x3f & (code>>18)));
	*ch++ = (char)(0x80 | (0x3f & (code>>12)));
	*ch++ = (char)(0x80 | (0x3f & (code>>6)));
	*ch++ = (char)(0x80 | (0x3f & (code)));
	*ch = '\0';
    } else {
	*ch++ = (char)(0xfc | (code>>30));
	*ch++ = (char)(0x80 | (0x3f & (code>>24)));
	*ch++ = (char)(0x80 | (0x3f & (code>>18)));
	*ch++ = (char)(0x80 | (0x3f & (code>>12)));
	*ch++ = (char)(0x80 | (0x3f & (code>>6)));
	*ch++ = (char)(0x80 | (0x3f & (code)));
	*ch = '\0';
    }
    return YES;
}

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
PUBLIC UCode_t UCGetUniFromUtf8String ARGS1(char **, ppuni)
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
