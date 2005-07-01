/*	$OpenBSD: wcstoll.c,v 1.1 2005/07/01 08:59:27 espie Exp $	*/
/* $NetBSD: wcstoll.c,v 1.1 2003/03/11 09:21:23 tshiozak Exp $ */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: wcstoll.c,v 1.1 2005/07/01 08:59:27 espie Exp $";
#endif /* LIBC_SCCS and not lint */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "wctoint.h"

#define	FUNCNAME	wcstoll
typedef long long int int_type;
#define	MIN_VALUE	LLONG_MIN
#define	MAX_VALUE	LLONG_MAX

#include "_wcstol.h"
