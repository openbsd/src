/*                     /Net/dxcern/userd/timbl/hypertext/WWW/Library/Implementation/HTTP.html
                                HYPERTEXT TRANFER PROTOCOL

 */
#ifndef HTTP_H
#define HTTP_H

#include "HTAccess.h"

#ifdef GLOBALREF_IS_MACRO
extern GLOBALREF (HTProtocol,HTTP);
extern GLOBALREF (HTProtocol,HTTPS);
#else
GLOBALREF HTProtocol HTTP;
GLOBALREF HTProtocol HTTPS;
#endif /* GLOBALREF_IS_MACRO */

#define URL_GET_METHOD  1
#define URL_POST_METHOD 2
#define URL_MAIL_METHOD 3

extern BOOL reloading;
extern char * redirecting_url;
extern BOOL permanent_redirection;
extern BOOL redirect_post_content;

#endif /* HTTP_H */

/*

   end of HTTP module definition

 */
