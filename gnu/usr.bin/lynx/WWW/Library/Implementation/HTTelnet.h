/*                 /Net/dxcern/userd/timbl/hypertext/WWW/Library/Implementation/HTTelnet.html
                            TELNET AND SIMILAR ACCESS METHODS
                                             
 */

#ifndef HTTELNET_H
#define HTTELNET_H

#include <HTAccess.h>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef GLOBALREF_IS_MACRO
    extern GLOBALREF (HTProtocol, HTTelnet);
    extern GLOBALREF (HTProtocol, HTRlogin);
    extern GLOBALREF (HTProtocol, HTTn3270);

#else
    GLOBALREF HTProtocol HTTelnet;
    GLOBALREF HTProtocol HTRlogin;
    GLOBALREF HTProtocol HTTn3270;
#endif				/* GLOBALREF_IS_MACRO */

#ifdef __cplusplus
}
#endif
#endif				/* HTTELNET_H */
