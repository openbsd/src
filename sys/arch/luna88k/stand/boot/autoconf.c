/*	$OpenBSD: autoconf.c,v 1.2 2013/10/29 18:51:37 miod Exp $	*/
/*	$NetBSD: autoconf.c,v 1.7 2013/01/22 15:48:40 tsutsui Exp $	*/

/*
 * Copyright (c) 1992 OMRON Corporation.
 *
 * This code is derived from software contributed to Berkeley by
 * OMRON Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)autoconf.c	8.1 (Berkeley) 6/10/93
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * OMRON Corporation.
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
 *	@(#)autoconf.c	8.1 (Berkeley) 6/10/93
 */

/*
 * autoconf.c -- Determine mass storage and memory configuration for a machine.
 *          by A.Fujita, NOV-30-1991
 *
 * Modified by A.Fujita, FEB-04-1992
 */


#include <sys/param.h>
#include <machine/board.h>
#include <lib/libkern/libkern.h>
#include <luna88k/stand/boot/samachdep.h>
#include <luna88k/stand/boot/device.h>

struct	hp_hw sc_table[MAX_CTLR];

#ifdef DEBUG
int	acdebug = 1;
#endif

static int find_controller(struct hp_hw *);
static int find_device(struct hp_hw *);
static void find_slaves(struct hp_ctlr *);
static int same_hw_device(struct hp_hw *, struct hp_device *);

/*
 * Determine mass storage and memory configuration for a machine.
 */
void
configure(void)
{
	struct hp_hw *hw;
	int found;

	/*
	 * Look over each hardware device actually found and attempt
	 * to match it with an ioconf.c table entry.
	 */
	for (hw = sc_table; hw->hw_type; hw++) {
		if (hw->hw_type & CONTROLLER)
			found = find_controller(hw);
		else
			found = find_device(hw);
#ifdef DEBUG
		if (!found) {
			printf("unconfigured %s ", hw->hw_name);
			printf("at 0x%x\n", (u_int)hw->hw_addr);
		}
#endif
	}

}

#define dr_type(d, s)	\
	(strcmp((d)->d_name, (s)) == 0)

#define same_hw_ctlr(hw, hc) \
	((hw)->hw_type == SCSI && dr_type((hc)->hp_driver, "sc"))

int
find_controller(struct hp_hw *hw)
{
	struct hp_ctlr *hc;
	struct hp_ctlr *match_c;
	uint8_t *addr, *oaddr;

#ifdef DEBUG
	if (acdebug)
		printf("find_controller: hw: %s at %p, type %x...",
		       hw->hw_name, hw->hw_addr, hw->hw_type);
#endif
	addr = hw->hw_addr;
	match_c = NULL;
	for (hc = hp_cinit; hc->hp_driver; hc++) {
		if (hc->hp_alive)
			continue;
		/*
		 * Make sure we are looking at the right
		 * controller type.
		 */
		if (!same_hw_ctlr(hw, hc))
			continue;
		/*
		 * Exact match; all done
		 */
		if (hc->hp_addr == addr) {
			match_c = hc;
			break;
		}
		/*
		 * Wildcard; possible match so remember first instance
		 * but continue looking for exact match.
		 */
		if ((int)hc->hp_addr == WILD_CARD_CTLR && match_c == NULL)
			match_c = hc;
	}
#ifdef DEBUG
	if (acdebug) {
		if (match_c)
			printf("found %s%d\n",
			       match_c->hp_driver->d_name,
			       match_c->hp_unit);
		else
			printf("not found\n");
	}
#endif
	/*
	 * Didn't find an ioconf entry for this piece of hardware,
	 * just ignore it.
	 */
	if (match_c == NULL)
		return(0);
	/*
	 * Found a match, attempt to initialize and configure all attached
	 * slaves.  Note, we can still fail if HW won't initialize.
	 */
	hc = match_c;
	oaddr = hc->hp_addr;
	hc->hp_addr = hw->hw_addr;
	if ((*hc->hp_driver->d_init)(hc)) {
		hc->hp_alive = 1;
		printf("%s%d", hc->hp_driver->d_name, hc->hp_unit);
		printf(" at %p\n", hc->hp_addr);
		find_slaves(hc);
	} else
		hc->hp_addr = oaddr;
	return(1);
}

