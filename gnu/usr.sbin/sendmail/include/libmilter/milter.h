/*
 * Copyright (c) 1999 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	$Sendmail: milter.h,v 8.24 1999/11/28 05:54:20 gshapiro Exp $
 */

/*
**  MILTER.H -- Global definitions for mail filter and MTA.
*/

#ifndef _LIBMILTER_MILTER_H
# define _LIBMILTER_MILTER_H	1

/* Shared protocol constants */
# define MILTER_LEN_BYTES	4	/* length of 32 bit integer in bytes */
# define MILTER_CHUNK_SIZE	65535	/* body chunk size */
# define SMFI_VERSION		1	/* version number */

/* address families */
# define SMFIA_UNKNOWN		'U'	/* unknown */
# define SMFIA_UNIX		'L'	/* unix/local */
# define SMFIA_INET		'4'	/* inet */
# define SMFIA_INET6		'6'	/* inet6 */

/* commands: don't use anything smaller than ' ' */
# define SMFIC_ABORT		'A'	/* Abort */
# define SMFIC_BODY		'B'	/* Body chunk */
# define SMFIC_CONNECT		'C'	/* Connection information */
# define SMFIC_MACRO		'D'	/* Define macro */
# define SMFIC_BODYEOB		'E'	/* final body chunk (End) */
# define SMFIC_HELO		'H'	/* HELO/EHLO */
# define SMFIC_HEADER		'L'	/* Header */
# define SMFIC_MAIL		'M'	/* MAIL from */
# define SMFIC_EOH		'N'	/* EOH */
# define SMFIC_OPTNEG		'O'	/* Option negotiation */
# define SMFIC_QUIT		'Q'	/* QUIT */
# define SMFIC_RCPT		'R'	/* RCPT to */

/* actions (replies) */
# define SMFIR_ADDRCPT		'+'	/* add recipient */
# define SMFIR_DELRCPT		'-'	/* remove recipient */
# define SMFIR_ACCEPT		'a'	/* accept */
# define SMFIR_REPLBODY		'b'	/* replace body (chunk) */
# define SMFIR_CONTINUE		'c'	/* continue */
# define SMFIR_DISCARD		'd'	/* discard */
# define SMFIR_PROGRESS		'p'	/* progress */
# define SMFIR_REJECT		'r'	/* reject */
# define SMFIR_TEMPFAIL		't'	/* tempfail */
# define SMFIR_ADDHEADER	'h'	/* add header */
# define SMFIR_REPLYCODE	'y'	/* reply code etc */

/* values for filter negotiation flags */
# define SMFIF_MODHDRS	0x00000001L	/* filter may add headers */
# define SMFIF_MODBODY	0x00000002L	/* filter may replace body */
# define SMFIF_ADDRCPT	0x00000004L	/* filter may add recipients */
# define SMFIF_DELRCPT	0x00000008L	/* filter may delete recipients */
# define SMFIF_NOCONNECT 0x00000010L	/* MTA should not send connect info */
# define SMFIF_NOHELO	0x00000020L	/* MTA should not send HELO info */
# define SMFIF_NOMAIL	0x00000040L	/* MTA should not send MAIL info */
# define SMFIF_NORCPT	0x00000080L	/* MTA should not send RCPT info */
# define SMFIF_NOBODY	0x00000100L	/* MTA should not send body */
# define SMFIF_NOHDRS	0x00000200L	/* MTA should not send headers */

#endif /* !_LIBMILTER_MILTER_H */
