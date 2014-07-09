/* $LynxId: src1_HTMLDTD.h,v 1.45 2011/10/07 00:54:36 Kihara.Hideto Exp $ */
#ifndef src_HTMLDTD_H1
#define src_HTMLDTD_H1 1

#ifndef once_HTMLDTD
#define once_HTMLDTD 1

#define T_A             0x00008,0x0B007,0x0FF17,0x37787,0x77BA7,0x8604F,0x00014
#define T_ABBR          0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00003,0x00000
#define T_ACRONYM       0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00003,0x00000
#define T_ADDRESS       0x00200,0x0F14F,0x8FFFF,0x36680,0xB6FAF,0x80317,0x00000
#define T_APPLET        0x02000,0x0B0CF,0x8FFFF,0x37F9F,0xB7FBF,0x8300F,0x00000
#define T_AREA          0x08000,0x00000,0x00000,0x08000,0x3FFFF,0x00F1F,0x00001
#define T_AU            0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00003,0x00000
#define T_AUTHOR        0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00003,0x00000
#define T_B             0x00001,0x8B04F,0xAFFFF,0xA778F,0xF7FBF,0x00001,0x00014
#define T_BANNER        0x00200,0x0FB8F,0x0FFFF,0x30000,0x30000,0x8031F,0x00000
#define T_BASE          0x40000,0x00000,0x00000,0x50000,0x50000,0x8000F,0x00001
#define T_BASEFONT      0x01000,0x00000,0x00000,0x377AF,0x37FAF,0x8F000,0x00001
#define T_BDO           0x00100,0x0B04F,0x8FFFF,0x36680,0xB6FAF,0x0033F,0x00000
#define T_BGSOUND       0x01000,0x00000,0x00000,0x777AF,0x77FAF,0x8730F,0x00001
#define T_BIG           0x00001,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00014
#define T_BLINK         0x00001,0x8B04F,0x8FFFF,0xA778F,0xF7FAF,0x00001,0x00014
#define T_BLOCKQUOTE    0x00200,0xAFBCF,0xAFFFF,0xB6680,0xB6FAF,0x8031F,0x00000
#define T_BODY          0x20000,0x2FB8F,0x2FFFF,0x30000,0x30000,0xDFF7F,0x00003
#define T_BODYTEXT      0x20000,0x0FB8F,0xAFFFF,0x30200,0xB7FAF,0x8F17F,0x00003
#define T_BQ            0x00200,0xAFBCF,0xAFFFF,0xB6680,0xB6FAF,0x8031F,0x00000
#define T_BR            0x01000,0x00000,0x00000,0x377BF,0x77FBF,0x8101F,0x00001
#define T_BUTTON        0x02000,0x0BB07,0x0FF37,0x0378F,0x37FBF,0x8115F,0x00000
#define T_CAPTION       0x00100,0x0B04F,0x8FFFF,0x06A00,0xB6FA7,0x8035F,0x00000
#define T_CENTER        0x00200,0x8FBCF,0x8FFFF,0xB6680,0xB6FA7,0x8071F,0x00000
#define T_CITE          0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00002,0x00010
#define T_CODE          0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00002,0x00000
#define T_COL           0x04000,0x00000,0x00000,0x00820,0x36FA7,0x88F5F,0x00001
#define T_COLGROUP      0x00020,0x04000,0x04000,0x00800,0x36FA7,0x8875F,0x00001
#define T_COMMENT       0x00004,0x00000,0x00000,0xA77AF,0x7FFFF,0x00003,0x00000
#define T_CREDIT        0x00100,0x0B04F,0x8FFFF,0x06A00,0xB7FBF,0x8030F,0x00000
#define T_DD            0x00400,0x0FBCF,0x8FFFF,0x00800,0xB6FFF,0x8071F,0x00001
#define T_DEL           0x00002,0x8BBCF,0x8FFFF,0xA7F8F,0xF7FBF,0x00003,0x00000
#define T_DFN           0x00002,0x8B0CF,0x8FFFF,0x8778F,0xF7FBF,0x00003,0x00000
#define T_DIR           0x00800,0x0B400,0x0F75F,0x37680,0x36FB7,0x84F7F,0x00000
#define T_DIV           0x00200,0x8FBCF,0x8FFFF,0xB66A0,0xB7FFF,0x8031F,0x00004
#define T_DL            0x00800,0x0C480,0x8FFFF,0x36680,0xB7FB7,0x0075F,0x00000
#define T_DLC           0x00800,0x0C480,0x8FFFF,0x36680,0xB7FB7,0x0075F,0x00000
#define T_DT            0x00400,0x0B04F,0x0B1FF,0x00800,0x17FFF,0x8071F,0x00001
#define T_EM            0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FAF,0x00003,0x00010
#define T_EMBED         0x02000,0x8F107,0x8FFF7,0xB6FBF,0xB7FBF,0x1FF7F,0x00001
#define T_FIELDSET      0x00200,0x8FB4F,0x8FF7F,0x86787,0xB7FF7,0x8805F,0x00000
#define T_FIG           0x00200,0x0FB00,0x8FFFF,0x36680,0xB6FBF,0x8834F,0x00000
#define T_FN            0x00200,0x8FBCF,0x8FFFF,0xB6680,0xB7EBF,0x8114F,0x00000
#define T_FONT          0x00001,0x8B04F,0x8FFFF,0xB778F,0xF7FBF,0x00001,0x00014
#define T_FORM          0x00080,0x0FF6F,0x0FF7F,0x36E07,0x32F07,0x88DFF,0x00000
#define T_FRAME         0x10000,0x00000,0x00000,0x10000,0x10000,0x9FFFF,0x00001
#define T_FRAMESET      0x10000,0x90000,0x90000,0x90000,0x93000,0x9FFFF,0x00000
#define T_H1            0x00100,0x0B04F,0x0B05F,0x36680,0x37FAF,0x80117,0x00000
#define T_H2            0x00100,0x0B04F,0x0B05F,0x36680,0x37FAF,0x80117,0x00000
#define T_H3            0x00100,0x0B04F,0x0B05F,0x36680,0x37FAF,0x80117,0x00000
#define T_H4            0x00100,0x0B04F,0x0B05F,0x36680,0x37FAF,0x80117,0x00000
#define T_H5            0x00100,0x0B04F,0x0B05F,0x36680,0x37FAF,0x80117,0x00000
#define T_H6            0x00100,0x0B04F,0x0B05F,0x36680,0x37FAF,0x80117,0x00000
#define T_HEAD          0x40000,0x4F000,0x47000,0x10000,0x10000,0x9FF7F,0x00007
#define T_HR            0x04000,0x00000,0x00000,0x3FE80,0x3FFBF,0x87F37,0x00001
#define T_HTML          0x10000,0x7FB8F,0x7FFFF,0x00000,0x00000,0x1FFFF,0x00003
#define T_HY            0x01000,0x00000,0x00000,0x3779F,0x77FBF,0x8101F,0x00001
#define T_I             0x00001,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00014
#define T_IFRAME        0x02000,0x8FBCF,0x8FFFF,0xB679F,0xB6FBF,0xD315F,0x00000
#define T_IMG           0x01000,0x00000,0x00000,0x3779F,0x37FBF,0x80000,0x00001
#define T_INPUT         0x00040,0x00000,0x00000,0x03F87,0x37F87,0x8904F,0x00001
#define T_INS           0x00002,0x8BBCF,0x8FFFF,0xA7F8F,0xF7FBF,0x00003,0x00000
#define T_ISINDEX       0x08000,0x00000,0x00000,0x7778F,0x7FFAF,0x80007,0x00001
#define T_KBD           0x00002,0x00000,0x00000,0x2778F,0x77FBF,0x00003,0x00000
#define T_KEYGEN        0x00040,0x00000,0x00000,0x07FB7,0x37FB7,0x80070,0x00001
#define T_LABEL         0x00002,0x0304F,0x0FFFF,0x0679F,0x36FBF,0x00007,0x00000
#define T_LEGEND        0x00002,0x0B04F,0x8FF7F,0x00200,0xB7FA7,0x00003,0x00000
#define T_LH            0x00400,0x0BB7F,0x8FFFF,0x00800,0x97FFF,0x8071F,0x00001
#define T_LI            0x00400,0x0BBFF,0x8FFFF,0x00800,0x97FFF,0x8071F,0x00001
#define T_LINK          0x08000,0x00000,0x00000,0x50000,0x50000,0x0FF7F,0x00001
#define T_LISTING       0x00800,0x00000,0x00000,0x36600,0x36F00,0x80F1F,0x00000
#define T_MAP           0x08000,0x08000,0x08000,0x37FCF,0x37FBF,0x0051F,0x00000
#define T_MARQUEE       0x04000,0x0000F,0x8F01F,0x37787,0xB7FA7,0x8301C,0x00000
#define T_MATH          0x00004,0x0B05F,0x8FFFF,0x2778F,0xF7FBF,0x0001F,0x00000
#define T_MENU          0x00800,0x0B400,0x0F75F,0x17680,0x36FB7,0x88F7F,0x00000
#define T_META          0x08000,0x00000,0x00000,0x50000,0x50000,0x0FF7F,0x00001
#define T_NEXTID        0x01000,0x00000,0x00000,0x50000,0x1FFF7,0x00001,0x00001
#define T_NOFRAMES      0x20000,0x2FB8F,0x0FFFF,0x17000,0x17000,0x0CF5F,0x00000
#define T_NOTE          0x00200,0x0BBAF,0x8FFFF,0x376B0,0xB7FFF,0x8031F,0x00000
#define T_OBJECT        0x02000,0x8FBCF,0x8FFFF,0xB679F,0xB6FBF,0x83D5F,0x00020
#define T_OL            0x00800,0x0C400,0x8FFFF,0x37680,0xB7FB7,0x88F7F,0x00000
#define T_OPTION        0x08000,0x00000,0x00000,0x00040,0x37FFF,0x8031F,0x00001
#define T_OVERLAY       0x04000,0x00000,0x00000,0x00200,0x37FBF,0x83F7F,0x00001
#define T_P             0x00100,0x0B04F,0x8FFFF,0x36680,0xB6FA7,0x80117,0x00001
#define T_PARAM         0x01000,0x00000,0x00000,0x33500,0x37FFF,0x81560,0x00001
#define T_PLAINTEXT     0x10000,0xFFFFF,0xFFFFF,0x90000,0x90000,0x3FFFF,0x00001
#define T_PRE           0x00200,0x0F04F,0x0F05E,0x36680,0x36FF0,0x8071E,0x00000
#define T_Q             0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FAF,0x00003,0x00000
#define T_S             0x00001,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00000
#define T_SAMP          0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00002,0x00010
#define T_SCRIPT        0x02000,0x00000,0x00000,0x77F9F,0x77FFF,0x87D5F,0x00000
#define T_SELECT        0x00040,0x08000,0x08000,0x03FAF,0x33FBF,0x80D5F,0x00008
#define T_SHY           0x01000,0x00000,0x00000,0x3779F,0x77FBF,0x8101F,0x00001
#define T_SMALL         0x00001,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00014
#define T_SPAN          0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x80003,0x00000
#define T_SPOT          0x00008,0x00000,0x00000,0x3FFF7,0x3FFF7,0x00008,0x00001
#define T_STRIKE        0x00001,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00000
#define T_STRONG        0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FAF,0x00003,0x00010
#define T_STYLE         0x40000,0x00000,0x00000,0x7638F,0x76FAF,0x8001F,0x00000
#define T_SUB           0x00004,0x8B05F,0x8FFFF,0x8779F,0xF7FBF,0x00007,0x00000
#define T_SUP           0x00004,0x8B05F,0x8FFFF,0x8779F,0xF7FBF,0x00007,0x00000
#define T_TAB           0x01000,0x00000,0x00000,0x3778F,0x57FAF,0x00001,0x00001
#define T_TABLE         0x00800,0x0F1E0,0x8FFFF,0x36680,0xB6FA7,0x8C57F,0x00000
#define T_TBODY         0x00020,0x00020,0x8FFFF,0x00880,0xB7FB7,0x8C75F,0x00003
#define T_TD            0x00400,0x0FBCF,0x8FFFF,0x00020,0xB7FB7,0x8C75F,0x00001
#define T_TEXTAREA      0x00040,0x00000,0x00000,0x07F8F,0x33FBF,0x80D5F,0x00040
#define T_TEXTFLOW      0x20000,0x8FBFF,0x9FFFF,0x977B0,0xB7FB7,0x9B00F,0x00003
#define T_TFOOT         0x00020,0x00020,0x8FFFF,0x00800,0xB7FB7,0x8CF5F,0x00001
#define T_TH            0x00400,0x0FBCF,0x0FFFF,0x00020,0xB7FB7,0x8CF5F,0x00001
#define T_THEAD         0x00020,0x00020,0x8FFFF,0x00800,0xB7FB7,0x8CF5F,0x00001
#define T_TITLE         0x40000,0x00000,0x00000,0x50000,0x50000,0x0031F,0x0000C
#define T_TR            0x00020,0x00400,0x8FFFF,0x00820,0xB7FB7,0x8C75F,0x00001
#define T_TT            0x00001,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00010
#define T_U             0x00001,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00014
#define T_UL            0x00800,0x0C480,0x8FFFF,0x36680,0xB7FFF,0x8075F,0x00000
#define T_VAR           0x00002,0x8B04F,0x8FFFF,0xA778F,0xF7FBF,0x00001,0x00000
#define T_WBR           0x00001,0x00000,0x00000,0x3778F,0x77FBF,0x8101F,0x00001
#define T_XMP           0x00800,0x00000,0x00000,0x367E0,0x36FFF,0x0875F,0x00001
#define T_OBJECT_PCDATA 0x02000,0x8FBCF,0x8FFFF,0xB679F,0xB6FBF,0x83D5F,0x00008
#define T__UNREC_	0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000
#ifdef USE_PRETTYSRC
# define N HTMLA_NORMAL
# define i HTMLA_ANAME
# define h HTMLA_HREF
# define c HTMLA_CLASS
# define x HTMLA_AUXCLASS
# define T(t) , t
#else
# define T(t)			/*nothing */
#endif
/* *INDENT-OFF* */

