/*	$Id: gstabs.h,v 1.1.1.1 1995/10/18 08:39:17 deraadt Exp $ */

#include "i386/gas.h"

/* We do not want to output SDB debugging information.  */

#undef SDB_DEBUGGING_INFO

/* We want to output DBX debugging information.  */

#define DBX_DEBUGGING_INFO
