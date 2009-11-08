/*	$OpenBSD: machdep.c,v 1.4 2009/11/08 22:00:34 kettenis Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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

#include <sys/types.h>

#include "libsa.h"
#include "wdvar.h"
#include "fdt.h"

/*
 * RouterBOOT firmware puts its FDT at an address that is low enough
 * to conflict with bsd.rd.  So we need to relocate the FDT.  As long
 * as we have at least 32MB of memory, the 16MB boundary should be
 * fine, and leave us plenty of room for future kernel growth.
 */
#define FDTADDRSTART	0x01000000

#define RPR	0xe0000918
#define  RPR_RSTE	0x52535445
#define RCR	0xe000091c
#define  RCR_SWSR	0x00000001
#define  RCR_SWHR	0x00000002

/* defines from pciide.c and wdc_obio.c */
int     pciide_init             (struct wdc_channel*, u_int);
int     wdc_obio_init           (struct wdc_channel*, u_int);

void
machdep(void)
{
	void *node;
	char *tmp;
	int len;

	extern int consfreq;
	extern uint8_t *consaddr;
	
	/* set default values */
	consfreq = NS16550_FREQ;
	consaddr = (uint8_t *)CONADDR;

	/* lookup FTD for informations about conole */
	node = fdt_find_node("/chosen");
	if (node) {
		char *console;
		fdt_node_property(node, "linux,stdout-path", &console);
		node = fdt_find_node(console);
		if (node) {
			len = fdt_node_property(node, "clock-frequency", &tmp);
			if (len == 4)
				consfreq = *(int *)tmp;

			len = fdt_node_property(node, "reg", &tmp);
			if (len == 8)
				consaddr = (uint8_t *)*(int *)tmp;
		}
		if (node = fdt_parent_node(node)) {
			fdt_node_property(node, "device_type", &tmp);
			if (strncmp(tmp, "soc", 3) == 0) {
				/* we are on a soc */
				len = fdt_node_property(node, "reg", &tmp);
				if (len == 8)
					consaddr += *(int *)tmp;
			}
		}
	}

	cninit();
{
	extern int (*controller_init)(struct wdc_channel *chp, u_int chan);
	extern u_int32_t pciide_base_addr;
	extern u_int32_t wdc_base_addr[];
	int *addr;
	int chnum;

	/* Thecus defaults */
	controller_init = pciide_init;
	pciide_base_addr = 0xe2000000;

	/* lookup the FDT, may have some CF there */
	chnum = 0;
	wdc_base_addr[0] = 0;
	wdc_base_addr[1] = 0;
	node = fdt_find_node("/");
	for (node = fdt_child_node(node); node; node = fdt_next_node(node)) {
		len = fdt_node_property(node, "device_type", &tmp);
		if (len && (strcmp(tmp, "rb,cf") == 0) && (chnum < 2)) {
			len = fdt_node_property(node, "reg", (char **)&addr);
			if (len == 8) {
				wdc_base_addr[chnum] = *addr;
				chnum++;
			}
		}
	}
	if (chnum)
		controller_init = wdc_obio_init;
}

}

int
main(void)
{
	extern char __bss_start[], _end[];
	extern int fdtaddrsave;

	bzero(__bss_start, _end - __bss_start);

	/* initialize FDT if the blob is available */
	if (fdtaddrsave) {
		if (fdt_init((void *)fdtaddrsave) == 0)
			fdtaddrsave = 0; /* no usable blob there */
	}

	/* relocate FDT */
	if (fdtaddrsave && fdtaddrsave < FDTADDRSTART) {
		struct fdt_head *fh = (void *)fdtaddrsave;

		bcopy((void *)fdtaddrsave, (void *)FDTADDRSTART, fh->fh_size);
		fdtaddrsave = FDTADDRSTART;
	}

	boot(0);
	return 0;
}

void
_rtt(void)
{
	uint32_t v;

	*((volatile uint32_t *)(RPR)) = RPR_RSTE;
	__asm __volatile("eieio");
	*((volatile uint32_t *)(RCR)) = RCR_SWHR;

	printf("RESET FAILED\n");
	for (;;) ;
}