#define ATTR_TYPE(name) #name, name##_attr_list

/* generic attributes, used in different tags */
static const attr core_attr_list[] = {
	{ "CLASS"         T(c) },
	{ "ID"            T(i) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType core_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ 0, 0 },
};

static const attr i18n_attr_list[] = {
	{ "DIR"           T(N) },
	{ "LANG"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType i18n_attr_type[] = {
	{ ATTR_TYPE(i18n) },
	{ 0, 0 },
};

static const attr events_attr_list[] = {
	{ "ONCLICK"       T(N) },
	{ "ONDBLCLICK"    T(N) },
	{ "ONKEYDOWN"     T(N) },
	{ "ONKEYPRESS"    T(N) },
	{ "ONKEYUP"       T(N) },
	{ "ONMOUSEDOWN"   T(N) },
	{ "ONMOUSEMOVE"   T(N) },
	{ "ONMOUSEOUT"    T(N) },
	{ "ONMOUSEOVER"   T(N) },
	{ "ONMOUSEUP"     T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType events_attr_type[] = {
	{ ATTR_TYPE(events) },
	{ 0, 0 },
};

static const attr align_attr_list[] = {
	{ "ALIGN"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType align_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ 0, 0 },
};

static const attr cellalign_attr_list[] = {
	{ "ALIGN"         T(N) },
	{ "CHAR"          T(N) },
	{ "CHAROFF"       T(N) },
	{ "VALIGN"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType cellalign_attr_type[] = {
	{ ATTR_TYPE(cellalign) },
	{ 0, 0 },
};

static const attr bgcolor_attr_list[] = {
	{ "BGCOLOR"       T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType bgcolor_attr_type[] = {
	{ ATTR_TYPE(bgcolor) },
	{ 0, 0 },
};


/* tables defining attributes per-tag in terms of generic attributes (editable) */
static const attr A_attr_list[] = {
	{ "ACCESSKEY"     T(N) },
	{ "CHARSET"       T(N) },
	{ "CLEAR"         T(N) },
	{ "COORDS"        T(N) },
	{ "HREF"          T(h) },
	{ "HREFLANG"      T(N) },
	{ "ISMAP"         T(N) },
	{ "MD"            T(N) },
	{ "NAME"          T(i) },
	{ "NOTAB"         T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONFOCUS"       T(N) },
	{ "REL"           T(N) },
	{ "REV"           T(N) },
	{ "SHAPE"         T(N) },
	{ "TABINDEX"      T(N) },
	{ "TARGET"        T(N) },
	{ "TYPE"          T(N) },
	{ "URN"           T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType A_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(A) },
	{ 0, 0 },
};

static const attr ADDRESS_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "NOWRAP"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType ADDRESS_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(ADDRESS) },
	{ 0, 0 },
};

static const attr APPLET_attr_list[] = {
	{ "ALT"           T(N) },
	{ "CLEAR"         T(N) },
	{ "CODE"          T(N) },
	{ "CODEBASE"      T(h) },
	{ "DOWNLOAD"      T(N) },
	{ "HEIGHT"        T(N) },
	{ "HSPACE"        T(N) },
	{ "NAME"          T(i) },
	{ "VSPACE"        T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType APPLET_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(APPLET) },
	{ 0, 0 },
};

static const attr AREA_attr_list[] = {
	{ "ACCESSKEY"     T(N) },
	{ "ALT"           T(N) },
	{ "CLEAR"         T(N) },
	{ "COORDS"        T(N) },
	{ "HREF"          T(h) },
	{ "NOHREF"        T(N) },
	{ "NOTAB"         T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONFOCUS"       T(N) },
	{ "SHAPE"         T(N) },
	{ "TABINDEX"      T(N) },
	{ "TARGET"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType AREA_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(AREA) },
	{ 0, 0 },
};

static const attr BASE_attr_list[] = {
	{ "HREF"          T(h) },
	{ "TARGET"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType BASE_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(BASE) },
	{ 0, 0 },
};

static const attr BGSOUND_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "LOOP"          T(N) },
	{ "SRC"           T(h) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType BGSOUND_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(BGSOUND) },
	{ 0, 0 },
};

static const attr BODY_attr_list[] = {
	{ "ALINK"         T(N) },
	{ "BACKGROUND"    T(h) },
	{ "CLEAR"         T(N) },
	{ "LINK"          T(N) },
	{ "ONLOAD"        T(N) },
	{ "ONUNLOAD"      T(N) },
	{ "TEXT"          T(N) },
	{ "VLINK"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType BODY_attr_type[] = {
	{ ATTR_TYPE(bgcolor) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(BODY) },
	{ 0, 0 },
};

static const attr BODYTEXT_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "DATA"          T(N) },
	{ "NAME"          T(N) },
	{ "OBJECT"        T(N) },
	{ "REF"           T(N) },
	{ "TYPE"          T(N) },
	{ "VALUE"         T(N) },
	{ "VALUETYPE"     T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType BODYTEXT_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(BODYTEXT) },
	{ 0, 0 },
};

static const attr BQ_attr_list[] = {
	{ "CITE"          T(h) },
	{ "CLEAR"         T(N) },
	{ "NOWRAP"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType BQ_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(BQ) },
	{ 0, 0 },
};

static const attr BUTTON_attr_list[] = {
	{ "ACCESSKEY"     T(N) },
	{ "CLEAR"         T(N) },
	{ "DISABLED"      T(N) },
	{ "NAME"          T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONFOCUS"       T(N) },
	{ "READONLY"      T(N) },
	{ "TABINDEX"      T(N) },
	{ "TYPE"          T(N) },
	{ "VALUE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType BUTTON_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(BUTTON) },
	{ 0, 0 },
};

static const attr CAPTION_attr_list[] = {
	{ "ACCESSKEY"     T(N) },
	{ "CLEAR"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType CAPTION_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(CAPTION) },
	{ 0, 0 },
};

static const attr COL_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "SPAN"          T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType COL_attr_type[] = {
	{ ATTR_TYPE(cellalign) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(COL) },
	{ 0, 0 },
};

static const attr DEL_attr_list[] = {
	{ "CITE"          T(N) },
	{ "DATETIME"      T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType DEL_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(DEL) },
	{ 0, 0 },
};

static const attr DIV_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType DIV_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(DIV) },
	{ 0, 0 },
};

static const attr DL_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "COMPACT"       T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType DL_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(DL) },
	{ 0, 0 },
};

static const attr EMBED_attr_list[] = {
	{ "ALT"           T(N) },
	{ "BORDER"        T(N) },
	{ "CLEAR"         T(N) },
	{ "HEIGHT"        T(N) },
	{ "IMAGEMAP"      T(N) },
	{ "ISMAP"         T(N) },
	{ "MD"            T(N) },
	{ "NAME"          T(i) },
	{ "NOFLOW"        T(N) },
	{ "PARAMS"        T(N) },
	{ "SRC"           T(h) },
	{ "UNITS"         T(N) },
	{ "USEMAP"        T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType EMBED_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(EMBED) },
	{ 0, 0 },
};

static const attr FIG_attr_list[] = {
	{ "BORDER"        T(N) },
	{ "CLEAR"         T(N) },
	{ "HEIGHT"        T(N) },
	{ "IMAGEMAP"      T(N) },
	{ "ISOBJECT"      T(N) },
	{ "MD"            T(N) },
	{ "NOFLOW"        T(N) },
	{ "SRC"           T(h) },
	{ "UNITS"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType FIG_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(FIG) },
	{ 0, 0 },
};

static const attr FONT_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "COLOR"         T(N) },
	{ "END"           T(N) },
	{ "FACE"          T(N) },
	{ "SIZE"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType FONT_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(FONT) },
	{ 0, 0 },
};

static const attr FORM_attr_list[] = {
	{ "ACCEPT"        T(N) },
	{ "ACCEPT-CHARSET" T(N) },
	{ "ACTION"        T(h) },
	{ "CLEAR"         T(N) },
	{ "ENCTYPE"       T(N) },
	{ "METHOD"        T(N) },
	{ "ONRESET"       T(N) },
	{ "ONSUBMIT"      T(N) },
	{ "SCRIPT"        T(N) },
	{ "SUBJECT"       T(N) },
	{ "TARGET"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType FORM_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(FORM) },
	{ 0, 0 },
};

static const attr FRAME_attr_list[] = {
	{ "FRAMEBORDER"   T(N) },
	{ "LONGDESC"      T(h) },
	{ "MARGINHEIGHT"  T(N) },
	{ "MARGINWIDTH"   T(N) },
	{ "NAME"          T(N) },
	{ "NORESIZE"      T(N) },
	{ "SCROLLING"     T(N) },
	{ "SRC"           T(h) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType FRAME_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(FRAME) },
	{ 0, 0 },
};

static const attr FRAMESET_attr_list[] = {
	{ "COLS"          T(N) },
	{ "ONLOAD"        T(N) },
	{ "ONUNLOAD"      T(N) },
	{ "ROWS"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType FRAMESET_attr_type[] = {
	{ ATTR_TYPE(FRAMESET) },
	{ 0, 0 },
};

static const attr GEN_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType GEN_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(GEN) },
	{ 0, 0 },
};

static const attr H_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "DINGBAT"       T(N) },
	{ "MD"            T(N) },
	{ "NOWRAP"        T(N) },
	{ "SEQNUM"        T(N) },
	{ "SKIP"          T(N) },
	{ "SRC"           T(h) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType H_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(H) },
	{ 0, 0 },
};

