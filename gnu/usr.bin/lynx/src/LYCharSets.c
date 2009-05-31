#include <HTUtils.h>
#include <HTCJK.h>
#include <HTMLDTD.h>

#include <LYGlobalDefs.h>
#include <UCMap.h>
#include <UCdomap.h>
#include <UCDefs.h>
#include <LYCharSets.h>
#include <GridText.h>
#include <LYCurses.h>
#include <LYStrings.h>

#include <LYLeaks.h>

HTkcode kanji_code = NOKANJI;
BOOLEAN LYHaveCJKCharacterSet = FALSE;
BOOLEAN DisplayCharsetMatchLocale = TRUE;
BOOL force_old_UCLYhndl_on_reload = FALSE;
int forced_UCLYhdnl;
int LYNumCharsets = 0;		/* Will be initialized later by UC_Register. */
int current_char_set = -1;	/* will be intitialized later in LYMain.c */
int linedrawing_char_set = -1;
const char **p_entity_values = NULL;	/* Pointer, for HTML_put_entity() */

			      /* obsolete and probably not used(???)        */
			      /* will be initialized in HTMLUseCharacterSet */
#ifdef EXP_CHARSET_CHOICE
charset_subset_t charset_subsets[MAXCHARSETS];
BOOL custom_display_charset = FALSE;
BOOL custom_assumed_doc_charset = FALSE;

#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN
int display_charset_map[MAXCHARSETS];
int assumed_doc_charset_map[MAXCHARSETS];

const char *display_charset_choices[MAXCHARSETS + 1];
const char *assumed_charset_choices[MAXCHARSETS + 1];
int displayed_display_charset_idx;
#endif
#endif /* EXP_CHARSET_CHOICE */

/*
 * New character sets now declared with UCInit() in UCdomap.c
 *
 * INSTRUCTIONS for adding new character sets which do not have
 *		Unicode tables now in UCdomap.h
 *
 *
 * [We hope you need not correct/add old-style mapping below as in ISO_LATIN1[]
 * or SevenBitApproximations[] any more - it works now via new chartrans
 * mechanism, but kept for compatibility only:  we should cleanup the stuff,
 * but this is not so easy...]
 *
 * Currently we only declare some charset's properties here (such as MIME
 * names, etc.), it does not include real mapping.
 *
 * There is a place marked "Add your new character sets HERE" in this file. 
 * Make up a character set and add it in the same style as the ISO_LATIN1 set
 * below, giving it a unique name.
 *
 * Add the name of the set to LYCharSets.  Similarly add the appropriate
 * information to the tables below:  LYchar_set_names, LYCharSet_UC,
 * LYlowest_eightbit.  These 4 tables all MUST have the same order.  (And this
 * is the order you will see in Lynx Options Menu, which is why few
 * unicode-based charsets are listed here).
 *
 */

/*	Entity values -- for ISO Latin 1 local representation
 *
 *	This MUST match exactly the table referred to in the DTD!
 */
