/* Finger protocol module for the WWW library */
/* History:
 *      21 Apr 94       Andrew Brooks
 */

#ifndef HTFINGER_H
#define HTFINGER_H

#include <HTAccess.h>
#include <HTAnchor.h>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef GLOBALREF_IS_MACRO
    extern GLOBALREF (HTProtocol, HTFinger);

#else
    GLOBALREF HTProtocol HTFinger;
#endif				/* GLOBALREF_IS_MACRO */

    extern int HTLoadFinger(const char *arg,
			    HTParentAnchor *anAnchor,
			    HTFormat format_out,
			    HTStream *stream);

#ifdef __cplusplus
}
#endif
#endif				/* HTFINGER_H */
