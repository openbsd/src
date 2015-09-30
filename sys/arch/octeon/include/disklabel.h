/* $OpenBSD: disklabel.h,v 1.2 2015/09/30 14:57:03 krw Exp $ */
/* public domain */

/*
 * Standard MBR partition scheme, with the label in the second sector
 * of the OpenBSD partition.
 */

#define	LABELSECTOR	1	/* sector containing label */
#define	LABELOFFSET	0	/* offset of label in sector */
#define	MAXPARTITIONS	16	/* number of partitions */