static const char *ISO_Latin1[] =
{
    "\306",			/* capital AE diphthong (ligature) (&#198;) - AElig */
    "\301",			/* capital A, acute accent (&#193;) - Aacute */
    "\302",			/* capital A, circumflex accent (&#194;) - Acirc */
    "\300",			/* capital A, grave accent (&#192;) - Agrave */
    "\305",			/* capital A, ring - Aring (&#197;) */
    "\303",			/* capital A, tilde - Atilde (&#195;) */
    "\304",			/* capital A, dieresis or umlaut mark (&#196;) - Auml */
    "\307",			/* capital C, cedilla - Ccedil (&#199;) */
    "\320",			/* capital Eth or D with stroke (&#208;) - Dstrok */
    "\320",			/* capital Eth, Icelandic (&#208;) - ETH */
    "\311",			/* capital E, acute accent (&#201;) - Eacute */
    "\312",			/* capital E, circumflex accent (&#202;) - Ecirc */
    "\310",			/* capital E, grave accent (&#200;) - Egrave */
    "\313",			/* capital E, dieresis or umlaut mark (&#203;) - Euml */
    "\315",			/* capital I, acute accent (&#205;) - Iacute */
    "\316",			/* capital I, circumflex accent (&#206;) - Icirc */
    "\314",			/* capital I, grave accent (&#204;) - Igrave */
    "\317",			/* capital I, dieresis or umlaut mark (&#207;) - Iuml */
    "\321",			/* capital N, tilde (&#209;) - Ntilde */
    "\323",			/* capital O, acute accent (&#211;) - Oacute */
    "\324",			/* capital O, circumflex accent (&#212;) - Ocirc */
    "\322",			/* capital O, grave accent (&#210;) - Ograve */
    "\330",			/* capital O, slash (&#216;) - Oslash */
    "\325",			/* capital O, tilde (&#213;) - Otilde */
    "\326",			/* capital O, dieresis or umlaut mark (&#214;) - Ouml */
    "\336",			/* capital THORN, Icelandic (&#222;) - THORN */
    "\332",			/* capital U, acute accent (&#218;) - Uacute */
    "\333",			/* capital U, circumflex accent (&#219;) - Ucirc */
    "\331",			/* capital U, grave accent (&#217;) - Ugrave */
    "\334",			/* capital U, dieresis or umlaut mark (&#220;) - Uuml */
    "\335",			/* capital Y, acute accent (&#221;) - Yacute */
    "\341",			/* small a, acute accent (&#225;) - aacute */
    "\342",			/* small a, circumflex accent (&#226;) - acirc */
    "\264",			/* spacing acute (&#180;) - acute */
    "\346",			/* small ae diphthong (ligature) (&#230;) - aelig */
    "\340",			/* small a, grave accent (&#224;) - agrave */
    "\046",			/* ampersand (&#38;) - amp */
    "\345",			/* small a, ring (&#229;) - aring */
    "\343",			/* small a, tilde (&#227;) - atilde */
    "\344",			/* small a, dieresis or umlaut mark (&#228;) - auml */
    "\246",			/* broken vertical bar (&#166;) - brkbar */
    "\246",			/* broken vertical bar (&#166;) - brvbar */
    "\347",			/* small c, cedilla (&#231;) - ccedil */
    "\270",			/* spacing cedilla (&#184;) - cedil */
    "\242",			/* cent sign (&#162;) - cent */
    "\251",			/* copyright sign (&#169;) - copy */
    "\244",			/* currency sign (&#164;) - curren */
    "\260",			/* degree sign (&#176;) - deg */
    "\250",			/* spacing dieresis (&#168;) - die */
    "\367",			/* division sign (&#247;) - divide */
    "\351",			/* small e, acute accent (&#233;) - eacute */
    "\352",			/* small e, circumflex accent (&#234;) - ecirc */
    "\350",			/* small e, grave accent (&#232;) - egrave */
    "-",			/* dash the width of emsp - emdash */
    "\002",			/* emsp, em space - not collapsed NEVER CHANGE THIS - emsp */
    "-",			/* dash the width of ensp - endash */
    "\002",			/* ensp, en space - not collapsed NEVER CHANGE THIS - ensp */
    "\360",			/* small eth, Icelandic (&#240;) - eth */
    "\353",			/* small e, dieresis or umlaut mark (&#235;) - euml */
    "\275",			/* fraction 1/2 (&#189;) - frac12 */
    "\274",			/* fraction 1/4 (&#188;) - frac14 */
    "\276",			/* fraction 3/4 (&#190;) - frac34 */
    "\076",			/* greater than (&#62;) - gt */
    "\257",			/* spacing macron (&#175;) - hibar */
    "\355",			/* small i, acute accent (&#237;) - iacute */
    "\356",			/* small i, circumflex accent (&#238;) - icirc */
    "\241",			/* inverted exclamation mark (&#161;) - iexcl */
    "\354",			/* small i, grave accent (&#236;) - igrave */
    "\277",			/* inverted question mark (&#191;) - iquest */
    "\357",			/* small i, dieresis or umlaut mark (&#239;) - iuml */
    "\253",			/* angle quotation mark, left (&#171;) - laquo */
    "\074",			/* less than (&#60;) - lt */
    "\257",			/* spacing macron (&#175;) - macr */
    "-",			/* dash the width of emsp - mdash */
    "\265",			/* micro sign (&#181;) - micro */
    "\267",			/* middle dot (&#183;) - middot */
    "\001",			/* nbsp non-breaking space NEVER CHANGE THIS - nbsp */
    "-",			/* dash the width of ensp - ndash */
    "\254",			/* negation sign (&#172;) - not */
    "\361",			/* small n, tilde (&#241;) - ntilde */
    "\363",			/* small o, acute accent (&#243;) - oacute */
    "\364",			/* small o, circumflex accent (&#244;) - ocirc */
    "\362",			/* small o, grave accent (&#242;) - ograve */
    "\252",			/* feminine ordinal indicator (&#170;) - ordf */
    "\272",			/* masculine ordinal indicator (&#186;) - ordm */
    "\370",			/* small o, slash (&#248;) - oslash */
    "\365",			/* small o, tilde (&#245;) - otilde */
    "\366",			/* small o, dieresis or umlaut mark (&#246;) - ouml */
    "\266",			/* paragraph sign (&#182;) - para */
    "\261",			/* plus-or-minus sign (&#177;) - plusmn */
    "\243",			/* pound sign (&#163;) - pound */
    "\042",			/* quote '"' (&#34;) - quot */
    "\273",			/* angle quotation mark, right (&#187;) - raquo */
    "\256",			/* circled R registered sign (&#174;) - reg */
    "\247",			/* section sign (&#167;) - sect */
    "\007",			/* soft hyphen (&#173;) NEVER CHANGE THIS - shy */
    "\271",			/* superscript 1 (&#185;) - sup1 */
    "\262",			/* superscript 2 (&#178;) - sup2 */
    "\263",			/* superscript 3 (&#179;) - sup3 */
    "\337",			/* small sharp s, German (sz ligature) (&#223;) - szlig */
    "\002",			/* thin space - not collapsed NEVER CHANGE THIS - thinsp */
    "\376",			/* small thorn, Icelandic (&#254;) - thorn */
    "\327",			/* multiplication sign (&#215;) - times */
    "(TM)",			/* circled TM trade mark sign (&#8482;) - trade */
    "\372",			/* small u, acute accent (&#250;) - uacute */
    "\373",			/* small u, circumflex accent (&#251;) - ucirc */
    "\371",			/* small u, grave accent (&#249;) - ugrave */
    "\250",			/* spacing dieresis (&#168;) - uml */
    "\374",			/* small u, dieresis or umlaut mark (&#252;) - uuml */
    "\375",			/* small y, acute accent (&#253;) - yacute */
    "\245",			/* yen sign (&#165;) - yen */
    "\377",			/* small y, dieresis or umlaut mark (&#255;) - yuml */
};

/*	Entity values -- 7 bit character approximations
 *
 *	This MUST match exactly the table referred to in the DTD!
 */
