/*		Our Static DTD for HTML
**		-----------------------
*/

/* Implements:
*/

#include "HTUtils.h"
#include "HTMLDTD.h"
#include "LYLeaks.h"

/*	Entity Names
**	------------
**
**	This table must be matched exactly with ALL the translation tables
**		(this is an obsolete translation mechanism,
**		currently replaced with unicode chartrans in most cases...)
*/
static CONST char* entities[] = {
  "AElig",	/* capital AE diphthong (ligature) */
  "Aacute",	/* capital A, acute accent */
  "Acirc",	/* capital A, circumflex accent */
  "Agrave",	/* capital A, grave accent */
  "Aring",	/* capital A, ring */
  "Atilde",	/* capital A, tilde */
  "Auml",	/* capital A, dieresis or umlaut mark */
  "Ccedil",	/* capital C, cedilla */
  "Dstrok",	/* capital Eth, Icelandic */
  "ETH",	/* capital Eth, Icelandic */
  "Eacute",	/* capital E, acute accent */
  "Ecirc",	/* capital E, circumflex accent */
  "Egrave",	/* capital E, grave accent */
  "Euml",	/* capital E, dieresis or umlaut mark */
  "Iacute",	/* capital I, acute accent */
  "Icirc",	/* capital I, circumflex accent */
  "Igrave",	/* capital I, grave accent */
  "Iuml",	/* capital I, dieresis or umlaut mark */
  "Ntilde",	/* capital N, tilde */
  "Oacute",	/* capital O, acute accent */
  "Ocirc",	/* capital O, circumflex accent */
  "Ograve",	/* capital O, grave accent */
  "Oslash",	/* capital O, slash */
  "Otilde",	/* capital O, tilde */
  "Ouml",	/* capital O, dieresis or umlaut mark */
  "THORN",	/* capital THORN, Icelandic */
  "Uacute",	/* capital U, acute accent */
  "Ucirc",	/* capital U, circumflex accent */
  "Ugrave",	/* capital U, grave accent */
  "Uuml",	/* capital U, dieresis or umlaut mark */
  "Yacute",	/* capital Y, acute accent */
  "aacute",	/* small a, acute accent */
  "acirc",	/* small a, circumflex accent */
  "acute",	/* spacing acute */
  "aelig",	/* small ae diphthong (ligature) */
  "agrave",	/* small a, grave accent */
  "amp",	/* ampersand */
  "aring",	/* small a, ring */
  "atilde",	/* small a, tilde */
  "auml",	/* small a, dieresis or umlaut mark */
  "brkbar",	/* broken vertical bar */
  "brvbar",	/* broken vertical bar */
  "ccedil",	/* small c, cedilla */
  "cedil",	/* spacing cedilla */
  "cent",	/* cent sign */
  "copy",	/* copyright sign */
  "curren",	/* currency sign */
  "deg",	/* degree sign */
  "die",	/* spacing dieresis */
  "divide",	/* division sign */
  "eacute",	/* small e, acute accent */
  "ecirc",	/* small e, circumflex accent */
  "egrave",	/* small e, grave accent */
  "emdash",	/* dash the width of emsp */
  "emsp",	/* em space - not collapsed */
  "endash",	/* dash the width of ensp */
  "ensp",	/* en space - not collapsed */
  "eth",	/* small eth, Icelandic */
  "euml",	/* small e, dieresis or umlaut mark */
  "frac12",	/* fraction 1/2 */
  "frac14",	/* fraction 1/4 */
  "frac34",	/* fraction 3/4 */
  "gt", 	/* greater than */
  "hibar",	/* spacing macron */
  "iacute",	/* small i, acute accent */
  "icirc",	/* small i, circumflex accent */
  "iexcl",	/* inverted exclamation mark */
  "igrave",	/* small i, grave accent */
  "iquest",	/* inverted question mark */
  "iuml",	/* small i, dieresis or umlaut mark */
  "laquo",	/* angle quotation mark, left */
  "lt", 	/* less than */
  "macr",	/* spacing macron */
  "mdash",	/* dash the width of emsp */
  "micro",	/* micro sign */
  "middot",	/* middle dot */
  "nbsp",	/* non breaking space */
  "ndash",	/* dash the width of ensp */
  "not",	/* negation sign */
  "ntilde",	/* small n, tilde */
  "oacute",	/* small o, acute accent */
  "ocirc",	/* small o, circumflex accent */
  "ograve",	/* small o, grave accent */
  "ordf",	/* feminine ordinal indicator */
  "ordm",	/* masculine ordinal indicator */
  "oslash",	/* small o, slash */
  "otilde",	/* small o, tilde */
  "ouml",	/* small o, dieresis or umlaut mark */
  "para",	/* paragraph sign */
  "plusmn",	/* plus-or-minus sign */
  "pound",	/* pound sign */
  "quot",	/* quote '"' */
  "raquo",	/* angle quotation mark, right */
  "reg",	/* circled R registered sign */
  "sect",	/* section sign */
  "shy",	/* soft hyphen */
  "sup1",	/* superscript 1 */
  "sup2",	/* superscript 2 */
  "sup3",	/* superscript 3 */
  "szlig",	/* small sharp s, German (sz ligature) */
  "thinsp",	/* thin space (not collapsed) */
  "thorn",	/* small thorn, Icelandic */
  "times",	/* multiplication sign */
  "trade",	/* trade mark sign (U+2122) */
  "uacute",	/* small u, acute accent */
  "ucirc",	/* small u, circumflex accent */
  "ugrave",	/* small u, grave accent */
  "uml",	/* spacing dieresis */
  "uuml",	/* small u, dieresis or umlaut mark */
  "yacute",	/* small y, acute accent */
  "yen",	/* yen sign */
  "yuml",	/* small y, dieresis or umlaut mark */
};

#define HTML_ENTITIES 112

#include <entities.h>

/*		Attribute Lists
**		---------------
**
**	Lists must be in alphabetical order by attribute name
**	The tag elements contain the number of attributes
*/
static attr a_attr[] = {			/* Anchor attributes */
	{ "ACCESSKEY" },
	{ "CHARSET" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "COORDS" },
	{ "DIR" },
	{ "HREF" },
	{ "ID" },
	{ "ISMAP" },
	{ "LANG" },
	{ "MD" },
	{ "NAME" },
	{ "NOTAB" },
	{ "ONCLICK" },
	{ "ONMOUSEOUT" },
	{ "ONMOUSEOVER" },
	{ "REL" },
	{ "REV" },
	{ "SHAPE" },
	{ "STYLE" },
	{ "TABINDEX" },
	{ "TARGET" },
	{ "TITLE" },
	{ "TYPE" },
	{ "URN" },
	{ 0 }	/* Terminate list */
};

static attr address_attr[] = {			/* ADDRESS attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "NOWRAP" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr applet_attr[] = {			/* APPLET attributes */
	{ "ALIGN" },
	{ "ALT" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "CODE" },
	{ "CODEBASE" },
	{ "DIR" },
	{ "DOWNLOAD" },
	{ "HEIGHT" },
	{ "HSPACE" },
	{ "ID" },
	{ "LANG" },
	{ "NAME" },
	{ "STYLE" },
	{ "TITLE" },
	{ "VSPACE" },
	{ "WIDTH" },
	{ 0 }	/* Terminate list */
};

