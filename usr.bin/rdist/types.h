/*	$OpenBSD: types.h,v 1.4 1998/06/26 21:21:22 millert Exp $	*/

#ifndef __myTYPES_H__
#define __myTYPES_H__

/*
 * $From: types.h,v 1.2 1996/01/30 01:52:24 mcooper Exp $
 */

/*
 * Dist Options.
 *
 * WARNING: This values are used by the server (rdistd)
 */
#define DO_VERIFY		0x000001
#define DO_WHOLE		0x000002
#define DO_YOUNGER		0x000004
#define DO_COMPARE		0x000008
#define DO_REMOVE		0x000010
#define DO_FOLLOW		0x000020
#define DO_IGNLNKS		0x000040
#define DO_QUIET		0x000100
#define DO_CHKNFS		0x000200
#define DO_CHKREADONLY		0x000400
#define DO_NOEXEC		0x000800
#define DO_SAVETARGETS		0x001000
#define DO_NODESCEND		0x002000
#define DO_NOCHKOWNER		0x004000
#define DO_NOCHKMODE		0x008000
#define DO_NOCHKGROUP		0x010000
#define DO_CHKSYM		0x020000
#define DO_NUMCHKGROUP		0x040000
#define DO_NUMCHKOWNER		0x080000
#define DO_SPARSE		0x100000

/*
 * Dist option information
 */
typedef long		opt_t;
struct _distoptinfo {
	opt_t		do_value;
	char	       *do_name;
};
typedef struct _distoptinfo DISTOPTINFO;

	/* Debug Message types */
#define DM_CALL		0x01
#define DM_PROTO	0x02
#define DM_CHILD	0x04
#define DM_MISC		0x10
#define DM_ALL		0x17

/*
 * Description of a message type
 */
struct _msgtype {
	int		mt_type;		/* Type (bit) */
	char	       *mt_name;		/* Name of message type */
};
typedef struct _msgtype MSGTYPE;

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

/*
 * Description of message facilities
 */
struct _msgfacility {
	/* compile time initialized data */
	int		mf_msgfac;		/* One of MF_* from below */
	char	       *mf_name;		/* Name of this facility */
	void	      (*mf_sendfunc)();		/* Function to send msg */
	/* run time initialized data */
	int		mf_msgtypes;		/* Bitmask of MT_* from above*/
	char	       *mf_filename;		/* Name of file */
	FILE	       *mf_fptr;		/* File pointer to output to */
};
typedef struct _msgfacility MSGFACILITY;

/*
 * Message Facilities
 */
#define MF_STDOUT	1			/* Standard Output */
#define MF_NOTIFY	2			/* Notify mail service */
#define MF_FILE		3			/* A normal file */
#define MF_SYSLOG	4			/* syslog() */

#endif	/* __myTYPES_H__ */
