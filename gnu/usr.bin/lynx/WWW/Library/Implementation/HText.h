/*                                                           Rich Hypertext object for libWWW
                                  RICH HYPERTEXT OBJECT

 */

/*

   This is the C interface to the Objective-C (or whatever) Style-oriented HyperText
   class.  It is used when a style-oriented text object is available or craeted in order to
   display hypertext.

 */
#ifndef HTEXT_H
#define HTEXT_H
#include <HTAnchor.h>
#include <HTStyle.h>
#include <HTStream.h>
#include <SGML.h>

#ifndef THINK_C
#ifndef HyperText               /* Objective C version defined HyperText */
typedef struct _HText HText;    /* Normal Library */
#endif
#else
class CHyperText;               /* Mac Think-C browser hook */
typedef CHyperText HText;
#endif

extern HText * HTMainText;              /* Pointer to current main text */
extern HTParentAnchor * HTMainAnchor;   /* Pointer to current text's anchor */

/*

Creation and deletion

  HTEXT_NEW: CREATE HYPERTEXT OBJECT

   There are several methods depending on how much you want to specify.  The output stream
   is used with objects which need to output the hypertext to a stream.  The structure is
   for objects which need to refer to the structure which is kep by the creating stream.

 */
 extern HText * HText_new PARAMS((HTParentAnchor * anchor));

 extern HText * HText_new2 PARAMS((HTParentAnchor * anchor,
                                HTStream * output_stream));

 extern HText * HText_new3 PARAMS((HTParentAnchor * anchor,
                                HTStream * output_stream,
                                HTStructured * structure));

/*

  FREE HYPERTEXT OBJECT

 */
extern void     HText_free PARAMS((HText * me));


/*

Object Building methods

   These are used by a parser to build the text in an object HText_beginAppend must be
   called, then any combination of other append calls, then HText_endAppend. This allows
   optimised handling using buffers and caches which are flushed at the end.

 */
extern void HText_beginAppend PARAMS((HText * text));

extern void HText_endAppend PARAMS((HText * text));

/*

  SET THE STYLE FOR FUTURE TEXT

 */

extern void HText_setStyle PARAMS((HText * text, HTStyle * style));

/*

  ADD ONE CHARACTER

 */
extern void HText_appendCharacter PARAMS((HText * text, char ch));

/*

  ADD A ZERO-TERMINATED STRING

 */

extern void HText_appendText PARAMS((HText * text, CONST char * str));

/*

  NEW PARAGRAPH

   and similar things

 */
extern void HText_appendParagraph PARAMS((HText * text));

extern void HText_appendLineBreak PARAMS((HText * text));

extern void HText_appendHorizontalRule PARAMS((HText * text));



/*

  START/END SENSITIVE TEXT

 */

/*

   The anchor object is created and passed to HText_beginAnchor.  The senstive text is
   added to the text object, and then HText_endAnchor is called. Anchors may not be
   nested.

 */
extern int HText_beginAnchor PARAMS((
	HText *		text,
	BOOL		underline,
	HTChildAnchor *	anc));
extern void HText_endAnchor PARAMS((HText * text, int number));


/*

  APPEND AN INLINE IMAGE

   The image is handled by the creation of an anchor whose destination is the image
   document to be included. The semantics is the intended inline display of the image.

   An alternative implementation could be, for example, to begin an anchor, append the
   alternative text or "IMAGE", then end the anchor.  This would simply generate some text
   linked to the image itself as a separate document.

 */
extern void HText_appendImage PARAMS((
        HText *         text,
        HTChildAnchor * anc,
        CONST char *    alternative_text,
        int             alignment,
        BOOL            isMap));

/*

  DUMP DIAGNOSTICS TO STDERR

 */

extern void HText_dump PARAMS((HText * me));

/*

  RETURN THE ANCHOR ASSOCIATED WITH THIS NODE

 */
extern HTParentAnchor * HText_nodeAnchor PARAMS((HText * me));


/*

Browsing functions

 */


/*

  BRING TO FRONT AND HIGHLIGHT IT

 */


extern BOOL HText_select PARAMS((HText * text));
extern BOOL HText_selectAnchor PARAMS((HText * text, HTChildAnchor* anchor));

/*

Editing functions

   These are called from the application.  There are many more functions not included here
   from the orginal text object.  These functions NEED NOT BE IMPLEMENTED in a browser
   which cannot edit.

 */
/*      Style handling:
*/
/*      Apply this style to the selection
*/
extern void HText_applyStyle PARAMS((HText * me, HTStyle *style));

/*      Update all text with changed style.
*/
extern void HText_updateStyle PARAMS((HText * me, HTStyle *style));

/*      Return style of  selection
*/
extern HTStyle * HText_selectionStyle PARAMS((
        HText * me,
        HTStyleSheet* sheet));

/*      Paste in styled text
*/
extern void HText_replaceSel PARAMS((HText * me,
        CONST char *aString,
        HTStyle* aStyle));

/*      Apply this style to the selection and all similarly formatted text
**      (style recovery only)
*/
extern void HTextApplyToSimilar PARAMS((HText * me, HTStyle *style));

/*      Select the first unstyled run.
**      (style recovery only)
*/
extern void HTextSelectUnstyled PARAMS((HText * me, HTStyleSheet *sheet));


/*      Anchor handling:
*/
extern void             HText_unlinkSelection PARAMS((HText * me));
extern HTAnchor *       HText_referenceSelected PARAMS((HText * me));
extern HTAnchor *       HText_linkSelTo PARAMS((HText * me, HTAnchor* anchor));


#endif /* HTEXT_H */
/*

   end */
