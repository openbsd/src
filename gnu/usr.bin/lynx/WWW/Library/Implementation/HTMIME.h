/*                   /Net/dxcern/userd/timbl/hypertext/WWW/Library/Implementation/HTMIME.html
                                       MIME PARSER

   The MIME parser stream presents a MIME document.  It recursively invokes the format
   manager to handle embedded formats.

   As well as stripping off and parsing the headers, the MIME parser has to parse any
   weirld MIME encodings it may meet within the body parts of messages, and must deal with
   multipart messages.

   This module is implemented to the level necessary for operation with WWW, but is not
   currently complete for any arbitrary MIME message.

   Check the source for latest additions to functionality.

   The MIME parser is complicated by the fact that WWW allows real binary to be sent, not
   ASCII encoded.  Therefore the netascii decoding is included in this module.  One cannot
   layer it by converting first from Net to local text, then decoding it.  Of course, for
   local files, the net ascii decoding is not needed.  There are therefore two creation
   routines.

 */
#ifndef HTMIME_H
#define HTMIME_H

#include <HTStream.h>
#include <HTAnchor.h>

/*
**  This function is for trimming off any paired
**  open- and close-double quotes from header values.
**  It does not parse the string for embedded quotes,
**  and will not modify the string unless both the
**  first and last characters are double-quotes. - FM
*/
extern void HTMIME_TrimDoubleQuotes PARAMS((
	char *		value));

/*

  INPUT: LOCAL TEXT

 */
extern HTStream * HTMIMEConvert PARAMS((HTPresentation * pres,
                                        HTParentAnchor * anchor,
                                        HTStream * sink));
/*

  INPUT: NET ASCII

 */
extern HTStream * HTNetMIME PARAMS((HTPresentation * pres,
                                        HTParentAnchor * anchor,
                                        HTStream * sink));
/*

  INPUT: Redirection message, parse headers only for Location if present

 */
extern HTStream * HTMIMERedirect PARAMS((HTPresentation * pres,
                                        HTParentAnchor * anchor,
                                        HTStream * sink));


/*

  For handling Japanese headers.

*/
extern void HTmmdecode PARAMS((
	char **	trg,
	char *	str));

extern int HTrjis PARAMS((
	char **	t,
	char *	s));

extern int HTmaybekanji PARAMS((
	int	c1,
	int	c2));

#endif /* !HTMIME_H */
