/*						  SGML parse and stream definition for libwww
			       SGML AND STRUCTURED STREAMS

   The SGML parser is a state machine.	It is called for every character
   of the input stream.	 The DTD data structure contains pointers
   to functions which are called to implement the actual effect of the
   text read. When these functions are called, the attribute structures pointed to by the
   DTD are valid, and the function is passed a pointer to the current tag structure, and an
   "element stack" which represents the state of nesting within SGML elements.

   The following aspects are from Dan Connolly's suggestions:  Binary search,
   Structured object scheme basically, SGML content enum type.

   (c) Copyright CERN 1991 - See Copyright.html

 */
#ifndef SGML_H
#define SGML_H

#include <HTStream.h>
#include <HTAnchor.h>
#include <LYJustify.h>

#ifdef __cplusplus
extern "C" {
#endif
/*
 *
 * SGML content types
 *
 */ typedef enum {
	SGML_EMPTY,		/* No content. */
	SGML_LITTERAL,		/* Literal character data.  Recognize exact close tag only.
				   Old www server compatibility only!  Not SGML */
	SGML_CDATA,		/* Character data.  Recognize </ only.
				   (But we treat it just as SGML_LITTERAL.) */
	SGML_SCRIPT,		/* Like CDATA, but allow it to be a comment */
	SGML_RCDATA,		/* Replaceable character data. Should recognize </ and &ref;
				   (but we treat it like SGML_MIXED for old times' sake). */
	SGML_MIXED,		/* Elements and parsed character data.
				   Recognize all markup. */
	SGML_ELEMENT,		/* Any data found should be regarded as an error.
				   (But we treat it just like SGML_MIXED.) */
	SGML_PCDATA		/* Should contain no elements but &ref; is parsed.
				   (We treat it like SGML_CDATA wrt. contained tags
				   i.e. pass them on literally, i.e. like we should
				   treat SGML_RCDATA) (added by KW). */
    } SGMLContent;

    typedef struct {
	const char *name;	/* The name of the attribute */
#ifdef USE_PRETTYSRC
	char type;		/* code of the type of the attribute. Code
				   values are in HTMLDTD.h */
#endif
    } attr;

    typedef int TagClass;

    /* textflow */
#define Tgc_FONTlike	0x00001	/* S,STRIKE,I,B,TT,U,BIG,SMALL,STYLE,BLINK;BR,TAB */
#define Tgc_EMlike	0x00002	/* EM,STRONG,DFN,CODE,SAMP,KBD,VAR,CITE,Q,INS,DEL,SPAN,.. */
#define Tgc_MATHlike	0x00004	/* SUB,SUP,MATH,COMMENT */
#define Tgc_Alike	0x00008	/* A */
#define Tgc_formula	0x00010	/* not used until math is supported better... */
    /* used for special structures: forms, tables,... */
#define Tgc_TRlike	0x00020	/* TR and similar */
#define Tgc_SELECTlike	0x00040	/* SELECT,INPUT,TEXTAREA(,...) */
    /* structure */
#define Tgc_FORMlike	0x00080	/* FORM itself */
#define Tgc_Plike	0x00100	/* P,H1..H6,... structures containing text or
				   insertion but not other structures */
#define Tgc_DIVlike	0x00200	/* ADDRESS,FIG,BDO,NOTE,FN,DIV,CENTER;FIG
				   structures which can contain other structures */
#define Tgc_LIlike	0x00400	/* LH,LI,DT,DD;TH,TD structure-like, only valid
				   within certain other structures */
#define Tgc_ULlike	0x00800	/* UL,OL,DL,DIR,MENU;TABLE;XMP,LISTING
				   special in some way, cannot contain (parsed)
				   text directly */
    /* insertions */
#define Tgc_BRlike	0x01000	/* BR,IMG,TAB allowed in any text */
#define Tgc_APPLETlike	0x02000	/* APPLET,OBJECT,EMBED,SCRIPT;BUTTON */
#define Tgc_HRlike	0x04000	/* HR,MARQUEE can contain all kinds of things
				   and/or are not allowed (?) in running text */
#define Tgc_MAPlike	0x08000	/* MAP,AREA some specials that never contain
				   (directly or indirectly) other things than
				   special insertions */
#define Tgc_outer	0x10000	/* HTML,FRAMESET,FRAME,PLAINTEXT; */
#define Tgc_BODYlike	0x20000	/* BODY,BODYTEXT,NOFRAMES,TEXTFLOW; */
#define Tgc_HEADstuff	0x40000	/* HEAD,BASE,STYLE,TITLE; */
    /* special relations */
#define Tgc_same	0x80000

/* Some more properties of tags (or rather, elements) and rules how
   to deal with them. - kw */
    typedef int TagFlags;

#define Tgf_endO	0x00001	/* end tag can be Omitted */
#define Tgf_startO	0x00002	/* start tag can be Omitted */
#define Tgf_mafse	0x00004	/* Make Attribute-Free Start-tag End instead
				   (if found invalid) */
#define Tgf_strict	0x00008	/* Ignore contained invalid elements,
				   don't pass them on; or other variant
				   handling for some content types */
#define Tgf_nreie	0x00010	/* Not Really Empty If Empty,
				   used by color style code */
#define Tgf_frecyc	0x00020	/* Pass element content on in a form that
				   allows recycling, i.e. don't translate to
				   output (display) character set yet (treat
				   content similar to attribute values) */
#define Tgf_nolyspcl	0x00040	/* Don't generate lynx special characters
				   for soft hyphen and various spaces (nbsp,
				   ensp,..) */

/*		A tag structure describes an SGML element.
 *		-----------------------------------------
 *
 *
 *	name		is the string which comes after the tag opener "<".
 *
 *	attributes	points to a zero-terminated array
 *			of attribute names.
 *
 *	litteral	determines how the SGML engine parses the characters
 *			within the element.  If set, tag openers are ignored
 *			except for that which opens a matching closing tag.
 *
 */
    typedef struct _tag HTTag;
    struct _tag {
	const char *name;	/* The name of the tag */
#ifdef USE_COLOR_STYLE
	int name_len;		/* The length of the name */
#endif
#ifdef EXP_JUSTIFY_ELTS
	BOOL can_justify;	/* justification allowed? */
#endif
	attr *attributes;	/* The list of acceptable attributes */
	int number_of_attributes;	/* Number of possible attributes */
	SGMLContent contents;	/* End only on end tag @@ */
	TagClass tagclass;
	TagClass contains;	/* which classes of elements this one can contain directly */
	TagClass icontains;	/* which classes of elements this one can contain indirectly */
	TagClass contained;	/* in which classes can this tag be contained ? */
	TagClass icontained;	/* in which classes can this tag be indirectly contained ? */
	TagClass canclose;	/* which classes of elements can this one close
				   if something looks wrong ? */
	TagFlags flags;
    };

/*		DTD Information
 *		---------------
 *
 *  Not the whole DTD, but all this parser uses of it.
 */
    typedef struct {
	HTTag *tags;		/* Must be in strcmp order by name */
	int number_of_tags;
	const char **entity_names;	/* Must be in strcmp order by name */
	size_t number_of_entities;
	/*  "entity_names" table probably unused,
	 *  see comments in HTMLDTD.c near the top
	 */
    } SGML_dtd;

/*	SGML context passed to parsers
*/
    typedef struct _HTSGMLContext *HTSGMLContext;	/* Hidden */

/*__________________________________________________________________________
*/

/*

Structured Object definition

   A structured object is something which can reasonably be represented
   in SGML.  I'll rephrase that.  A structured object is an ordered
   tree-structured arrangement of data which is representable as text.
   The SGML parser outputs to a Structured object.  A Structured object
   can output its contents to another Structured Object.  It's a kind of
   typed stream.  The architecture is largely Dan Conolly's.  Elements and
   entities are passed to the sob by number, implying a knowledge of the
   DTD.	 Knowledge of the SGML syntax is not here, though.

   Superclass: HTStream

   The creation methods will vary on the type of Structured Object.
   Maybe the callerData is enough info to pass along.

 */
    typedef struct _HTStructured HTStructured;

    typedef struct _HTStructuredClass {

	const char *name;	/* Just for diagnostics */

	void (*_free) (HTStructured * me);

	void (*_abort) (HTStructured * me, HTError e);

	void (*put_character) (HTStructured * me, char ch);

	void (*put_string) (HTStructured * me, const char *str);

	void (*put_block) (HTStructured * me, const char *str, int len);

	/* HTStreamClass ends here */

	int (*start_element) (HTStructured * me, int element_number,
			      const BOOL *attribute_present,
			      const char **attribute_value,
			      int charset,
			      char **include);

	int (*end_element) (HTStructured * me, int element_number,
			    char **include);

	int (*put_entity) (HTStructured * me, int entity_number);

    } HTStructuredClass;

/*
  Equivalents to the following functions possibly could be generalised
  into additional HTStructuredClass members.  For now they don't do
  anything target-specific. - kw
  */
    extern BOOLEAN LYCheckForCSI(HTParentAnchor *anchor, char **url);
    extern void LYDoCSI(char *url, const char *comment, char **csi);
    extern BOOLEAN LYCommentHacks(HTParentAnchor *anchor, const char *comment);

/*

Find a Tag by Name

   Returns a pointer to the tag within the DTD.

 */
    extern HTTag *SGMLFindTag(const SGML_dtd * dtd,
			      const char *string);

/*
 * Return the current offset within the file that SGML is parsing
 */
    extern int SGML_offset(void);

/*

Create an SGML parser

 */
/*
 * On entry,
 *	dtd		must point to a DTD structure as defined above
 *	callbacks	must point to user routines.
 *	callData	is returned in callbacks transparently.
 * On exit,
 *		The default tag starter has been processed.
 */
    extern HTStream *SGML_new(const SGML_dtd * dtd,
			      HTParentAnchor *anchor,
			      HTStructured * target);

    extern const HTStreamClass SGMLParser;

#ifdef __cplusplus
}
#endif
#endif				/* SGML_H */