static const attr HR_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "MD"            T(N) },
	{ "NOSHADE"       T(N) },
	{ "SIZE"          T(N) },
	{ "SRC"           T(h) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType HR_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(HR) },
	{ 0, 0 },
};

static const attr IFRAME_attr_list[] = {
	{ "FRAMEBORDER"   T(N) },
	{ "HEIGHT"        T(N) },
	{ "LONGDESC"      T(h) },
	{ "MARGINHEIGHT"  T(N) },
	{ "MARGINWIDTH"   T(N) },
	{ "NAME"          T(N) },
	{ "SCROLLING"     T(N) },
	{ "SRC"           T(h) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType IFRAME_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(IFRAME) },
	{ 0, 0 },
};

static const attr IMG_attr_list[] = {
	{ "ALT"           T(N) },
	{ "BORDER"        T(N) },
	{ "CLEAR"         T(N) },
	{ "HEIGHT"        T(N) },
	{ "HSPACE"        T(N) },
	{ "ISMAP"         T(N) },
	{ "ISOBJECT"      T(N) },
	{ "LONGDESC"      T(h) },
	{ "MD"            T(N) },
	{ "NAME"          T(N) },
	{ "SRC"           T(h) },
	{ "UNITS"         T(N) },
	{ "USEMAP"        T(h) },
	{ "VSPACE"        T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType IMG_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(IMG) },
	{ 0, 0 },
};

