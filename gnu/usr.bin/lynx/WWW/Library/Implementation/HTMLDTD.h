/*				 The HTML DTD -- software interface in libwww
			      HTML DTD - SOFTWARE INTERFACE

   SGML purists should excuse the use of the term "DTD" in this file to
   represent DTD-related information which is not exactly a DTD itself.

   The C modular structure doesn't work very well here, as the dtd is
   partly in the .h and partly in the .c which are not very independent.
   Tant pis.

 */
#ifndef HTMLDTD_H
#define HTMLDTD_H

#include <SGML.h>
#include <HTFont.h>

/*
**  Valid name chars for tag parsing.
*/
#define IsNmStart(c) (isalpha(UCH(c)))
#define IsNmChar(c) (isalnum(UCH(c)) || \
		      c == '_' || c=='-' || c == '.' || c==':')


#define ReallyEmptyTagNum(e) ((HTML_dtd.tags[e].contents == SGML_EMPTY) && \
			      !(HTML_dtd.tags[e].flags & Tgf_nreie))
#define ReallyEmptyTag(t) ((t->contents == SGML_EMPTY) && \
			   !(t->flags & Tgf_nreie))

/*

Element Numbers

 */

/*

   Must Match all tables by element!
   These include tables in HTMLDTD.c and code in HTML.c.

 */
typedef enum {
	HTML_A,
	HTML_ABBREV,
	HTML_ACRONYM,
	HTML_ADDRESS,
	HTML_APPLET,
	HTML_AREA,
	HTML_AU,
	HTML_AUTHOR,
	HTML_B,
	HTML_BANNER,
	HTML_BASE,
	HTML_BASEFONT,
	HTML_BDO,
	HTML_BGSOUND,
	HTML_BIG,
	HTML_BLINK,
	HTML_BLOCKQUOTE,
	HTML_BODY,
	HTML_BODYTEXT,
	HTML_BQ,
	HTML_BR,
	HTML_BUTTON,
	HTML_CAPTION,
	HTML_CENTER,
	HTML_CITE,
	HTML_CODE,
	HTML_COL,
	HTML_COLGROUP,
	HTML_COMMENT,
	HTML_CREDIT,
	HTML_DD,
	HTML_DEL,
	HTML_DFN,
	HTML_DIR,
	HTML_DIV,
	HTML_DL,
	HTML_DLC,
	HTML_DT,
	HTML_EM,
	HTML_EMBED,
	HTML_FIELDSET,
	HTML_FIG,
	HTML_FN,
	HTML_FONT,
	HTML_FORM,
	HTML_FRAME,
	HTML_FRAMESET,
	HTML_H1,
	HTML_H2,
	HTML_H3,
	HTML_H4,
	HTML_H5,
	HTML_H6,
	HTML_HEAD,
	HTML_HR,
	HTML_HTML,
	HTML_HY,
	HTML_I,
	HTML_IFRAME,
	HTML_IMG,
	HTML_INPUT,
	HTML_INS,
	HTML_ISINDEX,
	HTML_KBD,
	HTML_KEYGEN,
	HTML_LABEL,
	HTML_LEGEND,
	HTML_LH,
	HTML_LI,
	HTML_LINK,
	HTML_LISTING,
	HTML_MAP,
	HTML_MARQUEE,
	HTML_MATH,
	HTML_MENU,
	HTML_META,
	HTML_NEXTID,
	HTML_NOFRAMES,
	HTML_NOTE,
	HTML_OBJECT,
	HTML_OL,
	HTML_OPTION,
	HTML_OVERLAY,
	HTML_P,
	HTML_PARAM,
	HTML_PLAINTEXT,
	HTML_PRE,
	HTML_Q,
	HTML_S,
	HTML_SAMP,
	HTML_SCRIPT,
	HTML_SELECT,
	HTML_SHY,
	HTML_SMALL,
	HTML_SPAN,
	HTML_SPOT,
	HTML_STRIKE,
	HTML_STRONG,
	HTML_STYLE,
	HTML_SUB,
	HTML_SUP,
	HTML_TAB,
	HTML_TABLE,
	HTML_TBODY,
	HTML_TD,
	HTML_TEXTAREA,
	HTML_TEXTFLOW,
	HTML_TFOOT,
	HTML_TH,
	HTML_THEAD,
	HTML_TITLE,
	HTML_TR,
	HTML_TT,
	HTML_U,
	HTML_UL,
	HTML_VAR,
	HTML_WBR,
	HTML_XMP,
	HTML_ALT_OBJECT } HTMLElement;

/* Notes: HTML.c uses a different extension of the HTML_ELEMENTS space
          privately, see HTNestedList.h. */
/*        Don't replace HTML_ELEMENTS with TABLESIZE(mumble_dtd.tags). */
/* Keep the following defines in synch with the above enum! */

/* HTML_ELEMENTS:     number of elements visible to Lynx code in general,
                      alphabetic (ASCII) order. */
#define HTML_ELEMENTS 118

/* HTML_ALL_ELEMENTS: number of elements visible to SGML parser,
                      additional variant(s) at end. */
#define HTML_ALL_ELEMENTS 119


