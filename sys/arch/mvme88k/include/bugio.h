/*	$OpenBSD: bugio.h,v 1.13 2002/03/14 01:26:39 millert Exp $ */

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

#endif /* __MACHINE_BUGIO_H__ */
