/*	$OpenBSD: bugio.h,v 1.15 2005/12/11 21:45:31 miod Exp $ */

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
int spin_cpu(cpuid_t, vaddr_t);

#endif /* __MACHINE_BUGIO_H__ */