/*

Attribute numbers

 */

/*

   Identifier is HTML_<element>_<attribute>.
   These must match the tables in HTML.c!

 */
#define HTML_A_ACCESSKEY        0
#define HTML_A_CHARSET          1 /* i18n draft, added tentatively - KW */
#define HTML_A_CLASS            2
#define HTML_A_CLEAR            3
#define HTML_A_COORDS           4
#define HTML_A_DIR              5
#define HTML_A_HREF             6
#define HTML_A_ID               7
#define HTML_A_ISMAP            8
#define HTML_A_LANG             9
#define HTML_A_MD              10
#define HTML_A_NAME            11
#define HTML_A_NOTAB           12
#define HTML_A_ONCLICK         13
#define HTML_A_ONMOUSEOUT      14
#define HTML_A_ONMOUSEOVER     15
#define HTML_A_REL             16
#define HTML_A_REV             17
#define HTML_A_SHAPE           18
#define HTML_A_STYLE           19
#define HTML_A_TABINDEX        20
#define HTML_A_TARGET          21
#define HTML_A_TITLE           22
#define HTML_A_TYPE            23
#define HTML_A_URN             24
#define HTML_A_ATTRIBUTES      25

#define HTML_ADDRESS_CLASS      0
#define HTML_ADDRESS_CLEAR      1
#define HTML_ADDRESS_DIR        2
#define HTML_ADDRESS_ID         3
#define HTML_ADDRESS_LANG       4
#define HTML_ADDRESS_NOWRAP     5
#define HTML_ADDRESS_STYLE      6
#define HTML_ADDRESS_TITLE      7
#define HTML_ADDRESS_ATTRIBUTES 8

#define HTML_APPLET_ALIGN       0
#define HTML_APPLET_ALT         1
#define HTML_APPLET_CLASS       2
#define HTML_APPLET_CLEAR       3
#define HTML_APPLET_CODE        4
#define HTML_APPLET_CODEBASE    5
#define HTML_APPLET_DIR         6
#define HTML_APPLET_DOWNLOAD    7
#define HTML_APPLET_HEIGHT      8
#define HTML_APPLET_HSPACE      9
#define HTML_APPLET_ID         10
#define HTML_APPLET_LANG       11
#define HTML_APPLET_NAME       12
#define HTML_APPLET_STYLE      13
#define HTML_APPLET_TITLE      14
#define HTML_APPLET_VSPACE     15
#define HTML_APPLET_WIDTH      16
#define HTML_APPLET_ATTRIBUTES 17

#define HTML_AREA_ALT           0
#define HTML_AREA_CLASS         1
#define HTML_AREA_CLEAR         2
#define HTML_AREA_COORDS        3
#define HTML_AREA_DIR           4
#define HTML_AREA_HREF          5
#define HTML_AREA_ID            6
#define HTML_AREA_LANG          7
#define HTML_AREA_NOHREF        8
#define HTML_AREA_NONOTAB       9
#define HTML_AREA_ONCLICK      10
#define HTML_AREA_ONMOUSEOUT   11
#define HTML_AREA_ONMOUSEOVER  12
#define HTML_AREA_SHAPE        13
#define HTML_AREA_STYLE        14
#define HTML_AREA_TABINDEX     15
#define HTML_AREA_TARGET       16
#define HTML_AREA_TITLE        17
#define HTML_AREA_ATTRIBUTES   18

#define HTML_BASE_HREF          0
#define HTML_BASE_TARGET        1
#define HTML_BASE_TITLE         2
#define HTML_BASE_ATTRIBUTES    3

#define HTML_BGSOUND_CLASS      0
#define HTML_BGSOUND_CLEAR      1
#define HTML_BGSOUND_DIR        2
#define HTML_BGSOUND_ID         3
#define HTML_BGSOUND_LANG       4
#define HTML_BGSOUND_LOOP       5
#define HTML_BGSOUND_SRC        6
#define HTML_BGSOUND_STYLE      7
#define HTML_BGSOUND_TITLE      8
#define HTML_BGSOUND_ATTRIBUTES 9

#define HTML_BODY_ALINK         0
#define HTML_BODY_BACKGROUND    1
#define HTML_BODY_BGCOLOR       2
#define HTML_BODY_CLASS         3
#define HTML_BODY_CLEAR         4
#define HTML_BODY_DIR           5
#define HTML_BODY_ID            6
#define HTML_BODY_LANG          7
#define HTML_BODY_LINK          8
#define HTML_BODY_ONLOAD        9
#define HTML_BODY_ONUNLOAD     10
#define HTML_BODY_STYLE        11
#define HTML_BODY_TEXT         12
#define HTML_BODY_TITLE        13
#define HTML_BODY_VLINK        14
#define HTML_BODY_ATTRIBUTES   15

