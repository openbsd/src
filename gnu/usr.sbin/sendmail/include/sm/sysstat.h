/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Sendmail: sysstat.h,v 1.5 2001/04/03 01:53:01 gshapiro Exp $
 */

/*
**  This is a wrapper for <sys/stat.h> that fixes portability problems.
*/

#ifndef SM_SYSSTAT_H
# define SM_SYSSTAT_H

# include <sys/stat.h>

/*
**  Some platforms lack lstat()
*/

# ifndef S_ISLNK
#  define lstat(fn, st) stat(fn, st)
# endif /* ! S_ISLNK */

#endif /* ! SM_SYSSTAT_H */
