/*	$OpenBSD: devices.c,v 1.1 2013/05/12 10:43:45 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"
#include "netif.h"
#include "config.h"

#include "if_lereg.h"

/*
 * For netboot, we only intend to use the interface we have been booted from;
 * although we carry the list of all BUG-recognized Ethernet devices for which
 * we have drivers, we will select the appropriate instance from the BUG
 * arguments.
 */

struct ie_configuration ie_config[] = {
	{ .clun = 0x00, .phys_addr = INTEL_REG_ADDR }
};
int nie_config = sizeof(ie_config) / sizeof(ie_config[0]);

#define	VLE(u,a) \
	{ .clun = 0x02 + (u), .phys_addr = (a), \
	  .buf_addr = VLEMEMBASE - (u) * VLEMEMSIZE, .buf_size = VLEMEMSIZE }
struct le_configuration le_config[] = {
	VLE(0, 0xffff1200),
	VLE(1, 0xffff1400),
	VLE(2, 0xffff1600),
	VLE(3, 0xffff5400),
	VLE(4, 0xffff5600),
	VLE(5, 0xffffa400)
};

int nle_config = sizeof(le_config) / sizeof(le_config[0]);

extern struct netif_driver le_driver;
extern struct netif_driver ie_driver;

struct netif_driver *netif_drivers[] = {
	&ie_driver,
	&le_driver,
};
int n_netif_drivers = sizeof(netif_drivers) / sizeof(netif_drivers[0]);

static struct netif_stats bugif_stats;
static struct netif_dif bugif_dif = {
	.dif_unit = 0,
	.dif_nsel = 1,
	.dif_stats = &bugif_stats
};

int
probe_ethernet()
{
	int n;

	for (n = 0; n < nie_config; n++)
		if (ie_config[n].clun == bugargs.ctrl_lun) {
			bcopy(mvmeprom_brdid()->etheraddr,
			    ie_config[n].eaddr, 6);
			ie_driver.netif_nifs = 1;
			ie_driver.netif_ifs = &bugif_dif;
			break;
		}

	for (n = 0; n < nle_config; n++)
		if (le_config[n].clun == bugargs.ctrl_lun) {
			le_read_etheraddr(le_config[n].phys_addr,
			    le_config[n].eaddr);
			le_driver.netif_nifs = 1;
			le_driver.netif_ifs = &bugif_dif;
			break;
		}

	return ie_driver.netif_nifs + le_driver.netif_nifs;
}
