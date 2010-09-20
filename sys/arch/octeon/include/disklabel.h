/* $OpenBSD: disklabel.h,v 1.1 2010/09/20 06:32:30 syuu Exp $ */
/* public domain */

/*
 * Standard MBR partition scheme, with the label in the second sector
 * of the OpenBSD partition.
 */

#define	LABELSECTOR	1
#define	LABELOFFSET	0
#define	MAXPARTITIONS	16
