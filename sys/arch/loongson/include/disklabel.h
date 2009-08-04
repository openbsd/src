/* $OpenBSD: disklabel.h,v 1.1.1.1 2009/08/04 20:26:39 miod Exp $ */
/* public domain */

/*
 * Standard MBR partition scheme, with the label in the second sector
 * of the OpenBSD partition.
 */

#define	LABELSECTOR	1
#define	LABELOFFSET	0
#define	MAXPARTITIONS	16