int
find_device(struct hp_hw *hw)
{
	struct hp_device *hd;
	struct hp_device *match_d;
	uint8_t *addr, *oaddr;

#ifdef DEBUG
	if (acdebug)
		printf("find_device: hw: %s at %p, type %x...",
		       hw->hw_name, hw->hw_addr, hw->hw_type);
#endif
	match_d = NULL;
	for (hd = hp_dinit; hd->hpd_driver; hd++) {
		if (hd->hpd_alive)
			continue;
		/* Must not be a slave */
		if (hd->hpd_cdriver)
			continue;
		addr = hd->hpd_addr;
		/*
		 * Exact match; all done.
		 */
		if (addr != NULL && addr == hw->hw_addr) {
			match_d = hd;
			break;
		}
		/*
		 * Wildcard; possible match so remember first instance
		 * but continue looking for exact match.
		 */
		if (addr == NULL && same_hw_device(hw, hd) && match_d == NULL)
			match_d = hd;
	}
#ifdef DEBUG
	if (acdebug) {
		if (match_d)
			printf("found %s%d\n",
			       match_d->hpd_driver->d_name,
			       match_d->hpd_unit);
		else
			printf("not found\n");
	}
#endif
	/*
	 * Didn't find an ioconf entry for this piece
	 * of hardware, just ignore it.
	 */
	if (match_d == NULL)
		return(0);
	/*
	 * Found a match, attempt to initialize.
	 * Note, we can still fail if HW won't initialize.
	 */
	hd = match_d;
	oaddr = hd->hpd_addr;
	hd->hpd_addr = hw->hw_addr;
	if ((*hd->hpd_driver->d_init)(hd)) {
		hd->hpd_alive = 1;
		printf("%s%d", hd->hpd_driver->d_name, hd->hpd_unit);
		printf(" at %p\n", hd->hpd_addr);
	} else
		hd->hpd_addr = oaddr;
	return(1);
}

/*
 * Search each BUS controller found for slaves attached to it.
 * The bad news is that we don't know how to uniquely identify all slaves
 * (e.g. PPI devices on HP-IB).  The good news is that we can at least
 * differentiate those from slaves we can identify.  At worst (a totally
 * wildcarded entry) this will cause us to locate such a slave at the first
 * unused position instead of where it really is.  To save grief, non-
 * identifing devices should always be fully qualified.
 */