#define HTML_BODYTEXT_CLASS     0
#define HTML_BODYTEXT_CLEAR     1
#define HTML_BODYTEXT_DATA      2
#define HTML_BODYTEXT_DIR       3
#define HTML_BODYTEXT_ID        4
#define HTML_BODYTEXT_LANG      5
#define HTML_BODYTEXT_NAME      6
#define HTML_BODYTEXT_OBJECT    7
#define HTML_BODYTEXT_REF       8
#define HTML_BODYTEXT_STYLE     9
#define HTML_BODYTEXT_TITLE    10
#define HTML_BODYTEXT_TYPE     11
#define HTML_BODYTEXT_VALUE    12
#define HTML_BODYTEXT_VALUETYPE  13
#define HTML_BODYTEXT_ATTRIBUTES 14

#define HTML_BQ_CITE             0
#define HTML_BQ_CLASS            1
#define HTML_BQ_CLEAR            2
#define HTML_BQ_DIR              3
#define HTML_BQ_ID               4
#define HTML_BQ_LANG             5
#define HTML_BQ_NOWRAP           6
#define HTML_BQ_STYLE            7
#define HTML_BQ_TITLE            8
#define HTML_BQ_ATTRIBUTES       9

#define HTML_BUTTON_CLASS       0
#define HTML_BUTTON_CLEAR       1
#define HTML_BUTTON_DIR         2
#define HTML_BUTTON_DISABLED    3
#define HTML_BUTTON_ID          4
#define HTML_BUTTON_LANG        5
#define HTML_BUTTON_NAME        6
#define HTML_BUTTON_ONFOCUS     7
#define HTML_BUTTON_ONBLUR      8
#define HTML_BUTTON_STYLE       9
#define HTML_BUTTON_TABINDEX   10
#define HTML_BUTTON_TITLE      11
#define HTML_BUTTON_TYPE       12
#define HTML_BUTTON_VALUE      13
#define HTML_BUTTON_ATTRIBUTES 14

#define HTML_CAPTION_ACCESSKEY  0
#define HTML_CAPTION_ALIGN      1
#define HTML_CAPTION_CLASS      2
#define HTML_CAPTION_CLEAR      3
#define HTML_CAPTION_DIR        4
#define HTML_CAPTION_ID         5
#define HTML_CAPTION_LANG       6
#define HTML_CAPTION_STYLE      7
#define HTML_CAPTION_TITLE      8
#define HTML_CAPTION_ATTRIBUTES 9

#define HTML_COL_ALIGN          0
#define HTML_COL_CHAR           1
#define HTML_COL_CHAROFF        2
#define HTML_COL_CLASS          3
#define HTML_COL_CLEAR          4
#define HTML_COL_DIR            5
#define HTML_COL_ID             6
#define HTML_COL_LANG           7
#define HTML_COL_SPAN           8
#define HTML_COL_STYLE          9
#define HTML_COL_TITLE         10
#define HTML_COL_VALIGN        11
#define HTML_COL_WIDTH         12
#define HTML_COL_ATTRIBUTES    13

#define HTML_CREDIT_CLASS       0
#define HTML_CREDIT_CLEAR       1
#define HTML_CREDIT_DIR         2
#define HTML_CREDIT_ID          3
#define HTML_CREDIT_LANG        4
#define HTML_CREDIT_STYLE       5
#define HTML_CREDIT_TITLE       6
#define HTML_CREDIT_ATTRIBUTES  7

#define HTML_DIV_ALIGN          0
#define HTML_DIV_CLASS          1
#define HTML_DIV_CLEAR          2
#define HTML_DIV_DIR            3
#define HTML_DIV_ID             4
#define HTML_DIV_LANG           5
#define HTML_DIV_STYLE          6
#define HTML_DIV_TITLE          7
#define HTML_DIV_ATTRIBUTES     8

#define HTML_DL_CLASS           0
#define HTML_DL_CLEAR           1
#define HTML_DL_COMPACT         2
#define HTML_DL_DIR             3
#define HTML_DL_ID              4
#define HTML_DL_LANG            5
#define HTML_DL_STYLE           6
#define HTML_DL_TITLE           7
#define HTML_DL_ATTRIBUTES      8

#define HTML_EMBED_ALIGN        0
#define HTML_EMBED_ALT          1
#define HTML_EMBED_BORDER       2
#define HTML_EMBED_CLASS        3
#define HTML_EMBED_CLEAR        4
#define HTML_EMBED_DIR          5
#define HTML_EMBED_HEIGHT       6
#define HTML_EMBED_ID           7
#define HTML_EMBED_IMAGEMAP     8
#define HTML_EMBED_ISMAP        9
#define HTML_EMBED_LANG        10
#define HTML_EMBED_MD          11
#define HTML_EMBED_NAME        12
#define HTML_EMBED_NOFLOW      13
#define HTML_EMBED_PARAMS      14
#define HTML_EMBED_SRC         15
#define HTML_EMBED_STYLE       16
#define HTML_EMBED_TITLE       17
#define HTML_EMBED_UNITS       18
#define HTML_EMBED_USEMAP      19
#define HTML_EMBED_WIDTH       20
#define HTML_EMBED_ATTRIBUTES  21

#define HTML_FIELDSET_CLASS     0
#define HTML_FIELDSET_CLEAR     1
#define HTML_FIELDSET_DIR       2
#define HTML_FIELDSET_ID        3
#define HTML_FIELDSET_LANG      4
#define HTML_FIELDSET_STYLE     5
#define HTML_FIELDSET_TITLE     6
#define HTML_FIELDSET_ATTRIBUTES 7

