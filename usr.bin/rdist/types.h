/*	$OpenBSD: types.h,v 1.7 2015/01/20 06:08:08 guenther Exp $	*/

#ifndef __myTYPES_H__
#define __myTYPES_H__

/*
 * $From: types.h,v 1.5 1999/08/04 15:57:31 christos Exp $
 */

/*
 * Dist Options.
 *
 * WARNING: This values are used by the server (rdistd)
 */
#define DO_VERIFY		0x0000001
#define DO_WHOLE		0x0000002
#define DO_YOUNGER		0x0000004
#define DO_COMPARE		0x0000008
#define DO_REMOVE		0x0000010
#define DO_FOLLOW		0x0000020
#define DO_IGNLNKS		0x0000040
#define DO_QUIET		0x0000100
#define DO_CHKNFS		0x0000200
#define DO_CHKREADONLY		0x0000400
#define DO_NOEXEC		0x0000800
#define DO_SAVETARGETS		0x0001000
#define DO_NODESCEND		0x0002000
#define DO_NOCHKOWNER		0x0004000
#define DO_NOCHKMODE		0x0008000
#define DO_NOCHKGROUP		0x0010000
#define DO_CHKSYM		0x0020000
#define DO_NUMCHKGROUP		0x0040000
#define DO_NUMCHKOWNER		0x0080000
#define DO_HISTORY		0x0100000
#define DO_UPDATEPERM		0x0200000
#define DO_DEFGROUP		0x0400000
#define DO_DEFOWNER		0x0800000
#define DO_SPARSE		0x1000000	/* XXX not implemented */

typedef long		opt_t;

	/* Debug Message types */
#define DM_CALL		0x01
#define DM_PROTO	0x02
#define DM_CHILD	0x04
#define DM_MISC		0x10
#define DM_ALL		0x17

/*
 * Message Type definitions
 */
#define MT_DEBUG	0x0001			/* Debugging messages */
#define MT_NERROR	0x0002			/* Normal errors */
#define MT_FERROR	0x0004			/* Fatal errors */
#define MT_WARNING	0x0010			/* Warning messages */
#define MT_CHANGE	0x0020			/* Something changed */
#define MT_INFO		0x0040			/* General information */
#define MT_NOTICE	0x0100			/* Notice's */
#define MT_SYSLOG	0x0200			/* System log, but not user */
#define MT_REMOTE	0x0400			/* Ensure msg to remote */
#define MT_NOREMOTE	0x1000			/* Don't log to remote host */
#define MT_VERBOSE	0x2000			/* Verbose messages */
#define MT_ALL		(MT_NERROR|MT_FERROR|\
			 MT_WARNING|MT_CHANGE|\
			 MT_INFO|MT_NOTICE|\
			 MT_SYSLOG|MT_VERBOSE)

#endif	/* __myTYPES_H__ */
