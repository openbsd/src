/*                                                SGML parse and stream definition for libwww
                               SGML AND STRUCTURED STREAMS
                                             
   The SGML parser is a state machine. It is called for every character
   
   of the input stream. The DTD data structure contains pointers
   
   to functions which are called to implement the actual effect of the
   
   text read. When these functions are called, the attribute structures pointed to by the
   DTD are valid, and the function is passed a pointer to the curent tag structure, and an
   "element stack" which represents the state of nesting within SGML elements.
   
   The following aspects are from Dan Connolly's suggestions:  Binary search, Strcutured
   object scheme basically, SGML content enum type.
   
   (c) Copyright CERN 1991 - See Copyright.html
   
 */
#ifndef SGML_H
#define SGML_H

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */
#include "HTStream.h"
#include "HTAnchor.h"

/*

SGML content types

 */
typedef enum _SGMLContent {
    SGML_EMPTY,    /* No content. */
    SGML_LITTERAL, /* Literal character data.  Recognize exact close tag only.
		      Old www server compatibility only!  Not SGML */
    SGML_CDATA,    /* Character data.  Recognize </ only. */
    SGML_RCDATA,   /* Replaceable character data.  Recognize </ and &ref; */
    SGML_MIXED,    /* Elements and parsed character data.
		      Recognize all markup. */
    SGML_ELEMENT,  /* Any data found will be returned as an error. */
    SGML_PCDATA    /* Added by KW. */
} SGMLContent;


typedef struct {
    char *      name;           /* The (constant) name of the attribute */
                                /* Could put type info in here */
} attr;

typedef enum _TagClass {
    /* textflow */
    Tgc_FONTlike	= 0x00001,/* S,STRIKE,I,B,TT,U,BIG,SMALL,STYLE,BLINK;BR,TAB */
    Tgc_EMlike		= 0x00002, /* EM,STRONG,DFN,CODE,SAMP,KBD,VAR,CITE,Q,INS,DEL,SPAN,.. */
    Tgc_MATHlike	= 0x00004, /* SUB,SUP,MATH,COMMENT */
    Tgc_Alike		= 0x00008, /* A */
    Tgc_formula		= 0x00010, /* not used until math is supported better... */
    /* used for special structures: forms, tables,... */
    Tgc_TRlike		= 0x00020,/* TR and similar */
    Tgc_SELECTlike	= 0x00040,/* SELECT,INPUT,TEXTAREA(,...) */
    /* structure */
    Tgc_FORMlike	= 0x00080,/* FORM itself */
    Tgc_Plike		= 0x00100, /* P,H1..H6,... structures containing text or
				    insertion but not other structures */
    Tgc_DIVlike		= 0x00200, /* ADDRESS,FIG,BDO,NOTE,FN,DIV,CENTER;FIG
				    structures which can contain other structures */
    Tgc_LIlike		= 0x00400, /* LH,LI,DT,DD;TH,TD structure-like, only valid
				    within certain other structures */
    Tgc_ULlike		= 0x00800, /* UL,OL,DL,DIR,MENU;TABLE;XMP,LISTING
				    special in some way, cannot contain (parsed)
				    text directly */
    /* insertions */
    Tgc_BRlike		= 0x01000,/* BR,IMG,TAB allowed in any text */
    Tgc_APPLETlike	= 0x02000, /* APPLET,OBJECT,EMBED,SCRIPT */
    Tgc_HRlike		= 0x04000, /* HR,MARQUEE can contain all kinds of things
				    and/or are not allowed (?) in running text */
    Tgc_MAPlike		= 0x08000, /* MAP,AREA some specials that never contain
				    (directly or indirectly) other things than
				    special insertions */
    Tgc_outer		= 0x10000, /* HTML,FRAMESET,FRAME,PLAINTEXT; */
    Tgc_BODYlike	= 0x20000, /* BODY,BODYTEXT,NOFRAMES,TEXTFLOW; */
    Tgc_HEADstuff	= 0x40000, /* HEAD,BASE,STYLE,TITLE; */
    /* special relations */
    Tgc_same		= 0x80000
} TagClass;

/* Some more properties of tags (or rather, elements) and rules how
   to deal with them. - kw */
typedef enum _TagFlags {
    Tgf_endO		= 0x00001, /* end tag can be Omitted */
    Tgf_startO		= 0x00002, /* start tag can be Omitted */
    Tgf_mafse   	= 0x00004, /* Make Attribute-Free Start-tag End instead
				      (if found invalid) */
    Tgf_strict		= 0x00008  /* Ignore contained invalid elements,
				      don't pass them on */
} TagFlags;

