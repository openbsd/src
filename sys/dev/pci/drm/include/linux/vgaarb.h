/* Public domain. */

#ifndef _LINUX_VGAARB_H
#define _LINUX_VGAARB_H

#include <sys/errno.h>

#define VGA_RSRC_LEGACY_IO	0x01

struct pci_dev;

void vga_get_uninterruptible(struct pci_dev *, int);
void vga_put(struct pci_dev *, int);

static inline int
vga_client_register(struct pci_dev *a, void *b, void *c, void *d)
{
	return -ENODEV;
}

#endif
