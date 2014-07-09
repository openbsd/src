/*
 * $LynxId: HTMLDTD.c,v 1.57 2010/09/25 00:30:56 tom Exp $
 *
 *		Our Static DTD for HTML
 *		-----------------------
 */

/* Implements:
*/

#include <HTUtils.h>
#include <HTMLDTD.h>
#include <LYLeaks.h>
#include <LYJustify.h>

/*
 * Character entities like &nbsp now excluded from our DTD tables, they are
 * mapped to Unicode and handled by chartrans code directly the similar way the
 * numeric entities like &#123 does.  See src/chrtrans/entities.h for real
 * mapping.
 */

/*	Entity Names
 *	------------
 *
 *	This table must be matched exactly with ALL the translation tables
 *		(this is an obsolete translation mechanism, probably unused,
 *		currently replaced with Unicode chartrans in most cases...)
 */
static const char *entities[] =
{
    "AElig",			/* capital AE diphthong (ligature) */
    "Aacute",			/* capital A, acute accent */
    "Acirc",			/* capital A, circumflex accent */
    "Agrave",			/* capital A, grave accent */
    "Aring",			/* capital A, ring */
    "Atilde",			/* capital A, tilde */
    "Auml",			/* capital A, dieresis or umlaut mark */
    "Ccedil",			/* capital C, cedilla */
    "Dstrok",			/* capital Eth, Icelandic */
    "ETH",			/* capital Eth, Icelandic */
    "Eacute",			/* capital E, acute accent */
    "Ecirc",			/* capital E, circumflex accent */
    "Egrave",			/* capital E, grave accent */
    "Euml",			/* capital E, dieresis or umlaut mark */
    "Iacute",			/* capital I, acute accent */
    "Icirc",			/* capital I, circumflex accent */
    "Igrave",			/* capital I, grave accent */
    "Iuml",			/* capital I, dieresis or umlaut mark */
    "Ntilde",			/* capital N, tilde */
    "Oacute",			/* capital O, acute accent */
    "Ocirc",			/* capital O, circumflex accent */
    "Ograve",			/* capital O, grave accent */
    "Oslash",			/* capital O, slash */
    "Otilde",			/* capital O, tilde */
    "Ouml",			/* capital O, dieresis or umlaut mark */
    "THORN",			/* capital THORN, Icelandic */
    "Uacute",			/* capital U, acute accent */
    "Ucirc",			/* capital U, circumflex accent */
    "Ugrave",			/* capital U, grave accent */
    "Uuml",			/* capital U, dieresis or umlaut mark */
    "Yacute",			/* capital Y, acute accent */
    "aacute",			/* small a, acute accent */
    "acirc",			/* small a, circumflex accent */
    "acute",			/* spacing acute */
    "aelig",			/* small ae diphthong (ligature) */
    "agrave",			/* small a, grave accent */
    "amp",			/* ampersand */
    "aring",			/* small a, ring */
    "atilde",			/* small a, tilde */
    "auml",			/* small a, dieresis or umlaut mark */
    "brkbar",			/* broken vertical bar */
    "brvbar",			/* broken vertical bar */
    "ccedil",			/* small c, cedilla */
    "cedil",			/* spacing cedilla */
    "cent",			/* cent sign */
    "copy",			/* copyright sign */
    "curren",			/* currency sign */
    "deg",			/* degree sign */
    "die",			/* spacing dieresis */
    "divide",			/* division sign */
    "eacute",			/* small e, acute accent */
    "ecirc",			/* small e, circumflex accent */
    "egrave",			/* small e, grave accent */
    "emdash",			/* dash the width of emsp */
    "emsp",			/* em space - not collapsed */
    "endash",			/* dash the width of ensp */
    "ensp",			/* en space - not collapsed */
    "eth",			/* small eth, Icelandic */
    "euml",			/* small e, dieresis or umlaut mark */
    "frac12",			/* fraction 1/2 */
    "frac14",			/* fraction 1/4 */
    "frac34",			/* fraction 3/4 */
    "gt",			/* greater than */
    "hibar",			/* spacing macron */
    "iacute",			/* small i, acute accent */
    "icirc",			/* small i, circumflex accent */
    "iexcl",			/* inverted exclamation mark */
    "igrave",			/* small i, grave accent */
    "iquest",			/* inverted question mark */
    "iuml",			/* small i, dieresis or umlaut mark */
    "laquo",			/* angle quotation mark, left */
    "lt",			/* less than */
    "macr",			/* spacing macron */
    "mdash",			/* dash the width of emsp */
    "micro",			/* micro sign */
    "middot",			/* middle dot */
    "nbsp",			/* non breaking space */
    "ndash",			/* dash the width of ensp */
    "not",			/* negation sign */
    "ntilde",			/* small n, tilde */
    "oacute",			/* small o, acute accent */
    "ocirc",			/* small o, circumflex accent */
    "ograve",			/* small o, grave accent */
    "ordf",			/* feminine ordinal indicator */
    "ordm",			/* masculine ordinal indicator */
    "oslash",			/* small o, slash */
    "otilde",			/* small o, tilde */
    "ouml",			/* small o, dieresis or umlaut mark */
    "para",			/* paragraph sign */
    "plusmn",			/* plus-or-minus sign */
    "pound",			/* pound sign */
    "quot",			/* quote '"' */
    "raquo",			/* angle quotation mark, right */
    "reg",			/* circled R registered sign */
    "sect",			/* section sign */
    "shy",			/* soft hyphen */
    "sup1",			/* superscript 1 */
    "sup2",			/* superscript 2 */
    "sup3",			/* superscript 3 */
    "szlig",			/* small sharp s, German (sz ligature) */
    "thinsp",			/* thin space (not collapsed) */
    "thorn",			/* small thorn, Icelandic */
    "times",			/* multiplication sign */
    "trade",			/* trade mark sign (U+2122) */
    "uacute",			/* small u, acute accent */
    "ucirc",			/* small u, circumflex accent */
    "ugrave",			/* small u, grave accent */
    "uml",			/* spacing dieresis */
    "uuml",			/* small u, dieresis or umlaut mark */
    "yacute",			/* small y, acute accent */
    "yen",			/* yen sign */
    "yuml",			/* small y, dieresis or umlaut mark */
};

