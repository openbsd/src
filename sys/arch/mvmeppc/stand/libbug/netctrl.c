/*	$OpenBSD: netctrl.c,v 1.3 2004/01/24 21:12:38 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>

#include "libbug.h"

/* returns 0: success, nonzero: error */
int
mvmeprom_netctrl(arg)
	struct mvmeprom_netctrl *arg;
{
	asm volatile ("mr 3, %0":: "r" (arg));
	MVMEPROM_CALL(MVMEPROM_NETCTRL);
	return (arg->status);
}

int 
mvmeprom_netctrl_init(clun, dlun)
u_char	clun;
u_char	dlun;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 0; /* init */
	niocall.addr = 0;
	niocall.len = 0;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	return(niocall.status);
}

int 
mvmeprom_netctrl_hwa(clun, dlun, addr, len)
u_char	clun;
u_char	dlun;
void 	*addr;
u_long  *len;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 1; /* get hw address */
	niocall.addr = addr;
	niocall.len = *len;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	*len = niocall.len;
	return(niocall.status);
}

int 
mvmeprom_netctrl_tx(clun, dlun, addr, len)
u_char	clun;
u_char	dlun;
void 	*addr;
u_long  *len;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 2; /* transmit */
	niocall.addr = addr;
	niocall.len = *len;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	*len = niocall.len;
	return(niocall.status);
}

int 
mvmeprom_netctrl_rx(clun, dlun, addr, len)
u_char	clun;
u_char	dlun;
void 	*addr;
u_long  *len;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 3; /* receive */
	niocall.addr = addr;
	niocall.len = *len;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	*len = niocall.len;
	return(niocall.status);
}

int 
mvmeprom_netctrl_flush_rx(clun, dlun)
u_char	clun;
u_char	dlun;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 4; /* reset */
	niocall.addr = 0;
	niocall.len = 0;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	return(niocall.status);
}

int 
mvmeprom_netctrl_reset(clun, dlun)
u_char	clun;
u_char	dlun;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 5; /* reset */
	niocall.addr = 0;
	niocall.len = 0;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	return(niocall.status);
}


