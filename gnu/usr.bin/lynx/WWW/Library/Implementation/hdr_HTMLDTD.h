/* $LynxId: hdr_HTMLDTD.h,v 1.20 2009/04/12 01:15:23 tom Exp $ */
#ifndef hdr_HTMLDTD_H
#define hdr_HTMLDTD_H 1

#ifdef __cplusplus
extern "C" {
#endif
/*

   Element Numbers

   Must Match all tables by element!
   These include tables in HTMLDTD.c
   and code in HTML.c.

 */
    typedef enum {
	HTML_A,
	HTML_ABBR,
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
	HTML_ALT_OBJECT
    } HTMLElement;

/* Notes: HTML.c uses a different extension of the
          HTML_ELEMENTS space privately, see
          HTNestedList.h.

   Do NOT replace HTML_ELEMENTS with
   TABLESIZE(mumble_dtd.tags).

   Keep the following defines in synch with
   the above enum!
 */

/* # of elements generally visible to Lynx code */
#define HTML_ELEMENTS 118

/* # of elements visible to SGML parser */
#define HTML_ALL_ELEMENTS 119

/*

   Attribute numbers

   Identifier is HTML_<element>_<attribute>.
   These must match the tables in HTML.c!

 */
#define HTML_A_ACCESSKEY          0
#define HTML_A_CHARSET            1
#define HTML_A_CLASS              2
#define HTML_A_CLEAR              3
#define HTML_A_COORDS             4
#define HTML_A_DIR                5
#define HTML_A_HREF               6
#define HTML_A_HREFLANG           7
#define HTML_A_ID                 8
#define HTML_A_ISMAP              9
#define HTML_A_LANG              10
#define HTML_A_MD                11
#define HTML_A_NAME              12
#define HTML_A_NOTAB             13
#define HTML_A_ONBLUR            14
#define HTML_A_ONFOCUS           15
#define HTML_A_REL               16
#define HTML_A_REV               17
#define HTML_A_SHAPE             18
#define HTML_A_STYLE             19
#define HTML_A_TABINDEX          20
#define HTML_A_TARGET            21
#define HTML_A_TITLE             22
#define HTML_A_TYPE              23
#define HTML_A_URN               24
#define HTML_A_ATTRIBUTES        25

#define HTML_ADDRESS_CLASS        0
#define HTML_ADDRESS_CLEAR        1
#define HTML_ADDRESS_DIR          2
#define HTML_ADDRESS_ID           3
#define HTML_ADDRESS_LANG         4
#define HTML_ADDRESS_NOWRAP       5
#define HTML_ADDRESS_STYLE        6
#define HTML_ADDRESS_TITLE        7
#define HTML_ADDRESS_ATTRIBUTES   8

#define HTML_APPLET_ALIGN         0
#define HTML_APPLET_ALT           1
#define HTML_APPLET_CLASS         2
#define HTML_APPLET_CLEAR         3
#define HTML_APPLET_CODE          4
#define HTML_APPLET_CODEBASE      5
#define HTML_APPLET_DIR           6
#define HTML_APPLET_DOWNLOAD      7
#define HTML_APPLET_HEIGHT        8
#define HTML_APPLET_HSPACE        9
#define HTML_APPLET_ID           10
#define HTML_APPLET_LANG         11
#define HTML_APPLET_NAME         12
#define HTML_APPLET_STYLE        13
#define HTML_APPLET_TITLE        14
#define HTML_APPLET_VSPACE       15
#define HTML_APPLET_WIDTH        16
#define HTML_APPLET_ATTRIBUTES   17

#define HTML_AREA_ACCESSKEY       0
#define HTML_AREA_ALT             1
#define HTML_AREA_CLASS           2
#define HTML_AREA_CLEAR           3
#define HTML_AREA_COORDS          4
#define HTML_AREA_DIR             5
#define HTML_AREA_HREF            6
#define HTML_AREA_ID              7
#define HTML_AREA_LANG            8
#define HTML_AREA_NOHREF          9
#define HTML_AREA_NOTAB          10
#define HTML_AREA_ONBLUR         11
#define HTML_AREA_ONFOCUS        12
#define HTML_AREA_SHAPE          13
#define HTML_AREA_STYLE          14
#define HTML_AREA_TABINDEX       15
#define HTML_AREA_TARGET         16
#define HTML_AREA_TITLE          17
#define HTML_AREA_ATTRIBUTES     18

#define HTML_BASE_CLASS           0
#define HTML_BASE_HREF            1
#define HTML_BASE_ID              2
#define HTML_BASE_STYLE           3
#define HTML_BASE_TARGET          4
#define HTML_BASE_TITLE           5
#define HTML_BASE_ATTRIBUTES      6

#define HTML_BGSOUND_CLASS        0
#define HTML_BGSOUND_CLEAR        1
#define HTML_BGSOUND_DIR          2
#define HTML_BGSOUND_ID           3
#define HTML_BGSOUND_LANG         4
#define HTML_BGSOUND_LOOP         5
#define HTML_BGSOUND_SRC          6
#define HTML_BGSOUND_STYLE        7
#define HTML_BGSOUND_TITLE        8
#define HTML_BGSOUND_ATTRIBUTES   9

#define HTML_BODY_ALINK           0
#define HTML_BODY_BACKGROUND      1
#define HTML_BODY_BGCOLOR         2
#define HTML_BODY_CLASS           3
#define HTML_BODY_CLEAR           4
#define HTML_BODY_DIR             5
#define HTML_BODY_ID              6
#define HTML_BODY_LANG            7
#define HTML_BODY_LINK            8
#define HTML_BODY_ONLOAD          9
#define HTML_BODY_ONUNLOAD       10
#define HTML_BODY_STYLE          11
#define HTML_BODY_TEXT           12
#define HTML_BODY_TITLE          13
#define HTML_BODY_VLINK          14
#define HTML_BODY_ATTRIBUTES     15

#define HTML_BODYTEXT_CLASS       0
#define HTML_BODYTEXT_CLEAR       1
#define HTML_BODYTEXT_DATA        2
#define HTML_BODYTEXT_DIR         3
#define HTML_BODYTEXT_ID          4
#define HTML_BODYTEXT_LANG        5
#define HTML_BODYTEXT_NAME        6
#define HTML_BODYTEXT_OBJECT      7
#define HTML_BODYTEXT_REF         8
#define HTML_BODYTEXT_STYLE       9
#define HTML_BODYTEXT_TITLE      10
#define HTML_BODYTEXT_TYPE       11
#define HTML_BODYTEXT_VALUE      12
#define HTML_BODYTEXT_VALUETYPE  13
#define HTML_BODYTEXT_ATTRIBUTES 14

#define HTML_BQ_CITE              0
#define HTML_BQ_CLASS             1
#define HTML_BQ_CLEAR             2
#define HTML_BQ_DIR               3
#define HTML_BQ_ID                4
#define HTML_BQ_LANG              5
#define HTML_BQ_NOWRAP            6
#define HTML_BQ_STYLE             7
#define HTML_BQ_TITLE             8
#define HTML_BQ_ATTRIBUTES        9

#define HTML_BUTTON_ACCESSKEY     0
#define HTML_BUTTON_CLASS         1
#define HTML_BUTTON_CLEAR         2
#define HTML_BUTTON_DIR           3
#define HTML_BUTTON_DISABLED      4
#define HTML_BUTTON_ID            5
#define HTML_BUTTON_LANG          6
#define HTML_BUTTON_NAME          7
#define HTML_BUTTON_ONBLUR        8
#define HTML_BUTTON_ONFOCUS       9
#define HTML_BUTTON_STYLE        10
#define HTML_BUTTON_TABINDEX     11
#define HTML_BUTTON_TITLE        12
#define HTML_BUTTON_TYPE         13
#define HTML_BUTTON_VALUE        14
#define HTML_BUTTON_ATTRIBUTES   15

#define HTML_CAPTION_ACCESSKEY    0
#define HTML_CAPTION_ALIGN        1
#define HTML_CAPTION_CLASS        2
#define HTML_CAPTION_CLEAR        3
#define HTML_CAPTION_DIR          4
#define HTML_CAPTION_ID           5
#define HTML_CAPTION_LANG         6
#define HTML_CAPTION_STYLE        7
#define HTML_CAPTION_TITLE        8
#define HTML_CAPTION_ATTRIBUTES   9

#define HTML_COL_ALIGN            0
#define HTML_COL_CHAR             1
#define HTML_COL_CHAROFF          2
#define HTML_COL_CLASS            3
#define HTML_COL_CLEAR            4
#define HTML_COL_DIR              5
#define HTML_COL_ID               6
#define HTML_COL_LANG             7
#define HTML_COL_SPAN             8
#define HTML_COL_STYLE            9
#define HTML_COL_TITLE           10
#define HTML_COL_VALIGN          11
#define HTML_COL_WIDTH           12
#define HTML_COL_ATTRIBUTES      13

#define HTML_DEL_CITE             0
#define HTML_DEL_CLASS            1
#define HTML_DEL_DATETIME         2
#define HTML_DEL_DIR              3
#define HTML_DEL_ID               4
#define HTML_DEL_LANG             5
#define HTML_DEL_STYLE            6
#define HTML_DEL_TITLE            7
#define HTML_DEL_ATTRIBUTES       8

#define HTML_DIV_ALIGN            0
#define HTML_DIV_CLASS            1
#define HTML_DIV_CLEAR            2
#define HTML_DIV_DIR              3
#define HTML_DIV_ID               4
#define HTML_DIV_LANG             5
#define HTML_DIV_STYLE            6
#define HTML_DIV_TITLE            7
#define HTML_DIV_ATTRIBUTES       8

#define HTML_DL_CLASS             0
#define HTML_DL_CLEAR             1
#define HTML_DL_COMPACT           2
#define HTML_DL_DIR               3
#define HTML_DL_ID                4
#define HTML_DL_LANG              5
#define HTML_DL_STYLE             6
#define HTML_DL_TITLE             7
#define HTML_DL_ATTRIBUTES        8

#define HTML_EMBED_ALIGN          0
#define HTML_EMBED_ALT            1
#define HTML_EMBED_BORDER         2
#define HTML_EMBED_CLASS          3
#define HTML_EMBED_CLEAR          4
#define HTML_EMBED_DIR            5
#define HTML_EMBED_HEIGHT         6
#define HTML_EMBED_ID             7
#define HTML_EMBED_IMAGEMAP       8
#define HTML_EMBED_ISMAP          9
#define HTML_EMBED_LANG          10
#define HTML_EMBED_MD            11
#define HTML_EMBED_NAME          12
#define HTML_EMBED_NOFLOW        13
#define HTML_EMBED_PARAMS        14
#define HTML_EMBED_SRC           15
#define HTML_EMBED_STYLE         16
#define HTML_EMBED_TITLE         17
#define HTML_EMBED_UNITS         18
#define HTML_EMBED_USEMAP        19
#define HTML_EMBED_WIDTH         20
#define HTML_EMBED_ATTRIBUTES    21

#define HTML_FIG_ALIGN            0
#define HTML_FIG_BORDER           1
#define HTML_FIG_CLASS            2
#define HTML_FIG_CLEAR            3
#define HTML_FIG_DIR              4
#define HTML_FIG_HEIGHT           5
#define HTML_FIG_ID               6
#define HTML_FIG_IMAGEMAP         7
#define HTML_FIG_ISOBJECT         8
#define HTML_FIG_LANG             9
#define HTML_FIG_MD              10
#define HTML_FIG_NOFLOW          11
#define HTML_FIG_SRC             12
#define HTML_FIG_STYLE           13
#define HTML_FIG_TITLE           14
#define HTML_FIG_UNITS           15
#define HTML_FIG_WIDTH           16
#define HTML_FIG_ATTRIBUTES      17

#define HTML_FONT_CLASS           0
#define HTML_FONT_CLEAR           1
#define HTML_FONT_COLOR           2
#define HTML_FONT_DIR             3
#define HTML_FONT_END             4
#define HTML_FONT_FACE            5
#define HTML_FONT_ID              6
#define HTML_FONT_LANG            7
#define HTML_FONT_SIZE            8
#define HTML_FONT_STYLE           9
#define HTML_FONT_TITLE          10
#define HTML_FONT_ATTRIBUTES     11

#define HTML_FORM_ACCEPT          0
#define HTML_FORM_ACCEPT_CHARSET  1
#define HTML_FORM_ACTION          2
#define HTML_FORM_CLASS           3
#define HTML_FORM_CLEAR           4
#define HTML_FORM_DIR             5
#define HTML_FORM_ENCTYPE         6
#define HTML_FORM_ID              7
#define HTML_FORM_LANG            8
#define HTML_FORM_METHOD          9
#define HTML_FORM_ONRESET        10
#define HTML_FORM_ONSUBMIT       11
#define HTML_FORM_SCRIPT         12
#define HTML_FORM_STYLE          13
#define HTML_FORM_SUBJECT        14
#define HTML_FORM_TARGET         15
#define HTML_FORM_TITLE          16
#define HTML_FORM_ATTRIBUTES     17

#define HTML_FRAME_CLASS          0
#define HTML_FRAME_FRAMEBORDER    1
#define HTML_FRAME_ID             2
#define HTML_FRAME_LONGDESC       3
#define HTML_FRAME_MARGINHEIGHT   4
#define HTML_FRAME_MARGINWIDTH    5
#define HTML_FRAME_NAME           6
#define HTML_FRAME_NORESIZE       7
#define HTML_FRAME_SCROLLING      8
#define HTML_FRAME_SRC            9
#define HTML_FRAME_STYLE         10
#define HTML_FRAME_TITLE         11
#define HTML_FRAME_ATTRIBUTES    12

#define HTML_FRAMESET_COLS        0
#define HTML_FRAMESET_ONLOAD      1
#define HTML_FRAMESET_ONUNLOAD    2
#define HTML_FRAMESET_ROWS        3
#define HTML_FRAMESET_ATTRIBUTES  4

#define HTML_GEN_CLASS            0
#define HTML_GEN_CLEAR            1
#define HTML_GEN_DIR              2
#define HTML_GEN_ID               3
#define HTML_GEN_LANG             4
#define HTML_GEN_STYLE            5
#define HTML_GEN_TITLE            6
#define HTML_GEN_ATTRIBUTES       7

#define HTML_H_ALIGN              0
#define HTML_H_CLASS              1
#define HTML_H_CLEAR              2
#define HTML_H_DINGBAT            3
#define HTML_H_DIR                4
#define HTML_H_ID                 5
#define HTML_H_LANG               6
#define HTML_H_MD                 7
#define HTML_H_NOWRAP             8
#define HTML_H_SEQNUM             9
#define HTML_H_SKIP              10
#define HTML_H_SRC               11
#define HTML_H_STYLE             12
#define HTML_H_TITLE             13
#define HTML_H_ATTRIBUTES        14

#define HTML_HR_ALIGN             0
#define HTML_HR_CLASS             1
#define HTML_HR_CLEAR             2
#define HTML_HR_DIR               3
#define HTML_HR_ID                4
#define HTML_HR_LANG              5
#define HTML_HR_MD                6
#define HTML_HR_NOSHADE           7
#define HTML_HR_SIZE              8
#define HTML_HR_SRC               9
#define HTML_HR_STYLE            10
#define HTML_HR_TITLE            11
#define HTML_HR_WIDTH            12
#define HTML_HR_ATTRIBUTES       13

#define HTML_IFRAME_ALIGN         0
#define HTML_IFRAME_CLASS         1
#define HTML_IFRAME_FRAMEBORDER   2
#define HTML_IFRAME_HEIGHT        3
#define HTML_IFRAME_ID            4
#define HTML_IFRAME_LONGDESC      5
#define HTML_IFRAME_MARGINHEIGHT  6
#define HTML_IFRAME_MARGINWIDTH   7
#define HTML_IFRAME_NAME          8
#define HTML_IFRAME_SCROLLING     9
#define HTML_IFRAME_SRC          10
#define HTML_IFRAME_STYLE        11
#define HTML_IFRAME_TITLE        12
#define HTML_IFRAME_WIDTH        13
#define HTML_IFRAME_ATTRIBUTES   14

#define HTML_IMG_ALIGN            0
#define HTML_IMG_ALT              1
#define HTML_IMG_BORDER           2
#define HTML_IMG_CLASS            3
#define HTML_IMG_CLEAR            4
#define HTML_IMG_DIR              5
#define HTML_IMG_HEIGHT           6
#define HTML_IMG_HSPACE           7
#define HTML_IMG_ID               8
#define HTML_IMG_ISMAP            9
#define HTML_IMG_ISOBJECT        10
#define HTML_IMG_LANG            11
#define HTML_IMG_LONGDESC        12
#define HTML_IMG_MD              13
#define HTML_IMG_NAME            14
#define HTML_IMG_SRC             15
#define HTML_IMG_STYLE           16
#define HTML_IMG_TITLE           17
#define HTML_IMG_UNITS           18
#define HTML_IMG_USEMAP          19
#define HTML_IMG_VSPACE          20
#define HTML_IMG_WIDTH           21
#define HTML_IMG_ATTRIBUTES      22

#define HTML_INPUT_ACCEPT         0
#define HTML_INPUT_ACCEPT_CHARSET 1
#define HTML_INPUT_ACCESSKEY      2
#define HTML_INPUT_ALIGN          3
#define HTML_INPUT_ALT            4
#define HTML_INPUT_CHECKED        5
#define HTML_INPUT_CLASS          6
#define HTML_INPUT_CLEAR          7
#define HTML_INPUT_DIR            8
#define HTML_INPUT_DISABLED       9
#define HTML_INPUT_ERROR         10
#define HTML_INPUT_HEIGHT        11
#define HTML_INPUT_ID            12
#define HTML_INPUT_ISMAP         13
#define HTML_INPUT_LANG          14
#define HTML_INPUT_MAX           15
#define HTML_INPUT_MAXLENGTH     16
#define HTML_INPUT_MD            17
#define HTML_INPUT_MIN           18
#define HTML_INPUT_NAME          19
#define HTML_INPUT_NOTAB         20
#define HTML_INPUT_ONBLUR        21
#define HTML_INPUT_ONCHANGE      22
#define HTML_INPUT_ONFOCUS       23
#define HTML_INPUT_ONSELECT      24
#define HTML_INPUT_READONLY      25
#define HTML_INPUT_SIZE          26
#define HTML_INPUT_SRC           27
#define HTML_INPUT_STYLE         28
#define HTML_INPUT_TABINDEX      29
#define HTML_INPUT_TITLE         30
#define HTML_INPUT_TYPE          31
#define HTML_INPUT_USEMAP        32
#define HTML_INPUT_VALUE         33
#define HTML_INPUT_WIDTH         34
#define HTML_INPUT_ATTRIBUTES    35

#define HTML_ISINDEX_ACTION       0
#define HTML_ISINDEX_CLASS        1
#define HTML_ISINDEX_DIR          2
#define HTML_ISINDEX_HREF         3
#define HTML_ISINDEX_ID           4
#define HTML_ISINDEX_LANG         5
#define HTML_ISINDEX_PROMPT       6
#define HTML_ISINDEX_STYLE        7
#define HTML_ISINDEX_TITLE        8
#define HTML_ISINDEX_ATTRIBUTES   9

#define HTML_KEYGEN_CHALLENGE     0
#define HTML_KEYGEN_CLASS         1
#define HTML_KEYGEN_DIR           2
#define HTML_KEYGEN_ID            3
#define HTML_KEYGEN_LANG          4
#define HTML_KEYGEN_NAME          5
#define HTML_KEYGEN_STYLE         6
#define HTML_KEYGEN_TITLE         7
#define HTML_KEYGEN_ATTRIBUTES    8

#define HTML_LABEL_ACCESSKEY      0
#define HTML_LABEL_CLASS          1
#define HTML_LABEL_CLEAR          2
#define HTML_LABEL_DIR            3
#define HTML_LABEL_FOR            4
#define HTML_LABEL_ID             5
#define HTML_LABEL_LANG           6
#define HTML_LABEL_ONBLUR         7
#define HTML_LABEL_ONFOCUS        8
#define HTML_LABEL_STYLE          9
#define HTML_LABEL_TITLE         10
#define HTML_LABEL_ATTRIBUTES    11

#define HTML_LI_CLASS             0
#define HTML_LI_CLEAR             1
#define HTML_LI_DINGBAT           2
#define HTML_LI_DIR               3
#define HTML_LI_ID                4
#define HTML_LI_LANG              5
#define HTML_LI_MD                6
#define HTML_LI_SKIP              7
#define HTML_LI_SRC               8
#define HTML_LI_STYLE             9
#define HTML_LI_TITLE            10
#define HTML_LI_TYPE             11
#define HTML_LI_VALUE            12
#define HTML_LI_ATTRIBUTES       13

#define HTML_LINK_CHARSET         0
#define HTML_LINK_CLASS           1
#define HTML_LINK_DIR             2
#define HTML_LINK_HREF            3
#define HTML_LINK_HREFLANG        4
#define HTML_LINK_ID              5
#define HTML_LINK_LANG            6
#define HTML_LINK_MEDIA           7
#define HTML_LINK_REL             8
#define HTML_LINK_REV             9
#define HTML_LINK_STYLE          10
#define HTML_LINK_TARGET         11
#define HTML_LINK_TITLE          12
#define HTML_LINK_TYPE           13
#define HTML_LINK_ATTRIBUTES     14

#define HTML_MAP_CLASS            0
#define HTML_MAP_CLEAR            1
#define HTML_MAP_DIR              2
#define HTML_MAP_ID               3
#define HTML_MAP_LANG             4
#define HTML_MAP_NAME             5
#define HTML_MAP_STYLE            6
#define HTML_MAP_TITLE            7
#define HTML_MAP_ATTRIBUTES       8

#define HTML_MATH_BOX             0
#define HTML_MATH_CLASS           1
#define HTML_MATH_CLEAR           2
#define HTML_MATH_DIR             3
#define HTML_MATH_ID              4
#define HTML_MATH_LANG            5
#define HTML_MATH_STYLE           6
#define HTML_MATH_TITLE           7
#define HTML_MATH_ATTRIBUTES      8

#define HTML_META_CONTENT         0
#define HTML_META_HTTP_EQUIV      1
#define HTML_META_NAME            2
#define HTML_META_SCHEME          3
#define HTML_META_ATTRIBUTES      4

#define HTML_NEXTID_N             0
#define HTML_NEXTID_ATTRIBUTES    1

#define HTML_NOTE_CLASS           0
#define HTML_NOTE_CLEAR           1
#define HTML_NOTE_DIR             2
#define HTML_NOTE_ID              3
#define HTML_NOTE_LANG            4
#define HTML_NOTE_MD              5
#define HTML_NOTE_ROLE            6
#define HTML_NOTE_SRC             7
#define HTML_NOTE_STYLE           8
#define HTML_NOTE_TITLE           9
#define HTML_NOTE_ATTRIBUTES     10

#define HTML_OBJECT_ALIGN         0
#define HTML_OBJECT_ARCHIVE       1
#define HTML_OBJECT_BORDER        2
#define HTML_OBJECT_CLASS         3
#define HTML_OBJECT_CLASSID       4
#define HTML_OBJECT_CODEBASE      5
#define HTML_OBJECT_CODETYPE      6
#define HTML_OBJECT_DATA          7
#define HTML_OBJECT_DECLARE       8
#define HTML_OBJECT_DIR           9
#define HTML_OBJECT_HEIGHT       10
#define HTML_OBJECT_HSPACE       11
#define HTML_OBJECT_ID           12
#define HTML_OBJECT_ISMAP        13
#define HTML_OBJECT_LANG         14
#define HTML_OBJECT_NAME         15
#define HTML_OBJECT_NOTAB        16
#define HTML_OBJECT_SHAPES       17
#define HTML_OBJECT_STANDBY      18
#define HTML_OBJECT_STYLE        19
#define HTML_OBJECT_TABINDEX     20
#define HTML_OBJECT_TITLE        21
#define HTML_OBJECT_TYPE         22
#define HTML_OBJECT_USEMAP       23
#define HTML_OBJECT_VSPACE       24
#define HTML_OBJECT_WIDTH        25
#define HTML_OBJECT_ATTRIBUTES   26

#define HTML_OL_CLASS             0
#define HTML_OL_CLEAR             1
#define HTML_OL_COMPACT           2
#define HTML_OL_CONTINUE          3
#define HTML_OL_DIR               4
#define HTML_OL_ID                5
#define HTML_OL_LANG              6
#define HTML_OL_SEQNUM            7
#define HTML_OL_START             8
#define HTML_OL_STYLE             9
#define HTML_OL_TITLE            10
#define HTML_OL_TYPE             11
#define HTML_OL_ATTRIBUTES       12

#define HTML_OPTION_CLASS         0
#define HTML_OPTION_CLEAR         1
#define HTML_OPTION_DIR           2
#define HTML_OPTION_DISABLED      3
#define HTML_OPTION_ERROR         4
#define HTML_OPTION_ID            5
#define HTML_OPTION_LABEL         6
#define HTML_OPTION_LANG          7
#define HTML_OPTION_SELECTED      8
#define HTML_OPTION_SHAPE         9
#define HTML_OPTION_STYLE        10
#define HTML_OPTION_TITLE        11
#define HTML_OPTION_VALUE        12
#define HTML_OPTION_ATTRIBUTES   13

#define HTML_OVERLAY_CLASS        0
#define HTML_OVERLAY_HEIGHT       1
#define HTML_OVERLAY_ID           2
#define HTML_OVERLAY_IMAGEMAP     3
#define HTML_OVERLAY_MD           4
#define HTML_OVERLAY_SRC          5
#define HTML_OVERLAY_STYLE        6
#define HTML_OVERLAY_TITLE        7
#define HTML_OVERLAY_UNITS        8
#define HTML_OVERLAY_WIDTH        9
#define HTML_OVERLAY_X           10
#define HTML_OVERLAY_Y           11
#define HTML_OVERLAY_ATTRIBUTES  12

#define HTML_P_ALIGN              0
#define HTML_P_CLASS              1
#define HTML_P_CLEAR              2
#define HTML_P_DIR                3
#define HTML_P_ID                 4
#define HTML_P_LANG               5
#define HTML_P_NOWRAP             6
#define HTML_P_STYLE              7
#define HTML_P_TITLE              8
#define HTML_P_ATTRIBUTES         9

#define HTML_PARAM_ACCEPT         0
#define HTML_PARAM_ACCEPT_CHARSET 1
#define HTML_PARAM_ACCEPT_ENCODING 2
#define HTML_PARAM_CLASS          3
#define HTML_PARAM_CLEAR          4
#define HTML_PARAM_DATA           5
#define HTML_PARAM_DIR            6
#define HTML_PARAM_ID             7
#define HTML_PARAM_LANG           8
#define HTML_PARAM_NAME           9
#define HTML_PARAM_OBJECT        10
#define HTML_PARAM_REF           11
#define HTML_PARAM_STYLE         12
#define HTML_PARAM_TITLE         13
#define HTML_PARAM_TYPE          14
#define HTML_PARAM_VALUE         15
#define HTML_PARAM_VALUEREF      16
#define HTML_PARAM_VALUETYPE     17
#define HTML_PARAM_ATTRIBUTES    18

#define HTML_Q_CITE               0
#define HTML_Q_CLASS              1
#define HTML_Q_CLEAR              2
#define HTML_Q_DIR                3
#define HTML_Q_ID                 4
#define HTML_Q_LANG               5
#define HTML_Q_STYLE              6
#define HTML_Q_TITLE              7
#define HTML_Q_ATTRIBUTES         8

#define HTML_SCRIPT_CHARSET       0
#define HTML_SCRIPT_CLASS         1
#define HTML_SCRIPT_CLEAR         2
#define HTML_SCRIPT_DEFER         3
#define HTML_SCRIPT_DIR           4
#define HTML_SCRIPT_EVENT         5
#define HTML_SCRIPT_FOR           6
#define HTML_SCRIPT_ID            7
#define HTML_SCRIPT_LANG          8
#define HTML_SCRIPT_LANGUAGE      9
#define HTML_SCRIPT_NAME         10
#define HTML_SCRIPT_SCRIPTENGINE 11
#define HTML_SCRIPT_SRC          12
#define HTML_SCRIPT_STYLE        13
#define HTML_SCRIPT_TITLE        14
#define HTML_SCRIPT_TYPE         15
#define HTML_SCRIPT_ATTRIBUTES   16

#define HTML_SELECT_ALIGN         0
#define HTML_SELECT_CLASS         1
#define HTML_SELECT_CLEAR         2
#define HTML_SELECT_DIR           3
#define HTML_SELECT_DISABLED      4
#define HTML_SELECT_ERROR         5
#define HTML_SELECT_HEIGHT        6
#define HTML_SELECT_ID            7
#define HTML_SELECT_LANG          8
#define HTML_SELECT_MD            9
#define HTML_SELECT_MULTIPLE     10
#define HTML_SELECT_NAME         11
#define HTML_SELECT_NOTAB        12
#define HTML_SELECT_ONBLUR       13
#define HTML_SELECT_ONCHANGE     14
#define HTML_SELECT_ONFOCUS      15
#define HTML_SELECT_SIZE         16
#define HTML_SELECT_STYLE        17
#define HTML_SELECT_TABINDEX     18
#define HTML_SELECT_TITLE        19
#define HTML_SELECT_UNITS        20
#define HTML_SELECT_WIDTH        21
#define HTML_SELECT_ATTRIBUTES   22

#define HTML_STYLE_CLASS          0
#define HTML_STYLE_DIR            1
#define HTML_STYLE_ID             2
#define HTML_STYLE_LANG           3
#define HTML_STYLE_MEDIA          4
#define HTML_STYLE_NOTATION       5
#define HTML_STYLE_STYLE          6
#define HTML_STYLE_TITLE          7
#define HTML_STYLE_TYPE           8
#define HTML_STYLE_ATTRIBUTES     9

#define HTML_TAB_ALIGN            0
#define HTML_TAB_CLASS            1
#define HTML_TAB_CLEAR            2
#define HTML_TAB_DIR              3
#define HTML_TAB_DP               4
#define HTML_TAB_ID               5
#define HTML_TAB_INDENT           6
#define HTML_TAB_LANG             7
#define HTML_TAB_STYLE            8
#define HTML_TAB_TITLE            9
#define HTML_TAB_TO              10
#define HTML_TAB_ATTRIBUTES      11

#define HTML_TABLE_ALIGN          0
#define HTML_TABLE_BACKGROUND     1
#define HTML_TABLE_BORDER         2
#define HTML_TABLE_CELLPADDING    3
#define HTML_TABLE_CELLSPACING    4
#define HTML_TABLE_CLASS          5
#define HTML_TABLE_CLEAR          6
#define HTML_TABLE_COLS           7
#define HTML_TABLE_COLSPEC        8
#define HTML_TABLE_DIR            9
#define HTML_TABLE_DP            10
#define HTML_TABLE_FRAME         11
#define HTML_TABLE_ID            12
#define HTML_TABLE_LANG          13
#define HTML_TABLE_NOFLOW        14
#define HTML_TABLE_NOWRAP        15
#define HTML_TABLE_RULES         16
#define HTML_TABLE_STYLE         17
#define HTML_TABLE_SUMMARY       18
#define HTML_TABLE_TITLE         19
#define HTML_TABLE_UNITS         20
#define HTML_TABLE_WIDTH         21
#define HTML_TABLE_ATTRIBUTES    22

#define HTML_TD_ABBR              0
#define HTML_TD_ALIGN             1
#define HTML_TD_AXES              2
#define HTML_TD_AXIS              3
#define HTML_TD_BACKGROUND        4
#define HTML_TD_CHAR              5
#define HTML_TD_CHAROFF           6
#define HTML_TD_CLASS             7
#define HTML_TD_CLEAR             8
#define HTML_TD_COLSPAN           9
#define HTML_TD_DIR              10
#define HTML_TD_DP               11
#define HTML_TD_HEADERS          12
#define HTML_TD_HEIGHT           13
#define HTML_TD_ID               14
#define HTML_TD_LANG             15
#define HTML_TD_NOWRAP           16
#define HTML_TD_ROWSPAN          17
#define HTML_TD_SCOPE            18
#define HTML_TD_STYLE            19
#define HTML_TD_TITLE            20
#define HTML_TD_VALIGN           21
#define HTML_TD_WIDTH            22
#define HTML_TD_ATTRIBUTES       23

#define HTML_TEXTAREA_ACCEPT_CHARSET 0
#define HTML_TEXTAREA_ACCESSKEY   1
#define HTML_TEXTAREA_ALIGN       2
#define HTML_TEXTAREA_CLASS       3
#define HTML_TEXTAREA_CLEAR       4
#define HTML_TEXTAREA_COLS        5
#define HTML_TEXTAREA_DIR         6
#define HTML_TEXTAREA_DISABLED    7
#define HTML_TEXTAREA_ERROR       8
#define HTML_TEXTAREA_ID          9
#define HTML_TEXTAREA_LANG       10
#define HTML_TEXTAREA_NAME       11
#define HTML_TEXTAREA_NOTAB      12
#define HTML_TEXTAREA_ONBLUR     13
#define HTML_TEXTAREA_ONCHANGE   14
#define HTML_TEXTAREA_ONFOCUS    15
#define HTML_TEXTAREA_ONSELECT   16
#define HTML_TEXTAREA_READONLY   17
#define HTML_TEXTAREA_ROWS       18
#define HTML_TEXTAREA_STYLE      19
#define HTML_TEXTAREA_TABINDEX   20
#define HTML_TEXTAREA_TITLE      21
#define HTML_TEXTAREA_ATTRIBUTES 22

#define HTML_TR_ALIGN             0
#define HTML_TR_CHAR              1
#define HTML_TR_CHAROFF           2
#define HTML_TR_CLASS             3
#define HTML_TR_CLEAR             4
#define HTML_TR_DIR               5
#define HTML_TR_DP                6
#define HTML_TR_ID                7
#define HTML_TR_LANG              8
#define HTML_TR_NOWRAP            9
#define HTML_TR_STYLE            10
#define HTML_TR_TITLE            11
#define HTML_TR_VALIGN           12
#define HTML_TR_ATTRIBUTES       13

#define HTML_UL_CLASS             0
#define HTML_UL_CLEAR             1
#define HTML_UL_COMPACT           2
#define HTML_UL_DINGBAT           3
#define HTML_UL_DIR               4
#define HTML_UL_ID                5
#define HTML_UL_LANG              6
#define HTML_UL_MD                7
#define HTML_UL_PLAIN             8
#define HTML_UL_SRC               9
#define HTML_UL_STYLE            10
#define HTML_UL_TITLE            11
#define HTML_UL_TYPE             12
#define HTML_UL_WRAP             13
#define HTML_UL_ATTRIBUTES       14

#ifdef __cplusplus
}
#endif
#endif				/* hdr_HTMLDTD_H */
