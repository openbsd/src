/* Public domain. */

#ifndef _LINUX_KGDB_H
#define _LINUX_KGDB_H

#include <ddb/db_var.h>

static inline int
in_dbg_master(void)
{
#ifdef DDB
	return (db_is_active);
#endif
	return (0);
}

#endif