const char *SevenBitApproximations[] =
{
    "AE",			/* capital AE diphthong (ligature) (&#198;) - AElig */
    "A",			/* capital A, acute accent (&#193;) - Aacute */
    "A",			/* capital A, circumflex accent (&#194;) - Acirc */
    "A",			/* capital A, grave accent (&#192;) - Agrave */
    "A",			/* capital A, ring - Aring (&#197;) */
    "A",			/* capital A, tilde - Atilde (&#195;) */
#ifdef LY_UMLAUT
    "Ae",			/* capital A, dieresis or umlaut mark (&#196;) - Auml */
#else
    "A",			/* capital A, dieresis or umlaut mark (&#196;) - Auml */
#endif				/* LY_UMLAUT */
    "C",			/* capital C, cedilla (&#199;) - Ccedil */
    "Dj",			/* capital D with stroke (&#208;) - Dstrok */
    "DH",			/* capital Eth, Icelandic (&#208;) - ETH */
    "E",			/* capital E, acute accent (&#201;) - Eacute */
    "E",			/* capital E, circumflex accent (&#202;) - Ecirc */
    "E",			/* capital E, grave accent (&#200;) - Egrave */
    "E",			/* capital E, dieresis or umlaut mark (&#203;) - Euml */
    "I",			/* capital I, acute accent (&#205;) - Iacute */
    "I",			/* capital I, circumflex accent (&#206;) - Icirc */
    "I",			/* capital I, grave accent (&#204;) - Igrave */
    "I",			/* capital I, dieresis or umlaut mark (&#207;) - Iuml */
    "N",			/* capital N, tilde - Ntilde (&#209;) */
    "O",			/* capital O, acute accent (&#211;) - Oacute */
    "O",			/* capital O, circumflex accent (&#212;) - Ocirc */
    "O",			/* capital O, grave accent (&#210;) - Ograve */
    "O",			/* capital O, slash (&#216;) - Oslash */
    "O",			/* capital O, tilde (&#213;) - Otilde */
#ifdef LY_UMLAUT
    "Oe",			/* capital O, dieresis or umlaut mark (&#214;) - Ouml */
#else
    "O",			/* capital O, dieresis or umlaut mark (&#214;) - Ouml */
#endif				/* LY_UMLAUT */
    "P",			/* capital THORN, Icelandic (&#222;) - THORN */
    "U",			/* capital U, acute accent (&#218;) - Uacute */
    "U",			/* capital U, circumflex accent (&#219;) - Ucirc */
    "U",			/* capital U, grave accent (&#217;) - Ugrave */
#ifdef LY_UMLAUT
    "Ue",			/* capital U, dieresis or umlaut mark (&#220;) - Uuml */
#else
    "U",			/* capital U, dieresis or umlaut mark (&#220;) - Uuml */
#endif				/* LY_UMLAUT */
    "Y",			/* capital Y, acute accent (&#221;) - Yacute */
    "a",			/* small a, acute accent (&#225;) - aacute */
    "a",			/* small a, circumflex accent (&#226;) - acirc */
    "'",			/* spacing acute (&#180;) - acute */
    "ae",			/* small ae diphthong (ligature) (&#230;) - aelig */
    "`a",			/* small a, grave accent (&#232;) - agrave */
    "&",			/* ampersand (&#38;) - amp */
    "a",			/* small a, ring (&#229;) - aring */
    "a",			/* small a, tilde (&#227;) - atilde */
#ifdef LY_UMLAUT
    "ae",			/* small a, dieresis or umlaut mark (&#228;) - auml */
#else
    "a",			/* small a, dieresis or umlaut mark (&#228;) - auml */
#endif				/* LY_UMLAUT */
    "|",			/* broken vertical bar (&#166;) - brkbar */
    "|",			/* broken vertical bar (&#166;) - brvbar */
    "c",			/* small c, cedilla (&#231;) - ccedil */
    ",",			/* spacing cedilla (&#184;) - cedil */
    "-c-",			/* cent sign (&#162;) - cent */
    "(c)",			/* copyright sign (&#169;) - copy */
    "CUR",			/* currency sign (&#164;) - curren */
    "DEG",			/* degree sign (&#176;) - deg */
    "\042",			/* spacing dieresis (&#168;) - die */
    "/",			/* division sign (&#247;) - divide */
    "e",			/* small e, acute accent (&#233;) - eacute */
    "e",			/* small e, circumflex accent (&#234;) - ecirc */
    "e",			/* small e, grave accent (&#232;) - egrave */
    "-",			/* dash the width of emsp - emdash */
    "\002",			/* emsp NEVER CHANGE THIS - emsp */
    "-",			/* dash the width of ensp - endash */
    "\002",			/* ensp NEVER CHANGE THIS - ensp */
    "dh",			/* small eth, Icelandic eth (&#240;) */
    "e",			/* small e, dieresis or umlaut mark (&#235;) - euml */
    " 1/2",			/* fraction 1/2 (&#189;) - frac12 */
    " 1/4",			/* fraction 1/4 (&#188;) - frac14 */
    " 3/4",			/* fraction 3/4 (&#190;) - frac34 */
    ">",			/* greater than (&#62;) - gt */
    "-",			/* spacing macron (&#175;) - hibar */
    "i",			/* small i, acute accent (&#237;) - iacute */
    "i",			/* small i, circumflex accent (&#238;) - icirc */
    "!",			/* inverted exclamation mark (&#161;) - iexcl */
    "`i",			/* small i, grave accent (&#236;) - igrave */
    "?",			/* inverted question mark (&#191;) - iquest */
    "i",			/* small i, dieresis or umlaut mark (&#239;) - iuml */
    "<<",			/* angle quotation mark, left (&#171;) - laquo */
    "<",			/* less than - lt (&#60;) */
    "-",			/* spacing macron (&#175;) - macr */
    "-",			/* dash the width of emsp - mdash */
    "u",			/* micro sign (&#181;) - micro */
    ".",			/* middle dot (&#183;) - middot */
    "\001",			/* nbsp non-breaking space NEVER CHANGE THIS - nbsp */
    "-",			/* dash the width of ensp - ndash */
    "NOT",			/* negation sign (&#172;) - not */
    "n",			/* small n, tilde (&#241;) - ntilde */
    "o",			/* small o, acute accent (&#243;) - oacute */
    "o",			/* small o, circumflex accent (&#244;) - ocirc */
    "o",			/* small o, grave accent (&#242;) - ograve */
    "-a",			/* feminine ordinal indicator (&#170;) - ordf */
    "-o",			/* masculine ordinal indicator (&#186;) - ordm */
    "o",			/* small o, slash (&#248;) - oslash */
    "o",			/* small o, tilde (&#245;) - otilde */
#ifdef LY_UMLAUT
    "oe",			/* small o, dieresis or umlaut mark (&#246;) - ouml */
#else
    "o",			/* small o, dieresis or umlaut mark (&#246;) - ouml */
#endif				/* LY_UMLAUT */
    "P:",			/* paragraph sign (&#182;) - para */
    "+-",			/* plus-or-minus sign (&#177;) - plusmn */
    "-L-",			/* pound sign (&#163;) - pound */
    "\"",			/* quote '"' (&#34;) - quot */
    ">>",			/* angle quotation mark, right (&#187;) - raquo */
    "(R)",			/* circled R registered sign (&#174;) - reg */
    "S:",			/* section sign (&#167;) - sect */
    "\007",			/* soft hyphen (&#173;) NEVER CHANGE THIS - shy */
    "^1",			/* superscript 1 (&#185;) - sup1 */
    "^2",			/* superscript 2 (&#178;) - sup2 */
    "^3",			/* superscript 3 (&#179;) - sup3 */
    "ss",			/* small sharp s, German (sz ligature) (&#223;) - szlig */
    "\002",			/* thin space - not collapsed NEVER CHANGE THIS - thinsp */
    "p",			/* small thorn, Icelandic (&#254;) - thorn */
    "*",			/* multiplication sign (&#215;) - times */
    "(TM)",			/* circled TM trade mark sign (&#8482;) - trade */
    "u",			/* small u, acute accent (&#250;) - uacute */
    "u",			/* small u, circumflex accent (&#251;) - ucirc */
    "u",			/* small u, grave accent (&#249;) - ugrave */
    "\042",			/* spacing dieresis (&#168;) - uml */
#ifdef LY_UMLAUT
    "ue",			/* small u, dieresis or umlaut mark (&#252;) - uuml */
#else
    "u",			/* small u, dieresis or umlaut mark (&#252;) - uuml */
#endif				/* LY_UMLAUT */
    "y",			/* small y, acute accent (&#253;) - yacute */
    "YEN",			/* yen sign (&#165;) - yen */
    "y",			/* small y, dieresis or umlaut mark (&#255;) - yuml */
};

