/* $OpenBSD: protocol.h,v 1.3 2003/05/12 19:28:22 camield Exp $ */

/*
 * POP protocol handling.
 */

#ifndef _POP_PROTOCOL_H
#define _POP_PROTOCOL_H

/*
 * Responses and events, to be returned by command and state handlers.
 */
#define POP_OK				0	/* Reply with "+OK" */
#define POP_ERROR			1	/* Reply with "-ERR" */
#define POP_QUIET			2	/* We've already replied */
#define POP_LEAVE			3	/* Leave the session */
#define POP_STATE			4	/* Advance the state */
#define POP_CRASH_NETFAIL		5	/* Network failure */
#define POP_CRASH_NETTIME		6	/* Network timeout */
#define POP_CRASH_SERVER		7	/* POP server failure */

/*
 * POP command description.
 */
struct pop_command {
	char *name;
	int (*handler)(char *params);
};

/*
 * Internal POP command buffer.
 */
struct pop_buffer {
	unsigned int ptr, size;
	char data[POP_BUFFER_SIZE];
};

extern struct pop_buffer pop_buffer;

/*
 * Initializes the buffer.
 */
extern void pop_init(void);

/*
 * Zeroes out the part of the buffer that has already been processed.
 */
extern void pop_clean(void);

/*
 * Checks if the buffer is sane.
 */
extern int pop_sane(void);

/*
 * Handles a POP protocol state (AUTHORIZATION or TRANSACTION, as defined
 * in RFC 1939), processing the supplied commands. Returns when the state
 * is changed.
 */
extern int pop_handle_state(struct pop_command *commands);

/*
 * Returns the next parameter, or NULL if there's none or it is too long
 * to be valid (as defined in the RFC).
 */
extern char *pop_get_param(char **params);

/*
 * Returns the next parameter as a non-negative number, or -1 if there's
 * none or the syntax is invalid.
 */
extern int pop_get_int(char **params);

/*
 * Produces a generic POP response. Returns a non-zero value on error;
 * the POP session then has to crash.
 */
extern int pop_reply(char *format, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 1, 2)));
#else
	;
#endif

/*
 * The two simple POP responses. Return a non-zero value on error; the
 * POP session then has to crash.
 */
extern int pop_reply_ok(void);
extern int pop_reply_error(void);

/*
 * Produces a multi-line POP response, reading the data from the supplied
 * file descriptor for up to the requested size or number of lines of the
 * message body, if that number is non-negative. Returns POP_OK or one of
 * the POP_CRASH_* event codes.
 */
extern int pop_reply_multiline(int fd, unsigned long size, int lines);

/*
 * Terminates a multi-line POP response. Returns a non-zero value on error;
 * the POP session then has to crash.
 */
extern int pop_reply_terminate(void);

#endif
