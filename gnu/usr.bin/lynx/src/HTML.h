/*					HTML to rich text converter for libwww
**
**			THE HTML TO RTF OBJECT CONVERTER
**
**  This interprets the HTML semantics.
*/
#ifndef HTML_H
#define HTML_H

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */
#include "UCDefs.h"
#include "UCAux.h"
#include "HTAnchor.h"
#include "HTMLDTD.h"

#ifdef SHORT_NAMES
#define HTMLPresentation        HTMLPren
#define HTMLPresent             HTMLPres
#endif /* SHORT_NAMES */

/* #define ATTR_CS_IN (me->T.output_utf8 ? me->UCLYhndl : 0) */
#define ATTR_CS_IN me->tag_charset

#define TRANSLATE_AND_UNESCAPE_ENTITIES(s, p, h) \
	LYUCFullyTranslateString(s, ATTR_CS_IN, current_char_set, YES, p, h, st_HTML)

#define TRANSLATE_AND_UNESCAPE_ENTITIES5(s,cs_from,cs_to,p,h) \
	LYUCFullyTranslateString(s, cs_from, cs_to, YES, p, h, st_HTML)

#define TRANSLATE_AND_UNESCAPE_ENTITIES6(s,cs_from,cs_to,spcls,p,h) \
	LYUCFullyTranslateString(s, cs_from, cs_to, spcls, p, h, st_HTML)

/*
 *  Strings from attributes which should be converted to some kind
 *  of "standard" representation (character encoding), was Latin-1,
 *  esp. URLs (incl. #fragments) and HTML NAME and ID stuff.
 */
#define TRANSLATE_AND_UNESCAPE_TO_STD(s) \
	LYUCFullyTranslateString(s, ATTR_CS_IN, ATTR_CS_IN, NO, NO, YES, st_URL)
#define UNESCAPE_FIELDNAME_TO_STD(s) \
	LYUCFullyTranslateString(s, ATTR_CS_IN, ATTR_CS_IN, NO, NO, YES, st_HTML)

extern CONST HTStructuredClass HTMLPresentation;

#ifdef Lynx_HTML_Handler
/*
**	This section is semi-private to HTML.c and it's helper modules. - FM
**	--------------------------------------------------------------------
*/

typedef struct _stack_element {
	HTStyle *	style;
	int		tag_number;
} stack_element;

/*		HTML Object
**		-----------
*/
#define MAX_NESTING 800		/* Should be checked by parser */

struct _HTStructured {
    CONST HTStructuredClass * 	isa;
    HTParentAnchor * 		node_anchor;
    HText * 			text;

    HTStream*			target;			/* Output stream */
    HTStreamClass		targetClass;		/* Output routines */

    HTChildAnchor *		CurrentA;	/* current HTML_A anchor */
    int				CurrentANum;	/* current HTML_A number */
    char *			base_href;	/* current HTML_BASE href */
    char *			map_address;	/* current HTML_MAP address */

    HTChunk 			title;		/* Grow by 128 */
    HTChunk			object;		/* Grow by 128 */
    BOOL			object_started;
    BOOL			object_declare;
    BOOL			object_shapes;
    BOOL			object_ismap;
    char *			object_usemap;
    char *			object_id;
    char *			object_title;
    char *			object_data;
    char *			object_type;
    char *			object_classid;
    char *			object_codebase;
    char *			object_codetype;
    char *			object_name;
    HTChunk			option;		/* Grow by 128 */
    BOOL			first_option;	/* First OPTION in SELECT? */
    char *			LastOptionValue;
    BOOL			LastOptionChecked;
    BOOL			select_disabled;
    HTChunk			textarea;	/* Grow by 128 */
    char *			textarea_name;
    int				textarea_name_cs;
    char *			textarea_accept_cs;
    char *			textarea_cols;
    int 			textarea_rows;
    int				textarea_disabled;
    char *			textarea_id;
    HTChunk			math;		/* Grow by 128 */
    HTChunk			style_block;	/* Grow by 128 */
    HTChunk			script;		/* Grow by 128 */

    /*
     *  Used for nested lists. - FM
     */
    int		List_Nesting_Level;	/* counter for list nesting level */
    int 	OL_Counter[12];		/* counter for ordered lists */
    char 	OL_Type[12];		/* types for ordered lists */
    int 	Last_OL_Count;		/* last count in ordered lists */
    char 	Last_OL_Type;		/* last type in ordered lists */

    int				Division_Level;
    short			DivisionAlignments[MAX_NESTING];
    int				Underline_Level;
    int				Quote_Level;

    BOOL			UsePlainSpace;
    BOOL			HiddenValue;
    int				lastraw;

    char *			comment_start;	/* for literate programming */
    char *			comment_end;

    HTTag *			current_tag;
    BOOL			style_change;
    HTStyle *			new_style;
    HTStyle *			old_style;
    int				current_default_alignment;
    BOOL			in_word;  /* Have just had a non-white char */
    stack_element 	stack[MAX_NESTING];
    stack_element 	*sp;		/* Style stack pointer */
    BOOL		stack_overrun;	/* Was MAX_NESTING exceeded? */
    int			skip_stack; /* flag to skip next style stack operation */

