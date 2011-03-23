/*	$OpenBSD: bugio.h,v 1.17 2011/03/23 16:54:36 pirofti Exp $ */

#ifndef _MACHINE_BUGIO_H_
#define _MACHINE_BUGIO_H_

#include <machine/prom.h>

void	buginit(void);
char	buginchr(void);
void	bugpcrlf(void);
void	bugoutchr(int);
void	bugreturn(void);
void	bugbrdid(struct mvmeprom_brdid *);
void	bugdiskrd(struct mvmeprom_dskio *);
int	spin_cpu(cpuid_t, vaddr_t);

#endif /* _MACHINE_BUGIO_H_ */
