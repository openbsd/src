/*                  /Net/dxcern/userd/timbl/hypertext/WWW/Library/Implementation/HTPlain.html
                                    PLAIN TEXT OBJECT
                                             
 */
#ifndef HTPLAIN_H
#define HTPLAIN_H

#include <HTStream.h>
#include <HTAnchor.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern HTStream *HTPlainPresent(HTPresentation *pres,
				    HTParentAnchor *anchor,
				    HTStream *sink);

#ifdef __cplusplus
}
#endif
#endif				/* HTPLAIN_H */