#define HTML_FIG_ALIGN          0
#define HTML_FIG_BORDER         1
#define HTML_FIG_CLASS          2
#define HTML_FIG_CLEAR          3
#define HTML_FIG_DIR            4
#define HTML_FIG_HEIGHT         5
#define HTML_FIG_ID             6
#define HTML_FIG_IMAGEMAP       7
#define HTML_FIG_ISOBJECT       8
#define HTML_FIG_LANG           9
#define HTML_FIG_MD            10
#define HTML_FIG_NOFLOW        11
#define HTML_FIG_SRC           12
#define HTML_FIG_STYLE         13
#define HTML_FIG_TITLE         14
#define HTML_FIG_UNITS         15
#define HTML_FIG_WIDTH         16
#define HTML_FIG_ATTRIBUTES    17

#define HTML_FN_CLASS           0
#define HTML_FN_CLEAR           1
#define HTML_FN_DIR             2
#define HTML_FN_ID              3
#define HTML_FN_LANG            4
#define HTML_FN_STYLE           5
#define HTML_FN_TITLE           6
#define HTML_FN_ATTRIBUTES      7

#define HTML_FONT_CLASS         0
#define HTML_FONT_CLEAR         1
#define HTML_FONT_COLOR         2
#define HTML_FONT_DIR           3
#define HTML_FONT_FACE          4
#define HTML_FONT_ID            5
#define HTML_FONT_LANG          6
#define HTML_FONT_SIZE          7
#define HTML_FONT_STYLE         8
#define HTML_FONT_ATTRIBUTES    9

#define HTML_FORM_ACCEPT_CHARSET  0 /* HTML 4.0 draft - kw */
#define HTML_FORM_ACTION        1
#define HTML_FORM_CLASS         2
#define HTML_FORM_CLEAR         3
#define HTML_FORM_DIR           4
#define HTML_FORM_ENCTYPE       5
#define HTML_FORM_ID            6
#define HTML_FORM_LANG          7
#define HTML_FORM_METHOD        8
#define HTML_FORM_ONSUBMIT      9
#define HTML_FORM_SCRIPT       10
#define HTML_FORM_STYLE        11
#define HTML_FORM_SUBJECT      12
#define HTML_FORM_TARGET       13
#define HTML_FORM_TITLE        14
#define HTML_FORM_ATTRIBUTES   15

#define HTML_FRAME_ID            0
#define HTML_FRAME_LONGDESC      1
#define HTML_FRAME_MARGINHEIGHT  2
#define HTML_FRAME_MARGINWIDTH   3
#define HTML_FRAME_NAME          4
#define HTML_FRAME_NORESIZE      5
#define HTML_FRAME_SCROLLING     6
#define HTML_FRAME_SRC           7
#define HTML_FRAME_ATTRIBUTES    8

#define HTML_FRAMESET_COLS      0
#define HTML_FRAMESET_ROWS      1
#define HTML_FRAMESET_ATTRIBUTES 2

#define HTML_GEN_CLASS          0
#define HTML_GEN_CLEAR          1
#define HTML_GEN_DIR            2
#define HTML_GEN_ID             3
#define HTML_GEN_LANG           4
#define HTML_GEN_STYLE          5
#define HTML_GEN_TITLE          6
#define HTML_GEN_ATTRIBUTES     7

#define HTML_H_ALIGN            0
#define HTML_H_CLASS            1
#define HTML_H_CLEAR            2
#define HTML_H_DINGBAT          3
#define HTML_H_DIR              4
#define HTML_H_ID               5
#define HTML_H_LANG             6
#define HTML_H_MD               7
#define HTML_H_NOWRAP           8
#define HTML_H_SEQNUM           9
#define HTML_H_SKIP            10
#define HTML_H_SRC             11
#define HTML_H_STYLE           12
#define HTML_H_TITLE           13
#define HTML_H_ATTRIBUTES      14

#define HTML_HR_ALIGN           0
#define HTML_HR_CLASS           1
#define HTML_HR_CLEAR           2
#define HTML_HR_DIR             3
#define HTML_HR_ID              4
#define HTML_HR_MD              5
#define HTML_HR_NOSHADE         6
#define HTML_HR_SIZE            7
#define HTML_HR_SRC             8
#define HTML_HR_STYLE           9
#define HTML_HR_TITLE          10
#define HTML_HR_WIDTH          11
#define HTML_HR_ATTRIBUTES     12

#define HTML_IFRAME_ALIGN         0
#define HTML_IFRAME_FRAMEBORDER   1
#define HTML_IFRAME_HEIGHT        2
#define HTML_IFRAME_ID            3
#define HTML_IFRAME_LONGDESC      4
#define HTML_IFRAME_MARGINHEIGHT  5
#define HTML_IFRAME_MARGINWIDTH   6
#define HTML_IFRAME_NAME          7
#define HTML_IFRAME_SCROLLING     8
#define HTML_IFRAME_SRC           9
#define HTML_IFRAME_STYLE        10
#define HTML_IFRAME_WIDTH        11
#define HTML_IFRAME_ATTRIBUTES   12

