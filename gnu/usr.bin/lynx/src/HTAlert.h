/*  */

/*      Displaying messages and getting input for WWW Library
**      =====================================================
**
**         May 92 Created By C.T. Barker
**         Feb 93 Portablized etc TBL
*/

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */
#include "tcp.h"

/*      Display a message and get the input
**
**      On entry,
**              Msg is the message.
**
**      On exit,
**              Return value is malloc'd string which must be freed.
*/
extern char * HTPrompt PARAMS((CONST char * Msg, CONST char * deflt));


/*      Display a message, don't wait for input
**
**      On entry,
**              The input is a list of parameters for printf.
*/
extern void HTAlert PARAMS((CONST char * Msg));


/*      Display a progress message for information (and diagnostics) only
**
**      On entry,
**              The input is a list of parameters for printf.
*/
extern void HTProgress PARAMS((CONST char * Msg));
extern BOOLEAN mustshow;
#define _HTProgress(msg)	mustshow = TRUE, HTProgress(msg)

/*
 *  Indicates whether last HTConfirm was cancelled (^G or ^C) and
 *  resets flag. (so only call once!) - kw
 */
extern BOOL HTLastConfirmCancelled NOPARAMS;

/*      Display a message, then wait for 'yes' or 'no'.
**
**      On entry,
**              Takes a list of parameters for printf.
**
**      On exit,
**              If the user enters 'YES', returns TRUE, returns FALSE
**              otherwise.
*/
extern BOOL HTConfirm PARAMS ((CONST char * Msg));


/*      Prompt for password without echoing the reply
*/
extern char * HTPromptPassword PARAMS((CONST char * Msg));

/*      Prompt both username and password       HTPromptUsernameAndPassword()
**      ---------------------------------
** On entry,
**      Msg             is the prompting message.
**      *username and
**      *password       are char pointers; they are changed
**                      to point to result strings.
**	IsProxy		should be TRUE if this is for
**			proxy authentication.
**
**                      If *username is not NULL, it is taken
**                      to point to  a default value.
**                      Initial value of *password is
**                      completely discarded.
**
** On exit,
**      *username and *password point to newly allocated
**      strings -- original strings pointed to by them
**      are NOT freed.
**
*/
extern void HTPromptUsernameAndPassword PARAMS((
	CONST char *	Msg,
	char **		username,
	char **		password,
	BOOL		IsProxy));


/*	Confirm a cookie operation.			HTConfirmCookie()
**	---------------------------
**
**  On entry,
**	server			is the server sending the Set-Cookie.
**	domain			is the domain of the cookie.
**	path			is the path of the cookie.
**	name			is the name of the cookie.
**	value			is the value of the cookie.
**
**  On exit,
**	Returns FALSE on cancel,
**		TRUE if the cookie should be set.
*/
extern BOOL HTConfirmCookie PARAMS((
	void *		dp,
	CONST char *	server,
	CONST char *	domain,
	CONST char *	path,
	CONST char *	name,
	CONST char *	value));


/*      Confirm redirection of POST.		HTConfirmPostRedirect()
**	----------------------------
**  On entry,
**      Redirecting_url             is the Location.
**	server_status		    is the server status code.
**
**  On exit,
**      Returns 0 on cancel,
**	  1 for redirect of POST with content,
**	303 for redirect as GET without content
*/
extern int HTConfirmPostRedirect PARAMS((
	CONST char *	Redirecting_url,
	int		server_status));

/*

    */
