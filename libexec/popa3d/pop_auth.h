/* $OpenBSD: pop_auth.h,v 1.2 2001/08/13 20:19:33 camield Exp $ */

/*
 * AUTHORIZATION state handling.
 */

#ifndef _POP_AUTH_H
#define _POP_AUTH_H

/*
 * Possible authentication results.
 */
#define AUTH_OK				0
#define AUTH_NONE			1
#define AUTH_FAILED			2

/*
 * Handles the AUTHORIZATION state commands, and writes authentication
 * data into the channel.
 */
extern int do_pop_auth(int channel);

/*
 * Logs an authentication attempt for mailbox (or NULL if the requested
 * mailbox doesn't exist).
 */
extern void log_pop_auth(int result, char *mailbox);

#endif
