/*
 * Copyright (c) 2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Sendmail: sm_os_ultrix.h,v 1.2 2001/10/01 14:10:45 ca Exp $
 */

/*
**  platform definitions for Ultrix
*/

#define SM_OS_NAME "ultrix"

#define SM_CONF_SSIZE_T	0

/* can't set real gid */
#ifndef SM_CONF_CANT_SETRGID
# define SM_CONF_CANT_SETRGID	1
#endif /* SM_CONF_CANT_SETRGID */
