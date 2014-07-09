/*
 * $LynxId: HText.h,v 1.16 2010/09/25 11:41:08 tom Exp $
 *                                                           Rich Hypertext object for libWWW
 *                                RICH HYPERTEXT OBJECT
 *
 * This is the C interface to the Objective-C (or whatever) Style-oriented
 * HyperText class.  It is used when a style-oriented text object is available
 * or craeted in order to display hypertext.
 */
#ifndef HTEXT_H
#define HTEXT_H

#include <HTAnchor.h>
#include <HTStyle.h>
#include <HTStream.h>
#include <SGML.h>

#ifdef __cplusplus
extern "C" {
#endif
#ifndef THINK_C
#ifndef HyperText		/* Objective C version defined HyperText */
    typedef struct _HText HText;	/* Normal Library */
#endif
#else
    class CHyperText;		/* Mac Think-C browser hook */
    typedef CHyperText HText;
#endif

    extern HText *HTMainText;	/* Pointer to current main text */
    extern HTParentAnchor *HTMainAnchor;	/* Pointer to current text's anchor */

    extern const char *HTAppName;	/* Application name */
    extern const char *HTAppVersion;	/* Application version */

/*

Creation and deletion

  HTEXT_NEW: CREATE HYPERTEXT OBJECT

   There are several methods depending on how much you want to specify.  The
   output stream is used with objects which need to output the hypertext to a
   stream.  The structure is for objects which need to refer to the structure
   which is kep by the creating stream.

 */
    extern HText *HText_new(HTParentAnchor *anchor);

    extern HText *HText_new2(HTParentAnchor *anchor,
			     HTStream *output_stream);

    extern HText *HText_new3(HTParentAnchor *anchor,
			     HTStream *output_stream,
			     HTStructured * structure);

/*

  FREE HYPERTEXT OBJECT

 */
    extern void HText_free(HText *me);

/*

Object Building methods

   These are used by a parser to build the text in an object HText_beginAppend
   must be called, then any combination of other append calls, then
   HText_endAppend.  This allows optimised handling using buffers and caches
   which are flushed at the end.

 */
    extern void HText_beginAppend(HText *text);

    extern void HText_endAppend(HText *text);

/*

  SET THE STYLE FOR FUTURE TEXT

 */

    extern void HText_setStyle(HText *text, HTStyle *style);

/*

  ADD ONE CHARACTER

 */
    extern void HText_appendCharacter(HText *text, int ch);

/*

  ADD A ZERO-TERMINATED STRING

 */

    extern void HText_appendText(HText *text, const char *str);

/*

  NEW PARAGRAPH

   and similar things

 */
    extern void HText_appendParagraph(HText *text);

    extern void HText_appendLineBreak(HText *text);

    extern void HText_appendHorizontalRule(HText *text);

/*

  START/END SENSITIVE TEXT

 */

/*

   The anchor object is created and passed to HText_beginAnchor.  The senstive
   text is added to the text object, and then HText_endAnchor is called. 
   Anchors may not be nested.

 */
    extern int HText_beginAnchor(HText *text, int underline,
				 HTChildAnchor *anc);
    extern void HText_endAnchor(HText *text, int number);
    extern BOOL HText_isAnchorBlank(HText *text, int number);

/*

  APPEND AN INLINE IMAGE

   The image is handled by the creation of an anchor whose destination is the
   image document to be included.  The semantics is the intended inline display
   of the image.

   An alternative implementation could be, for example, to begin an anchor,
   append the alternative text or "IMAGE", then end the anchor.  This would
   simply generate some text linked to the image itself as a separate document.

 */
    extern void HText_appendImage(HText *text, HTChildAnchor *anc,
				  const char *alternative_text,
				  int alignment,
				  int isMap);

/*

  RETURN THE ANCHOR ASSOCIATED WITH THIS NODE

 */
    extern HTParentAnchor *HText_nodeAnchor(HText *me);

/*

Browsing functions

 */

/*

  BRING TO FRONT AND HIGHLIGHT IT

 */

    extern BOOL HText_select(HText *text);
    extern BOOL HText_selectAnchor(HText *text, HTChildAnchor *anchor);

/*

Editing functions

   These are called from the application.  There are many more functions not
   included here from the orginal text object.  These functions NEED NOT BE
   IMPLEMENTED in a browser which cannot edit.

 */
/*      Style handling:
*/
/*      Apply this style to the selection
*/
    extern void HText_applyStyle(HText *me, HTStyle *style);

/*      Update all text with changed style.
*/
    extern void HText_updateStyle(HText *me, HTStyle *style);

/*      Return style of  selection
*/
    extern HTStyle *HText_selectionStyle(HText *me, HTStyleSheet *sheet);

/*      Paste in styled text
*/
    extern void HText_replaceSel(HText *me, const char *aString,
				 HTStyle *aStyle);

/*      Apply this style to the selection and all similarly formatted text
 *      (style recovery only)
 */
    extern void HTextApplyToSimilar(HText *me, HTStyle *style);

/*      Select the first unstyled run.
 *      (style recovery only)
 */
    extern void HTextSelectUnstyled(HText *me, HTStyleSheet *sheet);

/*      Anchor handling:
*/
    extern void HText_unlinkSelection(HText *me);
    extern HTAnchor *HText_referenceSelected(HText *me);
    extern HTAnchor *HText_linkSelTo(HText *me, HTAnchor * anchor);

#ifdef __cplusplus
}
#endif
#endif				/* HTEXT_H */
