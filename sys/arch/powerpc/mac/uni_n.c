/*	$OpenBSD: uni_n.c,v 1.1 2000/10/16 00:18:01 drahn Exp $	*/


/* put BSD copyright here */

#include <sys/param.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>

void
uni_n_config(int handle)
{
	char name[20];
	char *baseaddr;
	int *ctladdr;
	u_int32_t address;
	if (OF_getprop(handle, "name", name, sizeof name) > 0) {
		/* sanity test */
		if (!strcmp (name, "uni-n")) { 
			if (OF_getprop(handle, "reg", &address,
					sizeof address) > 0)
			{
				printf("found uni-n at address %x\n", address);
				baseaddr = mapiodev(address, NBPG);
				ctladdr = (void*)(baseaddr + 0x20);
				*ctladdr |= 0x02;
			}
		}
	}
	return;
}
