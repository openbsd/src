/*                  /Net/dxcern/userd/timbl/hypertext/WWW/Library/Implementation/HTMLGen.html
                                      HTML GENERATOR
                                             
   This module converts structed stream into stream.  That is, given a stream
   to write to, it will give you a structured stream to
   
 */
#ifndef HTMLGEN_H
#define HTMLGEN_H

#include <HTML.h>
#include <HTStream.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern HTStructured *HTMLGenerator(HTStream *output);

    extern HTStream *HTPlainToHTML(HTPresentation *pres,
				   HTParentAnchor *anchor,
				   HTStream *sink);

#ifdef __cplusplus
}
#endif
#endif				/* HTMLGEN_H */
