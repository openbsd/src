/*	$NetBSD: db_machdep.h,v 1.8 1996/12/17 21:11:05 gwr Exp $	*/

#include <m68k/db_machdep.h>

/* This enables some code in db_command.c */
#define DB_MACHINE_COMMANDS

extern char	*esym;	/* end of symbols */
void ddb_init __P((void));
void db_machine_init __P((void));

/* These are in db_memrw.c */
extern void db_read_bytes  __P((vm_offset_t addr, size_t size, char *data));
extern void db_write_bytes __P((vm_offset_t addr, size_t size, char *data));