/*              A tag structure describes an SGML element.
**              -----------------------------------------
**
**
**      name            is the string which comes after the tag opener "<".
**
**      attributes      points to a zero-terminated array
**                      of attribute names.
**
**      litteral        determines how the SGML engine parses the charaters
**                      within the element. If set, tag openers are ignored
**                      except for that which opens a matching closing tag.
**
*/
typedef struct _tag HTTag;
struct _tag{
    char *      name;                   /* The name of the tag */
    attr *      attributes;             /* The list of acceptable attributes */
    int         number_of_attributes;   /* Number of possible attributes */
    SGMLContent contents;               /* End only on end tag @@ */
    TagClass	tagclass,
	contains,	/* which classes of elements this one can contain directly */
	icontains,	/* which classes of elements this one can contain indirectly */
	contained,	/* in which classes can this tag be contained ? */
	icontained,	/* in which classes can this tag be indirectly contained ? */
	canclose;	/* which classes of elements can this one close
			   if something looks wrong ? */
    TagFlags	flags;
};


/*              DTD Information
**              ---------------
**
**  Not the whole DTD, but all this parser uses of it.
*/
typedef struct {
    char* name;
    long code;
} UC_entity_info;

typedef struct {
    HTTag *             tags;           /* Must be in strcmp order by name */
    int                 number_of_tags;
    CONST char **       entity_names;   /* Must be in strcmp order by name */
    size_t              number_of_entities;
    CONST UC_entity_info * unicode_entity_info; /* strcmp order by name */
    size_t              number_of_unicode_entities;
			/*
			**  All calls to unicode_entities table should be done
			**  through HTMLGetEntityUCValue (LYCharSets.c) only.
			**  unicode_entities table now holds *all*
			**  old-style entities too.
			*/
} SGML_dtd;


/*      SGML context passed to parsers
*/
typedef struct _HTSGMLContext *HTSGMLContext;   /* Hidden */


/*__________________________________________________________________________
*/

/*

Structured Object definition

   A structured object is something which can reasonably be represented
   in SGML.  I'll rephrase that.  A structured object is am ordered
   tree-structured arrangement of data which is representable as text.
   The SGML parer outputs to a Structured object.  A Structured object
   can output its contents to another Structured Object. It's a kind of
   typed stream. The architecure is largely Dan Conolly's. Elements and
   entities are passed to the sob by number, implying a knowledge of the
   DTD.  Knowledge of the SGML syntax is not here, though.
   
   Superclass: HTStream
   
   The creation methods will vary on the type of Structured Object.
   Maybe the callerData is enough info to pass along.
   
 */
typedef struct _HTStructured HTStructured;

typedef struct _HTStructuredClass{

        char*  name;                            /* Just for diagnostics */

        void (*_free) PARAMS((
                HTStructured*   me));

        void (*_abort) PARAMS((
                HTStructured*   me,
                HTError         e));
                
        void (*put_character) PARAMS((
                HTStructured*   me,
                char            ch));
                                
        void (*put_string) PARAMS((
                HTStructured*   me,
                CONST char *    str));
                
        void (*_write) PARAMS((
                HTStructured*   me,
                CONST char *    str,
                int             len));
                
        void (*start_element) PARAMS((
                HTStructured*   me,
                int             element_number,
                CONST BOOL*     attribute_present,
                CONST char**    attribute_value,
		int		charset,
		char **		include));
                
        void (*end_element) PARAMS((
                HTStructured*   me,
                int             element_number,
		char **		include));

        int (*put_entity) PARAMS((
                HTStructured*   me,
                int             entity_number));
                
}HTStructuredClass;

/*
  Equivalents to the following functions possibly could be generalised
  into additional HTStructuredClass members.  FOr now they don't do
  anything target-specific. - kw
  */
extern BOOLEAN LYCheckForCSI PARAMS((HTParentAnchor *anchor, char **url));
extern void LYDoCSI PARAMS((char *url, CONST char *comment, char **csi));

/*

Find a Tag by Name

   Returns a pointer to the tag within the DTD.
   
 */
extern HTTag * SGMLFindTag PARAMS((
	CONST SGML_dtd *	dtd,
	CONST char *		string));


/*

Create an SGML parser

 */
/*
** On entry,
**      dtd             must point to a DTD structure as defined above
**      callbacks       must point to user routines.
**      callData        is returned in callbacks transparently.
** On exit,
**              The default tag starter has been processed.
*/
extern HTStream * SGML_new PARAMS((
	CONST SGML_dtd *	dtd,
	HTParentAnchor *	anchor,
	HTStructured *		target));

extern CONST HTStreamClass SGMLParser;

#endif  /* SGML_H */

/*

    */