/*
 * Add your new character sets HERE (but only if you can't construct Unicode
 * tables for them).  - FM
 */

/*
 * Add the array name to LYCharSets
 */
const char **LYCharSets[MAXCHARSETS] =
{
    ISO_Latin1,			/* ISO Latin 1          */
    SevenBitApproximations,	/* 7 Bit Approximations */
};

/*
 * Add the name that the user will see below.  The order of LYCharSets and
 * LYchar_set_names MUST be the same
 */
const char *LYchar_set_names[MAXCHARSETS + 1] =
{
    "Western (ISO-8859-1)",
    "7 bit approximations (US-ASCII)",
    (char *) 0
};

/*
 * Associate additional pieces of info with each of the charsets listed above. 
 * Will be automatically modified (and extended) by charset translations which
 * are loaded using the chartrans mechanism.  Most important piece of info to
 * put here is a MIME charset name.  Used for chartrans (see UCDefs.h).  The
 * order of LYCharSets and LYCharSet_UC MUST be the same.
 *
 * Note that most of the charsets added by the new mechanism in src/chrtrans
 * don't show up here at all.  They don't have to.
 */
LYUCcharset LYCharSet_UC[MAXCHARSETS] =
{
  /*
   * Zero position placeholder and HTMLGetEntityUCValue() reference.  - FM
   */
    {-1, "iso-8859-1", UCT_ENC_8BIT, 0,
     UCT_REP_IS_LAT1,
     UCT_CP_IS_LAT1, UCT_R_LAT1, UCT_R_LAT1},

  /*
   * Placeholders for Unicode tables.  - FM
   */
    {-1, "us-ascii", UCT_ENC_7BIT, 0,
     UCT_REP_SUBSETOF_LAT1,
     UCT_CP_SUBSETOF_LAT1, UCT_R_ASCII, UCT_R_ASCII},

};

/*
 * Add the code of the the lowest character with the high bit set that can be
 * directly displayed.  The order of LYCharSets and LYlowest_eightbit MUST be
 * the same.
 *
 * (If charset have chartrans unicode table, LYlowest_eightbit will be
 * verified/modified anyway.)
 */
int LYlowest_eightbit[MAXCHARSETS] =
{
    160,			/* ISO Latin 1          */
    999,			/* 7 bit approximations */
};

/*
 * Function to set the handling of selected character sets based on the current
 * LYUseDefaultRawMode value.  - FM
 */
