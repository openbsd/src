/*
 * Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	$Sendmail: mfapi.h,v 8.13 2000/02/26 19:13:36 gshapiro Exp $
 */

/*
**  MFAPI.H -- Global definitions for mail filter library and mail filters.
*/

#ifndef _LIBMILTER_MFAPI_H
# define _LIBMILTER_MFAPI_H	1

/*
**  Access common MTA/libmilter constants
*/

# include "libmilter/milter.h"

/*
**  Currently this is a C-fied version of ~eric/public_html/mfapi.html
**  It does not (yet) conform with the coding standard...
*/

/*
**  status codes
*/

/* XXX maybe use enum? */

/*
**  Continue processing message/connection.
*/

#define SMFIS_CONTINUE	0

/*
**  Reject the message/connection.
**  No further routines will be called for this message
**  (or connection, if returned from a connection-oriented routine).
*/

#define SMFIS_REJECT	1

/*
**  Accept the message,
**  but silently discard the message.
**  No further routines will be called for this message.
**  This is only meaningful from message-oriented routines.
*/

#define SMFIS_DISCARD	2

/*
**  Accept the message/connection.
**  No further routines will be called for this message
**  (or connection, if returned from a connection-oriented routine;
**  in this case, it causes all messages on this connection
**  to be accepted without filtering).
*/

#define SMFIS_ACCEPT	3

/*
**  Return a temporary failure, i.e.,
**  the corresponding SMTP command will return a 4xx status code.
**  In some cases this may prevent further routines from
**  being called on this message or connection,
**  although in other cases (e.g., when processing an envelope
**  recipient) processing of the message will continue.
*/
#define SMFIS_TEMPFAIL	4

/* type to store return value */
typedef int	sfsistat;

/* for now ... */
#if SOCKADDRHACK
# ifndef _SOCK_ADDR
#  define _SOCK_ADDR	struct sockaddr_in
# endif /* !_SOCK_ADDR */
#else /* SOCKADDRHACK */
# define NOT_SENDMAIL	1
# include "sendmail.h"
# define _SOCK_ADDR	SOCKADDR
#endif /* SOCKADDRHACK */

#include <pthread.h>

#ifndef MI_SUCCESS
# define MI_SUCCESS	0
#endif /* MI_SUCCESS */
#ifndef MI_FAILURE
# define MI_FAILURE	(-1)
#endif /* MI_FAILURE */

/* "forward" declarations */
typedef struct smfi_str SMFICTX;
typedef struct smfi_str *SMFICTX_PTR;

typedef struct smfiDesc smfiDesc_str;
typedef struct smfiDesc	* smfiDesc_ptr;

# define MAX_MACROS_ENTRIES	4	/* max size of macro pointer array */

/*
**  context for milter
**  implementation hint:
**  macros are stored in mac_buf[] as sequence of:
**  macro_name \0 macro_value
**  (just as read from the MTA)
**  mac_ptr is a list of pointers into mac_buf to the beginning of each
**  entry, i.e., macro_name, macro_value, ...
*/

struct smfi_str
{
	pthread_t	ctx_id;		/* thread id */
	int		ctx_fd;		/* filedescriptor */
	int		ctx_dbg;	/* debug level */
	time_t		ctx_timeout;	/* timeout */
	int		ctx_state;	/* state */
	smfiDesc_ptr	ctx_smfi;	/* filter description */
	char		**ctx_mac_ptr[MAX_MACROS_ENTRIES];
	char		*ctx_mac_buf[MAX_MACROS_ENTRIES];
	char		*ctx_reply;	/* reply code */
	void		*ctx_privdata;	/* private data */
};

/*
**  structure describing one milter
*/

struct smfiDesc
{
	char		*xxfi_name;	/* filter name */
	int		xxfi_version;	/* version code -- do not change */
	u_long		xxfi_flags;	/* flags */

	/* connection info filter */
	sfsistat	(*xxfi_connect) __P((SMFICTX *, char *, _SOCK_ADDR *));

	/* SMTP HELO command filter */
	sfsistat	(*xxfi_helo) __P((SMFICTX *, char *));

