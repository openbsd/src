/*
 * Copyright (C) 1984-2002  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Macros to define the method of doing filename "globbing".
 * There are three possible mechanisms:
 *   1.	GLOB_LIST
 *	This defines a function that returns a list of matching filenames.
 *   2. GLOB_NAME
 *	This defines a function that steps thru the list of matching
 *	filenames, returning one name each time it is called.
 *   3. GLOB_STRING
 *	This defines a function that returns the complete list of
 *	matching filenames as a single space-separated string.
 */

#include <glob.h>

#define	DECL_GLOB_LIST(list)		glob_t list;  int i;
#define	GLOB_LIST(filename,list)	glob(filename,GLOB_NOCHECK,0,&list)
#define	GLOB_LIST_FAILED(list)		0
#define	SCAN_GLOB_LIST(list,p)		i = 0;  i < list.gl_pathc;  i++
#define	INIT_GLOB_LIST(list,p)		p = list.gl_pathv[i]
#define	GLOB_LIST_DONE(list)		globfree(&list)
