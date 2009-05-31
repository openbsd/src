/*                                                       HTStyle: Style management for libwww
                              STYLE DEFINITION FOR HYPERTEXT

   Styles allow the translation between a logical property of a piece of text
   and its physical representation.

   A StyleSheet is a collection of styles, defining the translation necessary
   to represent a document.  It is a linked list of styles.

Overriding this module

   Why is the style structure declared in the HTStyle.h module, instead of
   having the user browser define the structure, and the HTStyle routines just
   use sizeof() for copying?

   It's not obvious whether HTStyle.c should be common code.  It's useful to
   have common code for loading style sheets, especially if the movement toward
   standard style sheets gets going.

   If it IS common code, then both the hypertext object and HTStyle.c must know
   the structure of a style, so HTStyle.h is a suitable place to put that. 
   HTStyle.c has to be compiled with a knowledge of the

   It we take it out of the library, then of course HTStyle could be declared
   as an undefined structure.  The only references to it are in the
   structure-flattening code HTML.c and HTPlain.c, which only use
   HTStypeNamed().

   You can in any case override this function in your own code, which will
   prevent the HTStyle from being loaded.  You will be able to redefine your
   style structure in this case without problems, as no other moule needs to
   know it.

 */
#ifndef HTStyle_H
#define HTStyle_H

#include <HTAnchor.h>

typedef long int HTFont;	/* Dummy definition instead */

#ifdef NeXT_suppressed
#include <appkit/appkit.h>
typedef NXCoord HTCoord;

#define HTParagraphStyle NXTextStyle
#define HTCoord NXCoord
typedef struct _color {
    float grey;
    int RGBColor;
} HTColor;

#else

typedef int HTCoord;		/* changed from float to int - kw */

typedef struct _HTParagraphStyle {
    HTCoord left_indent;	/* @@@@ junk! etc etc */
} HTParagraphStyle;

typedef int HTColor;		/* Sorry about the US spelling! */

#endif

#ifdef __cplusplus
extern "C" {
#endif
#define STYLE_NAME_LENGTH       80	/* @@@@@@@@@@@ */
    typedef struct {
	short kind;		/* only NX_LEFTTAB implemented */
	HTCoord position;	/* x coordinate for stop */
    } HTTabStop;

/*      The Style Structure
 *      -------------------
 */

    typedef struct _HTStyle {

/*      Style management information
*/
	struct _HTStyle *next;	/* Link for putting into stylesheet */
	char *name;		/* Style name */
	int id;			/* equivalent of name, for speed */
	char *SGMLTag;		/* Tag name to start */

/*      Character attributes    (a la NXRun)
*/
	HTFont font;		/* Font id */
	HTCoord fontSize;	/* The size of font, not independent */
	HTColor color;		/* text gray of current run */
	int superscript;	/* superscript (-sub) in points */

	HTAnchor *anchor;	/* Anchor id if any, else zero */

/*      Paragraph Attribtes     (a la NXTextStyle)
*/
	HTCoord indent1st;	/* how far first line in paragraph is
				   * indented */
	HTCoord leftIndent;	/* how far second line is indented */
	HTCoord rightIndent;	/* (Missing from NeXT version */
	short alignment;	/* quad justification */
	HTCoord lineHt;		/* line height */
	HTCoord descentLine;	/* descender bottom from baseline */
	const HTTabStop *tabs;	/* array of tab stops, 0 terminated */

	BOOL wordWrap;		/* Yes means wrap at space not char */
	BOOL freeFormat;	/* Yes means \n is just white space */
	HTCoord spaceBefore;	/* Omissions from NXTextStyle */
	HTCoord spaceAfter;
	int paraFlags;		/* Paragraph flags, bits as follows: */

#define PARA_KEEP       1	/* Do not break page within this paragraph */
#define PARA_WITH_NEXT  2	/* Do not break page after this paragraph */

#define HT_JUSTIFY 0		/* For alignment */
#define HT_LEFT 1
#define HT_RIGHT 2
#define HT_CENTER 3

    } HTStyle;

#define HT_ALIGN_NONE (-1)

/*      Style functions:
*/
    extern HTStyle *HTStyleNew(void);
    extern HTStyle *HTStyleNewNamed(const char *name);
    extern HTStyle *HTStyleFree(HTStyle *self);

#ifdef SUPRESS
    extern HTStyle *HTStyleRead(HTStyle *self, HTStream *stream);
    extern HTStyle *HTStyleWrite(HTStyle *self, HTStream *stream);
#endif
/*              Style Sheet
 *              -----------
 */
    typedef struct _HTStyleSheet {
	char *name;
	HTStyle *styles;
    } HTStyleSheet;

/*      Stylesheet functions:
*/
    extern HTStyleSheet *HTStyleSheetNew(void);
    extern HTStyleSheet *HTStyleSheetFree(HTStyleSheet *self);
    extern HTStyle *HTStyleNamed(HTStyleSheet *self, const char *name);
    extern HTStyle *HTStyleForParagraph(HTStyleSheet *self, HTParagraphStyle * paraStyle);
    extern HTStyle *HTStyleMatching(HTStyleSheet *self, HTStyle *style);

/* extern HTStyle * HTStyleForRun (HTStyleSheet *self, NXRun * run); */
    extern HTStyleSheet *HTStyleSheetAddStyle(HTStyleSheet *self, HTStyle *style);
    extern HTStyleSheet *HTStyleSheetRemoveStyle(HTStyleSheet *self, HTStyle *style);

#ifdef SUPPRESS
    extern HTStyleSheet *HTStyleSheetRead(HTStyleSheet *self, HTStream *stream);
    extern HTStyleSheet *HTStyleSheetWrite(HTStyleSheet *self, HTStream *stream);
#endif
#define CLEAR_POINTER ((void *)-1)	/* Pointer value means "clear me" */

/* DefaultStyle.c */
    extern HTStyleSheet *DefaultStyle(HTStyle ***result_array);

/* enum, use this instead of HTStyle name comparisons */
    enum HTStyle_Enum {
	ST_Normal = 0,
	ST_DivCenter,
	ST_DivLeft,
	ST_DivRight,
	ST_Banner,
	ST_Blockquote,
	ST_Bq,
	ST_Footnote,
	ST_List,
	ST_List1,
	ST_List2,
	ST_List3,
	ST_List4,
	ST_List5,
	ST_List6,
	ST_Menu,
	ST_Menu1,
	ST_Menu2,
	ST_Menu3,
	ST_Menu4,
	ST_Menu5,
	ST_Menu6,
	ST_Glossary,
	ST_Glossary1,
	ST_Glossary2,
	ST_Glossary3,
	ST_Glossary4,
	ST_Glossary5,
	ST_Glossary6,
	ST_GlossaryCompact,
	ST_GlossaryCompact1,
	ST_GlossaryCompact2,
	ST_GlossaryCompact3,
	ST_GlossaryCompact4,
	ST_GlossaryCompact5,
	ST_GlossaryCompact6,
	ST_Example,
	ST_Preformatted,
	ST_Listing,
	ST_Address,
	ST_Note,
	ST_Heading1,
	ST_Heading2,
	ST_Heading3,
	ST_Heading4,
	ST_Heading5,
	ST_Heading6,
	ST_HeadingCenter,
	ST_HeadingLeft,
	ST_HeadingRight
    };

#ifdef __cplusplus
}
#endif
#endif				/* HTStyle_H */
