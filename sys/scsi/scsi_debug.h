/*	$OpenBSD: scsi_debug.h,v 1.7 1997/09/05 05:56:49 millert Exp $	*/
/*	$NetBSD: scsi_debug.h,v 1.7 1996/10/12 23:23:16 christos Exp $	*/

/*
 * Written by Julian Elischer (julian@tfs.com)
 */
#ifndef	_SCSI_SCSI_DEBUG_H
#define _SCSI_SCSI_DEBUG_H 1

/*
 * These are the new debug bits.  (Sat Oct  2 12:46:46 WST 1993)
 * the following DEBUG bits are defined to exist in the flags word of
 * the scsi_link structure.
 */
#define	SDEV_DB1		0x0010	/* scsi commands, errors, data	*/ 
#define	SDEV_DB2		0x0020	/* routine flow tracking */
#define	SDEV_DB3		0x0040	/* internal to routine flows	*/
#define	SDEV_DB4		0x0080	/* level 4 debugging for this dev */

/* targets and LUNs we want to debug */
#ifndef SCSIDEBUG_TARGETS
#define	SCSIDEBUG_TARGETS	0
#endif
#ifndef SCSIDEBUG_LUNS
#define	SCSIDEBUG_LUNS		0
#endif
#ifndef SCSIDEBUG_LEVEL
#define	SCSIDEBUG_LEVEL		(SDEV_DB1|SDEV_DB2)
#endif

extern int scsidebug_targets, scsidebug_luns, scsidebug_level;

/*
 * This is the usual debug macro for use with the above bits
 */
#ifdef	SCSIDEBUG
#define	SC_DEBUG(sc_link,Level,Printstuff) \
	if ((sc_link)->flags & (Level)) {	\
		sc_print_addr(sc_link);		\
 		printf Printstuff;		\
	}
#define	SC_DEBUGN(sc_link,Level,Printstuff) \
	if ((sc_link)->flags & (Level)) {	\
 		printf Printstuff;		\
	}
#else
#define SC_DEBUG(A,B,C)
#define SC_DEBUGN(A,B,C)
#endif

#endif /* _SCSI_SCSI_DEBUG_H */
