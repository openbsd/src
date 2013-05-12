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
#include <netinet/in_systm.h>

#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"
#include "net.h"
#include "netif.h"
#include "config.h"

#include "if_lereg.h"

/*
 * For sboot, we can't assume anything about BUG network support, and
 * need to build the list of available devices ourselves.
 */

static uint32_t vle_addr[] = {
	0xffff1200,
	0xffff1400,
	0xffff1600,
	0xffff5400,
	0xffff5600,
	0xffffa400
};

struct ie_configuration ie_config[1];
int nie_config;

struct le_configuration le_config[0 + sizeof(vle_addr) / sizeof(vle_addr[0])];
int nle_config;

extern struct netif_driver le_driver;
extern struct netif_driver ie_driver;

struct netif_driver *netif_drivers[] = {
	&ie_driver,
	&le_driver,
};
int n_netif_drivers = sizeof(netif_drivers) / sizeof(netif_drivers[0]);

#define	MAXIFACES	((sizeof(ie_config) / sizeof(ie_config[0])) + \
			 (sizeof(le_config) / sizeof(le_config[0])))

static struct netif_stats if_stats[MAXIFACES];
static struct netif_dif if_dif[MAXIFACES];

static void
add_if(struct netif_driver *drv)
{
	static struct netif_stats *stats = if_stats;
	static struct netif_dif *dif = if_dif;

	dif->dif_unit = drv->netif_nifs;
	dif->dif_nsel = 1;
	dif->dif_stats = stats;

	if (drv->netif_nifs == 0)
		drv->netif_ifs = dif;
	drv->netif_nifs++;

	stats++;
	dif++;
}

/*
 * Figure out what devices are available.
 */
int
probe_ethernet()
{
	struct mvmeprom_brdid *brdid;
	uint n;

	brdid = mvmeprom_brdid();

	/* On-board Ethernet */
	switch (brdid->model) {
	case BRD_187:
	case BRD_197:
	case BRD_8120:
		ie_config[nie_config].clun = 0;
		ie_config[nie_config].phys_addr = INTEL_REG_ADDR;
		bcopy(brdid->etheraddr, ie_config[nie_config].eaddr, 6);
		add_if(&ie_driver);
		nie_config++;
		break;
	}
	
	/* MVME376 */
	for (n = 0; n < sizeof(vle_addr) / sizeof(vle_addr[0]); n++) {
		if (badaddr((void *)vle_addr[n], 2) == 0) {
			le_config[nle_config].clun = 2 + n;
			le_config[nle_config].phys_addr = vle_addr[n];
			le_config[nle_config].buf_addr =
			    VLEMEMBASE - n * VLEMEMSIZE;
			le_config[nle_config].buf_size = VLEMEMSIZE;
			le_read_etheraddr(vle_addr[n],
			    le_config[nle_config].eaddr);
			add_if(&le_driver);
			nle_config++;
		}
	}

	return nie_config + nle_config;
}

/*
 * Mimic the NIOT;H BUG command (except for `P-Address' field), adding our
 * own driver names.
 */
void
display_ethernet()
{
	struct mvmeprom_brdid *brdid;
	int i;

	brdid = mvmeprom_brdid();

	printf("Network Controllers/Nodes Supported\n");
	printf("Driver CLUN  DLUN  Name      Address    Ethernet Address\n");

	for (i = 0; i < nie_config; i++) {
		printf("ie%d       %x     0  VME%x",
		    i, ie_config[i].clun, brdid->model);
		/* @#$%! MVME8120 - and I don't even have one to test (miod) */
		if (brdid->model < 0x1000)
			printf(" ");
		printf("   $%x  %s\n",
		    ie_config[i].phys_addr, ether_sprintf(ie_config[i].eaddr));
	}

	for (i = 0; i < nle_config; i++) {
		printf("le%d       %x     0  VME376    $%x  %s\n",
		    i, le_config[i].clun, le_config[i].phys_addr,
		    ether_sprintf(le_config[i].eaddr));
	}
}