void HTMLSetCharacterHandling(int i)
{
    int chndl = safeUCGetLYhndl_byMIME(UCAssume_MIMEcharset);
    BOOLEAN LYRawMode_flag = LYRawMode;
    int UCLYhndl_for_unspec_flag = UCLYhndl_for_unspec;

    if (LYCharSet_UC[i].enc != UCT_ENC_CJK) {
	HTCJK = NOCJK;
	kanji_code = NOKANJI;
	if (i == chndl)
	    LYRawMode = LYUseDefaultRawMode;
	else
	    LYRawMode = (BOOL) (!LYUseDefaultRawMode);

	HTPassEightBitNum = (BOOL) (
				       ((LYCharSet_UC[i].codepoints & UCT_CP_SUPERSETOF_LAT1)
					|| (LYCharSet_UC[i].like8859 & UCT_R_HIGH8BIT)));

	if (LYRawMode) {
	    HTPassEightBitRaw = (BOOL) (LYlowest_eightbit[i] <= 160);
	} else {
	    HTPassEightBitRaw = FALSE;
	}
	if (LYRawMode || i == chndl) {
	    HTPassHighCtrlRaw = (BOOL) (LYlowest_eightbit[i] <= 130);
	} else {
	    HTPassHighCtrlRaw = FALSE;
	}

	HTPassHighCtrlNum = FALSE;

    } else {			/* CJK encoding: */
	const char *mime = LYCharSet_UC[i].MIMEname;

	if (!strcmp(mime, "euc-cn")) {
	    HTCJK = CHINESE;
	    kanji_code = EUC;
	} else if (!strcmp(mime, "euc-jp")) {
	    HTCJK = JAPANESE;
	    kanji_code = EUC;
	} else if (!strcmp(mime, "shift_jis")) {
	    HTCJK = JAPANESE;
	    kanji_code = SJIS;
	} else if (!strcmp(mime, "euc-kr")) {
	    HTCJK = KOREAN;
	    kanji_code = EUC;
	} else if (!strcmp(mime, "big5")) {
	    HTCJK = TAIPEI;
	    kanji_code = EUC;
	}

	/* for any CJK: */
	if (!LYUseDefaultRawMode)
	    HTCJK = NOCJK;
	LYRawMode = (BOOL) ((HTCJK != NOCJK) ? TRUE : FALSE);
	HTPassEightBitRaw = FALSE;
	HTPassEightBitNum = FALSE;
	HTPassHighCtrlRaw = (BOOL) ((HTCJK != NOCJK) ? TRUE : FALSE);
	HTPassHighCtrlNum = FALSE;
    }

    /*
     * Comment for coding below:
     * UCLYhndl_for_unspec is "current" state with LYRawMode, but
     * UCAssume_MIMEcharset is independent from LYRawMode:  holds the history
     * and may be changed from 'O'ptions menu only.  - LP
     */
    if (LYRawMode) {
	UCLYhndl_for_unspec = i;	/* UCAssume_MIMEcharset not changed! */
    } else {
	if (chndl != i &&
	    (LYCharSet_UC[i].enc != UCT_ENC_CJK ||
	     LYCharSet_UC[chndl].enc != UCT_ENC_CJK)) {
	    UCLYhndl_for_unspec = chndl;	/* fall to UCAssume_MIMEcharset */
	} else {
	    UCLYhndl_for_unspec = LATIN1;	/* UCAssume_MIMEcharset not changed! */
	}
    }

#ifdef USE_SLANG
    if (LYlowest_eightbit[i] > 191) {
	/*
	 * Higher than this may output cntrl chars to screen.  - KW
	 */
	SLsmg_Display_Eight_Bit = 191;
    } else {
	SLsmg_Display_Eight_Bit = LYlowest_eightbit[i];
    }
#endif /* USE_SLANG */

    ena_csi((BOOLEAN) (LYlowest_eightbit[current_char_set] > 155));

    /* some diagnostics */
    if (TRACE) {
	if (LYRawMode_flag != LYRawMode)
	    CTRACE((tfp,
		    "HTMLSetCharacterHandling: LYRawMode changed %s -> %s\n",
		    (LYRawMode_flag ? "ON" : "OFF"),
		    (LYRawMode ? "ON" : "OFF")));
	if (UCLYhndl_for_unspec_flag != UCLYhndl_for_unspec)
	    CTRACE((tfp,
		    "HTMLSetCharacterHandling: UCLYhndl_for_unspec changed %d -> %d\n",
		    UCLYhndl_for_unspec_flag,
		    UCLYhndl_for_unspec));
    }

    return;
}

/*
 * Function to set HTCJK based on "in" and "out" charsets.
 */
void Set_HTCJK(const char *inMIMEname,
	       const char *outMIMEname)
{
    /* need not check for synonyms: MIMEname's got from LYCharSet_UC */

    if (LYRawMode) {
	if ((!strcmp(inMIMEname, "euc-jp") ||
#ifdef EXP_JAPANESEUTF8_SUPPORT
	     !strcmp(inMIMEname, "utf-8") ||
#endif
	     !strcmp(inMIMEname, "shift_jis")) &&
	    (!strcmp(outMIMEname, "euc-jp") ||
	     !strcmp(outMIMEname, "shift_jis"))) {
	    HTCJK = JAPANESE;
	} else if (!strcmp(inMIMEname, "euc-cn") &&
		   !strcmp(outMIMEname, "euc-cn")) {
	    HTCJK = CHINESE;
	} else if (!strcmp(inMIMEname, "big5") &&
		   !strcmp(outMIMEname, "big5")) {
	    HTCJK = TAIPEI;
	} else if (!strcmp(inMIMEname, "euc-kr") &&
		   !strcmp(outMIMEname, "euc-kr")) {
	    HTCJK = KOREAN;
	} else {
	    HTCJK = NOCJK;
	}
    } else {
	HTCJK = NOCJK;
    }
}

