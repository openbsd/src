/*      Displaying messages and getting input for WWW Library
 *      =====================================================
 *
 *         May 92 Created By C.T. Barker
 *         Feb 93 Portablized etc TBL
 */

#ifndef HTALERT_H
#define HTALERT_H 1

#include <LYCookie.h>

#ifdef __cplusplus
extern "C" {
#endif
#define ALERT_PREFIX_LEN 5
/*      Display a message and get the input
 *
 *      On entry,
 *              Msg is the message.
 *
 *      On exit,
 *              Return value is malloc'd string which must be freed.
 */ extern char *HTPrompt(const char *Msg, const char *deflt);

/*      Display a message, don't wait for input
 *
 *      On entry,
 *              The input is a list of parameters for printf.
 */
    extern void HTAlert(const char *Msg);
    extern void HTAlwaysAlert(const char *extra_prefix, const char *Msg);
    extern void HTInfoMsg(const char *Msg);
    extern void HTInfoMsg2(const char *Msg, const char *Arg);
    extern void HTUserMsg(const char *Msg);
    extern void HTUserMsg2(const char *Msg, const char *Arg);

/*      Display a progress message for information (and diagnostics) only
 *
 *      On entry,
 *              The input is a list of parameters for printf.
 */
    extern const char *HTProgressUnits(int kilobytes);
    extern void HTProgress(const char *Msg);
    extern void HTReadProgress(off_t bytes, off_t total);

#define _HTProgress(msg)	mustshow = TRUE, HTProgress(msg)

/*
 *  Indicates whether last HTConfirm was cancelled (^G or ^C) and
 *  resets flag. (so only call once!) - kw
 */
    extern BOOL HTLastConfirmCancelled(void);

/*
 *	Supports logic for forced yes/no prompt results.
 */
    extern int HTForcedPrompt(int Opt, const char *Msg, int Dft);

/*      Display a message, then wait for 'yes' or 'no', allowing default
 *	response if a return or left-arrow is used.
 *
 *      On entry,
 *              Takes a list of parameters for printf.
 *
 *      On exit,
 *              If the user enters 'YES', returns TRUE, returns FALSE
 *              otherwise.
 */
    extern int HTConfirmDefault(const char *Msg, int Dft);

/*      Display a message, then wait for 'yes' or 'no'.
 *
 *      On entry,
 *              Takes a list of parameters for printf.
 *
 *      On exit,
 *              If the user enters 'YES', returns TRUE, returns FALSE
 *              otherwise.
 */
    extern BOOL HTConfirm(const char *Msg);

    extern BOOL confirm_post_resub(const char *address,
				   const char *title,
				   int if_imgmap,
				   int if_file);

/*      Prompt for password without echoing the reply
 */
    extern char *HTPromptPassword(const char *Msg);

/*      Prompt both username and password       HTPromptUsernameAndPassword()
 *      ---------------------------------
 * On entry,
 *      Msg             is the prompting message.
 *      *username and
 *      *password       are char pointers; they are changed
 *                      to point to result strings.
 *	IsProxy		should be TRUE if this is for
 *			proxy authentication.
 *
 *                      If *username is not NULL, it is taken
 *                      to point to  a default value.
 *                      Initial value of *password is
 *                      completely discarded.
 *
 * On exit,
 *      *username and *password point to newly allocated
 *      strings -- original strings pointed to by them
 *      are NOT freed.
 *
 */
    extern void HTPromptUsernameAndPassword(const char *Msg,
					    char **username,
					    char **password,
					    BOOL IsProxy);

/*	Confirm a cookie operation.			HTConfirmCookie()
 *	---------------------------
 *
 *  On entry,
 *	server			is the server sending the Set-Cookie.
 *	domain			is the domain of the cookie.
 *	path			is the path of the cookie.
 *	name			is the name of the cookie.
 *	value			is the value of the cookie.
 *
 *  On exit,
 *	Returns FALSE on cancel,
 *		TRUE if the cookie should be set.
 */
    extern BOOL HTConfirmCookie(domain_entry * dp, const char *server,
				const char *name,
				const char *value);

/*      Confirm redirection of POST.		HTConfirmPostRedirect()
 *	----------------------------
 *  On entry,
 *      Redirecting_url             is the Location.
 *	server_status		    is the server status code.
 *
 *  On exit,
 *      Returns 0 on cancel,
 *	  1 for redirect of POST with content,
 *	303 for redirect as GET without content
 */
    extern int HTConfirmPostRedirect(const char *Redirecting_url,
				     int server_status);

    extern void LYSleepAlert(void);
    extern void LYSleepDebug(void);
    extern void LYSleepInfo(void);
    extern void LYSleepMsg(void);
    extern void LYSleepReplay(void);

#ifdef HAVE_STRERROR
#define LYStrerror strerror
#else
    extern char *LYStrerror(int code);
#endif				/* HAVE_STRERROR */

#ifdef __cplusplus
}
#endif
#endif				/* HTALERT_H */
