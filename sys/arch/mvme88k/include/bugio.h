/*	$OpenBSD: bugio.h,v 1.14 2004/01/04 01:14:06 miod Exp $ */

#ifndef __MACHINE_BUGIO_H__
#define __MACHINE_BUGIO_H__

#include <sys/cdefs.h>

#include <machine/prom.h>

void buginit(void);
int buginstat(void);
char buginchr(void);
void bugoutchr(unsigned char);
void bugoutstr(char *, char *);
void bugrtcrd(struct mvmeprom_time *);
void bugreturn(void);
void bugbrdid(struct mvmeprom_brdid *);
void bugdiskrd(struct mvmeprom_dskio *);

#endif /* __MACHINE_BUGIO_H__ */
