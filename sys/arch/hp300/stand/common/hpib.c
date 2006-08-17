/*	$OpenBSD: hpib.c,v 1.4 2006/08/17 06:31:10 miod Exp $	*/
/*	$NetBSD: hpib.c,v 1.2 1997/05/12 07:48:23 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)hpib.c	8.1 (Berkeley) 6/10/93
 */

/*
 * HPIB driver
 */
#include <sys/param.h>
#include <sys/reboot.h>

#include <lib/libsa/stand.h>

#include "samachdep.h"
#include "device.h"
#include "hpibvar.h"

#include <hp300/dev/dioreg.h>

int	internalhpib = IIOV(DIO_IHPIBADDR);

struct	hpib_softc hpib_softc[NHPIB];

void
hpibinit()
{
	extern struct hp_hw sc_table[];
	struct hp_hw *hw;
	struct hpib_softc *hs;
	int i;

	i = 0;
	for (hw = sc_table; i < NHPIB && hw < &sc_table[MAXCTLRS]; hw++) {
		if (!HW_ISHPIB(hw))
			continue;
		hs = &hpib_softc[i];
		hs->sc_addr = hw->hw_kva;
		if (nhpibinit(i) == 0)
			if (fhpibinit(i) == 0)
				continue;
		if (howto & RB_ASKNAME)
			printf("hpib%d at sc%d\n", i, hw->hw_sc);
		hw->hw_pa = (caddr_t) i;	/* XXX for autoconfig */
		hs->sc_alive = 1;
		i++;
	}
}

int
hpibalive(int unit)
{
	if (unit >= NHPIB || hpib_softc[unit].sc_alive == 0)
		return (0);
	return (1);
}

int
hpibid(int unit, int slave)
{
	short id;
	int rv;

	if (hpib_softc[unit].sc_type == HPIBC)
		rv = fhpibrecv(unit, 31, slave, (char *)&id, 2);
	else
		rv = nhpibrecv(unit, 31, slave, (char *)&id, 2);
	if (rv != 2)
		return (0);
	return (id);
}

int
hpibsend(int unit, int slave, int sec, void *buf, int cnt)
{
	if (hpib_softc[unit].sc_type == HPIBC)
		return (fhpibsend(unit, slave, sec, (char *)buf, cnt));
	else
		return (nhpibsend(unit, slave, sec, (char *)buf, cnt));
}

int
hpibrecv(int unit, int slave, int sec, void *buf, int cnt)
{
	if (hpib_softc[unit].sc_type == HPIBC)
		return (fhpibrecv(unit, slave, sec, (char *)buf, cnt));
	else
		return (nhpibrecv(unit, slave, sec, (char *)buf, cnt));
}

int
hpibswait(int unit, int slave)
{
	int timo = 1000000;
	int (*poll)(int);

	slave = 0x80 >> slave;
	if (hpib_softc[unit].sc_type == HPIBC)
		poll = fhpibppoll;
	else
		poll = nhpibppoll;
	while (((*poll)(unit) & slave) == 0)
		if (--timo == 0)
			break;
	if (timo == 0)
		return (-1);
	return (0);
}

void
hpibgo(int unit, int slave, int sec, void *addr, int count, int flag)
{
	if (hpib_softc[unit].sc_type == HPIBC) {
		if (flag == F_READ)
			fhpibrecv(unit, slave, sec, (char *)addr, count);
		else
			fhpibsend(unit, slave, sec, (char *)addr, count);
	} else {
		if (flag == F_READ)
			nhpibrecv(unit, slave, sec, (char *)addr, count);
		else
			nhpibsend(unit, slave, sec, (char *)addr, count);
	}
}