/*
 * Function to set the LYDefaultRawMode value based on the selected character
 * set.  - FM
 *
 * Currently unused:  the default value so obvious that LYUseDefaultRawMode
 * utilized directly by someone's mistake.  - LP
 */
static void HTMLSetRawModeDefault(int i)
{
    LYDefaultRawMode = (BOOL) (LYCharSet_UC[i].enc == UCT_ENC_CJK);
    return;
}

/*
 * Function to set the LYUseDefaultRawMode value based on the selected
 * character set and the current LYRawMode value.  - FM
 */
void HTMLSetUseDefaultRawMode(int i,
			      BOOLEAN modeflag)
{
    if (LYCharSet_UC[i].enc != UCT_ENC_CJK) {

	int chndl = safeUCGetLYhndl_byMIME(UCAssume_MIMEcharset);

	if (i == chndl)
	    LYUseDefaultRawMode = modeflag;
	else
	    LYUseDefaultRawMode = (BOOL) (!modeflag);
    } else			/* CJK encoding: */
	LYUseDefaultRawMode = modeflag;

    return;
}

/*
 * Function to set the LYHaveCJKCharacterSet value based on the selected
 * character set.  - FM
 */
static void HTMLSetHaveCJKCharacterSet(int i)
{
    LYHaveCJKCharacterSet = (BOOL) (LYCharSet_UC[i].enc == UCT_ENC_CJK);
    return;
}

/*
 * Function to set the DisplayCharsetMatchLocale value based on the selected
 * character set.  It is used in UPPER8 for 8bit case-insensitive search by
 * matching def7_uni.tbl images.  - LP
 */
static void HTMLSetDisplayCharsetMatchLocale(int i)
{
    BOOLEAN match;

    if (LYHaveCJKCharacterSet) {
	/*
	 * We have no intention to pass CJK via UCTransChar if that happened.
	 * Let someone from CJK correct this if necessary.
	 */
	DisplayCharsetMatchLocale = TRUE;	/* old-style */
	return;

    } else if (strncasecomp(LYCharSet_UC[i].MIMEname, "cp", 2) ||
	       strncasecomp(LYCharSet_UC[i].MIMEname, "windows", 7)) {
	/*
	 * Assume dos/windows displays usually on remote terminal, hence it
	 * rarely matches locale.  (In fact, MS Windows codepoints locale are
	 * never seen on UNIX).
	 */
	match = FALSE;
    } else {
	match = TRUE;		/* guess, but see below */

#if !defined(LOCALE)
	if (LYCharSet_UC[i].enc != UCT_ENC_UTF8)
	    /*
	     * Leave true for utf-8 display - the code doesn't deal very well
	     * with this case.  - kw
	     */
	    match = FALSE;
#else
	if (UCForce8bitTOUPPER) {
	    /*
	     * Force disable locale (from lynx.cfg)
	     */
	    match = FALSE;
	}
#endif
    }

    DisplayCharsetMatchLocale = match;
    return;
}

/*
 * lynx 2.8/2.7.2(and more early) compatibility code:  "human-readable" charset
 * names changes with time so we map that history names to MIME here to get old
 * lynx.cfg and (especially) .lynxrc always recognized.  Please update this
 * table when you change "fullname" of any present charset.
 */
typedef struct _names_pairs {
    const char *fullname;
    const char *MIMEname;
} names_pairs;
/* *INDENT-OFF* */
static const names_pairs OLD_charset_names[] =
{
    {"ISO Latin 1",		"iso-8859-1"},
    {"ISO Latin 2",             "iso-8859-2"},
    {"WinLatin1 (cp1252)",      "windows-1252"},
    {"DEC Multinational",       "dec-mcs"},
    {"Macintosh (8 bit)",       "macintosh"},
    {"NeXT character set",      "next"},
    {"KOI8-R Cyrillic",         "koi8-r"},
    {"Chinese",                 "euc-cn"},
    {"Japanese (EUC)",          "euc-jp"},
    {"Japanese (SJIS)",         "shift_jis"},
    {"Korean",                  "euc-kr"},
    {"Taipei (Big5)",           "big5"},
    {"Vietnamese (VISCII)",     "viscii"},
    {"7 bit approximations",    "us-ascii"},
    {"Transparent",             "x-transparent"},
    {"DosLatinUS (cp437)",      "cp437"},
    {"IBM PC character set",    "cp437"},
    {"DosLatin1 (cp850)",       "cp850"},
    {"IBM PC codepage 850",     "cp850"},
    {"DosLatin2 (cp852)",       "cp852"},
    {"PC Latin2 CP 852",        "cp852"},
    {"DosCyrillic (cp866)",     "cp866"},
    {"DosArabic (cp864)",       "cp864"},
    {"DosGreek (cp737)",        "cp737"},
    {"DosBaltRim (cp775)",      "cp775"},
    {"DosGreek2 (cp869)",       "cp869"},
    {"DosHebrew (cp862)",       "cp862"},
    {"WinLatin2 (cp1250)",      "windows-1250"},
    {"WinCyrillic (cp1251)",    "windows-1251"},
    {"WinGreek (cp1253)",       "windows-1253"},
    {"WinHebrew (cp1255)",      "windows-1255"},
    {"WinArabic (cp1256)",      "windows-1256"},
    {"WinBaltRim (cp1257)",     "windows-1257"},
    {"ISO Latin 3",             "iso-8859-3"},
    {"ISO Latin 4",             "iso-8859-4"},
    {"ISO 8859-5 Cyrillic",     "iso-8859-5"},
    {"ISO 8859-6 Arabic",       "iso-8859-6"},
    {"ISO 8859-7 Greek",        "iso-8859-7"},
    {"ISO 8859-8 Hebrew",       "iso-8859-8"},
    {"ISO-8859-8-I",            "iso-8859-8"},
    {"ISO-8859-8-E",            "iso-8859-8"},
    {"ISO 8859-9 (Latin 5)",    "iso-8859-9"},
    {"ISO 8859-10",             "iso-8859-10"},
    {"UNICODE UTF 8",           "utf-8"},
    {"RFC 1345 w/o Intro",      "mnemonic+ascii+0"},
    {"RFC 1345 Mnemonic",       "mnemonic"},
    {NULL, NULL},		/* terminated with NULL */
};
/* *INDENT-ON* */