/*		Attribute Lists
 *		---------------
 *
 *	Lists must be in alphabetical order by attribute name
 *	The tag elements contain the number of attributes
 */

/* From Peter Flynn's intro to the HTML Pro DTD:

   %structure;

   DIV, CENTER, H1 to H6, P, UL, OL, DL, DIR, MENU, PRE, XMP, LISTING, BLOCKQUOTE, BQ,
   2	1	2     2   1  8	 8   8	 8    8     8	 8    8        4	   4
   MULTICOL,?NOBR, FORM, TABLE, ADDRESS, FIG, BDO, NOTE, and FN; plus?WBR, LI, and LH
   8 n	    ?1 n   8	 8	2	 2    2    2	     2	    ?1 nE  4	   4

   %insertions;

   Elements which usually contain special-purpose material, or no text material at all.

   BASEFONT, APPLET, OBJECT, EMBED, SCRIPT, MAP, MARQUEE, HR, ISINDEX, BGSOUND, TAB,?IMG,
   1 e?      2	     2 l     1 e    2 l     8	 4	  4 E 1? E     1 E	! E ?1 E
   IMAGE, BR, plus NOEMBED, SERVER, SPACER, AUDIOSCOPE, and SIDEBAR; ?area
   1 n	  1 E	     n	      n	      n	      n		      n	      8 E

   %text;

   Elements within the %structure; which directly contain running text.

   Descriptive or analytic markup: EM, STRONG, DFN, CODE, SAMP, KBD, VAR, CITE, Q, LANG, AU,
				   2   2       2    2	  2	2    2	  2	2  2 n	 2
   AUTHOR, PERSON, ACRONYM, ABBR, INS, DEL, and SPAN
   2	   2 n	   2	    2	    2	 2	  2
   Visual markup:S, STRIKE, I, B, TT, U,?NOBR,?WBR, BR, BIG, SMALL, FONT, STYLE, BLINK, TAB,
		 1  1	    1  1  1   1  ?1 n ?1nE? 1 E  1   1	    1	  1 l	 1	1 E?
   BLACKFACE, LIMITTEXT, NOSMARTQUOTES, and SHADOW
   1 n	      1 n	 1 n		    1 n
   Hypertext and graphics: A and?IMG
			   8	?8 E
   Mathematical: SUB, SUP, and MATH
		 4    4        4 l
   Documentary: COMMENT, ENTITY, ELEMENT, and ATTRIB
		4	 4 n	 4 n	      4 n
   %formula;
 */

/*	Elements
 *	--------
 *
 *	Must match definitions in HTMLDTD.html!
 *	Must be in alphabetical order.
 *
 *  The T_* extra info is listed here, even though most fields are not used
 *  in SGML.c if Old_DTD is set (with the exception of some Tgf_* flags).
 *  This simplifies comparison of the tags_table0[] table (otherwise unchanged
 *  from original Lynx treatment) with the tags_table1[] table below. - kw
 *
 *    Name*,	Attributes,	No. of attributes,     content,   extra info...
 */

