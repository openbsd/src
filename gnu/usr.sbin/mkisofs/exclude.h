/*	$OpenBSD: exclude.h,v 1.1.1.1 1997/09/15 06:01:53 downsj Exp $	*/
/*
 * 9-Dec-93 R.-D. Marzusch, marzusch@odiehh.hanse.de:
 * added 'exclude' option (-x) to specify pathnames NOT to be included in 
 * CD image.
 *
 * 	$From: exclude.h,v 1.1 1997/02/23 15:53:19 eric Rel $
 */

void exclude();
int is_excluded();