void
find_slaves(struct hp_ctlr *hc)
{
	int s;
	struct hp_device *hd;
	struct hp_device *match_s;
	int maxslaves = MAXSLAVES-1;
	int new_s, new_c, old_s, old_c;

#ifdef DEBUG
	if (acdebug)
		printf("find_slaves: for %s%d\n",
		       hc->hp_driver->d_name, hc->hp_unit);
#endif
	new_s = new_c = -1;
	for (s = 0; s < maxslaves; s++) {
		match_s = NULL;
		for (hd = hp_dinit; hd->hpd_driver; hd++) {
			/*
			 * Rule out the easy ones:
			 * 1. slave already assigned or not a slave
			 * 2. not of the proper type
			 * 3. controller specified but not this one
			 * 4. slave specified but not this one
			 */
			if (hd->hpd_alive || hd->hpd_cdriver == NULL)
				continue;
			if (!dr_type(hc->hp_driver, hd->hpd_cdriver->d_name))
				continue;
			if (hd->hpd_ctlr >= 0 && hd->hpd_ctlr != hc->hp_unit)
				continue;
			if (hd->hpd_slave >= 0 && hd->hpd_slave != s)
				continue;
			/*
			 * Case 0: first possible match.
			 * Remember it and keep looking for better.
			 */
			if (match_s == NULL) {
				match_s = hd;
				new_c = hc->hp_unit;
				new_s = s;
				continue;
			}
			/*
			 * Case 1: exact match.
			 * All done.  Note that we do not attempt any other
			 * matches if this one fails.  This allows us to
			 * "reserve" locations for dynamic addition of
			 * disk/tape drives by fully qualifing the location.
			 */
			if (hd->hpd_slave == s && hd->hpd_ctlr == hc->hp_unit) {
				match_s = hd;
				break;
			}
			/*
			 * Case 2: right controller, wildcarded slave.
			 * Remember first and keep looking for an exact match.
			 */
			if (hd->hpd_ctlr == hc->hp_unit &&
			    match_s->hpd_ctlr < 0) {
				match_s = hd;
				new_s = s;
				continue;
			}
			/*
			 * Case 3: right slave, wildcarded controller.
			 * Remember and keep looking for a better match.
			 */
			if (hd->hpd_slave == s &&
			    match_s->hpd_ctlr < 0 && match_s->hpd_slave < 0) {
				match_s = hd;
				new_c = hc->hp_unit;
				continue;
			}
			/*
			 * OW: we had a totally wildcarded spec.
			 * If we got this far, we have found a possible
			 * match already (match_s != NULL) so there is no
			 * reason to remember this one.
			 */
			continue;
		}
		/*
		 * Found a match.  We need to set hp_ctlr/hp_slave properly
		 * for the init routines but we also need to remember all
		 * the old values in case this doesn't pan out.
		 */
		if (match_s) {
			hd = match_s;
			old_c = hd->hpd_ctlr;
			old_s = hd->hpd_slave;
			if (hd->hpd_ctlr < 0)
				hd->hpd_ctlr = new_c;
			if (hd->hpd_slave < 0)
				hd->hpd_slave = new_s;
#ifdef DEBUG
			if (acdebug)
				printf("looking for %s%d at slave %d...",
				       hd->hpd_driver->d_name,
				       hd->hpd_unit, hd->hpd_slave);
#endif

			if ((*hd->hpd_driver->d_init)(hd)) {
#ifdef DEBUG
				if (acdebug)
					printf("found\n");
#endif
				printf("%s%d at %s%d, slave %d\n",
				       hd->hpd_driver->d_name, hd->hpd_unit,
				       hc->hp_driver->d_name, hd->hpd_ctlr,
				       hd->hpd_slave);
				hd->hpd_alive = 1;
			} else {
#ifdef DEBUG
				if (acdebug)
					printf("not found\n");
#endif
				hd->hpd_ctlr = old_c;
				hd->hpd_slave = old_s;
			}
		}
	}
}

int
same_hw_device(struct hp_hw *hw, struct hp_device *hd)
{
	int found = 0;

	switch (hw->hw_type) {
	case NET:
		found = dr_type(hd->hpd_driver, "le");
		break;
	case SCSI:
		found = dr_type(hd->hpd_driver, "scsi");
		break;
	}
	return(found);
}

#define setup_hw(hw, addr, type, name) \
	(hw)->hw_addr = addr; \
	(hw)->hw_type = type; \
	(hw)->hw_name = name

void
find_devs(void)
{
	struct hp_hw *hw = sc_table;
	
	setup_hw(hw, (uint8_t *)0x51000000, SIO,      "uPD7201A (SIO)");
	hw++;
	
	setup_hw(hw, (uint8_t *)0x51000004, KEYBOARD, "uPD7201A (KBD)");
	hw++;
	
	setup_hw(hw, (uint8_t *)0xe1000000, SCSI,     "MB89352  (SPC)");
	hw++;

	if (machtype == LUNA_88K2 && badaddr((void *)0xe1000040, 4) == 0) {
		setup_hw(hw, (uint8_t *)0xe1000040, SCSI,     "MB89352  (SPC)");
		hw++;
	}
	if (badaddr((void *)0xf1000000, 4) == 0) {
		setup_hw(hw, (uint8_t *)0xf1000000, NET,      "Am7990 (LANCE)");
		hw++;
	}
}