#include <src0_HTMLDTD.h>
#include <src1_HTMLDTD.h>

/* Dummy space, will be filled with the contents of either tags_table1
   or tags_table0 on calling HTSwitchDTD - kw */

static HTTag tags[HTML_ALL_ELEMENTS];

const SGML_dtd HTML_dtd =
{
    tags,
    HTML_ELEMENTS,
    entities,			/* probably unused */
    TABLESIZE(entities),
};

/* This function fills the "tags" part of the HTML_dtd structure with
   what we want to use, either tags_table0 or tags_table1.  Note that it
   has to be called at least once before HTML_dtd is used, otherwise
   the HTML_dtd contents will be invalid!  This could be coded in a way
   that would make an initialisation call unnecessary, but my C knowledge
   is limited and I didn't want to list the whole tags_table1 table
   twice... - kw */
void HTSwitchDTD(int new_flag)
{
    if (TRACE)
	CTRACE((tfp,
		"HTMLDTD: Copying %s DTD element info of size %d, %d * %d\n",
		new_flag ? "strict" : "tagsoup",
		(int) (new_flag ? sizeof(tags_table1) : sizeof(tags_table0)),
		HTML_ALL_ELEMENTS,
		(int) sizeof(HTTag)));
    if (new_flag)
	MemCpy(tags, tags_table1, HTML_ALL_ELEMENTS * sizeof(HTTag));
    else
	MemCpy(tags, tags_table0, HTML_ALL_ELEMENTS * sizeof(HTTag));
}

HTTag HTTag_unrecognized =

{NULL_HTTag, NULL, 0, 0, SGML_EMPTY, T__UNREC_};

/*
 *	Utility Routine:  Useful for people building HTML objects.
 */

/*	Start anchor element
 *	--------------------
 *
 *	It is kinda convenient to have a particulr routine for
 *	starting an anchor element, as everything else for HTML is
 *	simple anyway.
 */
struct _HTStructured {
    HTStructuredClass *isa;
    /* ... */
};

void HTStartAnchor(HTStructured * obj, const char *name,
		   const char *href)
{
    BOOL present[HTML_A_ATTRIBUTES];
    const char *value[HTML_A_ATTRIBUTES];
    int i;

    for (i = 0; i < HTML_A_ATTRIBUTES; i++)
	present[i] = NO;

    if (name && *name) {
	present[HTML_A_NAME] = YES;
	value[HTML_A_NAME] = (const char *) name;
    }
    if (href) {
	present[HTML_A_HREF] = YES;
	value[HTML_A_HREF] = (const char *) href;
    }

    (*obj->isa->start_element) (obj, HTML_A, present, value, -1, 0);
}

void HTStartAnchor5(HTStructured * obj, const char *name,
		    const char *href,
		    const char *linktype,
		    int tag_charset)
{
    BOOL present[HTML_A_ATTRIBUTES];
    const char *value[HTML_A_ATTRIBUTES];
    int i;

    for (i = 0; i < HTML_A_ATTRIBUTES; i++)
	present[i] = NO;

    if (name && *name) {
	present[HTML_A_NAME] = YES;
	value[HTML_A_NAME] = name;
    }
    if (href && *href) {
	present[HTML_A_HREF] = YES;
	value[HTML_A_HREF] = href;
    }
    if (linktype && *linktype) {
	present[HTML_A_TYPE] = YES;
	value[HTML_A_TYPE] = linktype;
    }

    (*obj->isa->start_element) (obj, HTML_A, present, value, tag_charset, 0);
}

void HTStartIsIndex(HTStructured * obj, const char *prompt,
		    const char *href)
{
    BOOL present[HTML_ISINDEX_ATTRIBUTES];
    const char *value[HTML_ISINDEX_ATTRIBUTES];
    int i;

    for (i = 0; i < HTML_ISINDEX_ATTRIBUTES; i++)
	present[i] = NO;

    if (prompt && *prompt) {
	present[HTML_ISINDEX_PROMPT] = YES;
	value[HTML_ISINDEX_PROMPT] = (const char *) prompt;
    }
    if (href) {
	present[HTML_ISINDEX_HREF] = YES;
	value[HTML_ISINDEX_HREF] = (const char *) href;
    }

    (*obj->isa->start_element) (obj, HTML_ISINDEX, present, value, -1, 0);
}