static attr area_attr[] = {			/* AREA attributes */
	{ "ALT" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "COORDS" },
	{ "DIR" },
	{ "HREF" },
	{ "ID" },
	{ "LANG" },
	{ "NOHREF" },
	{ "NOTAB" },
	{ "ONCLICK" },
	{ "ONMOUSEOUT" },
	{ "ONMOUSEOVER" },
	{ "SHAPE" },
	{ "STYLE" },
	{ "TABINDEX" },
	{ "TARGET" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr base_attr[] = {			/* BASE attributes */
	{ "HREF" },
	{ "TARGET" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr bgsound_attr[] = {			/* BGSOUND attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "LOOP" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr body_attr[] = {			/* BODY attributes */
	{ "ALINK" },
	{ "BACKGROUND" },
	{ "BGCOLOR" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "LINK" },
	{ "ONLOAD" },
	{ "ONUNLOAD" },
	{ "STYLE" },
	{ "TITLE" },
	{ "TEXT" },
	{ "VLINK" },
	{ 0 } /* Terminate list */
};

static attr bodytext_attr[] = { 		/* BODYTEXT attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DATA" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "NAME" },
	{ "OBJECT" },
	{ "REF" },
	{ "STYLE" },
	{ "TITLE" },
	{ "TYPE" },
	{ "VALUE" },
	{ "VALUETYPE" },
	{ 0 } /* Terminate list */
};

static attr bq_attr[] = {			/* BQ (BLOCKQUOTE) attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "NOWRAP" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr button_attr[] = {			/* BUTTON attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "DISABLED" },
	{ "ID" },
	{ "LANG" },
	{ "NAME" },
	{ "ONFOCUS" },
	{ "ONBLUR" },
	{ "STYLE" },
	{ "TABINDEX" },
	{ "TITLE" },
	{ "TYPE" },
	{ "VALUE" },
	{ 0 }	/* Terminate list */
};

static attr caption_attr[] = {			/* CAPTION attributes */
	{ "ACCESSKEY" },
	{ "ALIGN" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr col_attr[] = {		/* COL and COLGROUP attributes */
	{ "ALIGN" },
	{ "CHAR" },
	{ "CHAROFF" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "SPAN" },
	{ "STYLE" },
	{ "TITLE" },
	{ "VALIGN" },
	{ "WIDTH" },
	{ 0 }	/* Terminate list */
};

static attr credit_attr[] = {			/* CREDIT attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr div_attr[] = {			/* DIV attributes */
	{ "ALIGN" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr embed_attr[] = {			/* EMBED attributes */
	{ "ALIGN" },	/* (including, for now, those from FIG and IMG) */
	{ "ALT" },
	{ "BORDER" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "HEIGHT" },
	{ "ID" },
	{ "IMAGEMAP" },
	{ "ISMAP" },
	{ "LANG" },
	{ "MD" },
	{ "NAME" },
	{ "NOFLOW" },
	{ "PARAMS" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ "UNITS" },
	{ "USEMAP" },
	{ "WIDTH" },
	{ 0 }	/* Terminate list */
};

static attr fig_attr[] = {			/* FIG attributes */
	{ "ALIGN" },
	{ "BORDER" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "HEIGHT" },
	{ "ID" },
	{ "IMAGEMAP" },
	{ "ISOBJECT" },
	{ "LANG" },
	{ "MD" },
	{ "NOFLOW" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ "UNITS" },
	{ "WIDTH" },
	{ 0 }	/* Terminate list */
};

static attr fieldset_attr[] = { 		/* FIELDSET attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr fn_attr[] = {			/* FN attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr font_attr[] = {			/* FONT attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "COLOR" },
	{ "DIR" },
	{ "END" },
	{ "FACE" },
	{ "ID" },
	{ "LANG" },
	{ "SIZE" },
	{ "STYLE" },
	{ 0 }	/* Terminate list */
};

static attr form_attr[] = {			/* FORM attributes */
	{ "ACCEPT-CHARSET"},	/* HTML 4.0 draft - kw */
	{ "ACTION"},
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ENCTYPE" },
	{ "ID" },
	{ "LANG" },
	{ "METHOD" },
	{ "ONSUBMIT" },
	{ "SCRIPT" },
	{ "STYLE" },
	{ "SUBJECT" },
	{ "TARGET" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr frame_attr[] = {			/* FRAME attributes */
	{ "ID" },
	{ "MARGINHEIGHT"},
	{ "MARGINWIDTH" },
	{ "NAME" },
	{ "NORESIZE" },
	{ "SCROLLING" },
	{ "SRC" },
	{ 0 }	/* Terminate list */
};

static attr frameset_attr[] = { 		/* FRAMESET attributes */
	{ "COLS"},
	{ "ROWS" },
	{ 0 }	/* Terminate list */
};

static attr gen_attr[] = {			/* Minimum HTML 3.0 */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr glossary_attr[] = { 		/* DL (and DLC) attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "COMPACT" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr h_attr[] = {			/* H1 - H6 attributes */
	{ "ALIGN" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DINGBAT" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "MD" },
	{ "NOWRAP" },
	{ "SEQNUM" },
	{ "SKIP" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr hr_attr[] = {			/* HR attributes */
	{ "ALIGN" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "MD" },
	{ "NOSHADE" },
	{ "SIZE" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ "WIDTH" },
	{ 0 }	/* Terminate list */
};

static attr iframe_attr[] = {			/* IFRAME attributes */
	{ "ALIGN" },
	{ "FRAMEBORDER" },
	{ "HEIGHT" },
	{ "ID" },
	{ "MARGINHEIGHT"},
	{ "MARGINWIDTH" },
	{ "NAME" },
	{ "SCROLLING" },
	{ "SRC" },
	{ "STYLE" },
	{ "WIDTH" },
	{ 0 }	/* Terminate list */
};

static attr img_attr[] = {			/* IMG attributes */
	{ "ALIGN" },
	{ "ALT" },
	{ "BORDER" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "HEIGHT" },
	{ "ID" },
	{ "ISMAP" },
	{ "ISOBJECT" },
	{ "LANG" },
	{ "MD" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ "UNITS" },
	{ "USEMAP" },
	{ "WIDTH" },
	{ 0 }	/* Terminate list */
};

static attr input_attr[] = {			/* INPUT attributes */
	{ "ACCEPT" },
	{ "ACCEPT-CHARSET" },	/* RFC 2070 HTML i18n - kw */
	{ "ALIGN" },
	{ "ALT" },
	{ "CHECKED" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "DISABLED" },
	{ "ERROR" },
	{ "HEIGHT" },
	{ "ID" },
	{ "LANG" },
	{ "MAX" },
	{ "MAXLENGTH" },
	{ "MD" },
	{ "MIN" },
	{ "NAME" },
	{ "NOTAB" },
	{ "ONBLUR" },
	{ "ONCHANGE" },
	{ "ONCLICK" },
	{ "ONFOCUS" },
	{ "ONSELECT" },
	{ "SIZE" },
	{ "SRC" },
	{ "STYLE" },
	{ "TABINDEX" },
	{ "TITLE" },
	{ "TYPE" },
	{ "VALUE" },
	{ "WIDTH" },
	{ 0 } /* Terminate list */
};

static attr isindex_attr[] = {			/* ISINDEX attributes */
	{ "ACTION" },	/* Not in spec.  Lynx treats it as HREF. - FM */
	{ "DIR" },
	{ "HREF" },	/* HTML 3.0 attribute for search action. - FM */
	{ "ID" },
	{ "LANG" },
	{ "PROMPT" },	/* HTML 3.0 attribute for prompt string. - FM */
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr keygen_attr[] = {			/* KEYGEN attributes */
	{ "CHALLENGE" },
	{ "CLASS" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "NAME" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr label_attr[] = {			/* LABEL attributes */
	{ "ACCESSKEY" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "FOR" },
	{ "ID" },
	{ "LANG" },
	{ "ONCLICK" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr legend_attr[] = {			/* LEGEND attributes */
	{ "ACCESSKEY" },
	{ "ALIGN" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr link_attr[] = {			/* LINK attributes */
	{ "CHARSET" },		/* RFC 2070 HTML i18n -- hint for UA -- - kw */
	{ "CLASS" },
	{ "HREF" },
	{ "ID" },
	{ "MEDIA" },
	{ "REL" },
	{ "REV" },
	{ "STYLE" },
	{ "TARGET" },
	{ "TITLE" },
	{ "TYPE" },
	{ 0 }	/* Terminate list */
};

static attr list_attr[] = {			/* LI attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DINGBAT" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "MD" },
	{ "SKIP" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ "TYPE" },
	{ "VALUE" },
	{ 0 }	/* Terminate list */
};

static attr map_attr[] = {			/* MAP attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "NAME" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr math_attr[] = {			/* MATH attributes */
	{ "BOX" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr meta_attr[] = {			/* META attributes */
	{ "CONTENT" },
	{ "HTTP-EQUIV" },
	{ "NAME" },
	{ 0 }	/* Terminate list */
};

static attr nextid_attr[] = {			/* NEXTID attributes */
	{ "N" }
};

static attr note_attr[] = {			/* NOTE attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "MD" },
	{ "ROLE" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr object_attr[] = {			/* OBJECT attributes */
	{ "ALIGN" },
	{ "BORDER" },
	{ "CLASS" },
	{ "CLASSID" },
	{ "CODEBASE" },
	{ "CODETYPE" },
	{ "DATA" },
	{ "DECLARE" },
	{ "DIR" },
	{ "HEIGHT" },
	{ "HSPACE" },
	{ "ID" },
	{ "ISMAP" },
	{ "LANG" },
	{ "NAME" },
	{ "NOTAB" },
	{ "SHAPES" },
	{ "STANDBY" },
	{ "STYLE" },
	{ "TABINDEX" },
	{ "TITLE" },
	{ "TYPE" },
	{ "USEMAP" },
	{ "VSPACE" },
	{ "WIDTH" },
	{ 0 } /* Terminate list */
};

static attr olist_attr[] = {			/* OL attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "COMPACT" },
	{ "CONTINUE" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "SEQNUM" },
	{ "START" },
	{ "STYLE" },
	{ "TITLE" },
	{ "TYPE" },
	{ 0 }	/* Terminate list */
};

static attr option_attr[] = {			/* OPTION attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "DISABLED" },
	{ "ERROR" },
	{ "ID" },
	{ "LANG" },
	{ "SELECTED" },
	{ "SHAPE" },
	{ "STYLE" },
	{ "TITLE" },
	{ "VALUE" },
	{ 0 }	/* Terminate list */
};

static attr overlay_attr[] = {			/* OVERLAY attributes */
	{ "CLASS" },
	{ "HEIGHT" },
	{ "ID" },
	{ "IMAGEMAP" },
	{ "MD" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ "UNITS" },
	{ "WIDTH" },
	{ "X" },
	{ "Y" },
	{ 0 }	/* Terminate list */
};

static attr p_attr[] = {			/* P attributes */
	{ "ALIGN" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "NOWRAP" },
	{ "STYLE" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr param_attr[] = {			/* PARAM attributes */
	{ "ACCEPT" },
	{ "ACCEPT-CHARSET" },
	{ "ACCEPT-ENCODING" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DATA" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "NAME" },
	{ "OBJECT" },
	{ "REF" },
	{ "STYLE" },
	{ "TITLE" },
	{ "TYPE" },
	{ "VALUE" },
	{ "VALUEREF" },
	{ "VALUETYPE" },
	{ 0 }	/* Terminate list */
};

static attr script_attr[] = {			/* SCRIPT attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "EVENT" },
	{ "FOR" },
	{ "ID" },
	{ "LANG" },
	{ "LANGUAGE" },
	{ "NAME" },
	{ "SCRIPTENGINE" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ "TYPE" },
	{ 0 }	/* Terminate list */
};

static attr select_attr[] = {			/* SELECT attributes */
	{ "ALIGN" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "DISABLED" },
	{ "ERROR" },
	{ "HEIGHT" },
	{ "ID" },
	{ "LANG" },
	{ "MD" },
	{ "MULTIPLE" },
	{ "NAME" },
	{ "NOTAB" },
	{ "ONBLUR" },
	{ "ONCHANGE" },
	{ "ONFOCUS" },
	{ "SIZE" },
	{ "STYLE" },
	{ "TABINDEX" },
	{ "TITLE" },
	{ "UNITS" },
	{ "WIDTH" },
	{ 0 }	/* Terminate list */
};

static attr style_attr[] = {			/* STYLE attributes */
	{ "DIR" },
	{ "LANG" },
	{ "NOTATION" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr tab_attr[] = {			/* TAB attributes */
	{ "ALIGN" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "DP" },
	{ "ID" },
	{ "INDENT" },
	{ "LANG" },
	{ "STYLE" },
	{ "TITLE" },
	{ "TO" },
	{ 0 }	/* Terminate list */
};

static attr table_attr[] = {			/* TABLE attributes */
	{ "ALIGN" },
	{ "BORDER" },
	{ "CELLPADDING" },
	{ "CELLSPACING" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "COLS" },
	{ "COLSPEC" },
	{ "DIR" },
	{ "DP" },
	{ "FRAME" },
	{ "ID" },
	{ "LANG" },
	{ "NOFLOW" },
	{ "NOWRAP" },
	{ "RULES" },
	{ "STYLE" },
	{ "TITLE" },
	{ "UNITS" },
	{ "WIDTH" },
	{ 0 }	/* Terminate list */
};

static attr td_attr[] = {			/* TD and TH attributes */
	{ "ALIGN" },
	{ "AXES" },
	{ "AXIS" },
	{ "CHAR" },
	{ "CHAROFF" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "COLSPAN" },
	{ "DIR" },
	{ "DP" },
	{ "ID" },
	{ "LANG" },
	{ "NOWRAP" },
	{ "ROWSPAN" },
	{ "STYLE" },
	{ "TITLE" },
	{ "VALIGN" },
	{ 0 }	/* Terminate list */
};

static attr textarea_attr[] = { 		/* TEXTAREA attributes */
	{ "ACCEPT-CHARSET" },	/* RFC 2070 HTML i18n - kw */
	{ "ALIGN" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "COLS" },
	{ "DIR" },
	{ "DISABLED" },
	{ "ERROR" },
	{ "ID" },
	{ "LANG" },
	{ "NAME" },
	{ "NOTAB" },
	{ "ONBLUR" },
	{ "ONCHANGE" },
	{ "ONFOCUS" },
	{ "ONSELECT" },
	{ "ROWS" },
	{ "STYLE" },
	{ "TABINDEX" },
	{ "TITLE" },
	{ 0 }	/* Terminate list */
};

static attr tr_attr[] = {	/* TR, THEAD, TFOOT, and TBODY attributes */
	{ "ALIGN" },
	{ "CHAR" },
	{ "CHAROFF" },
	{ "CLASS" },
	{ "CLEAR" },
	{ "DIR" },
	{ "DP" },
	{ "ID" },
	{ "LANG" },
	{ "NOWRAP" },
	{ "STYLE" },
	{ "TITLE" },
	{ "VALIGN" },
	{ 0 }	/* Terminate list */
};

static attr ulist_attr[] = {			/* UL attributes */
	{ "CLASS" },
	{ "CLEAR" },
	{ "COMPACT" },
	{ "DINGBAT" },
	{ "DIR" },
	{ "ID" },
	{ "LANG" },
	{ "MD" },
	{ "PLAIN" },
	{ "SRC" },
	{ "STYLE" },
	{ "TITLE" },
	{ "TYPE" },
	{ "WRAP" },
	{ 0 }	/* Terminate list */
};

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
   1 n	  1 E	     n	      n       n       n 	      n       8 E

   %text;

   Elements within the %structure; which directly contain running text.

   Descriptive or analytic markup: EM, STRONG, DFN, CODE, SAMP, KBD, VAR, CITE, Q, LANG, AU,
				   2   2       2    2	  2	2    2	  2	2  2 n	 2
   AUTHOR, PERSON, ACRONYM, ABBREV, INS, DEL, and SPAN
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

/*	Extra element info
**	------------------
**
**	Order and number of tags should match the tags_* tables
**	further below and definitions in HTMLDTD.html.
**
**	The interspersed comments give the original Lynx tags[] entries
**	for orientation, so they do not necessarily reflect what will
**	be used with the T_* info (which is in tags_new[]).
**
**	An important design goal was that info for each tag should fit on
**	one 80 character screen line :).  The price to pay is that it's
**	a bit cryptic, to say the least...  - kw
*/
/*	 1	   2	     3	       4	 5	   6	     7	       8 */
/*345678901234567890123456789012345678901234567890123456789012345678901234567890 */

/*			self	contain icont'n contn'd icont'd canclos omit */
 /* { "A"	, a_attr,	HTML_A_ATTRIBUTES,	SGML_MIXED }, */
#define T_A		0x0008, 0x0B007,0x0FF17,0x37787,0x77BA7,0x8604F,0x00004
 /* { "ABBREV"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_ABBREV	0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00003,0x00000
 /* { "ACRONYM" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_ACRONYM	0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00003,0x00000
 /* { "ADDRESS" , address_attr, HTML_ADDRESS_ATTRIBUTES, SGML_MIXED }, */
#define T_ADDRESS	0x0200, 0x0F14F,0x8FFFF,0x36680,0xB6FAF,0x80317,0x00000
 /* { "APPLET"	, applet_attr,	HTML_APPLET_ATTRIBUTES, SGML_MIXED }, */
#define T_APPLET	0x2000, 0x0B0CF,0x8FFFF,0x37F9F,0xB7FBF,0x8300F,0x00000
 /* { "AREA"	, area_attr,	HTML_AREA_ATTRIBUTES,	SGML_EMPTY }, */
#define T_AREA		0x8000, 0x00000,0x00000,0x08000,0x3FFFF,0x00F1F,0x00001
 /* { "AU"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_AU		0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00003,0x00000
 /* { "AUTHOR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_AUTHOR	0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00003,0x00000
 /* { "B"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_B		0x0001, 0x8B04F,0xAFFFF,0xA778F,0xF7FBF,0x00001,0x00004
 /* { "BANNER"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_BANNER	0x0200, 0x0FB8F,0x0FFFF,0x30000,0x30000,0x8031F,0x00000
 /* { "BASE"	, base_attr,	HTML_BASE_ATTRIBUTES,	SGML_EMPTY }, */
#define T_BASE		0x40000,0x00000,0x00000,0x50000,0x50000,0x8000F,0x00001
 /* { "BASEFONT", font_attr,	HTML_FONT_ATTRIBUTES,	SGML_EMPTY }, */
#define T_BASEFONT	0x1000, 0x00000,0x00000,0x377AF,0x37FAF,0x8F000,0x00001
 /* { "BDO"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_BDO		0x0100, 0x0B04F,0x8FFFF,0x36680,0xB6FAF,0x0033F,0x00000
 /* { "BGSOUND" , bgsound_attr, HTML_BGSOUND_ATTRIBUTES, SGML_EMPTY }, */
#define T_BGSOUND	0x1000, 0x00000,0x00000,0x777AF,0x77FAF,0x8730F,0x00001
 /* { "BIG"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_BIG		0x0001, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00004
 /* { "BLINK"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_BLINK 	0x0001, 0x8B04F,0x8FFFF,0xA778F,0xF7FAF,0x00001,0x00004
 /* { "BLOCKQUOTE", bq_attr,	HTML_BQ_ATTRIBUTES,	SGML_MIXED }, */
#define T_BLOCKQUOTE	0x0200, 0xAFBCF,0xAFFFF,0xB6680,0xB6FAF,0x8031F,0x00000
 /* { "BODY"	, body_attr,	HTML_BODY_ATTRIBUTES,	SGML_MIXED }, */
#define T_BODY		0x20000,0x2FB8F,0x2FFFF,0x30000,0x30000,0xDFF7F,0x00003
 /* { "BODYTEXT", bodytext_attr,HTML_BODYTEXT_ATTRIBUTES, SGML_MIXED }, */
#define T_BODYTEXT	0x20000,0x0FB8F,0xAFFFF,0x30200,0xB7FAF,0x8F17F,0x00003
 /* { "BQ"	, bq_attr,	HTML_BQ_ATTRIBUTES,	SGML_MIXED }, */
#define T_BQ		0x0200, 0xAFBCF,0xAFFFF,0xB6680,0xB6FAF,0x8031F,0x00000
 /* { "BR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY }, */
#define T_BR		0x1000, 0x00000,0x00000,0x377BF,0x77FBF,0x8101F,0x00001
#define T_BUTTON	0x0200, 0x0BB0B,0x0FF3B,0x0378F,0x37FAF,0x8035F,0x00000
 /* { "CAPTION" , caption_attr, HTML_CAPTION_ATTRIBUTES, SGML_MIXED }, */
#define T_CAPTION	0x0100, 0x0B04F,0x8FFFF,0x06A00,0xB6FA7,0x8035F,0x00000
 /* { "CENTER"	, div_attr,	HTML_DIV_ATTRIBUTES,	SGML_MIXED }, */
#define T_CENTER	0x0200, 0x8FBCF,0x8FFFF,0xB6680,0xB6FA7,0x8071F,0x00000
 /* { "CITE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_CITE		0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00002,0x00000
 /* { "CODE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_CODE		0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00002,0x00000
 /* { "COL"	, col_attr,	HTML_COL_ATTRIBUTES,	SGML_EMPTY }, */
#define T_COL		0x4000, 0x00000,0x00000,0x00820,0x36FA7,0x88F5F,0x00001
 /* { "COLGROUP", col_attr,	HTML_COL_ATTRIBUTES,	SGML_EMPTY }, */
#define T_COLGROUP	0x0020, 0x04000,0x04000,0x00800,0x36FA7,0x8875F,0x00001
 /* { "COMMENT" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_COMMENT	0x0004, 0x00000,0x00000,0xA77AF,0x7FFFF,0x00003,0x00000
 /* { "CREDIT"	, credit_attr,	HTML_CREDIT_ATTRIBUTES, SGML_MIXED }, */
#define T_CREDIT	0x0100, 0x0B04F,0x8FFFF,0x06A00,0xB7FBF,0x8030F,0x00000
 /* { "DD"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY }, */
#define T_DD		0x0400, 0x0FBCF,0x8FFFF,0x00800,0xB6FFF,0x8071F,0x00001
 /* { "DEL"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_DEL		0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00003,0x00000
 /* { "DFN"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_DFN		0x0002, 0x8B0CF,0x8FFFF,0x8778F,0xF7FBF,0x00003,0x00000
 /* { "DIR"	, ulist_attr,	HTML_UL_ATTRIBUTES,	SGML_MIXED }, */
#define T_DIR		0x0800, 0x0B400,0x0F75F,0x37680,0x36FB7,0x84F7F,0x00000
 /* { "DIV"	, div_attr,	HTML_DIV_ATTRIBUTES,	SGML_MIXED }, */
#define T_DIV		0x0200, 0x8FB8F,0x8FFFF,0xB66A0,0xB7FFF,0x8031F,0x00004
 /* { "DL"	, glossary_attr, HTML_DL_ATTRIBUTES,	SGML_MIXED }, */
#define T_DL		0x0800, 0x0C480,0x8FFFF,0x36680,0xB7FB7,0x0075F,0x00000
 /* { "DLC"	, glossary_attr, HTML_DL_ATTRIBUTES,	SGML_MIXED }, */
#define T_DLC		0x0800, 0x0C480,0x8FFFF,0x36680,0xB7FB7,0x0075F,0x00000
 /* { "DT"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY }, */
#define T_DT		0x0400, 0x0B04F,0x0B1FF,0x00800,0x17FFF,0x8071F,0x00001
 /* { "EM"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_EM		0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FAF,0x00003,0x00000
 /* { "EMBED"	, embed_attr,	HTML_EMBED_ATTRIBUTES,	SGML_EMPTY }, */
#define T_EMBED 	0x2000, 0x8F107,0x8FFF7,0xB6FBF,0xB7FBF,0x1FF7F,0x00001
 /* { "FIELDSET", fieldset_attr,HTML_FIELDSET_ATTRIBUTES, SGML_MIXED }, */
#define T_FIELDSET	0x0200, 0x0FB42,0x0FF5F,0x07787,0x37FF7,0x8805F,0x00000
 /* { "FIG"	, fig_attr,	HTML_FIG_ATTRIBUTES,	SGML_MIXED }, */
#define T_FIG		0x0200, 0x0FB00,0x8FFFF,0x36680,0xB6FBF,0x8834F,0x00000
 /* { "FN"	, fn_attr,	HTML_FN_ATTRIBUTES,	SGML_MIXED }, */
#define T_FN		0x0200, 0x8FBCF,0x8FFFF,0xB6680,0xB7EBF,0x8114F,0x00000
 /* { "FONT"	, font_attr,	HTML_FONT_ATTRIBUTES,	SGML_EMPTY }, */
#define T_FONT		0x0001, 0x8B04F,0x8FFFF,0xB778F,0xF7FBF,0x00001,0x00004
 /* { "FORM"	, form_attr,	HTML_FORM_ATTRIBUTES,	SGML_EMPTY }, */
#define T_FORM		0x0080, 0x0FF6F,0x0FF7F,0x36E07,0x33F07,0x88DFF,0x00000
 /* { "FRAME"	, frame_attr,	HTML_FRAME_ATTRIBUTES,	SGML_EMPTY }, */
#define T_FRAME 	0x10000,0x00000,0x00000,0x10000,0x10000,0x9FFFF,0x00001
 /* { "FRAMESET", frameset_attr,HTML_FRAMESET_ATTRIBUTES, SGML_MIXED }, */
#define T_FRAMESET	0x10000,0x90000,0x90000,0x90000,0x93000,0x9FFFF,0x00000
 /* { "H1"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED }, */
#define T_H1		0x0100, 0x0B04F,0x0B05F,0x36680,0x37FAF,0x80317,0x00000
 /* { "H2"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED }, */
#define T_H2		0x0100, 0x0B04F,0x0B05F,0x36680,0x37FAF,0x80317,0x00000
 /* { "H3"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED }, */
#define T_H3		0x0100, 0x0B04F,0x0B05F,0x36680,0x37FAF,0x80317,0x00000
 /* { "H4"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED }, */
#define T_H4		0x0100, 0x0B04F,0x0B05F,0x36680,0x37FAF,0x80317,0x00000
 /* { "H5"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED }, */
#define T_H5		0x0100, 0x0B04F,0x0B05F,0x36680,0x37FAF,0x80317,0x00000
 /* { "H6"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED }, */
#define T_H6		0x0100, 0x0B04F,0x0B05F,0x36680,0x37FAF,0x80317,0x00000
 /* { "HEAD"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_HEAD		0x40000,0x4F000,0x47000,0x10000,0x10000,0x9FF7F,0x00006
 /* { "HR"	, hr_attr,	HTML_HR_ATTRIBUTES,	SGML_EMPTY }, */
#define T_HR		0x4000, 0x00000,0x00000,0x3FE80,0x3FFBF,0x87F37,0x00001
 /* { "HTML"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_HTML		0x10000,0x7FB8F,0x7FFFF,0x00000,0x00000,0x1FFFF,0x00003
#define T_HY		0x1000, 0x00000,0x00000,0x3779F,0x77FBF,0x8101F,0x00001
 /* { "I"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_I		0x0001, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00004
#define T_IFRAME	0x2000, 0x8FBCF,0x8FFFF,0xB679F,0xB6FBF,0xD335F,0x00000
 /* { "IMG"	, img_attr,	HTML_IMG_ATTRIBUTES,	SGML_EMPTY }, */
#define T_IMG		0x1000, 0x00000,0x00000,0x3779F,0x37FBF,0x80000,0x00001
 /* { "INPUT"	, input_attr,	HTML_INPUT_ATTRIBUTES,	SGML_EMPTY }, */
#define T_INPUT 	0x0040, 0x00000,0x00000,0x03F87,0x37F87,0x8904F,0x00001
 /* { "INS"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_INS		0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00003,0x00000
 /* { "ISINDEX" , isindex_attr, HTML_ISINDEX_ATTRIBUTES,SGML_EMPTY }, */
#define T_ISINDEX	0x8000, 0x00000,0x00000,0x7778F,0x7FFAF,0x80007,0x00001
 /* { "KBD"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_KBD		0x0002, 0x00000,0x00000,0x2778F,0x77FBF,0x00003,0x00000
 /* { "KEYGEN"	, keygen_attr,	HTML_KEYGEN_ATTRIBUTES, SGML_EMPTY }, */
#define T_KEYGEN	0x0040, 0x00000,0x00000,0x07FB7,0x37FB7,0x80070,0x00001
 /* { "LABEL"	, label_attr,	HTML_LABEL_ATTRIBUTES,	SGML_MIXED }, */
#define T_LABEL 	0x0020, 0x9FFFF,0x9FFFF,0x9FFFF,0x9FFFF,0x00007,0x00000
#define T_LEGEND	0x0002, 0x0B04F,0x0FF7F,0x00200,0x37FA7,0x00003,0x00000
 /* { "LH"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY }, */
#define T_LH		0x0400, 0x0BB7F,0x8FFFF,0x00800,0x97FFF,0x8071F,0x00001
 /* { "LI"	, list_attr,	HTML_LI_ATTRIBUTES,	SGML_EMPTY }, */
#define T_LI		0x0400, 0x0BBFF,0x8FFFF,0x00800,0x97FFF,0x8071F,0x00001
 /* { "LINK"	, link_attr,	HTML_LINK_ATTRIBUTES,	SGML_EMPTY }, */
#define T_LINK		0x8000, 0x00000,0x00000,0x50000,0x50000,0x0FF7F,0x00001
 /* { "LISTING" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_LITTERAL }, */
#define T_LISTING	0x0800, 0x00000,0x00000,0x36600,0x36F00,0x80F1F,0x00000
 /* { "MAP"	, map_attr,	HTML_MAP_ATTRIBUTES,	SGML_MIXED }, */
#define T_MAP		0x8000, 0x08000,0x08000,0x37FCF,0x37FBF,0x0071F,0x00000
 /* { "MARQUEE" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_MARQUEE	0x4000, 0x0000F,0x8F01F,0x37787,0xB7FA7,0x8301C,0x00000
 /* { "MATH"	, math_attr,	HTML_MATH_ATTRIBUTES,	SGML_LITTERAL }, */
#define T_MATH		0x0004, 0x0B05F,0x8FFFF,0x2778F,0xF7FBF,0x0001F,0x00000
 /* { "MENU"	, ulist_attr,	HTML_UL_ATTRIBUTES,	SGML_MIXED }, */
#define T_MENU		0x0800, 0x0B400,0x0F75F,0x17680,0x36FB7,0x88F7F,0x00000
 /* { "META"	, meta_attr,	HTML_META_ATTRIBUTES,	SGML_EMPTY }, */
#define T_META		0x8000, 0x00000,0x00000,0x50000,0x50000,0x0FF7F,0x00001
 /* { "NEXTID"	, nextid_attr,	1,			SGML_EMPTY }, */
#define T_NEXTID	0x1000, 0x00000,0x00000,0x50000,0x1FFF7,0x00001,0x00001
 /* { "NOFRAMES", gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_NOFRAMES	0x20000,0x2FB8F,0x0FFFF,0x17000,0x17000,0x0CF5F,0x00000
 /* { "NOTE"	, note_attr,	HTML_NOTE_ATTRIBUTES,	SGML_MIXED }, */
#define T_NOTE		0x0200, 0x0BBAF,0x8FFFF,0x376B0,0xB7FFF,0x8031F,0x00000
 /* { "OBJECT"	, object_attr,	HTML_OBJECT_ATTRIBUTES, SGML_LITTERAL }, */
#define T_OBJECT	0x2000, 0x8FBCF,0x8FFFF,0xB679F,0xB6FBF,0x83F5F,0x00000
 /* { "OL"	, olist_attr,	HTML_OL_ATTRIBUTES,	SGML_MIXED }, */
#define T_OL		0x0800, 0x0C400,0x8FFFF,0x37680,0xB7FB7,0x88F7F,0x00000
 /* { "OPTION"	, option_attr,	HTML_OPTION_ATTRIBUTES, SGML_EMPTY }, */
#define T_OPTION	0x8000, 0x00000,0x00000,0x00040,0x37FFF,0x8031F,0x00001
 /* { "OVERLAY" , overlay_attr, HTML_OVERLAY_ATTRIBUTES, SGML_EMPTY }, */
#define T_OVERLAY	0x4000, 0x00000,0x00000,0x00200,0x37FBF,0x83F7F,0x00001
 /* { "P"	, p_attr,	HTML_P_ATTRIBUTES,	SGML_EMPTY }, */
#define T_P		0x0100, 0x0B04F,0x8FFFF,0x36680,0xB6FA7,0x80117,0x00001
 /* { "PARAM"	, param_attr,	HTML_PARAM_ATTRIBUTES,	SGML_EMPTY }, */
#define T_PARAM 	0x1000, 0x00000,0x00000,0x03000,0x17FFF,0x81777,0x00001
 /* { "PLAINTEXT", gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_LITTERAL }, */
#define T_PLAINTEXT	0x10000,0xFFFFF,0xFFFFF,0x90000,0x90000,0x3FFFF,0x00001
 /* { "PRE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_PRE		0x0200, 0x0F04F,0x0F05E,0x36680,0x36FF0,0x8071E,0x00000
 /* { "Q"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_Q		0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FAF,0x00003,0x00000
 /* { "S"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_S		0x0001, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00000
 /* { "SAMP"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_SAMP		0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00002,0x00000
 /* { "SCRIPT"	, script_attr,	HTML_SCRIPT_ATTRIBUTES, SGML_LITTERAL }, */
#define T_SCRIPT	0x2000, 0x00000,0x00000,0x77F9F,0x77FFF,0x87F5F,0x00000
 /* { "SELECT"	, select_attr,	HTML_SELECT_ATTRIBUTES, SGML_MIXED }, */
#define T_SELECT	0x0040, 0x08000,0x08000,0x03FAF,0x13FBF,0x80F5F,0x00008
#define T_SHY		0x1000, 0x00000,0x00000,0x3779F,0x77FBF,0x8101F,0x00001
 /* { "SMALL"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_SMALL 	0x0001, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00004
 /* { "SPAN"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_SPAN		0x0002, 0x0B04F,0x0FFFF,0x2778F,0x77FBF,0x80003,0x00000
 /* { "SPOT"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY }, */
#define T_SPOT		0x0008, 0x00000,0x00000,0x3FFF7,0x3FFF7,0x00008,0x00001
 /* { "STRIKE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_STRIKE	0x0001, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00000
 /* { "STRONG"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_STRONG	0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FAF,0x00003,0x00000
 /* { "STYLE"	, style_attr,	HTML_STYLE_ATTRIBUTES,	SGML_LITTERAL }, */
#define T_STYLE 	0x40000,0x00000,0x00000,0x7638F,0x76FAF,0x8001F,0x00000
 /* { "SUB"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_SUB		0x0004, 0x8B05F,0x8FFFF,0x8779F,0xF7FBF,0x00007,0x00000
 /* { "SUP"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_SUP		0x0004, 0x8B05F,0x8FFFF,0x8779F,0xF7FBF,0x00007,0x00000
 /* { "TAB"	, tab_attr,	HTML_TAB_ATTRIBUTES,	SGML_EMPTY }, */
#define T_TAB		0x1000, 0x00000,0x00000,0x3778F,0x57FAF,0x00001,0x00001
 /* { "TABLE"	, table_attr,	HTML_TABLE_ATTRIBUTES,	SGML_MIXED }, */
#define T_TABLE 	0x0800, 0x0F1E0,0x8FFFF,0x36680,0xB6FA7,0x8C57F,0x00000
 /* { "TBODY"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_EMPTY }, */
#define T_TBODY 	0x0020, 0x00020,0x8FFFF,0x00880,0xB7FB7,0x8C75F,0x00003
 /* { "TD"	, td_attr,	HTML_TD_ATTRIBUTES,	SGML_EMPTY }, */
#define T_TD		0x0400, 0x0FBCF,0x8FFFF,0x00020,0xB7FB7,0x8C75F,0x00001
 /* { "TEXTAREA", textarea_attr,HTML_TEXTAREA_ATTRIBUTES, SGML_LITTERAL }, */
#define T_TEXTAREA	0x0040, 0x00000,0x00000,0x07F8F,0x33FBF,0x80F5F,0x00000
 /* { "TEXTFLOW", bodytext_attr,HTML_BODYTEXT_ATTRIBUTES, SGML_MIXED }, */
#define T_TEXTFLOW	0x20000,0x8FBFF,0x9FFFF,0x977B0,0xB7FB7,0x9B00F,0x00003
 /* { "TFOOT"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_EMPTY }, */
#define T_TFOOT 	0x0020, 0x00020,0x8FFFF,0x00800,0xB7FB7,0x8CF5F,0x00001
 /* { "TH"	, td_attr,	HTML_TD_ATTRIBUTES,	SGML_EMPTY }, */
#define T_TH		0x0400, 0x0FBCF,0x0FFFF,0x00020,0xB7FB7,0x8CF5F,0x00001
 /* { "THEAD"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_EMPTY }, */
#define T_THEAD 	0x0020, 0x00020,0x8FFFF,0x00880,0xB7FB7,0x8CF5F,0x00001
 /* { "TITLE",	  gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_RCDATA }, */
#define T_TITLE 	0x40000,0x00000,0x00000,0x50000,0x50000,0x0031F,0x00004
 /* { "TR"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_EMPTY }, */
#define T_TR		0x0020, 0x00400,0x8FFFF,0x00820,0xB7FB7,0x8C75F,0x00001
 /* { "TT"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_TT		0x0001, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00000
 /* { "U"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_U		0x0001, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00004
 /* { "UL"	, ulist_attr,	HTML_UL_ATTRIBUTES,	SGML_MIXED }, */
#define T_UL		0x0800, 0x0C480,0x8FFFF,0x36680,0xB7FFF,0x8075F,0x00000
 /* { "VAR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED }, */
#define T_VAR		0x0002, 0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00000
#define T_WBR		0x0001, 0x00000,0x00000,0x3778F,0x77FBF,0x8101F,0x00001
 /* { "XMP"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_LITTERAL }, */
#define T_XMP		0x0800, 0x00000,0x00000,0x367E0,0x36FFF,0x0875F,0x00001

#define T__UNREC_	0x0000, 0x00000,0x00000,0x00000,0x00000,0x00000,0x00000

/*	Elements
**	--------
**
**	Must match definitions in HTMLDTD.html!
**	Must be in alphabetical order.
**
**  The T_* extra info is listed here, but it won't matter (is not used
**  in SGML.c if New_DTD is not set).  This mainly simplifies comparison
**  of the tags_old[] table (otherwise unchanged from original Lynx treatment)
**  with the tags_new[] table below. - kw
**
**    Name,	Attributes,	No. of attributes,     content,   extra info...
*/
static HTTag tags_old[HTML_ELEMENTS] = {
    { "A"	, a_attr,	HTML_A_ATTRIBUTES,	SGML_EMPTY,T_A},
    { "ABBREV"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_ABBREV},
    { "ACRONYM" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_ACRONYM},
    { "ADDRESS" , address_attr, HTML_ADDRESS_ATTRIBUTES, SGML_MIXED,T_ADDRESS},
    { "APPLET"	, applet_attr,	HTML_APPLET_ATTRIBUTES, SGML_MIXED,T_APPLET},
    { "AREA"	, area_attr,	HTML_AREA_ATTRIBUTES,	SGML_EMPTY,T_AREA},
    { "AU"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_AU},
    { "AUTHOR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_AUTHOR},
    { "B"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_B},
    { "BANNER"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_BANNER},
    { "BASE"	, base_attr,	HTML_BASE_ATTRIBUTES,	SGML_EMPTY,T_BASE},
    { "BASEFONT", font_attr,	HTML_FONT_ATTRIBUTES,	SGML_EMPTY,T_BASEFONT},
    { "BDO"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_BDO},
    { "BGSOUND" , bgsound_attr, HTML_BGSOUND_ATTRIBUTES, SGML_EMPTY,T_BGSOUND},
    { "BIG"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_BIG},
    { "BLINK"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_BLINK},
    { "BLOCKQUOTE", bq_attr,	HTML_BQ_ATTRIBUTES,	SGML_MIXED,T_BLOCKQUOTE},
    { "BODY"	, body_attr,	HTML_BODY_ATTRIBUTES,	SGML_MIXED,T_BODY},
    { "BODYTEXT", bodytext_attr,HTML_BODYTEXT_ATTRIBUTES, SGML_MIXED,T_BODYTEXT},
    { "BQ"	, bq_attr,	HTML_BQ_ATTRIBUTES,	SGML_MIXED,T_BQ},
    { "BR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_BR},
    { "BUTTON"	, button_attr,	HTML_BUTTON_ATTRIBUTES, SGML_MIXED,T_BUTTON},
    { "CAPTION" , caption_attr, HTML_CAPTION_ATTRIBUTES, SGML_MIXED,T_CAPTION},
    { "CENTER"	, div_attr,	HTML_DIV_ATTRIBUTES,	SGML_MIXED,T_CENTER},
    { "CITE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_CITE},
    { "CODE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_CODE},
    { "COL"	, col_attr,	HTML_COL_ATTRIBUTES,	SGML_EMPTY,T_COL},
    { "COLGROUP", col_attr,	HTML_COL_ATTRIBUTES,	SGML_EMPTY,T_COLGROUP},
    { "COMMENT" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_COMMENT},
    { "CREDIT"	, credit_attr,	HTML_CREDIT_ATTRIBUTES, SGML_MIXED,T_CREDIT},
    { "DD"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_DD},
    { "DEL"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_DEL},
    { "DFN"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_DFN},
    { "DIR"	, ulist_attr,	HTML_UL_ATTRIBUTES,	SGML_MIXED,T_DIR},
    { "DIV"	, div_attr,	HTML_DIV_ATTRIBUTES,	SGML_MIXED,T_DIV},
    { "DL"	, glossary_attr, HTML_DL_ATTRIBUTES,	SGML_MIXED,T_DL},
    { "DLC"	, glossary_attr, HTML_DL_ATTRIBUTES,	SGML_MIXED,T_DLC},
    { "DT"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_DT},
    { "EM"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_EM},
    { "EMBED"	, embed_attr,	HTML_EMBED_ATTRIBUTES,	SGML_EMPTY,T_EMBED},
    { "FIELDSET", fieldset_attr,HTML_FIELDSET_ATTRIBUTES, SGML_MIXED,T_FIELDSET},
    { "FIG"	, fig_attr,	HTML_FIG_ATTRIBUTES,	SGML_MIXED,T_FIG},
    { "FN"	, fn_attr,	HTML_FN_ATTRIBUTES,	SGML_MIXED,T_FN},
    { "FONT"	, font_attr,	HTML_FONT_ATTRIBUTES,	SGML_EMPTY,T_FONT},
    { "FORM"	, form_attr,	HTML_FORM_ATTRIBUTES,	SGML_EMPTY,T_FORM},
    { "FRAME"	, frame_attr,	HTML_FRAME_ATTRIBUTES,	SGML_EMPTY,T_FRAME},
    { "FRAMESET", frameset_attr,HTML_FRAMESET_ATTRIBUTES, SGML_MIXED,T_FRAMESET},
    { "H1"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H1},
    { "H2"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H2},
    { "H3"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H3},
    { "H4"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H4},
    { "H5"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H5},
    { "H6"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H6},
    { "HEAD"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_HEAD},
    { "HR"	, hr_attr,	HTML_HR_ATTRIBUTES,	SGML_EMPTY,T_HR},
    { "HTML"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_HTML},
    { "HY"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_HY},
    { "I"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_I},
    { "IFRAME"	, iframe_attr,	HTML_IFRAME_ATTRIBUTES, SGML_MIXED,T_IFRAME},
    { "IMG"	, img_attr,	HTML_IMG_ATTRIBUTES,	SGML_EMPTY,T_IMG},
    { "INPUT"	, input_attr,	HTML_INPUT_ATTRIBUTES,	SGML_EMPTY,T_INPUT},
    { "INS"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_INS},
    { "ISINDEX" , isindex_attr, HTML_ISINDEX_ATTRIBUTES,SGML_EMPTY,T_ISINDEX},
    { "KBD"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_KBD},
    { "KEYGEN"	, keygen_attr,	HTML_KEYGEN_ATTRIBUTES, SGML_EMPTY,T_KEYGEN},
    { "LABEL"	, label_attr,	HTML_LABEL_ATTRIBUTES,	SGML_MIXED,T_LABEL},
    { "LEGEND"	, legend_attr,	HTML_LEGEND_ATTRIBUTES, SGML_MIXED,T_LEGEND},
    { "LH"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_LH},
    { "LI"	, list_attr,	HTML_LI_ATTRIBUTES,	SGML_EMPTY,T_LI},
    { "LINK"	, link_attr,	HTML_LINK_ATTRIBUTES,	SGML_EMPTY,T_LINK},
    { "LISTING" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_LITTERAL,T_LISTING},
    { "MAP"	, map_attr,	HTML_MAP_ATTRIBUTES,	SGML_MIXED,T_MAP},
    { "MARQUEE" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_MARQUEE},
    { "MATH"	, math_attr,	HTML_MATH_ATTRIBUTES,	SGML_LITTERAL,T_MATH},
    { "MENU"	, ulist_attr,	HTML_UL_ATTRIBUTES,	SGML_MIXED,T_MENU},
    { "META"	, meta_attr,	HTML_META_ATTRIBUTES,	SGML_EMPTY,T_META},
    { "NEXTID"	, nextid_attr,	1,			SGML_EMPTY,T_NEXTID},
    { "NOFRAMES", gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_NOFRAMES},
    { "NOTE"	, note_attr,	HTML_NOTE_ATTRIBUTES,	SGML_MIXED,T_NOTE},
    { "OBJECT"	, object_attr,	HTML_OBJECT_ATTRIBUTES, SGML_LITTERAL,T_OBJECT},
    { "OL"	, olist_attr,	HTML_OL_ATTRIBUTES,	SGML_MIXED,T_OL},
    { "OPTION"	, option_attr,	HTML_OPTION_ATTRIBUTES, SGML_EMPTY,T_OPTION},
    { "OVERLAY" , overlay_attr, HTML_OVERLAY_ATTRIBUTES, SGML_EMPTY,T_OVERLAY},
    { "P"	, p_attr,	HTML_P_ATTRIBUTES,	SGML_EMPTY,T_P},
    { "PARAM"	, param_attr,	HTML_PARAM_ATTRIBUTES,	SGML_EMPTY,T_PARAM},
    { "PLAINTEXT", gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_LITTERAL,T_PLAINTEXT},
    { "PRE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_PRE},
    { "Q"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_Q},
    { "S"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_S},
    { "SAMP"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_SAMP},
    { "SCRIPT"	, script_attr,	HTML_SCRIPT_ATTRIBUTES, SGML_LITTERAL,T_SCRIPT},
    { "SELECT"	, select_attr,	HTML_SELECT_ATTRIBUTES, SGML_MIXED,T_SELECT},
    { "SHY"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_SHY},
    { "SMALL"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_SMALL},
    { "SPAN"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_SPAN},
    { "SPOT"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_SPOT},
    { "STRIKE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_STRIKE},
    { "STRONG"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_STRONG},
    { "STYLE"	, style_attr,	HTML_STYLE_ATTRIBUTES,	SGML_LITTERAL,T_STYLE},
    { "SUB"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_SUB},
    { "SUP"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_SUP},
    { "TAB"	, tab_attr,	HTML_TAB_ATTRIBUTES,	SGML_EMPTY,T_TAB},
    { "TABLE"	, table_attr,	HTML_TABLE_ATTRIBUTES,	SGML_MIXED,T_TABLE},
    { "TBODY"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_EMPTY,T_TBODY},
    { "TD"	, td_attr,	HTML_TD_ATTRIBUTES,	SGML_EMPTY,T_TD},
    { "TEXTAREA", textarea_attr,HTML_TEXTAREA_ATTRIBUTES, SGML_LITTERAL,T_TEXTAREA},
    { "TEXTFLOW", bodytext_attr,HTML_BODYTEXT_ATTRIBUTES, SGML_MIXED,T_TEXTFLOW},
    { "TFOOT"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_EMPTY,T_TFOOT},
    { "TH"	, td_attr,	HTML_TD_ATTRIBUTES,	SGML_EMPTY,T_TH},
    { "THEAD"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_EMPTY,T_THEAD},
    { "TITLE",	  gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_RCDATA,T_TITLE},
    { "TR"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_EMPTY,T_TR},
    { "TT"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_TT},
    { "U"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_U},
    { "UL"	, ulist_attr,	HTML_UL_ATTRIBUTES,	SGML_MIXED,T_UL},
    { "VAR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_VAR},
    { "WBR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_WBR},
    { "XMP"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_LITTERAL,T_XMP},
};

static HTTag tags_new[HTML_ELEMENTS] = {
    { "A"	, a_attr,	HTML_A_ATTRIBUTES,	SGML_MIXED,T_A},
    { "ABBREV"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_ABBREV},
    { "ACRONYM" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_ACRONYM},
    { "ADDRESS" , address_attr, HTML_ADDRESS_ATTRIBUTES, SGML_MIXED,T_ADDRESS},
    { "APPLET"	, applet_attr,	HTML_APPLET_ATTRIBUTES, SGML_MIXED,T_APPLET},
    { "AREA"	, area_attr,	HTML_AREA_ATTRIBUTES,	SGML_EMPTY,T_AREA},
    { "AU"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_AU},
    { "AUTHOR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_AUTHOR},
    { "B"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_B},
    { "BANNER"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_BANNER},
    { "BASE"	, base_attr,	HTML_BASE_ATTRIBUTES,	SGML_EMPTY,T_BASE},
    { "BASEFONT", font_attr,	HTML_FONT_ATTRIBUTES,	SGML_EMPTY,T_BASEFONT},
    { "BDO"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_BDO},
    { "BGSOUND" , bgsound_attr, HTML_BGSOUND_ATTRIBUTES, SGML_EMPTY,T_BGSOUND},
    { "BIG"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_BIG},
    { "BLINK"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_BLINK},
    { "BLOCKQUOTE", bq_attr,	HTML_BQ_ATTRIBUTES,	SGML_MIXED,T_BLOCKQUOTE},
    { "BODY"	, body_attr,	HTML_BODY_ATTRIBUTES,	SGML_MIXED,T_BODY},
    { "BODYTEXT", bodytext_attr,HTML_BODYTEXT_ATTRIBUTES, SGML_MIXED,T_BODYTEXT},
    { "BQ"	, bq_attr,	HTML_BQ_ATTRIBUTES,	SGML_MIXED,T_BQ},
    { "BR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_BR},
    { "BUTTON"	, button_attr,	HTML_BUTTON_ATTRIBUTES, SGML_MIXED,T_BUTTON},
    { "CAPTION" , caption_attr, HTML_CAPTION_ATTRIBUTES, SGML_MIXED,T_CAPTION},
    { "CENTER"	, div_attr,	HTML_DIV_ATTRIBUTES,	SGML_MIXED,T_CENTER},
    { "CITE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_CITE},
    { "CODE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_CODE},
    { "COL"	, col_attr,	HTML_COL_ATTRIBUTES,	SGML_EMPTY,T_COL},
    { "COLGROUP", col_attr,	HTML_COL_ATTRIBUTES,	SGML_ELEMENT,T_COLGROUP},
    { "COMMENT" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_PCDATA,T_COMMENT},
    { "CREDIT"	, credit_attr,	HTML_CREDIT_ATTRIBUTES, SGML_MIXED,T_CREDIT},
    { "DD"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_DD},
    { "DEL"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_DEL},
    { "DFN"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_DFN},
    { "DIR"	, ulist_attr,	HTML_UL_ATTRIBUTES,	SGML_MIXED,T_DIR},
    { "DIV"	, div_attr,	HTML_DIV_ATTRIBUTES,	SGML_MIXED,T_DIV},
    { "DL"	, glossary_attr, HTML_DL_ATTRIBUTES,	SGML_MIXED,T_DL},
    { "DLC"	, glossary_attr, HTML_DL_ATTRIBUTES,	SGML_MIXED,T_DLC},
    { "DT"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_DT},
    { "EM"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_EM},
    { "EMBED"	, embed_attr,	HTML_EMBED_ATTRIBUTES,	SGML_EMPTY,T_EMBED},
    { "FIELDSET", fieldset_attr,HTML_FIELDSET_ATTRIBUTES, SGML_MIXED,T_FIELDSET},
    { "FIG"	, fig_attr,	HTML_FIG_ATTRIBUTES,	SGML_MIXED,T_FIG},
    { "FN"	, fn_attr,	HTML_FN_ATTRIBUTES,	SGML_MIXED,T_FN},
    { "FONT"	, font_attr,	HTML_FONT_ATTRIBUTES,	SGML_MIXED,T_FONT},
    { "FORM"	, form_attr,	HTML_FORM_ATTRIBUTES,	SGML_MIXED,T_FORM},
    { "FRAME"	, frame_attr,	HTML_FRAME_ATTRIBUTES,	SGML_EMPTY,T_FRAME},
    { "FRAMESET", frameset_attr,HTML_FRAMESET_ATTRIBUTES, SGML_ELEMENT,T_FRAMESET},
    { "H1"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H1},
    { "H2"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H2},
    { "H3"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H3},
    { "H4"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H4},
    { "H5"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H5},
    { "H6"	, h_attr,	HTML_H_ATTRIBUTES,	SGML_MIXED,T_H6},
    { "HEAD"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_ELEMENT,T_HEAD},
    { "HR"	, hr_attr,	HTML_HR_ATTRIBUTES,	SGML_EMPTY,T_HR},
    { "HTML"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_HTML},
    { "HY"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_HY},
    { "I"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_I},
    { "IFRAME"	, iframe_attr,	HTML_IFRAME_ATTRIBUTES, SGML_MIXED,T_IFRAME},
    { "IMG"	, img_attr,	HTML_IMG_ATTRIBUTES,	SGML_EMPTY,T_IMG},
    { "INPUT"	, input_attr,	HTML_INPUT_ATTRIBUTES,	SGML_EMPTY,T_INPUT},
    { "INS"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_INS},
    { "ISINDEX" , isindex_attr, HTML_ISINDEX_ATTRIBUTES,SGML_EMPTY,T_ISINDEX},
    { "KBD"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_KBD},
    { "KEYGEN"	, keygen_attr,	HTML_KEYGEN_ATTRIBUTES, SGML_EMPTY,T_KEYGEN},
    { "LABEL"	, label_attr,	HTML_LABEL_ATTRIBUTES,	SGML_MIXED,T_LABEL},
    { "LEGEND"	, legend_attr,	HTML_LEGEND_ATTRIBUTES, SGML_MIXED,T_LEGEND},
    { "LH"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_LH},
    { "LI"	, list_attr,	HTML_LI_ATTRIBUTES,	SGML_MIXED,T_LI},
    { "LINK"	, link_attr,	HTML_LINK_ATTRIBUTES,	SGML_EMPTY,T_LINK},
    { "LISTING" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_LITTERAL,T_LISTING},
    { "MAP"	, map_attr,	HTML_MAP_ATTRIBUTES,	SGML_ELEMENT,T_MAP},
    { "MARQUEE" , gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_MARQUEE},
    { "MATH"	, math_attr,	HTML_MATH_ATTRIBUTES,	SGML_LITTERAL,T_MATH},
    { "MENU"	, ulist_attr,	HTML_UL_ATTRIBUTES,	SGML_MIXED,T_MENU},
    { "META"	, meta_attr,	HTML_META_ATTRIBUTES,	SGML_EMPTY,T_META},
    { "NEXTID"	, nextid_attr,	1,			SGML_EMPTY,T_NEXTID},
    { "NOFRAMES", gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_NOFRAMES},
    { "NOTE"	, note_attr,	HTML_NOTE_ATTRIBUTES,	SGML_MIXED,T_NOTE},
    { "OBJECT"	, object_attr,	HTML_OBJECT_ATTRIBUTES, SGML_LITTERAL,T_OBJECT},
    { "OL"	, olist_attr,	HTML_OL_ATTRIBUTES,	SGML_MIXED,T_OL},
    { "OPTION"	, option_attr,	HTML_OPTION_ATTRIBUTES, SGML_PCDATA,T_OPTION},
    { "OVERLAY" , overlay_attr, HTML_OVERLAY_ATTRIBUTES, SGML_PCDATA,T_OVERLAY},
    { "P"	, p_attr,	HTML_P_ATTRIBUTES,	SGML_MIXED,T_P},
    { "PARAM"	, param_attr,	HTML_PARAM_ATTRIBUTES,	SGML_EMPTY,T_PARAM},
    { "PLAINTEXT", gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_LITTERAL,T_PLAINTEXT},
    { "PRE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_PRE},
    { "Q"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_Q},
    { "S"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_S},
    { "SAMP"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_SAMP},
    { "SCRIPT"	, script_attr,	HTML_SCRIPT_ATTRIBUTES, SGML_LITTERAL,T_SCRIPT},
    { "SELECT"	, select_attr,	HTML_SELECT_ATTRIBUTES, SGML_ELEMENT,T_SELECT},
    { "SHY"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_SHY},
    { "SMALL"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_SMALL},
    { "SPAN"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_SPAN},
    { "SPOT"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_SPOT},
    { "STRIKE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_STRIKE},
    { "STRONG"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_STRONG},
    { "STYLE"	, style_attr,	HTML_STYLE_ATTRIBUTES,	SGML_LITTERAL,T_STYLE},
    { "SUB"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_SUB},
    { "SUP"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_SUP},
    { "TAB"	, tab_attr,	HTML_TAB_ATTRIBUTES,	SGML_EMPTY,T_TAB},
    { "TABLE"	, table_attr,	HTML_TABLE_ATTRIBUTES,	SGML_ELEMENT,T_TABLE},
    { "TBODY"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_ELEMENT,T_TBODY},
    { "TD"	, td_attr,	HTML_TD_ATTRIBUTES,	SGML_MIXED,T_TD},
    { "TEXTAREA", textarea_attr,HTML_TEXTAREA_ATTRIBUTES, SGML_LITTERAL,T_TEXTAREA},
    { "TEXTFLOW", bodytext_attr,HTML_BODYTEXT_ATTRIBUTES, SGML_MIXED,T_TEXTFLOW},
    { "TFOOT"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_ELEMENT,T_TFOOT},
    { "TH"	, td_attr,	HTML_TD_ATTRIBUTES,	SGML_MIXED,T_TH},
    { "THEAD"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_ELEMENT,T_THEAD},
    { "TITLE"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_PCDATA,T_TITLE},
    { "TR"	, tr_attr,	HTML_TR_ATTRIBUTES,	SGML_MIXED,T_TR},
    { "TT"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_TT},
    { "U"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_U},
    { "UL"	, ulist_attr,	HTML_UL_ATTRIBUTES,	SGML_MIXED,T_UL},
    { "VAR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_MIXED,T_VAR},
    { "WBR"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_EMPTY,T_WBR},
    { "XMP"	, gen_attr,	HTML_GEN_ATTRIBUTES,	SGML_LITTERAL,T_XMP},
};

/* Dummy space, will be filled with the contents of either tags_new
   or tags_old on calling HTSwitchDTD - kw */

static HTTag tags[HTML_ELEMENTS];

PUBLIC CONST SGML_dtd HTML_dtd = {
	tags,
	HTML_ELEMENTS,
	entities,
	sizeof(entities)/sizeof(entities[0]),
	unicode_entities,
	sizeof(unicode_entities)/sizeof(unicode_entities[0])
};

/* This function fills the "tags" part of the HTML_dtd structure with
   what we want to use, either tags_old or tags_new.  Note that it
   has to be called at least once before HTML_dtd is used, otherwise
   the HTML_dtd contents will be invalid!  This could be coded in a way
   that would make an initialisation call unnecessary, but my C knowledge
   is limited and I didn't want to list the whole tags_new table
   twice... - kw */
PUBLIC void HTSwitchDTD ARGS1(
    BOOL,		new)
{
    if (TRACE)
	fprintf(stderr,"HTMLDTD: Copying DTD element info of size %d, %d * %d\n",
		new ? sizeof(tags_new) : sizeof(tags_old),
		HTML_ELEMENTS, sizeof(HTTag));
    if (new)
	memcpy(tags, tags_new, HTML_ELEMENTS * sizeof(HTTag));
    else
	memcpy(tags, tags_old, HTML_ELEMENTS * sizeof(HTTag));
}

PUBLIC CONST HTTag HTTag_unrecognized =
    { NULL,    NULL,		0,	SGML_EMPTY,T__UNREC_};

/*
**	Utility Routine:  Useful for people building HTML objects.
*/

/*	Start anchor element
**	--------------------
**
**	It is kinda convenient to have a particulr routine for
**	starting an anchor element, as everything else for HTML is
**	simple anyway.
*/
struct _HTStructured {
    HTStructuredClass * isa;
	/* ... */
};

PUBLIC void HTStartAnchor ARGS3(
	HTStructured *, 	obj,
	CONST char *,		name,
	CONST char *,		href)
{
    BOOL		present[HTML_A_ATTRIBUTES];
    CONST char *	value[HTML_A_ATTRIBUTES];
    int i;

    for (i = 0; i < HTML_A_ATTRIBUTES; i++)
	 present[i] = NO;

    if (name && *name) {
	present[HTML_A_NAME] = YES;
	value[HTML_A_NAME] = (CONST char *)name;
    }
    if (href) {
	present[HTML_A_HREF] = YES;
	value[HTML_A_HREF] = (CONST char *)href;
    }

    (*obj->isa->start_element)(obj, HTML_A, present, value, -1, 0);
}

PUBLIC void HTStartIsIndex ARGS3(
	HTStructured *, 	obj,
	CONST char *,		prompt,
	CONST char *,		href)
{
    BOOL		present[HTML_ISINDEX_ATTRIBUTES];
    CONST char *	value[HTML_ISINDEX_ATTRIBUTES];
    int i;

    for (i = 0; i < HTML_ISINDEX_ATTRIBUTES; i++)
	present[i] = NO;

    if (prompt && *prompt) {
	present[HTML_ISINDEX_PROMPT] = YES;
	value[HTML_ISINDEX_PROMPT] = (CONST char *)prompt;
    }
    if (href) {
	present[HTML_ISINDEX_HREF] = YES;
	value[HTML_ISINDEX_HREF] = (CONST char *)href;
    }

    (*obj->isa->start_element)(obj, HTML_ISINDEX , present, value, -1, 0);
}