/*
 * lynx 2.8/2.7.2 compatibility code:  read "character_set" parameter from
 * lynx.cfg and .lynxrc in both MIME name and "human-readable" name (old and
 * new style).  Returns -1 if not recognized.
 */
int UCGetLYhndl_byAnyName(char *value)
{
    int i;

    LYTrimTrailing(value);
    if (value == NULL)
	return -1;

    CTRACE((tfp, "UCGetLYhndl_byAnyName(%s)\n", value));

    /* search by name */
    for (i = 0; (i < MAXCHARSETS && LYchar_set_names[i]); i++) {
	if (!strcmp(value, LYchar_set_names[i])) {
	    return i;		/* OK */
	}
    }

    /* search by old name from 2.8/2.7.2 version */
    for (i = 0; (OLD_charset_names[i].fullname); i++) {
	if (!strcmp(value, OLD_charset_names[i].fullname)) {
	    return UCGetLYhndl_byMIME(OLD_charset_names[i].MIMEname);	/* OK */
	}
    }

    return UCGetLYhndl_byMIME(value);	/* by MIME */
}

/*
 * Entity names -- Ordered by ISO Latin 1 value.
 * ---------------------------------------------
 * For conversions of DECIMAL escaped entities.
 * Must be in order of ascending value.
 */
static const char *LYEntityNames[] =
{
/*	 NAME		   DECIMAL VALUE */
    "nbsp",			/* 160, non breaking space */
    "iexcl",			/* 161, inverted exclamation mark */
    "cent",			/* 162, cent sign */
    "pound",			/* 163, pound sign */
    "curren",			/* 164, currency sign */
    "yen",			/* 165, yen sign */
    "brvbar",			/* 166, broken vertical bar, (brkbar) */
    "sect",			/* 167, section sign */
    "uml",			/* 168, spacing dieresis */
    "copy",			/* 169, copyright sign */
    "ordf",			/* 170, feminine ordinal indicator */
    "laquo",			/* 171, angle quotation mark, left */
    "not",			/* 172, negation sign */
    "shy",			/* 173, soft hyphen */
    "reg",			/* 174, circled R registered sign */
    "hibar",			/* 175, spacing macron */
    "deg",			/* 176, degree sign */
    "plusmn",			/* 177, plus-or-minus sign */
    "sup2",			/* 178, superscript 2 */
    "sup3",			/* 179, superscript 3 */
    "acute",			/* 180, spacing acute (96) */
    "micro",			/* 181, micro sign */
    "para",			/* 182, paragraph sign */
    "middot",			/* 183, middle dot */
    "cedil",			/* 184, spacing cedilla */
    "sup1",			/* 185, superscript 1 */
    "ordm",			/* 186, masculine ordinal indicator */
    "raquo",			/* 187, angle quotation mark, right */
    "frac14",			/* 188, fraction 1/4 */
    "frac12",			/* 189, fraction 1/2 */
    "frac34",			/* 190, fraction 3/4 */
    "iquest",			/* 191, inverted question mark */
    "Agrave",			/* 192, capital A, grave accent */
    "Aacute",			/* 193, capital A, acute accent */
    "Acirc",			/* 194, capital A, circumflex accent */
    "Atilde",			/* 195, capital A, tilde */
    "Auml",			/* 196, capital A, dieresis or umlaut mark */
    "Aring",			/* 197, capital A, ring */
    "AElig",			/* 198, capital AE diphthong (ligature) */
    "Ccedil",			/* 199, capital C, cedilla */
    "Egrave",			/* 200, capital E, grave accent */
    "Eacute",			/* 201, capital E, acute accent */
    "Ecirc",			/* 202, capital E, circumflex accent */
    "Euml",			/* 203, capital E, dieresis or umlaut mark */
    "Igrave",			/* 204, capital I, grave accent */
    "Iacute",			/* 205, capital I, acute accent */
    "Icirc",			/* 206, capital I, circumflex accent */
    "Iuml",			/* 207, capital I, dieresis or umlaut mark */
    "ETH",			/* 208, capital Eth, Icelandic (or Latin2 Dstrok) */
    "Ntilde",			/* 209, capital N, tilde */
    "Ograve",			/* 210, capital O, grave accent */
    "Oacute",			/* 211, capital O, acute accent */
    "Ocirc",			/* 212, capital O, circumflex accent */
    "Otilde",			/* 213, capital O, tilde */
    "Ouml",			/* 214, capital O, dieresis or umlaut mark */
    "times",			/* 215, multiplication sign */
    "Oslash",			/* 216, capital O, slash */
    "Ugrave",			/* 217, capital U, grave accent */
    "Uacute",			/* 218, capital U, acute accent */
    "Ucirc",			/* 219, capital U, circumflex accent */
    "Uuml",			/* 220, capital U, dieresis or umlaut mark */
    "Yacute",			/* 221, capital Y, acute accent */
    "THORN",			/* 222, capital THORN, Icelandic */
    "szlig",			/* 223, small sharp s, German (sz ligature) */
    "agrave",			/* 224, small a, grave accent */
    "aacute",			/* 225, small a, acute accent */
    "acirc",			/* 226, small a, circumflex accent */
    "atilde",			/* 227, small a, tilde */
    "auml",			/* 228, small a, dieresis or umlaut mark */
    "aring",			/* 229, small a, ring */
    "aelig",			/* 230, small ae diphthong (ligature) */
    "ccedil",			/* 231, small c, cedilla */
    "egrave",			/* 232, small e, grave accent */
    "eacute",			/* 233, small e, acute accent */
    "ecirc",			/* 234, small e, circumflex accent */
    "euml",			/* 235, small e, dieresis or umlaut mark */
    "igrave",			/* 236, small i, grave accent */
    "iacute",			/* 237, small i, acute accent */
    "icirc",			/* 238, small i, circumflex accent */
    "iuml",			/* 239, small i, dieresis or umlaut mark */
    "eth",			/* 240, small eth, Icelandic */
    "ntilde",			/* 241, small n, tilde */
    "ograve",			/* 242, small o, grave accent */
    "oacute",			/* 243, small o, acute accent */
    "ocirc",			/* 244, small o, circumflex accent */
    "otilde",			/* 245, small o, tilde */
    "ouml",			/* 246, small o, dieresis or umlaut mark */
    "divide",			/* 247, division sign */
    "oslash",			/* 248, small o, slash */
    "ugrave",			/* 249, small u, grave accent */
    "uacute",			/* 250, small u, acute accent */
    "ucirc",			/* 251, small u, circumflex accent */
    "uuml",			/* 252, small u, dieresis or umlaut mark */
    "yacute",			/* 253, small y, acute accent */
    "thorn",			/* 254, small thorn, Icelandic */
    "yuml",			/* 255, small y, dieresis or umlaut mark */
};