static const attr INPUT_attr_list[] = {
	{ "ACCEPT"        T(N) },
	{ "ACCEPT-CHARSET" T(N) },
	{ "ACCESSKEY"     T(N) },
	{ "ALT"           T(N) },
	{ "CHECKED"       T(N) },
	{ "CLEAR"         T(N) },
	{ "DISABLED"      T(N) },
	{ "ERROR"         T(N) },
	{ "HEIGHT"        T(N) },
	{ "ISMAP"         T(N) },
	{ "MAX"           T(N) },
	{ "MAXLENGTH"     T(N) },
	{ "MD"            T(N) },
	{ "MIN"           T(N) },
	{ "NAME"          T(N) },
	{ "NOTAB"         T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONCHANGE"      T(N) },
	{ "ONFOCUS"       T(N) },
	{ "ONSELECT"      T(N) },
	{ "READONLY"      T(N) },
	{ "SIZE"          T(N) },
	{ "SRC"           T(h) },
	{ "TABINDEX"      T(N) },
	{ "TYPE"          T(N) },
	{ "USEMAP"        T(N) },
	{ "VALUE"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType INPUT_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(INPUT) },
	{ 0, 0 },
};

static const attr ISINDEX_attr_list[] = {
	{ "ACTION"        T(h) },
	{ "HREF"          T(h) },
	{ "PROMPT"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType ISINDEX_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(ISINDEX) },
	{ 0, 0 },
};

static const attr KEYGEN_attr_list[] = {
	{ "CHALLENGE"     T(N) },
	{ "NAME"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType KEYGEN_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(KEYGEN) },
	{ 0, 0 },
};

static const attr LABEL_attr_list[] = {
	{ "ACCESSKEY"     T(N) },
	{ "CLEAR"         T(N) },
	{ "FOR"           T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONFOCUS"       T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType LABEL_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(LABEL) },
	{ 0, 0 },
};

static const attr LI_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "DINGBAT"       T(N) },
	{ "MD"            T(N) },
	{ "SKIP"          T(N) },
	{ "SRC"           T(h) },
	{ "TYPE"          T(N) },
	{ "VALUE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType LI_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(LI) },
	{ 0, 0 },
};

static const attr LINK_attr_list[] = {
	{ "CHARSET"       T(N) },
	{ "HREF"          T(h) },
	{ "HREFLANG"      T(N) },
	{ "MEDIA"         T(N) },
	{ "REL"           T(N) },
	{ "REV"           T(N) },
	{ "TARGET"        T(N) },
	{ "TYPE"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType LINK_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(LINK) },
	{ 0, 0 },
};

static const attr MAP_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "NAME"          T(i) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType MAP_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(MAP) },
	{ 0, 0 },
};

static const attr MATH_attr_list[] = {
	{ "BOX"           T(N) },
	{ "CLEAR"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType MATH_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(MATH) },
	{ 0, 0 },
};

static const attr META_attr_list[] = {
	{ "CHARSET"       T(N) },
	{ "CONTENT"       T(N) },
	{ "HTTP-EQUIV"    T(N) },
	{ "NAME"          T(N) },
	{ "SCHEME"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType META_attr_type[] = {
	{ ATTR_TYPE(META) },
	{ 0, 0 },
};

static const attr NEXTID_attr_list[] = {
	{ "N"             T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType NEXTID_attr_type[] = {
	{ ATTR_TYPE(NEXTID) },
	{ 0, 0 },
};

static const attr NOTE_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "MD"            T(N) },
	{ "ROLE"          T(x) },
	{ "SRC"           T(h) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType NOTE_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(NOTE) },
	{ 0, 0 },
};

static const attr OBJECT_attr_list[] = {
	{ "ARCHIVE"       T(N) },
	{ "BORDER"        T(N) },
	{ "CLASSID"       T(h) },
	{ "CODEBASE"      T(h) },
	{ "CODETYPE"      T(N) },
	{ "DATA"          T(h) },
	{ "DECLARE"       T(N) },
	{ "HEIGHT"        T(N) },
	{ "HSPACE"        T(N) },
	{ "ISMAP"         T(N) },
	{ "NAME"          T(N) },
	{ "NOTAB"         T(N) },
	{ "SHAPES"        T(N) },
	{ "STANDBY"       T(N) },
	{ "TABINDEX"      T(N) },
	{ "TYPE"          T(N) },
	{ "USEMAP"        T(h) },
	{ "VSPACE"        T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType OBJECT_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(OBJECT) },
	{ 0, 0 },
};

static const attr OL_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "COMPACT"       T(N) },
	{ "CONTINUE"      T(N) },
	{ "SEQNUM"        T(N) },
	{ "START"         T(N) },
	{ "TYPE"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType OL_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(OL) },
	{ 0, 0 },
};

static const attr OPTION_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "DISABLED"      T(N) },
	{ "ERROR"         T(N) },
	{ "LABEL"         T(N) },
	{ "SELECTED"      T(N) },
	{ "SHAPE"         T(N) },
	{ "VALUE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType OPTION_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(OPTION) },
	{ 0, 0 },
};

static const attr OVERLAY_attr_list[] = {
	{ "HEIGHT"        T(N) },
	{ "IMAGEMAP"      T(N) },
	{ "MD"            T(N) },
	{ "SRC"           T(h) },
	{ "UNITS"         T(N) },
	{ "WIDTH"         T(N) },
	{ "X"             T(N) },
	{ "Y"             T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType OVERLAY_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(OVERLAY) },
	{ 0, 0 },
};

static const attr P_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "NOWRAP"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType P_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(P) },
	{ 0, 0 },
};

static const attr PARAM_attr_list[] = {
	{ "ACCEPT"        T(N) },
	{ "ACCEPT-CHARSET" T(N) },
	{ "ACCEPT-ENCODING" T(N) },
	{ "CLEAR"         T(N) },
	{ "DATA"          T(N) },
	{ "NAME"          T(N) },
	{ "OBJECT"        T(N) },
	{ "REF"           T(N) },
	{ "TYPE"          T(N) },
	{ "VALUE"         T(N) },
	{ "VALUEREF"      T(N) },
	{ "VALUETYPE"     T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType PARAM_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(PARAM) },
	{ 0, 0 },
};

static const attr Q_attr_list[] = {
	{ "CITE"          T(h) },
	{ "CLEAR"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType Q_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(Q) },
	{ 0, 0 },
};

static const attr SCRIPT_attr_list[] = {
	{ "CHARSET"       T(N) },
	{ "CLEAR"         T(N) },
	{ "DEFER"         T(N) },
	{ "EVENT"         T(N) },
	{ "FOR"           T(N) },
	{ "LANGUAGE"      T(N) },
	{ "NAME"          T(N) },
	{ "SCRIPTENGINE"  T(N) },
	{ "SRC"           T(h) },
	{ "TYPE"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType SCRIPT_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(SCRIPT) },
	{ 0, 0 },
};

static const attr SELECT_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "DISABLED"      T(N) },
	{ "ERROR"         T(N) },
	{ "HEIGHT"        T(N) },
	{ "MD"            T(N) },
	{ "MULTIPLE"      T(N) },
	{ "NAME"          T(N) },
	{ "NOTAB"         T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONCHANGE"      T(N) },
	{ "ONFOCUS"       T(N) },
	{ "SIZE"          T(N) },
	{ "TABINDEX"      T(N) },
	{ "UNITS"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType SELECT_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(SELECT) },
	{ 0, 0 },
};

static const attr STYLE_attr_list[] = {
	{ "MEDIA"         T(N) },
	{ "NOTATION"      T(N) },
	{ "TYPE"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType STYLE_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(STYLE) },
	{ 0, 0 },
};

static const attr TAB_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "DP"            T(N) },
	{ "INDENT"        T(N) },
	{ "TO"            T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType TAB_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(TAB) },
	{ 0, 0 },
};

static const attr TABLE_attr_list[] = {
	{ "BACKGROUND"    T(h) },
	{ "BORDER"        T(N) },
	{ "CELLPADDING"   T(N) },
	{ "CELLSPACING"   T(N) },
	{ "CLEAR"         T(N) },
	{ "COLS"          T(N) },
	{ "COLSPEC"       T(N) },
	{ "DP"            T(N) },
	{ "FRAME"         T(N) },
	{ "NOFLOW"        T(N) },
	{ "NOWRAP"        T(N) },
	{ "RULES"         T(N) },
	{ "SUMMARY"       T(N) },
	{ "UNITS"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType TABLE_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(TABLE) },
	{ 0, 0 },
};

