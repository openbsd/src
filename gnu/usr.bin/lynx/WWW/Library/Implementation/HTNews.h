/*                                  Network News Transfer protocol module for the WWW library
                                          HTNEWS
                                             
 */
/* History:
**      26 Sep 90       Written TBL in Objective-C
**      29 Nov 91       Downgraded to C, for portable implementation.
*/

#ifndef HTNEWS_H
#define HTNEWS_H

#include <HTAccess.h>
#include <HTAnchor.h>

extern int HTNewsChunkSize;
extern int HTNewsMaxChunk;

#ifdef GLOBALREF_IS_MACRO
extern GLOBALREF(HTProtocol, HTNews);
extern GLOBALREF(HTProtocol, HTNNTP);
extern GLOBALREF(HTProtocol, HTNewsPost);
extern GLOBALREF(HTProtocol, HTNewsReply);
extern GLOBALREF(HTProtocol, HTSNews);
extern GLOBALREF(HTProtocol, HTSNewsPost);
extern GLOBALREF(HTProtocol, HTSNewsReply);
#else
GLOBALREF HTProtocol HTNews;
GLOBALREF HTProtocol HTNNTP;
GLOBALREF HTProtocol HTNewsPost;
GLOBALREF HTProtocol HTNewsReply;
GLOBALREF HTProtocol HTSNews;
GLOBALREF HTProtocol HTSNewsPost;
GLOBALREF HTProtocol HTSNewsReply;
#endif /* GLOBALREF_IS_MACRO */

extern void HTSetNewsHost PARAMS((
	CONST char *	value));
extern CONST char * HTGetNewsHost NOPARAMS;
extern char * HTNewsHost;

extern void HTClearNNTPAuthInfo NOPARAMS;

#ifdef USE_SSL
extern int HTNewsProxyConnect PARAMS ((
	int		sock,
	CONST char *	url,
	HTParentAnchor *anAnchor,
	HTFormat	format_out,
	HTStream *	sink));
#endif

#endif /* HTNEWS_H */
