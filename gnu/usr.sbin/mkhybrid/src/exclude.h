/*
 * 9-Dec-93 R.-D. Marzusch, marzusch@odiehh.hanse.de:
 * added 'exclude' option (-x) to specify pathnames NOT to be included in 
 * CD image.
 *
 * 	$Id: exclude.h,v 1.1 2000/10/10 20:40:14 beck Exp $
 */

void exclude	__PR((char * fn));
int is_excluded	__PR((char * fn));