static const attr TD_attr_list[] = {
	{ "ABBR"          T(N) },
	{ "AXES"          T(N) },
	{ "AXIS"          T(N) },
	{ "BACKGROUND"    T(h) },
	{ "CLEAR"         T(N) },
	{ "COLSPAN"       T(N) },
	{ "DP"            T(N) },
	{ "HEADERS"       T(N) },
	{ "HEIGHT"        T(N) },
	{ "NOWRAP"        T(N) },
	{ "ROWSPAN"       T(N) },
	{ "SCOPE"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType TD_attr_type[] = {
	{ ATTR_TYPE(cellalign) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(TD) },
	{ 0, 0 },
};

static const attr TEXTAREA_attr_list[] = {
	{ "ACCEPT-CHARSET" T(N) },
	{ "ACCESSKEY"     T(N) },
	{ "CLEAR"         T(N) },
	{ "COLS"          T(N) },
	{ "DISABLED"      T(N) },
	{ "ERROR"         T(N) },
	{ "NAME"          T(N) },
	{ "NOTAB"         T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONCHANGE"      T(N) },
	{ "ONFOCUS"       T(N) },
	{ "ONSELECT"      T(N) },
	{ "READONLY"      T(N) },
	{ "ROWS"          T(N) },
	{ "TABINDEX"      T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType TEXTAREA_attr_type[] = {
	{ ATTR_TYPE(align) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(TEXTAREA) },
	{ 0, 0 },
};

static const attr TR_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "DP"            T(N) },
	{ "NOWRAP"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType TR_attr_type[] = {
	{ ATTR_TYPE(cellalign) },
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(events) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(TR) },
	{ 0, 0 },
};

static const attr UL_attr_list[] = {
	{ "CLEAR"         T(N) },
	{ "COMPACT"       T(N) },
	{ "DINGBAT"       T(N) },
	{ "MD"            T(N) },
	{ "PLAIN"         T(N) },
	{ "SRC"           T(h) },
	{ "TYPE"          T(N) },
	{ "WRAP"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const AttrType UL_attr_type[] = {
	{ ATTR_TYPE(core) },
	{ ATTR_TYPE(i18n) },
	{ ATTR_TYPE(UL) },
	{ 0, 0 },
};


/* attribute lists for the runtime (generated by dtd_util) */
static const attr A_attr[] = {          /* A attributes */
	{ "ACCESSKEY"     T(N) },
	{ "CHARSET"       T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "COORDS"        T(N) },
	{ "DIR"           T(N) },
	{ "HREF"          T(h) },
	{ "HREFLANG"      T(N) },
	{ "ID"            T(i) },
	{ "ISMAP"         T(N) },
	{ "LANG"          T(N) },
	{ "MD"            T(N) },
	{ "NAME"          T(i) },
	{ "NOTAB"         T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONFOCUS"       T(N) },
	{ "REL"           T(N) },
	{ "REV"           T(N) },
	{ "SHAPE"         T(N) },
	{ "STYLE"         T(N) },
	{ "TABINDEX"      T(N) },
	{ "TARGET"        T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ "URN"           T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr ADDRESS_attr[] = {    /* ADDRESS attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NOWRAP"        T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr APPLET_attr[] = {     /* APPLET attributes */
	{ "ALIGN"         T(N) },
	{ "ALT"           T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "CODE"          T(N) },
	{ "CODEBASE"      T(h) },
	{ "DIR"           T(N) },
	{ "DOWNLOAD"      T(N) },
	{ "HEIGHT"        T(N) },
	{ "HSPACE"        T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NAME"          T(i) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "VSPACE"        T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr AREA_attr[] = {       /* AREA attributes */
	{ "ACCESSKEY"     T(N) },
	{ "ALT"           T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "COORDS"        T(N) },
	{ "DIR"           T(N) },
	{ "HREF"          T(h) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NOHREF"        T(N) },
	{ "NOTAB"         T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONFOCUS"       T(N) },
	{ "SHAPE"         T(N) },
	{ "STYLE"         T(N) },
	{ "TABINDEX"      T(N) },
	{ "TARGET"        T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr BASE_attr[] = {       /* BASE attributes */
	{ "CLASS"         T(c) },
	{ "HREF"          T(h) },
	{ "ID"            T(i) },
	{ "STYLE"         T(N) },
	{ "TARGET"        T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr BGSOUND_attr[] = {    /* BGSOUND attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "LOOP"          T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr BODY_attr[] = {       /* BODY attributes */
	{ "ALINK"         T(N) },
	{ "BACKGROUND"    T(h) },
	{ "BGCOLOR"       T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "LINK"          T(N) },
	{ "ONLOAD"        T(N) },
	{ "ONUNLOAD"      T(N) },
	{ "STYLE"         T(N) },
	{ "TEXT"          T(N) },
	{ "TITLE"         T(N) },
	{ "VLINK"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr BODYTEXT_attr[] = {   /* BODYTEXT attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DATA"          T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NAME"          T(N) },
	{ "OBJECT"        T(N) },
	{ "REF"           T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ "VALUE"         T(N) },
	{ "VALUETYPE"     T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr BQ_attr[] = {         /* BLOCKQUOTE attributes */
	{ "CITE"          T(h) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NOWRAP"        T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr BUTTON_attr[] = {     /* BUTTON attributes */
	{ "ACCESSKEY"     T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "DISABLED"      T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NAME"          T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONFOCUS"       T(N) },
	{ "READONLY"      T(N) },
	{ "STYLE"         T(N) },
	{ "TABINDEX"      T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ "VALUE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr CAPTION_attr[] = {    /* CAPTION attributes */
	{ "ACCESSKEY"     T(N) },
	{ "ALIGN"         T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr COL_attr[] = {        /* COL attributes */
	{ "ALIGN"         T(N) },
	{ "CHAR"          T(N) },
	{ "CHAROFF"       T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "SPAN"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "VALIGN"        T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr DEL_attr[] = {        /* DEL attributes */
	{ "CITE"          T(N) },
	{ "CLASS"         T(c) },
	{ "DATETIME"      T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr DIV_attr[] = {        /* CENTER attributes */
	{ "ALIGN"         T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr DL_attr[] = {         /* DL attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "COMPACT"       T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr EMBED_attr[] = {      /* EMBED attributes */
	{ "ALIGN"         T(N) },
	{ "ALT"           T(N) },
	{ "BORDER"        T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "HEIGHT"        T(N) },
	{ "ID"            T(i) },
	{ "IMAGEMAP"      T(N) },
	{ "ISMAP"         T(N) },
	{ "LANG"          T(N) },
	{ "MD"            T(N) },
	{ "NAME"          T(i) },
	{ "NOFLOW"        T(N) },
	{ "PARAMS"        T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "UNITS"         T(N) },
	{ "USEMAP"        T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr FIG_attr[] = {        /* FIG attributes */
	{ "ALIGN"         T(N) },
	{ "BORDER"        T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "HEIGHT"        T(N) },
	{ "ID"            T(i) },
	{ "IMAGEMAP"      T(N) },
	{ "ISOBJECT"      T(N) },
	{ "LANG"          T(N) },
	{ "MD"            T(N) },
	{ "NOFLOW"        T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "UNITS"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr FONT_attr[] = {       /* BASEFONT attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "COLOR"         T(N) },
	{ "DIR"           T(N) },
	{ "END"           T(N) },
	{ "FACE"          T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "SIZE"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr FORM_attr[] = {       /* FORM attributes */
	{ "ACCEPT"        T(N) },
	{ "ACCEPT-CHARSET" T(N) },
	{ "ACTION"        T(h) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ENCTYPE"       T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "METHOD"        T(N) },
	{ "ONRESET"       T(N) },
	{ "ONSUBMIT"      T(N) },
	{ "SCRIPT"        T(N) },
	{ "STYLE"         T(N) },
	{ "SUBJECT"       T(N) },
	{ "TARGET"        T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr FRAME_attr[] = {      /* FRAME attributes */
	{ "CLASS"         T(c) },
	{ "FRAMEBORDER"   T(N) },
	{ "ID"            T(i) },
	{ "LONGDESC"      T(h) },
	{ "MARGINHEIGHT"  T(N) },
	{ "MARGINWIDTH"   T(N) },
	{ "NAME"          T(N) },
	{ "NORESIZE"      T(N) },
	{ "SCROLLING"     T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr FRAMESET_attr[] = {   /* FRAMESET attributes */
	{ "COLS"          T(N) },
	{ "ONLOAD"        T(N) },
	{ "ONUNLOAD"      T(N) },
	{ "ROWS"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr GEN_attr[] = {        /* ABBR attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr H_attr[] = {          /* H1 attributes */
	{ "ALIGN"         T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DINGBAT"       T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "MD"            T(N) },
	{ "NOWRAP"        T(N) },
	{ "SEQNUM"        T(N) },
	{ "SKIP"          T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr HR_attr[] = {         /* HR attributes */
	{ "ALIGN"         T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "MD"            T(N) },
	{ "NOSHADE"       T(N) },
	{ "SIZE"          T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr IFRAME_attr[] = {     /* IFRAME attributes */
	{ "ALIGN"         T(N) },
	{ "CLASS"         T(c) },
	{ "FRAMEBORDER"   T(N) },
	{ "HEIGHT"        T(N) },
	{ "ID"            T(i) },
	{ "LONGDESC"      T(h) },
	{ "MARGINHEIGHT"  T(N) },
	{ "MARGINWIDTH"   T(N) },
	{ "NAME"          T(N) },
	{ "SCROLLING"     T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr IMG_attr[] = {        /* IMG attributes */
	{ "ALIGN"         T(N) },
	{ "ALT"           T(N) },
	{ "BORDER"        T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "HEIGHT"        T(N) },
	{ "HSPACE"        T(N) },
	{ "ID"            T(i) },
	{ "ISMAP"         T(N) },
	{ "ISOBJECT"      T(N) },
	{ "LANG"          T(N) },
	{ "LONGDESC"      T(h) },
	{ "MD"            T(N) },
	{ "NAME"          T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "UNITS"         T(N) },
	{ "USEMAP"        T(h) },
	{ "VSPACE"        T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr INPUT_attr[] = {      /* INPUT attributes */
	{ "ACCEPT"        T(N) },
	{ "ACCEPT-CHARSET" T(N) },
	{ "ACCESSKEY"     T(N) },
	{ "ALIGN"         T(N) },
	{ "ALT"           T(N) },
	{ "CHECKED"       T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "DISABLED"      T(N) },
	{ "ERROR"         T(N) },
	{ "HEIGHT"        T(N) },
	{ "ID"            T(i) },
	{ "ISMAP"         T(N) },
	{ "LANG"          T(N) },
	{ "MAX"           T(N) },
	{ "MAXLENGTH"     T(N) },
	{ "MD"            T(N) },
	{ "MIN"           T(N) },
	{ "NAME"          T(N) },
	{ "NOTAB"         T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONCHANGE"      T(N) },
	{ "ONFOCUS"       T(N) },
	{ "ONSELECT"      T(N) },
	{ "READONLY"      T(N) },
	{ "SIZE"          T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TABINDEX"      T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ "USEMAP"        T(N) },
	{ "VALUE"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr ISINDEX_attr[] = {    /* ISINDEX attributes */
	{ "ACTION"        T(h) },
	{ "CLASS"         T(c) },
	{ "DIR"           T(N) },
	{ "HREF"          T(h) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "PROMPT"        T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr KEYGEN_attr[] = {     /* KEYGEN attributes */
	{ "CHALLENGE"     T(N) },
	{ "CLASS"         T(c) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NAME"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr LABEL_attr[] = {      /* LABEL attributes */
	{ "ACCESSKEY"     T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "FOR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONFOCUS"       T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr LI_attr[] = {         /* LI attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DINGBAT"       T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "MD"            T(N) },
	{ "SKIP"          T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ "VALUE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr LINK_attr[] = {       /* LINK attributes */
	{ "CHARSET"       T(N) },
	{ "CLASS"         T(c) },
	{ "DIR"           T(N) },
	{ "HREF"          T(h) },
	{ "HREFLANG"      T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "MEDIA"         T(N) },
	{ "REL"           T(N) },
	{ "REV"           T(N) },
	{ "STYLE"         T(N) },
	{ "TARGET"        T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr MAP_attr[] = {        /* MAP attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NAME"          T(i) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr MATH_attr[] = {       /* MATH attributes */
	{ "BOX"           T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr META_attr[] = {       /* META attributes */
	{ "CHARSET"       T(N) },
	{ "CONTENT"       T(N) },
	{ "HTTP-EQUIV"    T(N) },
	{ "NAME"          T(N) },
	{ "SCHEME"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr NEXTID_attr[] = {     /* NEXTID attributes */
	{ "N"             T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr NOTE_attr[] = {       /* NOTE attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "MD"            T(N) },
	{ "ROLE"          T(x) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr OBJECT_attr[] = {     /* OBJECT attributes */
	{ "ALIGN"         T(N) },
	{ "ARCHIVE"       T(N) },
	{ "BORDER"        T(N) },
	{ "CLASS"         T(c) },
	{ "CLASSID"       T(h) },
	{ "CODEBASE"      T(h) },
	{ "CODETYPE"      T(N) },
	{ "DATA"          T(h) },
	{ "DECLARE"       T(N) },
	{ "DIR"           T(N) },
	{ "HEIGHT"        T(N) },
	{ "HSPACE"        T(N) },
	{ "ID"            T(i) },
	{ "ISMAP"         T(N) },
	{ "LANG"          T(N) },
	{ "NAME"          T(N) },
	{ "NOTAB"         T(N) },
	{ "SHAPES"        T(N) },
	{ "STANDBY"       T(N) },
	{ "STYLE"         T(N) },
	{ "TABINDEX"      T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ "USEMAP"        T(h) },
	{ "VSPACE"        T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr OL_attr[] = {         /* OL attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "COMPACT"       T(N) },
	{ "CONTINUE"      T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "SEQNUM"        T(N) },
	{ "START"         T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr OPTION_attr[] = {     /* OPTION attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "DISABLED"      T(N) },
	{ "ERROR"         T(N) },
	{ "ID"            T(i) },
	{ "LABEL"         T(N) },
	{ "LANG"          T(N) },
	{ "SELECTED"      T(N) },
	{ "SHAPE"         T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "VALUE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr OVERLAY_attr[] = {    /* OVERLAY attributes */
	{ "CLASS"         T(c) },
	{ "HEIGHT"        T(N) },
	{ "ID"            T(i) },
	{ "IMAGEMAP"      T(N) },
	{ "MD"            T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "UNITS"         T(N) },
	{ "WIDTH"         T(N) },
	{ "X"             T(N) },
	{ "Y"             T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr P_attr[] = {          /* P attributes */
	{ "ALIGN"         T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NOWRAP"        T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr PARAM_attr[] = {      /* PARAM attributes */
	{ "ACCEPT"        T(N) },
	{ "ACCEPT-CHARSET" T(N) },
	{ "ACCEPT-ENCODING" T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DATA"          T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NAME"          T(N) },
	{ "OBJECT"        T(N) },
	{ "REF"           T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ "VALUE"         T(N) },
	{ "VALUEREF"      T(N) },
	{ "VALUETYPE"     T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr Q_attr[] = {          /* Q attributes */
	{ "CITE"          T(h) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr SCRIPT_attr[] = {     /* SCRIPT attributes */
	{ "CHARSET"       T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DEFER"         T(N) },
	{ "DIR"           T(N) },
	{ "EVENT"         T(N) },
	{ "FOR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "LANGUAGE"      T(N) },
	{ "NAME"          T(N) },
	{ "SCRIPTENGINE"  T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr SELECT_attr[] = {     /* SELECT attributes */
	{ "ALIGN"         T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "DISABLED"      T(N) },
	{ "ERROR"         T(N) },
	{ "HEIGHT"        T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "MD"            T(N) },
	{ "MULTIPLE"      T(N) },
	{ "NAME"          T(N) },
	{ "NOTAB"         T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONCHANGE"      T(N) },
	{ "ONFOCUS"       T(N) },
	{ "SIZE"          T(N) },
	{ "STYLE"         T(N) },
	{ "TABINDEX"      T(N) },
	{ "TITLE"         T(N) },
	{ "UNITS"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr STYLE_attr[] = {      /* STYLE attributes */
	{ "CLASS"         T(c) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "MEDIA"         T(N) },
	{ "NOTATION"      T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr TAB_attr[] = {        /* TAB attributes */
	{ "ALIGN"         T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "DP"            T(N) },
	{ "ID"            T(i) },
	{ "INDENT"        T(N) },
	{ "LANG"          T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "TO"            T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr TABLE_attr[] = {      /* TABLE attributes */
	{ "ALIGN"         T(N) },
	{ "BACKGROUND"    T(h) },
	{ "BORDER"        T(N) },
	{ "CELLPADDING"   T(N) },
	{ "CELLSPACING"   T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "COLS"          T(N) },
	{ "COLSPEC"       T(N) },
	{ "DIR"           T(N) },
	{ "DP"            T(N) },
	{ "FRAME"         T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NOFLOW"        T(N) },
	{ "NOWRAP"        T(N) },
	{ "RULES"         T(N) },
	{ "STYLE"         T(N) },
	{ "SUMMARY"       T(N) },
	{ "TITLE"         T(N) },
	{ "UNITS"         T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr TD_attr[] = {         /* TD attributes */
	{ "ABBR"          T(N) },
	{ "ALIGN"         T(N) },
	{ "AXES"          T(N) },
	{ "AXIS"          T(N) },
	{ "BACKGROUND"    T(h) },
	{ "CHAR"          T(N) },
	{ "CHAROFF"       T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "COLSPAN"       T(N) },
	{ "DIR"           T(N) },
	{ "DP"            T(N) },
	{ "HEADERS"       T(N) },
	{ "HEIGHT"        T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NOWRAP"        T(N) },
	{ "ROWSPAN"       T(N) },
	{ "SCOPE"         T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "VALIGN"        T(N) },
	{ "WIDTH"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr TEXTAREA_attr[] = {   /* TEXTAREA attributes */
	{ "ACCEPT-CHARSET" T(N) },
	{ "ACCESSKEY"     T(N) },
	{ "ALIGN"         T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "COLS"          T(N) },
	{ "DIR"           T(N) },
	{ "DISABLED"      T(N) },
	{ "ERROR"         T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NAME"          T(N) },
	{ "NOTAB"         T(N) },
	{ "ONBLUR"        T(N) },
	{ "ONCHANGE"      T(N) },
	{ "ONFOCUS"       T(N) },
	{ "ONSELECT"      T(N) },
	{ "READONLY"      T(N) },
	{ "ROWS"          T(N) },
	{ "STYLE"         T(N) },
	{ "TABINDEX"      T(N) },
	{ "TITLE"         T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr TR_attr[] = {         /* TBODY attributes */
	{ "ALIGN"         T(N) },
	{ "CHAR"          T(N) },
	{ "CHAROFF"       T(N) },
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "DIR"           T(N) },
	{ "DP"            T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "NOWRAP"        T(N) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "VALIGN"        T(N) },
	{ 0               T(N) }	/* Terminate list */
};

static const attr UL_attr[] = {         /* DIR attributes */
	{ "CLASS"         T(c) },
	{ "CLEAR"         T(N) },
	{ "COMPACT"       T(N) },
	{ "DINGBAT"       T(N) },
	{ "DIR"           T(N) },
	{ "ID"            T(i) },
	{ "LANG"          T(N) },
	{ "MD"            T(N) },
	{ "PLAIN"         T(N) },
	{ "SRC"           T(h) },
	{ "STYLE"         T(N) },
	{ "TITLE"         T(N) },
	{ "TYPE"          T(N) },
	{ "WRAP"          T(N) },
	{ 0               T(N) }	/* Terminate list */
};

/* *INDENT-ON* */

/* justification-flags */
#undef N
#undef i
#undef h
#undef c
#undef x

#undef T

/* tag-names */
#undef A
#undef ABBR
#undef ACRONYM
#undef ADDRESS
#undef APPLET
#undef AREA
#undef AU
#undef AUTHOR
#undef B
#undef BANNER
#undef BASE
#undef BASEFONT
#undef BDO
#undef BGSOUND
#undef BIG
#undef BLINK
#undef BLOCKQUOTE
#undef BODY
#undef BODYTEXT
#undef BQ
#undef BR
#undef BUTTON
#undef CAPTION
#undef CENTER
#undef CITE
#undef CODE
#undef COL
#undef COLGROUP
#undef COMMENT
#undef CREDIT
#undef DD
#undef DEL
#undef DFN
#undef DIR
#undef DIV
#undef DL
#undef DLC
#undef DT
#undef EM
#undef EMBED
#undef FIELDSET
#undef FIG
#undef FN
#undef FONT
#undef FORM
#undef FRAME
#undef FRAMESET
#undef H1
#undef H2
#undef H3
#undef H4
#undef H5
#undef H6
#undef HEAD
#undef HR
#undef HTML
#undef HY
#undef I
#undef IFRAME
#undef IMG
#undef INPUT
#undef INS
#undef ISINDEX
#undef KBD
#undef KEYGEN
#undef LABEL
#undef LEGEND
#undef LH
#undef LI
#undef LINK
#undef LISTING
#undef MAP
#undef MARQUEE
#undef MATH
#undef MENU
#undef META
#undef NEXTID
#undef NOFRAMES
#undef NOTE
#undef OBJECT
#undef OL
#undef OPTION
#undef OVERLAY
#undef P
#undef PARAM
#undef PLAINTEXT
#undef PRE
#undef Q
#undef S
#undef SAMP
#undef SCRIPT
#undef SELECT
#undef SHY
#undef SMALL
#undef SPAN
#undef SPOT
#undef STRIKE
#undef STRONG
#undef STYLE
#undef SUB
#undef SUP
#undef TAB
#undef TABLE
#undef TBODY
#undef TD
#undef TEXTAREA
#undef TEXTFLOW
#undef TFOOT
#undef TH
#undef THEAD
#undef TITLE
#undef TR
#undef TT
#undef U
#undef UL
#undef VAR
#undef WBR
#undef XMP
#undef OBJECT_PCDATA

/* these definitions are used in the tags-tables */
#undef P
#undef P_
#ifdef USE_COLOR_STYLE
#define P_(x) #x, (sizeof #x) -1
#define NULL_HTTag_ NULL, 0
#else
#define P_(x) #x
#define NULL_HTTag_ NULL
#endif

#ifdef USE_JUSTIFY_ELTS
#define P(x) P_(x), 1
#define P0(x) P_(x), 0
#define NULL_HTTag NULL_HTTag_,0
#else
#define P(x) P_(x)
#define P0(x) P_(x)
#define NULL_HTTag NULL_HTTag_
#endif

#define ATTR_DATA(name) name##_attr, HTML_##name##_ATTRIBUTES, name##_attr_type

#endif /* once_HTMLDTD */
/* *INDENT-OFF* */
static const HTTag tags_table1[HTML_ALL_ELEMENTS] = {
 { P(A),           ATTR_DATA(A),           SGML_MIXED,   T_A},
 { P(ABBR),        ATTR_DATA(GEN),         SGML_MIXED,   T_ABBR},
 { P(ACRONYM),     ATTR_DATA(GEN),         SGML_MIXED,   T_ACRONYM},
 { P(ADDRESS),     ATTR_DATA(ADDRESS),     SGML_MIXED,   T_ADDRESS},
 { P(APPLET),      ATTR_DATA(APPLET),      SGML_MIXED,   T_APPLET},
 { P(AREA),        ATTR_DATA(AREA),        SGML_EMPTY,   T_AREA},
 { P(AU),          ATTR_DATA(GEN),         SGML_MIXED,   T_AU},
 { P(AUTHOR),      ATTR_DATA(GEN),         SGML_MIXED,   T_AUTHOR},
 { P(B),           ATTR_DATA(GEN),         SGML_MIXED,   T_B},
 { P0(BANNER),     ATTR_DATA(GEN),         SGML_MIXED,   T_BANNER},
 { P(BASE),        ATTR_DATA(BASE),        SGML_EMPTY,   T_BASE},
 { P(BASEFONT),    ATTR_DATA(FONT),        SGML_EMPTY,   T_BASEFONT},
 { P(BDO),         ATTR_DATA(GEN),         SGML_MIXED,   T_BDO},
 { P(BGSOUND),     ATTR_DATA(BGSOUND),     SGML_EMPTY,   T_BGSOUND},
 { P(BIG),         ATTR_DATA(GEN),         SGML_MIXED,   T_BIG},
 { P(BLINK),       ATTR_DATA(GEN),         SGML_MIXED,   T_BLINK},
 { P(BLOCKQUOTE),  ATTR_DATA(BQ),          SGML_MIXED,   T_BLOCKQUOTE},
 { P(BODY),        ATTR_DATA(BODY),        SGML_MIXED,   T_BODY},
 { P(BODYTEXT),    ATTR_DATA(BODYTEXT),    SGML_MIXED,   T_BODYTEXT},
 { P(BQ),          ATTR_DATA(BQ),          SGML_MIXED,   T_BQ},
 { P(BR),          ATTR_DATA(GEN),         SGML_EMPTY,   T_BR},
 { P(BUTTON),      ATTR_DATA(BUTTON),      SGML_MIXED,   T_BUTTON},
 { P(CAPTION),     ATTR_DATA(CAPTION),     SGML_MIXED,   T_CAPTION},
 { P(CENTER),      ATTR_DATA(DIV),         SGML_MIXED,   T_CENTER},
 { P(CITE),        ATTR_DATA(GEN),         SGML_MIXED,   T_CITE},
 { P(CODE),        ATTR_DATA(GEN),         SGML_MIXED,   T_CODE},
 { P(COL),         ATTR_DATA(COL),         SGML_EMPTY,   T_COL},
 { P(COLGROUP),    ATTR_DATA(COL),         SGML_ELEMENT, T_COLGROUP},
 { P(COMMENT),     ATTR_DATA(GEN),         SGML_PCDATA,  T_COMMENT},
 { P(CREDIT),      ATTR_DATA(GEN),         SGML_MIXED,   T_CREDIT},
 { P(DD),          ATTR_DATA(GEN),         SGML_MIXED,   T_DD},
 { P(DEL),         ATTR_DATA(DEL),         SGML_MIXED,   T_DEL},
 { P(DFN),         ATTR_DATA(GEN),         SGML_MIXED,   T_DFN},
 { P(DIR),         ATTR_DATA(UL),          SGML_MIXED,   T_DIR},
 { P(DIV),         ATTR_DATA(DIV),         SGML_MIXED,   T_DIV},
 { P(DL),          ATTR_DATA(DL),          SGML_MIXED,   T_DL},
 { P(DLC),         ATTR_DATA(DL),          SGML_MIXED,   T_DLC},
 { P(DT),          ATTR_DATA(GEN),         SGML_MIXED,   T_DT},
 { P(EM),          ATTR_DATA(GEN),         SGML_MIXED,   T_EM},
 { P(EMBED),       ATTR_DATA(EMBED),       SGML_EMPTY,   T_EMBED},
 { P(FIELDSET),    ATTR_DATA(GEN),         SGML_MIXED,   T_FIELDSET},
 { P(FIG),         ATTR_DATA(FIG),         SGML_MIXED,   T_FIG},
 { P(FN),          ATTR_DATA(GEN),         SGML_MIXED,   T_FN},
 { P(FONT),        ATTR_DATA(FONT),        SGML_MIXED,   T_FONT},
 { P(FORM),        ATTR_DATA(FORM),        SGML_MIXED,   T_FORM},
 { P(FRAME),       ATTR_DATA(FRAME),       SGML_EMPTY,   T_FRAME},
 { P(FRAMESET),    ATTR_DATA(FRAMESET),    SGML_ELEMENT, T_FRAMESET},
 { P0(H1),         ATTR_DATA(H),           SGML_MIXED,   T_H1},
 { P0(H2),         ATTR_DATA(H),           SGML_MIXED,   T_H2},
 { P0(H3),         ATTR_DATA(H),           SGML_MIXED,   T_H3},
 { P0(H4),         ATTR_DATA(H),           SGML_MIXED,   T_H4},
 { P0(H5),         ATTR_DATA(H),           SGML_MIXED,   T_H5},
 { P0(H6),         ATTR_DATA(H),           SGML_MIXED,   T_H6},
 { P(HEAD),        ATTR_DATA(GEN),         SGML_ELEMENT, T_HEAD},
 { P(HR),          ATTR_DATA(HR),          SGML_EMPTY,   T_HR},
 { P(HTML),        ATTR_DATA(GEN),         SGML_MIXED,   T_HTML},
 { P(HY),          ATTR_DATA(GEN),         SGML_EMPTY,   T_HY},
 { P(I),           ATTR_DATA(GEN),         SGML_MIXED,   T_I},
 { P(IFRAME),      ATTR_DATA(IFRAME),      SGML_MIXED,   T_IFRAME},
 { P(IMG),         ATTR_DATA(IMG),         SGML_EMPTY,   T_IMG},
 { P(INPUT),       ATTR_DATA(INPUT),       SGML_EMPTY,   T_INPUT},
 { P(INS),         ATTR_DATA(DEL),         SGML_MIXED,   T_INS},
 { P(ISINDEX),     ATTR_DATA(ISINDEX),     SGML_EMPTY,   T_ISINDEX},
 { P(KBD),         ATTR_DATA(GEN),         SGML_MIXED,   T_KBD},
 { P(KEYGEN),      ATTR_DATA(KEYGEN),      SGML_EMPTY,   T_KEYGEN},
 { P(LABEL),       ATTR_DATA(LABEL),       SGML_MIXED,   T_LABEL},
 { P(LEGEND),      ATTR_DATA(CAPTION),     SGML_MIXED,   T_LEGEND},
 { P(LH),          ATTR_DATA(GEN),         SGML_MIXED,   T_LH},
 { P(LI),          ATTR_DATA(LI),          SGML_MIXED,   T_LI},
 { P(LINK),        ATTR_DATA(LINK),        SGML_EMPTY,   T_LINK},
 { P(LISTING),     ATTR_DATA(GEN),         SGML_LITTERAL,T_LISTING},
 { P(MAP),         ATTR_DATA(MAP),         SGML_ELEMENT, T_MAP},
 { P(MARQUEE),     ATTR_DATA(GEN),         SGML_MIXED,   T_MARQUEE},
 { P(MATH),        ATTR_DATA(MATH),        SGML_PCDATA,  T_MATH},
 { P(MENU),        ATTR_DATA(UL),          SGML_MIXED,   T_MENU},
 { P(META),        ATTR_DATA(META),        SGML_EMPTY,   T_META},
 { P(NEXTID),      ATTR_DATA(NEXTID),      SGML_EMPTY,   T_NEXTID},
 { P(NOFRAMES),    ATTR_DATA(GEN),         SGML_MIXED,   T_NOFRAMES},
 { P(NOTE),        ATTR_DATA(NOTE),        SGML_MIXED,   T_NOTE},
 { P(OBJECT),      ATTR_DATA(OBJECT),      SGML_LITTERAL,T_OBJECT},
 { P(OL),          ATTR_DATA(OL),          SGML_MIXED,   T_OL},
 { P(OPTION),      ATTR_DATA(OPTION),      SGML_PCDATA,  T_OPTION},
 { P(OVERLAY),     ATTR_DATA(OVERLAY),     SGML_PCDATA,  T_OVERLAY},
 { P(P),           ATTR_DATA(P),           SGML_MIXED,   T_P},
 { P(PARAM),       ATTR_DATA(PARAM),       SGML_EMPTY,   T_PARAM},
 { P(PLAINTEXT),   ATTR_DATA(GEN),         SGML_LITTERAL,T_PLAINTEXT},
 { P0(PRE),        ATTR_DATA(GEN),         SGML_MIXED,   T_PRE},
 { P(Q),           ATTR_DATA(Q),           SGML_MIXED,   T_Q},
 { P(S),           ATTR_DATA(GEN),         SGML_MIXED,   T_S},
 { P(SAMP),        ATTR_DATA(GEN),         SGML_MIXED,   T_SAMP},
 { P(SCRIPT),      ATTR_DATA(SCRIPT),      SGML_SCRIPT,  T_SCRIPT},
 { P(SELECT),      ATTR_DATA(SELECT),      SGML_ELEMENT, T_SELECT},
 { P(SHY),         ATTR_DATA(GEN),         SGML_EMPTY,   T_SHY},
 { P(SMALL),       ATTR_DATA(GEN),         SGML_MIXED,   T_SMALL},
 { P(SPAN),        ATTR_DATA(GEN),         SGML_MIXED,   T_SPAN},
 { P(SPOT),        ATTR_DATA(GEN),         SGML_EMPTY,   T_SPOT},
 { P(STRIKE),      ATTR_DATA(GEN),         SGML_MIXED,   T_STRIKE},
 { P(STRONG),      ATTR_DATA(GEN),         SGML_MIXED,   T_STRONG},
 { P(STYLE),       ATTR_DATA(STYLE),       SGML_CDATA,   T_STYLE},
 { P(SUB),         ATTR_DATA(GEN),         SGML_MIXED,   T_SUB},
 { P(SUP),         ATTR_DATA(GEN),         SGML_MIXED,   T_SUP},
 { P(TAB),         ATTR_DATA(TAB),         SGML_EMPTY,   T_TAB},
 { P(TABLE),       ATTR_DATA(TABLE),       SGML_ELEMENT, T_TABLE},
 { P(TBODY),       ATTR_DATA(TR),          SGML_ELEMENT, T_TBODY},
 { P(TD),          ATTR_DATA(TD),          SGML_MIXED,   T_TD},
 { P(TEXTAREA),    ATTR_DATA(TEXTAREA),    SGML_PCDATA,  T_TEXTAREA},
 { P(TEXTFLOW),    ATTR_DATA(BODYTEXT),    SGML_MIXED,   T_TEXTFLOW},
 { P(TFOOT),       ATTR_DATA(TR),          SGML_ELEMENT, T_TFOOT},
 { P(TH),          ATTR_DATA(TD),          SGML_MIXED,   T_TH},
 { P(THEAD),       ATTR_DATA(TR),          SGML_ELEMENT, T_THEAD},
 { P(TITLE),       ATTR_DATA(GEN),         SGML_PCDATA,  T_TITLE},
 { P(TR),          ATTR_DATA(TR),          SGML_MIXED,   T_TR},
 { P(TT),          ATTR_DATA(GEN),         SGML_MIXED,   T_TT},
 { P(U),           ATTR_DATA(GEN),         SGML_MIXED,   T_U},
 { P(UL),          ATTR_DATA(UL),          SGML_MIXED,   T_UL},
 { P(VAR),         ATTR_DATA(GEN),         SGML_MIXED,   T_VAR},
 { P(WBR),         ATTR_DATA(GEN),         SGML_EMPTY,   T_WBR},
 { P0(XMP),        ATTR_DATA(GEN),         SGML_LITTERAL,T_XMP},
/* additional (alternative variants), not counted in HTML_ELEMENTS: */
/* This one will be used as a temporary substitute within the parser when
   it has been signalled to parse OBJECT content as MIXED. - kw */
 { P(OBJECT),      ATTR_DATA(OBJECT),      SGML_MIXED,   T_OBJECT_PCDATA},
};
/* *INDENT-ON* */

#endif /* src_HTMLDTD_H1 */
