/*                          SERVER SIDE ACCESS AUTHORIZATION MODULE
                                             
   This module is the server side interface to Access Authorization (AA) package. It
   contains code only for server.
   
   Important to know about memory allocation:
   
   Routines in this module use dynamic allocation, but free automatically all the memory
   reserved by them.
   
   Therefore the caller never has to (and never should) free() any object returned by
   these functions.
   
   Therefore also all the strings returned by this package are only valid until the next
   call to the same function is made. This approach is selected, because of the nature of
   access authorization: no string returned by the package needs to be valid longer than
   until the next call.
   
   This also makes it easy to plug the AA package in: you don't have to ponder whether to
   free()something here or is it done somewhere else (because it is always done somewhere
   else).
   
   The strings that the package needs to store are copied so the original strings given as
   parameters to AA functions may be freed or modified with no side effects.
   
   Also note:The AA package does not free() anything else than what it has itself
   allocated.
   
 */

#ifndef HTAASERV_H
#define HTAASERV_H

#ifndef HTUTILS_H
#include "HTUtils.h"            /* BOOL, PARAMS, ARGS   */
#endif /* HTUTILS_H */
/*#include <stdio.h> included by HTUtils.h -- FM *//* FILE                */
#include "HTRules.h"            /* This module interacts with rule system */
#include "HTAAUtil.h"           /* Common parts of AA   */
#include "HTAuth.h"             /* Authentication       */


#ifdef SHORT_NAMES
#define HTAAstMs        HTAA_statusMessage
#define HTAAchAu        HTAA_checkAuthorization
#define HTAAcoAH        HTAA_composeAuthHeaders
#define HTAAsLog        HTAA_startLogging
#endif /*SHORT_NAMES*/

extern time_t theTime;

/*

Check Access Authorization

   HTAA_checkAuthorization() is the main access authorization function.
   
 */

/* PUBLIC                                             HTAA_checkAuthorization()
**              CHECK IF USER IS AUTHORIZED TO ACCESS A FILE
** ON ENTRY:
**      url             is the document to be accessed.
**      method_name     name of the method, e.g. "GET"
**      scheme_name     authentication scheme name.
**      scheme_specifics authentication string (or other
**                      scheme specific parameters, like
**                      Kerberos-ticket).
**
** ON EXIT:
**      returns status codes uniform with those of HTTP:
**        200 OK           if file access is ok.
**        401 Unauthorized if user is not authorized to
**                         access the file.
**        403 Forbidden    if there is no entry for the
**                         requested file in the ACL.
**
** NOTE:
**      This function does not check whether the file
**      exists or not -- so the status  404 Not found
**      must be returned from somewhere else (this is
**      to avoid unnecessary overhead of opening the
**      file twice).
**
*/
PUBLIC int HTAA_checkAuthorization PARAMS((CONST char * url,
                                           CONST char * method_name,
                                           CONST char * scheme_name,
                                           char *       scheme_specifics));
/*

Compose Status Line Message

 */

/* SERVER PUBLIC                                        HTAA_statusMessage()
**              RETURN A STRING EXPLAINING ACCESS
**              AUTHORIZATION FAILURE
**              (Can be used in server reply status line
**               with 401/403 replies.)
** ON EXIT:
**      returns a string containing the error message
**              corresponding to internal HTAAFailReason.
*/
PUBLIC char *HTAA_statusMessage NOPARAMS;
/*

Compose "Authenticate:" Header Lines for Server Reply

 */

/* SERVER PUBLIC                                    HTAA_composeAuthHeaders()
**              COMPOSE WWW-Authenticate: HEADER LINES
**              INDICATING VALID AUTHENTICATION SCHEMES
**              FOR THE REQUESTED DOCUMENT
** ON ENTRY:
**      No parameters, but HTAA_checkAuthorization() must
**      just before have failed because a wrong (or none)
**      authentication scheme was used.
**
** ON EXIT:
**      returns a buffer containing all the WWW-Authenticate:
**              fields including CRLFs (this buffer is auto-freed).
**              NULL, if authentication won't help in accessing
**              the requested document.
*/
PUBLIC char *HTAA_composeAuthHeaders NOPARAMS;
/*

Start Access Authorization Logging

 */

/* PUBLIC                                               HTAA_startLogging()
**              START UP ACCESS AUTHORIZATION LOGGING
** ON ENTRY:
**      fp      is the open log file.
**
*/
PUBLIC void HTAA_startLogging PARAMS((FILE * fp));
/*

 */

#endif  /* NOT HTAASERV_H */
/*

   End of file HTAAServ.h.  */
