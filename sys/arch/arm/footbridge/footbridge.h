/*	$OpenBSD: footbridge.h,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/*      $NetBSD: footbridge.h,v 1.2 2002/05/04 10:04:42 chris Exp $  */

#ifndef _FOOTBRIDGE_H_
#define _FOOTBRIDGE_H_

#include <sys/termios.h>
#include <arm/bus.h>
void footbridge_pci_bs_tag_init __P((void));
void footbridge_sa110_cc_setup	__P((void));
void footbridge_create_io_bs_tag __P((struct bus_space *, void *));
void footbridge_create_mem_bs_tag __P((struct bus_space *, void *));
int fcomcnattach __P((u_int, int, tcflag_t));
int fcomcndetach __P((void));
void calibrate_delay __P((void));

#endif
