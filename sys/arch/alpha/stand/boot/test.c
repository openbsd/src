/*	$OpenBSD: test.c,v 1.3 1996/10/30 22:40:46 niklas Exp $	*/
/*	$NetBSD: test.c,v 1.2 1995/02/16 02:33:00 cgd Exp $	*/

/*  
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
#include <sys/types.h>

#include <machine/prom.h>
#include <machine/rpb.h>
#include <machine/varargs.h>

main(a1, a2, a3, a4, a5)
	u_int64_t a1, a2, a3;
	char *a4[], *a5[];
{
	extern int console;
	prom_return_t ret;
	int cnt;
	char devname[128];

	init_prom_calls();		/* Init prom callback vector. */

	(void)printf("TEST BOOT\n");
	(void)printf("PFN: %lx\n", a1);
	(void)printf("PTBR: %lx\n", a2);
	(void)printf("argc: %lu\n", a3);
	(void)printf("argv[0]: %ls\n", a4[0]);
	(void)printf("envp: %lx\n", a5);

	ret.bits = prom_getenv(PROM_E_BOOTED_DEV, devname, sizeof(devname));
	devname[ret.u.retval] = '\0';
	(void)printf("booted_dev: %s\n", devname);

	ret.bits = prom_getenv(PROM_E_BOOTED_FILE, devname, sizeof(devname));
	devname[ret.u.retval] = '\0';
	(void)printf("booted_file: %s\n", devname);

	halt();
}
