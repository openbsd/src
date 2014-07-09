/*
 * $LynxId: HTAABrow.h,v 1.16 2010/10/27 00:13:53 tom Exp $
 *
 *                          BROWSER SIDE ACCESS AUTHORIZATION MODULE

   This module is the browser side interface to Access Authorization (AA) package.  It
   contains code only for browser.

   Important to know about memory allocation:

   Routines in this module use dynamic allocation, but free automatically all the memory
   reserved by them.

   Therefore the caller never has to (and never should) free() any object returned by
   these functions.

   Therefore also all the strings returned by this package are only valid until the next
   call to the same function is made.  This approach is selected, because of the nature of
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

#ifndef HTAABROW_H
#define HTAABROW_H

#include <HTAAUtil.h>		/* Common parts of AA */

#ifdef __cplusplus
extern "C" {
#endif
/*
   Routines for Browser Side Recording of AA Info

   Most of the browser-side AA is done by the following two functions (which are called
   from file HTTP.c so the browsers using libwww only need to be linked with the new
   library and not be changed at all):

      HTAA_composeAuth() composes the Authorization: line contents, if the AA package
      thinks that the given document is protected. Otherwise this function returns NULL.
      This function also calls the functions HTPrompt(),HTPromptPassword() and HTConfirm()
      to get the username, password and some confirmation from the user.

      HTAA_shouldRetryWithAuth() determines whether to retry the request with AA or with a
      new AA (in case username or password was misspelled).

 */
/* PUBLIC                                               HTAA_composeAuth()
 *
 *      COMPOSE THE ENTIRE AUTHORIZATION HEADER LINE IF WE
 *      ALREADY KNOW, THAT THE HOST MIGHT REQUIRE AUTHORIZATION
 *
 * ON ENTRY:
 *      hostname        is the hostname of the server.
 *      portnumber      is the portnumber in which the server runs.
 *      docname         is the pathname of the document (as in URL)
 *
 * ON EXIT:
 *      returns NULL, if no authorization seems to be needed, or
 *              if it is the entire Authorization: line, e.g.
 *
 *                 "Authorization: basic username:password"
 *
 *              As usual, this string is automatically freed.
 */
    extern char *HTAA_composeAuth(const char *hostname,
				  const int portnumber,
				  const char *docname,
				  int IsProxy);

/* BROWSER PUBLIC                               HTAA_shouldRetryWithAuth()
 *
 *              DETERMINES IF WE SHOULD RETRY THE SERVER
 *              WITH AUTHORIZATION
 *              (OR IF ALREADY RETRIED, WITH A DIFFERENT
 *              USERNAME AND/OR PASSWORD (IF MISSPELLED))
 * ON ENTRY:
 *      start_of_headers is the first block already read from socket,
 *                      but status line skipped; i.e., points to the
 *                      start of the header section.
 *      length          is the remaining length of the first block.
 *      soc             is the socket to read the rest of server reply.
 *
 *                      This function should only be called when
 *                      server has replied with a 401 (Unauthorized)
 *                      status code.
 * ON EXIT:
 *      returns         YES, if connection should be retried.
 *                           The node containing all the necessary
 *                           information is
 *                              * either constructed if it does not exist
 *                              * or password is reset to NULL to indicate
 *                                that username and password should be
 *                                reprompted when composing Authorization:
 *                                field (in function HTAA_composeAuth()).
 *                      NO, otherwise.
 */
    extern BOOL HTAA_shouldRetryWithAuth(char *start_of_headers,
					 size_t length,
					 int soc,
					 int IsProxy);

/*
 *  Function to allow clearing of all Authorization info
 *  via a browser command. - FM
 */
    extern void HTClearHTTPAuthInfo(void);

/*

Enabling Gateway httpds to Forward Authorization

   These functions should only be called from daemon code, and HTAAForwardAuth_reset()
   must be called before the next request is handled to make sure that authorization
   string isn't cached in daemon so that other people can access private files using
   somebody else's previous authorization information.

 */

    extern void HTAAForwardAuth_set(const char *scheme_name,
				    const char *scheme_specifics);
    extern void HTAAForwardAuth_reset(void);

#ifdef __cplusplus
}
#endif
#endif				/* NOT HTAABROW_H */
