/*	$NetBSD: prom.c,v 1.3 1995/06/28 00:59:04 cgd Exp $	*/

/*  
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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

#define	PUTS	0

#if PUTS
int console;
#endif

void
init_prom_calls()
{
	extern struct prom_vec prom_dispatch_v;
	struct rpb *r;
	struct crb *c;
	char buf[4];

	r = (struct rpb *)HWRPB_ADDR;
	c = (struct crb *)((u_int8_t *)r + r->rpb_crb_off);

	prom_dispatch_v.routine_arg = c->crb_v_dispatch;
	prom_dispatch_v.routine = c->crb_v_dispatch->code;

#if PUTS
	/* Look for console tty. */
	prom_getenv(PROM_E_TTY_DEV, buf, 4);
	console = buf[0] - '0';
#endif
}

#if PUTS
static inline void
putchar(c)
	int c;
{
	char cbuf;

	if (c == '\r' || c == '\n') {
		cbuf = '\r';
		do {
			ret.bits = prom_dispatch(PROM_R_PUTS, console,
			    &cbuf, 1);
		} while ((ret.u.retval & 1) == 0);
		cbuf = '\n';
	} else
		cbuf = c;
	do {
		ret.bits = prom_dispatch(PROM_R_PUTS, console, &cbuf, 1);
	} while ((ret.u.retval & 1) == 0);
}

void
puts(s)
	char *s;
{
	while (*s)
		putchar(*s++);
}
#endif

int
prom_getenv(id, buf, len)
	int id, len;
	char *buf;
{
	prom_return_t ret;

	ret.bits = prom_dispatch(PROM_R_GETENV, id, buf, len-1);
	if (ret.u.status & 0x4)
		ret.u.retval = 0;
	buf[ret.u.retval] = '\0';

	return (ret.u.retval);
}

int
prom_open(dev, len)
	char *dev;
	int len;
{
	prom_return_t ret;

	ret.bits = prom_dispatch(PROM_R_OPEN, dev, len);
	if (ret.u.status & 0x4)
		return (-1);
	else
		return (ret.u.retval);
}
