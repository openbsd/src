/*	$OpenBSD: db_machdep.h,v 1.4 1997/01/16 04:04:06 kstailey Exp $	*/
/*	$NetBSD: db_machdep.h,v 1.7 1995/02/07 04:34:45 gwr Exp $	*/

#include <m68k/db_machdep.h>

/* This enables some code in db_command.c */
#define DB_MACHINE_COMMANDS

void db_machine_init __P((void));
void ddb_init __P((void));

/* These are in db_memrw.c */
extern void db_read_bytes  __P((vm_offset_t addr, size_t size, char *data));
extern void db_write_bytes __P((vm_offset_t addr, size_t size, char *data));