    /*
    **  Track if we are in an anchor, paragraph, address, base, etc.
    */
    BOOL		inA;
    BOOL		inAPPLET;
    BOOL		inAPPLETwithP;
    BOOL		inBadBASE;
    BOOL		inBadHREF;
    BOOL		inBadHTML;
    BOOL		inBASE;
    BOOL		inBoldA;
    BOOL		inBoldH;
    BOOL		inCAPTION;
    BOOL		inCREDIT;
    BOOL		inFIG;
    BOOL		inFIGwithP;
    BOOL		inFONT;
    BOOL		inFORM;
    BOOL		inLABEL;
    BOOL		inP;
    BOOL		inPRE;
    BOOL		inSELECT;
    BOOL		inTABLE;
    BOOL		inTEXTAREA;
    BOOL		inUnderline;

    BOOL		needBoldH;

    /*
    **  UCI and UCLYhndl give the UCInfo and charset registered for
    **  the HTML parser in the node_anchor's UCStages structure.  It
    **  indicates what is fed to the HTML parser as the stream of character
    **  data (not necessarily tags and attributes).  It should currently
    **  always be set to be the same as UCI and UCLhndl for the HTEXT stage
    **  in the node_anchor's UCStages structure, since the HTML parser sends
    **  its input character data to the output without further charset
    **  translation.
    */
    LYUCcharset	*	UCI;
    int			UCLYhndl;
    /*
    **  inUCI and inUCLYhndl indicate the UCInfo and charset which the
    **  HTML parser treats at the input charset.  It is normally set
    **  to the UCI and UCLhndl for the SGML parser in the node_anchor's
    **  UCStages structure (which may be a dummy, based on the MIME
    **  parser's UCI and UCLhndl in that structure, when we are handling
    **  a local file or non-http(s) gateway).  It could be changed
    **  temporarily by the HTML parser, for conversions of attribute
    **  strings, but should be reset once done. - FM
    */
    LYUCcharset	*	inUCI;
    int			inUCLYhndl;
    /*
    **  outUCI and outUCLYhndl indicate the UCInfo and charset which
    **  the HTML parser treats as the output charset.  It is normally
    **  set to its own UCI and UCLhndl.  It could be changed for
    **  conversions of attribute strings, but should be reset once
    **  done. - FM
    */
    LYUCcharset	*	outUCI;
    int			outUCLYhndl;
    /*
    **  T holds the transformation rules for conversions of strings
    **  between the input and output charsets by the HTML parser. - FM
    */
    UCTransParams	T;

    int 		tag_charset; /* charset for attribute values etc. */
};

extern  HTStyle *styles[HTML_ELEMENTS+31]; /* adding 24 nested list styles  */
					   /* and 3 header alignment styles */
					   /* and 3 div alignment styles    */

/*
 *	Semi-Private functions. - FM
 */
extern void HTML_put_character PARAMS((HTStructured *me, char c));
extern void HTML_put_string PARAMS((HTStructured *me, CONST char *s));
extern void HTML_write PARAMS((HTStructured *me, CONST char *s, int l));
extern int HTML_put_entity PARAMS((HTStructured *me, int entity_number));
extern void actually_set_style PARAMS((HTStructured * me));

/*	Style buffering avoids dummy paragraph begin/ends.
*/
#define UPDATE_STYLE if (me->style_change) { actually_set_style(me); }
#endif /* Lynx_HTML_Handler */

extern void strtolower PARAMS((char* i));

/*				P U B L I C
*/

/*
**  HTConverter to present HTML
*/
extern HTStream* HTMLToPlain PARAMS((
	HTPresentation *	pres,
	HTParentAnchor *	anchor,
	HTStream *		sink));

extern HTStream* HTMLParsedPresent PARAMS((
	HTPresentation *	pres,
	HTParentAnchor *	anchor,
	HTStream *		sink));

extern HTStream* HTMLToC PARAMS((
	HTPresentation *	pres,
	HTParentAnchor *	anchor,
	HTStream *		sink));

extern HTStream* HTMLPresent PARAMS((
	HTPresentation *	pres,
	HTParentAnchor *	anchor,
	HTStream *		sink));

extern HTStructured* HTML_new PARAMS((
	HTParentAnchor * anchor,
	HTFormat	format_out,
	HTStream *	target));

/*
**  Record error message as a hypertext object.
**
**  The error message should be marked as an error so that it can be
**  reloaded later.  This implementation just throws up an error message
**  and leaves the document unloaded.
**
**  On entry,
**      sink    is a stream to the output device if any
**      number  is the HTTP error number
**      message is the human readable message.
**  On exit,
**      a retrun code like HT_LOADED if object exists else 60; 0
*/
extern int HTLoadError PARAMS((
	HTStream *	sink,
	int		number,
	CONST char *	message));

#endif /* HTML_H */

