/*	$NetBSD: isa_machdep.c,v 1.5 1996/11/23 06:38:49 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Machine-specific functions for PCI autoconfiguration.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <dev/isa/isavar.h>

#include "vga_isa.h"
#if NVGA_ISA
#include <alpha/isa/vga_isavar.h>
#endif

struct {
	int	(*probe) __P((bus_space_tag_t, bus_space_tag_t));
	void	(*console) __P((bus_space_tag_t, bus_space_tag_t));
} isa_display_console_devices[] = {
#if NVGA_ISA
	{ vga_isa_console_match, vga_isa_console_attach },
#endif
	{ },
};

void
isa_display_console(iot, memt)
	bus_space_tag_t iot, memt;
{
	int i = 0;

	while (isa_display_console_devices[i].probe != NULL)
		if ((*isa_display_console_devices[i].probe)(iot, memt)) {
			(*isa_display_console_devices[i].console)(iot, memt);
			break;
		}
}