#define HTML_IMG_ALIGN           0
#define HTML_IMG_ALT             1
#define HTML_IMG_BORDER          2
#define HTML_IMG_CLASS           3
#define HTML_IMG_CLEAR           4
#define HTML_IMG_DIR             5
#define HTML_IMG_HEIGHT          6
#define HTML_IMG_ID              7
#define HTML_IMG_ISMAP           8
#define HTML_IMG_ISOBJECT        9
#define HTML_IMG_LANG           10
#define HTML_IMG_LONGDESC       11
#define HTML_IMG_MD             12
#define HTML_IMG_SRC            13
#define HTML_IMG_STYLE          14
#define HTML_IMG_TITLE          15
#define HTML_IMG_UNITS          16
#define HTML_IMG_USEMAP         17
#define HTML_IMG_WIDTH          18
#define HTML_IMG_ATTRIBUTES     19

#define HTML_INPUT_ACCEPT       0
#define HTML_INPUT_ACCEPT_CHARSET  1 /* RFC 2070 HTML i18n - kw */
#define HTML_INPUT_ALIGN        2
#define HTML_INPUT_ALT          3
#define HTML_INPUT_CHECKED      4
#define HTML_INPUT_CLASS        5
#define HTML_INPUT_CLEAR        6
#define HTML_INPUT_DIR          7
#define HTML_INPUT_DISABLED     8
#define HTML_INPUT_ERROR        9
#define HTML_INPUT_HEIGHT      10
#define HTML_INPUT_ID          11
#define HTML_INPUT_LANG        12
#define HTML_INPUT_MAX         13
#define HTML_INPUT_MAXLENGTH   14
#define HTML_INPUT_MD          15
#define HTML_INPUT_MIN         16
#define HTML_INPUT_NAME        17
#define HTML_INPUT_NOTAB       18
#define HTML_INPUT_ONBLUR      19
#define HTML_INPUT_ONCHANGE    20
#define HTML_INPUT_ONCLICK     21
#define HTML_INPUT_ONFOCUS     22
#define HTML_INPUT_ONSELECT    23
#define HTML_INPUT_SIZE        24
#define HTML_INPUT_SRC         25
#define HTML_INPUT_STYLE       26
#define HTML_INPUT_TABINDEX    27
#define HTML_INPUT_TITLE       28
#define HTML_INPUT_TYPE        29
#define HTML_INPUT_VALUE       30
#define HTML_INPUT_WIDTH       31
#define HTML_INPUT_ATTRIBUTES  32

#define HTML_ISINDEX_ACTION     0  /* Treat as synonym for HREF. - FM */
#define HTML_ISINDEX_DIR        1
#define HTML_ISINDEX_HREF       2  /* HTML 3.0 "action". - FM */
#define HTML_ISINDEX_ID         3
#define HTML_ISINDEX_LANG       4
#define HTML_ISINDEX_PROMPT     5  /* HTML 3.0 "prompt". - FM */
#define HTML_ISINDEX_TITLE      6
#define HTML_ISINDEX_ATTRIBUTES 7

#define HTML_KEYGEN_CHALLENGE   0
#define HTML_KEYGEN_CLASS       1
#define HTML_KEYGEN_DIR         2
#define HTML_KEYGEN_ID          3
#define HTML_KEYGEN_LANG        4
#define HTML_KEYGEN_NAME        5
#define HTML_KEYGEN_STYLE       6
#define HTML_KEYGEN_TITLE       7
#define HTML_KEYGEN_ATTRIBUTES  8

#define HTML_LABEL_ACCESSKEY    0
#define HTML_LABEL_CLASS        1
#define HTML_LABEL_CLEAR        2
#define HTML_LABEL_DIR          3
#define HTML_LABEL_FOR          4
#define HTML_LABEL_ID           5
#define HTML_LABEL_LANG         6
#define HTML_LABEL_ONCLICK      7
#define HTML_LABEL_STYLE        8
#define HTML_LABEL_TITLE        9
#define HTML_LABEL_ATTRIBUTES  10

#define HTML_LEGEND_ACCESSKEY   0
#define HTML_LEGEND_ALIGN       1
#define HTML_LEGEND_CLASS       2
#define HTML_LEGEND_CLEAR       3
#define HTML_LEGEND_DIR         4
#define HTML_LEGEND_ID          5
#define HTML_LEGEND_LANG        6
#define HTML_LEGEND_STYLE       7
#define HTML_LEGEND_TITLE       8
#define HTML_LEGEND_ATTRIBUTES  9

#define HTML_LI_CLASS           0
#define HTML_LI_CLEAR           1
#define HTML_LI_DINGBAT         2
#define HTML_LI_DIR             3
#define HTML_LI_ID              4
#define HTML_LI_LANG            5
#define HTML_LI_MD              6
#define HTML_LI_SKIP            7
#define HTML_LI_SRC             8
#define HTML_LI_STYLE           9
#define HTML_LI_TITLE          10
#define HTML_LI_TYPE           11
#define HTML_LI_VALUE          12
#define HTML_LI_ATTRIBUTES     13

