/*	$OpenBSD: match.h,v 1.2 1997/09/22 05:25:49 millert Exp $	*/
/*
 * 27th March 1996. Added by Jan-Piet Mens for matching regular expressions
 * 		    in paths.
 * 
 */

/*
 * 	$From: match.h,v 1.1 1997/02/23 15:56:12 eric Rel $
 */

#include <fnmatch.h>

void add_match();
int matches();