	/* envelope sender filter */
	sfsistat	(*xxfi_envfrom) __P((SMFICTX *, char **));

	/* envelope recipient filter */
	sfsistat	(*xxfi_envrcpt) __P((SMFICTX *, char **));

	/* header filter */
	sfsistat	(*xxfi_header) __P((SMFICTX *, char *, char *));

	/* end of header */
	sfsistat	(*xxfi_eoh) __P((SMFICTX *));

	/* body block */
	sfsistat	(*xxfi_body) __P((SMFICTX *, u_char *, size_t));

	/* end of message */
	sfsistat	(*xxfi_eom) __P((SMFICTX *));

	/* message aborted */
	sfsistat	(*xxfi_abort) __P((SMFICTX *));

	/* connection cleanup */
	sfsistat	(*xxfi_close) __P((SMFICTX *));
};

#if 0
simple example what a filter program should do:

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct smfiDesc	XxFilterDesc;

	/* fill in elements */
	smfi_register(XxFilterDesc);
	smfi_main();
	/* NOTREACHED */
}
#endif /* 0 */

extern int smfi_register __P((smfiDesc_str));
extern int smfi_main __P((void));
extern int smfi_setdbg __P((int));
extern int smfi_settimeout __P((int));
extern int smfi_setconn __P((char *));

/*
**  Filter Routine Details
*/

#if 0
/* connection info filter */
extern sfsistat	xxfi_connect __P((SMFICTX *, char *, _SOCK_ADDR *));

/*
**  xxfi_connect(ctx, hostname, hostaddr) Invoked on each connection
**
**	char *hostname; Host domain name, as determined by a reverse lookup
**		on the host address.
**	_SOCK_ADDR *hostaddr; Host address, as determined by a getpeername
**		call on the SMTP socket.
*/

/* SMTP HELO command filter */
extern sfsistat	xxfi_helo __P((SMFICTX *, char *));

/*
**  xxfi_helo(ctx, helohost) Invoked on SMTP HELO/EHLO command
**
**	char *helohost; Value passed to HELO/EHLO command, which should be
**		the domain name of the sending host (but is, in practice,
**		anything the sending host wants to send).
*/

/* envelope sender filter */
extern sfsistat	xxfi_envfrom __P((SMFICTX *, char **));

/*
**  xxfi_envfrom(ctx, argv) Invoked on envelope from
**
**	char **argv; Null-terminated SMTP command arguments;
**		argv[0] is guaranteed to be the sender address.
**		Later arguments are the ESMTP arguments.
*/

/* envelope recipient filter */
extern sfsistat	xxfi_envrcpt __P((SMFICTX *, char **));

/*
**  xxfi_envrcpt(ctx, argv) Invoked on each envelope recipient
**
**	char **argv; Null-terminated SMTP command arguments;
**		argv[0] is guaranteed to be the recipient address.
**		Later arguments are the ESMTP arguments.
*/

/* header filter */
extern sfsistat	xxfi_header __P((SMFICTX *, char *, char *));

/*
**  xxfi_header(ctx, headerf, headerv) Invoked on each message header. The
**  content of the header may have folded white space (that is, multiple
**  lines with following white space) included.
**
**	char *headerf; Header field name
**	char *headerv; Header field value
*/

/* end of header */
extern sfsistat	xxfi_eoh __P((SMFICTX *));

/*
**  xxfi_eoh(ctx) Invoked at end of header
*/
#endif /* 0 */

/* body block */
extern sfsistat	xxfi_body __P((SMFICTX *, u_char *, size_t));

/*
**  xxfi_body(ctx, bodyp, bodylen) Invoked for each body chunk. There may
**  be multiple body chunks passed to the filter. End-of-lines are
**  represented as received from SMTP (normally Carriage-Return/Line-Feed).
**
**	u_char *bodyp; Pointer to body data
**	size_t bodylen; Length of body data
*/

/* end of message */
extern sfsistat	xxfi_eom __P((SMFICTX *));

