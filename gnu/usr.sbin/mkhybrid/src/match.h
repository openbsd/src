/*
 * 27th March 1996. Added by Jan-Piet Mens for matching regular expressions
 * 		    in paths.
 * 
 */

/*
 * 	$Id: match.h,v 1.2 2002/11/03 20:27:03 pvalchev Exp $
 */

#include <inttypes.h>
#include "fnmatch.h"

int matches	__PR((char *fn));

int i_matches	__PR((char *fn));
intptr_t i_ishidden	__PR((void));

int j_matches	__PR((char *fn));
intptr_t j_ishidden	__PR((void));

#ifdef APPLE_HYB
int add_match	__PR((char *fn));
int i_add_match __PR((char *fn));
int j_add_match __PR((char *fn));

int hfs_add_match __PR((char *fn));
int hfs_matches __PR((char *fn));
intptr_t hfs_ishidden __PR((void));

void add_list __PR((char *fn));
void i_add_list __PR((char *fn));
void j_add_list __PR((char *fn));
#else
void add_match	__PR((char *fn));
void i_add_match __PR((char *fn));
void j_add_match __PR((char *fn));
#endif /* APPLE_HYB */
