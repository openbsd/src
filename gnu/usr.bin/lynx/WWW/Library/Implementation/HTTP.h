/*
 * $LynxId: HTTP.h,v 1.10 2009/04/07 22:44:50 tom Exp $
 *
 * /Net/dxcern/userd/timbl/hypertext/WWW/Library/Implementation/HTTP.html
 *                                HYPERTEXT TRANFER PROTOCOL
 */
#ifndef HTTP_H
#define HTTP_H

#include <HTAccess.h>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef GLOBALREF_IS_MACRO
    extern GLOBALREF (HTProtocol, HTTP);
    extern GLOBALREF (HTProtocol, HTTPS);

#else
    GLOBALREF HTProtocol HTTP;
    GLOBALREF HTProtocol HTTPS;
#endif				/* GLOBALREF_IS_MACRO */

#define URL_GET_METHOD  1
#define URL_POST_METHOD 2
#define URL_MAIL_METHOD 3

    extern int ws_read_per_sec;
    extern BOOLEAN reloading;
    extern char *redirecting_url;
    extern BOOL permanent_redirection;
    extern BOOL redirect_post_content;

#ifdef USE_SSL
    extern SSL *SSL_handle;
#endif

#ifdef __cplusplus
}
#endif
#endif				/* HTTP_H */