/*
**  xxfi_eom(ctx) Invoked at end of message. This routine can perform
**  special operations such as modifying the message header, body, or
**  envelope.
*/

/* message aborted */
extern sfsistat	xxfi_abort __P((SMFICTX *));

/*
**  xxfi_abort(ctx) Invoked if message is aborted outside of the control of
**  the filter, for example, if the SMTP sender issues an RSET command. If
**  xxfi_abort is called, xxfi_eom will not be called and vice versa.
*/

/* connection cleanup */
extern sfsistat	xxfi_close __P((SMFICTX *));

/*
**  xxfi_close(ctx) Invoked at end of the connection. This is called on
**  close even if the previous mail transaction was aborted.
*/


/*
**  Additional information is passed in to the vendor filter routines using
**  symbols. Symbols correspond closely to sendmail macros. The symbols
**  defined depend on the context. The value of a symbol is accessed using:
*/

/* Return the value of a symbol. */
extern char * smfi_getsymval __P((SMFICTX *, char *));

/*
**  Return the value of a symbol.
**
**	SMFICTX *ctx; Opaque context structure
**	char *symname; The name of the symbol to access.
*/

/*
**  Vendor filter routines that want to pass additional information back to
**  the MTA for use in SMTP replies may call smfi_setreply before returning.
*/

extern int smfi_setreply __P((SMFICTX *, char *, char *, char *));

/*
**  Set the specific reply code to be used in response to the active
**  command. If not specified, a generic reply code is used.
**
**	SMFICTX *ctx; Opaque context structure
**	char *rcode; The three-digit (RFC 821) SMTP reply code to be
**		returned, e.g., ``551''.
**	char *xcode; The extended (RFC 2034) reply code, e.g., ``5.7.6''.
**	char *message; The text part of the SMTP reply.
*/

/*
**  The xxfi_eom routine is called at the end of a message (essentially,
**  after the final DATA dot). This routine can call some special routines
**  to modify the envelope, header, or body of the message before the
**  message is enqueued. These routines must not be called from any vendor
**  routine other than xxfi_eom.
*/

extern int smfi_addheader __P((SMFICTX *, char *, char *));

/*
**  Add a header to the message. This header is not passed to other
**  filters. It is not checked for standards compliance; the mail filter
**  must ensure that no protocols are violated as a result of adding this
**  header.
**
**	SMFICTX *ctx; Opaque context structure
**	char *headerf; Header field name
**	char *headerv; Header field value
*/

extern int smfi_addrcpt __P((SMFICTX *, char *));

/*
**  Add a recipient to the envelope
**
**	SMFICTX *ctx; Opaque context structure
**	char *rcpt; Recipient to be added
*/

extern int smfi_delrcpt __P((SMFICTX *, char *));

/*
**  Delete a recipient from the envelope
**
**	SMFICTX *ctx; Opaque context structure
**	char *rcpt; Envelope recipient to be deleted. This should be in
**		exactly the form passed to xxfi_envrcpt or the address may
**		not be deleted.
*/

extern int smfi_replacebody __P((SMFICTX *, u_char *, int));

/*
**  Replace the body of the message. This routine may be called multiple
**  times if the body is longer than convenient to send in one call. End of
**  line should be represented as Carriage-Return/Line Feed.
**
**	char *bodyp; Pointer to block of body information to insert
**	int bodylen; Length of data pointed at by bodyp
*/

/*
**  If the message is aborted (for example, if the SMTP sender sends the
**  envelope but then does a QUIT or RSET before the data is sent),
**  xxfi_abort is called. This can be used to reset state.
*/


/*
**  Connection-private data (specific to an SMTP connection) can be
**  allocated using the smfi_setpriv routine; routines can access private
**  data using smfi_getpriv.
*/

extern int smfi_setpriv __P((SMFICTX *, void *));

/*
**  Set the private data pointer
**
**	SMFICTX *ctx; Opaque context structure
**	void *privatedata; Pointer to private data area
*/

extern void *smfi_getpriv __P((SMFICTX *));

#endif /* !_LIBMILTER_MFAPI_H */
