/*	$OpenBSD: bugio.h,v 1.18 2013/05/14 20:30:47 miod Exp $ */

#ifndef _MACHINE_BUGIO_H_
#define _MACHINE_BUGIO_H_

#include <machine/prom.h>

void	buginit(void);
char	buginchr(void);
void	bugoutchr(int);
void	bugreturn(void);
void	bugbrdid(struct mvmeprom_brdid *);
void	bugdiskrd(struct mvmeprom_dskio *);
int	spin_cpu(cpuid_t, vaddr_t);

#endif /* _MACHINE_BUGIO_H_ */