/*
 * Function to return the entity names of ISO-8859-1 8-bit characters.  - FM
 */
const char *HTMLGetEntityName(UCode_t code)
{
#define IntValue code
    int MaxValue = (TABLESIZE(LYEntityNames) - 1);

    if (IntValue < 0 || IntValue > MaxValue) {
	return "";
    }

    return LYEntityNames[IntValue];
}

/*
 * Function to return the UCode_t (long int) value for entity names.  It
 * returns 0 if not found.
 *
 * unicode_entities[] handles all the names from old style entities[] too. 
 * Lynx now calls unicode_entities[] only through this function: 
 * HTMLGetEntityUCValue().  Note, we need not check for special characters here
 * in function or even before it, we should check them *after* invoking this
 * function, see put_special_unicodes() in SGML.c.
 *
 * In the future we will try to isolate all calls to entities[] in favor of new
 * unicode-based chartrans scheme.  - LP
 */
UCode_t HTMLGetEntityUCValue(const char *name)
{
#include <entities.h>

    UCode_t value = 0;
    size_t i, high, low;
    int diff = 0;
    size_t number_of_unicode_entities = TABLESIZE(unicode_entities);

    /*
     * Make sure we have a non-zero length name.  - FM
     */
    if (isEmpty(name))
	return (value);

    /*
     * Try UC_entity_info unicode_entities[].
     */
    for (low = 0, high = number_of_unicode_entities;
	 high > low;
	 diff < 0 ? (low = i + 1) : (high = i)) {
	/*
	 * Binary search.
	 */
	i = (low + (high - low) / 2);
	diff = AS_cmp(unicode_entities[i].name, name);	/* Case sensitive! */
	if (diff == 0) {
	    value = unicode_entities[i].code;
	    break;
	}
    }
    return (value);
}

/*
 * Function to select a character set and then set the character handling and
 * LYHaveCJKCharacterSet flag.  - FM
 */
void HTMLUseCharacterSet(int i)
{
    HTMLSetRawModeDefault(i);
    p_entity_values = LYCharSets[i];
    HTMLSetCharacterHandling(i);	/* set LYRawMode and CJK attributes */
    HTMLSetHaveCJKCharacterSet(i);
    HTMLSetDisplayCharsetMatchLocale(i);
    return;
}

/*
 * Initializer, calls initialization function for the CHARTRANS handling.  - KW
 */
int LYCharSetsDeclared(void)
{
    UCInit();

    return UCInitialized;
}

#ifdef EXP_CHARSET_CHOICE
void init_charset_subsets(void)
{
    int i, n;
    int cur_display = 0;
    int cur_assumed = 0;

    /* add them to displayed values */
    charset_subsets[UCLYhndl_for_unspec].hide_assumed = FALSE;
    charset_subsets[current_char_set].hide_display = FALSE;

#ifndef ALL_CHARSETS_IN_O_MENU_SCREEN
    /*all this stuff is for supporting old menu screen... */
    for (i = 0; i < LYNumCharsets; ++i) {
	if (charset_subsets[i].hide_display == FALSE) {
	    n = cur_display++;
	    if (i == current_char_set)
		displayed_display_charset_idx = n;
	    display_charset_map[n] = i;
	    display_charset_choices[n] = LYchar_set_names[i];
	}
	if (charset_subsets[i].hide_assumed == FALSE) {
	    n = cur_assumed++;
	    assumed_doc_charset_map[n] = i;
	    assumed_charset_choices[n] = LYCharSet_UC[i].MIMEname;
	    charset_subsets[i].assumed_idx = n;
	}
	display_charset_choices[cur_display] = NULL;
	assumed_charset_choices[cur_assumed] = NULL;
    }
#endif
}
#endif /* EXP_CHARSET_CHOICE */