#define HTML_LINK_CHARSET       0 /* RFC 2070 HTML i18n - kw */
#define HTML_LINK_CLASS         1
#define HTML_LINK_HREF          2
#define HTML_LINK_ID            3
#define HTML_LINK_MEDIA         4
#define HTML_LINK_REL           5
#define HTML_LINK_REV           6
#define HTML_LINK_STYLE         7
#define HTML_LINK_TARGET        8
#define HTML_LINK_TITLE         9
#define HTML_LINK_TYPE         10
#define HTML_LINK_ATTRIBUTES   11

#define HTML_MAP_CLASS          0
#define HTML_MAP_CLEAR          1
#define HTML_MAP_DIR            2
#define HTML_MAP_ID             3
#define HTML_MAP_LANG           4
#define HTML_MAP_NAME           5
#define HTML_MAP_STYLE          6
#define HTML_MAP_TITLE          7
#define HTML_MAP_ATTRIBUTES     8

#define HTML_MATH_BOX           0
#define HTML_MATH_CLASS         1
#define HTML_MATH_CLEAR         2
#define HTML_MATH_DIR           3
#define HTML_MATH_ID            4
#define HTML_MATH_LANG          5
#define HTML_MATH_STYLE         6
#define HTML_MATH_TITLE         7
#define HTML_MATH_ATTRIBUTES    8

#define HTML_META_CONTENT       0
#define HTML_META_HTTP_EQUIV    1  /* For parsing in HTML.c - FM */
#define HTML_META_NAME          2
#define HTML_META_ATTRIBUTES    3

#define NEXTID_N                0

#define HTML_NOTE_CLASS         0
#define HTML_NOTE_CLEAR         1
#define HTML_NOTE_DIR           2
#define HTML_NOTE_ID            3
#define HTML_NOTE_LANG          4
#define HTML_NOTE_MD            5
#define HTML_NOTE_ROLE          6 /* Old name for CLASS - FM */
#define HTML_NOTE_SRC           7
#define HTML_NOTE_STYLE         8
#define HTML_NOTE_TITLE         9
#define HTML_NOTE_ATTRIBUTES   10

#define HTML_OBJECT_ALIGN       0
#define HTML_OBJECT_BORDER      1
#define HTML_OBJECT_CLASS       2
#define HTML_OBJECT_CLASSID     3
#define HTML_OBJECT_CODEBASE    4
#define HTML_OBJECT_CODETYPE    5
#define HTML_OBJECT_DATA        6
#define HTML_OBJECT_DECLARE     7
#define HTML_OBJECT_DIR         8
#define HTML_OBJECT_HEIGHT      9
#define HTML_OBJECT_HSPACE     10
#define HTML_OBJECT_ID         11
#define HTML_OBJECT_ISMAP      12
#define HTML_OBJECT_LANG       13
#define HTML_OBJECT_NAME       14
#define HTML_OBJECT_NOTAB      15
#define HTML_OBJECT_SHAPES     16
#define HTML_OBJECT_STANDBY    17
#define HTML_OBJECT_STYLE      18
#define HTML_OBJECT_TABINDEX   19
#define HTML_OBJECT_TITLE      20
#define HTML_OBJECT_TYPE       21
#define HTML_OBJECT_USEMAP     22
#define HTML_OBJECT_VSPACE     23
#define HTML_OBJECT_WIDTH      24
#define HTML_OBJECT_ATTRIBUTES 25

#define HTML_OL_CLASS           0
#define HTML_OL_CLEAR           1
#define HTML_OL_COMPACT         2
#define HTML_OL_CONTINUE        3
#define HTML_OL_DIR             4
#define HTML_OL_ID              5
#define HTML_OL_LANG            6
#define HTML_OL_SEQNUM          7
#define HTML_OL_START           8
#define HTML_OL_STYLE           9
#define HTML_OL_TITLE          10
#define HTML_OL_TYPE           11
#define HTML_OL_ATTRIBUTES     12

#define HTML_OPTION_CLASS       0
#define HTML_OPTION_CLEAR       1
#define HTML_OPTION_DIR         2
#define HTML_OPTION_DISABLED    3
#define HTML_OPTION_ERROR       4
#define HTML_OPTION_ID          5
#define HTML_OPTION_LANG        6
#define HTML_OPTION_SELECTED    7
#define HTML_OPTION_SHAPE       8
#define HTML_OPTION_STYLE       9
#define HTML_OPTION_TITLE      10
#define HTML_OPTION_VALUE      11
#define HTML_OPTION_ATTRIBUTES 12

#define HTML_OVERLAY_CLASS      0
#define HTML_OVERLAY_HEIGHT     1
#define HTML_OVERLAY_ID         2
#define HTML_OVERLAY_IMAGEMAP   3
#define HTML_OVERLAY_MD         4
#define HTML_OVERLAY_SRC        5
#define HTML_OVERLAY_STYLE      6
#define HTML_OVERLAY_TITLE      7
#define HTML_OVERLAY_UNITS      8
#define HTML_OVERLAY_WIDTH      9
#define HTML_OVERLAY_X         10
#define HTML_OVERLAY_Y         11
#define HTML_OVERLAY_ATTRIBUTES 12

