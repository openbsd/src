/* Public domain. */

#ifndef _LINUX_VGA_SWITCHEROO_H
#define _LINUX_VGA_SWITCHEROO_H

#include <linux/fb.h>

struct pci_dev;

#define vga_switcheroo_register_client(a, b, c)	0
#define vga_switcheroo_unregister_client(a)
#define vga_switcheroo_process_delayed_switch()
#define vga_switcheroo_fini_domain_pm_ops(x)
#define vga_switcheroo_handler_flags()		0
#define vga_switcheroo_client_fb_set(a, b)
#define vga_switcheroo_init_domain_pm_ops(a, b)

#define VGA_SWITCHEROO_CAN_SWITCH_DDC		1

static inline int
vga_switcheroo_lock_ddc(struct pci_dev *pdev)
{
	return -ENOSYS;
}

static inline int
vga_switcheroo_unlock_ddc(struct pci_dev *pdev)
{
	return -ENOSYS;
}

#endif