#define HTML_P_ALIGN            0
#define HTML_P_CLASS            1
#define HTML_P_CLEAR            2
#define HTML_P_DIR              3
#define HTML_P_ID               4
#define HTML_P_LANG             5
#define HTML_P_NOWRAP           6
#define HTML_P_STYLE            7
#define HTML_P_TITLE            8
#define HTML_P_ATTRIBUTES       9

#define HTML_PARAM_ACCEPT       0
#define HTML_PARAM_ACCEPT_CHARSET  1
#define HTML_PARAM_ACCEPT_ENCODING 2
#define HTML_PARAM_CLASS        3
#define HTML_PARAM_CLEAR        4
#define HTML_PARAM_DATA         5
#define HTML_PARAM_DIR          6
#define HTML_PARAM_ID           7
#define HTML_PARAM_LANG         8
#define HTML_PARAM_NAME         9
#define HTML_PARAM_OBJECT      10
#define HTML_PARAM_REF         11
#define HTML_PARAM_STYLE       12
#define HTML_PARAM_TITLE       13
#define HTML_PARAM_TYPE        14
#define HTML_PARAM_VALUE       15
#define HTML_PARAM_VALUEREF    16  /* Use VALUETYPE (DATA|REF|OBJECT). - FM */
#define HTML_PARAM_VALUETYPE   17
#define HTML_PARAM_ATTRIBUTES  18

#define HTML_SCRIPT_CLASS       0
#define HTML_SCRIPT_CLEAR       1
#define HTML_SCRIPT_DIR         2
#define HTML_SCRIPT_EVENT       3
#define HTML_SCRIPT_FOR         4
#define HTML_SCRIPT_ID          5
#define HTML_SCRIPT_LANG        6
#define HTML_SCRIPT_LANGUAGE    7
#define HTML_SCRIPT_NAME        8
#define HTML_SCRIPT_SCRIPTENGINE 9
#define HTML_SCRIPT_SRC        10
#define HTML_SCRIPT_STYLE      11
#define HTML_SCRIPT_TITLE      12
#define HTML_SCRIPT_TYPE       13
#define HTML_SCRIPT_ATTRIBUTES 14

#define HTML_SELECT_ALIGN       0
#define HTML_SELECT_CLASS       1
#define HTML_SELECT_CLEAR       2
#define HTML_SELECT_DIR         3
#define HTML_SELECT_DISABLED    4
#define HTML_SELECT_ERROR       5
#define HTML_SELECT_HEIGHT      6
#define HTML_SELECT_ID          7
#define HTML_SELECT_LANG        8
#define HTML_SELECT_MD          9
#define HTML_SELECT_MULTIPLE   10
#define HTML_SELECT_NAME       11
#define HTML_SELECT_NOTAB      12
#define HTML_SELECT_ONBLUR     13
#define HTML_SELECT_ONCHANGE   14
#define HTML_SELECT_ONFOCUS    15
#define HTML_SELECT_SIZE       16
#define HTML_SELECT_STYLE      17
#define HTML_SELECT_TABINDEX   18
#define HTML_SELECT_TITLE      19
#define HTML_SELECT_UNITS      20
#define HTML_SELECT_WIDTH      21
#define HTML_SELECT_ATTRIBUTES 22

#define HTML_STYLE_DIR          0
#define HTML_STYLE_LANG         1
#define HTML_STYLE_NOTATION     2
#define HTML_STYLE_TITLE        3
#define HTML_STYLE_ATTRIBUTES   4

#define HTML_TAB_ALIGN          0
#define HTML_TAB_CLASS          1
#define HTML_TAB_CLEAR          2
#define HTML_TAB_DIR            3
#define HTML_TAB_DP             4
#define HTML_TAB_ID             5
#define HTML_TAB_INDENT         6
#define HTML_TAB_LANG           7
#define HTML_TAB_STYLE          8
#define HTML_TAB_TITLE          9
#define HTML_TAB_TO            10
#define HTML_TAB_ATTRIBUTES    11

#define HTML_TABLE_ALIGN        0
#define HTML_TABLE_BACKGROUND   1
#define HTML_TABLE_BORDER       2
#define HTML_TABLE_CELLPADDING  3
#define HTML_TABLE_CELLSPACING  4
#define HTML_TABLE_CLASS        5
#define HTML_TABLE_CLEAR        6
#define HTML_TABLE_COLS         7
#define HTML_TABLE_COLSPEC      8
#define HTML_TABLE_DIR          9
#define HTML_TABLE_DP          10
#define HTML_TABLE_FRAME       11
#define HTML_TABLE_ID          12
#define HTML_TABLE_LANG        13
#define HTML_TABLE_NOFLOW      14
#define HTML_TABLE_NOWRAP      15
#define HTML_TABLE_RULES       16
#define HTML_TABLE_STYLE       17
#define HTML_TABLE_SUMMARY     18
#define HTML_TABLE_TITLE       19
#define HTML_TABLE_UNITS       20
#define HTML_TABLE_WIDTH       21
#define HTML_TABLE_ATTRIBUTES  22

#define HTML_TD_ALIGN           0
#define HTML_TD_AXES            1
#define HTML_TD_AXIS            2
#define HTML_TD_BACKGROUND      3
#define HTML_TD_CHAR            4
#define HTML_TD_CHAROFF         5
#define HTML_TD_CLASS           6
#define HTML_TD_CLEAR           7
#define HTML_TD_COLSPAN         8
#define HTML_TD_DIR             9
#define HTML_TD_DP             10
#define HTML_TD_HEIGHT         11
#define HTML_TD_ID             12
#define HTML_TD_LANG           13
#define HTML_TD_NOWRAP         14
#define HTML_TD_ROWSPAN        15
#define HTML_TD_STYLE          16
#define HTML_TD_TITLE          17
#define HTML_TD_VALIGN         18
#define HTML_TD_WIDTH          19
#define HTML_TD_ATTRIBUTES     20

#define HTML_TEXTAREA_ACCEPT_CHARSET  0 /* RFC 2070 HTML i18n - kw */
#define HTML_TEXTAREA_ALIGN     1
#define HTML_TEXTAREA_CLASS     2
#define HTML_TEXTAREA_CLEAR     3
#define HTML_TEXTAREA_COLS      4
#define HTML_TEXTAREA_DIR       5
#define HTML_TEXTAREA_DISABLED  6
#define HTML_TEXTAREA_ERROR     7
#define HTML_TEXTAREA_ID        8
#define HTML_TEXTAREA_LANG      9
#define HTML_TEXTAREA_NAME     10
#define HTML_TEXTAREA_NOTAB    11
#define HTML_TEXTAREA_ONBLUR   12
#define HTML_TEXTAREA_ONCHANGE 13
#define HTML_TEXTAREA_ONFOCUS  14
#define HTML_TEXTAREA_ONSELECT 15
#define HTML_TEXTAREA_ROWS     16
#define HTML_TEXTAREA_STYLE    17
#define HTML_TEXTAREA_TABINDEX 18
#define HTML_TEXTAREA_TITLE    19
#define HTML_TEXTAREA_ATTRIBUTES 20

#define HTML_TR_ALIGN           0
#define HTML_TR_CHAR            1
#define HTML_TR_CHAROFF         2
#define HTML_TR_CLASS           3
#define HTML_TR_CLEAR           4
#define HTML_TR_DIR             5
#define HTML_TR_DP              6
#define HTML_TR_ID              7
#define HTML_TR_LANG            8
#define HTML_TR_NOWRAP          9
#define HTML_TR_STYLE          10
#define HTML_TR_TITLE          11
#define HTML_TR_VALIGN         12
#define HTML_TR_ATTRIBUTES     13

#define HTML_UL_CLASS           0
#define HTML_UL_CLEAR           1
#define HTML_UL_COMPACT         2
#define HTML_UL_DINGBAT         3
#define HTML_UL_DIR             4
#define HTML_UL_ID              5
#define HTML_UL_LANG            6
#define HTML_UL_MD              7
#define HTML_UL_PLAIN           8
#define HTML_UL_SRC             9
#define HTML_UL_STYLE          10
#define HTML_UL_TITLE          11
#define HTML_UL_TYPE           12
#define HTML_UL_WRAP           13
#define HTML_UL_ATTRIBUTES     14

#ifdef USE_PRETTYSRC
/* values of HTML attributes' types */
#define HTMLA_NORMAL 0 /* nothing specific */
#define HTMLA_ANAME  1 /* anchor name - 'id' or a's 'name' */
#define HTMLA_HREF   2 /* href */
#define HTMLA_CLASS  4 /* class name.  */
#define HTMLA_AUXCLASS 8 /* attribute, the value of which also designates
			    a class name */
#endif
extern CONST SGML_dtd HTML_dtd;

extern void HTSwitchDTD PARAMS((int new_flag));

extern HTTag HTTag_unrecognized;
extern HTTag HTTag_mixedObject;

/*

Start anchor element

   It is kinda convenient to have a particular routine for starting an anchor
   element, as everything else for HTML is simple anyway.

  ON ENTRY

   targetstream points to a structured stream object.

   name and href point to attribute strings or are NULL if the attribute is
   to be omitted.

 */
extern void HTStartAnchor PARAMS((
		HTStructured * targetstream,
		CONST char *	name,
		CONST char *	href));

extern void HTStartAnchor5 PARAMS((
		HTStructured * targetstream,
		CONST char *	name,
		CONST char *	href,
		CONST char *	linktype,
		int		tag_charset));

/*

Start IsIndex element - FM

   It is kinda convenient to have a particular routine for starting an IsIndex
   element with the prompt and/or href (action) attributes specified.

  ON ENTRY

   targetstream points to a structured stream object.

   prompt and href point to attribute strings or are NULL if the attribute is
   to be omitted.

 */
extern void HTStartIsIndex PARAMS((
		HTStructured * targetstream,
		CONST char *	prompt,
		CONST char *	href));


#endif /* HTMLDTD_H */

/*

   End of module definition  */
